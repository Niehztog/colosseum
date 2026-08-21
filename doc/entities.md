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
