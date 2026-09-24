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
// OSP Tourney DM v2.75, from osp-tourney@1895f8e.
// Donor-only: baseq2 has no counterpart, so it lives in src/tourney/ rather
// than being merged into a spine file.  The reconstruction's asm-matching
// address comments are stripped.  p_camera.c -- filename assigned by this
// tree.  Observer and autocam support.

#include "g_local.h"
#include "tourney/osp_types.h"
#include "tourney/osp_stats.h"

typedef struct entity_list_s {
    edict_t                 *ent;
    struct entity_list_s    *next;
} entity_list_t;

void CameraStaticThink(edict_t *ent);
entity_list_t *EntityListHead(void);
entity_list_t *EntityListNext(entity_list_t *entry);
unsigned long EntityListNumber(void);
void EntityListAdd(edict_t *ent);
bool OSP_1v1AllowJoin(edict_t *ent);
bool OSP_addTeamMember(edict_t *ent, int team);
bool OSP_readdTeamMember(edict_t *ent);
void OSP_removeTeamMember(edict_t *ent, bool announce);
void OSP_checkHalt(int reason);
void OSP_observerTeamFrags(edict_t *ent);
void OSP_removeChaseCam(edict_t *ent);
void OSP_deadDropRune(edict_t *ent);
edict_t *pDeadPlayer = NULL;

