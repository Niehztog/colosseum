#!/usr/bin/env python3
"""Count the acceptance-criteria surfaces: classnames, cvars, client commands.

R-TOOL-2: "Every number this spec publishes as an acceptance criterion --
classname, cvar and command counts [...] -- is produced by a script in `tools/`
that ships with it.  A figure with no script is indicative only (§3) and cannot
gate a phase."  R-TOOL-5 records that this script had never been written, which
is why R-BASE-1/2/3, R-MP-1, R-OSP-2 and R-RA-2 were unenforceable as written.

This is that script.  It does not attempt to reproduce the numbers in SPECS.md;
it measures the tree and prints what is there, so that a claim and a measurement
can be compared instead of a claim being compared with another claim.

WHAT IS COUNTED, and why each definition is the arguable part

  classnames  A classname is anything ED_CallSpawn will match.  That is two
              tables, not one: `spawn_funcs[]` in g_spawn.c, AND every
              `itemlist[]` entry with a non-NULL `.classname` -- g_spawn.c
              checks the item list *first* (ED_CallSpawn's item loop precedes
              its spawn_funcs loop).  Counting only spawn_funcs undercounts by
              the whole pickup set.

  cvars       Unique first argument of any `gi.cvar*()` call.  This is the count
              §3's "Counting method" paragraph calls naive, and it is: it cannot
              see a cvar registered through a wrapper, and it counts a cvar
              registered twice as one -- which is exactly the R-COMPAT-6 defect
              (`statsfile` registered by both RA2 and tourney), so the
              --duplicates mode reports those separately rather than hiding
              them in a unique count.

  commands    Every literal the client-command dispatcher compares against.
              baseq2 dispatches with a chain of `Q_stricmp(cmd, "...")` in
              ClientCommand; the donors add table-driven dispatch, so both
              shapes are matched.

USAGE
    tools/counts.py                       # this tree
    tools/counts.py --tree PATH [...]     # any donor tree, for comparison
    tools/counts.py --list classnames     # print the names, not just the count
    tools/counts.py --duplicates          # cvars registered more than once
"""
import argparse
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def sources(tree):
    out = []
    for root, dirs, files in os.walk(tree):
        dirs[:] = [d for d in dirs
                   if d not in ('debug', 'release', 'obj', '.deps')
                   and not d.startswith(('debug-', 'release-'))]
        for f in sorted(files):
            if f.endswith('.c'):
                out.append(os.path.join(root, f))
    return out


def read(p):
    return open(p, encoding='latin-1').read()


def strip_comments(t):
    t = re.sub(r'/\*.*?\*/', '', t, flags=re.S)
    return re.sub(r'//[^\n]*', '', t)


def table_rows(text, table):
    """String literals in the first column of a `<type> <table>[] = { ... }`."""
    m = re.search(r'\b%s\s*\[\s*\]\s*=\s*\{' % re.escape(table), text)
    if not m:
        return []
    i, depth = m.end(), 1
    while depth and i < len(text):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
        i += 1
    body = text[m.end():i - 1]
    return re.findall(r'\{\s*"([^"]+)"', body)


def classnames(files):
    spawn, items = [], []
    for f in files:
        t = strip_comments(read(f))
        spawn += table_rows(t, 'spawn_funcs')
        spawn += table_rows(t, 'spawns')          # id/donor name for the same table
        # itemlist: designated .classname = "..." (q2pro) and the positional
        # 1998 form, where the classname is the first string in the row.
        items += re.findall(r'\.classname\s*=\s*"([^"]+)"', t)
    return sorted(set(spawn)), sorted(set(items))


# Only gi.cvar() REGISTERS a cvar.  gi.cvar_set() and gi.cvar_forceset() write
# one that already exists, and counting them as registrations is what turns
# baseq2's 37 into 38: g_spawn.c force-sets `skill` and sets `sv_gravity`, both
# of which g_main.c registered.  It also invents an R-COMPAT-6 duplicate out of
# a cvar the same tree legitimately writes twice.
CVAR_REG = re.compile(r'\bgi\.cvar\s*\(\s*"([^"]+)"\s*,\s*("(?:[^"\\]|\\.)*")?')


