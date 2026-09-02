# Colosseum — Specification

A single modern Quake II game library that unites baseq2, both mission packs,
Threewave CTF, Rocket Arena 2 and OSP Tourney DM on one Q2PRO-derived code
base, and restores full Gladiator Bot command and botlib support on top of it.

| | |
|---|---|
| **Project name** | Colosseum |
| **Artifact** | `game<cpu>.so` / `game<cpu>.dll` (Q2PRO game API), gamedir `colosseum` |
| **Working directory** | `<workspace>/colosseum` |
| **Spec version** | 1.49 — 2026-09-02 |
| **Status** | **Phase 8 of nine.** Built, played and audited: seven rulesets and two content layers from one library, bots in the six that take them, ten build configurations warning-clean under gcc and clang at both game ABIs. `README.md` has the short version. |
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
| **Amendment 1.11** | **Phase 3 opens with the slots, because the CTF merge needs somewhere to put 17, 18 and 19.** The per-ruleset slot map (R-OSP-7) and the composed statusbar (R-OSP-7a) are implemented and observed: `sv slots` (new, R-VER-19) prints the resolved map and the emitted bar, and the `dm` bar it emits is **token-identical** to the two literals it replaced. **Three findings changed requirements.** R-OSP-7 clause 3's "one header" and clause 7's "a tool checks it" conflict in C, resolved by an X-macro both the compiler and `slotkind.py` read — and the tool takes the column order from the macro signature rather than assuming it. CTF's timer pair goes to **32/33**, which clause 6 permits as droppable content and which upstream could not do at all with a literal bar, so under `ctf` the second powerup timer now displays on an extended server where before it could not display anywhere. And `slotkind.py`'s "known gap" was **not a missing special case but a missing question**: the old check keyed on *kind* and found RA2's slot 20 only because its two claimants happened to disagree about kind; the availability check keys on the **number** and now reports 17, 18 *and* 19 on `q2pro/src/ctf` where it reported one. Its five controls ship inside the tool and run in `make check`. Changed: R-OSP-7 clauses 3/4/6/7, R-OSP-7a, R-TOOL-1, new R-VER-19, §9 Phase 3. |
| **Amendment 1.12** | **Phase 3 complete: Threewave CTF runs on the dispatch.** 20 shared files merged, 4 donor-only into `src/ctf/`, and **63% of the donor's 265 hunks against the spine are stale copies of id's code rather than CTF's feature** — §7 rule 1 decides all 167 without a judgement call. **Five corrections of fact, four findings that needed running and four that needed reading.** R-CTF-1 says "all five techs"; Threewave 1.52 ships **four** (`item_tech1..4`) and five is tourney's rune count. R-CTF-5 understates the problem: Threewave does not merely lack baseq2's spectator flag, it **deletes** both fields, the userinfo key, the password, `maxspectators`, `spectator_respawn()` and `GetChaseTarget()` — resolved by one named predicate, `G_IsObserver()`, at thirteen sites. **The donor's CTF cannot start a level** — built from upstream's own build system and run, not inferred: `CTFInit()` is called by nothing in `port_ctf` or in `q2pro/src/ctf`, so `ctf`, `competition`, `capturelimit` and `instantweap` are all null and the first `+map` faults in `SP_worldspawn`. The `xatrix` and `rogue` packs from the same branch and build boot clean, which is the control that makes it the CTF library rather than the branch. **`game.maxclients = game.maxclients;` has been in `src/g_main.c` since Phase 2** — a self-assignment that leaves maxclients at 0, invisible to every static check and to every dedicated-server boot with no client connecting. Measured across all five trees it is a **replay artefact**: `port_ctf` and `port_rogue` carry it, `baseq2` and `port_xatrix` do not, and upstream's three worktrees all read `maxclients->value` — so Colosseum took it from the rogue merge, and §7 rule 1 is what stopped `port_ctf` re-introducing it. The composed statusbar dropped an `if` while keeping its body under `ctf`, which 1.11's token-identical `dm` bar could not see. Read rather than run: `CTFObserver` resets the grapple for the client who is *already* an observer, so a player who types `observer` mid-swing keeps being winched; `ctf_PMenu_Close` left `menu_owner` set at eighteen call sites; baseq2's `spectator` userinfo key was still reachable under `ctf`, giving one client two observer systems; and re-applying `372e2503fe47` found a **third** unguarded `G_PickTarget` site that Phase 2 had introduced (R-VER-10 now records which re-apply rows are landed). And **the Makefile had no header dependencies**, so adding a `level_locals_t` field rebuilt nothing and the next `make` linked objects compiled against two struct layouts, cleanly: new R-BUILD-9. Q14 is answered — `ctf` × monsters runs on all three campaigns, twelve boots clean, and the reason is that CTF never edited the monster code, it deleted it. **And §7 rule 6's grapple count is wrong**: it says three implementations, and measuring finds **one** — `osp_hook.c` sets `CTF_GRAPPLE_STATE_FLY` by name and RA2 declares `ctf_grapplestate` and carries `CTFResetGrapple` verbatim, so both are Threewave's forked. The resolution is one core plus three tunable sets rather than a merit choice between designs. Changed: R-CTF-1/3/5/7, R-MENU-1/3/4/5, R-CORE-14, new R-BUILD-9, R-VER-2, R-VER-10, new R-VER-19 (1.11), §7 rule 6, §9 Phase 3. |
| **Amendment 1.13** | **Real Threewave assets, and the first check that waits rather than counts.** All nine shipped CTF maps boot with both flags at base, all four techs spawned, team spawn points and banners present — R-CTF-1 observed rather than argued. Getting there found the **worst defect in the tree**: `nextthink` is an `int` frame count and `src/g_phys.c`'s `SV_RunThink` compares it against `level.time` in *seconds*, so **every think in the game fires ten times late** — doors, plats, item respawns, trigger delays, every monster animation frame. Measured exactly: a spawner with `nextthink = 20` fired at frame ~200. It is **upstream's**, live in the shipped Ground Zero pack (`q2pro/src/rogue`), and Colosseum inherited it at `d3c6d83`; `port_ctf` and `port_xatrix` both have the correct version, so the CTF merge did not cause it and §7 rule 1 would have refused it. A second, opposite upstream defect collides with it: Xatrix has 43 `nextthink = level.time + N` sites, identical in `q2pro/src/xatrix`, which are correct only under the reverted comparison. **Recorded, not patched** — fixing one half moves the breakage, so it wants one pass with the full matrix behind it. And it explains a gap in the whole evidence base: every check here is static or a spawn-time census, and all of Phase 2's observations remain true with all timing 10× wrong. Changed: R-VER-1, R-VER-2, new R-VER-20, §9 Phase 3 evidence, §11 new risk 20. |
| **Amendment 1.14** | **Upstream's bugfix commit imported (`q2pro@38873740`), and it found more of the class than 1.13 did.** 32 files, 111 hunks, mapped hunk-by-hunk because upstream ships four separate libraries and this is one merged tree — a fix to `src/xatrix/g_func.c` lands in our `src/g_func.c`. **R-54 is fixed**: `SV_RunThink` restored *and* Xatrix's 43 `nextthink = level.time` sites converted with `tools/nextthink.py`, whose output is byte-identical to upstream's. Beyond it: `trail_framenum` read against seconds at five sites, plat2's `last_move_framenum`, the trap's `timestamp` (read **and** both writes), the intermittent `trigger_push` fed from two clocks at once, the intermission wait at 0.5s, the drowning gasp at 1.1s, two `/ FRAMETIME` quad-drop timeouts, and a `8000 / speed * BASE_FRAMERATE` precedence bug in three projectiles. **Three corrections of our own work.** Phase 2 *silenced* six `abs()`-on-float defects with an `(int)` cast rather than fixing them — the cast truncates before taking the magnitude, which was the bug. Phase 3 kept `BecomeExplosion1`'s wrong-team message under §7 rule 2; §7 rule 7 makes q2pro's fix cumulative, so it is ours. And Phase 3's `CTFObserver` fix is replaced by upstream's, which restores the braces, message and `return` the 1.52 release had — a better reading of the original than my inference. **One defect the import could not contain**, because it exists only in a merged tree: `attack_finished` is `float` here (rogue's declaration won R-CORE-6's union) while three Gekk sites write frames, agreeing with each other and disagreeing with the shared `M_CheckAttack` every monster reaches. New `tools/units.py` (R-VER-21) is upstream's two greps plus that third, merged-tree check, with three controls; new `tools/encoding.py` because writing files as latin-1 broke `grep` silently — twice. R-VER-20 is now two real temporal assertions: techs at frame 31 not 221, and `base1`'s gib freed by frame 331. Changed: R-VER-1, new R-VER-21, §9 Phase 3 evidence, §11 risk 20. |
| **Amendment 1.15** | **The harness has a client, and the first run of it dropped the server.** Every phase so far recorded "needs a client" as a property of the machine; it was a property of nobody having tried. q2pro's own client runs under `Xvfb` on Mesa's software GLX, `cl_beginmapcmd` executes a console script the moment the map loads, `wait N` paces it in frames and `viewpos` makes movement observable — a driven player went from `(1744 480 686)` to `(1528 480 654)` on `q2ctf1` having joined the RED team on the way. `tools/play.sh` is that, parameterised (new R-VER-23). **Its first run found `ERROR: PF_configstring: bad index: 13118`** — `CS_GENERAL` exactly. This library is compiled with the protocol extensions so the constant is 13118, but their *use* is negotiated at runtime and `g_protocol_extensions` **defaults to 0**, where the engine's end is 2080: so a stock `ctf` server died on the first client connect, on every connect, and three phases of static checks and boots could not see it because none of them was ever a player. Three sites fixed to `game.csr.general`, the runtime remap the line four above already used; a fourth of the same kind found by reading — `CONFIG_CTF_MATCH`/`CONFIG_CTF_TEAMINFO` compile to 57/58 and land inside the old model table — fixed to `game.csr.airaccel`. All four are upstream's verbatim, and the control that would have settled it by running cannot be run: upstream's own CTF still segfaults on `+map` (R-42, unchanged by the `38873740` import). And Phase 3's last mechanised residue lands: `tools/allocpairs.py` (new R-VER-22), the matched-pair allocator audit R-55 asked for, shaped like `units.py` because the unit that has to agree is the pointer — a libc ban would fail the build on `g_main.c`'s correct inherited `strdup`/`free` pair, and a file-level check describes that same one file. Changed: R-CTF-6, new R-VER-22, new R-VER-23, R-VER-3, §9 Phase 3 evidence, §11 risk 21. `doc/reconciliation.md` R-62..R-63. |
| **Amendment 1.16** | **Phase 4 opens: R-26 gains a sixth rule, and the slots land first again.** RA2 is the first donor that **deletes**, and R-26's five rules are complete only for donors that add — rule 2 ("base equals ours, take theirs") silently took RA2's removal of `M_walkmove`, `M_CheckBottom` and the turret driver's two `infantry_*` calls, all of which **link and run and are wrong**, because Colosseum keeps the monsters RA2 dropped (R-CORE-8) and the callee still exists. New `tools/lostref.py` (R-VER-24) is the sixth rule mechanised, with the three real deletions as its controls, and it only works because it strips comments first — RA2's `barrel_touch` explains the removal in a comment that names the function. **The merge is 141 conflicts before normalisation and 91 after**: 1,284 asm-matching address comment lines and a stripped GPL header are not RA2's feature and are removed before `git merge-file` sees them (new `tools/donorprep.py`), which is Phase 5's pass too. Eight of the 24 shared files turn out to carry **no RA2 feature at all** — pure monster-deletion residue — and are decided by R-CORE-8 alone. `src/arena/` is imported (5,300 lines, GameSpy already gone upstream at `789895b`), the pin moves to `d20e1ce` for its `-Wall` fixes with **one of them refused** because it is a consequence of the deletion we do not replay, and six symbol collisions are resolved by rule: RA2's 283-line grapple **deleted** (§7 rule 6, R-50 — it is Threewave's fork), `stuffcmd` moved to `g_utils.c`, `showmenu` replaced by R-MENU-2a's single owner field. R-OSP-7 gains ten arena rows measured off the donor's own bar rather than its field names — **two of which lie**, `STAT_QUEUE*_ICON` being drawn with `stat_string` — and the composed arena bar is **token-identical** to RA2's literal plus the second powerup timer at 26/27 that a literal could not carry. Factoring the composition so `sv slots` cannot print a bar the server does not install made `slotkind.py` go quiet, which its own control caught. Changed: R-CORE-8, R-OSP-7, R-OSP-7a, R-EXTRA-6, R-MENU-1, new R-VER-24, §3 RA2 pin, §5.2, §9 Phase 4, §11 risk 22. `doc/reconciliation.md` R-64..R-69. **Not done: the sixteen shared-file behaviour merges, so `arena` still plays as `dm`.** |
| **Amendment 1.17** | **All sixteen shared-file merges landed; `arena` stops inheriting `dm`.** The 91 conflicts were decided by one fact about the merged struct: **`FIGHT_SPECTATING` is 0**, so `resp.fightstate == FIGHT_ALIVE` is false for every client under `dm`, `ctf` and `sp`. RA2 tests it in a dozen shared places, and six of those, inherited ungated, *delete* behaviour from the whole game rather than adding any to arena — the teleport splash, the landing sound, falling damage, footsteps, every weapon animation, and telefragging. The sharpest is `KillBox`, which RA2 opens with `if (!ent->client) return true;` because in RA2 only clients telefrag: this tree calls it from **eight non-client sites**. **A donor's `if` is a claim about that donor's world**, and none of these arrives as a conflict — they sit inside hunks the five rules resolve cleanly. **Four things RA2 asked for are refused**: `deathmatch` registered `CVAR_NOSET` (the dispatch already forces deathmatch for every ruleset but sp, and RA2's registration would make coop unreachable *everywhere*), dropping the `showscores` descriptor (RA2 replaced the field; both rulesets still need theirs), deleting easy mode and the dmflags friendly-fire avoidance, and RA2's whole `ClientObituary` — a stale fork of id's with announcer sounds bolted on, which §7 rule 1 decides. **R-OSP-9 is fixed by placement rather than by a check**: `maploop.c` is the arena `EndLevel` row, so only one rotation is reachable, where the donor's port put `get_next_map()` inside `EndDMLevel` and had both. Six RA2 functions moved out of the spine files into `src/arena/` — the three scoreboards, the skin icon, the queue stats, the ZBot sampler — and `RA_ScoreboardMessage` is a **dispatch row**, not a gate. Three command names collided (`admin`, `playerlist`, `score`) and each got a ruleset arm rather than a rename. **Two defects found by reading the donor**: `menu_centerprint` dereferences a possibly-null `ent->client` on any `G_UseTargets` message, and `grhurt.wav` is played by `CTFGrapplePull()` and precached by nobody — §7 rule 7 puts the precache on Threewave's row. And **the boot matrix caught what no static check could**: `multi_arena_think()` wired before `arena_init()` had a call site walked a NULL team list and segfaulted all four arena rows, compiling and linking clean. `arena` now boots `q2dm1` at 37 edicts against `dm`'s 120. Changed: R-RA-1/2/6, R-ARENA-2, R-EXTRA-6, R-MENU-1/4, R-OSP-9, R-SAVE-3a, §9 Phase 4, §11 risk 22. `doc/reconciliation.md` R-70..R-74. **Not done: R-RA-3's round state machine is still `CheckDMRules`, and no arena match has been played.** |
| **Amendment 1.18** | **R-RA-3 and R-RA-4 discharged, and "no RA2 specifics ungated" became a check rather than a claim.** R-70's six ungated sites were found by reading, which is not a method, so the question is now `tools/donorgate.py` (new R-VER-25): a donor's own field or function, used in a **shared** file, must be inside that donor's gate. `gates.py` cannot see this class — those sites test a *field*, and the field is legitimately in R-CORE-6's union. The donor's surface is computed rather than listed, so `stuffcmd` moving into `g_utils.c` stopped being reported without an edit. **77 uses, 0 ungated — after one real finding**: `G_UseTargets` called `menu_centerprint` with no gate, and though it is *transparent* (it falls back to `gi.centerprintf`), "it happens to be transparent" is not a gate, so the exemption list stayed empty. **R-RA-3's seven states were never missing** — `arena_think()` has all of them — what was missing is that RA2's port calls `multi_arena_think()` from `G_RunFrame` beside `CheckDMRules()` with no statement of how two match-rule systems compose. `RA_CheckRules` is now the `CheckRules` row and says it: the round machine runs a **round**, `CheckDMRules` ends the **level**, and the order is fixed by the row instead of by insertion point — the same correction R-72 made to the map rotation. **R-RA-4 is thirteen of seventeen already true**, three Phase 7's, and one real gap: uGladQ2 v0.98.1u's *"observers ignored for spawn points"* is **not** fixed in RA2, and an arena observer is alive, noclipping and usually parked over the spawns, so "farthest from any player" was choosing by where the audience stood. Fixing it made `G_IsObserver()` answer for `arena` — a third spelling of one question — which in turn made two baseq2 chase-cam blocks reachable for observers who have their own four modes, so both are now excluded under `arena`: two observer systems live at once is the bug §7 rule 6 names, and the third answer is what would have made them live. Changed: R-RA-3, R-RA-4, R-ARENA-2, R-CTF-5, R-EXTRA-6, R-MODE-5, new R-VER-25, §9 Phase 4, §11 risk 23. `doc/reconciliation.md` R-75..R-77. **Still not claimed: no arena round has been played.** |
| **Amendment 1.19** | **Phase 4's programming closed; Phase 5 opens with the tourney import.** R-RA-2 measured rather than quoted: **all 39 RA2 client commands and all 48 cvars are present** — 48, not the 47 §3 claims, and the tool is the authority (R-TOOL-2). `counts.py --duplicates` also found R-COMPAT-6's real instance: the `game` cvar re-obtained with two different defaults across four translation units, now one. **Then osp-tourney.** 71 files, 27 shared, 171 conflicts after normalisation and 128 by hand — and the normalisation itself was the first finding: `donorprep.py` reported **zero** asm-matching comments and was wrong, because the two reconstructions spell them differently (`/* gamei386.so 0x... */` versus `// gamei386.so: ...`) and tourney has **1,910** of the second form. A tool that reports zero is indistinguishable from one that found nothing. **`client_respawn_t` arrives with 80 members named by byte offset**, 62 of them referenced 692 times, and they are carried verbatim — the opposite of R-67's decision on RA2's padding, and the difference is measurable: RA2's were never referenced. **R-58's class hit three times in one import**: `pers.spectator` is a bool here and tourney's *speed-cheat strike counter* (so the first strike would have made the player a spectator), `resp.entered` is RA2's bool and tourney's four-state enum, and `InitClientPersistant`'s partial reset computes its memset offset from the assumption that userinfo/netname/greenname are the **first** members of the struct — which R-CORE-6's union made false. The compiler caught the first two only because `-Wbool-compare` can see them; the third is arithmetic that is right in the donor and wrong here, and **no check in this project can see that**. **Eleven configstrings hardcoded as `0x620..0x62a`**, which is `CS_GENERAL_OLD` — R-62 in a third donor and the only one spelled in hex, so no grep for `CS_` would have found it; all 71 sites now go through `game.csr`. **R-CORE-14 was read wrong in Phase 2**: R-18 called `FL_BOTCLIENT`/`FL_OSP_BOT`/`FL_OSP_NOCMD` aliases of `FL_BOT`, and measuring the donor that defines all five shows they are aliases of **two different bits**; four names become two, not one. `src/tourney/` compiles and is **not linked** — it cannot be until the shared merges land — so `make check-tourney` compiles all twenty units instead (new R-VER-26), on R-MP-5's precedent. Changed: R-RA-2, R-COMPAT-6, R-CORE-14, R-CORE-6, R-OSP-5, R-OSP-7, new R-VER-26, §9 Phase 4 and 5, §11 risk 24. `doc/reconciliation.md` R-78..R-86. **Not done: the 26 shared-file merges, so no tourney code runs.** |
| **Amendment 1.20** | **A client that asserts, and three arena defects that needed one.** The play test the author deferred is now mechanised: `libq2` is a Go implementation of the Q2 network protocol, so a headless client can connect, spawn, read configstrings, layouts and centerprints, send console commands and push userinfo — everything except walk, which yaw-steering cannot do repeatably and which the ruleset code does not need. `tools/playtest.sh` (new **R-VER-27**) runs **108 assertions across five rulesets and three control servers**, and every check is a *difference*, because one library serving five rulesets makes "ctf maps the CTF stats" a fact about the table rather than about the dispatch. **It found three defects in `arena` that fifteen audits, ten build configurations and a twenty-row boot matrix could not**, all needing a client to exist before they can be wrong. `PutClientInServer`'s arena branch had lost the donor's tail — `move_to_arena(ent, resp.context, 1)` — and `init_player()` sets `fightstate` but touches neither `movetype` nor `solid`, so **a connecting player stood at a deathmatch spawn as a solid `PM_NORMAL` body** while `G_IsObserver()` answered *true* about that client: the game and the client disagreed and only the client could say so. Then the menu, twice: an RA2 menu **is** the client's statusbar, so `G_MenuClose()` has to repaint — the donor's `clear_menus()` did the clear and the repaint together and the unification kept only the clear, leaving the menu on screen after `score`, `inven`, intermission, respawn and disconnect; and closing **destroyed** the menu where RA2 only hides it, which broke `inven`'s reopen, `FinishMenu(show=false)` and the menu queue's own pop. `G_MenuClose()` now clears `menu_owner` before dispatching, which is what lets `DisplayMenu()` reach its restore branch, and `init_player()` owns the one reset a reused client slot needs. Confirmed by the same run, previously assertions: the composed bar **arrives byte-identical** in all five rulesets (442/658/578/770/310), R-CTF-7's userinfo half (`male/grunt` → `male/ctf_r` → `female/ctf_r`), R-38's dropped timer rows resolving at 32/33 with extensions on, R-MODE-4's refusal being a message that keeps serving, and R-RA-1's 37-against-120 edicts. **One finding left open: `MOD_RUNES` gates nothing** — CTF's techs come from `CTFSetupTechSpawn` on `DF_CTF_NO_TECH` alone and tourney's from the `runes_enable` **bitmask**, so `G_ModifierEnabled(MOD_RUNES)` has exactly one caller, `sv ruleset`, printing it. Every available fix changes behaviour, so it is recorded rather than decided. **And `make everything` was broken on both Windows targets**: `src/bot/bl_main.h` declares the bot library handle as `HANDLE` under `#if defined(_WIN32)`, which needs `<windows.h>` — and `<windows.h>` redefines the `MAX_PATH` the same header defines four lines above, so the include trades one `-Werror` break for another. Win32's `HANDLE` *is* `void *`, so one member serves both and the `#if` goes with it. `make native` compiles neither arm of an `#if defined(_WIN32)`, which is what the other nine configurations are for. Two manual checks became scripts in the same pass — **`tools/bootmatrix.sh`** for R-VER-2 and **`tools/smoke.sh`** for R-VER-17, each with a positive control — and writing them found that the boot matrix had only ever checked that the process survived, never that the ruleset it asked for was the one that resolved, and that **R-VER-17's illustrative number has been stale since 1.5**: `base1` in co-op reads **2 inhibited, not 28**, because Phase 2's Rogue merge brought the co-op arm that stock q2pro has commented out — upstream's fall-through deletes the entities marked `!easy & !med & !hard`, and that marking means *co-op only*. Changed: R-RA-4, R-MENU-2a, R-MENU-3, R-MENU-4, R-VER-2, R-VER-17, new R-VER-27, §9 Phase 5. `doc/reconciliation.md` R-87..R-90. |
| **Amendment 1.21** | **Phase 5 closes: tourney plays.** The 26 shared-file merges landed and the measure is not the conflict count — **62 of tourney's 349 exported functions were defined and never called from anywhere before this increment; now none are.** The merges were run three-way and used as a *guide*, not applied: a clean merge of `g_weapon.c`, zero conflicts, would have deleted `fire_hit` with the donor's monster set (R-CORE-8, R-64's sixth rule). **One donor habit decided most of the work** — the accuracy report writes `p_acc[...]` inline at **fifteen** sites across `g_weapon.c` and `g_combat.c`, each a five-line copy with its own `sync_stat` guard and **three without one** — so the concept is expressed once in `src/tourney/osp_acc.c` and the spine calls two functions, with damage credited at the single choke point all fifteen were feeding (`T_Damage`, after armour): rocket, grenade and BFG splash had been credited **twice** on a direct hit. Eight more reductions of the same kind, and one new header: `osp_types.h` declares a dozen names baseq2 defines `static`, so a shared file including it is an error — **`src/tourney/osp_hooks.h`** is the surface a shared file may see, and `osp_types.h` includes it so signatures cannot drift. **Four findings.** **R-OSP-13 was never implemented** and R-VER-16 is what said so: `match_mode 4` announced `*** DM 1V1 MODE ***` and set `match_type` to `1-vs-1` while skipping all 34 `m_mode == 3` branches — the server told its clients it was a duel server and was not one; `-1` did the same. **R-COMPAT-6 had four instances, not one**: the `statsname` pair §9 names (now one registration in `g_ruleset.c` with a ruleset-chosen default), plus `gamedir` as `"tourney"`/`"ospdm"`, `hostname` as `""`/`"noname"`, and `port` defaulting to `"."` in `osp_hiscore.c` — `basedir`'s default copy-pasted. **R-OSP-5's bug was in the other donor**: `donorgate.py` gained a stray-`extern` pass and found `extern int votetries_setting;` atop `arena/arena.c`. And **R-OSP-6's five spawn keys were in the wrong struct** — carried on `edict_t`, absent from `spawn_temp_t` and `temp_fields[]`, so a tourney map setting any of them was rejected with "not a field". **R-OSP-2 measured**: `ClientCommand` compares **138** names and **137 are present**, the absent one being `_ngws_client_id`, which R-OSP-3 deletes — so §3's 137 is right *because* of the deletion; of **237** cvar names 199 are present, and every one of the 38 absent is either the NetGames stack (16), a `bl_*.c` registration that is Phase 6's (21), or `version`, read only by the logger that is gone. **R-OSP-11 is not claimed**: its seven `bots_*` cvars are present, but `minimumplayers` and `botfile` are registered by a `bl_*.c` file that arrives with the bot layer, and its behaviour half needs a bot to exist — §9 lists it under Phases 6 and 7 too. Changed: R-OSP-2, R-OSP-5, R-OSP-6, R-OSP-13, R-COMPAT-6, R-VER-16, R-VER-25, R-VER-26 discharged, §9 Phase 5. `doc/reconciliation.md` R-91..R-92. |
| **Amendment 1.22** | **Phase 6: the bot layer lands, and the Trace slot has two ABIs.** `src/bot/` is eight translation units — `osp-tourney`'s six ported `bl_*.c` plus the Gladiator reconstruction's `p_menulib.c`/`p_botmenu.c`, which `osp-tourney` cannot supply because it moved its bot menu onto id's `PMenu` — and `src/tourney/osp_botseam.c` is **deleted, not edited**, which is what R-86 said the `!G_BotsAllowed()` early-out was for. Bots load, spawn, navigate and **fight** in `dm`: `player was blasted by Trash / Trash was blasted by Zero / Zero was blasted by player`, on `q2dm1`, with the brain computing and writing its own `.aas`. **The finding of the phase is R-BOT-5's hazard, live (R-97).** `bsp_trace_t` is 88 bytes so it is never returned in registers; on 32-bit the hidden buffer IS the first visible argument and the two spellings compile to identical code, and **on x86-64 and aarch64 it is a hidden register**, so they are different ABIs. R-BOT-1 says to take `osp-tourney`'s `botlib.h` and that copy carries only the 32-bit spelling, because `osp-tourney` is a 32-bit port — taken verbatim it shifts every argument one place and `passent` receives a truncated pointer. Every trace returned an all-zero `bsp_trace_t`, so the brain believed it was wedged in solid everywhere and **three bots stood on their spawn points for a minute without firing a shot**, with nothing in the game library wrong. `botlib.h` now carries the brain's own `#if defined(__x86_64__) \|\| defined(__aarch64__)` split, character for character, and `doc/botlib-contract.md` records **contract version 2** — the first use R-BOT-1's versioning has had. Neither mitigation caught it: `BotVersion` returns the same string on both sides of an ABI split and `Test()` passes no struct by value. **Two more decisions.** R-BOT-10's staging message holds **typed records rather than bytes** (R-93), because Q2PRO's `WritePosition` is three DeltaInt23 values under protocol extensions and not three shorts, its `WriteFloat` is `Com_Error`, and re-encoding either would corrupt every position in every multicast on an extended server — replaying the recorded calls through the engine's own writers makes R-BOT-13 true by construction and drops `anorms.h` from the tree. And **R-88 is decided (R-96): a modifier is the resolved answer, not the request.** `G_ResolveModifiers()` derives `runes` from the switch the ruleset's own code reads — `DF_CTF_NO_TECH` under ctf, `rune_stat` under tourney — with the cvar folded in beforehand as a request that can only ever turn something on, so a default server is untouched, `runes 1` means something under ctf and tourney for the first time, the bitmask survives, and `sv ruleset` stops lying. `bots` is the same shape with the switch on the other side: it IS the switch, defaults to 1 where accepted, and `G_BotsAllowed()` reads it, which is what R-SEC-2's staged exposure wants. **R-BOT-29 measured**: seventeen `TOURNEY` blocks, and they are six different things — four cvar names, four state reads, one fourth client state, two arithmetic changes, three genuine behaviour differences kept as the ruleset's, and three already dead in the donor, including a `TECH*_INDEX` block that has never compiled anywhere. **`GetGameAPIEx` is implemented** for R-BOT-8's paths and R-BOT-26's pak-visible `bots/*.cfg`, and the shim is `src/g_fs.c` rather than four functions in `g_main.c` because `allocpairs.py` refused the free of an engine-allocated buffer in a file that uses both allocator families — the check is right about the shape and the answer is to move the code, not the check (R-95). **R-VER-3 is mechanised as `tools/botmatrix.sh`** and reports **8 rows, bots spawning 1 and 16 in all four rulesets that accept them**, and it does not pass: `sv removebot` segfaults **inside the brain**, in two `(int)(intptr_t)` pointer truncations on `BotShutdownClient`'s two paths (`gladiator-bot-restored/botlib/be_ai2_main.c:390` and `be_ai_weap.c:242`). Measured with a throwaway copy that removes exactly those two casts: the whole cycle completes, every slot returns, `botlibdump` says *no libraries found*, exit 0. **Left open (R-98)** — the lines are in another repository with its own byte-matching contract, and the matrix tells that finding apart from a leak on purpose. **And the ABI had a second half, which R-97 was only the loudest part of (R-100).** Every struct in `botlib.h` crosses the boundary, so both sides must agree on its size and every offset in it, and two did not — each because a type NAME resolves differently on the two sides. `bsp_trace_t` is **84** bytes there and was **80** here, because `osp-tourney`'s port swept `qboolean` to `bool` and C99's `bool` is one byte, so every field from `fraction` down sat four bytes low and **the brain read our `endpos[0]` as the trace fraction on every trace it took**. And `bot_updateclient_t` is **1228** there and was **1292** here, because the bound was spelled `MAX_STATS` and Q2PRO's is 64 under `USE_NEW_GAME_API` where the brain's is 32 — the brain's `memcpy` is sized from its own type, so the inventory it read was our `stats[32..63]` followed by our inventory shifted sixteen slots down. The bots moved and fought through both, which is the part worth remembering. `BOTLIB_MAX_STATS`/`BOTLIB_MAX_ITEMS` are the contract's own bounds now, seven `_Static_assert`s pin the sizes, and `botabi.py` does not take those numbers on trust: it **compiles a probe against the brain's own headers** and compares what the brain's compiler says. Nine more donor defects and three stray declarations in R-99. Changed: R-BOT-1, R-BOT-5, R-BOT-10, R-BOT-12, R-BOT-24, R-BOT-27, R-BOT-29, R-BOT-30, R-MODE-4, R-ENG-3, R-ENG-4, R-ENG-6, R-VER-3, R-VER-7, R-BOT-16, new R-VER-28, §5.7, §9 Phase 6. `doc/reconciliation.md` R-93..R-100. **Then upstream answered all four (R-101).** `gladiator-bot-restored 57ce85a3` fixes R-98 — and finds **eight more** truncations of the same class, three on the same `BotShutdownClient` path, including `ScriptError`/`ScriptWarning` declared `int script`, which made every parser diagnostic a crash instead of a message across 29 call sites. It confirms R-100 as the host's, measured against the 1999 image, and hard-asserts `sizeof(qboolean) == 4`, `MAX_STATS == 32`, `sizeof(bsp_trace_t) == 84` and `sizeof(bot_updateclient_t) == 0x4CC` so no port can redefine either silently. And it **withdraws R-97's branch** rather than keeping it: by value on every target, because the published header was never branched and a foreign engine built against it disagreed with their aarch64 botlib. `botabi.py` reported that divergence on the first run after the update, before anything was built — which is the whole argument for R-VER-28. So the `#if` leaves this tree too, `doc/botlib-contract.md` records **contract version 3**, and R-VER-3 passes against the real brain. **Not done: R-VER-5, R-VER-6 and R-VER-7; R-BOT-3's optional static-link build; and the author's own play test, which nothing has replaced.** |
| **Amendment 1.23** | **Phase 7: bots reach the other three rulesets, and getting them there cost six defects.** `botmatrix.sh` reported bots spawning under `ctf`, `arena` and `tourney` since 1.22 and they did -- and then stood in the audience for the whole level, because **each ruleset's join is a key press and a bot presses nothing**. Under `ctf` the bot layer had been writing `botctfteam` into the `ctfteam` userinfo key since Phase 6 and **nothing read it**: four bots on `q2ctf1` fired zero shots in 337 frames, and the same four traded frags with `dmflags 131072`. The Gladiator donor's reader is `#ifdef BOT` at the top of `CTFStartClient`, and it is carried with two decisions the donor cannot make because its CTF is Threewave 1.09 and has no match system -- the arm goes **ahead of** `ctfgame.match`, and a bot that joins during `MATCH_SETUP` is marked ready, because it cannot type `ready` and one bot would otherwise wedge a `competition` countdown for ever (R-103). Under `arena` the `arena` userinfo key was dead the same way, and `RA_BotJoinArena` makes the two menu clicks a human makes -- pick a team, pick an arena -- landing the bot in the *waiting* queue as a noclip observer, which is R-ARENA-3 and R-RA-4's fifteenth row (R-104). Under `tourney` **the donor has no answer to carry**: a client connects as an observer and enters by pressing a key, the brain's whole command vocabulary is `say`/`use`/`drop`/`wave`/`EA_Command`, and neither `bl_*.c` nor `bots.cfg` issues a join -- so bots under `osp-tourney` sit out too, and R-OSP-11's "bots ready themselves up, join teams and are counted" is a contract the port states and does not implement. `OSP_botJoin` is one line, `OSP_startObserve`, because that is the donor's own toggle and already handles all four modes; picking the side by hand first put all four bots on "Visitors", since `OSP_teamCount` counts only clients that have entered and every count is 0 before the join (R-105). **Six defects underneath, and four of them predate Phase 7.** RA2's `InitClientResp` ends with ten assignments after its own memset and the Phase 4 merge kept none: eight are what the memset already gives, and two are not -- `teamnum` **must be -1**, because 0 is a valid team index and `PutClientInServer` branches on `>= 0`, so **every** client connecting under `arena` took `reinit_player()` instead of `init_player()` and **R-RA-4's first row, "arena menu on connect", has been structurally false since Phase 4**; and `ra_votes` must be `votetries_setting` or `menuVote` refuses every vote (R-104). Tourney's `PutClientInServer` needs the **partial** `InitClientPersistant`, which is what `full` exists for: with the full wipe, `ClientUserinfoChanged`'s rename refusal reads an empty `pers.netname` and installs it, so every bot announced itself as `" entered the game"` with a blank scoreboard row -- a human's first spawn takes the same path (R-106). **R-58's `entered`/`osp_entered` split missed twelve sites**, and no check in this tree can see it: `resp.entered` is a `bool`, `= ENTERED_OBSERVER` stores `true`, and `== ENTERED_ENTERED` then compares `true` against 1 and is **true**, so an observer read as entered everywhere those twelve asked -- including the bot layer's own "is this client playing" accessor. `OSP_gameInit()` ran **before** `game.maxclients` was set, R-47's ordering a third time, so its `team_maxplayers` clamp read 0 and `OSP_addTeamMember` refused every join with "both teams are full" -- which for a bot is `BotDestroy`, so a `match_mode 2` server destroyed every bot that tried to join. And `remove_from_team` had **no caller anywhere in the tree**: RA2's `ClientDisconnect` unlinks a leaver from its arena team and the merge dropped the call, so `sv removebot all` left sixteen dead members linked into two pickup teams and `UpdateStatusBars` dereferenced the first on the next frame (R-107). **Then a crash that is not Phase 7's at all.** `tools/smoke.sh` has printed `Segmentation fault` in the middle of its output and `smoke test passed` at the end of it for as long as it has existed: `ReadGame` opens with `gi.FreeTags(TAG_GAME)` and re-establishes the spine's two arrays, and the bot layer is a **third** TAG_GAME owner that Phase 6 added -- so every savegame `load` precached into freed memory, and fixing that uncovered a second crash at `ShutdownGame` walking a freed menu tree. `BotForgetGameMemory()`/`BotSetup()` are the pair. **The finding is the check**: three scripts read the console and none read the exit status, so a server that printed everything it was asked for and then died on the way out passed all three. `smoke.sh` and `bootmatrix.sh` now read it, each with a control that sends `SIGSEGV` from outside -- q2pro has no deliberate-abort command in this build and a bad map name exits 0 (R-108). **R-RA-5 and R-MODE-7's bot row are decided against RA2's queue rather than Gladiator's fields.** `ra_winner`/`ra_time` are `g_arena.c`'s, which R-ARENA-1 does not carry, and RA2's waiting queue holds both facts already -- the order IS "longest waiting" and `add_to_front_queue` IS "winners kept". So `ra_playercycle` becomes the switch over behaviour RA2 already had, and `ra_botcycle` is the half RA2 cannot have: take the first waiting team with a person on it, first side only, which is `RA2_GetLongestWaitingHuman`'s shape. `CheckMinimumPlayers` **stops returning early under `arena`** -- the fact its early-out rested on has changed -- and what keeps it from running away is a new arena arm in `BotCountsAsPlayer`, because under arena `G_IsObserver()` is true of the whole waiting queue and "playing" means being on a team (R-109). **R-VER-5 is two servers and R-VER-6 cannot reach its own second half.** A bot relocates when the human's slot is a bot's *and* one is free, which only happens after a high-slot bot leaves; with every slot taken R-BOT-15 **refuses** the human rather than stealing, and both rows now run. R-VER-6's tables are sized from `game.csr`, so the overflow arm cannot fire on a real server -- that is R-BOT-11 working -- and `sv indexprobe` asks `BotIndexRecord` directly so the reporting can be seen answering both ways; "near the old 256-model limit" is not reachable with the content here, and the headroom (171/256 on `command` under ctf with both layers) is printed rather than hidden (R-110). **And the author's play test is mechanised rather than deferred a fourth time**, which was the author's decision: `tools/playtest.sh` is **171 checks, up from 139**, and 32 of them are a `libq2` client connected to a server that already has bots in it -- a `playerskins` entry per bot, the placement census, and obituaries arriving as broadcast prints. **R-CTF-7's v0.95 half is discharged by the first of those**, read off the wire instead of out of the source, after being recorded as "structural and reviewed" since 1.12. R-OSP-4's SIGFPE row is there too and could not be checked any other way: only a client can start the vote that `OSP_votePercent` then divides by an empty room every frame (R-111). All of it is asserted against **two new lines in `sv ruleset`** -- `bots` and `botplace` -- for the same reason R-VER-19 exists (R-112). Changed: R-CTF-4, R-CTF-7, R-RA-4, R-RA-5, R-ARENA-2, R-MODE-7, R-MENU-4, R-OSP-4, R-OSP-11, R-BOT-17, R-VER-2, R-VER-3, R-VER-5, R-VER-6, R-VER-7, R-VER-17, R-VER-27, new R-VER-29, §9 Phase 7. `doc/reconciliation.md` R-103..R-112. **Not done: R-BOT-3's optional static-link build, and no human has yet watched a bot.** |
| **Amendment 1.24** | **Phase 8: the security posture becomes three checks, and the extras all land.** R-SEC-1 asks for every unbounded copy "on a path reachable from userinfo, a client command, a chat string, an address string or a config file" -- and nothing computes reachability, so the rule enforced is the stronger decidable one: `strcpy`, `strcat`, `sprintf`, `strncpy` and `strncat` do not appear in `src/` at all. **189 sites converted**, `tools/bounded.py` (R-VER-30) keeps them out, and three of the 189 were live: four `arena.cfg` overflows including an `add_val()` that appended to a 0x400 block with no bound at all; `setteamskin()` appending a forced team skin to the ENGINE's userinfo buffer after `Info_RemoveKey` "made room"; and RA2's menu value blocks, allocated from the INITIAL value's length and then overwritten in place by four writers, one of them with a map name out of `arena.cfg`. **And one the ban cannot see, which is worth as much as the ban**: `menu_centerprint` copies into `buf[2048]` with a hand-rolled loop and no bound, reachable from `G_UseTargets` with an entity's own `message` key, so a map string was a stack overflow under `arena`. It was found by reading R-SEC-2's ledger, which names the *wrap bookkeeping* in that function and not the overflow underneath it -- a ban on named functions is a floor, and the ledger is what looks under it. **R-SEC-7 took a deletion.** `src/arena/gslog.c` resolved a hostname, opened a UDP socket and sent a datagram per kill, connect and disconnect to whatever `netlog` named; three of its helpers called `exit(1)` on failure from inside a game library, so a DNS hiccup on the log host was a dead server. Gone, with `net_compat.h` and `-lws2_32`; the local `logfile 2` log is untouched and `netlog` stays registered so a 1999 config parses. `tools/noexec.py` (R-VER-32) is the check and bans `exit`/`abort` for the same reason. **R-SEC-8, both clauses.** `ra2menus.c` read `(int *)&arenas[n].proposetime + 1` -- a float's address, stepped one int forward -- which names the member now; the two layout assumptions underneath it are **pinned rather than rewritten**, 44 `_Static_assert`s over `arena_settings_t`'s 42 indices and `arena_t`'s inline copy, so the retype that shrank that struct from 168 bytes to 96 in RA2's own port would now fail the build. Forty cross-file `extern`s moved into headers -- four of them naming functions that exist nowhere -- and **moving `Move_Calc` made gcc reject Ground Zero's own prototype**, one `const` out since Phase 2. `tools/externs.py` (R-VER-31) enforces both halves, and the second is the interesting one: `extern` is a spelling, not a mechanism, so a bare prototype declares another file's symbol just as well -- the tool checks 250-odd of those against their definitions rather than demanding they move. **R-SEC-4** found two out-of-bounds indices: `UpdateStatusBars` guarding a team index against the COUNT rather than the last valid index, and a bot's `arena` userinfo key going straight into `arenas[32]`. **R-EXTRA-1..7 all land.** `g_log.c` and `g_gamelog`; `p_lag.c` and `g_clientlag`, off by default because `lag` is a CLIENT command and a player who can ask the server to hold two seconds of their input is asking it for memory; `trigger_counting`, `trigger_log` and `func_button_rotating`, off by default and freed rather than left inert; and **VWep measured rather than implemented** -- it is id's from 3.20 and not Gladiator's patch, 19 itemlist rows carry a `weapmodel`, and a cvar would be a switch to turn off something standard. **R-EXTRA-6 is 2,386 reconstructed lines**, and importing it found the question its own table does not answer: under `ctf` there are two things called `observer` and they are not the same thing. Threewave's drops the flag, the tech and the score; the Gladiator one toggles a camera. **CTF keeps the verb and R-EXTRA-6 supplies the cameras**, `G_IsObserver()` gains `FL_OBSERVER` as a fourth spelling so thirteen existing sites see the new observer unedited, and three defects came with it -- a `pers.weapon` dereferenced unconditionally on the way out, `CTFTeam_f`'s spectator arm never clearing the flag, and a `roll_diff` the disassembly also computes and never reads. **R-EXTRA-7 is not isolable here** and says so: all three Gladiator `g_func.c` copies in this workspace are byte-identical at 3,260 lines, so there is no before-and-after; the one adjacent divergence -- yquake2's "hack for entity without it's origin near the model" in `plat_blocked`, which neither id nor q2pro has -- is named rather than taken. **R-BOT-23 measured**: `BotRunFrame` timed on CLOCK_MONOTONIC, `sv botperf`, and 32 bots on q2dm1 over 300 steady-state frames cost **mean 2613 us, worst 10785 us against a 50000 us budget**. Getting there taught the harness two things: `sv addrandom` will not seat a character already in the game, so the eighteen-name roster is its ceiling; and this file's `&&`/`||` verdict idiom re-fires every later `||` once the verdict is bad, so the LAST message wins -- it reported a budget failure on a row whose worst frame was 4517 us. **R-VER-20 gains a second temporal check and this one is mechanised**: bots pick items up on their own, so `sv census <prefix>` before, during and 50 seconds after `sv removebot all` asserts that 11 weapons went out, 3 were waiting, and all 11 came back -- the removal being what makes "it came back" decidable at all. **D2-D9 complete**, D6 shipping as `colosseum/` -- five per-ruleset configs, `default.cfg`, the 1999 roster and RA2's own `arena.cfg`, each exec'd against a real server by `tools/extras.sh`. **R-SEC-2's two RA2 ledgers are entered item by item in this tree**: of seventeen deliberately-kept bugs three were live and are fixed, three are absent with the GameSpy layer, one is id's own and stays under §7 rule 1, and the NEXTROUND sweep does not reproduce because the donor's own `tnode` reset came across with the merge; all sixteen post-port fixes verified here rather than inferred from the pin. **R-COMPAT-6 reaches zero**: `basedir`, `gamedir` and `rcon_password` were registered from eight places with three different defaults, one of them NULL, and three writers built paths from the gamedir alone -- which resolves against the server's working directory and is why the new game log's first run said `Error opening log file colosseum/boot.log`. **R-BOT-3's optional static-link build is struck** at the author's direction rather than carried a fourth phase. **And `tools/watch.sh`**, which is the one thing here that asserts nothing: a server with bots, q2pro's own client on the operator's display, and three drive scripts that each open with what to look for. Phase 7 recorded "no person has watched a bot play"; this is the command, and it has not been run yet. Changed: R-BOT-3 struck, R-SEC-1, R-SEC-7, R-SEC-8, R-EXTRA-5, R-EXTRA-7, new R-VER-30/31/32/33, §9 Phase 8, §11 risk 18. New: `doc/exposure.md`, `colosseum/`. `doc/reconciliation.md` R-113..R-119. |
| **Amendment 1.25** | **The first time a person looked at it, D6's `default.cfg` turned out to be id's.** `tools/watch.sh` ran on a real display for the first time and the player could not move or shoot: **zero key bindings**. `default.cfg` is not a name a gamedir may use -- it ships inside `pak0.pak` and holds all 68 bindings a Quake II client starts with. q2pro satisfies `exec default.cfg` from a pak as readily as from a loose file and within one gamedir the pak wins, which is why this was invisible in every flat test tree; but the install layout this project documents (R-BUILD-8, and README.md says it twice) puts the library and its configs under `homedir` and the paks under `basedir`, homedir is searched first and has no pak, so our file answered and id's never did. **It cost nothing on a dedicated server**, which is why 41 checks, 187 client assertions and a headless run of `watch.sh` itself all passed with it: a server has no bindings to lose, and the only client in the harness is `libq2`, which has no keyboard. D6's file is `server.cfg` now. Two checks were added and the second is the one that matters: `extras.sh` counts the bindings a client gets **starting from nothing, in the documented homedir layout**, because the flat layout cannot see this and neither can a second run in the same tree -- a client writes `config.cfg` on quit and that carries the bindings forward whatever `default.cfg` did. Both facts were learned by writing the control and watching it fail to bite, twice: once for the layout and once for `grep -c` printing 0 *and* exiting 1, so a `|| echo 0` fallback made the count "0\n0" and every numeric test downstream failed on the wrong thing. **And separately, id's layout is not WASD**: `a` is `+lookup` and `s` is `use silencer`, because WASD was not a convention in 1997. `tools/drive/watch-keys.cfg` is the watcher's keyboard -- WASD, mouse aim and fire, and the observer verbs on one key each -- exec'd on the client command line so it lands after the config bootstrap rather than under it. It is not part of the shipped gamedir set, because bindings are a player's and D6 has no business writing anybody's config. **A third finding came out of the same session and is not a defect at all.** The author reported ESCAPE not opening the engine menu; ESCAPE **cannot be bound** -- q2pro intercepts `K_ESCAPE` before it looks a binding up and the source says so, *"menu key is hardcoded, so the user can never unbind it"*. It did nothing because **q2pro's menus are a data file rather than code**: `UI_Init` parses `q2pro.menu`, meson installs it into the base game directory, and `watch.sh` never installed it, so `UI_FindMenu("main")` returned nothing. `pushmenu main` answering *"No such menu: main"* is how it was found. And there is a second reason it can look dead, which is ours: with a LAYOUT up -- any Colosseum menu, or a scoreboard -- q2pro sends `putaway` on the first Escape and opens the menu only on the second or on a held key, and under `arena` a fresh client has the join menu up immediately (R-RA-4). `watch.sh` installs the menu file now and `watch-keys.cfg` binds F11 to `pushmenu main` as an unconditional way in, plus an inverted vertical axis (`m_pitch -0.022`; q2pro has no `m_invert`) and a 1280x720 window (`vid_fullscreen 0` explicitly, because it is CVAR_ARCHIVE and a stale `config.cfg` can carry a 1 forward). Changed: D6, R-VER-33's check list, §9 Phase 8. `doc/reconciliation.md` R-120. |
| **Amendment 1.26** | **Three findings from watching it, and one from running the Windows build.** **The Windows DLL would not load.** `-fstack-protector-strong` makes gcc add an implicit `-lssp` and on mingw the link prefers `libssp.dll.a` over `libssp.a`, so both PE artifacts declared **`libssp-0.dll`** as an import and needed a mingw runtime beside them. Dropping the hardening is what R-SEC-6 forbids, so the fix is the static archive that was in the toolchain all along: `-Wl,-Bstatic -lssp -Wl,-Bdynamic`. The protector is unchanged, the dependency is gone, and the one import it adds -- `ADVAPI32.dll`, because `__stack_chk_fail` reports through the event log -- ships with Windows. **New R-SEC-6a and `tools/pedeps.sh`**, run at LINK time rather than on request, because this failure is invisible from the host that produces it: the DLL links, ten configurations pass, and nothing in this workspace can load a PE image to find out it wanted a fourth DLL. **No player models.** `players/` is a LOOSE DIRECTORY and not a pak entry -- retail `pak0.pak` has 3,307 entries and not one under `players/`, only the player sounds -- and every harness here links `pak*.pak` and stops. A watcher saw players with no bodies and a scoreboard with no faces. Nothing could notice: four scripts run a dedicated server, which never loads a model, and `libq2` has no renderer. `watch.sh` links them into `baseq2/` where a real install keeps them, verified with q2pro's `whereis`. One skin the 1999 roster names, `cyborg/disguise`, exists in neither retail nor the Gladiator assets and falls back to `male/grunt`. **And "the controls are dead" was a camera.** `DoObserver` zeroes the movement axes in a camera mode and drives the view itself, because a camera following somebody else cannot also be steered; a plain observer is left alone on `PM_SPECTATOR` and flies. Correct, and unchecked -- so `playtest.sh` asserts both halves now. The SCENE was wrong: `watch-bots.cfg` drove four camera commands on a timer and left the watcher with no control for ninety seconds of a two-minute run, and the cameras are on keys now. **And a fifth, chased and honestly not found.** The author reported a second or two of dead input after leaving observer mode. Chasing it found five teleport holds written in the 1999 spelling -- a bare `14` or `160 >> 3` where `PM_TIME_SHIFT` belongs, which is 3 on a plain server and **0** on an extended one, so those holds last an eighth as long as they say. That is R-SEC-2's sixteen-fix list repeating itself: RA2's port had exactly this defect in `arena.c` and fixed it, and the same spelling survived in CTF, in Ground Zero and in the observer this phase imported. `units.py` gains a fourth check for it and found a false positive of its own on the first run. **It is not the cause** -- correcting it makes the hold longer, and 112 ms is not a second -- and the 1-2 s is left undiagnosed rather than explained away: the remaining candidates are all client-side and none is in this library. Changed: R-SEC-6 clause 3, new R-SEC-6a, R-VER-21's check list, §9 Phase 8. `doc/reconciliation.md` R-121..R-124. |
| **Amendment 1.27** | **Two findings from the first stage-0 evening, and neither one is ours.** **The console pauses the server** on a `deathmatch 1` game, which no other mod the author has played does. It is q2pro's, in three lines that never read `deathmatch` or `maxclients`: `CL_CheckForPause` sets `cl_paused` whenever the console or a menu is up and `cl_autopause` is 1, and `check_paused` freezes the server when that is set and **`sv_clientlist` holds exactly one client**. One connected client is the engine's proxy for single player, and it is the wrong proxy for every Quake II bot mod -- Gladiator bots are fake clients made by calling `ClientConnect` on a spare edict, so thirty-two of them still leave that list singular, and q2pro's own `SVF_BOT` is a flag its engine reads nowhere. The expectation came from vanilla Quake II, which pauses from the MENU only and gates it on `maxclients == 1`; under q2pro every mod does this, baseq2 included. **This library cannot fix it either way** -- `cl_paused`/`sv_paused` are `CVAR_ROM`, and the frame that would clear them is the frame `check_paused` skips -- so what ships is `cl_autopause 0` in `tools/drive/watch-keys.cfg` with the mechanism written beside it -- inert against `watch.sh`'s own `dedicated 1` server and there for the listen server a stage-0 operator actually plays on -- a stage-0 row in `doc/exposure.md`, and deliberately no check: the cvar is the client's, `libq2` has no console to open and `watch.sh` asserts nothing on purpose (R-125). **And no bots would load, because there is no auto-bspc to fail.** `no AAS file available` is `BLERR_NOAASFILE`, and brain-present-with-the-`.aas`-taken-away is **`tools/botmatrix.sh`'s own second positive control** -- a row required to fail, reproduced by hand. Three independent reasons nothing launched BSPC, any one of them sufficient: the libvar is pushed only when the cvar is set and R-SEC-7 keeps it unset, which the log proves by *not* printing `creating AAS for q2dm1...`; the branch is `#ifdef _WIN32` and spawns a `winbspc.exe` that has to be in the gamedir; and `SpawnProcess` in the reconstruction is a stub returning -1. **So R-SEC-7's one exception is currently unreachable**, which is a fact about the posture rather than a hope. In 1999 it would not have rescued the session anyway -- that branch prints *"You probably want to close Quake2 now"* and starts a compile. **The defect is documentation.** R-BUILD-8 says where the library goes and nothing said where the brain's assets go: the brain, its `pak7.pak` and **one `.aas` per map** all live under **`basedir`**, the mirror image of the library's homedir rule, because the botlib does its own file I/O from its own libvars and never touches the engine's search path. Sixteen meshes ship -- `q2dm1`-`q2dm8`, `q2ctf1`-`q2ctf8` -- and a seventeenth is two steps of which only the first is a tool: `bspc` for the geometry, then one load of the map for the brain to compute reachability and write the finished file. `README.md` gains the mirror-image section, `doc/exposure.md` a four-row stage-0 table, and `tools/watch.sh` now says when the map it is about to run has no mesh -- it linked a glob and never checked the map (R-126). **A sibling of the same class, found while fixing it**: `doc/cvars.md` still said the library pushes `fastchat "0"` and that this "is not a typo", four amendments after R-102 changed it to `"1"` and recorded why the donor was wrong. Nothing in this tree reads its own prose -- twenty-three audits, not one about a document -- so that class is open and cannot be closed from here. **And the hand-off note is deleted (R-127).** `doc/next-prompt.md` was read by nothing in the tree -- no tool, no `Makefile` rule, no requirement, no other document -- and was a third copy of git plus this file; three of the five figures in its evidence block were wrong within the hour of being checked: the play test is **193/0** and not 187, `tools/extras.sh` is **41/0** and not 39, and its controls are five and not four. The play-test count is the sharp case, because that scenario lives in the out-of-tree `q2-playtest` skill and can move with **no commit here that could carry the correction**. Two of its lines were the only copies of anything and both moved: the map from a changed file to the check that answers for it is section 10's preamble now, and the residual on `botmatrix.sh`'s `&&`/`||` verdict idiom is on R-117. **And four rows that answered "what is still open" wrongly (R-128).** win32 PE was recorded **built, not run** and risk 19 still said "nothing x86 is executed here" -- the author ran it, and the correction keeps the part that matters: that session had no `.aas`, so the bot ABI (R-BOT-4, R-BOT-5) has still never crossed on x86, which makes a bot on that machine the highest-information thing left to run. R-MENU-2a was still waiting for a second menu engine that arrived in Phase 4; all four ship, the contention is live wherever a bot is, and the row's check is now a contention test rather than a wait. §10's harness `TODO.md` item 7 -- `SAVE_VERSION` drift, "a live savegame-compatibility risk" -- was checkable in one command and is **byte-identical to q2pro's**, guard and magics included. And §9 Phase 8's evidence is **re-measured with a date** rather than overwritten: 193/0 play test, 41/0 extras with 5 controls, 14/14 bot matrix, 20/20 boot matrix, ten configurations with zero warnings, R-BOT-23 mean 2370 us worst 16292 us. Changed: R-SEC-7, R-BUILD-8, R-BUILD-5, R-VER-8, R-MENU-2a, section 10 preamble (new), section 10 item 7, section 11 risk 19, `doc/next-prompt.md` deleted, §9 Phase 8, `doc/cvars.md` (two rows), `tools/watch.sh`, `tools/drive/watch-keys.cfg`, `doc/exposure.md`. **And D6's `arena.cfg` is RA2's own file (R-129).** A play test found the arena join menu offering only *Start New Team* -- no pickup teams. Not a code defect: `menuRefreshTeamList` is byte-identical to the donor's, pickup teams are *teams* in that same list (`#N Pickup Red`/`#N Pickup Blue`), and a live client reading the menu off the wire finds them on `q2dm1`, because a map with no `arena` worldspawn key is an "idmap" where `pickup` defaults to 1. On **RA2's own maps** it defaults to 0 and only the arenas a config marks `pickup: 1` become pickup arenas -- and the 69-line example D6 shipped from 1.24 marks none, where RA2's own file marks **35** across **28 per-map blocks**. Measured all three ways: ours on `ra2map1` gives 4 rows, RA2's gives 6 with `#7 Pickup Red`/`#7 Pickup Blue`, ours on `q2dm1` gives 6. So D6 now ships **RA2's own 934-line file byte-identical** (CRLF, `.gitattributes`, the `bots.cfg` precedent), which also makes 1.24's claim that D6 ships "RA2's own `arena.cfg`" true for the first time. Two consequences recorded rather than papered over: its `maploop` names RA2's 28 maps, and a **donor data file** is a new class in `doc/provenance.md` -- it carries no copyright header and RA2's public tree has no licence text, so the row says what is known instead of naming one. **And the check that could not see this now can**: `extras.sh`'s D6 row asserted only that the file parses, so it gains a sibling that counts per-map blocks and `pickup:` keys, with the stripped example as its control -- 42 checks and 6 controls. `doc/reconciliation.md` R-117 amended, R-125..R-129. |
| **Amendment 1.28** | **A play test in an arena found two defects and an asset that does not exist.** **"Leave Team" left the player with no menu at all** -- in arena 0, spectating, unable to rejoin or spawn, with `inven` doing nothing. `UseMenu` reads `curmenulink` **before** it calls the row's callback and unlinks that menu afterwards; `init_player()` is the tail of both Leave Team rows and it *drops* the queue rather than walking it, because a reused client slot must not inherit the previous occupant's TAG_LEVEL menus. So the captured menu is orphaned with its `prev` still pointing at the queue HEAD, and `remove_from_queue` then writes `menuqueue.next = NULL` -- **taking the brand-new team menu the callback had just built**. `curmenulink` ends up NULL, which is the exact field the `inven` reopen tests. The donor cannot have this defect: its `init_player` does not touch the queue, so the old menu is still queued and the unlink is correct. Ours added those three lines in 1.19 and this is their cost. `UseMenu` now asks whether the captured menu is still in the queue and, when it is not, frees it without touching a queue it is no longer in (R-130). **And the bots were in the wrong arena, which is 1999's default doing exactly what it says.** The `arena` cvar picks the arena a new bot joins, default **1**, clamped to `1..num_arenas` -- `gladq2_src/bl_spawn.c`, copied faithfully here. On a deathmatch map that is the only arena and it is right; on `ra2map6` the human is in arena **8**, because that is where RA2's own `arena.cfg` puts the pickup arena, and four bots sat in arena 1 building teams of their own. **0 now means "follow the people"** -- the lowest-numbered arena with a human on a team, then the lowest-numbered pickup arena, then 1 -- resolved at join time rather than when the bot was added, and 1..N still behaves exactly as 1999 did. D6's `configs/arena.cfg` asked for **`botarena 0`**, a name nothing in this tree or any donor has ever read, with a comment describing behaviour that never existed; it is `set arena 0` now (R-131). **The asset is the finding underneath both.** Bots cannot play *any* RA2 map: the brain needs an `.aas` per map, the 16 that ship are `q2dm1`-`q2dm8` and `q2ctf1`-`q2ctf8`, and every attempt on `ra2map6` printed `Fatal: no AAS file available` and destroyed the bot. Measured end to end by making one: `bspc -bsp2aas` then a single load of the map for the brain to compute reachability and write it back, **about five minutes** for `ra2map6` on this host, after which four bots join `#8 Pickup Red`/`#8 Pickup Blue`, the round starts in arena 8 and the person spawns into it. **All 28 were then generated** at the author's request -- 17 to 127 seconds each, three workers, ten minutes for the set, 33 MB -- and each one verified to load and seat a bot, with bots fighting on three of them. `ra2map10` and `ra2map28` need `bspc -nocsg`: their hulls leak, so the flood fill cannot run and `bspc` exits **0 without writing a file**. The meshes stay out of this repository for R-126's reason -- they are the brain's assets. Changed: R-MENU-3, R-ARENA-2, D6. `doc/reconciliation.md` R-130..R-131. |
| **Amendment 1.29** | **The old game API is a build target, and building for it found a defect that had never run.** `make API=old` builds the same sources against `GAME_API_VERSION_OLD` (3) -- `gclient_old_t`, `pmove_old_t`, 32 stat slots -- for a 1997-vintage engine or a Q2PRO built without `USE_NEW_GAME_API`; Q2PRO's loader accepts either (`q2pro/src/server/game.c:1014`), and **new stays the default and the shipped configuration**. **The compile was the easy half**: every linked translation unit already built clean at both settings, because `g_save.c`, `p_client.c` and `g_local.h` carry Q2PRO's own `#if USE_NEW_GAME_API` arms and R-BOT-1's contract was already decoupled from the engine's `MAX_STATS`. Three things did change: `config.h`'s switch is `#ifndef`-guarded so a `-D` can win without the redefinition R-BUILD-2 fails on; the build directories diverge as `<dir>-oldapi`, because the switch changes **struct layouts** and mixed objects link cleanly and run wrong while `-MMD -MP` cannot see it -- R-48's failure with the one mechanism that stops it removed; and **R-OSP-7 clause 6's ceiling was one bound and needed two.** It asked only whether the client had negotiated protocol extensions, which was complete while the API was fixed on; the two switches are independent in *both* directions, so `game.csr.extended` can be true on a library whose `player_state_t` holds 32 slots, and CTF's timer pair at 32/33 would then be written **off the end of the struct, per client, per frame**, for every client holding a second powerup. `stat_ceiling()` is now the lower of what the array holds and what the wire carries. Observed rather than reasoned: the `API=old` release on the same `q2proded` that loads the 3302 library prints `ruleset ctf (extensions on, api 3, so slots 0..31 are reachable)` with both rows dropped, and the full battery is **identical across the two ABIs on every ctf row but that one**. **AND THEN THE INTERESTING PART (R-132).** `-Warray-bounds` at `API=old -O2` refused one line, and under it was **R-OSP-7's own abstraction being paid for four phases late**. Replacing the donor's `#define STAT_RUNE_RESIST 22` with a `statslot_t` changed what the token MEANS -- a logical id, ordinal 28 -- and the port renamed every consumer while migrating **one of the four uses**. The rune item's `quantity` stayed 22, so `G_SetStat(other, item->quantity, 1)` set **an arena stat tourney does not map** and picking up a rune granted nothing; fourteen reads indexed `stats[]` by ordinal, landing on 28..32 where the runes sat at 22..26 -- two of them tourney's *own* second-powerup-timer pair, so **holding a pent read as holding the STRENGTH and HASTE runes**, doubled damage and haste fire rate, while resist, regen and vampire could never fire; and `r_count[quantity - SID_OSP_RUNE_RESIST]` was `r_count[-6]`, which is the counter `OSP_checkMinRunes` terminates on -- so it never rose, the spawner and the checker tail-called each other, and **`g_ruleset tourney` with `runes 1` died at map load, every time**, with `ED_Alloc: no free edicts`. A shipped configuration was an instant server death and the whole rune feature had never run once. **Nothing could see it because everything asked the map and the map was right** -- 22..26, no collisions, right kinds; the compiler sees a well-typed enum-to-int conversion, the boot matrix runs tourney with runes off, and the battery ran `runes 1` under *ctf*. The fix restores the donor's idiom in this tree's vocabulary -- `quantity` holds the identity, and the identity is now a `SID_` -- which makes all three uses right at once, plus `G_GetStat` for the reads and a `_Static_assert` for the contiguity `r_count[]` silently needs. Two new `slotkind.py` questions, and **the first attempt at the second was wrong in an instructive way**: "the id argument must be a `SID_` literal" rejects the *fixed* code as loudly as the broken code, so the real invariant is a join between two files -- if an accessor reads `item->quantity`, every `IT_RUNE` item must spell it as an id. **Seven controls; 19 findings on the pre-fix tree, 0 after.** And a behaviour claim rather than a static one: new `tools/osprunes.sh` (R-VER-27) drives a real TeamPlay match and asserts in **both signs** -- a rune sets its own slot and only its own and `%r` names it; two powerups and no rune leave all five zero and `%r` says "no runes". It reads stat VALUES off the wire with slot numbers taken from `sv slots` by name, which is the only channel that could have caught this: **the diagnostic here was not vague, it was correct**, and still could not report that nothing ever wrote there. 33 rows, clean on both ABIs; on the pre-fix library its first row reports the `ED_Alloc` death. **And a diagnostic with a reader has a format (R-133)**: `sv slots` gained the api version, inserted ahead of the `(extensions <on|off),` anchor R-VER-27's harness matches on, so the battery reported `extensions on` failing on a server that had them on while the next row contradicted it. Appended instead. The harness now reads that field and asserts the **ABI-appropriate** outcome for ctf's timer pair -- resolved on 3302, dropped on 3 -- so both are 86/0 where the old ABI previously reported correct behaviour as broken. Changed: R-ENG-1, new R-ENG-1a, R-OSP-7 clauses 6/7, R-TOOL-1, `Makefile` (new `API`/`oldapi`/`bothapis`), `.gitignore`, `config.h`, `src/g_items.c`, `src/g_stats.{c,h}`, `src/tourney/osp_runes.c`, `src/tourney/osp_players.c`, `tools/slotkind.py`, new `tools/osprunes.sh`, `README.md`, `doc/regression.md`. `doc/reconciliation.md` R-132..R-133. |
| **Amendment 1.30** | **Two play tests, five defects, and only two of them are ours.** **Bots and people ended up in different arenas, and R-131's answer was right but asked once.** `arena 0` means "follow the people" and 1.28 resolved it at the bot's join; `CheckMinimumPlayers` adds its first bot at `level.framenum` **32**, before a person who typed `map` has finished the motd and the team list, so that bot has nobody to follow, takes the fallback arena and stays there for the level while every later bot follows the person. Reproduced exactly with a headless client on `ra2map9`: `Quad Bitch` on `#1 Pickup Red`, the person on `#2 Pickup Blue`, and both remaining bots on `#2 Pickup Red` -- one person against two, with a third bot alone next door. **The balance is not the defect**: three bots joining the smaller of two pickup teams settles at 2v2 on its own, and losing one to the other arena is the whole of "me vs 2". `RA_BotFollowPeople()` re-asks on the same 32-frame cadence, and moves a bot only when its `arena` key is out of `1..N` (so `arena 3` is still 1999's literal request), somebody is on a team somewhere, **nobody** is on a team in the arena it is already in, and the target would accept it -- checked *before* `remove_from_team`, so a refused move cannot strand it in arena 0 (R-134). **Two fighters spawned on one point and neither of RA2's two answers works during a countdown.** A pickup arena gives the two sides alternate spawn indices and picks at random, so `ra2map9` arena 2's twelve points are six a side and three bots on one side collide 44% of the time. The telefrag cannot fire -- an arena fighter is `takedamage DAMAGE_NO` until `ASTATE_FIGHTING`, and `!tr.ent->takedamage` is exactly the condition for taking the push branch **instead of** `T_Damage` -- and the push does not push apart, because `KillBox` adds **the same random vector to both bodies**, so the pair drifts and the distance between them never changes. That is `g_utils.c` character for character from the donor, and it is the sentence in the report. Measured with eleven players: **20 of 25 dumps had a fighter pair inside 50 units, worst case eight pairs, 36 pair-observations with both clients `SOLID_BBOX`** -- two solid bodies at one origin is `allsolid` in `PM_StepSlideMove`, which is a player who can shoot and cannot walk. Fixed at the root, by preferring a point with the 50 units of clearance `SelectFarthestArenaSpawnPoint` already calls usable and falling back to a clash only when the whole side is taken, which is RA2's own stated fallback; and at the mechanism, by giving the second body the opposite vector. **KillBox pushes 11 -> 0** on the same map with the same eleven players. Rewriting the walk found a **third** defect: the donor's index loop dereferences `G_Find`'s result without testing it, and `side == 1` on an arena with ONE spawn point asks for index 1 of one -- a server-killing NULL read reachable from `arena.cfg` alone (R-135). **The team list said "Players: 0" while the scoreboard showed the bots, and both halves are the donors'.** `menuRefreshTeamList` bakes the counts in when the menu is built and RA2's answer is its "Refresh List" row; R-MENU-3 made `inven` a *reopen* rather than a rebuild on purpose, because closing an RA2 menu is hiding it. Correct separately, wrong together. Rebuilding on open is refused -- `FinishMenu` pushes a menu onto the queue and nothing pops the old one, so a rebuild on a key pressed all match long turns a bounded 1999 leak into an unbounded one -- so `RA_RefreshMenuCounts()` updates the numbers **in place** from `MenuThink`, which already repaints every ten frames, making them live rather than fresh-on-open (R-136). **Then CTF, where the brain never knew whose side it was on.** `clientsettings[].skin` is the ONLY currency the Gladiator brain has for teams -- `BotCTFTeam()` is `strstr(skin, "ctf_r") ? RED : BLUE` and `BotSameTeam()` compares the half after the `/` -- and `bl_main.c` fills it from `Info_ValueForKey(pers.userinfo, "skin")`, which Threewave never rewrites: `CTFAssignSkin` writes the `playerskins` configstring and stops, because a client is the only thing Threewave has to convince. So every bot read its own chosen skin, found no `ctf_r`, **believed it was blue** -- wrong flag, wrong base -- and no two bots were ever team-mates. **The donor already fixes it and the merge dropped the fix**: `gladq2_src/g_ctf.c` adds the userinfo write to all three arms under `#ifdef BOT` and **moves the call after the `pers.userinfo` save**, leaving the note that says why; we kept ZOID's position, so even the write would have been overwritten. The libvar half is the other necessary piece and is an omission rather than a line: the donor sets **no** `teamplay` under ctf, because `BotSameTeam` tests it first and a whole-string compare makes `male/ctf_r` and `female/ctf_r` enemies -- the opposite of R-ARENA-2's answer, for the stated reason that under arena the skin is a synthetic team id and under ctf its second half already IS the team (R-137). **The check needed an instrument and saying why is half the finding**: under ctf the world barely answers, because `T_Damage` calls `CheckTeamDamage()` -- which refuses a team-mate's damage before knockback or an obituary -- for everything except `DAMAGE_NO_PROTECTION`, which is why the report says bots *attack* team-mates rather than kill them; the one thing that gets through is a **telefrag**, a spawn collision rather than an act of aim that Threewave expects and docks a frag for, so the obituary row counts those apart and neither build produces a non-telefrag same-team kill; and the `playerskins` configstring is no better, because it was always right and both builds pass 4/4. `sv ruleset`'s ctf `botplace` row gains **`teamskin=`**, the count of bots whose `pers.userinfo` skin carries `ctf_`, which is the string the brain is handed: **0 of 4 before, 4 of 4 after**. **And `bind e "+hook"` bound a key to nothing.** The offhand hook is implemented -- `hookon`/`hookoff` drive R-CTF-3's latch and `ClientThink` fires it -- and unreachable, because `+hook` and `-hook` are **console aliases and no Quake II client ships them**. uGladQ2 stuffs both at the top of `ClientBegin`, with no `cmd` prefix because an unrecognised console command is forwarded to the server, and skips a bot; `tourney` has done the same since Phase 5, which is why the gap was ruleset-shaped rather than visible (R-138). **A new `sv arenadump` (R-VER-34) is what made three of the five legible**, and `spawn_recheck` is its sharpest field: nothing but `KillBox`'s push branch sets it, so a non-zero value IS the record of two clients placed on one point. `scenarios/ctfteams` is new -- the hook aliases read off the wire through a new `playtest.Bot.Stuffs()`, the wire skin, the brain skin, and every obituary checked against the roster -- and `playtest.Bot` gains `Stuffs()`/`WaitStuff()`. Changed: R-ARENA-2, R-MENU-3, R-CTF-3, R-CTF-4, new R-VER-34. **Then the sweep the author asked for before committing, and it found three more of the same shape.** Every `stuffcmd` and every direct `svc_stufftext` in all five donors, checked against this tree: **one more missing client half, and it is the same defect in the other ruleset** -- RA2 stuffs `alias +grap grap_on` and `alias +hook grap_on` at the top of its own `ClientBeginDeathmatch`, `grap_on`/`grap_off` were carried into `ClientCommand` and the aliases were not, so the arena grapple was reachable only by typing the command by hand. All **78** `#ifdef BOT` fences in `gladq2_src` -- 66 of them in the shared files, which is where a merge can lose one -- and two more are missing: **`PrecacheCTFItems()`**, because `weapon_grapple` is always owned and never stands in a map, so `PrecacheItem` is never reached for it and its six sounds, view model, icon and pickup sound were registered on first use, one configstring at a time, mid-round, with no HUD icon until then (the hook model joins the row too -- `CTFGrappleFire` takes its index at the moment it fires).  **That one is inherited rather than ours, and from further back than Threewave**: `q2pro/src/ctf` carries the same dead list today and the row documents the condition two lines above it, *"always owned, never in the world"* -- and 1.17 had already ADDED `grhurt.wav` to that list without noticing nothing walked it, which is the lesson: extending a list is not evidence that the list is read; and **the brain's client table is only ever written**, because `BotLib_UpdateAllClientSettings` skips a slot whose edict is not `inuse` and `ClientDisconnect` clears `inuse`, so a client that leaves stays in it with its name and its skin, and `BotNumTeamMates` counts by `strlen(netname)`. Five further fences are differences rather than defects and are left alone with the reason stated, one of them because it is **unreachable here**: `g_phys.c`'s `case MOVETYPE_WALK` cannot fire when all four assignments of that movetype in this tree are on clients and `G_RunFrame` never runs a client through `G_RunEntity`. **And the RA2 pin is two commits stale.** `ddba883` and `6b8d058` are both bug fixes; of their five halves this tree already had two -- the `menu_centerprint` NULL deref (1.17, by reading) and the observer ranging (R-77, 1.18) -- and was missing three: the grapple precache, and the two remaining halves of "farthest from any player is a minimum, and which players it is taken over is three separate decisions". Those two compound R-135, so they are carried: the arena selectors ask their own question now (`ArenaFightersRangeFromSpot` -- fighters, in this arena, excluding the one being placed) and `PlayersRangeFromSpot` keeps R-77's arm because `PutClientInServer` still reaches the shared deathmatch selectors under `arena`. The pin **stays** at `d20e1ce`: `arena.c` has diverged here by R-131, R-134, R-135 and R-77's own different answer, so a merge would have to choose between two fixes for one defect rather than take one. **And the question that started the sweep is answered against upstream rather than by reasoning: upstream did not fix the push.** `rocketarena2` `main` -- 1999's v2.25 -- and `q2pro-enhancements` HEAD both still read `VectorAdd(tr.ent->velocity, forward, ...)` and `VectorAdd(ent->velocity, forward, ...)`, the same vector into both bodies, unchanged by four commits of fixes to that file. R-135's `VectorSubtract` is new work. Changed also: R-CTF-3, `doc/provenance.md`. `doc/reconciliation.md` R-134..R-139. |
| **Amendment 1.31** | **A bot that fired before the bell, and a brain reading somebody else's ammo.** Two reports from an `ra2map9` pickup round, and the second is a half of R-BOT-1's contract nobody had written down. **The bots shot at the reporter all through the round countdown**, when RA2 has granted no damage yet (`set_damage(DAMAGE_AIM)` fires on the frame the countdown reaches zero) and has already handed out the round's only ammo load — so every such shot is spent ammo and nothing else. A person is told by the HUD and the announcer; the brain has no concept of a round and R-BOT-6's libvar set is fixed, so there is nowhere to tell it. The answer is the **button**, which is what this side of the seam owns: `RA_RoundFighting()` asks whether a client's arena is in `ASTATE_FIGHTING` and `BotExecuteInput` declines to put `BUTTON_ATTACK` in the command when it is not — aiming, tracking and weapon choice untouched, and a person's pre-fire untouched with them. **Both** arms are gated, the `ACTION_RESPAWN` one included, because that arm latches the button directly on the client and `ClientLagThink` runs `Think_Weapon` on a latched attack whether the bot is dead or not; the grapple is exempt, being movement rather than damage (R-140). **Then the second report — "the bots prefer the machinegun and never use the chaingun" — which is a pattern, and not the one the character files describe.** R-100 made `bot_updateclient_t.inventory` the same LENGTH at both ends; **no part of the contract ever said what a slot MEANS**, and the brain has a definite opinion — `botfiles/inv.h` out of the 1999 pak, slot 10 the Machinegun and slot 19 Bullets, read by `weapons.c`'s `weaponindex`/`ammoindex`, `items.c`'s `index` and every `switch(INVENTORY_*)` in a character's `_w.c` and `_i.c`. So the numbering is contract carried as **data**. `inv.h` is baseq2's itemlist with Gladiator's additions appended; this tree's is R-CORE-2's union of five donors, which agrees for six rows and diverges at the seventh where `weapon_grapple` sits. A straight `memcpy` therefore showed a fighter carrying 100/200/150/50/50 as **`shells 1 bullets 1 cells 0 rockets 0 slugs 1`** — each of them a weapon-owned flag one to five rows away — and `fw_weap.c` zeroes any weapon whose ammo test fails, so of nine weapons the brain could see six, the Railgun only because slot 16 lands on `weapon_grenadelauncher`. Which of the six a bot picks is an accident of that accident: measured, the control's 1059 fighting samples hold the Railgun 363 times and `give_ammo`'s starting Rocket Launcher 353 more **because nothing ever scored high enough to issue a `use`** — and arithmetic over the seventeen weight files in `pak7.pak` says that with the GL granted the broken winner is the Railgun for nine of them, while with the GL absent from an arena's `weapons:` line it is the **Machinegun for twelve of seventeen**, which is where the report's own word comes from. `BotFillInventory()` translates slot by named slot, holding **classnames rather than numbers** because the game's numbering is the side that moves; 73 of 73 rows resolve, the Tag token by `pickup_name`; `INVENTORY_HEALTH` and everything from `ENEMY_HORIZONTAL_DIST` (200) up are left to the brain, guarded by the resolver's range check. **The chaingun half is not a defect** — with the indices right it tops exactly one of sixteen stock characters (Trash) and loses to the Machinegun in eleven — and **the defect is not only arena's**, because `fw_items.c`'s goal weights index the same slots, so bot item-seeking under `dm`, `ctf` and `tourney` was reading the wrong rows too (R-141). **Nothing in the tree could see it, and that is the lesson**: every instrument printed the GAME's numbers and the game's numbers were right — `sv inventory` the itemlist, `botabi.py` the struct sizes, seven `_Static_assert`s the lengths. New **`sv botinv`** is written in `inv.h`'s terms and prints the brain's slots and the client's own inventory on adjacent lines, plus `hold`/`asked`/`dropped` for R-140, which is the only shape that shows a disagreement about MEANING rather than about bytes. Checks: new `scenarios/ra2holdfire`, A/B against a control library carrying this tree with both behaviour changes reverted and the diagnostic kept — 93 of 93 out-of-fight attack frames held against 0 of 14, ammo intact in every countdown sample against two bots down a slug, and `brain` == `game` for every bot and every ammo type across 1200 samples. `tools/playtest.sh` 194 checks, 190 passed; the four arena menu-toggle failures are the pre-existing set and fail identically on the control and on a second run of the fixed library. `make check` clean. `doc/botlib-contract.md` gains **"Item indices — the DATA half of the contract"**; `doc/reconciliation.md` R-140..R-141; `doc/commands.md` and `doc/regression.md` carry the command and the A/B tables. |
| **Amendment 1.32** | **The sweep R-141 asked for, and two donors' new work.** R-141's root -- a fact crossing a boundary this tree does not own, carried by a number whose meaning is fixed on the other side -- was swept over every such crossing, and the botlib's whole data contract came back clean but one: `WEAP_*` is not a name but a POSITION, the ordinal of a weapon's `#w_*.md2` in SP_worldspawn's precache block, because ChangeWeapon puts it in `s.skinnum`'s high byte and the client draws `weaponmodel[skinnum >> 8]`. Each donor numbers its own extras from 12 -- CTF's grapple, Xatrix's phalanx, Rogue's disruptor, each self-consistent ALONE -- and R-CORE-2 unions the content, so **seven weapons drew somebody else's model**: a phalanx and a disruptor appeared as grapples, a ripper and an ETF rifle as phalanxes, and so on down a list of nineteen. The Gladiator donor met the same problem merging the same packs and renumbered to 12..18; this does the same against its own block, and no shipped bot data file reads the per-weapon enemy slots, so the brain loses nothing. New **`tools/dupvalue.py`** asks the two questions separately, because a value check alone passes a header that is internally consistent and still disagrees with its list: every define family and enum for two names on one integer (failing only for families that ARE ordered lists, reporting the other twenty-nine pairs), and `WEAP_*` against the precache block itself. Two controls (R-142). **Two more from the same sweep.** An arena observer could shoot the fighters: `ClientLagThink` sends an observer's ATTACK to the chase cam and returns, except under arena where that arm is deliberately not taken -- RA2 has four observer modes and no chase cam, so ATTACK cycles them -- and the press then fell through to `Think_Weapon`, which has no observer guard. An arena observer has no ammo and the Blaster needs none, so a player in the queue could stand in the arena and plink a live round. The donor shares the fall-through, so it is 1999's, fixed at the root (R-143). And **every ruleset libvar but `dmflags` was stale for the life of the map** -- pushed once at library init, while all five are derived from cvars and dmflags a person can change mid-map, so bots went on using a grapple the server had turned off. `BotRulesetLibVars()` joins `dmflags` in the per-frame loop; the donor's narrower answer pushes `usehook` from its bot menu through `BotLib_BotLibVarSet`, **which this tree ported and never called**, so the wrapper goes with the fix (R-144). **Then both donors' new work, and one of them corrected this tree's reasoning.** `rocketarena2@f90a8fb` fixes the trackcam horizon lean, which this tree had already fixed differently and more widely -- but its measurement says the note here was wrong: `gi.Pmove` IS the camera's integrator, and it returns early for exactly one pm_type. The donor writes the literal `pm_type = 3`; **this port read that as "stop this client moving" and wrote PM_FREEZE, which is 4** -- so `track_think`'s clear-path branch, which sets a velocity and expects pmove to cover it, moved the camera not at all, and its blocked branch, which teleports, became the only thing that ever did. The follow camera did not follow; it snapped when something got in the way -- though not by sitting still, and the first measurement of it asserted the wrong thing: the pre-fix library still covered 1837 units with 9 of 15 intervals moving, because the blocked branch fires often. The defect is the shape -- measured, 13 of 15 intervals moving in steps up to 274 units with the fix against 6..8 of 15 in jumps up to 548 without it -- and the reading that settles it is the pm_type off the wire, 3 against 4. And where the teleport stamp survived into the mode only YAW followed the mouse, because `PM_ClampAngles` pins the other two and the countdown that clears the flag sits below the PM_FREEZE return. Both halves kept: the donor's pm_type, and this tree's wider `VectorClear` (R-145). `rocketarena2@811af42` is the other: `ArenaFightersRangeFromSpot` returned 0 with no fighters to measure against and a note here called that deliberate, because it drops the caller through to the random selector -- **it does the reverse**, since random spots collide, and in the staging area everybody is FIGHT_SPECTATING so the fighter pass counts nobody every single time. Two arrivals on one spot is R-135's siamese twins by the other road, and neither KillBox nor check_telefrag will touch a spectating client. Two passes now: fighters first, every live body only when there are none (R-146). **And `q2pro`'s mission-packs branch was rewritten**, with its fixes folded into the commits that add the libraries, so the pin names commits no longer on the branch. Diffed old pin against new tip over every path this tree took: `src/game` and `src/shared` are byte-identical, and of the CTF and mission-pack changes only two were not already here -- `PMenu_Close` before `PutClientInServer`'s memset, since every menu handle lives in the part about to be zeroed and R-MENU-3 had fixed only the disconnect path (R-147), and `!(self->flags & FL_FLY)` in `ai_run_slide`, **which reverses R-30**: that decision kept the broken grouping because turning the clamp on changes how every Ground Zero monster sidesteps, and the donor has now made the call, so §7 rule 7 takes it and the clamp operates for the first time since 1998 -- a behaviour change in sp and coop, recorded as one (R-148). Everything else upstream fixed was already here and is tabulated. Neither pin moves, for R-139's reason. `doc/reconciliation.md` R-142..R-148; `doc/provenance.md` records the rewritten branch; new `scenarios/ra2trackcam` reads the pm_type and the camera's travel off the wire. |
| **Amendment 1.33** | **The donor had the countdown gate and this tree dropped it.** Asked while working out which of 1.31's and 1.32's fixes belong upstream: `gladq2_src/p_client.c` guards BOTH its `Think_Weapon` call sites with `!(ent->flags & FL_OBSERVER) && ent->movetype != MOVETYPE_NOCLIP && !(ra->value && ent->takedamage == DAMAGE_NO)`, and that third clause is R-140 and R-143 in one line -- broader than either, because it stops a PERSON pre-firing a countdown as well as a bot. So both are **dropped donor halves** rather than the new work their entries claimed, and both entries are corrected in place. R-140's gate is still right and still needed (it stops the brain's request at the button, which keeps `sv botinv`'s asked/dropped honest), and observers are airtight either way because `ClientBeginServerFrame`'s site already carries `!G_IsObserver`. **What is still missing is the human fighter, and the donor's fix is not adoptable verbatim**: it gates `Think_Weapon` wholesale, and a weapon CHANGE completes inside that call -- `Use_Weapon` sets `newweapon`, `Weapon_Generic` reaches `ChangeWeapon` through WEAPON_DROPPING, and RA2's `fastswitch` only skips the raise animation (`p_weapon.c:544`) -- so taking it would freeze weapon selection for the whole countdown, which is how a Rocket Arena round is prepared. Trading a wasted shot for that is the wrong way round. Doing it properly gates the FIRING rather than the think, which is a per-`weaponthink` change and wants its own increment; the first attempt here put the guard on `ClientLagThink`'s outer `if` and would have broken chase-target switching for every non-arena spectator, which is recorded so the next attempt starts past it. **Left open deliberately**, and a person can pre-fire an RA2 countdown in `rocketarena2` too. `doc/reconciliation.md` R-149, and corrections in R-140 and R-143. **Both halves of that last sentence are corrected in 1.48**: the gate landed later in this same spec version (`doc/reconciliation.md` R-151, `RA_HoldFire()` and `ClientLagThink`'s clear at the latch, measured by `scenarios/ra2prefire`) and no row here ever said so; and `rocketarena2` gates four `p_weapon.c` fire arms, so the comparison was wrong as well. |
| **Amendment 1.34** | **Bots that size themselves to the arena, the person who could still pre-fire a countdown, and three donor commits nobody had read.** **`ra_botfill`** (new **R-RA-7**, default 0) makes the bot count follow the arena rather than the server: `minimumplayers` is one number for a whole map and Rocket Arena runs up to 32 games at once out of one, so a flat four is a crowd in `ra2map8` arena 3 and an empty room in `ra2map9` arena 2. The answer comes from two places **because the mod keeps it in two places, and that split is the whole rule**: a non-pickup arena is `arena.cfg`'s own `playersperteam` times the two teams a round is fought between -- not advisory, since `AddtoArena` admits a team of exactly that size and `check_teams` ejects one that grows past it -- while a pickup arena has no such number at all, because `arena_init()` overwrites `playersperteam` with **128** for every one of them and all thirty of RA2's own pickup arenas leave the key unset. For those the map is the only signal, and the two sides take **alternate** spawn indices, so the arena seats two out of every pair. **Counting spawn points for the other kind would be wrong and it is recorded because it is the obvious thing to try**: only 55 of 141 non-pickup arenas have as many `info_player_deathmatch` as `2 * playersperteam`, and `ra2map8` arena 3 has thirteen while declaring 1v1 -- filling it to its spawn count builds teams that `check_teams` then deletes. Measured on the wire: `ra2map9` reports want=10, 12, 2, 2, 2, 2, 4, 4 across its eight arenas, and `q2dm1` settles at exactly 10 of 10 with zero removals over three dumps and two rounds. Two things came with it -- a bot **pairs up** on an under-full bot-only team before inventing one, so the sixteen arenas declaring `playersperteam` 2 or 3 can reach their size for the first time (2v2 measured, against four teams of one with the switch off), and the census counts bots **in transit**, because `CheckMinimumPlayers` runs before `G_CheckRules` on the same 32-frame tick and `RA_ArenaPlayers` now shares `RA_BotFollowPeople`'s predicate so the two cannot disagree (R-150). **R-149 is closed, and not the way the donor closed it.** A person could still empty a magazine into an opponent who cannot be hurt, spending ammo `give_ammo` hands out once a round. `gladq2_src` guards `Think_Weapon` at both call sites and one of those runs EVERY FRAME and is the weapon state machine -- `Use_Weapon` sets `newweapon`, `Weapon_Generic` walks WEAPON_DROPPING to `ChangeWeapon`, and `fastswitch` only skips the raise -- so the donor's clause freezes weapon SELECTION for the countdown, and choosing a weapon during the countdown is how a round is prepared. So the think keeps running and **the button goes**, which is what R-140 already does for bots one seam away; every weaponthink in the merged tree reads `client->buttons` or `latched_buttons`, so one clear at the latch reaches baseq2's weapons, the mission packs' and Threewave's. Observers keep ATTACK (it is RA2's camera-mode key, R-RA-4) and so does a player holding the grapple, which is movement rather than damage -- R-140's own two exemptions. A/B on `ra2map7` arena 6: **STAT_AMMO 50 -> 46 through the countdown before, 50 -> 50 after**, both builds still spending after the bell and both still able to change weapon while held back (R-151). **Then the four failing play-test rows, and the game was right about all four.** `arena/score closes the menu` and the three that cascade from it encoded the contract `30503c6` deliberately replaced: RA2's menu is a per-client `CS_STATUSBAR` overwrite rather than a layout, so the board and the menu are two channels, RA2's own `Cmd_Score_f` has no menu test, and keeping one meant an observer's press was always spent closing the menu `move_to_arena()` had just reopened. Read off the wire rather than argued: `score` leaves the menu up and produces a 175-byte layout, and `inven` toggles cleanly four times running. The scenario asserted a fixed open/close/open parity from an assumed starting state; it drives to a known one now, and `ra2menuleak`'s respawn driver was wrong in the same shape -- it waited for `spectator_respawn` to repeat, which it cannot, because `PutClientInServer` sets `resp.spectator = pers.spectator` on both arms (R-152). **And three donor commits had gone unanalysed.** `rocketarena2@28a8af7` is two leaks and this tree had both: `PutClientInServer` memsets `gclient_t` while `menuqueue`, `curmenulink` and `selected` sit outside `pers` and `resp`, and R-147's close is **not** enough on its own because under arena closing is HIDING by design -- the arbiter repaints and frees nothing -- so every respawn orphaned the menu the previous one opened, on a function that ends in the `move_to_arena` that opens it. Lifting the teardown out as `free_menu()` showed the second: `AddMenuItem` makes three allocations per row and it released two, leaking one `menuitem_t` per row every time anybody closed a menu by picking one. Measured with `ra2menuleak` on `ra2map7`: **+15 blocks and ~851 bytes per respawn before, +0 and +0 after**, over four pairs, with the menu still opening and closing to the same three byte counts on both builds. `osp-tourney@11563de` is a third and it is the BOT layer's, so it is not tourney's at all: `BotLib_BotUpdateClient` copied `ps.pmove.pm_time` straight into the frozen 1999 botlib ABI, which is a byte of 8 ms tics, while the source field is MILLISECONDS on an extended server -- so every hold the brain saw ran eight times long, 1600 ms for a spawn. `units.py` had an exemption naming that exact line and arguing it was safe *because* it was a copy; the reasoning was wrong -- a copy constructs no duration but can still cross a unit boundary -- and the check now flags a copy between two fields of different kinds, with a control that restores the old line and fires. `osp-tourney@48408f1` is the `match_mode` clamp, which this tree already has as R-OSP-13, reached independently from the harness's own finding; the two differ only on invalid input (the donor clamps 4 up to 3, this falls back to 0 with a message) and neither is a defect, so the divergence is recorded rather than resolved (R-153). **And `armorprotect` is 1 in the shipped `arena.cfg`**, at the author's request: both 1 and 2 exempt a team-mate's splash, the difference is you, and 2 leaves your own rocket to eat your armour. The nine arenas that name their own are untouched -- `ra2map22` reports aprot=1 on the arena that asks for it and 0 on the seven that ask for that -- and `sv arenadump` prints `aprot=`/`hprot=` now, because being hit by something was the only other way to see the value in force. The file's byte-identity claim in `doc/provenance.md` was **already stale** before this and is corrected rather than restated (R-154). **And the three things this report flagged and left are closed in the same increment (R-155).** The bot menu's RA2 page loses twelve rows that toggled cvars **registered and read by `gladq2_src/g_arena.c`** -- the Gladiator SDK's own arena, which R-ARENA-1 does not carry -- while the real Rocket Arena keeps every one of those concepts as a per-arena setting out of `arena.cfg`; they are not re-pointed at `arenas[n]`, because one shared menu tree cannot say whose arena's value it is showing, and RA2's own `arenaadmin` propose-and-vote menu is the per-arena path that already exists. `ra2join` and `ra2spawn` failed because they set **no cvars at all**: right for a library selected by its gamedir, wrong for one that reads `g_ruleset`, so Colosseum booted them as `dm` where there is no menu to click -- and sweeping for the same omission found **three more**, `ra2twins` and both servers of each ctf scenario, five in one cause. `ra2camera` was failing on two counts besides: loadout rows hardcoding a 100/100 the file stopped granting in 1.27, and -- this increment's own doing -- a row that proved the rocket had detonated by watching the attacker's armour drop, which `armorprotect 1` is precisely the setting that prevents. The witness is the KNOCKBACK now, which T_Damage applies before either protect arm returns: 121 units either way, `200 -> 200` under 1 and `200 -> 158` under 2, which is also R-154's behavioural proof. **And chasing that found a feature that had never run once**: `pers.showmotd` is read by `init_player()` and cleared by `menuMotdContinue()`, and nothing ever set it TRUE -- the merge kept the field, the reader and the clear and dropped the single write, so `motd.txt` was loaded at every map load, announced on the console, and shown to nobody. The donor's two halves are carried (the write in `ClientConnect`, the preserve across `InitClientPersistant`'s memset) and a client now gets `"Message of the Day"`, 9 rows, on connect. That moved the menu parity a third time and exposed R-152's bug in `ra2menuleak`'s own closing guard, whose numbers had been read backwards all along. **The provenance sweep found four stale claims, three older than the increment that noticed the first**: `arena.cfg`'s byte-identity and md5, `genptr.py`'s byte-identity, R-CORE-5's CONDITIONAL claim flattened into an absolute one about a five-donor merge (measured: 19 of 72 still identical, 52 diverged, 7 with no upstream counterpart), and an rr-cache count of 411 that is 407. Everything else in the file was checked and is recorded as checked. Changed: new R-RA-7, R-ARENA-2, R-ARENA-1, R-BOT-28, R-OSP-13, R-CORE-5, D6. `doc/reconciliation.md` R-150..R-155. |
| **Amendment 1.35** | **`ra_botfill` for the other two rulesets, and the reason tourney does not get one.** 1.34 gave `arena` a bot count read off the game instead of off the server and left the same complaint standing next door: `minimumplayers` is one number for a whole rotation, and it is a number an operator has to re-guess every map change. **New `ctf_botfill` (R-CTF-8) and `dm_botfill` (R-DM-1), both default 0**, so a tree with neither set behaves exactly as it did. **Neither ruleset declares a capacity, which is why the answer is the map**: Threewave has no `team_maxplayers` -- `matchlock` locks a match rather than sizing one and `warn_unbalanced` only warns -- and deathmatch has nothing at all, so both fall into the case R-RA-7 already named for a pickup arena. **The pool is two short of the count, and both rules rest on that fact**: `SelectRandomDeathmatchSpawnPoint` and `SelectCTFSpawnPoint` each find the two spots nearest a player, refuse them, and draw from `count - 2`, so `G_SpawnPointPool()` is one function in `g_utils.c` and both fills ask it. **Arena's own answer stays what it was and is not a contradiction**: `SelectRandomArenaSpawnPoint` refuses NOTHING -- it walks every candidate on the side's parity and takes the first with 50 units of clearance -- so a pickup arena still seats its whole even count. Two selectors, two pools, one rule. `dm_botfill` is the plain case, one pool: measured over the eight `q2dm` entity lumps, **8, 5, 5, 9, 7, 6, 4, 4** against raw counts of 10, 7, 7, 11, 9, 8, 6, 6 -- and the first row is the number each of those maps is actually played at, which is the argument for the subtraction rather than a taste for it. `DF_SPAWN_FARTHEST`'s selector does use every spot and is **not** given the two seats back, deliberately: `dmflags` is not latched, so a target that moved with the flag would add two bots on the write and take them away on the next one. Under `teamplay` the target is rounded down to even. **`ctf_botfill` has three pools, because a CTF client draws from two different ones** -- `SelectCTFSpawnPoint` sends a player to its own base while `resp.ctf_state` is 0 and to `info_player_deathmatch` for every spawn after, one line that decides the whole rule -- so a side seats the smaller of its base and half the shared pool and the target is twice that. Measured across the eight shipped maps: **16, 12, 14, 4, 20, 14, 14, 16**, and both halves of the `min` earn their place (four maps bounded by the shared pool, three by a base that cannot seat the side at the whistle, `q2ctf4` by a shared pool of five on a map whose bases would claim 8v8). **Taking the base pools alone would be wrong and it is recorded because it is the obvious thing to try**: `q2ctf1` carries 14 and 16 team spawn points and is played 8v8 -- Threewave's mappers used them for variety, the same trap R-RA-7 records for a non-pickup arena. One consequence the rule owns: **the bot a fill removes is named**, because `removebot` with no name takes the lowest client slot and a server filled to an even target could otherwise be shrunk 4v4 -> 4v3 -> 4v2 as people arrived; `CTFBotFillName()` takes one off the larger side, which is `RA_ArenaBotName` for the same reason one ruleset over. R-150's finding about the clamp holds unchanged for two more rulesets -- a switch cannot carry a count, so `BotFillNoMore()` holds the ceiling in `bl_spawn.c` and `BotSpawn()` clears it -- and `CheckMinimumPlayers` gains **one** variable rather than two arms, because these two differ from arena's in exactly one way: arena's census is per arena and these fill the whole server, which the function's own loop already counts. **The switch is read above the 32-frame gate and the target below it**, which the first version of this got wrong: everything above that gate runs on every frame and the target is not a cvar read but a walk of the entity list, three times under `ctf`. Arena's has never had the problem -- `RA_BotFillArena()` is a cvar and a walk of `game.maxclients` -- which is why the shape was easy to copy wrong. **The menu row moves off the RA2 page onto the BOTS page**, directly under `minimum players`, labelled with the ruleset's own cvar name out of `BotFillCvar()` and absent under the two rulesets that have none -- three rulesets sharing one concept do not want three pages carrying it, and nothing 1999 put on the RA2 page moved. **And the tourney question is asked and answered: it already has the concept, and what it has is better than what these two needed.** `bots_minplayers` (default 4) is the flat count under R-OSP-11's spelling, `bots_autoload 4` tops the roster up regardless of the player count, and `m_mode == 3 && bots_autoload == 2` zeroes the count because a duel has no seat for a bot -- one mode-aware clause, and it is an off switch. What tourney has that the other two lack is **`team_maxplayers`**: 4 by default, forced to 1 `CVAR_NOSET` under mode 3, clamped so twice it fits `maxclients`, compared against `OSP_teamCount` at six sites and enforced by `OSP_addTeamMember`. That IS `playersperteam` for an arena and it is exactly what `ctf` and `dm` had to read off the map for want of one, so `2 * team_maxplayers` is available to a tourney operator as a value they set rather than a rule the port infers. Modes 0 and 1 would fall to dm's map-sized answer; that is **not** done, because R-OSP-11 preserves tourney's bot contract rather than redesigning it, and `sv ruleset`'s tourney row prints the finding rather than a blank line. Checks: new `scenarios/botfill`, five servers, **24 of 24 passed** -- `dm` filling 0->8 on `q2dm1` and holding at 8 across three fill ticks, two people arriving costing exactly two bots, the control at `dm_botfill 0` settling at `minimumplayers 4` and not at 8, `q2ctf1` reporting `want=4 of seats=16, shared=17 base=12+14` under a latched `maxclients 4`, `q2ctf4` reporting `seats=4` with the sides level (2v2, then 1v1) before and after two arrivals, and a `tourney` server printing `no fill switch -- bots_minplayers 4 is the target` and settling there, which is the only place R-156's finding is observable from outside the library. **Two of those rows were wrong on their first run and the game was right about both**: a ctf client that has not picked a team is an OBSERVER, `G_IsObserver()` is `ctf_team == CTF_NOTEAM` under ctf and `BotCountsAsPlayer` excludes it (R-CTF-5, the Gladiator SDK's own exclusion), so two clients sitting in the join menu do not raise the census and the fill was right not to remove anybody -- they issue `team red`/`team blue` now; and "the two sides are equal" read `red=0 blue=0` and passed on nothing, because `clients` counts a CONNECTED bot while R-CTF-4 assigns its team inside the deferred `ClientBegin`, so the scenario now settles on every bot having reached a side and asserts both halves. `tools/playtest.sh` 196 checks / 0 failed (the 1.34 baseline, unmoved), `make check` 25 audits clean, `-Werror` clean on both build halves. **And the header said `Spec version 1.33` while the table's last row was 1.34** -- corrected here rather than restated. Changed: new R-CTF-8, new §6.12 R-DM with R-DM-1, R-RA-7, R-BOT-28, D6. `doc/reconciliation.md` R-156. |
| **Amendment 1.36** | **The second selector is gone: OSP's four modes of play are four rulesets, and `dm` is now one of them.** `g_ruleset` chose among five values and then the `tourney` ruleset chose again, among four, with `match_mode` cached into the global `m_mode` -- two latched selectors, two validation paths, two diagnostics, for one kind of choice. R-MODE-2 called them *orthogonal*; they never were. The four modes become **`dm`, `dmpro`, `tdm` and `duel`** (R-OSP-12), `match_mode` and `m_mode` are **deleted**, and the ruleset name `tourney` is **removed outright** -- not aliased, not mapped, not special-cased: it is simply not a ruleset name, so it takes R-MODE-1's unknown-value path and gets the same message any other unknown value gets. **`dm` is therefore no longer baseq2's deathmatch**, and the thing that had to be separated first is that `ops_dm` was two things wearing one name: the `dm` ruleset's row *and* the table every NULL row inherits from. It is **`ops_base`** now, selectable by nothing, and R-MODE-6 and §7 rule 2 are reworded to match. **None of its five functions could be deleted**, and not for the reason the plan assumed: `ctf`, `arena` and the OSP family call them **by name** from their own C -- `ctf_CheckRules` and `RA_CheckRules` both call `CheckDMRules`, `OSP_EndLevel` and `RA_EndLevel` both fall back to `EndDMLevel`, and `PutClientInServer` calls `G_SelectSpawnPoint()` before arena and the OSP family re-place the client. `DeathmatchScoreboardMessage` is **not** among them, and new **R-OSP-14** says why: under that name the donor keeps its own page dispatcher rather than baseq2's board, so tourney's three callers reach the `ScoreboardMessage` row through the gate. **What *was* unique to the old `dm` is four things and they are deleted**: `sb_dm_tail()`, reached only through a `switch (r)` `default:` that only `RULESET_DM` took; the **`dm` column of `STATSLOT_MAP`**, read by `RULESET_DM` alone, so five stat numberings become four (`osp`, `ctf`, `arena`, `sp`) and R-OSP-7 clause 3's "one column per ruleset -- including `dm`'s own, which owns 18/19" is false in both halves; `BotRulesetLibVars()`'s `default:` arm, which after this only `sp` could reach and `sp` has no bots; and the `ch` acceptance -- Colored Hitman has been out of scope since 1.2 (N7) and a cvar that is read, ignored and warned about is a third way of saying nothing. The **`ch` libvar survives**, pushed as `"0"`, because that one belongs to the frozen botlib and not to this dispatch (R-BOT-6). **The measured surface**: `m_mode` was **234 sites across 22 files**, twelve of them shared files rather than `src/tourney/`'s own, and `RULESET_TOURNEY` a further **97 across 19**; `RULESET_DM`, by contrast, appeared at **12**, which is the whole argument for why the old `dm`'s identity was never its own name. **Every comparison form was translated mechanically** rather than by reading -- `m_mode == 2` to `RULESET_TDM`, `m_mode > 1` to `OSP_IsTeams()`, and so on -- so the rewrite is equivalent by construction rather than by review. **The traps were the tables, and one of them is why this needed a plan.** `modifier_ok[MOD_COUNT][RULESET_COUNT]` is designated on its outer dimension and **positional** on its inner rows, so growing `RULESET_COUNT` pads the three new columns with `false` and `tdm` silently loses `MOD_TEAMPLAY` -- with `-Wno-missing-field-initializers` in the build flags there is no warning, and no audit in the tree can see it. The rows are designated now. Three more of the same family: a missing `ruleset_ops[]` row is NULL and `G_InitRuleset`'s `?:` hands that ruleset **baseq2**; a missing `ruleset_names[]` row is NULL rather than `"?"`; a missing `slotdef_t.slot[]` column is **0**, a real slot, not the `-1` sentinel. And `G_GladiatorObserver()` is an "everything except" test naming `RULESET_ARENA` and `RULESET_TOURNEY`, so `dmpro`, `tdm` and `duel` would each have switched on the *Gladiator* observer underneath OSP's own -- two live observer systems, which is the bug §7 rule 6 names. **`teamplay` is refused across the family** (R-MODE-4), because once team play is a ruleset a modifier that also reaches it is the second selector this amendment exists to remove; the refusal costs nothing measurable, since `G_TeamplayEnabled()` has exactly **two** callers here and R-MODE-5's "20 call sites" describes `q2pro-ng`'s prior art rather than this tree. R-BOT-29's rule survives the deletion of the value it was written about: the brain's `teamplay` libvar reads `RULESET_TDM` **alone**, not the family and not the family's team half, because a duellist and an opponent in the same model would otherwise become team-mates. **One thing lapses and it is recorded rather than found later (R-MODE-3):** `gamerules` reaches Ground Zero's `DMGame` vtable through `CheckDMRules()`, which `OSP_CheckRules()` does not call -- so Tag has never run under `tourney` and now does not run under `dm` either. It keeps working under `ctf` and `arena`. **And the three bot-fill switches become one.** `ra_botfill`, `ctf_botfill` and `dm_botfill` are one bare **`botfill`** (R-RA-7, R-CTF-8, R-DM-1). R-OSP-11 did not cover them and never did: that rule governs cvars a **donor** named, so each donor's readme keeps spelling its own; all three are Colosseum's own invention from 1.34 and 1.35, appear in no donor's documentation, and name one concept -- so §7 rule 6 governs instead. The logic was already unified behind `BotFillCvar()`/`BotFillTarget()`; what was not was the clamp, which `arena.c` carried as its own copy of the same three lines over its own `static int botfill_ceiling`, so R-DM-1's claim that "the ceilings are shared" was true of two rulesets and not the third. **This also closes what R-156 deferred.** R-156 recorded that modes 0 and 1 "have no teams and would fall to the same map-sized answer `dm` gets" and declined to give them one, on R-OSP-11's authority; once mode 0 **is** `dm`, the same ruleset cannot be both the rule's subject and its exception. `dm` and `dmpro` take the map-sized answer, `tdm` and `duel` take `2 * team_maxplayers` -- the capacity R-156 itself named as the better one. **R-OSP-11's one unavoidable migration** is stated rather than smoothed over: `dm` reads `bots_minplayers` now, so a hand-written config setting `minimumplayers` stops being read -- **and a check walked into it before a person could.** `tools/extras.sh`'s R-VER-20 row waits past an item's think deadline and asserts it came back, and it failed on the first run after the flattening: **5 of 11 weapons back 50 seconds after the last bot left**, where a build of the previous commit settles at 11 and stays. Not a respawn defect -- the row sets `minimumplayers 0` so that `sv removebot all` empties the map, `bots_minplayers` defaults to **4**, and the fill put four bots straight back, so the row was measuring item respawn on a map still being played in. With the name the ruleset actually reads, the new library is indistinguishable from the old one at every sample. Every driver that means "do not fill" now sets both spellings, which is what `scenarios/botfill` already did. **R-OSP-13 is struck** -- there is no `match_mode` left to validate, and what it defended is true by construction: an unknown ruleset warns and falls back, there is no numeric range to sit outside of, no banner block choosing on a bare `else`, and `match_type` is derived from the ruleset so serverinfo cannot drift from behaviour. R-VER-16's out-of-range arm goes with it and its four positive rows become four rows of the ruleset sweep, which is the point: one selector, one matrix. R-VER-2 grows from **20 rows to 28**, with no row for `tourney` -- the `banana` control already covers an unknown value, and a row asserting that `tourney` in particular falls back would be asserting it is still special. And `tools/botmatrix.sh` gained the wildcard arm its `case $rs` never had -- without it a new ruleset reuses the previous iteration's variable and asserts something stale instead of failing. `tools/donorgate.py` had to learn that a donor's surface can sit behind **four** rulesets rather than one; its `DONORS` map was 1:1 and its gate regex looked for a single `RULESET_` name. **Measured on the wire rather than claimed.** `tools/bootmatrix.sh` is **28 of 28** -- the seven rulesets across both content layers -- and its three controls all fire, the first of them being the unknown value that `tourney` is now one of. `tools/extras.sh` is **52 of 52** with 6 controls, and its D6 rows exec all seven shipped configs clean. `tools/playtest.sh` is **234 of 234**, its headless clients reading the composed bar, the slot map, the spawn placement and the obituaries off the wire under all seven. `tools/botmatrix.sh` is **18 of 18** -- and two of those rows are new evidence rather than a re-run, because `tdm` and `duel` are asked for sixteen bots and correctly refuse: `tdm entered=8 ready=8 team0=4 team1=4` at twice the default `team_maxplayers`, `duel entered=2 ready=2 team0=1 team1=1` with `FL_BOTCLIENT=16`, the other fourteen in R-OSP-12's queue. R-BOT-23's frame cost is unmoved (mean 2439 us, worst 8036 us) and R-VER-6's index tables read 175/8192 and 175/256 as before. `tools/smoke.sh` passes with both its controls, `make check` is 25 audits clean, all ten build configurations are warning-free under `-Werror`, and R-ENG-1a's **old game API** builds clean too. `tools/playtest.sh` found the last stale control: R-VER-27's "a modifier the matrix refuses" server was `dm` + `runes`, which is an **acceptance** now that `dm` is OSP's RegularDM -- so the control is `dm` + `teamplay`, the pair this amendment actually refuses, and the old one is kept as the acceptance check that exercises R-88's derivation under the four rulesets that gained runes. **And `scenarios/botfill`'s `dm` phase repeated one ruleset over the exact finding R-156 records against its `ctf` phase**: two clients connected to a filled server did not cost two bots, because a client under an OSP ruleset connects as an **observer** and `BotCountsAsPlayer` asks for `osp_entered == ENTERED_ENTERED`. Connecting is not playing, in either donor's model; `ctf`'s people already issue `team red`/`team blue` and `dm`'s now issue `join`. **And with them entering, the row was still one bot out, for a second reason that is also the donor's**: tourney's removal test is `(numplayers - bots_votedin - 1) > want` where every other ruleset's is `numplayers > want`, so its arms add up to `want` and remove down to `want + 1`. `dm` is inside that contract now, so under all four a filled server keeps one bot more than the target once people are on it -- 1999's behaviour for the ruleset those four came from, and `ctf`'s phase is the control that shows the other arm still gives both seats back. Four checks in this increment were the checks being wrong and the game being right, which is worth stating plainly: a flattening that changed what `dm` *is* was always going to invalidate assumptions written when `dm` was baseq2, and every one of them surfaced as a red row rather than as a silent difference. The composed statusbar (R-OSP-7a) re-measured over the seven: `dm` **770**, `dmpro` 770, `tdm` **759**, `duel` 759, `ctf` 658, `arena` 578, `sp` 310 -- `dm` composes the bar `tourney` composed, where baseq2's was 442, and the three untouched rulesets are unchanged to the byte. And the bot fill's per-ruleset targets all land on the figures their own requirements record: `dm` **8** on `q2dm1`'s pool, `ctf` want=**12** clamped from seats=**16** with shared=17 base=12+14, `arena` **10** on an idmap, `tdm` **6** from `team_maxplayers 3`, `duel` **2** from the 1 it forces `CVAR_NOSET`. Changed: R-MODE-1, R-MODE-2, R-MODE-3, R-MODE-6, R-MODE-7, R-OSP-7, R-OSP-11, R-OSP-12, R-OSP-13 struck, new R-OSP-14, R-BOT-29, R-CTF-8, R-RA-7, R-DM-1, R-VER-2, R-VER-16, R-VER-27, §2, §6.7, §7 rule 2, §9 Phase 1. `doc/reconciliation.md` R-157. |
| **Amendment 1.37** | **A deep read of `arena` against its donor, and fifteen things came back.** Method: `git diff baseq2 port_ra2` out of the vendored replay bundles is RA2's own feature set by construction (R-PROV-3), so every hook it contains was checked against this tree rather than the files being read side by side. Two of the findings are scoring and both were visible to any player inside one round. **A death with no player attacker cost TWO frags**: `ClientObituary` calls `RA_Obituary()` and then reaches its own `resp.score--` on the same paths, so every fall, lava bath, drowning and self-rocket in an arena has been charged twice since Phase 4. **And a team kill paid +1**, because `RA_Obituary` never implemented the kill half at all and baseq2's stand-in keys the penalty on `MOD_FRIENDLY_FIRE`, which `T_Damage` raises only for dmflags teamplay -- which arena never sets, and which R-ARENA-1 had already re-pointed `OnSameTeam` away from. `scorebydamage` suppressed neither. All of RA2's arithmetic is in `RA_Obituary` now, in the donor's shape, and the three baseq2 sites are gated; the obituary TEXT is still baseq2's (sec 7 rule 1, unmoved). **Four stat columns had never been written**: `RA2_STAT_GRENADEKILLS`, `ROCKETKILLS`, `RAILKILLS` and `OTHERKILLS` were declared and incremented by nothing, so the per-weapon breakdown in every round record ever written was a column of zeroes -- bucketed by `meansOfDeath` rather than by the donor's inflictor-model test, which cannot see forty-odd means of death and mis-files a held grenade as whatever the shooter was carrying. **A fifth thing was hiding under the fourth**: RA2's suicide announcer (`ra/outstand.wav` and two more) grades the opponent you took yourself away from, reads `self->enemy`, and could never have fired -- `Killed()` overwrites that field with the victim on a self-kill, and the donor guards it with `if (targ != attacker)`. **Three features were dead code.** `teamskins_precache*` is read by `RA_SkinIcon()` and was written nowhere, so the four arrays were zero, the match could not succeed and RA2's team colour in stat slot 0 has never once been drawn (`RA_Precache()`, R-161). `rocket_speed` is parsed by maploop.c, stored in `arenas[n]` and pinned by a `_Static_assert` -- and `Weapon_RocketLauncher_Fire` passed the literal 650, so a server that set the key got nothing (R-163). `fastswitch` was half carried: RA2 skips the raise AND seeks the drop to `FRAME_DEACTIVATE_LAST`, and only the raise was here, so every weapon change in every arena paid a deactivate sequence 1999 skips -- and `colosseum/arena.cfg` ships `fastswitch: 1` globally (R-162). **Two were leaks of the wrong kind.** `say_team` reached the WHOLE SERVER, because inherited baseq2 asks dmflags whether team chat means anything and an arena team is `resp.teamnum`; and plain `say` was server-wide too, which made RA2's own `say_world` a synonym for it and left `show_string` -- the donor's arena-scoped print, still in arena.c -- with no chat caller (R-165). **The offhand grapple obeyed the wrong switch entirely**: `grap_on` reached `CTFHookThink`, whose gate is `ctf_hook`, which CTFInit registers "1" for every ruleset (R-42) -- so it worked in an arena whose `arena.cfg` says `grapple: 0`, which the shipped file does, during a countdown, and for an observer. `give_ammo` was already right, which is what hid it: the ITEM obeyed the setting and the offhand hook did not. `RA_HookThink()` is the donor's own two conditions, and the brain was being told from a fourth switch again -- `usehook` came from the Gladiator SDK's `MOD_HOOK` rather than from `allow_grapple`, wrong in both directions (R-164). **The intermission never closed the arena menu.** `clear_menus(ent)` is `MoveClientToIntermission`'s first line in the donor and the merge dropped it; an RA2 menu IS the client's statusbar, so it stayed on screen over the end-of-level board with the real bar never written back -- and g_spawn.c's clearing loop had been silently carrying every map change rather than the console `map` its comment describes (R-159). Two more lines of `BeginIntermission` went with it: **a round still being fought when the timelimit ended the level was never written out**, and **an empty server never left the intermission at all** -- for a deathmatch ruleset the only other writer of `exitintermission` is a button press from a connected client (R-160, taken under arena only; the hole is baseq2's under `dm` and `ctf` and is recorded rather than widened). **`arena.cfg` and `motd.txt` were still read relative to the working directory** -- the same defect `ra2stats.c` and `gslog.c` each carry a written correction for, on the two files that sweep missed, so a server started from anywhere but the installation ran on built-in defaults and said so once (R-167). And four smaller ones: `drop` was live where RA2 empties it, on the same reasoning already written against `TossClientWeapon` (R-166); `ra2map13`'s crouch-to-die escape hatch was gone and that map is in the shipped maploop (R-168); observers grunted when they jumped, where the matching landing sound was already gated (R-169); `CheckTeamDamage` had no arena arm, so a team-mate on your hook heard `grhurt.wav` while healthprotect cancelled the damage a function later (R-171). **One finding was investigated and REFUSED, and the check is what refused it.** RA2 dispatches `kill` to nothing; gating it here broke `ra2observer`, which asserts that a round outlives a fighter's suicide and that wiping a side still ends it. SPECS.md's own "dead functions are live again" names `Cmd_Kill_f` in RA2's retired list and says every one of them is live in Colosseum. So the difference from the donor stands and is written down as a decision rather than corrected as a defect (R-166). **And one was the audit's, not the code's.** R-161's precache loop is the first array of image indices in the tree, and `slotkind.py` learned the SUBSCRIPT rather than the array -- so `i` was taught to hold an image index and six correct `num` writes were reported as `pic`. Its self-test could not have caught it either: the writer scan reads injected controls and the learner did not, so a kind error laundered through a variable declared in a control was invisible. Both fixed, with a new `indexed array` control (R-172). Checks: `make` clean under gcc and clang at both API settings, 24 audits clean, `tools/playtest.sh` **234 of 234**, `bootmatrix.sh` 28 of 28, `extras.sh` 52 of 52, `osprunes.sh` 33 of 33, `smoke.sh` pass, and the arena scenarios re-run one at a time -- `ra2spawn`, `ra2join`, `ra2menuleak` (+0 blocks over four respawn pairs), `ra2twins`, `ra2prefire`, `ra2queuefire`, `ra2holdfire`, `ra2camera`, `ra2gslog`, `ra2teamfire` (6 cross-team kills, 0 same-team) and `ra2observer`. Changed: R-ARENA-1, R-RA-4, R-MENU-3, R-SEC-7, R-TOOL-5. `doc/reconciliation.md` R-158..R-172. |
| **Amendment 1.38** | **Five reports from one play test, and three of them wanted a requirement rather than a patch.** `g_ruleset arena` with `xatrix 1`, then `rogue 1`, on `ra2map11` and `ra2map12`; `doc/reconciliation.md` R-186 carries the findings and this row carries what they changed here. **R-RA-2a is new and it AMENDS A DEFAULT R-182 chose deliberately.** R-182 gave the `weapons:` key's pack half no default and reasoned that a cfg naming none of the six leaves the key meaning what it always meant; the premise is true and the conclusion does not follow, because 137 of the shipped `arena.cfg`'s 171 arena blocks name a `weapons:` line of their own and the other 34 inherit the default -- so between them the file covered every arena and neither path could grant a pack weapon. `xatrix 1` drew two menu rows reading NO and changed nothing else, on any arena of any map. The key has two halves with separate defaults now, `nopack` is how an arena refuses all six out loud, and what an arena STORES is separated from what it GRANTS because an `arena.cfg` is served by servers running either pack, both or neither. The same row gates the five pack ammunitions per layer, which two of them require rather than merely benefit from: `ammo_tesla` and `ammo_trap` are the weapon as well as the ammunition, so an ungated count is an item in the weapon cycle and not a number on a HUD -- fifty Teslas and five Traps were being handed out by servers running neither pack. `sv arenadump` grew a `loadout` row, because R-182 recorded that all of this was `sv`-invisible and had to be measured with a probe compiled into `give_ammo`. **R-MENU-6 is new and it is R-MENU-5's missing sibling**: R-MENU-5 bounds a layout in bytes and nothing bounded it in PIXELS, so a page size was a constant rather than a number derived from the pic the menu draws over. `inventory.pcx` is 256x192 at `yv 8` with a flat interior at pic rows 18..174, glyphs paint eight rows, and the row grid starts at `yv 40` -- seventeen rows fit and the donor's eighteen paints across the frame while its "(More)" marker lands on the bevel outside the box. That was always true of RA2's own 26-row menu; R-182's two extra weapon rows only moved the damage onto a row somebody reads. Checked against R-SEC-2's kept-bugs ledger first -- all seventeen entries, none of them this -- so it is a deliberate divergence from `port_ra2` and recorded as one. Seventeen and not sixteen because sixteen collides with R-RA-8: it pushes the "Allow Bots" row off page 1, which is the position that row was placed for, and `scenarios/ra2botvote` now prints the boundary rather than the presence. **R-KEY-5 is new and it is the group's fourth contract, a LIFETIME rather than a name**: where a level-scoped allocation is indexed by a field that survives the level, the clear belongs where the allocation is DROPPED. The rule already lived as a comment in `g_spawn.c`'s loop at `gi.FreeTags(TAG_LEVEL)` -- "the free is what invalidates them, so the free is what clears them" -- at four menu-handle sites, and `teams[]` indexed by `resp.teamnum` was the fifth and was not in it. That is a SIGSEGV rather than a leak, and it needed the bot layer to become reachable: `BotStarted()` defers a bot's `ClientBegin`, which is where `InitClientResp` would have reset the field, so Phase 6 turned a 200-millisecond window into twenty seconds of frames on the first visit to a map whose `.aas` came out of bspc with an empty REACHABILITY lump, and a client pressing `score` in it walked the client list into a NULL slot. The donor's reads were safe by accident in 1999 and neither donor is wrong on its own, which is why §7 could not decide it and R-KEY had to. Writing the requirement moved the fix: 1.38's first cut cleared the field at `arena_init()`'s TagMalloc and it is in the free loop now, beside the four it belongs with. **Two reports were not defects and are recorded as checked.** `track_change` is byte-identical to `rocketarena2-public/arena.c:615`, where the tracking restriction applies only under `competition` -- with `playersperteam 1` the only FIGHT_ALIVE client in an arena is the opponent, which is what looked like a filter pointed the wrong way. And RA2's id-view row pointed at `playerskins + n`, which `stat_string` draws verbatim, so it read "Sarge\male/red"; Threewave answered the identical question in 1998 by keeping the bare name in `general + n` (R-CTF-6) and the write is made under arena too now, with the stat cleared in `SetObserverMode` where the camera's subject goes and `UpdateStatusBars`' two literal slot numbers resolved through the map that the writes beside them already use. Changed: §0 header, R-RA-2a new, R-MENU-6 new, R-KEY-5 new. |
| **Amendment 1.39** | **The base moved and three latent bugs came with it, one of which this tree had already fixed.** `q2pro@eefadf25` on `feature/mission-packs` carries three repairs to the imported mission packs that no compiler can see, and §7 rule 1 decides all three without a judgement call. **FindSubstituteItem's three dmflag filters tested the item being REPLACED inside the loop choosing the item to replace it WITH**, which is loop-invariant and the inverse of its own comment -- and R-183 item 11 found exactly that, described it correctly and DECLINED it on two grounds that have both since expired. Upstream no longer has it, and the other ground understated the defect: this is not an inverted filter, it is the trigger for a **use-after-free**. `SpawnItem` carries its own correctly-written copy of the same three guards, each of which calls `G_FreeEdict` and returns; `DoRandomRespawn` returns its `newEnt` whether or not that happened; and `g_items.c`'s caller reads non-NULL as success, frees the original, adopts the freed substitute, writes to it and calls `gi.linkentity` on it. All three guards free, so all three dmflags reach it, and the broken filter is what steers items into them -- reachable on a Ground Zero server with `randomrespawn 1` and any of DF_NO_SPHERES / DF_NO_NUKES / DF_NO_MINES, which is an ordinary configuration. The repair is one `SubstituteItemAllowed(it)` predicate shared by the count pass and the pick pass, which closes a SECOND defect R-183 did not reach: the two passes disagreed about eligibility -- the count filtered spheres and the pick did not -- harmless only while the test was loop-invariant, and making it depend on the candidate is precisely what would have made the disagreement matter. Spheres are identified by `Pickup_Sphere`, the same test `SpawnItem` uses for that dmflag, which retires the third classname literal as well: R-183 fixed Ground Zero's `item_spehre_defender` spelling, and a function pointer cannot be misspelled at all (R-141's argument, one file over). **`m_move.c`'s blocked-by-tesla chain compares `ent->enemy->classname` with `"telsa"`**, so the branch it guards -- the monster is ALREADY angry at a tesla, leave it alone -- never ran and control reached the final `else`, calling `TargetTesla` and setting `AI_BLOCKED` on every blocked frame. `TargetTesla` skips the enemy switch for the same tesla, but its `AI_MEDIC` bail runs BEFORE that test, so a blocked medic called `cleanupHealTarget` on a tesla once per frame; and when the blocking area belongs to a DIFFERENT tesla the monster switched to it and overwrote `oldenemy` with a tesla, losing the player it was chasing, which is the field `oldenemy` exists to protect. **Not previously recorded here, and the reason is a gap in the method rather than in the sweep**: R-183 resolved classname-shaped literals against the ITEMLIST, and `"telsa"` is compared against an `edict_t`'s classname, so `tools/itemnames.py` is outside it by construction. `m_move2.c` carries the same line and is fixed too although R-MP-5 does not build it -- a reference copy that disagrees with the built one is a trap. **And the third was already fixed here, before the base did it**: Xatrix's `NoAmmoWeaponChange` looked up `FindItem("ionrippergun")`, which matches no pickup_name, and its two Xatrix-added branches lacked the `return` every other branch has, so neither Ionripper nor Phalanx could ever be the out-of-ammo fallback -- R-183 item 5, same reasoning, same result, verified against `eefadf25` rather than assumed. One sweep, one hit and one miss, and the miss is the instructive half. **R-PROV-5a is new and it is R-PROV-5 clause 1 arriving.** `feature/mission-packs` was re-committed a second time -- `09d3c499` -> `eefadf25` -> `92849303`, same three-commit shape, same titles, same author dates -- and `c751d316` and `3ffc4642` both still RESOLVE, so identity is not the problem. The CONTENT moved: the pinned commit has none of the three fixes. So a re-pin now states whether the content moved and names the evidence either way, because that difference is what decides whether anything must be imported. Checked, and the obligation is smaller than the diff: `3ffc4642..eefadf25` touches 37 files under the pack dirs and the two raw import commits differ in 29 more, but the sample is upstream applying repairs this tree had already made independently -- `CS_GENERAL` -> `game.csr.general` (R-62), a literal `pm_time` -> `112 >> PM_TIME_SHIFT`, `strcpy` -> `Q_strlcpy`, `CTFSay_Team_Location` gaining its `size` parameter. Convergence, not a queue. Changed: §0 header, §3.3 `q2pro` pin -> `92849303` (source `eefadf25`), new R-PROV-5a. `doc/reconciliation.md` R-187, and R-183 item 11's declined half is struck there rather than deleted. |
| **Amendment 1.40** | **The second resolver, and the rule that says why there are two.** R-187 closed `"telsa"` and recorded that no audit in the tree could have found it: `itemnames.py` collects classname-shaped literals by SHAPE and resolves them against the ITEMLIST, and both halves are narrower than they look -- a classname belonging to a monster, a projectile or a runtime `->classname =` assignment is in neither space, and a bare `"tesla"` is not even collected. **`tools/classnames.py` asks the other question**: can any code path put this string in an `edict_t.classname`? The universe is three sets and all three are needed -- `g_spawn.c`'s spawn table, the itemlist's `.classname` rows, and every `->classname = "literal"` assignment -- because a check that knew only one would report the other two as findings. Partial comparisons stay partial: `strncmp` and `strstr` resolve against any producible classname that starts with or contains the literal, so `strstr(classname, "item_rune")` is live against `item_rune1..5` rather than reported as dead. Case follows the function. **Validated in both signs against the real tree, not only the selftest**: clean it reports 0 dead comparisons over 146 literal comparisons against 279 producible classnames with 3 exempt, and with `"telsa"` reintroduced at `m_move.c:467` it reports that line and exits 1. Thirteen controls of its own, including the two an audit of this shape most needs -- a `gi.dprintf("%s", ent->classname)` format string is NOT a comparison and must not be reported, and a stale exemption IS a finding. **The three exemptions are the only legitimate case** and both classname resolvers now carry them for the same reason: Ground Zero accepts `weapon_nailgun`, `ammo_nails` and `weapon_heatbeam` from MAPS and renames them in `ED_CallSpawn` before the spawn lookup, so the tree compares against three names it deliberately never assigns. **R-TOOL-6 is new and it is the generalisation rather than the tool**: a literal that names something the game looks up at runtime is resolved against the space that can produce it, and there is one resolver per SPACE -- not one per file and not one per literal shape, which is the mistake that cost twenty-seven years of a dead branch. Three spaces exist and each fails silently in its own way; the requirement tables them with their resolvers and their failure modes. `make check` 33 audits clean. Changed: §0 header, new R-TOOL-6, new `tools/classnames.py` wired into `audit.py`. `doc/reconciliation.md` R-188. |
| **Amendment 1.41** | **A tidy-up sweep of the whole repository, and the code was the half that was clean.** Asked for as docs, tools, comments and annotations. Outside the inherited trees there is no trailing whitespace but 1999's own, no file missing a final newline, every source valid UTF-8, all 33 audits green, and every `tools/` script a document names present -- so this amendment is almost entirely about prose that stopped describing the code. **One amendment caused most of it.** 1.36 made seven rulesets out of five and deleted `tourney` and `match_mode`; six files tracked it and four did not, all four of them what a reader meets first. `README.md`'s ruleset table still listed `tourney`, still said bots reach *four* rulesets rather than six, and still counted *five* server-driven scripts above a block listing six. **`doc/botlib-contract.md`'s libvar table was wrong in four of its five rows**, and that file opens by declaring itself authoritative over Appendix A: `dm` described a ruleset now inside the OSP four's branch, `ctf` had `laserhook` following the cvar after R-179 made it a constant 0, `arena` had `usehook` on the `hook` modifier after R-164 moved it to `arena.cfg`'s `grapple:` key, and the fifth row drove `teamplay` off an `m_mode` R-OSP-12 deleted. Rewritten from `BotRulesetLibVars`, split into the latched half and the per-frame half R-144 created. **`doc/regression.md` carried a green row for a check that does not exist** -- `check-tourney`, deleted in 1.21 exactly as R-VER-26 asked; a ledger whose rule is *an entry with no check is not done* had a green one, which answers the question before anybody asks it. **`meson.build` listed 66 of 111 sources** and had since Phase 2: R-BUILD-3's in-tree build would have produced a library with no CTF, arena, tourney or bots, while the file's header claimed it mirrored the Makefile. **`colosseum/arena.cfg` lost every CR** in `cdb9df2`, a whole-file rewrite -- `.gitattributes` governs what git does to the bytes, not what is handed to it, and the parser takes CR as whitespace, so three documents went on describing a property the file no longer had. Restored. **Numbers: R-TOOL-2 was right and the practice was not.** 23 audits is 19 checks in 33 runs; 20 boot-matrix rows is 28; `playtest.sh` had three figures in three files; the rr-cache was 411+85 and 407+85 and all of them counted directories while saying resolutions (516 resolutions, 492 directories, 486 hashes); `tools/` was 30+25 and is 28+28. The fix is that the scripts already print their own totals and the documents now quote them -- or, where a wrapper cannot count, publish nothing. **Three unreachable things in `tools/`**: `audit.py`'s `if not donors:` nested inside `if donors:`, its dead `auditsave` parser, and `extras.sh`'s `field()` with `\$1` inside double quotes plus two variables nothing read. **And a repair that was recorded and never made** -- `mech.py`'s astyle path, which 1.4 both found and prescribed the fix for, thirty-six amendments ago. **Deleted:** 27 `thisimage` files (rerere working state), `tools/mkvariant.py` and `tools/savemach.py` (replay-era, referenced by nothing, still in the pin). **And this table was broken**: blank lines at three places inside it ended it early, so 1.31-1.40 rendered as raw pipe text; the Status cell above still read *"Draft for review. No code written yet."* Changed: header Status, this table's rendering, R-TOOL-1, R-TOOL-4, R-VER-26 headline, Appendix B. `doc/reconciliation.md` R-189. |
| **Amendment 1.42** | **The one person who certainly is the server was the one person who could not open its menu.** Reported from a listen server: `menu` answered *need rcon password to open the menu* to the host player. The gate is R-BOT-28's, verbatim from 1999, and it is right about what it is for -- a password typed as argument 1 is what a REMOTE operator needs -- but it had no way to say who was asking, and **`ClientCommand` carries no authority at all**: q2pro hands `ge->ClientCommand` the host's `clc_stringcmd` exactly as it hands over a stranger's, because `menu` is not a client command and the console forwards it verbatim. The engine never solved this problem either, which is the part that reframes it: it does not identify the host PLAYER, it distinguishes CONSOLE execution from the network, and rcon exists to lend console authority to somebody who has not got it. The host has it already and is asked for nothing -- and cannot use it here, because a menu needs a client to draw on and `ServerCommand` has none, which is what `sv menu` answers with *only clients can open the menu*. **So the exemption is the two facts the engine does give the library**: `dedicated` 0 (CVAR_NOSET, so it cannot move under a running server) and userinfo `ip` `"loopback"`, which `parse_userinfo` force-sets in the connect packet from `NET_AdrToString` and which only the in-process client ever carries -- the same test `osp-tourney` used for its own host-only menu rows, and a fact this tree already half knew, since `ClientConnect`'s ZBot arm comments that *a local client connects without* a port. **It is latched, not read, and that is the whole of the security argument**: the engine owns `ip` in the connect packet and nowhere else -- `SV_ParseFullUserinfo` replaces the entire string and `SV_ParseDeltaUserinfo` writes any key unfiltered, `SV_UpdateUserinfo` validating only the name and the info-bans -- so `setu ip loopback` would otherwise hand the bot menu to every client on the server, and the menu is bot management: its rows reach `addbot`/`removebot` through `BotServerCommand`, i.e. as CONSOLE commands, so whoever gets it open has them whatever `serveronlybotcmds` says. `pers.listenhost` therefore survives `InitClientPersistant`'s memset unconditionally -- on both arms and under every ruleset, unlike the arena motd beside it -- because `PutClientInServer` calls that on the deathmatch arm of every single spawn, and a wipe would mean the host lost the exemption the first time they spawned. It is written unconditionally rather than set-if-true, because the seat may have been the host's before. The exemption is the MENU's alone: `BotCmdRefused` asks whether the caller is the console, a host player is not, and every other bot command still refuses them (R-BOT-25 unchanged, and measured -- the host's `botpause` comes back *not allowed to pause the bots* in the same run in which their `menu` opens). **And the second half is the default server, which was the worse of the two.** An unset `rcon_password` is the empty string, `menu ""` reaches the library as argc 2 with an EMPTY argv(1) -- q2pro's `Cmd_TokenizeString` registers the argument before it parses the quotes, and a client command is tokenized from the raw text the client forwarded -- so the donor's bare `strcmp` MATCHED: the honest `menu` was refused to everybody, the host included, while two quote marks admitted anybody. Measured on the wire under `ctf` before the fix, and the `listkeys ""` probe reads the empty argument off the wire rather than arguing it from the tokenizer. Unset is now *the menu needs the rcon password, and this server has not set one*, which is a different sentence from *need rcon password* on purpose: one is a configuration an operator can fix and the other is a password they mistyped. `rcon_password none` still disables the gate, 1999's own escape hatch, and it is tested before the empty check because `none` is not empty. Checks: `scenarios/botmenugate` rewritten to two whole servers and **13 of 13** under `arena`, 12 under `ctf` -- both signs of every direction, plus the spoof (a client that pushes `ip=loopback` into its own userinfo is still refused), plus `sv menu` refusing the console, plus the toggle CLOSING with no password because the gate is on opening. **The exemption itself is not observable from a dedicated server at all** -- q2proded has no local client and a libq2 client is a real UDP peer at 127.0.0.1, never NA_LOOPBACK -- so `tools/play.sh` grew **`-L`**, one q2pro process hosting its own map, and `tools/drive/listen-botmenu.cfg` drives it: `Connected to loopback`, the `botpause` control, no refusal, and a screenshot of the menu standing open with no password set and `serveronlybotcmds` at its default. That is the first thing in this tree that needed a listen server to be seen, and it is why R-VER-23's client harness gains a mode rather than a flag. Changed: R-BOT-28, R-BOT-25 note, `doc/commands.md`, `doc/cvars.md`, `doc/regression.md`. `doc/reconciliation.md` R-190. |
| **Amendment 1.43** | **A tourney client was an observer on the scoreboard and a body in the world, and the two halves of the defect hid each other.** Reported from play: `tdm` with `xatrix 1` on `xdm7`, and `dm` on `q2dm1`. The scoreboard said observer; the player collected items, and under `dm` -- where `sync_stat` is 8 and the match is live from the first frame -- shot and killed while `g_combat.c`'s `entered != ENTERED_ENTERED` test made them unkillable in return. **The donor's PutClientInServer has TWO arms and the merge took the second one unconditionally.** A tourney client arrives as an observer -- `OSP_clientBeginPre` sets `osp_entered` to 2 -- and the donor places it `MOVETYPE_NOCLIP`, `SOLID_NOT`, `SVF_NOCLIENT`, with no view weapon and no KillBox; the entered arm's three bookkeeping writes were kept, its `svflags &= ~SVF_NOCLIENT` and its two track-state clears were not, and the observer arm was gone entirely. That is R-RA-4 row 15's defect one ruleset over, and the comment beside that fix names the witness this one was found with: **a headless client reading back its own pmove type**, which is the one question about observing a mod's own HUD cannot fake. `ops_tourney` says "SelectSpawnPoint stays dm's: tourney places players through OSP_startObserve()", which is true of the `observe` COMMAND and was never true of a placement -- that function prints, scores and re-sorts, so it is not what a spawn may call. **The twin is `resp.osp_r240`, and it is why neither half could be found alone.** It is the donor's "this client has a body" flag, 2 after placement and 0 for an observer, read by `ChangeWeapon`'s gunindex, the KillBox and the autocam's candidate test -- and the merge kept the field and all three readers and dropped **every write of 2**, which is R-155's shape exactly (`pers.showmotd` had a reader, a clear and no write). So the autocam could never find a subject, and the two `ClientThink` readers that complete the mechanism were missing too. **That mechanism is how a tourney client gets a body at all, and it is not obvious**: none of the six entering paths places anybody -- `join`, the team join, the two menus, the 1v1 queue, a bot's join and a recovered seat all set `osp_entered` to ENTERED_ENTERED and clear `osp_r240` -- and the next `ClientThink` notices the pair and calls `respawn`, freezing the client (`PM_FREEZE`) in between. One trigger serves all six, and a **seventh** case comes free: a placement that is refused leaves the flag 0, so the retry is the same line. With the flag never set, a trigger on its own would have respawned every client on every frame: both halves or neither, which is why this had to be read out of the donor rather than patched at the symptom. **Two more sites of the same state.** `STAT_FRAGS` carried the **-100 sentinel** to an arriving client's HUD -- `OSP_clientBegunPost` sets that score on arrival and `OSP_startObserve` on every later exit, and the donor draws a 0 for a client that has not entered -- and `Cmd_Kill_f` let an observer suicide, where the donor's own combined condition refuses it. **And `ENTERED_QUEUED 3` was an invention**: the donor's states are BITS compared for equality -- 1 playing, 2 observing, 4 chasecam, 8 in-eyes, 16 autocam -- and no donor site writes or reads a 3. The header said "four states" and named three plus a fabrication. **One design was considered and declined.** R-CTF-5's `G_IsObserver()` is "one predicate, four answers" and has no tourney answer, which is the actual reason `Cmd_Kill_f` was wrong -- but adding one would move two of its other call sites AWAY from the donor: `ClientBeginDeathmatch` announces an arriving observer (the donor's `OSP_playerAnnounce` is unconditional there) and `p_view.c` would route it to `G_SetSpectatorStats`, which the donor does not have at all, having deleted baseq2's spectator system in favour of this very field. So the two sites that need the question ask it by name, and the reasoning is recorded rather than the predicate widened. **And one divergence is recorded and NOT fixed**: the donor's `SelectSpawnPoint` returns false when a player is within 60 units of the chosen spot and freezes the spawner instead of telefragging them -- which is safe precisely because of the retry restored here -- while `G_SelectSpawnPoint` is a `void` ops row and KillBoxes. Making it a predicate touches every ruleset's spawn hook, so it is R-191's own entry rather than this increment's. Checks: new `scenarios/ospenter`, **24 checks over the four OSP rulesets, 8 failing before and 0 after**, both signs of every phase -- PM_SPECTATOR and STAT_FRAGS 0 before the join, PM_NORMAL after it, and the entering broadcast absent before and present after. **The scenario was wrong twice and the game was right both times**: `join 1` is not a team, because `OSP_teamjoin_cmd` matches its argument against the team's netname, and `duel` announces NOTHING, because `OSP_addTeamMember` guards that broadcast with `m_mode == 2` -- so the silent ruleset now asserts the silence. `tools/playtest.sh` 234 of 234 (the 1.42 baseline, unmoved), `make check` 33 audits clean, `-Werror` clean under gcc, clang and `API=old`. Changed: R-OSP-1, R-OSP-4 note. `doc/reconciliation.md` R-191. |
| **Amendment 1.44** | **The refusal R-191 left recorded, and then a sweep for its whole class -- which found two more, cleared four, and reported one finding that was the SCAN's mistake.** R-191 restored the retry and recorded the thing that needs it: the donor's `SelectSpawnPoint` returns false when a player is within 60 units of the chosen spot, and `PutClientInServer` then leaves the client frozen and bodiless instead of telefragging whoever is standing there. **`G_SelectSpawnPoint` is a predicate now** -- the ops row, both other implementations (`ctf_SelectSpawnPoint`, `DBall_SelectSpawnPoint`) and `dm_game_rt`'s row with it -- and the refusal lives in `OSP_spawnRefused` on tourney's side of the seam, because the arithmetic is tourney's. **Two exclusions the shared `PlayersRangeFromSpot` does not make are load-bearing rather than tidy**, and both are the donor's: the client being placed is not measured against ITSELF -- its body is still where it was observing from, and a client parked near the spot it is about to be given would refuse its own spawn forever, since the retry finds the same spot and the same body -- and an OBSERVER is not a player, or one client hovering over a spawn point blocks it for everybody, which is the finding R-RA-4 already records for arena's farthest-spawn one ruleset over. Measured on `q2dm1`'s ten spawn points with eleven clients, which cannot walk and therefore park on them: **4 telefrag obituaries and nobody refused before, 1 frozen client and 0 telefrags after**, and the frozen one placed within four seconds of four clients leaving. **Then the sweep, and it is a TOOL rather than a reading**, because the class is defined by absence: the merge keeps a field and its readers and drops the write, and nothing warns -- `if (x != 2)` is a valid comparison that is simply always true. `tools/deadvalue.py` is R-TOOL-6's fourth resolver and its space is **the set of values a field can hold**, asked three ways: in-tree (a constant compared for equality that no write produces), against each DONOR (a `(field, value)` write the donor makes and this tree cannot -- the sharper question, because the donor is the authority on what a field is supposed to hold), and one dispatch question (a function-pointer row assigned through a named object and never called through one). Zero is never a finding: a memset, an `InitClientResp` and a designated initialiser all produce it. **Eight donor trees compared -- five donors and three q2pro mission packs -- and after triage it is two defects, four cleared and one open.** `osp_r24c = 8` is the PLAYER CARD, one of `OSP_ScoreboardMessage`'s five pages, with a `case 8:` arm and no writer: the donor's toggle is in `Cmd_InvUse_f` rather than in `score` -- pressing USE on an open scoreboard is how OSP asks for your own card -- and porting it brought the two guards around it, an intermission arm and **another `entered` gate**, since an observer carries a blaster and could use it. `DMGame.SelectSpawnPoint` looked like the second and **is the scan's own error, kept as a note because it is instructive**: a grep found the row assigned in `g_newdm.c` and no caller anywhere, and the tool -- which strips comments before it reads -- reported nothing, because Ground Zero's whole `case RDM_DEATHBALL` is commented out here and byte-identically commented out in q2pro. The row has never been installed; a caller for it would be unreachable code, and the first draft of this amendment added one. **Four cleared with the reason written down**: `isbot = 0`, `osp_r2a8 = 0` and `osp_r2bc = 0` are the memset's zero, which is why zero is exempt by construction rather than by a list; `entered = true` is RA2's and is baseq2's bool rather than tourney's enum, whose only RA2 reader is a reconnect arm this tree does not have (R-58). **One is open and says so in the exemption list**: `osp_entered = 8` is in-eyes chase mode, and the write alone would change nothing -- the donor's entire chasecam input branch is unported, zoom and free-look and the ATTACK cycle, and `osp_t018`, the field the free-look writes, is read nowhere here. It is a feature to port, not a line to restore. `osp_r2bc` is recorded beside it: written here, read nowhere, and the donor's reader is `G_SetStats`' early-out, whose sense in the reconstruction would blank every PLAYER's HUD -- so it is recorded rather than guessed at. Controls: six, including the two an audit of this shape most needs -- a mutant must use a FRESH field name, because a name the clean tree also writes resolves against that and the control passes while testing nothing; and an exemption for a donor that was not compared must not be reported stale, or `make check`, which passes one donor, reports four false stale entries on every run. Checks: `scenarios/ospenter` grew a crowded phase and is **27 of 27** over the four OSP rulesets, `tools/playtest.sh` 234 of 234, `make check` **35** audits clean, `-Werror` clean under gcc, clang and `API=old`. Changed: R-OSP-1, R-TOOL-6 (a fourth space), new `tools/deadvalue.py` wired into `audit.py`. `doc/reconciliation.md` R-192. |
| **Amendment 1.45** | **"Why was the in-eyes camera not ported, and what else is missing?" -- the answer to the first is that nothing decided it, and the answer to the second took two sweeps and a predicate.** R-OSP-1 has named *the observer and camera system* since 1.0 and no document, comment or reconciliation entry ever recorded a decision to leave part of it out; R-192's exemption list was the first time it was written down at all. What was here: the entry commands, `camera_depth` and `camera_pitch` registered and clamped, `osp_t018` and `wav_file` carried. What was not: **the input and the consumer**. So the chase camera sat at baseq2's fixed 30 units with no controls, and `entered == 8` -- in-eyes -- was a state two menu rows tested and nothing could reach. **The camera has four controls and all four are now spent in `UpdateChaseCam`**: forward/back zooms between `camera_depth` and the eye, strafe free-looks in four-degree steps, ATTACK cycles chasecam -> in-eyes -> out, jump cycles the target. The two modes differ in three ways the donor writes out and baseq2 has no room for -- the pitch allowed (56 against 1), the camera twelve units IN FRONT of the eye rather than behind the head, and the 30-unit floor lift that in-eyes does not get -- and the free-look reaches the VIEW angles as well as the placement, which is the donor's second `vangles` copy and the difference between moving the camera and moving the picture. **Two things the donor does that are NOT carried, each for its own reason**: its `avelocity` write, which its own comment calls a frame-to-frame delta and which nothing in the donor reads -- porting it would add exactly the dead state R-192 exists to find -- and its float punned into the int `osp_t018`, which is always an integral degree count because the donor takes `% 360` of it as an int, so it is kept as the int it is declared to be. **R-CTF-5 gains its fifth answer, declined twice before it was taken.** `G_IsObserver()` had no tourney spelling, which is why R-191 answered `Cmd_Kill_f` by hand; by this increment it was six sites, four of them the chase camera's own target search -- so without it the camera would have offered an OBSERVER as something to watch. The two callers that then diverge from the donor carry an arm each: `ClientBeginDeathmatch` announces an arriving observer and `p_view.c` keeps `G_SetStats`, because the donor deleted baseq2's spectator system and has no `G_SetSpectatorStats` at all. And tourney's observer INPUT replaces baseq2's rather than joining it: a free-flying observer gets a menu on the first press and the autocam after it, so baseq2's chase toggle and jump-cycle are excluded under the OSP four or two systems would read one key. **Then the second sweep, for everything else of this shape, and it is two questions.** Which cvars are registered and never read, and which functions does nothing reach? The first found four features: **`numgibs`** (the gib count, default 4 -- which is why nothing looked wrong), **`match_endmusic`** with `wav_file`'s five tunes and the team win/lose split, and **`nextlevel_click`/`nextlevel_lazy`**, the two intermission timers, one of which the map loop even WROTE and neither of which anything read: an intermission ended on the first press after baseq2's five seconds, or never. Porting the music brought the whole per-client end-of-level block it lives in -- the demo stop with its screenshot, the automatic accuracy page, the menu close, the recover latch -- and the board a player who died into the intermission is owed, whose `osp_r2dc == 2` had no reader and whose write carried a comment naming a function that reads the value 1. **Ten cvars remain unread and all ten are correct**: six the donor never reads either (they are published for clients or the engine), `bots_botfile` read by name through `BotFile()`, and the two power armour settings read through a helper's local alias -- which is why the sweep is a lens and not a verdict. **The function sweep says the tree is clean, and it took three passes to say it**: 390 findings when it counted only calls (a monster's frame table references by NAME), 972 when its prototype filter swallowed every plain call (`[\\w \\t*]+` matches leading whitespace), 75 when `return foo();` still looked like a declaration, and 72 once it did not -- every one of which is the donors' own dead code or an inherited q2pro utility. `UpdateEyeCamera` and `Log_Write` are uncalled **in the donors too**, and the DBall set is behind the `case RDM_DEATHBALL` q2pro comments out. Checks: new `scenarios/ospcamera`, **11 of 11 under `dm` and 11 under `tdm`** -- the camera is measured off the wire, because the observer's own edict origin IS the camera: 34 units back at `camera_depth` 20, 34 -> 29 -> 34 through a zoom in and out, a free-look yaw walking 0 -> 48 degrees away from the target's own, 26 units at the eye in-eyes with the offset dropped, and PM_SPECTATOR again on the way out. A row that wanted the shipped `camera_depth` 60 would have been asserting the MAP: the placement is traced, so at 100 the camera stands at xy 38 against a wall and fifteen units of zoom change nothing. `scenarios/ospenter` 15 of 15, `tools/playtest.sh` **234 of 234** with the predicate widened, `make check` 35 audits clean, `-Werror` clean under gcc, clang and `API=old`. **One thing is still open and unchanged**: `osp_r2bc`, whose reader in the reconstruction would blank every player's HUD (R-192). Changed: R-OSP-1, R-CTF-5, R-OSP-2's cvar claim. `doc/reconciliation.md` R-193. |
| **Amendment 1.46** | **Five reports from one `tdm` play test, and the first thing to establish was WHICH BUILD they came from.** Two of the five -- an observer that could only look left and right, and an observer that had weapons and could fire -- did not reproduce on HEAD, and the reason is that 1.45 had already fixed them four hours before the report: both are R-193 teaching `G_IsObserver()` to answer for the OSP four, and neither was recorded as a symptom because both were found from the predicate rather than from the game. The A/B is the evidence and the method: the same scenario against `a3eea8d` reads `pm_flags` **0x20 `PMF_TIME_TELEPORT`** on a `PM_SPECTATOR` observer with the view pinned at 0.0 however the mouse moves, and a gunframe high-water of 56; against HEAD, 0x00, full pitch, and 0. **`PMF_TIME_TELEPORT` does not expire on a client that never runs the countdown** -- `Pmove()` clears it below the `PM_SPECTATOR` early-out and `PM_ClampAngles()` answers it above, by pinning PITCH and ROLL and letting only YAW follow -- so a spectator handed it keeps it, on the client as well as the server. **A play-test report is evidence about a build**, and this tree moved three amendments in the twelve hours between the sitting and the report; reproducing on HEAD reproduced neither. **What was real: the match start left the map dirty.** `OSP_checkSync` kills everybody into a fresh spawn and then sweeps the world in the same frame -- gibs freed, every `bodyque` unlinked and hidden -- and the sweep was ported while the thing it works on was not: the donor's `Cmd_Kill_f` ends `ent->deadflag = DEAD_DEAD; respawn (ent);`, and a corpse exists only because `respawn()` -> `CopyToBodyQue()` made one. Without it the kills made nothing for the sweep to find, every player lay dead until they pressed fire, and the body queued THEN -- after the sweep -- stayed lit in a live match. Measured: both players **PM_DEAD** with **`STAT_HEALTH` 0** at "Match has started!" and 123 non-client edicts against a 120 baseline; after, PM_NORMAL, 100, and 120. Its two dropped companions are not cosmetic either -- `CopyToBodyQue` copies `s.effects` and `s.renderfx` to the corpse, so a quad shell or a rune glow would go on glowing on the floor. **The same class, narrower**: `ClientBeginServerFrame` asked `!G_IsObserver(ent)` where the donor asks `resp.osp_r240 == 2`, and the two disagree for exactly one state -- a player whose placement was REFUSED (R-191, R-192), which is `MOVETYPE_NOCLIP`, `SOLID_NOT`, `SVF_NOCLIENT`, `PM_FREEZE` and keeps its view weapon, so an observer in every visible respect ran its weaponthink. **What was not a defect: the countdown clock.** Read off the wire for a player AND an observer -- slot 17 -> configstring 1569, `"0:14"` -> `"0:11"`, `Time` panel in the 759-byte bar the client received. There is no clock during WARMUP and that is the donor's: both arms of `OSP_updateClock` need `sync_stat > 0`, and warmup draws `WARMUP` in the team cells instead. **What was a config value: friendly fire.** `team_hurtteam` 1 -> 0 in `configs/tdm.cfg`, and checked against the world rather than the switch, because a per-team seed can arrive and be read by nothing -- which is the shape the `runes` modifier had. New `scenarios/ospff` fires a live rocket straight down between two teammates on two whole servers: at 0 the teammate goes 100 -> 100 while the shooter takes 55 of its own splash, at 1 the same shot takes them to 17. **The shooter's own health is the receipt that the rocket went off**, without which a missed shot and a gated shot are the same observation. `doc/reconciliation.md` R-194 carries all five. **Checks**: new `ospwarmup` 17 of 17 and `ospff` 4 of 4, `ospwarmup` 6 failures on `a3eea8d`, new `ospobsmodes` sweeping every observer mode, `tools/playtest.sh` 234 of 234, 35 audits clean. |
| **Amendment 1.47** | **A function-by-function diff of all five donors against this tree, and the seven things it found -- one of which is a sentence in this document rather than a line of code.** R-CORE-10 states the merge load per FILE in diff lines and nothing had ever asked the per-DEFINITION question uniformly: *for each definition a donor touched, is that donor's behaviour here?* **New `tools/fnsweep.py` (R-VER-35)** takes R-PROV-3's five diffs and compares three texts per definition -- spine, donor, tree -- classifying each pair `as_donor` / `as_spine` / `merged` / `absent` / `donor_deleted`, and `doc/donor-fdiff.md` is the read-through with the tool authoritative over every table in it. **The shape of the answer first**: 3,197 definitions touched, 2,860 in files this tree carries; Rogue 388 of 555 shared definitions byte-identical at 0.93 median similarity and Xatrix 132 of 222 at 0.88 with **zero** absent identifiers, against CTF's 9 of 99 and tourney's 16 of 233 -- which is not a worse merge but R-MODE-5, since a gated hunk cannot be textually identical. Of **4,080** donor-added lines not at their own site, 1,165 are elsewhere in the tree verbatim and 1,381 name an identifier this tree does not have -- and **all 1,381 attribute to a recorded decision**: 490 to R-SEC-1's libc replacements, 222 to `match_mode`'s deletion, 168 to the ngLog stack `osp_stats.c` replaced, down to nine. **The nine are the findings and only two of the seven findings are inside the check's own space**, which is written into the tool rather than left to a reader: five turn on identifiers that are all present, so what is missing is a condition, a reader, an argument or a call site, and `NOT_MECHANISED` names them with the resolver each shape would need. **R-195.1 IS WITHDRAWN IN 1.48 and the sentence that follows was wrong when it was written** -- the gate has been in this tree since 1.31 (R-151), R-149's "Open, deliberately" was stale, and the sweep believed it. What survives is the half about the donor. As published: *"The one worth reading is R-195.1, because the correction is to a claim.* RA2 adds `ent->takedamage && arenas[…].state == ASTATE_FIGHTING` at FOUR `p_weapon.c` sites -- `Weapon_Generic`'s fire arm, `Weapon_HyperBlaster_Fire`, `Machinegun_Fire` and `Chaingun_Fire`'s gunframe-21 loop -- at the pin `d20e1ce` as well as the bundle tip, and this tree's four counterparts are the spine's. R-149 closes *"A person can still pre-fire an RA2 countdown here, as they can in `rocketarena2` itself"* and, two paragraphs above, says the right fix is *"the `buttons & BUTTON_ATTACK` test inside each `weaponthink` … or one gate in `Weapon_Generic`'s fire arm"* and wants an increment for it: **the donor has exactly that design at exactly those places**, so it is a port and not an invention, and R-143's "this is 1999's" is true of `p_client.c` and false of `p_weapon.c`.* **R-195.2 is a fifth space for R-TOOL-6**: RA2's chat-spam punishment (six lines in two seconds -> a broadcast and `stuffcmd(ent, "disconnect")`) is gone while `gclient_t.spamcount`/`.spamtime` are declared AND savegame-persisted and read by nothing -- a field with no reader and no writer, which `deadvalue.py` cannot see because its space is the values a field can hold and this one holds none. **R-195.3/.6 are one question in three places and only the call delta asks it**: `stats_logchat` reaches `talkto` and not `Cmd_Say_f`, `TossClientWeapon` logs neither the quad drop nor its expiry and never clears the entnum `p_view.c` reads, and `ShutdownGame`'s accuracy dump for a match still running has no counterpart -- in each case the function exists and has callers, and the missing thing is a call at a site. **R-195.4 and .7 are what `--check` fails on**: tourney's second ZBot heuristic with the `osp_r008` that fed it, and three one-line deltas (`SVF_PROJECTILE` on CTF's blaster bolts, the `MOD_GRAPPLE` obituary text where two donors claim one MOD, and Ground Zero's `FindSubstituteItem` typo that this tree spells correctly). **R-195.5 extends R-191's own argument by one call**: its two exclusions reach the refusal and not the selection, so `DF_SPAWN_FARTHEST` under the OSP four still measures against the body being placed and against observers. **The negative result is the larger half and is why the report exists**: 20 of RA2's 27 added files and 15 of tourney's 47 are correctly not carried -- including `g_monsters.c`, 161 lines holding 25 monster spawn stubs that would have compiled, linked and emptied every map -- eight cases where the sweep said "missing" and the donor's own copy cannot run, all 27 `OSP_Stats_*` entry points wired, and all four OSP client handshakes intact, which is this sweep confirming 1.30's `stuffcmd` audit. **The sweep re-derived eleven ledger claims independently and contradicted one** (R-KEY-1, R-40 including the exact `SVF_PROJECTILE` line that row names, R-CORE-8 with R-64's sixth rule, R-91, R-114, R-CORE-3, R-OSP-7a, R-OSP-12, R-MENU-1, R-191, R-77), which is the calibration worth having: on this tree the answer is seven findings, four of them single call sites. Controls: **eight**, two of them about the check's own blind spots, and the extractor is asserted against the shape that broke it -- a signature ending in a trailing `//PGM` under a commented-out prototype, which a name-by-regex extractor merges into its predecessor. Checks: `make check` **37** audits clean, `fnsweep.py --check` under 20 s over the five donors, and `-Werror` unchanged under gcc, clang and `API=old` **because no source file changed by this increment** -- the diff is one new tool, one new document, `audit.py`'s two rows and four documentation corrections. Also corrected: `doc/reconciliation.md`'s status line said 182 findings for seven increments (190, quoted with its counter), and `README.md`'s two audit figures are gone rather than restated, per its own precedent. Changed: new R-VER-35, new `tools/fnsweep.py` wired into `audit.py`, new `doc/donor-fdiff.md`. `doc/reconciliation.md` R-195. |
| **Amendment 1.48** | **The port of R-195.1, which turned out to be a withdrawal -- and the refusal is a measurement rather than an opinion.** Asked to port RA2's four `p_weapon.c` firing gates, and the first thing to establish was whether this tree needs them. **It does not: R-151 closed that question inside spec 1.33 itself** (commit `1aa6429`), at the latch in `ClientLagThink` rather than at four fire arms, with `scenarios/ra2prefire` measuring `STAT_AMMO` 50 -> 46 through a countdown before and 50 -> 50 after. 1.47's sweep was right that the donor's four gate lines are absent here and wrong about what that meant, and **what made it believable is that TWO documents still said it was open**: `doc/reconciliation.md` R-149 ends "Open, deliberately. A person can still pre-fire an RA2 countdown here" two entries before the one that closed it, and the amendment row for 1.33 -- the very version the gate landed in -- ends "Left open deliberately". R-151 records the closure in full, with a measurement and a scenario, and **no amendment row has ever named it**, so the table and the ledger disagreed for fifteen versions. A ledger entry that records something as open has to be closed where it is recorded, or the next sweep reads it as the state of the code; that is the lesson and it cost a false finding. **What survives is the half about the donor, and three texts had it backwards**: RA2 gates neither `Think_Weapon` call site (true, and what R-151 needed) but does gate four fire arms (`Weapon_Generic` 417, `Weapon_HyperBlaster_Fire` 818, `Machinegun_Fire` 897, `Chaingun_Fire` 997, at `d20e1ce` and at the bundle tip), so "a person can pre-fire a countdown in 1999's mod too" does not follow -- and the design R-151 chose is the donor's own answer reached independently rather than a divergence from it. Corrected in place in R-149, R-151, the 1.33 amendment row and `RA_HoldFire()`'s docstring. **And the port is refused with the enumeration that decides it**: asked of this MERGED tree rather than of RA2's weapon set, "where does a press become a shot" has **nine** answers, not four -- the generic fire arm, the hyperblaster, the machinegun and the chaingun loop that RA2 gates, plus `Throw_Generic` (the hand grenade and Ground Zero's tesla), the Reckoning Trap, the chainfist, the ETF rifle and the plasma beam, which it does not. **All five of those are reachable under `arena`**: three through R-182's grant bits in `give_weapons`' second pass and two through the ammunition that is also the weapon (`ammo_grenades` in every arena, `ammo_trap`/`ammo_tesla` with their layer). Porting the donor's four would have stopped the weapons RA2 noticed and left a fighter throwing hand grenades, dropping Traps, laying teslas, sawing with a chainfist and holding a plasma beam through the whole countdown -- one clear at the latch covers all nine, and that argument was unavailable while three documents said the donor had no gate to compare with. **Nine and not eleven**: a grep for `BUTTON_ATTACK` in `p_weapon.c` returns eleven, and two of them are inside baseq2's own `Weapon_Grenade`, which sits in a BLOCK COMMENT because Ground Zero replaced that weaponthink -- the third place this tree has had to record that a census which reads comments counts behaviour that does not exist. **One real divergence is recorded rather than left latent**: the donor QUEUES the press, because its clear of `latched_buttons` sits inside the gated arm, so a tap during a countdown fires on the bell; this tree drops it, and holding through the bell fires either way. The queue is refused because it cannot be had at the latch, only per site, and because a queued press is a shot at the bell with no reaction time in it. Checks: `make check` 37 audits clean and the native gcc build `-Werror` clean. **The other build legs are deliberately not re-run and this row says so rather than implying them**: the only change under `src/` is prose inside a block comment, and the file it is in compiled, so clang and `API=old` have nothing to disagree about -- a claim of three clean legs would be three assertions and one measurement. Changed: `src/arena/arena.c`'s `RA_HoldFire()` docstring, the stale claims in R-149, R-151 and the 1.33 amendment row, `doc/donor-fdiff.md` §3, §3.1, §5 and §6, `tools/fnsweep.py`'s `NOT_MECHANISED` note. `doc/reconciliation.md` R-195.1. |
| **Amendment 1.49** | **Five of R-195's findings ported, and the port found two defects that no sweep could have named.** Asked to close everything `doc/donor-fdiff.md` §3 still listed, in priority order. **R-195.3 and R-195.6 are one shape in three places and close together**: `OSP_Stats_Chat` is called from `Cmd_Say_f` now, at the donor's own position (after the 150-byte truncation, before the newline) -- ONE call and not two, because the donor routes `say_team` under TeamPlay to `OSP_sayteam_cmd` before `Cmd_Say_f` can see it and does not log there either, so `tdm`'s `say_team` stays out of the log **as the donor's gap rather than ours**; `TossClientWeapon` gained the quad drop and expiry events and the `resp.osp_r200` clear; `ShutdownGame` gained `if (!level.intermission_framenum) OSP_Stats_AccuracyAll()` between `sl_GameEnd` and the end event. **One token of the expiry arm is deliberately not the donor's**: `osp_r200` is set by the INVULNERABILITY pickup as well as the quad -- in the donor too -- so the donor's guard has three reachable cases and only one is the event it names, double-logging the second and writing a "Quad" expiry for an invulnerability in the third. `p_view.c`'s own reader tests `quad_framenum &&` before the comparison for exactly that reason and this arm now does too; nothing correct is lost, because a still-running quad fails `< level.framenum` either way (§7 rule 3). **R-195.5 threads the client being placed through three signatures** -- `PlayersRangeFromSpot(spot, ent)` and both selectors, `NULL` at the one caller with no such client -- and the donor's two exclusions are enabled by `ent != NULL && G_IsOspRuleset()`, which is not tidiness: `osp_entered` is maintained only by the OSP four, so applying it elsewhere would exclude every player on the server and hand every spot the same 9999999. **R-195.7's two text deltas are decided, and one takes neither donor's line**: the `MOD_GRAPPLE` obituary is per ruleset (tourney's "was hooked to death by", which its own log has said all along), and `SVF_PROJECTILE` on CTF's blaster bolts is set IN ADDITION to `SVF_DEADMONSTER` rather than instead of it, because q2pro's `SVF_PROJECTILE` arm is inside `if (svs.csr.extended)` and its `SVF_DEADMONSTER` arm is not -- the donor's line verbatim would leave a bolt on a non-extended server flagged with nothing at all, solid to `MASK_PLAYERSOLID`. R-195.2 and R-195.4 stay open as **decisions**: a policy call about a game library disconnecting a client, and a heuristic weak enough that declining it is defensible. **THE STALE-EXEMPTION RULE FIRED ON ITS FIRST REAL OCCASION** and that is worth more than the two lines it guarded: porting R-195.7's two made `fnsweep.py`'s `SVF_PROJECTILE` and `hooked` exemptions match no donor line, and the next `make` went red naming both -- before any test ran, from a rule written for a hazard that had never happened. A fix and its exemption have to retire together and the build now enforces it. **And two defects the sweep is blind to by construction, both reported by playing the game.** **The match started on a chorus of screams**: the donor puts `if (sync_stat != 2)` in front of BOTH of `player_die`'s death sounds, because OSP begins a match by killing every player into a fresh spawn, and this tree carried neither guard -- an ADDED CONDITION around a call that was already present, which no line-level rule and no call delta can see (`doc/donor-fdiff.md` §6 item 1, first specimen). **And the tdm match clock was gone from the bell onward**: `OSP_restartStats` points tourney's HUD panels at their configstrings and this tree called it forty-five lines BEFORE `memset(&ent->client->ps, 0, sizeof(client->ps))`, so every placement wrote the indices and wiped them. The donor's own call is the last statement before "force the current weapon up". Warmup looked right because `OSP_CheckReady` sets the same stat at the countdown and nothing respawns anybody between then and the bell. That one is §6 item 4 -- "two calls in the wrong order are two calls present" -- arriving as a call in the wrong PLACE, which is a fifth instrument nobody has built. **Evidence: two new scenarios, both in both signs.** `scenarios/ospchatlog` (20 rows, 0 failed) -- a `say` writes exactly one `chat` event AND a `stats_logchat 0` control writes none while logging five events of other kinds; dying with an active quad writes one `item_drop "Quad"` whose `from` is the entity the `item_pickup` named AND a bare death writes none; `map` typed mid-match on a `dm` server with four brain-driven bots writes four `accuracy` events with real tables before `shutdown{reason:"map"}`. Bots are not decoration there: `OSP_Stats_Accuracy` writes nothing for an empty per-weapon table and a libq2 client cannot fire, so the first version of that phase passed vacuously. `scenarios/ospclock` reads the clock at all four layers separately -- the composed bar, the client's copy of it, the stat, the configstring -- so a failure names the layer; it reported `stats[17]=0` and everything else correct, which is what identified the memset. **Three harness defects, each found by the row it made vacuous**: libq2 parsed `svc_sound` and never dispatched it, `ParseSound` defined `SoundIndex16` and did not honour it (one byte of desync for the rest of the packet on any sound index above 255), and `KillAndConfirm` exists because `Cmd_Kill_f` refuses a suicide within five seconds of a respawn AND PRINTS NOTHING WHEN IT REFUSES -- two scenarios asked what dying does, got a refusal, saw nothing and reported a pass. **The silence rows CALIBRATE THEMSELVES, and the reason is that they nearly lied.** "No death scream at the match start" is satisfied by a working guard and by a client that cannot hear a death at all, which are opposite facts -- and for three runs it was the second, because `KillAndConfirm` matched the wrong obituary verb and every suicide it thought it had made was refused. So the ordinary mid-match death is the calibration and the silence row hangs off it: same function, same client, same channel, `sync_stat` 4 instead of 2. Measured: **two CHAN_VOICE sounds from the player after the suicide and zero across the countdown and the bell**. When the calibration fails both rows skip and say why, because a row that cannot fail is worse than no row. Checks: `make check` 37 audits clean, `fnsweep.py --check` clean with two exemptions instead of four and `NOT_MECHANISED` down to R-195.1 and R-195.2, native gcc `-Werror` clean. Changed: `src/g_cmds.c`, `src/g_main.c`, `src/g_weapon.c`, `src/p_client.c`, `src/p_view.c`, `src/g_local.h`, `src/ctf/g_ctf.c`, `src/rogue/dm_ball.c`, `src/rogue/dm_tag.c`, `tools/fnsweep.py`, `doc/donor-fdiff.md` §3. `doc/reconciliation.md` R-196. |

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
| D6 | A default `colosseum/` gamedir config set: `bots.cfg`, per-ruleset configs, ~~`default.cfg`~~ **`server.cfg`** — *corrected in 1.25: the name `default.cfg` is not available in a Quake II gamedir.* It is **id's own file**, shipped inside `pak0.pak`, and it holds all 68 key bindings a client starts with. q2pro satisfies `exec default.cfg` from a pak as readily as from a loose file, and within one gamedir the pak wins — but in the layout R-BUILD-8 documents, where the library and its configs live under `homedir` and the paks under `basedir`, homedir is searched first and has no pak, so a file of ours by that name answers before id's ever does and a joining client gets **zero bindings**. Found the first time a person ran `tools/watch.sh` on a display. **`configs/arena.cfg` asked for `botarena 0` until 1.28**, a cvar name nothing in this tree or in any donor has ever read, with a comment describing behaviour that never existed; the knob is 1999's `arena` and the file says so now (R-131). **And `arena.cfg` is RA2's own file as of 1.27** rather than the 69-line example that shipped from 1.24: an example carries none of RA2's 28 per-map blocks and none of its **35 pickup-arena designations**, so on RA2's own maps the team menu offered nothing but *Start New Team* -- which is what a play test found (R-129) |
| D7 | A regression checklist keyed to §10 |
| D8 | `tools/` — the replay and audit toolchain of R-TOOL-1, re-pointed at this tree |
| D9 | `doc/replay-coverage.md` — the per-donor ledger of R-CORE-12 |

---

## 2. Terminology

| Term | Meaning |
|---|---|
| **Ruleset** | The primary game mode. Exactly one is active. `dm`, `dmpro`, `tdm`, `duel`, `ctf`, `arena`, `sp` |
| **OSP family** | The four rulesets that run OSP Tourney DM's code path — `dm`, `dmpro`, `tdm`, `duel`. `G_IsOspRuleset()` is the predicate |
| **Base ops** | Q2PRO's baseq2 implementations, inherited by any ruleset that fills no row (R-MODE-6). Not selectable |
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
| `q2pro` | `b1f2c18f` (`r1504-2231-gb1f2c18f`) | `c751d316` → **`92849303`** in 1.39 | `feature/mission-packs`, same branch, same three-commit shape: `bee7f7f0` *Add ctf, xatrix and rogue game libraries* → `3ffc4642` *Mission packs: portability and latent-bug fixes* → `c751d316` *README*. `src/{game,ctf,xatrix,rogue}` reproduce 72 / 31 / 79 / 92 files and 42,276 / 27,178 / 51,258 / 67,568 lines. **Re-pinned again in 1.39, and this time the CONTENT moved** — see R-PROV-5a |
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
* **R-PROV-5a.** *New in 1.39, and it is clause 1 of R-PROV-5 arriving.* The
  `q2pro` pin moved a second time, and **the content moved with it** — which is
  the case clause 1 anticipated and the first case where "where a SHA still
  resolves it is left alone" is not enough. `feature/mission-packs` was
  re-committed as `09d3c499` → `eefadf25` → `92849303`, the same three-commit
  shape with the same titles and the same author dates; `c751d316` and
  `3ffc4642` both still RESOLVE, so identity is not the problem. What changed is
  that `eefadf25` carries three latent-bug fixes to the imported packs that
  `3ffc4642` does not:

  | | `3ffc4642` (1.4 pin) | `eefadf25` (1.39 pin) |
  |---|---|---|
  | `"telsa"` in `src/rogue/m_move.c` | present | fixed |
  | `FindItem("ionrippergun")` | present | fixed |
  | `SubstituteItemAllowed` | absent | present |

  So the pin is `92849303` (tip, README-only) with `eefadf25` as the commit the
  source comes from, and the rule this adds is: **a re-pin states whether the
  content moved, and names the evidence either way.** An identity-only re-pin
  says so; a content re-pin lists what changed, because the difference decides
  whether anything has to be imported. §7 rule 1 makes the base's answer the
  tree's answer, so a content move is a merge obligation and an identity move is
  bookkeeping.

  *Checked, and the obligation is smaller than the diff.* `3ffc4642..eefadf25`
  touches 37 files under `src/{ctf,xatrix,rogue}` and the two raw import commits
  differ in 29 more, but the sample is upstream applying repairs this tree had
  already made independently — `CS_GENERAL` → `game.csr.general` (R-62), a
  literal `pm_time` → `112 >> PM_TIME_SHIFT`, `strcpy` → `Q_strlcpy`,
  `CTFSay_Team_Location` gaining its `size` parameter. Convergence, not a queue.
  The three rows above are what was owed and they are imported in 1.39
  (`doc/reconciliation.md` R-187).
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
               gslog.c  net_compat.h      # added in 1.16: R-RA-1a keeps them
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
  *Measured in 1.34*, because `doc/provenance.md` had flattened the conditional
  claim above into an absolute one: **19 of the 72 are still byte-identical, 52
  have diverged**, and 7 files in `src/` have no upstream counterpart at all —
  which is R-CORE-7's subfolder clause reaching the top level for the five the
  ruleset dispatch needed.

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

  *1.16 measures the deletion set exactly, because R-VER-24 needs it.* RA2
  deletes **46 files**: 24 `.c` — the 22 monster translation units, `m_move.c`
  and `g_chase.c` — and 22 `.h`. §3.2 reads "the 44 monster TUs, `m_move.c` and
  `g_chase.c`", which is the right total counted the wrong way: 44 is monster
  *files*, half of them headers. And the requirement's scope needed widening in
  a way nothing had noticed: a donor that deletes whole files is harmless here,
  but RA2 also deletes monster **code from inside the shared files**, which is
  what R-VER-24 exists for.
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

  *Widened in 1.12: the rule is about **value** collisions, not only `FL_` bits.*
  CTF brought three more, in three different numbering spaces, and they resolve
  three different ways — which is the useful part, because "allocate once in one
  table" is not a single remedy:

  | value | claimants | resolution |
  |---|---|---|
  | `IT_TECH` = `64` = `BIT(6)` | Ground Zero's `IT_MELEE` | **moved** to `BIT(8)`. Item flags are game-internal |
  | `MOD_GRAPPLE` = `34` | Xatrix's `MOD_RIPPER` | **moved** to `56`. `meansOfDeath` appears in no protocol or savegame |
  | `WEAP_GRAPPLE` = `12` | Xatrix's `WEAP_PHALANX`, Ground Zero's `WEAP_DISRUPTOR` | **left alone.** `weapmodel` is packed into `s.skinnum` as an index into the *player model's own* weapon list, so it is a per-content-set client asset index rather than a game identifier. The two mission packs already collide there and Phase 2 shipped it |

  The third row is the one worth keeping: a value collision is only a defect if
  the two claimants can be live in one namespace at one time, and this one cannot
  be — the client resolves it against whichever player model is loaded.

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
  `dm` | `dmpro` | `tdm` | `duel` | `ctf` | `arena` | `sp`. Default `dm`.
  Invalid values fall back to `dm` with a warning; they never abort startup.

  *Seven since 1.36, and four of them are one code path.* `dm`, `dmpro`, `tdm`
  and `duel` are OSP Tourney DM's four structures of play, which until 1.36 were
  selected by a **second** latched selector inside the `tourney` ruleset —
  `match_mode`, cached into the global `m_mode` (R-OSP-12). Two selectors for
  one kind of choice is what this flattens: the ruleset now *is* the mode, and
  `m_mode`, `match_mode` and the ruleset name `tourney` are gone. `dm` is
  therefore no longer baseq2's deathmatch; it is OSP's `RegularDM`, and baseq2's
  own implementations survive as the **base** ops that R-MODE-6 inherits from.
* **R-MODE-2.** Legacy cvars are honoured as aliases, evaluated once at
  `InitGame`, so existing configs and 1999 documentation keep working:
  `ctf 1` → `ctf`; `rocketarena 1` → `arena`. Two conflicting aliases produce a
  warning naming both, and the first in the order `ctf`, `arena` wins.

  *1.36 removes two things this rule used to carry.* `ch 1` is no longer read
  at all — Colored Hitman has been out of scope since 1.2 (N7) and a cvar that
  is accepted, ignored and warned about is a third way of saying nothing; the
  `ch` **libvar** is still pushed to the brain as `"0"`, because that one is the
  frozen botlib's, not this dispatch's (R-BOT-6). And `tourney` is **removed
  outright**: not aliased, not mapped, and not special-cased. It is simply not
  a ruleset name, so it takes R-MODE-1's path for any unknown value — warn,
  list the seven that are valid, fall back to `dm`, never abort. `match_mode`
  is not read either, so a `tourney` config selects `dm` whichever mode it
  asked for. A tailored message naming the four replacements was written and
  then removed: it would have made `tourney` the one unknown value that behaves
  differently from the others, which is a smaller copy of the special case this
  amendment exists to delete.
* **R-MODE-3.** `xatrix` and `rogue` are independent latched content layers,
  valid with every ruleset. Both may be on at once. The union content is always
  spawnable; the cvars gate *behaviour* (Ground Zero's plat2/danger-area logic,
  DM rules, `gamerules`) exactly as the 1999 build gated it — measured: 14 uses
  of `rogue->value`, 1 of `xatrix->value` in `gladq2_src`.

  *One narrowing, recorded in 1.36 rather than discovered later.* `gamerules`
  reaches Ground Zero's `DMGame` vtable through `CheckDMRules()`, and
  `OSP_CheckRules()` does not call it — it reimplements the fraglimit, timelimit
  and overtime tests itself. So Tag has never run under `tourney`, and after the
  flattening it does not run under `dm`, `dmpro`, `tdm` or `duel` either. It
  keeps working under `ctf` and `arena`, both of whose `CheckRules` rows call
  `CheckDMRules()` by name. The layers stay valid with every ruleset — every
  Ground Zero item, monster and weapon still spawns and behaves — what narrows
  is this one DM-rules hook, and it narrows because the ruleset that used to
  carry it is now somebody else's code.
* **R-MODE-4.** Modifiers: `teamplay`, `hook`, `runes`/`techs`, and bots. Each
  modifier declares which rulesets accept it; an unsupported combination is
  refused at `InitGame` with one message naming the ruleset, the modifier and
  what was done instead. It never calls `gi.error`. (`ch` and `sp_dm` were
  modifiers in 1.0–1.1 and are struck in 1.2 — see §1.2.)

  *1.20 found that this says nothing about what a modifier does where it is
  **accepted**, and that for `runes` the answer was "nothing"; 1.22 decides it,
  together with Phase 6's `bots`, because they are the same question* (R-88,
  R-96). **A modifier is the resolved answer, not the request.** Where the
  ruleset already has a switch that decides the thing — `DF_CTF_NO_TECH` under
  `ctf`, the `runes_enable` bitmask under the OSP four — the modifier reports what
  that switch says, and the cvar is folded into it beforehand as a request that
  can only ever turn something **on**. So `runes 0` means "do not ask" rather
  than "turn them off", a default server is untouched, `runes 1` clears
  `DF_CTF_NO_TECH` or selects all five runes where none were chosen, the bitmask
  survives, and `G_ModifierEnabled(MOD_RUNES)` becomes the fact that runes or
  techs will spawn. This runs as `G_ResolveModifiers()` at the **end** of
  `InitGame`, after the ruleset's own init has registered its cvars — reading
  `runes_enable` earlier would register it here first with a default of
  resolution's own, which is R-COMPAT-6's trap. Where there is no such switch,
  the modifier **is** the switch: `bots` is a latched cvar defaulting to 1
  wherever the ruleset accepts it, and `G_BotsAllowed()` reads it, so there is
  still one question asked by name. It exists because R-SEC-2's staged exposure
  wants the bot layer turned off without the ruleset being turned off, which
  `serveronlybotcmds` does not give — that gates client access only.
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
* **R-MODE-6.** A ruleset that does not implement a hook inherits the **base**
  implementation, which is Q2PRO's baseq2 code: `CheckDMRules`, `EndDMLevel`,
  `DeathmatchScoreboardMessage`, `BeginIntermission` and `SelectSpawnPoint`.
  The base fills every row and is **not itself selectable** — no value of
  `g_ruleset` dispatches to it.

  *Reworded in 1.36, and the rewording is the point.* Until then the base and
  the `dm` ruleset were the same table, `ops_dm`, so "inherits the `dm`
  implementation" was true by accident of naming. `dm` is OSP's `RegularDM`
  now and fills its own rows, while the five baseq2 functions still have to
  exist — not only for the NULL-row fallback, but because `ctf`, `arena` and
  the OSP family call them **by name** from their own C: `ctf_CheckRules` and
  `RA_CheckRules` both call `CheckDMRules`, `OSP_EndLevel` and `RA_EndLevel`
  both fall back to `EndDMLevel`, `OSP_highscores_cmd` / `OSP_showinfo_cmd` /
  `OSP_setStats` call `DeathmatchScoreboardMessage` as an alternate scoreboard
  view, and `PutClientInServer` calls `G_SelectSpawnPoint()` for every ruleset
  before arena and the OSP family re-place the client. The table is `ops_base`.
* **R-MODE-7.** Composability matrix — the contract:

  | | dm | dmpro | tdm | duel | ctf | arena | sp |
  |---|---|---|---|---|---|---|---|
  | bots | yes | yes | yes | yes | yes | yes | no |
  | `xatrix` layer | yes | yes | yes | yes | yes | yes | yes |
  | `rogue` layer | yes | yes | yes | yes | yes | yes | yes |
  | `teamplay` modifier | no † | no † | no † | no † | implied | via teams | no |
  | team play | no | no | **is the ruleset** | **is the ruleset** ‡ | implied | via teams | no |
  | `hook` | yes (OSP hook) | yes | yes | yes | yes | yes (grapple) | no |
  | `runes`/`techs` | yes (runes) | yes | yes | yes | yes (techs) | no | no |
  | monsters | no | no | no | no | yes (unproven — R-VER-2) | no | yes |
  | `gamerules` (R-MODE-3) | no | no | no | no | yes | yes | — |
  | savegames | — | — | — | — | — | — | yes |

  *The bot row was measured in full in 1.23*, via `sv ruleset`'s new `bots` and
  `botplace` lines: four bots on `q2ctf1` split 2/2 across the CTF teams, four
  on `q2dm1` under `arena` in arena 1 and on teams, four entered, ready and
  split 2/2 under what 1.36 renames `tdm`, and `sp` refusing the modifier by
  name. That measurement predates the flattening and its four OSP rows are
  re-measured under the new names by `tools/botmatrix.sh`, which is why the
  matrix above is seven columns and the evidence below is still five.
  `CheckMinimumPlayers` runs under `arena` as of 1.23 -- Phase 6's
  early-out rested on there being no arena roster a bot could reach, which
  R-ARENA-2 changed -- so `minimumplayers 4` under `arena` settles at exactly
  four (`doc/reconciliation.md` R-109).

  *The monster row was measured in 1.6*, on `base1`, via `sv ruleset`
  (R-VER-18), under the five ruleset names of the time: `dm` 0 live monsters,
  `ctf` **19**, `arena` 0, `tourney` 0, `sp` 17 plus 12 corpses. Only `ctf` and
  `sp` answer yes, and neither changes in 1.36 — the four OSP rulesets all
  answer what `dm` and `tourney` already answered, which is 0.
  `deathmatch` is 1 in the first four, which is the point —
  the inherited `if (deathmatch->value) G_FreeEdict(self)` would have suppressed
  the monsters this row promises under `ctf`, so the row is only true because
  R-MODE-5's predicate replaced that test (`doc/reconciliation.md` R-6). Q14's
  "unproven rather than broken" is now "spawns; behaviour under `ctf` still
  unproven", which is what R-VER-2 is for.

  † The `teamplay` **modifier** is refused across the OSP family, with R-MODE-4's
  one-line message naming `tdm` as the answer. That is the flattening applied to
  itself: once team play is a ruleset, a modifier that also reaches it is the
  second selector this amendment exists to remove. The refusal costs nothing
  measurable — `G_TeamplayEnabled()` has exactly **two** callers in this tree
  (R-MODE-5's "20 call sites" describes `q2pro-ng`'s prior art, not Colosseum):
  `DM_BotFillSeats()`, which rounds a bot-fill target down to even, and the
  `default:` arm of `BotRulesetLibVars()`, which after 1.36 only `sp` could
  reach and `sp` has no bots. The predicate answers from the ruleset now —
  `ctf`, `tdm`, `duel`.

  ‡ `duel` is team play of a particular size, not a mode beside it: two teams of
  **one**, forced — `osp_main.c` sets `team_maxplayers` to 1 and re-registers it
  `CVAR_NOSET`. The team machinery, scoreboard and overtime rules are shared
  with `tdm`; what differs is the roster size and the queue. **The brain is told
  otherwise on purpose**: `BotRulesetLibVars()` sets the botlib's `teamplay`
  libvar from `RULESET_TDM` alone, because on real skins a duellist and their
  opponent wearing the same model would become team-mates. R-BOT-29 is that
  rule, and 1.36 preserves it by making the test name the ruleset rather than
  the mode number.

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

  *And 1.22 is what that versioning was for.* Contract **version 2** gives the
  `Trace` slot its 64-bit spelling, because `osp-tourney`'s copy is a 32-bit
  port's and carries only one of the two (R-BOT-5, `doc/reconciliation.md`
  R-97). It also records what the handshake cannot do: `BotVersion` returns
  `"BotLib v0.96"` on both sides of an ABI split, because the version it reports
  is the brain's and not the convention's, and `Test(int, char *, vec3_t,
  vec3_t)` passes no struct by value. `tools/botabi.py` (R-VER-28) is the check
  that can fail — it compares the two headers slot by slot in `make check`, the
  selecting `#if` included.
* **R-BOT-2.** The game exports nothing to the botlib except `bot_import_t`.
  The botlib is reached only through `bot_export_t`. No shared globals.
* **R-BOT-3.** Loading is **dynamic**, as in 1999: `dlopen`/`LoadLibrary` of a
  botlib named per bot in `bots.cfg`, resolving `GetBotAPI`, with the search
  order of `BotUseLibrary` (direct path, then `basedir` + `gamedir`), reference
  counting per library, and `BotUnloadAllLibraries` on `ShutdownGame`.
  ~~A build option may additionally link `gladiator-bot-restored/botlib`
  statically; when it is on, the static brain is offered under a reserved name
  and dynamic loading still works.~~ **Struck in 1.24, at the author's
  direction.** It was carried as "recorded rather than quietly skipped" through
  1.22 and 1.23 and nothing has ever needed it; carrying an optional feature
  nobody wants across four phases is a worse record than deciding about it. The
  reasons are the ones 1.22 already gave and they have not changed — the brain
  is a sibling repository built for this platform (R-BOT-4) and `botlib` in
  `bots.cfg` names the file — and the cost is a second Makefile target, a
  reserved name in `BotUseLibrary`'s search, and an eleventh build configuration
  to keep honest. If a target ever appears that cannot `dlopen`, this is the
  requirement to un-strike and `BotUseLibrary`'s search order is where it goes.
  *1.22's two measured details are kept, because they are about the dynamic path
  that shipped.* Two details of the dynamic path were measured and
  are worth keeping: the search decides between "the path as given" and
  `basedir` + `gamedir` on whether the value **contains a separator**, not on
  whether the engine can read the file, because `dlopen` takes an OS path
  (`doc/reconciliation.md` R-95); and the donor's refusal of a *second* library
  is kept, because one process cannot hold two AAS worlds and the second brain
  would silently share the first one's — now with the refusal reported.
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

  *Measured in 1.22, and this is the one place the contract has two spellings.*
  The struct is 88 bytes, so it is never returned in registers and the caller
  always passes a hidden buffer. **On 32-bit that buffer is the first visible
  argument** and `bsp_trace_t (*)(vec3_t start, …)` and
  `bsp_trace_t *(*)(bsp_trace_t *retbuf, vec3_t start, …)` compile to the same
  code — the by-value spelling is the one that reproduces both 1999 binaries.
  **On x86-64 and aarch64 the buffer is a hidden REGISTER** (`rax` / `x8`), so
  the two spellings are different ABIs. `gladiator-bot-restored`'s
  `be_interface.h` had grown the explicit one there while `osp-tourney`'s
  `botlib.h` — which R-BOT-1 says to take — carries only the by-value form, and
  taking that on aarch64 shifts every argument one place and hands `passent` a
  truncated pointer: every trace returns all-zero, the brain believes it is
  wedged in solid, and the bots stand still. **Neither of this section's two
  mitigations can see that** — `BotVersion` returns the same string on both
  sides of an ABI split, and `Test()` passes no struct by value — which is why
  R-VER-3 has to be a run and not a handshake. `doc/reconciliation.md` R-97.

  *Settled the other way, hours later.* Upstream **withdrew** the branch
  (`57ce85a3`), from all three of its sites, because it was only ever sound
  while both sides were the same project's: the *published* `game/botlib.h` — the
  header a foreign engine compiles against — was never branched, so any such
  engine disagreed with their aarch64 botlib. By value on every target is also
  the faithful 1999 spelling and the only form that reproduces `gladi386.so`'s
  trace thunk. **So this slot is by value, unguarded, on every target**, and
  carries no `q_gameabi`: that attribute is `callee_pop_aggregate_return(0)`,
  which this build does not enable, and the brain's side has none — if the hack
  is ever turned on, this slot must still not get it. `doc/reconciliation.md`
  R-101, contract version 3.

  **And the convention is only half of it.** Every struct in the contract
  crosses the boundary by pointer or by value, so both sides must also agree on
  its size and on every offset inside it — and two did not, each because a type
  NAME resolves differently on the two sides (`doc/reconciliation.md` R-100).
  `qboolean` is `enum { qfalse, qtrue }` on both, and the donor's port swept it
  to `bool`, which is one byte: `bsp_trace_t` went from 84 to 80 and the brain
  read `endpos[0]` as `fraction`. `stats[MAX_STATS]` is 32 entries on the
  brain's side and 64 on Q2PRO's, so `bot_updateclient_t` went from 1228 to
  1292 and the inventory the brain read was shifted sixteen slots. The
  contract's array bounds are therefore the CONTRACT's — `BOTLIB_MAX_STATS`,
  `BOTLIB_MAX_ITEMS` — and `botlib.h` carries a `_Static_assert` per struct,
  every one of which is pointer-free so the number means something at both word
  sizes. R-VER-28 measures those numbers rather than trusting them.

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
  | `WriteChar/Byte/Short/Long/Float/String/Position/Dir/Angle` | Buffered into one staging message so `multicast`/`unicast` can read it before it goes out. ~~`WriteDir` needs `anorms.h`~~ *Amended in 1.22:* the staging message holds **typed records** — one type byte and the argument — replayed through the engine's own writers at the flush, so the engine receives the call sequence it would have received unredirected. It has to: Q2PRO's `WritePosition` is three **DeltaInt23** values with protocol extensions negotiated and not three shorts, and its `WriteFloat` is `Com_Error(ERR_DROP)`, so re-encoding either corrupts an extended server's every multicast or turns an abort into silence. R-BOT-13 becomes true by construction, and `anorms.h` does not enter the tree — the engine encodes the direction. `doc/reconciliation.md` R-93 |
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
  *Measured in 1.22:* the 1999 constant is **2048**, which is smaller than a
  single legal message — `MAX_MSGLEN` is 0x8000 and one `gi.WriteString` of a
  scoreboard page is over 1400 bytes on its own. It is 0x8000 + 4096 here,
  above the engine's maximum rather than at it, because a typed record costs one
  byte more than the value it carries.
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
  *Measured in 1.22: no donor uses it.* `SVF_NOBOTS` appears in neither
  `rocketarena2-public` nor `osp-tourney` nor the Gladiator reconstruction, so
  the second clause is vacuous and is recorded as vacuous rather than claimed as
  done. `SVF_BOT` is set from `game.csr.extended`, which is the negotiated
  answer, and cleared in `G_FreeClientEdict` — a reused slot must not inherit
  it.
* **R-BOT-17.** `CheckMinimumPlayers` maintains `minimumplayers` by adding and
  removing bots, and counts observers/spectators out — the fix uGladQ2 records
  for CTF (v0.98.2u) and RA2 (v0.98.1u).

  *1.23: it runs under `arena` too, and "counts observers out" needs a different
  sentence there.* Phase 6 returned early under `arena` because no bot could
  reach an arena roster, so "adding a body mid-round" was the only thing the
  call could have meant; R-ARENA-2 puts a new bot in the **waiting** queue, which
  is where a human connecting mid-round goes, so the early-out no longer
  describes anything.

  What replaces it is an arena arm in `BotCountsAsPlayer`, asked before the
  generic observer test. Under `arena`, `G_IsObserver()` means `fightstate ==
  FIGHT_SPECTATING`, which is true of **everyone not in the current round** —
  the whole waiting queue included. Read through the generic test, a server with
  four bots of whom two are fighting counts two players and adds two more, then
  four more, for as long as anybody is waiting, and `minimumplayers` is never
  satisfied. What "playing" means under arena is being on a team, which is also
  what R-RA-4's sixteenth row wants: a person parked in arena 0 with the team
  menu open is an audience and must not hold bots out. Measured:
  `minimumplayers 4` under `arena` settles at exactly four bots, all in arena 1,
  on teams, fighting (`doc/reconciliation.md` R-109).
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
  *Shipped in 1.22 as twenty names* — the seventeen above plus `becomebot`,
  `bbox` and `gps`, and `removebot all`. Three needed something the donor did
  not have: **`botpause`** was behind `#ifdef BOT_DEBUG`, which is defined in
  neither donor tree, so it could not work and `botglobals.nobotai` is now
  unconditional; **`menu`** was dropped from `osp-tourney`'s `bl_cmd.c` because
  that mod has its own menus, and comes back with R-BOT-28's; and **`gps`** had
  its handler kept and its dispatcher dropped, leaving `ShowGPSText` dead
  (`doc/reconciliation.md` R-99). `doc/commands.md` has the table.
* **R-BOT-25.** `serveronlybotcmds` gates client access to bot commands, and
  **defaults to 1** (server only). The 1999 default exposed `bl_spawn.c`'s
  32-byte bot-name copies to clients; `osp-tourney`'s port doc flags exactly
  this. Every string copy on that path is bounded regardless.
* **R-BOT-26.** `bots.cfg` and every `bots/*.cfg` under the gamedir are parsed
  as in v0.92: name, skin, character file, character name; `AddRandomBot`
  returns success; `CheckForNewBotFile` picks up edits without a restart.
* **R-BOT-27.** *Measured in 1.22: the fallback is the normal case, not the
  exception.* Q2PRO exposes `DEBUG_DRAW_API_V1` only in a non-dedicated build
  with a renderer and `USE_DEBUG` on (`src/server/game.c` `PF_GetExtension`), so
  on the dedicated server this library is for, the 1999 beam scheme is what
  runs. Both are implemented and which one is live is reported once at
  `InitGame`. The extension's lines last a stated number of milliseconds where a
  beam edict lasts until deleted, so the extension path re-issues every live
  line each frame from the same handles.
  Debug lines go through Q2PRO's `DEBUG_DRAW_API_V1` extension
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

  *1.34: the RA2 page loses twelve rows and keeps four.* `selfdamage`,
  `healthprotect`, `armorprotect` and the nine weapon switches are cvars
  **registered and read by `gladq2_src/g_arena.c`** — the arena R-ARENA-1 does
  not carry — so in this tree they had no reader at all, while the same sixteen
  concepts live as per-arena settings in `arena.cfg` that `g_combat.c` and
  `give_ammo` read out of `arenas[n]`. The rows this requirement actually names
  are untouched; what went was a second, dead control path for one set of
  settings (`doc/reconciliation.md` R-155).

  *1.42: the gate keeps the password and gains the one caller it cannot ask for
  one.* **The host of a listen server is exempt**, on `pers.listenhost` — a
  fact about the connection, latched in `ClientConnect` from the userinfo the
  ENGINE force-set (`dedicated` 0 and `ip` `"loopback"`, which only the
  in-process client ever has) and preserved across `InitClientPersistant`'s
  memset on both arms and under every ruleset. It must not be read live: `ip`
  is the engine's in the connect packet only, and `setu ip loopback` is a
  command every client has. The exemption is the MENU's alone —
  `serveronlybotcmds` still refuses the host every other bot command, which is
  R-BOT-25 unchanged. And **an unset `rcon_password` is no longer a password of
  `""`**: the donor's bare `strcmp` matched the empty argv(1) that `menu ""`
  produces, so a default server refused the honest form to everybody and
  admitted anybody who typed two quote marks (`doc/reconciliation.md` R-190).
* **R-BOT-29.** *The `TOURNEY` block becomes a runtime gate.* This is the largest
  single piece of work in §5.4 and 1.0–1.1 did not mention it.
  *Done in 1.22, and the seventeen are six different things rather than one*
  (`doc/reconciliation.md` R-94): four are a cvar NAME that stays per ruleset
  (R-OSP-11), four are tourney state reached through a ruleset-neutral accessor,
  one is a fourth client state, two are arithmetic, three are genuine behaviour
  differences kept as the ruleset's, and three are **already dead in the donor**
  — including `bl_redirgi.h`'s `TECH*_INDEX` block, which R-BOT-30 shows has
  never compiled anywhere. The rune→tech translation could not be carried as
  written for a second reason: its five constants are indexes into the 1999
  256-entry model table and R-BOT-11 makes that table's size a runtime fact, so
  the tech models are looked up by NAME in the live table instead. `osp-tourney`
  defines `TOURNEY` at `g_local.h:16` — **live**, not `#if 0`, and every `bl_*.c`
  includes `g_local.h` first, so the donor's own `//#define TOURNEY` lines are
  inert text. Seventeen blocks are active, and as shipped they apply
  library-wide. They must each become a branch on the active ruleset:

  | site | what it does when on |
  |---|---|
  | `bl_main.c` rune→tech translation | rewrites `bue.modelindex` to `TECH1..5_INDEX` for `IT_RUNE` items so the brain sees OSP runes as CTF techs |
  | `bl_main.c` libvar block | drives `usehook`/`laserhook` from `hook_enable`, `teamplay` from `RULESET_TDM` alone, `runes` from `rune_stat` |
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
  The `m_mode` accessor was a **value** accessor, not a predicate. `bl_main.c`
  compared `m_mode == MODE_TEAM` (`0x02`) to drive the brain's `teamplay`
  libvar, and that comparison is exactly right as written: 1v1 is two teams of
  one, the brain has no ally, and `teamplay 0` is the correct answer for a duel.
  A test generalised to "is this a team mode" would set `teamplay 1` in 1v1 and
  give every duel bot an imaginary teammate.

  *1.36 deletes `m_mode` and the accessor with it, and the rule survives
  unchanged in substance.* The libvar now reads `G_Ruleset() == RULESET_TDM`,
  which is the same set of one ruleset that `m_mode == MODE_TEAM` selected —
  **not** `G_IsOspRuleset()`, and **not** the family's team half (`tdm` **or**
  `duel`), either of which would be the generalisation this clause forbids. The
  hazard is if anything worse now than it was: `duel` is visibly a team ruleset
  in R-MODE-7's matrix, so the wrong test reads as the natural one. It is not.
  `G_TeamplayEnabled()` is a different question with different callers and must
  not be substituted here either (R-MODE-7 †).
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
  *Measured in 1.22: every item is present, and one of the transfers does not
  happen.* `anorms.h` **is not in the tree**: R-93 replaced the byte-level
  staging message with typed records replayed through the engine's own writers,
  so `WriteDir` is encoded by `DirToByte` on the engine's side and the 162-entry
  normal table has no second reader. The `TECH*_INDEX` block is dropped for the
  reason this requirement names — it has never compiled anywhere — and R-94
  replaces it with a lookup by model name. Two items landed as something better
  than the donor's fix rather than verbatim: the three `(int)` casts on
  `DF_ENTCLIENT` are unnecessary because the macro now casts, and the userinfo
  `memcpy` bounds are `Q_strlcpy` with `sizeof`, which gives the NUL termination
  and the bound in one.

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
  `doc/reconciliation.md`. *How many bots to place there* is R-RA-7's, which
  also owns the team-arena half of "the correct queue": a bot pairs up on an
  existing bot team where `playersperteam` is above one, instead of always
  starting a team of its own.

  *1.30 adds "telefragging during countdown" to the list of things that had to
  be measured rather than assumed.* It does not happen and cannot: an arena
  fighter is `takedamage DAMAGE_NO` until `ASTATE_FIGHTING`, and
  `!tr.ent->takedamage` is exactly the condition under which `KillBox` takes its
  push-apart branch **instead of** `T_Damage`. So for the whole countdown the
  push is the entire mechanism -- and RA2's push adds the SAME random vector to
  both bodies, which moves the pair without separating it. Two fighters who drew
  one spawn point therefore stand inside each other, which is what a play test
  reported. Fixed at the root -- `SelectRandomArenaSpawnPoint` prefers a point
  with the 50 units of clearance `SelectFarthestArenaSpawnPoint` already calls
  usable -- and at the mechanism, by giving the second body the opposite vector
  (`doc/reconciliation.md` R-135).

  *Discharged in 1.23.* `RA_BotJoinArena` makes the two menu clicks a human
  makes -- pick a team, pick an arena -- and lands the bot in the selected
  arena's **waiting** queue as a noclip observer, which is R-ARENA-3. **"The
  selected arena" was the weak word, and 1.28 fixes it**: the selection was
  1999's `arena` cvar, default 1, which on a real RA2 map is not where anybody
  is -- on `ra2map6` the pickup arena is 8 and four bots built teams of their own
  in arena 1 while the person waited in 8. A value of `0` now means "follow the
  people", resolved at join time (`doc/reconciliation.md` R-131). **"At join
  time" was the next weak word, and 1.30 fixes that**: `CheckMinimumPlayers`
  adds its first bot at `level.framenum` 32, before a person who typed `map` has
  finished the motd and the team list, so that bot has nobody to follow and is
  stranded in the fallback arena for the level while every later bot follows the
  person -- one person against two on `ra2map9`, with a third bot alone next
  door. `RA_BotFollowPeople()` re-asks the question on the same 32-frame cadence
  that added the bot, and moves one only when its `arena` key is out of `1..N`,
  somebody is on a team somewhere, **nobody** is on a team in the arena it is
  already in, and the target would accept it -- checked before it is taken off
  its current team (`doc/reconciliation.md` R-134). On an
  idarena it joins the smaller of the two pickup teams `arena_init()` made,
  because `AddtoArena` refuses an idarena outright and pickup teams are how a
  deathmatch map's arena is entered. It runs from `ClientBeginDeathmatch` after
  placement, which is where a human's click lands. Finding that it did not work
  is what turned up the `InitClientResp` defect below.
  `doc/reconciliation.md` R-104.
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

  *One of the four ships as of 1.12* — Threewave's, under `ctf`, at
  `src/ctf/p_menu.c` with `ctf_PMenu_*` symbols and a `ctf_pmenuhnd_t` that
  `g_local.h` forward-declares rather than includes. So the **contention** this
  requirement is really about is not yet observable: R-MENU-2a's "four engines
  claiming one input channel" needs at least two engines. What is in place now is
  the part that has to exist *before* the second one arrives — the single owner
  field and the single open path (R-MENU-3) — because retrofitting arbitration
  after two engines are live is how two menus end up open at once.

  *Overtaken in 1.16-1.22, and corrected here rather than left asserting a
  premise that expired three phases ago.* All four engines ship: RA2's `qmenu_t`
  under `arena` (Phase 4), OSP's `pmenu_t` under `tourney` (Phase 5), and the
  Gladiator `menu_t` tree with its bot menu (Phase 6), which is the one that is
  present under **every** ruleset that accepts bots. So the contention is live —
  two engines, one input channel, in the same server — and R-MENU-3's single
  owner field and single open path are what stand between them. What has not
  happened is an *observation* of the arbitration under contention:
  `tools/playtest.sh` drives each engine on its own (a menu opens, the cursor
  moves, `invuse` acts, `/score` closes it and `/inven` reopens it) and never
  asks one engine for a menu while the other holds the channel. That is the check
  this row now wants, in place of the second engine it used to be waiting for.
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

  *Implemented in 1.12* as `gclient_t.menu_owner` plus
  `G_MenuOpen`/`G_MenuClose`/`G_MenuActive` in `p_hud.c`, beside the other owners
  of the layout channel. Threewave's per-engine `inmenu` boolean is **not**
  carried: with four engines that is four answers to a question that must have
  one. Threewave leaks or dangles its handle in four places — disconnect, spawn,
  level change, and a second open that only warns — and each is one line here
  (`doc/reconciliation.md` R-46). The allocation moved to
  `gi.TagMalloc(TAG_LEVEL)`, which is what put the level-change case in reach of
  a fix rather than leaving it a leak.

  *1.28 adds the third thing the arbiter does not own, and the rule that goes
  with it.* Beside `menu_owner` (is a menu on screen) and `curmenulink` (which
  menu) there is the **queue**, and a row's callback may replace all three: both
  "Leave Team" rows end in `init_player()`, which drops the queue and builds a
  fresh menu. `UseMenu` captures `curmenulink` before calling the callback and
  unlinks it after, so it must not assume the capture is still queued -- doing so
  wrote the queue head to NULL and destroyed the callback's own new menu, leaving
  a player with no menu and no way to rejoin (`doc/reconciliation.md` R-130). The
  rule: **a dispatcher may only unlink what is still linked**, and what a
  callback has replaced belongs to the callback.

  *1.30 names the cost of "closing is hiding" and pays it.* A menu built once is
  a snapshot, and both of the arena lobby's menus put a live number in one --
  `Players:` per team, `T:` per arena. RA2's answer is the **"Refresh List"** row;
  ours made `inven` a reopen rather than a rebuild, which is right for every
  other reason and means a player who watched four bots arrive on the scoreboard
  and pressed TAB saw the counts from before they connected. Rebuilding on open
  is refused -- `FinishMenu` pushes a menu onto the queue and nothing pops the
  old one, so binding a rebuild to a key pressed all match long turns a bounded
  1999 leak into an unbounded one. `RA_RefreshMenuCounts()` updates the numbers
  **in place** from `MenuThink`, which already repaints every ten frames, making
  them live rather than merely fresh-on-open (`doc/reconciliation.md` R-136).

  *1.20 adds the two halves the arbiter must not conflate, found by a client
  rather than by reading (`doc/reconciliation.md` R-87).* **Closing a menu is
  hiding it, not destroying it** — RA2 keeps `showmenu` apart from
  `curmenulink`/`menuqueue` precisely so `inven` can bring one back and so
  `FinishMenu(show=false)` can build one without displaying it, and an arbiter
  that destroys on close breaks both. And **closing must repaint**, because a
  menu is not always drawn in the layout channel: RA2 draws its menus by
  overwriting `CS_STATUSBAR` for one client, so a close that writes nothing
  leaves the player looking at a menu the game has already forgotten. Both
  follow from one ordering rule: `G_MenuClose()` clears the owner field
  **before** dispatching to the engine's own close, so that close can ask which
  bar to restore and get the right answer.
* **R-MENU-4.** Menu input is consumed in `ClientThink` before weapon and
  movement handling, and the client's own `showscores` is forced off for bots
  in `ClientEndServerFrame` (the v0.91 fix).

  *First half in 1.12:* the four item-cycle and use commands
  (`invnext`/`invprev`/`invuse`/`inven`) check `G_MenuActive()` before anything
  else can interpret the same key, and `ClientThink` flushes a pending redraw at
  the engine's cadence rather than the player's. **The bot half is Phase 6** —
  there is no bot whose `showscores` could be forced off.

  *1.20:* `inven` is a **toggle**, and R-VER-27 checks it as one — open, close,
  open again. Checking only that it opens passes on a build where closing has
  destroyed the menu and nothing can reopen it.

  *The bot half lands in 1.23, and it is expressed as what it means rather than
  as the donor's one assignment.* The donor knew a single layout owner and could
  write `if (ent->flags & FL_BOT) ent->client->showscores = false;`. This tree
  has four menu engines and RA2's `scoremode` in place of `showscores` (§7 rule
  3), so a bot is instead **never sent a layout at all**: `ClientEndServerFrame`
  clears the field the running ruleset writes and returns before the menu think,
  the bot-menu repaint and the scoreboard block. It stops being cosmetic under
  `arena`, where `init_player()` opens the team menu for every connecting client
  — so without it every bot on an arena server repaints a menu it cannot operate
  every 32 frames. The bounding box (R-BOT-27) stays above the arm, because that
  overlay is drawn *around* a bot for a human to watch.
* **R-MENU-5.** The layout string built by the core respects `MAXSTATUSBAR`
  (1400) and truncates on a whole item, never mid-token.

  *Implemented in 1.12, and it was a real defect rather than a precaution.*
  Threewave builds into `char string[1400]` with
  `sprintf(string + strlen(string), …)` and **no bound at all** — 24 entries of
  40 characters run off the end of a stack buffer whose contents then go to
  `gi.WriteString`. `q2pro/src/ctf` did not fix it either. Each entry is now
  composed into its own scratch buffer and appended only if it fits whole, with
  the position and the text treated as one item: emitting the `yv`/`xv` pair and
  then dropping the string would move the cursor and draw nothing. The same 1400
  and the same discipline apply to the composed statusbar (R-OSP-7a).
* **R-MENU-6.** *New in 1.38, and it is R-MENU-5's sibling.* R-MENU-5 bounds a
  layout in **bytes**. A layout also has a **pixel** budget, and nothing bounded
  it: **a composed layout fits inside the pic it draws over, and a page size is
  DERIVED from that pic rather than being a constant.**

  For the arena menu the derivation is arithmetic and every term is measured
  rather than chosen. `menu.c` draws over `picn inventory` at `xv 32 yv 8`;
  `inventory.pcx` is 256x192 and its flat interior -- palette index 175
  throughout -- is pic rows 18..174, so the text area is statusbar y 26..182. A
  `string2` at `yv Y` paints Y..Y+7, because every printable glyph in
  `conchars.pcx` uses all eight rows. Rows start at `yv 40` and step by 8. So
  `40 + 8k + 7 <= 182` gives k <= 16 -- **seventeen rows** -- and the "(More)"
  marker, which is not a row and which only some pages carry, takes the 7px that
  are left.

  *Inherited or new (§0 rule 6): new.* The donor's `MAXMENUITEMS` is 18 and its
  marker sits at `y + 10`, and neither has ever fitted -- 18 rows end at
  `yv 176`, painting 176..183 across the frame line, and the marker lands on the
  bevel outside the box. RA2's own settings menu is 26 rows, so every page it
  ever drew showed it. This is therefore a deliberate divergence from `port_ra2`
  rather than a preservation, and it is **not** in R-SEC-2's kept-bugs ledger:
  all seventeen entries were read, 3 and 14 touch `UpdateStatusBars` and
  `SendStatusBar`, and neither is this.

  **Seventeen and not sixteen, and the difference is one requirement colliding
  with another.** Sixteen would fit the marker with clear space below it and
  would push R-RA-8's "Allow Bots" row onto page 2, which is the position that
  row was placed for. Every *content* row is inside the interior either way;
  what seventeen spends the remainder on is a hint marker touching the frame.
  Testable from outside the process: `scenarios/ra2botvote` reads a client's own
  statusbar and prints `page 1 is 17 row(s), ending "Allow Bots: YES"` -- the
  boundary, and not merely the row's presence.

### 5.7 Engine features used

* **R-ENG-1.** `apiversion` is `GAME_API_VERSION`. `USE_NEW_GAME_API` is on **by
  default**, so the library targets `GAME_API_VERSION_NEW` (3302) with
  `gclient_new_t` / `pmove_new_t`. That default is the shipped configuration and
  the one every other requirement is written against.
* **R-ENG-1a.** *The game ABI is a build switch, and `new` is the default.*
  `make API=old` builds the same sources against `GAME_API_VERSION_OLD` (3) with
  `gclient_old_t` / `pmove_old_t`, for a 1997-vintage engine or a Q2PRO built
  without `USE_NEW_GAME_API`. Q2PRO's own loader accepts either
  (`q2pro/src/server/game.c:1014` tests both constants), so this is a real
  target rather than a hypothetical one.

  *Landed in 1.29, and the compile was the easy half.* Every linked translation
  unit already built clean at both settings with no source change: `g_save.c`,
  `p_client.c` and `g_local.h` carry Q2PRO's own `#if USE_NEW_GAME_API` arms for
  the savegame field layout, `PM_trace`'s signature and `PM_TIME_SHIFT`, and
  R-BOT-1's contract was already decoupled from the engine's `MAX_STATS`
  (`BOTLIB_MAX_STATS`). Four things did have to change:

  1. `config.h`'s `USE_NEW_GAME_API` is `#ifndef`-guarded so a `-D` can win
     without a redefinition, which R-BUILD-2 makes a build failure;
  2. the build directories diverge — `<dir>-oldapi` — because the switch changes
     **struct layouts** and mixed objects link cleanly and run wrong, and
     `-MMD -MP` cannot catch it since no header changed (R-48's failure with the
     one mechanism that stops it removed). The artifact **name** does not change,
     because the engine looks for `game<CPU><suffix>` and nothing else;
  3. **R-OSP-7 clause 6's ceiling was one bound and needed two** — see below;
  4. **the tourney runes had never worked, and `runes 1` was a server death.**
     `-Warray-bounds` at `API=old -O2` refused one line, and under it was a
     rename whose *meaning* changed while three kinds of code kept the old one:
     the pickup granted an arena stat tourney does not map, fourteen reads took
     ordinals where slots belonged, and the rune counter incremented six ints
     before itself — so the spawner never terminated and `g_ruleset tourney`
     with `runes 1` died at map load with `ED_Alloc: no free edicts`. It is
     R-OSP-7's own failure mode, it is now two `slotkind.py` checks with
     controls and a play test (`tools/osprunes.sh`), and R-132 records why five
     phases of checks could not see it.

  **The ceiling.** Clause 6 read as a single question — did the client negotiate
  the protocol extension? — and that was complete only while the API was fixed
  on. The two switches are independent in **both** directions: `G_InitGame` sets
  `game.csr` from `sv_features` and `g_protocol_extensions` alone, so `extended`
  can be true on a library whose `player_state_t` is `player_state_old_t` with
  `stats[32]`. `g_stats.c`'s `stat_ceiling()` is therefore the lower of what the
  **array** holds and what the **wire** carries, and `ctf`'s timer pair at 32/33
  is dropped on an `API=old` build even with extensions on. Without it those two
  rows resolve and CTF writes two `int16`s off the end of the struct, per client,
  per frame, for every client holding a second powerup.

  *Observed, not reasoned:* `tools/playtest.sh` on the `API=old` release prints
  `ruleset ctf (extensions on, api 3, so slots 0..31 are reachable)` with both
  rows reported dropped, on the same `q2proded` that loads the 3302 library. The
  battery's `ctf+ext/timer rows resolve` row **fails** there and passing it would
  be the defect — it encodes the new-API expectation, and R-ENG-1a is the reason
  a second expectation now exists (§10, and the harness has no `-api` flag yet).
  `sv slots` prints the api version for exactly this reason: since R-ENG-1a,
  `extensions on` no longer implies 0..63, so the range needs both halves. The
  `(extensions <on|off>,` prefix is load-bearing — R-VER-27's harness anchors on
  it — so the field is appended rather than inserted.

  The two settings are **not** savegame-compatible and are not meant to be:
  `SAVE_VERSION` is already `0x100` against `8` behind the same guard, so a
  crossed savegame is refused rather than misread.
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
  *Landed in 1.22*, because R-BOT-8's paths and R-BOT-26's pak-visible
  `bots/*.cfg` are the first things that need it. The import side is kept by
  pointer, as `gameext.h` says it may be; the export table is declared with
  every entry NULL, so the entry point exists and nothing claims a capability it
  does not have. `CanSave` is the first of those Colosseum will want (R-ENG-6).
  The shim is `src/g_fs.c` rather than four functions beside `GetGameAPI`, for a
  reason `make check` gave: see `doc/reconciliation.md` R-95.
* **R-ENG-4.** `FILESYSTEM_API_V1`, when present, is used for **all** botlib
  file access — `.aas` files, `bots.cfg`, character files, weapon/item/sound
  configs — so bot data can live inside `.pak`/`.pkz` archives. The 1999 `stdio`
  path remains as fallback. This is a capability the original never had.

  *Measured in 1.22, and "all" needs a boundary.* It is all of the file access
  **the game library performs**: `bots.cfg` and every `bots/*.cfg` under the
  gamedir, which is what R-BOT-26 asks for. The brain has its own file layer —
  `<basedir>/<gamedir>/` then `<cddir>/<gamedir>/`, with its own pak reader —
  and reads its `.aas`, its character files and its weapon/item/chat configs
  through that. Those cannot go through this extension without the brain being
  handed the API, which the contract has no slot for; it does find files inside
  a `.pak` on its own, which is what matters. One place is **not** the
  extension and must not be: `BotUseLibrary`'s path for `dlopen`, which needs an
  OS path and where asking the engine "can you read this" gave the wrong answer
  (`doc/reconciliation.md` R-95).
* **R-ENG-5.** `DEBUG_DRAW_API_V1`, when present, backs R-BOT-27.
* **R-ENG-6.** `CanSave` returns false for every ruleset except `sp`, and
  false whenever a bot exists. `PrepFrame` and `RestartFilesystem` are
  implemented; `RestartFilesystem` invalidates cached bot file handles.
  *Half of this landed in 1.22:* `G_SavegamesAllowed()` now carries both clauses
  — and the second is a **guard rather than a gate**, because no bot can exist
  under `sp` and it has therefore never fired. It is in because a savegame with
  a fake client in a slot cannot be reloaded: the brain is not saved, and the
  client it describes would come back without one. The three `game_export_ex_t`
  entry points are declared and NULL; wiring `CanSave` is what tells the engine
  before the file is opened, and it lands with the savegame work.
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
  | win32 PE | `i686-w64-mingw32-gcc` 13. **Built** (1.5): `gamex86.dll`, PE32, clean. **Run** (1.27): loaded by q2pro on Windows 10, `map q2dm1`, one client connected, the brain loaded and read its seven configs — but **no bots**, so the boundary risk 19 names is still uncrossed on x86 | **yes** |
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

  **And the brain installs the other way round**, which 1.27 found the hard way
  when a play test could not add a bot. The botlib does its own file I/O from
  its own `basedir`/`gamedir` libvars (R-BOT-8) rather than through the engine,
  and `BotUseLibrary` hands `dlopen` an OS path — so the brain, its `pak7.pak`
  and **one `.aas` per map** all go under `basedir/colosseum`, which is the
  mirror image of the rule above and just as invisible when it is wrong.
  Nothing in the tree documented it: the only statement of it anywhere was a
  comment in `tools/botmatrix.sh`'s header, and a person installing by hand does
  not read a harness. `README.md` says it now and `doc/exposure.md` carries the
  whole install as a stage-0 table (`doc/reconciliation.md` R-126).
* **R-BUILD-9.** *The build tracks header dependencies, and a "clean build"
  claim means `make clean` was run.* `-MMD -MP` in `BASE_CFLAGS` and
  `-include $(OBJS:.o=.d)` after the compile rule.

  Added in 1.12 because its absence produced the worst failure of the phase.
  `$(BUILDDIR)/%.o: src/%.c` with no `.h` prerequisites means a change to
  `g_local.h` rebuilds **nothing**, and the next `make` links objects compiled
  against two different struct layouts — cleanly, warning-free. Adding
  `level_locals_t.forcemap` produced a library whose `AI_SetSightClient()`
  segfaulted on the first frame because one translation unit still had
  `level.total_goals` where another had `level.sight_client`; the symptom read as
  memory corruption in game code, it vanished under `gdb` and under ASAN because
  it was ASLR-sensitive, and a watchpoint blamed an innocent function.

  Two consequences beyond the flags. Every build claim in this project is now
  from `make clean && make everything`, because an incremental build over a
  header change is not evidence. And `git stash` is recorded as a build-cache
  hazard: the stale object survived a `stash -u` experiment that restored the
  sources with fresh mtimes but left objects only a header dependency could have
  invalidated.

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

* **R-CTF-1.** All CTF entities, both flags, ~~all five techs~~ **all four
  techs**, the grapple and the CTF scoreboard work as in `q2pro/src/ctf`.
  *Corrected in 1.12:* Threewave 1.52 ships **four** — `item_tech1` Disruptor
  Shield, `item_tech2` Power Amplifier, `item_tech3` Time Accel, `item_tech4`
  AutoDoc — and `g_ctf.c`'s own `tnames[]` lists four. Five is tourney's *rune*
  count (R-OSP-7's slot table, `RUNE_RESIST`…`RUNE_VAMPIRE`), which is a
  different mechanic in a different ruleset.
* **R-CTF-2.** `capturelimit` ends a match, and it works when `fraglimit` is
  unset — the uGladQ2 v0.97u fix.
* **R-CTF-3.** The offhand hook is available and switchable with `ctf_hook`
  (uGladQ2 v0.97u); observers cannot fire it. The `laserhook` variant is
  selectable and reported to the brain through the `laserhook` libvar.

  *Implemented in 1.12, except the libvar.* `ctf_hook` defaults to 1 as uGladQ2
  had it; `hookon`/`hookoff` drive a three-bit latch on the client
  (`ON`/`TURNOFF`/`FIRED`) and the hook fires from `ClientThink`, which is what
  makes it offhand — it costs no weapon slot and does not interrupt what the
  player is holding. "Observers cannot fire it" is `G_IsObserver()` rather than
  uGladQ2's `solid != SOLID_NOT`, which also catches a *dead* player.
  `laserhook` selects the beam rendering, which Threewave shipped as
  `#if 1 //def USE_GRAPPLE_CABLE` — a compile-time choice, and the reason it has
  to become a cvar is precisely that a compile-time switch cannot be reported to
  the brain. **The libvar push itself is Phase 6**, with the rest of the
  ruleset → libvar mapping: there is no botlib to push it to yet, and a check
  that cannot fail is not a check.

  *1.30 adds the half that makes it reachable.* `hookon`/`hookoff` were the whole
  implementation and **`+hook` is a console alias no Quake II client ships**, so
  `bind e "+hook"` -- which is what every 1999 config and every player types --
  bound a key to nothing. uGladQ2 stuffs `alias +hook hookon` and
  `alias -hook hookoff` at the top of `ClientBegin`, with no `cmd` prefix because
  an unrecognised console command is forwarded to the server, and skips a bot,
  which has no console. Carried verbatim and gated on the ruleset rather than on
  `ctf_hook`: `CTFHook_f` tests the cvar itself, so a server toggling the hook
  mid-map neither leaves a stale alias firing it nor needs clients to reconnect.
  `tourney` has done the same since Phase 5 (`OSP_hookAliases`), which is why the
  gap was ruleset-shaped rather than visible. `doc/reconciliation.md` R-138.

  *And the sweep for the same defect found it once more, in the other ruleset.*
  RA2 stuffs `alias +grap grap_on` and `alias +hook grap_on` at the top of its own
  `ClientBeginDeathmatch`; `grap_on`/`grap_off` were carried into `ClientCommand`
  under `RULESET_ARENA` and the aliases were not, so the arena grapple was
  reachable only by typing the command by hand. Both rulesets are stuffed from one
  place in `ClientBegin` now. Every `stuffcmd` and `svc_stufftext` in all five
  donors was checked against this tree and nothing else is missing
  (`doc/reconciliation.md` R-139).
* **R-CTF-4.** `botctfteam` assigns bots to a team, and it is editable from the
  bot menu (v0.92).

  *Implemented in 1.23, and the cvar had been written and never read.* The bot
  layer has set the `ctfteam` userinfo key from `botctfteam` since Phase 6;
  nothing in the tree consumed it, so a bot under `ctf` became a `CTF_NOTEAM`
  observer holding a join menu it cannot press. The reader is the Gladiator
  donor's `#ifdef BOT` arm at the top of `CTFStartClient`, and `CTFAssignTeam`
  is split into the donor's pair so its balancing body is reachable without the
  `DF_CTF_FORCEJOIN` gate. Two decisions are **not** the donor's, because its
  CTF is Threewave 1.09 and has no match system: the arm runs ahead of
  `ctfgame.match >= MATCH_SETUP`, and a bot joining during `MATCH_SETUP` is
  marked ready. `doc/reconciliation.md` R-103.

  *1.30: on a team is not the same as knowing it.* A play test reported CTF bots
  shooting their own side, and the cause is that **`clientsettings[].skin` is the
  only currency the brain has for teams** -- `BotCTFTeam()` is
  `strstr(skin, "ctf_r") ? RED : BLUE` and `BotSameTeam()` compares the half
  after the `/` -- while `bl_main.c` fills that field from
  `Info_ValueForKey(pers.userinfo, "skin")`, which Threewave never rewrites.
  `CTFAssignSkin` writes the `playerskins` configstring and stops, because a
  client is the only thing Threewave has to convince. So every bot read its own
  chosen skin, `strstr` found no `ctf_r`, **every bot on the server believed it
  was blue**, and no two bots were ever team-mates. The donor fixes it and the
  merge dropped the fix: `gladq2_src/g_ctf.c` adds the userinfo write to all
  three arms under `#ifdef BOT` -- the same fence this requirement found around
  `ctfteam` -- and **moves the call after the `pers.userinfo` save**, with the
  note saying why. Both are carried. The libvar half is the other necessary
  piece: the donor sets **no** `teamplay` under ctf, and that omission is
  load-bearing, because `BotSameTeam` tests `teamplay` first and a whole-string
  compare makes `male/ctf_r` and `female/ctf_r` enemies. `doc/reconciliation.md`
  R-137.
* **R-CTF-5.** Threewave has no baseq2 `spectator` flag — observers are
  `CTF_NOTEAM` players — so spectator handling and the savegame descriptors
  point at the fields Threewave uses, as the mission-pack port established.

  *Understated, corrected in 1.12.* Threewave does not merely lack the flag: it
  **deletes** `client_persistant_t.spectator`, `client_respawn_t.spectator`, the
  `spectator` userinfo key, the spectator password, `maxspectators`,
  `spectator_respawn()` and `GetChaseTarget()`. Colosseum cannot point at "the
  fields Threewave uses" and be done, because `dm` and `sp` need baseq2's and
  R-CORE-6 makes the struct a union. So the *question* is named once —
  `G_IsObserver()` in `p_hud.c` — and thirteen sites ask it instead of testing a
  field. Two of those sites are wrong in the donor and the predicate fixes both:
  `Cmd_Kill_f` tests `solid == SOLID_NOT`, which is also true of a dead player,
  and `ClientThink` gates the attack button on `movetype != MOVETYPE_NOCLIP`,
  which is the right set under `ctf` only by coincidence.

  *1.45: the FIFTH answer, and it was declined twice before it was taken.*
  Tourney's spelling is `resp.osp_entered != ENTERED_ENTERED` — the donor
  deleted baseq2's spectator system too — and R-191 answered two sites by hand
  rather than widening the predicate, on the grounds that two of its callers
  would then diverge from the donor. By R-193 that was six sites, four of them
  the chase camera's own target search, so the answer is in the predicate and
  the two divergent callers carry an explicit arm each: `ClientBeginDeathmatch`
  announces an arriving observer (the donor's is unconditional) and `p_view.c`
  keeps `G_SetStats` for one, because the donor has no `G_SetSpectatorStats` at
  all. Tourney's OBSERVER INPUT is not baseq2's either and the two must not both
  run: baseq2 gives a spectator one key, and the donor gives it a menu, an
  autocam, a chase camera with a zoom and a free-look, an in-eyes mode and a
  target cycle (`doc/reconciliation.md` R-193).
* **R-CTF-6.** Player id display is on by default and shows the team icon
  (uGladQ2 v0.98.2u).

  *1.15: the id view is what carried the defect of R-62.* It needs a player's
  name without the skin that the `playerskins` configstring carries, so it
  writes one to `CS_GENERAL + playernum` and puts that **index** in a stat for
  the client to dereference. Both halves used the compile-time constant while
  the line four above used `game.csr.playerskins`, and on a server that did not
  negotiate the protocol extensions — the default — the index is out of range and
  the server drops on the first connect. All three sites now take the runtime
  remap. The requirement is unchanged; what is added is that **a configstring
  number in this library is `game.csr.*`, never a `CS_*` literal**, for any
  slot the remap moves.
* **R-CTF-7.** The 1999 CTF bot-model bug fixed in `ClientUserInfoChanged`
  (v0.95) and the CTF userinfo bug (v0.93) must not reappear; both get a
  regression entry.

  *Both entries exist as of 1.12 and both are honestly **partial**.* Each one's
  trigger is a *client* — a bot in the first case, a userinfo change in the
  second — and this harness runs a dedicated server with no clients, so the
  observation waits for R-VER-3's bot matrix in Phase 7. What is discharged now
  is structural and reviewed: under `ctf`, `CTFAssignSkin` is the only writer of
  `game.csr.playerskins + n`, reached from the one call site, and
  `CS_GENERAL + n` is written only there. Recording them as held would be the
  vacuous pass `audit.py` exists to prevent.

  *The v0.95 half is discharged in 1.23, off the wire.* R-CTF-4 puts bots on CTF
  teams, so the path the 1999 bug lived on is now taken, and `tools/playtest.sh`
  connects a client to a server with four bots on it and reads each bot's
  `playerskins` configstring back: all four are `ctf_r` or `ctf_b`. That is the
  observation the entry has been waiting for since 1.12 -- not that
  `CTFAssignSkin` is the only writer, which was already reviewed, but that what
  arrives at a client is the team's skin. The v0.93 userinfo half is still
  structural: it needs a drive script that changes `skin` mid-game.

* **R-CTF-8.** `botfill` — under `ctf` the bot count may follow the map and its
  two bases instead of the server. Default **0**, which is off:
  `minimumplayers` is the target exactly as it has always been.

  *Named `ctf_botfill` in 1.35 and unified into one bare `botfill` in 1.36*,
  with `ra_botfill` and `dm_botfill` — R-OSP-11 for why the per-ruleset naming
  rule never covered these three, §7 rule 6 for why one concept keeps one
  implementation. One switch, and the target still computed per ruleset.

  R-RA-7's argument, one ruleset over. A flat count is one number for a whole
  rotation, and **Threewave declares no capacity anywhere**: there is no
  `team_maxplayers` here and never was, `matchlock` locks a match rather than
  sizing one, and `warn_unbalanced` only warns. So the map is the only signal —
  and unlike deathmatch there are **three pools**, because `SelectCTFSpawnPoint`
  sends a client to its own base while `resp.ctf_state` is 0 and to
  `info_player_deathmatch` for every spawn after that. A side therefore seats
  the smaller of **its base** and **half the shared pool**, and the target is
  twice that, because a capture game with uneven sides is not the game.

  The pools are `G_SpawnPointPool()`'s, not the raw counts: both random
  selectors in this tree refuse the two spots nearest a player and choose among
  the rest, so a map's usable pool is two short of what it carries. Measured
  across the eight shipped maps — `2 * min(shared/2, base1, base2)`:

  | map | shared | bases | target |
  |---|---|---|---|
  | `q2ctf1` | 17 | 12, 14 | **16** |
  | `q2ctf2` | 13 | 8, 7 | **12** |
  | `q2ctf3` | 16 | 7, 7 | **14** |
  | `q2ctf4` | 5 | 10, 8 | **4** |
  | `q2ctf5` | 20 | 12, 12 | **20** |
  | `q2ctf6` | 14 | 10, 11 | **14** |
  | `q2ctf7` | 30 | 7, 7 | **14** |
  | `q2ctf8` | 36 | 8, 8 | **16** |

  Both halves of the `min` earn their place: four maps are bounded by the shared
  pool they respawn into, three by a base that cannot seat the side at the
  whistle, and `q2ctf4` by a shared pool of five on a map whose bases would
  claim 8v8.

  **Taking the base pools alone would be wrong**, and it is recorded because it
  is the obvious thing to try — the same trap R-RA-7 records for a non-pickup
  arena. `q2ctf1` carries 14 and 16 team spawn points and is played 8v8;
  Threewave's mappers used them for variety, not capacity.

  One consequence the rule owns rather than leaves to be found. **The bot a fill
  removes is named**: `removebot` with no name takes the lowest client slot, so
  a server filled to an even target could still be shrunk 4v4 → 4v3 → 4v2 as
  people arrive, and an even target is the whole point. `CTFBotFillName()` takes
  one off the **larger** side and returns NULL when the sides are level, which
  falls through to the existing any-bot call. `RA_ArenaBotName` is the same
  function for the same reason one ruleset over.

  A map with no team spawns at all — a deathmatch map booted under `ctf` — is
  handled rather than special-cased: an empty base bounds nothing, which is what
  `SelectCTFSpawnPoint`'s own fallback to the shared pool already means. The
  ceilings are `game.maxclients` (latched, 4 by default) and the roster in
  `botcfg/bots.cfg`, and `sv ruleset` prints `want=`, `seats=`, `shared=` and
  `base=` for R-VER-19's reason. `doc/reconciliation.md` R-156.

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
* **R-RA-2a.** *New in 1.38, and it amends a default rather than adding a key.*
  `arena.cfg`'s `weapons:` names the nine baseq2 weapons by the digit that
  selects them (`2`..`9`, `0`) and the mission packs' six by word (`ripper`,
  `phalanx`, `etfrifle`, `proxlauncher`, `plasmabeam`, `chainfist`).
  **The two halves have separate defaults and separate inheritance**, because
  they have separate histories: the digits span exactly the nine weapons the key
  was invented for, so a line written before the packs existed says everything
  there was to say about the baseq2 half and nothing about the pack half.

  * the baseq2 half defaults to every weapon but the BFG -- `0xff`, the donor's;
  * the pack half defaults to every weapon of a content layer that is **on**;
  * a `weapons:` line owns the pack half only if it names a pack token, or
    `nopack`, which is how an arena refuses all six out loud;
  * a line that names none of them inherits the pack half from the enclosing
    block, and ultimately from that default.

  **What an arena STORES and what it GRANTS are two values.** A bit whose
  content layer is off is carried, not cleared -- an `arena.cfg` is written once
  and served by servers running the Reckoning, Ground Zero, both or neither, and
  switching a layer back on must restore the arena as written -- so
  `RA_ArenaGrantsMask()` is the honoured mask and `give_ammo()` reads that.
  `sv arenadump` prints both numbers side by side, which is what makes the
  difference reviewable without a probe.

  **The five pack ammunitions are each gated on their own layer**, and two of
  them have to be: `ammo_tesla` and `ammo_trap` are the weapon as well as the
  ammunition -- which is why neither has a `weapons:` bit, the donor treating
  `ammo_grenades` the same way -- so an ungated count is not a number on a HUD,
  it is an item in the weapon cycle. Gated by zeroing rather than skipping,
  because `give_ammo()` assigns and the inventory carries across a round.

  *Why this is an amendment and not new scope.* R-182 gave the pack half no
  default and reasoned that a cfg naming none of the six leaves the key meaning
  what it always meant. The premise is true and the conclusion does not follow:
  137 of the shipped `arena.cfg`'s 171 arena blocks name a `weapons:` line of
  their own and the other 34 inherit the default, so between them the file
  covered every arena and neither path could grant a pack weapon. `xatrix 1`
  drew two menu rows reading NO and changed nothing else. Measured on `ra2map11`
  arena 1 with the shipped file, `sv arenadump`:

  | layers | mask | grants | pack | ammo |
  |---|---|---|---|---|
  | neither | `0x00ff` | `0x00ff` | none | all five 0 |
  | `xatrix 1` | `0x06ff` | `0x06ff` | ripper phalanx | magslug 50, trap 5 |
  | `rogue 1` | `0x78ff` | `0x78ff` | the four | flechettes 200, prox 50, tesla 50 |
  | both | `0x7eff` | `0x7eff` | all six | all five |

  The first row is the report that opened this: Teslas and Traps handed out by a
  server running neither pack. `doc/reconciliation.md` R-186 §1 and §2.
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

  *1.20 makes the first two of those observable and finds them broken.*
  "Players into arena 1 as observers" is a claim about `pm_type`, and the only
  thing that can check it is a client: `PutClientInServer` must end its arena
  branch the way the donor does — `gi.linkentity`, `ChangeWeapon`,
  `move_to_arena(ent, resp.context, 1)` — because `init_player()` sets
  `fightstate` and opens the menu but never touches `movetype` or `solid`, and
  the free-flying body comes from `SetObserverMode()` inside `move_to_arena`.
  Without it `G_IsObserver()` said *observer* and the wire said `PM_NORMAL`,
  which no static check can see because the game was internally consistent.
  "Arena menu on connect and on `inven`" needs the menu to survive being closed;
  see R-MENU-3 in 1.20 and `doc/reconciliation.md` R-87.

  *1.23 lands the three bot rows -- 8's cycling half, 15 and 16 -- and finds
  that **row 1 has been structurally false since Phase 4**.* "Arena menu on
  connect" is `init_player()`, and `init_player()` was never running: RA2's
  `InitClientResp` sets `teamnum = -1` after its own memset, the merge kept the
  base, and `PutClientInServer` branches on `>= 0` -- so every connecting client
  took `reinit_player()` and claimed to be on whatever team is in slot 0. Row 15
  ("new bots initialised into the selected arena's queue") is `RA_BotJoinArena`,
  row 16 ("observers ignored against `minimumplayers`") is a new arena arm in
  `BotCountsAsPlayer` -- under arena "playing" is being on a team, because
  `G_IsObserver()` there is true of the whole waiting queue -- and row 8's
  bot-cycling half is `ra_botcycle` (R-RA-5). Row 17's bot half is measured: a
  `gamemap` under `arena` with four bots keeps every one of them in its arena
  and on its team. `doc/reconciliation.md` R-104, R-107, R-109.
* **R-RA-5.** `ra_playercycle` and `ra_botcycle` behave as v0.93 defined, and
  ~~`ra_winner`/`ra_time` live in `gclient_t`~~. *Amended in 1.23.*

  The two fields are `gladq2_src/g_arena.c`'s, and R-ARENA-1 explicitly does not
  carry that file: Colosseum runs RA2's arena, whose **waiting queue holds both
  facts already**. `fill_arena` pops the front, so the order IS "longest
  waiting"; `fight_done` puts the winning team at the front with
  `add_to_front_queue`, so that IS "winners kept between matches". Adding two
  fields to restate what the queue says would be two more things to keep in
  step, so the requirement asks for the behaviour and not the storage.

  What was missing is the switches. `ra_playercycle` (default 1) now gates the
  winner-to-front, so `0` rotates strictly by arrival. `ra_botcycle` (default 1)
  is the half RA2 cannot have because RA2 has no bots: Gladiator spells it
  `RA2_GetLongestWaitingHuman()`, and here it is "take the first waiting team
  that has a person on it, and fall back to the front when none does" --
  **first side only**, which is the donor's own shape, because doing it for
  every side would empty the arena of bots. `doc/reconciliation.md` R-109.
* **R-RA-6.** Monsters do not spawn in `arena`. The classnames still exist
  (R-CORE-2); the ruleset suppresses them.
* **R-RA-7.** `botfill` — under `arena` the bot count may follow the arena
  instead of the server. Default **0**, which is off: `minimumplayers` is the
  target exactly as it has always been, and nothing in the tree behaves
  differently.

  *Named `ra_botfill` in 1.34 and unified into one bare `botfill` in 1.36*, with
  `ctf_botfill` and `dm_botfill`. `arena` is the one of the three that had a
  `cvar_t *` of its own and its own copy of the clamp; both are gone, and the
  shipped `configs/arena.cfg` sets `botfill 1` where it used to set
  `ra_botfill 1`.

  `minimumplayers` is one number for a whole server, and under every other
  ruleset that is the right shape — one map, one game, one roster. Rocket Arena
  runs up to 32 games at once and they are not the same size, so a flat count is
  a crowd in one arena and an empty room in the next. `botfill 1` asks the
  arena that `RA_AutoArena()` is feeding bots into — the same function
  `RA_BotJoinArena` uses, so the census and the destination cannot disagree.

  **The answer comes from two places because the mod keeps it in two places,
  and which one is authoritative is the whole of this rule.**

  * A **non-pickup** arena is `arena.cfg`'s own `playersperteam`, times the two
    teams a round is fought between. That number is not advisory: `AddtoArena`
    admits a team of *exactly* that size and `check_teams` ejects one that grows
    past it. 155 of RA2's 171 arenas declare 1, fifteen declare 2 and `ra2map1`
    arena 5 declares 3.
  * A **pickup** arena has no such number. `arena_init()` overwrites
    `playersperteam` with **128** for every one of them, because RA2 wants a
    pickup team unbounded, and all thirty of RA2's own pickup arenas leave the
    key unset anyway. What is left is the map: the `info_player_deathmatch`
    entities carrying that arena's number, which the two sides take
    **alternately** (`SelectRandomArenaSpawnPoint`), so the arena seats two out
    of every pair. This is also the stock-deathmatch case — an idmap is one
    pickup arena and every spawn point is its own.

  **Counting spawn points for the first kind would be wrong**, and it is
  recorded because it is the obvious thing to try. RA2's mappers used spawn
  points for variety rather than capacity: `ra2map8` arena 3 has thirteen and is
  declared 1v1, and only 55 of 141 non-pickup arenas have as many spawn points
  as `2 * playersperteam`. Filling one to its spawn count builds teams that
  `check_teams` then deletes.

  Three consequences the rule owns rather than leaves to be found. **A bot pairs
  up before it starts a team**: `RA_BotJoinArena` gave every bot a team of its
  own, so a `playersperteam: 2` arena played 1v1 and its declared size was
  unreachable with bots — a bot now joins an under-full **bot-only** team in
  that arena first, which is `menuAddtoTeam`'s shape, and never a person's team
  uninvited. That is gated on the switch too, so `botfill 0` is off in every
  sense and not only in the count. **The census counts bots in transit**: `CheckMinimumPlayers` runs
  before `G_CheckRules` and both gate on `level.framenum & 31`, so on a fill
  tick the bots `RA_BotFollowPeople` is about to move have not moved yet —
  `RA_ArenaPlayers` and that function share one predicate so the fill cannot add
  replacements for bots already on their way. **The roster-exhausted clamp lives
  in `arena.c`**, not in the cvar: the other rulesets write the achieved count
  back into `minimumplayers` so both arms settle on one number, and a switch
  cannot carry a count.

  The ceilings are `game.maxclients` — latched, and 4 by default — and the
  roster in `botcfg/bots.cfg`. `sv arenadump` prints `spawns=`, `want=` and
  `here=` for every arena and `sv ruleset` prints the target in force, because
  the number is computed rather than stored and a play test has nowhere else to
  read it back from (R-VER-19). `doc/reconciliation.md` R-150.

### 6.6 R-OSP — OSP Tourney DM

* **R-OSP-1.** Tourney v2.75 behaviour from `osp-tourney`: the match system,
  referee and vote system, **all four modes of `m_mode` (R-OSP-12)**, the runes,
  the OSP hook, the observer and camera system, hi-scores, the accuracy/stats
  commands, the map system and the menu engine.

  *1.43: connecting is not entering, and the placement has to say so.* A
  tourney client arrives as an OBSERVER and enters through a command, so
  `PutClientInServer` has **two arms** keyed on `resp.osp_entered`: a client
  that has not entered is placed `MOVETYPE_NOCLIP`, `SOLID_NOT`,
  `SVF_NOCLIENT`, with no view weapon and no KillBox. `resp.osp_r240` is the
  other half of the same fact -- 2 for a client that has been given a body, 0
  for one that has not -- and it is what `ChangeWeapon`'s gunindex, that
  KillBox and the autocam's candidate test read. **No join path places
  anybody**: all six set `osp_entered` and clear `osp_r240`, and `ClientThink`
  respawns the client on the next think, freezing it (`PM_FREEZE`) until it
  does. One trigger serves the six, and a placement that was refused retries
  through the same line (`doc/reconciliation.md` R-191).

  *1.44: and a placement CAN be refused, which is what the retry is for.*
  `G_SelectSpawnPoint` is a predicate rather than a `void` row: tourney refuses a
  spot with a player within 60 units of it, and `PutClientInServer` leaves the
  client frozen and bodiless rather than telefragging whoever is standing there.
  Measured on `q2dm1`'s ten spawn points with eleven clients — **4 telefrag
  obituaries before, 1 frozen client and 0 telefrags after**, and the frozen one
  placed as soon as four left. Two exclusions the shared `PlayersRangeFromSpot`
  does not make are load-bearing and both are the donor's: the client being
  placed is not measured against itself, and an observer is not a player
  (`doc/reconciliation.md` R-192).

  *1.45: the observer and camera system, which this requirement has named since
  1.0 and which was carried without its input or its consumer.* The chase camera
  has FOUR controls — forward/back zooms it between `camera_depth` and the
  target's eye, strafe free-looks it in four-degree steps, ATTACK cycles
  chasecam → in-eyes → out, jump cycles the target — and `UpdateChaseCam` is
  where they are spent: the two modes differ in the pitch they allow (56 against
  1), in whether the camera is behind the target or twelve units in front of its
  eye, and in whether it is lifted 30 units off the floor. What tourney does to
  each client **when the level ends** is here too, and was missing whole: the
  demo it was recording is stopped (with a screenshot at `demo_*` 2), the
  end-of-match music plays from `wav_file`'s five tunes or from
  `match_endmusic`, a team ruleset plays the winner and the loser different
  ones, an entered client is shown its accuracy page, its menus are closed, and
  the board a player who died into the intermission is owed arrives 1.25
  seconds in. `nextlevel_click` (15s) is how long a press is ignored for and
  `nextlevel_lazy` (45s) ends the intermission with no press at all; `numgibs`
  is the gib count. All of those were registered cvars and carried tables with
  no reader (`doc/reconciliation.md` R-193).
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

  *All nine are entered in `doc/regression.md` in 1.23, which is what R-VER-1
  asks and what "gets a regression entry" means.* All nine arrive **fixed** —
  `osp-tourney` is the ported tree and this is §0 rule 6's inherited half — so
  each entry's job is to say what would tell us it had come back. Four of them
  are bot-reachable and had to wait for a bot that plays: the SIGFPE is a live
  check in `tools/playtest.sh` (a vote left running after the last human leaves
  a server with bots on it, which only a client can arrange), and the other
  three are structural against sites the bot rows now exercise. The remaining
  five are structural and reviewed, and say so rather than claiming a run.
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
     header. *Implemented in 1.11* as `src/g_stats.h`'s `STATSLOT_MAP(E)`
     X-macro: one row per logical stat, one column per **numbering**, the kind
     beside the number, `-1` for absent. The X-macro form is not decoration —
     clause 3 wants one header and clause 7 wants a tool to check it, and in C
     those pull opposite ways; this is the one text the compiler and
     `tools/slotkind.py` both read. `arena` and `tourney` inherited `dm`'s
     column until their own bars landed in Phases 4 and 5, per R-MODE-6: a
     column that renumbers a slot while the bar still says 18 is worse than
     inheriting, and the checker would have to be told to ignore it.

     *1.0–1.35 read "one column per ruleset … **including `dm`'s own**, which
     owns 18/19", and 1.36 makes both halves false.* A column is a numbering,
     and four rulesets now share one: `dm`, `dmpro`, `tdm` and `duel` all take
     the `osp` column, because they are one code path emitting one bar. And
     baseq2's own numbering — the 18/19 that sentence names — is **deleted**,
     not renamed: it was read by `RULESET_DM` alone, and `dm` uses OSP's now.
     Five columns become four: `osp`, `ctf`, `arena`, `sp`;
  4. a **shared mechanic** that needs a private slot in every ruleset declares
     it once per ruleset in that header and is compiled against the ruleset's
     map, never against a bare `#define`. The second powerup timer is the
     reference case and the only one known today. *Implemented in 1.11:*
     `p_hud.c`'s `#define STAT_TIMER2_ICON 18` is gone and the one shared
     `G_SetStats` writes `G_SetStat(ent, SID_TIMER2_ICON, …)`. An id the active
     ruleset does not have is a **silent no-op**, not a write to `stats[-1]`,
     which is what lets shared code stay ignorant of where — or whether — the
     mechanic landed;
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
     (R-COMPAT-5). *Exercised in 1.11:* `ctf` puts the second powerup timer at
     **32/33**, and "content it can drop" is made structural rather than
     remembered — `G_InitStats()` deletes every slot the running configuration
     cannot reach, so the bar items and the writes disappear together and
     `sv slots` reports each dropped row by name;

     *Amended in 1.29: the ceiling is two bounds, not one.* Until R-ENG-1a the
     test was `slot >= MAX_STATS_OLD && !game.csr.extended` — a question about
     the **wire** only, which was complete while `USE_NEW_GAME_API` was fixed on
     and `MAX_STATS` was therefore always 64. It is not complete now that the API
     is a build switch, because the two are independent: `extended` can be true
     on a library whose `player_state_t` holds 32 slots. The rule is the lower of
     what the array holds and what the wire carries (`g_stats.c`'s
     `stat_ceiling()`), enforced in the one place that also sizes `used[]`;
     "may use 32–63 only for content it can drop" now means droppable for **two**
     reasons rather than one, and `ctf` loses the pair under `API=old`
     unconditionally;
  7. the check is on **kind** as well as number. A statusbar element declares
     what a slot holds — `stat_string N` a configstring index, `pic N` an image
     index, `num W N` a plain number — and writing the wrong kind is a type
     error across a boundary with no compiler on it. A name-collision test
     cannot see it when the bar uses a bare number, which is how RA2's slot 20
     got through. `tools/slotkind.py` (R-TOOL-1) is that check and its known
     gap is closed before Phase 4 exits.

     *Closed in 1.11, and the diagnosis changed.* The gap was not a missing
     special case but a **missing question**. Kind alone found RA2's slot 20 by
     luck — its two claimants happened to disagree about kind; had both been
     `pic`, a slot with two live meanings would have passed. So the check now
     asks two questions, and the second keys on the **number**: *was the slot
     available at all?* No two ids on one slot per ruleset; nothing below 16;
     nothing on a slot the pristine bar already draws by its `shared.h` name,
     even when both agree on the number, because a slot with two owners is the
     ambiguity clause 3 exists to remove; and the emitter's op, the map's kind
     and the writer's value must all agree. On `q2pro/src/ctf` it now reports
     **17, 18 and 19** where it reported 17 — the two it used to miss are exactly
     the two whose claimants agreed about kind. Five controls ship inside the
     tool (`--selftest`) and run in `make check`, because a control kept in a
     document cannot report that it has stopped testing anything (R-35).

     *TWO MORE questions in 1.29, and between them a live defect that had never
     run rather than a precaution: is the map reached through the map at all, and
     does the thing carrying an id spell it as one?* `statslot_t` is an enum, so
     both directions convert silently. `ps.stats[SID_OSP_RUNE_HASTE]` indexes by
     the id's **ordinal in `STATSLOT_MAP`** instead of by the ruleset's slot, and
     `G_SetStat(ent, ent->item->quantity, 1)` resolves whatever logical id shares
     that ordinal — clause 4's "compiled against the ruleset's map" evaded once by
     an access that names the map and bypasses it, and once by a value that
     reaches the map meaning something else.

     Neither existing question can see either: no kind is named, so the kind
     check cannot fire, and **the map itself is correct**, so the availability
     check cannot either. Measured on this tree, and it is the largest defect the
     project has found in its own work. The tourney runes' `quantity` still held
     the donor's literal 22 while every consumer had been renamed to
     `SID_OSP_RUNE_RESIST` (28), so the pickup set an *arena* stat that tourney
     does not map — granting nothing — while fourteen reads took ordinals 28..32
     where the runes sat at 22..26, two of which are tourney's own
     second-powerup-timer pair, so holding a pent read as holding the STRENGTH and
     HASTE runes. And `r_count[quantity - SID_OSP_RUNE_RESIST]` was `r_count[-6]`,
     which is the counter `OSP_checkMinRunes` terminates on — so it never rose,
     the spawner and the checker tail-called each other, and **`g_ruleset tourney`
     with `runes 1` died at map load with `ED_Alloc: no free edicts`**. It
     survived five phases, fifteen audits, ten build configurations, a twenty-row
     boot matrix and the play-test battery, because every one of them asked the
     map and the map was right; what found it was `-Warray-bounds` at
     `API=old -O2`, where the array is narrow enough for ordinal 32 to be out of
     bounds.

     The checks are now "a `SID_` never appears inside `stats[]`" and "an item
     whose `quantity` an accessor reads must set it to a `SID_` name" — the second
     a **join between two files**, because the first attempt ("the id argument
     must be a `SID_` literal") rejected the fixed code as loudly as the broken
     code. Seven controls now, and the pair was run against the pre-fix tree
     before landing: 19 findings, 0 after. R-132 records why no existing check
     could have found it and R-VER-27's `tools/osprunes.sh` is the behaviour claim.
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

  *Implemented and verified in 1.11.* `single_statusbar` and `dm_statusbar` are
  deleted from `g_spawn.c` and `ctf_statusbar` never arrives; `G_SetStatusbar()`
  composes. The replacement is checked rather than trusted, because it replaces
  working code with generated code: the emitted `dm` bar is **token-identical**
  to the concatenation of the two literals as they stood at `4d03591`, compared
  mechanically after whitespace normalisation, and `sp` emits the universal block
  alone, which is what `single_statusbar` was. Two things fall out that upstream's
  `!ctf->value` gating could not buy: the CTF squeeze is *resolved* rather than
  worked around — the pent countdown displays under `ctf` on an extended server,
  where before it could display nowhere — and one deliberate departure from the
  donor becomes a decision rather than an accident: Threewave draws the powerup
  timer at `xv 246` and baseq2 at `xv 262`, which is a stale copy of id's layout
  rather than a CTF feature, so §7 rule 1 gives every ruleset 262.
* **R-OSP-8.** Monsters do not spawn in `tourney`; same treatment as R-RA-6.
* **R-OSP-9.** *Map rotation is per ruleset* — the third exemption to §7 rule 6.
  `sv_maplist` for `dm` and `sp`, RA2's `maploop.c` (845 lines, which is also the
  `arena.cfg` block/key parser and so cannot simply be deleted) for `arena`, and
  OSP's `osp_maps.c` (338 lines) for `tourney`. Exactly one is live at a time,
  and that is a requirement rather than an observation: **RA2 currently runs two
  at once** — `maploop.c` and the q2pro-inherited `sv_maplist` path it really
  reads at `g_main.c:369-370`. That is a bug and is fixed as part of the merge.
  OSP registers `sv_maplist` and never reads it, which is correct and stays.

  *Resolved in 1.17, by placement rather than by a check.* RA2's `maploop.c` is
  the arena ruleset's **`EndLevel` dispatch row**, so only one rotation is
  reachable at all and which one is a property of the ruleset. The donor's own
  q2pro port put `get_next_map()` inside `EndDMLevel`, which is the double
  rotation this requirement names: `sv_maplist` still ran when maploop had
  nothing to say, and both had already decided where to go. Falling back to
  `EndDMLevel()` for an empty maploop is now one choice made in one place.
  Tourney's `osp_maps.c` takes the same row in Phase 5.
* **R-OSP-10.** *Stats logging is per ruleset* — the fourth exemption. There are
  **four** subsystems, not the two 1.0–1.1 assumed: RA2's `ra2stats.c` (428
  lines, one JSON object per round) and `gslog.c` (339, StdLog, server-wide and
  arena-blind); tourney's `osp_stats.c` (867, one JSON object per event) and the
  Standard Log (`stdlog.c` 263 + `sl_write.c` 250). All four ship, active per
  ruleset. Both replacements for the dead upload services are **inherited, not
  work** (§0 rule 6): `rocketarena2-public@789895b` removed the GameSpy `gstats`
  SDK and `osp-tourney@a8e30d0` the NetGames USA layer.
* **R-OSP-11.** *Bot cvars are per ruleset.* Under the **OSP family** — `dm`,
  `dmpro`, `tdm`, `duel` — the names are tourney's: `bots_autoload`,
  `bots_minplayers` (default 4, so bots are on out of the box), `bots_botfile`,
  `bots_delayload`, `bots_warmuptime`, `bots_noclients`, `vote_bots_max`. Under
  `ctf` and `arena` they are `minimumplayers` and `botfile`. Both sets are
  registered; `doc/cvars.md` states which is authoritative per ruleset.

  *1.36 widens the first set from one ruleset to four and narrows the second
  from four to two*, because `dm` is OSP's `RegularDM` now. The contract is
  preserved, not redesigned — which is this rule's own closing clause — but an
  operator's hand-written `dm` config that sets `minimumplayers` stops being
  read, and that is the one migration this amendment cannot make silently.
  `configs/dm.cfg` carries the new name and `colosseum/README.md` says so.

  **The bot-*fill* switch is the exception, and 1.36 unifies it.** `ra_botfill`,
  `ctf_botfill` and `dm_botfill` became one bare `botfill` (R-RA-7, R-CTF-8,
  R-DM-1). They are not covered by this rule and never were: this rule governs
  cvars a **donor** named, so that each donor's own readme and configs keep
  spelling its cvars its own way. All three fill switches are Colosseum's own
  invention from amendments 1.34 and 1.35, appear in no donor's documentation,
  and name one concept — so §7 rule 6 governs them instead, and it says keep
  one. `BotFillCvar()` is deleted with them. Bots under the OSP four keep the
  behaviour the port already implements and it is more nuanced than
  "parity or match-only":
  bots ready themselves up after `bots_warmuptime`, join teams and are counted,
  but are already excluded from 1v1 (`bots_autoload == 2` zeroes
  `bots_minplayers`), from hi-scores, from team captaincy and from vote quorum,
  and bot management is already gated behind `serveronlybotcmds` and
  `vote_enable_bots`. That is the contract; it is preserved rather than redesigned.

  *And in 1.23 it turns out the port states the contract and does not implement
  the half that needs a bot to act.* A client under the OSP four connects as an
  **observer** and enters by pressing a key; nothing presses it for a bot, in
  this tree or in `osp-tourney`. The brain cannot — its whole client-command
  vocabulary is `say`, `say_team`, `use`, `drop`, `invuse`, `invdrop`, `wave`
  and `EA_Command` — and neither `bl_*.c` nor `bots.cfg` issues a join. So
  `OSP_botJoin` calls `OSP_startObserve`, the donor's own toggle, which already
  handles all four modes including `OSP_1v1AllowJoin` under `duel` and
  `OSP_addTeamMember(ent, 2)` under `tdm` and `duel`; and `OSP_botReady` implements
  `bots_warmuptime` as documented, with "all other real clients have readied"
  read as vacuously true on a server with no humans — which is the reading
  `OSP_ready_cmd`'s own "everybody left is a bot, start without waiting them
  out" shortcut already takes, and that shortcut was unreachable until something
  readied a bot. Measured across all four modes on `q2dm1`.
  `doc/reconciliation.md` R-105.
* **R-OSP-12.** *Tourney's four modes of play are four rulesets.* Until 1.36 the
  latched cvar `match_mode`, cached into the global `m_mode`, selected among
  four structures of play **inside** the `tourney` ruleset. They are now
  `g_ruleset` values in their own right — `dm`, `dmpro`, `tdm`, `duel` — and
  `match_mode`, `m_mode` and the name `tourney` are gone (R-MODE-1, R-MODE-2).

  What the four *are* is unchanged, and all four remain **inherited, not work**
  (§0 rule 6) — `osp-tourney@a8e30d0` implements them. Measured 2026-08-21, with
  the 1.36 name against each:

  | ruleset | was | `match_type` (serverinfo) | banner | structure | shipped configs |
  |---|---|---|---|---|---|
  | `dm` | `match_mode 0` | `RegularDM` | `*** REGULAR DEATHMATCH ***` | Plain FFA. No ready-up, `sync_stat` starts at 8. The only mode with hi-scores and with `qualifier_numspots` forced to 0. | 7 |
  | `dmpro` | `match_mode 1` | `QualifierDM` | `*** DM QUALIFIER ***` | FFA behind a ready gate; the top `qualifier_numspots` fraggers qualify and are starred on the scoreboard. Adds `qualifier_skinname` and `qualifier_forceskins`. | 1 |
  | `tdm` | `match_mode 2` | `TeamPlay` | `*** DM TEAM-PLAY MODE ***` | Two teams: captains, join codes, invites, team lock/unlock, `switchteam`, `kickplayer`, overtime. | 4 |
  | `duel` | `match_mode 3` | `1-vs-1` | `*** DM 1V1 MODE ***` | Duel. Two teams of one, forced. Adds a **spectator queue**: winner stays, the next in line has `team_nextuptime` seconds to claim the slot or forfeits it. Timeouts are per individual, not per team. | 3 |

  The banner block was `osp_main.c:564-586`; the mod documents the same four at
  `docs/server-settings.txt:413-419`. The banners and `match_type` survive the
  flattening — `match_type` is still published to serverinfo, derived from the
  ruleset now rather than from a cvar, because server browsers read it and
  `osp_stats.c` writes it into every JSON record.

  **`duel` is a first-class ruleset, not a corner of `tdm`**, and this is the
  clause that mattered for the port and now for the flattening: **34**
  `m_mode == 3` sites across **10**
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
  A merge that treats mode 3 as "mode 2 with two players" loses every one of
  them, and so does a flattening that gives `duel` `tdm`'s ops row and stops
  there. Measured again in 1.36 against this tree rather than the donor: **36**
  `m_mode == 3` sites, in a `m_mode` surface of **234** sites across **22**
  files, twelve of them shared files rather than `src/tourney/`'s own.
* ~~**R-OSP-13.** *`match_mode` is validated; every other bounded tourney cvar
  already is.*~~ **Struck in 1.36 — there is no `match_mode` left to validate.**

  The rule existed because the cvar that chose the mode of play was the one
  bounded tourney cvar the donor never range-checked, so the banner block's
  final `else` caught everything outside 0..2 and `match_mode 4` announced
  `*** DM 1V1 MODE ***`, set `match_type` to `1-vs-1`, and skipped all 34
  `m_mode == 3` branches: no queue, no forced `team_maxplayers 1`, team-scoped
  timeouts. **The server told its clients it was a duel server and was not
  one**, and a negative value did the same. It was never implemented until 1.21
  (`OSP_clampMatchMode()`), was found unimplemented by R-VER-16, and 1.34
  recorded that the donor reached the same finding independently in
  `osp-tourney@48408f1` — the two differing only on what an out-of-range value
  becomes (the donor clamps 4 → 3, this fell back to 0).

  The flattening removes the class of defect rather than the check. A mode is a
  `g_ruleset` value now, and an unknown one takes R-MODE-1's path: warn, fall
  back to `dm`, never abort. There is no numeric range to sit outside of, no
  banner block choosing on a bare `else`, and no way for the announced mode and
  the running one to disagree, because they are the same enum. `match_type` is
  derived from the ruleset, so serverinfo cannot drift from behaviour either.
  What R-OSP-13 was defending is now true by construction (R-MODE-1, R-OSP-12).

**R-OSP-14.** *Tourney's scoreboard is reached through the `ScoreboardMessage` row, never by name.* OSP **deleted** baseq2's `DeathmatchScoreboardMessage` and put its own five-page dispatcher under that name (`port_osp@205a89c` `p_hud.c:140-151`): `resp.osp_r24c` picks the player card, the MOTD, the match parameters or the previous match's scores, and only then does the mode pick a board. Three of tourney's own functions select a page and then draw it -- `OSP_setStats`' ten-second MOTD window, `OSP_highscores_cmd` and `OSP_showinfo_cmd` -- so each of them is `G_ScoreboardMessage()` here, because here that dispatcher **is** the row.

*The name is the trap, and that is why the rule is written down rather than left to whoever reads the donor next.* `DeathmatchScoreboardMessage` exists in this tree and is baseq2's grid board. A call to it from tourney compiles, links, runs and puts a scoreboard on the screen; the only thing wrong with it is that OSP has no such screen, so the MOTD window would answer a connecting player with an avatar grid and `highscores` would set `resp.osp_r034` for nobody to read. Neither is visible to a compiler or to any audit that asks whether a symbol exists. This row is therefore gate-only, and an exemption for it would be a claim about a donor that the donor's own source contradicts.


### 6.7 R-CH — Colored Hitman — **struck in 1.2**

Colored Hitman is out of scope (N7). R-CH-1..4 are struck and kept here per §6's
rule that a dropped requirement is struck rather than deleted.

* ~~**R-CH-1.** Colored Hitman is available as a `dm` modifier: `ch 1` with
  `ch_maxcolors` and `ch_colortime`, `gclient_t.chcolor`, the colour statusbar,
  `ColorImageName`, and the per-frame `UpdateColoredHitman` recolouring.~~
* ~~**R-CH-2.** The brain is told through the `ch` libvar (R-BOT-6).~~ The
  libvar survives, pushed as a constant `"0"` — see R-BOT-6.
* ~~**R-CH-3.** `ch` is refused with a message under `ctf`, `arena`, `tourney`
  and `sp`.~~ 1.2 replaced this with R-MODE-2 accepting a legacy `ch 1` and
  ignoring it with a warning; **1.36 removes that too**, so the cvar is read
  nowhere in the game library. Only the libvar of R-CH-2 survives.
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
* **R-EXTRA-5.** `VWEP` → visible weapons. *Measured in 1.24 and it needed no
  code and gets no cvar.* VWep is not a Gladiator patch in this tree: it is what
  id shipped from 3.20 on and what Q2PRO carries, so the 1999 `#define` was
  patching a **pre-3.20** baseq2 that this project does not have. The data the
  feature is stands in the merged `itemlist` -- **19 rows carry a `weapmodel`**
  -- `PutClientInServer` sets `s.modelindex2` and `ChangeWeapon` packs the model
  into the top byte of `s.skinnum`. A cvar here would be a switch to turn off
  something standard, which R-EXTRA's rule does not ask for and R-CORE-8 argues
  against. `sv extras` counts the rows so the claim is a measurement.
* **R-EXTRA-6.** `OBSERVER` → observer mode with the eye and chase cameras.
  ~~Reconciled with OSP's: one implementation, not two.~~ *Amended in 1.2:
  **three** implementations, one per ruleset,* the second exemption to §7 rule 6.
  Measured:

  | ruleset | implementation | lines |
  |---|---|---|
  | `dm`, `sp`, `ctf` | `gladiator-bot-restored/game/p_observer.c` + `camera_t` in `gclient_t` | 2,386 |
  | `tourney` | `osp_observe.c` + `p_camera.c` | 335 + 929 |
  | `arena` | RA2's four modes — `NORMAL`, `FREEFLYING`, `TRACKCAM`, `EYECAM` — with `ChangeOMode`, `track_think`, `eyecam_think` | in `arena.c`. *Live as of 1.17*, as `OMODE_*`: the bare names were global identifiers far too broad for a shared tree. `ClientThink` drives the two camera modes and jump/crouch cycles the subject; `p_view.c` keeps the observer's own body out of both the view origin and the screen blend |

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
  either way. *Recorded in 1.24, and the finding is that it **cannot be isolated
  from any tree in this workspace**.* Every Gladiator source here is at or past
  v0.96 and therefore already contains it: `gladq2_src/g_func.c`,
  `gladiator-bot-restored/game/g_func.c` and
  `q3a_bot_backport_for_q2/game_q2/g_func.c` are byte-identical at 3,260 lines,
  so there is no before-and-after to diff, and diffing any of them against id's
  own leaves style, yquake2's added null guards and Ground Zero's `#ifdef`s. The
  map is not in the retail set here either, so the trigger cannot be arranged.
  **One adjacent divergence is named rather than left implied**: yquake2's
  `plat_blocked` carries a "hack for entity without it's origin near the model"
  that neither id nor q2pro has, it is the only change to that function anywhere
  in this workspace, and it is consistent with the symptom the changelog
  describes. Colosseum does not take it -- §7 rule 1 gives q2pro the decision on
  a shared function -- and `doc/regression.md` records it so a future reader
  finds it rather than rediscovering it.

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
* **R-KEY-5.** *New in 1.38.* The fourth such contract is a LIFETIME rather than
  a name: **where a level-scoped allocation is indexed or pointed at by a field
  that survives the level, the index is cleared where the allocation is DROPPED,
  not where it is next read.** Reads additionally ask both halves of "is this
  index live" -- is it in range, and is the slot it names occupied -- through one
  named accessor rather than at each call site.

  `g_spawn.c`'s loop at `gi.FreeTags(TAG_LEVEL)` is where this rule already
  lived as a comment, at four sites: `menu_owner`, `ctf_menu`, `osp_menu` and
  `menustate`, plus RA2's `menuqueue`/`curmenulink`/`selected` -- "the free is
  what invalidates them, so the free is what clears them". The fifth is
  `teams[]`, TAG_LEVEL and reallocated empty by `arena_init()`, indexed by
  `resp.teamnum`, which is in `client_respawn_t` and survives because `g_save.c`
  carries it. It was not in the loop, and the accessor did not exist:
  `Serverwide_ScoreboardMessage` tested the range and not the slot,
  `Pickup_ScoreboardMessage` tested neither and would index `teams[-1]`, and
  `UpdateStatusBars` tested neither. `RA_TeamOf()` is the accessor now, and the
  clear is in that loop rather than at `arena_init()`'s TagMalloc, which is where
  1.38's first cut put it before this requirement was written.

  **Why this is R-KEY and not R-SEC.** No compiler and no three-way merge can see
  it, which is this group's criterion; and the window it opens stays invisible
  until something unrelated lengthens it. Here that was the bot layer:
  `BotStarted()` defers a bot's `ClientBegin`, which is where `InitClientResp`
  would have reset the field, so Phase 6 turned a 200-millisecond window into
  twenty seconds of frames on the first visit to a map whose `.aas` came out of
  bspc with an empty REACHABILITY lump. **The donor's unguarded reads were safe
  by accident in 1999 and became reachable when this tree added a feature RA2
  never had** -- which is the shape §7's rules cannot decide, because neither
  donor is wrong on its own. Reproduced and fixed in 1.38;
  `doc/reconciliation.md` R-186 §5 and `scenarios/ra2reachscore`.

### 6.10 R-SEC — security posture

The donors include two reconstructions that deliberately preserved their
originals' vulnerabilities, and a 1999 bot SDK with unbounded string handling.
Colosseum ships none of it.

* **R-SEC-1.** No unbounded copy of client-controlled data. Every `strcpy`,
  `strcat`, `sprintf` and `strncpy`-without-termination on a path reachable from
  userinfo, a client command, a chat string, an address string or a config file
  is replaced with a bounded, always-terminating form.

  *Enforced in 1.24 as a **ban** rather than as a reachability judgement, because
  reachability is not decidable and the direction a reviewer gets it wrong in is
  the one that matters* -- `sprintf(entry, "yv %d ", y)` reads as arithmetic and
  the netname three lines below it was always client-controlled. So the rule
  `tools/bounded.py` enforces (R-VER-30) is the stronger one: those five names
  do not appear in `src/` **at all**. 189 sites converted, of which three were
  live defects and all three were config-file or userinfo paths this requirement
  names by hand.

  **A ban on named functions is a floor, and it has a known blind spot**:
  `menu_centerprint` copied its argument into a 2048-byte stack buffer with a
  hand-rolled loop and no bound at all, which is none of the five names. It was
  found by reading R-SEC-2's ledger, not by the tool. Recorded here because the
  next one of that class will be found the same way.
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

  3. **On Windows the protector's runtime is linked IN, not imported** (added in
     1.26, and it is the only clause here that was found by trying to run the
     artifact rather than by building it). `-fstack-protector-strong` makes gcc
     add an implicit `-lssp`, and on mingw the link prefers `libssp.dll.a` over
     `libssp.a` — so `gamex86.dll` and `gamex86_64.dll` both declared
     **`libssp-0.dll`** as an import and **would not load** on any machine that
     did not happen to have a mingw runtime beside them. A game DLL that needs a
     compiler runtime shipped with it is not shippable.

     The answer is not to drop the hardening, which is what this requirement
     forbids: `libssp.a` is in the toolchain already, and
     `-Wl,-Bstatic -lssp -Wl,-Bdynamic` takes it. The protector is unchanged and
     the dependency is gone. Linking the archive adds one import,
     `ADVAPI32.dll` — `__stack_chk_fail` reports through the event log — and
     that ships with Windows.
* **R-SEC-6a.** *A PE artifact imports nothing but Windows.* `tools/pedeps.sh`
  reads the import table of every `.dll` the build produces and fails on any
  name outside the system set (`KERNEL32`, `ADVAPI32`, `msvcrt`, `USER32`,
  `GDI32`, `WS2_32`, `SHELL32`, `OLE32`). It runs **at link time**, in the
  build, for the reason R-TOOL-3 gives — and here the reason is sharper than
  usual: this failure is invisible from the host that produces it. The DLL
  links, the build is clean, ten configurations pass, and nothing in this
  workspace can load a PE image to discover that it wanted a fourth DLL beside
  it. Only running it on Windows finds that, which is what happened.
* **R-SEC-7.** No outbound network connection, no `system()`, no `exec*()`, with
  one exception: `autolaunchbspc` may launch the BSPC tool, it is **off by
  default**, and the path it runs is not client-controllable.

  *Discharged in 1.24, and it took a deletion.* RA2 arrived with a UDP event
  forwarder -- `netlog <host:port>`, one datagram per kill, connect and
  disconnect, through its own `gethostbyname`/`socket`/`connect` -- which is
  exactly what this requirement forbids and is not the exception it names. It is
  gone, along with three helpers that each called `exit(1)` on failure from
  inside a game library. `tools/noexec.py` (R-VER-32) is the check and it bans
  `exit()`/`abort()` for that reason as well. The `autolaunchbspc` exception
  holds as written: the cvar is empty by default, so the libvar is never pushed,
  and it is the **brain** that launches BSPC rather than this library.

  *Measured again in 1.27, and against this brain the exception is unreachable.*
  Even with the cvar set the spawn cannot happen: the branch is `#ifdef _WIN32`,
  it runs a `winbspc.exe` that has to be in the gamedir, and `SpawnProcess` in
  the reconstruction is a stub returning -1 (`botlib/botlib_port.h`). So the one
  hole this requirement admits is, on the brain Colosseum actually loads, closed
  by the brain — recorded because a posture claim should say what is true rather
  than what is permitted (`doc/reconciliation.md` R-126).
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

  *Both clauses discharged in 1.24, and the second one needed a qualification
  the rule as written does not carry.* Forty cross-file `extern`s left the `.c`
  files for headers -- four of them naming functions that exist nowhere -- and
  moving `Move_Calc` into `g_local.h` made gcc reject Ground Zero's own
  prototype on the spot, which had said `vec3_t dest` where `g_func.c` says
  `const vec3_t dest` since the Phase 2 merge. That is the rule paying for
  itself in one diagnostic.

  **The qualification: `extern` is a spelling, not a mechanism.** A bare
  prototype at column 0 in a `.c` declares another file's symbol exactly as
  `extern` does, and this tree has 250-odd of them, most of them baseq2's own
  spawn table. Moving every one into a header is a large diff for a property a
  tool can check directly, so `tools/externs.py` (R-VER-31) checks both: no
  cross-file `extern` in a `.c`, **and** every cross-file declaration -- either
  spelling -- must agree with its definition's types.

  **The pun clause is discharged by pinning rather than by rewriting.**
  `ra2menus.c` addresses `arena_settings_t` by index to `settings[41]` and
  `arena_t` carries a second inline copy of the same 42 members that two
  `memcpy`s write over; 44 `_Static_assert`s fix every index and the extent of
  the run, so a member inserted, reordered or retyped fails the build on the
  line that names it. The genuinely undefined pun -- a `float`'s address read as
  `int *` and stepped across a struct boundary -- is gone.
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

### 6.12 R-DM — deathmatch

*This group arrives in 1.35 and is the last one numbered for that reason: `dm`
had none, because R-BASE covers baseq2 **parity** and R-MODE covers the
dispatch, and until now every `dm` requirement was one or the other. The section
is appended rather than inserted so that no existing §6.x number moves.*

* **R-DM-1.** `botfill` — under `dm` and `dmpro` the bot count may follow the map
  instead of the server. Default **0**, which is off: `bots_minplayers` is the
  target exactly as it has always been, and nothing in the tree behaves
  differently.

  *Named `dm_botfill` in 1.35 and unified into one bare `botfill` in 1.36*, with
  `ra_botfill` and `ctf_botfill` — see R-OSP-11 for why those three were never
  covered by the per-ruleset naming rule, and §7 rule 6 for why one concept
  keeps one implementation. The switch is one cvar; the **target** is still
  computed per ruleset, because what a map can seat is a different question in
  each of them.

  R-RA-7's argument, a third time, and the plainest instance of it. A flat count
  is one number and a server plays a rotation: eight is a full house on `q2dm1`
  and four more bodies than `q2dm7` has anywhere to put. **Neither `dm` nor
  `dmpro` declares a capacity** — no `arena.cfg`, no `playersperteam`, and no
  `team_maxplayers`, because those two are the OSP family's *teamless* half —
  so the map is the only signal there is, and its spawn points are the map
  saying how many people it was built for.

  *1.35 wrote that as "**deathmatch** declares no capacity anywhere", which was
  a claim about baseq2's `dm`.* `dm` is OSP's `RegularDM` after 1.36 and OSP
  does have a declared capacity — `team_maxplayers` — so the sentence needed
  re-deriving rather than renaming. It survives, and the reason is narrower than
  it was: `team_maxplayers` sizes a **team**, and `dm` and `dmpro` have none.
  R-OSP-12's own table is the evidence — modes 0 and 1 are FFA, and every
  `team_*` cvar is inert there.

  **And this closes what `doc/reconciliation.md` R-156 deferred.** R-156
  recorded that modes 0 and 1 "have no teams and would fall to the same
  map-sized answer `dm` gets", and did not give them one, because R-OSP-11
  preserves tourney's bot contract rather than redesigning it. Once mode 0 **is**
  `dm`, that deferral cannot be restated — the same ruleset cannot both be the
  one the rule is about and the one it is deferred for. So `dm` and `dmpro` take
  the map-sized answer, `tdm` and `duel` take `2 * team_maxplayers` (the
  capacity they *do* declare, which is R-156's own preferred answer), and the
  bot contract is preserved in the only sense that survives the flattening:
  nothing changes for a server that leaves `botfill` at 0.

  The number is `G_SpawnPointPool()`'s and not the raw count.
  `SelectRandomDeathmatchSpawnPoint` finds the two spots nearest a player,
  refuses them, and draws from `count - 2`; the pool it can actually choose among
  is therefore two short of what the map carries. Measured over the eight `q2dm`
  maps: **8, 5, 5, 9, 7, 6, 4, 4**, against raw counts of 10, 7, 7, 11, 9, 8, 6,
  6 — and the first row is the count each of those maps is played at.

  Two things stated rather than left to be found. **`DF_SPAWN_FARTHEST` is not
  given the two seats back**, although its selector does use every spot:
  `dmflags` is not latched, so a target that moved with the flag would add two
  bots on the write and remove them on the next one, and a quiet answer two
  seats short beats a server that twitches. And **under `teamplay` the target is
  rounded down to even**, because two sides that cannot be the same size is the
  one thing a fill can get wrong for free.

  The ceilings and the roster-exhausted clamp are shared with R-CTF-8 and R-RA-7
  and live in `bl_spawn.c` — `BotFillTarget()` for the first, `BotFillNoMore()`
  for the second, cleared by `BotSpawn()` because a new level is a new question.
  *1.36 makes that sharing real*: until then `arena.c` carried its own copy of
  the same three clamp lines over its own `static int botfill_ceiling`, so "the
  ceilings are shared" was true of two rulesets and not of the third. The switch
  cannot carry a count, which is why the clamp is not written back into the cvar
  the way `bots_minplayers` is. `sv ruleset` prints one `botfill` line naming
  the ruleset, the target and where the number came from (R-VER-19).
  `doc/reconciliation.md` R-156, R-157.

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
   behaviour becomes the **base** branch of the dispatch. Which is which is not
   a judgement call: `git diff baseq2 port_<donor>` in the bundles of §3.2 is by
   construction that donor's own feature set and nothing else (R-PROV-3).

   *Said "the `dm` branch" until 1.36, and that stopped being true when `dm`
   became OSP's `RegularDM`.* The rule is unchanged — baseq2 still wins where
   the difference is not a donor's feature — but the place it wins into is
   `ops_base`, which no `g_ruleset` value selects. See R-MODE-6.
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

   Applies to: **grapples** (~~CTF grapple/hook, OSP hook, RA2's own — 3~~;
   *measured in 1.12 and the count is wrong: there is **one** implementation.
   `osp_hook.c` sets `CTF_GRAPPLE_STATE_FLY` by name and RA2 declares
   `ctf_grapplestate` and carries `CTFResetGrapple` verbatim — both are
   Threewave's, forked. So the resolution is not a merit choice between designs
   but one core plus three tunable sets: OSP's fourteen `hook_*` cvars and RA2's
   `allow_grapple` land as parameters on this code in Phases 5 and 4.
   `doc/reconciliation.md` R-50*),
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


  *Corrected again in 1.19, against the donor that defines all five names.* 1.7
  found it was six names on two bits rather than four; R-18 then read
  `FL_BOTCLIENT`, `FL_OSP_BOT` and `FL_OSP_NOCMD` as aliases of `FL_BOT` and
  dropped all three. Measured in `osp-tourney`, they are aliases of **two
  different bits**: `FL_BOT` is `BIT(13)` and so is `FL_OSP_NOCMD`, while
  `FL_BOTCLIENT` is `BIT(16)` and so is `FL_OSP_BOT` — the SDK's "this entity is
  a bot" and the mod's "this client is a bot", tested in different places for
  different reasons. Four names become **two**, not one: `FL_BOTCLIENT` is
  defined at `BIT(21)`. The instruction to remove duplicate aliases stands; what
  changes is which names were duplicates of what.
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
  | `slotkind.py` | R-OSP-7 clause 7 — ~~*and it has a known gap: its own docstring records that it missed RA2's slot 20*~~ *gap closed in 1.11: it asks about availability by slot number as well as kind, in both tree shapes, and carries five of its own controls*; *two more in 1.29 — a `SID_` inside `stats[]`, and an item `quantity` that must spell its id; seven controls (R-132)* |
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
  tree, and the `rr-cache` is committed — both the ~~496~~ **516** resolutions
  rescued per R-PROV-2a and everything this tree records subsequently. *496 was
  the two caches' DIRECTORY counts added, and one of those two was itself wrong;
  re-counted in 1.41 — 516 resolutions in 492 directories, because a merge whose
  conflict hashes collide across several paths stores them numbered inside one
  directory, and one holds 24. `doc/provenance.md` §1.3 gives all three figures
  and says which is which. 27 `thisimage` files went at the same time: rerere
  writes them for the conflict in front of it and never reads them back, so they
  are working state and not cache.* Note the rescued
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
  | the pristine-statusbar availability check | R-OSP-7 clause 7 and §9 Phase 3, as "closing `slotkind.py`'s known gap" | ~~`slotkind.py` exists and the gap is real, recorded in its own docstring. An extension, not a new tool~~ **done in 1.11**, and it was an extension: same tool, one more question |

* **R-TOOL-6.** *New in 1.40.* **A literal that names something the game
  looks up at runtime is resolved against the space that can produce it, and
  there is ONE RESOLVER PER SPACE.** Not one per file, and not one per literal
  *shape* — which is the mistake this requirement exists to prevent, because it
  has already cost twenty-seven years of a dead branch.

  Three of these spaces exist and each fails silently in its own way:

  | space | resolver | how a miss presents |
  |---|---|---|
  | `gitem_t.pickup_name` / `.classname` — what `FindItem()` and `FindItemByClassname()` match | `itemnames.py` (R-183) | NULL assigned is a weapon never selected; NULL through `ITEM_INDEX` is a negative subscript into `pers.inventory[]` |
  | `edict_t.classname` — what the spawn table, the itemlist and every `->classname =` assignment can produce | `classnames.py` (R-188) | a `strcmp` that is always false, so the branch it guards is dead and control falls to whatever the chain's final `else` does |
  | model, sound and image paths — what a donor actually ships | `assets.py` (R-184) | `gi.soundindex()` cannot check, so a misspelled sound is silence that looks like a design choice |
  | **the set of values a FIELD can hold** — every constant any write in the tree, or in a donor, produces | `deadvalue.py` (R-192, *new in 1.44*) | a comparison that is always false, so a whole feature does the other thing forever: R-191's `osp_r240 == 2`, R-155's `pers.showmotd`, R-192's five-page scoreboard whose fifth page nothing could open |

  *1.44: the fourth space is STATE rather than a name, which is why it took three
  amendments to see.* The first three resolve a literal against a table; this one
  resolves it against the writes, and it has a second question no other resolver
  has — **against a DONOR**, because the donor is the authority on which values a
  field is supposed to take. A merge drops a write far more easily than it drops
  a reader: the reader still compiles.

  **The two classname resolvers are not redundant and the overlap is the trap.**
  `itemnames.py` collects literals by SHAPE (`weapon_*`, `ammo_*`, `item_*`,
  `key_*`) and resolves them against the ITEMLIST. Both halves are narrower than
  they look: a classname belonging to a monster, a projectile or a runtime
  assignment is in neither space, and a bare `"tesla"` is not even collected.
  Ground Zero's `"telsa"` in `m_move.c` sat in that gap — misspelled, compared
  against `ent->enemy->classname`, invisible to a check that asks the itemlist
  about `item_*` names (R-187 §2). So `classnames.py` asks the other question:
  can ANY code path put this string in an `edict_t.classname`?

  Each resolver carries its own controls and each runs in the build (R-TOOL-3),
  and a resolver's exemption list is checked for going stale — an exemption for a
  name that no longer appears, or that the space can now produce, is itself a
  finding. That is not a nicety: an exemption list is how a check dies quietly,
  and both classname resolvers exempt the same three Ground Zero map aliases for
  the same reason, which is exactly the kind of duplicated licence that rots.

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
R-MODE-5 including `ClientPlaced`, the base ops implementing all of them.
(1.0–1.3 said 25 here; the list has always had 26. Said "`dm` implementing all
of them" until 1.36, when `dm` became a ruleset like any other and `ops_base`
became the table that fills every row.)
**Exit — met 2026-08-21.** R-MODE-1..7, each observed live via `sv ruleset`
(R-VER-18): an invalid `g_ruleset` warns and falls back to `dm` without aborting;
`ctf 1` and `rocketarena 1` select their rulesets and conflicting aliases warn
and resolve in R-MODE-2's order (`ch 1` was accepted and ignored until 1.36
removed it); both content
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
**Exit — met 2026-08-21.** R-CTF-1..7, R-MENU-1..5, R-OSP-7 and R-OSP-7a, with
one clause of R-CTF-3 and one of R-MENU-4 explicitly deferred to the phase that
can check them rather than claimed here.

*The slots landed first*, because the merge needs somewhere to put 17, 18 and 19
before it can put anything there: R-OSP-7's map as `src/g_stats.h`'s X-macro with
`p_hud.c` off its two `#define`s, R-OSP-7a's `sb_*` emitter with the `dm` bar
verified **token-identical** to the two literals it replaced, clause 7's kind
check, and `slotkind.py`'s gap closed — with five controls that ship inside the
tool and run in `make check`. `sv slots` (R-VER-19) is how any of it is
observable from outside the library, and it is what caught the emitter dropping
an `if` while keeping its body (`doc/reconciliation.md` R-49).

Then CTF: 20 shared files merged, 4 donor-only into `src/ctf/`, the ctf row of
the dispatch, its own `pmenu_t` engine with `ctf_` symbols, and the grapple as
the one implementation of its concept per §7 rule 6 — with R-CTF-3's offhand
`ctf_hook` and `laserhook` written from uGladQ2's behaviour rather than copied
(N2).

**Evidence.** Ten build configurations clean under `-Werror` from `make clean`,
the native one additionally under `clang` (R-BUILD-9 is why that sentence now
names `make clean`). `make check` clean: six audits, no vacuous rows — `auditems`
now runs in the build against `q2pro/src/game` with two recorded accepted
differences. R-VER-2's boot matrix: all **20** combinations boot, run and shut
down clean. Q14's added row: `ctf` × monsters on `base1`, `badlands` and `rmine1`
in all four layer combinations — **twelve boots, no finding**, and the reason is
visible in the hunk census (R-40): CTF never edited the monster code, it deleted
it. R-VER-17 passes under `ctf` and still under `dm` and `sp`, including the
cross-process savegame round-trip that is the only end-to-end check on the
regenerated `save_ptrs[]`. `gates.py`'s budget did **not** rise: 165 inherited
`deathmatch`/`coop` sites before the merge and 165 after.

R-VER-10 is discharged for this donor: CTF's three *re-apply* rows are landed
and marked `re-apply — done`, and one of them found an unguarded third
`G_PickTarget` call site that Phase 2 had introduced (R-53).

*Re-verified in 1.13 against real Threewave assets.* All nine shipped CTF maps
boot with both flags at base, all four techs in the world, team spawn points and
banners present, a 658-byte composed bar and a clean shutdown — R-CTF-1 measured
rather than argued. That test also found R-54: **every think in the tree fires
ten times late**, from a `SV_RunThink` the Rogue merge reverted in Phase 2. It is
upstream's defect, it is not CTF's, and it is recorded rather than patched
because the fix is one coherent pass that also has to convert Xatrix's 43
`level.time` sites — see R-VER-20 for why no check caught it for three phases.

**What is not claimed.** **R-CTF-4** is `botctfteam` and the bot menu, which §9
Phase 7's own exit list also names — it belongs to that phase, and listing it
under "R-CTF-1..7" here is the spec double-counting rather than a gap. Both
halves of R-CTF-7 whose trigger is a *client* — the v0.95 bot-model bug and the
v0.93 userinfo bug — are structural and reviewed but unobserved, because this
harness has no clients; they are Phase 7 rows too. R-CTF-3's `laserhook`
**libvar** and R-MENU-4's "bots' `showscores` forced off" are Phase 6, for the
same reason: there is no brain to tell and no bot to force. R-MENU-1 ships one of
its four engines, so the contention it exists to police is not yet observable —
what is in place is the arbitration that has to precede the second engine. And
no CTF map was played: the retail paks here have no `q2ctf*`, so flag capture,
tech pickup and the grapple are exercised as code paths and boots, not as a game.
`doc/reconciliation.md` R-40..R-53.

*And overtaken again in 1.15: the harness has a client.* `tools/play.sh` drives
q2pro's own client headlessly — `Xvfb`, `cl_beginmapcmd`, `wait`-paced console
input, `viewpos` for position — and its first run dropped the server with
`PF_configstring: bad index: 13118`, a `CS_GENERAL` literal read against a
runtime remap that a stock server does not extend (`doc/reconciliation.md` R-62).
Three sites fixed, a fourth of the same kind found by reading, all four
upstream's. Phase 3's last mechanised residue also lands: `tools/allocpairs.py`
(R-VER-22), the matched-pair allocator audit R-55 asked for, with three controls
— `make check` is now **11 audits**, all clean. **A played CTF match is still not
claimed**: a capture, a tech pickup and a grapple swing need drive scripts that
navigate, and what 1.15 delivers is the capability plus the first defect it
found, not the match.

*Both of the last two are overtaken by 1.13 and 1.14.* Retail Threewave assets
are in place and all nine shipped maps run with both flags at base and all four
techs (R-CTF-1 measured). And after importing `q2pro@38873740` the whole matrix
was re-run: ten configurations clean from `make clean` on gcc and clang, nine
audits clean, R-VER-2's 20 combinations clean, R-VER-17 under `sp`/`ctf`/`dm`
with the cross-process savegame round-trip, and R-VER-20's two temporal
assertions passing — CTF's techs now appear at frame 31 rather than frame 221.
`doc/reconciliation.md` R-54..R-60. What is still not claimed: a *played* CTF
match — a capture, a tech pickup, a grapple swing — which needs a client.

### Phase 4 — Rocket Arena 2
`src/arena/`, RA2 onto the dispatch, its own menu engine and its four observer
modes, the uGladQ2 fix list verified item by item, the local round log. Fix the
double map rotation (R-OSP-9).
**Exit:** R-RA-1..6, R-ARENA-1..4.

**In progress as of 1.16.** *The slots landed first again*, for the reason
Phase 3 found and this header states: a map that renumbers a slot while the bar
still says 18 is worse than inheriting, so arena's column and arena's bar had to
land together. Done: R-OSP-7's ten arena rows measured off the donor's own
statusbar literal, R-OSP-7a's arena bar **token-identical** to that literal plus
the second powerup timer at 26/27; `src/arena/` imported and building — 5,300
lines across six translation units — with the six symbol collisions of
`doc/reconciliation.md` R-67 resolved by rule rather than preference; the
`g_local.h` union; and R-26's sixth rule mechanised as R-VER-24 after four
monster-API calls crossed the merge undetected.

*All sixteen shared-file merges landed in 1.17*, along with the `arena` spawn
key, `arena_init()`, the savegame descriptors, two dispatch rows (`EndLevel` and
`ScoreboardMessage`) and R-OSP-9's fix. `arena` no longer inherits `dm`: it
frees every pickup item, enforces team skins, runs its own scoreboard and its
own four observer modes, and boots `q2dm1` at 37 edicts against `dm`'s 120.

*R-RA-3 and R-RA-4 discharged in 1.18.* `RA_CheckRules` is the `CheckRules` row
and drives the seven-state round machine ahead of the level-scope limits;
R-RA-4's seventeen items are verified one at a time in `doc/regression.md` --
thirteen already true, three Phase 7's, one real gap fixed (observers were
counted when choosing a spawn point). And **no RA2 specific is ungated**, which
is a check now rather than a claim: `tools/donorgate.py`, 77 uses, 0 ungated.

*R-RA-2 measured in 1.19*: **all 39 client commands and all 48 cvars resolve in
this tree**, compared name by name against the donor rather than against the
readme. 48, not 47 — R-TOOL-2 makes the tool the authority and §3's counting-
method note already warned that every published donor figure has disagreed.

**Remaining.** R-RA-5, R-ARENA-3 and the three bot items of R-RA-4 belong to
Phase 7 with the bot layer. And **no arena round has been played** -- the machine
runs, the menus build, the queues fill, and none of it has had two clients in it,
which needs the same drive scripts R-VER-23 is waiting on.

### Phase 5 — OSP Tourney DM
`src/tourney/`, tourney onto the dispatch, its own observer/camera, menu engine
and map rotation kept per-ruleset rather than reconciled away, the
`statsfile`/`statsname` collision resolved (R-COMPAT-6).
**Exit:** R-OSP-1..13, R-VER-16 — with R-OSP-11 shared with Phases 6 and 7,
which is where its bot cvars and its bot behaviour land.

**In progress as of 1.19 — the import, not the merge.** Done: 20 donor-only
files into `src/tourney/` (17,300 lines) plus the SDK headers into `src/bot/`;
the `g_local.h` union, including 80 offset-named `client_respawn_t` members that
are carried verbatim because they are live state rather than padding (R-79);
R-OSP-7's eleven tourney rows with the runes at 22–26 and the timer pair at
29/30; R-CORE-14's flag allocation corrected; and five defects the import
surfaced — three of R-58's class, eleven hardcoded configstrings and a lost
timer scale (R-80, R-81).

**Remaining, and it is the bulk: 26 shared-file merges, 171 conflicts of which
128 need a hand resolution** — `p_client.c` (+1450 lines), `g_local.h`'s
behaviour half, `g_cmds.c` (+533), `g_main.c` (+502), `g_items.c` (+472),
`g_spawn.c` (+401) and twenty smaller. Until they land **`src/tourney/` is
compiled but not linked** (R-VER-26) and no tourney code runs: `g_ruleset
tourney` boots and plays as `dm`. Also outstanding: the dispatch rows, tourney's
four statusbars (R-OSP-1's four `m_mode` values, R-OSP-12), R-COMPAT-6's
`statsname` collision with RA2's, R-OSP-5's `botglobals` check, R-OSP-6's fifth
spawn key, and R-OSP-2's 259 cvars and 137 commands measured the way R-RA-2's
were.

**Complete as of 1.21, with one requirement explicitly carried.** All 26
shared-file merges landed and every one of tourney's exported entry points is
reached: the measure used was "how many of the donor's 349 exported functions is
nothing calling", which was **62** before the increment and is **0** after.
`src/tourney/` is linked (R-VER-26 discharged and `check-tourney` deleted, as
that requirement asked), the dispatch rows are filled — `CheckRules`, `EndLevel`
and `ScoreboardMessage` — and R-OSP-12's four statusbars come from R-OSP-7a's
emitter with two booleans rather than four literals. R-OSP-2 is measured name by
name (137 of 138 commands, 199 of 237 cvars, every absence accounted for),
R-COMPAT-6's collisions are resolved, R-OSP-5 and R-OSP-6 are discharged, and
R-OSP-13 — which was never implemented — is implemented and checked.

**R-OSP-11 is NOT discharged here and is not claimed.** Its tourney half is
present: all seven `bots_*` cvars are registered with the donor's defaults. Its
other half — "under every other ruleset they are `minimumplayers` and `botfile`"
— cannot be, because both of those are registered by a `bl_*.c` file that
arrives with the bot layer, and registering them from a tourney file to make a
count come out right would put the wrong owner on them. The behaviour half
(ready-up after `bots_warmuptime`, exclusion from 1v1, hi-scores, captaincy and
vote quorum) needs a bot to exist at all. **R-OSP-11 belongs to Phases 6 and 7**,
where §9 also lists it.

**The author's own play test is still outstanding and is not replaced by any of
this.** `tools/playtest.sh` asserts what a program can assert; no CTF match, no
arena round and no tourney match has been *played*.

### Phase 6 — Bot layer
`src/bot/` from `osp-tourney`'s ported `bl_*` plus `p_menulib.c`/`p_botmenu.c`
from the Gladiator reconstruction; **the 17 live TOURNEY blocks re-expressed as
runtime branches (R-BOT-29)**; the flag-bit reallocation (R-CORE-14);
redirection, fake clients, the frame loop, commands, `bots.cfg`, the bot menu,
the debug-draw and filesystem extensions, the ruleset → libvar mapping.
**Exit:** R-BOT-1..30. Bots load, spawn, navigate, fight and chat in `dm`.

*Landed in 1.22, and the exit criterion is met.* Eight translation units in
`src/bot/`, `src/tourney/osp_botseam.c` deleted rather than edited, all
seventeen `TOURNEY` blocks converted, `GetGameAPIEx` implemented for R-BOT-8 and
R-BOT-26, ten build configurations clean on gcc and clang. **Bots load, spawn,
navigate, fight and chat in `dm`** — the brain computes and writes its own
`.aas`, three bots on `q2dm1` trade frags, and ten chat lines in a minute
including the `exit_game` pair. R-VER-3 is **8 rows, 8 passed** against
`gladiator-bot-restored 57ce85a3`, which fixed R-98's two truncations upstream
along with eight more of the same class, and withdrew R-97's branch (R-101).

Getting there cost three contract defects and one of ours: the `Trace` calling
convention (R-97), two struct sizes (R-100), and an inverted `fastchat` push
that made the chat half unobservable (R-102). Phase 7's own rows — `ctf`,
`arena` and `tourney` bot *behaviour* — are untouched, as intended; bots spawn
and are removed cleanly in all four rulesets.

**Four rows remain open and are named rather than implied:** R-VER-5's
slot-exhaustion test, R-VER-6's index-limit test, R-VER-7's demo comparison, and
R-BOT-3's optional static-link build.

### Phase 7 — Bots across the rulesets
Bots in `ctf`, `arena` and `tourney`; `minimumplayers` vs `bots_minplayers`
(R-OSP-11); bot cycling; team and arena assignment.
**Exit:** R-MODE-7's bot row in full, R-CTF-4, R-RA-4/5, R-OSP-4/5/11.

*Landed in 1.23.* Bots join a CTF team from `botctfteam`, an arena's waiting
queue from the `arena` key, and a tourney match through the donor's own
`OSP_startObserve` in all four `match_mode`s, readying themselves up per
`bots_warmuptime`. `ra_playercycle` and `ra_botcycle` are the two switches over
RA2's queue (R-RA-5), `CheckMinimumPlayers` runs under `arena` (R-MODE-7's bot
row), and R-MENU-4's second half -- the v0.91 "no layout for a bot" fix -- lands
with them.

**Evidence.** Ten build configurations clean under `-Werror` from `make clean`
on gcc, plus the native pair on clang; `make check` 17 audits; boot matrix
20/20; smoke ok; **play test 171/0**, of which 32 are a real client watching
bots; **bot matrix 13/13**, of which the ctf/arena/tourney rows now assert
PLACEMENT rather than reporting it, plus R-VER-6's two index rows. Four
controls fire in the bot matrix, three in the boot matrix, two in the smoke
test.

**Six defects, four of them older than this phase**, and the pattern in every
one is a donor statement the merge did not take: RA2's ten post-memset
assignments in `InitClientResp` (of which `teamnum = -1` made
`R-RA-4`'s first row structurally false since Phase 4), tourney's *partial*
`InitClientPersistant`, `OSP_gameInit` before `game.maxclients`,
`remove_from_team` in `ClientDisconnect`, R-58's split missing twelve sites, and
`ReadGame` not re-establishing the bot layer's TAG_GAME memory --- which
segfaulted every savegame load while three scripts read the console and none
read the exit status. `doc/reconciliation.md` R-103..R-112.

**What is not claimed.** R-BOT-3's optional static-link build is still not
implemented and nothing needs it. And **no person has watched a bot play**: the
play test is a client with assertions, not a pair of eyes, and R-VER-20's door
and item-respawn temporal checks still want `tools/play.sh` and a human.

### Phase 8 — Hardening and staged exposure
R-SEC sweep, the regression checklist, the docs of D3–D9, the extras of §6.8,
performance measurement. Then a private server, then widened access — there is
no analytical release gate (R-SEC-2).
**Exit:** R-SEC-1..9, R-EXTRA-1..7, R-BOT-23, D2–D9 complete.

*Landed in 1.24.* **R-SEC-1..9 are discharged and three of them are now checks
rather than claims**: `bounded.py` (R-VER-30) bans the five unbounded copies
outright, `noexec.py` (R-VER-32) bans sockets and processes, and `externs.py`
(R-VER-31) enforces both halves of R-SEC-8. 189 copy sites converted, RA2's UDP
event forwarder deleted, 40 cross-file `extern`s moved into headers, 44
`_Static_assert`s pinning `arena_settings_t`'s index view, and six live defects
fixed — four `arena.cfg` overflows, a forced team skin appended to the engine's
userinfo buffer, RA2's menu value blocks, two out-of-bounds indices, an
undefined `float*`→`int*` pun, and `menu_centerprint`'s unbounded stack copy,
which the ban could not see and the ledger could.

**R-EXTRA-1..7 all land**, six as cvars and the seventh — VWep — measured rather
than implemented, because it is id's from 3.20 and not Gladiator's patch. The
observer is 2,386 reconstructed lines for `dm`/`sp`/`ctf`, and under `ctf` it
supplies the cameras while Threewave keeps the verb. R-EXTRA-7 is recorded as
not isolable from any tree here, with the one adjacent divergence named.

**R-BOT-23 is measured**: 32 bots, 300 frames, mean 2613 us and worst 10785 us
against a 50000 us budget. **D2–D9 are complete**, D6 shipping as `colosseum/`.
**R-SEC-2's two RA2 ledgers are entered item by item** in `doc/regression.md`,
in this tree rather than inferred from a pin, and R-BOT-3's optional static-link
build is **struck** rather than carried a fourth time.

**Evidence.** Ten build configurations clean under `-Werror` from `make clean`
on gcc plus the native pair on clang; `make check` 23 audits; boot matrix 20/20;
smoke ok; play test **187/0**; bot matrix **14/14** including R-BOT-23;
`tools/extras.sh` **41/0** including R-VER-20's second temporal check — an item
taken by bots and given back once nobody is left to take it again — and, since
1.25, the two that count a client's key bindings.

**Re-measured 2026-08-26 for 1.27, from `make clean`**, because 1.26 changed
`src/bot/p_observer.c` and two `trigger_teleport` paths and recorded only the
build: ten configurations clean under `-Werror` with **zero warnings** and the 23
audits clean in each; boot matrix **20/20** (3 controls fired); smoke pass (2);
play test **193/0**; `tools/extras.sh` **41/0** (5 controls) — **42/0** with 6
once R-129 added the D6 content check later the same day; bot matrix **14/14**
(4 controls). R-BOT-23 came back **mean 2370 us, worst 16292 us**
against the same 50000 us budget. Two figures differ from the paragraph above and
that paragraph is left standing, because what Phase 8 landed with is a fact about
Phase 8: the play test grew from 187 because its scenario lives in the
out-of-tree `q2-playtest` skill and moved without a commit here (R-127), and
R-BOT-23's worst frame is half again 1.24's 10785 us on a host that was building
at the same time — a measurement that moved rather than a regression, and the
row's own verdict is ok either way.

**A person has now watched it, and that immediately found four things.** The
play test is a client with assertions; `tools/watch.sh` hands the judgement to a
person, and the first three runs produced: a client with no key bindings at all
(D6's `default.cfg` shadowing id's, 1.25), players with no models (`players/` is
in no pak, 1.26), an observer whose controls looked broken and were a camera
(1.26), and — from actually running the Windows build — a DLL that would not
load without `libssp-0.dll` beside it (R-SEC-6a, 1.26). **Not one of them was
visible to anything in this repository**: a dedicated server has no bindings to
lose, never loads a model and cannot run a PE image, and `libq2` has no keyboard
and no renderer. That is the argument for R-VER-9's surviving clause in one
paragraph.

**A second session, on Windows, found two more, and neither is a defect here.**
The console pauses the server because q2pro's proxy for single player is a single
connected client and bots are fake clients that never reach its client list
(R-125); and no bot would load at all because the brain needs an `.aas` per map,
there is no auto-bspc to make one, and nothing documented where the brain's
assets go (R-126). Both are fixed where they live — a client cvar in the
watcher's config, and three documents — and the second turned out to reproduce
`botmatrix.sh`'s own second control by hand. The door
half of R-VER-20 stays a person's verdict because nothing here distinguishes
"the door opened" from "the player walked forward". Staged exposure
(`doc/exposure.md`) has stage 0 in front of it and nothing behind it.

---

## 10. Verification

**Which check answers for which change.** The five run-time scripts are not
interchangeable and running all of them is minutes rather than seconds, so this
is the targeted map. It is recorded here in 1.27 because it existed only in a
hand-off note that nothing in this tree read (`doc/reconciliation.md` R-127):

| changed | run |
|---|---|
| `src/g_log.c`, `src/p_lag.c`, `src/bot/p_observer.c`, `g_trigger.c`, `g_func.c`, `colosseum/` | `tools/extras.sh` — R-VER-33, and D6's shipped configs |
| `p_client.c`, `p_hud.c`, or any menu | `tools/playtest.sh` — R-VER-27 |
| `src/bot/`, `src/arena/` | `tools/botmatrix.sh` — R-VER-3 |
| anything at all | `make` — the audits and the PE import check run in the build rather than on request (R-TOOL-3, R-SEC-6a) |

`tools/bootmatrix.sh` (R-VER-2) and `tools/smoke.sh` (R-VER-17) are not in the
table because they answer for the whole library rather than for a file: every
amendment runs them. None of this is a substitute for the full sweep an
amendment needs — R-VER-9's "you build and run" — it is what makes a one-file
change checkable without one.

* **R-VER-1.** `doc/regression.md` holds one entry per historically-fixed bug
  named in this spec — the Gladiator v0.91–v0.96 changelog, the uGladQ2
  v0.96u–v0.99u list, the RA2 kept-bugs set, R-OSP-4 — with the symptom, the
  trigger and the check. An entry with no check is not done.
* **R-VER-2.** Boot matrix: every `g_ruleset` × `xatrix` × `rogue` combination
  (**7 × 2 × 2 = 28** since 1.36, 5 × 2 × 2 = 20 before it) starts, loads a map
  appropriate to the ruleset, and runs 100 frames with no crash and no
  assertion. *Mechanised in 1.20 as
  `tools/bootmatrix.sh`, having been run by hand once per phase, which is how a
  matrix stops being run.* A row passes only if the server also **reports back
  the ruleset and the layers that were asked for** — the boot alone proves the
  process survived, not that the resolution happened — and it ships two positive
  controls (`--control`): an unknown ruleset, which must be seen falling back to
  `dm` — and since 1.36 that control covers the retired name too, because
  `g_ruleset tourney` is now exactly an unknown value and nothing more
  (R-MODE-2) — a layer comparison against the wrong value, and — new in 1.23 — a boot
  that completes its census and is then killed by `SIGSEGV`, because **the row
  now reads the exit status too**. The frame count already caught a crash before
  the census; a crash after it left every field correct, which is exactly what a
  savegame load did in every configuration (`doc/reconciliation.md` R-108). *Q14's row added in 1.12 and measured:*
  `ctf` × monsters on a map that has them, which means the first map of all three
  campaigns × all four layer combinations — twelve further boots. Passed;
  `doc/regression.md` has the counts. Q14 expected findings and there are none,
  which the hunk census explains rather than leaves lucky: CTF contributes no
  feature hunk at all to `g_ai.c`, `g_monster.c`, `g_combat.c` or `g_turret.c`.
* **R-VER-3.** Bot matrix: for each ruleset that accepts bots, 1 bot and 16
  bots spawn, play for 5 minutes and disconnect cleanly; `botlibdump` and
  `clientdump` show no leaked library reference and no leaked client slot.
  *1.15: the human half of this no longer waits on the harness.* R-VER-23's
  `tools/play.sh` connects real clients, so "1 bot and 16 bots" can be "1 bot,
  16 bots and a driven human", and R-BOT-15's slot relocation (R-VER-5) has
  something to relocate *for*.

  *Mechanised in 1.22 as `tools/botmatrix.sh`*, which needs more than the other
  two run-time scripts: a **brain** built from `gladiator-bot-restored/botlib`,
  its `bots.cfg` and the character files that roster names (in the Gladiator
  assets' `pak7.pak`, read through the botlib's own file search and not the
  engine's), and an `.aas` per map. It ships two positive controls — the brain
  removed from the gamedir, and the brain present with the `.aas` taken away,
  which is the `BLERR` path where every bot using the library is destroyed and
  the leak check must still pass. **It also tells two findings apart**, because
  "the game leaked a slot" and "the brain died being asked to let go of one" are
  not the same row: `sv clientdump` brackets the removal and one dump instead of
  two means the process died inside it. `FRAMES` defaults to one minute of game
  time; this requirement's five is `FRAMES=3000`. **8 rows, 8 passed** as of
  1.22, against `gladiator-bot-restored 57ce85a3`. It read 0 of 8 for one
  afternoon, every row `brain died in BotShutdownClient (R-98)`, and the
  classifier is what made "not our defect" a checkable claim instead of an
  assertion — the same run with two casts removed from a throwaway copy of the
  brain passed all eight, which is what was reported upstream and what came back
  fixed.
* **R-VER-4.** Savegame matrix: for each campaign combination, save and reload
  at three points, and verify entity count, client state and pointer fixup.
* **R-VER-5.** Slot-exhaustion test: fill every client slot with bots, then
  connect a human, and verify a bot relocates (R-BOT-15) rather than the
  connection being refused.

  *Run in 1.23, and it is **two** servers rather than one.* Measuring the
  requirement shows that its "rather than" is a false alternative:

  * a bot **relocates** when the slot the engine hands the human is a bot's
    *and* another slot is free. `G_SpawnClient` scans downward, so bots take the
    highest free slot (R-BOT-14) and the engine hands a human the lowest free
    one; they meet only after a high-slot bot has left. The check arranges that
    by name out of `sv clientdump` -- quoted, because the roster's names are
    "Java Man" and "Steroid Stud".
  * with **every** slot taken there is nowhere to move to, and R-BOT-15's own
    answer is to refuse the human rather than steal the slot, because the brain
    would otherwise be talking about a client that is now somebody else. The
    Gladiator donor's `ClientConnect` does the same.

  Both rows are in `tools/playtest.sh`, because both need a real client. The
  first also checks that `FL_BOT` and `FL_BOTCLIENT` survive the move together
  (R-CORE-14). `doc/reconciliation.md` R-110.
* **R-VER-6.** Index-limit test: on a map that precaches near the old 256-model
  limit, with and without protocol extensions, verify the bot index tables hold
  and that overflow is reported rather than written (R-BOT-11).

  *Run in 1.23 as two rows of `tools/botmatrix.sh`, and half of it cannot be
  reached by a server.* The tables are sized from `game.csr` -- 8192/2048/2048
  with the protocol extensions negotiated, 256/256/256 without -- so the engine
  **cannot issue an index the table has no room for**. That is R-BOT-11 working
  rather than a gap in it, and it leaves a check that only ever sees silence and
  cannot tell "it did not overflow" from "the reporting is broken". So
  `sv indexprobe <model|sound|image> <index>` asks `BotIndexRecord` the question
  directly, writes nothing, and is what both directions of the control drive.

  *And "near the old 256-model limit" is not reachable with the content on this
  machine.* The heaviest retail map under a ruleset that keeps monsters is
  `command` under `ctf` with both mission-pack layers: 171 models and 170
  sounds. Bots add none -- a bot's skin is a configstring, not a modelindex. The
  headroom is printed rather than hidden, because a row that says "no overflow"
  on a map using two thirds of the table is weaker evidence than one on a map
  using all of it. `doc/reconciliation.md` R-110.
* **R-VER-7.** No-bot transparency test: with `numbots == 0`, a recorded demo of
  a session must be byte-identical to one recorded by a build with the
  redirection compiled out (R-BOT-13).
  **Not run as of 1.22, and the reason is worth stating rather than deferring
  silently.** R-93 changed the staging message from bytes to typed records
  replayed through the engine's own writers, which makes the redirected call
  sequence *identical by construction* rather than equal by measurement — the
  engine encodes, in the same order, in both cases. That is a stronger argument
  than a demo diff, and it is also not a check: it cannot fail, so it cannot be
  trusted the way R-VER-9 clause 2 means. The demo comparison is still owed, and
  the build "with the redirection compiled out" it needs does not exist — the
  layer has no `#ifdef` (§7 rule 6), so producing one means a second Makefile
  target whose only purpose is this check.

  ***Withdrawn in 1.23, with the reason, rather than carried a fourth time.***
  The comparison asks for two builds to agree, and the second build cannot be
  produced without either an `#ifdef` in the redirection layer — which §7 rule 6
  forbids, and which would make the shipped and the measured library different
  code — or a link-time stub target that exists for nothing else. Neither is
  worth a demo diff whose result R-93 already determines: the redirection stages
  **typed records**, and both the redirected and the direct path hand them to
  the same `gi.Write*` in the same order, so the byte stream is identical by
  construction. What the requirement was defending against is that the
  redirection *re-encodes* — which is precisely what R-93 stopped it doing, and
  which `botabi.py` would now catch as a contract change.

  This is a withdrawal and not a pass. The residual risk it leaves is that a
  future edit reintroduces re-encoding inside the layer and nothing measures it;
  the mitigation is R-BOT-13 stated as a property of `bl_redirgi.c` — the layer
  may stage and replay, never encode — and §11's risk register carries it.
  R-VER-9 clause 2 is satisfied by *saying so*, which is the whole of its point:
  a check that cannot fail must not be recorded as one.
* **R-VER-8.** Build verification: all four configurations, zero warnings, under
  **both** `gcc` and `clang` (R-BUILD-6), on every gating target of R-BUILD-5 —
  ~~x86, x86-64 and aarch64~~ *re-scoped in 1.4* to all **five**: ELF aarch64,
  ELF x86-64, ELF i386, win32 PE and win64 PE. The reference machine can build
  all five and execute only the first. Measured 1.5: **all ten configurations
  build clean under `-Werror`**, and the native one additionally passes R-VER-17.
  The other four are recorded as **built, not run** — never as passing on the
  strength of a clean link. *Three of the four still are, as of 1.27:* **win32 PE
  has been run** — Windows 10, `q2dm1`, one client, the brain set up — which is
  the first execution of a non-aarch64 artifact here and is what found R-SEC-6a's
  defect. It ran without an `.aas`, so `BotLoadMap` refused the map and the bot
  ABI (R-BOT-4, R-BOT-5) has still never crossed on x86; win64 PE, ELF x86-64 and
  ELF i386 remain built and unrun. The two-compiler clause applies to the native target;
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

  *"has a commit that lands it" is now recorded, from 1.12.* `coverage.py` gained
  a `LANDED` table keyed by (donor, subject) beside `VERDICTS`, because a verdict
  is a judgement and says nothing about whether anyone acted on it; the generated
  table marks a discharged row `re-apply — done` and the summary counts them.
  CTF's three are the first, and discharging them taught the sharper form of the
  requirement: **re-applying a fix means re-asking its question of the merged
  tree, not confirming its hunks survived the merge.** `372e2503fe47` guarded the
  two `G_PickTarget` sites `g_turret.c` had in 2024; the merged tree has three,
  and the third — Ground Zero's `turret_brain_link`, arrived in Phase 2 — still
  crashed. Nothing in this project compares a fix against the *shape* it fixed,
  so that class is found by reading, and this is where it gets read.
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
* **R-VER-15.** *The original harness's unfinished checks are inherited.*
  **All eight are discharged as of 1.14** — the last two, items 4 and 8, by
  measurement rather than by the phase they were assigned to: the flash-offset
  table has 288 rows for 288 enumerators and is sized by the enum, and `clamp()`
  is gone from the tree with rogue's `p_view.c` clean in all ten configurations
  (`doc/reconciliation.md` R-61). Items 1 and 2 stopped being vacuous when
  `auditems` and `dsweep` were wired into `make check` with a real donor behind
  them. The
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
     risk.* **Confirmed clean 2026-08-26 (1.27):** `src/g_save.c` carries the
     same `#if USE_NEW_GAME_API` guard, the same `0x100` and `8`, and the same
     `SAVE_MAGIC1`/`SAVE_MAGIC2` as `q2pro/src/game/g_save.c`, character for
     character. It did not drift. The item stays on this list with the
     confirmation attached rather than being deleted, because the risk was real
     and the answer is now written down somewhere (R-128).
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

  *1.23 adds two lines, `bots` and `botplace`, for the same reason and for
  Phase 7's question.* `sv clientdump` says which slots hold bots and which
  library each uses; nothing said **where a bot ended up** — on which CTF team,
  in which arena, counted by which match — so every one of Phase 7's claims
  would have been a claim about source rather than a fact about a server:

  ```
  bots         4 bot(s) of 4 client(s) in 20 slot(s), at 16,17,18,19
  botplace     ctf red=2 blue=2 noteam=0, FL_BOTCLIENT=4 (R-CTF-4)
  botplace     arena in-arena=4 (arena1=4) on-team=4 fighting=3, FL_BOTCLIENT=4 (R-RA-4)
  botplace     tourney m_mode=2 entered=4 ready=4 team0=2 team1=2, FL_BOTCLIENT=4 (R-OSP-11)
  ```

  Both print under every ruleset that accepts bots, **including when there are
  none**: a row that prints nothing and a row that prints zero are not the same
  evidence. `FL_BOTCLIENT` is counted separately from `FL_BOT` on purpose —
  R-CORE-14 makes them two different bits, Phase 6 sets both on every bot, and a
  site that tests one and means the other compiles. If they ever diverge, this
  is where it shows. `doc/reconciliation.md` R-112.
* **R-VER-26.** *A donor's files are compiled even before they are linked.*
  ~~`make check-tourney`.~~ *— the target was deleted when the requirement was
  discharged in 1.21; see the closing paragraph. Struck here rather than only
  recorded there, because the headline is what a reader runs.*

  `src/tourney/` is imported and cannot be linked: its twenty translation units
  reference `match_paused`, `pause_time`, `endlvl_frame`, three armor tables,
  four statusbar literals and two `static` helpers in `g_cmds.c`, every one of
  them defined by a **shared** file whose merge has not landed. Linking would
  mean half-applying those merges, which is worse than not linking.

  R-MP-5 set the precedent — `rogue/m_move2.c` ships as a reference and is not
  built — but a file nothing builds is a file nothing checks, and twenty of them
  is not one. So they are compiled `-fsyntax-only` against the merged
  `g_local.h` on every `make check`. That is the only thing standing between the
  union header and twenty files the linker never sees: a field rename would
  otherwise break them silently until the next increment.

  R-61's mechanical count of unbuilt files goes from 1 to 21, with
  `doc/reconciliation.md` R-83 as the reason. **When the merges land, this target
  goes away** rather than becoming a permanent second build.

  *Discharged in 1.21.* The merges landed, `src/tourney/` is linked, and
  `check-tourney` is deleted rather than kept — which is what the paragraph above
  promised and is the difference between a temporary scaffold and a permanent
  second build. R-61's count is back to 1.
* **R-VER-27.** *A play test that asserts.* `tools/playtest.sh`, driving headless
  protocol clients through every ruleset.

  R-VER-23's `play.sh` drives q2pro's own client under `Xvfb`, which is the only
  way to test what a human sees and is how R-62 was found. It is also a GL client
  per run, paced in frames by a console script, and it cannot make a claim — it
  prints, and a person reads. R-VER-27 is the other half: a `libq2` client is a
  real client on the wire (configstrings, layouts, centerprints, playerstates,
  console commands, userinfo updates) driven by a program that **asserts**, so a
  regression is an exit code rather than a paragraph to read.

  What it cannot do is walk. Steering by yaw across a map is not repeatable
  enough to build a test on, and the attempt is recorded so it is not repeated.
  It does not matter: the ruleset code is reached through menus, client commands,
  userinfo and the two `sv` diagnostics, all deterministic.

  Every check must be a **difference** — the thing one ruleset does that another
  must not — because one library serves **seven** rulesets (five before 1.36)
  and "ctf maps the CTF stats" is true of the table, not of the dispatch. The
  four OSP rulesets sharpen this rather than diluting it: they share a code
  path, so a check that passes under all four proves only that the path runs,
  and the sweep's `dmpro`, `tdm` and `duel` rows must each assert what that
  ruleset does and the other three do not — `qualifier_numspots`, `Score_A`/
  `Score_B`, and `team_maxplayers 1 CVAR_NOSET` respectively. The three control servers are part of
  the requirement, not an extra: a modifier the matrix accepts, a modifier it
  refuses, and `g_protocol_extensions 1` as the control for R-38's dropped rows.

  The scenario and its harness live in the user-level `q2-playtest` skill rather
  than in this tree, because they are useful to any Quake II mod; `tools/playtest.sh`
  is the versioned entry point that records how to run them. It is **not** part of
  `make check`: it needs a built engine, retail paks and about a minute.

  *1.23: **171 checks, of which 32 are a client watching bots**, and that is the
  author's own play test mechanised rather than deferred a fourth time.* The
  rows need a brain as well as a server, so they are skipped when `$GLADDIR`
  holds none — and **the skip is recorded as a check**, because "171 passed"
  must not quietly mean "and thirty-two were not run".

  What they add over `tools/botmatrix.sh` is a person, or the nearest a script
  gets to one. The bot matrix boots a server, adds bots and reads two `sv`
  dumps; it can say a bot exists and that removing it returns the slot. It
  cannot say that a client connecting to that server **sees** the bots, that a
  bot wears its team's skin on the wire (R-CTF-7's v0.95 half, read off the
  configstring), that obituaries reach a watcher as broadcast prints, or that a
  bot gets out of the way when a person wants its slot (R-VER-5). R-OSP-4's
  SIGFPE row is here for the same reason: only a client can start the vote that
  `OSP_votePercent` then divides by an empty room every frame.

  Two things writing them settled. **Wait for the event, not for a frame
  count** — a first version waited 300 frames and failed under `arena`, where a
  round does not begin until two teams are populated and a countdown has run;
  waiting on the server's own obituary also separates "they did not fight" from
  "they fought and the client was not told". And **a census is only as good as
  its parser**: the arena line reports `in-arena` and `on-team`, and a
  `[A-Za-z0-9_]` key pattern captured `arena` and `team`, so two assertions
  failed against a line that said exactly what they wanted. And **"wait for an
  obituary" is not "wait for one while somebody is watching"**: the harness's
  `WaitLog` scans the backlog, so the wait matched a kill from before the client
  connected and then asked the client why it had not seen it — two runs failed
  that way in two different rulesets, which is a check measuring the wrong
  instant rather than a flaky game. `doc/reconciliation.md` R-111.
* **R-VER-28.** Botlib ABI check: `tools/botabi.py` compares
  `src/bot/botlib.h`'s two contract tables against
  `gladiator-bot-restored/botlib/be_interface.h`, slot by slot, and fails on any
  argument list that differs after normalisation — **and on the `Trace` slot
  being selected by a different preprocessor condition than the brain uses**.
  It exists because R-BOT-1's two mitigations cannot see the thing they were
  written for: `BotVersion` returns the same string on both sides of an ABI
  split, and `Test(int, char *, vec3_t, vec3_t)` passes no struct by value. The
  defect it would have caught cost a phase's worth of confusion — every trace
  returning all-zero, so the brain believed it was wedged in solid and the bots
  stood still — and a compiler cannot see it, because both spellings are valid C
  and only one of them agrees with the other side (`doc/reconciliation.md`
  R-97). It also checks the LAYOUT half, and by measurement rather than by
  comparison: it **compiles a probe against the brain's own headers** and checks
  every `_Static_assert(sizeof(...))` in `src/bot/botlib.h` against what the
  brain's compiler says. A number copied out of a document goes stale; a number
  a compiler produced this minute does not — and the two defects R-100 records
  are both a size, both invisible to every reading, and both fatal to what the
  brain does with the struct. Six positive controls, two of which are R-97 and
  R-100 exactly. It skips itself with a message when the brain is not beside the
  repository, which is the rule `auditems` already follows for q2pro; what it
  **cannot** see is a slot the brain calls wrongly inside itself, which is R-98.
  Member ORDER at equal size it now partly can: `botlib.h` asserts
  `q_offsetof(bsp_trace_t, fraction) == 8` and `endpos == 12` beside the sizes,
  mirroring the two upstream added.

  *It fired on its first real use, which is the argument for it.* The run after
  `gladiator-bot-restored 57ce85a3` reported `Trace is selected by a different
  condition (R-97) — ours: #if defined(__x86_64__) || defined(__aarch64__),
  brain: (unguarded)`, before anything was built or run. Version 2 of the
  contract was agreed by copying the other side's header; version 3 was agreed
  by measuring it.

* **R-VER-25.** *A donor's own surface, used in a shared file, is inside that
  donor's gate.* `tools/donorgate.py`, in `make check`.

  One library, one `gclient_t`: a donor's fields exist for every client and its
  functions link into every build. Nothing stops a shared file reading RA2's
  `resp.fightstate` under `sp` — the field is there, it compiles, and it is
  **zero**, which is `FIGHT_SPECTATING`, which is not `FIGHT_ALIVE`, which makes
  the test false. Six sites in the Phase 4 merge did exactly that and each one
  *deleted* behaviour from the whole game rather than adding any to arena
  (`doc/reconciliation.md` R-70). None was a merge conflict; none was visible to
  a compiler, an audit or a boot.

  R-MODE-5 already says a gate is a dispatch row or a named predicate. This is
  that rule enforced from the other end: not "is the ruleset chosen properly?"
  — which is `gates.py`'s question and cannot see a field test — but **"is this
  donor's state read under another donor's ruleset?"**

  A gate is `G_Ruleset() == RULESET_<DONOR>` anywhere in the statement, on an
  enclosing block, on a braceless `if` above, or on a `case` arm; and a test
  that is true for exactly one ruleset counts, of which `menu_owner ==
  MENU_ARENA` is one and `G_IsObserver()` deliberately is not.

  The donor's surface is **computed**, not listed: everything its headers
  declare at top level, minus anything `g_local.h` also declares or a spine file
  defines. A helper promoted into the shared tree stops being reported without
  an edit, which is what makes this usable for tourney in Phase 5 rather than a
  list that rots.
* **R-VER-24.** *A merge may not drop a call to a symbol the donor deleted.*
  `tools/lostref.py`, in `make check`. R-26's sixth rule.

  Three donors delete the whole monster set and Colosseum replays none of it
  (R-CORE-8). For CTF that cost nothing: its deletions were whole **files**, and
  a file the merge never opens cannot lose anything. RA2 deletes monster code
  from inside the **shared** files as well, and there R-26's five rules are
  blind — where our copy of a spine file happens to equal the base, rule 2 takes
  the donor's version of the hunk, deletion and all.

  **The compiler cannot see it.** `m_move.c` is still in the tree and
  `M_walkmove` is still exported, so nothing is undefined; the call site is
  simply gone. A barrel stops shifting when you walk into it. Nothing in §10
  looks at that, and nothing will.

  So: every function defined in the files a donor deletes, counted in each
  shared file against `q2pro/src/game`, and a count that went **down** fails the
  build. Comments are stripped first, and that clause is load-bearing rather
  than tidy — RA2's `barrel_touch` carries a comment naming `M_walkmove` to
  explain why the call is gone, which keeps the raw count level and puts the
  check to sleep exactly where it is needed. Risk 2d's shape, avoided by
  construction rather than by luck.
* **R-VER-23.** *At least one check must be a player.* `tools/play.sh`, driving
  q2pro's own client headlessly against a local server.

  R-VER-20 established that every check must not be static — that something has
  to advance the clock and look again. This is the next thing that was missing
  and it went unnoticed for the same reason: every check up to 1.14, R-VER-20's
  temporal assertions included, is *state a server reaches on its own*. A door
  that has to be walked into, an item that has to be picked up before it can
  respawn, a flag that has to be carried home, a userinfo string that only
  exists once a client sends one — none of it had ever been executed, and the
  phases recorded that as a property of the harness.

  It was not. `Xvfb` and q2pro's X11 backend on Mesa's software GLX give a
  client with no GPU and no audio device; `cl_beginmapcmd` fires a console
  script when the map finishes loading; `wait N` paces it in frames, so a drive
  is deterministic rather than timed; `viewpos` prints the player's position, so
  movement is observed rather than assumed. The requirement is that **at least
  one check connects a real client, sends real input, and asserts on what the
  server did about it** — and that the drive script's assertions name frames, the
  way R-VER-20's do.

  The evidence that it is real is not that the client starts. It is that
  reverting the one line of R-VER-22's sibling fix (`doc/reconciliation.md` R-62)
  makes `play.sh` report `verdict=2, server errored` and restoring it makes a
  player move 216 units.
* **R-VER-22.** *A pointer is released by the family that allocated it, and the
  check is not a libc ban.* `tools/allocpairs.py`, in `make check`.

  Two allocator families are live in this tree — libc's
  `malloc`/`calloc`/`realloc`/`strdup` released by `free()`, and the engine's
  `gi.TagMalloc`/`G_CopyString` released by `gi.TagFree()` — and crossing them is
  undefined behaviour, not a leak, because `gi.TagFree` walks a block list and
  asserts on a header it did not write. R-55 is the live case and it was
  client-reachable on the first `warp` command.

  **The obvious form of the check is wrong.** "No libc allocator in a tree whose
  frees go through `gi.TagFree`" fails on `src/g_main.c`'s `EndDMLevel`, whose
  `strdup`/`free` pair is correct and is upstream's, which R-CORE-5 keeps
  byte-identical — the build would fail on the one file the spec forbids editing.
  "This file uses both families" is no better: `g_main.c` also `gi.TagMalloc`s
  `g_edicts`, so it describes exactly the file that is right. The unit that has to
  agree is the **pointer**, which is why this is shaped like R-VER-21's tool:
  MIX is one pointer allocated by one family and freed by the other; SPLIT is one
  struct **member** allocated by both across the tree, the merged-tree case;
  UNKNOWN is a free whose operand its file never allocates, in a file that uses
  both families.

  Three controls ship in the tool, the first being R-55's actual bug restored.
  The tool verifies its own wrapper list too: if `G_CopyString` ever stops
  allocating with `gi.TagMalloc` the build fails, because otherwise the check
  would go green by no longer looking.
* **R-VER-21.** *The timer-unit contract is checked, and a merged tree needs a
  check the separate packs do not.* `tools/units.py`, in `make check`.

  q2pro's frame-number conversion retypes every timer from float seconds to `int`
  frames, so a field's **type no longer says what its unit is** and mixing them
  compiles silently and runs wrong by ten. `q2pro/doc/mission-packs.md` promotes
  this to the fourth contract the replay could not check and gives two greps for
  it: cross-unit mixing on one line, and a `level.framenum` beside a float
  literal with no `BASE_FRAMERATE`.

  The third check is ours and has no upstream equivalent, because upstream cannot
  have the defect. Four separate libraries are each internally consistent even
  where they disagree — rogue declares `monsterinfo.attack_finished` a `float`
  and writes seconds, xatrix declares it an `int` and writes frames. Colosseum has
  **one** `g_local.h`, so exactly one declaration survives R-CORE-6's union and
  every site written against the other is now in the wrong unit. So: **one field,
  both clocks, anywhere in the tree** is a finding, reported with the minority
  side named, because the minority is what has to move.

  Three controls ship in the tool, each a reversion of a real fix. And the gap is
  stated rather than left implied: none of the three can see a comparison written
  entirely in the wrong unit, because `SV_RunThink`'s `thinktime > level.time`
  names no timer field. That one needs R-VER-20.
* **R-VER-20.** *At least one check must wait for something to happen.* Every
  other check in §10 is static analysis or a dedicated-server boot with an entity
  census taken at spawn: builds, the four audits, R-VER-2's boot matrix,
  R-VER-17's savegame round-trip. **None of them advances the clock and then looks
  again**, and the whole of Phase 2's evidence — 74 monsters on `rmine1`, three
  campaigns loading, per-monster flavour and evasion latched — is spawn-time state
  that stays true even if every timer in the game is wrong by a factor of ten.

  It was, and the check that found it is the one that had to wait: CTF's techs
  are spawned by an entity think two seconds after level load, so counting them
  requires letting two seconds pass. They arrived at twenty
  (`doc/reconciliation.md` R-54). The requirement is therefore that the regression
  set keeps at least one *temporal* check — spawn a level, advance past a known
  think deadline, and assert the thing that think was supposed to do. `sv ruleset`
  reporting `level.framenum` is what makes the assertion precise rather than
  "it eventually appeared".
* **R-VER-19.** *The slot map must be observable from outside the library too, and
  `sv slots` is how.* It reports the active ruleset's resolved map as (slot, kind,
  logical id) triples, **every row the map declares that resolution dropped and
  why**, and the composed statusbar with its byte count.

  Same argument as R-VER-18 and the same failure it prevents: nothing the engine
  prints reveals which slot a stat landed in, and "the HUD looks right" is not
  evidence about slot 32 — a client that never negotiated the extension draws an
  identical HUD whether the extension-only rows were dropped correctly or written
  into a void. The dropped-row list is the part that matters, because a silently
  absent stat is precisely the failure mode R-OSP-7a exists to remove. It is also
  what makes R-OSP-7a's equivalence claim checkable at all: the bar is printed, so
  it can be diffed against the literal it replaced.
* **R-VER-17.** *The smoke test is a named, repeatable check, not a one-off.*
  Run after every phase, on the native target, against a stock Q2PRO.
  *Mechanised in 1.20 as `tools/smoke.sh`, with `--control`*:
  1. **Deathmatch.** `+set deathmatch 1 +map q2dm1`. Expect `Loaded game library
     from …`, `==== InitGame ====`, `SpawnServer: q2dm1`, `0 entities
     inhibited`, a `status` naming the map, and `==== ShutdownGame ====` on
     `quit`.
  2. **Campaign.** `+set deathmatch 0 +set coop 1 +map base1`. Expect
     `Game supports Q2PRO enhanced savegames.` and a non-zero inhibited count
     that **differs** from the *same map's* deathmatch count, which is
     `SPAWNFLAG_NOT_COOP`/`NOT_DEATHMATCH` filtering doing its job. The
     comparison has to be same-map: `q2dm1` against `base1` differs because they
     are different maps and proves nothing.

     *1.20 corrects a stale number and explains it.* 1.5 recorded "28 against
     50"; `base1` now reads **2 against 50**, and the 26 are Phase 2's, not a
     regression. Stock q2pro's `g_spawn.c` has the co-op arm **commented out**,
     so co-op falls through to the plain skill filter and that filter removes
     the entities marked `!easy & !med & !hard` — which is the marking for
     *co-op only*, so upstream deletes in co-op exactly what co-op is for. The
     Rogue merge (`d3c6d83`) brought the real arm, which honours
     `SPAWNFLAG_NOT_COOP` and guards the skill test against the co-op-only
     marking, as R-SP-1 requires. The 28 was measured before it landed.
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
  5. **Every boot's exit status is read**, and that is 1.23's addition rather
     than a refinement. Items 1–4 all judge a boot on what the log said, so a
     server that printed everything it was asked for and then died on the way
     out passed — and one did. `load` segfaulted, twice over, on every savegame
     in the tree, while this script printed `Segmentation fault` in the middle
     of its own output and `smoke test passed` at the end of it
     (`doc/reconciliation.md` R-108). The control that ships with it
     manufactures the failure by sending `SIGSEGV` from outside, because q2pro
     has no deliberate-abort console command in this build (`error` and `crash`
     are both unknown) and a bad map name exits 0.
* **R-VER-29.** *A run-time check reads the exit status, not only the log.*
  Every script that starts a server must fail the row when the process dies,
  and must be able to say **how** — `died on signal N` rather than a mismatch
  three fields away.

  It exists because the omission was invisible from inside: `tools/smoke.sh`,
  `tools/bootmatrix.sh` and `tools/botmatrix.sh` all judged a boot on its
  console output, and a server that printed everything it was asked for and then
  segfaulted on the way out satisfied all three. One did, on every savegame
  `load`, in every configuration, for as long as those scripts have existed —
  and `smoke.sh` printed the words `Segmentation fault` in the middle of its own
  output on the way to `smoke test passed`, because the shell writes a signal to
  *its* stderr and not to the log the script reads
  (`doc/reconciliation.md` R-108).

  The clause has a second half, which the boot matrix's control shows: the check
  must distinguish a crash **before** the row's own evidence from one after it.
  A frame count already catches the first; only the exit status catches the
  second. Each script ships a control that manufactures the failure, and since
  q2pro has no deliberate-abort console command in this build — `error` and
  `crash` are both unknown, and a bad map name exits 0 — the control sends
  `SIGSEGV` from outside, which is also the failure the clause is for.

  `botmatrix.sh` already read the status, for R-98, which is why its rows were
  the only ones that could tell a leak from a crash. That was a local answer to
  one finding; this makes it the rule.
* **R-VER-30.** *No unbounded string copy anywhere in `src/`* (new in 1.24).
  `tools/bounded.py` bans `strcpy`, `strcat`, `sprintf`, `vsprintf`, `strncpy`
  and `strncat` outright rather than asking which of them is reachable, because
  reachability is not decidable and R-SEC-1's list of reachable paths is a list
  of *examples*. `src/shared/` is exempt as a path, because it is vendored
  byte-identical from `q2pro/src/shared` and `divergence.py` is the check that
  covers it. Five controls, one of them a call split across two lines.
* **R-VER-31.** *A `.c` may not declare what another `.c` defines, and where a
  declaration is written anyway it must agree with the definition* (new in
  1.24). `tools/externs.py`, both halves of R-SEC-8's second clause. Six
  controls, including a dropped `const` on a parameter — which is not a
  hypothetical: it is what the check found on its first run. `src/g_ptrs.c` is
  exempt because `genptr.py` generates it from the definitions themselves and
  `check-ptrs` diffs it in the build.
* **R-VER-32.** *The game library opens no socket and starts no process* (new in
  1.24). `tools/noexec.py`, R-SEC-7 by name rather than by review, and `exit()`
  and `abort()` are on the list for the same reason — a game library that ends
  the server process is the shape R-SEC-5 is about. Five controls.
* **R-VER-33.** *The R-EXTRA features are observable from outside the library*
  (new in 1.24). `sv extras` prints one line per requirement and every line is a
  **measurement** — whether the log is open and where, the lag pool's size,
  whether the three classnames are in the spawn table, how many itemlist rows
  carry a `weapmodel`, which observer implementation the ruleset uses — for the
  same reason R-VER-18 and R-VER-19 exist: six of the seven extras are invisible
  from a console otherwise. `tools/extras.sh` asserts on those lines and on the
  files the log actually writes, with four controls including R-VER-29's
  post-census SIGSEGV.
* **R-VER-34.** *An arena round is observable from outside the library* (new in
  1.30). `sv arenadump` prints, under `arena` only, one line per arena (state,
  pickup flag, round, queue depths, `sidepick`, players per team), one per live
  team (arena, side, members, fighting, wins, locked) and one per connected
  client -- which arena, which team, `fightstate`, `solid`, `takedamage`,
  `deadflag`, `health`, `svflags`, `spawn_recheck` and the origin it was placed
  at -- followed by a `!!` line for every pair of clients that are both solid and
  overlapping.

  It exists because three defects in one play test were invisible to every other
  instrument. A client's HUD reports its own arena; the scoreboard reports its
  own team; `sv ruleset`'s `botplace` row counts bots per arena. None of them can
  say that two clients are at one origin with `solid` set on both -- which is a
  wedged telefrag and a player who cannot move -- or that a bot is on a team in
  an arena the people are not in. `spawn_recheck` is the sharpest field on the
  line: **nothing but `KillBox`'s push branch sets it**, so a non-zero value *is*
  the record of two clients having been placed on one spawn point, and counting
  those is how R-135's fix was measured -- 11 before, 0 after, eleven players on
  the same map.
* **R-VER-35.** *Every donor's feature set is walked definition by definition*
  (new in 1.47). `tools/fnsweep.py` takes R-PROV-3's five diffs and, for each
  top-level definition a donor added, changed or removed against the spine,
  compares three texts -- the spine's, the donor's and this tree's -- and
  classifies the pair: `as_donor`, `as_spine` (the tree kept the BASE, so the
  donor's change is not here), `merged`, `absent`, `donor_deleted`. For a
  `merged` definition every donor-added line is then looked for at its own site
  and, failing that, anywhere in `src/`. `doc/donor-fdiff.md` is the
  read-through and **the tool is authoritative over every table in it**, the way
  R-CORE-10 makes `doc/reconciliation-matrix.md` authoritative over §3's.

  It exists because R-CORE-10 measures the merge load per FILE, in diff lines,
  and a line count cannot say whether a donor's change to a particular function
  survived. The neighbouring checks are each narrower on purpose: `lostref.py`
  asks the question of the symbols a donor DELETED (R-VER-24), `donorgate.py` of
  whether a donor's surface is gated (R-VER-25), `deadvalue.py` of the values a
  field can hold (R-TOOL-6). None of them walks a donor's own feature set.

  **The finding is narrow and the report is not, and the difference is written
  into the tool.** `--check` fails on a donor line whose identifiers exist
  NOWHERE in `src/` and that no recorded decision explains -- because a line
  missing from its own site is the *normal* case under R-MODE-5, and 1,165 of
  them are elsewhere in this tree verbatim. Of R-195's seven findings that rule
  contains two; the other five turn on identifiers that are all present and are
  missing a condition, a reader, an argument or a call site, so the tool's
  `NOT_MECHANISED` list names them and names the resolver each shape would
  need. A clean run of this check is not a claim that the donors are fully
  carried, and it says so.
* **R-VER-16.** OSP-family matrix: for each of `dm`, `dmpro`, `tdm` and `duel`
  the server starts, prints the banner R-OSP-12 tabulates, sets `match_type` to
  the string in that row, and runs a match to completion. Three assertions per
  ruleset: the `match_type` serverinfo string matches the banner; the
  ruleset-gated client commands accept and reject per `g_cmds.c`'s guards
  (`highscores` only under `dm`, `queue`/`line`/`order` only under `duel`,
  `captain`/`invite`/`lockteam` only under `tdm`); and for `duel` specifically,
  `team_maxplayers` reads 1 and is `CVAR_NOSET`.

  *Was a `match_mode` matrix until 1.36, with an out-of-range arm.* That arm is
  gone with R-OSP-13 — there is no number to put out of range — and nothing
  replaces it: `tourney` is not a ruleset name, so R-VER-2's existing
  unknown-value control is all that remains to check. The four positive rows
  are unchanged in substance and are
  now four rows of the ruleset sweep rather than a matrix of their own, which is
  the point of the flattening: one selector, one matrix. This is still the check
  that would have caught the 1.0–1.2 omission of the fourth mode.

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
| 20 | **A whole class of defect that only shows when time passes.** Realised in 1.13, and 1.14 measured its extent: the reverted `SV_RunThink` was one of **twenty-four** unit defects in the same class, and upstream found them independently in the same week. Originally: `SV_RunThink` compared frames against seconds for three phases and nothing noticed, because every check was static or a spawn-time census. The same blindness covers item respawn intervals, powerup durations, door and plat cycles, monster animation rates, flood-protection windows and the match clock | **High** | R-VER-20 requires at least one temporal check in the regression set, and `sv ruleset` reports `level.framenum` so a temporal assertion can name a deadline instead of waiting vaguely. The timer conversion itself is one bounded pass over 43 sites plus one comparison, with `tools/nextthink.py` to do it |
| 17 | Re-expressing the 17 live TOURNEY blocks (R-BOT-29) breaks bot behaviour under `tourney`, which currently works | High | Enumerate all 17 with their gate targets before touching any; R-VER-3 bot matrix run under `tourney` before and after; the `bots_*` namespace preserved per R-OSP-11 |
| 18 | Staged exposure means the first real players are the security test (R-SEC-2) | High | Accepted deliberately. R-SEC-1..9 all still stand and the inherited upstream fixes are the floor; the mitigation is that exposure widens only after the private stage holds. **1.24: the stages are written down** — `doc/exposure.md` gives each one a list of what has to hold before the number of people who can be hurt goes up, and says plainly what none of the analytical work covers: no fuzzing of the network-facing parsers, no adversarial client, and nobody has reviewed this who did not write it |
| 19 | Development host and player host diverge: everything is written and *run* on **aarch64** (R-BUILD-6) while Q2 players are on x86-64 and win32 | Low–Medium | Reduced in 1.4 from Medium: all four player-facing targets now **cross-build from the dev host** (R-BUILD-5), so a compile or link regression on any of them fails here rather than in the field, and all five are gating in R-VER-8. What is left is narrow but real — nothing x86 is *executed* here, so anything that survives compilation and differs at runtime (pointer width, struct padding, the by-value `bsp_trace_t` of R-BOT-5, the botlib bitness handshake of R-BOT-4) is caught only when the user runs it. Endianness is not the exposure: all five targets are little-endian and R-BASE-7 already forbids `Swap_Init`. Mitigation is to make the 32-bit PE build part of the routine build set rather than an occasional check, since it exercises the widest gap from the host in one artifact. **1.27: the mitigation paid, and the exposure narrowed by one target rather than closing.** The author ran `gamex86.dll` on Windows 10 -- it loads, spawns `q2dm1`, takes a client and sets the brain up -- the first execution of a non-aarch64 artifact in this project, and the run that found R-123. What this row names is *still* unexercised, though: that session had no `.aas`, so `BotLoadMap` refused the map and neither the botlib bitness handshake nor the by-value `bsp_trace_t` has ever crossed the boundary on x86 -- which is exactly the pair that cost the whole of 1.22 on aarch64. Three targets remain unexecuted: win64 PE, ELF x86-64, ELF i386 |
| 24 | **A donor's ARITHMETIC can assume a struct layout that R-CORE-6's union makes false, and nothing can see it.** Realised in 1.19. Tourney's `InitClientPersistant(client, false)` clears from `sizeof(userinfo) + sizeof(netname) + sizeof(greenname)` onward, which keeps the client's identity **only while those three are the first members**; in this tree they are not, so it would have cleared the identity and preserved three unrelated ints. Both the donor's version and ours compile, both run, and the difference is silent. R-58's type-collision half is at least visible when the types differ (`-Wbool-compare` caught two of three this phase); an offset assumption is not visible at all | **Medium** | None mechanised, and none proposed: the check would have to know that a piece of arithmetic *means* a struct layout. What limits it is that R-CORE-6 puts every merged field in one header with its donor named, so a reviewer reading a `sizeof`-sum against `client_persistant_t` has the layout in front of them — which is how this one was found |
| 23 | **A donor's `if` is a claim about that donor's world.** Realised in 1.17. RA2 tests `resp.fightstate == FIGHT_ALIVE` in a dozen shared places; `FIGHT_SPECTATING` is 0, so the test is false for every client under every other ruleset, and six of those sites inherited ungated *delete* behaviour from the whole game — teleport splash, landing sound, falling damage, footsteps, weapon animation, telefragging. None of the six is a merge conflict, none is visible to a compiler or an audit, and none is reached by a boot. The class is wide open for Phase 5, which brings a donor with its own `m_mode` and its own observer state | **Medium** | *Reduced in 1.18: there is a mechanical check after all.* `tools/donorgate.py` (R-VER-25) asks it from the other end -- not "is the ruleset chosen properly?", which `gates.py` asks and which cannot see a field test, but **"is this donor's state read under another donor's ruleset?"**. 77 donor-surface uses in shared files, 0 ungated, three controls. What it still cannot judge is whether a *gated* test is the right one; that stays a reading job |
| 22 | **A donor's deletions are as dangerous as its additions, and only one of the two has a check.** Realised in 1.16. Every merge rule, tool and review habit in this project is built around what a donor *adds*; RA2 is the first that also removes, and four removals crossed the merge, linked cleanly and would have shipped. The class is wide open for Phase 5 — `osp-tourney` deletes the same 44 monster files | Medium | R-VER-24 and `tools/lostref.py`, which covers the monster set specifically. What it does **not** cover is a donor deleting anything else a shared file needs, and no check does |
| 21 | **Everything verified so far is state the server reaches on its own.** Realised in 1.15 and the sibling of risk 20: R-VER-20 fixed "no check advances the clock", but every check still ran with nobody playing, so nothing a *player* triggers had ever executed — and a defect on the first line of the connect path survived three phases, a boot matrix of 32 combinations and an upstream bugfix import. Doors walked into, items picked up and respawned, flags carried, userinfo sent, menus opened, the grapple fired: all unexecuted | **High** | R-VER-23 and `tools/play.sh`. The capability exists and is proven; what is left is drive scripts, and the rows in `doc/reconciliation.md` R-61 that read "needs a client" are re-scoped to "needs a drive script" |
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
| **Rescued harness archive** | `../colosseum-harness-archive` — `rr-cache` (516 resolutions in 407+85 directories), `commit_paths.txt`, `style_residual.patch`, `osp/`, id's five source releases, astyle 3.1, and the harness `TODO.md` (R-PROV-2a, R-VER-15) |
| uGladQ2 cleanup | `ugladq2` — **reference only** |
| Port method write-ups | `rocketarena2-public/doc/q2pro-port.md`, `osp-tourney/doc/q2pro-port.md`, `q2pro/doc/mission-packs.md` |
| Replay bundles (§3.2) | `../osp-q2pro-port-replay.bundle`, `../ra2-q2pro-port-replay.bundle`, `../q2pro-mission-pack-replay.bundle` |
| Replay + audit toolchain | `q2pro-mission-pack-tools` (R-TOOL-1) |
| RA2 kept-bug review | `rocketarena2-public/CLAUDE.md` §"Deliberately-kept original bugs" + `docs/FUNCTION_NOTES.md` (R-SEC-2). **The `docs/SECURITY_REVIEW.md` of 1.0–1.3 exists in no branch and no commit** |
| Donor comparison notes | `game-comparison-q2pro-vs-yquake2.md`, `q2pro-game-changes.md`, `yquake2-game-changes.md`, `key-contract-findings.md` — **absent from this machine.** They were WSL-side working notes; nothing in §5–§10 depends on them, so they are recorded as lost rather than sought |
| Rescued style tool | `../colosseum-harness-archive/astyle/astyle-3.1` — x86-64 ELF, byte-identical output to the host's native `astyle` 3.1 (R-BUILD-6); needed only if the host one changes version |
