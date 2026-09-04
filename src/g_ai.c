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
// g_ai.c

#include "g_local.h"

static bool ai_checkattack(edict_t *self, float dist);

static bool     enemy_vis;
static int      enemy_range;
static float    enemy_yaw;

// ROGUE STUFF
#define SLIDING_TROOPS  1
#define MAX_SIDESTEP    8.0f
//

//============================================================================

/*
=================
AI_SetSightClient

Called once each frame to set level.sight_client to the
player to be checked for in findtarget.

If all clients are either dead or in notarget, sight_client
will be null.

In coop games, sight_client will cycle between the clients.
=================
*/
static edict_t *NextSightClient(edict_t *previous, int excluded_flags)
{
    edict_t *ent;
    int     start, check;

    if (previous == NULL)
        start = 1;
    else
        start = previous - g_edicts;

    check = start;
    while (1) {
        check++;
        if (check > game.maxclients)
            check = 1;
        ent = &g_edicts[check];
        if (ent->inuse && ent->health > 0 && !(ent->flags & excluded_flags))
            return ent;
        if (check == start)
            return NULL;
    }
}

void AI_SetSightClient(void)
{
    /*
     * A base/Xatrix monster may notice a disguised client while an effective
     * Rogue monster may not.  One rotating cache cannot express both answers
     * when Rogue-only monsters coexist with a server running rogue 0.
     */
    level.sight_client = NextSightClient(level.sight_client, FL_NOTARGET);
    level.rogue_sight_client = NextSightClient(level.rogue_sight_client,
                                                FL_NOTARGET | FL_DISGUISED);
}

//============================================================================

/*
=============
ai_move

Move the specified distance at current facing.
This replaces the QC functions: ai_forward, ai_back, ai_pain, and ai_painforward
==============
*/
void ai_move(edict_t *self, float dist)
{
    M_walkmove(self, self->s.angles[YAW], dist);
}

/*
=============
ai_stand

Used for standing around and looking for players
Distance is for slight position adjustments needed by the animations
==============
*/
void ai_stand(edict_t *self, float dist)
{
    vec3_t  v;
    bool retval;
    bool rogue = M_UsesRogueBehavior(self);

    if (dist)
        M_walkmove(self, self->s.angles[YAW], dist);

    if (self->monsterinfo.aiflags & AI_STAND_GROUND) {
        if (self->enemy) {
            VectorSubtract(self->enemy->s.origin, self->s.origin, v);
            self->ideal_yaw = vectoyaw(v);
            if (self->s.angles[YAW] != self->ideal_yaw && self->monsterinfo.aiflags & AI_TEMP_STAND_GROUND) {
                self->monsterinfo.aiflags &= ~(AI_STAND_GROUND | AI_TEMP_STAND_GROUND);
                self->monsterinfo.run(self);
            }
            if (!rogue || !(self->monsterinfo.aiflags & AI_MANUAL_STEERING))
                M_ChangeYaw(self);
            if (rogue) {
                retval = ai_checkattack(self, 0);
                if ((self->enemy) && (self->enemy->inuse) &&
                    visible(self, self->enemy)) {
                    self->monsterinfo.aiflags &= ~AI_LOST_SIGHT;
                    VectorCopy(self->enemy->s.origin, self->monsterinfo.last_sighting);
                    VectorCopy(self->enemy->s.origin, self->monsterinfo.blind_fire_target);
                    self->monsterinfo.trail_framenum = level.framenum;
                    self->monsterinfo.blind_fire_delay = 0;
                } else if (!retval) {
                    FindTarget(self);
                    return;
                }
            } else {
                ai_checkattack(self, 0);
            }
        } else
            FindTarget(self);
        return;
    }

    if (FindTarget(self))
        return;

    if (level.framenum > self->monsterinfo.pause_framenum) {
        self->monsterinfo.walk(self);
        return;
    }

    if (!(self->spawnflags & 1) && (self->monsterinfo.idle) && (level.framenum > self->monsterinfo.idle_framenum)) {
        if (self->monsterinfo.idle_framenum) {
            self->monsterinfo.idle(self);
            if (rogue)
                self->monsterinfo.idle_framenum = level.framenum +
                    (15 + random() * 15) * BASE_FRAMERATE;
            else
                self->monsterinfo.idle_framenum = level.framenum +
                    (1 + random()) * 15 * BASE_FRAMERATE;
        } else {
            self->monsterinfo.idle_framenum = level.framenum + random() * 15 * BASE_FRAMERATE;
        }
    }
}

/*
=============
ai_walk

The monster is walking it's beat
=============
*/
void ai_walk(edict_t *self, float dist)
{
    bool rogue = M_UsesRogueBehavior(self);

    M_MoveToGoal(self, dist);

    // check for noticing a player
    if (FindTarget(self))
        return;

    if ((self->monsterinfo.search) && (level.framenum > self->monsterinfo.idle_framenum)) {
        if (self->monsterinfo.idle_framenum) {
            self->monsterinfo.search(self);
            if (rogue)
                self->monsterinfo.idle_framenum = level.framenum +
                    (15 + random() * 15) * BASE_FRAMERATE;
            else
                self->monsterinfo.idle_framenum = level.framenum +
                    (1 + random()) * 15 * BASE_FRAMERATE;
        } else {
            self->monsterinfo.idle_framenum = level.framenum + random() * 15 * BASE_FRAMERATE;
        }
    }
}

/*
=============
ai_charge

Turns towards target and advances
Use this call with a distance of 0 to replace ai_face
==============
*/
void ai_charge(edict_t *self, float dist)
{
    vec3_t  v;
    float   ofs;

    // Tesla targeting is relationship state shared by both behavior arms.
    if (!self->enemy || !self->enemy->inuse)
        return;

    if (!M_UsesRogueBehavior(self)) {
        VectorSubtract(self->enemy->s.origin, self->s.origin, v);
        self->ideal_yaw = vectoyaw(v);
        M_ChangeYaw(self);

        if (dist)
            M_walkmove(self, self->s.angles[YAW], dist);
        return;
    }

    // PMM - made AI_MANUAL_STEERING affect things differently here .. they turn, but
    // don't set the ideal_yaw

    // PMM - save blindfire target
    if (visible(self, self->enemy))
        VectorCopy(self->enemy->s.origin, self->monsterinfo.blind_fire_target);
    // pmm

    if (!(self->monsterinfo.aiflags & AI_MANUAL_STEERING)) {
        VectorSubtract(self->enemy->s.origin, self->s.origin, v);
        self->ideal_yaw = vectoyaw(v);
//      gi.dprintf ("enemy = %s\n", vtos (self->enemy->s.origin));
//      gi.dprintf ("enemy: ideal yaw is %f\n", self->ideal_yaw);
    }
//  if (!(self->monsterinfo.aiflags & AI_MANUAL_STEERING))
    M_ChangeYaw(self);
// PMM
//  if (dist)
//      M_walkmove (self, self->s.angles[YAW], dist);

    if (dist) {
        if (self->monsterinfo.aiflags & AI_CHARGING) {
            M_MoveToGoal(self, dist);
            return;
        }
        // circle strafe support
        if (self->monsterinfo.attack_state == AS_SLIDING) {
            // if we're fighting a tesla, NEVER circle strafe
            if ((self->enemy) && (self->enemy->classname) && (!strcmp(self->enemy->classname, "tesla")))
                ofs = 0;
            else if (self->monsterinfo.lefty)
                ofs = 90;
            else
                ofs = -90;

            if (M_walkmove(self, self->ideal_yaw + ofs, dist))
                return;

            self->monsterinfo.lefty = 1 - self->monsterinfo.lefty;
            M_walkmove(self, self->ideal_yaw - ofs, dist);
        } else
            M_walkmove(self, self->s.angles[YAW], dist);
    }
// PMM
}

