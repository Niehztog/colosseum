"""Mechanism (d), done with real brace matching -- and, first, the tree's own
descriptor tables checked for type agreement and duplicate rows (R-SAVE-3a),
which needs no donor and is never skipped.  `--selftest` runs its controls.

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

# ---------------------------------------------------------------------------
# The tree's OWN descriptor tables: every row names a member that exists, with
# the macro its C type needs, once (R-SAVE-3a).
#
# The record is positional -- write_field/read_field walk each table in order
# -- so a row with the wrong macro round-trips a member through the wrong
# encoder, and a second row for a member is a second copy of it in every
# savegame.  Four monsterinfo_t frame counters went through F() against `int`
# members, and eight members were listed twice, while R-SAVE-3a said type
# agreement was checked on every build: the tool that was meant to check it
# checked presence only.  This is the check, unconditional -- it reads only
# this repository, so it is never skipped with the donor sweep below.

TABLES = (('entityfields', 'edict_s'), ('clientfields', 'gclient_s'),
          ('levelfields', 'level_locals_t'), ('gamefields', 'game_locals_t'))

ARRAY_TYPEDEFS = {}     # vec3_t -> float, filled by c_structs
INT_TYPES = {'int', 'unsigned', 'unsigned int', 'int32_t', 'uint32_t',
             'signed int'}
ARRAY_MACROS = {'IA', 'SA', 'BA', 'FA', 'OA', 'SZ'}


# The one macro these files branch on, at the value the default build uses.
# Any other condition keeps both arms, which is the conservative reading.
PP_DEFINED = {'USE_NEW_GAME_API': True}


def preprocess(text):
    """`text` with the inactive arm of every `#if [!]KNOWN` blanked, line for
    line, so line numbers survive."""
    out, stack = [], []
    for line in text.split('\n'):
        d = re.match(r'^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b\s*(.*)', line)
        if d:
            kw, cond = d.group(1), d.group(2).strip()
            if kw in ('if', 'ifdef', 'ifndef'):
                m = re.match(r'^(!?)\s*(\w+)\s*$', cond)
                known = m and m.group(2) in PP_DEFINED
                val = None
                if known:
                    val = PP_DEFINED[m.group(2)]
                    if m.group(1) or kw == 'ifndef':
                        val = not val
                stack.append(val)
            elif kw in ('else', 'elif') and stack:
                if stack[-1] is not None:
                    stack[-1] = not stack[-1] if kw == 'else' else None
            elif kw == 'endif' and stack:
                stack.pop()
            out.append('')
            continue
        out.append('' if any(v is False for v in stack) else line)
    return '\n'.join(out)


def _split_decls(rest):
    out, depth, cur = [], 0, ''
    for ch in rest:
        if ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur)
            cur = ''
        else:
            cur += ch
    out.append(cur)
    return [d.strip() for d in out if d.strip()]


def c_members(body):
    """{name: (base type, 'scalar'|'pointer'|'fnptr', is_array)} for one body.

    Statements at depth 0 only; preprocessor lines are ignored, so a member
    declared under both arms of an #if keeps the first spelling."""
    out = {}
    text = re.sub(r'/\*.*?\*/', ' ', body, flags=re.S)
    text = re.sub(r'//[^\n]*', '', text)
    text = '\n'.join(l for l in text.split('\n')
                     if not l.lstrip().startswith('#'))
    depth, stmt = 0, ''
    for ch in text:
        if ch == '{':
            depth += 1
            stmt = ''
            continue
        if ch == '}':
            depth -= 1
            stmt = ''
            continue
        if depth:
            continue
        if ch == ';':
            st = ' '.join(stmt.split())
            stmt = ''
            fp = re.match(r'^(.*?)\(\s*\*\s*(\w+)\s*\)\s*\(.*\)$', st)
            if fp:
                out.setdefault(fp.group(2), (fp.group(1).strip(), 'fnptr',
                                             False))
                continue
            m = re.match(r'^((?:(?:const|volatile|unsigned|signed|struct|'
                         r'enum)\s+)*\w+(?:\s+int)?)\s+(.+)$', st)
            if not m:
                continue
            base = re.sub(r'^(?:const|volatile)\s+', '', m.group(1)).strip()
            for d in _split_decls(m.group(2)):
                dm = re.match(r'^(\**)\s*(\w+)\s*((?:\[[^\]]*\])*)$', d)
                if dm:
                    out.setdefault(dm.group(2), (
                        base, 'pointer' if dm.group(1) else 'scalar',
                        bool(dm.group(3))))
        else:
            stmt += ch
    return out


