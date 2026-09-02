#!/usr/bin/env python3
"""A donor's own surface, used in a spine file, must be inside that donor's gate.

R-VER-25, doc/reconciliation.md R-75.

WHY.  Every ruleset shares one library and one `gclient_t`, so a donor's fields
exist for every client and its functions link into every build.  Nothing stops a
spine file reading RA2's `resp.fightstate` under `sp`; the field is there, it
compiles, and it is **zero** -- which is `FIGHT_SPECTATING`, which is not
`FIGHT_ALIVE`, which makes the test false.  Six sites in the Phase 4 merge did
exactly that and each one deleted behaviour from the whole game rather than
adding any to arena (R-70).  None was a merge conflict and none was visible to a
compiler, an audit or a boot.

`gates.py` asks the neighbouring question -- "is a ruleset chosen by testing a
cvar?" -- and cannot see this one, because these sites test a *field*, not a
cvar, and the field is legitimately in the union.

WHAT COUNTS AS THE DONOR'S SURFACE.  Everything declared in `src/<donor>/*.h`
that is not also declared in `g_local.h` or defined in a spine file.  A helper
that moved into the shared tree on purpose -- `stuffcmd` did -- stops being the
donor's and stops being reported, automatically.

WHAT COUNTS AS A GATE.  `G_Ruleset() == RULESET_<DONOR>` on the occurrence's own
line, on the line that opens an enclosing block, or as the whole condition of a
braceless `if` immediately above.  A named predicate that is only true for one
ruleset counts too, by being listed in PREDICATES -- `G_IsObserver()` is not one
of those, deliberately: it is true under three.

WHAT IS NOT A FINDING.  Declarations (`g_local.h`), the savegame descriptor
tables and the dispatch tables: those are data naming a field, not behaviour
reading one.  Comments and string literals are stripped first, without which
this reports every prose mention of `teams`.

USAGE
    tools/donorgate.py [--tree src]
    tools/donorgate.py --selftest
"""
import argparse
import glob
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# donor subdirectory -> the ruleset whose gate its surface needs.
#
# A DONOR IS NOT A RULESET, and since the flattening it is not even one-to-one.
# `src/tourney/` is reachable from FOUR rulesets -- dm, dmpro, tdm, duel, which
# are OSP's four modes of play promoted to first-class values (R-OSP-12) -- so
# the gate its surface needs is the family predicate, not a single equality.
# The donor identity is the stable half of that and is what this map is keyed
# on; how many rulesets dispatch into a donor is the half that moved.
DONORS = {
    'arena': 'RULESET_ARENA',
    'tourney': 'RULESET_OSP',
}

# Tests that are true for exactly one ruleset -- or, for RULESET_OSP, for
# exactly one donor's four -- and therefore gate as well as the explicit one
# does.  Kept short on purpose.  `menu_owner == MENU_ARENA` is the strongest of
# them: only arena's engine ever sets that owner, and R-MENU-2a makes the field
# the single answer to "whose menu is open".
#
# G_IsOspRuleset() is the whole gate for tourney's surface, not a shortcut for
# one: there is no `RULESET_OSP` enum value, and a site that named all four by
# hand would be a list that the next ruleset gets left off.  OSP_IsMatch() and
# OSP_IsTeams() are strictly narrower -- each is true only under rulesets
# G_IsOspRuleset() is also true under -- so they gate too.
PREDICATES = {
    'RULESET_ARENA': ('MENU_ARENA',),
    'RULESET_OSP': ('MENU_TOURNEY', 'G_IsOspRuleset', 'OSP_IsMatch',
                    'OSP_IsTeams', 'RULESET_DM', 'RULESET_DMPRO',
                    'RULESET_TDM', 'RULESET_DUEL'),
}

# Fields that are the donor's but are declared in g_local.h, so the "declared in
# the donor's header" rule cannot find them.  One line per donor.
FIELDS = {
    'RULESET_ARENA': (
        'fightstate', 'teamnum', 'context', 'omode', 'lastomode',
        'omode_buttons', 'track_target', 'spawn_recheck', 'damagedealt',
        'zbotcount', 'zbotlastcheck', 'zbotscore', 'isbot', 'ra_voted',
        'ra_votes', 'scoremode', 'oldangles', 'spamcount', 'spamtime',
        'menuqueue', 'curmenulink', 'ra_menutime', 'menuusetime', 'menutext',
        'showmotd', 'teammember',
    ),
    'RULESET_TOURNEY': (),
}

