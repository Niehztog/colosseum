"""The check that missed RA2's slot 20 -- and now also the check for WHY.

A statusbar element declares what KIND of value a slot holds:
  stat_string N  -> a configstring index, drawn as text
  pic N          -> an image index
  num W N        -> a plain number
Writing the wrong kind is a type error across a boundary with no compiler check,
and a macro-name collision test cannot see it when the bar uses a bare number.

TWO CHECKS, and the second one is what R-OSP-7 clause 7 and SPECS.md Phase 3
mean by "close slotkind.py's known gap".

  KIND      does the code write a slot as the kind the bar draws it?
  AVAILABLE was the slot free to be claimed at all?

The kind check alone found RA2's slot-20 collision only by luck -- the two
claimants happened to disagree about kind.  Had both been `pic`, a slot with two
live meanings would have passed.  The availability check keys on the NUMBER, not
on the name and not on the kind:

  * two names that resolve to one slot, both assigned in the tree, is a
    collision whatever their kinds are;
  * a slot the *pristine* statusbar already draws by its shared.h name is not
    available through the map, even when both agree on the number -- a slot with
    two owners is the ambiguity R-OSP-7a exists to remove;
  * a slot below 16 is never claimable, and a slot at or above MAX_STATS_OLD is
    only reachable for a client that negotiated the protocol extension
    (clause 6, R-COMPAT-5).

TWO TREE SHAPES.  A donor ships `#define STAT_X N` plus a statusbar string
literal; Colosseum ships g_stats.h's STATSLOT_MAP and g_stats.c's `sb_*`
emitter (R-OSP-7a).  Both are audited, because the donor trees stay in the
repository as controls -- `q2pro/src/ctf` must keep failing on the slot-17
double assignment (doc/regression.md), and a check that cannot fail is not a
check.  `--selftest` proves the map-shape checks can fail too, by mutating the
real map and the real emitter five ways and asserting each one is caught.  It
runs in `make check`, so a control cannot rot separately from its check.
"""
import os, re, sys, glob

def read(p): return open(p, encoding='latin-1').read()

def bar_kinds(text):
    """slot -> set of kinds the statusbar STRING LITERALS expect (donor shape)."""
    out = {}
    for sm in re.finditer(r'((?:"(?:[^"\\]|\\.)*"\s*)+)', text):
        blob = sm.group(1)
        if not re.search(r'\byb\b|\bxv\b|stat_string', blob):
            continue
        s = re.sub(r'\\t', ' ', blob)
        s = re.sub(r'"\s*"', '', s).replace('"', '')
        for m in re.finditer(r'\bstat_string\s+(\d+)', s):
            out.setdefault(int(m.group(1)), set()).add('cs')
        for m in re.finditer(r'\bpic\s+(\d+)', s):
            out.setdefault(int(m.group(1)), set()).add('pic')
        for m in re.finditer(r'\bnum\s+\d+\s+(\d+)', s):
            out.setdefault(int(m.group(1)), set()).add('num')
    return out

def baseq2_enum():
    _repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    t = read(os.environ.get('BASEQ2_ENUM',
                            os.path.join(_repo, 'inc', 'shared', 'shared.h')))
    m = re.search(r'\benum\s*\{\s*\n(\s*STAT_HEALTH_ICON.*?)\n\};', t, re.S)
    names = [x.group(1) for x in re.finditer(r'^\s*(STAT_\w+)\s*,', m.group(1), re.M)]
    return {n: i for i, n in enumerate(names)}

def stat_values(files):
    out = baseq2_enum()
    for f in files:
        for mm in re.finditer(r'^\s*#\s*define\s+(STAT_\w+)\s+(\d+)\s*$', read(f), re.M):
            out[mm.group(1)] = int(mm.group(2))
    return out

MAX_STATS_OLD = 32
MAX_STATS_NEW = 64
UNIVERSAL_TOP = 16      # slots 0..15 are universal and unclaimable (clause 1)

ASSIGN = re.compile(r'stats\s*\[\s*([A-Za-z_]\w*|\d+)\s*\]\s*=\s*([^;]+);')

