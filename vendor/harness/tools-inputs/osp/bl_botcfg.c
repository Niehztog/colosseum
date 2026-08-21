//===========================================================================
//
// Name:				p_botcfg.c
// Function:		bot configuration files
// Programmer:		Mr Elusive (MrElusive@demigod.demon.nl)
// Last update:	1998-01-12
// Tab Size:		3
//===========================================================================

#include "g_local.h"
#include "bl_spawn.h"
#include "bl_main.h"
#include "bl_redirgi.h"

#if defined(WIN32)|defined(_WIN32)
#include <io.h>
#else
#include <glob.h>
#endif

#ifndef MAX_PATH
	#define MAX_PATH			144
#endif

#ifndef PATHSEPERATOR_STR
	#if defined(WIN32)|defined(_WIN32)
		#define PATHSEPERATOR_STR		"\\"
	#else
		#define PATHSEPERATOR_STR		"/"
	#endif
#endif
#ifndef PATHSEPERATOR_CHAR
	#if defined(WIN32)|defined(_WIN32)
		#define PATHSEPERATOR_CHAR		'\\'
	#else
		#define PATHSEPERATOR_CHAR		'/'
	#endif
#endif

typedef struct bot_s
{
	char name[MAX_PATH];
	char skin[MAX_PATH];
	char charfile[MAX_PATH];
	char charname[MAX_PATH];
	struct bot_s *next;
} bot_t;

bot_t *botlist;
char botfilename[MAX_PATH];

