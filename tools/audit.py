#!/usr/bin/env python3
"""Run the contract audits and fail the build on a finding.

"The four imported audits (`auditsave`, `slotkind`, `keycontract`,
`auditems`) run in the build, not on request.  A new finding fails the build the
way a warning does."

It is more than four now.  Those four were the imported harness's; every
phase since has added the check its own defect asked for, and each is registered
below with the requirement it discharges.  `auditsave` is the one that left:
`dsweep.py` asks its question better.  The count is not fixed here and is not
worth publishing from memory -- this driver prints it, and a document that wants
the figure should quote that line.

The tools were written as *reports*: they print what they find and exit 0
either way, because in the replay harness a human read the output.  A build
cannot read.  This driver runs each one, scans its output for the markers the
tool uses to signal a finding, and exits non-zero if any fired.

It also handles the case the harness never had: a tree with no donor in it.
`auditsave` and `auditems` are *comparative* -- they ask what a donor added on
top of baseq2 -- so before a donor lands they have nothing to compare
and must report "vacuous", not "clean".  Recording a vacuous pass as a pass is
how a check rots, so
they are labelled explicitly.

USAGE
    tools/audit.py [--tree src] [--donor LABEL=PATH ...] [-v]
"""
import argparse
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(REPO, 'tools')

# Each tool signals a finding differently, and matching on loose substrings is
# how a driver reports a failure on the line that says there were none: both
# `slotkind` and `auditems` print their own counts, so "kind mismatch" appears
# in "0 kind mismatch(es)".  Detection is therefore per tool, anchored on the
# marker each one actually uses for a finding rather than on its prose.
#
#   keycontract  prefixes each finding with '!!'
#   slotkind     prefixes each finding with '!!'
#   auditems     prints 'TOTAL differing fields across shared items: N'
#
# `auditsave.py` is not here and is not run: the savegame question is asked by
# `dsweep.py`, which brace-matches the struct bodies instead of regexing them
# and carries its own extractor self-test.  See the note at the dsweep call.


def _bang(out):
    return [ln for ln in out.splitlines() if '!!' in ln]


# This asks whether the merge lost a baseq2 field.  A merged union
# tree also shows the donors' legitimate changes, and those are not findings --
# Exactly two were measured and named.  Recording them
# here is what lets the check run in the build at all: pointed at baseq2 with no
# baseline it fails forever on correct code, and skipped entirely it reports
# "not applicable" while three donors sit in the tree, which is the vacuous pass
# this driver exists to prevent.
#
# A THIRD difference fails the build.  That is the whole point: the check is
# now sensitive to the next lost field rather than saturated by these two.
ACCEPTED_ITEM_DIFFS = {
    # (item, field): why
    ('weapon_hyperblaster', 'use'):
        "Xatrix's Use_Weapon2 -- the Ionripper shares the slot",
    ('weapon_railgun', 'use'):
        "Xatrix's Use_Weapon2 -- the Phalanx shares the slot",
}


def _auditems(out):
    m = re.search(r'TOTAL differing fields across shared items:\s*(\d+)', out)
    if not m:
        return _bang(out)
    hits = []
    for mm in re.finditer(r'^\s*"([^"]+)"\.(\w+)\s*$', out, re.M):
        key = (mm.group(1), mm.group(2))
        if key not in ACCEPTED_ITEM_DIFFS:
            hits.append(f'  !! {key[0]}.{key[1]} differs from baseq2 and is not '
                        f'an accepted difference')
    return hits


def _gates(out):
    # gates.py already exits non-zero and prefixes findings with '!!'
    return _bang(out)


PARSERS = {
    'gates.py': _gates,
    'arenaspawn.py': _bang,
    'units.py': _bang,
    'allocpairs.py': _bang,
    'lostref.py': _bang,
    'donorgate.py': _bang,
    'encoding.py': _bang,
    'bounded.py': _bang,
    'externs.py': _bang,
    'noexec.py': _bang,
    'nullattacker.py': _bang,
    'botabi.py': _bang,
    'dsweep.py': _bang,
    'keycontract.py': _bang,
    'slotkind.py': _bang,
    'dupvalue.py': _bang,
    'itemnames.py': _bang,
    'classnames.py': _bang,
    'deadvalue.py': _bang,
    'dispatch.py': _bang,
    'fnsweep.py': _bang,
    'counts.py': _bang,
    'assets.py': _bang,
    'engineapi.py': _bang,
    'auditems.py': _auditems,
}


