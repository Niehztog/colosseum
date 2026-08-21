# Colosseum — Specification

A single modern Quake II game library that unites baseq2, both mission packs,
Threewave CTF, Rocket Arena 2 and OSP Tourney DM on one Q2PRO-derived code
base, and restores full Gladiator Bot command and botlib support on top of it.

| | |
|---|---|
| **Project name** | Colosseum |
| **Artifact** | `game<cpu>.so` / `game<cpu>.dll` (Q2PRO game API), gamedir `colosseum` |
| **Working directory** | `<workspace>/colosseum` |
| **Spec version** | 1.10 — 2026-08-21 |
| **Status** | Draft for review. No code written yet. |
| **Amendment 1.1** | Re-measured against the three replay bundles (§3.2), the two donor port write-ups and `q2pro/doc/mission-packs.md`. Changed: §3, §3.1, new §3.2, D8–D9, R-CORE-8, R-CORE-10, R-CORE-11, new R-CORE-12/13, R-MODE-5, R-MP-5, new R-KEY group, R-SAVE-3, R-OSP-7, new R-SEC-8/9, R-CONV-1, new R-TOOL group, §7 rules 1/6/9, §9 phase order, §10, §11, Q1/Q2/Q9 closed |
| **Amendment 1.2** | Thirty-one design decisions settled with the author; §12 closed and emptied. **Scope cut:** Colored Hitman and `sp_dm` dropped entirely. **Posture:** public server software, released by staged exposure. **§7 rule 6 gains four exemptions** — observers, map rotation, stats logging and menus are per-ruleset. Eleven corrections of fact, the largest being that `gladq2_src` is *not* `gladiator-bot-restored/game` and that the harness scratchpad was never lost. Changed: §1.1, §2, §3, §3.2, §5.2, R-CORE-14, R-MODE-2/4/5/7, R-BOT-1/4/6/29, R-MENU-1/2, R-EXTRA-6, R-SP-6 struck, §6.7 struck, R-OSP-7, R-COMPAT-6, R-BUILD-5, §7 rule 6, §9 re-cut to nine phases, §10, §11, §12, Appendix B |
| **Amendment 1.3** | Re-measured `m_mode` against `osp-tourney@ab0e13a`. **One correction of fact, one new requirement.** OSP Tourney DM selects among **four** modes of play, not three: 1.0–1.2 omitted `match_mode 3`, the 1v1 duel mode, which carries 34 dedicated sites in 10 files, six `OSP_1v1*` helpers, its own client command, its own statusbar branch and three of the fifteen shipped configs. All four are inherited, not work. Changed: R-MODE-2, R-MODE-7 matrix, R-OSP-1, new R-OSP-12, new R-OSP-13, R-BOT-29, new R-VER-16, §9 Phase 5 exit. R-OSP-2's counts were re-measured and are unchanged — the gap was in prose, not in the measured surface. |
| **Amendment 1.4** | Re-verified every input against the trees on the reference machine, and found the spec had been measured on a **different host**. **Two corrections of environment, six of fact, three new requirements, no change of intent.** The largest: every donor commit SHA this spec pinned is unresolvable here — the trees were re-committed between the WSL box that measured 1.0–1.3 and the machine that will build Colosseum, so §3.3 re-pins them; the host is **aarch64**, not x86, which voids R-BUILD-5's entire matrix; `tools/divergence.py` and the count scripts R-TOOL-2 gates on **do not exist** and are work, not import; and R-SEC-2 cited a file present in no RA2 branch or commit. The three replay bundles are untouched — every ref they carry resolves exactly as 1.1 recorded, which is what makes the re-pin cheap. Changed: §0 rule 3, §3 header, §3 RA2/OSP rows, §3 counting method, new §3.3, R-CORE-5, R-MODE-5, §5.2 tree, R-BOT-4, R-BUILD-3, R-BUILD-5, new R-BUILD-6, R-CONV-1a, R-TOOL-1, new R-TOOL-5, R-SEC-2, R-SEC-8, R-EXTRA-6, R-OSP-3/10/12 pins, R-VER-8, §9 Phase 0 and Phase 1, §11 new risk 19, §12, Appendix A, Appendix B. **Mid-amendment the author installed `meson`, `clang`, `astyle` 3.1 and the x86-64 and i686 ELF cross toolchains**, which closed R-BUILD-3, made R-BUILD-2's two-compiler posture gating, satisfied R-CONV-1a's astyle pin natively (byte-identical to the rescued binary), and moved R-BUILD-5's last two rows from unreachable to gating — so this amendment records a machine that got better while it was being described, and risk 19 drops accordingly. |
| **Amendment 1.5** | **Phase 0 built and ran.** The author directed Claude to build the tree and the engine and run the smoke test, which R-VER-9 forbade; R-VER-9 is amended to match what the project actually does rather than left asserting otherwise. Everything built clean on all five targets under both compilers, and the smoke test passed end to end — `q2dm1` in deathmatch, `base1` in co-op, and a savegame round-trip **across two processes**, which is the empirical check on the regenerated `save_ptrs[]` of `doc/reconciliation.md` R-1. Four findings became requirements: R-BUILD-2's "`-Wall -Wextra` clean" is unreachable on inherited code and now carries three measured suppressions; `_FORTIFY_SOURCE` is gcc-only because glibc hands clang a function-like macro named `dprintf` that collides with `game_import_t.dprintf`; the engine loads the game library from `homedir` and the install `libdir`, never from `basedir`; and a dedicated server cannot run single player at all, so the campaign is verified through co-op. Changed: R-BUILD-2, R-BUILD-5, new R-BUILD-8, R-SEC-6, R-VER-8, R-VER-9, new R-VER-17, §9 Phase 0, §12. |
| **Amendment 1.6** | **Phase 1 landed.** The ruleset dispatch is built and its behaviour is observed live: `g_ruleset` resolves, the alias layer works, the modifier matrix refuses what R-MODE-7 forbids, and the monster row of that matrix is now *measured* rather than promised — `ctf` runs 19 monsters on `base1` while `dm`, `arena` and `tourney` run zero, with `deathmatch` at 1 in all four. R-MODE-5's "the hook count falls out of the merge" resolved to **6 dispatch rows and 5 named predicates**, not 26 hooks. Two findings worth requirements: the monster gate's real home is `monster_start()`, so converting the 25 per-monster sites alone would have been cosmetic; and R-MODE-* is unobservable from outside the library, which is now fixed by `sv ruleset` (new R-VER-18) because `entities inhibited` measures spawnflag filtering and says nothing about the gate. Changed: R-MODE-5, R-MODE-7, new R-VER-18, §9 Phase 1, §12. |
| **Amendment 1.7** | **Phase 2 opened: `g_local.h` merged.** The bundles make the merge computable rather than manual — `git merge-file` against the spine took Xatrix with **zero** conflicts and Rogue with **five**, all five resolved with recorded reasons. **Three findings changed requirements.** R-KEY-1's named defect is *live*: both donor branches carry the broken rename of `spawn_temp_t.pausetime` to `pause_framenum`, the merge propagated it, and only `g_func.c`'s surviving `st.pausetime` made the compiler catch it — had that file also come from a donor it would have compiled clean and broken every `func_timer` in every shipped map. R-CORE-14 undercounts: it is **six** names on two bits, not four, and **Ground Zero holds both** (`FL_MECHANICAL` on `0x2000`, `FL_NOGIB` on `0x10000`), arriving four phases before the claimants the spec names. And R-CONV-2 has **no mission-pack prefix at all**, which the two genuine symbol collisions (`fire_heat`, `monster_fire_heat` — Xatrix's Phalanx projectile versus Ground Zero's plasma beam) immediately needed. Changed: R-CORE-6, R-CORE-11a, R-CORE-14, R-CONV-2, R-KEY-1, R-KEY-2, §9 Phase 2. |
| **Amendment 1.8** | **Phase 2's exit criteria are met: all three campaigns run.** Rogue merged — 45 shared files, 20 donor-only, 12,263 diff lines, 25 files conflict-free and 60 hunks resolved. `rmine1` runs 74 Ground Zero monsters, all four `xatrix` × `rogue` combinations boot, savegames round-trip in each. **One correction of fact and one honest gap.** R-CORE-11 names `monster_start` as the content-flavour latch point and **that is too late** — every `SP_monster_*` assigns its frame tables before calling it, so a gate there would read 0, always pick baseq2 and never crash; the latch moved to `ED_CallSpawn`. And **R-CORE-11's per-monster gating is not implemented**: Ground Zero *rewrites* seven of baseq2's monsters rather than adding to them, so with `rogue 0` those seven still behave as Ground Zero — stated plainly rather than left implied. Also: a spawnflag-bit collision between the mission packs on `trigger_push` (both claim `0x02`), resolved by the entity's own keys; three latent Ground Zero bugs preserved under §7 rule 2 and one *fixed* because it was undefined behaviour rather than wrong behaviour. Changed: R-CORE-11, R-CORE-11a, R-MP-5, R-VER-15, §9 Phase 2, §11 risk 2a. |
| **Amendment 1.9** | **R-CORE-11 implemented; Phase 2 complete.** Both evasion sets ship for the six monsters Ground Zero rewrote and the spawn-time latch selects between them — verified by observation, not assertion: `base1` runs 17 monsters on baseq2's dodge with `rogue 0` and 17 on Ground Zero's with `rogue 1`, and `badlands` runs 58 on baseq2's, which is Reckoning maps no longer inheriting Ground Zero's AI. Savegames round-trip in every configuration. **One implementation constraint worth recording as a rule:** the gate must be an `if`/`else` with two literal assignments, because `genptr.py` builds `save_ptrs[]` by scanning source text — a ternary registers neither table and a token-pasting macro registers a fragment, either of which yields a library that plays correctly and then cannot reload a savegame. Changed: R-CORE-11, R-SAVE-2, §9 Phase 2, §11 risk 2b. |
| **Amendment 1.10** | **R-CORE-11 fully gated; the residual is closed.** 1.9 left "about eleven frame tables across five monsters" ungated. That figure was **mismeasured**: it came from `git diff`'s `@@` hunk-header labels, which name the symbol *preceding* a hunk rather than the changed one, so three tables were counted that had only been context lines. Re-measured by extracting each table body and comparing directly: 18 tables differ, of which 5 across 4 monsters are genuine replacements — and all 5 are now gated at all 9 assignment sites. The rest are pure additions or bookkeeping, with the reasoning recorded per kind. `gates.py` gained a completeness check that fails the build on an ungated site or a ternary assignment; it first shipped with a flaw that made it pass its own negative control, described in `doc/reconciliation.md` R-35. Changed: R-CORE-11b, §9 Phase 2, §11 risk 2c. |

**Why "Colosseum".** The Colosseum is the arena the gladiators fought in, and it
was one venue that hosted many different kinds of games. That is exactly this
project: one library, one venue, many rulesets, with Mr. Elusive's Gladiator
bots as the opponents. It also avoids collision with every existing name in the
workspace (`gladq2_src`, `ugladq2`, `gladiator-bot-restored`,
`q3a_bot_backport_for_q2`, `openra2`, `rocketarena2`, `osp-tourney`), none of
which this replaces.

---

## 0. How to use this document

This is the specification of record for spec-driven development. Every
requirement carries a stable ID (`R-<GROUP>-<n>`). Code, commits, tests and
review notes reference those IDs. Rules:

1. **The spec leads.** A behavioural change lands in this file first, then in
   code. A commit that changes behaviour names the requirement it implements or
   amends.
2. **Requirements are testable.** Each one is phrased so that a reviewer can say
   pass or fail without consulting the author.
3. **Measured, not assumed.** Every number in §3 was measured against the trees
   in `<workspace>`, first on 2026-08-20 and re-verified on 2026-08-21
   (§3.3). Re-measure before trusting a stale figure; state the method when you
   do — 1.4 exists because a figure was right and the *path and commit* it was
   measured at were not.
4. **Open questions live in §12** and must be closed before the phase that
   depends on them starts.
5. **Anything not stated here is not in scope.** Add it to the spec or leave it
   out.
6. **Distinguish inherited from new.** A requirement the donors already satisfy
   is a requirement to *preserve*, and it names the upstream commit that
   satisfied it. A requirement no donor satisfies is work. Conflating the two is
   how a phase estimate goes wrong in both directions.

---

## 1. Identity and scope

### 1.1 Mission

Produce one Quake II game library that:

* reproduces the **functionality** of Mr. Elusive's Gladiator Bot v0.96 game
  module — every play style it shipped, every bot command, every menu, and the
  same botlib integration contract — so that the same bot brain drives it;
* is built on the **modern Q2PRO game source** and its mission-pack siblings,
  inheriting twenty-odd years of crash, overflow, out-of-bounds and savegame
  fixes, the frame-number timer model, the portable savegame system and the
  protocol extensions;
* replaces Gladiator's thin, bot-oriented stand-ins for CTF and Rocket Arena
  with the **full mods**: Threewave CTF 1.52, the Rocket Arena 2 v2.25
  reconstruction, and OSP Tourney DM v2.75 — reconciled the way Gladiator
  reconciled its donors, but on modern ground;
* selects its ruleset and content layers **at runtime**, from one binary.

### 1.2 Non-goals

* **N1.** Not a byte-exact reconstruction of anything. The reconstruction trees
  (`rocketarena2-public@main`, `osp-tourney@main`, `gladiator-bot-restored`) own
  that property; nothing here is address- or byte-matched, and the
  `asm_matching/` oracles are meaningless in this tree.
* **N2.** No reuse of `gladq2_src/` code. It is a **reference only** — read it
  for behaviour, naming and integration order, never copy from it. The one
  category of exception is §5.4.1.
* **N3.** No new bot AI. Colosseum is the game side of the botlib contract. The
  brain is `gladiator-bot-restored/botlib` (or the original 1999 binary).
* **N4.** No engine changes. Colosseum is a game library and must load into
  stock Q2PRO. Engine-side wants go in §12 as open questions, not into the tree.
* **N5.** Not a commercial product. See §4.
* **N6.** The single-player and co-op campaigns *are* in scope, for baseq2 and
  for both mission packs, and the monster code that CTF, RA2 and OSP Tourney all
  deleted is deliberately kept (R-CORE-8). What is out of scope is **bots in
  single player** — the Gladiator botlib is deathmatch-only, so `g_ruleset sp`
  runs without them.
* **N7.** **Colored Hitman is out of scope** (decided 2026-08-21). `g_ch.c`,
  `g_ch.h`, the `ch` cvar as a behaviour switch, `gclient_t.chcolor`, the colour
  statusbar, `ColorImageName` and `UpdateColoredHitman` are not carried. §6.7
  (R-CH-1..4) is struck. One thing survives: the `ch` **libvar** is still pushed
  to the brain, always `0`, because the brain reads it and sending zero costs
  nothing (R-BOT-6).
* **N8.** **`sp_dm` is out of scope** (decided 2026-08-21). uGladQ2 v0.99u's
  "Singleplayer Deathmatch" — `deathmatch 1` with monsters spawned, exec'ing
  `sp_dm/<mapname>.cfg` — is not carried. R-SP-6 is struck. This removes the
  project's only configuration in which monsters and bots occupy the same level,
  and with it the need for the Gladiator botlib to navigate around entities it
  was never designed for. Note this is the `sp_dm` **modifier** only; the `sp`
  **ruleset** — the baseq2, Reckoning and Ground Zero campaigns and co-op — is
  fully in scope and remains the sole justification for R-CORE-8.

### 1.3 Deliverables

| ID | Deliverable |
|---|---|
| D1 | The game library source tree, building `game<cpu>.{so,dll}` on Linux and Windows, 32- and 64-bit |
| D2 | `SPECS.md` (this file), kept current |
| D3 | `doc/reconciliation.md` — the per-donor merge ledger required by §7 |
| D4 | `doc/cvars.md`, `doc/commands.md`, `doc/entities.md` — the merged, deduplicated inventories |
| D5 | `doc/botlib-contract.md` — the frozen game↔botlib ABI and libvar contract |
| D6 | A default `colosseum/` gamedir config set: `bots.cfg`, per-ruleset configs, `default.cfg` |
| D7 | A regression checklist keyed to §10 |
| D8 | `tools/` — the replay and audit toolchain of R-TOOL-1, re-pointed at this tree |
| D9 | `doc/replay-coverage.md` — the per-donor ledger of R-CORE-12 |

---

## 2. Terminology

| Term | Meaning |
|---|---|
| **Ruleset** | The primary game mode. Exactly one is active. `dm`, `ctf`, `arena`, `tourney`, `sp` |
| **Content layer** | An orthogonal, latched content/behaviour set: `xatrix`, `rogue` |
| **Modifier** | A composable option within a ruleset: `teamplay`, `hook`, `runes`/`techs`, bots |
| **Donor** | An upstream tree contributing code or behaviour (§3) |
| **botlib** | The bot brain, loaded as a separate shared library exporting `GetBotAPI` |
| **Bot glue** | The `bl_*` translation unit family inside the game library |
| **Fake client** | A bot: a real client slot and `gclient_t`, driven by synthesised `usercmd_t` |

---

## 3. Provenance: donor inventory

All paths relative to `<workspace>`. Sizes are `.c` + `.h`, measured 2026-08-20 and re-verified 2026-08-21 (§3.3).

| Donor | Path | Files | Lines | Contributes |
|---|---:|---:|---:|---|
| **Q2PRO baseq2** | `q2pro/src/game` | 72 | 42,276 | The base: modern game API, frame-number timers, portable savegames, protocol extensions, 157 spawn classnames, 37 cvars, 27 client commands |
| **Q2PRO CTF** †| `q2pro/src/ctf` | 31 | 27,178 | Threewave CTF 1.52 with Q2PRO's history replayed: `g_ctf.c`, grapple, techs, `p_menu.c`, 163 classnames, 49 cvars, 40 commands |
| **Q2PRO Xatrix** †| `q2pro/src/xatrix` | 79 | 51,258 | *The Reckoning*: 173 classnames, Ion Ripper / Phalanx / Trap / Quad-Fire, gekk, fixbot, boss5, soldier variants |
| **Q2PRO Rogue** †| `q2pro/src/rogue` | 92 | 67,568 | *Ground Zero*: 192 classnames, spheres, chainfist/disruptor/ETF/plasma-beam/prox/tesla/nuke, DM ball, tag, hint paths, `gamerules`, widow, carrier, stalker, turret |
| **Rocket Arena 2** †| `rocketarena2-public` @ `cd0708b` (branch `q2pro-enhancements`) | 44 | 32,790 | RA2 v2.25 reconstruction already replayed onto the Q2PRO game API: `arena.c` (2,226 lines), 32 arenas, teams, voting, `maploop.c`, `menu.c`+`ra2menus.c`, grapple, `ra2stats.c/.h` local round log, 161 classnames |
| **OSP Tourney DM** †| `osp-tourney` @ `a8e30d0` (branch `q2pro-enhancements`) | 71 | 51,634 | Tourney v2.75 reconstruction already replayed onto the Q2PRO game API: match/referee/vote system, runes, hook, observer + camera, hi-scores, `osp_stats.c/.h`, `osp_*` modules, 161 classnames — **and a working Q2PRO-API port of the Gladiator bot glue** |
| **Gladiator, 1999 original** | `gladq2_src` | 134 | 92,226 | **Reference only.** The shipped 1999 integration (mtime 1999-08-02): `bl_*` glue, `botlib.h`, `p_menulib.c`, `p_botmenu.c`, `g_arena.c`, `p_lag.c`, `g_log.c`, and the `#ifdef` reconciliation model. `OBSERVER` is `//#define`d out here and **there is no `p_observer.c`** |
| **Gladiator, reconstruction** | `gladiator-bot-restored/game` | 135 | — | **Reference only, and the one to read.** Not the same tree as `gladq2_src` — they differ in 17 files. `OBSERVER` is **enabled** (`g_local.h:7`), 13 files carry live `#ifdef OBSERVER` blocks, and it adds **`p_observer.c` (2,386 lines)**, the observer implementation R-EXTRA-6 describes |
| **Gladiator botlib** | `gladiator-bot-restored/botlib` | 75 | 38,178 | The bot brain. Reconstructed to ~95% byte-identity (PE) / ~89% (ELF) against the two shipped v0.96 builds; reported feature-complete. Exports `GetBotAPI` |
| **uGladQ2** | `ugladq2` | 134 | — | **Reference only.** Unofficial 0.96u→0.99u cleanup of the Gladiator game module: a ~40-item bug-fix and QoL list for CTF/RA2 bot behaviour, plus `ctf_hook`, `ra_fastswitch`, `sp_dm` |
| **Replay bundles** | `../{osp,ra2}-q2pro-port-replay.bundle`, `../q2pro-mission-pack-replay.bundle` | — | — | The per-commit replay history behind the five † rows, and the only surviving copy of it (§3.2) |
| **Replay toolchain** | `q2pro-mission-pack-tools` | 30 | 1,660 | The normaliser, the twelve mechanical passes, the replay driver and the four audit checks (R-TOOL-1) |

† **Not upstream Q2PRO.** `q2pro/src/{ctf,xatrix,rogue}` are two squashed
commits on a local `feature/mission-packs` branch, and the two mod branches are
three and four commits over their reconstruction `main`. All five are outputs of
the same replay harness, carrying the same class of hand-resolved deviations.
`q2pro/src/game` is the only genuine upstream Q2PRO tree in this table, and §7
rule 1's "Q2PRO is the base" means that tree specifically.

**Counting method.** The classname, cvar and command totals above and in §6 are
acceptance criteria, so they must be reproducible. A naive count of unique
`gi.cvar*()` first arguments gives 41 / 59 / 40 / 49 / 53 / 314 for
baseq2 / CTF / Xatrix / Rogue / RA2 / tourney, which is not what the donor port
docs report. R-TOOL-2 requires the counter that produced each published figure
to ship in `tools/`; until it does, treat every count in this spec as indicative
and not as a pass/fail threshold. That counter does not exist — see R-TOOL-5.

The **file and line** counts have a method, and 1.4 states it because it was not
obvious enough to reproduce: a count is `find <donor> -name '*.[ch]'` over the
donor's own source, **including its vendored `shared/` subdirectory** and
excluding the build spill of R-PROV-4, the `asm_matching/` oracles and the
bundled upstream sources (`vanilla-q2-3.20-src/`, `q2ctf-1.02-src/`,
`gladq2-0.96-src/`, `extracted/`). RA2 is 39 + 5 and tourney 66 + 5; counted at
`maxdepth 1` alone they read 39 and 66 and the table looks wrong when it is
right. `ugladq2`'s 134 files are under `src/`, not at its root.

### 3.1 Why the two mod reconstructions are the pivot

`rocketarena2-public` and `osp-tourney` have both had **most of Q2PRO's 188
baseq2 game commits replayed onto them**, commit by commit, with mechanical
passes re-run rather than diffed (`doc/q2pro-port.md` in each). They are
therefore already on the same API, the same style, the same timer model and the
same savegame system as `q2pro/src/game`. That converts the hardest part of this
project — dragging two 1999 mods twenty years forward — from work into input.

"Most", not all. Counted from the `q2pro-commit:` trailers in the bundles of
§3.2: OSP carries 174 of the 188, RA2 185, CTF 194 (with fixups). One gap was
caught only after the fact, and its fix is a commit in the RA2 bundle:
`ra2: apply 'Fix broken viewangles with spectator 1' (missed during the
replay)`. The shortfall is not a defect in those trees — a donor with no
monsters and no single player has no site for a monster or SP fix — but
Colosseum keeps both (R-CORE-8), so for Colosseum the shortfall is work.
R-CORE-12 makes it a tracked ledger rather than a surprise.

`osp-tourney` matters twice over: OSP Tourney DM shipped **with** the Gladiator
Bot SDK as a donor, so its `q2pro-enhancements` branch already contains all six
`bl_*` files plus `botlib.h` and `anorms.h` **ported to the Q2PRO game API and
compiling**, with the SDK's own crash and overflow bugs fixed. Measured:

