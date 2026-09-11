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
// osp_types.h -- OSP Tourney DM's own aggregate types.
//
// They live in the donor's g_local.h, which Colosseum has exactly one of
// and which every translation unit includes.  These six are read by
// `src/tourney/` alone, so they belong on this side of the seam -- the same
// argument that applies to the menu engines.
//
// `osp_team_t` is prefixed `osp_team_t`: Rocket Arena already has a `osp_team_t` in
// src/arena/arena.h, they are different structs for two mods' notions of a
// team, so the collision is resolved by prefix rather than by rename.

#ifndef OSP_TYPES_H
#define OSP_TYPES_H

#include "tourney/osp_hooks.h"

// The four statusbar literals the donor keeps are not here and are not anywhere:
// the bar is composed from the slot map, and tourney is the ruleset that
// proves why -- its four bars differ only in where two panels sit, which two
// booleans express and four literals cannot share.

// The donor defines this below hs_player_t, which uses it; here the struct
// and the macro are in one file, so the macro comes first.
#define OSP_HS_FIELD    16

// The mod's eleven status configstrings.  The donor hardcodes them 0x620..0x62a,
// which is CS_GENERAL_OLD + 0..10 -- correct only while the server runs the OLD
// configstring remap.  This library is compiled with USE_PROTOCOL_EXTENSIONS and
// negotiates at runtime, so the number has to come from game.csr like
// every other one in this tree.
#define OSP_CS(n)       (game.csr.general + (n))

// The donor defines this below hs_player_t, which uses it; here the struct and
// the macro are in one file, so the macro has to come first.

// The per-player accuracy/damage table behind the accuracy report,
// indexed by client->resp.clientid.  Member names <invented>.  Every update
// site is gated by `sync_stat > 2`.
#define ACC_BLASTER         0
#define ACC_SHOTGUN         1
#define ACC_SSHOTGUN        2
#define ACC_MACHINEGUN      3
#define ACC_CHAINGUN        4
#define ACC_GRENADELAUNCHER 5
#define ACC_ROCKET          6
#define ACC_HYPERBLASTER    7
#define ACC_RAILGUN         8
#define ACC_BFG             9
#define ACC_GRENADE         10

// The content layers' nine, appended.
//
// The donor's report has eleven columns because osp-tourney's itemlist has
// eleven weapons.  This tree's has Xatrix's and Ground Zero's too, and
// both layers are valid with every ruleset -- so a `tdm` match on a
// Reckoning or Ground Zero map is fought with weapons the report could not see:
// `acc_column()` returned -1 for every one of their MODs and the layer fire
// functions called `OSP_accShot` from nowhere at all.  What a player got was
// total damage given and taken (those are credited before the column lookup)
// and accuracy for nothing.
//
// APPENDED, never interleaved: 0..10 keep the numbers, the names and the order
// the donor gave them, so an ngLog parser pointed at the stats file reads the
// same eleven keys and simply gains new ones when a layer weapon is used.
//
// The Disruptor gets a column even though `weapon_disintegrator` is
// IT_NOT_GIVEABLE under Ground Zero's own KILL_DISRUPTOR: a map may still
// place one, the report only prints a row that has shots in it, and a column
// that stays empty costs four ints.  The A-M Bomb does not: `ammo_nuke` is
// IT_POWERUP, and a powerup that happens to do damage is not a weapon -- which
// is what -1 goes on meaning.
#define ACC_RIPPER          11      // Ionripper       -- Xatrix
#define ACC_PHALANX         12      // Phalanx         -- Xatrix
#define ACC_TRAP            13      // Trap            -- Xatrix
#define ACC_ETF_RIFLE       14      // ETF Rifle       -- Ground Zero
#define ACC_PROX            15      // Prox Launcher   -- Ground Zero
#define ACC_HEATBEAM        16      // Plasma Beam     -- Ground Zero
#define ACC_CHAINFIST       17      // Chainfist       -- Ground Zero
#define ACC_TESLA           18      // Tesla           -- Ground Zero
#define ACC_DISRUPTOR       19      // Disruptor       -- Ground Zero

#define ACC_COUNT           20

typedef struct {
    char    netname[16];        // ClientUserinfoChanged writes[15] = 0
    char    osp_a010[MAX_CLIENT_ADDRESS];   // invented name; copied from
                                            // client->pers.address
    int     dgiven;
    int     dtaken;
    int     shots[ACC_COUNT];
    int     hits[ACC_COUNT];
    int     given[ACC_COUNT];
    int     taken[ACC_COUNT];
} p_acc_t;

// The two teams.  Each carries its name twice: plain, and with 0x80 added to
// every byte, which is how Quake II's charset renders it green.
typedef struct {
    char    netname[32];
    char    greenname[32];      // name reconstructed
    char    skin[128];
    byte    osp_m0c0[32];
    char    joincode[16];       // name reconstructed
    int     osp_m0f0;
    int     osp_m0f4;           // non-zero = team is locked
    int     osp_m0f8;           // the team's frag total
    int     osp_m0fc;
    int     osp_m100;
    int     osp_m104;
    int     osp_m108;
    int     osp_m10c;
    int     osp_m110;           // last frag total pushed to the clients
    int     osp_m114;
    int     osp_m118;           // the fraglimit that went with it
    int     osp_m11c;
    int     osp_m120;
    int     osp_m124;
} osp_team_t;

// id CTF's `loc_t`, unchanged.
typedef struct {
    char    *classname;
    int     priority;
} loc_t;

