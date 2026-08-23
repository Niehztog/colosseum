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
// OSP Tourney DM v2.75, from osp-tourney@1d8427e (doc/provenance.md).
// Donor-only: baseq2 has no counterpart, so it lives in src/tourney/ rather
// than being merged into a spine file (R-CORE-7).  The reconstruction's
// asm-matching address comments are stripped -- SPECS.md N1 makes those oracles
// meaningless here, and they survive at the pin.
// osp_main.c -- <INVENTED FILENAME>. The mod's core: the config loader, the
// cvar registration, the match-state machine, the item enable/disable layer,
// the player-ID overlay and the scoreboard/params builders.

#include "g_local.h"
#include "tourney/osp_types.h"
#include "tourney/osp_stats.h"
#include "bot/bl_main.h"
#include "bot/bl_botcfg.h"
// The two bot entry points, until Phase 6 -- see src/tourney/osp_botseam.c.
void BotServerCommand(char *str, ...);
void BotDestroy(edict_t *bot);

int conf_size = 0;
int blink_on_count = 9;
int blink_off_count = 0;
int bots_votedin = 0;
int bots_delaytime = 15;
int bots_loadstat = 0;
int client_maxframes = 0;
int console_stampcount = 0;
int maxconn_clients = 0;
int reconn_index = 2;
int connected_clients = 0;
int active_clients = 0;
int bot_watch = 1;
int game_init = 0;
int m_mode = 0;
int sync_stat = 8;
int sync_frame = 0;
int sync_startframe = 0;
float   sync_time = 0;
int time_update = 0;
int time_blink = 0;
int rune_stat = 0;
int start_count = 0;
int start_suddendeath = 0;
int manual_map = 0;
int vote_inprogress = 0;
int vote_frametime = 0;
int vote_item = 0;
int vote_yea = 0;
int vote_nay = 0;
FILE * server_log = NULL;
char    wav_file[125] =
    "world/battle3.wav\0\0\0\0\0\0\0\0world/comp_hum3.wav\0\0\0\0\0\0world/xian1.wav\0\0\0\0\0\0\0\0\0\0makron/laf4.wav\0\0\0\0\0\0\0\0\0\0world/xian1.wav\0\0\0\0\0\0\0\0\0";
a_info_t a_info[10] = {
    {  0, "Blaster   :" },
    {  1, "Shotgun   :" },
    {  2, "S.Shotgun :" },
    {  3, "Machinegun:" },
    {  4, "Chaingun  :" },
    { 10, "Grenades  :" },
    {  5, "G.Launcher:" },
    {  6, "R.Launcher:" },
    {  7, "H.Blaster :" },
    {  8, "Railgun   :" },
};
int motd_read = 0;
int who_paused = -1;
char    conf_info[50][64];
char    conf_name[50][64];
cvar_t * qualifier_numspots;
cvar_t * max_cells;
p_acc_t p_acc[256];
p_acc_t o_acc[256];
cvar_t * allow_id;
cvar_t * time_remaining;
cvar_t * start_armortype;
cvar_t * client_hud;
char    old_scores[2048];
cvar_t * match_features;
cvar_t * max_armor;
cvar_t * start_armor;
cvar_t * runes_model;
cvar_t * hook_holdplayertime;
cvar_t * team_duelrecover;
cvar_t * match_pausetime;
cvar_t * vote_threshold;
cvar_t * console_timestamp;
cvar_t * nextlevel_click;
cvar_t * match_type;
cvar_t * bots_warmuptime;
int max_items[11];
cvar_t * team_a_score;
cvar_t * demo_referee;
cvar_t * pack_shells;
int start_items[11];
cvar_t * pack_cells;
cvar_t * runes_regen_amax;
cvar_t * vote_time;
cvar_t * client_nomove;
cvar_t * vote_carryover;
cvar_t * match_countinfo;
cvar_t * start_shells;
cvar_t * client_protect;
int pack_spawn;
cvar_t * runes_perplayer;
cvar_t * vote_bots_max;
cvar_t * referee_password;
cvar_t * team_a_hookcolor;
cvar_t * max_grenades;
cvar_t * vote_enable_hook;
cvar_t * vote_enable_map;
cvar_t * power_armor_screen;
cvar_t * team_overtime_time;
cvar_t * max_rockets;
cvar_t * vote_enable_time;
cvar_t * team_b_name;
cvar_t * bots_autoload;
cvar_t * hook_enable;
cvar_t * map_halt;
cvar_t * osp_game;
cvar_t * runes_vampire_max;
int item_settings;
cvar_t * vote_config_default;
cvar_t * runes_enable;
cvar_t * demo_tag;
cvar_t * pack_slugs;
cvar_t * runes_vampire;
cvar_t * pack_health;
cvar_t * pack_grenades;
cvar_t * client_botdetect;
cvar_t * runes_strength;
cvar_t * team_b_score;
cvar_t * team_a_skin;
cvar_t * hook_pullspeed;
cvar_t * resp_delay;
cvar_t * max_health;
cvar_t * match_mode;
char    conf_file[2048];
int p_order[28];
cvar_t * team_idteam;
cvar_t * bots_botfile;
cvar_t * fast_maxpbound;
cvar_t * warmup_armor;
cvar_t * team_b_skin;
cvar_t * runes_regen_hmax;
cvar_t * hook_sky;
cvar_t * vote_countspectators;
cvar_t * pack_armor;
cvar_t * bots_noclients;
cvar_t * client_recover;
cvar_t * client_fastweap;
cvar_t * client_maxping;
cvar_t * vote_enable_bots;
cvar_t * vote_enable_toggles;
cvar_t * max_shells;
char    default_timelimit[8];
cvar_t * fast_respawn;
cvar_t * runes_min;
cvar_t * weapon_have;
cvar_t * armor_shard;
cvar_t * match_endinfo;
int level_start;
int start_weap[11];
cvar_t * team_nextuptime;
cvar_t * referee_enable;
char    default_hook[8];
cvar_t * client_muzzlemode;
char    default_fraglimit[8];
cvar_t * menu_maxtime;
cvar_t * power_armor_shield;
cvar_t * team_hurtself;
cvar_t * fast_minpbound;
gclient_t   saved_clients[128];
cvar_t * match_readypercent;
cvar_t * vote_enable_config;
cvar_t * weapon_initial;
cvar_t * client_maxrate;
cvar_t * menu_timestep;
cvar_t * camera_pitch;
cvar_t * pack_rockets;
cvar_t * bots_minplayers;
cvar_t * runes_resist;
cvar_t * team_overtime_mode;
cvar_t * ffa_hurtself;
cvar_t * vote_enable;
cvar_t * armor_jacket;
cvar_t * menu_maxfrag;
cvar_t * demo_player;
cvar_t * hook_speed;
cvar_t * vote_enable_runes;
cvar_t * max_bullets;
cvar_t * team_a_name;
cvar_t * camera_depth;
cvar_t * client_highscores;
cvar_t * match_strictmode;
cvar_t * start_cells;
cvar_t * client_maxfps;
cvar_t * bots_delayload;
cvar_t * team_b_hookcolor;
cvar_t * match_countdown;
cvar_t * match_timeouts;
cvar_t * start_grenades;
cvar_t * hook_color;
cvar_t * team_maxplayers;
cvar_t * start_health;
cvar_t * start_bullets;
int initial_weap;
cvar_t * start_rockets;
cvar_t * match_prestartpercent;
cvar_t * vote_config_defaultname;
cvar_t * hook_wait;
cvar_t * client_deathweapdrop;
cvar_t * team_overtime_count;
cvar_t * runes_max;
char    reconn_player[32];
cvar_t * warmup_health;
cvar_t * hook_incdamage;
cvar_t * nextlevel_lazy;
cvar_t * runes_flash;
cvar_t * qualifier_forceskins;
cvar_t * numgibs;
cvar_t * armor_combat;
cvar_t * client_minping;
int pack_items[11];
cvar_t * max_slugs;
cvar_t * team_recovertime;
cvar_t * hook_initdamage;
cvar_t * client_infochange;
cvar_t * menu_fragstep;
cvar_t * team_lockskin;
cvar_t * match_latejoin;
cvar_t * damage_railgun;
cvar_t * __current_config;
cvar_t * hook_maxdamage;
char    vote_value[64];
cvar_t * qualifier_skinname;
cvar_t * armor_body;
int pack_life;
cvar_t * match_startsound;
cvar_t * match_endmusic;
cvar_t * pack_bullets;
cvar_t * start_slugs;
cvar_t * hook_holdtime;
cvar_t * team_hurtteam;
cvar_t * vote_enable_frag;
cvar_t * vote_enable_kick;
char    match_motd[1024];
char    match_info[1024];

