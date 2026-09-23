#!/bin/sh
# q2data.sh <destdir>          fetch, verify and lay out the free test data
# q2data.sh --verify <dir>     check a directory against the manifest, no network
#
# Build a Quake II data tree out of the two archives id made free to download,
# so the server-driven checks can run somewhere that has no retail copy of the
# game -- which is every CI runner.  `.github/workflows/ci.yml` is the caller.
#
# WHAT IT HOLDS, AND WHAT THAT BUYS.  Four paks and two licence texts:
#
#   baseq2/pak1.pak   the 3.20 point release: q2dm1..q2dm8
#   baseq2/pak2.pak   the 3.20 point release
#   ctf/pak0.pak      the 3.20 point release's Threewave CTF: q2ctf1..q2ctf5
#   baseq2/pak0.pak   the 3.14 DEMO's pak0: demo1..demo3
#
# The point release carries every map the multiplayer rulesets are tested on,
# and the demo carries the first campaign levels under the names demo1..demo3 --
# the same level `base1` is in retail pak0, which the demo does not have.  So
# `SPMAP=demo1` is what `sp` runs on here, and with it the boot matrix is 28 rows
# of 28 and `smoke.sh` passes whole, the savegame round trip included -- measured
# 2026-09-23, both checks' positive controls firing on the same tree.  A server
# reads the maps and nothing else: textures, models and sounds are the client's,
# so a tree no client could play on is a tree every server-side check can.
#
# THE ONE COLLISION IS THE RIGHT WAY ROUND.  Of the demo pak0's names, 23 are
# shadowed by pak1/pak2, and the only one a server reads is `default.cfg` --
# where the full version's winning is what is wanted.  The rest are textures.
#
# THE TERMS.  The demo's licence grants "the limited right to distribute, free
# of charge and by electronic means only, the Software" so long as the licence
# goes with it.  The point release's only licence is the full game's Limited
# Use Software License, which grants no right to redistribute it.  Both are
# honoured the same way: nothing this script writes is committed, packaged or
# uploaded as an artifact -- it is fetched into a runner's private cache and read
# there -- and both licence texts sit beside the paks they came with.  That is
# the posture `aas.sh` already takes with OSP's meshes, minus the shipping.
#
# WHAT IS PINNED.  The six members, by SHA-256, in `q2data.sha256` -- not the
# archives, for aas.sh's reason: a mirror may hand back a recompressed copy of
# the same members, and the members are what the checks read.  The paks were
# matched against the MD5s yquake2's installation notes publish before their
# SHA-256s were recorded.
#
# THE MIRROR is deponie.yamagi.org, which yquake2 points its own users at.  The
# Wayback Machine's copies of the same two URLs were byte-identical to it on
# 2026-09-23 and are tried when it does not answer.  A directory that already
# holds the six verified files is left alone, which is what makes the CI's cache
# a cache and keeps the mirror from being asked every run.
set -eu

SELF=$(cd "$(dirname "$0")" && pwd)
MANIFEST=$SELF/q2data.sha256

PR=q2-3.20-x86-full-ctf.exe
DEMO=q2-314-demo-x86.exe
BASE=https://deponie.yamagi.org/quake2/idstuff

# urls <archive> -- where to ask for it, in order.
urls() {
  case $1 in
    "$PR")   ts=20250703105446 ;;
    "$DEMO") ts=20250723125205 ;;
    *)       echo "q2data.sh: no source for $1" >&2; return 1 ;;
  esac
  echo "$BASE/$1"
  echo "https://web.archive.org/web/${ts}id_/$BASE/$1"
}

# The layout: which member of which archive becomes which file.
LAYOUT="baseq2/pak1.pak          $PR    baseq2/pak1.pak
baseq2/pak2.pak          $PR    baseq2/pak2.pak
ctf/pak0.pak             $PR    ctf/pak0.pak
DOCS/q2-3.20-license.txt $PR    DOCS/license.txt
baseq2/pak0.pak          $DEMO  Install/Data/baseq2/pak0.pak
DOCS/q2-demo-license.txt $DEMO  license.txt"

usage() { echo "usage: $0 <destdir> | $0 --verify <dir>" >&2; exit 2; }

sha256() {
  if   command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
  elif command -v shasum    >/dev/null 2>&1; then shasum -a 256 "$1" | cut -d' ' -f1
  else openssl dgst -sha256 "$1" | sed 's/^.*= *//'
  fi
}

manifest() { grep -v '^[[:space:]]*#' "$MANIFEST" | grep -v '^[[:space:]]*$'; }
names()    { manifest | awk '{print $2}' | sort; }

# Both directions, as aas.sh does: every file the manifest names is present and
# byte-exact, and the tree holds nothing else.  The second half keeps a stale
# cache from carrying a file that has since been dropped from the manifest.
verify() {
  dir=$1
  [ -d "$dir" ] || { echo "q2data.sh: no such directory: $dir" >&2; return 1; }
  want_names=$(names)
  have_names=$(cd "$dir" && find . -type f | sed 's|^\./||' | sort)
  if [ "$want_names" != "$have_names" ]; then
    echo "q2data.sh: $dir does not hold exactly the files $MANIFEST names." >&2
    echo "  expected: $(echo "$want_names" | tr '\n' ' ')" >&2
    echo "  found:    $(echo "$have_names" | tr '\n' ' ')" >&2
    return 1
  fi
  rc=0
  for name in $want_names; do
    want=$(manifest | awk -v n="$name" '$2 == n { print $1 }')
    have=$(sha256 "$dir/$name")
    if [ "$want" != "$have" ]; then
      echo "q2data.sh: $dir/$name is not the file $MANIFEST names." >&2
      echo "  expected $want" >&2
      echo "  found    $have" >&2
      rc=1
    fi
  done
  return $rc
}

fetch() {
  archive=$1 dest=$2
  for url in $(urls "$archive"); do
    echo "q2data.sh: fetching $url"
    if curl -fsSL --retry 3 --retry-delay 3 --max-time 600 -o "$dest" "$url"; then
      return 0
    fi
    echo "q2data.sh: no answer from $url" >&2
  done
  echo "q2data.sh: could not fetch $archive from any source." >&2
  return 1
}

install_data() {
  dir=$1
  tmp=${TMPDIR:-/tmp}/colosseum-q2data.$$
  trap 'rm -rf "$tmp"' EXIT INT TERM
  rm -rf "$tmp"; mkdir -p "$tmp"

  for archive in "$PR" "$DEMO"; do
    fetch "$archive" "$tmp/$archive"
  done

  # Named members only, each written straight to its place.  Nothing else in
  # either archive -- the 1998 executables, id's own game DLLs -- is wanted, and
  # a member that is never asked for cannot be written anywhere.
  rm -rf "$dir"; mkdir -p "$dir"
  echo "$LAYOUT" | while read -r out archive member; do
    mkdir -p "$dir/$(dirname "$out")"
    unzip -p "$tmp/$archive" "$member" > "$dir/$out"
  done
}

case ${1:-} in
  --verify) [ $# -eq 2 ] || usage; verify "$2"; exit $? ;;
  ""|-*)    usage ;;
esac

DEST=$1

if verify "$DEST" >/dev/null 2>&1; then
  echo "q2data.sh: $DEST already holds the $(names | wc -l | tr -d ' ') verified files"
  exit 0
fi

install_data "$DEST"
verify "$DEST"
echo "q2data.sh: laid out $(names | wc -l | tr -d ' ') verified files in $DEST"
