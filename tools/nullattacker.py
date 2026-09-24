#!/usr/bin/env python3
"""A NULL edict is answered before anything reads it: normalised where the
damage path enters, and tested where the use path reads it.

WHY.  baseq2 never assigns `activator` on a `func_door` -- no site in
`g_func.c` does -- and a `func_clock` that is not START_OFF never has one
either.  Both hand what they hold to `G_UseTargets`, which passes it three
ways: into its own message arm, into the `use` of every entity it fires, and --
through a targeted `target_explosion`, `T_RadiusDamage(self, self->activator,
...)` -- into `T_Damage` as the attacker.  id's own code survives the damage
half by accident and by a single thread: its one `attacker->client` read sits
behind `!(dflags & DAMAGE_RADIUS)`, which short-circuits on exactly the path
that produces the NULL.

The accident does not survive a merge.  Colosseum's `T_Damage` carries six
donors' arms -- CTF's techs and hurt-carrier bonus, tourney's runes and teams,
RA2's arena teams and score-by-damage, Ground Zero's DMGame hooks -- and reads
`attacker` at some twenty sites, most of which dereference before testing.  The
first one to fire in production was `CTFApplyStrength`, on a *xatrix
deathmatch* server (R-SEC-10).

TWO SHAPES, because the two halves want different answers, and they are the
donors' shapes -- `rocketarena2@99f8bb2`, `osp-tourney@a8d1725`, q2pro's
`feature/mission-packs@02857024`:

  * THE DAMAGE PATH IS NORMALISED WHERE IT ENTERS.  `T_Damage` substitutes
    `world` for a NULL attacker in its prologue, because its twenty reads grow
    with every donor and the boundary does not, and `world->client` is NULL, so
    every downstream `attacker->client` test still answers what the missing
    attacker meant.  Findings: the normalisation missing, a read placed above
    it, or something other than `world` substituted.

  * THE USE PATH IS TESTED WHERE IT READS.  A `use` callback is a different
    contract per entity, and the message arm prints to its activator, so the
    NULL travels on and each read carries its own test: `G_UseTargets`' message
    arm, `monster_use`, the two triggered-spawn callbacks, `trigger_key_use` --
    and `plat2_operate`, which receives the activator as `other` from
    `Use_Plat2` and is the one site beyond the donors' list, because their
    sweeps looked for parameters NAMED `activator`.  So this sweep follows the
    pointer rather than the name: it starts at every function a `->use` slot is
    assigned and every parameter named `activator`, and follows each call that
    passes the pointer on UNGUARDED into the callee's own parameter, whatever
    that is called.  A finding is a dereference (`p->`, `p[`) that no test
    covers.  What covers one: an early exit (`if (!p) return;`, `if (!p || ...)
    return;`), the normalisation above, an `if` whose condition tests `p`
    (the block it controls), or a test earlier in the same expression (`p &&`,
    `!p ||`, `p ? ...`).  A bare `if (!p) gi.dprintf(...)` covers nothing after
    it, and `!p->client` is a read, not a test -- both are controls below.

WHAT IS NOT.  Engine calls: Q2PRO's `centerprintf`, `cprintf` and `sound` all
return on a NULL edict, so `trigger_counter_use` handing its activator to them
is the donors' code and safe.  Comments and string literals are stripped first,
so this file's own prose and the sources' are not findings.

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

# The one boundary: the file, the function, and the parameter it normalises.
BOUNDARIES = [
    ('g_combat.c', 'T_Damage', 'attacker'),
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



# ------------------------------------------------------------ the use sweep

DEF_RE = re.compile(r'^[A-Za-z_][\w \*]*?\b([A-Za-z_]\w*)\s*\(([^;{)]*)\)\s*\{',
                    re.M)
USE_RE = re.compile(r'(?:->|\.)use\s*=\s*([A-Za-z_]\w*)\s*;')
CALL_RE = re.compile(r'\b([A-Za-z_]\w*)\s*\(')


def match_paren(s, i):
    """Index of the ')' matching the '(' at s[i]."""
    depth = 0
    for j in range(i, len(s)):
        if s[j] == '(':
            depth += 1
        elif s[j] == ')':
            depth -= 1
            if depth == 0:
                return j
    return len(s) - 1


def match_brace(s, i):
    depth = 0
    for j in range(i, len(s)):
        if s[j] == '{':
            depth += 1
        elif s[j] == '}':
            depth -= 1
            if depth == 0:
                return j
    return len(s) - 1


def split_top(expr, op):
    """Split expr on a top-level `&&` or `||`."""
    out, depth, cur, i = [], 0, [], 0
    while i < len(expr):
        c = expr[i]
        if c in '([':
            depth += 1
        elif c in ')]':
            depth -= 1
        if depth == 0 and expr.startswith(op, i):
            out.append(''.join(cur).strip())
            cur = []
            i += 2
            continue
        cur.append(c)
        i += 1
    out.append(''.join(cur).strip())
    return out


def is_test(term, p, positive):
    t = term.strip()
    while t.startswith('(') and match_paren(t, 0) == len(t) - 1:
        t = t[1:-1].strip()
    if positive:
        return re.fullmatch(r'%s(\s*!=\s*(NULL|0))?' % p, t) is not None
    return re.fullmatch(r'!\s*%s|%s\s*==\s*(NULL|0)' % (p, p), t) is not None


def controlled(body, k):
    """[start, end) of the statement or block that begins at body[k:]."""
    while k < len(body) and body[k].isspace():
        k += 1
    if k < len(body) and body[k] == '{':
        return k, match_brace(body, k) + 1
    if body.startswith('if', k) and re.match(r'if\s*\(', body[k:]):
        c = body.index('(', k)
        a, b = controlled(body, match_paren(body, c) + 1)
        m = re.match(r'\s*else\b', body[b:])
        if m:
            return k, controlled(body, b + m.end())[1]
        return k, b
    return k, body.find(';', k) + 1 or len(body)


def guarded_regions(body, p):
    """The spans of `body` in which `p` is known not to be NULL."""
    regions = []
    for m in norm_re(p).finditer(body):
        regions.append((m.end(), len(body)))
    for m in re.finditer(r'\bif\s*\(', body):
        c = body.index('(', m.start())
        e = match_paren(body, c)
        cond = body[c + 1:e]
        a, b = controlled(body, e + 1)
        stmt = body[a:b]
        if any(is_test(t, p, True) for t in split_top(cond, '&&')):
            regions.append((a, b))
        if any(is_test(t, p, False) for t in split_top(cond, '||')) and \
                re.search(r'\breturn\b', stmt) and \
                (stmt.lstrip().startswith('return') or
                 re.search(r'\breturn\b[^;]*;\s*}\s*$', stmt)):
            regions.append((b, len(body)))
    # a test earlier in the same expression covers the rest of it
    for rx in (r'\b%s\s*(!=\s*(NULL|0)\s*)?&&' % p,
               r'(!\s*%s|\b%s\s*==\s*(NULL|0))\s*\|\|' % (p, p),
               r'\b%s\s*\?' % p):
        for m in re.finditer(rx, body):
            depth, j = 0, m.start()
            while j > 0:
                j -= 1
                if body[j] == ')':
                    depth += 1
                elif body[j] == '(':
                    if depth == 0:
                        break
                    depth -= 1
                elif body[j] in ';{}' and depth == 0:
                    j = -1
                    break
            end = match_paren(body, j) if j >= 0 and body[j] == '(' else \
                body.find(';', m.end())
            regions.append((m.end(), end if end > 0 else len(body)))
    return regions


def covered(pos, regions):
    return any(a <= pos < b for a, b in regions)


def load_tree(tree):
    code = {}
    for d, _, fs in os.walk(tree):
        for f in sorted(fs):
            if f.endswith('.c'):
                path = os.path.join(d, f)
                code[path] = strip_comments(open(path, encoding='utf-8',
                                                 errors='replace').read())
    defs, uses = {}, set()
    for path, c in code.items():
        for m in USE_RE.finditer(c):
            uses.add(m.group(1))
        for m in DEF_RE.finditer(c):
            name, params = m.group(1), m.group(2)
            if name in ('if', 'while', 'for', 'switch'):
                continue
            start = c.index('{', m.end() - 1)
            end = match_brace(c, start)
            names = [re.sub(r'.*[\s\*]', '', x.strip()) for x in params.split(',')]
            defs[name] = (path, names, c[start:end + 1], c, start)
    return defs, uses


def sweep(tree):
    """Every use-path pointer, followed. Returns a list of finding strings."""
    defs, uses = load_tree(tree)
    work = []
    for name, (path, names, body, c, start) in defs.items():
        if name in uses and len(names) == 3:
            work.append((name, 2))
        for i, n in enumerate(names):
            if n == 'activator':
                work.append((name, i))
    seen, hits = set(), []
    while work:
        name, idx = work.pop()
        if (name, idx) in seen or name not in defs:
            continue
        seen.add((name, idx))
        path, names, body, c, start = defs[name]
        if idx >= len(names) or not re.fullmatch(r'[A-Za-z_]\w*', names[idx]):
            continue
        p = names[idx]
        regions = guarded_regions(body, p)
        rel = os.path.relpath(path, REPO)
        for r in re.finditer(r'\b%s\s*(->|\[)' % p, body):
            if not covered(r.start(), regions):
                hits.append('%s:%d: %s() reads `%s` before any NULL test (R-SEC-10)'
                            % (rel, c.count('\n', 0, start + r.start()) + 1, name, p))
                break
        # ...and every call that passes it on unguarded hands the NULL on.
        for m in CALL_RE.finditer(body):
            callee = m.group(1)
            if callee not in defs or callee == name:
                continue
            o = body.index('(', m.start())
            args = split_top(body[o + 1:match_paren(body, o)], ',')
            for i, a in enumerate(split_args(body[o + 1:match_paren(body, o)])):
                if a == p and not covered(m.start(), regions):
                    work.append((callee, i))
    return sorted(set(hits))


def split_args(s):
    out, depth, cur = [], 0, []
    for ch in s:
        if ch in '([':
            depth += 1
        elif ch in ')]':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(''.join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    out.append(''.join(cur).strip())
    return out


def scan(tree):
    """The boundary and the sweep. Returns a list of finding strings."""
    hits = []
    for fname, func, param in BOUNDARIES:
        hits += scan_one(tree, fname, func, param)
    return hits + sweep(tree)


# -------------------------------------------------------------- the controls

# A synthetic tree in the shape the real one has.  `%s` in T_Damage is where the
# normalisation goes; the use side carries every kind of cover the sweep
# accepts, plus a call made AFTER a guard into a helper that reads unguarded --
# which must not be a finding, because the pointer it hands on is not NULL.
FILES = {
    'g_combat.c': """void T_Damage(edict_t *targ, edict_t *inflictor, edict_t *attacker,
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
    'g_utils.c': """void G_UseTargets(edict_t *ent, edict_t *activator)
{
    edict_t *t;
    if (ent->delay) {
        t = G_Spawn();
        t->activator = activator;
        if (!activator)
            gi.dprintf("Think_Delay with no activator\\n");
        return;
    }
    if ((ent->message) && activator && !(activator->svflags & SVF_MONSTER))
        gi.centerprintf(activator, "%%s", ent->message);
}
""",
    'g_monster.c': """static void angry_at(edict_t *self, edict_t *who)
{
    self->enemy = who;
    if (who->client)
        self->goalentity = who;
}

void monster_use(edict_t *self, edict_t *other, edict_t *activator)
{
    if (self->enemy)
        return;
    if (!activator)
        return;
    if (activator->flags & FL_NOTARGET)
        return;
    angry_at(self, activator);
}

void monster_triggered_spawn_use(edict_t *self, edict_t *other, edict_t *activator)
{
    self->nextthink = level.framenum + 1;
    if (activator && activator->client)
        self->enemy = activator;
    self->use = monster_use;
}
""",
    'g_trigger.c': """void trigger_key_use(edict_t *self, edict_t *other, edict_t *activator)
{
    if (!self->item)
        return;
    if (!activator || !activator->client)
        return;
    gi.centerprintf(activator, "You need the %%s", self->item->pickup_name);
}

void SP_trigger_key(edict_t *self)
{
    self->use = trigger_key_use;
}
""",
    'g_func.c': """void plat2_operate(edict_t *ent, edict_t *other)
{
    int otherState = 0;
    if (ent->moveinfo.state == STATE_TOP) {
        if (other) {
            if (platCenter > other->s.origin[2])
                otherState = STATE_BOTTOM;
        }
    } else {
        if (other && other->s.origin[2] > platCenter)
            otherState = STATE_TOP;
    }
}

void Use_Plat2(edict_t *ent, edict_t *other, edict_t *activator)
{
    plat2_operate(ent->enemy, activator);
}

void SP_func_plat2(edict_t *ent)
{
    ent->use = Use_Plat2;
}
""",
}

