# The game<->botlib contract

The authoritative copy of the ABI and the libvar contract; SPECS.md Appendix A is a quick reference and this file wins where they differ.

**Implemented.** `src/bot/` is the game side and `gladiator-bot-restored/botlib` builds the botlib -- `gladiator.so`/`gladiator.dll`, the shared library that holds the bot AI; `tools/botabi.py` compares the two headers in `make check`, because the two mitigations this file lists below cannot see the one thing that went wrong.

## Versioning

`botlib.h` spans two repositories, so it stays stable by default and every change is deliberate: announced through `BotVersion` -- already slot 0 of `bot_export_t`, so the handshake mechanism exists -- and recorded here with the version it landed in. **A change that both sides do not agree on is a defect, not a version.** section 7 rule 5: a reconciliation needing `botlib.h` to change is *suspect*, and another way is preferred, but it is no longer automatically wrong now that both sides compile from source.

| version | change |
|---|---|
| 1 | initial contract, taken from `osp-tourney`'s already-ported `botlib.h` including its `const`-ification of the `PointContents` slot |
| **2** | *superseded by 3.* **The `Trace` slot gains its 64-bit spelling.** `bsp_trace_t` is 84 bytes -- the size table below, and `tools/botabi.py`'s compiled probe -- so the caller always passes a hidden return buffer; on 32-bit that buffer IS the first visible argument and the two spellings are the same ABI, and on x86-64 and aarch64 it is a hidden register (`rax` / `x8`) and they are **different** ABIs. Version 1 carried only the 32-bit form, because `osp-tourney` -- the port the contract says to take it from -- is a 32-bit port. Both sides now select with `#if defined(__x86_64__) \|\| defined(__aarch64__)`, and the condition is identical on both sides *by check*, not by intent |
| **3** | **The `Trace` slot's 64-bit spelling is WITHDRAWN -- by value on every target, which is what the published contract always said.** Upstream removed all three branches (`gladiator-bot-restored 57ce85a3`) and the reason is better than version 2's match: the branch was only sound while both sides were the same project's, because `game/botlib.h` -- the header a foreign engine builds against -- was never branched. By value is also the faithful 1999 spelling and the only form that reproduces `gladi386.so`'s trace thunk. `tools/botabi.py` reported the divergence on the first run after the update, which is what the ABI check exists for |

## Entry point

```c
bot_export_t *GetBotAPI(bot_import_t *import);
```

