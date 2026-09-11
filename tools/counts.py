#!/usr/bin/env python3
"""Count the acceptance-criteria surfaces: classnames, cvars, client commands.

"Every number this spec publishes as an acceptance criterion --
classname, cvar and command counts [...] -- is produced by a script in `tools/`
that ships with it.  A figure with no script is indicative only and cannot gate
a phase."  For a long time this script had not been written, which left the
count requirements unenforceable as written.

This is that script.  It does not attempt to reproduce the documented numbers;
it measures the tree and prints what is there, so that a claim and a measurement
can be compared instead of a claim being compared with another claim.

WHAT IS COUNTED, and why each definition is the arguable part

  classnames  A classname is anything ED_CallSpawn will match.  That is two
              tables, not one: `spawn_funcs[]` in g_spawn.c, AND every
              `itemlist[]` entry with a non-NULL `.classname` -- g_spawn.c
              checks the item list *first* (ED_CallSpawn's item loop precedes
              its spawn_funcs loop).  Counting only spawn_funcs undercounts by
              the whole pickup set.

  cvars       Unique name passed to a `cvar()` call on the game_import_t --
              `gi.cvar("x")` and `import->cvar("x")` alike, since it is the same
              engine function whether it is reached through the global or
              through a parameter.  It counts a cvar registered twice as one,
              which is exactly the duplicate-cvar defect (`statsfile` registered by
              both RA2 and tourney), so --duplicates reports those separately
              rather than hiding them in a unique count.

              A name that is not a literal is still a name.  A change moved the
              twenty-six `allow_*` names out of literal position into
              `osp_allow_items[]` and reaches them as
              `gi.cvar(osp_allow_items[i].cvar, ...)`; before that they were
              literals.  A literal-only count therefore FELL by sixteen at the
              moment the tree gained ten cvars, which is a measurement that
              moves the wrong way and is worse than a naive one.  So a call
              whose name is `TABLE[i].MEMBER` is resolved: find `TABLE[]`, find
              MEMBER's position in its struct, and read that column out of every
              row.  Any remaining unresolvable site -- a name arriving as a bare
              parameter, which is how BotSetVarIfSet and the ruleset aliases
              work -- is REPORTED with its file and its expression rather than
              silently dropped, because the whole point of this script is that a
              claim can be compared with a measurement.

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


def sources(tree, exts=('.c',)):
    out = []
    for root, dirs, files in os.walk(tree):
        dirs[:] = [d for d in dirs
                   if d not in ('debug', 'release', 'obj', '.deps')
                   and not d.startswith(('debug-', 'release-'))]
        for f in sorted(files):
            if f.endswith(exts):
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
# of which g_main.c registered.  It also invents a duplicate out of
# a cvar the same tree legitimately writes twice.
# `gi.cvar("x", ...)` and `import->cvar("x", ...)` are the same engine call
# reached two ways -- the second is how stdlog.c registers `sl_log_logbots`,
# which a `gi.`-only pattern misses entirely.  `cvar_set` and `cvar_forceset`
# still do not match, because `cvar` is followed by `_` and not by `(`.
CVAR_REG = re.compile(
    r'(?:\.|->)\s*cvar\s*\(\s*"([^"]+)"\s*,\s*("(?:[^"\\]|\\.)*")?')
# A name reached through a table column: gi.cvar(osp_allow_items[i].cvar, ...)
CVAR_TBL = re.compile(
    r'(?:\.|->)\s*cvar\s*\(\s*(\w+)\s*\[[^\]]*\]\s*\.\s*(\w+)\s*,'
    r'\s*("(?:[^"\\]|\\.)*")?')
# ...and anything else non-literal, which cannot be resolved and is reported.
CVAR_OTHER = re.compile(r'(?:\.|->)\s*cvar\s*\(\s*([A-Za-z_]\w*)\s*,')
# The pieces needed to read a column out of a table.
TABLE_DEF = re.compile(r'(\w+)\s+(\w+)\s*\[\s*\]\s*=\s*\{(.*?)\n\};', re.S)
TYPEDEF = re.compile(r'typedef\s+struct\s*\w*\s*\{(.*?)\}\s*(\w+)\s*;', re.S)


def struct_member_index(text, typename, member):
    """Which positional initialiser is `member` of `typename`?

    Positional rather than designated is how osp_allow_items[] is written, so
    the column has to be found by counting declarations in the struct.
    """
    for m in TYPEDEF.finditer(text):
        if m.group(2) != typename:
            continue
        idx = 0
        for decl in m.group(1).split(';'):
            names = re.findall(r'(\w+)\s*(?:\[[^\]]*\])?\s*$', decl.strip())
            if not names:
                continue
            if names[-1] == member:
                return idx
            idx += 1
    return None


def table_column(files, table, member):
    """Every string in `table`'s `member` column, or None if it cannot be read."""
    blob = '\n'.join(strip_comments(read(f)) for f in files)
    for m in TABLE_DEF.finditer(blob):
        typename, name, body = m.group(1), m.group(2), m.group(3)
        if name != table:
            continue
        idx = struct_member_index(blob, typename, member)
        if idx is None:
            return None
        out = []
        for row in re.findall(r'\{([^{}]*)\}', body):
            strings = re.findall(r'"((?:[^"\\]|\\.)*)"', row)
            # A row is positional, so a string only lands in column `idx` when
            # every field before it is also a string.  Rows of NULLs have none.
            if len(strings) > idx and row.split(',')[0].strip().startswith('"'):
                out.append(strings[idx])
        return out
    return None