| file | osp-tourney (ported) | gladq2_src (1999) |
|---|---:|---:|
| `bl_main.c` | 1,433 | 1,356 |
| `bl_redirgi.c` | 1,078 | 1,000 |
| `bl_spawn.c` | 785 | 732 |
| `bl_cmd.c` | 434 | 449 |
| `bl_botcfg.c` | 433 | 411 |
| `bl_debug.c` | 280 | 262 |
| `botlib.h` | 325 | 324 |

That port is the starting point for §5.4, not `gladq2_src`.

### 3.2 The replay bundles are the provenance record

Three git bundles in the parent directory carry the replay history behind every
† row of §3:

| bundle | branches that matter |
|---|---|
| `q2pro-mission-pack-replay.bundle` | `baseq2`, `v_ctf`/`port_ctf`, `v_xatrix`/`port_xatrix`, `v_rogue`/`port_rogue` |
| `ra2-q2pro-port-replay.bundle` | `v_ra2`/`port_ra2` (tip `7322c9f`) |
| `osp-q2pro-port-replay.bundle` | `v_osp`/`port_osp` (tip `205a89c`), and `port_ra2` carried one commit further (tip `3f5ecb9`) |

All three share one root, tag `C0` — "Official Quake II 3.20/3.21 baseq2 game
source (normalised)". On it: a `v_<donor>` branch holding that donor's whole
divergence from id's source as a single commit, and a `baseq2` branch holding
Q2PRO's 188 game commits. `port_<donor>` is those 188 rebased onto the
`v_<donor>` delta. Every replayed commit carries a `q2pro-commit: <sha>`
trailer, so any line in any donor traces back to the real Q2PRO commit that
put it there.

* **R-PROV-1.** The three bundles are vendored into the Colosseum repository
  under `vendor/replay/`, and `doc/provenance.md` pins the SHA of every
  `port_*` and `v_*` branch used. They are inputs, not history to be
  regenerated.
* **R-PROV-2.** This history exists nowhere else. The live donor repositories
  squashed it — `osp-tourney@q2pro-enhancements` is three commits over its
  reconstruction `main`, `rocketarena2-public@q2pro-enhancements` four, and
  `q2pro/src/{ctf,xatrix,rogue}` arrived in two. Losing the bundles makes §7
  unauditable, so R-PROV-1 is not optional.
* **R-PROV-2a.** ~~The harness scratchpad no longer exists and its `rerere`
  resolutions went with it.~~ *Corrected in 1.2: it did exist, and was rescued
  on 2026-08-21.* The scratchpad was intact at the literal path on line 2 of
  `build_replay.sh` — 194 MB on WSL `/tmp`, one reboot from deletion. It is now
  archived at `../colosseum-harness-archive`, verified byte-identical by
  recursive md5, with a `MANIFEST.md` recording provenance. Rescued: the two
  `rr-cache` directories (**411 + 85 recorded conflict resolutions**),
  `commit_paths.txt` (188 lines, every SHA still resolving against `q2pro`),
  `style_residual.patch`, `mkosp.py`'s `osp/` input, id's five official source
  releases under `dl/x/`, the exact Artistic Style 3.1 binary, and the harness's
  own `TODO.md` — eight unfinished verification items, folded into §10 as
  R-VER-15. The replay working trees were deliberately not copied because the
  bundles reconstitute them; the test fixtures were not copied because they are
  reproducible.
* **R-PROV-2b.** The `rr-cache` is the only asset in that archive that is
  genuinely irreplaceable, because bundles carry objects and refs and no record
  of how any conflict was resolved. `rerere` is content-addressed by conflict
  hash, so the cache works against a repository reconstituted from the bundles.
  It is committed under `vendor/harness/` (R-TOOL-4). The `astyle` binary is
  **not** committed — see R-CONV-1a.
