#!/bin/sh
# extras.sh -- R-EXTRA-1..7 observed on a running server (R-VER-33).
#
# Six of the seven extras are invisible from outside the library: a log that is
# open, a lag pool that is empty, two entity classnames no shipped map uses, a
# visible weapon that looks like a skin, and an observer implementation chosen
# per ruleset.  `sv extras` reports each of them as a MEASUREMENT -- the log's
# real path and write count, whether the classnames are in the spawn table, how
# many itemlist rows carry a weapon model -- and this script asserts on those
# lines and on the files the log actually wrote.
#
# `--control` runs four deliberate failures, the last of which is the one
# R-VER-29 requires of any new script that starts a server: a boot that prints
# everything it was asked for and then dies must be reported as a failure.
set -u

CONTROL=0
[ "${1:-}" = "--control" ] && CONTROL=1

ROOT=$(cd "$(dirname "$0")/.." && pwd)
Q2PRO_BUILD=${Q2PRO_BUILD:-$ROOT/../q2pro/builddir-native}
Q2DATA=${Q2DATA:-/usr/share/games/quake2/baseq2}
CTFDATA=${CTFDATA:-/usr/share/games/quake2/ctf}
GLADDIR=${GLADDIR:-$ROOT/../gladiator-bot-restored}
CPU=$(uname -m | sed -e 's/^aarch64$/arm64/' -e 's/^i.86$/i386/')
LIB=${LIB:-$ROOT/release/game$CPU.so}

