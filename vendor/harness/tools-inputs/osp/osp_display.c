// osp_display.c -- <INVENTED FILENAME>. The MOTD, match-params and
// scoreboard/player text builders.

#include "g_local.h"
#include "bl_main.h"


// Read motd.txt into nine 32-column lines and build the whole layout string
// match_motd out of them, followed by the fixed OSP credit block.  motd_center
// picks between a left-aligned "xl 4 " / "yb %d" block that grows upwards from
// the bottom of the screen and a centred "xv 32 " / "yv %d" one that starts at
// y = 64.
// gamex86.dll: 1002D810..1002DDAE
// gamei386.so: 00051608..00051B76
void OSP_setMOTD (void)
{
	char	motdpage[9][33];
	char	buf[1024];
	int		y;
	int		len = 0;
	int		lines;
	FILE	*f = NULL;
	// The four cvar lookups are DECLARATION INITIALISERS, which is what puts
	// the pooled "motd.txt" address between `motdfile` and `center` in the
	// frame: gcc creates a temp while expanding an initialiser, so it lands
	// between the variable it initialises and the next declaration.  Written
	// as plain statements the temp comes after every declared local instead,
	// and real's ELF puts it fourth of five.  The literal is repeated rather
	// than cached -- real's PE pushes two distinct .rdata copies.
	cvar_t	*gamedir = gi.cvar ("gamedir", "ospdm", CVAR_SERVERINFO);
	cvar_t	*basedir = gi.cvar ("basedir", ".", 0);
	cvar_t	*motdfile = gi.cvar ("motd_file", "motd.txt", 0);
	cvar_t	*center = gi.cvar ("motd_center", "0", 0);
	int		i;
	char	c;

	if (gamedir && basedir)
	{
		char	path[64] = {0};
		char	*p = path;

		sprintf (path, "%s/%s/", basedir->string, gamedir->string);
		if (motdfile)
			strcat (path, motdfile->string);
		else
			strcat (path, "motd.txt");

		f = fopen (p, "r");

		if (f)
		{
			if (!motd_read)
			{
				gi.dprintf ("MOTD: Reading from \"%s\"\n", motdfile->string);
				motd_read = 1;
			}
			for (lines = 0; lines < 9; lines++)
			{
				for (i = 0; i < 33; i++)
					motdpage[lines][i] = 0;

				for (i = 0; i < 33; i++)
				{
					c = fgetc (f);
					if (c == -1 || c == '\n')
						break;
					motdpage[lines][i] = c;
				}

				// Windows' CRT translates CRLF to LF on a text-mode fgetc, so a
				// motd.txt with Windows line endings never shows the CR to this
				// loop there; on Unix fopen's "r" does no such translation.
#ifndef _WIN32
				if (i && motdpage[lines][i - 1] == '\r')
					motdpage[lines][i - 1] = 0;
#endif

				if (i == 33)
				{
					motdpage[lines][32] = 0;
					while (c != '\n' && c != -1)
						c = fgetc (f);
				}

				if (c == -1)
					break;
			}

			if (i)
				lines++;
			if (lines > 9)
				lines = 9;

			fclose (f);
		}
		else
		{
			gi.dprintf ("MOTD: Couldn't open \"%s\"\n", motdfile->string);
			lines = 0;
		}
	}
	else
	{
		gi.dprintf ("MOTD: Couldn't find \"%s\"\n", motdfile->string);
		lines = 0;
	}

	if (!(int)center->value)
	{
		y = (9 - lines) * 8 - 136;
		strcpy (match_motd, "xl 4 ");
		len = strlen (match_motd);

		for (i = 0; i < lines; i++, y += 8)
		{
			Com_sprintf (buf, 1024, "yb %d string \"%s\"", y, motdpage[i]);
			strcpy (match_motd + len, buf);
			len += strlen (buf);
		}
	}
	else
	{
		y = 64;
		strcpy (match_motd, "xv 32 ");
		len = strlen (match_motd);

		for (i = 0; i < lines; i++, y += 8)
		{
			Com_sprintf (buf, 1024, "yv %d string \"%s\"", y, motdpage[i]);
			strcpy (match_motd + len, buf);
			len += strlen (buf);
		}

		strcat (match_motd, "xl 4 ");
	}

	strcat (match_motd, "yb -56 string \"");
	strcat (match_motd, "OSP Tourney DM v(2.75)");
	strcat (match_motd, "\" yb -48 string2 \"Orange Smoothie Productions\"");
	strcat (match_motd, "yb -40 string2 \"http://www.OrangeSmoothie.org\"");
	strcat (match_motd, "yb -32 string2 \"");
	strcat (match_motd, "rhea@OrangeSmoothie.org");
	strcat (match_motd, "\"");
}

