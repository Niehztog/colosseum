// osp_players.c -- <INVENTED FILENAME>. The three scoreboard renderers and
// the team-chat command.
//
// Four named functions plus four file-statics that have no symbol and whose
// names here are therefore <INVENTED>: sayteam_location (id CTF's
// CTFSay_Team_Location), sayteam_armor (CTF's CTFSay_Team_Armor),
// sayteam_sight (the mod's own, %n) and loc_findradius (CTF's, unchanged).
//
// OSP_sayteam_cmd is id CTF's CTFSay_Team with the %-expansion table rebuilt:
// CTF's l/a/h/t/w survive, the mod adds %n, and %t -- CTF's tech report --
// now reports RUNES instead.  %r and %t are two separate case bodies with
// identical contents.

#include "g_local.h"

static edict_t *loc_findradius (edict_t *from, vec3_t org, float rad);

// <INVENTED NAME>.  Counts the members of team `team`, used only by the
// scoreboard renderers' "# Players: %i" cell.  Deliberately written in
// OSP_teamCount's exact shape -- the real PE link folds the two bodies onto
// one address.
// gamex86.dll: 100373C0..10037447
// gamei386.so: absent
static int OSP_countTeamPlayers (int team)
{
	int			i;
	int			count;

	count = 0;
	for (i = 1; i <= game.maxclients; i++)
	{
		// Must stay byte-identical to OSP_teamCount -- see the note above.
		if (!g_edicts[i].inuse || !g_edicts[i].client ||
			g_edicts[i].client->resp.team != team)
			continue;

		count++;
	}
	return count;
}


