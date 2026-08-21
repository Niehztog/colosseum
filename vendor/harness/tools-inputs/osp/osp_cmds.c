// osp_cmds.c -- <INVENTED FILENAME>. The mod's client commands, the vote
// system and the referee commands.
//
// Three groups that share the vote state: the plain `OSP_*_cmd` client
// commands, the `OSP_*_vote` handlers the vote system dispatches through
// `vote_item`, and the `OSP_r*_cmd` referee commands, which are the same
// operations with an admin log line attached.

#include "g_local.h"
#include "bl_main.h"
#include "bl_botcfg.h"

void ClientDisconnect (edict_t *ent);

// gamex86.dll: 1001D6F0..1001D70E
// gamei386.so: 0005574C..00055779
void OSP_id_cmd (edict_t *ent)
{
	ent->client->ps.stats[STAT_CHASE] = OSP_changeID (ent);
}

// gamex86.dll: 1001D70E..1001D725
// gamei386.so: 0005577C..000557A5
void OSP_motd_cmd (edict_t *ent)
{
	ent->client->resp.osp_r0ac = level.framenum;
}

// "talkto <player> <message>" -- a private message, with vanilla Cmd_Say_f's
// flood protection copied in verbatim apart from the +1 on the lockout
// countdown.  Players and spectators cannot talk to each other.
// gamex86.dll: 1001D725..1001DB45
// gamei386.so: 000557A8..00055BD2
void OSP_talkto_cmd (edict_t *ent)
{
	char		text[2048];
	char		text2[2048];
	char		msg[2048];
	edict_t		*player;
	char		*p;
	int			i;

	if (gi.argc () < 2)
		return;

	player = OSP_findPlayer (gi.argv (1));

	if (!player)
	{
		gi.cprintf (ent, PRINT_HIGH, "\"%s\" is not logged on.\n",
					gi.argv (1));
		return;
	}

	if ((ent->client->resp.entered != ENTERED_ENTERED &&
		 player->client->resp.entered == ENTERED_ENTERED) ||
		(ent->client->resp.entered == ENTERED_ENTERED &&
		 player->client->resp.entered != ENTERED_ENTERED))
	{
		gi.cprintf (ent, PRINT_HIGH,
			"\"talkto\" not available between active players and spectators.\n");
		return;
	}

	if (player == ent)
	{
		gi.cprintf (ent, PRINT_HIGH,
					"Talking to ourselves again, are we?\n");
		return;
	}

	Com_sprintf (text, 2048, "[%s]:", ent->client->pers.netname);
	Com_sprintf (text2, 2048, "[%s->%s]:", ent->client->pers.netname,
				 player->client->pers.netname);

	msg[0] = 0;
	for (i = 2; i < gi.argc (); i++)
	{
		strcat (msg, " ");
		strcat (msg, gi.argv (i));
	}

	p = msg;
	if (*p == '"')
	{
		p++;
		p[strlen (p) - 1] = 0;
	}

	strcat (text, p);
	strcat (text2, p);
	text[150] = 0;
	text2[170] = 0;

	if (flood_msgs->value != 0 && !match_paused)
	{
		gclient_t	*cl;
		int			i;

		cl = ent->client;

		if (level.time < cl->flood_locktill)
		{
			gi.cprintf (ent, PRINT_HIGH,
						"You can't talk for %d more seconds\n",
						(int)(cl->flood_locktill - level.time) + 1);
			return;
		}

		i = cl->flood_whenhead - flood_msgs->value + 1;
		if (i < 0)
			i += 10;

		if (cl->flood_when[i] &&
			level.time - cl->flood_when[i] < flood_persecond->value)
		{
			cl->flood_locktill = level.time + flood_waitdelay->value;
			gi.cprintf (ent, PRINT_CHAT,
				"Flood protection:  You can't talk for %d seconds.\n",
				(int)flood_waitdelay->value);
			return;
		}

		cl->flood_whenhead = (unsigned int)(cl->flood_whenhead + 1) % 10;
		cl->flood_when[cl->flood_whenhead] = level.time;
	}

	q2log_playerChat (text);
	strcat (text, "\n");
	strcat (text2, "\n");

	if (dedicated->value)
		gi.dprintf ("%s", text);

	gi.cprintf (player, PRINT_CHAT, "%s", text);
	gi.cprintf (ent, PRINT_CHAT, "%s", text2);
}

// gamex86.dll: 1001DB45..1001DE3A
// gamei386.so: 00055BD4..00055E89
void OSP_ready_cmd (edict_t *ent, int quiet)
{
	int		t;
	int		x;
	edict_t	*cli;

	if (ent->client->resp.osp_r010 > level.framenum)
		return;
	ent->client->resp.osp_r010 = level.framenum + 2;

	if (sync_stat >= 4)
	{
		gi.cprintf (ent, PRINT_HIGH, "Match has already started.\n");
		return;
	}

	if (ent->client->resp.entered != ENTERED_ENTERED)
	{
		gi.cprintf (ent, PRINT_HIGH,
					"You must enter the game to be ready!\n");
		return;
	}

	if (!ent->client->resp.osp_r20c)
	{
		ent->client->resp.osp_r20c = 1;

		if (OSP_CheckReady () == 2)
			return;

		// Everybody left is a bot: start without waiting them out.
		if (sync_stat < 4 && botglobals.numbots >= OSP_countReady () &&
			!(int)bots_warmuptime->value)
		{
			OSP_allready_svcmd ();
			return;
		}

		if (!quiet)
			gi.bprintf (PRINT_HIGH, "%s is ready!\n",
						ent->client->pers.greenname);

		if (m_mode < 2)
		{
			if (!(ent->flags & FL_OSP_NOCMD))
				OSP_clientConfigString (ent, 0x623, "* WARMUP");
		}
		else if (!(ent->flags & FL_OSP_NOCMD) && quiet < 2)
		{
			for (t = 1; t <= game.maxclients; t++)
			{
				cli = g_edicts + t;

				if (!cli->inuse || !cli->client ||
					(cli->flags & FL_OSP_NOCMD))
					continue;

				for (x = 0; x < 2; x++)
				{
					if (OSP_teamCount (x) &&
						OSP_teamReady (x) == OSP_teamCount (x))
						OSP_clientConfigString (cli, 0x626 + x * 2,
												"     * WARMUP");
					else if (cli->client->resp.osp_r20c &&
							 x == cli->client->resp.team)
						OSP_clientConfigString (cli, 0x626 + x * 2,
												"     + WARMUP");
					else if (OSP_teamReady (x))
						OSP_clientConfigString (cli, 0x626 + x * 2,
												"     - WARMUP");
					else
						OSP_clientConfigString (cli, 0x626 + x * 2,
												"       WARMUP");
				}
			}
		}
	}
	else if (!(ent->flags & FL_OSP_NOCMD))
		gi.cprintf (ent, PRINT_HIGH, "You are already ready!\n");

	gi.cvar_set ("time_remaining", "WARMUP");
}

// "notready".  Drops the caller's ready flag and, if that takes the unready
// count back above the threshold, rewinds the whole match to warmup: every
// client's warmup configstrings are rebuilt, the countdown cells are cleared
// and any demo that had already started is stopped.
// gamex86.dll: 1001DE3A..1001E1E4
// gamei386.so: 00055E8C..000561FB
void OSP_notready_cmd (edict_t *ent, int quiet)
{
	int		notready = 0;
	int		i;
	int		j;
	edict_t	*cli;

	if (ent->client->resp.osp_r010 > level.framenum)
		return;
	ent->client->resp.osp_r010 = level.framenum + 2;

	if (ent->client->resp.entered != ENTERED_ENTERED)
		return;
	if (sync_stat >= 4)
		return;

	ent->client->resp.osp_r20c = 0;

	for (i = 1; i <= game.maxclients; i++)
	{
		cli = g_edicts + i;

		if (!cli->inuse || !cli->client ||
			cli->client->resp.entered != ENTERED_ENTERED)
			continue;

		if (!cli->client->resp.osp_r20c)
			notready++;
	}

	if (notready <= active_clients *
					(100 - (int)match_readypercent->value) / 100)
		return;

	if (ent->client->resp.enterframe < level.framenum)
	{
		if (!quiet)
			gi.bprintf (PRINT_HIGH, "%s is NOT ready!\n",
						ent->client->pers.greenname);

		sync_frame = level.framenum - 1;
	}

	if (sync_stat)
		gi.bprintf (PRINT_HIGH, "Match moved back to NOT READY status!\n");

	sync_stat = 0;
	gi.cvar_set ("time_remaining", "WARMUP");

	for (i = 1; i <= game.maxclients; i++)
	{
		cli = g_edicts + i;

		if (!cli->inuse || !cli->client || (cli == ent && !quiet) ||
			(cli->flags & FL_OSP_BOT))
			continue;

		if (m_mode < 2)
		{
			if (cli->client->resp.osp_r20c)
				OSP_clientConfigString (cli, 0x623, "* WARMUP");
			else
				OSP_clientConfigString (cli, 0x623, "  WARMUP");

			cli->client->ps.stats[20] = 0;
		}
		else if (quiet < 2)
		{
			for (j = 0; j < 2; j++)
			{
				if (OSP_teamCount (j) &&
					OSP_teamReady (j) == OSP_teamCount (j))
					OSP_clientConfigString (cli, 0x626 + j * 2, "     * WARMUP");
				else if (cli->client->resp.osp_r20c &&
						 j == cli->client->resp.team)
					OSP_clientConfigString (cli, 0x626 + j * 2, "     + WARMUP");
				else if (OSP_teamReady (j) > 0)
					OSP_clientConfigString (cli, 0x626 + j * 2, "     - WARMUP");
				else
					OSP_clientConfigString (cli, 0x626 + j * 2, "       WARMUP");
			}
		}

		cli->client->ps.stats[17] = 0;

		if (cli->client->resp.osp_r234)
		{
			char	stop[32];

			gi.WriteByte (svc_stufftext);
			strcpy (stop, "stop\n");
			gi.WriteString (stop);
			gi.unicast (cli, true);
		}
	}
}

// gamex86.dll: 1001E1E4..1001E2BD
// gamei386.so: 000561FC..000562AD
void OSP_highscores_cmd (edict_t *ent)
{
	ent->client->showinventory = false;
	ent->client->showhelp = false;

	// resp.osp_r24c is the scoreboard MODE, not a flag: 0 here, 1 from
	// OSP_oldscores_cmd, 2 from OSP_changeObserve/OSP_changeChase and 4 from
	// OSP_showinfo_cmd.
	if ((!ent->client->resp.osp_r24c || ent->client->resp.osp_r24c == 1) &&
		ent->client->showscores)
	{
		ent->client->showscores = false;
		ent->client->update_chase = 1;
		return;
	}

	ent->client->resp.osp_r24c = 0;
	ent->client->showscores = true;
	ent->client->resp.osp_r244 = 0;
	ent->client->resp.osp_r034 = 0;
	DeathmatchScoreboardMessage (ent, ent->enemy);
	gi.unicast (ent, false);
}

// gamex86.dll: 1001E2BD..1001E35A
// gamei386.so: 000562B0..0005633B
void OSP_showinfo_cmd (edict_t *ent)
{
	if (ent->client->resp.osp_r010 > level.framenum ||
		(ent->flags & FL_OSP_NOCMD))
		return;
	ent->client->resp.osp_r010 = level.framenum + 2;

	if (m_mode)
	{
		ent->client->resp.osp_r24c = 4;
		ent->client->showscores = true;
		ent->client->resp.osp_r0ac = level.framenum;
		DeathmatchScoreboardMessage (ent, ent);
		gi.unicast (ent, true);
	}
}

