"""Convert a set of `float <x>_time` timers to `int <x>_framenum`, the way q2pro's
'Convert ... to frame numbers.' commits did.

  timers.py old=new [old=new ...] -- file.c file.h ...
"""
import re, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from nextthink import simple

def conv_rhs(expr):
    expr = expr.strip()
    if expr == 'FRAMETIME':
        return 'level.framenum + 1'
    m = re.match(r'^\((.*)\s*\*\s*FRAMETIME\)$', expr) or re.match(r'^(.*?)\s*\*\s*FRAMETIME$', expr)
    if m:
        return 'level.framenum + ' + m.group(1).strip()
    return 'level.framenum + ' + (expr if simple(expr) else f'({expr})') + ' * BASE_FRAMERATE'

def fix(text, mapping):
    for old, new in mapping.items():
        text = re.sub(rf'\b{re.escape(old)}\b', new, text)
    names = '|'.join(re.escape(n) for n in mapping.values())
    text = re.sub(rf'((?:[\w\->\.\[\]]*)(?:{names}))\s*=\s*level\.time\s*\+\s*([^;]+);',
                  lambda m: f'{m.group(1)} = {conv_rhs(m.group(2))};', text)
    text = re.sub(rf'((?:[\w\->\.\[\]]*)(?:{names}))\s*=\s*level\.time\s*;',
                  r'\1 = level.framenum;', text)
    text = re.sub(rf'((?:[\w\->\.\[\]]*)(?:{names}))\s*([<>=!]=?)\s*level\.time',
                  r'\1 \2 level.framenum', text)
    text = re.sub(rf'level\.time\s*([<>=!]=?)\s*((?:[\w\->\.\[\]]*)(?:{names}))',
                  r'level.framenum \1 \2', text)
    # `(level.time + X) < field`  ->  `(level.framenum + X * BASE_FRAMERATE) < field`
    text = re.sub(rf'\(level\.time \+ ([0-9.]+f?)\)(\s*[<>]=?\s*)((?:[\w\->\.\[\]]*)(?:{names}))',
                  r'(level.framenum + \1 * BASE_FRAMERATE)\2\3', text)
    # declarations
    for new in mapping.values():
        text = re.sub(rf'\bfloat(\s+){re.escape(new)}\b', rf'int\1{new}', text)
        text = text.replace(f'F({new})', f'I({new})')
    return text

if __name__ == '__main__':
    i = sys.argv.index('--')
    mapping = dict(a.split('=') for a in sys.argv[1:i])
    for p in sys.argv[i+1:]:
        t = open(p, encoding='latin-1').read()
        n = fix(t, mapping)
        if n != t: open(p, 'w', encoding='latin-1', newline='').write(n)
