#!/usr/bin/env python3
"""Every literal compared against an entity classname resolves to one the tree
can actually produce (R-188).

WHY, AND WHY IT IS NOT `itemnames.py`.  That check resolves classname-shaped
literals against the **itemlist**, and it earns its keep: it is what found Ground
Zero's `item_spehre_defender` (R-183 item 11).  It asks the wrong question about
a literal that is not an item's name.  Two independent narrowings put `"telsa"`
outside it:

  * it resolves against `gitem_t.classname`, so a classname that belongs to a
    MONSTER, a projectile or a runtime-assigned entity was never in the space it
    compares with; and
  * it only looks at literals SHAPED like `weapon_*` / `ammo_*` / `item_*` /
    `key_*`, so a bare `"tesla"`, `"bolt"` or `"player"` is not even collected.

`m_move.c` compared `ent->enemy->classname` with `"telsa"` for twenty-seven
years -- Ground Zero's typo, still in upstream q2pro -- and the branch it guards
never ran once.  Nothing warns: `strcmp` against a misspelling is a valid call
that is simply always false, so the guarded branch is dead and control falls
wherever the chain's final `else` goes.  In that case it re-targeted the monster
and re-set `AI_BLOCKED` on every blocked frame (R-187 §2).

So this asks the OTHER question: not "is this a real item" but **"can anything in
this tree ever put this string in an `edict_t.classname`?"**  A literal that no
code path can produce is a comparison that cannot succeed.

THE UNIVERSE IS THREE SETS AND ALL THREE ARE NEEDED.  A classname reaches an
entity by exactly three routes, and a check that knew only one would report the
other two as findings:

  1. `g_spawn.c`'s spawn table -- `{"item_health", SP_item_health}` -- which is
     what `ED_CallSpawn` matches a map's classname against;
  2. the itemlist's `.classname` rows, which `ED_CallSpawn` falls through to and
     which `FindSubstituteItem` returns for `DoRandomRespawn` to spawn; and
  3. every `->classname = "literal"` assignment anywhere in the tree, which is
     where `"bolt"`, `"tesla"`, `"prox"`, `"player"`, `"bodyque"` and
     `"disconnected"` come from -- names no table declares.

The union is also why a `gitem_t.classname` comparison is safe to check with the
same predicate: set 2 is a subset of the union, so an item name resolves either
way and this cannot report one.

PARTIAL COMPARISONS ARE PARTIAL.  `strncmp(ent->classname, "monster_", 8)` and
`strstr(ent->classname, "item_rune")` are prefix and substring tests, so they
resolve when ANY producible classname starts with or contains the literal --
`item_rune` has no row of its own and `item_rune1..5` do.  Treating those as
exact would report six live tests as dead, which is how a check gets switched
off.  Case follows the function: `strcmp`/`strncmp`/`strstr` are exact,
`Q_stricmp`/`Q_strcasecmp`/`stricmp`/`Q_strncasecmp` fold.

WHAT IS NOT A FINDING.  A non-literal argument, which this cannot see; a literal
compared against any field that is not `->classname`, which is a different
contract; and the three Ground Zero map aliases, which are the one legitimate
case of a classname the tree deliberately does not produce -- see NOT_PRODUCED.

The exemptions are checked for going stale, for the reason `itemnames.py` gives:
an exemption list is how a check dies quietly.  An entry that no longer appears,
or that the tree can now produce, is itself reported.

USAGE
    tools/classnames.py [--tree src]
    tools/classnames.py --selftest
"""
import argparse
import os
import re
import shutil
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

BLOCK_RE = re.compile(r'/\*.*?\*/', re.S)

# --- the three producing routes ------------------------------------------
# `{ "classname", SP_func }`, g_spawn.c's table.
SPAWN_RE = re.compile(r'\{\s*"([^"]+)"\s*,\s*SP_[A-Za-z0-9_]+\s*\}')
# `.classname = "x"`, the itemlist's designated initialisers.
ITEM_RE = re.compile(r'\.classname\s*=\s*"([^"]*)"')
# `something->classname = "x"`, assigned at runtime.
ASSIGN_RE = re.compile(r'->classname\s*=\s*"([^"]*)"')

# --- the consuming shape -------------------------------------------------
# An lvalue ending in `->classname`, so `ent->classname`, `new_bad->owner->classname`
# and `game.clients[i].foo->classname` all match, and `ent->target` does not.
LVALUE = r'[A-Za-z_][A-Za-z0-9_.\[\]]*(?:->[A-Za-z0-9_.\[\]]+)*->classname'
FAMILY = r'strcmp|strncmp|strstr|Q_stricmp|Q_strcasecmp|stricmp|Q_strncasecmp|Q_stricmpn'
# Both argument orders.  The reverse one matches nothing in the tree today and is
# here so that writing it that way tomorrow does not silently leave the check.
# It is restricted to the comparison family on purpose: `gi.dprintf("%s",
# ent->classname)` has the same shape and is a format string, not a comparison.
CMP_RE = re.compile(r'\b(' + FAMILY + r')\s*\(\s*(' + LVALUE + r')\s*,\s*"([^"]*)"', re.S)
CMP_REV_RE = re.compile(r'\b(' + FAMILY + r')\s*\(\s*"([^"]*)"\s*,\s*(' + LVALUE + r')', re.S)

