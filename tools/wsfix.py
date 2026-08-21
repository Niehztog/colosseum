"""Reproduce q2pro's 'Fix some whitespace issues in game code.'"""
import re, sys

def fix(text):
    text = '\n'.join(l.rstrip() for l in text.split('\n'))     # trailing whitespace
    text = re.sub(r'\n{3,}', '\n\n', text)                      # collapse blank runs
    # space before ';' inside for(...) headers
    def forfix(m):
        return 'for (' + re.sub(r'\s+;', ';', m.group(1)) + ')'
    text = re.sub(r'for \(([^)\n]*)\)', forfix, text)
    text = re.sub(r'(\b\w+)\s+\[(\s*\d*\s*)\]', r'\1[\2]', text)   # `name [] =` -> `name[] =`
    return text

if __name__ == '__main__':
    for p in sys.argv[1:]:
        t = open(p, encoding='latin-1').read()
        n = fix(t)
        if n != t: open(p, 'w', encoding='latin-1', newline='').write(n)
