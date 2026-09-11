#!/bin/sh
# playtest.sh -- drive Colosseum with headless clients.
#
# WHY THIS EXISTS BESIDE play.sh.  play.sh drives q2pro's own client
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
# they can be wrong.
#
# The scenario and the harness live in the user-level skill `q2-playtest`,
# because they are useful to any Quake II mod and are not Colosseum's to own;
# this script is the versioned entry point that says how to run them.
#
# REQUIREMENTS
#   go                        the harness is Go
#   $HARNESS                  default tools/playtest-harness, in this
#                             repository.  Its dependencies are vendored, so
#                             no network and no other checkout is needed.
#   $Q2PRO_BUILD/q2proded     default ../q2pro/builddir-native
#   $Q2DATA                   retail baseq2 paks, default
#                             /usr/share/games/quake2/baseq2
#   $CTFDATA                  Threewave paks,      default
#                             /usr/share/games/quake2/ctf
#   $GLADDIR                  gladiator-bot-restored, default ../gladiator-bot-restored.
#                             The bot rows need a brain as well as a
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
#     tools/playtest.sh -s ra2join -ref /usr/share/games/quake2/arena \
#       -teams '#1 Pickup Red,#1 Pickup Blue'
#
#   `ra2join`'s default Medieval arena is pickup-only, so its pre-created team
#   rows are required; creating a new team correctly leaves the client in the
#   lobby and is not a spawn-placement failure.
#
#     ra2botvote   the per-arena `bots` switch and the fill
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
#                  /usr/share/games/quake2/arena -- and an `ra2map9.aas` for
#                  the bots to navigate by, which it looks for and takes
#                  `-aas <file>` for.  Without either it is skipped and says
#                  which was missing, rather than being silently subtracted.
#
#     samelevel    does `dmflags` "same map" outrank the ruleset's own map
#                  rotation (R-RA-12)?  Four arms on empty servers, 65 seconds
#                  each: `arena` reading a `maploop:` out of arena.cfg and
#                  tourney reading `maps.txt`, each with `dmflags 0` and then
#                  `dmflags 32`.  0 follows the rotation to q2dm3; 32 comes back
#                  to q2dm1, which is both donors' order.  Needs no brain and no
#                  clients: an empty intermission ends on the frame it begins.
#
#                  The control is the half that makes it evidence.  "The map did
#                  not change" is equally true of a server whose rotation file
#                  was never found, so a subject alone would pass for the wrong
#                  reason; every arm also refuses q2dm2, which is q2dm1's own
#                  `target_changelevel` and therefore the answer when nothing
#                  chose at all.  `tools/dispatch.py` question E is the
#                  source-level check on the same property.
#
#     nextlevel    does the level end when there is nobody to press a key
#                  (R-RA-10, R-OSP-15, R-CTF-9)?  Three phases: `arena` with
#                  only bots on the server, which never send BUTTON_ANY; `dm`
#                  where the last client quits while the scoreboard is up, after
#                  which no ClientThink runs at all; and `ctf`, which is the
#                  first case reached by a third ruleset.  Each takes
#                  `timelimit 1` and shortens `nextlevel_default`, so each is
#                  about ninety seconds.  The arena and ctf phases need $GLADDIR
#                  and a `q2dm1.aas` and skip without one -- with no mesh the
#                  brain destroys every bot and the server is merely EMPTY,
#                  which is the case that always worked.
#
#                  NOTE it walks the menu cursor rather than reading one page.
#                  The settings menu is 21 rows against menu.c's 18-row window,
#                  so anything at the end of the list is below the fold and a
#                  single-page read reports it missing -- which it did, before
#                  the scenario learned to scroll.
#
#     layeracc     the loadout cvars and the content layers, which need different
#                  evidence and so are two phases.  SPAWN boots a Reckoning and
#                  a Ground Zero map -- `xdm1` and `rdm1`, from the layers' own
#                  paks -- and censuses their weapons, items and ammo, which is
#                  "the union content is always spawnable" observed from a map
#                  that places it rather than from the itemlist.  ACC gives four
#                  bots a layer weapon through the loadout cvars
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
#                  default /usr/share/games/quake2/{xatrix,rogue} -- and
#                  $GLADDIR for the bots.  Each missing one skips its own phase
#                  and says which was missing.
#
#     ospenter     tourney's placement: under the OSP four, CONNECTING IS NOT
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
#                  the respawn retry was found with, and the arena bot row
#                  before it.
#
#                  `-rulesets` takes any comma-separated subset of the four and
#                  says so about anything else; it needs no bots and turns the
#                  fill off, because a fill would enter clients of its own and
#                  "who has entered" is the whole subject.
#
#     ospcamera    tourney's chase camera, which has CONTROLS: eleven
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
#     ospwarmup    four subjects in one `tdm` sitting: what an OBSERVER
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
#                  It is also the A/B that dates a report: against a library
#                  without the fix it fails six of the eight rows it reaches.
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
#     botmenugate  the gate on the bot menu: who may open it, and who
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
#     ra2packweap  the pack weapons' menu half: does the arena settings menu offer the
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
#
#     ospmapchange do OSP Tourney's four MANUAL map
#                  changes load the map that was asked for?  A passed `vote
#                  map`, a referee `r_map`, the referee admin menu's map choice,
#                  and a passed `vote config`.  All four were broken and none of
#                  them can be reached from a server console, which is why the
#                  defect shipped and why only half of its
#                  own fix.
#
#                  THE WITNESS IS `Next map:`, printed at exactly one place in
#                  the tree -- osp_maps.c, inside NextMap()'s `selected_map`
#                  arm, which is reachable only through OSP_EndLevel.  So each
#                  row asserts the mechanism (that line appeared) as well as the
#                  outcome (the level went there), and a broken build shows the
#                  second without the first.
#
#                  It writes its own fixtures: a five-line `maps.txt`, a
#                  `serverconfigs.txt`, and an alternate config naming a second
#                  map list -- so the config-vote row can assert the new map
#                  came from the list belonging to the config just voted in.
#                  Every row resets the level through the console first, so no
#                  row inherits where the previous one landed, and every target
#                  is chosen NOT to be the rotation's own next entry -- which
#                  is the one answer a broken build gives that would look like a
#                  pass.
#
#                  Measured A/B: 17 of 17 against the tree, 7 failures against
#                  a library without the fix, where all three
#                  `manual_map = 1` sites land on the rotation's next map with
#                  no witness and the config vote wedges the server in the
#                  intermission.
#
#     votematrix   THREE vote systems, seven rulesets, and the head count every
#                  one of them divides by: 117 checks, one server per ruleset.
#                  Tourney's `vote <what>` is a PERCENTAGE of the voters against
#                  `vote_threshold`, Threewave's election is a COUNT against
#                  `(players * electpercentage) / 100`, Rocket Arena's per-arena
#                  proposal is carried when `yes - no >= voters / 3`, and `sp`
#                  has none -- which is the fourth claim rather than a gap.
#
#                  WHY THE HEAD COUNT IS THE AXIS, and why this is not covered
#                  by testing each system once: every one of the three divides
#                  by a count of the people on the server, and with ONE client
#                  every divisor is 1 and every threshold 100% -- the one head
#                  count at which all three agree and would go on agreeing if
#                  the arithmetic were deleted.  So each ruleset is driven up a
#                  LADDER: one connected player, then two, then three, with the
#                  same proposal put at each rung and the divisor the server
#                  used read back off the wire.
#
#                  Each system counts a DIFFERENT set, which is what the ladder
#                  makes visible.  Tourney divides by everybody CONNECTED under
#                  `dm`/`dmpro` and by the people who ENTERED under `tdm`/`duel`
#                  (`vote_countspectators` is registered with a different
#                  default for a teams ruleset) -- so under `duel`, where
#                  `team_maxplayers` is 1 and the third client cannot enter at
#                  all, three players vote as two and the third is refused.
#                  Threewave counts every client including the proposer, who may
#                  not vote, and truncates: two players and three players both
#                  need exactly one yes.  Rocket Arena counts one ARENA.
#
#                  Every ruleset also gets the negative half -- the other two
#                  systems must not answer there -- read off the CHAT FALLBACK
#                  rather than off silence, because "no handler claimed it" and
#                  "a handler claimed it and did nothing" need different fixes.
#
#                  `-rulesets` takes any comma-separated subset of the seven
#                  (`-r` through this wrapper).  `VOTEDEBUG=1` prints the arena
#                  menu rows it is driving, which is the only way to see an RA2
#                  menu from outside: the menu IS the client's statusbar.
#
#     connectlog   the server's own record of a session (R-LOG-1), swept over
#                  all seven rulesets: one "(name connected from <ip>)" per
#                  arrival, one matching line per departure, the client's REAL
#                  address in both, and SERVER_BOT where a bot's would go.
#
#                  IT COUNTS RATHER THAN GREPS, because the defect it closes was
#                  a DUPLICATE: under the OSP four the record and the spine's
#                  bare "name connected" both reached the console, so a check
#                  asking only whether the record was present passed on the
#                  broken build.  Each row asserts exactly one line and reports
#                  any other line about the same arrival.
#
#                  The address cannot be read off the source.  The engine
#                  force-sets userinfo `ip` from the peer's real source address
#                  in the connect packet and nowhere else, so only a client on a
#                  real socket can say whether the field holds `127.0.0.1`
#                  rather than an empty string, `loopback`, or an IPv6 literal
#                  cut at the wrong colon.
#
#                  Its control is a server with a `password` and a client that
#                  does not send it: refused inside ClientConnect, which returns
#                  before the record, so no line may name it -- and the same
#                  server records the client that DOES send it, so the row is
#                  the refusal working rather than the record being gone.  The
#                  bot rows need $GLADDIR and say so when they skip.
#
#                  `-rulesets` takes any comma-separated subset of the seven
#                  (`-r` through this wrapper).
#
#     netlag       what a client EXPERIENCES on a server that is ALREADY
#                  RUNNING, which is the one question every other scenario here
#                  cannot ask: they start their own server, and a report of "it
#                  felt laggy" is always about a particular one.  So this takes
#                  `-host`/`-port` and starts nothing.
#
#                  Three measurements, because "latency" is three claims.
#                  PM_TYPE AND PM_FLAGS, sampled for the whole session:
#                  PMF_NO_PREDICTION is the only witness to a client rendering
#                  the server's interpolated origin instead of predicting its
#                  own -- a full round trip of felt delay that nothing in the
#                  HUD reports.  SNAPSHOT ARRIVALS, every gap kept rather than
#                  averaged: a server frame is 100 ms and what is felt is the
#                  spread and the ones that never come, so the gaps are also
#                  bucketed by `frame % 32`, the cadence every `level.framenum
#                  & 31` sweep runs on.  And END TO END -- forwardmove to the
#                  origin moving, ATTACK to the view weapon animating.
#
#                  THE LAST TWO ARE GATED ON PM_NORMAL, and that is not a
#                  refinement.  Under arena a client spends most of a round as
#                  an observer, and an observer's ATTACK is the key that CYCLES
#                  RA2's observer modes: an ungated fire test mismeasures the
#                  input AND moves the client into one of the two camera modes
#                  that set PMF_NO_PREDICTION, so it changes the thing it was
#                  sent to measure.
#
#                  It asserts nothing and exits 0 -- the question is "how do
#                  these two servers differ", not "is this one correct".
#                  `-arena` walks the team menu, `-join` sends the command the
#                  OSP four enter through, and `-anykey` holds BUTTON_ANY: the
#                  bit a real client sets for any key pressed, which the bot
#                  layer never sets and which a deathmatch intermission is the
#                  only thing to end on.
set -e

