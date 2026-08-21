"""Field-by-field itemlist audit that also compares precache ARRAYS.

auditems.py only captured single-line `.field = value,` pairs, so multi-line
`.precaches = (const char *const[]) { ... }` initialisers were invisible to it
-- which is exactly the field the railgun regression landed in.  This walks the
braces instead of regexing lines.
"""
import re, sys, os

def entries(path):
    t = open(path, encoding='latin-1').read()
    m = re.search(r'gitem_t itemlist\[\] = \{', t)
    i, d = m.end(), 1
    while d:
        if t[i] == '{': d += 1
        elif t[i] == '}': d -= 1
        i += 1
    arr, out, j = t[m.end():i - 1], {}, 0
    while j < len(arr):
        if arr[j] == '{':
            dd, k = 1, j + 1
            while dd:
                if arr[k] == '{': dd += 1
                elif arr[k] == '}': dd -= 1
                k += 1
            body = arr[j + 1:k - 1]
            fields = dict(re.findall(r'\.(\w+)\s*=\s*(.+?),\s*(?://.*)?$', body, re.M))
            # precaches: take the string literals inside the array initialiser
            pm = re.search(r'\.precaches\s*=\s*\(const char \*const\s*\[\]\)\s*\{(.*?)\}',
                           body, re.S)
            if pm:
                fields['precaches'] = tuple(re.findall(r'"([^"]*)"', pm.group(1)))
            else:
                sm = re.search(r'\.precaches\s*=\s*"([^"]*)"', body)
                if sm:
                    fields['precaches'] = tuple(sm.group(1).split())
            name = fields.get('classname')
            if name: out[name] = fields
            j = k
        else:
            j += 1
    return out

base = entries(sys.argv[1])
bad = 0
for path in sys.argv[2:]:
    var = entries(path)
    shared = sorted(set(base) & set(var))
    label = os.path.relpath(path, os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.abspath(__file__)))))
    diffs = []
    for name in shared:
        b, a = base[name], var[name]
        for k in sorted(set(b) | set(a)):
            if b.get(k) != a.get(k):
                diffs.append((name, k, b.get(k), a.get(k)))
    print(f'== {label}: {len(var)} items, {len(shared)} shared, {len(diffs)} field diff(s)')
    for name, k, bv, av in diffs:
        print(f'   {name}.{k}\n      baseq2: {bv}\n      variant: {av}')
    bad += len(diffs)
print(f'\nTOTAL differing fields across shared items: {bad}')
