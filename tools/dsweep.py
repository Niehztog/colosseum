"""Mechanism (d), done with real brace matching.

id's WriteEdict/WriteClient dumped whole structs with fwrite, so every member was
persisted implicitly.  q2pro replaced that with a per-member descriptor whitelist.
A mod-added member with no row silently stops surviving a save/load.

Severity split: id's raw dump wrote POINTERS as bit patterns, which could never
survive a reload anyway -- q2pro's F_EDICT/F_ITEM/F_POINTER machinery exists to
relocate those.  Losing a pointer member is not a regression; losing plain
scalar/vector/string state is.
"""
import os, re, subprocess, sys

# A reference is a file on disk, or `git:<repo>@<sha>:<path>` -- a donor read at
# the commit this tree pins, whatever its checkout happens to have on disk.
def _git(p):
    repo, rest = p[4:].split('@', 1)
    sha, path = rest.split(':', 1)
    return repo, sha, path

def read(p):
    if p.startswith('git:'):
        repo, sha, path = _git(p)
        return subprocess.run(['git', '-C', repo, 'show', '%s:%s' % (sha, path)],
                              capture_output=True, check=True).stdout.decode('latin-1')
    return open(p, encoding='latin-1').read()

def exists(p):
    if p.startswith('git:'):
        repo, sha, path = _git(p)
        return os.path.isdir(repo) and subprocess.run(
            ['git', '-C', repo, 'cat-file', '-e', '%s:%s' % (sha, path)],
            capture_output=True).returncode == 0
    return os.path.exists(p)

def ref(d, f):
    return d + ':' + f if d.startswith('git:') else os.path.join(d, f)

def pinned(repo, header, donor):
    """The donor tree at the commit the tree's own file header says it came from."""
    m = re.search(r'from %s@([0-9a-f]{7,40})\b' % re.escape(donor),
                  open(header, encoding='utf-8').read())
    return 'git:%s@%s' % (repo, m.group(1)) if m else 'git:%s@unpinned' % repo

def struct_body(text, name):
    """Body of `typedef struct {...} NAME;` or `struct NAME {...};`, found by
    locating the closing line then brace-matching BACKWARDS to its own `{`."""
    for pat in (r'\n\}\s*%s\s*;' % re.escape(name), ):
        for m in re.finditer(pat, text):
            j = m.start() + 1              # index of the '}'
            d, i = 1, j - 1
            while i >= 0 and d:
                if text[i] == '}': d += 1
                elif text[i] == '{': d -= 1
                i -= 1
            head = text[max(0, i - 120):i + 2]
            if re.search(r'typedef\s+struct\s*(\w+\s*)?\{\s*$', head):
                return text[i + 2:j]
    m = re.search(r'struct\s+%s\s*\{' % re.escape(name), text)
    if m:
        i = m.end(); d = 1; k = i
        while d and k < len(text):
            if text[k] == '{': d += 1
            elif text[k] == '}': d -= 1
            k += 1
        return text[i:k - 1]
    return None

MEMBER = re.compile(
    r'^\s*((?:const\s+|unsigned\s+|struct\s+)*[A-Za-z_]\w*)\s+([*\s]*)([A-Za-z_]\w*)\s*(\[[^\]]*\])?\s*;\s*$')

def members(body):
    out, depth = {}, 0
    for line in body.split('\n'):
        raw = line
        l = re.sub(r'//.*', '', line).strip()
        depth += raw.count('{') - raw.count('}')
        if depth > 0 and not l.endswith(';'):
            continue
        m = MEMBER.match(l)
        if m and m.group(3) not in ('if', 'else', 'return'):
            ptr = '*' in (m.group(2) or '')
            out[m.group(3)] = (m.group(1) + (' *' if ptr else ''), ptr)
    return out

SAVED = ['edict_s', 'gclient_s', 'level_locals_t', 'client_persistant_t',
         'client_respawn_t', 'monsterinfo_t', 'moveinfo_t']

def save_referenced(t):
    out = set()
    for m in re.finditer(r'\b[A-Z]{1,2}\(\s*([A-Za-z_][\w.]*)', t):
        out.add(m.group(1).split('.')[-1])
    for m in re.finditer(r'[A-Z]+OFS\(\s*([A-Za-z_][\w.]*)\s*\)', t):
        out.add(m.group(1).split('.')[-1])
    return out

