#!/bin/sh
# playtest.sh -- drive Colosseum with headless clients (R-VER-27).
#
# WHY THIS EXISTS BESIDE play.sh.  R-VER-23's play.sh drives q2pro's OWN client
# under Xvfb: it is the only way to test what a human sees, and it is how R-62
# was found.  It is also a whole GL client per run, paced in frames by a console
# script, and it cannot make an assertion -- it prints, and a person reads.
#
# This one drives `libq2`, a Go implementation of the Quake II network protocol.
# A libq2 client connects, spawns, receives configstrings, layouts, centerprints
# and playerstates, sends console commands and pushes userinfo updates.  It
# cannot walk -- steering by yaw across a map is not reliable enough to build a
# test on -- and that turns out not to matter, because the ruleset code lives in
# menus, client commands, userinfo and the two `sv` diagnostics, all of which are
# deterministic.  What it buys is ASSERTIONS: 108 of them across five rulesets
# and three control servers, exit code 0 or 1.
#
# It found three defects that fifteen audits, ten build configurations and a
# twenty-row boot matrix could not, all of which need a client to exist before
# they can be wrong (doc/reconciliation.md R-87).
#
# The scenario and the harness live in the user-level skill `q2-playtest`,
# because they are useful to any Quake II mod and are not Colosseum's to own;
# this script is the versioned entry point that says how to run them.
#
# REQUIREMENTS
#   go                        the harness is Go
#   $HARNESS                  default ~/.claude/skills/q2-playtest/harness
#   ~/q2-dev/libq2            the protocol library the harness imports
#   $Q2PRO_BUILD/q2proded     default ../q2pro/builddir-native
#   $Q2DATA                   retail baseq2 paks, default
#                             /usr/share/games/quake2/baseq2
#   $CTFDATA                  Threewave paks,      default
#                             /usr/share/games/quake2/ctf
#   $GLADDIR                  gladiator-bot-restored, default ../gladiator-bot-restored.
#                             Phase 7's bot rows need a BRAIN as well as a
#                             server: without it they are skipped, and the skip
#                             is reported rather than silently subtracted.
#
# USAGE
#   tools/playtest.sh [-s <scenario>] [-r dm,dmpro,tdm,duel,ctf,arena,sp] \
#                     [-l <game.so>] [extra args]
#
#   -s names a scenario under the harness's scenarios/ and defaults to
#   `colosseum`, the five-ruleset battery.  `-r` is that scenario's own flag and
#   is only passed when it is the one running.  The others are single-subject;
#   the wrapper supplies only the server/library/asset flags declared by the
#   selected scenario and passes its remaining arguments through unchanged:
#
#   RA2 map scenarios need their archive root supplied after the runner flags,
#   for example:
#
#     tools/playtest.sh -s ra2join -ref ~/q2-dev/yquake2/release_/arena \
#       -teams '#1 Pickup Red,#1 Pickup Blue'
#
#   `ra2join`'s default Medieval arena is pickup-only, so its pre-created team
#   rows are required; creating a new team correctly leaves the client in the
#   lobby and is not a spawn-placement failure.
#
#     ra2botvote   R-RA-8's per-arena `bots` switch and R-RA-9's fill
#                  scheduler: 16 checks in six phases, every one in both signs.
#                  Routing: `bots: 1` seats bots in the arena, `bots: 0` seats
#                  none and `sv ruleset` says which zero that is.  Menu:
#                  `allowvotingbots` 1 offers the "Allow Bots" row to a player
#                  and 0 withholds it, read off a client's statusbar -- and it
#                  checks the row is on the FIRST page, not behind "(More)",
#                  which is a fact about where the row was placed and not just
#                  that it exists.  Eviction: bots in the arena when the switch
#                  moves leave it (`sv arenadump`'s `here=`) and leave the
#                  server.  Crowded: TWO arenas with people in them both get
#                  bots from one fill.  Needs $GLADDIR.
#
#                  The crowded phase is the only one that needs a map with more
#                  than one arena on it, so it wants the RA2 install --
#                  `-ra2ref <dir>` holding pak0..2 and arena.cfg, default
#                  ~/q2-dev/yquake2/release_/arena -- and an `ra2map9.aas` for
#                  the bots to navigate by, which it looks for and takes
#                  `-aas <file>` for.  Without either it is skipped and says
#                  which was missing, rather than being silently subtracted.
#
#                  NOTE it walks the menu cursor rather than reading one page.
#                  The settings menu is 21 rows against menu.c's 18-row window,
#                  so anything at the end of the list is below the fold and a
#                  single-page read reports it missing -- which it did, before
#                  the scenario learned to scroll.
#
#     layeracc     R-181 and R-MODE-3's two halves, which need different
#                  evidence and so are two phases.  SPAWN boots a Reckoning and
#                  a Ground Zero map -- `xdm1` and `rdm1`, from the layers' own
#                  paks -- and censuses their weapons, items and ammo, which is
#                  "the union content is always spawnable" observed from a map
#                  that places it rather than from the itemlist.  ACC gives four
#                  bots a layer weapon through the loadout cvars R-181 added
#                  (`weapon_have`/`weapon_initial` 0x400 for the ETF Rifle bit,
#                  `start_flechettes` for its ammo), lets them fight, and reads
#                  the accuracy report back off the wire: one row proves the bit,
#                  the ammo cvar, the OSP_accShot call inside fire_flechette,
#                  acc_column's mapping and a_info's new row at once.
#
#                  Two things it depends on.  It runs `dm`, because RULESET_DM
#                  sets `sync_stat = 8` and a match is therefore live from the
#                  first frame -- OSP_accShot's `sync_stat <= 2` guard is open
#                  with nobody readying up.  And the ACC phase runs on `q2dm1`
#                  rather than on a layer map, because the weapon comes from the
#                  LOADOUT and not off the floor, so the map only has to be one
#                  the brain can navigate: there is a `q2dm1.aas` and there is no
#                  `xdm1.aas`.
#
#                  Its control is the same server with `weapon_have 0`: the
#                  report still prints and there is no layer row, so a row is the
#                  cvar working rather than the report listing everything it has.
#
#                  Wants the mission-pack paks -- $XATRIXDATA and $ROGUEDATA,
#                  default ~/q2-dev/yquake2/release_/{xatrix,rogue} -- and
#                  $GLADDIR for the bots.  Each missing one skips its own phase
#                  and says which was missing.
#
#     ospenter     R-OSP-1's placement: under the OSP four, CONNECTING IS NOT
#                  ENTERING.  A tourney client arrives as an observer and enters
#                  through a command -- `join` in a free-for-all, `join <team
#                  name>` where there are teams -- and PutClientInServer has two
#                  arms because of it.  24 checks over `dm`, `dmpro`, `tdm` and
#                  `duel`, both signs of every phase: PM_SPECTATOR and STAT_FRAGS
#                  0 before the join, PM_NORMAL after it, and the entering
#                  broadcast absent before and present after.
#
#                  THE PMOVE TYPE IS THE WITNESS and the reason this scenario
#                  exists at all: the server derives it from `movetype`, so it is
#                  the one question about observing that a mod's own HUD cannot
#                  fake -- an observer that is really a solid walking body says
#                  PM_NORMAL however its scoreboard draws it.  That is what
#                  R-191 was found with, and R-RA-4 row 15 before it.
#
#                  `-rulesets` takes any comma-separated subset of the four and
#                  says so about anything else; it needs no bots and turns the
#                  fill off, because a fill would enter clients of its own and
#                  "who has entered" is the whole subject.
#
#     ospcamera    R-OSP-1's chase camera, which has CONTROLS (R-193): eleven
#                  checks over two clients, one playing and one watching.
#                  Forward/back zooms the camera between `camera_depth` and the
#                  target's eye, strafe free-looks it in four-degree steps,
#                  ATTACK cycles chasecam -> in-eyes -> out, and jump cycles the
#                  target.  Every one of those is visible from outside because
#                  the observer's own edict origin IS the camera: the distance
#                  between the two clients' origins is the zoom, `ps.viewangles`
#                  is where it looks, and the mode changes announce themselves.
#
#                  `-depth` is `camera_depth` and defaults to 20 rather than the
#                  shipped 60 ON PURPOSE.  The placement is TRACED, so the
#                  distance is min(zoom, free space behind the target): at 100
#                  the camera stands at xy 38 against a wall on `q2dm1` and
#                  fifteen units of zoom change it not at all.  A row that used
#                  the shipped default would be asserting the map, which is why
#                  the zoom is asserted inwards first.
#
#                  `-ruleset` takes any of the OSP four and skips anything else.
#
#     ospwarmup    R-194's four subjects in one `tdm` sitting: what an OBSERVER
#                  may do, and what a match START leaves behind.  17 checks.
#
#                  THE PITCH PIN HAS EXACTLY ONE MECHANISM and the scenario
#                  reads both ends of it.  PM_ClampAngles answers
#                  PMF_TIME_TELEPORT by writing viewangles[PITCH] and [ROLL] to
#                  zero and letting only YAW follow the mouse, and the pm_time
#                  countdown that would clear the flag sits BELOW Pmove()'s
#                  early return for PM_SPECTATOR -- so an observer handed the
#                  flag keeps it for good, on the client as well as the server.
#                  So: the flag word off the wire, AND the pitch the client
#                  renders after being told to look down and then up.
#
#                  The weapon row is asked WITHOUT pressing anything, which is
#                  the trap this scenario exists to avoid.  ATTACK is not a
#                  trigger for a tourney observer: the first press opens the team
#                  menu and the second picks the row under the cursor, so a
#                  scenario that holds fire here JOINS A TEAM and then measures a
#                  player.  It does not need the button -- Weapon_Generic walks
#                  ps.gunframe round its idle loop on every think, so a gunframe
#                  that has moved at all is a weaponthink that ran.  The control
#                  is an entered client on the same server, whose gunframe does.
#
#                  The countdown clock is read as a stat that holds a
#                  CONFIGSTRING INDEX rather than a value: slot 17, then the
#                  configstring it points at, twenty-five frames apart, for a
#                  player and for an observer.
#
#                  The match-start rows are the ones the report was about: at
#                  "Match has started!" nobody may be PM_DEAD, health is 100, and
#                  the world holds no more entities than it did at level load.
#                  That last census SUBTRACTS THE CLIENTS -- one edict per
#                  connected player is not litter, and the first version of the
#                  check reported three every time and was counting the clients.
#
#                  It is also the A/B that dated the report (R-194): run against
#                  `a3eea8d` it fails six of the eight rows it reaches, which is
#                  what the reporter was playing.
#
#     ospobsmodes  The companion sweep: an observer through free-flight,
#                  chasecam, autocam and back out, reporting pm_type, pm_flags
#                  and what the view does in each.  The camera modes are
#                  REPORTED rather than judged -- a camera looks where the CAMERA
#                  looks and the mouse is not supposed to steer it -- so the
#                  judgement is reserved for free-flight, which is the mode a
#                  player spends warmup in.  Use it to find WHICH state a "the
#                  view is stuck" report came from.
#
#     ospff        Does `team_hurtteam` stop a shot, or only describe itself?
#                  Four checks over two whole servers, with a live rocket.
#
#                  Worth a scenario rather than a config diff because a per-team
#                  switch has two ways to be broken and only one is visible from
#                  a console: the seed can fail to arrive, or it can arrive and
#                  nothing can read it -- which is the shape the `runes` modifier
#                  had.  The cvar seeds `osp_teams[n]` once in OSP_gameInit and
#                  g_combat.c asks OSP_teamFriendlyFire(team) on the T_Damage
#                  path, so only the world can answer.
#
#                  The rocket is fired STRAIGHT DOWN at the shooter's own feet:
#                  120 units of splash reaches both clients and needs no aim, so
#                  the test never depends on hitting anything.  THE SHOOTER'S OWN
#                  HEALTH IS THE RECEIPT that it went off -- `team_hurtself` is 1
#                  on both servers, so a rocket that hurt nobody is a rocket that
#                  was never fired, and the attempt is retried rather than passed.
#
#                  Getting two libq2 clients into one place is `noclip` plus a
#                  CLOSED LOOP on the view angles, and the loop is the reusable
#                  part: pmove reads `cmd.angles + delta_angles` and
#                  PutClientInServer seeds delta_angles from the spawn point's
#                  facing, so asking for a bearing applies that bearing plus an
#                  offset nobody outside the server knows.  The first version
#                  flew 4,170 units the wrong way.  Ask, read `ps.viewangles`
#                  back, correct by the error, and size the burst to the distance
#                  left -- a fixed burst overshoots and then oscillates.
#
#     botmenugate  R-BOT-28's gate on the bot menu: who may open it, and who
#                  may not.  Thirteen checks (twelve under `ctf`) over two whole
#                  servers -- one that never set an `rcon_password` and one that
#                  did -- because the interesting half of the requirement is the
#                  DEFAULT server: an unset password is the empty string, and
#                  `menu ""` reaches the game as argc 2 with an empty argv(1),
#                  so the 1999 strcmp matched it and two quote marks let any
#                  client in while the honest `menu` was refused to everybody.
#                  It asserts the mechanism in all four directions on the second
#                  server (no argument, empty argument, wrong password, right
#                  password), that the toggle CLOSES with no password because the
#                  gate is on opening, that `sv menu` refuses the console itself,
#                  and -- the one that matters most -- that a client which pushes
#                  `ip=loopback` into its own userinfo is still refused, because
#                  the host exemption is a value latched at connect and not a
#                  string the client can rewrite.  Under `arena` it also probes
#                  `listkeys ""`, which prints "Block not found: <argv(1)>" and
#                  is how the empty-argument claim is read off the wire rather
#                  than argued from the tokenizer.
#
#                  Runs under `ctf` and `arena` only, and says so under the
#                  other four: g_cmds.c consults OSP_ClientCommand first and
#                  osp_clientcmd.c routes `menu` to Cmd_Inven_f, so the bot menu
#                  is not reachable from a client under dm/dmpro/tdm/duel at all.
#                  `-ruleset` picks one, `-rcon` renames the second server's
#                  password.
#
#                  WHAT IT CANNOT SEE, and the other half of the requirement:
#                  the host of a LISTEN server, who is exempt from the gate
#                  because they hold the server's own console.  A dedicated
#                  server has no local client and a libq2 client is a real UDP
#                  peer at 127.0.0.1, never NA_LOOPBACK -- so that arm belongs to
#                  the client harness:
#
#                      tools/play.sh tools/drive/listen-botmenu.cfg -L \
#                          -r ctf -m q2ctf1
#
#                  which is one q2pro process hosting its own map, and takes a
#                  screenshot of the menu that opens.
#
#     ra2packweap  R-182's menu half: does the arena settings menu offer the
#                  mission packs' six weapons exactly when their content layer
#                  is on?  Five whole servers, each asserting a difference --
#                  neither layer offers none, `xatrix 1` offers the two
#                  Reckoning rows and only those, `rogue 1` the four Ground Zero
#                  rows and only those, both offers all six, and
#                  `allowvotingpackweapons: 0` with both layers on offers none
#                  to a PLAYER.  That last one is the control that separates
#                  "the layer is on" from "the arena lets a player vote on it".
#
#                  The server console cannot answer any of it: an RA2 menu IS
#                  the client's statusbar, so the only witness is a client
#                  reading its own bar.  It walks the cursor and takes the union
#                  of the pages, because menu.c draws 18 rows and this menu is
#                  26 with both layers on -- and it matches a row by its LABEL
#                  PREFIX, because a drawn row is the label plus its value.
#                  DUMPROWS=1 prints every row it read.  Needs $GLADDIR.
set -e

