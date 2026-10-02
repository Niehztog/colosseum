#!/usr/bin/env python3
"""botduel.py -- Gladiator's bots against the Quake III botlib's, measured.

Plays seeded, frame-counted bot matches between two SIDES -- by default every
Gladiator bot on one and every Quake III bot on the other -- over every ruleset
that takes bots, every content layer and every stock map, and turns what
`sv botstats` (SPECS.md R-BOT-36) reports into statistics per map, per ruleset
and per pack.

    tools/botduel.py meshes                 # make and warm every AAS file (once)
    tools/botduel.py run -o /tmp/duel       # the matrix; resumable
    tools/botduel.py report -o /tmp/duel    # /tmp/duel/report.md and matches.csv
    tools/botduel.py control -o /tmp/duelc  # the controls (see below)

FRAME-BASED.  The server runs with `fixedtime 200`: every pass through q2pro's
main loop advances the game by exactly one 100 ms frame and sleeps for none, so
a match runs as fast as the host can compute it (a ten-minute match is seconds
here) and lasts exactly the frames it was asked for.  Every command reaches the
server through `wait <frames>` lines on stdin, which q2pro counts in frames, so
nothing in a match depends on wall-clock time -- which is also why a match can
share the host with three others and a build without changing its result.

SEEDED.  The engine, the game and Gladiator's botlib each seed their random
number generator from `time()`; the Quake III botlib draws on the C library's,
which Gladiator's seeding also sets.  A one-function LD_PRELOAD shim answers
`time()` with the match's seed, so a seed decides the whole match: the same
seed replays it byte for byte, and `control` checks that it does.  The seed's
parity decides which side joins first -- the first joiner is red under ctf and
team 0 elsewhere -- so map asymmetry is counterbalanced across seeds rather
than handed to one botlib; the seed and the match's ruleset, pack and map
draw each side's characters.

A SIDE is a botlib and, for the Quake III one, a skill: `gladiator`, `q3:4`.
Gladiator's characters carry their skill inside the character file, so its side
has none.  Teams are made by join order -- alternate joins land on alternate
teams under tdm, duel and arena's pickup teams, and `botctfteam` is set per bot
under ctf -- and every match checks afterwards that each team is one side.

MAPS.  Each pack is played on its own stock deathmatch maps with its content
layer on: baseq2 on q2dm1-8, The Reckoning (`xatrix 1`) on xdm1-7, Ground Zero
(`rogue 1`) on rdm1-14; ctf is played on q2ctf1-5 under each of the three.
arena runs a deathmatch map as RA2 does, one arena with two pickup teams.

THE MESHES.  Neither botlib can play a map without its own AAS file.  The
Quake III ones are made by the bspc built beside the library, in seconds.
Gladiator's q2dm/q2ctf meshes come from the sibling gladiator-bot-restored
checkout's HEAD; the pack maps' are made by Gladiator's own bspc (v1.4) -- under
qemu-i386 on a host that is not x86 -- and every Gladiator mesh is then WARMED:
loaded once by the botlib, which computes its reachability and writes the file
back.  An unwarmed mesh would be recomputed at the start of every match, on a
wall-clock budget, which is the one thing that could make a seed not decide a
match.  All of it is cached under ~/.cache/colosseum-botduel and none of it is
written into either checkout.

THE CONTROLS (`control`), because a measurement that has never been shown to
fail is not trusted (R-VER-9):
  determinism   one seed twice gives identical kill streams and scores, and a
                second seed gives a different one
  null          a side against itself -- gladiator v gladiator, q3:4 v q3:4 --
                must show NO difference: this is what proves that join order,
                team colour and the measure itself do not invent one
  sensitivity   q3:1 against q3:5 MUST show one: a measure that cannot see five
                skill levels cannot see a botlib either

ENV, all defaulted: Q2PRO_BUILD (../q2pro/builddir-native), Q2DATA
(/usr/share/games/quake2, holding baseq2/ xatrix/ rogue/ ctf/), LIB
(release/game<cpu>.so; gladiator.so, q3bot.so and q3bot/bspc beside it),
GLADMESHES (../gladiator-bot-restored), BOTDUEL_CACHE.
"""

import argparse
import concurrent.futures
import csv
import gzip
import hashlib
import json
import math
import os
import platform
import random
import re
import shutil
import statistics
import struct
import subprocess
import sys
import tempfile
import threading
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ------------------------------------------------------------------ the matrix

PACKS = {
    'baseq2': {'xatrix': 0, 'rogue': 0, 'maps': ['q2dm%d' % i for i in range(1, 9)]},
    'xatrix': {'xatrix': 1, 'rogue': 0, 'maps': ['xdm%d' % i for i in range(1, 8)]},
    'rogue':  {'xatrix': 0, 'rogue': 1, 'maps': ['rdm%d' % i for i in range(1, 15)]},
}
CTF_MAPS = ['q2ctf%d' % i for i in range(1, 6)]
RULESETS = ['dm', 'dmpro', 'tdm', 'duel', 'ctf', 'arena']
OSP = {'dmpro', 'tdm', 'duel'}           # a ready-up, a countdown, then the match
TEAMS = {'tdm', 'duel', 'ctf', 'arena'}  # where a side must be one team

# Frames between the last bot joining and the window opening.  The OSP three
# ready up and count down first -- "Match has started!" lands near frame 400
# with four bots -- and the window must not open before it.
WARMUP = {'dm': 200, 'ctf': 200, 'arena': 200, 'dmpro': 700, 'tdm': 700, 'duel': 700}


def maps_for(ruleset, pack):
    return CTF_MAPS if ruleset == 'ctf' else PACKS[pack]['maps']


# ------------------------------------------------------------------ paths

def cpu():
    m = platform.machine()
    return {'aarch64': 'arm64', 'x86_64': 'x86_64'}.get(m, 'i386' if re.match(r'i.86', m) else m)


