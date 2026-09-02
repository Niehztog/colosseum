/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// OSP Tourney DM v2.75, from osp-tourney@1d8427e (doc/provenance.md).
// Donor-only: baseq2 has no counterpart, so it lives in src/tourney/ rather
// than being merged into a spine file (R-CORE-7).  The reconstruction's
// asm-matching address comments are stripped -- SPECS.md N1 makes those oracles
// meaningless here, and they survive at the pin.
// osp_observe.c -- <INVENTED FILENAME>. The chasecam / observer entry points.
//
// OSP_ChaseCam, OSP_startObserve and OSP_removeChaseCam.  They share the
// join/leave sequence with p_camera.c's CameraCmd almost line for line.

#include "g_local.h"
#include "tourney/osp_types.h"
#include "tourney/osp_stats.h"

void OSP_ChaseCam(edict_t *ent)
{
    // Two separate edict pointers, not one reused across both loops.
    gclient_t   *clp;
    edict_t     *p;
    edict_t     *ep;
    int         t;

    clp = ent->client;

    if (level.intermission_framenum != 0)
        return;

    if (match_paused && OSP_IsTeams() && clp->resp.osp_entered != ENTERED_ENTERED) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Sorry, cannot join teams during a paused match.\n");
        return;
    }

    if (clp->resp.osp_entered == ENTERED_ENTERED &&
        ent->health < 100 && ent->health > 0) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Cannot go to spectator mode while injured.\n");
        return;
    }

    // Already chasing -> this is the "rejoin the game" half, the same sequence
    // CameraCmd runs when it leaves camera mode.
    if (clp->chase_target) {
        if (G_Ruleset() == RULESET_DUEL && !OSP_1v1AllowJoin(ent))
            return;

        if (!clp->resp.osp_r030 || G_Ruleset() == RULESET_DUEL) {
            if (OSP_IsTeams() && !OSP_addTeamMember(ent, 2))
                return;

            clp->resp.osp_r030 = 1;
            clp->resp.enterframe = level.framenum;
        } else {
            if (OSP_IsTeams() && !OSP_readdTeamMember(ent))
                return;

            if (clp->resp.osp_r030)
                clp->resp.enterframe = level.framenum - clp->resp.osp_r2d4;
        }

        clp->chase_target = NULL;
        clp->update_chase = false;
        clp->osp_t03c = NULL;
        clp->resp.osp_entered = ENTERED_ENTERED;
        clp->resp.osp_r240 = 0;
        clp->resp.score = clp->resp.osp_r248;
        clp->resp.osp_r0a0--;
        clp->resp.osp_r09c--;
        active_clients++;

        if (OSP_IsMatch() && sync_stat < 4) {
            clp->resp.osp_r010 -= 2;
            OSP_notready_cmd(ent, true);
        }

        gi.bprintf(PRINT_HIGH, "%s entered the game (clients = %d)\n",
                   clp->pers.netname, active_clients);
        EntityListAdd(ent);
        OSP_DoRankSort();
        OSP_Stats_PlayerEnter(ent);
        return;
    }

    if (clp->resp.osp_entered == ENTERED_ENTERED && !clp->resp.osp_r240)
        return;

    for (t = 1; t <= game.maxclients; t++) {
        p = g_edicts + t;

        if (p->inuse && p->solid && p != ent && p->client &&
            p->client->resp.osp_entered == ENTERED_ENTERED) {
            if (rune_stat)
                OSP_deadDropRune(ent);

            VectorSet(ent->movedir, 0, 0, 0);
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

            if (clp->resp.osp_r030 && clp->resp.osp_entered == ENTERED_ENTERED) {
                clp->resp.osp_r248 = clp->resp.score;

                if (OSP_IsTeams() && clp->resp.team != 2)
                    OSP_removeTeamMember(ent, false);
            }

            ent->deadflag = DEAD_NO;
            clp->resp.osp_r2dc = 0;
            clp->resp.score = -100;
            clp->resp.osp_r0a0--;
            clp->resp.osp_r09c--;
            clp->resp.osp_r000 = 0;
            clp->resp.osp_r2d4 = level.framenum - clp->resp.enterframe;

            if (sync_stat < 4 && clp->resp.osp_entered == ENTERED_ENTERED)
                OSP_notready_cmd(ent, true);

            if (clp->resp.osp_entered == ENTERED_ENTERED) {
                active_clients--;
                EntityListRemove(ent);

                if (G_Ruleset() == RULESET_DUEL)
                    OSP_1v1Remove(ent, false);
            }

            clp->resp.osp_entered = 4;
            clp->resp.osp_r240 = 0;
            clp->osp_menu = NULL;
            G_MenuClose(&g_edicts[(clp - game.clients) + 1]);

            if (OSP_IsTeams())
                OSP_checkHalt(clp->resp.osp_r2cc);
            else if (G_Ruleset() == RULESET_DMPRO)
                OSP_checkHalt(2);

            OSP_DoRankSort();
            break;
        }
    }

    if (!clp->chase_target) {
        gi.cprintf(ent, PRINT_HIGH, "No clients to chase.\n");
        return;
    }

    for (t = 1; t <= game.maxclients; t++) {
        ep = g_edicts + t;

        if (!ep->inuse || !ep->client ||
            ep->client->chase_target != ent)
            continue;

        gi.cprintf(ep, PRINT_HIGH, "Target switched to chasecam mode.\n");
        OSP_removeChaseCam(ep);
    }

    OSP_observerTeamFrags(ent);
    OSP_Stats_PlayerMode(ent, "Chasecam");
}