Loaded dynamically -- `bots.cfg` has no library field -- from the file of the botlib a bot needs (see [Two botlibs](#two-botlibs)): `gladiator.so`/`.dll` or `q3bot.so`/`.dll`, unless the `botlib` cvar names a file that answers as that botlib. The search order is `BotLibrarySearch`'s: a value containing a path separator as given; a bare name under `homedir` + `gamedir`, then `libdir` + `gamedir`, the first that holds the file, else `basedir` + `gamedir`. One library **per botlib**, reference counted, and `BotUnloadAllLibraries` on `ShutdownGame`.

`GetBotAPI` is a plain C function on every target, 32-bit Windows included. The 1999 game source declares its pointer `WINAPI` (`__stdcall`), and the real `gladiator.dll` never answered to that -- its `GetBotAPI` ends in a plain `ret` -- so this game's pointer is plain C, and so is the Quake III botlib's export: a `__stdcall` one, called the way Gladiator's is, would leave the stack four bytes off on every return. `tools/botabi.py` checks both sides' spelling.

## `bot_export_t` -- game -> botlib, 20 slots, in order

`BotVersion`, `BotSetupLibrary`, `BotShutdownLibrary`, `BotLibraryInitialized`, `BotLibVarSet`, `BotDefine`, `BotLoadMap`, `BotSetupClient`, `BotShutdownClient`, `BotMoveClient`, `BotClientSettings`, `BotSettings`, `BotStartFrame`, `BotUpdateClient`, `BotUpdateEntity`, `BotAddSound`, `BotAddPointLight`, `BotAI`, `BotConsoleMessage`, `Test`.

## `bot_import_t` -- botlib -> game, 10 slots, in order

`BotInput`, `BotClientCommand`, `Print`, `Trace`, `PointContents`, `GetMemory`, `FreeMemory`, `DebugLineCreate`, `DebugLineDelete`, `DebugLineShow`.

`Trace` has **one** spelling, on every target -- see versions 2 and 3 above for the eleven hours in which it had two:

```c
    bsp_trace_t (*Trace)(vec3_t start, vec3_t mins, vec3_t maxs,
                         vec3_t end, int passent, int contentmask);
```

No `q_gameabi`, and that is a decision rather than an omission. The obvious move is "the same treatment Q2PRO applies to `gi.trace`", and that treatment is `callee_pop_aggregate_return(0)`, which this build does not enable (`config.h` sets `USE_GAME_ABI_HACK 0`). The botlib's side carries no attribute at all, so if this build ever turns the hack on, **this slot must still not get it** -- the two sides have to agree and the contract's spelling is the plain one.

Four slots differ in **return type** between the two headers and are compatible anyway, because every caller ignores the result: the botlib declares `Print`, `BotClientCommand`, `DebugLineDelete` and `DebugLineShow` as returning `int` where the 1999 game header says `void`. `botabi.py` compares arguments and not return types for exactly that reason. `bot_input_t` and the botlib's `ea_state_t` are the same thirty-six bytes member for member; the names differ because the botlib's are offset-derived.

## Two ABI hazards

1. **`bsp_trace_t` is returned by value** from the `Trace` slot, so both sides must agree on the struct-return convention. The slot takes **no** `q_gameabi` treatment, on Windows or anywhere: the brain's side carries none, this build sets `USE_GAME_ABI_HACK 0` so the attribute would expand to nothing anyway, and if a build ever turned it on this slot must still not get it -- the two sides have to agree, and the contract's spelling is the plain one (`src/bot/botlib.h`). `BotLibImport_Trace` stays a translating wrapper (`trace_t` -> `bsp_trace_t`, `edict_t *` -> entity number, `passent` bounds-checked against `game.maxentities`) and must tolerate a null `trace.surface`. Risk 6 mitigates with a `Test()` round-trip at load.
2. **Word size must match.** Both tables are pointer-bearing, so game and botlib must share bitness; `BotUseLibrary` reports a load failure with the loader's reason and the game's word size -- the reason names the brain's side: on ELF and Mach-O the loader's text gives its class, and on Windows a DLL of the other word size is error 193, `ERROR_BAD_EXE_FORMAT`, reported as "not a N-bit DLL" -- and refuses the bot rather than killing the server. At 64 bits the botlib's `_Static_assert` layout guards are wrapped in `#if __SIZEOF_POINTER__ == 4` and go inert, so a 64-bit botlib carries **no layout verification** (Risk 5a) -- extend the guards to 64-bit offsets, or accept and record it, with the `Test()` round-trip as the fallback.

**Both mitigations failed the first time they were needed, and the third time as well.** `BotVersion` is slot 0 so the two sides can shake hands, and it returns `"BotLib v0.96"` on both sides of an ABI split -- the version is the botlib's, not the convention's. `Test(int, char *, vec3_t, vec3_t)` passes no struct by value, so the round trip exercises none of hazard 1. A handshake that cannot fail is not a handshake. What found version 2's defect was a headless dedicated server, three bots and a minute of watching nothing happen; what would have found it earlier is `tools/botabi.py`, which compares the two headers and ships that defect itself as one of its four positive controls.

The 1999 binaries are not a target: the botlib is compiled from `gladiator-bot-restored/botlib` for whichever platform the game is built for.

## Fixed limits

`MAX_NETNAME` 16, `MAX_CLIENTSKINNAME` 128, `MAX_FILEPATH` 144, `MAX_CHARACTERNAME` 144. `BLERR_NOERROR` 0 ... `BLERR_INVALIDSOUNDINDEX` 32.

**And two that were missing from the 1999 list:** `BOTLIB_MAX_STATS` **32** and `BOTLIB_MAX_ITEMS` **256**. The 1999 header spells them with the *engine's* names, `MAX_STATS` and `MAX_ITEMS`, which was safe when both sides included id's `q_shared.h` and is not safe now: Q2PRO's `MAX_STATS` is **64** under `USE_NEW_GAME_API`, so `bot_updateclient_t` was 1292 bytes here against the botlib's 1228 and the botlib's own `memcpy` read the inventory sixteen slots out of place. A contract array whose bound follows one side's engine is not a contract.

## Item indices -- the DATA half of the contract

The two halves above are about bytes: how big a struct is and where its members sit. `bot_updateclient_t.inventory` passes both and was still wrong, because a 256-int array agreed on at both ends says nothing about what any one of its slots MEANS -- and the botlib has a very definite opinion. Its numbering is `inv.h` out of the 1999 asset pak, where **slot 10 is the Machinegun and slot 19 is Bullets**, and three separate data files are written against it:

| botfile | field | what it indexes |
|---|---|---|
| `weapons.c` | `weaponindex`, `ammoindex` | "do I own this weapon, and do I have ammo for it" |
| `items.c` | `index` | which slot a pickup in the world fills |
| `bots/<char>_w.c`, `_i.c` | every `switch(INVENTORY_*)` | the fuzzy weight of a weapon or a goal |

So the numbering is not the botlib's private business. It is contract carried as data, and the game has to speak it.

`inv.h` is baseq2's itemlist with Gladiator's additions appended. Colosseum's itemlist is five donors merged into one array: it agrees for the first six rows and diverges at the seventh, where `weapon_grapple` sits and the Blaster used to. Everything after is off by one or more, so a straight memcpy handed the botlib an inventory reading "one bullet, no rockets, no cells" to a fighter carrying 200, 50 and 150 -- and `fw_weap.c` zeroes the weight of any weapon whose ammo test fails. Of a nine-weapon RA2 loadout the botlib could see six, and the Railgun only by accident: slot 16 lands on `weapon_grenadelauncher`, so an arena that grants the GL grants the botlib its Railgun too. Which of the six a bot then settles on is an accident of that accident, which is why "every bot uses the same one or two weapons" is the symptom and the character file is not what is talking.

`BotFillInventory()` in `src/bot/bl_main.c` translates, one named slot at a time, resolved through `FindItemByClassname()` rather than written out as numbers: the game's numbering is the side that moves. Two ranges are deliberately not written -- `INVENTORY_HEALTH` (41), which the botlib fills from `stats[STAT_HEALTH]`, and everything from `ENEMY_HORIZONTAL_DIST` (200) up, which `BotUpdateInventory`/`BotUpdateBattleInventory` derive after the copy lands. `sv botinv` prints the botlib's slots and the client's own inventory on adjacent lines, which is the only instrument that can see this: `sv inventory` prints the game's itemlist, and the game's itemlist was never wrong.

**The enemy-weapon slots are a second numbering, and it is the view weapon's.** `BotUpdateBattleInventory` sets `ENEMY_BLASTER`..`ENEMY_GRAPPLE` (230-241) from the second byte of an enemy's `bot_updateentity_t.skinnum`, which is the `weapmodel` `ChangeWeapon` packs there (SPECS.md R-CORE-14's `WEAP_GRAPPLE` row). The 1999 botlib dropped `skinnum` in `AAS_UpdateEntity`, so every enemy read as unarmed; since `gladiator-bot-restored 96be69d` it is copied under `GLAD_SERVERFIX`, which this tree builds with. View weapons 1-11 are id's and agree. The botlib's grapple case is view weapon **19**, the Gladiator game's renumbered `WEAP_GRAPPLE`; here 19 is the chainfist and the grapple is 12, so an enemy with the chainfist fills `ENEMY_GRAPPLE` and one with the grapple fills nothing. **No shipped bot script reads 230-241**: `pak7.pak`'s `fw_aggr.c` and `fw_weap.c` read only the powerup slots and the distance, so this mismatch changes no play. A character file that switches on an enemy's weapon would make it matter, and the game's numbering would be what moves.

## Struct sizes -- the layout half of the contract

Every struct here crosses the library boundary, so both sides must agree on its size and on every offset in it. None of them contains a pointer, which is what makes a number meaningful at 32 and 64 bits alike:

| struct | bytes |
|---|---|
| `bsp_surface_t` | 24 |
| `bsp_trace_t` | 84 |
| `bot_settings_t` | 432 |
| `bot_clientsettings_t` | 144 |
| `bot_input_t` (= the botlib's `ea_state_t`) | 36 |
| `bot_updateclient_t` | 1228 |
| `bot_updateentity_t` | 104 |

`src/bot/botlib.h` asserts each of them, plus `q_offsetof` on `bsp_trace_t`'s `fraction` (8) and `endpos` (12) -- a size cannot see a transposition -- and the two host-side definitions the sizes turn on: `sizeof(qboolean) == 4` and `BOTLIB_MAX_STATS == 32`. `tools/botabi.py` does not take any of it on trust: it compiles a probe against `gladiator-bot-restored/game/{q_shared,botlib}.h` and compares what the botlib's compiler measures.

Two of the seven were wrong before that existed, and both were a type NAME rather than a number: `qboolean` substituted for `bool` in `osp-tourney`'s copy of the published header (one byte instead of four, twice, in `bsp_trace_t`), and `MAX_STATS`, which the 1999 header spells with the engine's name. Upstream asserts the same four facts on its own side as of `57ce85a3`, so a port that redefines either now fails to build at both ends rather than one.

Action flags: `ATTACK` 1, `USE` 2, `RESPAWN` 4, `JUMP`/`MOVEUP` 8, `CROUCH`/`MOVEDOWN` 16, `MOVEFORWARD` 32, `MOVEBACK` 64, `MOVELEFT` 128, `MOVERIGHT` 256, `DELAYEDJUMP` 512.

## Two botlibs

The game runs two botlibs through the one ABI above: Gladiator's (`gladiator-bot-restored`) and the Quake III bot (`q3a_bot_backport_for_q2`, the Quake III Arena bot adapted to Quake II, `vendor/q3a_bot_backport_for_q2`). Its interface header is `botlib/be_interface_q2.h`, the same two tables under `q2_` names -- its own Quake III headers define the plain names with other layouts -- and its export table goes on after `Test` with slots of its own, which a game copying twenty never reads. `tools/botabi.py` compares that header with `src/bot/botlib.h` as it compares Gladiator's: both tables in order, the struct sizes measured by a probe compiled against its headers (which assert the same seven sizes themselves), and the calling convention.

What differs between the two is data, and it is one row per botlib in `src/bot/bl_botlib.c` (SPECS.md R-BOT-31):

| | Gladiator | Quake III |
|---|---|---|
| library | `gladiator.so` / `.dll` | `q3bot.so` / `.dll` |
| `BotVersion` -- the handshake | `"BotLib v0.96"` | `"Q3Backport-<n>"` |
| its files | the gamedir: `pak7.pak`, `maps/<map>.aas` | `colosseum/q3bot/` and nothing else -- the `datadir` libvar |
| bot list | `botfile` / `bots_botfile`, and `bots/*.cfg` | `q3bot/bots.cfg` |
| AAS versions it reads | 2..3 | 4..5 |
| inventory slots 51 / 52 | Magslugs / Trap | Trap / Magslugs |
| inventory slots it writes itself | 41 (health) | 28 and 29 (health, armour) |
| HUD stats it is shown | `ps.stats` as it stands | 0..15; 22/23 under `ctf` only; 28/29 the hit record |
| means of death, in 29 | -- | the 1999 game's numbering |
| per-bot skill | no -- the character's | 1..5, the `bot_skill` libvar |

**The handshake** is `BotVersion`'s prefix. A library is accepted for a botlib only when it says it is that botlib, and one that answers as neither is refused, because every row of the table is per botlib. It is the handshake this document says `BotVersion` cannot be -- for the *convention* -- and it can be, and is, for the *botlib*.

**`datadir`** is the libvar the Quake III botlib reads its data root from, relative to the gamedir: with it set, every file it opens -- botfiles, AAS files, routing caches, read and written -- comes from there and from nowhere else. Both botlibs look for `maps/<map>.aas`, in formats the other cannot read, and without it the Quake III botlib also fell back to Gladiator's `pak7.pak` layout for any botfile of its own it could not find. Gladiator's files stay where 1999 put them.

**The HUD view** exists because the Quake III botlib reads stats 22/23 as its CTF team and 28/29 as its last attacker on every ruleset, and this tree's per-ruleset stat map (`src/g_stats.h`) puts the OSP runes and the second powerup timer there. So it is shown 0..15 as they are, Threewave's joined-team pictures under `ctf` alone, and in 28/29 the record `T_Damage` keeps for every bot (`BotClientHurt`): the attacker's client number, 0 for nobody, and the means of death in the 1999 game's numbering -- equal to this tree's through `MOD_TARGET_BLASTER`, then Threewave's grapple at 34 and the mission packs' one further on, mapped by name.

**Both at once.** One library is loaded per botlib, and a server whose `botlibs` offers both runs both: every bot on its own, every per-frame call -- `BotStartFrame`, the entity updates, the sounds, the client settings -- to both. Two botlibs are two images with an AAS world each, and both are opened without `RTLD_GLOBAL`; what is refused is a second copy of ONE botlib's library under another name, which the loader answers with the image it already has, told apart by its handle (SPECS.md R-BOT-33).

## Libvars

`BotInitLibrary` pushes these through `BotLibVarSet` **before** `BotSetupLibrary`, and the set is fixed **per botlib** -- no new libvar may be invented, because the botlib would ignore it. Both are pushed these 32:

`maxclients`, `maxentities`, `max_aaslinks`, `max_bsplinks`, `max_levelitems`, `autolaunchbspc`, `dmflags`, `ctf`, `ch`, `ra`, `xatrix`, `rogue`, `log`, `nochat`, `fastchat`, `altnames`, `rocketjump`, `forceclustering`, `forcereachability`, `forcewrite`, `nooptimize`, `framereachability`, `basedir`, `gamedir`, `cddir`, `usehook`, `laserhook`, `runes`, `techs`, `teamplay`, `teamplay_shell`, `assimilation`.

`ch` is pushed as a constant `"0"`: Colored Hitman is out of scope (N7), but the botlib reads the libvar and sending zero costs nothing.

The Quake III botlib is pushed two more, which only it reads: `datadir` (`q3bot`), with the other paths before `BotSetupLibrary`, and `bot_skill` (1..5) immediately before each `BotSetupClient` -- it reads the skill inside that call, so the value pushed there is that bot's. Neither botlib reads `ra`. Gladiator's registers it in `BotSetupDeathmatchAI` and never reads the value; the Quake III one's gametype follows `ctf`, `teamplay` and the team `dmflags` alone. So `arena` is a team game to both for one reason: this game pushes `teamplay 1` there, with the synthetic team skin `BotLib_BotClientSettings` sends, and both botlibs then compare skins whole (`BotSameTeam`, `Q2_ClientsOnSameTeam`). To the Quake III botlib that makes it a `GT_TEAM` game, with that AI's team chat and team-leader talk.

### Ruleset -> libvar mapping

This table is the single authority for it, derived from the code rather than from any document. 21 of the 32 names are pushed on **every** ruleset, so the botlib sees the same set and only the values move -- a libvar that is set on one ruleset and absent on another is a libvar whose default the botlib would silently use, which is the whole concern. The other eleven are pushed only when the game cvar of that name is set, on every ruleset alike, so where it is not the botlib's own default stands: the three engine limits `max_aaslinks`, `max_bsplinks` and `max_levelitems` as the cvar's value when it is positive (botlib defaults 4096, 4096 and 512), and the eight switches `autolaunchbspc`, `nochat`, `fastchat`, `altnames`, `forceclustering`, `forcereachability`, `forcewrite` and `nooptimize` as 1 (`BotSetVarIfSet`; botlib default 0). `rocketjump` is pushed as the cvar's value: the donor pushed only a 1, and the botlib's own default is 1, so `rocketjump 0` could never reach it.

**Two places, and the split matters.** `BotInitLibrary` pushes the names that cannot move once the map is running -- the resolved ruleset, the content layers, the engine limits, the chat and log switches. `BotRulesetLibVars` pushes the five that a person can change *while* the map is running, and it runs **every frame** beside the `dmflags` push that is its precedent. Both are in `src/bot/bl_main.c`.

Latched, in `BotInitLibrary`:

| libvar | value |
|---|---|
| `ctf` | 1 under `ctf`, else 0 |
| `ra` | 1 under `arena`, else 0 |
| `xatrix`, `rogue` | the content layers, which are orthogonal to the ruleset |
| `ch` | constant `"0"` (N7) |
| `assimilation`, `teamplay_shell` | constant `"0"` -- in the contract's fixed set, set by no ruleset here, and omitting them would leave the botlib on its own defaults for two names the set names |

Per frame, in `BotRulesetLibVars`. Every cell is the value the code computes; where it is a constant, the constant is the finding that put it there:

| ruleset | `usehook` | `laserhook` | `teamplay` | `runes` | `techs` |
|---|---|---|---|---|---|
| `dm`, `dmpro`, `tdm`, `duel` | `BotTourneyHook()` | same as `usehook` | **`tdm` alone** -- see below | `BotTourneyRunes()` | 0 |
| `ctf` | `ctf_hook` | **0** | **0** -- see below | `!(dmflags & DF_CTF_NO_TECH)` | the same |
| `arena` | `allow_grapple`, i.e. `arena.cfg`'s `grapple:` key | 0 | **1** | 0 | 0 |
| `sp` | unreachable: `G_BotsAllowed()` is false, so no library is ever loaded (N6). The `default:` arm is kept as the safe answer for a ruleset added later, because an unset libvar is whatever the previous map left in the botlib | | | | |

**`teamplay` is the row worth reading, because it is 1 under `arena`, 0 under `ctf`, and neither is arithmetic on "is this a team game".** The libvar decides whether `BotSameTeam()` consults the SKIN, and what the skin means differs:

* Under `arena` the skin is a synthetic team id that `BotLib_BotClientSettings` pushes, so a whole-string compare is exactly right and a lone player's team has one member.
* Under `ctf` the skin's own second half already *is* the team -- `ctf_r` against `ctf_b` -- and the `ctf` branch beneath `teamplay` compares that half. Setting `teamplay 1` made the whole string the comparison instead, so a red in `male/ctf_r` and a red in `female/ctf_r` were enemies and two players sharing a model were allies: **the botlib had no CTF teams at all.** Threewave's own donor sets no `teamplay` under ctf either, and the omission is load-bearing.
* Under the OSP four it is `RULESET_TDM` **alone**, and not any of the three predicates that look like it. `G_IsOspRuleset()` is true under `duel`; so is `OSP_IsTeams()`; so is `G_TeamplayEnabled()`, which is ruleset-derived precisely because `duel` *is* team play -- to the game. Not to the botlib: a duel is two teams of one, there is no ally, and on real skins `teamplay 1` makes team-mates of two duellists who picked the same model.

This is the tourney rule surviving its own cvar. It used to read `m_mode == MODE_TEAM`, and this file used to say that comparison was preserved exactly as written. `m_mode` is gone and the set of one mode it selected is the set of one ruleset selected now -- the comparison is preserved, the thing compared is not. **Abstract where the value comes from, never the comparison.**

`laserhook` is 0 under `ctf` for a reason the name hides. To the botlib it is a *movement model* -- `be_ai_move.c`'s "0 = CTF hook, 1 = laser hook", meaning does the hook grab instantly or fly there. Threewave's hook is a projectile in both of its renderings, and this tree's `laserhook` cvar only chooses between `TE_GRAPPLE_CABLE` and `TE_MEDIC_CABLE_ATTACK`; pushing the cvar through told the botlib the hook was instantaneous whenever an operator preferred the beam. 1999 agrees by omission -- its `#ifdef ZOID` block sets `usehook` and `runes` and never `laserhook`, which it sets only under TOURNEY, where the hook really is a laser.

`techs` is the other one that is not what it looks like. The botlib's only read of its tech switch is `LibVar("runes", "0")` (`be_ai2_dmq2.c`, `BotSetupDeathmatchAI`); `techs` is a name only `botlib.h`'s table documents. So under `ctf` the value goes into `runes`, and `techs` carries the same value for the documented name. CTF's techs are gated by `DF_CTF_NO_TECH` alone, never by the `runes` modifier, so the dmflag is what the botlib has to be told about. The brain acts on it only under `ctf`: a bot touching a tech it does not hold drops its own (`drop tech`) to take the new one.

## Physics libvars the botlib expects

Defaults from `botlib.h`: `sv_friction` 6, `sv_stopspeed` 100, `sv_gravity` 800, `sv_waterfriction` 1, `sv_watergravity` 400, `sv_maxvelocity` 300, `sv_maxwalkvelocity` 300, `sv_maxcrouchvelocity` 100, `sv_maxswimvelocity` 150, `sv_maxacceleration` 2200, `sv_airaccelerate` 0, `sv_maxstep` 18, `sv_maxbarrier` 50, `sv_maxsteepness` 0.7, `sv_jumpvel` 224, `sv_maxwaterjump` 20.