class Paths:
    def __init__(self):
        e = os.environ.get
        self.q2pro = os.path.abspath(e('Q2PRO_BUILD', os.path.join(ROOT, '..', 'q2pro', 'builddir-native')))
        self.q2proded = os.path.join(self.q2pro, 'q2proded')
        self.data = e('Q2DATA', '/usr/share/games/quake2')
        self.lib = os.path.abspath(e('LIB', os.path.join(ROOT, 'release', 'game%s.so' % cpu())))
        here = os.path.dirname(self.lib)
        self.gladlib = os.path.join(here, 'gladiator.so')
        self.q3lib = os.path.join(here, 'q3bot.so')
        self.q3bspc = os.path.join(here, 'q3bot', 'bspc')
        self.glad = os.path.join(ROOT, 'vendor', 'gladiator-bot-restored')
        self.gladbspc = os.path.join(self.glad, 'tools', 'vendor', 'bspc', 'bspci386')
        self.q3 = os.path.join(ROOT, 'vendor', 'q3a_bot_backport_for_q2')
        self.meshrepo = os.path.abspath(e('GLADMESHES', os.path.join(ROOT, '..', 'gladiator-bot-restored')))
        self.cache = os.path.abspath(e('BOTDUEL_CACHE', os.path.expanduser('~/.cache/colosseum-botduel')))

    def mesh(self, lib, m):
        return os.path.join(self.cache, 'aas', lib, m + '.aas')

    def check(self):
        need = [self.q2proded, self.lib, self.gladlib, self.q3lib,
                os.path.join(self.glad, 'assets', 'pak7.pak'),
                os.path.join(self.glad, 'assets', 'bots.cfg'),
                os.path.join(self.q3, 'assets', 'botfiles', 'bots.cfg'),
                os.path.join(self.data, 'baseq2', 'pak0.pak')]
        for p in need:
            if not os.path.exists(p):
                sys.exit('botduel: missing %s' % p)

    def pak_for(self, m):
        if m.startswith('xdm'):
            return os.path.join(self.data, 'xatrix', 'pak0.pak')
        if m.startswith('rdm'):
            return os.path.join(self.data, 'rogue', 'pak0.pak')
        if m.startswith('q2ctf'):
            return os.path.join(self.data, 'ctf', 'pak0.pak')
        return os.path.join(self.data, 'baseq2', 'pak1.pak')


def sha1(path):
    h = hashlib.sha1()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()[:12]


