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
// bots.cfg, from osp-tourney@1d8427e (SPECS.md sec 5.4.6, R-BOT-26).
//
// WHAT CHANGED, AND WHY IT HAD TO.  The donor reads its config files with
// fopen/fgetc and enumerates bots/*.cfg with glob() on ELF and _findfirst() on
// Win32.  Both go straight to the operating system, so neither can see a file
// inside a .pak or .pkz, neither respects the engine's search path, and the
// path they build starts at the process's working directory -- which for a
// server started from anywhere but its own gamedir is the wrong place.  R-BOT-8
// already says the paths come from FILESYSTEM_API_V1 where the cvars cannot
// give them; this reads the files through the same door.  The GRAMMAR is
// unchanged -- R-BOT-26 says "parsed as in v0.92" and it is, character for
// character; only the source of the characters moved.
//===========================================================================
//
// Name:                bl_botcfg.c
// Function:        bot configuration files
// Programmer:      Mr Elusive (MrElusive@demigod.demon.nl)
// Last update: 1998-01-12
// Tab Size:        3
//===========================================================================

#include "g_local.h"
#include "bot/bl_main.h"
#include "bot/bl_spawn.h"
#include "bot/bl_redirgi.h"
#include "bot/bl_botcfg.h"

#if defined(_WIN32)
#define PATHSEPERATOR_CHAR      '\\'
#else
#define PATHSEPERATOR_CHAR      '/'
#endif

bot_t *botlist;
static char botfilename[BOT_MAX_PATH];

// The parser reads one character at a time out of a loaded file rather than a
// FILE *, so that a bots.cfg inside a pak parses exactly like one on disk.
typedef struct {
    const char *data;
    int         len;
    int         pos;
    const char *name;
    int         line;
} botcfg_t;

static int cfg_getc(botcfg_t *f)
{
    if (f->pos >= f->len) return EOF;
    return (unsigned char)f->data[f->pos++];
}

static void cfg_ungetc(botcfg_t *f)
{
    if (f->pos > 0) f->pos--;
}

