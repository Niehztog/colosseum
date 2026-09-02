#!/usr/bin/env python3
"""Every value this tree READS is a value this tree can PRODUCE (R-192).

WHY.  R-191's defect was not a wrong line, it was a MISSING one: the merge kept
`resp.osp_r240`, kept all three of its readers, and dropped every write of 2.
Nothing warns.  `if (client->resp.osp_r240 != 2)` is a valid comparison that is
simply always true, so the guarded branch is dead and the feature behind it --
the view weapon, the KillBox, the autocam's whole candidate search -- silently
does the other thing forever.  R-155 is the same shape one field over
(`pers.showmotd` had a reader, a clear and no write, so the message of the day
was loaded at every map load and shown to nobody), and so is R-192's own
`osp_r24c == 8`: a five-page scoreboard whose fifth page nothing could open.

This is R-TOOL-6's third space.  `itemnames.py` resolves a literal against the
ITEMLIST, `classnames.py` against every classname the tree can put in an edict,
and this one against **the set of values a field can hold** -- the same rule
("resolve a literal against the space that can produce it") applied to state
rather than to names.

THREE QUESTIONS, and the third is a different space in the same shape:

  1. IN-TREE.  A field compared for equality against a constant that no write in
     the tree produces.  Sound only when every write to that field is itself a
     constant: a field written from a variable can hold anything, so it is
     skipped rather than guessed at.  Zero is always producible -- `memset`,
     `InitClientResp` and a designated initialiser all write it -- so a
     comparison against 0 is never a finding.

  2. AGAINST A DONOR.  A (field, constant) pair a donor writes and this tree
     cannot produce.  This is the sharper instrument, because the donor is the
     authority on which values a field is *supposed* to take: it is what found
     the in-eyes camera mode and the player card.  Renamed fields are compared
     under the merged name (RENAME) and everything else by name, so a field the
     merge renamed without an entry here is a false negative rather than a false
     alarm.

  3. DISPATCH.  A function-pointer row assigned through a named object and never
     called through one.  `DMGame.SelectSpawnPoint = DBall_SelectSpawnPoint` was
     installed and never read, so Ground Zero's DBall spawned both teams out of
     the shared deathmatch pool for as long as this tree has existed (R-192).
     Rows reached through a macro (`GATE(row)`) or by another binary
     (`bot_import_t`, which the botlib calls back through) are not findings: the
     rule is per OBJECT, so only an object this tree dispatches through is asked.

WHAT IS NOT A FINDING.  A field written from an expression; a comparison against
0; `<`/`>`/`&` tests, which are ranges and masks rather than states; and the
exemptions below, each of which is either verified as correctly dropped or is an
open finding with a written record.  An exemption that no longer appears is
itself reported, for the reason itemnames.py gives: an exemption list is how a
check dies quietly.

USAGE
    tools/deadvalue.py [--tree src] [--donor LABEL=PATH]...
    tools/deadvalue.py --selftest
"""
import argparse
import collections
import os
import re
import shutil
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BLOCK_RE = re.compile(r'/\*.*?\*/', re.S)

# Fields the merge renamed because the donor's name collided with baseq2's in
# the merged struct (R-58).  PER DONOR, because the collision is between two
# donors: `resp.entered` is tourney's four-state enum and RA2's bool, and mapping
# both onto `osp_entered` would compare RA2's `true` against tourney's states.
RENAME = {
    'osp-tourney': {'entered': 'osp_entered'},
    'tourney': {'entered': 'osp_entered'},
}