// gamex86.dll: 1001E35A..1001E43A
// gamei386.so: 0005633C..0005640B
void OSP_accuracy_cmd (edict_t *ent)
{
	edict_t		*other;

	if (ent->client->resp.osp_r010 > level.framenum ||
		(ent->flags & FL_OSP_NOCMD))
		return;
	ent->client->resp.osp_r010 = level.framenum + 2;

	if (gi.argc () == 1)
		OSP_accuracyInfo (ent, ent->client->pers.netname,
						  ent->client->resp.clientid);
	else
	{
		other = OSP_findPlayer (gi.argv (1));
		if (!other)
			gi.cprintf (ent, PRINT_HIGH, "\"%s\" is not logged on.\n",
						gi.argv (1));
		else
			OSP_accuracyInfo (ent, other->client->pers.netname,
							  other->client->resp.clientid);
	}
}

// gamex86.dll: 1001E43A..1001E60A
// gamei386.so: 0005640C..000565C9
void OSP_accuracyInfo (edict_t *ent, char *name, int cid)
{
	unsigned int	k;
	int			playerno;	/* invented: local copy of the client id */
	int			any;
	int			nindex;

	playerno = cid;
	any = 0;
	if (Q_stricmp (name, p_acc[playerno].netname))
	{
		gi.cprintf (ent, PRINT_HIGH, "No accuracy information for \"%s\"\n", name);
		return;
	}

	gi.cprintf (ent, PRINT_HIGH, "\nAccuracy info for \"%s\"\n", name);
	gi.cprintf (ent, PRINT_HIGH, "----------------------------------\n");

	for (k = 0; k < 10; k++)
	{
		nindex = a_info[k].index;
		if (p_acc[playerno].shots[nindex])
		{
			gi.cprintf (ent, PRINT_HIGH, "%s %.1f%% (%d/%d hits)\n",
						a_info[k].name,
						(float)(p_acc[playerno].hits[nindex] * 100) /
						p_acc[playerno].shots[nindex],
						p_acc[playerno].hits[nindex],
						p_acc[playerno].shots[nindex]);
			any = 1;
		}
	}

	if (!any)
		gi.cprintf (ent, PRINT_HIGH, "\"%s\" didn't shoot a thing!\n", name);
	else
	{
		gi.cprintf (ent, PRINT_HIGH, "Total damage given: %d\n",
					p_acc[playerno].dgiven);
		gi.cprintf (ent, PRINT_HIGH, "Total damage rcvd : %d\n",
					p_acc[playerno].dtaken);
	}
	gi.cprintf (ent, PRINT_HIGH, "\n");
}

// gamex86.dll: 1001E60A..1001E807
// gamei386.so: 000565CC..00056795
void OSP_oldaccuracy_cmd (edict_t *ent)
{
	int			i;
	int			x;
	int			any;

	x = 0;
	if (ent->client->resp.osp_r010 > level.framenum)
		return;
	ent->client->resp.osp_r010 = level.framenum + 2;

	if (gi.argc () == 1)
	{
		gi.cprintf (ent, PRINT_HIGH, "Players from previous match:\n");
		gi.cprintf (ent, PRINT_HIGH, "----------------------------\n");
		any = 0;
		for (i = 0; i < 256; i++)
		{
			if (o_acc[i].netname[0])
			{
				if (!x)
				{
					gi.cprintf (ent, PRINT_HIGH, "%s", o_acc[i].netname);
					x++;
				}
				else if (x < 4)
				{
					gi.cprintf (ent, PRINT_HIGH, ", %s", o_acc[i].netname);
					x++;
				}
				else
				{
					gi.cprintf (ent, PRINT_HIGH, "\n%s", o_acc[i].netname);
					x = 0;
				}
				any = 1;
			}
		}

		if (any)
			gi.cprintf (ent, PRINT_HIGH, "\n\n");
		else
			gi.cprintf (ent, PRINT_HIGH,
						"No info on players from previous match!\n\n");
		return;
	}

	for (i = 0; i < 256; i++)
	{
		if (!Q_stricmp (gi.argv (1), o_acc[i].netname))
		{
			OSP_oldAccuracyInfo (ent, i);
			return;
		}
	}

	gi.cprintf (ent, PRINT_HIGH, "No accuracy information for \"%s\".\n",
				gi.argv (1));
}

// The o_acc twin of OSP_accuracyInfo: the same page, but read out of the
// previous match's saved accuracy table rather than the live one.
// gamex86.dll: 1001E807..1001E9AB
// gamei386.so: 00056798..00056939
void OSP_oldAccuracyInfo (edict_t *ent, int cid)
{
	unsigned int	k;
	int		nindex;					// invented name
	int		nfound;

	nfound = 0;

	gi.cprintf (ent, PRINT_HIGH, "\nOld accuracy info for \"%s\"\n",
				o_acc[cid].netname);
	gi.cprintf (ent, PRINT_HIGH,
				"--------------------------------------\n");

	for (k = 0; k < 10; k++)
	{
		nindex = a_info[k].index;
		if (o_acc[cid].shots[nindex])
		{
			gi.cprintf (ent, PRINT_HIGH, "%s %.1f%% (%d/%d hits)\n",
						a_info[k].name,
						(double)(100 * o_acc[cid].hits[nindex]) /
						o_acc[cid].shots[nindex],
						o_acc[cid].hits[nindex],
						o_acc[cid].shots[nindex]);
			nfound = 1;
		}
	}

	if (!nfound)
		gi.cprintf (ent, PRINT_HIGH, "\"%s\" didn't shoot a thing!\n",
					o_acc[cid].netname);
	else
	{
		gi.cprintf (ent, PRINT_HIGH, "Total damage given: %d\n",
					o_acc[cid].dgiven);
		gi.cprintf (ent, PRINT_HIGH, "Total damage rcvd : %d\n",
					o_acc[cid].dtaken);
	}

	gi.cprintf (ent, PRINT_HIGH, "\n");
}

// gamex86.dll: 1001E9AB..1001EB3F
// gamei386.so: 0005693C..00056A99
void OSP_ffajoin_cmd (edict_t *ent)
{
	if (sync_stat == 4 && (int)match_latejoin->value <= 1)
	{
		gi.cprintf (ent, PRINT_HIGH,
					"Sorry, you cannot enter a match in progress\n");
		return;
	}

	if (ent->client->resp.entered == ENTERED_ENTERED)
		return;

	ent->client->chase_target = NULL;
	ent->client->resp.entered = ENTERED_ENTERED;
	ent->client->resp.osp_r240 = 0;
	ent->client->osp_t040 = 0;
	ent->client->osp_t03c = NULL;

	if (!ent->client->resp.osp_r030)
	{
		ent->client->resp.osp_r030 = 1;
		ent->client->resp.enterframe = level.framenum;
		OSP_setSingleAccuracy (ent);
	}
	else
		ent->client->resp.enterframe = level.framenum - ent->client->resp.osp_r2d4;

	active_clients++;
	gi.bprintf (PRINT_HIGH, "%s entered the game (clients = %i)\n",
				ent->client->pers.netname, active_clients);
	ent->client->resp.score = ent->client->resp.osp_r248;
	ent->client->resp.osp_r0a0--;
	ent->client->resp.osp_r09c--;
	EntityListAdd (ent);
	OSP_DoRankSort ();
	q2log_playerEntered (ent);
}

