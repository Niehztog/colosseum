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
//===========================================================================
//
// Name:                bl_botcfg.h
// Function:        bot configuration files
// Programmer:      Mr Elusive (MrElusive@demigod.demon.nl)
// Last update: 1998-01-12
// Tab Size:        3
//===========================================================================

typedef struct bot_s
{
    char name[MAX_PATH];
    char skin[MAX_PATH];
    char charfile[MAX_PATH];
    char charname[MAX_PATH];
    struct bot_s *next;
} bot_t;

extern bot_t *botlist;

void AppendPathSeperator(char *path, int length);
bot_t *FindBotWithName(char *name);
void CheckForNewBotFile(void);
int AddRandomBot(edict_t *ent);
