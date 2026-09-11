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
// The Gladiator Bot SDK's headers, from osp-tourney@1d8427e -- which carries a
// working Q2PRO port of the 1999 glue.  src/tourney/ is written against these,
// and botglobals must be declared exactly once and included.  Donor-only:
// baseq2 has no counterpart, so it lives in src/tourney/ rather than being
// merged into a spine file.  The reconstruction's asm-matching address
// comments are stripped -- those oracles are meaningless here, and they
// survive at the pin.
//===========================================================================
//
// Name:         botlib.h
// Function:     Bot library header
// Programmer:   Mr Elusive (MrElusive@demigod.demon.nl)
// Last update:  1999-02-10
// Tab Size:     3
//===========================================================================

//debug line colors
#define LINECOLOR_NONE          -1
#define LINECOLOR_RED           0xf2f2f0f0L
#define LINECOLOR_GREEN         0xd0d1d2d3L
#define LINECOLOR_BLUE          0xf3f3f1f1L
#define LINECOLOR_YELLOW        0xdcdddedfL
#define LINECOLOR_ORANGE        0xe0e1e2e3L

//Print types
#define PRT_MESSAGE             1
#define PRT_WARNING             2
#define PRT_ERROR                   3
#define PRT_FATAL                   4
#define PRT_EXIT                    5

//console message types
#define CMS_NORMAL              0
#define CMS_CHAT                    1

//some maxs
#define MAX_NETNAME             16
#define MAX_CLIENTSKINNAME      128
#define MAX_FILEPATH                144
#define MAX_CHARACTERNAME       144

//action flags
#define ACTION_ATTACK           1
#define ACTION_USE              2
#define ACTION_RESPAWN          4
#define ACTION_JUMP             8
#define ACTION_MOVEUP           8
#define ACTION_CROUCH           16
#define ACTION_MOVEDOWN         16
#define ACTION_MOVEFORWARD      32
#define ACTION_MOVEBACK         64
#define ACTION_MOVELEFT         128
#define ACTION_MOVERIGHT        256
#define ACTION_DELAYEDJUMP      512

//botlib error codes
#define BLERR_NOERROR                       0   //no error
#define BLERR_LIBRARYNOTSETUP               1   //library not setup
#define BLERR_LIBRARYALREADYSETUP       2   //BotSetupLibrary: library already setup
#define BLERR_INVALIDCLIENTNUMBER       3   //invalid client number
#define BLERR_INVALIDENTITYNUMBER       4   //invalid entity number
#define BLERR_NOAASFILE                     5   //BotLoadMap: no AAS file available
#define BLERR_CANNOTOPENAASFILE         6   //BotLoadMap: cannot open AAS file
#define BLERR_CANNOTSEEKTOAASFILE       7   //BotLoadMap: cannot seek to AAS file
#define BLERR_CANNOTREADAASHEADER       8   //BotLoadMap: cannot read AAS header
#define BLERR_WRONGAASFILEID                9   //BotLoadMap: incorrect AAS file id
#define BLERR_WRONGAASFILEVERSION       10  //BotLoadMap: incorrect AAS file version
#define BLERR_CANNOTREADAASLUMP         11  //BotLoadMap: cannot read AAS file lump
#define BLERR_NOBSPFILE                     12  //BotLoadMap: no BSP file available
#define BLERR_CANNOTOPENBSPFILE         13  //BotLoadMap: cannot open BSP file
#define BLERR_CANNOTSEEKTOBSPFILE       14  //BotLoadMap: cannot seek to BSP file
#define BLERR_CANNOTREADBSPHEADER       15  //BotLoadMap: cannot read BSP header
#define BLERR_WRONGBSPFILEID                16  //BotLoadMap: incorrect BSP file id
#define BLERR_WRONGBSPFILEVERSION       17  //BotLoadMap: incorrect BSP file version
#define BLERR_CANNOTREADBSPLUMP         18  //BotLoadMap: cannot read BSP file lump
#define BLERR_AICLIENTNOTSETUP          19  //BotAI: client not setup
#define BLERR_AICLIENTALREADYSETUP      20  //BotSetupClient: client already setup
#define BLERR_AIMOVEINACTIVECLIENT      21  //BotMoveClient: cannot move inactive client
#define BLERR_AIMOVETOACTIVECLIENT      22  //BotMoveClient: cannot move to active client
#define BLERR_AICLIENTALREADYSHUTDOWN   23  //BotShutdownClient: client not setup
#define BLERR_AIUPDATEINACTIVECLIENT    24  //BotUpdateClient: called for inactive client
#define BLERR_AICMFORINACTIVECLIENT     25  //BotConsoleMessage: called for inactive client
#define BLERR_SETTINGSINACTIVECLIENT    26  //BotClientSettings: called for inactive client
#define BLERR_CANNOTLOADICHAT               27  //BotSetupClient: cannot load initial chats
#define BLERR_CANNOTLOADITEMWEIGHTS     28  //BotSetupClient: cannot load item weights
#define BLERR_CANNOTLOADITEMCONFIG      29  //BotSetupLibrary: cannot load item config
#define BLERR_CANNOTLOADWEAPONWEIGHTS   30  //BotSetupClient: cannot load weapon weights
#define BLERR_CANNOTLOADWEAPONCONFIG    31  //BotSetupLibrary: cannot load weapon config
#define BLERR_INVALIDSOUNDINDEX         32  //BotAddSound: invalid sound index value

