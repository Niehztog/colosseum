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

#include "g_local.h"
#include "arena/arena.h"
#include "arena/ra2stats.h"

game_locals_t   game;
level_locals_t  level;
game_import_t   gi;
game_export_t   globals;
spawn_temp_t    st;

int sm_meat_index;
int meansOfDeath;

edict_t     *g_edicts;

cvar_t  *deathmatch;
cvar_t  *coop;
cvar_t  *dmflags;
cvar_t  *skill;
cvar_t  *fraglimit;
cvar_t  *timelimit;
cvar_t  *capturelimit;      // CTF
cvar_t  *instantweap;       // CTF
cvar_t  *password;
cvar_t  *spectator_password;
cvar_t  *needpass;
cvar_t  *maxclients;
cvar_t  *maxspectators;
cvar_t  *maxentities;
cvar_t  *g_select_empty;
cvar_t  *g_protocol_extensions;
cvar_t  *dedicated;

cvar_t  *hostname;
cvar_t  *hostport;

cvar_t  *filterban;

cvar_t  *sv_maxvelocity;
cvar_t  *sv_gravity;

cvar_t  *sv_rollspeed;
cvar_t  *sv_rollangle;
cvar_t  *gun_x;
cvar_t  *gun_y;
cvar_t  *gun_z;

cvar_t  *run_pitch;
cvar_t  *run_roll;
cvar_t  *bob_up;
cvar_t  *bob_pitch;
cvar_t  *bob_roll;

cvar_t  *sv_cheats;

// RA2's two logging cvars (R-RA-1a, R-OSP-10).  `netlog` is the only socket
// user left in the tree after the GameSpy SDK was dropped and defaults to
// empty, so nothing opens one unless a server operator asks.  `logfile` is the
// ENGINE's own console-logging cvar, re-obtained: RA2 gates its stdlog on it
// rather than adding a second switch, which is an idiomatic re-obtain and not
// R-COMPAT-6's collision case.
cvar_t  *logfile;
cvar_t  *netlog;

cvar_t  *flood_msgs;
cvar_t  *flood_persecond;
cvar_t  *flood_waitdelay;

cvar_t  *sv_maplist;

cvar_t  *sv_stopspeed;  //PGM    (this was a define in g_phys.c)

//ROGUE cvars
cvar_t  *g_showlogic;
cvar_t  *gamerules;
cvar_t  *huntercam;
cvar_t  *strong_mines;
cvar_t  *randomrespawn;
//ROGUE

cvar_t  *sv_features;

static void G_RunFrame(void);

//===================================================================

static void ShutdownGame(void)
{
    gi.dprintf("==== ShutdownGame ====\n");

    if (G_Ruleset() == RULESET_ARENA) {
        GSLogShutdown();
        RA2_Stats_Shutdown();
#ifdef _WIN32
        GSNetShutdown();
#endif
    }

    memset(&game, 0, sizeof(game));

    gi.FreeTags(TAG_LEVEL);
    gi.FreeTags(TAG_GAME);
}

