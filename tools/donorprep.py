#!/usr/bin/env python3
"""Normalise a reconstruction donor's copy of a spine file before the merge.

doc/reconciliation.md R-65.  Used for RA2 in Phase 4 and for osp-tourney in
Phase 5, which is the same kind of tree with the same two habits.


Two categories of difference in the RA2 reconstruction are not RA2's feature
and must not reach the merge, because `git merge-file` would otherwise offer
each one as a conflict to resolve by hand:

  * the asm-matching address comments.  `rocketarena2-public` is byte-matched
    against the shipped 1999 binaries and annotates every function with the
    address range it matched.  SPECS.md N1: those oracles are meaningless in
    this tree.  1,563 lines of them across the donor.
  * the stripped GPL header.  The reconstruction drops id's licence block; every
    file in Colosseum carries one (R-LIC-1), and the spine's is the one to use
    under sec 7 rule 1.
"""
import re
import sys

# The two reconstructions annotate differently and BOTH have to be matched, or
# the pass reports zero on one of them and leaves 1,910 lines in the merge:
#   rocketarena2-public   /* gamei386.so 0x0001ddf8-0x0001de8a */
#   osp-tourney           // gamei386.so: 00014EE8..00014FC5
ASM = re.compile(r'^[ \t]*(?:/\*[ \t]*game[a-z0-9_]*\.(?:dll|so|exe)[ :][^\n]*\*/'
                 r'|//[ \t]*game[a-z0-9_]*\.(?:dll|so|exe):[^\n]*)\n', re.M)
HDR = re.compile(r'\A/\*\nCopyright \(C\) 1997-2001 Id Software, Inc\..*?\n\*/\n', re.S)


def main():
    if len(sys.argv) != 4:
        sys.exit('usage: donorprep.py <base-file> <donor-file> <output-file>')
    base, theirs, out = sys.argv[1:4]
    b = open(base, encoding='utf-8').read()
    t = open(theirs, encoding='utf-8').read()
    n_asm = len(ASM.findall(t))
    t = ASM.sub('', t)
    hdr = HDR.match(b)
    added = False
    if hdr and not HDR.match(t):
        # RA2 left a blank line where the header was; without dropping it the
        # header re-insertion shows up as a one-line diff in every single file.
        t = hdr.group(0) + t.lstrip('\n')
        added = True
    open(out, 'w', encoding='utf-8').write(t)
    print('  prep %-14s asm-comments -%-4d header %s'
          % (out.split('/')[-1], n_asm, '+1' if added else 'kept'))


if __name__ == '__main__':
    main()
