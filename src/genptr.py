#!/usr/bin/python3

import re
import sys

pointers = {
    'prethink'  : 'void {p}(edict_t *)',
    'think'     : 'void {p}(edict_t *)',
    'blocked'   : 'void {p}(edict_t *, edict_t *)',
    'touch'     : 'void {p}(edict_t *, edict_t *, cplane_t *, csurface_t *)',
    'use'       : 'void {p}(edict_t *, edict_t *, edict_t *)',
    'pain'      : 'void {p}(edict_t *, edict_t *, float, int)',
    'die'       : 'void {p}(edict_t *, edict_t *, edict_t *, int, vec3_t)',
    'moveinfo_endfunc'          : 'void {p}(edict_t *)',
    'monsterinfo_currentmove'   : 'const mmove_t {p}',
    'monsterinfo_stand'         : 'void {p}(edict_t *)',
    'monsterinfo_idle'          : 'void {p}(edict_t *)',
    'monsterinfo_search'        : 'void {p}(edict_t *)',
    'monsterinfo_walk'          : 'void {p}(edict_t *)',
    'monsterinfo_run'           : 'void {p}(edict_t *)',
    'monsterinfo_dodge'         : 'void {p}(edict_t *, edict_t *, float, trace_t *)',
    'monsterinfo_attack'        : 'void {p}(edict_t *)',
    'monsterinfo_melee'         : 'void {p}(edict_t *)',
    'monsterinfo_sight'         : 'void {p}(edict_t *, edict_t *)',
    'monsterinfo_checkattack'   : 'bool {p}(edict_t *)',
    # ROGUE
    'monsterinfo_blocked'       : 'bool {p}(edict_t *, float)',
    'monsterinfo_duck'          : 'void {p}(edict_t *, float)',
    'monsterinfo_unduck'        : 'void {p}(edict_t *)',
    'monsterinfo_sidestep'      : 'void {p}(edict_t *)',
    # ROGUE
}

# THE SCAN IS OVER C, NOT OVER LINES.  It was a line scanner that stripped
# block comments before line comments, so a `/*` inside a `//` comment --
# `bots/*.cfg`, a `//****` rule -- opened a block comment that ran to the next
# `*/` in the file and hid every assignment in between; it matched one line at
# a time, so `self->think =` with its value on the next line registered
# nothing; and it read the `#define`s of the listed .c files only, so an
# `#ifdef` on a macro a header defines was taken as inactive.  None of the
# three missed a pointer in the tree when they were found -- a missed pointer
# is `unknown pointer` at the next save, so it would have been visible -- and
# each was one edit away from it.
#
# So: comments and literals are stripped by one pass that knows which it is
# in, the preprocessor pass runs on what is left, and an assignment is matched
# as a statement, across lines.  A value that is not a plain name is REFUSED
# rather than guessed: a ternary registers neither arm, and a copy from another
# field or a call registers a name that is not a function (R-SAVE-2).  That is
# the constraint the save table has always had, now said by the build instead
# of discovered at a load.

def strip_c(text):
    """`text` with comments blanked and string/char literal contents blanked,
    every newline kept so line numbers survive."""
    out = []
    i, n, quote = 0, len(text), None
    while i < n:
        c = text[i]
        if quote is None:
            if text.startswith('//', i):
                j = text.find('\n', i)
                j = n if j < 0 else j
                out.append(' ' * (j - i))
                i = j
            elif text.startswith('/*', i):
                j = text.find('*/', i + 2)
                j = n if j < 0 else j + 2
                out.append(re.sub(r'[^\n]', ' ', text[i:j]))
                i = j
            else:
                if c in '"\'':
                    quote = c
                out.append(c)
                i += 1
        elif c == '\\' and i + 1 < n:
            out.append(' \n' if text[i + 1] == '\n' else '  ')
            i += 2
        else:
            if c == quote:
                quote = None
                out.append(c)
            else:
                out.append('\n' if c == '\n' else ' ')
            i += 1
    return ''.join(out)


def active_text(text, defined):
    """`text` with the lines of every inactive #if block blanked."""
    out, skip, depth = [], 0, 0
    for line in text.split('\n'):
        s = line.strip()
        if s.startswith('#'):
            m = re.match(r'#\s*(ifdef|ifndef|if|else|elif|endif)\b(.*)', s)
            if m:
                kind, rest = m.group(1), m.group(2).strip()
                if kind in ('ifdef', 'ifndef', 'if'):
                    depth += 1
                    if not skip:
                        inactive = (kind == 'ifdef' and rest not in defined) or \
                                   (kind == 'if' and rest == '0')
                        if inactive:
                            skip = depth
                elif kind == 'endif':
                    if skip == depth:
                        skip = 0
                    depth -= 1
                elif kind in ('else', 'elif') and skip == depth:
                    skip = 0
            out.append('')
            continue
        out.append('' if skip else line)
    return '\n'.join(out)


