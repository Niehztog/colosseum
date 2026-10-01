#!/usr/bin/env python3
"""Audit how ruleset behaviour is gated.

The criterion is: "Switching `g_ruleset` between `dm` and `sp` changes
behaviour through the dispatch and through no `if (ctf->value)` anywhere."  The
second half is a prohibition, and a prohibition nobody checks is a style note.
This is the check.

WHAT IS FORBIDDEN

A *ruleset* cvar tested at a call site.  The 1999 build gated on `ctf->value`,
`rocketarena->value` and `ch->value` scattered through the tree, which is what
forced InitGame to police the combinations with gi.error.  This tree replaces that
with one gate per concept: a dispatch row where a ruleset replaces behaviour, a
named predicate where it only adjusts it.  So outside g_ruleset.c, no file may
test a ruleset cvar at all.

WHAT IS NOT FORBIDDEN, and why the distinction matters

`deathmatch` and `coop` are *not* ruleset cvars.  They are engine-visible state
-- the engine registers them, forces `deathmatch` on a dedicated server, and
gates savegames on it -- and they keep working as legacy aliases.
baseq2 tests them in 138 places and Colosseum inherits every one.
Converting them wholesale would be a large diff against the pin for no
behavioural gain.

But the count is worth watching, because each one is a place where a *ruleset*
question might be being asked through a proxy.  So they are reported as a
census, not a failure: a number that should fall as phases convert the sites that
turn out to be ruleset questions, and that must not silently rise.

HOW EACH HALF IS CHECKED, and what it used to be

The census is a ceiling PER FILE (LEGACY_BUDGET), not a total: a total let a
`deathmatch->value` test deleted in one file pay for a new one in another.
The `bq2_` gate is read as the if/else it has to be -- a latch condition, one
arm assigning the table and the other its `bq2_` twin -- not as "the twin is
named within 240 characters", which an ungated site beside a gated one passed.
`--selftest` holds a control for every rule.

USAGE
    tools/gates.py [--tree src]
    tools/gates.py --selftest
"""
import argparse
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Cvars that name a ruleset.  Testing one of these at a call site is the defect.
#
# `ch` and `tourney` left in spec 1.36 with the things they named: Colored
# Hitman's cvar is no longer read at all and `tourney` is no
# longer a ruleset -- OSP's four modes of play are `dm`, `dmpro`, `tdm` and
# `duel` now.  Neither is listed defensively, because a name here
# that nothing can define is a check that cannot fire, and this list is meant
# to be short enough to read.
RULESET_CVARS = ('ctf', 'rocketarena', 'ra', 'arena')

# Where the ruleset cvars are legitimately read: resolution, once, at InitGame.
OWNER = 'g_ruleset.c'

# `->value` / `->integer` on a ruleset cvar, or a bare `cvarname->` deref.
RULESET_TEST = re.compile(
    r'\b(' + '|'.join(RULESET_CVARS) + r')\s*->\s*(value|integer|string)\b')

# The ratchet, per file (path under src/).  It moves only with a recorded
# reason, and only for genuinely inherited sites.  History of the total:
#   110  baseq2 alone, after 27 monster-gate conversions
#   115  xatrix merged: +5, each checked individually and each a
#        real deathmatch rule rather than a ruleset question in disguise --
#        g_items.c's `deathmatch && DF_NO_HEALTH`, two weapon behaviours,
#        and one `coop || deathmatch` spawn test.
#   174  rogue merged: +59 across 12 files.  Ground Zero is 12,263
#        diff lines and carries its own deathmatch rules throughout -- the
#        DM ball and tag rulesets, the sphere and nuke DM behaviours, the
#        no-armour/no-items dmflags.  The five monster gates it brought
#        (kamikaze, carrier, stalker, turret, widow) were converted, which is
#        the check that matters; the rest are inherited deathmatch rules.
#   166  where it stands, split by file so a site cannot move unseen.
LEGACY_BUDGET = {
    'rogue/dm_ball.c': 6, 'rogue/dm_tag.c': 1, 'g_ai.c': 4, 'g_cmds.c': 10,
    'g_combat.c': 4, 'g_func.c': 5, 'g_items.c': 29, 'g_main.c': 2,
    'g_misc.c': 4, 'rogue/g_newai.c': 2, 'rogue/g_newtarg.c': 2,
    'rogue/g_newweap.c': 6, 'g_spawn.c': 5, 'g_target.c': 7,
    'g_trigger.c': 1, 'g_weapon.c': 4, 'rogue/m_carrier.c': 2,
    'm_medic.c': 1, 'rogue/m_widow.c': 4, 'rogue/m_widow2.c': 3,
    'p_client.c': 32, 'p_hud.c': 10, 'p_trail.c': 1, 'p_view.c': 1,
    'p_weapon.c': 19, 'tourney/stdlog.c': 1,
}