//bsp_trace_t hit surface
// ---------------------------------------------------------------------------
// The contract's own array bounds.
//
// The contract lists MAX_NETNAME, MAX_CLIENTSKINNAME, MAX_FILEPATH and
// MAX_CHARACTERNAME as fixed limits.  These two belong on that list and were
// not on it, because the 1999 header spells them with the ENGINE's names --
// which was safe in 1999, when the engine was id's, and is not safe now:
// Q2PRO's MAX_STATS is 64 under USE_NEW_GAME_API where the brain's q_shared.h
// has 32.  A contract array whose bound follows one side's engine is not a
// contract.  Both values are the brain's, and `tools/botabi.py` compares the
// _Static_asserts at the bottom of this header against the brain's own.
#define BOTLIB_MAX_STATS        32
#define BOTLIB_MAX_ITEMS        256

typedef struct bsp_surface_s
{
    char name[16];
    int flags;
    int value;
} bsp_surface_t;

//remove the bsp_trace_s structure definition l8r on
//a trace is returned when a box is swept through the world
typedef struct bsp_trace_s
{
    // `qboolean`, not `bool`, and the difference is four bytes each.  The 1999
    // header says qboolean, which is `enum { qfalse, qtrue }` on both sides --
    // Q2PRO keeps it and marks it "ABI compat only, don't use", which is
    // precisely the use it is for here.  osp-tourney's port swept the name to
    // `bool` along with the rest of its tree, and C99's `bool` is one byte, so
    // every field from `fraction` down moved four bytes up: the brain read our
    // endpos[0] as the trace fraction.  It compiled, it linked, the bots ran,
    // and their navigation was running on garbage.
    qboolean    allsolid;   // if true, plane is not valid
    qboolean    startsolid; // if true, the initial point was in a solid area
    float           fraction;   // time completed, 1.0 = didn't hit anything
    vec3_t      endpos;     // final position
    cplane_t        plane;      // surface normal at impact
    float           exp_dist;   // expanded plane distance
    int         sidenum;        // number of the brush side hit
    bsp_surface_t surface;  // the hit point surface
    int         contents;   // contents on other side of surface hit
    int         ent;            // number of entity hit
} bsp_trace_t;

//bot settings
typedef struct bot_settings_s
{
    char characterfile[MAX_FILEPATH];
    char charactername[MAX_CHARACTERNAME];
    char ailibrary[MAX_FILEPATH];
} bot_settings_t;

//client settings
typedef struct bot_clientsettings_s
{
    char netname[MAX_NETNAME];
    char skin[MAX_CLIENTSKINNAME];
} bot_clientsettings_t;