/*
============
InitGame

This will be called when the dll is first loaded, which
only happens when a new game is started or a save game
is loaded.
============
*/
static void InitGame(void)
{
    int features = G_FEATURES;

    gi.dprintf("==== InitGame %s====\n", GAMEVERSION);

    Q_srand(time(NULL));

    gun_x = gi.cvar("gun_x", "0", 0);
    gun_y = gi.cvar("gun_y", "0", 0);
    gun_z = gi.cvar("gun_z", "0", 0);

    //FIXME: sv_ prefix is wrong for these
    sv_rollspeed = gi.cvar("sv_rollspeed", "200", 0);
    sv_rollangle = gi.cvar("sv_rollangle", "2", 0);
    sv_maxvelocity = gi.cvar("sv_maxvelocity", "2000", 0);
    sv_gravity = gi.cvar("sv_gravity", "800", 0);
    sv_stopspeed = gi.cvar("sv_stopspeed", "100", 0);         // PGM - was #define in g_phys.c

//ROGUE
    g_showlogic = gi.cvar("g_showlogic", "0", 0);
    huntercam = gi.cvar("huntercam", "1", CVAR_SERVERINFO | CVAR_LATCH);
    strong_mines = gi.cvar("strong_mines", "0", 0);
    randomrespawn = gi.cvar("randomrespawn", "0", 0);
//ROGUE

    // noset vars
    dedicated = gi.cvar("dedicated", "0", CVAR_NOSET);

    // latched vars
    sv_cheats = gi.cvar("cheats", "0", CVAR_SERVERINFO | CVAR_LATCH);

    logfile = gi.cvar("logfile", "0", CVAR_SERVERINFO);
    netlog = gi.cvar("netlog", "", CVAR_SERVERINFO);
    gi.cvar("gamename", GAMEVERSION, CVAR_SERVERINFO | CVAR_LATCH);
    gi.cvar("gamedate", __DATE__, CVAR_SERVERINFO | CVAR_LATCH);

    maxclients = gi.cvar("maxclients", "4", CVAR_SERVERINFO | CVAR_LATCH);
    maxspectators = gi.cvar("maxspectators", "4", CVAR_SERVERINFO);
    deathmatch = gi.cvar("deathmatch", "0", CVAR_LATCH);
    coop = gi.cvar("coop", "0", CVAR_LATCH);
    skill = gi.cvar("skill", "1", CVAR_LATCH);
    maxentities = gi.cvar("maxentities", "1024", CVAR_LATCH);
    gamerules = gi.cvar("gamerules", "0", CVAR_LATCH);             //PGM

    // change anytime vars
    dmflags = gi.cvar("dmflags", "0", CVAR_SERVERINFO);
    fraglimit = gi.cvar("fraglimit", "0", CVAR_SERVERINFO);
    timelimit = gi.cvar("timelimit", "0", CVAR_SERVERINFO);
    password = gi.cvar("password", "", CVAR_USERINFO);
    spectator_password = gi.cvar("spectator_password", "", CVAR_USERINFO);
    needpass = gi.cvar("needpass", "0", CVAR_SERVERINFO);
    filterban = gi.cvar("filterban", "1", 0);

    g_select_empty = gi.cvar("g_select_empty", "0", CVAR_ARCHIVE);
    g_protocol_extensions = gi.cvar("g_protocol_extensions", "0", CVAR_LATCH);

    run_pitch = gi.cvar("run_pitch", "0.002", 0);
    run_roll = gi.cvar("run_roll", "0.005", 0);
    bob_up  = gi.cvar("bob_up", "0.005", 0);
    bob_pitch = gi.cvar("bob_pitch", "0.002", 0);
    bob_roll = gi.cvar("bob_roll", "0.002", 0);

    // flood control
    flood_msgs = gi.cvar("flood_msgs", "4", 0);
    flood_persecond = gi.cvar("flood_persecond", "4", 0);
    flood_waitdelay = gi.cvar("flood_waitdelay", "10", 0);

    // dm map list
    sv_maplist = gi.cvar("sv_maplist", "", 0);

    // obtain server features
    sv_features = gi.cvar("sv_features", NULL, 0);

    // enable protocol extensions if supported
    if (sv_features && (int)sv_features->value & GMF_PROTOCOL_EXTENSIONS && (int)g_protocol_extensions->value) {
        features |= GMF_PROTOCOL_EXTENSIONS;
        game.csr = cs_remap_new;
    } else {
        game.csr = cs_remap_old;
    }

    // export our own features
    gi.cvar_forceset("g_features", va("%d", features));

    // items
    // R-MODE-1/2/3/4.  Placed here, and the ordering is load-bearing in both
    // directions: *after* the cvar block above, because resolution reads the
    // `deathmatch` and `coop` globals that block assigns and they are NULL until
    // it runs; and *before* everything below, because reconcile_legacy_cvars()
    // has to settle `deathmatch` before any of the 138 inherited baseq2 sites
    // that test it can run, and because every predicate in g_ruleset.h reads the
    // result.  Calling it at the top of InitGame -- which reads naturally and is
    // what the first draft did -- dereferences a NULL cvar_t and segfaults on
    // the first map load.
    G_InitRuleset();

    // RA2's two writers (R-RA-1a, R-OSP-10).  After resolution, because both
    // are per-ruleset and only one owner may be live at a time; `hostname` and
    // `port` are the engine's own cvars, re-obtained for the log header.
    if (G_Ruleset() == RULESET_ARENA) {
        cvar_t *publicserver;       // RA2 called this local `public`

        hostname = gi.cvar("hostname", "", CVAR_SERVERINFO);
        hostport = gi.cvar("port", "27910", CVAR_SERVERINFO | CVAR_NOSET);

        publicserver = gi.cvar("public", "1", 0);
        if (publicserver->value == 0)
            gi.cvar_set("netlog", "");

        GSLogStartup();
        RA2_Stats_Init();
    }

    InitItems();

//======
//ROGUE
    if (gamerules) {
        InitGameRules(); // if there are game rules to set up, do so now.
    }
//ROGUE
//======

    game.helpmessage1[0] = 0;
    game.helpmessage2[0] = 0;

    // Threewave's own init.  NO DONOR TREE CALLS THIS -- not port_ctf, not
    // q2pro/src/ctf -- so every CTF cvar pointer stays null and SpawnEntities'
    // CTFSpawn() dereferences `competition->value` on the first map load, before
    // a player can connect.  Found by running it (doc/reconciliation.md R-42).
    //
    // Called unconditionally rather than under `ctf`, because CTF's flag and
    // tech drop paths are reached from shared code -- player_die and
    // ClientDisconnect -- in every ruleset, and they need flag1_item resolved.
    CTFInit();

    // initialize all entities for this game
    //
    // `maxclients->value`, not `game.maxclients`.  Both donor branches carry
    // `game.maxclients = game.maxclients;` -- a self-assignment -- and the
    // mission-pack merge propagated it here in Phase 2, where nothing could
    // catch it: on a dedicated server with no clients connecting, maxclients 0
    // is invisible.  See doc/reconciliation.md R-47.
    game.maxentities = Q_clip(maxentities->value, (int)maxclients->value + 1, game.csr.max_edicts);
    g_edicts = gi.TagMalloc(game.maxentities * sizeof(g_edicts[0]), TAG_GAME);
    globals.edicts = g_edicts;
    globals.max_edicts = game.maxentities;

    // initialize all clients for this game
    game.maxclients = maxclients->value;
    game.clients = gi.TagMalloc(game.maxclients * sizeof(game.clients[0]), TAG_GAME);

    if (G_Ruleset() == RULESET_ARENA) {
        // RA2 pre-initialises every client slot, because its menus and its
        // team lists are walked for slots nobody has connected to yet.
        for (int i = 0; i < game.maxclients; i++) {
            InitClientPersistant(&game.clients[i], true);
            InitClientResp(&game.clients[i]);
            game.clients[i].resp.entered = false;
        }

#ifdef _WIN32
        if (!GSNetStartup())
            gi.cvar_set("netlog", "");
#endif
    }

    globals.num_edicts = game.maxclients + 1;
}