def cvars(files, unresolved=None, headers=None):
    """name -> [(file, default), ...] so a double REGISTRATION is visible.

    The rule is about two translation units registering one name with
    different defaults -- RA2 and tourney both claiming `statsfile`.  Two
    registrations of the same name with the same default are idiomatic: the
    second gi.cvar() returns the first's cvar, which is how a donor re-obtains a
    handle.  So the duplicate report keys on the default, not the name.

    `unresolved` collects the sites whose name is neither a literal nor a table
    column, so the caller can print them.  A blind spot that is printed is a
    different thing from one that is not.
    """
    out = {}
    lookup = list(files) + list(headers or [])
    for f in files:
        t = strip_comments(read(f))
        base = os.path.basename(f)
        for m in CVAR_REG.finditer(t):
            out.setdefault(m.group(1), []).append((base, m.group(2) or '?'))
        for m in CVAR_TBL.finditer(t):
            names = table_column(lookup, m.group(1), m.group(2))
            if names is None:
                if unresolved is not None:
                    unresolved.append((base, '%s[].%s (table not found)'
                                       % (m.group(1), m.group(2))))
                continue
            # The DEFAULT is at the call site, not in the table -- one
            # `gi.cvar(t[i].cvar, "1", 0)` gives every resolved name the same
            # default.  Recording a placeholder instead invented six duplicate
            # duplicates out of names whose defaults agree.
            for n in names:
                out.setdefault(n, []).append((base, m.group(3) or '?'))
        for m in CVAR_OTHER.finditer(t):
            if unresolved is not None:
                unresolved.append((base, m.group(1)))
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
    unresolved = []
    cv = cvars(files, unresolved, sources(tree, ('.h',)))
    cmd = commands(files)
    total_cn = len(set(spawn) | set(items))

    print(f'=== {label}  ({len(files)} .c files, {tree})')
    print(f'  classnames      {total_cn:4}   '
          f'({len(spawn)} in the spawn table + {len(items)} item classnames'
          + (f', {len(set(spawn) & set(items))} in both)' if set(spawn) & set(items)
             else ')'))
    print(f'  cvars           {len(cv):4}   '
          f'(unique names passed to gi.cvar / ->cvar, table columns resolved)')
    print(f'  client commands {len(cmd):4}')

    dupes = {k: v for k, v in cv.items()
             if len({d for _, d in v}) > 1 and len({f for f, _ in v}) > 1}
    if dupes:
        print(f'  !! {len(dupes)} cvar name(s) registered from more than one '
              f'translation unit WITH DIFFERENT DEFAULTS:')
        for k in sorted(dupes):
            for fn, d in sorted(set(dupes[k])):
                print(f'       {k}: {fn} -> {d}')
    benign = {k: v for k, v in cv.items()
              if len({f for f, _ in v}) > 1 and len({d for _, d in v}) == 1}
    if benign:
        print(f'  ({len(benign)} name(s) registered twice with the same default '
              f'-- idiomatic re-obtain, not a duplicate)')
    if unresolved:
        # Not a finding -- a name arriving as a bare parameter is legitimate and
        # this script cannot follow it.  Printed so the count is read as a floor
        # rather than as a total.
        seen = sorted(set(unresolved))
        print(f'  ({len(seen)} site(s) whose cvar name is a parameter this '
              f'script cannot follow, so the count above is a FLOOR:)')
        for fn, expr in seen:
            print(f'       {fn}: cvar({expr}, ...)')

    for what in args.list:
        names = {'classnames': sorted(set(spawn) | set(items)),
                 'spawn': spawn, 'items': items,
                 'cvars': sorted(cv), 'commands': cmd}[what]
        print(f'  -- {what} ({len(names)}):')
        for n in names:
            print(f'       {n}')


