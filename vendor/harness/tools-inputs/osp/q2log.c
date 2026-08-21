// q2log.c -- <INVENTED FILENAME>. The mod's game-event log layer.
//
// It formats one text line per game event and hands it to ngLog (nglog.c) for
// whichever of the two open logs is enabled.  `ngloglog_status` gates
// everything; `nglog_logstyle` selects log 1 (the ngLog format) and
// `nglog_worldstats` log 2 (the ngStats format), so most writers go through
// q2log_logWrite and let it fan out.

#include "g_local.h"

// gamex86.dll: 1004FA60..1004FBD3
// gamei386.so: 0007057C..000706C4
void q2log_customStart (void)
{
	char	line[1024];
	cvar_t	*port;

	port = gi.cvar ("port", "27910", CVAR_NOSET);

	sprintf (line, "0.0\tinfo\tGame_Patch\t%s", "tourney");
	q2log_logWrite (line);
	sprintf (line, "0.0\tinfo\tGame_Patch_Name\t%s", "OSP Tourney DM");
	q2log_logWrite (line);
	sprintf (line, "0.0\tinfo\tGame_Patch_Version\t%s", "2.75");
	q2log_logWrite (line);
	sprintf (line, "0.0\tinfo\tGame_Patch_Decoder_Ring_URL\t%s",
		"http://www.netgamesusa.com/ngLog/tourney");
	q2log_logWrite (line);
	sprintf (line, "0.0\tinfo\tGame_Patch_Author\t%s",
		"Orange Smoothie Productions");
	q2log_logWrite (line);
	sprintf (line, "0.0\tinfo\tGame_Patch_Author_URL\t%s",
		"http://www.OrangeSmoothie.org");
	q2log_logWrite (line);
	sprintf (line, "0.0\tinfo\tServer_Address\t%s", ngLog_hostAddr ());
	q2log_logWrite (line);
	sprintf (line, "0.0\tinfo\tServer_Port\t%d", (int)port->value);
	q2log_logWrite (line);
}

// gamex86.dll: 1004FBD3..1004FF9E
// gamei386.so: 000706C4..00070AB9
void q2log_gameInit (int restart)
{
	char	stamp[64];
	char	line[2048];
	cvar_t	*hostname;

	hostname = gi.cvar ("hostname", "noname", CVAR_SERVERINFO);

	if (restart)
		q2log_logStart ();

	if (!ngloglog_status)
		return;

	ngLog_getDateInfo (stamp, 1);
	sprintf (line, "0.0\tGame_Init\t%s", stamp);
	q2log_logWrite (line);
	sprintf (line, "0.0\tinfo\tmatch_mode\t%s", match_type->string);
	q2log_logWrite (line);
	sprintf (line, "0.0\tinfo\ttimelimit\t%s", timelimit->string);
	q2log_logWrite (line);
	sprintf (line, "0.0\tinfo\tfraglimit\t%s", fraglimit->string);
	q2log_logWrite (line);
	sprintf (line, "0.0\tinfo\tdmflags\t%s", dmflags->string);
	q2log_logWrite (line);
	sprintf (line, "0.0\tinfo\tcheats\t%s", sv_cheats->string);
	q2log_logWrite (line);
	sprintf (line, "0.0\tinfo\tmaxclients\t%s", maxclients->string);
	q2log_logWrite (line);
	sprintf (line, "0.0\tinfo\thostname\t%s", hostname->string);
	q2log_logWrite (line);

	if ((int)hook_enable->value)
		sprintf (line, "0.0\tinfo\thook_status\tENABLED");
	else
		sprintf (line, "0.0\tinfo\thook_status\tDISABLED");
	q2log_logWrite (line);

	if (rune_stat)
		sprintf (line, "0.0\tinfo\trunes_status\tENABLED\t%d", rune_stat);
	else
		sprintf (line, "0.0\tinfo\trunes_status\tDISABLED");
	q2log_logWrite (line);

	if ((int)client_protect->value)
		sprintf (line, "0.0\tinfo\trespawn_protection\tENABLED\t(%d seconds)",
			(int)client_protect->value);
	else
		sprintf (line, "0.0\tinfo\trespawn_protection\tDISABLED");

	if (m_mode > 1)
	{
		q2log_teamName (teams[0].netname);
		q2log_teamName (teams[1].netname);

		if ((int)team_hurtteam->value)
			sprintf (line, "0.0\tinfo\tteam_hurtteam\tYES");
		else
			sprintf (line, "0.0\tinfo\tteam_hurtteam\tNO");
		q2log_logWrite (line);

		if ((int)team_hurtself->value)
			sprintf (line, "0.0\tinfo\tteam_hurtself\tYES");
		else
			sprintf (line, "0.0\tinfo\tteam_hurtself\tNO");
		q2log_logWrite (line);
	}

	sprintf (line, "0.0\tinfo\trailgun_damage\t%d", (int)damage_railgun->value);
	sprintf (line, "0.0\tinfo\tdisabled_items");
	q2log_listDissedItems (line);
	q2log_logWrite (line);
	sprintf (line, "0.0\tinfo\tmap\t%s\t%s", level.mapname, level.level_name);
	q2log_logWrite (line);
}

