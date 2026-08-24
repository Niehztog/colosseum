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
// Bot setup and the botlib handshake, from osp-tourney@1d8427e (SPECS.md
// sec 5.4.1, 5.4.2, 5.4.5).  The reconstruction's asm-matching address comments
// are stripped -- SPECS.md N1 makes those oracles meaningless here.
//===========================================================================
//
// Name:                bl_main.c
// Function:        bot setup
// Programmer:      Mr Elusive (MrElusive@demigod.demon.nl)
// Last update: 1999-02-10
// Tab Size:        3
//===========================================================================

#include "g_local.h"
#include "bot/bl_main.h"
#include "bot/bl_spawn.h"
#include "bot/bl_redirgi.h"
#include "bot/bl_botcfg.h"
#include "bot/bl_debug.h"
#include "bot/p_botmenu.h"
#include "ctf/g_ctf.h"
#include "tourney/osp_hooks.h"

#if defined(_WIN32)
#include <windows.h>
#define PATHSEPERATOR_CHAR          '\\'
#define PATHSEPERATOR_STR           "\\"
#else
#include <dlfcn.h>
#include <unistd.h>
#define PATHSEPERATOR_CHAR          '/'
#define PATHSEPERATOR_STR           "/"
#endif

bot_globals_t botglobals;

/*
===============================================================================

R-BOT-29 -- THE SEVENTEEN TOURNEY BLOCKS, AS RUNTIME BRANCHES

The donor defines TOURNEY live in its g_local.h, so all seventeen apply
library-wide.  Here each one asks the active ruleset instead, and the tourney
state they read is reached through these accessors rather than through the
`extern int m_mode;` the donor put at the top of the file -- an extern that
resolves in every ruleset and answers with tourney's globals whether or not
tourney is running.  See bl_main.h for why BotTourneyMode() is a value and not
a predicate.

The donor also declared `extern void OSP_serverbotsRemove(void);` here and
never called it.  That is R-OSP-5's shape -- a donor's name declared outside
the header that owns it -- and it goes rather than being carried; the function
exists in src/tourney/ and osp_teams.c is what calls it.

===============================================================================
*/

int BotTourneyMode(void)
{
    return G_Ruleset() == RULESET_TOURNEY ? m_mode : -1;
}

int BotTourneyRunes(void)
{
    return G_Ruleset() == RULESET_TOURNEY ? rune_stat : 0;
}

bool BotTourneyHook(void)
{
    return G_Ruleset() == RULESET_TOURNEY && hook_enable && hook_enable->value;
}

int BotTourneyVotedIn(void)
{
    return G_Ruleset() == RULESET_TOURNEY ? bots_votedin : 0;
}

const char *BotMinPlayersCvar(void)
{
    // R-OSP-11: the cvar names stay PER RULESET.  tourney's readme, its configs
    // and its `bots_*` family all say bots_minplayers; every other ruleset's
    // 1999 documentation says minimumplayers.  Renaming either would break a
    // config file that has been correct for twenty years.
    return G_Ruleset() == RULESET_TOURNEY ? "bots_minplayers" : "minimumplayers";
}

const char *BotFileCvar(void)
{
    return G_Ruleset() == RULESET_TOURNEY ? "bots_botfile" : "botfile";
}

cvar_t *BotMinPlayers(void)
{
    // "4" under tourney because osp_main.c's OSP_gameInit registers it with
    // that default and runs first; "0" everywhere else because that is what the
    // 1999 SDK's `minimumplayers` has always defaulted to and a server that
    // says nothing must not grow bots.
    return gi.cvar(BotMinPlayersCvar(),
                   G_Ruleset() == RULESET_TOURNEY ? "4" : "0", 0);
}

cvar_t *BotFile(void)
{
    return gi.cvar(BotFileCvar(), "botcfg/bots.cfg", 0);
}

bool BotCountsAsPlayer(edict_t *cl_ent)
{
    if (!cl_ent->inuse || !cl_ent->client)
        return false;
    // The Gladiator SDK's own exclusion, which R-BOT-17 keeps and uGladQ2
    // records as a fix for CTF (v0.98.2u) and RA2 (v0.98.1u): somebody watching
    // is not somebody playing, so a server full of spectators still fills up
    // with bots.  G_IsObserver() is the one predicate that knows all four
    // spellings of that question (R-CTF-5).
    // Arena is asked FIRST and differently, and R-RA-4's sixteenth row is why.
    // Under arena G_IsObserver() means `fightstate == FIGHT_SPECTATING`, which
    // is true of everyone not in the CURRENT ROUND -- the whole waiting queue
    // included.  Read through the generic test, a server with four bots of whom
    // two are fighting counts two players and adds two more, then four more,
    // for as long as anybody is waiting: `minimumplayers` would never be
    // satisfied.  What "playing" means under arena is being on a team, which is
    // also what the uGladQ2 fix wants -- a person parked in arena 0 with the
    // team menu open is an audience and must not hold bots out.
    if (G_Ruleset() == RULESET_ARENA)
        return cl_ent->client->resp.teamnum >= 0;
    if ((cl_ent->flags & FL_OBSERVER) || G_IsObserver(cl_ent))
        return false;
    // Tourney adds a fourth state: connecting, entered, observing or queued.
    if (G_Ruleset() == RULESET_TOURNEY)
        return cl_ent->client->resp.osp_entered == ENTERED_ENTERED;
    return true;
}

