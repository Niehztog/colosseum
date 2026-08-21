// osp_teams.c -- <INVENTED FILENAME>. Team play: joining, leaving, the 1v1
// queue, per-team frag accounting and the team client commands.
//
// A client is sent its OWN team's configstring from the plain team name and
// the OTHER team's from the green one; the asymmetry is deliberate.

#include "g_local.h"
#include "bl_main.h"

int	overtime_timer;
team_t	teams[2];
int	frag_offset;


// gamex86.dll: 100373C0..10037447
// gamei386.so: 00063780..000637EF
int OSP_teamCount (int team)
{
	int			i;
	int			count;

	count = 0;
	for (i = 1; i <= game.maxclients; i++)
	{
		if (!g_edicts[i].inuse || !g_edicts[i].client ||
			g_edicts[i].client->resp.team != team)
			continue;

		count++;
	}
	return count;
}

// gamex86.dll: 10037447..100374ED
// gamei386.so: 000637F0..00063868
int OSP_teamReady (int team)
{
	int			i;
	int			count;

	count = 0;
	for (i = 1; i <= game.maxclients; i++)
	{
		if (!g_edicts[i].inuse || !g_edicts[i].client ||
			g_edicts[i].client->resp.team != team)
			continue;

		if (g_edicts[i].client->resp.osp_r20c)
			count++;
	}
	return count;
}

// Put a client on a team. `team` == 2 means "no team yet, pick one": the mod
// then tries the client's remembered default team first (OSP_defaultTeam), and
// failing that balances by head count, breaking ties towards team 0 and never
// picking a locked team.
// gamex86.dll: 100374ED..10037A9C
// gamei386.so: 00063868..00063F8D
qboolean OSP_addTeamMember (edict_t *ent, int requested_team)
{
	char		tmp[164];
	edict_t		*p;
	int			t;
	int			team;

	team = requested_team;
	if (requested_team == 2)
	{
		if (teams[0].osp_m0f4 && teams[1].osp_m0f4)
		{
			if (!(ent->flags & FL_OSP_NOCMD))
				gi.cprintf (ent, PRINT_HIGH, "Sorry, both teams are locked!\n");
			else
				BotDestroy (ent);
			return false;
		}

		if (OSP_teamCount (0) >= (int)team_maxplayers->value &&
			OSP_teamCount (1) >= (int)team_maxplayers->value)
		{
			if (!(ent->flags & FL_OSP_NOCMD))
				gi.cprintf (ent, PRINT_HIGH, "Sorry, both teams are full!\n");
			else
				BotDestroy (ent);
			return false;
		}

		if (m_mode == 3)
		{
			if (OSP_1v1Team (ent))
				return true;
			return false;
		}

		if (OSP_defaultTeam (ent))
			return true;

		if ((OSP_teamCount (0) > OSP_teamCount (1) || teams[0].osp_m0f4) &&
			!teams[1].osp_m0f4)
			team = 1;
		else
			team = 0;
	}

	ent->client->resp.team = team;
	q2log_teamJoin (ent);

	if (!(ent->flags & FL_OSP_NOCMD))
	{
		sprintf (tmp, "skin %s\n", teams[team].skin);
		gi.WriteByte (svc_stufftext);
		gi.WriteString (tmp);
		gi.unicast (ent, true);

		sprintf (tmp, "set default_teamname %s\n", teams[team].netname);
		strcpy (ent->osp_e3a0, teams[team].netname);
		gi.WriteByte (svc_stufftext);
		gi.WriteString (tmp);
		gi.unicast (ent, true);

		sprintf (tmp, "set default_teamskin %s\n", teams[team].skin);
		strcpy (ent->osp_e3b0, teams[team].skin);
		gi.WriteByte (svc_stufftext);
		gi.WriteString (tmp);
		gi.unicast (ent, true);
	}
	else
	{
		char		userinfo[512];

		strncpy (userinfo, ent->client->pers.userinfo, 511);
		userinfo[511] = 0;
		Info_SetValueForKey (userinfo, "skin", teams[team].skin);
		ClientUserinfoChanged (ent, userinfo);
	}

	sprintf (tmp, "%15s", teams[team].netname);
	OSP_clientConfigString (ent, 0x625 + team * 2, tmp);
	sprintf (tmp, "%15s", teams[1 - team].greenname);
	OSP_clientConfigString (ent, 0x625 + (1 - team) * 2, tmp);

	if (m_mode == 2)
		gi.bprintf (PRINT_HIGH, "%s joined team \"%s\"\n",
					ent->client->pers.greenname, teams[team].netname);

	if (!(ent->flags & FL_OSP_BOT))
	{
		if (m_mode == 2)
		{
			// First human on the team becomes captain.
			ent->client->resp.osp_r2c4 = 1;
			for (t = 1; t <= game.maxclients; t++)
			{
				p = g_edicts + t;
				if (!p->inuse || !p->client ||
					p->client->resp.entered != ENTERED_ENTERED ||
					p->client->resp.team != team || p == ent ||
					(p->flags & FL_OSP_BOT))
					continue;
				if (p->client->resp.osp_r2c4)
				{
					ent->client->resp.osp_r2c4 = 0;
					break;
				}
			}

			if (ent->client->resp.osp_r2c4)
			{
				gi.cprintf (ent, PRINT_CHAT, "*** You are team captain of \"%s\". ***\n",
							teams[team].greenname);
				if (ent->client->resp.osp_r07d[0])
					strcpy (teams[team].joincode, ent->client->resp.osp_r07d);
			}
		}
	}
	else if (m_mode == 2)
		ent->client->resp.osp_r2c4 = 0;

	if (m_mode == 2 && !(ent->flags & FL_OSP_NOCMD))
	{
		if (teams[team].joincode[0])
			gi.centerprintf (ent, "Team joincode is \"%s\"\n", teams[team].joincode);
		else
			gi.centerprintf (ent, "The team joincode has not been set.\n");
	}
	return true;
}

// The "I always play for <name>/<skin>" path. `defaultteam` stores a name and
// a skin on the edict; on connect this matches them against the two teams and,
// if the team it picks is still empty, renames/reskins that team to suit --
// swapping the two teams' names or skins over if the other one is in the way.
// gamex86.dll: 10037A9C..1003820B
// gamei386.so: 00063F90..000649C9
qboolean OSP_defaultTeam (edict_t *ent)
{
	char		msgbuf[64];
	int			team;
	int			i;
	edict_t		*p;
	int			k;

	team = 2;
	if (!ent->osp_e3a0[0])
		return false;

	for (k = 1; k >= 0; k--)
	{
		if (OSP_teamCount (k))
			team = k;
		if (!Q_stricmp (teams[k].skin, ent->osp_e3b0) ||
			!Q_stricmp (teams[k].netname, ent->osp_e3a0))
		{
			team = k;
			break;
		}
	}

	if (team == 2)
		return false;

	if (!OSP_teamCount (team))
	{
		if (Q_stricmp (teams[1 - team].netname, ent->osp_e3a0))
		{
			if (Q_stricmp (teams[team].netname, ent->osp_e3a0))
				q2log_teamRename (teams[team].netname, ent->osp_e3a0);
			strncpy (teams[team].netname, ent->osp_e3a0, 15);
			strncpy (teams[team].greenname, ent->osp_e3a0, 15);
			{

				for (i = 0; i < strlen (teams[team].greenname); i++)
					teams[team].greenname[i] += 128;
			}
			sprintf (msgbuf, "%15s", teams[team].greenname);
			gi.configstring (0x625 + team * 2, msgbuf);
		}
		else if (!OSP_teamCount (1 - team))
		{
			// The name we want is the OTHER team's and that team is empty, so
			// hand it our name and take theirs.
			strncpy (teams[1 - team].netname, teams[team].netname, 15);
			strncpy (teams[team].netname, ent->osp_e3a0, 15);
			strncpy (teams[team].greenname, ent->osp_e3a0, 15);
			{

				for (i = 0; i < strlen (teams[team].greenname); i++)
					teams[team].greenname[i] += 128;
			}
			sprintf (msgbuf, "%15s", teams[team].greenname);
			gi.configstring (0x625 + team * 2, msgbuf);
		}

		if (Q_stricmp (teams[1 - team].skin, ent->osp_e3b0))
			strncpy (teams[team].skin, ent->osp_e3b0, 128);
		else if (!OSP_teamCount (1 - team))
		{
			strncpy (teams[1 - team].skin, teams[team].skin, 128);
			strncpy (teams[team].skin, ent->osp_e3b0, 128);
		}
	}
	else if (OSP_teamCount (team) >= (int)team_maxplayers->value)
		return false;

	ent->client->resp.team = team;
	q2log_teamJoin (ent);

	if (!(ent->flags & FL_OSP_NOCMD))
	{
		sprintf (msgbuf, "skin %s\n", teams[team].skin);
		gi.WriteByte (svc_stufftext);
		gi.WriteString (msgbuf);
		gi.unicast (ent, true);

		sprintf (msgbuf, "%15s", teams[team].netname);
		OSP_clientConfigString (ent, 0x625 + team * 2, msgbuf);
		sprintf (msgbuf, "%15s", teams[1 - team].greenname);
		OSP_clientConfigString (ent, 0x625 + (1 - team) * 2, msgbuf);
	}
	else
	{
		char		userinfo[512];

		strncpy (userinfo, ent->client->pers.userinfo, 511);
		userinfo[511] = 0;
		Info_SetValueForKey (userinfo, "skin", teams[team].skin);
		ClientUserinfoChanged (ent, userinfo);
	}

	if (m_mode == 2)
		gi.bprintf (PRINT_HIGH, "%s joined team \"%s\"\n",
					ent->client->pers.greenname, teams[team].netname);

	if (!(ent->flags & FL_OSP_BOT))
	{
		ent->client->resp.osp_r2c4 = 1;
		{

			for (i = 1; i <= game.maxclients; i++)
			{
				p = g_edicts + i;
				if (!p->inuse || !p->client ||
					p->client->resp.entered != ENTERED_ENTERED ||
					p->client->resp.team != team || p == ent ||
					(p->flags & FL_OSP_BOT))
					continue;
				if (p->client->resp.osp_r2c4)
				{
					ent->client->resp.osp_r2c4 = 0;
					break;
				}
			}
		}

		if (ent->client->resp.osp_r2c4)
		{
			gi.cprintf (ent, PRINT_CHAT, "*** You are team captain of \"%s\". ***\n",
						teams[team].greenname);
			if (ent->client->resp.osp_r07d[0])
				strcpy (teams[team].joincode, ent->client->resp.osp_r07d);
		}
	}
	else
		ent->client->resp.osp_r2c4 = 0;

	if (m_mode == 2 && !(ent->flags & FL_OSP_NOCMD))
	{
		if (teams[team].joincode[0])
			gi.centerprintf (ent, "Team joincode is \"%s\"\n", teams[team].joincode);
		else
			gi.centerprintf (ent, "The team joincode has not been set.\n");
	}
	return true;
}

