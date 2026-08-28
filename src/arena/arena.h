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

// offsetof, for the layout assertions below (R-SEC-8).
#include <stddef.h>

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

// The block add_to_team() copies a name into.  It was a bare 100 at the
// allocation and an unbounded sprintf at the fill (R-SEC-1).
#define ARENA_TEAMNAME_SIZE     100
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

// R-SEC-8: `arena_settings_t` is addressed BY INDEX.  `ra2menus.c` reads and
// writes it as `int settings[42]` -- settings[2] is the weapon mask,
// settings[17] the armour protection, settings[41] the changed flag -- and
// `arena_t` carries a second, inline copy of the same 42 members that
// `arena.c` and `ra2menus.c` memcpy across with `sizeof(arena_settings_t)`.
// Both are assumptions about layout that no compiler was checking, and the
// class is the one that bit RA2 after its own port: a qboolean -> bool
// retype shrank this struct from 168 bytes to 96 while `ra2menus.c` still
// punned it as int[42], putting four writes outside `arena_t`.
//
// So the layout is pinned rather than trusted.  A member inserted, removed,
// reordered or retyped now fails the build on the line that names it,
// instead of silently moving every index after it.
_Static_assert(sizeof(arena_settings_t) == 42 * sizeof(int),
               "arena_settings_t is read as int[42] by ra2menus.c");
_Static_assert(offsetof(arena_settings_t, playersperteam) == 0 * sizeof(int),
               "arena_settings_t.playersperteam is index 0");
_Static_assert(offsetof(arena_settings_t, rounds) == 1 * sizeof(int),
               "arena_settings_t.rounds is index 1");
_Static_assert(offsetof(arena_settings_t, weapons) == 2 * sizeof(int),
               "arena_settings_t.weapons is index 2");
_Static_assert(offsetof(arena_settings_t, armor) == 3 * sizeof(int),
               "arena_settings_t.armor is index 3");
_Static_assert(offsetof(arena_settings_t, health) == 4 * sizeof(int),
               "arena_settings_t.health is index 4");
_Static_assert(offsetof(arena_settings_t, minping) == 5 * sizeof(int),
               "arena_settings_t.minping is index 5");
_Static_assert(offsetof(arena_settings_t, maxping) == 6 * sizeof(int),
               "arena_settings_t.maxping is index 6");
_Static_assert(offsetof(arena_settings_t, rocket_speed) == 7 * sizeof(int),
               "arena_settings_t.rocket_speed is index 7");
_Static_assert(offsetof(arena_settings_t, shells) == 8 * sizeof(int),
               "arena_settings_t.shells is index 8");
_Static_assert(offsetof(arena_settings_t, bullets) == 9 * sizeof(int),
               "arena_settings_t.bullets is index 9");
_Static_assert(offsetof(arena_settings_t, slugs) == 10 * sizeof(int),
               "arena_settings_t.slugs is index 10");
_Static_assert(offsetof(arena_settings_t, grenades) == 11 * sizeof(int),
               "arena_settings_t.grenades is index 11");
_Static_assert(offsetof(arena_settings_t, rockets) == 12 * sizeof(int),
               "arena_settings_t.rockets is index 12");
_Static_assert(offsetof(arena_settings_t, cells) == 13 * sizeof(int),
               "arena_settings_t.cells is index 13");
_Static_assert(offsetof(arena_settings_t, startdelay) == 14 * sizeof(int),
               "arena_settings_t.startdelay is index 14");
_Static_assert(offsetof(arena_settings_t, fastswitch) == 15 * sizeof(int),
               "arena_settings_t.fastswitch is index 15");
_Static_assert(offsetof(arena_settings_t, armorprotect) == 16 * sizeof(int),
               "arena_settings_t.armorprotect is index 16");
_Static_assert(offsetof(arena_settings_t, healthprotect) == 17 * sizeof(int),
               "arena_settings_t.healthprotect is index 17");
_Static_assert(offsetof(arena_settings_t, fallingdamage) == 18 * sizeof(int),
               "arena_settings_t.fallingdamage is index 18");
_Static_assert(offsetof(arena_settings_t, allow_voting_armor) == 19 * sizeof(int),
               "arena_settings_t.allow_voting_armor is index 19");
_Static_assert(offsetof(arena_settings_t, allow_voting_health) == 20 * sizeof(int),
               "arena_settings_t.allow_voting_health is index 20");
_Static_assert(offsetof(arena_settings_t, allow_voting_minping) == 21 * sizeof(int),
               "arena_settings_t.allow_voting_minping is index 21");
_Static_assert(offsetof(arena_settings_t, allow_voting_maxping) == 22 * sizeof(int),
               "arena_settings_t.allow_voting_maxping is index 22");
_Static_assert(offsetof(arena_settings_t, allow_voting_playersperteam) == 23 * sizeof(int),
               "arena_settings_t.allow_voting_playersperteam is index 23");
_Static_assert(offsetof(arena_settings_t, allow_voting_rounds) == 24 * sizeof(int),
               "arena_settings_t.allow_voting_rounds is index 24");
_Static_assert(offsetof(arena_settings_t, allow_voting_maxteams) == 25 * sizeof(int),
               "arena_settings_t.allow_voting_maxteams is index 25");
_Static_assert(offsetof(arena_settings_t, allow_voting_armorprotect) == 26 * sizeof(int),
               "arena_settings_t.allow_voting_armorprotect is index 26");
