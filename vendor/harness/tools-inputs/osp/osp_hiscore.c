// osp_hiscore.c -- <INVENTED FILENAME>. The persistent per-map high score
// table.
//
// Ten entries of name/score/date plus a "set this session" flag, kept in the
// global p_table, rendered into the layout string hs_table, and written to
//   <basedir>/<gamedir>/<client_highscoredir>/<port>/<mapname>
// as one "FL\t<n>" or "TL\t<n>" header line followed by ten tab-separated
// rows.  hs_mode 1 = fraglimit (score is frags per hour), 2 = timelimit (score
// is the raw frag count); hs_limit caches whichever limit is in force, and a
// mismatch against the header line resets the file.

#include "g_local.h"
#include <errno.h>

// The globals this TU owns, in .bss order.
int			hs_mode;
hs_player_t	p_table[10];
char		hs_table[1400];
int			hs_limit;

// gamex86.dll: 1002BA30..1002BB5C
// gamei386.so: 0005CB88..0005CD32
void OSP_initHighScores (void)
{
	int		i;

	if (!(int)client_highscores->value)
	{
		hs_mode = 0;
		return;
	}

	if ((int)fraglimit->value)
	{
		hs_limit = (int)fraglimit->value;
		hs_mode = 1;
	}
	else if ((int)timelimit->value)
	{
		hs_limit = (int)timelimit->value;
		hs_mode = 2;
	}
	else
	{
		hs_mode = 0;
		gi.cvar_set ("client_highscores", "0");
		return;
	}

	for (i = 0; i < 10; i++)
	{
		strcpy (p_table[i].name, "<empty>");
		strcpy (p_table[i].score, "0");
		strcpy (p_table[i].date, "01_Jan_70");
		p_table[i].isnew = 0;
	}

	OSP_loadHighScores ();
	OSP_formatHighScores ();
}

// gamex86.dll: 1002BB5C..1002BD5A
// gamei386.so: 0005CD34..0005CE90
void OSP_formatHighScores (void)
{
	char	line[1400];
	int		i;
	int		yvpos;
	char	tag;

	sprintf (hs_table, "xv 0 yv 0 cstring \"High scores (%s)\"", level.mapname);

	if (hs_mode == 1)
		sprintf (line, "yv 8 cstring2 \"Fraglimit: %d\"", hs_limit);
	else
		sprintf (line, "yv 8 cstring2 \"Timelimit: %d\"", hs_limit);
	strcat (hs_table, line);

	if (hs_mode == 1)
		sprintf (line, "yv 24 cstring2 \"  # Name              FPH  Date     \"");
	else
		sprintf (line, "yv 24 cstring2 \"  # Name            Score  Date     \"");
	strcat (hs_table, line);

	yvpos = 34;
	i = 0;

	// Keep the post-header call cleanup in its own target basic block.
format_high_scores_rows:
	{
		for (; i < 10; i++)
		{
			if (p_table[i].isnew)
				tag = '*';
			else
				tag = ' ';

			if (hs_mode == 1)
				sprintf (line, "yv %d cstring \"%c%2d %-16s%5s  %s\"", yvpos, tag,
					i + 1, p_table[i].name, p_table[i].score, p_table[i].date);
			else
				sprintf (line, "yv %d cstring \"%c%2d %-16s%5s  %s\"", yvpos, tag,
					i + 1, p_table[i].name, p_table[i].score, p_table[i].date);
			strcat (hs_table, line);
			yvpos += 8;
		}
	}
}

// gamex86.dll: 1002BD5A..1002BD78
// gamei386.so: 0005CE90..0005CEC3
void OSP_showHighScores (void)
{
	gi.WriteByte (svc_layout);
	gi.WriteString (hs_table);
}