// The team scoreboard, small-roster variant.  With more than six players in
// the game it hands straight over to OSP_showBIGTeamScores, which is the same
// board without the efficiency column.
//
// Each team gets a `client 80` banner card, a column header and up to FOUR
// rows; row 3 is a "...and you" promotion that swaps the viewer in when he is
// on that team and ranked below the visible four, exactly as OSP_showScores
// does at row 9.  The two teams' sorted lists live in one pair of 2-D arrays
// and are built here rather than by OSP_DoRankSort, but with the same three
// keys: score descending, then deaths and suicides ascending.
// gamex86.dll: 1003C185..1003D40E
// gamei386.so: 0006910C..0006A3A6
void OSP_showTeamScores (edict_t *ent)
{
	int			rank[2][256];
	int			pscore[2][256];
	int			viewers[256];
	char		rowline[200];
	char		str[256];
	char		temp[1024];
	char		buf[1400];
	char		time[32];
	int			tarr[2];
	int			count[2];
	int			i;
	int			eff;
	int			nframes;
	int			m;
	int			y;
	int			sideno;
	int			kk;
	int			basey;
	int			cscore;
	int			obscount;
	int			size;
	gclient_t	*cl;
	edict_t		*player;

	y = 0;
	obscount = 0;
	size = 0;

	if (active_clients > 6)
	{
		OSP_showBIGTeamScores (ent);
		return;
	}

	tarr[0] = 0;
	tarr[1] = 1;

	for (sideno = 0; sideno < 2; sideno++)
	{
		count[sideno] = 0;

		for (i = 0; i <= game.maxclients; i++)
		{
			player = g_edicts + 1 + i;

			if (!player->inuse || !player->client)
				continue;

			// observers are collected once, on the first team's pass
			if (!sideno && player->client->resp.entered != ENTERED_ENTERED)
			{
				viewers[obscount] = i;
				obscount++;
				continue;
			}

			if (player->client->resp.team != tarr[sideno])
				continue;

			cscore = game.clients[i].resp.score;

			for (kk = 0; kk < count[sideno]; kk++)
			{
				if (cscore > pscore[sideno][kk])
					break;
				if (cscore == pscore[sideno][kk])
				{
					if (game.clients[i].resp.osp_r014 < game.clients[rank[sideno][kk]].resp.osp_r014)
						break;
					if (game.clients[i].resp.osp_r014 == game.clients[rank[sideno][kk]].resp.osp_r014 &&
						game.clients[i].resp.osp_r2c0 < game.clients[rank[sideno][kk]].resp.osp_r2c0)
						break;
				}
			}

			for (m = count[sideno]; m > kk; m--)
			{
				rank[sideno][m] = rank[sideno][m - 1];
				pscore[sideno][m] = pscore[sideno][m - 1];
			}

			rank[sideno][kk] = i;
			pscore[sideno][kk] = cscore;
			count[sideno]++;
		}

		for (i = 0; i < count[sideno]; i++)
		{
			player = g_edicts + 1 + rank[sideno][i];
			player->client->resp.osp_r208 = i + 1;
		}
	}

	buf[0] = 0;

	if ((int)gi.cvar ("nglog_worldstats", "0", 0)->value)
		ent->client->ps.stats[28] = 0x62b;

	if (level.intermissiontime != 0)
		ent->client->ps.stats[27] = 0x62a;
	else
		ent->client->ps.stats[27] = 0x629;

	size = strlen (buf);
	basey = 0;

	for (sideno = 0; sideno < 2; sideno++)
	{
		if (count[sideno] > 4)
			count[sideno] = 4;

		for (i = 0; i < count[sideno]; i++)
		{
			cl = game.clients + rank[sideno][i];
			player = g_edicts + 1 + rank[sideno][i];
			y = basey + i * 8;

			if (i == 3 && ent->client->resp.team == sideno &&
				ent->client->resp.osp_r208 > 4)
			{
				player = ent;
				cl = ent->client;
				y += 2;
				i = cl->resp.osp_r208 - 1;
			}

			if (cl->resp.enterframe < sync_frame)
				nframes = level.framenum - sync_frame + 1;
			else
				nframes = level.framenum - cl->resp.enterframe + 1;

			if (nframes < 1)
				nframes = 1;

			if (cl->resp.score < 1)
				eff = 0;
			else if (!cl->resp.osp_r014 ||
					 !(cl->resp.osp_r014 + cl->resp.score))
				eff = 100;
			else
				eff = cl->resp.score * 100 /
					  (cl->resp.score + cl->resp.osp_r014);

			// The team card is emitted once, above the first rowline.
			if (!i)
			{
				sprintf (rowline, "%i", teams[tarr[sideno]].osp_m0f8);
				for (m = 0; m < strlen (rowline); m++)
					rowline[m] += 128;

				if (level.intermissiontime == 0 || sideno)
				{
					Com_sprintf (temp, 1024,
						"client 80 %i %i %i %i %i xv 112 picn tag1 xv 114 string \"%s\""
						"yv %i string2 \"Score: %s\"yv %i string2 \"Blunders: %i\""
						"yv %i string2 \"# Players: %i\"",
						basey - 16, rank[sideno][i], 0, 0, 0,
						teams[tarr[sideno]].netname, basey - 8, rowline,
						basey, teams[tarr[sideno]].osp_m104 + teams[tarr[sideno]].osp_m108,
						basey + 8, OSP_countTeamPlayers (tarr[sideno]));

					y += 26;
					basey += 26;
				}
				else
				{
					OSP_getDateInfo (time);

					if (manual_map == 1)
						sprintf (str, "[ Voted map change ]");
					else if (manual_map == 2)
						sprintf (str, "[ Voted server config change ]");
					else if (teams[0].osp_m124 == 1)
						sprintf (str, "[ %s defeats %s: %d to %d ]",
								 teams[0].greenname, teams[1].greenname,
								 teams[0].osp_m0f8, teams[1].osp_m0f8);
					else if (teams[1].osp_m124 == 1)
						sprintf (str, "[ %s defeats %s: %d to %d ]",
								 teams[1].greenname, teams[0].greenname,
								 teams[1].osp_m0f8, teams[0].osp_m0f8);
					else
						sprintf (str, "[ Tied match! (%d to %d) ]",
								 teams[1].osp_m0f8, teams[0].osp_m0f8);

					Com_sprintf (temp, 1024,
						"client 80 %i %i %i %i %i xv 112 picn tag1 xv 114 string \"%s\""
						"yv %i string2 \"Score: %s\"yv %i string2 \"Blunders: %i\""
						"yv %i string2 \"# Players: %i\""
						"xv 0 yv -43 cstring2 \"%s\"yv -25 cstring2 \"%s\"",
						basey - 16, rank[sideno][i], 0, 0, 0,
						teams[tarr[sideno]].netname, basey - 8, rowline,
						basey, teams[tarr[sideno]].osp_m104 + teams[tarr[sideno]].osp_m108,
						basey + 8, OSP_countTeamPlayers (tarr[sideno]), str, time);

					y += 26;
					basey += 26;
				}

				kk = strlen (temp);
				strcpy (buf + size, temp);
				size += kk;

				if (level.intermissiontime != 0 && sync_stat > 2)
					Com_sprintf (temp, 1024,
						"xv 120 yv %i string \"Frg Dth Frt Su Eff%% Ping\"xv -16 ",
						y);
				else if (sync_stat == 4)
					Com_sprintf (temp, 1024,
						"xv 24 yv %i string \"Player          Frags Deaths Ping\"xv 8 ",
						y);
				else
					Com_sprintf (temp, 1024,
						"xv 8 yv %i string \"Player          MATCH_STATUS Time Ping\"xv 8 ",
						y);

				y += 8;
				basey += 8;
				kk = strlen (temp);
				strcpy (buf + size, temp);
				size += kk;
			}

			if (sync_stat > 2)
			{
				if (level.intermissiontime != 0)
					sprintf (rowline, "%-16s%4i%4i%4i%3i%4i%%%5i",
							 cl->pers.netname, cl->resp.score,
							 cl->resp.osp_r014, cl->resp.osp_r028,
							 cl->resp.osp_r2c0, eff, cl->ping);
				else
					sprintf (rowline, "%i %-16s%4i   %3i   %4i", i + 1,
							 cl->pers.netname, cl->resp.score,
							 cl->resp.osp_r014, cl->ping);

				if (player != ent)
					Com_sprintf (temp, 1024, "yv %i string2 \"%s\"", y, rowline);
				else
					Com_sprintf (temp, 1024, "yv %i string \"%s\"", y, rowline);
			}
			else if (cl->resp.osp_r20c)
			{
				sprintf (rowline, "%-16s*** READY ***%3i  %4i", cl->pers.netname,
						 nframes / 600, cl->ping);
				Com_sprintf (temp, 1024, "yv %i string \"%s\"", y, rowline);
			}
			else
			{
				sprintf (rowline, "%-16s [NOT READY] %3i  %4i", cl->pers.netname,
						 nframes / 600, cl->ping);
				Com_sprintf (temp, 1024, "yv %i string2 \"%s\"", y, rowline);
			}

			kk = strlen (temp);
			if (size + kk > sizeof (buf))
				break;

			strcpy (buf + size, temp);
			size += kk;
		}

		if (OSP_countTeamPlayers (tarr[sideno]) > 1 && level.intermissiontime != 0)
		{
			if (OSP_countTeamPlayers (0) + OSP_countTeamPlayers (1) < 8 &&
				sync_stat > 2)
			{
				y += 11;

				if (teams[tarr[sideno]].osp_m0f8 < 1)
					eff = 0;
				else if (teams[tarr[sideno]].osp_m0fc == 0 ||
						 teams[tarr[sideno]].osp_m0f8 + teams[tarr[sideno]].osp_m0fc == 0)
					eff = 100;
				else
					eff = teams[tarr[sideno]].osp_m0f8 * 100 /
						  (teams[tarr[sideno]].osp_m0f8 + teams[tarr[sideno]].osp_m0fc);

				sprintf (rowline, " *** TOTALS:    %4i %3i  %2i %2i %3i%%",
						 teams[tarr[sideno]].osp_m0f8, teams[tarr[sideno]].osp_m0fc,
						 teams[tarr[sideno]].osp_m104, teams[tarr[sideno]].osp_m108, eff);
				Com_sprintf (temp, 1024, "yv %i string \"%s\"", y, rowline);

				kk = strlen (temp);
				if (size + kk > sizeof (buf))
					break;

				strcpy (buf + size, temp);
				size += kk;
			}
		}

		basey = y + 40;
	}

	if (active_clients < 8)
	{
		y += 24;

		for (i = 0; i < obscount; i++)
		{
			player = g_edicts + 1 + viewers[i];

			if (!i)
			{
				Com_sprintf (temp, 1024,
					"xv 32 yv %i string2 \"Observers:\"xv 40 ", y);

				kk = strlen (temp);
				if (size + kk > sizeof (buf))
					break;

				strcpy (buf + size, temp);
				size += kk;
				y += 12;
			}

			if (player->osp_e39c)
				Com_sprintf (temp, 1024, "yv %i string2 \"[Ref]%s (p:%d)\"", y,
							 player->client->pers.netname, player->client->ping);
			else
				Com_sprintf (temp, 1024, "yv %i string2 \"%s (p:%d)\"", y,
							 player->client->pers.netname, player->client->ping);

			kk = strlen (temp);
			if (size + kk > sizeof (buf))
				break;

			strcpy (buf + size, temp);
			size += kk;
			y += 8;
		}
	}

	gi.WriteByte (svc_layout);
	gi.WriteString (buf);

	if (level.intermissiontime != 0 &&
		ent->client->resp.entered == ENTERED_ENTERED)
		strcpy (old_scores, buf);
}

