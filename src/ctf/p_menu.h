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

// Threewave's menu engine -- one of the four R-MENU-1 ships.  §5.2 gives it
// `src/ctf/p_menu.c/.h` and the `ctf_` prefix, because OSP ships a p_menu.h of
// its own with an INCOMPATIBLE pmenu_t (`arg` per entry rather than on the
// handle) and the same basename.  The subfolder keeps the files apart, the
// prefix keeps the symbols apart, and neither directory goes on the include
// path (R-25 item 3): a user writes `#include "ctf/p_menu.h"`.

#ifndef CTF_P_MENU_H
#define CTF_P_MENU_H

enum {
    CTF_PMENU_ALIGN_LEFT,
    CTF_PMENU_ALIGN_CENTER,
    CTF_PMENU_ALIGN_RIGHT
};

// Declared in g_local.h as an opaque type, because gclient_t holds one.
struct ctf_pmenuhnd_s {
    struct ctf_pmenu_s *entries;
    int cur;
    int num;
    void *arg;
};

typedef void (*ctf_SelectFunc_t)(edict_t *ent, ctf_pmenuhnd_t *hnd);

typedef struct ctf_pmenu_s {
    char *text;
    int align;
    ctf_SelectFunc_t SelectFunc;
} ctf_pmenu_t;

ctf_pmenuhnd_t *ctf_PMenu_Open(edict_t *ent, const ctf_pmenu_t *entries, int cur, int num, void *arg);
void ctf_PMenu_Close(edict_t *ent);
void ctf_PMenu_UpdateEntry(ctf_pmenu_t *entry, char *text, int align, ctf_SelectFunc_t SelectFunc);
void ctf_PMenu_Do_Update(edict_t *ent);
void ctf_PMenu_Update(edict_t *ent);
void ctf_PMenu_Next(edict_t *ent);
void ctf_PMenu_Prev(edict_t *ent);
void ctf_PMenu_Select(edict_t *ent);

#endif // CTF_P_MENU_H
