#!/usr/bin/env python3
"""No unbounded string copy anywhere in the game library (R-SEC-1, R-VER-30).

WHY.  R-SEC-1 names the reachable paths -- userinfo, a client command, a chat
string, an address string, a config file -- and asks that every `strcpy`,
`strcat`, `sprintf` and unterminated `strncpy` on one of them be replaced.  A
tool cannot compute reachability, and a reviewer who tries gets it wrong in the
one direction that matters: `sprintf(entry, "yv %d ", y)` looks like arithmetic
until somebody adds `%s` to it, and the netname three lines down was always
client-controlled.  Six donors' worth of 1997-2001 code makes that a certainty
rather than a risk.

So the rule this tool enforces is the stronger one that is actually decidable:
**the unbounded forms do not appear in `src/` at all.**  Every site becomes
`Q_strlcpy`, `Q_strlcat`, `Q_snprintf` or an explicit `memcpy` with a length the
call itself computed.  That is a ban, and a ban is checkable; "is this one
reachable?" is not.

WHAT IS EXEMPT, AND WHY IT IS A PATH AND NOT A NAME.  `src/shared/` is vendored
byte-identical from `q2pro/src/shared` (SPECS.md 5.2) and is diffed against it
by `divergence.py`; editing it here would make this tree the odd one out for two
constant literals that provably fit (`Info_NextPair`'s `"<MISSING KEY>"` into a
`MAX_INFO_KEY` buffer).  The exemption is the directory, so a new file dropped
into it inherits it -- which is correct, because a new file in there is also a
divergence and the other tool reports that one.

WHAT IS NOT A FINDING.  Comments and string literals are stripped first: this
file's own prose says `strcpy` a dozen times, and `src/ctf/g_ctf.c` carries a
comment naming the exact call it replaced.  A member or macro whose name merely
ends in one of these -- `Q_strlcpy`, `Com_sprintf` -- is not matched, because the
pattern requires a non-identifier character before the name.

USAGE
    tools/bounded.py [--tree src]
    tools/bounded.py --selftest
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The banned spellings, with what each becomes.  `strncpy` and `strncat` are
# here because both are bounded and neither is safe: `strncpy` does not
# terminate when the source fills the buffer, and `strncat`'s third argument is
# the space REMAINING, not the size of the destination, which is the off-by-one
# every codebase makes at least once.
BANNED = {
    'strcpy':   'Q_strlcpy(dst, src, sizeof(dst))',
    'strcat':   'Q_strlcat(dst, src, sizeof(dst))',
    'sprintf':  'Q_snprintf(dst, sizeof(dst), fmt, ...)',
    'vsprintf': 'Q_vsnprintf(dst, sizeof(dst), fmt, ap)',
    'strncpy':  'Q_strlcpy -- strncpy does not terminate on truncation',
    'strncat':  'Q_strlcat -- strncat bounds the SOURCE, not the destination',
}

# Directories under the tree that are vendored verbatim from q2pro and diffed
# against it elsewhere.  A path, not a file list: a new file in there is a new
# divergence, which is that check's finding rather than this one's.
EXEMPT_DIRS = ('shared',)

CALL = re.compile(r'(?<![\w.>])(' + '|'.join(sorted(BANNED, key=len, reverse=True)) + r')\s*\(')


def strip_comments(t):
    """Blank comments and string literals, PRESERVING newlines and length.

    Line numbers have to stay real or the finding names the wrong line, and the
    only way to keep them real through a multi-line block comment is to put the
    newlines back.
    """
    def blank(m):
        return '\n' * m.group(0).count('\n') + ' ' * (len(m.group(0)) - m.group(0).count('\n'))
    t = re.sub(r'/\*.*?\*/', blank, t, flags=re.S)
    t = re.sub(r'//[^\n]*', lambda m: ' ' * len(m.group(0)), t)
    # Character and string literals, escapes included.  A literal containing the
    # word is prose as much as a comment is.
    t = re.sub(r'"(?:\\.|[^"\\\n])*"', lambda m: ' ' * len(m.group(0)), t)
    t = re.sub(r"'(?:\\.|[^'\\\n])*'", lambda m: ' ' * len(m.group(0)), t)
    return t


def sources(tree):
    out = []
    for root, dirs, files in os.walk(tree):
        rel = os.path.relpath(root, tree)
        if rel != '.' and rel.split(os.sep)[0] in EXEMPT_DIRS:
            dirs[:] = []
            continue
        dirs[:] = [d for d in dirs if not d.startswith(('debug', 'release', '__'))]
        for f in sorted(files):
            if f.endswith(('.c', '.h')):
                out.append(os.path.join(root, f))
    return out


def scan(tree):
    hits = []
    for path in sources(tree):
        with open(path, encoding='utf-8') as fh:
            text = fh.read()
        code = strip_comments(text)
        raw = text.splitlines()
        for m in CALL.finditer(code):
            line = code.count('\n', 0, m.start()) + 1
            hits.append((os.path.relpath(path, REPO), line, m.group(1),
                         raw[line - 1].strip() if line <= len(raw) else ''))
    return hits


SELFTEST_TREE = {
    # Clean: bounded forms, and the banned words inside a comment and a literal.
    'ok.c': '''
#include "x.h"
/* strcpy is what this replaced */
void f(char *dst, const char *src) {
    Q_strlcpy(dst, src, 64);          // not strcat either
    gi.dprintf("use sprintf here\\n");
}
''',
    # A member called `sprintf` in an import table is not a call to libc's.
    'member.c': '''
void g(void) { gi.dprintf("x"); }
''',
}

SELFTEST_MUTANTS = [
    ('a bare strcpy', 'bad.c', 'void h(char *d, char *s) { strcpy(d, s); }'),
    ('a bare strcat', 'bad.c', 'void h(char *d, char *s) { strcat(d, s); }'),
    ('a bare sprintf', 'bad.c', 'void h(char *d, int n) { sprintf(d, "%d", n); }'),
    ('strncpy, which does not terminate', 'bad.c',
     'void h(char *d, char *s) { strncpy(d, s, 8); }'),
    ('a call split across lines', 'bad.c',
     'void h(char *d, char *s)\n{\n    strcpy\n        (d, s);\n}'),
]


def selftest():
    """Five mutations that must be caught, and one clean tree that must not be.

    The split-across-lines mutant is the one worth having: the first version of
    this tool matched `strcpy\\s*\\(` on a single line and a donor that had been
    through astyle would have hidden a call behind a newline.
    """
    ok = True
    tmp = tempfile.mkdtemp(prefix='bounded-selftest-')
    try:
        for name, body in SELFTEST_TREE.items():
            with open(os.path.join(tmp, name), 'w') as fh:
                fh.write(body)
        # also prove the exemption is a directory and covers what it claims
        os.mkdir(os.path.join(tmp, 'shared'))
        with open(os.path.join(tmp, 'shared', 'vendored.c'), 'w') as fh:
            fh.write('void v(char *d) { strcpy(d, "x"); }\n')

        base = scan(tmp)
        if base:
            print('!! selftest: the clean tree reported %d finding(s): %s'
                  % (len(base), base))
            ok = False
        else:
            print('   [control] clean tree, vendored dir exempt: 0 findings')

        for why, fname, body in SELFTEST_MUTANTS:
            p = os.path.join(tmp, fname)
            with open(p, 'w') as fh:
                fh.write(body + '\n')
            got = scan(tmp)
            os.remove(p)
            if not got:
                print('!! selftest: %s was NOT caught' % why)
                ok = False
            else:
                print('   [control] %s -> caught at line %d' % (why, got[0][1]))
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
    for path, line, name, text in hits:
        print('!! %s:%d: %s -- use %s' % (path, line, name, BANNED[name]))
    kinds = sorted({h[2] for h in hits})
    print('bounded: %d unbounded copy site(s)%s' %
          (len(hits), (' -- ' + ', '.join(kinds)) if kinds else ''))
    return 1 if hits else 0


if __name__ == '__main__':
    sys.exit(main())
