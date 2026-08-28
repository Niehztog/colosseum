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

char    *get_next_map(char *current);

void    Cmd_arenaadmin_f(edict_t *ent, unsigned mode);
void    menu_centerprint(edict_t *ent, char *message);
int     menuNo(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg);

char *
StringForProtect(int protect)
{
    switch (protect) {
    case 0:
        return "Damage all          ";
    case 1:
        return "Dont damage team    ";
    case 2:
        return "Damage self not team";
    default:
        return "Damage all          ";
    }
}

int
NumForProtect(char *s)
{
    if (!strcmp(s, "Damage all          "))
        return 0;
    if (!strcmp(s, "Dont damage team    "))
        return 1;
    if (!strcmp(s, "Damage self not team"))
        return 2;

    return 0;
}

int
menuLeaveArena(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    int     *state;

    state = &arenas[((team_t *)teams[ent->client->resp.teamnum].it)->arenanum].state;

    if (*state != ASTATE_COUNTDOWN
        && *state != ASTATE_ROUNDEND
        && ent->takedamage) {
        menu_centerprint(ent, "Sorry, you cannot leave the arena\nduring a match");
        return 2;
    }

    remove_from_queue(&((team_t *)teams[ent->client->resp.teamnum].it)->arenalink, NULL);
    SendTeamToArena(&teams[ent->client->resp.teamnum], 0, 1, 1);

    return 0;
}

int
menuAddtoArena(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    qmenu_t     *node;
    int         arenanum;

    arenanum = 0;

    node = (qmenu_t *)menu->it;

    while (node->next) {
        arenanum++;
        node = node->next;
        if (node == item)
            break;
    }

    if (arenanum) {
        if (arg == 1)
            return AddtoArena(ent, arenanum, 0, 0);

        return AddtoArena(ent, arenanum, 1, 1);
    }

    return 0;
}

int
menuLeaveTeamAr(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    team_t  *team;
    int     *state;

    team = teams[ent->client->resp.teamnum].it;
    state = &arenas[team->arenanum].state;

    if (*state != ASTATE_COUNTDOWN
        && *state != ASTATE_ROUNDEND
        && ent->takedamage) {
        menu_centerprint(ent, "Sorry, you cannot leave the arena\nduring a match");
        return 2;
    }

    remove_from_team(ent);
    move_to_arena(ent, 0, 1);
    init_player(ent);

    return 0;
}

int
menuLeaveTeam(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    remove_from_team(ent);
    init_player(ent);

    return 0;
}

int
menuStepInOutofLine(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    int     arenanum;
    int     wasinline;

    arenanum = ent->client->resp.context;
    wasinline = (((team_t *)teams[ent->client->resp.teamnum].it)->outofline == 0);

    if (!menuLeaveArena(ent, NULL, NULL, 0))
        return AddtoArena(ent, arenanum, 1, wasinline);

    return 2;
}

char *
getarenaname(int arenanum)
{
    edict_t *spot = NULL;

    while ((spot = G_Find(spot, FOFS(classname), "info_player_intermission")) != NULL)
        if (spot->arena == arenanum)
            return spot->message;

    return va("Arena Number %d", arenanum);
}

int
menuShowSettingsPropose(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    if (arenas[ent->client->resp.context].proposetime > level.time) {
        if ((int)(arenas[ent->client->resp.context].proposetime - level.time) < 30)
            menu_centerprint(ent, va("Voting is in progress.\nPlease wait %d seconds",
                                     (int)(arenas[ent->client->resp.context].proposetime - level.time)));
        else
            menu_centerprint(ent, "Voting is in progress.\nPlease wait");

        return 2;
    }

    if (ent->client->resp.ra_votes == 0) {
        menu_centerprint(ent, va("Sorry, you cannot propose any more changes.\nYou have already proposed %d times\n", votetries_setting));
        return 2;
    }

    ent->client->resp.ra_votes--;
    Cmd_arenaadmin_f(ent, 1);

    return 2;
}

