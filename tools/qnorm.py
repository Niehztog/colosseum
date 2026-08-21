"""Reproduce q2pro's baseq2 import-time normalisation of the official id sources.

Validated to reproduce q2pro commit 629da4a5 (`Imported baseq2 game source.`)
byte-for-byte from the official quake2-3.21 `game/` sources, then applied
identically to the official ctf/xatrix/rogue sources so that q2pro's subsequent
commits can be replayed onto them with git's 3-way merge.
"""
import re, sys, os

ARRAY_START = re.compile(r'^\s*(?:static\s+)?(?:mframe_t|vec3_t)\s+\w+\s*\[\s*\]\s*=')

def expand_tabs(line, ts=4):
    out, col = [], 0
    for ch in line:
        if ch == '\t':
            n = ts - (col % ts); out.append(' ' * n); col += n
        else:
            out.append(ch); col += 1
    return ''.join(out)

def _split_comment(s):
    """Split trailing // comment that is not inside a string literal."""
    instr = False; i = 0
    while i < len(s) - 1:
        c = s[i]
        if c == '\\' and instr: i += 2; continue
        if c == '"': instr = not instr
        elif not instr and c == '/' and s[i+1] == '/': return s[:i], s[i:]
        i += 1
    return s, ''

def brace_arrays(lines, report, fname):
    out, i, n = [], 0, len(lines)
    while i < n:
        line = lines[i]
        if not ARRAY_START.match(line):
            out.append(line); i += 1; continue
        out.append(line); i += 1
        depth = line.count('{') - line.count('}')
        if depth <= 0:                       # opening brace on a following line
            while i < n and not lines[i].strip():
                out.append(lines[i]); i += 1
            if i >= n or not lines[i].lstrip().startswith('{'):
                continue                     # not the shape we expect; leave alone
            out.append(lines[i]); depth = 1; i += 1
        while i < n and depth > 0:
            cur = lines[i]
            depth += cur.count('{') - cur.count('}')
            if depth <= 0:
                out.append(cur); i += 1; break
            stripped = cur.strip()
            if not stripped or stripped.startswith(('//', '#', '/*', '*')):
                out.append(cur); i += 1; continue
            indent = cur[:len(cur) - len(cur.lstrip())]
            tws = cur[len(cur.rstrip()):]                  # trailing whitespace
            head, comment = _split_comment(cur)
            ccol = len(head) if comment else None          # keep comment's column
            body = head.strip()
            had_comma = body.endswith(',')
            if had_comma: body = body[:-1].rstrip()
            if body.startswith('{'):
                out.append(cur); i += 1; continue
            d, commas = 0, 0
            for ch in body:
                if ch in '([{': d += 1
                elif ch in ')]}': d -= 1
                elif ch == ',' and d == 0: commas += 1
            if commas != 2:
                report.append(f'{fname}:{i+1}: {commas+1} fields, left alone: {body!r}')
                out.append(cur); i += 1; continue
            new = f'{indent}{{ {body} }}' + (',' if had_comma else '')
            if comment:
                new += ' ' * max(1, ccol - len(new)) + comment
            out.append(new.rstrip() + tws if not comment else new + tws)
            i += 1
    return out

SUBS = [
    (r'\btrue\b',           'qtrue'),
    (r'\bfalse\b',          'qfalse'),
    (r'\b_DotProduct\b',    'DotProduct'),
    (r'\b_VectorSubtract\b', 'VectorSubtract'),
    (r'\b_VectorAdd\b',     'VectorAdd'),
    (r'\b_VectorCopy\b',    'VectorCopy'),
    (r'\bCom_sprintf\b',    'Q_snprintf'),
]

def norm(text, report=None, fname=''):
    if report is None: report = []
    text = text.replace('\r\n', '\n').replace('\r', '\n')
    lines = [expand_tabs(l) for l in text.split('\n')]        # tabs first: sets column layout
    text = '\n'.join(brace_arrays(lines, report, fname))
    for pat, rep in SUBS:
        text = re.sub(pat, rep, text)
    text = re.sub(r'(gi\.error\s*\()\s*ERR_\w+\s*,\s*', r'\1', text)
    return text

if __name__ == '__main__':
    a, b = sys.argv[1], sys.argv[2]
    tot = same = 0; report = []
    for f in sorted(os.listdir(b)):
        pa, pb = os.path.join(a, f), os.path.join(b, f)
        if not os.path.isfile(pa): continue
        tot += 1
        na = norm(open(pa, encoding='latin-1').read(), report, f)
        nb = open(pb, encoding='latin-1').read()
        if na == nb: same += 1
        else:
            import difflib
            d = [x for x in difflib.unified_diff(na.split('\n'), nb.split('\n'), lineterm='', n=0)
                 if x[:1] in '+-' and x[:3] not in ('+++', '---')]
            print(f'{f}: {len(d)} differing lines')
    for r in report[:15]: print('  NOTE', r)
    print(f'== {same}/{tot} exact match')
