# Command inventory

Every client and server command Colosseum accepts, what it does, which ruleset it belongs to, and whether it is server-only.

Generated from `tools/counts.py --list commands` and annotated. **The counts accrue by layer**, in the order the donors were merged; `tools/counts.py` measures the finished tree, so what it prints today is the last of them: **104**.

## Inherited from baseq2

`tools/counts.py` measures **32** client commands in `src/`, counting every literal the dispatcher compares against. The published figure is **27**. The surplus of five is an open question about the definition, not a defect: the likely cause is `Cmd_*` handlers the spec did not treat as client commands. A figure with no script behind it is indicative, so the tool is the authority and 27 is indicative until reconciled; neither figure gates the build, which asks `counts.py` for duplicates rather than for a total. See `SPECS.md` section 3.

```sh
tools/counts.py --list commands
```

## Server commands added by Colosseum

| command | what it does |
|---|---|
| `sv slots` | Reports the active ruleset's resolved stat-slot map (slot, kind, logical id), every row the map declares that resolution *dropped* and why, and the composed statusbar with its byte count. The slot map's counterpart to `sv ruleset`: a slot number that exists only inside the library cannot be checked from outside it |
| `sv ruleset` | Reports the resolved ruleset, content layers, modifiers, every predicate's answer, the legacy `deathmatch`/`coop` values, and an entity census split into live monsters, corpses and gibs. Self-checking: names any live monster present under a ruleset that forbids them. The boot matrix depends on it. **1.23 adds two lines**, `bots` and `botplace`: the bot census with its slot list, and where each bot ended up in the running ruleset's own terms -- CTF team, arena and team, or OSP entry and ready state. `FL_BOT` and `FL_BOTCLIENT` are counted separately on purpose. Both print under every ruleset that accepts bots, including when there are none. **1.34 adds a third, `botfill`, and 1.36 makes it ONE line for every ruleset** rather than four shapes in four switch arms: the ruleset, the target in force, and where the number came from -- an arena's `playersperteam` or spawn count, ctf's three spawn pools, the map's pool under `dm` and `dmpro`, `2 * team_maxplayers` under `tdm` and `duel`, or the flat `minimumplayers` / `bots_minplayers` where the switch is off. Four shapes is how a play test ends up with three regexes and a gap. The target is COMPUTED every tick rather than stored, so this is the only place it can be read back from |

## Threewave CTF client commands

**51 client commands** in `src/` (`tools/counts.py`), of which fourteen are CTF's. All are gated on the ruleset rather than registered conditionally, so a player who types `team` under `dm` gets the chat fallback rather than silence.

| command | what it does |
|---|---|
| `team` | join red, blue or neither |
| `id` | toggle the player-id display (on by default) |
| `yes` / `no` | vote in an election |
| `ready` / `notready` | match readiness |
| `ghost` | reclaim a match slot with a ghost code after a disconnect |
| `admin` | open the admin menu, subject to `allow_admin` and `admin_password` |
| `stats` | per-player capture and defence statistics |
| `warp` | propose a map from `warp_list` |
| `boot` | admin: remove a player |
| `observer` | leave the game for the chase camera |
| `hookon` / `hookoff` | The offhand hook. Two commands rather than a `+hook` alias, which is what 1999 did and what every 1999 config binds |

Three baseq2 commands change meaning under `ctf` and keep it elsewhere:

| command | `dm` / `sp` | `ctf` |
|---|---|---|
| `say_team` (and `steam`) | `Cmd_Say_f(team)` -- everyone with a matching skin | `CTFSay_Team` -- the sender's team, with flag and tech macros |
| `playerlist` | baseq2's, which reports `resp.spectator` | `CTFPlayerList`, which reports team, ghost code and ready state |
| `inven` | opens the inventory | opens the join menu when you have no team; otherwise the inventory |

`drop tech` is `Cmd_Drop_f`'s one special case: four tech classnames, one key.

## OSP Tourney

The commands of the four match rulesets, and the only surface here gated per RULESET rather than per layer: `dm`, `dmpro`, `tdm` and `duel` are one donor's four structures of play, so a command belonging to one of them is refused by name under the other three. `tools/counts.py --list commands` counts the surface; `tools/playtest.sh` checks the acceptance and the refusal.

These are reachable under all four:

| family | commands |
|---|---|
| match control | `ready` / `notready` / `unready`, `timeout` / `timein`, `matchpause`, `matchinfo` |
| teams | `join` / `joingame` / `jointeam` / `joincode`, `team`, `teamname`, `teamskin` |
| scores and stats | `score`, `stats`, `accuracy`, each with a `last` and an `old` variant |
| observing | `observe` / `observer`, `chase` / `chasecam`, `autocam`, `filter`, `display`, `hud` |
| referee | `ref` / `referee`, then `r_map`, `r_kick`, `r_endmatch`, `r_stopmatch`, `r_fraglimit`, `r_timelimit`, `r_mpause`, `r_allready`, `r_allnotready`, `r_players`, `r_plist`, `r_help`, and the ban set `r_ban` / `r_banaddr` / `r_unban` / `r_unbanaddr` / `r_banlist` / `r_listban` / `r_blist` |
| chat and moderation | `say` / `say_team`, `talk` / `talkto` / `tell`, `mute`, `ignore`, `admin`, `vote`, `motd` |

