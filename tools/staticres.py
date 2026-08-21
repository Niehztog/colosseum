"""Resolve a `Make ... static` commit's conflicts: keep the variant's side, but
prefix `static` on every definition q2pro staticised, matching on the function
name so a renamed or re-parametered variant signature still gets it."""
import os, re, sys
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'replay')

NAME = re.compile(r'^\s*(?:static\s+)?[A-Za-z_][\w \t\*]*?([A-Za-z_]\w*)\s*\(')

def names_made_static(base, theirs):
    b = {}
    for l in base:
        m = NAME.match(l)
        if m and 'static' not in l.split(m.group(1))[0]:
            b[m.group(1)] = l
    out = set()
    for l in theirs:
        m = NAME.match(l)
        if m and re.match(r'^\s*static\b', l) and m.group(1) in b:
            out.add(m.group(1))
    return out

def run(path):
    lines = open(path, encoding='latin-1').read().split('\n')
    out, i, n = [], 0, 0
    while i < len(lines):
        if lines[i].startswith('<<<<<<<'):
            ours, base, theirs, cur = [], [], [], None
            cur = ours; i += 1
            while not lines[i].startswith('>>>>>>>'):
                if lines[i].startswith('|||||||'): cur = base
                elif lines[i].startswith('======='): cur = theirs
                else: cur.append(lines[i])
                i += 1
            i += 1
            todo = names_made_static(base, theirs)
            res = []
            for l in ours:
                m = NAME.match(l)
                if m and m.group(1) in todo and not re.match(r'^\s*static\b', l):
                    ind = l[:len(l) - len(l.lstrip())]
                    l = ind + 'static ' + l.lstrip()
                res.append(l)
            out.extend(res); n += 1
        else:
            out.append(lines[i]); i += 1
    open(path, 'w', encoding='latin-1', newline='').write('\n'.join(out))
    return n

for f in sys.argv[1:]:
    print(f'{f}: {run(os.path.join(R, f))} hunk(s) resolved as ours + static')
