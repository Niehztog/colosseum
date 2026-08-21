"""Auto-resolve the conflict shapes that have an unambiguous reading.

  ours == base                  -> take q2pro's rewrite
  theirs == base                -> keep the variant's version
  ours = <added> + base         -> <added> + q2pro's rewrite      (splice tail)
  ours = base + <added>         -> q2pro's rewrite + <added>      (splice head)
  base empty, both inserted      -> keep both (de-duplicated)
Anything else is left conflicted for a human decision.
"""
import os, sys
SP = os.path.dirname(os.path.abspath(__file__))
R  = os.path.join(SP, 'replay')
sys.path.insert(0, SP)
src = open(os.path.join(SP, 'res.py')).read().split('f = sys.argv[1]')[0].replace(
    "os.path.dirname(os.path.abspath(__file__))", repr(SP))
exec(src)

def norm(lines):
    # comment-column drift is not a semantic difference
    return [' '.join(l.split()) for l in lines]

def _splice_tail(no, nb):
    """If ours ends with base's non-blank content, return the index in `ours` of
    the first line of that shared tail; otherwise None."""
    sb = [i for i, x in enumerate(nb) if x.strip()]
    so = [i for i, x in enumerate(no) if x.strip()]
    if not sb or len(so) <= len(sb):
        return None
    if [nb[i] for i in sb] != [no[i] for i in so[-len(sb):]]:
        return None
    return so[-len(sb)]

def auto(path):
    parts = parse(path)
    idx = [i for i, p in enumerate(parts) if isinstance(p, dict)]
    done = 0
    for k in idx:
        p = parts[k]; o, b, t = p['ours'], p['base'], p['theirs']
        no, nb, nt = norm(o), norm(b), norm(t)
        if no == nb:                                       res = t
        elif nt == nb:                                     res = o
        elif b and len(o) > len(b) and no[-len(b):] == nb:  res = o[:-len(b)] + t
        elif b and len(o) > len(b) and no[:len(b)] == nb:   res = t + o[len(b):]
        elif not b:
            # both sides inserted at the same spot: keep both, dropping any
            # line q2pro added that the variant already has
            # de-duplicate only substantive lines; punctuation like `{`/`}`
            # legitimately appears on both sides
            def trivial(x):
                return not x.strip() or x.strip() in ('{', '}', '};', '#endif', 'else', '} else {')
            seen = set(x.strip() for x in o if x.strip() and not trivial(x))
            res = o + [x for x in t if trivial(x) or x.strip() not in seen]
        elif b and o and t and _splice_tail(no, nb) is not None:
            # ours = <variant-added lines> + <base region>, but with blank-line
            # noise between them -- the shape every reconstruction annotation
            # above a function q2pro then edited produces.
            i = _splice_tail(no, nb)
            keep = o[:i]
            tail = t
            while keep and keep[-1].strip() and tail and not tail[0].strip():
                tail = tail[1:]
            res = keep + tail
        elif os.environ.get('AUTORES_GUTTED') and not o and b and len(t) <= len(b):
            # The variant deleted a whole region that q2pro only edited -- OSP
            # gutted the monster AI, the turret code and most of the HUD.
            # Guarded by len(theirs) <= len(base) so a q2pro *addition* inside
            # the region can never be swallowed silently, and every application
            # is logged for audit.
            res = o
            with open(os.path.join(SP, 'gutted.log'), 'a') as lg:
                lg.write(f'{os.path.basename(path)}: dropped {len(b)} base / '
                         f'{len(t)} theirs lines | {b[0].strip()[:70]!r}\n')
        else:                                          continue
        parts[k] = '\n'.join(res); done += 1
    if done:
        open(path, 'w', encoding='latin-1', newline='').write(render(parts))
    left = sum(1 for p in parse(path) if isinstance(p, dict))
    return done, left

def resolve_modify_delete():
    """CTF dropped baseq2's monster files; keep them dropped when q2pro edits them."""
    import subprocess
    out = subprocess.run(['git', '-C', R, 'status', '--porcelain'],
                         capture_output=True, text=True).stdout
    n = 0
    for line in out.split('\n'):
        if line[:2] in ('DU', 'UD'):
            # DU: the variant dropped this file and q2pro edited it -> stay dropped.
            # UD: q2pro deleted it -> delete here too.
            # git writes the merged text to disk either way, so never test for it.
            f = line[3:].strip()
            subprocess.run(['git', '-C', R, 'rm', '-q', '-f', '--', f], capture_output=True)
            n += 1
    if n: print(f'  kept {n} file(s) deleted (absent in this variant)')
    return n

if __name__ == '__main__':
    tot_d = tot_l = 0
    tot_d += resolve_modify_delete()
    for f in sorted(os.listdir(R)):
        if not f.endswith(('.c', '.h')): continue
        p = os.path.join(R, f)
        if '<<<<<<<' not in open(p, encoding='latin-1').read(): continue
        d, l = auto(p); tot_d += d; tot_l += l
        print(f'  {f}: auto-resolved {d}, left {l}')
    print(f'auto-resolved {tot_d} hunk(s); {tot_l} need a decision')
