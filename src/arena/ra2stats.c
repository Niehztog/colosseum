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
// Rocket Arena 2 v2.25, from rocketarena2@358b325.
// Donor-only: baseq2 has no counterpart, so it lives in src/arena/ rather than
// being merged into a spine file.  The reconstruction's asm-matching
// address comments are stripped.
// ra2stats.c -- local round statistics log.  See ra2stats.h for why.

#include <time.h>

#include "g_local.h"
#include "arena/arena.h"
#include "arena/ra2stats.h"



static char     stats_path[MAX_OSPATH];

// the counter names, in ra2_stat_t order.  These are the key names the
// GameSpy buckets used, kept so old parsers need only be pointed at the new
// file.
static const char *const stat_names[RA2_NUM_STATS] = {
    "score", "deaths", "suicides",
    "grenadekills", "rocketkills", "railkills", "otherkills"
};

/*
=================
RA2_Stats_Init
=================
*/
void RA2_Stats_Init(void)
{
    FILE    *f;


    stats_path[0] = 0;

    if (!g_statsfile->value || !g_statsname->string[0])
        return;

    // <homedir-or-basedir>/<gamedir>/<name>, not "<gamedir>/<name>": the donor's
    // relative path resolves against the server's working directory, so a
    // server started from anywhere but the installation wrote its stats
    // somewhere else or not at all.
    if (!G_FsGamePath(stats_path, sizeof(stats_path), g_statsname->string)) {
        gi.dprintf("RA2_Stats_Init: stats path too long, logging disabled\n");
        stats_path[0] = 0;
        return;
    }

    f = fopen(stats_path, "a");
    if (!f) {
        gi.dprintf("RA2_Stats_Init: couldn't open %s, logging disabled\n", stats_path);
        stats_path[0] = 0;
        return;
    }

    fprintf(f, "{\"event\":\"init\",\"version\":%d,\"game\":\"%s\",\"time\":%lld}\n",
            RA2_STATS_VERSION, GAMEVERSION, (long long)time(NULL));
    fclose(f);
}

/*
=================
RA2_Stats_Shutdown
=================
*/
void RA2_Stats_Shutdown(void)
{
    FILE    *f;

    if (!stats_path[0])
        return;

    f = fopen(stats_path, "a");
    if (f) {
        fprintf(f, "{\"event\":\"shutdown\",\"time\":%lld}\n", (long long)time(NULL));
        fclose(f);
    }

    stats_path[0] = 0;
}

/*
=================
json_string

Writes a JSON string literal, quotes included.  Player names arrive straight
off the network, so everything outside printable ASCII is escaped -- a raw
control byte or a stray quote would otherwise make the whole line unparseable.
=================
*/
static void json_string(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s; s++) {
        byte c = *s;
        if (c == '"' || c == '\\')
            fprintf(f, "\\%c", c);
        else if (c >= 0x20 && c < 0x7f)
            fputc(c, f);
        else
            fprintf(f, "\\u%04x", c);
    }
    fputc('"', f);
}

/*
=================
RA2_Stats_Begin
=================
*/
ra2_round_t *RA2_Stats_Begin(int arenanum)
{
    ra2_round_t *r;
    const arena_t *a;

    if (!stats_path[0])
        return NULL;

    r = gi.TagMalloc(sizeof(*r), TAG_LEVEL);
    memset(r, 0, sizeof(*r));

    a = &arenas[arenanum];

    r->arena = arenanum;
    r->round = 1;
    r->rounds = a->rounds;
    r->start_framenum = level.framenum;
    Q_strlcpy(r->mapname, level.mapname, sizeof(r->mapname));

    r->armor = a->armor;
    r->health = a->health;
    r->armorprotect = a->armorprotect;
    r->healthprotect = a->healthprotect;
    r->fallingdamage = a->fallingdamage;
    r->compmode = a->competition;
    r->damagescoring = a->scorebydamage;

    return r;
}

/*
=================
RA2_Stats_Write

Appends one line describing the round as it stands.  Called at round end, and
also mid-match where RA2 used to push an interim snapshot to GameSpy.
=================
*/
void RA2_Stats_Write(ra2_round_t *r)
{
    FILE    *f;
    int     i, j;
    bool    first;

    if (!r || !stats_path[0])
        return;

    f = fopen(stats_path, "a");
    if (!f)
        return;

    fprintf(f, "{\"event\":\"round\",\"time\":%lld", (long long)time(NULL));
    fprintf(f, ",\"arena\":%d,\"round\":%d,\"rounds\":%d", r->arena, r->round, r->rounds);
    fprintf(f, ",\"map\":");
    json_string(f, r->mapname);
    fprintf(f, ",\"duration\":%.1f", (level.framenum - r->start_framenum) * FRAMETIME);

    fprintf(f, ",\"settings\":{\"armor\":%d,\"health\":%d,\"armorprotect\":%d"
            ",\"healthprotect\":%d,\"fallingdamage\":%d,\"compmode\":%d"
            ",\"damagescoring\":%d}",
            r->armor, r->health, r->armorprotect, r->healthprotect,
            r->fallingdamage, r->compmode, r->damagescoring);

    fprintf(f, ",\"teams\":[");
    for (i = 0, first = true; i < RA2_STATS_MAX_TEAMS; i++) {
        if (!r->teams[i].inuse)
            continue;
        fprintf(f, "%s{\"id\":%d,\"name\":", first ? "" : ",", r->teams[i].id);
        json_string(f, r->teams[i].name);
        fprintf(f, ",\"score\":%d}", r->teams[i].score);
        first = false;
    }
    fprintf(f, "]");

    fprintf(f, ",\"players\":[");
    for (i = 0, first = true; i < MAX_CLIENTS; i++) {
        const ra2_pstats_t *p = &r->players[i];
        if (!p->inuse)
            continue;
        fprintf(f, "%s{\"slot\":%d,\"name\":", first ? "" : ",", p->slot);
        json_string(f, p->name);
        fprintf(f, ",\"team\":%d,\"ping\":%d", p->team, p->ping);
        for (j = 0; j < RA2_NUM_STATS; j++)
            fprintf(f, ",\"%s\":%d", stat_names[j], p->stat[j]);
        fputc('}', f);
        first = false;
    }
    fprintf(f, "]}\n");

    fclose(f);
}

