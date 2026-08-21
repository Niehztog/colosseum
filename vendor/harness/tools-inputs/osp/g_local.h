// g_local.h -- local definitions for game module

#include "q_shared.h"

// define GAME_INCLUDE so that game.h does not define the
// short, server-visible gclient_t and edict_t structures,
// because we define the full size ones in this file
//-------------------------------------------------------------
// Gladiator Bot SDK feature switches.
#define BOT							// Gladiator bot support
#define BOT_IMPORT					// game import redirection (bl_redirgi.c)
//#define BOT_DEBUG					// debug lines / bounding boxes
#define TOURNEY						// this mod's own paths in the SDK

#ifdef BOT
#if defined(WIN32) || defined(_WIN32)
#include <windows.h>
#else
//LINUX?
#endif
#endif
//-------------------------------------------------------------

#define	GAME_INCLUDE
#include "game.h"

// The Gladiator Bot SDK's library interface.  No <errno.h> here: bl_main.c
// declares a local spelled `errno`, which needs glibc's macro out of scope.
#include <stdio.h>
#include <time.h>
#include <sys/timeb.h>
#include <netdb.h>
#include "global.h"
#include "md5.h"
#include "botlib.h"
#include "bl_debug.h"

// the "gameversion" client command will print this plus compile date
#define	GAMEVERSION	"OSP Tourney DM v(2.75)"

// protocol bytes that can be directly added to messages
#define	svc_muzzleflash		1
#define	svc_muzzleflash2	2
#define	svc_temp_entity		3
#define	svc_layout			4
#define	svc_inventory		5
#define	svc_disconnect		7
#define	svc_configstring	13
#define	svc_stufftext		11

//==================================================================

// view pitching times
#define DAMAGE_TIME		0.5
#define	FALL_TIME		0.3


// edict->spawnflags
// these are set with checkboxes on each entity in the map editor
#define	SPAWNFLAG_NOT_EASY			0x00000100
#define	SPAWNFLAG_NOT_MEDIUM		0x00000200
#define	SPAWNFLAG_NOT_HARD			0x00000400
#define	SPAWNFLAG_NOT_DEATHMATCH	0x00000800
#define	SPAWNFLAG_NOT_COOP			0x00001000

// edict->flags
#define	FL_FLY					0x00000001
#define	FL_SWIM					0x00000002	// implied immunity to drowining
#define FL_IMMUNE_LASER			0x00000004
#define	FL_INWATER				0x00000008
#define	FL_GODMODE				0x00000010
#define	FL_NOTARGET				0x00000020
#define FL_IMMUNE_SLIME			0x00000040
#define FL_IMMUNE_LAVA			0x00000080
#define	FL_PARTIALGROUND		0x00000100	// not all corners are valid
#define	FL_WATERJUMP			0x00000200	// player jumping out of water
#define	FL_TEAMSLAVE			0x00000400	// not the first on the team
#define FL_NO_KNOCKBACK			0x00000800
#define FL_POWER_ARMOR			0x00001000	// power armor (if any) is active
// Gladiator Bot SDK flags.
#define FL_BOT					0x00002000	// set when entity is a bot
#define FL_BOTINPUT				0x00004000	// set when executing bot input
#define FL_OLDORGNOTSET			0x00008000	// set when oldorigin may not be set
// The mod's own "this client is a bot" flag.  <invented name>.
#define FL_BOTCLIENT			0x00010000
#define FL_RESPAWN				0x80000000	// used for item respawning


#define	FRAMETIME		0.1

// memory tags to allow dynamic memory to be cleaned up
#define	TAG_GAME	765		// clear when unloading the dll
#define	TAG_LEVEL	766		// clear when loading a new level


// Two popup-menu layout slots.  <invented names>.
#define STAT_OSP_LAYOUT1		27
#define STAT_OSP_LAYOUT2		28

// The values of resp.entered.
#define ENTERED_ENTERED			1

#define STAT_RUNE_RESIST		22
#define STAT_RUNE_STRENGTH		23
#define STAT_RUNE_HASTE			24
#define STAT_RUNE_REGEN			25
#define STAT_RUNE_VAMPIRE		26

// The bits of `rune_stat`, the cached `runes_enable` value.  <invented names>.
#define RUNE_RESIST				1
#define RUNE_STRENGTH			2
#define RUNE_HASTE				4
#define RUNE_REGEN				8
#define RUNE_VAMPIRE			16

#define MELEE_DISTANCE	80

#define BODY_QUEUE_SIZE		8

typedef enum
{
	DAMAGE_NO,
	DAMAGE_YES,			// will take damage if hit
	DAMAGE_AIM			// auto targeting recognizes this
} damage_t;

typedef enum 
{
	WEAPON_READY, 
	WEAPON_ACTIVATING,
	WEAPON_DROPPING,
	WEAPON_FIRING
} weaponstate_t;

typedef enum
{
	AMMO_BULLETS,
	AMMO_SHELLS,
	AMMO_ROCKETS,
	AMMO_GRENADES,
	AMMO_CELLS,
	AMMO_SLUGS
} ammo_t;


//deadflag
#define DEAD_NO					0
#define DEAD_DYING				1
#define DEAD_DEAD				2
#define DEAD_RESPAWNABLE		3

//range
#define RANGE_MELEE				0
#define RANGE_NEAR				1
#define RANGE_MID				2
#define RANGE_FAR				3

//gib types
#define GIB_ORGANIC				0
#define GIB_METALLIC			1

//monster ai flags
#define AI_STAND_GROUND			0x00000001
#define AI_TEMP_STAND_GROUND	0x00000002
#define AI_SOUND_TARGET			0x00000004
#define AI_LOST_SIGHT			0x00000008
#define AI_PURSUIT_LAST_SEEN	0x00000010
#define AI_PURSUE_NEXT			0x00000020
#define AI_PURSUE_TEMP			0x00000040
#define AI_HOLD_FRAME			0x00000080
#define AI_GOOD_GUY				0x00000100
#define AI_BRUTAL				0x00000200
#define AI_NOSTEP				0x00000400
#define AI_DUCKED				0x00000800
#define AI_COMBAT_POINT			0x00001000
#define AI_MEDIC				0x00002000
#define AI_RESURRECTING			0x00004000

//monster attack state
#define AS_STRAIGHT				1
#define AS_SLIDING				2
#define	AS_MELEE				3
#define	AS_MISSILE				4

// armor types
#define ARMOR_NONE				0
#define ARMOR_JACKET			1
#define ARMOR_COMBAT			2
#define ARMOR_BODY				3
#define ARMOR_SHARD				4

// power armor types
#define POWER_ARMOR_NONE		0
#define POWER_ARMOR_SCREEN		1
#define POWER_ARMOR_SHIELD		2

// handedness values
#define RIGHT_HANDED			0
#define LEFT_HANDED				1
#define CENTER_HANDED			2


// game.serverflags values
#define SFL_CROSS_TRIGGER_1		0x00000001
#define SFL_CROSS_TRIGGER_2		0x00000002
#define SFL_CROSS_TRIGGER_3		0x00000004
#define SFL_CROSS_TRIGGER_4		0x00000008
#define SFL_CROSS_TRIGGER_5		0x00000010
#define SFL_CROSS_TRIGGER_6		0x00000020
#define SFL_CROSS_TRIGGER_7		0x00000040
#define SFL_CROSS_TRIGGER_8		0x00000080
#define SFL_CROSS_TRIGGER_MASK	0x000000ff


// noise types for PlayerNoise
#define PNOISE_SELF				0
#define PNOISE_WEAPON			1
#define PNOISE_IMPACT			2


// edict->movetype values
typedef enum
{
MOVETYPE_NONE,			// never moves
MOVETYPE_NOCLIP,		// origin and angles change with no interaction
MOVETYPE_PUSH,			// no clip to world, push on box contact
MOVETYPE_STOP,			// no clip to world, stops on box contact

MOVETYPE_WALK,			// gravity
MOVETYPE_STEP,			// gravity, special edge handling
MOVETYPE_FLY,
MOVETYPE_TOSS,			// gravity
MOVETYPE_FLYMISSILE,	// extra size to monsters
MOVETYPE_BOUNCE
} movetype_t;



typedef struct
{
	int		base_count;
	int		max_count;
	float	normal_protection;
	float	energy_protection;
	int		armor;
} gitem_armor_t;


// gitem_t->flags
#define	IT_WEAPON		1		// use makes active weapon
#define	IT_AMMO			2
#define IT_ARMOR		4
#define IT_STAY_COOP	8
#define IT_KEY			16
#define IT_POWERUP		32
// The rune item class -- id CTF's own IT_TECH value.
#define IT_RUNE			64

// gitem_t->weapmodel for weapons indicates model index
#define WEAP_BLASTER			1 
#define WEAP_SHOTGUN			2 
#define WEAP_SUPERSHOTGUN		3 
#define WEAP_MACHINEGUN			4 
#define WEAP_CHAINGUN			5 
#define WEAP_GRENADES			6 
#define WEAP_GRENADELAUNCHER	7 
#define WEAP_ROCKETLAUNCHER		8 
#define WEAP_HYPERBLASTER		9 
#define WEAP_RAILGUN			10
#define WEAP_BFG				11

typedef struct gitem_s
{
	char		*classname;	// spawning name
	qboolean	(*pickup)(struct edict_s *ent, struct edict_s *other);
	void		(*use)(struct edict_s *ent, struct gitem_s *item);
	void		(*drop)(struct edict_s *ent, struct gitem_s *item);
	void		(*weaponthink)(struct edict_s *ent);
	char		*pickup_sound;
	char		*world_model;
	int			world_model_flags;
	char		*view_model;

	// client side info
	char		*icon;
	char		*pickup_name;	// for printing on pickup
	int			count_width;		// number of digits to display by icon

	int			quantity;		// for ammo how much, for weapons how much is used per shot
	char		*ammo;			// for weapons
	int			flags;			// IT_* flags

	int			weapmodel;		// weapon model index (for weapons)

	void		*info;
	int			tag;

	char		*precaches;		// string of all models, sounds, and images this item will use
} gitem_t;



