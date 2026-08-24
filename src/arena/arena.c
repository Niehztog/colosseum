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
#include "ctf/g_ctf.h"
#include "arena/ra2stats.h"

// R-OSP-5: no local `extern` of a donor object.  arena.h already declares
// `votetries_setting`, and a second declaration in a .c file is the shape that
// let the donor's `extern int botglobals;` resolve a four-byte int over the
// first member of a struct.  Found by donorgate.py's stray-extern pass.
bool    allow_grapple;
bool    broken = false;

arena_t     arenas[MAX_ARENAS];
int         num_arenas;
bool    idmap;

qmenu_t     *teams;

motd_t      motd;
cvar_t      *admincode;

char        *teamskins[MAX_ARENA_SKINS] = {
    "r2red", "r2blue", "r2dgre", "r2oran", "r2yell", "r2aqua", "r2lgre"
};

char        *vwepmodels[4] = {
    "male", "female", "cyborg", "crakhor"
};

int    teamskins_precachem[MAX_ARENA_SKINS];
int    teamskins_precachef[MAX_ARENA_SKINS];
int    teamskins_precachecw[MAX_ARENA_SKINS];
int    teamskins_precachecb[MAX_ARENA_SKINS];

char        *omode_descriptions[4] = {
    "Normal", "Free Flying", "Trackcam", "In Eyes"
};


void        teleporter_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf);

void        P_ProjectSource(gclient_t *client, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result);

void        load_config(int numarenas);
void        set_config(int first, int last);
void        load_motd(void);

void        show_observer_menu(edict_t *ent);
void        show_arena_menu(edict_t *ent);
void        show_teamconfirm_menu(edict_t *ent, int arenanum);

void
add_to_queue(qmenu_t *node, qmenu_t *head)
{
    for (; head->next; head = head->next)
        ;

    head->next = node;
    node->prev = head;
    node->next = NULL;
}

qmenu_t *
remove_from_queue(qmenu_t *node, qmenu_t *head)
{
    if (!node) {
        if (head)
            node = head->next;

        if (!node)
            return NULL;
    }

    if (node->prev)
        node->prev->next = node->next;
    if (node->next)
        node->next->prev = node->prev;

    node->prev = NULL;
    node->next = NULL;

    return node;
}

void
add_to_front_queue(qmenu_t *node, qmenu_t *head)
{
    remove_from_queue(node, NULL);

    node->prev = head;
    node->next = head->next;
    if (head->next)
        head->next->prev = node;
    head->next = node;
}

int count_queue(qmenu_t *head)
{
    int     count;

    count = 0;
    while (head->next) {
        head = head->next;
        count++;
    }

    return count;
}

int count_players_queue(qmenu_t *head)
{
    int         count;

    count = 0;
    while (head->next) {
        head = head->next;
        if (((edict_t *)head->it)->client->resp.fightstate == FIGHT_ALIVE)
            count++;
    }

    return count;
}

static q_unused team_t *TeamFromNode(qmenu_t *node)
{
    return (team_t *)((qmenu_t *)node->it)->it;
}

void set_damage(int arenanum, int state)
{
    qmenu_t     *tnode, *mnode;
    edict_t     *e;

    tnode = &arenas[arenanum].activeteams;

    while (tnode->next) {
        tnode = tnode->next;

        mnode = (qmenu_t *)tnode->it;

        while (mnode->next) {
            mnode = mnode->next;
            e = (edict_t *)mnode->it;
            if (e->client->resp.fightstate)
                e->takedamage = state;
        }
    }
}

void give_ammo(edict_t *e)
{
    const gitem_t   *w[9];
    arena_t     *arena = &arenas[e->client->resp.context];
    //    0  2  3  4  5   6    9   8   7
    int         weapon_vals_x[] = { 256, 1, 2, 4, 8, 16, 128, 64, 32 };
    const gitem_t   *it, *rl;
    bool    needswitch;
    int         i;

    // give health
    if (arena->health)
        e->health = arena->health;
    else
        e->health = 100;

    // give weapons
    rl = NULL;
    memset(w, 0, sizeof(w));

    w[0] = FindItemByClassname("weapon_bfg");
    w[1] = FindItemByClassname("weapon_shotgun");
    w[2] = FindItemByClassname("weapon_supershotgun");
    w[3] = FindItemByClassname("weapon_machinegun");
    w[4] = FindItemByClassname("weapon_chaingun");
    w[5] = FindItemByClassname("weapon_grenadelauncher");
    w[6] = FindItemByClassname("weapon_railgun");
    w[7] = FindItemByClassname("weapon_hyperblaster");
    w[8] = FindItemByClassname("weapon_rocketlauncher");

    needswitch = false;

    for (i = 8; i >= 0; i--) {
        if (arena->weapons & weapon_vals_x[i]) {
            if (!rl)
                rl = w[i];

            if (!e->client->pers.inventory[ITEM_INDEX(rl)] || needswitch) {
                e->client->newweapon = rl;
                e->client->pers.selected_item =
                    e->client->ps.stats[STAT_SELECTED_ITEM] = ITEM_INDEX(rl);
                needswitch = false;
            }

            e->client->pers.inventory[ITEM_INDEX(w[i])] = 1;
        } else {
            if (e->client->pers.weapon == w[i])
                needswitch = true;

            e->client->pers.inventory[ITEM_INDEX(w[i])] = 0;
        }
    }

    if (needswitch) {
        rl = FindItemByClassname("weapon_blaster");
        e->client->newweapon = rl;
        e->client->pers.selected_item =
            e->client->ps.stats[STAT_SELECTED_ITEM] = ITEM_INDEX(rl);
    }

    // give ammo
    if ((it = FindItemByClassname("ammo_shells"))) e->client->pers.inventory[ITEM_INDEX(it)] = arena->shells;
    if ((it = FindItemByClassname("ammo_bullets"))) e->client->pers.inventory[ITEM_INDEX(it)] = arena->bullets;
    if ((it = FindItemByClassname("ammo_slugs"))) e->client->pers.inventory[ITEM_INDEX(it)] = arena->slugs;
    if ((it = FindItemByClassname("ammo_grenades"))) e->client->pers.inventory[ITEM_INDEX(it)] = arena->grenades;
    if ((it = FindItemByClassname("ammo_rockets"))) e->client->pers.inventory[ITEM_INDEX(it)] = arena->rockets;
    if ((it = FindItemByClassname("ammo_cells"))) e->client->pers.inventory[ITEM_INDEX(it)] = arena->cells;

    // give body armor
    if ((it = FindItemByClassname("item_armor_body")))
        e->client->pers.inventory[ITEM_INDEX(it)] = arena->armor;

    if (allow_grapple) {
        it = FindItem("Grapple");
        if (it)
            e->client->pers.inventory[ITEM_INDEX(it)] = 1;
    }
}

team_t *add_to_team(edict_t *ent, char *teamname)
{
    int     i;
    team_t  *t;

    for (i = 0; i < MAX_TEAMS; i++) {
        if (!teams[i].it)
            continue;

        t = teams[i].it;
        if (strcmp(t->name, teamname))
            continue;

        if (t->arenanum) {
            if (count_queue(&teams[i]) == arenas[t->arenanum].playersperteam)
                return NULL;

            if (arenas[t->arenanum].locked)
                return NULL;

            if (t->fighting)
                RA2_Stats_AddPlayer(arenas[t->arenanum].stats, ent, i);
        }

        add_to_queue(&ent->client->resp.teammember, &teams[i]);
        ent->client->resp.teamnum = i;

        if (t->skin != -1)
            setteamskin(ent, ent->client->pers.userinfo, t->skin);

        gi.bprintf(PRINT_MEDIUM, "%s has been added to team %d (%s)\n",
                   ent->client->pers.netname, i, teamname);

        return t;
    }

    for (i = 0; i < MAX_TEAMS; i++)
        if (!teams[i].it)
            break;

    t = gi.TagMalloc(sizeof(team_t), TAG_LEVEL);
    if (!t) {
        gi.error("Ateam malloc failed!\n");
        return NULL;
    }

    t->name = teamname;
    t->teamnum = i;
    t->arenanum = 0;
    t->wins = -1;
    t->skin = -1;
    t->arenalink.it = &teams[i];
    t->fighting = 0;
    teams[i].it = t;

    if (ent) {
        add_to_queue(&t->arenalink, &arenas[0].waitingteams);
        t->locked = false;
        t->side = -1;

        add_to_queue(&ent->client->resp.teammember, &teams[i]);
        ent->client->resp.teamnum = i;

        gi.bprintf(PRINT_MEDIUM, "%s has created team number %d (%s)\n",
                   ent->client->pers.netname, i, teamname);
    } else
        t->locked = true;

    return t;
}

void remove_from_team(edict_t *ent)
{
    qmenu_t *node;

    if (ent->client->resp.teamnum < 0)
        return;

    node = &ent->client->resp.teammember;

    if (!TEAM(&teams[ent->client->resp.teamnum])->name) {
        gi.dprintf("ERROR in remove_from_team -- please e-mail crt\n");
        return;
    }

    gi.bprintf(PRINT_MEDIUM, "%s has been removed from team %d (%s)\n",
               ent->client->pers.netname, ent->client->resp.teamnum,
               TEAM(&teams[ent->client->resp.teamnum])->name);

    if (TEAM(&teams[ent->client->resp.teamnum])->fighting)
        RA2_Stats_RemovePlayer(arenas[ent->client->resp.context].stats,
                               ent - g_edicts);

    remove_from_queue(node, NULL);

    check_teams(ent->client->resp.context);

    ent->client->resp.teamnum = -1;
}

edict_t *SelectRandomArenaSpawnPoint(char *classn, int arenanum, int side)
{
    edict_t     *spot;
    int         count = 0;
    int         selection;

    spot = NULL;
    while ((spot = G_Find(spot, FOFS(classname), classn)) != NULL) {
        if (spot->arena != arenanum && idmap == false) continue;
        count++;
    }

    if (!count)
        return NULL;

    selection = rand() % count;

    //gi.dprintf("%d spots, %d selected\n",count,selection);

    if (side) {
        selection &= ~1;
        if (side == 1) {
            selection++;
            if (selection >= count)
                selection = 1;
        }
    }

    spot = NULL;
    do {
        spot = G_Find(spot, FOFS(classname), classn);
        if (spot->arena != arenanum && idmap == false)
            selection++;
    } while (selection--);

    return spot;
}

