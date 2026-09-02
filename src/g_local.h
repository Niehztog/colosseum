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
// g_local.h -- local definitions for game module

#include "shared/shared.h"
#include "shared/list.h"
#include "shared/m_flash.h"

// define GAME_INCLUDE so that game.h does not define the
// short, server-visible gclient_t and edict_t structures,
// because we define the full size ones in this file
#define GAME_INCLUDE
#include "shared/game.h"
// The engine's extended API (sec 5.7).  Vendored with the rest of inc/shared
// and never edited (R-CORE-9).  It carries FILESYSTEM_API_V1 and
// DEBUG_DRAW_API_V1, which R-BOT-8 and R-BOT-27 reach through GetGameAPIEx.
#include "shared/gameext.h"

// RA2's menu engine (R-MENU-1, sec 5.2).  Included rather than forward-declared
// because gclient_t embeds qmenu_t BY VALUE -- Threewave's ctf_pmenuhnd_t is a
// pointer and could be forward-declared, this one cannot.  The header needs
// only edict_t, which shared/game.h has just typedef'd.
#include "arena/menu.h"
// OSP's pmenu engine (R-MENU-1).  gclient_t holds a POINTER to its handle,
// so a forward declaration would do -- but p_menu.h also carries osp_pmenu_t,
// which osp_menus.c passes by value, and one include is cheaper than two
// half-declarations.
#include "tourney/p_menu.h"
// The Gladiator menu engine (R-MENU-1, R-BOT-28) and the SDK's debug-line
// bounding box (R-BOT-27).  Both are embedded BY VALUE -- menustate_t in
// gclient_t, visiblebbox_t in edict_t -- which is why the types come in here
// rather than being forward-declared.  Neither header pulls in botlib.h.
#include "bot/p_menulib.h"
#include "bot/bl_debug.h"

// features this game supports
#define G_FEATURES  (GMF_PROPERINUSE|GMF_WANT_ALL_DISCONNECTS|GMF_ENHANCED_SAVEGAMES)

// the ruleset dispatch (SPECS.md §5.3, R-MODE-1..7).  Included here rather than
// per-file so that a gate is always reachable by its name and never by a cvar
// test that happens to be in scope.
#include "g_ruleset.h"

// the per-ruleset stat slot map and the composed statusbar (R-OSP-7,
// R-OSP-7a).  Same argument as above: a slot is asked for by name, and the
// number it resolves to is the active ruleset's business, not the call
// site's.
#include "g_stats.h"

// the "gameversion" client command will print this plus compile date.
// R-ENG-8: this feeds the `gamename` serverinfo cvar.  Phase 0 deliberately
// left it at "baseq2" so src/ stayed byte-identical to the pin and the
// "unmodified baseq2 library" exit criterion was literally true
// (doc/reconciliation.md R-3); Phase 1 is the first behavioural change, so it
// lands here and that row closes.
#define GAMEVERSION "colosseum"

// protocol bytes that can be directly added to messages
#define svc_muzzleflash     1
#define svc_muzzleflash2    2
#define svc_temp_entity     3
#define svc_layout          4
#define svc_inventory       5
#define svc_stufftext       11
#define svc_disconnect      7
#define svc_configstring    13

//==================================================================

// min()/max() and isnan() now come from the engine headers

//==================================================================

// view pitching times
#define DAMAGE_TIME     0.5
#define FALL_TIME       0.3

// ROGUE.  Ground Zero's own `KILL_DISRUPTOR` switch -- id's note that they cut
// the disruptor -- is resolved statically and removed.  It was #defined 1 in the
// donor, so every branch it guarded was already decided; R-CORE-3 permits a
// feature #ifdef only for platform portability and the two Q2PRO build
// switches, so a live-but-constant one does not survive the merge.
// Consequences, both taken: no AMMO_DISRUPTOR in ammo_t, and no `max_rounds` in
// client_persistant_t.

// R-RA-9.  The port RA2 recognises as the ZBot aim cheat: a client arriving
// from it is flagged `resp.isbot` at connect, which is the other half of
// RA_ZBotSample's runtime heuristic.  Both only ever drive RA2's own reporting.
#define RA_ZBOT_PORT    27902

// edict->spawnflags
// these are set with checkboxes on each entity in the map editor
#define SPAWNFLAG_NOT_EASY          BIT(8)
#define SPAWNFLAG_NOT_MEDIUM        BIT(9)
#define SPAWNFLAG_NOT_HARD          BIT(10)
#define SPAWNFLAG_NOT_DEATHMATCH    BIT(11)
#define SPAWNFLAG_NOT_COOP          BIT(12)

// edict->flags
#define FL_FLY                  BIT(0)
#define FL_SWIM                 BIT(1)      // implied immunity to drowining
#define FL_IMMUNE_LASER         BIT(2)
#define FL_INWATER              BIT(3)
#define FL_GODMODE              BIT(4)
#define FL_NOTARGET             BIT(5)
#define FL_IMMUNE_SLIME         BIT(6)
#define FL_IMMUNE_LAVA          BIT(7)
#define FL_PARTIALGROUND        BIT(8)      // not all corners are valid
#define FL_WATERJUMP            BIT(9)      // player jumping out of water
#define FL_TEAMSLAVE            BIT(10)     // not the first on the team
#define FL_NO_KNOCKBACK         BIT(11)
#define FL_POWER_ARMOR          BIT(12)     // power armor (if any) is active
// Ground Zero: game-private svflag, kept clear of the engine's bits 0..11
#define SVF_DAMAGEABLE          BIT(16)

// ROGUE.  Renumbered to BIT() for consistency with the block above; the values
// are unchanged.
#define FL_MECHANICAL           BIT(13)     // mechanical: sparks, not blood
#define FL_SAM_RAIMI            BIT(14)     // in sam raimi cam mode
#define FL_DISGUISED            BIT(15)     // monsters will not recognise it
#define FL_NOGIB                BIT(16)     // vaporized by a nuke, drop no gibs

// ---------------------------------------------------------------------------
// R-CORE-14: entity flag bits are allocated ONCE, here, and this is the table.
//
// The requirement exists because the donors alias bits under several names, and
// it counted four names on two bits.  Measured 2026-08-21 across baseq2, xatrix
// and rogue, it is SIX names on those two bits, and Ground Zero holds both:
//
//   BIT(13) 0x2000   FL_MECHANICAL (rogue, above)
//                    vs FL_BOT and FL_OSP_NOCMD, which R-CORE-14 assigns here
//   BIT(16) 0x10000  FL_NOGIB (rogue, above)
//                    vs FL_OBSERVER, FL_BOTCLIENT and FL_OSP_BOT, likewise
//
// Ground Zero arrives in Phase 2 and the bot layer in Phase 6, so Rogue is the
// incumbent and the later names are the ones that move.  Allocating them now
// rather than in Phase 6 is the whole point of R-CORE-14: a bot flag colliding
// with "entity is mechanical" would make every bot bleed sparks, and one
// colliding with "vaporized by a nuke" would make every bot dropless.  There
// are 14 free bits, so nothing has to be squeezed.
//
// Reserved here, defined here, used when their phase lands.  A name with no
// user yet costs nothing; a bit allocated twice costs a debugging session.
#define FL_OBSERVER             BIT(17)     // Phase 5 -- Gladiator observer mode (R-EXTRA-6)
#define FL_BOT                  BIT(18)     // Phase 6 -- this client is a bot (R-BOT-14)
#define FL_BOTINPUT             BIT(19)     // Phase 6 -- inside BotExecuteInput (R-BOT-21)
#define FL_OLDORGNOTSET         BIT(20)     // Phase 6 -- skip the old_origin copy (R-BOT-20)
// *Corrected in 1.19, with the tourney donor in front of it.*  R-18 read all
// four spare names as aliases of FL_BOT and left FL_BOTCLIENT undefined.
// Measured in osp-tourney: `FL_OSP_NOCMD` is 0x2000 and `FL_BOT` is BIT(13),
// the SAME bit -- a true alias.  But `FL_OSP_BOT` is 0x10000 and `FL_BOTCLIENT`
// is BIT(16), which is a DIFFERENT bit from FL_BOT with a different meaning:
// FL_BOT is the SDK's "this entity is a bot" and FL_BOTCLIENT is the mod's
// "this client is a bot", and osp_cmds.c tests them in different places for
// different reasons.  Collapsing them would have merged two states.
//
// So the aliases go and the two real bits stay: FL_OSP_NOCMD -> FL_BOT,
// FL_OSP_BOT -> FL_BOTCLIENT.  R-CORE-14's instruction is honoured -- four
// names become two -- and R-OSP-4's "FL_OSP_BOT/FL_BOT mismatch" cannot recur,
// because the confusable pair no longer exists.
#define FL_BOTCLIENT            BIT(21)     // Phase 6 -- this CLIENT is a bot

// FL_OSP_BOT and FL_OSP_NOCMD are NOT defined: R-CORE-14 says the
// duplicate aliases are removed rather than kept as synonyms.  FL_BOT is the
// one name for "is a bot", and osp_* code that said FL_OSP_BOT or FL_OSP_NOCMD
// uses it.
//
// Free after this: BIT(22)..BIT(30).  BIT(31) is FL_RESPAWN.
// ---------------------------------------------------------------------------

#define FL_RESPAWN              BIT(31)     // used for item respawning

// R-CORE-11a: the bits carried in edict_t.content_flavour.
#define CONTENT_XATRIX          BIT(0)
#define CONTENT_ROGUE           BIT(1)

#define FRAMETIME       0.1

// memory tags to allow dynamic memory to be cleaned up
#define TAG_GAME    765     // clear when unloading the dll
#define TAG_LEVEL   766     // clear when loading a new level

#define MELEE_DISTANCE  80

#define BODY_QUEUE_SIZE     8

typedef enum {
    DAMAGE_NO,
    DAMAGE_YES,         // will take damage if hit
    DAMAGE_AIM          // auto targeting recognizes this
} damage_t;

typedef enum {
    WEAPON_READY,
    WEAPON_ACTIVATING,
    WEAPON_DROPPING,
    WEAPON_FIRING
} weaponstate_t;

typedef enum {
    AMMO_BULLETS,
    AMMO_SHELLS,
    AMMO_ROCKETS,
    AMMO_GRENADES,
    AMMO_CELLS,
    AMMO_SLUGS,
    // XATRIX
    AMMO_MAGSLUG,
    AMMO_TRAP,
    // ROGUE
    AMMO_FLECHETTES,
    AMMO_TESLA,
    AMMO_PROX
} ammo_t;

//deadflag
#define DEAD_NO                 0
#define DEAD_DYING              1
#define DEAD_DEAD               2
#define DEAD_RESPAWNABLE        3

//range
#define RANGE_MELEE             0
#define RANGE_NEAR              1
#define RANGE_MID               2
#define RANGE_FAR               3

//gib types
#define GIB_ORGANIC             0
#define GIB_METALLIC            1

//monster ai flags
#define AI_STAND_GROUND         BIT(0)
#define AI_TEMP_STAND_GROUND    BIT(1)
#define AI_SOUND_TARGET         BIT(2)
#define AI_LOST_SIGHT           BIT(3)
#define AI_PURSUIT_LAST_SEEN    BIT(4)
#define AI_PURSUE_NEXT          BIT(5)
#define AI_PURSUE_TEMP          BIT(6)
#define AI_HOLD_FRAME           BIT(7)
#define AI_GOOD_GUY             BIT(8)
#define AI_BRUTAL               BIT(9)
#define AI_NOSTEP               BIT(10)
#define AI_DUCKED               BIT(11)
#define AI_COMBAT_POINT         BIT(12)
#define AI_MEDIC                BIT(13)
#define AI_RESURRECTING         BIT(14)