# A slot's kind travels through variables, not just through the call that
# produced it.  baseq2 caches its icons once -- `level.pic_health =
# gi.imageindex("i_health")` -- and then writes `level.pic_health` to
# STAT_HEALTH_ICON every frame, so a classifier that only recognises a literal
# gi.imageindex() call sees a plain number being written to a slot the bar draws
# with `pic 0`, and reports a mismatch on pristine, correct code.
#
# That blindness cuts both ways and the false negative is the dangerous half: a
# genuine kind error laundered through one variable is invisible.  So the kinds
# of identifiers are learned from the tree first, and classify() consults them.
INDEXED = re.compile(r'([A-Za-z_][\w.\[\]]*)\s*=\s*gi\.imageindex\b')


# A second hop.  CTF caches its flag icons once (`imageindex_i_ctf1 =
# gi.imageindex("i_ctf1")`), then copies one of six into a local (`p1 =
# imageindex_i_ctf1d`), then writes the local.  One hop of learning sees the
# cache and not the local, so it reported pristine CTF code as writing `num` to
# a `pic` slot -- the same false positive as the original indirection blindness,
# one level further out.  Propagating to a fixed point also closes the matching
# false negative: a genuine kind error laundered through two variables.
COPY = re.compile(r'\b([A-Za-z_]\w*)\s*=\s*([A-Za-z_][\w.\[\]]*)\s*;')


def index_vars(files):
    """Identifiers proven to hold an image index -> 'pic'."""
    out = {}
    texts = [re.sub(r'//[^\n]*', '', read(f)) for f in files]
    for t in texts:
        for m in INDEXED.finditer(t):
            lval = m.group(1)
            # last component: level.pic_health -> pic_health, a[i].icon -> icon
            name = re.split(r'[.\[]', lval)[-1].strip(']')
            if name:
                out[name] = 'pic'
    for _ in range(4):          # bounded: four hops is far past any real chain
        grew = False
        for t in texts:
            for m in COPY.finditer(t):
                dst, src = m.group(1), re.split(r'[.\[]', m.group(2))[-1].strip(']')
                if src in out and dst not in out:
                    out[dst] = out[src]
                    grew = True
        if not grew:
            break
    return out


def classify(rhs, idx=None):
    if re.search(r'\bgi\.imageindex\b', rhs):                   return 'pic'
    # CONFIG_* is q2pro's name for a configstring index carved out of the
    # CS_ space -- CTF's g_ctf.h defines CONFIG_CTF_MATCH as (CS_AIRACCEL-1) --
    # so it is a configstring index by construction, same as a bare CS_ name.
    # OSP_CS(n) is tourney's spelling of the same thing -- (game.csr.general + n)
    # -- and OSP_setID/OSP_changeID return a playerskins index.  A helper that
    # RETURNS a configstring index is one by construction, exactly as CONFIG_*
    # is one by definition; without these three the check reports twelve
    # kind mismatches that are all the tool not knowing the donor's spelling.
    if re.search(r'\bCS_\w+|\bCONFIG_\w+|game\.csr\.\w+|configstring'
                 r'|\bOSP_CS\s*\(|\bOSP_(set|change)ID\b', rhs): return 'cs'
    if re.match(r'^\s*0\s*$', rhs):                              return None   # clearing is kind-neutral
    if idx:
        for name, kind in idx.items():
            if re.search(r'\b%s\b' % re.escape(name), rhs):
                return kind
    return 'num'


# ------------------------------------------------------------------ donor shape

