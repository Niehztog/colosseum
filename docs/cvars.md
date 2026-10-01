# Cvar inventory

The merged, deduplicated cvar inventory: every cvar Colosseum registers, its default, its flags, the donor that introduced it, and -- where section 7 rule 4 applied -- which ruleset owns the bare name and what the prefixed alias is. **Three sets are counted here and listed by the tool rather than transcribed**, because none of them carries a decision: baseq2's own 37; the loadout family, which is a matrix (`start_`/`max_`/`pack_`/`warmup_` x the ammo and armour rows) rather than a list; and the engine's own cvars the library only re-obtains to read (`basedir`, `gamedir`, `homedir`, `libdir`, `m_pitch`, `rcon_password`), which are registered by whoever asks first and are owned by nobody here -- `g_fs.c` holds the defaults for `basedir`, `gamedir` and `homedir` so that the order cannot decide them, and `libdir` has one reader, the botlib search. Everything else has a row.

Generated from `tools/counts.py --list cvars` and annotated. **The counts accrue by layer**, in the order the donors were merged; `tools/counts.py` measures the finished tree, so what it prints today is the last of them: **328**. Regenerate rather than hand-edit the counts -- and the total is enforced: `counts.py --docs` compares the figure above, and `docs/commands.md`'s, with what the tree measures, and runs in `make check` with `--duplicates` and the extraction controls, so a total the tree has moved away from fails the build rather than sitting in a document. It said that before it was true -- `--duplicates` checks duplicates, not this page -- and this page said 326 while the tree had 327.

## Inherited from baseq2

**37 cvars**, which is exactly the published figure -- the first of the spec's acceptance counts to be confirmed by a tool rather than asserted.

```sh
tools/counts.py --list cvars
```

## Threewave CTF

**69 cvars** total (`tools/counts.py`), of which fourteen are CTF's. Registered in `CTFInit()`, which `InitGame` calls unconditionally -- a donor that fails to call it at all gets a crash rather than a missing feature.

