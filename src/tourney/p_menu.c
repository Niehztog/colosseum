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
//
// THE SAME THREE DEPARTURES src/ctf/p_menu.c MAKES, MADE HERE TOO (1.31).  The
// two files are forks of one engine (see p_menu.h), so CTF's copy is not a
// model this one is being rewritten towards -- it is this one, three fixes
// later.  R-MENU-1 keeps the two engines apart because the *menus* are part of
// the ruleset a player is choosing; it does not ask the transport underneath
// them to stay broken in one of the two.  Each departure is a requirement, not
// taste:
//
//  1. THE BUILDER IS BOUNDED (R-MENU-5).  The donor writes into a
//     `char string[1400]` with `sprintf(string + strlen(string), ...)` and no
//     check at all.  Colosseum's copy had already been given `Q_snprintf`,
//     which cannot overrun -- but a snprintf that runs out of room stops
//     wherever it happens to be, and "wherever it happens to be" is the middle
//     of a token: a half-written `xv 6`, or a `"` that never closes.
//     R-MENU-5 wants truncation "on a whole item, never mid-token", so each
//     entry is composed into its own scratch buffer and appended only if it
//     fits whole.  With today's tables every line is capped at 31 characters
//     by the statics in osp_menus.c and the total lands near 1300, so this is
//     the requirement being met before a longer table reaches it rather than
//     after.
//
//  2. THE ENTRIES ARE COPIED PER CLIENT.  This is the one that was doing
//     damage.  The donor points the handle straight at the caller's array --
//     and osp_menus.c's thirteen tables are FILE-SCOPE GLOBALS whose text
//     points at file-scope statics, restaged from one client's `resp` state by
//     an OSP_update*Menu() immediately before the render.  That works only
//     while the two run back to back, and osp_PMenu_Next/Prev skipped the
//     restage entirely: a client moving the cursor redrew whatever the LAST
//     client to touch the table had staged.  Two players in the voting menu
//     showed each other's pending timelimit while proposing their own, and the
//     admin row's SelectFunc -- the only gate between a player and the
//     kick/ban menu, since nothing downstream re-checks `osp_e39c` -- is
//     written from the same globals.  Open now deep-copies the rows and their
//     strings, so what a client sees is the client's own.
//
//  3. THE REDRAW IS RATE-LIMITED (R-MENU-4).  The donor rebuilds and unicasts
//     ~1300 reliable bytes on every cursor keypress.  Update() now marks the
//     menu dirty and ClientThink flushes at the engine's cadence, which is
//     what CTF's half of R-MENU-4 has done since 1.12 and what the shared
//     `menutime`/`menudirty` pair in gclient_t is for -- one menu is open at a
//     time (R-MENU-3), so one pair of fields answers for whichever engine owns
//     it.
//
#include "g_local.h"
#include "tourney/osp_types.h"

// The layout channel's budget.  Same 1400 as the statusbar (g_stats.h) and the
// same reason: it is what svc_layout carries.
#define OSP_MENU_MAX    MAX_STATUSBAR

// R-MENU-5.  One whole item at a time; an item that does not fit is dropped and
// the caller is told once, rather than the buffer being run past its end.
//
// ctf/p_menu.c has this function too, byte for byte, and it stays duplicated:
// both are static, R-MENU-1 keeps the two engines in separate translation
// units on purpose, and a shared helper would be the one thread between them
// that a future change to either could pull.
static bool menu_append(char *string, size_t size, size_t *len, const char *item)
{
    size_t n = strlen(item);

    if (*len + n >= size)
        return false;
    memcpy(string + *len, item, n + 1);
    *len += n;
    return true;
}

// Note that the pmenu entries are duplicated: the static tables in osp_menus.c
// can then be used for several clients and edited per client without
// interference.  Departure 2 above is what this loop is.
void osp_PMenu_Open(edict_t *ent, const osp_pmenu_t *entries, int cur, int num)
{
    osp_pmenuhnd_t *hnd;
    const osp_pmenu_t *p;
    int i;

    if (!ent->client)
        return;

    // R-MENU-3, and the reason the donor's "warning, ent already has a menu"
    // dprintf is gone: the incumbent is closed by the one open path, whichever
    // engine owned it, so having one open is normal rather than notable.
    G_MenuOpen(ent, MENU_TOURNEY);

    // gi.TagMalloc, not malloc: the free below is gi.TagFree and the pair has
    // to match (R-55).  TAG_LEVEL because a menu does not outlive its level.
    hnd = gi.TagMalloc(sizeof(*hnd), TAG_LEVEL);

    hnd->entries = gi.TagMalloc(sizeof(osp_pmenu_t) * num, TAG_LEVEL);
    memcpy(hnd->entries, entries, sizeof(osp_pmenu_t) * num);
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
    ent->client->osp_menu = hnd;

    osp_PMenu_Do_Update(ent);
    gi.unicast(ent, true);
}