//ROGUE
#define AI_WALK_WALLS           0x00008000
#define AI_MANUAL_STEERING      0x00010000
#define AI_TARGET_ANGER         0x00020000
#define AI_DODGING              0x00040000
#define AI_CHARGING             0x00080000
#define AI_HINT_PATH            0x00100000
#define AI_IGNORE_SHOTS         0x00200000
// PMM - FIXME - last second added for E3 .. there's probably a better way to do this, but
// this works
#define AI_DO_NOT_COUNT         0x00400000  // set for healed monsters
#define AI_SPAWNED_CARRIER      0x00800000  // both do_not_count and spawned are set for spawned monsters
#define AI_SPAWNED_MEDIC_C      0x01000000  // both do_not_count and spawned are set for spawned monsters
#define AI_SPAWNED_WIDOW        0x02000000  // both do_not_count and spawned are set for spawned monsters
#define AI_SPAWNED_MASK         0x03800000  // mask to catch all three flavors of spawned
#define AI_BLOCKED              0x04000000  // used by blocked_checkattack: set to say I'm attacking while blocked
// (prevents run-attacks)
//ROGUE

//monster attack state
#define AS_STRAIGHT             1
#define AS_SLIDING              2
#define AS_MELEE                3
#define AS_MISSILE              4
#define AS_BLIND                5   // PMM - used by boss code to do nasty things even if it can't see you

// armor types
#define ARMOR_NONE              0
#define ARMOR_JACKET            1
#define ARMOR_COMBAT            2
#define ARMOR_BODY              3
#define ARMOR_SHARD             4

// power armor types
#define POWER_ARMOR_NONE        0
#define POWER_ARMOR_SCREEN      1
#define POWER_ARMOR_SHIELD      2

// handedness values
#define RIGHT_HANDED            0
#define LEFT_HANDED             1
#define CENTER_HANDED           2

// game.serverflags values
#define SFL_CROSS_TRIGGER_1     BIT(0)
#define SFL_CROSS_TRIGGER_2     BIT(1)
#define SFL_CROSS_TRIGGER_3     BIT(2)
#define SFL_CROSS_TRIGGER_4     BIT(3)
#define SFL_CROSS_TRIGGER_5     BIT(4)
#define SFL_CROSS_TRIGGER_6     BIT(5)
#define SFL_CROSS_TRIGGER_7     BIT(6)
#define SFL_CROSS_TRIGGER_8     BIT(7)
#define SFL_CROSS_TRIGGER_MASK  MASK(8)

// noise types for PlayerNoise
#define PNOISE_SELF             0
#define PNOISE_WEAPON           1
#define PNOISE_IMPACT           2

// edict->movetype values
typedef enum {
    MOVETYPE_NONE,          // never moves
    MOVETYPE_NOCLIP,        // origin and angles change with no interaction
    MOVETYPE_PUSH,          // no clip to world, push on box contact
    MOVETYPE_STOP,          // no clip to world, stops on box contact

    MOVETYPE_WALK,          // gravity
    MOVETYPE_STEP,          // gravity, special edge handling
    MOVETYPE_FLY,
    MOVETYPE_TOSS,          // gravity
    MOVETYPE_FLYMISSILE,    // extra size to monsters
    MOVETYPE_BOUNCE,
    MOVETYPE_WALLBOUNCE,    // XATRIX
    MOVETYPE_NEWTOSS        // ROGUE - for deathball
} movetype_t;

typedef struct {
    int     base_count;
    int     max_count;
    float   normal_protection;
    float   energy_protection;
    int     armor;
} gitem_armor_t;

// gitem_t->flags
#define IT_WEAPON       BIT(0)      // use makes active weapon
#define IT_AMMO         BIT(1)
#define IT_ARMOR        BIT(2)
#define IT_STAY_COOP    BIT(3)
#define IT_KEY          BIT(4)
#define IT_POWERUP      BIT(5)

// ROGUE
#define IT_MELEE        BIT(6)
#define IT_NOT_GIVEABLE BIT(7)      // item can not be given
// CTF -- Threewave writes this as a bare `64`, i.e. BIT(6), which Ground Zero
// already holds for IT_MELEE.  A value collision, not a name collision, so
// R-CORE-14's treatment applies rather than §7 rule 4's: the bit moves and the
// allocation is recorded (doc/reconciliation.md R-40).
#define IT_TECH         BIT(8)
// Tourney spells its runes IT_RUNE and gives them id CTF's IT_TECH value; here
// IT_TECH is Threewave's and already taken, so the runes get a bit of their own
// (sec 7 rule 3: two donors adding different content both get added).
#define IT_RUNE         BIT(9)
// ROGUE

// gitem_t->weapmodel for weapons indicates model index
//
// R-142: THESE ARE POSITIONS IN A LIST, NOT NAMES.  `ChangeWeapon` puts the
// value in the high byte of the player's `s.skinnum`, and the client draws
// `weaponmodel[skinnum >> 8]` -- an array it fills with `weaponModels[0] =
// "weapon.md2"` and then one entry per `#`-prefixed model in configstring
// order.  So the number IS the ordinal of that weapon's `#w_*.md2` in
// SP_worldspawn's precache block, and `g_spawn.c` says so above it: "THIS ORDER
// MUST MATCH THE DEFINES IN g_local.h".
//
// Each donor numbered its own extra weapons from 12 because each ships a list
// with nothing after the BFG: CTF's grapple is 12, Xatrix's phalanx is 12,
// Rogue's disruptor is 12.  R-CORE-2 unions the CONTENT, so the precache block
// is now one ordered list of nineteen -- and three donors' private 12s made
// seven weapons draw somebody else's model: a phalanx or a disruptor appeared
// as a grapple, a ripper and an ETF rifle as a phalanx, and so on down the
// list.  The Gladiator donor met the same problem when it merged the same two
// packs and answered it the same way, at 12..18 (`ugladq2/src/g_local.h`).
//
// So the numbering is the merged list's.  The only consumer is the one line in
// `ChangeWeapon`; `tools/dupvalue.py` is what keeps the next merge honest.
#define WEAP_BLASTER            1
#define WEAP_SHOTGUN            2
#define WEAP_SUPERSHOTGUN       3
#define WEAP_MACHINEGUN         4
#define WEAP_CHAINGUN           5
#define WEAP_GRENADES           6
#define WEAP_GRENADELAUNCHER    7
#define WEAP_ROCKETLAUNCHER     8
#define WEAP_HYPERBLASTER       9
#define WEAP_RAILGUN            10
#define WEAP_BFG                11
#define WEAP_GRAPPLE            12      // CTF
#define WEAP_PHALANX            13      // XATRIX
#define WEAP_BOOMER             14      // XATRIX
#define WEAP_DISRUPTOR          15      // PGM
#define WEAP_ETFRIFLE           16      // PGM
#define WEAP_PLASMA             17      // PGM
#define WEAP_PROXLAUNCH         18      // PGM
#define WEAP_CHAINFIST          19      // PGM

typedef struct gitem_s {
    char        *classname; // spawning name
    bool        (*pickup)(struct edict_s *ent, struct edict_s *other);
    void        (*use)(struct edict_s *ent, const struct gitem_s *item);
    void        (*drop)(struct edict_s *ent, const struct gitem_s *item);
    void        (*weaponthink)(struct edict_s *ent);
    char        *pickup_sound;
    char        *world_model;
    int         world_model_flags;
    char        *view_model;

    // client side info
    char        *icon;
    char        *pickup_name;   // for printing on pickup
    int         count_width;    // number of digits to display by icon

    int         quantity;       // for ammo how much, for weapons how much is used per shot
    char        *ammo;          // for weapons
    int         flags;          // IT_* flags

    int         weapmodel;      // weapon model index (for weapons)

    const void  *info;
    int         tag;

    const char *const   *precaches;     // array of all models, sounds, and images this item will use
} gitem_t;

typedef struct precache_s {
    struct precache_s   *next;
    void                (*func)(void);
} precache_t;

// new game API can be used w/o protocol extensions,
// so this needs to be dynamic
#if USE_NEW_GAME_API
#define PM_TIME_SHIFT   (game.csr.extended ? 0 : 3)
#else
#define PM_TIME_SHIFT   3
#endif

//
// this structure is left intact through an entire game
// it should be initialized at dll load time, and read/written to
// the server.ssv file for savegames
//
typedef struct {
    char        helpmessage1[512];
    char        helpmessage2[512];
    int         helpchanged;    // flash F1 icon if non 0, play sound
                                // and increment only if 1, 2, or 3

    gclient_t   *clients;       // [maxclients]

    // can't store spawnpoint in level, because
    // it would get overwritten by the savegame restore
    char        spawnpoint[512];    // needed for coop respawns

    // store latched cvars here that we want to get at often
    int         maxclients;
    int         maxentities;

    // cross level triggers
    int         serverflags;

    // items
    int         num_items;

    bool        autosaved;

    cs_remap_t  csr;

    precache_t  *precaches;
} game_locals_t;

// CTF types the client structs below hold pointers to.  Forward-declared rather
// than included: Threewave puts `#include "p_menu.h"` at the top of g_local.h
// and `#include "g_ctf.h"` at the bottom, and both of those headers need
// edict_t, so the include order only works by accident.  §5.2 is explicit that
// CTF and tourney will both ship a p_menu.h, which is the other half of the
// reason (R-25 item 3).
typedef struct ctf_pmenuhnd_s   ctf_pmenuhnd_t;
typedef struct ghost_s          ghost_t;

// R-MENU-1 ships four donor menu engines and R-MENU-3 allows exactly one open
// per client.  This is the single field R-MENU-2a asks for: the engines are
// separate translation units with prefixed symbols and no knowledge of each
// other, so the arbiter has to be outside all of them.
typedef enum {
    MENU_NONE,
    MENU_CTF,           // Threewave pmenu_t     -- src/ctf/p_menu.c
    MENU_TOURNEY,       // OSP pmenu_t           -- Phase 5
    MENU_ARENA,         // RA2 qmenu_t           -- Phase 4
    MENU_BOT,           // Gladiator menu_t tree -- Phase 6
} menu_owner_t;

//
// this structure is cleared as each map is entered
// it is read/written to the level.sav file for savegames
//
typedef struct {
    int         framenum;
    float       time;

    char        level_name[MAX_QPATH];  // the descriptive name (Outer Base, etc)
    char        mapname[MAX_QPATH];     // the server name (base1, etc)
    char        nextmap[MAX_QPATH];     // go here when fraglimit is hit
    char        forcemap[MAX_QPATH];    // CTF: a vote or `sv warp` overrides the
                                        // rotation for exactly one level change

    // intermission state
    int       intermission_framenum;       // time the intermission was started
    char        *changemap;
    int         exitintermission;
    vec3_t      intermission_origin;
    vec3_t      intermission_angle;

    edict_t     *sight_client;  // changed once each frame for coop games

    edict_t     *sight_entity;
    int         sight_entity_framenum;
    edict_t     *sound_entity;
    int         sound_entity_framenum;
    edict_t     *sound2_entity;
    int         sound2_entity_framenum;

    int         pic_health;

    int         total_secrets;
    int         found_secrets;

    int         total_goals;
    int         found_goals;

    int         total_monsters;
    int         killed_monsters;

    edict_t     *current_entity;    // entity running from G_RunFrame
    int         body_que;           // dead bodies

    int         power_cubes;        // ugly necessity for coop

    // ROGUE
    edict_t     *disguise_violator;
    int         disguise_violation_framenum;
    // ROGUE
} level_locals_t;

