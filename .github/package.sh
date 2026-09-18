#!/bin/sh
# package.sh <version> <os> <arch> <library> <tar|zip>
#
# Build one release package from a tree that has already been built.  The
# library is FOUND rather than named by directory, because each target builds
# into its own `release-*` and the caller should not have to know which.
#
# WHAT GOES IN: the library, the default gamedir config set, and the OPERATOR
# documentation -- `README.md`, the gamedir's own README, and the three
# inventories that answer "what can I set, type and spawn".
#
# THE CONFIG SET IS COPIED WHOLE AND THEN CHECKED, because `cp -r` is silent
# about what it did not find.  `README.md` tells an operator to start a match
# with `+exec configs/<ruleset>.cfg`, so a package missing one of those files
# is a package that cannot start one of its own seven rulesets -- and would
# ship looking exactly like one that could.  The CONFIG CHECK below reads the
# ruleset names out of `src/g_ruleset.c` rather than repeating them here, so an
# eighth ruleset added to the library with no config beside it fails this job.
#
# THE LIBRARIES GO INSIDE `colosseum/`, because that is the directory the
# engine loads one from: `<enginedir>/colosseum/game<cpu><suffix>`, which is
# R-BUILD-8's finding.  The `colosseum/` this script stages IS that gamedir --
# configs, brain, meshes and now the library too -- so installing a release is
# copying one folder into place rather than copying a folder and then being
# told, in a different section of the README, to go back for a file left at the
# top of the archive.
#
# TWO LIBRARIES, ONE PACKAGE.  The tree builds against two game ABIs and the
# artifact NAME is the same for both, because the engine looks for
# `game<cpu><suffix>` and nothing else.  They cannot both be that file, so the
# classic-ABI build goes in `colosseum/oldapi/` under its own correct name: an
# operator on R1Q2, Yamagi or id's 3.20 copies that one over the one a
# directory up, with no renaming.  It rides INSIDE the gamedir rather than
# beside it so the one-folder-copy above stays true for them as well; the
# engine looks for a library in the gamedir itself and never in a
# subdirectory of it, so the spare build sits there inert.  Both are REQUIRED
# -- a package that quietly carried one ABI would be indistinguishable from one
# that carried the other, which is the failure this script exists to prevent.
# Build them with `make <target> && make <target> API=old`, or
# `make bothapis GOAL=<target>`.
#
# AND THE EIGHT NAVIGATION MESHES, which are the one piece of data in a package
# that this project did not write.  A map with no `.aas` beside it is a map the
# botlib refuses -- "no AAS file available", every bot that wanted it destroyed
# -- so a package that carried the bot documentation and no mesh would be one
# whose bots never appear on the eight maps everybody starts with.  OSP Tourney
# DM shipped q2dm1..q2dm8 precomputed in 1999; `.github/aas.sh` fetches and
# checksums them and the MESH CHECK below re-reads the STAGED copies, so a job
# whose fetch step was skipped or half-finished fails here instead of shipping
# a gamedir that looks complete.  `README.md` says where they came from.
#
# AND THE BOT BRAIN, which is the other half of that same argument: eight
# meshes and no brain to walk them is no bots either.  `gladiator.so` is a
# separate build of a separate repository -- the `vendor/gladiator-bot-restored`
# submodule -- and leaving it out asked every operator to fetch a C tree and
# cross-compile it for their own platform, which is the one step the packaging
# job is already standing on the right machine to do: it has just built the
# game library for that exact target.  So every job builds it too, and it
# ships inside `colosseum/`, beside the meshes, under the name the game
# dlopens: `BotDefaultLibrary()` says `gladiator.dll` on Windows and
# `gladiator.so` everywhere else -- including macOS, where the submodule's own
# Makefile emits a `.dylib` and this script renames the copy.  It is GPL like
# the rest of the distribution and its source is the submodule pin, which
# travels with the tag this package was cut from.
#
# THE BRAIN CHECK is the mesh check's argument in another format, and it asks
# two questions of the bytes.  A brain built for the HOST instead of the target
# is a perfectly well-formed shared object that no operator's engine can load,
# and nothing about the package would look wrong -- so the staged copy is read
# back and the fields that name its target compared against the library it will
# sit beside.  A brain built without `GLAD_SERVERFIX=1` looks wrong in even
# fewer ways: it is the submodule's faithful default, the 1999 bugs included,
# and it plays perfectly until a map subdivides densely enough for
# AAS_AASLinkEntity to walk off its `int[64]` and over the return address.  The
# packaging job passes the gate (R-BUILD-11) and the same read-back proves it
# reached the object, because a flag on a command line is not a flag in a binary.
#
# WHAT DOES NOT: game assets of any kind, the brain's own assets (`pak7.pak`
# and the `bots.cfg` bot list are the Gladiator distribution's, R-LIC-2) and a
# mesh for any map but those eight -- made per map, and far larger than
# everything else here put together.
#
# WHAT DOES NOT, AND IS WHY THE DOCUMENTATION IS AN ALLOWLIST RATHER THAN
# `cp -r docs`: the developer documentation.  `SPECS.md`, `DEVELOPMENT.md`,
# `CLAUDE.md` and four of the seven files under `docs/` describe how this tree
# is built, decided and audited -- the specification of record, the donor diff,
# the divergence matrix, the botlib ABI contract and the play-test scenarios.
# An operator running a server needs none of them, and shipping them invites
# the reading that they are the package's contract rather than the
# repository's.  They stay in the repository, which is where they are versioned
# and where their cross-references resolve.
#
# Naming a file here is therefore a decision that it is operator-facing.  The
# STAGE CHECK at the bottom is that decision's check: an allowlist cannot see a
# developer file arriving inside a directory copied wholesale, so the staged
# tree is re-read and every markdown file in it compared against the intended
# set -- in both directions, so a new operator document that nobody added here
# fails just as loudly as a developer one that leaked.
set -eu