// The engine's own teardown.  G_MenuClose() calls THIS; nothing here may call
// G_MenuClose(), or the arbiter and the engine recurse into each other.
void osp_PMenu_Close(edict_t *ent)
{
    int i;
    osp_pmenuhnd_t *hnd;

    if (!ent->client->osp_menu)
        return;

    hnd = ent->client->osp_menu;
    for (i = 0; i < hnd->num; i++)
        if (hnd->entries[i].text)
            gi.TagFree(hnd->entries[i].text);
    gi.TagFree(hnd->entries);
    gi.TagFree(hnd);
    ent->client->osp_menu = NULL;
    ent->client->showscores = false;
    // A redraw this menu earned but never got is not owed to the next one.
    // The flush in ClientThink checks the owner and would skip it anyway; this
    // is so the two forks of the engine say the same thing.
    ent->client->menudirty = false;

    // R-MENU-3, and the same argument ctf/p_menu.c writes out: osp_menus.c
    // closes its own menus in 38 places -- every leaf that does something and
    // dismisses -- so the owner has to be released HERE as well as in
    // G_MenuClose.  Leaving it set makes G_MenuActive() true with a NULL
    // handle, and every OSP_*Menu() entry point is written
    // `if (owner == MENU_TOURNEY) close; else open`, so the next press closed a
    // menu that was already gone and the menu could be opened exactly once per
    // life.  Setting it twice is harmless; setting it in one of the two paths
    // is not.
    ent->client->menu_owner = MENU_NONE;

    gi.WriteByte(svc_layout);
    gi.WriteString("xv 0 yv 0 string \" \"");
    gi.unicast(ent, true);
}

// only use on menus that have been opened with osp_PMenu_Open.  Unlike CTF's,
// this one takes a NULL text: OSP's tables use blank rows as spacing and half
// of every table is one.
void osp_PMenu_UpdateEntry(osp_pmenu_t *entry, const char *text, int align,
                           void (*SelectFunc)(edict_t *ent, struct osp_pmenu_s *entry))
{
    if (entry->text)
        gi.TagFree(entry->text);
    entry->text = text ? G_CopyString((char *)text) : NULL;
    entry->align = align;
    entry->SelectFunc = SelectFunc;
}

/*
================
osp_PMenu_Sync

Re-copy a template into this client's private rows.

Departure 2 gave each client its own copy, which is what stops one client's
menu being redrawn from another's staged text -- but it also cuts the
OSP_update*Menu() builders off from the rows they are meant to be updating.
They still compose into the file statics in osp_menus.c, because thirteen
tables' worth of `Menu[7].text = tm_admin;` is the shape that file is, and
turning all of it into osp_PMenu_UpdateEntry() calls would be a rewrite of
osp_menus.c rather than a fix to the engine.  So the template keeps its job as
the STAGING AREA and this function takes the copy: after it returns, nothing
another client does to the globals can be seen by this one.

SILENT WHEN NO MENU IS OPEN, and that is the interesting half.  The builders
are called from two places -- the openers, which stage and then call
osp_PMenu_Open (which takes its own copy), and the leaves, which restage while
the menu is up.  Only the leaves need this, and they are exactly the fourteen
sites that used to end `osp_PMenu_Update(ent); gi.unicast(ent, true);`.  The
guard is what lets a caller not have to know which of the two it is: an opener
that grows a Sync later is still correct, because before the open there is
simply nothing to refresh.  A dprintf here would fire on every menu the mod
opens.

The template must be the one this menu was opened with; `hnd->num` is the
length, so a shorter table would be read past its end.  Each builder writes
exactly one table -- checked, it is 1:1 across all ten -- and each leaf syncs
the table its builder wrote, which is what makes that hold.
================
*/
void osp_PMenu_Sync(edict_t *ent, const osp_pmenu_t *entries)
{
    osp_pmenuhnd_t *hnd;
    int i;

    if (!ent->client || !ent->client->osp_menu)
        return;

    hnd = ent->client->osp_menu;

    for (i = 0; i < hnd->num; i++) {
        osp_PMenu_UpdateEntry(hnd->entries + i, entries[i].text,
                              entries[i].align, entries[i].SelectFunc);
        hnd->entries[i].arg = entries[i].arg;
    }
}