def run_literal(lbl, files, out):
    """Donor shape: `#define STAT_X N` plus statusbar string literals."""
    vals = stat_values(files)
    kinds = {}
    for f in files:
        for s, k in bar_kinds(read(f)).items():
            kinds.setdefault(s, set()).update(k)
    if not kinds:
        return False        # not this shape

    idx = index_vars(files)
    written = {}            # slot -> {name -> set(files)}
    problems = []
    for f in files:
        t = re.sub(r'//[^\n]*', '', read(f))
        for m in ASSIGN.finditer(t):
            key, rhs = m.group(1), m.group(2)
            slot = int(key) if key.isdigit() else vals.get(key)
            if slot is None:
                continue
            written.setdefault(slot, {}).setdefault(key, set()).add(os.path.basename(f))
            if slot not in kinds:
                continue
            got = classify(rhs, idx)
            if got and got not in kinds[slot]:
                problems.append(f'slot {slot} ({key}): bar wants '
                                f'{"/".join(sorted(kinds[slot]))}, '
                                f'{os.path.basename(f)} writes {got}: '
                                f'{rhs.strip()[:52]}')

    # THE AVAILABILITY HALF.  Two distinct names on one slot, both assigned, is
    # a collision by number -- which is what the kind check could not see.
    for slot in sorted(written):
        names = sorted(written[slot])
        if len(names) > 1:
            where = ', '.join(f'{n} ({"/".join(sorted(written[slot][n]))})'
                              for n in names)
            problems.append(f'slot {slot} is double-assigned: {where} '
                            f'-- the slot was not available (R-OSP-7)')
        if slot < UNIVERSAL_TOP and slot not in baseq2_enum().values():
            problems.append(f'slot {slot} ({names[0]}) is inside the universal '
                            f'range 0..{UNIVERSAL_TOP - 1} (R-OSP-7 clause 1)')

    out.append(f'{lbl:10} {len(kinds):2} bar slots typed (literal bars)  ->  '
               f'{len(problems)} problem(s)')
    for p in sorted(set(problems)):
        out.append(f'           !! {p}')
    return True


# ------------------------------------------------------------------- map shape

MAP_ROW = re.compile(r'^\s*E\(\s*(SID_\w+)\s*,\s*(SK_\w+)\s*,\s*'
                     r'([-\d\s,]+?)\)\s*\\?\s*$', re.M)
KIND_OF_SK = {'SK_NUM': 'num', 'SK_PIC': 'pic', 'SK_CS': 'cs'}

# sb_* op -> the kind it draws.  `if` asserts presence only, so it has none.
OP_KIND = {'sb_pic': 'pic', 'sb_num': 'num', 'sb_stat_string': 'cs',
           'sb_upic': 'pic', 'sb_unum': 'num', 'sb_ustat_string': 'cs'}


def parse_map(header):
    """STATSLOT_MAP -> (column names, {sid: (kind, {ruleset: slot})})."""
    sig = re.search(r'#define\s+STATSLOT_ENUM\(([^)]*)\)', header)
    if not sig:
        return None, None
    cols = [c.strip() for c in sig.group(1).split(',')][2:]   # drop id, kind
    rows = {}
    for m in MAP_ROW.finditer(header):
        sid, sk = m.group(1), m.group(2)
        nums = [int(x) for x in re.findall(r'-?\d+', m.group(3))]
        if len(nums) != len(cols):
            rows[sid] = (KIND_OF_SK.get(sk, sk), None)
            continue
        rows[sid] = (KIND_OF_SK.get(sk, sk), dict(zip(cols, nums)))
    return cols, rows


def parse_emitter(text):
    """Per emitter function: SIDs drawn (with op), raw slots drawn, callees."""
    fns = {}
    for m in re.finditer(r'^(?:static\s+)?void\s+(sb_\w+|G_SetStatusbar)\s*\('
                         r'[^)]*\)\s*\n\{', text, re.M):
        name = m.group(1)
        i, depth = text.index('{', m.start()), 0
        j = i
        while j < len(text):
            if text[j] == '{': depth += 1
            elif text[j] == '}':
                depth -= 1
                if depth == 0:
                    break
            j += 1
        body = re.sub(r'//[^\n]*', '', text[i:j])
        drawn, raw, callees = [], [], []
        for c in re.finditer(r'\b(sb_\w+)\s*\(\s*(?:&?sb)\s*(?:,\s*([^;]*?))?\)', body):
            op, args = c.group(1), (c.group(2) or '')
            sid = re.search(r'\b(SID_\w+)\b', args)
            stat = re.search(r'\b(STAT_\w+)\b', args)
            if op in OP_KIND or op in ('sb_if', 'sb_uif'):
                if sid:
                    drawn.append((op, sid.group(1)))
                elif stat:
                    raw.append((op, stat.group(1)))
        # A callee is any sb_* taking the bar as its first argument, with or
        # without further arguments -- sb_universal grew a shape parameter and
        # sb_compose takes the ruleset, and a pattern anchored on `)` saw
        # neither.
        for c in re.finditer(r'\b(sb_[a-z_]+)\s*\(\s*&?sb\s*[,)]', body):
            if c.group(1) not in ('sb_endif', 'sb_init'):
                callees.append(c.group(1))
        fns[name] = dict(drawn=drawn, raw=raw, callees=callees, body=body)
    return fns