[ $# -eq 5 ] || { echo "usage: $0 <version> <os> <arch> <library> <tar|zip>" >&2; exit 2; }
VERSION=$1 OS=$2 ARCH=$3 LIB=$4 FORMAT=$5
ROOT=$(cd "$(dirname "$0")/.." && pwd)
NAME=colosseum-$VERSION-$OS-$ARCH
STAGE=$ROOT/dist/$NAME

# The operator inventories, relative to `docs/`.  The rest of that directory is
# developer documentation; see the header.
DOCS='cvars.md commands.md entities.md'

# Where a reference that does not ship has to keep resolving.  Pinned at the
# release tag, not at a branch, so a package read in two years quotes the
# document that shipped with it.
REPO=https://github.com/Niehztog/colosseum

cd "$ROOT"

# Resolve each ABI's library by the directory it was built into, never by
# find(1) order: `release-oldapi/` and `release/` hold files of the SAME NAME
# and different struct layouts, so picking the wrong one links and loads and is
# wrong at every offset.  `API=old` suffixes the build directory; that suffix
# is the only thing telling the two apart.
find_lib() {
  for d in release release-*; do
    [ -d "$d" ] || continue
    case $d in
      *-oldapi) [ "$1" = old ] || continue ;;
      *)        [ "$1" = new ] || continue ;;
    esac
    if [ -f "$d/$LIB" ]; then echo "$d/$LIB"; return 0; fi
  done
  return 1
}

found=$(find_lib new || true)
[ -n "$found" ] || { echo "package.sh: no $LIB in any release directory -- was the build run?" >&2; exit 1; }

found_old=$(find_lib old || true)
[ -n "$found_old" ] || {
  echo "package.sh: no $LIB in any release-*-oldapi directory." >&2
  echo "  Every package carries both game ABIs; see the header." >&2
  echo "  Build the second one with: make <target> API=old" >&2
  exit 1
}

rm -rf "$STAGE"; mkdir -p "$STAGE/docs"
cp LICENSE "$STAGE/"
for d in $DOCS; do cp "docs/$d" "$STAGE/docs/$d"; done

# The gamedir template first and the libraries into it, in that order: `cp -r`
# copies INTO a destination that already exists, so staging the directory after
# its contents would bury the tree at `colosseum/colosseum`.
cp -r colosseum "$STAGE/colosseum"
mkdir -p "$STAGE/colosseum/oldapi"
cp "$found" "$STAGE/colosseum/"
cp "$found_old" "$STAGE/colosseum/oldapi/"

# `README.md` is the one shipped file that links OUT of the package: to
# `DEVELOPMENT.md` and `SPECS.md`, which are deliberately absent, and to the
# banner under `.github/`, which was never packaged.  In the repository those
# links are relative and correct, so the repository's copy keeps them; the
# PACKAGED copy gets them rewritten to the tag they shipped from, which is the
# difference between a reference an operator can follow and a dead path.
sed -e "s#](\(DEVELOPMENT\.md\|SPECS\.md\)#](${REPO}/blob/${VERSION}/\1#g" \
    -e "s#](\.github/banner\.png)#](${REPO}/raw/${VERSION}/.github/banner.png)#g" \
    README.md > "$STAGE/README.md"

# The roster is the brain's file and is installed from the brain's own
# distribution; the directory ships empty so the layout is visible.
mkdir -p "$STAGE/colosseum/botcfg"