//
// this structure is left intact through an entire game
// it should be initialized at dll load time, and read/written to
// the server.ssv file for savegames
//
typedef struct
{
	char		helpmessage1[512];
	char		helpmessage2[512];
	int			helpchanged;	// flash F1 icon if non 0, play sound
								// and increment only if 1, 2, or 3

	gclient_t	*clients;		// [maxclients]

	// can't store spawnpoint in level, because
	// it would get overwritten by the savegame restore
	char		spawnpoint[512];	// needed for coop respawns

	// store latched cvars here that we want to get at often
	int			maxclients;
	int			maxentities;

	// cross level triggers
	int			serverflags;

	// items
	int			num_items;

	qboolean	autosaved;
} game_locals_t;


//
// this structure is cleared as each map is entered
// it is read/written to the level.sav file for savegames
//
typedef struct
{
	int			framenum;
	float		time;

	char		level_name[MAX_QPATH];	// the descriptive name (Outer Base, etc)
	char		mapname[MAX_QPATH];		// the server name (base1, etc)
	char		nextmap[MAX_QPATH];		// go here when fraglimit is hit

	// intermission state
	float		intermissiontime;		// time the intermission was started
	char		*changemap;
	int			exitintermission;
	vec3_t		intermission_origin;
	vec3_t		intermission_angle;

	edict_t		*sight_client;	// changed once each frame for coop games

	edict_t		*sight_entity;
	int			sight_entity_framenum;
	edict_t		*sound_entity;
	int			sound_entity_framenum;
	edict_t		*sound2_entity;
	int			sound2_entity_framenum;

	int			pic_health;

	int			total_secrets;
	int			found_secrets;

	int			total_goals;
	int			found_goals;

	int			total_monsters;
	int			killed_monsters;

	edict_t		*current_entity;	// entity running from G_RunFrame
	int			body_que;			// dead bodies

	int			power_cubes;		// ugly necessity for coop
} level_locals_t;


// spawn_temp_t is only used to hold entity field values that
// can be set from the editor, but aren't actualy present
// in edict_t during gameplay
typedef struct
{
	// world vars
	char		*sky;
	float		skyrotate;
	vec3_t		skyaxis;
	char		*nextmap;

	int			lip;
	int			distance;
	int			height;
	char		*noise;
	float		pausetime;
	char		*item;
	char		*gravity;

	float		minyaw;
	float		maxyaw;
	float		minpitch;
	float		maxpitch;

	// OSP: five more, appended.  The last four are the Gladiator Bot SDK's;
	// `botlib` is this mod's own.
	char		*botlib;
	char		*name;
	char		*skin;
	char		*charfile;
	char		*charname;
} spawn_temp_t;


typedef struct
{
	// fixed data
	vec3_t		start_origin;
	vec3_t		start_angles;
	vec3_t		end_origin;
	vec3_t		end_angles;

	int			sound_start;
	int			sound_middle;
	int			sound_end;

	float		accel;
	float		speed;
	float		decel;
	float		distance;

	float		wait;

	// state data
	int			state;
	vec3_t		dir;
	float		current_speed;
	float		move_speed;
	float		next_speed;
	float		remaining_distance;
	float		decel_distance;
	void		(*endfunc)(edict_t *);
} moveinfo_t;


typedef struct
{
	void	(*aifunc)(edict_t *self, float dist);
	float	dist;
	void	(*thinkfunc)(edict_t *self);
} mframe_t;

typedef struct
{
	int			firstframe;
	int			lastframe;
	mframe_t	*frame;
	void		(*endfunc)(edict_t *self);
} mmove_t;

typedef struct
{
	mmove_t		*currentmove;
	int			aiflags;
	int			nextframe;
	float		scale;

	void		(*stand)(edict_t *self);
	void		(*idle)(edict_t *self);
	void		(*search)(edict_t *self);
	void		(*walk)(edict_t *self);
	void		(*run)(edict_t *self);
	void		(*dodge)(edict_t *self, edict_t *other, float eta);
	void		(*attack)(edict_t *self);
	void		(*melee)(edict_t *self);
	void		(*sight)(edict_t *self, edict_t *other);
	qboolean	(*checkattack)(edict_t *self);

	float		pausetime;
	float		attack_finished;

	vec3_t		saved_goal;
	float		search_time;
	float		trail_time;
	vec3_t		last_sighting;
	int			attack_state;
	int			lefty;
	float		idle_time;
	int			linkcount;

	int			power_armor_type;
	int			power_armor_power;
} monsterinfo_t;



extern	int		paused;

// OSP rune subsystem.
qboolean OSP_Pickup_Rune (edict_t *ent, edict_t *other);
void	 OSP_Drop_Rune (edict_t *ent, gitem_t *item);

// The mod's own subsystems.
void	 q2log_useItem (char *name, edict_t *ent);
void	 q2log_pickupItem (char *name, int arg, edict_t *ent);
qboolean OSP_runesHasHaste (edict_t *ent);
void	 OSP_respawnRune (edict_t *ent);
void	 Use_Quad (edict_t *ent, gitem_t *item);
void	 Use_Invulnerability (edict_t *ent, gitem_t *item);
void	 OSP_packPlayer (edict_t *ent);
void	 OSP_seedPlayer (gclient_t *client);
qboolean OSP_disableItems (edict_t *ent);
// The five `sv <name>` handlers ServerCommand dispatches to.
void	 OSP_allready_svcmd (void);
void	 OSP_allnotready_svcmd (edict_t *ent);
void	 OSP_rmpause_cmd (void);
void	 OSP_rstopmatch_cmd (edict_t *ent);
void	 OSP_playerlist_svcmd (void);
qboolean BotCmd (char *cmd, edict_t *ent, int server);
edict_t *SelectDeathmatchSpawnPoint (edict_t *ent);
void	 PlayerDied (edict_t *ent);
void	 PlayerResetGrapple (edict_t *ent);
void	 ResetGrapple (edict_t *self);
void	 GrappleTouch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf);
void	 GrapplePull (edict_t *self);
void	 FireGrapple (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int effect);
void	 GrappleFire (edict_t *ent, vec3_t g_offset, int damage, int effect);
void	 Grapple_Fire (edict_t *ent);
void	 OSP_hookAliases (edict_t *ent);
void	 G_Spawn_Sparks (int type, vec3_t pos, vec3_t dir, vec3_t org);
void	 P_ProjectSource (gclient_t *client, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result);

// id CTF's grapple tuning, from its g_ctf.h.
#define CTF_GRAPPLE_SPEED				650		// speed of grapple in flight
#define CTF_GRAPPLE_PULL_SPEED			650		// speed player is pulled at

// How many of each rune are loose in the world, indexed by
// `item->quantity - STAT_RUNE_RESIST`.
extern	int		r_count[5];

// The rune spawn pool.
extern	int		rune_spawncount;
extern	edict_t	*rune_spawnpoint[50];
// The once-per-map latch OSP_setupRuneSpawn tests and sets.
extern	int		runespawn;
extern	cvar_t	*runes_model;
extern	cvar_t	*runes_flash;
extern	cvar_t	*runes_min;
extern	cvar_t	*runes_max;
extern	cvar_t	*runes_perplayer;
extern	cvar_t	*runes_resist;
extern	cvar_t	*runes_strength;
extern	cvar_t	*runes_regen_hmax;
extern	cvar_t	*runes_regen_amax;
extern	cvar_t	*runes_vampire;
extern	cvar_t	*runes_vampire_max;

gitem_t	*OSP_What_Rune (edict_t *ent);
void	 OSP_runeThink (edict_t *self);
void	 OSP_setupRuneSpawn (int delay);
int		 OSP_runesApplyResistance (edict_t *ent, int damage);
int		 OSP_runesApplyStrength (edict_t *ent, int damage);
qboolean OSP_runesApplyStrengthSound (edict_t *ent);
void	 OSP_runesApplyHasteSound (edict_t *ent);
void	 OSP_runesApplyRegeneration (edict_t *ent);
qboolean OSP_runesHasRegeneration (edict_t *ent);
qboolean OSP_runesHasVampire (edict_t *ent);
void	 OSP_runesApplyVampire (edict_t *ent, int damage);
void	 OSP_zeroRuneStats (edict_t *ent);
void	 OSP_removeRunes (void);
int		 OSP_findMinRune (void);
void	 OSP_checkMinRunes (void);
qboolean OSP_checkMaxRunes (void);

// The cached `runes_enable` bitmask; see the RUNE_* bits above.
extern	int		rune_stat;

// The per-player accuracy/damage table behind the ngLog accuracy report,
// indexed by client->resp.clientid.  Member names <invented>.  Every update
// site is gated by `sync_stat > 2`.
#define ACC_BLASTER			0
#define ACC_SHOTGUN			1
#define ACC_SSHOTGUN		2
#define ACC_MACHINEGUN		3
#define ACC_CHAINGUN		4
#define ACC_GRENADELAUNCHER	5
#define ACC_ROCKET			6
#define ACC_HYPERBLASTER	7
#define ACC_RAILGUN			8
#define ACC_BFG				9
#define ACC_GRENADE			10

typedef struct
{
	char	netname[16];		// ClientUserinfoChanged writes [15] = 0
	char	osp_a010[32];		// invented name; copied from ent->osp_e37c
	int		dgiven;
	int		dtaken;
	int		shots[11];
	int		hits[11];
	int		given[11];
	int		taken[11];
} p_acc_t;

