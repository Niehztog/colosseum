# Colosseum harness archive

Rescued 2026-08-21 from an ephemeral WSL `/tmp` scratchpad that was one reboot
from deletion. Source path, which is the literal path hard-coded on line 2 of
`q2pro-mission-pack-tools/build_replay.sh`:

    /tmp/claude-1000/-mnt-c-Users-<user>-q2-dev-q2pro/<session>/scratchpad

Every item was verified byte-identical to its source by recursive md5 at copy
time. 52 MB total.

**What was rescued and what is committed are two different lists**, and this
file used to give only the first. The rescue is above; the table below marks
each row with whether it is in this repository. Three are not — `dl/` and
`astyle/` were deliberately left out (see *Deliberately not committed*), and
27 `thisimage` files were pruned from the rr-cache (see its row).

## What this is

The build harness behind five donor trees: `q2pro/src/{ctf,xatrix,rogue}` and
the `q2pro-enhancements` branches of `rocketarena2-public` and `osp-tourney`.
Each was produced by replaying Q2PRO's 188 baseq2 game commits onto a normalised
copy of id's 3.20/3.21 source. The scripts live in
`/mnt/c/Users/<user>/q2-dev/q2pro-mission-pack-tools`; this archive holds the
**inputs and accumulated state** they need, which the scripts do not carry.

## Contents

| path | what | in this repo? | irreplaceable? |
|---|---|---|---|
| `rr-cache/ra2replay/` | **431** recorded git rerere resolutions in **407** conflict directories. 411 was this file's figure and was the directory count read as a resolution count; one directory holds 24 numbered `preimage.N`/`postimage.N` pairs | yes, less 27 `thisimage` | **YES** |
| `rr-cache/replay-missionpacks/` | 85 resolutions in 85 directories. Six of its conflict hashes also appear under `ra2replay/`, so the two caches total 492 directories and 486 distinct hashes | yes | **YES** |
| `tools-inputs/commit_paths.txt` | 188 lines, `<sha> <path>`; every SHA still resolves against `q2pro` | yes | hard |
| `tools-inputs/style_residual.patch` | the astyle pass's residual fixups | yes | hard |
| `tools-inputs/osp/` | **110** files, `mkosp.py`'s input tree (73 was the count of its `.c`/`.h` alone; the configs, maps and motds are inputs too) | yes | hard |
| `dl/x/quake2-3.21/` | id's official 3.21 source; `writetree.py`'s input and the GPL-header donor | **no** | no (public) |
| `dl/x/{ctf102,xatrix320,rogue320,q2src320}/` | the other official releases | **no** | no (public) |
| `astyle/astyle-3.1` | the exact Artistic Style 3.1 binary used for the reformat pass | **no** | no, but pinning matters |
| `notes/TODO.md` | 8 verification items from the original harness — **all eight discharged**, SPECS.md R-VER-15 | yes | YES |
| `notes/commits.txt` | 188 × `sha|date|subject` | yes | no |
| `notes/gutted.log`, `notes/bin.json`, `notes/style.rej` | harness working state | yes | no |

**27 `thisimage` files are not committed.** `thisimage` is what `git rerere`
writes for the conflict it is resolving *right now* — working state, not cache.
Git does not read it back and does not need it to replay a resolution. All 27
were under `ra2replay/`, in four directories that each also carry the complete
`preimage`/`postimage` pair, so no resolution was lost. This repository's copy
is therefore a **pruned** one; the byte-identity above is a statement about the
2026-08-21 rescue, not about what git tracks. `doc/provenance.md` §1.3 says the
same from the other side.

**The rerere caches are the reason this archive exists.** Git bundles carry
objects and refs only, so the three `*-replay.bundle` files in
`/mnt/c/Users/<user>/q2-dev/` reconstitute every `v_*`/`port_*` branch and tag
`C0` — but not one line of how any conflict was resolved. Those 516 resolutions
existed nowhere else. rerere is content-addressed by conflict hash, so the cache
works against a repo reconstituted from the bundles.

## Deliberately not committed

- The `ra2replay/` and `replay-missionpacks/` working trees — the bundles
  reconstitute them in full (`git bundle verify` reports a complete history for
  all three).
- The `verify-*/`, `t_*/` and `as_*/` test fixtures — reproducible.
- `dl/` — 12 MB of id's five official source releases, public and reobtainable.
- `astyle/astyle-3.1` — see the licence note below.

## Licence note

`astyle/astyle-3.1` is a third-party binary and stays **in this archive only**.
It is not committed to the Colosseum repository: SPECS.md R-LIC-1 puts that tree
under GPL-2-or-later and R-LIC-4 makes the licence position a first-screen
statement, so a binary whose terms have not been checked against R-LIC-1 does
not enter the distribution. R-CONV-1a pins the version string instead.