edict_t *SelectFarthestArenaSpawnPoint(char *classn, int arenanum)
{
    edict_t     *bestspot;
    float       bestdistance, bestplayerdistance;
    edict_t     *spot;

    spot = NULL;
    bestspot = NULL;
    bestdistance = 50;
    while ((spot = G_Find(spot, FOFS(classname), classn)) != NULL) {
        //gi.bprintf (PRINT_HIGH,"arena %d spot %d\n", arenanum, spot->arena);
        if (spot->arena != arenanum && idmap == false) continue;
        bestplayerdistance = PlayersRangeFromSpot(spot);

        if (bestplayerdistance > bestdistance) {
            bestspot = spot;
            bestdistance = bestplayerdistance;
        }
    }

    if (bestspot) {
        return bestspot;
    }

    // if there is a player just spawned on each and every start spot
    // we have no choice to turn one into a telefrag meltdown
    return SelectRandomArenaSpawnPoint(classn, arenanum, 0);
}

void track_SetStats(edict_t *ent)
{
    edict_t *target;
    int     score;

    target = ent->client->resp.track_target;
    score = ent->client->resp.score;

    memcpy(ent->client->ps.stats, target->client->ps.stats,
           sizeof(ent->client->ps.stats));

    ent->client->ps.stats[STAT_FRAGS] = score;

    if (ent->client->scoremode)
        ent->client->ps.stats[STAT_LAYOUTS] |= LAYOUTS_LAYOUT;
    else
        ent->client->ps.stats[STAT_LAYOUTS] &= ~LAYOUTS_LAYOUT;

    CTFSetIDView(ent);
}

void eyecam_think(edict_t *ent, usercmd_t *ucmd)
{
    edict_t     *target;
    vec3_t      forward;
    vec3_t      dest;
    vec3_t      zero = {0, 0, 0};
    int         i;

    target = ent->client->resp.track_target;
    if (!target || target->client->resp.fightstate != FIGHT_ALIVE) {
        track_next(ent);
        return;
    }

    gi.unlinkentity(ent);

    VectorCopy(target->s.origin, ent->s.origin);

    AngleVectors(target->client->v_angle, forward, NULL, NULL);
    VectorScale(forward, 20, dest);

    ent->s.origin[0] = ent->s.origin[0] + dest[0];
    ent->s.origin[1] = ent->s.origin[1] + dest[1];
    ent->s.origin[2] = ent->s.origin[2] + dest[2] + 22;

    VectorCopy(zero, ent->velocity);

    VectorCopy(target->client->v_angle, ent->s.angles);
    VectorCopy(target->client->v_angle, ent->client->ps.viewangles);
    VectorCopy(target->client->v_angle, ent->client->v_angle);

    for (i = 0; i < 3; i++)
        ent->client->ps.pmove.delta_angles[i] =
            ANGLE2SHORT(ent->s.angles[i] - ent->client->resp.cmd_angles[i]);

    gi.linkentity(ent);

    track_SetStats(ent);
}

void track_think(edict_t *ent, usercmd_t *ucmd)
{
    edict_t     *target;
    vec3_t      forward;
    vec3_t      dest;
    int         stuck = 0;
    vec3_t      zero = {0, 0, 0};
    vec3_t      mins = {-16, -16, -24};
    vec3_t      maxs = {16, 16, 32};
    trace_t     tr;
    int         i;

    target = ent->client->resp.track_target;
    if (!target || target->client->resp.fightstate != FIGHT_ALIVE) {
        track_next(ent);
        return;
    }

    VectorCopy(ent->client->ps.viewangles, forward);
    AngleVectors(forward, forward, NULL, NULL);

    VectorScale(forward, 150, dest);
    VectorSubtract(target->s.origin, dest, dest);

    tr = gi.trace(target->s.origin, zero, zero, dest, target, MASK_SOLID);
    if (tr.fraction < 1.0f) {
        VectorScale(forward, tr.fraction * -130, dest);
        VectorAdd(target->s.origin, dest, dest);
    }

    tr = gi.trace(ent->s.origin, mins, maxs, ent->s.origin, ent,
                  MASK_PLAYERSOLID);
    if (tr.contents & MASK_SOLID)
        stuck = 1;

    if (!stuck)
        tr = gi.trace(ent->s.origin, mins, maxs, dest, ent, MASK_SOLID);

    if (tr.fraction < 1.0f || stuck) {
        gi.unlinkentity(ent);
        VectorCopy(dest, ent->s.origin);
        gi.linkentity(ent);
        VectorCopy(zero, ent->velocity);
    } else {
        VectorSubtract(dest, ent->s.origin, dest);
        for (i = 0; i < 3; i++)
            ent->velocity[i] = dest[i] * 10;
    }

    track_SetStats(ent);
}

void track_change(edict_t *ent, int dir)
{
    edict_t     *target, *e;
    int         i;
    bool    wrapped;

    wrapped = false;

    target = ent->client->resp.track_target;
    if (!target) {
        target = &g_edicts[1];
        wrapped = true;
    } else if (target->client->resp.fightstate != FIGHT_ALIVE ||
               target->client->resp.context != ent->client->resp.context)
        wrapped = true;

    i = target - g_edicts;

    do {
        i += dir;
        if ((float) i > game.maxclients)
            i = 1;
        if (i < 1)
            i = (int) game.maxclients;

        e = &g_edicts[i];

        if (e->inuse &&
            e->client->resp.fightstate == FIGHT_ALIVE &&
            e->client->resp.context == ent->client->resp.context &&
            (!arenas[ent->client->resp.context].competition ||
             ent->client->resp.teamnum == e->client->resp.teamnum) &&
            e->solid) {
            wrapped = false;
            break;
        }
    } while (e != target);

    if (e != target || !wrapped) {
        ent->client->resp.track_target = e;
        gi.cprintf(ent, PRINT_HIGH, "Tracking %s\n", e->client->pers.netname);
        return;
    }

    ent->client->resp.omode = ent->client->resp.lastomode;
    move_to_arena(ent, ent->client->resp.context, 2);
    gi.cprintf(ent, PRINT_HIGH, "No one to track\n");
}

void track_next(edict_t *ent)
{
    track_change(ent, 1);
}

void track_prev(edict_t *ent)
{
    track_change(ent, -1);
}

void SetObserverMode(edict_t *ent)
{
    int     i;

    switch (ent->client->resp.omode) {
    case OMODE_NORMAL:
        ent->movetype = MOVETYPE_WALK;
        ent->solid = SOLID_BBOX;
        ent->clipmask = MASK_PLAYERSOLID;
        ent->svflags &= ~SVF_NOCLIENT;
        ent->client->resp.track_target = NULL;
        ent->s.modelindex = 255;
        ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
        break;

    case OMODE_FREEFLYING:
        ent->movetype = MOVETYPE_NOCLIP;
        ent->solid = SOLID_NOT;
        ent->clipmask = 0;
        ent->svflags |= SVF_NOCLIENT;
        ent->client->resp.track_target = NULL;
        ent->client->ps.pmove.pm_time = 0;
        ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
        ent->client->ps.pmove.pm_flags &= ~PMF_TIME_TELEPORT;
        break;

    case OMODE_TRACKCAM:
        ent->movetype = MOVETYPE_NOCLIP;
        ent->solid = SOLID_NOT;
        ent->clipmask = 0;
        ent->svflags |= SVF_NOCLIENT;
        ent->s.modelindex = 0;
        ent->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;

        for (i = 0; i < 3; i++) {
            ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(-ent->client->resp.cmd_angles[i]);
            ent->s.angles[i] = 0;
        }

        VectorCopy(ent->s.angles, ent->client->ps.viewangles);
        VectorCopy(ent->s.angles, ent->client->v_angle);

        if (!ent->client->resp.track_target ||
            ent->client->resp.track_target->client->resp.fightstate != FIGHT_ALIVE)
            track_next(ent);
        break;

    case OMODE_EYECAM:
        ent->movetype = MOVETYPE_NOCLIP;
        ent->solid = SOLID_NOT;
        ent->clipmask = 0;
        ent->svflags |= SVF_NOCLIENT;
        ent->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;

        for (i = 0; i < 3; i++) {
            ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(-ent->client->resp.cmd_angles[i]);
            ent->s.angles[i] = 0;
        }

        VectorCopy(ent->s.angles, ent->client->ps.viewangles);
        VectorCopy(ent->s.angles, ent->client->v_angle);

        if (!ent->client->resp.track_target ||
            ent->client->resp.track_target->client->resp.fightstate != FIGHT_ALIVE)
            track_next(ent);
        break;
    }
}

