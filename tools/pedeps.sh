#!/bin/sh
# pedeps.sh -- the Windows DLLs must depend on nothing but Windows.
#
# WHY.  `-fstack-protector-strong` makes gcc add an implicit `-lssp`, and on
# mingw the link prefers `libssp.dll.a` over `libssp.a` -- so the shipped
# `gamex86.dll` declared **libssp-0.dll** as an import and would not load on any
# machine that did not happen to have a mingw runtime beside it.  A game DLL
# that needs a compiler runtime shipped with it is not shippable.
#
# The answer was not to drop the hardening -- that would be the evasion -- and
# the static archive is in the toolchain already.  This is the check that keeps
# it that way, because the failure is invisible from a Linux host -- the DLL
# links, the build is clean, and nothing here can load it to find out.
#
# WHAT IS ALLOWED.  Windows' own, and only those.  Every name below ships with
# the operating system, so a DLL importing them needs nothing beside it:
#
#   KERNEL32  ADVAPI32  msvcrt  USER32  GDI32  WS2_32  SHELL32  OLE32
#
# ADVAPI32 is on the list because of this fix: libssp.a's `__stack_chk_fail`
# reports through the event log, so linking it in adds that import.  It is a
# system DLL and costs nothing.
#
# USAGE
#   tools/pedeps.sh [dll ...]      default: every PE artifact in the tree
#   tools/pedeps.sh --selftest
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
ALLOWED="kernel32.dll advapi32.dll msvcrt.dll user32.dll gdi32.dll ws2_32.dll shell32.dll ole32.dll"

# imports <dll> -- the DLL names it needs, lowercased, one per line.
#
# The objdump is the one that can READ the file, found by trying rather than
# guessed from the path.  The two game DLLs carry their word size in the
# directory the Makefile hands over (`release-win32/`, `release-win64/`) and a
# guess off that name worked; the bot brain is `gladiator.dll` under ONE path
# for both word sizes, and there the guess is wrong half the time.  A wrong
# objdump answers "file format not recognized" on stderr, prints no import at
# all, and the row passes for want of anything to reject -- which is not a
# check.  So: try each, take the first that yields an import table, and say so
# with the exit status when none does.
imports() {
    for od in x86_64-w64-mingw32-objdump i686-w64-mingw32-objdump objdump; do
        command -v "$od" >/dev/null 2>&1 || continue
        names=$("$od" -p "$1" 2>/dev/null | sed -n 's/.*DLL Name: *//p' | tr 'A-Z' 'a-z')
        [ -n "$names" ] || continue
        printf '%s\n' "$names"
        return 0
    done
    return 1
}

fails=0
checked=0
check_one() {
    dll=$1
    [ -f "$dll" ] || return 0
    checked=$((checked+1))
    label="$(basename "$(dirname "$dll")")/$(basename "$dll")"
    # A PE with no import table is not a DLL that loads.  Read failure and
    # empty answer are one case here on purpose: both mean nothing was checked.
    if ! names=$(imports "$dll"); then
        printf '  [FAIL] %-32s no import table could be read\n' "$label"
        fails=$((fails+1))
        return 0
    fi
    bad=""
    for d in $names; do
        case " $ALLOWED " in
            *" $d "*) ;;
            *) bad="$bad $d" ;;
        esac
    done
    if [ -n "$bad" ]; then
        printf '  [FAIL] %-32s needs%s\n' "$label" "$bad"
        fails=$((fails+1))
    else
        printf '  [ ok ] %-32s %s\n' "$label" "$(printf '%s\n' "$names" | tr '\n' ' ')"
    fi
}

if [ "${1:-}" = "--selftest" ]; then
    # The control: a name that is NOT on the allowlist must be reported.  Driven
    # through the same comparison the real rows use rather than through a
    # mocked-up DLL, because building one on a Linux host to be rejected is more
    # machinery than the question deserves.
    echo "import controls"
    bad=""
    for d in kernel32.dll libssp-0.dll; do
        case " $ALLOWED " in
            *" $d "*) ;;
            *) bad="$bad $d" ;;
        esac
    done
    if [ "$bad" != " libssp-0.dll" ]; then
        echo "  [FAIL] control/allowlist                  got '$bad'"
        exit 1
    fi
    echo "  [ ok ] control/allowlist                  libssp-0.dll rejected, kernel32.dll accepted"

    # The second control, for the arm the first cannot reach: a file no objdump
    # can read must FAIL rather than pass for want of anything to reject.  That
    # is the hole the brain's one-path-for-both-word-sizes opened, so this
    # script itself -- a shell script, which no objdump reads -- stands in for
    # the unreadable PE and no DLL has to be built to be rejected.
    if imports "$0" >/dev/null 2>&1; then
        echo "  [FAIL] control/unreadable                 an unreadable file reported imports"
        exit 1
    fi
    echo "  [ ok ] control/unreadable                 a file with no import table is rejected"
    echo
    echo "2 control(s), 2 fired, 0 did not"
    exit 0
fi

echo "the Windows DLLs import nothing but Windows"
if [ $# -gt 0 ]; then
    for f in "$@"; do check_one "$f"; done
else
    for d in debug-win32 release-win32; do check_one "$ROOT/$d/gamex86.dll"; done
    for d in debug-win64 release-win64; do check_one "$ROOT/$d/gamex86_64.dll"; done
fi

echo
if [ "$checked" = 0 ]; then
    # A NAMED file that is not there is a different failure from an empty tree,
    # and the second message's advice is wrong for it: the bot brain is named
    # on the command line by the release workflow and `make windows` does not
    # build it.
    if [ $# -gt 0 ]; then
        echo "none of the named files exists: $*"
    else
        echo "no PE artifact found -- run 'make windows' first"
    fi
    exit 2
fi
echo "$checked artifact(s), $fails failed"
[ "$fails" -eq 0 ]