// gamex86.dll: 1004FF9E..10050472
// gamei386.so: 00070ABC..00071122
void q2log_listDissedItems (char *line)
{
	char	work[2048];
	int		length;
	cvar_t	*allow_shotgun;
	cvar_t	*allow_supershotgun;
	cvar_t	*allow_machinegun;
	cvar_t	*allow_chaingun;
	cvar_t	*allow_grenadelauncher;
	cvar_t	*allow_rocketlauncher;
	cvar_t	*allow_hyperblaster;
	cvar_t	*allow_railgun;
	cvar_t	*allow_bfg;
	cvar_t	*allow_ammo_grenades;
	cvar_t	*allow_item_powerscreen;
	cvar_t	*allow_item_powershield;
	cvar_t	*allow_item_quad;
	cvar_t	*allow_item_invul;

	allow_shotgun = gi.cvar ("allow_shotgun", "1", 0);
	allow_supershotgun = gi.cvar ("allow_supershotgun", "1", 0);
	allow_machinegun = gi.cvar ("allow_machinegun", "1", 0);
	allow_chaingun = gi.cvar ("allow_chaingun", "1", 0);
	allow_grenadelauncher = gi.cvar ("allow_grenadelauncher", "1", 0);
	allow_rocketlauncher = gi.cvar ("allow_rocketlauncher", "1", 0);
	allow_hyperblaster = gi.cvar ("allow_hyperblaster", "1", 0);
	allow_railgun = gi.cvar ("allow_railgun", "1", 0);
	allow_bfg = gi.cvar ("allow_bfg", "1", 0);
	allow_ammo_grenades = gi.cvar ("allow_ammo_grenades", "1", 0);
	allow_item_powerscreen = gi.cvar ("allow_item_powerscreen", "1", 0);
	allow_item_powershield = gi.cvar ("allow_item_powershield", "1", 0);
	allow_item_quad = gi.cvar ("allow_item_quad", "1", 0);
	allow_item_invul = gi.cvar ("allow_item_invul", "1", 0);

	length = strlen (line);

	if (!(int)allow_shotgun->value)
	{
		sprintf (work, "\tShotgun");
		strcat (line, work);
	}
	if (!(int)allow_supershotgun->value)
	{
		sprintf (work, "\tSuper Shotgun");
		strcat (line, work);
	}
	if (!(int)allow_machinegun->value)
	{
		sprintf (work, "\tMachinegun");
		strcat (line, work);
	}
	if (!(int)allow_chaingun->value)
	{
		sprintf (work, "\tChaingun");
		strcat (line, work);
	}
	if (!(int)allow_grenadelauncher->value)
	{
		sprintf (work, "\tGrenade Launcher");
		strcat (line, work);
	}
	if (!(int)allow_rocketlauncher->value)
	{
		sprintf (work, "\tRocket Launcher");
		strcat (line, work);
	}
	if (!(int)allow_hyperblaster->value)
	{
		sprintf (work, "\tHyperBlaster");
		strcat (line, work);
	}
	if (!(int)allow_railgun->value)
	{
		sprintf (work, "\tRailgun");
		strcat (line, work);
	}
	if (!(int)allow_bfg->value)
	{
		sprintf (work, "\tBFG10K");
		strcat (line, work);
	}
	if (!(int)allow_ammo_grenades->value)
	{
		sprintf (work, "\tGrenades");
		strcat (line, work);
	}
	if (!(int)allow_item_powerscreen->value)
	{
		sprintf (work, "\tPower Screen");
		strcat (line, work);
	}
	if (!(int)allow_item_powershield->value)
	{
		sprintf (work, "\tPower Shield");
		strcat (line, work);
	}
	if (!(int)allow_item_quad->value)
	{
		sprintf (work, "\tQuad");
		strcat (line, work);
	}
	if (!(int)allow_item_invul->value)
	{
		sprintf (work, "\tInvulnerability");
		strcat (line, work);
	}

	if (length == strlen (line))
	{
		sprintf (work, "\tNONE");
		strcat (line, work);
	}
}

// gamex86.dll: 10050472..100504BF
// gamei386.so: 00071124..00071178
void q2log_gameStart (void)
{
	char	stamp[64];
	char	line[256];

	ngLog_getDateInfo (stamp, 1);
	sprintf (line, "%.1f\tGame_Start", level.time);
	q2log_logWrite (line);
}

// gamex86.dll: 100504BF..100505AB
// gamei386.so: 00071178..000712B9
void q2log_gameEnd (char *reason, int closereason)
{
	char	text[256];
	char	mark[128];

	if (!ngloglog_status)
		return;

	if ((int)nglog_logstyle->value)
	{
		sprintf (text, "%.1f\tGame_End\t%s", level.time, reason);
		ngLog_logWrite (text, 1);
	}

	ngLog_giveMark (mark);
	sprintf (text, "%.1f\tGame_End\t%s\t%s", level.time, reason, mark);
	ngLog_logWrite (text, 2);

	if (!strcmp (reason, "server") || (int)nglog_logstyle->value == 4)
		ngLog_logClose (0, closereason);
	else
		ngLog_logClose (2, closereason);
}