void move_to_arena(edict_t *ent, int arenanum, int mode)
{
    edict_t     *dest;
    int         i;
    vec3_t      mins = {-16, -16, -24};
    vec3_t      maxs = {16, 16, 32};
    vec3_t      temp, temp2;
    trace_t     tr;

    if (ent->client->resp.isbot) {
        gi.dprintf("\n%s IS A ZBOT %d\n", ent->client->pers.netname, ent->client->resp.isbot);
        gi.centerprintf(ent, "The server seems to think you\nare a bot. If you aren't,\n you may wish to reconnect");
    }

    if (mode) {

        if (!arenas[arenanum].active)
            dest = SelectFarthestArenaSpawnPoint("misc_teleporter_dest", arenanum);
        else
            dest = SelectFarthestArenaSpawnPoint("info_player_deathmatch", arenanum);

        if (arenanum) {
            if (ent->client->resp.context == 0) {
                ent->client->resp.context = arenanum;
                show_observer_menu(ent);
            }
        } else {
            ent->client->resp.track_target = NULL;
            if (ent->client->resp.teamnum != -1)
                show_arena_menu(ent);
        }

        ent->client->resp.context = arenanum;
    } else {
        //get rid of all menus
        ent->client->resp.context = arenanum;
        ClientUserinfoChanged(ent, ent->client->pers.userinfo);

        if (arenas[arenanum].idarena)
            dest = SelectRandomArenaSpawnPoint("info_player_deathmatch", arenanum,
                                               (TEAM(&teams[ent->client->resp.teamnum])->side == arenas[arenanum].sidepick) ? 1 : 2);
        else
            dest = SelectFarthestArenaSpawnPoint("info_player_deathmatch", arenanum);
    }

    if (!dest) {
        gi.bprintf(PRINT_HIGH, "no dest found\n");
        return;
    }

    gi.unlinkentity(ent);

    // try to properly clip to the floor / spawn.  Q2PRO added this to
    // PutClientInServer(); in Rocket Arena the arena spot, not the spawn
    // point, is where the player actually lands, so it belongs here.
    VectorCopy(dest->s.origin, temp);
    VectorCopy(dest->s.origin, temp2);
    temp[2] -= 64;
    temp2[2] += 16;
    tr = gi.trace(temp2, mins, maxs, temp, ent, MASK_PLAYERSOLID);
    if (!tr.allsolid && !tr.startsolid) {
        VectorCopy(tr.endpos, ent->s.origin);
        ent->groundentity = tr.ent;
    } else {
        VectorCopy(dest->s.origin, ent->s.origin);
        ent->s.origin[2] += 10;
    }
    VectorCopy(ent->s.origin, ent->s.old_origin);

    // clear the velocity and hold them in place briefly
    VectorClear(ent->velocity);

    ent->client->ps.pmove.pm_time = 160 >> PM_TIME_SHIFT;   // hold time
    ent->client->ps.pmove.pm_flags |= PMF_TIME_TELEPORT;

    // draw the teleport splash at source and on the player
    if (mode == 0)
        ent->s.event = EV_PLAYER_TELEPORT;

    // set angles
    for (i = 0; i < 3; i++)
        ent->client->ps.pmove.delta_angles[i] =
            ANGLE2SHORT(dest->s.angles[i] - ent->client->resp.cmd_angles[i]);

    VectorClear(ent->s.angles);
    VectorClear(ent->client->ps.viewangles);
    VectorClear(ent->client->v_angle);

    // telefrag avoidance at destination
    if (!KillBox(ent)) {
    }

    if (mode) {
        if (arenas[arenanum].active && ent->client->resp.omode == OMODE_NORMAL)
            ent->client->resp.omode = OMODE_FREEFLYING;

        if (arenas[arenanum].competition && mode != 2)
            ent->client->resp.omode = OMODE_EYECAM;

        SetObserverMode(ent);
    } else {
        ent->client->resp.omode = OMODE_NORMAL;
        SetObserverMode(ent);
    }

    gi.linkentity(ent);

    if (arenas[arenanum].proposetime > level.time && !ent->client->resp.ra_voted) {
        menu_centerprint(ent, va("Settings changes have been proposed\n by %s!\nGoto the observer menu (TAB) to vote",
                                 arenas[arenanum].proposer->client->pers.netname));
        stuffcmd(ent, "play misc/pc_up.wav\n");
    }
}

void ChangeOMode(edict_t *ent)
{
    if (!ent->client->resp.fightstate) {
        if (ent->client->resp.omode != OMODE_TRACKCAM && ent->client->resp.omode != OMODE_EYECAM)
            ent->client->resp.lastomode = ent->client->resp.omode;

        ent->client->resp.omode = (ent->client->resp.omode + 1) % 4;

        gi.cprintf(ent, PRINT_HIGH, "Switched Observer Mode to: %s\n",
                   omode_descriptions[ent->client->resp.omode]);

        move_to_arena(ent, ent->client->resp.context, 1);
    }
}

int getfreeskin(int arenanum)
{
    bool    used[MAX_ARENA_SKINS];
    int         i;
    team_t      *t;

    memset(used, 0, sizeof(used));

    for (i = 0; i < MAX_TEAMS; i++) {
        t = teams[i].it;
        if (!t)
            continue;
        if (t->arenanum != arenanum)
            continue;
        if (t->skin == -1)
            continue;

        used[t->skin] = true;
    }

    for (i = 0; i < MAX_ARENA_SKINS; i++)
        if (!used[i])
            return i;

    return rand() % MAX_ARENA_SKINS;
}

void setteamskin(edict_t *ent, char *userinfo, int skinnum)
{
    char    *val;
    int     pnum;

    pnum = ent - g_edicts - 1;
    val = Info_ValueForKey(userinfo, "skin");

    if (val[0] == 'f') {
        if (strcmp(val, va("female/%s", teamskins[skinnum])))
            gi.configstring(game.csr.playerskins + pnum,
                            va("%s\\female/%s", ent->client->pers.netname, teamskins[skinnum]));

        Info_RemoveKey(userinfo, "skin");
        strcat(userinfo, va("\\skin\\female/%s", teamskins[skinnum]));

        stuffcmd(ent, "skin female/nullxxx\n");
    } else if (val[0] == 'c' && val[1] == 'r') {
        if (strcmp(val, va("crakhor/%s", teamskins[skinnum])))
            gi.configstring(game.csr.playerskins + pnum,
                            va("%s\\crakhor/%s", ent->client->pers.netname, teamskins[skinnum]));

        Info_RemoveKey(userinfo, "skin");
        strcat(userinfo, va("\\skin\\crakhor/%s", teamskins[skinnum]));

        stuffcmd(ent, "skin crakhor/nullxxx\n");
    } else if (val[0] == 'c' && val[1] == 'y') {
        if (strcmp(val, va("cyborg/%s", teamskins[skinnum])))
            gi.configstring(game.csr.playerskins + pnum,
                            va("%s\\cyborg/%s", ent->client->pers.netname, teamskins[skinnum]));

        Info_RemoveKey(userinfo, "skin");
        strcat(userinfo, va("\\skin\\cyborg/%s", teamskins[skinnum]));

        stuffcmd(ent, "skin cyborg/nullxxx\n");
    } else {
        if (strcmp(val, va("male/%s", teamskins[skinnum])))
            gi.configstring(game.csr.playerskins + pnum,
                            va("%s\\male/%s", ent->client->pers.netname, teamskins[skinnum]));

        Info_RemoveKey(userinfo, "skin");
        strcat(userinfo, va("\\skin\\male/%s", teamskins[skinnum]));

        stuffcmd(ent, "skin male/nullxxx\n");
    }
}

void SendTeamToArena(qmenu_t *team, int arenanum, bool observer, bool announce)
{
    qmenu_t     *mnode;
    edict_t     *ent;
    int         statsteam;

    statsteam = -1;
    mnode = team;

    if (!TEAM(team)->outofline) {
        if (arenanum && TEAM(team)->skin == -1
            && (arenas[arenanum].playersperteam > 1 || arenas[arenanum].idarena))
            TEAM(team)->skin = getfreeskin(arenanum);
        else if (!arenanum
                 || (arenas[arenanum].playersperteam == 1 && !arenas[arenanum].idarena))
            TEAM(team)->skin = -1;
    }

    if (!observer && announce && arenas[arenanum].stats) {
        statsteam = team - teams;

        RA2_Stats_AddTeam(arenas[arenanum].stats, statsteam, TEAM(team)->name);
    }

    while (mnode->next) {
        mnode = mnode->next;
        ent = (edict_t *)mnode->it;

        if (TEAM(team)->skin != -1)
            setteamskin(ent, ent->client->pers.userinfo, TEAM(team)->skin);

        if (observer) {
            ent->client->resp.fightstate = FIGHT_SPECTATING;
            ent->takedamage = DAMAGE_NO;
            move_to_arena(ent, arenanum, 1);
        } else {
            ent->client->resp.fightstate = FIGHT_ALIVE;
            ent->takedamage = DAMAGE_NO;
            move_to_arena(ent, arenanum, 0);
            give_ammo(ent);

            if (statsteam != -1)
                RA2_Stats_AddPlayer(arenas[arenanum].stats, ent, statsteam);
        }
    }

    if (announce) {
        if (observer) {
            TEAM(team)->fighting = false;
            add_to_queue(&TEAM(team)->arenalink, &arenas[arenanum].waitingteams);
        } else {
            add_to_queue(&TEAM(team)->arenalink, &arenas[arenanum].activeteams);
        }
    }

    TEAM(team)->arenanum = arenanum;

    gi.dprintf("%d: %d %s entered\n", arenanum, TEAM(team)->teamnum,
               TEAM(team)->name);
}

int AddtoArena(edict_t *ent, int arenanum, int allow_partial, int skip_checks)
{
    int     membercount;

    if (!skip_checks) {
        if (arenas[arenanum].minping && ent->client->ping < arenas[arenanum].minping) {
            menu_centerprint(ent, va("Your ping is too low\nMinimum ping for this arena: %d", arenas[arenanum].minping));
            return 1;
        }

        if (arenas[arenanum].maxping && ent->client->ping > arenas[arenanum].maxping) {
            menu_centerprint(ent, va("Your ping is too high\nMaximum ping for this arena: %d", arenas[arenanum].maxping));
            return 1;
        }

        if (arenas[arenanum].locked) {
            menu_centerprint(ent, "Sorry, that Arena is locked by an admin\n");
            return 1;
        }

        if (arenas[arenanum].idarena) {
            menu_centerprint(ent, "You must join a pickup team to\n enter that arena");
            return 1;
        }

        if (count_queue(&arenas[arenanum].waitingteams) + count_queue(&arenas[arenanum].activeteams) >= arenas[arenanum].maxteams) {
            menu_centerprint(ent, "Sorry, that arena is full");
            return 1;
        }
    }

    membercount = count_queue(&teams[ent->client->resp.teamnum]);

    if (!(membercount != arenas[arenanum].playersperteam
          && (membercount > arenas[arenanum].playersperteam || !allow_partial))) {
        TEAM(&teams[ent->client->resp.teamnum])->outofline = skip_checks;

        if (!skip_checks) {
            remove_from_queue(&TEAM(&teams[ent->client->resp.teamnum])->arenalink, NULL);
            SendTeamToArena(&teams[ent->client->resp.teamnum], arenanum, true, true);
        } else
            SendTeamToArena(&teams[ent->client->resp.teamnum], arenanum, true, false);

        return 0;
    }

    if (count_queue(&teams[ent->client->resp.teamnum]) < arenas[arenanum].playersperteam) {
        show_teamconfirm_menu(ent, arenanum);
        return 1;
    }

    menu_centerprint(ent, va("You have the incorrect number\nof team members, you need %d to play \nin that arena", arenas[arenanum].playersperteam));

    return 1;
}

