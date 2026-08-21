#include "g_local.h"



/*
======================================================================

INTERMISSION

======================================================================
*/

// gamex86.dll: 10058E30..1005903E
// gamei386.so: 0003EF84..0003F1A6
void MoveClientToIntermission (edict_t *ent)
{
	if (deathmatch->value || coop->value)
		ent->client->showscores = true;
	VectorCopy (level.intermission_origin, ent->s.origin);
	ent->client->ps.pmove.origin[0] = level.intermission_origin[0]*8;
	ent->client->ps.pmove.origin[1] = level.intermission_origin[1]*8;
	ent->client->ps.pmove.origin[2] = level.intermission_origin[2]*8;
	VectorCopy (level.intermission_angle, ent->client->ps.viewangles);
	ent->client->ps.pmove.pm_type = PM_FREEZE;
	ent->client->ps.gunindex = 0;
	ent->client->ps.blend[3] = 0;
	ent->client->ps.rdflags &= ~RDF_UNDERWATER;

	// clean up powerup info
	ent->client->quad_framenum = 0;
	ent->client->invincible_framenum = 0;
	ent->client->breather_framenum = 0;
	ent->client->enviro_framenum = 0;
	ent->client->grenade_blew_up = false;
	ent->client->grenade_time = 0;

	ent->viewheight = 0;
	ent->s.modelindex = 0;
	ent->s.modelindex2 = 0;
	ent->s.modelindex3 = 0;
	ent->s.modelindex = 0;
	ent->s.effects = 0;
	ent->s.sound = 0;
	ent->solid = SOLID_NOT;

	// add the layout

	if (!(ent->flags & FL_BOT) && !ent->client->resp.osp_r2dc)
	{
		DeathmatchScoreboardMessage (ent, NULL);
		gi.unicast (ent, true);
	}

}

// gamex86.dll: 1005903E..10059287
// gamei386.so: 0003F1A8..0003F43A
void BeginIntermission (edict_t *targ)
{
	int		i;
	edict_t	*ent, *client;

	if (level.intermissiontime)
		return;		// already activated

	game.autosaved = false;

	// respawn any dead clients
	for (i=0 ; i<maxclients->value ; i++)
	{
		client = g_edicts + 1 + i;
		if (!client->inuse)
			continue;
		if (client->health <= 0 && client->client->resp.entered == ENTERED_ENTERED)
			respawn(client);
	}

	level.intermissiontime = level.time;
	level.changemap = targ->map;

	if (!connected_clients)
	{
		level.exitintermission = 1;		// go immediately to the next level
		return;
	}

	level.exitintermission = 0;

	// find an intermission spot
	ent = G_Find (NULL, FOFS(classname), "info_player_intermission");
	if (!ent)
	{	// the map creator forgot to put in an intermission point...
		ent = G_Find (NULL, FOFS(classname), "info_player_start");
		if (!ent)
			ent = G_Find (NULL, FOFS(classname), "info_player_deathmatch");
	}
	else
	{	// chose one of four spots
		i = rand() & 3;
		while (i--)
		{
			ent = G_Find (ent, FOFS(classname), "info_player_intermission");
			if (!ent)	// wrap around the list
				ent = G_Find (ent, FOFS(classname), "info_player_intermission");
		}
	}

	VectorCopy (ent->s.origin, level.intermission_origin);
	VectorCopy (ent->s.angles, level.intermission_angle);

	// move all clients to the intermission point
	for (i=0 ; i<maxclients->value ; i++)
	{
		client = g_edicts + 1 + i;
		if (!client->inuse)
			continue;
		client->client->resp.osp_r2dc = 2;
		client->client->resp.osp_r034 = 0;
		OSP_zeroRuneStats (client);
		MoveClientToIntermission (client);
	}
}


