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
// Game-import redirection, from osp-tourney@1d8427e (SPECS.md sec 5.4.3).
// The reconstruction's asm-matching address comments are stripped -- SPECS.md
// N1 makes those oracles meaningless here, and they survive at the pin.
//===========================================================================
//
// Name:                bl_redirgi.c
// Function:        redirect the game import structure
// Programmer:      Mr Elusive (MrElusive@demigod.demon.nl)
// Last update: 1999-02-10
// Tab Size:        3
//===========================================================================

#include "g_local.h"
#include "bot/bl_main.h"
#include "bot/bl_redirgi.h"

#define MAX_COMMANDARGUMENTS            20
// R-BOT-12: "at least the engine's maximum message size".  Q2PRO's MAX_MSGLEN
// is 0x8000 (inc/common/protocol.h, which the game library does not include)
// and one gi.WriteString of a scoreboard page is already over 1400 bytes, so
// the 1999 constant of 2048 was under the size of a single legal message.  The
// staging records below add one type byte per write, so the buffer is sized
// above MAX_MSGLEN rather than at it.
#define MAX_NETWORKMESSAGE              (0x8000 + 4096)
// MZ_NUKE8 is 38, not 31.  The 1999 constant of 32 predates Ground Zero's
// flashes, and with it every rogue row in the table below indexes past the end
// of muzzleflashsoundindex[].  That used to be harmless only because all ten of
// those rows had no sound; R-183 gave two of them one (MZ_ETF_RIFLE at 30 and
// MZ_TRACKER at 35), so the write at muzzleflashsoundindex[mf] now actually
// happens above 31 and this constant is the only reason it lands inside the
// array.
#define MAX_MUZZLEFLASHES               64

//the gi will be redirected through botimport
game_import_t newgameimport;
//command arguments for the bots
static char *commandarguments[MAX_COMMANDARGUMENTS];
static char commandline[150]; //max see g_cmds.c
//the model, sound and image indexes -- R-BOT-11, sized from game.csr
char **modelindexes;
char **soundindexes;
char **imageindexes;
int    bot_max_modelindexes;
int    bot_max_soundindexes;
int    bot_max_imageindexes;
//soundindex of sound played with muzzleflash
static int muzzleflashsoundindex[MAX_MUZZLEFLASHES];

/*
===============================================================================

THE STAGING MESSAGE, AND WHY IT HOLDS TYPED RECORDS RATHER THAN BYTES

R-BOT-10 asks for the writes to be "buffered into one staging message so
multicast/unicast can read it before it goes out", and R-BOT-13 asks for the
redirection to be transparent -- with numbots == 0, every redirected slot must
behave exactly as if the engine had been called directly.  The 1999 design
cannot satisfy both against Q2PRO, and measuring it is what says so:

  * `gi.WritePosition` is not three shorts here.  Q2PRO's PF_WritePos calls
    MSG_WritePos(pos, extended && IS_NEW_GAME_API), and with protocol
    extensions negotiated that is three *DeltaInt23* values, not three shorts.
    The donor re-encodes the buffer with its own WriteShort pair and hands the
    result back to the engine one byte at a time, so every position in every
    multicast -- every splash, every railtrail, every explosion -- would come
    out corrupt on an extended server, and only on an extended server.
  * `gi.WriteFloat` does not exist.  PF_WriteFloat is `Com_Error(ERR_DROP,
    "PF_WriteFloat not implemented")`.  The donor's Bot_WriteFloat buffers four
    bytes happily, which turns an engine abort into silent corruption.
  * `gi.WriteDir` is DirToByte over the engine's own normal table.  Carrying
    anorms.h to recompute it is a second implementation of an encoding that
    only has to agree by inspection.

So the staging message records what was written, not how the engine would
encode it: one type byte and the argument, replayed through newgameimport's own
writers at flush.  The engine therefore receives exactly the call sequence it
would have received unredirected, in the same order, which makes R-BOT-13 true
by construction instead of by hoping two encoders match.  It also makes the
muzzle-flash sniff read a short as a short rather than parsing bytes back out
of a buffer, and it drops anorms.h from the tree entirely.

`doc/reconciliation.md` R-93.

===============================================================================
*/

typedef enum {
    BW_CHAR, BW_BYTE, BW_SHORT, BW_LONG, BW_FLOAT,
    BW_STRING, BW_POSITION, BW_DIR, BW_ANGLE
} bot_writekind_t;

typedef struct {
    int      writepos;                      // cursor for the next record
    bool     overflowed;                    // a write did not fit
    byte     data[MAX_NETWORKMESSAGE];
} bot_networkmessage_t;

static bot_networkmessage_t networkmessage;

//muzzle flash information
typedef struct bot_muzzleflashinfo_s
{
    int muzzleflash;        //muzzle flash number
    const char *sound;      //name of the sound
    float radius;           //light radius
    float r, g, b;          //rgb light color
    float time;             //light alive time
    float decay;            //light decay
} bot_muzzleflashinfo_t;