// 1v1 (m_mode 3): the "teams" are the two duellists, so the only choice is
// which of the two slots is free. The winner keeps their slot between rounds,
// which is why this renames the team to the player rather than the reverse.
// gamex86.dll: 1003820B..10038418
// gamei386.so: 000649CC..00064C01
qboolean OSP_1v1Team (edict_t *ent)
{
	char		tmp[64];
	int			t;
	int			team;

	team = 2;
	for (t = 1; t >= 0; t--)
		if (!OSP_teamCount (t))
			team = t;

	if (team == 2)
		return false;

	if (Q_stricmp (teams[1 - team].netname, ent->client->pers.netname))
	{
		if (strcmp (teams[team].netname, ent->client->pers.netname))
			q2log_teamRename (teams[team].netname, ent->client->pers.netname);
		strncpy (teams[team].netname, ent->client->pers.netname, 15);
		strncpy (teams[team].greenname, ent->client->pers.greenname, 15);
		sprintf (tmp, "%15s", teams[team].greenname);
		gi.configstring (0x625 + team * 2, tmp);
	}

	ent->client->resp.team = team;
	q2log_teamJoin (ent);

	if (!(ent->flags & FL_OSP_NOCMD))
	{
		sprintf (tmp, "%15s", teams[team].netname);
		OSP_clientConfigString (ent, 0x625 + team * 2, tmp);
		sprintf (tmp, "%15s", teams[1 - team].greenname);
		OSP_clientConfigString (ent, 0x625 + (1 - team) * 2, tmp);
	}
	return true;
}

// The 1v1 waiting line. p_order[] is one 112-byte symbol used as four things:
// [0..24] the queue of client numbers, [25] how many are in it, and [26]/[27]
// a "claim your slot by this framenum" deadline for the two players at the
// head of it. A slot whose deadline passes is dropped back into the queue.
// gamex86.dll: 10038418..1003864F
// gamei386.so: 00064C04..00064EA1
qboolean OSP_1v1AllowJoin (edict_t *ent)
{
	int			i;
	int			until;

	OSP_1v1QueueCheck ();

	if (p_order[25] < 2 || !(int)team_nextuptime->value)
		return true;

	if (p_order[27] > 0 && p_order[27] < level.framenum)
		OSP_1v1Remove (&g_edicts[p_order[1] + 1], 0);
	if (p_order[26] > 0 && p_order[26] < level.framenum)
		OSP_1v1Remove (&g_edicts[p_order[0] + 1], 0);

	if (ent - g_edicts - 1 == p_order[0])
	{
		p_order[26] = -1;
		return true;
	}
	if (ent - g_edicts - 1 == p_order[1])
	{
		p_order[27] = -1;
		return true;
	}

	for (i = 0; i < p_order[25]; i++)
		if (ent - g_edicts - 1 == p_order[i])
			break;

	gi.cprintf (ent, PRINT_CHAT, "*** It is not your turn! ***\n");
	gi.cprintf (ent, PRINT_HIGH, "%d players are ahead of you in line.\n", i);

	until = -1;
	if (!p_order[27])
		p_order[27] = until = level.framenum + (int)team_nextuptime->value * 10;
	else if (p_order[27] > 0)
		until = p_order[27];

	if (!p_order[26])
		p_order[26] = until = level.framenum + (int)team_nextuptime->value * 10;
	else if (p_order[26] > 0 && p_order[26] < until)
		until = p_order[26];

	if (until >= 0 && i == 2)
		gi.cprintf (ent, PRINT_HIGH,
					"Try again in %d seconds if they have not joined.\n",
					(until - level.framenum) / 10);
	return false;
}

// gamex86.dll: 1003864F..100386AC
// gamei386.so: 00064EA4..00064F41
void OSP_1v1Add (edict_t *ent)
{
	if (m_mode != 3 || p_order[25] >= 25 || !(int)team_nextuptime->value)
		return;

	p_order[p_order[25]] = ent - g_edicts - 1;
	p_order[25]++;
	OSP_1v1QueueCheck ();
}

// mode 1 drops the client out of the queue entirely; anything else moves them
// to the back of it. Only mode 0 also takes them off their team.
// gamex86.dll: 100386AC..100387B6
// gamei386.so: 00064F44..00065088
void OSP_1v1Remove (edict_t *ent, int mode)
{
	int			i;
	int			j;

	if (!(int)team_nextuptime->value)
		return;

	for (i = 0; i < p_order[25]; i++)
	{
		if (p_order[i] == ent - g_edicts - 1)
		{
			if (!i || !(i - 1))
				p_order[26 + i] = 0;
			for (j = i; j < p_order[25] - 1; j++)
				p_order[j] = p_order[j + 1];
			break;
		}
	}

	if (mode == 1)
		p_order[25]--;
	else
		p_order[p_order[25] - 1] = ent - g_edicts - 1;

	if (!mode)
		ent->client->resp.team = 2;

	OSP_1v1QueueCheck ();
}

// Compact the queue: drop any entry that duplicates one ahead of it, and any
// whose client has gone away -- gone meaning not in the game, and either the
// edict is free and we are more than 30 seconds into the level, or the client
// slot is null, or the client is no longer connected.
// gamex86.dll: 100387B6..10038920
// gamei386.so: 00065088..000651A8
void OSP_1v1QueueCheck (void)
{
	int			i;
	int			j;
	int			k;

	if (!(int)team_nextuptime->value)
		return;

	for (i = 0; i < p_order[25]; i++)
	{
		for (j = 0; j < i; j++)
		{
			if (!(p_order[i] == p_order[j] ||
				  (g_edicts[p_order[i] + 1].client->resp.entered !=
				   ENTERED_ENTERED &&
				   ((!g_edicts[p_order[i] + 1].inuse &&
					 level.framenum - level_start >= 300) ||
					!g_edicts[p_order[i] + 1].client ||
					!g_edicts[p_order[i] + 1].client->pers.connected))))
				continue;

			for (k = i; k < p_order[25] - 1; k++)
				p_order[k] = p_order[k + 1];
			i--;
			p_order[25]--;
			break;
		}
	}
}