# Files whose mention of a donor name is data or declaration, not behaviour.
EXEMPT_FILES = ('g_local.h', 'g_ptrs.c', 'g_ptrs.h')

# Line-level exemptions inside otherwise-live files: a descriptor row names a
# field so that the savegame writer can find it; it does not read it.
DESCRIPTOR = re.compile(r'^\s*[A-Z]\((?:resp\.|pers\.)?\w+\),\s*$')

# ...and a DESIGNATED INITIALISER row names a function so that a table can hold
# it: `.pickup = OSP_Pickup_Rune,` in itemlist[], `.CheckRules = OSP_CheckRules,`
# in a ruleset_ops_t.  The docstring has always said dispatch tables are data
# rather than behaviour; before the runes landed, DESCRIPTOR was the only line
# shape that said so, because CTF's techs are the one other case and CTF is not
# in DONORS.  A row like this is reached only through whatever spawns or invokes
# the table entry, and THAT is where the gate belongs -- for an item, the gate
# is whether the item is in the world at all (R-CORE-2, R-OSP-6).
INITIALISER = re.compile(r'^\s*\.\w+\s*=\s*[A-Za-z_]\w*,\s*$')

# Top-level only -- column 0.  Without that anchor this collects every STRUCT
# MEMBER in the donor's typedefs, and arena.h has members called `value`,
# `name` and `count`, which then match half the tree.
DECL = re.compile(r'^(?:extern\s+)?[A-Za-z_][\w \t*]*?\b([A-Za-z_]\w*)\s*[(;\[]', re.M)


def strip(t):
    t = re.sub(r'/\*.*?\*/', lambda m: re.sub(r'[^\n]', ' ', m.group(0)), t, flags=re.S)
    t = re.sub(r'//[^\n]*', '', t)
    return re.sub(r'"(\\.|[^"\\\n])*"', '""', t)


def read(p):
    return open(p, encoding='utf-8').read()


def donor_surface(tree, donor):
    """Names the donor's headers declare and the shared tree does not own."""
    names = set()
    for h in sorted(glob.glob(os.path.join(tree, donor, '*.h'))):
        for m in DECL.finditer(strip(read(h))):
            if len(m.group(1)) >= 4:
                names.add(m.group(1))
    # Anything g_local.h also declares, or a spine .c defines, is shared.  So is
    # every MEMBER of a struct g_local.h defines: `max_health` is baseq2's field
    # on edict_t, and a donor header that names it in a prototype does not make
    # it the donor's.  Without this the tool reports 111 findings, all of them
    # baseq2 reading its own fields.
    shared = set()
    local = strip(read(os.path.join(tree, 'g_local.h')))
    for m in DECL.finditer(local):
        shared.add(m.group(1))
    for m in re.finditer(r'^\s+[A-Za-z_][\w \t*]*?\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*;',
                         local, re.M):
        shared.add(m.group(1))
    for c in sorted(glob.glob(os.path.join(tree, '*.c'))):
        body = strip(read(c))
        # functions the spine defines
        for m in re.finditer(r'^[A-Za-z_][\w \t*]*?\b([A-Za-z_]\w*)\s*\([^;]*$', body, re.M):
            shared.add(m.group(1))
        # and OBJECTS it defines.  baseq2's three `static const gitem_armor_t`
        # tables are the case: tourney declares them extern and non-const
        # because it rewrites them from cvars, which makes them a merge decision
        # in g_items.c -- not a donor-private symbol leaking into the spine.
        for m in re.finditer(r'^(?:static\s+)?(?:const\s+)?[A-Za-z_][\w \t*]*?'
                             r'\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*=', body, re.M):
            shared.add(m.group(1))
        # ...including TENTATIVE definitions, which have no initialiser.  Once a
        # spine file defines `int match_paused;` the symbol is the tree's, and
        # the line that defines it is not a read under the wrong ruleset.
        for m in re.finditer(r'^(?:static\s+)?(?:const\s+)?[A-Za-z_][\w \t*]+'
                             r'\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*;', body, re.M):
            shared.add(m.group(1))
    return names - shared