| cvar | default | what it does |
|---|---|---|
| `ctf` | `0`, `CVAR_SERVERINFO` | **not a behaviour switch.** A legacy alias that can *select* the ruleset, and once resolution has decided, the dispatch forces it to agree so a Threewave-aware client reads the truth out of serverinfo. Registered by `g_ruleset.c`, not by CTF |
| `capturelimit` | `0`, `CVAR_SERVERINFO` under `ctf` | captures that end a public game (a competition match is timed by `matchtime` instead). Independent of `fraglimit`, and structural rather than patched: it is read before any fraglimit test can shadow it -- though a `fraglimit` still ends a CTF level too, on a score that counts captures, which is why `configs/ctf.cfg` sets it to 0 |
| `instantweap` | `0`, `CVAR_SERVERINFO` under `ctf` | no raise or lower animation on a weapon switch, under `ctf` only |
| `ctf_hook` | `1` | the offhand hook, driven by `hookon`/`hookoff` |
| `laserhook` | `0` | the beam rendering rather than the cable. Threewave shipped this as a `#if`; it is a cvar because the botlib has to be told which is in play (the libvar push is the bot layer's) |
| `ctf_forcejoin` | `""` | force joiners onto a named team |
| `competition` | `0`, `CVAR_SERVERINFO` under `ctf` | 0 public, 1 admin-managed, 2+ match mode. `2` starts the level in `MATCH_SETUP` -- **gated on the ruleset**, or a `dm` server with it left in its config would freeze all damage |
| `matchlock` | `1`, `CVAR_SERVERINFO` under `ctf` | no joining a match once it has started |
| `electpercentage` | `66` | share of votes an election needs |
| `matchtime` | `20`, `CVAR_SERVERINFO` under `ctf` | match length, minutes |
| `matchsetuptime` | `10` | setup window, minutes |
| `matchstarttime` | `20` | countdown, seconds |
| `admin_password` | `""` | the `admin` command's password. A wrong one costs the connection 2 seconds, doubling to 30 (R-SEC-12) |
| `allow_admin` | `1` | whether `admin` works at all |
| `warp_list` | `q2ctf1 ... q2ctf5` | maps the `warp` vote may choose |
| `warn_unbalanced` | `1` | the statusbar's "too many players" line |

Two names that do **not** appear and are worth saying so about: `maxspectators` survives from baseq2 although Threewave deletes it -- registered, and read only by baseq2's spectator admission, which no ruleset here reaches: `ctf`, `arena` and the OSP four have observers of their own, and `sp` is not deathmatch; and `ch` is accepted and ignored (N7).

Five of these -- `capturelimit`, `competition`, `matchlock`, `matchtime` and `instantweap` -- are registered under every ruleset, because CTF's flag and tech paths are reached from shared code, and carry `CVAR_SERVERINFO` only under `ctf`: a `dm` server has no business advertising a capture limit.

Every name is q2pro's own, with q2pro's default and flags, except where section 7 rule 4 needed a prefix to settle a collision: `ctf_*` for Threewave's and `bots_*` for the bot layer's. The collisions that rule settles are the next section.

## The mission packs

*The Reckoning adds none.* Measured against `q2pro/src/xatrix`: every `gi.cvar` in it names a cvar baseq2 already registers, so Xatrix's content arrives with no switch of its own -- `max_magslug` and `max_trap` are the loadout family's (below), and the layer itself is the `xatrix` cvar in the next section.

**Ground Zero adds nine.** Six are registered unconditionally in `g_main.c`, so that a config naming one is not rejected under a ruleset that ignores it. The other three -- `goallimit`, `dball_team1_skin` and `dball_team2_skin` -- are DeathBall's and are registered only by `DBall_GameInit`, which nothing installs: they are never registered, and a config that sets one creates a cvar nothing reads.

| cvar | default | meaning |
|---|---|---|
| `gamerules` | `0`, `CVAR_LATCH` | Ground Zero's alternate DM rules: `2` Tag, anything else off -- `3`, Ground Zero's DeathBall, is not installed and `InitGameRules` resets it to 0. Narrowed on purpose -- `G_UsesRogueGameRules()` is true under `ctf` only: `OSP_CheckRules()` reimplements the fraglimit and timelimit tests and never reaches the `DMGame` vtable, and arena purges the Tag token and scores by damage (`SPECS.md`, R-MODE-3). Set under any other ruleset, it is ignored and the log says so |
| `goallimit` | `0` | DeathBall goals that end the match; DeathBall's, so never registered |
| `dball_team1_skin`, `dball_team2_skin` | `male/ctf_r`, `male/ctf_b` | DeathBall team skins; DeathBall's, so never registered |
| `huntercam` | `1`, `CVAR_SERVERINFO \| CVAR_LATCH` | the hunter sphere's Sam Raimi cam. `0` leaves the view on the player |
| `randomrespawn` | `0` | an item respawns as a random other item of its own class, at its own spot -- never as one a map could not place, one a dmflag forbids, or, under the OSP four, one a referee has switched off |
| `strong_mines` | `0` | prox mines get the stronger variant's behaviour |
| `sv_stopspeed` | `100` | the speed below which the game's own friction stops a body -- monsters and items, not players, whose movement is the engine's. id's `#define` in `g_phys.c`, which Ground Zero made a cvar so that DeathBall could zero it |
| `g_showlogic` | `0` | Ground Zero's monster-AI tracing, read by the merged monster files |

## Added by Colosseum

| cvar | flags | meaning |
|---|---|---|
| `g_ruleset` | `CVAR_LATCH \| CVAR_SERVERINFO` | the primary ruleset: `dm` \| `dmpro` \| `tdm` \| `duel` \| `ctf` \| `arena` \| `sp`. Empty means "infer": legacy aliases first, then `coop`, then `dm`. An invalid value warns and falls back to `dm`. **Seven**: the first four are OSP's four modes of play, which `match_mode` selected inside a `tourney` ruleset until the flattening. `tourney` is not an alias and not a special case -- it is simply not a ruleset name, and takes the unknown-value path with the same message any other unknown value gets |
| `xatrix`, `rogue` | `CVAR_LATCH` | independent content layers, valid with every ruleset, both may be on |
| `teamplay`, `hook`, `runes` | `CVAR_LATCH` | modifiers, refused with a message by a ruleset that does not accept them. `teamplay` is refused by every ruleset: `tdm` and `duel` **are** team play, so the modifier that also reached it is the second selector the flattening removes, and `ctf` and `arena` decide their own teams. `hook` is accepted by the OSP four only, and `runes` under `ctf` and the OSP four (see the end of the bot-layer section). `hook 1` is a one-way baseline request to `hook_enable`; `ctf` and `arena` refuse it, naming their own switch, because each has one and nothing there read the request. `sv ruleset`'s `hook=` reports each ruleset's own switch -- `hook_enable` under the OSP four, `ctf_hook` under `ctf`, `arena.cfg`'s `grapple:` under `arena` |

Read but not claimed: `rocketarena`, the other legacy ruleset alias beside `ctf` (whose row is above), is read with `gi.cvar("rocketarena", "0", 0)`, so a config that sets it still selects `arena` and Colosseum claims none of its flags. `ch` is not read at all: a config may set it and nothing reads it, and the bot layer hands the brain a constant `0` for it (N7).

`deathmatch` and `coop` remain the engine's. Resolution runs in both directions: a legacy value can select the ruleset, and once selected the ruleset is authoritative and `deathmatch` is forced to agree.

## Known collisions, from the measured donors

Every donor named here is merged, so these are live collisions rather than predicted ones, and section 7 rule 4 is what settles each.

| name | claimants | resolution |
|---|---|---|
| `statsname` | RA2 (`ra2stats.jsonl`), tourney (`osptourney.jsonl`) | **registered once, in `G_InitRuleset()`**, with the default the resolved ruleset's -- see the row below. Section 7 rule 4's prefixed pair was predicted here and not built: the two donors mean the same thing by the name and differ only in the default (R-COMPAT-6) |
| `statsfile` | RA2 and tourney, **same default** | not a collision -- an idiomatic re-obtain, not a duplicate |
| `minimumplayers` vs `bots_minplayers` | Gladiator vs tourney | both registered, authority is per ruleset. The OSP names are authoritative under **four** rulesets -- `dm`, `dmpro`, `tdm`, `duel` -- and Gladiator's under `ctf` and `arena`. A hand-written `dm` config setting `minimumplayers` stopped being read; `configs/dm.cfg` carries the new name |
| `botfile` vs `bots_botfile` | Gladiator vs tourney | ditto |

## Rocket Arena 2

Registered unconditionally so that a config naming one is not rejected, but only `arena` reads them.

| cvar | default | meaning |
|---|---|---|
| `netlog` | `""`, `CVAR_SERVERINFO` under `arena` only (RA2's line) | **registered and read by nothing.** It named the host RA2's event log forwarded to; R-SEC-7 refused the forwarding, so the cvar is kept only so a legacy config still resolves, and under `arena` `InitGame` prints that the feature is gone when it is set. There is no socket anywhere in this tree, and `tools/noexec.py` fails the build on one. An empty value is not advertised (Q2PRO omits empty serverinfo values). Q2PRO keeps a flag the game ORs into a cvar and the game API cannot clear one, so after an arena session in one process `netlog` stays flagged |
| `arenacfg` | `arena.cfg` | the per-arena settings file, read per level from `<homedir-or-basedir>/<gamedir>/` |
| `admincode` | `0` | the numeric code `admin <code>` takes to open RA2's admin menu (fraglimit, timelimit, map), and `arenaadmin <code> [arena]` an arena's settings menu. `0` disables both outright. A wrong code costs the connection the backoff every login shares, 2 seconds doubling to 30 (R-SEC-12) |
| `logname` | `stdlog.log` | the StdLog file name, under the same root as `arenacfg` |
| `logfile` | `0` | the engine's own console-logging cvar, re-obtained: RA2 gates its stdlog on it rather than adding a second switch, so the StdLog runs while it is 2 -- which to Q2PRO also means "append the console log". **No** serverinfo flag: RA2 flagged it, but it is the engine's cvar, and a flag ORed into an engine cvar outlives the library that set it, so it would advertise the console log's mode under every ruleset run after an arena session. `configs/arena.cfg` does not set it, because a config's `set` would override the command line's `+set` and outlive the ruleset |
| `hostname`, `port` | engine's | re-obtained for the round log's header |
| `public` | `1` | re-obtained because the round log's header counts it. `netlog` forwards nothing, so a private server has nothing to clear |
| `ra_allowvotingbots` | `0` | Colosseum's own, and a **floor under `arena.cfg`, not a second switch**: non-zero offers the `Allow Bots` row in every arena's settings menu whatever the file's `allowvotingbots` key says, `0` leaves the file deciding. Read in `set_config()` after all three of the file's layers -- global, per-map, per-arena -- so no block can withdraw it, which is the point: a server that runs `botfill` wants "may the people here vote the bots away" answered by the server and not by the map rotation it happens to be running (R-RA-13). It cannot turn the vote off, only on |

`statsfile` / `statsname` are the shared pair and arrive with tourney, where the collision with tourney's is resolved. `configs/arena.cfg` sets `statsname "ra2stats.jsonl"` as the OSP configs set `osptourney.jsonl`: resolution's default applies only where the cvar is created, and a config's `set` outlives a ruleset change in the process.


## OSP Tourney DM

**227 names**, of which **218 are tourney's own** and nine it shares with the rest of the tree. That is more than two thirds of the tree's 328, and it is why this section is grouped by what a setting *does* rather than listed flat: an operator arrives here with a question about warmup or about voting, never about the alphabet.

**The count is `counts.py`'s, and the script had to be fixed to produce it.** It counted the unique first argument of a literal `gi.cvar("...")` and nothing else, which meant it could not see two shapes this layer uses. `stdlog.c` registers `sl_log_logbots` through `import->cvar(...)`, a `game_import_t` reached as a parameter rather than through the global -- the same engine function, invisible to a `gi.`-anchored pattern. And the twenty-six `allow_*` names moved out of literal position into `osp_allow_items[]`, reached as `gi.cvar(osp_allow_items[i].cvar, ...)`. **The published figure therefore FELL by sixteen at the moment the tree gained ten cvars** -- a measurement that moves the wrong way, which is worse than one that is merely naive. Both shapes are resolved now (a table column by finding the member's position in its struct), nine controls cover the extraction, and `--duplicates` runs in `make check`, so this document's numbers are re-derived on every build rather than asserted once. **Four sites remain unresolvable** and are printed rather than dropped: two `BotSetVarIfSet` parameters, `g_ruleset.c`'s alias table, and `osp_allowed()`'s own parameter. The count is a floor and says so.

**Where the descriptions come from.** The donor ships its own documentation -- `vendor/harness/tools-inputs/osp/docs/server-settings.txt`, 126 of these 227 names -- and that is the source used here, not the cvar's spelling. Writing this section from the names first produced **eight wrong descriptions**, several of them the kind an operator would act on: `client_protect` is seconds of spawn shell and not a name password, `demo_player` is server-FORCED recording and not permission to record, `match_prestartpercent` makes the weapons disappear rather than starting a countdown, `fast_respawn` scales ITEM respawn and not player respawn, and `vote_carryover` carries voted VALUES across a map rather than an unresolved vote. The remaining 101 were read from the code. The regeneration records the correction pass, because the failure mode is worth remembering: a plausible description is not a checkable one.

**Translating a 1999 config.** The donor selected its four modes with `match_mode`, which was flattened into `g_ruleset`. The values map exactly:

| donor | `g_ruleset` | |
|---|---|---|
| `match_mode 0` | `dm` | RegularDM -- join the fray and play |
| `match_mode 1` | `dmpro` | Qualifier DM -- everyone types `ready` first |
| `match_mode 2` | `tdm` | team play |
| `match_mode 3` | `duel` | 1v1 |

Everything here is registered under **all seven rulesets** so a config naming one is not rejected; only `dm`, `dmpro`, `tdm` and `duel` read them. Those four are OSP's own modes of play, which `match_mode` selected inside a single `tourney` ruleset until it was flattened -- so where the donor's documentation says "match_mode 3", this tree says `g_ruleset tdm`.

### Match control, warmup and intermission

`match_type` and `match_info` are the two the server browser sees. **`RULESET_DM` is OSP's RegularDM and not baseq2 deathmatch** (the spec), which is why a default of `RegularDM` appears under a ruleset named `dm`.

| cvar | default | what it does |
|---|---|---|
| `match_type` | `RegularDM`, `CVAR_SERVERINFO` | the mode's name for a browser. Set from the ruleset, not by hand |
| `match_info` | `None`, `CVAR_SERVERINFO` | free-text match description |
| `match_endinfo` | `OSP Tourney DM v(2.75)` | the line the intermission prints |
| `match_countdown` | `30` | seconds from both teams ready to live |
| `match_countinfo` | `1` | display the match info during the countdown |
| `match_startsound` | `1` | play the start sound |
| `match_readypercent` | `100` | share of players who must be ready |
| `match_prestartpercent` | `50` | share of players ready at which the WEAPONS DISAPPEAR -- the donor's hint to everyone else to ready up, not a countdown threshold |
| `match_latejoin` | `2` | joining after a synced match has begun: `0` not at all, `1` connect as an observer only, `2` may enter only if invited or a team is unlocked (teams auto-lock at the start), `3` free entry |
| `match_pausetime` | `60.0` | seconds a timeout lasts |
| `match_timeouts` | `3` | timeouts per team |
| `match_strictmode` | `0` | `0` the traditional `ready` style; `1` clients cannot change their ready status at all and a referee or admin must ready them, and a player who has entered cannot open the menu or go to a camera. Off under `dm` whatever it says (`OSP_StrictMode()`), and no ruleset writes it |
| `warmup_health` | `150` | health during warmup, where it is deliberately not `max_health` |
| `warmup_armor` | `200` | armour during warmup |
| `time_remaining` | `ServerInit`, `CVAR_SERVERINFO` | the clock, published for a browser. A **string**, because "Warmup" and "Overtime" are not times |
| `map_halt` | `0` | halt at the end of the level. It works by ZEROING both `nextlevel_*` timers, which is the only thing in the tree that reads either of them -- a write that outlives it: clearing `map_halt` restores neither, which is why the four OSP configs set both |

### Team play

| cvar | default | what it does |
|---|---|---|
| `team_maxplayers` | `4` | The size of a `tdm` team, and clamped at map load so `2 * team_maxplayers <= maxclients`. **`duel` does not read it**: 1v1 is two teams of one whatever this says, and `duel` leaves the cvar as it found it, so a `tdm` map after a duel gets the operator's size back rather than a 1 |
| `team_a_name`, `team_b_name` | `Hometeam`, `Visitors` | team names |
| `team_a_skin`, `team_b_skin` | `female/athena`, `male/sniper` | forced skins |
| `team_a_hookcolor`, `team_b_hookcolor` | `0xf2f2f2f2`, `0xd1d1d1d1` | per-team hook colour |
| `team_lockskin` | `0` | `0` clients may change team skins, `1` they may not. Teamplay and 1v1 |
| `team_idteam` | `1` | show a team-mate's name on the crosshair |
| `team_hurtteam` | `1` | friendly fire. Seeds `osp_teams[n]` when the game starts and again when a configuration change -- a passed `vote config`, the empty server's return to its default -- execs a new config (R-OSP-20), not at an ordinary map change; a referee and the team menus write the per-team switch directly. **`configs/tdm.cfg` ships `0`** |
| `team_hurtself` | `1` | self damage in team play |
| `ffa_hurtself` | `1` | self damage outside it |
| `team_overtime_mode` | `1` | `0` none, and a match may end tied; `1` sudden death, decided by the first kill or suicide; `2` timed rounds, for as long as the match stays tied; `3` timed rounds, then sudden death once `team_overtime_count` of them have been played |
| `team_overtime_time` | `1` | minutes per timed overtime round, minimum 1. Read under modes `2` and `3` |
| `team_overtime_count` | `1` | under mode `3`, how many timed rounds are played before sudden death decides it; at least 1 |
| `team_recovertime` | `0.0` | seconds to hold a slot open for a disconnected player. **`0` disables `team_duelrecover` outright**, whatever that is set to |
| `team_duelrecover` | `0` | what happens when a team drops to zero players: `0` the match terminates, `1` it waits for the player to come back. Needs `client_recover` AND a non-zero `team_recovertime` |
| `team_nextuptime` | `45` | seconds the next pair has to get ready |
| `Score_A`, `Score_B` | **per ruleset** | the browser's two score cells. `Disabled`/`CVAR_SERVERINFO` under `tdm`, otherwise `""` with no flags and explicitly cleared -- only team play publishes numbers |

### Voting

| cvar | default | what it does |
|---|---|---|
| `vote_enable` | `1` | the whole mechanism |
| `vote_threshold` | `51` | percent to pass |
| `vote_time` | `45` | seconds a vote is open |
| `vote_carryover` | `1` | carry voted VALUES across a map load rather than reverting to the server's defaults |
| `vote_countspectators` | `""` | whether observers vote. Empty is the ruleset's own answer -- yes under `dm` and `dmpro`, no under `tdm` and `duel`, where an audience could out-vote the players -- and `0` or `1` decides it for every ruleset |
| `vote_enable_map`, `_frag`, `_time`, `_kick`, `_toggles` | `1` | the votes that are on by default |
| `vote_enable_config`, `_bots`, `_runes`, `_hook` | `0`, `0`, `0`, `1` | and the ones that are not. `hook` is on because the hook itself defaults off -- the vote is how it gets turned on. `_bots` gates `addbot`, `specbot` and `rembot` together; with it on, `vote rembot <n>` reaches the bots `botfill` or `bots_minplayers` seated and the fill's target comes down by what the vote took off, so they stay gone for the rest of the level (R-OSP-16) |
| `vote_bots_max` | `8` | ceiling a bot vote may **add**. Removals are capped by the bots that are on the server instead, which is the count the menu's "Total active bots" row shows |
| `vote_config_list` | `serverconfigs.txt` | configs a config vote may choose |
| `vote_config_default` | `0` | load one at boot |
| `vote_config_defaultname` | `default` | which. **Our default differs from the donor's**, which documents `none`; a name that does not exist disables config cycling either way |
| `__current_config` | `default` | **not for an operator.** The name of the config in force, kept so a vote can report it; the leading underscores are the donor's own convention for "internal" |
| `menu_maxfrag`, `menu_fragstep` | `100`, `5` | the fraglimit vote menu's ceiling and step |
| `menu_maxtime`, `menu_timestep` | `120`, `5` | the timelimit vote menu's |

### The item switches -- `allow_*`

**Twenty-six, of which ten are the content layers'** and are the reason this family is one table in the code (`osp_allow_items[]`: cvar, classname, banner tag, log name) rather than three lists that had drifted apart. `0` inhibits the item where a map placed it; the scoreboard banner names what is off, and the stats log records it. Two rows are inhibited and deliberately named by neither, which the table says with `NULL` rather than by absence.

Ammunition follows its weapon by a **derived rule** rather than a cvar of its own: shells go when both shotguns are gone, bullets when both bullet weapons are, and rockets, slugs, mag slugs, flechettes, prox and disruptor ammo each when their one weapon is. `allow_ammo_cells` is the exception and has a switch, because three weapons share cells and the donor chose not to derive it.

| group | cvars |
|---|---|
| baseq2 weapons | `allow_shotgun` `allow_supershotgun` `allow_machinegun` `allow_chaingun` `allow_grenadelauncher` `allow_rocketlauncher` `allow_hyperblaster` `allow_railgun` `allow_bfg` |
| baseq2 ammo and items | `allow_ammo_grenades` `allow_ammo_cells` `allow_item_powerscreen` `allow_item_powershield` `allow_item_quad` `allow_item_invul` `allow_item_pack` |
| The Reckoning | `allow_ionripper` `allow_phalanx` `allow_ammo_trap` |
| Ground Zero | `allow_etfrifle` `allow_proxlauncher` `allow_plasmabeam` `allow_chainfist` `allow_disruptor` `allow_ammo_tesla` `allow_item_nuke` |

All default `1`. The Disruptor has a row although it is `IT_NOT_GIVEABLE`: `give` cannot hand it out, but a map may still *place* one, and inhibiting a placed entity is exactly what this table does.

`allow_id` (`1`) is not one of these. It governs **ID tagging** -- a team-mate's name under the crosshair -- and it is four-valued rather than boolean: `2` is server-locked off and `3` server-locked on, both of which force the client's own flag and refuse to let it toggle; anything else lets the client decide.

### The loadout -- `start_*`, `max_*`, `pack_*`

Three parallel families over one ammo list: what a player spawns with, the carrying capacity, and what a Bandolier or Pack raises it to. **Fifteen of the forty are the layers'** -- the two content layers' five ammo types (`magslug`, `flechettes`, `prox`, `tesla`, `trap`), which had no cvars at all until then, so under `xatrix 1` or `rogue 1` a referee could set every baseq2 ammo and not the Phalanx's.

| | `start_` | `max_` | `pack_` | `warmup_` |
|---|---|---|---|---|
| `health` | `100` | `100` | `100` | `150` |
| `armor` | `0` | `200` | `250` | `200` |
| `armortype` | `0` | -- | -- | -- |
| `shells` | `0` | `100` | `200` | -- |
| `bullets` | `0` | `200` | `300` | -- |
| `cells` | `0` | `200` | `300` | -- |
| `rockets` | `0` | `50` | `100` | -- |
| `slugs` | `0` | `50` | `100` | -- |
| `grenades` | `0` | `50` | `100` | -- |
| `magslug` | `0` | `50` | `100` | -- |
| `flechettes` | `0` | `200` | `200` | -- |
| `prox` | `0` | `50` | `50` | -- |
| `tesla` | `0` | `50` | `50` | -- |
| `trap` | `0` | `5` | `5` | -- |

`weapon_initial` (`0`) and `weapon_have` (`0`) are **bitmasks over nineteen weapon slots** -- which weapon a player spawns holding, and which they spawn owning. They stopped being written as literal indices: the slots are resolved by NAME through `OSP_resolveLoadoutSlots()`, lazily, because `OSP_gameInit` runs before `InitItems()`. The nineteen include the layers'.

Armour behaviour is four numbers per class rather than a flag:

| cvar | default | meaning |
|---|---|---|
| `armor_body` | `100 200 0.80 0.60` | count, max, normal absorption, energy absorption |
| `armor_combat` | `50 100 0.60 0.30` | ditto |
| `armor_jacket` | `25 50 0.30 0.00` | ditto |
| `armor_shard` | `2` | points a shard adds |

### Runes

OSP's runes are the CTF techs by another name, and `runes` is the modifier that asks for them; these tune them. `runes_enable` is a **bitmask** of which runes may appear, clamped to `0x1f`.

| cvar | default | what it does |
|---|---|---|
| `runes_enable` | `0` | a bitwise SUM: `1` resist, `2` strength, `4` haste, `8` regeneration, `16` vampire, so `31` is all five. Clamped to `0x1f`; a queued OSP configuration transition refreshes the cached `rune_stat` from this effective value before it advertises or seeds the new map's runes |
| `runes_min`, `runes_max` | `3`, `12` | how many exist. `max` is forced up to `min` if a config inverts them |
| `runes_perplayer` | `0.6` | ratio of runes to active players |
| `runes_model` | `models/items/c_head/tris.md2` | the model every rune uses -- which is John Carmack's head, and the donor says so |
| `runes_flash` | `1` | flash the client in their rune's colour when it is USED, not when picked up |
| `runes_resist`, `runes_strength` | `2.0`, `2.0` | damage divisor and multiplier |
| `runes_vampire`, `runes_vampire_max` | `0.5`, `200` | share of damage returned as health, and its ceiling |
| `runes_regen_hmax`, `runes_regen_amax` | `200`, `100` | regeneration ceilings |

### The hook

Registered by tourney and **distinct from CTF's grapple** -- one concept, two implementations, and section 7 rule 6 is not violated because they never both run: `hook_enable` answers under the OSP rulesets, `ctf_hook` under `ctf`.

The generic latched `hook` modifier has a deliberately narrower role than that table can imply: `hook 1` asks the four OSP rulesets to enable their native hook after a fresh baseline, while `hook 0` does not disable an explicit OSP setting or a live hook vote. CTF reads `ctf_hook` and Arena its `arena.cfg` `grapple:` setting, and both refuse the modifier with a line naming that switch. Native OSP, botlib, `sv ruleset`, and `match_info` use integer semantics, so `hook_enable 0.5` is off.

| cvar | default | what it does |
|---|---|---|
| `hook_enable` | `0` | off unless configured, requested by `hook 1`, or voted on. An OSP config's native value forms the fresh baseline; a pending generic request is applied after that config has executed. `hook 1` and a passed vote WRITE it, and the write outlives them in the process, which is why the four OSP configs set it |
| `hook_speed`, `hook_pullspeed` | `1600`, `1000` | flight and pull |
| `hook_wait` | `0.5` | seconds between throws |
| `hook_holdtime` | `7.5` | seconds on a surface before the hook releases itself |
| `hook_holdplayertime` | `5.0` | the same for a hooked player |
| `hook_initdamage`, `hook_incdamage`, `hook_maxdamage` | `20`, `1`, `30` | damage on hit, per tick, and its ceiling. **`hook_initdamage 0` also stops the hook sticking to a player at all** |
| `hook_color` | `0xd1d1d1d1` | cable colour |
| `hook_sky` | `0` | whether it may catch the sky |

### Client policy

What the server requires of, or does for, a connected client.

| cvar | default | what it does |
|---|---|---|
| `client_minping`, `client_maxping` | `0`, `0` | ping band; `0` disables the test |
| `client_maxrate`, `client_maxfps` | `0`, `0` | ditto for rate and fps |
| `client_infochange` | `4` | userinfo changes allowed per interval |
| `client_nomove` | `90` | seconds a client may stand completely still before being moved to observer. Teamplay and 1v1 only |
| `client_protect` | `0` | **seconds** of impervious shell after respawning, under `dm` only (`OSP_ClientProtect()`), and only for a player who spawns holding the blaster. Lost early on picking up a weapon, a powerup, a MegaHealth, armour, a power screen or shield or a rune, on `hookon`, on `kill`, and on a shot fired after the respawn's first 0.2 s. The match rulesets ignore it rather than writing 0 into it |
| `client_recover` | `0` | rejoin a match after a sudden disconnect with frags and deaths intact. **Prerequisite for `team_duelrecover`**, which does nothing without it |
| `client_botdetect` | `1` | client-side bot detection |
| `client_fastweap` | `0` | fast weapon switching for clients |
| `client_deathweapdrop` | `1` | drop the held weapon on death |
| `client_muzzlemode` | `0` | muzzle-flash policy |
| `client_hud` | `0` | where the match timer sits: `0` middle right, `1` bottom middle. Per-CLIENT in the donor and one statusbar here, which is what `g_stats.c` reconciles |
| `client_highscores` | `1` | highscore tracking, **FFA modes only** -- `dm` and `dmpro`, and only with a `timelimit` or `fraglimit`. Anything else sets it to `0`, and it stays `0` on later maps until it is set again: after `tdm` or `duel` an FFA map keeps no table unless its config sets this back, which `configs/dm.cfg` and `configs/dmpro.cfg` do |
| `client_highscoredir` | `highscores` | the hi-score directory under the gamedir, one file per `<port>/<mapname>`: read from the first of `homedir` and `basedir` that holds it, written -- and created -- under `homedir` when it is set (R-ENG-4) |
| `flood_msgs`, `flood_persecond` | `4`, `4` | that many messages inside that many seconds gets a client muzzled. **Shared with baseq2**, same names and defaults |
| `flood_waitdelay` | `10` | seconds the muzzle lasts |
| `fast_respawn` | `1.0` | **ITEM** respawn scaling, not player respawn: the fraction of an item's normal delay at full player count. Regular DM only, clamped up to `0.05` rather than rejected |
| `fast_minpbound`, `fast_maxpbound` | `1`, `20` | the player count is clamped into this band before the ratio is applied; `maxpbound` is also the ratio's base |

### Map rotation

| cvar | default | what it does |
|---|---|---|
| `map_file` | `maps.txt` | the rotation list |
| `map_queue` | `1` | `0` use the current map's own next-map info, `1` use `map_file`. A passed map vote, `r_map` and the admin menu's choice still name the next level with `0`; it turns off only the automatic rotation |
| `map_random` | `1` | `0` sequential through the queue, `1` random |
| `map_once` | `1` | use each map exactly once before recycling |
| `map_nocount` | `0` | `0` honour each map's min/max player counts when choosing the next one, `1` ignore them |
| `map_debug` | `0` | trace the chooser |

### Logging, and the three files that get written

Three independent logs, and the reason the names look inconsistent is that they are three donors' formats: a JSON event stream, the ngLog-era Standard Log, and a plain admin log.

| cvar | default | what it does |
|---|---|---|
| `statsfile`, `statsname` | `1`, **per ruleset** | the JSON event stream. **Registered ONCE, in `g_ruleset.c`**, with `ra2stats.jsonl` under `arena` and `osptourney.jsonl` otherwise. The collisions table above predicted `ra_statsname`/`osp_statsname` with a bare alias; resolution owning the pair turned out simpler, because resolution is what decides which log runs |
| `stats_logchat` | `0` | chat into the JSON stream |
| `stats_logallpickups` | `0` | every pickup, not just the ones that matter |
| `sl_log_method` | `0` | the Standard Log: 0 off, 1 file |
| `sl_filename` | `stdlog.log` | where, relative to the Quake II base directory when relative; an explicit absolute path remains absolute. An overlong resolved path disables Standard Log with a diagnostic rather than opening a truncated pathname |
| `sl_log_flush` | `2` | `0` flush when the system buffer fills, `1` that or a map change, `2` after every entry |
| `sl_log_logbots` | `1` | whether a bot's kills are logged. **The one cvar `counts.py` could not see**: it is registered through `import->cvar()` |
| `server_adminlog` | `0` | the admin log |
| `server_adminname` | `serveradmin.log` | where, under the gamedir (`homedir`, else `basedir`), like every writer |

**Twelve `nglog_*` names the donor documents are not registered**, and that is deliberate rather than missed: `nglog_logname`, `_logstyle`, `_flush`, `_buffer`, `_logchat`, `_logallpickups`, `_worldstats` and the five `nglog_ngstats_*` are the 1999 stack's own configuration, and its two halves live here under the names above -- the event stream as `stats_*`, the Standard Log as `sl_*`. The five `nglog_ngstats_*` drove an external `ngstats` binary through `system()`, which the security rules do not allow at all.

### Referee, admin and the player list

| cvar | default | what it does |
|---|---|---|
| `referee_enable` | `0` | whether a client may CONNECT as a referee at all, given the password below |
| `referee_password` | `""` | and its password. Empty and `none` both mean there is none, and then only `rcon_password` makes a referee. A wrong password costs the connection the backoff every login shares, 2 seconds doubling to 30 (R-SEC-12). Registered with `""` rather than the donor's NULL, for which `Cvar_Get` returns a NULL pointer on a server that has not set it |
| `rcon_password` | `""` | the engine's, re-obtained. What gates the bot menu -- and **unset is not a password of `""`**: a server that never set one refuses the menu to every client, rather than admitting the `menu ""` that the donor's bare `strcmp` matched. The host of a listen server needs it either way, because they are exempt. To the menu `none` is a password like any other -- q2pro gives it no meaning -- and a wrong one costs the connection the shared login backoff (R-SEC-12). Its client-side readers are `menu <pw>` and tourney's `referee <pw>`, which reads `none` as "no password" as the donor does |
| `player_file` | `players.txt` | known players and their privileges, read when the game starts |
| `player_reload` | `0` | re-read it at map change. `sv playerlist <file>` is the only way to load another file |
| `player_ban` | `0` | **which way `player_file` reads**: `0` the listed players are DENIED, `1` only the listed players are ALLOWED. A deny-list or an allow-list, not a flag for whether bans apply |
| `console_timestamp` | `0` | **not a timestamp.** The donor's own text: local server info printed to the console every *value* MAP CHANGES; `0` disables. Our default is `0` where the donor's was `10` |

### Demos, camera, qualifier and the MOTD

| cvar | default | what it does |
|---|---|---|
| `demo_player`, `demo_referee` | `0`, `0` | server-FORCED recording, separately for players and referees: `0` none, `1` forced demo, `2` demo plus an end-of-level screenshot |
| `demo_tag` | `tourney_tag` | filename tag |
| `camera_depth` | `60.0` | how far behind a chased player the camera sits. Positive floats only |
| `camera_pitch` | `15.0` | the camera's up/down angle, `0.0` to `45.0` |
| `qualifier_numspots` | `0` | how many ranks qualify -- a player at or above this rank gets a `*` on the scoreboard. `1` is top fragger only, `0` is plain deathmatch |
| `qualifier_forceskins` | `0` | force one skin on everybody. Only meaningful under `dmpro`, the donor's Qualifier DM |
| `qualifier_skinname` | `male/grunt` | which |
| `motd_file` | `motd.txt` | message of the day. **At most 9 rows** of 32 columns, which is the donor's own limit; a `"` is shown as `'`. The arena menu reads `motd.txt` itself and does not use this |
| `motd_center` | `0` | centre-print it rather than console-print it |

### Bots, and physics

`bots_minplayers` and `bots_botfile` are tourney's per-ruleset pair and are documented in the bot-layer section; the four below are tourney's own.

| cvar | default | what it does |
|---|---|---|
| `bots_autoload` | `0` | how tourney's flat count (`bots_minplayers`) runs. `0`: bots come only by a vote or a command, however high `bots_minplayers` is. `1`: the bot file (`bots_botfile`, a script of `sv addbot` lines) is exec'd once, 25 frames in. `2`, `3`, `4`: the fill keeps `bots_minplayers` seated, after `bots_delayload`'s delay on a dedicated server and never in an intermission (`OSP_BotFillReady`) -- except `2` under `duel`, which runs none, the donor's 1v1 restriction (`3` is `2` without it), and `4` adds bots up to the count whoever else is playing. Read when the game starts and when a configuration change execs a new config (R-OSP-20), not live: a flat count wants it set before the map loads. `botfill`, which sizes the fill from the game instead, is not gated on it. **Shared** -- the bot layer registers it too, same default |
| `bots_delayload` | `0` | wait before adding the first bot: the fill starts `125 * N + 15` frames into a level on a dedicated server -- 1.5 s at `0`, then 12.5 s more per step -- and 25 frames in on a listen server |
| `bots_warmuptime` | `0` | seconds before a bot readies up under `dmpro`, `tdm` or `duel`, counted from the bot's own arrival; `0` means once every person in the game is ready -- at once on a server with none. The donor's `0` never readied a bot at all |
| `bots_noclients` | `0` | `0`: when the last person leaves, the bots a vote added (`vote addbot`) leave with them; `1` they stay. The bots the fill or the flat count seated are the fill's, and this does not touch them |
| `damage_railgun` | `100` | railgun damage, the one weapon OSP made tunable |
| `sv_airaccelerate` | `0` | the engine's, re-obtained so a config setting it here still works |

### Five that upstream registers and never reads

Found by asking of every one of tourney's 163 cvar globals whether anything in the tree ever reads it. These five are registered by tourney, appear in the donor's own configuration file, and are answered **here** -- `osp-tourney` registers each one and reads none of them, so the reader is this tree's and so is the decision to add it.

| cvar | default | what it does | the read | who answers |
|---|---|---|---|---|
| `nextlevel_click` | `15.0` | seconds an intermission ignores a key press for. `0` switches the press half off | `ClientThink`, `p_client.c` | the OSP four only (R-RA-10a). `arena`, `ctf` and `sp` keep baseq2's five seconds, which under `arena` is RA2's own |
| `nextlevel_default` | `45.0` | seconds after which an intermission ends with **no** press at all. `0` switches the clock off | `G_CheckIntermissionExit()`, `g_main.c` | every deathmatch ruleset, `ctf` included (R-RA-10, R-OSP-15, R-CTF-9). Not `sp` |
| `numgibs` | `4` | gibs thrown by the main loop in `player_die`, clamped to 0..32 | `OSP_GibCount()`, `osp_main.c` | the OSP four only; every other ruleset takes baseq2's literal 4 |
| `match_endmusic` | `default` | the track played to each client at the end of a match; `default` or empty keeps tourney's five-tune `wav_file` rotation | `ClientThink`'s intermission block, `p_client.c` | the OSP four, free-for-all only -- a team ruleset plays the winner and the loser fixed tunes |
| `sl_log_style` | `0` | which Standard Log dialect. `0` is Standard Logging v1.2 and is the only one the format defines, so the read holds the cvar to it: any other value prints one line naming the legal one and is reset | `sl_OpenLogFile()`, `sl_write.c` | the OSP four, and only with `sl_log_method` bit 0 set -- a server that is not logging is not told about the format it is not writing (R-OSP-10) |

**Both `nextlevel_*` are read in SECONDS and the donor reads them in FRAMES.** `port_osp:p_client.c` writes `level.framenum > level.intermission_framenum + nextlevel_click->value`, a frame count plus a number its own documentation calls seconds, so at ten frames a second the pair is off by `BASE_FRAMERATE` across its whole documented range. This tree scales it, which is the same deliberate divergence `respawn_delay` below carries and for the same reason; `tools/units.py` is the standing check for the class.

`sl_log_style` is the one of the five whose read cannot do more than it does. The other four had a behaviour waiting for them; this one has a format with a single dialect, so the whole of the read is holding the cvar honest about that. A second dialect invented to give it range would be a feature rather than an import.

### Three whose read is upstream's

**`respawn_delay`, `power_armor_screen` and `power_armor_shield` are live in `osp-tourney`** -- `p_client.c:2625` adds the first to the forced-respawn test, and `g_combat.c:202`/`:212` assign the other two to `damagePerCell` -- and are read at those same three sites here.

| cvar | default | what it does | upstream site |
|---|---|---|---|
| `respawn_delay` | `0` | seconds a forced respawn waits beyond the button test, when `DF_FORCE_RESPAWN` is set | `p_client.c:2625` |
| `power_armor_screen` | `1.0` | damage absorbed per cell by the power screen | `g_combat.c:202` |
| `power_armor_shield` | `2.0` | the same for the power shield | `g_combat.c:212` |

All three answer **only under `dm`**, because upstream gates them on `!m_mode` and `m_mode` 0 is RegularDM -- a tournament cannot be re-balanced by a cvar mid-series. And with the defaults **none of them changes anything**: `0` frames makes the respawn test identical to the one above it, and `1.0`/`2.0` are exactly the constants that were hardcoded. That is why nobody noticed the reads were gone.

The clamp on the two ratios is upstream's and is a **reset, not a clamp**: a value above `2` does not become `2`, it becomes the default. Kept as the donor wrote it.

`respawn_delay` also carries a **deliberate divergence**. Upstream writes `client->respawn_framenum + resp_delay->value` -- a frame count plus a value its own documentation calls "a delay (in seconds)" -- so at 10 fps the cvar is off by `BASE_FRAMERATE` across its whole documented range. This tree scales it, so the seconds are seconds. See `tools/units.py`.

### Four handles whose name is not the cvar's

Worth knowing before grepping: four of tourney's globals are named differently from the cvar they hold, so searching for the cvar name finds only the registration line and searching for the handle finds no cvar at all.

| the global | the cvar it registers |
|---|---|
| `match_features` | `match_info` |
| `nextlevel_lazy` | `nextlevel_default` |
| `osp_game` | `gamename` |
| `resp_delay` | `respawn_delay` |

`team_a_score` / `team_b_score` are a fifth case of a different kind: the handles really are unread, but the cvars are not dead -- `Score_A` and `Score_B` are written by name with `gi.cvar_set` and published through `CVAR_SERVERINFO`, which is the whole of what they are for.

### The nine names tourney shares with the rest of the tree

None is a collision: each is either an engine cvar re-obtained for a handle, or one name two layers legitimately read with the same default.

| name | the other claimant | why it is not a duplicate |
|---|---|---|
| `hostname`, `port`, `gamename`, `dmflags` | the engine | re-obtained. The engine owns the flags; a second `gi.cvar` returns the same cvar |
| `rcon_password` | `osp_cmds.c` and the bot menu | one name, one default, two readers |
| `flood_msgs`, `flood_persecond`, `flood_waitdelay` | baseq2 | same three names, same defaults, and OSP's flood control replaced baseq2's in the OSP rulesets under section 7 rule 6 |
| `bots_autoload` | the bot layer | same default; the re-obtain is idiomatic |

`counts.py --duplicates` reports **zero** names registered from two translation units with different defaults, and it now runs in `make check` -- which it did not before the regeneration, so every earlier statement of that fact rested on a script somebody had run by hand once.

## The bot layer

Every cvar the Gladiator SDK's `bl_*.c` names, measured against `osp-tourney@1895f8e`'s six files: **28**, all reachable here. Eighteen are literals at a `gi.cvar` call, eight arrive through `BotSetVarIfSet`'s name parameter, and two -- `minimumplayers` and `botfile` -- through the per-ruleset accessor below. `python3 tools/counts.py --tree ../osp-tourney --duplicates` is the tool that says so; nothing in the donor's set is absent.

| cvar | default | meaning |
|---|---|---|
| `bots` | `1` | **New here.** The modifier resolution: the operator's switch for the whole layer, latched, refused under `sp` -- silently at the default, which is not a request there, and by name otherwise. `G_BotsAllowed()` reads it, so there is one question asked by name. `bots 0` on a ruleset that accepts bots makes `addbot`, `addrandom`, `SP_bot` and `CheckMinimumPlayers` all refuse |
| `botlib` | `gladiator.so` / `gladiator.dll` | the botlib file to `dlopen`. The 1999 defaults were `gladiator.dll` and `gladi386.so`; the 1999 binaries are not a target, so the name follows what `gladiator-bot-restored` actually produces -- with one exception it makes no sense to inherit: on macOS that Makefile emits `gladiator.dylib`, and a release package ships it under this name instead, because `dlopen` reads the Mach-O header and not the extension. A value containing a path separator is used as given; a bare name is looked for under `homedir`/`gamedir`, then `libdir`/`gamedir` -- beside the game library, where q2pro looks for that -- and the first that holds the file is used, else `basedir`/`gamedir`, the 1999 search. An OS path in every case, because `dlopen` needs one and the engine's filesystem is not one. One library per server, and `bots.cfg` names none |
| `serveronlybotcmds` | **`1`** | a deliberate policy change from the 1999 default of 0: that default exposed `bl_spawn.c`'s bot-name copies to any client. Also what gates the bot menu behind `rcon_password` -- with one exemption, the host of a listen server, and only for the menu: every other bot command still refuses them, because `BotCmdRefused` asks whether the caller is the console and a host player is not |
| `freebotlib` | `1` | unload the library when its last user goes |
| `minimumplayers` / `bots_minplayers` | `0` / `4` | the NAME is per ruleset and so is the default, and the pair is obtained **once** through `BotMinPlayers()` -- `osp_main.c` registers `bots_minplayers` with `"4"` and a second registration with `"0"` would be a fifth duplicate |
| `botfile` / `bots_botfile` | `botcfg/bots.cfg` | ditto, through `BotFile()`. Read through the engine's filesystem, so a `bots.cfg` inside a `.pak` is found -- and where the engine offers no filesystem extension, which is what a **proxy game library** such as q2admin leaves behind it, through a plain stdio read of `<homedir>/<gamedir>/`, then `<basedir>/<gamedir>/`, then the path as given (R-ENG-4). That arm finds loose files only; a bot list inside a pak needs the extension |
| `bots_autoload` | `0` | tourney's own, and the gate on its flat count (see the tourney section): `0` never, `1` the bot file once, `2`..`4` the fill -- `2` not under `duel`; `4` is the arm that keeps the server topped up with bots regardless of the player count |
| `botctfteam` | `0` | 0 auto-assign, 1 red, 2 blue. Re-registered here because both donors' ports dropped it |
| `botfill` | `0` | **One cvar for every ruleset that takes bots.** It was named per ruleset under a rule that never covered it: that rule governs cvars a **donor** named, and this switch is Colosseum's own and appears in no donor's documentation. section 7 rule 6 governs instead, and the logic was already shared. The CVAR default is `0`, but every shipped config that takes bots sets it to `1` and zeroes the flat count beside it -- `dm`, `dmpro`, `tdm`, `duel`, `ctf`, `arena`; `sp` sets neither. `1` replaces the flat count as the target with a number read off the GAME, and the TARGET is still per ruleset: under `arena` two teams of the arena's `playersperteam` or, for a pickup arena, its even spawn count -- and, with nobody on the map at all, the arena the bots are staged in, so that an empty arena server fills the way an empty `dm` one does instead of waiting for the first person to arrive; under `ctf` twice the smaller of a base and half the shared spawn pool; under `dm` and `dmpro` the shared spawn pool; under `tdm` `2 * team_maxplayers` and under `duel` 2 -- two full teams, the capacity those two declare, and a duel's team is one player whatever `team_maxplayers` says. `sp` has no bots. The ceilings are `game.maxclients` -- LATCHED, and 8 by default because Q2PRO registers it before the library loads -- and, under `ctf` and `arena`, the bot list in `bots.cfg`, where the fill settles once every bot it names is in the game; the OSP four seat a suffixed copy of a character instead and are held by `maxclients` alone. Both are applied in one place, `BotFillTarget()`. `sv ruleset` prints one line naming the ruleset, the target and where it came from, because the number is computed rather than stored -- and `staging` on the end of the arena form, which is the one thing that line's numbers cannot say for themselves. A passed `vote rembot` under the OSP four subtracts from the target for the rest of the level and the row says `, N voted out` while it does (R-OSP-16) |
| `ra_playercycle` | `1` | **behaviour **, not just a menu row. Gates RA2's own "the winning team goes to the front of the waiting queue"; `0` rotates the arena strictly by arrival |
| `arena` | `0` | 1999's, and which arena a new BOT joins -- `gladq2_src/bl_spawn.c` copies it into the bot's userinfo and `RA_BotJoinArena` reads it there. `1..N` is that arena, exactly as 1999 meant it, and pins the fill to it. **`0` is ours**, the default at all four registrations, and means "follow the people": the lowest-numbered arena where a person is on a team and bots are allowed -- none, if every arena with people in it has voted its bots out -- and with nobody on the map the staging arena: the one already holding the most bots, else the lowest-numbered pickup arena that will seat one, else the lowest-numbered arena that will. Resolved at join time and asked again while the bot can still be moved, so a bot added before the first person still ends up where that person went. It matters on a real RA2 map, where the pickup arena is wherever `arena.cfg` puts it and a fixed 1 puts every bot where nobody is; the shipped `configs/arena.cfg` sets `0`, having asked until 1.28 for a `botarena` that does not exist and that nothing has ever read |
| `ra_botcycle` | `1` | **A behaviour change.** When filling an arena, the **first** side is taken from the first waiting team that has a person on it, so no bot hogs the arena while somebody waits. First side only, which is `RA2_GetLongestWaitingHuman`'s shape; a server of only bots is unaffected. Both are registered by `arena_init()` and obtained by the bot menu with the same default |
| `log` | **`0`** | the botlib's own trace file. The second deliberate policy change, from the SDK's `1`: a public server should not write an AI trace by default |
| `nochat`, `fastchat`, `altnames` | unset | pushed as libvars only when set, which is the SDK's own shape. **`fastchat` pushes `"1"`** -- corrected. The donor pushed `"0"` with a comment rationalising it as turning the answer *delay* off, and the botlib does not read it that way: it tests `fastchat == 0` and only *then* applies the random gate that decides whether a bot says anything at all, so pushing `"0"` left `fastchat 1` unable to do the one thing it is named for. A row saying otherwise would be describing the donor rather than this tree |
| `rocketjump` | `1` | pushed as a libvar, as the cvar's value: `0` stops the bots rocket-jumping. The donor pushed only `1`, which is also the botlib's own default, so `0` could not reach it |
| `max_aaslinks`, `max_bsplinks`, `max_levelitems`, `framereachability` | unset / `20` | BSPC sizing, pushed only when positive |
| `autolaunchbspc`, `forceclustering`, `forcereachability`, `forcewrite`, `nooptimize` | unset | AAS compilation switches. `autolaunchbspc` is the one exception, and **fires on Windows only**: there, a map with no `.aas` makes the botlib start the `winbspc.exe` in the gamedir without waiting for it, and refuse the map until the file exists; every other build only prints that BSPC is a Win32 program. The other four act on an `.aas` that already exists -- they are the second half of making one, not a way to get one |
| `basedir`, `gamedir`, `cddir` | engine's | `basedir` falls back to `fs_basedir` and then `.`, `gamedir` to `game` and then `colosseum`, because under Q2PRO `gamedir` is `CVAR_ROM|CVAR_SERVERINFO` and `basedir` may not exist at all. Reported once at load |
| `sv_fps` | engine's | read, not set: one warning is logged at `InitGame` when it is not 10, because `FRAMETIME` is 0.1 s and Colosseum does not advertise `GMF_VARIABLE_FPS` |

`runes` gains a meaning under `ctf` and the four OSP rulesets, `dm` among them: `runes 1` clears `DF_CTF_NO_TECH` under `ctf` and sets `runes_enable` to all five, `31`, where no rune is selected. It is still refused under `arena` and `sp`, with the same message, and still defaults to 0, which means "do not ask" rather than "turn them off".

## The Gladiator extras, as cvars

**328 cvars** total (`tools/counts.py`), and `counts.py --duplicates` reports **zero** names registered from two translation units with different defaults -- `basedir`, `gamedir` and `rcon_password` were the last three and are settled.

The rule is that each of the 1999 module's `#define`s is kept, becomes a cvar and is never compiled out. Six of its seven land here as the table below. The seventh, `VWEP`, guarded **visible weapons**, which is not a Gladiator feature: id shipped it from 3.20 on and q2pro carries it, so the 1999 `#define` was patching a pre-3.20 baseq2 this project does not have. There is nothing left to gate, so it gets no cvar; `sv extras` counts the `weapmodel` rows in the merged `itemlist` instead, which is the data the feature is.

| cvar | default | what it does |
|---|---|---|
| `g_gamelog` | `""` | A **filename**, not a flag: the feature's whole content is which file it writes to, and "" is the natural off. Set it and `InitGame` opens `<homedir-or-basedir>/<gamedir>/<name>`; `sv openlog`/`closelog`/`writelog` still work and `Log_ShutDown` runs from `ShutdownGame` |
| `g_clientlag` | `0` | Off, and the reason is a threat rather than taste: `lag` is a **client** command, so a player could ask the server to hold two seconds of their own input, and with maxclients 256 that is a queue whose length the client sets. With it off nothing is allocated at all and both commands say so |
| `g_triggercounting` | `0`, `CVAR_LATCH` | `trigger_counting`, and `trigger_relay` carrying a count through. Latched because it decides what a map spawns |
| `g_triggerlog` | `0`, `CVAR_LATCH` | `trigger_log`, which writes a map's own message into the game log the first time a client touches it. Latched, same reason |
| `g_rotatingbutton` | `0`, `CVAR_LATCH` | `func_button_rotating`. Latched, same reason |
| `g_observer` | **`1`** | the one extra that is **on**. It is the 1999 module's own observer and `observer` is what a 1999 config binds. Under `arena` and the four OSP rulesets it does not reach: those have their own observers, and this is the second exemption to section 7 rule 6. It answers for `ctf` and `sp` |

**Off means the entity is not created**, for the three that gate a spawn function -- not created-and-inert. An inert trigger still occupies an edict slot and still shows in every census. The **classnames stay registered either way**, so a map that uses one loads without `doesn't have a spawn function`.

**`netlog` still resolves and no longer does anything.** It named the remote host RA2 forwarded its event log to over UDP; that is not allowed, so the forwarding is gone. The name stays registered so a 1999 config still parses, under `arena` `InitGame` says once that setting it does nothing, and `logfile 2` writes the same lines to the local log.
