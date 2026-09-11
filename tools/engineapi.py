#!/usr/bin/env python3
"""R-ENG-3's inventory of the extended engine API is checked against the tree.

WHY.  `GetGameAPIEx` hands the library two tables -- `game_import_ex_t`, which
the engine fills, and `game_export_ex_t`, which the library fills -- and taking
an entry from either is one line of code that nothing else depends on.  A
paragraph can therefore claim a capability the library does not take, and
nothing fails: not the build, not a boot, not a play test.  It happened.  R-ENG-3
read "Colosseum uses `get_configstring` for index recovery after a savegame load
and `local_sound` for per-client audio" while `gex` was dereferenced in
`src/g_fs.c` and nowhere else, which is section 0 rule 5's distinction failing in
the direction that hides -- a capability read as present while the code taking it
did not exist.  The same shape had already cost the `G_FsLoadFile` fallback
(R-ENG-4), so this is the second instance of the class rather than the first.

So the inventory is a TABLE in `SPECS.md` and this tool reads both sides of it:
the members out of `inc/shared/gameext.h`, the call sites out of `src/`.

WHAT IS A FINDING.  A row and the tree disagreeing, in either direction -- a row
saying `yes` with no call site, or a call site under a row saying `no`; a member
the header declares with no row; a row naming a member the header does not have.
Each is reported with the member and the side that has to move.

WHAT IS NOT.  Which entry points the library *ought* to take: that is R-ENG-3's
judgement and R-ENG-6's work, and this tool has no opinion about it.  The base
`game_import_t` is out of scope -- `gi.` is used on nearly every line of the
tree and a table of it would be a transcription, not a check.

HOW A MEMBER IS SEEN AS TAKEN.  `gex->NAME`, in code with comments and string
literals stripped.  If any file copies `gex` into another pointer the tool says
so and fails rather than answering from a search it knows is now incomplete.

USAGE
    tools/engineapi.py [--tree src] [--header inc/shared/gameext.h] [--spec SPECS.md]
    tools/engineapi.py --list          # the tree's own answer, for the table
    tools/engineapi.py --selftest
"""
import argparse
import os
import re
import shutil
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

IMPORT_STRUCT = 'game_import_ex_t'
EXPORT_STRUCT = 'game_export_ex_t'

# apiversion and structsize are the handshake rather than entry points: both are
# always set and neither is a capability.
NOT_ENTRY_POINTS = {'apiversion', 'structsize'}


def strip_comments(t):
    def blank(m):
        n = m.group(0).count('\n')
        return '\n' * n + ' ' * (len(m.group(0)) - n)
    t = re.sub(r'/\*.*?\*/', blank, t, flags=re.S)
    t = re.sub(r'//[^\n]*', lambda m: ' ' * len(m.group(0)), t)
    t = re.sub(r'"(?:\\.|[^"\\\n])*"', lambda m: ' ' * len(m.group(0)), t)
    t = re.sub(r"'(?:\\.|[^'\\\n])*'", lambda m: ' ' * len(m.group(0)), t)
    return t


def struct_body(text, name):
    """The body of `typedef struct { ... } name;`, brace-matched rather than
    regexed, because a member that is a function pointer carries braces of its
    own in no other file but would in a header that gained one."""
    pat = re.compile(r'typedef\s+struct\s*\{')
    m = pat.search(text)
    while m:
        i = m.end() - 1
        depth = 0
        for j in range(i, len(text)):
            if text[j] == '{':
                depth += 1
            elif text[j] == '}':
                depth -= 1
                if depth == 0:
                    tail = text[j + 1:j + 200]
                    if re.match(r'\s*' + re.escape(name) + r'\s*;', tail):
                        return text[i + 1:j]
                    break
        m = pat.search(text, m.end())
    return None