// The team scoreboard, large-roster variant -- reached ONLY from
// OSP_showTeamScores' opening `if (active_clients > 6)`.  Same board, three
// differences: the team card is a text line instead of the `client 80`
// picture, the efficiency column is gone from the header and the row, and
// TOTALS loses its trailing percentage.  With the bigger roster it also shows
// SIX rows per team rather than four, and the "...and you" promotion moves to
// row 5 with a rank threshold of 6.
// gamex86.dll: 1003D40E..1003E45F
// gamei386.so: 0006A3A8..0006B3B6
void OSP_showBIGTeamScores (edict_t *ent)
{
	int			rank[2][256];
	int			pscore[2][256];
	int			viewers[256];
	char		rowline[512];
	char		str[256];
	char		temp[1024];
	char		buf[1400];
	char		time[32];
	int			tarr[2];
	int			count[2];
	int			i;
	int			nframes;
	int			m;
	int			y;
	int			sideno;
	int			kk;
	int			basey;
	int			cscore;	/* invented: insertion-sort score snapshot */
	int			obscount;
	int			size;
	gclient_t	*cl;
	edict_t		*player;

	y = 0;
	obscount = 0;
	size = 0;

	tarr[0] = 0;
	tarr[1] = 1;

	for (sideno = 0; sideno < 2; sideno++)
	{
		count[sideno] = 0;

		for (i = 0; i <= game.maxclients; i++)
		{
			player = g_edicts + 1 + i;

			if (!player->inuse || !player->client)
				continue;

			// observers are collected once, on the first team's pass
			if (!sideno && player->client->resp.entered != ENTERED_ENTERED)
			{
				viewers[obscount] = i;
				obscount++;
				continue;
			}

			if (player->client->resp.team != tarr[sideno])
				continue;

			cscore = game.clients[i].resp.score;

			for (kk = 0; kk < count[sideno]; kk++)
			{
				if (cscore > pscore[sideno][kk])
					break;
				if (cscore == pscore[sideno][kk])
				{
					if (game.clients[i].resp.osp_r014 < game.clients[rank[sideno][kk]].resp.osp_r014)
						break;
					if (game.clients[i].resp.osp_r014 == game.clients[rank[sideno][kk]].resp.osp_r014 &&
						game.clients[i].resp.osp_r2c0 < game.clients[rank[sideno][kk]].resp.osp_r2c0)
						break;
				}
			}

			for (m = count[sideno]; m > kk; m--)
			{
				rank[sideno][m] = rank[sideno][m - 1];
				pscore[sideno][m] = pscore[sideno][m - 1];
			}

			rank[sideno][kk] = i;
			pscore[sideno][kk] = cscore;
			count[sideno]++;
		}

		for (i = 0; i < count[sideno]; i++)
		{
			player = g_edicts + 1 + rank[sideno][i];
			player->client->resp.osp_r208 = i + 1;
		}
	}

	buf[0] = 0;

	if ((int)gi.cvar ("nglog_worldstats", "0", 0)->value)
		ent->client->ps.stats[28] = 0x62b;

	if (level.intermissiontime != 0)
		ent->client->ps.stats[27] = 0x62a;
	else
		ent->client->ps.stats[27] = 0x629;

	size = strlen (buf);
	basey = 0;

	for (sideno = 0; sideno < 2; sideno++)
	{
		if (count[sideno] > 6)
			count[sideno] = 6;

		for (i = 0; i < count[sideno]; i++)
		{
			cl = game.clients + rank[sideno][i];
			player = g_edicts + 1 + rank[sideno][i];
			y = basey + i * 8;

			if (i == 5 && ent->client->resp.team == sideno &&
				ent->client->resp.osp_r208 > 6)
			{
				player = ent;
				cl = ent->client;
				y += 2;
				i = cl->resp.osp_r208 - 1;
			}

			if (cl->resp.enterframe < sync_frame)
				nframes = level.framenum - sync_frame + 1;
			else
				nframes = level.framenum - cl->resp.enterframe + 1;

			if (nframes < 1)
				nframes = 1;

			// The team card is emitted once, above the first rowline.
			if (!i)
			{
				sprintf (rowline, "%i", teams[tarr[sideno]].osp_m0f8);
				for (m = 0; m < strlen (rowline); m++)
					rowline[m] += 128;

				// The target passes the three y positions as basey-16, basey-8,
				// and basey, respectively.
				if (level.intermissiontime == 0 || sideno)
				{
					Com_sprintf (temp, 1024,
						"xv 78 yv %i string \"%s\"yv %i string2 \"Score: %s\""
						"yv %i string2 \"Skin: %s\"",
						basey - 16, teams[tarr[sideno]].netname, basey - 8, rowline,
						basey, teams[tarr[sideno]].skin);

					y += 18;
					basey += 18;
				}
				else
				{
					OSP_getDateInfo (time);

					if (manual_map == 1)
						sprintf (str, "[ Voted map change ]");
					else if (manual_map == 2)
						sprintf (str, "[ Voted server config change ]");
					else if (teams[0].osp_m124 == 1)
						sprintf (str, "[ %s defeats %s: %d to %d ]",
								 teams[0].greenname, teams[1].greenname,
								 teams[0].osp_m0f8, teams[1].osp_m0f8);
					else if (teams[1].osp_m124 == 1)
						sprintf (str, "[ %s defeats %s: %d to %d ]",
								 teams[1].greenname, teams[0].greenname,
								 teams[1].osp_m0f8, teams[0].osp_m0f8);
					else
						sprintf (str, "[ Tied match! (%d to %d) ]",
								 teams[1].osp_m0f8, teams[0].osp_m0f8);

					Com_sprintf (temp, 1024,
						"xv 78 yv %i string \"%s\"yv %i string2 \"Score: %s\""
						"yv %i string2 \"Skin: %s\""
						"xv 0 yv -43 cstring2 \"%s\"yv -33 cstring2 \"%s\"",
						basey - 16, teams[tarr[sideno]].netname, basey - 8, rowline,
						basey, teams[tarr[sideno]].skin, str, time);

					y += 18;
					basey += 18;
				}

				kk = strlen (temp);
				strcpy (buf + size, temp);
				size += kk;

				if (level.intermissiontime != 0 && sync_stat > 2)
					Com_sprintf (temp, 1024,
						"xv 140 yv %i string \"Frg Dth Frt Su Ping\"xv 4 ",
						y);
				else if (sync_stat == 4)
					Com_sprintf (temp, 1024,
						"xv 24 yv %i string \"Player          Frags Deaths Ping\"xv 8 ",
						y);
				else
					Com_sprintf (temp, 1024,
						"xv 8 yv %i string \"Player          MATCH_STATUS Time Ping\"xv 8 ",
						y);

				y += 8;
				basey += 8;

				kk = strlen (temp);
				strcpy (buf + size, temp);
				size += kk;
			}

			if (sync_stat > 2)
			{
				if (level.intermissiontime != 0)
					sprintf (rowline, "%-16s%4i%4i%4i%3i%5i",
							 cl->pers.netname, cl->resp.score,
							 cl->resp.osp_r014, cl->resp.osp_r028,
							 cl->resp.osp_r2c0, cl->ping);
				else
					sprintf (rowline, "%i %-16s%4i   %3i   %4i", i + 1,
							 cl->pers.netname, cl->resp.score,
							 cl->resp.osp_r014, cl->ping);

				if (player != ent)
					Com_sprintf (temp, 1024, "yv %i string2 \"%s\"", y, rowline);
				else
					Com_sprintf (temp, 1024, "yv %i string \"%s\"", y, rowline);
			}
			else if (cl->resp.osp_r20c)
			{
				sprintf (rowline, "%-16s*** READY ***%3i  %4i", cl->pers.netname,
						 nframes / 600, cl->ping);
				Com_sprintf (temp, 1024, "yv %i string \"%s\"", y, rowline);
			}
			else
			{
				sprintf (rowline, "%-16s [NOT READY] %3i  %4i", cl->pers.netname,
						 nframes / 600, cl->ping);
				Com_sprintf (temp, 1024, "yv %i string2 \"%s\"", y, rowline);
			}

			kk = strlen (temp);
			if (size + kk > sizeof (buf))
				break;

			strcpy (buf + size, temp);
			size += kk;
		}

		if (OSP_countTeamPlayers (tarr[sideno]) > 1 && level.intermissiontime != 0)
		{
			if (OSP_countTeamPlayers (0) + OSP_countTeamPlayers (1) < 12 &&
				sync_stat > 2)
			{
				y += 11;

				sprintf (rowline, " *** TOTALS:    %4i %3i  %2i %2i",
						 teams[tarr[sideno]].osp_m0f8, teams[tarr[sideno]].osp_m0fc,
						 teams[tarr[sideno]].osp_m104, teams[tarr[sideno]].osp_m108);
				Com_sprintf (temp, 1024, "yv %i string \"%s\"", y, rowline);

				kk = strlen (temp);
				if (size + kk > sizeof (buf))
					break;

				strcpy (buf + size, temp);
				size += kk;
			}
		}

		basey = y + 40;
	}

	if (active_clients < 12)
	{
	y += 24;

	for (i = 0; i < obscount; i++)
	{
		player = g_edicts + 1 + viewers[i];

		if (!i)
		{
			Com_sprintf (temp, 1024,
				"xv 32 yv %i string2 \"Observers:\"xv 40 ", y);

			kk = strlen (temp);
			if (size + kk > sizeof (buf))
				break;

			strcpy (buf + size, temp);
			size += kk;
			y += 12;
		}

		if (player->osp_e39c)
			Com_sprintf (temp, 1024, "yv %i string2 \"[Ref]%s (p:%d)\"", y,
						 player->client->pers.netname, player->client->ping);
		else
			Com_sprintf (temp, 1024, "yv %i string2 \"%s (p:%d)\"", y,
						 player->client->pers.netname, player->client->ping);

		kk = strlen (temp);
		if (size + kk > sizeof (buf))
			break;

		strcpy (buf + size, temp);
		size += kk;
		y += 8;
	}
	}

	gi.WriteByte (svc_layout);
	gi.WriteString (buf);

	if (level.intermissiontime != 0 &&
		ent->client->resp.entered == ENTERED_ENTERED)
		strcpy (old_scores, buf);
}