// gamex86.dll: 100505AB..10050754
// gamei386.so: 000712BC..0007143B
void q2log_playerConnect (edict_t *ent)
{
	char	line[512];
	char	ident[256];

	if (!ngloglog_status)
		return;

	if (!(ent->flags & FL_BOT))
		sprintf (line, "%.1f\tPlayer_Connect\t%s\t%d\t%s", level.time,
			ent->client->pers.netname, ent->client->resp.clientid, ent->osp_e37c);
	else
		sprintf (line, "%.1f\tPlayer_Connect\t%s\t%d\tIS_A_BOT", level.time,
			ent->client->pers.netname, ent->client->resp.clientid);

	if ((int)nglog_logstyle->value)
		ngLog_logWrite (line, 1);

	if (ent->client->resp.osp_r0b4[0])
	{
		sprintf (ident, "\t%s", ngLog_playerIdentifier (ent->client->pers.netname,
			ent->client->resp.osp_r0b4));
		strcat (line, ident);
	}
	// The strcat is duplicated in each arm rather than written once after
	// them.
	else if (ent->flags & FL_BOT)
	{
		sprintf (ident, "\t[IS_A_BOT]");
		strcat (line, ident);
	}
	else
	{
		sprintf (ident, "\t[NO IDENTIFIER]");
		strcat (line, ident);
	}

	ngLog_logWrite (line, 2);
}

// gamex86.dll: 10050754..10050811
// gamei386.so: 0007143C..000714F4
void q2log_playerReconnect (edict_t *ent)
{
	char	line[256];

	if (!ngloglog_status)
		return;

	if (!(ent->flags & FL_BOT))
		sprintf (line, "%.1f\tPlayer_Reconnect\t%s\t%d\t%s", level.time,
			ent->client->pers.netname, ent->client->resp.clientid, ent->osp_e37c);
	else
		sprintf (line, "%.1f\tPlayer_Reconnect\t%s\t%d\tIS_A_BOT", level.time,
			ent->client->pers.netname, ent->client->resp.clientid);

	q2log_logWrite (line);
}

// gamex86.dll: 10050811..1005089C
// gamei386.so: 000714F4..00071579
void q2log_playerEntered (edict_t *ent)
{
	char	line[256];

	if (!ngloglog_status)
		return;

	if (!ent->client->resp.osp_r2a8)
	{
		ent->client->resp.osp_r2a8 = 1;
		q2log_playerConnect (ent);
	}

	sprintf (line, "%.1f\tPlayer_Enter\t%s\t%d", level.time,
		ent->client->pers.netname, ent->client->resp.clientid);
	q2log_logWrite (line);
}

// gamex86.dll: 1005089C..1005091C
// gamei386.so: 0007157C..000715F6
void q2log_playerDisconnect (edict_t *ent)
{
	char	line[256];

	if (!ent->client->resp.osp_r2a8)
	{
		ent->client->resp.osp_r2a8 = 1;
		q2log_playerConnect (ent);
	}

	sprintf (line, "%.1f\tPlayer_Leave\t%s\t%d", level.time,
		ent->client->pers.netname, ent->client->resp.clientid);
	q2log_logWrite (line);
}

// gamex86.dll: 1005091C..10050A76
// gamei386.so: 000715F8..00071726
void q2log_playerRename (edict_t *ent, char *oldname)
{
	char	line[512];
	char	ident[256];

	if (!ngloglog_status)
		return;

	sprintf (line, "%.1f\tPlayer_Rename\t%s\t%s\t%d\t%d", level.time,
		ent->client->pers.netname, oldname, ent->client->resp.clientid,
		ent->client->ping);

	if ((int)nglog_logstyle->value)
		ngLog_logWrite (line, 1);

	if (ent->client->resp.osp_r0b4[0])
	{
		sprintf (ident, "\t%s", ngLog_playerIdentifier (oldname,
			ent->client->resp.osp_r0b4));
		strcat (line, ident);
	}
	// Duplicated strcat in each arm -- see q2log_playerConnect.
	else if (ent->flags & FL_BOT)
	{
		sprintf (ident, "\t[IS_A_BOT]");
		strcat (line, ident);
	}
	else
	{
		sprintf (ident, "\t[NO IDENTIFIER]");
		strcat (line, ident);
	}

	ngLog_logWrite (line, 2);
}

// gamex86.dll: 10050A76..10050ABF
// gamei386.so: 00071728..0007177D
void q2log_playerRespawn (edict_t *ent)
{
	char	line[256];

	sprintf (line, "%.1f\tPlayer_Respawn\t%d", level.time,
		ent->client->resp.clientid);
	q2log_logWrite (line);
}

// gamex86.dll: 10050ABF..10050B26
// gamei386.so: 00071780..000717E7
void q2log_playerMode (edict_t *ent, char *mode)
{
	char	line[256];

	sprintf (line, "%.1f\t%s\t%s\t%d\t%d", level.time, mode,
		ent->client->pers.netname, ent->client->resp.clientid,
		ent->client->ping);
	q2log_logWrite (line);
}

// gamex86.dll: 10050B26..10050B77
// gamei386.so: 000717E8..0007186A
void q2log_playerChat (char *text)
{
	char	line[256];

	if ((int)nglog_logchat->value)
	{
		sprintf (line, "%.1f\tChat\t%s", level.time, text);
		q2log_logWrite (line);
	}
}

