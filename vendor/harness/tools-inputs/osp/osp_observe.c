// osp_observe.c -- <INVENTED FILENAME>. The chasecam / observer entry points.
//
// OSP_ChaseCam, OSP_startObserve and OSP_removeChaseCam.  They share the
// join/leave sequence with p_camera.c's CameraCmd almost line for line.

#include "g_local.h"

// gamex86.dll: 1001C0E0..1001C72D
// gamei386.so: 00054420..00054969
void OSP_ChaseCam (edict_t *ent)
{
	// Two separate edict pointers, not one reused across both loops.
	gclient_t	*clp;
	edict_t		*p;
	edict_t		*ep;
	int			t;

	clp = ent->client;

	if (level.intermissiontime != 0)
		return;

	if (match_paused && m_mode > 1 && clp->resp.entered != ENTERED_ENTERED)
	{
		gi.cprintf (ent, PRINT_HIGH,
			"Sorry, cannot join teams during a paused match.\n");
		return;
	}

	if (clp->resp.entered == ENTERED_ENTERED &&
		ent->health < 100 && ent->health > 0)
	{
		gi.cprintf (ent, PRINT_HIGH,
			"Cannot go to spectator mode while injured.\n");
		return;
	}

	// Already chasing -> this is the "rejoin the game" half, the same sequence
	// CameraCmd runs when it leaves camera mode.
	if (clp->chase_target)
	{
		if (m_mode == 3 && !OSP_1v1AllowJoin (ent))
			return;

		if (!clp->resp.osp_r030 || m_mode == 3)
		{
			if (m_mode > 1 && !OSP_addTeamMember (ent, 2))
				return;

			clp->resp.osp_r030 = 1;
			clp->resp.enterframe = level.framenum;
		}
		else
		{
			if (m_mode > 1 && !OSP_readdTeamMember (ent))
				return;

			if (clp->resp.osp_r030)
				clp->resp.enterframe = level.framenum - clp->resp.osp_r2d4;
		}

		clp->chase_target = NULL;
		clp->update_chase = false;
		clp->osp_t03c = NULL;
		clp->resp.entered = ENTERED_ENTERED;
		clp->resp.osp_r240 = 0;
		clp->resp.score = clp->resp.osp_r248;
		clp->resp.osp_r0a0--;
		clp->resp.osp_r09c--;
		active_clients++;

		if (m_mode > 0 && sync_stat < 4)
		{
			clp->resp.osp_r010 -= 2;
			OSP_notready_cmd (ent, true);
		}

		gi.bprintf (PRINT_HIGH, "%s entered the game (clients = %d)\n",
			clp->pers.netname, active_clients);
		EntityListAdd (ent);
		OSP_DoRankSort ();
		q2log_playerEntered (ent);
		return;
	}

	if (clp->resp.entered == ENTERED_ENTERED && !clp->resp.osp_r240)
		return;

	for (t = 1; t <= maxclients->value; t++)
	{
		p = g_edicts + t;

		if (p->inuse && p->solid && p != ent && p->client &&
			p->client->resp.entered == ENTERED_ENTERED)
		{
			if (rune_stat)
				OSP_deadDropRune (ent);

			VectorSet (ent->movedir, 0, 0, 0);
			ent->speed = camera_depth->value;
			clp->osp_t018 = 0;
			clp->chase_target = p;
			p->client->resp.osp_r000++;
			clp->update_chase = true;
			clp->osp_t03c = NULL;
			ent->waterlevel = 0;
			ent->watertype = 0;
			ent->svflags |= SVF_NOCLIENT;
			ent->solid = SOLID_NOT;
			ent->movetype = MOVETYPE_FLYMISSILE;
			clp->osp_t040 = 0;
			clp->ps.gunindex = 0;

			if (clp->resp.osp_r030 && clp->resp.entered == ENTERED_ENTERED)
			{
				clp->resp.osp_r248 = clp->resp.score;

				if (m_mode > 1 && clp->resp.team != 2)
					OSP_removeTeamMember (ent, false);
			}

			ent->deadflag = DEAD_NO;
			clp->resp.osp_r2dc = 0;
			clp->resp.score = -100;
			clp->resp.osp_r0a0--;
			clp->resp.osp_r09c--;
			clp->resp.osp_r000 = 0;
			clp->resp.osp_r2d4 = level.framenum - clp->resp.enterframe;

			if (sync_stat < 4 && clp->resp.entered == ENTERED_ENTERED)
				OSP_notready_cmd (ent, true);

			if (clp->resp.entered == ENTERED_ENTERED)
			{
				active_clients--;
				EntityListRemove (ent);

				if (m_mode == 3)
					OSP_1v1Remove (ent, false);
			}

			clp->resp.entered = 4;
			clp->resp.osp_r240 = 0;
			clp->menu = NULL;
			clp->inmenu = false;

			if (m_mode > 1)
				OSP_checkHalt (clp->resp.osp_r2cc);
			else if (m_mode == 1)
				OSP_checkHalt (2);

			OSP_DoRankSort ();
			break;
		}
	}

	if (!clp->chase_target)
	{
		gi.cprintf (ent, PRINT_HIGH, "No clients to chase.\n");
		return;
	}

	for (t = 1; t <= game.maxclients; t++)
	{
		ep = g_edicts + t;

		if (!ep->inuse || !ep->client ||
			ep->client->chase_target != ent)
			continue;

		gi.cprintf (ep, PRINT_HIGH, "Target switched to chasecam mode.\n");
		OSP_removeChaseCam (ep);
	}

	OSP_observerTeamFrags (ent);
	q2log_playerMode (ent, "Chasecam");
}