bool CameraCmd(edict_t *ent, bool force)
{
    gclient_t   *cl;
    edict_t     *who;
    double      value;
    int         i;

    cl = ent->client;

    if (level.intermission_framenum != 0)
        return false;

    if (cl->resp.osp_entered == ENTERED_ENTERED && !cl->resp.osp_r240)
        return false;

    if (cl->resp.osp_entered == ENTERED_ENTERED &&
        ent->health < 100 && ent->health > 0) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Cannot go to spectator mode while injured.\n");
        return false;
    }

    if (match_paused && OSP_IsTeams() &&
        cl->resp.osp_entered != ENTERED_ENTERED) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Sorry, cannot join teams during a paused match.\n");
        return false;
    }

    if (gi.argc() == 1 || !Q_stricmp(gi.argv(1), "on") || !force) {
        if (ent->client->osp_t040) {
            if (G_Ruleset() == RULESET_DUEL && !OSP_1v1AllowJoin(ent))
                return false;

            if (cl->resp.osp_entered != 16 && !active_clients) {
                gi.cprintf(ent, PRINT_HIGH, "No clients to track.\n");
                return false;
            }

            if (!cl->resp.osp_r030 || G_Ruleset() == RULESET_DUEL) {
                if (OSP_IsTeams() && !OSP_addTeamMember(ent, 2))
                    return false;

                cl->resp.osp_r030 = 1;
                cl->resp.enterframe = level.framenum;
            } else {
                if (OSP_IsTeams() && !OSP_readdTeamMember(ent))
                    return false;

                if (cl->resp.osp_r030)
                    cl->resp.enterframe = level.framenum - cl->resp.osp_r2d4;
            }

            cl->chase_target = NULL;
            cl->update_chase = false;
            cl->osp_t040 = 0;
            cl->resp.osp_entered = ENTERED_ENTERED;
            cl->resp.osp_r240 = 0;
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
            return true;
        }

        if (rune_stat)
            OSP_deadDropRune(ent);

        ent->groundentity = NULL;
        ent->takedamage = DAMAGE_NO;
        ent->movetype = MOVETYPE_FLY;
        ent->viewheight = 0;
        ent->classname = "camera";
        ent->mass = 0;
        ent->solid = SOLID_NOT;
        ent->deadflag = DEAD_NO;
        ent->clipmask = -1;
        ent->model = "";
        ent->waterlevel = 0;
        ent->watertype = 0;
        ent->flags = FL_FLY;

        ent->client->osp_t040 = 1;
        cl->ps.fov = 90;
        cl->osp_t038 = 1;
        cl->osp_t044 = 0;
        cl->osp_t054 = 4.0f;
        cl->osp_t05c = 1.0f;
        cl->osp_t064 = 6.0f;
        cl->pers.weapon = NULL;
        cl->ps.gunindex = 0;
        cl->resp.osp_r000 = 0;
        cl->resp.osp_r2d4 = level.framenum - cl->resp.enterframe;

        ent->s.effects = 0;
        ent->s.skinnum = 0;
        ent->s.modelindex = 0;
        ent->s.modelindex2 = 0;
        ent->s.frame = 0;
        ent->s.angles[0] = 0;
        ent->s.angles[1] = 0;
        ent->s.angles[2] = 0;
        ent->client->ps.viewangles[0] = ent->s.angles[0];
        ent->client->ps.viewangles[1] = ent->s.angles[1];
        ent->client->ps.viewangles[2] = ent->s.angles[2];
        ent->client->v_angle[0] = ent->s.angles[0];
        ent->client->v_angle[1] = ent->s.angles[1];
        ent->client->v_angle[2] = ent->s.angles[2];
        cl->showscores = false;
        cl->showinventory = false;
        cl->pers.hand = CENTER_HANDED;
        cl->ps.stats[STAT_HELPICON] = 0;

        if (cl->resp.osp_r030 && cl->resp.osp_entered == ENTERED_ENTERED) {
            cl->resp.osp_r248 = cl->resp.score;
            if (OSP_IsTeams() && cl->resp.team != 2)
                OSP_removeTeamMember(ent, false);
        }

        if (cl->resp.osp_r030 && cl->resp.osp_entered == ENTERED_ENTERED) {
            cl->resp.osp_r248 = cl->resp.score;
            if (OSP_IsTeams() && cl->resp.team != 2)
                OSP_removeTeamMember(ent, false);
        }

        ent->deadflag = DEAD_NO;
        cl->chase_target = NULL;
        cl->update_chase = false;
        cl->resp.osp_r2dc = 0;
        cl->resp.score = -100;
        cl->resp.osp_r0a0--;
        cl->resp.osp_r09c--;

        if (sync_stat > 2 && !OSP_IsTeams())
            G_SetStat(ent, SID_OSP_STATUS3, 0);

        if (sync_stat < 4 && cl->resp.osp_entered == ENTERED_ENTERED)
            OSP_notready_cmd(ent, true);

        if (cl->resp.osp_entered == ENTERED_ENTERED) {
            active_clients--;
            if (G_Ruleset() == RULESET_DUEL)
                OSP_1v1Remove(ent, false);
        }

        cl->resp.osp_entered = 16;
        cl->resp.osp_r240 = 0;
        cl->osp_menu = NULL;
        G_MenuClose(&g_edicts[(cl - game.clients) + 1]);

        if (OSP_IsTeams())
            OSP_checkHalt(cl->resp.osp_r2cc);
        else if (G_Ruleset() == RULESET_DMPRO)
            OSP_checkHalt(2);

        OSP_DoRankSort();

        for (i = 1; i <= game.maxclients; i++) {
            who = &g_edicts[i];
            if (!who->inuse || !who->client ||
                who->client->chase_target != ent)
                continue;

            gi.cprintf(who, PRINT_HIGH,
                       "Target switched to autocam mode.\n");
            OSP_removeChaseCam(who);
        }

        OSP_observerTeamFrags(ent);
        OSP_Stats_PlayerMode(ent, "Autocam");
        return true;
    }
    // `return false;` is written inline at every site rather than jumping to
    // one shared label.
    else if (ent->classname[0] != 'c')
        return false;

    {
        if (!Q_stricmp(gi.argv(1), "follow")) {
            ent->client->osp_t038 = 0;
            goto camera_done;
        }

        if (!Q_stricmp(gi.argv(1), "normal")) {
            ent->client->osp_t038 = 1;
            goto camera_done;
        }

        if (!Q_stricmp(gi.argv(1), "min_xy") && ent->client->osp_t040) {
            value = atof(gi.argv(2));
            if (value < 1.0f)
                gi.cprintf(ent, PRINT_HIGH,
                           "Min X/Y delta of %f unchanged!\n",
                           ent->client->osp_t054);
            else {
                ent->client->osp_t054 = value;
                gi.cprintf(ent, PRINT_HIGH,
                           "Min X/Y delta of %f. set.\n",
                           ent->client->osp_t054);
            }
            goto camera_done;
        }

        if (!Q_stricmp(gi.argv(1), "min_z") && ent->client->osp_t040) {
            value = atof(gi.argv(2));
            if (value < 1.0f)
                gi.cprintf(ent, PRINT_HIGH,
                           "Min Z delta of %f unchanged!\n",
                           ent->client->osp_t05c);
            else {
                ent->client->osp_t05c = value;
                gi.cprintf(ent, PRINT_HIGH, "Min Z delta of %f set.\n",
                           ent->client->osp_t05c);
            }
            goto camera_done;
        }

        if (!Q_stricmp(gi.argv(1), "min_angle") && ent->client->osp_t040) {
            value = atof(gi.argv(2));
            if (value < 1.0f)
                gi.cprintf(ent, PRINT_HIGH,
                           "Min Yaw Angle delta of %f unchanged!\n",
                           ent->client->osp_t064);
            else {
                ent->client->osp_t064 = value;
                gi.cprintf(ent, PRINT_HIGH,
                           "Min Yaw Angle delta of %f set.\n",
                           ent->client->osp_t064);
            }
            goto camera_done;
        }

        gi.cprintf(ent, PRINT_HIGH, "Unknown command: \"%s\"\n",
                   gi.argv(1));
        gi.cprintf(ent, PRINT_HIGH, "\nAvailable \"autocam\" commands:\n");
        gi.cprintf(ent, PRINT_HIGH, "  follow: Follow a single player\n");
        gi.cprintf(ent, PRINT_HIGH, "  normal: Cycle through players\n");
        gi.cprintf(ent, PRINT_HIGH, "  min_xy: Change XY-plane delta rate\n");
        gi.cprintf(ent, PRINT_HIGH, "  min_z : Change Z-axis delta rate\n");
        gi.cprintf(ent, PRINT_HIGH,
                   "  min_angle: Change panning rate\n\n");
    }