/*
=================
GetGameAPI

Returns a pointer to the structure with all entry points
and global variables
=================
*/
q_exported game_export_t *GetGameAPI(game_import_t *import)
{
    gi = *import;

    globals.apiversion = GAME_API_VERSION;
    globals.Init = InitGame;
    globals.Shutdown = ShutdownGame;
    globals.SpawnEntities = SpawnEntities;

    globals.WriteGame = WriteGame;
    globals.ReadGame = ReadGame;
    globals.WriteLevel = WriteLevel;
    globals.ReadLevel = ReadLevel;

    globals.ClientThink = ClientThink;
    globals.ClientConnect = ClientConnect;
    globals.ClientUserinfoChanged = ClientUserinfoChanged;
    globals.ClientDisconnect = ClientDisconnect;
    globals.ClientBegin = ClientBegin;
    globals.ClientCommand = ClientCommand;

    globals.RunFrame = G_RunFrame;

    globals.ServerCommand = ServerCommand;

    globals.edict_size = sizeof(edict_t);

    return &globals;
}

#ifndef GAME_HARD_LINKED
// this is only here so the functions in q_shared.c can link
void Com_LPrintf(print_type_t type, const char *fmt, ...)
{
    va_list     argptr;
    char        text[MAX_STRING_CHARS];

    if (type == PRINT_DEVELOPER) {
        return;
    }

    va_start(argptr, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, argptr);
    va_end(argptr);

    gi.dprintf("%s", text);
}