/*
=============
ai_turn

don't move, but turn towards ideal_yaw
Distance is for slight position adjustments needed by the animations
=============
*/
void ai_turn(edict_t *self, float dist)
{
    if (dist)
        M_walkmove(self, self->s.angles[YAW], dist);

    if (FindTarget(self))
        return;

    M_ChangeYaw(self);
}

/*

.enemy
Will be world if not currently angry at anyone.

.movetarget
The next path spot to walk toward.  If .enemy, ignore .movetarget.
When an enemy is killed, the monster will try to return to it's path.

.hunt_time
Set to time + something when the player is in sight, but movement straight for
him is blocked.  This causes the monster to use wall following code for
movement direction instead of sighting on the player.

.ideal_yaw
A yaw angle of the intended direction, which will be turned towards at up
to 45 deg / state.  If the enemy is in view and hunt_time is not active,
this will be the exact line towards the enemy.

.pause_framenum
A monster will leave it's stand state and head towards it's .movetarget when
time > .pause_framenum.

walkmove(angle, speed) primitive is all or nothing
*/

/*
=============
range

returns the range catagorization of an entity reletive to self
0   melee range, will become hostile even if back is turned
1   visibility and infront, or visibility and show hostile
2   infront and show hostile
3   only triggered by damage
=============
*/
int range(edict_t *self, edict_t *other)
{
    vec3_t  v;
    float   len;

    VectorSubtract(self->s.origin, other->s.origin, v);
    len = VectorLength(v);
    if (len < MELEE_DISTANCE)
        return RANGE_MELEE;
    if (len < 500)
        return RANGE_NEAR;
    if (len < 1000)
        return RANGE_MID;
    return RANGE_FAR;
}

/*
=============
visible

returns 1 if the entity is visible to self, even if not infront ()
=============
*/
bool visible(edict_t *self, edict_t *other)
{
    vec3_t  spot1;
    vec3_t  spot2;
    trace_t trace;

    VectorCopy(self->s.origin, spot1);
    spot1[2] += self->viewheight;
    VectorCopy(other->s.origin, spot2);
    spot2[2] += other->viewheight;
    trace = gi.trace(spot1, vec3_origin, vec3_origin, spot2, self, MASK_OPAQUE);

    if (trace.fraction == 1.0f ||
        (M_UsesRogueBehavior(self) && trace.ent == other))
        return true;
    return false;
}

/*
=============
infront

returns 1 if the entity is in front (in sight) of self
=============
*/
bool infront(edict_t *self, edict_t *other)
{
    vec3_t  vec;
    float   dot;
    vec3_t  forward;

    AngleVectors(self->s.angles, forward, NULL, NULL);
    VectorSubtract(other->s.origin, self->s.origin, vec);
    VectorNormalize(vec);
    dot = DotProduct(vec, forward);

    if (dot > 0.3f)
        return true;
    return false;
}

//============================================================================

void HuntTarget(edict_t *self)
{
    vec3_t  vec;

    self->goalentity = self->enemy;
    if (self->monsterinfo.aiflags & AI_STAND_GROUND)
        self->monsterinfo.stand(self);
    else
        self->monsterinfo.run(self);
    VectorSubtract(self->enemy->s.origin, self->s.origin, vec);
    self->ideal_yaw = vectoyaw(vec);
    // wait a while before first attack
    if (!(self->monsterinfo.aiflags & AI_STAND_GROUND))
        AttackFinished(self, 1);
}

void FoundTarget(edict_t *self)
{
    // let other monsters see this monster for a while
    if (self->enemy->client) {
        if (M_UsesRogueBehavior(self) &&
            (self->enemy->flags & FL_DISGUISED)) {
            self->enemy->flags &= ~FL_DISGUISED;
        }

        level.sight_entity = self;
        level.sight_entity_framenum = level.framenum;
        level.sight_entity->light_level = 128;
    }

    self->show_hostile = level.framenum + 1 * BASE_FRAMERATE;        // wake up other monsters

    VectorCopy(self->enemy->s.origin, self->monsterinfo.last_sighting);
    self->monsterinfo.trail_framenum = level.framenum;
    if (M_UsesRogueBehavior(self)) {
        VectorCopy(self->enemy->s.origin, self->monsterinfo.blind_fire_target);
        self->monsterinfo.blind_fire_delay = 0;
    }

    if (!self->combattarget) {
        HuntTarget(self);
        return;
    }

    self->goalentity = self->movetarget = G_PickTarget(self->combattarget);
    if (!self->movetarget) {
        self->goalentity = self->movetarget = self->enemy;
        HuntTarget(self);
        gi.dprintf("%s at %s, combattarget %s not found\n", self->classname, vtos(self->s.origin), self->combattarget);
        return;
    }

    // clear out our combattarget, these are a one shot deal
    self->combattarget = NULL;
    self->monsterinfo.aiflags |= AI_COMBAT_POINT;

    // clear the targetname, that point is ours!
    self->movetarget->targetname = NULL;
    self->monsterinfo.pause_framenum = 0;

    // run for it
    self->monsterinfo.run(self);
}