// "vote <what> <value>".  Two calling conventions: with mode 0 the command
// reads its own arguments (and is rate-limited through resp.osp_r010); with
// mode non-zero the caller supplies argc/what/value directly, which is how the
// voting menus propose without going through the console.
// Every arm either sets vote_item and falls through to the propose tail, or
// leaves it at zero, which the `if (!vote_item)` there turns into a no-op --
// so a rejected vote needs no early return of its own.
// gamex86.dll: 1001EB3F..1001FBBE
// gamei386.so: 00056A9C..00057CB1
void OSP_vote_cmd (edict_t *ent, int mode, int nargs, char *what, char *value)
{
	char	*a1;
	char	*a2;
	cvar_t	*bfg;
	cvar_t	*quad;
	bot_t	*b;
	edict_t	*cl;
	edict_t	*other;
	// argc does double duty: it counts bots in the entry loop below, then is
	// overwritten with the real argument count once that is done.
	int		argc;
	int		nbots;
	int		i;

	bfg = gi.cvar ("allow_bfg", "1", 0);
	quad = gi.cvar ("allow_item_quad", "1", 0);

	connected_clients = 0;
	active_clients = 0;
	argc = 0;

	for (i = 1; i <= game.maxclients; i++)
	{
		cl = g_edicts + i;

		if (cl->inuse && cl->client && cl->client->pers.connected)
		{
			connected_clients++;

			if (cl->client->resp.entered == ENTERED_ENTERED)
				active_clients++;
			if (cl->flags & FL_OSP_BOT)
				argc++;
		}
	}

	botglobals.numbots = argc;
	if (bots_votedin > argc)
		bots_votedin = 0;

	if (!connected_clients)
		return;

	if (!mode)
	{
		if (ent->client->resp.osp_r010 > level.framenum)
			return;
		ent->client->resp.osp_r010 = level.framenum + 2;

		argc = gi.argc ();
		a1 = gi.argv (1);
		a2 = gi.argv (2);
	}
	else
	{
		argc = nargs;
		a1 = what;
		a2 = value;
	}

	if (!(int)vote_enable->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "Voting disabled on this server.\n");
		return;
	}

	if (ent->client->resp.entered != ENTERED_ENTERED && !ent->osp_e39c &&
		!(int)vote_countspectators->value &&
		active_clients - botglobals.numbots)
	{
		gi.cprintf (ent, PRINT_HIGH, "Observers cannot vote with active\n");
		gi.cprintf (ent, PRINT_HIGH, "players in the game.\n");
		return;
	}

	if (match_paused)
	{
		gi.cprintf (ent, PRINT_HIGH, "Cannot vote during a paused match.\n");
		return;
	}

	if (argc == 1)
	{
		if (!vote_inprogress)
		{
			gi.cprintf (ent, PRINT_HIGH, "\"vote\" Commands:\n\n");
			gi.cprintf (ent, PRINT_HIGH, " vote map <mapname>     New map\n");
			gi.cprintf (ent, PRINT_HIGH, " vote config \"name\"     New server config\n");
			gi.cprintf (ent, PRINT_HIGH, " vote timelimit <min>   New timelimit\n");
			gi.cprintf (ent, PRINT_HIGH, " vote fraglimit <frags> New fraglimit\n");
			gi.cprintf (ent, PRINT_HIGH, " vote hook <1|0>        Enable/disable hook\n");
			gi.cprintf (ent, PRINT_HIGH, " vote runes <1|0>       Enable/disable runes\n");
			gi.cprintf (ent, PRINT_HIGH, " vote quad <1|0>        Enable/disable quad\n");
			gi.cprintf (ent, PRINT_HIGH, " vote bfg  <1|0>        Enable/disable BFG\n");
			gi.cprintf (ent, PRINT_HIGH, " vote kick <name/ID>    Kick player\n");
			gi.cprintf (ent, PRINT_HIGH, " vote toggles <value>   Specify item toggle flags\n");
			gi.cprintf (ent, PRINT_HIGH, " vote addbot <#>        Add <#> bots\n");
			gi.cprintf (ent, PRINT_HIGH, " vote rembot <#>        Remove <#> bots\n");
			gi.cprintf (ent, PRINT_HIGH, " vote yes               Accept current vote\n");
			gi.cprintf (ent, PRINT_HIGH, " vote no                Deny current vote\n");
			gi.cprintf (ent, PRINT_HIGH, " vote                   Help or show current vote\n");
			gi.cprintf (ent, PRINT_HIGH, "(More info in the voting sections of the menu)\n\n");
			return;
		}

		OSP_voteinfo (ent, 0);
		OSP_votePercent (ent, 1);
		return;
	}

	if (!Q_stricmp (a1, "yes"))
	{
		OSP_yes_cmd (ent);
		return;
	}

	if (!Q_stricmp (a1, "no"))
	{
		OSP_no_cmd (ent);
		return;
	}

	if (vote_inprogress)
	{
		gi.cprintf (ent, PRINT_HIGH, "\nVote already in progress:\n");
		OSP_voteinfo (ent, 0);
		OSP_votePercent (ent, 1);
		return;
	}

	if (!Q_stricmp (a1, "map"))
	{
		if (!(int)vote_enable_map->value)
		{
			gi.cprintf (ent, PRINT_HIGH, "Map voting is currently disabled.\n");
			return;
		}

		if (argc == 2)
		{
			OSP_mapList (ent);
			gi.cprintf (ent, PRINT_HIGH, "Usage: vote map <mapname>\n\n");
		}
		else if (OSP_mapExists (ent, a2, 0))
			vote_item = 1;
	}
	else if (!Q_stricmp (a1, "config"))
	{
		if (!(int)vote_enable_config->value)
		{
			gi.cprintf (ent, PRINT_HIGH,
						"Server config voting is currently disabled.\n");
			return;
		}

		if (argc == 2)
		{
			OSP_configList (ent);
			gi.cprintf (ent, PRINT_HIGH,
						"Usage: vote config \"config_name\"\n\n");
		}
		else if (OSP_configExists (ent, a2))
			vote_item = 2;
	}
	else if (!Q_stricmp (a1, "timelimit") || !Q_stricmp (a1, "tl"))
	{
		if (!(int)vote_enable_time->value)
		{
			gi.cprintf (ent, PRINT_HIGH,
						"timelimit voting is currently disabled.\n");
			return;
		}

		if (argc == 2)
			gi.cprintf (ent, PRINT_HIGH, "Current timelimit: %d\n",
						(int)timelimit->value);
		else if (atoi (a2) < 0 || atoi (a2) > (int)menu_maxtime->value)
			gi.cprintf (ent, PRINT_HIGH, "Invalid timelimit!\n");
		else
			vote_item = 4;
	}
	else if (!Q_stricmp (a1, "fraglimit") || !Q_stricmp (a1, "fl"))
	{
		if (!(int)vote_enable_frag->value)
		{
			gi.cprintf (ent, PRINT_HIGH,
						"fraglimit voting is currently disabled.\n");
			return;
		}

		if (argc == 2)
			gi.cprintf (ent, PRINT_HIGH, "Current fraglimit: %d\n",
						(int)fraglimit->value);
		else if (atoi (a2) < 0 || atoi (a2) > (int)menu_maxfrag->value)
			gi.cprintf (ent, PRINT_HIGH, "Invalid fraglimit!\n");
		else
			vote_item = 8;
	}
	else if (!Q_stricmp (a1, "hook"))
	{
		if (!(int)vote_enable_hook->value)
		{
			gi.cprintf (ent, PRINT_HIGH, "Hook voting is currently disabled.\n");
			return;
		}

		if (argc == 2)
		{
			if ((int)hook_enable->value)
				gi.cprintf (ent, PRINT_HIGH, "Hook is currently ENABLED.\n");
			else
				gi.cprintf (ent, PRINT_HIGH, "Hook is currently DISABLED.\n");
		}
		else
			vote_item = 0x10;
	}
	else if (!Q_stricmp (a1, "runes"))
	{
		if (!(int)vote_enable_runes->value)
		{
			gi.cprintf (ent, PRINT_HIGH,
						"Runes voting is currently disabled.\n");
			return;
		}

		if (argc == 2)
		{
			if (rune_stat)
				gi.cprintf (ent, PRINT_HIGH, "Runes are currently ENABLED.\n");
			else
				gi.cprintf (ent, PRINT_HIGH, "Runes are currently DISABLED.\n");
		}
		else
			vote_item = 0x800;
	}
	else if (!Q_stricmp (a1, "toggles"))
	{
		if (!(int)vote_enable_toggles->value)
		{
			gi.cprintf (ent, PRINT_HIGH,
						"Item toggle voting is currently disabled.\n");
			return;
		}

		if (argc == 2)
			gi.cprintf (ent, PRINT_HIGH, "Current item toggles: %d\n",
						item_settings);
		else if (atoi (a2) < 0 || atoi (a2) >= 256)
			gi.cprintf (ent, PRINT_HIGH,
						"Invalid item toggle setting (%d) (%d)!\n",
						atoi (a2), item_settings);
		else
			vote_item = 0x20;
	}
	else if (!Q_stricmp (a1, "bfg"))
	{
		if (!(int)vote_enable_toggles->value)
		{
			gi.cprintf (ent, PRINT_HIGH, "BFG voting is currently disabled.\n");
			return;
		}

		if (argc == 2)
		{
			if ((int)bfg->value)
				gi.cprintf (ent, PRINT_HIGH, "BFG is currently ENABLED.\n");
			else
				gi.cprintf (ent, PRINT_HIGH, "BFG is currently DISABLED.\n");
		}
		else
			vote_item = 0x40;
	}
	else if (!Q_stricmp (a1, "quad"))
	{
		if (!(int)vote_enable_toggles->value)
		{
			gi.cprintf (ent, PRINT_HIGH, "Quad voting is currently disabled.\n");
			return;
		}

		if (argc == 2)
		{
			if ((int)quad->value)
				gi.cprintf (ent, PRINT_HIGH, "Quad is currently ENABLED.\n");
			else
				gi.cprintf (ent, PRINT_HIGH, "Quad is currently DISABLED.\n");
		}
		else
			vote_item = 0x80;
	}
	else if (!Q_stricmp (a1, "kick") || !Q_stricmp (a1, "kickplayer"))
	{
		if (!(int)vote_enable_kick->value)
		{
			gi.cprintf (ent, PRINT_HIGH,
						"Kick Player voting is currently disabled.\n");
			return;
		}

		if (argc == 2)
			gi.cprintf (ent, PRINT_HIGH,
				"\nUsage: vote kick [player_name or player_ID]\n\n");
		else
		{
			other = OSP_findPlayer (a2);

			if (!other)
			{
				gi.cprintf (ent, PRINT_HIGH, "\n*** Could not find \"%s\".\n",
							a2);
				gi.cprintf (ent, PRINT_HIGH,
					"Try quoting the name or use the player's ID.\n\n");
			}
			else if (other->osp_e39c)
			{
				gi.cprintf (ent, PRINT_HIGH,
							"\n*** Cannot vote to kick referees!\n");
				return;
			}
			else
			{
				// The vote carries the ID, not the name.
				sprintf (a2, "%d", other->client->resp.clientid);
				vote_item = 0x1000;
			}
		}
	}
	else if (!Q_stricmp (a1, "specbot") || !Q_stricmp (a1, "specificbot"))
	{
		CheckForNewBotFile ();

		for (nbots = 0, b = botlist; b; b = b->next, nbots++)
			;

		if (!(int)vote_enable_bots->value)
		{
			gi.cprintf (ent, PRINT_HIGH, "Bot voting is currently disabled.\n");
			return;
		}

		if (argc == 2)
			gi.cprintf (ent, PRINT_HIGH,
						"voted bots in the game: %d (max=%d).\n",
						bots_votedin, (int)vote_bots_max->value);
		else if (connected_clients == (int)maxclients->value)
			gi.cprintf (ent, PRINT_HIGH,
						"Sorry server is full, cannot add anymore bots.\n");
		else if (bots_votedin == (int)vote_bots_max->value)
			gi.cprintf (ent, PRINT_HIGH, "Sorry, cannot add anymore bots.\n");
		else if (atoi (a2) < 0 || atoi (a2) > nbots)
			gi.cprintf (ent, PRINT_HIGH, "Voted bot # out of range\n");
		else
			vote_item = 0x100;
	}
	else if (!Q_stricmp (a1, "addbots") || !Q_stricmp (a1, "addbot"))
	{
		if (!(int)vote_enable_bots->value)
		{
			gi.cprintf (ent, PRINT_HIGH, "Bot voting is currently disabled.\n");
			return;
		}

		if (argc == 2)
			gi.cprintf (ent, PRINT_HIGH,
						"voted bots in the game: %d (max=%d).\n",
						bots_votedin, (int)vote_bots_max->value);
		else if (connected_clients == (int)maxclients->value)
			gi.cprintf (ent, PRINT_HIGH,
						"Sorry server is full, cannot add anymore bots.\n");
		else if (connected_clients == (int)maxclients->value ||
				 bots_votedin == (int)vote_bots_max->value)
			gi.cprintf (ent, PRINT_HIGH, "Sorry, cannot add anymore bots.\n");
		else if (atoi (a2) + connected_clients > (int)maxclients->value)
			gi.cprintf (ent, PRINT_HIGH, "You can add in only %d more bots.\n",
						(int)maxclients->value - connected_clients);
		else if (atoi (a2) + bots_votedin > (int)vote_bots_max->value)
			gi.cprintf (ent, PRINT_HIGH, "You can add in only %d more bots.\n",
						(int)vote_bots_max->value - bots_votedin);
		else if (atoi (a2) < 0)
			gi.cprintf (ent, PRINT_HIGH, "Cannot add less than 0 bots!\n");
		else
			vote_item = 0x200;
	}
	else if (!Q_stricmp (a1, "rembots") || !Q_stricmp (a1, "removebots") ||
			 !Q_stricmp (a1, "rembot") || !Q_stricmp (a1, "removebot"))
	{
		if (!(int)vote_enable_bots->value)
		{
			gi.cprintf (ent, PRINT_HIGH, "Bot voting is currently disabled.\n");
			return;
		}

		if (argc == 2)
			gi.cprintf (ent, PRINT_HIGH,
						"voted bots in the game: %d (max=%d).\n",
						bots_votedin, (int)vote_bots_max->value);
		else if (!botglobals.numbots)
			gi.cprintf (ent, PRINT_HIGH, "Sorry, no more bots to remove!\n");
		else if (atoi (a2) > bots_votedin)
			gi.cprintf (ent, PRINT_HIGH, "You can remove only %d more bots.\n",
						bots_votedin);
		else if (atoi (a2) < 0)
			gi.cprintf (ent, PRINT_HIGH, "Cannot remove less than 0 bots!\n");
		else
			vote_item = 0x400;
	}
	else
		gi.cprintf (ent, PRINT_HIGH, "Invalid vote selection \"%s\"\n", a1);

	if (vote_item)
	{
		vote_inprogress = 1;
		vote_frametime = level.framenum + (int)vote_time->value * 10;
		strcpy (vote_value, a2);
		vote_yea = 1;
		ent->client->resp.osp_r2d8 = 1;

		// A referee's vote carries the room.
		if (ent->osp_e39c)
		{
			vote_yea = connected_clients;
			vote_nay = 0;
		}

		gi.bprintf (PRINT_HIGH, "%s has initiated a vote!\n",
					ent->client->pers.greenname);
		OSP_voteinfo (ent, 1);
		q2log_voteInfo ("Propose", a1, a2);
		OSP_checkVote ();
	}
}