# Verified.  Each entry is (field, value) -> why.  The first three are donor
# writes correctly NOT carried; the last two are open findings recorded in
# doc/reconciliation.md R-192, listed here so that `make check` reports the state
# of the tree rather than the state of the backlog.
# A donor write of ZERO is never a finding -- a memset, an InitClientResp and a
# designated initialiser all produce it -- so the three that resolve that way
# (`isbot = 0`, `osp_r2a8 = 0`, `osp_r2bc = 0`) need no entry and have none; they
# are recorded in R-192 as checked, along with why each was correctly dropped.
# Each entry names the DONOR it came from, or None for an in-tree finding, and
# staleness is only asked of an exemption whose donor was actually compared --
# `make check` passes one donor and would otherwise report the other four as
# stale on every run, which is how a check gets switched off.
EXEMPT = {
    ('entered', 'true'): ('rocketarena2',
        "RA2's, and it is baseq2's bool rather than tourney's enum: its only "
        "reader in RA2 is a reconnect arm this tree does not have, and the field "
        "is shared (R-58), so the owner clears it its own way"),
}

CONST = re.compile(r'^-?(?:0[xX][0-9a-fA-F]+|\d+|[A-Z][A-Z0-9_]*|true|false|NULL)$')
ASSIGN = re.compile(r'(?:->|\.)([A-Za-z_]\w*)\s*=\s*([^;=][^;]*);')
COMPOUND = re.compile(r'(?:->|\.)([A-Za-z_]\w*)\s*(?:\+\+|--|\+=|-=|\|=|&=|\^=|\*=|/=|<<=|>>=)')
PREINC = re.compile(r'(?:\+\+|--)\s*[\w.\[\]]*(?:->|\.)([A-Za-z_]\w*)')
ADDR = re.compile(r'&\s*[\w.\[\]]*(?:->|\.)([A-Za-z_]\w*)')
EQ = re.compile(r'(?:->|\.)([A-Za-z_]\w*)\s*(?:==|!=)\s*([A-Za-z_0-9]+)')
EQ_REV = re.compile(r'\b([A-Za-z_0-9]+)\s*(?:==|!=)\s*[\w.\[\]]*(?:->|\.)([A-Za-z_]\w*)\b')
USE = re.compile(r'(?:->|\.)([A-Za-z_]\w*)')
# `OBJ.row = f` / `OBJ->row = f` and `OBJ.row(` / `OBJ->row(`, for question 3.
OBJ_ASSIGN = re.compile(r'\b([A-Za-z_]\w*)\s*(?:\.|->)\s*([A-Za-z_]\w*)\s*=\s*'
                        r'([A-Za-z_]\w*)\s*;')
OBJ_CALL = re.compile(r'\b([A-Za-z_]\w*)\s*(?:\.|->)\s*([A-Za-z_]\w*)\s*\(')
# A function-pointer member: `void (*row)(edict_t *ent);`
FUNCPTR = re.compile(r'^\s*[A-Za-z_][\w \t*]*\(\s*\*\s*([A-Za-z_]\w*)\s*\)\s*\(', re.M)
# A struct/union body, with its typedef name if it has one.
STRUCT = re.compile(r'(?:typedef\s+)?(?:struct|union)\s*(?:\w+\s*)?\{(.*?)\}\s*'
                    r'([A-Za-z_]\w*)?\s*;', re.S)


def dispatch_tables(tree):
    """Objects this tree dispatches through, and the rows each type declares.

    A row is only asked the question where BOTH ends resolve: the member is a
    declared function pointer, and the object is a file-scope variable of a type
    that declares it.  Without the first, `ent->movetype = MOVETYPE_NOCLIP` is a
    row assignment; without the second, `self->think = func` is one too -- called
    through a different local's name in another function, which is every think in
    the tree.
    """
    rows = collections.defaultdict(set)          # typedef name -> {row, ...}
    for _, code in sources(tree):
        for m in STRUCT.finditer(code):
            body, tag = m.group(1), m.group(2)
            if not tag:
                continue
            for f in FUNCPTR.finditer(body):
                rows[tag].add(f.group(1))
    objects = {}                                 # object name -> typedef name
    for _, code in sources(tree):
        for tag in rows:
            for m in re.finditer(r'^\s*(?:extern\s+)?' + re.escape(tag) +
                                 r'\s+([A-Za-z_]\w*)\s*;', code, re.M):
                objects[m.group(1)] = tag
    return rows, objects
