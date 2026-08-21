"""const-ify gitem_t pointers tree-wide, matching q2pro's 'Const-ify itemlist[].'
Pads to the next 4-column boundary the way q2pro's own edit did."""
import os, re
SP = os.path.dirname(os.path.abspath(__file__)); R = os.path.join(SP, 'replay')

def fix(m):
    lead, gap = m.group(1), m.group(2)
    name = 'const gitem_t'
    if ' ' in gap or len(gap) > 1:
        col = len(lead) + len(name)
        pad = ' ' * max(1, (-col) % 4 or 4)
        return f'{lead}{name}{pad}*'
    return f'{lead}{name} *'

n = 0
for f in sorted(os.listdir(R)):
    if not f.endswith(('.c', '.h')): continue
    p = os.path.join(R, f)
    t = open(p, encoding='latin-1').read()
    new = re.sub(r'(?<!const )(?<!\w)()gitem_t(\s+)\*', lambda m: fix(m), t)
    new = re.sub(r'(?<!const )(?<!\w)gitem_t \*', 'const gitem_t *', new)
    if new != t:
        open(p, 'w', encoding='latin-1', newline='').write(new); n += 1
print(f'{n} file(s) const-ified')