camera_done:
    return false;
}

void PlayerDied(edict_t *ent)
{
    if (ent->client)
        pDeadPlayer = ent;
}

bool IsVisible(edict_t *ent, edict_t *other)
{
    int     dist;
    vec3_t  dir;
    trace_t tr;

    if (!gi.inPVS(ent->s.origin, other->s.origin))
        return false;

    tr = gi.trace(ent->s.origin, vec3_origin, vec3_origin, other->s.origin,
                  ent, MASK_SOLID);
    VectorSubtract(ent->s.origin, other->s.origin, dir);

    dist = (int)VectorLength(dir);
    if (dist < 1000 && tr.fraction == 1.0f)
        return true;
    return false;
}

int NumPlayersVisible(edict_t *ent)
{
    int             count = 0;
    entity_list_t   *entry;

    for (entry = EntityListHead(); entry; entry = entry->next) {
        if (!entry->ent->client->osp_t040 && IsVisible(entry->ent, ent))
            count++;
    }

    return count;
}

edict_t *ClosestVisible(edict_t *ent)
{
    vec3_t          direction;
    entity_list_t   *entry;
    entity_list_t   *hot = NULL;
    unsigned int    hotlen;
    unsigned int    closest_distance = 0xffffffff;

    for (entry = EntityListHead(); entry; entry = entry->next) {
        if (!entry->ent->client->osp_t040 && IsVisible(entry->ent, ent)) {
            VectorSubtract(entry->ent->s.origin, ent->s.origin, direction);
            hotlen = VectorLength(direction);

            if (hotlen < closest_distance) {
                hot = entry;
                closest_distance = hotlen;
            }
        }
    }

    if (!hot)
        return NULL;
    return hot->ent;
}

edict_t *PlayerToFollow(void)
{
    entity_list_t   *entry;
    entity_list_t   *closeent = NULL;
    int             closest_visible = 0;
    int             vis;

    for (entry = EntityListHead(); entry; entry = entry->next) {
        vis = 0;

        if (!entry->ent->deadflag && !entry->ent->client->osp_t040) {
            vis = NumPlayersVisible(entry->ent);
            if (vis > closest_visible) {
                closest_visible = vis;
                closeent = entry;
            } else if (vis && vis == closest_visible) {
                if (closeent->ent->client->resp.score <
                    entry->ent->client->resp.score)
                    closeent = entry;
            }
        }
    }

    if (!closeent)
        return NULL;
    return closeent->ent;
}