// Take a client off their team. `quiet` suppresses both the announcement and
// the scoreboard configstring update. The captaincy passes to the first other
// human still on the team, and an emptied team is unlocked.
// gamex86.dll: 10038920..10038B29
// gamei386.so: 000651A8..000653ED
void OSP_removeTeamMember (edict_t *ent, qboolean quiet)
{
	char		buf[20];
	edict_t		*other;
	int			i;
	int			tno;

	tno = ent->client->resp.team;
	if (tno == 2 || ent->client->resp.entered != ENTERED_ENTERED)
		return;

	if (m_mode == 2)
		gi.bprintf (PRINT_HIGH, "%s removed from team \"%s\"\n",
					ent->client->pers.greenname, teams[tno].netname);
	else if (!quiet)
		gi.bprintf (PRINT_HIGH,
					"%s has become a spectator and moves to the end of the line.\n",
					ent->client->pers.greenname);

	if (!quiet && !(ent->flags & FL_OSP_BOT))
	{
		sprintf (buf, "%15s", teams[tno].greenname);
		OSP_clientConfigString (ent, 0x625 + tno * 2, buf);
	}

	q2log_teamLeave (ent);

	if (ent->client->resp.osp_r2c4)
	{
		for (i = 1; i <= game.maxclients; i++)
		{
			other = g_edicts + i;

			if (!other->inuse || !other->client ||
				other->client->resp.entered != ENTERED_ENTERED ||
				other->client->resp.team != tno || other == ent ||
				(other->flags & FL_OSP_BOT))
				continue;

			other->client->resp.osp_r2c4 = 1;
			gi.cprintf (other, PRINT_CHAT,
						"*** You are now team captain of \"%s\". ***\n",
						teams[tno].greenname);
			break;
		}
	}

	ent->client->resp.osp_r2cc = tno;
	ent->client->resp.team = 2;
	ent->client->resp.osp_r2c4 = 0;

	if (!OSP_teamCount (tno))
		teams[tno].osp_m0f4 = 0;
}

// Rejoin the team the client was last on -- resp.osp_r2cc is where
// OSP_removeTeamMember parked it. resp.osp_r078 non-zero means the client got
// here from an invitation, which only changes which "team is full" text they
// get back.
// gamex86.dll: 10038B29..10038EA0
// gamei386.so: 000653F0..000657C0
qboolean OSP_readdTeamMember (edict_t *ent)
{
	char		tmp[64];
	edict_t		*p;
	int			t;
	int			team;

	team = ent->client->resp.osp_r2cc;
	if (team == 2)
		return false;

	if (OSP_teamCount (team) >= (int)team_maxplayers->value)
	{
		if (ent->client->resp.osp_r078)
		{
			ent->client->resp.osp_r078 = 0;
			gi.cprintf (ent, PRINT_HIGH, "Sorry, the inviting team is now full!\n");
		}
		else
			gi.cprintf (ent, PRINT_HIGH, "Sorry, your team is now full!\n");
		return false;
	}

	q2log_teamJoin (ent);
	ent->client->resp.team = ent->client->resp.osp_r2cc;

	if (!(ent->flags & FL_OSP_NOCMD))
	{
		sprintf (tmp, "skin %s\n", teams[team].skin);
		gi.WriteByte (svc_stufftext);
		gi.WriteString (tmp);
		gi.unicast (ent, true);

		sprintf (tmp, "%15s", teams[team].netname);
		OSP_clientConfigString (ent, 0x625 + team * 2, tmp);
		sprintf (tmp, "%15s", teams[1 - team].greenname);
		OSP_clientConfigString (ent, 0x625 + (1 - team) * 2, tmp);
	}
	else
	{
		char	userinfo[512];

		strncpy (userinfo, ent->client->pers.userinfo, 511);
		userinfo[511] = 0;
		Info_SetValueForKey (userinfo, "skin", teams[team].skin);
		ClientUserinfoChanged (ent, userinfo);
	}

	gi.bprintf (PRINT_HIGH, "%s rejoined team \"%s\"\n",
				ent->client->pers.greenname, teams[team].netname);

	if (!(ent->flags & FL_OSP_BOT))
	{
		ent->client->resp.osp_r2c4 = 1;
		for (t = 1; t <= game.maxclients; t++)
		{
			p = g_edicts + t;
			if (!p->inuse || !p->client ||
				p->client->resp.entered != ENTERED_ENTERED ||
				p->client->resp.team != team || p == ent ||
				(p->flags & FL_OSP_BOT))
				continue;
			if (p->client->resp.osp_r2c4)
			{
				ent->client->resp.osp_r2c4 = 0;
				break;
			}
		}

		if (ent->client->resp.osp_r2c4)
		{
			gi.cprintf (ent, PRINT_CHAT, "*** You are team captain of \"%s\". ***\n",
						teams[team].greenname);
			if (ent->client->resp.osp_r07d[0])
				strcpy (teams[team].joincode, ent->client->resp.osp_r07d);
		}
	}
	else
		ent->client->resp.osp_r2c4 = 0;
	return true;
}

// The two team-score status bar cells are configstrings 0x626 and 0x628; a
// client sees its OWN as "(own frags) team [/fraglimit]" and the other team's
// as a bare count. The three functions below are the same formatting for one
// client, for a whole team, and for an observer.
// gamex86.dll: 10038EA0..1003904A
// gamei386.so: 000657C0..000659B8
void OSP_initTeamFrags (edict_t *ent)
{
	char		buf[16];
	char		tmp[16];
	int			teamidx;

	teamidx = ent->client->resp.team;
	if (!(ent->flags & FL_OSP_NOCMD))
	{
		if (!(int)fraglimit->value)
		{
			sprintf (tmp, "(%i) %i", ent->client->resp.score, teams[teamidx].osp_m0f8);
			sprintf (buf, "%13s", tmp);
		}
		else
		{
			sprintf (tmp, "(%i) %i/%i", ent->client->resp.score, teams[teamidx].osp_m0f8,
					 (int)fraglimit->value);
			sprintf (buf, "%13s", tmp);
		}
		OSP_clientConfigString (ent, 0x626 + teamidx * 2, buf);

		if (ent->client->resp.osp_r210)
		{
			if (!(int)fraglimit->value)
				sprintf (buf, "%13i", teams[1 - teamidx].osp_m0f8);
			else
			{
				sprintf (tmp, "%i/%i", teams[1 - teamidx].osp_m0f8, (int)fraglimit->value);
				sprintf (buf, "%13s", tmp);
			}
			OSP_clientConfigString (ent, 0x626 + (1 - teamidx) * 2, buf);
		}
	}
}

// gamex86.dll: 1003904A..10039198
// gamei386.so: 000659B8..00065B4C
void OSP_playerTeamFrags (edict_t *ent)
{
	char		buf[16];
	char		tmp[16];
	edict_t		*other;
	int			i;
	int			teamidx;

	teamidx = ent->client->resp.team;
	for (i = 1; i <= game.maxclients; i++)
	{
		other = g_edicts + i;
		if (!other->inuse || !other->client || (other->flags & FL_OSP_BOT) ||
			other->client->resp.team != teamidx)
			continue;

		if (!(int)fraglimit->value)
		{
			sprintf (tmp, "(%i) %i", other->client->resp.score,
					 teams[teamidx].osp_m0f8);
			sprintf (buf, "%13s", tmp);
		}
		else
		{
			sprintf (tmp, "(%i) %i/%i", other->client->resp.score,
					 teams[teamidx].osp_m0f8, (int)fraglimit->value);
			sprintf (buf, "%13s", tmp);
		}
		OSP_clientConfigString (other, 0x626 + teamidx * 2, buf);
	}
}

// gamex86.dll: 10039198..10039284
// gamei386.so: 00065B4C..00065C65
void OSP_observerTeamFrags (edict_t *ent)
{
	char		num[32];
	char		msg[32];
	int			n;

	if (sync_stat > 2 && m_mode == 2)
	{
		for (n = 0; n < 2; n++)
		{
			if (!(int)fraglimit->value)
				sprintf (num, "%13i", teams[n].osp_m0f8);
			else
			{
				sprintf (msg, "%i/%i", teams[n].osp_m0f8, (int)fraglimit->value);
				sprintf (num, "%13s", msg);
			}
			if (!(ent->flags & FL_OSP_NOCMD))
				OSP_clientConfigString (ent, 0x626 + n * 2, num);
		}
	}
}

