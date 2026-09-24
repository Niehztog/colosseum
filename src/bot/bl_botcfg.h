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
// bots.cfg, from osp-tourney@1895f8e.
//===========================================================================
//
// Name:                bl_botcfg.h
// Function:        bot configuration files
// Programmer:      Mr Elusive (MrElusive@demigod.demon.nl)
// Last update: 1998-01-12
// Tab Size:        3
//===========================================================================

#ifndef BL_BOTCFG_H
#define BL_BOTCFG_H

typedef struct bot_s
{
    char name[BOT_MAX_PATH];
    char skin[BOT_MAX_PATH];
    char charfile[BOT_MAX_PATH];
    char charname[BOT_MAX_PATH];
    struct bot_s *next;
} bot_t;

extern bot_t *botlist;

void AppendPathSeperator(char *path, int length);
bot_t *FindBotWithName(const char *name);
void CheckForNewBotFile(void);
void LoadBots(void);
void BotListForget(void);
int AddRandomBot(edict_t *ent);

#endif // BL_BOTCFG_H
