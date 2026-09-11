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
// Fake clients, from osp-tourney@1d8427e.
//===========================================================================
//
// Name:                bl_spawn.c
// Function:        spawning of bots
// Programmer:      Mr Elusive (MrElusive@demigod.demon.nl)
// Last update: 1999-02-10
// Tab Size:        3
//===========================================================================

#include "g_local.h"
#include "bot/bl_main.h"
#include "bot/bl_spawn.h"
#include "bot/bl_redirgi.h"
#include "bot/bl_botcfg.h"
#include "bot/p_menulib.h"
#include "arena/arena.h"
// CTF's half of the fill: the seat count and the balanced removal are CTF's
// own knowledge, the same way arena.h's are arena's.
#include "ctf/g_ctf.h"
// ...and the half for `tdm` and `duel` is the capacity OSP declares.
#include "tourney/osp_hooks.h"

// `old_botcount` is the mod's own, not the SDK's: osp_teams.c compares it
// against botglobals.numbots to notice a bot joining or leaving, and
// CheckMinimumPlayers uses it to avoid asking for the same bot twice.  The
// donor initialises it to -1 in bl_spawn.c and to 0 in its g_main.c, which is
// two definitions of one object; -1 is the one CheckMinimumPlayers needs,
// because 0 is a legal bot count.
int old_botcount = -1;

// The roster-exhausted brake for `botfill`, in every ruleset.  See
// BotFillNoMore(); arena keeps its own in arena.c, because its target is per
// arena and so is the count it settled for.
static int botfill_ceiling;

typedef struct queuedbot_s
{
    int count;
    char library[BOT_MAX_PATH];
    char userinfo[MAX_INFO_STRING];
    edict_t *ent;
    struct queuedbot_s *next;
} queuedbot_t;

static queuedbot_t *queuedbots;

