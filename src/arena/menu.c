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
#include "g_local.h"
#include "arena/arena.h"

#define MAXMENUITEMS    18

char *
LoPrint(char *string)
{
    int     i;

    if (!string)
        return NULL;

    for (i = 0; i < strlen(string) ; i++)
        if ((unsigned char)string[i] > 0x7f)
            string[i] += 0x80;

    return string;
}

char *
HiPrint(char *string)
{
    int     i;

    if (!string)
        return NULL;

    for (i = 0; i < strlen(string) ; i++)
        if ((unsigned char)string[i] < 0x7f && (unsigned char)string[i] >= 0x20)
            string[i] += 0x80;

    return string;
}

void
SendMenu(edict_t *ent)
{
    gi.WriteByte(svc_configstring);
    gi.WriteShort(CS_STATUSBAR);
    gi.WriteString(ent->client->menutext);
    gi.unicast(ent, false);
}

void
SendStatusBar(edict_t *ent, const char *string, bool transmit)
{
    Q_strlcpy(ent->client->menutext, string, sizeof(ent->client->menutext));
    ent->client->ra_menutime = level.framenum + 1;

    if (transmit) {
        if (ent->client->ra_menutime != level.framenum)
            SendMenu(ent);
        ent->client->ra_menutime = level.framenum;
    } else {
        ent->client->ra_menutime = level.framenum + 1;
    }
}

void
DisplayMenu(edict_t *ent)
{
    gclient_t   *cl;
    menuinfo_t  *info;
    qmenu_t     *node, *selected;
    char        *p;
    int         shown, y;
    char        string[MAXSTATUSBAR];
    char        entry[1000];

    cl = ent->client;

    if (cl->menu_owner != MENU_ARENA) {
        // Put the real bar back.  RA2 chose between two literals here; there
        // are no literals any more, so this is the composed bar for whichever
        // ruleset is running (R-OSP-7a).
        SendStatusBar(ent, G_Statusbar(), true);
        return;
    }

    info = (menuinfo_t *)cl->curmenulink->it;
    selected = cl->selected;

    string[0] = 0;
    Q_strlcpy(string, "xv 32 yv 8 picn inventory ", sizeof(string));

    p = string + strlen(string);
    Q_snprintf(p, sizeof(string) - (p - string),
               "xv 202 yv 12 string2 \"%s\" ", "Menu");
    p = string + strlen(string);
    Q_snprintf(p, sizeof(string) - (p - string),
               "xv 0 yv 24 cstring2 \"%s\" ", info->title);

    p = string + strlen(string);
    shown = count_queue((qmenu_t *)info) - count_queue(selected);
    if (shown > MAXMENUITEMS) {
        node = selected;
        do {
            node = node->prev;
            shown--;
        } while (node != (qmenu_t *)info && (shown % MAXMENUITEMS) != 0);

        Q_strlcpy(p, "xv 50 yv 32 string2 \"(More)\" ",
                  sizeof(string) - (p - string));
    } else {
        node = (qmenu_t *)info;
        Q_strlcpy(p, "xv 50 ", sizeof(string) - (p - string));
    }

    p = string + strlen(string);
    y = 32;
    shown = 0;

    for (;;) {
        if (!node->next)
            break;

        if (shown >= MAXMENUITEMS)
            break;

        node = node->next;
        y += 8;
        shown++;

        entry[0] = 0;

        if (node == selected) {
            Q_strlcat(entry, "\r", sizeof(entry));
            Q_strlcat(entry, LoPrint(((menuitem_t *)node->it)->text), sizeof(entry));

            if (((menuitem_t *)node->it)->value)
                Q_strlcat(entry, ((menuitem_t *)node->it)->value, sizeof(entry));
        } else {
            Q_strlcat(entry, " ", sizeof(entry));
            Q_strlcat(entry, HiPrint(((menuitem_t *)node->it)->text), sizeof(entry));

            if (((menuitem_t *)node->it)->value)
                Q_strlcat(entry, ((menuitem_t *)node->it)->value, sizeof(entry));
        }

        LoPrint(((menuitem_t *)node->it)->text);

        if (((menuitem_t *)node->it)->num >= 0)
            Q_snprintf(entry + strlen(entry), sizeof(entry) - strlen(entry),
                       "%d", ((menuitem_t *)node->it)->num);

        if (strlen(string) + strlen(entry) + 50 >= MAXSTATUSBAR)
            break;

        Q_snprintf(p, sizeof(string) - (p - string),
                   "yv %d string2 \"%s\" ", y, entry);
        p = string + strlen(string);
    }

    if (shown == MAXMENUITEMS && node->next)
        Q_snprintf(p, sizeof(string) - (p - string),
                   "yv %d string2 \"(More)\" ", y + 10);

    SendStatusBar(ent, string, false);
}