/*
===========
FindTarget

Self is currently not attacking anything, so try to find a target

Returns TRUE if an enemy was sighted

When a player fires a missile, the point of impact becomes a fakeplayer so
that monsters that see the impact will respond as if they had seen the
player.

To avoid spending too much time, only a single client (or fakeclient) is
checked each frame.  This means multi player games will have slightly
slower noticing monsters.
============
*/
bool FindTarget(edict_t *self)
{
    edict_t     *client;
    bool        heardit;
    bool        rogue;
    int         r;

    rogue = M_UsesRogueBehavior(self);

    if (self->monsterinfo.aiflags & AI_GOOD_GUY) {
        if (self->goalentity && self->goalentity->inuse && self->goalentity->classname)
            if (strcmp(self->goalentity->classname, "target_actor") == 0)
                return false;

        //FIXME look for monsters?
        return false;
    }

    // if we're going to a combat point, just proceed
    if (self->monsterinfo.aiflags & AI_COMBAT_POINT)
        return false;

// if the first spawnflag bit is set, the monster will only wake up on
// really seeing the player, not another monster getting angry or hearing
// something

// revised behavior so they will wake up if they "see" a player make a noise
// but not weapon impact/explosion noises

    heardit = false;
    if ((level.sight_entity_framenum >= (level.framenum - 1)) && !(self->spawnflags & 1)) {
        client = level.sight_entity;
        if (client->enemy == self->enemy)
            return false;
        if (rogue && client->enemy && client->enemy->client &&
            (client->enemy->flags & FL_DISGUISED))
            return false;
    } else if (rogue &&
               level.disguise_violation_framenum > level.framenum &&
               level.disguise_violator) {
        client = level.disguise_violator;
    } else if (level.sound_entity_framenum >= (level.framenum - 1)) {
        client = level.sound_entity;
        heardit = true;
    } else if (!(self->enemy) && (level.sound2_entity_framenum >= (level.framenum - 1)) && !(self->spawnflags & 1)) {
        client = level.sound2_entity;
        heardit = true;
    } else {
        client = rogue ? level.rogue_sight_client : level.sight_client;
        if (!client)
            return false;   // no clients to get mad at
    }

    // if the entity went away, forget it
    if (!client->inuse)
        return false;

    if (client == self->enemy)
        return true;    // JDC false;

    if (rogue && (self->monsterinfo.aiflags & AI_HINT_PATH) && coop &&
        coop->value) {
        heardit = false;
    }

    if (client->client) {
        if (client->flags & FL_NOTARGET)
            return false;
    } else if (client->svflags & SVF_MONSTER) {
        if (!client->enemy)
            return false;
        if (client->enemy->flags & FL_NOTARGET)
            return false;
    } else if (heardit) {
        // Gladiator's NULL check sits OUTSIDE its ROGUE fence
        // (gladq2_src/g_ai.c:597), so under R-200 it is the integration
        // decision for BOTH arms rather than Ground Zero's own addition.
        // The base donor's bare deref is the arm R-211 rejects: it can only
        // differ from this one by crashing.
        if (client->owner && (client->owner->flags & FL_NOTARGET))
            return false;
    } else
        return false;

    if (!heardit) {
        r = range(self, client);

        if (r == RANGE_FAR)
            return false;

// this is where we would check invisibility

        // is client in an spot too dark to be seen?
        if (client->light_level <= 5)
            return false;

        if (!visible(self, client))
            return false;

        if (r == RANGE_NEAR) {
            if (client->show_hostile < level.framenum && !infront(self, client))
                return false;
        } else if (r == RANGE_MID) {
            if (!infront(self, client))
                return false;
        }

        self->enemy = client;

        if (strcmp(self->enemy->classname, "player_noise") != 0) {
            self->monsterinfo.aiflags &= ~AI_SOUND_TARGET;

            if (!self->enemy->client) {
                self->enemy = self->enemy->enemy;
                if (!self->enemy->client) {
                    self->enemy = NULL;
                    return false;
                }
            }
        }
    } else { // heardit
        vec3_t  temp;

        if (self->spawnflags & 1) {
            if (!visible(self, client))
                return false;
        } else {
            if (!gi.inPHS(self->s.origin, client->s.origin))
                return false;
        }

        VectorSubtract(client->s.origin, self->s.origin, temp);

        if (VectorLength(temp) > 1000)  // too far to hear
            return false;

        // check area portals - if they are different and not connected then we can't hear it
        if (client->areanum != self->areanum)
            if (!gi.AreasConnected(self->areanum, client->areanum))
                return false;

        self->ideal_yaw = vectoyaw(temp);
        if (!rogue || !(self->monsterinfo.aiflags & AI_MANUAL_STEERING))
            M_ChangeYaw(self);

        // hunt the sound for a bit; hopefully find the real player
        self->monsterinfo.aiflags |= AI_SOUND_TARGET;
        self->enemy = client;
    }

//
// got one
//
    if (rogue && (self->monsterinfo.aiflags & AI_HINT_PATH)) {
        hintpath_stop(self);
    } else {
        FoundTarget(self);
    }
    if (!(self->monsterinfo.aiflags & AI_SOUND_TARGET) && (self->monsterinfo.sight))
        self->monsterinfo.sight(self, self->enemy);

    return true;
}

//=============================================================================

/*
============
FacingIdeal

============
*/
bool FacingIdeal(edict_t *self)
{
    float   delta;

    delta = anglemod(self->s.angles[YAW] - self->ideal_yaw);
    if (delta > 45 && delta < 315)
        return false;
    return true;
}

//=============================================================================

static bool bq2_M_CheckAttack(edict_t *self)
{
    vec3_t  spot1, spot2;
    float   chance;
    trace_t tr;

    if (self->enemy->health > 0) {
        VectorCopy(self->s.origin, spot1);
        spot1[2] += self->viewheight;
        VectorCopy(self->enemy->s.origin, spot2);
        spot2[2] += self->enemy->viewheight;

        tr = gi.trace(spot1, NULL, NULL, spot2, self,
                      CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_SLIME |
                      CONTENTS_LAVA | CONTENTS_WINDOW);
        if (tr.ent != self->enemy)
            return false;
    }

    if (enemy_range == RANGE_MELEE) {
        if (skill->value == 0 && (Q_rand() & 3))
            return false;
        if (self->monsterinfo.melee)
            self->monsterinfo.attack_state = AS_MELEE;
        else
            self->monsterinfo.attack_state = AS_MISSILE;
        return true;
    }

    if (!self->monsterinfo.attack)
        return false;
    if (level.time < self->monsterinfo.attack_finished)
        return false;
    if (enemy_range == RANGE_FAR)
        return false;

    if (self->monsterinfo.aiflags & AI_STAND_GROUND)
        chance = 0.4f;
    else if (enemy_range == RANGE_MELEE)
        chance = 0.2f;
    else if (enemy_range == RANGE_NEAR)
        chance = 0.1f;
    else if (enemy_range == RANGE_MID)
        chance = 0.02f;
    else
        return false;

    if (skill->value == 0)
        chance *= 0.5f;
    else if (skill->value >= 2)
        chance *= 2;

    if (random() < chance) {
        self->monsterinfo.attack_state = AS_MISSILE;
        self->monsterinfo.attack_finished = level.time + 2 * random();
        return true;
    }

    if (self->flags & FL_FLY) {
        if (random() < 0.3f)
            self->monsterinfo.attack_state = AS_SLIDING;
        else
            self->monsterinfo.attack_state = AS_STRAIGHT;
    }

    return false;
}