die() { echo "extras.sh: $*" >&2; exit 2; }
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
# D6: the shipped config set, installed the way an operator would install it.
cp -r "$ROOT/colosseum/." "$DIR/colosseum/" 2>/dev/null || true
# The brain, if this machine has one.  R-VER-20's temporal row needs a bot to
# pick something up; without it the row is SKIPPED and says so rather than
# being silently subtracted from the total.
GLAD=""
[ -f "$GLADDIR/release/gladiator.so" ] && {
  # The brain AND its assets: pak7.pak holds the weapon and sound configs and
  # the bots/*.c characters, and the .aas files are the navigation meshes.
  # Without them the brain loads, reports "couldn't load the weapon config"
  # and unloads itself -- and a row that needs a bot then measures nothing
  # while looking like it ran.
  ln -s "$GLADDIR/release/gladiator.so" "$DIR/colosseum/gladiator.so"
  [ -f "$GLADDIR/assets/pak7.pak" ] && ln -s "$GLADDIR/assets/pak7.pak" "$DIR/colosseum/pak7.pak"
  mkdir -p "$DIR/colosseum/maps"
  for a in "$GLADDIR"/assets/maps/*.aas; do
    case $a in *.original_baseline) continue ;; esac
    [ -f "$a" ] && ln -s "$a" "$DIR/colosseum/maps/$(basename "$a")"
  done
  GLAD=1
}

pass=0; fail=0
ok()  { pass=$((pass+1)); printf '  [ ok ] %-42s %s\n' "$1" "$2"; }
bad() { fail=$((fail+1)); printf '  [FAIL] %-42s %s\n' "$1" "$2"; }
have() { case "$2" in *"$1"*) return 0 ;; *) return 1 ;; esac; }

# serve <log> <ruleset> <map> <console lines...>
#
# The EXIT STATUS is checked (R-VER-29): a server that printed everything it was
# asked for and then died on the way out is a failure, and three scripts in this
# tree once said "ok" to exactly that.
serve() {
  log=$1 rs=$2 map=$3; shift 3
  case $rs in
    sp) dm=0; coop=0 ;;
    *)  dm=1; coop=0 ;;
  esac
  ( ulimit -c 0
    { for line in "$@"; do echo "$line"; done; echo quit; } | \
    timeout -s KILL 90 "$Q2PRO_BUILD/q2proded" \
      +set basedir "$DIR" +set homedir "$DIR" +set game colosseum \
      +set dedicated 1 +set net_port 0 +set maxclients 8 \
      +set deathmatch "$dm" +set coop "$coop" +set g_ruleset "$rs" \
      $EXTRA_SETS +map "$map" >"$log" 2>&1 )
  rc=$?
  [ "$rc" = 0 ] && return 0
  if [ "$rc" -gt 128 ] 2>/dev/null; then
    bad "$(basename "$log" .log)/exit" "died on signal $((rc - 128))"
  else
    bad "$(basename "$log" .log)/exit" "exited $rc"
  fi
  return 1
}

line_for() { sed -n "s/^  \(R-EXTRA-$1 .*\)$/\1/p" "$2" | tail -1; }

# How many key bindings a client gets, starting from nothing, with the shipped
# config set installed the way README.md says to install it.
#
# THE LAYOUT IS THE CHECK.  q2pro satisfies `exec default.cfg` from a PAK as
# readily as from a loose file, and within one gamedir the pak wins -- so a
# stray `default.cfg` beside the paks is harmless.  It is only harmful in the
# layout this project documents, where the library and its configs live under
# `homedir` and the paks under `basedir`: homedir is searched first, has no pak,
# and a loose file there answers before id's ever does.  Measuring the flat
# layout says nothing, which is what this check did when it was first written.
#
# No `config.cfg` either: a client writes one on quit and it carries the
# bindings forward, so the second run in a tree gets its keyboard from there
# whatever `default.cfg` did.
bindings_in_layout() {
    extra=${1:-}
    b=$DIR/bind; rm -rf "$b"
    mkdir -p "$b/colosseum" "$b/home/colosseum"
    for p in "$Q2DATA"/pak*.pak; do ln -s "$p" "$b/colosseum/$(basename "$p")"; done
    cp -f "$LIB" "$b/home/colosseum/game$CPU.so"
    cp -r "$ROOT/colosseum/." "$b/home/colosseum/" 2>/dev/null
    [ -n "$extra" ] && cp "$ROOT/colosseum/server.cfg" "$b/home/colosseum/$extra"
    ( ulimit -c 0
      timeout -s KILL 90 xvfb-run -a -s "-screen 0 640x480x24" \
        "$Q2PRO_BUILD/q2pro" \
        +set basedir "$b" +set homedir "$b/home" +set game colosseum \
        +set vid_geometry 640x480 +set s_enable 0 +set allow_download 0 \
        +set logfile 1 +set logfile_flush 2 +set logfile_prefix "" \
        +bindlist +quit >/dev/null 2>&1 )
    # `grep -c` prints 0 AND exits 1 when nothing matches, so a `|| echo 0`
    # fallback appends a SECOND line and every numeric test downstream then
    # fails on "0\n0" rather than on the count.  It cost one confusing control
    # run: the shadowing had bitten, the count was 0, and the row still read
    # "the shadowing did not bite".
    c=$(grep -cE '^\S+ "' "$b/home/colosseum/logs/console.log" 2>/dev/null)
    printf '%s\n' "${c:-0}"
}

# ------------------------------------------------------------------ controls
if [ "$CONTROL" = 1 ]; then
  EXTRA_SETS='+set g_gamelog boot.log'
  serve "$DIR/c1.log" dm q2dm1 'sv extras'
  l=$(line_for 1 "$DIR/c1.log")
  # 1. the log IS open, so asserting it is closed must fail.
  if have "closed" "$l"; then
    bad "control/log-open" "reported closed with g_gamelog set"
  else
    ok "control/log-open" "an open log does not read as closed"
  fi
  # 2. the write count is real: a server that wrote its InitGame line reports 1,
  #    so asserting 0 must fail.
  if have "writes 0" "$l"; then
    bad "control/log-writes" "reported 0 writes after InitGame wrote one"
  else
    ok "control/log-writes" "the write count is not fixed at zero"
  fi
  # 3. the observer line is per ruleset, so dm must not report tourney's.
  l6=$(line_for 6 "$DIR/c1.log")
  if have "tourney" "$l6"; then
    bad "control/observer" "dm reported tourney's observer"
  else
    ok "control/observer" "the observer line follows the ruleset"
  fi
  # 4. D6: a `default.cfg` in the gamedir must be reported, because id's own is
  #    what a client's keyboard comes from.  Manufactured rather than hoped for.
  if [ -x "$Q2PRO_BUILD/q2pro" ] && command -v xvfb-run >/dev/null; then
    n=$(bindings_in_layout default.cfg)
    if [ "${n:-0}" -lt 60 ] 2>/dev/null; then
      ok "control/shadowed default.cfg" "$n binding(s) with a default.cfg in the gamedir"
    else
      bad "control/shadowed default.cfg" "$n bindings -- the shadowing did not bite"
    fi
  else
    ok "control/shadowed default.cfg" "SKIPPED: no q2pro client or no xvfb-run"
  fi

  # 5. R-VER-29: a boot killed after the census must still be a failure.
  before=$fail
  (
    ulimit -c 0
    printf 'wait 30\nsv extras\n' | \
    timeout -s KILL 60 "$Q2PRO_BUILD/q2proded" \
      +set basedir "$DIR" +set homedir "$DIR" +set game colosseum \
      +set dedicated 1 +set net_port 0 +set maxclients 8 \
      +set deathmatch 1 +set coop 0 +set g_ruleset dm \
      +map q2dm1 >"$DIR/c4.log" 2>&1 &
    pid=$!
    sleep 8
    kill -SEGV $pid 2>/dev/null
    wait $pid
    exit $?
  ) 2>/dev/null
  rc=$?
  if [ "$rc" -gt 128 ] 2>/dev/null && grep -q '^extras:' "$DIR/c4.log"; then
    ok "control/exit-status" "died on signal $((rc - 128)) after reporting"
  else
    bad "control/exit-status" "no post-census crash to detect (rc=$rc)"
  fi
  fail=$((before + fail - before))

  # 6. R-129: the "arena.cfg is RA2's own" test must reject an example of its
  # format.  The positive control is the file that actually shipped from 1.24 to
  # 1.27 -- valid, parseable, and with no per-map block in it -- reconstructed
  # here by stripping every block from the real one.
  sed -E '/^[a-z0-9_]+ *\{/,$d' "$ROOT/colosseum/arena.cfg" > "$DIR/c6-arena.cfg"
  blocks=$(grep -cE '^[a-z0-9_]+ *\{' "$DIR/c6-arena.cfg" || true)
  picks=$(grep -cE '^[[:space:]]*pickup:' "$DIR/c6-arena.cfg" || true)
  if [ "${blocks:-0}" -lt 20 ] || [ "${picks:-0}" -lt 1 ]; then
    ok "control/arena.cfg is an example" \
       "$blocks block(s), $picks pickup(s) in the stripped copy -- rejected"
  else
    bad "control/arena.cfg is an example" \
        "a copy with no blocks still passed ($blocks, $picks)"
  fi

  echo
  echo "6 control(s), $pass fired, $fail did not"
  [ "$fail" -eq 0 ] && echo "controls ok: every assertion below can fail"
  exit $([ "$fail" -eq 0 ] && echo 0 || echo 1)
fi

echo "R-VER-33: the R-EXTRA features, observed"
echo

# ---------------------------------------------- R-EXTRA-1, the game log
EXTRA_SETS='+set g_gamelog boot.log'
if serve "$DIR/log-on.log" dm q2dm1 'sv writelog round two starts now' 'sv extras'; then
  l=$(line_for 1 "$DIR/log-on.log")
  have "open" "$l" && ok "R-EXTRA-1/opened by cvar" "$l" \
                   || bad "R-EXTRA-1/opened by cvar" "$l"
  f=$DIR/colosseum/boot.log
  if [ -f "$f" ]; then
    ok "R-EXTRA-1/file written" "$(wc -l <"$f") line(s) in colosseum/boot.log"
  else
    bad "R-EXTRA-1/file written" "no colosseum/boot.log"
  fi
  # the whole line, not the first word: the donor logged gi.argv(2)
  if grep -q 'round two starts now' "$f" 2>/dev/null; then
    ok "R-EXTRA-1/writelog takes the line" "$(grep 'round two' "$f" | head -1)"
  else
    bad "R-EXTRA-1/writelog takes the line" "$(tail -1 "$f" 2>/dev/null)"
  fi
  # ...and the timestamp is the donor's four-field h:mm:ss:cc shape
  if grep -qE '^[0-9]+ +[0-9]{2}:[0-9]{2}:[0-9]{2}:[0-9]{2} ' "$f" 2>/dev/null; then
    ok "R-EXTRA-1/timestamp shape" "n hh:mm:ss:cc, as the 1999 log wrote it"
  else
    bad "R-EXTRA-1/timestamp shape" "$(head -1 "$f" 2>/dev/null)"
  fi
  # closed at ShutdownGame, which R-EXTRA-1 names by hand
  if grep -q 'Closed log' "$DIR/log-on.log"; then
    ok "R-EXTRA-1/closed on shutdown" "Log_ShutDown ran"
  else
    bad "R-EXTRA-1/closed on shutdown" "no 'Closed log' in the console"
  fi
fi

rm -f "$DIR/colosseum/boot.log"
EXTRA_SETS=''
if serve "$DIR/log-off.log" dm q2dm1 'sv extras'; then
  l=$(line_for 1 "$DIR/log-off.log")
  have "closed" "$l" && ok "R-EXTRA-1/off by default" "$l" \
                     || bad "R-EXTRA-1/off by default" "$l"
  [ -f "$DIR/colosseum/boot.log" ] && bad "R-EXTRA-1/no file when off" "boot.log exists" \
                                   || ok "R-EXTRA-1/no file when off" "nothing written"
fi

# the path guard: `openlog` may not carry a path of its own
if serve "$DIR/log-path.log" dm q2dm1 'sv openlog ../escape.log' 'sv extras'; then
  if grep -q 'may not contain a path' "$DIR/log-path.log"; then
    ok "R-EXTRA-1/no path in openlog" "refused ../escape.log"
  else
    bad "R-EXTRA-1/no path in openlog" "not refused"
  fi
fi

# ---------------------------------------------- R-EXTRA-2, the lag pool
EXTRA_SETS='+set g_clientlag 1'
if serve "$DIR/lag.log" dm q2dm1 'sv extras'; then
  l=$(line_for 2 "$DIR/lag.log")
  have "g_clientlag 1" "$l" && ok "R-EXTRA-2/cvar live" "$l" \
                            || bad "R-EXTRA-2/cvar live" "$l"
  # nothing is allocated until a command arrives, which is the point of the
  # cvar: an idle server with the simulation ON holds no memory for it.
  have "pool 0" "$l" && ok "R-EXTRA-2/nothing allocated idle" "$l" \
                     || bad "R-EXTRA-2/nothing allocated idle" "$l"
fi

# ---------------------------------------------- R-EXTRA-3/4, the classnames
EXTRA_SETS=''
for rs in dm ctf arena tourney sp; do
  case $rs in
    ctf) map=q2ctf1 ;;
    sp)  map=base1 ;;
    *)   map=q2dm1 ;;
  esac
  serve "$DIR/cls-$rs.log" "$rs" "$map" 'sv extras' || continue
  l3=$(line_for 3 "$DIR/cls-$rs.log")
  l4=$(line_for 4 "$DIR/cls-$rs.log")
  if have "MISSING" "$l3$l4"; then
    bad "R-EXTRA-3+4/$rs registered" "$l3 | $l4"
  else
    ok "R-EXTRA-3+4/$rs registered" "three classnames in the spawn table"
  fi
  # R-EXTRA-3 is off by default and the requirement says so
  if have "g_triggercounting 0" "$l3" && have "g_triggerlog 0" "$l3"; then
    ok "R-EXTRA-3/$rs off by default" "$l3"
  else
    bad "R-EXTRA-3/$rs off by default" "$l3"
  fi
  l5=$(line_for 5 "$DIR/cls-$rs.log")
  n=$(printf '%s' "$l5" | sed -n 's/.*on, \([0-9]*\) item.*/\1/p')
  if [ "${n:-0}" -ge 1 ] 2>/dev/null; then
    ok "R-EXTRA-5/$rs vwep data present" "$n item(s) carry a weapmodel"
  else
    bad "R-EXTRA-5/$rs vwep data present" "$l5"
  fi
  l6=$(line_for 6 "$DIR/cls-$rs.log")
  case $rs in
    tourney) want=tourney ;;
    arena)   want=arena ;;
    *)       want="dm/sp/ctf" ;;
  esac
  have "$want" "$l6" && ok "R-EXTRA-6/$rs observer" "$l6" \
                     || bad "R-EXTRA-6/$rs observer" "$l6"