void check_teams(int arenanum)
{
    qmenu_t     *tnode, *prev_tnode, *mnode;
    int         i;
    int         ping;
    bool    rejected;

    for (i = 0; i < MAX_TEAMS; i++) {
        if (!teams[i].it)
            continue;
        if (count_queue(&teams[i]) != 0)
            continue;

        if (TEAM(&teams[i])->locked)
            continue;

        remove_from_queue(&TEAM(&teams[i])->arenalink, NULL);
        gi.dprintf("Clearing team %d (%s)\n", TEAM(&teams[i])->teamnum,
                   TEAM(&teams[i])->name);
        gi.TagFree(TEAM(&teams[i]));
        teams[i].it = NULL;
    }

    if (!arenanum)
        return;

    tnode = &arenas[arenanum].waitingteams;

    while (tnode->next) {
        tnode = tnode->next;
        mnode = (qmenu_t *)tnode->it;
        rejected = false;

        if (!arenas[arenanum].idarena) {
            while (mnode->next) {
                mnode = mnode->next;

                ping = ((edict_t *)mnode->it)->client->ping;

                if ((ping > arenas[arenanum].maxping && ping < 1000)
                    || ping < arenas[arenanum].minping) {
                    rejected = true;
                    gi.cprintf((edict_t *)mnode->it, PRINT_HIGH,
                               "Sorry, your ping of %d does not work in this arena\n", ping);
                }
            }
        }

        if (count_queue((qmenu_t *)tnode->it) > arenas[arenanum].playersperteam || rejected) {
            gi.bprintf(PRINT_MEDIUM, "Removing team %d (%s)\n",
                       TEAM((qmenu_t *)tnode->it)->teamnum,
                       TEAM((qmenu_t *)tnode->it)->name);

            prev_tnode = tnode->prev;
            remove_from_queue(tnode, NULL);
            SendTeamToArena((qmenu_t *)tnode->it, 0, true, true);
            tnode = prev_tnode;
        }
    }

    if (arenas[arenanum].changed && count_queue(&arenas[arenanum].waitingteams) + count_queue(&arenas[arenanum].activeteams) == 0) {
        set_config(arenanum, arenanum);
        gi.dprintf("%d: Reseting to default config\n", arenanum);
    }
}

void init_player(edict_t *ent)
{
    // A client slot is reused, and the menu queue is TAG_LEVEL memory that
    // belonged to whoever had this slot before.  ra_MenuClose() deliberately
    // does not drop it -- closing a menu is hiding it -- so the one place that
    // must is here, where a client starts from nothing.
    ent->client->curmenulink = NULL;
    ent->client->selected = NULL;
    ent->client->menuqueue.next = NULL;

    ent->client->resp.teammember.it = ent;
    ent->client->resp.fightstate = FIGHT_SPECTATING;
    ent->client->resp.context = 0;
    ent->client->resp.teamnum = -1;

    if (ent->client->pers.showmotd)
        motd_menu(ent);
    else
        menuRefreshTeamList(ent, NULL, NULL, 0);

    ent->client->resp.track_target = NULL;
    ent->client->resp.lastomode = OMODE_FREEFLYING;
    ent->takedamage = DAMAGE_NO;

    send_configstring(ent, game.csr.items + game.num_items + 2, " Red");
    send_configstring(ent, game.csr.items + game.num_items + 3, "Blue");
}

void reinit_player(edict_t *ent)
{
    ent->client->resp.fightstate = FIGHT_SPECTATING;
    ent->client->resp.track_target = NULL;
    ent->client->resp.lastomode = OMODE_FREEFLYING;
}

void show_stringc(char *s, int context)
{
    int     i;
    edict_t *e;

    for (i = 0; i < game.maxclients; i++) {
        e = &g_edicts[i + 1];
        if (e->inuse && e->client && e->client->resp.context == context) {
            gi.centerprintf(e, "%s", s);
        }
    }
}

void show_string(int priority, char *s, int context)
{
    int     i;
    edict_t *e;

    for (i = 0; i < game.maxclients; i++) {
        e = &g_edicts[i + 1];
        if (e->inuse && e->client && e->client->resp.context == context) {
            gi.cprintf(e, priority, "%s", s);
        }
    }
}

void send_sound_to_arena(char *soundname, int context)
{
    int     i;
    edict_t *e;

    for (i = 0; i < game.maxclients; i++) {
        e = &g_edicts[i + 1];
        if (e->inuse && e->client && e->client->resp.context == context) {
            stuffcmd(e, va("play %s\n", soundname));

        }
    }
}

void send_configstring(edict_t *e, int index, char *string)
{
    gi.WriteByte(svc_configstring);
    gi.WriteShort(index);
    gi.WriteString(string);
    gi.unicast(e, true);
}

void show_countdown(int countdown, int arenanum)
{
    int     i;
    edict_t *e;

    for (i = 0; i < game.maxclients; i++) {
        e = &g_edicts[i + 1];
        if (!e->inuse || !e->client)
            continue;
        if (e->client->resp.context != arenanum)
            continue;

        if (arenas[arenanum].state == 0)
            send_configstring(e, game.csr.items + game.num_items, "Waiting for match to start");
        else
            send_configstring(e, game.csr.items + game.num_items, arenas[arenanum].vs);

        if (arenas[arenanum].rounds > 1)
            send_configstring(e, game.csr.items + game.num_items + 1, va("Round %d of %d", arenas[arenanum].round, arenas[arenanum].rounds));
        else
            send_configstring(e, game.csr.items + game.num_items + 1, "");

        G_SetStat(e, SID_RA_ARENASTATUS, game.csr.items + game.num_items);
        G_SetStat(e, SID_RA_ROUNDINFO, game.csr.items + game.num_items + 1);
        G_SetStat(e, SID_RA_COUNTDOWN, countdown);

        if (countdown == 15 || countdown == 10 || countdown == 5) {
            if (e->client->menu_owner != MENU_ARENA)
                SendStatusBar(e, G_Statusbar(), true);
        }

        if (countdown > 0 && countdown < 4 && arenas[arenanum].state)
            stuffcmd(e, va("play ra/%d.wav\n", countdown));
        else if (!countdown && arenas[arenanum].state) {
            stuffcmd(e, "play ra/fight.wav\n");
            gi.centerprintf(e, "FIGHT!");
        }
    }
}

int show_rank(qmenu_t *node)
{
    int     count;

    count = 0;
    for (node = node->prev; node; node = node->prev)
        count++;

    return count;
}

bool check_for_teams(int arenanum)
{
    qmenu_t *tnode;
    int     i;

    if (count_queue(&arenas[arenanum].waitingteams) >= arenas[arenanum].numteams) {
        tnode = &arenas[arenanum].waitingteams;
        i = 0;
        while (tnode->next) {
            if (i >= arenas[arenanum].numteams)
                break;
            tnode = tnode->next;
            i++;
            if (count_queue((qmenu_t *)tnode->it) == 0)
                return false;
        }

        return true;
    }

    return false;
}

int fill_arena(int arenanum)
{
    qmenu_t *popped;
    int     count;
    int     firstskin;
    char    vs[256];

    firstskin = -1;
    vs[0] = 0;

    arenas[arenanum].sidepick = rand() % 2;

    for (count = 0; count < arenas[arenanum].numteams; count++) {
        popped = remove_from_queue(NULL, &arenas[arenanum].waitingteams);

        if (!popped) {
            gi.dprintf("Team left during multi-round match\n");
            return 1;
        }

        if (firstskin == -1)
            firstskin = TEAM((qmenu_t *)popped->it)->skin;
        else if (firstskin == TEAM((qmenu_t *)popped->it)->skin) {
            gi.dprintf("Skin conflict in arena %d\n", arenanum);
            TEAM((qmenu_t *)popped->it)->skin = (firstskin + 1) % MAX_ARENA_SKINS;
        }

        SendTeamToArena((qmenu_t *)popped->it, arenanum, false, true);

        if (count)
            strcat(vs, " vs ");
        strcat(vs, TEAM((qmenu_t *)popped->it)->name);

        if (arenas[arenanum].round == 1)
            TEAM((qmenu_t *)popped->it)->wins = 0;

        TEAM((qmenu_t *)popped->it)->fighting = true;
    }

    Q_strlcpy(arenas[arenanum].vs, vs, sizeof(arenas[arenanum].vs));
    gi.dprintf("%d: %s\n", arenanum, arenas[arenanum].vs);

    return 1;
}

int fight_done(int arenanum)
{
    qmenu_t     *tnode, *mnode;
    edict_t     *e;
    int         winner;

    winner = -1;

    tnode = &arenas[arenanum].activeteams;

    while (tnode->next) {
        tnode = tnode->next;

        mnode = (qmenu_t *)tnode->it;

        while (mnode->next) {
            mnode = mnode->next;
            e = (edict_t *)mnode->it;

            if (e->takedamage != DAMAGE_AIM || e->deadflag != DEAD_NO)
                continue;

            if (winner == -1)
                winner = e->client->resp.teamnum;
            else if (winner != e->client->resp.teamnum)
                return -2;
        }
    }

    return winner;
}

