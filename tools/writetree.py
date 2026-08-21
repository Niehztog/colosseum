"""Write a normalised, GPL-headered copy of an official id source tree."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qnorm import norm

SP   = os.path.dirname(os.path.abspath(__file__))
DROP = {'game.h', 'q_shared.c', 'q_shared.h', 'm_flash.c'}
GPL  = ''.join(open(os.path.join(SP, 'dl/x/quake2-3.21/game/g_ai.c'),
                    encoding='latin-1').read().replace('\r\n', '\n').splitlines(True)[:19])

def add_header(text):
    return text if 'GNU General Public License' in '\n'.join(text.split('\n')[:25]) else GPL + text

def write_tree(srcdir, dstdir):
    """Replace *.c/*.h in dstdir with the normalised contents of srcdir. Returns file list."""
    for f in os.listdir(dstdir):
        if f.endswith(('.c', '.h')): os.remove(os.path.join(dstdir, f))
    files, report = [], []
    for f in sorted(os.listdir(srcdir)):
        if not f.endswith(('.c', '.h')) or f in DROP: continue
        text = norm(open(os.path.join(srcdir, f), encoding='latin-1').read(), report, f)
        open(os.path.join(dstdir, f), 'w', encoding='latin-1', newline='').write(add_header(text))
        files.append(f)
    return files, report

if __name__ == '__main__':
    files, report = write_tree(sys.argv[1], sys.argv[2])
    print(f'{len(files)} files')