# `sb_x(&sb)`, `sb_x(sb)`, `sb_x(sb, shape)` -- the emitter grew arguments when
# arena's bar needed two coordinates of the shared block to differ, and a
# pattern that only matched `(&sb)` stopped seeing the composition entirely.
EMIT_CALL = re.compile(r'\bsb_(\w+)\s*\(\s*&?sb\s*[,)]')


def compose_body(fns):
    """The function holding the per-ruleset switch.

    It was G_SetStatusbar itself until `sv slots` needed to print the bar it
    would install; composing in two places is how the printed bar and the
    installed one drift, so the switch moved into a helper and this follows it
    rather than assuming a name.
    """
    top = fns.get('G_SetStatusbar', {})
    if 'case RULESET_' in top.get('body', ''):
        return top['body']
    for f in top.get('callees', []):
        b = fns.get(f, {}).get('body', '')
        if 'case RULESET_' in b:
            return b
    return top.get('body', '')


def bars_per_ruleset(fns):
    """ruleset label -> the emitter functions its bar is composed from."""
    top = compose_body(fns)
    always = EMIT_CALL.findall(top.split('switch')[0])
    out, seen_cases = {}, []
    sw = top[top.find('switch'):] if 'switch' in top else ''
    for case in re.finditer(r'case\s+RULESET_(\w+)\s*:(.*?)break;', sw, re.S):
        label = case.group(1).lower()
        seen_cases.append(label)
        out[label] = ['sb_' + a for a in always] + \
                     ['sb_' + f for f in EMIT_CALL.findall(case.group(2))]
    dm = re.search(r'default\s*:(.*?)break;', sw, re.S)
    if dm:
        out['default'] = ['sb_' + a for a in always] + \
                         ['sb_' + f for f in EMIT_CALL.findall(dm.group(1))]
    return out, seen_cases


def expand(fns, roots):
    seen, stack = [], list(roots)
    while stack:
        f = stack.pop(0)
        if f in seen or f not in fns:
            continue
        seen.append(f)
        stack += fns[f]['callees']
    return seen


def sources(files):
    """[(label, text)] with comments stripped -- the writer scan's input."""
    return [(os.path.basename(f), re.sub(r'//[^\n]*', '', read(f))) for f in files]


def run_map(lbl, files, out):
    header = next((f for f in files if os.path.basename(f) == 'g_stats.h'), None)
    impl = next((f for f in files if os.path.basename(f) == 'g_stats.c'), None)
    if not header or not impl:
        return False
    return audit_map(lbl, files, out, read(header), read(impl))


