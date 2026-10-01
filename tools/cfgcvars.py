#!/usr/bin/env python3
"""Every cvar a shipped config sets is one something registers.

WHY.  A `set` of a name nothing registers is silent in Q2PRO: the engine
creates a user cvar, nothing ever reads it, and the operator is told nothing.
The four OSP configs shipped `set osp_statsfile 0` against a library whose
switch is `statsfile` -- a name a spec row had predicted and resolution never
adopted -- so every server started from them wrote the stats file its config
said was off.  `tools/extras.sh` boots every config and could not see it,
because an unknown `set` is not an error and prints nothing.

WHAT COUNTS AS REGISTERED.  A `gi.cvar("name"` anywhere in the tree, a name the
ENGINE registers (listed below, because a config may tune the server as well
as the game), or one of the INDIRECT names: the library registers those through
a computed name, so no literal `gi.cvar("name"` exists to find.  Each INDIRECT
and ENGINE entry must still be set by some shipped config, or it is reported
-- an allowance nothing uses is how a list like this rots.

USAGE
    tools/cfgcvars.py [--tree src] [--gamedir colosseum]
    tools/cfgcvars.py --selftest
Findings are printed with `!!` and exit 1.
"""
import argparse
import glob
import os
import re
import shutil
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Registered by the library through a computed name.
INDIRECT = {
    # BotMinPlayersCvar() -- `minimumplayers` outside the OSP four,
    # `bots_minplayers` inside them; bl_main.c:BotMinPlayers() registers
    # whichever the ruleset reads.
    'minimumplayers': 'bot/bl_main.c BotMinPlayers()',
    # BotFileCvar(), the same split: `botfile` / `bots_botfile`.
    'botfile': 'bot/bl_main.c BotFileCvar()',
    # BotSetVarIfSet(lib, "autolaunchbspc", ...) reads the game cvar of the
    # libvar's name and pushes it to the brain only when it is set.
    'autolaunchbspc': 'bot/bl_main.c BotSetVarIfSet()',
}

# Registered by the engine before the game library loads.  Only names a shipped
# config actually sets belong here (see WHAT COUNTS AS REGISTERED).
ENGINE = {
}

SET_RE = re.compile(r'^\s*(?:set|seta|sets|setu)\s+(\S+)', re.I)
REG_RE = re.compile(r'gi\.cvar\(\s*"([^"]+)"')


def registered(tree):
    names = set()
    for path in glob.glob(os.path.join(tree, '**', '*.[ch]'), recursive=True):
        with open(path, encoding='latin-1') as f:
            names.update(REG_RE.findall(f.read()))
    return names


def config_sets(gamedir):
    out = {}
    paths = [os.path.join(gamedir, 'server.cfg')] + \
        sorted(glob.glob(os.path.join(gamedir, 'configs', '*.cfg')))
    for path in paths:
        if not os.path.exists(path):
            continue
        with open(path, encoding='latin-1') as f:
            for n, line in enumerate(f, 1):
                m = SET_RE.match(line.split('//', 1)[0])
                if m:
                    out.setdefault(m.group(1), []).append(
                        '%s:%d' % (os.path.relpath(path, REPO), n))
    return out


def check(tree, gamedir, indirect=None, engine=None):
    indirect = INDIRECT if indirect is None else indirect
    engine = ENGINE if engine is None else engine
    reg = registered(tree)
    sets = config_sets(gamedir)
    lines = []
    if not sets:
        lines.append('  !! no `set` line found under %s -- the check has '
                     'nothing to look at' % gamedir)
    for name in sorted(sets):
        if name in reg or name in indirect or name in engine:
            continue
        lines.append('  !! %s sets `%s`, which nothing registers -- the engine '
                     'will create it and nothing will read it'
                     % (sets[name][0], name))
    for name in sorted(set(indirect) | set(engine)):
        if name not in sets:
            lines.append('  !! `%s` is allowed as registered elsewhere but no '
                         'shipped config sets it -- a stale allowance' % name)
    return lines, len(sets), len(reg)


def selftest():
    bad = 0
    tree = os.path.join(REPO, 'src')
    gamedir = os.path.join(REPO, 'colosseum')

    lines, _, _ = check(tree, gamedir)
    if lines:
        print('  !! control "the shipped set is clean" did NOT fire:')
        for l in lines:
            print('    ' + l)
        bad += 1
    else:
        print('  ok  control "the shipped set is clean" fires')

    with tempfile.TemporaryDirectory() as tmp:
        g = os.path.join(tmp, 'colosseum')
        shutil.copytree(gamedir, g)
        with open(os.path.join(g, 'configs', 'dm.cfg'), 'a') as f:
            f.write('set osp_statsfile 0\n')
        lines, _, _ = check(tree, g)
        if any('osp_statsfile' in l for l in lines):
            print('  ok  control "an unregistered set is reported" fires')
        else:
            print('  !! control "an unregistered set is reported" did NOT fire')
            bad += 1

    lines, _, _ = check(tree, gamedir,
                        indirect=dict(INDIRECT, no_such_allowance='control'))
    if any('no_such_allowance' in l and 'stale' in l for l in lines):
        print('  ok  control "a stale allowance is reported" fires')
    else:
        print('  !! control "a stale allowance is reported" did NOT fire')
        bad += 1
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tree', default=os.path.join(REPO, 'src'))
    ap.add_argument('--gamedir', default=os.path.join(REPO, 'colosseum'))
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    if a.selftest:
        bad = selftest()
        sys.exit(1 if bad else 0)
    lines, nsets, nreg = check(a.tree, a.gamedir)
    for l in lines:
        print(l)
    print('cfgcvars.py: %d name(s) set by the shipped configs, %d registered '
          'in the tree, %d finding(s)' % (nsets, nreg, len(lines)))
    sys.exit(1 if lines else 0)


if __name__ == '__main__':
    main()