HARNESS=${HARNESS:-$HOME/.claude/skills/q2-playtest/harness}
Q2PRO_BUILD=${Q2PRO_BUILD:-$(dirname "$0")/../../q2pro/builddir-native}
Q2DATA=${Q2DATA:-/usr/share/games/quake2/baseq2}
CTFDATA=${CTFDATA:-/usr/share/games/quake2/ctf}
GLADDIR=${GLADDIR:-$(dirname "$0")/../../gladiator-bot-restored}
# The mission-pack paks, for `-s layeracc`.  R-MODE-3's content layers ship as
# their own gamedirs and this tree has no copy of either.
XATRIXDATA=${XATRIXDATA:-$HOME/q2-dev/yquake2/release_/xatrix}
ROGUEDATA=${ROGUEDATA:-$HOME/q2-dev/yquake2/release_/rogue}
LIB=release/game$(uname -m | sed -e 's/^x86_64$/x86_64/' -e 's/^aarch64$/arm64/').so
RULESETS=dm,dmpro,tdm,duel,ctf,arena,sp
SCENARIO=colosseum
PLAYTEST_DIR=${PLAYTEST_DIR:-${TMPDIR:-/tmp}/q2playtest}

while [ $# -gt 0 ]; do
  case $1 in
    -r) RULESETS=$2; shift 2 ;;
    -l) LIB=$2; shift 2 ;;
    -s) SCENARIO=$2; shift 2 ;;
    *)  break ;;
  esac
