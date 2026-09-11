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
//
// Threewave's `pmenu_t` engine, kept as its own engine rather than unified
// -- one of four engines that ship side by side.  Three departures from the
// donor, each required rather than a matter of taste:
//
//  1. The builder is bounded.  The donor writes into a 1400-byte
//     `char string[1400]` with `sprintf(string + strlen(string), ...)` and no
//     check at all -- 24 entries of 40 characters overflow it, and the overflow
//     is a stack buffer in the middle of a function that then hands the result
//     to gi.WriteString.  MAXSTATUSBAR has to be respected, with truncation
//     "on a whole item, never mid-token", so each entry is composed into its own
//     scratch buffer and appended only if it fits whole.
//
//  2. Allocation goes through the game import.  The donor uses libc
//     malloc/strdup/free, and never frees on disconnect or level change, so a
//     client who quits with the join menu open leaks it.  gi.TagMalloc with
//     TAG_LEVEL puts the memory under the engine's accounting and under
//     SpawnEntities' FreeTags, and g_spawn.c clears every client's menu owner
//     immediately after that FreeTags so the handle cannot dangle.
//
//  3. Ownership is external.  The donor tracks `inmenu` per
//     client inside this engine.  With four engines that is four answers to one
//     question, so opening goes through G_MenuOpen() -- which closes whatever
//     was open first, whichever engine owned it.
//

#include "g_local.h"
#include "ctf/p_menu.h"

// The layout channel's budget.  Same 1400 as the statusbar (g_stats.h) and the
// same reason: it is what svc_layout carries.
#define CTF_MENU_MAX    MAX_STATUSBAR

// Note that the pmenu entries are duplicated: a static set of entries can then
// be used for several clients and edited per client without interference.  arg
// is freed when the menu is closed, so it must be TagMalloc'd by the caller.
ctf_pmenuhnd_t *ctf_PMenu_Open(edict_t *ent, const ctf_pmenu_t *entries, int cur, int num, void *arg)
{
    ctf_pmenuhnd_t *hnd;
    const ctf_pmenu_t *p;
    int i;

    if (!ent->client)
        return NULL;

    // And the reason the donor's "warning, ent already has a menu"
    // dprintf is gone: the incumbent is closed by the one open path, whichever
    // engine owned it, so having one open is normal rather than notable.
    G_MenuOpen(ent, MENU_CTF);

    hnd = gi.TagMalloc(sizeof(*hnd), TAG_LEVEL);

    hnd->arg = arg;
    hnd->entries = gi.TagMalloc(sizeof(ctf_pmenu_t) * num, TAG_LEVEL);
    memcpy(hnd->entries, entries, sizeof(ctf_pmenu_t) * num);
    // duplicate the strings since they may be from static memory
    for (i = 0; i < num; i++)
        if (entries[i].text)
            hnd->entries[i].text = G_CopyString(entries[i].text);

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
    ent->client->ctf_menu = hnd;

    ctf_PMenu_Do_Update(ent);
    gi.unicast(ent, true);

    return hnd;
}

void ctf_PMenu_Close(edict_t *ent)
{
    int i;
    ctf_pmenuhnd_t *hnd;

    if (!ent->client->ctf_menu)
        return;

    hnd = ent->client->ctf_menu;
    for (i = 0; i < hnd->num; i++)
        if (hnd->entries[i].text)
            gi.TagFree(hnd->entries[i].text);
    gi.TagFree(hnd->entries);
    if (hnd->arg)
        gi.TagFree(hnd->arg);
    gi.TagFree(hnd);
    ent->client->ctf_menu = NULL;
    ent->client->showscores = false;

    // G_ctf.c closes its own menus in eighteen places -- every menu
    // callback that does something and dismisses -- so the owner has to be
    // released here as well as in G_MenuClose.  Leaving it set would make
    // G_MenuActive() true with a NULL handle, which is a "warning: ent has no
    // menu" every frame and, worse, a menu that swallows input it cannot draw.
    // Setting it twice is harmless; setting it in only one of the two paths is
    // not.
    ent->client->menu_owner = MENU_NONE;
}

// only use on menus that have been opened with ctf_PMenu_Open
void ctf_PMenu_UpdateEntry(ctf_pmenu_t *entry, char *text, int align, ctf_SelectFunc_t SelectFunc)
{
    if (entry->text)
        gi.TagFree(entry->text);
    entry->text = G_CopyString(text);
    entry->align = align;
    entry->SelectFunc = SelectFunc;
}

