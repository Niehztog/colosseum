#!/bin/sh
# osprunes.sh -- do the OSP runes actually do anything?
#
# WHY THIS EXISTS BESIDE playtest.sh.  The 81-row battery asks `sv ruleset` and
# `sv slots` and believes them, which is right for a dispatch and wrong for a
# mechanic.  One case separates the two: the slot map said the five
# runes were at 22..26 and it was TELLING THE TRUTH, while the pickup wrote
# statslot_t 22 (another ruleset's stat, unmapped here) and the gameplay code
# read ordinals 28..32.  Nothing that consults the map could see that, and
# nothing did -- for five phases, through fifteen audits, ten build
# configurations, a twenty-row boot matrix and 193 client assertions.
#
# What settles it is the number ON THE WIRE, which is what this scenario reads:
# `sv slots` supplies the slot by NAME and the playerstate supplies its value.
#
# It asserts in both signs, and the negative one is the half no diagnostic could
# have produced:
#
#   give a rune           -> that rune's slot carries it, no other rune's does,
#                            and %r in a say_team names it
#   two powerups, no rune -> every rune slot stays zero and %r says "no runes"
#
# The second is the cross-talk case: ordinals 29 and 30 are the OSP map's OWN second
# powerup timer, so before the fix a player holding an invulnerability read as
# holding the STRENGTH and HASTE runes -- doubled damage and haste fire rate from
# a pent.
#
# ...and it detects the other half without reaching any of that, because a
# pre-fix library cannot survive an OSP ruleset with `runes 1`:
# OSP_spawnRuneAt incremented r_count[-6] rather than r_count[0..4], so
# OSP_checkMinRunes never saw its count rise and the two tail-called each other
# until the edict pool was gone -- `ED_Alloc: no free edicts` at map load.  The
# first row is therefore "did the server survive loading the map".
#
# NOT PART OF `make check`, for the same reason none of the server-driven scripts
# are: it needs a built engine, retail paks, and about two minutes -- most of it
# waiting out match_countdown, because a match ruleset makes nothing pickable until a
# match is actually running (Touch_Item, sync_stat < 4).
#
# REQUIREMENTS -- the same set playtest.sh documents.
#   go                        the harness is Go
#   $HARNESS                  default tools/playtest-harness, in this
#                             repository.  Its dependencies are vendored, so
#                             no network and no other checkout is needed.
#   $Q2PRO_BUILD/q2proded     default ../q2pro/builddir-native
#   $Q2DATA                   retail baseq2 paks
#   $CTFDATA                  Threewave paks (Install links both; unused here)
#
# USAGE
#   tools/osprunes.sh [-l <game.so>] [extra args]
#   tools/osprunes.sh -l release-oldapi/gamex86_64.so     # the other game ABI
set -e

HARNESS=${HARNESS:-$(cd "$(dirname "$0")/playtest-harness" && pwd)}
Q2PRO_BUILD=${Q2PRO_BUILD:-$(dirname "$0")/../../q2pro/builddir-native}
Q2DATA=${Q2DATA:-/usr/share/games/quake2/baseq2}
CTFDATA=${CTFDATA:-/usr/share/games/quake2/ctf}
LIB=release/game$(uname -m | sed -e 's/^x86_64$/x86_64/' -e 's/^aarch64$/arm64/').so

while [ $# -gt 0 ]; do
  case $1 in
    -l) LIB=$2; shift 2 ;;
    *)  break ;;
  esac
done

die() { echo "osprunes.sh: $*" >&2; exit 2; }
[ -d "$HARNESS" ] || die "no harness at $HARNESS (set HARNESS)"
[ -x "$Q2PRO_BUILD/q2proded" ] || die "no q2proded in $Q2PRO_BUILD (set Q2PRO_BUILD)"
[ -f "$LIB" ] || die "no game library at $LIB -- run make first"
[ -d "$Q2DATA" ] || die "no baseq2 paks at $Q2DATA (set Q2DATA)"

LIB=$(cd "$(dirname "$LIB")" && pwd)/$(basename "$LIB")
Q2=$(cd "$Q2PRO_BUILD" && pwd)/q2proded

cd "$HARNESS"
exec go run ./scenarios/osprunes \
  -q2proded "$Q2" -lib "$LIB" -ref "$Q2DATA" -ctf "$CTFDATA" "$@"
