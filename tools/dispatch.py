#!/usr/bin/env python3
"""Is every `ruleset_ops_t` row actually TRAVERSED?

WHY.  The dispatch has now been got wrong three times, in three rulesets, and
every one of them was silent:

  1.  `EndDMLevel` called `BeginIntermission` by name at all eight sites, so
         `ops_ctf.BeginIntermission` -- `CTFCalcScores()` -- ran on none of
         them.  `ctfgame.total1/total2` stayed at whatever `CTFEndMatch` last
         wrote, so the frag tie-breaker read 0 > 0 both ways and blanked BOTH
         team headers at the end of every level whose captures were level.
  2.  `CheckDMRules` called `EndDMLevel` by name, and the only caller of
         `G_EndLevel()` was `osp_main.c`, whose rulesets never reach
         `CheckDMRules`.  So `ops_arena.EndLevel` -- RA2's whole `maploop:`
         rotation -- had never once been called.
  3.  Four callers in `src/tourney/` called `EndDMLevel` by name after
         setting the map they wanted.  `NextMap()` is the only reader of
         `selected_map`, and it lives in `OSP_EndLevel`, so a passed map vote,
         a referee `r_map` and the admin menu's map choice each ended the level
         without applying the map they had just chosen.

NOTHING IN THE TREE COULD SEE ANY OF THEM, and the reason is one property they
share: **a table initialiser is a reference.**  `.EndLevel = RA_EndLevel` makes
`RA_EndLevel` look used to every instrument here -- `fnsweep.py` compares
definitions against a donor, `lostref.py` asks after symbols a donor deleted,
`deadvalue.py` asks which values a field can hold, and the row is both written
and read (through `GATE()`).  Reachability THROUGH the dispatcher is a
different question and no check asked it.  All three defects were found by
playing the game.

FIVE QUESTIONS.

  A. DOES THE ROW HAVE A DISPATCHER?  For each function-pointer member of
     `ruleset_ops_t` there must be a `G_<Row>()` in `g_ruleset.c` whose body
     reads it through `GATE(<Row>)`.  A row with no dispatcher is a row nothing
     can reach.

  B. IS THE DISPATCHER CALLED?  At least once, from outside its own body.

  C. IS EACH FILLED ROW REACHABLE UNDER THE RULESET THAT FILLS IT?  This is
     the second case, and it is the question that needs the scope rule below.  A row
     filled by `ops_arena` is only any use if some `G_<Row>()` call can execute
     while `arena` is the active ruleset.

     THE SCOPE RULE IS DERIVED, NOT LISTED: a table's own directory IS its
     ruleset's private subtree, because that is where the ruleset's code lives.
     `ops_arena` is defined in `src/arena/`, `ops_ctf` in `src/ctf/`,
     `ops_tourney` in `src/tourney/`; `ops_base` and `ops_sp` are defined at the
     top level and have no private subtree.  Every other directory --
     `src/`, `src/rogue/`, `src/xatrix/`, `src/bot/`, `src/shared/` -- is
     SHARED, which is correct: the content layers and the bot layer are
     orthogonal to the ruleset.  So a call in a shared file serves
     every ruleset, and a call in `src/tourney/` serves only the OSP four.

     The second case is exactly this check failing: `EndLevel` filled by `ops_arena`,
     every `G_EndLevel()` call in `src/tourney/`.

  D. IS EVERY BY-NAME CALL TO A BASE IMPLEMENTATION A SAME-ROW FALLBACK?
     `ops_base`'s five functions are called by name on purpose, and row inheritance
     is why: an override that has nothing better to offer ends with "use
     baseq2's", and `RA_EndLevel`, `OSP_EndLevel`, `RA_CheckRules`,
     `ctf_CheckRules` and `ctf_SelectSpawnPoint` all do exactly that.  What
     makes those legitimate is that each calls the base implementation of the
     SAME ROW it is itself filling.

     A call from anywhere else reaches past whatever the active ruleset put in
     the row.  All three historical defects are that shape and all three are
     caught here: `CheckDMRules` (row CheckRules) calling `EndDMLevel`
     (row EndLevel) is a cross-row call; `EndDMLevel` (row EndLevel) calling
     `BeginIntermission` (row BeginIntermission) is a cross-row call; and
     `OSP_map_vote` fills no row at all.

  E. DOES AN OVERRIDE KEEP A TEST THE BASE MAKES FIRST?  Filling a row takes
     over the base implementation's job, and a job can have a precondition.
     `EndLevel` is the worked case: `dmflags` DF_SAME_LEVEL ends a level on
     the map it started on, and `EndDMLevel` tests it before it looks at any
     rotation.  A row that brings a rotation of its own is subordinate to the
     same test -- and hoisting a rotation UP out of the base and into a row is
     the move that lifts it over a test that used to sit above it, which is
     why this is a question about order and not about presence.

     Both halves are literals, so the order is checkable: in the body of a
     function filling `EndLevel`, `DF_SAME_LEVEL` must appear before the first
     `G_BeginIntermission(`, which is the call that commits to a map.  A row
     that commits to no map of its own is not asked.

WHAT IS EXEMPT, and each one has to say why.  A stale exemption -- naming a
call that is no longer there -- is a finding in its own right, because it is a
statement about the tree.

USAGE
    tools/dispatch.py [--tree src]
    tools/dispatch.py --selftest
"""
import argparse
import os
import re
import shutil
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# (file, enclosing function, base implementation): why this by-name call stands.
EXEMPT_CALLS = {
    ('rogue/dm_tag.c', 'Tag_PostInitSetup', 'SelectSpawnPoint'):
        "places the tag TOKEN, which has no ->client; the dispatcher would "
        "reach SelectCTFSpawnPoint, whose first line is "
        "ent->client->resp.ctf_state -- a server crash, not a better spawn",
    ('rogue/dm_ball.c', 'DBall_SelectSpawnPoint', 'SelectSpawnPoint'):
        "same-row fallback for the DMGame vtable rather than for "
        "ruleset_ops_t, and unreachable for the reason above",
}

