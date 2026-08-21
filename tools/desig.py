"""Convert positional gitem_t initialisers in itemlist[] to designated ones,
matching the form q2pro adopted in 'Convert itemlist[] to designated initializers.'"""
import re, sys

FIELDS = ['classname', 'pickup', 'use', 'drop', 'weaponthink', 'pickup_sound',
          'world_model', 'world_model_flags', 'view_model', 'icon', 'pickup_name',
          'count_width', 'quantity', 'ammo', 'flags', 'weapmodel', 'info', 'tag',
          'precaches']
EMPTY = {'NULL', '0', '""', ''}

def strip_comments(s):
    s = re.sub(r'/\*.*?\*/', '', s, flags=re.S)
    s = re.sub(r'//[^\n]*', '', s)
    return s

def split_top(s):
    """Split on top-level commas, respecting (), [], {} and string literals."""
    parts, cur, depth, i, instr = [], [], 0, 0, False
    while i < len(s):
        c = s[i]
        if instr:
            cur.append(c)
            if c == '\\': cur.append(s[i+1]); i += 2; continue
            if c == '"': instr = False
            i += 1; continue
        if c == '"': instr = True; cur.append(c)
        elif c in '([{': depth += 1; cur.append(c)
        elif c in ')]}': depth -= 1; cur.append(c)
        elif c == ',' and depth == 0: parts.append(''.join(cur)); cur = []
        else: cur.append(c)
        i += 1
    parts.append(''.join(cur))
    return parts

def convert_entry(body):
    vals = [v.strip() for v in split_top(strip_comments(body))]
    trailing_comma = bool(vals) and vals[-1] == ''
    while vals and vals[-1] == '': vals.pop()
    if len(vals) > len(FIELDS):
        raise ValueError(f'{len(vals)} values > {len(FIELDS)} fields')
    if not [v for v in vals if v not in EMPTY]:
        return None                                # list terminator entry
    out = []
    for i, (name, v) in enumerate(zip(FIELDS, vals)):
        if v in EMPTY: continue
        out.append('        %-20s= %s,' % ('.' + name, v))
    return '\n'.join(out)

def convert(text):
    m = re.search(r'gitem_t itemlist\[\] = \{', text)
    if not m: return text, 0
    start = m.end()
    depth, i = 1, start
    while depth:
        if text[i] == '{': depth += 1
        elif text[i] == '}': depth -= 1
        i += 1
    arr, head, tail = text[start:i-1], text[:start], text[i-1:]
    out, j, n = [], 0, 0
    while j < len(arr):
        c = arr[j]
        if c == '{':
            d, k = 1, j + 1
            while d:
                if arr[k] == '{': d += 1
                elif arr[k] == '}': d -= 1
                k += 1
            body = arr[j+1:k-1]
            first = next((l.strip() for l in body.split('\n') if l.strip()), '')
            if first.startswith('.') and '=' in first:
                out.append(arr[j:k])            # already designated
            else:
                try:
                    conv = convert_entry(body)
                    out.append('{ NULL }' if conv is None else '{\n' + conv + '\n    }')
                    n += 1
                except ValueError as e:
                    print(f'  skipped an entry: {e}', file=sys.stderr)
                    out.append(arr[j:k])
            j = k
        else:
            out.append(c); j += 1
    return head + ''.join(out) + tail, n

if __name__ == '__main__':
    for p in sys.argv[1:]:
        t = open(p, encoding='latin-1').read()
        new, n = convert(t)
        open(p, 'w', encoding='latin-1', newline='').write(new)
        print(f'{p}: converted {n} entries')
