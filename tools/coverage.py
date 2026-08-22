#!/usr/bin/env python3
"""Derive the R-CORE-12 replay-coverage ledger (deliverable D9).

R-CORE-12: no donor received all 188 Q2PRO game commits, and because Colosseum
keeps the monsters and the campaigns the donors dropped, a commit a donor
skipped for want of a site usually *has* a site here.  D9 is one row per
(donor, skipped commit) with a verdict: *re-apply*, *not applicable*, or
*superseded*.  R-VER-10 makes a row with no verdict block the phase that
imports that donor.

METHOD.  Every replayed commit on a `port_<donor>` branch carries a
`q2pro-commit: <sha>` trailer naming the upstream Q2PRO commit it replays.  The
`baseq2` spine branch carries the same trailers for all 188.  A commit whose
trailer appears on the spine but not on a donor's branch was skipped by that
donor.  Nothing here is inferred from the diff -- the trailer is the record.

The verdicts themselves are judgement and live in VERDICTS below, keyed by
commit subject.  A subject with no entry is emitted as `TODO` and makes this
tool exit non-zero, which is R-VER-10 mechanised: the ledger cannot silently
be incomplete.

USAGE
    tools/coverage.py                       # markdown to stdout
    tools/coverage.py --md doc/replay-coverage.md
    tools/coverage.py --check               # exit 1 if any row lacks a verdict
"""
import argparse
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE = os.path.join(REPO, 'build', 'replay.git')

SPINE = 'refs/bundles/q2pro-mission-pack-replay/baseq2'
DONORS = [
    ('ctf',    'refs/bundles/q2pro-mission-pack-replay/port_ctf'),
    ('xatrix', 'refs/bundles/q2pro-mission-pack-replay/port_xatrix'),
    ('rogue',  'refs/bundles/q2pro-mission-pack-replay/port_rogue'),
    ('ra2',    'refs/bundles/osp-q2pro-port-replay/port_ra2'),
    ('osp',    'refs/bundles/osp-q2pro-port-replay/port_osp'),
]

# Verdicts, keyed by the upstream commit subject.
#
#   re-apply        the commit has a site in Colosseum that it did not have in
#                   the donor, and landing it is work
#   not-applicable  the commit touches something Colosseum does not have, or the
#                   donor's omission is correct here too
#   superseded      a later change already covers it
#
# The rule that decides most of these: R-CORE-8 keeps every monster and both
# campaigns, so a fix the donor skipped "for want of a site" has a site here.
# R-VER-10's other half: "every *re-apply* verdict has a commit that lands it."
# A verdict is a judgement about a (donor, commit) pair; LANDED records that the
# pair has actually been discharged, keyed the same way, because a verdict alone
# does not say whether anyone acted on it.
#
# Keyed by (donor, normalised subject) so the same commit can be landed for one
# donor and still outstanding for another -- which is the normal case, since a
# fix skipped by three donors is discharged once per import.
LANDED = {
    ('ctf', 'Avoid generating missing savegame pointer'):
        'landed, Phase 3: m_flyer.c carries no commented `&flyer_move_attack1` '
        'and check-ptrs passes',
    ('ctf', 'Fix crashes due to G_PickTarget() returning NULL'):
        'landed, Phase 3 -- and it found a THIRD call site. The two the '
        'commit names are guarded; turret_brain_link is Ground Zero\'s own '
        'addition, arrived in Phase 2, and dereferenced the result unguarded. '
        'See reconciliation.md R-53: re-applying a fix means re-asking its '
        'question of the merged tree, not confirming its hunks survived',
    ('ctf', 'Use initializer for aim vector'):
        'landed, Phase 3: five `vec3_t aim = { ... }` initialisers across '
        'm_berserk.c and m_brain.c',
}

