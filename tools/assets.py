#!/usr/bin/env python3
"""Every literal model and sound path resolves to a file some donor ships (R-184).

WHY.  `gi.modelindex("...")` and `gi.soundindex("...")` register a configstring
and return an index.  Neither checks that the file exists, and neither can: the
server does not read the asset, only names it.  The failure surfaces on the
CLIENT, once, as a console line nobody is reading, and the effect is a weapon
with no sound or an effect with no model -- indistinguishable from a gameplay
choice.  This is `itemnames.py`'s question asked of the other half of the
namespace: R-183 checked names against the itemlist, and this checks them
against the data.

WHAT IS A FINDING.  A string literal ending `.md2`, `.sp2` or `.wav` that names
no file in any donor's pak.  Sound paths are resolved under `sound/`, which is
what the engine prepends; model paths are already rooted.

WHAT IS NOT, and each of these is a shape rather than a judgement:

  * `#w_railgun.md2` -- a leading `#` is a VWep token.  The client resolves it
    against the PLAYER's model directory, so there is no such file and there is
    not meant to be.
  * `*death1.wav` -- a leading `*` is a sexed sound, resolved as
    `sound/player/<model>/death1.wav`.  Same reasoning.
  * `%s.wav`, `*pain%i_%i.wav` -- printf formats, not paths.
  * `play world/10_0.wav` -- a stuffed CONSOLE COMMAND that happens to contain a
    path.  The path inside it is real; the literal is not one.
  * the twelve names in ABSENT below, which id's own code references and id's
    own data does not contain.

THE DATA IS NOT IN THE REPOSITORY, so this check SKIPS unless it is pointed at
it -- `--data DIR`, or $Q2DATA, or the default beside the tree.  A skip prints
why.  It is deliberately not a silent pass: audit.py distinguishes the two, and
"no data supplied" is a different statement from "every asset resolves".

USAGE
    tools/assets.py [--tree src] [--data ~/q2-dev/yquake2/release_]
    tools/assets.py --selftest
"""
import argparse
import glob
import os
import re
import struct
import sys
import zipfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_DATA = os.path.expanduser('~/q2-dev/yquake2/release_')
# The donors whose data Colosseum may legitimately name.  `gladiator` and the
# q3bot test dirs are deliberately NOT here: they are other projects' gamedirs,
# and resolving against them would let a missing asset pass because some
# unrelated mod happens to ship one with the same name.
DONORS = ('baseq2', 'xatrix', 'rogue', 'ctf', 'arena', 'tourney')

BLOCK_RE = re.compile(r'/\*.*?\*/', re.S)
LIT_RE = re.compile(r'"([^"\n]*?\.(?:md2|sp2|wav))"', re.I)

# Referenced by this tree, referenced by upstream q2pro, and shipped by nobody.
# Every one was checked against upstream before being exempted -- `grep -rl` in
# q2pro's own src/ -- so this list is "id's dangling references", not "ours we
# gave up on".  A name that stops appearing in the tree, or that a later pak
# turns out to contain, is reported: see check_absent().
ABSENT = {
    'misc/trigger1.wav':        'q2pro src/game/g_trigger.c has it too',
    'world/hum1.wav':           'q2pro src/ctf/g_ctf.c has it too',
    'weapons/grapple/grhurt.wav':
        'q2pro src/ctf/g_ctf.c + g_items.c have it; the CTF pak ships five '
        'grapple sounds and not this one',
    'weapons/flame.wav':        'q2pro src/rogue/g_newweap.c has it too',
    'weapons/incend.wav':       'ditto -- Ground Zero incendiary, unused code',
    'weapons/grenlb2b.wav':     'ditto; baseq2 ships grenlb1b.wav, not 2b',
    'weapons/meatht.wav':       'ditto',
    'weapons/swish.wav':        'ditto',
    'weapons/tink1.wav':        'ditto',
    'models/projectiles/incend/tris.md2':
        'ditto; the model rogue ships is models/proj/incend/tris.md2',
    'models/projectiles/puff/tris.md2':  'ditto; no models/proj/puff either',
    'models/items/spawngro3/tris.md2':
        'ditto; rogue ships spawngro and spawngro2, no spawngro3',
}


