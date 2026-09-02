/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "g_local.h"
#include "bot/bl_cmd.h"
#include "bot/p_menulib.h"
#include "bot/p_observer.h"
#include "arena/arena.h"
#include "tourney/osp_hooks.h"
#include "m_player.h"

static char *ClientTeam(edict_t *ent)
{
    char        *p;
    static char value[MAX_INFO_STRING];

    value[0] = 0;

    if (!ent->client)
        return value;

    Q_strlcpy(value, Info_ValueForKey(ent->client->pers.userinfo, "skin"), sizeof(value));
    p = strchr(value, '/');
    if (!p)
        return value;

    if ((int)(dmflags->value) & DF_MODELTEAMS) {
        *p = 0;
        return value;
    }

    // if ((int)(dmflags->value) & DF_SKINTEAMS)
    return ++p;
}

bool OnSameTeam(edict_t *ent1, edict_t *ent2)
{
    char    ent1Team[MAX_INFO_STRING];
    char    ent2Team[MAX_INFO_STRING];

    // R-ARENA-1: under arena a team is an RA2 TEAM, not a model or a skin.
    // RA2 replaces this function outright with `resp.teamnum == resp.teamnum`
    // and every arena rule that asks "are these two on a side together" -- the
    // friendly-fire pair in g_combat.c, score-by-damage, the crosshair ID --
    // asks it through here.  Left on baseq2's answer the question is decided by
    // `dmflags`, which under arena is not set, so every one of them answered
    // "no" and RA2's teams meant nothing to the damage rules.
    //
    // Two properties the donor's version leaves implicit and this one states:
    // a non-client is on nobody's team, and `teamnum` -1 is "not on a team"
    // (p_client.c: 0 is a real index), so two team-less clients are not
    // team-mates.  Neither is reachable from a live round -- you cannot fight
    // without a team -- and both are reachable from the lobby.
    if (G_Ruleset() == RULESET_ARENA) {
        if (!ent1->client || !ent2->client)
            return false;
        if (ent1->client->resp.teamnum < 0)
            return false;
        return ent1->client->resp.teamnum == ent2->client->resp.teamnum;
    }

    if (!((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS)))
        return false;

    Q_strlcpy(ent1Team, ClientTeam(ent1), sizeof(ent1Team));
    Q_strlcpy(ent2Team, ClientTeam(ent2), sizeof(ent2Team));

    if (strcmp(ent1Team, ent2Team) == 0)
        return true;
    return false;
}

// Non-static: tourney's delegated dispatcher calls it (R-88).
void SelectNextItem(edict_t *ent, int itflags)
{
    gclient_t   *cl;
    int         i, index;
    const gitem_t   *it;

    cl = ent->client;

    // R-MENU-4: menu input is consumed before anything else can interpret the
    // same key.  The menu owner decides which engine sees it.
    if (G_MenuActive(ent)) {
        if (cl->menu_owner == MENU_CTF)
            ctf_PMenu_Next(ent);
        else if (cl->menu_owner == MENU_ARENA)
            MenuNext(ent);
        else if (cl->menu_owner == MENU_TOURNEY)
            osp_PMenu_Next(ent);
        else if (cl->menu_owner == MENU_BOT)
            bot_MenuNext(ent);
        return;
    }

    if (cl->chase_target) {
        ChaseNext(ent);
        return;
    }

    // scan  for the next valid one
    for (i = 1; i <= game.num_items; i++) {
        index = (cl->pers.selected_item + i) % game.num_items;
        if (!cl->pers.inventory[index])
            continue;
        it = &itemlist[index];
        if (!it->use)
            continue;
        if (!(it->flags & itflags))
            continue;

        cl->pers.selected_item = index;
        return;
    }

    cl->pers.selected_item = -1;
}

// Non-static: tourney's delegated dispatcher calls it (R-88).
void SelectPrevItem(edict_t *ent, int itflags)
{
    gclient_t   *cl;
    int         i, index;
    const gitem_t   *it;

    cl = ent->client;

    // R-MENU-4: menu input is consumed before anything else can interpret the
    // same key.  The menu owner decides which engine sees it.
    if (G_MenuActive(ent)) {
        if (cl->menu_owner == MENU_CTF)
            ctf_PMenu_Prev(ent);
        else if (cl->menu_owner == MENU_ARENA)
            MenuPrev(ent);
        else if (cl->menu_owner == MENU_TOURNEY)
            osp_PMenu_Prev(ent);
        else if (cl->menu_owner == MENU_BOT)
            bot_MenuPrev(ent);
        return;
    }

    if (cl->chase_target) {
        ChasePrev(ent);
        return;
    }

    // scan  for the next valid one
    for (i = 1; i <= game.num_items; i++) {
        index = (cl->pers.selected_item + game.num_items - i) % game.num_items;
        if (!cl->pers.inventory[index])
            continue;
        it = &itemlist[index];
        if (!it->use)
            continue;
        if (!(it->flags & itflags))
            continue;

        cl->pers.selected_item = index;
        return;
    }

    cl->pers.selected_item = -1;
}

void ValidateSelectedItem(edict_t *ent)
{
    gclient_t   *cl;

    cl = ent->client;

    if (cl->pers.inventory[cl->pers.selected_item])
        return;     // valid

    SelectNextItem(ent, -1);
}

