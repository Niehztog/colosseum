#!/usr/bin/env python3
"""Audit how ruleset behaviour is gated (SPECS.md §9 Phase 1 exit, R-MODE-5).

Phase 1's exit criterion is: "Switching `g_ruleset` between `dm` and `sp` changes
behaviour through the dispatch and through no `if (ctf->value)` anywhere."  The
second half is a prohibition, and a prohibition nobody checks is a style note.
This is the check.

WHAT IS FORBIDDEN

A *ruleset* cvar tested at a call site.  The 1999 build gated on `ctf->value`,
`rocketarena->value` and `ch->value` scattered through the tree, which is what
forced InitGame to police the combinations with gi.error.  R-MODE-5 replaces that
with one gate per concept: a dispatch row where a ruleset replaces behaviour, a
named predicate where it only adjusts it.  So outside g_ruleset.c, no file may
test a ruleset cvar at all.

WHAT IS NOT FORBIDDEN, and why the distinction matters

`deathmatch` and `coop` are *not* ruleset cvars.  They are engine-visible state
-- the engine registers them, forces `deathmatch` on a dedicated server, and
gates savegames on it -- and R-MODE-2 keeps them working as legacy aliases.
baseq2 tests them in 138 places and Colosseum inherits every one under R-CORE-5.
Converting them wholesale would be a large diff against the pin for no
behavioural gain.

But the count is worth watching, because each one is a place where a *ruleset*
question might be being asked through a proxy.  So they are reported as a
census, not a failure: a number that should fall as phases convert the sites that
turn out to be ruleset questions, and that must not silently rise.

USAGE
    tools/gates.py [--tree src] [--budget N]
"""
import argparse
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Cvars that name a ruleset.  Testing one of these at a call site is the defect.
RULESET_CVARS = ('ctf', 'rocketarena', 'ra', 'ch', 'arena', 'tourney')

# Where the ruleset cvars are legitimately read: resolution, once, at InitGame.
OWNER = 'g_ruleset.c'

# `->value` / `->integer` on a ruleset cvar, or a bare `cvarname->` deref.
RULESET_TEST = re.compile(
    r'\b(' + '|'.join(RULESET_CVARS) + r')\s*->\s*(value|integer|string)\b')

# The legacy pair: allowed, counted.
LEGACY_TEST = re.compile(r'\b(deathmatch|coop)\s*->\s*(value|integer)\b')

# The specific idiom Phase 1 converted, kept as a regression check: if it comes
# back, monsters silently stop spawning under ctf (R-MODE-7, reconciliation R-6).
MONSTER_IDIOM = re.compile(
    r'if\s*\(\s*deathmatch\s*->\s*(value|integer)\s*\)\s*\{\s*'
    r'G_FreeEdict\s*\(\s*self\s*\)\s*;', re.S)

# The idiom's *shape* -- "free self when deathmatch" -- is not unique to
# monsters.  baseq2 uses it for content it removes from deathmatch on its own
# terms, and says so in a comment at each site: `func_explosive` and
# `misc_explobox` are both marked "auto-remove for deathmatch", and
# `target_lightramp` is an SP presentation feature.  Those are genuine
# *deathmatch* decisions and CTF has always inherited them, so they stay.
#
# So the check keys on monster CONTEXT rather than carrying an exception list.
# An allowlist would have to grow by hand with every donor that adds a prop;
# this rule identifies new monsters correctly without being told about them.
MONSTER_INFRA = {
    'SP_point_combat',      # a combat point monsters are sent to
    'monster_start',        # the gate every SP_monster_* funnels through
}


def is_monster_context(filename, fn):
    return (filename.startswith('m_')
            or 'monster' in fn.lower()
            or fn in MONSTER_INFRA
            or filename == 'g_turret.c')


def enclosing_function(text, pos):
    """Name of the function a match sits in -- nearest `name(` at column 0."""
    best = ''
    for m in re.finditer(r'^[A-Za-z_][\w \t\*]*?\b(\w+)\s*\(', text[:pos],
                         re.M):
        best = m.group(1)
    return best


def strip_comments(t):
    """Blank comments but PRESERVE newlines, so reported line numbers are real.

    Deleting a block comment outright shifts every line number after it, which
    is how the first version of this tool reported five monster-idiom hits at
    five locations that had nothing to do with monsters.
    """
    def blank(m):
        return '\n' * m.group(0).count('\n')
    t = re.sub(r'/\*.*?\*/', blank, t, flags=re.S)
    return re.sub(r'//[^\n]*', '', t)


def sources(tree):
    out = []
    for root, dirs, files in os.walk(tree):
        dirs[:] = [d for d in dirs if not d.startswith(('debug', 'release'))]
        for f in sorted(files):
            if f.endswith(('.c', '.h')):
                out.append(os.path.join(root, f))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tree', default=os.path.join(REPO, 'src'))
    ap.add_argument('--budget', type=int, default=110,   # measured at the end of Phase 1; a ratchet, not a target
                    help='ceiling on inherited deathmatch/coop test sites')
    a = ap.parse_args()

    violations, legacy, idiom = [], {}, []

    for path in sources(os.path.abspath(a.tree)):
        name = os.path.basename(path)
        raw = open(path, encoding='latin-1').read()
        text = strip_comments(raw)

        if name != OWNER:
            for m in RULESET_TEST.finditer(text):
                line = text[:m.start()].count('\n') + 1
                violations.append((name, line, m.group(0)))

        # The owner reads the legacy cvars on purpose: resolution has to compare
        # them against the ruleset, and the `sv ruleset` diagnostic reports
        # them.  Counting those against the census would make the budget rise
        # every time the dispatch got better at its job.
        if name != OWNER:
            n = len(LEGACY_TEST.findall(text))
            if n:
                legacy[name] = n

        for m in MONSTER_IDIOM.finditer(text):
            fn = enclosing_function(text, m.start())
            if not is_monster_context(name, fn):
                continue        # baseq2's own deathmatch content rules
            idiom.append((name, text[:m.start()].count('\n') + 1, fn))

    total = sum(legacy.values())
    print(f'gates.py: {len(violations)} forbidden ruleset-cvar test(s); '
          f'{total} inherited deathmatch/coop site(s) '
          f'across {len(legacy)} file(s)')

    for name, line, snippet in violations:
        print(f'  !! {name}:{line}: `{snippet}` -- a ruleset cvar tested at a '
              f'call site (R-MODE-5). Use a predicate or a dispatch row.')

    for name, line, fn in idiom:
        print(f'  !! {name}:{line} ({fn}): the monster-suppression idiom is '
              f'back. `deathmatch` is 1 under ctf, so this suppresses the '
              f'monsters R-MODE-7 promises there. Use !G_MonstersAllowed().')

    if total > a.budget:
        print(f'  !! inherited legacy sites rose above the budget of '
              f'{a.budget}: new code must ask a named question, not test '
              f'`deathmatch`')

    ok = not violations and not idiom and total <= a.budget
    if ok:
        print('  every ruleset decision goes through the dispatch or a '
              'predicate')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
