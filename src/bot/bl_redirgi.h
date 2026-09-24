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
// Game-import redirection, from osp-tourney@1895f8e.  The reconstruction's
// asm-matching address comments are stripped.
//===========================================================================
//
// Name:         bl_redirgi.h
// Function:     redirect the game import structure
// Programmer:   Mr Elusive (MrElusive@demigod.demon.nl)
// Last update:  1999-02-10
// Tab Size:     3
//===========================================================================

#ifndef BL_REDIRGI_H
#define BL_REDIRGI_H

// The index tables are sized from game.csr, Q2PRO's configstring remap, not
// from the 1999 constant 256.  With protocol extensions negotiated the limits
// are MAX_MODELS 8192, MAX_SOUNDS 2048, MAX_IMAGES 2048; without them, 256
// each.  The 1999 header spelled all three as `#define MAX_MODELINDEXES 256`
// and indexed the arrays with the engine's return value -- which on an
// extended server is a number the array cannot hold, so the donor's
// `newgameimport.error("modelindex out of range")` would abort the map rather
// than report it.
extern int    bot_max_modelindexes;
extern int    bot_max_soundindexes;
extern int    bot_max_imageindexes;

// global botimport structure
extern game_import_t newgameimport;
// the three index tables, allocated at InitGame after game.csr is chosen
extern char **modelindexes;
extern char **soundindexes;
extern char **imageindexes;

// initializes the newgameimport structure
void BotRedirectGameImport(void);
// allocates the index tables; InitGame, after game.csr
void BotIndexesAlloc(void);
// execute a client command but now for a bot
void BotClientCommand(int client, char *str, ...);
// execute a server command
void BotServerCommand(char *str, ...);
// only stores a client command, does not execute it
void BotStoreClientCommand(char *str, ...);
// clears the bot command arguments
void BotClearCommandArguments(void);
// clears the model and sound index
void ClearIndexes(void);
// The control: ask BotIndexRecord about one index without writing it.
void BotIndexProbe(const char *what, int index);
void BotIndexesForget(void);
// initializes the muzzleflash to sound index table
void BotInitMuzzleFlashToSoundindex(void);
// dumps the model index
void BotDumpModelindex(void);
// dumps the sound index
void BotDumpSoundindex(void);
// dumps the image index
void BotDumpImageindex(void);

// The donor's TECH1..5_INDEX block is not here.  It hardcoded five model
// indexes at 251..255 -- inside the 1999 256-entry table -- so that the brain
// would see an OSP rune as a CTF tech.  The table's size is a runtime fact
// here, so a fixed index near the old ceiling is meaningless; the translation
// now looks the tech model up by name in the live table.  Note
// also records that both Gladiator trees define TECH4_INDEX twice (254 then
// 255) and never define TECH5_INDEX, so the donor's TOURNEY path has never
// compiled anywhere.
// The five tech models the rune->tech translation maps onto.
extern const char *const bot_tech_models[5];

#endif // BL_REDIRGI_H