// gamex86.dll: 1001FBBE..1001FE09
// gamei386.so: 00057CB4..00057EC7
int OSP_votePercent (edict_t *ent, int what)
{
	edict_t		*e;
	int			i;
	int			botcount;
	int			yes;
	int			no;

	connected_clients = 0;
	active_clients = 0;
	botcount = 0;

	for (i = 1; i <= game.maxclients; i++)
	{
		e = g_edicts + i;
		if (e->inuse)
		{
			if (e->client && e->client->pers.connected)
			{
				connected_clients++;
				if (e->client->resp.entered == ENTERED_ENTERED)
					active_clients++;
				if (e->flags & FL_OSP_BOT)
					botcount++;
			}
		}
	}
	botglobals.numbots = botcount;

	if (!(int)vote_countspectators->value)
	{
		if (!active_clients || active_clients - botglobals.numbots <= 0)
		{
			yes = vote_yea * 100 / (connected_clients - botglobals.numbots);
			no = vote_nay * 100 / (connected_clients - botglobals.numbots);
		}
		else
		{
			yes = vote_yea * 100 / (active_clients - botglobals.numbots);
			no = vote_nay * 100 / (active_clients - botglobals.numbots);
		}
	}
	else if (sync_stat != 4)
	{
		yes = vote_yea * 100 / (connected_clients - botglobals.numbots);
		no = vote_nay * 100 / (connected_clients - botglobals.numbots);
	}
	else
	{
		yes = vote_yea * 100 / (active_clients - botglobals.numbots);
		no = vote_nay * 100 / (active_clients - botglobals.numbots);
	}

	if (what == 1)
	{
		gi.cprintf (ent, PRINT_HIGH,
					"%d%% have accepted.\n%d%% have declined.\n", yes, no);
		gi.cprintf (ent, PRINT_HIGH, " ** Need %d%% to decide vote.\n\n",
					(int)vote_threshold->value);
	}
	else if (what == 2)
		return yes;
	else
		// what == 3: `ent` is really a char * -- OSP_menuVotePercent passes the
		// menu entry's text buffer, which is what makes this a sprintf.
		sprintf ((char *)ent, "%d%% Accepted, %d%% Declined.\n", yes, no);

	return 0;
}

// gamex86.dll: 1001FE09..1001FE9B
// gamei386.so: 00057EC8..00057F7A
void OSP_map_vote (void)
{
	q2log_voteInfo ("Pass", "map", vote_value);
	if (server_log)
		OSP_logAdminLog ("Vote_Pass: map - %s", vote_value);

	if (OSP_mapExists (NULL, vote_value, true))
	{
		sl_SoftGameEnd (&gi, level);
		q2log_gameEnd ("player map vote", 0);
		manual_map = 1;
		EndDMLevel ();
	}
}

// gamex86.dll: 1001FE9B..1001FFA3
// gamei386.so: 00057F7C..000580B7
void OSP_config_vote (void)
{
	char		cmd[256];

	q2log_voteInfo ("Pass", "config", vote_value);
	if (server_log)
		OSP_logAdminLog ("Vote_Pass: config - %s", vote_value);

	if (OSP_configExists (NULL, vote_value))
	{
		sl_SoftGameEnd (&gi, level);
		q2log_gameEnd ("player config vote", 0);
		manual_map = 2;
		gi.cvar_set ("__current_config", vote_value);
		gi.dprintf ("Changing to config: %s\n", vote_value);
		Com_sprintf (cmd, sizeof(cmd), "exec %s\n", vote_value);
		gi.AddCommandString (cmd);
		OSP_loadMaps ();
		EndDMLevel ();
		gi.cvar_set ("__dummy_nglog_name", "");
	}
}

// gamex86.dll: 1001FFA3..10020011
// gamei386.so: 000580B8..0005814B
void OSP_timelimit_vote (void)
{
	gi.bprintf (PRINT_HIGH, "New timelimit: %s\n", vote_value);
	q2log_voteInfo ("Pass", "timelimit", vote_value);
	if (server_log)
		OSP_logAdminLog ("Vote_Pass: timelimit - %s", vote_value);
	gi.cvar_set ("timelimit", vote_value);
	OSP_setShowParams ();
	hs_mode = 0;
}

// gamex86.dll: 10020011..1002007F
// gamei386.so: 0005814C..000581DF
void OSP_fraglimit_vote (void)
{
	gi.bprintf (PRINT_HIGH, "New fraglimit: %s\n", vote_value);
	q2log_voteInfo ("Pass", "fraglimit", vote_value);
	if (server_log)
		OSP_logAdminLog ("Vote_Pass: fraglimit - %s", vote_value);
	gi.cvar_set ("fraglimit", vote_value);
	OSP_setShowParams ();
	hs_mode = 0;
}

// gamex86.dll: 1002007F..1002012E
// gamei386.so: 000581E0..000582D4
void OSP_hook_vote (void)
{
	gi.cvar_set ("hook_enable", vote_value);
	OSP_setShowParams ();

	if ((int)hook_enable->value)
	{
		gi.bprintf (PRINT_HIGH, "Hook is ENABLED.\n");
		q2log_voteInfo ("Pass", "hook", "enabled");
		if (server_log)
			OSP_logAdminLog ("Vote_Pass: hook - enabled");
	}
	else
	{
		gi.bprintf (PRINT_HIGH, "Hook is DISABLED.\n");
		q2log_voteInfo ("Pass", "hook", "disabled");
		if (server_log)
			OSP_logAdminLog ("Vote_Pass: hook - disabled");
	}

	OSP_setFeatures ();
}

// gamex86.dll: 1002012E..1002020B
// gamei386.so: 000582D4..000583EF
void OSP_runes_vote (void)
{
	OSP_setShowParams ();

	if (atoi (vote_value))
	{
		rune_stat = RUNE_RESIST | RUNE_STRENGTH | RUNE_HASTE | RUNE_REGEN | RUNE_VAMPIRE;
		gi.bprintf (PRINT_HIGH, "Runes are ENABLED.\n");
		q2log_voteInfo ("Pass", "runes", "enabled");
		if (server_log)
			OSP_logAdminLog ("Vote_Pass: runes - enabled");
		runespawn = 0;
		OSP_setupRuneSpawn (0);
	}
	else
	{
		rune_stat = 0;
		gi.bprintf (PRINT_HIGH, "Runes are DISABLED.\n");
		q2log_voteInfo ("Pass", "runes", "disabled");
		if (server_log)
			OSP_logAdminLog ("Vote_Pass: runes - disabled");
		OSP_removeRunes ();
	}

	gi.dprintf ("rune_stat: %d\n", rune_stat);
	OSP_setFeatures ();
}

// gamex86.dll: 1002020B..1002026E
// gamei386.so: 000583F0..0005847C
void OSP_toggle_vote (void)
{
	gi.bprintf (PRINT_HIGH, "New item toggles passed!\n");
	q2log_voteInfo ("Pass", "item_toggle", vote_value);
	if (server_log)
		OSP_logAdminLog ("Vote_Pass: items - %s", vote_value);
	item_settings = atoi (vote_value);
	OSP_changeItems ();
	OSP_setShowParams ();
}

// gamex86.dll: 1002026E..1002030E
// gamei386.so: 0005847C..00058539
void OSP_bfg_vote (void)
{
	if (!atoi (vote_value))
	{
		gi.bprintf (PRINT_HIGH, "The BFG is now disabled.\n");
		item_settings &= ~ITEM_SET_BFG;
		if (server_log)
			OSP_logAdminLog ("Vote_Pass: bfg - disabled");
	}
	else
	{
		gi.bprintf (PRINT_HIGH, "The BFG is now enabled.\n");
		item_settings |= ITEM_SET_BFG;
		if (server_log)
			OSP_logAdminLog ("Vote_Pass: bfg - enabled");
	}
	q2log_voteInfo ("Pass", "bfg_status", vote_value);
	OSP_changeItems ();
	OSP_setShowParams ();
}

// gamex86.dll: 1002030E..100203AE
// gamei386.so: 0005853C..000585F9
void OSP_quad_vote (void)
{
	if (!atoi (vote_value))
	{
		gi.bprintf (PRINT_HIGH, "The Quad is now disabled\n");
		item_settings &= ~ITEM_SET_QUAD;
		if (server_log)
			OSP_logAdminLog ("Vote_Pass: quad - disabled");
	}
	else
	{
		gi.bprintf (PRINT_HIGH, "The Quad is now enabled\n");
		item_settings |= ITEM_SET_QUAD;
		if (server_log)
			OSP_logAdminLog ("Vote_Pass: quad - enabled");
	}
	q2log_voteInfo ("Pass", "quad_status", vote_value);
	OSP_changeItems ();
	OSP_setShowParams ();
}

// Carry out a passed "kick" vote.  A referee cannot be kicked this way -- that
// arm logs a Fail and gives up on the vote entirely rather than moving on to
// the next client.  Bots leave through BotServerCommand, everyone else gets an
// svc_disconnect and ClientDisconnect.
// gamex86.dll: 100203AE..10020570
// gamei386.so: 000585FC..000587D8
void OSP_kick_vote (void)
{
	char	scratch[32];
	int		t;
	edict_t	*cli;

	for (t = 1; t <= game.maxclients; t++)
	{
		cli = g_edicts + t;

		if (!cli->inuse || !cli->client ||
			cli->client->resp.clientid != atoi (vote_value))
			continue;

		if (cli->osp_e39c)
		{
			gi.bprintf (PRINT_HIGH,
				"** CANNOT KICK REFEREES BY VOTE -- vote ignored.\n");
			sprintf (scratch, "%s [ID: %d] (REFEREE)", cli->client->pers.netname,
					 cli->client->resp.clientid);
			q2log_voteInfo ("Fail", "kick_player", scratch);

			if (server_log)
				OSP_logAdminLog ("Vote_Fail: kick_player - %s", scratch);

			return;
		}

		gi.bprintf (PRINT_HIGH, "%s has been kicked by vote.\n",
					cli->client->pers.netname);
		sprintf (scratch, "%s [ID: %d]", cli->client->pers.netname,
				 cli->client->resp.clientid);
		q2log_voteInfo ("Pass", "kick_player", scratch);

		if (server_log)
			OSP_logAdminLog ("Vote_Pass: kick_player - %s", scratch);

		if (!(cli->flags & FL_OSP_BOT))
		{
			gi.WriteByte (svc_disconnect);
			gi.unicast (cli, true);
			ClientDisconnect (cli);
		}
		else
			BotServerCommand ("sv", "removebot", cli->client->pers.netname, 0);
	}
}

