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
//===========================================================================
//
// Name:        p_lag.c
// Function:    client lag simulation (R-EXTRA-2)
// Programmer:  Mr Elusive (MrElusive@demigod.demon.nl), 1999-04-02
//
// From gladiator-bot-restored/game/p_lag.c, behind `#define CLIENTLAG`.
//
// WHAT IT SIMULATES, because the name misleads.  It does not delay movement:
// pmove still runs on the command that just arrived, so a lagged player walks
// normally.  What it delays is the BUTTON half -- the weapon fires from the
// position and view angles the player had `delay` milliseconds ago, which is
// what an unlagged server sees a lagged client do.  `Lag_StoreClientInput`
// records origin and v_angle with the command; `Lag_GetClientInput` puts both
// back before the caller replays it, and the caller restores the live pair
// afterwards.
//
// Four changes from the donor:
//
//   * a cvar, `g_clientlag`, and off by default.  The `lag` command is a
//     CLIENT command in the donor, so any player could ask the server to hold
//     two seconds of their input; with maxclients at 256 that is a queue the
//     client controls the length of.  Off, nothing is allocated at all.
//   * the pool is capped, and says so once.  The donor extended it forever.
//   * `Lag_ForgetGameMemory()`, because the pool is TAG_GAME and `ReadGame`
//     frees every TAG_GAME block in the library before re-establishing the two
//     arrays it knows about (doc/reconciliation.md R-108).  A third owner that
//     does not forget its pointers hands the next allocation a freed block.
//   * `gi.error()` on an out-of-range client index becomes a return.  A game
//     library that kills the server on a bad index is R-SEC-5's shape: the
//     index cannot be out of range here, and if it ever is, one dead client is
//     better than one dead server.
//===========================================================================

#include "g_local.h"

#define LAGHEAPSIZE     100

// The pool ceiling.  A client asking for the maximum 2000 ms at a 125 Hz
// command rate holds about 250 commands; this is that, rounded up, times the
// client array -- enough that the cap is a backstop rather than a limit
// anybody meets.
#define LAGMAXBLOCKS    (4 * MAX_CLIENTS)

//delayed ucmd struct
typedef struct delayeducmd_s {
    usercmd_t   ucmd;
    vec3_t      origin, v_angle;
    int         sec, msec;
    struct delayeducmd_s *next;
} delayeducmd_t;

//heap with delayed ucmd structs
typedef struct delayeducmdheap_s {
    struct delayeducmdheap_s *next;
} delayeducmdheap_t;

typedef struct clientlag_s {
    delayeducmd_t *firstucmd;   //first ucmd to process by ClientThink
    delayeducmd_t *lastucmd;    //last ucmd to process
    int lag, lagvariance;       //the lag and the lag variance
    int delay;                  //lag in milli seconds
    int sec, msec;              //absolute client time
} clientlag_t;

static clientlag_t          clientlag[MAX_CLIENTS];
static delayeducmdheap_t    *heap;
static delayeducmd_t        *freedelayeducmds;
static int                  numblocks;
static bool                 saidfull;

static void Lag_FreeDelayeducmd(delayeducmd_t *ducmd)
{
    ducmd->next = freedelayeducmds;
    freedelayeducmds = ducmd;
}

// R-108's pair.  ReadGame's `gi.FreeTags(TAG_GAME)` invalidates every block
// this file owns and every queue pointer into them, so both go before the
// free rather than being discovered afterwards.  There is no matching Setup:
// the pool re-extends on the next command, which is the next frame.
void Lag_ForgetGameMemory(void)
{
    heap = NULL;
    freedelayeducmds = NULL;
    numblocks = 0;
    saidfull = false;
    memset(clientlag, 0, sizeof(clientlag));
}

