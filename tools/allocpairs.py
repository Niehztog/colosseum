#!/usr/bin/env python3
"""The allocator-pair contract, mechanised (R-VER-22, doc/reconciliation.md R-55/R-63).

A pointer must be released by the allocator that produced it.  This tree has
two allocator families and they are not interchangeable:

  LIBC  malloc / calloc / realloc / strdup   released by  free()
  TAG   gi.TagMalloc / G_CopyString          released by  gi.TagFree()

`gi.TagFree` walks the engine's tag block list and asserts on a header it did
not write, so crossing the families is undefined behaviour rather than a leak.
R-55 is the live case: a regex pass converted CTFWarp's `free(mlist)` to
`gi.TagFree(mlist)` and left the `strdup` that produced `mlist` alone.  It was
client-reachable -- the first `warp` command -- and no check in the tree could
see it, because every check was about types, slots or clocks.

WHY THIS IS NOT A LIBC BAN.  The obvious check is "no malloc/strdup/free in a
tree whose frees go through gi.TagFree", and it is wrong here: `src/g_main.c`'s
`EndDMLevel` strdups `sv_maplist` and frees it with `free()`, a correct pair,
and it is upstream's code that R-CORE-5 keeps byte-identical.  A ban would fail
the build on correct inherited code and the only way to pass would be to edit
it.  Nor is a file-level co-occurrence check enough: `g_main.c` also calls
`gi.TagMalloc` for `g_edicts`, so "this file uses both families" describes the
one file that is right.  The unit that has to agree is the POINTER, which is
why this is shaped like units.py -- that tool tracks a timer field across the
tree and reports the ones both clocks touch; this one tracks a pointer.

THREE CHECKS

  MIX      one pointer, allocated by one family and freed by the other, in the
           same file.  R-55 exactly.
  SPLIT    one struct MEMBER allocated by BOTH families across the tree.
           Whichever family the free site names, it is wrong for half the
           allocations.  This is the merged-tree case: two donors allocate the
           same member differently and only one free survives the merge.
           Members only, because a member has one identity across the tree and
           a bare local does not -- `s` is a strdup in g_main.c and a
           gi.TagMalloc in g_save.c, two unrelated locals that share a letter.
  UNKNOWN  a free whose operand was never allocated (or aliased to something
           allocated) in that file, IN A FILE THAT USES BOTH FAMILIES.  The
           provenance is off-file and the file has already shown it can get it
           wrong.  Restricted to mixed files on purpose: unrestricted it reports
           every `gi.TagFree` of a member filled in elsewhere, which is most of
           them.

WHAT IT CANNOT SEE.  A pointer that crosses files with no name in common, and
a free reached through a function pointer.  Neither is a reason to skip the
checks that do work -- the same sentence as units.py's, for the same reason.

USAGE
    tools/allocpairs.py [--tree src]
    tools/allocpairs.py --selftest
"""
import argparse
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

LIBC_ALLOC = ('malloc', 'calloc', 'realloc', 'strdup')
LIBC_FREE = ('free',)

# The tagged family.  gi.TagMalloc is the primitive; the wrappers are functions
# in this tree that return one.  WRAPPERS is checked against the tree rather
# than trusted -- see wrapper_rot() -- because a wrapper that stops allocating
# with gi.TagMalloc turns every one of its call sites into a silent MIX.
TAG_ALLOC = ('gi.TagMalloc',)
TAG_WRAPPERS = ('G_CopyString',)
TAG_FREE = ('gi.TagFree',)

ALLOC_RE = re.compile(
    r'(?P<lhs>[A-Za-z_]\w*(?:\s*(?:->|\.)\s*\w+|\s*\[[^\]]*\])*)'
    r'\s*=\s*(?!=)\s*(?P<fn>gi\s*\.\s*TagMalloc|[A-Za-z_]\w*)\s*\(')
FREE_RE = re.compile(r'(?P<fn>gi\s*\.\s*TagFree|free)\s*\(\s*(?P<arg>[^;]*?)\s*\)\s*;')
ALIAS_RE = re.compile(
    r'(?:^|[;{}]|\)\s*)\s*(?:(?:const\s+)?\w[\w \t*]*?\*\s*)?'
    r'(?P<lhs>[A-Za-z_]\w*(?:\s*(?:->|\.)\s*\w+|\s*\[[^\]]*\])*)'
    r'\s*=\s*(?P<rhs>[A-Za-z_]\w*(?:\s*(?:->|\.)\s*\w+|\s*\[[^\]]*\])*)\s*;')


