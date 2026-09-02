# Function-by-function behavioural diff — five donors against this tree

Deliverable of R-VER-35. Measured 2026-09-02 against spec 1.47.

**`tools/fnsweep.py` is authoritative over every table transcribed here**, the
way R-CORE-10 makes `doc/reconciliation-matrix.md` authoritative over the table
in `SPECS.md`. It prints all of them:

```sh
tools/fnsweep.py              # the matrix, the attribution, the exempt lines
tools/fnsweep.py --check      # what audit.py runs: exit 1 on a line nothing explains
tools/fnsweep.py --selftest   # the eight controls
```

---

## 0. Method

`doc/reconciliation-matrix.md` (R-CORE-10, `tools/divergence.py`) states the
merge load per **file**, in diff lines. A line count cannot answer the question
this report asks: *for each definition a donor touched, does this tree carry
that donor's behaviour?* So the same measurement is taken per **definition**.

**The donor's feature set is R-PROV-3's diff.** `git diff baseq2 port_<donor>`
in the vendored bundles, spine `baseq2` @ `5d180cb`, donor tips `ctf`=`e94bbeb`,
`xatrix`=`2829dba`, `rogue`=`2ebed93`, `ra2`=`3f5ecb9`, `osp`=`205a89c` — the
same refs `tools/divergence.py` uses, so both tools answer about the same
donors. Build spill (R-PROV-4) and the generated `g_ptrs.c` are excluded.

**Three texts per definition.** For every top-level definition (function, data
object, type, function-like macro) that a donor added, changed or removed
relative to the spine, three versions are extracted by brace matching over a
comment/string-masked copy: the spine's, the donor's, and this tree's — the
tree's found by name across all of `src/`, so a definition that **moved** file
(`stuffcmd` did) is still compared against the right body. Comparison is on
comment-stripped, whitespace-collapsed lines; string literals are kept, because
a literal is what a player reads.

**Verdicts.**

| verdict | meaning |
|---|---|
| `as_donor` | the tree's text equals the donor's, modulo comments and whitespace |
| `as_spine` | the tree kept the **base** version: the donor's change is not here at all |
| `merged` | neither; the donor's delta is then checked line by line |
| `absent` | no definition of that name anywhere in `src/` (a body-similarity rename search runs) |
| `donor_deleted` | the donor **removed** the definition (R-CORE-8 says a deletion is not replayed) |

**For `merged`, the delta is checked rather than the text.** Every line the donor
*added* against the spine is looked for in the tree's version of that definition
(exact, then ≥0.80 fuzzy), and every line the donor *deleted* is looked for too.
A donor line found in neither is then asked a second question, against the whole
tree rather than the one site:

| level | meaning |
|---|---|
| `relocated` | the exact line is somewhere else in `src/` — R-MODE-5 moved it |
| `adapted` | a ≥0.80 match is somewhere in `src/` |
| `rephrased` | every identifier in it exists in `src/`, the line does not |
| `ABSENT` | at least one identifier in it exists **nowhere** in `src/` |

`ABSENT` is the sharp one: a field nothing reads, a constant nothing names, a
string no player can see.

**And one more instrument, because text is not behaviour.** For every touched
definition, the multiset of *calls* is compared across the three versions: which
calls did the donor **add** that this tree does not make at that site, and for
each, does the callee exist in `src/` and does anything in `src/` call it?
`tools/lostref.py` asks this for the symbols a donor *deleted*; this asks it for
the calls a donor *added*, which is where a donor's own feature lives.

**The extractor is the load-bearing part and it carries its own control.**
`--selftest` asserts it reads the shape that broke it — a function whose
signature ends in a trailing `//PGM` after the closing paren, under a
commented-out prototype, which a name-by-regex extractor merges into its
predecessor (it did, to `check_dodge`, and the fix is to decide the name from
the MASKED text as well as scan it) — and then asserts the real tree still
yields **4,266** definitions with at most eight anonymous ones, so an extractor
that stops reading `src/` cannot report a clean sweep.

---

## 1. The matrix

**3,197 definitions** are touched by at least one donor, **2,860** of them in
files this tree carries. Per donor, and split by
where the definition lives, because the two are different kinds of work: a
**shared** spine file is an n-way merge, a donor's **own** file was imported into
its subfolder under R-CORE-7 and should read almost verbatim.

### 1.1 Shared spine files — the n-way merges

| donor | defs | as_donor | merged | of those, partial | as_spine | absent | donor_deleted | median similarity |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| ctf | 99 | 9 | 76 | 59 | 6 | 2 | 6 | 0.74 |
| xatrix | 222 | 132 | 86 | 45 | 1 | 2 | 1 | 0.88 |
| rogue | 555 | 388 | 127 | 32 | 3 | 1 | 36 | 0.93 |
| ra2 | 116 | 7 | 96 | 82 | 10 | 3 | 0 | 0.68 |
| osp | 233 | 16 | 131 | 118 | 17 | 13 | 56 | 0.70 |

"partial" is the subset of `merged` where at least one donor-added line is not
at its own site or a donor-deleted line is still there; the rest carry the
donor's whole delta and add to it.

### 1.2 The donors' own files

| donor | defs | as_donor | merged | of those, partial | absent | median similarity |
|---|---:|---:|---:|---:|---:|---:|
| ctf | 163 | 80 | 71 | 26 | 12 | 0.80 |
| xatrix | 252 | 239 | 13 | 11 | 0 | 0.96 |
| rogue | 518 | 490 | 27 | 11 | 1 | 0.97 |
| ra2 | 161 | 79 | 65 | 44 | 17 | 0.83 |
| osp | 541 | 126 | 395 | 272 | 20 | 0.79 |

