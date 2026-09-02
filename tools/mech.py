"""Replay a *mechanical* q2pro commit (astyle reformat etc.) onto the variant tree.

For every file:
  - identical to baseq2's pre-commit version  -> take q2pro's post-commit version verbatim
  - otherwise                                 -> re-run the same transformation locally
Files that exist only in the variant get the transformation too, which a plain
diff-replay could never reach.
"""
import os, subprocess, sys

SP = os.path.dirname(os.path.abspath(__file__))
R  = os.path.join(SP, 'replay')
# R-TOOL-1: was the rescued harness's own binary at SP/opt/root/usr/bin/astyle,
# a path that does not exist on this machine.  The host's astyle is 3.1 -- the
# version R-CONV-1a pins -- and produces byte-identical output (R-BUILD-6), so
# the pin is satisfied natively.  $ASTYLE overrides; the Makefile exports it.
AS = os.environ.get('ASTYLE', 'astyle')
ASTYLE = [AS, '--style=linux', '--indent=spaces=4', '--pad-oper', '--unpad-paren',
          '--pad-header', '--suffix=none', '--quiet']

def git(*a, binary=False):
    r = subprocess.run(['git', '-C', R] + list(a), capture_output=True,
                       text=not binary)
    return r.stdout if r.returncode == 0 else None

def blob(rev, path):
    return git('show', f'{rev}:{path}')

def t_astyle(files):
    subprocess.run([sys.executable, os.path.join(SP, 'balance.py')] + files,
                   cwd=R, capture_output=True)
    r = subprocess.run(ASTYLE + ['./' + f for f in files], cwd=R,
                       capture_output=True, text=True)
    if r.returncode != 0: print('ASTYLE FAILED:', r.stderr[:300])
    rej = os.path.join(SP, 'style.rej')
    if os.path.exists(rej): os.remove(rej)
    subprocess.run(['patch', '-p1', '-N', '-F3', '--no-backup-if-mismatch', '-r', rej,
                    '-i', os.path.join(SP, 'style_residual.patch')],
                   cwd=R, capture_output=True, text=True)
    n = open(rej).read().count('@@ ') // 2 if os.path.exists(rej) else 0
    print(f'residual quirks: {"applied" if n == 0 else str(n) + " hunk(s) rejected"}')

def t_striptrail(files):
    for f in files:
        p = os.path.join(R, f)
        t = open(p, encoding='latin-1').read()
        open(p, 'w', encoding='latin-1', newline='').write(t.rstrip('\n') + '\n')

def t_none(files):
    pass

def t_wsfix(files):
    subprocess.run([sys.executable, os.path.join(SP, 'wsfix.py')] + files,
                   cwd=R, capture_output=True)

def t_floatsuffix(files):
    subprocess.run([sys.executable, os.path.join(SP, 'floatsuffix.py')] +
                   [f for f in files if f.endswith('.c')], cwd=R, capture_output=True)

def t_nextthink(files):
    subprocess.run([sys.executable, os.path.join(SP, 'nextthink.py')] + files,
                   cwd=R, capture_output=True)

def t_timers(files):
    subprocess.run([sys.executable, os.path.join(SP, 'timers.py')] +
                   os.environ['TIMER_MAP'].split() + ['--'] + files,
                   cwd=R, capture_output=True)

def t_stdbool(files):
    """q2pro's `Convert to stdbool.h.` -- a pure rename, applied tree-wide."""
    import re
    subs = [(r'\bqboolean\b', 'bool'), (r'\bqtrue\b', 'true'), (r'\bqfalse\b', 'false')]
    for f in files:
        p = os.path.join(R, f)
        t = open(p, encoding='latin-1').read()
        for pat, rep in subs:
            t = re.sub(pat, rep, t)
        open(p, 'w', encoding='latin-1', newline='').write(t)