// spawn_temp_t is only used to hold entity field values that
// can be set from the editor, but aren't actualy present
// in edict_t during gameplay
typedef struct {
    // world vars
    char        *sky;
    float       skyrotate;
    vec3_t      skyaxis;
    char        *nextmap;
    char        *musictrack;

    int         lip;
    int         distance;
    int         height;
    char        *noise;
    // *** R-KEY-1, caught live. ***
    //
    // This member is `pausetime`, NOT `pause_framenum`, and the distinction is
    // the entire content of R-KEY-1.  A .bsp names entity fields as literal
    // strings matched against the key column of temp_fields[], so the member
    // name IS the map contract: `func_timer` entities set `"pausetime"`.
    //
    // Q2PRO's "Convert monster timers to frame numbers." renamed the unrelated
    // `monsterinfo_t.pausetime` to `pause_framenum`.  Replayed tree-wide it also
    // renamed THIS member, and every map setting `pausetime` on a func_timer
    // silently lost its initial pause -- ED_ParseEdict reported "pausetime is
    // not a field" and banks of timers that should stagger fired in lockstep.
    //
    // Q2PRO has since fixed it: `q2pro/src/game/g_local.h` carries both
    // `spawn_temp_t.pausetime` (float) and `monsterinfo_t.pause_framenum` (int)
    // as the two separate things they always were.  **Both donor branches in
    // the bundles still carry the broken rename**, so the three-way merge
    // propagated it in, and only `g_func.c`'s surviving `st.pausetime` made the
    // compiler catch it.  Had g_func.c come from a donor too it would have
    // compiled clean and broken every func_timer in every map.
    //
    // §7 rule 1 decides it regardless: a donor renaming a shared member is not
    // that donor's own feature, so Q2PRO wins.  R-VER-14 is the regression.
    float       pausetime;
    char        *item;
    char        *gravity;

    float       minyaw;
    float       maxyaw;
    float       minpitch;
    float       maxpitch;

    // R-OSP-6's five extra keys, and all five are PARSED now -- including
    // `botlib`, whose consumer is the bot layer and lands in Phase 6.  A key
    // that is not in temp_fields[] is rejected outright with "not a field", so
    // deferring the row would REJECT every tourney map that names a bot library
    // rather than ignoring the setting: storing the string and not yet reading
    // it is the difference between a map that loads and one that does not.
    //
    // These four share their names with four members of edict_t that tourney
    // also owns, which is legal and is the donor's own shape: the entity parser
    // writes here, and OSP_userinfoChanged writes there.
    char        *botlib;
    char        *name;
    char        *skin;
    char        *charfile;
    char        *charname;
} spawn_temp_t;

typedef struct {
    // fixed data
    vec3_t      start_origin;
    vec3_t      start_angles;
    vec3_t      end_origin;
    vec3_t      end_angles;

    int         sound_start;
    int         sound_middle;
    int         sound_end;

    float       accel;
    float       speed;
    float       decel;
    float       distance;

    float       wait;

    // state data
    int         state;
    vec3_t      dir;
    float       current_speed;
    float       move_speed;
    float       next_speed;
    float       remaining_distance;
    float       decel_distance;
    void        (*endfunc)(edict_t *);
} moveinfo_t;

typedef struct {
    void    (*aifunc)(edict_t *self, float dist);
    float   dist;
    void    (*thinkfunc)(edict_t *self);
} mframe_t;

typedef struct {
    int         firstframe;
    int         lastframe;
    const mframe_t  *frame;
    void        (*endfunc)(edict_t *self);
} mmove_t;

typedef struct {
    const mmove_t     *currentmove;
    unsigned int    aiflags;        // PGM - unsigned, since we're close to the max
    int         nextframe;
    float       scale;

    void (*stand)(edict_t *self);
    void (*idle)(edict_t *self);
    void (*search)(edict_t *self);
    void (*walk)(edict_t *self);
    void (*run)(edict_t *self);
    void (*dodge)(edict_t *self, edict_t *other, float eta, trace_t *tr);
    void (*attack)(edict_t *self);
    void (*melee)(edict_t *self);
    void (*sight)(edict_t *self, edict_t *other);
    bool(*checkattack)(edict_t *self);

    int       pause_framenum;
    float       attack_finished;

    vec3_t      saved_goal;
    int       search_framenum;
    int       trail_framenum;
    vec3_t      last_sighting;
    int         attack_state;
    int         lefty;
    int       idle_framenum;
    int         linkcount;

    int         power_armor_type;
    int         power_armor_power;

//ROGUE
    bool(*blocked)(edict_t *self, float dist);
//  edict_t     *last_hint;         // last hint_path the monster touched
    int       last_hint_framenum;     // last time the monster checked for hintpaths.
    edict_t     *goal_hint;         // which hint_path we're trying to get to
    int         medicTries;
    edict_t     *badMedic1, *badMedic2; // these medics have declared this monster "unhealable"
    edict_t     *healer;    // this is who is healing this monster
    void (*duck)(edict_t *self, float eta);
    void (*unduck)(edict_t *self);
    void (*sidestep)(edict_t *self);
    //  while abort_duck would be nice, only monsters which duck but don't sidestep would use it .. only the brain
    //  not really worth it.  sidestep is an implied abort_duck
//  void        (*abort_duck)(edict_t *self);
    float       base_height;
    int       next_duck_framenum;
    int       duck_wait_framenum;
    edict_t     *last_player_enemy;
    // blindfire stuff .. the boolean says whether the monster will do it, and blind_fire_time is the timing
    // (set in the monster) of the next shot
    bool    blindfire;      // will the monster blindfire?
    float       blind_fire_delay;
    vec3_t      blind_fire_target;
    // used by the spawners to not spawn too much and keep track of #s of monsters spawned
    int         monster_slots;
    int         monster_used;
    edict_t     *commander;
    // powerup timers, used by widow, our friend
    int         quad_framenum;
    int         invincible_framenum;
    int         double_framenum;
//ROGUE
} monsterinfo_t;

// ROGUE
// this determines how long to wait after a duck to duck again.  this needs to be longer than
// the time after the monster_duck_up in all of the animation sequences
#define DUCK_INTERVAL   0.5
// ROGUE

extern  game_locals_t   game;
extern  level_locals_t  level;
extern  game_import_t   gi;
extern  game_export_t   globals;
extern  spawn_temp_t    st;

extern  int sm_meat_index;

//extern  int jacket_armor_index;
//extern  int combat_armor_index;
//extern  int body_armor_index;

// means of death
#define MOD_UNKNOWN         0
#define MOD_BLASTER         1
#define MOD_SHOTGUN         2
#define MOD_SSHOTGUN        3
#define MOD_MACHINEGUN      4
#define MOD_CHAINGUN        5
#define MOD_GRENADE         6
#define MOD_G_SPLASH        7
#define MOD_ROCKET          8
#define MOD_R_SPLASH        9
#define MOD_HYPERBLASTER    10
#define MOD_RAILGUN         11
#define MOD_BFG_LASER       12
#define MOD_BFG_BLAST       13
#define MOD_BFG_EFFECT      14
#define MOD_HANDGRENADE     15
#define MOD_HG_SPLASH       16
#define MOD_WATER           17
#define MOD_SLIME           18
#define MOD_LAVA            19
#define MOD_CRUSH           20
#define MOD_TELEFRAG        21
#define MOD_FALLING         22
#define MOD_SUICIDE         23
#define MOD_HELD_GRENADE    24
#define MOD_EXPLOSIVE       25
#define MOD_BARREL          26
#define MOD_BOMB            27
#define MOD_EXIT            28
#define MOD_SPLASH          29
#define MOD_TARGET_LASER    30
#define MOD_TRIGGER_HURT    31
#define MOD_HIT             32
#define MOD_TARGET_BLASTER  33
// RAFAEL 14-APR-98
#define MOD_RIPPER              34
#define MOD_PHALANX             35
#define MOD_BRAINTENTACLE       36
#define MOD_BLASTOFF            37
#define MOD_GEKK                38
#define MOD_TRAP                39
// END 14-APR-98
#define MOD_FRIENDLY_FIRE   BIT(31)

//========
//ROGUE
#define MOD_CHAINFIST           40
#define MOD_DISINTEGRATOR       41
#define MOD_ETF_RIFLE           42
#define MOD_BLASTER2            43
#define MOD_HEATBEAM            44
#define MOD_TESLA               45
#define MOD_PROX                46
#define MOD_NUKE                47
#define MOD_VENGEANCE_SPHERE    48
#define MOD_HUNTER_SPHERE       49
#define MOD_DEFENDER_SPHERE     50
#define MOD_TRACKER             51
#define MOD_DBALL_CRUSH         52
#define MOD_DOPPLE_EXPLODE      53
#define MOD_DOPPLE_VENGEANCE    54
#define MOD_DOPPLE_HUNTER       55
//ROGUE
//========
// CTF.  Threewave numbers this 34, which Xatrix already holds for MOD_RIPPER.
// meansOfDeath is game-internal and appears in no protocol or savegame, so the
// collision is resolved by taking the next free value rather than by a prefix.
#define MOD_GRAPPLE             56

extern  int meansOfDeath;

extern  edict_t         *g_edicts;

#define FOFS(x) q_offsetof(edict_t, x)
#define STOFS(x) q_offsetof(spawn_temp_t, x)
#define LLOFS(x) q_offsetof(level_locals_t, x)
#define GLOFS(x) q_offsetof(game_locals_t, x)
#define CLOFS(x) q_offsetof(gclient_t, x)

#define random()    frand()
#define crandom()   crand()

extern  cvar_t  *maxentities;
extern  cvar_t  *deathmatch;
extern  cvar_t  *coop;
extern  cvar_t  *dmflags;
extern  cvar_t  *skill;
extern  cvar_t  *fraglimit;
extern  cvar_t  *timelimit;
extern  cvar_t  *capturelimit;      // CTF
extern  cvar_t  *instantweap;       // CTF
extern  cvar_t  *password;
extern  cvar_t  *spectator_password;
extern  cvar_t  *needpass;
extern  cvar_t  *g_select_empty;
extern  cvar_t  *dedicated;

extern  cvar_t  *filterban;

extern  cvar_t  *sv_gravity;
extern  cvar_t  *sv_maxvelocity;

extern  cvar_t  *gun_x, *gun_y, *gun_z;
extern  cvar_t  *sv_rollspeed;
extern  cvar_t  *sv_rollangle;

extern  cvar_t  *run_pitch;
extern  cvar_t  *run_roll;
extern  cvar_t  *bob_up;
extern  cvar_t  *bob_pitch;
extern  cvar_t  *bob_roll;

extern  cvar_t  *sv_cheats;
extern  cvar_t  *maxclients;
extern  cvar_t  *maxspectators;

// RA2's four (R-RA-1a).  `hostname`, `port` and `logfile` are the engine's own,
// re-obtained for the round log's header rather than re-declared with a second
// meaning; `netlog` no longer opens anything (R-SEC-7) and is kept registered
// so a legacy config still resolves.
extern  cvar_t  *hostname;
extern  cvar_t  *hostport;
extern  cvar_t  *logfile;
extern  cvar_t  *netlog;

extern  cvar_t  *flood_msgs;
extern  cvar_t  *flood_persecond;
extern  cvar_t  *flood_waitdelay;

extern  cvar_t  *sv_maplist;

extern  cvar_t  *sv_stopspeed;      // PGM - this was a define in g_phys.c

//ROGUE
extern  cvar_t  *g_showlogic;
extern  cvar_t  *gamerules;
extern  cvar_t  *huntercam;
extern  cvar_t  *strong_mines;
extern  cvar_t  *randomrespawn;

// this is for the count of monsters
#define ENT_SLOTS_LEFT      (ent->monsterinfo.monster_slots - ent->monsterinfo.monster_used)
#define SELF_SLOTS_LEFT     (self->monsterinfo.monster_slots - self->monsterinfo.monster_used)
//ROGUE

extern  cvar_t  *sv_features;

#define world   (&g_edicts[0])

