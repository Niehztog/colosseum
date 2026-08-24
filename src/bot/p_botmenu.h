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
// The Gladiator Bot menu tree (R-BOT-28), from gladiator-bot-restored@game.
//===========================================================================
//
// Name:         p_botmenu.h
// Function:     menu
// Programmer:   Mr Elusive (MrElusive@demigod.demon.nl)
// Last update:  1998-01-12
// Tab Size:     3
//===========================================================================

#ifndef BOT_P_BOTMENU_H
#define BOT_P_BOTMENU_H

// build the bot menu tree for the ACTIVE ruleset; called once per level
void bot_MenuCreate(void);
// free it; called from ShutdownGame, because the tree is TAG_GAME
void bot_MenuDestroy(void);
void bot_MenuForget(void);
// close the bot menu for the given client.  G_MenuClose() calls THIS; nothing
// here may call G_MenuClose(), or the arbiter and the engine recurse.
void bot_MenuClose(edict_t *ent);
// open the bot menu for the given client
void bot_MenuOpen(edict_t *ent);
// toggle the bot menu for the given client -- the `menu` bot command
void bot_MenuToggle(edict_t *ent);

#endif // BOT_P_BOTMENU_H