def members(header_text, struct):
    body = struct_body(strip_comments(header_text), struct)
    if body is None:
        return None
    out = []
    for decl in body.split(';'):
        decl = ' '.join(decl.split())
        if not decl:
            continue
        # a function pointer member: `type (*name)(args)` or `type (q_gameabi name)(args)`
        m = re.search(r'\(\s*\*?\s*(?:q_gameabi\s+)?\*?\s*([A-Za-z_]\w*)\s*\)\s*\(', decl)
        if not m:
            # a plain member: the last identifier before the semicolon
            m2 = re.search(r'([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?$', decl)
            if m2 and m2.group(1) not in NOT_ENTRY_POINTS:
                out.append(m2.group(1))
            elif m2:
                out.append(m2.group(1))
            continue
        out.append(m.group(1))
    return [m for m in out if m not in NOT_ENTRY_POINTS]


def sources(tree):
    out = []
    for root, dirs, files in os.walk(tree):
        dirs[:] = [d for d in dirs if not d.startswith(('debug', 'release', '__'))]
        for f in sorted(files):
            if f.endswith(('.c', '.h')):
                out.append(os.path.join(root, f))
    return out


def export_initialisers(code):
    """Every designator assigned a non-NULL value inside a `game_export_ex_t`
    initialiser.  Brace-matched: the initialiser of a table whose members are
    themselves initialised would otherwise end at the first `}`."""
    filled = {}
    for m in re.finditer(re.escape(EXPORT_STRUCT) + r'\s+\w+\s*=\s*\{', code):
        i = m.end() - 1
        depth = 0
        for j in range(i, len(code)):
            if code[j] == '{':
                depth += 1
            elif code[j] == '}':
                depth -= 1
                if depth == 0:
                    body = code[i + 1:j]
                    for d in re.finditer(r'\.\s*([A-Za-z_]\w*)\s*=\s*([^,}]*)', body):
                        name, val = d.group(1), d.group(2).strip()
                        if name in NOT_ENTRY_POINTS:
                            continue
                        filled[name] = (val != 'NULL' and val != '0')
                    break
    return filled


def scan_tree(tree):
    """-> (taken, filled, aliases): import members dereferenced off `gex`,
    export members given a non-NULL initialiser, and any site that copies `gex`
    into another pointer (which would make the first answer incomplete)."""
    taken, filled, aliases = set(), {}, []
    for path in sources(tree):
        code = strip_comments(open(path, encoding='utf-8', errors='replace').read())
        rel = os.path.relpath(path, REPO)
        for m in re.finditer(r'\bgex\s*->\s*([A-Za-z_]\w*)', code):
            taken.add(m.group(1))
        # `= gex;` is an alias; `gex = import;` in the entry point is not, and
        # neither is a test of the pointer itself.
        for m in re.finditer(r'([A-Za-z_]\w*)\s*=\s*gex\s*;', code):
            line = code.count('\n', 0, m.start()) + 1
            aliases.append((rel, line, m.group(1)))
        filled.update({k: v for k, v in export_initialisers(code).items() if v})
    return taken, set(k for k, v in filled.items() if v), aliases


TABLE_HEADS = {
    IMPORT_STRUCT: re.compile(r'^\s*\|\s*`' + IMPORT_STRUCT + r'`\s*\|'),
    EXPORT_STRUCT: re.compile(r'^\s*\|\s*`' + EXPORT_STRUCT + r'`\s*\|'),
}
ROW = re.compile(r'^\s*\|\s*`([A-Za-z_]\w*)`\s*\|\s*(?:\*\*)?(yes|no)(?:\*\*)?\s*\|')


def spec_tables(spec_text):
    """-> {struct: {member: claimed}}.  A table ends at the first line that is
    not a row of it, so the two tables cannot bleed into one another."""
    out = {}
    lines = spec_text.split('\n')
    for i, line in enumerate(lines):
        for struct, head in TABLE_HEADS.items():
            if not head.match(line):
                continue
            rows = {}
            for ln in lines[i + 1:]:
                if not ln.strip().startswith('|'):
                    break
                m = ROW.match(ln)
                if m:
                    rows[m.group(1)] = (m.group(2) == 'yes')
            out.setdefault(struct, {}).update(rows)
    return out