// gamex86.dll: 10050B77..10050C26
// gamei386.so: 0007186C..0007191B
void q2log_playerZBOT (edict_t *ent, char *extra)
{
	char	line[256];

	if (extra)
		sprintf (line, "%.1f\tPlayer_ZBOT\t%s\t%d\t%s\t%s", level.time,
			ent->client->pers.netname, ent->client->resp.clientid,
			ent->osp_e37c, extra);
	else
		sprintf (line, "%.1f\tPlayer_ZBOT\t%s\t%d\t%s", level.time,
			ent->client->pers.netname, ent->client->resp.clientid,
			ent->osp_e37c);

	q2log_logWrite (line);
}

// gamex86.dll: 10050C26..10050C66
// gamei386.so: 0007191C..00071968
void q2log_teamName (char *name)
{
	char	line[256];

	sprintf (line, "%.1f\tinfo\tTeam_Name\t%s", level.time, name);
	q2log_logWrite (line);
}

// gamex86.dll: 10050C66..10050CAA
// gamei386.so: 00071968..000719B7
void q2log_teamRename (char *oldname, char *newname)
{
	char	line[256];

	sprintf (line, "%.1f\tTeam_Rename\t%s\t%s", level.time, oldname, newname);
	q2log_logWrite (line);
}

// gamex86.dll: 10050CAA..10050D26
// gamei386.so: 000719B8..00071A33
void q2log_teamJoin (edict_t *ent)
{
	char	line[256];

	sprintf (line, "%.1f\tPlayer_Team_Join\t%s\t%d\t%s\t%d", level.time,
		ent->client->pers.netname, ent->client->resp.clientid,
		teams[ent->client->resp.team].netname, ent->client->ping);
	q2log_logWrite (line);
}

// gamex86.dll: 10050D26..10050D95
// gamei386.so: 00071A34..00071AA5
void q2log_teamLeave (edict_t *ent)
{
	char	line[256];

	sprintf (line, "%.1f\tPlayer_Team_Leave\t%d\t%s\t%d", level.time,
		ent->client->resp.clientid,
		teams[ent->client->resp.team].netname, ent->client->ping);
	q2log_logWrite (line);
}

// gamex86.dll: 10050D95..10050E0F
// gamei386.so: 00071AA8..00071B36
void q2log_voteInfo (char *what, char *a, char *b)
{
	char	line[256];

	if (a && b)
		sprintf (line, "%.1f\tVote\t%s\t%s\t%s", level.time, what, a, b);
	else
		sprintf (line, "%.1f\tVote\t%s", level.time, what);

	q2log_logWrite (line);
}

// gamex86.dll: 10050E0F..10050E6D
// gamei386.so: 00071B38..00071B9C
void q2log_pickupItem (char *name, int arg, edict_t *ent)
{
	char	line[256];

	sprintf (line, "%.1f\tItem_Pickup\t%s\t%d\t%d\t%d", level.time, name, arg,
		ent->client->resp.clientid, ent->client->ping);
	q2log_logWrite (line);
}

// gamex86.dll: 10050E6D..10050ED4
// gamei386.so: 00071B9C..00071C03
void q2log_useItem (char *name, edict_t *ent)
{
	char	line[256];

	sprintf (line, "%.1f\tItem_Use\t%s\t%d\t%d\t%d", level.time, name,
		ent->client->resp.osp_r200, ent->client->resp.clientid,
		ent->client->ping);
	q2log_logWrite (line);
}

// gamex86.dll: 10050ED4..10050F57
// gamei386.so: 00071C04..00071C71
void q2log_expireItem (char *name, edict_t *ent, int arg)
{
	char	line[256];

	// Two calls, not a ternary: the id is pushed in BOTH arms.
	if (ent)
		sprintf (line, "%.1f\tItem_Expire\t%s\t%d\t%d", level.time, name, arg,
			ent->client->resp.clientid);
	else
		sprintf (line, "%.1f\tItem_Expire\t%s\t%d\t%d", level.time, name, arg,
			-1);
	q2log_logWrite (line);
}

// gamex86.dll: 10050F57..10050FB5
// gamei386.so: 00071C74..00071CD8
void q2log_dropItem (char *name, int arg, edict_t *ent)
{
	char	line[256];

	sprintf (line, "%.1f\tItem_Drop\t%s\t%d\t%d\t%d", level.time, name,
		ent->client->resp.osp_r200, arg, ent->client->resp.clientid);
	q2log_logWrite (line);
}

// gamex86.dll: 10050FB5..10051011
// gamei386.so: 00071CD8..00071D40
void q2log_logAccuracy (void)
{
	int		i;
	edict_t	*ent;

	for (i = 1; i <= game.maxclients; i++)
	{
		ent = g_edicts + i;
		if (!ent->inuse || !ent->client)
			continue;
		q2log_logAccuracyStats (ent);
	}
}