// The 1v1 scoreboard.  Unlike the deathmatch board this one is two player
// CARDS, not a table: each of the two team slots gets its own "client 80"
// banner with the player's icon, frag total and suicide count, then its own
// column header and its own single row, 48 units apart.  Observers are listed
// underneath.  Only the row strcpy is length-guarded; the banner and header
// go on unconditionally.
// gamex86.dll: 1003E45F..1003EF69
// gamei386.so: 0006B3B8..0006BDC1
void OSP_show1v1Scores (edict_t *ent)
{
	int			viewers[256];
	char		rowline[512];
	char		str[256];
	char		temp[1024];
	char		buf[1400];
	char		time[32];
	int			cids[2];
	int			tarr[2];
	int			i;
	int			eff;
	int			nframes;
	int			y;
	int			sideno;
	int			kk;
	int			basey;
	int			obscount;
	int			size;
	gclient_t	*cl;
	edict_t		*player;

	y = 0;
	obscount = 0;
	size = 0;
	tarr[0] = 0;
	tarr[1] = 1;
	cids[0] = -1;
	cids[1] = -1;

	for (i = 0; i < game.maxclients; i++)
	{
		player = g_edicts + i + 1;

		if (!player->inuse || !player->client)
			continue;

		if (player->client->resp.entered != ENTERED_ENTERED)
		{
			viewers[obscount] = i;
			obscount++;
			continue;
		}

		if (!player->client->resp.team)
			cids[0] = i;
		else if (player->client->resp.team == 1)
			cids[1] = i;
	}

	buf[0] = 0;

	if ((int)gi.cvar ("nglog_worldstats", "0", 0)->value)
		ent->client->ps.stats[28] = 0x62b;

	if (level.intermissiontime != 0)
		ent->client->ps.stats[27] = 0x62a;
	else
		ent->client->ps.stats[27] = 0x629;

	size = strlen (buf);
	basey = 0;

	for (sideno = 0; sideno < 2; sideno++)
	{
		if (cids[sideno] == -1)
			break;

		cl = game.clients + cids[sideno];
		// Computed and never read again within the loop -- dead, but
		// faithfully reproduced.
		player = g_edicts + 1 + cids[sideno];
		y = basey;

		if (cl->resp.enterframe < sync_frame)
			nframes = level.framenum - sync_frame + 1;
		else
			nframes = level.framenum - cl->resp.enterframe + 1;

		if (nframes < 1)
			nframes = 1;

		if (cl->resp.score < 1)
			eff = 0;
		else if (!cl->resp.osp_r014 ||
				 !(cl->resp.osp_r014 + cl->resp.score))
			eff = 100;
		else
			eff = cl->resp.score * 100 /
				  (cl->resp.score + cl->resp.osp_r014);

		// the team's frag total, in green
		sprintf (rowline, "%i", teams[tarr[sideno]].osp_m0f8);
		for (kk = 0; kk < strlen (rowline); kk++)
			rowline[kk] += 128;

		// Only the first card at intermission carries the result line and
		// the time; everything else uses the short banner.
		if (level.intermissiontime == 0 || sideno)
			Com_sprintf (temp, 1024,
				"client 80 %i %i %i %i %i xv 112 picn tag1 xv 114 string \"%s\""
				"yv %i string2 \"Frags: %s\"yv %i string2 \"Suicides: %i\"",
				basey - 16, cids[sideno], 0, 0, 0, teams[tarr[sideno]].netname,
				basey - 4, rowline, basey + 4, teams[tarr[sideno]].osp_m108);
		else
		{
			OSP_getDateInfo (time);

			if (manual_map == 1)
				sprintf (str, "[ Voted map change ]");
			else if (manual_map == 2)
				sprintf (str, "[ Voted server config change ]");
			else if (teams[0].osp_m124 == 1)
				sprintf (str, "[ %s defeats %s: %d to %d ]",
						 teams[0].greenname, teams[1].greenname,
						 teams[0].osp_m0f8, teams[1].osp_m0f8);
			else if (teams[1].osp_m124 == 1)
				sprintf (str, "[ %s defeats %s: %d to %d ]",
						 teams[1].greenname, teams[0].greenname,
						 teams[1].osp_m0f8, teams[0].osp_m0f8);
			else
				sprintf (str, "[ Tied match! (%d to %d) ]",
						 teams[1].osp_m0f8, teams[0].osp_m0f8);

			Com_sprintf (temp, 1024,
				"client 80 %i %i %i %i %i xv 112 picn tag1 xv 114 string \"%s\""
				"yv %i string2 \"Frags: %s\"yv %i string2 \"Suicides: %i\""
				"xv 0 yv -43 cstring2 \"%s\"yv -25 cstring2 \"%s\"",
				basey - 16, cids[sideno], 0, 0, 0, teams[tarr[sideno]].netname,
				basey - 4, rowline, basey + 4, teams[tarr[sideno]].osp_m108, str, time);
		}

		// Real advances BOTH accumulators at each step, even though `basey` is
		// dead from here until it is reassigned `y + 48` below.
		y += 26;
		basey += 26;
		kk = strlen (temp);
		strcpy (buf + size, temp);
		size += kk;

		if (level.intermissiontime != 0 && sync_stat > 2)
			Com_sprintf (temp, 1024,
				"xv -8 yv %i string \"Player          Frags Deaths Eff%% FPH Ping\"xv -8 ",
				y);
		else if (sync_stat == 4)
			Com_sprintf (temp, 1024,
				"xv 0 yv %i string \"Player          Frags Deaths Ping\"xv 0 ",
				y);
		else
			Com_sprintf (temp, 1024,
				"xv 8 yv %i string \"Player          Frags Deaths Time Ping\"xv 8 ",
				y);

		y += 8;
		basey += 8;
		kk = strlen (temp);
		strcpy (buf + size, temp);
		size += kk;

		if (sync_stat > 2)
		{
			if (level.intermissiontime != 0)
				sprintf (rowline, "%-16s%4i%6i%6i%%%4i%5i", cl->pers.netname,
						 cl->resp.score, cl->resp.osp_r014, eff,
						 cl->resp.score * 36000 / nframes, cl->ping);
			else
				sprintf (rowline, "%-16s%4i   %3i   %4i", cl->pers.netname,
						 cl->resp.score, cl->resp.osp_r014, cl->ping);

			Com_sprintf (temp, 1024, "yv %i string2 \"%s\"", y, rowline);
		}
		else if (cl->resp.osp_r20c)
		{
			sprintf (rowline, "%-16s*** READY ***%3i  %4i", cl->pers.netname,
					 nframes / 600, cl->ping);
			Com_sprintf (temp, 1024, "yv %i string \"%s\"", y, rowline);
		}
		else
		{
			sprintf (rowline, "%-16s [NOT READY] %3i  %4i", cl->pers.netname,
					 nframes / 600, cl->ping);
			Com_sprintf (temp, 1024, "yv %i string2 \"%s\"", y, rowline);
		}

		kk = strlen (temp);
		if (size + kk > sizeof (buf))
			break;

		strcpy (buf + size, temp);
		size += kk;
		basey = y + 48;
	}

	y += 24;

	for (i = 0; i < obscount; i++)
	{
		player = g_edicts + 1 + viewers[i];

		if (!i)
		{
			Com_sprintf (temp, 1024,
				"xv 32 yv %i string2 \"Observers:\"xv 40 ", y);

			kk = strlen (temp);
			if (size + kk > sizeof (buf))
				break;

			strcpy (buf + size, temp);
			size += kk;
			y += 12;
		}

		Com_sprintf (temp, 1024, "yv %i string2 \"%s (p:%d)\"", y,
					 player->client->pers.netname, player->client->ping);

		kk = strlen (temp);
		if (size + kk > sizeof (buf))
			break;

		strcpy (buf + size, temp);
		size += kk;
		y += 8;
	}

	gi.WriteByte (svc_layout);
	gi.WriteString (buf);

	if (level.intermissiontime != 0 &&
		ent->client->resp.entered == ENTERED_ENTERED)
		strcpy (old_scores, buf);
}

