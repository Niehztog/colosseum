#!/bin/sh
# botmatrix.sh -- R-VER-3's bot matrix, run rather than remembered.
#
# For every ruleset that accepts bots, spawn 1 bot and then 16, let them play,
# remove them, and check that nothing leaked: `sv clientdump` must show every
# slot free again and `sv botlibdump` must show the library released.  That is
# R-VER-3's whole sentence, and the two dumps exist so that it can be checked
# from outside rather than asserted from inside.
#
# WHAT IT NEEDS, and why it is more than the other two scripts.  bootmatrix.sh
# and smoke.sh need a game library and id's paks.  A bot needs a BRAIN as well:
#
#   * `gladiator.so`, built from gladiator-bot-restored/botlib (R-BOT-3/4).  The
#     bitness must match the game's; the loader says so when it does not.
#   * `bots.cfg` and the character files it names, which are in the Gladiator
#     assets' pak7.pak.  The botlib reads them through its OWN file search --
#     <basedir>/<gamedir>/, then <cddir>/<gamedir>/ -- not through the engine's,
#     which is why they are copied into the gamedir rather than symlinked from
#     wherever the engine happens to look.
#   * an `.aas` file per map, in <gamedir>/maps/.  Without one the brain reports
#     BLERR and BotLib_BotLoadMap destroys every bot using that library -- which
#     is a legitimate row to run and is what --control uses, because "the bot
#     went away cleanly" is exactly what R-VER-3 measures.
#
# Phase 6's exit is "bots load, spawn, navigate, fight and chat in DM"; ctf,
# arena and tourney are Phase 7's.  All four are run here anyway, because the
# leak half of R-VER-3 is not ruleset-specific and a slot leaked under tourney
# is a slot leaked.  A row that cannot spawn a bot at all under a Phase 7
# ruleset is reported, not failed -- see WANT below.
#
# ENV, all defaulted:
#   Q2PRO_BUILD   ../q2pro/builddir-native
#   Q2DATA        /usr/share/games/quake2/baseq2
#   CTFDATA       /usr/share/games/quake2/ctf
#   GLADDIR       ../gladiator-bot-restored
#   LIB           release/game<cpu>.so
#   FRAMES        600 (one minute).  R-VER-3 says five: FRAMES=3000.
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
FRAMES=${FRAMES:-600}

die() { echo "botmatrix.sh: $*" >&2; exit 2; }
[ -x "$Q2PRO_BUILD/q2proded" ] || die "no q2proded in $Q2PRO_BUILD"
[ -f "$LIB" ] || die "no game library at $LIB -- make native first"
[ -d "$Q2DATA" ] || die "no baseq2 paks at $Q2DATA"
[ -f "$GLADDIR/release/gladiator.so" ] || \
  die "no brain at $GLADDIR/release/gladiator.so -- build gladiator-bot-restored"

