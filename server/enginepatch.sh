#!/bin/sh
# enginepatch.sh -- the engine patches, checked rather than assumed.
#
# Every patch under server/<engine>/ names in its header the upstream commit it
# applies to:
#
#     Applies to q2pro at commit <sha>.
#
# This checks that claim, because the claim is the whole contract.  A patch
# carried against a moving upstream rots silently: the next bump either fails a
# build far away from here or, worse, applies with fuzz and compiles into
# something nobody reviewed.  `git apply --check` is the same test the build
# does, run where the patch lives.
#
# The check needs a clone holding that commit -- it does not need it CHECKED
# OUT, and does not touch the caller's working tree: each patch is tested in a
# throwaway worktree at its own commit, so two patches naming two commits both
# get an honest answer.
#
# THE ENGINE NAME IN THE HEADER PICKS THE CLONE, because there is more than one
# upstream here now: `server/q2pro/` patches the engine and `server/q2admin/`
# the admin wrapper the engine loads in front of the game.  A name with no clone
# configured is a FAILURE and not a skip -- a patch nobody can check is the
# thing this script exists to prevent, and silently passing it would be worse
# than not running.
#
# ENV, all defaulted:
#   Q2PRO_SRC     ../q2pro     a clone holding the commits q2pro headers name
#   Q2ADMIN_SRC   ../q2admin   the same, for q2admin
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
Q2PRO_SRC=${Q2PRO_SRC:-$ROOT/../q2pro}
Q2ADMIN_SRC=${Q2ADMIN_SRC:-$ROOT/../q2admin}

die() { echo "enginepatch.sh: $*" >&2; exit 2; }

# src_for <engine> -- the clone that engine's commits live in, or empty.
# Resolved to an absolute path here so the worktree calls below can use it.
src_for() {
  case "$1" in
    q2pro)   echo "$Q2PRO_SRC" ;;
    q2admin) echo "$Q2ADMIN_SRC" ;;
    *)       echo "" ;;
  esac
}

# The clone a patch needs is the one its OWN header names, so a tree with only
# q2pro patches in it needs only a q2pro clone.  Checked per engine actually
# present rather than up front.
for e in $(ls -d "$ROOT"/server/*/ 2>/dev/null | xargs -n1 basename); do
  ls "$ROOT/server/$e"/*.patch >/dev/null 2>&1 || continue
  s=$(src_for "$e")
  [ -n "$s" ] || die "server/$e/ holds patches but no clone is configured for '$e'"
  [ -d "$s/.git" ] || die "no $e clone at $s -- set $(echo "$e" | tr 'a-z-' 'A-Z_')_SRC"
done

WORK=$(mktemp -d) || die "mktemp failed"
# `src` is whichever clone the patch being checked belongs to; on an interrupt
# it names the one that owns the worktree left behind.  Both are pruned because
# a prune is free and the alternative is a stale entry in the other clone.
src=""
cleanup() {
  if [ -d "$WORK/tree" ] && [ -n "$src" ]; then
    git -C "$src" worktree remove --force "$WORK/tree" >/dev/null 2>&1
  fi
  for c in "$Q2PRO_SRC" "$Q2ADMIN_SRC"; do
    [ -d "$c/.git" ] && git -C "$c" worktree prune >/dev/null 2>&1
  done
  rm -rf "$WORK"
}
trap cleanup EXIT

pass=0
fail=0

for p in "$ROOT"/server/*/*.patch; do
  [ -e "$p" ] || continue
  name=${p#"$ROOT"/}

  # The commit is read out of the patch, not out of a list kept beside it: a
  # list is a second place to forget.
  sha=$(sed -n 's/^Applies to [a-z0-9]* at commit \([0-9a-f]\{7,40\}\)\.*$/\1/p' "$p" | head -1)
  if [ -z "$sha" ]; then
    echo "$name: FAIL -- header names no commit (\"Applies to <engine> at commit <sha>.\")"
    fail=$((fail+1))
    continue
  fi

  # The engine is read out of the same header line, because a patch under
  # server/q2admin/ that claims to apply to q2pro is a mistake worth catching
  # rather than a directory to trust.
  eng=$(sed -n 's/^Applies to \([a-z0-9-]*\) at commit [0-9a-f]\{7,40\}\.*$/\1/p' "$p" | head -1)
  src=$(src_for "$eng")
  if [ -z "$src" ]; then
    echo "$name: FAIL -- header names engine '$eng', which has no clone configured"
    fail=$((fail+1))
    continue
  fi
  src=$(cd "$src" && pwd) || die "cannot enter $src"

  if ! git -C "$src" cat-file -e "$sha^{commit}" 2>/dev/null; then
    echo "$name: FAIL -- $src has no commit $sha"
    echo "         fetch it:  git -C $src fetch <$eng remote> $sha"
    fail=$((fail+1))
    continue
  fi

  rm -rf "$WORK/tree"
  if ! git -C "$src" worktree add --detach "$WORK/tree" "$sha" >/dev/null 2>&1; then
    echo "$name: FAIL -- cannot create a worktree at $sha"
    fail=$((fail+1))
    continue
  fi

  if git -C "$WORK/tree" apply --check --whitespace=nowarn "$p" 2>"$WORK/err"; then
    echo "$name: ok against $(echo "$sha" | cut -c1-7)"
    pass=$((pass+1))
  else
    echo "$name: FAIL -- does not apply to $sha"
    sed 's/^/         /' "$WORK/err"
    fail=$((fail+1))
  fi

  git -C "$src" worktree remove --force "$WORK/tree" >/dev/null 2>&1
done

echo
echo "$((pass+fail)) patch(es), $pass applied, $fail failed"
[ "$fail" -eq 0 ]