// id CTF's 23-entry table, a NAMED global here rather than CTF's file-static.
// Two changes from CTF's: the two `item_flag_team*` rows are gone (this mod
// has no flags to stand near) and every priority is therefore one lower.
loc_t	loc_names[23] =
{
	{	"item_quad",				1	},
	{	"item_invulnerability",		1	},
	{	"weapon_bfg",				2	},
	{	"weapon_railgun",			3	},
	{	"weapon_rocketlauncher",	3	},
	{	"weapon_hyperblaster",		3	},
	{	"weapon_chaingun",			3	},
	{	"weapon_grenadelauncher",	3	},
	{	"weapon_machinegun",		3	},
	{	"weapon_supershotgun",		3	},
	{	"weapon_shotgun",			3	},
	{	"item_power_screen",		4	},
	{	"item_power_shield",		4	},
	{	"item_armor_body",			5	},
	{	"item_armor_combat",		5	},
	{	"item_armor_jacket",		5	},
	{	"item_silencer",			6	},
	{	"item_breather",			6	},
	{	"item_enviro",				6	},
	{	"item_adrenaline",			6	},
	{	"item_bandolier",			7	},
	{	"item_pack",				7	},
	{	NULL,						0	}
};

// %l -- name the nearest landmark.  CTF's CTFSay_Team_Location with the
// capture-the-flag half removed, so there is no "the red " / "the blue ".
// gamex86.dll: 1003F349..1003F6B9
// gamei386.so: 0006BDC1..0006C0C7
static void sayteam_location (edict_t *who, char *buf)
{
	edict_t		*what = NULL;
	edict_t		*hot = NULL;
	float		hotdist = 999999, newdist;
	vec3_t		v;
	int			hotindex = 999;
	int			lastprio = -1;	// invented, dead -- never read again
	int			i;
	gitem_t		*item;
	qboolean	hotsee = false;
	qboolean	cansee;

	while ((what = loc_findradius (what, who->s.origin, 1024)) != NULL)
	{
		for (i = 0; loc_names[i].classname; i++)
			if (strcmp (what->classname, loc_names[i].classname) == 0)
				break;
		if (!loc_names[i].classname)
			continue;

		// something we can see gets priority over something we can't
		cansee = loc_CanSee (what, who);
		if (cansee && !hotsee)
		{
			hotsee = true;
			hotindex = loc_names[i].priority;
			hot = what;
			VectorSubtract (what->s.origin, who->s.origin, v);
			hotdist = VectorLength (v);
			continue;
		}

		if (hotsee && !cansee)
			continue;
		if (hotsee && hotindex < loc_names[i].priority)
			continue;

		VectorSubtract (what->s.origin, who->s.origin, v);
		newdist = VectorLength (v);

		if (newdist < hotdist ||
			(cansee && loc_names[i].priority < hotindex))
		{
			hot = what;
			hotdist = newdist;
			hotindex = i;
			hotsee = loc_CanSee (hot, who);
		}
	}

	if (!hot)
	{
		strcpy (buf, "nowhere");
		return;
	}

	what = NULL;
	while ((what = G_Find (what, FOFS (classname), hot->classname)) != NULL)
	{
		if (what == hot)
			continue;
		break;
	}

	if ((item = FindItemByClassname (hot->classname)) == NULL)
	{
		strcpy (buf, "nowhere");
		return;
	}

	if (who->waterlevel)
		strcpy (buf, "in the water ");
	else
		*buf = 0;

	VectorSubtract (who->s.origin, hot->s.origin, v);
	if (fabs (v[2]) > fabs (v[0]) && fabs (v[2]) > fabs (v[1]))
		if (v[2] > 0)
			strcat (buf, "above ");
		else
			strcat (buf, "below ");
	else
		strcat (buf, "near ");

	strcat (buf, "the ");
	strcat (buf, item->pickup_name);
}