//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void AddBotToList(bot_t *bot)
{
    bot_t *b, *lastbot;

    lastbot = NULL;
    for (b = botlist; b; b = b->next)
    {
        if (Q_stricmp(bot->name, b->name) < 0)
        {
            //add the new bot before the current bot
            bot->next = b;
            if (lastbot) lastbot->next = bot;
            else botlist = bot;
            return;
        } //end if
        lastbot = b;
    } //end for
    //add the bot to the end of the list
    if (lastbot) lastbot->next = bot;
    else botlist = bot;
    bot->next = NULL;
} //end of the function AddBotToList
//===========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void AppendPathSeperator(char *path, int length)
{
    int pathlen = strlen(path);

    if (pathlen && length - pathlen > 1 &&
        path[pathlen-1] != '/' && path[pathlen-1] != '\\')
    {
        path[pathlen] = PATHSEPERATOR_CHAR;
        path[pathlen+1] = '\0';
    } //end if
} //end of the function AppenPathSeperator
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
static int ReadString(botcfg_t *f, char *string, int maxlen)
{
    int c, i;

    c = cfg_getc(f);
    if (c != '"')
    {
        gi.dprintf("leading \" not found in %s line %d\n", f->name, f->line);
        return false;
    } //end if
    c = cfg_getc(f);
    for (i = 0; c != '\"'; i++)
    {
        // `>=` against maxlen would still write string[maxlen-1] and then the
        // terminator at the same index, so the bound is one lower: the donor's
        // own BOT_MAX_PATH callers were safe by luck and its `16` caller for the
        // bot name was not.
        if (i >= maxlen - 1)
        {
            gi.dprintf("string too long in %s line %d\n", f->name, f->line);
            string[i] = 0;
            return false;
        } //end if
        string[i] = c;
        c = cfg_getc(f);
        if (c == EOF || c == '\n')
        {
            gi.dprintf("string without trailing \" in %s line %d\n", f->name, f->line);
            string[i] = 0;
            return false;
        } //end if
    } //end while
    string[i] = 0;
    return true;
} //end of the function ReadString
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
static int ReadSpace(botcfg_t *f)
{
    int c;

    do
    {
        c = cfg_getc(f);
        if (c == EOF)
        {
            gi.dprintf("unexpected end of file in %s line %d\n", f->name, f->line);
            return false;
        } //end if
        if (c == '\n')
        {
            gi.dprintf("found unexpected end of line in %s line %d\n", f->name, f->line);
            return false;
        } //end if
    } while(c <= ' ');
    cfg_ungetc(f);
    return true;
} //end of the function ReadSpace
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
static int LoadBotsFromFile(const char *filename)
{
    int lastline, c, numbots;
    botcfg_t f;
    void *buffer;
    int len;
    char addbot[BOT_MAX_PATH];
    bot_t *bot, tmpbot;

    len = G_FsLoadFile(filename, &buffer);
    if (len < 0 || !buffer)
    {
        gi.dprintf("error opening %s\n", filename);
        return false;
    } //end if

    f.data = buffer;
    f.len  = len;
    f.pos  = 0;
    f.name = filename;
    f.line = 0;
    numbots = 0;
    c = EOF;
    while (f.pos < f.len)
    {
        lastline = f.line;
        do
        {
            c = cfg_getc(&f);
            if (c == '\n') f.line++;
            else if (c == ';')
            {
                while(c != EOF && c != '\n') c = cfg_getc(&f);
                f.line++;
            } //end if
            if (c == EOF) break;
        } while(c <= ' ');
        if (c == EOF) break;
        //if not at the start of the file we must cross at least one line
        //for the next character
        if (lastline && lastline == f.line)
        {
            gi.dprintf("expected end of line in %s line %d but found %c\n",
                       filename, f.line, c);
            break;
        } //end if
        cfg_ungetc(&f);
        memset(&tmpbot, 0, sizeof(tmpbot));
        //sv
        if (!ReadString(&f, addbot, BOT_MAX_PATH)) break;
        if (!ReadSpace(&f)) break;
        //addbot
        if (!ReadString(&f, addbot, BOT_MAX_PATH)) break;
        if (!ReadSpace(&f)) break;
        //name.  MAX_NETNAME, which is the botlib contract's own bound on a bot
        //name and the reason the donor wrote a bare 16 here.
        if (!ReadString(&f, tmpbot.name, MAX_NETNAME)) break;
        if (!ReadSpace(&f)) break;
        //skin
        if (!ReadString(&f, tmpbot.skin, BOT_MAX_PATH)) break;
        if (!ReadSpace(&f)) break;
        //charfile
        if (!ReadString(&f, tmpbot.charfile, BOT_MAX_PATH)) break;
        if (!ReadSpace(&f)) break;
        //charname
        if (!ReadString(&f, tmpbot.charname, BOT_MAX_PATH)) break;
        //
        bot = gi.TagMalloc(sizeof(bot_t), TAG_GAME);
        *bot = tmpbot;
        AddBotToList(bot);
        //
        numbots++;
    } //end while
    G_FsFreeFile(buffer);
    //if not at the end of the file something was wrong
    if (c != EOF && f.pos < f.len) return false;
    gi.dprintf("loaded %d bot%s from %s\n", numbots, numbots == 1 ? "" : "s", filename);
    return true;
} //end of the function LoadBotsFromFile
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
// Drop the roster without freeing it: its nodes are TAG_GAME and the caller
// has just had them freed underneath it.  LoadBots() below walks the list with
// gi.TagFree, so leaving `botlist` set after a FreeTags(TAG_GAME) is a double
// free the next time anything reloads bots.cfg.
void BotListForget(void)
{
    botlist = NULL;
}