// One whole item at a time; an item that does not fit is dropped and
// the caller is told once, rather than the buffer being run past its end.
static bool menu_append(char *string, size_t size, size_t *len, const char *item)
{
    size_t n = strlen(item);

    if (*len + n >= size)
        return false;
    memcpy(string + *len, item, n + 1);
    *len += n;
    return true;
}

void ctf_PMenu_Do_Update(edict_t *ent)
{
    char string[CTF_MENU_MAX];
    char item[128];
    size_t len;
    int i;
    ctf_pmenu_t *p;
    int x;
    ctf_pmenuhnd_t *hnd;
    char *t;
    bool alt = false;
    bool dropped = false;

    if (!ent->client->ctf_menu) {
        gi.dprintf("warning:  ent has no menu\n");
        return;
    }

    hnd = ent->client->ctf_menu;

    len = 0;
    string[0] = 0;
    menu_append(string, sizeof(string), &len, "xv 32 yv 8 picn inventory ");

    for (i = 0, p = hnd->entries; i < hnd->num; i++, p++) {
        if (!p->text || !*(p->text))
            continue; // blank line
        t = p->text;
        if (*t == '*') {
            alt = true;
            t++;
        }

        if (p->align == CTF_PMENU_ALIGN_CENTER)
            x = 196 / 2 - strlen(t) * 4 + 64;
        else if (p->align == CTF_PMENU_ALIGN_RIGHT)
            x = 64 + (196 - strlen(t) * 8);
        else
            x = 64;

        // The whole entry -- position and text -- is one item: emitting the
        // `yv`/`xv` pair and then dropping the string would leave the cursor
        // moved with nothing drawn.
        Q_snprintf(item, sizeof(item), "yv %d xv %d %s \"%s%s\" ",
                   32 + i * 8, x - ((hnd->cur == i) ? 8 : 0),
                   (hnd->cur == i || alt) ? "string2" : "string",
                   (hnd->cur == i) ? "\x0d" : "", t);

        if (!menu_append(string, sizeof(string), &len, item))
            dropped = true;
        alt = false;
    }

    if (dropped) {
        gi.dprintf("Colosseum: CTF menu exceeded %d bytes; entries were "
                   "dropped whole\n", CTF_MENU_MAX);
    }

    gi.WriteByte(svc_layout);
    gi.WriteString(string);
}

void ctf_PMenu_Update(edict_t *ent)
{
    if (!ent->client->ctf_menu) {
        gi.dprintf("warning:  ent has no menu\n");
        return;
    }

    if (level.time - ent->client->menutime >= 1.0f) {
        // been a second or more since last update, update now
        ctf_PMenu_Do_Update(ent);
        gi.unicast(ent, true);
        ent->client->menutime = level.time;
        ent->client->menudirty = false;
    }
    ent->client->menutime = level.time + 0.2f;
    ent->client->menudirty = true;
}

void ctf_PMenu_Next(edict_t *ent)
{
    ctf_pmenuhnd_t *hnd;
    int i;
    ctf_pmenu_t *p;

    if (!ent->client->ctf_menu) {
        gi.dprintf("warning:  ent has no menu\n");
        return;
    }

    hnd = ent->client->ctf_menu;

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

    ctf_PMenu_Update(ent);
}

void ctf_PMenu_Prev(edict_t *ent)
{
    ctf_pmenuhnd_t *hnd;
    int i;
    ctf_pmenu_t *p;

    if (!ent->client->ctf_menu) {
        gi.dprintf("warning:  ent has no menu\n");
        return;
    }

    hnd = ent->client->ctf_menu;

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

    ctf_PMenu_Update(ent);
}

void ctf_PMenu_Select(edict_t *ent)
{
    ctf_pmenuhnd_t *hnd;
    ctf_pmenu_t *p;

    if (!ent->client->ctf_menu) {
        gi.dprintf("warning:  ent has no menu\n");
        return;
    }

    hnd = ent->client->ctf_menu;

    if (hnd->cur < 0)
        return; // no selectable entries

    p = hnd->entries + hnd->cur;

    if (p->SelectFunc)
        p->SelectFunc(ent, hnd);
}
