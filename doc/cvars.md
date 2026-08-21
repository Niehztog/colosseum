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