void LoadBots(void)
{
    bot_t *bot;
    char botfile[BOT_MAX_PATH];
    char **list;
    int i, count;

    //free the current botlist
    while(botlist)
    {
        bot = botlist;
        botlist = botlist->next;
        gi.TagFree(bot);
    } //end for
    //
    // The donor builds "./<gamedir>/<botfile>" by hand.  The filesystem is
    // already rooted at the gamedir, so the path IS the cvar's value -- and
    // that is what makes a bots.cfg inside a pak findable at all.
    Q_strlcpy(botfile, BotFile()->string, sizeof(botfile));
    //load bots from the main bot cfg file
    LoadBotsFromFile(botfile);
    //load the bots from all *.cfg files in the "bots" sub-folder
    list = G_FsListFiles("bots", ".cfg", &count);
    for (i = 0; i < count; i++)
    {
        LoadBotsFromFile(list[i]);
    } //end for
    G_FsFreeFileList(list);
} //end of the function LoadBots
//========================================================================
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
bot_t *FindBotWithName(const char *name)
{
    bot_t *bot;

    for (bot = botlist; bot; bot = bot->next)
    {
        if (!strcmp(bot->name, name)) return bot;
    } //end for
    return NULL;
} //end of the function FindBotWithName
//========================================================================
// R-BOT-26: picks up an edit to the `botfile`/`bots_botfile` cvar without a
// restart.  R-BOT-29's blocks 1 and 2 of seventeen are the cvar NAME, which
// stays per ruleset (R-OSP-11).
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
void CheckForNewBotFile(void)
{
    cvar_t *botfile;

    botfile = BotFile();
    if (Q_stricmp(botfilename, botfile->string))
    {
        LoadBots();
        Q_strlcpy(botfilename, botfile->string, sizeof(botfilename));
    } //end if
} //end of the function CheckForNewBotFile
//========================================================================
// R-BOT-26: returns success.
//
// THE DONOR'S SEARCH, KEPT AS IT IS.  Three of the identifiers here are the
// reconstruction's inventions (R-BOT-30) and two of them look like bugs:
// `numbots` is doubled and then used as the loop's countdown, so the walk can
// go round the list twice, and `choice` counts down alongside it.  It is a
// random pick that skips bots already in the game and gives up after two laps.
// `nbots` holds the ORIGINAL count because `numbots` has been consumed by the
// time the result is tested.
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//========================================================================
int AddRandomBot(edict_t *ent)
{
    // Donor order, kept.
    int choice, i, numbots;
    // `nbots` (invented name) holds the *original* bot count across the search
    // loop, which consumes `numbots` as its countdown.
    int nbots;
    bot_t *bot;
    edict_t *cl_ent;

    if (!G_BotsAllowed())
    {
        // Two reasons reach here and they deserve different words: `sp` never
        // accepts bots (N6), and any other ruleset with `bots 0` has an
        // operator who turned them off (R-96).  `sv ruleset` prints both.
        const char *why = G_Ruleset() == RULESET_SP
            ? "ruleset 'sp' does not accept bots"
            : "bots are switched off on this server ('bots 0')";

        if (ent) gi.cprintf(ent, PRINT_HIGH, "%s\n", why);
        else gi.dprintf("%s\n", why);
        return false;
    } //end if
    CheckForNewBotFile();
    //
    for (numbots = 0, bot = botlist; bot; bot = bot->next) numbots++;
    if (!numbots)
    {
        if (ent) gi.cprintf(ent, PRINT_HIGH, "No configured bots to add!\n");
        else gi.bprintf(PRINT_HIGH, "No configured bots to add!\n");
        return false;
    } //end if
    choice = frand() * numbots;
    //
    nbots = numbots;
    // The doubling reads `numbots` again, not the copy just made.
    numbots = numbots * 2;
    for (bot = botlist; bot && numbots > 0; numbots--, choice--)
    {
        for (i = 0; i < game.maxclients; i++)
        {
            cl_ent = DF_CLIENTENT(i);
            if (!cl_ent->inuse) continue;
            if (!(cl_ent->flags & FL_BOT)) continue;
            if (!strcmp(bot->name, cl_ent->client->pers.netname)) break;
        } //end for
        //if the bot is NOT already in the game
        if (i >= game.maxclients)
        {
            if (choice <= 0) break;
        } //end if
        //
        bot = bot->next;
        if (!bot) bot = botlist;
    } //end for
    // The ORIGINAL count is tested first, and both paths reach one shared
    // BotServerCommand call.
    if (nbots > 0 && bot)
    {
        // R-BOT-29, block 8 of seventeen: when the walk ran out of laps without
        // settling, tourney re-picks at random instead of taking whatever the
        // cursor landed on.  Only under tourney -- it changes which bot joins.
        if (G_Ruleset() == RULESET_TOURNEY && numbots <= 0)
        {
            choice = frand() * nbots;
            for (i = 0; i < choice; i++)
            {
                bot = bot->next;
                if (!bot) bot = botlist;
            } //end for
        } //end if
        BotServerCommand("sv", "addbot", bot->name, bot->skin, bot->charfile,
                         bot->charname, NULL);
        return true;
    } //end if
    if (ent) gi.cprintf(ent, PRINT_HIGH, "No configured bots to add!\n");
    else gi.bprintf(PRINT_HIGH, "No configured bots to add!\n");
    return false;
} //end of the function AddRandomBot
