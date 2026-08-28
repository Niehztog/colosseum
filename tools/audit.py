#!/usr/bin/env python3
"""Run the four contract audits and fail the build on a finding (R-TOOL-3).

R-TOOL-3: "The four audits of R-TOOL-1 (`auditsave`, `slotkind`, `keycontract`,
`auditems`) run in the build, not on request.  A new finding fails the build the
way a warning does (R-BUILD-2)."

The four tools were written as *reports*: they print what they find and exit 0
either way, because in the replay harness a human read the output.  A build
cannot read.  This driver runs each one, scans its output for the markers the
tool uses to signal a finding, and exits non-zero if any fired.

It also handles the case the harness never had: a tree with no donor in it.
`auditsave` and `auditems` are *comparative* -- they ask what a donor added on
top of baseq2 -- so before Phase 2 lands a donor they have nothing to compare
and must report "vacuous", not "clean".  Recording a vacuous pass as a pass is
how a check rots (see SPECS.md R-BUILD-5 for the same failure twice over), so
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
#   auditsave    prints one bare '  label.field' line per finding, then a
#                trailer; no marker at all, so the lines are counted
#   auditems     prints 'TOTAL differing fields across shared items: N'


def _bang(out):
    return [ln for ln in out.splitlines() if '!!' in ln]


def _auditsave(out):
    # every line before the trailer that looks like 'label.field'
    hits = []
    for ln in out.splitlines():
        s = ln.strip()
        if s.startswith('(') or not s:
            continue
        if re.match(r'^[a-z_]+\.[A-Za-z_]\w*$', s):
            hits.append(ln)
    return hits


# R-VER-15 item 1 asks whether the merge LOST a baseq2 field.  A merged union
# tree also shows the donors' legitimate changes, and those are not findings --
# reconciliation.md R-33 measured exactly two and named them.  Recording them
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
        "Xatrix's Use_Weapon2 -- the Ionripper shares the slot (reconciliation R-33)",
    ('weapon_railgun', 'use'):
        "Xatrix's Use_Weapon2 -- the Phalanx shares the slot (reconciliation R-33)",
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
                        f'an accepted difference (R-VER-15 item 1)')
    return hits


def _gates(out):
    # gates.py already exits non-zero and prefixes findings with '!!'
    return _bang(out)


PARSERS = {
    'gates.py': _gates,
    'units.py': _bang,
    'allocpairs.py': _bang,
    'lostref.py': _bang,
    'donorgate.py': _bang,
    'encoding.py': _bang,
    'bounded.py': _bang,
    'externs.py': _bang,
    'noexec.py': _bang,
    'botabi.py': _bang,
    'dsweep.py': _bang,
    'keycontract.py': _bang,
    'slotkind.py': _bang,
    'dupvalue.py': _bang,
    'auditsave.py': _auditsave,
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
    # R-CORE-5 measures src/ against q2pro/src/game, but a tree without q2pro
    # beside it must still build.  Absent means skipped-and-said-so.
    donors = [(l, p) for l, p in donors if os.path.isdir(p)]

    results, vacuous = [], []

    # --- single-tree audits: meaningful from Phase 0 on -------------------
    # keycontract: statusbar slot vs STAT_ macro vs spawn key vs descriptor type
    results.append(run('keycontract.py', [f'colosseum={tree}'], 'keycontract'))
    # slotkind: does a bar read a slot as a kind the code does not write, and
    # was the slot available to be claimed at all (R-OSP-7 clause 7)?
    results.append(run('slotkind.py', [f'colosseum={tree}'], 'slotkind'))
    # ...and slotkind's own controls, five mutations of the real map and emitter.
    # R-VER-9 clause 2: a check that has never failed is not trusted, so the
    # controls run in the build beside the check rather than in a document.
    results.append(run('slotkind.py', ['--selftest'], 'slotkind/controls'))
    # gates: every ruleset decision goes through the dispatch or a predicate,
    # and the monster-suppression idiom has not come back (Phase 1 exit).
    results.append(run('gates.py', ['--tree', tree], 'gates'))
    # dupvalue: two names for one POSITION in a list something outside this tree
    # walks (R-142).  Each donor numbered its own extra weapons from 12 because
    # each ships a precache block with nothing after the BFG; R-CORE-2 unions the
    # content into ONE ordered block, and three private 12s made seven weapons
    # draw somebody else's model.  Checks the values and, separately, that
    # WEAP_* still agrees with the block it indexes -- a family can be
    # internally consistent and still disagree with its list.
    results.append(run('dupvalue.py', ['--tree', tree], 'dupvalue'))
    results.append(run('dupvalue.py', ['--selftest'], 'dupvalue/controls'))
    # units: the timer-unit contract (R-VER-21).  q2pro's frame-number conversion
    # left a field's type saying nothing about its unit, and a merged tree has a
    # failure mode the separate packs cannot: one g_local.h means one declaration
    # wins and every site written against the other is now a mix.
    results.append(run('units.py', ['--tree', tree], 'units'))
    results.append(run('units.py', ['--selftest'], 'units/controls'))
    # allocpairs: the allocator-pair contract (R-VER-22).  A pointer must be
    # released by the family that produced it, and this tree has two -- libc and
    # the engine's tagged blocks.  Not a libc ban: g_main.c's strdup/free pair is
    # correct and is upstream's, so the unit that has to agree is the pointer
    # (R-55, R-63).
    results.append(run('allocpairs.py', ['--tree', tree], 'allocpairs'))
    results.append(run('allocpairs.py', ['--selftest'], 'allocpairs/controls'))
    # lostref: R-26's sixth rule (R-VER-24).  Three donors delete the monster set
    # and Colosseum replays none of those deletions -- but RA2 deletes monster
    # code from inside the SHARED files too, where the five merge rules cannot
    # tell a feature deletion from that one and the compiler sees nothing
    # because the callee still exists (R-64).
    results.append(run('lostref.py', ['--tree', tree], 'lostref'))
    results.append(run('lostref.py', ['--selftest'], 'lostref/controls'))
    # donorgate: a donor's own fields and functions, used in a SHARED file, must
    # be inside that donor's gate (R-VER-25).  gates.py asks whether a ruleset is
    # chosen by testing a cvar; this asks whether one donor's state is read under
    # another donor's ruleset, which is what R-70's six sites did.
    results.append(run('donorgate.py', ['--tree', tree], 'donorgate'))
    results.append(run('donorgate.py', ['--selftest'], 'donorgate/controls'))
    # bounded: no unbounded string copy anywhere in src/ (R-SEC-1, R-VER-30).
    # A ban rather than a reachability judgement, because reachability is what a
    # reviewer gets wrong: `sprintf(entry, "yv %d ", y)` reads as arithmetic
    # until somebody adds a %s to it, and the netname three lines down was
    # always client-controlled.
    results.append(run('bounded.py', ['--tree', tree], 'bounded'))
    results.append(run('bounded.py', ['--selftest'], 'bounded/controls'))
    # externs: a .c may not declare what another .c defines, and where the tree
    # still does (baseq2's 150-row spawn table, mostly), the declaration must
    # agree with the definition (R-SEC-8, R-VER-31).  Its first run found
    # Ground Zero's `Move_Calc` prototype one qualifier out since Phase 2.
    results.append(run('externs.py', ['--tree', tree], 'externs'))
    results.append(run('externs.py', ['--selftest'], 'externs/controls'))
    # noexec: no socket, no process, no exit() (R-SEC-7, R-VER-32).  RA2 shipped
    # a UDP event forwarder whose three helpers each called exit(1) from inside
    # a game library; it is gone, and this is what keeps it gone.
    results.append(run('noexec.py', ['--tree', tree], 'noexec'))
    results.append(run('noexec.py', ['--selftest'], 'noexec/controls'))
    # encoding: every source file is valid UTF-8.  Trivial, and it has bitten
    # twice -- grep in a UTF-8 locale returns NOTHING for a file it cannot
    # decode, so a census can silently report zero (R-60).
    results.append(run('encoding.py', ['--tree', tree], 'encoding'))
    # botabi: the game<->botlib contract against the BRAIN's own header
    # (R-VER-28).  The Trace slot has two spellings and they are the same ABI at
    # 32 bits and different ABIs at 64, so taking the 32-bit one on aarch64
    # compiles, links, loads the library, spawns the bots and plays nothing
    # (doc/reconciliation.md R-97).  Skips itself with a message when the brain
    # is not beside the repository, the same rule auditems applies to q2pro.
    results.append(run('botabi.py', [], 'botabi'))
    results.append(run('botabi.py', ['--selftest'], 'botabi/controls'))

    # --- comparative audits: need a donor ---------------------------------
    # dsweep, not auditsave: both implement R-SAVE-3's "a persistent field with
    # no descriptor", but auditsave's struct_body regex misattributes members --
    # asked about the mission-pack merge it reported spawn_temp_t's `pausetime`,
    # `minyaw` and `height` as monsterinfo_t fields.  dsweep brace-matches the
    # struct bodies and carries its own extractor self-test.  §7 rule 9: where a
    # tool and a reviewer disagree, fix the tool.
    results.append(run('dsweep.py', [], 'dsweep/descriptors'))
    if donors:
        for label, path in donors:
            # R-VER-15 item 1 compares against **q2pro's baseq2 entries**, not
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
        if not donors:
            vacuous.append('auditems: the reference tree (q2pro/src/game) is not '
                           'beside this repository, so there is nothing to '
                           'compare the merged itemlist against')
    else:
        vacuous += [
            'auditsave: no donor supplied -- nothing has been added to a saved '
            'struct yet, so there is no descriptor to be missing (R-SAVE-3 '
            'bites from Phase 2)',
            'auditems: no donor supplied -- the itemlist union of R-CORE-2 does '
            'not exist yet, so there is nothing to diff against baseq2',
        ]

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
              f'(R-TOOL-3: a finding fails the build)')
        return 1
    print(f'\naudit.py: {len(results)} audit(s) clean, '
          f'{len(vacuous)} not applicable yet')
    return 0


if __name__ == '__main__':
    sys.exit(main())
