"""Convert nextthink from seconds to frame numbers, as q2pro's
'Convert nextthink to frame numbers.' did."""
import re, sys

def simple(e):
    e = e.replace('->', '@')          # the arrow's '-' is not an operator
    d = 0
    for c in e:
        if c in '([': d += 1
        elif c in ')]': d -= 1
        elif c in '+-' and d == 0: return False
    return True

def conv_rhs(expr):
    expr = expr.strip()
    if expr == 'FRAMETIME':
        return 'level.framenum + 1'
    m = re.match(r'^\((.*)\s*\*\s*FRAMETIME\)$', expr) or re.match(r'^(.*?)\s*\*\s*FRAMETIME$', expr)
    if m:
        return 'level.framenum + ' + m.group(1).strip()
    return 'level.framenum + ' + (expr if simple(expr) else f'({expr})') + ' * BASE_FRAMERATE'

def fix(text):
    # assignments:  X->nextthink = level.time + <expr>;
    def a(m):
        return f'{m.group(1)}nextthink = {conv_rhs(m.group(2))};'
    text = re.sub(r'([\w\->\.\[\]]*?)nextthink\s*=\s*level\.time\s*\+\s*([^;]+);', a, text)
    text = re.sub(r'([\w\->\.\[\]]*?)nextthink\s*=\s*level\.time\s*;', r'\1nextthink = level.framenum;', text)
    # comparisons and differences against level.time
    text = re.sub(r'([\w\->\.\[\]]*nextthink)\s*([-<>=!]=?)\s*level\.time', r'\1 \2 level.framenum', text)
    text = re.sub(r'level\.time\s*([-<>=!]=?)\s*([\w\->\.\[\]]*nextthink)', r'level.framenum \1 \2', text)
    # the field itself becomes an int, and the savegame descriptor follows
    text = re.sub(r'(\bfloat\b)(\s+nextthink\s*;)', r'int  \2', text)
    text = text.replace('    F(nextthink),', '    I(nextthink),')
    return text

if __name__ == '__main__':
    for p in sys.argv[1:]:
        t = open(p, encoding='latin-1').read()
        n = fix(t)
        if n != t: open(p, 'w', encoding='latin-1', newline='').write(n)