// gamex86.dll: 10020570..10020673
// gamei386.so: 000587D8..000588BA
void OSP_specbot_vote (void)
{
	int			count;
	bot_t		*bot;
	int			i;

	CheckForNewBotFile ();

	for (count = 0, bot = botlist; bot; bot = bot->next, count++)
		;

	for (i = 0, bot = botlist; i < atoi (vote_value);
		 i++, bot = bot->next)
		;

	BotServerCommand ("sv", "addbot", bot->name, bot->skin, bot->charfile,
					  bot->charname, NULL);
	bots_votedin++;
	gi.bprintf (PRINT_HIGH, "Bot: \"%s\" added!\n", bot->name);
	OSP_setShowParams ();
	q2log_voteInfo ("Pass", "specbot", bot->name);
	if (server_log)
		OSP_logAdminLog ("Vote_Pass: specific_bot - %s", bot->name);
}

// gamex86.dll: 10020673..10020736
// gamei386.so: 000588BC..000589AC
void OSP_addbots_vote (void)
{
	int			i;

	for (i = 0; i < atoi (vote_value); i++)
		AddRandomBot (NULL);

	bots_votedin += atoi (vote_value);

	if (atoi (vote_value) == 1)
		gi.bprintf (PRINT_HIGH, "1 new bot added!\n");
	else
		gi.bprintf (PRINT_HIGH, "%s new bots added!\n", vote_value);

	q2log_voteInfo ("Pass", "addbots", vote_value);
	if (server_log)
		OSP_logAdminLog ("Vote_Pass: addbots - %s", vote_value);
	OSP_setShowParams ();
}

// gamex86.dll: 10020736..1002081B
// gamei386.so: 000589AC..00058AC4
void OSP_removebots_vote (void)
{
	int			i;

	q2log_voteInfo ("Pass", "removebots", vote_value);
	if (server_log)
		OSP_logAdminLog ("Vote_Pass: removebots - %s", vote_value);

	OSP_clearVotes ();

	for (i = 0; i < atoi (vote_value); i++)
		BotServerCommand ("sv", "removebot", NULL);

	bots_votedin -= atoi (vote_value);
	if (bots_votedin < 0)
		bots_votedin = 0;

	if (atoi (vote_value) == 1)
		gi.bprintf (PRINT_HIGH, "1 bot removed!\n");
	else
		gi.bprintf (PRINT_HIGH, "%s bots removed!\n", vote_value);

	OSP_setShowParams ();
}

// gamex86.dll: 1002081B..1002095D
// gamei386.so: 00058AC4..00058C2F
void OSP_yes_cmd (edict_t *ent)
{
	if (!(int)vote_enable->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "Voting disabled on this server.\n");
		return;
	}

	if (ent->client->resp.entered != ENTERED_ENTERED && !ent->osp_e39c &&
		!(int)vote_countspectators->value &&
		active_clients - botglobals.numbots)
	{
		gi.cprintf (ent, PRINT_HIGH, "Observers cannot vote with active\n");
		gi.cprintf (ent, PRINT_HIGH, "players in the game.\n");
		return;
	}

	if (!vote_inprogress)
	{
		gi.cprintf (ent, PRINT_HIGH, "There is no vote currently in progress!\n");
		return;
	}

	if (match_paused)
	{
		gi.cprintf (ent, PRINT_HIGH, "Cannot vote during a paused match.\n");
		return;
	}

	if (ent->client->resp.osp_r2d8)
	{
		gi.cprintf (ent, PRINT_HIGH, "You have already voted!\n");
		return;
	}

	vote_yea++;
	ent->client->resp.osp_r2d8 = 1;
	// a referee's vote decides it outright
	if (ent->osp_e39c)
	{
		vote_yea = connected_clients;
		vote_nay = 0;
	}
	OSP_checkVote ();
}

// gamex86.dll: 1002095D..10020A9F
// gamei386.so: 00058C30..00058D9B
void OSP_no_cmd (edict_t *ent)
{
	if (!(int)vote_enable->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "Voting disabled on this server.\n");
		return;
	}

	if (ent->client->resp.entered != ENTERED_ENTERED && !ent->osp_e39c &&
		!(int)vote_countspectators->value &&
		active_clients - botglobals.numbots)
	{
		gi.cprintf (ent, PRINT_HIGH, "Observers cannot vote with active\n");
		gi.cprintf (ent, PRINT_HIGH, "players in the game.\n");
		return;
	}

	if (!vote_inprogress)
	{
		gi.cprintf (ent, PRINT_HIGH, "There is no vote currently in progress!\n");
		return;
	}

	if (match_paused)
	{
		gi.cprintf (ent, PRINT_HIGH, "Cannot vote during a paused match.\n");
		return;
	}

	if (ent->client->resp.osp_r2d8)
	{
		gi.cprintf (ent, PRINT_HIGH, "You have already voted!\n");
		return;
	}

	vote_nay++;
	ent->client->resp.osp_r2d8 = 1;
	if (ent->osp_e39c)
	{
		vote_nay = connected_clients;
		vote_yea = 0;
	}
	OSP_checkVote ();
}

// Called once a frame while a vote is running.  Recounts the clients (a vote
// can outlive the players that started it), then either carries the vote out
// or declares it failed.  The same OSP_votePercent result is used both as the
// pass threshold and as the DIVISOR of the nay tally, which is the target's
// own arithmetic; the clamp to 1 just below it is what keeps that division
// safe.
// gamex86.dll: 10020A9F..10020CFB
// gamei386.so: 00058D9C..000591DB
void OSP_checkVote (void)
{
	int		t;
	int		x;
	edict_t	*cli;
	int		nitem;

	connected_clients = 0;
	active_clients = 0;
	t = 0;

	for (x = 1; x <= game.maxclients; x++)
	{
		cli = g_edicts + x;

		if (cli->inuse && cli->client && cli->client->pers.connected)
		{
			connected_clients++;

			if (cli->client->resp.entered == ENTERED_ENTERED)
				active_clients++;
			if (cli->flags & FL_OSP_BOT)
				t++;
		}
	}

	botglobals.numbots = t;

	t = OSP_votePercent (NULL, 2);
	if (t < 1)
	t = 1;

	if (t >= (int)vote_threshold->value)
	{
	nitem = vote_item;
		OSP_clearVotes ();
		gi.bprintf (PRINT_HIGH, "Vote passed!\n");

	if (nitem == 1)
			OSP_map_vote ();
		else if (nitem == 2)
			OSP_config_vote ();
		else if (nitem == 4)
			OSP_timelimit_vote ();
		else if (nitem == 8)
			OSP_fraglimit_vote ();
		else if (nitem == 0x10)
			OSP_hook_vote ();
		else if (nitem == 0x800)
			OSP_runes_vote ();
		else if (nitem == 0x20)
			OSP_toggle_vote ();
		else if (nitem == 0x40)
			OSP_bfg_vote ();
		else if (nitem == 0x80)
			OSP_quad_vote ();
		else if (nitem == 0x1000)
			OSP_kick_vote ();
		else if (nitem == 0x100)
			OSP_specbot_vote ();
		else if (nitem == 0x200)
			OSP_addbots_vote ();
		else if (nitem == 0x400)
			OSP_removebots_vote ();
		else
			gi.bprintf (PRINT_HIGH, "Uhh, what were we voting on again?\n");

		OSP_closeMenus ();
	}
	else if (vote_nay * 100 / t >= (int)vote_threshold->value)
	{
		gi.bprintf (PRINT_HIGH, "Vote failed: %d to %d\n", vote_nay, vote_yea);
		OSP_clearVotes ();
		OSP_closeMenus ();
		q2log_voteInfo ("Fail", NULL, NULL);
	}
}

// gamex86.dll: 10020CFB..10020D83
// gamei386.so: 000591DC..0005926F
void OSP_clearVotes (void)
{
	int			i;
	edict_t		*ent;

	vote_inprogress = 0;
	vote_item = 0;
	vote_yea = 0;
	vote_nay = 0;

	for (i = 1; i <= game.maxclients; i++)
	{
		ent = g_edicts + i;
		if (!ent->inuse || !ent->client)
			continue;

		ent->client->resp.osp_r2d8 = 0;
	}
}

// Describe the vote that is currently running in one line and print it, either
// to one client or to everybody.  vote_item's encoding here is the vote's own,
// not resp.osp_r254's.
// gamex86.dll: 10020D83..100211F3
// gamei386.so: 00059270..0005970F
void OSP_voteinfo (edict_t *ent, qboolean broadcast)
{
	char	scratch[256];
	int		t;
	edict_t	*cp;

	if (vote_item == 0x10)
	{
		if (!atoi (vote_value))
			strcpy (scratch, "Set the \"hook\" to DISABLED");
		else
			strcpy (scratch, "Set the \"hook\" to ENABLED");
	}
	else if (vote_item == 0x800)
	{
		if (!atoi (vote_value))
			strcpy (scratch, "Set all \"runes\" to DISABLED");
		else
			strcpy (scratch, "Set all \"runes\" to ENABLED");
	}
	else if (vote_item == 4)
	{
		if (!atoi (vote_value))
			strcpy (scratch, "Set the \"timelimit\" to OFF");
		else
			sprintf (scratch, "Set \"timelimit\" to %d minute(s)",
					 atoi (vote_value));
	}
	else if (vote_item == 8)
	{
		if (!atoi (vote_value))
			strcpy (scratch, "Set the \"fraglimit\" to NONE");
		else
			sprintf (scratch, "Set \"fraglimit\" to %d frag(s)",
					 atoi (vote_value));
	}
	else if (vote_item == 1)
		sprintf (scratch, "Change current map to \"%s\"", vote_value);
	else if (vote_item == 2)
		sprintf (scratch, "Change current server config to \"%s\"", vote_value);
	else if (vote_item == 0x20)
		OSP_listItems (scratch);
	else if (vote_item == 0x40)
	{
		if (!atoi (vote_value))
			strcpy (scratch, "Set the BFG to DISABLED");
		else
			strcpy (scratch, "Set the BFG to ENABLED");
	}
	else if (vote_item == 0x80)
	{
		if (!atoi (vote_value))
			strcpy (scratch, "Set the Quad to DISABLED");
		else
			strcpy (scratch, "Set the Quad to ENABLED");
	}
	else if (vote_item == 0x1000)
	{
		for (t = 1; t <= game.maxclients; t++)
		{
			cp = g_edicts + t;

			if (!cp->inuse || !cp->client ||
				cp->client->resp.clientid != atoi (vote_value))
				continue;

			sprintf (scratch, "Kick Player: %s", cp->client->pers.netname);
			break;
		}
	}
	else if (vote_item == 0x100)
		strcpy (scratch, "Add 1 Gladiator bot.");
	else if (vote_item == 0x200)
	{
		if (atoi (vote_value) == 1)
			strcpy (scratch, "Add 1 Gladiator bot.");
		else
			sprintf (scratch, "Add %d Gladiator bots.", atoi (vote_value));
	}
	else if (vote_item == 0x400)
	{
		if (atoi (vote_value) == 1)
			strcpy (scratch, "Remove 1 Gladiator bot.");
		else
			sprintf (scratch, "Remove %d Gladiator bots.", atoi (vote_value));
	}
	else
		strcpy (scratch, "Vote error, invalid vote item active");

	if (!broadcast)
	{
		gi.cprintf (ent, PRINT_HIGH, "Proposal: %s\n", scratch);
		gi.cprintf (ent, PRINT_CHAT, "Type \"yes\" at console to accept.\n");
		gi.cprintf (ent, PRINT_CHAT, "Type \"no\" at console to decline.\n");
	}
	else
	{
		gi.bprintf (PRINT_HIGH, "Proposal: %s\n", scratch);
		gi.bprintf (PRINT_CHAT, "Type \"yes\" at console to accept.\n");
		gi.bprintf (PRINT_CHAT, "Type \"no\" at console to decline.\n");
	}
}

