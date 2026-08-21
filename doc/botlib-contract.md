# The game↔botlib contract

Deliverable D5. The authoritative copy of the ABI and the libvar contract;
SPECS.md Appendix A is a quick reference and this file wins where they differ.

**Nothing in this file is implemented yet.** The bot layer is Phase 6. This is
the frozen-by-agreement description the implementation will be checked against,
recorded now because R-BOT-1 makes the interface *versioned rather than frozen*
and a version needs a document to be a version of.

## Versioning (R-BOT-1)

`botlib.h` spans two repositories, so it stays stable by default and every change
is deliberate: announced through `BotVersion` — already slot 0 of
`bot_export_t`, so the handshake mechanism exists — and recorded here with the
version it landed in. **A change that both sides do not agree on is a defect, not
a version.** §7 rule 5: a reconciliation needing `botlib.h` to change is
*suspect*, and another way is preferred, but it is no longer automatically wrong
now that both sides compile from source.

| version | change | landed |
|---|---|---|
| — | initial contract, taken from `osp-tourney`'s already-ported `botlib.h` including its `const`-ification of the `PointContents` slot | Phase 6 |

## Entry point

```c
bot_export_t *GetBotAPI(bot_import_t *import);
```

Loaded dynamically per bot from `bots.cfg` (R-BOT-3), with `BotUseLibrary`'s
search order — direct path, then `basedir` + `gamedir` — reference counted per
library, and `BotUnloadAllLibraries` on `ShutdownGame`.

## `bot_export_t` — game → botlib, 20 slots, in order

`BotVersion`, `BotSetupLibrary`, `BotShutdownLibrary`, `BotLibraryInitialized`,
`BotLibVarSet`, `BotDefine`, `BotLoadMap`, `BotSetupClient`,
`BotShutdownClient`, `BotMoveClient`, `BotClientSettings`, `BotSettings`,
`BotStartFrame`, `BotUpdateClient`, `BotUpdateEntity`, `BotAddSound`,
`BotAddPointLight`, `BotAI`, `BotConsoleMessage`, `Test`.

## `bot_import_t` — botlib → game, 10 slots, in order

`BotInput`, `BotClientCommand`, `Print`, `Trace`, `PointContents`, `GetMemory`,
`FreeMemory`, `DebugLineCreate`, `DebugLineDelete`, `DebugLineShow`.

## Two ABI hazards (R-BOT-4, R-BOT-5)

1. **`bsp_trace_t` is returned by value** from the `Trace` slot, so both sides
   must agree on the struct-return convention. On Windows the slot takes the same
   `q_gameabi` treatment Q2PRO applies to `gi.trace`. `BotLibImport_Trace` stays
   a translating wrapper (`trace_t` → `bsp_trace_t`, `edict_t *` → entity number,
   `passent` bounds-checked against `game.maxentities`) and must tolerate a null
   `trace.surface`. Risk 6 mitigates with a `Test()` round-trip at load.
2. **Word size must match.** Both tables are pointer-bearing, so game and botlib
   must share bitness; `BotUseLibrary` reports a load failure with the reason and
   the bitness of both sides, and refuses the bot rather than killing the server.
   At 64 bits the botlib's `_Static_assert` layout guards are wrapped in
   `#if __SIZEOF_POINTER__ == 4` and go inert, so a 64-bit brain carries **no
   layout verification** (Risk 5a) — extend the guards to 64-bit offsets, or
   accept and record it, with the `Test()` round-trip as the fallback.

The 1999 binaries are not a target (R-BOT-4, amended in 1.2): the brain is
compiled from `gladiator-bot-restored/botlib` for whichever platform the game is
built for.

## Fixed limits

`MAX_NETNAME` 16, `MAX_CLIENTSKINNAME` 128, `MAX_FILEPATH` 144,
`MAX_CHARACTERNAME` 144. `BLERR_NOERROR` 0 … `BLERR_INVALIDSOUNDINDEX` 32.

Action flags: `ATTACK` 1, `USE` 2, `RESPAWN` 4, `JUMP`/`MOVEUP` 8,
`CROUCH`/`MOVEDOWN` 16, `MOVEFORWARD` 32, `MOVEBACK` 64, `MOVELEFT` 128,
`MOVERIGHT` 256, `DELAYEDJUMP` 512.

## Libvars (R-BOT-6, R-BOT-7)

`BotInitLibrary` pushes these through `BotLibVarSet` **before**
`BotSetupLibrary`, and the set is fixed — no new libvar may be invented, because
the brain would ignore it:

`maxclients`, `maxentities`, `max_aaslinks`, `max_bsplinks`, `max_levelitems`,
`autolaunchbspc`, `dmflags`, `ctf`, `ch`, `ra`, `xatrix`, `rogue`, `log`,
`nochat`, `fastchat`, `altnames`, `rocketjump`, `forceclustering`,
`forcereachability`, `forcewrite`, `nooptimize`, `framereachability`, `basedir`,
`gamedir`, `cddir`, `usehook`, `laserhook`, `runes`, `techs`, `teamplay`,
`teamplay_shell`, `assimilation`.

`ch` is pushed as a constant `"0"`: Colored Hitman is out of scope (N7), but the
brain reads the libvar and sending zero costs nothing.

### Ruleset → libvar mapping

This table is the single authority for it (R-BOT-7). To be filled in Phase 6/7;
the shape is fixed now:

| ruleset | libvars set |
|---|---|
| `dm` | baseline only |
| `ctf` | `ctf 1`, `techs`, `usehook`/`laserhook` per `ctf_hook` |
| `arena` | `ra 1` |
| `tourney` | `teamplay` from `m_mode == MODE_TEAM` **only** — see below; `usehook`/`laserhook` from `hook_enable`; `runes` from `rune_stat` |
| `sp` | none; bots do not run in single player (N6) |

**The `m_mode` comparison is preserved exactly as written** (R-BOT-29).
`bl_main.c:1035` tests `m_mode == MODE_TEAM` (`0x02`) to drive `teamplay`, and
that is correct for all four modes: 1v1 (`m_mode 3`) is two teams of one, the
brain has no ally, and `teamplay 0` is the right answer. An accessor that
generalised the test to "is this a team mode" would set `teamplay 1` in 1v1 and
give every duel bot an imaginary teammate. Abstract only where the value comes
from, never the comparison.

## Physics libvars the brain expects

Defaults from `botlib.h`: `sv_friction` 6, `sv_stopspeed` 100, `sv_gravity` 800,
`sv_waterfriction` 1, `sv_watergravity` 400, `sv_maxvelocity` 300,
`sv_maxwalkvelocity` 300, `sv_maxcrouchvelocity` 100, `sv_maxswimvelocity` 150,
`sv_maxacceleration` 2200, `sv_airaccelerate` 0, `sv_maxstep` 18,
`sv_maxbarrier` 50, `sv_maxsteepness` 0.7, `sv_jumpvel` 224,
`sv_maxwaterjump` 20.