# Anything that writes through an argument: a field passed to one of these is
# written with no `=` in sight.
WRITERS = ('VectorCopy|VectorClear|VectorSet|VectorAdd|VectorSubtract|VectorScale|'
           'VectorMA|VectorNormalize|VectorAvg|VectorInverse|AngleVectors|'
           'ClearBounds|AddPointToBounds|CrossProduct|G_ProjectSource|'
           'memcpy|memset|memmove|strcpy|strncpy|strcat|Q_strlcpy|Q_strlcat|'
           'Q_snprintf|snprintf|sprintf|Info_SetValueForKey|sscanf|fgets|fread')
WRITE_ARG = re.compile(r'\b(?:' + WRITERS + r')\s*\(([^;]*)')


def strip_comments(src):
    """Both comment forms, line count preserved: this file's own header names
    `osp_r240 != 2`, so a check that read comments would report itself."""
    return re.sub(r'//[^\n]*', '',
                  BLOCK_RE.sub(lambda m: '\n' * m.group(0).count('\n'), src))


def sources(tree, headers=True):
    exts = ('.c', '.h') if headers else ('.c',)
    for root, dirs, files in os.walk(tree):
        dirs[:] = [d for d in dirs if d != '.git']
        for fn in sorted(files):
            if fn.endswith(exts):
                p = os.path.join(root, fn)
                yield p, strip_comments(open(p, errors='replace').read())


def collect(tree, rename=None, headers=True):
    """writes[field] -> {constant or '?'}, plus where each pair came from."""
    writes = collections.defaultdict(set)
    where = {}
    eq = collections.defaultdict(list)
    uses = collections.Counter()
    assigned, called = {}, set()
    for path, code in sources(tree, headers):
        rel = os.path.relpath(path, tree)
        for i, line in enumerate(code.split('\n'), 1):
            for m in ASSIGN.finditer(line):
                f, v = m.group(1), m.group(2).strip()
                if rename:
                    f = rename.get(f, f)
                v = v if CONST.match(v) else '?'
                writes[f].add(v)
                where.setdefault((f, v), (rel, i))
            for rx in (COMPOUND, PREINC, ADDR):
                for m in rx.finditer(line):
                    f = m.group(1)
                    writes[rename.get(f, f) if rename else f].add('?')
            for m in WRITE_ARG.finditer(line):
                for f in USE.findall(m.group(1)):
                    writes[rename.get(f, f) if rename else f].add('?')
            for m in EQ.finditer(line):
                if CONST.match(m.group(2)):
                    eq[m.group(1)].append((m.group(2), rel, i))
            for m in EQ_REV.finditer(line):
                if CONST.match(m.group(1)):
                    eq[m.group(2)].append((m.group(1), rel, i))
            for m in USE.finditer(line):
                uses[m.group(1)] += 1
            for m in OBJ_ASSIGN.finditer(line):
                obj, row, val = m.groups()
                if val[:1].isupper() or val.startswith(('SP_', 'ctf_', 'osp_')):
                    assigned.setdefault((obj, row), (rel, i))
            for m in OBJ_CALL.finditer(line):
                called.add((m.group(1), m.group(2)))
    return writes, where, eq, uses, assigned, called


def macros(tree):
    out = {}
    for _, code in sources(tree):
        for m in re.finditer(r'^\s*#\s*define\s+([A-Z][A-Z0-9_]*)\s+(-?\w+)\s*$',
                             code, re.M):
            out[m.group(1)] = m.group(2)
        for m in re.finditer(r'^\s*#\s*define\s+([A-Z][A-Z0-9_]*)\s+BIT\((\d+)\)',
                             code, re.M):
            out[m.group(1)] = str(1 << int(m.group(2)))
    return out


def numeric(tok, mac):
    seen = set()
    while tok in mac and tok not in seen:
        seen.add(tok)
        tok = mac[tok]
    try:
        return int(tok, 0)
    except ValueError:
        return None