/*

g_weapons.c
-----------

//Grenade_Touch
//GRENade Launcher Bounce 1b
gi.sound (ent, CHAN_VOICE, gi.soundindex ("weapons/grenlb1b.wav"), 1, ATTN_NORM, 0);
//bfg_touch
//BFG eXplosion 1b (bfg ball hit)
gi.sound (self, CHAN_VOICE, gi.soundindex ("weapons/bfg__x1b.wav"), 1, ATTN_NORM, 0);
//fire_bfg
//BFG Launch 1a (bfg ball movement)
bfg->s.sound = gi.soundindex ("weapons/bfg__l1a.wav");
//fire_rocket
//"weapons/rockfly.wav"     //ROCKet FLY sound
rocket->s.sound = gi.soundindex ("weapons/rockfly.wav");

"weapons/grenlx1a.wav"      //GRENade Launcher eXplosion 1a
"weapons/rocklx1a.wav"      //ROCKet Launcher eXplosion 1a

*/

// The mission-pack rows are NOT behind a layer test.  A muzzle flash number is
// a wire constant, the two packs' numbers do not overlap baseq2's, and a row
// whose sound is NULL costs one comparison at level load and nothing after --
// so the table is the union and the layers select themselves by which flashes
// the world actually produces (R-MODE-3).  The donor's #ifdef XATRIX / #ifdef
// ROGUE fences go with them (sec 7 rule 6).
static bot_muzzleflashinfo_t muzzleflashinfo[MAX_MUZZLEFLASHES] =
{
//Blaster
    {MZ_BLASTER,        "weapons/blastf1a.wav"},    //BLASTer Fire
//Shotgun
    {MZ_SHOTGUN,        "weapons/shotgf1b.wav"},    //SHOTGun Fire 1b
//Super Shotgun
    {MZ_SSHOTGUN,       "weapons/sshotf1b.wav"},    //Super SHOTgun Fire 1b
//Machinegun
    {MZ_MACHINEGUN,     "weapons/machgf1b.wav"},    //MACHineGun Fire 1b (tock)
//Chaingun
    {MZ_CHAINGUN1,      "weapons/machgf1b.wav"},    //firing one bullet
    {MZ_CHAINGUN2,      "weapons/machgf1b.wav"},    //firing two bullets
    {MZ_CHAINGUN3,      "weapons/machgf1b.wav"},    //firing three bullets
//Grenade Launcher
    {MZ_GRENADE,        "weapons/grenlf1a.wav"},    //GRENade Launcher Fire 1a
//Rocket launcher
    {MZ_ROCKET,         "weapons/rocklf1a.wav"},    //ROCKet Launcher Fire 1a
//Hyperblaster
    {MZ_HYPERBLASTER,   "weapons/hyprbf1a.wav"},    //HYPeRBlaster Fire 1a
//Railgun
    {MZ_RAILGUN,        "weapons/railgf1a.wav"},    //RAILGun Fire 1a
//BFG
    {MZ_BFG,            "weapons/bfg__f1y.wav"},    //BFG Fire 1y
    //
    {MZ_LOGIN,          NULL},
    {MZ_LOGOUT,         NULL},
    {MZ_RESPAWN,        "misc/spawn1.wav"},
    {MZ_ITEMRESPAWN,    "items/respawn1.wav"},
    // R-183.  Every mission-pack row arrived NULL and stayed NULL, so a bot
    // could not HEAR those weapons fire: BotInitMuzzleFlashToSoundindex leaves
    // muzzleflashsoundindex[] at 0 for a row with no sound, and a flash the
    // brain has no sound for is a shot it never notices.
    //
    // Every name below is the ENGINE'S, read out of q2pro's CL_MuzzleFlash.
    // This table is the brain's copy of the client's, so a name invented here
    // would have the bot listening for a sound no client plays -- and the name
    // has to be one the GAME precaches too, because the lookup below resolves
    // it against the live configstring sound table and an unprecached name
    // silently resolves to 0.  All five are in the itemlist's own precache
    // strings or in an explicit gi.soundindex.
    //RAFAEL
    {MZ_IONRIPPER,      "weapons/rippfire.wav"},
    {MZ_BLUEHYPERBLASTER, "weapons/hyprbf1a.wav"},
    {MZ_PHALANX,        "weapons/plasshot.wav"},
    //END RAFAEL
    //ROGUE
    {MZ_ETF_RIFLE,      "weapons/nail1.wav"},
    // THE REMAINING ROGUE ROWS STAY NULL, each for its own reason, and none of
    // them is an omission:
    //
    //  * MZ_HEATBEAM and the four NUKES: the ENGINE plays nothing.
    //    CL_MuzzleFlash's MZ_HEATBEAM case has its S_StartSound commented out
    //    and the nuke cases only light the room.  A sound here would be one
    //    that only the bot could hear.
    //  * MZ_PROX, MZ_SHOTGUN2 and MZ_BLASTER2: this GAME never writes them.
    //    They are engine enum values (31, 32, 34) that no gi.WriteByte in the
    //    tree emits, so a sound on them could never fire.  Listed anyway, with
    //    the reason, so the next reader does not take the gap for the same
    //    oversight the four rows above it were.
    //  * MZ_SHOTGUN2 additionally has NO single right answer: the remaster
    //    overloads it as MZ_ETF_RIFLE_2, so an extended client plays nail1.wav
    //    and a vanilla one shotg2.wav.  A static row would be wrong for one of
    //    them, and the game precaches neither under that flash.
    {MZ_PROX,           NULL},
    {MZ_SHOTGUN2,       NULL},
    {MZ_HEATBEAM,       NULL},
    {MZ_BLASTER2,       NULL},
    {MZ_TRACKER,        "weapons/disint2.wav"},
    {MZ_NUKE1,          NULL},
    {MZ_NUKE2,          NULL},
    {MZ_NUKE4,          NULL},
    {MZ_NUKE8,          NULL},
    //END ROGUE
    // MZ_SILENCED is a BIT ORed with one of the numbers above, not a flash of
    // its own; the donor listed it as a row and the sniff below masks it off.
    {-1, NULL}
};

