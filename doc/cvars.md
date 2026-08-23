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