//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void StringMakeGreen(char *str)
{
    cvar_t *v;
    //never make green on a dedicated server
    v = gi.cvar("dedicated", "0", 0);
    if (v->value) return;
    //set the last bit
    while(*str)
    {
        if (*str > ' ') *str |= 128;
        str++;
    } //end while
} //end of the function StringMakeGreen
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static int BotSwimming(vec3_t origin)
{
    vec3_t testorg;

    VectorCopy(origin, testorg);
    testorg[2] += 3;
    if (gi.pointcontents(testorg) & MASK_WATER) return true;
    return false;
} //end of the function BotSwimming
//===========================================================================
// hooked in G_RunFrame in g_main.c
// NOTE: don't call between: ClientBeginServerFrame and ClientEndServerFrames
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotExecuteInput(edict_t *bot)
{
    vec3_t angles, forward, right;
    usercmd_t ucmd;
    bot_input_t *bi;
    int client;

    if (botglobals.nobotinput) return;

    client = DF_ENTCLIENT(bot);
    //
    if (!bot->client)
    {
        gi.dprintf("client %d without client structure\n", client);
        return;
    } //end if
    //get the input for this client
    if (botglobals.botnewinput[client])
    {
        botglobals.botnewinput[client] = false;
        bi = &botglobals.botinputs[client];
    } //end if
    else
    {
        //there's no new input
        return;
    } //end else
    //
    // usercmd_t
    //
    // byte msec;
    //      number of milliseconds since the last user command update of the client
    //
    // byte buttons;
    //      only BUTTON_ATTACK = 1, BUTTON_USE = 2 and BUTTON_ANY = 128 are
    //      sent from the client to the server.
    //
    // short angles[3];
    //      the current view angles of the client
    //
    // short forwardmove, sidemove, upmove;
    //      forwardmove and sidemove are relative to the given yaw
    //      upmove is not related to the given yaw
    //
    //clear the whole structure
    memset(&ucmd, 0, sizeof(usercmd_t));
    //the duration for the user command in milli seconds
    ucmd.msec = 1000 * bi->thinktime;
    //
    if (botglobals.nocldouble && (bi->actionflags & ACTION_DELAYEDJUMP))
    {
        bi->actionflags |= ACTION_JUMP;
        bi->actionflags &= ~ACTION_DELAYEDJUMP;
    } //end if
    //set the buttons
    if (bi->actionflags & ACTION_RESPAWN)
    {
        bot->client->latched_buttons |= BUTTON_ATTACK;
    } //end if
    if (bi->actionflags & ACTION_ATTACK)
    {
        ucmd.buttons |= BUTTON_ATTACK;
    } //end if
    if (bi->actionflags & ACTION_USE)
    {
        ucmd.buttons |= BUTTON_USE;
    } //end if
    //set the view angles
    ucmd.angles[PITCH] = ANGLE2SHORT(bi->viewangles[PITCH]);
    ucmd.angles[YAW] = ANGLE2SHORT(bi->viewangles[YAW]);
    ucmd.angles[ROLL] = ANGLE2SHORT(bi->viewangles[ROLL]);
    //get the horizontal forward and right vector
    //if swimming movement is true 3d
    //get the pitch in the range [-180, 180]
    if (BotSwimming(bot->s.origin)) angles[PITCH] = bi->viewangles[PITCH];
    else angles[PITCH] = 0;
    angles[YAW] = bi->viewangles[YAW];
    angles[ROLL] = 0;
    AngleVectors(angles, forward, right, NULL);
    //set the view independent movement
    ucmd.forwardmove = DotProduct(forward, bi->dir) * bi->speed;
    ucmd.sidemove = DotProduct(right, bi->dir) * bi->speed;
    ucmd.upmove = fabsf(forward[2]) * bi->dir[2] * bi->speed;
    //normal keyboard movement
    if (bi->actionflags & ACTION_MOVEFORWARD) ucmd.forwardmove += 400;
    if (bi->actionflags & ACTION_MOVEBACK) ucmd.forwardmove -= 400;
    if (bi->actionflags & ACTION_MOVELEFT) ucmd.sidemove -= 400;
    if (bi->actionflags & ACTION_MOVERIGHT) ucmd.sidemove += 400;
    //jump/moveup
    if (bi->actionflags & ACTION_JUMP) ucmd.upmove += 400;
    //crouch/movedown
    if (bi->actionflags & ACTION_CROUCH) ucmd.upmove -= 400;
    //impulse always zero
    ucmd.impulse = 0;
    //light level at client location
    ucmd.lightlevel = 64;
    //
    // R-BOT-21: FL_BOTINPUT is set for the duration, so client code can tell a
    // synthesised command from a network one.
    bot->flags |= FL_BOTINPUT;
    //the Pmove function was probably designed to run at a higher frequency
    //than 10 Hz.  At this low frequency the movement code performs amazingly
    //bad: the step checking fails half the time, out of water jumping gets
    //pretty difficult, and gravity seems to fail when walking down stairs.
    //To get a higher frequency of Pmove calls the calls are doubled here by
    //calling ClientThink twice, with the ucmd milliseconds halved.  The AI
    //still runs at 10 Hz.
    if (!botglobals.nocldouble)
    {
        ucmd.msec /= 2;
        ClientThink(bot, &ucmd);
        if (bi->actionflags & ACTION_DELAYEDJUMP) ucmd.upmove += 400;
        ClientThink(bot, &ucmd);
    } //end if
    else
    {
        //call the client think function to execute the usercmd_t of the bot
        ClientThink(bot, &ucmd);
    } //end else
    //
    bot->flags &= ~FL_BOTINPUT;
    //set the ping of the bot
    bot->client->ping = 1000 * bi->thinktime + crand() * 20;
} //end of the function BotExecuteInput
//===========================================================================
// set the pmove_state_t of the bot
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void BotSetPMoveState(edict_t *bot)
{
    // The Gladiator SDK redefined PMF_DUCKED..PMF_NO_PREDICTION here as its own
    // documentation of what the engine passes in.  Q2PRO's shared.h defines the
    // same seven with identical values (BIT(0)..BIT(6)), so they are dropped
    // rather than shadowed; bit 7, which the SDK called PMF_UNUSED, is
    // PMF_TELEPORT_BIT to Q2PRO.
    //
    //the Quake2 engine does this for real clients
    VectorClear(bot->client->ps.pmove.delta_angles);
} //end of the function BotSetPMoveState
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static bot_library_t *GetBotLibrary(edict_t *bot)
{
    bot_library_t *lib;

    lib = botglobals.botstates[DF_ENTCLIENT(bot)].library;
    if (!lib)
    {
        gi.dprintf("bot (client %d) without bot library\n", DF_ENTCLIENT(bot));
    } //end if
    return lib;
} //end of the function GetBotLibrary
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
bool BotStarted(edict_t *bot)
{
    bot_library_t *lib;

    //if the bot already started
    if (botglobals.botstates[DF_ENTCLIENT(bot)].started) return true;
    //get the library the bot uses
    lib = GetBotLibrary(bot);
    if (!lib) return false;
    //if the library is initialized
    if (lib->funcs.BotLibraryInitialized())
    {
        //NOTE: set the inuse flag to false because the bot isn't loaded
        //          from a savegame
        bot->inuse = false;
        //the Quake2 server calls this function for real clients
        ClientBegin(bot);
        //
        bot->flags |= FL_BOT;
        //the bot has started
        botglobals.botstates[DF_ENTCLIENT(bot)].started = true;
        return true;
    } //end if
    return false;
} //end of the function BotStarted

//==========================================================================
//
// usage of imported functions from library
//
//==========================================================================

