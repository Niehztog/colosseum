#!/bin/sh
# pedeps.sh -- the Windows DLLs must depend on nothing but Windows (R-SEC-6a).
#
# WHY.  `-fstack-protector-strong` makes gcc add an implicit `-lssp`, and on
# mingw the link prefers `libssp.dll.a` over `libssp.a` -- so the shipped
# `gamex86.dll` declared **libssp-0.dll** as an import and would not load on any
# machine that did not happen to have a mingw runtime beside it.  A game DLL
# that needs a compiler runtime shipped with it is not shippable.
#
# The answer was not to drop the hardening: R-SEC-6 forbids exactly that, and
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

objdump_for() {
    case $1 in
        *64*) echo x86_64-w64-mingw32-objdump ;;
        *)    echo i686-w64-mingw32-objdump ;;
    esac
}

# imports <dll> -- the DLL names it needs, lowercased, one per line
imports() {
    od=$(objdump_for "$1")
    command -v "$od" >/dev/null || od=objdump
    "$od" -p "$1" 2>/dev/null | sed -n 's/.*DLL Name: *//p' | tr 'A-Z' 'a-z'
}

fails=0
checked=0
check_one() {
    dll=$1
    [ -f "$dll" ] || return 0
    checked=$((checked+1))
    bad=""
    for d in $(imports "$dll"); do
        case " $ALLOWED " in
            *" $d "*) ;;
            *) bad="$bad $d" ;;
        esac
    done
    if [ -n "$bad" ]; then
        printf '  [FAIL] %-32s needs%s\n' "$(basename "$(dirname "$dll")")/$(basename "$dll")" "$bad"
        fails=$((fails+1))
    else
        printf '  [ ok ] %-32s %s\n' "$(basename "$(dirname "$dll")")/$(basename "$dll")" \
               "$(imports "$dll" | tr '\n' ' ')"
    fi
}

if [ "${1:-}" = "--selftest" ]; then
    # The control: a name that is NOT on the allowlist must be reported.  Driven
    # through the same comparison the real rows use rather than through a
    # mocked-up DLL, because building one on a Linux host to be rejected is more
    # machinery than the question deserves.
    echo "R-SEC-6a controls"
    bad=""
    for d in kernel32.dll libssp-0.dll; do
        case " $ALLOWED " in
            *" $d "*) ;;
            *) bad="$bad $d" ;;
        esac
    done
    if [ "$bad" = " libssp-0.dll" ]; then
        echo "  [ ok ] control/allowlist                  libssp-0.dll rejected, kernel32.dll accepted"
        echo
        echo "1 control(s), 1 fired, 0 did not"
        exit 0
    fi
    echo "  [FAIL] control/allowlist                  got '$bad'"
    exit 1
fi

echo "R-SEC-6a: the Windows DLLs import nothing but Windows"
if [ $# -gt 0 ]; then
    for f in "$@"; do check_one "$f"; done
else
    for d in debug-win32 release-win32; do check_one "$ROOT/$d/gamex86.dll"; done
    for d in debug-win64 release-win64; do check_one "$ROOT/$d/gamex86_64.dll"; done
fi

echo
if [ "$checked" = 0 ]; then
    echo "no PE artifact found -- run 'make windows' first"
    exit 2
fi
echo "$checked artifact(s), $fails failed"
[ "$fails" -eq 0 ]