void OSP_startObserve(edict_t *ent)
{
    gclient_t   *cl;

    cl = ent->client;

    if (level.intermission_framenum != 0)
        return;

    // Two independent top-level tests, each re-testing `entered`, not one
    // outer `if` with two arms.  Identical to CameraCmd's join sequence.
    if (cl->resp.osp_entered == ENTERED_ENTERED && !cl->resp.osp_r240)
        return;

    if (cl->resp.osp_entered == ENTERED_ENTERED && ent->health < 100 &&
        ent->health > 0 && sync_stat > 2) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Cannot go to spectator mode while injured.\n");
        return;
    }

    if (match_paused && OSP_IsTeams() && cl->resp.osp_entered != ENTERED_ENTERED) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Sorry, cannot join teams during a paused match.\n");
        return;
    }

    if (cl->resp.osp_entered == 2) {
        if (G_Ruleset() == RULESET_DUEL && !OSP_1v1AllowJoin(ent))
            return;

        if (!cl->resp.osp_r030 || G_Ruleset() == RULESET_DUEL) {
            if (OSP_IsTeams() && !OSP_addTeamMember(ent, 2))
                return;

            cl->resp.osp_r030 = 1;
            cl->resp.enterframe = level.framenum;
        } else {
            if (OSP_IsTeams() && !OSP_readdTeamMember(ent))
                return;

            if (cl->resp.osp_r030)
                cl->resp.enterframe = level.framenum - cl->resp.osp_r2d4;
        }

        ent->deadflag = DEAD_NO;
        cl->chase_target = NULL;
        cl->update_chase = false;
        cl->osp_t03c = NULL;
        cl->resp.osp_entered = ENTERED_ENTERED;
        cl->resp.osp_r240 = 0;
        cl->resp.osp_r2dc = 0;
        cl->resp.score = cl->resp.osp_r248;
        cl->resp.osp_r0a0--;
        cl->resp.osp_r09c--;
        active_clients++;

        if (OSP_IsMatch() && sync_stat < 4) {
            cl->resp.osp_r010 -= 2;
            OSP_notready_cmd(ent, true);
        }

        gi.bprintf(PRINT_HIGH, "%s entered the game (clients = %d)\n",
                   cl->pers.netname, active_clients);
        EntityListAdd(ent);
        OSP_DoRankSort();
        OSP_Stats_PlayerEnter(ent);
    } else {
        if (sync_stat < 4) {
            OSP_notready_cmd(ent, true);
            OSP_CheckReady();
        }

        if (rune_stat)
            OSP_deadDropRune(ent);

        OSP_observerTeamFrags(ent);
        cl->resp.osp_r2d4 = level.framenum - cl->resp.enterframe;
        cl->resp.osp_r000 = 0;
        cl->osp_menu = NULL;
        G_MenuClose(ent);
        OSP_removeChaseCam(ent);
    }
}

