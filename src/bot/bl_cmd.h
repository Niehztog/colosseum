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
// Bot commands, from osp-tourney@1d8427e (SPECS.md sec 5.4.6, R-BOT-24..25).
//===========================================================================
//
// Name:         bl_cmd.h
// Function:     bot commands
// Programmer:   Mr Elusive (MrElusive@demigod.demon.nl)
// Last update:  1999-02-10
// Tab Size:     3
//===========================================================================

#ifndef BL_CMD_H
#define BL_CMD_H

// `server` is true when the command arrived through ServerCommand (`sv <cmd>`)
// and false when it arrived through ClientCommand.  Returns true when handled.
bool BotCmd(const char *cmd, edict_t *ent, int server);

#endif // BL_CMD_H