// Turn a staged item-toggle vote value into the human-readable list of the
// toggles it would actually CHANGE -- each arm compares the requested bit
// against what is in force and says nothing when they already agree.  Built
// into a local and copied out at the end, so the caller's buffer need not be
// terminated first.
// gamex86.dll: 100211F3..100218DF
// gamei386.so: 00059710..00059E6B
void OSP_listItems (char *out)
{
	char	buf[256];
	int		any;
	cvar_t	*bfg;
	cvar_t	*cells;
	cvar_t	*powershield;
	cvar_t	*quad;
	cvar_t	*invul;
	cvar_t	*teamhurtself;
	cvar_t	*ffahurtself;
	cvar_t	*teamhurtteam;
	int		want;
	int		dmf;

	any = 0;

	bfg = gi.cvar ("allow_bfg", "1", 0);
	cells = gi.cvar ("allow_ammo_cells", "1", 0);
	powershield = gi.cvar ("allow_item_powershield", "1", 0);
	quad = gi.cvar ("allow_item_quad", "1", 0);
	invul = gi.cvar ("allow_item_invul", "1", 0);
	teamhurtself = gi.cvar ("team_hurtself", "1", 0);
	ffahurtself = gi.cvar ("ffa_hurtself", "1", 0);
	teamhurtteam = gi.cvar ("team_hurtteam", "1", 0);

	buf[0] = 0;
	want = atoi (vote_value);

	if (want & 1)
	{
		if (!(int)quad->value)
		{
			strcat (buf, " quad ON");
			any = 1;
		}
	}
	else if ((int)quad->value)
	{
		strcat (buf, " quad OFF");
		any = 1;
	}

	if (want & 2)
	{
		if (!(int)invul->value)
		{
			if (any)
				strcat (buf, ",");
			strcat (buf, " invul ON");
			any = 1;
		}
	}
	else if ((int)invul->value)
	{
		if (any)
			strcat (buf, ",");
		strcat (buf, " invul OFF");
		any = 1;
	}

	if (want & 8)
	{
		if (!(int)bfg->value)
		{
			if (any)
				strcat (buf, ",");
			strcat (buf, " bfg ON");
			any = 1;
		}
	}
	else if ((int)bfg->value)
	{
		if (any)
			strcat (buf, ",");
		strcat (buf, " bfg OFF");
		any = 1;
	}

	if (want & 0x10)
	{
		if (!(int)powershield->value)
		{
			if (any)
				strcat (buf, ",");
			strcat (buf, " power armor ON");
			any = 1;
		}
	}
	else if ((int)powershield->value)
	{
		if (any)
			strcat (buf, ",");
		strcat (buf, " power armor OFF");
		any = 1;
	}

	if (m_mode > 1)
	{
		if (want & 0x40)
		{
			if (!(int)teamhurtself->value)
			{
				if (any)
					strcat (buf, ",");
				strcat (buf, " self damage ON");
				any = 1;
			}
		}
		else if ((int)teamhurtself->value)
		{
			if (any)
				strcat (buf, ",");
			strcat (buf, " self damage OFF");
			any = 1;
		}

	}
	else
	{
		if (want & 0x40)
		{
			if (!(int)ffahurtself->value)
			{
				if (any)
					strcat (buf, ",");
				strcat (buf, " self damage ON");
				any = 1;
			}
		}
		else if ((int)ffahurtself->value)
		{
			if (any)
				strcat (buf, ",");
			strcat (buf, " self damage OFF");
			any = 1;
		}

	}

	if (m_mode == 2)
	{
		if (want & 0x80)
		{
			if (!(int)teamhurtself->value)
			{
				if (any)
					strcat (buf, ",");
				strcat (buf, " team damage ON");
				any = 1;
			}
		}
		else if ((int)teamhurtself->value)
		{
			if (any)
				strcat (buf, ",");
			strcat (buf, " team damage OFF");
			any = 1;
		}

	}

	dmf = (int)dmflags->value;

	if (want & 4)
	{
		if (!(dmf & DF_QUAD_DROP))
		{
			if (any)
				strcat (buf, ",");
			strcat (buf, " quad drop ON");
			any = 1;
		}
	}
	else if (dmf & DF_QUAD_DROP)
	{
		if (any)
			strcat (buf, ",");
		strcat (buf, " quad drop OFF");
		any = 1;
	}

	if (want & 0x20)
	{
		if (!(dmf & DF_WEAPONS_STAY))
		{
			if (any)
				strcat (buf, ",");
			strcat (buf, " weapons STAY");
			any = 1;
		}
	}
	else if (dmf & DF_WEAPONS_STAY)
	{
		if (any)
			strcat (buf, ",");
		strcat (buf, " weapons DONT STAY");
		any = 1;
	}

	strcpy (out, buf);
}

// "time" -- call a time-out, or a time-in on a match you paused yourself.
// Team play spends the team's allowance (teams[].osp_m10c), 1v1 the player's
// own (resp.osp_r2d0).  The time-in announcement really is printed three times
// over; that is the target's own.
// gamex86.dll: 100218DF..10021D33
// gamei386.so: 00059E6C..0005A312
void OSP_playertime_cmd (edict_t *ent)
{
	int		t;
	int		team;
	edict_t	*cli;

	team = ent->client->resp.team;

	if (m_mode == 2 && ent->osp_e39c && !ent->client->resp.osp_r2c4)
	{
		gi.cprintf (ent, PRINT_HIGH,
			"Refs can't call timeouts.  Use r_mpause to pause match.\n");
		return;
	}

	if (match_paused == 1)
		return;
	if (m_mode < 2 ||
		(ent->client->resp.entered != ENTERED_ENTERED && !ent->osp_e39c))
		return;

	if (!ent->client->resp.osp_r2c4 && m_mode == 2 && !ent->osp_e39c)
	{
		gi.cprintf (ent, PRINT_HIGH,
					"Only team captains can call a timeout\n");
		return;
	}

	if (match_paused && who_paused != -1 && who_paused != ent - g_edicts &&
		!ent->osp_e39c)
	{
		gi.cprintf (ent, PRINT_HIGH,
			"Sorry, you cannot unpause a match that you didn't pause.\n");
		return;
	}

	if (sync_stat < 4)
	{
		gi.cprintf (ent, PRINT_HIGH, "Can't pause a match during warmup.\n");
		return;
	}

	if (!match_paused && m_mode == 2 && !teams[team].osp_m10c)
	{
		gi.cprintf (ent, PRINT_HIGH,
			"Sorry, you team has no more timeouts to call.\n");
		return;
	}

	if (!match_paused && m_mode == 3 && !ent->client->resp.osp_r2d0)
	{
		gi.cprintf (ent, PRINT_HIGH,
					"Sorry, you have no more timeouts to call.\n");
		return;
	}

	if (m_mode == 2)
	{
		if (!match_paused)
		{
			for (t = 1; t <= game.maxclients; t++)
			{
				cli = g_edicts + t;

				if (!cli->inuse || !cli->client)
					continue;

				gi.centerprintf (cli, "Time-Out called by %s of team \"%s\"\n",
								 ent->client->pers.netname,
								 teams[team].netname);
				gi.cprintf (cli, PRINT_CHAT,
							"Time-Out called by %s of team \"%s\"\n",
							ent->client->pers.netname, teams[team].netname);
			}
		}
		else
		{
			gi.bprintf (PRINT_CHAT, "Time-In called by %s\n",
						ent->client->pers.netname);
			gi.bprintf (PRINT_CHAT, "Time-In called by %s\n",
						ent->client->pers.netname);
			gi.bprintf (PRINT_CHAT, "Time-In called by %s\n",
						ent->client->pers.netname);
		}
	}

	if (m_mode == 3)
	{
		if (!match_paused)
		{
			for (t = 1; t <= game.maxclients; t++)
			{
				cli = g_edicts + t;

				if (!cli->inuse || !cli->client)
					continue;

				gi.centerprintf (cli, "Time-Out called by %s\n",
								 ent->client->pers.netname);
				gi.cprintf (cli, PRINT_CHAT, "Time-Out called by %s\n",
							ent->client->pers.netname);
			}
		}
		else
		{
			gi.bprintf (PRINT_CHAT, "Time-In called by %s\n",
						ent->client->pers.netname);
			gi.bprintf (PRINT_CHAT, "Time-In called by %s\n",
						ent->client->pers.netname);
			gi.bprintf (PRINT_CHAT, "Time-In called by %s\n",
						ent->client->pers.netname);
		}
	}

	if (!match_paused)
	{
		match_paused = 1;
		pause_time = match_pausetime->value;
		who_paused = ent - g_edicts;

		if (m_mode == 2)
			teams[team].osp_m10c--;
		else
			ent->client->resp.osp_r2d0--;
	}
	else
		match_paused = 3;
}

// gamex86.dll: 10021D33..10021E14
// gamei386.so: 0005A314..0005A3BB
void OSP_hud_cmd (edict_t *ent)
{
	if (ent->client->resp.osp_r00c > 1)
		ent->client->resp.osp_r00c = 1;
	else if (ent->client->resp.osp_r00c < 0)
		ent->client->resp.osp_r00c = 0;
	ent->client->resp.osp_r00c = 1 - ent->client->resp.osp_r00c;

	if (m_mode < 2)
	{
		if (!ent->client->resp.osp_r00c)
			OSP_clientConfigString (ent, CS_STATUSBAR, dm_statusbar);
		else
			OSP_clientConfigString (ent, CS_STATUSBAR, dm_statusbar_alt);
	}
	else
	{
		if (!ent->client->resp.osp_r00c)
			OSP_clientConfigString (ent, CS_STATUSBAR, team_statusbar);
		else
			OSP_clientConfigString (ent, CS_STATUSBAR, team_statusbar_alt);
	}
}

// gamex86.dll: 10021E14..10021E82
// gamei386.so: 0005A3BC..0005A441
void OSP_oldscores_cmd (edict_t *ent)
{
	if (!old_scores[0])
	{
		gi.cprintf (ent, PRINT_HIGH, "No previous match to view!\n");
		return;
	}

	gi.WriteByte (svc_layout);
	gi.WriteString (old_scores);
	gi.unicast (ent, false);
	ent->client->resp.osp_r24c = 1;
	ent->client->showscores = true;
}

// gamex86.dll: 10021E82..10021FE7
// gamei386.so: 0005A444..0005A5A2
void OSP_muzzle_cmd (edict_t *ent)
{
	int		mode;

	if (gi.argc () == 1)
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: %s <mode>\nModes:\n", gi.argv (0));
		gi.cprintf (ent, PRINT_HIGH, " %d - Accept all chats.\n", 0);
		gi.cprintf (ent, PRINT_HIGH, " %d - Ignore observers.\n", 1);
		if (m_mode == 2)
			gi.cprintf (ent, PRINT_HIGH, " %d - Ignore opposing team.\n", 2);
		gi.cprintf (ent, PRINT_HIGH, " %d - Ignore all clients.\n\n", 3);
		gi.cprintf (ent, PRINT_HIGH,
					" ----> You're currently set to mode \"%d\"\n\n",
					ent->client->resp.osp_r0b0);
		return;
	}

	if (!(mode = ent->client->resp.osp_r0b0 =
		  atoi (gi.argv (1))))
		gi.cprintf (ent, PRINT_HIGH, "Accepting all client chat messages.\n");
	else if (mode == 1)
		gi.cprintf (ent, PRINT_HIGH, "Ignoring all observer chat messages.\n");
	else if (mode == 2)
		gi.cprintf (ent, PRINT_HIGH, "Ignoring all opposing team chat messages.\n");
	else
	{
		gi.cprintf (ent, PRINT_HIGH, "Ignoring ALL chat messages.\n");
		ent->client->resp.osp_r0b0 = 3;
	}
}