// osp_maps.c's map queue entry (type name reconstructed).  `map` is a realloc'd
// array, one entry per maps.txt line: `<map name> [min players] [max players]`.
typedef struct {
    int     minplayers;
    int     maxplayers;
    int     used;
    char    name[64];
} map_t;

// The accuracy report's row table: which p_acc_t.shots/hits column each
// printed row names.  Type and member names reconstructed.
typedef struct {
    int     index;          // a weapon index into p_acc_t.shots/hits
    char    name[128];
} a_info_t;

typedef struct {
    char    name[OSP_HS_FIELD];
    char    score[OSP_HS_FIELD];
    char    date[OSP_HS_FIELD];
    int     isnew;          // 1 = set during this map, drawn with a '*'
} hs_player_t;

// ---------------------------------------------------------------- the mod
//
// OSP's own globals, cvars and constants.  They live in the donor's g_local.h;
// Colosseum has exactly one of those and every translation unit
// includes it, so tourney's 300-odd private names would be in scope everywhere.
// They belong on this side of the seam for the same reason the types above do.
//
// `osp_teams` keeps its donor prefix: Rocket Arena already has
// a `teams`, in src/arena/arena.c, and they are two mods' team tables.  The
// PREFIX is on the identifier only -- the rename that introduced it also ran
// over comments and string literals, and thirteen player-facing messages, a
// menu row and one stats-log JSON key said `osp_teams` to the player until 1.37.
//-------------------------------------------------------------
// Gladiator Bot SDK feature switches.
//#define BOT_DEBUG                 // debug lines / bounding boxes

//LINUX?
//-------------------------------------------------------------

// The Gladiator Bot SDK's library interface.  No <errno.h> here: bl_main.c
// declares a local spelled `errno`, which needs glibc's macro out of scope.

// Gladiator Bot SDK flags.
// The mod's own "this client is a bot" flag.  <invented name>.

// Two popup-menu layout slots.  <invented names>.

// The values of resp.osp_entered.
#define ENTERED_ENTERED         1

// The bits of `rune_stat`, the cached `runes_enable` value.  <invented names>.
#define RUNE_RESIST             1
#define RUNE_STRENGTH           2
#define RUNE_HASTE              4
#define RUNE_REGEN              8
#define RUNE_VAMPIRE            16

// The rune item class -- id CTF's own IT_TECH value.
// IT_RUNE is g_local.h's now: it is an item class, shared with the item code.
    // OSP: five more, appended.  The last four are the Gladiator Bot SDK's;
    // `botlib` is this mod's own.

extern  int     paused;

// OSP rune subsystem.

// The mod's own subsystems.
// g_utils.c compares item->use against these two, so they stay external

// not const: OSP_parseString() rewrites all three from the armor_* cvars
extern gitem_armor_t    jacketarmor_info;
extern gitem_armor_t    combatarmor_info;
extern gitem_armor_t    bodyarmor_info;
// The five `sv <name>` handlers ServerCommand dispatches to.

// id CTF's grapple tuning, from its g_ctf.h.

// How many of each rune are loose in the world, indexed by `item->quantity -
// STAT_RUNE_RESIST`.
extern  int     r_count[5];

// The rune spawn pool.
extern  int     rune_spawncount;
extern  edict_t *rune_spawnpoint[50];
// The once-per-map latch OSP_setupRuneSpawn tests and sets.
extern  int     runespawn;
extern  cvar_t  *runes_model;
extern  cvar_t  *runes_flash;
extern  cvar_t  *runes_min;
extern  cvar_t  *runes_max;
extern  cvar_t  *runes_perplayer;
extern  cvar_t  *runes_resist;
extern  cvar_t  *runes_strength;
extern  cvar_t  *runes_regen_hmax;
extern  cvar_t  *runes_regen_amax;
extern  cvar_t  *runes_vampire;
extern  cvar_t  *runes_vampire_max;

// The cached `runes_enable` bitmask; see the RUNE_* bits above.
extern  int     rune_stat;


// ---------------------------------------------------------------------------
// The mod's logging. Two independent local logs, each its own TU:
//   osp_stats.c -- the JSON-lines game-event / statistics log (`OSP_Stats_*`)
//   stdlog.c    -- the "Standard Log" 1.2 format, with its writer in
//                  sl_write.c
// v2.75's NetGames USA stack (nglog.c, ngmark.c, q2log.c and the RFC 1321 MD5
// reference it used to sign ngWorldStats logs) is gone; osp_stats.h says why.
// ---------------------------------------------------------------------------
extern  int     sl_status;   // 0 = off, 1 = open, 2 = already open
extern  cvar_t  *sl_log_logbots;
extern  cvar_t  *sl_log_style;
extern  cvar_t  *sl_filename;
extern  cvar_t  *sl_log_flush;
extern  cvar_t  *sl_log_method;

// The two teams.  Each carries its name twice: plain, and with 0x80 added to
// every byte, which is how Quake II's charset renders it green.

extern  osp_team_t  osp_teams[2];

extern  int     sync_stat;
extern  int     active_clients;
extern  p_acc_t p_acc[256];

// osp_acc.c -- the two entry points the spine calls instead of writing p_acc
// at fifteen sites.  `mod` is the means of death the shot or the damage was
// tagged with; the ACC_ column is derived here.
void     OSP_accShot(edict_t *self, int mod, int count);
void     OSP_accDamage(edict_t *targ, edict_t *attacker, int mod, int take);

// Not id CTF's 34.