def cvars(files):
    """name -> [(file, default), ...] so a double REGISTRATION is visible.

    R-COMPAT-6 is about two translation units registering one name with
    different defaults -- RA2 and tourney both claiming `statsfile`.  Two
    registrations of the same name with the same default are idiomatic: the
    second gi.cvar() returns the first's cvar, which is how a donor re-obtains a
    handle.  So the duplicate report keys on the default, not the name.
    """
    out = {}
    for f in files:
        t = strip_comments(read(f))
        for m in CVAR_REG.finditer(t):
            out.setdefault(m.group(1), []).append(
                (os.path.basename(f), m.group(2) or '?'))
    return out


def commands(files):
    out = set()
    for f in files:
        t = strip_comments(read(f))
        # baseq2: a chain of Q_stricmp(cmd, "name") inside ClientCommand
        for m in re.finditer(r'Q_stricmp\s*\(\s*cmd\s*,\s*"([^"]+)"', t):
            out.add(m.group(1))
        for m in re.finditer(r'strcmp\s*\(\s*cmd\s*,\s*"([^"]+)"', t):
            out.add(m.group(1))
        # donors: table-driven, { "name", Cmd_Name_f }
        for tbl in ('cmds', 'commands', 'client_cmds', 'cmdlist'):
            for n in table_rows(t, tbl):
                out.add(n)
    return sorted(out)


def report(label, tree, args):
    files = sources(tree)
    if not files:
        print(f'{label}: no .c sources under {tree}')
        return
    spawn, items = classnames(files)
    cv = cvars(files)
    cmd = commands(files)
    total_cn = len(set(spawn) | set(items))

    print(f'=== {label}  ({len(files)} .c files, {tree})')
    print(f'  classnames      {total_cn:4}   '
          f'({len(spawn)} in the spawn table + {len(items)} item classnames'
          + (f', {len(set(spawn) & set(items))} in both)' if set(spawn) & set(items)
             else ')'))
    print(f'  cvars           {len(cv):4}   (unique gi.cvar* first arguments)')
    print(f'  client commands {len(cmd):4}')

    dupes = {k: v for k, v in cv.items()
             if len({d for _, d in v}) > 1 and len({f for f, _ in v}) > 1}
    if dupes:
        print(f'  !! {len(dupes)} cvar name(s) registered from more than one '
              f'translation unit WITH DIFFERENT DEFAULTS (R-COMPAT-6):')
        for k in sorted(dupes):
            for fn, d in sorted(set(dupes[k])):
                print(f'       {k}: {fn} -> {d}')
    benign = {k: v for k, v in cv.items()
              if len({f for f, _ in v}) > 1 and len({d for _, d in v}) == 1}
    if benign:
        print(f'  ({len(benign)} name(s) registered twice with the same default '
              f'-- idiomatic re-obtain, not R-COMPAT-6)')

    for what in args.list:
        names = {'classnames': sorted(set(spawn) | set(items)),
                 'spawn': spawn, 'items': items,
                 'cvars': sorted(cv), 'commands': cmd}[what]
        print(f'  -- {what} ({len(names)}):')
        for n in names:
            print(f'       {n}')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tree', action='append', default=[],
                    help='tree to count; repeatable; defaults to this repo\'s src/')
    ap.add_argument('--list', action='append', default=[],
                    choices=['classnames', 'spawn', 'items', 'cvars', 'commands'])
    ap.add_argument('--duplicates', action='store_true',
                    help='exit 1 if any cvar is registered twice (R-COMPAT-6)')
    a = ap.parse_args()

    trees = a.tree or [os.path.join(REPO, 'src')]
    bad = 0
    for t in trees:
        label = os.path.basename(os.path.normpath(t)) or t
        report(label, os.path.abspath(t), a)
        if a.duplicates:
            cv = cvars(sources(os.path.abspath(t)))
            bad += sum(1 for v in cv.values()
                       if len({d for _, d in v}) > 1
                       and len({f for f, _ in v}) > 1)
    return 1 if (a.duplicates and bad) else 0


if __name__ == '__main__':
    sys.exit(main())