# (why, file, old, new) -- one at a time, everything else good.
MUTANTS = [
    ('T_Damage(): the normalisation deleted', 'g_combat.c', None, ''),
    ('T_Damage(): a read placed above it', 'g_combat.c', None,
     "\n    if (attacker->client)\n        (void)0;\n"
     "    if (!attacker)\n        attacker = world;\n"),
    ('T_Damage(): `NULL` substituted instead of `world`', 'g_combat.c', None,
     "\n    if (!attacker)\n        attacker = NULL;\n"),
    ('G_UseTargets(): the message arm untested, the dprintf still there',
     'g_utils.c', '(ent->message) && activator && ', '(ent->message) && '),
    ('monster_use(): the early exit deleted', 'g_monster.c',
     '    if (!activator)\n        return;\n    if (activator->flags', '    if (activator->flags'),
    ('monster_triggered_spawn_use(): the test in the condition deleted',
     'g_monster.c', 'if (activator && activator->client)', 'if (activator->client)'),
    ('trigger_key_use(): `!activator->client` as the only "test"', 'g_trigger.c',
     'if (!activator || !activator->client)', 'if (!activator->client)'),
    ('plat2_operate(): the block test deleted, reached as `other` from Use_Plat2',
     'g_func.c', '        if (other) {\n            if (platCenter > other->s.origin[2])\n'
     '                otherState = STATE_BOTTOM;\n        }\n',
     '        if (platCenter > other->s.origin[2])\n            otherState = STATE_BOTTOM;\n'),
]