void Com_Error(error_type_t type, const char *fmt, ...)
{
    va_list     argptr;
    char        text[MAX_STRING_CHARS];

    va_start(argptr, fmt);
    Q_vsnprintf(text, sizeof(text), fmt, argptr);
    va_end(argptr);

    gi.error("%s", text);
}
#endif

//======================================================================

/*
=================
ClientEndServerFrames
=================
*/
static void ClientEndServerFrames(void)
{
    int     i;
    edict_t *ent;

    // calc the player views now that all pushing
    // and damage has been added
    for (i = 0; i < game.maxclients; i++) {
        ent = g_edicts + 1 + i;
        if (!ent->inuse || !ent->client)
            continue;
        ClientEndServerFrame(ent);
    }

}

/*
=================
CreateTargetChangeLevel

Returns the created target changelevel
=================
*/
edict_t *CreateTargetChangeLevel(char *map)
{
    edict_t *ent;

    ent = G_Spawn();
    ent->classname = "target_changelevel";
    if (map != level.nextmap)
        Q_strlcpy(level.nextmap, map, sizeof(level.nextmap));
    ent->map = level.nextmap;
    return ent;
}

/*
=================
EndDMLevel

The timelimit or fraglimit has been exceeded
=================
*/
void EndDMLevel(void)
{
    edict_t     *ent;
    char *s, *t, *f;
    static const char *seps = " ,\n\r";

    // stay on same level flag
    if ((int)dmflags->value & DF_SAME_LEVEL) {
        BeginIntermission(CreateTargetChangeLevel(level.mapname));
        return;
    }

    // see if it's in the map list
    // CTF's `warp` vote and admin menu override the rotation for exactly one
    // level change.  Empty in every other ruleset, so no gate.
    if (*level.forcemap) {
        BeginIntermission(CreateTargetChangeLevel(level.forcemap));
        return;
    }

    if (*sv_maplist->string) {
        s = strdup(sv_maplist->string);
        f = NULL;
        t = strtok(s, seps);
        while (t != NULL) {
            if (Q_stricmp(t, level.mapname) == 0) {
                // it's in the list, go to the next one
                t = strtok(NULL, seps);
                if (t == NULL) { // end of list, go to first one
                    if (f == NULL) // there isn't a first one, same level
                        BeginIntermission(CreateTargetChangeLevel(level.mapname));
                    else
                        BeginIntermission(CreateTargetChangeLevel(f));
                } else
                    BeginIntermission(CreateTargetChangeLevel(t));
                free(s);
                return;
            }
            if (!f)
                f = t;
            t = strtok(NULL, seps);
        }
        free(s);
    }

    if (level.nextmap[0]) // go to a specific map
        BeginIntermission(CreateTargetChangeLevel(level.nextmap));
    else {  // search for a changelevel
        ent = G_Find(NULL, FOFS(classname), "target_changelevel");
        if (!ent) {
            // the map designer didn't include a changelevel,
            // so create a fake ent that goes back to the same level
            BeginIntermission(CreateTargetChangeLevel(level.mapname));
            return;
        }
        BeginIntermission(ent);
    }
}