done

die() { echo "playtest.sh: $*" >&2; exit 1; }
[ -d "$HARNESS" ] || die "no harness at $HARNESS (set HARNESS)"
[ -d "$HARNESS/scenarios/$SCENARIO" ] || die "no scenario $SCENARIO in $HARNESS/scenarios"
SCENARIO_MAIN=$HARNESS/scenarios/$SCENARIO/main.go

# Specialist scenarios deliberately declare only the inputs they need. Passing
# every shared convenience flag makes some fail before their server starts.
supports_flag()
{
  grep -qE "flag\\.[A-Za-z0-9_]+\\(\\\"$1\\\"" "$SCENARIO_MAIN"
}

has_option()
{
  option=$1
  shift

  for argument
  do
    case "$argument" in
      "-$option"|"--$option"|-"$option"=*|--"$option"=*) return 0 ;;
    esac
  done

  return 1
}

if supports_flag q2proded; then
  [ -x "$Q2PRO_BUILD/q2proded" ] || die "no q2proded in $Q2PRO_BUILD (set Q2PRO_BUILD)"
  Q2PRODED=$(cd "$Q2PRO_BUILD" && pwd)/q2proded
fi

if supports_flag lib; then
  [ -f "$LIB" ] || die "no game library at $LIB -- run make native first"
  LIB=$(cd "$(dirname "$LIB")" && pwd)/$(basename "$LIB")