A further **337** definitions — 183 of RA2's and 154 of tourney's — are in donor
files this tree carries no file of that name for, and the tool counts them
outside the matrix rather than reporting each one: that is a **file**-level
decision (§4.1), and per definition it would arrive as several hundred rows
saying one thing.

**What the shape says.** The two mission packs are carried; the two mods and CTF
are *re-expressed*. Rogue's 388 verbatim definitions out of 555 shared, at 0.93
median similarity, is a merge that took the donor's hunks; CTF's 9 out of 99 is
not a worse merge, it is R-MODE-5 — a CTF hunk lands behind
`G_Ruleset() == RULESET_CTF` or as a call into `src/ctf/`, so almost nothing can
be textually identical. RA2 and tourney are lower again for the same reason plus
one more: they are the two donors whose infrastructure this tree replaced
outright (§4).

`ra2`'s 17 and `osp`'s 20 `absent` in their own files are what is left once
those whole files are set aside: RA2's dead menu helpers and `gslog.c`'s deleted
socket layer (R-114), and tourney's `q2log_stdlog_*` writers, `OSP_ngStatsView`
and the two `PMenu_*` names §4.4 resolves.

### 1.3 Where the donor's lines went

For the `merged` definitions, **4,080** donor-added lines are not present at
their own site. Asked again against the whole of `src/`:

| donor | missing at site | relocated | adapted | rephrased | ABSENT anywhere |
|---|---:|---:|---:|---:|---:|
| ctf | 203 | 55 | 11 | 60 | 77 |
| xatrix | 67 | 12 | 32 | 23 | 0 |
| rogue | 82 | 44 | 5 | 16 | 17 |
| ra2 | 460 | 104 | 42 | 169 | 145 |
| osp | 3268 | 950 | 298 | 878 | 1142 |
| **all** | 4080 | 1165 | 388 | 1146 | 1381 |

The `adapted`/`rephrased` split is the one figure here that is a lower bound
rather than a measurement: the ≥0.80 pass compares a line only against tree
lines that share its first identifier, because comparing all 4,080 against all
47,740 costs four minutes and neither bucket is a finding either way.

Xatrix has **zero** ABSENT lines: every line of its feature that is not at its
own site exists somewhere in this tree.

### 1.4 The 1,381 ABSENT lines, attributed

Every one is accounted for, and **an unattributed line is what `--check`
fails on**. The nine in the first row are §3, each named in the tool's `EXEMPT`
table with its finding number — and an exemption that stops matching any donor
line is itself reported, because an exemption that has quietly stopped applying
is how a check rots.

| what removed it | lines |
|---|---:|
| **findings — §3, and every one exempt by name** | **9** |
| unsafe libc replaced (`sprintf`/`strcat`/`strncpy`/`vsprintf`, R-SEC-1) | 490 |
| `match_mode` deleted, four values of `g_ruleset` instead (R-OSP-12) | 222 |
| the ngLog / ngWorldStats stack, replaced by `osp_stats.c` | 168 |
| `STAT_*` macros replaced by the `SID_*` map (R-OSP-7a) | 83 |
| symbol collisions prefixed (§7 rule 4: `xatrix_fire_heat`, `TECH*_INDEX`, …) | 71 |
| verified present under another name (§4.4) | 64 |
| locals and `goto` labels — no behaviour | 62 |
| RA2's GameSpy / remote stats (R-114, R-RA-1a, R-SEC-7) | 60 |
| donor build switches resolved statically (R-CORE-3) | 52 |
| the menu engines' renames (R-MENU-1, §7 rule 4) | 41 |
| the reconstructions' placeholder field names | 28 |
| donor file I/O replaced by `g_fs.c` (R-BOT-8, R-BOT-26) | 25 |
| statusbar literals replaced by the composed bar (R-OSP-7a) | 6 |

### 1.5 The call delta — what the donor *does* that this tree does not

| donor | defs losing ≥1 donor call | calls | made elsewhere in `src/` | made nowhere |
|---|---:|---:|---:|---:|
| ctf | 61 | 93 | 63 | 30 |
| xatrix | 2 | 2 | 1 | 1 |
| rogue | 12 | 16 | 14 | 2 |
| ra2 | 56 | 110 | 70 | 40 |
| osp | 304 | 828 | 543 | 285 |
| **all** | 435 | 1049 | 691 | 358 |

The 358 "made nowhere" reduce to the same clusters — the libc replacements, the
two logging stacks, the renames of §4.4 and the dead donor code of §4.3. The
691 made **elsewhere** are the more interesting column, because that is
R-MODE-5 visible from the other side: the act is in the tree, one call further
in. Two of them are findings, and both are of the shape only this pass can
see — a function that exists, has callers, and is *not* called from the site the
donor calls it from (§3.3, §3.6).

---

## 2. Per donor

### 2.1 Xatrix — carried, and the sweep found nothing

474 touched definitions, **371 byte-identical** to the donor after comment and
whitespace normalisation, 239 of the 252 in its own seven files. One `as_spine`
(`SP_func_timer`, which is R-KEY-1 — the donor renamed `st.pausetime` to
`st.pause_framenum` and §7 rule 1 refuses it), two `absent` (both the
`fire_heat`/`monster_fire_heat` collision resolved by prefixing, §7 rule 4), and
**zero** ABSENT lines anywhere. Nothing to report.

