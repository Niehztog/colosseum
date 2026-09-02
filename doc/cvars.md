# Cvar inventory

Deliverable D4. The merged, deduplicated cvar inventory: every cvar Colosseum
registers, its default, its flags, the donor that introduced it, and — where
§7 rule 4 applied — which ruleset owns the bare name and what the prefixed alias
is (R-COMPAT-4, R-COMPAT-6).

Generated from `tools/counts.py --list cvars` and annotated. Regenerate rather
than hand-edit the counts — and since R-184 the regeneration is enforced:
`counts.py --duplicates` and its nine extraction controls run in `make check`, so
a figure here that the tree has moved away from fails the build rather than
sitting in a document. Before that, nothing re-ran the script that produced these
numbers.

## Phase 0: baseq2 only

**37 cvars**, which is exactly R-BASE-2's published figure — the first of the
spec's acceptance counts to be confirmed by a tool rather than asserted
(R-TOOL-2).

```sh
tools/counts.py --list cvars
```

## Phase 3: Threewave CTF

**69 cvars** total (`tools/counts.py`), of which fourteen are CTF's. Registered
in `CTFInit()`, which `InitGame` calls unconditionally — see
`reconciliation.md` R-42 for why the donor's failure to call it at all was a
crash rather than a missing feature.

| cvar | default | what it does |
|---|---|---|
| `ctf` | `0`, `CVAR_SERVERINFO` | **not a behaviour switch.** A legacy alias that can *select* the ruleset (R-MODE-2), and once resolution has decided, the dispatch forces it to agree so a Threewave-aware client reads the truth out of serverinfo. Registered by `g_ruleset.c`, not by CTF |
| `capturelimit` | `0`, `CVAR_SERVERINFO` | captures that end a match. Independent of `fraglimit` — R-CTF-2, and structural rather than patched: it is read before any fraglimit test can shadow it |
| `instantweap` | `0` | no raise or lower animation on a weapon switch |
| `ctf_hook` | `1` | the offhand hook of R-CTF-3, driven by `hookon`/`hookoff` |
| `laserhook` | `0` | the beam rendering rather than the cable. Threewave shipped this as a `#if`; it is a cvar because the brain has to be told which is in play (the libvar push is Phase 6) |
| `ctf_forcejoin` | `""` | force joiners onto a named team |
| `competition` | `0`, `CVAR_SERVERINFO` | 0 public, 1 admin-managed, 2+ match mode. `2` starts the level in `MATCH_SETUP` — **gated on the ruleset**, or a `dm` server with it left in its config would freeze all damage |
| `matchlock` | `1`, `CVAR_SERVERINFO` | no joining a match once it has started |
| `electpercentage` | `66` | share of votes an election needs |
| `matchtime` | `20`, `CVAR_SERVERINFO` | match length, minutes |
| `matchsetuptime` | `10` | setup window, minutes |
| `matchstarttime` | `20` | countdown, seconds |
| `admin_password` | `""` | the `admin` command's password |
| `allow_admin` | `1` | whether `admin` works at all |
| `warp_list` | `q2ctf1 … q2ctf5` | maps the `warp` vote may choose |
| `warn_unbalanced` | `1` | the statusbar's "too many players" line |

Two names that do **not** appear and are worth saying so about: `maxspectators`
survives from baseq2 although Threewave deletes it, because `dm` and `sp` still
use it (R-CTF-5); and `ch` is accepted and ignored (N7).

Nothing is renamed, aliased or prefixed yet; every name is q2pro's own with
q2pro's default and flags (R-BASE-2). The prefixing of §7 rule 4 begins in
Phase 3, and R-OSP-11's per-ruleset bot cvars in Phase 6.

## Phase 1 additions

| cvar | flags | meaning |
|---|---|---|
| `g_ruleset` | `CVAR_LATCH \| CVAR_SERVERINFO` | the primary ruleset: `dm` \| `dmpro` \| `tdm` \| `duel` \| `ctf` \| `arena` \| `sp`. Empty means "infer": legacy aliases first, then `coop`, then `dm`. An invalid value warns and falls back to `dm` (R-MODE-1). **Seven since 1.36**: the first four are OSP's four modes of play, which `match_mode` selected inside a `tourney` ruleset until the flattening (R-OSP-12). `tourney` is not an alias and not a special case — it is simply not a ruleset name, and takes the unknown-value path with the same message any other unknown value gets |
| `xatrix`, `rogue` | `CVAR_LATCH` | independent content layers, valid with every ruleset, both may be on (R-MODE-3) |
| `teamplay`, `hook`, `runes` | `CVAR_LATCH` | modifiers, refused with a message by a ruleset that does not accept them (R-MODE-4). `teamplay` reaches only `arena` since 1.36: `tdm` and `duel` **are** team play, so the modifier that also reached it is the second selector the flattening removes |

Read but not registered by Colosseum: `ctf` and `rocketarena` as legacy ruleset
aliases (R-MODE-2), and `ch`, which is accepted and ignored (N7). They are read
with `gi.cvar(name, "0", 0)` so a config that sets them still works and Colosseum
does not claim their flags.

`deathmatch` and `coop` remain the engine's. Resolution runs in both directions:
a legacy value can select the ruleset, and once selected the ruleset is
authoritative and `deathmatch` is forced to agree (R-MODE-2,
`doc/reconciliation.md` R-8).

## Known collisions, from the measured donors

Not yet in the tree; recorded so the Phase 3–5 imports do not discover them late.