// ---------------------------------------------------------------------------
// The mod's logging stack. Three layers, each its own TU:
//   nglog.c   -- the ngLog/ngStats file writer (`ngLog_*`, `__nglog_*`)
//   q2log.c   -- the game-event layer that formats lines and hands them down
//   stdlog.c  -- the "Standard Log" 1.2 format, a second consumer of nglog
// ---------------------------------------------------------------------------
extern	int		ngloglog_status;
extern	int		sl_ngloglog_status;
extern	cvar_t	*sl_log_logbots;
extern	cvar_t	*sl_log_style;
extern	cvar_t	*sl_filename;
extern	cvar_t	*sl_log_flush;
extern	cvar_t	*sl_log_method;
extern	cvar_t	*nglog_logstyle;
extern	cvar_t	*nglog_logstyle_working;
extern	cvar_t	*nglog_logname;
extern	cvar_t	*nglog_flush;
extern	cvar_t	*nglog_buffer;
extern	cvar_t	*nglog_logchat;
extern	cvar_t	*nglog_logmiscpickup;
extern	cvar_t	*nglog_worldstats;
extern	cvar_t	*nglog_ngstats_browser;
extern	cvar_t	*nglog_ngstats_exec;
extern	cvar_t	*nglog_ngstats_cfg;
extern	cvar_t	*nglog_ngstats_logdir;
extern	cvar_t	*nglog_ngstats_vidrestart;
extern	cvar_t	*ngWorldStats_Status;
extern	int		__nglog_num_errs;
extern	char	__nglog_error_msg[8][4096];
extern	int		__nglog_logstyle;
extern	int		__nglog_worldlog;
extern	int		__nglog_flush;
extern	int		__nglog_buffer;
extern	int		__nglog_ngstats_exec;
extern	char	__nglog_logpath[1024];
extern	char	__nglog_logname[1024];
extern	char	__nglog_log_prefix[1024];
extern	char	__nglog_worldlog_path[1024];
extern	char	__nglog_worldlog_name[1024];
extern	char	__nglog_worldlog_prefix[1024];
extern	char	__nglog_worldlog_tag[64];
extern	char	__nglog_rel_path[1024];
extern	char	__nglog_ngstats_logdir[1024];
extern	char	__nglog_ngstats_cfg[1024];
extern	FILE	*log_file;
extern	FILE	*worldlog_file;
extern	int		buffer_lines;
extern	int		wbuffer_lines;
extern	MD5_CTX	context;

void	ngLog_logFlush (FILE *f);
void	ngLog_ngStatsCall (int arg);
int		ngLog_fileExists (char *name);
void	ngLog_errorMsgClear (void);
void	ngLog_rotateFile (void);
char	*ngLog_hostAddr (void);
void	ngLog_inputLine (char *line);
char	*ngLog_playerIdentifier (char *a, char *b);
void	ngLog_transMark (char *out, int *count);

int		ngLog_init (void);
void	ngLog_logWrite (char *line, int which);
void	ngLog_logClose (int which, int reason);
void	ngLog_getDateInfo (char *out, int fmt);
void	ngLog_giveMark (char *out);
void	ngLog_initMark (void);

void	q2log_logWrite (char *line);
void	q2log_showErrors (void);
int		q2log_init (void);
void	q2log_gameInit (int restart);
void	q2log_listDissedItems (char *line);
void	q2log_logStart (void);
void	q2log_logStartHeader (void);
void	q2log_customStart (void);
void	q2log_logTime (void);
void	q2log_gameStart (void);
void	q2log_gameEnd (char *reason, int closereason);
void	q2log_playerConnect (edict_t *ent);
void	q2log_playerReconnect (edict_t *ent);
void	q2log_playerEntered (edict_t *ent);
void	q2log_playerDisconnect (edict_t *ent);
void	q2log_playerRename (edict_t *ent, char *oldname);
void	q2log_playerRespawn (edict_t *ent);
void	q2log_playerMode (edict_t *ent, char *mode);
void	q2log_playerChat (char *text);
void	q2log_playerZBOT (edict_t *ent, char *extra);
void	q2log_teamName (char *name);
void	q2log_teamRename (char *oldname, char *newname);
void	q2log_teamJoin (edict_t *ent);
void	q2log_teamLeave (edict_t *ent);
void	q2log_voteInfo (char *what, char *a, char *b);
void	q2log_expireItem (char *name, edict_t *ent, int arg);
void	q2log_dropItem (char *name, int arg, edict_t *ent);
void	q2log_logAccuracy (void);
void	q2log_logAccuracyStats (edict_t *ent);
void	q2log_clientid_cmd (edict_t *ent);
int		sl_Logging (game_import_t *import, char *patch);
int		sl_OpenLogFile (game_import_t *import);
void	sl_GameStart (game_import_t *import, level_locals_t level);
void	sl_GameEnd (game_import_t *import, level_locals_t level);
void	sl_WriteStdLogDeath (game_import_t *import, level_locals_t level,
	edict_t *targ, edict_t *inflictor, edict_t *attacker);
void	sl_WriteStdLogPlayerEntered (game_import_t *import, level_locals_t level,
	edict_t *ent);
void	sl_LogPlayerDisconnect (game_import_t *import, level_locals_t level,
	edict_t *ent);
void	sl_SoftGameEnd (game_import_t *import, level_locals_t level);
void	sl_LogMapName (game_import_t *import, char *mapname);
void	sl_LogGameStart (game_import_t *import, float time);
void	sl_LogVers (game_import_t *import);
void	sl_LogPatch (game_import_t *import, char *patch);
void	sl_LogDate (game_import_t *import);
void	sl_LogTime (game_import_t *import);
void	sl_LogDeathFlags (game_import_t *import, unsigned long flags);
void	sl_LogGameEnd (game_import_t *import, float time);
void	sl_CloseLogFile (void);
void	sl_LogPlayerConnect (game_import_t *import, char *name, int unused,
	float time);
void	sl_LogPlayerLeft (game_import_t *import, char *name, float time);
void	sl_LogPlayerRename (game_import_t *import, char *oldname, char *newname,
	float time);
void	sl_LogScore (game_import_t *import, char *killer, char *victim,
	char *event, char *weapon, int score, float time, int ping);
void	q2log_stdlog_logWrite (char *line);
void	q2log_stdlog_showErrors (void);

// The two teams.  Each carries its name twice: plain, and with 0x80 added to
// every byte, which is how Quake II's charset renders it green.
typedef struct
{
	char	netname[32];
	char	greenname[32];		// <INVENTED NAME>
	char	skin[128];
	byte	osp_m0c0[32];
	char	joincode[16];		// <INVENTED NAME>
	int		osp_m0f0;
	int		osp_m0f4;			// non-zero = team is locked
	int		osp_m0f8;			// the team's frag total
	int		osp_m0fc;
	int		osp_m100;
	int		osp_m104;
	int		osp_m108;
	int		osp_m10c;
	int		osp_m110;			// last frag total pushed to the clients
	int		osp_m114;
	int		osp_m118;			// the fraglimit that went with it
	int		osp_m11c;
	int		osp_m120;
	int		osp_m124;
} team_t;

extern	team_t	teams[2];


extern	int		sync_stat;
extern	int		active_clients;
extern	int		start_count;
extern	p_acc_t	p_acc[256];

extern	game_locals_t	game;
extern	level_locals_t	level;
extern	game_import_t	gi;
extern	game_export_t	globals;
extern	spawn_temp_t	st;

extern	int	sm_meat_index;
extern	int	snd_fry;

extern	int	jacket_armor_index;
extern	int	combat_armor_index;
extern	int	body_armor_index;


// means of death
#define MOD_UNKNOWN			0
#define MOD_BLASTER			1
#define MOD_SHOTGUN			2
#define MOD_SSHOTGUN		3
#define MOD_MACHINEGUN		4
#define MOD_CHAINGUN		5
#define MOD_GRENADE			6
#define MOD_G_SPLASH		7
#define MOD_ROCKET			8
#define MOD_R_SPLASH		9
#define MOD_HYPERBLASTER	10
#define MOD_RAILGUN			11
#define MOD_BFG_LASER		12
#define MOD_BFG_BLAST		13
#define MOD_BFG_EFFECT		14
#define MOD_HANDGRENADE		15
#define MOD_HG_SPLASH		16
#define MOD_WATER			17
#define MOD_SLIME			18
#define MOD_LAVA			19
#define MOD_CRUSH			20
#define MOD_TELEFRAG		21
#define MOD_FALLING			22
#define MOD_SUICIDE			23
#define MOD_HELD_GRENADE	24
#define MOD_EXPLOSIVE		25
#define MOD_BARREL			26
#define MOD_BOMB			27
#define MOD_EXIT			28
#define MOD_SPLASH			29
#define MOD_TARGET_LASER	30
#define MOD_TRIGGER_HURT	31
#define MOD_HIT				32
#define MOD_TARGET_BLASTER	33
// Not id CTF's 34.
#define MOD_GRAPPLE			42
#define MOD_FRIENDLY_FIRE	0x8000000

extern	int	meansOfDeath;


extern	edict_t			*g_edicts;

#define	FOFS(x) (int)&(((edict_t *)0)->x)
#define	STOFS(x) (int)&(((spawn_temp_t *)0)->x)
#define	LLOFS(x) (int)&(((level_locals_t *)0)->x)
#define	CLOFS(x) (int)&(((gclient_t *)0)->x)

#define random()	((rand () & 0x7fff) / ((float)0x7fff))
#define crandom()	(2.0 * (random() - 0.5))

extern	cvar_t	*maxentities;
extern	cvar_t	*deathmatch;
extern	cvar_t	*coop;
extern	cvar_t	*dmflags;
extern	cvar_t	*skill;
extern	cvar_t	*fraglimit;
extern	cvar_t	*timelimit;
extern	cvar_t	*password;
extern	cvar_t	*g_select_empty;
extern	cvar_t	*dedicated;