// gamex86.dll: 10051011..10051823
// gamei386.so: 00071D40..0007226F
void q2log_logAccuracyStats (edict_t *ent)
{
	char	line[2048];
	char	stat[1024];
	// No cached per-weapon shot count.  `logged` is also zeroed TWICE at
	// entry: the declaration's initialiser plus a redundant first statement.
	int		logged = 0;
	int		clientid;

	logged = 0;
	clientid = ent->client->resp.clientid;

	sprintf (line, "%.1f\tPlayer_Accuracy\t%d", level.time, clientid);

	if (p_acc[clientid].shots[ACC_BLASTER] || p_acc[clientid].taken[ACC_BLASTER])
	{
		sprintf (stat, "\tBlaster:%d:%d:%d:%d",
			p_acc[clientid].hits[ACC_BLASTER], p_acc[clientid].shots[ACC_BLASTER],
			p_acc[clientid].given[ACC_BLASTER],
			p_acc[clientid].taken[ACC_BLASTER]);
		strcat (line, stat);
		logged = 1;
	}

	if (p_acc[clientid].shots[ACC_SHOTGUN] || p_acc[clientid].taken[ACC_SHOTGUN])
	{
		sprintf (stat, "\tShotgun:%d:%d:%d:%d",
			p_acc[clientid].hits[ACC_SHOTGUN], p_acc[clientid].shots[ACC_SHOTGUN],
			p_acc[clientid].given[ACC_SHOTGUN],
			p_acc[clientid].taken[ACC_SHOTGUN]);
		strcat (line, stat);
		logged = 1;
	}

	if (p_acc[clientid].shots[ACC_SSHOTGUN] || p_acc[clientid].taken[ACC_SSHOTGUN])
	{
		sprintf (stat, "\tSuper Shotgun:%d:%d:%d:%d",
			p_acc[clientid].hits[ACC_SSHOTGUN], p_acc[clientid].shots[ACC_SSHOTGUN],
			p_acc[clientid].given[ACC_SSHOTGUN],
			p_acc[clientid].taken[ACC_SSHOTGUN]);
		strcat (line, stat);
		logged = 1;
	}

	if (p_acc[clientid].shots[ACC_MACHINEGUN] || p_acc[clientid].taken[ACC_MACHINEGUN])
	{
		sprintf (stat, "\tMachinegun:%d:%d:%d:%d",
			p_acc[clientid].hits[ACC_MACHINEGUN], p_acc[clientid].shots[ACC_MACHINEGUN],
			p_acc[clientid].given[ACC_MACHINEGUN],
			p_acc[clientid].taken[ACC_MACHINEGUN]);
		strcat (line, stat);
		logged = 1;
	}

	if (p_acc[clientid].shots[ACC_CHAINGUN] || p_acc[clientid].taken[ACC_CHAINGUN])
	{
		sprintf (stat, "\tChaingun:%d:%d:%d:%d",
			p_acc[clientid].hits[ACC_CHAINGUN], p_acc[clientid].shots[ACC_CHAINGUN],
			p_acc[clientid].given[ACC_CHAINGUN],
			p_acc[clientid].taken[ACC_CHAINGUN]);
		strcat (line, stat);
		logged = 1;
	}

	if (p_acc[clientid].shots[ACC_GRENADE] || p_acc[clientid].taken[ACC_GRENADE])
	{
		sprintf (stat, "\tGrenades:%d:%d:%d:%d",
			p_acc[clientid].hits[ACC_GRENADE], p_acc[clientid].shots[ACC_GRENADE],
			p_acc[clientid].given[ACC_GRENADE],
			p_acc[clientid].taken[ACC_GRENADE]);
		strcat (line, stat);
		logged = 1;
	}

	if (p_acc[clientid].shots[ACC_GRENADELAUNCHER] || p_acc[clientid].taken[ACC_GRENADELAUNCHER])
	{
		sprintf (stat, "\tGrenade Launcher:%d:%d:%d:%d",
			p_acc[clientid].hits[ACC_GRENADELAUNCHER], p_acc[clientid].shots[ACC_GRENADELAUNCHER],
			p_acc[clientid].given[ACC_GRENADELAUNCHER],
			p_acc[clientid].taken[ACC_GRENADELAUNCHER]);
		strcat (line, stat);
		logged = 1;
	}

	if (p_acc[clientid].shots[ACC_ROCKET] || p_acc[clientid].taken[ACC_ROCKET])
	{
		sprintf (stat, "\tRocket Launcher:%d:%d:%d:%d",
			p_acc[clientid].hits[ACC_ROCKET], p_acc[clientid].shots[ACC_ROCKET],
			p_acc[clientid].given[ACC_ROCKET],
			p_acc[clientid].taken[ACC_ROCKET]);
		strcat (line, stat);
		logged = 1;
	}

	if (p_acc[clientid].shots[ACC_HYPERBLASTER] || p_acc[clientid].taken[ACC_HYPERBLASTER])
	{
		sprintf (stat, "\tHyperBlaster:%d:%d:%d:%d",
			p_acc[clientid].hits[ACC_HYPERBLASTER], p_acc[clientid].shots[ACC_HYPERBLASTER],
			p_acc[clientid].given[ACC_HYPERBLASTER],
			p_acc[clientid].taken[ACC_HYPERBLASTER]);
		strcat (line, stat);
		logged = 1;
	}

	if (p_acc[clientid].shots[ACC_RAILGUN] || p_acc[clientid].taken[ACC_RAILGUN])
	{
		sprintf (stat, "\tRailgun:%d:%d:%d:%d",
			p_acc[clientid].hits[ACC_RAILGUN], p_acc[clientid].shots[ACC_RAILGUN],
			p_acc[clientid].given[ACC_RAILGUN],
			p_acc[clientid].taken[ACC_RAILGUN]);
		strcat (line, stat);
		logged = 1;
	}

	if (p_acc[clientid].given[ACC_BFG] || p_acc[clientid].taken[ACC_BFG])
	{
		sprintf (stat, "\tBFG:0:0:%d:%d", p_acc[clientid].given[ACC_BFG],
			p_acc[clientid].taken[ACC_BFG]);
		strcat (line, stat);
		logged = 1;
	}

	if (logged)
	{
		sprintf (stat, "\tTotal Damage G/R:%d:%d",
			p_acc[clientid].dgiven, p_acc[clientid].dtaken);
		strcat (line, stat);
		q2log_logWrite (line);
	}
}