FOLDING = {'Q_stricmp', 'Q_strcasecmp', 'stricmp', 'Q_strncasecmp', 'Q_stricmpn'}
PARTIAL = {'strncmp', 'Q_strncasecmp', 'Q_stricmpn', 'strstr'}

# Classnames the tree deliberately never produces, each verified and with the
# reason recorded so a later reader can re-verify rather than trust the list.
# A stale entry is reported -- see check_exemptions().
NOT_PRODUCED = {
    # Ground Zero accepts three classnames from MAPS that it renames before the
    # spawn lookup, so the tree compares against them and never assigns them.
    # itemnames.py exempts the same three, for the same reason, one question over.
    'weapon_nailgun':  'g_spawn.c map alias -> weapon_etf_rifle (PMM hack)',
    'ammo_nails':      'g_spawn.c map alias -> ammo_flechettes (PMM hack)',
    'weapon_heatbeam': 'g_spawn.c map alias -> weapon_plasmabeam (PMM hack)',
}


def strip_comments(src):
    """Both comment forms, with the line count preserved.

    A `/* */` block is blanked to its own newlines rather than deleted, so a
    finding below one still reports the line it is on -- and this file's own
    header names `"telsa"`, so a check that read comments would report itself.
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


def producible(tree):
    """Every classname the tree can put in an edict, by all three routes."""
    names = set()
    for _, code in sources(tree):
        names |= {m.group(1) for m in SPAWN_RE.finditer(code)}
        names |= {m.group(1) for m in ITEM_RE.finditer(code)}
        names |= {m.group(1) for m in ASSIGN_RE.finditer(code)}
    return names


def comparisons(tree):
    """Every (file, line, function, literal) compared against a ->classname."""
    out = []
    for p, code in sources(tree):
        rel = os.path.relpath(p, REPO)
        for rx, lit_group in ((CMP_RE, 3), (CMP_REV_RE, 2)):
            for m in rx.finditer(code):
                line = code.count('\n', 0, m.start()) + 1
                out.append((rel, line, m.group(1), m.group(lit_group)))
    return out


def resolves(name, func, names, folded):
    """Does this literal name something the tree can produce?"""
    if func in PARTIAL:
        # A prefix or substring test succeeds against any classname that has it.
        if func in FOLDING:
            return any(name.lower() in c for c in folded)
        return any(name in c for c in names)
    if func in FOLDING:
        return name.lower() in folded
    return name in names


def check_exemptions(seen, exempt, resolving):
    """An exemption that is no longer needed is a finding of its own."""
    out = []
    for name in sorted(exempt):
        if name not in seen:
            out.append(('tools/classnames.py', 0, 'stale exemption',
                        '%s is compared against no classname in the tree' % name))
        elif name in resolving:
            out.append(('tools/classnames.py', 0, 'stale exemption',
                        '%s is a producible classname now' % name))
    return out


def scan(tree, exempt=None, check_stale=True):
    if exempt is None:
        exempt = NOT_PRODUCED
    names = producible(tree)
    if not names:
        # Nothing to compare against is not a pass.  Reported rather than
        # skipped, for the reason audit.py's header gives: a vacuous pass is how
        # a check rots.
        return None
    folded = {n.lower() for n in names}

    hits = []
    seen, resolving = set(), set()
    for rel, line, func, name in comparisons(tree):
        seen.add(name)
        if resolves(name, func, names, folded):
            resolving.add(name)
        elif name not in exempt:
            hits.append((rel, line, func, name))
    if check_stale:
        hits += check_exemptions(seen, exempt, resolving)
    return hits


# The clean tree carries one producer of each of the three kinds and the
# comparisons that must NOT fire: an exact hit, a folded hit, a prefix, a
# substring, a non-literal, a comparison against a different field, one in each
# comment form, and one exempt name.
SELFTEST_EXEMPT = {'weapon_nailgun': 'map alias'}

SELFTEST_CLEAN = {
    'g_spawn.c': '''
static const spawn_t spawns[] = {
    { "item_health", SP_item_health },
    { "monster_tank", SP_monster_tank },
    { NULL, NULL }
};
void alias(edict_t *ent)
{
    if (!strcmp(ent->classname, "weapon_nailgun"))
        ent->classname = "weapon_etf_rifle";
}
''',
    'g_items.c': '''
const gitem_t itemlist[] = {
    { .classname = "weapon_etf_rifle", .pickup_name = "ETF Rifle" },
    { .classname = "item_rune1" },
    { NULL }
};
''',
    'g_weapon.c': '''
void fire(edict_t *self)
{
    edict_t *bolt = G_Spawn();
    bolt->classname = "bolt";
    self->classname = "tesla";
}
''',
    'use.c': '''
void f(edict_t *ent, const char *name)
{
    if (!strcmp(ent->classname, "bolt")) return;            // runtime assign
    if (!strcmp(ent->classname, "tesla")) return;           // runtime assign
    if (!strcmp(ent->classname, "item_health")) return;     // spawn table
    if (!strcmp(ent->classname, "weapon_etf_rifle")) return; // itemlist
    if (!Q_stricmp(ent->classname, "MONSTER_tank")) return; // folds
    if (!strncmp(ent->classname, "monster_", 8)) return;    // prefix
    if (strstr(ent->classname, "item_rune")) return;        // substring
    if (!strcmp(ent->classname, name)) return;              // not a literal
    if (!strcmp(ent->target, "nosuchthing")) return;        // not a classname
    // strcmp(ent->classname, "telsa") in a line comment is prose
}

/*
 * strcmp(ent->classname, "alsonothing") in a BLOCK comment is prose too --
 * this file's own header names "telsa", so getting this wrong makes the audit
 * report itself.
 */
''',
}

SELFTEST_MUTANTS = [
    ('a misspelled classname (the "telsa" shape)',
     'void g(edict_t *e) { if (!strcmp(e->enemy->classname, "telsa")) return; }'),
    ('a misspelling through a chained lvalue',
     'void g(edict_t *b) { if (!strcmp(b->owner->classname, "teslaa")) return; }'),
    ('a classname that is only ever an ITEM PICKUP NAME',
     'void g(edict_t *e) { if (!strcmp(e->classname, "ETF Rifle")) return; }'),
    ('a real classname in the wrong case under strcmp',
     'void g(edict_t *e) { if (!strcmp(e->classname, "Bolt")) return; }'),
    ('a prefix no classname begins with',
     'void g(edict_t *e) { if (!strncmp(e->classname, "monstre_", 8)) return; }'),
    ('a substring no classname contains',
     'void g(edict_t *e) { if (strstr(e->classname, "item_runes")) return; }'),
    ('the reverse argument order',
     'void g(edict_t *e) { if (!strcmp("telsa", e->classname)) return; }'),
]

SELFTEST_STALE = [
    ('an exemption for a name compared nowhere',
     {'weapon_nailgun': 'x', 'weapon_vanished': 'x'}),
    ('an exemption for a name the tree can produce now',
     {'weapon_nailgun': 'x', 'bolt': 'x'}),
]

# A format string with a classname argument is the shape most likely to be
# mistaken for a comparison, so it is a control of its own.
SELFTEST_NOT_A_COMPARISON = (
    'a printf-family call naming a classname is not a comparison',
    'void g(edict_t *e) { gi.dprintf("%s: %s\\n", e->classname, "telsa"); }')


def selftest():
    ok = True
    tmp = tempfile.mkdtemp(prefix='classnames-selftest-')

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
        report(not base, 'spawn table, itemlist and runtime assign all produce; '
                         'exact, folded, prefix, substring, non-literal, other '
                         'field, both comment forms, live exemption: %s'
                         % ('0 findings' if not base else base))

        for why, body in SELFTEST_MUTANTS:
            p = os.path.join(tmp, 'bad.c')
            open(p, 'w').write(body + '\n')
            got = scan(tmp, SELFTEST_EXEMPT)
            os.remove(p)
            report(bool(got), '%s -> %s' % (why, 'caught' if got else 'MISSED'))

        why, body = SELFTEST_NOT_A_COMPARISON
        p = os.path.join(tmp, 'fmt.c')
        open(p, 'w').write(body + '\n')
        got = scan(tmp, SELFTEST_EXEMPT)
        os.remove(p)
        report(not got, '%s -> %s' % (why, 'not reported' if not got
                                      else 'REPORTED: %s' % got))

        for why, ex in SELFTEST_STALE:
            got = scan(tmp, ex)
            report(bool(got), '%s -> %s' % (why, 'caught' if got else 'MISSED'))

        # ...and a tree that produces no classname at all must report.
        empty = tempfile.mkdtemp(prefix='classnames-empty-')
        try:
            report(scan(empty) is None,
                   'no producible classname to compare against -> reported')
            # ...and a PARTIAL tree must not report every exemption as stale,
            # which is what pointing --tree at one subdirectory does.
            part = scan(tmp, NOT_PRODUCED, check_stale=False)
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
        print('!! classnames: nothing in %s produces a classname to compare '
              'against' % tree)
        return 1
    for path, line, what, name in hits:
        if what == 'stale exemption':
            print('!! %s: stale exemption: %s (R-188)' % (path, name))
        else:
            print('!! %s:%d: %s(..., "%s") -- no code path produces that '
                  'classname (R-188)' % (path, line, what, name))
    names = producible(tree)
    print('classnames: %d dead comparison(s); %d classname(s) are producible '
          'and %d literal comparison(s) resolve against them, %d exempt%s'
          % (len(hits), len(names), len(comparisons(tree)), len(NOT_PRODUCED),
             '' if whole else ' (partial tree: exemptions not checked for '
                              'going stale)'))
    return 1 if hits else 0


if __name__ == '__main__':
    sys.exit(main())