def t_qrand(files):
    """q2pro's `Replace remaining calls to rand() with Q_rand().` -- pure rename.
    \b before `rand` keeps Q_rand()/srand() out of it."""
    import re
    for f in files:
        p = os.path.join(R, f)
        t = open(p, encoding='latin-1').read()
        t = re.sub(r'\brand\s*\(\s*\)', 'Q_rand()', t)
        open(p, 'w', encoding='latin-1', newline='').write(t)

def t_desig(files):
    """q2pro's `Convert itemlist[] to designated initializers.`"""
    import subprocess
    targets = [os.path.join(R, f) for f in files if f == 'g_items.c']
    if targets:
        r = subprocess.run([sys.executable, os.path.join(SP, 'desig.py')] + targets,
                           capture_output=True, text=True)
        print(r.stdout.strip() or r.stderr[-300:])

def t_precarray(files):
    """q2pro's `Convert precache strings into arrays.`"""
    import subprocess
    r = subprocess.run([sys.executable, os.path.join(SP, 'precarray.py')] +
                       [f for f in files if f == 'g_items.c'],
                       capture_output=True, text=True)
    print(r.stdout.strip() or r.stderr[-300:])

def t_maxclients(files):
    """q2pro's `Use game.maxclients instead of maxclients->value.`  InitGame's
    own assignment is the one site that has to keep reading the cvar."""
    import re
    KEEP = 'game.maxclients = maxclients->value;'
    for f in files:
        p = os.path.join(R, f)
        t = open(p, encoding='latin-1').read()
        t = t.replace(KEEP, '\x00KEEP\x00')
        t = re.sub(r'\bmaxclients->value\b', 'game.maxclients', t)
        t = t.replace('\x00KEEP\x00', KEEP)
        open(p, 'w', encoding='latin-1', newline='').write(t)

def t_qatoi(files):
    """q2pro's `Add and use Q_atoi().`  A drop-in with defined overflow
    behaviour, applied tree-wide the way the astyle pass was."""
    import re
    for f in files:
        p = os.path.join(R, f)
        t = open(p, encoding='latin-1').read()
        t = re.sub(r'(?<![\w.>])atoi\s*\(', 'Q_atoi(', t)
        open(p, 'w', encoding='latin-1', newline='').write(t)

TRANSFORMS = {'qatoi': t_qatoi, 'maxclients': t_maxclients, 'precarray': t_precarray, 'desig': t_desig, 'qrand': t_qrand, 'stdbool': t_stdbool, 'astyle': t_astyle, 'timers': t_timers, 'nextthink': t_nextthink, 'striptrail': t_striptrail, 'none': t_none,
              'wsfix': t_wsfix, 'floatsuffix': t_floatsuffix}

def main():
    sha = sys.argv[1]
    transform = TRANSFORMS[sys.argv[2] if len(sys.argv) > 2 else 'astyle']
    subprocess.run(['git', '-C', R, 'read-tree', '--reset', '-u', 'HEAD'], check=True)
    before = {f for f in git('ls-tree', '--name-only', f'{sha}^').split()}
    after  = {f for f in git('ls-tree', '--name-only', sha).split()}
    # vendored third-party sources stay in their upstream shape
    skip = set(os.environ.get('MECH_SKIP', '').split())
    ours   = [f for f in os.listdir(R) if f.endswith(('.c', '.h')) and f not in skip]
    verbatim, transformed, skipped = [], [], []
    for f in sorted(ours):
        cur = open(os.path.join(R, f), encoding='latin-1').read()
        b = blob(f'{sha}^', f) if f in before else None
        if b is not None and b == cur and f in after:
            open(os.path.join(R, f), 'w', encoding='latin-1', newline='').write(blob(sha, f))
            verbatim.append(f)
        else:
            transformed.append(f)
    if transformed:
        transform(transformed)
    print(f'verbatim from q2pro: {len(verbatim)}')
    print(f'locally transformed: {len(transformed)}  -> {" ".join(transformed)}')

main()