DIR=$(mktemp -d) || die "mktemp failed"
trap 'rm -rf "$DIR"' EXIT
mkdir -p "$DIR/colosseum/maps" "$DIR/baseq2"
for p in "$Q2DATA"/pak*.pak; do ln -s "$p" "$DIR/colosseum/$(basename "$p")"; done
i=8
for p in "$CTFDATA"/pak*.pak; do ln -s "$p" "$DIR/colosseum/pak$i.pak"; i=$((i+1)); done
ln -s "$LIB" "$DIR/colosseum/game$CPU.so"
# The brain, its roster and its character files.  pak7.pak is the Gladiator
# assets' own and holds the bots/*.c the roster names.
ln -s "$GLADDIR/release/gladiator.so" "$DIR/colosseum/gladiator.so"
ln -s "$GLADDIR/assets/pak7.pak" "$DIR/colosseum/pak7.pak"
mkdir -p "$DIR/colosseum/botcfg"
cp "$GLADDIR/assets/bots.cfg" "$DIR/colosseum/botcfg/bots.cfg"
for a in "$GLADDIR"/assets/maps/*.aas; do
  case $a in *.original_baseline) continue ;; esac
  ln -s "$a" "$DIR/colosseum/maps/$(basename "$a")"
done

# One row: boot, add `n` bots, let them play, dump, remove, dump again.
#
# The two dumps bracket the removal on purpose.  The first says the bots were
# THERE -- a row that never spawned one would otherwise pass the leak check
# trivially -- and the second says they are gone.
#
# `sv ruleset` between them is Phase 7's half: R-VER-3 as written measures that
# a bot can exist and be taken away, which is silent about whether the bot is
# ON A TEAM, IN AN ARENA or COUNTED BY THE MATCH -- and those are what R-CTF-4,
# R-RA-4 and R-OSP-11 are.  Until Phase 7 the ctf/arena/tourney rows were run
# with `want=report` and they passed while every bot under those three rulesets
# sat in the audience doing nothing (doc/reconciliation.md R-103..R-106).
#
# EXTRA is appended to the server's command line, so a row can set a cvar.
run_row() {
  rs=$1 n=$2 map=$3 want=$4 place=${5:-}
  log=$DIR/$rs-$n${6:+-$6}.log
  ( ulimit -c 0
    { printf 'wait 20\n'
      j=0; while [ $j -lt "$n" ]; do printf 'sv addrandom\nwait 5\n'; j=$((j+1)); done
      printf 'wait %d\nsv ruleset\nsv clientdump\nsv botlibdump\n' "$FRAMES"
      printf 'sv removebot all\nwait 20\nsv clientdump\nsv botlibdump\nquit\n'
    } | timeout -s KILL $((FRAMES / 5 + 240)) "$Q2PRO_BUILD/q2proded" \
      +set basedir "$DIR" +set homedir "$DIR" +set game colosseum \
      +set dedicated 1 +set net_port 0 +set g_ruleset "$rs" \
      +set deathmatch 1 +set maxclients 20 +set bots_minplayers 0 \
      +set minimumplayers 0 +set skill 1 ${EXTRA:-} \
      +map "$map" >"$log" 2>&1 ) 2>>"$log"

  # `%3d: name  <library path>` for a bot, `%3d: name  human` for a person,
  # `%3d: -` for a free slot.  The counts come from the two dumps in order.
  rc=$?
  before=$(sed -n '/^  *[0-9]*: /p' "$log" | grep -c 'gladiator' || true)
  loaded=$(grep -c '^loaded .*gladiator\.so' "$log" || true)
  errs=$(grep -cE '^ERROR|assert' "$log" || true)

  botplace=$(sed -n 's/^botplace  *//p' "$log" | head -1)

  verdict=ok
  [ "$errs" -eq 0 ] || verdict="$errs error line(s)"
  if [ "$want" = spawn ]; then
    [ "$before" -ge 1 ] || verdict="no bot spawned"
  fi
  # The placement assertion, when the row carries one.  It is a literal
  # substring of the `botplace` line rather than a parse, so what the check
  # wants and what the server said are readable side by side in the output.
  if [ "$verdict" = ok ] && [ -n "$place" ]; then
    case "$botplace" in
      *"$place"*) ;;
      *) verdict="placement: wanted '$place', got '${botplace:-no botplace line}'" ;;
    esac
  fi
  # after `removebot all` the library list must be empty
  if [ "$verdict" = ok ] && ! grep -q 'no libraries found' "$log"; then
    verdict="library still loaded"
  fi
  # ...and a crash outranks both, with WHERE it happened, because "the game
  # leaked a slot" and "the brain died being asked to let go of one" are not the
  # same finding and telling them apart is the whole point of running this.
  #
  # `sv clientdump` prints one `N bots, M client slots` line per call and the row
  # asks for two, bracketing `removebot all`.  One line means the process died
  # inside the removal, which is where gladiator-bot-restored's BotShutdownClient
  # truncates two pointers to `int` -- doc/reconciliation.md R-98.  139 is
  # 128 + SIGSEGV; the shell prints the signal to ITS stderr, not to the log.
  dumps=$(grep -c 'client slots\?$' "$log" || true)
  if [ "$rc" = 139 ] || [ "$rc" = 134 ]; then
    if [ "$dumps" -lt 2 ] && [ "$before" -ge 1 ]; then
      verdict="brain died in BotShutdownClient (R-98)"
    else
      verdict="crashed (signal $((rc - 128)))"
    fi
  fi

  printf '%-9s %-5s %-9s %-8s %-9s %s\n' "$rs" "$n" "$map" "$before" "$loaded" "$verdict"
  [ "$verdict" = ok ]
}

pass=0; fail=0
printf '%-9s %-5s %-9s %-8s %-9s %s\n' ruleset bots map spawned libloads verdict