GOOD_NORM = "\n    if (!attacker)\n        attacker = world;\n"


def write_tree(tmp, mutant=None):
    for fname, text in FILES.items():
        if fname == 'g_combat.c':
            body = text % (mutant[3] if mutant and mutant[1] == fname and mutant[2] is None
                           else GOOD_NORM)
        else:
            body = text % () if '%%' in text else text
            if mutant and mutant[1] == fname and mutant[2] is not None:
                assert mutant[2] in body, mutant[0]
                body = body.replace(mutant[2], mutant[3])
        open(os.path.join(tmp, fname), 'w').write(body)


def selftest():
    """Every mutant caught on its own; the clean tree, and prose, silent."""
    ok = True
    tmp = tempfile.mkdtemp(prefix='nullattacker-selftest-')
    try:
        write_tree(tmp)
        base = scan(tmp)
        if base:
            print('!! selftest: the clean tree reported %s' % (base,))
            ok = False
        else:
            print('   [control] the clean tree: 0 findings -- including a guarded'
                  ' call into a helper that reads unguarded')
        for mutant in MUTANTS:
            write_tree(tmp, mutant)
            if not scan(tmp):
                print('!! selftest: %s was NOT caught' % mutant[0])
                ok = False
            else:
                print('   [control] %s -> caught' % mutant[0])
        # A negative control for the comment stripper: prose naming
        # `attacker->client` above the normalisation is not a read.
        write_tree(tmp)
        p = os.path.join(tmp, 'g_combat.c')
        text = open(p).read()
        open(p, 'w').write(text.replace(
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