// item spawnflags
#define ITEM_TRIGGER_SPAWN      BIT(0)
#define ITEM_NO_TOUCH           BIT(1)
// 6 bits reserved for editor flags
// 8 bits used as power cube id bits for coop games
#define DROPPED_ITEM            BIT(16)
#define DROPPED_PLAYER_ITEM     BIT(17)
#define ITEM_TARGETS_USED       BIT(18)

//
// fields are needed for spawning from the entity string
// and saving / loading games
//
typedef enum {
    F_BAD,
    F_BYTE,
    F_SHORT,
    F_INT,
    F_BOOL,
    F_FLOAT,
    F_LSTRING,          // string on disk, pointer in memory, TAG_LEVEL
    F_GSTRING,          // string on disk, pointer in memory, TAG_GAME
    F_ZSTRING,          // string on disk, string in memory
    F_VECTOR,
    F_ANGLEHACK,
    F_EDICT,            // index on disk, pointer in memory
    F_ITEM,             // index on disk, pointer in memory
    F_CLIENT,           // index on disk, pointer in memory
    F_FUNCTION,
    F_POINTER,
    F_IGNORE
} fieldtype_t;

extern const gitem_t    itemlist[];

//
// g_cmds.c
//
void Cmd_Help_f(edict_t *ent);
void Cmd_Score_f(edict_t *ent);
void ClientCommand(edict_t *ent);

//
// g_items.c
//
void PrecacheItem(const gitem_t *it);
void InitItems(void);
void SetItemNames(void);
const gitem_t *FindItem(const char *pickup_name);
const gitem_t *FindItemByClassname(const char *classname);
#define ITEM_INDEX(x) ((x)-itemlist)
edict_t *Drop_Item(edict_t *ent, const gitem_t *item);
void SetRespawn(edict_t *ent, float delay);
void ChangeWeapon(edict_t *ent);
void SpawnItem(edict_t *ent, const gitem_t *item);
void Think_Weapon(edict_t *ent);
int ArmorIndex(edict_t *ent);
int PowerArmorType(edict_t *ent);
const gitem_t *GetItemByIndex(int index);
bool Add_Ammo(edict_t *ent, const gitem_t *item, int count);
void Touch_Item(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf);

//
// g_utils.c
//
bool    KillBox(edict_t *ent);
void    G_ProjectSource(const vec3_t point, const vec3_t distance, const vec3_t forward, const vec3_t right, vec3_t result);
edict_t *G_Find(edict_t *from, int fieldofs, char *match);
// The spawn points of `classname` a random selection may choose among, which is
// two short of the map's count -- see the comment on the implementation.  Both
// map-sized bot fills read it (R-DM-1, R-CTF-8).
int     G_SpawnPointPool(char *classname);
edict_t *findradius(edict_t *from, vec3_t org, float rad);
edict_t *G_PickTarget(char *targetname);
void    G_UseTargets(edict_t *ent, edict_t *activator);
void    G_SetMovedir(vec3_t angles, vec3_t movedir);

void    G_InitEdict(edict_t *e);
edict_t *G_Spawn(void);
void    G_FreeEdict(edict_t *e);

void    G_TouchTriggers(edict_t *ent);

char    *G_CopyString(char *in);
// Threewave and RA2 each shipped a byte-identical copy; it is a generic
// engine helper and lives in g_utils.c (doc/reconciliation.md R-67).
void stuffcmd(edict_t *ent, char *s);
// Six of g_cmds.c's own handlers, reached by tourney's delegated dispatcher.
void Cmd_Say_f(edict_t *ent, bool team, bool arg0, bool bcast);
void Cmd_Players_f(edict_t *ent);
void Cmd_Help_f(edict_t *ent);
void SelectNextItem(edict_t *ent, int itflags);
void SelectPrevItem(edict_t *ent, int itflags);
void Cmd_Use_f(edict_t *ent);
void Cmd_InvDrop_f(edict_t *ent);
void Cmd_WeapPrev_f(edict_t *ent);
void Cmd_WeapNext_f(edict_t *ent);
void Cmd_WeapLast_f(edict_t *ent);
void Cmd_Drop_f(edict_t *ent);
void Cmd_Inven_f(edict_t *ent);
void Cmd_Give_f(edict_t *ent);
void Cmd_God_f(edict_t *ent);
void Cmd_Notarget_f(edict_t *ent);
void Cmd_Noclip_f(edict_t *ent);
void Cmd_Wave_f(edict_t *ent);
void Cmd_PlayerList_f(edict_t *ent);
void Cmd_PutAway_f(edict_t *ent);

float vectoyaw(vec3_t vec);
void vectoangles(vec3_t vec, vec3_t angles);

//ROGUE
void    G_ProjectSource2(const vec3_t point, const vec3_t distance, const vec3_t forward, const vec3_t right, const vec3_t up, vec3_t result);
float   vectoyaw2(vec3_t vec);
void    vectoangles2(const vec3_t vec, vec3_t angles);
edict_t *findradius2(edict_t *from, vec3_t org, float rad);
//ROGUE

//
// g_combat.c
//
bool OnSameTeam(edict_t *ent1, edict_t *ent2);
bool FloodProtect(edict_t *ent);
bool CanDamage(edict_t *targ, edict_t *inflictor);
bool CheckTeamDamage(edict_t *targ, edict_t *attacker);
void T_Damage(edict_t *targ, edict_t *inflictor, edict_t *attacker, const vec3_t dir, vec3_t point, const vec3_t normal, int damage, int knockback, int dflags, int mod);
void T_RadiusDamage(edict_t *inflictor, edict_t *attacker, float damage, edict_t *ignore, float radius, int mod);

//ROGUE
void T_RadiusNukeDamage(edict_t *inflictor, edict_t *attacker, float damage, edict_t *ignore, float radius, int mod);
void T_RadiusClassDamage(edict_t *inflictor, edict_t *attacker, float damage, char *ignoreClass, float radius, int mod);
void cleanupHealTarget(edict_t *ent);
//ROGUE

// damage flags
#define DAMAGE_RADIUS           BIT(0)  // damage was indirect
#define DAMAGE_NO_ARMOR         BIT(1)  // armour does not protect from this damage
#define DAMAGE_ENERGY           BIT(2)  // damage is from an energy based weapon
#define DAMAGE_NO_KNOCKBACK     BIT(3)  // do not affect velocity, just view angles
#define DAMAGE_BULLET           BIT(4)  // damage is from a bullet (used for ricochets)
#define DAMAGE_NO_PROTECTION    BIT(5)  // armor, shields, invulnerability, and godmode have no effect
//ROGUE
#define DAMAGE_DESTROY_ARMOR    0x00000040  // damage is done to armor and health.
#define DAMAGE_NO_REG_ARMOR     0x00000080  // damage skips regular armor
#define DAMAGE_NO_POWER_ARMOR   0x00000100  // damage skips power armor
//ROGUE

#define DEFAULT_BULLET_HSPREAD  300
#define DEFAULT_BULLET_VSPREAD  500
#define DEFAULT_SHOTGUN_HSPREAD 1000
#define DEFAULT_SHOTGUN_VSPREAD 500
#define DEFAULT_DEATHMATCH_SHOTGUN_COUNT    12
#define DEFAULT_SHOTGUN_COUNT   12
#define DEFAULT_SSHOTGUN_COUNT  20

//
// g_monster.c
//
void monster_fire_bullet(edict_t *self, vec3_t start, vec3_t dir, int damage, int kick, int hspread, int vspread, int flashtype);
void monster_fire_shotgun(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, int flashtype);
void monster_fire_blaster(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int flashtype, int effect);
void monster_fire_grenade(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int flashtype);
void monster_fire_rocket(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int flashtype);
void monster_fire_railgun(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int flashtype);
void monster_fire_bfg(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int kick, float damage_radius, int flashtype);
// RAFAEL
void monster_fire_ionripper(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int flashtype, int effect);
// §7 rule 4: two donors claim `fire_heat` and `monster_fire_heat` for different
// weapons -- Xatrix's Phalanx heat-seeking projectile and Ground Zero's plasma
// beam -- so both take their donor prefix.  Neither keeps the bare name: rule 4
// gives it to "the primary ruleset's meaning", and a content layer is not a
// ruleset, so there is no principled winner and an unprefixed `fire_heat` would
// be ambiguous to every future reader.  R-CONV-2 had no mission-pack prefix at
// all; `xatrix_`/`rogue_` are added there in spec 1.7.
void xatrix_monster_fire_heat(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int flashtype);
void monster_dabeam(edict_t *self);
void monster_fire_blueblaster(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int flashtype, int effect);

void M_droptofloor(edict_t *ent);
void monster_think(edict_t *self);
void walkmonster_start(edict_t *self);
void swimmonster_start(edict_t *self);
void flymonster_start(edict_t *self);
void AttackFinished(edict_t *self, float time);
void monster_death_use(edict_t *self);
void M_CatagorizePosition(edict_t *ent);
bool M_CheckAttack(edict_t *self);
void M_FlyCheck(edict_t *self);
void M_CheckGround(edict_t *ent);
//ROGUE
void monster_fire_blaster2(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int flashtype, int effect);
void monster_fire_tracker(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, edict_t *enemy, int flashtype);
void rogue_monster_fire_heat(edict_t *self, const vec3_t start, const vec3_t dir, const vec3_t offset, int damage, int kick, int flashtype);
void stationarymonster_start(edict_t *self);
void monster_done_dodge(edict_t *self);
//ROGUE

//
// g_misc.c
//
void ThrowHead(edict_t *self, char *gibname, int damage, int type);
void ThrowClientHead(edict_t *self, int damage);
void ThrowGib(edict_t *self, char *gibname, int damage, int type);
void BecomeExplosion1(edict_t *self);
// RAFAEL
void ThrowHeadACID(edict_t *self, char *gibname, int damage, int type);
void ThrowGibACID(edict_t *self, char *gibname, int damage, int type);

#define CLOCK_MESSAGE_SIZE  16
void func_clock_think(edict_t *self);
void func_clock_use(edict_t *self, edict_t *other, edict_t *activator);

//
// g_ai.c
//
void AI_SetSightClient(void);

void ai_stand(edict_t *self, float dist);
void ai_move(edict_t *self, float dist);
void ai_walk(edict_t *self, float dist);
void ai_turn(edict_t *self, float dist);
void ai_run(edict_t *self, float dist);
void ai_charge(edict_t *self, float dist);
int range(edict_t *self, edict_t *other);

bool FindTarget(edict_t *self);
void FoundTarget(edict_t *self);
bool infront(edict_t *self, edict_t *other);
bool visible(edict_t *self, edict_t *other);
bool FacingIdeal(edict_t *self);

//
// g_weapon.c
//
void ThrowDebris(edict_t *self, char *modelname, float speed, vec3_t origin);
bool fire_hit(edict_t *self, vec3_t aim, int damage, int kick);
void fire_bullet(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int mod);
void fire_shotgun(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, int mod);
void fire_blaster(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int effect, bool hyper);
void fire_grenade(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, float timer, float damage_radius);
void fire_grenade2(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, float timer, float damage_radius, bool held);
void fire_rocket(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius, int radius_damage);
void fire_rail(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick);
void fire_bfg(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius);
// RAFAEL
void fire_ionripper(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int effect);
void xatrix_fire_heat(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius, int radius_damage);
void fire_blueblaster(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int effect);
void fire_plasma(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius, int radius_damage);
void fire_trap(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, float timer, float damage_radius, bool held);

//
// p_trail.c
//
void PlayerTrail_Init(void);
void PlayerTrail_Add(vec3_t spot);
void PlayerTrail_New(vec3_t spot);
edict_t *PlayerTrail_PickFirst(edict_t *self);
edict_t *PlayerTrail_PickNext(edict_t *self);
edict_t *PlayerTrail_LastSpot(void);