And these reach exactly one ruleset each:

| ruleset | its own commands |
|---|---|
| `dm` | `highscores`, and the spellings `highscore` / `hiscores` / `hiscore` |
| `duel` | `queue`, `line`, `order` |
| `tdm` | the team verbs, which is most of the gated surface: `captain` / `captains` / `teamcaptain`, `invite` / `pick` / `pickplayer`, `remove` / `removeplayer`, `switchteam` / `switchteams`, `lockteam` / `teamlock` / `lock` with `unlockteam` / `teamunlock` / `unlock`, the team readiness spellings `readyteam` / `teamready` / `noreadyteam` / `notreadyteam` / `teamnotready` / `unreadyteam` / `teamallready`, plus `leader`, `kickplayer` and `steam` |

`say_team` is the one command in both tables. It is not `tdm`-only; it has a `tdm` arm of its own *and* a general one, so what changes under `tdm` is the behaviour rather than whether the command is accepted.

`inven`, `invnext`, `invprev`, `invuse`, `invdrop`, `weapnext`, `weapprev`, `weaplast`, `use` and `drop` gain tourney arms rather than new commands, as they do under `arena`.

## Rocket Arena 2

All gated on `g_ruleset arena`; a command named here does nothing under any other ruleset.

| command | notes |
|---|---|
| `arenaadmin` | the per-arena settings menu |
| `admin` | **third meaning of this name.** CTF's is the election; RA2's is the arena admin login. One ruleset is live at a time, so each keeps the bare name |
| `playerlist` | **third implementation.** baseq2's reports `resp.spectator`, CTF's adds team and ghost code, RA2's is arena-scoped |
| `score` | not a new command: under `arena` it *cycles* arena board -> server-wide -> off, where every other ruleset toggles |
| `menuhelp` | the menu key legend |
| `say_world` | reaches everyone even under teamplay, prefixed `W:`. Its counterpart is plain `say`, which under `arena` stays inside the speaker's ARENA -- so this is not a synonym |
| `grap_on`, `grap_off` | the offhand grapple latch. Mapped onto `ctf_hookstate`, because there is one grapple (section 7 rule 6). The latch is all they are: whether it may fire is `RA_HookThink`'s, gated on `arena.cfg`'s `grapple:` key and on FIGHT_ALIVE. `grap_off` sets TURNOFF rather than clearing, so a hook fired from the WEAPON slot is not released by the offhand key |
| `listkeys`, `listmaps`, `nextmap` | the map loop's reporting |
| `getdebugcode`, `pcount`, `play` | accepted and ignored, as in the donor: clients bind them and expect the server to swallow them |

`invnext`, `invprev`, `inven`, `invuse` and `invdrop` gain arena arms rather than new commands -- the menu owner has first claim on that input.

Two verbs the arena ruleset takes AWAY, and they are a matched pair rather than one rule. `drop` is refused: RA2 empties `Cmd_Drop_f`, an arena hands out a fixed loadout and `SpawnItem` frees every pickup on the map, so a dropped weapon is an item in a ruleset that has none. `kill` is **not** refused, although RA2 dispatches it to nothing -- section 3's "dead functions are live again" names `Cmd_Kill_f` in RA2's retired list, and `ra2observer` asserts a round outlives a fighter's suicide. The difference from the donor is deliberate and is recorded in the same row.


## The bot layer

The bot command set, reachable from the console as `sv <cmd>` and from a client where `serveronlybotcmds` allows it -- which by default it does not, reversing the 1999 default, because the 1999 path exposed `bl_spawn.c`'s 32-byte bot-name copies to clients. Six are console-only regardless, because they dump tables at the server.