//the bot input, will be converted to an usercmd_t
typedef struct bot_input_s
{
    float thinktime;        //time since last output (in seconds)
    vec3_t dir;             //movement direction
    float speed;            //speed in the range [0, 400]
    vec3_t viewangles;  //the view angles
    int actionflags;        //one of the ACTION_? flags
} bot_input_t;

//bot client update
typedef struct bot_updateclient_s
{
    pmtype_t    pm_type;            // movement type
    vec3_t  origin;         // client origin
    vec3_t  velocity;       // client velocity
    byte        pm_flags;       // ducked, jump_held, etc
    byte        pm_time;            // each unit = 8 ms
    float       gravity;            // current gravity
    vec3_t  delta_angles;   // add to command angles to get view direction
                                    // changed by spawns, rotating objects, and teleporters
    //====================================
    vec3_t  viewangles;     // for fixed views
    vec3_t  viewoffset;     // add to origin for view coordinates
    vec3_t  kick_angles;    // add to view direction to get render angles
                                    // set by weapon kicks, pain effects, etc
    vec3_t  gunangles;      // angles of the gun
    vec3_t  gunoffset;      // offset of the gun relative to the client origin
    int     gunindex;       // gun model number
    int     gunframe;       // gun model frame number

    float       blend[4];       // rgba full screen effect
    float       fov;                // horizontal field of view
    int     rdflags;            // refdef flags
    // BOTLIB_MAX_STATS, not the engine's MAX_STATS.  Under USE_NEW_GAME_API
    // Q2PRO's is 64 and the brain's q_shared.h has 32, so writing the engine's
    // name here makes the struct 64 bytes longer than the one the brain
    // memcpy's into -- and the brain's memcpy is sized from ITS type, so the
    // whole inventory it reads is our stats[32..63] followed by our inventory
    // shifted sixteen slots down.  The contract's bound is the contract's.
    short       stats[BOTLIB_MAX_STATS];
    //====================================
    int     inventory[BOTLIB_MAX_ITEMS];
    //
} bot_updateclient_t;

//entity update
typedef struct bot_updateentity_s
{
    vec3_t  origin;         // origin of the entity
    vec3_t  angles;         // angles of the model
    vec3_t  old_origin;     // for lerping
    vec3_t  mins;               // bounding box minimums
    vec3_t  maxs;               // bounding box maximums
    int     solid;
    int     modelindex;     // model used
    int     modelindex2, modelindex3, modelindex4;  // weapons, CTF flags, etc
    int     frame;          // model frame number
    int     skinnum;            // skin number
    int     effects;            // special effects
    int     renderfx;       // render fx flags
    int     sound;          // for looping sounds, to guarantee shutoff
    int     event;          // impulse events -- muzzle flashes, footsteps, etc
                                    // events only go out for a single frame, they
                                    // are automatically cleared each frame
} bot_updateentity_t;