def audit_map(lbl, files, out, header_text, impl_text, extra=None):
    cols, rows = parse_map(header_text)
    if not rows:
        out.append(f'{lbl}: g_stats.h has no STATSLOT_MAP to audit')
        return True

    fns = parse_emitter(impl_text)
    bars, cases = bars_per_ruleset(fns)
    enum = baseq2_enum()
    problems, notes = [], []

    # Which slots does the PRISTINE part of the bar already draw?  Every
    # ruleset composes it, so anything it touches is spoken for.  This is the
    # availability question the old check could not ask: it is answered from the
    # bar by NUMBER, not from the set of STAT_ macro names.
    pristine = {}
    for f, info in fns.items():
        for op, stat in info['raw']:
            if stat in enum:
                pristine[enum[stat]] = stat

    for sid, (kind, slots) in sorted(rows.items()):
        if slots is None:
            problems.append(f'{sid}: map row has the wrong number of columns '
                            f'for {cols}')
            continue
        for rs, slot in slots.items():
            if slot < 0:
                continue
            if slot < UNIVERSAL_TOP:
                problems.append(f'{sid} claims slot {slot} in `{rs}` -- slots '
                                f'0..{UNIVERSAL_TOP - 1} are universal and '
                                f'unclaimable (R-OSP-7 clause 1)')
            if slot in pristine:
                problems.append(f'{sid} claims slot {slot} in `{rs}`, which the '
                                f'pristine statusbar already draws as '
                                f'{pristine[slot]} -- the slot was not '
                                f'available (R-OSP-7 clause 7)')
            if slot >= MAX_STATS_NEW:
                problems.append(f'{sid} claims slot {slot} in `{rs}`, beyond '
                                f'MAX_STATS_NEW ({MAX_STATS_NEW}) '
                                f'(R-OSP-7 clause 6)')
            elif slot >= MAX_STATS_OLD:
                notes.append(f'{sid} sits at slot {slot} in `{rs}`: extension '
                             f'only, dropped without protocol extensions '
                             f'(clause 6, R-COMPAT-5)')

    # No two logical stats on one slot within one ruleset.
    for rs in cols:
        claimed = {}
        for sid, (kind, slots) in rows.items():
            if not slots:
                continue
            slot = slots.get(rs, -1)
            if slot < 0:
                continue
            if slot in claimed:
                problems.append(f'slot {slot} is claimed by both '
                                f'{claimed[slot]} and {sid} in `{rs}` -- one '
                                f'numbering per ruleset (R-OSP-7 clause 3)')
            claimed[slot] = sid

    # The bar's op must match the kind the map declares.  Runtime checks this
    # too (sb_kind_ok) but a build failure beats a dprintf nobody reads.
    for label, roots in sorted(bars.items()):
        rs = label if label in cols else 'dm'
        for f in expand(fns, roots):
            for op, sid in fns[f]['drawn']:
                if sid not in rows:
                    problems.append(f'{f} draws unknown {sid}')
                    continue
                want = OP_KIND.get(op)
                if want and want != rows[sid][0]:
                    problems.append(f'{f} draws {sid} with `{op}` but the map '
                                    f'declares it {rows[sid][0]} '
                                    f'(R-OSP-7 clause 7)')

    # A LOGICAL ID USED AS A SLOT NUMBER.  Neither question above can see this
    # one, and it is the failure R-OSP-7's whole shape exists to prevent: a
    # statslot_t is an ENUM, so `ps.stats[SID_OSP_RUNE_HASTE]` compiles silently
    # and indexes by the id's ordinal in STATSLOT_MAP instead of by the slot the
    # active ruleset assigned it.  The kind check cannot fire -- no kind is
    # named -- and the availability check cannot fire either, because the map
    # itself is correct; only the access bypasses it.
    #
    # Measured on this tree 2026-08-27: fourteen sites in the tourney rune code
    # read ordinals 28..32 where the map had put the runes at 22..26.  Ordinals
    # 29 and 30 are tourney's OWN second-powerup-timer pair, so holding a pent
    # read as holding the STRENGTH and HASTE runes -- doubled damage and haste
    # fire rate from an invulnerability -- while resist, regen and vampire, whose
    # ordinals landed on unassigned slots, could not fire at all.  Ordinal 32
    # was additionally out of bounds on a 32-slot player_state_t, which is how
    # this was found: -Warray-bounds at -O2 under the API=old build (R-ENG-1a).
    # Nothing at the shipped setting had reported it in five phases.
    #
    # G_Stat/G_GetStat/G_SetStat are the only legitimate readers of the map, so
    # the rule is simply that a SID never appears inside stats[].
    sid_index = re.compile(r'stats\s*\[\s*(SID_\w+)\s*\]')
    for name, t in sources(files) + list(extra or []):
        for m in sid_index.finditer(t):
            problems.append(f'{name} indexes stats[] with the logical id '
                            f'{m.group(1)} rather than its resolved slot -- '
                            f'use G_GetStat/G_SetStat (R-OSP-7 clause 4)')

    # ...AND THE MIRROR IMAGE: a slot number where an id belongs.  The accessors
    # take a statslot_t, which is an enum and therefore accepts any int, so
    # `G_SetStat(ent, ent->item->quantity, 1)` compiles and resolves whatever
    # logical id happens to share that ordinal.  That is the other half of R-132:
    # the rune items carried the donor's literal 22 in `quantity` while every
    # consumer had been renamed to `SID_OSP_RUNE_RESIST` (28), so the pickup set
    # statslot_t 22 -- SID_RA_ID_VIEW, unmapped under tourney -- and granted
    # nothing.  A silent no-op is the worst possible symptom: no crash, no
    # warning, and a feature that still reports itself as enabled.
    #
    # NAMING THE INVARIANT TOOK TWO TRIES, and the first one is worth recording
    # because it is the shape of a bad check.  "The id argument must be a `SID_`
    # name" rejects the *fixed* code as loudly as the broken code -- the pickup
    # legitimately carries the id in a field -- and it also flagged the
    # accessors' own `statslot_t id` parameters.  A check that fires on the
    # correct version of the thing it is guarding is not a check, it is a rename.
    #
    # The real invariant is a JOIN between two files: if an item's `quantity` is
    # passed to an accessor, then `quantity` is a statslot_t, so every item that
    # sets it must set it to a `SID_` name and not to a number.  Both halves are
    # checked, and a computed id that is *not* the item field is reported as a
    # note -- it cannot be kind-checked, so it should be visible without being
    # fatal.
    carries_id = False
    accessor = re.compile(r'\bG_(?:Set|Get)Stat\s*\(\s*[^,()]+,\s*([^,()]+?)\s*,')
    for name, t in sources(files) + list(extra or []):
        for m in accessor.finditer(t):
            arg = m.group(1).strip()
            if re.fullmatch(r'SID_\w+', arg):
                continue
            # the accessors' own definitions/declarations: `statslot_t id`
            if re.fullmatch(r'\w+\s+\w+', arg):
                continue
            if re.search(r'\bitem\s*->\s*quantity\b|\bitem\.quantity\b', arg):
                carries_id = True
                continue
            notes.append(f'{name} passes the computed id `{arg}` to an accessor: '
                         f'correct only if it holds a SID_, and its kind cannot '
                         f'be checked here (R-OSP-7 clause 4)')

    # The other half of the join: `quantity` is an id, so it must be spelled as
    # one.  Only for items that actually use it that way -- IT_RUNE is the flag
    # that marks them -- because `quantity` means a count on every other item.
    if carries_id:
        for name, t in sources(files) + list(extra or []):
            # segment the itemlist by .classname so an item's flags and its
            # quantity are read from the same entry
            parts = re.split(r'(?=\.classname\s*=)', t)
            for part in parts:
                if 'IT_RUNE' not in part:
                    continue
                q = re.search(r'\.quantity\s*=\s*([^,]+),', part)
                if not q:
                    continue
                val = q.group(1).strip()
                if re.fullmatch(r'SID_\w+', val):
                    continue
                cls = re.search(r'\.classname\s*=\s*"([^"]*)"', part)
                problems.append(
                    f'{name}: {cls.group(1) if cls else "an IT_RUNE item"} sets '
                    f'.quantity = {val}, but an accessor reads item->quantity as '
                    f'a statslot_t -- it must be a SID_ name (R-OSP-7 clause 4)')

    # And the writers must agree with the map, which is the original kind check
    # re-aimed at G_SetStat.
    idx = index_vars(files)
    setstat = re.compile(r'G_SetStat\s*\(\s*[^,]+,\s*(SID_\w+)\s*,\s*(.+?)\)\s*;',
                         re.S)
    for name, t in sources(files) + list(extra or []):
        for m in setstat.finditer(t):
            sid, rhs = m.group(1), m.group(2)
            if sid not in rows:
                problems.append(f'{name} writes unknown {sid}')
                continue
            got = classify(rhs, idx)
            if got and got != rows[sid][0]:
                problems.append(f'{sid}: map declares {rows[sid][0]}, '
                                f'{name} writes {got}: {rhs.strip()[:52]}')

    missing = [c for c in cols if c not in bars and 'default' not in bars]
    for m in missing:
        notes.append(f'ruleset `{m}` has no bar of its own; it inherits the '
                     f'default (R-MODE-6)')

    out.append(f'{lbl:10} {len(rows):2} mapped stats x {len(cols)} ruleset(s), '
               f'{len(pristine)} pristine slot(s)  ->  {len(problems)} problem(s)')
    for p in sorted(set(problems)):
        out.append(f'           !! {p}')
    for n in sorted(set(notes)):
        out.append(f'           -- {n}')
    return True