//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotLib_BotLoadMap(char *mapname)
{
    bot_library_t *lib, *nextlib;
    int err;

    for (lib = botglobals.firstbotlib; lib; lib = nextlib)
    {
        nextlib = lib->next;
        err = lib->funcs.BotLoadMap(mapname,
                                    bot_max_modelindexes, modelindexes,
                                    bot_max_soundindexes, soundindexes,
                                    bot_max_imageindexes, imageindexes);
        if (err != BLERR_NOERROR)
        {
            int i;
            edict_t *cl_ent;

            //remove all bots using this library
            for (i = 0; i < game.maxclients; i++)
            {
                cl_ent = DF_CLIENTENT(i);
                if (!cl_ent->inuse) continue;
                if (!(cl_ent->flags & FL_BOT)) continue;
                if (botglobals.botstates[i].library == lib)
                {
                    BotDestroy(cl_ent);
                } //end if
            } //end for
        } //end if
    } //end for
} //end of the function BotLib_BotLoadMap
//==========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//==========================================================================
int BotLib_BotSetupClient(edict_t *ent, char *userinfo)
{
    char *s;
    bot_library_t *lib;
    bot_settings_t settings;

    lib = GetBotLibrary(ent);
    if (!lib) return false;
    memset(&settings, 0, sizeof(bot_settings_t));
    //
    s = Info_ValueForKey(userinfo, "charfile");
    Q_strlcpy(settings.characterfile, s, sizeof(settings.characterfile));
    //
    s = Info_ValueForKey(userinfo, "charname");
    Q_strlcpy(settings.charactername, s, sizeof(settings.charactername));
    //
    return lib->funcs.BotSetupClient(DF_ENTCLIENT(ent), &settings);
} //end of the function BotLib_BotSetupClient
//==========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//==========================================================================
void BotLib_BotShutdownClient(edict_t *client)
{
    bot_library_t *lib;

    lib = GetBotLibrary(client);
    if (!lib) return;
    lib->funcs.BotShutdownClient(DF_ENTCLIENT(client));
} //end of the function BotLib_BotShutDownClient
//==========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//==========================================================================
void BotLib_BotMoveClient(edict_t *oldclient, edict_t *newclient)
{
    bot_library_t *lib;

    lib = GetBotLibrary(oldclient);
    if (!lib) return;
    lib->funcs.BotMoveClient(DF_ENTCLIENT(oldclient), DF_ENTCLIENT(newclient));
} //end of the function BotLib_BotMoveClient
//==========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//==========================================================================
void BotLib_BotClientSettings(edict_t *ent)
{
    bot_clientsettings_t settings;
    bot_library_t *lib;

    if (!ent->client)
    {
        gi.dprintf("client %d without client structure\n", DF_ENTCLIENT(ent));
        return;
    } //end if
    //copy the client name
    Q_strlcpy(settings.netname, ent->client->pers.netname, sizeof(settings.netname));
    //client skin
    Q_strlcpy(settings.skin, Info_ValueForKey(ent->client->pers.userinfo, "skin"),
              sizeof(settings.skin));

    for (lib = botglobals.firstbotlib; lib; lib = lib->next)
    {
        lib->funcs.BotClientSettings(DF_ENTCLIENT(ent), &settings);
    } //end for
} //end of the function BotLib_BotClientSettings
//==========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//==========================================================================
static void BotLib_UpdateAllClientSettings(void)
{
    int i;
    edict_t *ent;

    for (i = 0; i < game.maxclients; i++)
    {
        ent = DF_CLIENTENT(i);
        if (!ent->inuse) continue;
        BotLib_BotClientSettings(ent);
    } //end for
} //end of the function BotLib_UpdateAllClientSettings
//==========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//==========================================================================
void BotLib_BotSettings(edict_t *bot, bot_settings_t *settings)
{
} //end of the function BotLib_BotSettings
//==========================================================================
// sends a client (state) update to the bot library
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//==========================================================================
void BotLib_BotUpdateClient(edict_t *bot)
{
    bot_updateclient_t buc;
    bot_library_t *lib;
    int i;

    if (!bot->inuse) return;
    lib = GetBotLibrary(bot);
    if (!lib) return;

    if (!bot->client)
    {
        gi.dprintf("client %d without client structure\n", DF_ENTCLIENT(bot));
        return;
    } //end if
    //movement type
    buc.pm_type = bot->client->ps.pmove.pm_type;
    //origin of the bot
    VectorCopy(bot->s.origin, buc.origin);
    //velocity of the bot
    VectorCopy(bot->velocity, buc.velocity);
    //pm_flags
    buc.pm_flags = bot->client->ps.pmove.pm_flags;
    //pm_time
    buc.pm_time = bot->client->ps.pmove.pm_time;
    //gravity
    buc.gravity = sv_gravity->value;
    //delta_angles (NOTE: the bot->client->ps.pmove.delta_angles are of type short)
    VectorCopy(bot->client->ps.pmove.delta_angles, buc.delta_angles);
    //====================================
    //view angles
    VectorClear(buc.viewangles);
    //view offset
    VectorCopy(bot->client->ps.viewoffset, buc.viewoffset);
    //kick angles
    VectorCopy(bot->client->ps.kick_angles, buc.kick_angles);
    //gun angles
    VectorCopy(bot->client->ps.gunangles, buc.gunangles);
    //gun offset
    VectorCopy(bot->client->ps.gunoffset, buc.gunoffset);
    //gun index
    buc.gunindex = bot->client->ps.gunindex;
    //gun frame
    buc.gunframe = bot->client->ps.gunframe;
    //blend
    for (i = 0; i < 4; i++) buc.blend[i] = bot->client->ps.blend[i];
    //field of vision
    buc.fov = bot->client->ps.fov;
    //rdflags
    buc.rdflags = bot->client->ps.rdflags;
    //
    // R-ENG-1 makes MAX_STATS 64 here and the contract's bot_updateclient_t
    // still carries the 1999 array, so the copy is bounded by the SMALLER of
    // the two.  A memcpy of MAX_STATS shorts into a 32-entry member is how a
    // struct that "obviously matches" overruns.
    memcpy(buc.stats, bot->client->ps.stats,
           min(q_countof(buc.stats), q_countof(bot->client->ps.stats)) * sizeof(short));
    //====================================
    //inventory
    memcpy(buc.inventory, bot->client->pers.inventory,
           min(q_countof(buc.inventory), q_countof(bot->client->pers.inventory)) * sizeof(int));
    //update the client
    lib->funcs.BotUpdateClient(DF_ENTCLIENT(bot), &buc);
    //====================================
    BotSetPMoveState(bot);
} //end of the function BotLib_BotUpdateClient
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotLib_BotStartFrame(float time)
{
    bot_library_t *lib;

    //NOTE: not really nice to put this call here ... but it's functional
    BotLib_UpdateAllClientSettings();
    //
    for (lib = botglobals.firstbotlib; lib; lib = lib->next)
    {
        //set the dmflags
        lib->funcs.BotLibVarSet("dmflags", dmflags->string);
        //start the server frame
        lib->funcs.BotStartFrame(time);
    } //end for
} //end of the function BotLib_BotStartFrame
//===========================================================================
// R-BOT-29: the rune -> tech translation, expressed against a table whose size
// is a runtime fact.  The brain has no concept of an OSP rune; it does know
// CTF's techs, so a rune is shown to it wearing the matching tech's model.
// Only under tourney, and only when runes are actually in play.
//===========================================================================
static int BotRuneModelindex(edict_t *ent)
{
    static const int rune_slots[5] = {
        SID_OSP_RUNE_RESIST, SID_OSP_RUNE_STRENGTH, SID_OSP_RUNE_HASTE,
        SID_OSP_RUNE_REGEN, SID_OSP_RUNE_VAMPIRE
    };
    int i, j;

    if (G_Ruleset() != RULESET_TOURNEY || !BotTourneyRunes())
        return 0;
    if (!ent->item || !(ent->item->flags & IT_RUNE))
        return 0;
    for (i = 0; i < 5; i++)
    {
        if (ent->item->quantity != rune_slots[i]) continue;
        //look the tech model up in the live index rather than assuming 251..255
        for (j = 1; j < bot_max_modelindexes; j++)
        {
            if (modelindexes[j] && !strcmp(modelindexes[j], bot_tech_models[i]))
                return j;
        } //end for
        break;
    } //end for
    return 0;
}
//===========================================================================
// sends an entity update to the bot library
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotLib_BotUpdateEntity(edict_t *ent)
{
    bot_updateentity_t bue;
    bot_library_t *lib;
    int tech;

    VectorCopy(ent->s.origin, bue.origin);
    VectorCopy(ent->s.angles, bue.angles);
    VectorCopy(ent->s.old_origin, bue.old_origin);
    VectorCopy(ent->mins, bue.mins);
    VectorCopy(ent->maxs, bue.maxs);
    bue.solid = ent->solid;
    bue.modelindex = ent->s.modelindex;
    bue.modelindex2 = ent->s.modelindex2;
    bue.modelindex3 = ent->s.modelindex3;
    bue.modelindex4 = ent->s.modelindex4;
    bue.frame = ent->s.frame;
    bue.skinnum = ent->s.skinnum;
    bue.effects = ent->s.effects;
    bue.renderfx = ent->s.renderfx;
    bue.sound = ent->s.sound;
    bue.event = ent->s.event;

    tech = BotRuneModelindex(ent);
    if (tech) bue.modelindex = tech;

    for (lib = botglobals.firstbotlib; lib; lib = lib->next)
    {
        if (lib->funcs.BotLibraryInitialized())
        {
            lib->funcs.BotUpdateEntity(DF_ENTNUMBER(ent), &bue);
        } //end if
    } //end for
    //if this is a client
    if (ent->client)
    {
        //if the entity has a sound
        if (ent->s.sound)
        {
            //send the sound seperately
            BotLib_BotAddSound(ent, CHAN_AUTO, ent->s.sound, 1.0f, ATTN_IDLE, 0);
        } //end if
    } //end if
} //end of the function BotLib_BotUpdateEntity
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotLib_BotAddSound(edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs)
{
    bot_library_t *lib;
    int entnum;

    // The donor's bound is the 1999 constant 255.  It is the SOUND TABLE's
    // bound, which R-BOT-11 makes a runtime fact, and the brain is handed the
    // whole table at BotLoadMap -- so clamping at 255 would hide every sound
    // above it on an extended server rather than reporting anything.
    if (soundindex < 0 || soundindex >= bot_max_soundindexes)
    {
        return;
    } //end if
    entnum = DF_ENTNUMBER(ent);
    for (lib = botglobals.firstbotlib; lib; lib = lib->next)
    {
        if (lib->funcs.BotLibraryInitialized())
        {
            lib->funcs.BotAddSound(ent->s.origin, entnum, channel, soundindex, volume, attenuation, timeofs);
        } //end if
    } //end for
} //end of the function BotLib_BotAddSound
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotLib_BotAddPointLight(vec3_t origin, int ent, float radius, float r, float g, float b, float time, float decay)
{
    bot_library_t *lib;

    for (lib = botglobals.firstbotlib; lib; lib = lib->next)
    {
        if (lib->funcs.BotLibraryInitialized())
        {
            lib->funcs.BotAddPointLight(origin, ent, radius, r, g, b, time, decay);
        } //end if
    } //end for
} //end of the function BotLib_BotAddPointLight
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotLib_BotAI(edict_t *bot, float thinktime)
{
    bot_library_t *lib;

    if (botglobals.nobotai) return;

    lib = GetBotLibrary(bot);
    if (!lib) return;
    lib->funcs.BotAI(DF_ENTCLIENT(bot), thinktime);
} //end of the function BotLib_BotAI
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotLib_BotConsoleMessage(edict_t *bot, int type, char *message)
{
    bot_library_t *lib;

    lib = GetBotLibrary(bot);
    if (!lib) return;
    lib->funcs.BotConsoleMessage(DF_ENTCLIENT(bot), type, message);
} //end of the function BotLib_BotConsoleMessage
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotLib_BotLibVarSet(char *var_name, char *value)
{
    bot_library_t *lib;

    for (lib = botglobals.firstbotlib; lib; lib = lib->next)
    {
        lib->funcs.BotLibVarSet(var_name, value);
    } //end for
} //end of the function BotLib_BotLibVarSet
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
int BotLib_Test(int parm0, char *parm1, vec3_t parm2, vec3_t parm3)
{
    bot_library_t *lib;

    for (lib = botglobals.firstbotlib; lib; lib = lib->next)
    {
        lib->funcs.Test(parm0, parm1, parm2, parm3);
    } //end for
    return 0;
} //end of the function BotLib_Test

