
#include "g_local.h"
#include "bl_main.h"

int	endlvl_frame = 0;
int	end_timeout = -1;
int	match_paused = 0;
int	ot_count = 0;
float	pause_time = 0;


game_locals_t	game;
level_locals_t	level;
game_import_t	gi;
game_export_t	globals;
spawn_temp_t	st;

int	sm_meat_index;
int	snd_fry;
int meansOfDeath;

edict_t		*g_edicts;

cvar_t	*deathmatch;
cvar_t	*coop;
cvar_t	*dmflags;
cvar_t	*skill;
cvar_t	*fraglimit;
cvar_t	*timelimit;
cvar_t	*password;
cvar_t	*maxclients;
cvar_t	*maxentities;
cvar_t	*g_select_empty;
cvar_t	*dedicated;

cvar_t	*filterban;

cvar_t	*sv_maxvelocity;
cvar_t	*sv_gravity;

cvar_t	*sv_rollspeed;
cvar_t	*sv_rollangle;
cvar_t	*gun_x;
cvar_t	*gun_y;
cvar_t	*gun_z;

cvar_t	*run_pitch;
cvar_t	*run_roll;
cvar_t	*bob_up;
cvar_t	*bob_pitch;
cvar_t	*bob_roll;

cvar_t	*sv_cheats;

cvar_t	*flood_msgs;
cvar_t	*flood_persecond;
cvar_t	*flood_waitdelay;

void SpawnEntities (char *mapname, char *entities, char *spawnpoint);
void ClientThink (edict_t *ent, usercmd_t *cmd);
qboolean ClientConnect (edict_t *ent, char *userinfo);
void ClientUserinfoChanged (edict_t *ent, char *userinfo);
void ClientDisconnect (edict_t *ent);
void ClientBegin (edict_t *ent);
void ClientCommand (edict_t *ent);
void RunEntity (edict_t *ent);
void WriteGame (char *filename, qboolean autosave);
void ReadGame (char *filename);
void WriteLevel (char *filename);
void ReadLevel (char *filename);
void InitGame (void);
void G_RunFrame (void);


//===================================================================


// gamex86.dll: 10015960..10015B51
// gamei386.so: 00022254..00022534
void ShutdownGame (void)
{
	char	reason[128];

	BotUnloadAllLibraries ();
	gi.dprintf ("==== ShutdownGame ====\n");

	sl_GameEnd (&gi, level);

	if (!level.intermissiontime)
		q2log_logAccuracy ();

	if (gi.argc ())
	{
		if (strcmp (gi.argv (0), "map") != 0)
		{
			if ((int)nglog_ngstats_browser->value &&
				(int)nglog_logstyle->value == 4)
				q2log_gameEnd (gi.argv (0), 1);
			else
				q2log_gameEnd (gi.argv (0), 0);

			strcpy (reason, gi.argv (0));
		}
		else
		{
			q2log_gameEnd ("manual_map", 0);
			strcpy (reason, "manual_map");
		}
	}
	else
	{
		if ((int)nglog_ngstats_browser->value &&
			(int)nglog_logstyle->value == 4)
			q2log_gameEnd ("server", 1);
		else
			q2log_gameEnd ("server", 0);

		strcpy (reason, "server");
	}

	if (server_log)
	{
		char		date[32];
		time_t		now;
		struct tm	*tm;

		time (&now);
		tm = localtime (&now);
		sprintf (date, "%.19s", asctime (tm));
		OSP_logAdminLog ("Shutdown: %s (%s)", reason, date);
	}

	gi.FreeTags (TAG_LEVEL);
	gi.FreeTags (TAG_GAME);
}


/*
=================
GetGameAPI

Returns a pointer to the structure with all entry points
and global variables
=================
*/
// gamex86.dll: 10015B51..10015C22
// gamei386.so: 00022534..00022669
game_export_t *GetGameAPI (game_import_t *import)
{
	gi = *import;

	BotRedirectGameImport ();
	Swap_Init ();

	globals.apiversion = GAME_API_VERSION;
	globals.Init = InitGame;
	globals.Shutdown = ShutdownGame;
	globals.SpawnEntities = SpawnEntities;

	globals.WriteGame = WriteGame;
	globals.ReadGame = ReadGame;
	globals.WriteLevel = WriteLevel;
	globals.ReadLevel = ReadLevel;

	globals.ClientThink = ClientThink;
	globals.ClientConnect = ClientConnect;
	globals.ClientUserinfoChanged = ClientUserinfoChanged;
	globals.ClientDisconnect = ClientDisconnect;
	globals.ClientBegin = ClientBegin;
	globals.ClientCommand = ClientCommand;

	globals.RunFrame = G_RunFrame;

	globals.ServerCommand = ServerCommand;

	globals.edict_size = sizeof(edict_t);

	return &globals;
}