// %a -- CTF's CTFSay_Team_Armor, unchanged; its whole string set is present.
// gamex86.dll: 1003F6B9..1003F7E4
// gamei386.so: 0006C0C7..0006C209
static void sayteam_armor (edict_t *who, char *buf)
{
	gitem_t		*item;
	int			index, cells;
	int			power_armor_type;

	*buf = 0;

	power_armor_type = PowerArmorType (who);
	if (power_armor_type)
	{
		cells = who->client->pers.inventory[ITEM_INDEX (FindItem ("cells"))];
		if (cells)
			sprintf (buf + strlen (buf), "%s with %i cells ",
					 (power_armor_type == POWER_ARMOR_SCREEN) ?
					 "Power Screen" : "Power Shield", cells);
	}

	index = ArmorIndex (who);
	if (index)
	{
		item = GetItemByIndex (index);
		if (item)
		{
			if (*buf)
				strcat (buf, "and ");
			sprintf (buf + strlen (buf), "%i units of %s",
					 who->client->pers.inventory[index], item->pickup_name);
		}
	}

	if (!*buf)
		strcpy (buf, "no armor");
}



// <INVENTED NAMES> for three file-statics the ELF cannot see: gcc -O3
// inlines all three back into OSP_sayteam_cmd.  The VC6 image has them as
// three separate functions.