// Register every cvar the mod owns and clamp the ones that have a legal range.
void OSP_gameInit(void)
{
    char        buf[32];
    int         i;

    gi.cvar("sv_airaccelerate", "0", 0);
    resp_delay = gi.cvar("respawn_delay", "0", 0);
    console_timestamp = gi.cvar("console_timestamp", "0", 0);
    osp_game = gi.cvar("gamename", "OSP Tourney DM v(2.75)",
                       CVAR_SERVERINFO | CVAR_NOSET);
    nextlevel_click = gi.cvar("nextlevel_click", "15.0", 0);
    nextlevel_lazy = gi.cvar("nextlevel_default", "45.0", 0);
    numgibs = gi.cvar("numgibs", "4", 0);

    time_remaining = gi.cvar("time_remaining", "ServerInit", CVAR_SERVERINFO);
    gi.cvar_set("time_remaining", "ServerInit");

    allow_id = gi.cvar("allow_id", "1", 0);
    ffa_hurtself = gi.cvar("ffa_hurtself", "1", 0);
    damage_railgun = gi.cvar("damage_railgun", "100", 0);
    map_halt = gi.cvar("map_halt", "0", 0);

    bots_autoload = gi.cvar("bots_autoload", "0", 0);
    bots_botfile = gi.cvar("bots_botfile", "botcfg/bots.cfg", 0);
    bots_delayload = gi.cvar("bots_delayload", "0", 0);
    bots_minplayers = gi.cvar("bots_minplayers", "4", 0);
    bots_noclients = gi.cvar("bots_noclients", "0", 0);
    bots_warmuptime = gi.cvar("bots_warmuptime", "0", 0);

    match_mode = gi.cvar("match_mode", "0", 0);
    m_mode = (int)match_mode->value;
    match_type = gi.cvar("match_type", "RegularDM", CVAR_SERVERINFO);
    match_features = gi.cvar("match_info", "None", CVAR_SERVERINFO);
    match_latejoin = gi.cvar("match_latejoin", "2", 0);
    match_endinfo = gi.cvar("match_endinfo", "OSP Tourney DM v(2.75)", 0);
    match_endmusic = gi.cvar("match_endmusic", "default", 0);
    match_prestartpercent = gi.cvar("match_prestartpercent", "50", 0);
    match_readypercent = gi.cvar("match_readypercent", "100", 0);
    match_pausetime = gi.cvar("match_pausetime", "60.0", 0);
    match_timeouts = gi.cvar("match_timeouts", "3", 0);
    match_countdown = gi.cvar("match_countdown", "30", 0);
    match_countinfo = gi.cvar("match_countinfo", "1", 0);
    match_startsound = gi.cvar("match_startsound", "1", 0);
    match_strictmode = gi.cvar("match_strictmode", "0", 0);

    if ((int)match_countdown->value < 14)
        gi.cvar_set("match_countdown", "14");
    if (!m_mode)
        gi.cvar_set("match_strictmode", "0");
    who_paused = -1;

    fast_respawn = gi.cvar("fast_respawn", "1.0", 0);
    fast_minpbound = gi.cvar("fast_minpbound", "1", 0);
    fast_maxpbound = gi.cvar("fast_maxpbound", "20", 0);
    if ((int)fast_maxpbound->value < 1)
        gi.cvar_set("fast_maxpbound", "1");

    armor_jacket = gi.cvar("armor_jacket", "25 50 0.30 0.00", 0);
    armor_combat = gi.cvar("armor_combat", "50 100 0.60 0.30", 0);
    armor_body = gi.cvar("armor_body", "100 200 0.80 0.60", 0);
    armor_shard = gi.cvar("armor_shard", "2", 0);
    OSP_parseArmor();

    power_armor_screen = gi.cvar("power_armor_screen", "1.0", 0);
    power_armor_shield = gi.cvar("power_armor_shield", "2.0", 0);

    warmup_health = gi.cvar("warmup_health", "150", 0);
    warmup_armor = gi.cvar("warmup_armor", "200", 0);
    if ((int)warmup_health->value < 100)
        gi.cvar_set("warmup_health", "100");
    if ((int)warmup_armor->value < 0)
        gi.cvar_set("warmup_armor", "0");

    camera_depth = gi.cvar("camera_depth", "60.0", 0);
    camera_pitch = gi.cvar("camera_pitch", "15.0", 0);

    flood_msgs = gi.cvar("flood_msgs", "4", 0);
    flood_persecond = gi.cvar("flood_persecond", "4", 0);
    flood_waitdelay = gi.cvar("flood_waitdelay", "10", 0);

    hook_enable = gi.cvar("hook_enable", "0", 0);
    hook_color = gi.cvar("hook_color", "0xd1d1d1d1", 0);
    hook_speed = gi.cvar("hook_speed", "1600", 0);
    hook_pullspeed = gi.cvar("hook_pullspeed", "1000", 0);
    hook_initdamage = gi.cvar("hook_initdamage", "20", 0);
    hook_incdamage = gi.cvar("hook_incdamage", "1", 0);
    hook_maxdamage = gi.cvar("hook_maxdamage", "30", 0);
    hook_holdtime = gi.cvar("hook_holdtime", "7.5", 0);
    hook_holdplayertime = gi.cvar("hook_holdplayertime", "5.0", 0);
    hook_sky = gi.cvar("hook_sky", "0", 0);
    hook_wait = gi.cvar("hook_wait", "0.5", 0);

    // registers statsfile/statsname/stats_logchat/stats_logallpickups and
    // opens the log, so nothing may log before this point
    OSP_Stats_Init();

    qualifier_forceskins = gi.cvar("qualifier_forceskins", "0", 0);
    qualifier_skinname = gi.cvar("qualifier_skinname", "male/grunt", 0);
    qualifier_numspots = gi.cvar("qualifier_numspots", "0", 0);

    referee_enable = gi.cvar("referee_enable", "0", 0);
    referee_password = gi.cvar("referee_password", NULL, 0);

    runes_enable = gi.cvar("runes_enable", "0", 0);
    runes_min = gi.cvar("runes_min", "3", 0);
    runes_max = gi.cvar("runes_max", "12", 0);
    runes_perplayer = gi.cvar("runes_perplayer", "0.6", 0);
    runes_flash = gi.cvar("runes_flash", "1", 0);
    runes_resist = gi.cvar("runes_resist", "2.0", 0);
    runes_strength = gi.cvar("runes_strength", "2.0", 0);
    runes_regen_hmax = gi.cvar("runes_regen_hmax", "200", 0);
    runes_regen_amax = gi.cvar("runes_regen_amax", "100", 0);
    runes_vampire = gi.cvar("runes_vampire", "0.5", 0);
    runes_vampire_max = gi.cvar("runes_vampire_max", "200", 0);
    runes_model = gi.cvar("runes_model", "models/items/c_head/tris.md2", 0);

    rune_stat = (int)runes_enable->value;
    if (rune_stat > 0x1f)
        rune_stat = 0x1f;
    if ((int)runes_min->value > (int)runes_max->value)
        gi.cvar_set("runes_max", runes_min->string);

    if (m_mode < 2)
        vote_countspectators = gi.cvar("vote_countspectators", "1", 0);
    else
        vote_countspectators = gi.cvar("vote_countspectators", "0", 0);
    vote_enable = gi.cvar("vote_enable", "1", 0);
    vote_enable_map = gi.cvar("vote_enable_map", "1", 0);
    vote_enable_config = gi.cvar("vote_enable_config", "0", 0);
    vote_enable_time = gi.cvar("vote_enable_time", "1", 0);
    vote_enable_frag = gi.cvar("vote_enable_frag", "1", 0);
    vote_enable_hook = gi.cvar("vote_enable_hook", "1", 0);
    vote_enable_runes = gi.cvar("vote_enable_runes", "0", 0);
    vote_enable_toggles = gi.cvar("vote_enable_toggles", "1", 0);
    vote_enable_kick = gi.cvar("vote_enable_kick", "1", 0);
    vote_enable_bots = gi.cvar("vote_enable_bots", "0", 0);
    vote_bots_max = gi.cvar("vote_bots_max", "8", 0);
    vote_time = gi.cvar("vote_time", "45", 0);
    vote_threshold = gi.cvar("vote_threshold", "51", 0);
    vote_carryover = gi.cvar("vote_carryover", "1", 0);
    vote_config_default = gi.cvar("vote_config_default", "0", 0);
    vote_config_defaultname = gi.cvar("vote_config_defaultname", "default", 0);
    __current_config = gi.cvar("__current_config", "default", 0);
    gi.cvar_set("__current_config", "default");
    if (!(int)vote_enable_config->value) {
        gi.cvar_set("vote_config_default", "0");
        gi.cvar_set("vote_config_defaultname", "default");
    }

    menu_maxtime = gi.cvar("menu_maxtime", "120", 0);
    menu_timestep = gi.cvar("menu_timestep", "5", 0);
    menu_maxfrag = gi.cvar("menu_maxfrag", "100", 0);
    menu_fragstep = gi.cvar("menu_fragstep", "5", 0);

    demo_referee = gi.cvar("demo_referee", "0", 0);
    demo_player = gi.cvar("demo_player", "0", 0);
    demo_tag = gi.cvar("demo_tag", "tourney_tag", 0);

    team_duelrecover = gi.cvar("team_duelrecover", "0", 0);
    team_hurtteam = gi.cvar("team_hurtteam", "1", 0);
    team_hurtself = gi.cvar("team_hurtself", "1", 0);
    team_idteam = gi.cvar("team_idteam", "1", 0);
    team_lockskin = gi.cvar("team_lockskin", "0", 0);
    if (m_mode == 3)
        team_maxplayers = gi.cvar("team_maxplayers", "1", 0);
    else
        team_maxplayers = gi.cvar("team_maxplayers", "4", 0);
    team_nextuptime = gi.cvar("team_nextuptime", "45", 0);
    team_overtime_mode = gi.cvar("team_overtime_mode", "1", 0);
    team_overtime_time = gi.cvar("team_overtime_time", "1", 0);
    team_overtime_count = gi.cvar("team_overtime_count", "1", 0);
    team_recovertime = gi.cvar("team_recovertime", "0.0", 0);
    reconn_player[0] = 0;
    reconn_index = 2;

    client_botdetect = gi.cvar("client_botdetect", "1", 0);
    bot_watch = (int)client_botdetect->value;
    client_deathweapdrop = gi.cvar("client_deathweapdrop", "1", 0);
    client_fastweap = gi.cvar("client_fastweap", "0", 0);
    client_highscores = gi.cvar("client_highscores", "1", 0);
    client_hud = gi.cvar("client_hud", "0", 0);
    client_infochange = gi.cvar("client_infochange", "4", 0);
    client_maxfps = gi.cvar("client_maxfps", "0", 0);
    if ((int)client_maxfps->value)
        client_maxframes = (int)(1000.0f / client_maxfps->value);
    client_minping = gi.cvar("client_minping", "0", 0);
    client_maxping = gi.cvar("client_maxping", "0", 0);
    client_maxrate = gi.cvar("client_maxrate", "0", 0);
    client_muzzlemode = gi.cvar("client_muzzlemode", "0", 0);
    client_protect = gi.cvar("client_protect", "0", 0);
    if (m_mode)
        gi.cvar_set("client_protect", "0");
    client_recover = gi.cvar("client_recover", "0", 0);
    client_nomove = gi.cvar("client_nomove", "90", 0);

    // The two server-browser score cells: only team play publishes numbers.
    if (m_mode == 2) {
        team_a_score = gi.cvar("Score_A", "Disabled", CVAR_SERVERINFO);
        team_b_score = gi.cvar("Score_B", "Disabled", CVAR_SERVERINFO);
    } else {
        team_a_score = gi.cvar("Score_A", "", 0);
        team_b_score = gi.cvar("Score_B", "", 0);
        gi.cvar_set("Score_A", "");
        gi.cvar_set("Score_B", "");
    }

    OSP_initWeapItem();

    // The team names, skins and hook colours are read once per server run, not
    // once per map -- game_init is the latch.
    if (!game_init) {
        game_init = 1;

        team_a_hookcolor = gi.cvar("team_a_hookcolor", "0xf2f2f2f2", 0);
        Q_strlcpy((char *)osp_teams[0].osp_m0c0, team_a_hookcolor->string,
                  sizeof(osp_teams[0].osp_m0c0));
        team_a_skin = gi.cvar("team_a_skin", "female/athena", 0);
        Q_strlcpy(osp_teams[0].skin, team_a_skin->string, sizeof(osp_teams[0].skin));
        team_a_name = gi.cvar("team_a_name", "Hometeam", 0);
        // 16, not sizeof: a team name is drawn in a 15-column status bar cell
        Q_strlcpy(osp_teams[0].netname, team_a_name->string, 16);
        Q_strlcpy(osp_teams[0].greenname, team_a_name->string, 16);
        for (i = 0; i < strlen(osp_teams[0].greenname); i++)
            osp_teams[0].greenname[i] += 128;

        team_b_hookcolor = gi.cvar("team_b_hookcolor", "0xd1d1d1d1", 0);
        Q_strlcpy((char *)osp_teams[1].osp_m0c0, team_b_hookcolor->string,
                  sizeof(osp_teams[1].osp_m0c0));
        team_b_skin = gi.cvar("team_b_skin", "male/sniper", 0);
        Q_strlcpy(osp_teams[1].skin, team_b_skin->string, sizeof(osp_teams[1].skin));
        team_b_name = gi.cvar("team_b_name", "Visitors", 0);
        Q_strlcpy(osp_teams[1].netname, team_b_name->string, 16);
        Q_strlcpy(osp_teams[1].greenname, team_b_name->string, 16);
        for (i = 0; i < strlen(osp_teams[1].greenname); i++)
            osp_teams[1].greenname[i] += 128;
    }

    // 1v1 is two osp_teams of one, whatever the server asked for.
    if (m_mode > 1) {
        if (m_mode == 3) {
            gi.cvar_set("team_maxplayers", "1");
            gi.dprintf("1V1 Mode: setting osp_teams' maxplayers to 1.\n");
            team_maxplayers = gi.cvar("team_maxplayers", "1", CVAR_NOSET);
        }

        if ((int)team_maxplayers->value * 2 > (int)game.maxclients) {
            Q_snprintf(buf, sizeof(buf), "%d", (int)game.maxclients / 2);
            gi.cvar_set("team_maxplayers", buf);
            gi.dprintf("team_maxplayers too high!\nSetting maxplayers to: %s\n",
                       buf);
        }

        for (i = 0; i < 2; i++) {
            osp_teams[i].osp_m11c = (int)team_hurtteam->value;
            osp_teams[i].osp_m120 = (int)team_hurtself->value;
            osp_teams[i].osp_m124 = 0;
            osp_teams[i].joincode[0] = 0;
        }

        gi.dprintf("Team A name: %s\n", osp_teams[0].netname);
        gi.dprintf("Team B name: %s\n", osp_teams[1].netname);

        if (m_mode == 2) {
            gi.cvar_set("Score_A", "WARMUP");
            gi.cvar_set("Score_B", "WARMUP");
        }

        if ((int)team_overtime_mode->value > 1 &&
            (int)team_overtime_time->value < 1)
            gi.cvar_set("team_overtime_time", "1");

        if ((int)team_overtime_mode->value == 0)
            gi.dprintf("Overtime mode: NONE (match can end in a tie).\n");
        else if ((int)team_overtime_mode->value == 1)
            gi.dprintf("Overtime mode: Sudden Death (first death decides).\n");
        else if ((int)team_overtime_mode->value == 2) {
            if ((int)team_overtime_time->value == 1)
                gi.dprintf("Overtime mode: Timed round (1 minute) [until winner].\n");
            else
                gi.dprintf("Overtime mode: Timed round (%d minutes) [until winner].\n",
                           (int)team_overtime_time->value);
        } else {
            if ((int)team_overtime_count->value < 1)
                gi.cvar_set("team_overtime_count", "1");
            if ((int)team_overtime_time->value == 1)
                gi.dprintf("Overtime mode: %d timed rounds (1 minute), before sudden death.\n",
                           (int)team_overtime_count->value);
            else
                gi.dprintf("Overtime mode: %d timed rounds (%d minutes), before sudden death.\n",
                           (int)team_overtime_count->value,
                           (int)team_overtime_time->value);
        }
    }

    // The per-map high score table needs a limit to measure against.
    if (m_mode <= 1 && (int)client_highscores->value &&
        !(int)timelimit->value && !(int)fraglimit->value) {
        gi.dprintf("High score tracking disabled!\n");
        gi.cvar_set("client_highscores", "0");
    } else
        gi.dprintf("Client high scoring enabled!\n");

    if (match_pausetime->value > 99.0f)
        gi.cvar_set("match_pausetime", "99.0");
    if (match_pausetime->value < 10.0f)
        gi.cvar_set("match_pausetime", "10.0");
    if ((int)match_timeouts->value > 20)
        gi.cvar_set("match_timeouts", "20");
    if (camera_depth->value < 0.0f)
        gi.cvar_set("camera_depth", "0.0");
    if (camera_pitch->value > 45.0f)
        gi.cvar_set("camera_pitch", "45.0");
    if (camera_pitch->value < 0.0f)
        gi.cvar_set("camera_pitch", "0.0");
    if ((int)damage_railgun->value < 1)
        gi.cvar_set("damage_railgun", "1");
    if ((int)match_readypercent->value < 1)
        gi.cvar_set("match_readypercent", "1");
    if ((int)match_readypercent->value > 100)
        gi.cvar_set("match_readypercent", "100");
    if ((int)qualifier_numspots->value < 0)
        gi.cvar_set("qualifier_numspots", "0");

    if (!m_mode) {
        sync_stat = 8;
        sync_frame = 0;
        gi.cvar_set("qualifier_numspots", "0");
        gi.cvar_set("match_type", "RegularDM");
        gi.dprintf("Mode: *** REGULAR DEATHMATCH ***\n");
    } else if (m_mode == 1) {
        sync_stat = 0;
        if (!(int)qualifier_numspots->value)
            gi.cvar_set("qualifier_numspots", "1");
        gi.cvar_set("match_type", "QualifierDM");
        gi.dprintf("Mode: *** DM QUALIFIER ***\n");
        gi.dprintf("Number of qualifiers per match: %d\n",
                   (int)qualifier_numspots->value);
    } else if (m_mode == 2) {
        sync_stat = 0;
        gi.cvar_set("match_type", "TeamPlay");
        gi.dprintf("Mode: *** DM TEAM-PLAY MODE ***\n");
    } else {
        sync_stat = 0;
        gi.cvar_set("match_type", "1-vs-1");
        gi.dprintf("Mode: *** DM 1V1 MODE ***\n");
    }

    if ((int)map_halt->value) {
        gi.cvar_set("nextlevel_click", "0");
        gi.cvar_set("nextlevel_default", "0");
        gi.dprintf("Game will halt at end of level!\n");
    }

    if ((int)vote_enable->value) {
        gi.dprintf("\nClient voting enabled!\n");
        if ((int)vote_time->value < 30)
            gi.cvar_set("vote_time", "30");
        if ((int)vote_threshold->value < 10)
            gi.cvar_set("vote_threshold", "10");
        gi.dprintf("Proposal time: %ds, Threshold: %d%%\n\n",
                   (int)vote_time->value, (int)vote_threshold->value);

        if ((int)menu_maxtime->value < 0)
            gi.cvar_set("menu_maxtime", "0");
        else if ((int)menu_maxtime->value > 960)
            gi.cvar_set("menu_maxtime", "960");
        if ((int)menu_timestep->value < 1)
            gi.cvar_set("menu_timestep", "1");
        else if ((int)menu_timestep->value > (int)menu_maxtime->value)
            gi.cvar_set("menu_timestep", menu_maxtime->string);

        if ((int)menu_maxfrag->value < 0)
            gi.cvar_set("menu_maxfrag", "0");
        else if ((int)menu_maxfrag->value > 999)
            gi.cvar_set("menu_maxfrag", "999");
        if ((int)menu_fragstep->value < 1)
            gi.cvar_set("menu_fragstep", "1");
        else if ((int)menu_fragstep->value > (int)menu_maxfrag->value)
            gi.cvar_set("menu_fragstep", menu_maxfrag->string);
    } else
        gi.dprintf("\nClient voting DISABLED!\n\n");

    Q_strlcpy(default_timelimit, timelimit->string, sizeof(default_timelimit));
    Q_strlcpy(default_fraglimit, fraglimit->string, sizeof(default_fraglimit));
    Q_strlcpy(default_hook, hook_enable->string, sizeof(default_hook));

    item_settings = OSP_checkItems();

    for (i = 0; i < 256; i++) {
        p_acc[i].netname[0] = 0;
        o_acc[i].netname[0] = 0;
    }

    OSP_playerlist_svcmd();

    if ((int)vote_enable_config->value)
        OSP_configLoad();

    old_scores[0] = 0;
    server_log = NULL;
    OSP_setupAdminLog();

    p_order[25] = 0;
    p_order[26] = 0;
    p_order[27] = 0;
    botglobals.numbots = 0;
    bots_votedin = 0;
    bots_delaytime = (int)bots_delayload->value * 125 + 15;
    bots_loadstat = (int)bots_autoload->value;

    if (m_mode == 3 && (int)bots_autoload->value == 2 &&
        (int)bots_minplayers->value >= 1)
        gi.cvar_set("bots_minplayers", "0");

    OSP_setFeatures();
    level_start = level.framenum;

    gi.dprintf("%s\n", "OSP Tourney DM v(2.75)");
    gi.dprintf("%s\n", "29 Mar 00");
    gi.dprintf("%s\n", "rhea@OrangeSmoothie.org");
}

