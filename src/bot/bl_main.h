/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// The Gladiator Bot SDK's headers, from osp-tourney@1d8427e -- which carries a
// working Q2PRO port of the 1999 glue (SPECS.md sec 3).  The implementations
// arrive with this header's phase (sec 9 Phase 6); Phase 5 took the
// declarations alone so that R-OSP-5's "botglobals declared exactly once" could
// be true before there was anything to declare it for.
// The reconstruction's asm-matching address comments are stripped -- SPECS.md
// N1 makes those oracles meaningless here, and they survive at the pin.
// The SDK's own library interface.  The donor pulls it in from its g_local.h;
// here it is included where it is needed, which keeps botlib.h off every
// translation unit in the tree.
#include "bot/botlib.h"

//===========================================================================
//
// Name:         bl_main.h
// Function:     bot setup
// Programmer:   Mr Elusive (MrElusive@demigod.demon.nl)
// Last update:  1999-02-10
// Tab Size:     3
//===========================================================================

#ifndef BL_MAIN_H
#define BL_MAIN_H

// The SDK's own path bound, and it needs a name of its own.  The donor writes
// `MAX_PATH` behind `#ifndef`, which collides with the Windows constant:
// mingw's minwindef.h defines MAX_PATH as 260 UNCONDITIONALLY, and it is a
// system header, so gcc suppresses the redefinition warning -Werror would
// otherwise turn into an error.  The result compiles and diverges quietly --
// every buffer declared after a `#include <windows.h>` in the same translation
// unit is 260 bytes and every one before it is 144 -- and moving the include
// above this header would change `bot_library_t.path`'s size in one translation
// unit and not the others, which is a struct-layout mismatch rather than an
// inconsistency.  R-90 is this collision caught the other way round.
#define BOT_MAX_PATH        144

//first entity is the world, then the client entities follow
#define DF_ENTNUMBER(x)         ((int)((x) - g_edicts))
#define DF_ENTCLIENT(x)         ((int)((x) - 1 - g_edicts))
#define DF_NUMBERENT(x)         (&g_edicts[x])
#define DF_CLIENTENT(x)         (&g_edicts[(x) + 1])

//bot library
typedef struct bot_library_s
{
    char path[BOT_MAX_PATH];                //path to the library
    // The donor writes `HANDLE` on Win32 and `void *` elsewhere, which needs
    // <windows.h> in every translation unit that includes this header -- and
    // <windows.h> redefined the MAX_PATH this header used to define, so under
    // include is not free.  Win32's HANDLE *is* `void *` (`typedef PVOID
    // HANDLE`), so one member serves both and the #if disappears with it; the
    // loader casts, which it would have had to do anyway for dlsym's return.
    // Caught by the PE targets: `make native` alone never compiles this.
    void *handle;                       //dlopen/LoadLibrary handle
    bot_export_t funcs;             //functions exported from the bot library
    int users;                          //number of bots using the library
    struct bot_library_s *prev; //links in the library list
    struct bot_library_s *next;
} bot_library_t;

//bot state
typedef struct bot_state_s
{
    bool active;                    //true if a bot is active for this client
    bool started;                   //true if the bot has started
    bot_library_t *library;         //used library by the bot
    // R-140's two counters, and they are here rather than in a file-static
    // because this struct is already the per-client bot state and is already
    // reallocated per game.  `firecalls` is every frame the brain asked to
    // shoot while its arena was NOT being fought; `firedrops` is how many of
    // those the button gate took away.  `sv botinv` prints the pair: equal
    // means the gate is doing its whole job, and a gap is a shot that went out
    // during a countdown.
    int firecalls;
    int firedrops;
} bot_state_t;

//bot globals
typedef struct bot_globals_s
{
    int numbots;                        //number of bots
    bot_state_t *botstates;         //bot states
    bot_input_t *botinputs;         //bot inputs
    bool *botnewinput;          //array with flags, true if input is new
    bot_import_t gamebotimport; //bot library import functions
    bot_library_t *firstbotlib; //first bot libary
    int nocldouble;                 //no double client movement frames
    // The donor guards the four below with `#ifdef BOT_DEBUG`, which is never
    // defined anywhere in either tree -- so `botpause`, which R-BOT-24 requires,
    // could not work.  They are unconditional here: sec 7 rule 6 keeps #ifdef
    // out of the game tree, and a switch that cannot be compiled in is not a
    // switch.  `nobotai` is what `sv botpause` toggles.
    int notest;                         //don't call the library test function
    int nobotinput;                 //true if bot input isn't processed
    int nobotai;                        //true if bots don't execute ai
} bot_globals_t;

//bl_main.c
extern bot_globals_t botglobals;