extern	cvar_t	*filterban;

extern	cvar_t	*sv_gravity;
extern	cvar_t	*sv_maxvelocity;

extern	cvar_t	*gun_x, *gun_y, *gun_z;
extern	cvar_t	*sv_rollspeed;
extern	cvar_t	*sv_rollangle;

extern	cvar_t	*run_pitch;
extern	cvar_t	*run_roll;
extern	cvar_t	*bob_up;
extern	cvar_t	*bob_pitch;
extern	cvar_t	*bob_roll;

extern	cvar_t	*sv_cheats;
extern	cvar_t	*maxclients;
// The mod's own cvars, declared as the vanilla-derived files come to need them.
extern	cvar_t	*camera_depth;
extern	cvar_t	*client_hud;
extern	cvar_t	*damage_railgun;
extern	cvar_t	*match_type;
extern	cvar_t	*hook_enable;
extern	cvar_t	*client_protect;
extern	cvar_t	*team_hurtteam;
extern	cvar_t	*team_hurtself;
extern	cvar_t	*fast_minpbound;
extern	cvar_t	*fast_maxpbound;
extern	cvar_t	*fast_respawn;
extern	cvar_t	*nglog_logallpickups;
extern	cvar_t	*hook_initdamage;
extern	cvar_t	*numgibs;

extern	cvar_t	*flood_msgs;
extern	cvar_t	*flood_persecond;
extern	cvar_t	*flood_waitdelay;

#define world	(&g_edicts[0])

// item spawnflags
#define ITEM_TRIGGER_SPAWN		0x00000001
#define ITEM_NO_TOUCH			0x00000002
// 6 bits reserved for editor flags
// 8 bits used as power cube id bits for coop games
#define DROPPED_ITEM			0x00010000
#define	DROPPED_PLAYER_ITEM		0x00020000
#define ITEM_TARGETS_USED		0x00040000

//
// fields are needed for spawning from the entity string
// and saving / loading games
//
#define FFL_SPAWNTEMP		1
#define FFL_NOSPAWN			2

typedef enum {
	F_INT, 
	F_FLOAT,
	F_LSTRING,			// string on disk, pointer in memory, TAG_LEVEL
	F_GSTRING,			// string on disk, pointer in memory, TAG_GAME
	F_VECTOR,
	F_ANGLEHACK,
	F_EDICT,			// index on disk, pointer in memory
	F_ITEM,				// index on disk, pointer in memory
	F_CLIENT,			// index on disk, pointer in memory
	F_FUNCTION,
	F_MMOVE,
	F_IGNORE
} fieldtype_t;

typedef struct
{
	char	*name;
	int		ofs;
	fieldtype_t	type;
	int		flags;
} field_t;


extern	field_t fields[];
extern	gitem_t	itemlist[];


//
// g_cmds.c
//
void Cmd_Help_f (edict_t *ent);
void Cmd_Score_f (edict_t *ent);

//
// g_items.c
//
void PrecacheItem (gitem_t *it);
void InitItems (void);
void SetItemNames (void);
gitem_t	*FindItem (char *pickup_name);
gitem_t	*FindItemByClassname (char *classname);
#define	ITEM_INDEX(x) ((x)-itemlist)
edict_t *Drop_Item (edict_t *ent, gitem_t *item);
void SetRespawn (edict_t *ent, float delay);
void ChangeWeapon (edict_t *ent);
void SpawnItem (edict_t *ent, gitem_t *item);
void Think_Weapon (edict_t *ent);
int ArmorIndex (edict_t *ent);
int PowerArmorType (edict_t *ent);
gitem_t	*GetItemByIndex (int index);
qboolean Add_Ammo (edict_t *ent, gitem_t *item, int count);
void Touch_Item (edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf);

//
// g_utils.c
//
qboolean	KillBox (edict_t *ent);
void	G_ProjectSource (vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result);
edict_t *G_Find (edict_t *from, int fieldofs, char *match);
edict_t *findradius (edict_t *from, vec3_t org, float rad);
edict_t *G_PickTarget (char *targetname);
void	G_UseTargets (edict_t *ent, edict_t *activator);
void	G_SetMovedir (vec3_t angles, vec3_t movedir);

void	G_InitEdict (edict_t *e);
edict_t	*G_Spawn (void);
void	G_FreeEdict (edict_t *e);

void	G_TouchTriggers (edict_t *ent);
void	G_TouchSolids (edict_t *ent);

char	*G_CopyString (char *in);

float	*tv (float x, float y, float z);
char	*vtos (vec3_t v);

float vectoyaw (vec3_t vec);
void vectoangles (vec3_t vec, vec3_t angles);

//
// g_combat.c
//
qboolean OnSameTeam (edict_t *ent1, edict_t *ent2);
qboolean CanDamage (edict_t *targ, edict_t *inflictor);
void T_Damage (edict_t *targ, edict_t *inflictor, edict_t *attacker, vec3_t dir, vec3_t point, vec3_t normal, int damage, int knockback, int dflags, int mod);
void T_RadiusDamage (edict_t *inflictor, edict_t *attacker, float damage, edict_t *ignore, float radius, int mod);

// damage flags
#define DAMAGE_RADIUS			0x00000001	// damage was indirect
#define DAMAGE_NO_ARMOR			0x00000002	// armour does not protect from this damage
#define DAMAGE_ENERGY			0x00000004	// damage is from an energy based weapon
#define DAMAGE_NO_KNOCKBACK		0x00000008	// do not affect velocity, just view angles
#define DAMAGE_BULLET			0x00000010  // damage is from a bullet (used for ricochets)
#define DAMAGE_NO_PROTECTION	0x00000020  // armor, shields, invulnerability, and godmode have no effect

#define DEFAULT_BULLET_HSPREAD	300
#define DEFAULT_BULLET_VSPREAD	500
#define DEFAULT_SHOTGUN_HSPREAD	1000
#define DEFAULT_SHOTGUN_VSPREAD	500
#define DEFAULT_DEATHMATCH_SHOTGUN_COUNT	12
#define DEFAULT_SHOTGUN_COUNT	12
#define DEFAULT_SSHOTGUN_COUNT	20

//
// g_monster.c
//
void monster_fire_bullet (edict_t *self, vec3_t start, vec3_t dir, int damage, int kick, int hspread, int vspread, int flashtype);
void monster_fire_shotgun (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, int flashtype);
void monster_fire_blaster (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int flashtype, int effect);
void monster_fire_grenade (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int flashtype);
void monster_fire_rocket (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int flashtype);
void monster_fire_railgun (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int flashtype);
void monster_fire_bfg (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int kick, float damage_radius, int flashtype);
void M_droptofloor (edict_t *ent);
void monster_think (edict_t *self);
void walkmonster_start (edict_t *self);
void swimmonster_start (edict_t *self);
void flymonster_start (edict_t *self);
void AttackFinished (edict_t *self, float time);
void monster_death_use (edict_t *self);
void M_CatagorizePosition (edict_t *ent);
qboolean M_CheckAttack (edict_t *self);
void M_FlyCheck (edict_t *self);
void M_CheckGround (edict_t *ent);

//
// g_misc.c
//
void ThrowHead (edict_t *self, char *gibname, int damage, int type);
void ThrowClientHead (edict_t *self, int damage);
void ThrowGib (edict_t *self, char *gibname, int damage, int type);
void BecomeExplosion1(edict_t *self);

//
// g_ai.c
//
void AI_SetSightClient (void);

void ai_stand (edict_t *self, float dist);
void ai_move (edict_t *self, float dist);
void ai_walk (edict_t *self, float dist);
void ai_turn (edict_t *self, float dist);
void ai_run (edict_t *self, float dist);
void ai_charge (edict_t *self, float dist);
int range (edict_t *self, edict_t *other);

void FoundTarget (edict_t *self);
qboolean infront (edict_t *self, edict_t *other);
qboolean visible (edict_t *self, edict_t *other);
qboolean FacingIdeal(edict_t *self);

//
// g_weapon.c
//
void ThrowDebris (edict_t *self, char *modelname, float speed, vec3_t origin);
// fire_hit is gone with the monster melee code -- see g_weapon.c.
void fire_bullet (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int mod);
void fire_shotgun (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, int mod);
void fire_blaster (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, int effect, qboolean vhyper);
void fire_grenade (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, float timer, float damage_radius);
void fire_grenade2 (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, float timer, float damage_radius, qboolean held);
void fire_rocket (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius, int radius_damage);
void fire_rail (edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick);
void fire_bfg (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius);

//
// g_ptrail.c
//
void PlayerTrail_Init (void);
void PlayerTrail_Add (vec3_t spot);
void PlayerTrail_New (vec3_t spot);
edict_t *PlayerTrail_PickFirst (edict_t *self);
edict_t *PlayerTrail_PickNext (edict_t *self);
edict_t	*PlayerTrail_LastSpot (void);

//
// g_client.c
//
void respawn (edict_t *ent);
void BeginIntermission (edict_t *targ);
void PutClientInServer (edict_t *ent);
void InitClientPersistant (gclient_t *client, qboolean full);
void InitClientResp (gclient_t *client);
void InitBodyQue (void);
void ClientBeginServerFrame (edict_t *ent);

//
// g_player.c
//
void player_pain (edict_t *self, edict_t *other, float kick, int damage);
void player_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point);

//
// g_svcmds.c
//
void	ServerCommand (void);
qboolean SV_FilterPacket (char *from);

//
// p_view.c
//
void ClientEndServerFrame (edict_t *ent);

//
// p_hud.c
//
void MoveClientToIntermission (edict_t *client);
void G_SetStats (edict_t *ent);
void G_SetSpectatorStats (edict_t *ent);
void G_CheckChaseStats (edict_t *ent);
void ValidateSelectedItem (edict_t *ent);
void DeathmatchScoreboardMessage (edict_t *client, edict_t *killer);

