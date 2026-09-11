#!/usr/bin/env python3
"""The damage and use paths normalise a NULL edict before anything reads one.

WHY.  baseq2 never assigns `activator` on a `func_door` -- no site in
`g_func.c` does -- so `door_blocked` -> `door_go_up(ent, ent->activator)` ->
`G_UseTargets` -> a targeted `target_explosion` ->
`T_RadiusDamage(self, self->activator, ...)` reaches `T_Damage` with no
attacker at all.  id's own code survives that by accident and by a single
thread: its one `attacker->client` read sits behind `!(dflags & DAMAGE_RADIUS)`,
which short-circuits on exactly the path that produces the NULL.

The accident does not survive a merge.  Colosseum's `T_Damage` carries six
donors' arms -- CTF's techs and hurt-carrier bonus, tourney's runes and teams,
RA2's arena teams and score-by-damage, Ground Zero's DMGame hooks -- and reads
`attacker` at some twenty sites, most of which dereference before testing.  The
first one to fire in production was `CTFApplyStrength`, on a *xatrix
deathmatch* server (R-SEC-10).  Guarding it alone only moves the crash:
`CTFCheckHurtCarrier()` and `M_ReactToDamage()` are unconditional on every
ruleset, and between them they cover both kinds of entity a door can crush.

So the NULL is answered once, at the boundary.  This check exists because that
is a one-line invariant with twenty silent beneficiaries: delete it and the
tree still builds, still boots, still passes every other audit, and dies on a
map nobody in the rotation has reached yet.

THERE ARE TWO BOUNDARIES, and the second is not a refinement of the first.
`G_UseTargets` is reached BEFORE any damage -- `door_blocked` and a `func_clock`
that is not START_OFF both hand it the activator they never had -- and its
message arm reads `activator->svflags` on the spot, so an empty server with no
client and no damage at all dies a few seconds into a map carrying a
`target_explosion` with a `message`.  `scenarios/ra2nullattacker` is the
behavioural half of both: it fails on a tree missing either one.

WHAT IS A FINDING.  Three things, at each boundary.  The normalisation missing
from the function altogether; a read of the parameter placed ABOVE it, which is
the same crash with the fix still in the file; and the substituted value not
being `world`, since `world->client` is what makes every downstream
`->client` test still answer what the missing edict meant.

WHAT IS NOT.  Reads below the normalisation, guarded or not -- that is the
point of having it.  Comments and string literals are stripped first, so this
file's own prose and `g_combat.c`'s are not findings.

USAGE
    tools/nullattacker.py [--tree src]
    tools/nullattacker.py --selftest
"""
import argparse
import os
import re
import shutil
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The boundaries the contract is about: the file, the function, and the
# parameter each one normalises.  Two, because an edict that nothing assigned
# enters this library by two doors and only one of them is the damage path.
BOUNDARIES = [
    ('g_combat.c', 'T_Damage', 'attacker'),
    ('g_utils.c', 'G_UseTargets', 'activator'),
]
SUBST = 'world'


def strip_comments(code):
    # Comments and string/char literals become blanks of the same length, so
    # line numbers survive and prose about `attacker->` is not a finding.
    out = []
    i, n = 0, len(code)
    while i < n:
        c = code[i:i + 2]
        if c == '/*':
            j = code.find('*/', i + 2)
            j = n if j < 0 else j + 2
            out.append(''.join(ch if ch == '\n' else ' ' for ch in code[i:j]))
            i = j
        elif c == '//':
            j = code.find('\n', i)
            j = n if j < 0 else j
            out.append(' ' * (j - i))
            i = j
        elif code[i] in '"\'':
            q = code[i]
            j = i + 1
            while j < n and code[j] != q:
                j += 2 if code[j] == '\\' else 1
            j = min(j + 1, n)
            out.append(''.join(ch if ch == '\n' else ' ' for ch in code[i:j]))
            i = j
        else:
            out.append(code[i])
            i += 1
    return ''.join(out)


def body_of(code, name):
    """The text of `name`'s definition, by brace matching."""
    m = re.search(r'^[A-Za-z_][\w \*]*\b' + re.escape(name) + r'\s*\([^;{]*?\)\s*\{',
                  code, re.M | re.S)
    if not m:
        return None, None
    start = code.index('{', m.start())
    depth = 0
    for j in range(start, len(code)):
        if code[j] == '{':
            depth += 1
        elif code[j] == '}':
            depth -= 1
            if depth == 0:
                return code[m.start():j + 1], m.start()
    return None, None


# `if (!p)` / `if (p == NULL)`, then `p = world;`
def norm_re(param):
    return re.compile(
        r'if\s*\(\s*(?:!\s*' + param + r'|' + param + r'\s*==\s*(?:NULL|0))\s*\)\s*'
        r'(?:\{\s*)?' + param + r'\s*=\s*(\w+)\s*;')


def read_re(param):
    return re.compile(r'\b' + param + r'\s*(?:->|\[)')