# The legacy pair: allowed, counted.
LEGACY_TEST = re.compile(r'\b(deathmatch|coop)\s*->\s*(value|integer)\b')

# The specific idiom that was converted, kept as a regression check: if it
# comes back, monsters silently stop spawning under ctf.
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


# The frozen key, mechanised.  `spawn_temp_t.pausetime` is the member behind
# the `"pausetime"` map key, and Q2PRO's frame-number conversion renamed it to
# `pause_framenum` when replayed tree-wide.  BOTH donor branches still carry that
# rename, so every merge that touches g_func.c or g_spawn.c brings it back --
# it arrived three times from the mission-pack merges alone: the g_local.h merge, the
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
                    'the key text is frozen'))
    if name == 'g_spawn.c':
        for key, member in FROZEN_KEYS.items():
            if f'"{key}"' not in text:
                out.append((name, 0, f'the frozen map key "{key}" is missing from '
                                     f'the spawn tables'))
            bad = re.search(r'\{\s*"%s"\s*,\s*STOFS\((\w+)\)' % key, text)
            if bad and bad.group(1) != member:
                out.append((name, text[:bad.start()].count('\n') + 1,
                            f'key "{key}" resolves to {bad.group(1)}, must be '
                            f'{member}'))
    return out


# The content latch, mechanised.  Where baseq2's version of a monster table
# or evasion function is kept alongside Ground Zero's under a `bq2_` prefix, the
# gate is only real if EVERY assignment site chooses between them.  One
# ungated site silently pins that animation to Ground Zero's version whatever
# the content layer says -- and nothing at build or run time would show it.
#
# Also checks the shape: the gate must be an if/else with two literal
# assignments, because genptr.py builds save_ptrs[] by scanning source text
#.  A ternary here compiles, runs, plays, and then fails to reload
# a savegame.
# The pairing signal must come from the DEFINITION, not from a `&bq2_X` use:
# if it came from the use, deleting a gate would delete the evidence that the
# table was ever paired, and the check would fall silent exactly when it
# mattered.  Learned the hard way -- the first version of this check passed
# its own negative control.
GATED = re.compile(r'const\s+mmove_t\s+bq2_(\w+)\s*=')
ASSIGN = re.compile(r'self->monsterinfo\.currentmove\s*=\s*&(\w+);')
TERNARY = re.compile(r'currentmove\s*=\s*[^;\n]*\?[^;\n]*&')


# `if (<latch>) <arm> else <arm>`, an arm being one statement or one braced
# block with no block inside it -- the shape every gated site has.  The latch
# is the entity's own flavour or the predicate over it.
# The medic asks its own predicate over the same latch (medic_UsesRogueBehavior).
LATCH = re.compile(r'content_flavour\s*&\s*CONTENT_ROGUE|'
                   r'\b\w*_UsesRogueBehavior\s*\(')