//
// g_pweapon.c
//
void PlayerNoise(edict_t *who, vec3_t where, int type);

//
// m_move.c
//
qboolean M_CheckBottom (edict_t *ent);
qboolean M_walkmove (edict_t *ent, float yaw, float dist);
void M_MoveToGoal (edict_t *ent, float dist);
void M_ChangeYaw (edict_t *ent);

//
// g_phys.c
//
void G_RunEntity (edict_t *ent);

//
// g_main.c
//
void SaveClientData (void);
void FetchClientEntData (edict_t *ent);

//
// g_chase.c
//
void UpdateChaseCam(edict_t *ent);
void ChaseNext(edict_t *ent);
void ChasePrev(edict_t *ent);
void GetChaseTarget(edict_t *ent);

//============================================================================

// client_t->anim_priority
#define	ANIM_BASIC		0		// stand / run
#define	ANIM_WAVE		1
#define	ANIM_JUMP		2
#define	ANIM_PAIN		3
#define	ANIM_ATTACK		4
#define	ANIM_DEATH		5
#define	ANIM_REVERSE	6


// client data that stays across multiple level loads
typedef struct
{
	char		userinfo[MAX_INFO_STRING];

	// OSP: 16 bytes more than vanilla.  The netname is carried twice: plain,
	// and with 0x80 added to every byte (green).  `greenname` is <invented>.
	char		netname[16];
	char		greenname[16];

	int			hand;

	qboolean	connected;			// a loadgame will leave valid entities that
									// just don't have a connection yet

	// values saved and restored from edicts when changing levels
	int			health;
	int			max_health;
	int			savedFlags;

	int			selected_item;
	int			inventory[MAX_ITEMS];

	// ammo capacities
	int			max_bullets;
	int			max_shells;
	int			max_rockets;
	int			max_grenades;
	int			max_cells;
	int			max_slugs;

	gitem_t		*weapon;
	gitem_t		*lastweapon;

	int			power_cubes;	// used for tracking the cubes in coop games
	int			score;			// for calculating total unit score in coop games

	// OSP: `game_helpchanged`/`helpchanged` moved into client_respawn_t.

	// OSP: also a strike counter -- OSP_speedCheat_cmd increments it and
	// OSP_speedDetect tests it against 3.  Vanilla's `qboolean` type is what
	// reconciles the two oracles: gcc gives an all-non-negative enum UNSIGNED
	// int (real's ELF compares with `jbe`) while MSVC always gives it signed
	// int (real's PE compares with `jl`).  Neither `int` nor `unsigned`
	// satisfies both.
	qboolean	spectator;			// client is a spectator
} client_persistant_t;

// OSP: the pop-up menu engine is id's Q2 CTF p_menu.c.  CTF declares these in
// p_menu.h, but p_menu.c includes only g_local.h, so they live here.
enum {
	PMENU_ALIGN_LEFT,
	PMENU_ALIGN_CENTER,
	PMENU_ALIGN_RIGHT
};

typedef struct pmenuhnd_s {
	struct pmenu_s	*entries;
	int				cur;
	int				num;
} pmenuhnd_t;

typedef struct pmenu_s {
	char	*text;
	int		align;
	void	*arg;
	void	(*SelectFunc)(edict_t *ent, struct pmenu_s *entry);
} pmenu_t;

// id CTF's grapple states, renumbered 1/2/4.
typedef enum {
	CTF_GRAPPLE_STATE_FLY	= 1,
	CTF_GRAPPLE_STATE_PULL	= 2,
	CTF_GRAPPLE_STATE_HANG	= 4
} grapple_state_t;

void PMenu_Open(edict_t *ent, pmenu_t *entries, int cur, int num);
void PMenu_Close(edict_t *ent);
void PMenu_Update(edict_t *ent);
void PMenu_Next(edict_t *ent);
void PMenu_Prev(edict_t *ent);
void PMenu_Select(edict_t *ent);

// The mod's pop-up menus.
extern	pmenu_t	Team_Menu[18];
extern	pmenu_t	RegDM_Menu[18];
extern	pmenu_t	AdminMain_Menu[17];
extern	pmenu_t	AdminSelect_Menu[17];
extern	pmenu_t	Vote_Menu[19];
extern	pmenu_t	Vote_Menu2[18];
extern	pmenu_t	Bot_Menu[18];
extern	pmenu_t	Proposal_Menu[18];
extern	pmenu_t	Proposal_Menu2[18];
extern	pmenu_t	Help_Menu[18];
extern	pmenu_t	Help2_Menu[18];
extern	pmenu_t	Help3_Menu[18];
extern	pmenu_t	Invite_Menu[18];

extern	int		m_mode;
extern	int		vote_inprogress;
extern	int		match_paused;
extern	int		who_paused;
extern	float	pause_time;
extern	int		item_settings;
// `server_log` is a FILE *, not a cvar.
extern	FILE	*server_log;
extern	gitem_armor_t	jacketarmor_info;
extern	gitem_armor_t	combatarmor_info;
extern	gitem_armor_t	bodyarmor_info;
extern	cvar_t	*match_strictmode;

qboolean OSP_botDetect (edict_t *ent, usercmd_t *ucmd);
void	 OnBotDetection (edict_t *ent, char *why);
void	 OSP_speedCheat_cmd (edict_t *ent);
void	 OSP_getPlayerAddr (edict_t *ent);
void	 OSP_logAdminLog (char *fmt, ...);
void	 OSP_speedDetect (edict_t *ent);
extern	cvar_t	*vote_threshold;

void	OSP_teamMenu (edict_t *ent);
void	OSP_DMMenu (edict_t *ent);
void	OSP_adminMenu (edict_t *ent);
void	OSP_adminSelectMenu (edict_t *ent, pmenu_t *p);
void	OSP_voteMenu (edict_t *ent, pmenu_t *p);
void	OSP_voteMenu2 (edict_t *ent, pmenu_t *p);
void	OSP_helpMenu (edict_t *ent, pmenu_t *p);
void	OSP_help2Menu (edict_t *ent, pmenu_t *p);
void	OSP_help3Menu (edict_t *ent, pmenu_t *p);
void	OSP_inviteMenu (edict_t *ent);
int		OSP_updateAdminMenu (edict_t *ent);
void	OSP_updateVoteMenu2 (edict_t *ent);
void	OSP_updateProposalMenu (edict_t *ent);
int		OSP_updateInviteMenu (edict_t *ent);
void	OSP_returnMainTeam_menu (edict_t *ent, pmenu_t *p);
void	OSP_returnMainDM_menu (edict_t *ent, pmenu_t *p);
void	OSP_returnMainAdmin_menu (edict_t *ent, pmenu_t *p);
void	OSP_toggleID_menu (edict_t *ent, pmenu_t *p);
void	OSP_changeHUD (edict_t *ent, pmenu_t *p);
void	OSP_ngStatsView (edict_t *ent, pmenu_t *p);
void	OSP_changeObserve (edict_t *ent, pmenu_t *p);
void	OSP_changeChase (edict_t *ent, pmenu_t *p);
void	OSP_botMenu (edict_t *ent, pmenu_t *p);
void	OSP_dmReturn_menu (edict_t *ent, pmenu_t *p);
void	OSP_changeMap_menu (edict_t *ent, pmenu_t *p);
void	OSP_changeConfig_menu (edict_t *ent, pmenu_t *p);
void	OSP_changeTime_menu (edict_t *ent, pmenu_t *p);
void	OSP_changeFrag_menu (edict_t *ent, pmenu_t *p);
void	OSP_changeHook_menu (edict_t *ent, pmenu_t *p);
void	OSP_changeRunes_menu (edict_t *ent, pmenu_t *p);
void	OSP_changeKick_menu (edict_t *ent, pmenu_t *p);
void	OSP_changeItems_menu (edict_t *ent, pmenu_t *p);
void	OSP_addSpecificBot_menu (edict_t *ent, pmenu_t *p);
void	OSP_addBots_menu (edict_t *ent, pmenu_t *p);
void	OSP_removeBots_menu (edict_t *ent, pmenu_t *p);
void	OSP_proposeVote_menu (edict_t *ent, pmenu_t *p);
void	OSP_joinTeam_menu (edict_t *ent, pmenu_t *p);
void	OSP_inviteClose_menu (edict_t *ent, pmenu_t *p);
void	OSP_mapAdminSelect_menu (edict_t *ent, pmenu_t *p);
void	OSP_playerAdminSelect_menu (edict_t *ent, pmenu_t *p);
void	OSP_mapAdminChoose (edict_t *ent, pmenu_t *p);
void	OSP_playerAdminChoose (edict_t *ent, pmenu_t *p);
int		OSP_updateInviteMenu (edict_t *ent);
int		OSP_updateAdminMenu (edict_t *ent);
int		OSP_updateAdminSelectMenu (edict_t *ent);
int		OSP_updateTeamMenu (edict_t *ent);
int		OSP_updateDMMenu (edict_t *ent);
void	OSP_updateVoteMenu (edict_t *ent);
void	OSP_updateVoteMenu2 (edict_t *ent);
void	OSP_updateBotMenu (edict_t *ent);
void	OSP_updateProposalMenu (edict_t *ent);
void	OSP_updateProposalMenu2 (edict_t *ent);
void	OSP_acceptVote_menu (edict_t *ent, pmenu_t *p);
void	OSP_declineVote_menu (edict_t *ent, pmenu_t *p);
void	OSP_menuVotePercent (edict_t *ent, char *out);
void	OSP_id_cmd (edict_t *ent);
void	OSP_hud_cmd (edict_t *ent);
void	OSP_yes_cmd (edict_t *ent);
void	OSP_no_cmd (edict_t *ent);
void	OSP_ChaseCam (edict_t *ent);
void	OSP_startObserve (edict_t *ent);
void	OSP_removeChaseCam (edict_t *ent);
qboolean CameraCmd (edict_t *ent, qboolean force);
int		OSP_votePercent (edict_t *ent, int what);