done

# ---------------------------------------------- D6, the shipped config set
#
# Every file in colosseum/ is a default the code already carries, so the check
# is not "does it change anything" -- it is that an operator who execs one gets
# the ruleset it names, that nothing in it is a command this library does not
# have, and that RA2's arena.cfg PARSES rather than being reported unreadable.
for rs in dm ctf arena tourney sp; do
  case $rs in
    ctf) map=q2ctf1 ;;
    sp)  map=base1 ;;
    *)   map=q2dm1 ;;
  esac
  EXTRA_SETS=''
  # exec before the map, because g_ruleset is latched.
  log=$DIR/cfg-$rs.log
  ( ulimit -c 0
    printf 'sv ruleset\nquit\n' | \
    timeout -s KILL 90 "$Q2PRO_BUILD/q2proded" \
      +set basedir "$DIR" +set homedir "$DIR" +set game colosseum \
      +set dedicated 1 +set net_port 0 \
      +exec "configs/$rs.cfg" +map "$map" >"$log" 2>&1 )
  rc=$?
  got=$(sed -n 's/^ruleset *\([a-z]*\).*/\1/p' "$log" | tail -1)
  bad_cmds=$(grep -ciE 'Unknown command|unknown command' "$log")
  if [ "$rc" != 0 ]; then
    bad "D6/configs/$rs.cfg" "server exited $rc"
  elif [ "$got" != "$rs" ]; then
    bad "D6/configs/$rs.cfg" "ruleset resolved to '$got'"
  elif [ "$bad_cmds" != 0 ]; then
    bad "D6/configs/$rs.cfg" "$bad_cmds unknown command(s)"
  else
    ok "D6/configs/$rs.cfg" "execs clean, ruleset $got"
  fi
  # ...and under arena, the DATA arena.cfg has to have been read.
  if [ "$rs" = arena ]; then
    if grep -q "Couldn't read.*arena.cfg" "$log"; then
      bad "D6/arena.cfg parses" "$(grep "Couldn't read" "$log" | head -1)"
    elif grep -q 'Error reading config file' "$log"; then
      bad "D6/arena.cfg parses" "$(grep 'Error reading config file' "$log" | head -1)"
    else
      ok "D6/arena.cfg parses" "read with no unbalanced braces"
    fi
    # ...and that the file is RA2's OWN rather than an example of its format.
    # R-129: the parse check above passes perfectly on a 69-line example with no
    # per-map blocks and no pickup arenas, which is what shipped from 1.24 --
    # valid, and empty of the data the arena ruleset exists for.  A play test
    # found it as "the join menu offers only Start New Team", because a pickup
    # team is a TEAM in that menu and no arena was marked to have one.
    blocks=$(grep -cE '^[a-z0-9_]+ *\{' "$ROOT/colosseum/arena.cfg" || true)
    picks=$(grep -cE '^[[:space:]]*pickup:' "$ROOT/colosseum/arena.cfg" || true)
    if [ "${blocks:-0}" -lt 20 ] || [ "${picks:-0}" -lt 1 ]; then
      bad "D6/arena.cfg is RA2's own" \
          "$blocks per-map block(s), $picks pickup designation(s) -- an example, not the file"
    else
      ok "D6/arena.cfg is RA2's own" "$blocks per-map block(s), $picks pickup arena(s)"
    fi
  fi