bool M_CheckAttack(edict_t *self)
{
    vec3_t  spot1, spot2;
    float   chance = 0;
    trace_t tr;

    if (!M_UsesRogueBehavior(self))
        return bq2_M_CheckAttack(self);

    if (self->enemy->health > 0) {
        // see if any entities are in the way of the shot
        VectorCopy(self->s.origin, spot1);
        spot1[2] += self->viewheight;
        VectorCopy(self->enemy->s.origin, spot2);
        spot2[2] += self->enemy->viewheight;

        tr = gi.trace(spot1, NULL, NULL, spot2, self, CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_SLIME | CONTENTS_LAVA | CONTENTS_WINDOW);

        // do we have a clear shot?
        if (tr.ent != self->enemy) {
            // PGM - we want them to go ahead and shoot at info_notnulls if they can.
            if (self->enemy->solid != SOLID_NOT || tr.fraction < 1.0f) {     //PGM
                // PMM - if we can't see our target, and we're not blocked by a monster, go into blind fire if available
                if ((!(tr.ent->svflags & SVF_MONSTER)) && (!visible(self, self->enemy))) {
                    if ((self->monsterinfo.blindfire) && (self->monsterinfo.blind_fire_delay <= 20.0f)) {
                        if (level.time < self->monsterinfo.attack_finished) {
                            return false;
                        }
                        if (level.framenum < (self->monsterinfo.trail_framenum + self->monsterinfo.blind_fire_delay * BASE_FRAMERATE)) {
                            // wait for our time
                            return false;
                        } else {
//  gi.WriteByte (svc_temp_entity);
//  gi.WriteByte (TE_DEBUGTRAIL);
//  gi.WritePosition (spot1);
//  gi.WritePosition (self->monsterinfo.blind_fire_target);
//  gi.multicast (self->s.origin, MULTICAST_ALL);
                            // make sure we're not going to shoot a monster
                            tr = gi.trace(spot1, NULL, NULL, self->monsterinfo.blind_fire_target, self, CONTENTS_MONSTER);
                            if (tr.allsolid || tr.startsolid || ((tr.fraction < 1.0f) && (tr.ent != self->enemy))) {
//                              if ((g_showlogic) && (g_showlogic->value))
//                                  gi.dprintf ("blindfire blocked\n");
                                return false;
                            }

                            self->monsterinfo.attack_state = AS_BLIND;
                            return true;
                        }
                    }
                }
                // pmm
                return false;
            }
        }
    }

    // melee attack
    if (enemy_range == RANGE_MELEE) {
        // don't always melee in easy mode
        if (skill->value == 0 && (Q_rand() & 3)) {
            // PMM - fix for melee only monsters & strafing
            self->monsterinfo.attack_state = AS_STRAIGHT;
            return false;
        }
        if (self->monsterinfo.melee)
            self->monsterinfo.attack_state = AS_MELEE;
        else
            self->monsterinfo.attack_state = AS_MISSILE;
        return true;
    }

// missile attack
    if (!self->monsterinfo.attack) {
        // PMM - fix for melee only monsters & strafing
        self->monsterinfo.attack_state = AS_STRAIGHT;
        return false;
    }

    if (level.time < self->monsterinfo.attack_finished)
        return false;

    if (enemy_range == RANGE_FAR)
        return false;

    if (self->monsterinfo.aiflags & AI_STAND_GROUND)
        chance = 0.4f;
    else if (enemy_range == RANGE_MELEE)
        chance = 0.2f;
    else if (enemy_range == RANGE_NEAR)
        chance = 0.1f;
    else if (enemy_range == RANGE_MID)
        chance = 0.02f;
    else
        return false;

    if (skill->value == 0)
        chance *= 0.5f;
    else if (skill->value >= 2)
        chance *= 2;

    // PGM - go ahead and shoot every time if it's a info_notnull
    if ((random() < chance) || (self->enemy->solid == SOLID_NOT)) {
        self->monsterinfo.attack_state = AS_MISSILE;
        self->monsterinfo.attack_finished = level.time + 2 * random();
        return true;
    }

    // PMM -daedalus should strafe more .. this can be done here or in a customized
    // check_attack code for the hover.
    if (self->flags & FL_FLY) {
        // originally, just 0.3
        float strafe_chance;
        if (!(strcmp(self->classname, "monster_daedalus")))
            strafe_chance = 0.8f;
        else
            strafe_chance = 0.6f;

        // if enemy is tesla, never strafe
        if ((self->enemy) && (self->enemy->classname) && (!strcmp(self->enemy->classname, "tesla")))
            strafe_chance = 0;

        if (random() < strafe_chance)
            self->monsterinfo.attack_state = AS_SLIDING;
        else
            self->monsterinfo.attack_state = AS_STRAIGHT;
    }
// do we want the monsters strafing?
#ifdef SLIDING_TROOPS
    else {
        if (random() < 0.4f)
            self->monsterinfo.attack_state = AS_SLIDING;
        else
            self->monsterinfo.attack_state = AS_STRAIGHT;
    }
#endif
//-PMM

    return false;
}

/*
=============
ai_run_melee

Turn and close until within an angle to launch a melee attack
=============
*/
static void ai_run_melee(edict_t *self)
{
    if (!M_UsesRogueBehavior(self)) {
        self->ideal_yaw = enemy_yaw;
        M_ChangeYaw(self);

        if (FacingIdeal(self)) {
            self->monsterinfo.melee(self);
            self->monsterinfo.attack_state = AS_STRAIGHT;
        }
        return;
    }

    self->ideal_yaw = enemy_yaw;
    if (!(self->monsterinfo.aiflags & AI_MANUAL_STEERING))
        M_ChangeYaw(self);

    if (FacingIdeal(self)) {
        self->monsterinfo.melee(self);
        self->monsterinfo.attack_state = AS_STRAIGHT;
    }
}

/*
=============
ai_run_missile

Turn in place until within an angle to launch a missile attack
=============
*/
static void ai_run_missile(edict_t *self)
{
    if (!M_UsesRogueBehavior(self)) {
        self->ideal_yaw = enemy_yaw;
        M_ChangeYaw(self);

        if (FacingIdeal(self)) {
            self->monsterinfo.attack(self);
            self->monsterinfo.attack_state = AS_STRAIGHT;
        }
        return;
    }

    self->ideal_yaw = enemy_yaw;
    if (!(self->monsterinfo.aiflags & AI_MANUAL_STEERING))
        M_ChangeYaw(self);

    if (FacingIdeal(self)) {
        self->monsterinfo.attack(self);
//      if (self->monsterinfo.attack_state == AS_MISSILE)
        if ((self->monsterinfo.attack_state == AS_MISSILE) || (self->monsterinfo.attack_state == AS_BLIND))
            self->monsterinfo.attack_state = AS_STRAIGHT;
//      else if (self->monsterinfo.attack_state != AS_SLIDING)
//          gi.dprintf ("ai_run_missile: Unexpected attack state %d !\n", self->monsterinfo.attack_state);
    }
}

/*
=============
ai_run_slide

Strafe sideways, but stay at aproximately the same range
=============
*/
static void ai_run_slide(edict_t *self, float distance)
{
    float   ofs;
    float   angle;

    if (!M_UsesRogueBehavior(self)) {
        self->ideal_yaw = enemy_yaw;
        M_ChangeYaw(self);

        if (self->monsterinfo.lefty)
            ofs = 90;
        else
            ofs = -90;

        if (M_walkmove(self, self->ideal_yaw + ofs, distance))
            return;

        self->monsterinfo.lefty = 1 - self->monsterinfo.lefty;
        M_walkmove(self, self->ideal_yaw - ofs, distance);
        return;
    }

    self->ideal_yaw = enemy_yaw;

//  if (self->flags & FL_FLY)
//      angle = 90;
//  else
//      angle = 45;

    angle = 90;

    if (self->monsterinfo.lefty)
        ofs = angle;
    else
        ofs = -angle;
//
//  if (!(self->flags & FL_FLY))
//  {
//      // non fliers should actually turn towards the direction their trying to run
//      self->ideal_yaw += ofs;
//  }
//
    if (!(self->monsterinfo.aiflags & AI_MANUAL_STEERING))
        M_ChangeYaw(self);

    /*
    if (!(self->flags & FL_FLY))
    {
        if (M_walkmove (self, self->ideal_yaw + ofs, distance))
            return;
    }
    else
    {
        if (M_walkmove (self, self->ideal_yaw, distance))
            return;
    }
    */
    // ROGUE, and a latent bug in shipped Ground Zero rather than a merge
    // artifact.  The comment says "clamp maximum sideways move for non flyers",
    // and the intent was plainly `!(self->flags & FL_FLY)`.  What it computes is
    // `(!self->flags) & FL_FLY`: `!flags` is 0 or 1, FL_FLY is BIT(0), so the
    // clamp applies only when the entity has NO flags set at all -- never for a
    // flyer (which has FL_FLY) and never for any non-flyer carrying any other
    // flag.  The jerkiness smoothing has therefore never operated.
    //
    // R-30 kept the broken grouping, with the reason that turning the clamp on
    // would change how every Ground Zero monster sidesteps -- a gameplay change
    // disguised as a warning fix, which §7 rule 2 refuses.  **That decision is
    // reversed here (R-148), because the donor has made it**: `q2pro@21381ffa`
    // writes `!(self->flags & FL_FLY)` on the mission-packs branch, so taking it
    // is following the donor rather than overruling it, and §7 rule 7 makes a
    // Q2PRO fix cumulative.  The clamp now does what its own comment says --
    // "clamp maximum sideways move for non flyers" -- and Ground Zero's
    // sidestep smoothing operates for the first time since 1998.
    //
    // Found by clang; gcc is silent.
    if (!(self->flags & FL_FLY))
        distance = min(distance, MAX_SIDESTEP);
    if (M_walkmove(self, self->ideal_yaw + ofs, distance))
        return;
    // PMM - if we're dodging, give up on it and go straight
    if (self->monsterinfo.aiflags & AI_DODGING) {
        monster_done_dodge(self);
        // by setting as_straight, caller will know to try straight move
        self->monsterinfo.attack_state = AS_STRAIGHT;
        return;
    }

    self->monsterinfo.lefty = 1 - self->monsterinfo.lefty;
    if (M_walkmove(self, self->ideal_yaw - ofs, distance))
        return;
    // PMM - if we're dodging, give up on it and go straight
    if (self->monsterinfo.aiflags & AI_DODGING)
        monster_done_dodge(self);

    // PMM - the move failed, so signal the caller (ai_run) to try going straight
    self->monsterinfo.attack_state = AS_STRAIGHT;
    /*
    if (!(self->flags & FL_FLY))
    {
        M_walkmove (self, self->ideal_yaw + ofs, distance);
    }
    else
    {
        M_walkmove (self, self->ideal_yaw, distance);
    }*/
}