def check(tree, header_path, spec_path):
    findings = []
    header = open(header_path, encoding='utf-8').read()
    spec = open(spec_path, encoding='utf-8').read()

    imports = members(header, IMPORT_STRUCT)
    exports = members(header, EXPORT_STRUCT)
    if imports is None or exports is None:
        return ['%s: %s or %s not found -- the header moved and this check cannot '
                'answer' % (os.path.relpath(header_path, REPO), IMPORT_STRUCT,
                            EXPORT_STRUCT)], 0

    taken, filled, aliases = scan_tree(tree)
    for rel, line, name in aliases:
        findings.append('%s:%d: `gex` is copied into `%s`, so "taken" can no longer '
                        'be answered by searching for gex->' % (rel, line, name))

    tables = spec_tables(spec)
    rows = 0
    for struct, declared, live in ((IMPORT_STRUCT, imports, taken),
                                   (EXPORT_STRUCT, exports, filled)):
        claims = tables.get(struct)
        if claims is None:
            findings.append('%s: no `%s` table in the spec -- R-ENG-3 is the '
                            'inventory and there is nothing to check it against'
                            % (os.path.relpath(spec_path, REPO), struct))
            continue
        rows += len(claims)
        for name in declared:
            if name not in claims:
                findings.append('%s.%s: the header declares it and R-ENG-3 has no row '
                                'for it' % (struct, name))
        for name in sorted(claims):
            if name not in declared:
                findings.append('%s.%s: R-ENG-3 has a row and the header does not '
                                'declare it' % (struct, name))
                continue
            verb = 'taken' if struct == IMPORT_STRUCT else 'filled'
            if claims[name] and name not in live:
                findings.append('%s.%s: R-ENG-3 says %s and no site in the tree %s it'
                                % (struct, name, verb,
                                   'calls' if struct == IMPORT_STRUCT else 'fills'))
            if not claims[name] and name in live:
                findings.append('%s.%s: the tree %s it and R-ENG-3 says not %s'
                                % (struct, name,
                                   'calls' if struct == IMPORT_STRUCT else 'fills', verb))
    return findings, rows


SELFTEST_HEADER = '''
typedef struct {
    uint32_t apiversion;
    uint32_t structsize;
    void (*local_sound)(edict_t *t, const vec3_t o, edict_t *e, int c, int s, float v, float a, float f);
    const char *(*get_configstring)(int index);
    void *(*GetExtension)(const char *name);
} game_import_ex_t;

typedef struct {
    uint32_t apiversion;
    uint32_t structsize;
    qboolean (*CanSave)(void);
    void (*PrepFrame)(void);
} game_export_ex_t;
'''

SELFTEST_SRC = '''
#include "g_local.h"
const game_import_ex_t *gex;
static const game_export_ex_t globals_ex = {
    .apiversion = GAME_API_VERSION_EX,
    .structsize = sizeof(globals_ex),
};
static const void *G_Fs(void)
{
    /* gex->local_sound in a comment is not a call site */
    if (gex && gex->GetExtension)
        return gex->GetExtension("FILESYSTEM_API_V1");
    return NULL;
}
'''

SELFTEST_SPEC = '''
**R-ENG-3.** the inventory.

  | `game_import_ex_t` | taken | where, or why not |
  |---|---|---|
  | `GetExtension` | **yes** | g_fs.c |
  | `get_configstring` | no | nothing reads a configstring back |
  | `local_sound` | no | every sound here is a world event |

  | `game_export_ex_t` | filled | why not |
  |---|---|---|
  | `CanSave` | no | R-ENG-6 |
  | `PrepFrame` | no | R-ENG-6 |
'''


