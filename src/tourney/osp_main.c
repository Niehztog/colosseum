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
// old_botcount, which OSP_endClean resets -- declared by the file that defines
// it (R-OSP-5).
#include "bot/bl_spawn.h"
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
// R-OSP-12.  `m_mode` and `match_mode` are gone; these are the two questions
// that meant a RANGE of the old modes.  Everything that meant one mode tests
// the ruleset by name.
bool OSP_IsMatch(void)
{
    return G_Ruleset() != RULESET_DM && G_IsOspRuleset();
}

bool OSP_IsTeams(void)
{
    return G_Ruleset() == RULESET_TDM || G_Ruleset() == RULESET_DUEL;
}

// R-DM-1 / doc/reconciliation.md R-156: the capacity `tdm` and `duel` declare,
// which is what their bot fill is sized from.  OSP_gameInit has already
// registered it and clamped it so that twice it fits `maxclients`.
int OSP_TeamMaxPlayers(void)
{
    return team_maxplayers ? (int)team_maxplayers->value : 0;
}
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
// The accuracy report's rows, in the order they print.  NULL-TERMINATED rather
// than counted: three loops in two other files walked this with their own copy
// of `10`, so adding a row meant editing four places and the compiler could not
// say if you missed one (R-181).
//
// The donor's ten are first and unchanged, ACC_BFG's missing row included --
// osp-tourney collects the BFG's accuracy and never prints it, which is its
// choice about a weapon its rulesets remove, and sec 7 rule 2 leaves a donor its
// own feature.  The nine after them are the content layers' (R-MODE-3), and the
// names are padded to the same eleven columns so the report stays a table.
a_info_t a_info[] = {
    {  ACC_BLASTER,          "Blaster   :" },
    {  ACC_SHOTGUN,          "Shotgun   :" },
    {  ACC_SSHOTGUN,         "S.Shotgun :" },
    {  ACC_MACHINEGUN,       "Machinegun:" },
    {  ACC_CHAINGUN,         "Chaingun  :" },
    {  ACC_GRENADE,          "Grenades  :" },
    {  ACC_GRENADELAUNCHER,  "G.Launcher:" },
    {  ACC_ROCKET,           "R.Launcher:" },
    {  ACC_HYPERBLASTER,     "H.Blaster :" },
    {  ACC_RAILGUN,          "Railgun   :" },
    // Xatrix
    {  ACC_RIPPER,           "Ionripper :" },
    {  ACC_PHALANX,          "Phalanx   :" },
    {  ACC_TRAP,             "Trap      :" },
    // Ground Zero
    {  ACC_ETF_RIFLE,        "ETF Rifle :" },
    {  ACC_PROX,             "Prox Lnchr:" },
    {  ACC_HEATBEAM,         "PlasmaBeam:" },
    {  ACC_CHAINFIST,        "Chainfist :" },
    {  ACC_TESLA,            "Tesla     :" },
    {  ACC_DISRUPTOR,        "Disruptor :" },
    {  0, "" },     // sentinel: an empty name ends the table
};
int motd_read = 0;
int who_paused = -1;
// The match-restart countdown after a timeout expires; -1 means not counting.
int end_timeout = -1;
// How many overtime periods this match has already played.
int ot_count = 0;
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
int start_weap[OSP_NUM_WEAPS];
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
    // R-COMPAT-6: `gamename` is Colosseum's serverinfo identity, registered
    // once in g_main.c with GAMEVERSION.  Re-obtained here with the SAME
    // default so that a second registration cannot decide what the server calls
    // itself; the mod's own version banner is the OSP_CS(9) configstring
    // OSP_worldspawn writes, which is where it belongs.
    osp_game = gi.cvar("gamename", GAMEVERSION,
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

    // `match_mode` is gone: the mode of play is the ruleset (R-OSP-12), latched
    // by `g_ruleset` and resolved before this runs.  `match_type` stays, because
    // it is what a server browser and every stats record read, and it is set
    // from the ruleset in the banner block below.
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
    if (!OSP_IsMatch())
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

    if (!OSP_IsTeams())
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
    if (G_Ruleset() == RULESET_DUEL)
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
    if (OSP_IsMatch())
        gi.cvar_set("client_protect", "0");
    client_recover = gi.cvar("client_recover", "0", 0);
    client_nomove = gi.cvar("client_nomove", "90", 0);

    // The two server-browser score cells: only team play publishes numbers.
    if (G_Ruleset() == RULESET_TDM) {
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

    // 1v1 is two teams of one, whatever the server asked for.
    if (OSP_IsTeams()) {
        if (G_Ruleset() == RULESET_DUEL) {
            gi.cvar_set("team_maxplayers", "1");
            gi.dprintf("1V1 Mode: setting teams' maxplayers to 1.\n");
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

        if (G_Ruleset() == RULESET_TDM) {
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
    if (!OSP_IsTeams() && (int)client_highscores->value &&
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

    // R-OSP-12's four rows, one per ruleset.  `match_type` is still published to
    // serverinfo -- server browsers read it and osp_stats.c writes it into every
    // JSON record -- but it is DERIVED from the ruleset now rather than being a
    // second place the mode is stored, so the banner and the behaviour cannot
    // disagree the way R-OSP-13 records them doing.
    if (G_Ruleset() == RULESET_DM) {
        sync_stat = 8;
        sync_frame = 0;
        gi.cvar_set("qualifier_numspots", "0");
        gi.cvar_set("match_type", "RegularDM");
        gi.dprintf("Mode: *** REGULAR DEATHMATCH ***\n");
    } else if (G_Ruleset() == RULESET_DMPRO) {
        sync_stat = 0;
        if (!(int)qualifier_numspots->value)
            gi.cvar_set("qualifier_numspots", "1");
        gi.cvar_set("match_type", "QualifierDM");
        gi.dprintf("Mode: *** DM QUALIFIER ***\n");
        gi.dprintf("Number of qualifiers per match: %d\n",
                   (int)qualifier_numspots->value);
    } else if (G_Ruleset() == RULESET_TDM) {
        sync_stat = 0;
        gi.cvar_set("match_type", "TeamPlay");
        gi.dprintf("Mode: *** DM TEAM-PLAY MODE ***\n");
    } else {                    // RULESET_DUEL -- the fourth and last
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

    if (G_Ruleset() == RULESET_DUEL && (int)bots_autoload->value == 2 &&
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

    if (OSP_IsMatch())
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

    if (OSP_IsTeams())
        OSP_teamReset();
}

/*
=================
THE LOADOUT SLOTS, BY NAME -- R-180

*** THE DONOR ADDRESSES THE ITEMLIST BY ABSOLUTE INDEX, and this tree's
*** itemlist is not the donor's. ***

osp-tourney's is baseq2 3.20's, and `OSP_seedPlayer` is written against it:
1..3 the three armours, 7..17 the eleven weapons in `weapon_initial` /
`weapon_have` order, 18..22 the five ammo types.  This tree's inserts
Threewave's `weapon_grapple` at 7 and then Xatrix's and Rogue's weapons and
ammo among the rest, so nothing above 6 lines up -- and the drift is not a
constant, which is why a shift would not have fixed it either:

    blaster           7 -> 8      grenade launcher 13 -> 16
    shotgun           8 -> 9      rocket launcher  14 -> 18
    super shotgun     9 -> 10     hyperblaster     15 -> 19
    machinegun       10 -> 11     railgun          16 -> 22
    chaingun         11 -> 12     BFG10K           17 -> 24
    grenades (ammo)  12 -> 14     shells           18 -> 27

What it cost, measured rather than argued: the warmup arm below set
`pers.weapon = &itemlist[7]`, so **every player under dm, dmpro, tdm and duel
spawned holding the CTF grapple** -- which is also why the grapple's view model
and HUD icon were still being registered on a deathmatch server after R-173
gated the precache away, the two configstrings that led here.  `bl_main.c`
already documents the same offset for the brain's own inventory table
("`weapon_grapple` is index 7 here and the Blaster is 7 there"); nothing carried
that knowledge across to the loadout.

RESOLVED LAZILY, and that is not a style choice: `OSP_initWeapItem` is called
from `OSP_gameInit`, which InitGame runs BEFORE `InitItems()` -- so
`game.num_items` is still 0 there and `FindItemByClassname` would find nothing
and return NULL for all twenty names.  Every reader is a per-client spawn path,
long after that, so the first one resolves.  The itemlist is `const` and static,
so once is enough for the life of the library.
=================
*/
// OSP_NUM_WEAPS, OSP_NUM_AMMO, OSP_NUM_ARMOR, OSP_NUM_LAYER_AMMO and the
// OSP_LAYER_* ranks are in osp_types.h, beside the arrays they size.

// Rank 0 is the blaster, and it is the only rank the code names: the warmup arm
// hands it out and `client_protect` asks "did this player spawn with it".
#define OSP_WEAP_BLASTER    0

// Donor rank -> classname, in `weapon_initial` / `weapon_have` / `start_weap`
// order.  Rank 5 is `ammo_grenades` and not a weapon_* row, which is baseq2's
// own quirk -- hand grenades are ammo that fires itself -- and is why this
// table is the donor's order rather than the itemlist's.
// *** THIS TABLE IS THE CVAR CONTRACT.  APPEND ONLY, NEVER REORDER. ***
// Bit k-1 of `weapon_have` and of `weapon_initial` is rank k, so moving a row
// silently changes what an operator's config means.
static const char *const osp_weapnames[OSP_NUM_WEAPS] = {
    // The donor's eleven, bits 0x1..0x200 (rank 0, the blaster, has no bit --
    // `start_weap[0]` is hardcoded 1 because every player starts with one).
    "weapon_blaster", "weapon_shotgun", "weapon_supershotgun",
    "weapon_machinegun", "weapon_chaingun", "ammo_grenades",
    "weapon_grenadelauncher", "weapon_rocketlauncher", "weapon_hyperblaster",
    "weapon_railgun", "weapon_bfg",
    // The content layers' eight, bits 0x400..0x20000 (R-181).  Ranks 11..18, in
    // the merged itemlist's own order, continuing where the donor's eleven stop.
    // Both `ammo_trap` and `ammo_tesla` are IT_AMMO|IT_WEAPON -- a thrown device
    // that is its own ammunition, exactly like `ammo_grenades` at rank 5 -- so
    // they appear here AND in osp_layerammonames below, and the count written
    // second wins, which is the pattern the donor already has for grenades.
    "weapon_etf_rifle",         // 0x400   Ground Zero
    "ammo_trap",                // 0x800   Xatrix
    "weapon_proxlauncher",      // 0x1000  Ground Zero
    "weapon_boomer",            // 0x2000  Xatrix, the Ionripper
    "weapon_plasmabeam",        // 0x4000  Ground Zero
    "weapon_phalanx",           // 0x8000  Xatrix
    "weapon_chainfist",         // 0x10000 Ground Zero
    "ammo_tesla",               // 0x20000 Ground Zero
};

// `start_items[0..5]`.
static const char *const osp_ammonames[OSP_NUM_AMMO] = {
    "ammo_shells", "ammo_bullets", "ammo_cells",
    "ammo_grenades", "ammo_rockets", "ammo_slugs",
};

// `start_items[8..10]`, which is `start_armortype` 0, 1 and 2 in that order.
static const char *const osp_armornames[OSP_NUM_ARMOR] = {
    "item_armor_jacket", "item_armor_combat", "item_armor_body",
};

// The layers' five ammo types, in OSP_LAYER_* order (R-181).  `ammo_disruptor`
// is absent for the same reason the Disruptor is: IT_NOT_GIVEABLE.
static const char *const osp_layerammonames[OSP_NUM_LAYER_AMMO] = {
    "ammo_magslug", "ammo_flechettes", "ammo_prox", "ammo_tesla", "ammo_trap",
};

static int  osp_weapslot[OSP_NUM_WEAPS];
static int  osp_ammoslot[OSP_NUM_AMMO];
static int  osp_armorslot[OSP_NUM_ARMOR];
static int  osp_layerslot[OSP_NUM_LAYER_AMMO];
static bool osp_slots_resolved;

// The layers' start / ceiling / pack amounts, the three arrays R-181 adds
// beside the donor's `start_items`, `max_items` and `pack_items`.  Static
// because nothing outside this file reads them, which is true of the donor's
// three as well.
static int  start_layer[OSP_NUM_LAYER_AMMO];
static int  max_layer[OSP_NUM_LAYER_AMMO];
static int  pack_layer[OSP_NUM_LAYER_AMMO];

// Their fifteen cvars, likewise static.
static cvar_t *start_magslug, *start_flechettes, *start_prox_a, *start_tesla_a,
              *start_trap_a;
static cvar_t *max_magslug, *max_flechettes, *max_prox_a, *max_tesla_a,
              *max_trap_a;
static cvar_t *pack_magslug, *pack_flechettes, *pack_prox_a, *pack_tesla_a,
              *pack_trap_a;

// The rank `weapon_initial` chose.  `initial_weap` stays what the donor's name
// says it is -- an itemlist INDEX -- and is derived from this once the itemlist
// can be searched.
static int  osp_initial_rank;

static int OSP_loadoutSlot(const char *classname)
{
    const gitem_t *it = FindItemByClassname(classname);

    if (!it) {
        gi.dprintf("Colosseum: the tourney loadout wants %s and the itemlist "
                   "has no such item (R-180)\n", classname);
        return 0;   // itemlist[0] is the reserved NULL row: an inert write
    }
    return ITEM_INDEX(it);
}

static void OSP_resolveLoadoutSlots(void)
{
    int i;

    if (!osp_slots_resolved) {
        osp_slots_resolved = true;

        for (i = 0; i < OSP_NUM_WEAPS; i++)
            osp_weapslot[i] = OSP_loadoutSlot(osp_weapnames[i]);
        for (i = 0; i < OSP_NUM_AMMO; i++)
            osp_ammoslot[i] = OSP_loadoutSlot(osp_ammonames[i]);
        for (i = 0; i < OSP_NUM_ARMOR; i++)
            osp_armorslot[i] = OSP_loadoutSlot(osp_armornames[i]);
        for (i = 0; i < OSP_NUM_LAYER_AMMO; i++)
            osp_layerslot[i] = OSP_loadoutSlot(osp_layerammonames[i]);
    }

    // Every call, not just the first: OSP_initWeapItem can run again and move
    // the rank, and keeping the two in step here costs one array read rather
    // than a second place that has to remember.
    initial_weap = osp_weapslot[osp_initial_rank];
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

    // *** THE CONTENT LAYERS' FIFTEEN (R-181). ***
    //
    // Colosseum's own, not a donor's, so sec 7 rule 6 governs the naming rather
    // than R-OSP-11: they extend an existing tourney family and keep its
    // prefixes.  Every `start_*` defaults to 0 and every `max_*` and `pack_*` to
    // the number the code already used, so an untouched config behaves exactly
    // as before:
    //
    //   max_*   InitClientPersistant's ceilings -- 50, 200, 50, 50, 5 -- so
    //           OSP_seedPlayer's assignment is a no-op at the default.
    //   pack_*  what BASEQ2's pack gives, which is the one deliberate default
    //           change: pack_raise_ceilings lifts max_magslug to 100 and
    //           max_flechettes to 200 in every other ruleset, and tourney's
    //           OSP_packPlayer lifted neither because it had no cvar to read.
    //           Prox, tesla and trap are unlifted there too, so their pack
    //           defaults equal their ceilings.
    start_magslug = gi.cvar("start_magslug", "0", 0);
    start_flechettes = gi.cvar("start_flechettes", "0", 0);
    start_prox_a = gi.cvar("start_prox", "0", 0);
    start_tesla_a = gi.cvar("start_tesla", "0", 0);
    start_trap_a = gi.cvar("start_trap", "0", 0);
    max_magslug = gi.cvar("max_magslug", "50", 0);
    max_flechettes = gi.cvar("max_flechettes", "200", 0);
    max_prox_a = gi.cvar("max_prox", "50", 0);
    max_tesla_a = gi.cvar("max_tesla", "50", 0);
    max_trap_a = gi.cvar("max_trap", "5", 0);
    pack_magslug = gi.cvar("pack_magslug", "100", 0);
    pack_flechettes = gi.cvar("pack_flechettes", "200", 0);
    pack_prox_a = gi.cvar("pack_prox", "50", 0);
    pack_tesla_a = gi.cvar("pack_tesla", "50", 0);
    pack_trap_a = gi.cvar("pack_trap", "5", 0);

    initial = (int)weapon_initial->value;
    have = (int)weapon_have->value;

    // The donor's ladder, one bit per rank, highest set bit wins -- the same
    // eleven in the same order as `osp_weapnames` above.  It records the RANK
    // rather than the itemlist index the donor wrote here, because the itemlist
    // cannot be searched yet (R-180); OSP_resolveLoadoutSlots turns it into
    // `initial_weap` on the first spawn.
    osp_initial_rank = OSP_WEAP_BLASTER;
    if (initial & 0x1)
        osp_initial_rank = 1;
    if (initial & 0x2)
        osp_initial_rank = 2;
    if (initial & 0x4)
        osp_initial_rank = 3;
    if (initial & 0x8)
        osp_initial_rank = 4;
    if (initial & 0x10)
        osp_initial_rank = 5;
    if (initial & 0x20)
        osp_initial_rank = 6;
    if (initial & 0x40)
        osp_initial_rank = 7;
    if (initial & 0x80)
        osp_initial_rank = 8;
    if (initial & 0x100)
        osp_initial_rank = 9;
    if (initial & 0x200)
        osp_initial_rank = 10;
    // R-181's eight, continuing the donor's one-bit-per-rank scheme.
    if (initial & 0x400)
        osp_initial_rank = 11;
    if (initial & 0x800)
        osp_initial_rank = 12;
    if (initial & 0x1000)
        osp_initial_rank = 13;
    if (initial & 0x2000)
        osp_initial_rank = 14;
    if (initial & 0x4000)
        osp_initial_rank = 15;
    if (initial & 0x8000)
        osp_initial_rank = 16;
    if (initial & 0x10000)
        osp_initial_rank = 17;
    if (initial & 0x20000)
        osp_initial_rank = 18;

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
    // R-181's eight.  Zero unless an operator asks, so a config that predates
    // them hands out exactly the eleven it always did.
    start_weap[11] = (have & 0x400) != 0;
    start_weap[12] = (have & 0x800) != 0;
    start_weap[13] = (have & 0x1000) != 0;
    start_weap[14] = (have & 0x2000) != 0;
    start_weap[15] = (have & 0x4000) != 0;
    start_weap[16] = (have & 0x8000) != 0;
    start_weap[17] = (have & 0x10000) != 0;
    start_weap[18] = (have & 0x20000) != 0;

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

    // R-181's three, in OSP_LAYER_* order.
    start_layer[OSP_LAYER_MAGSLUG] = (int)start_magslug->value;
    start_layer[OSP_LAYER_FLECHETTES] = (int)start_flechettes->value;
    start_layer[OSP_LAYER_PROX] = (int)start_prox_a->value;
    start_layer[OSP_LAYER_TESLA] = (int)start_tesla_a->value;
    start_layer[OSP_LAYER_TRAP] = (int)start_trap_a->value;

    max_layer[OSP_LAYER_MAGSLUG] = (int)max_magslug->value;
    max_layer[OSP_LAYER_FLECHETTES] = (int)max_flechettes->value;
    max_layer[OSP_LAYER_PROX] = (int)max_prox_a->value;
    max_layer[OSP_LAYER_TESLA] = (int)max_tesla_a->value;
    max_layer[OSP_LAYER_TRAP] = (int)max_trap_a->value;

    pack_layer[OSP_LAYER_MAGSLUG] = (int)pack_magslug->value;
    pack_layer[OSP_LAYER_FLECHETTES] = (int)pack_flechettes->value;
    pack_layer[OSP_LAYER_PROX] = (int)pack_prox_a->value;
    pack_layer[OSP_LAYER_TESLA] = (int)pack_tesla_a->value;
    pack_layer[OSP_LAYER_TRAP] = (int)pack_trap_a->value;
}

// The warmup loadout: every weapon except the BFG, full ammo, body armour to
// taste and the railgun in hand.
void OSP_seedPlayer(gclient_t *client)
{
    edict_t     *p;
    int         t;
    int         ready;
    int         i;

    // R-180: the donor's absolute itemlist indices, resolved by classname.
    // First call does the lookups; every call keeps `initial_weap` in step.
    OSP_resolveLoadoutSlots();

    if (sync_stat <= 1) {
        if (sync_stat == 1) {
            client->pers.inventory[initial_weap] = 0;
            client->pers.selected_item = osp_weapslot[OSP_WEAP_BLASTER];
            client->pers.weapon = &itemlist[osp_weapslot[OSP_WEAP_BLASTER]];
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
            client->pers.selected_item = osp_weapslot[OSP_WEAP_BLASTER];
            client->pers.weapon = &itemlist[osp_weapslot[OSP_WEAP_BLASTER]];
            return;
        }
    }

    // The donor's eleven `inventory[7..17] = start_weap[0..10]`, in the donor's
    // order, which is what `osp_weapslot` is indexed by.
    for (i = 0; i < OSP_NUM_WEAPS; i++)
        client->pers.inventory[osp_weapslot[i]] = start_weap[i];

    // ...and its nine `start_items` writes, in the donor's order, which matters
    // for exactly one of them: hand grenades are `ammo_grenades` in BOTH tables,
    // so the count below deliberately lands on the slot the have/have-not flag
    // above just wrote, and the count is meant to win.
    client->pers.inventory[osp_armorslot[2]] = start_items[10];     // body
    client->pers.inventory[osp_armorslot[1]] = start_items[9];      // combat
    client->pers.inventory[osp_armorslot[0]] = start_items[8];      // jacket
    client->pers.inventory[osp_ammoslot[0]] = start_items[0];       // shells
    client->pers.inventory[osp_ammoslot[1]] = start_items[1];       // bullets
    client->pers.inventory[osp_ammoslot[2]] = start_items[2];       // cells
    client->pers.inventory[osp_ammoslot[3]] = start_items[3];       // grenades
    client->pers.inventory[osp_ammoslot[4]] = start_items[4];       // rockets
    client->pers.inventory[osp_ammoslot[5]] = start_items[5];       // slugs

    // R-181's five, after the weapon loop for the same reason the donor's ammo
    // block is: `ammo_trap` and `ammo_tesla` are ranks 12 and 18 of that loop as
    // well as layer ammo, so the count is written second and wins.
    for (i = 0; i < OSP_NUM_LAYER_AMMO; i++)
        client->pers.inventory[osp_layerslot[i]] = start_layer[i];

    client->pers.max_shells = max_items[0];
    client->pers.max_bullets = max_items[1];
    client->pers.max_cells = max_items[2];
    client->pers.max_grenades = max_items[3];
    client->pers.max_rockets = max_items[4];
    client->pers.max_slugs = max_items[5];

    // ...and the layers' five ceilings, which InitClientPersistant has just set
    // to the numbers these cvars default to, so this is a no-op unless a
    // referee has moved one (R-181).  Five statements rather than a loop
    // because they are five distinct struct members.
    client->pers.max_magslug = max_layer[OSP_LAYER_MAGSLUG];
    client->pers.max_flechettes = max_layer[OSP_LAYER_FLECHETTES];
    client->pers.max_prox = max_layer[OSP_LAYER_PROX];
    client->pers.max_tesla = max_layer[OSP_LAYER_TESLA];
    client->pers.max_trap = max_layer[OSP_LAYER_TRAP];

    client->pers.health = start_items[7];
    client->pers.max_health = max_items[7];

    client->pers.inventory[initial_weap] = 1;
    client->pers.selected_item = initial_weap;
    client->pers.weapon = &itemlist[initial_weap];

    // client_protect seconds of spawn protection, but only in plain DM, only
    // for a player actually in the game, and only if they spawn with the
    // BLASTER -- i.e. not on a weapons-start server.  The donor spells that
    // `initial_weap == 7`, which is the itemlist index of the blaster there and
    // of weapon_grapple here (R-180); asked by rank it needs no index at all.
    if ((int)client_protect->value && client->resp.osp_entered == ENTERED_ENTERED &&
        G_Ruleset() == RULESET_DM && osp_initial_rank == OSP_WEAP_BLASTER)
        client->resp.osp_r23c = level.framenum +
                                (int)client_protect->value * 10;
    else
        client->resp.osp_r23c = 0;
}

/*
=================
OSP_itemFreed

R-OSP-3: a powerup that timed out in the world rather than on a player.

`nextthink <= level.framenum` is the whole test for "it expired" as against "it
was removed": a quad taken by a player, swept by a referee's allow_item_quad 0
or cleared by a map change is freed with its think still in the future.

The two items are found by name and cached, because G_FreeEdict runs for every
edict the game ever throws away and the donor's own test -- comparing
`item->use` against Use_Quad -- needs two functions that are static to
g_items.c.  `itemlist` is static storage, so the pointers stay valid.
=================
*/
void OSP_itemFreed(edict_t *ed)
{
    static const gitem_t *quad, *invul;

    if (!quad) {
        quad = FindItem("Quad Damage");
        invul = FindItem("Invulnerability");
    }

    if (ed->nextthink > level.framenum)
        return;

    if (ed->item == quad)
        OSP_Stats_ItemExpire("Quad", NULL, ed - g_edicts);
    else if (ed->item == invul)
        OSP_Stats_ItemExpire("Invulnerability", NULL, ed - g_edicts);
}

// R-OSP-1's armour numbers, asked by the shared Pickup_Armor so that g_items.c
// needs neither the cvars nor max_items[]/pack_items[].  0 means "no ceiling of
// its own", which is what Pickup_Armor starts from and what every ruleset but
// tourney keeps.
//
// `max_items[6]` is `max_armor` and `pack_items[6]` is `pack_armor`; both were
// computed by OSP_initWeapItem and read by nobody, which made the two cvars
// dead and left the armour ceiling at baseq2's per-type max_count.
int OSP_armorCeiling(edict_t *ent)
{
    const gitem_t *pack;

    if (!ent->client)
        return 0;

    pack = FindItem("Ammo Pack");
    if (pack && ent->client->pers.inventory[ITEM_INDEX(pack)])
        return pack_items[6];

    return max_items[6];
}

int OSP_armorShard(void)
{
    return armor_shard ? (int)armor_shard->value : 2;
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

    // R-181: the layers' five, which this function had no cvar to read -- so a
    // pack under tourney raised the six baseq2 ceilings and left magslug and
    // flechettes where every other ruleset's pack_raise_ceilings() lifts them.
    // Assigned rather than raised, which is this function's whole shape: a
    // referee may set a pack worth LESS than baseq2's.
    ent->client->pers.max_magslug = pack_layer[OSP_LAYER_MAGSLUG];
    ent->client->pers.max_flechettes = pack_layer[OSP_LAYER_FLECHETTES];
    ent->client->pers.max_prox = pack_layer[OSP_LAYER_PROX];
    ent->client->pers.max_tesla = pack_layer[OSP_LAYER_TESLA];
    ent->client->pers.max_trap = pack_layer[OSP_LAYER_TRAP];
}

// Append a short tag for every allow_* cvar that is switched off to whatever
// the caller has already built in buf.  The length is taken before the run and
// compared after, so " NONE" goes on only when nothing was appended.
// Appends to whatever the caller has already built.  All three callers pass a
// 1024-byte layout scratch buffer, which is what OSP_DISABLED_ITEMS_MAX below
// is measured against: the whole run is under 60 characters.
#define OSP_DISABLED_ITEMS_MAX  1024

/*
=================
THE allow_* FAMILY, IN ONE TABLE -- R-183

An operator turns an item off with `allow_<thing> 0` and three separate pieces
of code have to agree about what that means: SpawnItem asks whether to inhibit
the entity, the scoreboard banner names what is off, and the stats log records
it.  The donor holds three copies of the correspondence -- sixteen classname
compares here, fourteen banner tags here, fourteen cvar/name pairs in
osp_stats.c -- and they did not even cover the same set: `allow_ammo_cells` and
`allow_item_pack` inhibit an item that neither the banner nor the log mentions.

THAT DRIFT IS THE POINT.  All three lists stopped at baseq2, so under `xatrix 1`
or `rogue 1` -- which R-MODE-3 makes valid with every ruleset -- a referee could
switch off the shotgun and not the Ion Ripper, and no banner or log could have
said so.  One table now, and a row that omits a tag or a log name says so with
NULL rather than by being absent from a second list.

The ammo rules below the table are NOT rows, and deliberately: shells are gone
when BOTH shotgun weapons are, bullets when both bullet weapons are, and so on.
That is a rule about other cvars rather than a cvar of its own, which is the one
shape a single row cannot carry.
=================
*/
const osp_allow_t osp_allow_items[] = {
    // cvar                      classname               tag         log name
    { "allow_shotgun",          "weapon_shotgun",        "S",        "Shotgun" },
    { "allow_supershotgun",     "weapon_supershotgun",   "SS",       "Super Shotgun" },
    { "allow_machinegun",       "weapon_machinegun",     "MG",       "Machinegun" },
    { "allow_chaingun",         "weapon_chaingun",       "CG",       "Chaingun" },
    { "allow_grenadelauncher",  "weapon_grenadelauncher", "GL",      "Grenade Launcher" },
    { "allow_rocketlauncher",   "weapon_rocketlauncher", "RL",       "Rocket Launcher" },
    { "allow_hyperblaster",     "weapon_hyperblaster",   "HB",       "HyperBlaster" },
    { "allow_railgun",          "weapon_railgun",        "RG",       "Railgun" },
    { "allow_bfg",              "weapon_bfg",            "BFG",      "BFG10K" },
    { "allow_ammo_grenades",    "ammo_grenades",         "G",        "Grenades" },
    { "allow_item_powerscreen", "item_power_screen",     "P.Screen", "Power Screen" },
    { "allow_item_powershield", "item_power_shield",     "P.Shield", "Power Shield" },
    { "allow_item_quad",        "item_quad",             "Quad",     "Quad" },
    { "allow_item_invul",       "item_invulnerability",  "Invul",    "Invulnerability" },
    // The donor inhibits these two and names them nowhere.  Kept that way: the
    // banner is a fixed-width HUD line and the log is a wire format, so adding
    // to either is a change to what a client draws or a parser reads, and
    // neither is what R-183 is about.
    { "allow_ammo_cells",       "ammo_cells",            NULL,       NULL },
    { "allow_item_pack",        "item_pack",             NULL,       NULL },

    // R-183: the content layers.  Xatrix first, then Ground Zero, each in its
    // own itemlist order.  The Disruptor is here even though it is
    // IT_NOT_GIVEABLE (R-16): `give` cannot hand it out, but a map may still
    // PLACE one, and inhibiting a placed entity is exactly what this table does.
    { "allow_ionripper",        "weapon_boomer",         "IR",       "Ionripper" },
    { "allow_phalanx",          "weapon_phalanx",        "PH",       "Phalanx" },
    { "allow_ammo_trap",        "ammo_trap",             "Trap",     "Trap" },
    { "allow_etfrifle",         "weapon_etf_rifle",      "ETF",      "ETF Rifle" },
    { "allow_proxlauncher",     "weapon_proxlauncher",   "PL",       "Prox Launcher" },
    { "allow_plasmabeam",       "weapon_plasmabeam",     "PB",       "Plasma Beam" },
    { "allow_chainfist",        "weapon_chainfist",      "CF",       "Chainfist" },
    { "allow_disruptor",        "weapon_disintegrator",  "DIS",      "Disruptor" },
    { "allow_ammo_tesla",       "ammo_tesla",            "Tesla",    "Tesla" },
    { "allow_item_nuke",        "ammo_nuke",             "A-M",      "A-M Bomb" },
    { NULL, NULL, NULL, NULL }
};

// Is `thing` switched on?  The default is "1" at every call site the donor has,
// so an unknown name reads as allowed.
static bool osp_allowed(const char *cvar)
{
    return (int)gi.cvar(cvar, "1", 0)->value != 0;
}

void OSP_listDisabledItems(char *buf)
{
    size_t  len = strlen(buf);
    int     i;

    for (i = 0; osp_allow_items[i].cvar; i++)
        if (osp_allow_items[i].tag && !osp_allowed(osp_allow_items[i].cvar))
            Q_strlcat(buf, va(" %s", osp_allow_items[i].tag),
                      OSP_DISABLED_ITEMS_MAX);

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
    if (!OSP_IsTeams()) {
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

    if (!OSP_IsTeams()) {
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
            (G_Ruleset() == RULESET_TDM && (int)team_idteam->value)) {
            if (cl->resp.osp_entered != 16)
                G_SetStat(ent, SID_CHASE, OSP_setID(ent));
        }
    }

    if (!(level.framenum & 1) && sync_stat > 2) {
        OSP_checkAnnounce(ent);

        if (OSP_IsTeams())
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

        if (G_Ruleset() == RULESET_DMPRO) {
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
        } else if (G_Ruleset() == RULESET_DM) {
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
                (G_Ruleset() == RULESET_DMPRO &&
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
        if (G_Ruleset() == RULESET_TDM && (ent->client->resp.team == 2 ||
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
    int     i;

    for (i = 0; osp_allow_items[i].cvar; i++)
        if (!strcmp(ent->classname, osp_allow_items[i].classname) &&
            !osp_allowed(osp_allow_items[i].cvar))
            return true;

    // The derived ammo rules: ammunition goes when every weapon that fires it
    // has gone.  The donor's four, and R-183's four beside them -- one per pack
    // weapon that has ammo of its own.  `ammo_cells` is not here because it has
    // a cvar in the table: three weapons share cells and the donor gave the
    // ammo its own switch rather than deriving it from all three.
    if (!strcmp(ent->classname, "ammo_shells") &&
        !osp_allowed("allow_shotgun") && !osp_allowed("allow_supershotgun"))
        return true;
    if (!strcmp(ent->classname, "ammo_bullets") &&
        !osp_allowed("allow_machinegun") && !osp_allowed("allow_chaingun"))
        return true;
    if (!strcmp(ent->classname, "ammo_rockets") &&
        !osp_allowed("allow_rocketlauncher"))
        return true;
    if (!strcmp(ent->classname, "ammo_slugs") && !osp_allowed("allow_railgun"))
        return true;
    if (!strcmp(ent->classname, "ammo_magslug") && !osp_allowed("allow_phalanx"))
        return true;
    if (!strcmp(ent->classname, "ammo_flechettes") &&
        !osp_allowed("allow_etfrifle"))
        return true;
    if (!strcmp(ent->classname, "ammo_prox") &&
        !osp_allowed("allow_proxlauncher"))
        return true;
    if (!strcmp(ent->classname, "ammo_disruptor") &&
        !osp_allowed("allow_disruptor"))
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

    if (!OSP_IsTeams()) {
        if ((int)ffa_hurtself->value)
            bits |= 0x40;
    }

    if (OSP_IsTeams()) {
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

    if (OSP_IsTeams()) {
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

    if (OSP_IsTeams()) {
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
// expires voting shuts off, the teams lock, every client is re-seeded and the
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

        if (G_Ruleset() == RULESET_TDM) {
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

            if (!OSP_IsTeams())
                G_SetStat(ent, SID_OSP_STATUS3, OSP_CS(4));
            else {
                for (j = 0; j < 2; j++) {
                    osp_teams[j].osp_m110 = -1999;
                    osp_teams[j].osp_m0f8 = 0;

                    if ((int)match_latejoin->value <= 2 || G_Ruleset() == RULESET_DUEL)
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

                if (G_Ruleset() == RULESET_DMPRO) {
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
            } else if (G_Ruleset() == RULESET_DMPRO) {
                Q_strlcpy(clinfo, ent->client->pers.userinfo, sizeof(clinfo));
                Info_SetValueForKey(clinfo, "skin", qualifier_skinname->string);
                ClientUserinfoChanged(ent, clinfo);
            }

            if (G_Ruleset() == RULESET_TDM)
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

    if (OSP_IsTeams() && (!OSP_teamCount(0) || !OSP_teamCount(1))) {
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

        if (!OSP_IsTeams())
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
            if (G_Ruleset() == RULESET_DMPRO && active_clients <= 1) {
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

                if (G_Ruleset() == RULESET_TDM) {
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
            if (OSP_IsTeams() && !active_clients)
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
            for (j = 0; j < ACC_COUNT; j++) {
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

    if (G_Ruleset() == RULESET_DUEL) {
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
            if (G_Ruleset() == RULESET_TDM)
                Q_snprintf(wbuf, sizeof(wbuf), "REF%s-%s-%s-%s-%s-%s",
                        cl->pers.netname, osp_teams[0].netname,
                        osp_teams[1].netname, demo_tag->string, level.mapname,
                        tstr);
            else if (G_Ruleset() == RULESET_DUEL)
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
            if (G_Ruleset() == RULESET_TDM)
                Q_snprintf(wbuf, sizeof(wbuf), "%s-%s-%s-%s-%s-%s", cl->pers.netname,
                        osp_teams[0].netname, osp_teams[1].netname, demo_tag->string,
                        level.mapname, tstr);
            else if (G_Ruleset() == RULESET_DUEL) {
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
        (ent->client->resp.osp_r210 && G_Ruleset() == RULESET_DUEL &&
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

    hostname = gi.cvar("hostname", "", CVAR_SERVERINFO);
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
    if (G_Ruleset() != RULESET_DUEL || sync_stat < 4 ||
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

/*
=================
OSP_CheckRules

R-OSP-1's match system as the tourney ruleset's CheckRules row (R-MODE-5).

The donor drives all of this from G_RunFrame directly, beside CheckDMRules,
which is the same shape RA2 had and R-76 corrected there: two match-rule systems
at one call site with nothing saying how they compose.  Here the row says it.
The match clock, the vote timeout, the team frag totals and the pre-match sync
are tourney's ROUND-scope work; CheckDMRules ends the LEVEL, and tourney keeps
`timelimit` and `fraglimit` at that scope exactly as RA2 does.
=================
*/
/*
=================
OSP_CheckRules

Tourney's CheckRules row (R-MODE-5, R-OSP-1).  It REPLACES CheckDMRules rather
than wrapping it, because every one of baseq2's three answers is different here:

  * the clock runs from `sync_time`, the frame the match went live, not from the
    start of the level -- warmup does not count against the timelimit
  * a drawn team match at the timelimit goes to overtime rather than ending, as
    many times as `OSP_overtimeWork` allows
  * the fraglimit is per TEAM under `tdm` and `duel`, and sudden death (`frag_offset`)
    ends the moment the two totals differ at all

The per-frame work that used to live here -- the clock, the vote timeout, the
team totals, the sync check -- moved to OSP_frameStart when the frame loop was
merged: it runs every frame whether or not the rules are being checked, and
running it from the rules row made it look like a rule.
=================
*/
void OSP_CheckRules(void)
{
    int i;

    if (level.intermission_framenum)
        return;

    if (timelimit->value && sync_stat > 2) {
        if (level.time - sync_time >=
            (timelimit->value + overtime_timer) * 60 && !frag_offset) {
            if (OSP_IsTeams()) {
                if (OSP_teamFrags(0) == OSP_teamFrags(1) &&
                    OSP_overtimeWork(ot_count)) {
                    ot_count++;
                    return;
                }
                OSP_findTeamWinner();
            }

            ot_count = 0;
            gi.bprintf(PRINT_HIGH, "Timelimit hit.\n");
            sl_SoftGameEnd(&gi, level);
            // R-OSP-3: every way a match ends closes the accuracy table and
            // stamps the record with WHY, which is the field a report groups
            // by.  `overtime_timer` non-zero means this was an overtime period
            // running out rather than the match's own clock.
            OSP_Stats_AccuracyAll();
            OSP_Stats_MatchEnd(overtime_timer ? "overtime timelimit"
                                              : "timelimit");
            G_EndLevel();
            return;
        }
    } else if (connected_clients - botglobals.numbots <= 0 &&
               level.time > 3600) {
        // An hour with nobody on it: end the level so the rotation moves and a
        // server left running does not sit on one map for ever.  BOTS DO NOT
        // COUNT -- they never leave, so counting them makes a bot-filled server
        // one that never rotates again.
        ot_count = 0;
        gi.bprintf(PRINT_HIGH, "Inactive client timelimit hit.\n");
        sl_SoftGameEnd(&gi, level);
        OSP_Stats_MatchEnd("inactive client timelimit");
        G_EndLevel();
        return;
    }

    if (!fraglimit->value && !frag_offset)
        return;

    if (!OSP_IsTeams()) {
        for (i = 0; i < game.maxclients; i++) {
            if (!g_edicts[i + 1].inuse)
                continue;
            if (game.clients[i].resp.score >= fraglimit->value) {
                gi.bprintf(PRINT_HIGH, "Fraglimit hit.\n");
                sl_SoftGameEnd(&gi, level);
                OSP_Stats_AccuracyAll();
                OSP_Stats_MatchEnd("fraglimit");
                G_EndLevel();
                return;
            }
        }
    } else if (frag_offset) {
        if (OSP_teamFrags(0) != OSP_teamFrags(1)) {
            OSP_findTeamWinner();
            gi.bprintf(PRINT_HIGH, "We have a sudden-death winner!\n");
            sl_SoftGameEnd(&gi, level);
            OSP_Stats_AccuracyAll();
            OSP_Stats_MatchEnd("sudden death fraglimit");
            G_EndLevel();
        }
    } else if (OSP_teamFrags(0) >= fraglimit->value + frag_offset ||
               OSP_teamFrags(1) >= fraglimit->value + frag_offset) {
        OSP_findTeamWinner();
        gi.bprintf(PRINT_HIGH, "Team fraglimit hit.\n");
        sl_SoftGameEnd(&gi, level);
        OSP_Stats_AccuracyAll();
        OSP_Stats_MatchEnd("team fraglimit");
        G_EndLevel();
    }
}

/*
=================
OSP_EndLevel

R-OSP-9's third rotation, and the third time the answer is the EndLevel row
rather than a second rotation inside EndDMLevel.  `osp_maps.c`'s list decides;
an empty list falls back to baseq2's, which is one choice made in one place.
=================
*/
void OSP_EndLevel(void)
{
    edict_t *next;

    // Every camera and every entity the camera system was tracking goes now:
    // the level is over and the lists point at edicts that are about to be
    // reused.
    EnitityListClean();
    endlvl_frame = level.framenum;

    if (hs_mode && !manual_map)
        OSP_updateHighScores();

    // The dmflag outranks the rotation, and the split is why that has to be
    // said here: `port_osp:g_main.c`'s `EndDMLevel` tests "same map" and only
    // reaches `NextMap()` when the test is false, and lifting the rotation into
    // this row put those two in separate functions.  The guard is here and the
    // ANSWER stays in `EndDMLevel()`, which opens with the same test -- one
    // site owns it, for this row and for `RA_EndLevel` (R-RA-12).
    //
    // `manual_map != 1` is the donor's clause and is the whole difference from
    // arena's, which has no such state: somebody typed `map`, or carried a map
    // vote, and a dmflag does not outrank a command.  The three writers of
    // `manual_map = 1` each name a map through `selected_map` immediately
    // before ending the level, and `NextMap()` is its only reader.
    if (!(((int)dmflags->value & DF_SAME_LEVEL) && manual_map != 1)) {
        next = NextMap();
        if (next) {
            G_BeginIntermission(next);
            return;
        }
    }

    EndDMLevel();
}

const ruleset_ops_t ops_tourney = {
    .name              = "tourney",
    .CheckRules        = OSP_CheckRules,
    .EndLevel          = OSP_EndLevel,
    .ScoreboardMessage = OSP_ScoreboardMessage,
    // BeginIntermission, SelectSpawnPoint and ClientPlaced stay dm's: tourney
    // places players through OSP_startObserve() and its own team spawns, which
    // p_client.c reaches directly, and its intermission is EndLevel's.
    // R-MODE-6 makes a NULL row inherit rather than crash.
};

/*
=================
OSP_spawnRefused

"Is somebody standing on this spot?", and tourney answers it by REFUSING the
spawn rather than by telefragging whoever is there.

The donor's SelectSpawnPoint returns false when a player is within 60 units of
the chosen spot, and PutClientInServer then leaves the client frozen and
bodiless -- `osp_r240` 0 -- which R-191's respawn trigger retries on the next
think.  The two only make sense together: the refusal is safe BECAUSE the retry
exists, and the retry has something to retry BECAUSE placements can be refused.

Two exclusions that PlayersRangeFromSpot does not make, both the donor's and both
load-bearing rather than tidy:

  * the client being placed is not measured against ITSELF.  Its body is still
    wherever it was observing from, and an observer parked near the spot it is
    about to be given would refuse its own spawn -- forever, since the retry
    would find the same spot and the same body.
  * an OBSERVER is not a player.  `resp.osp_entered != ENTERED_ENTERED` is
    exactly the test, and without it one client hovering over a spawn point
    blocks it for everybody.  It is the same finding R-RA-4 records for arena's
    farthest-spawn one ruleset over, made by the donor for its own reason.

60.0 is a double in the donor and the comparison is written the way the
decompilation has it, `60.0 > range`, which is not the same as `range < 60.0f`
for a NaN and is free otherwise.
=================
*/
bool OSP_spawnRefused(edict_t *ent, edict_t *spot)
{
    edict_t *player;
    vec3_t   v;
    float    best = 9999999;
    int      n;

    if (ent->client->resp.osp_entered != ENTERED_ENTERED)
        return false;

    for (n = 1; n <= game.maxclients; n++) {
        player = &g_edicts[n];

        if (!player->inuse || !player->client || player == ent ||
            player->client->resp.osp_entered != ENTERED_ENTERED)
            continue;

        if (player->health <= 0)
            continue;

        VectorSubtract(spot->s.origin, player->s.origin, v);
        if (VectorLength(v) < best)
            best = VectorLength(v);
    }

    return 60.0 > best;
}

/*
=================
OSP_teamLost

"Is this client on the losing side?", which the end-of-level music asks and
nothing else does.  `osp_m124` is the match result the donor writes per team --
2 is the loser -- and team 2 is "no team at all", which the donor also gives the
loser's tune to: an observer did not win it either.
=================
*/
bool OSP_teamLost(int team)
{
    return team < 0 || team > 1 || osp_teams[team].osp_m124 == 2;
}

/*
=================
OSP_GibCount

`numgibs` (default 4), which p_client.c's two gib loops ask for.  Registered,
documented, clamped by nothing and read by nobody until R-193: the default
matches baseq2's literal 4, which is exactly why the omission was invisible --
the cvar worked on every server that left it alone.
=================
*/
int OSP_GibCount(void)
{
    int n;

    if (!G_IsOspRuleset() || !numgibs)
        return 4;

    n = (int)numgibs->value;
    // The donor takes the value as it stands; a negative one is a loop that
    // does not run, and a huge one is an entity flood, so it is bounded here for
    // the same reason R-VER-11 bounds every other operator number.
    return Q_clip(n, 0, 32);
}

// R-OSP-1's fast respawn, as a function so that g_items.c's SetRespawn stays
// one line of tourney.  The donor computes it inline there; the arithmetic and
// the three cvars are tourney's, so they live here.
//
// The shape: a full-strength server (`players` at fast_maxpbound) multiplies
// the delay by fast_respawn, an empty one leaves it alone, and everything
// between interpolates.  fast_respawn is clamped up to 0.05 rather than
// rejected, which is the donor's own guard against a division that would make
// every item instant.
float OSP_respawnDelay(float delay)
{
    int players;

    players = active_clients;
    if (players < (int)fast_minpbound->value)
        players = (int)fast_minpbound->value;
    if (players > (int)fast_maxpbound->value)
        players = (int)fast_maxpbound->value;

    if (fast_respawn->value < 0.05f)
        gi.cvar_set("fast_respawn", "0.05");
    if (fast_maxpbound->value < 1.0f)
        return delay;

    return delay * (1.0f - (1.0f - fast_respawn->value) *
                    (float)players / fast_maxpbound->value);
}

/*
=================
R-185: THE TWO READS THE IMPORT DROPPED

Both cvars below were registered by the merge and read by nobody, so an operator
could set them and nothing happened.  Upstream reads both, and the reads are
what came across as blanks:

  * `respawn_delay` -- `osp-tourney/p_client.c:2625` adds it to the forced-respawn
    test.  Our `p_client.c` kept the DF_FORCE_RESPAWN arm and dropped the term.
  * `power_armor_screen` / `power_armor_shield` -- `osp-tourney/g_combat.c:202`
    and `:212` assign them to `damagePerCell`, which the merge left as the
    hardcoded 1 and 2.

BOTH ARE GATED ON RegularDM, because upstream gates them on `!m_mode` and
`m_mode` is the donor's match mode -- the selector R-OSP-12 flattened into
`g_ruleset`, where 0 is `RULESET_DM`.  Outside RegularDM the donor uses the fixed
ratios, so a tournament cannot be re-balanced by a cvar mid-series.

NEITHER IS MEASURABLE FROM A BOT TEST, and R-185 records why rather than
claiming one: `respawn_delay` is only on the forced-respawn arm and a bot holds
attack, so it respawns through the button arm regardless; and the ratios' clamp
only fires for a player holding power armour, which no map with an `.aas` places
and `start_armortype` cannot grant.

*** WITH THE DEFAULTS, NEITHER CHANGES ANYTHING. *** `respawn_delay` defaults to
0, and 0 frames makes the added test identical to the one already above it;
the two ratios default to exactly the 1 and 2 that were hardcoded.  That is the
point: a faithful restoration of a dropped read is invisible until somebody sets
the cvar, which is precisely why nobody noticed it was missing.
=================
*/

// Seconds, into frames.  *** UPSTREAM DOES NOT SCALE THIS. ***
// `osp-tourney/p_client.c:2625` writes `client->respawn_framenum +
// resp_delay->value` -- a FRAME count plus a value its own documentation calls
// "an allowable delay (in seconds)".  At 10 fps that makes `respawn_delay 1`
// mean a tenth of a second, so the cvar is off by BASE_FRAMERATE for its whole
// documented range.  This is the unit-bug class `tools/units.py` exists for and
// the same one R-CTF-1 found in the flag return; §7 rule 1 takes the reading
// that matches the donor's own documentation, so the seconds are seconds here.
int OSP_forcedRespawnDelay(void)
{
    // `resp_delay` is NULL until OSP_gameInit has run, and this is reached from
    // p_client.c's ClientThink -- guarded for the same reason the sibling below
    // guards its pair, not because a live server can get here first.
    if (!G_IsOspRuleset() || !resp_delay)
        return 0;

    return (int)(resp_delay->value * BASE_FRAMERATE);
}

// Cells-per-point-of-damage for the two power armours.  `screen` picks which.
//
// The clamp is upstream's and is a RESET rather than a clamp: a value above 2
// does not become 2, it becomes the default.  Kept, because an operator who
// typed 5 gets a defined answer either way and this is the one the donor gives.
float OSP_powerArmorPerCell(bool screen)
{
    cvar_t *cv = screen ? power_armor_screen : power_armor_shield;

    if (G_Ruleset() != RULESET_DM || !cv)
        return screen ? 1.0f : 2.0f;

    if (cv->value > 2.0f)
        gi.cvar_set(screen ? "power_armor_screen" : "power_armor_shield",
                    screen ? "1.0" : "2.0");

    return cv->value;
}

// Choose which member of a respawn team comes back, skipping the ones a referee
// has switched off (R-OSP-1).  g_items.c's DoRespawn calls this instead of
// walking the chain itself: the skip has to happen when COUNTING as well as
// when choosing, or the random index points past the survivors and the walk
// runs off the end of the chain.  Returns NULL when every member is disabled,
// which means nothing respawns.
edict_t *OSP_pickRespawnMember(edict_t *master)
{
    edict_t *ent;
    int     count, choice;

    for (count = 0, ent = master; ent; ent = ent->chain)
        if (!OSP_disableItems(ent))
            count++;

    if (!count)
        return NULL;

    choice = Q_rand_uniform(count);

    for (count = 0, ent = master; ent; ent = ent->chain) {
        if (OSP_disableItems(ent))
            continue;
        if (count == choice)
            return ent;
        count++;
    }

    return master;
}

// "Is any member of this respawn team still enabled?"  droptofloor asks it so
// that a disabled member of a team that still has enabled members is left alone
// -- the team respawns as a team, and taking one member out of the rotation is
// OSP_pickRespawnMember's job, not the spawner's.
bool OSP_teamHasEnabled(edict_t *master)
{
    edict_t *ent;

    for (ent = master; ent; ent = ent->chain)
        if (!OSP_disableItems(ent))
            return true;

    return false;
}

/*
=================
OSP_worldspawn

Everything the donor does in SP_worldspawn, in one call so that the shared file
holds one gate rather than forty lines of tourney (R-MODE-5).

Ordering inside is the donor's and two parts of it matter.  The hi-score table
is read from disk before the MOTD is built, because the MOTD can quote it.  And
the team name configstrings go out before OSP_teamReset(), which zeroes the
scores those names are drawn beside -- the other order shows last match's
numbers under this match's names for one frame.

The configstring indices are OSP_CS(5), (7), (9) and (10): the donor spells them
0x625, 0x627, 0x629 and 0x62a, which is CS_GENERAL_OLD plus the offset, and
R-81 is why they are not spelled that way here.
=================
*/
void OSP_worldspawn(void)
{
    char    buf[64];
    size_t  i;

    if (server_log) {
        char date[64];

        OSP_getDateInfo(date);
        OSP_logAdminLog("Map: %s (%s)", level.mapname, date);
    }

    OSP_initHighScores();

    // The mode used to be re-read here, because `match_mode` was not latched and
    // a referee's mid-match `set match_mode 3` had to reach the next map through
    // the clamp rather than around it.  `g_ruleset` IS latched (R-MODE-1), so
    // there is nothing to re-read and no window in which the announced mode and
    // the running one can differ -- which is what R-OSP-13 existed to police.
    sync_stat = OSP_IsMatch() ? 0 : 8;

    OSP_setMOTD();
    overtime_timer = 0;
    frag_offset = 0;

    if (OSP_IsTeams()) {
        Q_snprintf(buf, sizeof(buf), "%15s", osp_teams[0].greenname);
        gi.configstring(OSP_CS(5), buf);
        Q_snprintf(buf, sizeof(buf), "%15s", osp_teams[1].greenname);
        gi.configstring(OSP_CS(7), buf);
        OSP_teamReset();
    }

    // The two banner lines the HUD draws, both in the alternate charset: Quake
    // II renders a byte with the high bit set as the green glyph for that
    // character, which is how the mod gets coloured text out of one font.
    Q_strlcpy(buf, "OSP Tourney DM v(2.75)", sizeof(buf));
    for (i = 0; i < strlen(buf); i++)
        buf[i] |= 128;
    gi.configstring(OSP_CS(9), buf);

    Q_strlcpy(buf, match_endinfo->string, sizeof(buf));
    for (i = 0; i < strlen(buf); i++)
        buf[i] |= 128;
    gi.configstring(OSP_CS(10), buf);
}

/*
=================
OSP_levelSpawned

The tail of SpawnEntities: the parts that need the world to exist.  Called from
the tourney arm there, beside the rune spawner and the two logs.
=================
*/
void OSP_levelSpawned(void)
{
    console_stampcount = 0;

    // `player_reload` restores the player list across a level change, which is
    // what keeps a match's scores and team memberships through a map vote.
    if ((int)gi.cvar("player_reload", "0", 0)->value)
        OSP_playerlist_svcmd();
}

/*
=================
OSP_frameStart

The per-frame work the donor does at the top of G_RunFrame, after the clock has
advanced (R-OSP-1).  Five independent things, none of which is a rule check --
OSP_CheckRules is the ops row for that.
=================
*/
void OSP_frameStart(void)
{
    if ((int)console_timestamp->value && console_stampcount < level.framenum) {
        console_stampcount = level.framenum +
                             (int)console_timestamp->value * 600;
        OSP_consoleStamp();
    }

    if (!level.intermission_framenum)
        OSP_updateClock();

    // A vote that nobody answered fails on its own rather than hanging.
    if (vote_inprogress && level.framenum > vote_frametime &&
        !level.intermission_framenum) {
        gi.bprintf(PRINT_HIGH, "Time up. Vote failed. No changes made.\n");
        OSP_Stats_Vote("Fail", NULL, NULL);
        OSP_clearVotes();
        OSP_closeMenus();
    }

    if (OSP_IsTeams())
        OSP_updateTeamFrags();

    // Before the match is live, watch for the conditions that start it.
    if (sync_stat < 4 && !level.intermission_framenum)
        OSP_checkSync();
}

/*
=================
OSP_frameEnd

The pause TRANSITION, which is the last thing in the donor's frame: a pause
asked for during this frame (`match_paused == 1`) takes effect after it, so the
frame that requested it still completes.  Freezing mid-frame would leave half
the entities thought and half not.
=================
*/
void OSP_frameEnd(void)
{
    edict_t *ent;
    int     i;

    if (match_paused != 1)
        return;

    match_paused = 2;

    if (who_paused == -1 || who_paused == -3)
        gi.configstring(OSP_CS(1), "Pause");
    else if (who_paused == -2)
        gi.configstring(OSP_CS(1), " Wait");

    for (i = 1; i <= game.maxclients; i++) {
        ent = g_edicts + i;
        if (!ent->inuse || !ent->client)
            continue;

        ent->client->ps.pmove.pm_type = PM_FREEZE;
        ent->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
    }
}

// Give every client their movement back.  Three of the four ways out of a pause
// need it, so it is one function.
// `queued` false skips the clients the donor skips on the reconnect-timeout
// path: `osp_entered` above 2 is the 1-vs-1 queue (3) and the autocam (16), and
// both of those set PMF_NO_PREDICTION for a reason of their own that outlives
// the pause.  Clearing it for them put a queued player's prediction back on
// while they were still standing in the audience.
static void OSP_unfreezeAll(bool queued)
{
    edict_t *ent;
    int     i;

    for (i = 1; i <= game.maxclients; i++) {
        ent = g_edicts + i;
        if (!ent->inuse || !ent->client)
            continue;
        if (!queued && ent->client->resp.osp_entered > 2)
            continue;
        ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
    }
}

/*
=================
OSP_pauseFrame

What happens while the world is frozen (`match_paused >= 2`): no entity thinks,
no time passes, and the only thing that moves is the countdown.

Four states, told apart by `who_paused`:
  -1  a plain pause with no timer -- it ends when somebody unpauses
  -2  waiting for a disconnected player to come back; the match is terminated
      if they do not
  -3  an admin is reading the stats; a notice every ten seconds
  otherwise, a player's timeout, counted down and shown as "TO nn"

`match_paused == 3` is the separate five-second restart countdown after a
timeout runs out.  Reaching intermission clears all of it: a pause cannot
survive the end of the level.
=================
*/
void OSP_pauseFrame(void)
{
    edict_t *ent;
    int     i, secs;

    if (level.intermission_framenum) {
        match_paused = 0;
        who_paused = -1;
        end_timeout = -1;
        OSP_unfreezeAll(true);
        return;
    }

    if (match_paused == 3) {
        char    message[64];

        if (end_timeout == -1)
            end_timeout = 51;
        end_timeout--;

        if (!end_timeout) {
            match_paused = 0;
            who_paused = -1;
            OSP_unfreezeAll(true);
            gi.bprintf(PRINT_CHAT, "**** MATCH HAS RESTARTED!! ****\n");
            end_timeout = -1;
            return;
        }

        if (!(end_timeout % 10)) {
            Q_snprintf(message, sizeof(message), "Match restarting in %d %s\n",
                       end_timeout / 10,
                       end_timeout / 10 == 1 ? "second!" : "seconds.");
            for (i = 1; i <= game.maxclients; i++) {
                ent = g_edicts + i;
                if (!ent->inuse || !ent->client)
                    continue;
                gi.centerprintf(ent, "%s", message);
                stuffcmd(ent, "play misc/secret.wav\n");
            }
        }
        return;
    }

    if (who_paused == -1)
        return;

    if (who_paused == -3) {
        secs = (int)pause_time;
        if (pause_time - secs < FRAMETIME && !(secs % 10)) {
            for (i = 1; i <= game.maxclients; i++) {
                ent = g_edicts + i;
                if (!ent->inuse || !ent->client)
                    continue;
                gi.centerprintf(ent, "Admin is viewing stats.  Please Wait.\n");
            }
        }
        pause_time -= FRAMETIME;
        return;
    }

    if (who_paused == -2) {
        secs = (int)pause_time;
        if (pause_time - secs < FRAMETIME && !(secs % 10)) {
            char message[128];

            Q_snprintf(message, sizeof(message),
                       "Waiting for %s to reconnect.\n(%d seconds)\n",
                       reconn_player, secs);
            for (i = 1; i <= game.maxclients; i++) {
                ent = g_edicts + i;
                if (!ent->inuse || !ent->client)
                    continue;
                gi.centerprintf(ent, "%s", message);
            }
        }
        pause_time -= FRAMETIME;
        if (pause_time < FRAMETIME) {
            match_paused = 0;
            who_paused = -1;
            OSP_unfreezeAll(false);
            gi.bprintf(PRINT_HIGH, "No reconnect. Match terminated.\n");
            OSP_checkHalt(reconn_index);
        }
        return;
    }

    // A player's timeout: "TO nn" in the match-state panel, ticking down.
    secs = (int)pause_time;
    if (pause_time - secs < FRAMETIME) {
        char message[8];

        Q_snprintf(message, sizeof(message), "TO %.2d", secs);
        gi.configstring(OSP_CS(1), message);
    }
    pause_time -= FRAMETIME;
    if (pause_time < FRAMETIME)
        match_paused = 3;
}

/*
=================
OSP_exitLevel

The exit-intermission path.  True means "handled, do not call ExitLevel".

Two of its three arms restart the match in place rather than changing level, so
that a `dmflags` same-level match or a server that has emptied out does not have
to reload the map: OSP_endClean() puts the match state back to warmup and every
client is begun again.  The third is the ordinary exit, which still needs
OSP_endClean() first so the next level does not inherit this match's state.
=================
*/
bool OSP_exitLevel(void)
{
    edict_t *ent;
    int     i;

    OSP_serverbotsRemove();

    // R-OSP-1: an overtime count belongs to the match that ran it, not to the
    // next one.  Reset on EVERY exit, not only on the two timelimit arms of the
    // rules row -- a match that went to overtime and then ended on the team
    // fraglimit was handing its count to the match after it.
    ot_count = 0;

    // A CONFIG VOTE CHANGES THE MAP LIST, and the new list only exists now.
    // OSP_config_vote sets manual_map = 2, queues `exec <config>` through
    // gi.AddCommandString and ends the level; the exec runs during the
    // intermission, so this is the first moment the voted-for configuration is
    // actually in force.  Re-reading `maps.txt` here and picking from THAT is
    // what makes the vote change the rotation -- without this arm the next
    // level came from the map list of the configuration being replaced.
    if (manual_map == 2) {
        OSP_loadMaps();
        ent = NextMap();
        gi.AddCommandString(va("map %s\n",
                               ent ? ent->map : level.mapname));
        return true;
    }

    // Same level again, with somebody still on the server: restart in place.
    // BOTS DO NOT COUNT as somebody: they never leave, so a bot-filled server
    // would restart this level for ever instead of moving on.
    if (((int)dmflags->value & DF_SAME_LEVEL) && manual_map != 1 &&
        level.framenum < 64000 &&
        connected_clients - botglobals.numbots > 0) {
        OSP_endClean();
        level.changemap = NULL;
        level.exitintermission = 0;
        level.intermission_framenum = 0;
        ClientEndServerFrames();
        botglobals.numbots = 0;
        OSP_Stats_GameInit();
        sl_GameStart(&gi, level);
        // Free play has no ready gate, so the restarted match is live at once
        // and the log has to say so; every other ruleset opens its match record
        // when the countdown ends instead.
        if (G_Ruleset() == RULESET_DM)
            OSP_Stats_MatchStart();
        OSP_consoleStamp();

        for (i = 0; i < game.maxclients; i++) {
            ent = g_edicts + 1 + i;
            if (!ent->inuse)
                continue;
            if (ent->health > ent->client->pers.max_health)
                ent->health = ent->client->pers.max_health;
            ent->client->resp.score = ent->client->pers.score = 0;
            ent->client->resp.osp_r030 = 0;
            CTFPlayerResetGrapple(ent);
            ClientBegin(ent);
        }

        // Everything that thinks gets one frame to put itself back, which is
        // how the items return without a map reload.  A disabled item class
        // stays disabled.
        ent = g_edicts + game.maxclients + 1;
        for (i = game.maxclients + 1; i < globals.num_edicts; i++, ent++) {
            if (!ent->inuse || !ent->think)
                continue;
            if ((!ent->team || ent == ent->teammaster) && !OSP_disableItems(ent))
                ent->nextthink = level.framenum - 1;
        }
        return true;
    }

    // Empty server on a voted config: go back to the default one.
    if ((int)vote_config_default->value &&
        vote_config_defaultname->string[0] &&
        strcmp(vote_config_defaultname->string, "default") &&
        strcmp(__current_config->string, "default") &&
        !(connected_clients - botglobals.numbots)) {
        OSP_endClean();
        gi.cvar_set("__current_config", "default");
        gi.dprintf("Changing back to default config: %s\n",
                   vote_config_defaultname->string);
        gi.AddCommandString(va("exec %s\n", vote_config_defaultname->string));
        gi.AddCommandString(va("map %s\n", level.mapname));
        return true;
    }

    OSP_endClean();
    return false;
}

/*
=================
OSP_clientBeginPre

Everything the donor does in ClientBeginDeathmatch BEFORE the client is placed
(R-OSP-1, R-OSP-2).

Most of it is stufftext: the mod asks the client for four things it cannot know
otherwise -- whether they hold a referee password, their default team name and
skin, their default join code, and the hook aliases.  Each is a `cmd _...`
whose answer arrives back as a client command.  A bot answers none of them,
which is why every one is guarded by FL_BOT.

`resp.osp_r210` is "this client is coming back to a slot they already had", set
by OSP_recoverClient at connect time.  Almost everything here is conditional on
it being CLEAR -- a returning player keeps their score, their team and their
place in the queue.
=================
*/
void OSP_clientBeginPre(edict_t *ent)
{
    gclient_t *cl = ent->client;

    // R-OSP-3: the connect record, here rather than lazily from the first event
    // that needs one, so the log's order matches the game's.  `osp_r2a8` is the
    // "already announced" latch and it lives in resp, which the InitClientResp
    // immediately above has just cleared for a client that is not recovering.
    if (!cl->resp.osp_r2a8) {
        cl->resp.osp_r2a8 = 1;
        OSP_Stats_PlayerConnect(ent);
    }

    if (OSP_IsMatch() && !ent->osp_e39c && !(ent->flags & FL_BOT))
        stuffcmd(ent, "cmd _is_referee $ref_status $ref_passwd\n");

    if (OSP_IsTeams() && !cl->resp.osp_r210 && !ent->osp_e3a0[0] &&
        !(ent->flags & FL_BOT)) {
        ent->osp_e3a0[0] = 0;
        ent->osp_e3b0[0] = 0;
        if (!(int)team_lockskin->value)
            stuffcmd(ent, "cmd _default_team_info "
                     "$default_teamname $default_teamskin\n");
        OSP_observerTeamFrags(ent);
    }

    if (G_Ruleset() == RULESET_TDM && !cl->resp.osp_r210 && !(ent->flags & FL_BOT)) {
        cl->resp.osp_r07d[0] = 0;
        stuffcmd(ent, "cmd _default_join_code $default_joincode\n");
    }

    if (!(ent->flags & FL_BOT))
        OSP_hookAliases(ent);

    if (!cl->resp.osp_r210) {
        cl->resp.osp_entered = ENTERED_OBSERVER;
        cl->resp.osp_r240 = 0;
    } else if (cl->resp.osp_entered == ENTERED_ENTERED) {
        active_clients++;
        OSP_DoRankSort();
    }
}

/*
=================
OSP_clientBegunPost

Everything after placement.  True means the client has been DISCONNECTED and the
caller must return without touching it again -- which happens when a match is
already running and `match_latejoin` forbids joining it.  The player is told how
much time is left before the connection is closed, so the answer is "come back
in four minutes" rather than a silent drop.
=================
*/
bool OSP_clientBegunPost(edict_t *ent)
{
    gclient_t *cl = ent->client;

    connected_clients++;
    cl->resp.osp_r0a0 = -1;
    cl->resp.osp_r2ac = -1;
    cl->resp.osp_r09c = 0;
    cl->resp.osp_r0b0 = (int)client_muzzlemode->value;

    if (!cl->resp.osp_r210) {
        // -100 rather than 0: a player who has not entered the match sorts
        // below everyone who has, and the scoreboard shows a blank instead of
        // a score.
        cl->resp.score = -100;
        cl->resp.osp_r248 = 0;
        cl->resp.osp_r0ac = level.framenum;
        cl->resp.osp_r24c = 0;
        cl->showscores = false;
        if (OSP_IsTeams())
            cl->resp.team = 2;          // 2 is "no team", 0 and 1 are the teams
        cl->resp.osp_r204 = OSP_initID();

        if (G_Ruleset() == RULESET_DUEL && OSP_teamCount(0) && OSP_teamCount(1))
            cl->resp.osp_r02c = 1;
    }

    if (!OSP_IsTeams()) {
        OSP_DoRankSort();
        OSP_showFrags(ent);
    }
    sl_WriteStdLogPlayerEntered(&gi, level, ent);

    if (sync_stat == 4 && !(int)match_latejoin->value &&
        !ent->osp_e39c && !(ent->flags & FL_BOT)) {
        int mins, secs;

        mins = (int)(timelimit->value + overtime_timer -
                     (level.framenum - sync_frame) / 600) - 1;
        secs = (int)((overtime_timer + timelimit->value) * 60 -
                     (level.framenum - sync_frame) / 10) - mins * 60 - 1;
        if (secs == 60) {
            secs = 0;
            mins++;
        } else if (mins < 0) {
            secs = 0;
            mins = 0;
        }
        gi.cprintf(ent, PRINT_HIGH,
                   "Match already started.\nTime left in match: %d:%.2d\n",
                   mins, secs);
        gi.WriteByte(svc_disconnect);
        gi.unicast(ent, true);
        ClientDisconnect(ent);
        return true;
    }

    OSP_setShowParams();
    cl->resp.osp_r210 = 0;
    OSP_zeroRuneStats(ent);

    // The ping and framerate checks only apply to real clients, and a value of
    // -1 is how the donor spells "not being watched".
    cl->resp.osp_r1fc = (((int)client_minping->value ||
                          (int)client_maxping->value) &&
                         !(ent->flags & FL_BOTCLIENT)) ? 0 : -1;
    cl->resp.osp_r024 = (client_maxframes &&
                         !(ent->flags & FL_BOTCLIENT)) ? 0 : -1;

    cl->resp.osp_r0d4 = level.framenum + 60;

    // R-OSP-4: arm the speed watch, or mark this client as one that is never
    // sampled.  16 is the donor's "not watched" value and it is what
    // ClientBeginServerFrame tests; a bot has no console to answer
    // `_init_state` from, so it is never a candidate.
    if (bot_watch && !(ent->flags & FL_BOT)) {
        cl->resp.osp_r2b8 = 0;
        cl->resp.osp_r2b4 = level.framenum + 100;
    } else {
        cl->resp.osp_r2b8 = 16;
    }

    return false;
}

/*
=================
OSP_botJoin / OSP_botReady

R-OSP-11's behaviour half, which is Phase 7's because it needs a bot to exist.

A client under tourney connects as an OBSERVER -- OSP_clientBeginPre sets
`entered` to ENTERED_OBSERVER for everyone -- and enters the game by pressing a
key: `join` in modes 0 and 1, the team menu in mode 2, the queue in mode 3.  A
bot presses nothing.  Neither the donor's bot layer nor the 1999 brain sends
`join`: the brain's whole client-command vocabulary is say / say_team / use /
drop / invuse / invdrop / wave plus EA_Command, and nothing in `bl_*.c` or
`bots.cfg` issues one either.  So bots under `osp-tourney` connect, sit in the
audience and are not counted -- which is what they did here too, silently,
because R-58's `entered`/`osp_entered` split had been applied to most of the
tree and not to CheckMinimumPlayers' accessor, and a `bool` reads ENTERED_
OBSERVER (2) as equal to ENTERED_ENTERED (1).

The join goes through the donor's own entry points rather than assigning
`entered` here, because entering is nine other assignments as well -- the
enterframe stamp, the score restore, the accuracy row, the team member list and
active_clients among them -- and every one of the donor's four entry paths does
all of them.

1v1 is deliberately left alone: OSP_clientBeginLevel already calls OSP_1v1Add
for mode 3, so a bot takes its place in the spectator queue and enters when the
queue reaches it, which is R-OSP-12's rule and not something a bot may skip.
=================
*/
void OSP_botJoin(edict_t *ent)
{
    if (!(ent->flags & FL_BOT))
        return;
    if (ent->client->resp.osp_entered != ENTERED_OBSERVER)
        return;

    // OSP_startObserve is the toggle behind the `observe` command and it is
    // the ONE function that handles all four modes: it asks OSP_1v1AllowJoin
    // under mode 3, OSP_addTeamMember(ent, 2) under 2 and 3 -- 2 being the
    // donor's "pick a side for me", which balances, honours a locked or full
    // team and consults OSP_defaultTeam -- and under 0 and 1 simply enters.
    // Then it does the nine other assignments entering is: the enterframe
    // stamp, the score restore, active_clients, the rank sort, the stats row.
    //
    // Picking the side here instead was the first attempt and it put all four
    // bots on "Visitors": OSP_teamCount only counts clients that have ENTERED,
    // so before the join every count is 0 and the balance always answers 0.
    // The donor's own argument does not have that problem because it runs
    // inside the join.
    OSP_startObserve(ent);
}

/*
The ready-up half.  `bots_warmuptime` is documented as "the amount of time
before a bot will ready up in qualifier, teamplay or 1v1 modes.  If 0, a bot
will automatically ready itself when all other real clients have moved to ready
status."  Both halves are implemented here.

THE DONOR DOES IMPLEMENT THE FIRST HALF, contrary to what
doc/reconciliation.md R-105 used to say: `osp-tourney`'s ClientThink opens with

    if (ent->flags & FL_BOT) {
        if (resp.entered != ENTERED_ENTERED)
            OSP_startObserve(ent);
        else if (sync_stat < 4 && !resp.osp_r20c && bots_warmuptime->value &&
                 resp.enterframe + bots_warmuptime->value * 10 < level.framenum)
            OSP_ready_cmd(ent, 2);
    }

-- which is the join (OSP_botJoin above) and this, both of them.  Three things
here are deliberately the donor's and one is deliberately not:

  * THE CLOCK IS PER BOT, from `resp.enterframe`, not absolute from the start of
    the level.  A bot that joins late gets its own `bots_warmuptime` rather than
    being ready the moment it arrives.
  * `quiet` is 2, which is what suppresses both the "%s is ready!" broadcast and
    the rewrite of both teams' WARMUP cells.  A bot readying up is not news, and
    with four of them it is four lines of chat nobody typed.
  * a bot that has left the game re-joins, which is the `entered !=
    ENTERED_ENTERED` arm and belongs to OSP_botJoin.
  * `bots_warmuptime` 0 means "never" in the donor, because its condition tests
    the cvar.  The documented meaning is "when all other real clients are
    ready", and that half is implemented below -- it is what makes
    OSP_ready_cmd's own "everybody left is a bot, start without waiting them
    out" shortcut reachable at all.

Mode 0 has no ready gate at all (`sync_stat` starts at 8), which is why this
runs only for modes 1..3.
*/
void OSP_botReady(void)
{
    edict_t *ent;
    int      i, humans, humansready;
    bool     due;

    if (level.framenum & 31)
        return;

    // A BOT THAT IS NOT IN THE GAME IS PUT BACK IN IT.  The donor asks this on
    // every frame of every bot's ClientThink, and it is not only about the
    // first join: the inactivity rule, a forfeited team and a referee's
    // `kickplayer` all end with a bot sitting in the audience, and nothing else
    // would ever bring it back.  Outside the ready gate below because it
    // applies in free play too, where there is no ready gate at all.
    for (i = 1; i <= game.maxclients; i++) {
        ent = g_edicts + i;
        if (ent->inuse && ent->client && (ent->flags & FL_BOT))
            OSP_botJoin(ent);
    }

    if (!OSP_IsMatch() || sync_stat >= 4)
        return;

    if ((int)bots_warmuptime->value) {
        // Per bot, from the frame it entered -- see above.  Decided inside the
        // loop below rather than here, because each bot has its own answer.
        due = true;
    } else {
        // "when all other real clients have moved to ready status", which is
        // vacuously true on a server with no humans -- and that is the right
        // reading: OSP_ready_cmd has its own "everybody left is a bot, start
        // without waiting them out" shortcut, so the mod already intends a
        // bot-only match to start.  What it does about nobody being there is
        // `bots_noclients`, which removes the bots; it is not this.
        humans = humansready = 0;
        for (i = 1; i <= game.maxclients; i++) {
            ent = g_edicts + i;
            if (!ent->inuse || !ent->client || (ent->flags & FL_BOT))
                continue;
            if (ent->client->resp.osp_entered != ENTERED_ENTERED)
                continue;
            humans++;
            if (ent->client->resp.osp_r20c)
                humansready++;
        }
        due = humans == humansready;
    }

    if (!due)
        return;

    for (i = 1; i <= game.maxclients; i++) {
        ent = g_edicts + i;
        if (!ent->inuse || !ent->client || !(ent->flags & FL_BOT))
            continue;
        if (ent->client->resp.osp_entered != ENTERED_ENTERED)
            continue;
        if (ent->client->resp.osp_r20c)
            continue;
        if ((int)bots_warmuptime->value &&
            ent->client->resp.enterframe +
            (int)bots_warmuptime->value * 10 >= level.framenum)
            continue;
        OSP_ready_cmd(ent, 2);
    }
}

/*
=================
OSP_clientBeginLevel

The ClientBegin half: a client that is already connected and is arriving on a
new level, or coming back after a disconnect.  Separate from the two above
because it runs on EVERY level load, where those run once per connection.
=================
*/
void OSP_clientBeginLevel(edict_t *ent)
{
    gclient_t *cl = ent->client;

    if (!cl->resp.osp_r210) {
        OSP_giveClientID(ent);

        // The accuracy row remembers where the player was playing from, so a
        // report can tell two people with the same name apart.  p_acc[] is a
        // global and survives the InitClientResp the caller is about to run;
        // resp.clientid survives it too, deliberately (see InitClientResp).
        if (cl->resp.clientid >= 0 &&
            cl->resp.clientid < (int)q_countof(p_acc))
            Q_strlcpy(p_acc[cl->resp.clientid].osp_a010, ent->osp_e37c,
                      sizeof(p_acc[cl->resp.clientid].osp_a010));
    } else {
        // R-OSP-3: a player who took their seat back is a reconnect, not a new
        // player -- the log has to say so or the report counts them twice.
        OSP_Stats_PlayerReconnect(ent);
        // ...and the camera system tracks them again.  Without this a
        // reconnecting player was invisible to the autocam for the rest of the
        // map: EntityListRemove took them out on the way down and nothing put
        // them back.
        EntityListAdd(ent);
    }

    if (cl->resp.osp_entered == ENTERED_ENTERED) {
        if (G_Ruleset() == RULESET_TDM) {
            // A standing invitation back onto the team they were on, so a
            // locked or full team still lets its own player back in.  Team + 1
            // because 0 has to keep meaning "no invitation".
            if (sync_stat > 2)
                cl->resp.osp_r078 = cl->resp.team + 1;
            OSP_readdTeamMember(ent);
            OSP_initTeamFrags(ent);
        } else if (G_Ruleset() == RULESET_DUEL) {
            OSP_readdTeamMember(ent);
        }
    }

    // THE MATCH WAS WAITING FOR THIS PLAYER, and only for this one.  The other
    // side has been playing a man down since they dropped; `reconn_player` is
    // whose name ClientDisconnect wrote there, and matching it is what stops
    // any other arrival ending a pause that is not theirs to end.
    if (cl->resp.osp_r210 && OSP_IsTeams() && who_paused == -2 &&
        !Q_stricmp(cl->pers.netname, reconn_player)) {
        who_paused = -1;
        match_paused = 3;
        gi.bprintf(PRINT_CHAT, "%s has returned!\nMatch continues.\n",
                   reconn_player);
        reconn_player[0] = 0;
    }

    if (G_Ruleset() == RULESET_DUEL)
        OSP_1v1Add(ent);
}

/*
=================
OSP_clientLeaving / OSP_clientLeft

The two halves of tourney's disconnect (R-OSP-1), split where the SPINE has to
do its own work in between: the first runs before the edict is torn down and
needs the client still linked and still on its team, the second after, and
recounts the server.

The recount at the end is not bookkeeping for its own sake.  `active_clients`
drives the fast-respawn scaling, the ready percentage and the match-halt check,
and the donor recomputes it by walking every slot rather than decrementing --
which is what makes it survive a client that left without ever entering.

Returns true from the first half when the match has been PAUSED to wait for the
player to come back, which is a state the caller must not disturb.
=================
*/
void OSP_clientLeaving(edict_t *ent, int *out_team)
{
    gclient_t *cl = ent->client;
    int       state, tno;

    if (server_log) {
        char when[64];

        OSP_getDateInfo(when);
        OSP_logAdminLog("Disconnect: %s (%s)%s", cl->pers.netname, when,
                        (ent->flags & FL_BOTCLIENT) ? " [SERVER_BOT]" : "");
    }

    state = cl->resp.osp_entered;
    if (G_Ruleset() == RULESET_DUEL)
        OSP_1v1Remove(ent, 1);
    if (rune_stat)
        OSP_deadDropRune(ent);

    if (state == ENTERED_ENTERED) {
        EntityListRemove(ent);
        tno = cl->resp.team;
        active_clients--;
        if (active_clients < 0)
            active_clients = 0;

        if (OSP_IsTeams()) {
            if (tno != 2)
                OSP_removeTeamMember(ent, true);
            if (cl->resp.osp_r20c)
                OSP_notready_cmd(ent, true);
        }
    } else {
        tno = 2;
    }

    sl_LogPlayerDisconnect(&gi, level, ent);
    OSP_Stats_PlayerLeave(ent);
    // R-OSP-3: their accuracy row, while the client is still addressable.  Not
    // at intermission, where OSP_Stats_AccuracyAll has already written it.
    if (!level.intermission_framenum)
        OSP_Stats_Accuracy(ent);
    OSP_playerAnnounce(ent, 10);

    *out_team = tno;
}

bool OSP_clientLeft(edict_t *ent, int tno)
{
    gclient_t *cl = ent->client;
    edict_t   *p;
    int       i, connected, active, bots;

    cl->osp_t00c = 0;
    cl->osp_t040 = 0;
    cl->resp.osp_r0f4[0] = 0;

    // A player who leaves mid-match can be held a seat, name and score for
    // `team_recovertime` seconds -- that is what makes the pause below worth
    // having.  Otherwise every trace of them goes now.
    if ((int)client_recover->value && cl->resp.osp_entered == ENTERED_ENTERED &&
        sync_stat > 2 && !cl->resp.osp_r07c[0] &&
        !level.intermission_framenum && !(ent->flags & FL_BOTCLIENT)) {
        Q_strlcpy(cl->resp.osp_r214, cl->pers.netname, sizeof(cl->resp.osp_r214));
        cl->resp.osp_r018 = level.framenum;
        OSP_saveClient(ent);
    } else {
        cl->resp.osp_entered = ENTERED_OBSERVER;
        cl->resp.score = 0;
        cl->resp.team = 2;
        cl->resp.osp_r214[0] = 0;
        cl->resp.osp_r018 = 12345678;
        cl->resp.osp_r07d[0] = 0;
        ent->osp_e3a0[0] = 0;
        ent->osp_e3b0[0] = 0;
    }

    if (G_Ruleset() == RULESET_DUEL && (int)team_nextuptime->value)
        cl->resp.team = 2;

    for (i = 1; i <= game.maxclients; i++) {
        p = g_edicts + i;
        if (!p->inuse || !p->client || p->client->chase_target != ent)
            continue;
        gi.cprintf(p, PRINT_HIGH, "Target disconnected.\n");
        OSP_removeChaseCam(p);
    }

    OSP_DoRankSort();

    // The last player on one side of a live team match leaving pauses it,
    // rather than handing the other side a win they did not play for.
    if (sync_stat > 2 && !level.intermission_framenum &&
        !(ent->flags & FL_BOTCLIENT) && (int)client_recover->value &&
        (int)team_duelrecover->value && (int)team_recovertime->value &&
        OSP_IsTeams() && tno != 2 && !cl->resp.osp_r07c[0] &&
        !OSP_teamCount(tno) && OSP_teamCount(1 - tno)) {
        char message[64];

        who_paused = -2;
        Q_strlcpy(reconn_player, cl->pers.netname, sizeof(reconn_player));
        reconn_index = tno;
        pause_time = team_recovertime->value;
        match_paused = 1;
        Q_snprintf(message, sizeof(message),
                   "Waiting for %s to reconnect.\n(%d seconds)\n",
                   reconn_player, (int)pause_time);

        for (i = 1; i <= game.maxclients; i++) {
            p = g_edicts + i;
            if (!p->inuse || !p->client || p == ent ||
                (p->flags & FL_BOTCLIENT))
                continue;
            gi.centerprintf(p, "%s", message);
        }
        return true;
    }

    // A vote about the player who just left is over.
    if (vote_inprogress && !level.intermission_framenum &&
        !(ent->flags & FL_BOTCLIENT)) {
        if (vote_item == 0x1000 &&
            cl->resp.clientid == Q_atoi(vote_value)) {
            gi.bprintf(PRINT_HIGH, "%s left on own accord.  Vote terminated.\n",
                       cl->pers.netname);
            OSP_Stats_Vote("Fail", "Player manually left", NULL);
            OSP_clearVotes();
            OSP_closeMenus();
        } else {
            OSP_checkVote();
        }
    }

    // Recount rather than decrement: a slot that never entered, a bot, and a
    // client still in the connect handshake all have to come out the same way.
    connected = active = bots = 0;
    for (i = 1; i <= game.maxclients; i++) {
        p = g_edicts + i;
        if (p->inuse && p->client && p->client->pers.connected) {
            connected++;
            if (p->client->resp.osp_entered == ENTERED_ENTERED)
                active++;
            if (p->flags & FL_BOTCLIENT)
                bots++;
        }
    }
    connected_clients = connected;
    active_clients = active;
    // The bot half of the same recount.  `bots_votedin` is how many of them a
    // VOTE put here, and it cannot exceed how many there are -- a bot that
    // dropped for any other reason would otherwise leave the counter claiming
    // a bot the `removebots` vote can no longer find.
    botglobals.numbots = bots;
    if (bots_votedin > bots)
        bots_votedin = 0;

    gi.bprintf(PRINT_HIGH, "%s wimped out and left. (clients = %i)\n",
               cl->pers.netname, active_clients);
    cl->pers.netname[0] = 0;
    Info_SetValueForKey(cl->pers.userinfo, "skin", "");
    // The brain keeps a settings row per client slot; the slot is free now and
    // the row has to say so before something reuses it.
    BotLib_BotClientSettings(ent);

    if (OSP_IsTeams())
        OSP_checkHalt(tno);
    else if (G_Ruleset() == RULESET_DMPRO)
        OSP_checkHalt(2);

    // THE LAST HUMAN LEFT.  Bots a vote put here are the vote's, not the
    // server's, so with nobody left to have voted they go too -- unless
    // `bots_noclients` says to keep a bot-only server running.
    if (!(ent->flags & FL_BOTCLIENT) &&
        !(active_clients - botglobals.numbots) &&
        !(int)bots_noclients->value && bots_votedin) {
        for (i = 0; i < bots_votedin; i++)
            BotServerCommand("sv", "removebot", NULL);
        bots_votedin = 0;
    }

    return false;
}

/*
=================
OSP_userinfoChanged

Tourney's half of ClientUserinfoChanged (R-OSP-1, R-OSP-4), run before the spine
copies the name and skin out of the userinfo it is handed -- because what this
does is EDIT that userinfo in place, and the spine reading it afterwards is what
makes the edit stick.

Four things, and each is a rule about what a player may call themselves:

  * a name the player list forbids, or a name changed again inside
    `client_infochange` seconds, is refused and the old one put back.  The
    second is not vanity policing: a player who renames every frame makes the
    scoreboard unreadable for everyone else, and the userinfo-key-order auto-ban
    R-OSP-4 names came from the same place.
  * a real rename is written to both logs and to the admin log.
  * `pers.greenname` is the name with the high bit set on every byte, which is
    how the mod draws it in the alternate charset; it is rebuilt here so that
    nothing else has to remember to.
  * in 1-vs-1 a player IS their team, so the team name and its configstring
    follow the rename.
=================
*/
void OSP_userinfoChanged(edict_t *ent, char *userinfo)
{
    gclient_t *cl = ent->client;
    char      *s;
    char      newnick[16];
    size_t    i;
    int       tnum;

    // R-OSP-4's rate cap, and it is edited into the userinfo the caller is
    // about to copy out, like everything else here.  A bot has no connection to
    // rate-limit; `pers.connected` false is the first ClientUserinfoChanged of
    // a connect, where there is nobody to tell yet.
    if ((int)client_maxrate->value && !(ent->flags & FL_BOTCLIENT)) {
        s = Info_ValueForKey(userinfo, "rate");
        if ((int)client_maxrate->value < Q_atoi(s)) {
            if (cl->pers.connected)
                gi.cprintf(ent, PRINT_HIGH,
                           "*** Server max rate capped at %s\n",
                           client_maxrate->string);
            Info_SetValueForKey(userinfo, "rate", client_maxrate->string);
        }
    }

    s = Info_ValueForKey(userinfo, "name");

    // Refused: keep what they had.
    if (OSP_playerAllow(s, userinfo) ||
        (!OSP_IsMatch() && ent->osp_infochange_framenum > level.framenum)) {
        Info_SetValueForKey(userinfo, "name", cl->pers.netname);
        s = Info_ValueForKey(userinfo, "name");
    }

    if (cl->pers.netname[0]) {
        Q_strlcpy(newnick, s, sizeof(newnick));
        if (Q_stricmp(cl->pers.netname, newnick) && cl->resp.osp_r2a8) {
            OSP_Stats_PlayerRename(ent, cl->pers.netname);
            sl_LogPlayerRename(&gi, cl->pers.netname, s, level.time);
            if (server_log)
                OSP_logAdminLog("Rename: %s -> %s", cl->pers.netname, s);
        }
    }

    if (Q_stricmp(cl->pers.netname, s)) {
        // The rename cooldown, in an int.  The donor stamps it on `charname`
        // and casts a frame number to and from a `char *`; R-79 carried that
        // as-is and added `osp_infochange_framenum` for it, then never wired
        // the field -- so the cast stayed and the field sat dead.  The cast
        // survives LP64 by luck, not by contract: nothing stops a later reader
        // treating that member as the string its type says it is, and the
        // three other `char *` beside it ARE strings.
        if ((int)client_infochange->value)
            ent->osp_infochange_framenum = level.framenum +
                                           (int)client_infochange->value * 10;
        else
            ent->osp_infochange_framenum = 0;

        Q_strlcpy(cl->pers.netname, s, sizeof(cl->pers.netname));

        if (cl->resp.clientid >= 0 &&
            cl->resp.clientid < (int)q_countof(p_acc)) {
            Q_strlcpy(p_acc[cl->resp.clientid].netname, s,
                      sizeof(p_acc[cl->resp.clientid].netname));
        }

        memset(cl->pers.greenname, 0, sizeof(cl->pers.greenname));
        for (i = 0; i < strlen(cl->pers.netname) &&
             i < sizeof(cl->pers.greenname) - 1; i++)
            cl->pers.greenname[i] = cl->pers.netname[i] + 128;

        tnum = cl->resp.team;
        if (G_Ruleset() == RULESET_DUEL && cl->resp.osp_entered == ENTERED_ENTERED &&
            !cl->resp.osp_r210 && !level.intermission_framenum &&
            tnum >= 0 && tnum < (int)q_countof(osp_teams)) {
            char buf[24];

            Q_strlcpy(osp_teams[tnum].netname, cl->pers.netname,
                      sizeof(osp_teams[tnum].netname));
            Q_strlcpy(osp_teams[tnum].greenname, cl->pers.greenname,
                      sizeof(osp_teams[tnum].greenname));

            Q_snprintf(buf, sizeof(buf), "%15s", osp_teams[tnum].greenname);
            gi.configstring(OSP_CS(5) + tnum * 2, buf);

            if (ent->inuse && !(ent->flags & FL_BOT)) {
                Q_snprintf(buf, sizeof(buf), "%15s", osp_teams[tnum].netname);
                OSP_clientConfigString(ent, OSP_CS(5) + tnum * 2, buf);
            }
        }
    }

    // THE SKIN IS THE SERVER'S UNDER THREE RULES, and `resp.osp_r0f4` is the
    // one in force -- what the player asked for is only ever a proposal.
    //
    //   * a qualifier match can force every player onto one skin so that the
    //     round is watched rather than the models;
    //   * a team match owns the skin outright, and so does any team ruleset
    //     with `team_lockskin` on: you wear your team's, which is what makes a
    //     team readable at a glance.  `team` 2 is "no team", so an observer
    //     keeps their own.
    //
    // Anything else leaves the player's own choice alone.
    tnum = cl->resp.team;
    if (sync_stat == 4 && G_Ruleset() == RULESET_DMPRO && (int)qualifier_forceskins->value) {
        Info_SetValueForKey(userinfo, "skin", qualifier_skinname->string);
        Q_strlcpy(cl->resp.osp_r0f4, qualifier_skinname->string,
                  sizeof(cl->resp.osp_r0f4));
    } else if (tnum >= 0 && tnum < (int)q_countof(osp_teams) &&
               ((OSP_IsTeams() && (int)team_lockskin->value) ||
                G_Ruleset() == RULESET_TDM)) {
        Info_SetValueForKey(userinfo, "skin", osp_teams[tnum].skin);
        Q_strlcpy(cl->resp.osp_r0f4, osp_teams[tnum].skin,
                  sizeof(cl->resp.osp_r0f4));
    } else {
        Q_strlcpy(cl->resp.osp_r0f4, Info_ValueForKey(userinfo, "skin"),
                  sizeof(cl->resp.osp_r0f4));
    }
}

/*
=================
OSP_clientAllowed

The player-list check on the connect path (R-OSP-1, R-OSP-4).  False means the
connection is refused and `rejmsg` says why.

The four refusals are the donor's and they are the reason the mod has a player
list at all: a name already on the server, a name that is not allowed to play, a
wrong password for a reserved name, and a banned address.  Each gets its own
message, because "connection refused" with no reason is what makes a player try
again forever.

R-OSP-4's `strcpy` of the client address is fixed here rather than carried: the
donor copies an unbounded `ip` userinfo value into a 1024-byte stack buffer and
then into `ent->osp_e37c`, which is 32.
=================
*/
bool OSP_clientAllowed(edict_t *ent, char *userinfo)
{
    char        msg[128];
    const char  *name;
    int         idx;

    name = Info_ValueForKey(userinfo, "name");
    idx = OSP_playerAllow((char *)name, userinfo);
    if (!idx)
        return true;

    if (idx < 0)
        Q_snprintf(msg, sizeof(msg), "%s is already connected.", name);
    else if (idx == 1)
        Q_snprintf(msg, sizeof(msg), "%s is not allowed to play.", name);
    else if (idx == 2)
        Q_snprintf(msg, sizeof(msg), "Incorrect password/address for %s", name);
    else
        Q_strlcpy(msg, "Your address has been banned!", sizeof(msg));

    Info_SetValueForKey(userinfo, "rejmsg", msg);
    return false;
}

/*
=================
OSP_clientConnected

The tail of the connect path: remember where they came from, and say so on the
console and in the admin log.  `osp_e37c` is the address with the port stripped,
which is what the player list matches a reserved name against.
=================
*/
void OSP_clientConnected(edict_t *ent, char *userinfo)
{
    char *colon;

    if (ent->flags & FL_BOTCLIENT) {
        Q_strlcpy(ent->osp_e37c, "SERVER_BOT", sizeof(ent->osp_e37c));
    } else {
        Q_strlcpy(ent->osp_e37c, Info_ValueForKey(userinfo, "ip"),
                  sizeof(ent->osp_e37c));
        colon = strchr(ent->osp_e37c, ':');
        if (colon)
            *colon = 0;
    }

    gi.dprintf("(%s connected from %s)\n", ent->client->pers.netname,
               ent->osp_e37c);

    if (server_log) {
        char date[64];

        OSP_getDateInfo(date);
        OSP_logAdminLog("Connect: %s - %s (%s)", ent->osp_e37c,
                        ent->client->pers.netname, date);
    }

    // R-OSP-4: the first arming, half a second sooner than the one at the end
    // of ClientBeginDeathmatch -- a client that connects and never enters is
    // still sampled once.
    if (bot_watch && !(ent->flags & FL_BOT)) {
        ent->client->resp.osp_r2b8 = 0;
        ent->client->resp.osp_r2b4 = level.framenum + 50;
    } else {
        ent->client->resp.osp_r2b8 = 16;
    }

    ent->osp_e39c = 0;
}
