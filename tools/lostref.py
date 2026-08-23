#!/usr/bin/env python3
"""R-26's sixth rule: a merge may not drop a call to a symbol the donor deleted.

R-VER-24, doc/reconciliation.md R-64.

THE PROBLEM.  Three donors delete the whole monster set (R-CORE-8) -- CTF drops
45 files, RA2 46, tourney 44 -- and Colosseum replays none of those deletions,
because it keeps the campaign.  For CTF that was free: the deletions were whole
FILES, and a file the merge never opens cannot lose anything.  RA2 deletes
monster code from inside the SHARED files as well, and there R-26's five rules
are blind -- where our copy of a file happens to equal the base, rule 2
("base==ours: take theirs") takes the donor's version of the hunk, monster
deletion and all.

AND THE COMPILER CANNOT SEE IT.  The callee still exists: m_move.c is in the
tree, M_walkmove is defined and linked, so nothing is undefined.  The call site
is simply gone.  A barrel stops shifting when you walk into it, a dead monster
stops sliding, the turret driver stops dying like an infantry.  Each one links,
runs, and is wrong -- and none of them is in any boot, census or savegame check.

THE CHECK.  Take every function DEFINED in the files those donors delete -- the
22 monster pairs, m_move.c and g_chase.c.  For each SHARED file, count how many
times the reference tree calls each of those names and how many times we do.  A
count that went DOWN is a call R-CORE-8 says we keep and somebody dropped.

Comments are stripped first.  Without that the check is asleep exactly when it
matters most: a donor that explains why it dropped a call names the function in
the explanation, and the count stays level.  That is not hypothetical -- RA2's
barrel_touch carries a five-line comment naming M_walkmove.

USAGE
    tools/lostref.py [--ref ../q2pro/src/game] [--tree src]
    tools/lostref.py --selftest
"""
import argparse
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The files RA2 and tourney delete and Colosseum keeps.  Named by pattern rather
# than listed, so a monster added by a later phase is covered without an edit.
DELETED = re.compile(r'^(m_.*|g_chase)\.c$')

DEFN = re.compile(r'^(?:static\s+)?[A-Za-z_][\w \t*]*?\b([A-Za-z_]\w*)\s*\([^;]*$', re.M)


def strip(t):
    t = re.sub(r'/\*.*?\*/', '', t, flags=re.S)
    return re.sub(r'//[^\n]*', '', t)


def read(p):
    return open(p, encoding='utf-8').read()


def defined_in_deleted(tree):
    syms = set()
    for f in sorted(os.listdir(tree)):
        if not DELETED.match(f):
            continue
        for m in DEFN.finditer(strip(read(os.path.join(tree, f)))):
            if len(m.group(1)) >= 4:
                syms.add(m.group(1))
    return syms


# g_ptrs.c/.h are genptr.py's output: a table of symbol NAMES, not call sites.
# R-CORE-11b renamed six monsters' dodge functions when it split them into a
# baseq2 and a Ground Zero variant, so the old names are legitimately absent
# there -- and check-ptrs already proves that file matches its generator.
GENERATED = ('g_ptrs.c', 'g_ptrs.h')


def shared_files(ref, tree):
    """Spine files that both trees have and that are NOT themselves deleted."""
    out = []
    for f in sorted(os.listdir(ref)):
        if not f.endswith(('.c', '.h')) or DELETED.match(f) or f in GENERATED:
            continue
        if os.path.exists(os.path.join(tree, f)):
            out.append(f)
    return out


def run(ref, tree, override=None):
    """override: {filename: text} to test against instead of what is on disk."""
    syms = defined_in_deleted(ref)
    out, bad = [], 0
    files = shared_files(ref, tree)
    for f in files:
        a = strip(read(os.path.join(ref, f)))
        b = strip((override or {}).get(f) or read(os.path.join(tree, f)))
        for s in sorted(syms):
            r = re.compile(r'\b%s\b' % re.escape(s))
            na, nb = len(r.findall(a)), len(r.findall(b))
            if nb < na:
                out.append('  !! %s: %s called %d time(s) upstream, %d here -- '
                           'a monster-API call this tree keeps (R-CORE-8) was '
                           'dropped by a merge' % (f, s, na, nb))
                bad += 1
    out.insert(0, 'lostref.py: %d symbol(s) from the deleted set, %d shared '
                  'file(s); %d dropped call site(s)' % (len(syms), len(files), bad))
    if not bad:
        out.append('  every monster-API call the spine makes from a shared file '
                   'is still made here')
    return out


# R-VER-9 clause 2.  Each control is a real deletion this merge actually made
# before the check existed -- see doc/reconciliation.md R-64.
SELFTESTS = [
    ('barrel', 'g_misc.c',
     'M_walkmove(self, vectoyaw(v), 20 * ratio * FRAMETIME);',
     'M_walkmove'),
    ('slide', 'g_phys.c',
     '!M_CheckBottom(ent)', 'M_CheckBottom'),
    ('turret driver', 'g_turret.c',
     'infantry_die(self, inflictor, attacker, damage, point);', 'infantry_die'),
]


def selftest(ref, tree):
    bad = 0
    for name, fname, snippet, sym in SELFTESTS:
        text = read(os.path.join(tree, fname))
        if snippet not in text:
            print('  !! control "%s": its own mutation no longer applies -- '
                  'the control has rotted' % name)
            bad += 1
            continue
        lines = run(ref, tree, {fname: text.replace(snippet, '', 1)})
        if any(sym in l and '!!' in l for l in lines):
            print('  ok  control "%s" fires: %s dropped from %s'
                  % (name, sym, fname))
        else:
            print('  !! control "%s" did NOT fire; the check is asleep' % name)
            bad += 1
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ref', default=os.path.join(REPO, '..', 'q2pro', 'src', 'game'))
    ap.add_argument('--tree', default=os.path.join(REPO, 'src'))
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    ref, tree = os.path.abspath(a.ref), os.path.abspath(a.tree)

    if not os.path.isdir(ref):
        print('lostref.py: no reference tree at %s -- skipped' % ref)
        return 0
    if a.selftest:
        return 1 if selftest(ref, tree) else 0

    lines = run(ref, tree)
    for l in lines:
        print(l)
    return 1 if any('!!' in l for l in lines) else 0


if __name__ == '__main__':
    sys.exit(main())