#ifndef GAME_HARD_LINKED
// this is only here so the functions in q_shared.c and q_shwin.c can link
// gamex86.dll: 10015C22..10015C73
// gamei386.so: 0002266C..000226B5
void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	gi.error (ERR_FATAL, "%s", text);
}

// gamex86.dll: 10015C73..10015CC2
// gamei386.so: 000226B8..000226FF
void Com_Printf (char *msg, ...)
{
	va_list		argptr;
	char		text[1024];

	va_start (argptr, msg);
	vsprintf (text, msg, argptr);
	va_end (argptr);

	gi.dprintf ("%s", text);
}

#endif

//======================================================================


/*
=================
ClientEndServerFrames
=================
*/
// gamex86.dll: 10015CC2..10015D26
// gamei386.so: 00022700..0002277F
void ClientEndServerFrames (void)
{
	int		i;
	edict_t	*ent;

	// calc the player views now that all pushing
	// and damage has been added
	for (i=1 ; i<=maxclients->value ; i++)
	{
		ent = g_edicts + i;
		if (!ent->inuse || !ent->client)
			continue;
		ClientEndServerFrame (ent);
	}
}

// vanilla's CreateTargetChangeLevel helper is gone from the real source:
// each of the three sites in EndDMLevel is G_Spawn / classname / map written
// out.


/*
=================
EndDMLevel

The timelimit or fraglimit has been exceeded
=================
*/
// gamex86.dll: 10015D26..10015E29
// gamei386.so: 00022780..000228A2
void EndDMLevel (void)
{
	edict_t		*ent;

	ent = NULL;
	EnitityListClean ();
	endlvl_frame = level.framenum;

	if (hs_mode && !manual_map)
		OSP_updateHighScores ();

	// stay on same level flag
	if (((int)dmflags->value & DF_SAME_LEVEL) && manual_map != 1)
	{
		ent = G_Spawn ();
		ent->classname = "target_changelevel";
		ent->map = level.mapname;
	}

	if (!ent)
	{
		ent = NextMap ();

		if (!ent)
		{
			if (level.nextmap[0])		// go to a specific map
			{
				ent = G_Spawn ();
				ent->classname = "target_changelevel";
				ent->map = level.nextmap;
			}
			else
			{	// search for a changelevel
				ent = G_Find (NULL, FOFS(classname), "target_changelevel");

				if (!ent)
				{	// the map designer didn't include a changelevel, so create
					// a fake ent that goes back to the same level
					ent = G_Spawn ();
					ent->classname = "target_changelevel";
					ent->map = level.mapname;
				}
			}

		}
	}

	BeginIntermission (ent);
}

/*
=================
CheckDMRules
=================
*/
// gamex86.dll: 10015E29..100161D0
// gamei386.so: 000228A4..00022CB0
void CheckDMRules (void)
{
	int			i;

	if (level.intermissiontime)
		return;

	if (timelimit->value)
	{
		if (sync_stat > 2)
		{
			if (level.time - sync_time >=
				(timelimit->value + overtime_timer) * 60 &&
				!frag_offset)
			{
				if (m_mode > 1)
				{
					if (teams[0].osp_m0f8 == teams[1].osp_m0f8)
					{
						if (OSP_overtimeWork (ot_count))
						{
							ot_count++;
							return;
						}
					}

					OSP_findTeamWinner ();
				}

				ot_count = 0;
				gi.bprintf (PRINT_HIGH, "Timelimit hit.\n");
				sl_SoftGameEnd (&gi, level);
				q2log_logAccuracy ();
				if (!overtime_timer)
					q2log_gameEnd ("timelimit", 0);
				else
					q2log_gameEnd ("overtime timelimit", 0);
				EndDMLevel ();
				return;
			}
			goto fraglimit_check;
		}
	}

	if (connected_clients - botglobals.numbots <= 0 &&
		level.time > 3600)
	{
		ot_count = 0;
		gi.bprintf (PRINT_HIGH, "Inactive client timelimit hit.\n");
		sl_SoftGameEnd (&gi, level);
		q2log_gameEnd ("inactive client timelimit", 0);
		EndDMLevel ();
		return;
	}

fraglimit_check:
	if (fraglimit->value || frag_offset)
	{
		if (m_mode < 2)
		{
			for (i = 0; i < maxclients->value; i++)
			{
				gclient_t	*cl;

				cl = game.clients + i;
				if (!g_edicts[i+1].inuse)
					continue;

				if (cl->resp.score >= fraglimit->value)
				{
					gi.bprintf (PRINT_HIGH, "Fraglimit hit.\n");
					sl_SoftGameEnd (&gi, level);
					q2log_logAccuracy ();
					q2log_gameEnd ("fraglimit", 0);
					EndDMLevel ();
					return;
				}
			}
		}
		else if (frag_offset)
		{
			if (teams[0].osp_m0f8 != teams[1].osp_m0f8)
			{
				OSP_findTeamWinner ();
				gi.bprintf (PRINT_HIGH, "We have a sudden-death winner!\n");
				sl_SoftGameEnd (&gi, level);
				q2log_logAccuracy ();
				q2log_gameEnd ("sudden death fraglimit", 0);
				EndDMLevel ();
				return;
			}
		}
		else
		{
			if (teams[0].osp_m0f8 >= fraglimit->value + frag_offset ||
				teams[1].osp_m0f8 >= fraglimit->value + frag_offset)
			{
				OSP_findTeamWinner ();
				gi.bprintf (PRINT_HIGH, "Team fraglimit hit.\n");
				sl_SoftGameEnd (&gi, level);
				q2log_logAccuracy ();
				q2log_gameEnd ("team fraglimit", 0);
				EndDMLevel ();
			}
		}
	}
}


