# The game↔botlib contract

Deliverable D5. The authoritative copy of the ABI and the libvar contract;
SPECS.md Appendix A is a quick reference and this file wins where they differ.

**Implemented in Phase 6, spec 1.22.** `src/bot/` is the game side and
`gladiator-bot-restored/botlib` is the brain; `tools/botabi.py` compares the two
headers in `make check` (R-VER-28), because the two mitigations this file lists
below cannot see the one thing that went wrong.

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
| 1 | initial contract, taken from `osp-tourney`'s already-ported `botlib.h` including its `const`-ification of the `PointContents` slot | Phase 6 |
| **3** | **the `Trace` slot's 64-bit spelling is WITHDRAWN — by value on every target, which is what the published contract always said.** Upstream removed all three branches (`gladiator-bot-restored 57ce85a3`) and the reason is better than version 2's match: the branch was only sound while both sides were the same project's, because `game/botlib.h` — the header a foreign engine builds against — was never branched. By value is also the faithful 1999 spelling and the only form that reproduces `gladi386.so`'s trace thunk. `tools/botabi.py` reported the divergence on the first run after the update, which is what R-VER-28 exists for | Phase 6, spec 1.22 |
| **2** | *superseded by 3 after eleven hours.* **the `Trace` slot gains its 64-bit spelling.** `bsp_trace_t` is 88 bytes, so the caller always passes a hidden return buffer; on 32-bit that buffer IS the first visible argument and the two spellings are the same ABI, and on x86-64 and aarch64 it is a hidden register (`rax` / `x8`) and they are **different** ABIs. Version 1 carried only the 32-bit form, because `osp-tourney` — the port R-BOT-1 says to take it from — is a 32-bit port. Both sides now select with `#if defined(__x86_64__) \|\| defined(__aarch64__)`, and the condition is identical on both sides *by check*, not by intent | Phase 6, spec 1.22 |

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

`Trace` has **one** spelling, on every target — see versions 2 and 3 above for
the eleven hours in which it had two:

```c
    bsp_trace_t (*Trace)(vec3_t start, vec3_t mins, vec3_t maxs,
                         vec3_t end, int passent, int contentmask);
```

No `q_gameabi`, and that is a decision rather than an omission. R-BOT-5 asks for
"the same treatment Q2PRO applies to `gi.trace`", and that treatment is
`callee_pop_aggregate_return(0)`, which this build does not enable
(`config.h` sets `USE_GAME_ABI_HACK 0`). The brain's side carries no attribute at
all, so if this build ever turns the hack on, **this slot must still not get
it** — the two sides have to agree and the contract's spelling is the plain one.

Four slots differ in **return type** between the two headers and are compatible
anyway, because every caller ignores the result: the brain declares `Print`,
`BotClientCommand`, `DebugLineDelete` and `DebugLineShow` as returning `int`
where the 1999 game header says `void`. `botabi.py` compares arguments and not
return types for exactly that reason. `bot_input_t` and the brain's
`ea_state_t` are the same thirty-six bytes member for member; the names differ
because the brain's are offset-derived.

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

**Both mitigations failed the first time they were needed, and the third time as
well.** `BotVersion` is slot 0 so the two sides can shake hands, and it
returns `"BotLib v0.96"` on both sides of an ABI split — the version is the
brain's, not the convention's. `Test(int, char *, vec3_t, vec3_t)` passes no
struct by value, so the round trip exercises none of hazard 1. A handshake that
cannot fail is not a handshake. What found version 2's defect was a headless
dedicated server, three bots and a minute of watching nothing happen; what would
have found it earlier is `tools/botabi.py`, which compares the two headers and
ships R-97 itself as one of its four positive controls.

The 1999 binaries are not a target (R-BOT-4, amended in 1.2): the brain is
compiled from `gladiator-bot-restored/botlib` for whichever platform the game is
built for.

## Fixed limits

`MAX_NETNAME` 16, `MAX_CLIENTSKINNAME` 128, `MAX_FILEPATH` 144,
`MAX_CHARACTERNAME` 144. `BLERR_NOERROR` 0 … `BLERR_INVALIDSOUNDINDEX` 32.

**And two that were missing from this list, added in spec 1.22:**
`BOTLIB_MAX_STATS` **32** and `BOTLIB_MAX_ITEMS` **256**. The 1999 header spells
them with the *engine's* names, `MAX_STATS` and `MAX_ITEMS`, which was safe when
both sides included id's `q_shared.h` and is not safe now: Q2PRO's `MAX_STATS`
is **64** under `USE_NEW_GAME_API`, so `bot_updateclient_t` was 1292 bytes here
against the brain's 1228 and the brain's own `memcpy` read the inventory sixteen
slots out of place. A contract array whose bound follows one side's engine is not
a contract. `doc/reconciliation.md` R-100.

## Item indices — the DATA half of the contract (R-141, 1.31)

The two halves above are about bytes: how big a struct is and where its members
sit. `bot_updateclient_t.inventory` passes both and was still wrong, because a
256-int array agreed on at both ends says nothing about what any one of its
slots MEANS — and the brain has a very definite opinion. Its numbering is
`inv.h` out of the 1999 asset pak, where **slot 10 is the Machinegun and slot 19
is Bullets**, and three separate data files are written against it:

| botfile | field | what it indexes |
|---|---|---|
| `weapons.c` | `weaponindex`, `ammoindex` | "do I own this weapon, and do I have ammo for it" |
| `items.c` | `index` | which slot a pickup in the world fills |
| `bots/<char>_w.c`, `_i.c` | every `switch(INVENTORY_*)` | the fuzzy weight of a weapon or a goal |

So the numbering is not the brain's private business. It is contract carried as
data, and the game has to speak it.

`inv.h` is baseq2's itemlist with Gladiator's additions appended. Colosseum's
itemlist is five donors merged into one array (R-CORE-2): it agrees for the
first six rows and diverges at the seventh, where `weapon_grapple` sits and the
Blaster used to. Everything after is off by one or more, so a straight memcpy
handed the brain an inventory reading "one bullet, no rockets, no cells" to a
fighter carrying 200, 50 and 150 — and `fw_weap.c` zeroes the weight of any
weapon whose ammo test fails. Of a nine-weapon RA2 loadout the brain could see
six, and the Railgun only by accident: slot 16 lands on
`weapon_grenadelauncher`, so an arena that grants the GL grants the brain its
Railgun too. Which of the six a bot then settles on is an accident of that
accident, which is why "every bot uses the same one or two weapons" is the
symptom and the character file is not what is talking.

`BotFillInventory()` in `src/bot/bl_main.c` translates, one named slot at a
time, resolved through `FindItemByClassname()` rather than written out as
numbers: the game's numbering is the side that moves. Two ranges are
deliberately not written — `INVENTORY_HEALTH` (41), which the brain fills from
`stats[STAT_HEALTH]`, and everything from `ENEMY_HORIZONTAL_DIST` (200) up,
which `BotUpdateInventory`/`BotUpdateBattleInventory` derive after the copy
lands. `sv botinv` prints the brain's slots and the client's own inventory on
adjacent lines, which is the only instrument that can see this: `sv inventory`
prints the game's itemlist, and the game's itemlist was never wrong.

## Struct sizes — the layout half of the contract

Every struct here crosses the library boundary, so both sides must agree on its
size and on every offset in it. None of them contains a pointer, which is what
makes a number meaningful at 32 and 64 bits alike:

| struct | bytes |
|---|---|
| `bsp_surface_t` | 24 |
| `bsp_trace_t` | 84 |
| `bot_settings_t` | 432 |
| `bot_clientsettings_t` | 144 |
| `bot_input_t` (= the brain's `ea_state_t`) | 36 |
| `bot_updateclient_t` | 1228 |
| `bot_updateentity_t` | 104 |

`src/bot/botlib.h` asserts each of them, plus `q_offsetof` on `bsp_trace_t`'s
`fraction` (8) and `endpos` (12) — a size cannot see a transposition — and the
two host-side definitions the sizes turn on: `sizeof(qboolean) == 4` and
`BOTLIB_MAX_STATS == 32`. `tools/botabi.py` (R-VER-28) does not take any of it
on trust: it compiles a probe against
`gladiator-bot-restored/game/{q_shared,botlib}.h` and compares what the brain's
compiler measures.

Two of the seven were wrong before that existed, and both were a type NAME rather
than a number: `qboolean` substituted for `bool` in `osp-tourney`'s copy of the
published header (one byte instead of four, twice, in `bsp_trace_t`), and
`MAX_STATS`, which the 1999 header spells with the engine's name. Upstream
asserts the same four facts on its own side as of `57ce85a3`, so a port that
redefines either now fails to build at both ends rather than one.

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

This table is the single authority for it (R-BOT-7). *Filled in Phase 6, spec
1.22.* Every one of the 32 names is pushed on **every**
ruleset, so the brain always sees the same set and only the values move — a
libvar that is set on one ruleset and absent on another is a libvar whose
default the brain would silently use, which is R-BOT-7's whole concern.
`BotInitLibrary` in `src/bot/bl_main.c` is the single place this happens.

| ruleset | libvars that differ from the baseline |
|---|---|
| `dm` | `usehook` from the `hook` modifier; everything else 0 |
| `ctf` | `ctf 1`, `teamplay 1`, `usehook` from `ctf_hook`, `laserhook` from `laserhook`, `techs` from `!(dmflags & DF_CTF_NO_TECH)` |
| `arena` | `ra 1`; `usehook` from the `hook` modifier |
| `tourney` | `usehook`/`laserhook` both from `hook_enable`; `teamplay` from `m_mode == MODE_TEAM` **only** — see below; `runes` from `rune_stat` |
| `sp` | unreachable: `G_BotsAllowed()` is false, so no library is ever loaded (N6) |

`ch` is a constant `"0"` (N7). `xatrix` and `rogue` follow the content layers,
not the ruleset. `assimilation` and `teamplay_shell` are pushed as `"0"`: they
are in R-BOT-6's fixed set, no ruleset here sets them, and omitting them would
leave the brain on its own defaults for two names the set names.

`techs` is the interesting row. The brain reads it as `LibVar("runes", "0")` —
one libvar under two names — and CTF's techs are gated by `DF_CTF_NO_TECH`
alone, never by the `runes` modifier, so the dmflag is what the brain has to be
told about. That is the same fact R-88 recorded as a finding and R-96 turned
into the modifier's definition.

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