done

# ---- the name that is not ours to use.
#
# `default.cfg` is ID'S file: it ships inside pak0.pak and holds all 68 key
# bindings a Quake II client starts with.  q2pro execs `default.cfg` from the
# gamedir before `config.cfg` and a real file there wins over a pak entry, so a
# file of ours by that name leaves a joining client with NO bindings -- no
# movement, no fire, no menu key.  It cost nothing on a dedicated server, which
# is why every check in this repository passed with it for a whole increment.
if [ -e "$ROOT/colosseum/default.cfg" ]; then
  bad "D6/no default.cfg" "colosseum/default.cfg shadows id's, which holds every key binding"
else
  ok "D6/no default.cfg" "id's default.cfg in pak0 is not shadowed"
fi

# ...and the same question asked of the running client rather than of the
# directory listing, which is the only form that can see the next way of
# breaking it.  Needs the client binary and a display; skipped, and said, when
# either is missing.
if [ ! -x "$Q2PRO_BUILD/q2pro" ] || ! command -v xvfb-run >/dev/null; then
  ok "D6/a client has bindings" "SKIPPED: no q2pro client or no xvfb-run"
else
  n=$(bindings_in_layout)
  if [ "${n:-0}" -ge 60 ] 2>/dev/null; then
    ok "D6/a client has bindings" "$n bindings, so id's default.cfg was reached"
  else
    bad "D6/a client has bindings" "only ${n:-0} binding(s) -- something in the gamedir shadows id's default.cfg"
  fi
