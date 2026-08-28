# Cvar inventory

Deliverable D4. The merged, deduplicated cvar inventory: every cvar Colosseum
registers, its default, its flags, the donor that introduced it, and — where
§7 rule 4 applied — which ruleset owns the bare name and what the prefixed alias
is (R-COMPAT-4, R-COMPAT-6).

Generated from `tools/counts.py --list cvars` and annotated. Regenerate rather
than hand-edit the counts.

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
| `g_ruleset` | `CVAR_LATCH \| CVAR_SERVERINFO` | the primary ruleset: `dm` \| `ctf` \| `arena` \| `tourney` \| `sp`. Empty means "infer": legacy aliases first, then `coop`, then `dm`. An invalid value warns and falls back to `dm` (R-MODE-1) |
| `xatrix`, `rogue` | `CVAR_LATCH` | independent content layers, valid with every ruleset, both may be on (R-MODE-3) |
| `teamplay`, `hook`, `runes` | `CVAR_LATCH` | modifiers, refused with a message by a ruleset that does not accept them (R-MODE-4) |

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
| `minimumplayers` vs `bots_minplayers` | Gladiator vs tourney | both registered, authority is per ruleset (R-OSP-11) |
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
| `serveronlybotcmds` | **`1`** | R-BOT-25, and a deliberate policy change from the 1999 default of 0: that default exposed `bl_spawn.c`'s bot-name copies to any client. Also what gates the bot menu behind `rcon_password` (R-BOT-28) |
| `freebotlib` | `1` | unload the library when its last user goes |
| `minimumplayers` / `bots_minplayers` | `0` / `4` | R-OSP-11: the NAME is per ruleset and so is the default, and the pair is obtained **once** through `BotMinPlayers()` — `osp_main.c` registers `bots_minplayers` with `"4"` and a second registration with `"0"` would be R-COMPAT-6's fifth instance |
| `botfile` / `bots_botfile` | `botcfg/bots.cfg` | ditto, through `BotFile()`. Read through the engine's filesystem, so a `bots.cfg` inside a `.pak` is found (R-BOT-26) |
| `bots_autoload` | `0` | tourney's own; `4` is the arm that keeps the roster topped up regardless of the player count |
| `botctfteam` | `0` | 0 auto-assign, 1 red, 2 blue. Re-registered here because both donors' ports dropped it (R-BOT-28) |
| `ra_botfill` / `ctf_botfill` / `dm_botfill` | `0` | **new in 1.34 (arena) and 1.35 (ctf, dm).** One concept, one switch per ruleset, obtained through `BotFillCvar()` for R-OSP-11's reason — each ruleset's own configs and readme spell its cvars its own way. `1` replaces `minimumplayers` as the target with a number read off the GAME: under `arena` the arena's `playersperteam` or, for a pickup arena, its even spawn count (R-RA-7); under `ctf` twice the smaller of a base and half the shared spawn pool (R-CTF-8); under `dm` the shared spawn pool, rounded down to even under `teamplay` (R-DM-1). `tourney` has no such switch and needs none — `team_maxplayers` is a declared capacity — and `sp` has no bots (see `BotFillCvar()`). The ceilings are `game.maxclients`, which is LATCHED at 4 by default, and the roster in `bots.cfg`; `sv ruleset` prints the target in force because it is computed rather than stored (R-VER-19) |
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

`runes` gains a meaning under `ctf` and `tourney` in 1.22 — see R-MODE-4 and
reconciliation R-96. It is still refused under `dm` and `arena`, with the same
message, and still defaults to 0, which means "do not ask" rather than "turn
them off".

## Phase 8: R-EXTRA's seven, as cvars

**313 cvars** total (`tools/counts.py`), and `counts.py --duplicates` reports
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
| `g_observer` | **`1`** | R-EXTRA-6, and the one extra that is **on**. It is the 1999 module's own observer and `observer` is what a 1999 config binds. Under `arena` and `tourney` it does not reach: those rulesets have their own observers and R-EXTRA-6 is the second exemption to §7 rule 6 |

**Off means the entity is not created**, for the three that gate a spawn
function — not created-and-inert. An inert trigger still occupies an edict slot
and still shows in every census. The **classnames stay registered either way**,
so a map that uses one loads without `doesn't have a spawn function`.

**`netlog` still resolves and no longer does anything.** It named the remote host
RA2 forwarded its event log to over UDP; R-SEC-7 does not allow that, so the
forwarding is gone (`reconciliation.md` R-114). The name stays registered so a
1999 config still parses (R-COMPAT-3), `InitGame` says once that setting it does
nothing, and `logfile 2` writes the same lines to the local log.
