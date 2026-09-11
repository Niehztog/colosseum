#!/bin/sh
# aas.sh [destdir]        fetch, verify and install the meshes (default
#                         `colosseum/maps`, which is what the package carries)
# aas.sh --verify <dir>   check a directory against the manifest, no network
#
# Put the eight precomputed navigation meshes for q2dm1..q2dm8 where the bots
# look for them.  Without an `.aas` the botlib loads, refuses the map with
# "no AAS file available" and destroys every bot that wanted it -- which reads
# on the console as no bots at all -- and making one by hand is a `bspc` run
# plus a full load of the map (`README.md`, the Bots section).  For the eight
# retail deathmatch maps nobody has to: OSP Tourney DM 2.0 shipped them
# "ALREADY CONVERTED AND COMPUTED", its own `README.txt` says so in as many
# words, and the files are still on OSP's download host.
#
# WHY THESE EIGHT AND NO OTHERS.  They are the only prebuilt meshes OSP ever
# distributed: every architecture package of tourney 2.5 carries the same eight
# and no other download on that page carries any, and the "additional
# precomputed .aas files at the OSP site" its readme points at went with
# planetquake.com and are in no archive.  Every other map still needs one made,
# by the two steps in `README.md`.
#
# WHAT IS PINNED, AND WHY THE CONTAINER IS NOT.  Each mesh is named and
# checksummed in `aas.sha256`; nothing enters the tree, and nothing is
# packaged, that this script has not hashed against it.  The archive holding
# them is deliberately not pinned -- a mirror may hand back a recompressed copy
# of the same members, and what ships is the eight files, which are what is
# checked.
#
# THE MIRROR is the Wayback Machine's copy of the same URL and is tried only
# when OSP's host does not answer: one 1999 download server is a single point
# of failure a release should not have.  Its copy was verified byte-identical
# to the live one on 2026-09-14.  If both are ever unreachable, a directory
# that already holds the eight verified files is left alone -- so a cached or
# hand-placed copy gets a release out, and cannot be the WRONG copy, because it
# is hashed like any other.
set -eu

SELF=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$SELF")
MANIFEST=$SELF/aas.sha256

ARCHIVE=tourney-2.5-linux-x86-glibc.tar.gz
URLS="http://osp.dget.cc/orangesmoothie/downloads/$ARCHIVE
https://web.archive.org/web/20240712111636id_/http://osp.dget.cc/orangesmoothie/downloads/$ARCHIVE"
# The meshes sit in the mod directory of that archive, not in a `maps/`; moving
# them is the whole of "where they belong" -- the engine looks under `maps/`.
MEMBERDIR=tourney

usage() { echo "usage: $0 [destdir] | $0 --verify <dir>" >&2; exit 2; }

# `sha256sum` is GNU and a macOS runner has `shasum` instead.  One helper,
# either answer, bare hex.
sha256() {
  if   command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -d' ' -f1
  elif command -v shasum    >/dev/null 2>&1; then shasum -a 256 "$1" | cut -d' ' -f1
  else openssl dgst -sha256 "$1" | sed 's/^.*= *//'
  fi
}

manifest() { grep -v '^[[:space:]]*#' "$MANIFEST" | grep -v '^[[:space:]]*$'; }
names()    { manifest | awk '{print $2}' | sort; }

# Verify in BOTH directions, like package.sh's other two checks: every mesh the
# manifest names is present and byte-exact, and the directory holds nothing
# else.  The second half is the half that matters at packaging time, because
# that directory is copied whole and a stray file would ride along unnoticed.
verify() {
  dir=$1
  [ -d "$dir" ] || { echo "aas.sh: no such directory: $dir" >&2; return 1; }
  want_names=$(names)
  have_names=$(ls "$dir" | sort)
  if [ "$want_names" != "$have_names" ]; then
    echo "aas.sh: $dir does not hold exactly the meshes $MANIFEST names." >&2
    echo "  expected: $(echo "$want_names" | tr '\n' ' ')" >&2
    echo "  found:    $(echo "$have_names" | tr '\n' ' ')" >&2
    return 1
  fi
  rc=0
  for name in $want_names; do
    want=$(manifest | awk -v n="$name" '$2 == n { print $1 }')
    have=$(sha256 "$dir/$name")
    if [ "$want" != "$have" ]; then
      echo "aas.sh: $dir/$name is not the file $MANIFEST names." >&2
      echo "  expected $want" >&2
      echo "  found    $have" >&2
      rc=1
    fi
  done
  return $rc
}

fetch() {
  dest=$1
  for url in $URLS; do
    echo "aas.sh: fetching $url"
    if curl -fsSL --retry 3 --retry-delay 3 --max-time 300 -o "$dest" "$url"; then
      return 0
    fi
    echo "aas.sh: no answer from $url" >&2
  done
  echo "aas.sh: could not fetch $ARCHIVE from any source." >&2
  return 1
}

install_meshes() {
  dir=$1
  tmp=${TMPDIR:-/tmp}/colosseum-aas.$$
  trap 'rm -rf "$tmp"' EXIT INT TERM
  rm -rf "$tmp"; mkdir -p "$tmp"

  fetch "$tmp/$ARCHIVE"

  # Named members only.  Nothing else in that 1999 archive is wanted, and a
  # member that is never asked for cannot be written anywhere.
  members=$(names | sed "s|^|$MEMBERDIR/|" | tr '\n' ' ')
  # shellcheck disable=SC2086
  tar xzf "$tmp/$ARCHIVE" -C "$tmp" $members

  mkdir -p "$dir"
  for name in $(names); do cp "$tmp/$MEMBERDIR/$name" "$dir/$name"; done
}

case ${1:-} in
  --verify) [ $# -eq 2 ] || usage; verify "$2"; exit $? ;;
  -*)       usage ;;
esac

DEST=${1:-$ROOT/colosseum/maps}

# Already installed and already right: say so and do nothing.  The workflow
# runs this in every packaging job, and a re-run of a job should not re-fetch.
if verify "$DEST" >/dev/null 2>&1; then
  echo "aas.sh: $DEST already holds the $(names | wc -l | tr -d ' ') meshes"
  exit 0
fi

install_meshes "$DEST"
verify "$DEST"
echo "aas.sh: installed $(names | wc -l | tr -d ' ') navigation meshes in $DEST"