// gamex86.dll: 1002DDAE..1002DDCC
// gamei386.so: 00051B78..00051BAB
void OSP_showMOTD (edict_t *ent)
{
	gi.WriteByte (svc_layout);
	gi.WriteString (match_motd);
}

// Build the "match parameters" page shown at the start of a match into
// match_info.  Three layouts, one per mode: the qualifier lists the number of
// qualifying spots, team play lists both teams' head counts and skins and both
// friendly-fire switches, and 1v1 lists the two team names either side of a
// "vs." plus the overtime rule.  Every value is written into tmp, greened by
// adding 128 to each byte, and then substituted into the layout line.
// m_mode 0 (plain deathmatch) builds nothing at all.
// gamex86.dll: 1002DDCC..1002F1FA
// gamei386.so: 00051BAC..00052FEC
void OSP_setShowParams (void)
{
	char	buf[80];
	char	tmp[80];
	cvar_t	*host;
	int		x;

	host = gi.cvar ("hostname", "noname", 0);

	if (m_mode == 1)
	{
		sprintf (buf, "xv 2 yv 0 string \"Match: %s\"", host->string);
		strcpy (match_info, buf);

		sprintf (buf, "yv 8 string2 \"------------------------------\"");
		strcat (match_info, buf);

		sprintf (buf, "yv 16 string \"Number of connected players: %i\"", active_clients);
		strcat (match_info, buf);

		sprintf (buf, "yv 24 string \"Number of qualifying spots : %d\"", (int)qualifier_numspots->value);
		strcat (match_info, buf);

		sprintf (tmp, "%s (%s)", level.level_name, level.mapname);
		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 40 string2 \"Map: %s\"", tmp);
		strcat (match_info, buf);

		sprintf (tmp, "%d", (int)dmflags->value);
		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 48 string2 \"DM Flags: %s\"", tmp);
		strcat (match_info, buf);

		if ((int)fraglimit->value)
			sprintf (tmp, "%d", (int)fraglimit->value);
		else
			sprintf (tmp, "NONE");

		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 56 string2 \"Fraglimit: %s\"", tmp);
		strcat (match_info, buf);

		if ((int)timelimit->value)
		{
			if ((int)timelimit->value == 1)
				sprintf (tmp, "1 minute");
			else
				sprintf (tmp, "%d minutes", (int)timelimit->value);
		}
		else
			sprintf (tmp, "NONE");

		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 64 string2 \"Timelimit: %s\"", tmp);
		strcat (match_info, buf);

		if ((int)hook_enable->value)
			sprintf (tmp, "ENABLED");
		else
			sprintf (tmp, "DISABLED");

		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 72 string2 \"The Hook : %s\"", tmp);
		strcat (match_info, buf);

		sprintf (buf, "yv 104 string \"Good Luck!!\"");
		strcat (match_info, buf);

		sprintf (buf, "yv 80 string2 \"Removed Items:\"xv 10 yv 88 string \"");
		strcat (match_info, buf);

		buf[0] = 0;
		OSP_listDisabledItems (buf);
		strcat (buf, "\"");
		strcat (match_info, buf);
	}
	else if (m_mode == 2)
	{
		sprintf (buf, "xv 2 yv 0 string \"Match: %s\"", host->string);
		strcpy (match_info, buf);

		sprintf (buf, "yv 8 string2 \"------------------------------\"");
		strcat (match_info, buf);

		sprintf (tmp, "# of players:");
		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 16 string \"%s %s %i\"", teams[0].netname, tmp,
				 OSP_teamCount (0));
		strcat (match_info, buf);

		sprintf (buf, "yv 24 string \"%s %s %i\"", teams[1].netname, tmp,
				 OSP_teamCount (1));
		strcat (match_info, buf);

		sprintf (tmp, "skin:");
		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 40 string \"%s %s %s\"", teams[0].netname, tmp, teams[0].skin);
		strcat (match_info, buf);

		sprintf (buf, "yv 48 string \"%s %s %s\"", teams[1].netname, tmp, teams[1].skin);
		strcat (match_info, buf);

		sprintf (tmp, "%s (%s)", level.level_name, level.mapname);
		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 64 string2 \"Map: %s\"", tmp);
		strcat (match_info, buf);

		sprintf (tmp, "%d", (int)dmflags->value);
		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 72 string2 \"DM Flags : %s\"", tmp);
		strcat (match_info, buf);

		if ((int)fraglimit->value)
			sprintf (tmp, "%d", (int)fraglimit->value);
		else
			sprintf (tmp, "NONE");

		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 80 string2 \"Fraglimit: %s\"", tmp);
		strcat (match_info, buf);

		if ((int)timelimit->value)
		{
			if ((int)timelimit->value == 1)
				sprintf (tmp, "1 minute");
			else
				sprintf (tmp, "%d minutes", (int)timelimit->value);
		}
		else
			sprintf (tmp, "NONE");

		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 88 string2 \"Timelimit: %s\"", tmp);
		strcat (match_info, buf);

		if ((int)hook_enable->value)
			sprintf (tmp, "ENABLED");
		else
			sprintf (tmp, "DISABLED");

		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 96 string2 \"The Hook : %s\"", tmp);
		strcat (match_info, buf);

		if (!(int)team_overtime_mode->value)
			sprintf (tmp, "NONE (match can end in a tie)");
		else if ((int)team_overtime_mode->value == 1)
			sprintf (tmp, "Sudden Death (first death decides)");
		else if ((int)team_overtime_time->value == 1)
			sprintf (tmp, "Timed round (1 minute)");
		else
			sprintf (tmp, "Timed round (%d minutes)",
					 (int)team_overtime_time->value);

		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 104 string2 \"Overtime : %s\"", tmp);
		strcat (match_info, buf);

		if (teams[0].osp_m11c)
			sprintf (tmp, "YES");
		else
			sprintf (tmp, "NO");

		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 112 string2 \"Hurt Team: %s\"", tmp);
		strcat (match_info, buf);

		if (teams[0].osp_m120)
			sprintf (tmp, "YES");
		else
			sprintf (tmp, "NO");

		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 120 string2 \"Hurt Self: %s\"", tmp);
		strcat (match_info, buf);

		sprintf (buf, "yv 152 string \"Good Luck!!\"");
		strcat (match_info, buf);

		sprintf (buf, "yv 128 string2 \"Removed Items:\"xv 10 yv 136 string \"");
		strcat (match_info, buf);

		buf[0] = 0;
		OSP_listDisabledItems (buf);
		strcat (buf, "\"");
		strcat (match_info, buf);
	}
	else if (m_mode == 3)
	{
		sprintf (buf, "xv 2 yv 0 string \"Match: %s\"", host->string);
		strcpy (match_info, buf);

		sprintf (buf, "yv 8 string2 \"------------------------------\"");
		strcat (match_info, buf);

		sprintf (tmp, "vs.");
		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 16 string \"*** %s %s %s ***\"", teams[0].netname, tmp,
				 teams[1].netname);
		strcat (match_info, buf);

		sprintf (tmp, "%s (%s)", level.level_name, level.mapname);
		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 32 string2 \"Map: %s\"", tmp);
		strcat (match_info, buf);

		sprintf (tmp, "%d", (int)dmflags->value);
		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 40 string2 \"DM Flags : %s\"", tmp);
		strcat (match_info, buf);

		if ((int)fraglimit->value)
			sprintf (tmp, "%d", (int)fraglimit->value);
		else
			sprintf (tmp, "NONE");

		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 48 string2 \"Fraglimit: %s\"", tmp);
		strcat (match_info, buf);

		if ((int)timelimit->value)
		{
			if ((int)timelimit->value == 1)
				sprintf (tmp, "1 minute");
			else
				sprintf (tmp, "%d minutes", (int)timelimit->value);
		}
		else
			sprintf (tmp, "NONE");

		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 56 string2 \"Timelimit: %s\"", tmp);
		strcat (match_info, buf);

		if ((int)hook_enable->value)
			sprintf (tmp, "ENABLED");
		else
			sprintf (tmp, "DISABLED");

		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 64 string2 \"The Hook : %s\"", tmp);
		strcat (match_info, buf);

		if (!(int)team_overtime_mode->value)
			sprintf (tmp, "NONE (match can end in a tie)");
		else if ((int)team_overtime_mode->value == 1)
			sprintf (tmp, "Sudden Death (first death decides)");
		else if ((int)team_overtime_time->value == 1)
			sprintf (tmp, "Timed round (1 minute)");
		else
			sprintf (tmp, "Timed round (%d minutes)",
					 (int)team_overtime_time->value);

		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 72 string2 \"Overtime : %s\"", tmp);
		strcat (match_info, buf);

		if (teams[0].osp_m120)
			sprintf (tmp, "YES");
		else
			sprintf (tmp, "NO");

		for (x = 0; x < strlen (tmp); x++)
			tmp[x] += 128;
		sprintf (buf, "yv 80 string2 \"Hurt Self: %s\"", tmp);
		strcat (match_info, buf);

		sprintf (buf, "yv 112 string \"Good Luck!!\"");
		strcat (match_info, buf);

		sprintf (buf, "yv 88 string2 \"Removed Items:\"xv 10 yv 96 string \"");
		strcat (match_info, buf);

		buf[0] = 0;
		OSP_listDisabledItems (buf);
		strcat (buf, "\"");
		strcat (match_info, buf);
	}
}

