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
// The botlibs this game can run, and which of them a server offers.
// Colosseum's own file; see bl_botlib.h, and R-BOT-31..R-BOT-35 for the rules.

#include "g_local.h"
#include "bot/bl_main.h"
#include "bot/bl_botlib.h"
#include "bot/bl_botcfg.h"

// The extension BotDefaultLibrary() gives Gladiator's library, for both: ".so"
// on macOS as well, because the packaging renames the .dylib there and dlopen
// reads the Mach-O header, not the name.
#if defined(_WIN32)
#define BOTLIB_SHLIBEXT     ".dll"
#else
#define BOTLIB_SHLIBEXT     ".so"
#endif

#define BOTLIB_MAX_OSPATH   512

const botlib_t botlibs[BOTLIB_COUNT] = {
    [BOTLIB_GLADIATOR] = {
        .num        = BOTLIB_GLADIATOR,
        .id         = "gladiator",
        .title      = "Gladiator",
        .tag        = "GB",
        .library    = "gladiator",
        .version    = "BotLib v0.96",
        .datadir    = "",
        .roster     = NULL,
        // 2 loads with a warning in the 1999 botlib ("found an old AAS file"),
        // 3 is what its bspc writes
        .aasmin     = 2,
        .aasmax     = 3,
        .skills     = false,
    },
    [BOTLIB_Q3] = {
        .num        = BOTLIB_Q3,
        .id         = "q3",
        .title      = "Quake III Arena",
        .tag        = "Q3",
        .library    = "q3bot",
        .version    = "Q3Backport-",
        .datadir    = "q3bot",
        .roster     = "q3bot/bots.cfg",
        // AASVERSION_OLD and AASVERSION of its be_aas_file.c
        .aasmin     = 4,
        .aasmax     = 5,
        .skills     = true,
    },
};

const botlib_t *BotlibNamed(const char *id)
{
    int i;

    for (i = 0; i < BOTLIB_COUNT; i++)
    {
        if (!Q_stricmp(id, botlibs[i].id)) return &botlibs[i];
    } //end for
    return NULL;
}

// The handshake.  A prefix, because the version after it is the library's
// own business and moves with it; the prefix is what says WHICH botlib it is,
// and `BotLib v0.96` is what both the reconstruction and the 1999 binary say.
const botlib_t *BotlibForVersion(const char *version)
{
    int i;

    if (!version) return NULL;
    for (i = 0; i < BOTLIB_COUNT; i++)
    {
        if (!strncmp(version, botlibs[i].version, strlen(botlibs[i].version)))
            return &botlibs[i];
    } //end for
    return NULL;
}

const char *BotlibFile(const botlib_t *botlib)
{
    static char names[BOTLIB_COUNT][MAX_QPATH];

    Q_snprintf(names[botlib->num], sizeof(names[botlib->num]), "%s%s",
               botlib->library, BOTLIB_SHLIBEXT);
    return names[botlib->num];
}

/*
===============================================================================

`botlibs`: which botlibs this server offers, and in what order

One cvar, a list of botlib ids, and its order means something: the first
botlib that can play the map is the one the fill, `addrandom`, `vote addbots`
and a `becomebot` use, so `botlibs "q3 gladiator"` fills with Quake III bots
and falls back to Gladiator's on a map that has no Quake III AAS file.  A
botlib the list does not name is not offered at all: its bot list is not
loaded, the menus do not show it, and no bot of it can be added.  The default
is "gladiator", which is the tree as it was before there were two (R-COMPAT-2).

===============================================================================
*/

cvar_t *BotlibsCvar(void)
{
    return gi.cvar("botlibs", "gladiator", 0);
}

int BotlibsOffered(const botlib_t **list)
{
    // The value whose unknown words were last reported, so a typo is said once
    // and not once a frame -- the fill asks this every 32 frames.
    static char said[MAX_INFO_STRING];
    const char *value = BotlibsCvar()->string;
    const botlib_t *botlib;
    bool report;
    char word[MAX_QPATH];
    const char *s;
    int n, i, len;

    report = strcmp(said, value) != 0;
    if (report) Q_strlcpy(said, value, sizeof(said));

    n = 0;
    for (s = value; *s; )
    {
        // words separated by spaces or commas
        while (*s == ' ' || *s == ',' || *s == '\t') s++;
        for (len = 0; s[len] && s[len] != ' ' && s[len] != ',' && s[len] != '\t'; len++)
            ;
        if (!len) break;
        Q_strlcpy(word, s, len + 1 < (int)sizeof(word) ? len + 1 : (int)sizeof(word));
        s += len;

        botlib = BotlibNamed(word);
        if (!botlib)
        {
            if (report)
                gi.dprintf("botlibs: no botlib is called \"%s\" (gladiator, q3)\n", word);
            continue;
        } //end if
        for (i = 0; i < n; i++)
        {
            if (list[i] == botlib) break;
        } //end for
        if (i < n) continue;
        list[n++] = botlib;
    } //end for

    // An empty list is not "no bots" -- `bots 0` is that switch -- so it is
    // read as the default, and said once.
    if (!n)
    {
        if (report)
            gi.dprintf("botlibs names no botlib; offering gladiator\n");
        list[n++] = &botlibs[BOTLIB_GLADIATOR];
    } //end if
    return n;
}