//
// p_client.c
//
void respawn(edict_t *self);
void PutClientInServer(edict_t *ent);
void InitClientPersistant(gclient_t *client, bool full);
void ClientObituary(edict_t *self, edict_t *inflictor, edict_t *attacker);
// `ent` is the client being placed, or NULL where there is none (R-195.5).
float PlayersRangeFromSpot(edict_t *spot, edict_t *ent);
edict_t *SelectRandomDeathmatchSpawnPoint(edict_t *ent);
edict_t *SelectFarthestDeathmatchSpawnPoint(edict_t *ent);
// R-DM-1: how many players the MAP seats, for `botfill` under `dm` and
// `dmpro`.  Unclamped and with
// no cvar test -- BotFillTarget() owns both.  Lives here because it is the spawn
// selectors' own number; see the comment on the implementation.
int  DM_BotFillSeats(void);
void InitBodyQue(void);
void ClientBeginServerFrame(edict_t *ent);
void ClientBegin(edict_t *ent);
qboolean ClientConnect(edict_t *ent, char *userinfo);
void ClientDisconnect(edict_t *ent);
void ClientThink(edict_t *ent, usercmd_t *ucmd);
void ClientUserinfoChanged(edict_t *ent, char *userinfo);
void SaveClientData(void);
// arena.c drives its own respawn, so this one stops being static (sec 7 rule 2);
// InitClientPersistant and PlayersRangeFromSpot were already declared above.
void InitClientResp(gclient_t *client);
void FetchClientEntData(edict_t *ent);
void player_pain(edict_t *self, edict_t *other, float kick, int damage);
void player_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point);

//
// g_svcmds.c
//
void ServerCommand(void);
bool SV_FilterPacket(char *from);

//
// p_view.c
//
void ClientEndServerFrame(edict_t *ent);

//
// p_hud.c
//
void MoveClientToIntermission(edict_t *client);
void BeginIntermission(edict_t *targ);
// Non-static because the ruleset dispatch table in g_ruleset.c points at them,
// which is a call site across a file boundary (R-CORE-13).  Donors add their own
// call sites later -- tourney needs EndDMLevel and DeathmatchScoreboard, and
// R-CORE-13 makes the answer the union, not a per-file judgement.
void CheckDMRules(void);
void EndDMLevel(void);
edict_t *CreateTargetChangeLevel(char *map);
// False REFUSES the placement, which only tourney does (R-OSP-1): see
// PutClientInServer's frozen arm and OSP_spawnRefused.
bool SelectSpawnPoint(edict_t *ent, vec3_t origin, vec3_t angles);
void G_SetStats(edict_t *ent);
void DeathmatchScoreboard(edict_t *ent);
// R-193: reached from ClientThink's observer arms, where a menu press consumes
// the key that would otherwise change camera mode.
void Cmd_InvUse_f(edict_t *ent);

// R-MENU-2a/3, implemented in p_hud.c beside the other owners of the layout
// channel.  G_MenuOpen closes the incumbent -- whichever engine owns it -- and
// records the new owner; nothing else may write gclient_t.menu_owner.
void G_MenuOpen(edict_t *ent, menu_owner_t who);
void G_MenuClose(edict_t *ent);
bool G_MenuActive(edict_t *ent);
// The layout channel's other claimant.  `showscores` under dm/ctf/tourney,
// `scoremode` under arena -- see p_hud.c.
bool G_ScoreboardUp(edict_t *ent);

// Takes a layout back off a client.  Four engines and the SDK's loading image
// all write into the layout channel and every one of them needs the same
// undo; the donors each open-coded it against their own idea of which
// statusbar to repaint, which is doc/reconciliation.md R-87's defect in the
// small.  One function, and it asks G_Statusbar() which bar the ACTIVE ruleset
// composed rather than choosing between dm_statusbar and single_statusbar.
void G_LayoutClear(edict_t *ent);

// ---- the engine's extended API (sec 5.7) ----------------------------------
//
// GetGameAPIEx is guaranteed to be called after GetGameAPI and before Init, and
// the structure it hands over stays valid for the life of the library.  It is
// how R-BOT-8 resolves basedir/gamedir/cddir where the cvars cannot, how
// R-BOT-26 enumerates bots/*.cfg inside a pak, and how R-BOT-27 reaches
// DEBUG_DRAW_API_V1.  Every one of these answers something sensible when the
// engine offers no extension at all -- q2pro only exposes the debug-draw
// extension in a non-dedicated debug build, so the fallback is the normal case.
// The engine's extended import table, captured by GetGameAPIEx in g_main.c and
// read by src/g_fs.c.  NULL until the engine calls it, and NULL forever on an
// engine that does not.
extern const game_import_ex_t *gex;

const char *G_FsBaseDir(void);
const char *G_FsGameDir(void);
bool G_FsGamePath(char *out, size_t size, const char *name);
int         G_FsLoadFile(const char *path, void **buffer);
void        G_FsFreeFile(void *buffer);
char      **G_FsListFiles(const char *path, const char *ext, int *count);
void        G_FsFreeFileList(char **list);
const debug_draw_api_v1_t *G_DebugDraw(void);

// R-CTF-5, and the one predicate that replaces thirteen `resp.spectator` tests.
// "Is this client watching rather than playing?" has two answers in one library:
// baseq2's `resp.spectator` under dm and sp, and Threewave's
// `resp.ctf_team == CTF_NOTEAM` under ctf, which has no baseq2 spectator flag
// at all.  Asking the question by name means no call site has to know which.
bool G_IsObserver(edict_t *ent);
void G_SetSpectatorStats(edict_t *ent);
void G_CheckChaseStats(edict_t *ent);
void ValidateSelectedItem(edict_t *ent);
void DeathmatchScoreboardMessage(edict_t *client, edict_t *killer);

//
// p_weapon.c
//
void PlayerNoise(edict_t *who, vec3_t where, int type);
void P_ProjectSource(gclient_t *client, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result);
// R-COMPAT-6: the one registration of the statistics-log pair, in g_ruleset.c,
// shared by RA2's ra2stats.c and tourney's osp_stats.c.
extern cvar_t *g_statsfile;
extern cvar_t *g_statsname;

void ClientEndServerFrames(void);
void Weapon_Generic(edict_t *ent, int FRAME_ACTIVATE_LAST, int FRAME_FIRE_LAST, int FRAME_IDLE_LAST, int FRAME_DEACTIVATE_LAST, const int *pause_frames, const int *fire_frames, void (*fire)(edict_t *ent));
void ChangeWeapon(edict_t *ent);

//
// m_move.c
//
bool M_CheckBottom(edict_t *ent);
bool M_walkmove(edict_t *ent, float yaw, float dist);
void M_MoveToGoal(edict_t *ent, float dist);
void M_ChangeYaw(edict_t *ent);

//
// g_phys.c
//
void G_RunEntity(edict_t *ent);
void SV_AddGravity(edict_t *ent);
void rogue_SP_trigger_teleport(edict_t *self);
void rogue_SP_info_teleport_destination(edict_t *self);
void ra_SP_trigger_teleport(edict_t *self);

//
// g_chase.c
//
void UpdateChaseCam(edict_t *ent);
void ChaseNext(edict_t *ent);
void ChasePrev(edict_t *ent);
void GetChaseTarget(edict_t *ent);

//====================
// ROGUE PROTOTYPES
//
// g_newweap.c
//
//extern float nuke_framenum;

void fire_flechette(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int kick);
void fire_prox(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed);
void fire_nuke(edict_t *self, vec3_t start, vec3_t aimdir, int speed);
void fire_flame(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed);
void fire_burst(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed);
void fire_maintain(edict_t *, edict_t *, vec3_t start, vec3_t aimdir, int damage, int speed);
void fire_incendiary_grenade(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, float timer, float damage_radius);
void fire_player_melee(edict_t *self, vec3_t start, vec3_t aim, int reach, int damage, int kick, int quiet, int mod);
void fire_tesla(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed);
void fire_blaster2(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int effect, bool hyper);
void rogue_fire_heat(edict_t *self, const vec3_t start, const vec3_t aimdir, const vec3_t offset, int damage, int kick, bool monster);
void fire_tracker(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, edict_t *enemy);

//
// g_newai.c
//
bool blocked_checkshot(edict_t *self, float shotChance);
bool blocked_checkplat(edict_t *self, float dist);
bool blocked_checkjump(edict_t *self, float dist, float maxDown, float maxUp);
bool blocked_checknewenemy(edict_t *self);
bool monsterlost_checkhint(edict_t *self);
bool inback(edict_t *self, edict_t *other);
float realrange(edict_t *self, edict_t *other);
edict_t *SpawnBadArea(vec3_t mins, vec3_t maxs, float lifespan, edict_t *owner);
edict_t *CheckForBadArea(edict_t *ent);
bool MarkTeslaArea(edict_t *self, edict_t *tesla);
void InitHintPaths(void);
void PredictAim(edict_t *target, vec3_t start, float bolt_speed, bool eye_height, float offset, vec3_t aimdir, vec3_t aimpoint);
bool below(edict_t *self, edict_t *other);
void drawbbox(edict_t *self);
void M_MonsterDodge(edict_t *self, edict_t *attacker, float eta, trace_t *tr);
void monster_duck_down(edict_t *self);
void monster_duck_hold(edict_t *self);
void monster_duck_up(edict_t *self);
bool has_valid_enemy(edict_t *self);
void TargetTesla(edict_t *self, edict_t *tesla);
void hintpath_stop(edict_t *self);
edict_t * PickCoopTarget(edict_t *self);
int CountPlayers(void);
void monster_jump_start(edict_t *self);
bool monster_jump_finished(edict_t *self);

//
// g_sphere.c
//
void Defender_Launch(edict_t *self);
void Vengeance_Launch(edict_t *self);
void Hunter_Launch(edict_t *self);

//
// g_newdm.c
//
void InitGameRules(void);
edict_t *DoRandomRespawn(edict_t *ent);
void PrecacheForRandomRespawn(void);
bool Tag_PickupToken(edict_t *ent, edict_t *other);
void Tag_DropToken(edict_t *ent, const gitem_t *item);
void Tag_PlayerDeath(edict_t *targ, edict_t *inflictor, edict_t *attacker);
void fire_doppleganger(edict_t *ent, vec3_t start, vec3_t aimdir);

//
// g_spawn.c
//
edict_t *CreateMonster(const vec3_t origin, const vec3_t angles, const char *classname);
edict_t *CreateFlyMonster(vec3_t origin, vec3_t angles, vec3_t mins, vec3_t maxs, char *classname);
edict_t *CreateGroundMonster(const vec3_t origin, const vec3_t angles, const vec3_t mins, const vec3_t maxs, const char *classname, int height);
bool FindSpawnPoint(vec3_t startpoint, vec3_t mins, vec3_t maxs, vec3_t spawnpoint, float maxMoveUp);
bool CheckSpawnPoint(const vec3_t origin, const vec3_t mins, const vec3_t maxs);
bool CheckGroundSpawnPoint(const vec3_t origin, const vec3_t entMins, const vec3_t entMaxs, float height, float gravity);
void DetermineBBox(const char *classname, vec3_t mins, vec3_t maxs);
void SpawnGrow_Spawn(vec3_t startpos, int size);
void Widowlegs_Spawn(vec3_t startpos, vec3_t angles);

//
// p_client.c
//
void RemoveAttackingPainDaemons(edict_t *self);

//
// g_log.c -- the Gladiator game log (R-EXTRA-1)
//
void Log_Open(const char *filename);
void Log_Close(void);
void Log_ShutDown(void);
void Log_Write(const char *fmt, ...) q_printf(1, 2);
void Log_WriteTimeStamped(const char *fmt, ...) q_printf(1, 2);
bool LogCmd(const char *cmd);
bool Log_IsOpen(void);
const char *Log_Path(void);
int Log_Writes(void);
extern cvar_t *g_gamelog;

