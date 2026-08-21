"""Apply regex substitutions across every .c/.h in the replay tree.

Used to extend a mechanical q2pro rename to the variant's own code, which a
diff-replay of the commit can never reach.
  sweep.py 'pat===rep' ['pat===rep' ...]   [--resolve]
--resolve first collapses any conflict hunks to their 'ours' side, so the
substitution then covers both sides uniformly.
"""
import os, re, sys

SP = os.path.dirname(os.path.abspath(__file__))
R  = os.path.join(SP, 'replay')

def collapse_ours(text):
    out, i, lines = [], 0, text.split('\n')
    while i < len(lines):
        if lines[i].startswith('<<<<<<<'):
            i += 1
            while not lines[i].startswith(('|||||||', '=======')):
                out.append(lines[i]); i += 1
            while not lines[i].startswith('>>>>>>>'): i += 1
            i += 1
        else:
            out.append(lines[i]); i += 1
    return '\n'.join(out)

resolve = '--resolve' in sys.argv
subs = [a.split('===') for a in sys.argv[1:] if a != '--resolve']
changed = 0
for f in sorted(os.listdir(R)):
    if not f.endswith(('.c', '.h')): continue
    p = os.path.join(R, f)
    t0 = open(p, encoding='latin-1').read()
    t = collapse_ours(t0) if resolve and '<<<<<<<' in t0 else t0
    for pat, rep in subs: t = re.sub(pat, rep, t)
    if t != t0:
        open(p, 'w', encoding='latin-1', newline='').write(t); changed += 1
print(f'{changed} file(s) updated')
