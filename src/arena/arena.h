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
#ifndef G_ARENA_H
#define G_ARENA_H

#define MAX_ARENAS          32
#define MAX_TEAMS           256
#define MAX_ARENA_SKINS     7

// The buffer trigger_multiple rewrites its message into when the trigger is an
// arena door.  RA2 sprintf'd into a bare 300; the arena name comes out of the
// map, so the size has a name and the write is bounded (R-SEC-1).
#define ARENA_MESSAGE_SIZE  300

#define ASTATE_WARMUP           0
#define ASTATE_COUNTDOWN        1
#define ASTATE_FIGHTING         2
#define ASTATE_ROUNDEND         3
#define ASTATE_INTERMISSION     4
#define ASTATE_RESULTS          5
#define ASTATE_NEXTROUND        6

#define FIGHT_SPECTATING        0
#define FIGHT_ALIVE             1
#define FIGHT_DEAD              2

// MOD_GRAPPLE was 34 here and is 56 in g_local.h's merged means-of-death list;
// the number is the union's, the concept is one (R-CORE-6).

// STAT_LINEPOSITION (19) and STAT_CTF_ID_VIEW (20) were here; they are rows in
// g_stats.h's map now, as SID_RA_LINEPOSITION and SID_RA_ID_VIEW, along with
// the eight RA2 kept in its own g_local.h (R-OSP-7 clause 3).

#define MAX_STATUS_TEAMS        2
#define MAX_STATUS_MEMBERS      4

// The grapple -- state enum, speeds AND the eleven CTF* declarations that were
// here -- lives in src/ctf/g_ctf.h.  RA2
// declares the same enum and carries CTFResetGrapple verbatim, which is what
// measuring settled in R-50: there is ONE grapple implementation, Threewave's,
// and RA2's is a fork of it rather than a rival design (sec 7 rule 6).

typedef struct motd_s {
    char            *line;
    struct motd_s   *next;
    struct motd_s   *prev;
} motd_t;

typedef struct team_s {
    char        *name;
    int         teamnum;
    int         arenanum;
    int         wins;

    qmenu_t     arenalink;

    bool    locked;

    int         side;

    int         skin;

    bool    fighting;

    bool    outofline;
} team_t;

#define TEAM(node)  ((team_t *)(node)->it)

typedef struct arena_settings_s {
    int         playersperteam;
    int         rounds;
    int         weapons;
    int         armor;
    int         health;
    int         minping;
    int         maxping;
    int         rocket_speed;
    int         shells, bullets, slugs, grenades, rockets, cells;

    int         startdelay;

    int         fastswitch;
    int         armorprotect;
    int         healthprotect;
    int     fallingdamage;
    int     allow_voting_armor;
    int     allow_voting_health;
    int     allow_voting_minping;
    int     allow_voting_maxping;
    int     allow_voting_playersperteam;
    int     allow_voting_rounds;
    int     allow_voting_maxteams;
    int     allow_voting_armorprotect;
    int     allow_voting_healthprotect;
    int     allow_voting_shotgun;
    int     allow_voting_supershotgun;
    int     allow_voting_machinegun;
    int     allow_voting_chaingun;
    int     allow_voting_grenadelauncher;
    int     allow_voting_rocketlauncher;
    int     allow_voting_hyperblaster;
    int     allow_voting_railgun;
    int     allow_voting_bfg;
    int     allow_voting_fallingdamage;
    int     locked;
    int     competition;
    int     scorebydamage;
    int     changed;
} arena_settings_t;

typedef struct arena_s {
    int         numteams;

    qmenu_t     waitingteams;

    qmenu_t     activeteams;

    int         state;

    bool    teamplay;

    int         countdown_next_tick;

    int         countdown;

    bool    active;

    char        msg[160];

    char        vs[64];

    int         playersperteam;

    int         rounds;

    int         weapons;

    int         armor;
    int         health;

    int         minping, maxping;

    int         rocket_speed;

    int         shells, bullets, slugs, grenades, rockets, cells;

    int         startdelay;

    int         fastswitch;
    int         armorprotect;
    int         healthprotect;
    int     fallingdamage;
    int     allow_voting_armor;
    int     allow_voting_health;
    int     allow_voting_minping;
    int     allow_voting_maxping;
    int     allow_voting_playersperteam;
    int     allow_voting_rounds;
    int     allow_voting_maxteams;
    int     allow_voting_armorprotect;
    int     allow_voting_healthprotect;
    int     allow_voting_shotgun;
    int     allow_voting_supershotgun;
    int     allow_voting_machinegun;
    int     allow_voting_chaingun;
    int     allow_voting_grenadelauncher;
    int     allow_voting_rocketlauncher;
    int     allow_voting_hyperblaster;
    int     allow_voting_railgun;
    int     allow_voting_bfg;
    int     allow_voting_fallingdamage;
    int     locked;
    int     competition;
    int     scorebydamage;
    int     changed;
    float       proposetime;

    arena_settings_t    proposed;

    int         votetries;
    int         votes_yes, votes_no;
    edict_t     *proposer;

    bool    idarena;

    int         sidepick;

    int         maxteams;

    int         round;
    team_t      *pickupteam[2];

    struct ra2_round_s  *stats;     // NULL when statsfile is off
} arena_t;

extern  int         votetries_setting;
extern  bool    allow_grapple;
extern  bool    broken;