void StringMakeGreen(char *str);
//
void BotSetup(void);
// The counterpart of BotSetup, for a caller that has already freed TAG_GAME.
void BotForgetGameMemory(void);
void BotShutdown(void);
void BotExecuteInput(edict_t *bot);
//bot usage of libraries
bot_library_t *BotUseLibrary(const char *path);
void BotFreeLibrary(bot_library_t *lib);
void BotUnloadAllLibraries(void);
void BotLibraryDump(void);
void BotClientDump(void);
void BotInventoryDump(void);
bool BotStarted(edict_t *bot);
//the default botlib filename for this platform and build (R-BOT-4)
const char *BotDefaultLibrary(void);
//
void BotLib_BotLoadMap(char *mapname);
int  BotLib_BotSetupClient(edict_t *ent, char *userinfo);
void BotLib_BotShutdownClient(edict_t *client);
void BotLib_BotMoveClient(edict_t *oldclient, edict_t *newclient);
void BotLib_BotClientSettings(edict_t *client);
void BotLib_ClientDisconnected(edict_t *ent);
void BotLib_BotSettings(edict_t *bot, bot_settings_t *settings);
void BotLib_BotStartFrame(float time);
void BotLib_BotUpdateClient(edict_t *bot);
void BotLib_BotUpdateEntity(edict_t *ent);
void BotLib_BotAddSound(edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs);
void BotLib_BotAddPointLight(vec3_t origin, int ent, float radius, float r, float g, float b, float time, float decay);
void BotLib_BotAI(edict_t *bot, float thinktime);
void BotLib_BotConsoleMessage(edict_t *bot, int type, char *message);
int  BotLib_Test(int parm0, char *parm1, vec3_t parm2, vec3_t parm3);

// R-BOT-20's frame section, in one function so that the order the requirement
// fixes cannot be re-arranged by an edit to G_RunFrame.
void BotRunFrame(void);

// R-BOT-23's measurement.  The budget is half a 100 ms frame, in microseconds.
#define BOTPERF_BUDGET_US   50000
void BotPerfReset(void);
void BotPerfReport(void);

// ---- R-BOT-29: the seventeen TOURNEY blocks, as ruleset-neutral accessors ---
//
// osp-tourney defines TOURNEY at g_local.h:16 -- LIVE, not `#if 0` -- and every
// bl_*.c includes g_local.h first, so the donor's own `//#define TOURNEY` lines
// are inert text and all seventeen blocks are active library-wide.  Neither
// state is acceptable here: on, ctf/arena lose `minimumplayers`, `botfile`
// and the SDK's loading-screen swap; off, OSP's runes stop reaching the brain.
// So each block becomes a branch on the ACTIVE ruleset, and the tourney state
// the blocks read is reached through these rather than through an extern that
// would resolve to tourney's globals in every ruleset.
//
// There is no BotTourneyMode() any more: `m_mode` is gone and the mode of play
// IS the ruleset (R-OSP-12).  What the accessor existed to protect survives and
// is worth restating, because the flattening makes the wrong answer look more
// natural than it did: the brain's `teamplay` libvar is `RULESET_TDM` ALONE.
// `duel` is two teams of one, the brain has no ally, and telling it otherwise
// gives both duellists an imaginary team-mate the moment they wear the same
// model.  So do NOT reach for G_IsOspRuleset(), OSP_IsTeams() or
// G_TeamplayEnabled() at that call site -- all three are true under `duel`, and
// all three are a different question (R-BOT-29).
int  BotTourneyRunes(void);         // rune_stat, or 0
bool BotTourneyHook(void);          // hook_enable, or false
int  BotTourneyVotedIn(void);       // bots_votedin, or 0
// The cvar names are per ruleset (R-OSP-11): the OSP four use tourney's own
// `bots_minplayers` and `bots_botfile`, ctf and arena `minimumplayers` and
// `botfile`.
const char *BotMinPlayersCvar(void);
const char *BotFileCvar(void);
// The switch that replaces the flat count with a target read off the game is
// ONE cvar, `botfill`, for every ruleset (R-RA-7, R-CTF-8, R-DM-1).  It was
// three names on R-OSP-11's authority and that rule never covered it -- see the
// comment on BotFillEnabled().
bool BotFillEnabled(void);
// ...and the cvars themselves, obtained once with the RULESET's default.
// R-COMPAT-6: osp_main.c registers `bots_minplayers` with a default of "4" and
// the SDK registers `minimumplayers` with "0", so a bot-layer call site that
// spelled its own default would be a second registration of one name with two
// values -- the exact collision `statsfile`/`statsname` was in 1.21.
cvar_t *BotMinPlayers(void);
cvar_t *BotFile(void);
// True when a client is playing rather than connecting, observing or queued.
// Under tourney that is `resp.osp_entered == ENTERED_ENTERED`; elsewhere the
// question has no fourth state and the answer is "it is in use".
bool BotCountsAsPlayer(edict_t *cl_ent);

#endif // BL_MAIN_H