static void loc_buildboxpoints(vec3_t p[8], vec3_t org, vec3_t mins, vec3_t maxs)
{
    VectorAdd(org, mins, p[0]);
    VectorCopy(p[0], p[1]);
    p[1][0] -= mins[0];
    VectorCopy(p[0], p[2]);
    p[2][1] -= mins[1];
    VectorCopy(p[0], p[3]);
    p[3][0] -= mins[0];
    p[3][1] -= mins[1];
    VectorAdd(org, maxs, p[4]);
    VectorCopy(p[4], p[5]);
    p[5][0] -= maxs[0];
    VectorCopy(p[0], p[6]);
    p[6][1] -= maxs[1];
    VectorCopy(p[0], p[7]);
    p[7][0] -= maxs[0];
    p[7][1] -= maxs[1];
}

static q_unused bool loc_CanSee(edict_t *targ, edict_t *inflictor)
{
    trace_t trace;
    vec3_t  targpoints[8];
    int     i;
    vec3_t  viewpoint;

// bmodels need special checking because their origin is 0,0,0
    if (targ->movetype == MOVETYPE_PUSH)
        return false;       // bmodels not supported

    loc_buildboxpoints(targpoints, targ->s.origin, targ->mins, targ->maxs);

    VectorCopy(inflictor->s.origin, viewpoint);
    viewpoint[2] += inflictor->viewheight;

    for (i = 0; i < 8; i++) {
        trace = gi.trace(viewpoint, vec3_origin, vec3_origin, targpoints[i], inflictor, MASK_SOLID);
        if (trace.fraction == 1.0f)
            return true;
    }

    return false;
}

void CTFSetIDView(edict_t *ent)
{
    vec3_t      forward;
    trace_t     tr;

    G_SetStat(ent, SID_RA_ID_VIEW, 0);

    if (ent->client->resp.fightstate)
        return;

    if (ent->client->resp.track_target) {
        G_SetStat(ent, SID_RA_ID_VIEW,
                  game.csr.playerskins + (ent->client->resp.track_target - g_edicts) - 1);
        return;
    }

    AngleVectors(ent->client->v_angle, forward, NULL, NULL);
    VectorScale(forward, 1024, forward);
    VectorAdd(ent->s.origin, forward, forward);

    tr = gi.trace(ent->s.origin, NULL, NULL, forward, ent, CONTENTS_SOLID | CONTENTS_MONSTER);

    if (tr.fraction < 1 && tr.ent && tr.ent->client && tr.ent->solid)
        G_SetStat(ent, SID_RA_ID_VIEW,
                  game.csr.playerskins + (tr.ent - g_edicts) - 1);
}

void UpdateStatusBars(int arenanum)
{
    qmenu_t *tnode, *tslot, *mnode;
    edict_t *e;
    int     numteams;
    int     ti, i, y, n;
    int     membercount[MAX_STATUS_TEAMS];
    char    *teamname[MAX_STATUS_TEAMS];
    char    *names[MAX_STATUS_TEAMS][MAX_STATUS_MEMBERS];
    int     health[MAX_STATUS_TEAMS][MAX_STATUS_MEMBERS];
    char    *p;
    char    string[1400];

    tnode = &arenas[arenanum].activeteams;
    numteams = -1;
    while (tnode->next) {
        if (numteams >= MAX_STATUS_TEAMS)
            break;
        numteams++;
        tnode = tnode->next;

        tslot = (qmenu_t *)tnode->it;
        teamname[numteams] = ((team_t *)tslot->it)->name;
        membercount[numteams] = -1;

        for (mnode = tslot->next; mnode; mnode = mnode->next) {
            if (membercount[numteams] >= MAX_STATUS_MEMBERS - 1)
                break;

            e = (edict_t *)mnode->it;
            if (e->takedamage != DAMAGE_AIM)
                continue;
            if (e->deadflag != DEAD_NO)
                continue;

            membercount[numteams]++;
            names[numteams][membercount[numteams]] = e->client->pers.netname;
            health[numteams][membercount[numteams]] = e->health;
        }
    }

    y = 40;

    strcpy(string, "xl 8 yb -10 string2 \"Line Position:\" xl 100 yb -24 num 2 19 ");

    p = string + strlen(string);
    if (!arenas[arenanum].competition) {
        for (ti = 0; ti <= numteams; ti++) {
            sprintf(p, "xl %d yt %d string2 \"%s\" ", 8, y, teamname[ti]);
            p = string + strlen(string);
            y += 8;

            for (i = 0; i <= membercount[ti]; i++) {
                sprintf(p, "xl %d yt %d string2 \"%s: %d\" ", 8, y,
                        names[ti][i], health[ti][i]);
                p = string + strlen(string);
                y += 8;
            }

            y += 8;
        }
    }

    strcpy(p, "if 20 xv 0 yb -58 stat_string 20 endif ");

    for (n = 0; n < game.maxclients; n++) {
        e = &g_edicts[n + 1];

        if (!e->inuse || !e->client)
            continue;
        if (e->client->resp.context != arenanum)
            continue;
        if (e->client->resp.fightstate != FIGHT_SPECTATING)
            continue;
        if (e->client->menu_owner == MENU_ARENA)
            continue;

        if (e->client->resp.track_target || e->client->scoremode) {
            SendStatusBar(e, G_Statusbar(), true);
        } else {
            G_SetStat(e, SID_RA_LINEPOSITION,
                      show_rank(&((team_t *)teams[e->client->resp.teamnum].it)->arenalink));
            SendStatusBar(e, string, true);
        }
    }
}

void check_telefrag(int arenanum)
{
    int         i;
    edict_t     *e;
    trace_t     tr;
    vec3_t      angles;
    vec3_t      forward;

    for (i = 0; i < game.maxclients; i++) {
        e = &g_edicts[i + 1];

        if (!e->inuse)
            continue;
        if (!e->client)
            continue;
        if (e->client->resp.context != arenanum)
            continue;
        if (!e->client->resp.fightstate)
            continue;
        if (!e->client->resp.spawn_recheck)
            continue;
        if (e->client->resp.spawn_recheck > level.framenum)
            continue;

        tr = gi.trace(e->s.origin, e->mins, e->maxs, e->s.origin, NULL, MASK_PLAYERSOLID);

        if (tr.contents == CONTENTS_SOLID) {
            e->solid = SOLID_NOT;

            angles[1] = rand() % 360;
            angles[0] = 0;
            angles[2] = 0;
            AngleVectors(angles, forward, NULL, NULL);
            VectorScale(forward, 600, forward);
            VectorAdd(e->velocity, forward, e->velocity);

            e->client->resp.spawn_recheck = level.framenum + 0.5f / FRAMETIME;
        } else {
            e->solid = SOLID_BBOX;

            gi.unlinkentity(e);
            KillBox(e);
            gi.linkentity(e);
        }
    }
}

void start_voting(edict_t *proposer, int arenanum)
{
    int         i;
    edict_t     *cl_ent;

    if (arenas[arenanum].state == ASTATE_FIGHTING || arenas[arenanum].state == ASTATE_COUNTDOWN)
        arenas[arenanum].proposetime = level.time + 30000;
    else
        arenas[arenanum].proposetime = level.time + 30;

    arenas[arenanum].votetries = arenas[arenanum].votes_no = arenas[arenanum].votes_yes = 0;
    arenas[arenanum].proposer = proposer;

    for (i = 0; i < game.maxclients; i++) {
        cl_ent = &g_edicts[i + 1];
        if (!cl_ent->inuse)
            continue;

        if (!cl_ent->client)
            continue;

        if (cl_ent->client->resp.context != arenanum)
            continue;

        cl_ent->client->resp.ra_voted = false;
        arenas[arenanum].votetries++;

        if (cl_ent->client->resp.fightstate != FIGHT_SPECTATING)
            continue;

        if (cl_ent != proposer) {
            menu_centerprint(cl_ent, va("Settings changes have been proposed\nby %s!\nGoto the observer menu (TAB) to vote",
                                        arenas[arenanum].proposer->client->pers.netname));
            stuffcmd(cl_ent, "play misc/pc_up.wav\n");
        } else {
            stuffcmd(cl_ent, "play misc/pc_up.wav\n");
        }
    }

    gi.dprintf("Starting Voting in Arena %d with %d voters\n", arenanum, arenas[arenanum].votetries);
}

void check_voting(int arenanum)
{
    int         i;
    edict_t     *cl_ent;
    char        msg[80];

    if (!arenas[arenanum].proposetime)
        return;

    if (arenas[arenanum].proposetime > level.time)
        return;

    arenas[arenanum].proposetime = 0;

    if (arenas[arenanum].votes_yes - arenas[arenanum].votes_no >= (float)arenas[arenanum].votetries * (1.0f / 3.0f)) {
        memcpy(&arenas[arenanum].playersperteam, &arenas[arenanum].proposed, sizeof(arena_settings_t));
        arenas[arenanum].changed = true;

        sprintf(msg, "Changes Passed! Yes votes: %d No votes: %d\n",
                arenas[arenanum].votes_yes, arenas[arenanum].votes_no);
    } else {
        sprintf(msg, "Changes Failed! Yes votes: %d No votes: %d\n",
                arenas[arenanum].votes_yes, arenas[arenanum].votes_no);
    }

    for (i = 0; i < game.maxclients; i++) {
        cl_ent = &g_edicts[i + 1];
        if (!cl_ent->inuse)
            continue;

        if (!cl_ent->client)
            continue;

        if (cl_ent->client->resp.context != arenanum)
            continue;

        gi.cprintf(cl_ent, PRINT_CHAT, "%s", msg);

        if (arenas[arenanum].changed)
            cl_ent->client->resp.ra_votes = votetries_setting;
    }

    gi.dprintf("%s", msg);

    check_teams(arenanum);
}