// R-BOT-29: the rune->tech translation.  The donor rewrote bue.modelindex to a
// fixed 251..255 so the brain would recognise an OSP rune as a CTF tech; those
// constants are indexes into the 1999 256-entry table and mean nothing once
// R-BOT-11 sizes the table from game.csr.  The models are looked up by name in
// the live table instead, which is the same translation expressed against a
// table whose size is a runtime fact.
//
// R-184: THERE IS NO FIFTH TECH.  OSP has five runes and Threewave has four
// techs, and the fifth row named `models/ctf/vampire/tris.md2` -- a path that
// exists in no pak any donor ships.  The lookup below is a strcmp against the
// live modelindex table, so it could never match and a vampire rune was already
// reaching the brain as modelindex 0; the name only made it look otherwise.
//
// NULL says the same thing and says it out loud, which is the distinction R-183
// drew between a documented NULL and a name that resolves to nothing.  Mapping
// vampire onto the regeneration tech would ALSO have worked and would have made
// bots pick the rune up -- both drain-to-heal -- but that is a change to what
// the AI values, not a spelling fix, so it is recorded rather than made.
const char *const bot_tech_models[5] = {
    "models/ctf/resistance/tris.md2",       // RUNE_RESIST   -> tech1
    "models/ctf/strength/tris.md2",         // RUNE_STRENGTH -> tech2
    "models/ctf/haste/tris.md2",            // RUNE_HASTE    -> tech3
    "models/ctf/regeneration/tris.md2",     // RUNE_REGEN    -> tech4
    NULL,                                   // RUNE_VAMPIRE  -> no tech exists
};

//===========================================================================
// R-BOT-11.  Allocated at InitGame, after game.csr is chosen, and TAG_GAME so
// that a level change does not take the tables with it -- the strings inside
// them are TAG_LEVEL and are re-registered by the next map's precache.
//===========================================================================
void BotIndexesAlloc(void)
{
    bot_max_modelindexes = game.csr.max_models;
    bot_max_soundindexes = game.csr.max_sounds;
    bot_max_imageindexes = game.csr.max_images;

    modelindexes = gi.TagMalloc(bot_max_modelindexes * sizeof(char *), TAG_GAME);
    soundindexes = gi.TagMalloc(bot_max_soundindexes * sizeof(char *), TAG_GAME);
    imageindexes = gi.TagMalloc(bot_max_imageindexes * sizeof(char *), TAG_GAME);

    ClearIndexes();
}
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
// The three tables are TAG_GAME, so a gi.FreeTags(TAG_GAME) takes them with it
// and the globals are left dangling.  This is the "they are already gone" half:
// drop the pointers without freeing anything.  BotShutdown and ReadGame are
// both in that position, for different reasons.
void BotIndexesForget(void)
{
    modelindexes = soundindexes = imageindexes = NULL;
    bot_max_modelindexes = bot_max_soundindexes = bot_max_imageindexes = 0;
}
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void ClearIndexes(void)
{
    //NOTE: at level changes the individual strings are already freed
    //      with gi.FreeTags(TAG_LEVEL), so there's no need to free them
    //      here, we'll just clear the pointer arrays (indexes)
    if (!modelindexes)
        return;
    memset(modelindexes, 0, bot_max_modelindexes * sizeof(char *));
    memset(soundindexes, 0, bot_max_soundindexes * sizeof(char *));
    memset(imageindexes, 0, bot_max_imageindexes * sizeof(char *));
    memset(muzzleflashsoundindex, 0, sizeof(muzzleflashsoundindex));
} //end of the function ClearIndexes
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotInitMuzzleFlashToSoundindex(void)
{
    int i, j, mf;

    if (!soundindexes)
        return;
    for (i = 0; muzzleflashinfo[i].muzzleflash >= 0; i++)
    {
        mf = muzzleflashinfo[i].muzzleflash;
        //if the muzzle flash is valid
        if (mf >= 0 && mf < MAX_MUZZLEFLASHES)
        {
            //if the muzzle flash uses sounds
            if (muzzleflashinfo[i].sound)
            {
                for (j = 0; j < bot_max_soundindexes; j++)
                {
                    if (soundindexes[j])
                    {
                        if (!Q_stricmp(soundindexes[j], muzzleflashinfo[i].sound))
                        {
                            muzzleflashsoundindex[mf] = j;
                            break;
                        } //end if
                    } //end if
                } //end for
            } //end if
        } //end if
    } //end for
} //end of the function BotInitMuzzleFlashToSoundindex
//===========================================================================
// The three dumps are `sv modelindex` / `soundindex` / `imageindex`
// (R-BOT-24).  Empty rows are skipped: the 1999 table had 256 entries and
// printing all of them was already a screenful; game.csr's is up to 8192.
//===========================================================================
static bool BotIndexRecord(char **table, int count, const char *what, int i, const char *name);