def c_structs(texts):
    """({struct or typedef name: members}, enum names, {alias: target})."""
    structs, enums, alias = {}, set(), {}
    for t in texts:
        for m in re.finditer(r'\b(typedef\s+)?struct\s*(\w*)\s*\{', t):
            i, d = m.end(), 1
            k = i
            while d and k < len(t):
                if t[k] == '{':
                    d += 1
                elif t[k] == '}':
                    d -= 1
                k += 1
            body = t[i:k - 1]
            tail = re.match(r'\s*(\w+)\s*;', t[k:])
            mem = c_members(body)
            if m.group(2):
                structs.setdefault(m.group(2), mem)
            if m.group(1) and tail:
                structs.setdefault(tail.group(1), mem)
        for m in re.finditer(r'typedef\s+enum\s*\w*\s*\{[^}]*\}\s*(\w+)\s*;', t):
            enums.add(m.group(1))
        for m in re.finditer(r'typedef\s+(\w+)\s+(\w+)\s*\[[^\]]+\]\s*;', t):
            ARRAY_TYPEDEFS.setdefault(m.group(2), m.group(1))
        for m in re.finditer(r'typedef\s+(?:struct\s+)?([\w ]+?)\s+(\w+)\s*;',
                             t):
            alias.setdefault(m.group(2), m.group(1).strip())
    return structs, enums, alias


def _resolve(structs, alias, name):
    seen = 0
    while name not in structs and name in alias and seen < 8:
        name = alias[name]
        seen += 1
    name = re.sub(r'^struct\s+', '', name)
    return structs.get(name)


def _type_ok(macro, base, kind, array, enums, alias):
    b = base
    for _ in range(4):
        if b in alias and b not in enums and alias[b] != b:
            nxt = alias[b]
            if nxt in ('unsigned char', 'int', 'float', 'short') or nxt in enums:
                b = nxt
                break
            b = nxt
        else:
            break
    base_s = re.sub(r'^struct\s+', '', base)
    if macro in ('FA', 'IA') and not array and base in ARRAY_TYPEDEFS:
        # FA(ps.blend, 4) on a vec4_t: the typedef IS the array
        b, array = ARRAY_TYPEDEFS[base], True
        b = alias.get(b, b)             # vec_t -> float
    if (macro in ARRAY_MACROS) != array and macro != 'SZ':
        return 'the member is %san array' % ('' if array else 'not ')
    m = macro[0] if macro in ARRAY_MACROS and macro != 'SZ' else macro
    if m == 'P':
        return None if kind in ('pointer', 'fnptr') else 'P() on a non-pointer'
    if kind == 'fnptr':
        return 'a function pointer wants P()'
    want = {
        'I': lambda: kind == 'scalar' and (b in INT_TYPES or b in enums or
                                           base in enums),
        'F': lambda: kind == 'scalar' and b == 'float',
        'V': lambda: kind == 'scalar' and base == 'vec3_t',
        'E': lambda: kind == 'pointer' and base_s in ('edict_t', 'edict_s'),
        'T': lambda: kind == 'pointer' and base_s in ('gitem_t', 'gitem_s'),
        'L': lambda: kind == 'pointer' and base == 'char',
        'SZ': lambda: kind == 'scalar' and base == 'char' and array,
        'B': lambda: kind == 'scalar' and b in ('byte', 'unsigned char',
                                                'uint8_t'),
        'S': lambda: kind == 'scalar' and b in ('short', 'int16_t',
                                                'uint16_t', 'unsigned short'),
        'O': lambda: kind == 'scalar' and b == 'bool',
    }.get(m)
    if want is None:
        return 'unknown macro %s' % macro
    return None if want() else 'the member is %s%s%s' % (
        base, ' *' if kind == 'pointer' else '', '[]' if array else '')