void OSP_endClean(void)
{
    edict_t     *ent;
    int         i;
    int         clientid;

    if (m_mode)
        sync_stat = 0;
    else
        sync_stat = 8;

    sync_time = 0;
    sync_frame = 0;
    start_count = 0;
    active_clients = 0;
    connected_clients = 0;

    if (!(int)vote_carryover->value && manual_map != 2) {
        gi.cvar_set("timelimit", default_timelimit);
        gi.cvar_set("fraglimit", default_fraglimit);
        gi.cvar_set("hook_enable", default_hook);
        rune_stat = (int)runes_enable->value;
    }

    time_update = 0;
    time_blink = 0;
    blink_on_count = 9;
    blink_off_count = 0;
    who_paused = -1;
    level_start = level.framenum + 10;
    frag_offset = 0;
    overtime_timer = 0;
    start_suddendeath = 0;
    vote_inprogress = 0;
    vote_frametime = 0;
    vote_item = 0;
    vote_yea = 0;
    vote_nay = 0;
    manual_map = 0;
    maxconn_clients = 0;
    OSP_clearClients();
    reconn_player[0] = 0;
    reconn_index = 2;

    // Snapshot each player's accuracy for the end-of-match report before the
    // live table is reused.
    for (i = 1; i <= game.maxclients; i++) {
        ent = g_edicts + i;
        if (ent->inuse && ent->client) {
            if (ent->client->resp.osp_entered != ENTERED_ENTERED)
                continue;

            clientid = ent->client->resp.clientid;
            if (clientid < 0 || clientid >= q_countof(p_acc))
                continue;

            memcpy(&o_acc[clientid], &p_acc[clientid], sizeof(p_acc_t));
        }
    }

    p_order[26] = 0;
    p_order[27] = 0;
    bots_votedin = 0;
    old_botcount = -1;

    if (m_mode > 1)
        OSP_teamReset();
}

// Register the loadout cvars and fold them into the four int tables the rest
// of the mod reads: initial_weap (the weapon a player spawns holding),
// start_weap (a have/have-not flag per weapon, slot 0 always 1 for the
// blaster), start_items, max_items and pack_items.  weapon_initial is a
// bitmask where the HIGHEST set bit wins, which is why the ten tests are
// separate ifs rather than an else chain.
void OSP_initWeapItem(void)
{
    int     initial;
    unsigned int    have;

    weapon_initial = gi.cvar("weapon_initial", "0", 0);
    weapon_have = gi.cvar("weapon_have", "0", 0);
    start_shells = gi.cvar("start_shells", "0", 0);
    start_bullets = gi.cvar("start_bullets", "0", 0);
    start_cells = gi.cvar("start_cells", "0", 0);
    start_grenades = gi.cvar("start_grenades", "0", 0);
    start_rockets = gi.cvar("start_rockets", "0", 0);
    start_slugs = gi.cvar("start_slugs", "0", 0);
    start_health = gi.cvar("start_health", "100", 0);
    start_armor = gi.cvar("start_armor", "0", 0);
    start_armortype = gi.cvar("start_armortype", "0", 0);
    max_shells = gi.cvar("max_shells", "100", 0);
    max_bullets = gi.cvar("max_bullets", "200", 0);
    max_cells = gi.cvar("max_cells", "200", 0);
    max_grenades = gi.cvar("max_grenades", "50", 0);
    max_rockets = gi.cvar("max_rockets", "50", 0);
    max_slugs = gi.cvar("max_slugs", "50", 0);
    max_armor = gi.cvar("max_armor", "200", 0);
    max_health = gi.cvar("max_health", "100", 0);
    pack_shells = gi.cvar("pack_shells", "200", 0);
    pack_bullets = gi.cvar("pack_bullets", "300", 0);
    pack_cells = gi.cvar("pack_cells", "300", 0);
    pack_grenades = gi.cvar("pack_grenades", "100", 0);
    pack_rockets = gi.cvar("pack_rockets", "100", 0);
    pack_slugs = gi.cvar("pack_slugs", "100", 0);
    pack_armor = gi.cvar("pack_armor", "250", 0);
    pack_health = gi.cvar("pack_health", "100", 0);

    initial = (int)weapon_initial->value;
    have = (int)weapon_have->value;

    initial_weap = 7;
    if (initial & 0x1)
        initial_weap = 8;
    if (initial & 0x2)
        initial_weap = 9;
    if (initial & 0x4)
        initial_weap = 10;
    if (initial & 0x8)
        initial_weap = 11;
    if (initial & 0x10)
        initial_weap = 12;
    if (initial & 0x20)
        initial_weap = 13;
    if (initial & 0x40)
        initial_weap = 14;
    if (initial & 0x80)
        initial_weap = 15;
    if (initial & 0x100)
        initial_weap = 16;
    if (initial & 0x200)
        initial_weap = 17;

    start_weap[0] = 1;
    start_weap[1] = (have & 0x1) != 0;
    start_weap[2] = (have & 0x2) != 0;
    start_weap[3] = (have & 0x4) != 0;
    start_weap[4] = (have & 0x8) != 0;
    start_weap[5] = (have & 0x10) != 0;
    start_weap[6] = (have & 0x20) != 0;
    start_weap[7] = (have & 0x40) != 0;
    start_weap[8] = (have & 0x80) != 0;
    start_weap[9] = (have & 0x100) != 0;
    start_weap[10] = (have & 0x200) != 0;

    start_items[0] = (int)start_shells->value;
    start_items[1] = (int)start_bullets->value;
    start_items[2] = (int)start_cells->value;
    start_items[3] = (int)start_grenades->value;
    start_items[4] = (int)start_rockets->value;
    start_items[5] = (int)start_slugs->value;
    start_items[7] = (int)start_health->value;
    start_items[10] = 0;
    start_items[9] = 0;
    start_items[8] = 0;

    if ((int)start_armortype->value == 2)
        start_items[10] = (int)start_armor->value;
    else if ((int)start_armortype->value == 1)
        start_items[9] = (int)start_armor->value;
    else
        start_items[8] = (int)start_armor->value;

    max_items[0] = (int)max_shells->value;
    max_items[1] = (int)max_bullets->value;
    max_items[2] = (int)max_cells->value;
    max_items[3] = (int)max_grenades->value;
    max_items[4] = (int)max_rockets->value;
    max_items[5] = (int)max_slugs->value;
    max_items[6] = (int)max_armor->value;
    max_items[7] = (int)max_health->value;

    pack_items[0] = (int)pack_shells->value;
    pack_items[1] = (int)pack_bullets->value;
    pack_items[2] = (int)pack_cells->value;
    pack_items[3] = (int)pack_grenades->value;
    pack_items[4] = (int)pack_rockets->value;
    pack_items[5] = (int)pack_slugs->value;
    pack_items[6] = (int)pack_armor->value;
    pack_items[7] = (int)pack_health->value;
}

// The warmup loadout: every weapon except the BFG, full ammo, body armour to
// taste and the railgun in hand.
void OSP_seedPlayer(gclient_t *client)
{
    edict_t     *p;
    int         t;
    int         ready;

    if (sync_stat <= 1) {
        if (sync_stat == 1) {
            client->pers.inventory[initial_weap] = 0;
            client->pers.selected_item = 7;
            client->pers.weapon = &itemlist[7];
            return;
        }

        ready = 0;
        for (t = 1; t <= game.maxclients; t++) {
            p = g_edicts + t;
            if (!p->inuse || !p->client ||
                p->client->resp.osp_entered != ENTERED_ENTERED)
                continue;
            if (!p->client->resp.osp_r20c)
                ready++;
        }

        if (ready <= active_clients *
            (100 - (int)match_prestartpercent->value) / 100) {
            client->pers.inventory[initial_weap] = 0;
            client->pers.selected_item = 7;
            client->pers.weapon = &itemlist[7];
            return;
        }
    }

    client->pers.inventory[7] = start_weap[0];
    client->pers.inventory[8] = start_weap[1];
    client->pers.inventory[9] = start_weap[2];
    client->pers.inventory[10] = start_weap[3];
    client->pers.inventory[11] = start_weap[4];
    client->pers.inventory[12] = start_weap[5];
    client->pers.inventory[13] = start_weap[6];
    client->pers.inventory[14] = start_weap[7];
    client->pers.inventory[15] = start_weap[8];
    client->pers.inventory[16] = start_weap[9];
    client->pers.inventory[17] = start_weap[10];

    client->pers.inventory[1] = start_items[10];
    client->pers.inventory[2] = start_items[9];
    client->pers.inventory[3] = start_items[8];
    client->pers.inventory[18] = start_items[0];
    client->pers.inventory[19] = start_items[1];
    client->pers.inventory[20] = start_items[2];
    client->pers.inventory[12] = start_items[3];
    client->pers.inventory[21] = start_items[4];
    client->pers.inventory[22] = start_items[5];

    client->pers.max_shells = max_items[0];
    client->pers.max_bullets = max_items[1];
    client->pers.max_cells = max_items[2];
    client->pers.max_grenades = max_items[3];
    client->pers.max_rockets = max_items[4];
    client->pers.max_slugs = max_items[5];

    client->pers.health = start_items[7];
    client->pers.max_health = max_items[7];

    client->pers.inventory[initial_weap] = 1;
    client->pers.selected_item = initial_weap;
    client->pers.weapon = &itemlist[initial_weap];

    // client_protect seconds of spawn protection, but only in plain DM, only
    // for a player actually in the game, and only if they spawn with the
    // blaster (item 7) -- i.e. not on a weapons-start server.
    if ((int)client_protect->value && client->resp.osp_entered == ENTERED_ENTERED &&
        m_mode == 0 && initial_weap == 7)
        client->resp.osp_r23c = level.framenum +
                                (int)client_protect->value * 10;
    else
        client->resp.osp_r23c = 0;
}

void OSP_packPlayer(edict_t *ent)
{
    ent->client->pers.max_shells = pack_items[0];
    ent->client->pers.max_bullets = pack_items[1];
    ent->client->pers.max_cells = pack_items[2];
    ent->client->pers.max_grenades = pack_items[3];
    ent->client->pers.max_rockets = pack_items[4];
    ent->client->pers.max_slugs = pack_items[5];
    ent->client->pers.max_health = pack_items[7];
}

// Append a short tag for every allow_* cvar that is switched off to whatever
// the caller has already built in buf.  The length is taken before the run and
// compared after, so " NONE" goes on only when nothing was appended.
// Appends to whatever the caller has already built.  All three callers pass a
// 1024-byte layout scratch buffer, which is what OSP_DISABLED_ITEMS_MAX below
// is measured against: the whole run is under 60 characters.
#define OSP_DISABLED_ITEMS_MAX  1024