# The brain itself, built for this target by the job that called this script.
# FOUND under the name the submodule's Makefile gives it on this platform and
# SHIPPED under the name the game dlopens; the two differ only on macOS.
# $GLADDIR moves the submodule exactly as it does for tools/botabi.py.
GLADDIR=${GLADDIR:-$ROOT/vendor/gladiator-bot-restored}
case $OS in
  windows) BRAIN_BUILT=gladiator.dll   BRAIN=gladiator.dll ;;
  macos)   BRAIN_BUILT=gladiator.dylib BRAIN=gladiator.so  ;;
  *)       BRAIN_BUILT=gladiator.so    BRAIN=gladiator.so  ;;
esac
[ -f "$GLADDIR/release/$BRAIN_BUILT" ] || {
  echo "package.sh: no $BRAIN_BUILT in $GLADDIR/release -- was the brain built?" >&2
  echo "  Every package carries one, for its own platform; see the header." >&2
  echo "  Build it with: make -C vendor/gladiator-bot-restored botlib GLAD_SERVERFIX=1" >&2
  echo "  That flag is not optional for a package -- see the header, and the" >&2
  echo "  gate check below.  A cross-compiled target needs its own CC= there," >&2
  echo "  as the release workflow passes; a submodule that was never checked" >&2
  echo "  out has no Makefile at all." >&2
  exit 1
}
cp "$GLADDIR/release/$BRAIN_BUILT" "$STAGE/colosseum/$BRAIN"

# Two files of the same name in one package need a note beside them, and it has
# to be here rather than in `docs/`: the operator reading it is standing in the
# directory, deciding which file to copy.  It is in the allowlist below like
# every other shipped markdown file.
cat > "$STAGE/colosseum/oldapi/README.md" <<'EOF'
# The classic game ABI

This directory holds a second build of the same library, against the **classic
id game ABI** (`GAME_API_VERSION` 3) instead of the current one (3302).
Same sources, same version, same file name -- different struct layouts.

The gamedir is the directory **above** this one, and the library already in it
is the one to keep if your engine is Q2PRO built with `USE_NEW_GAME_API`, which
is the default and the recommended engine. Nothing in here is loaded from here:
the engine looks for its library in the gamedir itself.

Use **this** one if your engine is R1Q2, Yamagi Quake II, id's own 3.20, or a
Q2PRO built without that switch. Copy it over the one above, under whatever
name your engine looks for: Q2PRO and R1Q2 want this file's own name, Yamagi
wants a flat `game.so`. This directory can then be deleted. The package's
`README.md` has the table.

Two things follow from the choice and neither is a defect:

* under `ctf` the classic ABI has no room for the second powerup timer --
  Threewave already uses 0..30 of its 32 stat slots -- so that display is
  dropped, exactly as upstream does;
* **savegames do not cross the two.** One written by either build is refused by
  the other rather than misread.

`sv slots` prints which ABI the running library was built against.
EOF

# The config check.  One `configs/<ruleset>.cfg` per ruleset the library
# accepts, and the ruleset names come from the library's own list so this
# cannot drift from it -- `RULESET_NAME_LIST` is the single place the seven are
# spelled for a human, and `g_ruleset` warns and falls back to `dm` for
# anything not in it.  Compared in BOTH directions, like the markdown check: a
# config for a ruleset that no longer exists is as wrong as a missing one.
rulesets=$(sed -n 's/^#define[[:space:]]*RULESET_NAME_LIST[[:space:]]*"\([^"]*\)".*/\1/p' src/g_ruleset.c)
[ -n "$rulesets" ] || {
  echo "package.sh: no RULESET_NAME_LIST in src/g_ruleset.c -- has it been renamed?" >&2
  exit 1
}
expected_cfg=$(for r in $rulesets; do echo "$r.cfg"; done | sort)
actual_cfg=$(ls "$STAGE/colosseum/configs" 2>/dev/null | sort)
if [ "$expected_cfg" != "$actual_cfg" ]; then
  echo "package.sh: colosseum/configs/ is not one config per ruleset." >&2
  echo "  README.md starts a match with '+exec configs/<ruleset>.cfg'; see the header." >&2
  printf '%s\n' "$expected_cfg" > "$ROOT/dist/.cfg-expected"
  printf '%s\n' "$actual_cfg"   > "$ROOT/dist/.cfg-actual"
  diff -u "$ROOT/dist/.cfg-expected" "$ROOT/dist/.cfg-actual" >&2 || true
  rm -f "$ROOT/dist/.cfg-expected" "$ROOT/dist/.cfg-actual"
  exit 1
fi