static bool Lag_ExtendDelayeducmdHeap(void)
{
    delayeducmdheap_t *h;
    char *ptr;
    int i;

    if (numblocks >= LAGMAXBLOCKS) {
        if (!saidfull) {
            gi.dprintf("WARNING: the client-lag pool is full at %d commands; "
                       "further input is not delayed (R-EXTRA-2)\n",
                       numblocks * LAGHEAPSIZE);
            saidfull = true;
        }
        return false;
    }

    h = gi.TagMalloc(sizeof(delayeducmdheap_t) + LAGHEAPSIZE * sizeof(delayeducmd_t), TAG_GAME);
    ptr = (char *)h + sizeof(delayeducmdheap_t);
    for (i = 0; i < LAGHEAPSIZE; i++) {
        Lag_FreeDelayeducmd((delayeducmd_t *)ptr);
        ptr += sizeof(delayeducmd_t);
    }
    h->next = heap;
    heap = h;
    numblocks++;
    return true;
}

static delayeducmd_t *Lag_AllocDelayeducmd(void)
{
    delayeducmd_t *ducmd;

    if (!freedelayeducmds && !Lag_ExtendDelayeducmdHeap())
        return NULL;

    ducmd = freedelayeducmds;
    freedelayeducmds = freedelayeducmds->next;
    return ducmd;
}

static int Lag_MilliSecondsSubtract(int seca, int mseca, int secb, int msecb)
{
    int msec;

    msec = (seca - secb) * 1000;
    msec += mseca;
    msec -= msecb;
    return msec;
}

// The index every entry point derives.  Out of range is impossible from the
// engine and is not worth a dead server if it ever happens (R-SEC-4).
static int Lag_ClientIndex(edict_t *ent)
{
    // Not DF_ENTCLIENT: that macro is the bot layer's own and this file is not
    // part of it.  Same arithmetic, spelled where it is used.
    int client = (int)(ent - g_edicts - 1);

    if (client < 0 || client >= game.maxclients || client >= MAX_CLIENTS)
        return -1;
    return client;
}

void Lag_BeginGame(edict_t *ent)
{
    delayeducmd_t *firstucmd;
    int client = Lag_ClientIndex(ent);

    if (client < 0)
        return;

    while (clientlag[client].firstucmd) {
        firstucmd = clientlag[client].firstucmd;
        clientlag[client].firstucmd = clientlag[client].firstucmd->next;
        if (!clientlag[client].firstucmd)
            clientlag[client].lastucmd = NULL;
        Lag_FreeDelayeducmd(firstucmd);
    }
    memset(&clientlag[client], 0, sizeof(clientlag_t));
}

void Lag_StoreClientInput(edict_t *ent, usercmd_t *ucmd, vec3_t origin, vec3_t v_angle)
{
    delayeducmd_t *ducmd;
    int client = Lag_ClientIndex(ent);

    if (client < 0)
        return;

    //update the absolute client time
    clientlag[client].msec += ucmd->msec;
    while (clientlag[client].msec > 1000) {
        clientlag[client].msec -= 1000;
        clientlag[client].sec++;
        //the lag fluctuates every second
        clientlag[client].delay = clientlag[client].lag +
                                  crandom() * clientlag[client].lagvariance;
        //
        if (clientlag[client].delay < 0)
            clientlag[client].delay = 0;
        else if (clientlag[client].delay > LAG_MAX_DELAY)
            clientlag[client].delay = LAG_MAX_DELAY;
        //set the client ping.  AFTER the clamp: the donor set it from the
        //unclamped value, so a big variance showed a ping the simulation was
        //not actually applying.
        ent->client->ping = clientlag[client].delay;
    }

    ducmd = Lag_AllocDelayeducmd();
    if (!ducmd)
        return;

    //copy the ucmd
    memcpy(&ducmd->ucmd, ucmd, sizeof(usercmd_t));
    VectorCopy(origin, ducmd->origin);
    VectorCopy(v_angle, ducmd->v_angle);
    ducmd->sec = clientlag[client].sec;
    ducmd->msec = clientlag[client].msec;
    ducmd->next = NULL;
    //add the ucmd to the delayed list for this client
    if (clientlag[client].lastucmd)
        clientlag[client].lastucmd->next = ducmd;
    else
        clientlag[client].firstucmd = ducmd;
    clientlag[client].lastucmd = ducmd;
}