fi

# The bot roster is the library's own read.  What this script can assert is
# that the library FOUND and PARSED the shipped file -- it says how many names
# it got and where from.  Whether a bot then spawns needs the brain, which is a
# separate dependency and is what tools/botmatrix.sh is for; asserting it here
# would make this script silently skip on a machine without one.
EXTRA_SETS='+set bots 1'
# `sv addrandom` is what makes the library READ the roster -- CheckForNewBotFile
# runs on demand, not at InitGame -- and the bot then needs a brain to spawn,
# which this script does not install.
if serve "$DIR/roster.log" dm q2dm1 'sv addrandom' 'wait 20'; then
  line=$(grep -E 'loaded [0-9]+ bots from' "$DIR/roster.log" | tail -1)
  n=$(printf '%s' "$line" | sed -n 's/^loaded \([0-9]*\) bots.*/\1/p')
  if [ "${n:-0}" -ge 1 ] 2>/dev/null && have "botcfg/bots.cfg" "$line"; then
    ok "D6/botcfg/bots.cfg" "$line"
  else
    bad "D6/botcfg/bots.cfg" "${line:-no roster line}"
  fi
fi

# ---------------------------------------------- R-VER-20, a check that waits
#
# "At least one check must wait for something to happen ... spawn a level,
# advance past a known think deadline, and assert the thing that think was
# supposed to do."  CTF's techs were the first and their deadline is two
# seconds.  An item respawn is a better one: the deadline is thirty seconds,
# and getting there needs somebody to pick the item up first.
#
# Nobody has to.  Bots do it on their own, which is what turns this from a
# drive script a person runs into a check.  `sv census <classname>` counts the
# distinction a respawn is about -- in the world, or taken and waiting on its
# own think -- and the row takes it three times: at the start, after the bots
# have been loose in the map, and again past the deadline.
if [ -z "$GLAD" ]; then
  ok "R-VER-20/item respawn" "SKIPPED: no brain at $GLADDIR/release/gladiator.so"