//==========================================================================
//
// functions exported to the bot library
//
//==========================================================================

//===========================================================================
// stores the new bot input
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void BotLibImport_BotInput(int client, bot_input_t *bi)
{
    if (client < 0 || client >= game.maxclients)
    {
        gi.dprintf("BotInput: client number out of range\n");
        return;
    } //end if
    memcpy(&botglobals.botinputs[client], bi, sizeof(bot_input_t));
    botglobals.botnewinput[client] = true;
} //end of the function BotLibImport_BotInput
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void BotLibImport_Print(int type, char *fmt, ...)
{
    char str[MAX_STRING_CHARS];
    char warning[64] = "Warning: ";
    char error[64] = "Error: ";
    char fatal[64] = "Fatal: ";
    va_list ap;

    // Q_vsnprintf, not vsprintf: this is the brain's own format string and its
    // own arguments, and a 2 KiB stack buffer with no bound is how a brain bug
    // becomes a server bug (R-SEC).
    va_start(ap, fmt);
    Q_vsnprintf(str, sizeof(str), fmt, ap);
    va_end(ap);

    switch(type)
    {
        case PRT_MESSAGE:
        {
            gi.dprintf("%s", str);
            break;
        } //end case
        case PRT_WARNING:
        {
            StringMakeGreen(warning);
            gi.dprintf("%s%s", warning, str);
            break;
        } //end case
        case PRT_ERROR:
        {
            StringMakeGreen(error);
            gi.dprintf("%s%s", error, str);
            break;
        } //end case
        case PRT_FATAL:
        {
            StringMakeGreen(fatal);
            gi.dprintf("%s%s", fatal, str);
            break;
        } //end case
        case PRT_EXIT:
        {
            StringMakeGreen(str);
            gi.error("Exit: %s", str);
        } //end case
        default:
        {
            gi.dprintf("unknown print type\n");
            break;
        } //end case
    } //end switch
} //end of the function BotLibImport_Print
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void *BotLibImport_GetMemory(int size)
{
    void *ptr;
    //NOTE: don't use TAG_LEVEL, because all that memory will be freed
    //      at level changes. The game library doesn't change during level
    //      changes except for a LoadMap call. The game library assumes
    //      the allocated memory will stay during level changes, so the
    //      memory should not be freed in the game dll.
    if (size <= 0)
    {
        gi.dprintf("botlib asked for %d bytes\n", size);
        return NULL;
    } //end if
    ptr = gi.TagMalloc(size, TAG_GAME);
    if (!ptr)
    {
        gi.error("out of memory\n");
    } //end if
    return ptr;
} //end of the function BotLibImport_GetMemory
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void BotLibImport_FreeMemory(void *ptr)
{
    if (ptr) gi.TagFree(ptr);
} //end of the function BotLibImport_FreeMemory
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void BotFillTrace(bsp_trace_t *out, vec3_t start, vec3_t mins, vec3_t maxs,
                         vec3_t end, int passent, int contentmask)
{
    trace_t trace;
    edict_t *p;

    //just for the errors
    memset(out, 0, sizeof(*out));
    //check for valid passent entity number
    if (passent < 0 || passent >= game.maxentities)
    {
        gi.dprintf("BotLibTrace: invalid passent\n");
        return;
    } //end if
    p = DF_NUMBERENT(passent);
    //
    trace = gi.trace(start, mins, maxs, end, p, contentmask);
    // R-BOT-5: must tolerate a null trace.surface.  Q2PRO returns one for a
    // trace that hit nothing, and the donor memcpy'd through it unguarded.
    if (trace.surface)
    {
        memcpy(out->surface.name, trace.surface->name, sizeof(out->surface.name));
        out->surface.flags = trace.surface->flags;
        out->surface.value = trace.surface->value;
    } //end if
    out->allsolid = trace.allsolid;
    out->startsolid = trace.startsolid;
    out->fraction = trace.fraction;
    VectorCopy(trace.endpos, out->endpos);
    out->ent = trace.ent ? DF_ENTNUMBER(trace.ent) : 0;
    out->contents = trace.contents;
    memcpy(&out->plane, &trace.plane, sizeof(cplane_t));
}