def descriptor_findings(save_text, header_texts):
    save_text = preprocess(save_text)
    structs, enums, alias = c_structs([preprocess(h) for h in header_texts])
    out, rows = [], 0
    for table, top in TABLES:
        m = re.search(r'static const save_field_t %s\[\] = \{(.*?)\n\};'
                      % table, save_text, re.S)
        if not m:
            out.append('g_save.c: no %s[] table' % table)
            continue
        base_line = save_text[:m.start(1)].count('\n') + 1
        seen = {}
        for off, line in enumerate(m.group(1).split('\n')):
            rm = re.match(r'^\s*([A-Z]{1,2})\(\s*([\w.]+)', line)
            if not rm:
                continue
            rows += 1
            macro, path = rm.group(1), rm.group(2)
            where = 'g_save.c:%d' % (base_line + off)
            if path in seen:
                out.append('%s: %s(%s) in %s[] duplicates line %d -- the '
                           'record is positional, so every savegame carries '
                           'it twice' % (where, macro, path, table, seen[path]))
            seen.setdefault(path, base_line + off)
            mem = _resolve(structs, alias, top)
            info = None
            for i, part in enumerate(path.split('.')):
                if mem is None or part not in mem:
                    info = None
                    break
                info = mem[part]
                if i < len(path.split('.')) - 1:
                    mem = _resolve(structs, alias, info[0])
            if info is None:
                out.append('%s: %s(%s) names no member of %s' %
                           (where, macro, path, top))
                continue
            why = _type_ok(macro, info[0], info[1], info[2], enums, alias)
            if why:
                out.append('%s: %s(%s) -- %s' % (where, macro, path, why))
    return out, rows


# The map's own descriptor tables, g_spawn.c's spawn_fields[] and
# temp_fields[]: ED_ParseField writes each key through its row's F_* type, so a
# row whose type is not its member's writes the wrong representation into it --
# Ground Zero retyped spawn_temp_t's `pausetime` to int while its row still
# said F_FLOAT, and the parser wrote a float bit pattern into the int.  The
# parser also takes the FIRST row whose key matches, so a second row for a key
# is dead.  FOFS names an edict_t member, STOFS a spawn_temp_t one.
SPAWN_TABLES = ('spawn_fields', 'temp_fields')
SPAWN_BASES = {'FOFS': 'edict_s', 'STOFS': 'spawn_temp_t'}


def _spawn_type_ok(ftype, base, kind, array, enums, alias):
    if ftype == 'F_IGNORE':
        return None
    vec = (kind == 'scalar' and not array and base == 'vec3_t') or \
        (kind == 'scalar' and array and base in ('float', 'vec_t'))
    want = {
        'F_INT': kind == 'scalar' and not array and
                 (base in INT_TYPES or base in enums or
                  alias.get(base) in INT_TYPES),
        'F_FLOAT': kind == 'scalar' and not array and base in ('float', 'vec_t'),
        'F_LSTRING': kind == 'pointer' and base == 'char',
        'F_VECTOR': vec,
        'F_ANGLEHACK': vec,
    }.get(ftype)
    if want is None:
        return 'unknown field type %s' % ftype
    return None if want else 'the member is %s%s%s' % (
        base, ' *' if kind == 'pointer' else '', '[]' if array else '')


def spawn_findings(spawn_text, header_texts):
    spawn_text = preprocess(spawn_text)
    structs, enums, alias = c_structs([preprocess(h) for h in header_texts])
    out, rows = [], 0
    for table in SPAWN_TABLES:
        m = re.search(r'spawn_field_t\s+%s\[\]\s*=\s*\{(.*?)\n\};' % table,
                      spawn_text, re.S)
        if not m:
            out.append('g_spawn.c: no %s[] table' % table)
            continue
        base_line = spawn_text[:m.start(1)].count('\n') + 1
        seen = {}
        for off, line in enumerate(m.group(1).split('\n')):
            rm = re.match(r'^\s*\{\s*"([^"]+)"\s*,\s*(FOFS|STOFS)\(\s*([\w.]+)\s*\)'
                          r'\s*,\s*(F_\w+)', line)
            if not rm:
                continue
            rows += 1
            key, ofs, path, ftype = rm.groups()
            where = 'g_spawn.c:%d' % (base_line + off)
            low = key.lower()
            if low in seen:
                out.append('%s: key "%s" in %s[] repeats line %d -- the parser '
                           'takes the first, so this row is dead'
                           % (where, key, table, seen[low]))
            seen.setdefault(low, base_line + off)
            mem = _resolve(structs, alias, SPAWN_BASES[ofs])
            info = None
            for i, part in enumerate(path.split('.')):
                if mem is None or part not in mem:
                    info = None
                    break
                info = mem[part]
                if i < len(path.split('.')) - 1:
                    mem = _resolve(structs, alias, info[0])
            if info is None:
                out.append('%s: "%s" -> %s(%s) names no member of %s'
                           % (where, key, ofs, path, SPAWN_BASES[ofs]))
                continue
            why = _spawn_type_ok(ftype, info[0], info[1], info[2], enums, alias)
            if why:
                out.append('%s: "%s" -> %s(%s) is %s -- %s'
                           % (where, key, ofs, path, ftype, why))
    return out, rows


