"""Balance braces inside #if 0 blocks so astyle does not mis-indent the rest of the file.
The blocks are dead code; the inserted braces only close initialisers that the
original id sources left dangling."""
import sys, re

def balance(text):
    lines = text.split('\n')
    out, i = [], 0
    while i < len(lines):
        l = lines[i]
        if re.match(r'^\s*#if\s+0\b', l):
            j, depth, nest = i + 1, 0, 0
            while j < len(lines):
                s = lines[j].strip()
                if re.match(r'^#if', s): nest += 1
                elif re.match(r'^#endif', s):
                    if nest == 0: break
                    nest -= 1
                depth += lines[j].count('{') - lines[j].count('}')
                j += 1
            out.append(l); out += lines[i+1:j]
            if depth > 0: out += ['}' * 1] * depth
            if j < len(lines): out.append(lines[j])
            i = j + 1
        else:
            out.append(l); i += 1
    return '\n'.join(out)

if __name__ == '__main__':
    for p in sys.argv[1:]:
        t = open(p, encoding='latin-1').read()
        b = balance(t)
        if b != t:
            open(p, 'w', encoding='latin-1', newline='').write(b)
            print(f'balanced {p}')
