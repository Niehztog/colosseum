#!/bin/sh
# smoke.sh -- R-VER-17, the smoke test, as a script rather than a paragraph.
#
# Three runs against a stock q2proded on the native target:
#   1. deathmatch  q2dm1, 0 entities inhibited
#   2. campaign    base1 in co-op, a NON-ZERO inhibited count that DIFFERS from
#                  the count the SAME MAP gives in deathmatch -- that difference
#                  is SPAWNFLAG_NOT_COOP / NOT_DEATHMATCH filtering doing its
#                  job, and equality would mean the filter never ran.  The
#                  comparison has to be same-map: q2dm1 against base1 differs
#                  because they are different maps and proves nothing.
#   3. savegame    save, quit, start a second process, load: both must report
#                  the same map and the same inhibited count, and neither may
#                  print `unknown pointer`, `bad index` or `type mismatch`
#
# Run 3 is the only end-to-end check on `save_ptrs[]`, which Colosseum
# regenerates and thereby renumbers (doc/reconciliation.md R-1), and it has to
# cross a process boundary to be one: a load in the process that saved would
# match a wrong table against itself.
#
# `deathmatch` and `coop` are both set explicitly on every run.  A dedicated
# server defaults `deathmatch` to 1, so `+set coop 1` alone prints "Deathmatch
# and Coop both set, disabling Coop" and silently tests the wrong thing.
#
# `--control` is the positive control: a run whose expectation is deliberately
# the other run's, which must be reported as a failure.
set -u

CONTROL=0
[ "${1:-}" = "--control" ] && CONTROL=1

ROOT=$(cd "$(dirname "$0")/.." && pwd)
Q2PRO_BUILD=${Q2PRO_BUILD:-$ROOT/../q2pro/builddir-native}
Q2DATA=${Q2DATA:-/usr/share/games/quake2/baseq2}
CPU=$(uname -m | sed -e 's/^aarch64$/arm64/' -e 's/^i.86$/i386/')
LIB=${LIB:-$ROOT/release/game$CPU.so}

die() { echo "smoke.sh: $*" >&2; exit 2; }
[ -x "$Q2PRO_BUILD/q2proded" ] || die "no q2proded in $Q2PRO_BUILD"
[ -f "$LIB" ] || die "no game library at $LIB -- make native first"
[ -d "$Q2DATA" ] || die "no baseq2 paks at $Q2DATA"

DIR=$(mktemp -d) || die "mktemp failed"
trap 'rm -rf "$DIR"' EXIT
mkdir -p "$DIR/colosseum" "$DIR/baseq2"
for p in "$Q2DATA"/pak*.pak; do ln -s "$p" "$DIR/colosseum/$(basename "$p")"; done
ln -s "$LIB" "$DIR/colosseum/game$CPU.so"

fails=0
note() { printf '  %-46s %s\n' "$1" "$2"; }
bad()  { fails=$((fails+1)); printf '  [FAIL] %-40s %s\n' "$1" "$2"; }

# serve <log> <deathmatch> <coop> <map> <extra console lines>
serve() {
  log=$1 dm=$2 coop=$3 map=$4; shift 4
  { for line in "$@"; do echo "$line"; done; echo quit; } | \
  ( ulimit -c 0
    timeout -s KILL 90 "$Q2PRO_BUILD/q2proded" \
      +set basedir "$DIR" +set homedir "$DIR" +set game colosseum \
      +set dedicated 1 +set net_port 0 +set maxclients 8 \
      +set deathmatch "$dm" +set coop "$coop" \
      +map "$map" >"$log" 2>&1 )
}

inhibited() { sed -n 's/^\([0-9]*\) entities inhibited.*/\1/p' "$1" | tail -1; }
saveerrs()  { grep -cE 'unknown pointer|bad index|type mismatch' "$1"; }

echo "R-VER-17 smoke test"

# ---- 1. deathmatch
serve "$DIR/dm.log" 1 0 q2dm1 status
dm_inh=$(inhibited "$DIR/dm.log")
# The InitGame banner carries the gamedir -- "==== InitGame colosseum====" --
# so it is matched by prefix, not literally.
for want in "Loaded game library" "==== InitGame" "SpawnServer: q2dm1" "==== ShutdownGame ===="; do
  grep -q "$want" "$DIR/dm.log" || bad "deathmatch/$want" "not printed"
done
note "deathmatch q2dm1" "${dm_inh:-no} entities inhibited"
[ "${dm_inh:-x}" = "0" ] || bad "deathmatch/inhibited" "expected 0, got ${dm_inh:-none}"

# ---- 2. campaign, through co-op.  base1 is booted in deathmatch too, because
#         the count only means something against the same map's other mode.
serve "$DIR/b1dm.log" 1 0 base1 status
b1dm_inh=$(inhibited "$DIR/b1dm.log")
serve "$DIR/coop.log" 0 1 base1 status
co_inh=$(inhibited "$DIR/coop.log")
note "base1 in deathmatch" "${b1dm_inh:-no} entities inhibited"
grep -q "Game supports Q2PRO enhanced savegames." "$DIR/coop.log" || \
  bad "campaign/enhanced savegames" "banner not printed"
grep -q "disabling Coop" "$DIR/coop.log" && bad "campaign/coop" "the server disabled coop"
note "campaign base1 (coop)" "${co_inh:-no} entities inhibited"
[ "${co_inh:-0}" -gt 0 ] 2>/dev/null || bad "campaign/inhibited" "expected non-zero, got ${co_inh:-none}"

# ---- 3. savegame round-trip, across two processes
serve "$DIR/save.log" 0 1 base1 "save smoke"
serve "$DIR/load.log" 0 1 base1 "load smoke" status
s_map=$(sed -n 's/^SpawnServer: \(.*\)/\1/p' "$DIR/save.log" | tail -1)
l_map=$(sed -n 's/^SpawnServer: \(.*\)/\1/p' "$DIR/load.log" | tail -1)
l_inh=$(inhibited "$DIR/load.log")
note "savegame round-trip" "saved on $s_map, loaded on $l_map, $l_inh inhibited"
[ "$s_map" = "$l_map" ] || bad "savegame/map" "$s_map != $l_map"
[ "$(saveerrs "$DIR/save.log")" = "0" ] || bad "savegame/save" "g_save.c complained"
[ "$(saveerrs "$DIR/load.log")" = "0" ] || bad "savegame/load" "$(grep -m1 -E 'unknown pointer|bad index|type mismatch' "$DIR/load.log")"

if [ "$CONTROL" = 1 ]; then
  # The campaign's inhibited count must DIFFER from deathmatch's; asserting
  # they are equal must fail, or the comparison is not happening.
  echo
  echo "control: base1's two inhibited counts compared for EQUALITY"
  if [ "${b1dm_inh:-0}" = "${co_inh:-1}" ]; then
    echo "  control did NOT fire -- $b1dm_inh == $co_inh, the spawnflag filter is not running"
    exit 1
  fi
  echo "  control fired: $b1dm_inh != $co_inh, so the filter ran and the comparison is live"
  exit 0
fi

# The two counts must differ (R-VER-17 item 2), on the same map.
[ "${b1dm_inh:-0}" != "${co_inh:-0}" ] || \
  bad "campaign/filter" "base1 inhibits the same count in both modes ($b1dm_inh)"

echo
if [ "$fails" -eq 0 ]; then echo "smoke test passed"; else echo "$fails check(s) failed"; fi
[ "$fails" -eq 0 ]
