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
| `sv ruleset` | Reports the resolved ruleset, content layers, modifiers, every predicate's answer, the legacy `deathmatch`/`coop` values, and an entity census split into live monsters, corpses and gibs. Self-checking: names any live monster present under a ruleset that forbids them. R-VER-18, and R-VER-2's boot matrix depends on it. **1.23 adds two lines**, `bots` and `botplace`: the bot census with its slot list, and where each bot ended up in the running ruleset's own terms — CTF team, arena and roster, or tourney entry and ready state. `FL_BOT` and `FL_BOTCLIENT` are counted separately on purpose (R-CORE-14). Both print under every ruleset that accepts bots, including when there are none |

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
  `botpause`, `menu`, `modelindex`, `soundindex`, `imageindex`, `indexprobe`,
  `inventory`, `botlibdump`, `clientdump`, plus the bot-issued and GPS/macro
  commands.
  Server-only by default: `serveronlybotcmds` defaults to **1** (R-BOT-25),
  reversing the 1999 default, because the 1999 path exposed `bl_spawn.c`'s
  32-byte bot-name copies to clients
* tourney's 137 and RA2's 39 (R-OSP-2, R-RA-2)
* tourney's mode-gated commands, which R-VER-16 checks accept and reject per
  `match_mode`: `highscores` only in mode 0, `queue`/`line`/`order` only in
  mode 3, `captain`/`invite`/`lockteam` only in mode 2

## Rocket Arena 2 — added in spec 1.17

All gated on `g_ruleset arena`; a command named here does nothing under any
other ruleset.

| command | notes |
|---|---|
| `arenaadmin` | the per-arena settings menu |
| `admin` | **third meaning of this name.** CTF's is the election; RA2's is the arena admin login. One ruleset is live at a time, so each keeps the bare name |
| `playerlist` | **third implementation.** baseq2's reports `resp.spectator`, CTF's adds team and ghost code, RA2's is arena-scoped |
| `score` | not a new command: under `arena` it *cycles* arena board → server-wide → off, where every other ruleset toggles |
| `menuhelp` | the menu key legend |
| `say_world` | reaches everyone even under teamplay, prefixed `W:` |
| `grap_on`, `grap_off` | the offhand grapple latch. Mapped onto `ctf_hookstate`, because there is one grapple (§7 rule 6) |
| `listkeys`, `listmaps`, `nextmap` | the map loop's reporting |
| `getdebugcode`, `pcount`, `play` | accepted and ignored, as in the donor: clients bind them and expect the server to swallow them |

`invnext`, `invprev`, `inven`, `invuse` and `invdrop` gain arena arms rather than
new commands — R-MENU-4 gives the menu owner first claim on that input.


## The bot layer — added in spec 1.22 (Phase 6)

R-BOT-24's set, reachable from the console as `sv <cmd>` and from a client where
`serveronlybotcmds` allows it — which by default it does not (R-BOT-25). Six
are console-only regardless, because they dump tables at the server.

| command | who | notes |
|---|---|---|
| `addbot <name> <skin> <charfile> <charname>` | both | queues one bot (R-BOT-18); never creates one inside `SpawnEntities` or a `ClientConnect`. A duplicate name is refused, except under `tourney`, which auto-suffixes (R-BOT-29) |
| `addrandom [n]` | both | picks from `bots.cfg`, skipping bots already in the game |
| `removebot [name\|all]` | both | `all` is new: `BotDestroyAll()`, which `ShutdownGame` needs too |
| `becomebot` | both | turns a human client into a bot |
| `botpause` | both | toggles `botglobals.nobotai`. The SDK's own was behind `#ifdef BOT_DEBUG`, which is defined in neither donor tree, so this command **could not work** before 1.22 |
| `menu [rcon_password]` | client | the Gladiator menu tree (R-BOT-28). Dropped from `osp-tourney`'s `bl_cmd.c` because that mod has its own menus; it comes back with the menu |
| `bbox` | client | R-BOT-27's fourteen-line bounding box, on the entity you are looking at when observing. Behind `cheats`, because it shows through walls |
| `name`, `skin`, `gender` | client | the three the brain issues on its own behalf |
| `teamhelp [who]`, `teamaccompany [who]` | client | a `say_team` naming the best item within 500 units |
| `checkpoint <name>` | client | a `say_team` naming a position the bots can route to |
| `gps <x> <y> <z>` | client | v0.93's compass. The donor kept `ShowGPSText` and dropped the command that reaches it, leaving the function dead (reconciliation R-99) |
| `modelindex`, `soundindex`, `imageindex` | console | the three index tables R-BOT-11 sizes from `game.csr`. Empty rows are skipped: the 1999 table had 256 entries and this one has up to 8192 |
| `indexprobe <model\|sound\|image> <index>` | console | R-VER-6's control (1.23). Asks `BotIndexRecord` about one index and **writes nothing**. It exists because the tables are sized from `game.csr`, so the engine cannot issue an index they have no room for and the overflow arm cannot fire on a real server — which leaves a check that only ever sees silence and cannot tell "it did not overflow" from "the reporting is broken" |
| `inventory` | console | `itemlist[]` by name and index |
| `botlibdump` | console | one block per loaded library, its path, its user count and the clients using it. R-VER-3 reads this |
| `clientdump` | console | every client slot: free, human, or bot with its library. R-VER-3 reads this too |

`invnext`, `invprev` and `invuse` gain a `MENU_BOT` arm rather than new
commands, so the help screen's "your inventory key to select" becomes true —
the donor drove the cursor from `forwardmove`/`sidemove` alone, and that is
still wired (R-MENU-4).