// baseq2 cvars Q2PRO's InitGame registers.  tourney has its own observer and
// map-rotation systems, so nothing here reads them -- they are kept because
// they still show up in serverinfo where a client or a stats tool may look.

// The mod's own cvars, declared as the vanilla-derived files come to need them.
extern  cvar_t  *camera_depth;
extern  cvar_t  *damage_railgun;
extern  cvar_t  *match_type;
extern  cvar_t  *client_protect;
extern  cvar_t  *team_hurtteam;
extern  cvar_t  *team_hurtself;
extern  cvar_t  *fast_minpbound;
extern  cvar_t  *fast_maxpbound;
extern  cvar_t  *fast_respawn;
extern  cvar_t  *stats_logallpickups;
extern  cvar_t  *hook_initdamage;
extern  cvar_t  *numgibs;

// fire_hit is gone with the monster melee code -- see g_weapon.c.

// static in baseq2 now, but tourney calls all three from its own files

    // OSP: 16 bytes more than vanilla.  The netname is carried twice: plain,
    // and with 0x80 added to every byte (green).  `greenname` is <invented>.

    // OSP: `game_helpchanged`/`helpchanged` moved into client_respawn_t.

    // OSP: also a strike counter -- OSP_speedCheat_cmd increments it and
    // OSP_speedDetect tests it against 3.  In the original this was vanilla's
    // `qboolean`, an int-sized enum, so counting worked.  Q2PRO's conversion to
    // C99 `bool` would saturate it at 1 and make the `>= 3` test unreachable,
    // so this one field stays an int.

// OSP: the pop-up menu engine is id's Q2 CTF p_menu.c.  CTF declares these in
// p_menu.h, but p_menu.c includes only g_local.h, so they live here.

// id CTF's grapple states, renumbered 1/2/4.

// The mod's pop-up menus.
extern  osp_pmenu_t Team_Menu[18];
extern  osp_pmenu_t RegDM_Menu[18];
extern  osp_pmenu_t AdminMain_Menu[17];
extern  osp_pmenu_t AdminSelect_Menu[17];
extern  osp_pmenu_t Vote_Menu[19];
extern  osp_pmenu_t Vote_Menu2[18];
extern  osp_pmenu_t Bot_Menu[18];
extern  osp_pmenu_t Proposal_Menu[18];
extern  osp_pmenu_t Proposal_Menu2[18];
extern  osp_pmenu_t Help_Menu[18];
extern  osp_pmenu_t Help2_Menu[18];
extern  osp_pmenu_t Help3_Menu[18];
extern  osp_pmenu_t Invite_Menu[18];

extern  int     vote_inprogress;
extern  int     match_paused;
extern  int     who_paused;
extern  float   pause_time;
extern  int     item_settings;
// `server_log` is a FILE *, not a cvar.
extern  FILE    *server_log;
extern  cvar_t  *match_strictmode;

extern  cvar_t  *vote_threshold;

// The join/leave helpers osp_observe.c and p_camera.c share.

// ---------------------------------------------------------------------------
// The mod's own globals, en masse.
// ---------------------------------------------------------------------------

// id CTF's `loc_t`, unchanged.

// osp_maps.c's map queue entry (type name reconstructed).  `map` is a realloc'd
// array, one entry per maps.txt line: `<map name> [min players] [max players]`.

extern  map_t   *map;

extern  int     end_timeout;
extern  int     ot_count;
extern  int     conf_size;
extern  int     blink_on_count;
extern  int     blink_off_count;
extern  cvar_t  *runes_enable;  // 4-byte cvar_t* -- OSP_endClean reloads rune_stat from it
extern  int     bots_delaytime;
extern  int     bots_loadstat;
extern  int     client_maxframes;
extern  int     console_stampcount;
extern  int     maxconn_clients;
extern  int     reconn_index;
extern  int     bot_watch;
extern  int     game_init;
extern  int     sync_startframe;
extern  float   sync_time;
extern  int     time_update;
extern  int     time_blink;
extern  int     start_suddendeath;
extern  int     vote_frametime;
extern  int     vote_item;
extern  int     vote_yea;
extern  int     vote_nay;
extern  char    wav_file[125];
// The accuracy report's row table: which p_acc_t.shots/hits column each
// printed row names.  Type and member names reconstructed.

