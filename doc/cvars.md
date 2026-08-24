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
| `ra_playercycle` | `1` | **behaviour as of 1.23**, not just a menu row. Gates RA2's own "the winning team goes to the front of the waiting queue"; `0` rotates the arena strictly by arrival (R-RA-5) |
| `ra_botcycle` | `1` | **behaviour as of 1.23.** When filling an arena, the **first** side is taken from the first waiting team that has a person on it, so no bot hogs the arena while somebody waits. First side only, which is `RA2_GetLongestWaitingHuman`'s shape; a server of only bots is unaffected (R-RA-5). Both are registered by `arena_init()` and obtained by the bot menu with the same default |
| `log` | **`0`** | the brain's own trace file. R-BOT-30's second deliberate policy change, from the SDK's `1`: a public server should not write an AI trace by default |
| `nochat`, `fastchat`, `altnames` | unset | pushed as libvars only when set, which is the SDK's own shape — `fastchat` pushes `"0"`, and that is not a typo: the switch turns the brain's answer *delay* off |
| `rocketjump` | `1` | pushed as a libvar |
| `max_aaslinks`, `max_bsplinks`, `max_levelitems`, `framereachability` | unset / `20` | BSPC sizing, pushed only when positive |
| `autolaunchbspc`, `forceclustering`, `forcereachability`, `forcewrite`, `nooptimize` | unset | AAS compilation switches |
| `basedir`, `gamedir`, `cddir` | engine's | R-BOT-8. `basedir` falls back to `fs_basedir` and then `.`, `gamedir` to `game` and then `colosseum`, because under Q2PRO `gamedir` is `CVAR_ROM|CVAR_SERVERINFO` and `basedir` may not exist at all. Reported once at load |
| `sv_fps` | engine's | read, not set: R-BOT-22 logs one warning at `InitGame` when it is not 10, because `FRAMETIME` is 0.1 s and Colosseum does not advertise `GMF_VARIABLE_FPS` |

`runes` gains a meaning under `ctf` and `tourney` in 1.22 — see R-MODE-4 and
reconciliation R-96. It is still refused under `dm` and `arena`, with the same
message, and still defaults to 0, which means "do not ask" rather than "turn
them off".