# NOT EXEMPT, and worth saying so here because it is the one that looks as if it
# should be.  OSP_setStats, OSP_highscores_cmd and OSP_showinfo_cmd each select
# a page in `resp.osp_r24c` and then draw it, and the donor draws it by calling
# `DeathmatchScoreboardMessage` -- which is the donor's name for its OWN
# five-page dispatcher, not for baseq2's board.  Here that dispatcher is the
# ScoreboardMessage row, so all three go through `G_ScoreboardMessage()` and
# question D has nothing to forgive (R-OSP-14).
#
# An exemption is a claim about a donor, so it is only ever as good as the donor
# text behind it.  That one would have had the donor's source against it.

# Question E, spelled out.  `ENDLEVEL_GUARD` is the precondition and
# `ENDLEVEL_COMMIT` is the act it guards; the row is the one that has both
# today.  Widening this to a second row means a second triple here, not a
# second checker.
ENDLEVEL_ROW = 'EndLevel'
ENDLEVEL_GUARD = 'DF_SAME_LEVEL'
ENDLEVEL_COMMIT = re.compile(r'\bG_BeginIntermission\s*\(')

# A row every table leaves NULL is reported as a note rather than a finding:
# it is a hook placed ahead of its first user, which the struct's own comment
# asks not to happen, but it is not a behaviour that fails to run.
NOTE_UNFILLED = True