fi

if supports_flag ref && ! has_option ref "$@"; then
  [ -d "$Q2DATA" ] || die "no baseq2 paks at $Q2DATA (set Q2DATA)"
fi

GLAD=""
if [ -f "$GLADDIR/release/gladiator.so" ]; then
  GLAD=$(cd "$GLADDIR" && pwd)
fi
GLADSO=
[ -n "$GLAD" ] && GLADSO=$GLAD/release/gladiator.so
GLADPAK=
GLADBOTCFG=
if [ -f "$GLAD/assets/pak7.pak" ]; then
  GLADPAK=$GLAD/assets/pak7.pak
fi
if [ -f "$GLAD/assets/bots.cfg" ]; then
  GLADBOTCFG=$GLAD/assets
fi

if supports_flag q2proded && ! has_option q2proded "$@"; then
  set -- "$@" -q2proded "$Q2PRODED"
fi
if supports_flag lib && ! has_option lib "$@"; then
  set -- "$@" -lib "$LIB"
fi
if supports_flag ref && ! has_option ref "$@"; then
  set -- "$@" -ref "$Q2DATA"
fi
if supports_flag dir && ! has_option dir "$@"; then
  set -- "$@" -dir "$PLAYTEST_DIR/$SCENARIO"
fi
if supports_flag ctf && ! has_option ctf "$@"; then
  set -- "$@" -ctf "$CTFDATA"