def run(name, args, label):
    cmd = [sys.executable, os.path.join(TOOLS, name)] + args
    p = subprocess.run(cmd, capture_output=True, text=True)
    out = (p.stdout or '') + (p.stderr or '')
    hits = PARSERS[name](out)
    return {
        'name': name, 'label': label, 'rc': p.returncode,
        'out': out, 'hits': hits,
        # A crash is a finding too: an audit that cannot run is not a pass.
        'broken': p.returncode != 0,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tree', default=os.path.join(REPO, 'src'))
    ap.add_argument('--donor', action='append', default=[],
                    metavar='LABEL=PATH',
                    help='a donor tree to compare against --tree; repeatable')
    ap.add_argument('-v', '--verbose', action='store_true')
    a = ap.parse_args()

    tree = os.path.abspath(a.tree)
    donors = [d.split('=', 1) for d in a.donor if '=' in d]
    # A donor path outside the repository is a convenience, not a dependency:
    # This measures src/ against q2pro/src/game, but a tree without q2pro
    # beside it must still build.  Absent means skipped-and-said-so.
    donors = [(l, p) for l, p in donors if os.path.isdir(p)]

    results, vacuous = [], []
    # The same donor list, in the form the comparative single-tool audits take.
    dflags = [f'--donor={l}={p}' for l, p in donors]

    # --- single-tree audits: meaningful with no donor present ------------
    # keycontract: statusbar slot vs STAT_ macro vs spawn key vs descriptor type
    results.append(run('keycontract.py', [f'colosseum={tree}'], 'keycontract'))
    # slotkind: does a bar read a slot as a kind the code does not write, and
    # was the slot available to be claimed at all?
    results.append(run('slotkind.py', [f'colosseum={tree}'], 'slotkind'))
    # ...and slotkind's own controls, five mutations of the real map and emitter.
    # A check that has never failed is not trusted, so the
    # controls run in the build beside the check rather than in a document.
    results.append(run('slotkind.py', ['--selftest'], 'slotkind/controls'))
    # gates: every ruleset decision goes through the dispatch or a predicate,
    # and the monster-suppression idiom has not come back.
    results.append(run('gates.py', ['--tree', tree], 'gates'))
    # arenaspawn: fighter-only ranking must still reject an occupied,
    # same-arena solid pad before either arena selector accepts it.
    results.append(run('arenaspawn.py', ['--tree', tree], 'arenaspawn'))
    results.append(run('arenaspawn.py', ['--selftest', '--tree', tree],
                       'arenaspawn/controls'))
    # dupvalue: two names for one POSITION in a list something outside this tree
    # walks.  Each donor numbered its own extra weapons from 12 because
    # each ships a precache block with nothing after the BFG; the merge unions the
    # content into ONE ordered block, and three private 12s made seven weapons
    # draw somebody else's model.  Checks the values and, separately, that
    # WEAP_* still agrees with the block it indexes -- a family can be
    # internally consistent and still disagree with its list.
    results.append(run('dupvalue.py', ['--tree', tree], 'dupvalue'))
    results.append(run('dupvalue.py', ['--selftest'], 'dupvalue/controls'))
    # units: the timer-unit contract.  q2pro's frame-number conversion
    # left a field's type saying nothing about its unit, and a merged tree has a
    # failure mode the separate packs cannot: one g_local.h means one declaration
    # wins and every site written against the other is now a mix.
    results.append(run('units.py', ['--tree', tree], 'units'))
    results.append(run('units.py', ['--selftest'], 'units/controls'))
    # allocpairs: the allocator-pair contract.  A pointer must be
    # released by the family that produced it, and this tree has two -- libc and
    # the engine's tagged blocks.  Not a libc ban: g_main.c's strdup/free pair is
    # correct and is upstream's, so the unit that has to agree is the pointer
    # families.
    results.append(run('allocpairs.py', ['--tree', tree], 'allocpairs'))
    results.append(run('allocpairs.py', ['--selftest'], 'allocpairs/controls'))
    # lostref: three donors delete the monster set
    # and Colosseum replays none of those deletions -- but RA2 deletes monster
    # code from inside the SHARED files too, where the five merge rules cannot
    # tell a feature deletion from that one and the compiler sees nothing
    # because the callee still exists.
    results.append(run('lostref.py', ['--tree', tree], 'lostref'))
    results.append(run('lostref.py', ['--selftest'], 'lostref/controls'))
    # donorgate: a donor's own fields and functions, used in a SHARED file, must
    # be inside that donor's gate.  gates.py asks whether a ruleset is
    # chosen by testing a cvar; this asks whether one donor's state is read under
    # another donor's ruleset, which is what six sites did.
    results.append(run('donorgate.py', ['--tree', tree], 'donorgate'))
    results.append(run('donorgate.py', ['--selftest'], 'donorgate/controls'))
    # bounded: no unbounded string copy or raw fscanf string scan in src/
    #. The config parser must reject an overlong token
    # rather than treat a bounded fscanf tail as the next valid token.
    # A ban rather than a reachability judgement, because reachability is what a
    # reviewer gets wrong: `sprintf(entry, "yv %d ", y)` reads as arithmetic
    # until somebody adds a %s to it, and the netname three lines down was
    # always client-controlled.
    results.append(run('bounded.py', ['--tree', tree], 'bounded'))
    results.append(run('bounded.py', ['--selftest'], 'bounded/controls'))
    # externs: a .c may not declare what another .c defines, and where the tree
    # still does (baseq2's 150-row spawn table, mostly), the declaration must
    # agree with the definition.  Its first run found
    # Ground Zero's `Move_Calc` prototype one qualifier out.
    results.append(run('externs.py', ['--tree', tree], 'externs'))
    results.append(run('externs.py', ['--selftest'], 'externs/controls'))
    # noexec: no socket, no process, no exit().  RA2 shipped
    # a UDP event forwarder whose three helpers each called exit(1) from inside
    # a game library; it is gone, and this is what keeps it gone.
    results.append(run('noexec.py', ['--tree', tree], 'noexec'))
    results.append(run('noexec.py', ['--selftest'], 'noexec/controls'))
    # nullattacker: T_Damage normalises a NULL attacker before anything reads
    # one.  A one-line invariant with twenty silent beneficiaries -- delete it
    # and the tree builds, boots and passes every other audit, then dies the
    # first time a blocked door fires a target_explosion.  id's own code
    # survives the same NULL by accident (its single read sits behind
    # !(dflags & DAMAGE_RADIUS)); six donors' arms merged into one function is
    # what spends that accident.  And the use path, which the donors guard at
    # the read rather than normalise: every `use` callback and every pointer
    # one hands on is swept for a dereference no NULL test covers.  R-SEC-10.
    results.append(run('nullattacker.py', ['--tree', tree], 'nullattacker'))
    results.append(run('nullattacker.py', ['--selftest'], 'nullattacker/controls'))
    # encoding: every source file is valid UTF-8.  Trivial, and it has bitten
    # twice -- grep in a UTF-8 locale returns NOTHING for a file it cannot
    # decode, so a census can silently report zero.
    results.append(run('encoding.py', ['--tree', tree], 'encoding'))
    # itemnames: does every literal FindItem()/FindItemByClassname() in the tree
    # resolve to a row the itemlist actually has?  A merged union
    # itemlist is where a name goes stale -- six donors' items in one list, so a
    # donor's own spelling can be renamed by the union and a name one donor
    # invented for another's item never resolved at all -- and the failure is
    # silent: NULL assigned is a weapon that is never selected, NULL indexed
    # through ITEM_INDEX is a negative subscript into pers.inventory[].
    results.append(run('itemnames.py', ['--tree', tree], 'itemnames'))
    results.append(run('itemnames.py', ['--selftest'], 'itemnames/controls'))
    # classnames: the SECOND resolver, and it exists because the first one's
    # question is narrower than it looks.  itemnames.py resolves
    # classname-SHAPED literals against the ITEMLIST; a literal compared against
    # an entity's classname may name a monster, a projectile or something a
    # runtime assignment invented, and it need not be shaped like an item at
    # all.  Both narrowings applied to `"telsa"` in m_move.c, Ground Zero's own
    # typo, which meant a branch of the blocked-by-tesla chain never ran once in
    # twenty-seven years.  So this asks whether ANY code path can put
    # the string in an edict_t.classname -- the spawn table, the itemlist, or a
    # `->classname =` assignment -- because a literal none of the three produces
    # is a comparison that cannot succeed.
    results.append(run('classnames.py', ['--tree', tree], 'classnames'))
    results.append(run('classnames.py', ['--selftest'], 'classnames/controls'))
    # counts: the acceptance-criteria surfaces, and the duplicate
    # check inside them.  `docs/cvars.md` publishes counts.py's numbers and cites
    # its `--duplicates` as evidence, and for a while nothing re-ran it -- a
    # figure in a document with no build check behind it is the
    # "indicative only" no matter which script first produced it.  --duplicates
    # exits non-zero on a real collision, which is what makes it an audit.
    results.append(run('counts.py', ['--tree', tree, '--duplicates'], 'counts'))
    results.append(run('counts.py', ['--selftest'], 'counts/controls'))
    # assets: does every literal .md2/.sp2/.wav name a file some donor actually
    # ships?  itemnames.py asks this of the itemlist; this asks it of the
    # DATA, which is the other half of the same namespace and fails even more
    # quietly -- gi.soundindex() cannot check, so a weapon with a misspelled
    # sound is silent and looks like a design choice.  The game data is not in
    # the repository, so this one SKIPS rather than passes when it is absent.
    results.append(run('assets.py', ['--tree', tree], 'assets'))
    results.append(run('assets.py', ['--selftest'], 'assets/controls'))
    # deadvalue: the third space.  A field compared against a
    # value nothing in the tree writes is a branch that cannot be taken, and the
    # merge produces them by keeping a reader and dropping the write -- the
    # `osp_r240 == 2`, `pers.showmotd`, the five-page scoreboard
    # whose fifth page nothing could open.  The in-tree half needs no donor; the
    # sharper half compares against each one below.
    results.append(run('deadvalue.py', ['--tree', tree] + dflags, 'deadvalue'))
    results.append(run('deadvalue.py', ['--selftest'], 'deadvalue/controls'))
    # dispatch: is every ruleset_ops_t row actually TRAVERSED?  The
    # sibling of deadvalue one indirection up: that one asks whether a field can
    # hold the value a branch tests for, this one whether a FILLED ROW is ever
    # reached.  A table initialiser is a reference, so `.EndLevel = RA_EndLevel`
    # makes the override look live to every other instrument here while the
    # dispatcher that would call it is reachable only under another ruleset.
    # Three defects of that shape shipped -- the CTF scoreboard, the
    # arena rotation, the tourney map votes -- and all three were found by
    # playing the game.  Pointed at the tree as it stood at `e22dcdb` it names
    # eleven sites; the controls reproduce each of the three.
    results.append(run('dispatch.py', ['--tree', tree], 'dispatch'))
    results.append(run('dispatch.py', ['--selftest'], 'dispatch/controls'))
    # fnsweep: the function-level donor diff.  divergence.py states
    # the merge load per FILE in diff lines, which cannot say whether a donor's
    # change to a particular definition survived; this walks each donor's own
    # feature set definition by definition and asks.  Its finding is narrow on
    # purpose -- a donor line whose identifiers exist NOWHERE in src/ and that
    # no recorded decision explains -- because a line missing from its own site
    # is the normal case for a gated merge and 1,168 of them are elsewhere in the
    # tree verbatim. docs/donor-fdiff.md is the read-through, which records the
    # historical findings; the non-policy paths are closed.
    results.append(run('fnsweep.py', ['--check', '--tree', tree], 'fnsweep'))
    results.append(run('fnsweep.py', ['--selftest'], 'fnsweep/controls'))
    # botabi: the game<->botlib contract against the BRAIN's own header
    #.  The Trace slot has two spellings and they are the same ABI at
    # 32 bits and different ABIs at 64, so taking the 32-bit one on aarch64
    # compiles, links, loads the library, spawns the bots and plays nothing
    #.  Skips itself with a message when the brain
    # is not beside the repository, the same rule auditems applies to q2pro.
    results.append(run('botabi.py', [], 'botabi'))
    results.append(run('botabi.py', ['--selftest'], 'botabi/controls'))

    # engineapi: R-ENG-3's table of the extended engine API against the tree.
    # Taking an entry from `game_import_ex_t` is one line that nothing else
    # depends on, so a paragraph can claim a capability the library does not
    # take and no build, boot or play test notices -- R-ENG-3 claimed
    # `get_configstring` and `local_sound` while `gex` was dereferenced in
    # g_fs.c and nowhere else.  Both sides are read from the tree: the members
    # from inc/shared/gameext.h, the call sites from src/.
    results.append(run('engineapi.py', ['--tree', tree], 'engineapi'))
    results.append(run('engineapi.py', ['--selftest'], 'engineapi/controls'))

    # --- comparative audits: need a donor ---------------------------------
    # dsweep, not auditsave: both implement "a persistent field with
    # no descriptor", but auditsave's struct_body regex misattributes members --
    # asked about the mission-pack merge it reported spawn_temp_t's `pausetime`,
    # `minyaw` and `height` as monsterinfo_t fields.  dsweep brace-matches the
    # struct bodies and carries its own extractor self-test.  Where a
    # tool and a reviewer disagree, fix the tool.
    results.append(run('dsweep.py', [], 'dsweep/descriptors'))
    if donors:
        for label, path in donors:
            # This compares against q2pro's baseq2 entries, not
            # against the donor: the regression it names is a baseq2 field lost
            # in the merge (a railgun precache, dropped by a conflict
            # resolution).  Pointed at a donor instead, a merged union tree
            # always reports the OTHER donor's legitimate changes -- Xatrix's
            # Use_Weapon2 on the hyperblaster and railgun, and the items Ground
            # Zero's KILL_DISRUPTOR branch marks IT_NOT_GIVEABLE -- which are
            # correct and would fail the build forever.
            base_items = os.path.join(tree, 'g_items.c')
            var_items = os.path.join(path, 'g_items.c')
            if os.path.exists(var_items):
                r = run('auditems.py', [base_items, var_items],
                        f'auditems/{label}')
                # auditems exits 0 whatever it finds, and the accepted-difference
                # baseline above decides; a non-zero rc here would be a crash.
                results.append(r)
            else:
                vacuous.append(f'auditems/{label}: {var_items} absent')
    else:
        vacuous.append(
            'auditems: the reference tree (q2pro/src/game) is not beside this '
            'repository, so the merged itemlist union has nothing to be '
            'diffed against')

    # An audit that could not be given its inputs is a SKIP and not a pass, and
    # the distinction is the whole reason this list exists -- see the header.
    # assets.py decides at runtime, because whether the donor paks are on this
    # machine is not something audit.py can know in advance.
    for r in list(results):
        if 'assets: SKIP' in r['out']:
            results.remove(r)
            vacuous.append(r['out'].strip().replace('assets: SKIP -- ',
                                                    'assets: '))
        # A PARTIAL run is the same statement one donor at a time: it resolved
        # against the data it had and cannot speak for the rest, so it is not a
        # pass either.  Its own last line already says which donors were absent.
        elif 'assets: PARTIAL' in r['out']:
            results.remove(r)
            vacuous.append([ln for ln in r['out'].splitlines()
                            if 'assets: PARTIAL' in ln][0]
                           .strip().replace('assets: PARTIAL -- ', 'assets: '))
        # fnsweep needs the vendored bundles.  They are in the
        # repository, so this fires only on a partial checkout -- and it says so
        # rather than passing, for the reason in this file's header.
        elif 'fnsweep.py: SKIP' in r['out']:
            results.remove(r)
            vacuous.append(r['out'].strip().replace('fnsweep.py: SKIP -- ',
                                                    'fnsweep: '))
        # dsweep reads the workspace's trees, and `q2pro/src/{ctf,xatrix,rogue}`
        # there is a local replay branch rather than anything cloneable
        # (section 4.1), so a release runner cannot have them.  It raised
        # instead of saying so until the guard in its `__main__` was written,
        # and the traceback arrived here as a finding: a missing input read as
        # a defect in the tree, and every release job failed on it.
        elif 'dsweep.py: SKIP' in r['out']:
            results.remove(r)
            vacuous.append(r['out'].strip().replace('dsweep.py: SKIP -- ',
                                                    'dsweep: '))

    # --- report -----------------------------------------------------------
    failed = [r for r in results if r['hits'] or r['broken']]
    for r in results:
        mark = 'FAIL' if (r['hits'] or r['broken']) else 'ok  '
        print(f'[{mark}] {r["label"]}')
        if a.verbose or r['hits'] or r['broken']:
            for ln in r['out'].splitlines():
                print('        ' + ln)
    for v in vacuous:
        print(f'[skip] {v}')

    if failed:
        print(f'\naudit.py: {len(failed)} audit(s) reported findings '
              f'(a finding fails the build)')
        return 1
    print(f'\naudit.py: {len(results)} audit(s) clean, '
          f'{len(vacuous)} not applicable yet')
    return 0


if __name__ == '__main__':
    sys.exit(main())
