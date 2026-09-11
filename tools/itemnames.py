#!/usr/bin/env python3
"""Every literal item name in the tree resolves to a real itemlist row.

WHY.  `FindItem()` matches `gitem_t.pickup_name` and `FindItemByClassname()`
matches `gitem_t.classname`, both by exact string, and both return **NULL** for a
name that is not there.  Nothing warns: the call compiles, the lookup fails at
runtime, and what happens next depends entirely on what the caller does with the
NULL.  Two things it does in this tree:

  * assigns it -- `ent->client->newweapon = FindItem("ionrippergun")`, which is
    p_weapon.c's real defect and is upstream Xatrix's too.  The Ionripper is
    simply never chosen by the out-of-ammo fallback, silently, forever.
  * indexes with it -- `ITEM_INDEX(FindItem("x"))` is `(x) - itemlist`, so a
    NULL becomes a large negative subscript and the next line reads or writes
    `pers.inventory[]` out of bounds.

The second is the reason this is a build check rather than a grep somebody runs.
There are over two hundred literal lookups in the tree and a merged union
itemlist is exactly where a name goes stale: the merge unions six donors' items
into one list, so a donor's own spelling can arrive correct and be renamed by the
union, and a name invented by one donor for another's item never resolved at all.

TWO KINDS OF FINDING, because there are two ways to reach an item by name.

1.  A string literal passed to FindItem() or FindItemByClassname() that matches
    no `.pickup_name` / `.classname` in `g_items.c`'s itemlist.  Compared
    case-INSENSITIVELY, because both functions use Q_stricmp.

2.  Any CLASSNAME-SHAPED literal anywhere in the tree -- `weapon_*`, `ammo_*`,
    `item_*`, `key_*` -- that matches no `.classname`.  Compared case-SENSITIVELY,
    because the code that reads these is `strcmp(ent->classname, ...)` and a
    table of classnames, not a FindItem() call.  This half is the one that
    earns its keep: it is what found `item_spehre_defender` in
    `FindSubstituteItem`, Ground Zero's own typo, still present upstream, which
    means `DF_NO_SPHERES` never recognised the Defender sphere.  A targeted
    check that looked only at the tables already touched would have missed it.

The second kind needs an EXEMPTION LIST, and an exemption list is how a check
dies quietly, so the exemptions are checked too: one that no longer appears in
the tree, or that has since become a real item, is itself a finding.  That half
runs only when the WHOLE tree is being scanned, because an exemption is defined
against the whole tree -- point `--tree` at one subdirectory and every exemption
for a name outside it would report, which is noise rather than a finding.

WHAT IS NOT A FINDING.  A non-literal argument -- `FindItem(name)`,
`FindItemByClassname(tnames[i])` -- which this check cannot see and does not
pretend to; and the itemlist row for the Tag Token, which has a pickup_name and
deliberately no classname.

BOTH COMMENT FORMS ARE STRIPPED, from the callers AND from the itemlist, and each
half earns it.  Prose naming a missing item is not a finding -- this file's own
header names `ionrippergun`, so a check that read comments would report itself.
And a COMMENTED-OUT itemlist row is not a row: `item_torch` sits inside a `/* */`
block in g_items.c, and counting it would make `FindItem("torch")` resolve
against an item the game does not have.

USAGE
    tools/itemnames.py [--tree src]
    tools/itemnames.py --selftest
"""
import argparse
import os
import re
import shutil
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

BLOCK_RE = re.compile(r'/\*.*?\*/', re.S)
CLASSNAME_RE = re.compile(r'\.classname\s*=\s*"([^"]*)"')
PICKUP_RE = re.compile(r'\.pickup_name\s*=\s*"([^"]*)"')
CALL_RE = re.compile(r'\b(FindItem|FindItemByClassname)\s*\(\s*"([^"]*)"')
# Case-insensitive on the SHAPE, so a right name in the wrong case is still
# recognised as an attempt at a classname and then fails to resolve.
SHAPED_RE = re.compile(r'"((?:weapon|ammo|item|key)_[a-z0-9_]+)"', re.I)