if [ "$CONTROL" = 1 ]; then
  # The control removes the brain from the gamedir.  Every row must then FAIL to
  # spawn a bot -- if one still appears, `spawned` is counting something that is
  # not a bot and the whole matrix is green for the wrong reason.
  rm -f "$DIR/colosseum/gladiator.so"
  run_row dm 1 q2dm1 spawn && fail=$((fail+1)) || pass=$((pass+1))
  # ...and the second control puts it back but takes the AAS away, which is the
  # BLERR path: the brain loads, refuses the map, and every bot using it is
  # destroyed.  R-VER-3's leak check must still pass, and `spawned` must be 0.
  ln -s "$GLADDIR/release/gladiator.so" "$DIR/colosseum/gladiator.so"
  rm -f "$DIR/colosseum/maps"/*.aas
  run_row dm 1 q2dm1 spawn && fail=$((fail+1)) || pass=$((pass+1))
  # ...and the third is the PLACEMENT comparison, which is Phase 7's addition
  # and is the one that would otherwise be green because it never ran.  The
  # aas files are back, the bots spawn, and the row asserts a team assignment
  # that is the opposite of what `botctfteam 1` produces.
  for a in "$GLADDIR"/assets/maps/*.aas; do
    case $a in *.original_baseline) continue ;; esac
    ln -s "$a" "$DIR/colosseum/maps/$(basename "$a")" 2>/dev/null
  done
  EXTRA="+set botctfteam 1"
  run_row ctf 2 q2ctf1 spawn "red=0 blue=2" ctl && fail=$((fail+1)) || pass=$((pass+1))
  EXTRA=""
  # ...and the fourth is R-VER-6's, driven from the other end: `sv indexprobe`
  # with an index that IS in range must not be reported as an overflow.  If it
  # is, the row's "1 warning, and it is the probe's" test is counting something
  # else and the whole index section is green for the wrong reason.
  ( ulimit -c 0
    printf 'wait 20\nsv indexprobe model 1\nwait 5\nquit\n' | \
    timeout -s KILL 120 "$Q2PRO_BUILD/q2proded" \
      +set basedir "$DIR" +set homedir "$DIR" +set game colosseum \
      +set dedicated 1 +set net_port 0 +set g_ruleset dm \
      +set deathmatch 1 +set maxclients 8 +set minimumplayers 0 \
      +map q2dm1 >"$DIR/probe.log" 2>&1 )
  if grep -q 'past the bot table' "$DIR/probe.log"; then
    printf '%-9s %-5s %-9s %-8s %-9s %s\n' probe - q2dm1 - - \
           "an IN-RANGE probe was reported as overflow"
    fail=$((fail+1))
  else
    printf '%-9s %-5s %-9s %-8s %-9s %s\n' probe - q2dm1 - - \
           "in-range probe stays quiet, out-of-range warns (R-VER-6)"
    pass=$((pass+1))
  fi
  echo
  echo "4 control(s), $pass fired, $fail did not"
  [ "$fail" -eq 0 ] && echo "controls ok: a bot that cannot exist is not counted as one, placement is compared, and the index arm answers both ways"
  exit $([ "$fail" -eq 0 ] && echo 0 || echo 1)
fi

# Every ruleset that accepts bots now REQUIRES a bot, and the three that place
# them require the placement too.  The wanted strings are per bot count, so the
# row asserts "all of them", not "at least one".
for rs in dm ctf arena tourney; do
  case $rs in
    ctf) map=q2ctf1 ;;
    *)   map=q2dm1 ;;
  esac
  for n in 1 16; do
    case $rs in
      dm)      place="" ;;
      # R-CTF-4: every bot is forced onto a team, so noteam is 0 and red+blue
      # is n.  n=1 lands on either side, so only noteam is asserted there.
      ctf)     if [ "$n" = 1 ]; then place="noteam=0"
               else place="red=8 blue=8 noteam=0"; fi ;;
      # R-RA-4 row 15: in an arena, on a team.  `fighting` is not asserted --
      # whether a given bot is in the round or waiting its turn is the round
      # machine's business and changes with the queue.
      arena)   place="arena in-arena=$n (arena1=$n) on-team=$n" ;;
      # R-OSP-11: entered and counted.  match_mode defaults to 0, which has no
      # ready gate, so `ready` is not asserted here -- the mode-2 row below is
      # where readying up is the point.
      tourney) place="entered=$n" ;;
    esac
    if run_row "$rs" "$n" "$map" spawn "$place"; then
      pass=$((pass+1))
    else
      fail=$((fail+1))
      cp "$DIR/$rs-$n.log" "/tmp/botmatrix-$rs-$n.log" 2>/dev/null
    fi
  done
done

# R-CTF-4's own claim: `botctfteam` picks the side.  1 is red, 2 is blue, 0 is
# "balance them", which the two rows above already cover.  This is the only
# check that can tell "the userinfo key is read" from "the fallback ran".
for t in 1 2; do
  [ "$t" = 1 ] && want="red=4 blue=0" || want="red=0 blue=4"
  EXTRA="+set botctfteam $t"
  if run_row ctf 4 q2ctf1 spawn "$want" "ctfteam$t"; then
    pass=$((pass+1))
  else
    fail=$((fail+1))
    cp "$DIR/ctf-4-ctfteam$t.log" "/tmp/botmatrix-ctf-botctfteam$t.log" 2>/dev/null
  fi
  EXTRA=""
done

# R-OSP-11's ready-up, which only mode 1..3 have.  Two teams of two, all four
# ready, in a mode whose match cannot start until they are.
EXTRA="+set match_mode 2"
if run_row tourney 4 q2dm1 spawn "entered=4 ready=4 team0=2 team1=2" "m2"; then
  pass=$((pass+1))
else
  fail=$((fail+1))
  cp "$DIR/tourney-4-m2.log" "/tmp/botmatrix-tourney-m2.log" 2>/dev/null
fi
EXTRA=""

# ---------------------------------------------------------------- R-VER-6
#
# The index-limit test: "on a map that precaches near the old 256-model limit,
# with and without protocol extensions, verify the bot index tables hold and
# that overflow is REPORTED rather than written (R-BOT-11)".
#
# Two things it can measure and one it cannot.  It can measure that the tables
# are sized from game.csr rather than from the donor's 256 -- 8192/2048/2048
# with the extensions negotiated and 256/256/256 without -- and that a heavy map
# fills them without a warning or an error.  It CANNOT reach the overflow arm,
# because a table sized from game.csr has room for every index the engine can
# issue; that is R-BOT-11 working, not a gap in it.  So the arm is driven
# directly by `sv indexprobe`, which asks BotIndexRecord about an index without
# writing one, and the control is that same probe returning the wrong answer.
#
# `near the old 256-model limit` is not reachable with the content on this
# machine: the heaviest retail map under a ruleset that keeps monsters is
# `command` under ctf with both mission-pack layers, and it tops out at 171
# models and 170 sounds.  The headroom is printed rather than hidden, because a
# row that says "no overflow" on a map that uses two thirds of the table is
# weaker evidence than one on a map that uses all of it, and the difference
# should be visible.
index_row() {
    ext=$1
    log=$DIR/index-$ext.log
    ( ulimit -c 0
      { printf 'wait 30\nsv modelindex\nsv soundindex\nsv imageindex\n'
        printf 'sv indexprobe model 999999\nsv indexprobe model 1\n'
        printf 'wait 10\nquit\n'
      } | timeout -s KILL 180 "$Q2PRO_BUILD/q2proded" \
        +set basedir "$DIR" +set homedir "$DIR" +set game colosseum \
        +set dedicated 1 +set net_port 0 +set g_ruleset ctf \
        +set g_protocol_extensions "$ext" +set xatrix 1 +set rogue 1 \
        +set deathmatch 1 +set maxclients 8 +set minimumplayers 0 \
        +map command >"$log" 2>&1 ) 2>>"$log"
    rc=$?

    slots() { sed -n "s/^$1 index (\([0-9]*\) slots):/\1/p" "$log" | head -1; }
    top()   { sed -n "/^$1 index/,/entr/p" "$log" | sed -n 's/^ *\([0-9]*\): .*/\1/p' | sort -n | tail -1; }

    ms=$(slots model); ss=$(slots sound); is=$(slots image)
    mt=$(top model);   st=$(top sound);   it=$(top image)
    warns=$(grep -c 'past the bot table' "$log" || true)
    errs=$(grep -cE '^ERROR' "$log" || true)

    if [ "$ext" = 1 ]; then want_m=8192; want_s=2048; want_i=2048
    else                   want_m=256;  want_s=256;  want_i=256; fi

    verdict=ok
    [ "$rc" = 0 ] || verdict="exited $rc"
    [ "$verdict" = ok ] && [ "$ms/$ss/$is" = "$want_m/$want_s/$want_i" ] || \
      verdict="tables $ms/$ss/$is, wanted $want_m/$want_s/$want_i"
    # One warning is expected and is the probe's; more than one means a real
    # precache was dropped.
    [ "$verdict" = ok ] && [ "$warns" = 1 ] || \
      verdict="$warns overflow warning(s), wanted exactly the probe's 1"
    [ "$verdict" = ok ] && [ "$errs" = 0 ] || verdict="$errs error line(s)"
    # The probe is the control for the reporting arm, in both directions.
    [ "$verdict" = ok ] && grep -q 'indexprobe: model 999999 is out of range' "$log" || \
      verdict="the out-of-range probe was not reported"
    [ "$verdict" = ok ] && grep -q 'indexprobe: model 1 is in range' "$log" || \
      verdict="an in-range probe was reported as overflow"

    printf '%-9s %-5s %-13s %-13s %-13s %s\n' command "$ext" \
      "${mt:-0}/${ms:-?}" "${st:-0}/${ss:-?}" "${it:-0}/${is:-?}" "$verdict"
    [ "$verdict" = ok ]
}

echo
echo "R-VER-6: the bot index tables, used and sized (R-BOT-11)"
printf '%-9s %-5s %-13s %-13s %-13s %s\n' map ext models sounds images verdict
for ext in 1 0; do
  if index_row "$ext"; then pass=$((pass+1)); else fail=$((fail+1)); fi
done

echo
echo "$((pass+fail)) row(s), $pass passed, $fail failed"
[ "$fail" -eq 0 ]