bool BotlibOffered(const botlib_t *botlib)
{
    const botlib_t *list[BOTLIB_COUNT];
    int i, n;

    n = BotlibsOffered(list);
    for (i = 0; i < n; i++)
    {
        if (list[i] == botlib) return true;
    } //end for
    return false;
}

bool BotlibsMany(void)
{
    const botlib_t *list[BOTLIB_COUNT];

    return BotlibsOffered(list) > 1;
}

const char *BotlibsQualifier(void)
{
    static char qualifier[MAX_QPATH];
    const botlib_t *list[BOTLIB_COUNT];

    if (BotlibsOffered(list) != 1) return "";
    Q_snprintf(qualifier, sizeof(qualifier), "%s ", list[0]->title);
    return qualifier;
}

const botlib_t *BotlibDefault(void)
{
    const botlib_t *list[BOTLIB_COUNT];

    BotlibsOffered(list);
    return list[0];
}

const botlib_t *BotlibPreferred(void)
{
    const botlib_t *list[BOTLIB_COUNT];
    int i, n;

    n = BotlibsOffered(list);
    for (i = 0; i < n; i++)
    {
        if (BotlibPlayable(list[i])) return list[i];
    } //end for
    return NULL;
}

/*
===============================================================================

Can a botlib play this map?

Two questions, asked in the order they can be answered.  Whether its library
refused the map is known only once it has tried, and is remembered for the
level (BotlibSetMapFailed) -- that is the latch `botglobals.mapfailed` was for
one library, and it is per botlib now, because one botlib refusing a map is
exactly when the other one should be asked.  Whether its AAS file is there is
known before anything is loaded: the file is searched for the way the BOTLIB
searches, not the way the engine does, because it is the botlib that has to
find it.  Gladiator's search is FindQuakeFile's -- the loose file, then
pak0..pak9.pak, in the gamedir and then in baseq2, first under `basedir` and
then under `cddir` (empty, so relative to the server's working directory,
exactly as in that botlib), first as "<map>.aas" and then as "maps/<map>.aas".
The Quake III botlib's is its data directory and nothing else.

The answer is kept for the level, so the fill can ask it every 32 frames, and
`sv botlibs` asks afresh.

===============================================================================
*/

static bool botlibmapfailed[BOTLIB_COUNT];

static struct {
    bool    probed;
    char    map[MAX_QPATH];
    int     version;
} meshcache[BOTLIB_COUNT];

void BotlibSetMapFailed(const botlib_t *botlib)
{
    if (botlib) botlibmapfailed[botlib->num] = true;
}

bool BotlibMapFailed(const botlib_t *botlib)
{
    return botlib && botlibmapfailed[botlib->num];
}

void BotlibNewLevel(void)
{
    memset(botlibmapfailed, 0, sizeof(botlibmapfailed));
    memset(meshcache, 0, sizeof(meshcache));
}

// An AAS header's version, or -1: "EAAS" then the version, little-endian.
static int BotMeshHeader(FILE *f, long offset)
{
    unsigned char h[8];

    if (fseek(f, offset, SEEK_SET) || fread(h, 1, sizeof(h), f) != sizeof(h))
        return -1;
    if (h[0] != 'E' || h[1] != 'A' || h[2] != 'A' || h[3] != 'S')
        return -1;
    return h[4] | (h[5] << 8) | (h[6] << 16) | (h[7] << 24);
}

static int BotMeshLoose(const char *ospath, char *path, size_t size)
{
    FILE *f;
    int version;

    f = fopen(ospath, "rb");
    if (!f) return 0;
    version = BotMeshHeader(f, 0);
    fclose(f);
    if (path) Q_strlcpy(path, ospath, size);
    return version;
}

// Both botlibs compare a pak entry's name with the one they want after turning
// both kinds of separator into one, and without regard to case.
static bool BotPakNameMatches(const char *entry, const char *want)
{
    for (; *entry && *want; entry++, want++)
    {
        int a = *entry == '\\' ? '/' : Q_tolower(*entry);
        int b = *want == '\\' ? '/' : Q_tolower(*want);

        if (a != b) return false;
    } //end for
    return !*entry && !*want;
}

