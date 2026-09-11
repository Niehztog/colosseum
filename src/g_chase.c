/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "g_local.h"
#include "tourney/osp_hooks.h"

void UpdateChaseCam(edict_t *ent)
{
    vec3_t o, ownerv, goal;
    edict_t *targ;
    vec3_t forward, right;
    trace_t trace;
    int i;
    vec3_t angles, vangles;

    // is our chase target gone?
    if (!ent->client->chase_target->inuse
        || G_IsObserver(ent->client->chase_target)) {
        edict_t *old = ent->client->chase_target;
        ChaseNext(ent);
        if (ent->client->chase_target == old) {
            ent->client->chase_target = NULL;
            ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
            return;
        }
    }

    targ = ent->client->chase_target;

    VectorCopy(targ->s.origin, ownerv);

    ownerv[2] += targ->viewheight;

    VectorCopy(targ->s.origin, ownerv);
    ownerv[2] += targ->viewheight;

    VectorCopy(targ->client->v_angle, angles);
    // Tourney's two cameras, and everything below that reads `osp` is
    // theirs.  Tourney has a chase camera with controls and an in-eyes mode,
    // and this function is the consumer the merge left behind: `movedir` carries
    // the free-look offsets ClientThink accumulated, `speed` the zoom, and the
    // two modes differ in the pitch they allow, in whether the camera is behind
    // the target or at its eyes, and in whether it is lifted off the floor.
    //
    // `vangles` is the donor's second copy: the offsets have to reach the
    // VIEW angles as well as the ones the camera is placed along, or free-look
    // moves the camera and not the picture.  baseq2 has one copy because it has
    // no free-look.
    int osp = G_IsOspRuleset() ? ent->client->resp.osp_entered : 0;

    VectorCopy(targ->client->v_angle, vangles);

    // In-eyes looks along the target's own view, so the pitch is pinned rather
    // than clamped -- 1 against the chase camera's 56, which is baseq2's too.
    if (angles[PITCH] > (osp == ENTERED_INEYES ? 1 : 56))
        angles[PITCH] = (osp == ENTERED_INEYES ? 1 : 56);

    // The gate is written out rather than carried in `osp`, because
    // `camera_pitch` is tourney's own cvar in a spine file, and that is worth
    // being legible at the use rather than three lines up.
    if (G_IsOspRuleset()) {
        if (osp == ENTERED_CHASECAM) {
            ent->movedir[0] = camera_pitch->value;
            ent->movedir[1] = ent->client->osp_t018;
        } else {
            // In-eyes has no free-look and no zoom, and -12 puts the camera
            // twelve units in front of the eye rather than behind the head.
            ent->movedir[0] = 0;
            ent->movedir[1] = 0;
            ent->speed = -12;
        }

        angles[PITCH] += ent->movedir[0];
        angles[PITCH] = Q_clipf(angles[PITCH], -90, 90);
        angles[YAW] += ent->movedir[1];

        vangles[PITCH] += ent->movedir[0];
        vangles[PITCH] = Q_clipf(vangles[PITCH], -90, 90);
        vangles[YAW] += ent->movedir[1];
    }

    AngleVectors(angles, forward, right, NULL);
    VectorNormalize(forward);
    VectorMA(ownerv, osp ? -ent->speed : -30, forward, o);

    // The chase camera is kept 30 units off the target's feet; in-eyes is not
    // lifted at all, because it is meant to be where the eyes are.
    if (!osp) {
        if (o[2] < targ->s.origin[2] + 20)
            o[2] = targ->s.origin[2] + 20;
    } else if (osp == ENTERED_CHASECAM) {
        if (o[2] < targ->s.origin[2] + 30)
            o[2] = targ->s.origin[2] + 30;
    }

    // jump animation lifts
    if (!targ->groundentity)
        o[2] += 16;

    trace = gi.trace(ownerv, vec3_origin, vec3_origin, o, targ, MASK_SOLID);

    VectorCopy(trace.endpos, goal);

    VectorMA(goal, 2, forward, goal);

    // pad for floors and ceilings
    VectorCopy(goal, o);
    o[2] += 6;
    trace = gi.trace(goal, vec3_origin, vec3_origin, o, targ, MASK_SOLID);
    if (trace.fraction < 1) {
        VectorCopy(trace.endpos, goal);
        goal[2] -= 6;
    }

    VectorCopy(goal, o);
    o[2] -= 6;
    trace = gi.trace(goal, vec3_origin, vec3_origin, o, targ, MASK_SOLID);
    if (trace.fraction < 1) {
        VectorCopy(trace.endpos, goal);
        goal[2] += 6;
    }

    if (targ->deadflag)
        ent->client->ps.pmove.pm_type = PM_DEAD;
    else
        ent->client->ps.pmove.pm_type = PM_FREEZE;

    VectorCopy(goal, ent->s.origin);
    for (i = 0; i < 3; i++)
        ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(targ->client->v_angle[i] - ent->client->resp.cmd_angles[i]);

    if (targ->deadflag) {
        ent->client->ps.viewangles[ROLL] = 40;
        ent->client->ps.viewangles[PITCH] = -15;
        ent->client->ps.viewangles[YAW] = targ->client->killer_yaw;
    } else {
        // `vangles` rather than the target's own view under tourney: it is the
        // target's view PLUS the free-look offsets, and sending the target's
        // instead is what makes a free-look camera point the wrong way.
        VectorCopy(vangles, ent->client->ps.viewangles);
        VectorCopy(vangles, ent->client->v_angle);
    }

    ent->viewheight = 0;
    ent->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
    gi.linkentity(ent);

    // Under ctf the chased player's name is a unicast layout rather than a
    // statusbar element: CTF's bar has no slot 16 element and the slot is
    // unmapped for that ruleset (g_stats.h), which is the honest expression of
    // "Threewave draws this differently".
    // Elsewhere STAT_CHASE and the bar's `stat_string 16` do the job and this
    // block would fight them for the layout channel.
    if ((G_Ruleset() == RULESET_CTF || G_IsOspRuleset()) &&
        ((!ent->client->showscores && !G_MenuActive(ent) &&
          !ent->client->showinventory && !ent->client->showhelp &&
          !(level.framenum & 31)) || ent->client->update_chase)) {
        char s[MAX_STRING_CHARS];

        ent->client->update_chase = false;

        // Tourney names the team and, in a live team match, the score --
        // tourney's observer is meant to be able to follow a match, and
        // "Chasing Bob" alone does not say which side Bob is on.  Its slot 16
        // is the crosshair-id line, not this one, so the two do not collide.
        if (G_IsOspRuleset() && G_Ruleset() == RULESET_TDM && sync_stat > 2)
            Q_snprintf(s, sizeof(s),
                       "xv 44 yb -59 string \"Chasing `%s' [%d] (%s)\"",
                       targ->client->pers.netname, targ->client->resp.score,
                       OSP_teamName(targ->client->resp.team));
        else if (G_IsOspRuleset() && G_Ruleset() == RULESET_TDM)
            Q_snprintf(s, sizeof(s), "xv 44 yb -59 string \"Chasing `%s' (%s)\"",
                       targ->client->pers.netname,
                       OSP_teamName(targ->client->resp.team));
        else if (G_IsOspRuleset())
            Q_snprintf(s, sizeof(s), "xv 44 yb -59 string \"Chasing `%s'\"",
                       targ->client->pers.netname);
        else
            Q_snprintf(s, sizeof(s), "xv 0 yb -68 string2 \"Chasing %s\"",
                       targ->client->pers.netname);

        gi.WriteByte(svc_layout);
        gi.WriteString(s);
        gi.unicast(ent, false);
    }
}