else
  log=$DIR/respawn.log
  ( ulimit -c 0
    { printf 'wait 40\n'
      i=0; while [ $i -lt 6 ]; do printf 'sv addrandom\nwait 5\n'; i=$((i+1)); done
      printf 'wait 60\nsv census weapon_\n'
      printf 'wait 900\nsv census weapon_\n'
      # ...and then take the bots away.  With them still in the map the count
      # oscillates -- something is always being picked up -- so "it came back"
      # is only decidable once nobody is left to take it again.  A weapon's
      # respawn in deathmatch is 30 seconds, which is 300 frames; 500 is that
      # plus the slack for the last pickup before the removal.
      printf 'sv removebot all\nwait 500\nsv census weapon_\nwait 10\nquit\n'
    } | timeout -s KILL 600 "$Q2PRO_BUILD/q2proded" \
      +set basedir "$DIR" +set homedir "$DIR" +set game colosseum \
      +set dedicated 1 +set net_port 0 +set maxclients 16 \
      +set deathmatch 1 +set coop 0 +set g_ruleset dm +set bots 1 \
      +set minimumplayers 0 +map q2dm1 >"$log" 2>&1 )
  rc=$?
  field() { sed -n "s/^census weapon_: \([0-9]*\) spawned, \([0-9]*\) in world, \([0-9]*\) waiting.*/\$1/p" "$log" | sed -n "$2p"; }
  s0=$(field 1 1); w0=$(field 1 1)
  spawned=$(sed -n 's/^census weapon_: \([0-9]*\) spawned.*/\1/p' "$log")
  inworld=$(sed -n 's/^census weapon_: [0-9]* spawned, \([0-9]*\) in world.*/\1/p' "$log")
  waiting=$(sed -n 's/^census weapon_: [0-9]* spawned, [0-9]* in world, \([0-9]*\) waiting.*/\1/p' "$log")
  n0=$(printf '%s' "$inworld" | sed -n 1p)
  n1=$(printf '%s' "$inworld" | sed -n 2p)
  w1=$(printf '%s' "$waiting" | sed -n 2p)
  n2=$(printf '%s' "$inworld" | sed -n 3p)
  frames=$(sed -n 's/.*frame \([0-9]*\)$/\1/p' "$log" | tail -1)

  if [ "$rc" != 0 ]; then
    bad "R-VER-20/item respawn" "server exited $rc"
  elif [ -z "$n2" ]; then
    bad "R-VER-20/item respawn" "fewer than three censuses ($(printf '%s' "$inworld" | tr '\n' ' '))"
  elif [ "${w1:-0}" -lt 1 ] 2>/dev/null; then
    bad "R-VER-20/item taken" "no weapon was ever waiting to respawn (in world $n0 -> $n1)"
  elif [ "${n2:-0}" -lt "${n0:-99}" ] 2>/dev/null; then
    bad "R-VER-20/item respawn" "$n2 of $n0 back in the world at frame $frames, 50s after the last bot left"
  else
    ok "R-VER-20/item taken" "$n0 in world -> $n1, $w1 waiting on their own think"
    ok "R-VER-20/item respawn" "all $n2 back by frame $frames, with nobody left to take them"
  fi
fi

echo
echo "$((pass + fail)) check(s), $fail failed"
exit $([ "$fail" -eq 0 ] && echo 0 || echo 1)