def mask(t):
    """Blank comments and string bodies, preserving every byte offset."""
    out = list(t)
    i, n = 0, len(t)
    while i < n:
        c = t[i]
        if c == '/' and i + 1 < n and t[i + 1] == '*':
            j = t.find('*/', i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if out[k] != '\n':
                    out[k] = ' '
            i = j
        elif c == '/' and i + 1 < n and t[i + 1] == '/':
            j = t.find('\n', i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = ' '
            i = j
        elif c in '"\'':
            q, j = c, i + 1
            while j < n:
                if t[j] == '\\':
                    j += 2
                    continue
                if t[j] == q:
                    break
                j += 1
            for k in range(i + 1, min(j, n)):
                if out[k] != '\n':
                    out[k] = ' '
            i = min(j + 1, n)
        else:
            i += 1
    return ''.join(out)


IDENT_BEFORE_PAREN = re.compile(r'([A-Za-z_]\w*)\s*$')


def functions(masked):
    """[(name, body_start, body_end)] for top-level `foo(...)\\n{ ... }`."""
    out = []
    depth = 0
    i, n = 0, len(masked)
    while i < n:
        c = masked[i]
        if c == '{':
            if depth == 0:
                # A definition's `{` is preceded, past whitespace, by the `)`
                # that closes its parameter list.  `= {` is an initialiser and
                # is skipped by exactly this test.
                j = i - 1
                while j >= 0 and masked[j] in ' \t\r\n':
                    j -= 1
                if j >= 0 and masked[j] == ')':
                    k, d = j, 0
                    while k >= 0:
                        if masked[k] == ')':
                            d += 1
                        elif masked[k] == '(':
                            d -= 1
                            if d == 0:
                                break
                        k -= 1
                    m = IDENT_BEFORE_PAREN.search(masked[:k]) if k >= 0 else None
                    if m:
                        start = i
                        d2, p = 0, i
                        while p < n:
                            if masked[p] == '{':
                                d2 += 1
                            elif masked[p] == '}':
                                d2 -= 1
                                if d2 == 0:
                                    break
                            p += 1
                        out.append((m.group(1), start, p))
            depth += 1
        elif c == '}':
            depth -= 1
        i += 1
    return out


def enclosing(funcs, pos):
    for name, a, b in funcs:
        if a <= pos <= b:
            return name
    return None


STRUCT = re.compile(r'typedef struct \{(.*?)\}\s*ruleset_ops_t;', re.S)
ROWMEM = re.compile(r'\(\s*\*\s*(\w+)\s*\)\s*\(')
TABLE = re.compile(r'(?:static\s+)?const\s+ruleset_ops_t\s+(ops_\w+)\s*=\s*\{(.*?)\n\};', re.S)
ROWSET = re.compile(r'\.\s*(\w+)\s*=\s*([A-Za-z_]\w*)')


def sources(tree):
    out = {}
    for root, _, files in os.walk(tree):
        for f in sorted(files):
            if f.endswith(('.c', '.h')):
                p = os.path.join(root, f)
                rel = os.path.relpath(p, tree).replace(os.sep, '/')
                out[rel] = open(p, encoding='utf-8', errors='replace').read()
    return out


def scan(tree):
    src = sources(tree)
    masked = {r: mask(t) for r, t in src.items()}
    funcs = {r: functions(m) for r, m in masked.items()}
    hits = []
    notes = []

    if 'g_ruleset.h' not in src or 'g_ruleset.c' not in src:
        return ['!! dispatch: no g_ruleset.[ch] under the tree'], []

    m = STRUCT.search(masked['g_ruleset.h'])
    if not m:
        return ['!! dispatch: ruleset_ops_t not found in g_ruleset.h'], []
    rows = ROWMEM.findall(m.group(1))
    if not rows:
        return ['!! dispatch: ruleset_ops_t has no function-pointer rows'], []

    # Every table, and the directory it is defined in -- which IS its
    # ruleset's private subtree (see the header).
    tables = {}
    for rel, mt in masked.items():
        if not rel.endswith('.c'):
            continue
        for tm in TABLE.finditer(mt):
            name = tm.group(1)
            filled = {r: impl for r, impl in ROWSET.findall(tm.group(2))
                      if r in rows and impl != 'NULL'}
            d = os.path.dirname(rel)
            tables[name] = {'file': rel, 'dir': d, 'filled': filled}

    if not tables:
        return ['!! dispatch: no ruleset_ops_t table found'], []

    base = tables.get('ops_base', {}).get('filled', {})
    private = {t['dir'] for t in tables.values() if t['dir']}

    # impl -> the row it fills.  One implementation may only fill one row; two
    # rows sharing a function would make check D ambiguous, so say so.
    impl_row = {}
    for tname, t in tables.items():
        for row, impl in t['filled'].items():
            if impl_row.setdefault(impl, row) != row:
                hits.append('!! dispatch: %s fills two different rows (%s and %s)'
                            % (impl, impl_row[impl], row))

    # ---- A. every row has a dispatcher that reads it through GATE()
    disp = {}
    for row in rows:
        want = 'G_' + row
        gate = re.compile(r'GATE\(\s*' + re.escape(row) + r'\s*\)')
        found = None
        for name, a, b in funcs['g_ruleset.c']:
            if name == want and gate.search(masked['g_ruleset.c'][a:b]):
                found = want
                break
        if found:
            disp[row] = want
        else:
            hits.append('!! dispatch: row `%s` has no dispatcher -- expected '
                        '%s() in g_ruleset.c reading GATE(%s) (question A)'
                        % (row, want, row))

    # ---- B/C. call sites of each dispatcher, and their scope
    calls = {}
    for row, fn in disp.items():
        pat = re.compile(r'\b' + re.escape(fn) + r'\s*\(')
        sites = []
        for rel, mt in masked.items():
            if not rel.endswith('.c'):
                continue
            for cm in pat.finditer(mt):
                who = enclosing(funcs[rel], cm.start())
                # `who is None` means file scope -- the definition's own
                # signature or a prototype, not a call.  Counting those was
                # this check's own first bug: the definition of G_EndLevel()
                # lives in a SHARED file, so it made every row look reachable
                # from everywhere and questions B and C could never fire.
                if who is None or who == fn:
                    continue
                sites.append(rel)
        calls[row] = sites
        if not sites:
            hits.append('!! dispatch: %s() is called by nothing -- row `%s` is '
                        'filled by %d table(s) and reached by none (question B)'
                        % (fn, row, sum(1 for t in tables.values()
                                        if row in t['filled'])))

    def serves(rel, tdir):
        # A table with no private subtree is `ops_base` (or `ops_sp`), which no
        # g_ruleset value selects: it is what a NULL row inherits,
        # so it is reachable from wherever any ruleset leaves the row NULL --
        # which is every directory.  Only a ruleset that OWNS a subtree can
        # have a call site that fails to serve it.
        if not tdir:
            return True
        d = os.path.dirname(rel)
        return d not in private or d == tdir

    for tname, t in sorted(tables.items()):
        for row in rows:
            if row not in t['filled'] or row not in calls:
                continue
            if not any(serves(rel, t['dir']) for rel in calls[row]):
                where = ', '.join(sorted(set(calls[row]))) or 'nowhere'
                hits.append(
                    '!! dispatch: %s.%s = %s can never run -- every G_%s() call '
                    'is in %s, which does not serve %s (question C)' % (tname, row, t['filled'][row], row, where, tname))

    # ---- a row nothing fills: a note, not a finding
    if NOTE_UNFILLED:
        for row in rows:
            if not any(row in t['filled'] for t in tables.values()):
                notes.append('note: row `%s` is NULL in every table, so G_%s() '
                             'is a no-op for every ruleset -- a hook ahead of '
                             'its first user' % (row, row))

    # ---- D. by-name calls to a base implementation
    used_exempt = set()
    for row, impl in sorted(base.items()):
        pat = re.compile(r'\b' + re.escape(impl) + r'\s*\(')
        for rel, mt in sorted(masked.items()):
            if not rel.endswith('.c'):
                continue
            for cm in pat.finditer(mt):
                who = enclosing(funcs[rel], cm.start())
                if who == impl or who is None:
                    continue        # its own definition, or a declaration
                if impl_row.get(who) == row:
                    continue        # the same-row fallback
                key = (rel, who, impl)
                if key in EXEMPT_CALLS:
                    used_exempt.add(key)
                    continue
                line = mt[:cm.start()].count('\n') + 1
                if who in impl_row:
                    role = ('it fills row `%s`, so this is a CROSS-ROW call '
                            'and not a fallback' % impl_row[who])
                else:
                    role = 'it fills no ruleset_ops_t row, so it has no '\
                           'fallback to make'
                over = sorted(t for t, v in tables.items()
                              if row in v['filled'] and t != 'ops_base')
                hits.append(
                    '!! %s:%d: %s() calls %s() by name -- %s.  `%s` is '
                    'overridden by %s, so this reaches past whatever the '
                    'active ruleset put in the row.  Use G_%s(), or exempt it '
                    'with a reason (question D)'
                    % (rel, line, who, impl, role, row,
                       ', '.join(over) if over else 'nothing today',
                       row))

    # ---- E. an EndLevel override may not commit to a map before asking the
    # test its base asks first.
    impl_def = {}
    for rel, fl in funcs.items():
        for name, a, b in fl:
            impl_def.setdefault(name, (rel, a, b))

    seen_impl = set()
    for tname, t in sorted(tables.items()):
        impl = t['filled'].get(ENDLEVEL_ROW)
        if not impl or impl in seen_impl:
            continue
        seen_impl.add(impl)
        if impl not in impl_def:
            hits.append('!! dispatch: %s.%s = %s has no definition under the '
                        'tree, so question E cannot be asked of it'
                        % (tname, ENDLEVEL_ROW, impl))
            continue
        rel, a, b = impl_def[impl]
        body = masked[rel][a:b]
        commit = ENDLEVEL_COMMIT.search(body)
        if not commit:
            continue        # commits to no map of its own
        guard = body.find(ENDLEVEL_GUARD)
        if guard >= 0 and guard < commit.start():
            continue
        line = masked[rel][:a + commit.start()].count('\n') + 1
        hits.append(
            '!! %s:%d: %s() fills row `%s` and commits to a map without '
            'asking %s first -- the base it overrides (%s) tests that flag '
            'before any rotation, so a rotation lifted into the row is lifted '
            'over the test.  Ask it, or delegate the choice (question E)'
            % (rel, line, impl, ENDLEVEL_ROW, ENDLEVEL_GUARD,
               base.get(ENDLEVEL_ROW, 'the base')))

    for key, why in sorted(EXEMPT_CALLS.items()):
        if key not in used_exempt:
            hits.append('!! dispatch: STALE EXEMPTION %s:%s -> %s() -- that '
                        'call is not in the tree any more, so the exemption is '
                        'a false statement about it (%s)'
                        % (key[0], key[1], key[2], why))

    return hits, notes


# ------------------------------------------------------------------ selftest

def _mutate(tree, rel, old, new):
    """Replace EVERY occurrence: a control that changes one of three call
    sites leaves the other two answering the question and proves nothing."""
    p = os.path.join(tree, rel)
    t = open(p, encoding='utf-8').read()
    assert old in t, '%s: control anchor missing: %r' % (rel, old)
    open(p, 'w', encoding='utf-8').write(t.replace(old, new))


def selftest():
    real = os.path.join(REPO, 'src')
    ok = True

    hits, notes = scan(real)
    if hits:
        print('!! selftest: the real tree reports %d finding(s); a control run '
              'from a red tree proves nothing' % len(hits))
        for h in hits:
            print('   ' + h)
        return False
    print('   [control] the tree as it stands: 0 findings')
    for n in notes:
        print('   ' + n)

    # Each control is a mutation of the real tree that MUST change the
    # verdict, and the three historical defects are three of them: a check that
    # cannot reproduce the bug it was written for is not a check.  `_mutate`
    # replaces EVERY occurrence, so a control that leaves two of three call
    # sites intact cannot pass by accident.
    controls = [
        # The second case as it actually shipped: the level-end limits stop dispatching.
        ('the level-end limits stop dispatching',
         [('g_main.c', 'G_EndLevel();', 'EndDMLevel();')],
         'question D'),
        # Its other half.  The dispatcher survives and keeps a caller, but
        # every caller that can run under `arena` goes away, so ops_arena's row
        # is filled and unreachable.
        #
        # THREE FILES, and the third is the control on the control.  Every
        # shared directory serves every ruleset, so `rogue/dm_ball.c`'s
        # `G_EndLevel()` -- the DMGame rules row ending the level through the
        # dispatch -- keeps the arena row reachable on its own; a mutation that
        # left it standing would report nothing and say only that the tool
        # cannot count.
        ('every G_EndLevel() caller that serves arena goes away',
         [('g_main.c', 'G_EndLevel();', 'G_EndLevelShim();'),
          ('ctf/g_ctf.c', 'G_EndLevel();', 'G_EndLevelShim();'),
          ('rogue/dm_ball.c', 'G_EndLevel();', 'G_EndLevelShim();')],
         'question C'),
        # The first case as it actually shipped, one row over.
        ('EndDMLevel begins the intermission by name',
         [('g_main.c', 'G_BeginIntermission(', 'BeginIntermission(')],
         'question D'),
        # The third case as it actually shipped.
        ('a tourney command ends the level by name',
         [('tourney/osp_cmds.c', 'G_EndLevel();', 'EndDMLevel();')],
         'question D'),
        # A dispatcher stops reading its row: an override can never be
        # selected again, and the base is hard-wired in its place.
        ('a dispatcher stops reading its row through GATE()',
         [('g_ruleset.c',
           'if (GATE(ScoreboardMessage))\n        GATE(ScoreboardMessage)(ent, killer);',
           'DeathmatchScoreboardMessage(ent, killer);')],
         'question A'),
        # A filled row whose dispatcher nobody calls at all.  FOUR FILES, and
        # the count is the control: `G_ScoreboardMessage()` has callers inside
        # `src/tourney/` as well as in the two spine files, so a mutation that
        # leaves those standing has not removed EVERY caller -- the tool then
        # reports the arena and ctf rows as unreachable (question C), which is
        # true and is not what this control is asking.
        ('a dispatcher loses every caller',
         [('p_hud.c', 'G_ScoreboardMessage(', 'Shim_ScoreboardMessage('),
          ('p_view.c', 'G_ScoreboardMessage(', 'Shim_ScoreboardMessage('),
          ('tourney/osp_cmds.c', 'G_ScoreboardMessage(', 'Shim_ScoreboardMessage('),
          ('tourney/osp_main.c', 'G_ScoreboardMessage(', 'Shim_ScoreboardMessage(')],
         'question B'),
        # Two rows sharing one implementation makes question D ambiguous --
        # "same row" stops having one answer -- so it is a finding of its own.
        ('one function is made to fill two rows',
         [('arena/arena.c', '.EndLevel          = RA_EndLevel,',
           '.EndLevel          = RA_EndLevel,\n    .BeginIntermission = RA_EndLevel,')],
         'two different rows'),
        # THE NEGATIVE CONTROL, and the one that decides whether this check is
        # usable at all: the same-row fallback must not be reported.
        # RA_ScoreboardMessage fills row ScoreboardMessage and has no fallback
        # today; giving it baseq2's is exactly what an override is allowed to
        # do, and a check that flags it would have to be switched off.
        ('a same-row fallback is added to an override that had none',
         [('arena/arena.c',
           'void RA_ScoreboardMessage(edict_t *ent, edict_t *killer)\n{',
           'void RA_ScoreboardMessage(edict_t *ent, edict_t *killer)\n{\n'
           '    if (!arenas) { DeathmatchScoreboardMessage(ent, killer); return; }')],
         None),
        # Question E, once per overriding row, because the two rows reach the
        # same mistake from different code and a control on one says nothing
        # about the other.  `if (1)` keeps the rotation and deletes only the
        # test above it, which is the shape being guarded against.
        ('the arena row rotates without asking DF_SAME_LEVEL',
         [('arena/arena.c', 'if (!((int)dmflags->value & DF_SAME_LEVEL)) {',
           'if (1) {')],
         'question E'),
        ('the tourney row rotates without asking DF_SAME_LEVEL',
         [('tourney/osp_main.c',
           'if (!(((int)dmflags->value & DF_SAME_LEVEL) && manual_map != 1)) {',
           'if (1) {')],
         'question E'),
        # THE SECOND NEGATIVE CONTROL, and question E's own: a row that commits
        # to no map of its own cannot get the order wrong and must not be asked
        # to prove it.  Both halves go, because a row with the test and no
        # commit would pass for the wrong reason.
        ('an EndLevel row that commits to no map of its own',
         [('arena/arena.c', 'if (!((int)dmflags->value & DF_SAME_LEVEL)) {',
           'if (1) {'),
          ('arena/arena.c', 'G_BeginIntermission(CreateTargetChangeLevel(next));',
           'EndDMLevel();')],
         None),
        # The DMGame rules row ending the level by name.  It carried an
        # exemption while `case RDM_DEATHBALL` was the only thing that could
        # reach it; the line dispatches now, so the exemption is gone and this
        # is what keeps it gone.
        ('the deathball rules row ends the level by name',
         [('rogue/dm_ball.c', 'G_EndLevel();', 'EndDMLevel();')],
         'question D'),
        # An exemption that no longer names a real call is a false statement
        # about the tree, so it fails on its own.
        ('an exempt call is deleted, leaving its exemption stale',
         [('rogue/dm_tag.c', 'SelectSpawnPoint(e, origin, angles);',
           'VectorClear(origin); VectorClear(angles);')],
         'STALE EXEMPTION'),
    ]

    for title, muts, want in controls:
        tmp = tempfile.mkdtemp()
        try:
            t = os.path.join(tmp, 'src')
            shutil.copytree(real, t)
            for rel, old, new in muts:
                _mutate(t, rel, old, new)
            got, _ = scan(t)
            if want is None:
                if got:
                    print('!! selftest: [%s] was reported and must not be' % title)
                    for g in got:
                        print('   ' + g)
                    ok = False
                else:
                    print('   [control] %s -> 0 findings, correctly' % title)
            else:
                match = [g for g in got if want in g]
                if not match:
                    print('!! selftest: [%s] was NOT caught (wanted %r)'
                          % (title, want))
                    for g in got:
                        print('   ' + g)
                    ok = False
                else:
                    print('   [control] %s -> caught' % title)
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
    hits, notes = scan(os.path.abspath(a.tree))
    for n in notes:
        print(n)
    for h in hits:
        print(h)
    print('dispatch: %d finding(s), %d exemption(s) on record'
          % (len(hits), len(EXEMPT_CALLS)))
    return 1 if hits else 0


if __name__ == '__main__':
    sys.exit(main())
