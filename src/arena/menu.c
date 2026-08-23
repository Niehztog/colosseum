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
    sprintf(string, "xv 32 yv 8 picn inventory ");

    p = string + strlen(string);
    sprintf(p, "xv 202 yv 12 string2 \"%s\" ", "Menu");
    p = string + strlen(string);
    sprintf(p, "xv 0 yv 24 cstring2 \"%s\" ", info->title);

    p = string + strlen(string);
    shown = count_queue((qmenu_t *)info) - count_queue(selected);
    if (shown > MAXMENUITEMS) {
        node = selected;
        do {
            node = node->prev;
            shown--;
        } while (node != (qmenu_t *)info && (shown % MAXMENUITEMS) != 0);

        sprintf(p, "xv 50 yv 32 string2 \"(More)\" ");
    } else {
        node = (qmenu_t *)info;
        sprintf(p, "xv 50 ");
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
            strcat(entry, "\r");
            strcat(entry, LoPrint(((menuitem_t *)node->it)->text));

            if (((menuitem_t *)node->it)->value)
                strcat(entry, ((menuitem_t *)node->it)->value);
        } else {
            strcat(entry, " ");
            strcat(entry, HiPrint(((menuitem_t *)node->it)->text));

            if (((menuitem_t *)node->it)->value)
                strcat(entry, ((menuitem_t *)node->it)->value);
        }

        LoPrint(((menuitem_t *)node->it)->text);

        if (((menuitem_t *)node->it)->num >= 0)
            sprintf(entry + strlen(entry), "%d", ((menuitem_t *)node->it)->num);

        if (strlen(string) + strlen(entry) + 50 >= MAXSTATUSBAR)
            break;

        sprintf(p, "yv %d string2 \"%s\" ", y, entry);
        p = string + strlen(string);
    }

    if (shown == MAXMENUITEMS && node->next)
        sprintf(p, "yv %d string2 \"(More)\" ", y + 10);

    SendStatusBar(ent, string, false);
}

qmenu_t *
CreateQMenu(edict_t *ent, char *title)
{
    menuinfo_t  *info;
    qmenu_t     *menu;

    info = gi.TagMalloc(sizeof(*info), TAG_LEVEL);
    menu = gi.TagMalloc(sizeof(*menu), TAG_LEVEL);
    menu->it = info;

    info->title = gi.TagMalloc(strlen(title) + 1, TAG_LEVEL);
    strcpy(info->title, title);
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

    node = gi.TagMalloc(sizeof(*node), TAG_LEVEL);
    item = gi.TagMalloc(sizeof(*item), TAG_LEVEL);

    item->text = gi.TagMalloc(strlen(text) + 1, TAG_LEVEL);
    strcpy(item->text, text);

    if (value) {
        item->value = gi.TagMalloc(strlen(value) + 1, TAG_LEVEL);
        strcpy(item->value, value);
    } else {
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

void
UseMenu(edict_t *ent, int arg)
{
    qmenu_t     *menu, *item, *node;
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

    remove_from_queue(menu, &ent->client->menuqueue);

    node = (qmenu_t *)menu->it;
    gi.TagFree(node->it);

    while (node->next) {
        node = node->next;

        gi.TagFree(((menuitem_t *)node->it)->text);
        if (((menuitem_t *)node->it)->value)
            gi.TagFree(((menuitem_t *)node->it)->value);
        if (node->prev)
            gi.TagFree(node->prev);
    }

    if (node)
        gi.TagFree(node);

    gi.TagFree(menu);

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
        SendMenu(ent);
        return true;
    }

    return false;
}

// The engine's own teardown.  G_MenuClose() calls THIS; nothing here may call
// G_MenuClose(), or the arbiter and the engine recurse into each other.  The
// queue nodes are TAG_LEVEL, so dropping the list is not a leak past the level
// -- which is the reasoning the donor recorded for the same code.
void
ra_MenuClose(edict_t *ent)
{
    ent->client->curmenulink = NULL;
    ent->client->selected = NULL;
    ent->client->menuqueue.next = NULL;
}

void
clear_menus(edict_t *ent)
{
    G_MenuClose(ent);       // clears the owner field and calls ra_MenuClose
    DisplayMenu(ent);       // and puts the composed bar back
}