/*
=================
RA2_Stats_End
=================
*/
void RA2_Stats_End(ra2_round_t *r)
{
    if (!r)
        return;

    RA2_Stats_Write(r);
    gi.TagFree(r);
}

/*
=================
RA2_Stats_NextRound
=================
*/
void RA2_Stats_NextRound(ra2_round_t *r)
{
    if (!r)
        return;

    r->round++;

    // a vote may have changed the arena since the last round started
    RA2_Stats_ArenaInfo(r, r->arena);
}

/*
=================
find_team

Finds the slot holding a global team index, optionally allocating one.  The
table is small and only ever holds the teams fighting in this arena, so a
linear scan is the whole implementation.
=================
*/
static ra2_tstats_t *find_team(ra2_round_t *r, int team, bool alloc)
{
    int     i;

    if (!r || team < 0)
        return NULL;

    for (i = 0; i < RA2_STATS_MAX_TEAMS; i++)
        if (r->teams[i].inuse && r->teams[i].id == team)
            return &r->teams[i];

    if (!alloc)
        return NULL;

    for (i = 0; i < RA2_STATS_MAX_TEAMS; i++)
        if (!r->teams[i].inuse)
            return &r->teams[i];

    return NULL;    // more teams in one arena than anyone plays with
}

/*
=================
RA2_Stats_AddTeam
=================
*/
void RA2_Stats_AddTeam(ra2_round_t *r, int team, const char *name)
{
    ra2_tstats_t *t = find_team(r, team, true);

    if (!t)
        return;

    t->inuse = true;
    t->id = team;
    t->score = 0;
    Q_strlcpy(t->name, name, sizeof(t->name));
}

/*
=================
RA2_Stats_TeamScore
=================
*/
void RA2_Stats_TeamScore(ra2_round_t *r, int team, int delta)
{
    ra2_tstats_t *t = find_team(r, team, false);

    if (t)
        t->score += delta;
}

/*
=================
RA2_Stats_ArenaInfo

Re-snapshots the arena settings.  RA2_Stats_Begin() takes them once, but they
can change between rounds through the vote menu.
=================
*/
void RA2_Stats_ArenaInfo(ra2_round_t *r, int arenanum)
{
    const arena_t *a;

    if (!r)
        return;

    a = &arenas[arenanum];

    r->rounds = a->rounds;
    r->armor = a->armor;
    r->health = a->health;
    r->armorprotect = a->armorprotect;
    r->healthprotect = a->healthprotect;
    r->fallingdamage = a->fallingdamage;
    r->compmode = a->competition;
    r->damagescoring = a->scorebydamage;

    Q_strlcpy(r->mapname, level.mapname, sizeof(r->mapname));
}

/*
=================
slot_stats

Finds the record currently counting for a client slot.  A slot can be handed
to a different player mid-match, so records are retired rather than reused and
the table is a pool rather than an array indexed by slot -- the same thing the
GameSpy code got from allocating a fresh registration slot per player.
=================
*/
static ra2_pstats_t *slot_stats(ra2_round_t *r, int slot)
{
    int     i;

    if (!r || slot < 1)
        return NULL;

    for (i = 0; i < MAX_CLIENTS; i++)
        if (r->players[i].active && r->players[i].slot == slot)
            return &r->players[i];

    return NULL;
}

/*
=================
RA2_Stats_AddPlayer
=================
*/
void RA2_Stats_AddPlayer(ra2_round_t *r, edict_t *ent, int team)
{
    int     slot = ent - g_edicts;
    ra2_pstats_t *p;
    int     i;

    if (!r || slot < 1)
        return;

    // whatever was counting under this slot keeps its numbers
    p = slot_stats(r, slot);
    if (p) {
        p->active = false;
        p->team = -1;
    }

    for (i = 0, p = NULL; i < MAX_CLIENTS; i++)
        if (!r->players[i].inuse) {
            p = &r->players[i];
            break;
        }

    if (!p)
        return;

    memset(p, 0, sizeof(*p));
    p->inuse = true;
    p->active = true;
    p->slot = slot;
    p->team = team;
    p->ping = ent->client->ping;
    Q_strlcpy(p->name, ent->client->pers.netname, sizeof(p->name));
}

/*
=================
RA2_Stats_RemovePlayer

The player's numbers stay in the record -- they earned them -- but they stop
counting towards this round.
=================
*/
void RA2_Stats_RemovePlayer(ra2_round_t *r, int slot)
{
    ra2_pstats_t *p = slot_stats(r, slot);

    if (p) {
        p->active = false;
        p->team = -1;
    }
}

/*
=================
RA2_Stats_Add
=================
*/
void RA2_Stats_Add(ra2_round_t *r, int slot, ra2_stat_t stat, int delta)
{
    ra2_pstats_t *p = slot_stats(r, slot);

    if (p)
        p->stat[stat] += delta;
}

/*
=================
RA2_Stats_Set
=================
*/
void RA2_Stats_Set(ra2_round_t *r, int slot, ra2_stat_t stat, int value)
{
    ra2_pstats_t *p = slot_stats(r, slot);

    if (p)
        p->stat[stat] = value;
}