void CameraAloneThink(edict_t *ent)
{
    CameraStaticThink(ent);
}

void PointCamAtOrigin(edict_t *ent, vec3_t origin)
{
    vec3_t  difference;
    vec3_t  newangles;

    VectorSubtract(origin, ent->s.origin, difference);
    vectoangles(difference, newangles);

    VectorCopy(newangles, ent->s.angles);
    VectorCopy(newangles, ent->client->ps.viewangles);
    VectorCopy(newangles, ent->client->v_angle);
}

void PointCamAtTarget(edict_t *ent)
{
    vec3_t  diff;
    vec3_t  aimang;
    float   angdiff;

    VectorSubtract(ent->client->osp_t03c->s.origin, ent->s.origin,
                   diff);
    diff[2] += 35.0f;
    vectoangles(diff, aimang);

    ent->s.angles[0] = aimang[0];
    ent->s.angles[2] = 0;
    angdiff = aimang[1] - ent->s.angles[1];

    while (fabsf(angdiff) > 180) {
        if (angdiff > 0)
            angdiff -= 360;
        else
            angdiff += 360;
    }

    if (fabsf(angdiff) > ent->client->osp_t064) {
        if (angdiff > 0)
            ent->s.angles[1] += ent->client->osp_t064;
        else
            ent->s.angles[1] -= ent->client->osp_t064;
    } else
        ent->s.angles[1] = aimang[1];

    VectorCopy(ent->s.angles, ent->client->ps.viewangles);
    VectorCopy(ent->s.angles, ent->client->v_angle);
}

void RepositionAtTarget(edict_t *ent, vec3_t offset)
{
    vec3_t  delta;
    vec3_t  camera_position;
    vec3_t  forward;
    trace_t tr;

    AngleVectors(ent->client->osp_t03c->client->v_angle, forward, NULL, NULL);
    forward[2] = 0;
    VectorNormalize(forward);

    camera_position[0] = ent->client->osp_t03c->s.origin[0] +
                         offset[0] * forward[0];
    camera_position[1] = ent->client->osp_t03c->s.origin[1] +
                         offset[1] * forward[1];
    camera_position[2] = ent->client->osp_t03c->s.origin[2] + offset[2];

    tr = gi.trace(ent->client->osp_t03c->s.origin, NULL, NULL,
                  camera_position, ent->client->osp_t03c, CONTENTS_SOLID);

    if (tr.fraction < 1) {
        VectorSubtract(tr.endpos, ent->client->osp_t03c->s.origin,
                       delta);
        VectorNormalize(delta);
        VectorMA(tr.endpos, -8, delta, tr.endpos);

        if (tr.plane.normal[2] > 0.8f)
            tr.endpos[2] += 4;
    }

    if (fabsf(tr.endpos[0] - ent->s.origin[0]) > ent->client->osp_t054) {
        if (tr.endpos[0] > ent->s.origin[0])
            ent->s.origin[0] += ent->client->osp_t054;
        else
            ent->s.origin[0] -= ent->client->osp_t054;
    } else
        ent->s.origin[0] = tr.endpos[0];

    if (fabsf(tr.endpos[1] - ent->s.origin[1]) > ent->client->osp_t054) {
        if (tr.endpos[1] > ent->s.origin[1])
            ent->s.origin[1] += ent->client->osp_t054;
        else
            ent->s.origin[1] -= ent->client->osp_t054;
    } else
        ent->s.origin[1] = tr.endpos[1];

    if (fabsf(tr.endpos[2] - ent->s.origin[2]) > ent->client->osp_t05c) {
        if (tr.endpos[2] > ent->s.origin[2])
            ent->s.origin[2] += ent->client->osp_t05c;
        else
            ent->s.origin[2] -= ent->client->osp_t05c;
    } else
        ent->s.origin[2] = tr.endpos[2];

    tr = gi.trace(ent->client->osp_t03c->s.origin, NULL, NULL,
                  ent->s.origin, ent->client->osp_t03c, CONTENTS_SOLID);

    if (tr.fraction < 1) {
        VectorSubtract(tr.endpos, ent->client->osp_t03c->s.origin,
                       delta);
        VectorNormalize(delta);
        VectorMA(tr.endpos, -8, delta, tr.endpos);

        if (tr.plane.normal[2] > 0.8f)
            tr.endpos[2] += 4;

        VectorCopy(tr.endpos, ent->s.origin);
    }
}

