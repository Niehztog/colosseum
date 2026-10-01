# Entity inventory

The enforcement point for the key contract: the union of all six donors' spawn keys is one table, and every key any donor accepted is still accepted.

For each key: the key text, its `spawn_temp_t`/`edict_t` member, its `F_*` type macro, and the donor that introduced it.

**The counts below accrue by layer**, in the order the donors were merged, and each is the figure for the tree at that point. `tools/counts.py` measures the finished tree, so what it prints today is the last of them: **241**.

## The key column is frozen

A `.bsp` names entity fields as **literal strings**, matched at runtime against the key column of `spawn_fields[]` and `temp_fields[]`. Renaming a member therefore changes the key text that shipped maps use. **A member rename that changes a key is a spec change, not a refactor**, and the QUAKED documentation block is part of the same contract and moves with it.

The named case: Q2PRO's *"Convert monster timers to frame numbers."* renamed `monsterinfo_t.pausetime` to `pause_framenum`; replayed tree-wide it also hit the *unrelated* `spawn_temp_t` member of the same name, silently breaking every map that sets `pausetime` on a `func_timer` -- `ED_ParseEdict` reported `pausetime is not a field` and banks of timers that should stagger fired in lockstep, and there is a regression check for it.

The `F_*` macro on a row must match the member's C type. Ground Zero retyped that same `pausetime` to `int` while its row still said `F_FLOAT`, so the parser wrote a float bit pattern into an int.

Both are checked mechanically, in the build, not by review: the frozen key by `tools/gates.py`, and the types -- every row of both tables against its member, and no key twice -- by `tools/dsweep.py`.

## Inherited from baseq2

**149 classnames** by the definition "anything `ED_CallSpawn` will match", which is two tables: `spawn_funcs[]` (109 rows) plus every `itemlist[]` entry carrying a `.classname` (40), because `ED_CallSpawn` checks the item list first.

The published figure is 157. The gap of 8 is unresolved. Evidence that the definition is right rather than the count: the same definition reproduces CTF's published 163 **exactly**. So the question is what the 157 counted, not whether the counter works. A figure with no script behind it is indicative until reconciled.

```sh
tools/counts.py --list classnames
tools/counts.py --list spawn     # the spawn_funcs[] table alone
tools/counts.py --list items     # itemlist[] classnames alone
```

Tourney's five extra spawn keys (`botlib`, `name`, `skin`, `charfile`, `charname`) arrive with OSP Tourney and live in `g_spawn.c`'s `temp_fields[]`, where Q2PRO moved that table. They are the map-placed `bot`'s (below): `SP_bot` hands the last four to `addbot`, and `botlib` is parsed and read by nothing -- the library is the `botlib` cvar's, one per server, as in 1999's `SP_bot`.

## Threewave CTF

**231 classnames** at this point (159 in the spawn table and 72 item classnames), up from 220 with the mission packs alone. CTF adds **eleven** -- four spawn rows and seven items -- and *shares* two classnames with Ground Zero without adding a row for either.