// Unsized on purpose: the table is NULL-terminated and every reader walks it
// to the sentinel, so a row can be added in one place.  `a_info[10]`
// here meant three loops carried their own copy of the count and the BFG row
// the donor never added would have needed all four edited.
extern  a_info_t a_info[];
extern  int     motd_read;
extern  loc_t   loc_names[23];
extern  int     num_names;
extern  unsigned    map_size;   // unsigned
extern  int     selected_map;
extern  char    conf_info[50][64];
extern  char    conf_name[50][64];
extern  cvar_t  *qualifier_numspots;
extern  p_acc_t o_acc[256];
extern  cvar_t  *allow_id;
extern  cvar_t  *time_remaining;
extern  cvar_t  *start_armortype;
extern  char    old_scores[2048];
extern  cvar_t  *match_features;
extern  cvar_t  *max_shells;
extern  cvar_t  *max_bullets;
extern  cvar_t  *max_cells;
extern  cvar_t  *max_grenades;
extern  cvar_t  *max_rockets;
extern  cvar_t  *max_slugs;
extern  cvar_t  *max_health;
extern  cvar_t  *max_armor;
extern  cvar_t  *start_armor;
extern  cvar_t  *hook_holdplayertime;
extern  cvar_t  *team_duelrecover;
extern  cvar_t  *match_pausetime;
extern  cvar_t  *console_timestamp;
extern  cvar_t  *bots_warmuptime;
extern  int     max_items[11];
extern  cvar_t  *team_a_score;
extern  cvar_t  *demo_referee;
extern  cvar_t  *pack_shells;
extern  int     start_items[11];
extern  cvar_t  *pack_cells;
extern  cvar_t  *vote_time;
extern  cvar_t  *client_nomove;
extern  cvar_t  *vote_carryover;
extern  cvar_t  *match_countinfo;
extern  cvar_t  *start_shells;
extern  int     pack_spawn;
extern  cvar_t  *vote_bots_max;
extern  cvar_t  *referee_password;
extern  cvar_t  *team_a_hookcolor;
extern  cvar_t  *vote_enable_hook;
extern  cvar_t  *vote_enable_map;
extern  cvar_t  *power_armor_screen;
extern  cvar_t  *team_overtime_time;
extern  cvar_t  *vote_enable_time;
extern  cvar_t  *team_b_name;
extern  cvar_t  *map_halt;
extern  cvar_t  *osp_game;
extern  cvar_t  *vote_config_default;
extern  cvar_t  *demo_tag;
extern  cvar_t  *pack_slugs;
extern  cvar_t  *pack_health;
extern  cvar_t  *pack_grenades;
extern  cvar_t  *client_botdetect;
extern  cvar_t  *team_b_score;
extern  cvar_t  *team_a_skin;
extern  cvar_t  *hook_pullspeed;
extern  cvar_t  *resp_delay;
extern  char    conf_file[2048];
extern  int     p_order[28];
extern  cvar_t  *team_idteam;
extern  cvar_t  *warmup_armor;
extern  cvar_t  *team_b_skin;
extern  cvar_t  *hook_sky;
extern  cvar_t  *vote_countspectators;
extern  cvar_t  *pack_armor;
extern  cvar_t  *bots_autoload;
extern  cvar_t  *bots_botfile;
extern  cvar_t  *bots_minplayers;
extern  cvar_t  *bots_noclients;
extern  cvar_t  *client_recover;
extern  cvar_t  *client_maxping;
extern  cvar_t  *vote_enable_bots;
extern  cvar_t  *vote_enable_toggles;
extern  char    default_timelimit[8];
extern  cvar_t  *weapon_have;
extern  cvar_t  *armor_shard;
extern  cvar_t  *match_endinfo;
extern  int     level_start;
// ---- the loadout tables' shapes ----
//
// OSP_NUM_WEAPS is the length of the `weapon_initial` / `weapon_have` /
// `start_weap` family: the donor's ELEVEN baseq2 weapons and then the content
// layers' eight, appended because both layers are valid with every ruleset and
// the donor's bitmask had no bit for any of them.  The order is a CVAR
// CONTRACT -- bit k-1 of `weapon_have` is rank k -- so osp_main.c's
// `osp_weapnames[]` may be appended to and must never be reordered.
//
// The Disruptor is deliberately absent: `weapon_disintegrator` carries
// IT_NOT_GIVEABLE and no IT_WEAPON bit under Ground Zero's KILL_DISRUPTOR,
// so a loadout cannot hand it out and a bit for it would be a bit that lies.
#define OSP_NUM_WEAPS       19
#define OSP_NUM_AMMO        6       // start_items[0..5], the donor's
#define OSP_NUM_ARMOR       3       // start_items[8..10], the donor's

// The layers' five ammo types.  They get their OWN three arrays rather than
// slots appended to `start_items` / `max_items` / `pack_items`, because those
// three carry the donor's layout -- ammo at 0..5, health at 7, armour at 8..10,
// 6 dead -- and bolting a second meaning onto the gaps is how that goes wrong.
#define OSP_LAYER_MAGSLUG       0
#define OSP_LAYER_FLECHETTES    1
#define OSP_LAYER_PROX          2
#define OSP_LAYER_TESLA         3
#define OSP_LAYER_TRAP          4
#define OSP_NUM_LAYER_AMMO      5