// The join/leave helpers osp_observe.c and p_camera.c share.
void	 EntityListAdd (edict_t *ent);
void	 EntityListRemove (edict_t *ent);
qboolean OSP_1v1AllowJoin (edict_t *ent);
void	 OSP_1v1Remove (edict_t *ent, int mode);
qboolean OSP_addTeamMember (edict_t *ent, int team);
qboolean OSP_defaultTeam (edict_t *ent);
qboolean OSP_readdTeamMember (edict_t *ent);
void	 OSP_removeTeamMember (edict_t *ent, qboolean quiet);
void	 OSP_observerTeamFrags (edict_t *ent);
void	 OSP_checkHalt (int reason);
void	 OSP_notready_cmd (edict_t *ent, int quiet);
void	 OSP_DoRankSort (void);
int		 OSP_CheckReady (void);
void	 OSP_deadDropRune (edict_t *ent);

// ---------------------------------------------------------------------------
// The mod's own globals, en masse.
// ---------------------------------------------------------------------------

// id CTF's `loc_t`, unchanged.
typedef struct {
	char	*classname;
	int		priority;
} loc_t;

// osp_maps.c's map queue entry (<INVENTED type name>).  `map` is a realloc'd
// array, one entry per maps.txt line: `<map name> [min players] [max players]`.
typedef struct {
	int		minplayers;
	int		maxplayers;
	int		used;
	char	name[64];
} map_t;

extern	map_t	*map;

int		 read_map_entry (FILE *f, char *name, int *lo, int *hi);
int		 read_player_entry (FILE *f, char *name, char *pass, char *addr);
void	 OSP_loadMaps (void);
void	 OSP_loadPlayers (char *filename);
int		 OSP_playerAllow (char *name, char *userinfo);
int		 OSP_addBan (char *name, char *addr);
qboolean OSP_removeBan (char *name, char *addr);
void	 OSP_listbans (edict_t *ent);
qboolean OSP_mapExists (edict_t *ent, char *name, qboolean set);
void	 OSP_mapList (edict_t *ent);
extern	int		end_timeout;
extern	int		ot_count;
extern	char	*single_statusbar;
extern	char	*dm_statusbar;
extern	char	*dm_statusbar_alt;
extern	char	*team_statusbar;
extern	char	*team_statusbar_alt;
extern	int		conf_size;
extern	int		blink_on_count;
extern	int		blink_off_count;
extern	cvar_t	*runes_enable;	// 4-byte cvar_t* -- OSP_endClean reloads rune_stat from it
extern	int		old_botcount;	// bl_spawn.c's CheckMinimumPlayers guard; OSP_endClean resets it
extern	int		bots_delaytime;
extern	int		bots_loadstat;
extern	int		client_maxframes;
extern	int		console_stampcount;
extern	int		maxconn_clients;
extern	int		reconn_index;
extern	int		connected_clients;
extern	int		bot_watch;
extern	int		game_init;
extern	int		sync_startframe;
extern	float	sync_time;
extern	int		time_update;
extern	int		time_blink;
extern	int		start_suddendeath;
extern	int		vote_frametime;
extern	int		bots_votedin;
extern	int		vote_item;
extern	int		vote_yea;
extern	int		vote_nay;
extern	char	wav_file[125];
// The accuracy report's row table: which p_acc_t.shots/hits column each
// printed row names.  Type and member names <INVENTED>.
typedef struct
{
	int		index;			// a weapon index into p_acc_t.shots/hits
	char	name[128];
} a_info_t;

extern	a_info_t a_info[10];
extern	int		motd_read;
extern	loc_t	loc_names[23];
extern	int		num_names;
extern	unsigned	map_size;	// unsigned
extern	int		selected_map;
extern	char	conf_info[50][64];
extern	char	conf_name[50][64];
extern	cvar_t	*qualifier_numspots;
extern	p_acc_t	o_acc[256];
extern	cvar_t	*allow_id;
extern	cvar_t	*time_remaining;
extern	cvar_t	*start_armortype;
extern	char	old_scores[2048];
extern	cvar_t	*match_features;
extern	cvar_t	*max_shells;
extern	cvar_t	*max_bullets;
extern	cvar_t	*max_cells;
extern	cvar_t	*max_grenades;
extern	cvar_t	*max_rockets;
extern	cvar_t	*max_slugs;
extern	cvar_t	*max_health;
extern	cvar_t	*max_armor;
extern	cvar_t	*start_armor;
extern	cvar_t	*hook_holdplayertime;
extern	cvar_t	*team_duelrecover;
extern	cvar_t	*match_pausetime;
extern	cvar_t	*console_timestamp;
extern	cvar_t	*nextlevel_click;
extern	cvar_t	*bots_warmuptime;
extern	int		max_items[11];
extern	cvar_t	*team_a_score;
extern	cvar_t	*demo_referee;
extern	cvar_t	*pack_shells;
extern	int		start_items[11];
extern	cvar_t	*pack_cells;
extern	cvar_t	*vote_time;
extern	cvar_t	*client_nomove;
extern	cvar_t	*vote_carryover;
extern	cvar_t	*match_countinfo;
extern	cvar_t	*start_shells;
extern	int		pack_spawn;
extern	cvar_t	*vote_bots_max;
extern	cvar_t	*referee_password;
extern	cvar_t	*team_a_hookcolor;
extern	cvar_t	*vote_enable_hook;
extern	cvar_t	*vote_enable_map;
extern	cvar_t	*power_armor_screen;
extern	cvar_t	*team_overtime_time;
extern	cvar_t	*vote_enable_time;
extern	cvar_t	*team_b_name;
extern	cvar_t	*map_halt;
extern	cvar_t	*osp_game;
extern	cvar_t	*vote_config_default;
extern	cvar_t	*demo_tag;
extern	cvar_t	*pack_slugs;
extern	cvar_t	*pack_health;
extern	cvar_t	*pack_grenades;
extern	cvar_t	*client_botdetect;
extern	cvar_t	*team_b_score;
extern	cvar_t	*team_a_skin;
extern	cvar_t	*hook_pullspeed;
extern	cvar_t	*resp_delay;
extern	cvar_t	*match_mode;
extern	char	conf_file[2048];
extern	int		p_order[28];
extern	cvar_t	*team_idteam;
extern	cvar_t	*warmup_armor;
extern	cvar_t	*team_b_skin;
extern	cvar_t	*hook_sky;
extern	cvar_t	*vote_countspectators;
extern	cvar_t	*pack_armor;
extern	cvar_t	*bots_autoload;
extern	cvar_t	*bots_botfile;
extern	cvar_t	*bots_minplayers;
extern	cvar_t	*bots_noclients;
extern	cvar_t	*client_recover;
extern	cvar_t	*client_fastweap;
extern	cvar_t	*client_maxping;
extern	cvar_t	*vote_enable_bots;
extern	cvar_t	*vote_enable_toggles;
extern	char	default_timelimit[8];
extern	cvar_t	*weapon_have;
extern	cvar_t	*armor_shard;
extern	cvar_t	*match_endinfo;
extern	int		level_start;
extern	int		start_weap[11];
extern	cvar_t	*team_nextuptime;
extern	cvar_t	*referee_enable;
extern	char	default_hook[8];
extern	cvar_t	*client_muzzlemode;
extern	char	default_fraglimit[8];
extern	cvar_t	*menu_maxtime;
extern	cvar_t	*power_armor_shield;
extern	cvar_t	*match_readypercent;
extern	cvar_t	*vote_enable_config;
extern	cvar_t	*weapon_initial;
extern	cvar_t	*client_maxrate;
extern	cvar_t	*menu_timestep;
extern	cvar_t	*camera_pitch;
extern	cvar_t	*pack_rockets;
extern	cvar_t	*team_overtime_mode;
extern	cvar_t	*ffa_hurtself;
extern	cvar_t	*vote_enable;
extern	cvar_t	*armor_jacket;
extern	cvar_t	*menu_maxfrag;
extern	cvar_t	*demo_player;
extern	cvar_t	*hook_speed;
extern	cvar_t	*vote_enable_runes;
extern	cvar_t	*team_a_name;
extern	cvar_t	*start_cells;
extern	cvar_t	*client_maxfps;
extern	cvar_t	*bots_delayload;
extern	cvar_t	*team_b_hookcolor;
extern	cvar_t	*match_countdown;
extern	cvar_t	*match_timeouts;
extern	cvar_t	*start_grenades;
extern	cvar_t	*hook_color;
extern	cvar_t	*team_maxplayers;
extern	cvar_t	*start_health;
extern	cvar_t	*start_bullets;
extern	int		initial_weap;
extern	cvar_t	*start_rockets;
extern	cvar_t	*match_prestartpercent;
extern	cvar_t	*vote_config_defaultname;
extern	cvar_t	*__current_config;
extern	cvar_t	*hook_wait;
extern	cvar_t	*client_deathweapdrop;
extern	cvar_t	*team_overtime_count;
extern	char	reconn_player[32];
extern	cvar_t	*warmup_health;
extern	cvar_t	*hook_incdamage;
extern	cvar_t	*nextlevel_lazy;
extern	cvar_t	*qualifier_forceskins;
extern	cvar_t	*armor_combat;
extern	cvar_t	*client_minping;
extern	int		pack_items[11];
extern	cvar_t	*team_recovertime;
extern	cvar_t	*client_infochange;
extern	cvar_t	*menu_fragstep;
extern	cvar_t	*team_lockskin;
extern	cvar_t	*match_latejoin;
extern	cvar_t	*hook_maxdamage;
extern	char	vote_value[64];
extern	cvar_t	*qualifier_skinname;
extern	cvar_t	*armor_body;
extern	int		pack_life;
extern	cvar_t	*match_startsound;
extern	cvar_t	*match_endmusic;
extern	cvar_t	*pack_bullets;
extern	cvar_t	*start_slugs;
extern	cvar_t	*hook_holdtime;
extern	cvar_t	*vote_enable_frag;
extern	cvar_t	*vote_enable_kick;
extern	char	match_motd[1024];
extern	char	match_info[1024];
extern	char	voted_botname[32];
extern	int		overtime_timer;
extern	int		frag_offset;
extern	char	pl_bname[200][16];
extern	char	pl_names[200][16];
extern	char	pl_pass[200][32];
extern	char	pl_addr[200][16];
extern	int		next_map;