qmenu_t *
CreateQMenu(edict_t *ent, char *title)
{
    menuinfo_t  *info;
    qmenu_t     *menu;
    size_t      tlen;

    info = gi.TagMalloc(sizeof(*info), TAG_LEVEL);
    menu = gi.TagMalloc(sizeof(*menu), TAG_LEVEL);
    menu->it = info;

    tlen = strlen(title) + 1;
    info->title = gi.TagMalloc(tlen, TAG_LEVEL);
    memcpy(info->title, title, tlen);
    info->flags = 0;
    info->items = NULL;

    return menu;
}

qmenu_t *
AddMenuItem(qmenu_t *menu, char *text, char *value, int num, menuselect_t select)
{
    menuinfo_t  *info;
    menuitem_t  *item;
    qmenu_t     *node;
    size_t      len;

    node = gi.TagMalloc(sizeof(*node), TAG_LEVEL);
    item = gi.TagMalloc(sizeof(*item), TAG_LEVEL);

    len = strlen(text) + 1;
    item->text = gi.TagMalloc(len, TAG_LEVEL);
    memcpy(item->text, text, len);

    if (value) {
        item->valuesize = strlen(value) + 1;
        item->value = gi.TagMalloc(item->valuesize, TAG_LEVEL);
        memcpy(item->value, value, item->valuesize);
    } else {
        item->valuesize = 0;
        item->value = NULL;
    }

    item->num = num;
    item->select = select;
    node->it = item;

    info = (menuinfo_t *)menu->it;
    add_to_queue(node, (qmenu_t *)info);

    return node;
}

void
FinishMenu(edict_t *ent, qmenu_t *menu, bool show)
{
    ent->client->curmenulink = menu;
    ent->client->selected = ((menuinfo_t *)menu->it)->items;
    // R-MENU-2a: opening goes through the arbiter, which closes whatever else
    // this client had open.  `show` false means "build it but do not display",
    // which is a closed menu as far as the owner field is concerned.
    if (show)
        G_MenuOpen(ent, MENU_ARENA);
    else
        G_MenuClose(ent);

    add_to_queue(menu, &ent->client->menuqueue);

    DisplayMenu(ent);
}

void
MenuNext(edict_t *ent)
{
    if (ent->client->selected->next) {
        ent->client->selected = ent->client->selected->next;

        while (ent->client->selected->next
               && !((menuitem_t *)ent->client->selected->it)->select)
            ent->client->selected = ent->client->selected->next;
    } else
        ent->client->selected =
            ((menuinfo_t *)ent->client->curmenulink->it)->items;

    DisplayMenu(ent);
}

void
MenuPrev(edict_t *ent)
{
    if (ent->client->selected->prev->prev) {
        ent->client->selected = ent->client->selected->prev;

        while (ent->client->selected->prev->prev
               && !((menuitem_t *)ent->client->selected->it)->select)
            ent->client->selected = ent->client->selected->prev;
    } else {
        while (ent->client->selected->next)
            ent->client->selected = ent->client->selected->next;
    }

    DisplayMenu(ent);
}

/*
================
free_menu

Releases one menu: its title block, its items' text and values, the items, the
item nodes, and the node that carries the menu in a queue.  The caller unlinks
that node first.

This was UseMenu's inline teardown and was the only one, so it is lifted out
here for close_menus() to reach -- and lifting it out is what showed the leak.
AddMenuItem makes THREE allocations per row (the qmenu_t node, the menuitem_t,
and its text) and the teardown released two: `node->it`, the menuitem_t itself,
was never freed.  That is one block leaked per row every time anybody closes a
menu by picking a row, which on a Rocket Arena server is how menus normally
close.  Taken from `rocketarena2@28a8af7`.
================
*/
static void free_menu(qmenu_t *menu)
{
    qmenu_t     *node;
    menuitem_t  *item;

    node = (qmenu_t *)menu->it;
    gi.TagFree(node->it);

    while (node->next) {
        node = node->next;

        item = (menuitem_t *)node->it;
        gi.TagFree(item->text);
        if (item->value)
            gi.TagFree(item->value);
        gi.TagFree(item);
        if (node->prev)
            gi.TagFree(node->prev);
    }

    if (node)
        gi.TagFree(node);

    gi.TagFree(menu);
}

