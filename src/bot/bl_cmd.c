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
// Bot commands, from osp-tourney@1d8427e.
//===========================================================================
//
// Name:                bl_cmd.c
// Function:        bot commands
// Programmer:      Mr Elusive (MrElusive@demigod.demon.nl)
// Last update: 1999-02-10
// Tab Size:        3
//===========================================================================

#include "g_local.h"
#include "bot/bl_main.h"
#include "bot/bl_spawn.h"
#include "bot/bl_redirgi.h"
#include "bot/bl_botcfg.h"
#include "bot/bl_debug.h"
#include "bot/p_botmenu.h"

typedef struct nearbyitem_s
{
    const char *classname;
    float weight;
} nearbyitem_t;

static const nearbyitem_t nearbyitems[] =
{
    {"item_armor_body",         70},
    {"item_armor_combat",       70},
    {"item_armor_jacket",       50},
    {"item_power_screen",       90},
    {"item_power_shield",       90},
    {"weapon_shotgun",          40},
    {"weapon_supershotgun",     60},
    {"weapon_machinegun",       50},
    {"weapon_chaingun",         100},
    {"weapon_grenadelauncher",  100},
    {"weapon_rocketlauncher",   100},
    {"weapon_hyperblaster",     100},
    {"weapon_railgun",          100},
    {"weapon_bfg",              100},
    {"item_quad",               100},
    {"item_invulnerability",    100},
    {"item_silencer",           60},
    {"item_breather",           60},
    {"item_enviro",             60},
    {"item_ancient_head",       60},
    {"item_bandolier",          60},
    {"item_pack",               70},
    {"key_data_cd",             40},
    {"key_power_cube",          40},
    {"key_pyramid",             40},
    {"key_data_spinner",        40},
    {"key_pass",                40},
    {"key_blue_key",            40},
    {"key_red_key",             40},
    //CTF
    {"item_flag_team1",         100},
    {"item_flag_team2",         100},
    // The table had already been extended once -- the two flags above
    // are Colosseum's, under a `//CTF` fence -- and stopped there, so the rest
    // of what a merged tree can place was invisible to it: every Reckoning and
    // Ground Zero item, CTF's four techs and its grapple, and three baseq2 rows
    // the 1999 list simply omitted.
    //
    // Weights are by kind rather than invented per item, which is how the rows
    // above read: a weapon that ends fights is 100, a lesser weapon 40-60,
    // armour 50-70, a powerup 100, a timed pickup 60, a key 40.
    {"item_adrenaline",         60},
    {"key_commander_head",      40},
    {"key_airstrike_target",    40},
    // CTF's four techs.  No map places them -- CTFSetupTechSpawn/SpawnTech
    // create the edicts at runtime -- which is exactly why they belong here:
    // they ARE world entities and this loop walks g_edicts.
    //
    // The grapple is not here, and checking why is what kept it out: its
    // itemlist row is documented "always owned, never in the world", nothing
    // spawns one, and no pak in the test data places one.  This loop matches
    // `item->classname` on live edicts, so the row could never have fired.
    //CTF
    {"item_tech1",              90},
    {"item_tech2",              90},
    {"item_tech3",              90},
    {"item_tech4",              90},
    //XATRIX
    {"weapon_boomer",           60},
    {"weapon_phalanx",          100},
    {"ammo_trap",               60},
    {"item_quadfire",           100},
    //ROGUE
    {"weapon_etf_rifle",        60},
    {"weapon_proxlauncher",     100},
    {"weapon_plasmabeam",       100},
    {"weapon_chainfist",        20},
    {"ammo_tesla",              60},
    {"ammo_nuke",               100},
    {"item_double",             100},
    {"item_ir_goggles",         60},
    {"item_sphere_vengeance",   100},
    {"item_sphere_hunter",      100},
    {"item_sphere_defender",    100},
    {"item_doppleganger",       100},
    {"key_green_key",           40},
    {"key_nuke_container",      40},
    // The Disruptor is absent on purpose: it is IT_NOT_GIVEABLE and carries no
    // IT_WEAPON bit, so nothing else in this tree treats it as a weapon
    // a player would cross a room for.
    {NULL,                      0}
};