| name | claimants | resolution |
|---|---|---|
| `statsname` | RA2 (`ra2stats.jsonl`), tourney (`osptourney.jsonl`) | §7 rule 4: `ra_statsname` / `osp_statsname`, bare name a read-only alias to the active ruleset's (R-COMPAT-6) |
| `statsfile` | RA2 and tourney, **same default** | not a collision — an idiomatic re-obtain. R-COMPAT-6 names it as one; see `reconciliation.md` §3.3 |
| `minimumplayers` vs `bots_minplayers` | Gladiator vs tourney | both registered, authority is per ruleset (R-OSP-11). Since 1.36 the OSP names are authoritative under **four** rulesets — `dm`, `dmpro`, `tdm`, `duel` — and Gladiator's under `ctf` and `arena`. A hand-written `dm` config setting `minimumplayers` stopped being read; `configs/dm.cfg` carries the new name |
| `botfile` vs `bots_botfile` | Gladiator vs tourney | ditto |

## Rocket Arena 2 — added in spec 1.17

Registered unconditionally so that a config naming one is not rejected, but only
`arena` reads them (R-RA-1a, R-OSP-10).

| cvar | default | meaning |
|---|---|---|
| `netlog` | `""` | UDP destination for the RA2 event log. **The only socket user in the tree**; empty means no socket is opened |
| `logfile` | `0` | the engine's own console-logging cvar, re-obtained: RA2 gates its stdlog on it rather than adding a second switch |
| `hostname`, `port` | engine's | re-obtained for the round log's header |
| `public` | `1` | a private server clears `netlog` rather than forwarding |

`statsfile` / `statsname` are R-COMPAT-6's pair and arrive with Phase 5, where
the collision with tourney's is resolved.


## OSP Tourney DM — added in Phase 5

**227 names**, of which **218 are tourney's own** and nine it shares with the
rest of the tree. That is more than two thirds of the tree's 326, and it is why
this section is grouped by what a setting *does* rather than listed flat: an
operator arrives here with a question about warmup or about voting, never about
the alphabet.