def tree_descriptors(src):
    save = read(os.path.join(src, 'g_save.c'))
    inc = os.path.join(os.path.dirname(src), 'inc', 'shared')
    heads = [read(os.path.join(src, 'g_local.h'))]
    for h in ('shared.h', 'game.h'):
        if exists(os.path.join(inc, h)):
            heads.append(read(os.path.join(inc, h)))
    found, rows = descriptor_findings(save, heads)
    sfound, srows = spawn_findings(read(os.path.join(src, 'g_spawn.c')), heads)
    return found + sfound, rows + srows, save, heads


def descriptor_selftest(src):
    found, rows, save, heads = tree_descriptors(src)
    bad = 0
    cases = [('clean tree', save, 0)]
    for label, old, new in (
            ('an int counter through F()', 'I(monsterinfo.pause_framenum)',
             'F(monsterinfo.pause_framenum)'),
            ('a second row for a member', '    I(monsterinfo.nextframe),\n',
             '    I(monsterinfo.nextframe),\n    I(monsterinfo.nextframe),\n'),
            ('a row naming no member', 'I(monsterinfo.nextframe)',
             'I(monsterinfo.no_such_member)')):
        if old not in save:
            print('  !! control "%s": its mutation no longer applies -- the '
                  'control has rotted' % label)
            bad += 1
            continue
        cases.append((label, save.replace(old, new, 1), 1))
    # ...and the map's own table, with the defect named above: Ground Zero's
    # int `pausetime` behind a row that still says F_FLOAT.
    spawn = read(os.path.join(src, 'g_spawn.c'))
    got, _ = spawn_findings(spawn, heads)
    if got:
        print('  !! control "the spawn tables are clean" fired: ' + got[0])
        bad += 1
    else:
        print('  ok  control "the spawn tables are clean" is quiet')
    old = '{"pausetime", STOFS(pausetime), F_FLOAT}'
    if old not in spawn:
        print('  !! control "a spawn row with the wrong type": its mutation no '
              'longer applies -- the control has rotted')
        bad += 1
    else:
        got, _ = spawn_findings(spawn.replace(old, '{"pausetime", STOFS(pausetime), F_INT}', 1), heads)
        print('  %s control "a spawn row with the wrong type" %s'
              % (('ok ', 'fires') if got else ('!!', 'did NOT fire')))
        bad += not got
    for label, text, want in cases:
        got, _ = descriptor_findings(text, heads)
        fired = 1 if got else 0
        if fired == want:
            print('  ok  control "%s" %s' % (label, 'fires' if want else
                                             'is quiet'))
        else:
            print('  !! control "%s" %s' % (label, 'did NOT fire' if want else
                                            'fired: ' + got[0]))
            bad += 1
    return bad


if __name__ == '__main__':
    # Donor trees come from the workspace beside this repo, not /mnt/c.
    WS = os.environ.get('Q2DEV', os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.abspath(__file__)))))
    Q = os.path.join(WS, 'q2pro'); B = Q + '/src/game/g_local.h'
    v = '-v' in sys.argv
    SRC0 = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'src')
    if '--selftest' in sys.argv:
        sys.exit(1 if descriptor_selftest(SRC0) else 0)
    found, nrows, _, _ = tree_descriptors(SRC0)
    for f in found:
        print('  !! ' + f)
    print('descriptors: %d row(s) in the four save tables and the two spawn '
          'tables, %d finding(s)' % (nrows, len(found)))
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
        sys.exit(1 if found else 0)

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
    sys.exit(1 if found else 0)