// R-BOT-5, and see the long comment on the Trace slot in botlib.h: BY VALUE on
// every target, which is the published contract's spelling and, since
// gladiator-bot-restored 57ce85a3, the brain's on every target too.  No
// q_gameabi -- the brain's side carries no attribute, and the two have to agree.
static bsp_trace_t BotLibImport_Trace(vec3_t start, vec3_t mins, vec3_t maxs,
                                      vec3_t end, int passent, int contentmask)
{
    bsp_trace_t bsptrace;

    BotFillTrace(&bsptrace, start, mins, maxs, end, passent, contentmask);
    return bsptrace;
} //end of the function BotLibImport_Trace

//==========================================================================
//
// bot library loading, initialization etc.
//
//==========================================================================

//===========================================================================
// R-BOT-6/7/8 and doc/botlib-contract.md.  The set is fixed -- no new libvar
// may be invented, because the brain would ignore it -- and the ruleset chooses
// among the existing ones.  Pushed BEFORE BotSetupLibrary.
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void BotSetVarIfSet(bot_library_t *lib, const char *name, const char *value)
{
    cvar_t *cvar = gi.cvar(name, "", 0);

    if (cvar && cvar->value) lib->funcs.BotLibVarSet((char *)name, (char *)value);
}

// R-BOT-8: basedir, gamedir and cddir must resolve under Q2PRO, where `gamedir`
// is CVAR_ROM|CVAR_SERVERINFO and `basedir` may not exist at all.  Where they
// cannot be derived they come from FILESYSTEM_API_V1 and the library says so
// once at load.
static void BotSetPathVars(bot_library_t *lib)
{
    static bool said;
    const char *basedir, *gamedir;
    cvar_t *cvar;

    basedir = G_FsBaseDir();
    gamedir = G_FsGameDir();

    lib->funcs.BotLibVarSet("basedir", (char *)basedir);
    lib->funcs.BotLibVarSet("gamedir", (char *)gamedir);
    cvar = gi.cvar("cddir", "", 0);
    lib->funcs.BotLibVarSet("cddir", cvar ? cvar->string : "");

    if (!said)
    {
        gi.dprintf("Colosseum: botlib paths -- basedir \"%s\", gamedir \"%s\" "
                   "(R-BOT-8)\n", basedir, gamedir);
        said = true;
    } //end if
}