| command | who | notes |
|---|---|---|
| `addbot <name> <skin> <charfile> <charname>` | both | queues one bot; never creates one inside `SpawnEntities` or a `ClientConnect`. A duplicate name is refused, except under the four OSP rulesets, which auto-suffix |
| `addrandom [n]` | both | picks from `bots.cfg`, skipping bots already in the game |
| `removebot [name\|all]` | both | `all` is new: `BotDestroyAll()`, which `ShutdownGame` needs too |
| `becomebot` | both | turns a human client into a bot |
| `botpause` | both | toggles `botglobals.nobotai`. The SDK's own was behind `#ifdef BOT_DEBUG`, which is defined in neither donor tree, so this command **could not work**  |
| `menu [rcon_password]` | client | the Gladiator menu tree. Dropped from `osp-tourney`'s `bl_cmd.c` because that mod has its own menus; it comes back with the menu. **Reachable from a client under `ctf` and `arena` only** -- under the OSP four `OSP_ClientCommand` routes `menu` to tourney's own inventory menu first. The argument is the rcon password and is not needed by **the host of a listen server**, who is exempt (they hold the server's console, which is the authority rcon lends out). A server with no `rcon_password` set refuses every client, including on `menu ""`; a second bare `menu` always CLOSES an open menu, because the gate is on opening |
| `bbox` | client | The fourteen-line bounding box, on the entity you are looking at when observing. Behind `cheats`, because it shows through walls |
| `name`, `skin`, `gender` | client | the three the botlib issues on its own behalf |
| `teamhelp [who]`, `teamaccompany [who]` | client | a `say_team` naming the best item within 500 units |
| `checkpoint <name>` | client | a `say_team` naming a position the bots can route to |
| `gps <x> <y> <z>` | client | v0.93's compass. The donor kept `ShowGPSText` and dropped the command that reaches it, leaving the function dead |
| `modelindex`, `soundindex`, `imageindex` | console | the three index tables sized from `game.csr`. Empty rows are skipped: the 1999 table had 256 entries and this one has up to 8192 |
| `indexprobe <model\|sound\|image> <index>` | console | The control. Asks `BotIndexRecord` about one index and **writes nothing**. It exists because the tables are sized from `game.csr`, so the engine cannot issue an index they have no room for and the overflow arm cannot fire on a real server -- which leaves a check that only ever sees silence and cannot tell "it did not overflow" from "the reporting is broken" |
| `inventory` | console | `itemlist[]` by name and index |
| `botlibdump` | console | one block per loaded library, its path, its user count and the clients using it. The bot matrix reads this |
| `clientdump` | console | every client slot: free, human, or bot with its library. The bot matrix reads this too |
| `botinv` | console | The instrument: per bot, its arena and round state, the weapon it has chosen, and the inventory **as the botlib reads it** beside the client's own. The botlib's rows are labelled `brain` in that output; a `brain` ammo row that disagrees with the `game` row under it is an index-space defect. `hold`, `asked` and `dropped` are the fire gate's: whether it is closed right now, how many AI frames asked to shoot while the round was not being fought, and how many of those the gate took away -- a gap between the last two is a shot that left during a countdown |

`invnext`, `invprev` and `invuse` gain a `MENU_BOT` arm rather than new commands, so the help screen's "your inventory key to select" becomes true -- the donor drove the cursor from `forwardmove`/`sidemove` alone, and that is still wired.

## The Gladiator extras' commands, and three diagnostics

**104 client commands** (`tools/counts.py`).  The figure counts every literal the dispatchers compare against, so the two `sv` diagnostics added since it last read 102 -- `arenadump` and `botinv` -- are in it.

| command | who | what |
|---|---|---|
| `observer` | client | Toggles the Gladiator observer under `dm` and `sp`. **Under `ctf` this name is Threewave's** and stays Threewave's -- it drops the flag, drops the tech, resets the score and opens the join menu, none of which the Gladiator toggle knows about |
| `autocam` | client | The camera picks its own subject and cuts between players like a spectator camera |
| `chasecam` | client | Follow one player from behind; jump cycles to the next |
| `cyclecam` | client | Next subject |
| `setcam <name>` | client | Watch a named player |
| `camfixed` | client | Fix or free the chase camera's offset |
| `camname` | client | Show or hide the tracked player's name and score |
| `observerhelp` | client | The seven above, listed |
| `lag <ms>` | client | Simulate this much lag on **your own** input, 0..2000. Says so and does nothing when `g_clientlag` is 0 |
| `lagvariance <ms>` | client | How much the simulated lag wanders each second, 0..2000 |
| `sv openlog <name>` | console | Opens `<gamedir>/<name>`. The name may not contain a path -- the donor took the argument as one, which let `sv openlog ../../anything` write outside the game directory |
| `sv closelog` | console | -- |
| `sv writelog <text>` | console | **The whole line**, not `gi.argv(2)`'s first word, and passed as an argument rather than as a format string -- the donor passed it as the format, so `sv writelog %n` wrote through a stack pointer |
| `sv extras` | console | One line per extra, each a **measurement** rather than a restatement of the cvar: whether the log is open and where, the lag pool's size, whether the three classnames are in the spawn table, how many itemlist rows carry a `weapmodel`, and which observer implementation the running ruleset uses |
| `sv census <classname-or-prefix>` | console | How many entities of that class are spawned, in the world, and taken-and-waiting on their own think, with the frame the count was taken on. A prefix, so `sv census weapon_` answers about every weapon on the map. `sv ruleset` cannot: an item that has been picked up is still `inuse`, still counted and still at the same origin -- what changed is `solid` |
| `sv botperf [reset]` | console | Frames measured, mean and worst cost of the bot section of `G_RunFrame` in microseconds, and the budget. `reset` starts a fresh window so a map load and thirty-two connects are not averaged into the steady state |