def run(lbl, base_h, port_h, port_save, verbose=False):
    bh, ph, ps = read(base_h), read(port_h), read(port_save)
    refd = save_referenced(ps)
    assert refd, f'{lbl}: ZERO members extracted from the save tables'
    lost_state, lost_ptr, added_total = [], [], 0
    for s in SAVED:
        bb, pb = struct_body(bh, s), struct_body(ph, s)
        if pb is None:
            print(f'  (note: {lbl} has no struct {s})'); continue
        b = members(bb) if bb else {}
        p = members(pb)
        for m, (ty, ptr) in p.items():
            if m in b: continue
            added_total += 1
            if m in refd: continue
            (lost_ptr if ptr else lost_state).append((s, m, ty))
    print(f'{lbl:8} {added_total:3} mod-added  ->  {len(lost_state)} STATE lost, '
          f'{len(lost_ptr)} pointer-only (id dumped these as raw bit patterns anyway)')
    if verbose:
        for s, m, t in lost_state:
            print(f'         !! (d) {s}.{m}  ({t})')
    return lost_state, lost_ptr

if __name__ == '__main__':
    # Donor trees come from the workspace beside this repo, not /mnt/c.
    WS = os.environ.get('Q2DEV', os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.abspath(__file__)))))
    Q = os.path.join(WS, 'q2pro'); B = Q + '/src/game/g_local.h'
    v = '-v' in sys.argv
    # The two mod donors are read AT THEIR PINS, through git, and the pin is the
    # one this tree's own file header names (R-LIC-5), so there is one source of
    # truth for it.  Not the working tree: `rocketarena2` keeps `main` checked
    # out -- the 1999 reconstruction, whose g_save.c has no descriptor tables at
    # all -- and reading that would trip the extractor's own assertion below.
    # Which branch a checkout has on disk must not decide what this compares.
    SRC = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'src')
    ROWS = [('ctf', Q + '/src/ctf'), ('xatrix', Q + '/src/xatrix'),
            ('rogue', Q + '/src/rogue'),
            ('RA2', pinned(os.path.join(WS, 'rocketarena2'),
                           os.path.join(SRC, 'arena', 'arena.c'), 'rocketarena2')),
            ('OSP', pinned(os.path.join(WS, 'osp-tourney'),
                           os.path.join(SRC, 'tourney', 'osp_main.c'), 'osp-tourney'))]

    # A MISSING INPUT IS A SKIP AND NOT A FINDING.  Every tree this sweep reads
    # is the WORKSPACE's rather than this repository's, and `q2pro/src/{ctf,
    # xatrix,rogue}` is a local replay branch rather than anything that can be
    # cloned (SPECS section 4.1) -- so a machine holding only this repository, a
    # release runner or a fresh clone, cannot have them and never could.  Until
    # this guard existed the first `read()` below raised FileNotFoundError,
    # audit.py reported the traceback as a finding, and a finding fails the
    # build: every job of the release workflow died here, on a tree with
    # nothing wrong with it.  audit.py lifts the line below into its
    # "not applicable" list, which is what `assets` and `fnsweep` already do,
    # and it is the only thing printed because that is the shape audit.py
    # rewrites.
    need = [B]
    for _, d in ROWS:
        need += [ref(d, 'g_local.h'), ref(d, 'g_save.c')]
    missing = [p for p in need if not exists(p)]
    if missing:
        first = missing[0] if missing[0].startswith('git:') else \
            os.path.relpath(missing[0], WS)
        print('dsweep.py: SKIP -- %d of the %d reference files are not in the '
              'workspace beside this repository (first: %s), so the mod-added '
              'members have nothing to be swept against'
              % (len(missing), len(need), first))
        sys.exit(0)

    # sanity: the extractor must find the three members the method already named
    rb = struct_body(read(Q + '/src/rogue/g_local.h'), 'edict_s')
    rm = members(rb)
    for probe in ('plat2flags', 'gravityVector', 'hint_chain_id'):
        assert probe in rm, f'SELF-TEST FAILED: {probe} not found in rogue edict_s'
    assert 'forcemap' not in members(struct_body(read(Q + '/src/ctf/g_local.h'), 'monsterinfo_t')), \
        'SELF-TEST FAILED: struct bleed still present'
    print('extractor self-test: rogue edict_s has all three probes; no struct bleed\n')
    print('mechanism (d) -- mod-added members of saved structs with no descriptor row')
    print('-' * 78)
    for lbl, d in ROWS:
        run(lbl, B, ref(d, 'g_local.h'), ref(d, 'g_save.c'), v)