### 2.2 Rogue — carried, with the biggest deliberate deletion set

1,073 definitions, 878 identical, median similarity 0.93 in the shared files.
The 36 `donor_deleted` records are Ground Zero replacing baseq2's per-monster
`*_dodge` / `*_duck_*` functions with its own `M_MonsterDodge` system: the tree
took the deletions, so both sides agree, which is why they read as "the tree
lacks it too". 17 ABSENT lines, all of them `KILL_DISRUPTOR` and its two
consequences (`AMMO_DISRUPTOR`, `pers.max_rounds`) — resolved statically and
recorded at `src/g_local.h:93` and `src/p_client.c:993`, because Ground Zero
`#define`s the switch to 1 and every branch behind it was already dead in the
donor (R-CORE-3).

One deviation in the tree's favour, in `FindSubstituteItem`, and it is §3.7.

### 2.3 CTF — 9 of 99 shared definitions verbatim, and that is R-MODE-5 working

The low identity count is the gate model, not a thin merge: a CTF hunk lands
behind `G_Ruleset() == RULESET_CTF` or as a call into `src/ctf/`, so it cannot be
textually identical. R-40 already measured the other half of this — 167 of CTF's
265 shared-file hunks are stale copies of id's code rather than CTF's feature —
and the six `as_spine` verdicts here are all in that set (a seventh line of the
same kind, `SVF_PROJECTILE`, is §3.7):

| definition | the donor's version | verdict |
|---|---|---|
| `SP_misc_banner` | `Q_rand() % 16` where the spine has `Q_rand_uniform(16)` | R-40 names this line |
| `SV_FlyMove` | drops the `!VectorCompare(planes[i], planes[j])` guard | stale; the spine's is the fixed one |
| `IsFemale` | reads the `skin` userinfo key, not `gender` | stale (id 3.20's) |
| `ThrowClientHead` | drops the `else { think = NULL; nextthink = 0; }` arm | stale |
| `Drop_Ammo` | drops id's "Can't drop current weapon" grenade guard | stale |
| `TH_viewthing` | adds the robotron model cycler | see below |

`TH_viewthing` is the one worth a sentence, because it looks like added
behaviour and is not: the donor's block reads `robotron[ent->spawnflags - 1]`
and `static int robotron[4]` is **never written**, so the cycle sets
`modelindex` to 0 and the debugging viewthing goes invisible. id shipped that
block inside `#if 0`; q2pro deleted it. Not carrying it is right.

The 12 `absent` in CTF's own files are the menu API: `ctf_PMenu_*` and
`ctf_pmenu_t`/`ctf_pmenuhnd_t` per §7 rule 4 and R-MENU-1, plus `ctf_statusbar`
(R-OSP-7a) and `ghost_t`, which is present as `struct ghost_s` in
`src/ctf/g_ctf.h` and only looked absent because the tree splits the typedef.

### 2.4 Rocket Arena 2 — the arena's own files are carried, the network stack is not

277 definitions in files this tree carries, and 183 more in files it does not.
20 of RA2's 27 added files are not carried and every one is a recorded
decision (§4). Of the 10 `as_spine` verdicts, seven are RA2's monster
deletions reaching inside shared files — `ai_move` and `flymonster_start_go`
emptied, `barrel_touch`'s `M_walkmove` gone, `turret_driver_die`'s
`infantry_die` gone, `G_CheckChaseStats` emptied because RA2 deletes
`g_chase.c` — which is exactly what R-CORE-8 and `tools/lostref.py` (R-64,
R-VER-24) exist to refuse, and they did. Two are `q_unused` markers on functions
whose callers RA2 deleted. **One is a finding, §3.1.**

RA2's obituary rewrite is the largest single `merged` record in the sweep (69
donor lines missing at the site, 120 spine lines the donor deleted that the tree
keeps) and it is fully accounted for: `RA_Obituary` in `src/arena/arena.c` carries
all six announcer sounds, `scorebydamage`, the team-kill taunt `stuffcmd` and a
per-weapon kill breakdown that is *better* than the donor's — RA2 buckets kills
by the inflictor's model index, which mis-files a held grenade, and the tree
switches on `meansOfDeath` (§7 rule 3). The obituary **text** stays baseq2's,
deliberately, and `arena.c:3393` says so.

### 2.5 OSP Tourney DM — 526 of 774 definitions merged, and the log was rebuilt

774 definitions in files this tree carries, 526 of them `merged`. The most
re-expressed donor, for three reasons that are all recorded: fifteen inline
`p_acc[]` writes became two calls (R-91), the NetGames USA logging stack became
`osp_stats.c` with 27 entry points, and `match_mode` became four values of
`g_ruleset` (R-OSP-12) — which alone accounts for 222 ABSENT lines.

**The log replacement checks out.** All 27 `OSP_Stats_*` entry points have at
least one caller. The call delta reports **105 missing `q2log_*` calls across 78
definitions**; mapping each onto the `OSP_Stats_*` that replaced it, name by
name, **67 of the 88 in files this tree carries are present at the same site**,
and 18 of the other 21 are R-MODE-5 relocations into `src/tourney/` —
`G_FreeEdict` calls `OSP_itemFreed()`, `ClientDisconnect`'s events are raised
from `osp_main.c`, and so on. **Three are not relocations**: §3.3 and §3.6.

