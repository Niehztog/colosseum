# CLAUDE.md -- Colosseum

Guidance for Claude Code when working in this repository. `SPECS.md` is the
contract, `doc/reconciliation.md` is the decision log, and this file is the
standing instruction for how to decide a question the two do not already answer.

## Working on Colosseum: Gladiator is the authoritative integration reference

`colosseum/` merges baseq2, The Reckoning (Xatrix) and Ground Zero (Rogue) into one
game library **loaded against the original 1998 maps and assets**, with all three
content layers available at once. That constraint decides which prior art counts.

**When a location is ambiguous -- the donors disagree and it is not obvious which
behaviour to carry -- read `gladq2_src/` first.** Mr. Elusive's Gladiator Bot game
source is the one merge of these same three packs built under *our* constraint: one
DLL, unmodified retail maps, both packs enabled (`#define XATRIX` / `#define ROGUE`
in its `g_local.h`). Files map by name: `src/m_medic.c` -> `gladq2_src/m_medic.c`.

**Its rule, and it is consistent across all 19 shared monster files:**

> The spine is the default. Ground Zero's and Xatrix's changes are ADDITIONS, each
> wrapped in `#ifdef ROGUE` / `#ifdef XATRIX`, with **id's line in the `#else`**
> where the two conflict. Where he took nothing from a pack, the file is id's,
> untouched.

That is Colosseum's architecture one level up. Gladiator resolves the choice at
COMPILE time and so gets one behaviour per binary;
`if (content_flavour & CONTENT_ROGUE) <pack> else <id>` is the runtime
generalisation of `#ifdef ROGUE <pack> #else <id> #endif`. So Gladiator's blocks map
1:1 onto the latch's two arms and can be read straight off.

**How to use it:**

* **50 `#ifdef` blocks across the shared monster files are a direct per-site oracle**
  -- hover 14, flyer 8, gunner 8, chick 7+1, brain 3+1, soldier 2+1, supertank 2,
  infantry/medic/parasite 1 each. The `#ifdef` arm is the pack's, the `#else` arm is
  id's.
* **Where Gladiator has NO `#ifdef`, the file is id's and that is a judgement, not an
  omission** -- verified for `m_boss2.c` (plain `DEFAULT_BULLET_HSPREAD`, no
  predictive rocket), `m_tank.c`, `m_berserk.c`, `m_boss31.c`, `m_boss32.c`,
  `m_float.c` (`floater_attack` is id's one-liner), `m_mutant.c`, `m_gladiator.c`:
  zero Ground Zero tokens in any of them. A latch Colosseum adds there goes BEYOND
  Gladiator; that is legitimate, because Colosseum can serve both maps at once, but
  it is Colosseum's decision and the pinned bundles stay the reference for the arms.
* **Not every `#ifdef` is behaviour.** `m_medic.c`'s only one adds `trace_t *trace`
  to `medic_dodge`'s signature -- an ABI accommodation for the tree-wide
  `monsterinfo.dodge` type change under ROGUE.
* **The latch is for a monster BOTH donors have. Never use it to gate a
  pack-exclusive entity's own defining feature.** `content_flavour` is set in
  `ED_CallSpawn` from `G_LayerEnabled`, so it reports whether the SERVER has a layer
  on -- it is not a property of the entity, and it is the same value for everything
  in the level. The spawn path refuses nothing when a layer is off, so
  `monster_gekk`, `monster_widow` and the rest keep working; a feature gated on the
  latch instead silently disappears. Where the world already carries the
  distinction, that is the selector: the heat chick is its own classname whose spawn
  function sets `s.skinnum = 3`, so `skinnum > 1` is the test and the layer must not
  appear in it (R-201).
* **Gladiator shows the COST of not latching**, which is the argument for
  Colosseum's: `g_spawn.c:412` maps `monster_medic_commander` to `SP_monster_medic`,
  and that function is id's, so a Ground Zero map's Medic Commander degrades to a
  plain medic -- no commander health, no `monster_slots`, no reinforcement spawning.
  Colosseum's latch is Gladiator's answer plus the capability Gladiator gave up.

### The 2023 rerelease: authoritative on nothing here

`quake2-rerelease-dll-ra2/rerelease/` is id's own merge of the same packs
(`8dc1fc9`), and it is tempting for that reason. **It works under a premise this
project explicitly rejects**, and its own README says so: *"In cases of conflicting
spawnflags, maps were modified in order to resolve issues, so original expansion
pack maps may not load correctly with this DLL."* It ships its own maps and assets;
we do not. So it has no content latch at all (its only content switch is
`level.is_n64`, from the map path prefix), it keeps no parallel donor variants -- every
`MMOVE_T` name in `m_*.cpp` is unique -- it resolves `trigger_push`'s spawnflag
collision by RENUMBERING `START_OFF` from `0x02` to `0x08`, and it picks one winner
per site with no way back to the other. **Do not take its switching answers, and do
not treat its per-site winner as a correction to ours** -- it chose Ground Zero's
medic wholesale where both Gladiator and the spine say id's.

One thing in it is still worth borrowing where a difference is a tuning value rather
than a policy: it dissolves donor variants into shared helpers -- `M_ShouldReactToPain`
for the `skill == 3` nightmare-pain test, `PredictAim` for each donor's hand-rolled
velocity lead, `M_SlotsLeft`, `M_CalculatePitchToFire`. A helper that takes the
difference as a parameter needs no latch. Treat that as a refactoring idea, never as
a behaviour source.

## The rest of the standing rules

* `SPECS.md` sections 0 and 7 are the rules the whole tree is built on; a change
  that contradicts one needs an amendment row, not a comment.
* Every behaviour decision gets a `doc/reconciliation.md` R-row in the same commit,
  and the amendment table in `SPECS.md` gets a line. A ledger entry with no check is
  not done (`doc/regression.md`).
* `src/g_ptrs.c` is generated. Regenerate it -- `cd src && python3 genptr.py $(PTR_SRC)
  > g_ptrs.c` -- never hand-edit; `make check-ptrs` fails a stale one.
* `make check` runs the audits and a finding fails the build. `tools/bootmatrix.sh`
  (28 rows) and `tools/smoke.sh` (including the cross-process savegame round-trip)
  are the two on-demand checks worth running after touching shared or monster code.
* `tools/playtest.sh` drives the real server with headless clients. It cannot make a
  monster fight, so monster-AI changes are verified by donor comparison and must say
  so rather than implying a scenario covered them.
* The pinned donors live in `vendor/replay/*.bundle`, read through `build/replay.git`:
  spine `baseq2` `5d180cb`, `port_xatrix` `2829dba`, `port_rogue` `2ebed93`,
  `port_ctf` `e94bbeb`, plus `port_ra2` and `port_osp`. Compare against those, not
  against yamagi's ports -- they differ (yq2's Ground Zero drops boss2's
  `yaw_speed = 50` and makes both boss2 machine guns lead `+0.2`, where the pinned
  q2pro port has neither).
