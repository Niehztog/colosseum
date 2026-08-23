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
#include "arena/arena.h"

/*
===============================================================================

MENU OWNERSHIP AND THE OBSERVER QUESTION

Two small arbiters, here because p_hud.c is where the other claimants of the
layout channel already live -- the scoreboard, the inventory and the help
computer -- and because neither belongs inside any one donor's engine.

===============================================================================
*/

// R-MENU-2a/3.  One owner per client, and the open path closes the incumbent.
// Which engine that is is the owner field's business, not the caller's: an
// engine does not know the others exist, so nothing but this function can close
// a menu it did not open.
void G_MenuClose(edict_t *ent)
{
    if (!ent->client)
        return;

    switch (ent->client->menu_owner) {
    case MENU_CTF:
        ctf_PMenu_Close(ent);
        break;
    case MENU_ARENA:
        ra_MenuClose(ent);
        break;
    case MENU_TOURNEY:      // Phase 5
    case MENU_BOT:          // Phase 6
    case MENU_NONE:
        break;
    }
    ent->client->menu_owner = MENU_NONE;
}

void G_MenuOpen(edict_t *ent, menu_owner_t who)
{
    G_MenuClose(ent);
    ent->client->menu_owner = who;
}

bool G_MenuActive(edict_t *ent)
{
    return ent->client && ent->client->menu_owner != MENU_NONE;
}

// R-CTF-5.  "Is this client watching rather than playing?" has two answers in
// one library and thirteen call sites that must not have to know which.
//
// baseq2 has a `spectator` userinfo key, a password, a limit and a pers/resp
// pair that ClientBeginServerFrame watches for a change.  Threewave has none of
// that: it deleted both fields and expressed the same state as
// `ctf_team == CTF_NOTEAM`, joined and left through the menu.  Both survive
// (R-CORE-6's union, R-CORE-8's campaign), so the question is asked by name.
bool G_IsObserver(edict_t *ent)
{
    if (!ent->client)
        return false;
    if (G_Ruleset() == RULESET_CTF)
        return ent->client->resp.ctf_team == CTF_NOTEAM;
    // RA2 has a third spelling: it deleted baseq2's spectator too, and expresses
    // watching as fightstate.  Same question, third answer, still one predicate.
    if (G_Ruleset() == RULESET_ARENA)
        return ent->client->resp.fightstate == FIGHT_SPECTATING;
    return ent->client->resp.spectator;
}

/*
======================================================================

INTERMISSION

======================================================================
*/

void MoveClientToIntermission(edict_t *ent)
{
    if (deathmatch->value || coop->value)
        ent->client->showscores = true;
    VectorCopy(level.intermission_origin, ent->s.origin);
    ent->client->ps.pmove.origin[0] = COORD2SHORT(level.intermission_origin[0]);
    ent->client->ps.pmove.origin[1] = COORD2SHORT(level.intermission_origin[1]);
    ent->client->ps.pmove.origin[2] = COORD2SHORT(level.intermission_origin[2]);
    VectorCopy(level.intermission_angle, ent->client->ps.viewangles);
    ent->client->ps.pmove.pm_type = PM_FREEZE;
    ent->client->ps.gunindex = 0;
    ent->client->ps.blend[3] = 0;
    ent->client->ps.rdflags &= ~RDF_UNDERWATER;

    // clean up powerup info
    ent->client->quad_framenum = 0;
    ent->client->invincible_framenum = 0;
    ent->client->breather_framenum = 0;
    ent->client->enviro_framenum = 0;
    ent->client->grenade_blew_up = false;
    ent->client->grenade_framenum = 0;

    // RAFAEL
    ent->client->quadfire_framenum = 0;

    // RAFAEL
    ent->client->trap_blew_up = false;
    ent->client->trap_time = 0;
    ent->client->ps.rdflags &= ~RDF_IRGOGGLES;      // PGM
    ent->client->ir_framenum = 0;                   // PGM
    ent->client->nuke_framenum = 0;                 // PMM
    ent->client->double_framenum = 0;               // PMM

    ent->watertype = 0;
    ent->waterlevel = 0;
    ent->viewheight = 0;
    ent->s.modelindex = 0;
    ent->s.modelindex2 = 0;
    ent->s.modelindex3 = 0;
    ent->s.modelindex4 = 0;
    ent->s.effects = 0;
    ent->s.renderfx = 0;
    ent->s.sound = 0;
    ent->s.event = 0;
    ent->s.solid = 0;
    ent->solid = SOLID_NOT;
    ent->svflags = SVF_NOCLIENT;
    gi.unlinkentity(ent);

    gi.linkentity(ent);

    // add the layout

    if (deathmatch->value || coop->value) {
        G_ScoreboardMessage(ent, NULL);
        gi.unicast(ent, true);
    }

}

