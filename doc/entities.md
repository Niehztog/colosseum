# Entity inventory

Deliverable D4, and the enforcement point for R-KEY-3: the union of all six
donors' spawn keys is one table, and every key any donor accepted is still
accepted.

For each key: the key text, its `spawn_temp_t`/`edict_t` member, its `F_*` type
macro, and the donor that introduced it.

## The key column is frozen (R-KEY-1)

A `.bsp` names entity fields as **literal strings**, matched at runtime against
the key column of `spawn_fields[]` and `temp_fields[]`. Renaming a member
therefore changes the key text that shipped maps use. **A member rename that
changes a key is a spec amendment, not a refactor**, and the QUAKED
documentation block is part of the same contract and moves with it.

The named case: Q2PRO's *"Convert monster timers to frame numbers."* renamed
`monsterinfo_t.pausetime` to `pause_framenum`; replayed tree-wide it also hit the
*unrelated* `spawn_temp_t` member of the same name, silently breaking every map
that sets `pausetime` on a `func_timer` — `ED_ParseEdict` reported
`pausetime is not a field` and banks of timers that should stagger fired in
lockstep. R-VER-14 is the regression check.

R-KEY-2: the `F_*` macro on a row must match the member's C type. Ground Zero
retyped that same `pausetime` to `int` while its row still said `F_FLOAT`, so the
parser wrote a float bit pattern into an int.

Both are checked mechanically by `keycontract.py`, which runs in the build
(R-TOOL-3), not by review.

## Phase 0: baseq2 only

**149 classnames** by the definition "anything `ED_CallSpawn` will match", which
is two tables: `spawn_funcs[]` (109 rows) plus every `itemlist[]` entry carrying
a `.classname` (40), because `ED_CallSpawn` checks the item list first.

R-BASE-1 publishes 157. The gap of 8 is unresolved. Evidence that the definition
is right rather than the count: the same definition reproduces CTF's published
163 **exactly**. So the question is what the 157 counted, not whether the counter
works. Under R-TOOL-2, 157 is indicative until reconciled and does not gate
Phase 0.

```sh
tools/counts.py --list classnames
tools/counts.py --list spawn     # the spawn_funcs[] table alone
tools/counts.py --list items     # itemlist[] classnames alone
```

Tourney's five extra spawn keys (`botlib`, `name`, `skin`, `charfile`,
`charname`) arrive in Phase 5 and live in `g_spawn.c`'s `temp_fields[]`, where
Q2PRO moved that table (R-OSP-6).

## Phase 3: Threewave CTF

**231 classnames** now (164 in the spawn table, 72 item classnames, 5 in both),
up from Phase 2's 220. CTF adds **eleven** — four spawn rows and seven items —
and *shares* two classnames with Ground Zero without adding a row for either.

| classname | kind | note |
|---|---|---|
| `info_player_team1`, `info_player_team2` | spawn | the team spawn sets `SelectCTFSpawnPoint` chooses from |
| `misc_ctf_banner`, `misc_ctf_small_banner` | spawn | scenery |
| `weapon_grapple` | item | always owned, never in the world; issued by `InitClientPersistant` under `ctf` only |
| `item_flag_team1`, `item_flag_team2` | item | in `itemlist[]` in every ruleset (R-CORE-2's union), so a CTF map loads anywhere; `SpawnItem` removes them outside `ctf` rather than leaving them as scenery |
| `item_tech1` … `item_tech4` | item | **four**, not five. `g_ctf.c`'s own `tnames[]` lists four, and R-CTF-1's "five" was tourney's rune count (SPECS.md 1.12) |
| `trigger_teleport`, `info_teleport_destination` | spawn, **shared** | one classname, two implementations, no new row: Ground Zero brought both in Phase 2 and CTF brings its own version of each. `g_misc.c` dispatches on the ruleset and each donor's spawn function keeps its prefix — `reconciliation.md` R-44 |

**No new spawn key.** CTF adds no `spawn_temp_t` member and no
`spawn_fields[]`/`temp_fields[]` row, so the frozen key column above is
untouched — `keycontract.py` is clean on the merged tables. The two entities that
*look* like they need one, the flags, are ordinary `itemlist[]` rows and are
configured entirely from their classname.

One spawnflag note: CTF's `trigger_teleport` reads **no** spawnflags at all while
Ground Zero's reads four (`player_only`, `silent`, `ctf_only`, `start_on`), so
unlike R-27's `trigger_push` there is no bit collision to resolve here — only the
classname, and the ruleset settles that.

## Phase 8: R-EXTRA's three entities

**240 classnames** (`tools/counts.py`), three of them new here. All three come
from the 1999 module, all three are gated on a cvar that is **off by default**,
and for a spawn function "off" means the entity is freed rather than created and
inert — an inert entity still occupies an edict slot and still shows in every
census.

The **classnames stay in `spawn_funcs[]` either way**, which is the property
worth having: a map that uses one loads without `doesn't have a spawn function`
whichever way the cvar is set, and `sv extras` reports that they are registered.

| classname | cvar | keys | notes |
|---|---|---|---|
| `trigger_counting` | `g_triggercounting` | `count`, `target`, `targetname`, `style` | An intermediary for an action that takes several inputs. `style` is the door state to drive the target into when the count reaches zero: **0 is STATE_TOP and 1 is STATE_BOTTOM**, and those two numbers are part of the map contract. They are spelled out in `g_trigger.c` rather than exported from `g_func.c`, because exporting a door's internal states into a shared header to satisfy one trigger is the wrong direction |
| `trigger_log` | `g_triggerlog` | `message` | Writes `message` into the game log the first time a client touches it, and can be re-triggered after **two frames** of not being touched. The donor wrote `level.time + FRAMETIME * 2`; this tree counts frames (R-VER-21) |
| `func_button_rotating` | `g_rotatingbutton` | `move_angles`, `move_origin`, `speed`, `killtarget`, `style`, `dmg`, `target`, `pathtarget`, `deathtarget` | A three-position rotating button — top, middle, bottom — with a spawnflag choosing the axis (1 X, 2 Y, else Z), 4 to skip the middle position and 8 to reverse. `target` fires at the top, `pathtarget` at the bottom, `deathtarget` in the middle, and `killtarget` is used as the button's **name** in the game log rather than as a kill target |

**No new spawn key.** `move_angles` and `move_origin` are already in
`spawn_fields[]` — Xatrix's turret uses both — so the frozen key column of
R-KEY-1 is untouched and `keycontract.py` is clean on the merged tables. The
rotating button reads `move_angles[0..2]` as its three positions and
`move_origin[0..2]` as the counter values it hands to a `trigger_counting`
chain, which is why a button and a counting trigger are one feature in two
entities.