/*
==================
DeathmatchScoreboardMessage

==================
*/
// gamex86.dll: 10059287..100597B6
// gamei386.so: 0003F43C..0003F931
void DeathmatchScoreboardMessage (edict_t *ent, edict_t *killer)
{
	int		sorted[MAX_CLIENTS];
	int		sortedscores[MAX_CLIENTS];
	int		i;
	int		score;
	int		total;
	int		j;
	int		k;
	edict_t	*cl_ent;

	if (ent->client->resp.osp_r24c == 8)
	{
		OSP_showPlayer (ent);
		return;
	}
	if (ent->client->resp.osp_r24c == 2)
	{
		OSP_showMOTD ();
		return;
	}
	if (ent->client->resp.osp_r24c == 4)
	{
		OSP_showParams ();
		return;
	}
	if (ent->client->resp.osp_r24c == 1)
	{
		OSP_oldscores_cmd (ent);
		return;
	}

	if (m_mode == 2)
	{
		OSP_showTeamScores (ent);
		return;
	}
	if (m_mode == 3)
	{
		OSP_show1v1Scores (ent);
		return;
	}

	if (hs_mode && (int)client_highscores->value &&
		level.intermissiontime != 0)
	{
		if (level.framenum > ent->client->resp.osp_r244)
		{
			if (ent->client->resp.osp_r034)
				ent->client->resp.osp_r244 = level.framenum + 40;
			else
				ent->client->resp.osp_r244 = level.framenum + 100;
			ent->client->resp.osp_r034 = 1 - ent->client->resp.osp_r034;
		}

		if (!ent->client->resp.osp_r034)
		{
			OSP_showHighScores (ent);
			return;
		}
	}

	total = 0;
	if (sync_stat < 4)
	{
		for (i = 0; i < game.maxclients; i++)
		{
			cl_ent = g_edicts + i + 1;
			if (!cl_ent->inuse || !cl_ent->client)
				continue;

			for (j = 0; j < total; j++)
			{
				if (!cl_ent->client->resp.osp_r20c && cl_ent->osp_e39c != 1)
					break;
				// The bracketed form at this site, unlike the loop head above.
				if (cl_ent->osp_e39c == 1 &&
					(g_edicts[j + 1].osp_e39c == 1 ||
					 game.clients[j].resp.osp_r20c))
					break;
			}

			for (k = total; k > j; k--)
				sorted[k] = sorted[k - 1];
			sorted[j] = i;
			total++;
		}

		OSP_showScores (sorted, total, ent);
	}
	else
	{
		for (i = 0; i < game.maxclients; i++)
		{
			cl_ent = g_edicts + i + 1;
			if (!cl_ent->inuse)
				continue;

			score = cl_ent->client->resp.score;
			for (j = 0; j < total; j++)
			{
				if (score > sortedscores[j])
					break;
				if (score == sortedscores[j])
				{
					if (game.clients[i].resp.osp_r014 <
						game.clients[sorted[j]].resp.osp_r014)
						break;
					if (game.clients[i].resp.osp_r014 ==
						game.clients[sorted[j]].resp.osp_r014)
					{
						if (game.clients[i].resp.osp_r2c0 <
							game.clients[sorted[j]].resp.osp_r2c0)
							break;
					}
				}
			}

			for (k = total; k > j; k--)
			{
				sorted[k] = sorted[k - 1];
				sortedscores[k] = sortedscores[k - 1];
			}
			sorted[j] = i;
			sortedscores[j] = score;
			total++;
		}

		// The call is written out in BOTH arms.
		OSP_showScores (sorted, total, ent);
	}
}


/*
==================
DeathmatchScoreboard

Draw instead of help message.
Note that it isn't that hard to overflow the 1400 byte message limit!
==================
*/
// gamex86.dll: 100597B6..10059801
// gamei386.so: 0003F934..0003F97E
void DeathmatchScoreboard (edict_t *ent)
{
	if (!ent->client->resp.osp_r2dc)
	{
		if (!(ent->flags & FL_BOT))
		{
			DeathmatchScoreboardMessage (ent, ent->enemy);
			gi.unicast (ent, true);
		}
	}
}


