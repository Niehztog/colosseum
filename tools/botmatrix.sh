#!/bin/sh
# botmatrix.sh -- the bot matrix, run rather than remembered.
#
# For every ruleset that accepts bots, spawn 1 bot and then 16, let them play,
# remove them, and check that nothing leaked: `sv clientdump` must show every
# slot free again and `sv botlibdump` must show the library released.  That is
# The whole requirement, and the two dumps exist so that it can be checked
# from outside rather than asserted from inside.
#
# WHAT IT NEEDS, and why it is more than the other two scripts.  bootmatrix.sh
# and smoke.sh need a game library and id's paks.  A bot needs a BRAIN as well:
#
#   * `gladiator.so`, built from gladiator-bot-restored/botlib.  The
#     bitness must match the game's; the loader says so when it does not.
#   * `bots.cfg` and the character files it names, which are in the Gladiator
#     assets' pak7.pak.  The botlib reads them through its OWN file search --
#     <basedir>/<gamedir>/, then <cddir>/<gamedir>/ -- not through the engine's,
#     which is why they are copied into the gamedir rather than symlinked from
#     wherever the engine happens to look.
#   * an `.aas` file per map, in <gamedir>/maps/.  Without one the brain reports
#     BLERR and BotLib_BotLoadMap destroys every bot using that library -- which
#     is a legitimate row to run and is what --control uses, because "the bot
#     went away cleanly" is exactly what this measures.
#
# The first criterion is "bots load, spawn, navigate, fight and chat in DM";
# ctf, arena and the OSP four come after it.  All are run here anyway, because
# the leak half is not ruleset-specific and a slot leaked under `tdm` is a slot
# leaked.  A row that cannot spawn a bot at all under a later
# ruleset is reported, not failed -- see WANT below.
#
# ENV, all defaulted:
#   Q2PRO_BUILD   ../q2pro/builddir-native
#   Q2DATA        /usr/share/games/quake2/baseq2
#   CTFDATA       /usr/share/games/quake2/ctf
#   GLADDIR       ../gladiator-bot-restored
#   LIB           release/game<cpu>.so
#   FRAMES        600 (one minute).  Five minutes is FRAMES=3000.
set -u

CONTROL=0
[ "${1:-}" = "--control" ] && CONTROL=1

ROOT=$(cd "$(dirname "$0")/.." && pwd)
Q2PRO_BUILD=${Q2PRO_BUILD:-$ROOT/../q2pro/builddir-native}
Q2DATA=${Q2DATA:-/usr/share/games/quake2/baseq2}
CTFDATA=${CTFDATA:-/usr/share/games/quake2/ctf}
# GLADDIR -- the brain, its assets and bspc.  `vendor/gladiator-bot-restored`
# is the submodule and the documented place; a sibling checkout beside the
# repository still works, because that is where it lived before the submodule
# existed.  An explicit GLADDIR wins over both, and a directory holding a BUILT
# brain is preferred over one that does not, so neither layout stops working.
if [ -z "${GLADDIR:-}" ]; then
  for _g in "$ROOT/vendor/gladiator-bot-restored" \
            "$ROOT/../gladiator-bot-restored"; do
    [ -f "$_g/release/gladiator.so" ] && GLADDIR=$_g && break
  done
  GLADDIR=${GLADDIR:-$ROOT/vendor/gladiator-bot-restored}
fi
CPU=$(uname -m | sed -e 's/^aarch64$/arm64/' -e 's/^i.86$/i386/')
LIB=${LIB:-$ROOT/release/game$CPU.so}
FRAMES=${FRAMES:-600}

die() { echo "botmatrix.sh: $*" >&2; exit 2; }
[ -x "$Q2PRO_BUILD/q2proded" ] || die "no q2proded in $Q2PRO_BUILD"
[ -f "$LIB" ] || die "no game library at $LIB -- make native first"
[ -d "$Q2DATA" ] || die "no baseq2 paks at $Q2DATA"
[ -f "$GLADDIR/release/gladiator.so" ] || \
  die "no brain at $GLADDIR/release/gladiator.so -- build gladiator-bot-restored"

