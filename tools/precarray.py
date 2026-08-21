"""Convert any remaining `.precaches = "a b c",` string initialisers into the
NULL-terminated arrays q2pro switched to."""
import os, re, sys
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'replay')
def conv(m):
    ind, items = m.group(1), m.group(2).split()
    body = '\n'.join(f'{ind}    "{i}",' for i in items)
    return f'{ind}.precaches          = (const char *const []) {{\n{body}\n{ind}    NULL\n{ind}}},'
for f in sys.argv[1:]:
    p = os.path.join(R, f); t = open(p, encoding='latin-1').read()
    new, n = re.subn(r'^([ \t]*)\.precaches\s*=\s*"([^"]*)",\s*$', conv, t, flags=re.M)
    if n: open(p, 'w', encoding='latin-1', newline='').write(new)
    print(f'  {f}: {n} precache string(s) -> array')
