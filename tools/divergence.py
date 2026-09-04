#!/usr/bin/env python3
"""Regenerate the R-CORE-10 divergence matrix from the replay bundles.

R-CORE-10 states the merge load per shared file, in diff lines, across the five
donors, and declares that the generated copy is authoritative over the table
transcribed into SPECS.md.  R-TOOL-5 records that the tool it names had never
been written.  This is it.

METHOD.  R-PROV-3: `git diff baseq2 port_<donor>` in the bundles of SPECS.md
§3.2 *is* that donor's own feature set, by construction -- the `port_*` branches
are Q2PRO's 188 baseq2 game commits replayed onto each donor's divergence from
id's source, so both sides of the diff sit on the same spine tip and nothing but
the donor's feature is left in it.

That is one uniform measurement for all five donors.  SPECS.md 1.0-1.4 used two:
CTF, Xatrix and Rogue diffed against the q2pro worktree, RA2 and tourney against
the bundles.  The bundle method is the one R-PROV-3 prescribes, it is
reproducible from vendored data alone (R-PROV-1), and it does not depend on the
donor pins that went stale between 1.3 and 1.4 (SPECS.md §3.3).  Where this
tool and the transcribed table disagree, R-CORE-10 says this tool wins.

USAGE
    tools/divergence.py                 # markdown matrix on stdout
    tools/divergence.py --md doc/reconciliation-matrix.md
    tools/divergence.py --csv           # machine-readable
    tools/divergence.py --check         # exit 1 if the cache cannot be built
"""
import argparse
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUNDLES = os.path.join(REPO, 'vendor', 'replay')
CACHE = os.path.join(REPO, 'build', 'replay.git')

# The bundle each ref is taken from, and the donor label it carries.  port_ra2
# appears in two bundles at different tips (SPECS.md §3.2): the OSP bundle
# carries it one commit further, so that is the one used, and the SHA is printed
# so the choice is visible rather than implicit.
DONORS = [
    ('ctf',     'q2pro-mission-pack-replay', 'port_ctf'),
    ('xatrix',  'q2pro-mission-pack-replay', 'port_xatrix'),
    ('rogue',   'q2pro-mission-pack-replay', 'port_rogue'),
    ('ra2',     'osp-q2pro-port-replay',     'port_ra2'),
    ('osp',     'osp-q2pro-port-replay',     'port_osp'),
]
SPINE_BUNDLE = 'q2pro-mission-pack-replay'
SPINE_REF = 'baseq2'

# R-PROV-4: object files and build output committed into port_ra2/port_osp are
# build spill and are excluded from every diff and every count.
SPILL = re.compile(r'(^|/)(debug|release|release-win32|release-win64|'
                   r'debug-win32|debug-win64|obj|\.deps)(/|$)')

# R-CORE-10: "g_ptrs.c is excluded -- it is generated (R-SAVE-2)."
EXCLUDE = {'g_ptrs.c'}


def git(*args, cwd=CACHE, check=True):
    # `-C` enters a bare repository and is rejected when Git is configured with
    # safe.bareRepository=explicit.  Naming it as the Git directory is portable.
    r = subprocess.run(['git', f'--git-dir={cwd}'] + list(args),
                       capture_output=True, text=True)
    if check and r.returncode != 0:
        sys.exit(f'divergence.py: git {" ".join(args)} failed:\n{r.stderr.strip()}')
    return r.stdout


def build_cache():
    """Reconstitute a git repository from the vendored bundles.

    R-TOOL-1's `replay/` working tree was lost; the bundles are the durable
    provenance (R-PROV-6), so the cache is derived rather than stored.  It lands
    under build/ and is never committed (R-BUILD-4).
    """
    fresh = not os.path.isdir(os.path.join(CACHE, 'objects'))
    if fresh:
        os.makedirs(CACHE, exist_ok=True)
        subprocess.run(['git', 'init', '-q', '--bare', CACHE], check=True)
    for name in sorted({b for _, b, _ in DONORS} | {SPINE_BUNDLE}):
        path = os.path.join(BUNDLES, name + '.bundle')
        if not os.path.exists(path):
            sys.exit(f'divergence.py: missing bundle {path} (R-PROV-1)')
        # Namespaced per bundle: the three bundles share branch names, and
        # port_ra2 genuinely differs between two of them.
        git('fetch', '-q', '--force', path,
            f'refs/heads/*:refs/bundles/{name}/*')
    return fresh


def ref(bundle, name):
    return f'refs/bundles/{bundle}/{name}'


def interesting(path):
    """A .c/.h file that counts toward the merge load."""
    if not path.endswith(('.c', '.h')):
        return False
    if SPILL.search(path):                 # R-PROV-4
        return False
    if os.path.basename(path) in EXCLUDE:  # R-CORE-10 excludes the generated table
        return False
    return True


def diffstat(base, head):
    """Classify every changed .c/.h file.

    Returns (modified, added, deleted) where `modified` maps path -> diff lines.
    The distinction is the whole point: only a MODIFIED file is an n-way merge.
    A file the donor DELETED is not merge load at all -- R-CORE-8 says the
    deletion is not replayed, so the work is to ignore it.  A file the donor
    ADDED goes into that donor's subfolder under R-CORE-7 and never collides.

    Counting all three together is what makes a deleted monster look like a
    1,200-line merge: `git diff --numstat` reports a deletion as every line
    removed, so m_soldier.c scores 1,187 against CTF, RA2 and tourney alike --
    identical numbers, because it is the same file being dropped three times.
    """
    lines = {}
    for row in git('diff', '--numstat', base, head).splitlines():
        parts = row.split('\t')
        if len(parts) == 3 and parts[0] != '-':
            lines[parts[2]] = int(parts[0]) + int(parts[1])

    modified, added, deleted = {}, [], []
    for row in git('diff', '--name-status', base, head).splitlines():
        parts = row.split('\t')
        if len(parts) < 2:
            continue
        status, path = parts[0][:1], parts[-1]
        if not interesting(path):
            continue
        if status == 'M':
            modified[path] = lines.get(path, 0)
        elif status == 'A':
            added.append(path)
        elif status == 'D':
            deleted.append(path)
    return modified, added, deleted