// gamex86.dll: 1002BD78..1002C0E6
// gamei386.so: 0005CEC4..0005D243
void OSP_updateHighScores (void)
{
	char	stamp[256];
	int		i;
	int		j;
	int		rowi;
	int		framecnt;
	int		value;
	int		allowed;
	edict_t	*ent;

	allowed = 1;

	if (!active_clients)
		return;

	for (i = 1; i <= game.maxclients; i++)
	{
		ent = g_edicts + i;

		if (!ent->inuse || !ent->client ||
			ent->client->resp.entered != ENTERED_ENTERED ||
			ent->client->resp.osp_r208 != 1 ||
			(ent->flags & FL_BOT))
			continue;

		if (hs_mode == 2)
			value = ent->client->resp.score;
		else
		{
			if (ent->client->resp.enterframe < sync_frame)
				framecnt = level.framenum - sync_frame + 1;
			else
				framecnt = level.framenum - ent->client->resp.enterframe + 1;

			if (framecnt < 1)
				framecnt = 1;

			value = ent->client->resp.score * 36000 / framecnt;

			// A short game cannot inflate the frags-per-hour past 1250.
			if (value > 1250 && framecnt < 600)
				value = 1250;

			if (ent->client->resp.score * 100 / (int)fraglimit->value < 90)
				allowed = 0;
		}

		if (allowed)
		{
			for (rowi = 0; rowi < 10; rowi++)
			{
				if (value >= atoi (p_table[rowi].score) ||
					!strcmp (p_table[rowi].name, "<empty>"))
				{
					for (j = 9; j > rowi; j--)
					{
						strcpy (p_table[j].name, p_table[j - 1].name);
						strcpy (p_table[j].score, p_table[j - 1].score);
						strcpy (p_table[j].date, p_table[j - 1].date);
					}

					strcpy (p_table[rowi].name, ent->client->pers.netname);
					sprintf (stamp, "%d", value);
					strcpy (p_table[rowi].score, stamp);
					OSP_highscoreDate (stamp);
					strcpy (p_table[rowi].date, stamp);
					p_table[rowi].isnew = 1;
					break;
				}
			}
		}
		else
			rowi = 10;

		break;
	}

	if (rowi < 10)
	{
		OSP_formatHighScores ();
		OSP_writeHighScores ();
	}
}

// gamex86.dll: 1002C0E6..1002C3E8
// gamei386.so: 0005D244..0005D568
void OSP_loadHighScores (void)
{
	char	name[64];
	char	score[64];
	char	date[64];
	char	file[64];
	char	dir[64];
	int		i;
	FILE	*f = NULL;
	cvar_t	*gamedir;
	cvar_t	*basedir;
	cvar_t	*port;
	cvar_t	*hsdir;

	gamedir = gi.cvar ("gamedir", "tourney", 0);
	basedir = gi.cvar ("basedir", ".", 0);
	port = gi.cvar ("port", ".", 0);
	hsdir = gi.cvar ("client_highscoredir", "highscores", 0);

	if (gamedir && basedir)
	{
		sprintf (dir, "%s/%s", basedir->string, gamedir->string);
		sprintf (file, "%s/%s/%d/%s", dir, hsdir->string, (int)port->value,
			level.mapname);

		f = fopen (file, "r");

		if (f)
		{
			if (!OSP_readLine (f, name, score, date))
			{
				fclose (f);
				return;
			}

			if ((hs_mode == 1 && (strcmp (name, "FL") || hs_limit != atoi (score))) ||
				(hs_mode == 2 && (strcmp (name, "TL") || hs_limit != atoi (score))))
			{
				gi.dprintf ("Server parameters changed, resetting highscores.\n");
				fclose (f);
				OSP_writeHighScores ();
				return;
			}

			for (i = 0; i < 10; i++)
			{
				if (OSP_readLine (f, name, score, date) != 3)
				{
					gi.dprintf ("Not all players (high scores) loaded.\n");
					fclose (f);
					return;
				}

				strcpy (p_table[i].name, name);
				strcpy (p_table[i].score, score);
				strcpy (p_table[i].date, date);
			}

			fclose (f);
			gi.dprintf ("High scores loaded.\n");
		}
		else
		{
			if (!OSP_makeHSDir (dir))
				return;

			gi.dprintf ("\nNew \"%s\" created.\n\n", file);
			OSP_writeHighScores ();
		}
	}
}

