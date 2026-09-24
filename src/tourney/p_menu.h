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
// OSP Tourney DM v2.75, from osp-tourney@1895f8e.
// Donor-only: baseq2 has no counterpart, so it lives in src/tourney/ rather
// than being merged into a spine file.  The reconstruction's asm-matching
// address comments are stripped.
//
// This IS src/ctf/p_menu.h's engine, one generation earlier.  Threewave wrote
// `pmenu_t`; OSP took a copy and CTF kept developing theirs, so the two
// engines that ship side by side are forks of one file rather than two
// designs.  The `arg`-per-entry here against `arg`-on-the-handle there is the
// visible divergence, and the invisible ones were that this copy had none of
// the three departures `src/ctf/p_menu.c` documents.  It has them now;
// `p_menu.c` says what each one is for.
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

void osp_PMenu_Open(edict_t *ent, const osp_pmenu_t *entries, int cur, int num);
void osp_PMenu_Close(edict_t *ent);
// The select callback is spelled out rather than typedef'd, which ctf/p_menu.h
// does not have to do.  tools/donorgate.py builds this donor's surface out of
// this header with a regex, and `typedef void (*osp_SelectFunc_t)(...)` reads
// to it as a declaration of `void` -- which then makes every `void` in every
// spine file an ungated use of tourney's surface.  ctf/ is not a donor
// directory, so its typedef is never scanned.
void osp_PMenu_UpdateEntry(osp_pmenu_t *entry, const char *text, int align,
                           void (*SelectFunc)(edict_t *ent, struct osp_pmenu_s *entry));
// Re-copy `entries` into this client's private rows.  Every leaf in
// osp_menus.c that restages a table calls it before osp_PMenu_Update(); see
// p_menu.c for why it is silent when no menu is open.
void osp_PMenu_Sync(edict_t *ent, const osp_pmenu_t *entries);
// Compose and write the layout now.  osp_PMenu_Update() is the rate-limited
// door to it, and ClientThink is what flushes what that door defers.
void osp_PMenu_Do_Update(edict_t *ent);
void osp_PMenu_Update(edict_t *ent);
void osp_PMenu_Next(edict_t *ent);
void osp_PMenu_Prev(edict_t *ent);
void osp_PMenu_Select(edict_t *ent);
// The menu key, both of them.  `reverse` is `invdrop` rather than
// `invuse`, and it is the STEP DIRECTION the settings rows read off
// `resp.osp_r264`, not a second kind of selection.  Carries the donor's
// two-frame debounce, which belongs with the key and not with its caller.
void OSP_menuSelect(edict_t *ent, bool reverse);

#endif // OSP_P_MENU_H