_COND = r'\((?:[^;{}()]|\([^;{}()]*(?:\([^;{}()]*\)[^;{}()]*)*\))*\)'
_ARM = r'(?:\{[^{}]*\}|[^;{}]*;)'
# The other arm may be an else-if chain -- `if (ROGUE) a; else if (XATRIX)
# xatrix_a; else bq2_a;` is the three-way form, and bq2_ is its last arm.
IFELSE = re.compile(r'\bif\s*(?P<cond>' + _COND + r')\s*(?P<a>' + _ARM +
                    r')\s*else\s*(?P<b>(?:if\s*' + _COND + r'\s*' + _ARM +
                    r'\s*else\s*)*' + _ARM + r')')


def gated_spans(text, tbl):
    """Spans of every arm that assigns `tbl` inside a latch if/else whose
    other arm assigns `bq2_<tbl>`."""
    spans = []
    # Tried at EVERY `if`, not left to finditer: an outer if/else whose arms
    # hold the latch's own -- `if (range <= 125) {...} else {...}`, an `else
    # if` chain -- would otherwise be consumed first and the inner one never
    # tried on its own.
    for start in re.finditer(r'\bif\s*\(', text):
        m = IFELSE.match(text, start.start())
        if not m or not LATCH.search(m.group('cond')):
            continue
        for mine, other in (('a', 'b'), ('b', 'a')):
            if (re.search(r'=\s*&%s\s*;' % re.escape(tbl), m.group(mine)) and
                    re.search(r'=\s*&bq2_%s\s*;' % re.escape(tbl),
                              m.group(other))):
                spans.append((m.start(mine), m.end(mine)))
    return spans


def gate_violations(name, text):
    out = []
    for m in TERNARY.finditer(text):
        out.append((name, text[:m.start()].count('\n') + 1,
                    'currentmove assigned through a ternary -- genptr.py cannot '
                    'see the table, so the savegame will not reload '
                    ''))
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
        # a gated site is an arm of the latch's if/else, and the other arm
        # assigns the bq2_ twin
        if not any(a <= m.start() < b for a, b in gated_spans(text, tbl)):
            out.append((name, text[:m.start()].count('\n') + 1,
                        f'`{tbl}` has a bq2_ counterpart but this assignment '
                        f'is not an arm of a latch if/else whose other arm '
                        f'assigns bq2_{tbl} -- the latch has to select at '
                        f'every site'))
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


def read_tree(tree):
    tree = os.path.abspath(tree)
    return {os.path.relpath(p, tree).replace(os.sep, '/'):
            open(p, encoding='latin-1').read() for p in sources(tree)}


def analyse(texts):
    """{relpath: source} -> (violations, legacy, idiom, frozen)."""
    violations, legacy, idiom, frozen = [], {}, [], []

    for rel in sorted(texts):
        name = os.path.basename(rel)
        text = strip_comments(texts[rel])

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
                legacy[rel] = n

        for v in frozen_key_violations(name, text):
            frozen.append(v)

        for v in gate_violations(name, text):
            frozen.append(v)

        for m in MONSTER_IDIOM.finditer(text):
            fn = enclosing_function(text, m.start())
            if not is_monster_context(name, fn):
                continue        # baseq2's own deathmatch content rules
            idiom.append((name, text[:m.start()].count('\n') + 1, fn))
    return violations, legacy, idiom, frozen


def over_budget(legacy):
    return [(rel, n, LEGACY_BUDGET.get(rel, 0)) for rel, n in sorted(legacy.items())
            if n > LEGACY_BUDGET.get(rel, 0)]


