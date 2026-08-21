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


# R-KEY-1 / R-VER-14, mechanised.  `spawn_temp_t.pausetime` is the member behind
# the `"pausetime"` map key, and Q2PRO's frame-number conversion renamed it to
# `pause_framenum` when replayed tree-wide.  BOTH donor branches still carry that
# rename, so every merge that touches g_func.c or g_spawn.c brings it back --
# it has arrived three times in Phase 2 alone, from the g_local.h merge, the
# xatrix merge and the rogue merge.
#
# Vigilance is clearly not working, so it is a check.  `monsterinfo.pause_framenum`
# is the OTHER member and is correct; only the spawn_temp_t one is frozen.
FROZEN_KEYS = {
    # key text that must appear -> the member it must resolve to
    'pausetime': 'pausetime',
}
STALE_SPAWNTEMP = re.compile(r'\bst\s*\.\s*pause_framenum\b')


def frozen_key_violations(name, text):
    out = []
    for m in STALE_SPAWNTEMP.finditer(text):
        out.append((name, text[:m.start()].count('\n') + 1,
                    'st.pause_framenum -- the spawn_temp_t member is `pausetime`; '
                    'the key text is frozen (R-KEY-1)'))
    if name == 'g_spawn.c':
        for key, member in FROZEN_KEYS.items():
            if f'"{key}"' not in text:
                out.append((name, 0, f'the frozen map key "{key}" is missing from '
                                     f'the spawn tables (R-KEY-1)'))
            bad = re.search(r'\{\s*"%s"\s*,\s*STOFS\((\w+)\)' % key, text)
            if bad and bad.group(1) != member:
                out.append((name, text[:bad.start()].count('\n') + 1,
                            f'key "{key}" resolves to {bad.group(1)}, must be '
                            f'{member} (R-KEY-1)'))
    return out


# R-CORE-11 / R-CORE-11b, mechanised.  Where baseq2's version of a monster table
# or evasion function is kept alongside Ground Zero's under a `bq2_` prefix, the
# gate is only real if EVERY assignment site chooses between them.  One
# ungated site silently pins that animation to Ground Zero's version whatever
# the content layer says -- and nothing at build or run time would show it.
#
# Also checks the shape: the gate must be an if/else with two literal
# assignments, because genptr.py builds save_ptrs[] by scanning source text
# (R-CORE-11b).  A ternary here compiles, runs, plays, and then fails to reload
# a savegame.
# The pairing signal must come from the DEFINITION, not from a `&bq2_X` use:
# if it came from the use, deleting a gate would delete the evidence that the
# table was ever paired, and the check would fall silent exactly when it
# mattered.  Learned the hard way -- the first version of this check passed
# its own negative control.
GATED = re.compile(r'const\s+mmove_t\s+bq2_(\w+)\s*=')
ASSIGN = re.compile(r'self->monsterinfo\.currentmove\s*=\s*&(\w+);')
TERNARY = re.compile(r'currentmove\s*=\s*[^;\n]*\?[^;\n]*&')


def gate_violations(name, text):
    out = []
    for m in TERNARY.finditer(text):
        out.append((name, text[:m.start()].count('\n') + 1,
                    'currentmove assigned through a ternary -- genptr.py cannot '
                    'see the table, so the savegame will not reload '
                    '(R-CORE-11b)'))
    paired = set(GATED.findall(text))
    if not paired:
        return out
    for m in ASSIGN.finditer(text):
        tbl = m.group(1)
        if tbl.startswith('bq2_') or tbl not in paired:
            continue
        # A site inside a FLAVOUR-SPECIFIC function needs no gate: reaching it
        # already means that flavour's evasion was installed.  Ground Zero's
        # X_duck/X_sidestep/X_blocked are installed only under CONTENT_ROGUE,
        # and every bq2_* function only when it is absent.  Only a function
        # reachable under both flavours -- X_attack, X_pain, X_run -- needs one.
        fn = enclosing_function(text, m.start())
        if fn.startswith('bq2_') or fn.endswith(('_duck', '_sidestep',
                                                 '_blocked', '_duck_up')):
            continue
        # a gated site has the bq2_ alternative within a few lines
        seg = text[m.end():m.end() + 240]
        if f'&bq2_{tbl};' not in seg:
            before = text[max(0, m.start() - 240):m.start()]
            if f'&bq2_{tbl};' not in before:
                out.append((name, text[:m.start()].count('\n') + 1,
                            f'`{tbl}` has a bq2_ counterpart but this assignment '
                            f'is ungated -- R-CORE-11 requires the latch to '
                            f'select at every site'))
    return out


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
    # The ratchet.  It moves only with a recorded reason, and only for genuinely
    # inherited sites.  History:
    #   110  end of Phase 1 (baseq2, after 27 monster-gate conversions)
    #   115  Phase 2, xatrix merged: +5, each checked individually and each a
    #        real deathmatch rule rather than a ruleset question in disguise --
    #        g_items.c's `deathmatch && DF_NO_HEALTH`, two weapon behaviours,
    #        and one `coop || deathmatch` spawn test.
    #   174  Phase 2, rogue merged: +59 across 12 files.  Ground Zero is 12,263
    #        diff lines and carries its own deathmatch rules throughout -- the
    #        DM ball and tag rulesets, the sphere and nuke DM behaviours, the
    #        no-armour/no-items dmflags.  The five monster gates it brought
    #        (kamikaze, carrier, stalker, turret, widow) were converted, which is
    #        the check that matters; the rest are inherited deathmatch rules.
    ap.add_argument('--budget', type=int, default=166,
                    help='ceiling on inherited deathmatch/coop test sites')
    a = ap.parse_args()

    violations, legacy, idiom, frozen = [], {}, [], []

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

        for v in frozen_key_violations(name, text):
            frozen.append(v)

        for v in gate_violations(name, text):
            frozen.append(v)

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

    for name, line, msg in frozen:
        where = f'{name}:{line}' if line else name
        print(f'  !! {where}: {msg}')

    for name, line, fn in idiom:
        print(f'  !! {name}:{line} ({fn}): the monster-suppression idiom is '
              f'back. `deathmatch` is 1 under ctf, so this suppresses the '
              f'monsters R-MODE-7 promises there. Use !G_MonstersAllowed().')

    if total > a.budget:
        print(f'  !! inherited legacy sites rose above the budget of '
              f'{a.budget}: new code must ask a named question, not test '
              f'`deathmatch`')

    ok = not violations and not idiom and not frozen and total <= a.budget
    if ok:
        print('  every ruleset decision goes through the dispatch or a '
              'predicate')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