def family(fn):
    fn = fn.replace(' ', '').replace('\t', '')
    if fn in LIBC_ALLOC or fn in LIBC_FREE:
        return 'libc'
    if fn in TAG_ALLOC or fn in TAG_WRAPPERS or fn in TAG_FREE:
        return 'tag'
    return None


def key(expr):
    """Normalise a pointer expression to something comparable across sites.

    `mlist` -> `mlist`; `hnd->entries[i].text` -> `->entries[].text`;
    `ent->message` and `self->message` both -> `->message`, which is what lets
    SPLIT see one member allocated two ways in two files.  The base variable is
    dropped deliberately: it is the member that has one allocator, not the
    local that happens to point at it this time.
    """
    e = re.sub(r'\s+', '', expr)
    e = re.sub(r'\[[^\]]*\]', '[]', e)
    e = re.sub(r'^\(?[A-Za-z_]\w*\)?\*?', lambda m: m.group(0), e)
    m = re.search(r'(->|\.)', e)
    if m:
        return e[m.start():]
    return e


def strip(t):
    """Blank comments and string literals, keeping line numbering."""
    def blank(m):
        return re.sub(r'[^\n]', ' ', m.group(0))
    t = re.sub(r'/\*.*?\*/', blank, t, flags=re.S)
    t = re.sub(r'//[^\n]*', '', t)
    return re.sub(r'"(\\.|[^"\\\n])*"', '""', t)


def sources(tree):
    out = []
    for root, dirs, files in os.walk(tree):
        dirs[:] = [d for d in dirs if not d.startswith(('debug', 'release'))]
        for f in sorted(files):
            if f.endswith(('.c', '.h')):
                out.append(os.path.join(root, f))
    return out


def wrapper_rot(files):
    """Every name in TAG_WRAPPERS must still allocate with gi.TagMalloc.

    A wrapper that quietly became a strdup would reclassify all of its call
    sites and the tool would report nothing at all -- the failure mode where a
    check goes green because it stopped looking.
    """
    bad = []
    for w in TAG_WRAPPERS:
        found = False
        for _, text in files:
            body = re.search(r'^\w[\w \t*]*\b%s\s*\([^)]*\)\s*\{(.*?)\n\}' % re.escape(w),
                             strip(text), re.S | re.M)
            if body:
                found = True
                if 'gi.TagMalloc' not in re.sub(r'\s+', '', body.group(1)) \
                        .replace('gi.TagMalloc', 'gi.TagMalloc'):
                    bad.append('%s no longer allocates with gi.TagMalloc' % w)
        if not found:
            bad.append('%s is in TAG_WRAPPERS but not defined in the tree' % w)
    return bad


def scan(text):
    """-> (allocs, frees, aliases) as {key: [(line, family, fn)]} / {key: key}."""
    allocs, frees, aliases = {}, {}, {}
    for i, line in enumerate(strip(text).split('\n'), 1):
        for m in ALLOC_RE.finditer(line):
            fam = family(m.group('fn'))
            if fam:
                allocs.setdefault(key(m.group('lhs')), []).append(
                    (i, fam, re.sub(r'\s+', '', m.group('fn'))))
        for m in FREE_RE.finditer(line):
            arg = m.group('arg')
            if not re.fullmatch(r'[A-Za-z_]\w*(?:\s*(?:->|\.)\s*\w+|\s*\[[^\]]*\])*', arg):
                continue        # a cast or an expression: not a name we can track
            frees.setdefault(key(arg), []).append(
                (i, family(m.group('fn')), re.sub(r'\s+', '', m.group('fn'))))
        for m in ALIAS_RE.finditer(line):
            lhs, rhs = key(m.group('lhs')), key(m.group('rhs'))
            if lhs != rhs:
                aliases.setdefault(lhs, rhs)
    return allocs, frees, aliases


def resolve(k, allocs, aliases, seen=None):
    """Follow `char *msg = ent->message;` back to the member that was allocated."""
    seen = seen or set()
    while k not in allocs and k in aliases and k not in seen:
        seen.add(k)
        k = aliases[k]
    return k