def run(lbl, d):
    files = sorted(glob.glob(os.path.join(d, '*.c')) +
                   glob.glob(os.path.join(d, '*.h')) +
                   glob.glob(os.path.join(d, '*', '*.c')) +
                   glob.glob(os.path.join(d, '*', '*.h')))
    out = []
    did = run_map(lbl, files, out)
    did |= run_literal(lbl, files, out)
    if not did:
        out.append(f'{lbl}: no statusbar in any known shape -- nothing audited')
    print('\n'.join(out))


# --------------------------------------------------------------- self-test
#
# R-VER-9 clause 2: a check that has never failed is not trusted.  These mutate
# the real map and the real emitter and assert the audit notices.  They run in
# `make check`, so the controls cannot rot separately from the check.

SELFTESTS = [
    ('collision', 'header',
     (r'E\(SID_CTF_TECH,\s+SK_PIC,\s+-1,\s+26',
      'E(SID_CTF_TECH,              SK_PIC,  -1,  21'),
     'is claimed by both'),
    ('universal', 'header',
     (r'E\(SID_TIMER2_ICON,\s+SK_PIC,\s+18',
      'E(SID_TIMER2_ICON,           SK_PIC,   5'),
     'universal and unclaimable'),
    # Not a slot below 16 -- that arm is clause 1's and has its own control.
    # This one is the boundary the pristine check exists for: a slot owned by
    # the map AND drawn by a shared.h name in the same bar, which is a slot with
    # two owners even when both agree on the number.
    ('pristine', 'impl',
     (r'sb_stat_string\(sb, SID_CHASE\);',
      'sb_ustat_string(sb, STAT_CHASE);'),
     'pristine statusbar already draws'),
    ('bar kind', 'impl',
     (r'sb_num\(sb, 2, SID_CTF_TEAM1_CAPS\);',
      'sb_pic(sb, SID_CTF_TEAM1_CAPS);'),
     'declares it num'),
    ('writer kind', 'inject',
     (None, 'void ctl(edict_t *ent) { '
            'G_SetStat(ent, SID_TIMER2, gi.imageindex("p_quad")); }'),
     'writes pic'),
    # The real bug, as its own control: a SID inside stats[].  Written as the
    # fourteen sites were actually written rather than as a minimal case, so
    # that the control still resembles the thing it is guarding against.
    ('id as index', 'inject',
     (None, 'bool ctl(edict_t *ent) { '
            'return ent->client->ps.stats[SID_OSP_RUNE_HASTE] != 0; }'),
     'rather than its resolved slot'),
    # R-132's other half, as the join it actually is: the accessor reads
    # item->quantity as an id, so an IT_RUNE item that sets quantity to a NUMBER
    # is the defect.  The control supplies the item, because the real accessor
    # call is already in the tree -- which is the point: this control fails only
    # while both halves disagree, exactly as the tree did.
    ('number as id', 'inject',
     (None, 'const gitem_t rune_ctl = { .classname = "item_rune_ctl", '
            '.quantity = 22, .flags = IT_RUNE };'),
     'must be a SID_ name'),
]