//bot library exported functions
typedef struct bot_export_s
{
    //returns the version of the library
    char *(*BotVersion)(void);
    //setup the bot library
    int (*BotSetupLibrary)(void);
    //shutdown the bot library
    int (*BotShutdownLibrary)(void);
    //returns true if the bot library has been initialized, false otherwise
    int (*BotLibraryInitialized)(void);
    //sets a library variable returns BLERR_
    int (*BotLibVarSet)(char *var_name, char *value);
    //sets a C-like define returns BLERR_
    int (*BotDefine)(char *string);
    //load a map for the bot clients returns BLERR_
    int (*BotLoadMap)(char *mapname, int modelindexes, char *modelindex[],
                                                int soundindexes, char *soundindex[],
                                                int imageindexes, char *imageindex[]);
    //setup a bot client (NOTE: returns true on success, false on failure)
    int (*BotSetupClient)(int client, bot_settings_t *settings);
    //shut down a bot client returns BLERR_
    int (*BotShutdownClient)(int client);
    //move a client to another client number returns BLERR_
    int (*BotMoveClient)(int oldclnum, int newclnum);
    //update the settings of a client returns BLERR_
    int (*BotClientSettings)(int client, bot_clientsettings_t *settings);
    //update the settings of a bot returns BLERR_
    int (*BotSettings)(int client, bot_settings_t *settings);
    //start the updates for this frame returns BLERR_
    int (*BotStartFrame)(float time);
    //update a bot client returns BLERR_
    int (*BotUpdateClient)(int client, bot_updateclient_t *buc);
    //update an entity returns BLERR_
    int (*BotUpdateEntity)(int ent, bot_updateentity_t *bue);
    //add a sound returns BLERR_
    int (*BotAddSound)(vec3_t origin, int ent, int channel, int soundindex,
                                        float volume, float attenuation, float timeofs);
    //add a point light returns BLERR_
    int (*BotAddPointLight)(vec3_t origin, int ent, float radius, float r, float g, float b, float time, float decay);
    //trigger the bot AI for a client returns BLERR_
    int (*BotAI)(int client, float thinktime);
    //send a console message to a bot client returns BLERR_
    int (*BotConsoleMessage)(int client, int type, char *message);
    //just for testing
    int (*Test)(int parm0, char *parm1, vec3_t parm2, vec3_t parm3);
} bot_export_t;

//bot library imported functions
typedef struct bot_import_s
{
    //recieve bot input
    void        (*BotInput)(int client, bot_input_t *bi);
    //recieve a bot client command
    void        (*BotClientCommand)(int client, char *str, ...);
    //print messages from the bot library
    void        (*Print)(int type, char *fmt, ...);
    // The aggregate-return hazard, and it took two rounds to settle.
    // `bsp_trace_t` is 84 bytes, so it is never returned in registers: the
    // caller passes a hidden buffer.  On 32-bit that buffer IS the first
    // visible argument, so `bsp_trace_t (*)(vec3_t start, ...)` and
    // `bsp_trace_t *(*)(bsp_trace_t *retbuf, vec3_t start, ...)` compile to
    // the same code; on x86-64 and aarch64 it is a hidden REGISTER (rax / x8)
    // and they are different ABIs.
    //
    // Colosseum first met this as a MISMATCH: osp-tourney's copy of this header
    // carries the by-value spelling, the brain's be_interface.h had grown an
    // `#if __x86_64__ || __aarch64__` branch declaring the retbuf explicitly,
    // and on aarch64 that put `start` where the brain expected the buffer and
    // shifted every argument one place -- `passent` received a truncated
    // pointer, every trace came back zeroed, and the bots stood still
    //1.22 matched the branch.
    //
    // Upstream then withdrew it (gladiator-bot-restored 57ce85a3), and its
    // reasoning is better than the match was: the PUBLISHED contract --
    // game/botlib.h, the header a foreign engine builds against -- was never
    // branched, so the branch only ever worked when both sides were theirs.
    // By value on every target is also the faithful 1999 spelling and the only
    // form that reproduces gladi386.so's trace thunk.  So the `#if` is gone
    // from both sides; contract version 3.
    //
    // NO `q_gameabi` here, and that is a decision rather than an omission.
    // The obvious move is "the same treatment Q2PRO applies to gi.trace"; that
    // treatment is `callee_pop_aggregate_return(0)`, which this build does not
    // enable (config.h sets USE_GAME_ABI_HACK 0, so q_gameabi expands to
    // nothing).  The brain's side carries no attribute at all, so if this build
    // ever turns the hack on, this slot must still not get it -- the two sides
    // have to agree, and the contract's spelling is the plain one.
    bsp_trace_t (*Trace)(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask);
    // const to match Q2PRO's gi.pointcontents, which is assigned straight
    // into this slot.  Only the pointer's type changes, not the ABI.
    int     (*PointContents)(const vec3_t point);
    //memory allocation
    void        *(*GetMemory)(int size);
    void        (*FreeMemory)(void *ptr);
    //debug shit
    int     (*DebugLineCreate)(void);
    void        (*DebugLineDelete)(int line);
    void        (*DebugLineShow)(int line, vec3_t start, vec3_t end, int color);
} bot_import_t;

