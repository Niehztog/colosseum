"""R-142's check: two names for one position in a list somebody else indexes.

The defect this exists for is not a collision in the abstract -- plenty of
unrelated constants share a value harmlessly, and MAX_ITEMS being 256 like
MAX_CLIENTS is nobody's problem.  It is a collision inside a family whose
numbers are POSITIONS IN AN ORDERED LIST that something outside this tree walks:

  * `WEAP_*` is the ordinal of a weapon's `#w_*.md2` in SP_worldspawn's
    precache block, because ChangeWeapon puts it in the high byte of
    `s.skinnum` and the client draws `weaponmodel[skinnum >> 8]`.  Each donor
    numbered its own extras from 12, R-CORE-2 unioned the CONTENT into one
    ordered list, and seven weapons then drew somebody else's model.

  * the brain's `INVENTORY_*` is the same shape one boundary further out, and
    is R-141: `botfiles/inv.h` numbers this tree's itemlist and the itemlist
    moved under it.  That half is checked by comparing the two lists, which
    only `sv botinv` can do at runtime; this tool checks the half that is
    visible in the source -- that no two names in such a family share a slot.

TWO CHECKS.

  DUPLICATES   every `#define` family and every enum in the tree, reported when
               two DISTINCT names resolve to one integer.  Families that are
               deliberately aliased are listed in ALIASES with the reason; a
               family that is not an ordered list is reported as `info` and
               does not fail.  ORDERED is the set that fails the build.

  ORDER        `WEAP_*` specifically, against the precache block that defines
               what its numbers mean.  A value check cannot see a family that
               is internally consistent and disagrees with the list it indexes,
               which is exactly what R-142 was: no two WEAP_* collided after a
               renumbering, and they would still have been wrong if the block
               and the header had been renumbered differently.

`--selftest` mutates both -- a duplicated WEAP_ value, and a header whose order
disagrees with the precache block -- and asserts each is caught, because a
check that cannot fail is not a check.
"""
import re, sys, os

# Families whose members legitimately share values, with the reason.
ALIASES = {
    'ACTION_': 'ACTION_JUMP/MOVEUP and CROUCH/MOVEDOWN are one bit by design '
               '(the brain sends one flag for both meanings)',
}

# Families whose numbers are positions in a list something else walks.  Only
# these fail the build; everything else is reported for the reader.
ORDERED = {'WEAP_'}

# The precache block WEAP_* indexes, and the item each slot belongs to.  Slot 0
# is the client's own "weapon.md2" default, which no WEAP_ names.
VWEP_MODELS = 'src/g_spawn.c'
VWEP_EXPECT = {
    'WEAP_BLASTER': '#w_blaster.md2',
    'WEAP_SHOTGUN': '#w_shotgun.md2',
    'WEAP_SUPERSHOTGUN': '#w_sshotgun.md2',
    'WEAP_MACHINEGUN': '#w_machinegun.md2',
    'WEAP_CHAINGUN': '#w_chaingun.md2',
    'WEAP_GRENADES': '#a_grenades.md2',
    'WEAP_GRENADELAUNCHER': '#w_glauncher.md2',
    'WEAP_ROCKETLAUNCHER': '#w_rlauncher.md2',
    'WEAP_HYPERBLASTER': '#w_hyperblaster.md2',
    'WEAP_RAILGUN': '#w_railgun.md2',
    'WEAP_BFG': '#w_bfg.md2',
    'WEAP_GRAPPLE': '#w_grapple.md2',
    'WEAP_PHALANX': '#w_phalanx.md2',
    'WEAP_BOOMER': '#w_ripper.md2',
    'WEAP_DISRUPTOR': '#w_disrupt.md2',
    'WEAP_ETFRIFLE': '#w_etfrifle.md2',
    'WEAP_PLASMA': '#w_plasma.md2',
    'WEAP_PROXLAUNCH': '#w_plauncher.md2',
    'WEAP_CHAINFIST': '#w_chainfist.md2',
}


def read(p):
    return open(p, encoding='latin-1').read()


def headers(tree):
    out = []
    for root, _, files in os.walk(tree):
        for f in sorted(files):
            if f.endswith('.h'):
                out.append(os.path.join(root, f))
    for extra in ('inc/shared/shared.h', 'inc/shared/game.h'):
        if os.path.exists(extra):
            out.append(extra)
    return out