extern  int     start_weap[OSP_NUM_WEAPS];
extern  cvar_t  *team_nextuptime;
extern  cvar_t  *referee_enable;
extern  char    default_hook[8];
extern  cvar_t  *client_muzzlemode;
extern  char    default_fraglimit[8];
extern  cvar_t  *menu_maxtime;
extern  cvar_t  *power_armor_shield;
extern  cvar_t  *match_readypercent;
extern  cvar_t  *vote_enable_config;
extern  cvar_t  *weapon_initial;
extern  cvar_t  *client_maxrate;
extern  cvar_t  *menu_timestep;
extern  cvar_t  *camera_pitch;
extern  cvar_t  *pack_rockets;
extern  cvar_t  *team_overtime_mode;
extern  cvar_t  *ffa_hurtself;
extern  cvar_t  *vote_enable;
extern  cvar_t  *armor_jacket;
extern  cvar_t  *menu_maxfrag;
extern  cvar_t  *demo_player;
extern  cvar_t  *hook_speed;
extern  cvar_t  *vote_enable_runes;
extern  cvar_t  *team_a_name;
extern  cvar_t  *start_cells;
extern  cvar_t  *client_maxfps;
extern  cvar_t  *bots_delayload;
extern  cvar_t  *team_b_hookcolor;
extern  cvar_t  *match_countdown;
extern  cvar_t  *match_timeouts;
extern  cvar_t  *start_grenades;
extern  cvar_t  *hook_color;
extern  cvar_t  *team_maxplayers;
extern  cvar_t  *start_health;
extern  cvar_t  *start_bullets;
extern  int     initial_weap;
extern  cvar_t  *start_rockets;
extern  cvar_t  *match_prestartpercent;
extern  cvar_t  *vote_config_defaultname;
extern  cvar_t  *__current_config;
extern  cvar_t  *hook_wait;
extern  cvar_t  *client_deathweapdrop;
extern  cvar_t  *team_overtime_count;
extern  char    reconn_player[32];
extern  cvar_t  *warmup_health;
extern  cvar_t  *hook_incdamage;
extern  cvar_t  *qualifier_forceskins;
extern  cvar_t  *armor_combat;
extern  cvar_t  *client_minping;
extern  int     pack_items[11];
extern  cvar_t  *team_recovertime;
extern  cvar_t  *client_infochange;
extern  cvar_t  *menu_fragstep;
extern  cvar_t  *team_lockskin;
extern  cvar_t  *match_latejoin;
extern  cvar_t  *hook_maxdamage;
extern  char    vote_value[64];
extern  cvar_t  *qualifier_skinname;
extern  cvar_t  *armor_body;
extern  int     pack_life;
extern  cvar_t  *match_startsound;
extern  cvar_t  *match_endmusic;
extern  cvar_t  *pack_bullets;
extern  cvar_t  *start_slugs;
extern  cvar_t  *hook_holdtime;
extern  cvar_t  *vote_enable_frag;
extern  cvar_t  *vote_enable_kick;
extern  char    match_motd[1024];
extern  char    match_info[1024];
extern  char    voted_botname[32];
extern  int     overtime_timer;
extern  char    pl_bname[200][16];
extern  char    pl_names[200][16];
extern  char    pl_pass[200][32];
extern  char    pl_addr[200][16];
extern  int     next_map;

// osp_hiscore.c (filename assigned by this tree). One entry of the per-map high
// score table.  Type and member names reconstructed.
// The three fields are drawn in fixed-width scoreboard columns, so they are
// short.  OSP_HS_FIELD is what the file parser and the date formatter write
// through, and it has to match.

extern  int         hs_mode;        // 1 = fraglimit/FPH, 2 = timelimit/frags
extern  int         hs_limit;
extern  hs_player_t p_table[10];
extern  char        hs_table[1400];
extern  cvar_t      *client_highscores;
extern  int         sync_frame;
extern  int         endlvl_frame;
extern  int         manual_map;

// osp_cmds.c (filename assigned by this tree). The client/vote/referee commands.
// FL_OSP_NOCMD is edict_t.flags bit 0x2000, the mod's own.  <invented name>.
// edict_t.flags bit 0x10000 -- the mod's "this client is a bot" flag.

// The three configstring slots the match status line uses.
#define CS_OSP_STATUS_DM    0x623
#define CS_OSP_STATUS_A     0x626
#define CS_OSP_STATUS_B     0x628

// item_settings bits, read off the vote handlers' and/or masks.
#define ITEM_SET_QUAD   1
#define ITEM_SET_BFG    8

// osp_main.c (filename assigned by this tree).

// osp_teams.c / osp_players.c (filenames assigned by this tree).

    // Moved here out of client_persistant_t; see the note there.

    // OSP: 736 bytes of the mod's own per-match state, at the end of the struct.
    // `osp_rNNN` is the field's byte offset inside this block and nothing more;
    // a field gets a real name only when something actually names it.
    // `char`, and a joincode string.
    // OSP_showFrags and OSP_setStats cache what they last pushed into the
    // status bar here, so an unchanged cell costs no network traffic.
    // Invented names: ClientThink's 16-sample ping accumulator.
    // `char`: a saved netname.

    // OSP: 136 bytes of new gclient_t state, at the end.  Same convention as the
    // client_respawn_t block above -- `osp_tNNN` is the byte offset inside this
    // block and is not a recovered name.  The three grapple members are id CTF's,
    // with the CTF prefix stripped.

extern  gclient_t   saved_clients[128];

    // ClientUserinfoChanged's client_infochange lockout.  v2.75 kept it in
    // the Gladiator SDK's `char *charname` and cast the frame number to and
    // from a pointer; this int is what carries it here, and the cast is gone
    // (the field was added for that and left unwired until then).