HARNESS=${HARNESS:-$(cd "$(dirname "$0")/playtest-harness" && pwd)}
Q2PRO_BUILD=${Q2PRO_BUILD:-$(dirname "$0")/../../q2pro/builddir-native}
Q2DATA=${Q2DATA:-/usr/share/games/quake2/baseq2}
CTFDATA=${CTFDATA:-/usr/share/games/quake2/ctf}
# GLADDIR -- the brain, its assets and bspc.  `vendor/gladiator-bot-restored`
# is the submodule and the documented place; a sibling checkout beside the
# repository still works, because that is where it lived before the submodule
# existed.  An explicit GLADDIR wins over both, and a directory holding a BUILT
# brain is preferred over one that does not, so neither layout stops working.
if [ -z "${GLADDIR:-}" ]; then
  for _g in "$(dirname "$0")/../vendor/gladiator-bot-restored" \
            "$(dirname "$0")/../../gladiator-bot-restored"; do
    [ -f "$_g/release/gladiator.so" ] && GLADDIR=$_g && break
  done
  GLADDIR=${GLADDIR:-$(dirname "$0")/../vendor/gladiator-bot-restored}
fi
# The mission-pack paks, for `-s layeracc`.  The content layers ship as
# their own gamedirs and this tree has no copy of either.  Defaulted beside
# $Q2DATA and $CTFDATA rather than at a particular checkout, because a retail
# install puts all four side by side; override either if yours does not.
XATRIXDATA=${XATRIXDATA:-/usr/share/games/quake2/xatrix}
ROGUEDATA=${ROGUEDATA:-/usr/share/games/quake2/rogue}
# Rocket Arena's own gamedir, on the same terms: its paks carry `ra2map1`..
# `ra2map28` and the arena keys `mapinfo` reads, and no retail pak has either.
ARENADATA=${ARENADATA:-/usr/share/games/quake2/arena}
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
#
# Three scenarios sweep the rulesets and all three spell the flag `-rulesets`,
# so `-r` reaches any of them.  Nothing else takes one: a single-subject
# scenario handed a `-rulesets` it never declared fails before its server
# starts.
if { [ "$SCENARIO" = colosseum ] || [ "$SCENARIO" = votematrix ] ||
     [ "$SCENARIO" = connectlog ]; } &&
   ! has_option rulesets "$@"; then
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
# A PICKUP arena is not reachable from the arena menu -- `AddtoArena` answers
# "You must join a pickup team to enter that arena" -- so a scenario that seats
# clients in one joins through the team rows instead.  Without this the clients
# start teams of their own, end up somewhere else entirely, and the scenario
# reports spawn points thousands of units away as though the placement were
# wrong.  The row number is the arena's: `-map`/`-arena` default to `ra2map9`
# arena 1, so `#1` is the pair that matches them.
if [ "$SCENARIO" = ra2join ] && ! has_option teams "$@"; then
  set -- "$@" -teams '#1 Pickup Red,#1 Pickup Blue'
