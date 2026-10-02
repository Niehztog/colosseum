# Command inventory

Every client and server command Colosseum accepts, what it does, which ruleset it belongs to, and whether it is server-only.

Generated from `tools/counts.py --list commands` and annotated. **The counts accrue by layer**, in the order the donors were merged; `tools/counts.py` measures the finished tree, so what it prints today is the last of them: **106**.

## Inherited from baseq2

`tools/counts.py` measures **32** client commands in `src/`, counting every literal the dispatcher compares against. The published figure is **27**. The surplus of five is an open question about the definition, not a defect: the likely cause is `Cmd_*` handlers the spec did not treat as client commands. A figure with no script behind it is indicative, so the tool is the authority and 27 is indicative until reconciled; neither figure gates the build, which asks `counts.py` for duplicates rather than for a total. See `SPECS.md` section 3.

```sh
tools/counts.py --list commands
```

## Server commands added by Colosseum

| command | what it does |
|---|---|
| `sv slots` | Reports the active ruleset's resolved stat-slot map (slot, kind, logical id), every row the map declares that resolution *dropped* and why, and the composed statusbar with its byte count. The slot map's counterpart to `sv ruleset`: a slot number that exists only inside the library cannot be checked from outside it |
| `sv ruleset` | Reports the resolved ruleset, content layers, modifiers, every predicate's answer, the legacy `deathmatch`/`coop` values, and an entity census split into live monsters, corpses and gibs. Self-checking: names any live monster present under a ruleset that forbids them. The boot matrix depends on it. **1.23 adds two lines**, `bots` and `botplace`: the bot census with its slot list, and where each bot ended up in the running ruleset's own terms -- CTF team, arena and team, or OSP entry and ready state. `FL_BOT` and `FL_BOTCLIENT` are counted separately on purpose. Both print under every ruleset that accepts bots, including when there are none. **1.34 adds a third, `botfill`, and 1.36 makes it ONE line for every ruleset** rather than four shapes in four switch arms: the ruleset, the target in force, and where the number came from -- an arena's `playersperteam` or spawn count, ctf's three spawn pools, the map's pool under `dm` and `dmpro`, `2 * team_maxplayers` under `tdm` and 2 under `duel`, whose teams are one player each, or the flat `minimumplayers` / `bots_minplayers` where the switch is off. Four shapes is how a play test ends up with three regexes and a gap. The target is COMPUTED every tick rather than stored, so this is the only place it can be read back from. Under `arena` the line ends in `staging` while the target is the one held for people who have not arrived yet -- `arena 1 want=10` is otherwise the same sentence for a busy arena and for an empty map. The row also appends `, N voted out` to whichever arm is printed while a passed `vote rembot` is holding the count down (R-OSP-16): `want=` already has it subtracted and cannot say so, and the flat count printed beside `off` is the cvar's value rather than the number in force |

## Threewave CTF client commands

**51 client commands** in `src/` (`tools/counts.py`), of which fourteen are CTF's. All are gated on the ruleset rather than registered conditionally, so a player who types `team` under `dm` gets the chat fallback rather than silence.

| command | what it does |
|---|---|
| `team` | join red, blue or neither |
| `id` | toggle the player-id display (on by default) |
| `yes` / `no` | vote in an election |
| `ready` / `notready` | match readiness |
| `ghost` | reclaim a match slot with a ghost code after a disconnect |
| `admin` | open the admin menu, subject to `allow_admin` and `admin_password`; without the password, or with a wrong one, it asks for an admin election instead. A wrong password costs the connection 2 seconds, doubling to 30, and an attempt inside that wait is refused unread (R-SEC-12) |
| `stats` | per-player capture and defence statistics |
| `warp` | propose a map from `warp_list` |
| `boot` | admin: remove a player or a bot by slot number (`playerlist` shows them). A bot leaves through `removebot`, because the engine's `kick` has no client in its slot |
| `observer` | leave the game for the chase camera |
| `hookon` / `hookoff` | The offhand hook. Two commands rather than a `+hook` alias, which is what 1999 did and what every 1999 config binds |

Three baseq2 commands change meaning under `ctf`:

