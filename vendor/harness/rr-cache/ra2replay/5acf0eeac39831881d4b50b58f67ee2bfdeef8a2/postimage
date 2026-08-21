
#include "g_local.h"

// gamex86.dll: 1001CD50..1001D49B
// gamei386.so: 00054E80..0005550C
void UpdateChaseCam(edict_t *ent)
{
    vec3_t o, ownerv, goal;
    edict_t *targ;
    vec3_t forward, right;
    trace_t trace;
    int i;
    vec3_t angles;
    vec3_t vangles;         // <INVENTED NAME>

    targ = ent->client->chase_target;

    VectorCopy(targ->s.origin, ownerv);

    ownerv[2] += targ->viewheight;

    VectorCopy(targ->client->v_angle, angles);
    VectorCopy(targ->client->v_angle, vangles);

    if (ent->client->resp.entered == 4) {
        if (angles[PITCH] > 56)
            angles[PITCH] = 56;
    } else {
        if (angles[PITCH] > 1)
            angles[PITCH] = 1;
    }

    // chained chasecam: ent's own velocity/avelocity are unused while it is
    // frozen for chasecam, so they are repurposed as the previous frame's
    // cmd_angles, giving the frame-to-frame delta in avelocity.
    VectorSubtract(ent->velocity, ent->client->resp.cmd_angles, ent->avelocity);
    VectorCopy(ent->client->resp.cmd_angles, ent->velocity);

    // ent's own movedir/speed are likewise unused, and hold the free-look
    // pitch/yaw offset and zoom distance while chasecamming (entered==4);
    // in in-eyes mode (entered==8) there is no free-look and no zoom.
    if (ent->client->resp.entered == 4)
        ent->movedir[0] = camera_pitch->value;
    else
        ent->movedir[0] = 0;

    if (ent->client->resp.entered == 4)
        ent->movedir[1] = *(float *)&ent->client->osp_t018;
    else {
        ent->movedir[1] = 0;
        ent->speed = -12;
    }

    angles[PITCH] += ent->movedir[0];
    if (angles[PITCH] > 90)
        angles[PITCH] = 90;
    if (angles[PITCH] < -90)
        angles[PITCH] = -90;

    angles[YAW] += ent->movedir[1];

    vangles[PITCH] += ent->movedir[0];
    if (vangles[PITCH] > 90)
        vangles[PITCH] = 90;
    if (vangles[PITCH] < -90)
        vangles[PITCH] = -90;

    vangles[YAW] += ent->movedir[1];

    AngleVectors(angles, forward, right, NULL);
    VectorNormalize(forward);
    VectorMA(ownerv, -ent->speed, forward, o);

    if (ent->client->resp.entered == 4) {
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

    ent->client->ps.pmove.pm_type = PM_FREEZE;

    VectorCopy(goal, ent->s.origin);
    for (i = 0; i < 3; i++)
        ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(targ->client->v_angle[i] - ent->client->resp.cmd_angles[i]);

    VectorCopy(vangles, ent->client->ps.viewangles);
    VectorCopy(vangles, ent->client->v_angle);

    ent->viewheight = 0;
    ent->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
    gi.linkentity(ent);

    if ((!ent->client->showscores && !ent->client->showinventory &&
         !ent->client->showhelp && !(level.framenum & 0x1f)) ||
        ent->client->update_chase) {
        char string[1024];          // <INVENTED SIZE>

        ent->client->update_chase = false;

        if (m_mode != 2) {
            sprintf(string, "xv 44 yb -59 string \"Chasing `%s'\"",
                    targ->client->pers.netname);
        } else if (sync_stat > 2) {
            sprintf(string, "xv 44 yb -59 string \"Chasing `%s' [%d] (%s)\"",
                    targ->client->pers.netname, targ->client->resp.score,
                    teams[targ->client->resp.team].netname);
        } else {
            sprintf(string, "xv 44 yb -59 string \"Chasing `%s' (%s)\"",
                    targ->client->pers.netname, teams[targ->client->resp.team].netname);
        }

        gi.WriteByte(svc_layout);
        gi.WriteString(string);
        gi.unicast(ent, false);
    }
}

// gamex86.dll: 1001D49B..1001D5C4
// gamei386.so: 0005550C..0005562E
void ChaseNext(edict_t *ent)
{
    int i;
    edict_t *e;

    if (!ent->client->chase_target)
        return;

    VectorSet(ent->movedir, 0, 0, 0);
    ent->speed = camera_depth->value;
    ent->client->osp_t018 = 0;

    i = ent->client->chase_target - g_edicts;
    do {
        i++;
        if (i > game.maxclients)
            i = 1;
        e = g_edicts + i;
        if (!e->inuse)
            continue;
        if (e->solid)
            break;
    } while (e != ent->client->chase_target);

    ent->client->chase_target = e;
    e->client->resp.osp_r000++;
    ent->client->update_chase = true;
    UpdateChaseCam(ent);
}

// gamex86.dll: 1001D5C4..1001D6F0
// gamei386.so: 00055630..0005574B
void ChasePrev(edict_t *ent)
{
    int i;
    edict_t *e;

    if (!ent->client->chase_target)
        return;

    VectorSet(ent->movedir, 0, 0, 0);
    ent->speed = camera_depth->value;
    ent->client->osp_t018 = 0;

    i = ent->client->chase_target - g_edicts;
    do {
        i--;
        if (i < 1)
            i = game.maxclients;
        e = g_edicts + i;
        if (!e->inuse)
            continue;
        if (e->solid)
            break;
    } while (e != ent->client->chase_target);

    ent->client->chase_target = e;
    e->client->resp.osp_r000++;
    ent->client->update_chase = true;
    UpdateChaseCam(ent);
}