void OSP_listDisabledItems(char *buf)
{
    int     len;
    cvar_t  *shotgun;
    cvar_t  *supershotgun;
    cvar_t  *machinegun;
    cvar_t  *chaingun;
    cvar_t  *grenadelauncher;
    cvar_t  *rocketlauncher;
    cvar_t  *hyperblaster;
    cvar_t  *railgun;
    cvar_t  *bfg;
    cvar_t  *grenades;
    cvar_t  *powerscreen;
    cvar_t  *powershield;
    cvar_t  *quad;
    cvar_t  *invul;

    shotgun = gi.cvar("allow_shotgun", "1", 0);
    supershotgun = gi.cvar("allow_supershotgun", "1", 0);
    machinegun = gi.cvar("allow_machinegun", "1", 0);
    chaingun = gi.cvar("allow_chaingun", "1", 0);
    grenadelauncher = gi.cvar("allow_grenadelauncher", "1", 0);
    rocketlauncher = gi.cvar("allow_rocketlauncher", "1", 0);
    hyperblaster = gi.cvar("allow_hyperblaster", "1", 0);
    railgun = gi.cvar("allow_railgun", "1", 0);
    bfg = gi.cvar("allow_bfg", "1", 0);
    grenades = gi.cvar("allow_ammo_grenades", "1", 0);
    powerscreen = gi.cvar("allow_item_powerscreen", "1", 0);
    powershield = gi.cvar("allow_item_powershield", "1", 0);
    quad = gi.cvar("allow_item_quad", "1", 0);
    invul = gi.cvar("allow_item_invul", "1", 0);

    len = strlen(buf);

    if (!(int)shotgun->value)
        Q_strlcat(buf, " S", OSP_DISABLED_ITEMS_MAX);
    if (!(int)supershotgun->value)
        Q_strlcat(buf, " SS", OSP_DISABLED_ITEMS_MAX);
    if (!(int)machinegun->value)
        Q_strlcat(buf, " MG", OSP_DISABLED_ITEMS_MAX);
    if (!(int)chaingun->value)
        Q_strlcat(buf, " CG", OSP_DISABLED_ITEMS_MAX);
    if (!(int)grenadelauncher->value)
        Q_strlcat(buf, " GL", OSP_DISABLED_ITEMS_MAX);
    if (!(int)rocketlauncher->value)
        Q_strlcat(buf, " RL", OSP_DISABLED_ITEMS_MAX);
    if (!(int)hyperblaster->value)
        Q_strlcat(buf, " HB", OSP_DISABLED_ITEMS_MAX);
    if (!(int)railgun->value)
        Q_strlcat(buf, " RG", OSP_DISABLED_ITEMS_MAX);
    if (!(int)bfg->value)
        Q_strlcat(buf, " BFG", OSP_DISABLED_ITEMS_MAX);
    if (!(int)grenades->value)
        Q_strlcat(buf, " G", OSP_DISABLED_ITEMS_MAX);
    if (!(int)powerscreen->value)
        Q_strlcat(buf, " P.Screen", OSP_DISABLED_ITEMS_MAX);
    if (!(int)powershield->value)
        Q_strlcat(buf, " P.Shield", OSP_DISABLED_ITEMS_MAX);
    if (!(int)quad->value)
        Q_strlcat(buf, " Quad", OSP_DISABLED_ITEMS_MAX);
    if (!(int)invul->value)
        Q_strlcat(buf, " Invul", OSP_DISABLED_ITEMS_MAX);

    if (len == strlen(buf))
        Q_strlcat(buf, " NONE", OSP_DISABLED_ITEMS_MAX);
}

void OSP_clientConfigString(edict_t *ent, short index, const char *string)
{
    if (!(ent->flags & FL_BOT)) {
        gi.WriteByte(svc_configstring);
        gi.WriteShort(index);
        gi.WriteString(string);
        gi.unicast(ent, true);
    }
}

void OSP_clearStats(edict_t *ent)
{
    if (m_mode < 2) {
        G_SetStat(ent, SID_OSP_STATUS1, 0);
        G_SetStat(ent, SID_OSP_STATUS2, 0);
        G_SetStat(ent, SID_OSP_STATUS3, 0);
    } else {
        G_SetStat(ent, SID_OSP_STATUS1, 0);
        G_SetStat(ent, SID_OSP_STATUS2, 0);
        G_SetStat(ent, SID_OSP_STATUS3, 0);
        G_SetStat(ent, SID_OSP_STATUS4, 0);
        G_SetStat(ent, SID_OSP_MATCHSTATE, 0);
        G_SetStat(ent, SID_CHASE, 0);
    }
}

void OSP_restartStats(edict_t *ent)
{
    if (sync_stat > 0)
        G_SetStat(ent, SID_OSP_MATCHSTATE, OSP_CS(1));
    else
        G_SetStat(ent, SID_OSP_MATCHSTATE, 0);

    if (m_mode < 2) {
        G_SetStat(ent, SID_OSP_STATUS1, OSP_CS(3));
        G_SetStat(ent, SID_OSP_STATUS2, OSP_CS(2));
        if (sync_stat < 4 || ent->client->resp.osp_entered == 2)
            G_SetStat(ent, SID_OSP_STATUS3, 0);
        else
            G_SetStat(ent, SID_OSP_STATUS3, OSP_CS(4));
    } else {
        G_SetStat(ent, SID_OSP_STATUS1, OSP_CS(5));
        G_SetStat(ent, SID_OSP_STATUS2, OSP_CS(6));
        G_SetStat(ent, SID_OSP_STATUS3, OSP_CS(7));
        G_SetStat(ent, SID_OSP_STATUS4, OSP_CS(8));
    }
}

// Rank every player in the game by score and stamp each one's place into
// resp.osp_r208, plus the score they are chasing into resp.osp_r0a8.  An
// insertion sort into two parallel arrays; the two tie-breaks after score are
// resp.osp_r014 and resp.osp_r2c0, both LOWER-is-better.
void OSP_setStats(edict_t *ent)
{
    char        num[16];
    char        buf[16];
    gclient_t   *cl;

    cl = ent->client;

    if ((level.framenum - cl->resp.osp_r0ac) / 10 > 9.5f &&
        cl->menu_owner != MENU_TOURNEY) {
        if (cl->resp.osp_r24c != 0 && cl->resp.osp_r24c != 1 &&
            cl->resp.osp_r24c != 8) {
            cl->resp.osp_r24c = 0;
            if (cl->showscores == 1)
                cl->showscores = 0;
        }
    } else if (!cl->resp.osp_r24c && !cl->showscores &&
               cl->menu_owner != MENU_TOURNEY) {
        // Page 2 is the MOTD, and this is the ten-second window a connecting
        // player is shown it in: re-armed on every frame the player has
        // nothing up, which is what makes the board refuse to close until the
        // timer above expires.  The donor's, and deliberate.
        //
        // THROUGH THE GATE, and the donor's own line is why.  The donor writes
        // `DeathmatchScoreboardMessage(ent, ent->enemy)` here, and under that
        // name it keeps its OWN five-page dispatcher: OSP deleted baseq2's
        // board and put the `osp_r24c == 2 -> OSP_showMOTD()` switch in its
        // place (`port_osp@205a89c` p_hud.c:140).  This tree keeps baseq2's
        // grid board under that name, so the call that means "draw the page I
        // have just selected" is the ScoreboardMessage row.  By name it would
        // draw a screen OSP does not have.
        cl->resp.osp_r24c = 2;
        cl->showscores = 1;
        G_ScoreboardMessage(ent, ent->enemy);
        gi.unicast(ent, false);
    }

    // The ID overlay is refreshed on every fourth frame.
    if (!(level.framenum & 3)) {
        if (cl->resp.osp_r204 || cl->resp.osp_entered == 2 ||
            (m_mode == 2 && (int)team_idteam->value)) {
            if (cl->resp.osp_entered != 16)
                G_SetStat(ent, SID_CHASE, OSP_setID(ent));
        }
    }

    if (!(level.framenum & 1) && sync_stat > 2) {
        OSP_checkAnnounce(ent);

        if (m_mode > 1)
            return;

        OSP_showFrags(ent);

        // The "rank/total" cell, sent only when either half has changed.
        if (cl->resp.osp_entered == ENTERED_ENTERED) {
            if (cl->resp.osp_r208 != cl->resp.osp_r09c ||
                cl->resp.osp_r090 != active_clients) {
                Q_snprintf(num, sizeof(num), "%i/%i", cl->resp.osp_r208,
                        active_clients);
                Q_snprintf(buf, sizeof(buf), "%5s", num);
                OSP_clientConfigString(ent, OSP_CS(4), buf);
                cl->resp.osp_r09c = cl->resp.osp_r208;
                cl->resp.osp_r090 = active_clients;
            }
        } else {
            if (cl->chase_target) {
                if (cl->chase_target->client->resp.osp_r208 !=
                    cl->resp.osp_r09c ||
                    cl->resp.osp_r090 != active_clients) {
                    Q_snprintf(num, sizeof(num), "%i/%i",
                               cl->chase_target->client->resp.osp_r208,
                               active_clients);
                    Q_snprintf(buf, sizeof(buf), "%5s", num);
                    OSP_clientConfigString(ent, OSP_CS(4), buf);
                    cl->resp.osp_r09c =
                        cl->chase_target->client->resp.osp_r208;
                    cl->resp.osp_r090 = active_clients;
                }
            }
        }
    }
}

void OSP_DoRankSort(void)
{
    int         idx[256];
    int         score[256];
    edict_t     *p;
    int         i;
    int         j;
    int         k;
    int         curscore;
    int         count;

    count = 0;
    if (sync_stat < 4) {
        gi.configstring(OSP_CS(2), " ");
        return;
    }

    for (i = 0; i < game.maxclients; i++) {
        p = g_edicts + i + 1;
        if (!p->inuse || !p->client ||
            p->client->resp.osp_entered != ENTERED_ENTERED)
            continue;

        curscore = game.clients[i].resp.score;

        for (j = 0; j < count; j++) {
            if (curscore > score[j])
                break;
            if (curscore == score[j]) {
                if (game.clients[i].resp.osp_r014 <
                    game.clients[idx[j]].resp.osp_r014)
                    break;
                if (game.clients[i].resp.osp_r014 ==
                    game.clients[idx[j]].resp.osp_r014) {
                    if (game.clients[i].resp.osp_r2c0 <
                        game.clients[idx[j]].resp.osp_r2c0)
                        break;
                }
            }
        }

        for (k = count; k > j; k--) {
            idx[k] = idx[k - 1];
            score[k] = score[k - 1];
        }
        idx[j] = i;
        score[j] = curscore;
        count++;
    }

    for (i = 0; i < count; i++) {
        p = g_edicts + 1 + idx[i];
        p->client->resp.osp_r208 = i + 1;

        if (m_mode == 1) {
            // Qualifier mode: everyone chases the score of the last player still
            // inside the qualifying places.
            if ((int)qualifier_numspots->value >= 1 &&
                count > (int)qualifier_numspots->value) {
                if ((int)qualifier_numspots->value >= i + 1)
                    p->client->resp.osp_r0a8 =
                        score[(int)qualifier_numspots->value];
                else
                    p->client->resp.osp_r0a8 =
                        score[(int)qualifier_numspots->value - 1];
            } else
                p->client->resp.osp_r0a8 = 0;
        } else if (m_mode == 0) {
            // Plain DM: the leader chases second place, everybody else the
            // leader.
            if (!i) {
                if (active_clients > 1)
                    p->client->resp.osp_r0a8 = score[1];
                else
                    p->client->resp.osp_r0a8 = 0;
            } else
                p->client->resp.osp_r0a8 = score[0];
        }
    }
}

// The two status-bar cells above the scoreboard: the score/fraglimit cell
// (configstring OSP_CS(3)) and the "+n ahead / -n behind" cell (OSP_CS(2)).  A client
// in chasecam sees the target's numbers, not its own, which is what the
// separate `me` and `show` respawn pointers are for.  Both cells are only sent
// when something in them has actually changed.
void OSP_showFrags(edict_t *ent)
{
    char                scratch[32];
    char                line[32];
    client_respawn_t    *show;
    client_respawn_t    *mine;
    int                 i;

    mine = &ent->client->resp;

    if (mine->osp_entered == ENTERED_ENTERED || mine->osp_entered == 2 ||
        mine->osp_entered == 16)
        show = mine;
    else if (!ent->client->chase_target) {
        OSP_removeChaseCam(ent);
        show = mine;
    } else
        show = &ent->client->chase_target->client->resp;

    if (mine->osp_r0a0 != show->score ||
        mine->osp_r098 != (int)fraglimit->value) {
        if (mine->osp_entered == 2)
            Q_snprintf(scratch, sizeof(scratch), "%8s", "OBSERVE");
        else if (mine->osp_entered == 16)
            Q_snprintf(scratch, sizeof(scratch), "%8s", "AUTOCAM");
        else if (!(int)fraglimit->value)
            Q_snprintf(scratch, sizeof(scratch), "%8i", show->score);
        else {
            Q_snprintf(line, sizeof(line), "%i/%i", show->score,
                       (int)fraglimit->value);
            Q_snprintf(scratch, sizeof(scratch), "%8s", line);
        }

        OSP_clientConfigString(ent, OSP_CS(3), scratch);
        mine->osp_r098 = (int)fraglimit->value;
    }

    if (mine->osp_r0a0 != show->score || mine->osp_r094 != show->osp_r0a8 ||
        mine->osp_r090 != active_clients) {
        if (mine->osp_entered == 2 || mine->osp_entered == 16) {
            Q_strlcpy(scratch, " ", sizeof(scratch));
            mine->osp_r090 = active_clients;
        } else {
            if (show->osp_r208 == 1 ||
                (m_mode == 1 &&
                 show->osp_r208 <= (int)qualifier_numspots->value)) {
                Q_snprintf(line, sizeof(line), "+ %i", show->score - show->osp_r0a8);
                Q_snprintf(scratch, sizeof(scratch), "%8s", line);
            } else {
                // Behind: the whole cell goes green.
                Q_snprintf(line, sizeof(line), "- %i", show->osp_r0a8 - show->score);
                Q_snprintf(scratch, sizeof(scratch), "%8s", line);
                for (i = 0; i < strlen(scratch); i++)
                    scratch[i] += 128;
            }
        }

        OSP_clientConfigString(ent, OSP_CS(2), scratch);
        mine->osp_r094 = show->osp_r0a8;
    }

    mine->osp_r0a0 = show->score;
}

