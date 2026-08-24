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
// The Gladiator Bot menu engine (R-BOT-28, R-MENU-1), from
// gladiator-bot-restored@game.  The two reconstructions ship byte-identical
// copies, so there is one source of truth.
//===========================================================================
//
// Name:                p_menulib.c
// Function:        menu
// Programmer:      Mr Elusive (MrElusive@demigod.demon.nl)
// Last update: 1998-01-12
// Tab Size:        3
//===========================================================================

#include "g_local.h"
#include "bot/p_menulib.h"
#include "bot/p_botmenu.h"

#define MAX_DISPLAYEDMENUITEMS  16
#define MENUITEMTEXT_YOFFSET    48
#define MENUITEMTEXT_XOFFSET    66
#define MENUCHANGE_MOVE         1

// R-MENU-2a: "is this menu up" is menu_owner, not a boolean of this engine's
// own.  Every `menustate->showmenu` test in the donor is this.
static bool bot_MenuUp(edict_t *ent)
{
    return ent->client && ent->client->menu_owner == MENU_BOT;
}

//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
bot_menuitem_t *bot_MenuItemWithId(bot_menu_t *menu, int id)
{
    bot_menuitem_t *mi, *found;

    if (!menu) return NULL;
    for (mi = menu->firstmenuitem; mi; mi = mi->next)
    {
        if (mi->id == id) return mi;
    } //end for
    for (mi = menu->firstmenuitem; mi; mi = mi->next)
    {
        if (mi->type == MI_SUBMENU)
        {
            if (mi->submenu)
            {
                found = bot_MenuItemWithId(mi->submenu, id);
                if (found) return found;
            } //end if
        } //end if
    } //end for
    return NULL;
} //end of the function bot_MenuItemWithId
//========================================================================
// returns the menu with the given id
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
static bot_menu_t *GetMenuWithId(bot_menu_t *mainmenu, int id)
{
    bot_menuitem_t *mi;
    bot_menu_t *found;

    if (!mainmenu) return NULL;
    if (mainmenu->id == id) return mainmenu;
    for (mi = mainmenu->firstmenuitem; mi; mi = mi->next)
    {
        if (mi->type == MI_SUBMENU)
        {
            if (mi->submenu)
            {
                found = GetMenuWithId(mi->submenu, id);
                if (found) return found;
            } //end if
        } //end if
    } //end for
    return NULL;
} //end of the function GetMenuWithId
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
static bot_menu_t *GetCurrentMenu(menustate_t *menustate)
{
    bot_menu_t *menu;

    menu = GetMenuWithId(menustate->mainmenu, menustate->menuid);
    if (!menu) return menustate->mainmenu;
    return menu;
} //end of the function GetCurrentMenu
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
void bot_MenuItemRename(bot_menu_t *menu, int id, const char *name)
{
    bot_menuitem_t *mi;

    mi = bot_MenuItemWithId(menu, id);
    if (!mi) return;

    // Q_strlcpy, not strncpy: strncpy(dst, src, MAX_MENU_NAME) leaves the
    // string unterminated when the name is exactly MAX_MENU_NAME long, and the
    // names here are built by sprintf("%-18s%s") into a 128-byte buffer.
    Q_strlcpy(mi->name, name, sizeof(mi->name));
} //end of the function bot_MenuItemRename
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
char *bot_MenuItemName(bot_menu_t *menu, int id)
{
    bot_menuitem_t *mi;

    mi = bot_MenuItemWithId(menu, id);
    if (!mi) return NULL;
    return mi->name;
} //end of the function bot_MenuItemName
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
bot_menuitem_t *bot_MenuItemAt(bot_menu_t *menu, int item)
{
    bot_menuitem_t *i;

    if (!menu) return NULL;
    for (i = menu->firstmenuitem; i; i = i->next)
    {
        if (--item < 0)
        {
            return i;
        } //end if
    } //end for
    return NULL;
} //end of the function bot_MenuItemAt
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
bot_menu_t *bot_MenuTreeCreate(int id, const char *title, const char *background)
{
    bot_menu_t *m;
    unsigned char *p;

    m = gi.TagMalloc(sizeof(bot_menu_t), TAG_GAME);
    memset(m, 0, sizeof(bot_menu_t));
    m->id = id;
    Q_strlcpy(m->title, title, sizeof(m->title));
    //make the title green.  Through unsigned char: `*title += 128` on a plain
    //char is signed overflow, which -fwrapv makes defined and -Wall would
    //rather not see at all.
    for (p = (unsigned char *)m->title; *p; p++) *p += 128;
    //
    if (background) Q_strlcpy(m->background, background, sizeof(m->background));
    m->nummenuitems = 0;
    return m;
} //end of the function bot_MenuTreeCreate
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
void bot_MenuTreeDelete(bot_menu_t *menu)
{
    bot_menuitem_t *mi;

    if (!menu) return;
    for (mi = menu->firstmenuitem; mi; mi = menu->firstmenuitem)
    {
        if (mi->type == MI_SUBMENU) bot_MenuTreeDelete(mi->submenu);
        menu->firstmenuitem = mi->next;
        gi.TagFree(mi);
    } //end for
    gi.TagFree(menu);
} //end of the function bot_MenuTreeDelete
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
void bot_MenuAppend(bot_menu_t *menu, unsigned char type, int id,
                    bot_menu_t *submenu, const char *name, const char *bitmap)
{
    bot_menuitem_t *mi;

    mi = gi.TagMalloc(sizeof(bot_menuitem_t), TAG_GAME);
    // sizeof(bot_menuitem_t), not sizeof(bot_menu_t).  The donor clears a bot_menuitem_t
    // with the size of a bot_menu_t, which is the smaller of the two -- so the tail
    // of every menu item starts as whatever the allocator last had there, and
    // `bitmap` in particular is only assigned when the caller passes one.
    memset(mi, 0, sizeof(bot_menuitem_t));

    mi->type = type;
    mi->id = id;
    mi->menu = menu;        //the menu the item is in
    mi->submenu = submenu;
    if (submenu) submenu->parent = menu;
    if (name) Q_strlcpy(mi->name, name, sizeof(mi->name));
    if (bitmap) Q_strlcpy(mi->bitmap, bitmap, sizeof(mi->bitmap));

    mi->prev = menu->lastmenuitem;
    if (menu->lastmenuitem) menu->lastmenuitem->next = mi;
    else menu->firstmenuitem = mi;
    menu->lastmenuitem = mi;
    mi->next = NULL;

    menu->nummenuitems++;
} //end of the function bot_MenuAppend
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
void bot_MenuRemoveItem(bot_menu_t *menu, int id)
{
    bot_menuitem_t *mi;

    mi = bot_MenuItemWithId(menu, id);
    if (!mi) return;

    if (mi->prev) mi->prev->next = mi->next;
    else mi->menu->firstmenuitem = mi->next;
    if (mi->next) mi->next->prev = mi->prev;
    else mi->menu->lastmenuitem = mi->prev;
    //
    mi->menu->nummenuitems--;
    // The donor unlinks and leaks.  The item is TAG_GAME, so it survives the
    // level change that would otherwise clean up after it, and the add/remove
    // submenus are rebuilt every time they are entered.
    gi.TagFree(mi);
} //end of the function bot_MenuRemoveItem
//========================================================================
// Sends a LAYOUT, not a statusbar.  svc_layout is the channel the scoreboard
// and the inventory use, which is why opening this menu sets showscores and
// closing it clears it -- and why closing needs no repaint, unlike RA2's menu,
// which really does overwrite CS_STATUSBAR (doc/reconciliation.md R-87).
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
void bot_SendLayout(edict_t *ent, const char *layout)
{
    gi.WriteByte(svc_layout);
    gi.WriteString(layout);
    gi.unicast(ent, false);
} //end of the function bot_SendLayout
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
static void bot_DisplayMenu(edict_t *ent, bot_menu_t *menu, int highlighteditem,
                            int firstdisplayeditem)
{
    int i, yoffset;
    char menustring[MAX_STRING_CHARS];
    size_t len = 0;
    bot_menuitem_t *menuitem;

    if (!menu) return;
    menustring[0] = '\0';
    //background pic
    if (menu->background[0])
        len += Q_scnprintf(menustring + len, sizeof(menustring) - len,
                           "xv 32 yv 4 picn %s ", menu->background);
    //menu title
    len += Q_scnprintf(menustring + len, sizeof(menustring) - len,
                       "xv 0 yv 24 cstring \"%s\" ", menu->title);
    //first menu item
    menuitem = menu->firstmenuitem;
    //go to the first menu item.  The donor walks `next` firstdisplayeditem
    //times without testing it; a scrolled menu that shrinks -- which is every
    //`remove bot` -- walks off the end.
    for (i = 0; i < firstdisplayeditem && menuitem; i++) menuitem = menuitem->next;
    //
    yoffset = MENUITEMTEXT_YOFFSET;
    if (firstdisplayeditem)
    {
        len += Q_scnprintf(menustring + len, sizeof(menustring) - len,
                           "xv %d yv %d string \"... more ...\"",
                           MENUITEMTEXT_XOFFSET, MENUITEMTEXT_YOFFSET - 8);
    } //end if
    //show the menu items
    for (i = 0; i < MAX_DISPLAYEDMENUITEMS && menuitem; i++)
    {
        if (firstdisplayeditem + i == highlighteditem)
        {
            len += Q_scnprintf(menustring + len, sizeof(menustring) - len,
                               "xv %d yv %d string \"%c\"",
                               MENUITEMTEXT_XOFFSET - 16, yoffset, 13 + 128);
        } //end if
        //print the menu option
        len += Q_scnprintf(menustring + len, sizeof(menustring) - len,
                           "xv %d yv %d string \"%-16s\" ",
                           MENUITEMTEXT_XOFFSET, yoffset, menuitem->name);
        //
        if (menuitem->type == MI_SUBMENU)
        {
            len += Q_scnprintf(menustring + len, sizeof(menustring) - len,
                               "xv %d yv %d string \"%c\"",
                               MENUITEMTEXT_XOFFSET + 128, yoffset, 13);
        } //end if
        //
        menuitem = menuitem->next;
        yoffset += 8;
    } //end for
    if (menuitem)
    {
        len += Q_scnprintf(menustring + len, sizeof(menustring) - len,
                           "xv %d yv %d string \"... more ...\"",
                           MENUITEMTEXT_XOFFSET,
                           MENUITEMTEXT_YOFFSET + MAX_DISPLAYEDMENUITEMS * 8);
    } //end if
    //keep the show loading image if present
    if (ent->client->showloading)
    {
        Q_scnprintf(menustring + len, sizeof(menustring) - len,
                    "xv 104 yv 128 picn loading");
    } //end if
    bot_SendLayout(ent, menustring);
} //end of the function bot_DisplayMenu
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
static void BoundHighlightedItem(menustate_t *menustate, bot_menu_t *menu)
{
    bot_menuitem_t *mi;

    if (!menu || menu->nummenuitems <= 0)
    {
        menustate->highlighteditem = 0;
        menustate->firstdisplayeditem = 0;
        return;
    } //end if
    if (menustate->highlighteditem >= menu->nummenuitems)
    {
        menustate->highlighteditem = 0;
    } //end if
    if (menustate->highlighteditem < 0)
    {
        menustate->highlighteditem = menu->nummenuitems - 1;
    } //end if
    while((mi = bot_MenuItemAt(menu, menustate->highlighteditem)) != NULL)
    {
        if (mi->type == MI_SEPERATOR) menustate->highlighteditem++;
        else break;
        if (menustate->highlighteditem >= menu->nummenuitems)
        {
            menustate->highlighteditem = 0;
            break;
        } //end if
    } //end while
    if (menustate->firstdisplayeditem >= menu->nummenuitems)
    {
        menustate->firstdisplayeditem = 0;
    } //end if
    if (menustate->firstdisplayeditem < 0)
    {
        menustate->firstdisplayeditem = menu->nummenuitems - 1;
    } //end if
} //end of the function BoundHighlightedItem
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
static int MenuForward(edict_t *ent)
{
    bot_menuitem_t *mi;
    menustate_t *menustate;
    bot_menu_t *menu;

    menustate = &ent->client->menustate;
    if (!bot_MenuUp(ent)) return false;
    menu = GetCurrentMenu(menustate);
    if (!menu) return false;
    BoundHighlightedItem(menustate, menu);
    mi = bot_MenuItemAt(menu, menustate->highlighteditem);
    if (mi)
    {
        // The proc may close the menu (MID_EXIT does), and it may rebuild the
        // submenu it is about to enter (MID_BOT_ADD does).  Both invalidate
        // `mi`, so its submenu is read back through the id rather than through
        // the pointer -- the donor keeps using `mi` and enters a freed tree.
        int id = mi->id;
        unsigned char type = mi->type;

        menustate->menuproc(ent, id);
        if (!bot_MenuUp(ent)) return true;
        menu = GetCurrentMenu(menustate);
        if (type == MI_SUBMENU)
        {
            mi = bot_MenuItemWithId(menustate->mainmenu, id);
            if (mi && mi->submenu)
            {
                menustate->menuid = mi->submenu->id;
                menustate->highlighteditem = 0;
                menustate->firstdisplayeditem = 0;
                menu = mi->submenu;
            } //end if
        } //end if
    } //end if
    BoundHighlightedItem(menustate, menu);
    menustate->redrawmenu = true;
    return true;
} //end of the function MenuForward
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
int bot_MenuBack(edict_t *ent)
{
    int i;
    bot_menuitem_t *mi;
    menustate_t *menustate;
    bot_menu_t *menu;

    menustate = &ent->client->menustate;
    if (!bot_MenuUp(ent)) return false;
    menu = GetCurrentMenu(menustate);
    if (!menu) return false;
    if (menu->parent)
    {
        for (i = 0, mi = menu->parent->firstmenuitem; mi; mi = mi->next, i++)
        {
            if (mi->submenu == menu)
            {
                menustate->highlighteditem = i;
                menustate->firstdisplayeditem = 0;
                if (menu->parent->nummenuitems >= MAX_DISPLAYEDMENUITEMS)
                {
                    menustate->firstdisplayeditem = i;
                } //end if
                break;
            } //end if
        } //end for
        menustate->menuid = menu->parent->id;
        menu = menu->parent;
    } //end if
    BoundHighlightedItem(menustate, menu);
    menustate->redrawmenu = true;
    return true;
} //end of the function bot_MenuBack
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
static int MenuDown(edict_t *ent)
{
    bot_menuitem_t *mi;
    menustate_t *menustate;
    bot_menu_t *menu;

    menustate = &ent->client->menustate;
    if (!bot_MenuUp(ent)) return false;
    menu = GetCurrentMenu(menustate);
    if (!menu || menu->nummenuitems <= 0) return false;
    menustate->highlighteditem++;
    if (menustate->highlighteditem >= menu->nummenuitems)
    {
        menustate->highlighteditem = 0;
        menustate->firstdisplayeditem = 0;
    } //end if
    while((mi = bot_MenuItemAt(menu, menustate->highlighteditem)) != NULL)
    {
        if (mi->type == MI_SEPERATOR) menustate->highlighteditem++;
        else break;
        if (menustate->highlighteditem >= menu->nummenuitems)
        {
            menustate->highlighteditem = 0;
            menustate->firstdisplayeditem = 0;
            break;
        } //end if
    } //end while
    if (menustate->highlighteditem - menustate->firstdisplayeditem >= MAX_DISPLAYEDMENUITEMS)
    {
        menustate->firstdisplayeditem = menustate->highlighteditem - MAX_DISPLAYEDMENUITEMS + 1;
    } //end if
    BoundHighlightedItem(menustate, menu);
    menustate->redrawmenu = true;
    return true;
} //end of the function MenuDown
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
static int MenuUp(edict_t *ent)
{
    bot_menuitem_t *mi;
    menustate_t *menustate;
    bot_menu_t *menu;

    menustate = &ent->client->menustate;
    if (!bot_MenuUp(ent)) return false;
    menu = GetCurrentMenu(menustate);
    if (!menu || menu->nummenuitems <= 0) return false;
    menustate->highlighteditem--;
    if (menustate->highlighteditem < 0)
    {
        menustate->highlighteditem = menu->nummenuitems - 1;
        menustate->firstdisplayeditem = menustate->highlighteditem - MAX_DISPLAYEDMENUITEMS + 1;
        if (menustate->firstdisplayeditem < 0) menustate->firstdisplayeditem = 0;
    } //end if
    while((mi = bot_MenuItemAt(menu, menustate->highlighteditem)) != NULL)
    {
        if (mi->type == MI_SEPERATOR) menustate->highlighteditem--;
        else break;
        if (menustate->highlighteditem < 0)
        {
            menustate->highlighteditem = menu->nummenuitems - 1;
            menustate->firstdisplayeditem = menustate->highlighteditem - MAX_DISPLAYEDMENUITEMS + 1;
            if (menustate->firstdisplayeditem < 0) menustate->firstdisplayeditem = 0;
            break;
        } //end if
    } //end while
    if (menustate->highlighteditem - menustate->firstdisplayeditem < 0)
    {
        menustate->firstdisplayeditem = menustate->highlighteditem;
    } //end if
    BoundHighlightedItem(menustate, menu);
    menustate->redrawmenu = true;
    return true;
} //end of the function MenuUp