# Both of these are read AFTER the chdir below, so they have to survive it, and
# a caller is as entitled to pass them relative to where it stood as $ROOT was
# to be derived that way.  Absolutised here rather than at the point of use so
# that the two `die`s above still quote what the caller actually wrote.
Q2PRO_BUILD=$(cd "$Q2PRO_BUILD" && pwd) || die "cannot enter $Q2PRO_BUILD"
GLADDIR=$(cd "$GLADDIR" && pwd)         || die "cannot enter $GLADDIR"

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

# EVERY SERVER BELOW IS STARTED FROM THE FIXTURE, and that is the second
# control's business rather than tidiness.  The botlib does its own file I/O
# (R-BOT-8): `<basedir>/<gamedir>/` first, then `<cddir>/<gamedir>/` -- and with
# no cddir set the second arm is a RELATIVE path, resolved against whatever
# working directory the server inherited.  Run from the repository root, which
# is where this script is normally run from, `colosseum/maps/` is the shipped
# gamedir and holds the eight OSP meshes, so the brain finds one no matter what
# this fixture holds.  The log gives it away: that load reads
# `loaded colosseum/maps/q2dm1.aas` where every other load in it is absolute.
#
# What that costs is the control, not the rows.  The second one takes the
# meshes AWAY and must see every bot destroyed; from the repository root it saw
# a bot spawn instead and the script reported `4 control(s), 3 fired, 1 did
# not` -- a control blind in the one directory the check is run from, which is
# exactly what R-VER-9 clause 2 ("a check that has never failed is not
# trusted") exists to catch.  R-VER-3 names this control, so a control that
# cannot fire is that requirement unmet, not a rough edge in a script.
# Chdir'ing into $DIR aims both arms at the fixture, so what the brain
# can see is what this script put there and nothing else.  It is also how a
# real server is started: from the directory holding `baseq2/` and
# `colosseum/`, which is what `basedir` already points at.
cd "$DIR" || die "cannot enter $DIR"