void RepositionAtOrigin(edict_t *ent, vec3_t offset)
{
    vec3_t  camera_position;
    trace_t tr;

    camera_position[0] = offset[0] + 40;
    camera_position[1] = offset[1] + 40;
    camera_position[2] = offset[2] + 30;

    tr = gi.trace(offset, NULL, NULL, camera_position,
                  ent->client->osp_t03c, CONTENTS_SOLID);

    if (tr.fraction < 1) {
        vec3_t  difference;

        VectorSubtract(tr.endpos, offset, difference);
        VectorNormalize(difference);
        VectorMA(tr.endpos, -8, difference, tr.endpos);

        if (tr.plane.normal[2] > 0.8f)
            tr.endpos[2] += 4;
    }

    if (fabsf(tr.endpos[0] - ent->s.origin[0]) > ent->client->osp_t054) {
        if (tr.endpos[0] > ent->s.origin[0])
            ent->s.origin[0] += ent->client->osp_t054;
        else
            ent->s.origin[0] -= ent->client->osp_t054;
    } else
        ent->s.origin[0] = tr.endpos[0];

    if (fabsf(tr.endpos[1] - ent->s.origin[1]) > ent->client->osp_t054) {
        if (tr.endpos[1] > ent->s.origin[1])
            ent->s.origin[1] += ent->client->osp_t054;
        else
            ent->s.origin[1] -= ent->client->osp_t054;
    } else
        ent->s.origin[1] = tr.endpos[1];

    if (fabsf(tr.endpos[2] - ent->s.origin[2]) > ent->client->osp_t05c) {
        if (tr.endpos[2] > ent->s.origin[2])
            ent->s.origin[2] += ent->client->osp_t05c;
        else
            ent->s.origin[2] -= ent->client->osp_t05c;
    } else
        ent->s.origin[2] = tr.endpos[2];

    tr = gi.trace(offset, NULL, NULL, ent->s.origin,
                  ent->client->osp_t03c, CONTENTS_SOLID);

    if (tr.fraction < 1) {
        vec3_t  difference;

        VectorSubtract(tr.endpos, offset, difference);
        VectorNormalize(difference);
        VectorMA(tr.endpos, -8, difference, tr.endpos);

        if (tr.plane.normal[2] > 0.8f)
            tr.endpos[2] += 4;
    }

    if (tr.fraction != 1)
        VectorCopy(tr.endpos, ent->s.origin);
}

void UpdateValues(edict_t *ent)
{
    edict_t *target;
    char    layout[1024];

    target = ent->client->osp_t03c;

    if (target) {
        if (!(ent->client->showscores || ent->client->showinventory ||
              ent->client->showhelp || (level.framenum & 31))) {
            int     tnum = target->client->resp.team;
            // resp.team is 2 for a client on no team, and osp_teams[] has two
            // entries
            const char *tname = (tnum == 0 || tnum == 1) ?
                                osp_teams[tnum].netname : "no team";

            if (G_Ruleset() != RULESET_TDM)
                Q_snprintf(layout, sizeof(layout),
                           "xv 44 yb -59 string \"Tracking `%s'\"",
                           target->client->pers.netname);
            else if (sync_stat > 2)
                Q_snprintf(layout, sizeof(layout),
                           "xv 44 yb -59 string \"Tracking `%s' [%d] (%s)\"",
                           target->client->pers.netname,
                           target->client->resp.score, tname);
            else
                Q_snprintf(layout, sizeof(layout),
                           "xv 44 yb -59 string \"Tracking `%s' (%s)\"",
                           target->client->pers.netname, tname);

            gi.WriteByte(svc_layout);
            gi.WriteString(layout);
            gi.unicast(ent, false);
        }
    } else {
        ent->client->resp.score = 0;
        ent->health = 0;

        if (!(ent->client->showscores || ent->client->showinventory ||
              ent->client->showhelp || (level.framenum & 31))) {
            Q_strlcpy(layout, "xv 44 yb -59 string \" \"", sizeof(layout));
            gi.WriteByte(svc_layout);
            gi.WriteString(layout);
            gi.unicast(ent, false);
        }
    }
}