static void BotDumpIndex(const char *what, char **table, int count)
{
    int i, n = 0;

    gi.dprintf("%s index (%d slots):\n", what, count);
    for (i = 0; i < count; i++)
    {
        if (!table || !table[i])
            continue;
        gi.dprintf("%4d: %s\n", i, table[i]);
        n++;
    } //end for
    gi.dprintf("%d entr%s\n", n, n == 1 ? "y" : "ies");
}

void BotDumpModelindex(void)
{
    BotDumpIndex("model", modelindexes, bot_max_modelindexes);
} //end of the function BotDumpModelindex

void BotDumpSoundindex(void)
{
    BotDumpIndex("sound", soundindexes, bot_max_soundindexes);
} //end of the function BotDumpSoundindex

void BotDumpImageindex(void)
{
    BotDumpIndex("image", imageindexes, bot_max_imageindexes);
} //end of the function BotDumpImageindex
//===========================================================================
// `sv indexprobe <model|sound|image> <index>` -- R-VER-6's control.
//
// The tables are sized from game.csr (R-BOT-11), which is the whole point: the
// engine cannot hand out an index the table has no room for, so the overflow
// arm of BotIndexRecord is a backstop that CANNOT FIRE on a real server.  That
// is the right design and it leaves R-VER-6's second half -- "overflow is
// reported rather than written" -- with nothing to observe, because a check
// that only ever sees silence cannot tell "it did not overflow" from "the
// reporting is broken".
//
// So the probe asks BotIndexRecord the question directly, with an index the
// caller chooses.  It is a diagnostic beside `sv modelindex` and the other
// three (R-BOT-24), it writes nothing -- the name it passes is NULL, so even a
// valid index only reports -- and it is what the check's control drives.
//===========================================================================
void BotIndexProbe(const char *what, int index)
{
    char  **table;
    int     count;

    if (!what)
    {
        gi.dprintf("usage: sv indexprobe <model|sound|image> <index>\n");
        return;
    } //end if
    if (!Q_stricmp(what, "model"))      { table = modelindexes; count = bot_max_modelindexes; }
    else if (!Q_stricmp(what, "sound")) { table = soundindexes; count = bot_max_soundindexes; }
    else if (!Q_stricmp(what, "image")) { table = imageindexes; count = bot_max_imageindexes; }
    else
    {
        gi.dprintf("usage: sv indexprobe <model|sound|image> <index>\n");
        return;
    } //end else

    if (BotIndexRecord(table, count, what, index, NULL))
        gi.dprintf("indexprobe: %s %d would be recorded\n", what, index);
    else if (index >= 0 && index < count)
        gi.dprintf("indexprobe: %s %d is in range (%d slots) and %s\n",
                   what, index, count,
                   table && table[index] ? "already taken" : "free");
    else
        gi.dprintf("indexprobe: %s %d is out of range (%d slots)\n",
                   what, index, count);
} //end of the function BotIndexProbe
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotClearCommandArguments(void)
{
    int i;
    for (i = 0; i < MAX_COMMANDARGUMENTS; i++)
    {
        commandarguments[i] = NULL;
    } //end for
} //end of the function BotClearCommandArguments
//===========================================================================
// Collects the varargs into commandarguments[] and commandline.
//
// The donor had this loop written out three times, once per caller, with the
// same off-by-one in each: `for (i = 1; ...) { ... if (!arg) break; }` leaves
// `i == MAX_COMMANDARGUMENTS` both when the list was exactly full AND when it
// overflowed, and the overflow branch then calls newgameimport.error(), which
// kills the server on a legal 19-argument command.  One copy, and the test is
// on whether a terminator was actually seen.
//===========================================================================
static bool BotCollectArguments(char *str, va_list ap)
{
    int i;

    BotClearCommandArguments();
    commandarguments[0] = str;
    for (i = 1; i < MAX_COMMANDARGUMENTS; i++)
    {
        commandarguments[i] = va_arg(ap, char *);
        if (!commandarguments[i]) break;
    } //end for
    if (i >= MAX_COMMANDARGUMENTS)
    {
        newgameimport.dprintf("WARNING: bot command has more than %d arguments, "
                              "dropped\n", MAX_COMMANDARGUMENTS - 1);
        BotClearCommandArguments();
        return false;
    } //end if
    commandline[0] = '\0';
    for (i = 1; i < MAX_COMMANDARGUMENTS; i++)
    {
        if (!commandarguments[i]) break;
        if (i > 1) Q_strlcat(commandline, " ", sizeof(commandline));
        Q_strlcat(commandline, commandarguments[i], sizeof(commandline));
    } //end for
    return true;
}
//===========================================================================
// terminate the arguments with a NULL string
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotClientCommand(int client, char *str, ...)
{
    edict_t *clent;
    va_list ap;

    if (client < 0 || client >= game.maxclients)
    {
        newgameimport.dprintf("BotClientCommand: client number out of range\n");
        return;
    } //end if
    va_start(ap, str);
    if (!BotCollectArguments(str, ap))
    {
        va_end(ap);
        return;
    } //end if
    va_end(ap);
    clent = DF_CLIENTENT(client);
    ClientCommand(clent);
    BotClearCommandArguments();
} //end of the function BotClientCommand
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotServerCommand(char *str, ...)
{
    va_list ap;

    va_start(ap, str);
    if (!BotCollectArguments(str, ap))
    {
        va_end(ap);
        return;
    } //end if
    va_end(ap);
    ServerCommand();
    BotClearCommandArguments();
} //end of the function BotServerCommand
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotStoreClientCommand(char *str, ...)
{
    va_list ap;

    va_start(ap, str);
    BotCollectArguments(str, ap);
    va_end(ap);
} //end of the function BotStoreClientCommand
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void q_printf(3, 4) Bot_cprintf(edict_t *ent, int printlevel, const char *fmt, ...)
{
    char str[MAX_STRING_CHARS];
    va_list ap;

    //ent == NULL means print to the server console, dedicated or not
    va_start(ap, fmt);
    Q_vsnprintf(str, sizeof(str), fmt, ap);
    va_end(ap);

    if (ent && (ent->flags & FL_BOT))
    {
        if (printlevel == PRINT_CHAT) BotLib_BotConsoleMessage(ent, CMS_CHAT, str);
        else BotLib_BotConsoleMessage(ent, CMS_NORMAL, str);
    } //end if
    else
    {
        newgameimport.cprintf(ent, printlevel, "%s", str);
    } //end else
} //end of the function Bot_cprintf
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void q_printf(2, 3) Bot_centerprintf(edict_t *ent, const char *fmt, ...)
{
    char str[MAX_STRING_CHARS];
    va_list ap;

    if (!ent || !(ent->flags & FL_BOT))
    {
        va_start(ap, fmt);
        Q_vsnprintf(str, sizeof(str), fmt, ap);
        va_end(ap);
        // R-BOT-30: `%s`, not `str`.  The donor's port made this fix and it is
        // a real one -- a centerprint carrying a player name carries whatever
        // format specifiers that name contains.
        newgameimport.centerprintf(ent, "%s", str);
    } //end if
} //end of the function Bot_centerprintf
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void q_printf(2, 3) Bot_bprintf(int printlevel, const char *fmt, ...)
{
    int j;
    char str[MAX_STRING_CHARS];
    edict_t *other;
    va_list ap;

    va_start(ap, fmt);
    Q_vsnprintf(str, sizeof(str), fmt, ap);
    va_end(ap);
    newgameimport.bprintf(printlevel, "%s", str);

    //print the message for all the bots
    for (j = 1; j <= game.maxclients; j++)
    {
        other = &g_edicts[j];
        if (!other->inuse) continue;
        if (!other->client) continue;
        if (!(other->flags & FL_BOT)) continue;
        BotLib_BotConsoleMessage(other, CMS_NORMAL, str);
    } //end for
} //end of the function Bot_bprintf
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void Bot_sound(edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs)
{
    BotLib_BotAddSound(ent, channel, soundindex, volume, attenuation, timeofs);
    newgameimport.sound(ent, channel, soundindex, volume, attenuation, timeofs);
} //end of the function Bot_sound
//===========================================================================
// Records name -> index so BotLoadMap can hand the brain the whole index
// space.  R-BOT-11: an index past the end of the table is DROPPED with one
// warning, never written -- the donor called newgameimport.error() here, which
// aborts the map, and on an extended server every index above 255 took that
// branch.
//===========================================================================
static bool BotIndexRecord(char **table, int count, const char *what, int i, const char *name)
{
    static bool reported[3];
    int slot = (table == modelindexes) ? 0 : (table == soundindexes) ? 1 : 2;
    size_t len;

    if (!table)
        return false;
    if (i < 0 || i >= count)
    {
        if (!reported[slot])
        {
            newgameimport.dprintf("WARNING: %sindex %d past the bot table's %d "
                                  "slots; the brain will not see it (R-BOT-11)\n",
                                  what, i, count);
            reported[slot] = true;
        } //end if
        return false;
    } //end if
    if (table[i] || !name)
        return false;
    len = strlen(name) + 1;
    table[i] = newgameimport.TagMalloc(len, TAG_LEVEL);
    memcpy(table[i], name, len);
    return true;
}

