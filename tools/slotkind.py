"""The check that missed RA2's slot 20.

A statusbar element declares what KIND of value a slot holds:
  stat_string N  -> a configstring index, drawn as text
  pic N          -> an image index
  num W N        -> a plain number
Writing the wrong kind is a type error across a boundary with no compiler check,
and a macro-name collision test cannot see it when the bar uses a bare number.
"""
import os, re, sys, glob

def read(p): return open(p, encoding='latin-1').read()

def bar_kinds(text):
    """slot -> set of kinds the statusbar strings expect."""
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

def stat_values(files):
    import os
    _repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    t = read(os.environ.get('BASEQ2_ENUM',
                            os.path.join(_repo, 'inc', 'shared', 'shared.h')))
    m = re.search(r'\benum\s*\{\s*\n(\s*STAT_HEALTH_ICON.*?)\n\};', t, re.S)
    names = [x.group(1) for x in re.finditer(r'^\s*(STAT_\w+)\s*,', m.group(1), re.M)]
    out = {n: i for i, n in enumerate(names)}
    for f in files:
        for mm in re.finditer(r'^\s*#\s*define\s+(STAT_\w+)\s+(\d+)\s*$', read(f), re.M):
            out[mm.group(1)] = int(mm.group(2))
    return out

ASSIGN = re.compile(r'stats\s*\[\s*([A-Za-z_]\w*|\d+)\s*\]\s*=\s*([^;]+);')

# A slot's kind travels through variables, not just through the call that
# produced it.  baseq2 caches its icons once at spawn -- `level.pic_health =
# gi.imageindex("i_health")` -- and then writes `level.pic_health` to
# STAT_HEALTH_ICON every frame, so a classifier that only recognises a literal
# gi.imageindex() call sees a plain number being written to a slot the bar draws
# with `pic 0`, and reports a mismatch on pristine, correct code.
#
# That blindness cuts both ways and the false negative is the dangerous half: a
# genuine kind error laundered through one variable is invisible.  So the kinds
# of identifiers are learned from the tree first, and classify() consults them.
#
# This is a different gap from the one R-OSP-7 clause 7 and SPECS.md Phase 3
# name (the pristine-statusbar availability check, still open); found and closed
# 2026-08-21 because it fires on baseq2 and would block Phase 0 forever.
INDEXED = re.compile(r'([A-Za-z_][\w.\[\]]*)\s*=\s*gi\.imageindex\b')


def index_vars(files):
    """Identifiers proven to hold an image index -> 'pic'."""
    out = {}
    for f in files:
        for m in INDEXED.finditer(re.sub(r'//[^\n]*', '', read(f))):
            lval = m.group(1)
            # last component: level.pic_health -> pic_health, a[i].icon -> icon
            name = re.split(r'[.\[]', lval)[-1].strip(']')
            if name:
                out[name] = 'pic'
    return out


def classify(rhs, idx=None):
    if re.search(r'\bgi\.imageindex\b', rhs):                   return 'pic'
    # CONFIG_* is q2pro's name for a configstring index carved out of the
    # CS_ space -- CTF's g_ctf.h defines CONFIG_CTF_MATCH as (CS_AIRACCEL-1) --
    # so it is a configstring index by construction, same as a bare CS_ name.
    if re.search(r'\bCS_\w+|\bCONFIG_\w+|game\.csr\.\w+|configstring', rhs): return 'cs'
    if re.match(r'^\s*0\s*$', rhs):                              return None   # clearing is kind-neutral
    if idx:
        for name, kind in idx.items():
            if re.search(r'\b%s\b' % re.escape(name), rhs):
                return kind
    return 'num'

def run(lbl, d):
    files = sorted(glob.glob(os.path.join(d, '*.c')) + glob.glob(os.path.join(d, '*.h')))
    vals = stat_values(files)
    kinds = {}
    for f in files:
        for s, k in bar_kinds(read(f)).items():
            kinds.setdefault(s, set()).update(k)
    assert kinds, f'{lbl}: no statusbar slots parsed'
    problems = []
    idx = index_vars(files)
    for f in files:
        t = re.sub(r'//[^\n]*', '', read(f))
        for m in ASSIGN.finditer(t):
            key, rhs = m.group(1), m.group(2)
            slot = int(key) if key.isdigit() else vals.get(key)
            if slot is None or slot not in kinds:
                continue
            got = classify(rhs, idx)
            if got and got not in kinds[slot]:
                problems.append((slot, key, "/".join(sorted(kinds[slot])), got,
                                 os.path.basename(f), rhs.strip()[:52]))
    print(f'{lbl:8} {len(kinds):2} bar slots typed  ->  {len(problems)} kind mismatch(es)')
    for slot, key, want, got, f, rhs in sorted(set(problems)):
        print(f'         !! slot {slot} ({key}): bar wants {want}, {f} writes {got}: {rhs}')

if __name__ == '__main__':
    # R-TOOL-1: was a hardcoded list of six /mnt/c donor trees.  Now takes
    # label=path pairs, the same CLI shape keycontract.py already had, and
    # defaults to this repo's own tree.
    import os
    args = sys.argv[1:]
    if not args:
        repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        args = ['colosseum=' + os.path.join(repo, 'src')]
    for a in args:
        lbl, d = a.split('=', 1) if '=' in a else ('tree', a)
        run(lbl, d)