* **R-PROV-3.** `git diff baseq2 port_<donor>` **is** that donor's own feature
  set, already expressed in modern Q2PRO style against a spine tip
  (`5d180cb`) identical for all five donors. §7 rule 2 ("a donor owns its own
  feature") is therefore computed from that diff, not judged. `doc/reconciliation.md`
  is generated from it and then annotated, never hand-enumerated.
* **R-PROV-4.** Object files committed into `port_ra2` and `port_osp`
  (`debug/`, `release/`, `release-win32/`, …) are build spill and are excluded
  from every diff and every count.

### 3.3 The donor pins are re-pinned to this machine

1.0–1.3 were written against a WSL working copy under
`/mnt/c/Users/<user>/q2-dev`. Colosseum is built on a different host, and the
donor repositories there were re-committed — same content, same commit shapes,
new object names. Re-measured 2026-08-21: **every donor SHA this spec pinned is
unresolvable**, while every file and line count in §3 reproduces exactly. The
identifiers were stale, not the measurements.

| donor | 1.0–1.3 pin | 1.4 pin | evidence the content is the same |
|---|---|---|---|
| `q2pro` | `b1f2c18f` (`r1504-2231-gb1f2c18f`) | **`c751d316`** (`c751d316845ac4b27456d5cbd97f837979b24b26`) | `feature/mission-packs`, same branch, same three-commit shape: `bee7f7f0` *Add ctf, xatrix and rogue game libraries* → `3ffc4642` *Mission packs: portability and latent-bug fixes* → `c751d316` *README*. `src/{game,ctf,xatrix,rogue}` reproduce 72 / 31 / 79 / 92 files and 42,276 / 27,178 / 51,258 / 67,568 lines |
| `rocketarena2-public` | `c7d0f3a` | **`cd0708b`** (`cd0708b0d87a988df4bbc32ec4a474ce7563433d`) | `q2pro-enhancements`, still four commits over reconstruction `main`; 44 files / 30,192 + `shared/` |
| `osp-tourney` | `ab0e13a` | **`a8e30d0`** (`a8e30d01357d13dc359b47bdf59ddaae0582544a`) | `q2pro-enhancements`, still three commits over reconstruction `main`; 71 files / 49,034 + `shared/` |

* **R-PROV-5.** `doc/provenance.md` pins the SHAs of the 1.4 column, and the
  1.0–1.3 column is recorded beside them as **superseded identifiers, not
  history**. Two consequences:

  1. `c751d316` is a README-only commit; `3ffc4642` is the last commit that
     touches `src/`. Pin the branch tip for reproducibility and record which
     commit the source actually came from, because a future re-pin must be able
     to tell a content change from a documentation one.
  2. `789895b` — the RA2 commit R-RA-1a and R-OSP-10 name for the GameSpy
     removal — **does** resolve on this machine and is not re-pinned. Where a
     SHA still resolves it is left alone; only the three above moved.
* **R-PROV-6.** *The bundles did not move and that is the point.* Every ref of
  §3.2 resolves byte-exactly as recorded: root `C0` `7ceeeed`, spine tip
  `baseq2` `5d180cb` (the tip R-PROV-3 computes every donor diff against),
  `port_ra2` `7322c9f` in the RA2 bundle and `3f5ecb9` in the OSP bundle,
  `port_osp` `205a89c`, `port_ctf` `e94bbeb`, `port_xatrix` `2829dba`,
  `port_rogue` `2ebed93`. A bundle carries objects, so its names cannot drift the
  way a live branch's did. R-PROV-1's vendoring is therefore the durable half of
  the pin and the §3 table the perishable half — which is the argument for
  R-PROV-1, restated with evidence.
* **R-PROV-7.** The rescued archive verifies against R-PROV-2a's claims in full:
  `rr-cache/ra2replay/` holds **411** resolutions and
  `rr-cache/replay-missionpacks/` **85**, and all **188** SHAs in
  `tools-inputs/commit_paths.txt` still resolve against `q2pro` — those are
  upstream Q2PRO commits and survived the re-commit that moved the three pins
  above. The archive lives at `../colosseum-harness-archive`.

---

## 4. Licensing and attribution

* **R-LIC-1.** id-derived code (baseq2, CTF, Xatrix, Rogue, and everything
  descended from them) is GPL-2-or-later. The tree ships `LICENSE` with GPL-2
  and keeps every upstream copyright header intact.
* **R-LIC-2.** The Gladiator Bot game source licence forbids sale and requires
  that any derivative carry, in its documentation, the attribution block quoted
  in `gladq2_src/readme.txt` ("This product incorporates source code from the
  Gladiator bot… This program is in NO way supported by MrElusive… may NOT be
  sold in ANY form whatsoever"). `README.md` reproduces it verbatim.
* **R-LIC-3.** The Rocket Arena 2 Bot Support Routines are © 1998 David Wright,
  non-commercial use only. Colosseum does not carry that file (§5.5 replaces it
  with the full RA2), but if any line of it survives, the notice ships with it.
* **R-LIC-4.** Net effect: **Colosseum is non-commercial.** `README.md` states
  this in the first screen. No requirement in this spec may be satisfied in a
  way that conflicts with R-LIC-1..3.
* **R-LIC-5.** `doc/provenance.md` records, per source file, which donor it came
  from and under which licence.

---

## 5. Architecture

### 5.1 One library, all content, runtime selection

Gladiator's model — verified in `gladq2_src/g_local.h`, where `BOT`,
`BOT_IMPORT`, `TRIGGER_COUNTING`, `TRIGGER_LOG`, `FUNC_BUTTON_ROTATING`,
`LOGFILE`, `CLIENTLAG`, `VWEP`, `ZOID`, `CTF_HOOK`, `ROCKETARENA`, `CH`,
`XATRIX` and `ROGUE` are **all defined at once** — was: compile every donor in,
put the union of all items and spawn functions in one table, and switch
behaviour at runtime on cvars (`ctf`, `rocketarena`, `ch`, `xatrix`, `rogue`).
Colosseum keeps that model and drops the `#ifdef`s.

* **R-CORE-1.** One shared library. One `GetGameAPI`. No per-ruleset build
  variants, no per-mod `.so`.
* **R-CORE-2.** All content is compiled in unconditionally. `itemlist` is the
  union of baseq2 + Xatrix + Rogue + CTF + RA2 + tourney items; the spawn table
  is the union of all classnames. Runtime state, never the preprocessor, decides
  what spawns and how it behaves.
* **R-CORE-3.** No `#ifdef` gates a donor. Feature `#ifdef`s are permitted only
  for platform portability and for the two Q2PRO build switches
  (`USE_PROTOCOL_EXTENSIONS`, `USE_NEW_GAME_API`).
* **R-CORE-4.** Adding a ruleset must not require touching another ruleset's
  files. Ruleset-specific logic lives behind the dispatch of §5.3.

### 5.2 Source tree layout

`src/` **is** `q2pro/src/game`, file for file. That tree is the spine, and every
donor's change to a file it already owns is **merged into that file**, never
forked into a copy. Only files a donor adds that baseq2 has no counterpart for
go into a subfolder named after the donor.

```
colosseum/
  SPECS.md  README.md  LICENSE  Makefile  meson.build  config.h
  doc/    reconciliation.md cvars.md commands.md entities.md
          botlib-contract.md provenance.md regression.md
  inc/    shared/ common/ format/        # vendored q2pro engine headers, verbatim
  src/                                   # = q2pro/src/game, merged
    g_local.h  g_main.c  g_spawn.c  g_save.c  g_ptrs.c  g_ptrs.h  genptr.py
    g_utils.c  g_phys.c  g_func.c  g_target.c  g_trigger.c  g_misc.c  g_turret.c
    g_items.c  g_weapon.c  g_combat.c  g_cmds.c  g_svcmds.c  g_chase.c
    g_ai.c  g_monster.c  m_move.c
    m_actor  m_berserk  m_boss2  m_boss3  m_boss31  m_boss32  m_brain  m_chick
    m_flipper  m_float  m_flyer  m_gladiator  m_gunner  m_hover  m_infantry
    m_insane  m_medic  m_mutant  m_parasite  m_soldier  m_supertank  m_tank
                                    # all 22 baseq2 monster TUs, .c + .h
    p_client.c  p_hud.c  p_view.c  p_weapon.c  p_trail.c  m_player.h
    shared/    m_flash.c  shared.c        # vendored from q2pro/src/shared
    ctf/       g_ctf.c  g_ctf.h  p_menu.c  p_menu.h
    xatrix/    m_boss5.c  m_fixbot.c/.h  m_gekk.c/.h  m_gladb.c  m_soldierh.h
    rogue/     g_newai.c  g_newdm.c  g_newfnc.c  g_newtarg.c  g_newtrig.c
               g_newweap.c  g_sphere.c  dm_ball.c  dm_tag.c
               m_carrier.c/.h  m_stalker.c/.h  m_turret.c/.h
               m_widow.c/.h  m_widow2.c/.h
    arena/     arena.c  arena.h  maploop.c  menu.c  menu.h
               ra2menus.c  ra2stats.c  ra2stats.h
    tourney/   p_menu.c  p_menu.h
               osp_main.c  osp_cmds.c  osp_config.c  osp_detect.c  osp_display.c
               osp_hiscore.c  osp_hook.c  osp_maps.c  osp_menus.c  osp_observe.c
               osp_players.c  osp_plist.c  osp_runes.c  osp_stats.c/.h
               osp_teams.c  p_camera.c  sl_write.c  stdlog.c
    bot/       bl_main.c/.h  bl_spawn.c/.h  bl_cmd.c/.h  bl_redirgi.c/.h
               bl_botcfg.c/.h  bl_debug.c/.h  botlib.h  anorms.h
               p_menulib.c/.h  p_botmenu.c/.h   # the tree engine + the bot menu
```

Menus are **per ruleset** (R-MENU-1), so each donor's engine lives in that
donor's subfolder under its own filename, and colliding symbols take the donor
prefix of §7 rule 4:

| file | engine | symbols |
|---|---|---|
| `src/ctf/p_menu.c/.h` | Threewave `pmenu_t` (`arg` on the handle) | `ctf_PMenu_*` |
| `src/tourney/p_menu.c/.h` | OSP `pmenu_t` (`arg` per entry) | `osp_PMenu_*` |
| `src/arena/menu.c/.h` | RA2 `qmenu_t`, doubly-linked, dynamic | `ra_*`/`qmenu_*` |
| `src/bot/p_menulib.c/.h` | Gladiator `menu_t` tree, submenus and bitmaps | `Quake*Menu` |

The two `p_menu.c` files have incompatible `pmenu_t` layouts and the same
basename; the subfolder keeps them apart and the prefix keeps their symbols
apart. `dm` and `sp` use the `src/bot/` tree engine, which the bot menu needs
anyway. There is no `src/ch/` — Colored Hitman is out of scope (N7).

* **R-CORE-5.** `src/` mirrors `q2pro/src/game`: same 72 file names, no
  additions, no removals. A file that no donor touches is byte-identical to its
  q2pro counterpart. Every deviation is one row in `doc/reconciliation.md`.
  *Clarified in 1.4:* "no additions, no removals" scopes to the **top level of
  `src/`**. The subfolders of §5.2 and `src/shared/` are additions by
  construction and are governed by R-CORE-7, which is the requirement that says
  what may live in one. Read otherwise, R-CORE-5 forbids the tree layout in the
  section that defines it.
* **R-CORE-6.** Exactly one `g_local.h`, in `src/`. It is the only place
  `edict_t`, `gclient_t`, `client_persistant_t`, `client_respawn_t`,
  `level_locals_t` and `game_locals_t` are defined, and it is the merged union
  of every donor's fields.
* **R-CORE-7.** A donor's version of a file baseq2 owns is **merged into
  `src/<file>`**. There is no `src/ctf/g_items.c`, no `src/rogue/p_weapon.c`, no
  second `p_client.c`. A subfolder holds only files with no baseq2 counterpart.
* **R-CORE-8.** *Monster deletions are not replayed.* **Three** donors dropped
  the whole monster set, not two: `osp-tourney` ships 71 files and omits 44
  baseq2 files, every monster except the shared `m_move.c`; RA2 omits them and
  `m_move.c` with them; and `q2pro/src/ctf` — 31 files — omits them too, along
  with `g_turret.c`. Importing any of those trees **must not** carry the
  deletion. All 22 baseq2 monster translation units, all 4 Xatrix-only monsters,
  all 5 Rogue-only monsters, `g_ai.c`, `g_monster.c`, `m_move.c`, `g_turret.c`
  and Rogue's `g_newai.c` hint-path code stay in the tree and stay linked,
  because Colosseum supports the **full single-player and co-op experience**
  (`g_ruleset sp`) alongside the arena rulesets. (`m_move2.c` is *not* in that
  list — see R-MP-5.) Three consequences, each of which the donors show:

  1. **Dead functions are live again.** Measured from the donors'
     `q_unused` tags: OSP retired `HelpComputer`, `PlayerSort`, `ai_run_melee`,
     `ai_run_missile`, `ai_run_slide` and the `enemy_vis`/`enemy_range` pair;
     RA2 retired `CheckPowerArmor`, `ClientTeam`, `Cmd_Kill_f`, `IsFemale`,
     `IsNeutral`, `M_ReactToDamage`, `SelectSpawnPoint`, `TossClientWeapon`,
     `ai_checkattack`, `ai_run_slide`, plus its own `TeamFromNode` and
     `loc_CanSee`. Every one of those except the last two is live in Colosseum
     and must not be tagged `q_unused` or dropped.
  2. **Stub files do not come across.** `osp-tourney/g_monsters.c` is 161 lines
     of `SP_monster_* → G_FreeEdict` stubs whose own header says the filename
     is invented; it exists only to keep the spawn table resolvable in a tree
     with no monsters. It, and any donor's equivalent, is deleted — the real
     `SP_monster_*` are back.
  3. **Deleted spawn rows and prototypes come back.** The donors pruned the
     spawn table as well as the code; `monster_makron` is the case the OSP port
     names explicitly. Every classname a donor removed from `spawns[]` is
     restored, and R-BASE-1/R-MP-1 are the checks.

  A fourth consequence is large enough to be its own requirement: R-CORE-12.
* **R-CORE-9.** Engine headers under `inc/` are vendored verbatim from
  `q2pro/inc` and never edited; `config.h` carries the build switches, the way
  `osp-tourney/config.h` does. The source commit is recorded in
  `doc/provenance.md`.
* **R-CORE-10.** Divergence inside a shared file is resolved by **runtime
  gating**, exactly as the original Gladiator game library did: both code paths
  are compiled in, and the ruleset dispatch (R-MODE-5) or the content-layer cvar
  (`xatrix->value`, `rogue->value`) chooses between them. Each shared file is
  merged once, gated once, and gets a row in `doc/reconciliation.md` naming the
  gate and the behaviour on each side.

  The merge load is **five-way**, not two-way, and this is the largest single
  risk in the project. Measured 2026-08-20 in diff lines — CTF, Xatrix and Rogue
  against `q2pro/src/game`; RA2 and tourney against the replay bundles' `baseq2`
  tip, which is the same spine:

  | file | ctf | xatrix | rogue | ra2 | osp | Σ |
  |---|---:|---:|---:|---:|---:|---:|
  | `p_client.c` | 522 | 79 | 427 | 730 | 1991 | **3749** |
  | `g_spawn.c` | 395 | 421 | 1118 | 297 | 634 | **2865** |
  | `g_items.c` | 272 | 267 | 1087 | 189 | 589 | **2404** |
  | `g_local.h` | 108 | 44 | 425 | 130 | 1193 | **1900** |
  | `g_ai.c` | — | 6 | 580 | 450 | 735 | **1771** |
  | `p_weapon.c` | 50 | 418 | 894 | 128 | 256 | **1746** |
  | `g_cmds.c` | 145 | 84 | 72 | 265 | 715 | **1281** |
  | `g_func.c` | 3 | 168 | 764 | 170 | 174 | **1279** |
  | `p_hud.c` | 55 | 18 | 47 | 555 | 487 | **1162** |
  | `g_monster.c` | 2 | 135 | 273 | 105 | 552 | **1067** |
  | `g_weapon.c` | 17 | 590 | 28 | 67 | 282 | **984** |
  | `g_misc.c` | 48 | 303 | 152 | 208 | 244 | **955** |
  | `g_combat.c` | 78 | 9 | 388 | 154 | 234 | **863** |
  | `m_soldier.c` | — | 1251 | 639 | — | — | **1890** |
  | `m_medic.c` | — | — | 1208 | — | — | **1208** |
  | `m_gunner.c` | — | — | 510 | — | — | **510** |
  | `p_view.c` | 77 | 18 | 145 | 97 | 209 | **546** |
  | `g_turret.c` | — | — | 184 | 46 | 193 | **423** |
  | `g_utils.c` | 15 | — | 178 | 80 | 77 | **350** |
  | `g_phys.c` | 21 | 21 | 184 | 66 | 71 | **363** |
  | `g_trigger.c` | — | 107 | 113 | 72 | 64 | **356** |
  | `g_target.c` | 4 | 93 | 60 | 92 | 99 | **348** |
  | `m_move.c` | — | 38 | 301 | — | 34 | **373** |
  | `g_main.c` | 69 | 30 | 72 | 103 | 688 | **962** |
  | `g_save.c` | 9 | 13 | 77 | 34 | 7 | **140** |
  | `g_svcmds.c` | — | — | — | 32 | 56 | **88** |
  | `g_chase.c` | 60 | — | — | — | 166 | **226** |
  | `m_player.h` | 1 | — | — | 16 | 16 | **33** |
  | `p_trail.c` | — | — | — | 28 | 28 | **56** |

  Twenty-nine shared files need an n-way merge; a further 24 Rogue-modified and
  7 Xatrix-modified monster translation units are covered by R-CORE-11.
  `g_ptrs.c` is excluded — it is generated (R-SAVE-2). This table is regenerated
  by `tools/divergence.py` (R-TOOL-2) rather than transcribed, and the generated
  copy in `doc/reconciliation.md` is authoritative if the two disagree.
* **R-CORE-11.** Monster *frame tables* and `mmove_t` data are content, not
  behaviour: where Xatrix or Rogue changed a monster's animation set or attack
  table, both tables ship and the gate selects at spawn time, never mid-move.
  A live `mmove_t` pointer must stay valid across a cvar change, so the gate is
  ~~read at `monster_start`~~ *— corrected in 1.8: `monster_start` is too late.*
  Every `SP_monster_*` assigns its `mmove_t` tables and AI hooks and *then* calls
  `walkmonster_start()` → `monster_start()`; in `m_gunner.c` the assignments are
  some twenty lines ahead of the call. A gate placed in a spawn function would
  read 0 every time, always select baseq2, and never crash. The latch is in
  **`ED_CallSpawn`**, immediately before the spawn function runs — which also
  generalises correctly, since every entity then carries a flavour rather than
  only monsters. Latched into the entity, and not re-read. This covers
  24 Rogue-modified and 7 Xatrix-modified monster translation units — the
  largest are `m_medic.c` (rogue 1208), `m_soldier.c` (xatrix 1251, rogue 639),
  `m_gunner.c` (510), `m_chick.c` (353), `m_boss2.c` (298), `m_flyer.c` (266),
  `m_hover.c` (247), `m_brain.c` (xatrix 230), `m_tank.c` (193).
* **R-CORE-11b.** *Implemented in 1.9, and the mechanism has one hard
  constraint.* Ground Zero replaces the evasion of six of baseq2's monsters —
  chick, gunner, infantry, brain, soldier, medic — swapping each per-monster
  `X_dodge` and its `X_duck_*` callbacks for the shared `M_MonsterDodge` with
  generic `monster_duck_*` and a sidestep. Berserk needs no gate: baseq2's has no
  evasion at all, so Ground Zero's is a pure addition there.

  baseq2's set is reintroduced from `q2pro/src/game` under a `bq2_` prefix so
  both are distinct symbols, and `SP_monster_X` selects on
  `content_flavour & CONTENT_ROGUE`.

  **The gate must be an `if`/`else` with two literal assignments.** `genptr.py`
  builds `save_ptrs[]` by scanning the *source text* for `= &name`, so
  `= cond ? &a : &b` registers neither table and a token-pasting macro registers
  a fragment. Either produces a library that plays correctly and then fails to
  reload a savegame — which is why this is a requirement and not a style note.
  R-SAVE-2's generator constrains the shape of every runtime gate that selects
  between two saved pointers.

  Verified by observation (`sv ruleset`, R-VER-18), because nothing else can see
  it: `base1` 17 monsters on baseq2's dodge at `rogue 0` and 17 on Ground Zero's
  at `rogue 1`; `badlands` 58 on baseq2's, which is Reckoning no longer
  inheriting Ground Zero's AI; `rmine1` 54 on Ground Zero's, and at `rogue 0`
  47 on baseq2's with 7 remaining on Ground Zero's — correctly, since those are
  its own stalker, carrier, widow and turret plus berserk, none of which has a
  baseq2 fallback.

  *The frame tables are gated too, as of 1.10, and 1.9's "eleven tables across
  five monsters" was a mismeasurement.* Counting changed tables from `git diff`'s
  `@@` hunk-header labels names the symbol *preceding* each hunk, not the changed
  one. Re-measured by extracting each table body and comparing: **18 differ**,
  splitting into pure additions (`X_frames_jump`, `soldier_frames_blind`,
  `medic_frames_call` — unreachable without Ground Zero's AI), bookkeeping (a
  `monster_done_dodge` callback that clears a bit nothing sets; the duck tables'
  switch to generic callbacks, which is a strict refactor that stops the bounding
  box shrinking cumulatively), and **five genuine replacements across four
  monsters** — `gunner_frames_attack_grenade`, `infantry_frames_attack1`,
  `soldier_frames_attack6`, `soldier_move_death4`, `medic_frames_attackCable`.
  All five are gated at all nine assignment sites, `save_ptrs[]` carries all 11
  `bq2_` pairs, and savegames round-trip in every configuration.

  `tools/gates.py` enforces it: a table with a `bq2_` counterpart must be gated
  at **every** site, and no `currentmove` may be assigned through a ternary. See
  `doc/reconciliation.md` R-34 and R-35 — the latter records that the first
  version of that check passed its own negative control, because it derived
  "is this table paired?" from the gate it was checking for.
* **R-CORE-11a.** *Shared monster infrastructure needs a different mechanism.*
  The spawn-time latch has no purchase on the files every monster calls every
  frame, and those diverge too: `g_ai.c` (rogue 580), `m_move.c` (xatrix 38,
  rogue 301), `g_monster.c` (rogue 273), `g_phys.c` (rogue 184). There is no
  `monster_start` to latch at inside `SV_movestep` or `ai_run`. So the latched
  gate is stored **on the entity** as a content flavour at spawn, and
  `g_ai.c`/`m_move.c`/`g_monster.c`/`g_phys.c` read that field, never a cvar.
  One field, set once, read everywhere; a cvar change mid-map cannot reach a
  monster already spawned. Each of the four files gets a
  `doc/reconciliation.md` row naming the field and every read site.

  *Implemented in 1.7.* The field is `edict_t.content_flavour`, carrying
  `CONTENT_XATRIX` and `CONTENT_ROGUE` as two independent bits per Q13, latched
  in `monster_start()` immediately after the R-MODE-7 monster gate, and given a
  savegame descriptor because a monster reloaded without it would silently change
  flavour mid-game. `sv ruleset` (R-VER-18) reports the latched counts, which is
  how the mechanism is verified rather than assumed: on `base1` with both layers
  on, all 17 live monsters carry both bits, and the counts survive a save/load
  round-trip. **No file reads the field yet** — the four that will
  (`g_ai.c`, `m_move.c`, `g_monster.c`, `g_phys.c`) are merged later in Phase 2,
  and their rows are written then.
* **R-CORE-12.** *Replay coverage is tracked, and its gaps are work.* No donor
  received all 188 Q2PRO commits (§3.1). Because Colosseum keeps the monsters
  and the campaigns that the donors dropped, a commit a donor skipped for want
  of a site usually **has** a site here. `doc/replay-coverage.md` (D9) is one
  row per (donor, skipped commit), derived from the `q2pro-commit:` trailers in
  the bundles, with a verdict: *re-apply*, *not applicable*, or *superseded*.
  Measured, the set skipped by both RA2 and tourney is `Avoid generating missing
  savegame pointer.`, `Fix nofriendlyfire being applied to AI in coop`,
  `Fix nofriendlyfire flag not working correctly on easy skill`,
  `Remove commented out monster code.` and `Use initializer for aim vector.`;
  tourney additionally skipped `Fix annoying FOV change when exiting SP level.`,
  `Fix client disconnect in ss_pic state.`, `Fix out of array access.`,
  `Re-introduce savegame and loadgame menus.`, `Reduce status bar string
  duplication.`, `Remove power cubes at end of unit.`, `Replace strstr() with
  strchr().`, `Require C99 semantics for ‘bool’.` and `Use ‘menu_loadgame’
  inside baseq2 game library.`; RA2 additionally skipped `Fix broken viewangles
  with ‘spectator 1’.` (later applied as its own commit), `Make more game
  definitions static (monsters).`; CTF additionally skipped `Fix crashes due to
  G_PickTarget() returning NULL.`. A ledger row with no verdict blocks the phase
  that imports that donor.
* **R-CORE-13.** *Linkage is decided by the union of all donors' call sites.*
  Q2PRO made a number of baseq2 functions `static` once nothing outside their
  file called them; each donor then had to undo that for the ones **it** calls —
  tourney for `EndDMLevel`, `DeathmatchScoreboard`, `Cmd_InvUse_f`, `Cmd_Kill_f`,
  `InitClientPersistant`, `Use_Quad`, `Use_Invulnerability` and `dm_statusbar`;
  RA2 for `InitClientPersistant`, `InitClientResp`, `PlayersRangeFromSpot`,
  `SV_AddGravity` and `Weapon_Generic`. In Colosseum the answer is the union:
  `static` survives only where **no** donor calls across the file boundary.
  Deciding this by hand per file is how a donor's call site gets silently
  dropped, so it is mechanised (R-TOOL-1) and forward declarations are made to
  agree with the definitions in the same pass.
* **R-CORE-14.** *Entity flag bits are allocated once, in one table.* Measured
  2026-08-20, the donors alias two bits under four names, and with `OBSERVER`
  live in the Gladiator reconstruction the collision is real rather than dormant:

  | bit | claimed as | by |
  |---|---|---|
  | `0x10000` | `FL_OBSERVER` | Gladiator reconstruction (`g_local.h:117`, 13 files with live `#ifdef OBSERVER`, `p_client.c` alone has six sites) |
  | `0x10000` | `FL_BOTCLIENT` | `osp-tourney/g_local.h:86` — the flag that persists across `ClientConnect`, letting bots past the server password |
  | `0x10000` | `FL_OSP_BOT` | `osp-tourney/g_local.h:1539` — the same bit recovered from the mod's own code under a second name |
  | `0x2000` | `FL_BOT` | `osp-tourney/g_local.h:82` |
  | `0x2000` | `FL_OSP_NOCMD` | `osp-tourney/g_local.h:1538` — so `OSP_serverbotsRemove` reads as "destroy every client with `FL_OSP_NOCMD`" and *means* "destroy every bot" |

  One name per bit, `FL_OBSERVER` relocated to a free bit, the duplicate aliases
  removed rather than kept as synonyms, and the whole allocation in one table in
  `g_local.h` with a row per bit in `doc/reconciliation.md`. Four names for two
  bits is how a merge silently makes every bot an observer.

  *Re-measured in 1.7 against baseq2 + xatrix + rogue, and it is worse than the
  table above: **six** names on those two bits, and Ground Zero holds both.*

  | bit | also claimed as | by |
  |---|---|---|
  | `0x2000` | `FL_MECHANICAL` — "mechanical, use sparks not blood" | Ground Zero |
  | `0x10000` | `FL_NOGIB` — "vaporized by a nuke, drop no gibs" | Ground Zero |

  Ground Zero lands in **Phase 2** and the bot layer in Phase 6, so Rogue is the
  incumbent and the bot and observer names are the ones that move. The
  consequence of not noticing is specific rather than vague: a bot flag sharing
  `0x2000` makes every bot bleed sparks instead of blood, and one sharing
  `0x10000` makes every bot drop no gibs. There are **14 free FL_ bits**, so
  nothing needs squeezing — the allocation only needs making once, and Phase 2
  makes it, reserving `FL_OBSERVER`, `FL_BOT`, `FL_BOTINPUT` and
  `FL_OLDORGNOTSET` at `BIT(17)`..`BIT(20)` before any code wants them.
  `FL_BOTCLIENT`, `FL_OSP_BOT` and `FL_OSP_NOCMD` are **not** defined at all,
  per this requirement's own instruction to remove the duplicate aliases rather
  than keep them as synonyms.

### 5.3 Ruleset model

The 1999 build used three independent booleans (`ctf`, `rocketarena`, `ch`) and
then had to police them: `InitGame` in `gladq2_src/g_save.c` force-resets and
calls `gi.error` on `ctf && ra`, `ctf && ch` and `ra && ch`. That is a
constraint expressed as an error message. Colosseum expresses it as a type.

* **R-MODE-1.** One latched cvar `g_ruleset` selects the primary ruleset:
  `dm` | `ctf` | `arena` | `tourney` | `sp`. Default `dm`. Invalid values fall
  back to `dm` with a warning; they never abort startup.
* **R-MODE-2.** Legacy cvars are honoured as aliases, evaluated once at
  `InitGame`, so existing configs and 1999 documentation keep working:
  `ctf 1` → `ctf`; `rocketarena 1` → `arena`. A legacy `ch 1` is accepted and
  ignored with one warning, because Colored Hitman is out of scope (§1.2).
  Two conflicting aliases produce a warning naming both, and the first in the
  order `ctf`, `arena`, `tourney` wins. OSP's `m_mode` continues to select the
  mode of play **within** the `tourney` ruleset, and it selects among **four**,
  not three (R-OSP-12). `g_ruleset` and `m_mode` are orthogonal: the ruleset
  picks the code path, `m_mode` picks the match structure inside it.
* **R-MODE-3.** `xatrix` and `rogue` are independent latched content layers,
  valid with every ruleset. Both may be on at once. The union content is always
  spawnable; the cvars gate *behaviour* (Ground Zero's plat2/danger-area logic,
  DM rules, `gamerules`) exactly as the 1999 build gated it — measured: 14 uses
  of `rogue->value`, 1 of `xatrix->value` in `gladq2_src`.
* **R-MODE-4.** Modifiers: `teamplay`, `hook`, `runes`/`techs`, and bots. Each
  modifier declares which rulesets accept it; an unsupported combination is
  refused at `InitGame` with one message naming the ruleset, the modifier and
  what was done instead. It never calls `gi.error`. (`ch` and `sp_dm` were
  modifiers in 1.0–1.1 and are struck in 1.2 — see §1.2.)
* **R-MODE-5.** Ruleset-specific behaviour is reached through **one** gate per
  concept, not scattered `if (ctf->value)` tests. The gate's *form* is sized to
  what the ruleset does: a **dispatch-table hook** where a ruleset *replaces*
  the baseq2 implementation, and a **named predicate** or a comment-fenced
  inline test where it only *adjusts* it. The hook count therefore falls out of
  the merge rather than being fixed here — the list below is the **audit of
  where gates live**, not a mandate that all twenty-five be function pointers.

  1.0–1.1 mandated twenty-five function-pointer hooks. Measured against prior
  art, that has no precedent: the only ruleset vtable in any Quake II tree is
  Rogue's `dm_game_rt`, which has **12** hooks, has been copied unchanged into
  five trees since 1998, and has exactly **two** rows ever filled (Tag,
  Deathball) covering only spawn/score/death/damage. It has no default row —
  `memset` to zero plus a null check at each of ~21 call sites. Everything
  outside those twelve points is gated in `q2pro-ng` and
  `quake2-rerelease-dll` by a single named predicate — `bool
  G_TeamplayEnabled(void) { return ctf.integer || teamplay.integer; }`, 20 call
  sites — or by an inline test behind a donor-name comment fence, with **zero
  `#ifdef`** in the whole game tree. Colosseum follows that shape.

  *Resolved in 1.6, and the prediction held.* Phase 1 produced **6 dispatch
  rows** — `CheckRules`, `EndLevel`, `ScoreboardMessage`, `BeginIntermission`,
  `SelectSpawnPoint`, `ClientPlaced` — and **5 named predicates**:
  `G_MonstersAllowed`, `G_IsCampaign`, `G_TeamplayEnabled`, `G_BotsAllowed`,
  `G_SavegamesAllowed`. `sp` fills no rows and inherits all of them, because
  baseq2 already branches internally on `deathmatch`/`coop` and moving those
  branches into replaced rows would be churn with no donor to justify it.
  R-MODE-6 is implemented once, in a `GATE()` macro, rather than as a null check
  at each call site — `dm_game_rt` has no default row and ~21 hand-written null
  checks around it. `doc/reconciliation.md` R-7 has the reasoning.

  The prohibition half of the Phase 1 exit is mechanised: `tools/gates.py` fails
  the build on a ruleset cvar tested at a call site, and keeps a ratcheted census
  of the 110 inherited `deathmatch`/`coop` sites that R-CORE-5 requires to stay.

  Gate locations to be accounted for:
  `Init`, `Shutdown`, `SpawnEntities`, `ClientConnect`, `ClientBegin`,
  `ClientUserinfoChanged`, `ClientDisconnect`, `ClientCommand`, `ClientThink`,
  `ClientBeginServerFrame`, `ClientEndServerFrame`, `PutClientInServer`,
  `SelectSpawnPoint`, `ClientPlaced`, `PlayerDie`, `ClientObituary`,
  `CheckDMRules`, `ScoreboardMessage`, `BeginIntermission`, `EndDMLevel`,
  `PickupItem`, `Precache`, `AllowDamage`, `TeamOf`, `SameTeam`, `RunFrame`.

  The list above has **26** entries, and has since 1.1. 1.0–1.3 called
  `ClientPlaced` the twenty-fifth hook and §9 Phase 1 said "the 25 dispatch
  hooks"; both are corrected in 1.4. The "twenty-five function-pointer hooks" of
  the paragraph above stands as written — it records what 1.0–1.1 *mandated*,
  which is the thing this requirement replaced, not a count of the list.
  `ClientPlaced` — "the player now has a final origin" — is the twenty-sixth and
  it exists because the donors prove the need. Q2PRO's spawn floor-clip
  trace sits in `PutClientInServer`; RA2 does not place the player there at all,
  so its port put the trace in `move_to_arena()`; tourney's port put it in
  `ClientBeginDeathmatch`. All three placement paths coexist in Colosseum, so
  anything that must run after placement hangs off one hook rather than being
  copied into three functions.
* **R-MODE-6.** A ruleset that does not implement a hook inherits the `dm`
  implementation. `dm` implements all of them.
* **R-MODE-7.** Composability matrix — the contract:

  | | dm | ctf | arena | tourney | sp |
  |---|---|---|---|---|---|
  | bots | yes | yes | yes | yes | no |
  | `xatrix` layer | yes | yes | yes | yes | yes |
  | `rogue` layer | yes | yes | yes | yes | yes |
  | `teamplay` | yes | implied | via teams | via `m_mode` 2 or 3 † | no |
  | `hook` | opt | yes | yes (grapple) | yes (OSP hook) | no |
  | `runes`/`techs` | no | yes (techs) | no | yes (runes) | no |
  | monsters | yes | yes (unproven — R-VER-2) | no | no | yes |
  | savegames | — | — | — | — | yes |

  *The monster row was measured in 1.6*, on `base1`, via `sv ruleset`
  (R-VER-18): `dm` 0 live monsters, `ctf` **19**, `arena` 0, `tourney` 0, `sp`
  17 plus 12 corpses. `deathmatch` is 1 in the first four, which is the point —
  the inherited `if (deathmatch->value) G_FreeEdict(self)` would have suppressed
  the monsters this row promises under `ctf`, so the row is only true because
  R-MODE-5's predicate replaced that test (`doc/reconciliation.md` R-6). Q14's
  "unproven rather than broken" is now "spawns; behaviour under `ctf` still
  unproven", which is what R-VER-2 is for.

  † `tourney` reaches team play by two routes. `m_mode 2` is team play proper;
  `m_mode 3` is 1v1, which is *also* two teams — of one player each, forced
  (`osp_main.c:479-484` sets `team_maxplayers` to 1 and re-registers it
  `CVAR_NOSET`). The team machinery, scoreboard and overtime rules are shared;
  what differs is the roster size and the queue. A reader who takes the row as
  "team play means `m_mode 2`" will wire 1v1 wrongly — see R-BOT-29.

### 5.4 Bot subsystem

This is the part that must mirror 1999 exactly, because the other side of the
contract is a binary.

#### 5.4.1 The ABI is frozen

* **R-BOT-1.** `botlib.h` defines the contract and is **versioned**, not frozen.
  ~~Frozen: no field may be added, removed, reordered or retyped.~~ *Amended in
  1.2.* The freeze's stated justification was that the other side of the contract
  is a 1999 binary; it is not — `gladiator-bot-restored` reconstructed the botlib
  source, so both sides are compiled together from one header (see R-BOT-4). What
  replaces the freeze: the interface still spans two repositories, so it stays
  stable by default and every change is deliberate, announced through
  `BotVersion` — already slot 0 of `bot_export_t`, so the handshake mechanism
  exists — and recorded in `doc/botlib-contract.md` with the version it landed
  in. A change that both sides do not agree on is a defect, not a version. This
  is what makes R-BOT-5's by-value `bsp_trace_t` and `MAX_NETNAME` 16 fixable
  rather than permanent. The contract's current shape: `bot_export_t`
  (20 slots, `BotVersion` … `Test`), `bot_import_t` (10 slots, `BotInput` …
  `DebugLineShow`), `bot_settings_t`, `bot_clientsettings_t`, `bot_input_t`,
  `bot_updateclient_t`, `bot_updateentity_t`, `bsp_trace_t`, `bsp_surface_t`,
  the `ACTION_*` flags, the `BLERR_*` codes (0–32), the `PRT_*`/`CMS_*`
  constants, the `LINECOLOR_*` values, and `MAX_NETNAME` 16 /
  `MAX_CLIENTSKINNAME` 128 / `MAX_FILEPATH` 144 / `MAX_CHARACTERNAME` 144.
  `osp-tourney`'s already-ported copy is the one to take — including the
  `const`-ification of the `PointContents` slot it already made, because
  `gi.pointcontents` is assigned straight into it. §7 rule 5 is amended to match:
  a reconciliation that needs `botlib.h` to change is still *suspect*, but it is
  no longer automatically wrong.
* **R-BOT-2.** The game exports nothing to the botlib except `bot_import_t`.
  The botlib is reached only through `bot_export_t`. No shared globals.
* **R-BOT-3.** Loading is **dynamic**, as in 1999: `dlopen`/`LoadLibrary` of a
  botlib named per bot in `bots.cfg`, resolving `GetBotAPI`, with the search
  order of `BotUseLibrary` (direct path, then `basedir` + `gamedir`), reference
  counting per library, and `BotUnloadAllLibraries` on `ShutdownGame`. A
  build option may additionally link `gladiator-bot-restored/botlib` statically;
  when it is on, the static brain is offered under a reserved name and dynamic
  loading still works.
* **R-BOT-4.** Bitness. `bot_export_t`/`bot_import_t` are pointer-bearing
  structs, so game and botlib must share word size, and `BotUseLibrary` reports
  a load failure with the reason and the bitness of both sides, refusing the bot
  rather than killing the server. ~~A 32-bit Colosseum can load the original
  1999 `gladiator.dll` / `gladi386.so`.~~ *Amended in 1.2: the 1999 binaries are
  not a target.* `gladiator-bot-restored` reconstructed the botlib source, so the
  brain is compiled from source for whichever platform the game is built for,
  and bitness is a build-matrix fact rather than a compatibility mission. Two
  measured caveats: `gladiator-bot-restored/Makefile` auto-selects
  `i686-w64-mingw32-gcc` on a WSL host with `/mnt/c` present, so a bare `make`
  yields 32-bit DLLs — pass `CC` explicitly. *Re-measured in 1.4:* the reference
  machine has no `/mnt/c`, so that arm no longer fires and a bare `make` yields
  a **native aarch64** brain (R-BUILD-6) — which is the correct default here, but
  it means the caveat now bites in the other direction: a Windows brain must be
  asked for, and the bitness of game and brain must be checked rather than
  assumed to follow from the host. And at 64 bits the botlib's
  `_Static_assert` layout guards are wrapped in `#if __SIZEOF_POINTER__ == 4`
  and go inert, so a 64-bit brain carries no layout verification. Loading a 1999
  binary remains *possible* on a 32-bit build and is neither required nor tested.
  (`gladi386.so` does not exist anywhere in the workspace.)
* **R-BOT-5.** `bsp_trace_t` is returned **by value** from the `Trace` slot.
  Both sides must agree on struct-return convention; on Windows the slot is
  declared with the same `q_gameabi` treatment Q2PRO applies to `gi.trace`.
  `BotLibImport_Trace` stays a translating wrapper (`trace_t` → `bsp_trace_t`,
  `edict_t*` → entity number, `int passent` bounds-checked against
  `game.maxentities`), and must tolerate a null `trace.surface`.

#### 5.4.2 Libvar contract

* **R-BOT-6.** `BotInitLibrary` pushes game state into the brain through
  `BotLibVarSet` before `BotSetupLibrary`, and the set is fixed:
  `maxclients`, `maxentities`, `max_aaslinks`, `max_bsplinks`,
  `max_levelitems`, `autolaunchbspc`, `dmflags`, `ctf`, `ch`*, `ra`, `xatrix`,
  `rogue`, `log`, `nochat`, `fastchat`, `altnames`, `rocketjump`,
  `forceclustering`, `forcereachability`, `forcewrite`, `nooptimize`,
  `framereachability`, `basedir`, `gamedir`, `cddir`, `usehook`, `laserhook`,
  `runes`, `techs`, `teamplay`, `teamplay_shell`, `assimilation`.

  \* `ch` is pushed as a constant `"0"`. Colored Hitman is out of scope (N7),
  but the brain reads the libvar, and sending zero costs nothing and keeps the
  set identical to what the reconstructed botlib expects.
* **R-BOT-7.** New rulesets map onto the **existing** libvars; no new libvar may
  be invented, because the brain would ignore it. `tourney` sets
  `teamplay`/`usehook`/`runes` as OSP's own bot glue does; `arena` sets `ra 1`.
  `doc/botlib-contract.md` records the full ruleset → libvar mapping and is the
  single authority for it.
* **R-BOT-8.** `basedir`, `gamedir` and `cddir` must resolve correctly under
  Q2PRO, where `gamedir` is a `CVAR_ROM|CVAR_SERVERINFO` cvar and `basedir` may
  not exist. Where they cannot be derived, Colosseum sets them from
  `FILESYSTEM_API_V1` (§5.7) and says so once at load.

#### 5.4.3 Game import redirection

The mechanism from `bl_redirgi.c`: `GetGameAPI` copies the engine's
`game_import_t` into a private `newgameimport`, then overwrites 20 slots of the
game's own `gi` with `Bot_*` interceptors. Everything the game does thereafter
is observed on the way through.

* **R-BOT-9.** `BotRedirectGameImport()` is called from `GetGameAPI`
  **immediately after** `gi = *import` and before anything else, matching 1999
  order. Nothing may read `gi` before it returns.
* **R-BOT-10.** Redirected slots and their purpose:

  | slot | interception |
  |---|---|
  | `cprintf`, `bprintf`, `centerprintf` | Text aimed at a bot becomes `BotConsoleMessage(CMS_NORMAL\|CMS_CHAT)`; humans see it unchanged |
  | `sound` | Becomes `BotAddSound` for the bots as well as reaching clients |
  | `modelindex`, `soundindex`, `imageindex` | Records name→index into local tables so `BotLoadMap` can hand the brain the whole index space |
  | `setmodel` | Same, for models set by entity |
  | `multicast` | Sniffs `svc_muzzleflash` out of the pending message and converts it, via the `muzzleflashinfo` table, into `BotAddSound` (and `BotAddPointLight`), then forwards |
  | `unicast` | **Suppressed** for `FL_BOT` recipients; a bot has no network connection |
  | `WriteChar/Byte/Short/Long/Float/String/Position/Dir/Angle` | Buffered into one staging message so `multicast`/`unicast` can read it before it goes out. `WriteDir` needs `anorms.h` |
  | `argc`, `argv`, `args` | Serve synthesised bot command arguments when a bot command is executing, and the engine's otherwise |

* **R-BOT-11.** The index tables must be sized from `game.csr` — Q2PRO's
  configstring remap — not from the 1999 constant 256.
  With `USE_PROTOCOL_EXTENSIONS` the limits are `MAX_MODELS` 8192,
  `MAX_SOUNDS` 2048, `MAX_IMAGES` 2048; without it, 256 each. The tables are
  allocated at `InitGame` after `game.csr` is chosen. An index past the end is
  dropped with one warning, never written.
* **R-BOT-12.** The `Bot_Write*` staging buffer is bounds-checked on every
  write. `MAX_NETWORKMESSAGE` is at least the engine's maximum message size, and
  an overflow drops the message with a warning instead of writing past the end.
  (The 1999 code range-checked the *values* but not the *cursor*.)
* **R-BOT-13.** Redirection must be transparent when no bot exists: with
  `botglobals.numbots == 0` the observable behaviour of every redirected slot is
  identical to calling the engine directly.

#### 5.4.4 Fake clients

* **R-BOT-14.** A bot is a real client slot: `G_SpawnClient` searches
  `game.maxclients-1 → 0`, i.e. **downward**, so bots take high slots and humans
  take low ones. `ClientConnect` is called with `inuse` false and set true after
  it returns; `FL_BOT` is cleared across `ClientConnect`/`ClientBegin`/
  `ClientDisconnect` so those paths do not recurse into bot handling.
* **R-BOT-15.** `BotMoveToFreeClientEdict` relocates a bot to another slot when
  a human needs its slot, and calls `BotMoveClient(old, new)` so the brain
  follows. If no slot is free the connection is refused, not stolen.
* **R-BOT-16.** Bot edicts additionally set `SVF_BOT` when
  `GMF_PROTOCOL_EXTENSIONS` is negotiated, so the engine and MVD spectators can
  tell bots from players. `SVF_NOBOTS`, where a donor uses it, is honoured.
* **R-BOT-17.** `CheckMinimumPlayers` maintains `minimumplayers` by adding and
  removing bots, and counts observers/spectators out — the fix uGladQ2 records
  for CTF (v0.98.2u) and RA2 (v0.98.1u).
* **R-BOT-18.** `AddBotToQueue`/`AddQueuedBots` defer spawning to frame start;
  no bot is created inside `SpawnEntities` or inside a `ClientConnect`.
* **R-BOT-19.** `BotDestroy` clears both `edict_t` and `gclient_t`, restores the
  `client` back-pointer, decrements `numbots`, releases the library reference
  and frees the slot — in that order.

#### 5.4.5 Frame loop

* **R-BOT-20.** `G_RunFrame` order is fixed, mirroring 1999:
  1. `AddQueuedBots()`
  2. `BotStartFrame(level.time)`
  3. entity loop (`old_origin` copy skipped for `FL_OLDORGNOTSET`; clients get
     `ClientBeginServerFrame`, everything else `G_RunEntity`)
  4. `BotUpdateEntity()` for every `inuse` edict without `SVF_NOCLIENT`
  5. per bot, in slot order: `BotUpdateClient` → `BotAI(FRAMETIME)` →
     `BotExecuteInput`
  6. `CheckMinimumPlayers()`
  7. `CheckDMRules()`

  Steps 4 and 5 must not interleave: the brain is entitled to a complete world
  snapshot before any bot thinks.
* **R-BOT-21.** `BotExecuteInput` converts `bot_input_t` to `usercmd_t` and
  calls `ClientThink` with `FL_BOTINPUT` set for the duration, so client code
  can tell a synthesised command from a network one. The `nocldouble` behaviour
  (two half-`msec` `ClientThink` calls per frame unless disabled, and the
  `ACTION_DELAYEDJUMP` interaction) is preserved.
* **R-BOT-22.** `FRAMETIME` is 0.1 s and Colosseum does not advertise
  `GMF_VARIABLE_FPS`. Variable server FPS with bots is out of scope; if
  `sv_fps` differs from 10 the game logs one warning at `InitGame`.
* **R-BOT-23.** Bot AI cost must be bounded: with 32 bots on a loaded map the
  bot section of `G_RunFrame` stays under half a 100 ms frame on the reference
  machine, or the shortfall is reported in `doc/regression.md`.

#### 5.4.6 Commands and configuration

* **R-BOT-24.** All Gladiator bot commands work, from console (`sv <cmd>`) and
  from a client where the 1999 build allowed it: `addbot`, `removebot`,
  `addrandom [n]`, `botpause`, `menu`, `modelindex`, `soundindex`,
  `imageindex`, `inventory`, `botlibdump`, `clientdump`, and the bot-issued
  `name`, `skin`, `gender`, `teamhelp`, `teamaccompany`, `checkpoint`, plus the
  GPS and macro commands `bl_cmd.c` grew in v0.93.
* **R-BOT-25.** `serveronlybotcmds` gates client access to bot commands, and
  **defaults to 1** (server only). The 1999 default exposed `bl_spawn.c`'s
  32-byte bot-name copies to clients; `osp-tourney`'s port doc flags exactly
  this. Every string copy on that path is bounded regardless.
* **R-BOT-26.** `bots.cfg` and every `bots/*.cfg` under the gamedir are parsed
  as in v0.92: name, skin, character file, character name; `AddRandomBot`
  returns success; `CheckForNewBotFile` picks up edits without a restart.
* **R-BOT-27.** Debug lines go through Q2PRO's `DEBUG_DRAW_API_V1` extension
  (`AddDebugLine`) when the engine offers it, and fall back to the 1999
  `bl_debug.c` beam-entity scheme when it does not. `DebugLineCreate`/`Delete`/
  `Show` keep their `bot_import_t` signatures either way, and the `LINECOLOR_*`
  values map to RGBA. `SetVisibleBoundingBox`/`ToggleVisibleBoundingBox` and the
  14-line bbox are kept.
* **R-BOT-28.** The bot menu is `p_botmenu.c` on the Gladiator `menu_t` tree
  engine (`p_menulib.c`), both taken from the reconstruction, both living in
  `src/bot/` (§5.2), with its 1999 structure intact: DM / CTF / RA2 / credits
  submenus, title bitmaps, `botctfteam`, `ra_playercycle`, `ra_botcycle`,
  minimum players, teamplay and dmflags editing, `rcon` password gating on
  `ToggleBotMenu`, and per-ruleset menu trees driven by `xatrix`/`rogue`.
  `osp-tourney` cannot supply this: it has no `p_botmenu.c` and no `p_menulib.c`,
  having moved its bot menu into `osp_menus.c`'s `Bot_Menu` on id's `PMenu`. It
  also dropped `botctfteam`, `ra_playercycle` and `ra_botcycle` as cvars, so
  those are re-registered here. The `menu` bot command, dropped from
  `osp-tourney/bl_cmd.c`, comes back with the menu (R-BOT-24).
* **R-BOT-29.** *The `TOURNEY` block becomes a runtime gate.* This is the largest
  single piece of work in §5.4 and 1.0–1.1 did not mention it. `osp-tourney`
  defines `TOURNEY` at `g_local.h:16` — **live**, not `#if 0`, and every `bl_*.c`
  includes `g_local.h` first, so the donor's own `//#define TOURNEY` lines are
  inert text. Seventeen blocks are active, and as shipped they apply
  library-wide. They must each become a branch on the active ruleset:

  | site | what it does when on |
  |---|---|
  | `bl_main.c` rune→tech translation | rewrites `bue.modelindex` to `TECH1..5_INDEX` for `IT_RUNE` items so the brain sees OSP runes as CTF techs |
  | `bl_main.c` libvar block | drives `usehook`/`laserhook` from `hook_enable`, `teamplay` from `m_mode == MODE_TEAM`, `runes` from `rune_stat` |
  | `bl_botcfg.c` ×2 | `botfile` → **`bots_botfile`**, default `botcfg/bots.cfg` |
  | `bl_spawn.c` minimum players | `minimumplayers` → **`bots_minplayers`**; count gated on `resp.entered == ENTERED_ENTERED`; arithmetic subtracts `bots_votedin`; a `bots_autoload == 4` arm and an `old_botcount` guard |
  | `bl_spawn.c` name collision | a duplicate bot name is auto-suffixed rather than refused |
  | `bl_spawn.c` `#ifndef TOURNEY` ×2 | **empties** `ShowLoadImage`/`RemoveLoadImage` |
  | `bl_redirgi.c` | `OSP_PrecacheCTFRunes`, already reduced to `#if 0` upstream |

  Neither state is acceptable library-wide: on, `dm`/`ctf`/`arena` lose
  `minimumplayers` and `botfile` and the SDK's loading-screen swap; off, OSP's
  runes stop reaching the brain. The cvar names stay **per ruleset** (R-OSP-11).
  The block also carries externs that resolve only when the tourney translation
  units are linked — `m_mode`, `hook_enable`, `OSP_serverbotsRemove`,
  `bots_votedin`, and `rune_stat`, which is a plain `int` in `osp_main.c` whose
  lifecycle `OSP_endClean` owns. Each needs a ruleset-neutral accessor.
  The `m_mode` accessor is a **value** accessor, not a predicate. `bl_main.c:1035`
  compares `m_mode == MODE_TEAM` (`0x02`) to drive the brain's `teamplay` libvar,
  and that comparison is exactly right as written: 1v1 is two teams of one, the
  brain has no ally, and `teamplay 0` is the correct answer for `m_mode 3`. An
  accessor that generalises the test to "is this a team mode" would set
  `teamplay 1` in 1v1 and give every duel bot an imaginary teammate. Preserve the
  comparison; abstract only where the value comes from.
* **R-BOT-30.** Fixes to take from `osp-tourney`'s `bl_*` verbatim, each verified
  present: `TECH5_INDEX 255` (**both** Gladiator trees carry `#define
  TECH4_INDEX 254` followed by `#define TECH4_INDEX 255`, and `TECH5_INDEX` is
  consumed inside `TOURNEY` blocks — so the Gladiator `TOURNEY` path has never
  compiled anywhere); `gi.centerprintf(ent, "%s", buf)`; the three `userinfo`
  NUL terminations in `BotCmd`; the `(int)` casts on `DF_ENTCLIENT` in three
  `gi.dprintf` calls, which is a real LP64 fix; `game.csr.playerskins` in place
  of `CS_PLAYERSKINS`; `MAX_INFO_STRING-1` on the userinfo `memcpy`; and
  `game.maxclients` for `maxclients->value`. Two changes there are **policy, not
  correctness**, and are adopted deliberately: `serveronlybotcmds` default `0`→`1`
  (which R-BOT-25 already requires) and botlib `log` default `1`→`0`. Three
  identifiers in that tree are reconstruction-invented and documented as such —
  `choice`, `nbots` in `AddRandomBot`, and `old_botcount` — and
  `bl_spawn.c`'s `CheckMinimumPlayers` has mangled indentation that will read as
  merge damage. `bl_debug.c` is code-identical to the 1999 original and transfers
  unchanged; so do `anorms.h` and `bl_botcfg.h`.

### 5.5 What replaces Gladiator's arena

* **R-ARENA-1.** `gladq2_src/g_arena.c` (1,069 lines, "Rocket Arena 2 Bot
  Support Routines", © David Wright) is **not carried across**. Its role is
  taken by `rocketarena2-public/arena.c` (2,226 lines) and its siblings — the
  full mod, with teams, per-arena settings, voting, locking, competition mode,
  score-by-damage, `maploop.c` rotation and the local round log.
* **R-ARENA-2.** Everything the bots need from the old file is re-provided
  against the real RA2: per-client arena assignment, observer handling for bots,
  `ra_playercycle`, `ra_botcycle`, telefragging during countdown, and bot
  placement into the correct arena queue. The uGladQ2 fixes v0.97u–v0.98.3u are
  the acceptance list for this requirement, item by item, in
  `doc/reconciliation.md`.
* **R-ARENA-3.** Bots in `arena` are never parked in a waiting room; they are
  noclip observers, as v0.92 established.
* **R-ARENA-4.** `MAX_ARENAS` 32 and `MAX_TEAMS` 256 come from RA2 and are not
  reduced.

### 5.6 Menu subsystem

Four donor menu engines, three paradigms, all of which claim the same client
input:

| donor | type | shape |
|---|---|---|
| Q2PRO CTF `p_menu.c` | `pmenu_t` | static array, per-entry callback, `arg` on the handle |
| OSP `p_menu.c` | `pmenu_t` | same, but `arg` per entry and no handle `arg` |
| RA2 `menu.c` | `qmenu_t` | doubly-linked list, dynamic, `menuselect_t` returning int |
| Gladiator `p_menulib.c` | `menu_t`/`menuitem_t` | linked list **with submenus and bitmaps**, hierarchical navigation, highlight timing |

* **R-MENU-1.** Menus are **per ruleset**. ~~One menu core, the richest of the
  four, `p_menulib`'s model rewritten.~~ *Amended in 1.2.* All four donor engines
  ship, exactly one is active for the running ruleset, and each keeps its own
  donor behaviour unchanged: Threewave's `pmenu_t` under `ctf`, OSP's under
  `tourney`, RA2's `qmenu_t` under `arena`, and the Gladiator `menu_t` tree —
  which the bot menu is written against — under `dm`, `sp` and the bot menu
  everywhere. File and symbol layout is in §5.2. This is the fourth exemption to
  §7 rule 6, for the same reason as the other three: the donors' menus are part
  of the ruleset a player is choosing, not a generic mechanism.
* **R-MENU-2.** ~~`pmenu_*` and `qmenu_*` are thin adapters over one core.~~
  *Struck in 1.2.* There is no core to adapt to; each engine is itself. What the
  adapter scheme was for — the two incompatible `pmenu_t` layouts, `arg` on the
  handle in Threewave's and `arg` per entry in OSP's — is now handled by keeping
  the two engines in separate translation units with prefixed symbols (§5.2),
  which is strictly simpler than reconciling the layouts.
* **R-MENU-2a.** The cost of R-MENU-1 is that four engines contend for one
  client input channel, and nothing structural prevents two being open at once.
  R-MENU-3 is therefore the load-bearing requirement in this section, not a
  detail, and it is enforced rather than assumed: the menu owner is a single
  field in `gclient_t`, every engine's open path goes through one function that
  closes the incumbent first, and a per-donor menu walk is a release check
  (§10).
* **R-MENU-3.** Exactly one menu owner per client at a time, recorded in
  `gclient_t`. Opening a menu while another is open closes the first and logs
  nothing. Entering the chase camera closes any open menu — the v0.93 fix.
* **R-MENU-4.** Menu input is consumed in `ClientThink` before weapon and
  movement handling, and the client's own `showscores` is forced off for bots
  in `ClientEndServerFrame` (the v0.91 fix).
* **R-MENU-5.** The layout string built by the core respects `MAXSTATUSBAR`
  (1400) and truncates on a whole item, never mid-token.

### 5.7 Engine features used

* **R-ENG-1.** `apiversion` is `GAME_API_VERSION`. `USE_NEW_GAME_API` is on, so
  the library targets `GAME_API_VERSION_NEW` (3302) with `gclient_new_t` /
  `pmove_new_t`.
* **R-ENG-2.** `G_FEATURES` advertises at least
  `GMF_PROPERINUSE|GMF_WANT_ALL_DISCONNECTS|GMF_ENHANCED_SAVEGAMES`, plus
  `GMF_CLIENTNUM` (bots make correct `clientNum` matter), and
  `GMF_PROTOCOL_EXTENSIONS` when `sv_features` offers it and
  `g_protocol_extensions` is set. `game.csr` is set from the outcome, as
  `q2pro/src/game/g_main.c` does.
* **R-ENG-3.** `GetGameAPIEx` is implemented. `GAME_API_VERSION_EX` 3 gives
  `local_sound`, `get_configstring`, `clip`, `inVIS`, `GetExtension` and
  `TagRealloc`; Colosseum uses `get_configstring` for index recovery after a
  savegame load and `local_sound` for per-client audio.
* **R-ENG-4.** `FILESYSTEM_API_V1`, when present, is used for **all** botlib
  file access — `.aas` files, `bots.cfg`, character files, weapon/item/sound
  configs — so bot data can live inside `.pak`/`.pkz` archives. The 1999 `stdio`
  path remains as fallback. This is a capability the original never had.
* **R-ENG-5.** `DEBUG_DRAW_API_V1`, when present, backs R-BOT-27.
* **R-ENG-6.** `CanSave` returns false for every ruleset except `sp`, and
  false whenever a bot exists. `PrepFrame` and `RestartFilesystem` are
  implemented; `RestartFilesystem` invalidates cached bot file handles.
* **R-ENG-7.** Colosseum must load and run in **stock** Q2PRO. Any behaviour
  requiring an engine change is an open question in §12, not a patch.
* **R-ENG-8.** `GAMEVERSION` is `"colosseum"` and feeds the `gamename`
  serverinfo cvar, as `gi.cvar("gamename", GAMEVERSION, CVAR_SERVERINFO |
  CVAR_LATCH)` in every donor. Each donor ships its own — the mission packs keep
  `"baseq2"` exactly as id's sources had it, tourney uses
  `"OSP Tourney DM v(2.75)"` — so this is a merge decision with a row, not a
  default. `config.h`'s `VERSION` string is `"colosseum"` too, replacing the
  donors' `"osp-q2pro"`/`"ra2-q2pro"`, and `CPUSTRING` is derived from the build
  rather than hardcoded `"x86"` as both donors leave it.

### 5.8 Savegames

* **R-SAVE-1.** Q2PRO's portable savegame system is used unchanged: field
  descriptors plus the `save_ptrs[]` table from `g_ptrs.c`, generated by
  `genptr.py`. `GMF_ENHANCED_SAVEGAMES` is advertised.
* **R-SAVE-2.** *The generator constrains the code it scans.* `genptr.py` finds
  saved function and `mmove_t` pointers by matching `= &name` in the **source
  text**, so any construct that hides the name hides the pointer: a ternary
  (`= c ? &a : &b`) registers neither, and a token-pasting macro registers a
  fragment. A gate selecting between two saved pointers is therefore written as
  an `if`/`else` with two literal assignments — see R-CORE-11b, which is the
  first place it mattered. The failure mode is why this is stated: the library
  builds, runs and plays correctly, and fails only when a savegame is reloaded.

  `genptr.py` runs as a **build step** over the whole merged tree,
  and the mission-pack fix is carried: it skips inactive `#ifdef` blocks and
  block comments so it stops emitting pointers for functions compiled out.
  A stale `g_ptrs.c` is a build failure, not a warning.
* **R-SAVE-3.** Every field added to `edict_t`, `gclient_t`,
  `client_persistant_t`, `client_respawn_t`, `level_locals_t`,
  `monsterinfo_t` or `game_locals_t` by any donor gets a descriptor, or an
  explicit `doc/reconciliation.md` entry saying why it must not persist. The
  mission-pack port's own additions are inherited: Quad-Fire, `max_magslug`,
  `max_trap`, and Ground Zero's Double/IR/Nuke/Tracker timers.
* **R-SAVE-3a.** *A row that is present can be wrong rather than missing.* The
  descriptor's type macro must agree with the member's C type. Q2PRO's portable
  savegame turned id's "preserved by default" into "preserved only if listed",
  and nothing warns either way: the mission-pack port lost `gravityVector` and
  the whole `blindfire` set from Ground Zero to a missing row, and separately
  round-tripped four `monsterinfo_t` frame counters through the *float* macro
  against `int` members, so a counter of 3600 did not survive. `attack_finished`
  is the one that legitimately keeps the float macro, because Ground Zero really
  left that member `float`; tourney's `pers.spectator` is the mirror case, kept
  `int` on purpose because tourney counts speed-cheat strikes in it and tests
  `>= 3`, which a C99 `bool` would saturate at 1. Presence **and** type
  agreement are checked mechanically (R-TOOL-1), per struct, on every build.
* **R-SAVE-4.** Bot state is **not** saved. `bot_state_t`, `bot_globals_t`,
  library handles and queued bots are excluded from every descriptor set. On
  `ReadLevel`, any client slot marked as a bot in the loaded data is released.
* **R-SAVE-5.** A savegame written by one ruleset and read under another is
  refused with a clear message, not loaded.

### 5.9 Build

* **R-BUILD-1.** `make` produces `game<cpu>.so` natively; `make windows` cross
  compiles `gamex86.dll` and `gamex64.dll` with MinGW. All four
  debug/release configurations build clean.
* **R-BUILD-2.** Warnings are on and clean: `-Wall -Wextra`, plus
  `_FORTIFY_SOURCE=2` and `-fstack-protector-strong` — the posture
  `osp-tourney`'s port adopted once the mod's own overflows were fixed. New
  warnings are not tolerated, and 1.5 makes that mechanical: **`-Werror`**.

  *Measured in 1.5, and the requirement as written was unreachable.* On the
  pristine spine, `-Wall -Wextra` produces **622** warnings — none from `-Wall`,
  all from `-Wextra`. Upstream never met this bar because q2pro builds with
  `-Wall` and a curated set (`-Werror=vla`, `-Wformat-security`,
  `-Wpointer-arith`, `-Wstrict-prototypes`), **never `-Wextra`**. And R-CORE-5
  forbids editing the code that warns. So three suppressions, each measured,
  each recorded with the count it silences:

  | flag | count | why it is not sloppiness |
  |---|---:|---|
  | `-Wno-unused-parameter` | 594 | Quake II's callbacks have signatures fixed by their function-pointer types — `touch` takes a plane and a surf, `die` takes an inflictor and a point — so most implementations ignore some. Fixing them means ~600 `(void)` casts in code R-CORE-5 requires to stay byte-identical |
  | `-Wno-sign-compare` | 28 | every one was read; all are int-versus-unsigned in a bounded loop or a guarded check. `g_save.c:678` tests `len < 0` **before** the unsigned comparison, which is correct defensive code the warning cannot see. Zero real defects |
  | `-Wno-missing-field-initializers` | 2 | clang only, and both are the `{ NULL }` sentinels in `itemlist[]` — the index-0 placeholder and the end-of-list marker that the item loop and `auditems.py` walk to |

  What survives is worth keeping: `-Wextra` minus those three still gives
  `-Wmissing-field-initializers` on new code, `-Wtype-limits` and
  `-Wempty-body`, and `-Werror` makes any new warning fatal — which is the half
  of this requirement that was always the point. A phase that adds new code
  re-checks whether a suppression can be narrowed to the inherited translation
  units.
* **R-BUILD-3.** A `meson.build` mirroring `q2pro/src/ctf/meson.build` lets the
  tree build in place under `q2pro` as `-Dmission-packs=…`-style target, sharing
  `game_shared_src` and `engine_inc`. The standalone Makefile stays primary.
  The model file exists (`q2pro/src/game` has none — take `src/ctf`'s), and
  `meson 1.3.2` on the reference machine clears `q2pro/meson.build`'s
  `meson_version: '>= 0.59.0'`, so this requirement is exercisable.
* **R-BUILD-4.** No generated file is committed except `g_ptrs.c`, and that one
  must be reproducible by running `genptr.py`.
* **R-BUILD-5.** ~~32-bit builds are supported and tested, because they are the
  only ones that can load the 1999 botlib binaries.~~ *Rationale void in 1.2 —
  the 1999 binaries are not a target (R-BOT-4).* Replaced by **full platform
  support**: `win32` and `win64` PE, and 32- and 64-bit ELF. ~~Measured on the
  reference machine 2026-08-20.~~

  *Re-measured in 1.4, on a different machine.* 1.0–1.3's matrix was measured on
  an x86-64 WSL host. The reference machine is **aarch64** (R-BUILD-6), so three
  of the four rows changed state and the primary native target is one the matrix
  did not contain. Re-measured 2026-08-21:

  | target | state | gating? |
  |---|---|---|
  | **ELF aarch64** | **the primary native target. Built and run** (1.5): clean under both `gcc` 13.3.0 and `clang` 18.1.3, debug and release, and it passes the R-VER-17 smoke test in a stock `q2proded` | **yes** |
  | win32 PE | `i686-w64-mingw32-gcc` 13. **Built** (1.5): `gamex86.dll`, PE32, clean. Not run | **yes** |
  | win64 PE | `x86_64-w64-mingw32-gcc` 13. **Built** (1.5): `gamex86_64.dll`, PE32+, clean. Not run | **yes** |
  | ELF x86-64 | `x86_64-linux-gnu-gcc` 13, same 13.3.0 as native. **Built** (1.5): `gamex86_64.so`, clean. Not run | **yes** |
  | ELF i386 | `i686-linux-gnu-gcc` 13. **Built** (1.5): `gamei386.so`, ELF 32-bit, clean. Not run | **yes** |

  *The last two rows changed state while 1.4 was being written*, and the change
  is worth recording because it retires a diagnosis that had been wrong twice
  over. 1.2 called ELF i386 "currently impossible — `Scrt1.o` and `crti.o` are
  absent; needs `gcc-13-multilib` and `libc6-dev-i386`". That was an **x86-host**
  diagnosis: on an ARM host, multilib is not the mechanism at all — i386 is a
  cross target, not a second ABI, and `gcc-i686-linux-gnu` brings its own cross
  libc. Both rows were verified 2026-08-21 by cross-linking a `-shared -fPIC`
  probe that returns a struct by value (R-BOT-5's ABI concern) and reports
  `sizeof(void*)`, clean under `-Wall -Wextra`; `file` confirms
  *ELF 64-bit x86-64* and *ELF 32-bit Intel 80386*.

  **All five rows are now gating**, and each must reach "built and run" or be
  struck — a toolchain that links is not a target that runs, and the distance
  between those two is precisely where this requirement's original rationale
  rotted. 1.4 is the second time that has happened to this one requirement, so
  the rule is now explicit: **a row states the host it was measured on, and
  "links" is never recorded as "runs."** Running an x86 or PE build is the user's
  step (R-VER-9); the reference machine can produce every artifact but can
  natively execute only the aarch64 one.
* **R-BUILD-6.** *The reference machine is named, and its toolchain is an
  inventory rather than an assumption.* Measured 2026-08-21:

  | | |
  |---|---|
  | host | Raspberry Pi 5 Model B Rev 1.1, `aarch64`, Linux 6.8.0-1061-raspi, Ubuntu 24.04 |
  | native compilers | `gcc` 13.3.0, `clang` 18.1.3 (`aarch64-unknown-linux-gnu`) |
  | cross compilers | `i686-w64-mingw32-gcc` and `x86_64-w64-mingw32-gcc` 13-win32 (PE); `x86_64-linux-gnu-gcc` and `i686-linux-gnu-gcc` 13 (ELF) — all four verified to link a `.so`/`.dll` from this host |
  | build systems | `make` 4.3, `meson` 1.3.2, `ninja` 1.11.1 |
  | style tool | `astyle` **3.1** — the version R-CONV-1a pins |
  | x86 emulation | `qemu-x86_64` and `box64`, both present |
  | absent | nothing R-BUILD-5 needs. **No x86 or PE target can be *executed* here** — only cross-built (R-VER-9) |

  Two findings that change requirements rather than merely recording state:

  1. **The astyle pin is satisfied natively.** The distribution's `astyle` is
     3.1, the same version as the binary rescued per R-PROV-2a — and its output
     is **byte-identical** to the rescued x86-64 binary's, verified 2026-08-21
     over six files under mech.py's exact flag set (`g_ai.c`, `p_client.c`,
     `g_items.c`, `g_local.h`, tourney's `osp_main.c`, and Gladiator's
     `p_menulib.c`, which has never been through any pass). R-CONV-1a's "same
     tools" is therefore met without emulation, and R-PROV-2b's decision not to
     commit the binary is confirmed rather than merely tolerated. The two
     emulators stay listed as the fallback that this check made unnecessary.
  2. **Both compilers are present, so both are gating.** `-Wall -Wextra` clean
     under `gcc` *and* `clang` (R-BUILD-2), on every gating row of R-BUILD-5.
* **R-BUILD-8.** *The engine loads the game library from `homedir` and the
  install `libdir` — never from `basedir`.* Measured 2026-08-21 against
  `q2pro/src/server/game.c`: `SV_LoadGameLibrary` is called with the home
  directory and with the compiled-in `libdir`, and tries
  `<dir>/<gamedir>/game<CPUSTRING><LIBSUFFIX>` then the same under the base game
  directory. The **filesystem** search path *does* include `basedir/<gamedir>`,
  which is what makes this confusing: assets are found there and the library is
  not. A `colosseum/` directory beside the assets with the library in it
  produces `Failed to load game library` with four `Can't access` lines, none of
  them naming the place a user would have put it.

  So the install instruction is `<homedir>/colosseum/game<cpu>.<ext>`, and
  `README.md` says so. This is a documentation requirement, not an engine
  change (R-ENG-7).
* **R-BUILD-7.** *Phase 0 needs an engine, and the engine is a prerequisite
  rather than a deliverable.* Colosseum is a game library; nothing in §10 can be
  observed without a Q2PRO that loads it, and the reference machine has **no
  built q2pro** — no `builddir*`, no binary. So Phase 0 carries one task that is
  not about Colosseum at all: a native `q2proded` for the host, built by the user
  (R-VER-9) from the pinned `q2pro` tree with the `meson` of R-BUILD-6. Until it
  exists, Phase 0's exit criterion cannot be evaluated, only its build half.

---

## 6. Requirements catalogue

§5 states requirements that are inseparable from the architecture. This section
holds the rest: parity, behaviour and quality obligations, grouped by area. The
IDs are stable; renumbering is forbidden. A requirement that is dropped is
struck through and kept.

### 6.1 R-BASE — baseq2 parity

* **R-BASE-1.** Every one of baseq2's 157 spawn classnames spawns and behaves as
  in `q2pro/src/game`.
* **R-BASE-2.** All 37 baseq2 cvars are registered with the same names, defaults
  and flags.
* **R-BASE-3.** All 27 baseq2 client commands work, including `help`, `give`,
  `god`, `notarget`, `noclip`, `kill`, `wave`, `players`, `inven`, `invuse` and
  the `weapnext`/`weapprev`/`weaplast` family.
* **R-BASE-4.** The frame-number timer model is used throughout. No float
  `level.time` comparison may be introduced where q2pro uses a frame counter.
* **R-BASE-5.** `Q_rand()`, `Q_atoi()`, `Q_snprintf`, `Q_strlcpy` and
  `Q_strlcat` are used, never `rand()`, `atoi()`, `sprintf` or `strcpy`.
* **R-BASE-6.** `game.maxclients` is used, never `maxclients->value`, in any hot
  path.
* **R-BASE-7.** `Swap_Init` is not called; q2pro's `shared.c` is
  little-endian-only and has no equivalent.

### 6.2 R-SP — single player and co-op

* **R-SP-1.** `g_ruleset sp` plays the baseq2 campaign end to end, including
  intermissions, `target_changelevel` chaining, the help computer, and
  `spawnflags` difficulty filtering (`SPAWNFLAG_NOT_EASY` … `NOT_COOP`).
* **R-SP-2.** With `xatrix 1`, *The Reckoning* campaign plays end to end; with
  `rogue 1`, *Ground Zero* does, including `gamerules`, hint paths
  (`InitHintPaths`), the DM ball and tag rulesets, and the sphere weapons.
* **R-SP-3.** Co-op works: `coop 1` with the baseq2 and both mission-pack
  campaigns, with co-op respawn, shared keys and `SPAWNFLAG_NOT_COOP` honoured.
* **R-SP-4.** Savegames and loadgames work for every campaign combination of
  `xatrix` × `rogue` (R-SAVE-1..5).
* **R-SP-5.** All 31 monster translation units link and function: 22 baseq2,
  4 Xatrix-only (`m_gekk`, `m_fixbot`, `m_boss5`, `m_gladb`), 5 Rogue-only
  (`m_carrier`, `m_stalker`, `m_turret`, `m_widow`, `m_widow2`). Measured
  against `q2pro/src/{game,xatrix,rogue}` on 2026-08-20. `m_move2.c` is not one
  of them and is not built (R-MP-5).
* ~~**R-SP-6.** `sp_dm` — uGladQ2 v0.99u's "Singleplayer Deathmatch" campaign —
  is available as a `dm` modifier.~~ **Struck in 1.2** (N8). It was the only
  configuration in which monsters and bots occupied the same level, and the
  Gladiator botlib never navigated around monsters. The `sp` ruleset is
  unaffected and R-SP-1..5 stand.

### 6.3 R-MP — mission packs

* **R-MP-1.** All 173 Xatrix and all 192 Rogue spawn classnames spawn.
* **R-MP-2.** All Xatrix weapons and items work: Ion Ripper, Phalanx, Trap,
  Quad-Fire, `max_magslug`, `max_trap`.
* **R-MP-3.** All Rogue weapons and items work: chainfist, disruptor, ETF
  rifle, plasma beam, prox launcher, tesla, nuke, defender/hunter/vengeance
  spheres, doppelganger, Double Damage, IR goggles, Tracker.
* **R-MP-4.** Rogue's `SVF_DAMAGEABLE` keeps the bit the mission-pack port moved
  it to; it must not collide with the engine's `SVF_PLAYER` (bit 3).
* **R-MP-5.** `m_move2.c` ships as a reference file and is **not built**.
  ~~It ships and is built.~~ *Amended in 1.1; Q9 is closed.* Measured against
  `q2pro/src/rogue`: `m_move2.c` defines `M_CheckBottom`, `SV_movestep`,
  `M_ChangeYaw`, `SV_StepDirection`, `SV_FixCheckBottom`, `SV_NewChaseDir`,
  `SV_CloseEnough`, `M_MoveToGoal` and `M_walkmove` — every one with external
  linkage — and `m_move.c` defines the same nine, four of them still external.
  Building both is a guaranteed multiple-definition link error, which is exactly
  the link error the 1999 Gladiator readme documents around this file. It is not
  a byte-identical copy but a **stale** one: it predates `IsBadAhead` and the
  `static` pass. Rogue's own Makefile does not build it, `q2pro/src/rogue/meson.build`
  documents why, and Colosseum follows. The finding is recorded in
  `doc/reconciliation.md`.
* **R-MP-6.** `huntercam`, `strong_mines`, `randomrespawn`, `g_showlogic`,
  `gamerules` and `sv_stopspeed` are registered with Rogue's defaults and flags.

### 6.4 R-CTF — Threewave CTF

* **R-CTF-1.** All CTF entities, both flags, all five techs, the grapple and
  the CTF scoreboard work as in `q2pro/src/ctf`.
* **R-CTF-2.** `capturelimit` ends a match, and it works when `fraglimit` is
  unset — the uGladQ2 v0.97u fix.
* **R-CTF-3.** The offhand hook is available and switchable with `ctf_hook`
  (uGladQ2 v0.97u); observers cannot fire it. The `laserhook` variant is
  selectable and reported to the brain through the `laserhook` libvar.
* **R-CTF-4.** `botctfteam` assigns bots to a team, and it is editable from the
  bot menu (v0.92).
* **R-CTF-5.** Threewave has no baseq2 `spectator` flag — observers are
  `CTF_NOTEAM` players — so spectator handling and the savegame descriptors
  point at the fields Threewave uses, as the mission-pack port established.
* **R-CTF-6.** Player id display is on by default and shows the team icon
  (uGladQ2 v0.98.2u).
* **R-CTF-7.** The 1999 CTF bot-model bug fixed in `ClientUserInfoChanged`
  (v0.95) and the CTF userinfo bug (v0.93) must not reappear; both get a
  regression entry.

### 6.5 R-RA — Rocket Arena 2

* **R-RA-1.** RA2 v2.25 behaviour from `rocketarena2-public`: up to 32 arenas,
  256 teams, per-arena settings, the full voting set, locking, competition mode,
  score-by-damage, `maploop` rotation, the RA2 menus and the round log.
* **R-RA-1a.** The GameSpy `gstats` SDK does **not** come back, and this too is
  inherited (§0 rule 6): `rocketarena2-public@789895b` removed `gstats.c`,
  `gbucket.c/.h`, `darray.c/.h`, `hashtable.c/.h`, `md5c.c`/`md5.h` and
  `nonport.c/.h`, and with them `ValidatePlayer()`, which read a `pid`/`pass`
  pair out of client userinfo, and the per-round `NewGame()` that resolved a
  dead hostname and grew an on-disk retry cache nothing drained.
  `ra2stats.c/.h` writes the same counters as one JSON object per round to
  `ra2stats.jsonl`, under `statsfile`/`statsname`. `gslog.c` is untouched and is
  not GameSpy code despite the prefix. Colosseum keeps this state, and `netlog`
  stays the only socket user in the tree.
* **R-RA-2.** All 47 RA2 cvars and all 39 RA2 client commands work.
* **R-RA-3.** The arena state machine keeps its seven states (`WARMUP`,
  `COUNTDOWN`, `FIGHTING`, `ROUNDEND`, `INTERMISSION`, `RESULTS`, `NEXTROUND`).
* **R-RA-4.** The uGladQ2 arena fixes are all present, each as its own
  regression entry: arena menu on connect and on `inven` (v0.97u); scoreboard
  scoped to the current arena; no more than two teams per arena; no over-filling
  an arena; 5-second countdown after population; `ra_fastswitch`; no starting a
  match in an arena that has one; winners kept between matches and spawned into
  the next when bot-cycling (v0.98u); auto-start idling below two teams;
  auto-start when 2+ teams observe and nobody plays; `noitems 1` default;
  players into arena 1 as observers on DM maps; observers ignored for spawn
  points (v0.98.1u); telefragging during countdown; new bots initialised into
  the selected arena's queue; observers ignored against `minimumplayers`
  (v0.98.2u); arena menu and bot assignment survive a level change (v0.98.3u).
* **R-RA-5.** `ra_playercycle` and `ra_botcycle` behave as v0.93 defined, and
  `ra_winner`/`ra_time` live in `gclient_t`.
* **R-RA-6.** Monsters do not spawn in `arena`. The classnames still exist
  (R-CORE-2); the ruleset suppresses them.

### 6.6 R-OSP — OSP Tourney DM

* **R-OSP-1.** Tourney v2.75 behaviour from `osp-tourney`: the match system,
  referee and vote system, **all four modes of `m_mode` (R-OSP-12)**, the runes,
  the OSP hook, the observer and camera system, hi-scores, the accuracy/stats
  commands, the map system and the menu engine.
* **R-OSP-2.** All 259 tourney cvars and all 137 tourney client commands work.
* **R-OSP-3.** The NetGames USA subsystem does **not** come back. Events are
  written locally as one JSON object per line to `<statsname>`, controlled by
  `statsfile`, `statsname`, `stats_logchat` and `stats_logallpickups`, and the
  Standard Log (`sl_log_method`) keeps its own file writer in `sl_write.c`; both
  can run at once. No `system()` call, no cleartext password prompt to clients.
  **Inherited, not work** (§0 rule 6): `osp-tourney@a8e30d0` already did it —
  `nglog.c`, `ngmark.c`, `q2log.c`, `global.h` and the RSA `md5c.c`/`md5.h` are
  gone, along with 17 cvars, the `_ngws_client_id` client command, the
  `ngWorldStats_password` field on `client_respawn_t`, both menu rows, the
  `who_paused == -3` state and the scoreboard banner; `osp_stats.c/.h` is the
  replacement. What is left for Colosseum is to *not undo it*, and to keep the
  three specific holes it closed shut: the `system()` call at every map end with
  two unquoted 1024-byte `sprintf` paths, the stuffcmd that asked every
  connecting client for its own account password in cleartext, and
  `q2log_clientid_cmd`'s unterminated `strncpy(dst, gi.argv(1), 16)`. This is a
  regression entry (R-VER-1), not a task.
* **R-OSP-4.** Every bug the OSP port fixed stays fixed and gets a regression
  entry. Named explicitly, because each is client-reachable: the referee-status
  grant from a long address; the `strcpy` of the client address in
  `ClientConnect` and `OSP_getPlayerAddr`; the SIGFPE when the last human leaves
  a server with bots; `OSP_specbot_vote`'s off-by-one bot index; the userinfo
  key-order auto-ban; the two kick paths that unicast a corrupt message
  (`OnBotDetection`, `OSP_speedDetect`); `OSP_teamskin_cmd`'s bot arm writing
  the caller's userinfo; the `FL_OSP_BOT`/`FL_BOT` mismatch with no
  client check; `OSP_startDemos`'s `cids[2]` overrun.
* **R-OSP-5.** `g_spawn.c`'s `extern int botglobals;` bug — a local `extern`
  declaration that zeroed `bot_globals_t.numbots` — must not exist in any form.
  `botglobals` is declared once, in `bl_main.h`, and included.
* **R-OSP-6.** Tourney's five extra entity spawn keys — `botlib`, `name`,
  `skin`, `charfile`, `charname` — are accepted, and live in `g_spawn.c`'s
  `temp_fields[]` where Q2PRO moved that table.
* **R-OSP-7.** Stat slot allocation is **per ruleset**, from one table in
  `doc/reconciliation.md`, enforced by a compile-time check. Measured
  2026-08-20, the donors' claims overlap almost completely above slot 15:

  | slots | baseq2 / `dm` | CTF | RA2 | tourney |
  |---|---|---|---|---|
  | 0 | `STAT_HEALTH_ICON` (`shared.h`) | shared | **also `STAT_SKIN_ICON`** | shared |
  | 1–15 | `STAT_HEALTH` … `STAT_FLASHES` (`shared.h`) | shared | shared | shared |
  | 16 | `STAT_CHASE` (`shared.h`) | shared | `STAT_COUNTDOWN` | shared |
  | 17 | `STAT_SPECTATOR` (`shared.h`) | `CTF_TEAM1_PIC` | `ARENASTATUS` | shared |
  | 18–19 | **`STAT_TIMER2_ICON`, `STAT_TIMER2`** (`p_hud.c`) | `CTF_TEAM1_CAPS`, `TEAM2_PIC` | `ROUNDINFO`, `LINEPOSITION` | — |
  | 20–21 | — | `TEAM2_CAPS`, `FLAG_PIC` | `CTF_ID_VIEW`, `QUEUE1` | — |
  | 22–26 | — | `JOINED_TEAM1_PIC` … `CTF_TECH` | `QUEUE2`, `SHOWQUEUE`, `QUEUE1_ICON`, `QUEUE2_ICON`, **`TIMER2_ICON`** | `RUNE_RESIST` … `RUNE_VAMPIRE` |
  | 27–30 | — | `CTF_ID_VIEW`, `CTF_MATCH`, `ID_VIEW_COLOR`, `CTF_TEAMINFO` | **`TIMER2`** (27) | `OSP_LAYOUT1` (27), popup menus (28), **`TIMER2_ICON`** (29), **`TIMER2`** (30) |

  Two rows in bold are new in spec 1.1 and they change the requirement.

  *baseq2 does not stop at 17.* `q2pro/src/game/p_hud.c` defines
  `STAT_TIMER2_ICON 18` and `STAT_TIMER2 19` for the second powerup timer. The
  1.0 table showed 18–21 as free for baseq2, which is wrong, and RA2 already
  claims slot 0 for `STAT_SKIN_ICON` on top of `STAT_HEALTH_ICON`, so "0–15 are
  unclaimable" was already untrue of a shipped donor.

  *The second powerup timer is the hard case.* It is a **baseq2** mechanic,
  written by the shared `G_SetStats` in the single merged `p_hud.c` (R-CORE-7),
  and each donor had to find it two free slots inside its own crowded range:
  baseq2 18/19, RA2 26/27 (moved off 19/20 after the slot-20 collision the
  checker missed), tourney 29/30. One `#define` cannot serve three rulesets, so
  `STAT_TIMER2_ICON`/`STAT_TIMER2` — and any future shared mechanic in the same
  position — must resolve through the ruleset's slot map, not a preprocessor
  constant.

  The conflict is only a *runtime* one where two claimants can be active at
  once, and the rulesets are mutually exclusive (R-MODE-1), so CTF, RA2 and
  tourney may each keep their own numbering. The requirement is therefore:
  1. slots 1–15 are universal and unclaimable. Slot 0 is universal too, and
     RA2's `STAT_SKIN_ICON` overload is a reconciliation row, not a precedent;
  2. 16 and 17 are claimable only by a ruleset that does not use baseq2's
     `STAT_CHASE`/`STAT_SPECTATOR` semantics — Threewave qualifies, since it has
     no baseq2 spectator flag (R-CTF-5), and RA2 and tourney qualify through
     their own observer systems;
  3. 18 upward is ruleset-private, one numbering per ruleset, declared in one
     header — **including `dm`'s own**, which owns 18/19;
  4. a **shared mechanic** that needs a private slot in every ruleset declares
     it once per ruleset in that header and is compiled against the ruleset's
     map, never against a bare `#define`. The second powerup timer is the
     reference case and the only one known today;
  5. a **modifier** may only take a slot no ruleset it composes with uses —
     ~~which is why Colored Hitman cannot keep its 1999 `STAT_COLOR 30`
     unexamined (R-CH-4)~~. *R-CH-4 is struck in 1.2 with Colored Hitman, so this
     clause now has no worked example. It stays as a general rule: the remaining
     modifiers are `teamplay`, `hook`, `runes`/`techs` and bots, none of which
     currently claims a slot;*
  6. `MAX_STATS` is not exceeded. With `USE_NEW_GAME_API` there are 64 slots
     (`MAX_STATS_NEW`) rather than 32, so headroom exists — but only for clients
     that negotiate the extension, so a ruleset must stay inside 32 for its
     baseline statusbar and may use 32–63 only for content it can drop
     (R-COMPAT-5);
  7. the check is on **kind** as well as number. A statusbar element declares
     what a slot holds — `stat_string N` a configstring index, `pic N` an image
     index, `num W N` a plain number — and writing the wrong kind is a type
     error across a boundary with no compiler on it. A name-collision test
     cannot see it when the bar uses a bare number, which is how RA2's slot 20
     got through. `tools/slotkind.py` (R-TOOL-1) is that check and its known
     gap is closed before Phase 4 exits.
* **R-OSP-7a.** *The statusbar is built, not stored.* Static bar strings are why
  the slot problem is intractable: a literal hardcodes its slot numbers, so a
  ruleset that needs a slot a bar already uses has nowhere to go. CTF is the
  worked case, measured — slots 17, 18 and 19 each carry **two** assigned names
  (`STAT_SPECTATOR`/`STAT_CTF_TEAM1_PIC`, `STAT_TIMER2_ICON`/`STAT_CTF_TEAM1_CAPS`,
  `STAT_TIMER2`/`STAT_CTF_TEAM2_PIC`), and there was **no free slot to move to**:
  CTF uses 0–30 of `MAX_STATS_OLD`'s 32, leaving only 31, and the second powerup
  timer needs two. Upstream resolved it by gating the writes on `!ctf->value`,
  matching what `SP_worldspawn` already did with the bar itself. Colosseum
  instead emits the bar imperatively at runtime from the active ruleset's slot
  map (the `sb_*` model), so slot numbers exist in exactly one place and the
  shared second-powerup-timer mechanic stops needing three different `#define`s
  (baseq2 18/19, RA2 26/27, tourney 29/30). The 64 slots of `MAX_STATS_NEW` are
  only available under `USE_NEW_GAME_API`, which is why the CTF squeeze exists at
  all, and clause 6 still applies.
* **R-OSP-8.** Monsters do not spawn in `tourney`; same treatment as R-RA-6.
* **R-OSP-9.** *Map rotation is per ruleset* — the third exemption to §7 rule 6.
  `sv_maplist` for `dm` and `sp`, RA2's `maploop.c` (845 lines, which is also the
  `arena.cfg` block/key parser and so cannot simply be deleted) for `arena`, and
  OSP's `osp_maps.c` (338 lines) for `tourney`. Exactly one is live at a time,
  and that is a requirement rather than an observation: **RA2 currently runs two
  at once** — `maploop.c` and the q2pro-inherited `sv_maplist` path it really
  reads at `g_main.c:369-370`. That is a bug and is fixed as part of the merge.
  OSP registers `sv_maplist` and never reads it, which is correct and stays.
* **R-OSP-10.** *Stats logging is per ruleset* — the fourth exemption. There are
  **four** subsystems, not the two 1.0–1.1 assumed: RA2's `ra2stats.c` (428
  lines, one JSON object per round) and `gslog.c` (339, StdLog, server-wide and
  arena-blind); tourney's `osp_stats.c` (867, one JSON object per event) and the
  Standard Log (`stdlog.c` 263 + `sl_write.c` 250). All four ship, active per
  ruleset. Both replacements for the dead upload services are **inherited, not
  work** (§0 rule 6): `rocketarena2-public@789895b` removed the GameSpy `gstats`
  SDK and `osp-tourney@a8e30d0` the NetGames USA layer.
* **R-OSP-11.** *Bot cvars are per ruleset.* Under `tourney` the names are
  tourney's — `bots_autoload`, `bots_minplayers` (default 4, so bots are on out
  of the box), `bots_botfile`, `bots_delayload`, `bots_warmuptime`,
  `bots_noclients`, `vote_bots_max` — and under every other ruleset they are
  `minimumplayers` and `botfile`. Both sets are registered; `doc/cvars.md` states
  which is authoritative per ruleset. Bots under `tourney` keep the behaviour the
  port already implements and it is more nuanced than "parity or match-only":
  bots ready themselves up after `bots_warmuptime`, join teams and are counted,
  but are already excluded from 1v1 (`bots_autoload == 2` zeroes
  `bots_minplayers`), from hi-scores, from team captaincy and from vote quorum,
  and bot management is already gated behind `serveronlybotcmds` and
  `vote_enable_bots`. That is the contract; it is preserved rather than redesigned.
* **R-OSP-12.** *Tourney has four modes of play, not three.* The latched cvar
  `match_mode` is cached into the global `m_mode` (`osp_main.c:23`) and selects
  among four. All four ship and all four are **inherited, not work** (§0 rule 6)
  — `osp-tourney@a8e30d0` implements them. Measured 2026-08-21:

  | `match_mode` | `match_type` (serverinfo) | banner | structure | shipped configs |
  |---|---|---|---|---|
  | `0` | `RegularDM` | `*** REGULAR DEATHMATCH ***` | Plain FFA. No ready-up, `sync_stat` starts at 8. The only mode with hi-scores and with `qualifier_numspots` forced to 0. | 7 |
  | `1` | `QualifierDM` | `*** DM QUALIFIER ***` | FFA behind a ready gate; the top `qualifier_numspots` fraggers qualify and are starred on the scoreboard. Adds `qualifier_skinname` and `qualifier_forceskins`. | 1 |
  | `2` | `TeamPlay` | `*** DM TEAM-PLAY MODE ***` | Two teams: captains, join codes, invites, team lock/unlock, `switchteam`, `kickplayer`, overtime. | 4 |
  | `3` | `1-vs-1` | `*** DM 1V1 MODE ***` | Duel. Two teams of one, forced. Adds a **spectator queue**: winner stays, the next in line has `team_nextuptime` seconds to claim the slot or forfeits it. Timeouts are per individual, not per team. | 3 |

  The banner block is `osp_main.c:564-586`; the mod documents the same four at
  `docs/server-settings.txt:413-419`.

  **1v1 is a first-class mode, not a corner of team play**, and this is the
  clause that matters for the port: **34** `m_mode == 3` sites across **10**
  files, six dedicated helpers in `osp_teams.c` (`OSP_1v1Team` :329,
  `OSP_1v1AllowJoin` :370, `OSP_1v1Add` :421, `OSP_1v1Remove` :435,
  `OSP_1v1QueueCheck` :471, `OSP_1v1queue_cmd` :1658), its own client command
  under three aliases — `queue`, `line`, `order` (`g_cmds.c:1217-1219`) — its own
  statusbar branch (`p_hud.c:172`), its own scoreboard branch
  (`osp_display.c:337`), its own observer, camera and menu join rules
  (`osp_observe.c` ×5, `p_camera.c` ×3, `osp_menus.c` ×4), and four cvar
  overrides applied at `InitGame`: `team_maxplayers` → 1 `CVAR_NOSET`
  (`osp_main.c:479-484`), its own `team_maxplayers` default of 1 rather than 4
  (`osp_main.c:404-407`), `bots_minplayers` → 0 when `bots_autoload == 2`
  (`osp_main.c:651-653`), and a `match_latejoin` override (`osp_main.c:2137`).
  A merge that treats mode 3 as "mode 2 with two players" loses every one of them.
* **R-OSP-13.** *`match_mode` is validated; every other bounded tourney cvar
  already is.* This one is **work, not inherited.** `match_mode` is registered
  raw at `osp_main.c:270` and never range-checked, while its immediate
  neighbours all are — `match_countdown` clamped to ≥14, `match_readypercent`
  to 1..100, `qualifier_numspots` to ≥0, `damage_railgun` to ≥1. The banner
  block's final `else` therefore catches everything outside 0..2, so
  `match_mode 4` announces `*** DM 1V1 MODE ***` and sets `match_type` to
  `1-vs-1` while skipping all 34 `m_mode == 3` branches: no queue, no forced
  `team_maxplayers 1`, team-scoped timeouts. The server tells clients it is a
  duel server and is not one. A negative value does the same.

  Colosseum clamps `match_mode` to 0..3 at `InitGame`, warns once naming both
  the supplied and the substituted value, and falls back to `0` — the same
  posture R-MODE-1 sets for `g_ruleset`: correct the value, warn, never abort
  startup. The mode is then read from the clamped value everywhere, so the
  banner, `match_type` and the behaviour cannot disagree.

### 6.7 R-CH — Colored Hitman — **struck in 1.2**

Colored Hitman is out of scope (N7). R-CH-1..4 are struck and kept here per §6's
rule that a dropped requirement is struck rather than deleted.

* ~~**R-CH-1.** Colored Hitman is available as a `dm` modifier: `ch 1` with
  `ch_maxcolors` and `ch_colortime`, `gclient_t.chcolor`, the colour statusbar,
  `ColorImageName`, and the per-frame `UpdateColoredHitman` recolouring.~~
* ~~**R-CH-2.** The brain is told through the `ch` libvar (R-BOT-6).~~ The
  libvar survives, pushed as a constant `"0"` — see R-BOT-6.
* ~~**R-CH-3.** `ch` is refused with a message under `ctf`, `arena`, `tourney`
  and `sp`.~~ R-MODE-2 now accepts a legacy `ch 1` and ignores it with a warning.
* ~~**R-CH-4.** The CH stat slot is allocated by R-OSP-7, not hard-coded to 30.~~
  This was R-OSP-7 clause 5's only worked example; see the note there.

### 6.8 R-EXTRA — the small Gladiator features

Features the 1999 module carried behind its own `#define`s. Each is kept, each
becomes a cvar, none is compiled out.

* **R-EXTRA-1.** `LOGFILE` → a game log (`g_log.c`) with `Log_ShutDown` on
  `ShutdownGame`.
* **R-EXTRA-2.** `CLIENTLAG` → client lag simulation (`p_lag.c`, 265 lines):
  the `lag` client command, the `ClientThink` hook, and the ping adjustment in
  the rankings screen (v0.93).
* **R-EXTRA-3.** `TRIGGER_COUNTING` and `TRIGGER_LOG` → trigger counting and
  trigger logging, off by default.
* **R-EXTRA-4.** `FUNC_BUTTON_ROTATING` → the rotating button.
* **R-EXTRA-5.** `VWEP` → visible weapons.
* **R-EXTRA-6.** `OBSERVER` → observer mode with the eye and chase cameras.
  ~~Reconciled with OSP's: one implementation, not two.~~ *Amended in 1.2:
  **three** implementations, one per ruleset,* the second exemption to §7 rule 6.
  Measured:

  | ruleset | implementation | lines |
  |---|---|---|
  | `dm`, `sp`, `ctf` | `gladiator-bot-restored/game/p_observer.c` + `camera_t` in `gclient_t` | 2,386 |
  | `tourney` | `osp_observe.c` + `p_camera.c` | 335 + 929 |
  | `arena` | RA2's four modes — `NORMAL`, `FREEFLYING`, `TRACKCAM`, `EYECAM` — with `ChangeOMode`, `track_think`, `eyecam_think` | in `arena.c` |

  The Gladiator implementation comes from the **reconstruction**, not
  `gladq2_src` (which has only `p_observer.h`, with `OBSERVER` commented out) and
  not `ugladq2` (2,245 lines, re-measured in 1.4 on branch `restore-p-observer`;
  1.0–1.3 said 2,243). RA2's cannot be substituted away regardless of
  rule 6: its observers *are* the voting electorate and the queue audience, so
  R-RA-4 depends on their exact behaviour. The v0.95 fix — entering autocam while
  dead must not corrupt view angles — is a regression entry. See also R-CORE-14:
  `FL_OBSERVER` collides with two bot flags and is relocated.
* **R-EXTRA-7.** The ztn2dm2 squished-plat fix (v0.94, `g_func.c`) is a
  regression entry; verify whether q2pro already fixed it and record the finding
  either way.

### 6.9 R-KEY — the contracts no compiler checks

Two data contracts in this code survive only as *text*, so neither the compiler
nor git's three-way merge can tell when a change breaks one. Both were broken
during the mission-pack replay and both were found only afterwards. Colosseum
merges six donors' worth of the same tables, so they get their own group.

* **R-KEY-1.** *Map entity keys are an external contract.* A `.bsp` names entity
  fields as literal strings, matched at runtime against the key column of
  `spawn_fields[]` and `temp_fields[]`. Renaming a `spawn_temp_t` member
  therefore changes the key text that shipped maps use. Q2PRO's
  `Convert monster timers to frame numbers.` renamed `monsterinfo_t.pausetime`
  to `pause_framenum`; replayed tree-wide it also hit the *unrelated*
  `spawn_temp_t` member of the same name, which is the target of the
  `"pausetime"` row. Every map setting `pausetime` on a `func_timer` silently
  lost its initial pause — `ED_ParseEdict` said `pausetime is not a field` and
  banks of timers that should stagger fired in lockstep. The key column of both
  tables is **frozen**: a member rename that changes a key is a spec amendment,
  not a refactor, and the QUAKED documentation block is part of the same
  contract and moves with it.

  *Confirmed live in 1.7, and it is the reason this requirement exists.* Both
  donor branches in the bundles **still carry the broken rename** — the spine has
  `spawn_temp_t.pausetime`, `port_xatrix` and `port_rogue` have neither — so the
  three-way merge of `g_local.h` propagated it straight into Colosseum. It was
  caught only because `g_func.c` had not been merged yet and still said
  `st.pausetime`, which made it a compile error instead of a silent behaviour
  change. **Had `g_func.c` come from a donor in the same step, the tree would
  have compiled clean and every `func_timer` in every shipped map would have lost
  its initial pause.** Q2PRO itself has since fixed it and carries both members
  as the separate things they always were: `spawn_temp_t.pausetime` (float) and
  `monsterinfo_t.pause_framenum` (int). §7 rule 1 decides it — a donor renaming a
  shared member is not that donor's feature — and R-VER-14 is the regression
  check that must exist before Phase 2 merges `g_func.c`.
* **R-KEY-2.** The `F_*` type macro on a `spawn_fields[]`/`temp_fields[]` row
  must match the member's C type. Ground Zero retyped the same `pausetime`
  member to `int` while its row still said `F_FLOAT`, so the parser wrote a
  float bit pattern into an int. Checked mechanically (R-TOOL-1), not by review.
* **R-KEY-3.** The union of all six donors' spawn keys is one table, and every
  key any donor accepted is still accepted — tourney's five extra keys
  (R-OSP-6) included. `doc/entities.md` (D4) lists every key, its member, its
  type macro and the donor that introduced it.
* **R-KEY-4.** A statusbar slot's *kind* is the third such contract; it lives
  under R-OSP-7 clause 7 rather than here, because the slot table is where it is
  enforced.

### 6.10 R-SEC — security posture

The donors include two reconstructions that deliberately preserved their
originals' vulnerabilities, and a 1999 bot SDK with unbounded string handling.
Colosseum ships none of it.

* **R-SEC-1.** No unbounded copy of client-controlled data. Every `strcpy`,
  `strcat`, `sprintf` and `strncpy`-without-termination on a path reachable from
  userinfo, a client command, a chat string, an address string or a config file
  is replaced with a bounded, always-terminating form.
* **R-SEC-2.** The RA2 "Deliberately Kept Original Bugs" set and the tourney set
  in R-OSP-4 are enumerated in `doc/reconciliation.md` with the fix for each.
  An unfixed entry blocks release. ~~The two upstream sources are
  `rocketarena2-public/docs/SECURITY_REVIEW.md` on `main`~~ *— corrected in 1.4:
  that file exists in no RA2 branch and in no RA2 commit.* The RA2 set is real
  and is in two places: the **"Deliberately-kept original bugs"** section of
  `rocketarena2-public/CLAUDE.md`, and the per-function notes in
  `rocketarena2-public/docs/FUNCTION_NOTES.md`. Because an unfixed entry blocks
  release, a citation that resolves to nothing is a release risk and not a
  typo — the ledger is built from those two files. The second upstream source is
  unchanged: the
  "Bugs and security fixes" section of `osp-tourney/doc/q2pro-port.md`; both
  branches fixed everything either lists as remotely reachable memory
  corruption, so the entries import as *fixed upstream* and Colosseum's job is
  R-VER-1 checks, not re-fixes. Two cautions carry with them: neither branch has
  been run against a live server, and RA2's own post-port review "was not
  exhaustive" by its author's account — so *fixed upstream* means materially
  safer, not audited.
* **R-SEC-3.** Bot command paths are server-only by default (R-BOT-25).
* **R-SEC-4.** Array indices derived from client input are bounds-checked at the
  point of use — including bot indices, arena numbers, team numbers, arena
  settings, menu item indices, stat slots and configstring indices.
* **R-SEC-5.** No code path may hand a client a message it cannot parse. The
  two tourney kick paths that did so are the reference case.
* **R-SEC-6.** `_FORTIFY_SOURCE=2` and `-fstack-protector-strong` are on
  (R-BUILD-2) and are not disabled to make anything compile. Two measured
  qualifications, added in 1.5, neither of which is that evasion:

  1. **`_FORTIFY_SOURCE` is release-only.** glibc's fortified headers require
     optimisation; at `-O0` the define is inert and emits only *"requires
     compiling with optimization"*, which `-Werror` would then fail the build
     on. There is no configuration where it would have had an effect and was
     removed. `-fstack-protector-strong` is on everywhere.
  2. **`_FORTIFY_SOURCE` is gcc-only, and this one is an ABI collision rather
     than a policy choice.** `glibc`'s `bits/stdio2.h` guards its fortified
     `dprintf` on `#ifdef __va_arg_pack`: gcc has it and gets a harmless
     function *declaration*, clang does not and gets a function-like **macro
     named `dprintf`** — and `game_import_t` has a member called `dprintf`, so
     every `gi.dprintf(...)` expands to `gi.__dprintf_chk(...)` and the tree
     stops compiling. Isolated 2026-08-21: it needs clang **and** `-O2`
     **and** fortify; any two are fine. Neither side can move — R-CORE-9 forbids
     editing the header that names the member, R-CORE-5 forbids parenthesising
     the call sites as `(gi.dprintf)(...)`. So the shipped artifact is built
     with gcc and hardened, and clang stays a full second opinion on warnings.
     If a future glibc or clang gains `__va_arg_pack`, re-test and delete this
     clause.
* **R-SEC-7.** No outbound network connection, no `system()`, no `exec*()`, with
  one exception: `autolaunchbspc` may launch the BSPC tool, it is **off by
  default**, and the path it runs is not client-controllable.
* **R-SEC-8.** *No struct is punned as an array of another type, and no
  translation unit declares `extern` for itself.* The `qboolean` → C99 `bool`
  retype changes `sizeof`, and three of the sixteen defects found in RA2 after
  its port are this one class — none visible to the compiler: the retype shrank
  `arena_settings_t` from 168 to 96 bytes while `ra2menus.c` still punned the
  block as `int[42]`, putting four writes outside `arena_t`; four `bool[7]`
  arrays were written through a stale `extern int[]` in another translation
  unit, 21 bytes out of bounds apiece on every map load; and `game.maxclients`
  was never initialised, so the client array was allocated for zero entries.
  R-OSP-5's `extern int botglobals;` is a fourth instance of the same shape.
  The rule: every cross-translation-unit declaration comes from a header, never
  from a local `extern` in a `.c`, and a block of one type is never accessed
  through a pointer to another.
* **R-SEC-9.** The `__attribute__((format))` annotations Q2PRO puts on the game
  import table stay on, and nothing is cast to silence them. They are a
  bug-finder: they immediately exposed nine places where RA2 passed a runtime
  string as a *format* argument, including a kill log where a player named
  `%n%n%n` controlled the format string. CTF, tourney and the 1999 bot glue have
  never been compiled under them, so Phase 3, 6 and 7 each expect a fresh crop.

### 6.11 R-COMPAT — compatibility

* **R-COMPAT-1.** Colosseum loads into stock Q2PRO of the pinned commit, and
  into any later Q2PRO advertising a compatible `GAME_API_VERSION`.
* **R-COMPAT-2.** Existing 1999 bot assets load unchanged: `bots.cfg`,
  character files, `.aas` files, `pak7.pak`, the `default/` config set — as
  shipped in `gladiator-bot-restored/assets`.
* **R-COMPAT-3.** Legacy cvar names keep working (R-MODE-2) and no legacy cvar
  is repurposed to mean something else.
* **R-COMPAT-4.** Where two donors define the same cvar name with different
  meanings, the conflict is resolved per §7 rule 4 and both names remain
  available.
* **R-COMPAT-5.** Client-visible protocol behaviour degrades cleanly: with
  `g_protocol_extensions 0` and a vanilla client, everything except the extended
  limits works.
* **R-COMPAT-6.** *A cvar name registered twice is a bug, not an alias.* Measured
  2026-08-20: RA2 (`ra2stats.c:32-33`) and tourney (`osp_stats.c:163-164`) both
  register `statsfile` and `statsname`, with **different defaults**
  (`ra2stats.jsonl` and `osptourney.jsonl`). In one library the second `gi.cvar`
  returns the first's existing cvar, so whichever `InitGame` path runs second
  silently writes to the other's filename. Per §7 rule 4 each takes its donor
  prefix — `ra_statsfile`/`ra_statsname`, `osp_statsfile`/`osp_statsname` — and
  the bare names remain as read-only aliases resolving to the active ruleset's.
  A build-time check enumerates every `gi.cvar*` first argument and fails on a
  duplicate registered from two translation units (R-TOOL-3).

---

## 7. Reconciliation rules

Six donors will disagree. These rules decide, in order, so that no merge
decision is made twice or by taste.

1. **Q2PRO is the base, and "Q2PRO" means `q2pro/src/game`.** Where a donor and
   that tree differ on something that is not the donor's own feature — a timer,
   a bounds check, a style, an API call — Q2PRO wins. The donor's version is a
   stale copy of id's code. The scoping matters: `q2pro/src/{ctf,xatrix,rogue}`
   are **not** upstream Q2PRO (§3) but replay outputs like the two mod branches,
   so they carry no more authority than RA2 or tourney does.
2. **A donor owns its own feature, and the diff says which.** Where the
   difference *is* the donor's feature, the donor wins and Q2PRO's baseq2
   behaviour becomes the `dm` branch of the dispatch. Which is which is not a
   judgement call: `git diff baseq2 port_<donor>` in the bundles of §3.2 is by
   construction that donor's own feature set and nothing else (R-PROV-3).
3. **Content unions, behaviour gates.** Two donors adding different things
   (items, classnames, weapons, entities) both get added. Two donors changing
   the same thing get a gate, and the gate is the ruleset or the content-layer
   cvar — never a new one invented for the occasion.
4. **Name collisions are resolved by prefix, not by rename.** A donor-specific
   function or cvar that collides keeps its donor prefix (`ctf_`, `ra_`, `osp_`,
   `ch_`, `bot_`). Where two donors claim the same *cvar name* with different
   meanings, the primary ruleset's meaning wins for the bare name and the other
   gets its prefixed alias; both are documented in `doc/cvars.md`.
5. **The bot ABI yields last, and visibly.** ~~If a reconciliation would require
   changing `botlib.h`, the reconciliation is wrong.~~ *Amended in 1.2 with
   R-BOT-1:* a reconciliation that needs `botlib.h` to change is suspect and
   another way is preferred, but the header is versioned rather than frozen now
   that both sides are compiled from source. A change lands only with a
   `BotVersion` bump, agreement from the botlib side, and a row in
   `doc/botlib-contract.md`.
6. **One implementation per concept — except where the concept *is* the
   ruleset.** Where two donors ship their own version of the same generic
   mechanism, Colosseum keeps **one**, chosen as the most capable, and the others
   become adapters or are dropped. The choice and its loser are recorded.

   Applies to: **grapples** (CTF grapple/hook, OSP hook, RA2's own — 3),
   **flood/spam checks** (Q2PRO's `FloodProtect()`, RA2's hardcoded spam
   counter, tourney's inline check gated on `!team && !match_paused`; both ports
   kept Q2PRO's and ran it first — 3), **statusbar assembly** (Q2PRO
   concatenates `dm_statusbar` onto `single_statusbar`; RA2 and tourney each ship
   complete bars, tourney four of them — resolved by R-OSP-7a's composed
   emitter), and **vote systems**.

   **Four named exemptions, decided 2026-08-21.** In each of these the donor's
   implementation is not a generic mechanism that happens to be duplicated — it
   is part of the ruleset a player chose, and unifying it would change how that
   ruleset plays. All four ship, exactly one active per ruleset:

   | concept | implementations | requirement |
   |---|---|---|
   | menus | 4 engines | R-MENU-1 |
   | observer / chase camera | 3 (Gladiator, OSP, RA2's four modes) | R-EXTRA-6 |
   | map rotation | 3 (`sv_maplist`, RA2 `maploop.c`, OSP `osp_maps.c`) | R-OSP-9 |
   | stats logging | 4 (RA2 round log + `gslog`, OSP `osp_stats` + Standard Log) | R-OSP-10 |

   An exemption is not a licence to skip reconciliation: each still gets its
   rows in `doc/reconciliation.md`, each still has exactly one owner at a time,
   and where two of a kind can be live simultaneously that is a **bug** — see
   R-OSP-9's double-rotation case.
7. **Fixes are cumulative and never reverted.** A bug fixed by Q2PRO, by either
   reconstruction's port branch, or by uGladQ2 stays fixed. A donor's older
   code that reintroduces it loses, regardless of rule 2.
8. **Every decision is a row.** `doc/reconciliation.md` has one row per
   decision: the file, the concept, the donors involved, the rule applied, the
   outcome, and the requirement ID it serves. A merge with no row is not done.
   The file's skeleton is generated from R-PROV-3's diffs, so the rows exist
   before the decisions do and an unannotated row is visible work.
9. **Linkage and mechanical form are not decisions.** `static`/`extern`
   (R-CORE-13), the twelve formatting and retyping passes (R-CONV-1), forward
   declarations, and the four audits of R-TOOL-1 are settled by running a tool,
   not by choosing. Where a tool and a reviewer disagree, the tool is fixed and
   re-run; a hand edit that a re-run would undo is not a merge decision, it is a
   defect.

---

## 8. Conventions

* **R-CONV-1.** Style follows Q2PRO's post-astyle form — 4 spaces, K&R braces,
  `bool`/`true`/`false`, LF endings, no trailing whitespace. `src/bot/`'s
  `bl_*` files, `botlib.h` and `anorms.h` are **excluded from reformatting** and
  keep their upstream shape, including the `} //end of the function NAME` idiom,
  exactly as `osp-tourney` excluded them. They still take every API and type
  change and compile with the same flags.
* **R-CONV-1a.** *Newly imported code is transformed, not merely styled to
  match.* Roughly a fifth of Q2PRO's commits are mechanical, and the donors
  replayed them by **re-running the transformation** over the whole tree rather
  than applying the diff — because a diff only reaches files baseq2 also has,
  which would have left `osp_main.c`, `arena.c`, the menu engines and the
  logging layer stranded in 1998 style and not compiling once the shared headers
  moved underneath them. Colosseum imports code that has never been through any
  of it: Gladiator's `p_menulib.c`, `p_botmenu.c`, `g_ch.c`, `p_lag.c`,
  `g_log.c`, `p_observer.c` and every uGladQ2 fix. Each such file is put through
  the same passes, in the same order, with the same tools (R-TOOL-1):

  | pass | what it does |
  |---|---|
  | astyle | the "Massive coding style change" reformat |
  | stdbool | `qboolean`/`qtrue`/`qfalse` → `bool`/`true`/`false` |
  | qrand | `rand()` → `Q_rand()` |
  | qatoi | `atoi()` → `Q_atoi()` |
  | nextthink | `nextthink` seconds → frame numbers |
  | timers | debounce, client, monster and misc timers → frame numbers |
  | desig | positional item initialisers → designated |
  | precarray | precache strings → NULL-terminated arrays |
  | floatsuffix | `1.0` → `1.0f` |
  | maxclients | `maxclients->value` → `game.maxclients` |
  | constify | `gitem_t` pointers const-ified |
  | striptrail, wsfix | the two whitespace passes |

  A file byte-identical to its Q2PRO counterpart is taken verbatim instead. The
  R-CONV-1 exclusion applies to the *formatting* passes only; the retyping and
  API passes reach `bl_*` too.

  *Accounting corrected in 1.4.* The table has thirteen rows and R-TOOL-1 says
  "the twelve pass scripts". Both are right and neither is a script count:
  `mech.py`'s `TRANSFORMS` holds exactly twelve — `astyle`, `stdbool`, `qrand`,
  `qatoi`, `nextthink`, `timers`, `desig`, `precarray`, `floatsuffix`,
  `maxclients`, `striptrail`, `wsfix` — and `constify` is the thirteenth pass,
  implemented as a standalone `constify.py`. Six of the twelve have no file of
  their own because they live inside `mech.py`. Anyone looking for thirteen
  scripts will not find them; anyone applying twelve passes will miss
  const-ification.
* **R-CONV-2.** Prefixes: `CTF*`/`ctf_` for CTF, `RA2_`/`ra_` for arena,
  `OSP_`/`osp_` for tourney, `Bot*`/`bl_` for the bot layer,
  **`xatrix_` for *The Reckoning* and `rogue_` for *Ground Zero*** (added in 1.7
  — 1.0–1.6 listed a prefix for every ruleset and none for either content layer,
  and Phase 2 needed one on its first day), `Col_`/`g_col*` for anything
  genuinely new to Colosseum.

  *Which side keeps the bare name.* §7 rule 4 gives it to "the primary ruleset's
  meaning", and a content layer is not a ruleset, so for an xatrix/rogue
  collision there is no principled winner and **both** sides take the prefix.
  Measured: exactly two collisions in the whole 16,676-line two-donor merge —
  `fire_heat` and `monster_fire_heat`, which are Xatrix's Phalanx heat-seeking
  projectile and Ground Zero's plasma beam, two different weapons that happen to
  share a name. An unprefixed `fire_heat` would be ambiguous to every later
  reader, so neither keeps it. (`CH_`/`ch_` is retired with Colored
  Hitman, N7.) Where two donors' menu engines collide the prefix applies to
  symbols as well as folders — see §5.2.
* **R-CONV-3.** New code introduces no new global mutable state without a row
  in `doc/reconciliation.md` saying why it cannot live in `level`, `game` or the
  entity.
* **R-CONV-4.** Commits reference requirement IDs. A behavioural commit with no
  ID is incomplete.
* **R-CONV-5.** `git log` is authoritative for chronology and progress; this
  spec is authoritative for intent. Neither duplicates the other, and `SPECS.md`
  is not a progress log.

### 8.1 R-TOOL — the toolchain

The replay harness that produced the five † donors of §3 already carries the
audits this spec asks for. They are not rewritten from the prose here.

* **R-TOOL-1.** `tools/` (D8) is `q2pro-mission-pack-tools` — 30 scripts,
  ~1,700 lines — re-pointed at this tree. ~~It hard-codes a scratchpad path that
  no longer exists~~ *— narrowed in 1.4, because the repair is smaller than
  1.1–1.3 assumed and mis-stating it invites a rewrite where a patch will do.*
  Measured 2026-08-21: exactly **one** file hard-codes the dead scratchpad,
  `build_replay.sh` line 2 (with the WSL `q2pro` path on line 3); every Python
  tool resolves `SP = dirname(abspath(__file__))` and `R = SP/'replay'`, so they
  need a `replay/` working tree beside them and nothing else. One further path
  is stale and is not a scratchpad: `mech.py:13` expects astyle at
  `SP/opt/root/usr/bin/astyle`, which does not exist — point it at the host's
  `astyle` 3.1 (R-BUILD-6). The `replay/` tree is reconstituted from
  `vendor/replay/` (R-PROV-1) as the first task of Phase 0.
  What it provides, and the requirement each one discharges:

  | tool | discharges |
  |---|---|
  | `qnorm.py`, `writetree.py` | the id-source normalisation, validated by reproducing Q2PRO's own import commit byte-for-byte |
  | `mech.py`, `sweep.py`, `dsweep.py` + the twelve pass scripts | R-CONV-1a |
  | `rb.py`, `drive.py`, `autores.py`, `res.py`, `staticres.py` | the replay driver and its conflict resolution |
  | `staticize.py`, `unstatic.py`, `fixdecls.py` | R-CORE-13 |
  | `auditsave.py` | R-SAVE-3 and R-SAVE-3a |
  | `slotkind.py` | R-OSP-7 clause 7 — *and it has a known gap: its own docstring records that it missed RA2's slot 20* |
  | `keycontract.py`, `keys.py` | R-KEY-1..3 |
  | `auditems.py` | R-CORE-2's itemlist union |
  | `balance.py` | brace balancing inside `#if 0` so astyle does not mis-indent past it |

* **R-TOOL-2.** Every number this spec publishes as an acceptance criterion —
  classname, cvar and command counts, the R-CORE-10 divergence matrix, the
  R-CORE-12 coverage ledger — is produced by a script in `tools/` that ships
  with it. A figure with no script is indicative only (§3) and cannot gate a
  phase.
* **R-TOOL-3.** The four audits of R-TOOL-1 (`auditsave`, `slotkind`,
  `keycontract`, `auditems`) run in the build, not on request. A new finding
  fails the build the way a warning does (R-BUILD-2).
* **R-TOOL-4.** `git config rerere.enabled true` for any replay done in this
  tree, and the `rr-cache` is committed — both the 496 resolutions rescued per
  R-PROV-2a and everything this tree records subsequently. Note the rescued
  cache resolves *baseq2-history-onto-donor* conflicts; Colosseum's merge runs
  the other way (donor features onto the spine), so it saves no labour here. Its
  value is provenance and the ability to re-run or audit the donor ports.
* **R-TOOL-5.** *Three tools this spec relies on do not exist and are work, not
  import.* R-TOOL-1's table is an inventory of what `q2pro-mission-pack-tools`
  **has**; the following are named elsewhere in the spec as though they were in
  it, and 1.4 separates them out because each gates a phase:

  | tool | named by | status |
  |---|---|---|
  | `tools/divergence.py` | R-CORE-10 — "regenerated by `tools/divergence.py` rather than transcribed", and the generated copy is declared **authoritative over the spec's own table** | **does not exist.** Written in Phase 0; until then the R-CORE-10 matrix is a transcription, which is the one thing that requirement forbids |
  | the classname / cvar / command counters | R-TOOL-2, and every count in §3 and §6 acting as an acceptance criterion — 157 / 173 / 192 / 259 / 137 and the rest | **does not exist.** §3's counting-method paragraph already concedes it. Until it ships, no count gates a phase, which means R-BASE-1/2/3, R-MP-1, R-OSP-2 and R-RA-2 are **unenforceable as written** |
  | the pristine-statusbar availability check | R-OSP-7 clause 7 and §9 Phase 3, as "closing `slotkind.py`'s known gap" | `slotkind.py` exists and the gap is real, recorded in its own docstring. An extension, not a new tool |

  R-TOOL-1's own line counts do check out — 30 scripts, 1,696 lines, twelve
  `mech.py` transforms — so the inventory is accurate about what it holds. The
  defect was reading it as a list of everything the spec needs.

---

## 9. Phases

Each phase ends on a demonstrable state. A phase does not start before the
previous one's exit criteria are met and the §12 questions it depends on are
closed.

### Phase 0 — Foundations
Vendored `inc/`, `config.h`, Makefile, meson shim, `genptr.py` wired as a build
step, `src/` populated with `q2pro/src/game` verbatim, `src/shared/`. The
bundles vendored under `vendor/replay/` and their SHAs pinned (R-PROV-1);
`tools/` imported and repaired off the dead scratchpad path (R-TOOL-1); the four
audits wired into the build (R-TOOL-3); `doc/replay-coverage.md` generated from
the `q2pro-commit:` trailers (R-CORE-12) and the R-CORE-10 divergence matrix
regenerated (R-TOOL-2).

*Four tasks added in 1.4.* `git init` — Colosseum has no repository of its own
and currently resolves to the one above it, which Q2 decided against; the
re-pinning of §3.3 written into `doc/provenance.md`; **writing**
`tools/divergence.py` and the R-TOOL-2 counters, which do not exist (R-TOOL-5);
and the engine prerequisite of R-BUILD-7 — a native `q2proded` for the host,
without which the exit criterion cannot be evaluated.

**Exit — met 2026-08-21.** An unmodified baseq2 game library builds in all four
configurations, on all five gating targets of R-BUILD-5, under both compilers
natively (ten configurations, `-Werror`, zero warnings), and plays baseq2
deathmatch and the campaign in a stock `q2proded` on the native target, with a
savegame surviving a round-trip across two processes (R-VER-17 — the campaign is
verified through co-op because a dedicated server cannot run single player at
all). The audits run clean on it; `tools/divergence.py` regenerates the R-CORE-10 matrix and the generated
copy is what `doc/reconciliation.md` carries; D9 exists with a verdict on every
row.

### Phase 1 — Ruleset dispatch
`g_ruleset`, the alias layer, the modifier system, the **26** gate locations of
R-MODE-5 including `ClientPlaced`, `dm` implementing all of them. (1.0–1.3 said
25 here; the list has always had 26.)
**Exit — met 2026-08-21.** R-MODE-1..7, each observed live via `sv ruleset`
(R-VER-18): an invalid `g_ruleset` warns and falls back to `dm` without aborting;
`ctf 1` and `rocketarena 1` select their rulesets and conflicting aliases warn
and resolve in R-MODE-2's order; `ch 1` is accepted and ignored; both content
layers compose; and a modifier the ruleset does not accept is disabled with one
message. Switching between `dm` and `sp` changes behaviour through the dispatch,
and `tools/gates.py` fails the build on any ruleset cvar tested at a call site,
which is the "no `if (ctf->value)` anywhere" half enforced rather than asserted.

*Moved ahead of the mission packs in spec 1.1.* Every donor merge lands in the
same handful of functions — `p_client.c` alone carries 3,749 diff lines across
five donors (R-CORE-10) — and `PutClientInServer`, `ClientThink`, `respawn` and
`ClientEndServerFrame` are reopened by each. Cutting the dispatch seam first
means Phases 2 through 6 add hook implementations to a file with a seam in it,
rather than each one re-cutting the seam through code the previous phase just
gated. The 1.0 order had Phase 1 merge two content gates into raw baseq2 files
and Phase 2 retrofit 25 hooks into the result.

### Phase 2 — Mission packs merged and gated
Xatrix and Rogue merged into `src/` per R-CORE-10/11/11a, their own files in
`src/xatrix/` and `src/rogue/`, `xatrix`/`rogue` cvars live, the entity content
flavour of R-CORE-11a carried on the edict, savegame descriptors complete and
type-checked (R-SAVE-3a), the R-CORE-12 re-apply set landed.
**Exit — met 2026-08-21.** All three campaigns load and run: `base1` (357
edicts, 17 monsters), `badlands` (618, 62, all latched `xatrix`), `rmine1` (620,
74, all latched `rogue`), with no unknown classname and no unknown spawn key in
any of them. All four `xatrix` × `rogue` combinations boot and run `q2dm1`.
Savegames round-trip in every combination with monster counts and flavour bits
intact. 220 classnames, 68 translation units, ten build configurations clean
under `-Werror` on both compilers, and all four contract audits clean —
`auditems` against baseq2 shows no lost field, discharging R-VER-15 item 1.

R-CORE-11's per-monster gating landed in 1.9 (R-CORE-11b), so the phase is
complete rather than merely past its exit gate: `rogue 0` gets baseq2's evasion
and `rogue 1` gets Ground Zero's, observed per monster and preserved across a
save/load.

*"Play to completion" is read as "load, run and save/load correctly at the start
of each campaign".* A full playthrough of three campaigns is a human judgement
(R-VER-9's surviving clause) and is not claimed here.

**In progress.** Done: `g_local.h` merged and gated (R-CORE-6), the R-CORE-14
flag allocation, R-CORE-11a's latched content flavour with its descriptor, and
Rogue's `genptr.py` adopted so the `save_ptrs[]` externs match the
`monsterinfo_t` the tree now has (closing `doc/reconciliation.md` R-2).

Remaining, and it is the bulk: **70 shared `.c` merges** (25 xatrix, 46 rogue, 25
of them touched by both) and **27 donor-only files** into `src/xatrix/` and
`src/rogue/`; R-CORE-11's per-monster frame-table gating, which is genuinely new
code because each donor ships a *standalone* `m_soldier.c` and Colosseum needs
one file holding both tables with a spawn-time latch; the `itemlist` and spawn
table unions (R-CORE-2); the remaining descriptors (R-SAVE-3/3a — `auditsave`
and `auditems` still report "not applicable" and stop doing so the moment a donor
lands); the merged key tables (R-KEY-3); and R-CORE-12's one rogue re-apply.

The method is settled and is the useful output of the first increment: the
bundles make each file a three-way merge against the spine rather than a manual
reconciliation, and the conflict rate on the hardest header was 5 hunks out of
390 added lines.

*Re-cut to nine phases in 1.2.* Phase 4 was "menu unification and Colored
Hitman" and both subjects are gone — menus are per-ruleset (R-MENU-1) and CH is
out of scope (N7) — so its one surviving subject, the stat-slot table, folds into
Phase 3, which is where slot contention first bites. Old Phases 5–9 shift down
one.

### Phase 3 — CTF and the stat-slot table
Threewave CTF onto the dispatch, `src/ctf/` including its own menu engine, the
grapple reconciled per §7 rule 6. Then the part folded in from the old Phase 4:
the per-ruleset slot map, R-OSP-7a's composed statusbar emitter, clause 7's kind
check, and closing `slotkind.py`'s known gap by writing the prescribed
pristine-statusbar availability check. CTF is the right home for it — slots 17,
18 and 19 are each double-assigned there with no free slot to move to.
**Exit:** R-CTF-1..7, R-MENU-1..5, R-OSP-7, R-OSP-7a.

### Phase 4 — Rocket Arena 2
`src/arena/`, RA2 onto the dispatch, its own menu engine and its four observer
modes, the uGladQ2 fix list verified item by item, the local round log. Fix the
double map rotation (R-OSP-9).
**Exit:** R-RA-1..6, R-ARENA-1..4.

### Phase 5 — OSP Tourney DM
`src/tourney/`, tourney onto the dispatch, its own observer/camera, menu engine
and map rotation kept per-ruleset rather than reconciled away, the
`statsfile`/`statsname` collision resolved (R-COMPAT-6).
**Exit:** R-OSP-1..13, R-VER-16.

### Phase 6 — Bot layer
`src/bot/` from `osp-tourney`'s ported `bl_*` plus `p_menulib.c`/`p_botmenu.c`
from the Gladiator reconstruction; **the 17 live TOURNEY blocks re-expressed as
runtime branches (R-BOT-29)**; the flag-bit reallocation (R-CORE-14);
redirection, fake clients, the frame loop, commands, `bots.cfg`, the bot menu,
the debug-draw and filesystem extensions, the ruleset → libvar mapping.
**Exit:** R-BOT-1..30. Bots load, spawn, navigate, fight and chat in `dm`.

### Phase 7 — Bots across the rulesets
Bots in `ctf`, `arena` and `tourney`; `minimumplayers` vs `bots_minplayers`
(R-OSP-11); bot cycling; team and arena assignment.
**Exit:** R-MODE-7's bot row in full, R-CTF-4, R-RA-4/5, R-OSP-4/5/11.

### Phase 8 — Hardening and staged exposure
R-SEC sweep, the regression checklist, the docs of D3–D9, the extras of §6.8,
performance measurement. Then a private server, then widened access — there is
no analytical release gate (R-SEC-2).
**Exit:** R-SEC-1..9, R-EXTRA-1..7, R-BOT-23, D2–D9 complete.

---

## 10. Verification

* **R-VER-1.** `doc/regression.md` holds one entry per historically-fixed bug
  named in this spec — the Gladiator v0.91–v0.96 changelog, the uGladQ2
  v0.96u–v0.99u list, the RA2 kept-bugs set, R-OSP-4 — with the symptom, the
  trigger and the check. An entry with no check is not done.
* **R-VER-2.** Boot matrix: every `g_ruleset` × `xatrix` × `rogue` combination
  (5 × 2 × 2 = 20) starts, loads a map appropriate to the ruleset, and runs 100
  frames with no crash and no assertion.
* **R-VER-3.** Bot matrix: for each ruleset that accepts bots, 1 bot and 16
  bots spawn, play for 5 minutes and disconnect cleanly; `botlibdump` and
  `clientdump` show no leaked library reference and no leaked client slot.
* **R-VER-4.** Savegame matrix: for each campaign combination, save and reload
  at three points, and verify entity count, client state and pointer fixup.
* **R-VER-5.** Slot-exhaustion test: fill every client slot with bots, then
  connect a human, and verify a bot relocates (R-BOT-15) rather than the
  connection being refused.
* **R-VER-6.** Index-limit test: on a map that precaches near the old 256-model
  limit, with and without protocol extensions, verify the bot index tables hold
  and that overflow is reported rather than written (R-BOT-11).
* **R-VER-7.** No-bot transparency test: with `numbots == 0`, a recorded demo of
  a session must be byte-identical to one recorded by a build with the
  redirection compiled out (R-BOT-13).
* **R-VER-8.** Build verification: all four configurations, zero warnings, under
  **both** `gcc` and `clang` (R-BUILD-6), on every gating target of R-BUILD-5 —
  ~~x86, x86-64 and aarch64~~ *re-scoped in 1.4* to all **five**: ELF aarch64,
  ELF x86-64, ELF i386, win32 PE and win64 PE. The reference machine can build
  all five and execute only the first. Measured 1.5: **all ten configurations
  build clean under `-Werror`**, and the native one additionally passes R-VER-17.
  The other four are recorded as **built, not run** — never as passing on the
  strength of a clean link. The two-compiler clause applies to the native target;
  a cross target is built by its own cross `gcc`. `genptr.py` output is
  regenerated and unchanged.
* **R-VER-9.** ~~The user runs the builds and the game. Claude's side of
  verification is producing the checks, the matrices and the analysis — never
  invoking `make`, copying binaries, or launching the engine.~~ *Amended in 1.5,
  on the author's instruction.* Claude builds and runs. The original rule was
  written to keep the verification honest — an agent that runs its own build can
  report a pass it did not get — and that concern is real but is better served by
  **evidence than by abstention**: a claim of "builds clean" is checkable when
  the command and its output are in the transcript, and unfalsifiable when
  nobody ran it. Phase 0 is the argument. Under the old rule the tree was
  handed over unbuilt and four defects were still in it: 622 warnings that
  R-BUILD-2 forbade, a clang-only compile failure, a library the engine would
  not have found, and an untested savegame table that had just been renumbered.
  Building found all four in one afternoon.

  What replaces the prohibition:
  1. **Every build and run is reproducible from the transcript** — the exact
     command, and enough output to see the result. A summarised pass is not a
     pass.
  2. **A check that has never failed is not trusted.** Every mechanised check
     ships with a positive control that makes it fail (§10, and
     `doc/regression.md`'s control table).
  3. **"Built" and "run" stay separate columns.** R-BUILD-5 records them
     separately, because the reference machine can build five targets and
     execute one.
  4. The author remains the authority on whether the game *plays well*. Claude
     verifies that it loads, spawns, saves, reloads and shuts down; judgement
     about feel, balance and bot behaviour is not delegated.
* **R-VER-10.** Replay-coverage check: `doc/replay-coverage.md` has a verdict on
  every (donor, skipped commit) pair, every *re-apply* verdict has a commit that
  lands it, and the check is re-run after each donor import. Neither compiler nor
  test suite can see a Q2PRO fix that simply never arrived (R-CORE-12).
* **R-VER-11.** Contract checks, run in the build (R-TOOL-3) and re-run per
  phase: `keycontract.py` over the merged spawn tables (R-KEY-1..3),
  `auditsave.py` over every persistent struct for presence *and* type
  (R-SAVE-3/3a), `slotkind.py` over every ruleset's statusbars for number *and*
  kind (R-OSP-7 clause 7), `auditems.py` over the merged `itemlist` (R-CORE-2).
  A finding fails the build.
* **R-VER-12.** Linkage check: no function is `static` that any donor's code
  calls across a file boundary, and no forward declaration disagrees with its
  definition (R-CORE-13). Run as a tool, not read.
* **R-VER-13.** Format-string check: the build carries Q2PRO's
  `__attribute__((format))` annotations on the game import table with no casts
  added to silence them, and the resulting diagnostics are zero (R-SEC-9).
* **R-VER-14.** Regression: the `pausetime` key survives. A map that sets
  `pausetime` on a `func_timer` staggers as intended, and `ED_ParseEdict` reports
  no unknown field. This is R-KEY-1's named case and the one contract breach the
  replay shipped before it was caught.
* **R-VER-15.** *The original harness's unfinished checks are inherited.* The
  rescued archive (R-PROV-2a) contains the harness's own `TODO.md` — eight items
  its author left open. Each becomes a check here, because each is a thing the
  replay could have got wrong in the donors Colosseum is built from:
  1. **`itemlist[]` field-by-field** against q2pro's baseq2 entries per donor.
     The note records a real instance: a conflict resolution at "Convert
     `itemlist[]` to designated initializers" regenerated two entries from the
     variant side and dropped q2pro's railgun precache fix. It was caught at the
     next commit; "others may lurk". `auditems.py` is the check.
  2. **Persistent fields without descriptors** (R-SAVE-3/3a). The note's example
     — xatrix's `quadfire_framenum` missing from the client save table, so the
     powerup is lost across save/load — was fixed upstream, but the class is live.
  3. **`g_ptrs.c` regenerated per tree** with `genptr.py`.
  4. **`m_flash.c` / `MZ2_*`** — the mission packs need flash offsets beyond
     baseq2's table. Relevant to §5.2's `src/shared/m_flash.c`.
  5. **Build integration** — unique output directories per configuration.
  6. **`genptr.py` and commented-out `&*_move_*` references** pointing at
     symbols that no longer exist break pointer generation; q2pro hit this with
     `flyer_move_attack1`. Check Rogue's `m_flyer`/`m_carrier`/`m_widow` comments.
  7. **`SAVE_VERSION` drifted in rogue** — local transforms skipped q2pro's
     bumps. Confirm the final value matches `q2pro/src/game/g_save.c`. *This is
     not recorded anywhere else in this spec and is a live savegame-compatibility
     risk.*
  8. **`clamp()`** was introduced mid-history and no longer exists in the current
     `shared.h`; verify the final compile, Rogue's `p_view.c` gun-angle delta
     specifically.
* **R-VER-18.** *The ruleset must be observable from outside the library, and
  `sv ruleset` is how.* It reports the resolved ruleset, the content layers, the
  modifiers, **every predicate's answer**, the legacy `deathmatch`/`coop` values,
  and an entity census split into live monsters, corpses and gibs.

  This is not a convenience. Nothing the engine prints reveals whether a ruleset
  gate was consulted: `SpawnEntities`' "N entities inhibited" counts spawnflag
  filtering only, so it tracks `deathmatch`/`coop` and would have moved
  identically whether or not R-MODE-5's predicate existed, and a monster the gate
  rejects is spawned and then freed, which nothing reports at all. R-VER-2's boot
  matrix is 20 combinations and each needs an answer to "did this do what the
  matrix says".

  It is **self-checking**: a live monster under a ruleset that forbids them is a
  contradiction, so it names the offending entities. That caught its own two
  classification errors — `misc_deadsoldier` carries
  `SVF_MONSTER|SVF_DEADMONSTER` for a corpse and `misc_gib_*` carries
  `SVF_MONSTER` plus `EF_GIB` for a gib, and both correctly spawn in every
  ruleset — which had made `dm` report a live monster on `base1` and the gate look
  leaky while it was working.
* **R-VER-17.** *The smoke test is a named, repeatable check, not a one-off.*
  Run after every phase, on the native target, against a stock Q2PRO:
  1. **Deathmatch.** `+set deathmatch 1 +map q2dm1`. Expect `Loaded game library
     from …`, `==== InitGame ====`, `SpawnServer: q2dm1`, `0 entities
     inhibited`, a `status` naming the map, and `==== ShutdownGame ====` on
     `quit`.
  2. **Campaign.** `+set deathmatch 0 +set coop 1 +map base1`. Expect
     `Game supports Q2PRO enhanced savegames.` and a non-zero inhibited count
     that **differs** from the deathmatch run — 28 against 50 on `base1`, which
     is `SPAWNFLAG_NOT_COOP`/`NOT_DEATHMATCH` filtering doing its job.
     *A dedicated server cannot run single player at all*: `src/server/init.c`
     forces `deathmatch 1` unless `coop` is set, on the stated grounds that
     "dedicated servers can't be single player". Co-op is therefore how the
     campaign is verified headless, which R-SP-3 wanted anyway; true single
     player needs the full client.
  3. **Savegame round-trip, across two processes.** `save`, quit, start again,
     `load`. Both processes must report the same map and inhibited count, and
     neither may emit `unknown pointer`, `bad index` or `type mismatch` from
     `g_save.c`. This is the only check that exercises `save_ptrs[]` end to
     end, which matters because Colosseum regenerates that table and thereby
     renumbers every entry (`doc/reconciliation.md` R-1).
  4. `deathmatch` and `coop` must **both** be set explicitly: `deathmatch`
     defaults to `1` on a dedicated server, so `+set coop 1` alone produces
     *"Deathmatch and Coop both set, disabling Coop"* and silently tests the
     wrong thing.
* **R-VER-16.** Tourney mode matrix: for each `match_mode` in 0..3, the server
  starts, prints the banner R-OSP-12 tabulates, sets `match_type` to the string
  in that row, and runs a match to completion. Three assertions per mode: the
  `match_type` serverinfo string matches the banner; the mode-gated client
  commands accept and reject per `g_cmds.c`'s guards (`highscores` only in 0,
  `queue`/`line`/`order` only in 3, `captain`/`invite`/`lockteam` only in 2);
  and for mode 3 specifically, `team_maxplayers` reads 1 and is `CVAR_NOSET`.
  Then the out-of-range arm (R-OSP-13): `match_mode 4` and `match_mode -1` each
  warn once and run as mode 0, with `match_type` reading `RegularDM` — not
  `1-vs-1`. This is the check that would have caught the 1.0–1.2 omission, and
  it is cheap: four boots and two negatives.

---

## 11. Risks

| # | Risk | Impact | Mitigation |
|---|---|---|---|
| 1 | `g_local.h` becomes a 6-donor union too large or too tangled to reason about | High | R-CORE-6 plus a per-donor field block with a comment naming the donor and the requirement; savegame descriptors as the forcing function (R-SAVE-3) |
| 2 | Monster frame-table gating (R-CORE-11) corrupts a live `mmove_t` on a cvar change | High | Latch the gate at `monster_start`; never re-read mid-move; a boot-matrix check per gated monster |
| 2a | Shared monster infrastructure (`g_ai.c` 580, `m_move.c` 301, `g_monster.c` 273, `g_phys.c` 184 rogue diff lines) has no `monster_start` to latch at | High | R-CORE-11a: the flavour lives on the edict and those files read the field, never a cvar. **Realised and mitigated in 1.8** — and it turned out `monster_start` was the wrong latch point for the *monsters* too, not just for the shared files (R-CORE-11) |
| ~~2b~~ | ~~R-CORE-11's per-monster gating is unimplemented~~ | — | **Closed in 1.9.** Both evasion sets ship and the latch selects; `sv ruleset` reports which each monster got, and savegames round-trip in every configuration (R-CORE-11b) |
| 2c | A runtime gate that selects between two saved pointers is invisible to `genptr.py` unless written as an if/else with literal assignments | Low | R-CORE-11b states the constraint, R-SAVE-2 owns it, and **`tools/gates.py` now fails the build** on a ternary assignment or an ungated site (1.10). The failure was silent at build and run time and appeared only on savegame load, which is exactly why it needed a check rather than a rule |
| 2d | A check that derives its trigger from the thing it is checking for cannot fail | Medium | Found in `gates.py`'s own gate-completeness check, which passed its negative control because it inferred "table is paired" from the presence of the gate (`doc/reconciliation.md` R-35). Every mechanised check ships with a control that makes it fail — §10, and the control table in `doc/regression.md` |
| 3 | Two reconstructions' 3.20-era assumptions survive the merge and reintroduce a fixed bug | High | §7 rule 7, plus R-VER-1 making every historical fix a named check |
| 4 | Configstring/model index space overflows with the union of all content | Medium | R-BOT-11 sizing from `game.csr`; R-VER-6; protocol extensions on by default in the shipped config |
| ~~5~~ | ~~The 32-bit-only 1999 botlib binaries cannot be loaded by a 64-bit build~~ | — | **Void in 1.2.** The 1999 binaries are not a target (R-BOT-4); the brain is compiled from reconstructed source per platform. |
| 5a | A 64-bit botlib carries no layout verification — its `_Static_assert` guards are wrapped in `#if __SIZEOF_POINTER__ == 4` and go inert | Medium | Extend the guards to 64-bit offsets, or accept and record it; a `Test()` round-trip at load (Risk 6) is the fallback |
| 6 | `bsp_trace_t` struct-return ABI mismatch between game and botlib on Windows | Medium | R-BOT-5; both sides built from the same header with the same convention; a `Test()` round-trip at load |
| 7 | A donor's menus break subtly (wrong item selected, stuck menu) | Medium | R-MENU-1 keeps each donor's engine intact rather than unifying, so there is nothing to break in translation; R-MENU-2a makes single ownership structural; per-donor menu walk in the regression checklist. See also Risk 16 |
| 8 | Stat slot collision — CTF, RA2 and tourney all claim 17..30 with different meanings, `STAT_CHASE`/`STAT_SPECTATOR` sit under them, and baseq2's own second powerup timer needs 2 private slots in every ruleset (18/19, 26/27, 29/30 across the three donors) | Medium | R-OSP-7's per-ruleset table incl. `dm`, clause 4 for shared mechanics, clause 7's kind check, and 64 slots under the new API |
| 8a | A slot's *kind* is wrong rather than its number — no compiler sees it, and the existing checker already missed one (RA2 slot 20) | Medium | R-OSP-7 clause 7; close `slotkind.py`'s known gap before Phase 4 exits |
| 9 | Bot AI cost with many bots on large maps | Medium | R-BOT-23 budget, measured, with the shortfall reported rather than hidden |
| 10 | Scope: six donors, ~180k lines of donor code | High | Phase gates in §9, each ending on a playable state; no phase depends on a later one |
| 11 | Divergence between this spec and the tree | Medium | R-CONV-4/5; the spec is amended in the same commit as the behaviour |
| 12 | A Q2PRO fix silently never arrives, because the donor that carried the file had no site for it | High | R-CORE-12's ledger, R-VER-10; measured today at 14 commits missing from tourney, 7 from RA2, 4 from CTF |
| 13 | The bundles are lost and §7 becomes unauditable — the live repos squashed the per-commit history | High | R-PROV-1/2 vendor them and pin every branch SHA. The `rerere` cache was **not** lost after all: rescued 2026-08-21 and committed per R-PROV-2a/R-TOOL-4 |
| 14 | An entity key or savegame descriptor breaks silently — text contracts with no compiler on them, and the replay broke both before | Medium | R-KEY-1..3, R-SAVE-3a, R-VER-11/14, all wired into the build |
| 15 | `p_client.c` — 3,749 diff lines across five donors in one merged file (R-CORE-7) — becomes unreviewable | High | Phase 1 cuts the dispatch seam before any donor merge; decompose-and-replay keeps each step reviewable |
| 16 | Four menu engines contend for one client input channel (R-MENU-1) and two end up open at once | Medium | R-MENU-2a makes the single-owner rule structural, not advisory: one field, one open path that closes the incumbent, and a per-donor menu walk as a release check |
| 17 | Re-expressing the 17 live TOURNEY blocks (R-BOT-29) breaks bot behaviour under `tourney`, which currently works | High | Enumerate all 17 with their gate targets before touching any; R-VER-3 bot matrix run under `tourney` before and after; the `bots_*` namespace preserved per R-OSP-11 |
| 18 | Staged exposure means the first real players are the security test (R-SEC-2) | High | Accepted deliberately. R-SEC-1..9 all still stand and the inherited upstream fixes are the floor; the mitigation is that exposure widens only after the private stage holds |
| 19 | Development host and player host diverge: everything is written and *run* on **aarch64** (R-BUILD-6) while Q2 players are on x86-64 and win32 | Low–Medium | Reduced in 1.4 from Medium: all four player-facing targets now **cross-build from the dev host** (R-BUILD-5), so a compile or link regression on any of them fails here rather than in the field, and all five are gating in R-VER-8. What is left is narrow but real — nothing x86 is *executed* here, so anything that survives compilation and differs at runtime (pointer width, struct padding, the by-value `bsp_trace_t` of R-BOT-5, the botlib bitness handshake of R-BOT-4) is caught only when the user runs it. Endianness is not the exposure: all five targets are little-endian and R-BASE-7 already forbids `Swap_Init`. Mitigation is to make the 32-bit PE build part of the routine build set rather than an occasional check, since it exercises the widest gap from the host in one artifact |
| 20 | A pin in this spec silently stops resolving, as all three donor pins did between 1.3 and 1.4 | Medium | §3.3's re-pin plus R-PROV-6: the bundles are the durable half of the provenance and their refs cannot drift. A pin check belongs in the R-TOOL-3 build audits, so the failure is loud rather than discovered by a re-read |

---

## 12. Open decisions — **empty as of 1.4**

Every question in this section is closed. Q1, Q2 and Q9 closed in 1.1 on
evidence; Q3–Q8 and Q10–Q14 closed on 2026-08-21 with the author across seven
rounds. **Two more were decided in 1.4 without ever having been posed here**,
both of them consequences of the machine change rather than of the design: the
build-target matrix on an aarch64 host — aarch64 ELF becomes the primary native
target, the two PE targets stay gating, and the two x86 ELF targets are deferred
and marked unverified (R-BUILD-5, R-BUILD-6, R-VER-8, risk 19) — and the
provenance re-pin, which pins what this machine has rather than restoring the
identifiers 1.0–1.3 measured (§3.3, R-PROV-5). Recording them here rather than
only in the requirements is the §12 discipline applied retroactively: a decision
that was never a question is the kind that gets re-litigated later. The decisions are recorded where they belong — in the requirements they
affect — and the closures are kept here so the reasoning is not lost.

**Summary of the closures.** Q3: `g_ruleset` as a string with R-MODE-2's alias
layer. Q4: superseded — the botlib boundary is dynamic loading with a *versioned*
`botlib.h` (R-BOT-1), because the 1999 binary stopped being a target. Q5: one
`src/bot/` on `osp-tourney`'s `bl_*`, with the TOURNEY block becoming a runtime
gate (R-BOT-29). Q6: **three** observer systems, one per ruleset, the Gladiator
one taken from the reconstruction (R-EXTRA-6). Q7: all three map rotations kept,
one per ruleset (R-OSP-9) — and `openra2` was examined as an alternative arena
donor and rejected on facts: it is a fresh build on OpenFFA whose author states
he never had RA2 source, it caps at 8 arenas and 5 teams against RA2's 32 and
256, it is on `GAME_API_VERSION 3` with no `game.csr` and stub savegame exports,
and it has **zero** bot surface. Q8: all four stats logs kept, one per ruleset
(R-OSP-10). Q10: tourney's `bots_*` namespace and existing bot policy preserved
(R-OSP-11). Q11: `sp_dm` **dropped** (N8), and Colored Hitman with it (N7).
Q12: the reconciliation ledger is one row per (file, donor, hunk-cluster),
generated then annotated. Q13: two independent content bits in one latched field
(R-CORE-11a). Q14: CTF × monsters is unproven rather than broken — one added row
in R-VER-2.

A new question goes here, not into a commit message. The section is expected to
refill; it is empty only because the tree has no code yet.

### The questions as originally posed

Kept verbatim so the reasoning survives, not because anything below is open.
Every outcome is in the summary above; where a body's recommendation differs
from the outcome, the outcome won. **Phase numbers in these bodies predate the
1.2 re-cut** (§9) and should be read as the 1.0–1.1 numbering.

* **Q1 (Phase 0) — CLOSED in 1.1.** Pin which Q2PRO commit `inc/` and `src/`
  come from. `q2pro` is on `feature/mission-packs` at `b1f2c18f`
  (`r1504-2231-gb1f2c18f`), which is the only branch carrying `src/ctf`,
  `src/xatrix` and `src/rogue`. **Decision:** pin `b1f2c18f` and record it in
  `doc/provenance.md`, with two things stated there that 1.0 did not know:
  `src/{ctf,xatrix,rogue}` arrived on that branch in *two* commits (`ea14df30`,
  `2753e99f`) and are replay outputs, not upstream Q2PRO (§3); and the
  per-commit history behind them lives only in
  `q2pro-mission-pack-replay.bundle`, so the pin covers the bundle SHAs too
  (R-PROV-1). Re-base deliberately rather than tracking a branch.
  **[1.4: the three SHAs in this body no longer resolve — the decision stands,
  the identifiers are superseded by §3.3. Do not pin from this paragraph.]**
* **Q2 (Phase 0) — CLOSED in 1.1.** Should Colosseum live in its own git
  repository, or as a branch/subtree of one of the existing ones?
  **Decision:** its own repo. It is not a reconstruction and must not inherit
  any tree's `asm_matching` oracles or authenticity claims — nor the address
  annotations still sitting above most functions in RA2 and tourney, which are
  accurate on their `main` and meaningless on the branches Colosseum takes from.
  The repo carries the three bundles under `vendor/replay/` (R-PROV-1), which is
  what makes a standalone repo safe rather than a loss of provenance.
* **Q3 (Phase 1).** `g_ruleset` as a string, or keep three booleans with
  arbitration? **Recommend:** the string, with R-MODE-2's alias layer. Three
  booleans is what forced the 1999 build to call `gi.error` on conflicts.
* **Q4 (Phase 7).** Dynamic botlib loading only, or also a static link?
  **Recommend:** dynamic as primary (R-BOT-3) — it is how the original
  integrates, and it keeps the 1999 binaries usable — with a static option for
  a single-file 64-bit deployment.
* **Q5 (Phase 6/8).** OSP Tourney's own bot glue and Gladiator's are the same
  code with tourney's `TOURNEY` block; the tree only needs one. **Recommend:**
  one `src/bot/`, tourney's `TOURNEY`-gated additions folded into the ruleset
  dispatch, `osp-tourney`'s ported files as the base.
* **Q6 (Phase 4).** Which observer/camera implementation wins — Gladiator's
  `p_observer.c` + `camera_t`, or OSP's `osp_observe.c` + `p_camera.c`?
  **Recommend:** OSP's; it is larger, more capable and already ported. Keep
  Gladiator's chase-cam menu interaction fix (v0.93) as a behaviour.
* **Q7 (Phase 5/6).** Which map rotation wins between `sv_maplist`, RA2's
  `maploop.c` and OSP's `osp_maps.c`? **Recommend:** keep all three, one active
  per ruleset, with `sv_maplist` as the `dm`/`sp` default — rotation is
  genuinely ruleset-shaped, so §7 rule 6 does not apply cleanly. Confirm.
* **Q8 (Phase 9).** Should the two local stats logs be unified into one format?
  **Recommend:** no. RA2's per-round log and tourney's `osptourney.jsonl` record
  different events; keep both, active per ruleset.
* **Q9 (Phase 2) — CLOSED in 1.1.** `m_move2.c` — build it or drop it? The diff
  settles it: **drop it.** It defines all nine of `m_move.c`'s functions with
  external linkage against `m_move.c`'s four, so building both is a
  multiple-definition link error — which *is* the link error the 1999 Gladiator
  readme documents, not evidence against the reading. It is a stale copy
  predating `IsBadAhead` and the `static` pass, not a live alternative. R-MP-5 is
  amended accordingly and the file ships unbuilt, as reference.
* **Q10 (Phase 8).** Bot support under `tourney` — full parity with `dm`, or
  match-play only? The 1999 `TOURNEY` block existed but was `#if 0`-adjacent
  (`//#define TOURNEY` in `bl_main.c`). **Recommend:** full parity, with bots
  excluded from ranked match play by default.
* **Q11 (Phase 9).** Ship `sp_dm` (R-SP-6) with monsters *and* bots in the same
  level, given the Gladiator botlib never navigated around monsters?
  **Recommend:** ship it, off by default, documented as experimental.
* **Q12 (Phase 0).** How is `doc/reconciliation.md` generated? R-PROV-3 makes
  the row skeleton computable from `git diff baseq2 port_<donor>`, but the five
  diffs overlap in 29 shared files and a naive per-file row loses the per-hunk
  detail the merge actually needs. **Recommend:** one row per (file, donor,
  hunk-cluster), generated, then annotated by hand with the rule applied and the
  outcome. Confirm the granularity before Phase 0 exits, because every later
  phase writes into this file.
* **Q13 (Phase 2).** R-CORE-11a puts a content flavour on the edict. Is one
  field enough, or does Xatrix × Rogue need two independent bits — a monster
  that is a Rogue variant on a map running with `xatrix 1` as well?
  **Recommend:** two bits latched as one field, set at `monster_start`, because
  R-MODE-3 already allows both layers at once and a single enum would forbid the
  combination the composability matrix promises.
* **Q14 (Phase 3).** `q2pro/src/ctf` deleted the monster set (R-CORE-8), so
  Threewave's edits to `g_ai.c`, `g_monster.c`, `g_combat.c` and `g_turret.c`
  have never been compiled against live monster code, and R-MODE-7 promises
  monsters under `ctf`. **Recommend:** treat the CTF monster combination as
  unproven rather than broken — add it to the R-VER-2 boot matrix explicitly
  (`ctf` × monsters on a map that has them) and expect findings.

---

## Appendix A — Botlib contract quick reference

Versioned by R-BOT-1 — *not frozen; corrected in 1.4, the freeze was lifted in
1.2 and this line was missed.* Authoritative copy in `doc/botlib-contract.md`.

**`bot_export_t`** (game → botlib): `BotVersion`, `BotSetupLibrary`,
`BotShutdownLibrary`, `BotLibraryInitialized`, `BotLibVarSet`, `BotDefine`,
`BotLoadMap`, `BotSetupClient`, `BotShutdownClient`, `BotMoveClient`,
`BotClientSettings`, `BotSettings`, `BotStartFrame`, `BotUpdateClient`,
`BotUpdateEntity`, `BotAddSound`, `BotAddPointLight`, `BotAI`,
`BotConsoleMessage`, `Test`.

**`bot_import_t`** (botlib → game): `BotInput`, `BotClientCommand`, `Print`,
`Trace`, `PointContents`, `GetMemory`, `FreeMemory`, `DebugLineCreate`,
`DebugLineDelete`, `DebugLineShow`.

**Entry point:** `bot_export_t *GetBotAPI(bot_import_t *import)`.

**Action flags:** `ATTACK` 1, `USE` 2, `RESPAWN` 4, `JUMP`/`MOVEUP` 8,
`CROUCH`/`MOVEDOWN` 16, `MOVEFORWARD` 32, `MOVEBACK` 64, `MOVELEFT` 128,
`MOVERIGHT` 256, `DELAYEDJUMP` 512.

**Error codes:** `BLERR_NOERROR` 0 through `BLERR_INVALIDSOUNDINDEX` 32.

**Physics libvars the brain expects** (defaults from `botlib.h`):
`sv_friction` 6, `sv_stopspeed` 100, `sv_gravity` 800, `sv_waterfriction` 1,
`sv_watergravity` 400, `sv_maxvelocity` 300, `sv_maxwalkvelocity` 300,
`sv_maxcrouchvelocity` 100, `sv_maxswimvelocity` 150,
`sv_maxacceleration` 2200, `sv_airaccelerate` 0, `sv_maxstep` 18,
`sv_maxbarrier` 50, `sv_maxsteepness` 0.7, `sv_jumpvel` 224,
`sv_maxwaterjump` 20.

## Appendix B — Reference paths

| What | Path |
|---|---|
| Q2PRO base + mission packs | `q2pro/src/{game,ctf,xatrix,rogue}` @ `c751d316` (§3.3; `3ffc4642` is the last commit touching `src/`) |
| Q2PRO engine headers | `q2pro/inc/{shared,common,format}` |
| RA2, ported | `rocketarena2-public` @ `q2pro-enhancements` = `cd0708b` (§3.3) |
| OSP Tourney, ported (and the ported `bl_*`) | `osp-tourney` @ `q2pro-enhancements` = `a8e30d0` (§3.3) |
| Gladiator, 1999 original | `gladq2_src` — **reference only**, mtime 1999-08-02, no `p_observer.c` |
| Gladiator, reconstruction | `gladiator-bot-restored/game` — **reference only, read this one.** *Not* the same tree as `gladq2_src`; 17 files differ |
| Gladiator botlib (the brain) | `gladiator-bot-restored/botlib` |
| Gladiator runtime assets | `gladiator-bot-restored/assets` — `bots.cfg`, `Gladiator.gsl`, `default/`, `maps/`, `pak7.pak`, **32** prebuilt `.aas` (1.0–1.3 said 17) |
| **Rescued harness archive** | `../colosseum-harness-archive` — `rr-cache` (411+85), `commit_paths.txt`, `style_residual.patch`, `osp/`, id's five source releases, astyle 3.1, and the harness `TODO.md` (R-PROV-2a, R-VER-15) |
| uGladQ2 cleanup | `ugladq2` — **reference only** |
| Port method write-ups | `rocketarena2-public/doc/q2pro-port.md`, `osp-tourney/doc/q2pro-port.md`, `q2pro/doc/mission-packs.md` |
| Replay bundles (§3.2) | `../osp-q2pro-port-replay.bundle`, `../ra2-q2pro-port-replay.bundle`, `../q2pro-mission-pack-replay.bundle` |
| Replay + audit toolchain | `q2pro-mission-pack-tools` (R-TOOL-1) |
| RA2 kept-bug review | `rocketarena2-public/CLAUDE.md` §"Deliberately-kept original bugs" + `docs/FUNCTION_NOTES.md` (R-SEC-2). **The `docs/SECURITY_REVIEW.md` of 1.0–1.3 exists in no branch and no commit** |
| Donor comparison notes | `game-comparison-q2pro-vs-yquake2.md`, `q2pro-game-changes.md`, `yquake2-game-changes.md`, `key-contract-findings.md` — **absent from this machine.** They were WSL-side working notes; nothing in §5–§10 depends on them, so they are recorded as lost rather than sought |
| Rescued style tool | `../colosseum-harness-archive/astyle/astyle-3.1` — x86-64 ELF, byte-identical output to the host's native `astyle` 3.1 (R-BUILD-6); needed only if the host one changes version |