**And the client handshakes are intact**, which is the sweep confirming
Amendment 1.30's `stuffcmd` audit rather than finding anything: `_is_referee`,
`_default_team_info`, `_default_join_code` and `_init_state` are all present.
The only one dropped is `_ngws_client_id`, which asked the client for its
ngWorldStats password in cleartext, and `osp_stats.h` says why.

---

## 3. Findings

Seven entries, numbered the way `doc/reconciliation.md` R-195 numbers them —
**six findings and one withdrawal.** Each of the six is a donor behaviour this
tree does not have, with no row in `doc/reconciliation.md`, `SPECS.md` or
`doc/regression.md` that decides it, checked by grep for every identifier named
below.

**§3.1 is withdrawn** and kept as a correction rather than deleted: the
behaviour it reported missing has been in the tree since 1.31, and what the
sweep actually found there is a claim about the donor that three documents had
backwards. Six stand.

**STATUS AFTER R-196 (spec 1.49).** Five are closed and two are open as
*decisions* rather than as gaps. The entries below are kept as written, each
with its outcome added, because what the sweep saw and what was done about it
are different facts and a reader wants both:

| entry | what it found | now |
|---|---|---|
| §3.1 | RA2's four fire gates absent | **withdrawn** in 1.48 — R-151 answers it at the latch |
| §3.2 | `spamcount`/`spamtime` with no reader and no writer | **open**, and a policy call |
| §3.3 | `stats_logchat` logs `talkto` only | **closed** by R-196: one call in `Cmd_Say_f` |
| §3.4 | tourney's second ZBot heuristic, `osp_r008` | **open**, exempt by name with its reason |
| §3.5 | R-191's exclusions reach the refusal, not the selection | **closed** by R-196: `ent` threaded through three signatures |
| §3.6 | two log events at their donor sites | **closed** by R-196, with one guard tightened past the donor |
| §3.7 | three one-line deltas | **closed** by R-196; the third needed no code |

**And porting them found two things this method cannot see, both absences
AROUND code that is present.** The donor puts `if (sync_stat != 2)` in front of
both of `player_die`'s death sounds so an OSP match does not start on a chorus
of screams, and this tree carried neither guard. And `OSP_restartStats` was
called forty-five lines before the `memset` that clears `ps`, so every placement
wrote tourney's HUD panel indices and then wiped them — the match clock is
correct through the countdown and gone from the bell onward. The first is §6
item 1 arriving as a real specimen; the second is §6 item 4, "two calls in the
wrong order are two calls present", arriving as a call in the wrong *place*.
Both were found by playing the game. `doc/reconciliation.md` R-196.

**Only two of them are inside `--check`'s own space**, and saying so is part of
landing the tool. §3.4 and §3.7 turn on identifiers that appear nowhere in
`src/`, which is what the check tests; §3.2, §3.3, §3.5 and §3.6 turn on
identifiers that are all here — what is missing is a *reader*, an *argument* or
a *call site*. The tool's `NOT_MECHANISED` names them so that a clean run
cannot be read as "the donors are fully carried", and §3.2 says what the fifth
resolver for its shape would be. **R-196 retired two of the four exemptions and
the build made it do so**: `--check` reports an exemption that no longer matches
a donor line, and the first `make` after the two ports went red naming
`SVF_PROJECTILE` and `hooked` — a rule written for a hazard that had not yet
happened, firing on its first real occasion.

### 3.1 (R-195.1) Withdrawn: the gate is here. What is real is that RA2 has one too, which three texts denied

**This entry said a person could pre-fire an RA2 countdown here. They cannot.**
`RA_HoldFire()` and `ClientLagThink` clear `BUTTON_ATTACK` out of both
`buttons` and `latched_buttons` at the latch for an arena client whose round is
not being fought, and `scenarios/ra2prefire` measures it in both signs on
`ra2map7` arena 6 — `STAT_AMMO` 50 → 46 through a countdown before, 50 → 50
after. It has been closed since spec 1.33, as **R-151** -- which no amendment row
has ever named, while 1.33's own row still ended "Left open deliberately".

The sweep found what it says it found: the donor's four `p_weapon.c` gate lines
are absent from this tree, and all four counterparts are the spine's. The error
was the inference — R-149's closing paragraph still reads *"Open, deliberately.
A person can still pre-fire an RA2 countdown here"*, two entries before the one
that closed it, so the absence looked like the gap that paragraph described
rather than a different answer to the same question one level up. **An absence
is a finding only once you know what else could be answering the question**,
which is the limitation §6 item 1 already names, arriving in the shape of a
false positive.

