#!/usr/bin/env python3
"""The timer-unit contract, mechanised.

q2pro's `Convert ... to frame numbers.` commits retype every timer from a float
count of seconds to an int count of frames.  A field's *type* therefore no
longer says what its *unit* is: `int nextthink` holds frames, `float level.time`
holds seconds, and mixing them compiles silently and runs wrong by a factor of
ten.  `q2pro/doc/mission-packs.md` names this as the fourth contract the replay
could not check and gives two greps for it; this is those two greps, plus the
part a merged tree needs and separate packs do not.

WHY A MERGED TREE NEEDS MORE.  Upstream ships four independent game libraries,
so each one is internally consistent even where they disagree: xatrix declares
`monsterinfo.attack_finished` an `int` and writes frames, rogue declares it a
`float` and writes seconds, and neither is wrong on its own.  Colosseum has ONE
g_local.h, so exactly one of those declarations survives the merge and every
site written against the other is now a unit mix.  That is how the gekk's three
`attack_finished` sites came to disagree with the other fifty, and no
upstream check could have seen it.

FOUR CHECKS

  MIX    an int-declared timer field used with level.time, or a float-declared
         one used with level.framenum, on the same line.
  SCALE  level.framenum combined with a floating-point literal and no
         BASE_FRAMERATE.  A frame count is an integer, so a bare `5.0f` beside
         one is almost always a duration that forgot to be scaled.
  SPLIT  one field, both clocks, across the tree -- the merged-tree case above.
         Reported per field with the minority side named, because the minority
         is what has to move.
  SHIFT  a `pm_time` written with a raw literal instead of `PM_TIME_SHIFT`.
         This is a second unit contract in the same tree and it is the
         PROTOCOL's rather than the clock's: `PM_TIME_SHIFT` is 3 on a plain
         server and **0** on one that negotiated extensions, so `160 >> 3` and
         a bare `14` are the pre-extension spellings and hold for an eighth as
         long as they say on an extended server.  RA2's own post-port fix list
         names one instance of this;
         four more survived in CTF, Ground Zero and the Gladiator observer
         until somebody read the assignments side by side.

None of the three can see a comparison written entirely in the wrong unit --
`SV_RunThink`'s `thinktime > level.time` mentions no timer field at all.  Only
running a level and waiting finds that, which is the census check's job and why
`sv ruleset` prints level.framenum.

USAGE
    tools/units.py [--tree src]
"""
import argparse
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Members whose name marks them as a timer.  `_framenum` is q2pro's converted
# spelling; the rest are the unconverted names that survive in the donors.
TIMER = re.compile(r'\b(\w*(?:_framenum|_finished|_debounce|time|timeout'
                   r'|nextthink|timestamp|electtime|matchtime|lasttime'
                   r'|freetime))\b')

# Anything that is plainly not a timer despite matching above.
NOT_TIMER = {
    'level.time', 'levelname', 'framenum', 'timelimit', 'matchtime',
    'timeout', 'time', 'timestamp_type', 'realtime', 'frametime',
    'FRAMETIME', 'BASE_FRAMERATE', 'matchsetuptime', 'matchstarttime',
    'lastidtime', 'menutime', 'nextmap',
}

FLOAT_LIT = re.compile(r'(?<![\w.])\d+\.\d*f?(?![\w.])')

# `level.framenum + 5.0f` / `5.0f + level.framenum` / `level.framenum - 2.0`:
# a float literal added to or subtracted from a frame count, which is a duration
# that forgot its scale.  Requiring adjacency is what keeps an unrelated literal
# elsewhere on the line from reading as one.
# `pmove.pm_time = <something>`.  The value is captured so an assignment of a
# literal 0 -- "cancel the hold", which has no unit at all -- is not a finding.
PM_TIME = re.compile(r'([\w\[\]\.>-]*\bpm_time)\s*=\s*([^;]+);')

ADJACENT_LIT = re.compile(
    r'level\.framenum\s*[-+]\s*\(?\s*\d+\.\d*f?'
    r'|\d+\.\d*f?\s*[-+]\s*level\.framenum')