// R-MENU-4's three, so the inventory keys reach this engine the way they reach
// the other three.
void bot_MenuNext(edict_t *ent)   { MenuDown(ent); }
void bot_MenuPrev(edict_t *ent)   { MenuUp(ent); }
void bot_MenuSelect(edict_t *ent) { MenuForward(ent); }

//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
void bot_MenuThink(edict_t *ent, usercmd_t *ucmd)
{
    menustate_t *menustate;
    bot_menu_t *menu;

    menustate = &ent->client->menustate;

    if (!bot_MenuUp(ent)) return;
    menu = GetCurrentMenu(menustate);
    if (!menu) return;
    //
    if ((ucmd->forwardmove || ucmd->sidemove)
            && (menustate->lastchange_time + 0.21f < level.time))
    {
        menustate->lastchange_time = level.time;
        //if activate menu item
        if (ucmd->sidemove > MENUCHANGE_MOVE) MenuForward(ent);
        //if go back to parent menu
        else if (ucmd->sidemove < -MENUCHANGE_MOVE) bot_MenuBack(ent);
        //if go to prev menu item
        else if (ucmd->forwardmove > MENUCHANGE_MOVE) MenuUp(ent);
        //if go to next menu item
        else if (ucmd->forwardmove < -MENUCHANGE_MOVE) MenuDown(ent);
        //the proc may have closed the menu
        if (!bot_MenuUp(ent)) return;
    } //end if
    else
    {
        if (!(level.framenum & 15))
        {
            menustate->redrawmenu = true;
        } //end if
    } //end else
    menu = GetCurrentMenu(menustate);
    BoundHighlightedItem(menustate, menu);
    //clear the velocity so the menu option pointer won't get jumpy
    VectorClear(ent->velocity);
    ent->client->ps.pmove.pm_type = PM_DEAD;
    //clear the use and fire buttons
    ucmd->buttons &= ~(BUTTON_ATTACK | BUTTON_USE);
    ucmd->forwardmove = 0;
    ucmd->sidemove = 0;
    ucmd->upmove = 0;
    //NOTE: this is necessary for the chasecam!!
    //      otherwise the camera keeps sinking
    ent->client->ps.pmove.gravity = 0;
} //end of the function bot_MenuThink
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
void bot_MenuShow(edict_t *ent)
{
    menustate_t *menustate;
    bot_menu_t *menu;

    if (!bot_MenuUp(ent)) return;
    menustate = &ent->client->menustate;
    if (menustate->redrawmenu)
    {
        menu = GetCurrentMenu(menustate);
        bot_DisplayMenu(ent, menu, menustate->highlighteditem,
                        menustate->firstdisplayeditem);
        menustate->redrawmenu = false;
    } //end if
    // The donor's `else if (removemenu)` arm -- repaint dm_statusbar or
    // single_statusbar -- is commented out in the reconstruction and is not
    // restored: this engine writes a LAYOUT, and a layout stops being drawn
    // when showscores goes false.  bot_MenuClose is what clears it, and
    // G_LayoutClear is the one place that knows how (R-87's lesson, applied
    // before the defect rather than after).
} //end of the function bot_MenuShow