// gamex86.dll: 1002F1FA..1002F218
// gamei386.so: 00052FEC..0005301F
void OSP_showParams (edict_t *ent)
{
	gi.WriteByte (svc_layout);
	gi.WriteString (match_info);
}

// Render the scoreboard.  `list` is OSP_DoRankSort's ordered client numbers,
// `count` how many of them to draw (capped at ten), `ent` the viewer.  Four
// row layouts: (a) plain DM in progress, (b) intermission, (c) a running
// match, (d) warmup.  resp.osp_r2b0 is the viewer's row cursor and
// resp.osp_r2ac the client number it currently points at, which is what
// OSP_showPlayer then reads.  The page stops growing at 1400 bytes.
// gamex86.dll: 1002F218..1003035E
// gamei386.so: 00053020..00053EC3
void OSP_showScores (int *list, int count, edict_t *ent)
{
	char		rline[200];
	char		chase[32];
	char		headbuf[1024];
	char		buf[1400];
	char		time[32];
	int			nchars;
	int			outlen;
	int			i;
	int			eff;
	int			nframes;
	int			y;
	int			endframe;
	gclient_t	*cl;
	edict_t		*other;

	outlen = 0;
	buf[0] = 0;
	outlen = strlen (buf);

	if (level.intermissiontime != 0)
		endframe = endlvl_frame;
	else
		endframe = level.framenum;

	if (active_clients)
		ent->client->resp.osp_r2b0 %= active_clients;
	else
		ent->client->resp.osp_r2b0 = -1;

	if (count > 10)
	{
		count = 10;
		ent->client->resp.osp_r2b0 %= count;
	}

	for (i = 0; i < count; i++)
	{
		cl = game.clients + list[i];
		other = g_edicts + 1 + list[i];
		y = i * 8 + 34;

		if (i == ent->client->resp.osp_r2b0)
			ent->client->resp.osp_r2ac = list[i] + 1;

		if (i == 9 && ent->client->resp.osp_r208 > 10)
		{
			other = ent;
			cl = ent->client;
			y += 2;
			i = cl->resp.osp_r208 - 1;
		}

		if (cl->resp.enterframe < sync_frame)
			nframes = endframe - sync_frame + 1;
		else
			nframes = endframe - cl->resp.enterframe + 1;

		if (nframes < 1)
		{
			nframes = 1;
			cl->resp.enterframe = endframe + 1;
			cl->resp.osp_r2d4 = 1;
		}

		if (cl->resp.score < 1)
			eff = 0;
		else if (!cl->resp.osp_r014 || !(cl->resp.osp_r014 + cl->resp.score))
			eff = 100;
		else
			eff = cl->resp.score * 100 /
				  (cl->resp.score + cl->resp.osp_r014);

		// Row zero doubles as the ngWorldStats leader/champion banner.
		if (!i)
		{
			if (cl->resp.entered == ENTERED_ENTERED)
			{
				if (level.intermissiontime == 0)
					Com_sprintf (headbuf, 1024,
						"client 80 -16 %i %i %i %i xv 112 picn tag1 xv 114 string \"%s\""
						"yv -8 string2 \"Frags: %i\"yv 0 string2 \"Eff%% : %i%%\""
						"yv 8 string2 \"FPH  : %i\"xv 0 yv -24 cstring2 \"Current Leader:\"",
						list[i], 0, 0, 0, cl->pers.netname, cl->resp.score,
						eff, cl->resp.score * 36000 / nframes);
				else
				{
					OSP_getDateInfo (time);
					Com_sprintf (headbuf, 1024,
						"client 80 -16 %i %i %i %i xv 112 picn tag1 xv 114 string \"%s\""
						"yv -8 string2 \"Frags: %i\"yv 0 string2 \"Eff%% : %i%%\""
						"yv 8 string2 \"FPH  : %i\"xv 0 yv -24 cstring2 \"Champion:\""
						"yv 17 cstring2 \"%s\"",
						list[i], 0, 0, 0, cl->pers.netname, cl->resp.score,
						eff, cl->resp.score * 36000 / nframes, time);
				}

				nchars = strlen (headbuf);
				strcpy (buf + outlen, headbuf);
				outlen += nchars;
			}

			if (sync_stat == 8)
				Com_sprintf (headbuf, 1024, "xv -16 yv 26 string \"Player          Frgs Dths Eff%% FPH Time Ping\"xv -40 ");
			else if (level.intermissiontime != 0)
				Com_sprintf (headbuf, 1024, "xv -24 yv 26 string \"Player          Frgs Dths Eff%% FPH Time Ping\"xv -56 ");
			else if (sync_stat == 4)
				Com_sprintf (headbuf, 1024, "xv 32 yv 26 string \"Player          Frags Deaths Ping\"xv 0 ");
			else
				Com_sprintf (headbuf, 1024, "xv 8 yv 26 string \"Player          Frags Deaths Time Ping\"xv 8 ");

			nchars = strlen (headbuf);
			strcpy (buf + outlen, headbuf);
			outlen += nchars;
		}

		if (sync_stat > 2 || level.intermissiontime != 0)
		{
			char		mark;

			if ((int)qualifier_numspots->value &&
				cl->resp.osp_r208 <= (int)qualifier_numspots->value)
				mark = '*';
			else
				mark = ' ';

			if (sync_stat == 8)
			{
				// (a) plain DM: no rank marker, and the selected rline is
				// pulled out to xv -48 with a leading \r / 0x8d.
				if (cl->resp.entered == ENTERED_ENTERED)
					sprintf (rline, "%2i %-16s%4i  %3i %3i%%%4i %3i  %4i",
							 i + 1, cl->pers.netname, cl->resp.score,
							 cl->resp.osp_r014, eff,
							 cl->resp.score * 36000 / nframes, nframes / 600,
							 cl->ping);
				else if (other->osp_e39c)
					sprintf (rline, "   %-16s<<<Referee>>>      %3i  %4i",
							 cl->pers.netname, nframes / 600, cl->ping);
				else if (cl->resp.entered == 2)
					sprintf (rline, "   %-16s(Observing)        %3i  %4i",
							 cl->pers.netname, nframes / 600, cl->ping);
				else if (cl->resp.entered == 16)
					sprintf (rline, "   %-16s(Autocam)          %3i  %4i",
							 cl->pers.netname, nframes / 600, cl->ping);
				else
				{
					sprintf (chase, "(Chasing %s)",
							 cl->chase_target->client->pers.greenname);

					if (strlen (cl->chase_target->client->pers.netname) == 15)
						sprintf (rline, "   %-16s%-24s%3i", cl->pers.netname,
								 chase, cl->ping);
					else
						sprintf (rline, "   %-16s%-24s%4i", cl->pers.netname,
								 chase, cl->ping);
				}

				if (other != ent)
				{
					if (i == ent->client->resp.osp_r2b0)
					{
						Com_sprintf (headbuf, 1024,
									 "xv -48 yv %i string2 \"\x8d%s\"xv -40 ",
									 y, rline);
						goto appended;
					}
					Com_sprintf (headbuf, 1024, "yv %i string2 \"%s\"", y, rline);
				}
				else
				{
					if (i == ent->client->resp.osp_r2b0)
					{
						Com_sprintf (headbuf, 1024,
									 "xv -48 yv %i string \"\r%s\"xv -40 ",
									 y, rline);
						goto appended;
					}
					Com_sprintf (headbuf, 1024, "yv %i string \"%s\"", y, rline);
				}
			}
			else if (level.intermissiontime != 0)
			{
				// (b) intermission: same columns, rank marker, four spaces.
				if (cl->resp.entered == ENTERED_ENTERED)
					sprintf (rline, "%c%2i %-16s%4i  %3i %3i%%%4i %3i  %4i",
							 mark, i + 1, cl->pers.netname, cl->resp.score,
							 cl->resp.osp_r014, eff,
							 cl->resp.score * 36000 / nframes, nframes / 600,
							 cl->ping);
				else if (other->osp_e39c)
					sprintf (rline, "    %-16s<<<REFEREE>>>      %3i  %4i",
							 cl->pers.netname, nframes / 600, cl->ping);
				else if (cl->resp.entered == 2)
					sprintf (rline, "    %-16s(Observing)        %3i  %4i",
							 cl->pers.netname, nframes / 600, cl->ping);
				else if (cl->resp.entered == 16)
					sprintf (rline, "    %-16s(Autocam)          %3i  %4i",
							 cl->pers.netname, nframes / 600, cl->ping);
				else
				{
					sprintf (chase, "(Chasing %s)",
							 cl->chase_target->client->pers.greenname);

					if (strlen (cl->chase_target->client->pers.netname) == 15)
						sprintf (rline, "    %-16s%-24s%3i", cl->pers.netname,
								 chase, cl->ping);
					else
						sprintf (rline, "    %-16s%-24s%4i", cl->pers.netname,
								 chase, cl->ping);
				}

				if (other != ent)
					Com_sprintf (headbuf, 1024, "yv %i string2 \"%s\"", y, rline);
				else
					Com_sprintf (headbuf, 1024, "yv %i string \"%s\"", y, rline);
			}
			else
			{
				// (c) match running: frags/deaths/ping only.
				if (cl->resp.entered == ENTERED_ENTERED)
					sprintf (rline, "%c%2i %-16s%4i   %3i   %4i", mark, i + 1,
							 cl->pers.netname, cl->resp.score,
							 cl->resp.osp_r014, cl->ping);
				else if (other->osp_e39c)
					sprintf (rline, "    %-16s<<<Referee>>>%4i",
							 cl->pers.netname, cl->ping);
				else if (cl->resp.entered == 2)
					sprintf (rline, "    %-16s(Observing)  %4i",
							 cl->pers.netname, cl->ping);
				else if (cl->resp.entered == 16)
					sprintf (rline, "    %-16s(Autocam)    %4i",
							 cl->pers.netname, cl->ping);
				else
				{
					sprintf (chase, "(Chasing #%d)",
							 cl->chase_target->client->resp.osp_r208);
					sprintf (rline, "    %-16s%-13s%4i", cl->pers.netname, chase,
							 cl->ping);
				}

				if (other != ent)
					Com_sprintf (headbuf, 1024, "yv %i string2 \"%s\"", y, rline);
				else
					Com_sprintf (headbuf, 1024, "yv %i string \"%s\"", y, rline);
			}
		}
		else
		{
			// (d) warmup: no rank column, ready state instead of scores.
			// Every arm wraps its OWN rline -- six separate Com_sprintf sites and
			// six separate wrap literals in the PE, no shared tail and no goto.
			if (other->osp_e39c == 1 ||
				(cl->resp.entered != ENTERED_ENTERED && other->osp_e39c == 2))
			{
				sprintf (rline, "%-16s<<<Referee>>>%3i  %4i", cl->pers.netname,
						 nframes / 600, cl->ping);
				Com_sprintf (headbuf, 1024, "yv %i string2 \"%s\"", y, rline);
			}
			else if (cl->resp.entered == 2)
			{
				sprintf (rline, "%-16s(Observing)  %3i  %4i", cl->pers.netname,
						 nframes / 600, cl->ping);
				Com_sprintf (headbuf, 1024, "yv %i string2 \"%s\"", y, rline);
			}
			else if (cl->resp.entered == 16)
			{
				sprintf (rline, "%-16s(Autocam)    %3i  %4i", cl->pers.netname,
						 nframes / 600, cl->ping);
				Com_sprintf (headbuf, 1024, "yv %i string2 \"%s\"", y, rline);
			}
			else if (cl->chase_target)
			{
				sprintf (chase, "(Chasing %s)",
						 cl->chase_target->client->pers.greenname);
				sprintf (rline, "%-16s%-18s%4i", cl->pers.netname, chase,
						 nframes / 600, cl->ping);
				Com_sprintf (headbuf, 1024, "yv %i string2 \"%s\"", y, rline);
			}
			else if (cl->resp.osp_r20c)
			{
				sprintf (rline, "%-16s*** READY ***%3i  %4i", cl->pers.netname,
						 nframes / 600, cl->ping);
				Com_sprintf (headbuf, 1024, "yv %i string \"%s\"", y, rline);
			}
			else
			{
				sprintf (rline, "%-16s [NOT READY] %3i  %4i", cl->pers.netname,
						 nframes / 600, cl->ping);
				Com_sprintf (headbuf, 1024, "yv %i string2 \"%s\"", y, rline);
			}
		}

appended:
		nchars = strlen (headbuf);
		if (outlen + nchars > sizeof (buf))
			break;

		strcpy (buf + outlen, headbuf);
		outlen += nchars;
	}

	if ((int)gi.cvar ("nglog_worldstats", "0", 0)->value)
		ent->client->ps.stats[28] = 0x62b;

	if (level.intermissiontime != 0 && sync_stat != 8)
		ent->client->ps.stats[27] = 0x62a;
	else
		ent->client->ps.stats[27] = 0x629;

	gi.WriteByte (svc_layout);
	gi.WriteString (buf);

	if (level.intermissiontime != 0 &&
		ent->client->resp.entered == ENTERED_ENTERED)
	{
		char		*cur;

		strcpy (old_scores, buf);

		if ((cur = strchr (old_scores, '\r')))
			*cur = ' ';
	}
}