void BeginIntermission(edict_t *targ)
{
    int     i, n;
    edict_t *ent, *client;

    if (level.intermission_framenum)
        return;     // already activated

    game.autosaved = false;

    // respawn any dead clients
    for (i = 0; i < game.maxclients; i++) {
        client = g_edicts + 1 + i;
        if (!client->inuse)
            continue;
        if (client->health <= 0)
            respawn(client);
    }

    level.intermission_framenum = level.framenum;
    level.changemap = targ->map;

    if (strchr(level.changemap, '*')) {
        if (coop->value) {
            for (i = 0; i < game.maxclients; i++) {
                client = g_edicts + 1 + i;
                if (!client->inuse)
                    continue;
                // strip players of all keys between units
                for (n = 0; n < game.num_items; n++) {
                    if (itemlist[n].flags & IT_KEY)
                        client->client->pers.inventory[n] = 0;
                }
                client->client->pers.power_cubes = 0;
            }
        }
    } else {
        if (!deathmatch->value) {
            level.exitintermission = 1;     // go immediately to the next level
            return;
        }
    }

    level.exitintermission = 0;

    // find an intermission spot
    ent = G_Find(NULL, FOFS(classname), "info_player_intermission");
    if (!ent) {
        // the map creator forgot to put in an intermission point...
        ent = G_Find(NULL, FOFS(classname), "info_player_start");
        if (!ent)
            ent = G_Find(NULL, FOFS(classname), "info_player_deathmatch");
    } else {
        // chose one of four spots
        i = Q_rand() & 3;
        while (i--) {
            ent = G_Find(ent, FOFS(classname), "info_player_intermission");
            if (!ent)   // wrap around the list
                ent = G_Find(ent, FOFS(classname), "info_player_intermission");
        }
    }

    if (ent) {
        VectorCopy(ent->s.origin, level.intermission_origin);
        VectorCopy(ent->s.angles, level.intermission_angle);
    }

    // move all clients to the intermission point
    for (i = 0; i < game.maxclients; i++) {
        client = g_edicts + 1 + i;
        if (!client->inuse)
            continue;
        MoveClientToIntermission(client);
    }
}

