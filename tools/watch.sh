#!/bin/bash
# watch.sh -- one command that puts a PERSON in front of the game.
#
# Every other check in this tree is a program looking at a program.  Ten build
# configurations, twenty-three audits, a twenty-row boot matrix, a hundred and
# eighty-seven client assertions and a bot matrix, and not one of them can say
# whether what a player sees looks like a game.  Phase 7 recorded that plainly:
# "no person has watched a bot play".  This is what closes it, and it closes it
# by handing the judgement to you rather than by asserting something weaker.
#
# It sets up a data tree, starts a Colosseum server with bots in it, and runs
# q2pro's own client against it on YOUR display, executing one of the drive
# scripts in tools/drive/ once the map has loaded.  Each of those scripts opens
# with what to look for.
#
# USAGE
#   tools/watch.sh [scene] [-r ruleset] [-m map] [-b bots] [--headless]
#
#   scene      watch-bots (default) | doors | ctf-skin, or a path to a .cfg
#   -r         dm (default) | dmpro | tdm | duel | ctf | arena | sp
#   -m         the map; defaults per ruleset
#   -b         how many bots to add, default 4 (0 for none)
#   --headless run under Xvfb and print the consoles instead of showing it.
#              Useful on a machine with no display; it is NOT the point.
#
# EXAMPLES
#   tools/watch.sh                                  # four bots on q2dm1
#   tools/watch.sh doors -r sp -m base1             # R-VER-20's door half
#   tools/watch.sh ctf-skin -r ctf -m q2ctf1        # R-CTF-7's v0.93 half
#
# The server keeps running until the client quits.  In the client, `quit` at
# the console (or the menu) ends both.
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
Q2PRO_BUILD=${Q2PRO_BUILD:-$ROOT/../q2pro/builddir-native}
Q2DATA=${Q2DATA:-/usr/share/games/quake2/baseq2}
CTFDATA=${CTFDATA:-/usr/share/games/quake2/ctf}
GLADDIR=${GLADDIR:-$ROOT/../gladiator-bot-restored}
TREE=${COLOSSEUM_WATCH_DIR:-$HOME/.colosseum-watch}
CPU=$(uname -m | sed -e 's/^aarch64$/arm64/' -e 's/^i.86$/i386/')
LIB=${LIB:-$ROOT/release/game$CPU.so}

SCENE=watch-bots RULESET=dm MAP= BOTS=4 HEADLESS=0
[ $# -gt 0 ] && case ${1:-} in -*) ;; *) SCENE=$1; shift ;; esac
while [ $# -gt 0 ]; do
  case $1 in
    -r) RULESET=$2; shift 2 ;;
    -m) MAP=$2; shift 2 ;;
    -b) BOTS=$2; shift 2 ;;
    --headless) HEADLESS=1; shift ;;
    *) sed -n '2,32p' "$0" | sed 's/^# \{0,1\}//'; exit 1 ;;
  esac
done

die() { echo "watch.sh: $*" >&2; exit 1; }

DRIVE=$SCENE
[ -f "$DRIVE" ] || DRIVE=$ROOT/tools/drive/$SCENE.cfg
[ -f "$DRIVE" ] || die "no drive script '$SCENE' (looked for $DRIVE)"

if [ -z "$MAP" ]; then
  case $RULESET in
    ctf) MAP=q2ctf1 ;;
    sp)  MAP=base1 ;;
    *)   MAP=q2dm1 ;;
  esac
fi

[ -x "$Q2PRO_BUILD/q2proded" ] || die "no q2proded in $Q2PRO_BUILD (set Q2PRO_BUILD)"
[ -x "$Q2PRO_BUILD/q2pro" ]    || die "no q2pro client in $Q2PRO_BUILD"
[ -f "$LIB" ] || die "no game library at $LIB -- run make first"
[ -d "$Q2DATA" ] || die "no baseq2 paks at $Q2DATA (set Q2DATA)"

# ------------------------------------------------------------------ the tree
#
# Stable rather than temporary: a person watching will want to run this again
# with a different scene, and re-linking the paks every time is a wait for no
# reason.  Everything in it is a link or a copy of something in the repo.
mkdir -p "$TREE/home/colosseum" "$TREE/colosseum" "$TREE/baseq2" || die "cannot make $TREE"
for p in "$Q2DATA"/pak*.pak; do
  ln -sf "$p" "$TREE/colosseum/$(basename "$p")"
done
i=8
for p in "$CTFDATA"/pak*.pak; do
  [ -f "$p" ] && ln -sf "$p" "$TREE/colosseum/pak$i.pak" && i=$((i+1))
