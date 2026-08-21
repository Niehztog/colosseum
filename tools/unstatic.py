"""Functions referenced from the generated g_ptrs.c must have external linkage."""
import os, re, sys
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'replay')
ptrs = open(os.path.join(R, 'g_ptrs.c'), encoding='latin-1').read()
names = set(re.findall(r'^extern\s+(?:const\s+)?[\w]+\s+\*?(\w+)\s*[;(]', ptrs, re.M))
print(f'{len(names)} symbol(s) externed by g_ptrs.c')
total = 0
for f in sorted(os.listdir(R)):
    if not f.endswith('.c') or f == 'g_ptrs.c': continue
    p = os.path.join(R, f); t = open(p, encoding='latin-1').read()
    out, n = [], 0
    for line in t.split('\n'):
        m = re.match(r'^static\s+((?:const\s+)?[\w]+\s+\*?)(\w+)\s*([;(])', line)
        if m and m.group(2) in names:
            out.append(line[len('static '):]); n += 1
        else:
            out.append(line)
    if n:
        open(p, 'w', encoding='latin-1', newline='').write('\n'.join(out))
        print(f'  {f}: {n} symbol(s) given external linkage')
    total += n
print(f'total: {total}')