| command | `sp` | `ctf` |
|---|---|---|
| `say_team` (and `steam`) | `Cmd_Say_f(team)` -- everyone with a matching skin | `CTFSay_Team` -- the sender's team, with flag and tech macros |
| `playerlist` | baseq2's, which marks a client whose `pers.spectator` is set -- the Reckoning's spelling of the test; the spine and Ground Zero read `resp.spectator` | `CTFPlayerList`, which reports team, ghost code and ready state |
| `inven` | opens the inventory | opens the join menu when you have no team; otherwise the inventory |

baseq2's `playerlist` is also the one `arena` and the OSP four run. `inven` opens tourney's menu under the OSP four and the arena menu under `arena`, and `say_team` has a `tdm` arm of its own (below).

`drop tech` is `Cmd_Drop_f`'s one special case: four tech classnames, one key.

## OSP Tourney

The commands of the four match rulesets, and the only surface here gated per RULESET rather than per layer: `dm`, `dmpro`, `tdm` and `duel` are one donor's four structures of play, so a command belonging to one of them is refused by name under the other three. `tools/counts.py --list commands` counts the surface; `tools/playtest.sh` checks the acceptance and the refusal.

These are reachable under all four:

| family | commands |
|---|---|
| match control | `ready` / `notready` / `unready` / `noready` (refused under `match_strictmode`), `matchinfo` |
| joining | `join` -- a free-for-all join under `dm` and `dmpro`, a team join under `tdm` and `duel` -- and `joincode`. `joingame` is the free-for-all spelling only: under `tdm` and `duel` it is not tourney's and goes to chat, as in the donor |
| scores and stats | `score`, with `oldscore` / `oldscores` / `lastscore` / `lastscores`; `accuracy` / `stats`, with `oldaccuracy` / `oldstats` / `laststats`. A weapon's percentage is held at 100 -- a splash or a rail through two players still counts two hits -- and the raw hits/shots pair is printed beside it |
| observing | `observe` / `observer`, `chase` / `chasecam`, `autocam` |
| the hook and the runes | `hook` / `hookon`, `unhook` / `hookoff` -- tourney's hook, while `hook_enable` is on; `droptech` / `droprune` drops the rune carried |
| display | `hud` / `display`, `id` (the name under the crosshair, which `allow_id` can lock), `motd`, and `menu` / `ctfmenu` / `inven`, tourney's menu |
| referee | `ref` / `referee` / `admin`, the login -- a wrong password costs the connection 2 seconds, doubling to 30 (R-SEC-12) -- then `r_map`, `r_kick`, `r_endmatch`, `r_stopmatch`, `r_fraglimit`, `r_timelimit`, `r_mpause`, `r_allready`, `r_allnotready`, `r_players`, `r_plist`, `r_help`, and the ban set `r_ban` / `r_banaddr` / `r_unban` / `r_unbanaddr` / `r_banlist` / `r_listban` / `r_blist`. `r_allready`, `r_allnotready` and `r_stopmatch` / `r_endmatch` need a match and are not commands under `dm`; `r_banaddr` refuses an empty address and one longer than the ban list's 15 characters |
| chat and moderation | `say` / `say_team`, `talk` / `talkto` / `tell`, `mute` / `ignore` / `muzzle` / `filter` -- four names for one mute -- `vote`, `yes` / `no`, `players` |

`match_strictmode` is off under `dm` whatever it says. Under the three match rulesets it takes the ready verbs -- `tdm`'s team ones included -- from everybody, and `menu`, `ctfmenu`, `inven`, `chase` / `chasecam`, `observe` / `observer` and `autocam` from a player who has entered. `menu`, `ctfmenu` and `inven` are swallowed there rather than handed on, so `menu` cannot reach the bot menu; the rest go to chat, as any word tourney does not take does.

These reach the two TEAM rulesets, `tdm` and `duel`, and not `dm` or `dmpro`, which have no teams (`OSP_IsTeams()`):

| family | commands |
|---|---|
| teams | `jointeam` / `team`, `teamname`, `teamskin` |
| match control | `timeout` / `timein`, `matchpause`, `time` |

And these reach exactly one ruleset each:

| ruleset | its own commands |
|---|---|
| `dm` | `highscores`, and the spellings `highscore` / `hiscores` / `hiscore` |
| `duel` | `queue`, `line`, `order` |
| `tdm` | the team verbs, which is most of the gated surface: `captain` / `captains` / `teamcaptain`, `invite` / `pick` / `pickplayer`, `remove` / `removeplayer`, `switchteam` / `switchteams`, `lockteam` / `teamlock` / `lock` with `unlockteam` / `teamunlock` / `unlock`, the team readiness spellings `readyteam` / `teamready` / `noreadyteam` / `notreadyteam` / `teamnotready` / `unreadyteam` / `teamallready`, plus `leader` and `kickplayer` |