def scan(files, defined):
    """-> ({pointer kind: [names, first seen first]}, [refusals])."""
    fields = '|'.join(p.replace('_', r'\s*\.\s*') for p in pointers.keys()
                      if not p == 'moveinfo_endfunc')
    assign = re.compile(r'->\s*(%s)\s*=(?!=)\s*([^;]*);' % fields, re.ASCII)
    endfunc = re.compile(r'(\w+\s+)?\b(?:Angle)?Move_Calc\s*\(([^;]*)\)\s*;',
                         re.ASCII)

    types = {p: [] for p in pointers.keys()}
    refused = []

    def add(kind, name):
        if name != 'NULL' and name not in types[kind]:
            types[kind].append(name)

    for a, text in files:
        t = active_text(strip_c(text), defined)
        for m in assign.finditer(t):
            rhs = ' '.join(m.group(2).split())
            name = re.fullmatch(r'&?\s*(\w+)', rhs)
            if name:
                add(re.sub(r'\s*\.\s*', '_', m.group(1)), name.group(1))
            else:
                refused.append('%s:%d: `->%s = %s` is not a plain name' % (
                    a, t[:m.start()].count('\n') + 1,
                    re.sub(r'\s+', '', m.group(1)), rhs))
        for m in endfunc.finditer(t):
            if m.group(1) and m.group(1).strip() not in ('return', 'else'):
                continue        # the definition, not a call
            last = m.group(2).split(',')[-1].strip()
            if re.fullmatch(r'\w+', last):
                add('moveinfo_endfunc', last)
            else:
                refused.append('%s:%d: `Move_Calc(..., %s)` is not a plain '
                               'name' % (a, t[:m.start()].count('\n') + 1,
                                         last))
    return types, refused


def headers_beside(files):
    """Every header the listed sources could include: this tree's and the
    engine's shared ones.  Their `#define`s decide which blocks are live."""
    import glob
    import os
    here = os.path.dirname(os.path.abspath(__file__))
    out = glob.glob(os.path.join(here, '**', '*.h'), recursive=True)
    out += glob.glob(os.path.join(here, '..', 'inc', '**', '*.h'),
                     recursive=True)
    return out


def defines(texts):
    found = set()
    for t in texts:
        for line in t.split('\n'):
            m = re.match(r'\s*#\s*define\s+(\w+)', line)
            if m:
                found.add(m.group(1))
    return found


def selftest():
    """Each hole the scanner had, as a source it must read right."""
    bad = 0
    cases = [
        ('a /* inside a // comment',
         '// bots/*.cfg\nvoid f(edict_t *e) { e->think = alpha; }\n',
         ['alpha'], 0),
        ('the value on the next line',
         'void f(edict_t *e) { e->think =\n    beta; }\n', ['beta'], 0),
        ('a header macro',
         '#ifdef FROM_A_HEADER\nvoid f(edict_t *e) { e->think = gamma; }\n#endif\n',
         ['gamma'], 0),
        ('a block the build compiles out',
         '#ifdef NEVER_DEFINED\nvoid f(edict_t *e) { e->think = delta; }\n#endif\n',
         [], 0),
        ('a string that looks like code',
         'void f(void) { puts("e->think = epsilon;"); }\n', [], 0),
        ('a ternary', 'void f(edict_t *e) { e->think = c ? zeta : eta; }\n',
         [], 1),
        ('a copy from another field',
         'void f(edict_t *e, edict_t *o) { e->think = o->think; }\n', [], 1),
    ]
    for label, src, want, refusals in cases:
        types, refused = scan([('<%s>' % label, src)], {'FROM_A_HEADER'})
        got = types['think']
        ok = got == want and len(refused) == refusals
        print('  %-34s %s' % (label, 'ok' if ok else 'WRONG: %r %r'
                              % (got, refused)))
        bad += not ok
    print('genptr.py --selftest: %d control(s), %d wrong' % (len(cases), bad))
    return bad


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == '--selftest':
        sys.exit(1 if selftest() else 0)
    if len(sys.argv) < 2:
        print('Usage: genptr.py <file> [...]')
        sys.exit(1)

    files = []
    for a in sys.argv[1:]:
        with open(a, encoding='latin-1') as f:
            files.append((a, f.read()))
    heads = []
    for h in headers_beside(sys.argv[1:]):
        with open(h, encoding='latin-1') as f:
            heads.append(f.read())

    # Collect the macros the sources and their headers actually define, so
    # blocks guarded by feature switches that are never set
    # (INCLUDE_INCENDIARY, ...) are skipped; otherwise we would emit externs
    # for functions that do not exist.
    types, refused = scan(files, defines([t for _, t in files] + heads))
    if refused:
        for r in refused:
            print('genptr.py: %s -- the save table can only register a '
                  'function named in the assignment (R-SAVE-2)' % r,
                  file=sys.stderr)
        sys.exit(1)

    print('// generated by genptr.py, do not modify')
    print('#include "g_local.h"')
    print('#include "g_ptrs.h"')

    decls = []
    for k, v in types.items():
        for p in v:
            decls.append('extern ' + pointers[k].replace('{p}', p) + ';')
    for d in sorted(decls, key=str.lower):
        print(d)

    print('const save_ptr_t save_ptrs[] = {')
    for k, v in types.items():
        for p in sorted(v, key=str.lower):
            amp = '&' if k == 'monsterinfo_currentmove' else ''
            print('{ %s, %s%s },' % ('P_' + k, amp, p))
    print('};')
    print('const int num_save_ptrs = sizeof(save_ptrs) / sizeof(save_ptrs[0]);')