def collect():
    spine = ref(SPINE_BUNDLE, SPINE_REF)
    spine_sha = git('rev-parse', '--short', spine).strip()
    data, heads, adds, dels = {}, {}, {}, {}
    for label, bundle, name in DONORS:
        r = ref(bundle, name)
        heads[label] = git('rev-parse', '--short', r).strip()
        data[label], adds[label], dels[label] = diffstat(spine, r)
    return spine_sha, heads, data, adds, dels


def matrix(data):
    labels = [d[0] for d in DONORS]
    files = sorted({f for d in data.values() for f in d},
                   key=lambda f: (-sum(data[l].get(f, 0) for l in labels), f))
    return labels, files


def render_md(spine_sha, heads, data, adds, dels):
    labels, files = matrix(data)
    nway = sum(1 for f in files
               if sum(1 for l in labels if f in data[l]) > 1)
    w = ['# R-CORE-10 divergence matrix',
         '',
         'Generated by `tools/divergence.py`; R-CORE-10 makes this copy',
         'authoritative over the table transcribed into `SPECS.md`.',
         '',
         f'Spine: `{SPINE_REF}` @ `{spine_sha}` (R-PROV-3). Donor tips: '
         + ', '.join(f'`{l}`=`{heads[l]}`' for l in labels) + '.',
         '',
         'Diff lines are added+deleted over `.c`/`.h` files, with build spill',
         '(R-PROV-4) and the generated `g_ptrs.c` (R-CORE-10) excluded.',
         '',
         '## Modified files -- the actual merge load',
         '',
         'Only a file present on **both** sides is an n-way merge. Files a donor',
         'added or deleted are counted separately below, because they are',
         'different kinds of work: an added file goes to that donor\'s subfolder',
         'under R-CORE-7 and collides with nothing, and a deleted file is not',
         'work at all -- R-CORE-8 says the deletion is not replayed.',
         '',
         '| file | ' + ' | '.join(labels) + ' | Σ |',
         '|---|' + '---:|' * (len(labels) + 1)]
    for f in files:
        cells = [str(data[l][f]) if f in data[l] else '—' for l in labels]
        tot = sum(data[l].get(f, 0) for l in labels)
        w.append(f'| `{f}` | ' + ' | '.join(cells) + f' | **{tot}** |')
    tot_row = [str(sum(data[l].values())) for l in labels]
    grand = sum(sum(d.values()) for d in data.values())
    w.append('| **total** | ' + ' | '.join(tot_row) + f' | **{grand}** |')
    w += ['',
          f'{len(files)} files are modified by at least one donor; **{nway}** by',
          f'more than one, and those are the n-way merges R-CORE-10 is about.',
          '',
          '## Added and deleted, per donor',
          '',
          '| donor | modified | added | deleted |',
          '|---|---:|---:|---:|']
    for l in labels:
        w.append(f'| {l} | {len(data[l])} | {len(adds[l])} | {len(dels[l])} |')
    w += ['',
          '### The deletions are R-CORE-8, measured',
          '',
          'R-CORE-8 asserts that three donors dropped the whole monster set and',
          'that importing any of them must not carry the deletion. The deleted',
          'column above is that claim in numbers. Every file listed here is',
          'restored in Colosseum by R-CORE-8, so each is zero merge work and',
          'non-zero vigilance.',
          '']
    for l in labels:
        if not dels[l]:
            continue
        w.append(f'* **{l}** deletes {len(dels[l])}: '
                 + ', '.join(f'`{os.path.basename(p)}`' for p in sorted(dels[l]))
                 + '')
    return '\n'.join(w) + '\n'


def render_csv(data, adds, dels):
    labels, files = matrix(data)
    out = ['kind,file,' + ','.join(labels) + ',total']
    for f in files:
        cells = [str(data[l].get(f, 0)) for l in labels]
        out.append(f'modified,{f},' + ','.join(cells)
                   + f',{sum(data[l].get(f, 0) for l in labels)}')
    for kind, src in (('added', adds), ('deleted', dels)):
        allf = sorted({f for v in src.values() for f in v})
        for f in allf:
            cells = ['1' if f in src[l] else '0' for l in labels]
            out.append(f'{kind},{f},' + ','.join(cells)
                       + f',{sum(1 for l in labels if f in src[l])}')
    return '\n'.join(out) + '\n'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--md', metavar='PATH', help='write markdown to PATH')
    ap.add_argument('--csv', action='store_true')
    ap.add_argument('--check', action='store_true',
                    help='only verify the bundles reconstitute')
    a = ap.parse_args()

    build_cache()
    spine_sha, heads, data, adds, dels = collect()

    if a.check:
        print(f'divergence.py: spine {spine_sha}, '
              + ', '.join(f'{l}@{heads[l]}' for l in heads) + ' — ok')
        return 0
    if a.csv:
        sys.stdout.write(render_csv(data, adds, dels))
        return 0
    text = render_md(spine_sha, heads, data, adds, dels)
    if a.md:
        os.makedirs(os.path.dirname(os.path.abspath(a.md)), exist_ok=True)
        with open(a.md, 'w', encoding='utf-8') as fh:
            fh.write(text)
        print(f'wrote {a.md}')
    else:
        sys.stdout.write(text)
    return 0


if __name__ == '__main__':
    sys.exit(main())