**What survives is the half about the donor, and it is load-bearing.** RA2 gates
firing at four fire arms — `Weapon_Generic` (`p_weapon.c:417`),
`Weapon_HyperBlaster_Fire` (`:818`), `Machinegun_Fire` (`:897`) and
`Chaingun_Fire`'s gunframe-21 loop (`:997`), each adding `ent->takedamage &&
arenas[…].state == ASTATE_FIGHTING`, at the pin `d20e1ce` as well as at the
bundle tip. Three texts in this tree said it gates nothing and each leaned on
that: R-149's closing sentence, R-151's parenthetical, and `RA_HoldFire()`'s own
docstring. All three are corrected.

**And the port is refused with a measurement rather than an opinion**, which is
the increment this produced. Asked of this merged tree instead of RA2's weapon
set, "where does a press become a shot" has ten answers:

| `src/p_weapon.c` | reached by | one of RA2's four? |
|---|---|---|
| `:685` `Weapon_Generic2` | every generic weapon — baseq2's nine, phalanx, ionripper, prox launcher, disruptor | yes (`:417`) |
| `:1378` `Weapon_HyperBlaster_Fire` | the hyperblaster's fire loop | yes (`:818`) |
| `:1457` `Machinegun_Fire` | the machinegun's fire loop | yes (`:897`) |
| `:1564` `Chaingun_Fire` | the chaingun's re-fire loop | yes (`:997`) |
| `:1019` `Throw_Generic` | the **hand grenade** (`ammo_grenades`, in every arena) and Ground Zero's tesla | **no** |
| `:2148` `Weapon_Trap` | the Reckoning Trap (`ammo_trap`) | **no** |
| `:2341` `Weapon_ChainFist` | the chainfist's re-attack loop | **no** |
| `:2551` `Weapon_ETF_Rifle` | the ETF rifle's re-fire loop | **no** |
| `:2659` `Weapon_Heatbeam` | the plasma beam's beam-on loop | **no** |

All five of the "no" rows are reachable under `arena` — three through R-182's
grant bits (`weapon_chainfist`, `weapon_etf_rifle`, `weapon_plasmabeam`, walked
in `give_weapons`' second pass) and two through the ammunition that is also the
weapon (`ammo_grenades` in every arena, `ammo_trap`/`ammo_tesla` with their
layer). Taking the donor's four hunks would have stopped the weapons RA2
noticed and left a fighter free to throw a hand grenade, drop a Trap, lay a
tesla, saw with a chainfist or hold a plasma beam through the whole countdown.
One clear at the latch covers all nine.

**Nine and not eleven**, and the two that drop out are why `lostref.py` strips
comments before it counts: `grep BUTTON_ATTACK src/p_weapon.c` returns eleven
sites, and two of them (`:907`, `:960`) sit inside baseq2's own
`Weapon_Grenade` at `:890`, which is in a **block comment** — Ground Zero
replaced that weaponthink with the `Throw_Generic` form at `:1108`, and the
itemlist selects the replacement. The first hand-grenade copy a grep finds
cannot run.

**One real divergence, now recorded.** The donor **queues** the press — its
clear of `latched_buttons` is *inside* the gated arm, so a tap during a
countdown fires on the bell. This tree drops it. Holding through the bell still
fires either way. The queue is not followed because it cannot be had at the
latch, only per site (the ten-versus-four trade above), and because a queued
press is a shot at the bell with no reaction time in it.

### 3.2 (R-195.2) RA2's chat-spam punishment is gone; its two fields are declared and savegame-persisted

`port_ra2:g_cmds.c:764–780`, in `Cmd_Say_f`, with the donor's own comment
*"Q2PRO's cvar-driven flood protection runs ahead of Rocket Arena's own
hardcoded spam counter; both stay in place"*:

```c
if (ent->client->spamcount == -1)   return;          // already punished
if (level.time < ent->client->spamtime + 2.0f) {
    ent->client->spamcount++;
    if (ent->client->spamcount > 5) {
        ent->client->spamcount = -1;
        gi.bprintf(PRINT_CHAT, "%s: Sorry guys, I talk too much\n", …);
        stuffcmd(ent, "disconnect\n");
```

This tree has `gclient_t.spamcount` and `.spamtime` (`src/g_local.h:1839–1840`)
and persists both (`src/g_save.c:532–533`) and **reads and writes neither**:
`grep -rn 'spamcount\|spamtime' src --include='*.c'` returns the two savegame
descriptor rows and nothing else. Six chat lines in two seconds disconnect you
in `rocketarena2` and do nothing here.

Two things follow. The behaviour is a decision to make either way — RA2's
punishment is harsher than `flood_*` and self-disconnecting a client from a game
library is worth thinking about — but it is currently made by omission. And the
shape is one `tools/deadvalue.py` cannot see (R-192): its space is *values a
field can hold*, and this field holds none because nothing writes it. **A field
declared and persisted with no reader and no writer is a fifth resolver**, and
this is its first specimen.

### 3.3 (R-195.3) `stats_logchat` logs private messages only

The donor calls `q2log_playerChat(text)` from `Cmd_Say_f` — which serves both
`say` and `say_team` — and again from `OSP_talkto_cmd`. This tree calls
`OSP_Stats_Chat()` **once**, at `src/tourney/osp_cmds.c:119`, inside
`OSP_talkto_cmd`. Neither `Cmd_Say_f` nor `OSP_sayteam_cmd` calls it.

So with `stats_logchat 1` the stats file records `talkto` and nothing else,
while `osp_stats.h` and `doc/cvars.md` describe the cvar as "log chat lines".
One call in `Cmd_Say_f`, behind `G_IsOspRuleset()`, closes it.

### 3.4 (R-195.4) One of tourney's two ZBot heuristics, and the flag that fed it

The donor has two: `OnBotDetection(ent, "i")` from the view-angle detector
(carried here as R-OSP-4, `OSP_botDetect`), and `OnBotDetection(ent, "cr")` in
`ClientThink`'s fire arm:

```c
if (bot_watch && ent->client->resp.osp_r008) { OnBotDetection(ent, "cr"); return; }
```

`osp_r008` is set in `ClientConnect` when the client's userinfo **begins** with
`\name\` — spelled obfuscated in the donor as `{'`','r','e','q','i','`'}` with 4
subtracted from each byte. Neither the flag nor the trigger is in this tree, and
`osp_r008` appears in no document.

The heuristic is weak — plenty of clients put `name` first, so it may well fire
on everybody — so declining it is a defensible call. It is not currently a call:
it is a gap. R-192's exemption list is where it belongs, with that reason.

### 3.5 (R-195.5) R-191's two spawn exclusions are applied to the refusal, not to the selection

The donor threads the client being placed all the way down:
`SelectDeathmatchSpawnPoint(ent)` → `SelectRandomDeathmatchSpawnPoint(ent)` /
`SelectFarthestDeathmatchSpawnPoint(ent)` → `PlayersRangeFromSpot(spot, ent)`,
and that function skips `player == ent` and
`player->client->resp.entered != ENTERED_ENTERED`.

This tree's selectors take no argument (`src/p_client.c:1185`, `:1234`, `:1303`)
and call the shared `PlayersRangeFromSpot(spot)`, which skips the dead, and —
under `arena` only — observers (R-77/R-RA-4). R-191 put the same two exclusions
into `OSP_spawnRefused`, for the 60-unit refusal, and argued they are
*"load-bearing rather than tidy"*: **the argument applies unchanged one call up.**
With `DF_SPAWN_FARTHEST` under the OSP four, "farthest from any player" is
measured against the body of the client being placed and against every observer
that has one, so a client joining from the queue is sent as far as possible from
where it was watching, and an observer parked on a point moves everybody off it.

### 3.6 (R-195.6) Two OSP log events at their donor sites, and the field the other site reads

`TossClientWeapon`. The donor logs, and clears `resp.osp_r200` (the entnum of
the powerup the player picked up) in both arms:

* `q2log_dropItem("Quad", drop - g_edicts, self)` where the quad is dropped;
* `q2log_expireItem("Quad", self, resp.osp_r200)` where `DF_QUAD_DROP` is off and
  the quad's timer had already run out.

This tree's `TossClientWeapon` (`src/p_client.c:578`) does neither and leaves
`osp_r200` set. The reader is `src/p_view.c:1231`, which is per-frame and alive
—so a quad **dropped** on death reaches the log, if at all, as an *expiry* at
whatever later frame the timer runs out, rather than as a drop at the moment of
death.

`ShutdownGame`. The donor's is `if (!level.intermission_framenum)
q2log_logAccuracy();` — dump the accuracy table when the server goes down
*outside* intermission, i.e. with a match still running — followed by
`q2log_gameEnd(reason, …)`. This tree carries the second half: `sl_GameEnd()`
and `OSP_Stats_Shutdown(reason)` (`src/g_main.c:151`) write the end event with
the same reason, `map` / `gamemap` / `quit` / `server`. It does not carry the
first: `OSP_Stats_AccuracyAll()` is called from six places in `osp_main.c`, all
of them *match*-end conditions, so a `map` typed mid-match ends the log without
the accuracy the donor would have written. The donor's guard is the giveaway
that this is the case it was for.

### 3.7 (R-195.7) Three one-line deltas, each defensible, none recorded

These are the three the check *can* see: each turns on an identifier that
exists nowhere in `src/`, and each is exempt by name in `tools/fnsweep.py`.

* **`SVF_PROJECTILE` on CTF's blaster bolts.** `port_ctf:g_weapon.c:320` sets
  `bolt->svflags = SVF_PROJECTILE`; the spine and this tree set
  `SVF_DEADMONSTER`, and the tree never names `SVF_PROJECTILE` at all, though
  `inc/shared/game.h:47` defines it ("treat as `CONTENTS_PROJECTILE` for
  collision"). It is q2pro's own divergence between its `baseq2` and its `ctf`,
  not Threewave's feature, and R-40 already counted the line among CTF's 167
  stale hunks that §7 rule 1 decides. Recorded here because "decided by a rule
  that names 167 hunks" is not the same as "somebody looked at this one", and
  under `ctf` upstream q2pro sets it.
* **The `MOD_GRAPPLE` obituary text.** Two donors claim one means of death:
  Threewave's "was caught by …'s grapple" and tourney's "was hooked to death
  by". This tree uses Threewave's under every ruleset (`p_client.c:425`), so a
  hook kill in `tdm` reads as CTF's. The *log* is tourney-correct —
  `osp_stats.c:760` maps the MOD to `"Hook"` — so only the broadcast differs.
  §7 rule 4 territory with no row.
* **Ground Zero's `FindSubstituteItem` typo, fixed here.** The donor matches
  `item_spehre_defender`; `src/rogue/g_newdm.c` matches
  `item_sphere_defender`. The donor's substitution therefore never fired for
  the defender sphere and this tree's does — a behaviour change in this tree's
  favour, and the same class as the `FindItem("ionrippergun")` repair that
  moved the q2pro pin in 1.39 (R-PROV-5a), which did get a row.

---

## 4. What is correctly not carried — the negative result

This is the larger half of the sweep's output and it is worth stating, because
"the donor does X and we do not" is only a finding when the omission is not a
decision.

### 4.1 Whole donor files, and one that would have deleted behaviour

| donor | added `.c`/`.h` | carried | not carried |
|---|---:|---:|---|
| ctf | 4 | 4 | — |
| xatrix | 7 | 7 | — |
| rogue | 20 | 20 | — |
| ra2 | 27 | 7 | `gstats.c` (GameSpy), `darray`, `gbucket`, `hashtable`, `md5c`, `nonport`, `net_compat.h`, `config.h`, and its vendored `shared/` copies |
| osp | 47 | 32 | `q2log.c`, `nglog.c`, `ngmark.c`, `md5c`, `g_monsters.c`, `global.h`, `anorms.h`, `config.h`, and its vendored `shared/` copies |

The donors' `shared_shared.c` / `shared_m_flash.c` and `shared/*.h` are their
flattened copies of Q2PRO's own `src/shared` and `inc/`, which this tree takes
from the pin instead (R-CORE-9) — carried, under the upstream names.

**`g_monsters.c` is the one to notice.** It is 161 lines of monster spawn stubs
(`SP_monster_berserk(self) { G_FreeEdict(self); }` × 25) that tourney needs
because it deleted the monster files. Carrying it would have compiled, linked,
and silently removed every monster from every map — R-CORE-8's whole point,
arriving as a whole *file* rather than as a hunk.

### 4.2 Subsystems replaced, and every event checked

* **RA2's GameSpy / remote stats** (`gstats.c` + the four container libraries +
  `gslog.c`'s seven `net_*` helpers): R-114, R-RA-1a, R-SEC-7. 60 ABSENT lines
  and 40 of RA2's 110 lost calls, the rest of the stack living in files the
  matrix sets aside entirely (§4.1). `logfile 2`'s local log is untouched and
  the `netlog` cvar still parses (R-COMPAT-3).
* **Tourney's NetGames USA logging** (`q2log.c`, `nglog.c`, `ngmark.c`):
  replaced by `src/tourney/osp_stats.c`, one JSON object per line, four cvars.
  168 ABSENT lines. **All 27 `OSP_Stats_*` entry points have a caller** and 67
  of the donor's 88 `q2log_*` call sites map to one at the same site; of the
  other 21, 18 are R-MODE-5 relocations and three are §3.3/§3.6.
* **The accuracy table**: 15 inline `p_acc[]` writes → `OSP_accShot` /
  `OSP_accDamage` (R-91). The sweep's three `as_spine` verdicts in
  `T_RadiusDamage`, `Grenade_Explode` and `fire_lead` are that reduction, and
  R-91 already records the two behaviour changes it makes.
* **`match_mode`** → four values of `g_ruleset` (R-OSP-12): 222 ABSENT lines,
  every one an `m_mode` comparison.
* **The `STAT_*` macros** → the `SID_*` map and the composed statusbar
  (R-OSP-7a): 83 + 6 ABSENT lines. The four OSP bar variants are present —
  `sb_tourney_tail(sb, alt, team)` and `G_StatusbarVariant(alt, team)`.
* **Both menu engines**: `ctf_PMenu_*` / `osp_PMenu_*`, `ra_MenuClose` /
  `close_menus` for RA2's `clear_menus` (§7 rule 4, R-MENU-1).
* **The donors' file I/O**: `glob`/`_findfirst`/`getcwd`/`CreateProcess` →
  `g_fs.c` (R-BOT-8, R-BOT-26). `autolaunchbspc` stays off (R-SEC-7).
* **`sprintf`/`strcat`/`strncpy`/`vsprintf`**: 490 ABSENT lines, the single
  largest cluster, all replaced by `Q_snprintf` / `Q_strlcpy` / `Q_strlcat`.

### 4.3 Donor code that is dead in the donor

Eight cases where the sweep says "missing" and the donor's own copy cannot run.
Each is a positive result for the merge:

| donor | definition | why it is dead there |
|---|---|---|
| ctf | `TH_viewthing`'s robotron cycle | `static int robotron[4]` is never written, so `modelindex` becomes 0 |
| rogue | everything behind `#ifndef KILL_DISRUPTOR` | Ground Zero `#define`s it to 1 (R-CORE-3, `g_local.h:93`) |
| ra2 | `DisplaySimpMenu`, `MySelect`, `MySelect2`, `MySelect3`, `PrintMenu*` | defined in `menu.c`, prototyped in `menu.h`, called from nowhere |
| ra2 | `checkvwepmodel`, `mylcase` | same: `arena.c` + `arena.h`, no caller |
| ra2 | `InitBodyQue`'s `movetype = MOVETYPE_TOSS` | `CopyToBodyQue` assigns `body->movetype = ent->movetype` before linking |
| osp | `GrapplePull`'s `damagescale` | the reconstruction's own note: "written, overwritten and never read" |
| osp | `ClientObituary`'s `goto` labels, `Cmd_Say_f`'s `clear_args`, … | 62 ABSENT lines that are labels and locals |
| rogue | `count1`/`count4` in `monsterlost_checkhint` | counters declared, never read |
| **all five, and the spine** | **`ClientObituary`'s `case MOD_SUICIDE: message = "suicides"`** | **id's, and 28 years old.** `MOD_SUICIDE` has exactly ONE writer anywhere — `Cmd_Kill_f`, which calls `player_die(ent, ent, ent, …)` — so `attacker == self` is always true, and the second switch's `default:` arm overwrites the first switch's message on every path that can reach it. A `kill` prints "X killed himself.", never "X suicides." |

**The last row is a *negative* result and it is here because it was asked as a
bug.** 1.49's play test read "`clockwatch` killed itself." off the wire after a
`kill` that had set `meansOfDeath = MOD_SUICIDE`, which looks exactly like a
merge that lost a case label. It is not: `ClientObituary` has **two** switches on
`mod` — one for world and environment deaths, then `if (attacker == self)` and a
second for self-inflicted weapons whose `default:` catches everything the second
switch does not name. The spine and all four donors that carry the function have
that structure character for character, and this tree's only differences from
tourney's copy in that block are the merged union: baseq2 3.20's neutral-gender
arms, which tourney's older base predates, plus Xatrix's `MOD_TRAP` and Ground
Zero's `MOD_DOPPLE_EXPLODE`. Tourney's *log* half is carried too and says the
same thing in its own words — `q2log_logDeath`'s `if (mod == MOD_SUICIDE) wname
= "Couldnt_Take_It_Anymore"` is `osp_stats.c`'s `suicide` event, observed in the
1.49 stats files.

**And "it**self**" rather than "her**self**" was the test client, not the game.**
`IsNeutral` reads the `gender` userinfo key (`p_client.c:200`) and a libq2 bot
never sets one; a real Quake II client derives `gender` from the skin and sends
it. A headless client is the right instrument for four of the five channels and
the wrong one for anything a real client computes on its own, which is worth
knowing before the next scenario asserts on a pronoun.

### 4.4 Renames verified rather than assumed

`fire_heat`/`monster_fire_heat` → `xatrix_*` and `rogue_*` (§7 rule 4,
`g_local.h:1066`); `CheckFlood` → `FloodProtect`; `clear_menus` →
`close_menus`; `STAT_RUNE_*` → `SID_OSP_RUNE_*`; `TECH1..5_INDEX` → the
`SID_OSP_RUNE_*` table at `bl_main.c:1094`; `Cmd_LastWeap_f` →
`Cmd_WeapLast_f` — same `weaplast` command string, and this tree's checks the
inventory and `IT_WEAPON` where CTF's calls `use` on a weapon you may no longer
hold; `pers.spectator` →
`pers.osp_speedstrikes`; `ghost_t` → `struct ghost_s`; `allow_*` weapon cvars
present at `osp_main.c:1388`; `client_maxfps` / `client_maxrate` /
inactivity-to-observer all present in `osp_observe.c` — and `osp_observe.c:367`
records that the donor's copies of the first two *did nothing at all*.

`OSP_speedDetect` is worth one line of its own: this tree keeps the kick and
drops what the donor sent with it — a random temp-entity id followed by up to
two random bytes, unicast into the offender's stream. That is a deliberate
deviation and it is the right one.

---

## 5. Confidence: what the sweep re-derived on its own

The sweep was run without reading `doc/reconciliation.md` first, and then
cross-referenced against it. It independently landed on R-KEY-1
(`st.pausetime`), R-40 (CTF's 167 stale hunks, including the exact
`SVF_DEADMONSTER`→`SVF_PROJECTILE` line the row names), R-CORE-8 with R-64's
sixth rule (all seven RA2 monster-deletion sites), R-91, R-114, R-CORE-3,
R-OSP-7a, R-OSP-12, R-MENU-1, R-191, R-77/R-RA-4 and §7 rules 1 and 4 — each
time by measurement rather than by being told. Of the twelve ledger claims the
sweep put to the donors directly, eleven held; the twelfth is R-149's closing
sentence (§3.1).

That is the useful calibration, and the twelfth claim cuts the other way:
R-149's "Open, deliberately" was stale, the sweep believed it, and §3.1 had to
be withdrawn. On this tree a function-level sweep of all five donors yields
**six** findings, four of them single call sites — plus one withdrawal, which is
the cost of reading a ledger as if it described the code.

---

## 6. What this method cannot see

1. **Semantics inside a matching line.** A line present at the right site is
   counted as carried. A gate around it that is wrong, an argument reordered, a
   constant changed inside an otherwise identical line — none of that is caught
   here. `tools/donorgate.py`, `tools/deadvalue.py` and `tools/units.py` are the
   instruments for those questions.
2. **Behaviour that is an absence.** A donor that *stops* doing something by
   deleting a call is caught (`kept_removed`, and `tools/lostref.py`); a donor
   that stops doing something by changing a data table is not.
3. **Definitions with no brace body and no initialiser.** A tentative definition
   (`int match_paused;`) is a declaration to this extractor, so a handful of
   donor globals read as `absent` when the tree has them — verified by hand for
   `match_paused`, `pause_time`, `endlvl_frame`, `m_mode`.
4. **Order.** Two calls in the wrong order are two calls present.
5. **The donor's own bugs are not flagged as such.** §4.3 was found by reading,
   not by the tool; the tool only said "this line is not here".
6. **The pins.** Everything above is measured against the bundle tips of
   R-PROV-3. RA2's live `q2pro-enhancements` is two commits past the pin and
   tourney's is three; `doc/provenance.md` tracks that separately and this sweep
   does not re-ask it, except that §3.1's four gates are present at `d20e1ce`
   too, which was checked by hand.
7. **Its own finding rule is narrower than its output.** `--check` fails on an
   unattributed ABSENT line and nothing else, which is two of the six findings
   above. The report is the read-through, and the tool's `NOT_MECHANISED` list
   is there so that a green build is not mistaken for a complete answer. Two of
   its entries name the resolver that would catch their shape: a field with
   neither reader nor writer (§3.2), and a call-count question (§3.3, §3.6) —
   which the call delta of §1.5 already asks and which nothing yet fails the
   build on.
8. **A ledger entry is not the state of the code, and this method reads
   ledgers.** §3.1 was withdrawn because R-149 recorded a gap as open and the
   increment that closed it never came back to say so. The sweep can only tell
   you that a donor's lines are absent; whether that is a gap depends on what
   else answers the question, and the cheapest place to look — the ledger — is
   the one place that can be confidently, specifically out of date.