# The mesh check.  `.github/aas.sh` wrote these and hashed them on the way in;
# this re-reads the STAGED copies through the same script, which is the same
# discipline as the two checks either side of it and for the same reason --
# `cp -r colosseum` copies a directory whole and cannot say what was in it.
if ! "$ROOT/.github/aas.sh" --verify "$STAGE/colosseum/maps"; then
  echo "package.sh: colosseum/maps/ is not the mesh set .github/aas.sha256 names." >&2
  echo "  Run .github/aas.sh before packaging; see the header." >&2
  exit 1
fi

# The brain check.  See the header: the question has to be asked of the bytes,
# because every wrong answer is still a valid shared object.  The two files are
# object code for one target, so their headers agree in the fields that name it
# -- ELF's magic, class and machine at 0..4 and 18..19, Mach-O's magic and
# cputype in the first eight bytes.  Windows is answered by the NAME instead:
# the submodule emits `gladiator.dll` only under a mingw CC, and the packaging
# job runs tools/pedeps.sh over it for what it imports.
hdr() { od -An -tx1 -j"$2" -N"$3" "$1" | tr -d ' \n'; }
case $OS in
  linux) want="$(hdr "$STAGE/colosseum/$LIB" 0 5)$(hdr "$STAGE/colosseum/$LIB" 18 2)"
         got="$(hdr "$STAGE/colosseum/$BRAIN" 0 5)$(hdr "$STAGE/colosseum/$BRAIN" 18 2)" ;;
  macos) want=$(hdr "$STAGE/colosseum/$LIB" 0 8)
         got=$(hdr "$STAGE/colosseum/$BRAIN" 0 8) ;;
  *)     want= got= ;;
esac
# Two empty answers compare equal, which would be this check passing because it
# could not run.  The library is object code and always has a header; if it
# reads as nothing, od(1) is what is wrong and the comparison below means
# nothing either.
if [ "$OS" != windows ] && [ -z "$want" ]; then
  echo "package.sh: read no header from $LIB -- is od(1) missing?" >&2
  echo "  The brain check cannot run, so this job stops here; see the header." >&2
  exit 1
fi
if [ "$want" != "$got" ]; then
  echo "package.sh: colosseum/$BRAIN is not built for the same target as $LIB." >&2
  echo "  library header $want, brain header $got." >&2
  echo "  A brain built for the host loads on nobody's server; see the header." >&2
  exit 1
fi

# The second question, and this one has the same answer on all three platforms.
# `AAS_LinkEntity: stack overflow` is the PRT_ERROR text of the overflow guard
# Quake III added, so the preprocessor emits it on the gated arm and nowhere
# else: no gate, no string.  `server/Dockerfile` checks its own build the same
# way, and strings(1) reads a PE and a Mach-O as willingly as an ELF.
if ! command -v strings >/dev/null 2>&1; then
  echo "package.sh: strings(1) is missing -- the gate check cannot run." >&2
  echo "  This job stops here rather than ship an unverified brain." >&2
  exit 1
fi
if ! strings -a "$STAGE/colosseum/$BRAIN" | grep -q 'AAS_LinkEntity: stack overflow'; then
  echo "package.sh: colosseum/$BRAIN carries no GLAD_SERVERFIX code." >&2
  echo "  It is the faithful 1999 reconstruction, fatal bugs and all; see the header." >&2
  echo "  Build it with: make -C vendor/gladiator-bot-restored botlib GLAD_SERVERFIX=1" >&2
  exit 1
fi

# The stage check.  See the header: this is the part that holds when someone
# adds a file to a directory this script copies whole.
expected=$({ echo README.md; echo colosseum/README.md; echo colosseum/oldapi/README.md
             for d in $DOCS; do echo "docs/$d"; done; } | sort)
actual=$(cd "$STAGE" && find . -name '*.md' | sed 's#^\./##' | sort)
if [ "$expected" != "$actual" ]; then
  echo "package.sh: the staged markdown is not the intended set." >&2
  echo "  Only operator documentation ships; see the header of this script." >&2
  printf '%s\n' "$expected" > "$ROOT/dist/.md-expected"
  printf '%s\n' "$actual"   > "$ROOT/dist/.md-actual"
  diff -u "$ROOT/dist/.md-expected" "$ROOT/dist/.md-actual" >&2 || true
  rm -f "$ROOT/dist/.md-expected" "$ROOT/dist/.md-actual"
  exit 1
fi

cd "$ROOT/dist"
case $FORMAT in
  tar) tar czf "$NAME.tar.gz" "$NAME" ;;
  zip) zip -qr "$NAME.zip" "$NAME" ;;
  *)   echo "package.sh: unknown format $FORMAT" >&2; exit 2 ;;
esac
rm -rf "$NAME"
ls -l "$ROOT/dist"