//===========================================================================
// spawns a client entity, initializes the edict and sets the pointer
// to the gclient_t structure
//
// The search runs game.maxclients-1 -> 0, i.e. DOWNWARD, so bots take
// high slots and humans take low ones.  That is not cosmetic: a human
// connecting to a full server relocates a bot, and the relocation
// only ever has somewhere to go if the bots are packed at the top.
//
// Parameter:               -
// Returns:                 the spawned free client edict
// Changes Globals:     -
//===========================================================================
static edict_t *G_SpawnClient(void)
{
    int i;
    edict_t *cl_ent;

    for (i = game.maxclients - 1; i >= 0; i--)
    {
        cl_ent = DF_CLIENTENT(i);
        if (!cl_ent->inuse)
        {
            memset(cl_ent, 0, sizeof(*cl_ent));
            G_InitEdict(cl_ent);
            cl_ent->client = &game.clients[i];
            return cl_ent;
        } //end if
    } //end for
    return NULL;
} //end of the function G_SpawnClient
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void G_FreeClientEdict(edict_t *ent)
{
    ent->s.modelindex = 0;
    ent->solid = SOLID_NOT;
    ent->svflags &= ~SVF_BOT;
    ent->inuse = false;
    ent->classname = "disconnected";
    ent->client->pers.connected = false;
} //end of the function G_FreeClientEdict
//===========================================================================
// The engine and MVD spectators can tell a bot from a player only if
// the game says so, and only the extended protocol has anywhere to say it.
//===========================================================================
static void BotSetSvFlags(edict_t *ent)
{
    if (game.csr.extended)
        ent->svflags |= SVF_BOT;
}
//===========================================================================
// the entity will become a bot
//
// Parameter:               lib             : library the bot will use
// Returns:                 -
// Changes Globals:     botglobals.states
//                              botglobals.numbots
//===========================================================================
static void BotBecome(edict_t *ent, bot_library_t *lib)
{
    bot_state_t *bs;

    //remove bot flag before calling ClientBegin to make sure
    //BotMoveToFreeClientEdict won't be called there
    ent->flags &= ~FL_BOT;
    //begin
    ClientBegin(ent);
    //set the bot flag
    ent->flags |= FL_BOT | FL_BOTCLIENT;
    BotSetSvFlags(ent);
    //get pointer to the bot state structure
    bs = &botglobals.botstates[DF_ENTCLIENT(ent)];
    //clear the bot state
    memset(bs, 0, sizeof(bot_state_t));
    //set botstate active flag
    bs->active = true;
    //pointer to the library used by the bot
    bs->library = lib;
    //one extra bot
    botglobals.numbots++;
    //setup the bot client in the library
    BotLib_BotSetupClient(ent, ent->client->pers.userinfo);
} //end of the function BotBecome
//===========================================================================
// destroy the given bot
//
// The donor's order is corrected here.  It cleared the edict and the gclient_t
// First and released the library reference afterwards, out of `bs` -- which
// still holds the pointer, so it works, but every step between the memset and
// the release is running against a client structure that has already been
// zeroed.  The order here is the requirement's: shut the brain's client down,
// disconnect, decrement, release the library, then clear.
//
// Parameter:               bot             : bot to destroy
// Returns:                 -
// Changes Globals:     botglobals.botstates
//                              botglobals.numbots
//===========================================================================
void BotDestroy(edict_t *bot)
{
    bot_state_t *bs;
    bot_library_t *lib;

    if (!(bot->flags & FL_BOT)) return;
    if (!bot->client) return;
    //shutdown the bot client in the library
    BotLib_BotShutdownClient(bot);
    //remove bot flag before disconnecting to prevent printing messages to
    //the bot library
    bot->flags &= ~FL_BOT;
    //disconnect the client
    ClientDisconnect(bot);
    bot->flags &= ~FL_BOTCLIENT;
    //pointer to the bot state
    bs = &botglobals.botstates[DF_ENTCLIENT(bot)];
    //remove botstate active flag
    bs->active = false;
    bs->started = false;
    //there is a bot less
    botglobals.numbots--;
    //free the library used by the bot, and forget it before the clear
    lib = bs->library;
    bs->library = NULL;
    BotFreeLibrary(lib);
    //clear the entity
    memset(bot, 0, sizeof(*bot));
    //pointer to gclient_t structure
    bot->client = &game.clients[DF_ENTCLIENT(bot)];
    //clear the gclient_t structure
    memset(bot->client, 0, sizeof(gclient_t));
    //free up the client edict
    G_FreeClientEdict(bot);
} //end of the function BotDestroy
//===========================================================================
// Every bot, in one call.  ShutdownGame needs it -- a level change that does
// not go through here leaks the brain's per-client state -- and so does
// `sv removebot all`.
//===========================================================================
void BotDestroyAll(void)
{
    int i;
    edict_t *cl_ent;

    if (!botglobals.botstates) return;
    for (i = 0; i < game.maxclients; i++)
    {
        cl_ent = DF_CLIENTENT(i);
        if (!cl_ent->inuse) continue;
        if (!(cl_ent->flags & FL_BOT)) continue;
        BotDestroy(cl_ent);
    } //end for
} //end of the function BotDestroyAll
//===========================================================================
// create a bot with the given user info
//
// Parameter:               userinfo            : userinfo for the bot to create
//                              lib             : library the bot will use
// Returns:                 created bot
// Changes Globals:     botglobals.states
//                              botglobals.numbots
//===========================================================================
static edict_t *BotCreate(char *userinfo, bot_library_t *lib)
{
    edict_t *ent;
    bot_state_t *bs;
    int arena;

    //spawn a client entity
    ent = G_SpawnClient();
    //check if there was a free client entity
    if (!ent) return NULL;
    //remove bot flag before calling ClientConnect to make sure
    //BotMoveToFreeClientEdict won't be called there
    ent->flags &= ~FL_BOT;
    ent->flags |= FL_BOTCLIENT;
    //connect the client
    //NOTE: set entity inuse flag to false because the bot isn't spawned
    //          from a savegame
    ent->inuse = false;
    if (!ClientConnect(ent, userinfo))
    {
        //free the client edict
        G_FreeClientEdict(ent);
        return NULL;
    } //end if
    //set the inuse flag after connecting
    ent->inuse = true;
    //set the bot flag
    ent->flags |= FL_BOT;
    BotSetSvFlags(ent);
    //get pointer to the bot state structure
    bs = &botglobals.botstates[DF_ENTCLIENT(ent)];
    //clear the bot state
    memset(bs, 0, sizeof(bot_state_t));
    //set botstate active flag
    bs->active = true;
    //pointer to the library used by the bot
    bs->library = lib;
    //setup the bot client in the library
    //NOTE: call after the bs->library pointer is set
    if (!BotLib_BotSetupClient(ent, userinfo))
    {
        //remove botstate active flag
        bs->active = false;
        //remove library pointer
        bs->library = NULL;
        //clear the entity
        memset(ent, 0, sizeof(*ent));
        //pointer to gclient_t structure
        ent->client = &game.clients[DF_ENTCLIENT(ent)];
        //clear the gclient_t structure
        memset(ent->client, 0, sizeof(gclient_t));
        //free the client edict
        G_FreeClientEdict(ent);
        return NULL;
    } //end if
    //
    if (G_Ruleset() == RULESET_ARENA)
    {
        // `resp.context` is the arena number, which RA2's own code reads.  The
        // donor also clears `client->ra_time` here; no RA2 tree in this
        // workspace has that field.  It belongs to GLADIATOR's own RA2 support
        // -- its own g_arena.c -- which this tree does not carry,
        // and the donor's #ifdef ROCKETARENA block was never compiled anywhere.
        //
        // The key is text out of bots.cfg and `resp.context` indexes
        // arenas[MAX_ARENAS] from a dozen places -- give_ammo() takes
        // &arenas[context] before it looks at anything.  `arena 999` in a bot
        // file was an out-of-bounds read and write with no diagnostic, so the
        // value is bounded where it enters: 0 means "no arena yet", which is
        // what a human gets, and anything outside 1..num_arenas becomes 0 with
        // one line saying so rather than being silently clamped to a real
        // arena the author did not name.
        arena = Q_atoi(Info_ValueForKey(userinfo, "arena"));
        if (arena < 0 || arena > num_arenas || arena > MAX_ARENAS)
        {
            newgameimport.dprintf("WARNING: bot arena key %d is outside 0..%d; "
                                  "the bot waits in the queue instead\n",
                                  arena, num_arenas);
            arena = 0;
        } //end if
        ent->client->resp.context = arena;
    } //end if
    //one extra bot
    botglobals.numbots++;
    //
    return ent;
} //end of the function BotCreate
//===========================================================================
// move the given bot from its current client edict_t to a free one
// returns true if the bot is moved otherwise returns false
//
// Called from ClientConnect when a human needs the slot; if no slot
// is free the human's connection is refused, not stolen.
//
// Parameter:               bot             : bot to move
// Returns:                 boolean depending on a succesfull move
// Changes Globals:     botglobals.botstates
//===========================================================================
bool BotMoveToFreeClientEdict(edict_t *bot)
{
    edict_t *newcl;
    gclient_t *newclient;
    bot_state_t *bs, *newbs;
    int playernum;

    if (!bot->inuse) return true;
    if (!(bot->flags & FL_BOT)) return true;
    //spawn a free client edict
    newcl = G_SpawnClient();
    //if there isn't a free client edict available
    if (!newcl) return false;
    //copy the bot to the new client edict.  The gclient_t pointer G_SpawnClient
    //just installed is the NEW slot's and must survive the copy -- the donor
    //overwrites it with the old one and then relies on the memcpy of the
    //gclient_t below happening to the right place.  It does not: `newcl->client`
    //is the old client after the memcpy, so the copy writes the bot's state
    //back over itself and the new slot's gclient_t is never filled.
    newclient = newcl->client;
    *newcl = *bot;
    newcl->client = newclient;
    //copy the contents of the g_client_t structure
    *newcl->client = *bot->client;
    //the edict's own identity does not travel with it
    newcl->s.number = DF_ENTNUMBER(newcl);
    //copy bot state
    bs = &botglobals.botstates[DF_ENTCLIENT(bot)];
    newbs = &botglobals.botstates[DF_ENTCLIENT(newcl)];
    *newbs = *bs;
    //old bot state isn't used anymore
    bs->active = false;
    bs->started = false;
    bs->library = NULL;
    //the new state is
    newbs->active = true;
    //move the bot client in the library before the old edict is cleared: the
    //brain is told the new client number while both still exist
    BotLib_BotMoveClient(bot, newcl);
    //change the user info
    ClientUserinfoChanged(newcl, newcl->client->pers.userinfo);
    gi.linkentity(newcl);
    //Initialize client edict previously used by the bot
    //clear the gclient_t structure the bot was using
    memset(bot->client, 0, sizeof(gclient_t));
    //clear the edict the bot was using
    playernum = DF_ENTCLIENT(bot);
    memset(bot, 0, sizeof(edict_t));
    //initialize edict
    G_InitEdict(bot);
    //set pointer to g_client structure
    bot->client = &game.clients[playernum];
    bot->inuse = false;
    //remove client userinfo.  The configstring number is game.csr's,
    //never a literal.
    gi.configstring(game.csr.playerskins + playernum, "");
    //the bot has been succesfully moved
    return true;
} //end of the function BotMoveToFreeClientEdict
//===========================================================================
// Drop the queue's head without freeing it.
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     queuedbots
//===========================================================================
void BotQueueForget(void)
{
    queuedbots = NULL;
}
//===========================================================================
// spawn bots after level changes
// called from SpawnEntities in g_spawn.c
// ClientConnect for real clients is called after SpawnEntities is executed
// so the bot will be moved to another client edict when new clients come
// into the game during the level change
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotSpawn(void)
{
    int i;
    edict_t *cl_ent;

    //a new level is a new question: the last one's roster ceiling was about the
    //last one's target.  Ahead of the guard below,
    //because a server with no bot states yet still changes map.  This is
    //arena's clear too now -- arena_init() used to zero a second copy.
    botfill_ceiling = 0;
    if (!botglobals.botstates) return;
    for (i = 0; i < game.maxclients; i++)
    {
        //if the bot state was in use
        if (botglobals.botstates[i].active)
        {
            cl_ent = DF_CLIENTENT(i);
            //set started to false
            botglobals.botstates[i].started = false;
            //entity is used
            cl_ent->inuse = true;
            //set the bot flag
            cl_ent->flags |= FL_BOT | FL_BOTCLIENT;
            BotSetSvFlags(cl_ent);
            //set user info because Quake2 likes to remove it for fake clients
            ClientUserinfoChanged(cl_ent, cl_ent->client->pers.userinfo);
        } //end if
    } //end for
} //end of the function BotSpawn
//===========================================================================
// Blocks 6 and 7 of seventeen: the donor's two `#ifndef TOURNEY`
// fences EMPTY these, because tourney's own statusbar owns the layout channel
// and a "loading" pic drawn over it never goes away.  A runtime branch, so the
// other four rulesets keep the SDK's loading screen.
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void ShowLoadImage(edict_t *ent)
{
    char loadstring[64];

    if (G_IsOspRuleset()) return;
    if (!ent || !ent->client) return;
    Q_snprintf(loadstring, sizeof(loadstring), "xv 104 yv 128 picn loading");
    bot_SendLayout(ent, loadstring);
    ent->client->showloading = true;
} //end of the function ShowLoadImage
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void RemoveLoadImage(edict_t *ent)
{
    if (G_IsOspRuleset()) return;
    if (!ent || !ent->client) return;
    //
    // The donor repaints dm_statusbar or single_statusbar here, which is the
    // 1999 shape: one bar per game type, chosen at the point of use.  Here the
    // bar is composed per ruleset and G_Layout_Clear() is the one place that
    // knows how to take a layout back off a client -- so the repaint is asked
    // for by name rather than re-derived here.
    if (ent->client->menu_owner != MENU_BOT)
    {
        G_LayoutClear(ent);
    } //end if
    ent->client->showloading = false;
} //end of the function RemoveLoadImage
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void AddBotToQueue(edict_t *ent, const char *library, const char *userinfo)
{
    queuedbot_t *bot;

    gi.dprintf("loading...\n");
    if (ent) ShowLoadImage(ent);
    bot = gi.TagMalloc(sizeof(queuedbot_t), TAG_GAME);
    memset(bot, 0, sizeof(*bot));
    bot->ent = ent;
    bot->count = 2;
    Q_strlcpy(bot->library, library, sizeof(bot->library));
    // MAX_INFO_STRING-1 on the userinfo copy, and Q_strlcpy rather
    // than a memcpy of a fixed length out of a buffer that may be shorter.
    Q_strlcpy(bot->userinfo, userinfo, sizeof(bot->userinfo));

    bot->next = queuedbots;
    queuedbots = bot;
} //end of the function AddBotToQueue
//===========================================================================
// Spawning is deferred to frame start, so no bot is created inside
// SpawnEntities or inside a ClientConnect.
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void AddQueuedBots(void)
{
    queuedbot_t *bot;
    bot_library_t *lib;
    edict_t *botent;

    if (!queuedbots) return;
    if (queuedbots->count-- > 0) return;

    bot = queuedbots;
    queuedbots = queuedbots->next;
    //load the default library
    lib = BotUseLibrary(bot->library);
    if (!lib)
    {
        gi.cprintf(bot->ent, PRINT_HIGH, "%s not available\n", bot->library);
    } //end if
    else
    {
        botent = BotCreate(bot->userinfo, lib);
        if (!botent)
        {
            gi.cprintf(bot->ent, PRINT_HIGH, "can't create bot, maxclients = %d\n", game.maxclients);
            //free the library used by the bot
            BotFreeLibrary(lib);
        } //end if
    } //end else
    if (bot->ent) RemoveLoadImage(bot->ent);
    gi.TagFree(bot);
} //end of the function AddQueuedBots
//===========================================================================
// returns true if there is a client with the given name
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static bool ClientNameExists(const char *name)
{
    int i;
    edict_t *cl_ent;

    for (i = 0; i < game.maxclients; i++)
    {
        cl_ent = DF_CLIENTENT(i);
        if (cl_ent->inuse && cl_ent->client)
        {
            if (Q_strcasecmp(cl_ent->client->pers.netname, name) == 0)
            {
                return true;
            } //end if
        } //end if
    } //end for
    return false;
} //end of the function ClientNameExists
//===========================================================================
// Block 5 of seventeen: a duplicate bot name.  The SDK refuses it; tourney
// auto-suffixes so that `addrandom 8` cannot stall on a name clash.  The
// suffixing is the better behaviour and it is also the one that changes what a
// client sees, so it stays tourney's: a donor's behaviour belongs to the
// ruleset that shipped it.
//
// Returns false when the name cannot be used at all.
//===========================================================================
static bool BotUniqueName(const char *want, char *out, size_t outsize)
{
    char suffix;

    Q_strlcpy(out, want, outsize);
    if (!ClientNameExists(out)) return true;

    if (!G_IsOspRuleset()) return false;

    // MAX_NETNAME is 16 including the terminator, so a name of 15 characters
    // has nowhere to put a suffix and the last character is overwritten.
    if (strlen(out) < 15)
    {
        for (suffix = '0'; suffix <= '9'; suffix++)
        {
            Q_snprintf(out, outsize, "%s%c", want, suffix);
            if (!ClientNameExists(out)) return true;
        } //end for
    } //end if
    else
    {
        for (suffix = '0'; suffix <= '9'; suffix++)
        {
            out[14] = suffix;
            out[15] = '\0';
            if (!ClientNameExists(out)) return true;
        } //end for
    } //end else
    return false;
}
//===========================================================================
// add a deathmatch bot
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotAddDeathmatch(edict_t *ent)
{
    char uinfo[MAX_INFO_STRING];
    char bname[MAX_NETNAME];
    int max, i;

    if (!Q_stricmp(gi.argv(0), "sv"))
    {
        max = 6;
        i = 1;
    } //end if
    else
    {
        max = 5;
        i = 0;
    } //end else
    //need at least 6 parmeters (command name included)
    if (gi.argc() < max)
    {
        gi.cprintf(ent, PRINT_HIGH, "too few parameters\n");
        gi.cprintf(ent, PRINT_HIGH,
                        "Usage:   addbot <name> <skin> <charfile> <charname>\n"
                        "Example: addbot brianna female/brianna char.c Brianna\n"
                        "\n"
                        "<name>     = name of the bot\n"
                        "<skin>     = skin of the bot\n"
                        "<charfile> = character file\n"
                        "<charname> = character name\n");
        return;
    } //end if

    memset(uinfo, 0, sizeof(uinfo));
    if (!BotUniqueName(gi.argv(i + 1), bname, sizeof(bname)))
    {
        gi.cprintf(ent, PRINT_HIGH, "client name %s is already used\n", gi.argv(i + 1));
        return;
    } //end if
    Info_SetValueForKey(uinfo, "name", bname);
    Info_SetValueForKey(uinfo, "skin", gi.argv(i + 2));
    Info_SetValueForKey(uinfo, "charfile", gi.argv(i + 3));
    Info_SetValueForKey(uinfo, "charname", gi.argv(i + 4));
    //
    // The donor's two #ifdef fences, as the resolution's answer.  `arena` and
    // `botctfteam` are cvars a server operator sets; the userinfo key is how
    // the value reaches ClientConnect, which is the only place that can act on
    // it.  botctfteam is re-registered here, RA2's port having dropped it.
    if (G_Ruleset() == RULESET_ARENA)
    {
        // "0", and see RA_BotFillArena() for why -- the four readers of
        // this cvar have to agree on the default, because the first one to run
        // is the one that creates it.
        cvar_t *arena_cvar = gi.cvar("arena", "0", 0);

        // The donor forced "1" here when the cvar was out of range, and
        // "1" is a request like any other -- it cannot be told apart from an
        // operator who meant arena 1.  "0" is the absence of a request, which
        // RA_BotJoinArena resolves by following the people; 1..N still reaches
        // it verbatim, which is 1999's behaviour.
        if (arena_cvar->value > 0 && arena_cvar->value <= num_arenas)
            Info_SetValueForKey(uinfo, "arena", arena_cvar->string);
        else
        {
            // ...and where the operator named no arena, the FILL may
            // still have one in mind.  It works the arena furthest from its own
            // target, which on a multi-arena map is not the one "follow the
            // people" resolves to -- that is always the lowest-numbered
            // populated arena, so without this every bot the fill added for
            // arena 5 would arrive in arena 1 and the fill would ask again next
            // tick, forever.  0 still comes back for a staging bot, which must
            // stay a follower or it cannot be moved when people arrive.
            int want = RA_BotFillDestination();
            char buf[16];

            if (want > 0)
            {
                Q_snprintf(buf, sizeof(buf), "%d", want);
                Info_SetValueForKey(uinfo, "arena", buf);
            } //end if
            else Info_SetValueForKey(uinfo, "arena", "0");
        } //end else
    } //end if
    if (G_Ruleset() == RULESET_CTF)
    {
        Info_SetValueForKey(uinfo, "ctfteam", gi.cvar("botctfteam", "0", 0)->string);
    } //end if
    //load the default library
    AddBotToQueue(ent, gi.cvar("botlib", BotDefaultLibrary(), 0)->string, uinfo);
} //end of the function BotAddDeathmatch
//===========================================================================
// become a bot
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotBecomeDeathmatch(edict_t *ent)
{
    bot_library_t *lib;

    if (!ent)
    {
        gi.dprintf("only a client can become a bot\n");
        return;
    } //end if
    //load the default library
    lib = BotUseLibrary(gi.cvar("botlib", BotDefaultLibrary(), 0)->string);
    if (!lib)
    {
        gi.cprintf(ent, PRINT_HIGH, "%s not available\n",
                   gi.cvar("botlib", BotDefaultLibrary(), 0)->string);
        return;
    } //end if
    BotBecome(ent, lib);
} //end of the function BotBecomeDeathmatchBot
//===========================================================================
// add a deathmatch bot
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================