fi
# `mapinfo` reads BSPs straight out of a pak and groups spawn points by RA2's
# `arena` key, so the paks it wants are the arena's rather than the reference
# install's.  The value stays a GLOB: the scenario expands it itself.
if [ "$SCENARIO" = mapinfo ] && ! has_option paks "$@"; then
  set -- "$@" -paks "$ARENADATA/pak*.pak"
fi
# Two scenarios take a `-aas` and neither defaults it, so without one the brain
# has no navigation mesh at all: it never reports "AAS initialized.", and the
# scenario times out waiting on a bot that was never going to think.  The
# meshes live with the brain, beside the sixteen it ships.  $GLAD is used
# rather than $GLADDIR because it is the ABSOLUTE form -- this script cd's into
# the harness before exec, so a relative path would resolve from there.
aas_for()
{
  [ -n "$GLAD" ] || die "no brain under $GLADDIR -- $1 needs one for its meshes"
  AAS=$GLAD/assets/maps/$2.aas
  [ -f "$AAS" ] || die "no navigation mesh at $AAS -- make one with \`bspc -bsp2aas\`
  for the geometry and one load of that map with the brain to fill the
  reachability lump, or pass -aas (and the matching -map) to use another"
}
if [ "$SCENARIO" = ra2botchat ] && ! has_option aas "$@"; then
  aas_for ra2botchat ra2map7
  set -- "$@" -aas "$AAS"
fi
# `nextlevel`'s arena and ctf phases need a mesh, and SKIP rather than die
# without one: its dm phase is independent of the brain and is worth running
# alone.
if [ "$SCENARIO" = nextlevel ] && ! has_option aas "$@"; then
  if [ -n "$GLAD" ] && [ -f "$GLAD/assets/maps/q2dm1.aas" ]; then
    set -- "$@" -aas "$GLAD/assets/maps/q2dm1.aas"
  fi
fi
# `ra2reachscore` also wants the arena gamedir, and its mesh must be COMPLETE:
# it empties the second map's reachability lump itself to open the window it
# measures, so one that arrives empty measures nothing.
if [ "$SCENARIO" = ra2reachscore ]; then
  if ! has_option ra2ref "$@"; then
    set -- "$@" -ra2ref "$ARENADATA"
  fi
  if ! has_option aas "$@"; then
    aas_for ra2reachscore ra2map11
    set -- "$@" -aas "$AAS"
  fi
fi

cd "$HARNESS"
exec go run "./scenarios/$SCENARIO" "$@"
