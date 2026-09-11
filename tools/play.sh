#!/bin/bash
# Drive a real Q2PRO client, headlessly, against a local Colosseum server.
#
# WHY THIS EXISTS.  Every check in this project up to now is static, a
# spawn-time census, or a boot.  None of them needs a player, and so none of
# them could see anything a player triggers: a door that has to be walked into,
# an item that has to be picked up before it can respawn, a flag that has to be
# carried home, a userinfo string that only exists once a client sends one.
# Phases 0-3 recorded that gap as "needs a client, which this harness has not
# got".  It has got one: q2pro's own client runs under Xvfb with its X11/GLX
# backend on Mesa's software rasteriser, connects to localhost, and executes a
# script of console commands paced by the `wait` command.  The first run of it
# dropped the server.
#
# HOW IT DRIVES.  `+set cl_beginmapcmd "exec <script>"` fires once the client
# has finished loading the map, which is the only moment at which sending
# commands is meaningful.  From there the drive script is ordinary console
# input: `cmd <x>` reaches the game library's ClientCommand, `+forward` /
# `-forward` and `+attack` / `-attack` are held down and released across
# `wait N` frames, and `viewpos` prints the player's position so that movement
# is observable rather than assumed.  Both consoles are captured: the server's
# stdout, and the client's own console via `logfile 1` + `logfile_flush 2`
# (unbuffered, because the client is killed rather than asked to leave if the
# drive script has no `quit`).
#
# REQUIREMENTS
#   Xvfb                      apt: xvfb.  No GPU is used or wanted.
#   a built q2pro + q2proded  $Q2PRO_BUILD, default ../q2pro/builddir-native
#   a game data tree          $COLOSSEUM_Q2DIR, containing:
#                               <dir>/colosseum/pak*.pak   assets incl. CTF
#                               <dir>/baseq2/pak*.pak      retail baseq2
#                               <dir>/home/colosseum/      writable; the game
#                                                          library goes HERE,
#                                                          not in basedir --
#                                                          the engine only
#                                                          looks in homedir and
#                                                          the install libdir
#
#
# USAGE
#   tools/play.sh <drive-script> [-r ruleset] [-m map] [-n name] [-p port]
#                 [-s "extra server args"] [-c "extra client args"] [-t secs]
#                 [-L]
#
#   The drive script is copied into the homedir gamedir and exec'd by name, so
#   it may be anywhere.  Its last line should be `quit`; if it is not, the
#   client is killed at the timeout and the run still reports.
#
#   -L IS A LISTEN SERVER: one process, no q2proded, the client hosting the map
#   itself and playing on it.  It exists because a listen server is not a
#   configuration of the normal shape but a DIFFERENT KIND OF CLIENT -- the host
#   player reaches the server over the in-process loopback, so the engine gives
#   the game library userinfo `ip` "loopback" for them and for nobody else, and
#   `dedicated` is 0.  Anything the game library decides from those two facts is
#   invisible to every other harness here: q2proded has no local client, and a
#   libq2 client is a real UDP peer at 127.0.0.1.  The bot menu's host exemption is
#   the first such thing (tools/drive/listen-botmenu.cfg).
#
#   In -L mode there is one console, the client's, and the server's prints
#   arrive in it -- so `-s` server args are passed to the client, and the
#   verdict reads the same log for both.
#
# EXAMPLE
#   cat > /tmp/d.cfg <<'EOF'
#   wait 40
#   cmd team red
#   wait 40
#   viewpos
#   +forward
#   wait 60
#   -forward
#   viewpos
#   wait 20
#   quit
#   EOF
#   tools/play.sh /tmp/d.cfg -r ctf -m q2ctf1
#
# EXIT STATUS
#   0  the client ran the script and the server neither errored nor died
#   1  a setup problem (no Xvfb, no binaries, no data tree, no script)
#   2  the server printed ERROR/FATAL, or shut down uncleanly
#   3  the client failed to connect
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
Q2PRO_BUILD=${Q2PRO_BUILD:-$REPO/../q2pro/builddir-native}
Q2DIR=${COLOSSEUM_Q2DIR:-}
GAME=${COLOSSEUM_GAMEDIR:-colosseum}

RULESET=dm MAP=q2dm1 NAME=TESTBOT PORT=0 SRVARGS= CLARGS= TMO=90 LISTEN=
SCRIPT=${1:-}; shift || true
while getopts "r:m:n:p:s:c:t:L" o; do
  case $o in
    r) RULESET=$OPTARG ;; m) MAP=$OPTARG ;; n) NAME=$OPTARG ;;
    p) PORT=$OPTARG ;; s) SRVARGS=$OPTARG ;; c) CLARGS=$OPTARG ;;
    t) TMO=$OPTARG ;; L) LISTEN=1 ;;
    *) sed -n '2,60p' "$0" | sed 's/^# \{0,1\}//'; exit 1 ;;
  esac
done

die() { echo "play.sh: $*" >&2; exit 1; }
[ -n "$SCRIPT" ] && [ -f "$SCRIPT" ] || die "no drive script (arg 1)"
command -v xvfb-run >/dev/null || die "xvfb-run not found (apt install xvfb)"
[ -n "$LISTEN" ] || [ -x "$Q2PRO_BUILD/q2proded" ] || \
  die "no q2proded in $Q2PRO_BUILD (set Q2PRO_BUILD)"