void CameraFollowThink(edict_t *ent)
{
    vec3_t  offset;

    if (ent->client->osp_t03c || (ent->client->osp_t03c = PlayerToFollow())) {
        offset[0] = -60;
        offset[1] = -60;
        offset[2] = 40;
        RepositionAtTarget(ent, offset);
        PointCamAtTarget(ent);
    }

    UpdateValues(ent);
}

void CameraNormalThink(edict_t *ent)
{
    int     viscnt;
    vec3_t  offset;

    viscnt = NumPlayersVisible(ent);

    if (!ent->client->osp_t044 && ent->client->osp_t03c &&
        ent->client->osp_t03c->deadflag) {
        ent->client->osp_t044 = true;
        ent->last_move_framenum = level.framenum + 2.0f * BASE_FRAMERATE;
        PointCamAtTarget(ent);
        goto done;
    }

    if (ent->client->osp_t044) {
        if (ent->last_move_framenum < level.framenum) {
            ent->client->osp_t044 = false;
        } else {
            if (ent->client->osp_t03c->deadflag)
                VectorCopy(ent->client->osp_t03c->s.origin,
                           ent->client->osp_t048);

            PointCamAtOrigin(ent, ent->client->osp_t048);
            RepositionAtOrigin(ent, ent->client->osp_t048);
        }
        goto done;
    }

    if (viscnt < 2) {
        offset[0] = -60;
        offset[1] = -60;
        offset[2] = 40;

        if (ent->last_move_framenum > level.framenum) {
            if (viscnt > 0) {
                if ((ent->client->osp_t03c = ClosestVisible(ent)) != NULL) {
                    RepositionAtTarget(ent, offset);
                    PointCamAtTarget(ent);
                    ent->client->osp_t03c->client->resp.osp_r000++;
                }
            } else if ((ent->client->osp_t03c = PlayerToFollow()) != NULL) {
                RepositionAtTarget(ent, offset);
                PointCamAtTarget(ent);
                ent->last_move_framenum = 0;
                ent->client->osp_t03c->client->resp.osp_r000++;
            }
        } else if ((ent->client->osp_t03c = PlayerToFollow()) != NULL) {
            offset[0] = -60;
            offset[1] = -60;
            offset[2] = 47;
            RepositionAtTarget(ent, offset);
            PointCamAtTarget(ent);
            ent->client->osp_t03c->client->resp.osp_r000++;
        }
        goto done;
    }

    // One condition, not two `goto`s: real takes a DIRECT single-instruction
    // jump at each of the three tests, which a textual `goto` never gets.
    if (ent->last_move_framenum < level.framenum ||
        (ent->client->osp_t03c &&
         !gi.inPVS(ent->s.origin, ent->client->osp_t03c->s.origin))) {
        if ((ent->client->osp_t03c = PlayerToFollow()) != NULL) {
            offset[0] = -60;
            offset[1] = -60;
            offset[2] = 80;
            PointCamAtTarget(ent);
            RepositionAtTarget(ent, offset);
            ent->last_move_framenum = level.framenum + 15 * BASE_FRAMERATE;
            ent->client->osp_t03c->client->resp.osp_r000++;
        }
    } else if (ent->client->osp_t03c) {
        if (ent->last_move_framenum > level.framenum + 15 - 3 ||
            ent->last_move_framenum < level.framenum + 5)
            RepositionAtOrigin(ent, ent->client->osp_t03c->s.origin);

        PointCamAtTarget(ent);
    }

done:
    pDeadPlayer = NULL;

    if (!ent->client->osp_t03c)
        return;

    UpdateValues(ent);
}