//
// The rest of R-EXTRA's `#define`s, as cvars.  R-EXTRA's rule is that each is
// kept, each becomes a cvar and none is compiled out; these three are the ones
// the requirement also says are OFF by default, which for a spawn function
// means the entity is not created rather than created and inert.
//
extern cvar_t *g_clientlag;         // R-EXTRA-2, CLIENTLAG
extern cvar_t *g_observer;          // R-EXTRA-6, OBSERVER (ctf and sp only)

// Is the GLADIATOR observer the implementation in force?  R-EXTRA-6 is the
// second exemption to sec 7 rule 6 -- three implementations, one per ruleset --
// so this is a ruleset question and not a cvar question, with the cvar folded
// in as the operator's request the way G_ResolveModifiers does it (R-88).
//
// It is an EVERYTHING-EXCEPT test, which is the shape that goes wrong when a
// ruleset is added: it named RULESET_ARENA and RULESET_TOURNEY, so `dmpro`,
// `tdm` and `duel` would each have switched Gladiator's observer on underneath
// OSP's own -- two live observer systems, which is the bug sec 7 rule 6 names.
// G_IsOspRuleset() is why it does not: the four answer as one.  What is left
// after the exclusions is `ctf` and `sp`.
static inline bool G_GladiatorObserver(void)
{
    return g_observer->value &&
           G_Ruleset() != RULESET_ARENA && !G_IsOspRuleset();
}
extern cvar_t *g_triggercounting;   // R-EXTRA-3, TRIGGER_COUNTING
extern cvar_t *g_triggerlog;        // R-EXTRA-3, TRIGGER_LOG
extern cvar_t *g_rotatingbutton;    // R-EXTRA-4, FUNC_BUTTON_ROTATING

//
// p_lag.c -- the Gladiator client-lag simulation (R-EXTRA-2)
//
// The ceiling on both `lag` and `lagvariance`, in milliseconds.  The donor
// spelled 2000 in four places and clamped the ping from the unclamped value in
// a fifth.
#define LAG_MAX_DELAY   2000

void Lag_StoreClientInput(edict_t *ent, usercmd_t *ucmd, vec3_t origin, vec3_t v_angle);
bool Lag_GetClientInput(edict_t *ent, usercmd_t *laggeducmd, vec3_t origin, vec3_t v_angle);
void Lag_BeginGame(edict_t *ent);
void Lag_SetClientLag(edict_t *ent, int delay);
void Lag_SetClientLagVariance(edict_t *ent, int lagvariance);
void Lag_SetClientPing(edict_t *ent);
void Lag_ForgetGameMemory(void);
int Lag_PoolBlocks(void);
void Lag_ClientState(edict_t *ent, int *lag, int *variance, int *delay, int *queued);

void SP_trigger_counting(edict_t *self);
void SP_trigger_log(edict_t *self);
void SP_func_button_rotating(edict_t *ent);
bool G_SpawnFuncExists(const char *classname);

//
// Cross-file declarations that used to be local `extern`s in a .c (R-SEC-8).
//
// Each of these names something another translation unit defines.  Declared in
// a .c, the two sides never meet and nothing checks them -- which is how RA2
// shipped four `bool[7]` arrays written through a stale `extern int[]` in
// another file, 21 bytes out of bounds on every map load, with a clean build.
// Declared here, the defining file includes the same line and the compiler
// does the checking.
//
void M_WorldEffects(edict_t *ent);              // g_monster.c
void train_use(edict_t *self, edict_t *other, edict_t *activator);  // g_func.c
void func_train_find(edict_t *self);            // g_func.c
void SP_item_foodcube(edict_t *self);           // g_items.c
void SP_monster_makron(edict_t *self);          // m_boss32.c
bool Pickup_Health(edict_t *ent, edict_t *other);       // g_items.c
bool Pickup_Adrenaline(edict_t *ent, edict_t *other);   // g_items.c
bool Pickup_Armor(edict_t *ent, edict_t *other);        // g_items.c
bool Pickup_PowerArmor(edict_t *ent, edict_t *other);   // g_items.c
bool Pickup_Sphere(edict_t *ent, edict_t *other);       // g_items.c
edict_t *Sphere_Spawn(edict_t *owner, int spawnflags);  // rogue/g_sphere.c
void check_dodge(edict_t *self, vec3_t start, vec3_t dir, int speed);   // g_weapon.c
void hurt_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf);  // g_trigger.c
void droptofloor(edict_t *ent);                 // g_items.c
void Grenade_Explode(edict_t *ent);             // g_weapon.c
byte P_DamageModifier(edict_t *ent);            // p_weapon.c
char *ED_NewString(const char *string);         // g_spawn.c
void Move_Calc(edict_t *ent, const vec3_t dest, void (*func)(edict_t *));  // g_func.c
// m_flyer.c's three tables, which m_carrier.c drives directly.
extern const mmove_t flyer_move_attack2;
extern const mmove_t flyer_move_attack3;
extern const mmove_t flyer_move_kamikaze;
// m_widow.c's shared stalker bounding box, read by m_widow2.c.
extern vec3_t stalker_mins, stalker_maxs;

// ROGUE PROTOTYPES
//====================

void G_AddPrecache(void (*func)(void));
void G_RefreshPrecaches(void);
void ED_CallSpawn(edict_t *ent);
void SpawnEntities(const char *mapname, const char *entities, const char *spawnpoint);

//
// g_save.c
//
void WriteGame(const char *filename, qboolean autosave);
void ReadGame(const char *filename);
void WriteLevel(const char *filename);
void ReadLevel(const char *filename);

//============================================================================

// client_t->anim_priority
#define ANIM_BASIC      0       // stand / run
#define ANIM_WAVE       1
#define ANIM_JUMP       2
#define ANIM_PAIN       3
#define ANIM_ATTACK     4
#define ANIM_DEATH      5
#define ANIM_REVERSE    6

// client data that stays across multiple level loads
typedef struct {
    char        userinfo[MAX_INFO_STRING];
    char        netname[16];
    int         hand;

    bool        connected;  // a loadgame will leave valid entities that
                            // just don't have a connection yet

    // values saved and restored from edicts when changing levels
    int         health;
    int         max_health;
    int         savedFlags;

    int         selected_item;
    int         inventory[MAX_ITEMS];

    // ammo capacities
    int         max_bullets;
    int         max_shells;
    int         max_rockets;
    int         max_grenades;
    int         max_cells;
    int         max_slugs;
    // RAFAEL
    int         max_magslug;
    int         max_trap;

    const gitem_t   *weapon;
    const gitem_t   *lastweapon;

    int         power_cubes;    // used for tracking the cubes in coop games
    int         score;          // for calculating total unit score in coop games

    int         game_helpchanged;
    int         helpchanged;

    bool    spectator;          // client is a spectator

    // RA2: the message of the day is shown once, on the first spawn.
    bool        showmotd;

    // OSP: the "green name" a referee or admin is drawn with.
    char        greenname[16];

    // OSP's speed-cheat strike counter.  The donor OVERLOADS baseq2's
    // `spectator` for this -- its g_local.h says so in as many words,
    // `int spectator; // client is a spectator, and a strike counter` -- and in
    // this tree that member is baseq2's `bool`.  Merged as-is, `++` saturates
    // at 1 so `>= 3` never fires and OSP_speedDetect kicks nobody; worse, the
    // first strike would make the player a SPECTATOR.  R-58's class exactly,
    // and the third instance of it, so it gets its own field (sec 7 rule 4).
    int         osp_speedstrikes;

    // The host player of a LISTEN server, latched at connect (R-BOT-28).  It is
    // a fact about the connection, so it is read exactly once, from the
    // userinfo the engine hands ClientConnect: q2pro force-sets `ip` in the
    // connect packet only (`SVC_DirectConnect` -> `parse_userinfo`), and every
    // later userinfo update is whatever the client sent -- `setu ip loopback`
    // is a command every client has.  Reading it live would hand the bot menu
    // to anybody.
    bool        listenhost;

//=========
//ROGUE
    int         max_tesla;
    int         max_prox;
    int         max_mines;
    int         max_flechettes;
//ROGUE
//=========
} client_persistant_t;

// The Gladiator observer's camera (R-EXTRA-6), for `dm`, `sp` and `ctf`.  One
// per client because the eye and chase cameras are per viewer -- two observers
// watching the same player have different smoothing state and different
// offsets.  `ent`, `lastent` and `goalent` are edict pointers and therefore
// need savegame descriptors, which g_save.c has (R-SAVE-3).
typedef struct camera_s {
    edict_t *ent;                   //observed client
    vec3_t  angles;                 //camera angles
    vec3_t  origin;                 //camera origin
    vec3_t  ent_angles;             //observed client angles
    vec3_t  chaseoffset;            //offset for the chasecamera
    int     flags;                  //camera flags, CAMFL_*
    //autocam fields
    edict_t *lastent;               //targeted entity (player, bot etc)
    edict_t *goalent;               //previous targeted entity
    vec3_t  dest;                   //current expected camera position
    vec3_t  viewtarget;             //current expected camera view target
    vec3_t  dest2;                  //next target in Idle Mode
    int     state;                  //the state of the camera
    float   pause_time;             //delay measure
    float   delay;                  //how long to keep current target / idle mode
    float   search_time;            //when to drop from current mode
    float   maxflybydist;             //maximum distance in flyby mode
    int     cnt;                    //entity passing counter
    //
    float   lasttime;               //last time camera was updated
    float   lastcycle;              //last time camera was cycled
    vec3_t  clientangles;           //angles of the client using this camera
    vec3_t  clientorigin;           //origin of the client using this camera
} camera_t;

// RA2's observer modes (R-EXTRA-6, the second exemption to sec 7 rule 6).  The
// eye and chase cameras of `dm`/`ctf`/`sp` are g_chase.c's; these four are
// arena.c's own and are not reconciled away, because RA2's observers are the
// voting electorate and the queue audience that R-RA-4 depends on.
typedef enum {
    OMODE_NORMAL,
    OMODE_FREEFLYING,
    OMODE_TRACKCAM,
    OMODE_EYECAM
} observer_mode_t;