def producible(values, mac):
    """Every spelling of every value a field can hold, plus zero."""
    out = set(values) | {'0', 0, 'false', 'NULL'}
    for v in values:
        n = numeric(v, mac)
        if n is not None:
            out.add(n)
    return out


def scan(tree, donors=(), check_stale=True):
    hits = []
    mac = macros(tree)
    writes, _, eq, uses, assigned, called = collect(tree)
    used_exempt = set()

    # --- 1. in-tree ------------------------------------------------------
    for f, cmps in sorted(eq.items()):
        w = writes.get(f, set())
        if not w or '?' in w:
            continue
        have = producible(w, mac)
        for c, rel, line in cmps:
            n = numeric(c, mac)
            if c in have or (n is not None and n in have):
                continue
            if (f, c) in EXEMPT:
                used_exempt.add((f, c))
                continue
            hits.append(('value', rel, line,
                         '%s is compared with %s and only %s is ever written'
                         % (f, c, '/'.join(sorted(w)))))

    # --- 2. against each donor ------------------------------------------
    for label, path in donors:
        theirs, dwhere, _, _, _, _ = collect(path, RENAME.get(label), headers=False)
        for f in sorted(theirs):
            if f not in writes or '?' in writes[f]:
                continue
            # A field this tree barely mentions is not state it maintains.
            if uses[f] < 2:
                continue
            have = producible(writes[f], mac)
            for v in sorted({x for x in theirs[f] if x != '?'}):
                n = numeric(v, mac)
                if v in have or (n is not None and n in have):
                    continue
                if (f, v) in EXEMPT:
                    used_exempt.add((f, v))
                    continue
                rel, line = dwhere[(f, v)]
                hits.append(('donor', '%s/%s' % (label, rel), line,
                             '%s = %s is written by the donor and by nothing '
                             'here (this tree writes only %s)'
                             % (f, v, '/'.join(sorted(writes[f])))))

    # --- 3. dispatch -----------------------------------------------------
    rows, objects = dispatch_tables(tree)
    for (obj, row), (rel, line) in sorted(assigned.items()):
        if obj not in objects or row not in rows[objects[obj]]:
            continue
        # ...and the object has to be one THIS tree dispatches through.  The
        # game fills `gamebotimport` in for the BOTLIB to call back through, so
        # none of its rows has an in-tree caller and reporting all nine of them
        # says nothing.  One sibling row being called is what makes the table's
        # silence about a row meaningful.
        if not any((obj, r) in called for r in rows[objects[obj]]):
            continue
        if (obj, row) in called:
            continue
        if (obj, row) in EXEMPT:
            used_exempt.add((obj, row))
            continue
        hits.append(('dispatch', rel, line,
                     '%s.%s is assigned and never called through %s'
                     % (obj, row, obj)))

    # --- and the exemptions, checked for going stale ---------------------
    if check_stale:
        compared = {l for l, _ in donors}
        for key, (origin, _why) in sorted(EXEMPT.items()):
            if origin is not None and origin not in compared:
                continue
            if key not in used_exempt:
                hits.append(('stale', '-', 0,
                             'exemption %s/%s no longer fires' % key))
    return hits


# ---------------------------------------------------------------------------
# controls
# ---------------------------------------------------------------------------
CLEAN = {
    'ops.h': '''
typedef struct {
    void (*row)(edict_t *ent);
    void (*orphan)(edict_t *ent);
} ops_t;
extern ops_t Ops;
''',
    'ok.c': '''
void f(edict_t *ent)
{
    ent->client->resp.mode = 2;
    if (ent->client->resp.mode == 2) return;      /* producible */
    if (ent->client->resp.mode == 0) return;      /* zero always is */
    if (ent->client->resp.other == 7) return;     /* written from a variable */
    ent->client->resp.other = compute();
    ent->client->resp.count++;                    /* ++ can produce anything */
    if (ent->client->resp.count == 9) return;
    VectorCopy(a, ent->s.origin);                 /* written through an arg */
    if (ent->s.origin == 3) return;
    Ops.row = Handler;
    Ops.row(ent);                                 /* assigned and called */
}
''',
}
MUTANTS = [
    # Fresh field and row names in every mutant: a name the clean tree also
    # writes or calls would resolve against THAT, which is how a control passes
    # while testing nothing.
    ('a constant nothing writes',
     'void g(edict_t *e) { e->client->resp.phase = 1;'
     ' if (e->client->resp.phase == 2) return; }'),
    ('a row assigned and never called',
     'void g(void) { Ops.orphan = Handler; }'),
]