bool Lag_GetClientInput(edict_t *ent, usercmd_t *laggeducmd, vec3_t origin, vec3_t v_angle)
{
    delayeducmd_t *firstucmd;
    int client = Lag_ClientIndex(ent);
    int msec;

    if (client < 0)
        return false;

    //first delayed ucmd that might be processed
    firstucmd = clientlag[client].firstucmd;
    if (!firstucmd)
        return false;

    //if the first ucmd isn't delayed enough
    msec = Lag_MilliSecondsSubtract(clientlag[client].sec, clientlag[client].msec,
                                    firstucmd->sec, firstucmd->msec);
    if (msec < clientlag[client].delay - 1)
        return false;

    memcpy(laggeducmd, &firstucmd->ucmd, sizeof(usercmd_t));
    VectorCopy(firstucmd->origin, origin);
    VectorCopy(firstucmd->v_angle, v_angle);
    //remove the ducmd from the client lag and free it
    clientlag[client].firstucmd = clientlag[client].firstucmd->next;
    if (!clientlag[client].firstucmd)
        clientlag[client].lastucmd = NULL;
    Lag_FreeDelayeducmd(firstucmd);
    return true;
}

// v0.93's rankings-screen adjustment: the scoreboard shows the simulated ping
// when it is worse than the real one, so a lagged player reads as lagged.
void Lag_SetClientPing(edict_t *ent)
{
    int client = Lag_ClientIndex(ent);

    if (client < 0)
        return;
    if (clientlag[client].delay > ent->client->ping)
        ent->client->ping = clientlag[client].delay;
}

void Lag_SetClientLag(edict_t *ent, int lag)
{
    int client = Lag_ClientIndex(ent);

    if (client < 0)
        return;
    if (!g_clientlag->value) {
        gi.cprintf(ent, PRINT_HIGH, "lag simulation is off on this server\n");
        return;
    }
    if (lag < 0)
        lag = 0;
    if (lag > LAG_MAX_DELAY)
        lag = LAG_MAX_DELAY;
    //set the client lag
    clientlag[client].lag = lag;
    clientlag[client].delay = lag;

    gi.cprintf(ent, PRINT_HIGH, "lag set to %d\n", lag);
}

void Lag_SetClientLagVariance(edict_t *ent, int lagvariance)
{
    int client = Lag_ClientIndex(ent);

    if (client < 0)
        return;
    if (!g_clientlag->value) {
        gi.cprintf(ent, PRINT_HIGH, "lag simulation is off on this server\n");
        return;
    }
    if (lagvariance < 0)
        lagvariance = 0;
    if (lagvariance > LAG_MAX_DELAY)
        lagvariance = LAG_MAX_DELAY;
    //set the client lag variance
    clientlag[client].lagvariance = lagvariance;

    gi.cprintf(ent, PRINT_HIGH, "lag variance set to %d\n", lagvariance);
}

// For `sv extras` (R-VER-33).
int Lag_PoolBlocks(void)
{
    return numblocks;
}

void Lag_ClientState(edict_t *ent, int *lag, int *variance, int *delay, int *queued)
{
    int client = Lag_ClientIndex(ent);
    delayeducmd_t *d;
    int n = 0;

    *lag = *variance = *delay = *queued = 0;
    if (client < 0)
        return;
    *lag = clientlag[client].lag;
    *variance = clientlag[client].lagvariance;
    *delay = clientlag[client].delay;
    for (d = clientlag[client].firstucmd; d; d = d->next)
        n++;
    *queued = n;
}