`vote` carries the whole election in its second word: `map`, `config`, `timelimit`, `fraglimit`, `hook`, `runes`, `quad`, `bfg`, `toggles`, `kick`, `specbot`, `addbot` and `rembot`, plus the bare `yes` / `no` / no-argument forms. Each is gated by its own `vote_enable_*` cvar, all three bot rows by `vote_enable_bots`. **`rembot <n>` reaches the bots the fill seated** (R-OSP-16): its cap is the bots on the server rather than the ones a vote added, and the target `botfill` or `bots_minplayers` asks for comes down by what the vote took off, so the next 32-frame fill tick does not put them back. Every value the vote system holds is also reachable from the menus, which propose through this command rather than around it.

`say_team`, with its alias `steam`, is the one command in both tables. It is not `tdm`-only; it has a `tdm` arm of its own *and* a general one -- `steam` reaches the general one through the shared chain, so it works under every ruleset -- and what changes under `tdm` is the behaviour rather than whether the command is accepted.

`inven`, `invnext`, `invprev`, `invuse`, `invdrop`, `weapnext`, `weapprev`, `weaplast`, `use` and `drop` gain tourney arms rather than new commands, as they do under `arena`.

Five **server** commands, for a referee on the console rather than in the game -- the same actions as the client commands with no player behind them:

| command | what it does |
|---|---|
| `sv allready` | readies every player, as `r_allready` does |
| `sv allnotready` | un-readies every player, as `r_allnotready` does |
| `sv mpause` | pauses or resumes the match, as `r_mpause` does |
| `sv stopmatch` | stops the match, as `r_stopmatch` does |
| `sv playerlist [file]` | re-reads tourney's player list, which has no client equivalent: `player_file`, or the file named -- the only way to load another |

The three match verbs need a match, as their `r_` forms do: under `dm` each says there is no match to control and does nothing.

## Rocket Arena 2

All gated on `g_ruleset arena`; a command named here does nothing under any other ruleset.

| command | notes |
|---|---|
| `arenaadmin <code> [arena]` | the per-arena settings menu, for the caller's arena when none is named |
| `admin <code>` | **third meaning of this name.** CTF's is the election, tourney's the referee login; RA2's is the admin menu -- fraglimit, timelimit, map. One ruleset is live at a time, so each keeps the bare name. Both take `admincode`, and `0` there disables both; a wrong code costs the connection 2 seconds, doubling to 30 (R-SEC-12) -- `admin` says so, `arenaadmin` stays silent, as each donor does |
| `playerlist` | **not a third implementation.** `arena` runs baseq2's -- RA2's own is baseq2's, line for line |
| `score` | not a new command: under `arena` it *cycles* arena board -> server-wide -> off, where every other ruleset toggles |
| `menuhelp` | the menu key legend |
| `say_world` | reaches everyone even under teamplay, prefixed `W:`. Its counterpart is plain `say`, which under `arena` stays inside the speaker's ARENA -- so this is not a synonym |
| `grap_on` / `hookon`, `grap_off` / `hookoff` | the offhand grapple latch. Mapped onto `ctf_hookstate`, because there is one grapple (section 7 rule 6). The latch is all they are: whether it may fire is `RA_HookThink`'s, gated on `arena.cfg`'s `grapple:` key, on FIGHT_ALIVE and on not being dead. `hookon` and `hookoff` are the names every other ruleset and the bot brain use. `grap_off` sets TURNOFF, on which the next frame's `RA_HookThink` lets the hook go. The Grapple item is never in the weapon slot here, so this latch is the only hook a fighter has |
| `listkeys`, `listmaps`, `nextmap` | the map loop's reporting |
| `getdebugcode`, `pcount`, `play` | accepted and ignored, as in the donor: clients bind them and expect the server to swallow them |
| `sv arenadump` | console only. A line per arena -- its round state, `pickup`, `ppt`, spawn count, the fill's `want=` and `here=`, the two protect settings -- its loadout, then every team and every connected client: arena, team, fight state, `solid`, `takedamage`, health and origin, with any two clients whose boxes overlap named. What no client can see: a wedged telefrag, a bot on a team in an arena the people are not in |

