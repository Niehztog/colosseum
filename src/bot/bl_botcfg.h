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

struct botlib_s;

typedef struct bot_s
{
    char name[BOT_MAX_PATH];
    char skin[BOT_MAX_PATH];
    char charfile[BOT_MAX_PATH];
    char charname[BOT_MAX_PATH];
    // The botlib whose bot list this row came from, and that botlib's place in
    // `botlibs`: the list is kept in that order, then by name, so each
    // botlib's bots are together and the preferred botlib's come first
    // (R-BOT-32).
    const struct botlib_s *botlib;
    int rank;
    struct bot_s *next;
} bot_t;

extern bot_t *botlist;

void AppendPathSeperator(char *path, int length);
// A bot by its name, in one botlib's list or, for NULL, in the first list
// that has it.
bot_t *FindBotWithName(const char *name, const struct botlib_s *botlib);
// The row naming this character file and character name, if any list has one.
bot_t *FindBotWithCharacter(const char *charfile, const char *charname);
// How many bots a botlib's list holds; 0 for a botlib not offered.
int BotRosterCount(const struct botlib_s *botlib);
void CheckForNewBotFile(void);
void LoadBots(void);
void BotListForget(void);
int AddRandomBot(edict_t *ent);

#endif // BL_BOTCFG_H
