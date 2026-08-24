#!/bin/sh
# bootmatrix.sh -- R-VER-2's boot matrix, run rather than remembered.
#
# Every g_ruleset x xatrix x rogue combination (5 x 2 x 2 = 20) starts, loads a
# map the ruleset can hold, runs past 100 frames, and is asked `sv ruleset`.  A
# row passes when the server reached that frame count, printed no ERROR and no
# "!!" contradiction, and reported the ruleset and layers that were asked for.
#
# It had been run by hand once per phase, which is how a matrix stops being run.
#
# `--control` is the positive control this check ships with (SPECS.md sec 0): two
# boots whose verdicts MUST be failures, so that a row reading "ok" means the
# comparison ran rather than that the comparison is gone.
#
# ENV, all defaulted:
#   Q2PRO_BUILD   ../q2pro/builddir-native
#   Q2DATA        /usr/share/games/quake2/baseq2
#   CTFDATA       /usr/share/games/quake2/ctf
#   LIB           release/game<cpu>.so
set -u

CONTROL=0
[ "${1:-}" = "--control" ] && CONTROL=1

ROOT=$(cd "$(dirname "$0")/.." && pwd)
Q2PRO_BUILD=${Q2PRO_BUILD:-$ROOT/../q2pro/builddir-native}
Q2DATA=${Q2DATA:-/usr/share/games/quake2/baseq2}
CTFDATA=${CTFDATA:-/usr/share/games/quake2/ctf}
CPU=$(uname -m | sed -e 's/^aarch64$/arm64/' -e 's/^i.86$/i386/')
LIB=${LIB:-$ROOT/release/game$CPU.so}
FRAMES=${FRAMES:-100}

die() { echo "bootmatrix.sh: $*" >&2; exit 2; }
[ -x "$Q2PRO_BUILD/q2proded" ] || die "no q2proded in $Q2PRO_BUILD"
[ -f "$LIB" ] || die "no game library at $LIB -- make native first"
[ -d "$Q2DATA" ] || die "no baseq2 paks at $Q2DATA"

DIR=$(mktemp -d) || die "mktemp failed"
trap 'rm -rf "$DIR"' EXIT
mkdir -p "$DIR/colosseum" "$DIR/baseq2"
for p in "$Q2DATA"/pak*.pak; do ln -s "$p" "$DIR/colosseum/$(basename "$p")"; done
i=8
for p in "$CTFDATA"/pak*.pak; do ln -s "$p" "$DIR/colosseum/pak$i.pak"; i=$((i+1)); done
ln -s "$LIB" "$DIR/colosseum/game$CPU.so"

# One boot, one verdict.  Takes the ruleset/layers to ASK for and the ones to
# COMPARE against, which are the same for every real row and deliberately
# different for the controls.
run_row() {
  rs=$1 xatrix=$2 rogue=$3 map=$4 dm=$5 coop=$6 want_rs=$7 want_layers=$8
  log=$DIR/$rs-$xatrix-$rogue.log
  # `sv ruleset` after the frames have run, then quit: the census is only
  # meaningful once the world has finished filling.
  ( ulimit -c 0
    printf 'wait %d\nsv ruleset\nquit\n' "$FRAMES" | \
    timeout -s KILL 60 "$Q2PRO_BUILD/q2proded" \
      +set basedir "$DIR" +set homedir "$DIR" +set game colosseum \
      +set dedicated 1 +set net_port 0 +set g_ruleset "$rs" \
      +set deathmatch "$dm" +set coop "$coop" +set maxclients 8 \
      +set xatrix "$xatrix" +set rogue "$rogue" \
      +map "$map" >"$log" 2>&1 )

  frames=$(sed -n 's/^world *frame \([0-9]*\),.*/\1/p' "$log" | tail -1)
  got_rs=$(sed -n 's/^ruleset *\([a-z]*\).*/\1/p' "$log" | tail -1)
  layers=$(sed -n 's/^layers *//p' "$log" | tail -1)
  errs=$(grep -cE '^ERROR|^\*\*\*|!!' "$log")

  verdict=ok
  [ "${frames:-0}" -ge "$FRAMES" ] 2>/dev/null || verdict="only reached frame ${frames:-none}"
  [ "$got_rs" = "$want_rs" ] || verdict="reported '$got_rs'"
  [ "$layers" = "$want_layers" ] || verdict="layers '$layers'"
  [ "$errs" -eq 0 ] || verdict="$errs error line(s)"

  printf '%-9s %-7s %-6s %-9s %-7s %s\n' "$rs" "$xatrix" "$rogue" "$map" "${frames:-0}" "$verdict"
  [ "$verdict" = ok ]
}

pass=0; fail=0

if [ "$CONTROL" = 1 ]; then
  printf '%-9s %-7s %-6s %-9s %-7s %s\n' ruleset xatrix rogue map frames verdict
  # 1. an unknown ruleset falls back to dm (R-MODE-1), so comparing against the
  #    NAME ASKED FOR must fail.  If this reads ok, the ruleset comparison is
  #    not happening.
  run_row banana 0 0 q2dm1 1 0 banana "xatrix=0 rogue=0" && fail=$((fail+1)) || pass=$((pass+1))
  # 2. boot with xatrix on and compare against off: the layer comparison must
  #    notice.
  run_row dm 1 0 q2dm1 1 0 dm "xatrix=0 rogue=0" && fail=$((fail+1)) || pass=$((pass+1))
  echo
  echo "2 control(s), $pass fired, $fail did not"
  [ "$fail" -eq 0 ] && echo "controls ok: both comparisons are live"
  exit $([ "$fail" -eq 0 ] && echo 0 || echo 1)
fi

printf '%-9s %-7s %-6s %-9s %-7s %s\n' ruleset xatrix rogue map frames verdict
for rs in dm ctf arena tourney sp; do
  case $rs in
    ctf) map=q2ctf1; dm=1; coop=0 ;;
    sp)  map=base1;  dm=0; coop=0 ;;
    *)   map=q2dm1;  dm=1; coop=0 ;;
  esac
  for xatrix in 0 1; do
    for rogue in 0 1; do
      if run_row "$rs" "$xatrix" "$rogue" "$map" "$dm" "$coop" \
                 "$rs" "xatrix=$xatrix rogue=$rogue"; then
        pass=$((pass+1))
      else
        fail=$((fail+1))
        cp "$DIR/$rs-$xatrix-$rogue.log" "/tmp/bootmatrix-$rs-$xatrix-$rogue.log"
      fi
    done
  done
done

echo
echo "$((pass+fail)) row(s), $pass passed, $fail failed"
[ "$fail" -eq 0 ]
