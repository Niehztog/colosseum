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
    vec3_t angles;

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

    VectorCopy(targ->client->v_angle, angles);
    if (angles[PITCH] > 56)
        angles[PITCH] = 56;
    AngleVectors(angles, forward, right, NULL);
    VectorNormalize(forward);
    VectorMA(ownerv, -30, forward, o);

    if (o[2] < targ->s.origin[2] + 20)
        o[2] = targ->s.origin[2] + 20;

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
        VectorCopy(targ->client->v_angle, ent->client->ps.viewangles);
        VectorCopy(targ->client->v_angle, ent->client->v_angle);
    }

    ent->viewheight = 0;
    ent->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
    gi.linkentity(ent);

    // Under ctf the chased player's name is a unicast layout rather than a
    // statusbar element: CTF's bar has no slot 16 element and the slot is
    // unmapped for that ruleset (g_stats.h), which is the honest expression of
    // "Threewave draws this differently" -- see doc/reconciliation.md R-43.
    // Elsewhere STAT_CHASE and the bar's `stat_string 16` do the job and this
    // block would fight them for the layout channel.
    if ((G_Ruleset() == RULESET_CTF || G_Ruleset() == RULESET_TOURNEY) &&
        ((!ent->client->showscores && !G_MenuActive(ent) &&
          !ent->client->showinventory && !ent->client->showhelp &&
          !(level.framenum & 31)) || ent->client->update_chase)) {
        char s[MAX_STRING_CHARS];

        ent->client->update_chase = false;

        // Tourney names the team and, in a live team match, the score --
        // R-OSP-1's observer is meant to be able to follow a match, and
        // "Chasing Bob" alone does not say which side Bob is on.  Its slot 16
        // is the crosshair-id line, not this one, so the two do not collide.
        if (G_Ruleset() == RULESET_TOURNEY && m_mode == 2 && sync_stat > 2)
            Q_snprintf(s, sizeof(s),
                       "xv 44 yb -59 string \"Chasing `%s' [%d] (%s)\"",
                       targ->client->pers.netname, targ->client->resp.score,
                       OSP_teamName(targ->client->resp.team));
        else if (G_Ruleset() == RULESET_TOURNEY && m_mode == 2)
            Q_snprintf(s, sizeof(s), "xv 44 yb -59 string \"Chasing `%s' (%s)\"",
                       targ->client->pers.netname,
                       OSP_teamName(targ->client->resp.team));
        else if (G_Ruleset() == RULESET_TOURNEY)
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
    ent->client->update_chase = true;
}

void ChasePrev(edict_t *ent)
{
    int i;
    edict_t *e;

    if (!ent->client->chase_target)
        return;

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
    ent->client->update_chase = true;
}

// Threewave DELETES this and reaches the chase camera through CTFObserver
// instead.  Kept, because dm and sp still enter it from the attack button and
// R-CORE-8's rule -- a donor's deletion is not replayed -- applies to functions
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