def group(lines):
    """line index -> (first, last) of the statement it belongs to.

    Grouped FORWARD by parenthesis balance rather than guessed backwards: a
    condition wrapped over three lines is one test, and the ruleset half of it is
    usually on the first of them, so a per-line check reports the second and
    third as ungated.  Backward guessing gets the common shapes and misses a
    ternary whose branches each balance on their own -- which is exactly the
    shape p_view.c's falling damage has.
    """
    out, i, n = {}, 0, len(lines)
    while i < n:
        bal, j = 0, i
        while j < n:
            bal += lines[j].count('(') - lines[j].count(')')
            if bal <= 0:
                break
            j += 1
        for k in range(i, min(j, n - 1) + 1):
            out[k] = (i, min(j, n - 1))
        i = min(j, n - 1) + 1
    return out


COND = re.compile(r'^\s*(?:\}\s*)?(?:else\s+)?if\s*\(.*\)\s*$')
CASE = re.compile(r'^\s*(?:case\s+\w+|default)\s*:')

# `} else if (G_Ruleset() == RULESET_TOURNEY) {` opens the block its body is in,
# but it closes one too, so the outward walk's brace arithmetic nets to zero and
# sails straight past it -- the gate is right there on the line and the tool
# reports the body as ungated.  An else-arm opener is recognised explicitly.
ELSEARM = re.compile(r'^\s*\}\s*else\b.*\{\s*$')


def gate_lines(lines, groups, ruleset, i):
    """True if line i is inside a gate for `ruleset`."""
    # RULESET_OSP is the family, not an enum value, so it contributes no
    # literal name of its own -- everything that gates it is in PREDICATES.
    pats = [] if ruleset == 'RULESET_OSP' else \
        [r'RULESET_' + ruleset.split('_', 1)[1]]
    for p in PREDICATES.get(ruleset, ()):
        pats.append(re.escape(p))
    rx = re.compile('|'.join(pats))

    def stmt(k):
        a, b = groups.get(k, (k, k))
        return ' '.join(lines[a:b + 1])

    start = groups.get(i, (i, i))[0]
    if rx.search(stmt(i)):
        return True

    # A braceless `if (...)` above this statement, skipping blank lines and the
    # comment lines strip() has emptied.
    k = start - 1
    while k >= 0 and not lines[k].strip():
        k -= 1
    if k >= 0:
        cond = stmt(k)
        if COND.match(cond.strip()) and rx.search(cond):
            return True

    # Walk outward through enclosing blocks, and check the `case` arm we are in.
    depth, seen_case = 0, False
    j = start - 1
    while j >= 0:
        line = lines[j]
        if depth == 0 and ELSEARM.match(line) and rx.search(stmt(j)):
            return True
        if depth == 0 and not seen_case and CASE.match(line):
            seen_case = True
            if rx.search(line):
                return True
        depth += line.count('}') - line.count('{')
        if depth < 0:
            if rx.search(stmt(j)):
                return True
            depth, seen_case = 0, False
        j -= 1
    return False


def run(tree, override=None):
    out, bad = [], 0
    checked = 0
    for donor, ruleset in DONORS.items():
        if not os.path.isdir(os.path.join(tree, donor)):
            continue
        surface = donor_surface(tree, donor) | set(FIELDS.get(ruleset, ()))
        rx = re.compile(r'\b(%s)\b' % '|'.join(sorted(re.escape(s) for s in surface)))
        for f in sorted(glob.glob(os.path.join(tree, '*.c')) +
                        glob.glob(os.path.join(tree, '*.h'))):
            name = os.path.basename(f)
            if name in EXEMPT_FILES:
                continue
            raw = (override or {}).get(name) or read(f)
            lines = strip(raw).split('\n')
            groups = group(lines)
            for i, line in enumerate(lines):
                m = rx.search(line)
                if not m or DESCRIPTOR.match(line) or INITIALISER.match(line):
                    continue
                checked += 1
                if not gate_lines(lines, groups, ruleset, i):
                    out.append('  !! %s:%d: `%s` is %s\'s and is not inside a '
                               '%s gate: %s'
                               % (name, i + 1, m.group(1), donor, ruleset,
                                  line.strip()[:60]))
                    bad += 1
    # R-OSP-5's shape, which is not a gate question but is the same INPUT: a
    # donor's own object re-declared `extern` somewhere other than the header
    # that defines it.  The bug it is named for is `extern int botglobals;` in
    # the donor's g_spawn.c against a `bot_globals_t botglobals;` elsewhere --
    # the linker resolved a four-byte int over the first member of a struct and
    # zeroed numbits, and nothing warned.  A local extern of a donor object is
    # never right here: every one of them has a header.
    redecl = 0
    for donor in DONORS:
        if not os.path.isdir(os.path.join(tree, donor)):
            continue
        objs = set()
        for h in sorted(glob.glob(os.path.join(tree, donor, '*.h'))):
            for m in re.finditer(r'^extern\s+[A-Za-z_][\w \t*]*?\b(\w+)\s*[;\[]',
                                 strip(read(h)), re.M):
                objs.add(m.group(1))
        if not objs:
            continue
        for f in sorted(glob.glob(os.path.join(tree, '*.c')) +
                        glob.glob(os.path.join(tree, '*.h')) +
                        glob.glob(os.path.join(tree, donor, '*.c'))):
            name = os.path.relpath(f, tree)
            raw = (override or {}).get(os.path.basename(f)) or read(f)
            for i, line in enumerate(strip(raw).split('\n')):
                m = re.match(r'^extern\s+[A-Za-z_][\w \t*]*?\b(\w+)\s*[;\[]', line)
                if m and m.group(1) in objs:
                    out.append('  !! %s:%d: `%s` is %s\'s and is re-declared '
                               'extern outside its own header (R-OSP-5): %s'
                               % (name, i + 1, m.group(1), donor,
                                  line.strip()[:60]))
                    redecl += 1
    bad += redecl

    out.insert(0, 'donorgate.py: %d donor-surface use(s) in spine files; '
                  '%d ungated; %d stray extern(s)' % (checked, bad - redecl, redecl))
    if not bad:
        out.append('  every donor-private field and function in a shared file '
                   'is inside its own ruleset\'s gate')
    return out