VERDICTS = {
    # --- monster / AI fixes.  Three donors deleted the monster set, so these
    # had nowhere to land there.  Colosseum keeps all 31 monster TUs, so every
    # one of them has a site and must be re-applied.  R-CORE-8, R-SP-5.
    'Fix nofriendlyfire being applied to AI in coop':
        ('re-apply', 'Coop + monsters both exist here (R-SP-3, R-CORE-8); '
                     'the donor had no AI to apply it to'),
    'Fix nofriendlyfire flag not working correctly on easy skill':
        ('re-apply', 'Same site as the above; skill levels are live under '
                     'g_ruleset sp (R-SP-1)'),
    'Remove commented out monster code':
        ('not-applicable', 'A cleanup of dead code in files the donor deleted '
                           'outright. Colosseum keeps the files; the comment '
                           'removal is cosmetic and carries no behaviour'),
    'Make more game definitions static (monsters)':
        ('re-apply', 'But subject to R-CORE-13: static survives only where NO '
                     'donor calls across the file boundary, so this lands '
                     'through staticize.py, not verbatim'),
    'Use initializer for aim vector':
        ('re-apply', 'g_weapon.c/monster fire paths are live here'),

    # --- savegame / single player.  Live under g_ruleset sp.
    'Avoid generating missing savegame pointer':
        ('re-apply', 'Savegames are in scope for sp (R-SAVE-1..5) and g_ptrs.c '
                     'is generated here (R-SAVE-2)'),
    'Fix annoying FOV change when exiting SP level':
        ('re-apply', 'SP level exit exists here (R-SP-1)'),
    'Remove power cubes at end of unit':
        ('re-apply', 'Unit transitions are live in the baseq2 campaign'),
    'Re-introduce savegame and loadgame menus':
        ('re-apply', 'R-VER-15 item 9 and the sp ruleset need them; note the '
                     'menu engine is per-ruleset (R-MENU-1), so this lands in '
                     'the dm/sp tree engine'),
    "Use 'menu_loadgame' inside baseq2 game library":
        ('re-apply', 'Same site as the above'),

    # --- client / protocol / hygiene.  Live everywhere.
    'Fix client disconnect in ss_pic state':
        ('re-apply', 'Client state machine is shared by every ruleset'),
    'Fix out of array access':
        ('re-apply', 'R-SEC-4 makes this mandatory rather than optional'),
    'Reduce status bar string duplication':
        ('superseded', 'R-OSP-7a replaces stored bar strings with a composed '
                       'emitter, which subsumes the de-duplication entirely'),
    'Replace strstr() with strchr()':
        ('re-apply', 'Trivial and tree-wide'),
    "Require C99 semantics for 'bool'":
        ('re-apply', 'Load-bearing: R-SAVE-3a depends on bool being C99, and '
                     "tourney's pers.spectator is deliberately int because a "
                     'C99 bool would saturate its >= 3 test'),
    # norm() folds curly quotes to straight ones, so the key carries them.
    "Fix broken viewangles with 'spectator 1'":
        ('superseded', 'RA2 applied it later as its own commit -- §3.1 names '
                       'that commit by name. Verify it is present in '
                       'rocketarena2-public; do not re-apply on top'),

    # Found by this tool, 2026-08-21, and absent from R-CORE-12's enumerated
    # sets: the spec listed skips for RA2, tourney and CTF but recorded none for
    # rogue.  Rogue skipped one.
    'Properly check inuse flag in blocked functions':
        ('re-apply', 'A use-after-free guard on freed edicts in the blocked() '
                     'paths of g_func.c and the monster movers. Colosseum keeps '
                     'both (R-CORE-8), and R-SEC-4 makes the check mandatory '
                     'rather than discretionary'),
    'Fix crashes due to G_PickTarget() returning NULL':
        ('re-apply', 'R-SEC-4; G_PickTarget is in g_utils.c, which is live'),
}


def git(*args):
    r = subprocess.run(['git', '-C', CACHE] + list(args),
                       capture_output=True, text=True)
    if r.returncode:
        sys.exit(f'coverage.py: git {" ".join(args)} failed:\n{r.stderr.strip()}')
    return r.stdout


def trailers(ref):
    """{q2pro sha: subject} for every commit on ref carrying the trailer."""
    out = {}
    log = git('log', '--format=%x01%s%x02%b', ref)
    for rec in log.split('\x01'):
        if not rec.strip():
            continue
        subject, _, body = rec.partition('\x02')
        for m in re.finditer(r'^q2pro-commit:\s*([0-9a-f]{7,40})\s*$',
                             body, re.M):
            out[m.group(1)] = subject.strip()
    return out


def norm(subject):
    """Match VERDICTS keys tolerantly: trailing period, curly quotes."""
    s = subject.strip().rstrip('.')
    return (s.replace('‘', "'").replace('’', "'")
             .replace('“', '"').replace('”', '"'))