// gamex86.dll: 10051823..10051ED6
// gamei386.so: 00072270..00072713
void q2log_logDeath (edict_t *self, edict_t *inflictor, edict_t *attacker)
{
	char	line[256];
	int		mod;
	int		diff;
	int		ping;
	int		theirping;
	// Two more char * locals, both names invented.  `victimname` is written
	// once and never read; `slayname` gates the kill block.
	char	*slayname;
	char	*victimname;
	char	*wname;
	char	*victimweapon;

	mod = meansOfDeath & ~MOD_FRIENDLY_FIRE;
	diff = 0;
	ping = -1;
	theirping = -1;
	slayname = NULL;
	victimname = NULL;
	wname = NULL;
	victimweapon = NULL;

	if (!ngloglog_status)
		return;

	if (attacker == self)
	{
		slayname = self->client->pers.netname;
		ping = self->client->ping;
		diff = -1;

		if (mod == MOD_SUICIDE)
			wname = "Couldnt_Take_It_Anymore";
		else
			wname = attacker->client->pers.weapon
				? attacker->client->pers.weapon->pickup_name : NULL;

		sprintf (line, "%.1f\tSuicide\t%d\t%s\t%d\t%d", level.time,
			self->client->resp.clientid, wname, diff, ping);
		q2log_logWrite (line);
		return;
	}

	{
		qboolean	suicideflag = false;

		switch (mod)
		{
		case MOD_FALLING:
			wname = "Fell";
			suicideflag = true;
			break;
		case MOD_CRUSH:
			wname = "Crushed";
			suicideflag = true;
			break;
		case MOD_WATER:
			wname = "Drowned";
			suicideflag = true;
			break;
		case MOD_SLIME:
			wname = "Melted";
			suicideflag = true;
			break;
		case MOD_LAVA:
			wname = "Lava";
			suicideflag = true;
			break;
		case MOD_BOMB:
			wname = "Bomb";
			suicideflag = true;
			break;
		case MOD_EXPLOSIVE:
			wname = "Explosive";
			suicideflag = true;
			break;
		case MOD_BARREL:
			wname = "Barrel";
			suicideflag = true;
			break;
		case MOD_TARGET_LASER:
			wname = "Lasered";
			suicideflag = true;
			break;
		case MOD_TARGET_BLASTER:
			wname = "Blasted";
			suicideflag = true;
			break;
		case MOD_SPLASH:
			wname = "Splash";
			suicideflag = true;
			break;
		case MOD_TRIGGER_HURT:
			wname = "Trigger_Hurt";
			suicideflag = true;
			break;
		case MOD_EXIT:
			wname = "Exit";
			suicideflag = true;
			break;
		case MOD_SUICIDE:
			wname = "Couldnt_Take_It_Anymore";
			suicideflag = true;
			break;
		}

			// Every case sets `suicideflag` itself.
		if (suicideflag)
		{
			slayname = self->client->pers.netname;
			ping = self->client->ping;
			diff = -1;
			if (!wname)
				wname = "none";
			sprintf (line, "%.1f\tSuicide\t%d\t%s\t%d\t%d", level.time,
				self->client->resp.clientid, wname, diff, ping);
			q2log_logWrite (line);
			return;
		}
	}

	if (!slayname && attacker && attacker->client)
	{
		wname = "Unknown";
		switch (mod)
		{
		case MOD_BLASTER:
			wname = "Blaster";
			break;
		case MOD_SHOTGUN:
			wname = "Shotgun";
			break;
		case MOD_SSHOTGUN:
			wname = "Super Shotgun";
			break;
		case MOD_MACHINEGUN:
			wname = "Machinegun";
			break;
		case MOD_CHAINGUN:
			wname = "Chaingun";
			break;
		case MOD_GRENADE:
		case MOD_G_SPLASH:
			wname = "Grenade Launcher";
			break;
		case MOD_HANDGRENADE:
		case MOD_HG_SPLASH:
		case MOD_HELD_GRENADE:
			wname = "Grenades";
			break;
		case MOD_ROCKET:
		case MOD_R_SPLASH:
			wname = "Rocket Launcher";
			break;
		case MOD_HYPERBLASTER:
			wname = "HyperBlaster";
			break;
		case MOD_RAILGUN:
			wname = "Railgun";
			break;
		case MOD_BFG_LASER:
		case MOD_BFG_BLAST:
		case MOD_BFG_EFFECT:
			wname = "BFG10K";
			break;
		case MOD_TELEFRAG:
			wname = "Telefrag";
			break;
		case MOD_GRAPPLE:
			wname = "Hook";
			break;
		}

		victimname = self->client->pers.netname;
		slayname = attacker->client->pers.netname;
		ping = attacker->client->ping;
		theirping = self->client->ping;
		if (m_mode > 1 && (meansOfDeath & MOD_FRIENDLY_FIRE))
			diff = -1;
		else
			diff = 1;

		victimweapon = self->client->pers.weapon
			? self->client->pers.weapon->pickup_name : NULL;
		if (!victimweapon)
			victimweapon = "UNKNOWN";
	}

	if (m_mode > 1 && (meansOfDeath & MOD_FRIENDLY_FIRE))
		sprintf (line, "%.1f\tFratricide\t%d\t%s\t%d\t%d\t%d\t%s\t%d\t%d",
			level.time, attacker->client->resp.clientid, wname, diff,
			ping, self->client->resp.clientid, victimweapon, 0,
			theirping);
	else
		sprintf (line, "%.1f\tKill\t%d\t%s\t%d\t%d\t%d\t%s\t%d\t%d",
			level.time, attacker->client->resp.clientid, wname, diff,
			ping, self->client->resp.clientid, victimweapon, 0,
			theirping);

	q2log_logWrite (line);
}