static int Bot_modelindex(const char *name)
{
    int i = newgameimport.modelindex(name);

    if (BotIndexRecord(modelindexes, bot_max_modelindexes, "model", i, name))
    {
        //if there's already a bot library loaded
        if (botglobals.firstbotlib) BotLib_BotLoadMap(NULL);
    } //end if
    return i;
} //end of the function Bot_modelindex

static int Bot_soundindex(const char *name)
{
    int i = newgameimport.soundindex(name);

    if (BotIndexRecord(soundindexes, bot_max_soundindexes, "sound", i, name))
    {
        if (botglobals.firstbotlib) BotLib_BotLoadMap(NULL);
    } //end if
    return i;
} //end of the function Bot_soundindex

static int Bot_imageindex(const char *name)
{
    int i = newgameimport.imageindex(name);

    if (BotIndexRecord(imageindexes, bot_max_imageindexes, "image", i, name))
    {
        if (botglobals.firstbotlib) BotLib_BotLoadMap(NULL);
    } //end if
    return i;
} //end of the function Bot_imageindex
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void Bot_setmodel(edict_t *ent, const char *name)
{
    newgameimport.setmodel(ent, name);
    BotIndexRecord(modelindexes, bot_max_modelindexes, "model",
                   ent->s.modelindex, name);
} //end of the function Bot_setmodel