def families(paths, texts=None):
    """prefix -> {value: {names}} over #defines and explicitly-valued enums."""
    fam = {}

    def add(name, val):
        pre = name.split('_')[0] + '_'
        fam.setdefault(pre, {}).setdefault(val, set()).add(name)

    for path in paths:
        txt = texts[path] if texts and path in texts else read(path)
        for line in txt.splitlines():
            m = re.match(r'\s*#define\s+([A-Z][A-Z0-9]*(?:_[A-Z0-9]+)+)\s+(\d+)\s*(?://.*)?$', line)
            if m:
                add(m.group(1), int(m.group(2)))
        body = re.sub(r'/\*.*?\*/', '', txt, flags=re.S)
        body = re.sub(r'//[^\n]*', '', body)
        for blk in re.findall(r'enum[^{;]*\{(.*?)\}', body, re.S):
            nxt = 0
            for item in blk.split(','):
                m = re.match(r'([A-Za-z_]\w*)\s*(?:=\s*(-?\w+))?$', item.strip())
                if not m:
                    continue
                if m.group(2) is not None:
                    try:
                        nxt = int(m.group(2), 0)
                    except ValueError:
                        continue
                add(m.group(1), nxt)
                nxt += 1
    return fam


def check_duplicates(fam):
    bad, info = [], []
    for pre in sorted(fam):
        for val, names in sorted(fam[pre].items()):
            if len(names) < 2:
                continue
            row = f'{pre} value {val} shared by ' + ', '.join(sorted(names))
            if pre in ALIASES:
                continue
            (bad if pre in ORDERED else info).append(row)
    return bad, info


def vwep_order(text):
    """the registration ordinal of each `#`-prefixed model, 1-based."""
    order = re.findall(r'gi\.modelindex\("(#[^"]+)"\)', text)
    return {m: i + 1 for i, m in enumerate(order)}


def check_order(fam, spawn_text):
    slot = vwep_order(spawn_text)
    value = {}
    for val, names in fam.get('WEAP_', {}).items():
        for n in names:
            value[n] = val
    bad = []
    for name, model in sorted(VWEP_EXPECT.items()):
        if name not in value:
            bad.append(f'{name} is not defined')
            continue
        if model not in slot:
            bad.append(f'{model} is not registered in {VWEP_MODELS}')
            continue
        if value[name] != slot[model]:
            bad.append(f'{name} is {value[name]} but {model} is registered at '
                       f'slot {slot[model]} -- a client would draw '
                       f'{[m for m, i in slot.items() if i == value[name]] or ["nothing"]}')
    return bad


def run(tree, texts=None, spawn_text=None):
    paths = headers(tree)
    fam = families(paths, texts)
    dup, info = check_duplicates(fam)
    order = check_order(fam, spawn_text if spawn_text is not None else read(VWEP_MODELS))
    return dup, info, order


def main():
    tree = 'src'
    if '--tree' in sys.argv:
        tree = sys.argv[sys.argv.index('--tree') + 1]
    selftest = '--selftest' in sys.argv
    dup, info, order = run(tree)

    if '-v' in sys.argv or '--verbose' in sys.argv:
        for row in info:
            print(f'  info  {row}')
    elif info:
        print(f'  ..    {len(info)} harmless same-value pair(s) in families that are '
              f'not ordered lists (-v to list them)')
    # '!!' is the marker audit.py's driver greps for (R-TOOL-3).
    for row in dup:
        print(f'  !! ordered family collides: {row}')
    for row in order:
        print(f'  !! vwep order: {row}')

    if selftest:
        controls = 0
        # 1. a duplicated value inside an ordered family must be caught
        paths = headers(tree)
        texts = {p: read(p) for p in paths}
        h = 'src/g_local.h'
        texts[h] = texts[h].replace('#define WEAP_PHALANX            13',
                                    '#define WEAP_PHALANX            12')
        d, _, _ = run(tree, texts)
        assert d, 'selftest: a duplicated WEAP_ value was not caught'
        controls += 1
        # 2. a header that disagrees with the precache block must be caught
        bad_spawn = read(VWEP_MODELS).replace('gi.modelindex("#w_phalanx.md2");',
                                             'gi.modelindex("#w_tesla.md2");')
        _, _, o = run(tree, None, bad_spawn)
        assert o, 'selftest: a header/precache disagreement was not caught'
        controls += 1
        print(f'  ..    {controls} control(s) fail as they must')

    if dup or order:
        return 1
    print('  ok    no ordered family collides, and WEAP_ matches its precache block')
    return 0


if __name__ == '__main__':
    sys.exit(main())