fi
if supports_flag gladdir && ! has_option gladdir "$@"; then
  set -- "$@" -gladdir "$GLAD"
fi
if supports_flag glad && ! has_option glad "$@"; then
  set -- "$@" -glad "$GLADSO"
fi
if supports_flag pak7 && ! has_option pak7 "$@"; then
  [ -n "$GLADPAK" ] || die "no Gladiator pak7.pak under $GLADDIR/assets"
  set -- "$@" -pak7 "$GLADPAK"
fi
if supports_flag botcfg && ! has_option botcfg "$@"; then
  [ -n "$GLADBOTCFG" ] || die "no Gladiator bots.cfg under $GLADDIR/assets"
  set -- "$@" -botcfg "$GLADBOTCFG"
fi

# These defaults belong to only one scenario. Preserve explicit overrides
# instead of emitting duplicate flags after the caller's arguments.
if [ "$SCENARIO" = colosseum ] && ! has_option rulesets "$@"; then
  set -- "$@" -rulesets "$RULESETS"
fi
if [ "$SCENARIO" = layeracc ]; then
  if ! has_option xatrix "$@"; then
    set -- "$@" -xatrix "$XATRIXDATA"
  fi
  if ! has_option rogue "$@"; then
    set -- "$@" -rogue "$ROGUEDATA"
  fi
fi

cd "$HARNESS"
exec go run "./scenarios/$SCENARIO" "$@"