/*
=============
ai_checkattack

Decides if we're going to attack or do something else
used by ai_run and ai_stand
=============
*/
static bool bq2_ai_checkattack(edict_t *self, float dist)
{
    vec3_t  temp;
    bool    hesDeadJim;

    if (self->goalentity) {
        if (self->monsterinfo.aiflags & AI_COMBAT_POINT)
            return false;

        if (self->monsterinfo.aiflags & AI_SOUND_TARGET) {
            if ((level.framenum - self->enemy->last_sound_framenum) >
                5.0f * BASE_FRAMERATE) {
                if (self->goalentity == self->enemy) {
                    if (self->movetarget)
                        self->goalentity = self->movetarget;
                    else
                        self->goalentity = NULL;
                }
                self->monsterinfo.aiflags &= ~AI_SOUND_TARGET;
                if (self->monsterinfo.aiflags & AI_TEMP_STAND_GROUND)
                    self->monsterinfo.aiflags &=
                        ~(AI_STAND_GROUND | AI_TEMP_STAND_GROUND);
            } else {
                self->show_hostile = level.framenum + BASE_FRAMERATE;
                return false;
            }
        }
    }

    enemy_vis = false;
    hesDeadJim = false;
    if (!self->enemy || !self->enemy->inuse) {
        hesDeadJim = true;
    } else if (self->monsterinfo.aiflags & AI_MEDIC) {
        if (self->enemy->health > 0) {
            hesDeadJim = true;
            self->monsterinfo.aiflags &= ~AI_MEDIC;
        }
    } else if (self->monsterinfo.aiflags & AI_BRUTAL) {
        if (self->enemy->health <= -80)
            hesDeadJim = true;
    } else if (self->enemy->health <= 0) {
        hesDeadJim = true;
    }

    if (hesDeadJim) {
        self->enemy = NULL;
        if (self->oldenemy && self->oldenemy->health > 0) {
            self->enemy = self->oldenemy;
            self->oldenemy = NULL;
            HuntTarget(self);
        } else {
            if (self->movetarget) {
                self->goalentity = self->movetarget;
                self->monsterinfo.walk(self);
            } else {
                self->monsterinfo.pause_framenum = INT_MAX;
                self->monsterinfo.stand(self);
            }
            return true;
        }
    }

    self->show_hostile = level.framenum + BASE_FRAMERATE;
    enemy_vis = visible(self, self->enemy);
    if (enemy_vis) {
        self->monsterinfo.search_framenum =
            level.framenum + 5 * BASE_FRAMERATE;
        VectorCopy(self->enemy->s.origin, self->monsterinfo.last_sighting);
    }

    enemy_range = range(self, self->enemy);
    VectorSubtract(self->enemy->s.origin, self->s.origin, temp);
    enemy_yaw = vectoyaw(temp);

    if (self->monsterinfo.attack_state == AS_MISSILE) {
        ai_run_missile(self);
        return true;
    }
    if (self->monsterinfo.attack_state == AS_MELEE) {
        ai_run_melee(self);
        return true;
    }

    if (!enemy_vis)
        return false;

    return self->monsterinfo.checkattack(self);
}

static bool ai_checkattack(edict_t *self, float dist)
{
    vec3_t      temp;
    bool    hesDeadJim;
    // PMM
    bool    retval;

    if (!M_UsesRogueBehavior(self))
        return bq2_ai_checkattack(self, dist);

// this causes monsters to run blindly to the combat point w/o firing
    if (self->goalentity) {
        if (self->monsterinfo.aiflags & AI_COMBAT_POINT)
            return false;

        if (self->monsterinfo.aiflags & AI_SOUND_TARGET) {
            if ((level.framenum - self->enemy->last_sound_framenum) > 5.0f * BASE_FRAMERATE) {
                if (self->goalentity == self->enemy) {
                    if (self->movetarget)
                        self->goalentity = self->movetarget;
                    else
                        self->goalentity = NULL;
                }
                self->monsterinfo.aiflags &= ~AI_SOUND_TARGET;
                if (self->monsterinfo.aiflags & AI_TEMP_STAND_GROUND)
                    self->monsterinfo.aiflags &= ~(AI_STAND_GROUND | AI_TEMP_STAND_GROUND);
            } else {
                self->show_hostile = level.framenum + 1 * BASE_FRAMERATE;
                return false;
            }
        }
    }

    enemy_vis = false;

// see if the enemy is dead
    hesDeadJim = false;
    if ((!self->enemy) || (!self->enemy->inuse)) {
        hesDeadJim = true;
    } else if (self->monsterinfo.aiflags & AI_MEDIC) {
        if (!(self->enemy->inuse) || (self->enemy->health > 0)) {
            hesDeadJim = true;
//          self->monsterinfo.aiflags &= ~AI_MEDIC;
        }
    } else {
        if (self->monsterinfo.aiflags & AI_BRUTAL) {
            if (self->enemy->health <= -80)
                hesDeadJim = true;
        } else {
            if (self->enemy->health <= 0)
                hesDeadJim = true;
        }
    }

    if (hesDeadJim) {
        self->monsterinfo.aiflags &= ~AI_MEDIC;
        self->enemy = NULL;
        // FIXME: look all around for other targets
        if (self->oldenemy && self->oldenemy->health > 0) {
            self->enemy = self->oldenemy;
            self->oldenemy = NULL;
            HuntTarget(self);
        }
//ROGUE - multiple teslas make monsters lose track of the player.
        else if (self->monsterinfo.last_player_enemy && self->monsterinfo.last_player_enemy->health > 0) {
//          if ((g_showlogic) && (g_showlogic->value))
//              gi.dprintf("resorting to last_player_enemy...\n");
            self->enemy = self->monsterinfo.last_player_enemy;
            self->oldenemy = NULL;
            self->monsterinfo.last_player_enemy = NULL;
            HuntTarget(self);
        }
//ROGUE
        else {
            if (self->movetarget) {
                self->goalentity = self->movetarget;
                self->monsterinfo.walk(self);
            } else {
                // we need the pause_framenum otherwise the stand code
                // will just revert to walking with no target and
                // the monsters will wonder around aimlessly trying
                // to hunt the world entity
                self->monsterinfo.pause_framenum = level.framenum + 100000000 * BASE_FRAMERATE;
                self->monsterinfo.stand(self);
            }
            return true;
        }
    }

    self->show_hostile = level.framenum + 1 * BASE_FRAMERATE;        // wake up other monsters

// check knowledge of enemy
    enemy_vis = visible(self, self->enemy);
    if (enemy_vis) {
        self->monsterinfo.search_framenum = level.framenum + 5 * BASE_FRAMERATE;
        VectorCopy(self->enemy->s.origin, self->monsterinfo.last_sighting);
        // PMM
        self->monsterinfo.aiflags &= ~AI_LOST_SIGHT;
        self->monsterinfo.trail_framenum = level.framenum;
        VectorCopy(self->enemy->s.origin, self->monsterinfo.blind_fire_target);
        self->monsterinfo.blind_fire_delay = 0;
        // pmm
    }

// look for other coop players here
//  if (coop && self->monsterinfo.search_framenum < level.framenum)
//  {
//      if (FindTarget (self))
//          return true;
//  }

    enemy_range = range(self, self->enemy);
    VectorSubtract(self->enemy->s.origin, self->s.origin, temp);
    enemy_yaw = vectoyaw(temp);

    // JDC self->ideal_yaw = enemy_yaw;

    // PMM -- reordered so the monster specific checkattack is called before the run_missle/melee/checkvis
    // stuff .. this allows for, among other things, circle strafing and attacking while in ai_run
    retval = self->monsterinfo.checkattack(self);
    if (retval) {
        // PMM
        if (self->monsterinfo.attack_state == AS_MISSILE) {
            ai_run_missile(self);
            return true;
        }
        if (self->monsterinfo.attack_state == AS_MELEE) {
            ai_run_melee(self);
            return true;
        }
        // PMM -- added so monsters can shoot blind
        if (self->monsterinfo.attack_state == AS_BLIND) {
            ai_run_missile(self);
            return true;
        }
        // pmm

        // if enemy is not currently visible, we will never attack
        if (!enemy_vis)
            return false;
        // PMM
    }
    return retval;
    // PMM
//  return self->monsterinfo.checkattack (self);
}