// Push a changed team score out. teams[].osp_m110/osp_m118 cache what was last
// sent so an unchanged score costs nothing. In team mode each client is sent
// only the OTHER team's cell here (its own comes from OSP_playerTeamFrags) and
// the score also goes into the Score_A/Score_B cvars for the server browser;
// otherwise it is one broadcast configstring.
// gamex86.dll: 10039284..1003950F
// gamei386.so: 00065C68..00065F67
void OSP_updateTeamFrags (void)
{
	char		buf[32];
	char		tmp[32];
	edict_t		*other;
	int			i;
	int			j;

	for (i = 0; i < 2; i++)
	{
		if (sync_stat > 2)
		{
			if (teams[i].osp_m110 != teams[i].osp_m0f8 ||
				teams[i].osp_m118 != (int)fraglimit->value)
			{
				if (!(int)fraglimit->value)
					sprintf (buf, "%13i", teams[i].osp_m0f8);
				else
				{
					sprintf (tmp, "%i/%i", teams[i].osp_m0f8, (int)fraglimit->value);
					sprintf (buf, "%13s", tmp);
				}

				if (m_mode == 2)
				{
					for (j = 1; j <= game.maxclients; j++)
					{
						other = g_edicts + j;
						if (!other->inuse || !other->client ||
							other->client->resp.team == i ||
							(other->flags & FL_OSP_NOCMD))
							continue;
						OSP_clientConfigString (other, 0x626 + i * 2, buf);
					}

					if (!(int)fraglimit->value)
						sprintf (buf, "%i-%s", teams[i].osp_m0f8, teams[i].netname);
					else
						sprintf (buf, "%i/%i-%s", teams[i].osp_m0f8,
								 (int)fraglimit->value, teams[i].netname);

					if (!i)
						gi.cvar_set ("Score_A", buf);
					else
						gi.cvar_set ("Score_B", buf);
				}
				else
					gi.configstring (0x626 + i * 2, buf);

				teams[i].osp_m110 = teams[i].osp_m0f8;
				teams[i].osp_m118 = (int)fraglimit->value;
			}
		}
	}
}

// gamex86.dll: 1003950F..10039563
// gamei386.so: 00065F68..00065FD9
void OSP_defaultteam_cmd (edict_t *ent)
{
	if (gi.argc () != 3)
		return;

	strncpy (ent->osp_e3a0, gi.argv (1), 15);
	strncpy (ent->osp_e3b0, gi.argv (2), 128);
}

// gamex86.dll: 10039563..10039598
// gamei386.so: 00065FDC..0006602E
void OSP_defaultjoincode_cmd (edict_t *ent)
{
	if (gi.argc () != 2)
		return;
	strncpy (ent->client->resp.osp_r07d, gi.argv (1), 15);
}

// `joincode` with no argument, or from a non-captain, prints the code; from a
// captain in the game it sets it; from someone not in the game it is the way
// in -- the code picks the team and hands off to OSP_teamjoin_cmd.
// gamex86.dll: 10039598..100397CE
// gamei386.so: 00066030..000662B5
void OSP_joincode_cmd (edict_t *ent)
{
	edict_t		*p;
	int			t;
	int			teamidx;

	teamidx = ent->client->resp.team;
	if (m_mode != 2 || level.intermissiontime)
		return;

	if (ent->client->resp.entered == ENTERED_ENTERED)
	{
		if (!ent->client->resp.osp_r2c4 || gi.argc () == 1)
		{
			if (teams[teamidx].joincode[0])
				gi.cprintf (ent, PRINT_HIGH, "You're team's joincode is \"%s\"\n",
							teams[teamidx].joincode);
			else
				gi.cprintf (ent, PRINT_HIGH, "No joincode set for your team.\n");
			return;
		}

		strncpy (teams[teamidx].joincode, gi.argv (1), 15);
		for (t = 1; t < game.maxclients; t++)
		{
			p = g_edicts + t;
			if (!p->inuse || !p->client ||
				p->client->resp.team != teamidx)
				continue;

			gi.centerprintf (p, "Team joincode is now \"%s\".\n",
							 gi.argv (1));
		}
		return;
	}

	if (gi.argc () == 1 || !gi.argv (1))
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: joincode <team_joincode_string>\n");
		return;
	}

	if (teams[0].joincode[0] && !Q_stricmp (gi.argv (1), teams[0].joincode))
	{
		ent->client->resp.osp_r078 = 1;
		OSP_teamjoin_cmd (ent, teams[0].netname);
	}
	else if (teams[1].joincode[0] && !Q_stricmp (gi.argv (1), teams[1].joincode))
	{
		ent->client->resp.osp_r078 = 2;
		OSP_teamjoin_cmd (ent, teams[1].netname);
	}
	else
		gi.cprintf (ent, PRINT_HIGH, "Illegal joincode.\n");
}

// `teamname <words>` -- warmup only. The argument is squeezed to at most 15
// non-space characters before it is accepted, so "Red Team" becomes "RedTeam".
// gamex86.dll: 100397CE..10039BE4
// gamei386.so: 000662B8..0006670F
void OSP_teamname_cmd (edict_t *ent)
{
	char		buf[128];
	char		pname[64];
	char		cmd[64];
	edict_t		*player;
	int			i;
	int			j;
	int			tnum;

	tnum = ent->client->resp.team;
	if (tnum == 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "You have not joined any team yet.\n");
		return;
	}

	if (gi.argc () == 1)
	{
		gi.cprintf (ent, PRINT_HIGH, "Current teamname: \"%s\"\n",
					teams[tnum].netname);
		return;
	}

	if (sync_stat > 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "Cannot change team's name during match!\n");
		return;
	}

	strncpy (buf, gi.args (), 30);
	buf[30] = 0;

	for (i = 0, j = 0; i < strlen (buf) && j < 15; i++)
	{
		if (buf[i] == ' ')
			continue;
		pname[j++] = buf[i];
	}
	pname[j] = 0;

	if (!Q_stricmp (pname, teams[1 - tnum].netname))
	{
		gi.cprintf (ent, PRINT_HIGH, "Sorry, cannot use same name for both teams.\n");
		return;
	}

	gi.bprintf (PRINT_HIGH, "Team \"%s\" renamed to \"%s\"\n",
				teams[tnum].netname, pname);
	q2log_teamRename (teams[tnum].netname, pname);
	strcpy (teams[tnum].netname, pname);
	strcpy (teams[tnum].greenname, pname);
	for (i = 0; i < strlen (teams[tnum].greenname); i++)
		teams[tnum].greenname[i] += 128;

	sprintf (buf, "%15s", teams[tnum].greenname);
	gi.configstring (0x625 + tnum * 2, buf);
	sprintf (cmd, "set default_teamname \"%s\"\n", pname);

	for (i = 1; i <= game.maxclients; i++)
	{
		player = g_edicts + i;
		if (!player->inuse || !player->client || (player->flags & FL_OSP_NOCMD))
			continue;
		if (player->client->resp.team == tnum)
		{
			sprintf (buf, "%15s", teams[tnum].netname);
			OSP_clientConfigString (player, 0x625 + tnum * 2, buf);
			gi.WriteByte (svc_stufftext);
			gi.WriteString (cmd);
			gi.unicast (player, true);
		}
	}

	if (m_mode == 2)
	{
		gi.cvar_set ("Score_A", "WARMUP");
		gi.cvar_set ("Score_B", "WARMUP");
	}
	OSP_setShowParams ();
}

// `teamskin <skin>` -- warmup only, and only when the server has not set
// team_lockskin.  Two faults faithfully reproduced from the real image: the
// bot arm rewrites the CALLER's userinfo rather than the client it is looping
// over, and the skin it installs is indexed by the CLIENT loop counter
// (`teams[i]`) rather than by `team`, which walks off the end of teams[] for
// every client past the second.
// gamex86.dll: 10039BE4..10039E6B
// gamei386.so: 00066710..00066A3F
void OSP_teamskin_cmd (edict_t *ent)
{
	char		stuff[256];
	edict_t		*p;
	int			t;
	int			teamidx;

	teamidx = ent->client->resp.team;
	if (ent->client->resp.team == 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "You have not joined any team yet.\n");
		return;
	}

	if (gi.argc () == 1)
	{
		gi.cprintf (ent, PRINT_HIGH, "Current teamskin: \"%s\"\n", teams[teamidx].skin);
		return;
	}

	if ((int)team_lockskin->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "Sorry, teamskins are locked by server.\n");
		return;
	}

	if (sync_stat > 0)
	{
		gi.cprintf (ent, PRINT_HIGH, "Cannot change team's skin after warmup!\n");
		return;
	}

	if (!Q_stricmp (gi.argv (1), teams[1 - teamidx].skin))
	{
		gi.cprintf (ent, PRINT_HIGH, "Sorry, cannot use same skin for both teams.\n");
		return;
	}

	gi.bprintf (PRINT_HIGH, "Team %s skin changed to \"%s\"\n",
				teams[teamidx].greenname, gi.argv (1));
	strcpy (teams[teamidx].skin, gi.argv (1));
	sprintf (stuff, "skin %s; set default_teamskin %s\n", gi.argv (1), gi.argv (1));

	for (t = 1; t <= game.maxclients; t++)
	{
		p = g_edicts + t;
		if (!p->inuse || !p->client ||
			p->client->resp.team != teamidx)
			continue;

		{
			if (p->flags & FL_OSP_NOCMD)
			{
				char	userinfo[512];

				strncpy (userinfo, ent->client->pers.userinfo, 511);
				userinfo[511] = 0;
				Info_SetValueForKey (userinfo, "skin", teams[t].skin);
				ClientUserinfoChanged (ent, userinfo);
			}
			else
			{
				gi.WriteByte (svc_stufftext);
				gi.WriteString (stuff);
				gi.unicast (p, true);
			}
		}
	}
	OSP_setShowParams ();
}