/*
=============
ExitLevel
=============
*/
// gamex86.dll: 100161D0..10016302
// gamei386.so: 00022CB0..00022E26
void ExitLevel (void)
{
	int		i;
	edict_t	*ent;
	char	command [256];

	Com_sprintf (command, sizeof(command), "gamemap \"%s\"\n", level.changemap);
	gi.AddCommandString (command);
	level.changemap = NULL;
	level.exitintermission = 0;
	level.intermissiontime = 0;
	ClientEndServerFrames ();

	// clear some things before going to next level
	for (i=0 ; i<maxclients->value ; i++)
	{
		ent = g_edicts + 1 + i;
		if (!ent->inuse)
			continue;
		if (ent->health > ent->client->pers.max_health)
			ent->health = ent->client->pers.max_health;
		ent->client->resp.score = ent->client->pers.score = 0;
		PlayerResetGrapple (ent);
	}

}

/*
================
G_RunFrame

Advances the world by 0.1 seconds
================
*/
// gamex86.dll: 10016302..10017330
// gamei386.so: 00022E28..00024162
void G_RunFrame (void)
{
	int		i;
	edict_t	*ent;

	if (match_paused < 2)
	{
		level.framenum++;
		level.time = level.framenum*FRAMETIME;

		if ((int)console_timestamp->value &&
			console_stampcount < level.framenum)
		{
			console_stampcount = level.framenum +
				(int)console_timestamp->value * 600;
			OSP_consoleStamp ();
		}

		if (level.framenum == 25 && bots_botfile->string &&
			bots_loadstat == 1)
		{
			char	command[256];

			gi.bprintf (PRINT_HIGH, "Loading bots...\n");
			Com_sprintf (command, sizeof(command), " exec %s\n",
						 bots_botfile->string);
			gi.AddCommandString (command);
		}

		if (!level.intermissiontime)
			OSP_updateClock ();

		if (vote_inprogress && level.framenum > vote_frametime &&
			!level.intermissiontime)
		{
			gi.bprintf (PRINT_HIGH, "Time up. Vote failed. No changes made.\n");
			q2log_voteInfo ("Fail", 0, 0);
			OSP_clearVotes ();
			OSP_closeMenus ();
		}

		if (m_mode > 1)
			OSP_updateTeamFrags ();

		if (sync_stat < 4 && !level.intermissiontime)
			OSP_checkSync ();

		if (level.exitintermission)
		{
			OSP_serverbotsRemove ();
			ot_count = 0;

			if (manual_map == 2)
			{
				char	command[256];
				edict_t	*ent;

				OSP_loadMaps ();
				ent = NextMap ();
				if (ent)
					Com_sprintf (command, sizeof(command), "map %s\n", ent->map);
				else
					Com_sprintf (command, sizeof(command), "map %s\n", level.mapname);
				gi.AddCommandString (command);
				return;
			}

			if (((int)dmflags->value & DF_SAME_LEVEL) && manual_map != 1 &&
				level.framenum < 64000 &&
				connected_clients - botglobals.numbots > 0)
			{
				OSP_endClean ();
				level.changemap = NULL;
				level.exitintermission = 0;
				level.intermissiontime = 0;
				ClientEndServerFrames ();
				botglobals.numbots = 0;
				q2log_gameInit (1);
				sl_GameStart (&gi, level);
				if (m_mode < 1)
					q2log_gameStart ();
				OSP_consoleStamp ();

				for (i = 0; i < maxclients->value; i++)
				{
					ent = g_edicts + 1 + i;
					if (!ent->inuse)
						continue;
					if (ent->health > ent->client->pers.max_health)
						ent->health = ent->client->pers.max_health;
					ent->client->resp.score = ent->client->pers.score = 0;
					ent->client->resp.osp_r030 = 0;
					PlayerResetGrapple (ent);
					ClientBegin (ent);
				}

				ent = g_edicts + ((int)maxclients->value + 1);
				for (i = (int)(maxclients->value + 1); i < globals.num_edicts;
					 i++, ent++)
				{
					if (!ent->inuse || !ent->think)
						continue;
					if ((!ent->team || ent == ent->teammaster) &&
						!OSP_disableItems (ent))
						ent->nextthink = level.time - 1.0;
				}
				return;
			}

			if ((int)vote_config_default->value &&
				vote_config_defaultname->string &&
				strcmp (vote_config_defaultname->string, "default") &&
				strcmp (__current_config->string, "default") &&
				!(connected_clients - botglobals.numbots))
			{
				char	command[256];

				OSP_endClean ();
				gi.cvar_set ("__current_config", "default");
				gi.dprintf ("Changing back to default config: %s\n",
							vote_config_defaultname->string);
				Com_sprintf (command, sizeof(command), "exec %s\n",
							 vote_config_defaultname->string);
				gi.AddCommandString (command);
				Com_sprintf (command, sizeof(command), "map %s\n", level.mapname);
				gi.AddCommandString (command);
				return;
			}

			OSP_endClean ();
			ExitLevel ();
			return;
		}

		AddQueuedBots ();
		BotLib_BotStartFrame (level.time);

		ent = g_edicts;
		for (i=0 ; i<globals.num_edicts && ent ; i++, ent++)
		{
			vec3_t	forward;
			vec3_t	right;
			vec3_t	offset;
			vec3_t	start;

			if (!ent->inuse || !ent->classname)
				continue;

			level.current_entity = ent;

			if (ent->classname && !strncmp (ent->classname, "hook", 4))
			{
				if (ent != ent->owner->client->grapple || !ent->owner->inuse)
				{
					G_FreeEdict (ent);
					continue;
				}

				AngleVectors (ent->owner->client->v_angle, forward, right, NULL);
				VectorSet (offset, 24, 8, ent->owner->viewheight - 8);
				P_ProjectSource (ent->owner->client, ent->owner->s.origin, offset,
								 forward, right, start);
				VectorSubtract (start, ent->owner->s.origin, offset);
				VectorAdd (offset, ent->owner->s.origin, ent->s.old_origin);
			}
			else if (!(ent->flags & FL_OLDORGNOTSET))
				VectorCopy (ent->s.origin, ent->s.old_origin);

			if ((ent->groundentity) && (ent->groundentity->linkcount != ent->groundentity_linkcount))
				ent->groundentity = NULL;

			if (i > 0 && i <= maxclients->value)
			{
				ClientBeginServerFrame (ent);
				continue;
			}

			G_RunEntity (ent);
		}

		if (botglobals.numbots)
		{
			ent = g_edicts;
			for (i = 0; i < globals.num_edicts && ent; i++, ent++)
			{
				if (!ent->inuse)
					continue;
				if (!(ent->svflags & SVF_NOCLIENT))
					BotLib_BotUpdateEntity (ent);
			}
		}

		for (i = 0; i < maxclients->value; i++)
		{
			ent = g_edicts + (i + 1);
			if (ent->inuse && (ent->flags & FL_BOT) && BotStarted (ent))
			{
				BotLib_BotUpdateClient (ent);
				BotLib_BotAI (ent, FRAMETIME);
				BotExecuteInput (ent);
			}
		}

		if (bots_loadstat > 1 && (int)bots_minplayers->value &&
			(((level.framenum > bots_delaytime) && (int)dedicated->value) ||
			 ((level.framenum > 25) && !(int)dedicated->value)) &&
			!level.intermissiontime && !level.exitintermission)
			CheckMinimumPlayers ();

		CheckDMRules ();

		ClientEndServerFrames ();

		if (match_paused == 1)
		{
			match_paused = 2;

			if (who_paused == -1 || who_paused == -3)
				gi.configstring (0x621, "Pause");
			else if (who_paused == -2)
				gi.configstring (0x621, " Wait");

			{
				edict_t	*ent;

				for (i = 1; i <= game.maxclients; i++)
				{
					ent = g_edicts + i;
					if (!ent->inuse || !ent->client)
						continue;

					ent->client->ps.pmove.pm_type = PM_FREEZE;
					ent->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
				}
			}
		}
		return;
	}

	if (level.intermissiontime)
	{
		edict_t	*ent;

		match_paused = 0;
		who_paused = -1;
		end_timeout = -1;

		for (i = 1; i <= game.maxclients; i++)
		{
			ent = g_edicts + i;
			if (!ent->inuse || !ent->client)
				continue;

			ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
		}
		return;
	}

	if (match_paused == 3)
	{
		// message[64], and the per-iteration `command` is 32 bytes, not 48.
		char	message[64];
		char	command[32];
		edict_t	*ent;

		if (end_timeout == -1)
			end_timeout = 51;
		end_timeout--;

		if (!end_timeout)
		{
			edict_t	*ent;

			match_paused = 0;
			who_paused = -1;
			for (i = 1; i <= game.maxclients; i++)
			{
				ent = g_edicts + i;
				if (!ent->inuse || !ent->client)
					continue;

				ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
			}
			gi.bprintf (PRINT_CHAT, "**** MATCH HAS RESTARTED!! ****\n");
			end_timeout = -1;
			return;
		}

		if (!(end_timeout % 10))
		{
			if (end_timeout / 10 != 1)
				sprintf (message, "Match restarting in %d seconds.\n",
						 end_timeout / 10);
			else
				sprintf (message, "Match restarting in %d second!\n",
						 end_timeout / 10);
			for (i = 1; i <= game.maxclients; i++)
			{
				ent = g_edicts + i;
				if (!ent->inuse || !ent->client)
					continue;
				gi.centerprintf (ent, "%s", message);
				sprintf (command, "play misc/secret.wav");
				gi.WriteByte (svc_stufftext);
				gi.WriteString (command);
				gi.unicast (ent, false);
			}
		}
		return;
	}

	if (who_paused == -1)
	{
		ClientEndServerFrames ();
		return;
	}

	// Each of the three pause countdowns truncates pause_time ONCE into its own
	// int local and reuses it.  (`secs` is an invented name.)
	if (who_paused == -3)
	{
		int		secs = (int)pause_time;

		if (pause_time - secs < FRAMETIME && !(secs % 10))
		{
			edict_t	*ent;

			for (i = 1; i <= game.maxclients; i++)
			{
				ent = g_edicts + i;
				if (!ent->inuse || !ent->client)
					continue;

				gi.centerprintf (ent,
					"Admin is viewing ngStats.  Please Wait.\n");
			}
		}
		pause_time -= FRAMETIME;
		ClientEndServerFrames ();
		return;
	}

	if (who_paused == -2)
	{
		int		secs = (int)pause_time;

		if (pause_time - secs < FRAMETIME && !(secs % 10))
		{
			char	message[128];

			sprintf (message, "Waiting for %s to reconnect.\n(%d seconds)\n",
					 reconn_player, secs);
			for (i = 1; i <= game.maxclients; i++)
			{
				ent = g_edicts + i;
				if (!ent->inuse || !ent->client)
					continue;

				gi.centerprintf (ent, message);
			}
		}
		pause_time -= FRAMETIME;
		if (pause_time < FRAMETIME)
		{
			edict_t	*ent;

			match_paused = 0;
			who_paused = -1;
			for (i = 1; i <= game.maxclients; i++)
			{
				ent = g_edicts + i;
				if (!ent->inuse || !ent->client ||
					ent->client->resp.entered > 2)
					continue;

				ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
			}
			gi.bprintf (PRINT_HIGH, "No reconnect. Match terminated.\n");
			OSP_checkHalt (reconn_index);
		}
		ClientEndServerFrames ();
		return;
	}

	{
		int		secs = (int)pause_time;

		if (pause_time - secs < FRAMETIME)
		{
			char	message[8];

			sprintf (message, "TO %.2d", secs);
			gi.configstring (0x621, message);
		}
	}
	pause_time -= FRAMETIME;
	if (pause_time < FRAMETIME)
		match_paused = 3;
	ClientEndServerFrames ();
}