// osp_hiscore.c (<INVENTED FILENAME>). One entry of the per-map high
// score table.  Type and member names <INVENTED>.
typedef struct
{
	char	name[16];
	char	score[16];
	char	date[16];
	int		isnew;			// 1 = set during this map, drawn with a '*'
} hs_player_t;

extern	int			hs_mode;		// 1 = fraglimit/FPH, 2 = timelimit/frags
extern	int			hs_limit;
extern	hs_player_t	p_table[10];
extern	char		hs_table[1400];
extern	cvar_t		*client_highscores;
extern	int			sync_frame;
extern	int			endlvl_frame;
extern	int			manual_map;


// osp_cmds.c (<INVENTED FILENAME>). The client/vote/referee commands.
// FL_OSP_NOCMD is edict_t.flags bit 0x2000, the mod's own.  <invented name>.
#define FL_OSP_NOCMD	0x2000
// edict_t.flags bit 0x10000 -- the mod's "this client is a bot" flag.
#define FL_OSP_BOT		0x10000

// The three configstring slots the match status line uses.
#define CS_OSP_STATUS_DM	0x623
#define CS_OSP_STATUS_A		0x626
#define CS_OSP_STATUS_B		0x628

// item_settings bits, read off the vote handlers' and/or masks.
#define ITEM_SET_QUAD	1
#define ITEM_SET_BFG	8

void	 OSP_motd_cmd (edict_t *ent);
void	 OSP_talkto_cmd (edict_t *ent);
void	 OSP_ready_cmd (edict_t *ent, int quiet);
void	 OSP_highscores_cmd (edict_t *ent);
void	 OSP_showinfo_cmd (edict_t *ent);
void	 OSP_accuracy_cmd (edict_t *ent);
void	 OSP_accuracyInfo (edict_t *ent, char *name, int cid);
void	 OSP_oldaccuracy_cmd (edict_t *ent);
void	 OSP_oldAccuracyInfo (edict_t *ent, int cid);
void	 OSP_ffajoin_cmd (edict_t *ent);
void	 OSP_vote_cmd (edict_t *ent, int a, int b, char *what, char *value);
void	 OSP_map_vote (void);
void	 OSP_config_vote (void);
void	 OSP_timelimit_vote (void);
void	 OSP_fraglimit_vote (void);
void	 OSP_hook_vote (void);
void	 OSP_runes_vote (void);
void	 OSP_toggle_vote (void);
void	 OSP_bfg_vote (void);
void	 OSP_quad_vote (void);
void	 OSP_kick_vote (void);
void	 OSP_specbot_vote (void);
void	 OSP_addbots_vote (void);
void	 OSP_removebots_vote (void);
void	 OSP_checkVote (void);
void	 OSP_clearVotes (void);
void	 OSP_voteinfo (edict_t *ent, qboolean broadcast);
void	 OSP_listItems (char *out);
void	 OSP_playertime_cmd (edict_t *ent);
void	 OSP_oldscores_cmd (edict_t *ent);
void	 OSP_muzzle_cmd (edict_t *ent);
void	 OSP_isreferee_cmd (edict_t *ent);
void	 OSP_referee_cmd (edict_t *ent);
void	 OSP_rhelp_cmd (edict_t *ent);
void	 OSP_rkick_cmd (edict_t *ent);
void	 OSP_rmap_cmd (edict_t *ent);
void	 OSP_rtimelimit_cmd (edict_t *ent);
void	 OSP_rfraglimit_cmd (edict_t *ent);
void	 OSP_rbanlist_cmd (edict_t *ent);
void	 OSP_rban_cmd (edict_t *ent, char *who);
void	 OSP_rbanaddr_cmd (edict_t *ent);
void	 OSP_runban_cmd (edict_t *ent);
void	 OSP_runbanaddr_cmd (edict_t *ent);
void	 OSP_hookon_cmd (edict_t *ent);
void	 OSP_hookoff_cmd (edict_t *ent);

// osp_main.c (<INVENTED FILENAME>).
void		OSP_configLoad (void);
void	 OSP_configList (edict_t *ent);
qboolean OSP_configExists (edict_t *ent, char *name);
qboolean OSP_configFileExists (char *name);
void	 OSP_gameInit (void);
void	 OSP_endClean (void);
void	 OSP_initWeapItem (void);
void	 OSP_listDisabledItems (char *buf);
void	 OSP_clientConfigString (edict_t *ent, short index, char *string);
void	 OSP_clearStats (edict_t *ent);
void	 OSP_restartStats (edict_t *ent);
void	 OSP_setStats (edict_t *ent);
void	 OSP_showFrags (edict_t *ent);
void	 OSP_updateClock (void);
void	 OSP_getDateInfo (char *out);
void	 OSP_checkAnnounce (edict_t *ent);
qboolean PlayerIdCanSee (edict_t *a, edict_t *b);
int		 OSP_setID (edict_t *ent);
qboolean OSP_changeID (edict_t *ent);
int		 OSP_initID (void);
qboolean loc_CanSee (edict_t *targ, edict_t *inflictor);
void	 loc_buildboxpoints (vec3_t p[8], vec3_t org, vec3_t mins, vec3_t maxs);
int		 OSP_checkItems (void);
void	 OSP_changeItems (void);
void	 OSP_removeItem (char *classname);
void	 OSP_spawnItem (char *classname);
void	 OSP_checkSync (void);
int		 OSP_countReady (void);
void	 OSP_setAllAccuracy (void);
void	 OSP_setSingleAccuracy (edict_t *ent);
void	 OSP_startDemos (void);
void	 OSP_warmupItems (edict_t *ent);
void	 OSP_closeMenus (void);
void	 OSP_serverbotsRemove (void);
void	 OSP_saveClient (edict_t *ent);
void	 OSP_recoverClient (edict_t *ent, char *userinfo);
void	 OSP_giveClientID (edict_t *ent);
void	 OSP_clearClients (void);
void	 OSP_consoleStamp (void);
edict_t	*OSP_findPlayer (char *name);
void	 OSP_setFeatures (void);
void	 OSP_setupAdminLog (void);
void	 OSP_playerAnnounce (edict_t *ent, char sound);
void	 OSP_parseArmor (void);
void	 OSP_parseString (char *s, gitem_armor_t *info);
void	 OSP_setMOTD (void);
void	 OSP_showMOTD ();
void	 OSP_setShowParams (void);
void	 OSP_showParams ();
void	 OSP_showScores (int *list, int count, edict_t *ent);
void	 OSP_showPlayer (edict_t *ent);

// osp_teams.c / osp_players.c (<INVENTED FILENAMES>).
int		 OSP_teamCount (int team);
int		 OSP_teamReady (int team);
qboolean OSP_1v1Team (edict_t *ent);
void	 OSP_1v1Add (edict_t *ent);
void	 OSP_1v1QueueCheck (void);
void	 OSP_initTeamFrags (edict_t *ent);
void	 OSP_playerTeamFrags (edict_t *ent);
void	 OSP_updateTeamFrags (void);
void	 OSP_defaultjoincode_cmd (edict_t *ent);
void	 OSP_joincode_cmd (edict_t *ent);
void	 OSP_teamname_cmd (edict_t *ent);
void	 OSP_teamskin_cmd (edict_t *ent);
void	 OSP_teamjoin_cmd (edict_t *ent, char *teamname);
void	 OSP_switchteam_cmd (edict_t *ent);
void	 OSP_teaminvite_cmd (edict_t *ent);
void	 OSP_lockteam_cmd (edict_t *ent);
void	 OSP_unlockteam_cmd (edict_t *ent);
void	 OSP_readyteam_cmd (edict_t *ent);
void	 OSP_notreadyteam_cmd (edict_t *ent);
void	 OSP_captain_cmd (edict_t *ent);
void	 OSP_captains_cmd (edict_t *ent);
void	 OSP_kickplayer_cmd (edict_t *ent);
void	 OSP_1v1queue_cmd (edict_t *ent);
void	 OSP_teamReset (void);
void	 OSP_findTeamWinner (void);
qboolean OSP_overtimeWork (int count);
void	 OSP_showTeamScores (edict_t *ent);
void	 OSP_showBIGTeamScores (edict_t *ent);
void	 OSP_show1v1Scores (edict_t *ent);
void	 OSP_sayteam_cmd (edict_t *ent, char *msg);

void	 EnitityListClean (void);	// sic -- the target's own typo, in p_camera.c
edict_t	*NextMap (void);

void	 OSP_initHighScores (void);
void	 OSP_formatHighScores (void);
void	 OSP_showHighScores ();
void	 OSP_updateHighScores (void);
void	 OSP_loadHighScores (void);
void	 OSP_writeHighScores (void);
qboolean OSP_makeHSDir (char *base);
int		 OSP_readLine (FILE *f, char *a, char *b, char *c);
void	 OSP_highscoreDate (char *out);