void arena_think(int arenanum)
{
    int         winner;
    int         morewins;
    qmenu_t     *tnode, *popped;
    arena_t     *arena;

    arena = &arenas[arenanum];

    check_teams(arenanum);
    check_voting(arenanum);
    check_telefrag(arenanum);

    if (arena->state == ASTATE_COUNTDOWN || arena->state == ASTATE_WARMUP) {
        if (arena->countdown_next_tick == 0) {
            arena->countdown_next_tick = level.framenum + 1 / FRAMETIME;

            if (arena->state == ASTATE_WARMUP)
                arena->countdown = 15;
            else if (arena->proposetime > level.time)
                arena->countdown = 10;
            else
                arena->countdown = 5;

            show_countdown(arena->countdown, arenanum);
            return;
        }

        if (arena->countdown_next_tick >= level.framenum)
            return;

        show_countdown(--arena->countdown, arenanum);

        if (arena->countdown == 0) {
            arena->countdown_next_tick = 0;

            if (arena->state == ASTATE_WARMUP) {
                if (!check_for_teams(arenanum)) {
                    arena->state = ASTATE_ROUNDEND;
                    show_stringc("Not enough teams to start", arenanum);
                    return;
                }

                if (arena->idarena == 1) {
                    RA2_Stats_End(arena->stats);
                    arena->stats = RA2_Stats_Begin(arenanum);
                }

                arena->state = ASTATE_COUNTDOWN;
                fill_arena(arenanum);
                return;
            }

            arena->state = ASTATE_FIGHTING;
            set_damage(arenanum, DAMAGE_AIM);
            return;
        }

        arena->countdown_next_tick = level.framenum + 1 / FRAMETIME;
        return;
    } else if (arena->state == ASTATE_FIGHTING && !broken) {
        UpdateStatusBars(arenanum);

        if (fight_done(arenanum) <= -2)
            return;

        arena->state = ASTATE_RESULTS;
        return;
    } else if (arena->state == ASTATE_ROUNDEND) {
        if (!check_for_teams(arenanum))
            return;

        arena->round = 1;

        if (arenas[arenanum].idarena) {
            arena->state = ASTATE_WARMUP;
            return;
        }

        if (arena->idarena == 1) {
            RA2_Stats_End(arena->stats);
            arena->stats = RA2_Stats_Begin(arenanum);
        }

        arena->state = ASTATE_COUNTDOWN;
        fill_arena(arenanum);
        return;
    } else if (arena->state == ASTATE_RESULTS) {
        UpdateStatusBars(arenanum);

        if (arena->countdown_next_tick == 0) {
            arena->countdown_next_tick = level.framenum + 3 / FRAMETIME;
            return;
        }

        if (arena->countdown_next_tick >= level.framenum)
            return;

        arena->state = ASTATE_NEXTROUND;
        arena->countdown_next_tick = 0;

        if (arena->proposetime - level.time <= 30)
            return;

        arena->proposetime = level.time + 30;
        return;
    } else if (arena->state == ASTATE_NEXTROUND) {
        winner = fight_done(arenanum);

        if (winner == -1)
            sprintf(arena->msg, "It was a tie!");
        else {
            RA2_Stats_TeamScore(arena->stats, winner, 1);

            if (++((team_t *)teams[winner].it)->wins > arenas[arenanum].rounds / 2) {
                Q_snprintf(arena->msg, sizeof(arena->msg), "%s has won the match!!",
                           ((team_t *)teams[winner].it)->name);
                arena->round = arena->rounds;

                RA2_Stats_End(arena->stats);
                arena->stats = NULL;
            } else
                Q_snprintf(arena->msg, sizeof(arena->msg), "%s has won the round!",
                           ((team_t *)teams[winner].it)->name);
        }

        RA2_Stats_Write(arena->stats);

        if (winner == -1) {
            if (count_queue(&arenas[arenanum].activeteams) != 0) {
                if (count_queue((qmenu_t *)arenas[arenanum].activeteams.next->it) != 0)
                    arena->round--;
            }
        }

        gi.dprintf("%d: %d %s\n", arenanum, winner, arena->msg);
        set_damage(arenanum, DAMAGE_NO);
        show_stringc(arena->msg, arenanum);
        tnode = &arenas[arenanum].activeteams;
        arenas[arenanum].sidepick = rand() % 2;

        while (tnode->next) {
            tnode = tnode->next;

            if (arena->round < arenas[arenanum].rounds) {
                morewins = arenas[arenanum].rounds / 2 + 1 - ((team_t *)((qmenu_t *)tnode->it)->it)->wins;
                Q_snprintf(arena->msg, sizeof(arena->msg), "%s has %d wins and needs %d more to take the match\n", ((team_t *)((qmenu_t *)tnode->it)->it)->name, ((team_t *)((qmenu_t *)tnode->it)->it)->wins, morewins);
                show_string(2, arena->msg, arenanum);
                SendTeamToArena((qmenu_t *)tnode->it, arenanum, false, false);
            } else {
                popped = remove_from_queue(NULL, &arenas[arenanum].activeteams);
                ((team_t *)((qmenu_t *)popped->it)->it)->fighting = false;
                tnode = &arenas[arenanum].activeteams;

                if (((team_t *)((qmenu_t *)popped->it)->it)->teamnum == winner || winner == -1)
                    add_to_front_queue(popped, &arenas[arenanum].waitingteams);
                else
                    add_to_queue(popped, &arenas[arenanum].waitingteams);
            }
        }

        if (arena->round < arenas[arenanum].rounds) {
            RA2_Stats_NextRound(arena->stats);

            arena->round++;
            arena->state = ASTATE_COUNTDOWN;
            return;
        }

        arena->state = ASTATE_ROUNDEND;
        return;
    }
}

void multi_arena_think(void)
{
    int     i;

    if (level.intermission_framenum)
        return;

    i = level.framenum % (num_arenas * 2);
    if (i % 2)
        return;

    arena_think(i / 2 + 1);
}

void arena_init(edict_t *wsent)
{
    int     i;
    team_t  *t;
    char    *name;

    if (!wsent)
        return;

    teams = gi.TagMalloc(MAX_TEAMS * sizeof(qmenu_t), TAG_LEVEL);
    memset(teams, 0, MAX_TEAMS * sizeof(qmenu_t));
    memset(arenas, 0, sizeof(arenas));

    admincode = gi.cvar("admincode", "0", 0);

    num_arenas = wsent->arena;  //worldspawn arena flag is # of arenas
    if (!num_arenas) {
        num_arenas = 1;
        idmap = true;
    } else idmap = false;

    load_config(num_arenas + 1);
    set_config(1, num_arenas);

    for (i = 0; i <= num_arenas; i++) {
        arenas[i].state = ASTATE_ROUNDEND;
        arenas[i].stats = NULL;
        arenas[i].active = idmap;
        arenas[i].numteams = 2;
        arenas[i].waitingteams.prev = NULL;
        arenas[i].activeteams.prev = NULL;
        arenas[i].waitingteams.next = NULL;
        arenas[i].activeteams.next = NULL;
        arenas[i].countdown_next_tick = 0;
        arenas[i].countdown = 0;
        arenas[i].proposetime = 0;
        arenas[i].round = 0;

        if (!SelectFarthestArenaSpawnPoint("misc_teleporter_dest", i)) {
            gi.dprintf("Setting arena %d to idarena mode\n", i);
            arenas[i].active = true;
        }

        if (i && arenas[i].idarena) {
            name = gi.TagMalloc(100, TAG_LEVEL);
            sprintf(name, "#%d Pickup Red", i);
            t = add_to_team(NULL, name);
            t->side = 0;
            SendTeamToArena(t->arenalink.it, i, true, true);
            arenas[i].pickupteam[0] = t;

            name = gi.TagMalloc(100, TAG_LEVEL);
            sprintf(name, "#%d Pickup Blue", i);
            t = add_to_team(NULL, name);
            t->side = 1;
            SendTeamToArena(t->arenalink.it, i, true, true);
            arenas[i].pickupteam[1] = t;

            arenas[i].maxteams = 2;
            arenas[i].playersperteam = 128;
        }
    }

    load_motd();
}

// Prefixed per sec 7 rule 4 and reached from g_misc.c's per-ruleset dispatcher,
// which already chose between Threewave's and Ground Zero's (R-44).
void ra_SP_trigger_teleport(edict_t *ent)
{
    ent->touch = teleporter_touch;
    ent->movetype = MOVETYPE_NONE;
    ent->svflags |= SVF_NOCLIENT;
    ent->solid = SOLID_TRIGGER;
    ent->use = NULL;
    gi.setmodel(ent, ent->model);
    gi.linkentity(ent);
}

void SP_func_illusionary(edict_t *ent)
{
    ent->movetype = MOVETYPE_NONE;
    ent->solid = SOLID_NOT;
    gi.setmodel(ent, ent->model);
    gi.linkentity(ent);
}

// RA2's SP_info_teleport_destination was here and was an EMPTY body -- the
// classname has to exist so ED_CallSpawn does not reject the entity, and RA2
// needs nothing else from it.  g_misc.c already owns that classname and
// dispatches it per ruleset (R-44), so an empty stub here is a link collision
// rather than a feature; the arena arm is a row in that dispatcher.

// RA2's grapple was here -- 280 lines, CTFPlayerResetGrapple through
// CTFWeapon_Grapple, function for function the same set Threewave ships.  That
// is the measurement R-50 made and sec 7 rule 6's resolution: ONE grapple, and
// it is src/ctf/g_ctf.c's.  RA2's is a fork of it, not a rival design, so this
// is a deletion rather than a choice between two implementations.  What RA2
// tunes on top of it -- `allow_grapple` -- is a parameter on that code.