def declared_types(header):
    """member name -> 'int' | 'float', from the struct declarations."""
    out = {}
    for m in re.finditer(r'^\s*(int|float|unsigned|int64_t)\s+(\w+)\s*(?:\[\s*\w*\s*\])?\s*;',
                         header, re.M):
        ty, name = m.group(1), m.group(2)
        if TIMER.search(name):
            out[name] = 'int' if ty != 'float' else 'float'
    return out


def strip(t):
    def blank(m):
        return '\n' * m.group(0).count('\n')
    t = re.sub(r'/\*.*?\*/', blank, t, flags=re.S)
    return re.sub(r'//[^\n]*', '', t)


def sources(tree):
    out = []
    for root, dirs, files in os.walk(tree):
        dirs[:] = [d for d in dirs if not d.startswith(('debug', 'release'))]
        for f in sorted(files):
            if f.endswith(('.c', '.h')):
                out.append(os.path.join(root, f))
    return out


# A check that has never failed is not trusted.  Each control
# is a real reversion of a real fix from this import, applied to a copy of the
# tree in memory.
SELFTESTS = [
    ('mix', 'g_weapon.c',
     ('trap->timestamp = level.framenum + 30 * BASE_FRAMERATE;',
      'trap->timestamp = level.time + 30;'),
     'int field read against level.time'),
    ('lost scale', 'g_func.c',
     ('ent->last_move_framenum = level.framenum - 1.0f * BASE_FRAMERATE;',
      'ent->last_move_framenum = level.framenum - 1.0f;'),
     'float literal and no BASE_FRAMERATE'),
    ('split', 'm_gekk.c',
     ('self->monsterinfo.attack_finished = level.time + 3;',
      'self->monsterinfo.attack_finished = level.framenum + 3 * BASE_FRAMERATE;'),
     'is used with BOTH clocks'),
    # The mutation IS the defect this check was written for: the 1999 spelling
    # of the observer's teleport hold, which holds for an eighth as long as it
    # says on an extended server.
    ('raw pm_time', 'p_observer.c',
     ('ent->client->ps.pmove.pm_time = 112 >> PM_TIME_SHIFT;',
      'ent->client->ps.pmove.pm_time = 14;'),
     'pm_time written without PM_TIME_SHIFT'),
    # ...and the copy that crosses the ABI, which the exemption used to wave
    # through.  The mutation restores the line as it stood before
    # `osp-tourney@11563de`.
    ('copied pm_time', 'bl_main.c',
     ('buc.pm_time = min(bot->client->ps.pmove.pm_time >> (3 - PM_TIME_SHIFT), 255);',
      'buc.pm_time = bot->client->ps.pmove.pm_time;'),
     'pm_time written without PM_TIME_SHIFT'),
]


def selftest(tree):
    bad = 0
    for name, fname, (orig, rev), expect in SELFTESTS:
        out = []
        hit = False
        for path in sources(tree):
            t = open(path, encoding='latin-1').read()
            if os.path.basename(path) == fname and orig in t:
                t = t.replace(orig, rev, 1)
                hit = True
            out.append((path, t))
        if not hit:
            print(f'  !! control "{name}": its own mutation no longer applies '
                  f'-- the control has rotted')
            bad += 1
            continue
        text = '\n'.join(run(tree, out))
        if expect in text:
            print(f'  ok  control "{name}" fires: {expect}')
        else:
            print(f'  !! control "{name}" did NOT fire; the check is asleep')
            bad += 1
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tree', default=os.path.join(REPO, 'src'))
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    tree = os.path.abspath(a.tree)
    if a.selftest:
        return 1 if selftest(tree) else 0

    lines = run(tree, [(p, open(p, encoding='latin-1').read())
                       for p in sources(tree)])
    for l in lines:
        print(l)
    return 1 if any('!!' in l for l in lines) else 0