/*
===============================================================================

THE STAGING MESSAGE

===============================================================================
*/

//===========================================================================
// Parameter:               len, the number of bytes about to be written
// Returns:                 where to write them, or NULL if they do not fit
// Changes Globals:     networkmessage
//===========================================================================
static byte *BotMessageSpace(size_t len)
{
    //the SDK's writers had no bound at all.  The buffer holds one whole
    //message and is only cleared when Bot_multicast or Bot_unicast flushes it,
    //so game code that writes bytes and then returns without multicasting
    //leaves the cursor where it was -- it accumulates across frames until
    //something overruns the buffer.  Report once per message and drop the rest.
    byte *p;

    if (len > (size_t)(MAX_NETWORKMESSAGE - networkmessage.writepos))
    {
        if (!networkmessage.overflowed)
        {
            newgameimport.dprintf("WARNING: bot network message overflow "
                                  "(%d bytes staged, %zu more wanted)\n",
                                  networkmessage.writepos, len);
        } //end if
        networkmessage.overflowed = true;
        return NULL;
    } //end if
    p = &networkmessage.data[networkmessage.writepos];
    networkmessage.writepos += len;
    return p;
}

static void BotStageInt(bot_writekind_t kind, int v)
{
    byte *p = BotMessageSpace(1 + sizeof(int));

    if (!p) return;
    p[0] = kind;
    memcpy(p + 1, &v, sizeof(int));
}

static void BotStageFloat(bot_writekind_t kind, float v)
{
    byte *p = BotMessageSpace(1 + sizeof(float));

    if (!p) return;
    p[0] = kind;
    memcpy(p + 1, &v, sizeof(float));
}

static void BotStageVec(bot_writekind_t kind, const vec3_t v)
{
    byte *p = BotMessageSpace(1 + sizeof(vec3_t));

    if (!p) return;
    p[0] = kind;
    memcpy(p + 1, v, sizeof(vec3_t));
}

static void BotClearMessage(void)
{
    networkmessage.writepos = 0;
    networkmessage.overflowed = false;
} //end of the function BotClearMessage