/*
=================

CheckDMRules
=================
*/
void CheckDMRules(void)
{
    int         i;
    gclient_t   *cl;

    if (level.intermission_framenum)
        return;

    if (!deathmatch->value)
        return;

//=======
//ROGUE
    if (gamerules && gamerules->value && DMGame.CheckDMRules) {
        if (DMGame.CheckDMRules())
            return;
    }
//ROGUE
//=======

    if (timelimit->value) {
        if (level.time >= timelimit->value * 60) {
            gi.bprintf(PRINT_HIGH, "Timelimit hit.\n");
            EndDMLevel();
            return;
        }
    }

    if (fraglimit->value) {
        for (i = 0; i < game.maxclients; i++) {
            cl = game.clients + i;
            if (!g_edicts[i + 1].inuse)
                continue;

            if (cl->resp.score >= fraglimit->value) {
                gi.bprintf(PRINT_HIGH, "Fraglimit hit.\n");
                EndDMLevel();
                return;
            }
        }
    }
}

/*
=============
ExitLevel
=============
*/
static void ExitLevel(void)
{
    int     i;
    edict_t *ent;
    char    command[256];

    // The two clears move ahead of CTFNextMap because it starts the next map
    // itself when a match is running, and it must not leave the intermission
    // flags set behind it.  Harmless for dm: nothing below reads either.
    level.exitintermission = 0;
    level.intermission_framenum = 0;

    // CTF's match system owns the level change during a match.  False in every
    // other ruleset.
    if (CTFNextMap())
        return;

    Q_snprintf(command, sizeof(command), "gamemap \"%s\"\n", level.changemap);
    if (G_Ruleset() != RULESET_ARENA)
        gi.AddCommandString(command);
    level.changemap = NULL;

    // clear some things before going to next level
    for (i = 0; i < game.maxclients; i++) {
        ent = g_edicts + 1 + i;
        if (!ent->inuse)
            continue;
        if (ent->health > ent->client->pers.max_health)
            ent->health = ent->client->pers.max_health;
        if (G_Ruleset() == RULESET_ARENA)
            InitClientResp(ent->client);
    }

    if (G_Ruleset() == RULESET_ARENA)
        gi.AddCommandString(command);
}

/*
================
G_RunFrame

Advances the world by 0.1 seconds
================
*/
static void G_RunFrame(void)
{
    int     i;
    edict_t *ent;

    level.framenum++;
    level.time = level.framenum * FRAMETIME;

    // choose a client for monsters to target this frame
    AI_SetSightClient();

    // exit intermissions
    if (level.exitintermission) {
        ExitLevel();
        return;
    }

    //
    // treat each object in turn
    // even the world gets a chance to think
    //
    ent = &g_edicts[0];
    for (i = 0; i < globals.num_edicts; i++, ent++) {
        if (!ent->inuse)
            continue;

        level.current_entity = ent;

        if (!(ent->s.renderfx & RF_BEAM))
            VectorCopy(ent->s.origin, ent->s.old_origin);

        // if the ground entity moved, make sure we are still on it
        if ((ent->groundentity) && (ent->groundentity->linkcount != ent->groundentity_linkcount)) {
            ent->groundentity = NULL;
            if (!(ent->flags & (FL_SWIM | FL_FLY)) && (ent->svflags & SVF_MONSTER)) {
                M_CheckGround(ent);
            }
        }

        if (i > 0 && i <= game.maxclients) {
            ClientBeginServerFrame(ent);
            continue;
        }

        G_RunEntity(ent);
    }

    // exit intermission right now to avoid annoying fov change
    if (level.exitintermission) {
        ExitLevel();
        return;
    }

    // see if it is time to end a deathmatch
    // R-MODE-5: the match-rules gate.  dm checks fraglimit and timelimit, ctf
    // adds capturelimit, arena runs its round state machine, tourney its match
    // system, and sp has no match to end.  NULL inherits dm (R-MODE-6).
    //
    // Ground Zero calls CheckDMRules() directly here.  Phase 1's seam wins:
    // Rogue's own DM rulesets (dm_ball, dm_tag, driven by `gamerules`) are a
    // dispatch row, not a second direct call.
    G_CheckRules();

    // build the playerstate_t structures for all players
    ClientEndServerFrames();
}