// client data that stays across deathmatch respawns
typedef struct
{
	client_persistant_t	coop_respawn;
	int			enterframe;			// level.framenum the client entered the game
	int			score;				// frags, etc


	vec3_t		cmd_angles;			// angles sent over in the last command

	// Moved here out of client_persistant_t; see the note there.
	int			game_helpchanged;
	int			helpchanged;

	qboolean	spectator;			// client is a spectator

	// OSP: 736 bytes of the mod's own per-match state, at the end of the struct.
	// `osp_rNNN` is the field's byte offset inside this block and nothing more;
	// a field gets a real name only when something actually names it.
	int       osp_r000;
	int       clientid;
	int       osp_r008;
	int       osp_r00c;
	int       osp_r010;
	int       osp_r014;
	int       osp_r018;
	int       osp_r01c;
	int       entered;		// the player state: 1, 2, 4, 8, 0x10
	int       osp_r024;
	int       osp_r028;
	int       osp_r02c;
	int       osp_r030;
	int       osp_r034;
	char      osp_r038[64];	// the last player-ID string sent
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
	int       osp_r0a0;		// countdown, decremented by CameraCmd
	byte      osp_r0a4[4];
	int       osp_r0a8;
	int       osp_r0ac;
	int       osp_r0b0;
	// `char`: it holds the ngWorldStats password string.
	char      osp_r0b4[32];
	int       osp_r0d4;
	int       osp_r0d8;
	int       osp_r0dc;
	int       osp_r0e0;
	int       osp_r0e4;
	int       osp_r0e8;
	int       osp_r0ec;
	int       osp_r0f0;
	byte      osp_r0f4[256];
	// Invented names: ClientThink's 16-sample ping accumulator.
	int       osp_r1f4;		// sample count
	unsigned  osp_r1f8;		// sample sum
	int       osp_r1fc;		// next sample frame
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
	int       team;			// the team index
	int       osp_r2cc;
	int       osp_r2d0;
	int       osp_r2d4;
	int       osp_r2d8;
	int       osp_r2dc;
} client_respawn_t;

// this structure is cleared on each PutClientInServer(),
// except for 'client->pers'
struct gclient_s
{
	// known to server
	player_state_t	ps;				// communicated by server to clients
	int				ping;

	// private to game
	client_persistant_t	pers;
	client_respawn_t	resp;
	pmove_state_t		old_pmove;	// for detecting out-of-pmove changes

	qboolean	showscores;			// set layout stat
	qboolean	showinventory;		// set layout stat
	qboolean	showhelp;
	qboolean	showhelpicon;

	int			ammo_index;

	int			buttons;
	int			oldbuttons;
	int			latched_buttons;

	qboolean	weapon_thunk;

	gitem_t		*newweapon;

	// sum up damage over an entire frame, so
	// shotgun blasts give a single big kick
	int			damage_armor;		// damage absorbed by armor
	int			damage_parmor;		// damage absorbed by power armor
	int			damage_blood;		// damage taken out of health
	int			damage_knockback;	// impact damage
	vec3_t		damage_from;		// origin for vector calculation

	float		killer_yaw;			// when dead, look at killer

	weaponstate_t	weaponstate;
	vec3_t		kick_angles;	// weapon kicks
	vec3_t		kick_origin;
	float		v_dmg_roll, v_dmg_pitch, v_dmg_time;	// damage kicks
	float		fall_time, fall_value;		// for view drop on fall
	float		damage_alpha;
	float		bonus_alpha;
	vec3_t		damage_blend;
	vec3_t		v_angle;			// aiming direction
	float		bobtime;			// so off-ground doesn't change it
	vec3_t		oldviewangles;
	vec3_t		oldvelocity;

	float		next_drown_time;
	int			old_waterlevel;
	int			breather_sound;

	int			machinegun_shots;	// for weapon raising

	// animation vars
	int			anim_end;
	int			anim_priority;
	qboolean	anim_duck;
	qboolean	anim_run;

	// powerup timers
	float		quad_framenum;
	float		invincible_framenum;
	float		breather_framenum;
	float		enviro_framenum;

	qboolean	grenade_blew_up;
	float		grenade_time;
	int			silencer_shots;
	int			weapon_sound;

	float		pickup_msg_time;
	float		respawn_time;		// can respawn when time > this
	pmenuhnd_t	*menu;				// id CTF's own field
	qboolean	inmenu;
	float		flood_locktill;		// locked from talking
	float		flood_when[10];		// when messages were said
	int			flood_whenhead;		// head pointer for when said

	// OSP: 136 bytes of new gclient_t state, at the end.  Same convention as the
	// client_respawn_t block above -- `osp_tNNN` is the byte offset inside this
	// block and is not a recovered name.  The three grapple members are id CTF's,
	// with the CTF prefix stripped.
	edict_t   *grapple;
	int       grapplestate;
	float     grapplereleasetime;
	float     osp_t00c;
	edict_t   *chase_target;
	qboolean  update_chase;
	int       osp_t018;
	byte      osp_t01c[4];
	float     osp_t020;
	int       osp_t024;
	short     osp_t028[2];	// the previous ucmd->angles pair
	byte      osp_t02c[8];
	byte      osp_t034[4];
	int       osp_t038;
	edict_t   *osp_t03c;	// camera target
	int       osp_t040;
	int       osp_t044;
	vec3_t    osp_t048;		// camera target death position
	double    osp_t054;		// camera XY lag
	double    osp_t05c;		// camera Z lag
	double    osp_t064;		// camera angle lag
	float     osp_t06c;
	float     osp_t070;
	float     osp_t074;
	float     osp_t078;
	float     osp_t07c;
	float     osp_t080;
	float     osp_t084;
};

extern	gclient_t	saved_clients[128];


struct edict_s
{
	entity_state_t	s;
	struct gclient_s	*client;	// NULL if not a player
									// the server expects the first part
									// of gclient_s to be a player_state_t
									// but the rest of it is opaque

	qboolean	inuse;
	int			linkcount;

	// FIXME: move these fields to a server private sv_entity_t
	link_t		area;				// linked to a division node or leaf
	
	int			num_clusters;		// if -1, use headnode instead
	int			clusternums[MAX_ENT_CLUSTERS];
	int			headnode;			// unused if num_clusters != -1
	int			areanum, areanum2;

	//================================

	int			svflags;
	vec3_t		mins, maxs;
	vec3_t		absmin, absmax, size;
	solid_t		solid;
	int			clipmask;
	edict_t		*owner;


	// DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
	// EXPECTS THE FIELDS IN THAT ORDER!

	//================================
	int			movetype;
	int			flags;

	char		*model;
	float		freetime;			// sv.time when the object was freed
	
	//
	// only used locally in game, not by server
	//
	char		*message;
	char		*classname;
	int			spawnflags;

	float		timestamp;

	float		angle;			// set in qe3, -1 = up, -2 = down
	char		*target;
	char		*targetname;
	char		*killtarget;
	char		*team;
	char		*pathtarget;
	char		*deathtarget;
	char		*combattarget;
	edict_t		*target_ent;

	float		speed, accel, decel;
	vec3_t		movedir;
	vec3_t		pos1, pos2;

	vec3_t		velocity;
	vec3_t		avelocity;
	int			mass;
	float		air_finished;
	float		gravity;		// per entity gravity multiplier (1.0 is normal)
								// use for lowgrav artifact, flares

	edict_t		*goalentity;
	edict_t		*movetarget;
	float		yaw_speed;
	float		ideal_yaw;

	float		nextthink;
	void		(*prethink) (edict_t *ent);
	void		(*think)(edict_t *self);
	void		(*blocked)(edict_t *self, edict_t *other);	//move to moveinfo?
	void		(*touch)(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf);
	void		(*use)(edict_t *self, edict_t *other, edict_t *activator);
	void		(*pain)(edict_t *self, edict_t *other, float kick, int damage);
	void		(*die)(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point);

	float		touch_debounce_time;		// are all these legit?  do we need more/less of them?
	float		pain_debounce_time;
	float		damage_debounce_time;
	float		fly_sound_debounce_time;	//move to clientinfo
	float		last_move_time;

	int			health;
	int			max_health;
	int			gib_health;
	int			deadflag;
	qboolean	show_hostile;

	float		powerarmor_time;

	char		*map;			// target_changelevel

	int			viewheight;		// height above origin where eyesight is determined
	int			takedamage;
	int			dmg;
	int			radius_dmg;
	float		dmg_radius;
	int			sounds;			//make this a spawntemp var?
	int			count;

	edict_t		*chain;
	edict_t		*enemy;
	edict_t		*oldenemy;
	edict_t		*activator;
	edict_t		*groundentity;
	int			groundentity_linkcount;
	edict_t		*teamchain;
	edict_t		*teammaster;

	edict_t		*mynoise;		// can go in client only
	edict_t		*mynoise2;

	int			noise_index;
	int			noise_index2;
	float		volume;
	float		attenuation;

	// timing variables
	float		wait;
	float		delay;			// before firing targets
	float		random;

	float		teleport_time;

	int			watertype;
	int			waterlevel;

	vec3_t		move_origin;
	vec3_t		move_angles;

	// move this to clientinfo?
	int			light_level;

	int			style;			// also used as areaportal number

	gitem_t		*item;			// for bonus items

	// common data blocks
	moveinfo_t		moveinfo;
	monsterinfo_t	monsterinfo;

	char			osp_e37c[32];	// a string; %s in the q2log_* lines
	int				osp_e39c;
	char			osp_e3a0[16];	// default team name
	char			osp_e3b0[80];	// default team skin
	int				osp_e400;
	int				osp_e404;
	int				osp_e408;
	byte			osp_e40c[20];
	int				osp_e420;

	char			*name;
	char			*skin;
	char			*charfile;
	char			*charname;

	visiblebbox_t	box;
};