def selftest(tree):
    files = sorted(glob.glob(os.path.join(tree, '*.c')) +
                   glob.glob(os.path.join(tree, '*.h')) +
                   glob.glob(os.path.join(tree, '*', '*.c')) +
                   glob.glob(os.path.join(tree, '*', '*.h')))
    hdr = read(os.path.join(tree, 'g_stats.h'))
    imp = read(os.path.join(tree, 'g_stats.c'))
    bad = 0
    for name, where, (pat, repl), expect in SELFTESTS:
        h, i, extra = hdr, imp, []
        if where == 'inject':
            extra = [('control.c', repl)]
        elif where == 'header':
            h, n = re.subn(pat, repl, hdr, count=1)
        else:
            i, n = re.subn(pat, repl, imp, count=1)
        if where != 'inject' and not n:
            print(f'  !! selftest "{name}": its own mutation no longer '
                  f'applies -- the control has rotted')
            bad += 1
            continue
        out = []
        audit_map('control', files, out, h, i, extra)
        text = '\n'.join(out)
        if expect in text:
            print(f'  ok  control "{name}" fires: {expect}')
        else:
            print(f'  !! control "{name}" did NOT fire; the check is asleep')
            bad += 1
    return bad


if __name__ == '__main__':
    # R-TOOL-1: was a hardcoded list of six /mnt/c donor trees.  Now takes
    # label=path pairs, the same CLI shape keycontract.py already had, and
    # defaults to this repo's own tree.
    args = sys.argv[1:]
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if args and args[0] == '--selftest':
        sys.exit(1 if selftest(os.path.join(repo, 'src')) else 0)
    if not args:
        args = ['colosseum=' + os.path.join(repo, 'src')]
    for a in args:
        lbl, d = a.split('=', 1) if '=' in a else ('tree', a)
        run(lbl, d)