// gamex86.dll: 1001C72D..1001CA58
// gamei386.so: 0005496C..00054BFD
void OSP_startObserve (edict_t *ent)
{
	gclient_t	*cl;

	cl = ent->client;

	if (level.intermissiontime != 0)
		return;

	// Two independent top-level tests, each re-testing `entered`, not one
	// outer `if` with two arms.  Identical to CameraCmd's join sequence.
	if (cl->resp.entered == ENTERED_ENTERED && !cl->resp.osp_r240)
		return;

	if (cl->resp.entered == ENTERED_ENTERED && ent->health < 100 &&
		ent->health > 0 && sync_stat > 2)
	{
		gi.cprintf (ent, PRINT_HIGH,
			"Cannot go to spectator mode while injured.\n");
		return;
	}

	if (match_paused && m_mode > 1 && cl->resp.entered != ENTERED_ENTERED)
	{
		gi.cprintf (ent, PRINT_HIGH,
			"Sorry, cannot join teams during a paused match.\n");
		return;
	}

	if (cl->resp.entered == 2)
	{
		if (m_mode == 3 && !OSP_1v1AllowJoin (ent))
			return;

		if (!cl->resp.osp_r030 || m_mode == 3)
		{
			if (m_mode > 1 && !OSP_addTeamMember (ent, 2))
				return;

			cl->resp.osp_r030 = 1;
			cl->resp.enterframe = level.framenum;
		}
		else
		{
			if (m_mode > 1 && !OSP_readdTeamMember (ent))
				return;

			if (cl->resp.osp_r030)
				cl->resp.enterframe = level.framenum - cl->resp.osp_r2d4;
		}

		ent->deadflag = DEAD_NO;
		cl->chase_target = NULL;
		cl->update_chase = false;
		cl->osp_t03c = NULL;
		cl->resp.entered = ENTERED_ENTERED;
		cl->resp.osp_r240 = 0;
		cl->resp.osp_r2dc = 0;
		cl->resp.score = cl->resp.osp_r248;
		cl->resp.osp_r0a0--;
		cl->resp.osp_r09c--;
		active_clients++;

		if (m_mode > 0 && sync_stat < 4)
		{
			cl->resp.osp_r010 -= 2;
			OSP_notready_cmd (ent, true);
		}

		gi.bprintf (PRINT_HIGH, "%s entered the game (clients = %d)\n",
			cl->pers.netname, active_clients);
		EntityListAdd (ent);
		OSP_DoRankSort ();
		q2log_playerEntered (ent);
	}
	else
	{
		if (sync_stat < 4)
		{
			OSP_notready_cmd (ent, true);
			OSP_CheckReady ();
		}

		if (rune_stat)
			OSP_deadDropRune (ent);

		OSP_observerTeamFrags (ent);
		cl->resp.osp_r2d4 = level.framenum - cl->resp.enterframe;
		cl->resp.osp_r000 = 0;
		cl->menu = NULL;
		cl->inmenu = false;
		OSP_removeChaseCam (ent);
	}
}

// gamex86.dll: 1001CA58..1001CD50
// gamei386.so: 00054C00..00054E7E
void OSP_removeChaseCam (edict_t *ent)
{
	gclient_t	*client;
	edict_t		*ee;
	int			x;
	int			was;

	client = ent->client;

	if (level.intermissiontime != 0)
		return;

	gi.cprintf (ent, PRINT_HIGH, "Changing to OBSERVER mode.\n");
	client->chase_target = NULL;
	client->update_chase = false;

	if (sync_stat > 2 && m_mode < 2)
		client->ps.stats[20] = 0;

	OSP_zeroRuneStats (ent);
	ent->movetype = MOVETYPE_NOCLIP;
	ent->clipmask = 0;
	ent->solid = SOLID_NOT;
	ent->waterlevel = 0;
	ent->watertype = 0;
	ent->svflags |= SVF_NOCLIENT;
	client->resp.osp_r2bc = 1;
	client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
	client->osp_t040 = 0;
	client->osp_t03c = NULL;
	client->latched_buttons &= ~BUTTON_ATTACK;
	client->ps.gunindex = 0;

	if (client->resp.osp_r030 && client->resp.entered == ENTERED_ENTERED)
	{
		client->resp.osp_r248 = client->resp.score;

		if (m_mode > 1 && client->resp.team != 2)
			OSP_removeTeamMember (ent, false);

		if (m_mode == 3)
			OSP_1v1Remove (ent, false);
	}

	client->resp.score = -100;
	client->resp.osp_r0a0--;
	client->resp.osp_r09c--;

	if (client->resp.entered == ENTERED_ENTERED)
	{
		active_clients--;
		EntityListRemove (ent);
	}

	was = client->resp.entered;
	client->resp.entered = 2;
	client->resp.osp_r240 = 0;
	OSP_DoRankSort ();

	if (sync_stat < 4)
	{
		client->resp.osp_r20c = 0;
		OSP_CheckReady ();
	}

	if (was == ENTERED_ENTERED)
	{
		for (x = 1; x <= game.maxclients; x++)
		{
			ee = g_edicts + x;

			if (!ee->inuse || !ee->client || ee->client->chase_target != ent)
				continue;

			gi.cprintf (ee, PRINT_HIGH, "Target switched to observer mode.\n");
			OSP_removeChaseCam (ee);
		}

		if (m_mode > 1)
			OSP_checkHalt (client->resp.osp_r2cc);
		else if (m_mode == 1)
			OSP_checkHalt (2);
	}

	q2log_playerMode (ent, "Observe");
}