//linking of bot library
bot_export_t *GetBotAPI(bot_import_t *import);

/* Library variables:

name:                           default:                module(s):              description:

"basedir"                   ""                      l_utils.c               Quake2 base directory
"gamedir"                   ""                      l_utils.c               Quake2 game directory
"cddir"                     ""                      l_utils.c               Quake2 CD directory

"autolaunchbspc"            "0"                 be_aas_load.c           automatically launch (Win)BSPC
"log"                   "0"               l_log.c              enable/disable creating a log file
"maxclients"                "4"                 be_interface.c          maximum number of clients
"maxentities"               "1024"              be_interface.c          maximum number of entities
"sv_friction"               "6"                 be_interface.c          ground friction
"sv_stopspeed"              "100"                   be_interface.c          stop speed
"sv_gravity"                "800"                   be_interface.c          gravity value
"sv_waterfriction"      "1"                 be_interface.c          water friction
"sv_watergravity"           "400"                   be_interface.c          gravity in water
"sv_maxvelocity"            "300"                   be_interface.c          maximum velocity
"sv_maxwalkvelocity"        "300"                   be_interface.c          maximum walk velocity
"sv_maxcrouchvelocity"  "100"                   be_interface.c          maximum crouch velocity
"sv_maxswimvelocity"        "150"                   be_interface.c          maximum swim velocity
"sv_maxacceleration"        "2200"              be_interface.c          maximum acceleration
"sv_airaccelerate"      "0"                 be_interface.c          maximum air acceleration
"sv_maxstep"                "18"                    be_interface.c          maximum step height
"sv_maxbarrier"         "50"                    be_interface.c          maximum barrier height
"sv_maxsteepness"           "0.7"                   be_interface.c          maximum floor steepness
"sv_jumpvel"                "224"                   be_interface.c          jump z velocity
"sv_maxwaterjump"           "20"                    be_interface.c          maximum waterjump height

"max_aaslinks"              "4096"              be_aas_sample.c     maximum links in the AAS
"max_bsplinks"              "4096"              be_aas_bsp.c            maximum links in the BSP

"notspawnflags"         "2048"              be_ai_goal.c            entities with these spawnflags will be removed
"weaponconfig"              "weapons.c"         be_ai_weap.c            weapon configuration file
"itemconfig"                "items.c"           be_ai_goal.c            item configuration file
"soundconfig"               "sounds.c"          be_aas_sound.c          sound configuration file
"matchfile"                 "match.c"           be_ai_chat.c            file with match strings
"max_messages"              "1024"              be_ai_chat.c            console message heap size
"max_weaponinfo"            "32"                    be_ai_weap.c            maximum number of weapon info
"max_projectileinfo"        "32"                    be_ai_weap.c            maximum number of projectile info
"max_iteminfo"              "256"                   be_ai_goal.c            maximum number of item info
"max_levelitems"            "256"                   be_ai_goal.c            maximum number of level items
"max_weights"               "256"                   be_ai_goal.c            maximum number of item weights
"max_soundinfo"         "256"                   be_aas_sound.c          maximum number of sound info
"max_aassounds"         "256"                   be_aas_sound.c          maximum number of playing AAS sounds
"framereachability"     ""                      be_aas_reach.c          number of reachabilities to calucate per frame
"forceclustering"           "0"                 be_aas_main.c           force recalculation of clusters
"forcereachability"     "0"                 be_aas_main.c           force recalculation of reachabilities
"forcewrite"                "0"                 be_aas_main.c           force writing of aas file
"nooptimize"                "0"                 be_aas_main.c           no aas optimization

"usehook"                   "0"                 be_ai2_dm.c             enable/disable grapple hook
"laserhook"                 "0"                 be_ai_move.c            0 = CTF hook, 1 = laser hook
"rocketjump"                "1"                 be_ai2_dm.c             enable/disable rocket jumping al together
"techs"                     "0"                 be_ai2_dm.c             enable/disable bot CTF tech ussage
"teamplay"                  "0"                 be_ai2_dm.c             enable/disable tourney teamplay
"teamplay_shell"            "0"                 be_ai2_dm.c             enable/disable teamplay based on shell color
"assimilation"              "0"                 be_ai2_dm.c             enable/disable assimilation
"dmflags"                   "0"                 be_ai2_dm.c             deathmatch flags
"ctf"                           "0"                 be_ai2_dm.c             enable/disable CTF
"ch"                            "0"                 be_ai2_dm.c             enable/disable Colored Hitman
"ra"                            "0"                 be_ai2_dm.c             enable/disable Rocket Arena 2
"nochat"                        "0"                 be_ai2_dm.c             enable/disable bot chatting
"altnames"                  "0"                 be_ai2_dm.c             have the bots use their alternative names
"fastchat"                  "0"                 be_ai2_dm.c             fast chatting for debugging

*/