/*
=============
ai_run

The monster has an enemy it is trying to kill
=============
*/
static void bq2_ai_run(edict_t *self, float dist)
{
    vec3_t  v;
    edict_t *tempgoal;
    edict_t *save;
    bool    new;
    edict_t *marker;
    float   d1, d2;
    trace_t tr;
    vec3_t  v_forward, v_right;
    float   left, center, right;
    vec3_t  left_target, right_target;

    if (self->monsterinfo.aiflags & AI_COMBAT_POINT) {
        M_MoveToGoal(self, dist);
        return;
    }

    if (self->monsterinfo.aiflags & AI_SOUND_TARGET) {
        VectorSubtract(self->s.origin, self->enemy->s.origin, v);
        if (VectorLength(v) < 64) {
            self->monsterinfo.aiflags |=
                AI_STAND_GROUND | AI_TEMP_STAND_GROUND;
            self->monsterinfo.stand(self);
            return;
        }

        M_MoveToGoal(self, dist);
        if (!FindTarget(self))
            return;
    }

    if (ai_checkattack(self, dist))
        return;

    if (self->monsterinfo.attack_state == AS_SLIDING) {
        ai_run_slide(self, dist);
        return;
    }

    if (enemy_vis) {
        M_MoveToGoal(self, dist);
        self->monsterinfo.aiflags &= ~AI_LOST_SIGHT;
        VectorCopy(self->enemy->s.origin, self->monsterinfo.last_sighting);
        self->monsterinfo.trail_framenum = level.framenum;
        return;
    }

    if (coop->value) {
        if (FindTarget(self))
            return;
    }

    if (self->monsterinfo.search_framenum &&
        level.framenum > self->monsterinfo.search_framenum +
                         20 * BASE_FRAMERATE) {
        M_MoveToGoal(self, dist);
        self->monsterinfo.search_framenum = 0;
        return;
    }

    save = self->goalentity;
    tempgoal = G_Spawn();
    self->goalentity = tempgoal;
    new = false;

    if (!(self->monsterinfo.aiflags & AI_LOST_SIGHT)) {
        self->monsterinfo.aiflags |= AI_LOST_SIGHT | AI_PURSUIT_LAST_SEEN;
        self->monsterinfo.aiflags &= ~(AI_PURSUE_NEXT | AI_PURSUE_TEMP);
        new = true;
    }

    if (self->monsterinfo.aiflags & AI_PURSUE_NEXT) {
        self->monsterinfo.aiflags &= ~AI_PURSUE_NEXT;
        self->monsterinfo.search_framenum =
            level.framenum + 5 * BASE_FRAMERATE;

        if (self->monsterinfo.aiflags & AI_PURSUE_TEMP) {
            self->monsterinfo.aiflags &= ~AI_PURSUE_TEMP;
            marker = NULL;
            VectorCopy(self->monsterinfo.saved_goal,
                       self->monsterinfo.last_sighting);
            new = true;
        } else if (self->monsterinfo.aiflags & AI_PURSUIT_LAST_SEEN) {
            self->monsterinfo.aiflags &= ~AI_PURSUIT_LAST_SEEN;
            marker = PlayerTrail_PickFirst(self);
        } else {
            marker = PlayerTrail_PickNext(self);
        }

        if (marker) {
            VectorCopy(marker->s.origin, self->monsterinfo.last_sighting);
            self->monsterinfo.trail_framenum = marker->timestamp;
            self->s.angles[YAW] = self->ideal_yaw = marker->s.angles[YAW];
            new = true;
        }
    }

    VectorSubtract(self->s.origin, self->monsterinfo.last_sighting, v);
    d1 = VectorLength(v);
    if (d1 <= dist) {
        self->monsterinfo.aiflags |= AI_PURSUE_NEXT;
        dist = d1;
    }

    VectorCopy(self->monsterinfo.last_sighting, self->goalentity->s.origin);

    if (new) {
        tr = gi.trace(self->s.origin, self->mins, self->maxs,
                      self->monsterinfo.last_sighting, self, MASK_PLAYERSOLID);
        if (tr.fraction < 1) {
            VectorSubtract(self->goalentity->s.origin, self->s.origin, v);
            d1 = VectorLength(v);
            center = tr.fraction;
            d2 = d1 * ((center + 1) / 2);
            self->s.angles[YAW] = self->ideal_yaw = vectoyaw(v);
            AngleVectors(self->s.angles, v_forward, v_right, NULL);

            VectorSet(v, d2, -16, 0);
            G_ProjectSource(self->s.origin, v, v_forward, v_right, left_target);
            tr = gi.trace(self->s.origin, self->mins, self->maxs, left_target,
                          self, MASK_PLAYERSOLID);
            left = tr.fraction;

            VectorSet(v, d2, 16, 0);
            G_ProjectSource(self->s.origin, v, v_forward, v_right, right_target);
            tr = gi.trace(self->s.origin, self->mins, self->maxs, right_target,
                          self, MASK_PLAYERSOLID);
            right = tr.fraction;

            center = (d1 * center) / d2;
            if (left >= center && left > right) {
                if (left < 1) {
                    VectorSet(v, d2 * left * 0.5f, -16, 0);
                    G_ProjectSource(self->s.origin, v, v_forward, v_right,
                                    left_target);
                }
                VectorCopy(self->monsterinfo.last_sighting,
                           self->monsterinfo.saved_goal);
                self->monsterinfo.aiflags |= AI_PURSUE_TEMP;
                VectorCopy(left_target, self->goalentity->s.origin);
                VectorCopy(left_target, self->monsterinfo.last_sighting);
                VectorSubtract(self->goalentity->s.origin, self->s.origin, v);
                self->s.angles[YAW] = self->ideal_yaw = vectoyaw(v);
            } else if (right >= center && right > left) {
                if (right < 1) {
                    VectorSet(v, d2 * right * 0.5f, 16, 0);
                    G_ProjectSource(self->s.origin, v, v_forward, v_right,
                                    right_target);
                }
                VectorCopy(self->monsterinfo.last_sighting,
                           self->monsterinfo.saved_goal);
                self->monsterinfo.aiflags |= AI_PURSUE_TEMP;
                VectorCopy(right_target, self->goalentity->s.origin);
                VectorCopy(right_target, self->monsterinfo.last_sighting);
                VectorSubtract(self->goalentity->s.origin, self->s.origin, v);
                self->s.angles[YAW] = self->ideal_yaw = vectoyaw(v);
            }
        }
    }

    M_MoveToGoal(self, dist);
    G_FreeEdict(tempgoal);

    if (self->inuse)
        self->goalentity = save;
}