done
# THE PLAYER MODELS, which are not in any pak.
#
# Retail Quake II ships `baseq2/players/` as a LOOSE DIRECTORY -- male, female,
# cyborg, crakhor, each with `tris.md2`, its skins and the `_i.pcx` the
# scoreboard draws.  `pak0.pak` has none of it: 3,307 entries and not one under
# `players/`.  Every harness in this repository links `pak*.pak` and stops, so
# no player model has ever been installed by any of them -- and none of them
# could notice, because a dedicated server does not render and `libq2` has no
# renderer at all.  A watcher sees invisible players and an empty scoreboard.
#
# Linked into `baseq2/`, which is where a real installation has it and where the
# CTF paks' `players/*/ctf_r.pcx` skins expect to find the models they skin.
if [ -d "$Q2DATA/players" ]; then
  ln -sfn "$Q2DATA/players" "$TREE/baseq2/players"
else
  echo "watch.sh: no $Q2DATA/players -- players will be INVISIBLE."
  echo "          retail Quake II keeps them there as loose files, not in a pak."
fi
# The engine loads the game library from homedir, never from basedir (SPECS 1.5).
cp -f "$LIB" "$TREE/home/colosseum/game$CPU.so" || die "cannot write $TREE/home"
cp -r "$ROOT/colosseum/." "$TREE/home/colosseum/" 2>/dev/null
BRAIN=""
if [ -f "$GLADDIR/release/gladiator.so" ]; then
  # The brain is loaded from BASEDIR's gamedir, not homedir's -- that is where
  # BotSetPathVars looks and where every other harness here puts it.  It needs
  # its own assets too: pak7.pak holds the weapon and sound configs and the
  # bots/*.c characters the roster names, and the .aas files are the navigation
  # meshes.  Without pak7 the brain loads, says "couldn't load the weapon
  # config" and unloads itself again.
  ln -sf "$GLADDIR/release/gladiator.so" "$TREE/colosseum/gladiator.so"
  [ -f "$GLADDIR/assets/pak7.pak" ] && ln -sf "$GLADDIR/assets/pak7.pak" "$TREE/colosseum/pak7.pak"
  mkdir -p "$TREE/colosseum/maps"
  for a in "$GLADDIR"/assets/maps/*.aas; do
    case $a in *.original_baseline) continue ;; esac
    [ -f "$a" ] && ln -sf "$a" "$TREE/colosseum/maps/$(basename "$a")"
  done
  # ...and say so when THIS map has none, because the failure is silent from
  # here: the brain loads, refuses the map with "no AAS file available", the
  # game destroys every bot that wanted it, and the scene runs with an empty
  # server.  The assets ship 16 -- q2dm1-8 and q2ctf1-8 -- so any other map
  # needs one made first, and there is no auto-bspc to fall back on
  # (doc/reconciliation.md R-126).  Not under `sp`, where G_BotsAllowed() is
  # false and the ruleset is what decides rather than the mesh (R-MODE-7).
  if [ "$BOTS" -gt 0 ] && [ "$RULESET" != sp ] \
     && [ ! -f "$TREE/colosseum/maps/$MAP.aas" ]; then
    echo "watch.sh: no $MAP.aas in $GLADDIR/assets/maps -- there will be NO BOTS."
    echo "          the brain refuses a map it has no navigation mesh for."
  fi
  BRAIN=1
elif [ "$BOTS" -gt 0 ]; then
  echo "watch.sh: no brain at $GLADDIR/release/gladiator.so -- running with no bots"
  BOTS=0
fi
cp -f "$DRIVE" "$TREE/home/colosseum/play_drive.cfg" || die "cannot write the drive script"
# The watcher's keyboard.  id's own default.cfg -- the only reason a Quake II
# client can move at all -- is the 1997 layout: arrows walk, `a` is +lookup and
# `s` is `use silencer`.  There is no WASD in it.  Exec'd from the command line,
# which runs after the config bootstrap, so it overrides id's rather than being
# overridden by it.
cp -f "$ROOT/tools/drive/watch-keys.cfg" "$TREE/home/colosseum/watch_keys.cfg" \
  || die "cannot write the key bindings"
# q2pro's own MENUS.  They are not built into the binary: `q2pro.menu` is a
# script the UI parses at startup, and meson installs it into the base game
# directory.  Without it `UI_FindMenu("main")` returns nothing and ESCAPE does
# NOTHING AT ALL -- which is what it did the first time a person ran this, and
# is a separate cause from the bindings.  Copied where q2pro installs it.
MENU_SRC=${Q2PRO_MENU:-$Q2PRO_BUILD/../src/client/ui/q2pro.menu}
if [ -f "$MENU_SRC" ]; then
  cp -f "$MENU_SRC" "$TREE/baseq2/q2pro.menu"
else
  echo "watch.sh: no q2pro.menu at $MENU_SRC -- ESCAPE will open nothing."
  echo "          set Q2PRO_MENU to q2pro's src/client/ui/q2pro.menu."
fi
rm -f "$TREE/home/colosseum/logs/console.log"

PORT=$(( 28000 + RANDOM % 1500 ))
SRVLOG=$TREE/server.log

# ------------------------------------------------------------------ the server
{
  echo "wait 40"
  i=0
  while [ "$i" -lt "$BOTS" ]; do echo "sv addrandom"; echo "wait 5"; i=$((i+1)); done
} > "$TREE/home/colosseum/watch_boot.cfg"

( ulimit -c 0
  "$Q2PRO_BUILD/q2proded" \
    +set basedir "$TREE" +set homedir "$TREE/home" +set game colosseum \
    +set dedicated 1 +set net_port "$PORT" +set g_ruleset "$RULESET" \
    +set deathmatch 1 +set coop 0 +set maxclients 16 \
    +set bots 1 +set minimumplayers 0 +set bots_minplayers 0 \
    +map "$MAP" +exec watch_boot.cfg >"$SRVLOG" 2>&1 ) &
SRVPID=$!
trap 'kill -TERM $SRVPID 2>/dev/null' EXIT INT TERM

for _ in $(seq 1 90); do
  grep -q "Q2PRO initialized" "$SRVLOG" 2>/dev/null && break
  kill -0 $SRVPID 2>/dev/null || break
  sleep 0.5
done
grep -q "Q2PRO initialized" "$SRVLOG" 2>/dev/null || {
  echo "--- the server never came up ---"; tail -25 "$SRVLOG"; exit 2
}

echo
echo "  Colosseum: $RULESET on $MAP, port $PORT, $BOTS bot(s)${BRAIN:+, brain loaded}"
echo "  scene:     $DRIVE"
echo
sed -n '/^\/\/ WHAT TO LOOK FOR/,/^$/p' "$DRIVE" | sed 's|^// \{0,1\}|  |'
echo
echo "  keys:      WASD move, mouse aims (INVERTED) and fires, SPACE jump, SHIFT run"
echo "             O observer  K chasecam  L cyclecam  I autocam  H help"
echo "             TAB menu  F1 bot menu  E use  Q/R weapon  wheel inventory"
echo "             ESC engine menu -- TWICE if one of our layouts is up, or F11"
echo "             ALT+ENTER toggles fullscreen; the window is 1280x720"
echo
echo "  note:      in a CAMERA (K chasecam, I autocam) you cannot move or look --"
echo "             the camera is doing both, and SPACE cycles its subject."
echo "             Plain observer (O, no camera) flies with WASD and the mouse."
echo "             (tools/drive/watch-keys.cfg -- id's own layout has no WASD)"
echo

# ------------------------------------------------------------------ the client
CLIENT_ARGS=(
  +set basedir "$TREE" +set homedir "$TREE/home" +set game colosseum
  +set allow_download 0 +set name watcher
  +exec watch_keys.cfg
  +set cl_beginmapcmd "exec play_drive.cfg"
  +connect "localhost:$PORT"
)

if [ "$HEADLESS" = 1 ] || [ -z "${DISPLAY:-}" ]; then
  if [ "$HEADLESS" != 1 ]; then
    cat <<EOF
  There is no DISPLAY on this machine, so there is nobody to watch.

  To WATCH it, run this from a machine with a display -- an X session on this
  host, or an ssh -X login:

      cd $ROOT && tools/watch.sh $SCENE -r $RULESET -m $MAP -b $BOTS

  Running headless instead, which drives the same scene and prints both
  consoles.  That is not the same thing and is not what this script is for.

EOF
  fi
  command -v xvfb-run >/dev/null || die "no DISPLAY and no xvfb-run"
  ( ulimit -c 0
    timeout -s KILL 180 xvfb-run -a -s "-screen 0 640x480x24" \
      "$Q2PRO_BUILD/q2pro" +set vid_geometry 640x480 +set s_enable 0 \
      +set cl_maxfps 60 +set logfile 1 +set logfile_flush 2 +set logfile_prefix "" \
      "${CLIENT_ARGS[@]}" >"$TREE/client.stdout" 2>&1 )
  echo "=== server ==="
  sed -n '/Q2PRO initialized/,$p' "$SRVLOG" | grep -v '^$'
  echo "=== client ==="
  [ -f "$TREE/home/colosseum/logs/console.log" ] && \
    sed -n '/Execing play_drive.cfg/,$p' "$TREE/home/colosseum/logs/console.log"
else
  "$Q2PRO_BUILD/q2pro" "${CLIENT_ARGS[@]}"
fi

kill -TERM $SRVPID 2>/dev/null
wait $SRVPID 2>/dev/null
echo
echo "  server log: $SRVLOG"
