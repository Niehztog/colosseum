# Command inventory

Deliverable D4. Every client and server command Colosseum accepts, what it does,
which ruleset it belongs to, and whether it is server-only.

Generated from `tools/counts.py --list commands` and annotated.

## Phase 0: baseq2 only

`tools/counts.py` measures **32** client commands in `src/`, counting every
literal the dispatcher compares against. R-BASE-3 publishes **27**. The surplus
of five is an open question about the definition, not a defect: the likely cause
is `Cmd_*` handlers the spec did not treat as client commands. Under R-TOOL-2 the
tool is the authority and 27 is indicative until reconciled; neither figure gates
Phase 0. See `reconciliation.md` §3.3.

```sh
tools/counts.py --list commands
```

## Server commands added by Colosseum

| command | what it does |
|---|---|
| `sv ruleset` | Reports the resolved ruleset, content layers, modifiers, every predicate's answer, the legacy `deathmatch`/`coop` values, and an entity census split into live monsters, corpses and gibs. Self-checking: names any live monster present under a ruleset that forbids them. R-VER-18, and R-VER-2's boot matrix depends on it |

## Reserved for later phases

* the Gladiator bot commands of R-BOT-24 — `addbot`, `removebot`, `addrandom`,
  `botpause`, `menu`, `modelindex`, `soundindex`, `imageindex`, `inventory`,
  `botlibdump`, `clientdump`, plus the bot-issued and GPS/macro commands.
  Server-only by default: `serveronlybotcmds` defaults to **1** (R-BOT-25),
  reversing the 1999 default, because the 1999 path exposed `bl_spawn.c`'s
  32-byte bot-name copies to clients
* tourney's 137 and RA2's 39 (R-OSP-2, R-RA-2)
* tourney's mode-gated commands, which R-VER-16 checks accept and reject per
  `match_mode`: `highscores` only in mode 0, `queue`/`line`/`order` only in
  mode 3, `captain`/`invite`/`lockteam` only in mode 2
