# Colosseum harness archive

Rescued 2026-08-21 from an ephemeral WSL `/tmp` scratchpad that was one reboot
from deletion. Source path, which is the literal path hard-coded on line 2 of
`q2pro-mission-pack-tools/build_replay.sh`:

    /tmp/claude-1000/-mnt-c-Users-<user>-q2-dev-q2pro/<session>/scratchpad

Every item below was verified byte-identical to its source by recursive md5 at
copy time. 52 MB total.

## What this is

The build harness behind five donor trees: `q2pro/src/{ctf,xatrix,rogue}` and
the `q2pro-enhancements` branches of `rocketarena2-public` and `osp-tourney`.
Each was produced by replaying Q2PRO's 188 baseq2 game commits onto a normalised
copy of id's 3.20/3.21 source. The scripts live in
`/mnt/c/Users/<user>/q2-dev/q2pro-mission-pack-tools`; this archive holds the
**inputs and accumulated state** they need, which the scripts do not carry.

## Contents

| path | what | irreplaceable? |
|---|---|---|
| `rr-cache/ra2replay/` | 411 recorded git rerere conflict resolutions | **YES** |
| `rr-cache/replay-missionpacks/` | 85 recorded git rerere conflict resolutions | **YES** |
| `tools-inputs/commit_paths.txt` | 188 lines, `<sha> <path>`; every SHA still resolves against `q2pro` | hard |
| `tools-inputs/style_residual.patch` | the astyle pass's residual fixups | hard |
| `tools-inputs/osp/` | 73 files, `mkosp.py`'s input tree | hard |
| `dl/x/quake2-3.21/` | id's official 3.21 source; `writetree.py`'s input and the GPL-header donor | no (public) |
| `dl/x/{ctf102,xatrix320,rogue320,q2src320}/` | the other official releases | no (public) |
| `astyle/astyle-3.1` | the exact Artistic Style 3.1 binary used for the reformat pass | no, but pinning matters |
| `notes/TODO.md` | **8 unfinished verification items from the original harness — read this** | YES |
| `notes/commits.txt` | 188 × `sha|date|subject` | no |
| `notes/gutted.log`, `notes/bin.json`, `notes/style.rej` | harness working state | no |

**The rerere caches are the reason this archive exists.** Git bundles carry
objects and refs only, so the three `*-replay.bundle` files in
`/mnt/c/Users/<user>/q2-dev/` reconstitute every `v_*`/`port_*` branch and tag
`C0` — but not one line of how any conflict was resolved. Those 496 resolutions
existed nowhere else. rerere is content-addressed by conflict hash, so the cache
works against a repo reconstituted from the bundles.

## Deliberately not copied

- The `ra2replay/` and `replay-missionpacks/` working trees — the bundles
  reconstitute them in full (`git bundle verify` reports a complete history for
  all three).
- The `verify-*/`, `t_*/` and `as_*/` test fixtures — reproducible.

## Licence note

`astyle/astyle-3.1` is a third-party binary and stays **in this archive only**.
It is not committed to the Colosseum repository: SPECS.md R-LIC-1 puts that tree
under GPL-2-or-later and R-LIC-4 makes the licence position a first-screen
statement, so a binary whose terms have not been checked against R-LIC-1 does
not enter the distribution. R-CONV-1a pins the version string instead.
