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
// Fake clients, from osp-tourney@1d8427e (SPECS.md sec 5.4.4, R-BOT-14..19).
//===========================================================================
//
// Name:         bl_spawn.h
// Function:     spawning of bots
// Programmer:   Mr Elusive (MrElusive@demigod.demon.nl)
// Last update:  1999-02-10
// Tab Size:     3
//===========================================================================

#ifndef BL_SPAWN_H
#define BL_SPAWN_H

// The mod's own bot-count latch, not the SDK's: CheckMinimumPlayers compares it
// against the count it just took so that it does not ask for the same bot twice
// while the first one is still in the queue, and tourney's OSP_endClean resets
// it at the end of a match.  Declared HERE because bl_spawn.c defines it --
// R-OSP-5 is about exactly this, a donor's object declared somewhere other than
// the header that owns it, and it was in src/tourney/osp_types.h until Phase 6.
extern int old_botcount;

// spawns bots after level change
void BotSpawn(void);
void BotQueueForget(void);
// adds one deathmatch bot
void BotAddDeathmatch(edict_t *ent);
// removes one deathmatch bot
void BotRemoveDeathmatch(edict_t *ent);
// moves a bot to a free client edict
bool BotMoveToFreeClientEdict(edict_t *bot);
// lets a human client become a bot
void BotBecomeDeathmatch(edict_t *ent);
// destroys a bot
void BotDestroy(edict_t *bot);
// destroys every bot; ShutdownGame, and the `sv removebot all` path
void BotDestroyAll(void);
// adds a bot to the spawn queue
void AddBotToQueue(edict_t *ent, const char *library, const char *userinfo);
// spawn waiting bots
void AddQueuedBots(void);
//
void CheckMinimumPlayers(void);
// the loading image the SDK paints while a bot is being created (R-BOT-29:
// emptied under tourney, which has its own bar)
void ShowLoadImage(edict_t *ent);
void RemoveLoadImage(edict_t *ent);

#endif // BL_SPAWN_H