// gamex86.dll: 10051ED6..10051F2C
// gamei386.so: 00072714..00072780
void q2log_logStart (void)
{
	if (ngloglog_status)
	{
		ngLog_logClose (0, NULL);
		ngloglog_status = 0;
	}

	if (!ngloglog_status && !q2log_init ())
		return;

	ngLog_initMark ();
	q2log_logStartHeader ();
	q2log_customStart ();
	q2log_logTime ();
	ngloglog_status = 2;
}

// gamex86.dll: 10051F2C..10052061
// gamei386.so: 00072780..00072873
void q2log_logStartHeader (void)
{
	char	text[128];
	cvar_t	*version;

	version = gi.cvar ("version", "3.20", CVAR_SERVERINFO | CVAR_NOSET);

	sprintf (text, "0.0\tinfo\tLog_Standard\t%s", "ngLog");
	q2log_logWrite (text);
	sprintf (text, "0.0\tinfo\tLog_Version\t%s", "1.2");
	q2log_logWrite (text);
	sprintf (text, "0.0\tinfo\tLog_Info_URL\t%s",
		"http://www.netgamesusa.com/ngLog/");
	q2log_logWrite (text);
	sprintf (text, "0.0\tinfo\tGame_Name\t%s", "Quake II");
	q2log_logWrite (text);
	sprintf (text, "0.0\tinfo\tGame_Version\t%s", version->string);
	q2log_logWrite (text);
	sprintf (text, "0.0\tinfo\tGame_Author\tid Software, Inc.");
	q2log_logWrite (text);
	sprintf (text, "0.0\tinfo\tGame_Author_URL\thttp://www.idsoftware.com");
	q2log_logWrite (text);
}