/*
==================
Cmd_Score_f

Display the scoreboard
==================
*/
// gamex86.dll: 10059801..10059935
// gamei386.so: 0003F980..0003FAA1
void Cmd_Score_f (edict_t *ent)
{
	ent->client->showinventory = false;
	ent->client->showhelp = false;

	if ((ent->client->resp.osp_r24c == 0 || ent->client->resp.osp_r24c == 1) &&
		ent->client->showscores)
	{
		ent->client->showscores = false;
		ent->client->update_chase = true;
		ent->client->ps.stats[STAT_OSP_LAYOUT1] = 0;
		ent->client->ps.stats[STAT_OSP_LAYOUT2] = 0;
		ent->client->resp.osp_r2ac = -1;
		return;
	}

	if (ent->client->resp.osp_r24c == 4)
	{
		ent->client->resp.osp_r0ac = level.framenum - 100;
		if (ent->client->resp.osp_r0ac < 0)
			ent->client->resp.osp_r0ac = 0;
	}

	ent->client->resp.osp_r24c = 0;
	ent->client->showscores = true;
	ent->client->resp.osp_r034 = 1;
	ent->client->resp.osp_r244 = 0;

	DeathmatchScoreboard (ent);
}


/*
==================
HelpComputer

Draw help computer.
==================
*/
// gamex86.dll: 10059935..10059A32
// gamei386.so: 0003FAA4..0003FBA6
void HelpComputer (edict_t *ent)
{
	char	string[1024];
	char	*sk;

	if (skill->value == 0)
		sk = "easy";
	else if (skill->value == 1)
		sk = "medium";
	else if (skill->value == 2)
		sk = "hard";
	else
		sk = "hard+";

	// send the layout
	Com_sprintf (string, sizeof(string),
		"xv 32 yv 8 picn help "			// background
		"xv 202 yv 12 string2 \"%s\" "		// skill
		"xv 0 yv 24 cstring2 \"%s\" "		// level name
		"xv 0 yv 54 cstring2 \"%s\" "		// help 1
		"xv 0 yv 110 cstring2 \"%s\" "		// help 2
		"xv 50 yv 164 string2 \" kills     goals    secrets\" "
		"xv 50 yv 172 string2 \"%3i/%3i     %i/%i       %i/%i\" ", 
		sk,
		level.level_name,
		game.helpmessage1,
		game.helpmessage2,
		level.killed_monsters, level.total_monsters, 
		level.found_goals, level.total_goals,
		level.found_secrets, level.total_secrets);

	gi.WriteByte (svc_layout);
	gi.WriteString (string);
	gi.unicast (ent, true);
}


/*
==================
Cmd_Help_f

Display the current help message
==================
*/
// gamex86.dll: 10059A32..10059A82
// gamei386.so: 0003FBA8..0003FD29
void Cmd_Help_f (edict_t *ent)
{
	if (ent->client->resp.osp_r010 <= level.framenum)
	{
		ent->client->resp.osp_r010 = level.framenum + 2;
		Cmd_Score_f (ent);
	}
	else if (match_paused)
		Cmd_Score_f (ent);
}


//=======================================================================