//===========================================================================
// Replays the staged records through the engine's own writers, so the bytes
// that reach the wire are the engine's encoding and not a second one.
//===========================================================================
static void BotWriteMessage(void)
{
    int pos = 0;

    //a message that did not fit is incomplete, and half a message desyncs the
    //receiver's parser -- drop it rather than send it
    if (networkmessage.overflowed)
    {
        newgameimport.dprintf("WARNING: BotWriteMessage: message overflowed, dropped\n");
        return;
    } //end if
    while (pos < networkmessage.writepos)
    {
        bot_writekind_t kind = networkmessage.data[pos++];
        int   iv;
        float fv;
        vec3_t vv;

        switch (kind) {
        case BW_CHAR:
            memcpy(&iv, &networkmessage.data[pos], sizeof(int));
            pos += sizeof(int);
            newgameimport.WriteChar(iv);
            break;
        case BW_BYTE:
            memcpy(&iv, &networkmessage.data[pos], sizeof(int));
            pos += sizeof(int);
            newgameimport.WriteByte(iv);
            break;
        case BW_SHORT:
            memcpy(&iv, &networkmessage.data[pos], sizeof(int));
            pos += sizeof(int);
            newgameimport.WriteShort(iv);
            break;
        case BW_LONG:
            memcpy(&iv, &networkmessage.data[pos], sizeof(int));
            pos += sizeof(int);
            newgameimport.WriteLong(iv);
            break;
        case BW_FLOAT:
            memcpy(&fv, &networkmessage.data[pos], sizeof(float));
            pos += sizeof(float);
            newgameimport.WriteFloat(fv);
            break;
        case BW_STRING:
            newgameimport.WriteString((const char *)&networkmessage.data[pos]);
            pos += strlen((const char *)&networkmessage.data[pos]) + 1;
            break;
        case BW_POSITION:
            memcpy(vv, &networkmessage.data[pos], sizeof(vec3_t));
            pos += sizeof(vec3_t);
            newgameimport.WritePosition(vv);
            break;
        case BW_DIR:
            memcpy(vv, &networkmessage.data[pos], sizeof(vec3_t));
            pos += sizeof(vec3_t);
            newgameimport.WriteDir(vv);
            break;
        case BW_ANGLE:
            memcpy(&fv, &networkmessage.data[pos], sizeof(float));
            pos += sizeof(float);
            newgameimport.WriteAngle(fv);
            break;
        default:
            // Unreachable: only the writers below stage records, and every
            // kind they stage has a case.  Saying so beats walking off the end
            // of a buffer whose cursor has been corrupted.
            newgameimport.dprintf("WARNING: BotWriteMessage: bad record %d, "
                                  "message dropped\n", kind);
            return;
        }
    } //end while
} //end of the function BotWriteMessage

//===========================================================================
// Reads the first three staged records back.  Returns true and fills in the
// pair when this message is an unsilenced svc_muzzleflash.
//===========================================================================
static bool BotSniffMuzzleFlash(int *entnum, int *muzzleflash)
{
    int pos = 0, v[3], i;

    for (i = 0; i < 3; i++)
    {
        bot_writekind_t want = (i == 1) ? BW_SHORT : BW_BYTE;

        if (pos + 1 + (int)sizeof(int) > networkmessage.writepos)
            return false;
        if (networkmessage.data[pos] != want)
            return false;
        memcpy(&v[i], &networkmessage.data[pos + 1], sizeof(int));
        pos += 1 + sizeof(int);
    } //end for
    if (v[0] != svc_muzzleflash)
        return false;
    *entnum = v[1];
    *muzzleflash = v[2];
    return true;
}
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void Bot_multicast(const vec3_t origin, multicast_t to)
{
    int entnum, muzzleflash, i;

    if (BotSniffMuzzleFlash(&entnum, &muzzleflash))
    {
        //MZ_SILENCED is a bit ORed onto the flash number, not a flash: a
        //silenced shot makes no sound for the brain to hear
        if (!(muzzleflash & MZ_SILENCED))
        {
            //check for valid muzzleflash and a valid entity
            if (muzzleflash >= 0 && muzzleflash < MAX_MUZZLEFLASHES &&
                entnum >= 0 && entnum < game.maxentities)
            {
                //if there is a sound for this muzzle flash
                if (muzzleflashsoundindex[muzzleflash])
                {
                    BotLib_BotAddSound(DF_NUMBERENT(entnum), CHAN_AUTO,
                        muzzleflashsoundindex[muzzleflash], 1.0f, ATTN_NORM, 0);
                } //end if
                //and its light, if the table gives the flash one.  Every row
                //ships with radius 0, so this is inert until one does not --
                //kept because R-BOT-10 names it and because the alternative is
                //a table column nothing reads.
                for (i = 0; muzzleflashinfo[i].muzzleflash >= 0; i++)
                {
                    if (muzzleflashinfo[i].muzzleflash != muzzleflash) continue;
                    if (muzzleflashinfo[i].radius <= 0) break;
                    BotLib_BotAddPointLight(DF_NUMBERENT(entnum)->s.origin, entnum,
                        muzzleflashinfo[i].radius, muzzleflashinfo[i].r,
                        muzzleflashinfo[i].g, muzzleflashinfo[i].b,
                        muzzleflashinfo[i].time, muzzleflashinfo[i].decay);
                    break;
                } //end for
            } //end if
        } //end if
    } //end if
    BotWriteMessage();
    newgameimport.multicast(origin, to);
    BotClearMessage();
} //end of the function Bot_multicast
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void Bot_unicast(edict_t *ent, qboolean reliable)
{
    // R-BOT-10: suppressed for FL_BOT recipients.  A bot has no network
    // connection, so the message has nowhere to go -- and this is the reason
    // the writes are staged at all rather than forwarded as they arrive: once
    // a byte is in the engine's message buffer there is no gi slot to take it
    // back out again.
    if (ent && (ent->flags & FL_BOT))
    {
        BotClearMessage();
        return;
    } //end if
    BotWriteMessage();
    newgameimport.unicast(ent, reliable);
    BotClearMessage();
} //end of the function Bot_unicast
//===========================================================================
//
// The value range checks are the donor's, and both of the two it silences are
// documented there: target_laser_think writes a LONG skinnum through
// WriteByte, and something in id's own code writes a short out of range.  They
// are kept because the engine's own writers do not check and a staged record
// is replayed verbatim.
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void Bot_WriteChar(int c)
{
    if (c < -128 || c > 127)
    {
        newgameimport.dprintf("WARNING: Bot_WriteChar: range error\n");
    } //end if
    BotStageInt(BW_CHAR, c);
} //end of the function Bot_WriteChar

