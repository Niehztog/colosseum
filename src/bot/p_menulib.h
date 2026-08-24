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
// The Gladiator Bot menu engine, from gladiator-bot-restored@game (SPECS.md
// sec 5.2, R-BOT-28).  osp-tourney cannot supply this: it moved its bot menu
// into osp_menus.c on id's PMenu and ships neither p_menulib.c nor
// p_botmenu.c.  The two reconstructions' copies are byte-identical, so there
// is one source of truth.
//
// THE FOURTH MENU ENGINE, AND WHY EVERY EXPORTED NAME IS PREFIXED.
// R-MENU-1 ships four donor engines over one client input channel and
// R-MENU-2a puts one arbiter over all of them.  `SendStatusBar` and
// `DisplayMenu` already exist in src/arena/menu.h with different signatures, so
// sec 7 rule 4 applies: the donor prefix goes on.  `bot_SendLayout` also
// corrects a misnomer -- the Gladiator "status bar" is an svc_layout unicast,
// the same channel as the scoreboard, NOT the CS_STATUSBAR that RA2's menu
// overwrites.  That difference is why closing this menu needs no repaint
// (doc/reconciliation.md R-87) and why `showscores` is what gates its drawing.
//===========================================================================
//
// Name:         p_menulib.h
// Function:     menu
// Programmer:   Mr Elusive (MrElusive@demigod.demon.nl)
// Last update:  1998-01-12
// Tab Size:     3
//===========================================================================

#ifndef BOT_P_MENULIB_H
#define BOT_P_MENULIB_H

#define MAX_MENU_NAME       32

#define MI_ITEM             1
#define MI_SUBMENU          2
#define MI_SEPERATOR        3

typedef struct bot_menuitem_s {
    unsigned char type;                 // type of menu item
    char name[MAX_MENU_NAME];           // name of the menu item
    char bitmap[MAX_MENU_NAME];         // name of the menu item bitmap
    int id;                             // menu item id
    struct bot_menu_s *menu;                // menu the item is in
    struct bot_menu_s *submenu;             // submenu if it is a submenu
    struct bot_menuitem_s *prev, *next;     // next and prev item in menu
} bot_menuitem_t;

typedef struct bot_menu_s {
    int id;                             // menu id
    char title[MAX_MENU_NAME];          // title of the menu
    char background[MAX_MENU_NAME];     // background picture
    unsigned char type;                 // menu type
    struct bot_menu_s *parent;              // menu id of the parent
    short nummenuitems;                 // number of menu items
    struct bot_menuitem_s *firstmenuitem, *lastmenuitem;
} bot_menu_t;

// `menustate_t` needs no prefix: nothing else in the tree carries the name, and
// gclient_t names the field after it.  `bot_menu_t` and `bot_menuitem_t` do --
// RA2's menu.h already defines a `menuitem_t` with different members, and
// sec 7 rule 4 puts the donor prefix on the later arrival.
typedef struct menustate_s {
    // The donor's `showmenu` is NOT here, and neither is gclient_t.showmenu.
    // R-MENU-2a says the owner is ONE field and that field is menu_owner; a
    // per-engine boolean is a second answer to the same question, which is
    // exactly what Phase 4 removed from RA2 (doc/reconciliation.md R-87).
    // Every `menustate->showmenu` test below is `menu_owner == MENU_BOT`.
    struct bot_menu_s *mainmenu;            // the main menu
    int menuid;                         // id of the current (sub)menu
    void (*menuproc)(edict_t *ent, int id);     // menu procedure
    int highlighteditem;                // highlighted menu item
    int firstdisplayeditem;             // first item displayed
    float lastchange_time;              // last time the highlight changed
    int redrawmenu;                     // true when the menu should be redrawn
} menustate_t;

// returns the menuitem with the given id
bot_menuitem_t *bot_MenuItemWithId(bot_menu_t *menu, int id);
// change the name of the menu item with the given id
void bot_MenuItemRename(bot_menu_t *menu, int id, const char *name);
// returns the name of the menu item with the given id
char *bot_MenuItemName(bot_menu_t *menu, int id);
// returns the 'item'th menu item from the given menu
bot_menuitem_t *bot_MenuItemAt(bot_menu_t *menu, int item);
// create a new menu with the given title and background
bot_menu_t *bot_MenuTreeCreate(int id, const char *title, const char *background);
// delete the given menu
void bot_MenuTreeDelete(bot_menu_t *menu);
// append a menu option to the given menu
void bot_MenuAppend(bot_menu_t *menu, unsigned char type, int id,
                    bot_menu_t *submenu, const char *name, const char *bitmap);
// remove the menu item with the given id
void bot_MenuRemoveItem(bot_menu_t *menu, int id);
// send a layout to the client -- svc_layout, not CS_STATUSBAR
void bot_SendLayout(edict_t *ent, const char *layout);
// show the menu the client is viewing
void bot_MenuShow(edict_t *ent);
// go backward in the menu (go back to parent menu)
int bot_MenuBack(edict_t *ent);
// R-MENU-4's three: the inventory keys drive this engine as they drive the
// other three, so `invnext`/`invprev`/`invuse` reach the cursor and the help
// screen's "your inventory key to select" becomes true.  The donor drove the
// menu from forwardmove/sidemove alone, which is still wired in bot_MenuThink.
void bot_MenuNext(edict_t *ent);
void bot_MenuPrev(edict_t *ent);
void bot_MenuSelect(edict_t *ent);
// do regular menu updates
void bot_MenuThink(edict_t *ent, usercmd_t *ucmd);

#endif // BOT_P_MENULIB_H
