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
// Fake clients, from osp-tourney@1d8427e.
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
// it at the end of a match.  Declared here because bl_spawn.c defines it --
// a donor's object declared somewhere other than the header that owns it is
// the shape to avoid, and this one was in src/tourney/osp_types.h.
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
// The target `botfill` asks for, clamped to two sides, `game.maxclients` and
// whatever the roster could actually supply, or 0 when the switch is off.  `sv
// ruleset` prints it, because it is computed rather than stored and there is
// nowhere else to read it back from.
int  BotFillTarget(void);
void BotFillNoMore(int achieved);
// Where BotFillTarget()'s number came from, for `sv ruleset`.
void BotFillDescribe(char *buf, size_t len);
// the loading image the SDK paints while a bot is being created (emptied under
// tourney, which has its own bar)
void ShowLoadImage(edict_t *ent);
void RemoveLoadImage(edict_t *ent);

#endif // BL_SPAWN_H