def build():
    spine = trailers(SPINE)
    rows = []
    per_donor = {}
    for label, ref in DONORS:
        got = trailers(ref)
        missing = [(sha, subj) for sha, subj in spine.items() if sha not in got]
        per_donor[label] = (len(got), len(missing))
        for sha, subj in missing:
            v = VERDICTS.get(norm(subj))
            landed = LANDED.get((label, norm(subj)))
            why = v[1] if v else ''
            if landed:
                why = f'{why}. **{landed}**' if why else f'**{landed}**'
            rows.append({'donor': label, 'sha': sha, 'subject': subj,
                         'verdict': v[0] if v else 'TODO',
                         'landed': bool(landed), 'why': why})
    return spine, per_donor, rows


def render(spine, per_donor, rows):
    todo = [r for r in rows if r['verdict'] == 'TODO']
    w = ['# Replay coverage ledger (D9)',
         '',
         'Generated by `tools/coverage.py` from the `q2pro-commit:` trailers in',
         'the bundles vendored under `vendor/replay/` (R-PROV-1). One row per',
         '(donor, skipped commit), with a verdict, per R-CORE-12. R-VER-10 makes',
         'a row without a verdict block the phase that imports that donor.',
         '',
         f'Spine: **{len(spine)}** Q2PRO game commits carry a trailer on',
         '`baseq2`.',
         '',
         '| donor | replayed | skipped | phase that imports it |',
         '|---|---:|---:|---|']
    phase = {'ctf': 'Phase 3', 'xatrix': 'Phase 2', 'rogue': 'Phase 2',
             'ra2': 'Phase 4', 'osp': 'Phase 5'}
    for label, _ in DONORS:
        got, miss = per_donor[label]
        w.append(f'| {label} | {got} | {miss} | {phase[label]} |')

    w += ['',
          '## Rows',
          '',
          'A commit skipped by more than one donor appears once per donor,',
          'because the verdict is about that donor\'s import.',
          '',
          '| donor | q2pro commit | subject | verdict | why |',
          '|---|---|---|---|---|']
    order = {'re-apply': 0, 'superseded': 1, 'not-applicable': 2, 'TODO': -1}
    for r in sorted(rows, key=lambda r: (order[r['verdict']], r['donor'],
                                         r['subject'])):
        v = r['verdict']
        mark = f'**{v}**' if v in ('re-apply', 'TODO') else v
        if r.get('landed'):
            mark = f'{v} — done'
        w.append(f'| {r["donor"]} | `{r["sha"][:12]}` | {r["subject"]} '
                 f'| {mark} | {r["why"]} |')

    counts = {}
    for r in rows:
        counts[r['verdict']] = counts.get(r['verdict'], 0) + 1
    done = sum(1 for r in rows if r.get('landed'))
    w += ['',
          '## Summary',
          '',
          f'{len(rows)} rows: '
          + ', '.join(f'{n} {k}' for k, n in sorted(counts.items()))
          + f'. {done} of the {counts.get("re-apply", 0)} re-apply rows are '
            f'landed and recorded in LANDED.',
          '']
    if todo:
        w += [f'**{len(todo)} row(s) have no verdict and block their phase '
              f'(R-VER-10).**', '']
    else:
        w += ['Every row has a verdict, which is the Phase 0 exit condition for',
              'D9. The `re-apply` rows are the work R-CORE-12 predicted: each',
              'needs a commit that lands it, and R-VER-10 re-runs this check',
              'after each donor import.', '']
    return '\n'.join(w)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--md', metavar='PATH')
    ap.add_argument('--check', action='store_true')
    a = ap.parse_args()

    spine, per_donor, rows = build()
    todo = [r for r in rows if r['verdict'] == 'TODO']

    if a.check:
        for r in todo:
            print(f'  no verdict: {r["donor"]} {r["sha"][:12]} {r["subject"]}')
        print(f'coverage.py: {len(rows)} rows, {len(todo)} without a verdict')
        return 1 if todo else 0

    text = render(spine, per_donor, rows)
    if a.md:
        with open(a.md, 'w', encoding='utf-8') as f:
            f.write(text)
        print(f'wrote {a.md}  ({len(rows)} rows, {len(todo)} without a verdict)')
    else:
        sys.stdout.write(text)
    return 1 if todo else 0


if __name__ == '__main__':
    sys.exit(main())
