"""Extract spawn-parseable map keys from a Quake II game tree, per the method:
tolerate both `{"key",` and `{ "key",`, and ASSERT a non-zero row count."""
import os, re, sys, glob

def read(p): return open(p, encoding='latin-1').read()

ROW = re.compile(r'^\s*\{\s*"([a-zA-Z_0-9]+)"\s*,\s*([A-Z]+)\(([a-zA-Z_0-9.]+)\)\s*,\s*(F_\w+)\s*(?:,\s*(FFL_\w+)\s*)?\}', re.M)
# id/mod single dual-purpose table, or q2pro's split pair
TABLES = [r'field_t\s+fields\[\]\s*=\s*\{', r'spawn_field_t\s+spawn_fields\[\]\s*=\s*\{',
          r'spawn_field_t\s+temp_fields\[\]\s*=\s*\{', r'spawn_fields\[\]\s*=\s*\{',
          r'temp_fields\[\]\s*=\s*\{']

def table_bodies(t):
    out = []
    for pat in TABLES:
        for m in re.finditer(pat, t):
            i = m.end(); d = 1; j = i
            while d and j < len(t):
                if t[j] == '{': d += 1
                elif t[j] == '}': d -= 1
                j += 1
            out.append(t[i:j])
    return out

def keys(path, parseable_only=True):
    """-> {key: (macro, member, ftype, flags)}"""
    t = read(path)
    out = {}
    for body in table_bodies(t):
        for m in ROW.finditer(body):
            key, macro, member, ftype, flags = m.groups()
            if parseable_only and flags == 'FFL_NOSPAWN':
                continue          # save-only in id's dual table; correctly absent from a split spawn table
            out[key] = (macro, member, ftype, flags or '')
    assert out, f'{path}: extractor returned ZERO rows -- do not trust any diff built on this'
    return out

def member_type(hdr_text, struct, member):
    """C type of `member` inside `struct { ... }` (typedef or tagged)."""
    m = (re.search(r'typedef\s+struct\s*\{(.*?)\}\s*%s\s*;' % re.escape(struct), hdr_text, re.S)
         or re.search(r'struct\s+%s\s*\{(.*?)\n\}\s*;' % re.escape(struct), hdr_text, re.S))
    if not m: return None
    leaf = member.split('.')[-1]
    mm = re.search(r'^\s*((?:const\s+)?[A-Za-z_]\w*)\s+\**%s\s*(?:\[[^\]]*\])?\s*;' % re.escape(leaf),
                   m.group(1), re.M)
    return mm.group(1) if mm else None

FTYPE_C = {'F_INT': {'int'}, 'F_FLOAT': {'float', 'vec_t'}, 'F_LSTRING': {'char'},
           'F_GSTRING': {'char'}, 'F_ZSTRING': {'char'}, 'F_VECTOR': {'vec3_t'},
           'F_ANGLEHACK': {'vec3_t'}, 'F_IGNORE': set(), 'F_EDICT': set(),
           'F_ITEM': set(), 'F_CLIENT': set()}