def run(tree, files):
    out = []
    rot = wrapper_rot(files)
    mix, unknown = [], []
    tree_alloc = {}     # key -> {'libc': n, 'tag': n}

    for path, text in files:
        name = os.path.relpath(path, tree)
        allocs, frees, aliases = scan(text)
        fams_here = {f for v in allocs.values() for _, f, _ in v} | \
                    {f for v in frees.values() for _, f, _ in v}

        for k, sites in allocs.items():
            for _, fam, _ in sites:
                tree_alloc.setdefault(k, {'libc': 0, 'tag': 0})[fam] += 1

        for k, sites in frees.items():
            src = resolve(k, allocs, aliases)
            if src not in allocs:
                if len(fams_here) > 1:
                    for i, fam, fn in sites:
                        unknown.append((name, i, k, fn))
                continue
            afams = {f for _, f, _ in allocs[src]}
            for i, fam, fn in sites:
                if fam not in afams:
                    a = allocs[src][0]
                    mix.append((name, i, k, fn, a[2], a[0]))

    # Members only: see SPLIT in the docstring.  A bare local name has no
    # cross-file identity, so comparing two of them is comparing spellings.
    split = [(k, c) for k, c in sorted(tree_alloc.items())
             if k.startswith(('->', '.')) and c['libc'] and c['tag']]
    members = sum(1 for k in tree_alloc if k.startswith(('->', '.')))

    out.append('allocpairs.py: %d tracked pointer(s), %d of them members; '
               '%d mix, %d split, %d unknown-provenance free(s)'
               % (len(tree_alloc), members, len(mix), len(split), len(unknown)))
    for w in rot:
        out.append('  !! wrapper list has rotted: %s' % w)
    for name, i, k, fn, afn, ai in mix:
        out.append('  !! %s:%d: `%s(%s)` releases a pointer allocated by `%s` '
                   'at line %d -- the families do not match (R-55)'
                   % (name, i, fn, k, afn, ai))
    for k, c in split:
        minority = 'libc' if c['libc'] < c['tag'] else 'tag'
        out.append('  !! `%s` is allocated by BOTH families -- %d libc, %d '
                   'tagged. Whichever family its free names is wrong for the '
                   'other; the %s side is the minority and is what has to move'
                   % (k, c['libc'], c['tag'], minority))
    for name, i, k, fn in unknown:
        out.append('  !! %s:%d: `%s(%s)` frees a pointer this file never '
                   'allocates, and this file uses both allocator families'
                   % (name, i, fn, k))

    if not (rot or mix or split or unknown):
        out.append('  every tracked pointer is released by the family that '
                   'allocated it')
        out.append('  (this cannot see a pointer that crosses files under two '
                   'different names)')
    return out


# R-VER-9 clause 2: a check that has never failed is not trusted.  Control 1 is
# R-55's actual bug, restored.  Controls 2 and 3 are the two failure modes it
# did not have, so that each check has fired at least once.
SELFTESTS = [
    ('mix', 'g_ctf.c',
     ('mlist = G_CopyString(warp_list->string);',
      'mlist = strdup(warp_list->string);'),
     'the families do not match'),
    ('split', 'g_misc.c',
     ('self->message = gi.TagMalloc(CLOCK_MESSAGE_SIZE, TAG_LEVEL);',
      'self->message = malloc(CLOCK_MESSAGE_SIZE);'),
     'is allocated by BOTH families'),
    ('unknown', 'g_main.c',
     ('s = strdup(sv_maplist->string);',
      's2 = strdup(sv_maplist->string);'),
     'frees a pointer this file never allocates'),
]


def selftest(tree):
    bad = 0
    files = [(p, open(p, encoding='utf-8').read()) for p in sources(tree)]
    for name, fname, (orig, rev), expect in SELFTESTS:
        out, hit = [], False
        for path, t in files:
            if os.path.basename(path) == fname and orig in t:
                t = t.replace(orig, rev, 1)
                hit = True
            out.append((path, t))
        if not hit:
            print('  !! control "%s": its own mutation no longer applies -- '
                  'the control has rotted' % name)
            bad += 1
            continue
        text = '\n'.join(run(tree, out))
        if expect in text:
            print('  ok  control "%s" fires: %s' % (name, expect))
        else:
            print('  !! control "%s" did NOT fire; the check is asleep' % name)
            bad += 1
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tree', default=os.path.join(REPO, 'src'))
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    tree = os.path.abspath(a.tree)

    if a.selftest:
        return 1 if selftest(tree) else 0

    lines = run(tree, [(p, open(p, encoding='utf-8').read())
                       for p in sources(tree)])
    for l in lines:
        print(l)
    return 1 if any('!!' in l for l in lines) else 0


if __name__ == '__main__':
    sys.exit(main())