int
menuShowSettingsVote(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{

    if (arenas[ent->client->resp.context].proposetime < level.time) {
        menu_centerprint(ent, "No changes have been proposed");
        return 2;
    }

    if (ent->client->resp.ra_voted) {
        menu_centerprint(ent, "You have already voted");
        return 2;
    }

    Cmd_arenaadmin_f(ent, 2);

    return 2;
}

void
show_observer_menu(edict_t *ent)
{
    qmenu_t *m;

    m = CreateQMenu(ent, "Observer Options");

    if (!((team_t *)teams[ent->client->resp.teamnum].it)->outofline) {
        AddMenuItem(m, "Change Arena Settings", NULL, -1, menuShowSettingsPropose);
        AddMenuItem(m, "Vote on Changes", NULL, -1, menuShowSettingsVote);
        AddMenuItem(m, "", NULL, -1, NULL);
    }

    if (!arenas[ent->client->resp.context].idarena) {
        AddMenuItem(m, va("Step %s Line",
                          ((team_t *)teams[ent->client->resp.teamnum].it)->outofline ? "into" : "out of"),
                    NULL, -1, menuStepInOutofLine);
        AddMenuItem(m, "", NULL, -1, NULL);
    }

    AddMenuItem(m, "Leave Team", NULL, -1, menuLeaveTeamAr);

    if (!arenas[ent->client->resp.context].idarena)
        AddMenuItem(m, "Leave Arena", NULL, -1, menuLeaveArena);

    FinishMenu(ent, m, 0);
}

void
show_arena_menu(edict_t *ent)
{
    qmenu_t *m;
    int     i;

    m = CreateQMenu(ent, "Choose Your Arena");

    for (i = 1; i <= num_arenas; i++) {
        if (arenas[i].idarena)
            AddMenuItem(m, getarenaname(i), " (PT)", -1, menuAddtoArena);
        else
            AddMenuItem(m, getarenaname(i), " T:",
                        count_queue(&arenas[i].waitingteams) + count_queue(&arenas[i].activeteams),
                        menuAddtoArena);
    }

    AddMenuItem(m, "", NULL, -1, NULL);
    AddMenuItem(m, "Leave Team", NULL, -1, menuLeaveTeam);

    FinishMenu(ent, m, 1);
}

int
menuAddtoTeam(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    if (add_to_team(ent, ((menuitem_t *)item->it)->text)) {
        if (!((team_t *)teams[ent->client->resp.teamnum].it)->arenanum)
            show_arena_menu(ent);
        else {
            ent->client->resp.fightstate = FIGHT_SPECTATING;
            ent->takedamage = 0;
            move_to_arena(ent, ((team_t *)teams[ent->client->resp.teamnum].it)->arenanum, 1);
        }

        return 0;
    }

    menu_centerprint(ent, "That team is already in an arena\nand full or\nthe arena is locked");

    return 2;
}

int
menuNewTeam(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    // The name and its uniquifier moved to arena.c so the bot join uses the
    // same one; the donor's "!"-appending loop had no bound (see RA_NewTeamName).
    add_to_team(ent, RA_NewTeamName(ent));
    show_arena_menu(ent);

    return 0;
}

int
menuRefreshTeamList(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    qmenu_t *m;
    int     i;

    m = CreateQMenu(ent, "Choose your team");
    AddMenuItem(m, "Start New Team", NULL, -1, menuNewTeam);

    for (i = 0; i < MAX_TEAMS; i++) {
        if (teams[i].it)
            AddMenuItem(m, ((team_t *)teams[i].it)->name, " Players: ", count_queue(&teams[i]), menuAddtoTeam);
    }

    AddMenuItem(m, "Refresh List", NULL, -1, menuRefreshTeamList);
    AddMenuItem(m, "", NULL, -1, NULL);
    AddMenuItem(m, "Confused? try /cmd menuhelp", NULL, -1, NULL);

    FinishMenu(ent, m, 1);

    return 2;
}

// R-MENU: A MENU BUILT ONCE IS A SNAPSHOT, and both lobby menus put a number
// in one.  RA2's answer is the "Refresh List" row, which is a real answer for
// 1999 and a poor one now: `minimumplayers` fills the pickup teams while the
// person is still reading the list, and `inven` REOPENS the menu rather than
// rebuilding it (R-MENU-3: closing is hiding), so a player who joined, watched
// four bots arrive on the scoreboard and pressed TAB again saw "Players: 0"
// against every team and no reason to believe otherwise.
//
// Rebuilding on each open was the other candidate and is refused: FinishMenu
// pushes a fresh menu onto the client's queue and nothing pops the old one --
// which is why "Refresh List" leaks a menu per press -- so binding a rebuild to
// a key that is pressed all match long turns a bounded 1999 leak into an
// unbounded one.  The numbers are updated in place instead, which costs no
// allocation and makes them LIVE rather than merely fresh-on-open: MenuThink
// already repaints every ten frames and only needs to be told the composed bar
// is stale.
//
// The two row kinds are matched the two different ways their own callbacks
// resolve them.  A team row is matched by NAME, which add_to_team makes unique,
// because teams are created and freed while the list is open and a row's
// position stops meaning anything.  An arena row is matched by POSITION,
// because that is exactly what menuAddtoArena does with the row it was clicked
// on, and because arena display names come out of the map and may repeat.
bool RA_RefreshMenuCounts(edict_t *ent)
{
    qmenu_t     *node;
    menuitem_t  *item;
    bool        changed = false;
    int         i, idx = 0, num;

    if (!ent->client->curmenulink)
        return false;

    for (node = ((menuinfo_t *)ent->client->curmenulink->it)->items;
         node; node = node->next) {
        item = (menuitem_t *)node->it;
        idx++;
        num = -1;

        if (item->select == menuAddtoTeam) {
            for (i = 0; i < MAX_TEAMS; i++)
                if (teams[i].it && !strcmp(TEAM(&teams[i])->name, item->text)) {
                    num = count_queue(&teams[i]);
                    break;
                }
        } else if (item->select == menuAddtoArena &&
                   idx >= 1 && idx <= num_arenas && !arenas[idx].idarena) {
            num = count_queue(&arenas[idx].waitingteams) +
                  count_queue(&arenas[idx].activeteams);
        }

        // -1 is "this row carries no number", which is a pickup arena's (PT)
        // row and every row that is not one of the two kinds above.  It is also
        // what a team that has been freed since the list was built leaves
        // behind, and leaving the stale count on a dead row is the honest
        // answer: clicking it re-creates the team under that name.
        if (num >= 0 && item->num != num) {
            item->num = num;
            changed = true;
        }
    }

    return changed;
}

int
menuChangeValue10AZ(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    if (arg)
        ((menuitem_t *)item->it)->num += 10;
    else
        ((menuitem_t *)item->it)->num -= 10;

    if (((menuitem_t *)item->it)->num < 0)
        ((menuitem_t *)item->it)->num = 0;

    return 1;
}

int
menuChangeValue50AZ(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    if (arg)
        ((menuitem_t *)item->it)->num += 50;
    else
        ((menuitem_t *)item->it)->num -= 50;

    if (((menuitem_t *)item->it)->num < 0)
        ((menuitem_t *)item->it)->num = 0;

    return 1;
}

int
menuChangeValue50(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    if (arg)
        ((menuitem_t *)item->it)->num += 50;
    else
        ((menuitem_t *)item->it)->num -= 50;

    if (((menuitem_t *)item->it)->num <= 0)
        ((menuitem_t *)item->it)->num = 50;

    return 1;
}

int
menuChangeValue(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    if (arg)
        ((menuitem_t *)item->it)->num++;
    else
        ((menuitem_t *)item->it)->num--;

    if (((menuitem_t *)item->it)->num == 0)
        ((menuitem_t *)item->it)->num = 1;

    return 1;
}

int
menuChangeYesNo(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    menuitem_t  *it = (menuitem_t *)item->it;

    if (it->value[0] == 'Y')
        Q_strlcpy(it->value, "NO ", it->valuesize);
    else
        Q_strlcpy(it->value, "YES", it->valuesize);

    return 1;
}

int
menuChangeProtect(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    int         protect;

    protect = NumForProtect(((menuitem_t *)item->it)->value);

    if (arg)
        protect++;
    else
        protect--;

    if (protect < 0)
        protect = 2;
    if (protect > 2)
        protect = 0;

    Q_strlcpy(((menuitem_t *)item->it)->value, StringForProtect(protect),
              ((menuitem_t *)item->it)->valuesize);

    return 1;
}

int
menuChangeMap(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    // The map name comes out of arena.cfg and the block was sized by the
    // menu's placeholder, so a long entry truncates here rather than running
    // off the end of a TAG_LEVEL allocation (R-SEC-1).
    Q_strlcpy(((menuitem_t *)item->it)->value,
              get_next_map(((menuitem_t *)item->it)->value),
              ((menuitem_t *)item->it)->valuesize);

    return 1;
}

void
cvar_setvalue(char *name, int value)
{
    char    buf[256];

    Q_snprintf(buf, sizeof(buf), "%d", value);
    gi.cvar_set(name, buf);
}

int
menuApplyAdmin(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    qmenu_t     *node;
    menuitem_t  *it;
    edict_t     *e;
    char        *map = NULL;
    size_t      maplen;

    node = (qmenu_t *)menu->it;

    while (node->next) {
        node = node->next;
        it = node->it;

        if (!Q_stricmp(it->text, "Fraglimit:        "))
            cvar_setvalue("fraglimit", it->num);
        else if (!Q_stricmp(it->text, "Timelimit:        "))
            cvar_setvalue("timelimit", it->num);
        else if (!Q_stricmp(it->text, "Mapname:          "))
            map = it->value;
    }

    if (!map)
        return 0;

    e = G_Spawn();
    e->classname = "target_changelevel";
    maplen = strlen(map) + 1;
    e->map = gi.TagMalloc(maplen, TAG_LEVEL);
    memcpy(e->map, map, maplen);

    BeginIntermission(e);

    return 0;
}

int
menuCancel(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    return 0;
}

void
Cmd_menuhelp_f(edict_t *ent)
{
    gi.cprintf(ent, PRINT_HIGH, "H| Use invprev and invnext ([ and ])\nH| to navigate the menu\nH| invuse (ENTER) selects\nH| inven (TAB) toggles it on/off\n");
}

void
Cmd_admin_f(edict_t *ent)
{
    int         code;
    qmenu_t     *m, *mi;

    if (admincode->value == 0)
        return;

    code = atoi(gi.argv(1));

    if ((float) code == admincode->value) {
        m = CreateQMenu(ent, "Admin Menu");

        AddMenuItem(m, "Fraglimit:        ", NULL, (int) fraglimit->value, menuChangeValue10AZ);
        AddMenuItem(m, "Timelimit:        ", NULL, (int) timelimit->value, menuChangeValue10AZ);
        mi = AddMenuItem(m, "Mapname:          ",
                         "                                ", -1, menuChangeMap);
        Q_strlcpy(((menuitem_t *)mi->it)->value, level.mapname,
                  ((menuitem_t *)mi->it)->valuesize);

        AddMenuItem(m, "", NULL, -1, NULL);
        AddMenuItem(m, "Apply", NULL, -1, menuApplyAdmin);
        AddMenuItem(m, "Cancel", NULL, -1, menuCancel);

        FinishMenu(ent, m, 1);
    } else
        gi.cprintf(ent, PRINT_HIGH, "Sorry, incorrect admin code\n");
}

int
menuApplyArenaAdmin(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    qmenu_t     *node;
    menuitem_t  *it;
    int         *settings = NULL;
    int         arenanum = 0;
    int         weapons;

    node = (qmenu_t *)menu->it;

    while (node->next) {
        node = node->next;
        it = node->it;

        if (!Q_stricmp(it->text, "Arena:                 ")) {
            arenanum = it->num;

            if (((menuitem_t *)item->it)->text[0] == 'A') {
                settings = &arenas[arenanum].playersperteam;
            } else {
                if (arenas[arenanum].proposetime > level.time) {
                    menu_centerprint(ent,
                                     va("Voting is in progress.\nPlease wait %d seconds",
                                        (int)(arenas[ent->client->resp.context].proposetime - level.time)));
                    return 2;
                }

                memcpy(&arenas[arenanum].proposed, &arenas[arenanum].playersperteam,
                       sizeof(arena_settings_t));
                settings = &arenas[arenanum].proposed.playersperteam;
                start_voting(ent, arenanum);
                arenas[arenanum].votes_yes++;
                ent->client->resp.ra_voted = true;
            }

            settings[41] = 1;
            weapons = 0;

            if (!settings[28])
                weapons |= (settings[2] & weapon_vals[0]) ? weapon_vals[0] : 0;
            if (!settings[29])
                weapons |= (settings[2] & weapon_vals[1]) ? weapon_vals[1] : 0;
            if (!settings[30])
                weapons |= (settings[2] & weapon_vals[2]) ? weapon_vals[2] : 0;
            if (!settings[31])
                weapons |= (settings[2] & weapon_vals[3]) ? weapon_vals[3] : 0;
            if (!settings[32])
                weapons |= (settings[2] & weapon_vals[4]) ? weapon_vals[4] : 0;
            if (!settings[33])
                weapons |= (settings[2] & weapon_vals[5]) ? weapon_vals[5] : 0;
            if (!settings[34])
                weapons |= (settings[2] & weapon_vals[6]) ? weapon_vals[6] : 0;
            if (!settings[35])
                weapons |= (settings[2] & weapon_vals[7]) ? weapon_vals[7] : 0;
            if (!settings[36])
                weapons |= (settings[2] & weapon_vals[8]) ? weapon_vals[8] : 0;

            settings[2] = weapons;
        } else if (!settings) {
            continue;       // no "Arena:" row, so there is no base to write to
        } else if (!Q_stricmp(it->text, "Players per team:      ")) {
            settings[0] = it->num;
        } else if (!Q_stricmp(it->text, "Initial Health:        ")) {
            settings[4] = it->num;
        } else if (!Q_stricmp(it->text, "Initial Armor:         ")) {
            settings[3] = it->num;
        } else if (!Q_stricmp(it->text, "Minimum Ping:          ")) {
            settings[5] = it->num;
        } else if (!Q_stricmp(it->text, "Maximum Ping:          ")) {
            settings[6] = it->num;
        } else if (!Q_stricmp(it->text, "Rounds:                ")) {
            settings[1] = (it->num / 2) * 2 + 1;
        } else if (!Q_stricmp(it->text, "Allow Shotgun:         ")) {
            settings[2] |= (it->value[0] == 'Y') ? weapon_vals[0] : 0;
        } else if (!Q_stricmp(it->text, "Allow Super Shotgun:   ")) {
            settings[2] |= (it->value[0] == 'Y') ? weapon_vals[1] : 0;
        } else if (!Q_stricmp(it->text, "Allow Machine gun:     ")) {
            settings[2] |= (it->value[0] == 'Y') ? weapon_vals[2] : 0;
        } else if (!Q_stricmp(it->text, "Allow Chain gun:       ")) {
            settings[2] |= (it->value[0] == 'Y') ? weapon_vals[3] : 0;
        } else if (!Q_stricmp(it->text, "Allow Grenade Launcher:")) {
            settings[2] |= (it->value[0] == 'Y') ? weapon_vals[4] : 0;
        } else if (!Q_stricmp(it->text, "Allow Rocket Launcher: ")) {
            settings[2] |= (it->value[0] == 'Y') ? weapon_vals[5] : 0;
        } else if (!Q_stricmp(it->text, "Allow Hyperblaster:    ")) {
            settings[2] |= (it->value[0] == 'Y') ? weapon_vals[6] : 0;
        } else if (!Q_stricmp(it->text, "Allow Railgun:         ")) {
            settings[2] |= (it->value[0] == 'Y') ? weapon_vals[7] : 0;
        } else if (!Q_stricmp(it->text, "Allow BFG10K:          ")) {
            settings[2] |= (it->value[0] == 'Y') ? weapon_vals[8] : 0;
        } else if (!Q_stricmp(it->text, "Health: ")) {
            settings[17] = NumForProtect(it->value);
        } else if (!Q_stricmp(it->text, "Armor:  ")) {
            settings[16] = NumForProtect(it->value);
        } else if (!Q_stricmp(it->text, "Falling Damage:        ")) {
            settings[18] = (it->value[0] == 'Y');
        } else if (!Q_stricmp(it->text, "Lock Arena:            ")) {
            settings[38] = (it->value[0] == 'Y');
        } else if (!Q_stricmp(it->text, "Competition Mode:      ")) {
            settings[39] = (it->value[0] == 'Y');
        } else if (!Q_stricmp(it->text, "Damage Scoring:        ")) {
            settings[40] = (it->value[0] == 'Y');
        }
    }

    if (arenanum)
        check_teams(arenanum);

    return 0;
}

int
menuVote(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    if (arenas[ent->client->resp.context].proposetime < level.time) {
        menu_centerprint(ent, "Sorry, voting is over");
        return 2;
    }

    if (ent->client->resp.ra_voted) {
        menu_centerprint(ent, "You have already voted");
        return 2;
    }

    if (((menuitem_t *)item->it)->value[0] == 'Y')
        arenas[ent->client->resp.context].votes_yes++;
    else
        arenas[ent->client->resp.context].votes_no++;

    ent->client->resp.ra_voted = true;

    return 0;
}

void
Cmd_arenaadmin_f(edict_t *ent, unsigned mode)
{
    qmenu_t         *m = NULL;
    menuselect_t    changevalue;
    menuselect_t    changevalue50;
    menuselect_t    changevalue50az;
    menuselect_t    changeyesno;
    menuselect_t    changeprotect;
    int             *vals;
    int             *live;
    int             arenanum = 0;
    int             code;

    switch (mode) {
    case 0:
        if (admincode->value == 0)
            return;

        code = atoi(gi.argv(1));
        arenanum = atoi(gi.argv(2));

        if ((float) code != admincode->value)
            return;

        if (!arenanum) {
        case 1:
            arenanum = ent->client->resp.context;
        }

        if (arenanum < 1 || arenanum > num_arenas)
            return;

        changevalue = menuChangeValue;
        changevalue50 = menuChangeValue50;
        changevalue50az = menuChangeValue50AZ;
        changeyesno = menuChangeYesNo;
        changeprotect = menuChangeProtect;

        vals = &arenas[arenanum].playersperteam;
        break;

    case 2:
        arenanum = ent->client->resp.context;

        if (arenanum < 1 || arenanum > num_arenas)
            return;

        changevalue50 = NULL;
        changevalue50az = NULL;
        changeprotect = NULL;
        changeyesno = NULL;
        changevalue = NULL;

        // R-SEC-8: this was `(int *)&arenas[arenanum].proposetime + 1`, which
        // takes the address of a float, reads it as int* and steps one int
        // forward hoping to land on `proposed` -- undefined, and correct only
        // while sizeof(float) == sizeof(int) and nothing pads between the two
        // members.  The member it meant is nameable.
        vals = &arenas[arenanum].proposed.playersperteam;
        live = &arenas[arenanum].playersperteam;

        m = CreateQMenu(ent, "Proposed Changes");
        AddMenuItem(m, "Arena:                 ", NULL, arenanum, NULL);

        if (arenas[arenanum].idarena != 1 && vals[0] != live[0])
            AddMenuItem(m, "Players per team:      ", NULL, vals[0], NULL);
        if (vals[4] != live[4])
            AddMenuItem(m, "Initial Health:        ", NULL, vals[4], changevalue50);
        if (vals[3] != live[3])
            AddMenuItem(m, "Initial Armor:         ", NULL, vals[3], changevalue50az);
        if (arenas[arenanum].idarena != 1) {
            if (vals[5] != live[5])
                AddMenuItem(m, "Minimum Ping:          ", NULL, vals[5], changevalue50az);
            if (vals[6] != live[6])
                AddMenuItem(m, "Maximum Ping:          ", NULL, vals[6], changevalue50az);
        }
        if (vals[1] != live[1])
            AddMenuItem(m, "Rounds:                ", NULL, vals[1], changevalue);
        if ((vals[2] & weapon_vals[0]) != (live[2] & weapon_vals[0]))
            AddMenuItem(m, "Allow Shotgun:         ", (vals[2] & weapon_vals[0]) ? "YES" : "NO ", -1, changeyesno);
        if ((vals[2] & weapon_vals[1]) != (live[2] & weapon_vals[1]))
            AddMenuItem(m, "Allow Super Shotgun:   ", (vals[2] & weapon_vals[1]) ? "YES" : "NO ", -1, changeyesno);
        if ((vals[2] & weapon_vals[2]) != (live[2] & weapon_vals[2]))
            AddMenuItem(m, "Allow Machine gun:     ", (vals[2] & weapon_vals[2]) ? "YES" : "NO ", -1, changeyesno);
        if ((vals[2] & weapon_vals[3]) != (live[2] & weapon_vals[3]))
            AddMenuItem(m, "Allow Chain gun:       ", (vals[2] & weapon_vals[3]) ? "YES" : "NO ", -1, changeyesno);
        if ((vals[2] & weapon_vals[4]) != (live[2] & weapon_vals[4]))
            AddMenuItem(m, "Allow Grenade Launcher:", (vals[2] & weapon_vals[4]) ? "YES" : "NO ", -1, changeyesno);
        if ((vals[2] & weapon_vals[5]) != (live[2] & weapon_vals[5]))
            AddMenuItem(m, "Allow Rocket Launcher: ", (vals[2] & weapon_vals[5]) ? "YES" : "NO ", -1, changeyesno);
        if ((vals[2] & weapon_vals[6]) != (live[2] & weapon_vals[6]))
            AddMenuItem(m, "Allow Hyperblaster:    ", (vals[2] & weapon_vals[6]) ? "YES" : "NO ", -1, changeyesno);
        if ((vals[2] & weapon_vals[7]) != (live[2] & weapon_vals[7]))
            AddMenuItem(m, "Allow Railgun:         ", (vals[2] & weapon_vals[7]) ? "YES" : "NO ", -1, changeyesno);
        if ((vals[2] & weapon_vals[8]) != (live[2] & weapon_vals[8]))
            AddMenuItem(m, "Allow BFG10K:          ", (vals[2] & weapon_vals[8]) ? "YES" : "NO ", -1, changeyesno);
        if (vals[17] != live[17])
            AddMenuItem(m, "Health: ", StringForProtect(vals[17]), -1, changeprotect);
        if (vals[16] != live[16])
            AddMenuItem(m, "Armor:  ", StringForProtect(vals[16]), -1, changeprotect);
        if (vals[18] != live[18])
            AddMenuItem(m, "Falling Damage:        ", vals[18] ? "YES" : "NO ", -1, changeyesno);
        if (vals[39] != live[39])
            AddMenuItem(m, "Competition Mode:      ", vals[39] ? "YES" : "NO ", -1, changeyesno);
        if (vals[40] != live[40])
            AddMenuItem(m, "Damage Scoring:        ", vals[40] ? "YES" : "NO ", -1, changeyesno);

        AddMenuItem(m, "", NULL, -1, NULL);
        break;

    default:
        return;
    }

    if (mode != 2) {
        m = CreateQMenu(ent, "Arena Admin Menu");
        AddMenuItem(m, "Arena:                 ", NULL, arenanum, NULL);

        if (arenas[arenanum].idarena != 1) {
            if (!mode || vals[23])
                AddMenuItem(m, "Players per team:      ", NULL, vals[0], changevalue);
        }

        if (!mode || vals[20])
            AddMenuItem(m, "Initial Health:        ", NULL, vals[4], changevalue50);
        if (!mode || vals[19])
            AddMenuItem(m, "Initial Armor:         ", NULL, vals[3], changevalue50az);

        if (arenas[arenanum].idarena != 1) {
            if (!mode || vals[21])
                AddMenuItem(m, "Minimum Ping:          ", NULL, vals[5], changevalue50az);
            if (!mode || vals[22])
                AddMenuItem(m, "Maximum Ping:          ", NULL, vals[6], changevalue50az);
        }

        if (!mode || vals[24])
            AddMenuItem(m, "Rounds:                ", NULL, vals[1], changevalue);
        if (!mode || vals[28])
            AddMenuItem(m, "Allow Shotgun:         ", (vals[2] & weapon_vals[0]) ? "YES" : "NO ", -1, changeyesno);
        if (!mode || vals[29])
            AddMenuItem(m, "Allow Super Shotgun:   ", (vals[2] & weapon_vals[1]) ? "YES" : "NO ", -1, changeyesno);
        if (!mode || vals[30])
            AddMenuItem(m, "Allow Machine gun:     ", (vals[2] & weapon_vals[2]) ? "YES" : "NO ", -1, changeyesno);
        if (!mode || vals[31])
            AddMenuItem(m, "Allow Chain gun:       ", (vals[2] & weapon_vals[3]) ? "YES" : "NO ", -1, changeyesno);
        if (!mode || vals[32])
            AddMenuItem(m, "Allow Grenade Launcher:", (vals[2] & weapon_vals[4]) ? "YES" : "NO ", -1, changeyesno);
        if (!mode || vals[33])
            AddMenuItem(m, "Allow Rocket Launcher: ", (vals[2] & weapon_vals[5]) ? "YES" : "NO ", -1, changeyesno);
        if (!mode || vals[34])
            AddMenuItem(m, "Allow Hyperblaster:    ", (vals[2] & weapon_vals[6]) ? "YES" : "NO ", -1, changeyesno);
        if (!mode || vals[35])
            AddMenuItem(m, "Allow Railgun:         ", (vals[2] & weapon_vals[7]) ? "YES" : "NO ", -1, changeyesno);
        if (!mode || vals[36])
            AddMenuItem(m, "Allow BFG10K:          ", (vals[2] & weapon_vals[8]) ? "YES" : "NO ", -1, changeyesno);
        if (!mode || vals[27])
            AddMenuItem(m, "Health: ", StringForProtect(vals[17]), -1, changeprotect);
        if (!mode || vals[26])
            AddMenuItem(m, "Armor:  ", StringForProtect(vals[16]), -1, changeprotect);
        if (!mode || vals[37])
            AddMenuItem(m, "Falling Damage:        ", vals[18] ? "YES" : "NO ", -1, changeyesno);
        if (!mode)
            AddMenuItem(m, "Lock Arena:            ", vals[38] ? "YES" : "NO ", -1, changeyesno);
        AddMenuItem(m, "Competition Mode:      ", vals[39] ? "YES" : "NO ", -1, changeyesno);
        AddMenuItem(m, "Damage Scoring:        ", vals[40] ? "YES" : "NO ", -1, changeyesno);
        AddMenuItem(m, "", NULL, -1, NULL);
    }

    if (!m)
        return;

    switch (mode) {
    case 0:
        AddMenuItem(m, "Apply", NULL, -1, menuApplyArenaAdmin);
        // falls through -- an admin gets Apply AND Propose, a player only Propose
    case 1:
        AddMenuItem(m, "Propose", NULL, -1, menuApplyArenaAdmin);
        break;
    case 2:
        AddMenuItem(m, "Vote ", "Yes", -1, menuVote);
        AddMenuItem(m, "Vote ", "No", -1, menuVote);
        break;
    }

    AddMenuItem(m, "Cancel", NULL, -1, menuCancel);
    FinishMenu(ent, m, 1);
}

int
menuMotdContinue(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    ent->client->pers.showmotd = false;
    menuRefreshTeamList(ent, NULL, NULL, 0);

    return 0;
}

void
motd_menu(edict_t *ent)
{
    qmenu_t *m;
    motd_t  *node;

    if (motd.next == NULL) {
        menuMotdContinue(ent, NULL, NULL, 0);
        return;
    }

    m = CreateQMenu(ent, "Message of the Day");
    AddMenuItem(m, "---------Continue----------", NULL, -1, menuMotdContinue);

    node = &motd;
    while (node->next) {
        node = node->next;
        AddMenuItem(m, node->line, NULL, -1, NULL);
    }

    FinishMenu(ent, m, 1);
}

int
menuNo(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    return 0;
}

int
menuTeamConfirm(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg)
{
    AddtoArena(ent, ((menuitem_t *)item->it)->num, 1, 0);

    return 1;
}

void
show_teamconfirm_menu(edict_t *ent, int arenanum)
{
    qmenu_t *m;

    m = CreateQMenu(ent, "Confirmation");

    AddMenuItem(m, "You have too few players", NULL, -1, NULL);
    AddMenuItem(m, "Do you wish to continue?", NULL, -1, NULL);
    AddMenuItem(m, "", NULL, -1, NULL);

    AddMenuItem(m, "Yes, continue to arena ", NULL, arenanum, menuTeamConfirm);
    AddMenuItem(m, "No, choose another", NULL, -1, menuNo);

    FinishMenu(ent, m, 1);
}

void
menu_centerprint(edict_t *ent, char *message)
{
    qmenu_t     *m;
    menuinfo_t  *info;
    char        *dst;
    char        *src;
    char        *line;
    char        *lastspace;
    int         linelen;
    char        c;
    char        buf[2048];

    src = message;
    dst = buf;
    lastspace = NULL;
    line = buf;
    linelen = 0;

    // !ent->client as well as the owner test: G_UseTargets routes every
    // entity message through here and its activator is not always a client --
    // a func_button firing a trigger with a message reaches this with one that
    // is not.  RA2 never saw it because arena maps do not have those.
    if (!ent->client || ent->client->menu_owner != MENU_ARENA) {
        gi.centerprintf(ent, "%s", message);
        return;
    }

    m = ent->client->curmenulink;
    if (m != NULL) {
        info = m->it;

        if (!strcmp(info->title, "Message")) {
            ent->client->menuusetime = 0;
            UseMenu(ent, 1);
        }
    }

    m = CreateQMenu(ent, "Message");
    AddMenuItem(m, "---------Continue----------", NULL, -1, menuNo);

    // R-SEC-1, and R-SEC-2's kept-bug list only names half of it.  The list
    // says `lastspace` is never reset after a wrap, which is why this buffer is
    // 2048 bytes and the text is never compacted back to the start.  The other
    // half is that `dst` was never bounded at all: `message` reaches here from
    // `va()` and, through G_UseTargets, from an entity's `message` key -- so a
    // map with a long enough string on a trigger overflowed 2048 bytes of stack
    // under `arena`.  `bounded.py` cannot see this one: it is a hand-rolled
    // copy loop and not one of the five names that tool bans, which is worth
    // knowing about the check as much as about the bug.
    while ((c = *src++) != 0) {
        if (dst >= buf + sizeof(buf) - 1)
            break;
        *dst++ = c;
        linelen++;

        if (c == ' ' || c == '\n') {
            lastspace = dst - 1;
            *lastspace = ' ';
        }

        if (linelen >= 27) {
            if (lastspace)
                *lastspace = '\0';
            else
                *dst = '\0';

            AddMenuItem(m, line, NULL, -1, NULL);
            linelen -= strlen(line);

            if (lastspace)
                line = lastspace + 1;
            else
                line = dst;
        }
    }

    *dst = '\0';
    AddMenuItem(m, line, NULL, -1, NULL);

    FinishMenu(ent, m, 1);
}