// The match clock.  Pushes "MM:SS" into configstring OSP_CS(1) and into the
// time_remaining serverinfo cvar once a second (time_update = framenum + 9);
// announces the 10 / 5 / 1 minute marks once each through the start_count
// bitmask, and blinks the last minute by writing the string in green -- every
// byte + 0x80 -- while blink_on_count is down to zero.  Before the match
// starts the same two cells count DOWN to sync_startframe instead.
void OSP_updateClock(void)
{
    char    buf[32];
    int     mins;
    int     seconds;
    int     i;

    if (frag_offset && !start_suddendeath) {
        start_suddendeath = 1;
        gi.configstring(OSP_CS(1), "DEATH");
        gi.cvar_set("time_remaining", "SuddenDeath");
        return;
    }

    if (start_suddendeath)
        return;

    if (((sync_stat == 4 && connected_clients) || sync_stat == 8) &&
        level.framenum > time_update) {
        if (timelimit->value != 0) {
            mins = (int)(timelimit->value + overtime_timer -
                         (level.framenum - sync_frame) / 600) - 1;
            seconds = (int)((overtime_timer + timelimit->value) * 60 -
                            (level.framenum - sync_frame) / 10) - mins * 60 - 1;

            if (seconds == 60) {
                seconds = 0;
                mins++;
            }
            if (mins < 0) {
                seconds = 0;
                mins = 0;
            }

            if (!mins && seconds > 0)
                time_blink = 1;
            else {
                time_blink = 0;
                blink_on_count = 9;
            }

            if (mins < 10 && !(start_count & 1) && timelimit->value > 10) {
                start_count |= 1;
                gi.bprintf(PRINT_HIGH, "10 minutes remaining in match.\n");
            }
            if (mins < 5 && !(start_count & 2) && timelimit->value > 5) {
                start_count |= 3;
                gi.bprintf(PRINT_HIGH, "5 minutes remaining in match.\n");
            }
            if (mins < 1) {
                if (!(start_count & 4) && timelimit->value >= 1) {
                    start_count |= 7;
                    gi.bprintf(PRINT_HIGH, "1 minute remaining in match.\n");
                }
                if (!mins && seconds <= 10 && !(start_count & 8))
                    start_count |= 0xf;
            }

            Q_snprintf(buf, sizeof(buf), "%2i:%.2i", mins, seconds);
            gi.cvar_set("time_remaining", buf);
            time_update = level.framenum + 9;

            if (!blink_on_count)
                for (i = 0; i < strlen(buf); i++)
                    buf[i] += 128;

            gi.configstring(OSP_CS(1), buf);

            if (time_blink) {
                if (!blink_on_count--)
                    blink_off_count = 9;
                else if (!blink_off_count--)
                    blink_on_count = 9;
                else
                    blink_on_count = 0;
            }
        } else {
            Q_strlcpy(buf, "  OFF", sizeof(buf));
            gi.cvar_set("time_remaining", "NoTimelimit");
            gi.configstring(OSP_CS(1), buf);
            time_blink = 0;
            time_update = level.framenum + 60;
        }
    } else if (sync_stat > 0 && connected_clients &&
               level.framenum > time_update) {
        int mins;
        int seconds;

        mins = (sync_startframe - level.framenum) / 600;
        seconds = (sync_startframe - level.framenum) / 10 - mins * 60 + 1;

        if (seconds == 60) {
            seconds = 0;
            mins++;
        }
        if (mins < 0) {
            seconds = 0;
            mins = 0;
        }

        Q_snprintf(buf, sizeof(buf), "%2i:%.2i", mins, seconds);
        gi.cvar_set("time_remaining", buf);
        time_update = level.framenum + 9;
        gi.configstring(OSP_CS(1), buf);
    }
}

// Defined after OSP_showScores, its only intra-TU caller, so that the call
// stays out of line.
void OSP_getDateInfo(char *out)
{
    time_t      t;
    struct tm   *tm;

    time(&t);
    tm = localtime(&t);
    if (tm)
        Q_snprintf(out, 32, "(%.19s)", asctime(tm));
    else
        Q_strlcpy(out, "()", 32);
}

// The match countdown beep.  resp.osp_r01c remembers the value of start_count
// this client was last told about, so each step is announced exactly once.
// Each of the four arms builds and unicasts its own stufftext.
void OSP_checkAnnounce(edict_t *ent)
{
    char    buf[32];

    if (start_count && start_count != ent->client->resp.osp_r01c &&
        !level.intermission_framenum) {
        if (!(ent->client->resp.osp_r01c & 1)) {
            Q_strlcpy(buf, "play misc/secret.wav", sizeof(buf));
            gi.WriteByte(svc_stufftext);
            gi.WriteString(buf);
            gi.unicast(ent, false);
        } else if (!(ent->client->resp.osp_r01c & 2)) {
            Q_strlcpy(buf, "play misc/secret.wav", sizeof(buf));
            gi.WriteByte(svc_stufftext);
            gi.WriteString(buf);
            gi.unicast(ent, false);
        } else if (!(ent->client->resp.osp_r01c & 4)) {
            Q_strlcpy(buf, "play misc/secret.wav", sizeof(buf));
            gi.WriteByte(svc_stufftext);
            gi.WriteString(buf);
            gi.unicast(ent, false);
        } else if (ent->client->resp.osp_r01c < start_count) {
            Q_strlcpy(buf, "play world/10_0.wav", sizeof(buf));
            gi.WriteByte(svc_stufftext);
            gi.WriteString(buf);
            gi.unicast(ent, false);
        }

        ent->client->resp.osp_r01c = start_count;
    }
}

// The player-ID overlay's line-of-sight test.  Same shape as loc_CanSee, but
// it bails on a MOVETYPE_PUSH target and traces from the *other* entity's
// eyes.
bool PlayerIdCanSee(edict_t *targ, edict_t *other)
{
    trace_t     trace;
    vec3_t      targpoint;
    vec3_t      viewpos;

    if (targ->movetype == MOVETYPE_PUSH)
        return false;

    VectorCopy(other->s.origin, targpoint);
    targpoint[2] += other->viewheight;
    VectorCopy(targ->s.origin, viewpos);
    viewpos[2] += targ->viewheight;

    trace = gi.trace(targpoint, vec3_origin, vec3_origin, viewpos, other,
                     MASK_SOLID);
    if (trace.fraction == 1.0f)
        return true;
    return false;
}

// The player-ID overlay.  Pick whichever in-play client is closest to the
// crosshair -- largest forward.dir, and at least 0.9, so roughly a 25 degree
// cone -- confirm line of sight, and push the name to configstring OSP_CS(0).
// resp.osp_r038 caches the last string sent so the unicast only goes out when
// it changes.  Returns the configstring index, or 0 when there is nobody to
// name, which is what OSP_setStats puts in SID_CHASE.
int OSP_setID(edict_t *ent)
{
    char        str[64];
    vec3_t      forward;
    vec3_t      dir;
    int         i;
    float       best = 0;
    edict_t     *bestent;
    edict_t     *cl;
    float       d;

    AngleVectors(ent->client->v_angle, forward, NULL, NULL);
    bestent = NULL;

    for (i = 1; i <= game.maxclients; i++) {
        cl = g_edicts + i;

        if (!cl->inuse || cl->client->resp.osp_r240 != 2 ||
            cl == ent->client->chase_target)
            continue;

        if ((cl->waterlevel >= 3 && ent->waterlevel < 3) ||
            (cl->waterlevel < 3 && ent->waterlevel >= 3))
            continue;

        VectorSubtract(cl->s.origin, ent->s.origin, dir);
        VectorNormalize(dir);
        d = DotProduct(forward, dir);

        if (d > best && d > 0.9f && PlayerIdCanSee(ent, cl)) {
            best = d;
            bestent = cl;
        }
    }

    if (best > 0.9f) {
        if (m_mode == 2 && (ent->client->resp.team == 2 ||
                            bestent->client->resp.team == ent->client->resp.team)) {
            Q_snprintf(str, sizeof(str), "Teammate \"%s\"\n",
                       bestent->client->pers.greenname);

            if (strcmp(ent->client->resp.osp_r038, str)) {
                Q_strlcpy(ent->client->resp.osp_r038, str,
                          sizeof(ent->client->resp.osp_r038));
                OSP_clientConfigString(ent, OSP_CS(0), str);
            }

            return OSP_CS(0);
        } else {
            if (ent->client->resp.osp_r204) {
                Q_snprintf(str, sizeof(str), "Viewing \"%s\"",
                           bestent->client->pers.netname);
                for (i = 0; i < strlen(str); i++)
                    str[i] += 128;

                if (strcmp(ent->client->resp.osp_r038, str)) {
                    Q_strlcpy(ent->client->resp.osp_r038, str,
                              sizeof(ent->client->resp.osp_r038));
                    OSP_clientConfigString(ent, OSP_CS(0), str);
                }

                return OSP_CS(0);
            }
        }
    }

    return 0;
}

// allow_id 2 and 3 are the server-locked off/on settings and simply force the
// client's flag; anything else toggles it.
bool OSP_changeID(edict_t *ent)
{
    if ((int)allow_id->value == 2) {
        gi.cprintf(ent, PRINT_HIGH, "ID tagging (OFF) cannot be changed.\n");
        ent->client->resp.osp_r204 = 0;
        return false;
    }

    if ((int)allow_id->value == 3) {
        gi.cprintf(ent, PRINT_HIGH, "ID tagging (ON) cannot be changed.\n");
        ent->client->resp.osp_r204 = 1;
        return true;
    }

    if ((ent->client->resp.osp_r204 =
             1 - ent->client->resp.osp_r204)) {
        gi.cprintf(ent, PRINT_HIGH, "Player ID tagging enabled.\n");
        return true;
    }

    gi.cprintf(ent, PRINT_HIGH, "Player ID tagging disabled.\n");
    return false;
}

// allow_id 0/1 pass straight through, 2 and 3 map onto 0 and 1, anything
// higher clamps to 1.
int OSP_initID(void)
{
    if ((int)allow_id->value <= 3 && (int)allow_id->value >= 0) {
        if ((int)allow_id->value > 1)
            return (int)allow_id->value - 2;
        return (int)allow_id->value;
    }
    return 1;
}

bool loc_CanSee(edict_t *targ, edict_t *other)
{
    trace_t     trace;
    vec3_t      targpoints[8];
    vec3_t      viewpoint;
    int         i;

    if (targ->movetype == MOVETYPE_PUSH)
        return false;

    loc_buildboxpoints(targpoints, targ->s.origin, targ->mins, targ->maxs);

    VectorCopy(other->s.origin, viewpoint);
    viewpoint[2] += other->viewheight;

    for (i = 0; i < 8; i++) {
        trace = gi.trace(viewpoint, vec3_origin, vec3_origin, targpoints[i],
                         other, MASK_SOLID);
        if (trace.fraction == 1.0f)
            return true;
    }
    return false;
}

// id's own build_box_points, bug included: p[6] and p[7] are built from p[0]
// rather than from p[4], so two of the eight "corners" are not corners of the
// maxs box at all.
void loc_buildboxpoints(vec3_t p[8], vec3_t org, vec3_t mins, vec3_t maxs)
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

// True when this entity is one the allow_* cvars have switched off, so
// DoRespawn and droptofloor can leave it out of the world.  Ammo goes when
// every weapon that eats it has gone, which is why ammo_shells and
// ammo_bullets test two cvars each.  Two quirks are the target's own and are
// reproduced: ammo_grenades is tested twice, and allow_supershotgun is the one
// cvar read through an int cast rather than compared against 0.0.

bool OSP_disableItems(edict_t *ent)
{
    cvar_t  *shotgun;
    cvar_t  *supershotgun;
    cvar_t  *machinegun;
    cvar_t  *chaingun;
    cvar_t  *grenadelauncher;
    cvar_t  *rocketlauncher;
    cvar_t  *hyperblaster;
    cvar_t  *railgun;
    cvar_t  *bfg;
    cvar_t  *grenades;
    cvar_t  *cells;
    cvar_t  *powerscreen;
    cvar_t  *powershield;
    cvar_t  *quad;
    cvar_t  *invul;
    cvar_t  *pack;

    shotgun = gi.cvar("allow_shotgun", "1", 0);
    supershotgun = gi.cvar("allow_supershotgun", "1", 0);
    machinegun = gi.cvar("allow_machinegun", "1", 0);
    chaingun = gi.cvar("allow_chaingun", "1", 0);
    grenadelauncher = gi.cvar("allow_grenadelauncher", "1", 0);
    rocketlauncher = gi.cvar("allow_rocketlauncher", "1", 0);
    hyperblaster = gi.cvar("allow_hyperblaster", "1", 0);
    railgun = gi.cvar("allow_railgun", "1", 0);
    bfg = gi.cvar("allow_bfg", "1", 0);
    grenades = gi.cvar("allow_ammo_grenades", "1", 0);
    cells = gi.cvar("allow_ammo_cells", "1", 0);
    powerscreen = gi.cvar("allow_item_powerscreen", "1", 0);
    powershield = gi.cvar("allow_item_powershield", "1", 0);
    quad = gi.cvar("allow_item_quad", "1", 0);
    invul = gi.cvar("allow_item_invul", "1", 0);
    pack = gi.cvar("allow_item_pack", "1", 0);

    if (!strcmp(ent->classname, "weapon_shotgun") && shotgun->value == 0)
        return true;
    if (!strcmp(ent->classname, "weapon_supershotgun") &&
        !(int)supershotgun->value)
        return true;
    if (!strcmp(ent->classname, "weapon_machinegun") && machinegun->value == 0)
        return true;
    if (!strcmp(ent->classname, "weapon_chaingun") && chaingun->value == 0)
        return true;
    if (!strcmp(ent->classname, "weapon_grenadelauncher") &&
        grenadelauncher->value == 0)
        return true;
    if (!strcmp(ent->classname, "weapon_rocketlauncher") &&
        rocketlauncher->value == 0)
        return true;
    if (!strcmp(ent->classname, "weapon_hyperblaster") &&
        hyperblaster->value == 0)
        return true;
    if (!strcmp(ent->classname, "weapon_railgun") && railgun->value == 0)
        return true;
    if (!strcmp(ent->classname, "weapon_bfg") && bfg->value == 0)
        return true;
    if (!strcmp(ent->classname, "ammo_grenades") && grenades->value == 0)
        return true;
    if (!strcmp(ent->classname, "ammo_shells") && shotgun->value == 0 &&
        supershotgun->value == 0)
        return true;
    if (!strcmp(ent->classname, "ammo_bullets") && machinegun->value == 0 &&
        chaingun->value == 0)
        return true;
    if (!strcmp(ent->classname, "ammo_rockets") && rocketlauncher->value == 0)
        return true;
    if (!strcmp(ent->classname, "ammo_slugs") && railgun->value == 0)
        return true;
    if (!strcmp(ent->classname, "ammo_grenades") && grenades->value == 0)
        return true;
    if (!strcmp(ent->classname, "ammo_cells") && cells->value == 0)
        return true;
    if (!strcmp(ent->classname, "item_power_screen") &&
        powerscreen->value == 0)
        return true;
    if (!strcmp(ent->classname, "item_power_shield") &&
        powershield->value == 0)
        return true;
    if (!strcmp(ent->classname, "item_quad") && quad->value == 0)
        return true;
    if (!strcmp(ent->classname, "item_invulnerability") && invul->value == 0)
        return true;
    if (!strcmp(ent->classname, "item_pack") && pack->value == 0)
        return true;

    return false;
}