def strip_comments(src):
    def blank(m):
        return '\n' * m.group(0).count('\n')
    return re.sub(r'//[^\n]*', '', BLOCK_RE.sub(blank, src))


def pak_names(path):
    """The Quake II .pak directory: 56-byte name, then offset and length."""
    out = []
    try:
        with open(path, 'rb') as f:
            if f.read(4) != b'PACK':
                return out
            dirofs, dirlen = struct.unpack('<ii', f.read(8))
            f.seek(dirofs)
            for _ in range(dirlen // 64):
                rec = f.read(64)
                if len(rec) < 64:
                    break
                out.append(rec[:56].split(b'\0')[0].decode('latin-1')
                           .lower().replace('\\', '/'))
    except OSError:
        pass
    return out


def data_index(root, donors=DONORS):
    """Every file each donor ships, from its paks and from loose files."""
    names = set()
    found = []
    for d in donors:
        dd = os.path.join(root, d)
        if not os.path.isdir(dd):
            continue
        found.append(d)
        for p in sorted(glob.glob(os.path.join(dd, '*.pak'))):
            names |= set(pak_names(p))
        for p in sorted(glob.glob(os.path.join(dd, '*.pk3'))):
            try:
                names |= set(n.lower() for n in zipfile.ZipFile(p).namelist())
            except Exception:
                pass
        for r, _, fs in os.walk(dd):
            for fn in fs:
                if not fn.lower().endswith(('.pak', '.pk3')):
                    rel = os.path.relpath(os.path.join(r, fn), dd)
                    names.add(rel.lower().replace('\\', '/'))
    return names, found


def is_shape(lit):
    """A literal that is not a path, for a reason that is about its shape."""
    if lit.startswith(('#', '*')):
        return True             # VWep token, or a sexed sound
    if '%' in lit:
        return True             # a printf format
    if ' ' in lit.strip():
        return True             # a stuffed console command, e.g. `play x.wav`
    if lit.lstrip() in ('.wav', '.md2', '.sp2'):
        return True             # a bare extension being concatenated
    return False


def resolve(lit, names):
    p = lit.lower().replace('\\', '/')
    return ('sound/' + p) in names if p.endswith('.wav') else p in names


def literals(tree):
    out = {}
    for root, dirs, files in os.walk(tree):
        for fn in sorted(files):
            if not fn.endswith(('.c', '.h')):
                continue
            path = os.path.join(root, fn)
            code = strip_comments(open(path, errors='replace').read())
            for n, line in enumerate(code.split('\n'), 1):
                for m in LIT_RE.finditer(line):
                    out.setdefault(m.group(1), []).append(
                        (os.path.relpath(path, REPO), n))
    return out


def check_absent(seen, names, absent):
    """An exemption that is no longer needed is a finding of its own."""
    out = []
    for p in sorted(absent):
        if p not in seen:
            out.append('%s no longer appears in the tree' % p)
        elif resolve(p, names):
            out.append('%s is shipped after all' % p)
    return out


def scan(tree, names, absent=None):
    if absent is None:
        absent = ABSENT
    lits = literals(tree)
    hits = []
    for lit, sites in sorted(lits.items()):
        if is_shape(lit) or lit.lower() in absent:
            continue
        if not resolve(lit, names):
            hits.append((sites[0][0], sites[0][1], lit, len(sites)))
    return hits, check_absent({k.lower() for k in lits}, names, absent), len(lits)


SELFTEST_ABSENT = {'weapons/tink1.wav': 'id ships no such file'}
SELFTEST_NAMES = {
    'sound/weapons/real.wav',
    'models/proj/real/tris.md2',
}
SELFTEST_CLEAN = '''
void f(void) {
    gi.soundindex("weapons/real.wav");
    gi.modelindex("models/proj/real/tris.md2");
    gi.soundindex("weapons/tink1.wav");      /* exempt: id ships none */
    gi.modelindex("#w_railgun.md2");         /* VWep token */
    gi.soundindex("*death1.wav");            /* sexed sound */
    gi.soundindex(va("*pain%i_%i.wav", a, b));
    stuffcmd(ent, "play world/10_0.wav\\n");
    // gi.soundindex("weapons/prose.wav") in a comment is not code
}
'''
SELFTEST_MUTANTS = [
    ('a sound no donor ships', 'void g(void){ gi.soundindex("weapons/nope.wav"); }'),
    ('a model no donor ships',
     'void g(void){ gi.modelindex("models/proj/nope/tris.md2"); }'),
    ('the models/projectiles-for-models/proj shape',
     'void g(void){ gi.modelindex("models/projectiles/real/tris.md2"); }'),
    ('a real name with a wrong directory',
     'void g(void){ gi.soundindex("world/real.wav"); }'),
]


def selftest():
    import shutil
    import tempfile
    ok = True
    tmp = tempfile.mkdtemp(prefix='assets-selftest-')

    def want(cond, why):
        nonlocal ok
        print(('   [control] ' if cond else '!! selftest: ') + why)
        if not cond:
            ok = False

    try:
        open(os.path.join(tmp, 'use.c'), 'w').write(SELFTEST_CLEAN)
        hits, stale, n = scan(tmp, SELFTEST_NAMES, SELFTEST_ABSENT)
        want(not hits, 'VWep token, sexed sound, printf, stuffcmd, comment and '
                       'a live exemption: %s' % ('0 findings' if not hits else hits))
        want(not stale, 'a live exemption is not reported stale')

        for why, body in SELFTEST_MUTANTS:
            p = os.path.join(tmp, 'bad.c')
            open(p, 'w').write(body + '\n')
            hits, _, _ = scan(tmp, SELFTEST_NAMES, SELFTEST_ABSENT)
            os.remove(p)
            want(bool(hits), '%s -> %s' % (why, 'caught' if hits else 'MISSED'))

        # the two ways an exemption goes stale
        _, stale, _ = scan(tmp, SELFTEST_NAMES,
                           dict(SELFTEST_ABSENT, **{'weapons/gone.wav': 'x'}))
        want(bool(stale), 'an exemption for a name not in the tree -> reported')
        _, stale, _ = scan(tmp, SELFTEST_NAMES | {'sound/weapons/tink1.wav'},
                           SELFTEST_ABSENT)
        want(bool(stale), 'an exemption for a name that IS shipped -> reported')

        # and an empty data set must not pass everything
        hits, _, _ = scan(tmp, set(), SELFTEST_ABSENT)
        want(bool(hits), 'no data at all -> the real names do not resolve')
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tree', default=os.path.join(REPO, 'src'))
    ap.add_argument('--data', default=os.environ.get('Q2DATA', DEFAULT_DATA))
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    if a.selftest:
        return 0 if selftest() else 1

    names, found = data_index(a.data)
    if not names:
        print('assets: SKIP -- no donor game data under %s, so there is nothing '
              'to resolve against (pass --data or set $Q2DATA)' % a.data)
        return 0

    hits, stale, total = scan(os.path.abspath(a.tree), names)
    for path, line, lit, n in hits:
        print('!! %s:%d: "%s" names no file any donor ships%s (R-184)'
              % (path, line, lit, '' if n == 1 else ' (+%d more site(s))' % (n - 1)))
    for s in stale:
        print('!! tools/assets.py: stale exemption: %s (R-184)' % s)
    print('assets: %d unresolved of %d literal model/sound path(s); %d file(s) '
          'across %s; %d inherited absence(s) exempt'
          % (len(hits) + len(stale), total, len(names), '+'.join(found),
             len(ABSENT)))
    return 1 if (hits or stale) else 0


if __name__ == '__main__':
    sys.exit(main())