// gamex86.dll: 1002C3E8..1002C5EA
// gamei386.so: 0005D568..0005D777
void OSP_writeHighScores (void)
{
	char	line[120];
	char	file[64];
	char	dir[64];
	int		i;
	FILE	*f = NULL;
	cvar_t	*gamedir;
	cvar_t	*basedir;
	cvar_t	*port;
	cvar_t	*hsdir;

	gamedir = gi.cvar ("gamedir", "tourney", 0);
	basedir = gi.cvar ("basedir", ".", 0);
	port = gi.cvar ("port", ".", 0);
	hsdir = gi.cvar ("client_highscoredir", "highscores", 0);

	if (gamedir && basedir)
	{
		sprintf (dir, "%s/%s", basedir->string, gamedir->string);
		sprintf (file, "%s/%s/%d/%s", dir, hsdir->string, (int)port->value,
			level.mapname);

		f = fopen (file, "w+");

		if (!f)
		{
			gi.dprintf ("Couldn't write high score table (%d)\n", errno);
			return;
		}

		if (hs_mode == 2)
			sprintf (line, "TL\t%d\n", hs_limit);
		else
			sprintf (line, "FL\t%d\n", hs_limit);
		fputs (line, f);

		for (i = 0; i < 10; i++)
		{
			sprintf (line, "%s\t%s\t%s\n", p_table[i].name, p_table[i].score,
				p_table[i].date);
			fputs (line, f);
		}

		fclose (f);
	}
}

// gamex86.dll: 1002C5EA..1002C71C
// gamei386.so: 0005D778..0005D8D4
qboolean OSP_makeHSDir (char *base)
{
	char	num[128];
	char	dir[1024];
	cvar_t	*port;
	cvar_t	*hsdir;

	port = gi.cvar ("port", ".", 0);
	hsdir = gi.cvar ("client_highscoredir", "highscores", 0);

	sprintf (dir, "%s/%s", base, hsdir->string);

	// mkdir() is called unprototyped, and the original really did write two
	// different call shapes per platform.
#ifdef _WIN32
	if (mkdir (dir) && errno == ENOENT)
#else
	if (mkdir (dir, 0755) && errno == ENOENT)
#endif
	{
		gi.dprintf ("Couldn't make %s, aborting.\n", dir);
		// The two cvar names really do differ -- this one is the target's own
		// typo.
		gi.cvar_set ("client_hiscores", "0");
		return false;
	}

	sprintf (num, "/%d", (int)port->value);
	strcat (dir, num);

#ifdef _WIN32
	if (mkdir (dir) && errno == ENOENT)
#else
	if (mkdir (dir, 0755) && errno == ENOENT)
#endif
	{
		gi.dprintf ("Couldn't make %s, aborting.\n", dir);
		gi.cvar_set ("client_highscores", "0");
		return false;
	}

	return true;
}

// gamex86.dll: 1002C71C..1002C8B5
// gamei386.so: 0005D8D4..0005D9E4
int OSP_readLine (FILE *f, char *a, char *b, char *c)
{
	char	line[1024];
	char	*linep;
	char	*pstr;
	char	*t;						// invented name
	char	*val2;						// invented name

	if (!fgets (line, 1024, f))
	{
		gi.dprintf ("Error reading highscores file\n");
		return 0;
	}

	linep = strchr (line, '\r');
	if (linep)
		*linep = 0;

	linep = strchr (line, '\n');
	if (linep)
		*linep = 0;

	t = line;
	strcpy (a, t);

	linep = strchr (t, '\t');
	if (!linep)
		return 1;

	*linep++ = 0;
	pstr = linep;
	strcpy (a, t);
	strcpy (b, pstr);

	linep = strchr (pstr, '\t');
	if (!linep)
		return 2;

	*linep++ = 0;
	val2 = linep;
	strcpy (b, pstr);
	strcpy (c, val2);
	return 3;
}

// gamex86.dll: 1002C8B5..1002C9C0
// gamei386.so: 0005D9E4..0005DAFC
void OSP_highscoreDate (char *out)
{
	time_t		tval;
	char		*month;
	struct tm	*tm;
	int			monidx;

	time (&tval);
	tm = localtime (&tval);
	monidx = tm->tm_mon;

	if (monidx == 0)
		month = "Jan";
	else if (monidx == 1)
		month = "Feb";
	else if (monidx == 2)
		month = "Mar";
	else if (monidx == 3)
		month = "Apr";
	else if (monidx == 4)
		month = "May";
	else if (monidx == 5)
		month = "Jun";
	else if (monidx == 6)
		month = "Jul";
	else if (monidx == 7)
		month = "Aug";
	else if (monidx == 8)
		month = "Sep";
	else if (monidx == 9)
		month = "Oct";
	else if (monidx == 10)
		month = "Nov";
	else
		month = "Dec";

	sprintf (out, "%.2d_%s_%.2d", tm->tm_mday, month, tm->tm_year % 100);
}
