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
| `sv slots` | Reports the active ruleset's resolved stat-slot map (slot, kind, logical id), every row the map declares that resolution *dropped* and why, and the composed statusbar with its byte count. R-OSP-7/7a's counterpart to `sv ruleset`: a slot number that exists only inside the library cannot be checked from outside it |
| `sv ruleset` | Reports the resolved ruleset, content layers, modifiers, every predicate's answer, the legacy `deathmatch`/`coop` values, and an entity census split into live monsters, corpses and gibs. Self-checking: names any live monster present under a ruleset that forbids them. R-VER-18, and R-VER-2's boot matrix depends on it |

## Phase 3: Threewave CTF client commands

**51 client commands** in `src/` (`tools/counts.py`), of which fourteen are
CTF's. All are gated on the ruleset rather than registered conditionally, so a
player who types `team` under `dm` gets the chat fallback rather than silence.

| command | what it does |
|---|---|
| `team` | join red, blue or neither |
| `id` | toggle the player-id display (on by default — R-CTF-6) |
| `yes` / `no` | vote in an election |
| `ready` / `notready` | match readiness |
| `ghost` | reclaim a match slot with a ghost code after a disconnect |
| `admin` | open the admin menu, subject to `allow_admin` and `admin_password` |
| `stats` | per-player capture and defence statistics |
| `warp` | propose a map from `warp_list` |
| `boot` | admin: remove a player |
| `observer` | leave the game for the chase camera |
| `hookon` / `hookoff` | R-CTF-3's offhand hook. Two commands rather than a `+hook` alias, which is what uGladQ2 v0.97u did and what every 1999 config binds |

Three baseq2 commands change meaning under `ctf` and keep it elsewhere:

| command | `dm` / `sp` | `ctf` |
|---|---|---|
| `say_team` (and `steam`) | `Cmd_Say_f(team)` — everyone with a matching skin | `CTFSay_Team` — the sender's team, with flag and tech macros |
| `playerlist` | baseq2's, which reports `resp.spectator` | `CTFPlayerList`, which reports team, ghost code and ready state |
| `inven` | opens the inventory | opens the join menu when you have no team; otherwise the inventory |

`drop tech` is `Cmd_Drop_f`'s one special case: four tech classnames, one key.

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