| classname | kind | note |
|---|---|---|
| `info_player_team1`, `info_player_team2` | spawn | the team spawn sets `SelectCTFSpawnPoint` chooses from |
| `misc_ctf_banner`, `misc_ctf_small_banner` | spawn | scenery |
| `weapon_grapple` | item | always owned, never in the world; issued by `InitClientPersistant` under `ctf` only |
| `item_flag_team1`, `item_flag_team2` | item | in `itemlist[]` in every ruleset through the merged union, so a CTF map loads anywhere; `SpawnItem` removes them outside `ctf` rather than leaving them as scenery |
| `item_tech1` ... `item_tech4` | item | **four**, not five. `g_ctf.c`'s own `tnames[]` lists four, and the "five" was tourney's rune count |
| `trigger_teleport`, `info_teleport_destination` | spawn, **shared** | `trigger_teleport` has three implementations -- Threewave's under `ctf`, RA2's under `arena` (a bare brush on baseq2's `teleporter_touch`), Ground Zero's under every other ruleset -- and `info_teleport_destination` two, Threewave's under `ctf` and Ground Zero's elsewhere. `g_misc.c` dispatches on the ruleset and each donor's spawn function keeps its prefix |

**No new spawn key.** CTF adds no `spawn_temp_t` member and no `spawn_fields[]`/`temp_fields[]` row, so the frozen key column above is untouched -- `tools/dsweep.py` is clean on the merged tables. The two entities that *look* like they need one, the flags, are ordinary `itemlist[]` rows and are configured entirely from their classname.

One spawnflag note: CTF's `trigger_teleport` and RA2's read **no** spawnflags at all while Ground Zero's reads four (`player_only`, `silent`, `ctf_only`, `start_on`), so unlike `trigger_push` there is no bit collision to resolve here -- only the classname, and the ruleset settles that.

## Rocket Arena 2 and OSP Tourney

**238 classnames** at this point. RA2 adds one and OSP Tourney six, and neither adds a spawn key beyond tourney's five above:

| classname | kind | note |
|---|---|---|
| `func_illusionary` | spawn | RA2's non-solid brush: drawn, walked through, never used. **`arena` only** -- no other donor has the classname, so outside `arena` it is freed with `doesn't have a spawn function` |
| `bot` | spawn | the map-placed bot, from the bot layer that arrived with OSP Tourney's port: it queues an `addbot` from its `name`, `skin`, `charfile` and `charname` keys and frees itself -- queued, because it runs inside `SpawnEntities`. Freed outright where `G_BotsAllowed()` is false, which is `sp` or `bots 0` |
| `item_rune1` ... `item_rune5` | item | tourney's five runes -- resist, strength, haste, regeneration, vampire -- placed by tourney's own spawner as `runes_enable` selects them, on one mesh told apart by colour shell. One a map places is freed outside the four OSP rulesets |

## The Gladiator extras' three entities

**241 classnames** (`tools/counts.py`: 164 in the spawn table and 77 item classnames, none in both), three of them new here. All three come from the 1999 module, all three are gated on a cvar that is **off by default**, and for a spawn function "off" means the entity is freed rather than created and inert -- an inert entity still occupies an edict slot and still shows in every census.

The **classnames stay in `spawn_funcs[]` either way**, which is the property worth having: a map that uses one loads without `doesn't have a spawn function` whichever way the cvar is set, and `sv extras` reports that they are registered.

| classname | cvar | keys | notes |
|---|---|---|---|
| `trigger_counting` | `g_triggercounting` | `count`, `target`, `targetname`, `style` | An intermediary for an action that takes several inputs. `style` is the door state to drive the target into when the count reaches zero: **0 is STATE_TOP and 1 is STATE_BOTTOM**, and those two numbers are part of the map contract. They are spelled out in `g_trigger.c` rather than exported from `g_func.c`, because exporting a door's internal states into a shared header to satisfy one trigger is the wrong direction |
| `trigger_log` | `g_triggerlog` | `message` | Writes `message` into the game log the first time a client touches it, and can be re-triggered after **two frames** of not being touched. The donor wrote `level.time + FRAMETIME * 2`; this tree counts frames |
| `func_button_rotating` | `g_rotatingbutton` | `move_angles`, `move_origin`, `speed`, `killtarget`, `style`, `dmg`, `target`, `pathtarget`, `deathtarget` | A three-position rotating button -- top, middle, bottom -- with a spawnflag choosing the axis (1 X, 2 Y, else Z), 4 to skip the middle position and 8 to reverse. `target` fires at the top, `pathtarget` at the bottom, `deathtarget` in the middle, and `killtarget` is the button's **name** in the game log -- `unknown` when it has none. It is still a kill target too, as in the 1999 module: each position fires through `G_UseTargets`, which frees every entity whose `targetname` equals it |

**No new spawn key.** `move_angles` and `move_origin` are already in `spawn_fields[]` -- Xatrix's turret uses both -- so the frozen key column above is untouched and `tools/dsweep.py` is clean on the merged tables. The rotating button reads `move_angles[0..2]` as its three positions and `move_origin[0..2]` as the counter values it hands to a `trigger_counting` chain, which is why a button and a counting trigger are one feature in two entities.
