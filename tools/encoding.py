#!/usr/bin/env python3
"""Every source file is valid UTF-8.

Trivial, and it has bitten this project twice: a merge left `g_ai.c` and
`g_combat.c` holding a latin-1 `\xa7` where a `§` was meant, and the import in
1.14 did the same to `g_misc.c` and `g_ctf.c` -- both times because an edit was
written through a latin-1 encoder while containing a non-ASCII character.

The cost is not cosmetic.  Every tool in tools/ reads with encoding='latin-1'
precisely to survive it, but `grep` in a UTF-8 locale silently produces NO
OUTPUT for a file it cannot decode -- which is how a string-op census briefly
reported that g_ctf.c contained zero sprintf calls.  A check that returns
"nothing found" for the wrong reason is worse than one that fails.

USAGE
    tools/encoding.py [--tree src]
"""
import argparse
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tree', default=os.path.join(REPO, 'src'))
    a = ap.parse_args()

    bad = []
    n = 0
    for root, dirs, files in os.walk(os.path.abspath(a.tree)):
        dirs[:] = [d for d in dirs if not d.startswith(('debug', 'release'))]
        for f in sorted(files):
            if not f.endswith(('.c', '.h')):
                continue
            n += 1
            path = os.path.join(root, f)
            raw = open(path, 'rb').read()
            try:
                raw.decode('utf-8')
            except UnicodeDecodeError as e:
                ctx = raw[max(0, e.start - 30):e.start + 10]
                bad.append((path, e.start, ctx.decode('latin-1')))

    print(f'encoding.py: {n} source file(s), {len(bad)} not valid UTF-8')
    for path, off, ctx in bad:
        print(f'  !! {os.path.basename(path)}: byte {off} is not UTF-8 -- '
              f'...{ctx.strip()}')
    if not bad:
        print('  every source file decodes as UTF-8, so grep sees all of them')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