extern  arena_t     arenas[MAX_ARENAS];
extern  int         num_arenas;
extern  bool    idmap;

extern  qmenu_t     *teams;

extern  motd_t      motd;
extern  cvar_t      *admincode;
// R-RA-5, re-expressed against RA2's queue rather than Gladiator's own arena.
extern  cvar_t      *ra_playercycle;
extern  cvar_t      *ra_botcycle;

extern  char        *teamskins[MAX_ARENA_SKINS];
extern  char        *vwepmodels[4];
extern  int    teamskins_precachem[MAX_ARENA_SKINS];
extern  int    teamskins_precachef[MAX_ARENA_SKINS];
extern  int    teamskins_precachecw[MAX_ARENA_SKINS];
extern  int    teamskins_precachecb[MAX_ARENA_SKINS];

extern  char        *omode_descriptions[4];

extern const char   dm_statusbar[];

extern  int         weapon_vals[9];

int         count_queue(qmenu_t *head);
int         count_players_queue(qmenu_t *head);

void        set_damage(int arenanum, int state);
void        give_ammo(edict_t *ent);

team_t      *add_to_team(edict_t *ent, char *teamname);
char        *RA_NewTeamName(edict_t *ent);
void        RA_BotJoinArena(edict_t *ent);
void        remove_from_team(edict_t *ent);

edict_t     *SelectRandomArenaSpawnPoint(char *classn, int arenanum, int side);
edict_t     *SelectFarthestArenaSpawnPoint(char *classn, int arenanum);

void        track_SetStats(edict_t *ent);
void        eyecam_think(edict_t *ent, usercmd_t *ucmd);
void        track_think(edict_t *ent, usercmd_t *ucmd);
void        track_change(edict_t *ent, int dir);
void        track_next(edict_t *ent);
void        track_prev(edict_t *ent);
void        SetObserverMode(edict_t *ent);

void        move_to_arena(edict_t *ent, int arenanum, int mode);
void        ChangeOMode(edict_t *ent);

int         getfreeskin(int arenanum);
void        setteamskin(edict_t *ent, char *userinfo, int skinnum);

void        SendTeamToArena(qmenu_t *team, int arenanum, bool observer, bool announce);
int         AddtoArena(edict_t *ent, int arenanum, int allow_partial, int skip_checks);
void        check_teams(int arenanum);

void        init_player(edict_t *ent);
void        reinit_player(edict_t *ent);

void        show_stringc(char *s, int context);
void        show_string(int priority, char *s, int context);
void        send_sound_to_arena(char *soundname, int context);
void        send_configstring(edict_t *e, int index, char *string);
void        show_countdown(int countdown, int arenanum);
int     show_rank(qmenu_t *node);

bool    check_for_teams(int arenanum);
int         fill_arena(int arenanum);
int         fight_done(int arenanum);

void        CTFSetIDView(edict_t *ent);
void        UpdateStatusBars(int arenanum);
void        check_telefrag(int arenanum);

void        start_voting(edict_t *proposer, int arenanum);
void        check_voting(int arenanum);

void        arena_think(int arenanum);
void        multi_arena_think(void);
void        arena_init(edict_t *wsent);
void        Cmd_admin_f(edict_t *ent);
void        Cmd_arenaadmin_f(edict_t *ent, unsigned mode);
void        Cmd_menuhelp_f(edict_t *ent);
void        list_keys(edict_t *ent);
void        print_map_loop(edict_t *ent);

void        SP_func_illusionary(edict_t *ent);

char        *getarenaname(int arenanum);
char        *get_next_map(char *current);        // maploop.c
void        multi_arena_think(void);
void        RA_CheckRules(void);
void        RA_EndLevel(void);
int         RA_SkinIcon(edict_t *ent);
void        RA_ZBotSample(edict_t *ent, usercmd_t *ucmd);
void        RA_Obituary(edict_t *self, edict_t *inflictor, edict_t *attacker);
void        send_sound_to_arena(char *soundname, int context);
void        GSLogDeath(edict_t *self, edict_t *inflictor, edict_t *attacker);
void        init_player(edict_t *ent);
void        reinit_player(edict_t *ent);
void        track_think(edict_t *ent, usercmd_t *ucmd);
void        eyecam_think(edict_t *ent, usercmd_t *ucmd);
void        track_next(edict_t *ent);
void        track_prev(edict_t *ent);
void        RA_SetQueueStats(edict_t *ent);
void        RA_ScoreboardMessage(edict_t *ent, edict_t *killer);
void        arena_init(edict_t *wsent);
void        GSLogStartup(void);
void        GSLogShutdown(void);
#ifdef _WIN32
bool        GSNetStartup(void);
void        GSNetShutdown(void);
#endif
void        menu_centerprint(edict_t *ent, char *message);
int         menuRefreshTeamList(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg);

void        motd_menu(edict_t *ent);


void        GSLogStartup(void);
void        GSLogShutdown(void);
void        GSLogNewmap(void);
void        GSLogEnter(edict_t *ent);
void        GSLogExit(edict_t *ent);
void        GSLogDeath(edict_t *self, edict_t *inflictor, edict_t *attacker);
#ifdef _WIN32
bool        GSNetStartup(void);
void        GSNetShutdown(void);
#endif


#endif // G_ARENA_H