**The count is `counts.py`'s, and the script had to be fixed to produce it.** It
counted the unique first argument of a literal `gi.cvar("...")` and nothing else,
which meant it could not see two shapes this layer uses. `stdlog.c` registers
`sl_log_logbots` through `import->cvar(...)`, a `game_import_t` reached as a
parameter rather than through the global — the same engine function, invisible to
a `gi.`-anchored pattern. And R-183 moved the twenty-six `allow_*` names out of
literal position into `osp_allow_items[]`, reached as
`gi.cvar(osp_allow_items[i].cvar, ...)`. **The published figure therefore FELL by
sixteen at the moment the tree gained ten cvars** — a measurement that moves the
wrong way, which is worse than one that is merely naive. Both shapes are resolved
now (a table column by finding the member's position in its struct), nine
controls cover the extraction, and `--duplicates` runs in `make check`, so this
document's numbers are re-derived on every build rather than asserted once.
**Four sites remain unresolvable** and are printed rather than dropped: two
`BotSetVarIfSet` parameters, `g_ruleset.c`'s alias table, and `osp_allowed()`'s
own parameter. The count is a floor and says so.

**Where the descriptions come from.** The donor ships its own documentation --
`vendor/harness/tools-inputs/osp/docs/server-settings.txt`, 126 of these 227
names -- and that is the source used here, not the cvar's spelling. Writing this
section from the names first produced **eight wrong descriptions**, several of
them the kind an operator would act on: `client_protect` is seconds of spawn
shell and not a name password, `demo_player` is server-FORCED recording and not
permission to record, `match_prestartpercent` makes the weapons disappear rather
than starting a countdown, `fast_respawn` scales ITEM respawn and not player
respawn, and `vote_carryover` carries voted VALUES across a map rather than an
unresolved vote. The remaining 101 were read from the code. R-184 records the
correction pass, because the failure mode is worth remembering: a plausible
description is not a checkable one.

**Translating a 1999 config.** The donor selected its four modes with
`match_mode`, which R-OSP-12 flattened into `g_ruleset`. The values map exactly:

| donor | `g_ruleset` | |
|---|---|---|
| `match_mode 0` | `dm` | RegularDM — join the fray and play |
| `match_mode 1` | `dmpro` | Qualifier DM — everyone types `ready` first |
| `match_mode 2` | `tdm` | team play |
| `match_mode 3` | `duel` | 1v1 |

Everything here is registered under **all seven rulesets** so a config naming one
is not rejected (R-RA-1a, R-OSP-10); only `dm`, `dmpro`, `tdm` and `duel` read
them. Those four are OSP's own modes of play, which `match_mode` selected inside
a single `tourney` ruleset until R-OSP-12 flattened it — so where the donor's
documentation says "match_mode 3", this tree says `g_ruleset tdm`.

### Match control, warmup and intermission

`match_type` and `match_info` are the two the server browser sees. **`RULESET_DM`
is OSP's RegularDM and not baseq2 deathmatch** (Amendment 1.36), which is why a
default of `RegularDM` appears under a ruleset named `dm`.

| cvar | default | what it does |
|---|---|---|
| `match_type` | `RegularDM`, `CVAR_SERVERINFO` | the mode's name for a browser. Set from the ruleset, not by hand |
| `match_info` | `None`, `CVAR_SERVERINFO` | free-text match description |
| `match_endinfo` | `OSP Tourney DM v(2.75)` | the line the intermission prints |
| `match_countdown` | `30` | seconds from both teams ready to live |
| `match_countinfo` | `1` | display the match info during the countdown |
| `match_startsound` | `1` | play the start sound |
| `match_readypercent` | `100` | share of players who must be ready |
| `match_prestartpercent` | `50` | share of players ready at which the WEAPONS DISAPPEAR — the donor's hint to everyone else to ready up, not a countdown threshold |
| `match_latejoin` | `2` | joining after a synced match has begun: `0` not at all, `1` connect as an observer only, `2` may enter only if invited or a team is unlocked (teams auto-lock at the start), `3` free entry |
| `match_pausetime` | `60.0` | seconds a timeout lasts |
| `match_timeouts` | `3` | timeouts per team |
| `match_strictmode` | `0` | `0` the traditional `ready` style; `1` clients cannot change their ready status at all and a referee or admin must ready them |
| `warmup_health` | `150` | health during warmup, where it is deliberately not `max_health` |
| `warmup_armor` | `200` | armour during warmup |
| `time_remaining` | `ServerInit`, `CVAR_SERVERINFO` | the clock, published for a browser. A **string**, because "Warmup" and "Overtime" are not times |
| `map_halt` | `0` | halt at the end of the level. It works by ZEROING both `nextlevel_*` timers, which is the only thing in the tree that reads either of them |

### Team play

| cvar | default | what it does |
|---|---|---|
| `team_maxplayers` | **per ruleset** | `1` under `duel`, `4` otherwise, and under `duel` it is re-registered `CVAR_NOSET` after being forced to 1 — 1v1 is two teams of one whatever the config asked for. Also clamped so `2 * team_maxplayers <= maxclients` |
| `team_a_name`, `team_b_name` | `Hometeam`, `Visitors` | team names |
| `team_a_skin`, `team_b_skin` | `female/athena`, `male/sniper` | forced skins |
| `team_a_hookcolor`, `team_b_hookcolor` | `0xf2f2f2f2`, `0xd1d1d1d1` | per-team hook colour |
| `team_lockskin` | `0` | `0` clients may change team skins, `1` they may not. Teamplay and 1v1 |
| `team_idteam` | `1` | show a team-mate's name on the crosshair |
| `team_hurtteam` | `1` | friendly fire. Seeds `osp_teams[n]` once in `OSP_gameInit`, so a change takes effect at the next game-library load and not at the next map; a referee and the team menus write the per-team switch directly. **`configs/tdm.cfg` ships `0`** (R-194) |
| `team_hurtself` | `1` | self damage in team play |
| `ffa_hurtself` | `1` | self damage outside it |
| `team_overtime_mode` | `1` | `0` none, and a match may end tied; `1` sudden death, decided by the first kill or suicide; `2` timed rounds |
| `team_overtime_time` | `1` | minutes per timed overtime round, minimum 1. Read only when `team_overtime_mode` is `2` |
| `team_overtime_count` | `1` | how many periods before a draw stands |
| `team_recovertime` | `0.0` | seconds to hold a slot open for a disconnected player. **`0` disables `team_duelrecover` outright**, whatever that is set to |
| `team_duelrecover` | `0` | what happens when a team drops to zero players: `0` the match terminates, `1` it waits for the player to come back. Needs `client_recover` AND a non-zero `team_recovertime` |
| `team_nextuptime` | `45` | seconds the next pair has to get ready |
| `Score_A`, `Score_B` | **per ruleset** | the browser's two score cells. `Disabled`/`CVAR_SERVERINFO` under `tdm`, otherwise `""` with no flags and explicitly cleared — only team play publishes numbers |

### Voting

| cvar | default | what it does |
|---|---|---|
| `vote_enable` | `1` | the whole mechanism |
| `vote_threshold` | `51` | percent to pass |
| `vote_time` | `45` | seconds a vote is open |
| `vote_carryover` | `1` | carry voted VALUES across a map load rather than reverting to the server's defaults |
| `vote_countspectators` | **per ruleset** | `1` outside team play, `0` inside it |
| `vote_enable_map`, `_frag`, `_time`, `_kick`, `_toggles` | `1` | the votes that are on by default |
| `vote_enable_config`, `_bots`, `_runes`, `_hook` | `0`, `0`, `0`, `1` | and the ones that are not. `hook` is on because the hook itself defaults off — the vote is how it gets turned on |
| `vote_bots_max` | `8` | ceiling a bot vote may ask for |
| `vote_config_list` | `serverconfigs.txt` | configs a config vote may choose |
| `vote_config_default` | `0` | load one at boot |
| `vote_config_defaultname` | `default` | which. **Our default differs from the donor's**, which documents `none`; a name that does not exist disables config cycling either way |
| `__current_config` | `default` | **not for an operator.** The name of the config in force, kept so a vote can report it; the leading underscores are the donor's own convention for "internal" |
| `menu_maxfrag`, `menu_fragstep` | `100`, `5` | the fraglimit vote menu's ceiling and step |
| `menu_maxtime`, `menu_timestep` | `120`, `5` | the timelimit vote menu's |

### The item switches — `allow_*`

**Twenty-six, of which ten are R-183's** and are the reason this family is one
table in the code (`osp_allow_items[]`: cvar, classname, banner tag, log name)
rather than three lists that had drifted apart. `0` inhibits the item where a map
placed it; the scoreboard banner names what is off, and the stats log records it.
Two rows are inhibited and deliberately named by neither, which the table says
with `NULL` rather than by absence.

Ammunition follows its weapon by a **derived rule** rather than a cvar of its
own: shells go when both shotguns are gone, bullets when both bullet weapons are,
and rockets, slugs, mag slugs, flechettes, prox and disruptor ammo each when
their one weapon is. `allow_ammo_cells` is the exception and has a switch,
because three weapons share cells and the donor chose not to derive it.

| group | cvars |
|---|---|
| baseq2 weapons | `allow_shotgun` `allow_supershotgun` `allow_machinegun` `allow_chaingun` `allow_grenadelauncher` `allow_rocketlauncher` `allow_hyperblaster` `allow_railgun` `allow_bfg` |
| baseq2 ammo and items | `allow_ammo_grenades` `allow_ammo_cells` `allow_item_powerscreen` `allow_item_powershield` `allow_item_quad` `allow_item_invul` `allow_item_pack` |
| The Reckoning — R-183 | `allow_ionripper` `allow_phalanx` `allow_ammo_trap` |
| Ground Zero — R-183 | `allow_etfrifle` `allow_proxlauncher` `allow_plasmabeam` `allow_chainfist` `allow_disruptor` `allow_ammo_tesla` `allow_item_nuke` |

All default `1`. The Disruptor has a row although it is `IT_NOT_GIVEABLE` (R-16):
`give` cannot hand it out, but a map may still *place* one, and inhibiting a
placed entity is exactly what this table does.

`allow_id` (`1`) is not one of these. It governs **ID tagging** — a team-mate's
name under the crosshair — and it is four-valued rather than boolean: `2` is
server-locked off and `3` server-locked on, both of which force the client's own
flag and refuse to let it toggle; anything else lets the client decide.

### The loadout — `start_*`, `max_*`, `pack_*`

Three parallel families over one ammo list: what a player spawns with, the
carrying capacity, and what a Bandolier or Pack raises it to. **Fifteen of the
forty are R-181's** — the two content layers' five ammo types (`magslug`,
`flechettes`, `prox`, `tesla`, `trap`), which had no cvars at all until then, so
under `xatrix 1` or `rogue 1` a referee could set every baseq2 ammo and not the
Phalanx's.

| | `start_` | `max_` | `pack_` | `warmup_` |
|---|---|---|---|---|
| `health` | `100` | `100` | `100` | `150` |
| `armor` | `0` | `200` | `250` | `200` |
| `armortype` | `0` | — | — | — |
| `shells` | `0` | `100` | `200` | — |
| `bullets` | `0` | `200` | `300` | — |
| `cells` | `0` | `200` | `300` | — |
| `rockets` | `0` | `50` | `100` | — |
| `slugs` | `0` | `50` | `100` | — |
| `grenades` | `0` | `50` | `100` | — |
| `magslug` (R-181) | `0` | `50` | `100` | — |
| `flechettes` (R-181) | `0` | `200` | `200` | — |
| `prox` (R-181) | `0` | `50` | `50` | — |
| `tesla` (R-181) | `0` | `50` | `50` | — |
| `trap` (R-181) | `0` | `5` | `5` | — |

`weapon_initial` (`0`) and `weapon_have` (`0`) are **bitmasks over nineteen
weapon slots** — which weapon a player spawns holding, and which they spawn
owning. R-180 stopped them being written as literal indices: the slots are
resolved by NAME through `OSP_resolveLoadoutSlots()`, lazily, because
`OSP_gameInit` runs before `InitItems()`. The nineteen include the layers'.

Armour behaviour is four numbers per class rather than a flag:

| cvar | default | meaning |
|---|---|---|
| `armor_body` | `100 200 0.80 0.60` | count, max, normal absorption, energy absorption |
| `armor_combat` | `50 100 0.60 0.30` | ditto |
| `armor_jacket` | `25 50 0.30 0.00` | ditto |
| `armor_shard` | `2` | points a shard adds |

### Runes

OSP's runes are the CTF techs by another name, and `runes` is the R-MODE-4
modifier that asks for them; these tune them. `runes_enable` is a **bitmask** of
which runes may appear, clamped to `0x1f`.

| cvar | default | what it does |
|---|---|---|
| `runes_enable` | `0` | a bitwise SUM: `1` resist, `2` strength, `4` haste, `8` regeneration, `16` vampire, so `31` is all five. Clamped to `0x1f` |
| `runes_min`, `runes_max` | `3`, `12` | how many exist. `max` is forced up to `min` if a config inverts them |
| `runes_perplayer` | `0.6` | ratio of runes to active players |
| `runes_model` | `models/items/c_head/tris.md2` | the model every rune uses — which is John Carmack's head, and the donor says so |
| `runes_flash` | `1` | flash the client in their rune's colour when it is USED, not when picked up |
| `runes_resist`, `runes_strength` | `2.0`, `2.0` | damage divisor and multiplier |
| `runes_vampire`, `runes_vampire_max` | `0.5`, `200` | share of damage returned as health, and its ceiling |
| `runes_regen_hmax`, `runes_regen_amax` | `200`, `100` | regeneration ceilings |

### The hook

Registered by tourney and **distinct from CTF's grapple** — one concept, two
implementations, and §7 rule 6 is not violated because they never both run:
`hook_enable` answers under the OSP rulesets, `ctf_hook` under `ctf`.

| cvar | default | what it does |
|---|---|---|
| `hook_enable` | `0` | off unless voted on |
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
| `client_protect` | `0` | **seconds** of impervious shell after respawning, regular DM only. Lost early on picking up a weapon, powerup, MegaHealth or Power Shield |
| `client_recover` | `0` | rejoin a match after a sudden disconnect with frags and deaths intact. **Prerequisite for `team_duelrecover`**, which does nothing without it |
| `client_botdetect` | `1` | client-side bot detection |
| `client_fastweap` | `0` | fast weapon switching for clients |
| `client_deathweapdrop` | `1` | drop the held weapon on death |
| `client_muzzlemode` | `0` | muzzle-flash policy |
| `client_hud` | `0` | where the match timer sits: `0` middle right, `1` bottom middle. Per-CLIENT in the donor and one statusbar here, which is what `g_stats.c` reconciles |
| `client_highscores` | `1` | highscore tracking, **FFA modes only** |
| `client_highscoredir` | `highscores` | where |
| `flood_msgs`, `flood_persecond` | `4`, `4` | that many messages inside that many seconds gets a client muzzled. **Shared with baseq2**, same names and defaults |
| `flood_waitdelay` | `10` | seconds the muzzle lasts |
| `fast_respawn` | `1.0` | **ITEM** respawn scaling, not player respawn: the fraction of an item's normal delay at full player count. Regular DM only, clamped up to `0.05` rather than rejected |
| `fast_minpbound`, `fast_maxpbound` | `1`, `20` | the player count is clamped into this band before the ratio is applied; `maxpbound` is also the ratio's base |

### Map rotation

| cvar | default | what it does |
|---|---|---|
| `map_file` | `maps.txt` | the rotation list |
| `map_queue` | `1` | `0` use the current map's own next-map info, `1` use `map_file` |
| `map_random` | `1` | `0` sequential through the queue, `1` random |
| `map_once` | `1` | use each map exactly once before recycling |
| `map_nocount` | `0` | `0` honour each map's min/max player counts when choosing the next one, `1` ignore them |
| `map_debug` | `0` | trace the chooser |

### Logging, and the three files that get written

Three independent logs, and the reason the names look inconsistent is that they
are three donors' formats: a JSON event stream, the ngLog-era Standard Log, and a
plain admin log.

| cvar | default | what it does |
|---|---|---|
| `statsfile`, `statsname` | `1`, **per ruleset** | the JSON event stream. **Registered ONCE, in `g_ruleset.c`**, with `ra2stats.jsonl` under `arena` and `osptourney.jsonl` otherwise. The collisions table above predicted `ra_statsname`/`osp_statsname` with a bare alias; resolution owning the pair turned out simpler, because resolution is what decides which log runs (R-COMPAT-6) |
| `stats_logchat` | `0` | chat into the JSON stream |
| `stats_logallpickups` | `0` | every pickup, not just the ones that matter |
| `sl_log_method` | `0` | the Standard Log: 0 off, 1 file |
| `sl_filename` | `stdlog.log` | where, relative to the Quake II base directory |
| `sl_log_flush` | `2` | `0` flush when the system buffer fills, `1` that or a map change, `2` after every entry |
| `sl_log_logbots` | `1` | whether a bot's kills are logged. **The one cvar `counts.py` could not see** until R-184: it is registered through `import->cvar()` |
| `server_adminlog` | `0` | the admin log |
| `server_adminname` | `serveradmin.log` | where |

**Twelve `nglog_*` names the donor documents are not registered**, and that is
deliberate rather than missed: `nglog_logname`, `_logstyle`, `_flush`, `_buffer`,
`_logchat`, `_logallpickups`, `_worldstats` and the five `nglog_ngstats_*` are the
1999 stack's own configuration, and its two halves live here under the names above
— the event stream as `stats_*`, the Standard Log as `sl_*`. The five
`nglog_ngstats_*` drove an external `ngstats` binary through `system()`, which
R-SEC-7 does not allow at all.

### Referee, admin and the player list

| cvar | default | what it does |
|---|---|---|
| `referee_enable` | `0` | whether a client may CONNECT as a referee at all, given the password below |
| `referee_password` | *(NULL)* | and its password. Registered with a **NULL** default, which is the donor's way of saying "no password will ever match" — distinct from `""` |
| `rcon_password` | `""` | the engine's, re-obtained. What gates the bot menu (R-BOT-28) — and **unset is not a password of `""`**: a server that never set one refuses the menu to every client, rather than admitting the `menu ""` that the donor's bare `strcmp` matched. The host of a listen server needs it either way, because they are exempt. `menu <pw>` is the only client-side reader; `rcon_password none` disables the gate, which is 1999's own escape hatch |
| `player_file` | `players.txt` | known players and their privileges |
| `player_reload` | `0` | re-read it at map change |
| `player_ban` | `0` | **which way `player_file` reads**: `0` the listed players are DENIED, `1` only the listed players are ALLOWED. A deny-list or an allow-list, not a flag for whether bans apply |
| `console_timestamp` | `0` | **not a timestamp.** The donor's own text: local server info printed to the console every *value* MAP CHANGES; `0` disables. Our default is `0` where the donor's was `10` |

### Demos, camera, qualifier and the MOTD

| cvar | default | what it does |
|---|---|---|
| `demo_player`, `demo_referee` | `0`, `0` | server-FORCED recording, separately for players and referees: `0` none, `1` forced demo, `2` demo plus an end-of-level screenshot |
| `demo_tag` | `tourney_tag` | filename tag |
| `camera_depth` | `60.0` | how far behind a chased player the camera sits. Positive floats only |
| `camera_pitch` | `15.0` | the camera's up/down angle, `0.0` to `45.0` |
| `qualifier_numspots` | `0` | how many ranks qualify — a player at or above this rank gets a `*` on the scoreboard. `1` is top fragger only, `0` is plain deathmatch |
| `qualifier_forceskins` | `0` | force one skin on everybody. Only meaningful under `dmpro`, the donor's Qualifier DM |
| `qualifier_skinname` | `male/grunt` | which |
| `motd_file` | `motd.txt` | message of the day. **At most 9 rows**, which is the donor's own limit |
| `motd_center` | `0` | centre-print it rather than console-print it |

### Bots, and physics

`bots_minplayers` and `bots_botfile` are R-OSP-11's per-ruleset pair and are
documented in the bot-layer section; the four below are tourney's own.

| cvar | default | what it does |
|---|---|---|
| `bots_autoload` | `0` | keep the roster topped up. **Shared** — the bot layer registers it too, same default |
| `bots_delayload` | `0` | wait before adding the first bot |
| `bots_warmuptime` | `0` | seconds before a bot readies up under `dmpro`, `tdm` or `duel`; `0` means it readies immediately |
| `bots_noclients` | `0` | `0` bots leave when no real client is connected, `1` they stay |
| `damage_railgun` | `100` | railgun damage, the one weapon OSP made tunable |
| `sv_airaccelerate` | `0` | the engine's, re-obtained so a config setting it here still works |

### Five that are registered and that nothing reads

Found while writing this section, by asking of every one of tourney's 163 cvar
globals whether anything in the tree ever reads it. **These five are registered,
appear in the donor's own configuration file, accept a value, and change
nothing.** They are listed here rather than quietly omitted, because a document
that described `numgibs` as "gibs per death" would be worse than one that did not
mention it: an operator would set it and believe it.

| cvar | default | what it should do | what actually decides | and upstream? |
|---|---|---|---|---|
| `numgibs` | `4` | gibs per death | every `ThrowGib` count in the tree is a literal | registered, unread |
| `nextlevel_default` | `45.0` | intermission seconds | nothing. Only `map_halt` touches it, and only to write `0` | registered, unread |
| `nextlevel_click` | `15.0` | intermission seconds after a click | nothing, same | registered, unread |
| `match_endmusic` | `default` | CD track at match end | nothing; there is no CD-music path in this tree | registered, unread |
| `sl_log_style` | `0` | which Standard Log dialect | nothing — and the donor documents `0` as the *only available option*, so nothing is actually missing but the read | one legal value |

**Each was checked against `osp-tourney` before being left alone**, and that is
the whole difference between this table and the three cvars below it: upstream
registers these five and reads them nowhere either, so there is no dropped read
to restore and fixing one would be inventing a feature rather than finishing an
import.

### Three that were on that list until upstream was checked

**`respawn_delay`, `power_armor_screen` and `power_armor_shield` are live in
`osp-tourney`** — `p_client.c:2625` adds the first to the forced-respawn test,
and `g_combat.c:202`/`:212` assign the other two to `damagePerCell`. So these
were never dead cvars; they were **reads this tree's import dropped**, which is a
different defect with a different fix. R-185 restores all three.

| cvar | default | what it does now | upstream site |
|---|---|---|---|
| `respawn_delay` | `0` | seconds a forced respawn waits beyond the button test, when `DF_FORCE_RESPAWN` is set | `p_client.c:2625` |
| `power_armor_screen` | `1.0` | damage absorbed per cell by the power screen | `g_combat.c:202` |
| `power_armor_shield` | `2.0` | the same for the power shield | `g_combat.c:212` |

All three answer **only under `dm`**, because upstream gates them on `!m_mode`
and `m_mode` 0 is RegularDM — a tournament cannot be re-balanced by a cvar
mid-series. And with the defaults **none of them changes anything**: `0` frames
makes the respawn test identical to the one above it, and `1.0`/`2.0` are exactly
the constants that were hardcoded. That is why nobody noticed the reads were
gone.

The clamp on the two ratios is upstream's and is a **reset, not a clamp**: a
value above `2` does not become `2`, it becomes the default. Kept as the donor
wrote it.

`respawn_delay` also carries a **deliberate divergence**. Upstream writes
`client->respawn_framenum + resp_delay->value` — a frame count plus a value its
own documentation calls "a delay (in seconds)" — so at 10 fps the cvar is off by
`BASE_FRAMERATE` across its whole documented range. This tree scales it, so the
seconds are seconds. See R-185 and `tools/units.py`.

### Four handles whose name is not the cvar's

Worth knowing before grepping: four of tourney's globals are named differently
from the cvar they hold, so searching for the cvar name finds only the
registration line and searching for the handle finds no cvar at all.

| the global | the cvar it registers |
|---|---|
| `match_features` | `match_info` |
| `nextlevel_lazy` | `nextlevel_default` |
| `osp_game` | `gamename` |
| `resp_delay` | `respawn_delay` |

`team_a_score` / `team_b_score` are a fifth case of a different kind: the handles
really are unread, but the cvars are not dead — `Score_A` and `Score_B` are
written by name with `gi.cvar_set` and published through `CVAR_SERVERINFO`, which
is the whole of what they are for.

### The nine names tourney shares with the rest of the tree

None is a collision: each is either an engine cvar re-obtained for a handle, or
one name two layers legitimately read with the same default.

| name | the other claimant | why it is not R-COMPAT-6 |
|---|---|---|
| `hostname`, `port`, `gamename`, `dmflags` | the engine | re-obtained. The engine owns the flags; a second `gi.cvar` returns the same cvar |
| `rcon_password` | `osp_cmds.c` and the bot menu | one name, one default, two readers |
| `flood_msgs`, `flood_persecond`, `flood_waitdelay` | baseq2 | same three names, same defaults, and OSP's flood control replaced baseq2's in the OSP rulesets under §7 rule 6 |
| `bots_autoload` | the bot layer | same default; the re-obtain is idiomatic |

`counts.py --duplicates` reports **zero** names registered from two translation
units with different defaults, and it now runs in `make check` — which it did not
before R-184, so every earlier statement of that fact rested on a script somebody
had run by hand once.

## The bot layer — added in spec 1.22 (Phase 6)

Every cvar the Gladiator SDK's `bl_*.c` names, measured against
`osp-tourney@1d8427e`'s six files: **28**, all reachable here. Eighteen are
literals at a `gi.cvar` call, eight arrive through `BotSetVarIfSet`'s name
parameter, and two — `minimumplayers` and `botfile` — through the per-ruleset
accessor below. `python3 tools/counts.py --tree ../osp-tourney --duplicates` is
the tool that says so; nothing in the donor's set is absent.

| cvar | default | meaning |
|---|---|---|
| `bots` | `1` | **new in 1.22.** R-88's decision (reconciliation R-96): the operator's switch for the whole layer, latched, refused under `sp`. `G_BotsAllowed()` reads it, so there is one question asked by name. `bots 0` on a ruleset that accepts bots makes `addbot`, `addrandom`, `SP_bot` and `CheckMinimumPlayers` all refuse |
| `botlib` | `gladiator.so` / `gladiator.dll` | the brain to `dlopen`. The 1999 defaults were `gladiator.dll` and `gladi386.so`; R-BOT-4 says the 1999 binaries are not a target, so the name follows what `gladiator-bot-restored` actually produces. A value containing a path separator is used as given; a bare name is resolved under `basedir`/`gamedir`, because `dlopen` needs an OS path and the engine's filesystem is not one (reconciliation R-95) |
| `serveronlybotcmds` | **`1`** | R-BOT-25, and a deliberate policy change from the 1999 default of 0: that default exposed `bl_spawn.c`'s bot-name copies to any client. Also what gates the bot menu behind `rcon_password` (R-BOT-28) — with one exemption, the host of a listen server, and only for the menu: every other bot command still refuses them, because `BotCmdRefused` asks whether the caller is the console and a host player is not |
| `freebotlib` | `1` | unload the library when its last user goes |
| `minimumplayers` / `bots_minplayers` | `0` / `4` | R-OSP-11: the NAME is per ruleset and so is the default, and the pair is obtained **once** through `BotMinPlayers()` — `osp_main.c` registers `bots_minplayers` with `"4"` and a second registration with `"0"` would be R-COMPAT-6's fifth instance |
| `botfile` / `bots_botfile` | `botcfg/bots.cfg` | ditto, through `BotFile()`. Read through the engine's filesystem, so a `bots.cfg` inside a `.pak` is found (R-BOT-26) |
| `bots_autoload` | `0` | tourney's own; `4` is the arm that keeps the roster topped up regardless of the player count |
| `botctfteam` | `0` | 0 auto-assign, 1 red, 2 blue. Re-registered here because both donors' ports dropped it (R-BOT-28) |
| `botfill` | `0` | **new in 1.34 (arena) and 1.35 (ctf, dm) as three cvars; ONE since 1.36.** It was `ra_botfill` / `ctf_botfill` / `dm_botfill`, named per ruleset on R-OSP-11's authority — which never covered them: that rule governs cvars a **donor** named, and all three are Colosseum's own and appear in no donor's documentation. §7 rule 6 governs instead, and the logic was already shared. `1` replaces the flat count as the target with a number read off the GAME, and the TARGET is still per ruleset: under `arena` the arena's `playersperteam` or, for a pickup arena, its even spawn count (R-RA-7); under `ctf` twice the smaller of a base and half the shared spawn pool (R-CTF-8); under `dm` and `dmpro` the shared spawn pool, rounded down to even under teamplay (R-DM-1); under `tdm` and `duel` `2 * team_maxplayers`, which is the capacity those two declare and what `doc/reconciliation.md` R-156 deferred until the flattening forced it. `sp` has no bots. The ceilings are `game.maxclients`, LATCHED at 4 by default, and the roster in `bots.cfg` — one copy of them now, in `BotFillTarget()`; `arena.c` carried a second until 1.36. `sv ruleset` prints one line naming the ruleset, the target and where it came from, because the number is computed rather than stored (R-VER-19) |
| `ra_playercycle` | `1` | **behaviour as of 1.23**, not just a menu row. Gates RA2's own "the winning team goes to the front of the waiting queue"; `0` rotates the arena strictly by arrival (R-RA-5) |
| `arena` | `1` | 1999's, and which arena a new BOT joins — `gladq2_src/bl_spawn.c` copies it into the bot's userinfo and `RA_BotJoinArena` reads it there. `1..N` is that arena, exactly as 1999 meant it. **`0` is ours as of 1.28** and means "follow the people": the lowest-numbered arena with a human on a team, then the lowest-numbered pickup arena, then 1 — resolved at join time, so a bot added before the first person still ends up where that person went. It matters on a real RA2 map, where the pickup arena is wherever `arena.cfg` puts it and a fixed 1 puts every bot where nobody is; D6's `configs/arena.cfg` ships `0` and asked for a non-existent `botarena` until 1.28 (R-131) |
| `ra_botcycle` | `1` | **behaviour as of 1.23.** When filling an arena, the **first** side is taken from the first waiting team that has a person on it, so no bot hogs the arena while somebody waits. First side only, which is `RA2_GetLongestWaitingHuman`'s shape; a server of only bots is unaffected (R-RA-5). Both are registered by `arena_init()` and obtained by the bot menu with the same default |
| `log` | **`0`** | the brain's own trace file. R-BOT-30's second deliberate policy change, from the SDK's `1`: a public server should not write an AI trace by default |
| `nochat`, `fastchat`, `altnames` | unset | pushed as libvars only when set, which is the SDK's own shape. **`fastchat` pushes `"1"`** — corrected in 1.22 (R-102). The donor pushed `"0"` with a comment rationalising it as turning the answer *delay* off, and the brain does not read it that way: it tests `fastchat == 0` and only *then* applies the random gate that decides whether a bot says anything at all, so pushing `"0"` left `fastchat 1` unable to do the one thing it is named for. This row said otherwise until 1.27, four amendments after the code changed |
| `rocketjump` | `1` | pushed as a libvar |
| `max_aaslinks`, `max_bsplinks`, `max_levelitems`, `framereachability` | unset / `20` | BSPC sizing, pushed only when positive |
| `autolaunchbspc`, `forceclustering`, `forcereachability`, `forcewrite`, `nooptimize` | unset | AAS compilation switches. `autolaunchbspc` is R-SEC-7's one exception and **cannot fire against the reconstructed brain**: the branch is `#ifdef _WIN32`, it spawns a `winbspc.exe` that must be in the gamedir, and `SpawnProcess` there is a stub returning -1 (R-126). The other four act on an `.aas` that already exists — they are the second half of making one, not a way to get one |
| `basedir`, `gamedir`, `cddir` | engine's | R-BOT-8. `basedir` falls back to `fs_basedir` and then `.`, `gamedir` to `game` and then `colosseum`, because under Q2PRO `gamedir` is `CVAR_ROM|CVAR_SERVERINFO` and `basedir` may not exist at all. Reported once at load |
| `sv_fps` | engine's | read, not set: R-BOT-22 logs one warning at `InitGame` when it is not 10, because `FRAMETIME` is 0.1 s and Colosseum does not advertise `GMF_VARIABLE_FPS` |

`runes` gains a meaning under `ctf` and the OSP rulesets in 1.22 — see R-MODE-4 and
reconciliation R-96. It is still refused under `dm` and `arena`, with the same
message, and still defaults to 0, which means "do not ask" rather than "turn
them off".

## Phase 8: R-EXTRA's seven, as cvars

**326 cvars** total (`tools/counts.py`), and `counts.py --duplicates` reports
**zero** names registered from two translation units with different defaults —
`basedir`, `gamedir` and `rcon_password` were the last three and are settled in
`reconciliation.md` R-119.

R-EXTRA's rule is that each of the 1999 module's `#define`s is kept, becomes a
cvar and is never compiled out. Six of the seven are here; the seventh, VWep, is
not a Gladiator patch in this tree at all.

| cvar | default | what it does |
|---|---|---|
| `g_gamelog` | `""` | R-EXTRA-1. A **filename**, not a flag: the feature's whole content is which file it writes to, and "" is the natural off. Set it and `InitGame` opens `<homedir-or-basedir>/<gamedir>/<name>`; `sv openlog`/`closelog`/`writelog` still work and `Log_ShutDown` runs from `ShutdownGame` |
| `g_clientlag` | `0` | R-EXTRA-2. Off, and the reason is a threat rather than taste: `lag` is a **client** command, so a player could ask the server to hold two seconds of their own input, and with maxclients 256 that is a queue whose length the client sets. With it off nothing is allocated at all and both commands say so |
| `g_triggercounting` | `0`, `CVAR_LATCH` | R-EXTRA-3. `trigger_counting`, and `trigger_relay` carrying a count through. Latched because it decides what a map spawns |
| `g_triggerlog` | `0`, `CVAR_LATCH` | R-EXTRA-3. `trigger_log`, which writes a map's own message into the game log the first time a client touches it. Latched, same reason |
| `g_rotatingbutton` | `0`, `CVAR_LATCH` | R-EXTRA-4. `func_button_rotating`. Latched, same reason |
| `g_observer` | **`1`** | R-EXTRA-6, and the one extra that is **on**. It is the 1999 module's own observer and `observer` is what a 1999 config binds. Under `arena` and the four OSP rulesets it does not reach: those have their own observers and R-EXTRA-6 is the second exemption to §7 rule 6. It answers for `ctf` and `sp` |

**Off means the entity is not created**, for the three that gate a spawn
function — not created-and-inert. An inert trigger still occupies an edict slot
and still shows in every census. The **classnames stay registered either way**,
so a map that uses one loads without `doesn't have a spawn function`.

**`netlog` still resolves and no longer does anything.** It named the remote host
RA2 forwarded its event log to over UDP; R-SEC-7 does not allow that, so the
forwarding is gone (`reconciliation.md` R-114). The name stays registered so a
1999 config still parses (R-COMPAT-3), `InitGame` says once that setting it does
nothing, and `logfile 2` writes the same lines to the local log.