//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
// gamex86.dll: 10001000..1000109D
// gamei386.so: 00074908..00074992
void AddBotToList(bot_t *bot)
{
	bot_t *b, *lastbot;

	lastbot = NULL;
	for (b = botlist; b; b = b->next)
	{
		if (stricmp(bot->name, b->name) < 0)
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
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
// gamex86.dll: 1000109D..100010D3
// gamei386.so: 00074994..000749B5
void ConvertPath(char *path)
{
	while(*path)
	{
		if (*path == '/' || *path == '\\') *path = PATHSEPERATOR_CHAR;
		path++;
	} //end while
} //end of the function ConvertPath
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
// gamex86.dll: 100010D3..10001136
// gamei386.so: 000749B8..00074A0B
void AppendPathSeperator(char *path, int length)
{
	int pathlen = strlen(path);

	if (strlen(path) && length-pathlen > 1 && path[pathlen-1] != '/' && path[pathlen-1] != '\\')
	{
		path[pathlen] = PATHSEPERATOR_CHAR;
		path[pathlen+1] = '\0';
	} //end if
} //end of the function AppenPathSeperator
//========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//========================================================================
// gamex86.dll: 10001136..10001220
// gamei386.so: 00074A0C..00074AE3
int ReadString(FILE *fp, char *filename, int line, char *string, int maxlen)
{
	int c, i;

	c = fgetc(fp);
	if (c != '"')
	{
		gi.dprintf("leading \" not found in %s line %d\n", filename, line);
		return false;
	} //end if
	c = fgetc(fp);
	for (i = 0; c != '\"'; i++)
	{
		if (i >= maxlen)
		{
			gi.dprintf("string too long in %s line %d\n", filename, line);
			string[i] = 0;
			return false;
		} //end if
		string[i] = c;
		c = fgetc(fp);
		if (c == EOF || c == '\n')
		{
			gi.dprintf("string without trailing \" in %s line %d\n", filename, line);
			string[i] = 0;
			return false;
		} //end if
	} //end while
	string[i] = 0;
	return true;
} //end of the function ReadString
//========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//========================================================================
// gamex86.dll: 10001220..10001292
// gamei386.so: 00074AE4..00074B69
int ReadSpace(FILE *fp, char *filename, int line)
{
	int c;

	do
	{
		c = fgetc(fp);
		if (c == EOF)
		{
			gi.dprintf("unexpected end of file in %s line %d\n", filename, line);
			return false;
		} //end if
		if (c == '\n')
		{
			gi.dprintf("found unexpected end of line in %s line %d\n", filename, line);
			return false;
		} //end if
	} while(c <= ' ');
	ungetc(c, fp);
	return true;
} //end of the function ReadSpace
//========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//========================================================================
// gamex86.dll: 10001292..10001631
// gamei386.so: 00074B6C..000756BC
int LoadBotsFromFile(char *filename)
{
	int line, lastline, c, numbots;
	FILE *fp;
	char addbot[MAX_PATH];
	bot_t *bot, tmpbot;

	fp = fopen(filename, "rb");
	if (!fp)
	{
		gi.dprintf("error opening %s\n", filename);
		return false;
	} //end if

	line = 0;
	numbots = 0;
	while(!feof(fp))
	{
		lastline = line;
		do
		{
			c = fgetc(fp);
			if (c == '\n') line++;
			else if (c == ';')
			{
				while(c != EOF && c != '\n') c = fgetc(fp);
				line++;
			} //end if
			if (c == EOF) break;
		} while(c <= ' ');
		if (c == EOF) break;
		//if not at the start of the file we must cross at least one line
		//for the next character
		if (lastline && lastline == line)
		{
			gi.dprintf("expected end of line in %s line %d but found %c\n", filename, line, c);
			break;
		} //end if
		ungetc(c, fp);
		//sv
		if (!ReadString(fp, filename, line, addbot, MAX_PATH)) break;
		if (!ReadSpace(fp, filename, line)) break;
		//addbot
		if (!ReadString(fp, filename, line, addbot, MAX_PATH)) break;
		if (!ReadSpace(fp, filename, line)) break;
		//name
		if (!ReadString(fp, filename, line, tmpbot.name, 16)) break;
		if (!ReadSpace(fp, filename, line)) break;
		//skin
		if (!ReadString(fp, filename, line, tmpbot.skin, MAX_PATH)) break;
		if (!ReadSpace(fp, filename, line)) break;
		//charfile
		if (!ReadString(fp, filename, line, tmpbot.charfile, MAX_PATH)) break;
		if (!ReadSpace(fp, filename, line)) break;
		//charname
		if (!ReadString(fp, filename, line, tmpbot.charname, MAX_PATH)) break;
		//
		bot = gi.TagMalloc(sizeof(bot_t), TAG_GAME);
		memcpy(bot, &tmpbot, sizeof(bot_t));
		AddBotToList(bot);
		//
		numbots++;
	} //end while
	fclose(fp);
	//if not at the end of the file something was wrong
	if (c != EOF) return false;
	gi.dprintf("loaded %d bot%s from %s\n", numbots, numbots==1?"":"s", filename);
	return true;
} //end of the function LoadBotsFromFile
//========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//========================================================================
// gamex86.dll: 10001631..10001821
// gamei386.so: 000756BC..00075974
void LoadBots(void)
{
	bot_t *bot;
	char path[MAX_PATH], botfile[MAX_PATH], filespec[MAX_PATH];
	cvar_t *gamedir;
#if defined(WIN32)|defined(_WIN32)
	struct _finddata_t fileinfo;
	int done;
#else
	glob_t globbuf;
	int i;
#endif

	//free the current botlist
	while(botlist)
	{
		bot = botlist;
		botlist = botlist->next;
		gi.TagFree(bot);
	} //end for
	//
	gamedir = gi.cvar("game", "gladiator", 0);
	strcpy(path, ".");
	AppendPathSeperator(path, MAX_PATH);
	strcat(path, gamedir->string);
	AppendPathSeperator(path, MAX_PATH);
	ConvertPath(path);
	//
	strcpy(botfile, path);
#ifdef TOURNEY
	strcat(botfile, gi.cvar("bots_botfile", "botcfg/bots.cfg", 0)->string);
#else
	strcat(botfile, gi.cvar("botfile", "botcfg/bots.cfg", 0)->string);
#endif //TOURNEY
	//load bots from the main bot cfg file
	LoadBotsFromFile(botfile);
	//load the bots from all *.cfg files in the "bots" sub-folder
	strcat(path, "bots");
	AppendPathSeperator(path, MAX_PATH);
	ConvertPath(path);
	strcpy(filespec, path);
	strcat(filespec, "*.cfg");
#if defined(WIN32)|defined(_WIN32)
	done = _findfirst(filespec, &fileinfo);
	while(done != -1)
	{
		strcpy(botfile, path);
		strcat(botfile, fileinfo.name);
		LoadBotsFromFile(botfile);
		done = _findnext(done, &fileinfo);
	} //end while
#else
	glob(filespec, 0, NULL, &globbuf);
	for (i = 0; i < globbuf.gl_pathc; i++)
	{
		strcpy(botfile, globbuf.gl_pathv[i]);
		LoadBotsFromFile(botfile);
	} //end for
	globfree(&globbuf);
#endif
} //end of the function LoadBots
//========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//========================================================================
// gamex86.dll: 10001821..10001862
// gamei386.so: 00075974..000759C0
bot_t *FindBotWithName(char *name)
{
	bot_t *bot;

	for (bot = botlist; bot; bot = bot->next)
	{
		if (!strcmp(bot->name, name)) return bot;
	} //end for
	return NULL;
} //end of the function FindBotWithName
//========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//========================================================================
// gamex86.dll: 10001862..100018B3
// gamei386.so: 000759C0..00075A20
void CheckForNewBotFile(void)
{
	cvar_t *botfile;

#ifdef TOURNEY
	botfile = gi.cvar("bots_botfile", "botcfg/bots.cfg", 0);
#else
	botfile = gi.cvar("botfile", "botcfg/bots.cfg", 0);
#endif
	if (stricmp(botfilename, botfile->string))
	{
		LoadBots();
		strcpy(botfilename, botfile->string);
	} //end if
} //end of the function CheckForNewBotFile
//========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//========================================================================
// gamex86.dll: 100018B3..10001AD0
// gamei386.so: 00075A20..00075C61
int AddRandomBot(edict_t *ent)
{
	// Donor order, kept.
	int choice, i, numbots;
	// `nbots` (invented name) holds the *original* bot count across the search
	// loop, which consumes `numbots` as its countdown.
	int nbots;
	bot_t *bot;
	edict_t *cl_ent;

	CheckForNewBotFile();
	//
	for (numbots = 0, bot = botlist; bot; bot = bot->next) numbots++;
	choice = random() * numbots;
	//
	nbots = numbots;
	// The doubling reads `numbots` again, not the copy just made.
	numbots = numbots * 2;
	for (bot = botlist; bot && numbots > 0; numbots--, choice--)
	{
		for (i = 0; i < game.maxclients; i++)
		{
			cl_ent = g_edicts + 1 + i;
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
	if (nbots > 0)
	{
#ifdef TOURNEY
		if (numbots <= 0)
		{
			choice = random() * nbots;
			for(i = 0; i < choice; i++, bot = bot->next)
				if (!bot) bot = botlist;
		} //end if
#endif //TOURNEY
		BotServerCommand("sv", "addbot", bot->name, bot->skin, bot->charfile, bot->charname, NULL);
		return true;
	} //end if
	if (ent) gi.cprintf(ent, PRINT_HIGH, "No configured bots to add!\n");
	else gi.bprintf(PRINT_HIGH, "No configured bots to add!\n");
	return false;
} //end of the function AddRandomBot