def selftest():
    ok = True

    def report(good, why):
        nonlocal ok
        print('%s selftest: %s' % ('  ' if good else '!!', why))
        if not good:
            ok = False

    tmp = tempfile.mkdtemp(prefix='deadvalue-')
    try:
        for name, body in CLEAN.items():
            open(os.path.join(tmp, name), 'w').write(body)
        base = scan(tmp, check_stale=False)
        report(not base, 'producible constant, zero, variable write, ++, '
                         'writer-arg and a called row: %s'
                         % ('0 findings' if not base else base))

        for why, body in MUTANTS:
            p = os.path.join(tmp, 'bad.c')
            open(p, 'w').write(body + '\n')
            got = scan(tmp, check_stale=False)
            os.remove(p)
            report(bool(got), '%s -> %s' % (why, 'caught' if got else 'MISSED'))

        # a donor that writes a value this tree cannot
        donor = tempfile.mkdtemp(prefix='deadvalue-donor-')
        try:
            open(os.path.join(donor, 'd.c'), 'w').write(
                'void h(edict_t *e) { e->client->resp.mode = 5; }\n')
            got = scan(tmp, [('donor', donor)], check_stale=False)
            report(bool(got), 'a donor write this tree cannot produce -> %s'
                              % ('caught' if got else 'MISSED'))
        finally:
            shutil.rmtree(donor, ignore_errors=True)

        # ...and a stale exemption reports.  Injected rather than borrowed from
        # EXEMPT, because the real entries are donor-scoped and a selftest
        # compares no donor.
        EXEMPT[('nosuchfield', '3')] = (None, 'injected by the selftest')
        try:
            got = scan(tmp, check_stale=True)
            report(any(h[0] == 'stale' for h in got),
                   'a stale exemption is itself a finding -> %s'
                   % ('caught' if any(h[0] == 'stale' for h in got) else 'MISSED'))
            # ...and one whose donor was not compared is NOT stale.
            EXEMPT[('nosuchfield', '4')] = ('absent-donor', 'injected')
            got = scan(tmp, check_stale=True)
            report(sum(1 for h in got if h[0] == 'stale') == 1,
                   "an exemption for a donor that was not compared is not "
                   "stale -> %s" % ('correct' if sum(
                       1 for h in got if h[0] == 'stale') == 1 else 'REPORTED'))
        finally:
            EXEMPT.pop(('nosuchfield', '3'), None)
            EXEMPT.pop(('nosuchfield', '4'), None)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tree', default=os.path.join(REPO, 'src'))
    ap.add_argument('--donor', action='append', default=[], metavar='LABEL=PATH')
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    if a.selftest:
        return 0 if selftest() else 1

    tree = os.path.abspath(a.tree)
    donors = [d.split('=', 1) for d in a.donor if '=' in d]
    donors = [(l, p) for l, p in donors if os.path.isdir(p)]
    whole = (tree == os.path.join(REPO, 'src'))
    hits = scan(tree, donors, check_stale=whole)
    for kind, rel, line, what in hits:
        if kind == 'stale':
            print('!! stale exemption: %s (R-192)' % what)
        else:
            print('!! %s:%d: %s (R-192)' % (rel, line, what))
    print('deadvalue: %d finding(s); %d donor tree(s) compared, %d exemption(s)%s'
          % (len(hits), len(donors), len(EXEMPT),
             '' if whole else ' (partial tree: exemptions not checked for going '
                              'stale)'))
    return 1 if hits else 0


if __name__ == '__main__':
    sys.exit(main())