//=================================================================================

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void Cmd_Give_f(edict_t *ent)
{
    char        *name;
    const gitem_t   *it;
    int         index;
    int         i;
    bool        give_all;
    edict_t     *it_ent;

    if ((deathmatch->value || coop->value) && !sv_cheats->value) {
        gi.cprintf(ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
        return;
    }

    name = gi.args();

    if (Q_stricmp(name, "all") == 0)
        give_all = true;
    else
        give_all = false;

    if (give_all || Q_stricmp(gi.argv(1), "health") == 0) {
        if (gi.argc() == 3)
            ent->health = Q_atoi(gi.argv(2));
        else
            ent->health = ent->max_health;
        if (!give_all)
            return;
    }

    if (give_all || Q_stricmp(name, "weapons") == 0) {
        for (i = 0; i < game.num_items; i++) {
            it = itemlist + i;
            if (!it->pickup)
                continue;
            if (!(it->flags & IT_WEAPON))
                continue;
            ent->client->pers.inventory[i] += 1;
        }
        if (!give_all)
            return;
    }

    if (give_all || Q_stricmp(name, "ammo") == 0) {
        for (i = 0; i < game.num_items; i++) {
            it = itemlist + i;
            if (!it->pickup)
                continue;
            if (!(it->flags & IT_AMMO))
                continue;
            Add_Ammo(ent, it, 1000);
        }
        if (!give_all)
            return;
    }

    if (give_all || Q_stricmp(name, "armor") == 0) {
        const gitem_armor_t *info;

        it = FindItem("Jacket Armor");
        ent->client->pers.inventory[ITEM_INDEX(it)] = 0;

        it = FindItem("Combat Armor");
        ent->client->pers.inventory[ITEM_INDEX(it)] = 0;

        it = FindItem("Body Armor");
        info = (const gitem_armor_t *)it->info;
        ent->client->pers.inventory[ITEM_INDEX(it)] = info->max_count;

        if (!give_all)
            return;
    }

    if (give_all || Q_stricmp(name, "Power Shield") == 0) {
        it = FindItem("Power Shield");
        it_ent = G_Spawn();
        it_ent->classname = it->classname;
        SpawnItem(it_ent, it);
        Touch_Item(it_ent, ent, NULL, NULL);
        if (it_ent->inuse)
            G_FreeEdict(it_ent);

        if (!give_all)
            return;
    }

    if (give_all) {
        for (i = 0; i < game.num_items; i++) {
            it = itemlist + i;
            if (!it->pickup)
                continue;
            if (it->flags & IT_NOT_GIVEABLE)                    // ROGUE
                continue;                                       // ROGUE
            if (it->flags & (IT_ARMOR | IT_WEAPON | IT_AMMO))
                continue;
            ent->client->pers.inventory[i] = 1;
        }
        return;
    }

    it = FindItem(name);
    if (!it) {
        name = gi.argv(1);
        it = FindItem(name);
        if (!it) {
            gi.cprintf(ent, PRINT_HIGH, "unknown item\n");
            return;
        }
    }

    if (!it->pickup) {
        gi.cprintf(ent, PRINT_HIGH, "non-pickup item\n");
        return;
    }

//ROGUE
    if (it->flags & IT_NOT_GIVEABLE) {
        gi.dprintf("item cannot be given\n");
        return;
    }
//ROGUE

    index = ITEM_INDEX(it);

    if (it->flags & IT_AMMO) {
        if (gi.argc() == 3)
            ent->client->pers.inventory[index] = Q_atoi(gi.argv(2));
        else
            ent->client->pers.inventory[index] += it->quantity;
    } else {
        it_ent = G_Spawn();
        it_ent->classname = it->classname;
        SpawnItem(it_ent, it);
        // PMM - since some items don't actually spawn when you say to ..
        if (!it_ent->inuse)
            return;
        // pmm
        Touch_Item(it_ent, ent, NULL, NULL);
        if (it_ent->inuse)
            G_FreeEdict(it_ent);
    }
}

/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f(edict_t *ent)
{
    if ((deathmatch->value || coop->value) && !sv_cheats->value) {
        gi.cprintf(ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
        return;
    }

    ent->flags ^= FL_GODMODE;
    if (!(ent->flags & FL_GODMODE))
        gi.cprintf(ent, PRINT_HIGH, "godmode OFF\n");
    else
        gi.cprintf(ent, PRINT_HIGH, "godmode ON\n");
}

/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f(edict_t *ent)
{
    if ((deathmatch->value || coop->value) && !sv_cheats->value) {
        gi.cprintf(ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
        return;
    }

    ent->flags ^= FL_NOTARGET;
    if (!(ent->flags & FL_NOTARGET))
        gi.cprintf(ent, PRINT_HIGH, "notarget OFF\n");
    else
        gi.cprintf(ent, PRINT_HIGH, "notarget ON\n");
}

/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f(edict_t *ent)
{
    if ((deathmatch->value || coop->value) && !sv_cheats->value) {
        gi.cprintf(ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
        return;
    }

    if (ent->movetype == MOVETYPE_NOCLIP) {
        ent->movetype = MOVETYPE_WALK;
        gi.cprintf(ent, PRINT_HIGH, "noclip OFF\n");
    } else {
        ent->movetype = MOVETYPE_NOCLIP;
        gi.cprintf(ent, PRINT_HIGH, "noclip ON\n");
    }
}

/*
==================
Cmd_Use_f

Use an inventory item
==================
*/

void Cmd_Use_f(edict_t *ent)
{
    int         index;
    const gitem_t   *it;
    char        *s;

    s = gi.args();
    it = FindItem(s);
    if (!it) {
        gi.cprintf(ent, PRINT_HIGH, "unknown item: %s\n", s);
        return;
    }
    if (!it->use) {
        gi.cprintf(ent, PRINT_HIGH, "Item is not usable.\n");
        return;
    }

    index = ITEM_INDEX(it);
    if (!ent->client->pers.inventory[index]) {
        // RAFAEL
        if (strcmp(it->pickup_name, "HyperBlaster") == 0) {
            it = FindItem("Ionripper");
            index = ITEM_INDEX(it);
            if (!ent->client->pers.inventory[index]) {
                gi.cprintf(ent, PRINT_HIGH, "Out of item: %s\n", s);
                return;
            }
        }
        // RAFAEL
        else if (strcmp(it->pickup_name, "Railgun") == 0) {
            it = FindItem("Phalanx");
            index = ITEM_INDEX(it);
            if (!ent->client->pers.inventory[index]) {
                gi.cprintf(ent, PRINT_HIGH, "Out of item: %s\n", s);
                return;
            }
        } else {
            gi.cprintf(ent, PRINT_HIGH, "Out of item: %s\n", s);
            return;
        }
    }

    it->use(ent, it);
}

/*
==================
Cmd_Drop_f

Drop an inventory item
==================
*/
void Cmd_Drop_f(edict_t *ent)
{
    int         index;
    const gitem_t   *it;
    char        *s;

    // CTF: `drop tech` drops whichever tech you hold, since the four techs have
    // four classnames and the player has one key bound.
    if (Q_stricmp(gi.args(), "tech") == 0 && (it = CTFWhat_Tech(ent)) != NULL) {
        it->drop(ent, it);
        return;
    }

    s = gi.args();
    it = FindItem(s);
    if (!it) {
        gi.cprintf(ent, PRINT_HIGH, "unknown item: %s\n", s);
        return;
    }
    if (!it->drop || ((coop->value) && (it->flags & IT_STAY_COOP))) {
        gi.cprintf(ent, PRINT_HIGH, "Item is not dropable.\n");
        return;
    }

    index = ITEM_INDEX(it);
    if (!ent->client->pers.inventory[index]) {
        // RAFAEL
        if (strcmp(it->pickup_name, "HyperBlaster") == 0) {
            it = FindItem("Ionripper");
            index = ITEM_INDEX(it);
            if (!ent->client->pers.inventory[index]) {
                gi.cprintf(ent, PRINT_HIGH, "Out of item: %s\n", s);
                return;
            }
        }
        // RAFAEL
        else if (strcmp(it->pickup_name, "Railgun") == 0) {
            it = FindItem("Phalanx");
            index = ITEM_INDEX(it);
            if (!ent->client->pers.inventory[index]) {
                gi.cprintf(ent, PRINT_HIGH, "Out of item: %s\n", s);
                return;
            }
        } else {
            gi.cprintf(ent, PRINT_HIGH, "Out of item: %s\n", s);
            return;
        }
    }

    it->drop(ent, it);
}

/*
=================
Cmd_Inven_f
=================
*/
void Cmd_Inven_f(edict_t *ent)
{
    int         i;
    gclient_t   *cl;

    cl = ent->client;

    cl->showscores = false;
    if (G_Ruleset() == RULESET_ARENA)
        cl->scoremode = 0;
    cl->showhelp = false;

    // R-MENU-4 again: `inven` closes an open menu rather than opening the
    // inventory behind it.
    if (G_MenuActive(ent)) {
        G_MenuClose(ent);
        cl->update_chase = true;
        return;
    }

    if (G_Ruleset() == RULESET_ARENA) {
        // R-RA-4's "arena menu on connect and on `inven`".  G_MenuActive above
        // has already handled the close; this is the reopen.
        if (cl->curmenulink) {
            G_MenuOpen(ent, MENU_ARENA);
            DisplayMenu(ent);
        }
        return;
    }

    // R-OSP-2: THIS FUNCTION IS TOURNEY'S MENU KEY.  The donor rewrites
    // Cmd_Inven_f into the opener for its own menus and never shows an
    // inventory at all, and osp_clientcmd.c still routes `menu`, `ctfmenu` and
    // `inven` here for that reason -- so with no arm the whole OSP menu tree
    // (team, vote, help, bots, HUD toggle) had no way in and only a referee,
    // who reaches OSP_adminMenu through `referee`, ever saw a menu.
    //
    // `osp_r010` is the mod's own two-frame debounce on menu keys, and
    // `osp_r02c` is "this client has opened a menu", which the 1-vs-1 queue
    // reads.  Both belong with the open, not with the caller.
    if (G_IsOspRuleset()) {
        if (cl->resp.osp_r010 <= level.framenum) {
            cl->resp.osp_r010 = level.framenum + 2;
            cl->resp.osp_r02c = 1;

            if (OSP_IsTeams())
                OSP_teamMenu(ent);
            else
                OSP_DMMenu(ent);
        }
        return;
    }

    if (cl->showinventory) {
        cl->showinventory = false;
        return;
    }

    // ...and under ctf, with no team yet, `inven` is how you reach the join
    // menu.  R-RA-4 wants the same for the arena menu in Phase 4, which is why
    // the test is on the ruleset here rather than inside CTFOpenJoinMenu.
    if (G_Ruleset() == RULESET_CTF && cl->resp.ctf_team == CTF_NOTEAM) {
        CTFOpenJoinMenu(ent);
        return;
    }

    cl->showinventory = true;

    gi.WriteByte(svc_inventory);
    for (i = 0; i < MAX_ITEMS; i++) {
        gi.WriteShort(cl->pers.inventory[i]);
    }
    gi.unicast(ent, true);
}

/*
=================
Cmd_InvUse_f
=================
*/
// Non-static: tourney's menus bind `invuse` AND `invdrop` (R-MENU-4).  They are
// the same key to the menu and differ only in DIRECTION -- see OSP_menuSelect().
void Cmd_InvUse_f(edict_t *ent)
{
    const gitem_t   *it;

    if (G_MenuActive(ent)) {
        if (ent->client->menu_owner == MENU_CTF)
            ctf_PMenu_Select(ent);
        else if (ent->client->menu_owner == MENU_ARENA &&
                 !level.intermission_framenum)
            UseMenu(ent, 1);
        else if (ent->client->menu_owner == MENU_TOURNEY)
            OSP_menuSelect(ent, false);
        else if (ent->client->menu_owner == MENU_BOT)
            bot_MenuSelect(ent);
        return;
    }

    // R-OSP-1: THIS KEY IS THE PLAYER CARD, and the merge dropped the only site
    // that opens it.  `OSP_ScoreboardMessage` has a `case 8: OSP_showPlayer()`
    // arm -- one of its five pages -- and nothing in this tree ever wrote 8 to
    // `resp.osp_r24c`, so the page was unreachable: the donor's toggle lives
    // here rather than in `score`, because pressing USE on an open scoreboard is
    // how OSP asks for your own card.  Under RegularDM only (`dm`), which is the
    // donor's `!m_mode`; the other three have a match result on that key.
    // `osp_r2ac` is the card's own cursor and -1 is "closed" (R-192).
    //
    // The two guards around it are the donor's as well: at intermission a match
    // ruleset consumes the key rather than using an item, and an OBSERVER uses
    // nothing at all -- the same `entered` gate R-191 restored in Cmd_Kill_f,
    // and an observer does carry a blaster for `it->use` to fire.
    if (G_IsOspRuleset()) {
        if (OSP_IsMatch() && level.intermission_framenum)
            return;

        if (G_Ruleset() == RULESET_DM && ent->client->showscores &&
            ent->client->resp.osp_r24c != 1 &&
            ent->client->resp.osp_r010 <= level.framenum && active_clients) {
            ent->client->resp.osp_r010 = level.framenum + 2;
            if (!ent->client->resp.osp_r24c) {
                ent->client->resp.osp_r24c = 8;
            } else {
                ent->client->resp.osp_r24c = 0;
                ent->client->resp.osp_r2ac = -1;
            }
            DeathmatchScoreboard(ent);
            return;
        }

        if (ent->client->resp.osp_entered != ENTERED_ENTERED)
            return;
    }

    ValidateSelectedItem(ent);

    if (ent->client->pers.selected_item == -1) {
        gi.cprintf(ent, PRINT_HIGH, "No item to use.\n");
        return;
    }

    it = &itemlist[ent->client->pers.selected_item];
    if (!it->use) {
        gi.cprintf(ent, PRINT_HIGH, "Item is not usable.\n");
        return;
    }
    it->use(ent, it);
}

/*
=================
Cmd_WeapPrev_f
=================
*/

void Cmd_WeapPrev_f(edict_t *ent)
{
    gclient_t   *cl;
    int         i, index;
    const gitem_t   *it;
    int         selected_weapon;

    cl = ent->client;

    if (!cl->pers.weapon)
        return;

    selected_weapon = ITEM_INDEX(cl->pers.weapon);

    // scan  for the next valid one
    for (i = 1; i <= game.num_items; i++) {
        // PMM - prevent scrolling through ALL weapons
//      index = (selected_weapon + i) % game.num_items;
        index = (selected_weapon + game.num_items - i) % game.num_items;
        if (!cl->pers.inventory[index])
            continue;
        it = &itemlist[index];
        if (!it->use)
            continue;
        if (!(it->flags & IT_WEAPON))
            continue;
        it->use(ent, it);
        // PMM - prevent scrolling through ALL weapons
//      if (cl->pers.weapon == it)
//          return; // successful
        if (cl->newweapon == it)
            return;
    }
}

/*
=================
Cmd_WeapNext_f
=================
*/
#if 0
void Cmd_WeapNext_f(edict_t *ent)
{
    gclient_t   *cl;
    int         i, index;
    const gitem_t   *it;
    int         selected_weapon;

    cl = ent->client;

    if (!cl->pers.weapon)
        return;

    selected_weapon = ITEM_INDEX(cl->pers.weapon);

    // scan  for the next valid one
    for (i = 1; i <= game.num_items; i++) {
        // PMM - prevent scrolling through ALL weapons
//      index = (selected_weapon + game.num_items - i) % game.num_items;
        index = (selected_weapon + i) % game.num_items;
        if (!cl->pers.inventory[index])
            continue;
        it = &itemlist[index];
        if (!it->use)
            continue;
        if (!(it->flags & IT_WEAPON))
            continue;
        it->use(ent, it);
        // PMM - prevent scrolling through ALL weapons
//      if (cl->pers.weapon == it)
//          return; // successful
        if (cl->newweapon == it)
            return;
    }
}
#endif
void Cmd_WeapNext_f(edict_t *ent)
{
    gclient_t   *cl;
    int         i, index;
    const gitem_t   *it;
    int         selected_weapon;

    cl = ent->client;

    if (!cl->pers.weapon)
        return;

    selected_weapon = ITEM_INDEX(cl->pers.weapon);

    // scan  for the next valid one
    for (i = 1; i <= MAX_ITEMS; i++) {
        index = (selected_weapon + MAX_ITEMS - i) % MAX_ITEMS;

        if (!cl->pers.inventory[index])
            continue;
        it = &itemlist[index];
        if (!it->use)
            continue;
        if (!(it->flags & IT_WEAPON))
            continue;
        it->use(ent, it);
        if (cl->pers.weapon == it)
            return; // successful
    }
}

/*
=================
Cmd_WeapLast_f
=================
*/
void Cmd_WeapLast_f(edict_t *ent)
{
    gclient_t   *cl;
    int         index;
    const gitem_t   *it;

    cl = ent->client;

    if (!cl->pers.weapon || !cl->pers.lastweapon)
        return;

    index = ITEM_INDEX(cl->pers.lastweapon);
    if (!cl->pers.inventory[index])
        return;
    it = &itemlist[index];
    if (!it->use)
        return;
    if (!(it->flags & IT_WEAPON))
        return;
    it->use(ent, it);
}

/*
=================
Cmd_InvDrop_f
=================
*/
void Cmd_InvDrop_f(edict_t *ent)
{
    const gitem_t   *it;

    if (G_Ruleset() == RULESET_ARENA && G_MenuActive(ent) &&
        ent->client->menu_owner == MENU_ARENA) {
        // RA2 binds the menu's "back" to `drop`, the way it binds "select" to
        // `invuse`; there is nothing droppable in an arena anyway.
        UseMenu(ent, 0);
        return;
    }

    // R-OSP-13.  Tourney binds `invdrop` to the menu too, and it is the REVERSE
    // of `invuse`: OSP_menuSelect's flag is the step direction every settings
    // row reads (`osp_r290--` against `osp_r290++`) and the scan direction of
    // the player list.  Without this arm the flag was never once set, so nine
    // branches in osp_menus.c stood permanently on their false side and a
    // tourney menu value could be stepped up but never down.
    if (G_IsOspRuleset() && G_MenuActive(ent) &&
        ent->client->menu_owner == MENU_TOURNEY) {
        OSP_menuSelect(ent, true);
        return;
    }

    ValidateSelectedItem(ent);

    if (ent->client->pers.selected_item == -1) {
        gi.cprintf(ent, PRINT_HIGH, "No item to drop.\n");
        return;
    }

    it = &itemlist[ent->client->pers.selected_item];
    if (!it->drop || ((coop->value) && (it->flags & IT_STAY_COOP))) {
        gi.cprintf(ent, PRINT_HIGH, "Item is not dropable.\n");
        return;
    }
    it->drop(ent, it);
}

/*
=================
Cmd_Kill_f
=================
*/
// Non-static: tourney's match system kills a player on a team change.
void Cmd_Kill_f(edict_t *ent)
{
    // An observer has nothing to kill.  Threewave tests `solid != SOLID_NOT`,
    // which also catches a dead player -- who should be able to re-suicide.
    //
    // Tourney's spelling is its own and G_IsObserver() does not carry it: the
    // donor deleted baseq2's spectator system and says "watching" as
    // `resp.entered != ENTERED_ENTERED`, which is the second half of its
    // combined condition in this same function.  Asked here rather than added
    // to the shared predicate, because two of that predicate's other call sites
    // would then diverge from the donor -- see doc/reconciliation.md R-191.
    if (G_IsObserver(ent) ||
        (G_IsOspRuleset() && ent->client->resp.osp_entered != ENTERED_ENTERED))
        return;

    // R-OSP-1 exempts the COUNTDOWN from the five-second guard: OSP_checkSync
    // kills everybody into a fresh spawn through this function at sync_stat 2,
    // and a player who respawned inside the last five seconds would otherwise
    // be left standing where they were while the rest of the map was reset.
    if ((level.framenum - ent->client->respawn_framenum) < 5 * BASE_FRAMERATE &&
        !(G_IsOspRuleset() && sync_stat == 2))
        return;

    // R-OSP-1: dying ends `client_protect` spawn protection, so that it cannot
    // be carried through a suicide into the next life.
    if (G_IsOspRuleset())
        ent->client->resp.osp_r23c = 0;

    // *** THE DONOR'S THREE LINES BEFORE THE DEATH, and they are the ones that
    // make a tourney `kill` LEAVE NO TRACE. ***  Clearing `s.effects` and
    // `s.renderfx` matters because CopyToBodyQue copies BOTH to the corpse:
    // whatever the body was wearing -- a quad shell, a rune glow, the CTF flag
    // effect -- would otherwise go on glowing on the floor after its owner had
    // gone.  And the hook is let go before the die rather than inside it,
    // because tourney's grapple is not Threewave's and player_die releases
    // Threewave's (p_client.c).
    if (G_IsOspRuleset()) {
        ent->s.effects = 0;
        ent->s.renderfx = 0;
        OSP_hookoff_cmd(ent);       // the donor's PlayerResetGrapple(ent)
    }

    ent->flags &= ~FL_GODMODE;
    ent->health = 0;
    meansOfDeath = MOD_SUICIDE;

//ROGUE
    // make sure no trackers are still hurting us.
    if (ent->client->tracker_pain_framenum)
        RemoveAttackingPainDaemons(ent);

    if (ent->client->owned_sphere) {
        G_FreeEdict(ent->client->owned_sphere);
        ent->client->owned_sphere = NULL;
    }
//ROGUE

    player_die(ent, ent, ent, 100000, ent->s.origin);

    // *** ...AND THE TWO AFTER IT, WHICH THE MERGE DROPPED. ***  The donor's
    // Cmd_Kill_f does not wait for the death frames: it stamps DEAD_DEAD and
    // respawns in the same call, so `kill` under tourney is "put me back at a
    // spawn point", not "lie here until I press fire".
    //
    // WHICH IS LOAD-BEARING AT THE START OF EVERY MATCH, and that is how the
    // gap was found -- played, not read.  OSP_checkSync ends the countdown by
    // killing EVERYBODY into a fresh spawn (sync_stat 2 -> 4) and then, in the
    // same frame, sweeping the map: gibs are freed and every `bodyque` is
    // unlinked, zeroed and hidden.  The sweep can only park the corpses that
    // ALREADY EXIST when it runs, and a corpse exists because respawn() ->
    // CopyToBodyQue made one.  With the respawn missing, the kills made no
    // corpses for the sweep to find; each player then lay dead until they
    // pressed fire, and the body queued THEN -- after the sweep had run --
    // stayed lit in the middle of a live match, with whatever the death had
    // dropped beside it.  Measured on q2dm1: both players PM_DEAD with
    // STAT_HEALTH 0 at "Match has started!", and three entities in the world
    // that were not there at level load.
    //
    // Gated, because it is tourney's answer and not baseq2's: everywhere else
    // `kill` leaves a body that waits for the respawn button, which is the
    // behaviour ra2observer asserts a round outlives (g_cmds.c's `drop` note).
    if (G_IsOspRuleset()) {
        ent->deadflag = DEAD_DEAD;
        respawn(ent);
    }
}

/*
=================
Cmd_PutAway_f
=================
*/
void Cmd_PutAway_f(edict_t *ent)
{
    ent->client->showscores = false;
    // RA2's `putaway` clears `scoremode`; `showscores` is not the field its
    // HUD reads (sec 7 rule 3), so clearing only that left the arena board up.
    if (G_Ruleset() == RULESET_ARENA)
        ent->client->scoremode = 0;
    ent->client->showhelp = false;
    ent->client->showinventory = false;
    G_MenuClose(ent);
    ent->client->update_chase = true;
}

static int PlayerSort(void const *a, void const *b)
{
    int     anum, bnum;

    anum = *(int *)a;
    bnum = *(int *)b;

    anum = game.clients[anum].ps.stats[STAT_FRAGS];
    bnum = game.clients[bnum].ps.stats[STAT_FRAGS];

    if (anum < bnum)
        return -1;
    if (anum > bnum)
        return 1;
    return 0;
}

/*
=================
Cmd_Players_f
=================
*/
// Non-static: tourney's delegated dispatcher calls it (R-88).
void Cmd_Players_f(edict_t *ent)
{
    int     i;
    int     count;
    char    small[64];
    char    large[1280];
    int     index[MAX_CLIENTS];

    // R-OSP-2: tourney's own listing, because under tourney a player is
    // addressed by NUMBER -- `vote kick 3`, `accuracy 3`, `r_kick` -- and a
    // list of names sorted by frags does not say what those numbers are.  A
    // referee gets the address column as well, which is what `r_ban` needs;
    // `osp_e39c` is the referee flag.  Bots are marked so that a `removebots`
    // vote can be reasoned about.
    if (G_IsOspRuleset()) {
        edict_t *e;

        gi.cprintf(ent, PRINT_HIGH,
                   ent->osp_e39c ? "\nID:Name [Address]\n" : "\nID:Name\n");
        gi.cprintf(ent, PRINT_HIGH, "---------------------\n");

        for (i = 1; i <= game.maxclients; i++) {
            e = g_edicts + i;
            if (!e->inuse || !e->client)
                continue;

            if (e->flags & FL_BOT) {
                gi.cprintf(ent, PRINT_HIGH, "%2d:\"%s\" [BOT]\n",
                           e->client->resp.clientid, e->client->pers.netname);
            } else if (e->client->resp.clientid == -1) {
                gi.cprintf(ent, PRINT_HIGH, "XX:\"%s\" (Connecting)\n",
                           e->client->pers.netname);
            } else if (!ent->osp_e39c) {
                gi.cprintf(ent, PRINT_HIGH, "%2d:\"%s\"\n",
                           e->client->resp.clientid, e->client->pers.netname);
            } else {
                OSP_getPlayerAddr(e);
                gi.cprintf(ent, PRINT_HIGH, "%2d:\"%s\" [%s]\n",
                           e->client->resp.clientid, e->client->pers.netname,
                           e->osp_e37c);
            }
        }
        return;
    }

    count = 0;
    for (i = 0; i < game.maxclients; i++)
        if (game.clients[i].pers.connected) {
            index[count] = i;
            count++;
        }

    // sort by frags
    qsort(index, count, sizeof(index[0]), PlayerSort);

    // print information
    large[0] = 0;

    for (i = 0; i < count; i++) {
        Q_snprintf(small, sizeof(small), "%3i %s\n",
                   game.clients[index[i]].ps.stats[STAT_FRAGS],
                   game.clients[index[i]].pers.netname);
        if (strlen(small) + strlen(large) > sizeof(large) - 100) {
            // can't print all of them in one packet
            Q_strlcat(large, "...\n", sizeof(large));
            break;
        }
        Q_strlcat(large, small, sizeof(large));
    }

    gi.cprintf(ent, PRINT_HIGH, "%s\n%i players\n", large, count);
}

/*
=================
Cmd_Wave_f
=================
*/
void Cmd_Wave_f(edict_t *ent)
{
    int     i;

    i = Q_atoi(gi.argv(1));

    // can't wave when ducked
    if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
        return;

    if (ent->client->anim_priority > ANIM_WAVE)
        return;

    ent->client->anim_priority = ANIM_WAVE;

    switch (i) {
    case 0:
        gi.cprintf(ent, PRINT_HIGH, "flipoff\n");
        ent->s.frame = FRAME_flip01 - 1;
        ent->client->anim_end = FRAME_flip12;
        break;
    case 1:
        gi.cprintf(ent, PRINT_HIGH, "salute\n");
        ent->s.frame = FRAME_salute01 - 1;
        ent->client->anim_end = FRAME_salute11;
        break;
    case 2:
        gi.cprintf(ent, PRINT_HIGH, "taunt\n");
        ent->s.frame = FRAME_taunt01 - 1;
        ent->client->anim_end = FRAME_taunt17;
        break;
    case 3:
        gi.cprintf(ent, PRINT_HIGH, "wave\n");
        ent->s.frame = FRAME_wave01 - 1;
        ent->client->anim_end = FRAME_wave11;
        break;
    case 4:
    default:
        gi.cprintf(ent, PRINT_HIGH, "point\n");
        ent->s.frame = FRAME_point01 - 1;
        ent->client->anim_end = FRAME_point12;
        break;
    }
}

// §7 rule 6 keeps ONE flood check and Q2PRO's is the one; Threewave renamed it
// CheckFlood and added a second call in Cmd_Say_f, which charges the counter
// twice for one message.  The name and the single call stay; g_ctf.c's
// CTFSay_Team calls this one (R-CORE-13: linkage follows what calls what).
bool FloodProtect(edict_t *ent)
{
    int i, msgs = flood_msgs->value;
    gclient_t *cl = ent->client;

    if (msgs < 1)
        return false;

    if (level.time < cl->flood_locktill) {
        gi.cprintf(ent, PRINT_HIGH, "You can't talk for %d more seconds\n",
                   (int)(cl->flood_locktill - level.time));
        return true;
    }

    i = cl->flood_whenhead - min(msgs, FLOOD_MSGS) + 1;
    if (i < 0)
        i += FLOOD_MSGS;
    if (cl->flood_when[i] &&
        level.time - cl->flood_when[i] < flood_persecond->value) {
        cl->flood_locktill = level.time + flood_waitdelay->value;
        gi.cprintf(ent, PRINT_CHAT, "Flood protection:  You can't talk for %d seconds.\n",
                   (int)flood_waitdelay->value);
        return true;
    }

    cl->flood_whenhead = (cl->flood_whenhead + 1) % FLOOD_MSGS;
    cl->flood_when[cl->flood_whenhead] = level.time;
    return false;
}

/*
==================
Cmd_Say_f
==================
*/
// Non-static: tourney's delegated dispatcher calls it (R-88).
void Cmd_Say_f(edict_t *ent, bool team, bool arg0, bool bcast)
{
    int     j;
    edict_t *other;
    char    text[2048];

    if (gi.argc() < 2 && !arg0)
        return;

    if (FloodProtect(ent))
        return;

    // *** R-165: RA2 DELETES THIS TEST, AND LEAVING IT IN LEAKED EVERY TEAM
    // *** CALLOUT TO THE OTHER SIDE.
    //
    // baseq2 has no teams of its own, so it decides whether `say_team` means
    // anything by asking dmflags for model- or skin-teams.  An arena team is
    // `resp.teamnum` and has nothing to do with either bit, nothing under arena
    // sets them, and no shipped config does -- so this line turned `say_team`
    // into a server-wide `say` on a mod that is played in teams and whose
    // audience is standing in the same room.
    if (G_Ruleset() != RULESET_ARENA &&
        !((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS)))
        team = false;

    if (team)
        Q_snprintf(text, sizeof(text), "(%s): ", ent->client->pers.netname);
    else if (bcast)
        // RA2's `say_world`: reaches everyone even when teamplay would have
        // confined it, and marks itself so the other team knows why it heard.
        Q_snprintf(text, sizeof(text), "W:%s: ", ent->client->pers.netname);
    else
        Q_snprintf(text, sizeof(text), "%s: ", ent->client->pers.netname);

    if (arg0) {
        Q_strlcat(text, gi.argv(0), sizeof(text));
        Q_strlcat(text, " ", sizeof(text));
        Q_strlcat(text, gi.args(), sizeof(text));
    } else {
        Q_strlcat(text, COM_StripQuotes(gi.args()), sizeof(text));
    }

    // don't let text be too long for malicious reasons
    if (strlen(text) > 150)
        text[150] = 0;

    // R-195.3.  The donor logs chat from HERE, not from `talkto` alone:
    // `q2log_playerChat(text)` sits at `port_osp:g_cmds.c:934`, after the
    // truncation and before the newline, so the line reaches the log with its
    // "name: " or "(name): " prefix and without the trailing "\n".  This tree
    // had the call only in `OSP_talkto_cmd`, so `stats_logchat 1` recorded
    // private messages and nothing else while `osp_stats.h` and doc/cvars.md
    // both describe the cvar as logging chat lines.
    //
    // One call, and the donor's coverage is exactly what one call gives: `say`
    // under all four OSP rulesets, and `say_team` under three of them.  Under
    // `tdm` say_team is routed to OSP_sayteam_cmd (osp_clientcmd.c:80) before
    // it can reach here -- and the donor routes it the same way at
    // `port_osp:g_cmds.c:1031` and does not log there either.  Sec 7 rule 2
    // hands the donor its own feature; the tdm say_team gap is the donor's.
    if (G_IsOspRuleset())
        OSP_Stats_Chat(text);

    Q_strlcat(text, "\n", sizeof(text));

    if (dedicated->value)
        gi.cprintf(NULL, PRINT_CHAT, "%s", text);

    // R-165's other half: RA2 keeps ordinary chat inside the ARENA it was said
    // in, and `say_world` -- the `bcast` arm, which marks itself "W:" so the
    // other arenas know why they heard it -- is the server-wide one.  The merge
    // delivered every `say` server-wide, which made `say_world` a synonym for
    // `say` and left `show_string`, the donor's own arena-scoped print, with no
    // chat caller at all.
    //
    // After the console line above and not before it: HiPrint() rewrites the
    // buffer in place, and the server log wants the plain text.
    if (G_Ruleset() == RULESET_ARENA && !team && !bcast) {
        show_string(PRINT_MEDIUM, HiPrint(text), ent->client->resp.context);
        return;
    }

    for (j = 1; j <= game.maxclients; j++) {
        other = &g_edicts[j];
        if (!other->inuse)
            continue;
        if (!other->client)
            continue;
        if (team) {
            // R-EXTRA-6: observers are their own team for `say_team`.  An
            // observer's team chat reaches observers and nobody else, and a
            // player's reaches players -- otherwise the audience reads the
            // callouts, which is the whole reason the donor added this.
            bool i_watch = (ent->flags & FL_OBSERVER) != 0;
            bool they_watch = (other->flags & FL_OBSERVER) != 0;

            if (i_watch != they_watch)
                continue;
            if (!i_watch && !OnSameTeam(ent, other))
                continue;
            // R-165.  RA2 adds a second condition of its own and it is the same
            // thought as the observer split above, one state finer: a team-mate
            // who is not in the same part of the round does not hear it.  Under
            // arena `fightstate` has three values -- spectating, alive, dead --
            // so a dead team-mate is neither an observer nor still in the
            // fight, and the callout is not theirs to read.
            if (G_Ruleset() == RULESET_ARENA &&
                other->client->resp.fightstate != ent->client->resp.fightstate)
                continue;
        }
        gi.cprintf(other, PRINT_CHAT, "%s", text);
    }
}

//======
//ROGUE
void Cmd_Ent_Count_f(edict_t *ent)
{
    int     x;
    edict_t *e;

    x = 0;

    for (e = g_edicts; e < &g_edicts[globals.num_edicts]; e++) {
        if (e->inuse)
            x++;
    }

    gi.dprintf("%d entites active\n", x);
}
//ROGUE
//======

void Cmd_PlayerList_f(edict_t *ent)
{
    int i;
    char st[80];
    char text[1400];
    edict_t *e2;

    // connect time, ping, score, name
    *text = 0;
    for (i = 0, e2 = g_edicts + 1; i < game.maxclients; i++, e2++) {
        if (!e2->inuse)
            continue;

        Q_snprintf(st, sizeof(st), "%02d:%02d %4d %3d %s%s\n",
                   (level.framenum - e2->client->resp.enterframe) / 600,
                   ((level.framenum - e2->client->resp.enterframe) % 600) / 10,
                   e2->client->ping,
                   e2->client->resp.score,
                   e2->client->pers.netname,
                   e2->client->pers.spectator ? " (spectator)" : "");
        if (strlen(text) + strlen(st) > sizeof(text) - 50) {
            if (strlen(text) < sizeof(text) - 12)
                Q_strlcat(text, "And more...\n", sizeof(text));
            gi.cprintf(ent, PRINT_HIGH, "%s", text);
            return;
        }
        Q_strlcat(text, st, sizeof(text));
    }
    gi.cprintf(ent, PRINT_HIGH, "%s", text);
}

/*
=================
ClientCommand
=================
*/
void ClientCommand(edict_t *ent)
{
    char    *cmd;

    if (!ent->client)
        return;     // not fully in game yet

    cmd = gi.argv(0);

    // OSP Tourney has 137 client commands (R-OSP-2).  Threewave's twelve are
    // arms in the chain below and that reads fine at twelve; 137 would triple
    // this file and put a donor's whole command surface in a spine file.  One
    // gate, one question: it returns true if it handled the command
    // (doc/reconciliation.md R-88).
    if (G_IsOspRuleset() && OSP_ClientCommand(ent))
        return;

    if (Q_stricmp(cmd, "players") == 0) {
        Cmd_Players_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "say") == 0) {
        // R-165.  A speaker who is in no arena has nobody to be arena-local to,
        // so their `say` is the server-wide one; inside an arena it stays in it.
        // The donor's own condition, and it is why `say_world` exists.
        Cmd_Say_f(ent, false, false,
                  G_Ruleset() == RULESET_ARENA && !ent->client->resp.context);
        return;
    }
    if (Q_stricmp(cmd, "say_team") == 0 || Q_stricmp(cmd, "steam") == 0) {
        // CTF has its own team chat: it prefixes the team name, reports flag
        // and tech state through `%` macros, and reaches only the sender's
        // team rather than everyone with a matching skin.
        if (G_Ruleset() == RULESET_CTF)
            CTFSay_Team(ent, gi.args());
        else
            Cmd_Say_f(ent, true, false, false);
        return;
    }
    if (Q_stricmp(cmd, "score") == 0) {
        Cmd_Score_f(ent);
        return;
    }
    if (Q_stricmp(cmd, "help") == 0) {
        Cmd_Help_f(ent);
        return;
    }

    if (level.intermission_framenum)
        return;

    // R-166: `drop` IS A NO-OP UNDER ARENA.  RA2 empties Cmd_Drop_f outright,
    // and the reason is already written down in this tree against
    // TossClientWeapon (p_client.c), which IS gated: an arena hands out a fixed
    // loadout and SpawnItem() frees every pickup on the map, so a dropped
    // weapon is an ITEM in a ruleset that has none, lying on the floor for
    // whoever the round goes to next.  The death route was closed and the one
    // the player drives was not.
    //
    // *** `kill` IS THE OTHER HALF AND IS DELIBERATELY NOT TAKEN. ***  RA2
    // dispatches it to nothing, so this differs from the donor knowingly.
    // SPECS.md's "dead functions are live again" names `Cmd_Kill_f` in the list
    // of what RA2 retired and says every one of them is live here -- Colosseum
    // keeps four rulesets and three campaigns that RA2 dropped, and `kill` is a
    // verb all of them have.  It is also load-bearing under arena rather than
    // merely tolerated: `ra2observer` asserts that a round OUTLIVES a fighter's
    // suicide and that wiping a side still ends it, which is a property of the
    // round machine worth testing and needs a way to reach it.  Whether an
    // arena fighter should be able to leave a round that way is a rules
    // question for the ruleset to answer, not a merge defect to correct.
    //
    // At the dispatcher rather than inside Cmd_Drop_f, because that function
    // has another caller that is not this one: CTF's `drop tech`.
    if (G_Ruleset() == RULESET_ARENA && Q_stricmp(cmd, "drop") == 0)
        return;

    if (Q_stricmp(cmd, "use") == 0)
        Cmd_Use_f(ent);
    else if (Q_stricmp(cmd, "drop") == 0)
        Cmd_Drop_f(ent);
    else if (Q_stricmp(cmd, "give") == 0)
        Cmd_Give_f(ent);
    else if (Q_stricmp(cmd, "god") == 0)
        Cmd_God_f(ent);
    else if (Q_stricmp(cmd, "notarget") == 0)
        Cmd_Notarget_f(ent);
    else if (Q_stricmp(cmd, "noclip") == 0)
        Cmd_Noclip_f(ent);
    else if (Q_stricmp(cmd, "inven") == 0)
        Cmd_Inven_f(ent);
    else if (Q_stricmp(cmd, "invnext") == 0)
        SelectNextItem(ent, -1);
    else if (Q_stricmp(cmd, "invprev") == 0)
        SelectPrevItem(ent, -1);
    else if (Q_stricmp(cmd, "invnextw") == 0)
        SelectNextItem(ent, IT_WEAPON);
    else if (Q_stricmp(cmd, "invprevw") == 0)
        SelectPrevItem(ent, IT_WEAPON);
    else if (Q_stricmp(cmd, "invnextp") == 0)
        SelectNextItem(ent, IT_POWERUP);
    else if (Q_stricmp(cmd, "invprevp") == 0)
        SelectPrevItem(ent, IT_POWERUP);
    else if (Q_stricmp(cmd, "invuse") == 0)
        Cmd_InvUse_f(ent);
    else if (Q_stricmp(cmd, "invdrop") == 0)
        Cmd_InvDrop_f(ent);
    else if (Q_stricmp(cmd, "weapprev") == 0)
        Cmd_WeapPrev_f(ent);
    else if (Q_stricmp(cmd, "weapnext") == 0)
        Cmd_WeapNext_f(ent);
    else if (Q_stricmp(cmd, "weaplast") == 0)
        Cmd_WeapLast_f(ent);
    else if (Q_stricmp(cmd, "kill") == 0)
        Cmd_Kill_f(ent);
    else if (Q_stricmp(cmd, "putaway") == 0)
        Cmd_PutAway_f(ent);
    else if (Q_stricmp(cmd, "wave") == 0)
        Cmd_Wave_f(ent);
    // CTF's client commands.  Gated on the ruleset rather than registered
    // conditionally, so that a player who types `team` under dm gets the chat
    // fallback rather than silence.
    else if (G_Ruleset() == RULESET_CTF && Q_stricmp(cmd, "team") == 0)
        CTFTeam_f(ent);
    else if (G_Ruleset() == RULESET_CTF && Q_stricmp(cmd, "id") == 0)
        CTFID_f(ent);
    else if (G_Ruleset() == RULESET_CTF && Q_stricmp(cmd, "yes") == 0)
        CTFVoteYes(ent);
    else if (G_Ruleset() == RULESET_CTF && Q_stricmp(cmd, "no") == 0)
        CTFVoteNo(ent);
    else if (G_Ruleset() == RULESET_CTF && Q_stricmp(cmd, "ready") == 0)
        CTFReady(ent);
    else if (G_Ruleset() == RULESET_CTF && Q_stricmp(cmd, "notready") == 0)
        CTFNotReady(ent);
    else if (G_Ruleset() == RULESET_CTF && Q_stricmp(cmd, "ghost") == 0)
        CTFGhost(ent);
    else if (G_Ruleset() == RULESET_CTF && Q_stricmp(cmd, "admin") == 0)
        CTFAdmin(ent);
    else if (G_Ruleset() == RULESET_CTF && Q_stricmp(cmd, "stats") == 0)
        CTFStats(ent);
    else if (G_Ruleset() == RULESET_CTF && Q_stricmp(cmd, "warp") == 0)
        CTFWarp(ent);
    else if (G_Ruleset() == RULESET_CTF && Q_stricmp(cmd, "boot") == 0)
        CTFBoot(ent);
    else if (G_Ruleset() == RULESET_CTF && Q_stricmp(cmd, "observer") == 0)
        CTFObserver(ent);
    // R-CTF-3's offhand hook.  Two commands rather than a +hook alias, which is
    // what uGladQ2 v0.97u did and what every 1999 config binds.
    else if (G_Ruleset() == RULESET_CTF && Q_stricmp(cmd, "hookon") == 0)
        CTFHook_f(ent);
    else if (G_Ruleset() == RULESET_CTF && Q_stricmp(cmd, "hookoff") == 0)
        CTFUnhook_f(ent);
    // R-EXTRA-6's eight, from the 1999 module: observer, autocam, chasecam,
    // cyclecam, setcam, camfixed, camname, observerhelp.  Asked as one
    // predicate, the shape the donor used, and only where the Gladiator
    // observer is the implementation in force -- under `arena` and `tourney`
    // the ruleset's own observer owns those verbs.
    else if (G_GladiatorObserver() && ClientObserverCmd(cmd, ent))
        ;
    // R-EXTRA-2's two, from the 1999 module.  Client commands in the donor and
    // client commands here; `g_clientlag` decides whether they do anything, and
    // both say so when it is off rather than silently accepting a number.
    else if (Q_stricmp(cmd, "lag") == 0)
        Lag_SetClientLag(ent, Q_atoi(gi.argv(1)));
    else if (Q_stricmp(cmd, "lagvariance") == 0)
        Lag_SetClientLagVariance(ent, Q_atoi(gi.argv(1)));
    else if (Q_stricmp(cmd, "playerlist") == 0) {
        // Threewave DELETES baseq2's Cmd_PlayerList_f and replaces it; R-CORE-8's
        // rule applies to functions as well as files, so both survive and the
        // ruleset picks.  CTF's adds team, ghost code and ready state; baseq2's
        // reports `resp.spectator`, which CTF has no equivalent for.
        if (G_Ruleset() == RULESET_CTF)
            CTFPlayerList(ent);
        else
            Cmd_PlayerList_f(ent);
    }
    // Rocket Arena's own commands (R-RA-2).  `admin` and `playerlist` collide
    // with names that already mean something else here, so the ruleset picks
    // which meaning is live rather than either being renamed.
    else if (G_Ruleset() == RULESET_ARENA && Q_stricmp(cmd, "say_world") == 0)
        Cmd_Say_f(ent, false, false, true);
    else if (G_Ruleset() == RULESET_ARENA && Q_stricmp(cmd, "admin") == 0)
        Cmd_admin_f(ent);
    else if (G_Ruleset() == RULESET_ARENA && Q_stricmp(cmd, "arenaadmin") == 0)
        Cmd_arenaadmin_f(ent, 0);
    else if (G_Ruleset() == RULESET_ARENA && Q_stricmp(cmd, "menuhelp") == 0)
        Cmd_menuhelp_f(ent);
    // The offhand latch, and only the latch: whether it may fire is
    // RA_HookThink()'s question, asked every frame from ClientThink, because
    // `grapple` can be voted off and a round can end under a player who is
    // still holding the key (R-164).
    else if (G_Ruleset() == RULESET_ARENA && Q_stricmp(cmd, "grap_on") == 0)
        ent->client->ctf_hookstate = CTF_HOOK_STATE_ON;
    // TURNOFF rather than 0: it has to say "the offhand hook is finished" and
    // not merely "no offhand hook is out", because those are the two cases
    // RA_HookThink() has to tell apart -- a bare 0 is also what a player firing
    // the Grapple ITEM from the weapon slot looks like, and that hook is not
    // the latch's to release.
    else if (G_Ruleset() == RULESET_ARENA && Q_stricmp(cmd, "grap_off") == 0)
        ent->client->ctf_hookstate = CTF_HOOK_STATE_TURNOFF;
    else if (G_Ruleset() == RULESET_ARENA && Q_stricmp(cmd, "listkeys") == 0)
        list_keys(ent);
    else if (G_Ruleset() == RULESET_ARENA && Q_stricmp(cmd, "listmaps") == 0)
        print_map_loop(ent);
    else if (G_Ruleset() == RULESET_ARENA && Q_stricmp(cmd, "nextmap") == 0)
        gi.cprintf(ent, PRINT_MEDIUM, "Next map is %s\n",
                   get_next_map(level.mapname));
    // Three RA2 commands that are `return;` in the donor too -- clients bind
    // them and the server is expected to swallow them silently.
    else if (G_Ruleset() == RULESET_ARENA &&
             (Q_stricmp(cmd, "getdebugcode") == 0 ||
              Q_stricmp(cmd, "pcount") == 0 ||
              Q_stricmp(cmd, "play") == 0))
        return;
    else if (Q_stricmp(cmd, "entcount") == 0)       // PGM
        Cmd_Ent_Count_f(ent);                       // PGM
    else if (Q_stricmp(cmd, "disguise") == 0) {     // PGM
        ent->flags |= FL_DISGUISED;
    }
    // R-BOT-24, from a client rather than the console, so `server` is false --
    // which is what makes the six dump commands console-only.  R-BOT-25's
    // `serveronlybotcmds` gates the rest and defaults to 1.  Asked LAST, and
    // before the chat fallback, so no bot name can shadow a ruleset's command.
    else if (BotCmd(cmd, ent, false))
        ;
    else  // anything that doesn't match a command will be a chat
        // R-165: server-wide under arena, which is the donor's own asymmetry --
        // a mistyped command is not a callout and RA2 would rather everybody
        // saw it than that it vanished into one arena.
        Cmd_Say_f(ent, false, true, G_Ruleset() == RULESET_ARENA);
}