void CameraThink(edict_t *ent)
{
    entity_list_t   *curp;
    int             count;
    int             fmode;  // invented name

    if (ent->client->osp_t03c && !ent->client->osp_t03c->inuse) {
        count = 0;
        curp = EntityListHead();

        while (curp && ent->client->osp_t03c &&
               ent->client->osp_t03c->inuse &&
               curp->ent != ent->client->osp_t03c)
            curp = EntityListNext(curp);

        curp = EntityListNext(curp);
        if (!curp)
            curp = EntityListHead();

        while (!curp->ent->solid) {
            curp = EntityListNext(curp);
            if (!curp)
                curp = EntityListHead();

            count++;
            if (count >= game.maxclients)
                goto camera_mode;
        }

        if (count < game.maxclients) {
            ent->client->osp_t03c = curp->ent;
            curp->ent->client->resp.osp_r000++;
        }
    }

camera_mode:
    ent->client->ps.pmove.pm_type = PM_FREEZE;
    ent->client->ps.pmove.gravity = 0;

    // CameraAloneThink is a one-line forwarder to CameraStaticThink.
    if (!EntityListNumber())
        CameraAloneThink(ent);
    else {
        // An explicit forward `goto`, not an if/else.  The Follow arm then
        // needs its own `return`.
        fmode = ent->client->osp_t038;
        if (fmode)
            goto camera_normal;

        CameraFollowThink(ent);
        return;

camera_normal:
        CameraNormalThink(ent);
    }
}

void CameraStaticThink(edict_t *ent)
{
    trace_t tr;
    vec3_t  epos;
    vec3_t  camera_end;

    epos[0] = ent->s.origin[0];
    epos[1] = ent->s.origin[1];
    epos[2] = ent->s.origin[2] - 40000;
    tr = gi.trace(ent->s.origin, NULL, NULL, epos, ent, CONTENTS_SOLID);

    // The three camera_end stores read back out of `epos`, not out of
    // tr.endpos -- same values, different code.
    VectorCopy(tr.endpos, epos);
    camera_end[0] = epos[0];
    camera_end[1] = epos[1];
    camera_end[2] = epos[2] + 175;
    tr = gi.trace(epos, NULL, NULL, camera_end, ent, CONTENTS_SOLID);

    VectorCopy(tr.endpos, ent->s.origin);

    if (ent->last_move_framenum < level.framenum) {
        ent->last_move_framenum = level.framenum + 2 * BASE_FRAMERATE;
        ent->s.angles[0] = 25;
        ent->s.angles[1] = 0;
        ent->s.angles[2] = 0;
        VectorCopy(ent->s.angles, ent->client->ps.viewangles);
        VectorCopy(ent->s.angles, ent->client->v_angle);
    }
}

entity_list_t   *pEntityListHead;
unsigned long   ulCount = 0;

void EntityListRemove(edict_t *ent)
{
    entity_list_t   *current;
    entity_list_t   *previous;

    current = pEntityListHead;
    previous = current;

    while (current) {
        if (current->ent->client->osp_t040)
            current->ent->last_move_framenum = level.framenum;

        if (current->ent == ent) {
            if (current == previous)
                pEntityListHead = current->next;
            else
                previous->next = current->next;

            free(current);
            ulCount--;
            current = NULL;
        } else {
            previous = current;
            current = current->next;
        }
    }
}

void EntityListAdd(edict_t *ent)
{
    entity_list_t   *entry;

    entry = malloc(sizeof(entity_list_t));
    entry->ent = ent;
    entry->next = pEntityListHead;
    pEntityListHead = entry;
    ulCount++;
}

unsigned long EntityListNumber(void)
{
    return ulCount;
}

entity_list_t *EntityListHead(void)
{
    if (pEntityListHead)
        return pEntityListHead;
    return NULL;
}

entity_list_t *EntityListNext(entity_list_t *entry)
{
    return entry->next;
}

void PrintEntityList(void)
{
    int             ncount = 0;
    entity_list_t   *newentry;

    gi.dprintf("PrintEntityList\n");

    for (newentry = pEntityListHead; newentry; newentry = newentry->next) {
        gi.dprintf("Name: %s ", newentry->ent->client->pers.netname);
        gi.dprintf("Class: %s\n", newentry->ent->classname);
        ncount++;
    }

    gi.dprintf("Actual Count: %d List Count %lu\n", ncount, EntityListNumber());
}

void EnitityListClean(void)
{
    entity_list_t   *entry;

    while ((entry = EntityListHead()) != 0) {
        EntityListRemove(entry->ent);
    }
}
