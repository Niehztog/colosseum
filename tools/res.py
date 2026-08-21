"""Resolve conflict hunks in the replay repo.

  res.py FILE theirs|ours|both [HUNK]   pick a side for hunk N (default: all hunks)
  res.py FILE show  [HUNK]              print hunk(s) with the three sides labelled
  res.py FILE text  HUNK  < newtext     replace hunk N with text read from stdin
"""
import os, sys

R = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'replay')

def parse(path):
    lines = open(path, encoding='latin-1').read().split('\n')
    out, i = [], 0
    while i < len(lines):
        if lines[i].startswith('<<<<<<<'):
            ours, base, theirs = [], [], None
            i += 1; cur = ours
            while not lines[i].startswith('>>>>>>>'):
                if lines[i].startswith('|||||||'): cur = base
                elif lines[i].startswith('======='): theirs = []; cur = theirs
                else: cur.append(lines[i])
                i += 1
            i += 1
            out.append({'ours': ours, 'base': base, 'theirs': theirs if theirs is not None else []})
        else:
            out.append(lines[i]); i += 1
    return out

def render(parts):
    res = []
    for p in parts:
        if isinstance(p, str): res.append(p)
        else:
            res += ['<<<<<<< HEAD'] + p['ours'] + ['||||||| base'] + p['base'] + ['======='] + p['theirs'] + ['>>>>>>> theirs']
    return '\n'.join(res)

f = sys.argv[1]; mode = sys.argv[2]
path = os.path.join(R, f)
parts = parse(path)
idx = [i for i, p in enumerate(parts) if isinstance(p, dict)]

if mode == 'show':
    which = [int(sys.argv[3])] if len(sys.argv) > 3 else range(len(idx))
    for k in which:
        p = parts[idx[k]]
        print(f'===== hunk {k} =====')
        for name in ('ours', 'base', 'theirs'):
            print(f'--- {name} ({len(p[name])} lines)')
            for l in p[name]: print('   ', l)
    sys.exit(0)

which = [int(sys.argv[3])] if len(sys.argv) > 3 else list(range(len(idx)))
if mode == 'text':
    new = sys.stdin.read().split('\n')
    if new and new[-1] == '': new.pop()
    parts[idx[which[0]]] = '\n'.join(new)
elif mode == 'splice':
    # ours == <variant additions> + <the base lines q2pro rewrote>.
    # Keep the additions, then take q2pro's rewritten lines.
    for k in which:
        p = parts[idx[k]]
        o, b = p['ours'], p['base']
        cut = 0
        for n in range(min(len(o), len(b)), 0, -1):
            if o[-n:] == b[-n:]: cut = n; break
        if not cut:
            print(f'hunk {k}: no common tail with base; left alone'); continue
        parts[idx[k]] = '\n'.join(o[:-cut] + p['theirs'])
else:
    for k in which:
        p = parts[idx[k]]
        parts[idx[k]] = '\n'.join(p['theirs'] if mode == 'theirs'
                                  else p['ours'] if mode == 'ours'
                                  else p['ours'] + p['theirs'])
open(path, 'w', encoding='latin-1', newline='').write(render(parts))
left = sum(1 for p in parse(path) if isinstance(p, dict))
print(f'{f}: {left} conflict hunk(s) remaining')
