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
// The Gladiator Bot menu tree (R-BOT-28), from gladiator-bot-restored@game.
//
// osp-tourney cannot supply this file: it moved its bot menu into
// osp_menus.c's Bot_Menu on id's PMenu and dropped `botctfteam`,
// `ra_playercycle` and `ra_botcycle` as cvars, so those three are re-registered
// here.  The 1999 structure is intact -- DM / CTF / RA2 / help / credits
// submenus, title bitmaps, minimum players, teamplay and dmflags editing, rcon
// gating -- with the donor's four #ifdefs replaced by the resolved ruleset and
// the two content layers (sec 7 rule 6).
//===========================================================================
//
// Name:                p_botmenu.c
// Function:        menu
// Programmer:      Mr Elusive (MrElusive@demigod.demon.nl)
// Last update: 1998-01-12
// Tab Size:        3
//===========================================================================

#include "g_local.h"
#include "bot/bl_main.h"
#include "bot/bl_spawn.h"
#include "bot/bl_redirgi.h"
#include "bot/bl_botcfg.h"
#include "bot/p_menulib.h"
#include "bot/p_botmenu.h"
#include "arena/arena.h"

//menu IDs
#define MID_MAIN                    0
#define MID_GAME                    1
#define MID_GAME_TIMELIMIT          2
#define MID_GAME_FRAGLIMIT          3
#define MID_BOT                     4
#define MID_BOT_ADD                 5
#define MID_BOT_ADDRANDOM           6
#define MID_BOT_REMOVE              7
#define MID_BOT_REMOVEALL           8
#define MID_BOT_MINPLAYERS          9
#define MID_DM                      10
#define MID_DM_TEAMPLAY             11
#define MID_DM_NO_HEALTH            12
#define MID_DM_NO_ITEMS             13
#define MID_DM_WEAPONS_STAY         14
#define MID_DM_NO_FALLING           15
#define MID_DM_INSTANT_ITEMS        16
#define MID_DM_SAME_LEVEL           17
#define MID_DM_NO_FRIENDLY_FIRE     18
#define MID_DM_SPAWN_FARTHEST       19
#define MID_DM_FORCE_RESPAWN        20
#define MID_DM_NO_ARMOR             21
#define MID_DM_ALLOW_EXIT           22
#define MID_DM_INFINITE_AMMO        23
#define MID_DM_QUAD_DROP            24
#define MID_DM_FIXED_FOV            25
#define MID_DM_QUADFIRE_DROP        26
#define MID_DM_NO_MINES             27
#define MID_DM_NO_STACK_DOUBLE      28
#define MID_DM_NO_NUKES             29
#define MID_DM_NO_SPHERES           30
#define MID_CTF                     31
#define MID_CTF_BOTTEAM             32
#define MID_CTF_FORCEJOIN           33
#define MID_CTF_ARMOR_PROTECT       34
#define MID_CTF_NO_TECH             35
#define MID_RA2                     36
#define MID_RA2_BOTARENA            37
#define MID_RA2_PLAYERCYCLE         38
#define MID_RA2_BOTCYCLE            39
// R-RA-7 / R-CTF-8 / R-DM-1 are one switch, so the row sits on the BOTS page,
// immediately below `minimum players` -- which is the number it replaces.  Its
// label is the cvar's own name, because that is what an operator sets in a
// config, and there is one name to print now: the three per-ruleset spellings
// became `botfill`.  One concept, one row: two pages toggling one cvar is what
// the RA2 page's own comment below argues against.
#define MID_BOT_BOTFILL             40
#define MID_HELP                    41
#define MID_CREDITS                 42
#define MID_EXIT                    43
#define MID_BACK                    44
// The donor gave the credits submenu MID_HELP as its item id, so
// bot_MenuItemWithId(MID_HELP) found the help menu and "Credits" could never be
// entered -- a duplicate id in a tree that is searched by id.
#define MID_BOT_ADD_FIRST           256
#define MID_BOT_ADD_LAST            511
#define MID_BOT_REMOVE_FIRST        512
#define MID_BOT_REMOVE_LAST         1024

static bot_menu_t *mainmenu;