/*
==================
DeathmatchScoreboardMessage

==================
*/
void DeathmatchScoreboardMessage(edict_t *ent, edict_t *killer)
{
    char    entry[1024];
    char    string[1400];
    int     stringlength;
    int     i, j, k;
    int     sorted[MAX_CLIENTS];
    int     sortedscores[MAX_CLIENTS];
    int     score, total;
    int     x, y;
    gclient_t   *cl;
    edict_t     *cl_ent;
    char    *tag;

    // sort the clients by score
    total = 0;
    for (i = 0; i < game.maxclients; i++) {
        cl_ent = g_edicts + 1 + i;
        if (!cl_ent->inuse || G_IsObserver(cl_ent))
            continue;
        score = game.clients[i].resp.score;
        for (j = 0; j < total; j++) {
            if (score > sortedscores[j])
                break;
        }
        for (k = total; k > j; k--) {
            sorted[k] = sorted[k - 1];
            sortedscores[k] = sortedscores[k - 1];
        }
        sorted[j] = i;
        sortedscores[j] = score;
        total++;
    }

    // print level name and exit rules
    string[0] = 0;

    stringlength = strlen(string);

    // add the clients in sorted order
    if (total > 12)
        total = 12;

    for (i = 0; i < total; i++) {
        cl = &game.clients[sorted[i]];
        cl_ent = g_edicts + 1 + sorted[i];

        x = (i >= 6) ? 160 : 0;
        y = 32 + 32 * (i % 6);

        // add a dogtag
        if (cl_ent == ent)
            tag = "tag1";
        else if (cl_ent == killer)
            tag = "tag2";
        else
            tag = NULL;

//===============
//ROGUE
        // allow new DM games to override the tag picture
        if (gamerules && gamerules->value) {
            if (DMGame.DogTag)
                DMGame.DogTag(cl_ent, killer, &tag);
        }
//ROGUE
//===============

        if (tag) {
            Q_snprintf(entry, sizeof(entry),
                       "xv %i yv %i picn %s ", x + 32, y, tag);
            j = strlen(entry);
            if (stringlength + j > 1024)
                break;
            strcpy(string + stringlength, entry);
            stringlength += j;
        }

        // send the layout
        Q_snprintf(entry, sizeof(entry),
                   "client %i %i %i %i %i %i ",
                   x, y, sorted[i], cl->resp.score, cl->ping, (level.framenum - cl->resp.enterframe) / 600);
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
DeathmatchScoreboard

Draw instead of help message.
Note that it isn't that hard to overflow the 1400 byte message limit!
==================
*/
// Non-static: CTF's admin menu and match code show the scoreboard too, and the
// ruleset picks which message it carries (R-CORE-13).
void DeathmatchScoreboard(edict_t *ent)
{
    G_ScoreboardMessage(ent, ent->enemy);
    // RA2 redraws the arena board every 32 frames, so it goes out unreliably;
    // only the server-wide board, which is a one-off, is worth a reliable slot.
    gi.unicast(ent, G_Ruleset() != RULESET_ARENA ||
               ent->client->scoremode == 2);
}

/*
==================
Cmd_Score_f

Display the scoreboard
==================
*/
void Cmd_Score_f(edict_t *ent)
{
    ent->client->showinventory = false;
    ent->client->showhelp = false;

    // R-MENU-3: the scoreboard and the menu are the same channel, so asking for
    // one closes the other.
    if (G_MenuActive(ent)) {
        G_MenuClose(ent);
        return;
    }

    if (!deathmatch->value && !coop->value)
        return;

    if (G_Ruleset() == RULESET_ARENA) {
        // RA2's `score` cycles: arena board -> server-wide -> off.  A client
        // with no arena has only the server-wide one, so it toggles that.
        if (ent->client->scoremode == 2)
            ent->client->scoremode = 0;
        else if (!ent->client->resp.context)
            ent->client->scoremode = 2;
        else
            ent->client->scoremode++;
        ent->client->update_chase = true;
        if (!ent->client->scoremode)
            return;
        DeathmatchScoreboard(ent);
        return;
    }

    if (ent->client->showscores) {
        ent->client->showscores = false;
        ent->client->update_chase = true;
        return;
    }

    ent->client->showscores = true;
    DeathmatchScoreboard(ent);
}

/*
==================
HelpComputer

Draw help computer.
==================
*/
static void HelpComputer(edict_t *ent)
{
    char    string[1024];
    char    *sk;

    if (skill->value == 0)
        sk = "easy";
    else if (skill->value == 1)
        sk = "medium";
    else if (skill->value == 2)
        sk = "hard";
    else
        sk = "hard+";

    // send the layout
    Q_snprintf(string, sizeof(string),
               "xv 32 yv 8 picn help "         // background
               "xv 202 yv 12 string2 \"%s\" "      // skill
               "xv 0 yv 24 cstring2 \"%s\" "       // level name
               "xv 0 yv 54 cstring2 \"%s\" "       // help 1
               "xv 0 yv 110 cstring2 \"%s\" "      // help 2
               "xv 50 yv 164 string2 \" kills     goals    secrets\" "
               "xv 50 yv 172 string2 \"%3i/%3i     %i/%i       %i/%i\" ",
               sk,
               level.level_name,
               game.helpmessage1,
               game.helpmessage2,
               level.killed_monsters, level.total_monsters,
               level.found_goals, level.total_goals,
               level.found_secrets, level.total_secrets);

    gi.WriteByte(svc_layout);
    gi.WriteString(string);
    gi.unicast(ent, true);
}

/*
==================
Cmd_Help_f

Display the current help message
==================
*/
void Cmd_Help_f(edict_t *ent)
{
    // this is for backwards compatability
    if (deathmatch->value) {
        Cmd_Score_f(ent);
        return;
    }

    ent->client->showinventory = false;
    ent->client->showscores = false;

    if (ent->client->showhelp && (ent->client->pers.game_helpchanged == game.helpchanged)) {
        ent->client->showhelp = false;
        return;
    }

    ent->client->showhelp = true;
    ent->client->pers.helpchanged = 0;
    HelpComputer(ent);
}

//=======================================================================

/*
===============
G_SetStats
===============
*/
void G_SetStats(edict_t *ent)
{
    const gitem_t   *item;
    int         index, cells;
    int         power_armor_type;

    //
    // health
    //
    ent->client->ps.stats[STAT_HEALTH_ICON] = level.pic_health;
    if (G_Ruleset() == RULESET_ARENA)
        ent->client->ps.stats[STAT_HEALTH_ICON] = RA_SkinIcon(ent);
    ent->client->ps.stats[STAT_HEALTH] = ent->health;

    //
    // ammo
    //
    if (!ent->client->ammo_index /* || !ent->client->pers.inventory[ent->client->ammo_index] */) {
        ent->client->ps.stats[STAT_AMMO_ICON] = 0;
        ent->client->ps.stats[STAT_AMMO] = 0;
    } else {
        item = &itemlist[ent->client->ammo_index];
        ent->client->ps.stats[STAT_AMMO_ICON] = gi.imageindex(item->icon);
        ent->client->ps.stats[STAT_AMMO] = ent->client->pers.inventory[ent->client->ammo_index];
    }

    //
    // armor
    //
    power_armor_type = PowerArmorType(ent);
    if (power_armor_type) {
        cells = ent->client->pers.inventory[ITEM_INDEX(FindItem("cells"))];
        if (cells == 0) {
            // ran out of cells for power armor
            ent->flags &= ~FL_POWER_ARMOR;
            gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/power2.wav"), 1, ATTN_NORM, 0);
            power_armor_type = 0;
        }
    }

    index = ArmorIndex(ent);
    if (power_armor_type && (!index || (level.framenum & 8))) {
        // flash between power armor and other armor icon
        if (power_armor_type == POWER_ARMOR_SHIELD)
            ent->client->ps.stats[STAT_ARMOR_ICON] = gi.imageindex("i_powershield");
        else
            ent->client->ps.stats[STAT_ARMOR_ICON] = gi.imageindex("i_powerscreen");
        ent->client->ps.stats[STAT_ARMOR] = cells;
    } else if (index) {
        item = GetItemByIndex(index);
        ent->client->ps.stats[STAT_ARMOR_ICON] = gi.imageindex(item->icon);
        ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.inventory[index];
    } else {
        ent->client->ps.stats[STAT_ARMOR_ICON] = 0;
        ent->client->ps.stats[STAT_ARMOR] = 0;
    }

    //
    // pickup message
    //
    if (level.framenum > ent->client->pickup_msg_framenum) {
        ent->client->ps.stats[STAT_PICKUP_ICON] = 0;
        ent->client->ps.stats[STAT_PICKUP_STRING] = 0;
    }

    //
    // timer 1 (quad, quadfire, double, enviro, breather)
    //
    if (ent->client->quad_framenum > level.framenum) {
        ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_quad");
        ent->client->ps.stats[STAT_TIMER] = (ent->client->quad_framenum - level.framenum) / 10;
    }
    // RAFAEL
    else if (ent->client->quadfire_framenum > level.framenum) {
        // note to self
        // need to change imageindex
        ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_quadfire");
        ent->client->ps.stats[STAT_TIMER] = (ent->client->quadfire_framenum - level.framenum) / 10;
    }
    // ROGUE -- Double Damage
    else if (ent->client->double_framenum > level.framenum) {
        ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_double");
        ent->client->ps.stats[STAT_TIMER] = (ent->client->double_framenum - level.framenum) / 10;
    } else if (ent->client->enviro_framenum > level.framenum) {
        ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_envirosuit");
        ent->client->ps.stats[STAT_TIMER] = (ent->client->enviro_framenum - level.framenum) / 10;
    } else if (ent->client->breather_framenum > level.framenum) {
        ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_rebreather");
        ent->client->ps.stats[STAT_TIMER] = (ent->client->breather_framenum - level.framenum) / 10;
    }
// PGM
    else if (ent->client->owned_sphere) {
        if (ent->client->owned_sphere->spawnflags == 1)         // defender
            ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_defender");
        else if (ent->client->owned_sphere->spawnflags == 2)    // hunter
            ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_hunter");
        else if (ent->client->owned_sphere->spawnflags == 4)    // vengeance
            ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_vengeance");
        else                                                    // error case
            ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("i_fixme");

        ent->client->ps.stats[STAT_TIMER] = (int)(ent->client->owned_sphere->wait - level.time);
    } else if (ent->client->ir_framenum > level.framenum) {
        ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_ir");
        ent->client->ps.stats[STAT_TIMER] = (ent->client->ir_framenum - level.framenum) / 10;
    }
// PGM
    else {
        ent->client->ps.stats[STAT_TIMER_ICON] = 0;
        ent->client->ps.stats[STAT_TIMER] = 0;
    }

    //
    // timer 2 (pent)
    //
    // R-OSP-7 clause 4's reference case: a baseq2 mechanic written by the shared
    // G_SetStats that needs two private slots in every ruleset, and that each
    // donor had to place differently (baseq2 18/19, RA2 26/27, tourney 29/30,
    // and CTF had nowhere at all).  It asks the slot map where its pair landed
    // and does not care; where the ruleset has no pair, both writes and the bar
    // items vanish together and the pent falls back to timer 1 below.
    if (G_Ruleset() == RULESET_ARENA)
        RA_SetQueueStats(ent);

    G_SetStat(ent, SID_TIMER2_ICON, 0);
    G_SetStat(ent, SID_TIMER2, 0);
    if (ent->client->invincible_framenum > level.framenum) {
        if (ent->client->ps.stats[STAT_TIMER_ICON] && G_Stat(SID_TIMER2_ICON) >= 0) {
            G_SetStat(ent, SID_TIMER2_ICON, gi.imageindex("p_invulnerability"));
            G_SetStat(ent, SID_TIMER2, (ent->client->invincible_framenum - level.framenum) / 10);
        } else {
            ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_invulnerability");
            ent->client->ps.stats[STAT_TIMER] = (ent->client->invincible_framenum - level.framenum) / 10;
        }
    }

    //
    // selected item
    //
    if (ent->client->pers.selected_item == -1)
        ent->client->ps.stats[STAT_SELECTED_ICON] = 0;
    else
        ent->client->ps.stats[STAT_SELECTED_ICON] = gi.imageindex(itemlist[ent->client->pers.selected_item].icon);

    ent->client->ps.stats[STAT_SELECTED_ITEM] = ent->client->pers.selected_item;

    //
    // layouts
    //
    ent->client->ps.stats[STAT_LAYOUTS] = 0;

    if (deathmatch->value) {
        if (ent->client->pers.health <= 0 || level.intermission_framenum
            || ent->client->showscores)
            ent->client->ps.stats[STAT_LAYOUTS] |= LAYOUTS_LAYOUT;
        if (ent->client->showinventory && ent->client->pers.health > 0)
            ent->client->ps.stats[STAT_LAYOUTS] |= LAYOUTS_INVENTORY;
    } else {
        if (ent->client->showscores || ent->client->showhelp)
            ent->client->ps.stats[STAT_LAYOUTS] |= LAYOUTS_LAYOUT;
        if (ent->client->showinventory && ent->client->pers.health > 0)
            ent->client->ps.stats[STAT_LAYOUTS] |= LAYOUTS_INVENTORY;
    }

    //
    // frags
    //
    ent->client->ps.stats[STAT_FRAGS] = ent->client->resp.score;

    //
    // help icon / current weapon if not shown
    //
    if (ent->client->pers.helpchanged && (level.framenum & 8))
        ent->client->ps.stats[STAT_HELPICON] = gi.imageindex("i_help");
    else if ((ent->client->pers.hand == CENTER_HANDED || ent->client->ps.fov > 91)
             && ent->client->pers.weapon)
        ent->client->ps.stats[STAT_HELPICON] = gi.imageindex(ent->client->pers.weapon->icon);
    else
        ent->client->ps.stats[STAT_HELPICON] = 0;

    G_SetStat(ent, SID_SPECTATOR, 0);

    // CTF's fourteen stats.  A gated tail rather than an ops row: everything
    // above this line is shared and only the last step differs, which is exactly
    // the case R-MODE-5 says to gate inline rather than to hook.
    if (G_Ruleset() == RULESET_CTF)
        SetCTFStats(ent);
}

/*
===============
G_CheckChaseStats
===============
*/
void G_CheckChaseStats(edict_t *ent)
{
    int i;
    gclient_t *cl;

    for (i = 1; i <= game.maxclients; i++) {
        cl = g_edicts[i].client;
        if (!g_edicts[i].inuse || cl->chase_target != ent)
            continue;
        memcpy(cl->ps.stats, ent->client->ps.stats, sizeof(cl->ps.stats));
        G_SetSpectatorStats(g_edicts + i);
    }
}

/*
===============
G_SetSpectatorStats
===============
*/
void G_SetSpectatorStats(edict_t *ent)
{
    gclient_t *cl = ent->client;

    if (!cl->chase_target)
        G_SetStats(ent);

    G_SetStat(ent, SID_SPECTATOR, 1);

    // layouts are independant in spectator
    cl->ps.stats[STAT_LAYOUTS] = 0;
    if (cl->pers.health <= 0 || level.intermission_framenum || cl->showscores)
        cl->ps.stats[STAT_LAYOUTS] |= LAYOUTS_LAYOUT;
    if (cl->showinventory && cl->pers.health > 0)
        cl->ps.stats[STAT_LAYOUTS] |= LAYOUTS_INVENTORY;

    if (cl->chase_target && cl->chase_target->inuse) {
        G_SetStat(ent, SID_CHASE, game.csr.playerskins +
                  (cl->chase_target - g_edicts) - 1);
    } else
        G_SetStat(ent, SID_CHASE, 0);
}