int OSP_checkItems(void)
{
    cvar_t      *bfg;
    cvar_t      *pscreen;
    cvar_t      *pshield;
    cvar_t      *quad;
    cvar_t      *invul;
    int         bits;

    bits = 0;
    bfg = gi.cvar("allow_bfg", "1", 0);
    pscreen = gi.cvar("allow_item_powerscreen", "1", 0);
    pshield = gi.cvar("allow_item_powershield", "1", 0);
    quad = gi.cvar("allow_item_quad", "1", 0);
    invul = gi.cvar("allow_item_invul", "1", 0);

    if ((int)bfg->value)
        bits |= 8;
    if ((int)pscreen->value)
        bits |= 0x10;
    if ((int)pshield->value)
        bits |= 0x10;
    if ((int)quad->value)
        bits |= 1;
    if ((int)invul->value)
        bits |= 2;

    if (m_mode < 2) {
        if ((int)ffa_hurtself->value)
            bits |= 0x40;
    }

    if (m_mode > 1) {
        if ((int)team_hurtteam->value)
            bits |= 0x80;
        if ((int)team_hurtself->value)
            bits |= 0x40;
    }

    if ((int)dmflags->value & DF_QUAD_DROP)
        bits |= 4;
    if ((int)dmflags->value & DF_WEAPONS_STAY)
        bits |= 0x20;

    return bits;
}

// Make the world agree with item_settings after a vote has changed it: spawn
// or remove each affected pickup, then move the matching allow_* cvar so
// OSP_disableItems keeps it that way.  Cells are only ever spawned, never
// removed -- both the BFG and power armour eat them, so neither arm can know
// the other consumer is gone.  The last two bits are dmflags rather than
// cvars: 4 is DF_QUAD_DROP, 0x20 is DF_WEAPONS_STAY.
void OSP_changeItems(void)
{
    char    buf[32];
    cvar_t  *bfg;
    cvar_t  *cells;
    cvar_t  *powerscreen;
    cvar_t  *powershield;
    cvar_t  *quad;
    cvar_t  *invul;
    int     dmf;

    bfg = gi.cvar("allow_bfg", "1", 0);
    cells = gi.cvar("allow_ammo_cells", "1", 0);
    powerscreen = gi.cvar("allow_item_powerscreen", "1", 0);
    powershield = gi.cvar("allow_item_powershield", "1", 0);
    quad = gi.cvar("allow_item_quad", "1", 0);
    invul = gi.cvar("allow_item_invul", "1", 0);

    if (item_settings & 1) {
        if (!(int)quad->value)
            OSP_spawnItem("item_quad");
        gi.cvar_set("allow_item_quad", "1");
    } else {
        if ((int)quad->value)
            OSP_removeItem("item_quad");
        gi.cvar_set("allow_item_quad", "0");
    }

    if (item_settings & 2) {
        if (!(int)invul->value)
            OSP_spawnItem("item_invulnerability");
        gi.cvar_set("allow_item_invul", "1");
    } else {
        if ((int)invul->value)
            OSP_removeItem("item_invulnerability");
        gi.cvar_set("allow_item_invul", "0");
    }

    if (item_settings & 8) {
        if (!(int)bfg->value)
            OSP_spawnItem("weapon_bfg");
        if (!(int)cells->value)
            OSP_spawnItem("ammo_cells");
        gi.cvar_set("allow_bfg", "1");
    } else {
        if ((int)bfg->value)
            OSP_removeItem("weapon_bfg");
        gi.cvar_set("allow_bfg", "0");
    }

    if (item_settings & 0x10) {
        if (!(int)powerscreen->value)
            OSP_spawnItem("item_power_screen");
        if (!(int)powershield->value)
            OSP_spawnItem("item_power_shield");
        if (!(int)cells->value)
            OSP_spawnItem("ammo_cells");
        gi.cvar_set("allow_item_powershield", "1");
        gi.cvar_set("allow_item_powerscreen", "1");
    } else {
        if ((int)powerscreen->value)
            OSP_removeItem("item_power_screen");
        if ((int)powershield->value)
            OSP_removeItem("item_power_shield");
        gi.cvar_set("allow_item_powershield", "0");
        gi.cvar_set("allow_item_powerscreen", "0");
    }

    if (m_mode > 1) {
        if (item_settings & 0x40) {
            gi.cvar_set("team_hurtself", "1");
            osp_teams[0].osp_m120 = 1;
            osp_teams[1].osp_m120 = 1;
        } else {
            gi.cvar_set("team_hurtself", "0");
            osp_teams[0].osp_m120 = 0;
            osp_teams[1].osp_m120 = 0;
        }
    } else {
        if (item_settings & 0x40)
            gi.cvar_set("ffa_hurtself", "1");
        else
            gi.cvar_set("ffa_hurtself", "0");
    }

    if (m_mode > 1) {
        if (item_settings & 0x80) {
            gi.cvar_set("team_hurtteam", "1");
            osp_teams[0].osp_m11c = 1;
            osp_teams[1].osp_m11c = 1;
        } else {
            gi.cvar_set("team_hurtteam", "0");
            osp_teams[0].osp_m11c = 0;
            osp_teams[1].osp_m11c = 0;
        }
    }

    dmf = (int)dmflags->value;

    if (item_settings & 4)
        dmf |= DF_QUAD_DROP;
    else
        dmf &= ~DF_QUAD_DROP;

    if (item_settings & 0x20)
        dmf |= DF_WEAPONS_STAY;
    else
        dmf &= ~DF_WEAPONS_STAY;

    Q_snprintf(buf, sizeof(buf), "%d", dmf);
    gi.cvar_set("dmflags", buf);
}

void OSP_removeItem(char *classname)
{
    edict_t     *ent;
    int         t;

    for (ent = &g_edicts[(int)game.maxclients + 1],
         t = game.maxclients + 1;
         t < globals.num_edicts;
         t++, ent++) {
        if (!ent->inuse)
            continue;
        if (!strcmp(classname, ent->classname))
            SetRespawn(ent, 65000);
    }
}

void OSP_spawnItem(char *classname)
{
    edict_t     *ent;
    int         t;

    for (ent = &g_edicts[(int)game.maxclients + 1],
         t = game.maxclients + 1;
         t < globals.num_edicts;
         t++, ent++) {
        if (!ent->inuse)
            continue;

        if (!strcmp(classname, ent->classname) &&
            (!ent->team || ent == ent->teammaster))
            // 1, not 1.0f: this is one FRAME before now, so that the think
            // fires next frame.  As a float literal it reads as a duration
            // that lost its BASE_FRAMERATE, which is what units.py reported.
            ent->nextthink = level.framenum - 1;
    }
}

// The pre-match state machine, one step per frame.  sync_stat 0 is warmup and
// only nags the unready every 90 seconds; 1 is the countdown, and when it
// expires voting shuts off, the osp_teams lock, every client is re-seeded and the
// map is swept clean of gibs and bodies; 2 is the last ten seconds, and when
// THAT expires everybody is killed into a fresh spawn, the sweep runs again
// and the match goes live at sync_stat 4.
// The `sync_stat = 4; ...; sync_stat = 2;` pairs are the target's own: they
// exist so the functions called in between (OSP_DoRankSort, Cmd_Kill_f) see a
// running match rather than a countdown.
void OSP_checkSync(void)
{
    if (sync_stat > 2 || level.intermission_framenum != 0)
        return;

    if (!sync_stat && level.framenum > sync_frame) {
        int         notrdy;
        int         i;
        edict_t     *ent;

        notrdy = 0;

        if (active_clients > 1 && !(int)match_strictmode->value) {
            for (i = 1; i <= game.maxclients; i++) {
                ent = g_edicts + i;

                if (!ent->inuse || !ent->client ||
                    ent->client->resp.osp_entered != ENTERED_ENTERED)
                    continue;

                if (!ent->client->resp.osp_r20c) {
                    notrdy++;

                    if (!(ent->flags & FL_BOT))
                        gi.cprintf(ent, PRINT_HIGH,
                                   "Type \"ready\" at console to start.\n");
                }
            }

            gi.bprintf(PRINT_CHAT, "%d players not ready.\n", notrdy);
        }

        sync_frame = level.framenum + 900;
    }

    if (sync_stat == 1 && level.framenum > sync_frame) {
        char        userinfo[512];
        int         i;
        edict_t     *ent;

        vote_inprogress = 0;
        gi.bprintf(PRINT_CHAT, "Voting now disabled.\n");

        if (m_mode == 2) {
            if ((int)match_latejoin->value <= 2) {
                osp_teams[0].osp_m0f4 = 1;
                osp_teams[1].osp_m0f4 = 1;
                gi.bprintf(PRINT_HIGH, "Teams locked.\n");
            } else {
                osp_teams[0].osp_m0f4 = 0;
                osp_teams[1].osp_m0f4 = 0;
            }
        }

        for (i = 1; i <= game.maxclients; i++) {
            ent = g_edicts + i;

            if (!ent->inuse || !ent->client || (ent->flags & FL_BOT))
                continue;

            gi.WriteByte(svc_stufftext);
            gi.WriteString("play world/10_0.wav");
            gi.unicast(ent, true);

            if (ent->client->resp.osp_entered != ENTERED_ENTERED)
                continue;

            Q_strlcpy(userinfo, ent->client->pers.userinfo, sizeof(userinfo));
            InitClientPersistant(ent->client, false);
            ent->client->pers.health = 150;
            ent->client->latched_buttons = 0;
            ent->client->newweapon = NULL;
            ent->client->pers.weapon = NULL;
            ChangeWeapon(ent);
            ent->client->pers.lastweapon = NULL;
        }

        for (i = 1; i < globals.num_edicts; i++) {
            ent = g_edicts + i;

            if (!ent->inuse)
                continue;

            if ((ent->s.effects & EF_GIB) &&
                strcmp(ent->classname, "bodyque"))
                G_FreeEdict(ent);
            else if (!strcmp(ent->classname, "bodyque")) {
                gi.unlinkentity(ent);
                ent->s.origin[0] = 0;
                ent->s.origin[1] = 0;
                ent->s.origin[2] = 0;
                ent->s.modelindex = 0;
                ent->solid = SOLID_NOT;
                ent->svflags |= SVF_NOCLIENT;
                gi.linkentity(ent);
            }
        }

        if (rune_stat)
            OSP_removeRunes();

        sync_stat = 2;
        sync_frame = level.framenum + 100;
    }

    if (sync_stat == 2 && level.framenum > sync_frame) {
        char        sndcmd[80];
        char        clinfo[512];
        int         i;
        int         j;
        edict_t     *ent;

        sync_stat = 4;
        OSP_DoRankSort();
        sync_stat = 2;

        for (i = 1; i <= game.maxclients; i++) {
            ent = g_edicts + i;

            if (!ent->inuse || !ent->client)
                continue;

            if (ent->client->resp.osp_entered != ENTERED_ENTERED) {
                Q_strlcpy(sndcmd, "play misc/bigtele.wav\n", sizeof(sndcmd));
                gi.WriteByte(svc_stufftext);
                gi.WriteString(sndcmd);
                gi.unicast(ent, true);
                continue;
            }

            Cmd_Kill_f(ent);
            sync_stat = 4;
            sync_stat = 2;

            ent->client->latched_buttons = 0;
            ent->client->resp.osp_r0a0 = -1998;
            ent->client->resp.osp_r09c = -1;

            if (m_mode < 2)
                G_SetStat(ent, SID_OSP_STATUS3, OSP_CS(4));
            else {
                for (j = 0; j < 2; j++) {
                    osp_teams[j].osp_m110 = -1999;
                    osp_teams[j].osp_m0f8 = 0;

                    if ((int)match_latejoin->value <= 2 || m_mode == 3)
                        osp_teams[j].osp_m0f4 = 1;

                    osp_teams[j].osp_m0fc = 0;
                    osp_teams[j].osp_m100 = 0;
                    osp_teams[j].osp_m104 = 0;
                    osp_teams[j].osp_m108 = 0;
                    osp_teams[j].osp_m10c = (int)match_timeouts->value;
                    ent->client->resp.osp_r2d0 = (int)match_timeouts->value;
                }
            }

            if (!(ent->flags & FL_BOT)) {
                sndcmd[0] = 0;

                if (m_mode == 1) {
                    if ((int)qualifier_forceskins->value) {
                        if ((int)match_startsound->value)
                            Q_snprintf(sndcmd, sizeof(sndcmd),
                                       "play misc/bigtele.wav; skin %s\n",
                                       qualifier_skinname->string);
                        else
                            Q_snprintf(sndcmd, sizeof(sndcmd), "skin %s\n",
                                       qualifier_skinname->string);
                    } else if ((int)match_startsound->value)
                        Q_strlcpy(sndcmd, "play misc/bigtele.wav\n", sizeof(sndcmd));
                } else if ((int)match_startsound->value)
                    Q_strlcpy(sndcmd, "play misc/bigtele.wav\n", sizeof(sndcmd));

                if (sndcmd[0]) {
                    gi.WriteByte(svc_stufftext);
                    gi.WriteString(sndcmd);
                    gi.unicast(ent, true);
                }
            } else if (m_mode == 1) {
                Q_strlcpy(clinfo, ent->client->pers.userinfo, sizeof(clinfo));
                Info_SetValueForKey(clinfo, "skin", qualifier_skinname->string);
                ClientUserinfoChanged(ent, clinfo);
            }

            if (m_mode == 2)
                OSP_initTeamFrags(ent);
        }

        for (i = 1; i < globals.num_edicts; i++) {
            ent = g_edicts + i;

            if (!ent->inuse)
                continue;

            if ((ent->s.effects & EF_GIB) && strcmp(ent->classname, "bodyque"))
                G_FreeEdict(ent);
            else if (!strcmp(ent->classname, "bodyque")) {
                gi.unlinkentity(ent);
                ent->s.origin[0] = 0;
                ent->s.origin[1] = 0;
                ent->s.origin[2] = 0;
                ent->s.modelindex = 0;
                ent->solid = SOLID_NOT;
                ent->svflags |= SVF_NOCLIENT;
                gi.linkentity(ent);
            }
        }

        OSP_setAllAccuracy();
        OSP_clearClients();

        sync_frame = level.framenum;
        sync_time = level.time;

        gi.bprintf(PRINT_HIGH, "Match has started!\n");
        sync_stat = 4;
        OSP_Stats_MatchStart();

        if (rune_stat) {
            runespawn = 0;
            OSP_setupRuneSpawn(5);
        }
    }
}