// `team [<name>]`. `name` non-NULL is the OSP_joincode_cmd entry, which has
// already picked the team; otherwise the name comes from the command line.
// resp.osp_r078 is the pending invitation: 1 or 2 meaning "invited to team
// 0 or 1", and it is what lets a player past a locked or full team.
// gamex86.dll: 10039E6B..1003A309
// gamei386.so: 00066A40..00066FB4
void OSP_teamjoin_cmd (edict_t *ent, char *name)
{
	char		teamname[32];
	int			i;
	int			invited;

	invited = ent->client->resp.osp_r078;

	if (m_mode == 3 && ent->client->resp.entered != ENTERED_ENTERED)
	{
		if (!OSP_1v1AllowJoin (ent))
			return;
	}

	if (gi.argc () == 1)
	{
		if (ent->client->resp.team == 2)
			gi.cprintf (ent, PRINT_HIGH, "You aren't currently on any team.\n");
		else
			gi.cprintf (ent, PRINT_HIGH, "You are on team \"%s\"\n",
						teams[ent->client->resp.team].netname);
		return;
	}

	if (name)
		strcpy (teamname, name);
	else
		strncpy (teamname, gi.args (), 15);

	if (who_paused == -2)
	{
		gi.cprintf (ent, PRINT_HIGH, "Sorry, cannot join on a forced pause.\n");
		return;
	}

	for (i = 0; i < 2; i++)
	{
		if (!Q_stricmp (teamname, teams[i].netname))
		{
			if (!((OSP_teamCount (i) >= (int)team_maxplayers->value && !invited &&
				   (m_mode != 2 ||
					((int)match_latejoin->value <= 2 &&
					 (sync_stat <= 2 ||
					  (int)match_latejoin->value != 2 ||
					  OSP_teamCount (i) >= (int)team_maxplayers->value)))) ||
				  (teams[i].osp_m0f4 && !invited)))
			{
				if (invited)
				{
					if (i != invited - 1 &&
						OSP_teamCount (i) >= (int)team_maxplayers->value)
					{
						gi.cprintf (ent, PRINT_HIGH,
									"You have been invited to join only team %s\n",
									teams[invited - 1].greenname);
						return;
					}
					ent->client->resp.osp_r078 = 0;
				}

				if (ent->client->resp.team != 2)
				{
					ent->client->pers.score = 0;
					ent->client->resp.osp_r0a0 = 0;
				}
				ent->client->resp.osp_r20c = 0;
				OSP_addTeamMember (ent, i);

				if (sync_stat < 4 && ent->client->resp.entered == ENTERED_ENTERED &&
					!(ent->flags & FL_OSP_BOT))
					OSP_notready_cmd (ent, true);

				if (ent->client->resp.entered != ENTERED_ENTERED)
				{
					active_clients++;
					ent->client->chase_target = NULL;
					ent->client->resp.entered = ENTERED_ENTERED;
					ent->client->resp.osp_r240 = 0;
					ent->client->osp_t040 = 0;
					ent->client->osp_t03c = NULL;
					if (!ent->client->resp.osp_r030)
					{
						ent->client->resp.osp_r030 = 1;
						ent->client->resp.enterframe = level.framenum;
					}
					else
						ent->client->resp.enterframe =
							level.framenum - ent->client->resp.osp_r2d4;
					ent->client->resp.score = ent->client->resp.osp_r248;
					ent->client->resp.osp_r0a0--;
					ent->client->resp.osp_r09c--;
					EntityListAdd (ent);
					q2log_playerEntered (ent);
				}

				if (sync_stat > 2)
					OSP_initTeamFrags (ent);
				OSP_setShowParams ();
				return;
			}

			if (teams[i].osp_m0f4 && !invited)
				gi.cprintf (ent, PRINT_HIGH, "\"%s\" is locked.\n", teams[i].netname);
			else
				gi.cprintf (ent, PRINT_HIGH, "\"%s\" is full.\n", teams[i].netname);
			return;
		}
	}

	gi.cprintf (ent, PRINT_HIGH, "Unknown team \"%s\"\n", teamname);
}

// gamex86.dll: 1003A309..1003A4EC
// gamei386.so: 00066FB4..000671F9
void OSP_switchteam_cmd (edict_t *ent)
{
	int			team;

	team = ent->client->resp.team;
	if (team == 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "You have not joined any team yet.\n");
		return;
	}

	if (who_paused == -2)
	{
		gi.cprintf (ent, PRINT_HIGH,
					"Sorry, cannot switch teams during a forced pause.\n");
		return;
	}

	if (OSP_teamCount (1 - team) < (int)team_maxplayers->value)
	{
		if (sync_stat < 4)
		{
			gi.cprintf (ent, PRINT_HIGH, "Sorry, \"%s\" is full.\n",
						teams[1 - team].netname);
			return;
		}

		if (teams[1 - team].osp_m0f4 && !ent->client->resp.osp_r078)
		{
			gi.cprintf (ent, PRINT_HIGH, "Sorry, \"%s\" is locked.\n",
						teams[1 - team].netname);
			return;
		}

		if (!ent->client->resp.osp_r078 && (int)match_latejoin->value < 2)
		{
			gi.cprintf (ent, PRINT_HIGH,
						"You need to be invited to switch teams.\n");
			return;
		}

		OSP_removeTeamMember (ent, false);
		ent->client->resp.osp_r2cc = 1 - team;
		OSP_readdTeamMember (ent);
		ent->client->resp.osp_r2c4 = 0;
		ent->client->resp.osp_r20c = 0;
		ent->client->pers.score = 0;

		if (sync_stat < 4)
			OSP_notready_cmd (ent, true);
		OSP_initTeamFrags (ent);
		OSP_setShowParams ();
		return;
	}

	gi.cprintf (ent, PRINT_HIGH, "Sorry, the other team is full.\n");
}

// `invite <player>` -- a captain's way past a locked or full team. The
// invitation is stored on the TARGET as resp.osp_r078 = team + 1 (so that 0
// still means "no invitation") plus resp.osp_r2cc as the team to re-add to.
// gamex86.dll: 1003A4EC..1003A71B
// gamei386.so: 000671FC..0006748F
void OSP_teaminvite_cmd (edict_t *ent)
{
	edict_t		*target;

	if (ent->client->resp.entered != ENTERED_ENTERED)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must be in the game to invite others!\n");
		return;
	}
	if (ent->client->resp.team == 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must be on a team to invite others!\n");
		return;
	}
	if (!ent->client->resp.osp_r2c4)
	{
		gi.cprintf (ent, PRINT_HIGH, "Only captains can invite others!\n");
		return;
	}

	target = OSP_findPlayer (gi.args ());
	if (!target)
	{
		gi.cprintf (ent, PRINT_HIGH, "Player \"%s\" is not logged on.\n", gi.args ());
		return;
	}
	if (target == ent)
	{
		gi.cprintf (ent, PRINT_HIGH, "You can't invite youself!\n");
		return;
	}
	if (target->client->resp.team == ent->client->resp.team)
	{
		gi.cprintf (ent, PRINT_HIGH, "\"%s\" is already on your team!\n",
					target->client->pers.netname);
		return;
	}

	{
	if (OSP_teamCount (ent->client->resp.team) >=
		(int)team_maxplayers->value)
	{
		gi.cprintf (ent, PRINT_HIGH,
					"Sorry, your team is already full (max %d players).\n",
					(int)team_maxplayers->value);
		return;
	}
	if (target->client->resp.osp_r078)
	{
		gi.cprintf (ent, PRINT_HIGH, "\"%s\" has already been invited.\n");
		return;
	}

	target->client->resp.osp_r030 = 1;
	target->client->resp.osp_r078 = ent->client->resp.team + 1;
	target->client->resp.osp_r2cc = ent->client->resp.team;

	gi.cprintf (target, PRINT_HIGH, "You have been invited to join team %s\n",
				teams[ent->client->resp.team].greenname);
	gi.cprintf (ent, PRINT_HIGH, "%s has been sent a \"join\" invitation.\n",
				target->client->pers.greenname);
	OSP_inviteMenu (target);
	}
}