void ChaseNext(edict_t *ent)
{
    int i;
    edict_t *e;

    if (!ent->client->chase_target)
        return;

    // Tourney starts every new target at the configured distance and
    // straight behind, so a zoom or a free-look does not follow the cursor from
    // the last player to the next one.  `osp_r000` is the count of clients
    // watching this one, which p_view.c reads to draw "N watching".
    if (G_IsOspRuleset()) {
        VectorClear(ent->movedir);
        ent->speed = camera_depth->value;
        ent->client->osp_t018 = 0;
    }

    i = ent->client->chase_target - g_edicts;
    do {
        i++;
        if (i > game.maxclients)
            i = 1;
        e = g_edicts + i;
        if (!e->inuse)
            continue;
        if (!G_IsObserver(e))
            break;
    } while (e != ent->client->chase_target);

    ent->client->chase_target = e;
    if (G_IsOspRuleset() && e->client)
        e->client->resp.osp_r000++;
    ent->client->update_chase = true;
}

void ChasePrev(edict_t *ent)
{
    int i;
    edict_t *e;

    if (!ent->client->chase_target)
        return;

    // Tourney starts every new target at the configured distance and
    // straight behind, so a zoom or a free-look does not follow the cursor from
    // the last player to the next one.  `osp_r000` is the count of clients
    // watching this one, which p_view.c reads to draw "N watching".
    if (G_IsOspRuleset()) {
        VectorClear(ent->movedir);
        ent->speed = camera_depth->value;
        ent->client->osp_t018 = 0;
    }

    i = ent->client->chase_target - g_edicts;
    do {
        i--;
        if (i < 1)
            i = game.maxclients;
        e = g_edicts + i;
        if (!e->inuse)
            continue;
        if (!G_IsObserver(e))
            break;
    } while (e != ent->client->chase_target);

    ent->client->chase_target = e;
    if (G_IsOspRuleset() && e->client)
        e->client->resp.osp_r000++;
    ent->client->update_chase = true;
}

// Threewave DELETES this and reaches the chase camera through CTFObserver
// instead.  Kept, because dm and sp still enter it from the attack button and
// the rule that a donor's deletion is not replayed applies to functions
// as well as to files.
void GetChaseTarget(edict_t *ent)
{
    int i;
    edict_t *other;

    for (i = 1; i <= game.maxclients; i++) {
        other = g_edicts + i;
        if (other->inuse && !G_IsObserver(other)) {
            ent->client->chase_target = other;
            ent->client->update_chase = true;
            UpdateChaseCam(ent);
            return;
        }
    }
    gi.centerprintf(ent, "No other players to chase.");
}