// The per-player stats page behind "showinfo <n>": name, an underline as
// long as the name, frags/efficiency, deaths/frags-per-hour, suicides/rank,
// one line per weapon the player has fired, and the two damage totals.  Built
// as one layout string and pushed straight down the wire; the caller does the
// unicast.  Efficiency is frags * 100 / (frags + deaths), and frags-per-hour
// is the high-score table's own frags * 36000 / frames.
// gamex86.dll: 1003035E..100309E0
// gamei386.so: 00053EC4..0005441D
void OSP_showPlayer (edict_t *ent)
{
	char		line[256];
	char		name[256];
	char		buf[1400];
	int			cid;
	unsigned int	i;
	int			frames;
	float		eff;
	int			y;
	int			frags;
	int			deaths;
	int			suicides;
	edict_t		*other;
	int			found;

	found = 0;

	if (ent->client->resp.osp_r2ac < 1)
	{
		gi.cprintf (ent, PRINT_CHAT, "** Sorry, illegal player view!\n");
		Cmd_InvUse_f (ent);
		return;
	}

	other = g_edicts + ent->client->resp.osp_r2ac;

	if (!other->inuse || !other->client)
	{
		gi.cprintf (ent, PRINT_CHAT, "** Sorry, player has disconnected!\n");
		Cmd_InvUse_f (ent);
		return;
	}

	frags = other->client->resp.score;
	deaths = other->client->resp.osp_r014;
	suicides = other->client->resp.osp_r2c0;

	sprintf (name, "Player: %s (%s)", other->client->pers.greenname, other->client->resp.osp_r0f4);
	sprintf (buf, "xv 0 yv 0 string2 \"%s\"", name);

	strcpy (line, "_");
	{
		for (cid = 0; cid < strlen (name) - 1 && cid < 59; cid++)
			strcat (line, "_");
	}

	sprintf (name, "yv 4 string2 \"%s\"", line);
	strcat (buf, name);

	y = 18;

	// The zero case is a two-part disjunction whose second half is dead --
	// `frags < 1` already covers `!frags` -- and the redundancy is the
	// original's.
	if (frags < 1 || (!deaths && !frags))
		eff = 0;
	else
		eff = 100.0 * frags / (0.0 + frags + deaths);

	sprintf (line, "yv %d string \"Frags   :%3d     Efficiency: %.1f%%\"",
			 y, frags, eff);
	strcat (buf, line);
	y += 8;

	if (level.intermissiontime != 0)
		frames = endlvl_frame;
	else
		frames = level.framenum;

	{
		int		i;
		int		fph;			// invented name

		if (other->client->resp.enterframe < sync_frame)
			i = frames - sync_frame + 1;
		else
			i = frames - other->client->resp.enterframe + 1;

		if (i < 1)
		{
			i = 1;
			other->client->resp.enterframe = frames + 1;
			other->client->resp.osp_r2d4 = 1;
		}

		fph = frags * 36000 / i;
		sprintf (line, "yv %d string \"Deaths  :%3d     Frags/Hour: %d\"",
				 y, deaths, fph);
	}
	strcat (buf, line);
	y += 8;

	sprintf (line, "yv %d string \"Suicides: %2d     Rank: %d/%d\"",
			 y, suicides, other->client->resp.osp_r208, active_clients);
	strcat (buf, line);
	y += 16;

	cid = other->client->resp.clientid;

	{
		int				index;			// invented name

		for (i = 0; i < 10; i++)
		{
			index = a_info[i].index;
			if (p_acc[cid].shots[index])
			{
				sprintf (line, "yv %d string \"%s %.1f%% (%d/%d hits)\"", y,
						 a_info[i].name,
						 (double)(100 * p_acc[cid].hits[index]) /
						 p_acc[cid].shots[index],
						 p_acc[cid].hits[index],
						 p_acc[cid].shots[index]);
				strcat (buf, line);
				found = 1;
				y += 8;
			}
		}
	}

	if (!found)
	{
		sprintf (line, "yv %d string \"Hasn't taken a shot.\"", y);
		strcat (buf, line);
		y += 8;
	}
	else
	{
		y += 8;
		sprintf (line, "yv %d string2 \"Total damage given: %d\"", y,
				 p_acc[cid].dgiven);
		strcat (buf, line);
		y += 8;
		sprintf (line, "yv %d string2 \"Total damage rcvd : %d\"", y,
				 p_acc[cid].dtaken);
		strcat (buf, line);
		y += 8;
	}

	y += 8;
	sprintf (line, "yv %d cstring \"\x90 CONTINUE \x91\"", y);
	strcat (buf, line);

	gi.WriteByte (svc_layout);
	gi.WriteString (buf);
}