def run(tree, files):
    out = []
    header = os.path.join(tree, 'g_local.h')
    if not os.path.exists(header):
        return ['units.py: no g_local.h in %s' % tree]
    types = declared_types(dict(files).get(header) or
                           open(header, encoding='latin-1').read())

    mix, scale, shift = [], [], []
    clocks = {}     # field -> {'time': n, 'framenum': n}

    for path, text in files:
        name = os.path.basename(path)
        for i, line in enumerate(strip(text).split('\n'), 1):
            # SHIFT is asked first because it is about a different clock and
            # would otherwise be skipped by the level.time/framenum filter.
            m = PM_TIME.search(line)
            if m:
                lhs, rhs = m.group(1).strip(), m.group(2).strip()
                # A literal 0 cancels the hold and is zero in either unit, and a
                # line that already names PM_TIME_SHIFT has been converted.
                if rhs == '0' or 'PM_TIME_SHIFT' in line:
                    pass
                elif 'pm_time' in rhs:
                    # A COPY constructs no duration -- but it can still cross a
                    # UNIT BOUNDARY, and this exemption used to say it could not.
                    # `ps.pmove.pm_time` is 8 ms tics on a plain server and
                    # MILLISECONDS on an extended one; `bot_updateclient_t`'s is
                    # the frozen 1999 botlib ABI and is always tics.  bl_main.c
                    # copied one straight into the other, so every hold the brain
                    # saw ran eight times long, and the old wording here -- "it
                    # COPIES a value that was already shifted where it was set"
                    # -- is exactly the reasoning that let it stand
                    # (`osp-tourney@11563de`).  So a copy is exempt only between
                    # two fields of the SAME kind.
                    if ('ps.pmove.pm_time' in rhs) != ('ps.pmove.pm_time' in lhs):
                        shift.append((name, i, line.strip()[:70]))
                else:
                    shift.append((name, i, line.strip()[:70]))

            has_t = 'level.time' in line
            has_f = 'level.framenum' in line
            if not (has_t or has_f):
                continue

            fields = {f for f in TIMER.findall(line)
                      if f in types and f not in NOT_TIMER}
            for f in fields:
                c = clocks.setdefault(f, {'time': 0, 'framenum': 0})
                if has_t:
                    c['time'] += 1
                if has_f:
                    c['framenum'] += 1
                # MIX: the declaration and the clock disagree on this line
                if types[f] == 'int' and has_t and not has_f:
                    mix.append((name, i, f, 'int field read against level.time',
                                line.strip()[:70]))
                elif types[f] == 'float' and has_f and not has_t \
                        and 'BASE_FRAMERATE' not in line:
                    # A float field beside level.framenum is CORRECT when it is
                    # being scaled -- `level.framenum + st.pausetime *
                    # BASE_FRAMERATE` is the converted idiom, not a mix.  Without
                    # this g_func.c's func_timer reads as a defect.
                    mix.append((name, i, f, 'float field used with level.framenum',
                                line.strip()[:70]))

            # SCALE: a frame count with a float literal in the SAME arithmetic
            # expression and no scaling.  Anywhere-on-the-line is too loose: it
            # reported `if ((random() < 0.1f) || (level.framenum < timestamp))`
            # and a brightness ratio, neither of which is a duration.
            if has_f and 'BASE_FRAMERATE' not in line and 'FRAMETIME' not in line \
                    and ADJACENT_LIT.search(line):
                scale.append((name, i, line.strip()[:70]))

    # SPLIT: one field, both clocks
    split = [(f, c) for f, c in sorted(clocks.items())
             if c['time'] and c['framenum']]

    out.append(f'units.py: {len(types)} declared timer field(s); '
               f'{len(mix)} mix, {len(scale)} lost-scale, {len(split)} split, '
               f'{len(shift)} raw pm_time')

    for n, i, f, why, txt in mix:
        out.append(f'  !! {n}:{i}: {f} -- {why}: {txt}')
    for n, i, txt in scale:
        out.append(f'  !! {n}:{i}: level.framenum with a float literal and no '
                   f'BASE_FRAMERATE: {txt}')
    for f, c in split:
        minority = 'framenum' if c['framenum'] < c['time'] else 'time'
        out.append(f'  !! {f} is used with BOTH clocks -- {c["time"]} line(s) '
                   f'with level.time, {c["framenum"]} with level.framenum. The '
                   f'{minority} side is the minority and is what has to move')

    for n, i, txt in shift:
        out.append(f'  !! {n}:{i}: pm_time written without PM_TIME_SHIFT -- an '
                   f'extended server holds for an eighth as long: {txt}')

    if not (mix or scale or split or shift):
        out.append('  every declared timer field agrees with the clock it is '
                   'used with')
        out.append('  (this cannot see a comparison written entirely in the '
                   'wrong unit -- the temporal check covers that)')
    return out


if __name__ == '__main__':
    sys.exit(main())