int OSP_CheckReady(void)
{
    char    userinfo[512];
    int     i;
    int     readycnt = 0;
    int     syncret = 0;
    edict_t *ent;

    if (sync_stat > 0)
        return syncret;

    if (m_mode > 1 && (!OSP_teamCount(0) || !OSP_teamCount(1))) {
        gi.bprintf(PRINT_HIGH, "Not enough players to start match.\n");
        sync_stat = 0;
        return syncret;
    }

    if (active_clients < 2) {
        gi.bprintf(PRINT_HIGH, "Not enough players to start match.\n");
        sync_stat = 0;
        return syncret;
    }

    readycnt = OSP_countReady();

    if (readycnt <= active_clients *
        (100 - (int)match_prestartpercent->value) / 100 &&
        readycnt > active_clients *
        (100 - (int)match_readypercent->value) / 100) {
        syncret = 1;

        for (i = 1; i <= game.maxclients; i++) {
            ent = g_edicts + i;

            if (!ent->inuse || !ent->client)
                continue;

            if (!(ent->flags & FL_BOT)) {
                if (!ent->client->resp.osp_r20c)
                    gi.cprintf(ent, PRINT_CHAT,
                               "Warmup mode over. Ready up!\n");
                else
                    gi.cprintf(ent, PRINT_HIGH,
                               "Warmup mode over. Waiting for others to ready up.\n");
            }

            Q_strlcpy(userinfo, ent->client->pers.userinfo, sizeof(userinfo));
            InitClientPersistant(ent->client, false);
            ent->client->pers.health = 150;
            ent->client->latched_buttons = 0;
            ent->client->newweapon = NULL;
            ent->client->pers.weapon = NULL;
            ChangeWeapon(ent);
            ent->client->pers.lastweapon = NULL;
        }
    }

    if (readycnt <= active_clients *
        (100 - (int)match_readypercent->value) / 100) {
        syncret = 2;

        if (m_mode < 2)
            gi.configstring(OSP_CS(3), "STARTING");
        else {
            gi.configstring(OSP_CS(6), "     STARTING");
            gi.configstring(OSP_CS(8), "     STARTING");
        }

        OSP_setShowParams();

        for (i = 1; i <= game.maxclients; i++) {
            ent = g_edicts + i;

            if (!ent->inuse || !ent->client)
                continue;

            G_SetStat(ent, SID_OSP_MATCHSTATE, OSP_CS(1));
            ent->client->resp.osp_r0ac = level.framenum;

            Q_strlcpy(userinfo, ent->client->pers.userinfo, sizeof(userinfo));
            InitClientPersistant(ent->client, false);
            ent->client->pers.health = 150;
            ent->client->latched_buttons = 0;
            ent->client->newweapon = NULL;
            ent->client->pers.weapon = NULL;
            ChangeWeapon(ent);
            ent->client->pers.lastweapon = NULL;
            ent->client->resp.osp_r010 = 0;

            if ((int)match_countinfo->value)
                OSP_showinfo_cmd(ent);
        }

        gi.bprintf(PRINT_HIGH, "All players ready... countdown starts!\n");
        sync_stat = 1;

        if ((int)match_countdown->value < 14)
            gi.cvar_set("match_countdown", "14");

        sync_frame = level.framenum +
                     ((int)match_countdown->value - 10) * 10;
        sync_startframe = level.framenum + (int)match_countdown->value * 10;
        time_update = 0;

        if ((int)demo_referee->value || (int)demo_player->value)
            OSP_startDemos();
    }

    return syncret;
}

int OSP_countReady(void)
{
    edict_t     *ent;
    int         i;
    int         count;

    count = 0;
    for (i = 1; i <= game.maxclients; i++) {
        ent = g_edicts + i;
        if (!ent->inuse || !ent->client ||
            ent->client->resp.osp_entered != ENTERED_ENTERED)
            continue;

        if (!ent->client->resp.osp_r20c)
            count++;
    }
    return count;
}

// Recount the clients every frame and decide whether the match can still go
// on.  A qualifier that drops below two players, or a team/1v1 side that
// empties out, ends the match here: the logs are closed and restarted, every
// client's demo is stopped and its connect/mode lines are re-emitted so the
// fresh log has a full player list.  When nobody is left the pause is lifted
// too, which is what clears PMF_NO_PREDICTION off the frozen clients.
void OSP_checkHalt(int team)
{
    char    scratch[64];
    int     bots;
    int     t;
    edict_t *targ;

    connected_clients = 0;
    active_clients = 0;
    bots = 0;

    for (t = 1; t <= game.maxclients; t++) {
        targ = g_edicts + t;

        if (targ->inuse && targ->client && targ->client->pers.connected) {
            connected_clients++;

            if (targ->client->resp.osp_entered == ENTERED_ENTERED)
                active_clients++;
            if (targ->flags & FL_BOTCLIENT)
                bots++;
        }
    }

    botglobals.numbots = bots;
    if (bots_votedin > bots)
        bots_votedin = 0;

    if (level.intermission_framenum == 0) {
        if (sync_stat == 4) {
            if (m_mode == 1 && active_clients <= 1) {
                gi.bprintf(PRINT_HIGH,
                           "Not enough players for match!  Match terminated.\n");
                OSP_allnotready_svcmd(false);
                OSP_clearClients();
                OSP_Stats_AccuracyAll();
                OSP_Stats_MatchEnd("qualifier not enough players");
                OSP_Stats_GameInit();

                for (t = 1; t <= game.maxclients; t++) {
                    targ = g_edicts + t;

                    if (!targ->inuse || !targ->client)
                        continue;

                    if (targ->client->resp.osp_r234) {
                        gi.WriteByte(svc_stufftext);
                        gi.WriteString("stop\n");
                        gi.unicast(targ, true);
                    }

                    OSP_Stats_PlayerConnect(targ);
                    OSP_setSingleAccuracy(targ);
                    targ->client->resp.osp_r248 = 0;

                    if (targ->client->resp.osp_entered == 1) {
                        OSP_Stats_PlayerRespawn(targ);
                        OSP_Stats_PlayerEnter(targ);
                    } else if (targ->client->resp.osp_entered == 2)
                        OSP_Stats_PlayerMode(targ, "Observe");
                    else if (targ->client->resp.osp_entered == 16)
                        OSP_Stats_PlayerMode(targ, "Autocam");
                    else
                        OSP_Stats_PlayerMode(targ, "Chasecam");
                }
            } else if (team != 2 && !OSP_teamCount(team)) {
                for (t = 0; t < 2; t++) {
                    osp_teams[t].osp_m0f8 = 0;
                    osp_teams[t].osp_m0f4 = 0;
                    osp_teams[t].osp_m100 = 0;
                    osp_teams[t].osp_m0fc = 0;
                    osp_teams[t].osp_m104 = 0;
                    osp_teams[t].osp_m108 = 0;
                    osp_teams[t].osp_m124 = 0;
                }

                if (m_mode == 2) {
                    if (sync_stat > 2)
                        gi.bprintf(PRINT_HIGH,
                                   "%s forfeits! %s wins by default!\n",
                                   osp_teams[team].netname, osp_teams[1 - team].netname);
                    else
                        gi.bprintf(PRINT_HIGH,
                                   "No team to play! Match terminated.\n");

                    Q_snprintf(scratch, sizeof(scratch),
                               "teamplay not enough players (%s)",
                            osp_teams[team].netname);
                } else {
                    if (sync_stat > 2)
                        gi.bprintf(PRINT_HIGH,
                                   "%s forfeits! %s wins by default!\n",
                                   osp_teams[team].netname, osp_teams[1 - team].netname);
                    else
                        gi.bprintf(PRINT_HIGH,
                                   "No opponent to play! Match terminated.\n");

                    Q_snprintf(scratch, sizeof(scratch), "1v1 (%s) left",
                               osp_teams[team].netname);
                }

                OSP_Stats_AccuracyAll();
                OSP_Stats_MatchEnd(scratch);
                OSP_allnotready_svcmd(false);
                OSP_clearClients();
                OSP_Stats_GameInit();

                for (t = 1; t <= game.maxclients; t++) {
                    targ = g_edicts + t;

                    if (!targ->inuse || !targ->client)
                        continue;

                    if (targ->client->resp.osp_r234) {
                        gi.WriteByte(svc_stufftext);
                        gi.WriteString("stop\n");
                        gi.unicast(targ, true);
                    }

                    OSP_Stats_PlayerConnect(targ);
                    OSP_setSingleAccuracy(targ);
                    targ->client->resp.osp_r248 = 0;

                    if (targ->client->resp.osp_entered == 1) {
                        OSP_Stats_PlayerRespawn(targ);
                        OSP_Stats_TeamJoin(targ);
                        OSP_Stats_PlayerEnter(targ);
                    } else if (targ->client->resp.osp_entered == 2)
                        OSP_Stats_PlayerMode(targ, "Observe");
                    else if (targ->client->resp.osp_entered == 16)
                        OSP_Stats_PlayerMode(targ, "Autocam");
                    else
                        OSP_Stats_PlayerMode(targ, "Chasecam");
                }
            }
        } else if (sync_stat < 4 && active_clients > 1)
            OSP_CheckReady();

        if (!active_clients ||
            (!(int) team_duelrecover->value && active_clients == 1)) {
            if (m_mode > 1 && !active_clients)
                OSP_teamReset();

            if (match_paused) {
                match_paused = 0;
                who_paused = -1;

                for (t = 1; t <= game.maxclients; t++) {
                    edict_t *other;

                    other = g_edicts + t;

                    if (!other->inuse || !other->client ||
                        other->client->resp.osp_entered > 2)
                        continue;

                    other->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
                }
            }
        }
    }
}

void OSP_setAllAccuracy(void)
{
    edict_t     *ent;
    int         i;
    int         j;
    int         cid;

    for (i = 0; i < game.maxclients; i++) {
        ent = g_edicts + i + 1;
        if (ent->inuse && ent->client) {
            if (ent->client->resp.osp_entered != ENTERED_ENTERED)
                continue;

            cid = ent->client->resp.clientid;
            if (cid < 0 || cid >= q_countof(p_acc))
                continue;
            Q_strlcpy(p_acc[cid].netname, ent->client->pers.netname,
                      sizeof(p_acc[cid].netname));
            p_acc[cid].dgiven = 0;
            p_acc[cid].dtaken = 0;
            for (j = 0; j < 11; j++) {
                p_acc[cid].shots[j] = 0;
                p_acc[cid].hits[j] = 0;
                p_acc[cid].given[j] = 0;
                p_acc[cid].taken[j] = 0;
            }
        }
    }
}

void OSP_setSingleAccuracy(edict_t *ent)
{
    int         pid;
    int         i;

    pid = ent->client->resp.clientid;
    if (pid < 0 || pid >= q_countof(p_acc))
        return;
    Q_strlcpy(p_acc[pid].netname, ent->client->pers.netname,
              sizeof(p_acc[pid].netname));
    p_acc[pid].dgiven = 0;
    p_acc[pid].dtaken = 0;
    for (i = 0; i < 11; i++) {
        p_acc[pid].shots[i] = 0;
        p_acc[pid].hits[i] = 0;
        p_acc[pid].given[i] = 0;
        p_acc[pid].taken[i] = 0;
    }
}