void ai_run(edict_t *self, float dist)
{
    vec3_t      v;
    edict_t     *tempgoal;
    edict_t     *save;
    bool        new;
    edict_t     *marker;
    float       d1, d2;
    trace_t     tr;
    vec3_t      v_forward, v_right;
    float       left, center, right;
    vec3_t      left_target, right_target;
    //PMM
    bool    retval;
    bool    alreadyMoved = false;
    bool    gotcha = false;
    edict_t     *realEnemy;

    if (!M_UsesRogueBehavior(self)) {
        bq2_ai_run(self, dist);
        return;
    }

    // if we're going to a combat point, just proceed
    if (self->monsterinfo.aiflags & AI_COMBAT_POINT) {
        M_MoveToGoal(self, dist);
        return;
    }

    // PMM
    if (self->monsterinfo.aiflags & AI_DUCKED) {
//      if ((g_showlogic) && (g_showlogic->value))
//          gi.dprintf ("%s - duck flag cleaned up!\n", self->classname);
        self->monsterinfo.aiflags &= ~AI_DUCKED;
    }
    if (self->maxs[2] != self->monsterinfo.base_height) {
//      if ((g_showlogic) && (g_showlogic->value))
//          gi.dprintf ("%s - ducked height corrected!\n", self->classname);
        monster_duck_up(self);
    }
//  if ((self->monsterinfo.aiflags & AI_MANUAL_STEERING) && (strcmp(self->classname, "monster_turret")))
//  {
//      if ((g_showlogic) && (g_showlogic->value))
//          gi.dprintf ("%s - manual steering in ai_run!\n", self->classname);
//  }
    // pmm

//==========
//PGM
    // if we're currently looking for a hint path
    if (self->monsterinfo.aiflags & AI_HINT_PATH) {
        // determine direction to our destination hintpath.
        // FIXME - is this needed EVERY time? I was having trouble with them
        // sometimes not going after it, and this fixed it.
//      VectorSubtract(self->movetarget->s.origin, self->s.origin, v);
//      vectoangles(v, v_forward);
//      self->ideal_yaw = v_forward[YAW];
//      gi.dprintf("seeking hintpath. origin: %s %0.1f\n", vtos(v), self->ideal_yaw);
        M_MoveToGoal(self, dist);
        if (!self->inuse)
            return;         // PGM - g_touchtrigger free problem
//      return;

        // if we've already seen the player, and can't see him now, return
//      if(self->enemy && !visible(self, self->enemy))
//          return;

        // if not and can't find the player, return
//      if(!FindTarget(self))
//          return;

        // first off, make sure we're looking for the player, not a noise he made
        if (self->enemy) {
            if (self->enemy->inuse) {
                if (strcmp(self->enemy->classname, "player_noise") != 0)
                    realEnemy = self->enemy;
                else if (self->enemy->owner)
                    realEnemy = self->enemy->owner;
                else { // uh oh, can't figure out enemy, bail
                    self->enemy = NULL;
                    hintpath_stop(self);
                    return;
                }
            } else {
                self->enemy = NULL;
                hintpath_stop(self);
                return;
            }
        } else {
            hintpath_stop(self);
            return;
        }

        if (coop && coop->value) {
            // if we're in coop, check my real enemy first .. if I SEE him, set gotcha to true
            if (self->enemy && visible(self, realEnemy))
                gotcha = true;
            else // otherwise, let FindTarget bump us out of hint paths, if appropriate
                FindTarget(self);
        } else {
            if (self->enemy && visible(self, realEnemy))
                gotcha = true;
        }

        // if we see the player, stop following hintpaths.
        if (gotcha) {
//          if(g_showlogic && g_showlogic->value)
//              gi.dprintf("stopped following hint paths in ai_run\n");

            // disconnect from hintpaths and start looking normally for players.
            hintpath_stop(self);
            // pmm - no longer needed, since hintpath_stop does it
//          HuntTarget(self);
        }
        return;
    }
//PGM
//==========

    if (self->monsterinfo.aiflags & AI_SOUND_TARGET) {
        // PMM - paranoia checking
        if (self->enemy)
            VectorSubtract(self->s.origin, self->enemy->s.origin, v);

        if ((!self->enemy) || (VectorLength(v) < 64))
            // pmm
        {
            self->monsterinfo.aiflags |= (AI_STAND_GROUND | AI_TEMP_STAND_GROUND);
            self->monsterinfo.stand(self);
            return;
        }

        M_MoveToGoal(self, dist);
        // PMM - prevent double moves for sound_targets
        alreadyMoved = true;
        // pmm
        if (!self->inuse)
            return;         // PGM - g_touchtrigger free problem

        if (!FindTarget(self))
            return;
    }

    // PMM -- moved ai_checkattack up here so the monsters can attack while strafing or charging

    // PMM -- if we're dodging, make sure to keep the attack_state AS_SLIDING

    retval = ai_checkattack(self, dist);

    // PMM - don't strafe if we can't see our enemy
    if ((!enemy_vis) && (self->monsterinfo.attack_state == AS_SLIDING))
        self->monsterinfo.attack_state = AS_STRAIGHT;
    // unless we're dodging (dodging out of view looks smart)
    if (self->monsterinfo.aiflags & AI_DODGING)
        self->monsterinfo.attack_state = AS_SLIDING;
    // pmm

    if (self->monsterinfo.attack_state == AS_SLIDING) {
        // PMM - protect against double moves
        if (!alreadyMoved)
            ai_run_slide(self, dist);
        // PMM
        // we're using attack_state as the return value out of ai_run_slide to indicate whether or not the
        // move succeeded.  If the move succeeded, and we're still sliding, we're done in here (since we've
        // had our chance to shoot in ai_checkattack, and have moved).
        // if the move failed, our state is as_straight, and it will be taken care of below
        if ((!retval) && (self->monsterinfo.attack_state == AS_SLIDING))
            return;
    } else if (self->monsterinfo.aiflags & AI_CHARGING) {
        self->ideal_yaw = enemy_yaw;
        if (!(self->monsterinfo.aiflags & AI_MANUAL_STEERING))
            M_ChangeYaw(self);
    }
    if (retval) {
        // PMM - is this useful?  Monsters attacking usually call the ai_charge routine..
        // the only monster this affects should be the soldier
        if ((dist != 0) && (!alreadyMoved) && (self->monsterinfo.attack_state == AS_STRAIGHT) && (!(self->monsterinfo.aiflags & AI_STAND_GROUND))) {
            M_MoveToGoal(self, dist);
        }
        if ((self->enemy) && (self->enemy->inuse) && (enemy_vis)) {
            self->monsterinfo.aiflags &= ~AI_LOST_SIGHT;
            VectorCopy(self->enemy->s.origin, self->monsterinfo.last_sighting);
            self->monsterinfo.trail_framenum = level.framenum;
            //PMM
            VectorCopy(self->enemy->s.origin, self->monsterinfo.blind_fire_target);
            self->monsterinfo.blind_fire_delay = 0;
            //pmm
        }
        return;
    }
    //PMM
//  if (ai_checkattack (self, dist))
//      return;

//  if (self->monsterinfo.attack_state == AS_SLIDING)
//  {
//      ai_run_slide (self, dist);
//      return;
//  }

    // PGM - added a little paranoia checking here... 9/22/98
    if ((self->enemy) && (self->enemy->inuse) && (enemy_vis)) {
        // PMM - check for alreadyMoved
        if (!alreadyMoved)
            M_MoveToGoal(self, dist);
        if (!self->inuse)
            return;         // PGM - g_touchtrigger free problem


        self->monsterinfo.aiflags &= ~AI_LOST_SIGHT;
        VectorCopy(self->enemy->s.origin, self->monsterinfo.last_sighting);
        self->monsterinfo.trail_framenum = level.framenum;
        // PMM
        VectorCopy(self->enemy->s.origin, self->monsterinfo.blind_fire_target);
        self->monsterinfo.blind_fire_delay = 0;
        // pmm
        return;
    }

//=======
//PGM
    // if we've been looking (unsuccessfully) for the player for 10 seconds
    // PMM - reduced to 5, makes them much nastier
    if ((self->monsterinfo.trail_framenum + 5 * BASE_FRAMERATE) <= level.framenum) {
        // and we haven't checked for valid hint paths in the last 10 seconds
        if ((self->monsterinfo.last_hint_framenum + 10 * BASE_FRAMERATE) <= level.framenum) {
            // check for hint_paths.
            self->monsterinfo.last_hint_framenum = level.framenum;
            if (monsterlost_checkhint(self))
                return;
        }
    }
//PGM
//=======

// PMM - moved down here to allow monsters to get on hint paths
    // coop will change to another enemy if visible
    if (coop->value) {
        // FIXME: insane guys get mad with this, which causes crashes!
        if (FindTarget(self))
            return;
    }
// pmm

    if ((self->monsterinfo.search_framenum) && (level.framenum > (self->monsterinfo.search_framenum + 20 * BASE_FRAMERATE))) {
        // PMM - double move protection
        if (!alreadyMoved)
            M_MoveToGoal(self, dist);
        self->monsterinfo.search_framenum = 0;

        return;
    }

    save = self->goalentity;
    tempgoal = G_Spawn();
    self->goalentity = tempgoal;

    new = false;

    if (!(self->monsterinfo.aiflags & AI_LOST_SIGHT)) {
        // just lost sight of the player, decide where to go first

        self->monsterinfo.aiflags |= (AI_LOST_SIGHT | AI_PURSUIT_LAST_SEEN);
        self->monsterinfo.aiflags &= ~(AI_PURSUE_NEXT | AI_PURSUE_TEMP);
        new = true;
    }

    if (self->monsterinfo.aiflags & AI_PURSUE_NEXT) {
//      vec3_t  debug_vec;

        self->monsterinfo.aiflags &= ~AI_PURSUE_NEXT;


        // give ourself more time since we got this far
        self->monsterinfo.search_framenum = level.framenum + 5 * BASE_FRAMERATE;

        if (self->monsterinfo.aiflags & AI_PURSUE_TEMP) {

            self->monsterinfo.aiflags &= ~AI_PURSUE_TEMP;
            marker = NULL;
            VectorCopy(self->monsterinfo.saved_goal, self->monsterinfo.last_sighting);
            new = true;
        } else if (self->monsterinfo.aiflags & AI_PURSUIT_LAST_SEEN) {
            self->monsterinfo.aiflags &= ~AI_PURSUIT_LAST_SEEN;
            marker = PlayerTrail_PickFirst(self);
        } else {
            marker = PlayerTrail_PickNext(self);
        }

        if (marker) {
            VectorCopy(marker->s.origin, self->monsterinfo.last_sighting);
            self->monsterinfo.trail_framenum = marker->timestamp;
            self->s.angles[YAW] = self->ideal_yaw = marker->s.angles[YAW];

            new = true;
        }
    }

    VectorSubtract(self->s.origin, self->monsterinfo.last_sighting, v);
    d1 = VectorLength(v);
    if (d1 <= dist) {
        self->monsterinfo.aiflags |= AI_PURSUE_NEXT;
        dist = d1;
    }

    VectorCopy(self->monsterinfo.last_sighting, self->goalentity->s.origin);

    if (new) {
        tr = gi.trace(self->s.origin, self->mins, self->maxs, self->monsterinfo.last_sighting, self, MASK_PLAYERSOLID);
        if (tr.fraction < 1) {
            VectorSubtract(self->goalentity->s.origin, self->s.origin, v);
            d1 = VectorLength(v);
            center = tr.fraction;
            d2 = d1 * ((center + 1) / 2);
            self->s.angles[YAW] = self->ideal_yaw = vectoyaw(v);
            AngleVectors(self->s.angles, v_forward, v_right, NULL);

            VectorSet(v, d2, -16, 0);
            G_ProjectSource(self->s.origin, v, v_forward, v_right, left_target);
            tr = gi.trace(self->s.origin, self->mins, self->maxs, left_target, self, MASK_PLAYERSOLID);
            left = tr.fraction;

            VectorSet(v, d2, 16, 0);
            G_ProjectSource(self->s.origin, v, v_forward, v_right, right_target);
            tr = gi.trace(self->s.origin, self->mins, self->maxs, right_target, self, MASK_PLAYERSOLID);
            right = tr.fraction;

            center = (d1 * center) / d2;
            if (left >= center && left > right) {
                if (left < 1) {
                    VectorSet(v, d2 * left * 0.5f, -16, 0);
                    G_ProjectSource(self->s.origin, v, v_forward, v_right, left_target);
                }
                VectorCopy(self->monsterinfo.last_sighting, self->monsterinfo.saved_goal);
                self->monsterinfo.aiflags |= AI_PURSUE_TEMP;
                VectorCopy(left_target, self->goalentity->s.origin);
                VectorCopy(left_target, self->monsterinfo.last_sighting);
                VectorSubtract(self->goalentity->s.origin, self->s.origin, v);
                self->s.angles[YAW] = self->ideal_yaw = vectoyaw(v);
            } else if (right >= center && right > left) {
                if (right < 1) {
                    VectorSet(v, d2 * right * 0.5f, 16, 0);
                    G_ProjectSource(self->s.origin, v, v_forward, v_right, right_target);
                }
                VectorCopy(self->monsterinfo.last_sighting, self->monsterinfo.saved_goal);
                self->monsterinfo.aiflags |= AI_PURSUE_TEMP;
                VectorCopy(right_target, self->goalentity->s.origin);
                VectorCopy(right_target, self->monsterinfo.last_sighting);
                VectorSubtract(self->goalentity->s.origin, self->s.origin, v);
                self->s.angles[YAW] = self->ideal_yaw = vectoyaw(v);
            }
        }
    }

    M_MoveToGoal(self, dist);
    if (!self->inuse)
        return;         // PGM - g_touchtrigger free problem

    G_FreeEdict(tempgoal);

    if (self->inuse)
        self->goalentity = save;
}