void osp_PMenu_Do_Update(edict_t *ent)
{
    char string[OSP_MENU_MAX];
    char item[128];
    size_t len;
    int i;
    osp_pmenu_t *p;
    int x;
    osp_pmenuhnd_t *hnd;
    char *t;
    bool alt = false;
    bool dropped = false;

    if (!ent->client->osp_menu) {
        gi.dprintf("warning:  ent has no menu\n");
        return;
    }

    hnd = ent->client->osp_menu;

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

        if (p->align == osp_PMENU_ALIGN_CENTER)
            x = 196 / 2 - strlen(t) * 4 + 60;
        else if (p->align == osp_PMENU_ALIGN_RIGHT)
            x = 60 + (212 - strlen(t) * 8);
        else
            x = 60;

        // The whole entry -- position and text -- is one item: emitting the
        // `yv`/`xv` pair and then dropping the string would leave the cursor
        // moved with nothing drawn.
        //
        // The donor's four-way if/else chain says exactly this: the arrow is
        // on the cursor row, and the font is `string2` when the cursor row and
        // the leading `*` DISAGREE.  Note that this is an XOR and CTF's is an
        // OR -- the two forks differ on the one case where a starred row is
        // also the cursor row, and this one is OSP's answer, not a
        // simplification of it.
        Q_snprintf(item, sizeof(item), "yv %d xv %d %s \"%s%s\" ",
                   32 + i * 8, x - ((hnd->cur == i) ? 8 : 0),
                   ((hnd->cur == i) != alt) ? "string2" : "string",
                   (hnd->cur == i) ? "\x0d" : "", t);

        if (!menu_append(string, sizeof(string), &len, item))
            dropped = true;
        alt = false;
    }

    if (dropped) {
        gi.dprintf("Colosseum: tourney menu exceeded %d bytes; entries were "
                   "dropped whole (R-MENU-5)\n", OSP_MENU_MAX);
    }

    gi.WriteByte(svc_layout);
    gi.WriteString(string);
}

// R-MENU-4's cadence.  The donor composed and unicast ~1300 reliable bytes here
// on every keypress; this defers to ClientThink, which flushes at most five
// times a second and forces one through after a second of silence.
void osp_PMenu_Update(edict_t *ent)
{
    if (!ent->client->osp_menu) {
        gi.dprintf("warning:  ent has no menu\n");
        return;
    }

    if (level.time - ent->client->menutime >= 1.0f) {
        // been a second or more since last update, update now
        osp_PMenu_Do_Update(ent);
        gi.unicast(ent, true);
        ent->client->menutime = level.time;
        ent->client->menudirty = false;
    }
    ent->client->menutime = level.time + 0.2f;
    ent->client->menudirty = true;
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
}

// R-OSP-13.  `invuse` and `invdrop` are one key to an OSP menu; the difference
// between them is `resp.osp_r264`, which every settings row reads as its step
// direction (`osp_r290--` against `osp_r290++`) and the kick/player list reads
// as its scan direction.  The merge carried only `invuse` and never set the
// flag, so nine branches in osp_menus.c stood on their false side for good and
// a value could be stepped up but never down.
//
// `osp_r010` is the donor's two-frame debounce, and a key inside it is EATEN
// rather than passed on -- the donor returns either way, so a held key does not
// fall through to the inventory underneath.
void OSP_menuSelect(edict_t *ent, bool reverse)
{
    gclient_t *cl = ent->client;

    if (cl->resp.osp_r010 > level.framenum)
        return;

    cl->resp.osp_r010 = level.framenum + 2;
    cl->resp.osp_r264 = reverse;

    osp_PMenu_Select(ent);
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

    // A callback may close this menu, which frees the array `p` points into
    // (p_menu.c departure 2).  Nothing here may touch `p` after the call --
    // and the leaves that need a field of it must read it before they close.
    if (p->SelectFunc)
        p->SelectFunc(ent, p);
}