// lock/unlock/readyteam/notreadyteam share one shape: a captain acts on their
// own team, a referee names the team on the command line.  The real image
// prints "Ref: Usage: unlockteam <teamname>" from BOTH lock and unlock -- the
// mod's own copy-paste, reproduced here.
// gamex86.dll: 1003A71B..1003A856
// gamei386.so: 00067490..000675EA
void OSP_lockteam_cmd (edict_t *ent)
{
	int			team;

	team = ent->client->resp.team;

	if (!ent->osp_e39c && !ent->client->resp.osp_r2c4)
	{
		gi.cprintf (ent, PRINT_HIGH,
					"Only captains or referees can lock a team.\n");
		return;
	}

	if (ent->osp_e39c)
	{
		if (ent->client->resp.entered != ENTERED_ENTERED && gi.argc () == 1)
		{
			gi.cprintf (ent, PRINT_HIGH, "Ref: Usage: unlockteam <teamname>\n");
			return;
		}

		if (gi.argc () > 1)
		{
			if (!Q_stricmp (gi.args (), teams[0].netname))
				team = 0;
			else if (!Q_stricmp (gi.args (), teams[1].netname))
				team = 1;
			else
			{
				gi.cprintf (ent, PRINT_HIGH,
							"Ref (lockteam): unknown team \"%s\"\n", gi.args ());
				return;
			}
		}
	}

	if (team == 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "You have not joined any team yet.\n");
		return;
	}

	teams[team].osp_m0f4 = 1;
	gi.cprintf (ent, PRINT_HIGH,
				"Team locked.  Use \"invite\" to allow others to join.\n");
}

// gamex86.dll: 1003A856..1003A991
// gamei386.so: 000675EC..00067746
void OSP_unlockteam_cmd (edict_t *ent)
{
	int			team;

	team = ent->client->resp.team;

	if (!ent->osp_e39c && !ent->client->resp.osp_r2c4)
	{
		gi.cprintf (ent, PRINT_HIGH,
					"Only captains or referees can unlock a team.\n");
		return;
	}

	if (ent->osp_e39c)
	{
		if (ent->client->resp.entered != ENTERED_ENTERED && gi.argc () == 1)
		{
			gi.cprintf (ent, PRINT_HIGH, "Ref: Usage: unlockteam <teamname>\n");
			return;
		}

		if (gi.argc () > 1)
		{
			if (!Q_stricmp (gi.args (), teams[0].netname))
				team = 0;
			else if (!Q_stricmp (gi.args (), teams[1].netname))
				team = 1;
			else
			{
				gi.cprintf (ent, PRINT_HIGH,
							"Ref (unlockteam): unknown team \"%s\"\n", gi.args ());
				return;
			}
		}
	}

	if (team == 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "You have not joined any team yet.\n");
		return;
	}

	teams[team].osp_m0f4 = 0;
	gi.cprintf (ent, PRINT_HIGH, "Team unlocked.  Anybody can now join.\n");
}

// gamex86.dll: 1003A991..1003AB8C
// gamei386.so: 00067748..00067962
void OSP_readyteam_cmd (edict_t *ent)
{
	edict_t		*p;
	int			t;
	int			teamidx;

	teamidx = ent->client->resp.team;
	// The second disjunct re-tests osp_e39c, which the first has already
	// settled -- another of the target's duplicate-condition bugs.
	if (!ent->osp_e39c ||
		(ent->osp_e39c && !ent->client->resp.osp_r2c4 &&
		 ent->client->resp.entered == ENTERED_ENTERED && gi.argc () == 1))
	{
		if (teamidx == 2)
		{
			gi.cprintf (ent, PRINT_HIGH, "You have not joined any team yet.\n");
			return;
		}
		if (!ent->client->resp.osp_r2c4 && !ent->osp_e39c)
		{
			gi.cprintf (ent, PRINT_HIGH,
						"Only team captain can \"ready\" entire team.\n");
			return;
		}
	}
	else
	{
		if (gi.argc () == 1)
		{
			gi.cprintf (ent, PRINT_HIGH, "Ref: Usage: readyteam <teamname>\n");
			return;
		}
		if (!Q_stricmp (gi.args (), teams[0].netname))
			teamidx = 0;
		else if (!Q_stricmp (gi.args (), teams[1].netname))
			teamidx = 1;
		else
		{
			gi.cprintf (ent, PRINT_HIGH,
						"Ref (readyteam): unknown team \"%s\"\n", gi.args ());
			return;
		}
	}

	if (sync_stat >= 4)
		return;

	for (t = 1; t <= game.maxclients; t++)
	{
		p = g_edicts + t;
		if (!p->inuse || !p->client ||
			p->client->resp.team != teamidx ||
			p->client->resp.entered != ENTERED_ENTERED ||
			p->client->resp.osp_r20c)
			continue;

		OSP_ready_cmd (p, true);
		if (sync_stat)
			break;
	}

	gi.bprintf (PRINT_HIGH, "Team \"%s\" is ready!\n", teams[teamidx].greenname);
}

// gamex86.dll: 1003AB8C..1003AD79
// gamei386.so: 00067964..00067B73
void OSP_notreadyteam_cmd (edict_t *ent)
{
	edict_t		*p;
	int			t;
	int			teamidx;

	teamidx = ent->client->resp.team;
	// The second disjunct re-tests osp_e39c, which the first has already
	// settled -- another of the target's duplicate-condition bugs.
	if (!ent->osp_e39c ||
		(ent->osp_e39c && !ent->client->resp.osp_r2c4 &&
		 ent->client->resp.entered == ENTERED_ENTERED && gi.argc () == 1))
	{
		if (teamidx == 2)
		{
			gi.cprintf (ent, PRINT_HIGH, "You have not joined any team yet.\n");
			return;
		}
		if (!ent->client->resp.osp_r2c4 && !ent->osp_e39c)
		{
			gi.cprintf (ent, PRINT_HIGH,
						"Only team captain can \"notready\" entire team.\n");
			return;
		}
	}
	else
	{
		if (gi.argc () == 1)
		{
			gi.cprintf (ent, PRINT_HIGH,
						"Ref (notreadyteam): Usage: notreadyteam <teamname>\n");
			return;
		}
		if (!Q_stricmp (gi.args (), teams[0].netname))
			teamidx = 0;
		else if (!Q_stricmp (gi.args (), teams[1].netname))
			teamidx = 1;
		else
		{
			gi.cprintf (ent, PRINT_HIGH, "Ref: unknown team \"%s\"\n", gi.args ());
			return;
		}
	}

	if (sync_stat >= 4)
		return;

	for (t = 1; t <= game.maxclients; t++)
	{
		p = g_edicts + t;
		if (!p->inuse || !p->client ||
			p->client->resp.team != teamidx ||
			p->client->resp.entered != ENTERED_ENTERED ||
			!p->client->resp.osp_r20c)
			continue;

		OSP_notready_cmd (p, true);
	}

	gi.bprintf (PRINT_HIGH, "Team \"%s\" is NOT ready!\n", teams[teamidx].greenname);
}