// Stuff a "record <name>" into every client that asked for a demo -- a referee
// through osp_e39c and demo_referee, a player through resp.osp_entered and
// demo_player.  The name carries the players or the two team names, the
// demo_tag, the map and the date, and is then filtered down to what a
// filesystem will take.  In 1v1 a player's demo is named after the OPPONENT,
// which is what the idx[0] == i test picks.
// Two faults are the target's own: name[] holds two entries but the collect
// loop stops at three, and idx[2] spills into the date buffer (harmless,
// since date is filled afterwards).
void OSP_startDemos(void)
{
    char        name[2][16];
    char        wbuf[MAX_STRING_CHARS];
    char        clean[MAX_STRING_CHARS];
    char        tstr[128];
    int         cids[2];
    int         i;
    int         index;
    // <INVENTED NAME>: separate from `chars` below.
    int         found;
    int         chars;
    int         c;
    edict_t     *ent;
    gclient_t   *cl;

    found = 0;

    if (m_mode == 3) {
        for (i = 0; i < game.maxclients; i++) {
            ent = g_edicts + i + 1;

            if (!ent->inuse || !ent->client ||
                ent->client->resp.osp_entered != 1)
                continue;

            cids[found] = i;
            Q_strlcpy(name[found], ent->client->pers.netname, sizeof(name[0]));

            if (++found == 2)
                break;
        }
    }

    OSP_Stats_DateString(tstr, sizeof(tstr));

    for (i = 0; i < game.maxclients; i++) {
        ent = g_edicts + i + 1;

        if (!ent->inuse || !ent->client || (ent->flags & FL_BOT))
            continue;

        cl = ent->client;

        if (ent->osp_e39c == 1 && (int)demo_referee->value) {
            if (m_mode == 2)
                Q_snprintf(wbuf, sizeof(wbuf), "REF%s-%s-%s-%s-%s-%s",
                        cl->pers.netname, osp_teams[0].netname,
                        osp_teams[1].netname, demo_tag->string, level.mapname,
                        tstr);
            else if (m_mode == 3)
                Q_snprintf(wbuf, sizeof(wbuf), "REF%s-%s-%s-%s-%s-%s",
                        cl->pers.netname, name[0], name[1],
                        demo_tag->string, level.mapname, tstr);
            else
                Q_snprintf(wbuf, sizeof(wbuf), "REF%s-%s-%s-%s", cl->pers.netname,
                        demo_tag->string, level.mapname, tstr);

            for (index = 0; index < sizeof(clean); index++)
                clean[index] = 0;

            for (index = 0, chars = 0;
                 wbuf[index] && chars < sizeof(clean) - 1; index++) {
                c = wbuf[index];
                if (c == '<' || c == '>' || c == '\\' || c == '/' ||
                    c == '*' || c == '&' || c == '?' || c == '|' ||
                    c == ' ' || c == ':' || c == ';' || c == '"' ||
                    c == '$' || (byte)c < 0x20)
                    continue;

                clean[chars] = wbuf[index];
                chars++;
            }

            Q_snprintf(wbuf, sizeof(wbuf), "record %s\n", clean);
            cl->resp.osp_r234 = 1;
            gi.WriteByte(svc_stufftext);
            gi.WriteString(wbuf);
            gi.unicast(ent, true);
        } else if (cl->resp.osp_entered == 1 && (int)demo_player->value) {
            if (m_mode == 2)
                Q_snprintf(wbuf, sizeof(wbuf), "%s-%s-%s-%s-%s-%s", cl->pers.netname,
                        osp_teams[0].netname, osp_teams[1].netname, demo_tag->string,
                        level.mapname, tstr);
            else if (m_mode == 3) {
                if (i == cids[0])
                    Q_snprintf(wbuf, sizeof(wbuf), "%s-%s-%s-%s-%s",
                            cl->pers.netname, name[1],
                            demo_tag->string, level.mapname, tstr);
                else
                    Q_snprintf(wbuf, sizeof(wbuf), "%s-%s-%s-%s-%s",
                            cl->pers.netname, name[0],
                            demo_tag->string, level.mapname, tstr);
            } else
                Q_snprintf(wbuf, sizeof(wbuf), "%s-%s-%s-%s", cl->pers.netname,
                        demo_tag->string, level.mapname, tstr);

            for (index = 0; index < sizeof(clean); index++)
                clean[index] = 0;

            for (index = 0, chars = 0;
                 wbuf[index] && chars < sizeof(clean) - 1; index++) {
                c = wbuf[index];
                if (c == '<' || c == '>' || c == '\\' || c == '/' ||
                    c == '*' || c == '&' || c == '?' || c == '|' ||
                    c == ' ' || c == ':' || c == ';' || c == '"' ||
                    c == '$' || (byte)c < 0x20)
                    continue;

                clean[chars] = wbuf[index];
                chars++;
            }

            Q_snprintf(wbuf, sizeof(wbuf), "record %s\n", clean);
            cl->resp.osp_r234 = 1;
            gi.WriteByte(svc_stufftext);
            gi.WriteString(wbuf);
            gi.unicast(ent, true);
        }
    }
}

void OSP_warmupItems(edict_t *ent)
{
    const gitem_t   *curitem;
    int         j;

    ent->client->pers.health = (int)warmup_health->value;
    ent->client->pers.max_health = (int)warmup_health->value;

    for (j = 0; j < game.num_items; j++) {
        curitem = &itemlist[j];
        if (!curitem->pickup)
            continue;
        if (!(curitem->flags & IT_WEAPON))
            continue;
        if (!strcmp(curitem->pickup_name, "BFG10K"))
            continue;
        ent->client->pers.inventory[j]++;
    }

    for (j = 0; j < game.num_items; j++) {
        curitem = &itemlist[j];
        if (!curitem->pickup)
            continue;
        if (!(curitem->flags & IT_AMMO))
            continue;
        Add_Ammo(ent, curitem, 1000);
    }

    curitem = FindItem("Jacket Armor");
    ent->client->pers.inventory[ITEM_INDEX(curitem)] = 0;
    curitem = FindItem("Combat Armor");
    ent->client->pers.inventory[ITEM_INDEX(curitem)] = 0;
    curitem = FindItem("Body Armor");
    ent->client->pers.inventory[ITEM_INDEX(curitem)] = (int)warmup_armor->value;
    curitem = FindItem("Railgun");
    ent->client->pers.selected_item = ITEM_INDEX(curitem);
    ent->client->pers.weapon = curitem;
}

void OSP_closeMenus(void)
{
    edict_t     *ent;
    int         i;

    for (i = 1; i <= game.maxclients; i++) {
        ent = g_edicts + i;
        if (!ent->inuse || !ent->client || ent->client->menu_owner != MENU_TOURNEY)
            continue;

        osp_PMenu_Close(ent);
    }
}

void OSP_serverbotsRemove(void)
{
    edict_t     *ent;
    int         i;

    for (i = 1; i <= game.maxclients; i++) {
        ent = g_edicts + i;
        if (ent->inuse) {
            if (!(ent->flags & FL_BOT))
                continue;
            BotDestroy(ent);
        }
    }
}

void OSP_saveClient(edict_t *ent)
{
    if (ent->client->resp.clientid < 0 ||
        ent->client->resp.clientid >= q_countof(saved_clients))
        return;

    memcpy(&saved_clients[ent->client->resp.clientid],
           &game.clients[ent - g_edicts - 1], sizeof(gclient_t));
}

// A reconnecting player gets their old gclient_t back if saved_clients[] still
// holds one under the same name.  resp.osp_r018 is the cookie that says the
// slot is real -- 0 and 12345678 are both "empty" (OSP_clearClients writes the
// latter).  resp.osp_r210 is the "was recovered" flag, and it survives only
// during a live 1v1 match with somebody on that team.
void OSP_recoverClient(edict_t *ent, char *userinfo)
{
    char        *name;
    int         n;

    ent->client->resp.osp_r210 = 0;
    ent->client->resp.osp_r018 = 0;
    ent->client->resp.clientid = -1;

    name = Info_ValueForKey(userinfo, "name");

    if (!level.intermission_framenum) {
        for (n = 0; n < maxconn_clients; n++) {
            gclient_t   *saved = &saved_clients[n];

            if (saved->resp.osp_r214[0] &&
                !Q_stricmp(name, saved->resp.osp_r214) &&
                saved->resp.osp_r018 &&
                saved->resp.osp_r018 != 12345678) {
                saved->resp.osp_r210 = 1;
                memcpy(&game.clients[ent - g_edicts - 1], &saved_clients[n],
                       sizeof(gclient_t));
                break;
            }
        }
    }

    // v2.75 passed the search loop's index here, which is a client number,
    // not a team.  The intent is "the slot this player would come back to is
    // still occupied", so ask about their own team.
    if (sync_stat < 4 ||
        (ent->client->resp.osp_r210 && m_mode == 3 &&
         OSP_teamCount(ent->client->resp.team)))
        ent->client->resp.osp_r210 = 0;
}

// The client-id allocator: hands out `maxconn_clients` and bumps it, but never
// past the end of saved_clients[].
void OSP_giveClientID(edict_t *ent)
{
    // v2.75 let the counter reach 128 and then handed that out for every
    // further client, one past saved_clients[]'s last element -- and
    // OSP_saveClient memcpy's a whole gclient_t through it.  The last slot is
    // shared instead.
    if (maxconn_clients >= q_countof(saved_clients))
        maxconn_clients = q_countof(saved_clients) - 1;

    ent->client->resp.clientid = maxconn_clients++;
}

void OSP_clearClients(void)
{
    int         i;

    for (i = 0; i < 128; i++) {
        saved_clients[i].resp.osp_r214[0] = 0;
        saved_clients[i].resp.osp_r018 = 12345678;
    }
}

void OSP_consoleStamp(void)
{
    cvar_t      *port;
    time_t      t;
    char        tmp[32];
    struct tm   *tm;

    port = gi.cvar("port", "27910", CVAR_SERVERINFO | CVAR_NOSET);
    time(&t);
    tm = localtime(&t);
    Q_snprintf(tmp, sizeof(tmp), "%.19s", tm ? asctime(tm) : "");
    gi.dprintf("[ SERVERTIME (port %d) : %s ]\n", (int)port->value, tmp);
    if (server_log)
        OSP_logAdminLog("Date: %s", tmp);
}

// Look a player up by name first and by client id second.  "0" and "00" have
// to be tested explicitly because Q_atoi() cannot tell them from a failed parse.
edict_t *OSP_findPlayer(char *name)
{
    edict_t     *ent;
    int         i;
    int         pid;

    for (i = 1; i <= game.maxclients; i++) {
        ent = g_edicts + i;
        if (!ent->inuse || !ent->client ||
            Q_stricmp(ent->client->pers.netname, name))
            continue;

        return ent;
    }

    pid = Q_atoi(name);
    if (!pid && Q_stricmp(name, "0") && Q_stricmp(name, "00"))
        return NULL;

    for (i = 1; i <= game.maxclients; i++) {
        ent = g_edicts + i;
        if (!ent->inuse || !ent->client ||
            ent->client->resp.clientid != pid)
            continue;

        return ent;
    }
    return NULL;
}

// Build the "what is switched on" line the scoreboard header shows, and push
// it into the match_info cvar.
void OSP_setFeatures(void)
{
    char        buf[1024];

    buf[0] = 0;
    if (rune_stat)
        Q_strlcat(buf, "Runes", sizeof(buf));

    if ((int)hook_enable->value)
        Q_strlcat(buf, buf[0] ? ", Hook" : "Hook", sizeof(buf));

    if ((int)client_protect->value)
        Q_strlcat(buf, buf[0] ? ", Protection" : "Protection", sizeof(buf));

    if (!buf[0])
        Q_strlcpy(buf, "None", sizeof(buf));

    gi.cvar_set("match_info", buf);
}

void OSP_setupAdminLog(void)
{
    cvar_t      *hostname;
    cvar_t      *port;
    cvar_t      *logmode;
    cvar_t      *adminname;
    char        date[32];
    time_t      now;
    struct tm   *tm;

    hostname = gi.cvar("hostname", "noname", CVAR_SERVERINFO);
    port = gi.cvar("port", "27910", CVAR_SERVERINFO | CVAR_NOSET);
    logmode = gi.cvar("server_adminlog", "0", 0);
    adminname = gi.cvar("server_adminname", "serveradmin.log", 0);

    if (!(int)logmode->value) {
        server_log = NULL;
        gi.dprintf("Local server admin logging disabled.\n");
        return;
    }

    server_log = fopen(adminname->string, "a+");
    if (!server_log) {
        gi.dprintf("Couldn't open admin log \"%s\".\n", adminname->string);
        gi.dprintf("Local server admin logging disabled.\n");
        return;
    }

    gi.dprintf("Admin log for server is \"%s\".\n", adminname->string);
    fprintf(server_log, "-----------------------------------\n");
    fprintf(server_log, "Server: %s [port %d]\n", hostname->string,
            (int)port->value);
    time(&now);
    tm = localtime(&now);
    Q_snprintf(date, sizeof(date), "%.19s", tm ? asctime(tm) : "");
    fprintf(server_log, "Date: %s\n", date);
    fflush(server_log);
}

void OSP_logAdminLog(char *fmt, ...)
{
    char        text[1024];
    va_list     argptr;

    if (!server_log)
        return;

    va_start(argptr, fmt);
    vsnprintf(text, sizeof(text), fmt, argptr);
    va_end(argptr);

    fprintf(server_log, "%s\n", text);
    fflush(server_log);
}

// Caches the client's dotted-quad (without the port) in edict+0x37c, which is
// what every ban and every admin log line prints.
void OSP_getPlayerAddr(edict_t *ent)
{
    char        buf[128];
    char        *p;

    if (ent->osp_e37c[0])
        return;

    p = Info_ValueForKey(ent->client->pers.userinfo, "ip");
    Q_strlcpy(buf, p, sizeof(buf));
    p = strchr(buf, ':');
    if (p)
        *p = 0;
    // osp_e37c is 32 bytes and osp_e39c -- the referee flag -- is the field
    // right behind it, so an address that did not fit used to hand out
    // referee status.  An IPv6 literal is long enough to do it.
    Q_strlcpy(ent->osp_e37c, buf, sizeof(ent->osp_e37c));
}

// A muzzle-flash-channel sound played on the player themselves.  In 1v1 with a
// match running, only players actually in the game make a noise.
void OSP_playerAnnounce(edict_t *ent, char sound)
{
    if (m_mode != 3 || sync_stat < 4 ||
        ent->client->resp.osp_entered == ENTERED_ENTERED) {
        gi.WriteByte(svc_muzzleflash);
        gi.WriteShort(ent - g_edicts);
        gi.WriteByte(sound);
        gi.multicast(ent->s.origin, MULTICAST_PVS);
    }
}

void OSP_parseArmor(void)
{
    OSP_parseString(armor_jacket->string, &jacketarmor_info);
    OSP_parseString(armor_combat->string, &combatarmor_info);
    OSP_parseString(armor_body->string, &bodyarmor_info);
}

// Split "base max normal energy" into a gitem_armor_t.  Note the tokeniser
// copies the WHOLE remaining string into the next 32-byte slot each time round
// and then cuts it at the first space, so tok[3] is what is left over; nothing
// is written unless all four fields were present.
void OSP_parseString(const char *str, gitem_armor_t *info)
{
    char        tok[4][32];
    char        *p;
    const char  *s;
    int         n;

    s = str;
    for (n = 0; n < 4; n++) {
        Q_strlcpy(tok[n], s, sizeof(tok[n]));
        p = strchr(tok[n], ' ');
        if (!p) {
            n++;
            break;
        }
        *p = 0;
        p++;
        s = p;
    }

    if (n == 4) {
        info->base_count = Q_atoi(tok[0]);
        info->max_count = Q_atoi(tok[1]);
        info->normal_protection = atof(tok[2]);
        info->energy_protection = atof(tok[3]);
    }
}