static int BotInitLibrary(bot_library_t *lib)
{
    int err;
    cvar_t *cvar;
    char buf[144];

    //set the maxclients and maxentities library variables before calling BotSetupLibrary
    // R-BOT-30: game.maxclients, not maxclients->value -- the cvar is what was
    // asked for and game.maxclients is what was allocated.
    lib->funcs.BotLibVarSet("maxclients", va("%d", game.maxclients));
    lib->funcs.BotLibVarSet("maxentities", va("%d", game.maxentities));
    //maximum number of aas links
    cvar = gi.cvar("max_aaslinks", "", 0);
    if (cvar && cvar->value > 0) lib->funcs.BotLibVarSet("max_aaslinks", cvar->string);
    //maximum number of bsp links
    cvar = gi.cvar("max_bsplinks", "", 0);
    if (cvar && cvar->value > 0) lib->funcs.BotLibVarSet("max_bsplinks", cvar->string);
    //maximum number of items in a level
    cvar = gi.cvar("max_levelitems", "", 0);
    if (cvar && cvar->value > 0) lib->funcs.BotLibVarSet("max_levelitems", cvar->string);
    //automatically launch WinBSPC if AAS file not available
    BotSetVarIfSet(lib, "autolaunchbspc", "1");
    //deathmatch flags
    lib->funcs.BotLibVarSet("dmflags", dmflags->string);
    Q_snprintf(buf, sizeof(buf), "DMFLAGS %s", dmflags->string);
    lib->funcs.BotDefine(buf);

    // ---- the ruleset's own libvars (R-BOT-7) ------------------------------
    //
    // The donor fenced these five with #ifdef ZOID / CH / ROCKETARENA / XATRIX
    // / ROGUE, which is one build per ruleset.  One library serves five, so the
    // fences become the resolution's answer (sec 7 rule 6).  No libvar is
    // invented and none is dropped: every name below is in R-BOT-6's fixed set
    // and every one is pushed on every ruleset, so the brain always sees the
    // same 32 names and only their values move.
    lib->funcs.BotLibVarSet("ctf", G_Ruleset() == RULESET_CTF ? "1" : "0");
    lib->funcs.BotLibVarSet("ra", G_Ruleset() == RULESET_ARENA ? "1" : "0");
    // N7: Colored Hitman is out of scope, and the brain still reads the libvar.
    lib->funcs.BotLibVarSet("ch", "0");
    lib->funcs.BotLibVarSet("xatrix", G_LayerEnabled(LAYER_XATRIX) ? "1" : "0");
    lib->funcs.BotLibVarSet("rogue", G_LayerEnabled(LAYER_ROGUE) ? "1" : "0");
    lib->funcs.BotLibVarSet("assimilation", "0");
    lib->funcs.BotLibVarSet("teamplay_shell", "0");

    //log file.  R-BOT-30: the donor's port changed the default from 1 to 0 and
    //that is policy rather than correctness -- adopted deliberately, because a
    //public server should not write an AI trace by default.
    cvar = gi.cvar("log", "0", 0);
    lib->funcs.BotLibVarSet("log", cvar->string);
    //no chatting
    BotSetVarIfSet(lib, "nochat", "1");
    //fast chatting.  The donor pushes "0" when the cvar is SET, and 1.22 first
    //carried that with a comment rationalising it as "the switch turns the WAIT
    //off".  It does not: the brain reads `if (fastchat->value == 0.0f)` and only
    //THEN applies the random gate that decides whether a bot says anything, so
    //fastchat != 0 means "always chat".  Pushing "0" leaves the gate on, which
    //makes `fastchat 1` on the server unable to do the one thing it is named
    //for -- and every neighbour in this block pushes "1".  It is a donor typo,
    //not a subtlety, and it is what made the chat half of Phase 6's exit
    //unobservable (doc/reconciliation.md R-102).
    BotSetVarIfSet(lib, "fastchat", "1");
    //alternative names
    BotSetVarIfSet(lib, "altnames", "1");
    //enable rocket jumping
    cvar = gi.cvar("rocketjump", "1", 0);
    if (cvar && cvar->value) lib->funcs.BotLibVarSet("rocketjump", "1");
    //forced clustering calculations
    BotSetVarIfSet(lib, "forceclustering", "1");
    //forced reachability calculations
    BotSetVarIfSet(lib, "forcereachability", "1");
    //force writing of AAS to file
    BotSetVarIfSet(lib, "forcewrite", "1");
    //no AAS optimization
    BotSetVarIfSet(lib, "nooptimize", "1");
    //number of reachabilities to calculate each frame
    cvar = gi.cvar("framereachability", "20", 0);
    lib->funcs.BotLibVarSet("framereachability", cvar->string);
    //base, game and cd directory
    BotSetPathVars(lib);

    // ---- the hook, the runes and teamplay ---------------------------------
    //
    // R-BOT-29's libvar block.  Under tourney these come from hook_enable and
    // rune_stat; under ctf from Threewave's own ctf_hook/laserhook, which
    // R-CTF-3 already registers; elsewhere from the MOD_HOOK modifier.
    switch (G_Ruleset()) {
    case RULESET_TOURNEY:
        lib->funcs.BotLibVarSet("usehook", BotTourneyHook() ? "1" : "0");
        lib->funcs.BotLibVarSet("laserhook", BotTourneyHook() ? "1" : "0");
        // The comparison is preserved EXACTLY: 1v1 (m_mode 3) is two teams of
        // one, the brain has no ally, and teamplay 0 is the right answer there.
        lib->funcs.BotLibVarSet("teamplay", BotTourneyMode() == MODE_TEAM ? "1" : "0");
        lib->funcs.BotLibVarSet("runes", BotTourneyRunes() ? "1" : "0");
        lib->funcs.BotLibVarSet("techs", "0");
        break;
    case RULESET_CTF:
        lib->funcs.BotLibVarSet("usehook", ctf_hook && ctf_hook->value ? "1" : "0");
        lib->funcs.BotLibVarSet("laserhook", laserhook && laserhook->value ? "1" : "0");
        lib->funcs.BotLibVarSet("teamplay", "1");
        lib->funcs.BotLibVarSet("runes", "0");
        // CTFSetupTechSpawn gates the techs on DF_CTF_NO_TECH alone, so that
        // dmflag -- not the `runes` modifier -- is what the brain must be told
        // about.  See doc/reconciliation.md R-88 and R-94.
        lib->funcs.BotLibVarSet("techs",
            ((int)dmflags->value & DF_CTF_NO_TECH) ? "0" : "1");
        break;
    default:
        lib->funcs.BotLibVarSet("usehook", G_ModifierEnabled(MOD_HOOK) ? "1" : "0");
        lib->funcs.BotLibVarSet("laserhook", "0");
        lib->funcs.BotLibVarSet("teamplay", G_TeamplayEnabled() ? "1" : "0");
        lib->funcs.BotLibVarSet("runes", "0");
        lib->funcs.BotLibVarSet("techs", "0");
        break;
    }

    //setup the bot library
    err = lib->funcs.BotSetupLibrary();
    if (err != BLERR_NOERROR) return false;
    //load the map
    err = lib->funcs.BotLoadMap(level.mapname,
                                bot_max_modelindexes, modelindexes,
                                bot_max_soundindexes, soundindexes,
                                bot_max_imageindexes, imageindexes);
    if (err != BLERR_NOERROR) return false;
    return true;
} //end of the function BotInitLibrary
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void BotUnloadLibrary(bot_library_t *lib)
{
    //unlink library from list
    if (lib->prev) lib->prev->next = lib->next;
    else botglobals.firstbotlib = lib->next;
    if (lib->next) lib->next->prev = lib->prev;
    //shut down the library
    lib->funcs.BotShutdownLibrary();
#if defined(_WIN32)
    //Win32 free the bot library
    FreeLibrary(lib->handle);
#else
    //free the shared object
    dlclose(lib->handle);
#endif
    //free the memory of the library structure
    gi.TagFree(lib);
} //end of the function BotUnloadLibrary
//===========================================================================
// R-BOT-4: the 1999 binaries are not a target.  The brain is compiled from
// gladiator-bot-restored/botlib for whichever platform the game was built for,
// so the default name follows the same CPUSTRING/SHLIBEXT convention the engine
// uses for the game library itself rather than the 1999 "gladi386.so".
//===========================================================================
const char *BotDefaultLibrary(void)
{
    // The 1999 defaults were `gladiator.dll` and `gladi386.so`, and the second
    // is a name for a 32-bit x86 object -- which R-BOT-4 says is not a target,
    // because gladiator-bot-restored compiles the brain for whichever platform
    // the game was built for.  The name it produces is `gladiator.so`, so that
    // is the default and the CPU does not appear in it: one gamedir holds one
    // brain, the loader reports the bitness of both sides when they disagree,
    // and `botlib` overrides the name for anyone who wants two.
#if defined(_WIN32)
    return "gladiator.dll";
#else
    return "gladiator.so";
#endif
}
//===========================================================================
// bot library loading
//
// NOTE: this is platform dependent code
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
typedef bot_export_t *(*PFNGetBotAPI)(bot_import_t *import);