//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
static void SetupRemoveBotMenu(void)
{
    int i, n;
    edict_t *cl_ent;
    bot_menuitem_t *menuitem;
    bot_menu_t *removemenu, *parent = NULL;

    menuitem = bot_MenuItemWithId(mainmenu, MID_BOT_REMOVE);
    if (!menuitem) return;
    if (menuitem->submenu)
    {
        parent = menuitem->submenu->parent;
        bot_MenuTreeDelete(menuitem->submenu);
        menuitem->submenu = NULL;
    } //end if
    //
    removemenu = bot_MenuTreeCreate(MID_BOT_REMOVE, "", "m_remove");
    n = 0;
    for (i = 0; i < game.maxclients; i++)
    {
        cl_ent = DF_CLIENTENT(i);
        if (!cl_ent->inuse) continue;
        if (!(cl_ent->flags & FL_BOT)) continue;
        bot_MenuAppend(removemenu, MI_ITEM, MID_BOT_REMOVE_FIRST + n,
                       NULL, cl_ent->client->pers.netname, NULL);
        n++;
    } //end for
    //if there are no bots in the game
    if (n == 0) bot_MenuAppend(removemenu, MI_SEPERATOR, -1, NULL, "- no bots loaded -", NULL);
    bot_MenuAppend(removemenu, MI_SEPERATOR, -1, NULL, "-----------", NULL);
    bot_MenuAppend(removemenu, MI_ITEM, MID_BACK, NULL, "back", NULL);
    removemenu->parent = parent;
    menuitem->submenu = removemenu;
} //end of the function SetupRemoveBotMenu
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
static void SetupAddBotMenu(void)
{
    int i, n;
    edict_t *cl_ent;
    bot_menuitem_t *menuitem;
    bot_menu_t *addmenu, *parent = NULL;
    bot_t *bot;

    CheckForNewBotFile();
    //
    menuitem = bot_MenuItemWithId(mainmenu, MID_BOT_ADD);
    if (!menuitem) return;
    if (menuitem->submenu)
    {
        parent = menuitem->submenu->parent;
        bot_MenuTreeDelete(menuitem->submenu);
        menuitem->submenu = NULL;
    } //end if
    //
    addmenu = bot_MenuTreeCreate(MID_BOT_ADD, "", "m_add");
    n = 0;
    for (bot = botlist; bot; bot = bot->next)
    {
        for (i = 0; i < game.maxclients; i++)
        {
            cl_ent = DF_CLIENTENT(i);
            if (!cl_ent->inuse) continue;
            if (!(cl_ent->flags & FL_BOT)) continue;
            if (!strcmp(bot->name, cl_ent->client->pers.netname)) break;
        } //end for
        if (i < game.maxclients) continue;
        if (n > MID_BOT_ADD_LAST - MID_BOT_ADD_FIRST) break;
        bot_MenuAppend(addmenu, MI_ITEM, MID_BOT_ADD_FIRST + n, NULL, bot->name, NULL);
        n++;
    } //end for
    if (n == 0) bot_MenuAppend(addmenu, MI_SEPERATOR, -1, NULL, "- no bots.cfg -", NULL);
    bot_MenuAppend(addmenu, MI_SEPERATOR, -1, NULL, "-----------", NULL);
    bot_MenuAppend(addmenu, MI_ITEM, MID_BACK, NULL, "back", NULL);
    addmenu->parent = parent;
    menuitem->submenu = addmenu;
} //end of the function SetupAddBotMenu
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
static const char *OnOffString(const char *name, int value)
{
    static char buf[128];

    Q_snprintf(buf, sizeof(buf), "%-18s%s", name, value ? "on" : "off");
    return buf;
} //end of the function OnOffString
//===========================================================================
// R-RA-9: the `arena` row, which has to be able to SAY zero.
//
// 0 is not an unset cvar, it is a request with a meaning -- "put the bots where
// the people are, and keep every populated arena at its own size" -- and since
// this commit it is the default.  Printed as a bare `0` beside "bot arena" it
// reads as a broken row, and the two places that draw it would otherwise each
// carry their own copy of the test.
//
// Parameter:               -
// Returns:                 the row text
// Changes Globals:     -
//===========================================================================
static const char *BotArenaString(void)
{
    static char buf[128];
    int n = (int)gi.cvar("arena", "0", 0)->value;

    if (n < 1)
        Q_snprintf(buf, sizeof(buf), "%-18s%s", "bot arena", "follow players");
    else
        Q_snprintf(buf, sizeof(buf), "%-18s%d", "bot arena", n);

    return buf;
} //end of the function BotArenaString
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
static void ToggleMenuCVarBoolean(const char *name, const char *def, int id)
{
    cvar_t *cvar = gi.cvar(name, def, 0);
    char buf[32];
    int value = !cvar->value;

    // The donor writes `cvar->value = !cvar->value` and THEN cvar_set()s the
    // string, which leaves the engine's own copy of value and string briefly
    // disagreeing and is a write into engine-owned memory besides.  cvar_set
    // does both.
    Q_snprintf(buf, sizeof(buf), "%d", value);
    gi.cvar_set(name, buf);
    bot_MenuItemRename(mainmenu, id, OnOffString(name, value));
} //end of the function ToggleMenuCVarBoolean
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
static void ToggleDMFlag(const char *name, int flag, int flip, int id)
{
    char buf[32];
    int value = ((int)dmflags->value) ^ flag;

    Q_snprintf(buf, sizeof(buf), "%d", value);
    gi.cvar_set("dmflags", buf);
    bot_MenuItemRename(mainmenu, id, OnOffString(name, ((value & flag) != 0) ^ flip));
} //end of the function ToggleDMFlag
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
static const char *TeamPlayMenuString(void)
{
    if (((int)dmflags->value) & DF_MODELTEAMS)
        return "teamplay          by model";
    else if (((int)dmflags->value) & DF_SKINTEAMS)
        return "teamplay          by skin";
    else
        return "teamplay          disabled";
} //end of the function TeamPlayMenuString
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
static const char *BotCTFTeamString(void)
{
    // R-BOT-28: re-registered here, because RA2's and tourney's ports both
    // dropped it and the add-bot path reads it to fill the `ctfteam` userinfo.
    cvar_t *botctfteam = gi.cvar("botctfteam", "0", 0);

    if (botctfteam->value == 1)
        return "bot team          red";
    else if (botctfteam->value == 2)
        return "bot team          blue";
    else
        return "bot team          auto assign";
} //end of the function BotCTFTeam
//========================================================================
// R-BOT-28's "minimum players" row, which the donor's tree names and never
// builds.  The cvar is the ruleset's own (R-OSP-11).
//========================================================================
static const char *MinPlayersString(void)
{
    static char buf[128];

    Q_snprintf(buf, sizeof(buf), "%-18s%d", "minimum players",
               (int)BotMinPlayers()->value);
    return buf;
}
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
static void MenuProc(edict_t *ent, int id)
{
    int i;
    bot_t *bot;
    edict_t *cl_ent;
    char buf[128];
    const char *str;

    if (id >= MID_BOT_ADD_FIRST && id <= MID_BOT_ADD_LAST)
    {
        str = bot_MenuItemName(mainmenu, id);
        if (!str) return;
        bot = FindBotWithName(str);
        if (!bot) return;
        BotServerCommand("sv", "addbot", bot->name, bot->skin, bot->charfile,
                         bot->charname, NULL);
        bot_MenuRemoveItem(mainmenu, id);
        return;
    } //end if
    if (id >= MID_BOT_REMOVE_FIRST && id <= MID_BOT_REMOVE_LAST)
    {
        // The name has to be COPIED before the item is removed: `str` points
        // into the menu item bot_MenuRemoveItem is about to free, and the donor
        // hands that pointer to BotServerCommand first only by accident of
        // never freeing.
        char name[MAX_MENU_NAME];

        str = bot_MenuItemName(mainmenu, id);
        if (!str) return;
        Q_strlcpy(name, str, sizeof(name));
        BotServerCommand("sv", "removebot", name, NULL);
        bot_MenuRemoveItem(mainmenu, id);
        return;
    } //end if
    switch(id)
    {
        case MID_BOT_ADD: //entering the add bot menu
        {
            SetupAddBotMenu();
            break;
        } //end case
        case MID_BOT_ADDRANDOM: //add a random bot
        {
            AddRandomBot(ent);
            break;
        } //end case
        case MID_BOT_REMOVE: //entering the bot remove menu
        {
            SetupRemoveBotMenu();
            break;
        } //end case
        case MID_BOT_REMOVEALL: //remove all the bots
        {
            for (i = 0; i < game.maxclients; i++)
            {
                cl_ent = DF_CLIENTENT(i);
                if (!cl_ent->inuse) continue;
                if (!(cl_ent->flags & FL_BOT)) continue;
                BotServerCommand("sv", "removebot", cl_ent->client->pers.netname, NULL);
            } //end for
            break;
        } //end case
        case MID_BOT_MINPLAYERS:
        {
            int want = (int)BotMinPlayers()->value + 1;

            if (want > game.maxclients) want = 0;
            Q_snprintf(buf, sizeof(buf), "%d", want);
            gi.cvar_set(BotMinPlayersCvar(), buf);
            bot_MenuItemRename(mainmenu, id, MinPlayersString());
            break;
        } //end case
        case MID_DM_TEAMPLAY:
        {
            int flags = (int)dmflags->value;

            // The donor's chain tests MODELTEAMS before SKINTEAMS and writes
            // `buf` in each arm -- but the first arm is the "neither" case, so
            // with both bits somehow set nothing is written and cvar_set gets
            // an uninitialised buffer.  Written as the three-state cycle it is.
            if (!(flags & (DF_MODELTEAMS | DF_SKINTEAMS)))
                flags = (flags & ~DF_SKINTEAMS) | DF_MODELTEAMS;
            else if (flags & DF_MODELTEAMS)
                flags = (flags & ~DF_MODELTEAMS) | DF_SKINTEAMS;
            else
                flags &= ~(DF_MODELTEAMS | DF_SKINTEAMS);
            Q_snprintf(buf, sizeof(buf), "%d", flags);
            gi.cvar_set("dmflags", buf);
            bot_MenuItemRename(mainmenu, id, TeamPlayMenuString());
            break;
        } //end case
        case MID_DM_NO_HEALTH: ToggleDMFlag("allow health", DF_NO_HEALTH, 1, id); break;
        case MID_DM_NO_ITEMS: ToggleDMFlag("allow powerups", DF_NO_ITEMS, 1, id); break;
        case MID_DM_WEAPONS_STAY: ToggleDMFlag("weapons stay", DF_WEAPONS_STAY, 0, id); break;
        case MID_DM_NO_FALLING: ToggleDMFlag("falling damage", DF_NO_FALLING, 1, id); break;
        case MID_DM_INSTANT_ITEMS: ToggleDMFlag("instant items", DF_INSTANT_ITEMS, 0, id); break;
        case MID_DM_SAME_LEVEL: ToggleDMFlag("same map", DF_SAME_LEVEL, 0, id); break;
        case MID_DM_NO_FRIENDLY_FIRE: ToggleDMFlag("friendly fire", DF_NO_FRIENDLY_FIRE, 1, id); break;
        case MID_DM_SPAWN_FARTHEST: ToggleDMFlag("spawn farthest", DF_SPAWN_FARTHEST, 0, id); break;
        case MID_DM_FORCE_RESPAWN: ToggleDMFlag("force respawn", DF_FORCE_RESPAWN, 0, id); break;
        case MID_DM_NO_ARMOR: ToggleDMFlag("allow armor", DF_NO_ARMOR, 1, id); break;
        case MID_DM_ALLOW_EXIT: ToggleDMFlag("allow exit", DF_ALLOW_EXIT, 0, id); break;
        case MID_DM_INFINITE_AMMO: ToggleDMFlag("infinite ammo", DF_INFINITE_AMMO, 0, id); break;
        case MID_DM_QUAD_DROP: ToggleDMFlag("quad drop", DF_QUAD_DROP, 0, id); break;
        case MID_DM_FIXED_FOV: ToggleDMFlag("fixed FOV", DF_FIXED_FOV, 0, id); break;
        case MID_DM_QUADFIRE_DROP: ToggleDMFlag("quad fire drop", DF_QUADFIRE_DROP, 0, id); break;
        case MID_DM_NO_MINES: ToggleDMFlag("allow mines", DF_NO_MINES, 1, id); break;
        case MID_DM_NO_STACK_DOUBLE: ToggleDMFlag("allow stack double", DF_NO_STACK_DOUBLE, 1, id); break;
        case MID_DM_NO_NUKES: ToggleDMFlag("allow nukes", DF_NO_NUKES, 1, id); break;
        case MID_DM_NO_SPHERES: ToggleDMFlag("allow spheres", DF_NO_SPHERES, 1, id); break;
        case MID_CTF_BOTTEAM:
        {
            cvar_t *botctfteam = gi.cvar("botctfteam", "0", 0);

            if (botctfteam->value == 1) gi.cvar_set("botctfteam", "2");
            else if (botctfteam->value == 2) gi.cvar_set("botctfteam", "0");
            else gi.cvar_set("botctfteam", "1");
            bot_MenuItemRename(mainmenu, id, BotCTFTeamString());
            break;
        } //end case
        case MID_CTF_FORCEJOIN: ToggleDMFlag("force join", DF_CTF_FORCEJOIN, 0, id); break;
        case MID_CTF_ARMOR_PROTECT: ToggleDMFlag("armor protect", DF_ARMOR_PROTECT, 0, id); break;
        case MID_CTF_NO_TECH: ToggleDMFlag("allow techs", DF_CTF_NO_TECH, 1, id); break;

        case MID_RA2_BOTARENA:
        {
            cvar_t *arena_cvar = gi.cvar("arena", "0", 0);
            // R-RA-9: 0 IS IN THE CYCLE.  It used to wrap 1..num_arenas and
            // treat anything below 1 as 1, so an operator who opened this row
            // once could never get back to "follow the players" without a
            // console command -- which mattered little while 0 was a value
            // nobody started on, and is a trap now that it is the default.
            int a = (int)arena_cvar->value + 1;

            if (a < 0 || a > num_arenas) a = 0;
            Q_snprintf(buf, sizeof(buf), "%d", a);
            gi.cvar_set("arena", buf);
            bot_MenuItemRename(mainmenu, MID_RA2_BOTARENA, BotArenaString());
            break;
        } //end case
        case MID_RA2_PLAYERCYCLE: ToggleMenuCVarBoolean("ra_playercycle", "1", id); break;
        case MID_RA2_BOTCYCLE: ToggleMenuCVarBoolean("ra_botcycle", "1", id); break;
        case MID_BOT_BOTFILL:
            ToggleMenuCVarBoolean("botfill", "0", id);
            break;
        case MID_BACK: //back to the parent menu
        {
            bot_MenuBack(ent);
            break;
        } //end case
        case MID_EXIT:
        {
            // Through the ARBITER, not through bot_MenuClose: R-MENU-2a says
            // menu_owner is written in one place and this is a close.
            G_MenuClose(ent);
            return;
        } //end case
    } //end switch
} //end of the function MenuProc
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
void bot_MenuOpen(edict_t *ent)
{
    menustate_t *menustate;

    if (!ent->client) return;
    if (!mainmenu) bot_MenuCreate();
    if (!mainmenu) return;

    // R-MENU-2a/3: the arbiter closes whatever was open and records the owner.
    G_MenuOpen(ent, MENU_BOT);

    menustate = &ent->client->menustate;
    menustate->mainmenu = mainmenu;
    menustate->menuid = MID_MAIN;
    menustate->highlighteditem = 0;
    menustate->firstdisplayeditem = 0;
    menustate->menuproc = MenuProc;
    menustate->redrawmenu = true;
    //the menu IS a layout, so the client has to be drawing layouts
    ent->client->showscores = true;
} //end of the function bot_MenuOpen
//========================================================================
// The engine's own close.  G_MenuClose() calls THIS; nothing here may call
// G_MenuClose(), or the arbiter and the engine recurse into each other.
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
void bot_MenuClose(edict_t *ent)
{
    menustate_t *menustate;

    if (!ent->client) return;
    menustate = &ent->client->menustate;
    menustate->redrawmenu = false;
    // The TREE is shared and stays; only this client's cursor is forgotten.
    // R-87's second defect was a close that destroyed what it should have hid,
    // in the other donor -- here the distinction is that the tree was never
    // this client's to begin with.
    menustate->mainmenu = NULL;
    menustate->menuid = MID_MAIN;
    menustate->highlighteditem = 0;
    menustate->firstdisplayeditem = 0;
    ent->client->showscores = false;
    G_LayoutClear(ent);
} //end of the function bot_MenuClose
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
void bot_MenuToggle(edict_t *ent)
{
    cvar_t *rcon_password;

    if (!ent)
    {
        gi.dprintf("only clients can open the menu\n");
        return;
    } //end if
    if (ent->client->menu_owner == MENU_BOT)
    {
        G_MenuClose(ent);
        return;
    } //end if
    // R-BOT-28: rcon password gating.  serveronlybotcmds defaults to 1 here
    // (R-BOT-25), so unlike 1999 this gate is on unless a server operator turns
    // it off -- which is the point: the menu adds and removes bots, and it does
    // it through BotServerCommand, i.e. as CONSOLE commands, so this gate is
    // the only thing standing between a client and bot management.
    //
    // THE HOST OF A LISTEN SERVER IS EXEMPT, and the 1999 gate had no way to
    // say so.  A password typed as an argument is what a REMOTE operator needs;
    // the host has the server's own console, which is the authority rcon exists
    // to lend out, and the engine asks them for nothing.  They cannot use the
    // console for this either -- a menu needs a client to draw on and
    // ServerCommand has none, which is what `sv menu` answers.  So without the
    // exemption the one person who certainly is the server was the one person
    // who could not open its menu.  pers.listenhost is latched in
    // ClientConnect; see the note there for why it is not read live.
    if (gi.cvar("serveronlybotcmds", "1", 0)->value &&
        !ent->client->pers.listenhost)
    {
        rcon_password = gi.cvar("rcon_password", "", 0);
        // AN UNSET rcon_password IS NOT A PASSWORD OF "", which is what the
        // donor's bare strcmp made it.  `menu ""` reaches the game as argc 2
        // with an empty argv(1) -- q2pro's Cmd_TokenizeString registers the
        // argument before it parses the quotes, and a client command is
        // tokenized from the raw text the client forwarded -- so on a server
        // that never set a password the honest form was refused to everybody
        // and two quote marks admitted anybody.  Measured on the wire under
        // ctf before this: `menu` refused, `menu ""` opened the menu.
        //
        // Ordered before the "none" test, which is a password of that name and
        // is not empty.
        if (!rcon_password->string[0])
        {
            gi.cprintf(ent, PRINT_HIGH, "the menu needs the rcon password, "
                       "and this server has not set one\n");
            return;
        } //end if
        if (strcmp(rcon_password->string, "none") &&
            (gi.argc() <= 1 || strcmp(rcon_password->string, gi.argv(1))))
        {
            gi.cprintf(ent, PRINT_HIGH, "need rcon password to open the menu\n");
            return;
        } //end if
    } //end if
    bot_MenuOpen(ent);
} //end of the function bot_MenuToggle
//========================================================================
// R-BOT-28's per-ruleset trees.  The donor selects with `ctf->value` and
// `ra->value`; here the resolved ruleset does, and the two content layers pick
// their own dmflag rows (R-MODE-3).
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
void bot_MenuCreate(void)
{
    bot_menu_t *helpmenu, *botmenu, *addmenu, *removemenu, *dmmenu, *ra2menu, *ctfmenu;
    bot_menu_t *creditsmenu;
    // R-RA-9: the `buf` that stood here composed the "bot arena" row inline; it
    // is BotArenaString()'s now, because the row has to say "follow players"
    // for 0 and the toggle in bot_MenuAction needed the same text.
    int flags = (int)dmflags->value;

    if (mainmenu) return;

    helpmenu = bot_MenuTreeCreate(MID_HELP, "", "m_help");
    bot_MenuAppend(helpmenu, MI_SEPERATOR, -1, NULL, "Use your forward and", NULL);
    bot_MenuAppend(helpmenu, MI_SEPERATOR, -1, NULL, "backward keys (usually ", NULL);
    bot_MenuAppend(helpmenu, MI_SEPERATOR, -1, NULL, "the arrow keys) to move", NULL);
    bot_MenuAppend(helpmenu, MI_SEPERATOR, -1, NULL, "the cursor,", NULL);
    bot_MenuAppend(helpmenu, MI_SEPERATOR, -1, NULL, "your inventory key", NULL);
    bot_MenuAppend(helpmenu, MI_SEPERATOR, -1, NULL, "(usually Enter) to", NULL);
    bot_MenuAppend(helpmenu, MI_SEPERATOR, -1, NULL, "select,", NULL);
    bot_MenuAppend(helpmenu, MI_SEPERATOR, -1, NULL, "Esc to exit the menu", NULL);
    bot_MenuAppend(helpmenu, MI_SEPERATOR, -1, NULL, "", NULL);
    bot_MenuAppend(helpmenu, MI_SEPERATOR, -1, NULL, "-----------", NULL);
    bot_MenuAppend(helpmenu, MI_ITEM, MID_BACK, NULL, "back", NULL);
    //
    creditsmenu = bot_MenuTreeCreate(MID_CREDITS, "", "m_credits");
    bot_MenuAppend(creditsmenu, MI_SEPERATOR, -1, NULL, "", NULL);
    bot_MenuAppend(creditsmenu, MI_SEPERATOR, -1, NULL, "Allan (Strider) Kivlin", NULL);
    bot_MenuAppend(creditsmenu, MI_SEPERATOR, -1, NULL, "Philip Niewold", NULL);
    bot_MenuAppend(creditsmenu, MI_SEPERATOR, -1, NULL, "Info-Zip Team", NULL);
    bot_MenuAppend(creditsmenu, MI_SEPERATOR, -1, NULL, "Adrian (Mr Pink) Finol", NULL);
    bot_MenuAppend(creditsmenu, MI_SEPERATOR, -1, NULL, "Dominic (Cube) Rutter", NULL);
    bot_MenuAppend(creditsmenu, MI_SEPERATOR, -1, NULL, "Matt (Genocyde) Freitas", NULL);
    bot_MenuAppend(creditsmenu, MI_SEPERATOR, -1, NULL, "Timm Stokke", NULL);
    bot_MenuAppend(creditsmenu, MI_SEPERATOR, -1, NULL, "Rhea", NULL);
    bot_MenuAppend(creditsmenu, MI_SEPERATOR, -1, NULL, "Raven", NULL);
    bot_MenuAppend(creditsmenu, MI_SEPERATOR, -1, NULL, "-----------", NULL);
    bot_MenuAppend(creditsmenu, MI_ITEM, MID_BACK, NULL, "back", NULL);
    //
    addmenu = bot_MenuTreeCreate(MID_BOT_ADD, "", "m_add");
    removemenu = bot_MenuTreeCreate(MID_BOT_REMOVE, "", "m_remove");
    //
    botmenu = bot_MenuTreeCreate(MID_BOT, "", "m_bots");
    bot_MenuAppend(botmenu, MI_SEPERATOR, -1, NULL, "", NULL);
    bot_MenuAppend(botmenu, MI_SEPERATOR, -1, NULL, "", NULL);
    bot_MenuAppend(botmenu, MI_SEPERATOR, -1, NULL, "", NULL);
    bot_MenuAppend(botmenu, MI_SUBMENU, MID_BOT_ADD, addmenu, "add bot", NULL);
    bot_MenuAppend(botmenu, MI_ITEM, MID_BOT_ADDRANDOM, NULL, "add random", NULL);
    bot_MenuAppend(botmenu, MI_SUBMENU, MID_BOT_REMOVE, removemenu, "remove bot", NULL);
    bot_MenuAppend(botmenu, MI_ITEM, MID_BOT_REMOVEALL, NULL, "remove all", NULL);
    bot_MenuAppend(botmenu, MI_ITEM, MID_BOT_MINPLAYERS, NULL, MinPlayersString(), NULL);
    // R-RA-7 / R-CTF-8 / R-DM-1's switch, under the count it replaces.  The row
    // is unconditional now: every ruleset that can reach this menu has bots, and
    // every ruleset that has bots has a fill target (`sp` has neither).
    bot_MenuAppend(botmenu, MI_ITEM, MID_BOT_BOTFILL, NULL,
                   OnOffString("botfill",
                               (int)gi.cvar("botfill", "0", 0)->value), NULL);
    bot_MenuAppend(botmenu, MI_SEPERATOR, -1, NULL, "-----------", NULL);
    bot_MenuAppend(botmenu, MI_ITEM, MID_BACK, NULL, "back", NULL);
    //Deathmatch
    dmmenu = bot_MenuTreeCreate(MID_DM, "", "m_dm");
    bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_TEAMPLAY, NULL, TeamPlayMenuString(), NULL);
    bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_NO_HEALTH, NULL, OnOffString("allow health", !(flags & DF_NO_HEALTH)), NULL);
    bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_NO_ITEMS, NULL, OnOffString("allow powerups", !(flags & DF_NO_ITEMS)), NULL);
    bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_NO_ARMOR, NULL, OnOffString("allow armor", !(flags & DF_NO_ARMOR)), NULL);
    bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_WEAPONS_STAY, NULL, OnOffString("weapons stay", flags & DF_WEAPONS_STAY), NULL);
    bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_NO_FALLING, NULL, OnOffString("falling damage", !(flags & DF_NO_FALLING)), NULL);
    bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_INSTANT_ITEMS, NULL, OnOffString("instant items", flags & DF_INSTANT_ITEMS), NULL);
    bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_SAME_LEVEL, NULL, OnOffString("same map", flags & DF_SAME_LEVEL), NULL);
    bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_NO_FRIENDLY_FIRE, NULL, OnOffString("friendly fire", !(flags & DF_NO_FRIENDLY_FIRE)), NULL);
    bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_SPAWN_FARTHEST, NULL, OnOffString("spawn farthest", flags & DF_SPAWN_FARTHEST), NULL);
    bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_FORCE_RESPAWN, NULL, OnOffString("force respawn", flags & DF_FORCE_RESPAWN), NULL);
    bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_ALLOW_EXIT, NULL, OnOffString("allow exit", flags & DF_ALLOW_EXIT), NULL);
    bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_INFINITE_AMMO, NULL, OnOffString("infinite ammo", flags & DF_INFINITE_AMMO), NULL);
    bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_QUAD_DROP, NULL, OnOffString("quad drop", flags & DF_QUAD_DROP), NULL);
    bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_FIXED_FOV, NULL, OnOffString("fixed FOV", flags & DF_FIXED_FOV), NULL);
    //Xatrix mission pack 1
    if (G_LayerEnabled(LAYER_XATRIX))
    {
        bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_QUADFIRE_DROP, NULL, OnOffString("quad fire drop", flags & DF_QUADFIRE_DROP), NULL);
    } //end if
    //Rogue mission pack 2
    if (G_LayerEnabled(LAYER_ROGUE))
    {
        bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_NO_MINES, NULL, OnOffString("allow mines", !(flags & DF_NO_MINES)), NULL);
        bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_NO_STACK_DOUBLE, NULL, OnOffString("allow stack double", !(flags & DF_NO_STACK_DOUBLE)), NULL);
        bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_NO_NUKES, NULL, OnOffString("allow nukes", !(flags & DF_NO_NUKES)), NULL);
        bot_MenuAppend(dmmenu, MI_ITEM, MID_DM_NO_SPHERES, NULL, OnOffString("allow spheres", !(flags & DF_NO_SPHERES)), NULL);
    } //end if
    bot_MenuAppend(dmmenu, MI_SEPERATOR, -1, NULL, "-----------", NULL);
    bot_MenuAppend(dmmenu, MI_ITEM, MID_BACK, NULL, "back", NULL);
    //Capture The Flag
    ctfmenu = bot_MenuTreeCreate(MID_CTF, "", "m_ctf");
    bot_MenuAppend(ctfmenu, MI_ITEM, MID_CTF_BOTTEAM, NULL, BotCTFTeamString(), NULL);
    bot_MenuAppend(ctfmenu, MI_ITEM, MID_CTF_FORCEJOIN, NULL, OnOffString("force join", flags & DF_CTF_FORCEJOIN), NULL);
    bot_MenuAppend(ctfmenu, MI_ITEM, MID_CTF_ARMOR_PROTECT, NULL, OnOffString("armor protect", flags & DF_ARMOR_PROTECT), NULL);
    bot_MenuAppend(ctfmenu, MI_ITEM, MID_CTF_NO_TECH, NULL, OnOffString("allow techs", !(flags & DF_CTF_NO_TECH)), NULL);
    bot_MenuAppend(ctfmenu, MI_SEPERATOR, -1, NULL, "-----------", NULL);
    bot_MenuAppend(ctfmenu, MI_ITEM, MID_BACK, NULL, "back", NULL);
    //Rocket Arena 2
    // *** THE RA2 PAGE IS THREE ROWS, NOT SIXTEEN, AND THE TWELVE THAT WENT
    // BELONG TO AN ARENA THIS TREE DOES NOT HAVE. ***  (Four in 1.34; the
    // fourth was the arena's bot fill, which 1.35 moved to the bots page.)
    //
    // The donor's page carried `selfdamage`, `healthprotect`, `armorprotect`
    // and a switch per weapon.  Every one of those is a CVAR REGISTERED AND
    // READ BY `gladq2_src/g_arena.c` -- the Gladiator SDK's own arena, which
    // R-ARENA-1 explicitly does not carry across: its role is taken by the real
    // Rocket Arena, where the same sixteen concepts are PER-ARENA settings read
    // out of `arena.cfg` into `arenas[n]`.  `g_combat.c` reads
    // `arenas[ctx].armorprotect`, `give_ammo` reads `arenas[ctx].weapons`, and
    // nothing anywhere reads a cvar by any of those names.
    //
    // So the twelve rows toggled cvars with no reader: a menu that reports
    // itself.  That is the shape sec 7 rule 6 names -- two implementations of
    // one thing -- with the second one dead, and a row that lies about what it
    // controls is worse than no row.  They are not re-pointed at `arenas[n]`
    // either, because `mainmenu` is one tree shared by every client while the
    // settings are per arena, so a single row cannot say whose value it shows.
    //
    // RA2's own path for changing them at runtime already exists and is the one
    // R-RA-4 lists: `arenaadmin` opens the settings menu for the arena you are
    // in, `allowvoting*` gates each row, and the change is proposed and voted.
    // ra2menus.c builds exactly this list there, per arena and live.
    //
    // R-BOT-28's "1999 structure intact" is unaffected: it names the DM / CTF /
    // RA2 / credits submenus, `botctfteam`, `ra_playercycle`, `ra_botcycle`,
    // minimum players, teamplay and dmflags editing.  All of those stay.
    ra2menu = bot_MenuTreeCreate(MID_RA2, "", "m_ra2");
    bot_MenuAppend(ra2menu, MI_ITEM, MID_RA2_BOTARENA, NULL, BotArenaString(), NULL);
    bot_MenuAppend(ra2menu, MI_ITEM, MID_RA2_PLAYERCYCLE, NULL, OnOffString("ra_playercycle", (int)gi.cvar("ra_playercycle", "1", 0)->value), NULL);
    bot_MenuAppend(ra2menu, MI_ITEM, MID_RA2_BOTCYCLE, NULL, OnOffString("ra_botcycle", (int)gi.cvar("ra_botcycle", "1", 0)->value), NULL);
    // The arena's bot fill had a row here in 1.34 and it is on the BOTS page
    // now -- one `botfill` cvar for every ruleset since 1.36 -- with
    // ctf's and dm's, because 1.35 makes the three one concept.  It is not
    // 1999's structure and R-BOT-28 does not name it, so moving it moves
    // nothing a donor put here.
    bot_MenuAppend(ra2menu, MI_SEPERATOR, -1, NULL, "-----------", NULL);
    bot_MenuAppend(ra2menu, MI_ITEM, MID_BACK, NULL, "back", NULL);
    //
    mainmenu = bot_MenuTreeCreate(MID_MAIN, "", "m_main");
    bot_MenuAppend(mainmenu, MI_SEPERATOR, -1, NULL, "", NULL);
    bot_MenuAppend(mainmenu, MI_SEPERATOR, -1, NULL, "", NULL);
    bot_MenuAppend(mainmenu, MI_SEPERATOR, -1, NULL, "", NULL);
    bot_MenuAppend(mainmenu, MI_SEPERATOR, -1, NULL, "", NULL);
    bot_MenuAppend(mainmenu, MI_SUBMENU, MID_BOT, botmenu, "Bots", NULL);
    //
    // One of the three, chosen by the resolved ruleset.  Tourney reaches the
    // same settings through its own menus (osp_menus.c) and its dmflags are
    // not the switch that matters there, so it gets the DM tree -- the bot
    // rows are what a tourney operator opens this menu for.
    switch (G_Ruleset()) {
    case RULESET_CTF:
        bot_MenuAppend(mainmenu, MI_SUBMENU, MID_CTF, ctfmenu, "CTF", NULL);
        bot_MenuTreeDelete(dmmenu);
        bot_MenuTreeDelete(ra2menu);
        break;
    case RULESET_ARENA:
        bot_MenuAppend(mainmenu, MI_SUBMENU, MID_RA2, ra2menu, "RA2", NULL);
        bot_MenuTreeDelete(dmmenu);
        bot_MenuTreeDelete(ctfmenu);
        break;
    default:
        bot_MenuAppend(mainmenu, MI_SUBMENU, MID_DM, dmmenu, "DM", NULL);
        bot_MenuTreeDelete(ctfmenu);
        bot_MenuTreeDelete(ra2menu);
        break;
    }
    //
    bot_MenuAppend(mainmenu, MI_SUBMENU, MID_HELP, helpmenu, "Help", NULL);
    bot_MenuAppend(mainmenu, MI_SUBMENU, MID_CREDITS, creditsmenu, "Credits", NULL);
    bot_MenuAppend(mainmenu, MI_SEPERATOR, -1, NULL, "-----------", NULL);
    bot_MenuAppend(mainmenu, MI_ITEM, MID_EXIT, NULL, "Exit", NULL);
} //end of the function bot_MenuCreate
//========================================================================
// The tree is TAG_GAME, so it outlives a level change and nothing frees it at
// one.  ShutdownGame does.
//========================================================================
// The tree is TAG_GAME, so a gi.FreeTags(TAG_GAME) has already taken it: drop
// the root without walking it.  bot_MenuDestroy below DOES walk it, which is
// what segfaulted at ShutdownGame after a savegame load until this existed --
// ReadGame's FreeTags had freed every node and `mainmenu` still pointed at the
// first one.  ToggleBotMenu rebuilds on demand (`if (!mainmenu) ...`), so
// nulling the root is the whole of the recovery.
void bot_MenuForget(void)
{
    mainmenu = NULL;
}

void bot_MenuDestroy(void)
{
    if (!mainmenu) return;
    bot_MenuTreeDelete(mainmenu);
    mainmenu = NULL;
} //end of the function bot_MenuDestroy
