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
#include "g_local.h"
#include "tourney/osp_types.h"

void osp_PMenu_Open(edict_t *ent, osp_pmenu_t *entries, int cur, int num)
{
    osp_pmenuhnd_t *hnd;
    osp_pmenu_t *p;
    int i;

    if (!ent->client)
        return;

    if (ent->client->osp_menu) {
        gi.dprintf("warning, ent already has a menu\n");
        osp_PMenu_Close(ent);
    }

    // gi.TagMalloc, not malloc: the free below is gi.TagFree and the pair has
    // to match (R-55).  TAG_LEVEL because a menu does not outlive its level.
    hnd = gi.TagMalloc(sizeof(*hnd), TAG_LEVEL);

    hnd->entries = entries;
    hnd->num = num;

    if (cur < 0 || !entries[cur].SelectFunc) {
        for (i = 0, p = entries; i < num; i++, p++)
            if (p->SelectFunc)
                break;
    } else
        i = cur;

    if (i >= num)
        hnd->cur = -1;
    else
        hnd->cur = i;

    ent->client->showscores = true;
    G_MenuOpen(ent, MENU_TOURNEY);
    ent->client->osp_menu = hnd;

    osp_PMenu_Update(ent);
    gi.unicast(ent, true);
}

// The engine's own teardown.  G_MenuClose() calls THIS; nothing here may call
// G_MenuClose(), or the arbiter and the engine recurse into each other.
void osp_PMenu_Close(edict_t *ent)
{
    if (!ent->client->osp_menu)
        return;

    gi.TagFree(ent->client->osp_menu);
    ent->client->osp_menu = NULL;
    ent->client->showscores = false;

    gi.WriteByte(svc_layout);
    gi.WriteString("xv 0 yv 0 string \" \"");
    gi.unicast(ent, true);
}

void osp_PMenu_Update(edict_t *ent)
{
    char string[1400];
    int i;
    osp_pmenu_t *p;
    int x;
    osp_pmenuhnd_t *hnd;
    char *t;
    bool alt = false;

    if (!ent->client->osp_menu) {
        gi.dprintf("warning:  ent has no menu\n");
        return;
    }

    hnd = ent->client->osp_menu;

    strcpy(string, "xv 32 yv 8 picn inventory ");

    for (i = 0, p = hnd->entries; i < hnd->num; i++, p++) {
        if (!p->text || !*(p->text))
            continue; // blank line
        t = p->text;
        if (*t == '*') {
            alt = true;
            t++;
        }
        sprintf(string + strlen(string), "yv %d ", 32 + i * 8);
        if (p->align == osp_PMENU_ALIGN_CENTER)
            x = 196 / 2 - strlen(t) * 4 + 60;
        else if (p->align == osp_PMENU_ALIGN_RIGHT)
            x = 60 + (212 - strlen(t) * 8);
        else
            x = 60;

        sprintf(string + strlen(string), "xv %d ",
                x - ((hnd->cur == i) ? 8 : 0));

        if (hnd->cur == i && !alt)
            sprintf(string + strlen(string), "string2 \"\x0d%s\" ", t);
        else if (alt && hnd->cur != i)
            sprintf(string + strlen(string), "string2 \"%s\" ", t);
        else if (alt && hnd->cur == i)
            sprintf(string + strlen(string), "string \"\x0d%s\" ", t);
        else
            sprintf(string + strlen(string), "string \"%s\" ", t);
        alt = false;
    }

    gi.WriteByte(svc_layout);
    gi.WriteString(string);
}

void osp_PMenu_Next(edict_t *ent)
{
    osp_pmenuhnd_t *hnd;
    int i;
    osp_pmenu_t *p;

    if (!ent->client->osp_menu) {
        gi.dprintf("warning:  ent has no menu\n");
        return;
    }

    hnd = ent->client->osp_menu;

    if (hnd->cur < 0)
        return; // no selectable entries

    i = hnd->cur;
    p = hnd->entries + hnd->cur;
    do {
        i++, p++;
        if (i == hnd->num)
            i = 0, p = hnd->entries;
        if (p->SelectFunc)
            break;
    } while (i != hnd->cur);

    hnd->cur = i;

    osp_PMenu_Update(ent);
    gi.unicast(ent, true);
}

void osp_PMenu_Prev(edict_t *ent)
{
    osp_pmenuhnd_t *hnd;
    int i;
    osp_pmenu_t *p;

    if (!ent->client->osp_menu) {
        gi.dprintf("warning:  ent has no menu\n");
        return;
    }

    hnd = ent->client->osp_menu;

    if (hnd->cur < 0)
        return; // no selectable entries

    i = hnd->cur;
    p = hnd->entries + hnd->cur;
    do {
        if (i == 0) {
            i = hnd->num - 1;
            p = hnd->entries + i;
        } else
            i--, p--;
        if (p->SelectFunc)
            break;
    } while (i != hnd->cur);

    hnd->cur = i;

    osp_PMenu_Update(ent);
    gi.unicast(ent, true);
}

void osp_PMenu_Select(edict_t *ent)
{
    osp_pmenuhnd_t *hnd;
    osp_pmenu_t *p;

    if (!ent->client->osp_menu) {
        gi.dprintf("warning:  ent has no menu\n");
        return;
    }

    hnd = ent->client->osp_menu;

    if (hnd->cur < 0)
        return; // no selectable entries

    p = hnd->entries + hnd->cur;

    if (p->SelectFunc)
        p->SelectFunc(ent, p);
}