def selftest():
    ok = True
    tmp = tempfile.mkdtemp(prefix='engineapi-selftest-')
    try:
        hdr = os.path.join(tmp, 'gameext.h')
        spec = os.path.join(tmp, 'SPECS.md')
        tree = os.path.join(tmp, 'src')
        os.mkdir(tree)
        src = os.path.join(tree, 'g_fs.c')

        def write(header=SELFTEST_HEADER, source=SELFTEST_SRC, spectext=SELFTEST_SPEC):
            open(hdr, 'w').write(header)
            open(src, 'w').write(source)
            open(spec, 'w').write(spectext)

        write()
        base, rows = check(tree, hdr, spec)
        if base:
            print('!! selftest: the clean fixture reported %s' % (base,))
            ok = False
        else:
            print('   [control] a fixture whose table matches its tree: 0 findings, '
                  '%d row(s)' % rows)

        mutants = [
            ('R-ENG-3\'s own defect: a row claiming an entry nothing calls',
             dict(spectext=SELFTEST_SPEC.replace(
                 '| `get_configstring` | no |', '| `get_configstring` | **yes** |'))),
            ('a call site under a row that says no',
             dict(source=SELFTEST_SRC.replace(
                 'return NULL;',
                 'gex->local_sound(NULL, NULL, NULL, 0, 0, 1, 1, 0);\n    return NULL;'))),
            ('a header member with no row',
             dict(header=SELFTEST_HEADER.replace(
                 '    void *(*GetExtension)(const char *name);',
                 '    void *(*GetExtension)(const char *name);\n'
                 '    void *(*TagRealloc)(void *p, size_t n);'))),
            ('a row for a member the header does not declare',
             dict(spectext=SELFTEST_SPEC.replace(
                 '  | `local_sound` | no | every sound here is a world event |',
                 '  | `local_sound` | no | every sound here is a world event |\n'
                 '  | `inVIS` | no | nothing asks |'))),
            ('an export entry filled in the tree and denied in the table',
             dict(source=SELFTEST_SRC.replace(
                 '    .structsize = sizeof(globals_ex),',
                 '    .structsize = sizeof(globals_ex),\n    .CanSave = G_CanSave,'))),
            ('`gex` copied into another pointer, which the search cannot follow',
             dict(source=SELFTEST_SRC.replace(
                 'static const void *G_Fs(void)',
                 'static const game_import_ex_t *mine;\n'
                 'void G_Alias(void) { mine = gex; }\n'
                 'static const void *G_Fs(void)'))),
        ]
        for why, kw in mutants:
            write(**kw)
            got, _ = check(tree, hdr, spec)
            if not got:
                print('!! selftest: %s was NOT caught' % why)
                ok = False
            else:
                print('   [control] %s -> caught' % why)
        write()
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tree', default=os.path.join(REPO, 'src'))
    ap.add_argument('--header', default=os.path.join(REPO, 'inc/shared/gameext.h'))
    ap.add_argument('--spec', default=os.path.join(REPO, 'SPECS.md'))
    ap.add_argument('--list', action='store_true',
                    help="print the tree's own answer, which is what the table states")
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()

    if a.selftest:
        return 0 if selftest() else 1

    if a.list:
        header = open(a.header, encoding='utf-8').read()
        taken, filled, aliases = scan_tree(os.path.abspath(a.tree))
        for struct, live in ((IMPORT_STRUCT, taken), (EXPORT_STRUCT, filled)):
            for name in members(header, struct) or []:
                print('%-20s %-26s %s' % (struct, name,
                                          'taken' if name in live else '-'))
        for rel, line, name in aliases:
            print('# %s:%d aliases gex as %s' % (rel, line, name))
        return 0

    findings, rows = check(os.path.abspath(a.tree), a.header, a.spec)
    for f in findings:
        print('!! %s' % f)
    print('engineapi: %d row(s) checked, %d finding(s)' % (rows, len(findings)))
    return 1 if findings else 0


if __name__ == '__main__':
    sys.exit(main())
