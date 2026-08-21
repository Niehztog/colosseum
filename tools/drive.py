"""Run the replay, auto-resolving unambiguous conflicts, stopping only for real decisions."""
import subprocess, sys, os
SP = os.path.dirname(os.path.abspath(__file__))
variant = sys.argv[1]
auto_total = 0
last_out = ''
for _ in range(600):
    a = subprocess.run([sys.executable, os.path.join(SP, 'autores.py')],
                       capture_output=True, text=True).stdout
    last = a.strip().split('\n')[-1] if a.strip() else ''
    if last.startswith('auto-resolved'):
        auto_total += int(last.split()[1])
        if int(last.split(';')[1].split()[0]):
            print(last_out); print(a.rstrip()); print('--- needs a decision ---'); break
    r = subprocess.run([sys.executable, os.path.join(SP, 'rb.py'), variant],
                       capture_output=True, text=True)
    last_out = r.stdout.strip()
    if r.returncode == 0:
        print(last_out); print(f'(auto-resolved {auto_total} hunk(s) along the way)'); break
    if r.returncode != 1:
        print(last_out); print(r.stderr[-1500:]); break
else:
    print('iteration cap hit')
