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


def _auditems(out):
    m = re.search(r'TOTAL differing fields across shared items:\s*(\d+)', out)
    if m and int(m.group(1)) > 0:
        return [ln for ln in out.splitlines() if ln.strip().startswith(('==', '   '))]
    return _bang(out)


def _gates(out):
    # gates.py already exits non-zero and prefixes findings with '!!'
    return _bang(out)


PARSERS = {
    'gates.py': _gates,
    'keycontract.py': _bang,
    'slotkind.py': _bang,
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

    results, vacuous = [], []

    # --- single-tree audits: meaningful from Phase 0 on -------------------
    # keycontract: statusbar slot vs STAT_ macro vs spawn key vs descriptor type
    results.append(run('keycontract.py', [f'colosseum={tree}'], 'keycontract'))
    # slotkind: does a bar read a slot as a kind the code does not write?
    results.append(run('slotkind.py', [f'colosseum={tree}'], 'slotkind'))
    # gates: every ruleset decision goes through the dispatch or a predicate,
    # and the monster-suppression idiom has not come back (Phase 1 exit).
    results.append(run('gates.py', ['--tree', tree], 'gates'))

    # --- comparative audits: need a donor ---------------------------------
    if donors:
        for label, path in donors:
            results.append(run('auditsave.py', [path], f'auditsave/{label}'))
            base_items = os.path.join(tree, 'g_items.c')
            var_items = os.path.join(path, 'g_items.c')
            if os.path.exists(var_items):
                results.append(run('auditems.py', [base_items, var_items],
                                   f'auditems/{label}'))
            else:
                vacuous.append(f'auditems/{label}: {var_items} absent')
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