/*
==================
RA_Obituary

RA2's side of a death: the round statistics, the score adjustment when the arena
is not scoring by damage, and the announcer.  The obituary TEXT stays baseq2's
(merged with Threewave's and both mission packs'); RA2's copy of it is a stale
fork of id's with sounds bolted on, which sec 7 rule 1 decides -- so this is the
part that is RA2's feature and nothing else.

The announcer grades the kill by how much health the winner had left, which is
why it needs the arena's configured starting health rather than a constant.
==================
*/
void RA_Obituary(edict_t *self, edict_t *inflictor, edict_t *attacker)
{
    int ctx = self->client->resp.context;

    RA2_Stats_Add(arenas[ctx].stats, self - g_edicts, RA2_STAT_DEATHS, 1);

    if (!attacker || !attacker->client || attacker == self) {
        // A suicide costs a frag unless the arena scores by damage, in which
        // case the score is not a frag count at all.
        if (!arenas[ctx].scorebydamage) {
            self->client->resp.score--;
            RA2_Stats_Add(arenas[ctx].stats, self - g_edicts, RA2_STAT_SUICIDES, 1);
            RA2_Stats_Add(arenas[ctx].stats, self - g_edicts, RA2_STAT_SCORE, -1);
        }
        return;
    }

    if (attacker->client->resp.isbot)
        gi.dprintf("%s is flagged as a ZBot (%d)\n",
                   attacker->client->pers.netname, attacker->client->resp.isbot);

    ctx = attacker->client->resp.context;

    if (attacker->health >= arenas[ctx].health) {
        if (self->health < -40)
            send_sound_to_arena("ra/fatality.wav", ctx);
        else
            send_sound_to_arena("ra/flawless.wav", ctx);
    } else if (attacker->health >= arenas[ctx].health - 20) {
        send_sound_to_arena("ra/excelent.wav", ctx);
    }
}

/*
==================
RA_ZBotSample

RA2's aim-cheat detector, sampled once per client command.  A ZBot snaps the
view to a target and back, which shows up as this frame's angles matching two
frames ago while last frame's did not; ten of those inside five seconds marks
the client.  Impulses 161..179 are the cooperating-bot handshake, which marks it
honestly instead.

Kept as the donor has it and NOT acted on here -- `resp.isbot` is only read by
RA2's own reporting.  Whether a heuristic like this should be enabled by default
on a public server is R-SEC's question in Phase 8, not this merge's.
==================
*/
void RA_ZBotSample(edict_t *ent, usercmd_t *ucmd)
{
    gclient_t *cl = ent->client;

    if (ucmd->impulse > 160 && ucmd->impulse < 180)
        cl->resp.isbot = 2;

    if (cl->resp.isbot)
        return;

    if (level.time - cl->resp.zbotlastcheck > 5.0f)
        cl->resp.zbotcount = 0;

    if (cl->oldangles[0][0] == ucmd->angles[0] && cl->oldangles[1][0] != ucmd->angles[0] &&
        cl->oldangles[0][1] == ucmd->angles[1] && cl->oldangles[1][1] != ucmd->angles[1]) {
        cl->resp.zbotlastcheck = level.time;
        cl->resp.zbotcount++;
    }

    cl->oldangles[0][0] = cl->oldangles[1][0];
    cl->oldangles[1][0] = ucmd->angles[0];
    cl->oldangles[0][1] = cl->oldangles[1][1];
    cl->oldangles[1][1] = ucmd->angles[1];

    if (cl->resp.zbotcount > 10)
        cl->resp.isbot = 3;
}

// ---------------------------------------------------------------- hud
//
// The two things RA2 writes into the shared HUD.  They are here rather than in
// p_hud.c so that the arena-only state -- the team skin table, the pickup
// queues -- stays on this side of the seam and p_hud.c asks by name.

/*
==================
RA_SkinIcon

R-OSP-7's table records slot 0 as "also STAT_SKIN_ICON": RA2 draws the player's
team skin where baseq2 draws the health icon.  Same slot, same `pic 0` in the
bar, different writer -- so this is a value, not a second slot.  Falls back to
the health icon for a skin that is not one of the seven team skins.
==================
*/
int RA_SkinIcon(edict_t *ent)
{
    char    skinicon[MAX_QPATH];
    int     image, i;

    Q_snprintf(skinicon, sizeof(skinicon), "%s_i",
               Info_ValueForKey(ent->client->pers.userinfo, "skin"));
    image = gi.imageindex(skinicon);

    for (i = 0; i < MAX_ARENA_SKINS; i++) {
        if (teamskins_precachem[i] == image || teamskins_precachef[i] == image ||
            teamskins_precachecw[i] == image || teamskins_precachecb[i] == image)
            return image;
    }

    return level.pic_health;
}

/*
==================
RA_SetQueueStats

The pickup-team queue panel: how many are waiting on each side, and the two
configstrings naming them.  A client with no arena gets the panel switched off
rather than left holding the last arena's numbers.
==================
*/
void RA_SetQueueStats(edict_t *ent)
{
    int ctx = ent->client->resp.context;

    if (!ctx) {
        G_SetStat(ent, SID_RA_COUNTDOWN, 0);
        G_SetStat(ent, SID_RA_ARENASTATUS, 0);
        G_SetStat(ent, SID_RA_SHOWQUEUE, 0);
        return;
    }

    if (!arenas[ctx].idarena) {
        G_SetStat(ent, SID_RA_SHOWQUEUE, 0);
        return;
    }

    // Mid-round the queue is what is left to play; between rounds it is
    // everyone signed up.
    if (arenas[ctx].state == ASTATE_FIGHTING ||
        arenas[ctx].state == ASTATE_RESULTS ||
        arenas[ctx].state == ASTATE_NEXTROUND) {
        G_SetStat(ent, SID_RA_QUEUE1,
                  count_players_queue(arenas[ctx].pickupteam[0]->arenalink.it));
        G_SetStat(ent, SID_RA_QUEUE2,
                  count_players_queue(arenas[ctx].pickupteam[1]->arenalink.it));
    } else {
        G_SetStat(ent, SID_RA_QUEUE1,
                  count_queue(arenas[ctx].pickupteam[0]->arenalink.it));
        G_SetStat(ent, SID_RA_QUEUE2,
                  count_queue(arenas[ctx].pickupteam[1]->arenalink.it));
    }

    G_SetStat(ent, SID_RA_QUEUE1_ICON, game.csr.items + game.num_items + 2);
    G_SetStat(ent, SID_RA_QUEUE2_ICON, game.csr.items + game.num_items + 3);
    G_SetStat(ent, SID_RA_SHOWQUEUE, 1);
}

// ---------------------------------------------------------------- scoreboard
//
// RA2's three scoreboard layouts and the chooser that picks between them.  They
// live here rather than in p_hud.c for the reason Threewave's CTFScoreboard
// does: a ruleset's scoreboard is that ruleset's, and the dispatch already has
// a ScoreboardMessage row for exactly this (R-MODE-5).  RA2 wrote them in its
// own p_hud.c, where they would have had to be gated three times over.

/*
==================
Serverwide_ScoreboardMessage

==================
*/
void Serverwide_ScoreboardMessage(edict_t *ent)
{
    char    entry[1024];
    char    string[1400];
    int     stringlength;
    int     i, j, k;
    int     sorted[MAX_CLIENTS];
    int     sortedscores[MAX_CLIENTS];
    int     score, total;
    int     y;
    char    teamname[100];
    char    line[1024];
    gclient_t   *cl;
    edict_t     *cl_ent;

    total = 0;
    for (i = 0; i < game.maxclients; i++) {
        cl_ent = g_edicts + 1 + i;
        if (!cl_ent->inuse)
            continue;

        score = game.clients[i].resp.score;

        for (j = 0; j < total; j++)
            if (score > sortedscores[j])
                break;

        for (k = total; k > j; k--) {
            sorted[k] = sorted[k - 1];
            sortedscores[k] = sortedscores[k - 1];
        }

        sorted[j] = i;
        sortedscores[j] = score;
        total++;
    }

    string[0] = 0;
    stringlength = strlen(string);

    Q_snprintf(entry, sizeof(entry),
               "xv 0 yv 32 string2 \"Frags Ping   Name        Team       A\" xv 0 yv 40 string2 \""
               "\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b"
               "\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\" ");
    j = strlen(entry);
    if (stringlength + j < 1024) {
        strcpy(string + stringlength, entry);
        stringlength += j;
    }

    if (total > 23)
        total = 23;

    for (i = 0; i < total; i++) {
        cl = &game.clients[sorted[i]];
        cl_ent = g_edicts + 1 + sorted[i];

        if (cl->resp.teamnum > -1)
            Q_strlcpy(teamname, ((team_t *)teams[cl->resp.teamnum].it)->name,
                      sizeof(teamname));
        else
            Q_strlcpy(teamname, "None", sizeof(teamname));

        Q_snprintf(line, sizeof(line), "%3i %4i %12.12s %12.12s %1i",
                   cl->resp.score, cl->ping, cl->pers.netname, teamname, cl->resp.context);

        if (cl_ent == ent)
            HiPrint(line);

        y = i * 8 + 48;

        Q_snprintf(entry, sizeof(entry), "xv 8 yv %i string2 \"%s\"", y, line);

        j = strlen(entry);
        if (stringlength + j > 1024)
            break;
        strcpy(string + stringlength, entry);
        stringlength += j;
    }

    gi.WriteByte(svc_layout);
    gi.WriteString(string);
}