void OSP_removeChaseCam(edict_t *ent)
{
    gclient_t   *client;
    edict_t     *ee;
    int         x;
    int         was;

    client = ent->client;

    if (level.intermission_framenum != 0)
        return;

    gi.cprintf(ent, PRINT_HIGH, "Changing to OBSERVER mode.\n");
    client->chase_target = NULL;
    client->update_chase = false;

    if (sync_stat > 2 && !OSP_IsTeams())
        G_SetStat(ent, SID_OSP_STATUS3, 0);

    OSP_zeroRuneStats(ent);
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

    if (client->resp.osp_r030 && client->resp.osp_entered == ENTERED_ENTERED) {
        client->resp.osp_r248 = client->resp.score;

        if (OSP_IsTeams() && client->resp.team != 2)
            OSP_removeTeamMember(ent, false);

        if (G_Ruleset() == RULESET_DUEL)
            OSP_1v1Remove(ent, false);
    }

    client->resp.score = -100;
    client->resp.osp_r0a0--;
    client->resp.osp_r09c--;

    if (client->resp.osp_entered == ENTERED_ENTERED) {
        active_clients--;
        EntityListRemove(ent);
    }

    was = client->resp.osp_entered;
    client->resp.osp_entered = 2;
    client->resp.osp_r240 = 0;
    OSP_DoRankSort();

    if (sync_stat < 4) {
        client->resp.osp_r20c = 0;
        OSP_CheckReady();
    }

    if (was == ENTERED_ENTERED) {
        for (x = 1; x <= game.maxclients; x++) {
            ee = g_edicts + x;

            if (!ee->inuse || !ee->client || ee->client->chase_target != ent)
                continue;

            gi.cprintf(ee, PRINT_HIGH, "Target switched to observer mode.\n");
            OSP_removeChaseCam(ee);
        }

        if (OSP_IsTeams())
            OSP_checkHalt(client->resp.osp_r2cc);
        else if (G_Ruleset() == RULESET_DMPRO)
            OSP_checkHalt(2);
    }

    OSP_Stats_PlayerMode(ent, "Observe");
}

/*
=================
OSP_clientPolice

R-OSP-4's three enforcement rules, run once per client per frame from
ClientThink after the pmove -- so the position the inactivity rule compares is
the one the frame ended on.  True means the client is GONE (disconnected, or
moved to observer) and the caller must not touch it again.

All three were registered as cvars and read by nobody, so `client_minping`,
`client_maxping`, `client_maxfps` and `client_nomove` did nothing at all.

  * PING is sampled every three seconds and judged on the mean of sixteen
    samples, not on one: a single spike is not evidence and a server that
    kicked on one would be unplayable.  `osp_r1fc` is the next sample frame and
    -1 means "not watched", which is what OSP_clientBegunPost sets when neither
    bound is configured or the client is a bot.
  * INACTIVITY is a TEAM-MATCH warmup rule: a player who is not moving is
    holding up a match everybody else is waiting to start, so they are moved to
    observer rather than kicked.  Six seconds per check, and the position AND
    the view angles both have to be unchanged -- looking around counts as
    being there.
  * FRAMERATE is not enforced by kicking but by stuffing `cl_maxfps`, once and
    then at most once a minute: `ucmd->msec` below the cap is what a client
    running faster than the server allows looks like from here.
=================
*/
bool OSP_clientPolice(edict_t *ent, usercmd_t *ucmd)
{
    gclient_t   *client = ent->client;
    int          mean;

    if (client->resp.osp_r1fc >= 0 && client->resp.osp_r1fc < level.framenum) {
        client->resp.osp_r1f4++;
        client->resp.osp_r1f8 += client->ping;
        client->resp.osp_r1fc = level.framenum + 30;

        if (client->resp.osp_r1f4 > 15) {
            mean = client->resp.osp_r1f8 / client->resp.osp_r1f4;
            client->resp.osp_r1f4 = 0;
            client->resp.osp_r1f8 = 0;

            if ((int)client_minping->value && mean < (int)client_minping->value) {
                gi.cprintf(ent, PRINT_HIGH,
                           "Minimum allowed server ping %d, yours is %d.\n",
                           (int)client_minping->value, mean);
                gi.WriteByte(svc_disconnect);
                gi.unicast(ent, true);
                ClientDisconnect(ent);
                return true;
            }

            if ((int)client_maxping->value && mean > (int)client_maxping->value) {
                gi.cprintf(ent, PRINT_HIGH,
                           "Maximum allowed server ping %d, yours is %d.\n",
                           (int)client_maxping->value, mean);
                gi.WriteByte(svc_disconnect);
                gi.unicast(ent, true);
                ClientDisconnect(ent);
                return true;
            }
        }
    }

    if ((int)client_nomove->value &&
        client->resp.osp_entered == ENTERED_ENTERED &&
        OSP_IsTeams() && sync_stat < 4 &&
        client->resp.osp_r0d4 < level.framenum &&
        !(ent->flags & FL_BOTCLIENT)) {
        int idle;

        client->resp.osp_r0d4 = level.framenum + 60;

        // The two remembered vectors are three consecutive donor-offset ints
        // each; the cast is the donor's own and is what -fno-strict-aliasing is
        // for.  A named vec3_t here would renumber the struct.
        if (!VectorCompare((vec_t *)&client->resp.osp_r0dc, ent->s.angles) ||
            !VectorCompare((vec_t *)&client->resp.osp_r0e8, ent->s.origin)) {
            VectorCopy(ent->s.angles, ((vec_t *)&client->resp.osp_r0dc));
            VectorCopy(ent->s.origin, ((vec_t *)&client->resp.osp_r0e8));
            idle = 0;
        } else {
            idle = client->resp.osp_r0d8 + 6;
        }

        client->resp.osp_r0d8 = idle;
        if (idle >= (int)client_nomove->value) {
            gi.bprintf(PRINT_CHAT,
                       "%s inactive for %d seconds, moved to OBSERVER mode.\n",
                       client->pers.netname, idle);
            OSP_startObserve(ent);
            return true;
        }
    }

    if (client->resp.osp_r024 >= 0 && level.framenum > client->resp.osp_r024 &&
        ucmd->msec < client_maxframes - 2) {
        char cmd[64];

        if (!client->resp.osp_r024)
            gi.cprintf(ent, PRINT_HIGH, "*** Server cl_maxfps capped at %d\n",
                       (int)client_maxfps->value);

        client->resp.osp_r024 = level.framenum + 60;
        Q_snprintf(cmd, sizeof(cmd), "cl_maxfps %d\n", (int)client_maxfps->value);
        gi.WriteByte(svc_stufftext);
        gi.WriteString(cmd);
        gi.unicast(ent, false);
    }

    return false;
}

