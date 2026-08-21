"""Add `static` to file-local function definitions, as q2pro's
'Make more game definitions static' commits did — extended to variant-only code."""
import os, re, sys
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'replay')
DEF = re.compile(r'^(?!static\b)(?!extern\b)([A-Za-z_][\w ]*?[\w\*]\s*\*?\s*)([A-Za-z_]\w*)\s*\(([^;]*)\)\s*$')

files = [f for f in sorted(os.listdir(R)) if f.endswith(('.c', '.h'))]
text = {f: open(os.path.join(R, f), encoding='latin-1').read() for f in files}

targets = sys.argv[1:]
for f in targets:
    lines = text[f].split('\n')
    out, n = [], 0
    for i, l in enumerate(lines):
        m = DEF.match(l)
        if m and i + 1 < len(lines) and lines[i+1].startswith('{') and not m.group(2).startswith('SP_'):
            name = m.group(2)
            elsewhere = any(re.search(rf'\b{re.escape(name)}\b', t)
                            for g, t in text.items() if g != f)
            if not elsewhere:
                out.append('static ' + l); n += 1; continue
        out.append(l)
    if n:
        open(os.path.join(R, f), 'w', encoding='latin-1', newline='').write('\n'.join(out))
    print(f'  {f}: {n} definition(s) made static')