# One row: boot, add `n` bots, let them play, dump, remove, dump again.
#
# The two dumps bracket the removal on purpose.  The first says the bots were
# THERE -- a row that never spawned one would otherwise pass the leak check
# trivially -- and the second says they are gone.
#
# `sv ruleset` between them is the other half, which measures that
# a bot can exist and be taken away, which is silent about whether the bot is
# ON A TEAM, IN AN ARENA or COUNTED BY THE MATCH -- and those are the three
# per-ruleset placements.  Earlier the ctf/arena/OSP rows were run
# with `want=report` and they passed while every bot under those three rulesets
# sat in the audience doing nothing.
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
      +set deathmatch 1 +set maxclients 20 \
      +set minimumplayers 0 +set bots_minplayers 0 +set skill 1 ${EXTRA:-} \
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
  # truncates two pointers to `int`.  139 is
  # 128 + SIGSEGV; the shell prints the signal to ITS stderr, not to the log.
  dumps=$(grep -c 'client slots\?$' "$log" || true)
  if [ "$rc" = 139 ] || [ "$rc" = 134 ]; then
    if [ "$dumps" -lt 2 ] && [ "$before" -ge 1 ]; then
      verdict="brain died in BotShutdownClient"
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
  # destroyed.  The leak check must still pass, and `spawned` must be 0.
  ln -s "$GLADDIR/release/gladiator.so" "$DIR/colosseum/gladiator.so"
  rm -f "$DIR/colosseum/maps"/*.aas
  run_row dm 1 q2dm1 spawn && fail=$((fail+1)) || pass=$((pass+1))
  # ...and the third is the PLACEMENT comparison, a later addition
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
  # ...and the fourth is the index tables', driven from the other end: `sv indexprobe`
  # with an index that IS in range must not be reported as an overflow.  If it
  # is, the row's "1 warning, and it is the probe's" test is counting something
  # else and the whole index section is green for the wrong reason.
  ( ulimit -c 0
    printf 'wait 20\nsv indexprobe model 1\nwait 5\nquit\n' | \
    timeout -s KILL 120 "$Q2PRO_BUILD/q2proded" \
      +set basedir "$DIR" +set homedir "$DIR" +set game colosseum \
      +set dedicated 1 +set net_port 0 +set g_ruleset dm \
      +set deathmatch 1 +set maxclients 8 +set minimumplayers 0 +set bots_minplayers 0 \
      +map q2dm1 >"$DIR/probe.log" 2>&1 )
  if grep -q 'past the bot table' "$DIR/probe.log"; then
    printf '%-9s %-5s %-9s %-8s %-9s %s\n' probe - q2dm1 - - \
           "an IN-RANGE probe was reported as overflow"
    fail=$((fail+1))
  else
    printf '%-9s %-5s %-9s %-8s %-9s %s\n' probe - q2dm1 - - \
           "in-range probe stays quiet, out-of-range warns"
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
for rs in dm dmpro tdm duel ctf arena; do
  case $rs in
    ctf) map=q2ctf1 ;;
    *)   map=q2dm1 ;;
  esac
  for n in 1 16; do
    # A `case` WITH NO WILDCARD leaves `place` holding the previous iteration's
    # value, so a ruleset added to the loop above without an arm here asserts
    # something stale and passes.  The `*)` arm is the check on the check.
    case $rs in
      # Every bot is forced onto a team, so noteam is 0 and red+blue
      # is n.  n=1 lands on either side, so only noteam is asserted there.
      ctf)     if [ "$n" = 1 ]; then place="noteam=0"
               else place="red=8 blue=8 noteam=0"; fi ;;
      # In an arena, on a team.  `fighting` is not asserted --
      # whether a given bot is in the round or waiting its turn is the round
      # machine's business and changes with the queue.
      arena)   place="arena in-arena=$n (arena1=$n) on-team=$n" ;;
      # Entered and counted.  `dm` has no ready gate, so `ready` is
      # not asserted for it -- the `tdm` row below is where readying up is the
      # point.  All four OSP rulesets print the same botplace line.
      #
      # `tdm` and `duel` CANNOT seat sixteen, and that is the check rather than
      # a problem with it: they declare a capacity -- `team_maxplayers`, 4 by
      # default and forced to 1 under `duel` -- and OSP_addTeamMember refuses
      # past it, so the roster stops at twice that.  `dm` and `dmpro` have no
      # teams and take everybody.  Asserting `entered=16` here would be
      # asserting that the capacity is NOT enforced.
      dm|dmpro) place="entered=$n" ;;
      tdm)      place="entered=$([ "$n" -gt 8 ] && echo 8 || echo "$n")" ;;
      duel)     place="entered=$([ "$n" -gt 2 ] && echo 2 || echo "$n")" ;;
      *)       echo "!! botmatrix.sh: no placement expectation for '$rs'" >&2
               exit 2 ;;
    esac
    if run_row "$rs" "$n" "$map" spawn "$place"; then
      pass=$((pass+1))
    else
      fail=$((fail+1))
      cp "$DIR/$rs-$n.log" "/tmp/botmatrix-$rs-$n.log" 2>/dev/null
    fi
  done
done

# The CTF claim: `botctfteam` picks the side.  1 is red, 2 is blue, 0 is
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

# Tourney's ready-up, which `dm` does not have and the other three do.  Two
# teams of two, all four ready, in a ruleset whose match cannot start until they
# are.  This was `+set match_mode 2` on the `tourney` ruleset until spec 1.36.
if run_row tdm 4 q2dm1 spawn "entered=4 ready=4 team0=2 team1=2" "ready"; then
  pass=$((pass+1))
else
  fail=$((fail+1))
  cp "$DIR/tdm-4-ready.log" "/tmp/botmatrix-tdm-ready.log" 2>/dev/null
fi

# --------------------------------------------------------------
#
# The index-limit test: "on a map that precaches near the old 256-model limit,
# with and without protocol extensions, verify the bot index tables hold and
# that overflow is REPORTED rather than written".
#
# Two things it can measure and one it cannot.  It can measure that the tables
# are sized from game.csr rather than from the donor's 256 -- 8192/2048/2048
# with the extensions negotiated and 256/256/256 without -- and that a heavy map
# fills them without a warning or an error.  It CANNOT reach the overflow arm,
# because a table sized from game.csr has room for every index the engine can
# issue; that is the index tables working, not a gap in them.  So the arm is driven
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
        +set deathmatch 1 +set maxclients 8 +set minimumplayers 0 +set bots_minplayers 0 \
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

# --------------------------------------------------------------
#
# "Bot AI cost must be bounded: with 32 bots on a loaded map the bot section of
# G_RunFrame stays under half a 100 ms frame on the reference machine, or the
# shortfall is reported."
#
# A requirement with a number in it needs a measurement from INSIDE the library:
# nothing outside can tell the bot section apart from the rest of the frame.
# `sv botperf` reports it -- frames, mean and worst, in microseconds -- and
# `sv botperf reset` starts a fresh window, so the map load and thirty-two
# connects are not averaged into the steady state this asks about.
#
# The WORST frame is what the row judges, not the mean: a requirement about a
# frame budget is about the frame that misses it.  The mean is printed beside it
# because a single slow frame during the AAS load is a different fact from a
# section that is over budget every frame.
botperf_row() {
    n=$1
    log=$DIR/botperf-$n.log
    ( ulimit -c 0
      { printf 'wait 40\n'
        # `sv addrandom` will not seat a character already in the game, so the
        # shipped eighteen-name roster is its ceiling.  The budget is about
        # THIRTY-TWO, so these are added by name -- the roster's own character
        # under distinct netnames, which is what addrandom calls underneath.
        i=0
        while [ $i -lt "$n" ]; do
          printf 'sv addbot "perf%d" "male/grunt" "bots/trash_c.c" "trash"\nwait 5\n' "$i"
          i=$((i+1))
        done
        printf 'wait 100\nsv botperf reset\nwait 300\nsv botperf\nwait 10\nquit\n'
      } | timeout -s KILL 900 "$Q2PRO_BUILD/q2proded" \
        +set basedir "$DIR" +set homedir "$DIR" +set game colosseum \
        +set dedicated 1 +set net_port 0 +set g_ruleset dm \
        +set deathmatch 1 +set coop 0 +set maxclients 40 +set minimumplayers 0 +set bots_minplayers 0 \
        +set bots 1 +map q2dm1 >"$log" 2>&1 ) 2>>"$log"
    rc=$?

    line=$(grep '^botperf frames' "$log" | tail -1)
    bots=$(printf '%s' "$line" | sed -n 's/.* bots \([0-9]*\) .*/\1/p')
    mean=$(printf '%s' "$line" | sed -n 's/.* mean \([0-9]*\) us.*/\1/p')
    worst=$(printf '%s' "$line" | sed -n 's/.* worst \([0-9]*\) us.*/\1/p')
    budget=$(printf '%s' "$line" | sed -n 's/.* budget \([0-9]*\) us.*/\1/p')
    frames=$(printf '%s' "$line" | sed -n 's/^botperf frames \([0-9]*\) .*/\1/p')

    # A cascade rather than this file's `&&`/`||` chain: that idiom re-fires
    # every later `||` once the verdict is already bad, so the LAST message
    # wins and the row reports the wrong reason.  It cost one confusing run
    # here -- "over the 50000us budget" on a row whose worst frame was 4517us,
    # because the bot count had failed two lines earlier.
    verdict=ok
    if [ "$rc" != 0 ]; then
      verdict="exited $rc"
    elif [ -z "$line" ]; then
      verdict="sv botperf did not answer"
    elif [ "${frames:-0}" -lt 100 ] 2>/dev/null; then
      verdict="only ${frames:-0} measured frame(s)"
    elif [ "${bots:-0}" -lt "$n" ] 2>/dev/null; then
      verdict="only ${bots:-0} of $n bot(s) present"
    elif [ "${worst:-999999}" -ge "${budget:-50000}" ] 2>/dev/null; then
      verdict="worst frame ${worst}us over the ${budget}us budget"
    fi

    printf '%-9s %-5s %-13s %-13s %-13s %s\n' botperf "${bots:-0}" \
      "${frames:-0} frames" "mean ${mean:-?}us" "worst ${worst:-?}us" "$verdict"
    [ "$verdict" = ok ]
}

echo
echo "the bot section of G_RunFrame, measured"
printf '%-9s %-5s %-13s %-13s %-13s %s\n' row bots frames mean worst verdict
if botperf_row 32; then pass=$((pass+1)); else fail=$((fail+1)); fi

echo
echo "the bot index tables, used and sized"
printf '%-9s %-5s %-13s %-13s %-13s %s\n' map ext models sounds images verdict
for ext in 1 0; do
  if index_row "$ext"; then pass=$((pass+1)); else fail=$((fail+1)); fi
done

echo
echo "$((pass+fail)) row(s), $pass passed, $fail failed"
[ "$fail" -eq 0 ]