/*
=================
OSP_clientThink

Tourney's arms of ClientThink, in one call (R-OSP-1, R-EXTRA-6).  True means the
frame is CONSUMED: the client is on a camera or has just been moved to one, and
neither the spine's pmove nor its chase-cam code may also run.

Three arms, in the donor's order:

  * a client who died and is looking at the board goes back to a live HUD half a
    second after respawning
  * a client on the autocam has its own input handling: attack switches to the
    chase cam, jump toggles between the camera's two modes, and CameraThink
    drives the view.  It is here rather than in g_chase.c because the autocam is
    not a chase cam -- it picks its own subject and its own position
  * a client with no one left to watch is told so and put back in free-fly
=================
*/
bool OSP_clientThink(edict_t *ent, usercmd_t *ucmd)
{
    gclient_t *client = ent->client;

    // Dead, showing the board, and now alive again: put the HUD back.
    if (client->resp.osp_r2dc == 1 &&
        client->resp.osp_entered == ENTERED_ENTERED &&
        level.framenum > client->respawn_framenum + 5) {
        OSP_clearStats(ent);
        client->resp.osp_r2dc = 0;
        client->showscores = false;
        Cmd_Score_f(ent);
    }

    if (!client->osp_t040)
        return false;

    if (!active_clients) {
        gi.cprintf(ent, PRINT_HIGH,
                   "No clients to track. Switching to OBSERVE mode.\n");
        OSP_startObserve(ent);
        return true;
    }

    client->oldbuttons = client->buttons;
    client->buttons = ucmd->buttons;
    client->latched_buttons = client->buttons & ~client->oldbuttons;

    if ((client->latched_buttons & BUTTON_ATTACK) &&
        client->resp.osp_r010 <= level.framenum) {
        if (G_MenuActive(ent)) {
            Cmd_InvUse_f(ent);
        } else {
            client->resp.score = client->resp.osp_r248;
            OSP_ChaseCam(ent);
            if (client->chase_target) {
                if (sync_stat > 2 && !OSP_IsTeams())
                    G_SetStat(ent, SID_OSP_STATUS3, OSP_CS(4));
                gi.cprintf(ent, PRINT_HIGH, "Changing to CHASECAM mode.\n");
            } else {
                // Nothing to chase: back to the score that says "observing".
                client->resp.score = -100;
            }
        }
        client->resp.osp_r010 = level.framenum + 2;
        return true;
    }

    if (ucmd->upmove && !G_MenuActive(ent) &&
        client->resp.osp_r010 <= level.framenum) {
        if (client->osp_t038 == 1) {
            gi.cprintf(ent, PRINT_HIGH, "Switching to Autocam FOLLOW mode.\n");
            client->osp_t038 = 0;
        } else {
            gi.cprintf(ent, PRINT_HIGH, "Switching to Autocam NORMAL mode.\n");
            client->osp_t038 = 1;
        }
        client->resp.osp_r010 = level.framenum + 8;
    }

    CameraThink(ent);
    return true;
}