def pak_extract(pak, name, dst):
    with open(pak, 'rb') as f:
        magic, off, size = struct.unpack('<4sii', f.read(12))
        f.seek(off)
        table = f.read(size)
        for i in range(size // 64):
            n, o, ln = struct.unpack('<56sii', table[i * 64:i * 64 + 64])
            if n.split(b'\0')[0].decode('latin1').lower() == name.lower():
                f.seek(o)
                with open(dst, 'wb') as g:
                    g.write(f.read(ln))
                return True
    return False


# ------------------------------------------------------------------ the seed

SHIM = r'''
/* botduel's seed: time() answers BOTDUEL_SEED, so every generator seeded
 * from the clock -- the engine's, the game's, Gladiator's -- is seeded from
 * the match.  Generated by tools/botduel.py; GPL-2.0-or-later. */
#include <stdlib.h>
#include <time.h>
time_t time(time_t *t)
{
    const char *s = getenv("BOTDUEL_SEED");
    time_t v = s ? (time_t)strtoll(s, NULL, 10) : 0;
    if (t)
        *t = v;
    return v;
}
'''


def shim(paths):
    out = os.path.join(paths.cache, 'bin', 'seedtime.so')
    src = out[:-3] + '.c'
    os.makedirs(os.path.dirname(out), exist_ok=True)
    if not (os.path.exists(out) and os.path.exists(src) and open(src).read() == SHIM):
        with open(src, 'w') as f:
            f.write(SHIM)
        subprocess.check_call([os.environ.get('CC', 'cc'), '-O2', '-shared', '-fPIC', '-o', out, src])
    return out


# ------------------------------------------------------------------ fixtures

def roster(path):
    """Names out of a bots.cfg: `"sv" "addbot" "<name>" ...` per line."""
    names = []
    for line in open(path, encoding='latin1'):
        m = re.match(r'\s*"sv"\s+"addbot"\s+"([^"]+)"', line)
        if m:
            names.append(m.group(1))
    return names


def make_fixture(paths, d):
    """One worker's game tree: README's layout -- xatrix pak0 as pak0, rogue's
    as pak1, ctf's as pak2, Gladiator's pak7 -- with both botlibs."""
    shutil.rmtree(d, ignore_errors=True)
    g = os.path.join(d, 'colosseum')
    for sub in ('baseq2', 'colosseum/maps', 'colosseum/botcfg', 'colosseum/q3bot/maps'):
        os.makedirs(os.path.join(d, sub))
    base = os.path.join(paths.data, 'baseq2')
    for p in sorted(os.listdir(base)):
        if re.match(r'pak\d+\.pak$', p, re.I):
            os.symlink(os.path.join(base, p), os.path.join(d, 'baseq2', p))
    for n, pack in ((0, 'xatrix'), (1, 'rogue'), (2, 'ctf')):
        os.symlink(os.path.join(paths.data, pack, 'pak0.pak'), os.path.join(g, 'pak%d.pak' % n))
    os.symlink(os.path.join(paths.glad, 'assets', 'pak7.pak'), os.path.join(g, 'pak7.pak'))
    shutil.copy(os.path.join(paths.glad, 'assets', 'bots.cfg'), os.path.join(g, 'botcfg', 'bots.cfg'))
    os.symlink(paths.lib, os.path.join(g, 'game%s.so' % cpu()))
    os.symlink(paths.gladlib, os.path.join(g, 'gladiator.so'))
    shutil.copy(paths.q3lib, os.path.join(g, 'q3bot.so'))
    shutil.copytree(os.path.join(paths.q3, 'assets', 'botfiles'), os.path.join(g, 'q3bot', 'botfiles'))
    shutil.copy(os.path.join(paths.q3, 'assets', 'botfiles', 'bots.cfg'), os.path.join(g, 'q3bot', 'bots.cfg'))


def place_meshes(paths, d, m):
    """COPIED, never linked: the botlib writes a mesh back (and through a link
    would write the cache's)."""
    g = os.path.join(d, 'colosseum')
    for sub in ('maps', 'q3bot/maps'):
        for f in os.listdir(os.path.join(g, sub)):
            os.unlink(os.path.join(g, sub, f))
    shutil.copy(paths.mesh('gladiator', m), os.path.join(g, 'maps', m + '.aas'))
    shutil.copy(paths.mesh('q3', m), os.path.join(g, 'q3bot', 'maps', m + '.aas'))


def server_argv(paths, d, ruleset, m, xatrix, rogue, extra=()):
    return [paths.q2proded,
            '+set', 'basedir', d, '+set', 'homedir', d, '+set', 'game', 'colosseum',
            '+set', 'dedicated', '1', '+set', 'net_port', '0',
            '+set', 'g_ruleset', ruleset, '+set', 'deathmatch', '1',
            '+set', 'xatrix', str(xatrix), '+set', 'rogue', str(rogue),
            '+set', 'maxclients', '16', '+set', 'minimumplayers', '0', '+set', 'bots_minplayers', '0',
            '+set', 'botlibs', 'q3 gladiator',
            '+set', 'cheats', '1', '+set', 'fixedtime', '200',
            '+set', 'timelimit', '0', '+set', 'fraglimit', '0', '+set', 'capturelimit', '0',
            '+set', 'flood_msgs', '0'] + list(extra) + ['+map', m]


def waits(frames):
    out = []
    while frames > 0:
        out.append('wait %d' % min(frames, 1000))
        frames -= 1000
    return out


# ------------------------------------------------------------------ meshes

def warm_glad(paths, src, m, xatrix, rogue):
    """Load `src` once with a Gladiator bot until the botlib has written it
    back, then load the result again and require that nothing is recomputed."""
    d = tempfile.mkdtemp(prefix='botduel-warm-')
    try:
        make_fixture(paths, d)
        dst = os.path.join(d, 'colosseum', 'maps', m + '.aas')
        for attempt in ('warm', 'verify'):
            shutil.copy(src, dst)
            log = os.path.join(d, attempt + '.log')
            script = ['wait 20', 'sv addbot "Adrenaline Hunk"'] + waits(3000) + ['quit']
            with open(log, 'w') as out:
                subprocess.run(server_argv(paths, d, 'dm', m, xatrix, rogue), cwd=d,
                               input='\n'.join(script) + '\n', text=True,
                               stdout=out, stderr=subprocess.STDOUT, timeout=1800)
            text = open(log, errors='replace').read()
            if 'entered the game' not in text:
                raise RuntimeError('%s: no Gladiator bot entered during the %s load, see %s' % (m, attempt, log))
            recomputed = 'calculating reachability' in text
            if attempt == 'warm':
                src = os.path.join(d, m + '.warm.aas')
                shutil.copy(dst, src)
            elif recomputed:
                raise RuntimeError('%s: the warmed mesh was recomputed again on load' % m)
        os.makedirs(os.path.dirname(paths.mesh('gladiator', m)), exist_ok=True)
        shutil.copy(src, paths.mesh('gladiator', m))
    finally:
        shutil.rmtree(d, ignore_errors=True)


def cmd_meshes(args, paths):
    paths.check()
    allmaps = []
    for pack, p in PACKS.items():
        allmaps += [(m, p['xatrix'], p['rogue']) for m in p['maps']]
    allmaps += [(m, 0, 0) for m in CTF_MAPS]
    if args.maps:
        allmaps = [t for t in allmaps if t[0] in args.maps]
    work = tempfile.mkdtemp(prefix='botduel-mesh-')
    bspc = os.path.join(paths.cache, 'bin', 'bspci386')
    os.makedirs(os.path.dirname(bspc), exist_ok=True)
    # The submodule carries Gladiator's bspc without its exec bit, and qemu
    # refuses to run it then -- silently, with status 1.
    shutil.copy(paths.gladbspc, bspc)
    os.chmod(bspc, 0o755)
    runner = [] if cpu() in ('i386', 'x86_64') else ['qemu-i386', '-L', '/usr/i686-linux-gnu']

    def one(t):
        m, xatrix, rogue = t
        msgs = []
        bsp = os.path.join(work, m + '.bsp')
        # Quake III: bspc does geometry, reachability and clusters in one run.
        if not os.path.exists(paths.mesh('q3', m)):
            od = os.path.join(work, 'q3-' + m)
            os.makedirs(od, exist_ok=True)
            pak_extract(paths.pak_for(m), 'maps/%s.bsp' % m, os.path.join(od, m + '.bsp'))
            subprocess.run([paths.q3bspc, '-bsp2aas', m + '.bsp', '-output', './'], cwd=od,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            made = os.path.join(od, m + '.aas')
            if not os.path.exists(made):
                return '%s: the Quake III bspc made no mesh' % m
            os.makedirs(os.path.dirname(paths.mesh('q3', m)), exist_ok=True)
            shutil.copy(made, paths.mesh('q3', m))
            msgs.append('q3 made')
        # Gladiator: HEAD of the mesh checkout where it has one, else its bspc.
        if not os.path.exists(paths.mesh('gladiator', m)):
            src = os.path.join(work, m + '.glad.aas')
            got = subprocess.run(['git', '-C', paths.meshrepo, 'show', 'HEAD:assets/maps/%s.aas' % m],
                                 stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
            if got.returncode == 0 and got.stdout:
                open(src, 'wb').write(got.stdout)
                msgs.append('gladiator from %s HEAD' % os.path.basename(paths.meshrepo))
            else:
                geom = os.path.join(paths.cache, 'glad-geom', m + '.d', m + '.aas')
                if not os.path.exists(geom):
                    gd = os.path.dirname(geom)
                    os.makedirs(gd, exist_ok=True)
                    pak_extract(paths.pak_for(m), 'maps/%s.bsp' % m, os.path.join(gd, m + '.bsp'))
                    with open(os.path.join(gd, m + '.log'), 'w') as out:
                        subprocess.run(runner + [bspc, '-bsp2aas', m + '.bsp', '-output', './'],
                                       cwd=gd, stdout=out, stderr=subprocess.STDOUT)
                    os.unlink(os.path.join(gd, m + '.bsp'))
                    if not os.path.exists(geom):
                        return '%s: Gladiator\'s bspc made no mesh, see %s' % (m, gd)
                shutil.copy(geom, src)
                msgs.append('gladiator geometry by bspc')
            try:
                warm_glad(paths, src, m, xatrix, rogue)
            except Exception as e:
                return str(e)
            msgs.append('warmed')
        return '%s: %s' % (m, ', '.join(msgs) or 'cached')

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:
        for r in ex.map(one, allmaps):
            print(r, flush=True)
    shutil.rmtree(work, ignore_errors=True)


# ------------------------------------------------------------------ one match

class Side:
    def __init__(self, spec):
        lib, _, skill = spec.partition(':')
        if lib not in ('gladiator', 'q3'):
            sys.exit('botduel: a side is gladiator or q3[:skill], not %r' % spec)
        if lib == 'gladiator' and skill:
            sys.exit('botduel: a Gladiator bot\'s skill is its character\'s')
        self.lib = lib
        self.skill = int(skill or 4) if lib == 'q3' else None
        self.spec = spec if lib == 'gladiator' else 'q3:%d' % self.skill

    def __str__(self):
        return self.spec


def match_id(spec):
    return '%s-%s-%s-%s-v-%s-s%d' % (spec['ruleset'], spec['pack'], spec['map'],
                                    spec['a'].replace(':', ''), spec['b'].replace(':', ''), spec['seed'])


def lineup(paths, a, b, spec, per_side):
    """Each side's characters, drawn afresh for every match from the match's
    own identity, and never one character twice in a match.  Per match rather
    than per seed: a lineup shared by every match of a seed would make those
    matches one observation of that lineup, and the tests below count them as
    independent."""
    rng = random.Random('botduel-%s-%s-%s-%d' % (spec['ruleset'], spec['pack'], spec['map'], spec['seed']))
    pool = {'gladiator': roster(os.path.join(paths.glad, 'assets', 'bots.cfg')),
            'q3': roster(os.path.join(paths.q3, 'assets', 'botfiles', 'bots.cfg'))}
    used = set()
    out = []
    for side in (a, b):
        names = [n for n in pool[side.lib] if n not in used]
        pick = rng.sample(names, per_side)
        used.update(pick)
        out.append(pick)
    return out


def play(paths, so, wdir, spec, keep_logs):
    a, b = Side(spec['a']), Side(spec['b'])
    rs, m, seed = spec['ruleset'], spec['map'], spec['seed']
    per_side = 1 if rs == 'duel' else spec['per_side']
    names = lineup(paths, a, b, spec, per_side)
    sides = [(a, names[0]), (b, names[1])]
    # Join order: alternate bots, and which side goes first alternates with
    # the seed's parity.  The first joiner is red / team 0.
    first = seed % 2
    order = sides if first == 0 else sides[::-1]
    script = ['wait 20']
    for i in range(per_side):
        for k, (side, ns) in enumerate(order):
            if rs == 'ctf':
                script.append('set botctfteam %d' % (k + 1))
            if side.lib == 'q3':
                script.append('sv addbot "%s" %d' % (ns[i], side.skill))
            else:
                script.append('sv addbot "%s"' % ns[i])
            script.append('wait 5')
    script += waits(WARMUP[rs]) + ['sv botstats reset'] + waits(spec['frames'])
    script += ['sv botstats', 'sv clientdump', 'sv ruleset', 'quit']

    place_meshes(paths, wdir, m)
    p = PACKS[spec['pack']]
    env = dict(os.environ, BOTDUEL_SEED=str(seed), LD_PRELOAD=so)
    t0 = time.time()
    try:
        r = subprocess.run(server_argv(paths, wdir, rs, m, p['xatrix'], p['rogue']), cwd=wdir,
                           input='\n'.join(script) + '\n', text=True, env=env,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           errors='replace', timeout=spec['frames'] / 20 + 600)
        text, rc = r.stdout, r.returncode
    except subprocess.TimeoutExpired as e:
        text = e.stdout.decode('latin1') if isinstance(e.stdout, bytes) else (e.stdout or '')
        rc = 'timeout'
    res = parse(text, spec, sides, first)
    res['rc'] = rc
    res['wall'] = round(time.time() - t0, 1)
    if rc != 0:
        res['problems'].append('server exited %s' % rc)
    res['valid'] = not res['problems']
    if keep_logs:
        with gzip.open(os.path.join(keep_logs, match_id(spec) + '.log.gz'), 'wt') as f:
            f.write(text)
    return res


RE_KILL = re.compile(r'^botstats kill (\d+) (-?\d+) (\d+) (\d+) (\w+)$')
RE_CLIENT = re.compile(r'^botstats client (\d+) lib (\S+) skill (\S+) team (-?\d+) score (-?\d+) (.*) name (.*)$')
RE_FRAME = re.compile(r'^botstats frame (\d+) since (\d+) frames (\d+) ruleset (\S+) (\w+)$')
# arena's round end: `<arena>: <team> <name> has won the round!`, or `...the
# match!!` for the round that decides a match, which is a round won as well.
RE_ROUND = re.compile(r'^(\d+): (\d+) (.*) has won the (round!|match!!)$')
RE_CAPTURE = re.compile(r'^(.*) captured the (\w+) flag!$')
RE_FLAGGOT = re.compile(r'^(.*) got the (\w+) flag!$')
RE_RETURN = re.compile(r'^(.*) returned the (\w+) flag!$')
# Anything that ends or interrupts a match inside the window.
RE_ENDED = re.compile(r'^(SpawnServer|Timelimit hit|Fraglimit hit|Capturelimit hit|.*remaining in match|'
                      r'.* won over the .* team|Match (halted|terminated|has been forced to terminate)|'
                      r'Match mode has been terminated)')


def parse(text, spec, sides, first):
    """Everything after `botstats reset` -- the window -- and the checks that
    say whether the window measured what it was meant to."""
    res = {'spec': spec, 'problems': [], 'kills': [], 'rounds': [], 'captures': [],
           'flaggrabs': [], 'returns': [], 'clients': [], 'first': sides[first][0].spec}
    lines = text.splitlines()
    if sum(1 for l in lines if l.startswith('SpawnServer:')) != 1:
        res['problems'].append('the map loaded %d times' % sum(1 for l in lines if l.startswith('SpawnServer:')))
    try:
        start = next(i for i, l in enumerate(lines) if l.startswith('botstats reset '))
    except StopIteration:
        res['problems'].append('the window never opened')
        return res
    if spec['ruleset'] in OSP and not any(l.startswith('Match has started') for l in lines[:start]):
        res['problems'].append('the match had not started when the window opened')
    report = None
    for i, l in enumerate(lines[start:]):
        m = RE_KILL.match(l)
        if m:
            res['kills'].append([int(x) for x in m.groups()[:4]] + [m.group(5)])
            continue
        m = RE_ROUND.match(l)
        if m:
            res['rounds'].append(int(m.group(2)))
            continue
        m = RE_CAPTURE.match(l)
        if m:
            res['captures'].append(m.group(1))
            continue
        m = RE_FLAGGOT.match(l)
        if m:
            res['flaggrabs'].append(m.group(1))
            continue
        m = RE_RETURN.match(l)
        if m:
            res['returns'].append(m.group(1))
            continue
        m = RE_FRAME.match(l)
        if m:
            report = start + i
            res['frames'] = int(m.group(3))
        if RE_ENDED.match(l):
            res['problems'].append('the window saw: %s' % l.strip())
    if report is None:
        res['problems'].append('no botstats report')
        return res
    if res.get('frames') != spec['frames']:
        res['problems'].append('the window was %s frames, not %d' % (res.get('frames'), spec['frames']))
    for l in lines[report + 1:]:
        m = RE_CLIENT.match(l)
        if not m:
            if res['clients']:
                break
            continue
        slot, lib, skill, team, score, rest, name = m.groups()
        c = {'slot': int(slot), 'lib': lib, 'skill': skill, 'team': int(team), 'score': int(score),
             'name': name}
        kv = rest.split()
        c.update({kv[i]: int(kv[i + 1]) for i in range(0, len(kv), 2)})
        res['clients'].append(c)
    # Which side each client is on, by name: the lineup is the match's own.
    per = {}
    for k, (side, ns) in enumerate(sides):
        for n in ns:
            per[n] = k
    for c in res['clients']:
        c['side'] = per.get(c['name'])
        if c['side'] is None:
            res['problems'].append('a client nobody added: %s' % c['name'])
        elif c['lib'] != sides[c['side']][0].lib:
            res['problems'].append('%s is driven by %s, not %s' % (c['name'], c['lib'], sides[c['side']][0].lib))
    want = sum(len(ns) for _, ns in sides)
    if len([c for c in res['clients'] if c['side'] is not None]) != want:
        res['problems'].append('%d of %d bots in the game' % (len(res['clients']), want))
    if spec['ruleset'] in TEAMS and res['clients']:
        teams = [{c['team'] for c in res['clients'] if c['side'] == k} for k in (0, 1)]
        if any(len(t) != 1 or -1 in t for t in teams) or teams[0] == teams[1]:
            res['problems'].append('teams are not one side each: %s' % teams)
        else:
            res['team'] = [teams[0].pop(), teams[1].pop()]
    return res


# ------------------------------------------------------------------ the run

def plan(args):
    specs = []
    for seed in args.seeds:
        for rs in args.rulesets:
            for pack in args.packs:
                for m in maps_for(rs, pack):
                    if args.maps and m not in args.maps:
                        continue
                    specs.append({'ruleset': rs, 'pack': pack, 'map': m, 'seed': seed,
                                  'a': str(Side(args.a)), 'b': str(Side(args.b)),
                                  'frames': args.frames, 'per_side': args.per_side})
    return specs


def cmd_run(args, paths):
    paths.check()
    so = shim(paths)
    out = os.path.abspath(args.out)
    mdir = os.path.join(out, 'matches')
    ldir = os.path.join(out, 'logs')
    os.makedirs(mdir, exist_ok=True)
    os.makedirs(ldir, exist_ok=True)
    specs = plan(args)
    # A map one botlib has no mesh for cannot host this comparison; it is
    # left out, said here, and listed in the report.
    missing = sorted({s['map'] for s in specs
                      if not (os.path.exists(paths.mesh('gladiator', s['map'])) and
                              os.path.exists(paths.mesh('q3', s['map'])))})
    if missing:
        print('botduel: no mesh for %s -- not played; `%s meshes` makes them' % (' '.join(missing), sys.argv[0]))
        specs = [s for s in specs if s['map'] not in missing]
    meta = {'lib': paths.lib, 'lib_sha1': sha1(paths.lib), 'gladiator_sha1': sha1(paths.gladlib),
            'q3bot_sha1': sha1(paths.q3lib), 'q2proded_sha1': sha1(paths.q2proded),
            'host': platform.node(), 'started': time.strftime('%Y-%m-%d %H:%M:%S'),
            'argv': sys.argv, 'no_mesh': missing}
    with open(os.path.join(out, 'meta-%d.json' % int(time.time())), 'w') as f:
        json.dump(meta, f, indent=1)
    todo = [s for s in specs if not os.path.exists(os.path.join(mdir, match_id(s) + '.json'))]
    print('%d match(es) planned, %d already played, %d to play on %d worker(s)'
          % (len(specs), len(specs) - len(todo), len(todo), args.jobs), flush=True)
    pool = [os.path.join(tempfile.gettempdir(), 'botduel-w%d-%d' % (os.getpid(), i)) for i in range(args.jobs)]
    free = list(pool)
    lock = threading.Lock()
    for d in pool:
        make_fixture(paths, d)
    done = [0]
    t0 = time.time()

    def one(spec):
        with lock:
            d = free.pop()
        try:
            res = play(paths, so, d, spec, ldir)
        finally:
            with lock:
                free.append(d)
        with open(os.path.join(mdir, match_id(spec) + '.json'), 'w') as f:
            json.dump(res, f)
        with lock:
            done[0] += 1
            el = time.time() - t0
            eta = el / done[0] * (len(todo) - done[0])
            print('[%d/%d %4.0fs eta %4.0fs] %-44s %5.1fs %s' % (
                done[0], len(todo), el, eta, match_id(spec), res['wall'],
                'ok' if res['valid'] else 'INVALID: ' + '; '.join(res['problems'])), flush=True)
        return res

    try:
        with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:
            list(ex.map(one, todo))
    finally:
        for d in pool:
            shutil.rmtree(d, ignore_errors=True)


# ------------------------------------------------------------------ statistics

def mods():
    """MOD_* numbers to names, out of the game's own header."""
    out = {}
    for line in open(os.path.join(ROOT, 'src', 'g_local.h'), encoding='latin1'):
        m = re.match(r'#define\s+MOD_(\w+)\s+(\d+)\b', line)
        if m:
            out[int(m.group(2))] = m.group(1).lower()
    return out


def metrics(res):
    """Per-match, per-side numbers.  Side 0 is A, side 1 is B.  Rates are per
    ten minutes of game time (6000 frames)."""
    spec = res['spec']
    scale = 6000.0 / spec['frames']
    slot_side = {c['slot']: c['side'] for c in res['clients']}
    name_side = {c['name']: c['side'] for c in res['clients']}
    out = {}
    for k in (0, 1):
        cs = [c for c in res['clients'] if c['side'] == k]
        s = lambda key: sum(c.get(key, 0) for c in cs)
        out[k] = {
            'score': s('score'), 'kills': s('kills'), 'deaths': s('deaths'), 'suicides': s('suicides'),
            'teamkills': s('teamkills'), 'dmggiven': s('dmggiven'), 'dmgtaken': s('dmgtaken'),
            'dmgself': s('dmgself'), 'hits': s('hits'),
            'pickups': s('weapons') + s('ammo') + s('armor') + s('health') + s('powerups') + s('other'),
            'armor': s('armor'), 'health': s('health'), 'powerups': s('powerups'), 'weapons': s('weapons'),
            'n': len(cs),
        }
    h2h = [0, 0]
    same = [0, 0]
    bymod = [{}, {}]
    for f, att, vic, mod, kind in res['kills']:
        if kind != 'frag':
            continue
        sa, sv = slot_side.get(att), slot_side.get(vic)
        if sa is None or sv is None:
            continue
        if sa != sv:
            h2h[sa] += 1
        else:
            same[sa] += 1
        bymod[sa][mod] = bymod[sa].get(mod, 0) + 1
    rounds = [0, 0]
    if res.get('team'):
        for t in res['rounds']:
            if t in res['team']:
                rounds[res['team'].index(t)] += 1
    caps = [0, 0]
    for n in res['captures']:
        if name_side.get(n) is not None:
            caps[name_side[n]] += 1
    grabs = [0, 0]
    for n in res['flaggrabs']:
        if name_side.get(n) is not None:
            grabs[name_side[n]] += 1
    returns = [0, 0]
    for n in res.get('returns', []):
        if name_side.get(n) is not None:
            returns[name_side[n]] += 1
    rs = spec['ruleset']
    # The ruleset's own verdict: captures under ctf, rounds under arena, the
    # score everywhere else -- with the score as the tie-break.
    obj = caps if rs == 'ctf' else rounds if rs == 'arena' else [out[0]['score'], out[1]['score']]
    key = (obj[0] - obj[1], out[0]['score'] - out[1]['score'])
    winner = 0 if key > (0, 0) else 1 if key < (0, 0) else None
    tot = h2h[0] + h2h[1]
    return {
        'side': out, 'h2h': h2h, 'same': same, 'bymod': bymod, 'rounds': rounds, 'caps': caps,
        'grabs': grabs, 'returns': returns, 'objective': obj, 'winner': winner, 'scale': scale,
        'h2h_share': h2h[0] / tot if tot else None,
        'per_bot_kpm': [out[k]['kills'] * scale / max(1, out[k]['n']) for k in (0, 1)],
        'dmg_share': (out[0]['dmggiven'] / (out[0]['dmggiven'] + out[1]['dmggiven'])
                      if out[0]['dmggiven'] + out[1]['dmggiven'] else None),
        'suicide_rate': [out[k]['suicides'] * scale / max(1, out[k]['n']) for k in (0, 1)],
        'pickup_rate': [out[k]['pickups'] * scale / max(1, out[k]['n']) for k in (0, 1)],
        'dmg_per_hit': [out[k]['dmggiven'] / out[k]['hits'] if out[k]['hits'] else None for k in (0, 1)],
    }


def bootstrap_ci(xs, rng, n=2000):
    xs = [x for x in xs if x is not None]
    if len(xs) < 2:
        return (None, None)
    means = sorted(sum(rng.choice(xs) for _ in xs) / len(xs) for _ in range(n))
    return (means[int(0.025 * n)], means[int(0.975 * n) - 1])


def sign_test(wins, losses):
    """Exact two-sided binomial test of wins against losses at p = 0.5."""
    n = wins + losses
    if n == 0:
        return None
    k = min(wins, losses)
    p = sum(math.comb(n, i) for i in range(0, k + 1)) / 2 ** n
    return min(1.0, 2 * p)


def wilson(k, n, z=1.96):
    if n == 0:
        return (None, None)
    p = k / n
    d = 1 + z * z / n
    c = (p + z * z / (2 * n)) / d
    h = z * math.sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / d
    return (c - h, c + h)


def mean(xs):
    xs = [x for x in xs if x is not None]
    return sum(xs) / len(xs) if xs else None


class Cell:
    """A set of matches and what can be said about them."""

    def __init__(self, ms, rng):
        self.ms = ms
        self.n = len(ms)
        self.w = sum(1 for m in ms if m['winner'] == 0)
        self.l = sum(1 for m in ms if m['winner'] == 1)
        self.d = self.n - self.w - self.l
        self.p = sign_test(self.w, self.l)
        self.h2h = mean([m['h2h_share'] for m in ms])
        self.h2h_ci = bootstrap_ci([m['h2h_share'] for m in ms], rng)
        self.h2h_pooled = (sum(m['h2h'][0] for m in ms), sum(m['h2h'][1] for m in ms))
        self.dmg = mean([m['dmg_share'] for m in ms])
        self.dmg_ci = bootstrap_ci([m['dmg_share'] for m in ms], rng)
        self.score = [mean([m['side'][k]['score'] * m['scale'] / max(1, m['side'][k]['n']) for m in ms]) for k in (0, 1)]
        self.kpm = [mean([m['per_bot_kpm'][k] for m in ms]) for k in (0, 1)]
        # Pooled, not a mean of per-match ratios: one match with a single
        # death would otherwise outweigh ten ordinary ones.
        self.kd = [sum(m['side'][k]['kills'] for m in ms) / max(1, sum(m['side'][k]['deaths'] for m in ms))
                   if ms else None for k in (0, 1)]
        self.sui = [mean([m['suicide_rate'][k] for m in ms]) for k in (0, 1)]
        self.pick = [mean([m['pickup_rate'][k] for m in ms]) for k in (0, 1)]
        self.dph = [mean([m['dmg_per_hit'][k] for m in ms]) for k in (0, 1)]
        self.obj = [sum(m['objective'][k] for m in ms) for k in (0, 1)]
        self.grabs = [sum(m['grabs'][k] for m in ms) for k in (0, 1)]
        self.returns = [sum(m['returns'][k] for m in ms) for k in (0, 1)]

    def verdict(self, a, b):
        if self.p is None:
            return '-'
        if self.p < 0.05:
            return '**%s**' % (a if self.w > self.l else b)
        return 'n.s.'


def f(x, fmt='%.2f'):
    return '-' if x is None else fmt % x


def ci(c):
    return '-' if c[0] is None else '[%.2f, %.2f]' % c


def cmd_report(args, paths):
    out = os.path.abspath(args.out)
    mdir = os.path.join(out, 'matches')
    results = [json.load(open(os.path.join(mdir, p))) for p in sorted(os.listdir(mdir)) if p.endswith('.json')]
    if not results:
        sys.exit('botduel: no matches in %s' % mdir)
    valid = [r for r in results if r['valid']]
    invalid = [r for r in results if not r['valid']]
    rng = random.Random(1)
    modname = mods()
    for r in valid:
        r['m'] = metrics(r)
    a = valid[0]['spec']['a'] if valid else '?'
    b = valid[0]['spec']['b'] if valid else '?'
    rsets = [rs for rs in RULESETS if any(r['spec']['ruleset'] == rs for r in valid)]
    packs = [p for p in PACKS if any(r['spec']['pack'] == p for r in valid)]
    seeds = sorted({r['spec']['seed'] for r in valid})
    frames = sorted({r['spec']['frames'] for r in valid})
    per_side = sorted({r['spec']['per_side'] for r in valid})

    def cell(**kw):
        return Cell([r['m'] for r in valid if all(r['spec'][k] == v for k, v in kw.items())], rng)

    L = []
    L.append('# %s against %s -- bot match statistics\n' % (a, b))
    L.append('Generated by `tools/botduel.py report` from %d match(es) in `%s`; %d valid, %d invalid '
             '(listed at the end).\n' % (len(results), out, len(valid), len(invalid)))
    metas = sorted(p for p in os.listdir(out) if p.startswith('meta-'))
    if metas:
        meta = json.load(open(os.path.join(out, metas[-1])))
        L.append('Game library `%s` (sha1 %s), gladiator.so %s, q3bot.so %s, q2proded %s, host %s.\n' % (
            meta['lib'], meta['lib_sha1'], meta['gladiator_sha1'], meta['q3bot_sha1'],
            meta['q2proded_sha1'], meta['host']))
        if meta.get('no_mesh'):
            L.append('**Not played**, because a botlib has no mesh for it: %s.\n' % ', '.join(meta['no_mesh']))
    L.append('**Design.** Each match is %s frames (%s minutes of game time) measured after the warm-up, '
             'with %s bot(s) a side (one a side under duel). Seeds %s; a seed fixes the RNGs and which side '
             'joins first (red / team 0), and each match draws its own characters. Side A is `%s`, side B is `%s`.\n' % (
                 '/'.join(map(str, frames)), '/'.join('%g' % (x / 600) for x in frames),
                 '/'.join(map(str, per_side)), ', '.join(map(str, seeds)), a, b))
    L.append('**Measures.** *W-D-L*: matches A won, drew and lost on the ruleset\'s own objective -- '
             'captures under ctf, rounds under arena, the summed score elsewhere -- with the score as the '
             'tie-break; *p*: exact two-sided sign test on W against L. *H2H*: of the frags between the '
             'sides, the share A made (0.5 is parity), the mean over matches with a bootstrap 95% interval. '
             '*Dmg*: A\'s share of the damage the sides did to each other. *Score/bot*, *Kills/bot*: per bot '
             'per ten minutes. *K/D*: the side\'s kills over its deaths, pooled over the matches. *Suic*: deaths by self or world per bot per ten '
             'minutes. *Pick*: item pickups per bot per ten minutes. Damage is what reached health, as '
             'tourney\'s accuracy table counts it. Per-map cells hold one match per seed, so a per-map '
             '*p* below 0.05 needs five or more seeds all going one way, and with this many cells some '
             'will cross it by chance; the pooled rows are where the evidence is.\n')

    def row(label, c):
        return '| %s | %d | %d-%d-%d | %s | %s | %s %s | %s | %s : %s | %s : %s | %s : %s | %s : %s | %s : %s |' % (
            label, c.n, c.w, c.d, c.l, f(c.p, '%.3f'), c.verdict('A', 'B'), f(c.h2h), ci(c.h2h_ci), f(c.dmg),
            f(c.score[0], '%.1f'), f(c.score[1], '%.1f'), f(c.kpm[0], '%.1f'), f(c.kpm[1], '%.1f'),
            f(c.kd[0]), f(c.kd[1]), f(c.sui[0], '%.1f'), f(c.sui[1], '%.1f'),
            f(c.pick[0], '%.0f'), f(c.pick[1], '%.0f'))

    head = ('| %s | n | W-D-L (A) | p | better | H2H share A [95%%] | Dmg share A | Score/bot A : B | '
            'Kills/bot A : B | K/D A : B | Suic A : B | Pick A : B |\n|---|---|---|---|---|---|---|---|---|---|---|---|')

    L.append('\n## Overall\n')
    L.append(head % 'scope')
    L.append(row('everything', Cell([r['m'] for r in valid], rng)))
    for rs in rsets:
        L.append(row('ruleset %s' % rs, cell(ruleset=rs)))
    for p in packs:
        L.append(row('pack %s' % p, cell(pack=p)))
    L.append('')

    L.append('\n## Ruleset by pack\n')
    L.append(head % 'ruleset / pack')
    for rs in rsets:
        for p in packs:
            c = cell(ruleset=rs, pack=p)
            if c.n:
                L.append(row('%s / %s' % (rs, p), c))
    L.append('')

    for rs in rsets:
        L.append('\n## %s, per map\n' % rs)
        extra = {'ctf': 'captures A : B (flag grabs A : B, returns A : B)', 'arena': 'rounds won A : B'}.get(rs)
        L.append(head % 'pack / map')
        objrows = []
        for p in packs:
            for m in maps_for(rs, p):
                c = cell(ruleset=rs, pack=p, map=m)
                if c.n:
                    L.append(row('%s / %s' % (p, m), c))
                    if extra:
                        objrows.append('| %s / %s | %d : %d%s |' % (
                            p, m, c.obj[0], c.obj[1],
                            ' (%d : %d, %d : %d)' % tuple(c.grabs + c.returns) if rs == 'ctf' else ''))
        if extra:
            L.append('\n| pack / map | %s |\n|---|---|' % extra)
            L += objrows
        L.append('')

    L.append('\n## Frags by weapon\n')
    L.append('Frags between and within the sides, pooled over every valid match of the ruleset; '
             'the share is of that side\'s frags.\n')
    for rs in rsets:
        ms = [r['m'] for r in valid if r['spec']['ruleset'] == rs]
        tot = [{}, {}]
        for mm in ms:
            for k in (0, 1):
                for mod, n in mm['bymod'][k].items():
                    tot[k][int(mod)] = tot[k].get(int(mod), 0) + n
        allm = sorted(set(tot[0]) | set(tot[1]), key=lambda x: -(tot[0].get(x, 0) + tot[1].get(x, 0)))
        s0, s1 = sum(tot[0].values()) or 1, sum(tot[1].values()) or 1
        L.append('\n**%s**\n\n| weapon | A | A share | B | B share |\n|---|---|---|---|---|' % rs)
        for mod in allm:
            L.append('| %s | %d | %.0f%% | %d | %.0f%% |' % (
                modname.get(mod, str(mod)), tot[0].get(mod, 0), 100.0 * tot[0].get(mod, 0) / s0,
                tot[1].get(mod, 0), 100.0 * tot[1].get(mod, 0) / s1))
    L.append('')

    L.append('\n## Within-side frags\n')
    L.append('Under dm and dmpro a bot also frags its own side; these are excluded from H2H.\n')
    L.append('| ruleset | A frags A | B frags B | A frags B | B frags A |\n|---|---|---|---|---|')
    for rs in rsets:
        ms = [r['m'] for r in valid if r['spec']['ruleset'] == rs]
        L.append('| %s | %d | %d | %d | %d |' % (rs, sum(m['same'][0] for m in ms), sum(m['same'][1] for m in ms),
                                               sum(m['h2h'][0] for m in ms), sum(m['h2h'][1] for m in ms)))
    L.append('')

    if invalid:
        L.append('\n## Invalid matches\n\nNot counted above.\n')
        for r in invalid:
            L.append('* `%s`: %s' % (match_id(r['spec']), '; '.join(r['problems'])))
        L.append('')

    rep = os.path.join(out, 'report.md')
    with open(rep, 'w') as fh:
        fh.write('\n'.join(L) + '\n')
    with open(os.path.join(out, 'matches.csv'), 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['ruleset', 'pack', 'map', 'seed', 'a', 'b', 'first', 'frames', 'winner',
                    'objective_a', 'objective_b', 'h2h_a', 'h2h_b', 'score_a', 'score_b', 'kills_a', 'kills_b',
                    'deaths_a', 'deaths_b', 'suicides_a', 'suicides_b', 'dmggiven_a', 'dmggiven_b',
                    'hits_a', 'hits_b', 'pickups_a', 'pickups_b', 'wall_s'])
        for r in valid:
            s, m = r['spec'], r['m']
            sd = m['side']
            w.writerow([s['ruleset'], s['pack'], s['map'], s['seed'], s['a'], s['b'], r['first'], s['frames'],
                        {0: 'A', 1: 'B', None: 'draw'}[m['winner']], m['objective'][0], m['objective'][1],
                        m['h2h'][0], m['h2h'][1], sd[0]['score'], sd[1]['score'], sd[0]['kills'], sd[1]['kills'],
                        sd[0]['deaths'], sd[1]['deaths'], sd[0]['suicides'], sd[1]['suicides'],
                        sd[0]['dmggiven'], sd[1]['dmggiven'], sd[0]['hits'], sd[1]['hits'],
                        sd[0]['pickups'], sd[1]['pickups'], r['wall']])
    print('wrote %s and matches.csv: %d valid match(es), %d invalid' % (rep, len(valid), len(invalid)))
    return valid, invalid


# ------------------------------------------------------------------ controls

def cmd_control(args, paths):
    """Three controls; exits non-zero unless each one comes out as it must."""
    paths.check()
    so = shim(paths)
    out = os.path.abspath(args.out)
    os.makedirs(out, exist_ok=True)
    verdicts = []

    # 1. determinism: one spec twice, in two fixtures, and a second seed.
    d1, d2 = os.path.join(out, 'det1'), os.path.join(out, 'det2')
    make_fixture(paths, d1)
    make_fixture(paths, d2)
    base = {'ruleset': 'dm', 'pack': 'baseq2', 'map': 'q2dm1', 'a': 'gladiator', 'b': 'q3:4',
            'frames': 3000, 'per_side': 2}
    r1 = play(paths, so, d1, dict(base, seed=11), None)
    r2 = play(paths, so, d2, dict(base, seed=11), None)
    r3 = play(paths, so, d1, dict(base, seed=12), None)
    fp = lambda r: json.dumps([r['kills'], [(c['name'], c['score'], c['dmggiven']) for c in r['clients']]])
    same = r1['valid'] and fp(r1) == fp(r2) and len(r1['kills']) > 0
    differ = r3['valid'] and fp(r1) != fp(r3)
    verdicts.append(('determinism: seed 11 twice is identical (%d kills)' % len(r1['kills']), same))
    verdicts.append(('determinism: seed 12 differs from seed 11', differ))
    shutil.rmtree(d1, ignore_errors=True)
    shutil.rmtree(d2, ignore_errors=True)

    # 2 and 3: the null and the sensitivity controls, through the real run.
    def series(name, a, b, rulesets):
        o = os.path.join(out, name)
        ns = argparse.Namespace(out=o, a=a, b=b, rulesets=rulesets, packs=['baseq2'], maps=None,
                                seeds=args.seeds, frames=args.frames, per_side=args.per_side, jobs=args.jobs)
        cmd_run(ns, paths)
        valid, _ = cmd_report(argparse.Namespace(out=o), paths)
        rng = random.Random(1)
        return Cell([metrics(r) for r in valid], rng)

    for a, b in (('gladiator', 'gladiator'), ('q3:4', 'q3:4')):
        c = series('null-%s' % a.replace(':', ''), a, b, ['dm', 'tdm'])
        ok = c.p is not None and c.p >= 0.05 and c.h2h_ci[0] is not None and c.h2h_ci[0] <= 0.5 <= c.h2h_ci[1]
        verdicts.append(('null: %s v %s shows no difference -- W-D-L %d-%d-%d, p %s, H2H %s %s' % (
            a, b, c.w, c.d, c.l, f(c.p, '%.3f'), f(c.h2h), ci(c.h2h_ci)), ok))
    c = series('sens-q31-q35', 'q3:1', 'q3:5', ['dm', 'tdm'])
    ok = c.p is not None and c.p < 0.05 and c.h2h_ci[1] is not None and c.h2h_ci[1] < 0.5
    verdicts.append(('sensitivity: q3:1 v q3:5 is told apart -- W-D-L %d-%d-%d, p %s, H2H %s %s' % (
        c.w, c.d, c.l, f(c.p, '%.3f'), f(c.h2h), ci(c.h2h_ci)), ok))

    print()
    for text, ok in verdicts:
        print('%-6s %s' % ('ok' if ok else 'FAILED', text))
    print('%d control(s), %d as they must be' % (len(verdicts), sum(ok for _, ok in verdicts)))
    sys.exit(0 if all(ok for _, ok in verdicts) else 1)


# ------------------------------------------------------------------ main

def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter,
                                 epilog=__doc__.split('\n', 2)[2])
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('meshes', help='make, warm and cache every AAS file the matrix needs')
    p.add_argument('maps', nargs='*')
    p.add_argument('-j', '--jobs', type=int, default=3)

    def common(p):
        p.add_argument('-o', '--out', required=True)
        p.add_argument('-j', '--jobs', type=int, default=3)
        p.add_argument('--seeds', type=lambda s: [int(x) for x in s.split(',')] if ',' in s
                       else list(range(1, int(s) + 1)), default=list(range(1, 6)),
                       help='N for seeds 1..N, or a comma list (default 5)')
        p.add_argument('--frames', type=int, default=6000, help='the window, in frames (default 6000)')
        p.add_argument('--per-side', type=int, default=2, help='bots a side; duel is always 1 (default 2)')

    p = sub.add_parser('run', help='play the matrix (resumable)')
    common(p)
    p.add_argument('-a', default='gladiator', help='side A (default gladiator)')
    p.add_argument('-b', default='q3:4', help='side B (default q3:4)')
    p.add_argument('--rulesets', type=lambda s: s.split(','), default=RULESETS)
    p.add_argument('--packs', type=lambda s: s.split(','), default=list(PACKS))
    p.add_argument('--maps', type=lambda s: s.split(','), default=None)
    p = sub.add_parser('report', help='statistics out of a run directory')
    p.add_argument('-o', '--out', required=True)
    p = sub.add_parser('control', help='determinism, null and sensitivity controls')
    common(p)
    args = ap.parse_args()
    paths = Paths()
    {'meshes': cmd_meshes, 'run': cmd_run, 'report': cmd_report, 'control': cmd_control}[args.cmd](args, paths)


if __name__ == '__main__':
    main()
