# CLAUDE.md -- Colosseum

Guidance for Claude Code when working in this repository. `SPECS.md` is the contract, and this file is the standing instruction for how to decide a question it does not already answer.

## What this tree is, and what that decides

`colosseum/` merges baseq2, The Reckoning (Xatrix) and Ground Zero (Rogue) into one game library **loaded against the original 1998 maps and assets**, with all three content layers available at once. Everything below follows from that constraint: a merge that needs its own maps is not a merge this project can make.

## The merge rule

> **The spine is the default.** Ground Zero's and Xatrix's changes are ADDITIONS. Where two donors conflict at a site, **id's line is the fallback arm** and the pack's is the one the content latch selects.

`if (content_flavour & CONTENT_ROGUE) <pack> else <id>` is that rule in code, and it is the runtime generalisation of the compile-time form a 1999 merge of these same three packs would have used. The derivation, and the per-site evidence behind each arm, is in `SPECS.md` (R-CORE-11); read it before changing a latched site.

**Where a shared file carries no latch, that is a judgement and not an omission.** Adding one there goes beyond what the merge decided; it is a legitimate decision for this project to make, and it is a decision, so it gets a row.

## The latch has a scope, and it is narrow

* **`content_flavour` reports the SERVER's configuration, not the entity's.** It is set in `ED_CallSpawn` from `G_LayerEnabled`, so it is the same value for everything in the level.
* **It is for a monster BOTH donors have.** Never use it to gate a pack-exclusive entity's own defining feature. The spawn path refuses nothing when a layer is off, so `monster_gekk`, `monster_widow` and the rest keep working; a feature gated on the latch instead silently disappears.
* **Where the world already carries the distinction, the world is the selector.** The heat chick is its own classname whose spawn function sets `s.skinnum = 3`, so `skinnum > 1` is the test and the layer must not appear in it (R-201).
* **Not every difference is behaviour.** Some are ABI accommodations -- a `trace_t *` added to a dodge signature, say -- and want no latch at all.
* **A helper can remove the need for a latch.** Where a difference is a tuning value rather than a policy, a shared helper that takes the difference as a parameter is better than two arms: the nightmare-pain test, a velocity lead, a slots-left count. That is a refactoring idea, never a behaviour source.

## Where the evidence lives

The pinned donor bundles are the reference for what an arm actually contained: `vendor/replay/*.bundle`, read through `build/replay.git` -- spine `baseq2` `5d180cb`, `port_xatrix` `2829dba`, `port_rogue` `2ebed93`, `port_ctf` `e94bbeb`, plus `port_ra2` and `port_osp`. Compare against those, not against another project's port of the same pack -- they differ, and the difference is usually the other project's decision rather than the donor's.

## The rest of the standing rules

* `SPECS.md` sections 0 and 7 are the rules the whole tree is built on; a change that contradicts one is a change to that spec, not a comment in the code.
* Every behaviour decision is stated where it lives -- the requirement it serves in `SPECS.md`, and the reasoning at the site it touches. A decision with no check is not done.
* `src/g_ptrs.c` is generated. Regenerate it -- `cd src && python3 genptr.py $(PTR_SRC) > g_ptrs.c` -- never hand-edit; `make check-ptrs` fails a stale one.
* `make check` runs the audits and a finding fails the build. `tools/bootmatrix.sh` and `tools/smoke.sh` are the two on-demand checks worth running after touching shared or monster code; each prints its own total, and that is the figure to quote.
* `tools/playtest.sh` drives the real server with headless clients. It cannot make a monster fight, so monster-AI changes are verified by donor comparison and must say so rather than implying a scenario covered them.
* The botlib -- `gladiator.so`/`gladiator.dll`, the shared library holding the bot AI -- is the submodule at `vendor/gladiator-bot-restored`. It is a separate build and is never merged into `src/`; `docs/botlib-contract.md` is the interface and `tools/botabi.py` checks it.
* The distribution is GPL-2-or-later throughout (R-LIC-1..4). A file whose upstream terms are narrower does not enter the tree.
* `README.md` is for operators and `DEVELOPMENT.md` for contributors; keep build, audit and ledger material out of the first one.