// gamex86.dll: 10021FE7..10022150
// gamei386.so: 0005A5A4..0005A774
void OSP_isreferee_cmd (edict_t *ent)
{
	if (!(int)referee_enable->value || !referee_password->string[0] ||
		!strncmp (referee_password->string, "none", 4))
	{
		if (gi.argc () == 3)
			gi.cprintf (ent, PRINT_HIGH, "Referee status disabled.\n");
		ent->osp_e39c = 0;
		return;
	}

	if (gi.argc () == 1)
	{
		ent->osp_e39c = 0;
		return;
	}

	if (strcmp (gi.argv (1), "1"))
	{
		ent->osp_e39c = 0;
		return;
	}

	if (strncmp (referee_password->string, gi.argv (2),
				 strlen (referee_password->string)))
	{
		gi.cprintf (ent, PRINT_HIGH, "Referee password required or incorrect.\n");
		gi.WriteByte (svc_disconnect);
		gi.unicast (ent, true);
		ClientDisconnect (ent);
		return;
	}

	gi.bprintf (PRINT_HIGH, "** Referee %s joined the match!\n",
				ent->client->pers.netname + 16);
	ent->osp_e39c = 1;
	ent->client->resp.osp_r02c = 1;
}

// "referee <password>".  Either referee_password or the server's own
// rcon_password will do, and either being unset or literally "none" takes it
// out of play -- so the mode is only available when at least one of the two is
// a real password.  Every outcome is logged to the admin log with the player's
// address, which OSP_getPlayerAddr writes into ent->osp_e37c.
// gamex86.dll: 10022150..100224E7
// gamei386.so: 0005A774..0005AB42
void OSP_referee_cmd (edict_t *ent)
{
	cvar_t	*rcon;

	rcon = gi.cvar ("rcon_password", NULL, 0);

	if (ent->osp_e39c)
	{
		OSP_adminMenu (ent);
		return;
	}

	if (!(int)referee_enable->value ||
		(!(rcon && rcon->string[0] && strcmp (rcon->string, "none")) &&
		 !(referee_password && referee_password->string[0] &&
		   strcmp (referee_password->string, "none"))))
	{
		gi.cprintf (ent, PRINT_HIGH,
					"Referee mode is disabled on this server.\n");

		if (server_log)
		{
			OSP_getPlayerAddr (ent);
			OSP_logAdminLog ("Referee_Attempt: %s (%s) [%s]",
							 ent->client->pers.netname, gi.argv (1),
							 ent->osp_e37c);
		}

		return;
	}

	if (gi.argc () == 1)
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: referee <password>\n\n");
		return;
	}

	// Neither password matches at all: log the attempt and stop.
	if (referee_password && referee_password->string[0] &&
		strcmp (referee_password->string, gi.argv (1)) &&
		rcon && rcon->string[0] && strcmp (rcon->string, gi.argv (1)))
	{
		if (server_log)
		{
			OSP_getPlayerAddr (ent);
			OSP_logAdminLog ("Referee_Fail: %s (%s) [%s]",
							 ent->client->pers.netname, gi.argv (1),
							 ent->osp_e37c);
		}

		gi.cprintf (ent, PRINT_HIGH, "Password incorrect.\n");
		return;
	}

	if ((referee_password && referee_password->string[0] &&
		 !strcmp (referee_password->string, gi.argv (1)) &&
		 strcmp (referee_password->string, "none")) ||
		(rcon && rcon->string[0] && !strcmp (rcon->string, gi.argv (1)) &&
		 strcmp (rcon->string, "none")))
	{
		gi.bprintf (PRINT_HIGH, "%s now has referee status.\n",
					ent->client->pers.greenname);
		gi.cprintf (ent, PRINT_HIGH,
					"\nType \"r_help\" for listing of ref commands.\n\n");
		ent->osp_e39c = 2;

		if (server_log)
		{
			OSP_getPlayerAddr (ent);
			OSP_logAdminLog ("Referee_Enable: %s [%s]",
							 ent->client->pers.netname, ent->osp_e37c);
		}

		OSP_adminMenu (ent);
		return;
	}

	gi.cprintf (ent, PRINT_HIGH, "Password incorrect.\n");

	if (server_log)
	{
		OSP_getPlayerAddr (ent);
		OSP_logAdminLog ("Referee_Fail2: %s (%s) [%s]",
						 ent->client->pers.netname, gi.argv (1),
						 ent->osp_e37c);
	}
}

// gamex86.dll: 100224E7..10022647
// gamei386.so: 0005AB44..0005AC96
void OSP_rhelp_cmd (edict_t *ent)
{
	gi.cprintf (ent, PRINT_HIGH, "Available REFEREE commands:\n\n");
	gi.cprintf (ent, PRINT_HIGH, "  r_kick <player_name>: Disconnects <player_name>\n");
	gi.cprintf (ent, PRINT_HIGH, "  r_mpause: Pause/Unpause a match in progress\n");
	gi.cprintf (ent, PRINT_HIGH, "  r_map <mapname>: Loads map <mapname>, if in the map queue\n");
	gi.cprintf (ent, PRINT_HIGH, "  r_timelimit <min>: Sets the timelimit to <min>\n");
	gi.cprintf (ent, PRINT_HIGH, "  r_fraglimit <frags>: Sets the fraglimit to <frags>\n");
	if (m_mode > 0 && sync_stat < 4)
	{
		gi.cprintf (ent, PRINT_HIGH, "  r_allready: Sets all active clients to \"ready\"\n");
		gi.cprintf (ent, PRINT_HIGH, "  r_allnotready: Set all active clients to \"notready\"\n");
	}
	if (m_mode > 0)
		gi.cprintf (ent, PRINT_HIGH, "  r_stopmatch: Stops a match in progress\n");
	gi.cprintf (ent, PRINT_HIGH, "  r_plist: Lists info on all active players.\n");
	gi.cprintf (ent, PRINT_HIGH, "  r_ban <name|id>: Bans a playername.\n");
	gi.cprintf (ent, PRINT_HIGH, "  r_banaddr <addr>: Bans an address range.\n");
	gi.cprintf (ent, PRINT_HIGH, "  r_unban <name>: Unbans a player name.\n");
	gi.cprintf (ent, PRINT_HIGH, "  r_unbanaddr <addr>: Unbans an address range.\n");
	gi.cprintf (ent, PRINT_HIGH, "  r_banlist: Lists all active bans.\n");
	gi.cprintf (ent, PRINT_HIGH, "  r_help: This help screen\n\n");
}

// gamex86.dll: 10022647..100227BA
// gamei386.so: 0005AC98..0005ADF7
void OSP_rkick_cmd (edict_t *ent)
{
	edict_t		*victim;

	if (gi.argc () == 1)
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: r_kick <player_name>\n\n");
		return;
	}

	victim = OSP_findPlayer (gi.args ());
	if (!victim)
	{
		gi.cprintf (ent, PRINT_HIGH, "Player \"%s\" is not logged on.\n",
					gi.args ());
		return;
	}

	if (victim == ent)
	{
		gi.cprintf (ent, PRINT_HIGH, "Sorry, you can't kick yourself!\n");
		return;
	}

	gi.bprintf (PRINT_CHAT, "%s has been kicked!\n",
				victim->client->pers.netname);
	if (server_log)
	{
		OSP_getPlayerAddr (victim);
		OSP_logAdminLog ("Referee_Kick: %s -> %s [%s]",
						 ent->client->pers.netname,
						 victim->client->pers.netname, victim->osp_e37c);
	}

	if (victim->flags & FL_OSP_NOCMD)
	{
		BotServerCommand ("sv", "removebot", victim->client->pers.netname, NULL);
		// Third site of the target's copy-paste of the rembots idiom
		// (`bots_votedin -= N; if (< 0) = 0;`) with the counter itself in place of N.
		bots_votedin -= bots_votedin;
		if (bots_votedin < 0)
			bots_votedin = 0;
		return;
	}

	OSP_startObserve (victim);
	gi.WriteByte (svc_disconnect);
	gi.unicast (victim, true);
	ClientDisconnect (victim);
}

// gamex86.dll: 100227BA..10022873
// gamei386.so: 0005ADF8..0005AED2
void OSP_rmpause_cmd (void)
{
	int			i;
	edict_t		*ent;

	if (server_log)
		OSP_logAdminLog ("Referee_Pause");

	if (!match_paused)
	{
		match_paused = 1;
		pause_time = 0;
		for (i = 1; i <= game.maxclients; i++)
		{
			ent = g_edicts + i;
			if (ent->inuse)
			{
				if (!ent->client)
					continue;
				gi.centerprintf (ent, "Match paused by referee.\n");
				gi.cprintf (ent, PRINT_CHAT, "Match paused by referee.\n");
			}
		}
	}
	else
		match_paused = 3;
}

// gamex86.dll: 10022873..10022945
// gamei386.so: 0005AED4..0005AFD1
void OSP_rmap_cmd (edict_t *ent)
{
	if (gi.argc () == 1)
		OSP_mapList (ent);

	if (gi.argc () != 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: r_map <mapname>\n\n");
		return;
	}

	if (server_log)
		OSP_logAdminLog ("Referee_Map: %s (%s)", ent->client->pers.netname,
						 gi.argv (1));

	if (OSP_mapExists (ent, gi.argv (1), true))
	{
		sl_SoftGameEnd (&gi, level);
		q2log_gameEnd ("referee map change", 0);
		manual_map = 1;
		EndDMLevel ();
	}
}

// gamex86.dll: 10022945..10022AC1
// gamei386.so: 0005AFD4..0005B1C3
void OSP_rtimelimit_cmd (edict_t *ent)
{
	char		num[32];
	int			setval;

	if (gi.argc () == 1)
	{
		gi.cprintf (ent, PRINT_HIGH, "Current match timelimit: %d\n\n",
					(int)timelimit->value);
		return;
	}

	if (gi.argc () != 2 || atoi (gi.argv (1)) < 0)
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: r_timelimit <time in minutes>\n\n");
		return;
	}

	setval = atoi (gi.argv (1));
	if (setval < 0)
		setval = 0;
	if (setval > (int)menu_maxtime->value)
		setval = (int)menu_maxtime->value;

	sprintf (num, "%d", setval);
	gi.cvar_set ("timelimit", num);
	strcpy (default_timelimit, num);
	OSP_setShowParams ();

	if (!atoi (num))
		gi.bprintf (PRINT_HIGH, "Match timelimit disabled.\n");
	else if (atoi (num) == 1)
		gi.bprintf (PRINT_HIGH, "Match timelimit changed to 1 minute.\n");
	else
		gi.bprintf (PRINT_HIGH, "Match timelimit changed to %s minutes.\n", num);

	if (server_log)
		OSP_logAdminLog ("Referee_Timelimit: %s -> %s",
						 ent->client->pers.netname, num);
}