void
UseMenu(edict_t *ent, int arg)
{
    qmenu_t     *menu, *item, *qnode;
    bool        orphan;
    int         result;

    if (ent->client->menuusetime + 5 > level.framenum)
        return;
    ent->client->menuusetime = level.framenum;

    menu = ent->client->curmenulink;
    item = ent->client->selected;

    if (!((menuitem_t *)item->it)->select)
        return;

    result = ((menuitem_t *)item->it)->select(ent, menu, item, arg);

    if (result) {
        if (result == 1)
            DisplayMenu(ent);
        return;
    }

    // R-130: A CALLBACK MAY DISCARD THE WHOLE QUEUE, and `menu` above was read
    // BEFORE it ran.  `init_player()` is the tail of two of them -- both "Leave
    // Team" rows -- and it drops the queue rather than walking it, because a
    // reused client slot must not inherit the previous occupant's TAG_LEVEL
    // menus.  The menu captured here is then orphaned with its `prev` still
    // pointing at the queue HEAD, so the unlink below writes
    // `menuqueue.next = menu->next`, which is NULL, and takes the callback's
    // BRAND NEW menu with it.  `curmenulink` ends up NULL, the `inven` reopen
    // tests exactly that field, and the player is left in arena 0 with no menu,
    // unable to rejoin or spawn -- which is what a play test hit and what the
    // donor never could, because its `init_player` does not touch the queue.
    // So ask whether the captured menu is still in the queue before unlinking.
    orphan = true;
    for (qnode = ent->client->menuqueue.next; qnode; qnode = qnode->next) {
        if (qnode == menu) {
            orphan = false;
            break;
        }
    }

    if (!orphan)
        remove_from_queue(menu, &ent->client->menuqueue);

    free_menu(menu);

    // An orphaned menu was the callback's to replace, and it has: `curmenulink`,
    // `selected` and the queue all belong to the menu it built.  Freeing the old
    // one is all this function may do -- the relink below would overwrite that
    // menu with whatever the walk finds, and the walk is what went wrong.
    if (orphan) {
        DisplayMenu(ent);
        return;
    }

    menu = &ent->client->menuqueue;
    while (menu->next)
        menu = menu->next;

    if (menu->it) {
        ent->client->curmenulink = menu;
        ent->client->selected = ((menuinfo_t *)ent->client->curmenulink->it)->items;
    } else {
        ent->client->curmenulink = NULL;
        G_MenuClose(ent);
    }

    DisplayMenu(ent);
}

bool
MenuThink(edict_t *ent)
{
    gclient_t   *cl;

    cl = ent->client;

    if (cl->menu_owner == MENU_ARENA && !((level.framenum - cl->ra_menutime) % 10)) {
        // The repaint is where a menu's live numbers get to be live: SendMenu
        // re-transmits the bar DisplayMenu last composed, so a count that moved
        // has to be composed again before it is sent (see RA_RefreshMenuCounts).
        if (RA_RefreshMenuCounts(ent))
            DisplayMenu(ent);
        SendMenu(ent);
        return true;
    }

    return false;
}

// The engine's own close.  G_MenuClose() calls THIS; nothing here may call
// G_MenuClose(), or the arbiter and the engine recurse into each other.
//
// CLOSING IS HIDING, NOT DESTROYING, and the difference is the whole function.
// RA2 splits the two: `showmenu` is whether the menu is on screen, and
// `curmenulink` + `menuqueue` are the menu itself, which `inven` toggles back
// into view and which FinishMenu(show=false) builds without displaying.  Only
// clear_menus() -- intermission -- destroyed both, and this is not that.
// Tearing the content down here instead made three things unreachable at once:
// `inven` could not reopen the arena menu after `score` had closed it, because
// its reopen tests curmenulink; FinishMenu(ent, m, false) destroyed the menu it
// had just built; and a menu popped off the queue took the rest of the queue
// with it.
//
// What the close MUST do is repaint.  An RA2 menu *is* the client's statusbar,
// drawn by overwriting CS_STATUSBAR for that one client, so a close that does
// not write the real bar back leaves the player looking at a menu the game has
// already forgotten.  G_MenuClose has set menu_owner to MENU_NONE by the time
// this runs, which is what makes DisplayMenu take its "put the real bar back"
// branch.
void
ra_MenuClose(edict_t *ent)
{
    DisplayMenu(ent);
}


/*
================
close_menus

Like clear_menus, but it FREES what it drops instead of merely forgetting it.

clear_menus can afford to forget: it runs from the intermission, where the level
and every TAG_LEVEL allocation behind a menu is over anyway.  PutClientInServer
is the other place the queue head goes away -- it memsets the whole gclient_t
and copies `pers`/`resp` back, and `menuqueue`, `curmenulink` and `selected` are
in neither -- and that one runs on EVERY RESPAWN, so what it drops stays dropped
for the rest of the map.  A menu is open on every one of those, because
PutClientInServer ends in move_to_arena(..., 1) and that reopens the observer
menu.

R-147 already put a close there and it is not enough by itself, which is the
point worth keeping: under arena, closing is HIDING (see ra_MenuClose above), so
the arbiter's close repaints the statusbar and frees nothing.  The repaint is
still its job and still goes through it -- nothing here may call G_MenuClose's
engine row directly (R-MENU-3) -- and the freeing is this function's.

Taken from `rocketarena2@28a8af7`.  RA2's own version tests its `showmenu` bool
to decide whether a repaint is owed; this tree does not carry that field
(sec 7 rule 3), and `menu_owner == MENU_ARENA` is the same question -- a menu
built with FinishMenu(show=false) has never claimed the channel.
================
*/
void
close_menus(edict_t *ent)
{
    qmenu_t *menu;

    if (!ent->client)
        return;

    if (ent->client->menu_owner == MENU_ARENA)
        G_MenuClose(ent);

    while ((menu = remove_from_queue(NULL, &ent->client->menuqueue)) != NULL)
        free_menu(menu);

    ent->client->menuqueue.next = NULL;
    ent->client->menuqueue.prev = NULL;
    ent->client->curmenulink = NULL;
    ent->client->selected = NULL;
}