// %h -- "dead" below zero, otherwise the health count.
// gamex86.dll: 1003F7E4..1003F823
// gamei386.so: absent
static void sayteam_health (edict_t *who, char *buf)
{
	if (who->health <= 0)
		strcpy (buf, "dead");
	else
		sprintf (buf, "%i health", who->health);
}

// %w -- the weapon in hand, or "none".
// gamex86.dll: 1003F823..1003F866
// gamei386.so: absent
static void sayteam_weapon (edict_t *who, char *buf)
{
	if (who->client->pers.weapon)
		strcpy (buf, who->client->pers.weapon->pickup_name);
	else
		strcpy (buf, "none");
}

// %r and %t -- whichever rune is held. Both escapes share this one body; the
// two case labels in the caller are separate, the code behind them is not.
// gamex86.dll: 1003F866..1003F933
// gamei386.so: absent
static void sayteam_runes (edict_t *who, char *buf)
{
	if (who->client->ps.stats[STAT_RUNE_RESIST])
		strcpy (buf, "the RESIST rune");
	else if (who->client->ps.stats[STAT_RUNE_STRENGTH])
		strcpy (buf, "the STRENGTH rune");
	else if (who->client->ps.stats[STAT_RUNE_HASTE])
		strcpy (buf, "the HASTE rune");
	else if (who->client->ps.stats[STAT_RUNE_REGEN])
		strcpy (buf, "the REGEN rune");
	else if (who->client->ps.stats[STAT_RUNE_VAMPIRE])
		strcpy (buf, "the VAMPIRE rune");
	else
		strcpy (buf, "no runes");
}

