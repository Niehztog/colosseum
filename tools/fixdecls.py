"""Make forward declarations agree with definitions that were just made static."""
import os, re, sys
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'replay')
n_tot = 0
for f in sorted(os.listdir(R)):
    if not f.endswith('.c'): continue
    p = os.path.join(R, f)
    t = open(p, encoding='latin-1').read()
    statics = set(re.findall(r'^static\s+[\w \*]+?\b(\w+)\s*\([^;]*\)\s*$', t, re.M))
    if not statics: continue
    out, n = [], 0
    for l in t.split('\n'):
        m = re.match(r'^(extern\s+)?((?:[\w]+\s+)+\*?)(\w+)\s*\(([^;]*)\);\s*$', l)
        if m and m.group(3) in statics and not l.startswith('static'):
            out.append(f'static {m.group(2)}{m.group(3)}({m.group(4)});'); n += 1
        else:
            out.append(l)
    if n:
        open(p, 'w', encoding='latin-1', newline='').write('\n'.join(out))
        print(f'  {f}: {n} declaration(s) aligned')
    n_tot += n
print(f'total: {n_tot}')
