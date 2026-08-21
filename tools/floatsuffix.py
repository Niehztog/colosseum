"""Add the `f` suffix to floating point constants, as q2pro's
'Add proper suffix to floating point constants.' did."""
import re, sys

NUM = re.compile(r'(?<![\w.])(?:\d+\.\d*|\.\d+)(?![\w.])')

def fix_code(s):
    return NUM.sub(lambda m: m.group(0) + 'f', s)

INIT_START = re.compile(r'=\s*\{\s*$')

def fix(text):
    out = []
    in_block = False
    init_depth = 0
    for line in text.split('\n'):
        # q2pro's pass left aggregate initialisers alone (frame tables, vec3_t
        # constants, gitem_armor_t): only expressions got the suffix.
        if line[:1] not in ('', ' ', '\t', '}') and not INIT_START.search(line) \
           and '= {' not in line and '={' not in line:
            init_depth = 0            # a new top-level construct ends any initialiser
        if init_depth:
            init_depth += line.count('{') - line.count('}')
            out.append(line)
            continue
        if re.search(r'=\s*\{.*\}', line):          # initialiser complete on one line
            out.append(line)
            continue
        if INIT_START.search(line) or re.search(r'=\s*\{[^}]*$', line):
            init_depth = 1 + line.count('{') - line.count('}') - 1
            if init_depth < 0: init_depth = 0
            out.append(line)
            if init_depth: continue
        res, i, n = [], 0, len(line)
        buf = ''
        while i < n:
            if in_block:
                j = line.find('*/', i)
                if j < 0: res.append(line[i:]); i = n; break
                res.append(line[i:j+2]); i = j + 2; in_block = False; continue
            c = line[i]
            if c == '"' or c == "'":
                res.append(fix_code(buf)); buf = ''
                q, j = c, i + 1
                while j < n:
                    if line[j] == '\\': j += 2; continue
                    if line[j] == q: j += 1; break
                    j += 1
                res.append(line[i:j]); i = j; continue
            if line.startswith('//', i):
                res.append(fix_code(buf)); buf = ''
                res.append(line[i:]); i = n; break
            if line.startswith('/*', i):
                res.append(fix_code(buf)); buf = ''
                j = line.find('*/', i + 2)
                if j < 0: res.append(line[i:]); in_block = True; i = n; break
                res.append(line[i:j+2]); i = j + 2; continue
            buf += c; i += 1
        res.append(fix_code(buf))
        out.append(''.join(res))
    return '\n'.join(out)

if __name__ == '__main__':
    for p in sys.argv[1:]:
        t = open(p, encoding='latin-1').read()
        n = fix(t)
        if n != t: open(p, 'w', encoding='latin-1', newline='').write(n)