// gamex86.dll: 10022AC1..10022C3D
// gamei386.so: 0005B1C4..0005B3B3
void OSP_rfraglimit_cmd (edict_t *ent)
{
	char		num[32];
	int			setval;

	if (gi.argc () == 1)
	{
		gi.cprintf (ent, PRINT_HIGH, "Current match fraglimit: %d\n\n",
					(int)fraglimit->value);
		return;
	}

	if (gi.argc () != 2 || atoi (gi.argv (1)) < 0)
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: r_fraglimit <frags>\n\n");
		return;
	}

	setval = atoi (gi.argv (1));
	if (setval < 0)
		setval = 0;
	if (setval > (int)menu_maxfrag->value)
		setval = (int)menu_maxfrag->value;

	sprintf (num, "%d", setval);
	gi.cvar_set ("fraglimit", num);
	strcpy (default_fraglimit, num);
	OSP_setShowParams ();

	if (!atoi (num))
		gi.bprintf (PRINT_HIGH, "Match fraglimit disabled.\n");
	else if (atoi (num) == 1)
		gi.bprintf (PRINT_HIGH, "Match fraglimit changed to 1 frag.\n");
	else
		gi.bprintf (PRINT_HIGH, "Match fraglimit changed to %s frags.\n", num);

	if (server_log)
		OSP_logAdminLog ("Referee_Fraglimit: %s -> %s",
						 ent->client->pers.netname, num);
}

// gamex86.dll: 10022C3D..10022DAF
// gamei386.so: 0005B3B4..0005B52B
void OSP_rstopmatch_cmd (edict_t *ent)
{
	int			i;

	if (sync_stat != 4)
	{
		if (ent)
			gi.cprintf (ent, PRINT_HIGH,
						"There is no match in progress to stop.\n");
		else
			gi.dprintf ("There is no match in progress to stop.\n");
		return;
	}

	if (m_mode == 1)
	{
		OSP_allnotready_svcmd (NULL);
		OSP_clearClients ();
	}
	else
	{
		for (i = 0; i < 2; i++)
		{
			teams[i].osp_m0f8 = 0;
			teams[i].osp_m0f4 = 0;
			teams[i].osp_m100 = 0;
			teams[i].osp_m0fc = 0;
			teams[i].osp_m104 = 0;
			teams[i].osp_m108 = 0;
			teams[i].osp_m124 = 0;
		}
	}

	if (ent)
		gi.bprintf (PRINT_CHAT, "Match terminated by referee!!\n");
	else
		gi.bprintf (PRINT_CHAT, "Match terminated by console!!\n");

	OSP_allnotready_svcmd (NULL);
	OSP_clearClients ();

	if (server_log)
	{
		if (ent)
			OSP_logAdminLog ("Referee_Stopmatch: %s",
							 ent->client->pers.netname);
		else
			OSP_logAdminLog ("Referee_Stopmatch: (in console)");
	}
}

// gamex86.dll: 10022DAF..10022DC0
// gamei386.so: 0005B52C..0005B549
void OSP_rbanlist_cmd (edict_t *ent)
{
	OSP_listbans (ent);
}

// gamex86.dll: 10022DC0..10023059
// gamei386.so: 0005B54C..0005B78C
void OSP_rban_cmd (edict_t *ent, char *who)
{
	char		bname[16];
	edict_t		*victim;
	int			ret;

	if (!who || !who[0])
	{
		if (gi.argc () < 2)
		{
			gi.cprintf (ent, PRINT_HIGH,
						"Usage: r_ban <player_name|player_id>\n");
			return;
		}
		strncpy (bname, gi.args (), 15);
	}
	else
		strncpy (bname, who, 15);
	victim = OSP_findPlayer (bname);

	if (!victim)
	{
		ret = OSP_addBan (bname, NULL);
		if (!ret)
			gi.cprintf (ent, PRINT_HIGH, "Player \"%s\" already in ban list!\n",
						bname);
		else if (ret == -1)
			gi.cprintf (ent, PRINT_HIGH,
						"Ban list full, player \"%s\" not added!\n", bname);
		else
			gi.cprintf (ent, PRINT_HIGH,
						"Player \"%s\" added to ban list (name only).\n", bname);
		if (server_log)
			OSP_logAdminLog ("Referee_Ban: %s -> %s (plain)",
							 ent->client->pers.netname, bname);
		return;
	}

	if (victim == ent)
	{
		gi.cprintf (ent, PRINT_HIGH, "Sorry, you can't ban yourself!\n");
		return;
	}

	if (victim->flags & FL_OSP_BOT)
	{
		gi.cprintf (ent, PRINT_HIGH, "Sorry, you can't ban bots!\n");
		return;
	}

	strncpy (bname, victim->client->pers.netname, 15);
	OSP_getPlayerAddr (victim);
	ret = OSP_addBan (bname, victim->osp_e37c);
	if (!ret)
	{
		gi.cprintf (ent, PRINT_HIGH, "Player \"%s\" already in ban list!\n",
					bname);
		return;
	}
	if (ret == -1)
	{
		gi.cprintf (ent, PRINT_HIGH, "Ban list full, player \"%s\" not added!\n",
					bname);
		return;
	}
	if (ret == -2)
	{
		gi.cprintf (ent, PRINT_HIGH,
					"Ban list full, player \"%s\" added, but not address!\n",
					bname);
		return;
	}

	gi.cprintf (ent, PRINT_HIGH, "Player \"%s\" added to ban list.\n", bname);
	if (server_log)
		OSP_logAdminLog ("Referee_Ban: %s -> %s [%s]",
						 ent->client->pers.netname, bname, victim->osp_e37c);
	gi.bprintf (PRINT_CHAT, "%s has been banned!\n", bname);
	victim->client->resp.osp_r07c[0] = 1;
	gi.WriteByte (svc_disconnect);
	gi.unicast (victim, true);
	ClientDisconnect (victim);
}

// gamex86.dll: 10023059..10023203
// gamei386.so: 0005B78C..0005B955
void OSP_rbanaddr_cmd (edict_t *ent)
{
	char		banip[16];
	edict_t		*e;
	int			i;
	char		*cliaddr;

	if (gi.argc () < 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: r_banaddr <address>\n");
		return;
	}

	strncpy (banip, gi.argv (1), 15);
	i = OSP_addBan (NULL, banip);
	if (!i)
	{
		gi.cprintf (ent, PRINT_HIGH, "Address \"%s\" already in ban list!\n",
					banip);
		return;
	}
	if (i == -1)
	{
		gi.cprintf (ent, PRINT_HIGH,
					"Ban list full, address \"%s\" not added!\n", banip);
		return;
	}

	gi.cprintf (ent, PRINT_HIGH, "Address \"%s\" added to ban list.\n", banip);

	for (i = 1; i <= game.maxclients; i++)
	{
		e = g_edicts + i;
		if (!e->inuse || !e->client || !e->client->pers.connected)
			continue;

		cliaddr = e->osp_e37c;

		if (strstr (cliaddr, banip) == cliaddr)
		{
			gi.bprintf (PRINT_CHAT, "%s has been banned!\n",
						e->client->pers.netname);
			e->client->resp.osp_r07c[0] = 1;
			gi.WriteByte (svc_disconnect);
			gi.unicast (e, true);
			ClientDisconnect (e);
		}
		if (server_log)
			OSP_logAdminLog ("Referee_BanAddress: %s -> %s",
							 ent->client->pers.netname, banip);
	}
}

// gamex86.dll: 10023203..100232B3
// gamei386.so: 0005B958..0005BA30
void OSP_runban_cmd (edict_t *ent)
{
	char		name[16];

	if (gi.argc () < 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: r_unban <player_name>\n");
		return;
	}

	strncpy (name, gi.args (), 15);
	if (OSP_removeBan (name, NULL))
	{
		gi.cprintf (ent, PRINT_HIGH, "Playername \"%s\" removed from ban list.\n",
					name);
		if (server_log)
			OSP_logAdminLog ("Referee_Unban: %s -> %s",
							 ent->client->pers.netname, gi.args ());
	}
	else
		gi.cprintf (ent, PRINT_HIGH, "Playername \"%s\" not found in ban list!\n",
					name);
}

// gamex86.dll: 100232B3..10023368
// gamei386.so: 0005BA30..0005BB0E
void OSP_runbanaddr_cmd (edict_t *ent)
{
	char		addr[16];

	if (gi.argc () < 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: r_unbanaddr <address>\n");
		return;
	}

	strncpy (addr, gi.argv (1), 15);
	if (OSP_removeBan (NULL, addr))
	{
		gi.cprintf (ent, PRINT_HIGH, "Address \"%s\" removed from ban list.\n",
					addr);
		if (server_log)
			OSP_logAdminLog ("Referee_UnbanAddress: %s -> %s",
							 ent->client->pers.netname, gi.args ());
	}
	else
		gi.cprintf (ent, PRINT_HIGH, "Address \"%s\" not found in ban list!\n",
					addr);
}

// gamex86.dll: 10023368..100233EC
// gamei386.so: 0005BB10..0005BB92
void OSP_allready_svcmd (void)
{
	int			i;
	edict_t		*ent;

	if (sync_stat == 4)
		return;

	for (i = 1; i <= game.maxclients; i++)
	{
		ent = g_edicts + i;
		if (ent->inuse && ent->client)
		{
			if (ent->client->resp.entered != ENTERED_ENTERED)
				continue;
			ent->client->resp.osp_r20c = 1;
		}
	}

	OSP_setShowParams ();
	OSP_CheckReady ();
}

// gamex86.dll: 100233EC..10023565
// gamei386.so: 0005BB94..0005BD2C
void OSP_allnotready_svcmd (edict_t *ent)
{
	edict_t		*e;
	int			i;

	for (i = 1; i <= game.maxclients; i++)
	{
		e = g_edicts + i;
		if (!e->inuse || !e->client)
			continue;

		if (m_mode < 2)
			e->client->ps.stats[20] = 0;
		e->client->ps.stats[17] = 0;
		e->client->resp.osp_r20c = 0;
		e->client->resp.score = 0;
		e->client->resp.osp_r014 = 0;
		e->client->resp.osp_r2c0 = 0;
		e->client->resp.osp_r028 = 0;
	}

	if (m_mode < 2)
		gi.configstring (CS_OSP_STATUS_DM, "  WARMUP");
	else
	{
		gi.configstring (CS_OSP_STATUS_A, "       WARMUP");
		gi.configstring (CS_OSP_STATUS_B, "       WARMUP");
		if (m_mode == 2)
		{
			gi.cvar_set ("Score_A", "WARMUP");
			gi.cvar_set ("Score_B", "WARMUP");
		}
	}

	if (ent)
		gi.bprintf (PRINT_HIGH, "All clients set to NOT ready!\n");

	sync_stat = 0;
	OSP_DoRankSort ();
	gi.cvar_set ("time_remaining", "WARMUP");
}

// gamex86.dll: 10023565..100235C0
// gamei386.so: 0005BD2C..0005BD97
void OSP_playerlist_svcmd (void)
{
	cvar_t		*pfile;

	pfile = gi.cvar ("player_file", "players.txt", 0);
	if (gi.argc () >= 3)
		OSP_loadPlayers (gi.argv (2));
	else
		OSP_loadPlayers (pfile->string);
}