# Classname-shaped literals that are deliberately not classnames.  Every one is
# verified, and the reason is recorded so a later reader can re-verify it rather
# than trust the list.  A stale entry is reported -- see check_exemptions().
NOT_AN_ITEM = {
    # baseq2 health has no itemlist row at all; it is spawned by SP_item_health*
    # and FindSubstituteItem returns these to be spawned that way.
    'item_health':          'spawned by SP_item_health, no itemlist row',
    'item_health_small':    'spawned by SP_item_health_small, no itemlist row',
    'item_health_large':    'spawned by SP_item_health_large, no itemlist row',
    'item_health_mega':     'spawned by SP_item_health_mega, no itemlist row',
    # Ground Zero's own deliberate rename, applied in ED_CallSpawn before the
    # spawn lookup: three map classnames that mean three real items.
    'weapon_nailgun':       'g_spawn.c alias -> ETF Rifle (PMM classnames hack)',
    'ammo_nails':           'g_spawn.c alias -> Flechettes (PMM classnames hack)',
    'weapon_heatbeam':      'g_spawn.c alias -> Plasma Beam (PMM classnames hack)',
    # Names that only look like classnames.
    'item_pickup':          'osp_stats.c JSON event type',
    'item_use':             'osp_stats.c JSON event type',
    'item_expire':          'osp_stats.c JSON event type',
    'item_drop':            'osp_stats.c JSON event type',
    'item_toggle':          'osp_cmds.c vote name',
    'item_rune':            'osp_runes.c strstr() PREFIX, matches item_rune1..5',
    'weapon_have':          'osp_main.c cvar name',
    'weapon_initial':       'osp_main.c cvar name',
}


def itemlist_names(tree, fold=True):
    """The two name spaces the itemlist actually defines.

    Lowercased by default, because FindItem() and FindItemByClassname() both
    compare with Q_stricmp.  `fold=False` returns them verbatim, for the
    consumers that match with strcmp() instead.
    """
    path = os.path.join(tree, 'g_items.c')
    if not os.path.exists(path):
        return None, None
    src = strip_comments(open(path, errors='replace').read())
    i = src.find('itemlist[] = {')
    if i < 0:
        return None, None
    body = src[i:]
    norm = (lambda s: s.lower()) if fold else (lambda s: s)
    return ({norm(m.group(1)) for m in CLASSNAME_RE.finditer(body)},
            {norm(m.group(1)) for m in PICKUP_RE.finditer(body)})


def strip_comments(src):
    """Both comment forms, with the line count preserved.

    A `/* */` block is blanked to its own newlines rather than deleted, so a
    finding below one still reports the line it is on.  Line-at-a-time stripping
    cannot do this, which is how this check first reported its own prose: the
    header comment above names the very lookup it exists to catch.
    """
    def blank(m):
        return '\n' * m.group(0).count('\n')
    return re.sub(r'//[^\n]*', '', BLOCK_RE.sub(blank, src))


def sources(tree):
    for root, dirs, files in os.walk(tree):
        for fn in sorted(files):
            if fn.endswith(('.c', '.h')):
                p = os.path.join(root, fn)
                yield p, strip_comments(open(p, errors='replace').read())


def check_exemptions(seen, exempt, resolving):
    """An exemption that is no longer needed is a finding of its own.

    `seen` is every classname-shaped literal in the tree, `resolving` those that
    now match a real itemlist classname.  An exemption for a name that has gone,
    or that has since become a real item, is dead weight that hides the next
    typo with the same spelling.
    """
    out = []
    for name in sorted(exempt):
        if name not in seen:
            out.append(('tools/itemnames.py', 0, 'stale exemption',
                        '%s no longer appears in the tree' % name))
        elif name in resolving:
            out.append(('tools/itemnames.py', 0, 'stale exemption',
                        '%s is a real itemlist classname now' % name))
    return out


def scan(tree, exempt=None, check_stale=True):
    if exempt is None:
        exempt = NOT_AN_ITEM
    folded_class, folded_pickup = itemlist_names(tree)
    if folded_class is None:
        # No itemlist to compare against is not a pass.  Reported as a finding
        # rather than skipped, for the reason audit.py's own header gives: a
        # vacuous pass is how a check rots.
        return None
    exact_class, _ = itemlist_names(tree, fold=False)

    hits = []
    seen, resolving = set(), set()
    for p, code in sources(tree):
        rel = os.path.relpath(p, REPO)
        for n, line in enumerate(code.split('\n'), 1):
            for m in CALL_RE.finditer(line):
                func, name = m.group(1), m.group(2)
                space = folded_pickup if func == 'FindItem' else folded_class
                if name.lower() not in space:
                    hits.append((rel, n, func, name))
            for m in SHAPED_RE.finditer(line):
                name = m.group(1)
                seen.add(name)
                if name in exact_class:
                    resolving.add(name)
                elif name not in exempt:
                    hits.append((rel, n, 'classname literal', name))
    if check_stale:
        hits += check_exemptions(seen, exempt, resolving)
    return hits


# The clean tree is a miniature itemlist plus the lookups that must NOT fire:
# one differing only in case, one non-literal, one in each comment form, a row
# with no classname, and one exempt classname-shaped name.
SELFTEST_EXEMPT = {'item_health': 'spawned by SP_item_health'}

