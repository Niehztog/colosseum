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
#
# USAGE
#   tools/playtest.sh [-r dm,ctf,arena,tourney,sp] [-l <game.so>] [extra args]
set -e

HARNESS=${HARNESS:-$HOME/.claude/skills/q2-playtest/harness}
Q2PRO_BUILD=${Q2PRO_BUILD:-$(dirname "$0")/../../q2pro/builddir-native}
Q2DATA=${Q2DATA:-/usr/share/games/quake2/baseq2}
CTFDATA=${CTFDATA:-/usr/share/games/quake2/ctf}
LIB=release/game$(uname -m | sed -e 's/^x86_64$/x86_64/' -e 's/^aarch64$/arm64/').so
RULESETS=dm,ctf,arena,tourney,sp

while [ $# -gt 0 ]; do
  case $1 in
    -r) RULESETS=$2; shift 2 ;;
    -l) LIB=$2; shift 2 ;;
    *)  break ;;
  esac
done

die() { echo "playtest.sh: $*" >&2; exit 1; }
[ -d "$HARNESS" ] || die "no harness at $HARNESS (set HARNESS)"
[ -x "$Q2PRO_BUILD/q2proded" ] || die "no q2proded in $Q2PRO_BUILD (set Q2PRO_BUILD)"
[ -f "$LIB" ] || die "no game library at $LIB -- run make native first"
[ -d "$Q2DATA" ] || die "no baseq2 paks at $Q2DATA (set Q2DATA)"

LIB=$(cd "$(dirname "$LIB")" && pwd)/$(basename "$LIB")

cd "$HARNESS"
exec go run ./scenarios/colosseum \
  -q2proded "$(cd "$OLDPWD" && cd "$Q2PRO_BUILD" && pwd)/q2proded" \
  -lib "$LIB" -ref "$Q2DATA" -ctf "$CTFDATA" -rulesets "$RULESETS" "$@"