/*
==================
Arena_ScoreboardMessage

==================
*/
void Arena_ScoreboardMessage(edict_t *ent)
{
    char    entry[1024];
    char    string[1400];
    int     sortedteams[MAX_TEAMS];
    int     teamscores[MAX_TEAMS];
    int     sortedplayers[MAX_CLIENTS];
    int     playerscores[MAX_CLIENTS];
    int     teampings[MAX_TEAMS];
    char    line[1024];
    int     stringlength;
    int     i, j, k;
    int     score;
    int     total;
    edict_t *cl_ent;
    int     totalplayers;
    int     arenanum;
    int     ping;
    int     row;
    int     n;
    qmenu_t *node;
    team_t  *t;
    gclient_t   *cl;

    arenanum = ent->client->resp.context;

    for (total = 0, i = 0; i < MAX_TEAMS; i++) {
        if (!teams[i].it)
            continue;
        if (((team_t *)teams[i].it)->arenanum != arenanum)
            continue;
        if (((team_t *)teams[i].it)->outofline)
            continue;

        node = &teams[i];
        score = 0;
        k = 0;
        ping = 0;
        while (node->next) {
            node = node->next;
            score += ((edict_t *)node->it)->client->resp.score;
            ping += ((edict_t *)node->it)->client->ping;
            k++;
        }

        if (!k)
            continue;

        ping /= k;

        for (j = 0; j < total; j++)
            if (score > teamscores[j])
                break;

        for (k = total; k > j; k--) {
            sortedteams[k] = sortedteams[k - 1];
            teamscores[k] = teamscores[k - 1];
            teampings[k] = teampings[k - 1];
        }

        sortedteams[j] = i;
        teamscores[j] = score;
        teampings[j] = ping;
        total++;
    }

    string[0] = 0;
    stringlength = strlen(string);

    Q_snprintf(entry, sizeof(entry), "xv 0 yv 40 string2 \"Teams\" xv 160 string2 \"Players\" ");
    j = strlen(entry);
    strcpy(string + stringlength, entry);
    stringlength += j;

    row = 1;
    total = total > 20 ? 20 : total;

    for (n = 0; n < total; n++) {
        t = teams[sortedteams[n]].it;

        Q_snprintf(line, sizeof(line), "%-2d %-3d %.11s", teamscores[n], teampings[n], t->name);
        if (t->fighting)
            HiPrint(line);

        Q_snprintf(entry, sizeof(entry), "xv 0 yv %d string2 \"%s\" ", row * 8 + 40, line);
        j = strlen(entry);
        if (stringlength + j > 1024)
            break;
        strcpy(string + stringlength, entry);
        stringlength += j;

        totalplayers = 0;
        node = t->arenalink.it;

        while (node->next) {
            node = node->next;
            cl_ent = node->it;
            score = cl_ent->client->resp.score;

            for (j = 0; j < totalplayers; j++)
                if (score > playerscores[j])
                    break;

            for (k = totalplayers; k > j; k--) {
                sortedplayers[k] = sortedplayers[k - 1];
                playerscores[k] = playerscores[k - 1];
            }

            sortedplayers[j] = cl_ent - g_edicts - 1;
            playerscores[j] = score;
            totalplayers++;
        }

        totalplayers = totalplayers > 20 ? 20 : totalplayers;

        for (i = 0; i < totalplayers; i++) {
            cl_ent = g_edicts + 1 + sortedplayers[i];
            cl = &game.clients[sortedplayers[i]];

            Q_snprintf(line, sizeof(line), "%-2d %-3d %.11s",
                       cl->resp.score, cl->ping, cl->pers.netname);
            if (cl_ent->takedamage)
                HiPrint(line);

            Q_snprintf(entry, sizeof(entry), "xv 160 yv %d string2 \"%s\" ", row * 8 + 40, line);
            j = strlen(entry);
            if (stringlength + j > 1024)
                break;
            strcpy(string + stringlength, entry);
            stringlength += j;
            row++;
        }
    }

    gi.WriteByte(svc_layout);
    gi.WriteString(string);
}

/*
==================
Pickup_ScoreboardMessage

==================
*/
void Pickup_ScoreboardMessage(edict_t *ent)
{
    char    entry[1024];
    char    string[1400];
    int     redsorted[MAX_CLIENTS];
    int     redscores[MAX_CLIENTS];
    int     bluesorted[MAX_CLIENTS];
    int     bluescores[MAX_CLIENTS];
    char    line[1024];
    int     stringlength;
    int     i, j, k;
    int     score;
    int     redtotal;
    edict_t     *cl_ent;
    int     bluewins;
    int     redwins;
    int     bluetotal;
    gclient_t   *cl;

    bluewins = 0;
    redwins = 0;
    redtotal = 0;

    for (i = 0; i < game.maxclients; i++) {
        cl_ent = &g_edicts[i + 1];
        if (!cl_ent->inuse)
            continue;
        if (cl_ent->client->resp.context != ent->client->resp.context)
            continue;
        if (((team_t *)teams[cl_ent->client->resp.teamnum].it)->side)
            continue;

        score = game.clients[i].resp.score;

        for (j = 0; j < redtotal; j++)
            if (score > redscores[j])
                break;

        for (k = redtotal; k > j; k--) {
            redsorted[k] = redsorted[k - 1];
            redscores[k] = redscores[k - 1];
        }

        redsorted[j] = i;
        redscores[j] = score;
        redwins = ((team_t *)teams[cl_ent->client->resp.teamnum].it)->wins;
        redtotal++;
    }

    bluetotal = 0;

    for (i = 0; i < game.maxclients; i++) {
        cl_ent = &g_edicts[i + 1];
        if (!cl_ent->inuse)
            continue;
        if (cl_ent->client->resp.context != ent->client->resp.context)
            continue;
        if (((team_t *)teams[cl_ent->client->resp.teamnum].it)->side != 1)
            continue;

        score = game.clients[i].resp.score;

        for (j = 0; j < bluetotal; j++)
            if (score > bluescores[j])
                break;

        for (k = bluetotal; k > j; k--) {
            bluesorted[k] = bluesorted[k - 1];
            bluescores[k] = bluescores[k - 1];
        }

        bluesorted[j] = i;
        bluescores[j] = score;
        bluewins = ((team_t *)teams[cl_ent->client->resp.teamnum].it)->wins;
        bluetotal++;
    }

    string[0] = 0;
    stringlength = strlen(string);

    if (redwins < 0)
        redwins = 0;
    if (bluewins < 0)
        bluewins = 0;

    Q_snprintf(entry, sizeof(entry),
               "xv 0 yv 40 string2 \"Team Red  : %d\" xv 160 yv 40 string2 \"Team Blue : %d\" ",
               redwins, bluewins);
    j = strlen(entry);
    strcpy(string + stringlength, entry);
    stringlength += j;

    redtotal = redtotal > 20 ? 20 : redtotal;
    bluetotal = bluetotal > 20 ? 20 : bluetotal;

    for (i = 0; i < redtotal || i < bluetotal; i++) {
        if (i < redtotal) {
            cl_ent = g_edicts + 1 + redsorted[i];
            cl = &game.clients[redsorted[i]];

            strcpy(line, cl->pers.netname);
            if (!cl_ent->takedamage)
                LoPrint(line);
            else
                HiPrint(line);

            Q_snprintf(entry, sizeof(entry),
                       "xv 0 yv %d string2 \"%2d %3d %.12s\" ", i * 8 + 48, cl->resp.score,
                       cl->ping, line);

            j = strlen(entry);
            if (stringlength + j > 1024)
                break;
            strcpy(string + stringlength, entry);
            stringlength += j;
        }

        if (i < bluetotal) {
            cl_ent = g_edicts + 1 + bluesorted[i];
            cl = &game.clients[bluesorted[i]];

            strcpy(line, cl->pers.netname);
            if (!cl_ent->takedamage)
                LoPrint(line);
            else
                HiPrint(line);

            Q_snprintf(entry, sizeof(entry),
                       "xv 160 yv %d string2 \"%2d %3d %.12s\" ", i * 8 + 48, cl->resp.score,
                       cl->ping, line);

            j = strlen(entry);
            if (stringlength + j > 1024)
                break;
            strcpy(string + stringlength, entry);
            stringlength += j;
        }

    }

    gi.WriteByte(svc_layout);
    gi.WriteString(string);
}

/*
==================
RA_ScoreboardMessage

The arena ruleset's ScoreboardMessage row.  `scoremode` is RA2's three-state
replacement for baseq2's `showscores` bool -- 0 off, 1 the arena board, 2 the
server-wide one -- and a client with no arena has only the server-wide one to
show.
==================
*/
void RA_ScoreboardMessage(edict_t *ent, edict_t *killer)
{
    if (!ent->client->resp.context && ent->client->scoremode == 1)
        ent->client->scoremode = 2;

    if (ent->client->scoremode == 2) {
        Serverwide_ScoreboardMessage(ent);
        return;
    }

    if (arenas[ent->client->resp.context].idarena)
        Pickup_ScoreboardMessage(ent);
    else
        Arena_ScoreboardMessage(ent);
}

/*
=================
RA_EndLevel

R-OSP-9.  RA2 has its own map rotation (`maploop.c`) and baseq2 has `sv_maplist`
inside EndDMLevel; the double-rotation bug is what happens when both are live,
and the port that put `get_next_map()` into EndDMLevel had exactly that.  Here
it is the arena ruleset's EndLevel row, so only one rotation can run and which
one is a property of the ruleset rather than of call order (sec 7 rule 6's third
exemption, R-MODE-5's EndLevel hook).
=================
*/
void RA_EndLevel(void)
{
    char *next = get_next_map(level.mapname);

    if (next) {
        BeginIntermission(CreateTargetChangeLevel(next));
        return;
    }

    // maploop had nothing to say -- fall back to baseq2's rotation.
    EndDMLevel();
}

/*
=================
RA_CheckRules

R-RA-3's row.  The seven-state round machine is the arena ruleset's match rule,
so it is reached through the dispatch rather than bolted onto G_RunFrame beside
CheckDMRules -- which is where RA2's own port called it, and which left two
match-rule systems with no statement of how they compose.

They DO compose, and the statement is here: `arena_think` runs a **round**
inside one arena, `CheckDMRules` ends the **level**.  RA2 keeps `timelimit` and
`fraglimit` working at level scope and so does this; what changes is that both
now happen on one row, in a fixed order, instead of at two call sites whose
order was an accident of where each was inserted.

`multi_arena_think` walks one arena per frame -- `level.framenum % (num_arenas *
2)` -- so a server with 32 arenas costs one arena's think per frame rather than
32.  `num_arenas` is at least 1 after arena_init(), which is what makes that
modulo safe; the guard says so rather than trusting it, because a divide by zero
here is a server-killing fault in the frame loop.
=================
*/
void RA_CheckRules(void)
{
    if (num_arenas > 0)
        multi_arena_think();

    CheckDMRules();
}

const ruleset_ops_t ops_arena = {
    .name     = "arena",
    .CheckRules        = RA_CheckRules,
    .EndLevel          = RA_EndLevel,
    .ScoreboardMessage = RA_ScoreboardMessage,
    // BeginIntermission, SelectSpawnPoint and ClientPlaced are still dm's:
    // arena does its own placement inside init_player()/reinit_player(), which
    // PutClientInServer calls before the shared spawn path runs, so there is
    // nothing for those rows to do.  R-MODE-6 makes a NULL row inherit.
};