// client data that stays across deathmatch respawns
typedef struct {
    client_persistant_t coop_respawn;   // what to set client->pers to on a respawn
    int         enterframe;         // level.framenum the client entered the game
    int         score;              // frags, etc
    vec3_t      cmd_angles;         // angles sent over in the last command

    bool        spectator;          // client is a spectator

    // CTF.  Threewave DELETED pers.spectator and resp.spectator and expressed
    // "watching rather than playing" as ctf_team == CTF_NOTEAM instead
    // (R-CTF-5).  The merged struct keeps both, because dm and sp still need
    // baseq2's flag and R-CORE-8 keeps the campaign in scope; G_IsObserver()
    // is the one place that knows which is authoritative.
    int         ctf_team;
    int         ctf_state;
    float       ctf_lasthurtcarrier;
    float       ctf_lastreturnedflag;
    float       ctf_flagsince;
    float       ctf_lastfraggedcarrier;
    bool        id_state;           // player-id display, on by default (R-CTF-6)
    float       lastidtime;
    bool        voted;              // for elections
    bool        ready;
    bool        admin;
    ghost_t     *ghost;             // ghost code, for reconnecting mid-match

    // Rocket Arena 2 (R-RA-1..6, R-ARENA-1..4).  `context` is the arena whose
    // menu the client is looking at, which is not always the arena it is in.
    // The reconstruction's byte-layout padding members (`_unidentified*`) are
    // NOT carried: N1 says nothing here is address- or byte-matched, so a hole
    // that exists to reproduce a 1999 struct offset is a hole with no reason.
    int         teamnum;            // -1 when on no team
    int         fightstate;         // FIGHT_SPECTATING / _ALIVE / _DEAD
    int         context;            // arena the menus are operating on
    qmenu_t     teammember;         // this client's node in its team's list
    int         spawn_recheck;      // framenum: retry the telefrag push
    observer_mode_t omode;          // R-EXTRA-6's four modes
    observer_mode_t lastomode;
    int         omode_buttons;
    edict_t     *track_target;      // TRACKCAM/EYECAM subject
    bool        entered;            // has been in the game at least once
    // `ra_voted` keeps the donor prefix: CTF's election flag above is also
    // `voted`, the two vote systems are different (sec 7 rule 6 lists them) and
    // one struct cannot hold two members of one name.
    bool        ra_voted;
    int         ra_votes;
    int         damagedealt;        // for scorebydamage

    // OSP's `entered` is a STATE, not a flag: 1 entered, 2 observing, 16
    // autocam (its ENTERED_* values).  RA2's above is a bool meaning "has been
    // in the game at least once".  Same name, two donors, two types -- R-58's
    // class, and the second instance of it in this import, so it gets its own
    // field rather than a silent truncation to 0 or 1 (sec 7 rule 4).
    int         osp_entered;

    // RA2's ZBot aim-cheat detector.  Kept as the donor has it; R-SEC's sweep
    // in Phase 8 decides whether it stays enabled by default.
    int         zbotcount;
    float       zbotlastcheck;
    int         isbot;

    // ---- OSP Tourney DM (R-OSP-1..13) -------------------------------------
    //
    // 736 bytes of the mod's own per-match state.  `osp_rNNN` is the field's
    // byte offset inside that block in the shipped 1999 binary and nothing
    // more: the reconstruction names a field only when something in the code
    // names it, and 62 of these are still unnamed.
    //
    // They are carried VERBATIM, which is the opposite of what R-67 did with
    // RA2's `_unidentified` padding -- and the difference is measurable rather
    // than a matter of taste.  RA2's were padding: declared, never referenced,
    // existing only to reproduce a struct offset, so dropping them cost
    // nothing.  These are **live state**: 692 references across
    // `src/tourney/`.  Renaming them would invent meaning the reconstruction
    // deliberately declined to claim, and dropping them would break every one
    // of those sites.  A name that says "I do not know" is worth more than a
    // guess that says something false.
    // OSP: 736 bytes of the mod's own per-match state, at the end of the struct.
    // `osp_rNNN` is the field's byte offset inside this block and nothing more;
    // a field gets a real name only when something actually names it.
    int       osp_r000;
    int       clientid;
    int       osp_r00c;
    int       osp_r010;
    int       osp_r014;
    int       osp_r018;
    int       osp_r01c;
    int       osp_r024;
    int       osp_r028;
    int       osp_r02c;
    int       osp_r030;
    int       osp_r034;
    char      osp_r038[64]; // the last player-ID string sent
    int       osp_r078;
    byte      osp_r07c[1];
    // `char`, and a joincode string.
    char      osp_r07d[19];
    // OSP_showFrags and OSP_setStats cache what they last pushed into the
    // status bar here, so an unchanged cell costs no network traffic.
    int       osp_r090;
    int       osp_r094;
    int       osp_r098;
    int       osp_r09c;
    int       osp_r0a0;     // countdown, decremented by CameraCmd
    byte      osp_r0a4[4];
    int       osp_r0a8;
    int       osp_r0ac;
    int       osp_r0b0;
    int       osp_r0d4;
    int       osp_r0d8;
    int       osp_r0dc;
    int       osp_r0e0;
    int       osp_r0e4;
    int       osp_r0e8;
    int       osp_r0ec;
    int       osp_r0f0;
    char      osp_r0f4[256];   // the skin name in force for this client
    // Invented names: ClientThink's 16-sample ping accumulator.
    int       osp_r1f4;     // sample count
    unsigned  osp_r1f8;     // sample sum
    int       osp_r1fc;     // next sample frame
    int       osp_r200;
    int       osp_r204;
    int       osp_r208;
    int       osp_r20c;
    int       osp_r210;
    // `char`: a saved netname.
    char      osp_r214[32];
    int       osp_r234;
    int       osp_r238;
    int       osp_r23c;
    int       osp_r240;
    int       osp_r244;
    int       osp_r248;
    unsigned  osp_r24c;
    int       osp_r250;
    int       osp_r254;
    int       osp_r258;
    int       osp_r25c;
    int       osp_r260;
    int       osp_r264;
    int       osp_r268;
    byte      osp_r26c[36];
    int       osp_r290;
    int       osp_r294;
    int       osp_r298;
    int       osp_r29c;
    int       osp_r2a0;
    int       osp_r2a4;
    int       osp_r2a8;
    int       osp_r2ac;
    int       osp_r2b0;
    int       osp_r2b4;
    int       osp_r2b8;
    int       osp_r2bc;
    int       osp_r2c0;
    int       osp_r2c4;
    int       team;         // the team index
    int       osp_r2cc;
    int       osp_r2d0;
    int       osp_r2d4;
    int       osp_r2d8;
    int       osp_r2dc;
} client_respawn_t;

// this structure is cleared on each PutClientInServer(),
// except for 'client->pers'
struct gclient_s {
    // known to server
    player_state_t  ps;             // communicated by server to clients
    int             ping;

    // private to game
    client_persistant_t pers;
    client_respawn_t    resp;
    pmove_state_t       old_pmove;  // for detecting out-of-pmove changes

    bool        showscores;         // set layout stat
    bool        showinventory;      // set layout stat
    bool        showhelp;
    bool        showhelpicon;

    // RA2 replaced baseq2's `showscores` bool with a small enum, because its
    // scoreboard has more than one mode.  Both are kept: `showscores` is what
    // dm, ctf and sp test, `scoremode` is what arena tests (sec 7 rule 3).
    int         scoremode;

    // RA2: ZBot detection samples, and its own flood counter.  q2pro's
    // FloodProtect() is the one that runs (sec 7 rule 6); these two are the
    // donor's spam counter and are kept with its code rather than reconciled
    // into a second answer.
    //
    // `zbotscore` is the PORT the client connected from, stamped by
    // ClientConnect and read once by InitClientResp -- the misleading name is
    // the donor's.  RA_ZBOT_PORT is what the cheat client of the day used.
    int         zbotscore;
    short       oldangles[2][2];
    int         spamcount;
    float       spamtime;

    // R-MENU-2a: the menu owner is a SINGLE field, and every engine's open path
    // goes through G_MenuOpen() which closes the incumbent first.  Four donor
    // menu engines contend for one client input channel (R-MENU-1) and nothing
    // else structurally prevents two being open at once, so this field -- not a
    // per-engine `inmenu` boolean -- is what makes R-MENU-3 true.  Threewave's
    // own `inmenu` is therefore not carried: it would be a second answer to a
    // question that must have one.
    menu_owner_t    menu_owner;
    ctf_pmenuhnd_t  *ctf_menu;      // MENU_CTF's handle
    float           menutime;       // next allowed refresh
    bool            menudirty;

    // MENU_TOURNEY's handle.  `osp_` per sec 7 rule 4: OSP's pmenu_t puts `arg`
    // on the ENTRY and Threewave's on the HANDLE, so the two engines' types are
    // incompatible and cannot share a name.  OSP's own `inmenu` bool is NOT
    // carried -- R-MENU-2a says the owner is one field, and that is menu_owner.
    osp_pmenuhnd_t  *osp_menu;

    // MENU_BOT's state (R-BOT-28).  By value, because the Gladiator engine
    // keeps the cursor and the current submenu per client and the tree itself
    // is shared.  Its own `showmenu` bool is NOT carried, for the same reason
    // RA2's was not: menu_owner is the one answer to "is a menu open".
    menustate_t     menustate;
    // The SDK paints a "loading" pic over the layout while a bot is being
    // created, and the menu has to keep painting it if one is open.  Emptied
    // under tourney (R-BOT-29), whose own bar owns that channel.
    bool            showloading;

    // ---- OSP Tourney DM (R-OSP-1..13) -------------------------------------
    // Offset-named for the same reason client_respawn_t's are, and carried for
    // the same reason: they are live state, not padding.  Several DO have a
    // meaning the reconstruction was able to name, and those carry it.
    float     osp_t00c;
    int       osp_t018;
    byte      osp_t01c[4];
    float     osp_t020;
    int       osp_t024;
    short     osp_t028[2];  // the previous ucmd->angles pair
    byte      osp_t02c[8];
    byte      osp_t034[4];
    int       osp_t038;
    edict_t   *osp_t03c;    // camera target
    int       osp_t040;
    int       osp_t044;
    vec3_t    osp_t048;     // camera target death position
    double    osp_t054;     // camera XY lag
    double    osp_t05c;     // camera Z lag
    double    osp_t064;     // camera angle lag
    float     osp_t06c;
    float     osp_t070;
    float     osp_t074;
    float     osp_t078;
    float     osp_t07c;
    float     osp_t080;
    float     osp_t084;

    // MENU_ARENA's state.  RA2 carried a `showmenu` bool of its own; it is not
    // here, because R-MENU-2a says the owner is ONE field and a second boolean
    // is a second answer to the same question -- arena.c and menu.c ask
    // `menu_owner == MENU_ARENA` instead.  `ra_menutime` keeps the donor prefix
    // of sec 7 rule 4: RA2's is an int frame count, ours above is float seconds,
    // and two members of one struct cannot share a name and disagree on both.
    qmenu_t         menuqueue;      // stack of open menus
    qmenu_t         *curmenulink;   // the one being drawn
    qmenu_t         *selected;      // highlighted item
    int             ra_menutime;    // framenum: next allowed redraw
    int             menuusetime;
    char            menutext[MAXMENUTEXT];

    int         ammo_index;

    int         buttons;
    int         oldbuttons;
    int         latched_buttons;

    bool        weapon_thunk;

    const gitem_t   *newweapon;

    // sum up damage over an entire frame, so
    // shotgun blasts give a single big kick
    int         damage_armor;       // damage absorbed by armor
    int         damage_parmor;      // damage absorbed by power armor
    int         damage_blood;       // damage taken out of health
    int         damage_knockback;   // impact damage
    vec3_t      damage_from;        // origin for vector calculation

    float       killer_yaw;         // when dead, look at killer

    weaponstate_t   weaponstate;
    vec3_t      kick_angles;    // weapon kicks
    vec3_t      kick_origin;
    float       v_dmg_roll, v_dmg_pitch, v_dmg_time;    // damage kicks
    float       fall_time, fall_value;      // for view drop on fall
    float       damage_alpha;
    float       bonus_alpha;
    vec3_t      damage_blend;
    vec3_t      v_angle;            // aiming direction
    float       bobtime;            // so off-ground doesn't change it
    vec3_t      oldviewangles;
    vec3_t      oldvelocity;

    int       next_drown_framenum;
    int         old_waterlevel;
    int         breather_sound;

    int         machinegun_shots;   // for weapon raising

    // animation vars
    int         anim_end;
    int         anim_priority;
    bool        anim_duck;
    bool        anim_run;

    // powerup timers
    int         quad_framenum;
    int         invincible_framenum;
    int         breather_framenum;
    int         enviro_framenum;

    bool        grenade_blew_up;
    int         grenade_framenum;
    // XATRIX.  quadfire_framenum is R-VER-15 item 2's named case: it was
    // missing from the client save table in the mission-pack port, so Quad-Fire
    // did not survive a save/load.  R-SAVE-3 requires a descriptor.
    int         quadfire_framenum;
    bool        trap_blew_up;
    float       trap_time;
    int         silencer_shots;
    int         weapon_sound;

    int       pickup_msg_framenum;

#define FLOOD_MSGS  10

    float       flood_locktill;             // locked from talking
    float       flood_when[FLOOD_MSGS];     // when messages were said
    int         flood_whenhead;             // head pointer for when said