// ---------------------------------------------------------------------------
// The layout half of the contract.
//
// Every struct above is passed by pointer or by value across a library
// boundary, so both sides must agree on its size and on every offset inside it
// -- and nothing in the 1999 design checks that.  `BotVersion` returns the
// brain's version, not the convention's; `Test()` passes no struct by value.
// Two mismatches were live before these asserts existed, both of them a type
// NAME that means something different on the two sides:
//
//   bsp_trace_t         80 here against 84 there, because osp-tourney's port
//                       swept `qboolean` to `bool` and C99's bool is one byte.
//                       Every field from `fraction` down was four bytes out,
//                       so the brain read our endpos[0] as the trace fraction.
//   bot_updateclient_t  1292 here against 1228 there, because the array bound
//                       was spelled MAX_STATS and Q2PRO's is 64 where the
//                       brain's q_shared.h has 32.  The brain's memcpy is
//                       sized from ITS type, so the inventory it read was our
//                       stats[32..63] followed by our inventory shifted down
//                       sixteen slots.
//
// The numbers are the brain's, measured against
// gladiator-bot-restored/botlib/struct_sizes_asserts.h and its game/botlib.h.
// They hold at 32 and 64 bits alike: not one of these structs contains a
// pointer, which is what makes an assert on the size meaningful rather than
// word-size noise.  `tools/botabi.py` compares these lines against the brain's
// own, so a change on EITHER side fails `make check`.
_Static_assert(sizeof(bsp_surface_t)        == 24,   "bsp_surface_t");
_Static_assert(sizeof(bsp_trace_t)          == 84,   "bsp_trace_t");
// ...and two OFFSETS, mirroring the brain's own (57ce85a3).  A size cannot see a
// transposition, which is the one thing `tools/botabi.py` says it cannot check;
// `fraction` at 8 and `endpos` at 12 are what the 4-byte qboolean buys, and they
// are where the wrongness showed when it was 80 bytes -- `fraction` landed on
// `endpos[0]`.  `q_offsetof` rather than <stddef.h>'s: it is this tree's own
// spelling and needs no include in a header every bot TU pulls in.
_Static_assert(q_offsetof(bsp_trace_t, fraction) == 8,  "bsp_trace_t.fraction");
_Static_assert(q_offsetof(bsp_trace_t, endpos)   == 12, "bsp_trace_t.endpos");
// The two host-side definitions the brain now hard-asserts against a port, so
// that a divergence fails here too and not only over there.
_Static_assert(sizeof(qboolean)             == 4,    "qboolean must stay 4 bytes");
_Static_assert(BOTLIB_MAX_STATS             == 32,   "the contract's stat count");
_Static_assert(sizeof(bot_settings_t)       == 432,  "bot_settings_t");
_Static_assert(sizeof(bot_clientsettings_t) == 144,  "bot_clientsettings_t");
_Static_assert(sizeof(bot_input_t)          == 36,   "bot_input_t");
_Static_assert(sizeof(bot_updateclient_t)   == 1228, "bot_updateclient_t");
_Static_assert(sizeof(bot_updateentity_t)   == 104,  "bot_updateentity_t");