//===========================================================================
// The GPS commands v0.93 grew.
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void ShowGPSText(edict_t *ent, vec3_t goal)
{
    vec3_t hordir, dir, forward, right;
    float dot;
    char buf[128];

    VectorSubtract(ent->s.origin, goal, dir);
    VectorCopy(dir, hordir);
    hordir[2] = 0;
    AngleVectors(ent->client->resp.cmd_angles, forward, right, NULL);
    forward[2] = 0;
    right[2] = 0;
    VectorNormalize(hordir);
    VectorNormalize(forward);
    VectorNormalize(right);

    dot = DotProduct(hordir, forward);

    Q_snprintf(buf, sizeof(buf),
               "^\n< >\n_\n\ndx=%1.0f dy=%1.0f dz=%1.0f\n\ndistance = %1.0f",
               dir[0], dir[1], dir[2], VectorLength(dir));
    if (dot > 0.7f) buf[0] += 128;
    else if (dot < -0.7f) buf[6] += 128;
    else if (DotProduct(hordir, right) > 0) buf[4] += 128;
    else buf[2] += 128;

    gi.centerprintf(ent, "%s", buf);
} //end of the function ShowGPSText
//===========================================================================
// `gps <x> <y> <z>` -- the compass the bot's checkpoint chatter refers to.
// The donor kept ShowGPSText and dropped the command that reaches it, which
// makes the function dead; the contract lists "the GPS and macro commands
// bl_cmd.c grew in v0.93" as required, so the command comes back.
//===========================================================================
static void GPS_f(edict_t *ent)
{
    vec3_t goal;

    if (gi.argc() < 4)
    {
        gi.cprintf(ent, PRINT_HIGH, "gps <x> <y> <z>\n");
        return;
    } //end if
    goal[0] = Q_atof(gi.argv(1));
    goal[1] = Q_atof(gi.argv(2));
    goal[2] = Q_atof(gi.argv(3));
    ShowGPSText(ent, goal);
} //end of the function GPS_f
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void TeamHelp_f(edict_t *ent)
{
    int i, j;
    float radius, weight, bestweight, dist;
    char buf[144];
    vec3_t eorg;
    edict_t *item, *bestitem;

    if (!ent) return;
    if (!ent->client) return;
    if (ent->flags & FL_OBSERVER) return;

    radius = 500;

    bestweight = 0;
    bestitem = NULL;
    for (item = g_edicts; item < &g_edicts[globals.num_edicts]; item++)
    {
        if (!item->inuse) continue;
        // The donor tests item->item only at the very end, when it prints the
        // pickup name -- but the classname compare below can match an entity
        // that has no item pointer at all (a `target_` alias, a map's own
        // trigger renamed).  Ask first.
        if (!item->item) continue;
        for (j = 0; j < 3; j++)
        {
            eorg[j] = ent->s.origin[j] - (item->s.origin[j] + (item->mins[j] + item->maxs[j]) * 0.5f);
        } //end for
        dist = VectorLength(eorg);
        //the item should be in the given radius
        if (dist > radius) continue;
        //never use dropped items
        if (item->spawnflags & DROPPED_ITEM) continue;
        //the location of the item should be visible
        if (!visible(ent, item)) continue;
        //check if item and calculate weight
        for (i = 0; nearbyitems[i].classname; i++)
        {
            if (!strcmp(item->classname, nearbyitems[i].classname))
            {
                weight = nearbyitems[i].weight / dist;
                if (weight > bestweight)
                {
                    bestweight = weight;
                    bestitem = item;
                } //end if
                break;
            } //end if
        } //end for
    } //end for
    buf[0] = '\0';
    //if addressed
    if (gi.argc() > 1)
    {
        Q_strlcat(buf, gi.argv(1), sizeof(buf));
        Q_strlcat(buf, " ", sizeof(buf));
    } //end if
    //help or accompany
    if (!strcmp(gi.argv(0), "teamhelp")) Q_strlcat(buf, "help me", sizeof(buf));
    else Q_strlcat(buf, "accompany me", sizeof(buf));
    //if near an item
    if (bestitem)
    {
        Q_strlcat(buf, " near the ", sizeof(buf));
        Q_strlcat(buf, bestitem->item->pickup_name, sizeof(buf));
    } //end if
    BotClientCommand(DF_ENTCLIENT(ent), "say_team", buf, NULL);
} //end of the function TeamHelp_f
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void CheckPoint_f(edict_t *ent)
{
    vec3_t mins = {-16, -16, -24}, maxs = {16, 16, 4};
    vec3_t start, end;
    char buf[144];
    int loc[3], x, y, z;
    trace_t trace;

    if (gi.argc() <= 1)
    {
        gi.cprintf(ent, PRINT_HIGH, "checkpoint <name>\n");
        return;
    } //end if

    VectorCopy(ent->s.origin, end);
    loc[0] = (int) end[0];
    loc[1] = (int) end[1];
    loc[2] = (int) end[2];
    // The donor's triple loop breaks out of each level with `if (z <= 1) break;`
    // -- which is true on every iteration, so only (x,y,z) = (-1,-1,-1) is ever
    // traced and `x > 1` is never reached.  Written as a search that stops on
    // the first free position, which is what the comment and the error message
    // both say it is.
    for (x = -1; x <= 1; x++)
    {
        for (y = -1; y <= 1; y++)
        {
            for (z = -1; z <= 1; z++)
            {
                start[0] = loc[0] + x;
                start[1] = loc[1] + y;
                start[2] = loc[2] + z;
                //
                trace = gi.trace(start, mins, maxs, end, ent, MASK_PLAYERSOLID);
                //
                if (!trace.startsolid)
                {
                    Q_snprintf(buf, sizeof(buf), "checkpoint %s is at gps %d %d %d",
                               gi.argv(1), (int)start[0], (int)start[1], (int)start[2]);
                    BotClientCommand(DF_ENTCLIENT(ent), "say_team", buf, NULL);
                    return;
                } //end if
            } //end for
        } //end for
    } //end for
    gi.cprintf(ent, PRINT_HIGH, "invalid checkpoint position\n");
} //end of the function CheckPoint_f
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void BotDumpInventory(void)
{
    int i;

    for (i = 1; itemlist[i].pickup_name; i++)
    {
        gi.dprintf("%-16s %d\n", itemlist[i].pickup_name, i);
    } //end for
} //end of the function BotDumpInventory
//===========================================================================
// `serveronlybotcmds` gates client access to the bot commands and
// Defaults to 1 -- the 1999 default of 0 exposed bl_spawn.c's bot-name copies
// to any client, and osp-tourney's own port doc flags exactly this.  Adopted
// deliberately as policy.
//===========================================================================
static bool BotCmdRefused(edict_t *ent, const char *what)
{
    if (!ent) return false;                 // the console is always allowed
    if (!gi.cvar("serveronlybotcmds", "1", 0)->value) return false;
    gi.cprintf(ent, PRINT_HIGH, "not allowed to %s\n", what);
    return true;
}
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static bool BotServerCmd(const char *cmd, edict_t *ent, int server)
{
    if (Q_stricmp(cmd, "addbot") == 0)
    {
        if (!BotCmdRefused(ent, "add bots")) BotAddDeathmatch(ent);
    } //end if
    else if (Q_stricmp(cmd, "removebot") == 0)
    {
        if (!BotCmdRefused(ent, "remove bots")) BotRemoveDeathmatch(ent);
    } //end else if
    else if (Q_stricmp(cmd, "addrandom") == 0)
    {
        if (!BotCmdRefused(ent, "randomly add bots"))
        {
            int i, num;

            if (!Q_stricmp(gi.argv(0), "sv")) num = 2;
            else num = 1;
            if (gi.argc() <= num) AddRandomBot(ent);
            else
            {
                num = Q_atoi(gi.argv(num));
                for (i = 0; i < num; i++) if (!AddRandomBot(ent)) break;
            } //end else
        } //end if
    } //end else if
    else if (Q_stricmp(cmd, "becomebot") == 0)
    {
        if (!BotCmdRefused(ent, "become a bot")) BotBecomeDeathmatch(ent);
    } //end else if
    else if (Q_stricmp(cmd, "botpause") == 0)
    {
        // The contract requires it and the donor dropped the branch; the SDK's own
        // was behind BOT_DEBUG, which is never defined (see bl_main.h).
        if (!BotCmdRefused(ent, "pause the bots"))
        {
            botglobals.nobotai = !botglobals.nobotai;
            gi.bprintf(PRINT_HIGH, "bot AI %s\n",
                       botglobals.nobotai ? "paused" : "running");
        } //end if
    } //end else if
    else if (Q_stricmp(cmd, "menu") == 0)
    {
        // The `menu` command, dropped from osp-tourney's bl_cmd.c
        // because that mod has its own menus, comes back with the bot menu.
        // ToggleBotMenu does its own gating, which is why it is not behind
        // BotCmdRefused: the menu is gated on the rcon password rather
        // than on serveronlybotcmds alone, and exempts the host of a listen
        // server, which BotCmdRefused's console-or-nothing test cannot express.
        bot_MenuToggle(ent);
    } //end else if
    else
    {
        return false;
    } //end else
    return true;
} //end of the function BotServerCmd
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
bool BotCmd(const char *cmd, edict_t *ent, int server)
{
    char userinfo[MAX_INFO_STRING];

    // The Gladiator botlib is deathmatch-only, so under `sp` the
    // whole command set is simply not there.  Answering false lets the caller
    // report "unknown command", which is the truth.
    if (!G_BotsAllowed())
        return false;

    //check for commands that might be server only
    if (BotServerCmd(cmd, ent, server))
    {
    } //end if
    else if (server && Q_stricmp(cmd, "modelindex") == 0)
    {
        BotDumpModelindex();
    } //end else if
    else if (server && Q_stricmp(cmd, "soundindex") == 0)
    {
        BotDumpSoundindex();
    } //end else if
    else if (server && Q_stricmp(cmd, "imageindex") == 0)
    {
        BotDumpImageindex();
    } //end else if
    else if (server && Q_stricmp(cmd, "indexprobe") == 0)
    {
        //The index-table control.  gi.argv(2) is the table, gi.argv(3) the index.
        BotIndexProbe(gi.argc() > 2 ? gi.argv(2) : NULL,
                      gi.argc() > 3 ? Q_atoi(gi.argv(3)) : -1);
    } //end else if
    else if (server && Q_stricmp(cmd, "inventory") == 0)
    {
        BotDumpInventory();
    } //end else if
    else if (server && Q_stricmp(cmd, "botperf") == 0)
    {
        //The frame measurement.  An argument of "reset" starts a new window,
        //so a script can discard the frames a map load and 32 connects cost
        //and measure only the steady state.
        if (gi.argc() > 2 && !Q_stricmp(gi.argv(2), "reset"))
        {
            BotPerfReset();
            gi.dprintf("botperf reset\n");
        } //end if
        else
        {
            BotPerfReport();
        } //end else
    } //end else if
    else if (server && Q_stricmp(cmd, "botlibdump") == 0)
    {
        BotLibraryDump();
    } //end else if
    else if (server && Q_stricmp(cmd, "clientdump") == 0)
    {
        BotClientDump();
    } //end else if
    else if (server && Q_stricmp(cmd, "botinv") == 0)
    {
        //The slot instrument: the inventory as the brain reads it.
        BotInventoryDump();
    } //end else if
    else if (ent && Q_stricmp(cmd, "bbox") == 0)
    {
        // The visible bounding box, which the SDK reaches from its debug
        // build only.  Behind sv_cheats, because it shows through walls.
        if (sv_cheats->value) ToggleVisibleBoundingBox(ent);
        else gi.cprintf(ent, PRINT_HIGH, "bbox needs cheats\n");
    } //end else if
    //so the bot can easily change its name
    else if (ent && Q_stricmp(cmd, "name") == 0)
    {
        // The three userinfo NUL terminations.  Q_strlcpy is what
        // gives all three at once -- the donor's memcpy of sizeof(userinfo)-1
        // copies the terminator's neighbour, not the terminator.
        Q_strlcpy(userinfo, ent->client->pers.userinfo, sizeof(userinfo));
        Info_SetValueForKey(userinfo, "name", gi.argv(1));
        ClientUserinfoChanged(ent, userinfo);
    } //end else if
    //so the bot can easily change its skin
    else if (ent && Q_stricmp(cmd, "skin") == 0)
    {
        Q_strlcpy(userinfo, ent->client->pers.userinfo, sizeof(userinfo));
        Info_SetValueForKey(userinfo, "skin", gi.argv(1));
        ClientUserinfoChanged(ent, userinfo);
    } //end else if
    //so the bot can easily change its gender
    else if (ent && Q_stricmp(cmd, "gender") == 0)
    {
        Q_strlcpy(userinfo, ent->client->pers.userinfo, sizeof(userinfo));
        Info_SetValueForKey(userinfo, "gender", gi.argv(1));
        ClientUserinfoChanged(ent, userinfo);
    } //end else if
    else if (ent && Q_stricmp(cmd, "teamhelp") == 0)
    {
        TeamHelp_f(ent);
    } //end else if
    else if (ent && Q_stricmp(cmd, "teamaccompany") == 0)
    {
        TeamHelp_f(ent);
    } //end else if
    else if (ent && Q_stricmp(cmd, "checkpoint") == 0)
    {
        CheckPoint_f(ent);
    } //end else if
    else if (ent && Q_stricmp(cmd, "gps") == 0)
    {
        GPS_f(ent);
    } //end else if
    else
    {
        return false;
    } //end else
    return true;
} //end of the function BotCmd
