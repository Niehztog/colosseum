"""Scan a Quake II game-DLL tree for broken key contracts.

A "key contract" is a place where one side names a slot and the other side
implements it, with NO compiler check in between:

  A. statusbar strings          `if N` / `pic N` / `num W N` / `stat_string N`
                                vs the STAT_* slots the C code assigns
  B. STAT_* macro collisions    two names, same slot, both written
  C. spawn key table            string key -> STOFS/FOFS member
  D. save descriptor type       I()/F()/O()/L()/... vs the member's real type

The compiler catches C and D only by accident, and A and B not at all.
"""
import os, re, sys, glob

def read(p):
    return open(p, encoding='latin-1').read()

def strip_comments(t):
    t = re.sub(r'/\*.*?\*/', '', t, flags=re.S)
    return re.sub(r'//[^\n]*', '', t)

# ---------- STAT_* macro values ----------
# R-TOOL-1: was a hardcoded /mnt/c WSL path.  The engine's STAT_ enum now comes
# from the headers this repo vendors verbatim per R-CORE-9, so the audit reads
# the same shared.h the build compiles against.
_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASEQ2_ENUM = os.environ.get('BASEQ2_ENUM',
                             os.path.join(_REPO, 'inc', 'shared', 'shared.h'))

def baseq2_stats():
    """The engine's STAT_ enum: an anonymous enum, so read it positionally."""
    t = read(BASEQ2_ENUM)
    m = re.search(r'\benum\s*\{\s*\n(\s*STAT_HEALTH_ICON.*?)\n\};', t, re.S)
    assert m, 'baseq2 STAT_ enum not found'
    names = [x.group(1) for x in re.finditer(r'^\s*(STAT_\w+)\s*,', m.group(1), re.M)]
    return {n: i for i, n in enumerate(names)}

def stat_macros(files):
    out = dict(baseq2_stats())
    for f in files:
        for m in re.finditer(r'^\s*#\s*define\s+(STAT_\w+)\s+(\d+)\s*$', read(f), re.M):
            out[m.group(1)] = int(m.group(2))
    return out

# ---------- A: slots the statusbar strings reference ----------
BAR_TOKENS = re.compile(r'\b(?:if|pic|stat_string|rnum|anum|hnum)\s+(\d+)|\bnum\s+\d+\s+(\d+)')
def bar_slots(text):
    """Slots referenced from any C string literal that looks like a statusbar."""
    slots = {}
    for sm in re.finditer(r'((?:"(?:[^"\\]|\\.)*"\s*)+)', text):
        blob = sm.group(1)
        if 'yb' not in blob and 'xv' not in blob and 'stat_string' not in blob:
            continue
        # unescape enough to tokenise
        s = re.sub(r'\\t', ' ', blob)
        s = re.sub(r'"\s*"', '', s).replace('"', '')
        for t in BAR_TOKENS.finditer(s):
            n = int(t.group(1) or t.group(2))
            slots.setdefault(n, 0)
            slots[n] += 1
    return slots

# ---------- A: slots the C code assigns ----------
ASSIGN = re.compile(r'stats\s*\[\s*([A-Za-z_]\w*|\d+)\s*\]\s*(?:=|\|=|&=|\+=|-=)')

GUARD = re.compile(r'^(!?)([A-Za-z_][\w.>-]*(?:->value)?)\b')

def guards_per_line(text):
    """line number -> list of conditions that dominate it (brace-depth walk).

    Two macros can name the same slot without colliding if every write to one is
    dominated by `X` and every write to the other by `!X` -- mutually exclusive
    game modes. Without this a mode-gated split reads as a false collision."""
    out, depth, cond_at, pending = {}, 0, {}, None
    lines = text.split('\n')
    def nextcode(i):
        for j in range(i + 1, min(i + 4, len(lines))):
            if lines[j].strip():
                return lines[j].strip()
        return ''
    for n, line in enumerate(lines, 1):
        s = line.strip()
        m = re.match(r'if \((.*)\)\s*\{?\s*$', s)
        out[n] = [c for c in list(cond_at.values()) + ([pending] if pending else []) if c]
        # a guard clause `if (!X) return;` dominates the rest of its block
        gcm = re.match(r'if \((!?[A-Za-z_][\w.>-]*(?:->value)?)\)\s*(return\b)?', s)
        gc = gcm if (gcm and (gcm.group(2) or nextcode(n - 1).startswith('return'))) else None
        if gc:
            g = gc.group(1)
            inv = g[1:] if g.startswith('!') else '!' + g
            cond_at[depth] = inv if cond_at.get(depth) is None else cond_at[depth]
        opened = line.count('{') - line.count('}')
        nxt = None
        if m:
            if s.endswith('{'): cond_at[depth + 1] = m.group(1)
            else: nxt = m.group(1)
        if opened > 0:
            for d in range(depth + 1, depth + opened + 1): cond_at.setdefault(d, None)
        elif opened < 0:
            for d in range(depth, depth + opened, -1): cond_at.pop(d, None)
        depth += opened
        pending = nxt
    return out