// ---------------------------------------------------------------- prototypes
//
// The mod's own functions.  Anything g_local.h already declares is not repeated
// here: one declaration, one place.
bool OSP_Pickup_Rune(edict_t *ent, edict_t *other);
void     OSP_Drop_Rune(edict_t *ent, const gitem_t *item);
bool OSP_runesHasHaste(edict_t *ent);
void     OSP_respawnRune(edict_t *ent);
void Use_Quad(edict_t *ent, const gitem_t *item);
void Use_Invulnerability(edict_t *ent, const gitem_t *item);
void     OSP_packPlayer(edict_t *ent);
void     OSP_seedPlayer(gclient_t *client);
bool OSP_disableItems(edict_t *ent);
void     OSP_allready_svcmd(void);
void     OSP_allnotready_svcmd(bool announce);
void     OSP_rmpause_cmd(void);
void     OSP_rstopmatch_cmd(edict_t *ent);
void     OSP_playerlist_svcmd(void);
// `BotCmd` was declared here, by the donor, with `char *` where the SDK's own
// bl_cmd.h says `const char *`.  The same shape a third time -- a name
// declared outside the header that owns it -- and this one had a signature that
// disagreed as well.  Nothing in src/tourney/ calls it; the two call sites are
// g_svcmds.c and g_cmds.c, and both include bot/bl_cmd.h.
void     PlayerDied(edict_t *ent);
void     PlayerResetGrapple(edict_t *ent);
void     ResetGrapple(edict_t *self);
void     GrappleTouch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf);
void     GrapplePull(edict_t *self);
void     FireGrapple(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int effect);
void     GrappleFire(edict_t *ent, const vec3_t g_offset, int damage, int effect);
void     Grapple_Fire(edict_t *ent);
void     OSP_hookAliases(edict_t *ent);
void     G_Spawn_Sparks(int type, vec3_t pos, vec3_t dir, vec3_t org);
const gitem_t *OSP_What_Rune(edict_t *ent);
void     OSP_runeThink(edict_t *self);
void     OSP_setupRuneSpawn(int delay);
int      OSP_runesApplyResistance(edict_t *ent, int damage);
int      OSP_runesApplyStrength(edict_t *ent, int damage);
bool OSP_runesApplyStrengthSound(edict_t *ent);
void     OSP_runesApplyHasteSound(edict_t *ent);
void     OSP_runesApplyRegeneration(edict_t *ent);
bool OSP_runesHasRegeneration(edict_t *ent);
bool OSP_runesHasVampire(edict_t *ent);
void     OSP_runesApplyVampire(edict_t *ent, int damage);
void     OSP_zeroRuneStats(edict_t *ent);
void     OSP_removeRunes(void);
int      OSP_findMinRune(void);
void     OSP_checkMinRunes(void);
void     OSP_runesShell(edict_t *ent);
bool OSP_checkMaxRunes(void);
int     sl_Logging(game_import_t *import, char *patch);
int     sl_OpenLogFile(game_import_t *import);
void    sl_GameStart(game_import_t *import, level_locals_t level);
void    sl_GameEnd(game_import_t *import, level_locals_t level);
void    sl_SoftGameEnd(game_import_t *import, level_locals_t level);
void    sl_LogMapName(game_import_t *import, char *mapname);
void    sl_LogGameStart(game_import_t *import, float time);
void    sl_LogVers(game_import_t *import);
void    sl_LogPatch(game_import_t *import, char *patch);
void    sl_LogDate(game_import_t *import);
void    sl_LogTime(game_import_t *import);
void    sl_LogDeathFlags(game_import_t *import, unsigned long flags);
void    sl_LogGameEnd(game_import_t *import, float time);
void    sl_CloseLogFile(void);
void    sl_LogPlayerLeft(game_import_t *import, char *name, float time);
void    sl_logWrite(char *line);
const char *OSP_teamNameFor(int team);
void Cmd_InvUse_f(edict_t *ent);
void Cmd_Kill_f(edict_t *ent);
void PMenu_Open(edict_t *ent, osp_pmenu_t *entries, int cur, int num);
void PMenu_Close(edict_t *ent);
void PMenu_Update(edict_t *ent);
void PMenu_Next(edict_t *ent);
void PMenu_Prev(edict_t *ent);
void PMenu_Select(edict_t *ent);
bool OSP_botDetect(edict_t *ent, usercmd_t *ucmd);
void     OnBotDetection(edict_t *ent, char *why);
void     OSP_speedCheat_cmd(edict_t *ent);
void     OSP_logAdminLog(char *fmt, ...);
void     OSP_speedDetect(edict_t *ent);
void    OSP_teamMenu(edict_t *ent);
void    OSP_DMMenu(edict_t *ent);
void    OSP_adminMenu(edict_t *ent);
void    OSP_adminSelectMenu(edict_t *ent, osp_pmenu_t *p);
void    OSP_voteMenu(edict_t *ent, osp_pmenu_t *p);
void    OSP_voteMenu2(edict_t *ent, osp_pmenu_t *p);
void    OSP_helpMenu(edict_t *ent, osp_pmenu_t *p);
void    OSP_help2Menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_help3Menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_inviteMenu(edict_t *ent);
int     OSP_updateAdminMenu(edict_t *ent);
void    OSP_updateVoteMenu2(edict_t *ent);
void    OSP_updateProposalMenu(edict_t *ent);
int     OSP_updateInviteMenu(edict_t *ent);
void    OSP_returnMainTeam_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_returnMainDM_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_returnMainAdmin_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_toggleID_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_changeHUD(edict_t *ent, osp_pmenu_t *p);
void    OSP_changeObserve(edict_t *ent, osp_pmenu_t *p);
void    OSP_changeChase(edict_t *ent, osp_pmenu_t *p);
void    OSP_botMenu(edict_t *ent, osp_pmenu_t *p);
void    OSP_dmReturn_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_changeMap_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_changeConfig_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_changeTime_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_changeFrag_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_changeHook_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_changeRunes_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_changeKick_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_changeItems_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_addSpecificBot_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_addBots_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_removeBots_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_proposeVote_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_joinTeam_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_inviteClose_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_mapAdminSelect_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_playerAdminSelect_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_mapAdminChoose(edict_t *ent, osp_pmenu_t *p);
void    OSP_playerAdminChoose(edict_t *ent, osp_pmenu_t *p);
int     OSP_updateInviteMenu(edict_t *ent);
int     OSP_updateAdminMenu(edict_t *ent);
int     OSP_updateAdminSelectMenu(edict_t *ent);
int     OSP_updateTeamMenu(edict_t *ent);
int     OSP_updateDMMenu(edict_t *ent);
void    OSP_updateVoteMenu(edict_t *ent);
void    OSP_updateVoteMenu2(edict_t *ent);
void    OSP_updateBotMenu(edict_t *ent);
void    OSP_updateProposalMenu(edict_t *ent);
void    OSP_updateProposalMenu2(edict_t *ent);
void    OSP_acceptVote_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_declineVote_menu(edict_t *ent, osp_pmenu_t *p);
void    OSP_voteSummary(char *out, size_t size);
void    OSP_id_cmd(edict_t *ent);
void    OSP_hud_cmd(edict_t *ent);
void    OSP_yes_cmd(edict_t *ent);
void    OSP_no_cmd(edict_t *ent);
void    OSP_ChaseCam(edict_t *ent);
void    OSP_startObserve(edict_t *ent);
void    OSP_removeChaseCam(edict_t *ent);
bool CameraCmd(edict_t *ent, bool force);
int     OSP_votePercent(edict_t *ent, int what);
void     EntityListAdd(edict_t *ent);
void     EntityListRemove(edict_t *ent);
void    EnitityListClean(void);
bool OSP_1v1AllowJoin(edict_t *ent);
void     OSP_1v1Remove(edict_t *ent, int mode);
bool OSP_addTeamMember(edict_t *ent, int team);
bool OSP_defaultTeam(edict_t *ent);
bool OSP_readdTeamMember(edict_t *ent);
void     OSP_removeTeamMember(edict_t *ent, bool quiet);
void     OSP_observerTeamFrags(edict_t *ent);
void     OSP_checkHalt(int reason);
void     OSP_notready_cmd(edict_t *ent, int quiet);
void     OSP_DoRankSort(void);
int      OSP_CheckReady(void);
void     OSP_deadDropRune(edict_t *ent);
int      read_map_entry(FILE *f, char *name, int *lo, int *hi);
int      read_player_entry(FILE *f, char *name, char *pass, char *addr);
void     OSP_loadMaps(void);
void     OSP_loadPlayers(char *filename);
int      OSP_playerAllow(char *name, char *userinfo);
bool     OSP_addrMatch(const char *addr, const char *entry);
int      OSP_addBan(char *name, char *addr);
bool OSP_removeBan(char *name, char *addr);
void     OSP_listbans(edict_t *ent);
bool OSP_mapExists(edict_t *ent, char *name, bool set);
void     OSP_mapList(edict_t *ent);
void     OSP_motd_cmd(edict_t *ent);
void     OSP_talkto_cmd(edict_t *ent);
void     OSP_ready_cmd(edict_t *ent, int quiet);
void     OSP_highscores_cmd(edict_t *ent);
void     OSP_showinfo_cmd(edict_t *ent);
void     OSP_accuracy_cmd(edict_t *ent);
void     OSP_accuracyInfo(edict_t *ent, char *name, int cid);
void     OSP_oldaccuracy_cmd(edict_t *ent);
void     OSP_oldAccuracyInfo(edict_t *ent, int cid);
void     OSP_ffajoin_cmd(edict_t *ent);
void     OSP_vote_cmd(edict_t *ent, int a, int b, char *what, char *value);
void     OSP_map_vote(void);
void     OSP_config_vote(void);
void     OSP_timelimit_vote(void);
void     OSP_fraglimit_vote(void);
void     OSP_hook_vote(void);
void     OSP_runes_vote(void);
void     OSP_toggle_vote(void);
void     OSP_bfg_vote(void);
void     OSP_quad_vote(void);
void     OSP_kick_vote(void);
void     OSP_specbot_vote(void);
void     OSP_addbots_vote(void);
void     OSP_removebots_vote(void);
void     OSP_checkVote(void);
void     OSP_clearVotes(void);
void     OSP_voteinfo(edict_t *ent, bool broadcast);
void     OSP_listItems(char *out);
void     OSP_playertime_cmd(edict_t *ent);
void     OSP_oldscores_cmd(edict_t *ent);
void     OSP_muzzle_cmd(edict_t *ent);
void     OSP_isreferee_cmd(edict_t *ent);
void     OSP_referee_cmd(edict_t *ent);
void     OSP_rhelp_cmd(edict_t *ent);
void     OSP_rkick_cmd(edict_t *ent);
void     OSP_rmap_cmd(edict_t *ent);
void     OSP_rtimelimit_cmd(edict_t *ent);
void     OSP_rfraglimit_cmd(edict_t *ent);
void     OSP_rbanlist_cmd(edict_t *ent);
void     OSP_rban_cmd(edict_t *ent, char *who);
void     OSP_rbanaddr_cmd(edict_t *ent);
void     OSP_runban_cmd(edict_t *ent);
void     OSP_runbanaddr_cmd(edict_t *ent);
void     OSP_hookon_cmd(edict_t *ent);
void     OSP_hookoff_cmd(edict_t *ent);
void        OSP_configLoad(void);
void     OSP_configList(edict_t *ent);
bool OSP_configExists(edict_t *ent, char *name);
bool OSP_configFileExists(char *name);
void     OSP_gameInit(void);
void     OSP_endClean(void);
void     OSP_initWeapItem(void);
// One table for the allow_* family: the cvar an operator sets, the
// entity it inhibits, the short tag the scoreboard banner prints and the name
// the stats log records.  A NULL tag or log name means "inhibited but not
// named", which two of the donor's own rows are.  NULL-terminated.
typedef struct {
    const char  *cvar;
    const char  *classname;
    const char  *tag;
    const char  *logname;
} osp_allow_t;