SELFTEST_CLEAN = {
    'g_items.c': '''
const gitem_t itemlist[] = {
    { NULL },
    {
        .classname   = "weapon_boomer",
        .pickup_name = "Ionripper",
    },
    {
        .classname   = "ammo_magslug",
        .pickup_name = "Mag Slug",
    },
    // A row with a pickup_name and no classname, like the Tag Token.
    {
        .pickup_name = "Tag Token",
    },
    { NULL }
};
''',
    'use.c': '''
void f(void)
{
    FindItem("ionripper");              // case differs, Q_stricmp says equal
    FindItem("Mag Slug");
    FindItem("Tag Token");
    FindItemByClassname("weapon_boomer");
    FindItem(name);                     // not a literal, not our business
    strcmp(ent->classname, "item_health");   // exempt: no itemlist row
    // FindItem("nosuchthing") in a line comment is prose, not code
}

/*
 * FindItem("alsonothing") in a BLOCK comment is prose too -- this check's own
 * header names the lookup it exists to catch, so getting this wrong makes the
 * audit report itself.
 */
void h(void)
{
    FindItemByClassname("ammo_magslug");
}
''',
}

SELFTEST_MUTANTS = [
    ('a pickup_name that does not exist (the ionrippergun shape)',
     'void g(void) { ent->newweapon = FindItem("ionrippergun"); }'),
    ('a classname that does not exist',
     'void g(void) { FindItemByClassname("weapon_nosuchgun"); }'),
    ('a pickup_name looked up as a classname',
     'void g(void) { FindItemByClassname("Ionripper"); }'),
    ('a misspelled classname in a strcmp (the item_spehre_defender shape)',
     'void g(void) { if (!strcmp(ent->classname, "weapon_boommer")) return; }'),
    ('a real classname in the wrong case (strcmp, not stricmp)',
     'void g(void) { if (!strcmp(ent->classname, "Weapon_Boomer")) return; }'),
    ('a classname-shaped name in a table, not a call at all',
     'static const char *t[] = { "ammo_magslug", "ammo_magslugs" };'),
]

SELFTEST_STALE = [
    ('an exemption for a name that has gone from the tree',
     {'item_health': 'x', 'item_vanished': 'x'}),
    ('an exemption for a name that is a real item now',
     {'item_health': 'x', 'weapon_boomer': 'x'}),
]


def selftest():
    ok = True
    tmp = tempfile.mkdtemp(prefix='itemnames-selftest-')

    def report(good, why):
        nonlocal ok
        if good:
            print('   [control] %s' % why)
        else:
            print('!! selftest: %s' % why)
            ok = False

    try:
        for name, body in SELFTEST_CLEAN.items():
            p = os.path.join(tmp, name)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            open(p, 'w').write(body)
        base = scan(tmp, SELFTEST_EXEMPT)
        report(not base, 'case-insensitive, non-literal, line and block comment, '
                         'classname-less row, live exemption: %s'
                         % ('0 findings' if not base else base))

        for why, body in SELFTEST_MUTANTS:
            p = os.path.join(tmp, 'bad.c')
            open(p, 'w').write(body + '\n')
            got = scan(tmp, SELFTEST_EXEMPT)
            os.remove(p)
            report(bool(got), '%s -> %s' % (why, 'caught' if got else 'MISSED'))

        for why, ex in SELFTEST_STALE:
            got = scan(tmp, ex)
            report(bool(got), '%s -> %s' % (why, 'caught' if got else 'MISSED'))

        # ...and a tree with no itemlist must report, not pass.
        empty = tempfile.mkdtemp(prefix='itemnames-empty-')
        try:
            report(scan(empty) is None,
                   'no itemlist to compare against -> reported')
            # ...and a PARTIAL tree must not report every exemption as stale,
            # which is what pointing --tree at one subdirectory does.
            part = scan(tmp, NOT_AN_ITEM, check_stale=False)
            report(not part, 'a partial tree reports no stale exemption: %s'
                             % ('0 findings' if not part else part))
        finally:
            shutil.rmtree(empty, ignore_errors=True)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tree', default=os.path.join(REPO, 'src'))
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    if a.selftest:
        return 0 if selftest() else 1

    tree = os.path.abspath(a.tree)
    # The exemptions are defined against the whole source tree, so they are only
    # checked for going stale when that is what is being scanned.
    whole = (tree == os.path.join(REPO, 'src'))
    hits = scan(tree, check_stale=whole)
    if hits is None:
        print('!! itemnames: no itemlist in %s/g_items.c to compare against'
              % tree)
        return 1
    for path, line, what, name in hits:
        if what == 'stale exemption':
            print('!! %s: stale exemption: %s' % (path, name))
        else:
            print('!! %s:%d: %s("%s") matches no itemlist row'
                  % (path, line, what, name))
    classnames, pickups = itemlist_names(tree)
    print('itemnames: %d unresolved item name(s); the itemlist defines %d '
          'classname(s) and %d pickup name(s), and %d classname-shaped literal(s)'
          ' are exempt%s' % (len(hits), len(classnames), len(pickups),
                             len(NOT_AN_ITEM),
                             '' if whole else ' (partial tree: exemptions not '
                                              'checked for going stale)'))
    return 1 if hits else 0


if __name__ == '__main__':
    sys.exit(main())