/*
===============
G_SetStats
===============
*/
// gamex86.dll: 10059A82..10059FB0
// gamei386.so: 0003FD2C..0004023A
void G_SetStats (edict_t *ent)
{
	gitem_t		*item;
	int			index, cells;
	int			power_armor_type;
	// Declared AFTER the three above -- declaration order is spill-slot order.
	gclient_t	*cl = ent->client;
	// A SECOND client pointer, and it is the majority user: one reaches only
	// `ps.fov` and `ps.stats[]`, the other only `pers`/`resp`.
	player_state_t	*ps = &ent->client->ps;

	//
	// layouts
	//
	ps->stats[STAT_LAYOUTS] = 0;

	if (!cl->resp.osp_r2dc)
	{
		if (cl->pers.health <= 0 || level.intermissiontime
			|| cl->showscores)
			ps->stats[STAT_LAYOUTS] |= 1;
		if (cl->showinventory && cl->pers.health > 0)
			ps->stats[STAT_LAYOUTS] |= 2;
	}

	//
	// frags
	//
	if (cl->resp.entered == ENTERED_ENTERED)
		ps->stats[STAT_FRAGS] = cl->resp.score;
	else
		ps->stats[STAT_FRAGS] = 0;

	if (!cl->resp.osp_r2bc)
		return;

	//
	// health
	//
	ps->stats[STAT_HEALTH_ICON] = level.pic_health;
	ps->stats[STAT_HEALTH] = ent->health;

	//
	// ammo
	//
	if (!cl->ammo_index /* || !cl->pers.inventory[cl->ammo_index] */)
	{
		ps->stats[STAT_AMMO_ICON] = 0;
		ps->stats[STAT_AMMO] = 0;
	}
	else
	{
		item = &itemlist[cl->ammo_index];
		ps->stats[STAT_AMMO_ICON] = gi.imageindex (item->icon);
		ps->stats[STAT_AMMO] = cl->pers.inventory[cl->ammo_index];
	}
	
	//
	// armor
	//
	power_armor_type = PowerArmorType (ent);
	if (power_armor_type)
	{
		cells = cl->pers.inventory[ITEM_INDEX(FindItem ("cells"))];
		if (cells == 0)
		{	// ran out of cells for power armor
			ent->flags &= ~FL_POWER_ARMOR;
			gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/power2.wav"), 1, ATTN_NORM, 0);
			power_armor_type = 0;;
		}
	}

	index = ArmorIndex (ent);
	if (power_armor_type && (!index || (level.framenum & 8) ) )
	{	// flash between power armor and other armor icon
		ps->stats[STAT_ARMOR_ICON] = gi.imageindex ("i_powershield");
		ps->stats[STAT_ARMOR] = cells;
	}
	else if (index)
	{
		item = GetItemByIndex (index);
		ps->stats[STAT_ARMOR_ICON] = gi.imageindex (item->icon);
		ps->stats[STAT_ARMOR] = cl->pers.inventory[index];
	}
	else
	{
		ps->stats[STAT_ARMOR_ICON] = 0;
		ps->stats[STAT_ARMOR] = 0;
	}

	//
	// pickup message
	//
	if (level.time > cl->pickup_msg_time)
	{
		ps->stats[STAT_PICKUP_ICON] = 0;
		ps->stats[STAT_PICKUP_STRING] = 0;
	}

	//
	// timers
	//
	if (cl->quad_framenum > level.framenum)
	{
		ps->stats[STAT_TIMER_ICON] = gi.imageindex ("p_quad");
		ps->stats[STAT_TIMER] = (cl->quad_framenum - level.framenum)/10;
	}
	else if (cl->invincible_framenum > level.framenum)
	{
		ps->stats[STAT_TIMER_ICON] = gi.imageindex ("p_invulnerability");
		ps->stats[STAT_TIMER] = (cl->invincible_framenum - level.framenum)/10;
	}
	else if (cl->enviro_framenum > level.framenum)
	{
		ps->stats[STAT_TIMER_ICON] = gi.imageindex ("p_envirosuit");
		ps->stats[STAT_TIMER] = (cl->enviro_framenum - level.framenum)/10;
	}
	else if (cl->breather_framenum > level.framenum)
	{
		ps->stats[STAT_TIMER_ICON] = gi.imageindex ("p_rebreather");
		ps->stats[STAT_TIMER] = (cl->breather_framenum - level.framenum)/10;
	}
	else
	{
		ps->stats[STAT_TIMER_ICON] = 0;
		ps->stats[STAT_TIMER] = 0;
	}

	//
	// selected item
	//
	if (cl->pers.selected_item == -1)
		ps->stats[STAT_SELECTED_ICON] = 0;
	else
		ps->stats[STAT_SELECTED_ICON] = gi.imageindex (itemlist[cl->pers.selected_item].icon);

	ps->stats[STAT_SELECTED_ITEM] = cl->pers.selected_item;

	//
	// help icon / current weapon if not shown
	//
	if (cl->resp.helpchanged && (level.framenum&8) )
		ps->stats[STAT_HELPICON] = gi.imageindex ("i_help");
	else if ( (cl->pers.hand == CENTER_HANDED || ps->fov > 91)
		&& ent->client->pers.weapon)
		ps->stats[STAT_HELPICON] = gi.imageindex (cl->pers.weapon->icon);
	else
		ps->stats[STAT_HELPICON] = 0;

	cl->resp.osp_r2bc = 0;
}