static int BotMeshInPak(const char *pakpath, const char *name, char *path, size_t size)
{
    unsigned char hdr[12], ent[64];
    int dirofs, dirlen, i, version = 0;
    FILE *f;

    f = fopen(pakpath, "rb");
    if (!f) return 0;
    if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr) ||
        memcmp(hdr, "PACK", 4))
    {
        fclose(f);
        return 0;
    } //end if
    dirofs = hdr[4] | (hdr[5] << 8) | (hdr[6] << 16) | (hdr[7] << 24);
    dirlen = hdr[8] | (hdr[9] << 8) | (hdr[10] << 16) | (hdr[11] << 24);
    if (dirofs < 0 || dirlen < 0 || fseek(f, dirofs, SEEK_SET))
    {
        fclose(f);
        return 0;
    } //end if
    for (i = 0; i < dirlen / 64; i++)
    {
        int filepos;

        if (fread(ent, 1, sizeof(ent), f) != sizeof(ent)) break;
        ent[55] = 0;
        if (!BotPakNameMatches((const char *)ent, name)) continue;
        filepos = ent[56] | (ent[57] << 8) | (ent[58] << 16) | (ent[59] << 24);
        version = BotMeshHeader(f, filepos);
        if (path) Q_snprintf(path, size, "%s/%s", pakpath, name);
        break;
    } //end for
    fclose(f);
    return version;
}

// One FindQuakeFile2 arm: `root` is basedir or cddir, either of which may be
// empty -- and then the path is relative, as in Gladiator's botlib.
static int BotMeshUnder(const char *root, const char *dir, const char *name,
                        char *path, size_t size)
{
    char base[BOTLIB_MAX_OSPATH], ospath[BOTLIB_MAX_OSPATH];
    int i, version;

    base[0] = 0;
    if (root && *root)
    {
        Q_strlcpy(base, root, sizeof(base));
        AppendPathSeperator(base, sizeof(base));
    } //end if
    if (dir && *dir)
    {
        Q_strlcat(base, dir, sizeof(base));
        AppendPathSeperator(base, sizeof(base));
    } //end if
    Q_snprintf(ospath, sizeof(ospath), "%s%s", base, name);
    version = BotMeshLoose(ospath, path, size);
    if (version) return version;
    for (i = 0; i < 10; i++)
    {
        Q_snprintf(ospath, sizeof(ospath), "%spak%d.pak", base, i);
        version = BotMeshInPak(ospath, name, path, size);
        if (version) return version;
    } //end for
    return 0;
}

int BotlibMesh(const botlib_t *botlib, const char *map, char *path, size_t size)
{
    char name[MAX_QPATH], ospath[BOTLIB_MAX_OSPATH];
    const char *roots[2], *dirs[2];
    int i, r, d, version;

    if (path && size) path[0] = 0;
    if (!map || !*map) return 0;

    if (botlib->datadir[0])
    {
        // The Quake III botlib's: its data directory, under basedir and the
        // gamedir, and nowhere else (the `datadir` libvar, its
        // be_interface_q2.c Q3_FS_FOpenFile).
        Q_snprintf(ospath, sizeof(ospath), "%s/%s/%s/maps/%s.aas", G_FsBaseDir(),
                   G_FsGameDir()[0] ? G_FsGameDir() : "baseq2",
                   botlib->datadir, map);
        return BotMeshLoose(ospath, path, size);
    } //end if

    // Gladiator's: be_aas_main.c AAS_LoadMap over l_utils.c FindQuakeFile
    roots[0] = G_FsBaseDir();
    roots[1] = gi.cvar("cddir", "", 0)->string;
    dirs[0] = G_FsGameDir();
    dirs[1] = "baseq2";
    for (i = 0; i < 2; i++)
    {
        Q_snprintf(name, sizeof(name), i ? "maps/%s.aas" : "%s.aas", map);
        for (r = 0; r < 2; r++)
        {
            for (d = 0; d < 2; d++)
            {
                version = BotMeshUnder(roots[r], dirs[d], name, path, size);
                if (version) return version;
            } //end for
        } //end for
    } //end for
    return 0;
}

bool BotlibPlayable(const botlib_t *botlib)
{
    int n;

    if (!botlib || BotlibMapFailed(botlib)) return false;
    n = botlib->num;
    if (!meshcache[n].probed || strcmp(meshcache[n].map, level.mapname))
    {
        meshcache[n].probed = true;
        Q_strlcpy(meshcache[n].map, level.mapname, sizeof(meshcache[n].map));
        meshcache[n].version = BotlibMesh(botlib, level.mapname, NULL, 0);
    } //end if
    if (meshcache[n].version >= botlib->aasmin &&
        meshcache[n].version <= botlib->aasmax)
        return true;
#if defined(_WIN32)
    // ...except where Gladiator's botlib makes a missing one itself: on
    // Windows, with `autolaunchbspc`, a map with no AAS file starts WinBSPC
    // on it, and only the LIBRARY starts it.  So there the fill asks the
    // library, as it did before there were two botlibs, and the library's
    // refusal latches the map as it always has (README.md, Bots).
    if (!meshcache[n].version && botlib->num == BOTLIB_GLADIATOR &&
        gi.cvar("autolaunchbspc", "", 0)->value)
        return true;
#endif
    return false;
}

