import os, sys, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qnorm import norm

SP = os.path.dirname(os.path.abspath(__file__))
R  = os.path.join(SP, 'replay')
OSP = os.path.join(SP, 'osp')          # git archive of github/main
# q2pro supplies these.  game.h and m_player.h are byte-identical to vanilla;
# q_shared.h differs by one comment; q_shared.c differs only by reconstruction
# annotations, an #if 0 around RotatePointAroundVector, and three overflow
# guards that q2pro's shared.c already has properly.
DROP = {'game.h', 'q_shared.c', 'q_shared.h'}

# norm() rewrites the bare words true/false everywhere, including inside string
# literals.  Two literals in nglog.c must survive -- one of them lands in a
# char[6], which "qfalse" would overflow.
LITERAL_FIXUPS = {'nglog.c': [('"qfalse"', '"false"'), ('"qtrue"', '"true"')]}

def git(*a): return subprocess.run(['git', '-C', R] + list(a), check=True,
                                   capture_output=True, text=True).stdout

git('checkout', '-q', '-B', 'v_osp', 'C0')
c0 = {f for f in os.listdir(R) if f.endswith(('.c', '.h'))}
for f in c0:
    os.remove(os.path.join(R, f))

files, report = [], []
for f in sorted(os.listdir(OSP)):
    if not f.endswith(('.c', '.h')) or f in DROP: continue
    text = norm(open(os.path.join(OSP, f), encoding='latin-1').read(), report, f)
    for bad, good in LITERAL_FIXUPS.get(f, []):
        assert bad in text, f'{f}: expected {bad} to need restoring'
        text = text.replace(bad, good)
    open(os.path.join(R, f), 'w', encoding='latin-1', newline='').write(text)
    files.append(f)

added, removed = sorted(set(files) - c0), sorted(c0 - set(files))
git('add', '-A')
git('commit', '-q', '-m', 'OSP Tourney DM reconstruction (normalised)')
git('tag', '-f', 'V0_osp')
print(f'osp: {len(files)} files | osp-only {len(added)} | baseq2 files absent {len(removed)}')
print(f'  osp-only: {" ".join(added)}')
print(f'  absent:   {" ".join(removed)}')
if report:
    print(f'  normaliser notes ({len(report)}):')
    for r in report[:20]: print('   ', r)
