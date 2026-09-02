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
// Rocket Arena 2 v2.25, from rocketarena2-public@d20e1ce (doc/provenance.md).
// Donor-only: baseq2 has no counterpart, so it lives in src/arena/ rather than
// being merged into a spine file (R-CORE-7).  The reconstruction's asm-matching
// address comments are stripped -- SPECS.md N1 makes those oracles meaningless
// here, and they survive at the pin.
#ifndef _MENU_H
#define _MENU_H

typedef struct qmenu_s {
    void            *it;
    struct qmenu_s  *next;
    struct qmenu_s  *prev;
} qmenu_t;

typedef int (*menuselect_t)(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg);

typedef struct {
    char            *text;
    char            *value;
    // The value block is allocated from the INITIAL value's length and then
    // overwritten in place by menuChangeYesNo, menuChangeProtect,
    // menuChangeMap and Cmd_admin_f.  Nothing recorded how big it was, so each
    // of those was an unbounded write into a TAG_LEVEL block sized by an
    // unrelated string -- a map name out of arena.cfg being the reachable one
    // (R-SEC-1).  This is that size.
    size_t          valuesize;
    int             num;
    menuselect_t    select;
} menuitem_t;

typedef struct {
    char    *title;
    qmenu_t *items;
    int     flags;
} menuinfo_t;

#define MAXSTATUSBAR    1400
#define MAXMENUTEXT     MAXSTATUSBAR

void        add_to_queue(qmenu_t *node, qmenu_t *head);
qmenu_t     *remove_from_queue(qmenu_t *node, qmenu_t *head);
void        add_to_front_queue(qmenu_t *node, qmenu_t *head);

int         count_queue(qmenu_t *head);

char        *LoPrint(char *string);
char        *HiPrint(char *string);
void        SendMenu(edict_t *ent);
void        SendStatusBar(edict_t *ent, const char *string, bool transmit);
void        DisplayMenu(edict_t *ent);
qmenu_t     *CreateQMenu(edict_t *ent, char *title);
// `text` and `value` are const: both are COPIED into TAG_LEVEL storage and
// never written through, and R-182 passes a row label out of a
// `const ra_pack_weapon_t []`.  Every existing caller still compiles --
// char * converts to const char * on its own.
qmenu_t     *AddMenuItem(qmenu_t *menu, const char *text, const char *value, int num, menuselect_t select);
void        FinishMenu(edict_t *ent, qmenu_t *menu, bool show);
void        MenuNext(edict_t *ent);
void        MenuPrev(edict_t *ent);
void        UseMenu(edict_t *ent, int arg);
bool    MenuThink(edict_t *ent);
// MENU_ARENA's row in G_MenuClose()'s switch (R-MENU-2a).
void        ra_MenuClose(edict_t *ent);
// ...and the DESTROY that row deliberately is not.  See close_menus() in
// menu.c: under arena a close is a hide, so the one caller that is about to
// lose the queue head has to free the queue as well.
void        close_menus(edict_t *ent);

#endif // _MENU_H