static bot_library_t *BotLoadLibrary(const char *botlibdir)
{
    bot_library_t *lib;
    PFNGetBotAPI GetBotAPI;
    void *botlibhandle;
    bot_export_t *exports;

#if defined(_WIN32)
    botlibhandle = LoadLibraryA(botlibdir);
    if (!botlibhandle)
    {
        // R-BOT-4: report the reason AND the bitness of both sides, and refuse
        // the bot rather than killing the server.  A mismatched brain is the
        // failure this message exists for, and "couldn't load" alone does not
        // say which of the two is 32-bit.
        gi.dprintf("couldn't load %s (game is %d-bit; a brain of the other "
                   "word size cannot be loaded)\n",
                   botlibdir, (int)(sizeof(void *) * 8));
        return NULL;
    } //end if
    GetBotAPI = (PFNGetBotAPI)(void *)GetProcAddress(botlibhandle, "GetBotAPI");
    if (!GetBotAPI)
    {
        FreeLibrary(botlibhandle);
        gi.dprintf("couldn't find GetBotAPI in %s\n", botlibdir);
        return NULL;
    } //end if
#else
    botlibhandle = dlopen(botlibdir, RTLD_NOW);
    if (!botlibhandle)
    {
        gi.dprintf("couldn't load %s: %s (game is %d-bit; a brain of the other "
                   "word size cannot be loaded)\n",
                   botlibdir, dlerror(), (int)(sizeof(void *) * 8));
        return NULL;
    } //end if
    dlerror();
    GetBotAPI = (PFNGetBotAPI)dlsym(botlibhandle, "GetBotAPI");
    if (!GetBotAPI)
    {
        gi.dprintf("couldn't find GetBotAPI in %s: %s\n", botlibdir, dlerror());
        dlclose(botlibhandle);
        return NULL;
    } //end if
#endif

    lib = gi.TagMalloc(sizeof(bot_library_t), TAG_GAME);
    memset(lib, 0, sizeof(bot_library_t));
    Q_strlcpy(lib->path, botlibdir, sizeof(lib->path));
    lib->handle = botlibhandle;
    exports = GetBotAPI(&botglobals.gamebotimport);
    if (!exports)
    {
        gi.dprintf("GetBotAPI in %s returned nothing\n", botlibdir);
        gi.TagFree(lib);
#if defined(_WIN32)
        FreeLibrary(botlibhandle);
#else
        dlclose(botlibhandle);
#endif
        return NULL;
    } //end if
    lib->funcs = *exports;
    //add the library to the list
    lib->next = botglobals.firstbotlib;
    lib->prev = NULL;
    if (botglobals.firstbotlib) botglobals.firstbotlib->prev = lib;
    botglobals.firstbotlib = lib;
    //initialize library
    if (!BotInitLibrary(lib))
    {
        BotUnloadLibrary(lib);
        return NULL;
    } //end if
    //
    gi.dprintf("loaded %s (%s)\n", botlibdir,
               lib->funcs.BotVersion ? lib->funcs.BotVersion() : "no version");
    // R-BOT-5, risk 6: a Test() round trip at load, which is the only check
    // either side has that the by-value bsp_trace_t and the pointer-bearing
    // tables agree.  botglobals.notest turns it off for a brain that has none.
    if (!botglobals.notest && lib->funcs.Test)
    {
        vec3_t a = { 1, 2, 3 }, b = { 4, 5, 6 };
        lib->funcs.Test(0, "colosseum", a, b);
    } //end if
    return lib;
} //end of the function BotLoadLibrary
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotUnloadAllLibraries(void)
{
    bot_library_t *lib;

    for (lib = botglobals.firstbotlib; lib; lib = botglobals.firstbotlib)
    {
        BotUnloadLibrary(lib);
    } //end for
} //end of the function BotUnloadAllLibraries
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
bot_library_t *BotUseLibrary(const char *path)
{
    char botlibdir[BOT_MAX_PATH] = "";
    bot_library_t *lib;

    // R-BOT-3's search order: the path as given, then basedir + gamedir.  The
    // donor decides between them with `access(path, 4)`, and the obvious
    // modernisation -- ask the engine's filesystem whether it can read the
    // file -- is WRONG and was wrong in the first run of tools/botmatrix.sh:
    // the engine finds `gladiator.so` in the gamedir and answers yes, so the
    // bare name went to dlopen, which searches the LINKER's paths and not the
    // gamedir, and every bot failed to load with "cannot open shared object
    // file".  dlopen needs an OS path, so the question is not "can this be
    // read" but "is this already a path" -- which is what a separator says.
    if (!strchr(path, '/') && !strchr(path, '\\'))
    {
        //get the base directory
        Q_strlcpy(botlibdir, G_FsBaseDir(), sizeof(botlibdir));
        AppendPathSeperator(botlibdir, BOT_MAX_PATH);
        //user specified game directory
        Q_strlcat(botlibdir, G_FsGameDir(), sizeof(botlibdir));
        AppendPathSeperator(botlibdir, BOT_MAX_PATH);
    } //end if
    //the dll name
    Q_strlcat(botlibdir, path, sizeof(botlibdir));
    //check if the library is loaded already
    for (lib = botglobals.firstbotlib; lib; lib = lib->next)
    {
        if (!Q_stricmp(lib->path, botlibdir))
        {
            lib->users++;
            return lib;
        } //end if
    } //end for
    // The donor refuses a SECOND library outright -- `if (firstbotlib) return
    // NULL` -- which R-BOT-3's per-bot naming makes look like a bug and is not:
    // one process cannot hold two AAS worlds, and the second brain would
    // silently share the first one's. Kept, with the refusal now reported.
    if (botglobals.firstbotlib)
    {
        gi.dprintf("a bot library is already loaded (%s); one per server "
                   "(R-BOT-3)\n", botglobals.firstbotlib->path);
        return NULL;
    } //end if
    lib = BotLoadLibrary(botlibdir);
    if (lib) lib->users++;
    return lib;
} //end of the function BotUseLibrary
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotFreeLibrary(bot_library_t *lib)
{
    cvar_t *freebotlib;

    if (!lib) return;
    lib->users--;
    if (lib->users <= 0)
    {
        freebotlib = gi.cvar("freebotlib", "1", 0);
        if (freebotlib->value) BotUnloadLibrary(lib);
    } //end if
} //end of the function BotFreeLibrary
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotLibraryDump(void)
{
    bot_library_t *lib;
    bot_state_t *bs;
    edict_t *ent;
    int i;

    gi.dprintf("Library Dump:\n");
    if (!botglobals.firstbotlib)
    {
        gi.dprintf("no libraries found\n");
        return;
    } //end if
    for (lib = botglobals.firstbotlib; lib; lib = lib->next)
    {
        gi.dprintf("-------------------------------------\n");
        gi.dprintf("%s (%d user%s)\n", lib->path, lib->users,
                   lib->users == 1 ? "" : "s");
        for (i = 0; i < game.maxclients; i++)
        {
            bs = &botglobals.botstates[i];
            if (!bs->active) continue;
            if (bs->library != lib) continue;
            ent = DF_CLIENTENT(i);
            gi.dprintf("    client %3d: %s\n", i,
                       ent->client ? ent->client->pers.netname : "?");
        } //end for
    } //end for
} //end of the function BotLibraryDump
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static const char *Ptr2PathWithMaxSize(const char *path, int size)
{
    int length;
    const char *ptr, *bestptr;

    bestptr = path;
    length = strlen(bestptr);
    if (length > size)
    {
        ptr = &bestptr[length - 1];
        bestptr = NULL;
        for (length = 0; length < size; length++)
        {
            if (*ptr == '\\' || *ptr == '/') bestptr = ptr;
            ptr--;
        } //end for
        if (!bestptr) bestptr = ptr;
    } //end if
    return bestptr;
} //end of the function Ptr2PathWithMaxSize
//===========================================================================
// `sv clientdump` -- R-VER-3 reads this to prove no client slot leaked.
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotClientDump(void)
{
    edict_t *ent;
    int i;
    const char *path;

    for (i = 0; i < game.maxclients; i++)
    {
        ent = DF_CLIENTENT(i);
        if (!ent->inuse)
        {
            gi.dprintf("%3d: -\n", i);
        } //end if
        else if (ent->flags & FL_BOT)
        {
            gi.dprintf("%3d: %-16s ", i, ent->client->pers.netname);
            // The donor indexes botstates[i].library unguarded, which is a
            // null dereference for a client that carries FL_BOT with no
            // library -- exactly the state BotDestroy passes through.
            if (!botglobals.botstates[i].library)
            {
                gi.dprintf("(no library)\n");
                continue;
            } //end if
            path = botglobals.botstates[i].library->path;
            if (strlen(path) > 25)
            {
                //minus three for the dots
                path = Ptr2PathWithMaxSize(path, 25 - 3);
                gi.dprintf("...");
            } //end if
            gi.dprintf("%s\n", path);
        } //end if
        else
        {
            gi.dprintf("%3d: %-16s human\n", i, ent->client->pers.netname);
        } //end else
    } //end for
    gi.dprintf("%d bot%s, %d client slot%s\n", botglobals.numbots,
               botglobals.numbots == 1 ? "" : "s",
               game.maxclients, game.maxclients == 1 ? "" : "s");
} //end of the function BotClientDump
//===========================================================================
// setup the bot library import functions
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void BotSetupBotLibImport(void)
{
    bot_import_t gamebotimport;

    memset(&gamebotimport, 0, sizeof(gamebotimport));
    gamebotimport.BotInput = BotLibImport_BotInput;
    gamebotimport.BotClientCommand = BotClientCommand;      //bl_redirgi.c
    gamebotimport.Print = BotLibImport_Print;
    gamebotimport.Trace = BotLibImport_Trace;
    gamebotimport.PointContents = gi.pointcontents;
    gamebotimport.GetMemory = BotLibImport_GetMemory;
    gamebotimport.FreeMemory = BotLibImport_FreeMemory;
    //debug lines
    gamebotimport.DebugLineCreate = DebugLineCreate;        //bl_debug.c
    gamebotimport.DebugLineDelete = DebugLineDelete;        //bl_debug.c
    gamebotimport.DebugLineShow = DebugLineShow;            //bl_debug.c
    //
    botglobals.gamebotimport = gamebotimport;
} //end of the function BotSetupBotLibImport
//===========================================================================
// initialize bot globals and allocate bot states
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotSetup(void)
{
    static bool announced;

    //allocate memory for the bot states
    botglobals.botstates = gi.TagMalloc(game.maxclients * sizeof(bot_state_t), TAG_GAME);
    memset(botglobals.botstates, 0, game.maxclients * sizeof(bot_state_t));
    //allocate memory for the bot input
    botglobals.botinputs = gi.TagMalloc(game.maxclients * sizeof(bot_input_t), TAG_GAME);
    memset(botglobals.botinputs, 0, game.maxclients * sizeof(bot_input_t));
    //allocate memory for the array with new input flags
    botglobals.botnewinput = gi.TagMalloc(game.maxclients * sizeof(bool), TAG_GAME);
    memset(botglobals.botnewinput, 0, game.maxclients * sizeof(bool));
    //number of bots currently in the game
    botglobals.numbots = 0;
    //setup the bot library import structure
    BotSetupBotLibImport();
    //R-BOT-11: the index tables, now that game.csr is chosen
    BotIndexesAlloc();
    //R-BOT-27: pick the debug-line implementation and say which one
    BotDebugInit();

    // R-BOT-22: FRAMETIME is 0.1 s and Colosseum does not advertise
    // GMF_VARIABLE_FPS.  If the engine is running at another rate the bots'
    // think time and the brain's physics libvars disagree, so say so once.
    //
    // "Once" is now load-bearing: ReadGame calls BotSetup again after its
    // FreeTags(TAG_GAME), so this runs twice per savegame load and the warning
    // is about the server, not about the call.
    if (!announced && gi.cvar("sv_fps", "10", 0)->value != 1.0f / FRAMETIME)
    {
        gi.dprintf("Colosseum: sv_fps is %s and the bot layer assumes %.0f "
                   "(R-BOT-22); bot movement will be wrong\n",
                   gi.cvar("sv_fps", "10", 0)->string, 1.0f / FRAMETIME);
    } //end if
    announced = true;
} //end of the function BotSetup
//===========================================================================
// ShutdownGame.  R-BOT-3 requires BotUnloadAllLibraries here; the bots
// themselves go first, so that the brain sees a shutdown per client before its
// library is closed rather than being unmapped underneath them.
//===========================================================================
void BotShutdown(void)
{
    BotDestroyAll();
    BotUnloadAllLibraries();
    bot_MenuDestroy();
    memset(&botglobals, 0, sizeof(botglobals));
    BotIndexesForget();
}
//===========================================================================
// Every TAG_GAME allocation this layer owns has just been freed by somebody
// else -- ReadGame opens with gi.FreeTags(TAG_GAME) -- so drop the pointers
// without freeing them.  Five owners, and they were found one crash at a time:
// the index tables (SpawnEntities precached into freed memory and
// BotInitMuzzleFlashToSoundindex read a string pointer out of it) and the menu
// tree (bot_MenuDestroy walked it at ShutdownGame).  The roster and the queue
// are here because gi.TagFree is how LoadBots and the queue drain release them,
// which after a FreeTags is a double free rather than a crash -- the quiet one.
//
// This is the counterpart of BotSetup and the two are called as a pair.
//===========================================================================
void BotForgetGameMemory(void)
{
    memset(&botglobals, 0, sizeof(botglobals));
    BotIndexesForget();
    BotListForget();
    BotQueueForget();
    bot_MenuForget();
}
//===========================================================================
// R-BOT-20: the frame order, in one place.
//
//   1. AddQueuedBots()
//   2. BotStartFrame(level.time)
//   3. the entity loop            -- G_RunFrame's, before this is called
//   4. BotUpdateEntity() for every inuse edict without SVF_NOCLIENT
//   5. per bot, in slot order: BotUpdateClient -> BotAI -> BotExecuteInput
//   6. CheckMinimumPlayers()
//   7. CheckDMRules()             -- G_RunFrame's, after this returns
//
// Steps 4 and 5 must not interleave: the brain is entitled to a complete world
// snapshot before any bot thinks.  Two loops, and this comment, are why.
//===========================================================================
void BotRunFrame(void)
{
    int i;
    edict_t *ent;

    // Steps 4 and 5 cost a walk of every edict, so they are skipped entirely
    // when no bot exists -- which is R-BOT-13's transparency for the frame
    // loop, and what keeps a bot-free server paying nothing for the layer.
    if (botglobals.numbots <= 0)
        return;

    //4: the world snapshot
    ent = &g_edicts[0];
    for (i = 0; i < globals.num_edicts; i++, ent++)
    {
        if (!ent->inuse) continue;
        if (ent->svflags & SVF_NOCLIENT) continue;
        BotLib_BotUpdateEntity(ent);
    } //end for

    //5: and only then, the bots
    for (i = 0; i < game.maxclients; i++)
    {
        ent = DF_CLIENTENT(i);
        if (!ent->inuse) continue;
        if (!(ent->flags & FL_BOT)) continue;
        if (!BotStarted(ent)) continue;
        BotLib_BotUpdateClient(ent);
        BotLib_BotAI(ent, FRAMETIME);
        BotExecuteInput(ent);
    } //end for

    //the debug lines the brain asked for, re-issued if the engine's extension
    //is what is drawing them
    BotDebugFrame();
} //end of the function BotRunFrame