// `captain` with no argument reports the captain; a captain naming a player
// hands the job over; a referee names a team, and with two arguments names a
// team and a player. resp.osp_r2c4 is the captain flag throughout.
// gamex86.dll: 1003AD79..1003B2FC
// gamei386.so: 00067B74..00068120
void OSP_captain_cmd (edict_t *ent)
{
	edict_t		*other;
	int			i;
	int			prevcap;
	int			tnum;

	tnum = ent->client->resp.team;
	if (ent->osp_e39c)
	{
		if (ent->client->resp.entered != ENTERED_ENTERED && gi.argc () == 1)
		{
			gi.cprintf (ent, PRINT_HIGH, "Ref: Usage: captain <teamname>\n");
			return;
		}
		if (gi.argc () > 1 && !ent->client->resp.osp_r2c4)
		{
			if (!Q_stricmp (gi.argv (1), teams[0].netname))
				tnum = 0;
			else if (!Q_stricmp (gi.argv (1), teams[1].netname))
				tnum = 1;
			else
			{
				gi.cprintf (ent, PRINT_HIGH,
							"Ref (captain): unknown team \"%s\"\n", gi.argv (1));
				return;
			}
		}
	}

	if (tnum == 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "You have not joined any team yet.\n");
		return;
	}

	// Nobody is asking to change anything -- just report.
	if ((!ent->client->resp.osp_r2c4 && !ent->osp_e39c) || gi.argc () == 1 ||
		(!ent->client->resp.osp_r2c4 && ent->osp_e39c && gi.argc () == 2))
	{
		for (i = 1; i <= game.maxclients; i++)
		{
			other = g_edicts + i;
			if (!other->inuse || !other->client ||
				other->client->resp.team != tnum ||
				other->client->resp.entered != ENTERED_ENTERED ||
				!other->client->resp.osp_r2c4)
				continue;

			gi.cprintf (ent, PRINT_HIGH, "Current team captain is \"%s\"\n",
						other->client->pers.netname);
			return;
		}
		gi.cprintf (ent, PRINT_HIGH, "Currently, there is no team captain.\n");
		return;
	}

	// A captain handing the job over names the new captain in gi.args().
	if (ent->client->resp.osp_r2c4 && gi.argc () > 1)
	{
		for (i = 1; i <= game.maxclients; i++)
		{
			other = g_edicts + i;
			if (!other->inuse || !other->client ||
				other->client->resp.team != tnum ||
				other->client->resp.entered != ENTERED_ENTERED ||
				Q_stricmp (gi.args (), other->client->pers.netname))
				continue;

			gi.cprintf (ent, PRINT_HIGH, "Team captain is now \"%s\"\n",
						other->client->pers.netname);
			gi.cprintf (other, PRINT_HIGH, "You are now team captain.\n");
			ent->client->resp.osp_r2c4 = 0;
			other->client->resp.osp_r2c4 = 1;
			return;
		}
		gi.cprintf (ent, PRINT_HIGH, "\"%s\" is not on team %s.\n",
					gi.args (), teams[tnum].netname);
		return;
	}

	// Referee form: `captain <tnum> <player>`.
	if (ent->osp_e39c && gi.argc () > 2)
	{
		prevcap = -1;
		for (i = 1; i <= game.maxclients; i++)
		{
			other = g_edicts + i;
			if (!other->inuse || !other->client ||
				other->client->resp.team != tnum ||
				other->client->resp.entered != ENTERED_ENTERED ||
				!other->client->resp.osp_r2c4)
				continue;

			prevcap = i;
			break;
		}

		if (prevcap < 0)
			gi.cprintf (ent, PRINT_HIGH, "There is no team captain for \"%s\".\n",
						teams[tnum].netname);

		for (i = 1; i <= game.maxclients; i++)
		{
			other = g_edicts + i;
			if (!other->inuse || !other->client ||
				other->client->resp.team != tnum ||
				other->client->resp.entered != ENTERED_ENTERED ||
				Q_stricmp (gi.argv (2), other->client->pers.netname))
				continue;

			gi.cprintf (ent, PRINT_HIGH, "Team captain is now \"%s\"\n",
						other->client->pers.netname);
			gi.cprintf (other, PRINT_HIGH, "You are now team captain.\n");
			other->client->resp.osp_r2c4 = 1;
			if (prevcap >= 0)
			{
				g_edicts[prevcap].client->resp.osp_r2c4 = 0;
				gi.cprintf (g_edicts + prevcap, PRINT_HIGH, "Team captain is now \"%s\"\n",
							other->client->pers.netname);
			}
			return;
		}

		gi.cprintf (ent, PRINT_HIGH, "\"%s\" is not on team %s.\n",
					gi.argv (2), teams[tnum].netname);
	}
	else
		gi.cprintf (ent, PRINT_HIGH, "Unknown captain request (%d)\n", gi.argc ());
}

// gamex86.dll: 1003B2FC..1003B3AB
// gamei386.so: 00068120..000681C9
void OSP_captains_cmd (edict_t *ent)
{
	edict_t		*other;
	int			i;

	for (i = 1; i <= game.maxclients; i++)
	{
		other = g_edicts + i;
		if (!other->inuse || !other->client ||
			other->client->resp.entered != ENTERED_ENTERED ||
			!other->client->resp.osp_r2c4)
			continue;

		gi.cprintf (ent, PRINT_HIGH, "Team captain for %s is \"%s\".\n",
						teams[other->client->resp.team].netname,
						other->client->pers.netname);
	}
}

// A captain (or a referee, who must name the team first) drops a player from
// the team. A bot is destroyed outright through the Gladiator SDK's `removebot`
// server command; a human is dropped into observer mode.
// gamex86.dll: 1003B3AB..1003B6BC
// gamei386.so: 000681CC..000684DE
void OSP_kickplayer_cmd (edict_t *ent)
{
	char		pname[32];
	edict_t		*victim;
	int			i;
	int			tnum;

	tnum = ent->client->resp.team;
	if (!ent->osp_e39c && !ent->client->resp.osp_r2c4)
	{
		gi.cprintf (ent, PRINT_HIGH, "Only team captains can kick players.\n");
		return;
	}

	if (ent->client->resp.osp_r2c4 && gi.argc () < 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: kickplayer <player_name>\n");
		return;
	}

	if (ent->osp_e39c && gi.argc () < 3 && !ent->client->resp.osp_r2c4)
	{
		gi.cprintf (ent, PRINT_HIGH,
					"(Referee) Usage: kickplayer <team_name> <player_name>\n");
		return;
	}

	if (ent->client->resp.osp_r2c4)
		strcpy (pname, gi.args ());
	else
	{
		if (!Q_stricmp (gi.argv (1), teams[0].netname))
			tnum = 0;
		else if (!Q_stricmp (gi.argv (1), teams[1].netname))
			tnum = 1;
		else
		{
			gi.cprintf (ent, PRINT_HIGH,
						"Ref (kickplayer): unknown team \"%s\"\n", gi.argv (1));
			return;
		}
		strcpy (pname, gi.argv (2));
	}

	victim = OSP_findPlayer (pname);
	if (!victim)
	{
		for (i = 1; i <= game.maxclients; i++)
		{
			victim = g_edicts + i;
			if (!victim->inuse || !victim->client ||
				victim->client->resp.team != tnum ||
				victim->client->resp.entered != ENTERED_ENTERED ||
				Q_stricmp (pname, victim->client->pers.netname))
				continue;
			break;
		}
		if (i > game.maxclients)
		{
			gi.cprintf (ent, PRINT_HIGH, "\"%s\" is not on team %s\n",
						pname, teams[tnum].greenname);
			return;
		}
	}
	else if (victim->client->resp.team != tnum ||
			 victim->client->resp.entered != ENTERED_ENTERED)
	{
		gi.cprintf (ent, PRINT_HIGH, "\"%s\" is not on team %s\n",
					pname, teams[tnum].greenname);
		return;
	}

	gi.bprintf (PRINT_HIGH, "%s has been removed from \"%s\"\n",
				pname, teams[tnum].netname);

	if (victim->flags & FL_OSP_NOCMD)
	{
		BotServerCommand ("sv", "removebot", pname, 0);
		// The target's own oddity, reproduced: the counter is subtracted from
		// itself and the (always zero) result clamped.
		bots_votedin -= bots_votedin;
		if (bots_votedin < 0)
			bots_votedin = 0;
	}
	else
		OSP_startObserve (victim);
}