# R-VER-9 clause 2.  Each control removes a real gate this merge added.
SELFTESTS = [
    ('landing sound', 'g_phys.c',
     ('if (G_Ruleset() != RULESET_ARENA || !ent->client ||\n'
      '            ent->client->resp.fightstate == FIGHT_ALIVE)',
      'if (!ent->client || ent->client->resp.fightstate == FIGHT_ALIVE)')),
    ('killbox', 'g_utils.c',
     ('    if (G_Ruleset() == RULESET_ARENA) {\n'
      '        // RA2 telefrags only between fighting players.',
      '    if (1) {\n'
      '        // RA2 telefrags only between fighting players.')),
    # R-OSP-5's own bug, in its shape: a local `extern` of a donor object where
    # the donor's own header already declares it.  `m_mode` is tourney's match
    # mode and an `extern int` of it in a shared file is exactly what
    # `extern int botglobals;` was.
    ('stray extern', 'g_spawn.c',
     ('#include "tourney/osp_hooks.h"',
      '#include "tourney/osp_hooks.h"\nextern int sync_stat;')),
    # An else-arm gate is a gate.  Removing the ruleset test from the `} else
    # if (...) {` that opens the block must be reported -- before this control
    # existed the tool could not see that line at all.
    ('else-arm gate', 'g_items.c',
     ('} else if (G_IsOspRuleset()) {',
      '} else if (master->item) {')),
    # The initialiser exemption must not swallow a real call.  `.pickup =`
    # rows are data; `OSP_Pickup_Rune(ent, other);` in a function body is not,
    # and turning one into the other has to be reported.
    ('rune pickup call', 'g_items.c',
     ('    taken = ent->item->pickup(ent, other);',
      '    taken = OSP_Pickup_Rune(ent, other);')),
    ('weapon think', 'p_weapon.c',
     ('if (G_Ruleset() == RULESET_ARENA && ent->client &&\n'
      '        ent->client->resp.fightstate != FIGHT_ALIVE)',
      'if (ent->client &&\n'
      '        ent->client->resp.fightstate != FIGHT_ALIVE)')),
]


def selftest(tree):
    bad = 0
    for name, fname, (orig, rev) in SELFTESTS:
        text = read(os.path.join(tree, fname))
        if orig not in text:
            print('  !! control "%s": its own mutation no longer applies -- '
                  'the control has rotted' % name)
            bad += 1
            continue
        lines = run(tree, {fname: text.replace(orig, rev, 1)})
        if any('!!' in l and fname in l for l in lines):
            print('  ok  control "%s" fires: an ungated use in %s' % (name, fname))
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

    lines = run(tree)
    for l in lines:
        print(l)
    return 1 if any('!!' in l for l in lines) else 0


if __name__ == '__main__':
    sys.exit(main())