`invnext`, `invprev`, `inven`, `invuse` and `invdrop` gain arena arms rather than new commands -- the menu owner has first claim on that input.

Two verbs the arena ruleset takes AWAY, and they are a matched pair rather than one rule. `drop` is refused: RA2 empties `Cmd_Drop_f`, an arena hands out a fixed loadout and `SpawnItem` frees every pickup on the map, so a dropped weapon is an item in a ruleset that has none. `kill` is **not** refused, although RA2 dispatches it to nothing -- section 3's "dead functions are live again" names `Cmd_Kill_f` in RA2's retired list, and `ra2observer` asserts a round outlives a fighter's suicide. The difference from the donor is deliberate and is recorded in the same row.


## Ground Zero's client commands

Two commands Ground Zero added to baseq2's dispatcher, reachable under every ruleset; `disguise` asks for `cheats`:

| command | what it does |
|---|---|
| `entcount` | prints the number of entities in use, a Ground Zero debugging aid |
| `disguise` | sets `FL_DISGUISED` on the player, so a monster running Ground Zero's AI does not pick them as a target until they fire a weapon (which names them `level.disguise_violator`) or a monster that has found them clears it. **A cheat**, asked as `god` and `notarget` ask: refused in deathmatch and co-op unless `cheats 1`, because it also has the client draw `players/<model>/disguise.pcx` instead of the skin -- under `ctf`, `tdm` or `arena`, no team's colours. A respawn clears it, and a new arrival never inherits it; `trigger_disguise` sets it regardless |

## The bot layer

The bot command set, reachable from the console as `sv <cmd>`. The five marked `both` are reachable from a client where `serveronlybotcmds` allows it -- which by default it does not, reversing the 1999 default, because the 1999 path exposed `bl_spawn.c`'s 32-byte bot-name copies to clients. The `client` rows are a client's whatever it says (R-BOT-24): the bot menu has its own gate, `bbox` asks for `cheats`, and the rest are what a bot types on its own behalf. Ten are console-only regardless, because they report at the server: `modelindex`, `soundindex`, `imageindex`, `indexprobe`, `inventory`, `botperf`, `botlibdump`, `botlibs`, `clientdump` and `botinv`.

