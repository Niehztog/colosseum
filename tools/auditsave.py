"""Report variant-added persistent fields that no savegame descriptor covers."""
import os, re, sys
# R-TOOL-1: was tools/replay, a working tree that no longer exists.  The tree
# to audit is now argv[1], defaulting to this repo's src/.
_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
R = sys.argv[1] if len(sys.argv) > 1 else os.path.join(_REPO, 'src')
h = open(os.path.join(R, 'g_local.h'), encoding='latin-1').read()
save = open(os.path.join(R, 'g_save.c'), encoding='latin-1').read()

def struct_body(name, kind='struct'):
    if kind == 'struct':
        m = re.search(rf'struct {name} \{{(.*?)\n\}};', h, re.S)
    else:
        m = re.search(rf'typedef struct \{{(.*?)\n\}} {name};', h, re.S)
    return m.group(1) if m else ''

def fields(body):
    out = []
    for line in body.split('\n'):
        line = re.sub(r'//.*', '', line).strip()
        m = re.match(r'^(?:const\s+)?\w[\w \*]*?[\s\*](\w+)(\[[^\]]*\])?\s*;$', line)
        if m and not line.startswith(('#', '}')):
            out.append(m.group(1))
    return out

TARGETS = [('gclient_s', 'struct', 'client'), ('edict_s', 'struct', 'entity'),
           ('level_locals_t', 'typedef', 'level'), ('client_persistant_t', 'typedef', 'pers'),
           ('client_respawn_t', 'typedef', 'resp'), ('monsterinfo_t', 'typedef', 'monsterinfo')]
base = os.environ.get('BASE_H')
baseh = open(base, encoding='latin-1').read() if base else ''
for name, kind, label in TARGETS:
    body = struct_body(name, kind)
    if not body: continue
    for f in fields(body):
        if baseh and re.search(rf'\b{re.escape(f)}\s*(\[|;)', baseh): continue   # also in baseq2
        if re.search(rf'[A-Z]{{1,3}}\((?:[\w.]*\.)?{re.escape(f)}[,)\]]', save): continue
        if re.search(rf'\b{re.escape(f)}\b', save): continue
        print(f'  {label}.{f}')
print('(fields above have no savegame descriptor)')
