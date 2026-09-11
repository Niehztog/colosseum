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
// Rocket Arena 2 v2.25, from rocketarena2-public@d20e1ce.
// Donor-only: baseq2 has no counterpart, so it lives in src/arena/ rather than
// being merged into a spine file.  The reconstruction's asm-matching
// address comments are stripped.
// ra2stats.h -- local round statistics log
//
// Replaces the GameSpy `gstats` SDK that shipped with RA2 v2.25.  That
// subsystem accumulated per-round numbers in a tagged-value store and
// uploaded them to gamestats.gamespy.com, which has been offline for years;
// every round start still paid for a DNS lookup and a connect attempt to a
// dead host, and anything it could not send went into an on-disk retry cache
// that was never drained.
//
// The numbers it collected were worth keeping, so they are collected here
// instead and appended to a local file, one JSON object per round.  Nothing
// leaves the machine and nothing is linked in beyond libc.
//
// Controlled by two cvars, mirroring RA2's own logfile/logname pair:
//
//   statsfile   0 = off, 1 = on (default 1).  Registered by G_InitRuleset()
//               and read as `g_statsfile`, because tourney names the pair too
//               and means the same thing by it (R-COMPAT-6)
//   statsname   file name under <homedir-or-basedir>/<gamedir>; ra2stats.jsonl
//               is the default the resolver picks under `arena`
//
// gslog.c's StdLog is unaffected -- it logs individual kills and connects
// across the whole server, where this logs a per-arena, per-round summary.

#ifndef RA2STATS_H
#define RA2STATS_H

#define RA2_STATS_VERSION   1

// per-player counters carried through a round.  Order matters only in that
// RA2_Stats_Add() takes one of these.
typedef enum {
    RA2_STAT_SCORE,
    RA2_STAT_DEATHS,
    RA2_STAT_SUICIDES,
    RA2_STAT_GRENADEKILLS,
    RA2_STAT_ROCKETKILLS,
    RA2_STAT_RAILKILLS,
    RA2_STAT_OTHERKILLS,
    RA2_NUM_STATS
} ra2_stat_t;

// RA2's global team table holds up to MAX_TEAMS entries but only the handful
// actually fighting in an arena appear in a round record, so the team slots
// here are allocated on demand and carry the global index as `id`.
#define RA2_STATS_MAX_TEAMS 16

typedef struct {
    bool        inuse;                  // holds numbers worth writing out
    bool        active;                 // still on a team, still counting
    int         slot;                   // client slot, 1-based, as the GameSpy pid was
    int         team;                   // global team index, -1 if none
    int         ping;
    char        name[16];               // matches client_persistant_t::netname
    int         stat[RA2_NUM_STATS];
} ra2_pstats_t;

typedef struct {
    bool        inuse;
    int         id;                     // index into RA2's global teams[]
    char        name[32];
    int         score;
} ra2_tstats_t;

typedef struct ra2_round_s {
    int             arena;
    int             round;              // 1-based round number within the match
    int             rounds;             // rounds the arena is configured for
    int             start_framenum;
    char            mapname[MAX_QPATH];

    // arena settings, snapshotted at round start
    int             armor, health;
    int             armorprotect, healthprotect;
    int             fallingdamage, compmode, damagescoring;

    ra2_tstats_t    teams[RA2_STATS_MAX_TEAMS];
    ra2_pstats_t    players[MAX_CLIENTS];
} ra2_round_t;

void    RA2_Stats_Init(void);
void    RA2_Stats_Shutdown(void);

// round lifecycle.  RA2_Stats_Begin() returns NULL when logging is off, and
// every other entry point tolerates a NULL round, so callers do not need to
// test the cvar themselves.
ra2_round_t *RA2_Stats_Begin(int arenanum);
void    RA2_Stats_Write(ra2_round_t *r);        // append the round record
void    RA2_Stats_End(ra2_round_t *r);          // write and free
void    RA2_Stats_NextRound(ra2_round_t *r);

void    RA2_Stats_AddTeam(ra2_round_t *r, int team, const char *name);
void    RA2_Stats_TeamScore(ra2_round_t *r, int team, int delta);
void    RA2_Stats_ArenaInfo(ra2_round_t *r, int arenanum);

void    RA2_Stats_AddPlayer(ra2_round_t *r, edict_t *ent, int team);
void    RA2_Stats_RemovePlayer(ra2_round_t *r, int slot);
void    RA2_Stats_Add(ra2_round_t *r, int slot, ra2_stat_t stat, int delta);
void    RA2_Stats_Set(ra2_round_t *r, int slot, ra2_stat_t stat, int value);

#endif // RA2STATS_H