/*QUAKED bot (0 .5 .8) ?
Spawns a bot.

"name"          name
"skin"          skin
"charfile"      file which contains the bot character
"charname"      name of the bot character
*/

void SP_bot(edict_t *self)
{
    // No bots in the campaign, and a map that spawns one there is
    // a map that was built for a deathmatch mod.  Freed rather than refused.
    if (!G_BotsAllowed())
    {
        G_FreeEdict(self);
        return;
    } //end if
    // This runs inside SpawnEntities, so it may only QUEUE.  The
    // donor is already careful about that -- BotAddDeathmatch ends in
    // AddBotToQueue -- and the synthesised argument vector is what carries the
    // five spawn keys to it.  Those keys are spawn_temp_t's.
    BotStoreClientCommand("sv", "addbot", st.name, st.skin, st.charfile,
                          st.charname, NULL);
    BotAddDeathmatch(NULL);
    BotClearCommandArguments();
    G_FreeEdict(self);
} //end of the function SP_bot
//===========================================================================
// remove a deathmatch bot
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotRemoveDeathmatch(edict_t *ent)
{
    int i;
    edict_t *cl_ent;
    const char *name;

    //
    if (!Q_stricmp(gi.argv(0), "sv")) i = 2;
    else i = 1;
    //check if there's a name available
    if (gi.argc() > i) name = gi.argv(i);
    else name = NULL;
    //
    if (name && !Q_stricmp(name, "all"))
    {
        BotDestroyAll();
        return;
    } //end if
    for (i = 0; i < game.maxclients; i++)
    {
        cl_ent = DF_CLIENTENT(i);
        if (!cl_ent->inuse) continue;
        if (cl_ent->flags & FL_BOT)
        {
            if (!name || (Q_strcasecmp(cl_ent->client->pers.netname, name) == 0))
            {
                BotDestroy(cl_ent);
                return;
            } //end if
        } //end if
    } //end for
    if (name) gi.cprintf(ent, PRINT_HIGH, "No bot found with name %s\n", name);
    else gi.cprintf(ent, PRINT_HIGH, "No bots found to remove!\n");
} //end of the functoin BotRemoveDeathmatch
//===========================================================================
// What `botfill` asks for, or 0 when the switch is off -- and 0 is the only
// value that means off, which is why the clamp to two lives here: a map with
// nothing to count still gets a game.
//
// The RULESET owns its number and this owns the ceilings, because the ceilings
// are the same three questions for all of them: two sides make a round,
// `game.maxclients` is what the engine will seat, and the roster is what
// `bots.cfg` can supply.  arena's number is per arena as well, so it comes from
// RA_BotFillTarget(), but it comes through here now -- arena.c carried its own
// copy of these same three lines over its own `botfill_ceiling`, so "the
// ceilings are shared" was true of two rulesets and not the third.
//
// Parameter:               -
// Returns:                 the target, or 0
// Changes Globals:     -
//===========================================================================
int BotFillTarget(void)
{
    int want;

    if (!BotFillEnabled())
        return 0;

    switch (G_Ruleset())
    {
        case RULESET_CTF: want = CTF_BotFillSeats(); break;
        // `dm` and `dmpro` are the OSP four's teamless half and declare no
        // capacity -- `team_maxplayers` sizes a TEAM and they have none -- so
        // the map is the only signal, exactly as it was for baseq2's `dm`
        // before the flattening.
        case RULESET_DM:
        case RULESET_DMPRO: want = DM_BotFillSeats(); break;
        // ...and `tdm` and `duel` do declare one, so that is what is used.
        // `duel` forces team_maxplayers to 1 CVAR_NOSET, so this is 2 there by
        // construction.
        case RULESET_TDM:
        case RULESET_DUEL: want = 2 * OSP_TeamMaxPlayers(); break;
        case RULESET_ARENA:
        {
            // No arena on the map will take bots.  That is a real zero
            // and has to leave before the two-sides clamp below, which exists
            // for the other zero -- "a map with nothing to count" -- and would
            // otherwise turn this one into 2.  CheckMinimumPlayers never reads
            // it (its `fillarena` is 0 too, so it does not ask), but `sv
            // ruleset` prints it unconditionally, and a diagnostic that says
            // `want=2` beside "every arena has bots switched off" is the kind
            // of half-truth this line exists to stop telling.
            int n = RA_BotFillArena();

            if (!n) return 0;
            want = RA_BotFillTarget(n);
            // ...and a selected arena can want zero -- the one everyone
            // has just walked out of, which the removal arm has to drain to
            // nothing.  That zero leaves before the two-sides clamp for the
            // same reason the other one does: the clamp is for a map with
            // nothing to count, and this is a number that was counted.
            if (!want) return 0;
            break;
        }
        default: return 0;
    } //end switch

    // Two sides is what makes it a game.
    if (want < 2) want = 2;
    if (botfill_ceiling && want > botfill_ceiling) want = botfill_ceiling;
    if (want > game.maxclients) want = game.maxclients;

    return want;
} //end of the function BotFillTarget
//===========================================================================
// Where the number in BotFillTarget() came from, for `sv ruleset`.
// The target is computed rather than stored, so a play test has nowhere else to
// read it back from, and printing only the clamped answer hides the clamp.
//
// Parameter:               buf, len: the description is written here
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotFillDescribe(char *buf, size_t len)
{
    switch (G_Ruleset())
    {
        case RULESET_CTF:
            Q_snprintf(buf, len, "seats=%d, shared=%d base=%d+%d",
                       CTF_BotFillSeats(),
                       G_SpawnPointPool("info_player_deathmatch"),
                       G_SpawnPointPool("info_player_team1"),
                       G_SpawnPointPool("info_player_team2"));
            break;
        case RULESET_DM:
        case RULESET_DMPRO:
            Q_snprintf(buf, len, "seats=%d, spawns=%d",
                       DM_BotFillSeats(),
                       G_SpawnPointPool("info_player_deathmatch"));
            break;
        case RULESET_TDM:
        case RULESET_DUEL:
            Q_snprintf(buf, len, "2 * team_maxplayers %d",
                       OSP_TeamMaxPlayers());
            break;
        case RULESET_ARENA:
        {
            int n = RA_BotFillArena();

            // 0 is an answer now -- no arena on the map will take bots
            // -- and it is worth saying so in words, because the numbers that
            // would otherwise be printed beside it are those of arena 0, which
            // is the observers' non-arena and never a fill target.
            if (!n)
            {
                Q_strlcpy(buf, "nothing -- every arena has bots switched off",
                          len);
                break;
            } //end if

            Q_snprintf(buf, len, "arena %d, %d here, %s", n,
                       RA_ArenaPlayers(n, NULL),
                       RA_ArenaIsPickup(n) ? "pickup: by spawn points"
                                           : "duel: by playersperteam");
            break;
        }
        default:
            Q_strlcpy(buf, "nothing -- this ruleset has no fill target", len);
            break;
    } //end switch
} //end of the function BotFillDescribe
//===========================================================================
// The roster ran out.  Both arms of CheckMinimumPlayers have to settle on one
// number or the server never sits still, and the arms that read a flat count
// get that by writing the count they achieved back into `minimumplayers`.  A
// switch cannot carry a count -- and writing 0 into it would be worse than
// useless, because the target would fall back to `minimumplayers`, which a
// server using the fill has no reason to have set, and the removal arm would
// then delete every bot that had just been added.  So the ceiling is held here
// and cleared by BotSpawn().  arena's used to be a second copy in arena.c;
// there is one now.
//
// Parameter:               achieved: the bot count the roster could reach
// Returns:                 -
// Changes Globals:     botfill_ceiling
//===========================================================================
void BotFillNoMore(int achieved)
{
    if (achieved < 2) achieved = 2;
    if (!botfill_ceiling || achieved < botfill_ceiling)
        botfill_ceiling = achieved;
} //end of the function BotFillNoMore
//===========================================================================
// Blocks 3 and 4 of seventeen.
//
// The donor's `#ifdef TOURNEY` arm is not one branch but four differences, and
// keeping them straight is the whole job:
//
//   * the cvar is `bots_minplayers`, not `minimumplayers`
//   * a client only counts when resp.osp_entered == ENTERED_ENTERED
//   * the arithmetic subtracts bots_votedin -- players who voted a bot in are
//     not players the bot count should replace
//   * `bots_autoload == 4` is its OWN arm, not the first half of one `||`
//
// The `else` arm's removal test is a WRAP, not a guard-return: the donor's
// `if ((numplayers - bots_votedin - 1) > minplayers)` sits inside the else and
// falls through to nothing when it fails.  Read as a guard it would remove a
// bot every 32 frames on a quiet server.
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void CheckMinimumPlayers(void)
{
    cvar_t *minplayers;
    edict_t *cl_ent;
    queuedbot_t *bot;
    int i, numplayers, numbots, pending, totalbots, want, votedin;
    int fillarena, fill;
    bool fillon;
    char buf[32];

    minplayers = BotMinPlayers();

    // Which arena the fill is feeding, and it is asked for first
    // because under `arena` it is also the CENSUS that changes: a target
    // belonging to one of up to 32 games on the map has to be compared against
    // that game's head count and not the server's.  That is the one asymmetry
    // left between the rulesets now that there is one cvar and one target
    // function -- and it is an asymmetry of counting, not of switching.
    fillarena = G_Ruleset() == RULESET_ARENA ? RA_BotFillArena() : 0;

    // Only the switch is read here.  Everything above the frame gate below runs
    // on every frame, and the target is not a cvar read: BotFillTarget() walks
    // the entity list once per spawn-point class -- three times under `ctf` --
    // so it is asked for on a fill tick and not before.
    fill = 0;
    // `!fillarena` used to mean "not the arena ruleset", and now it can
    // also mean "the arena ruleset declined".  Until this feature there was no
    // difference: with `botfill` on and a map loaded, RA_BotFillArena() always
    // answered 1..N, so a 0 from it could only be the switch being off -- in
    // which case BotFillEnabled() was false too and this line came out false
    // either way.  A per-arena `bots` switch breaks that: every arena on the
    // map can now refuse while `botfill` is still on, and reading that 0 as
    // "some other ruleset" would hand the arena map to the FLAT count, whose
    // census spans every arena at once and whose bots RA_BotJoinArena is busy
    // refusing to seat.  The ruleset is asked instead of inferred.
    fillon = !fillarena && BotFillEnabled() && G_Ruleset() != RULESET_ARENA;

    if (!minplayers->value && !fillarena && !fillon) return;
    //
    if (level.framenum & 31) return;
    //arena used to return here, and rightly: with no bot able to reach an
    //arena roster, a bot added under `arena` stood in arena 0 forever and
    //adding "a body mid-round" was the only thing it could have meant.
    //RA_BotJoinArena changes the fact the early-out rested on -- it puts a new
    //bot in the selected arena's waiting queue, which is where a human who
    //connects mid-round goes too -- so bots under `arena` are implementable
    //and the early-out is gone.  What keeps this
    //from running away is BotCountsAsPlayer's arena arm: under arena a player
    //is somebody on a team, not somebody currently alive in the round.
    if (!G_BotsAllowed()) return;
    //count the number of players and the number of bots
    numplayers = 0;
    numbots = 0;
    pending = 0;
    for (i = 0; i < game.maxclients; i++)
    {
        cl_ent = DF_CLIENTENT(i);
        if (!BotCountsAsPlayer(cl_ent))
        {
            // A bot that is not yet a player is still a bot on its way in, and
            // the add arm below has to know that or it asks for the same bot
            // again every 32 frames.  BotStarted() holds a connected bot's
            // ClientBegin back until its library reports initialised, and on
            // the first visit to a map that is the whole reachability build --
            // twenty seconds of frames on a real map.  Under arena there is a
            // second window on top of it: BotCountsAsPlayer asks for
            // `teamnum >= 0`, which RA_BotJoinArena only sets from inside that
            // deferred ClientBegin.
            //
            // Measured on ra2map7: seven bots connected while ra2map7.aas was
            // being written, all seven joined the pickup teams at once when it
            // finished, and the removal arm then threw three of them straight
            // back out -- "Java Man entered the game" / "Java Man: goodbye"
            // three lines apart.
            if (cl_ent->inuse && cl_ent->client && (cl_ent->flags & FL_BOTCLIENT))
                pending++;
            continue;
        } //end if
        numplayers++;
        if (cl_ent->flags & FL_BOT) numbots++;
    } //end for
    //a queued bot has not even been created yet, and counts the same way
    for (bot = queuedbots; bot; bot = bot->next)
    {
        pending++;
    } //end for
    //
    // The arena arithmetic replaces the two counts and the target, and leaves
    // `pending` alone.  A pending bot has connected and has no arena yet --
    // `resp.context` is 0 and `resp.teamnum` is -1 until RA_BotJoinArena runs
    // from the deferred ClientBegin -- so it belongs to no arena's census and
    // must still be subtracted from what the fill asks for.  That is the same
    // fact the comment above records, reached from the other side.
    if (fillarena)
    {
        numplayers = RA_ArenaPlayers(fillarena, &numbots);
    } //end if
    //0 from a ruleset that has no fill target, which `arena` is when its own
    //RA_BotFillArena() declined -- then the flat count is the target, as it is
    //with the switch off.  One call for every ruleset now that there is one
    //cvar: the arena's census is still its own, but its TARGET comes back
    //through BotFillTarget() with everybody else's ceilings applied.
    if (fillon || fillarena) fill = BotFillTarget();
    // Under `arena` the fill owns the number, and zero is one of its
    // ANSWERS.  `fill ? fill : minimumplayers` reads 0 as "this ruleset has no
    // target", which is true of a ruleset that declined and false of an ARENA
    // That wants none -- the one everybody has just walked out of.  Falling
    // back to the flat count there would answer a per-arena question with a
    // server-wide census and re-add the bots the removal arm had just drained.
    if (fillarena) want = fill;
    else want = fill ? fill : (int)minplayers->value;
    votedin = BotTourneyVotedIn();
    // `totalbots` is the donor's `numbots`: it counted the queue into the same
    // variable.  They are separate here because the removal arm needs a bot
    // that has actually been created to remove, and `numbots` is that count.
    totalbots = numbots + pending;

    if (G_IsOspRuleset() &&
        (int)gi.cvar("bots_autoload", "0", 0)->value == 4 && totalbots < want)
    {
        if (!AddRandomBot(NULL))
        {
            Q_snprintf(buf, sizeof(buf), "%d", totalbots);
            gi.cvar_set(BotMinPlayersCvar(), buf);
        } //end if
        old_botcount = totalbots;
    } //end if
    else if (G_IsOspRuleset()
             ? ((numplayers - votedin - 1) < want &&
                numplayers < game.maxclients && totalbots < want &&
                old_botcount != totalbots)
             : (numplayers + pending < want))
    {
        if (!AddRandomBot(NULL))
        {
            // The clamp exists so that the add and the remove arms
            // settle on one number when the roster runs out.  `botfill` is a
            // switch and cannot hold a count -- and writing 0 into it would be
            // worse than useless, because the target would fall back to the
            // flat count, which is 0 under this configuration, and the removal
            // arm would then delete every bot there is.  So the ceiling is held
            // beside the target, in one place for every ruleset.
            if (fill) BotFillNoMore(totalbots);
            else
            {
                Q_snprintf(buf, sizeof(buf), "%d", totalbots);
                gi.cvar_set(BotMinPlayersCvar(), buf);
            } //end else
        } //end if
        old_botcount = totalbots;
    } //end if
    else
    {
        // Adding and removing both, and the non-TOURNEY arm is the SDK's own:
        // Gladiator's bl_spawn.c ends in `else if (numplayers >
        // minplayers->value)` with no `- 1`, so add and remove settle on the
        // same number.  Tourney's `- bots_votedin - 1` is tourney's, and
        // carrying it into the other rulesets left the two arms half a player
        // apart -- adding up to `want`, removing down to `want + 1` -- which
        // is a server that never sits still on either side of a join.
        if (G_IsOspRuleset()
            ? ((numplayers - votedin - 1) > want)
            : (numplayers > want))
        {
            // Under the fill the bot is NAMED, because `removebot` with no name
            // takes the lowest client slot -- and that bot may be playing in an
            // arena nobody asked to shrink, or holding up the side that is
            // already short.  A NULL name terminates the vararg list,
            // which is the call the other rulesets already make.
            if (numbots > 0)
                BotServerCommand("sv", "removebot",
                                 fillarena ? RA_ArenaBotName(fillarena) :
                                 fill && G_Ruleset() == RULESET_CTF ?
                                 CTFBotFillName() : NULL,
                                 NULL);
            old_botcount = totalbots;
        } //end if
    } //end else
} //end of the function CheckMinimumPlayers