/*
===============================================================================

`botskill`, and a bot's own

Quake III's characters come in skills 1..5 -- each character file holds the
three it was written for and the botlib interpolates the rest -- and the skill
is the bot's, not the server's: Q3's `addbot <name> <skill>`.  It travels in
the bot's userinfo as `skill` and reaches the botlib as the `bot_skill`
libvar, pushed immediately before that bot's BotSetupClient, which is where
the botlib reads it -- so it is per bot without a byte of the ABI changing
(R-BOT-34).  `botskill` is the skill of a bot nobody chose one for: the fill's,
a vote's, `addrandom`'s.  4 is Quake III's own default for an `addbot` without
one.  Gladiator's characters have no such thing; their skill is in the file.

===============================================================================
*/

cvar_t *BotSkill(void)
{
    return gi.cvar("botskill", "4", 0);
}

int BotSkillOf(const char *userinfo)
{
    const char *s = userinfo ? Info_ValueForKey(userinfo, "skill") : "";
    int skill;

    skill = *s ? Q_atoi(s) : (int)BotSkill()->value;
    return Q_clip(skill, 1, 5);
}

const char *BotSkillName(int skill)
{
    // Quake III's own names for them
    static const char *names[5] = {
        "I Can Win", "Bring It On", "Hurt Me Plenty", "Hardcore", "Nightmare!"
    };

    return names[Q_clip(skill, 1, 5) - 1];
}

/*
===============================================================================

`sv botlibs`: what the two questions above were asked about, in one place

Per botlib: whether it is offered and where in the order, its library file and
whether one is loaded, its data directory and bot list, and the AAS file it
would find for this map.  Each line is something an operator can act on, and
it is what tools/botmatrix.sh reads to tell "no library" from "no AAS file".

===============================================================================
*/

void BotlibDump(void)
{
    const botlib_t *list[BOTLIB_COUNT], *botlib;
    char path[BOTLIB_MAX_OSPATH], lib[BOT_MAX_PATH];
    bot_library_t *loaded;
    int i, n, total, version;

    //the offered ones in `botlibs`' order, then the rest
    n = total = BotlibsOffered(list);
    for (i = 0; i < BOTLIB_COUNT; i++)
    {
        if (!BotlibOffered(&botlibs[i])) list[total++] = &botlibs[i];
    } //end for
    gi.dprintf("botlibs    \"%s\"\n", BotlibsCvar()->string);
    for (i = 0; i < total; i++)
    {
        botlib = list[i];
        gi.dprintf("%-10s %s -- %s\n", botlib->id, botlib->title,
                   i < n ? va("offered, %d of %d", i + 1, n) : "not offered");

        BotlibPath(botlib, lib, sizeof(lib));
        loaded = BotlibLoaded(botlib);
        if (loaded)
            gi.dprintf("           library %s, loaded (%s), %d user%s\n",
                       loaded->path, loaded->funcs.BotVersion ?
                       loaded->funcs.BotVersion() : "no version",
                       loaded->users, loaded->users == 1 ? "" : "s");
        else
            gi.dprintf("           library %s, %s\n", lib,
                       BotLibraryExists(lib) ? "not loaded" : "NOT FOUND");

        gi.dprintf("           data    %s, bot list %s: %d bot%s\n",
                   botlib->datadir[0] ? va("%s/", botlib->datadir) : "the gamedir",
                   botlib->roster ? botlib->roster : BotFile()->string,
                   BotRosterCount(botlib), BotRosterCount(botlib) == 1 ? "" : "s");

        version = BotlibMesh(botlib, level.mapname, path, sizeof(path));
        if (version > 0)
            gi.dprintf("           aas     %s: %s, version %d%s\n", level.mapname,
                       path, version,
                       version < botlib->aasmin || version > botlib->aasmax ?
                       va(" -- NOT ONE THIS BOTLIB READS (%d..%d)",
                          botlib->aasmin, botlib->aasmax) : "");
        else if (version < 0)
            gi.dprintf("           aas     %s: %s is not an AAS file\n",
                       level.mapname, path);
        else
            gi.dprintf("           aas     %s: none\n", level.mapname);

        gi.dprintf("           %s\n", BotlibMapFailed(botlib) ?
                   "refused this map: its library could not load it" :
                   BotlibPlayable(botlib) ? "can play this map" :
                   "cannot play this map");
    } //end for
    gi.dprintf("botskill   %d (%s)\n", Q_clip((int)BotSkill()->value, 1, 5),
               BotSkillName((int)BotSkill()->value));
}