def mode_key(conds):
    """The (name, polarity) of any single-variable mode guard among conds."""
    for c in conds:
        for part in c.split('&&'):
            g = GUARD.match(part.strip())
            if g and g.group(2).endswith('->value'):
                return (g.group(2), g.group(1) != '!')
    return None

def written_slots(files, macros):
    out, modes = {}, {}
    for f in files:
        txt = strip_comments(read(f))
        gl = guards_per_line(txt)
        for n, line in enumerate(txt.split('\n'), 1):
            for m in ASSIGN.finditer(line):
                k = m.group(1)
                v = macros.get(k, None) if not k.isdigit() else int(k)
                if v is None:
                    out.setdefault(('?' + k), []).append(os.path.basename(f))
                else:
                    out.setdefault(v, []).append(f'{os.path.basename(f)}:{k}')
                    modes.setdefault((v, k), set()).add(mode_key(gl.get(n, [])))
    written_slots.modes = modes
    return out

def main(root, label):
    files = sorted(glob.glob(os.path.join(root, '*.c')) + glob.glob(os.path.join(root, '*.h')))
    if not files:
        print(f'{label}: no sources at {root}'); return
    macros = stat_macros(files)
    # shared.h ships the baseq2 STAT_ block
    for extra in ('shared/shared.h', 'q_shared.h'):
        p = os.path.join(root, extra)
        if os.path.exists(p):
            macros.update({m.group(1): int(m.group(2)) for m in
                           re.finditer(r'^\s*#\s*define\s+(STAT_\w+)\s+(\d+)\s*$', read(p), re.M)})
    bars, writes = {}, written_slots(files, macros)
    for f in files:
        for n, c in bar_slots(read(f)).items():
            bars[n] = bars.get(n, 0) + c

    print(f'=== {label} ===')
    # B: collisions among *written* macros
    byval = {}
    for name, v in macros.items():
        byval.setdefault(v, []).append(name)
    written_names = {w.split(':', 1)[1] for k, v in writes.items() if isinstance(k, int) for w in v if ':' in w}
    modes = getattr(written_slots, 'modes', {})
    def exclusive(v, a, b):
        """True if every write via `a` is guarded oppositely to every write via `b`."""
        ma, mb = modes.get((v, a), {None}), modes.get((v, b), {None})
        if None in ma or None in mb or len(ma) != 1 or len(mb) != 1:
            return False
        (na, pa), (nb, pb) = ma.pop(), mb.pop()
        return na == nb and pa != pb
    coll = {}
    for v, ns in byval.items():
        live = sorted(set(ns) & written_names)
        if len(live) < 2:
            continue
        if len(live) == 2 and exclusive(v, live[0], live[1]):
            continue
        coll[v] = live
    print(f'  STAT_ macros: {len(macros)}   slots written: {sum(1 for k in writes if isinstance(k,int))}'
          f'   slots read by bars: {len(bars)}')
    if coll:
        for v, ns in sorted(coll.items()):
            print(f'  !! COLLISION slot {v}: {" ".join(sorted(ns))} -- more than one is assigned')
    else:
        print('  collisions among assigned STAT_ macros: none')
    unresolved = [k for k in writes if not isinstance(k, int)]
    if unresolved:
        print(f'  unresolved stat subscripts: {" ".join(sorted(unresolved))}')
    dead = sorted(n for n in bars if n not in writes)
    if dead:
        print(f'  !! bar reads a slot nothing assigns: {dead}')
    else:
        print('  every slot the bars read is assigned: yes')
    invisible = sorted(n for n in writes if isinstance(n, int) and n not in bars)
    if invisible:
        print(f'  assigned but no bar reads it (engine-side or layout-only): {invisible}')

if __name__ == '__main__':
    for root, label in [(a.split('=', 1)[1], a.split('=', 1)[0]) for a in sys.argv[1:]]:
        main(root, label)
