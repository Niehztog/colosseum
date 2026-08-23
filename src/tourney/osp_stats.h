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
// osp_stats.h -- the local game-event / statistics log.
//
// Replaces the NetGames USA logging stack that shipped with tourney v2.75:
// `nglog.c` (the ngLog / ngWorldStats file writer), `ngmark.c` (the
// ngWorldStats MD5 verification mark and the XOR-scrambled log body) and
// `q2log.c` (the ngLog 1.2 event formatter).  ngWorldStats and ngStats have
// been offline for over two decades; what the subsystem still did on every
// map change was scramble a log nothing can read, spawn `ngWorldStats` /
// `ngStatsQ2T` out of `NetGamesUSA.com/` through `system()`, and ask every
// connecting client to hand over its ngWorldStats account password in
// cleartext.
//
// The events it collected were worth keeping, so they are collected here
// instead and appended to a local file, one JSON object per line.  Nothing
// leaves the machine and nothing is linked in beyond libc.
//
// Controlled by four cvars:
//
//   statsfile           0 = off, 1 = on (default 1)
//   statsname           file name under <basedir>/<gamedir>
//                       (default osptourney.jsonl)
//   stats_logchat       log chat lines (default 0)
//   stats_logallpickups log every item pickup, not just the powerful ones
//                       (default 0)
//
// The Standard Log (`stdlog.c` / `sl_write.c`) is unaffected.  It is a
// different, unrelated format that also writes locally; it used to borrow
// nglog.c's file writer and now has its own.

#ifndef OSP_STATS_H
#define OSP_STATS_H

#define OSP_STATS_VERSION   1

extern  cvar_t  *statsfile;
extern  cvar_t  *statsname;
extern  cvar_t  *stats_logchat;
extern  cvar_t  *stats_logallpickups;

// lifecycle
void    OSP_Stats_Init(void);
void    OSP_Stats_Shutdown(const char *reason);
void    OSP_Stats_GameInit(void);
void    OSP_Stats_MatchStart(void);
void    OSP_Stats_MatchEnd(const char *reason);

// players
void    OSP_Stats_PlayerConnect(edict_t *ent);
void    OSP_Stats_PlayerReconnect(edict_t *ent);
void    OSP_Stats_PlayerEnter(edict_t *ent);
void    OSP_Stats_PlayerLeave(edict_t *ent);
void    OSP_Stats_PlayerRename(edict_t *ent, const char *oldname);
void    OSP_Stats_PlayerRespawn(edict_t *ent);
void    OSP_Stats_PlayerMode(edict_t *ent, const char *mode);
void    OSP_Stats_Chat(const char *text);
void    OSP_Stats_BotDetect(edict_t *ent, const char *why);

// teams
void    OSP_Stats_TeamName(const char *name);
void    OSP_Stats_TeamRename(const char *oldname, const char *newname);
void    OSP_Stats_TeamJoin(edict_t *ent);
void    OSP_Stats_TeamLeave(edict_t *ent);

// votes
void    OSP_Stats_Vote(const char *result, const char *what, const char *value);

// items
void    OSP_Stats_ItemPickup(const char *name, int entnum, edict_t *ent);
void    OSP_Stats_ItemUse(const char *name, edict_t *ent);
void    OSP_Stats_ItemExpire(const char *name, edict_t *ent, int entnum);
void    OSP_Stats_ItemDrop(const char *name, int entnum, edict_t *ent);

// combat
void    OSP_Stats_Death(edict_t *self, edict_t *inflictor, edict_t *attacker);
void    OSP_Stats_Accuracy(edict_t *ent);
void    OSP_Stats_AccuracyAll(void);

// "YY.MM.DD.HH.MM", the stamp the admin log and the demo names use
void    OSP_Stats_DateString(char *out, size_t size);

#endif // OSP_STATS_H