extern const osp_allow_t osp_allow_items[];

void     OSP_listDisabledItems(char *buf);
void     OSP_clientConfigString(edict_t *ent, short index, const char *string);
void     OSP_clearStats(edict_t *ent);
void     OSP_restartStats(edict_t *ent);
void     OSP_setStats(edict_t *ent);
void     OSP_showFrags(edict_t *ent);
void     OSP_updateClock(void);
void     OSP_getDateInfo(char *out);
void     OSP_checkAnnounce(edict_t *ent);
bool PlayerIdCanSee(edict_t *a, edict_t *b);
int      OSP_setID(edict_t *ent);
bool OSP_changeID(edict_t *ent);
int      OSP_initID(void);
bool loc_CanSee(edict_t *targ, edict_t *inflictor);
void     loc_buildboxpoints(vec3_t p[8], vec3_t org, vec3_t mins, vec3_t maxs);
int      OSP_checkItems(void);
void     OSP_changeItems(void);
void     OSP_removeItem(char *classname);
void     OSP_spawnItem(char *classname);
void     OSP_checkSync(void);
int      OSP_countReady(void);
void     OSP_setAllAccuracy(void);
void     OSP_setSingleAccuracy(edict_t *ent);
void     OSP_startDemos(void);
void     OSP_warmupItems(edict_t *ent);
void     OSP_closeMenus(void);
void     OSP_serverbotsRemove(void);
void     OSP_saveClient(edict_t *ent);
void     OSP_recoverClient(edict_t *ent, char *userinfo);
void     OSP_giveClientID(edict_t *ent);
void     OSP_clearClients(void);
void     OSP_consoleStamp(void);
edict_t *OSP_findPlayer(char *name);
void     OSP_setFeatures(void);
void     OSP_setupAdminLog(void);
void     OSP_playerAnnounce(edict_t *ent, char sound);
void     OSP_parseArmor(void);
void     OSP_parseString(const char *s, gitem_armor_t *info);
void     OSP_setMOTD(void);
void     OSP_showMOTD(void);
void     OSP_setShowParams(void);
void     OSP_showParams(void);
void     OSP_showScores(int *list, int count, edict_t *ent);
void     OSP_showPlayer(edict_t *ent);
int      OSP_teamCount(int team);
int      OSP_teamReady(int team);
bool OSP_1v1Team(edict_t *ent);
void     OSP_1v1Add(edict_t *ent);
void     OSP_1v1QueueCheck(void);
void     OSP_initTeamFrags(edict_t *ent);
void     OSP_playerTeamFrags(edict_t *ent);
void     OSP_updateTeamFrags(void);
void     CameraThink(edict_t *ent);
void     OSP_defaultteam_cmd(edict_t *ent);
void     OSP_defaultjoincode_cmd(edict_t *ent);
void     OSP_joincode_cmd(edict_t *ent);
void     OSP_teamname_cmd(edict_t *ent);
void     OSP_teamskin_cmd(edict_t *ent);
void     OSP_teamjoin_cmd(edict_t *ent, char *teamname);
void     OSP_switchteam_cmd(edict_t *ent);
void     OSP_teaminvite_cmd(edict_t *ent);
void     OSP_lockteam_cmd(edict_t *ent);
void     OSP_unlockteam_cmd(edict_t *ent);
void     OSP_readyteam_cmd(edict_t *ent);
void     OSP_notreadyteam_cmd(edict_t *ent);
void     OSP_captain_cmd(edict_t *ent);
void     OSP_captains_cmd(edict_t *ent);
void     OSP_kickplayer_cmd(edict_t *ent);
void     OSP_1v1queue_cmd(edict_t *ent);
void     OSP_teamReset(void);
void     OSP_findTeamWinner(void);
bool OSP_overtimeWork(int count);
void     OSP_showTeamScores(edict_t *ent);
void     OSP_showBIGTeamScores(edict_t *ent);
void     OSP_show1v1Scores(edict_t *ent);
void     OSP_sayteam_cmd(edict_t *ent, char *msg);
edict_t *NextMap(void);
void     OSP_initHighScores(void);
void     OSP_formatHighScores(void);
void     OSP_showHighScores(void);
void     OSP_updateHighScores(void);
void     OSP_loadHighScores(void);
void     OSP_writeHighScores(void);
bool OSP_makeHSDir(char *base);
int      OSP_readLine(FILE *f, char *a, char *b, char *c);
void     OSP_highscoreDate(char *out);

// The Standard Log writers, declared `static` in the donor's g_local.h and
// therefore missed by the prototype sweep.
void    sl_WriteStdLogDeath(game_import_t *import, level_locals_t level,
                            edict_t *targ, edict_t *inflictor, edict_t *attacker);
void    sl_WriteStdLogPlayerEntered(game_import_t *import, level_locals_t level,
                                    edict_t *ent);
void    sl_LogPlayerDisconnect(game_import_t *import, level_locals_t level,
                               edict_t *ent);
void    sl_LogPlayerConnect(game_import_t *import, char *name, int unused,
                            float time);
void    sl_LogPlayerRename(game_import_t *import, char *oldname, char *newname,
                           float time);
void    sl_LogScore(game_import_t *import, char *player, char *other,
                    char *event, char *weapon, int score, float time, int ping);

// The delegated client-command dispatcher.
bool OSP_ClientCommand(edict_t *ent);
void OSP_CheckRules(void);
void OSP_EndLevel(void);

#endif // OSP_TYPES_H