def selftest(tree):
    texts = read_tree(tree)
    bad = 0

    def fires(label, rel, old, new, want=True):
        nonlocal bad
        if rel not in texts or old not in texts[rel]:
            print('  !! control "%s": its mutation no longer applies -- the '
                  'control has rotted' % label)
            bad += 1
            return
        mutated = dict(texts)
        mutated[rel] = texts[rel].replace(old, new, 1)
        v, legacy, idiom, frozen = analyse(mutated)
        hit = bool(v or idiom or frozen or over_budget(legacy))
        if hit == want:
            print('  ok  control "%s" %s' % (label, 'fires' if want else
                                             'is quiet'))
        else:
            print('  !! control "%s" %s' % (label, 'did NOT fire' if want
                                            else 'fired on clean input'))
            bad += 1

    v, legacy, idiom, frozen = analyse(texts)
    if v or idiom or frozen or over_budget(legacy):
        print('  !! control "the tree is clean" fired on the tree itself')
        bad += 1
    else:
        print('  ok  control "the tree is clean" is quiet')
    fires('a ruleset cvar tested at a call site', 'g_items.c',
          'bool Pickup_Health(edict_t *ent, edict_t *other)\n{\n',
          'bool Pickup_Health(edict_t *ent, edict_t *other)\n{\n'
          '    if (ctf->value) return false;\n')
    fires('a legacy site moved into another file', 'g_ai.c',
          'void AI_SetSightClient(void)\n{\n',
          'void AI_SetSightClient(void)\n{\n    if (deathmatch->value) return;\n')
    fires('an ungated bq2_ site beside a gated one', 'm_boss2.c',
          '    if (self->content_flavour & CONTENT_ROGUE)\n'
          '        self->monsterinfo.currentmove = &boss2_move_walk;\n'
          '    else\n'
          '        self->monsterinfo.currentmove = &bq2_boss2_move_walk;\n',
          '    self->monsterinfo.currentmove = &boss2_move_walk;\n'
          '    if (self->content_flavour & CONTENT_ROGUE)\n'
          '        self->monsterinfo.currentmove = &boss2_move_walk;\n'
          '    else\n'
          '        self->monsterinfo.currentmove = &bq2_boss2_move_walk;\n')
    fires('a gate on something that is not the latch', 'm_boss2.c',
          '    if (self->content_flavour & CONTENT_ROGUE)\n'
          '        self->monsterinfo.currentmove = &boss2_move_walk;\n',
          '    if (random() < 0.5f)\n'
          '        self->monsterinfo.currentmove = &boss2_move_walk;\n')
    fires('a ternary currentmove', 'm_boss2.c',
          '    if (self->content_flavour & CONTENT_ROGUE)\n'
          '        self->monsterinfo.currentmove = &boss2_move_walk;\n'
          '    else\n'
          '        self->monsterinfo.currentmove = &bq2_boss2_move_walk;\n',
          '    self->monsterinfo.currentmove = (self->content_flavour & '
          'CONTENT_ROGUE) ? &boss2_move_walk : &bq2_boss2_move_walk;\n')
    fires('the stale spawn_temp_t member', 'g_spawn.c', '"pausetime"',
          '"pausetime"; int x = st.pause_framenum')
    print('gates.py --selftest: %d wrong' % bad)
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tree', default=os.path.join(REPO, 'src'))
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    if a.selftest:
        return 1 if selftest(a.tree) else 0

    violations, legacy, idiom, frozen = analyse(read_tree(a.tree))

    total = sum(legacy.values())
    print(f'gates.py: {len(violations)} forbidden ruleset-cvar test(s); '
          f'{total} inherited deathmatch/coop site(s) '
          f'across {len(legacy)} file(s)')

    for name, line, snippet in violations:
        print(f'  !! {name}:{line}: `{snippet}` -- a ruleset cvar tested at a '
              f'call site. Use a predicate or a dispatch row.')

    for name, line, msg in frozen:
        where = f'{name}:{line}' if line else name
        print(f'  !! {where}: {msg}')

    for name, line, fn in idiom:
        print(f'  !! {name}:{line} ({fn}): the monster-suppression idiom is '
              f'back. `deathmatch` is 1 under ctf, so this suppresses the '
              f'monsters that are promised there. Use !G_MonstersAllowed().')

    over = over_budget(legacy)
    for rel, n, cap in over:
        print(f'  !! {rel}: {n} inherited deathmatch/coop test(s) against a '
              f'ceiling of {cap} -- new code must ask a named question, not '
              f'test `deathmatch`')

    ok = not violations and not idiom and not frozen and not over
    if ok:
        print('  every ruleset decision goes through the dispatch or a '
              'predicate')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