// gamex86.dll: 10052061..100525EA
// gamei386.so: 00072874..000730BD
int q2log_init (void)
{
	// Declarations WITH initialisers, not declarations then assignments.
	cvar_t	*port = gi.cvar ("port", "27910", CVAR_NOSET);
	cvar_t	*gamedir = gi.cvar ("gamedir", "tourney", 0);
	cvar_t	*basedir = gi.cvar ("basedir", ".", 0);
	cvar_t	*nglog_name = gi.cvar ("__dummy_nglog_name", NULL, 0);

	ngWorldStats_Status = gi.cvar ("ngWorldStats_Status", "<<< Disabled >>>",
		CVAR_SERVERINFO);
	gi.cvar_set ("ngWorldStats_Status", "<<< Disabled >>>");

	if (ngloglog_status && !__nglog_worldlog &&
		(int)nglog_logstyle->value != 4)
	{
		ngloglog_status = 2;
		return ngloglog_status;
	}

	if (!ngloglog_status)
	{
		nglog_logname = gi.cvar ("nglog_logname", "nglog.log", 0);
		nglog_flush = gi.cvar ("nglog_flush", "2", 0);
		nglog_buffer = gi.cvar ("nglog_buffer", "10", 0);
		nglog_logchat = gi.cvar ("nglog_logchat", "0", 0);
		nglog_logmiscpickup = gi.cvar ("nglog_logmiscpickup", "1", 0);
		gi.cvar_set ("nglog_logmiscpickup", "1");
		nglog_logallpickups = gi.cvar ("nglog_logallpickups", "0", 0);
		nglog_ngstats_exec = gi.cvar ("nglog_ngstats_exec", "1", 0);
		nglog_ngstats_browser = gi.cvar ("nglog_ngstats_browser", "0", 0);
		nglog_ngstats_cfg = gi.cvar ("nglog_ngstats_cfg", "ngStatsQ2T.cfg", 0);
		nglog_ngstats_logdir = gi.cvar ("nglog_ngstats_logdir", "logs", 0);
		nglog_ngstats_vidrestart = gi.cvar ("nglog_ngstats_vidrestart", "0", 0);

		if ((int)nglog_logstyle->value != 4)
		{
			if (!nglog_name || !nglog_name->string[0])
				gi.cvar_set ("__dummy_nglog_name", nglog_logname->string);
			else
				gi.cvar_set ("nglog_logname", nglog_name->string);
		}
	}

	nglog_worldstats = gi.cvar ("nglog_worldstats", "0", 0);

	if ((int)nglog_logstyle->value == 4)
	{
		gi.cvar_set ("nglog_logstyle_working", "4");
		strncpy (__nglog_ngstats_cfg, nglog_ngstats_cfg->string, 1023);
		__nglog_ngstats_cfg[1023] = '\0';
		strncpy (__nglog_ngstats_logdir, nglog_ngstats_logdir->string, 1023);
		__nglog_ngstats_logdir[1023] = '\0';
		__nglog_ngstats_exec = (int)nglog_ngstats_exec->value;
	}

	if (!(int)nglog_logstyle_working->value &&
		!(int)nglog_worldstats->value)
	{
		ngloglog_status = 0;
		return ngloglog_status;
	}

	if (!ngloglog_status)
	{
		if ((int)nglog_logstyle->value == 4)
		{
			__nglog_logstyle = (int)nglog_logstyle_working->value;
			__nglog_flush = (int)nglog_flush->value;
			__nglog_buffer = (int)nglog_buffer->value;
			sprintf (__nglog_logpath, "%s/%s/ngStats/%s/Q2_%s-",
				basedir->string, "NetGamesUSA.com",
				nglog_ngstats_logdir->string, gamedir->string);
		}
		else if ((int)nglog_logstyle->value &&
			(int)nglog_logstyle_working->value != atoi ("5"))
		{
			__nglog_logstyle = (int)nglog_logstyle_working->value;
			__nglog_flush = (int)nglog_flush->value;
			__nglog_buffer = (int)nglog_buffer->value;
			strcpy (__nglog_logname, nglog_logname->string);
		}
	}

	sprintf (__nglog_rel_path, "%s", "NetGamesUSA.com");
	sprintf (__nglog_worldlog_path, "%s/%s/ngWorldStats/logs/Q2_%s-",
		basedir->string, "NetGamesUSA.com", gamedir->string);

	/* Preserve the target's post-call basic block. */
q2log_worldlog_path_ready:
	__nglog_worldlog = (int)nglog_worldstats->value;
	strcpy (__nglog_worldlog_tag, port->string);

	if (ngLog_init ())
	{
		q2log_showErrors ();
		ngloglog_status = 0;
		return ngloglog_status;
	}

	if (!ngloglog_status && __nglog_logstyle == 3)
		gi.cvar_set ("nglog_logname", __nglog_logname);

	if ((int)nglog_logstyle_working->value != atoi ("5"))
	{
		q2log_showErrors ();
		if ((int)nglog_logstyle_working->value != 4 &&
			(int)nglog_logstyle->value)
		{
			gi.cvar_set ("nglog_logstyle_working", "5");
			__nglog_logstyle = (int)nglog_logstyle_working->value;
		}
	}

	ngloglog_status = 1;

	if ((int)nglog_worldstats->value)
		gi.cvar_set ("ngWorldStats_Status", "*** ENABLED ***");

	return ngloglog_status;
}

// gamex86.dll: 100525EA..1005263E
// gamei386.so: 000730C0..00073152
void q2log_logWrite (char *line)
{
	if (!ngloglog_status)
		return;

	if ((int)nglog_logstyle->value)
		ngLog_logWrite (line, 1);
	if ((int)nglog_worldstats->value)
		ngLog_logWrite (line, 2);
	q2log_showErrors ();
}

// gamex86.dll: 1005263E..10052680
// gamei386.so: 00073154..0007322D
void q2log_logTime (void)
{
	char	stamp[64];
	char	line[128];

	ngLog_getDateInfo (stamp, 1);
	sprintf (line, "0.0\tinfo\tAbsolute_Time\t%s", stamp);
	q2log_logWrite (line);
}

// gamex86.dll: 10052680..100526C2
// gamei386.so: 00073230..00073287
void q2log_showErrors (void)
{
	int		i;

	for (i = 0; i < __nglog_num_errs; i++)
		gi.dprintf ("%s", __nglog_error_msg[i]);
}

// gamex86.dll: 100526C2..10052760
// gamei386.so: 00073288..00073336
void q2log_clientid_cmd (edict_t *ent)
{
	if (gi.argc () == 1)
	{
		ent->client->resp.osp_r0b4[0] = '\0';
		gi.cprintf (ent, PRINT_CHAT,
			"You dont have an ngWorldStats Password!\n");
		gi.cprintf (ent, PRINT_CHAT,
			"Visit http://Quake2.ngWorldStats.com/FAQ for complete\n");
		gi.cprintf (ent, PRINT_CHAT,
			"info on how to setup for YOUR worldwide stats tracking!\n");
	}
	else
		strncpy (ent->client->resp.osp_r0b4, gi.argv (1), 16);

	q2log_playerConnect (ent);
	ent->client->resp.osp_r2a8 = 1;
}
