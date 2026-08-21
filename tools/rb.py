"""Drive the q2pro->variant commit replay, reporting each conflict compactly."""
import os, subprocess, sys, re

SP = os.path.dirname(os.path.abspath(__file__))
R  = os.path.join(SP, 'replay')
env = dict(os.environ, GIT_EDITOR='true')

def git(*a, check=False):
    return subprocess.run(['git', '-C', R] + list(a), capture_output=True, text=True,
                          env=env, check=check)

def rebasing():
    return os.path.isdir(os.path.join(R, '.git/rebase-merge'))

def progress():
    d = os.path.join(R, '.git/rebase-merge')
    try:
        return (open(f'{d}/msgnum').read().strip(), open(f'{d}/end').read().strip())
    except OSError:
        return ('?', '?')

def conflicts():
    out = git('diff', '--name-only', '--diff-filter=U').stdout.split()
    return out

def show_conflicts(maxlines=120):
    total = 0
    for f in conflicts():
        p = os.path.join(R, f)
        try: lines = open(p, encoding='latin-1').read().split('\n')
        except OSError: print(f'  {f}: (deleted/unmerged)'); continue
        blocks, cur = [], None
        for i, l in enumerate(lines, 1):
            if l.startswith('<<<<<<<'): cur = [i]
            elif l.startswith('>>>>>>>') and cur: cur.append(i); blocks.append(tuple(cur)); cur = None
        print(f'  --- {f}: {len(blocks)} hunk(s), {len(lines)} lines')
        for (a, b) in blocks:
            n = b - a + 1; total += n
            print(f'      lines {a}-{b} ({n})')
    print(f'  total conflict lines: {total}')

def main():
    variant = sys.argv[1]
    if not rebasing():
        br = f'port_{variant}'
        git('checkout', '-q', '-B', br, 'baseq2', check=True)
        r = git('rebase', '--onto', f'v_{variant}', 'C0', br)
    else:
        # stage everything currently resolved, then continue
        unresolved = [f for f in conflicts()
                      if os.path.exists(os.path.join(R, f))
                      and '<<<<<<<' in open(os.path.join(R, f), encoding='latin-1').read()]
        if unresolved:
            print('STILL HAS MARKERS:', ' '.join(unresolved)); return 2
        git('add', '-A')
        r = git('rebase', '--continue')
    while True:
        if not rebasing():
            print(f'REBASE COMPLETE for {variant}')
            print(git('log', '--oneline', '-1').stdout.strip()); return 0
        if conflicts(): break
        git('add', '-A')
        r = git('rebase', '--continue')
        if r.returncode != 0 and not conflicts() and rebasing():
            print('rebase stopped, stderr:'); print(r.stderr[-2000:]); return 3
    n, tot = progress()
    subj = open(os.path.join(R, '.git/rebase-merge/message'), encoding='latin-1').read().split('\n')[0]
    sha  = open(os.path.join(R, '.git/rebase-merge/stopped-sha')).read().strip() if os.path.exists(os.path.join(R,'.git/rebase-merge/stopped-sha')) else '?'
    print(f'=== CONFLICT at {n}/{tot}: {subj}')
    show_conflicts()
    return 1

sys.exit(main())
