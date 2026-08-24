#ifndef OSP_P_MENU_H
#define OSP_P_MENU_H

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
// OSP Tourney DM v2.75, from osp-tourney@1d8427e (doc/provenance.md).
// Donor-only: baseq2 has no counterpart, so it lives in src/tourney/ rather
// than being merged into a spine file (R-CORE-7).  The reconstruction's
// asm-matching address comments are stripped -- SPECS.md N1 makes those oracles
// meaningless here, and they survive at the pin.
enum {
    osp_PMENU_ALIGN_LEFT,
    osp_PMENU_ALIGN_CENTER,
    osp_PMENU_ALIGN_RIGHT
};

typedef struct osp_pmenuhnd_s {
    struct osp_pmenu_s *entries;
    int cur;
    int num;
} osp_pmenuhnd_t;

typedef struct osp_pmenu_s {
    char *text;
    int align;
    void *arg;
    void (*SelectFunc)(edict_t *ent, struct osp_pmenu_s *entry);
} osp_pmenu_t;

void osp_PMenu_Open(edict_t *ent, osp_pmenu_t *entries, int cur, int num);
void osp_PMenu_Close(edict_t *ent);
void osp_PMenu_Update(edict_t *ent);
void osp_PMenu_Next(edict_t *ent);
void osp_PMenu_Prev(edict_t *ent);
void osp_PMenu_Select(edict_t *ent);

#endif // OSP_P_MENU_H