[ -x "$Q2PRO_BUILD/q2pro" ]    || die "no q2pro client in $Q2PRO_BUILD"
[ -n "$Q2DIR" ] || die "set COLOSSEUM_Q2DIR to the game data tree"
HOME_G=$Q2DIR/home/$GAME
[ -d "$HOME_G" ] || die "no $HOME_G -- see REQUIREMENTS at the top of this file"
ls "$HOME_G"/game*.so >/dev/null 2>&1 || \
  die "no game library in $HOME_G; copy release/game<cpu>.so there first"

# A fixed port makes two runs collide; 0 means "pick one from the ephemeral
# range and hope", which is what every other harness here does.
[ "$PORT" = 0 ] && PORT=$(( 28000 + RANDOM % 1500 ))

OUT=$(mktemp -d) || die "mktemp failed"
trap 'rm -rf "$OUT"' EXIT
SRVLOG=$OUT/server.log
cp -f "$SCRIPT" "$HOME_G/play_drive.cfg" || die "cannot write $HOME_G"
rm -f "$HOME_G"/logs/console.log

cd "$Q2DIR" || die "cannot cd $Q2DIR"
SRVPID=
if [ -z "$LISTEN" ]; then
  ( ulimit -c 0
    timeout -s KILL "$TMO" "$Q2PRO_BUILD/q2proded" \
      +set basedir "$Q2DIR" +set homedir "$Q2DIR/home" +set game "$GAME" \
      +set dedicated 1 +set net_port "$PORT" +set g_ruleset "$RULESET" \
      +set deathmatch 1 +set coop 0 +set sv_maxclients 8 \
      $SRVARGS +map "$MAP" >"$SRVLOG" 2>&1 ) &
  SRVPID=$!

  # Wait for the map to be up rather than sleeping a guess: a cold pak scan on
  # this host takes several seconds and a fixed sleep is either slow or flaky.
  for _ in $(seq 1 60); do
    grep -q "Q2PRO initialized" "$SRVLOG" 2>/dev/null && break
    kill -0 $SRVPID 2>/dev/null || break
    sleep 0.5
  done
  if ! grep -q "Q2PRO initialized" "$SRVLOG" 2>/dev/null; then
    echo "--- server never came up ---"; tail -20 "$SRVLOG"; exit 2
  fi
fi

# The two shapes differ in their last two arguments and in nothing else: a
# listen server takes the server's cvars and OPENS the map, a client CONNECTS.
# `dedicated` is not passed at all -- the client binary registers it 0 and it is
# CVAR_NOSET, so the mode is the binary rather than a setting.
if [ -n "$LISTEN" ]; then
  ENDARGS="+set g_ruleset $RULESET +set deathmatch 1 +set coop 0"
  ENDARGS="$ENDARGS +set sv_maxclients 8 +set net_port $PORT $SRVARGS +map $MAP"
else
  ENDARGS="+connect localhost:$PORT"
fi

( ulimit -c 0
  timeout -s KILL "$TMO" xvfb-run -a -s "-screen 0 640x480x24" \
    "$Q2PRO_BUILD/q2pro" \
    +set basedir "$Q2DIR" +set homedir "$Q2DIR/home" +set game "$GAME" \
    +set vid_geometry 640x480 +set s_enable 0 +set cl_maxfps 60 \
    +set allow_download 0 +set name "$NAME" \
    +set logfile 1 +set logfile_flush 2 +set logfile_prefix "" \
    $CLARGS +set cl_beginmapcmd "exec play_drive.cfg" \
    $ENDARGS >"$OUT/client.stdout" 2>&1 )
CLRC=$?

sleep 1
[ -n "$SRVPID" ] && { kill -TERM $SRVPID 2>/dev/null; wait $SRVPID 2>/dev/null; }
CLLOG=$HOME_G/logs/console.log
# One process means one console: the game library's prints are in the client's
# log, so that is what the verdict reads and the only thing there is to print.
if [ -n "$LISTEN" ]; then
  [ -f "$CLLOG" ] && cp -f "$CLLOG" "$SRVLOG"
  echo "=== the listen server's one console ==="
  [ -f "$CLLOG" ] && sed -n '/Q2PRO initialized/,$p' "$CLLOG" | grep -v '^$'
else
  echo "=== server (post-init) ==="
  sed -n '/Q2PRO initialized/,$p' "$SRVLOG" | grep -v '^$'
  echo "=== client (from the drive script) ==="
  [ -f "$CLLOG" ] && sed -n '/Execing play_drive.cfg/,$p' "$CLLOG"
fi
cat "$OUT/client.stdout"

rc=0
# Order matters: a server that died takes the client's connection with it, so
# both checks fire and the server's verdict is the informative one.
# On a listen server this is "Connected to loopback", which is also the one
# line that proves the host player took the in-process path rather than a socket.
grep -q "Connected to" "${CLLOG:-/dev/null}" || { echo "!! client never connected"; rc=3; }
grep -qE '^(ERROR|FATAL):|\*\*\*' "$SRVLOG" && { echo "!! server errored"; rc=2; }
echo "=== play.sh: ruleset=$RULESET map=$MAP port=$PORT listen=${LISTEN:-0} client_rc=$CLRC verdict=$rc ==="
exit $rc