| command | who | notes |
|---|---|---|
| `addbot <name> [skill]`, `addbot <name> <skin> <charfile> <charname> [skill]` | both | queues one bot; never creates one inside `SpawnEntities` or a `ClientConnect`. **The short form is Quake III's**: a bot out of the offered botlibs' bot lists by name, without regard to case, searched in `botlibs`' order, and a skill 1..5 (R-BOT-34). The long form names a character, and the bot list that holds that character file and name says which botlib drives the bot; a character no offered list holds goes to the first botlib offered. A skill is the bot's -- `botskill` when it is left out -- and only a botlib with skills reads it: one given for a Gladiator bot is said to be ignored, because Gladiator's characters carry theirs. A duplicate name is refused, except under the four OSP rulesets, which auto-suffix; a bot already queued holds its name as surely as one in the game. A name, skin, character file or character name the userinfo cannot hold -- 64 characters or more, or a `\`, `"` or `;` -- is refused with a message, and a bot that cannot be seated says why: no free slot, the connect's refusal, or the botlib refusing the character |
| `addrandom [n]` | both | picks from `bots.cfg`, skipping bots already in the game or queued for it. Once every one is in, the four OSP rulesets pick again at random and seat a suffixed copy; the rest refuse with `Every configured bot is already in the game.`, which is also where the bot fill stops. `n` is held to the seats a bot could still be given -- the free client slots, less the bots already queued -- and the command says so when it cuts |
| `removebot [name\|all]` | both | `all` is new: `BotDestroyAll()`, which `ShutdownGame` needs too |
| `becomebot` | both | turns a human client into a bot |
| `botpause` | both | toggles `botglobals.nobotai`. The SDK's own was behind `#ifdef BOT_DEBUG`, which is defined in neither donor tree, so this command **could not work**  |
| `menu [rcon_password]` | client | the Gladiator menu tree. Dropped from `osp-tourney`'s `bl_cmd.c` because that mod has its own menus; it comes back with the menu. **Reachable from a client under `ctf` and `arena` only** -- under the OSP four `OSP_ClientCommand` routes `menu` to tourney's own inventory menu first. The argument is the rcon password and is not needed by **the host of a listen server**, who is exempt (they hold the server's console, which is the authority rcon lends out). A server with no `rcon_password` set refuses every client, including on `menu ""`; `none` is a password like any other, and a wrong one costs the connection 2 seconds, doubling to 30 (R-SEC-12) -- `menu` with no password is not a guess and costs nothing. A second bare `menu` always CLOSES an open menu, because the gate is on opening |
| `bbox` | client | The fourteen-line bounding box, on the entity you are looking at when observing. Behind `cheats`, because it shows through walls |
| `name`, `skin`, `gender` | client | the three the botlib issues on its own behalf |
| `teamhelp [who]`, `teamaccompany [who]` | client | a `say_team` naming the best item within 500 units |
| `checkpoint <name>` | client | a `say_team` naming a position the bots can route to |
| `gps <x> <y> <z>` | client | v0.93's compass. The donor kept `ShowGPSText` and dropped the command that reaches it, leaving the function dead |
| `modelindex`, `soundindex`, `imageindex` | console | the three index tables sized from `game.csr`. Empty rows are skipped: the 1999 table had 256 entries and this one has up to 8192 |
| `indexprobe <model\|sound\|image> <index>` | console | The control. Asks `BotIndexRecord` about one index and **writes nothing**. It exists because the tables are sized from `game.csr`, so the engine cannot issue an index they have no room for and the overflow arm cannot fire on a real server -- which leaves a check that only ever sees silence and cannot tell "it did not overflow" from "the reporting is broken" |
| `inventory` | console | `itemlist[]` by name and index |
| `botlibdump` | console | one block per loaded library, its path, its user count, which botlib it is with its `BotVersion`, and the clients using it -- with each bot's skill where its botlib has them. The bot matrix reads this |
| `botlibs` | console | **New here.** Per botlib: whether `botlibs` offers it and where in the order, its library file and whether it is loaded, its data directory and bot list, the AAS file it would find for this map with that file's version, and whether it can play the map -- the one view of why a botlib's bots do or do not appear (R-BOT-32) |
| `clientdump` | console | every client slot: free, human, or bot with its library. The bot matrix reads this too |
| `botinv` | console | The instrument: per bot, its arena and round state, the weapon it has chosen, and the inventory **as the botlib reads it** beside the client's own. The botlib's rows are labelled `brain` in that output; a `brain` ammo row that disagrees with the `game` row under it is an index-space defect. `hold`, `asked` and `dropped` are the fire gate's: whether it is closed right now, how many AI frames asked to shoot while the round was not being fought, and how many of those the gate took away -- a gap between the last two is a shot that left during a countdown |

`invnext`, `invprev` and `invuse` gain a `MENU_BOT` arm rather than new commands, so the help screen's "your inventory key to select" becomes true -- the donor drove the cursor from `forwardmove`/`sidemove` alone, and that is still wired.

## The Gladiator extras' commands, and three diagnostics

**106 client commands** (`tools/counts.py`).  The figure counts every literal the dispatchers compare against, so the `sv` diagnostics -- `arenadump`, `botinv` and `botstats` among them -- are in it.

| command | who | what |
|---|---|---|
| `observer` | client | Toggles the Gladiator observer under `sp`. **Under `ctf` this name is Threewave's** and stays Threewave's -- it drops the flag, drops the tech, resets the score and opens the join menu, none of which the Gladiator toggle knows about -- and under the OSP four it is tourney's (`observe`). The seven below reach `ctf` and `sp`; under `ctf`, entering a camera (`autocam`, `chasecam`, `cyclecam`, `setcam`) lets go of the flag, the tech and the grapple first, as Threewave's `observer` does |
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
| `sv botstats [reset\|off]` | console | What each client did in a measurement window (R-BOT-36): per client its botlib, skill, team, the ruleset's score, kills, deaths, suicides, team kills, damage given, taken, to itself and to team-mates, hits, and pickups by kind. `reset` opens a fresh window and, while it is open, prints a `botstats kill` line per death; `off` closes it. Closed by default, and closed it counts and prints nothing. `tools/botduel.py` reads it |
