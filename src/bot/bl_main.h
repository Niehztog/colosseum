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
// working Q2PRO port of the 1999 glue (SPECS.md sec 3).  HEADERS ONLY in Phase 5:
// src/tourney/ is written against them and R-OSP-5 requires botglobals to be
// declared exactly once and included.  The implementations are Phase 6 (sec 9).
// Donor-only: baseq2 has no counterpart, so it lives in src/tourney/ rather
// than being merged into a spine file (R-CORE-7).  The reconstruction's
// asm-matching address comments are stripped -- SPECS.md N1 makes those oracles
// meaningless here, and they survive at the pin.
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

#ifndef MAX_PATH
#define MAX_PATH        144
#endif

//first entity is the world, then the client entities follow
#define DF_ENTNUMBER(x)         (x - g_edicts)
#define DF_ENTCLIENT(x)         (x - 1 - g_edicts)
#define DF_NUMBERENT(x)         (&g_edicts[x])
#define DF_CLIENTENT(x)         (&g_edicts[x + 1])

//bot library
typedef struct bot_library_s
{
    char path[MAX_PATH];                //path to the library
#if defined(WIN32) || defined(_WIN32)
    HANDLE handle;                      //Win32 handle to the bot library
#else
    void *handle;
#endif
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
    //
#ifdef BOT_DEBUG
    int notest;                         //don't call the library test function
    int framecount;
    int timeframes;
    clock_t starttime;
    int nobotinput;                 //true if bot input isn't processed
    int nobotai;                        //true if bots don't execute ai
    //
    int debug_ainet;
    int debug_goalai;
    int debug_moveai;
    int debug_weapai;
#endif //BOT_DEBUG
} bot_globals_t;

//bl_setup.c
extern bot_globals_t botglobals;

void StringMakeGreen(char *str);
//
void BotSetup(void);
void BotExecuteInput(edict_t *bot);
//bot usage of libraries
bot_library_t *BotUseLibrary(char *path);
void BotFreeLibrary(bot_library_t *lib);
void BotUnloadAllLibraries(void);
void BotLibraryDump(void);
void BotClientDump(void);
bool BotStarted(edict_t *bot);
//
void BotLib_BotLoadMap(char *mapname);
int  BotLib_BotSetupClient(edict_t *ent, char *userinfo);
void BotLib_BotShutdownClient(edict_t *client);
void BotLib_BotMoveClient(edict_t *oldclient, edict_t *newclient);
void BotLib_BotClientSettings(edict_t *client);
void BotLib_BotSettings(edict_t *bot, bot_settings_t *settings);
void BotLib_BotLibVarSet(char *var_name, char *value);
void BotLib_BotStartFrame(float time);
void BotLib_BotUpdateClient(edict_t *bot);
void BotLib_BotUpdateEntity(edict_t *ent);
void BotLib_BotAddSound(edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs);
void BotLib_BotAddPointLight(vec3_t origin, int ent, float radius, float r, float g, float b, float time, float decay);
void BotLib_BotAI(edict_t *bot, float thinktime);
void BotLib_BotConsoleMessage(edict_t *bot, int type, char *message);
int  BotLib_Test(int parm0, char *parm1, vec3_t parm2, vec3_t parm3);
