#!/usr/bin/env python3
"""No unbounded string copy or raw token scan anywhere in the game library.

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

The arena.cfg reader used unbounded `fscanf(..., "%s", ...)` for a raw config
token.  Unlike the copy calls, a bounded replacement must reject a token that
does not fit rather than silently parse its tail as another token.  This audit
therefore rejects every unbounded string conversion in fscanf as well.

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
FSCANF_ADVICE = 'read and reject an overlong token explicitly'

# Directories under the tree that are vendored verbatim from q2pro and diffed
# against it elsewhere.  A path, not a file list: a new file in there is a new
# divergence, which is that check's finding rather than this one's.
EXEMPT_DIRS = ('shared',)

CALL = re.compile(r'(?<![\w.>])(' + '|'.join(sorted(BANNED, key=len, reverse=True)) + r')\s*\(')
FSCANF = re.compile(r'(?<![\w.>])fscanf\s*\(')
# A scanset has the same destination-buffer rule as an s conversion.  The
# optional zeroes reject `%0s`, which does not establish a positive bound.
# `%4095s` and `%9[abc]` remain permitted.  An even run of percent signs is
# literal text; an odd run ends in the actual conversion.
UNBOUNDED_FSCANF_STRING = re.compile(
    r'(?<!%)(?:%%)*%(?:\*)?0*(?:hh|ll|[hljztL])*(?:s|\[)')


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


def strip_comments_keep_literals(t):
    """Blank comments while retaining literals at their original offsets."""
    out = list(t)
    i = 0
    while i < len(t):
        if t.startswith('//', i):
            end = t.find('\n', i)
            end = len(t) if end < 0 else end
            out[i:end] = ' ' * (end - i)
            i = end
        elif t.startswith('/*', i):
            end = t.find('*/', i + 2)
            end = len(t) - 2 if end < 0 else end
            for n in range(i, end + 2):
                if t[n] != '\n':
                    out[n] = ' '
            i = end + 2
        elif t[i] in '"\'':
            quote = t[i]
            i += 1
            while i < len(t):
                if t[i] == '\\':
                    i += 2
                    continue
                if t[i] == quote:
                    i += 1
                    break
                i += 1
        else:
            i += 1
    return ''.join(out)


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
        literals = strip_comments_keep_literals(text)
        raw = text.splitlines()
        for m in CALL.finditer(code):
            line = code.count('\n', 0, m.start()) + 1
            hits.append((os.path.relpath(path, REPO), line, m.group(1),
                         raw[line - 1].strip() if line <= len(raw) else ''))
        for m in FSCANF.finditer(code):
            end = literals.find(';', m.start())
            if end < 0:
                continue
            if UNBOUNDED_FSCANF_STRING.search(literals[m.start():end]):
                line = code.count('\n', 0, m.start()) + 1
                hits.append((os.path.relpath(path, REPO), line, 'fscanf %s',
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
    # A width makes the conversion bounded; the arena parser needs its own
    # stricter reject-not-split helper, but this must not reject a safe scanf.
    'arena/maploop.c': '''
void read_word(FILE *fp, char *word) {
    fscanf(fp, "%4095s", word);
    fscanf(fp, "%9[abc]", word);
    fscanf(fp, "%%s", word);
}
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
    ('an unbounded fscanf string token', 'arena/maploop.c',
     'void h(FILE *fp, char *d) { fscanf(fp, "%s", d); }'),
    ('a zero-width fscanf string token', 'arena/maploop.c',
     'void h(FILE *fp, char *d) { fscanf(fp, "%0s", d); }'),
]


def selftest():
    """Seven mutations that must be caught, and one clean tree that must not be.

    The split-across-lines mutant is the one worth having: the first version of
    this tool matched `strcpy\\s*\\(` on a single line and a donor that had been
    through astyle would have hidden a call behind a newline.
    """
    ok = True
    tmp = tempfile.mkdtemp(prefix='bounded-selftest-')
    try:
        for name, body in SELFTEST_TREE.items():
            path = os.path.join(tmp, name)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'w') as fh:
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
        advice = FSCANF_ADVICE if name == 'fscanf %s' else BANNED[name]
        print('!! %s:%d: %s -- use %s' % (path, line, name, advice))
    kinds = sorted({h[2] for h in hits})
    print('bounded: %d unbounded copy or raw-scan site(s)%s' %
          (len(hits), (' -- ' + ', '.join(kinds)) if kinds else ''))
    return 1 if hits else 0


if __name__ == '__main__':
    sys.exit(main())