// `queue` -- print the 1v1 waiting line. The two slots at the head of it also
// get their claim deadline seeded here if it has not been set yet, which is
// why a read-only-looking command writes p_order[26]/[27].
// gamex86.dll: 1003B6BC..1003B926
// gamei386.so: 000684E0..000687D4
void OSP_1v1queue_cmd (edict_t *ent)
{
	char		tmp[128];
	char		scratch[64];
	int			t;

	if (!(int)team_nextuptime->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "Player queueing currently disabled.\n");
		return;
	}

	gi.cprintf (ent, PRINT_HIGH, "\nCurrent 1v1 queue:\n------------------\n");

	for (t = 0; t < p_order[25]; t++)
	{
		if (p_order[t] == ent - g_edicts - 1)
			strcpy (tmp, ent->client->pers.greenname);
		else
			strcpy (tmp, g_edicts[p_order[t] + 1].client->pers.netname);

		if (t < 2)
		{
			if (g_edicts[p_order[t] + 1].client->resp.entered == ENTERED_ENTERED)
				strcat (tmp, " [Playing]");
			else
			{
				if (!p_order[26 + t])
					p_order[26 + t] =
						level.framenum + (int)team_nextuptime->value * 10;

				if (p_order[26 + t] > 0)
				{
					if (p_order[26 + t] < level.framenum)
						strcat (tmp, " [Not yet joined --> will give up slot]");
					else
					{
						if (!g_edicts[p_order[t] + 1].inuse)
							sprintf (scratch,
									 " [Connecting --> must join in %d sec]",
									 (p_order[26 + t] - level.framenum) / 10);
						else
							sprintf (scratch,
									 " [Not yet joined --> must join in %d sec]",
									 (p_order[26 + t] - level.framenum) / 10);
						strcat (tmp, scratch);
					}
				}
				else
					strcat (tmp, " [Not yet joined]");
			}
		}

		gi.cprintf (ent, PRINT_HIGH, "%d. %s\n", t + 1, tmp);
	}

	gi.cprintf (ent, PRINT_HIGH, "\n");
}

// Wipe both teams back to the server's configured names. The green copy is
// rebuilt from the plain one by the same `+= 0x80` loop OSP_defaultTeam uses.
// gamex86.dll: 1003B926..1003BAF5
// gamei386.so: 000687D4..000689BE
void OSP_teamReset (void)
{
	int			i;

	for (i = 0; i < 2; i++)
	{
		teams[i].osp_m0f8 = 0;
		teams[i].osp_m0f4 = 0;
		teams[i].osp_m0f0 = 0;
		teams[i].osp_m100 = 0;
		teams[i].osp_m0fc = 0;
		teams[i].osp_m104 = 0;
		teams[i].osp_m108 = 0;
		teams[i].joincode[0] = 0;
		teams[i].osp_m124 = 0;
	}

	strcpy (teams[0].netname, team_a_name->string);
	strcpy (teams[0].greenname, team_a_name->string);
	for (i = 0; i < strlen (teams[0].greenname); i++)
		teams[0].greenname[i] += 128;

	strcpy (teams[1].netname, team_b_name->string);
	strcpy (teams[1].greenname, team_b_name->string);
	for (i = 0; i < strlen (teams[1].greenname); i++)
		teams[1].greenname[i] += 128;

	if (m_mode == 2)
	{
		gi.cvar_set ("Score_A", "WARMUP");
		gi.cvar_set ("Score_B", "WARMUP");
	}
}

// End of match: announce the result, stamp teams[].osp_m124 with 1 = won,
// 2 = lost, 4 = tied, and print the two-line team summary. In 1v1 the loser
// goes to the back of the queue instead.
// gamex86.dll: 1003BAF5..1003BFAB
// gamei386.so: 000689C0..00068DF4
void OSP_findTeamWinner (void)
{
	edict_t		*ent;
	int			winpct;
	int			loserpct;
	int			winner;
	int			lose;

	winner = 0;
	lose = 1;
	if (teams[0].osp_m0f8 < teams[1].osp_m0f8)
	{
		winner = 1;
		lose = 0;
	}

	if (teams[winner].osp_m0f8 < 1)
		winpct = 0;
	else if (!teams[winner].osp_m0fc ||
			 !(teams[winner].osp_m0fc + teams[winner].osp_m0f8))
		winpct = 100;
	else
		winpct = teams[winner].osp_m0f8 * 100 /
				 (teams[winner].osp_m0fc + teams[winner].osp_m0f8);

	if (teams[lose].osp_m0f8 < 1)
		loserpct = 0;
	else if (!teams[lose].osp_m0fc ||
			 !(teams[lose].osp_m0fc + teams[lose].osp_m0f8))
		loserpct = 100;
	else
		loserpct = teams[lose].osp_m0f8 * 100 /
				  (teams[lose].osp_m0fc + teams[lose].osp_m0f8);

	if (teams[winner].osp_m0f8 > teams[lose].osp_m0f8)
	{
		teams[winner].osp_m124 = 1;
		teams[lose].osp_m124 = 2;
		gi.bprintf (PRINT_HIGH, "\n\n%s defeats %s: %d - %d\n\n",
					teams[winner].netname, teams[lose].netname,
					teams[winner].osp_m0f8, teams[lose].osp_m0f8);
	}
	else
	{
		teams[winner].osp_m124 = 4;
		teams[lose].osp_m124 = 4;
		gi.bprintf (PRINT_HIGH, "\n\nTied match! (%d to %d)\n\n",
					teams[winner].osp_m0f8, teams[lose].osp_m0f8);
	}

	if (m_mode == 2)
	{
		gi.bprintf (PRINT_HIGH, "Frt: Fratricides          F  S  E\n");
		gi.bprintf (PRINT_HIGH, "EK : Enemy Kills       E  r  u  f\n");
		gi.bprintf (PRINT_HIGH, " S : Score         S   K  t  i  f\n");
		gi.bprintf (PRINT_HIGH, "====================================\n");
		gi.bprintf (PRINT_HIGH, "%-16s %3d %3d %2d %2d %d%%\n",
					teams[winner].netname, teams[winner].osp_m0f8,
					teams[winner].osp_m100, teams[winner].osp_m104,
					teams[winner].osp_m108, winpct);
		gi.bprintf (PRINT_HIGH, "%-16s %3d %3d %2d %2d %d%%\n\n",
					teams[lose].netname, teams[lose].osp_m0f8,
					teams[lose].osp_m100, teams[lose].osp_m104,
					teams[lose].osp_m108, loserpct);
		return;
	}

	gi.bprintf (PRINT_HIGH, "Sui: Suicides            S  E\n");
	gi.bprintf (PRINT_HIGH, " EK: Enemy Kills      E  u  f\n");
	gi.bprintf (PRINT_HIGH, "  S: Score        S   K  i  f\n");
	gi.bprintf (PRINT_HIGH, "================================\n");
	gi.bprintf (PRINT_HIGH, "%-15s %3d %3d %2d %d%%\n",
				teams[winner].netname, teams[winner].osp_m0f8,
				teams[winner].osp_m100, teams[winner].osp_m108, winpct);
	gi.bprintf (PRINT_HIGH, "%-15s %3d %3d %2d %d%%\n\n",
				teams[lose].netname, teams[lose].osp_m0f8,
				teams[lose].osp_m100, teams[lose].osp_m108, loserpct);

	for (winpct = 1; winpct <= game.maxclients; winpct++)
	{
		ent = g_edicts + winpct;
		if (!ent->inuse || !ent->client)
			continue;

		if (ent->client->resp.team == lose &&
			ent->client->resp.entered == ENTERED_ENTERED)
		{
			OSP_1v1Remove (ent, 2);
			return;
		}
	}
}

// A tied match: mode 1 is sudden death straight away, mode 2 always adds time,
// anything else adds time until `count` reaches team_overtime_count and then
// falls back to sudden death. `frag_offset` is what makes the next frag win.
// gamex86.dll: 1003BFAB..1003C185
// gamei386.so: 00068DF4..0006910B
qboolean OSP_overtimeWork (int count)
{
	if (!(int)team_overtime_mode->value)
		return false;

	if ((int)team_overtime_mode->value == 1)
	{
		frag_offset = teams[0].osp_m0f8 + 1;
		gi.bprintf (PRINT_HIGH, "Tied match!! Sudden Death mode in effect!!!\n");
		return true;
	}

	if ((int)team_overtime_mode->value == 2)
	{
		overtime_timer += (int)team_overtime_time->value;
		gi.bprintf (PRINT_HIGH, "Tied match!! %d minutes added to time!\n",
					(int)team_overtime_time->value);

		if ((int)team_overtime_time->value >= 1)
			start_count = 3;
		if ((int)team_overtime_time->value >= 5)
			start_count = 1;
		if ((int)team_overtime_time->value >= 10)
			start_count = 0;

		return true;
	}

	if (count >= (int)team_overtime_count->value)
	{
		frag_offset = teams[0].osp_m0f8 + 1;
		gi.bprintf (PRINT_HIGH, "Tied match!! Sudden Death mode now in effect!!!\n");
		return true;
	}

	overtime_timer += (int)team_overtime_time->value;
	gi.bprintf (PRINT_HIGH, "Tied match!! %d minutes added to time!\n",
				(int)team_overtime_time->value);

	if ((int)team_overtime_time->value >= 1)
		start_count = 3;
	if ((int)team_overtime_time->value >= 5)
		start_count = 1;
	if ((int)team_overtime_time->value >= 10)
		start_count = 0;

	return true;
}