static void Bot_WriteByte(int c)
{
    if (c < 0 || c > 255)
    {
        //NOTE: in target_laser_think: gi.WriteByte (self->s.skinnum); the
        // skin number is a LONG value this causes a write byte out of range
        c = 0;
    } //end if
    BotStageInt(BW_BYTE, c);
} //end of the function Bot_WriteByte

static void Bot_WriteShort(int c)
{
    if (c < INT16_MIN || c > INT16_MAX)
    {
        //NOTE: I guess somewhere in the original id code is some sort of bug
        // that causes a WriteShort out of range error
        c = 0;
    } //end if
    BotStageInt(BW_SHORT, c);
} //end of the function Bot_WriteShort

static void Bot_WriteLong(int c)
{
    BotStageInt(BW_LONG, c);
} //end of the function Bot_WriteLong

static void Bot_WriteFloat(float f)
{
    // Q2PRO's PF_WriteFloat is `Com_Error(ERR_DROP, "not implemented")`, so a
    // caller that reaches this has a bug and the transparent thing is to fail
    // the same way it would have without the redirection (R-BOT-13).  Staging
    // it and replaying it does exactly that, at the flush.
    BotStageFloat(BW_FLOAT, f);
} //end of the function Bot_WriteFloat

static void Bot_WriteString(const char *s)
{
    size_t len = s ? strlen(s) + 1 : 1;
    byte *p = BotMessageSpace(1 + len);

    if (!p) return;
    p[0] = BW_STRING;
    if (s) memcpy(p + 1, s, len - 1);
    p[len] = 0;
} //end of the function Bot_WriteString

static void Bot_WritePosition(const vec3_t pos)
{
    BotStageVec(BW_POSITION, pos);
} //end of the function Bot_WritePosition

static void Bot_WriteAngle(float f)
{
    BotStageFloat(BW_ANGLE, f);
} //end of the function Bot_WriteAngle

static void Bot_WriteDir(const vec3_t dir)
{
    static const vec3_t zero;

    BotStageVec(BW_DIR, dir ? dir : zero);
} //end of the function Bot_WriteDir
//===========================================================================
// R-BOT-10's last row: argc/argv/args serve synthesised bot command arguments
// when a bot command is executing, and the engine's otherwise.
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static int Bot_argc(void)
{
    int i;

    for (i = 0; i < MAX_COMMANDARGUMENTS; i++)
    {
        if (!commandarguments[i]) break;
    } //end for
    if (i) return i;
    else return newgameimport.argc();
} //end of the function Bot_argc

static char *Bot_argv(int n)
{
    if (n >= 0 && n < MAX_COMMANDARGUMENTS && commandarguments[n])
    {
        return commandarguments[n];
    } //end if
    else
    {
        return newgameimport.argv(n);
    } //end else
} //end of the function Bot_argv

static char *Bot_args(void)
{
    if (commandarguments[1])
    {
        return commandline;
    } //end if
    else
    {
        return newgameimport.args();
    } //end else
} //end of the function Bot_args
//===========================================================================
// R-BOT-9: called from GetGameAPI immediately after `gi = *import` and before
// anything else.  Nothing may read gi before it returns.
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void BotRedirectGameImport(void)
{
    //copy the import structure
    newgameimport = gi;
    //replace some of the import functions
    gi.cprintf = Bot_cprintf;
    gi.bprintf = Bot_bprintf;
    gi.centerprintf = Bot_centerprintf;
    gi.sound = Bot_sound;

    gi.modelindex = Bot_modelindex;
    gi.soundindex = Bot_soundindex;
    gi.imageindex = Bot_imageindex;
    gi.setmodel = Bot_setmodel;

    gi.multicast = Bot_multicast;
    gi.unicast = Bot_unicast;
    gi.WriteChar = Bot_WriteChar;
    gi.WriteByte = Bot_WriteByte;
    gi.WriteShort = Bot_WriteShort;
    gi.WriteLong = Bot_WriteLong;
    gi.WriteFloat = Bot_WriteFloat;
    gi.WriteString = Bot_WriteString;
    gi.WritePosition = Bot_WritePosition;
    gi.WriteDir = Bot_WriteDir;
    gi.WriteAngle = Bot_WriteAngle;

    gi.argc = Bot_argc;
    gi.argv = Bot_argv;
    gi.args = Bot_args;
    //clear the command arguments
    BotClearCommandArguments();
    BotClearMessage();
} //end of the function BotRedirectGameImport
