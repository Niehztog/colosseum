set -e
SP=/tmp/claude-1000/-mnt-c-Users-<user>-q2-dev-q2pro/<session>/scratchpad
Q2=/mnt/c/Users/<user>/q2-dev/q2pro
R=$SP/replay
rm -rf $R && mkdir -p $R && cd $R
git init -q -b baseq2 .
git config user.name "q2pro replay"; git config user.email "replay@localhost"
git config merge.conflictStyle diff3
git config rerere.enabled true
git config rerere.autoupdate true

# 3.20 and 3.21 baseq2 game sources are identical apart from the GPL header,
# so a single root suffices as the common ancestor for all four games.
python3 $SP/writetree.py $SP/dl/x/quake2-3.21/game $R
git add -A && git commit -q -m "Official Quake II 3.20/3.21 baseq2 game source (normalised)"
git tag C0

while read -r sha path; do
    find . -maxdepth 1 \( -name '*.c' -o -name '*.h' -o -name '*.py' -o -name '*.def' -o -name '*.rc' \) -delete
    git -C $Q2 archive "$sha" "$path" | tar -x --strip-components=2 -C .
    git add -A
    GIT_AUTHOR_DATE=$(git -C $Q2 log -1 --format='%aI' "$sha") \
    GIT_COMMITTER_DATE=$(git -C $Q2 log -1 --format='%aI' "$sha") \
    git commit -q --allow-empty -m "$(git -C $Q2 log -1 --format='%s' "$sha")" -m "q2pro-commit: $sha"
done < $SP/commit_paths.txt
echo "commits: $(git rev-list --count HEAD)"