// %n -- the mod's own: name every teammate the caller can actually see, as
// "a, b and c".  The pending-name buffer is what makes the last separator
// " and " rather than ", ".
// gamex86.dll: 1003F933..1003FB12
// gamei386.so: 0006C209..0006C450
static void sayteam_sight (edict_t *who, char *buf)
{
	char	list[1024];
	char	names[1024];
	edict_t	*e;
	int		i;
	int		counts;

	// counts, then names[0], then list[0] -- and the last two are ONE chained
	// assignment.
	counts = 0;
	list[0] = names[0] = 0;

	for (i = 1; i <= maxclients->value; i++)
	{
		e = g_edicts + i;

		if (!e->inuse ||
			e->client->resp.entered != ENTERED_ENTERED ||
			e == who ||
			!loc_CanSee (e, who))
			continue;

		if (names[0])
		{
			if (strlen (list) + strlen (names) + 3 < 1024)
			{
				if (counts)
					strcat (list, ", ");
				strcat (list, names);
				names[0] = 0;
			}
			counts++;
		}

		strcpy (names, e->client->pers.netname);
	}

	if (names[0])
	{
		if (strlen (list) + strlen (names) + 6 < 1024)
		{
			if (counts)
				strcat (list, " and ");
			strcat (list, names);
		}

		strcpy (buf, list);
	}
	else
		strcpy (buf, "no one");
}


// "say_team".  id CTF's CTFSay_Team: expand the % escapes into outmsg, then
// send the result to everyone on the caller's team.
// gamex86.dll: 1003EF69..1003F349
// gamei386.so: 0006C450..0006C979
void OSP_sayteam_cmd (edict_t *ent, char *msg)
{
	char	outmsg[1024];
	char	scratch[1024];
	char	tmp[2048];
	char	*p;
	int		t;
	edict_t	*cp;

	outmsg[0] = 0;

	if (*msg == '"')
	{
		msg[strlen (msg) - 1] = 0;
		msg++;
	}

	for (p = outmsg; *msg && (p - outmsg) < sizeof (outmsg) - 1; msg++)
	{
		if (*msg == '%')
		{
			switch (*++msg)
			{
			case 'l':
			case 'L':
				sayteam_location (ent, scratch);
				strcpy (p, scratch);
				p += strlen (scratch);
				break;

			case 'a':
			case 'A':
				sayteam_armor (ent, scratch);
				strcpy (p, scratch);
				p += strlen (scratch);
				break;

			case 'h':
			case 'H':
				sayteam_health (ent, scratch);
				strcpy (p, scratch);
				p += strlen (scratch);
				break;

			case 'w':
			case 'W':
				sayteam_weapon (ent, scratch);
				strcpy (p, scratch);
				p += strlen (scratch);
				break;

			case 'n':
			case 'N':
				sayteam_sight (ent, scratch);
				strcpy (p, scratch);
				p += strlen (scratch);
				break;

			case 'r':
			case 'R':
				sayteam_runes (ent, scratch);
				strcpy (p, scratch);
				p += strlen (scratch);
				break;

			case 't':
			case 'T':
				sayteam_runes (ent, scratch);
				strcpy (p, scratch);
				p += strlen (scratch);
				break;

			default:
				*p++ = *msg;
			}
		}
		else
			*p++ = *msg;
	}
	*p = 0;

	sprintf (tmp, "(%s): %s\n", ent->client->pers.netname, outmsg);

	for (t = 1; t <= maxclients->value; t++)
	{
		cp = g_edicts + t;

		if (!cp->inuse)
			continue;
		if (cp->client->resp.team == ent->client->resp.team)
			gi.cprintf (cp, PRINT_CHAT, "%s", tmp);
	}
}


// CTF's loc_findradius, unchanged, and it really is placed AFTER
// OSP_sayteam_cmd in the real link order.
// gamex86.dll: 1003FB12..1003FBF0
// gamei386.so: 0006C979..0006CA24
static edict_t *loc_findradius (edict_t *from, vec3_t org, float rad)
{
	vec3_t	eorg;
	int		j;

	if (!from)
		from = g_edicts;
	else
		from++;

	for ( ; from < &g_edicts[globals.num_edicts]; from++)
	{
		if (!from->inuse)
			continue;

		for (j = 0; j < 3; j++)
			eorg[j] = org[j] - (from->s.origin[j] +
								(from->mins[j] + from->maxs[j]) * 0.5);

		if (VectorLength (eorg) > rad)
			continue;

		return from;
	}

	return NULL;
}