def scan_one(tree, fname, func, param):
    """One boundary. Returns a list of finding strings."""
    path = os.path.join(tree, fname)
    if not os.path.exists(path):
        return ['%s: not found, so the contract cannot be checked'
                % os.path.relpath(path, REPO)]
    rel = os.path.relpath(path, REPO)
    code = strip_comments(open(path, encoding='utf-8').read())
    body, off = body_of(code, func)
    if body is None:
        return ['%s: %s() not found, so the contract cannot be checked'
                % (rel, func)]

    def lineno(pos):
        return code.count('\n', 0, off + pos) + 1

    hits = []
    m = norm_re(param).search(body)
    if not m:
        hits.append('%s: %s() does not normalise a NULL `%s` (R-SEC-10)'
                    % (rel, func, param))
        return hits
    if m.group(1) != SUBST:
        hits.append('%s:%d: %s() substitutes `%s`, not `%s` (R-SEC-10)'
                    % (rel, lineno(m.start()), func, m.group(1), SUBST))
    for r in read_re(param).finditer(body):
        if r.start() < m.start():
            hits.append('%s:%d: `%s` is read above the normalisation (R-SEC-10)'
                        % (rel, lineno(r.start()), param))
    return hits


def scan(tree):
    """Every boundary. Returns a list of finding strings."""
    hits = []
    for fname, func, param in BOUNDARIES:
        hits += scan_one(tree, fname, func, param)
    return hits


# One synthetic body per boundary, in the shape the real one has: the guard
# the function opens with, the normalisation, and a read below it.  `%s` is
# where the normalisation goes, so a mutant is made by putting something else
# there.
PROLOGUE = {
    'T_Damage': """void T_Damage(edict_t *targ, edict_t *inflictor, edict_t *attacker,
              int damage, int dflags, int mod)
{
    if (!targ->takedamage)
        return;
%s
    if (attacker->client && targ->client)
        damage = CTFApplyStrength(attacker, damage);
    CTFCheckHurtCarrier(targ, attacker);
}
""",
    'G_UseTargets': """void G_UseTargets(edict_t *ent, edict_t *activator)
{
    edict_t *t;
%s
    if (ent->delay) {
        t = G_Spawn();
        t->activator = activator;
        return;
    }
    if ((ent->message) && !(activator->svflags & SVF_MONSTER))
        gi.centerprintf(activator, "%%s", ent->message);
}
""",
}


def good(func, param):
    return PROLOGUE[func] % ("\n    if (!%s)\n        %s = world;\n" % (param, param))


def mutants(func, param):
    """The three ways one boundary can be wrong, as (why, body) pairs."""
    return [
        ('the normalisation deleted', PROLOGUE[func] % ''),
        ('a read placed above it',
         PROLOGUE[func] % ("\n    if (%s->client)\n        (void)0;\n"
                           "    if (!%s)\n        %s = world;\n"
                           % (param, param, param))),
        ('`NULL` substituted instead of `world`',
         PROLOGUE[func] % ("\n    if (!%s)\n        %s = NULL;\n" % (param, param))),
    ]


def selftest():
    """Every boundary, mutated one at a time while the others stay good.

    One at a time on purpose: a mutant that is only caught because a SECOND
    boundary is also broken would pass a selftest that broke both, and the
    whole point of the table is that each row is checked on its own.
    """
    ok = True
    tmp = tempfile.mkdtemp(prefix='nullattacker-selftest-')

    def write_all():
        for fname, func, param in BOUNDARIES:
            open(os.path.join(tmp, fname), 'w').write(good(func, param))

    try:
        write_all()
        base = scan(tmp)
        if base:
            print('!! selftest: the clean tree reported %s' % (base,))
            ok = False
        else:
            print('   [control] %d normalised boundary/ies, reads below: 0 findings'
                  % len(BOUNDARIES))
        for fname, func, param in BOUNDARIES:
            for why, body in mutants(func, param):
                write_all()
                open(os.path.join(tmp, fname), 'w').write(body)
                if not scan(tmp):
                    print('!! selftest: %s(): %s was NOT caught' % (func, why))
                    ok = False
                else:
                    print('   [control] %s(): %s -> caught' % (func, why))
        # A negative control for the comment stripper: prose naming
        # `attacker->client` above the normalisation is not a read.
        write_all()
        open(os.path.join(tmp, 'g_combat.c'), 'w').write(
            good('T_Damage', 'attacker').replace(
                '    if (!targ->takedamage)',
                '    // attacker->client is named here on purpose\n'
                '    if (!targ->takedamage)'))
        if scan(tmp):
            print('!! selftest: prose naming attacker->client was read as code')
            ok = False
        else:
            print('   [control] prose naming `attacker->client`: 0 findings')
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
    hits = scan(os.path.abspath(a.tree))
    for h in hits:
        print('!! %s' % h)
    print('nullattacker: %d finding(s)' % len(hits))
    return 1 if hits else 0


if __name__ == '__main__':
    sys.exit(main())