_Static_assert(offsetof(arena_settings_t, allow_voting_healthprotect) == 27 * sizeof(int),
               "arena_settings_t.allow_voting_healthprotect is index 27");
_Static_assert(offsetof(arena_settings_t, allow_voting_shotgun) == 28 * sizeof(int),
               "arena_settings_t.allow_voting_shotgun is index 28");
_Static_assert(offsetof(arena_settings_t, allow_voting_supershotgun) == 29 * sizeof(int),
               "arena_settings_t.allow_voting_supershotgun is index 29");
_Static_assert(offsetof(arena_settings_t, allow_voting_machinegun) == 30 * sizeof(int),
               "arena_settings_t.allow_voting_machinegun is index 30");
_Static_assert(offsetof(arena_settings_t, allow_voting_chaingun) == 31 * sizeof(int),
               "arena_settings_t.allow_voting_chaingun is index 31");
_Static_assert(offsetof(arena_settings_t, allow_voting_grenadelauncher) == 32 * sizeof(int),
               "arena_settings_t.allow_voting_grenadelauncher is index 32");
_Static_assert(offsetof(arena_settings_t, allow_voting_rocketlauncher) == 33 * sizeof(int),
               "arena_settings_t.allow_voting_rocketlauncher is index 33");
_Static_assert(offsetof(arena_settings_t, allow_voting_hyperblaster) == 34 * sizeof(int),
               "arena_settings_t.allow_voting_hyperblaster is index 34");
_Static_assert(offsetof(arena_settings_t, allow_voting_railgun) == 35 * sizeof(int),
               "arena_settings_t.allow_voting_railgun is index 35");
_Static_assert(offsetof(arena_settings_t, allow_voting_bfg) == 36 * sizeof(int),
               "arena_settings_t.allow_voting_bfg is index 36");
_Static_assert(offsetof(arena_settings_t, allow_voting_fallingdamage) == 37 * sizeof(int),
               "arena_settings_t.allow_voting_fallingdamage is index 37");
_Static_assert(offsetof(arena_settings_t, locked) == 38 * sizeof(int),
               "arena_settings_t.locked is index 38");
_Static_assert(offsetof(arena_settings_t, competition) == 39 * sizeof(int),
               "arena_settings_t.competition is index 39");
_Static_assert(offsetof(arena_settings_t, scorebydamage) == 40 * sizeof(int),
               "arena_settings_t.scorebydamage is index 40");
_Static_assert(offsetof(arena_settings_t, changed) == 41 * sizeof(int),
               "arena_settings_t.changed is index 41");

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

// The second half of the same contract: `arena_t` repeats those 42 members
// inline, from `playersperteam` to `changed`, and both arena.c and ra2menus.c
// copy a whole `arena_settings_t` over that run with memcpy.  If the run and
// the struct ever differ in extent the copy writes past `changed` and into
// `proposetime` -- so the extent is pinned too, measured from the first member
// of the run to the field that follows it.
_Static_assert(offsetof(arena_t, proposetime) - offsetof(arena_t, playersperteam)
               == sizeof(arena_settings_t),
               "arena_t's inline settings run must match arena_settings_t");
_Static_assert(offsetof(arena_t, changed) - offsetof(arena_t, playersperteam)
               == offsetof(arena_settings_t, changed),
               "arena_t's inline settings run must match arena_settings_t");

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
// R-RA-7: the bot count follows the arena's own size instead of the server's.
extern  cvar_t      *ra_botfill;

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
bool        RA_RoundFighting(edict_t *ent);
bool        RA_HoldFire(edict_t *ent);
void        give_ammo(edict_t *ent);

team_t      *add_to_team(edict_t *ent, char *teamname);
char        *RA_NewTeamName(edict_t *ent);
void        RA_BotJoinArena(edict_t *ent);
void        remove_from_team(edict_t *ent);

edict_t     *SelectRandomArenaSpawnPoint(char *classn, int arenanum, int side, edict_t *ignore);
edict_t     *SelectFarthestArenaSpawnPoint(char *classn, int arenanum, edict_t *ignore);

// R-RA-7.  The four the bot layer asks for, and `sv arenadump`/`sv ruleset`
// report; see the block above RA_BotFillArena() in arena.c for why the answer
// comes from `arena.cfg` for one kind of arena and from the map for the other.
int         RA_BotFillArena(void);
int         RA_BotFillTarget(int arenanum);
void        RA_BotFillNoMore(int achieved);
int         RA_ArenaPlayers(int arenanum, int *bots);
char        *RA_ArenaBotName(int arenanum);

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
void        G_Svcmd_ArenaDump_f(void);

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

void        ra_SP_func_illusionary(edict_t *ent);

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
void        menu_centerprint(edict_t *ent, char *message);
int         menuRefreshTeamList(edict_t *ent, qmenu_t *menu, qmenu_t *item, int arg);
bool        RA_RefreshMenuCounts(edict_t *ent);

void        motd_menu(edict_t *ent);


void        GSLogStartup(void);
void        GSLogShutdown(void);
void        GSLogNewmap(void);
void        GSLogEnter(edict_t *ent);
void        GSLogExit(edict_t *ent);
void        GSLogDeath(edict_t *self, edict_t *inflictor, edict_t *attacker);


#endif // G_ARENA_H
