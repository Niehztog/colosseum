#!/usr/bin/env python3
"""Derive the replay-coverage ledger.

No donor received all 188 Q2PRO game commits, and because Colosseum
keeps the monsters and the campaigns the donors dropped, a commit a donor
skipped for want of a site usually *has* a site here.  this ledger is one row per
(donor, skipped commit) with a verdict: *re-apply*, *not applicable*, or
*superseded*.  A row with no verdict blocks the import that
imports that donor.

METHOD.  Every replayed commit on a `port_<donor>` branch carries a
`q2pro-commit: <sha>` trailer naming the upstream Q2PRO commit it replays.  The
`baseq2` spine branch carries the same trailers for all 188.  A commit whose
trailer appears on the spine but not on a donor's branch was skipped by that
donor.  Nothing here is inferred from the diff -- the trailer is the record.

The verdicts themselves are judgement and live in VERDICTS below, keyed by
commit subject.  A subject with no entry is emitted as `TODO` and makes this
tool exit non-zero: the ledger cannot silently
be incomplete.

USAGE
    tools/coverage.py                       # markdown to stdout
    tools/coverage.py --md coverage.md      # write it out instead
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
# The rule that decides most of these: this tree keeps every monster and both
# campaigns, so a fix the donor skipped "for want of a site" has a site here.
# The other half: "every *re-apply* verdict has a commit that lands it."
# A verdict is a judgement about a (donor, commit) pair; LANDED records that the
# pair has actually been discharged, keyed the same way, because a verdict alone
# does not say whether anyone acted on it.
#
# Keyed by (donor, normalised subject) so the same commit can be landed for one
# donor and still outstanding for another -- which is the normal case, since a
# fix skipped by three donors is discharged once per import.
LANDED = {
    ('ctf', 'Avoid generating missing savegame pointer'):
        'landed: m_flyer.c carries no commented `&flyer_move_attack1` '
        'and check-ptrs passes',
    ('ctf', 'Fix crashes due to G_PickTarget() returning NULL'):
        'landed -- and it found a THIRD call site. The two the '
        'commit names are guarded; turret_brain_link is Ground Zero\'s own '
        'addition, and dereferenced the result unguarded. '
        'Re-applying a fix means re-asking its '
        'question of the merged tree, not confirming its hunks survived',
    ('ctf', 'Use initializer for aim vector'):
        'landed: five `vec3_t aim = { ... }` initialisers across '
        'm_berserk.c and m_brain.c',

    # THE OTHER EIGHTEEN, discharged by the osp and ra2 imports and by the
    # rogue one, and recorded here a release late.  They were landed in the
    # tree and not in this table, which is the failure mode a LANDED table
    # exists to prevent: the ledger read `21 re-apply, 3 landed` and so
    # announced eighteen upstream fixes still owed by a tree that already had
    # every one of them.  Each row below names the site it was read at, so the
    # claim is checkable rather than asserted -- `grep` the note.
    #
    # A fix skipped by more than one donor is one piece of work and several
    # rows: `Use initializer for aim vector` is ctf's, osp's and ra2's, and
    # lands once.  The note says so rather than pretending to three edits.
    ('osp', 'Avoid generating missing savegame pointer'):
        'landed with ctf: `make check-ptrs` passes against a regenerated '
        'g_ptrs.c, and no mmove_t row in the tree carries a commented pointer',
    ('ra2', 'Avoid generating missing savegame pointer'):
        'landed with ctf: same regenerated g_ptrs.c, same check-ptrs',
    ('osp', 'Fix annoying FOV change when exiting SP level'):
        'landed: g_main.c carries the SECOND `if (level.exitintermission) '
        '{ ExitLevel(); return; }`, below the G_RunEntity loop as well as '
        'above it -- the later exit is the whole of the fix',
    ('osp', 'Fix client disconnect in ss_pic state'):
        'landed: ClientDisconnect guards the MZ_LOGOUT muzzleflash with '
        '`if (ent->inuse)` (p_client.c)',
    ('osp', 'Fix nofriendlyfire being applied to AI in coop'):
        'landed: T_Damage tests `(coop->value && targ->client)` (g_combat.c), '
        'so the coop arm no longer reaches a monster',
    ('ra2', 'Fix nofriendlyfire being applied to AI in coop'):
        'landed with osp: one site in T_Damage, shared by both',
    ('osp', 'Fix nofriendlyfire flag not working correctly on easy skill'):
        'landed: the easy-mode halving sits ABOVE the friendly-fire block in '
        'T_Damage and above `meansOfDeath = mod`, which is the move the '
        'commit is -- the value it halves has to be the one the block reads',
    ('ra2', 'Fix nofriendlyfire flag not working correctly on easy skill'):
        'landed with osp: the same statement in the same T_Damage',
    ('osp', 'Fix out of array access'):
        'landed: BeginIntermission strips keys over `n < game.num_items`, '
        'not MAX_ITEMS (p_hud.c)',
    ('osp', 'Re-introduce savegame and loadgame menus'):
        'landed as the PAIR it belongs to: this commit spells respawn()\'s '
        'restart `pushmenu loadgame` and its successor `Use menu_loadgame '
        'inside baseq2 game library` spells it back, so the tree carrying '
        '`menu_loadgame` (p_client.c) is both of them, at the later of the '
        'two spellings',
    ('osp', 'Use \'menu_loadgame\' inside baseq2 game library'):
        'landed: respawn() calls `menu_loadgame` (p_client.c) -- the same '
        'line as the row above, which is why they discharge together',
    ('osp', 'Remove power cubes at end of unit'):
        'landed: BeginIntermission clears `pers.power_cubes` beside the key '
        'strip (p_hud.c)',
    ('osp', 'Replace strstr() with strchr()'):
        'landed: BeginIntermission tests `strchr(level.changemap, \'*\')` '
        '(p_hud.c)',
    ('osp', 'Require C99 semantics for \'bool\''):
        'landed: Pickup_Ammo assigns `weapon = (ent->item->flags & '
        'IT_WEAPON)` with no `!!` (g_items.c), which is only correct under '
        'C99 bool -- and R-SEC-8 turns the same retype into a rule',
    ('osp', 'Use initializer for aim vector'):
        'landed with ctf: the same five initialisers in m_berserk.c and '
        'm_brain.c',
    ('ra2', 'Use initializer for aim vector'):
        'landed with ctf: same five',
    ('ra2', 'Make more game definitions static (monsters)'):
        'landed through staticize.py rather than verbatim, as the verdict '
        'said it must: m_actor.c\'s actorMachineGun, actor_dead and '
        'actor_fire are static, and R-VER-12\'s linkage audit is what keeps '
        'the ones a donor calls across a file boundary from joining them',
    ('rogue', 'Properly check inuse flag in blocked functions'):
        'landed, and re-asking found THREE more sites than the commit has. '
        'It names four blocked handlers; this tree has seven, because Ground '
        'Zero brought plat2_blocked, smart_water_blocked and '
        'button_rotating_blocked. All seven guard BecomeExplosion1 on '
        '`other->inuse` (g_func.c) -- the last spelled without the redundant '
        'null test, with the reason at the site',
}

VERDICTS = {
    # --- monster / AI fixes.  Three donors deleted the monster set, so these
    # had nowhere to land there.  Colosseum keeps all 31 monster TUs, so every
    # one of them has a site and must be re-applied.
    'Fix nofriendlyfire being applied to AI in coop':
        ('re-apply', 'Coop + monsters both exist here; '
                     'the donor had no AI to apply it to'),
    'Fix nofriendlyfire flag not working correctly on easy skill':
        ('re-apply', 'Same site as the above; skill levels are live under '
                     'g_ruleset sp'),
    'Remove commented out monster code':
        ('not-applicable', 'A cleanup of dead code in files the donor deleted '
                           'outright. Colosseum keeps the files; the comment '
                           'removal is cosmetic and carries no behaviour'),
    'Make more game definitions static (monsters)':
        ('re-apply', 'But subject to the linkage rule: static survives only where NO '
                     'donor calls across the file boundary, so this lands '
                     'through staticize.py, not verbatim'),
    'Use initializer for aim vector':
        ('re-apply', 'g_weapon.c/monster fire paths are live here'),

    # --- savegame / single player.  Live under g_ruleset sp.
    'Avoid generating missing savegame pointer':
        ('re-apply', 'Savegames are in scope for sp and g_ptrs.c '
                     'is generated here'),
    'Fix annoying FOV change when exiting SP level':
        ('re-apply', 'SP level exit exists here'),
    'Remove power cubes at end of unit':
        ('re-apply', 'Unit transitions are live in the baseq2 campaign'),
    'Re-introduce savegame and loadgame menus':
        ('re-apply', 'The sp ruleset needs them; note the '
                     'menu engine is per-ruleset, so this lands in '
                     'the dm/sp tree engine'),
    "Use 'menu_loadgame' inside baseq2 game library":
        ('re-apply', 'Same site as the above'),

    # --- client / protocol / hygiene.  Live everywhere.
    'Fix client disconnect in ss_pic state':
        ('re-apply', 'Client state machine is shared by every ruleset'),
    'Fix out of array access':
        ('re-apply', 'The bounds rule makes this mandatory rather than optional'),
    'Reduce status bar string duplication':
        ('superseded', 'The composed statusbar replaces stored bar strings with a '
                       'emitter, which subsumes the de-duplication entirely'),
    'Replace strstr() with strchr()':
        ('re-apply', 'Trivial and tree-wide'),
    "Require C99 semantics for 'bool'":
        ('re-apply', 'Load-bearing: the savegame descriptors depend on bool being C99, and '
                     "tourney's pers.spectator is deliberately int because a "
                     'C99 bool would saturate its >= 3 test'),
    # norm() folds curly quotes to straight ones, so the key carries them.
    "Fix broken viewangles with 'spectator 1'":
        ('superseded', 'RA2 applied it later as its own commit -- section 3.1 names '
                       'that commit by name. Verify it is present in '
                       'rocketarena2; do not re-apply on top'),

    # Found by this tool and absent from the enumerated
    # sets: the spec listed skips for RA2, tourney and CTF but recorded none for
    # rogue.  Rogue skipped one.
    'Properly check inuse flag in blocked functions':
        ('re-apply', 'A use-after-free guard on freed edicts in the blocked() '
                     'paths of g_func.c and the monster movers. Colosseum keeps '
                     'both, and the bounds rule makes the check mandatory '
                     'rather than discretionary'),
    'Fix crashes due to G_PickTarget() returning NULL':
        ('re-apply', 'Mandatory; G_PickTarget is in g_utils.c, which is live'),
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


def ascii_text(s):
    """An upstream subject is spelled in ASCII when it is rendered.

    The subjects come from other people's commit messages and carry curly
    quotes and dashes; the documents in this tree are ASCII, and a generated
    one has to stay that way without anybody editing it afterwards.
    """
    for a, b in (('\u2018', "'"), ('\u2019', "'"), ('\u201c', '"'),
                 ('\u201d', '"'), ('\u2014', '--'), ('\u2013', '-'),
                 ('\u2026', '...'), ('\u00a0', ' ')):
        s = s.replace(a, b)
    return s


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
    w = ['# Replay coverage ledger',
         '',
         'Generated by `tools/coverage.py` from the `q2pro-commit:` trailers in'
         ' the bundles vendored under `vendor/replay/`. One row per'
         ' (donor, skipped commit), with a verdict. A row without a verdict'
         ' blocks that donor\'s import.',
         '',
         f'Spine: **{len(spine)}** Q2PRO game commits carry a trailer on'
         ' `baseq2`.',
         '',
         '| donor | replayed | skipped |',
         '|---|---:|---:|']
    for label, _ in DONORS:
        got, miss = per_donor[label]
        w.append(f'| {label} | {got} | {miss} |')

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
            mark = f'{v} -- done'
        w.append(f'| {r["donor"]} | `{r["sha"][:12]}` | {ascii_text(r["subject"])} '
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
        w += [f'**{len(todo)} row(s) have no verdict and block their import '
              f'.**', '']
    else:
        w += ['Every row has a verdict, which is the condition for this ledger.'
              ' The `re-apply` rows are the predicted work: each needs a commit'
              ' that lands it, and this check re-runs after each donor import.', '']
        # A ledger that says "each needs a commit" while every row already has
        # one is describing a tree it has stopped reading.  It said exactly
        # that for eighteen rows through one release, so the state is now
        # stated rather than left to be inferred from two numbers above.
        missing = [r for r in rows
                   if r['verdict'] == 're-apply' and not r.get('landed')]
        if missing:
            w += [f'**{len(missing)} of them do not yet: '
                  + ', '.join(f'{r["donor"]} `{norm(r["subject"])}`'
                              for r in missing) + '.**', '']
        else:
            w += ['**All of them have one.** Nothing here is outstanding; the'
                  ' next donor import is what can make it so again.', '']
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