    int       respawn_framenum;       // can respawn when time > this

    edict_t     *chase_target;      // player we are chasing
    bool    update_chase;       // need to update chase info?

    // CTF.  ctf_grapple is an edict_t*; Threewave declares it `void *` only
    // because g_local.h had not defined edict_t yet at that point in its own
    // file.  It has here, so it is typed.
    //
    // The timeouts stay `float level.time` seconds rather than being converted
    // to the `_framenum` model.  §7 rule 1 says q2pro wins on a timer, and
    // q2pro's own CTF port is the tree that answers what q2pro thinks here: it
    // kept every one of these as a float.  level.time is live and saved, the
    // conversion would touch ~40 sites in g_ctf.c for no behavioural gain, and
    // the _framenum model in this tree covers client powerup timers and entity
    // debounces, not mod-internal timeouts (doc/reconciliation.md R-41).
    edict_t     *ctf_grapple;
    int         ctf_grapplestate;
    int         ctf_hookstate;      // the offhand hook's latch (R-CTF-3)
    float       ctf_grapplereleasetime;
    float       ctf_regentime;
    float       ctf_techsndtime;
    float       ctf_lasttechmsg;

//=======
//ROGUE
    int         double_framenum;
    int         ir_framenum;
//  float       torch_framenum;
    int         nuke_framenum;
    int         tracker_pain_framenum;

    edict_t     *owned_sphere;      // this points to the player's sphere
//ROGUE
//=======

    // R-EXTRA-6: the Gladiator observer's eye and chase camera, live under
    // `dm`, `sp` and `ctf`.  Not in a union with arena's `omode` pair or
    // tourney's camera state: the three implementations are per ruleset and one
    // library serves all of them, and R-70's six sites are what sharing a field
    // between two donors' meanings costs.
    camera_t    camera;
};

struct edict_s {
    entity_state_t  s;
    struct gclient_s    *client;    // NULL if not a player
                                    // the server expects the first part
                                    // of gclient_s to be a player_state_t
                                    // but the rest of it is opaque

    qboolean    inuse;
    int         linkcount;

    // FIXME: move these fields to a server private sv_entity_t
    list_t      area;               // linked to a division node or leaf

    int         num_clusters;       // if -1, use headnode instead
    int         clusternums[MAX_ENT_CLUSTERS];
    int         headnode;           // unused if num_clusters != -1
    int         areanum, areanum2;

    //================================

    int         svflags;
    vec3_t      mins, maxs;
    vec3_t      absmin, absmax, size;
    solid_t     solid;
    int         clipmask;
    edict_t     *owner;

    //================================

    entity_state_extension_t    x;

    // DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
    // EXPECTS THE FIELDS IN THAT ORDER!

    //================================
    int         movetype;
    int         flags;

    char        *model;
    float       freetime;           // sv.time when the object was freed

    //
    // only used locally in game, not by server
    //
    char        *message;
    char        *classname;
    int         spawnflags;

    int       timestamp;

    float       angle;          // set in qe3, -1 = up, -2 = down
    char        *target;
    char        *targetname;
    char        *killtarget;
    char        *team;
    char        *pathtarget;
    char        *deathtarget;
    char        *combattarget;
    edict_t     *target_ent;

    float       speed, accel, decel;
    vec3_t      movedir;
    vec3_t      pos1, pos2;

    vec3_t      velocity;
    vec3_t      avelocity;
    int         mass;
    int       air_finished_framenum;
    float       gravity;        // per entity gravity multiplier (1.0 is normal)
                                // use for lowgrav artifact, flares

    edict_t     *goalentity;
    edict_t     *movetarget;
    float       yaw_speed;
    float       ideal_yaw;

    int         nextthink;
    void        (*prethink)(edict_t *ent);
    void        (*think)(edict_t *self);
    void        (*blocked)(edict_t *self, edict_t *other);         // move to moveinfo?
    void        (*touch)(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf);
    void        (*use)(edict_t *self, edict_t *other, edict_t *activator);
    void        (*pain)(edict_t *self, edict_t *other, float kick, int damage);
    void        (*die)(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point);

    int       touch_debounce_framenum;        // are all these legit?  do we need more/less of them?
    int       pain_debounce_framenum;
    int       damage_debounce_framenum;
    int       fly_sound_debounce_framenum;    // move to clientinfo
    int       last_move_framenum;

    int         health;
    int         max_health;
    int         gib_health;
    int         deadflag;
    float       show_hostile;

    int       powerarmor_framenum;

    char        *map;           // target_changelevel

    int         viewheight;     // height above origin where eyesight is determined
    int         takedamage;
    int         dmg;
    int         radius_dmg;
    float       dmg_radius;
    int         sounds;         // make this a spawntemp var?
    int         count;

    edict_t     *chain;
    edict_t     *enemy;
    edict_t     *oldenemy;
    edict_t     *activator;
    edict_t     *groundentity;
    int         groundentity_linkcount;
    edict_t     *teamchain;
    edict_t     *teammaster;

    edict_t     *mynoise;       // can go in client only
    edict_t     *mynoise2;

    int         noise_index;
    int         noise_index2;
    float       volume;
    float       attenuation;

    // timing variables
    float       wait;
    float       delay;          // before firing targets
    float       random;

    int         last_sound_framenum;

    int         watertype;
    int         waterlevel;

    vec3_t      move_origin;
    vec3_t      move_angles;

    // move this to clientinfo?
    int         light_level;

    int         style;          // also used as areaportal number

    const gitem_t   *item;      // for bonus items

    // common data blocks
    moveinfo_t      moveinfo;
    monsterinfo_t   monsterinfo;

    // XATRIX
    int         orders;

    // ROGUE.  gravityVector is R-SAVE-3a's named case -- it and the whole
    // blindfire set were lost to a missing descriptor row in the mission-pack
    // port.  R-SAVE-3 requires descriptors for all of these.
    int         plat2flags;
    vec3_t      offset;
    vec3_t      gravityVector;
    edict_t     *bad_area;
    edict_t     *hint_chain;
    edict_t     *monster_hint_chain;
    edict_t     *target_hint_chain;
    int         hint_chain_id;

    // R-CORE-11a and Q13: the content flavour, latched.
    //
    // R-CORE-11 gates a monster's frame tables at monster_start and forbids
    // re-reading the gate mid-move, because a live mmove_t pointer must stay
    // valid across a cvar change.  R-CORE-11a is the harder half: the files
    // every monster calls every frame -- g_ai.c, m_move.c, g_monster.c,
    // g_phys.c -- have no monster_start to latch at.  So the flavour lives
    // HERE, on the entity, set once at spawn, and those files read this field
    // and never a cvar.  A cvar change mid-map cannot reach a monster that has
    // already spawned.
    //
    // Two independent bits rather than an enum, which is Q13's closure:
    // R-MODE-3 allows `xatrix` and `rogue` on at once, and an enum would forbid
    // exactly the combination the R-MODE-7 matrix promises.
    int         content_flavour;
    float       lastMoveTime;

    // ---- OSP Tourney DM ---------------------------------------------------
    char            osp_e37c[32];   // the client's dotted-quad, as a string
    int             osp_e39c;
    int             osp_infochange_framenum;
    char            osp_e3a0[16];   // default team name
    char            osp_e3b0[80];   // default team skin
    int             osp_e400;
    int             osp_e404;
    int             osp_e408;
    byte            osp_e40c[20];
    int             osp_e420;
    // Tourney's four per-CLIENT strings of the same names as R-OSP-6's spawn
    // keys.  They are a different thing in a different struct, and the spawn
    // keys themselves are in spawn_temp_t, where the entity parser looks.
    //
    // All four are layout only, like the osp_eNNN placeholders above them.
    // `charname` USED to carry OSP's rename cooldown as a frame number cast to
    // and from this pointer; that stamp is `osp_infochange_framenum` now, which
    // is what R-79 added the field for.
    char            *name;
    char            *skin;
    char            *charfile;
    char            *charname;
    // The bot debug-draw bounding box (R-BOT-27), fourteen beam entities that
    // outline this entity when `sv bbox` is on for it.  Not saved: the lines
    // are TAG_LEVEL edicts and a reload rebuilds them from nothing.
    visiblebbox_t   box;

    // RA2: which arena this entity belongs to.  Zero on every map that is not
    // an arena map, which is what makes the handful of `if (ent->arena)` tests
    // in the spine files self-gating (doc/reconciliation.md R-6x).  A spawn key
    // and a savegame descriptor, both below.
    int         arena;
};

//=============
//ROGUE
#define ROGUE_GRAVITY   1

#define SPHERE_DEFENDER         0x0001
#define SPHERE_HUNTER           0x0002
#define SPHERE_VENGEANCE        0x0004
#define SPHERE_DOPPLEGANGER     0x0100

#define SPHERE_TYPE             0x00FF
#define SPHERE_FLAGS            0xFF00

//
// deathmatch games
//
#define     RDM_TAG         2
#define     RDM_DEATHBALL   3

typedef struct dm_game_rs {
    void (*GameInit)(void);
    void (*PostInitSetup)(void);
    void (*ClientBegin)(edict_t *ent);
    bool (*SelectSpawnPoint)(edict_t *ent, vec3_t origin, vec3_t angles);
    void (*PlayerDeath)(edict_t *targ, edict_t *inflictor, edict_t *attacker);
    void (*Score)(edict_t *attacker, edict_t *victim, int scoreChange);
    void (*PlayerEffects)(edict_t *ent);
    void (*DogTag)(edict_t *ent, edict_t *killer, char **pic);
    void (*PlayerDisconnect)(edict_t *ent);
    int (*ChangeDamage)(edict_t *targ, edict_t *attacker, int damage, int mod);
    int (*ChangeKnockback)(edict_t *targ, edict_t *attacker, int knockback, int mod);
    int (*CheckDMRules)(void);
} dm_game_rt;

extern dm_game_rt   DMGame;

void Tag_GameInit(void);
void Tag_PostInitSetup(void);
void Tag_PlayerDeath(edict_t *targ, edict_t *inflictor, edict_t *attacker);
void Tag_Score(edict_t *attacker, edict_t *victim, int scoreChange);
void Tag_PlayerEffects(edict_t *ent);
void Tag_DogTag(edict_t *ent, edict_t *killer, char **pic);
void Tag_PlayerDisconnect(edict_t *ent);
int  Tag_ChangeDamage(edict_t *targ, edict_t *attacker, int damage, int mod);

void DBall_GameInit(void);
void DBall_ClientBegin(edict_t *ent);
bool DBall_SelectSpawnPoint(edict_t *ent, vec3_t origin, vec3_t angles);
int  DBall_ChangeKnockback(edict_t *targ, edict_t *attacker, int knockback, int mod);
int  DBall_ChangeDamage(edict_t *targ, edict_t *attacker, int damage, int mod);
void DBall_PostInitSetup(void);
int  DBall_CheckDMRules(void);
//void Tag_PlayerDeath (edict_t *targ, edict_t *inflictor, edict_t *attacker);
//void Tag_Score (edict_t *attacker, edict_t *victim, int scoreChange);
//void Tag_PlayerEffects (edict_t *ent);
//void Tag_DogTag (edict_t *ent, edict_t *killer, char **pic);
//void Tag_PlayerDisconnect (edict_t *ent);
//int  Tag_ChangeDamage (edict_t *targ, edict_t *attacker, int damage);

//ROGUE
//============

// Threewave CTF, at the bottom because g_ctf.h's prototypes need edict_t and
// gclient_t.  Qualified path, never -Isrc/ctf: §5.2 has CTF and tourney both
// shipping a p_menu.h, so putting a donor directory on the include path is
// precisely the thing that breaks later (R-25 item 3).
#include "ctf/g_ctf.h"