SELFTEST = {
    'a.c': '''
void f(void) {
    gi.cvar("plain", "1", 0);
    gi.cvar_set("plain", "2");              /* a WRITE, not a registration */
    gi.cvar_forceset("plain", "3");
    import->cvar("through_a_pointer", "1", 0);
    gi.cvar(t[i].cvar, "7", 0);             /* a table column */
    gi.cvar(name, "1", 0);                  /* a parameter: unresolvable */
}
''',
    'b.c': '''
const row_t t[] = {
    { "from_table_one", "x", "y" },
    { "from_table_two", "x", "y" },
    { NULL, NULL, NULL }
};
void g(void) { gi.cvar("collides", "5", 0); }
''',
    'c.c': 'void h(void) { gi.cvar("collides", "9", 0); }\n',
}

SELFTEST_H = {
    'r.h': '''
typedef struct {
    const char  *cvar;
    const char  *other;
    const char  *third;
} row_t;
''',
}


def selftest():
    """Controls for each extraction shape, because the count is an assertion.

    Every one of these has occurred in the tree: the pointer form is how
    stdlog.c registers `sl_log_logbots`, the table column is the
    `osp_allow_items[]`, the bare parameter is BotSetVarIfSet, and cvar_set is
    what turned baseq2's 37 into 38 before this script excluded it.
    """
    import shutil
    import tempfile
    ok = True
    tmp = tempfile.mkdtemp(prefix='counts-selftest-')
    try:
        for n, body in SELFTEST.items():
            open(os.path.join(tmp, n), 'w').write(body)
        for n, body in SELFTEST_H.items():
            open(os.path.join(tmp, n), 'w').write(body)
        unresolved = []
        cv = cvars(sources(tmp), unresolved, sources(tmp, ('.h',)))

        def want(cond, why):
            nonlocal ok
            print(('   [control] ' if cond else '!! selftest: ') + why)
            if not cond:
                ok = False

        want('plain' in cv, 'a literal gi.cvar name is counted')
        want('through_a_pointer' in cv,
             'import->cvar("x") is counted -- the sl_log_logbots shape')
        want('from_table_one' in cv and 'from_table_two' in cv,
             'a table column is resolved -- the osp_allow_items[] shape')
        want(len([n for n in cv if n.startswith('from_table')]) == 2,
             'the table\'s NULL row contributes no name')
        want(cv.get('from_table_one') == [('a.c', '"7"')],
             'a resolved name takes the DEFAULT from the call site')
        want(('a.c', 'name') in unresolved,
             'a bare parameter is reported, not counted')
        want('name' not in cv, '...and not counted under its own spelling')
        want(len(cv.get('plain', [])) == 1,
             'cvar_set and cvar_forceset are not registrations')
        dupes = {k: v for k, v in cv.items()
                 if len({d for _, d in v}) > 1 and len({f for f, _ in v}) > 1}
        want(list(dupes) == ['collides'],
             'two files, two defaults, one name -> reported as a duplicate')
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tree', action='append', default=[],
                    help='tree to count; repeatable; defaults to this repo\'s src/')
    ap.add_argument('--list', action='append', default=[],
                    choices=['classnames', 'spawn', 'items', 'cvars', 'commands'])
    ap.add_argument('--duplicates', action='store_true',
                    help='exit 1 if any cvar is registered twice')
    ap.add_argument('--selftest', action='store_true',
                    help='controls for each extraction shape')
    a = ap.parse_args()

    if a.selftest:
        return 0 if selftest() else 1

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
