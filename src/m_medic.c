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
/*
==============================================================================

MEDIC

==============================================================================
*/

#include "g_local.h"
#include "m_medic.h"

#define MEDIC_MIN_DISTANCE  32
#define MEDIC_MAX_HEAL_DISTANCE 400
#define MEDIC_TRY_TIME      10.0f

// FIXME -
//
// owner moved to monsterinfo.healer instead
//
// For some reason, the healed monsters are rarely ending up in the floor
//
// 5/15/1998 I think I fixed these, keep an eye on them

void M_SetEffects(edict_t *ent);
void HuntTarget(edict_t *self);
char *ED_NewString(const char *string);
void spawngrow_think(edict_t *self);
void M_FliesOff(edict_t *self);
void M_FliesOn(edict_t *self);

static int  sound_idle1;
static int  sound_pain1;
static int  sound_pain2;
static int  sound_die;
static int  sound_sight;
static int  sound_search;
static int  sound_hook_launch;
static int  sound_hook_hit;
static int  sound_hook_heal;
static int  sound_hook_retract;

// PMM - commander sounds
static int  commander_sound_idle1;
static int  commander_sound_pain1;
static int  commander_sound_pain2;
static int  commander_sound_die;
static int  commander_sound_sight;
static int  commander_sound_search;
static int  commander_sound_hook_launch;
static int  commander_sound_hook_hit;
static int  commander_sound_hook_heal;
static int  commander_sound_hook_retract;
static int  commander_sound_spawn;

static bool medic_IsCommander(const edict_t *self)
{
    return !strcmp(self->classname, "monster_medic_commander");
}

// content_flavour describes the server; Commander identity describes this Medic.
static bool medic_UsesRogueBehavior(const edict_t *self)
{
    return M_UsesRogueBehavior(self);
}

static const char *const reinforcements[] = {
    "monster_soldier_light",    // 0
    "monster_soldier",          // 1
    "monster_soldier_ss",       // 2
    "monster_infantry",         // 3
    "monster_gunner",           // 4
//  "monster_chick",            // 4
    "monster_medic",            // 5
    "monster_gladiator"         // 6
};

vec3_t reinforcement_mins[] = {
    {-16, -16, -24},
    {-16, -16, -24},
    {-16, -16, -24},
    {-16, -16, -24},
    {-16, -16, -24},
    {-16, -16, -24},
    {-32, -32, -24}
};

vec3_t reinforcement_maxs[] = {
    {16, 16, 32},
    {16, 16, 32},
    {16, 16, 32},
    {16, 16, 32},
    {16, 16, 32},
    {16, 16, 32},
    {32, 32, 64}
};

vec3_t reinforcement_position[] = {
    {80, 0, 0},
    {40, 60, 0},
    {40, -60, 0},
    {0, 80, 0},
    {0, -80, 0}
};

static void cleanupHeal(edict_t *self, bool change_frame)
{
    // clean up target, if we have one and it's legit
    if (self->enemy && self->enemy->inuse) {
        // The claim is released here whichever field holds it -- see
        // medic_ClaimPatient.  This is reached for a baseq2 medic too, because
        // its frame table drives Ground Zero's `medic_cable_attack` and that
        // aborts on an obstructed spawn point.
        self->enemy->monsterinfo.healer = NULL;
        if (self->enemy->owner == self)
            self->enemy->owner = NULL;
        self->enemy->monsterinfo.aiflags &= ~AI_RESURRECTING;
        self->enemy->takedamage = DAMAGE_YES;
        M_SetEffects(self->enemy);
    }

    if (change_frame)
        self->monsterinfo.nextframe = FRAME_attack52;
}

static void abortHeal(edict_t *self, bool change_frame, bool gib, bool mark)
{
    int hurt;
    static vec3_t   pain_normal = { 0, 0, 1 };

    // clean up target
    cleanupHeal(self, change_frame);
    // gib em!
    if ((mark) && (self->enemy) && (self->enemy->inuse)) {
//      if ((g_showlogic) && (g_showlogic->value))
//          gi.dprintf ("%s - marking target as bad\n", self->classname);
        // if the first badMedic slot is filled by a medic, skip it and use the second one
        if ((self->enemy->monsterinfo.badMedic1) && (self->enemy->monsterinfo.badMedic1->inuse)
            && (!strncmp(self->enemy->monsterinfo.badMedic1->classname, "monster_medic", 13))) {
            self->enemy->monsterinfo.badMedic2 = self;
        } else {
            self->enemy->monsterinfo.badMedic1 = self;
        }
    }
    if ((gib) && (self->enemy) && (self->enemy->inuse)) {
//      if ((g_showlogic) && (g_showlogic->value))
//          gi.dprintf ("%s - gibbing bad heal target", self->classname);

        if (self->enemy->gib_health)
            hurt = - self->enemy->gib_health;
        else
            hurt = 500;

        T_Damage(self->enemy, self, self, vec3_origin, self->enemy->s.origin,
                 pain_normal, hurt, 0, 0, MOD_UNKNOWN);
    }
    // clean up self

    self->monsterinfo.aiflags &= ~AI_MEDIC;
    if ((self->oldenemy) && (self->oldenemy->inuse))
        self->enemy = self->oldenemy;
    else
        self->enemy = NULL;

    self->monsterinfo.medicTries = 0;
}

bool canReach(edict_t *self, edict_t *other)
{
    vec3_t  spot1;
    vec3_t  spot2;
    trace_t trace;

    VectorCopy(self->s.origin, spot1);
    spot1[2] += self->viewheight;
    VectorCopy(other->s.origin, spot2);
    spot2[2] += other->viewheight;
    trace = gi.trace(spot1, vec3_origin, vec3_origin, spot2, self, MASK_SHOT | MASK_WATER);

    if (trace.fraction == 1.0f || trace.ent == other)        // PGM
        return true;
    return false;
}

static edict_t *medic_FindDeadMonster(edict_t *self)
{
    float   radius;
    edict_t *ent = NULL;
    edict_t *best = NULL;

    if (self->monsterinfo.aiflags & AI_STAND_GROUND)
        radius = MEDIC_MAX_HEAL_DISTANCE;
    else
        radius = 1024;

    while ((ent = findradius(ent, self->s.origin, radius)) != NULL) {
        if (ent == self)
            continue;
        if (!(ent->svflags & SVF_MONSTER))
            continue;
        if (ent->monsterinfo.aiflags & AI_GOOD_GUY)
            continue;
        // check to make sure we haven't bailed on this guy already
        if ((ent->monsterinfo.badMedic1 == self) || (ent->monsterinfo.badMedic2 == self))
            continue;
        if (ent->monsterinfo.healer)
            // FIXME - this is correcting a bug that is somewhere else
            // if the healer is a monster, and it's in medic mode .. continue .. otherwise
            //   we will override the healer, if it passes all the other tests
            if ((ent->monsterinfo.healer->inuse) && (ent->monsterinfo.healer->health > 0) &&
                (ent->monsterinfo.healer->svflags & SVF_MONSTER) && (ent->monsterinfo.healer->monsterinfo.aiflags & AI_MEDIC))
                continue;
        if (ent->health > 0)
            continue;
        if ((ent->nextthink) && !((ent->think == M_FliesOn) || (ent->think == M_FliesOff)))
            continue;
        if (!visible(self, ent))
//      if (!canReach(self, ent))
            continue;
        if (!strncmp(ent->classname, "player", 6))       // stop it from trying to heal player_noise entities
            continue;
        // FIXME - there's got to be a better way ..
        // make sure we don't spawn people right on top of us
        if (realrange(self, ent) <= MEDIC_MIN_DISTANCE)
            continue;
        if (!best) {
            best = ent;
            continue;
        }
        if (ent->max_health <= best->max_health)
            continue;
        best = ent;
    }

    if (best)
        self->timestamp = level.framenum + MEDIC_TRY_TIME * BASE_FRAMERATE;

    return best;
}

// The claim field is the thing the two medics disagree about everywhere,
// and it is why splitting the search alone would mean nothing.
//
// baseq2 marks a corpse it intends to heal by setting `ent->owner`, and skips
// a corpse that already has one.  Ground Zero moved that to
// `monsterinfo.healer` -- its own header comment at the top of this file says
// so, "owner moved to monsterinfo.healer instead" -- so that `owner` could go
// on meaning what it means everywhere else in the game.
//
// Before this split the tree ran Ground Zero's search under both flavours,
// which was internally CONSISTENT -- the callers wrote `healer` and the search
// read it -- and simply not id's policy.  So the defect was never a broken
// claim; it was that a baseq2 medic searched 400 units instead of 1024 while
// standing its ground, refused a corpse inside 32 units, accepted one whose
// `nextthink` is the fly timer, and armed a heal deadline its own donor never
// had.  Restoring id's predicate is what drags the claim field along: that
// predicate reads `owner`, so the search and the claim have to move together
// or the search tests a field nobody writes.
//
// What is not split, and is worth naming rather than leaving to be discovered:
// the heal itself.  `medic_cable_attack`, `medic_hook_launch` and
// `medic_hook_retract` are Ground Zero's for every flavour, and baseq2's
// `bq2_medic_move_attackCable` drives them on id's frame distances.  Splitting
// those too would be a second medic rather than a latch.
static edict_t *bq2_medic_FindDeadMonster(edict_t *self)
{
    edict_t *ent = NULL;
    edict_t *best = NULL;

    while ((ent = findradius(ent, self->s.origin, 1024)) != NULL) {
        if (ent == self)
            continue;
        if (!(ent->svflags & SVF_MONSTER))
            continue;
        if (ent->monsterinfo.aiflags & AI_GOOD_GUY)
            continue;
        if (ent->owner)
            continue;
        if (ent->health > 0)
            continue;
        if (ent->nextthink)
            continue;
        if (!visible(self, ent))
            continue;
        if (!best) {
            best = ent;
            continue;
        }
        if (ent->max_health <= best->max_health)
            continue;
        best = ent;
    }

    return best;
}

static edict_t *medic_FindPatient(edict_t *self)
{
    if (medic_UsesRogueBehavior(self))
        return medic_FindDeadMonster(self);

    return bq2_medic_FindDeadMonster(self);
}

static void medic_ClaimPatient(edict_t *self, edict_t *ent)
{
    if (medic_UsesRogueBehavior(self))
        ent->monsterinfo.healer = self;
    else
        ent->owner = self;
}

void medic_idle(edict_t *self)
{
    edict_t *ent;

    // PMM - commander sounds
    if (self->mass == 400)
        gi.sound(self, CHAN_VOICE, sound_idle1, 1, ATTN_IDLE, 0);
    else
        gi.sound(self, CHAN_VOICE, commander_sound_idle1, 1, ATTN_IDLE, 0);

    // Ground Zero will not go looking while it still owes an old enemy a fight;
    // baseq2's idle has neither that guard nor the `oldenemy` save, and takes a
    // patient whenever it finds one.  medic_search below is the donors' own
    // point of agreement and keeps the guard for both.
    if (!medic_UsesRogueBehavior(self)) {
        ent = medic_FindPatient(self);
        if (ent) {
            self->enemy = ent;
            medic_ClaimPatient(self, self->enemy);
            self->monsterinfo.aiflags |= AI_MEDIC;
            FoundTarget(self);
        }
        return;
    }

    if (!self->oldenemy) {
        ent = medic_FindPatient(self);
        if (ent) {
            self->oldenemy = self->enemy;
            self->enemy = ent;
            medic_ClaimPatient(self, self->enemy);
            self->monsterinfo.aiflags |= AI_MEDIC;
            FoundTarget(self);
        }
    }
}

void medic_search(edict_t *self)
{
    edict_t *ent;

    // PMM - commander sounds
    if (self->mass == 400)
        gi.sound(self, CHAN_VOICE, sound_search, 1, ATTN_IDLE, 0);
    else
        gi.sound(self, CHAN_VOICE, commander_sound_search, 1, ATTN_IDLE, 0);

    if (!self->oldenemy) {
        ent = medic_FindPatient(self);
        if (ent) {
            self->oldenemy = self->enemy;
            self->enemy = ent;
            medic_ClaimPatient(self, self->enemy);
            self->monsterinfo.aiflags |= AI_MEDIC;
            FoundTarget(self);
        }
    }
}

void medic_sight(edict_t *self, edict_t *other)
{
    // PMM - commander sounds
    if (self->mass == 400)
        gi.sound(self, CHAN_VOICE, sound_sight, 1, ATTN_NORM, 0);
    else
        gi.sound(self, CHAN_VOICE, commander_sound_sight, 1, ATTN_NORM, 0);
}

static const mframe_t medic_frames_stand[] = {
    { ai_stand, 0, medic_idle },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },

};
const mmove_t medic_move_stand = {FRAME_wait1, FRAME_wait90, medic_frames_stand, NULL};

void medic_stand(edict_t *self)
{
    self->monsterinfo.currentmove = &medic_move_stand;
}

static const mframe_t medic_frames_walk[] = {
    { ai_walk, 6.2,   NULL },
    { ai_walk, 18.1,  NULL },
    { ai_walk, 1,     NULL },
    { ai_walk, 9,     NULL },
    { ai_walk, 10,    NULL },
    { ai_walk, 9,     NULL },
    { ai_walk, 11,    NULL },
    { ai_walk, 11.6,  NULL },
    { ai_walk, 2,     NULL },
    { ai_walk, 9.9,   NULL },
    { ai_walk, 14,    NULL },
    { ai_walk, 9.3,   NULL }
};
const mmove_t medic_move_walk = {FRAME_walk1, FRAME_walk12, medic_frames_walk, NULL};

void medic_walk(edict_t *self)
{
    self->monsterinfo.currentmove = &medic_move_walk;
}

static const mframe_t medic_frames_run[] = {
    { ai_run, 18,     NULL },
    { ai_run, 22.5,   NULL },
    { ai_run, 25.4,   monster_done_dodge },
    { ai_run, 23.4,   NULL },
    { ai_run, 24,     NULL },
    { ai_run, 35.6,   NULL }    //pmm

};
const mmove_t medic_move_run = {FRAME_run1, FRAME_run6, medic_frames_run, NULL};

void medic_run(edict_t *self)
{
    if (medic_UsesRogueBehavior(self))
        monster_done_dodge(self);

    if (!(self->monsterinfo.aiflags & AI_MEDIC)) {
        edict_t *ent;

        ent = medic_FindPatient(self);
        if (ent) {
            self->oldenemy = self->enemy;
            self->enemy = ent;
            medic_ClaimPatient(self, self->enemy);
            self->monsterinfo.aiflags |= AI_MEDIC;
            FoundTarget(self);
            return;
        }
    }
//  else if (!canReach(self, self->enemy))
//  {
//      abortHeal (self, 0);
//  }

    if (self->monsterinfo.aiflags & AI_STAND_GROUND)
        self->monsterinfo.currentmove = &medic_move_stand;
    else
        self->monsterinfo.currentmove = &medic_move_run;
}

static const mframe_t medic_frames_pain1[] = {
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL }
};
const mmove_t medic_move_pain1 = {FRAME_paina1, FRAME_paina8, medic_frames_pain1, medic_run};

static const mframe_t medic_frames_pain2[] = {
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL }
};
const mmove_t medic_move_pain2 = {FRAME_painb1, FRAME_painb15, medic_frames_pain2, medic_run};

void medic_pain(edict_t *self, edict_t *other, float kick, int damage)
{
    monster_done_dodge(self);

    if ((self->health < (self->max_health / 2))) {
        if (self->mass > 400)
            self->s.skinnum = 3;
        else
            self->s.skinnum = 1;
    }

    if (level.framenum < self->pain_debounce_framenum)
        return;

    self->pain_debounce_framenum = level.framenum + 3 * BASE_FRAMERATE;

    if (skill->value == 3)
        return;     // no pain anims in nightmare

    // if we're healing someone, we ignore pain
    if (self->monsterinfo.aiflags & AI_MEDIC)
        return;

    if (self->mass > 400) {
        if (damage < 35) {
            gi.sound(self, CHAN_VOICE, commander_sound_pain1, 1, ATTN_NORM, 0);
            return;
        }

        self->monsterinfo.aiflags &= ~AI_MANUAL_STEERING;
        self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;

        gi.sound(self, CHAN_VOICE, commander_sound_pain2, 1, ATTN_NORM, 0);

        if (random() < (min(((float)damage * 0.005f), 0.5f)))     // no more than 50% chance of big pain
            self->monsterinfo.currentmove = &medic_move_pain2;
        else
            self->monsterinfo.currentmove = &medic_move_pain1;
    } else if (random() < 0.5f) {
        self->monsterinfo.currentmove = &medic_move_pain1;
        gi.sound(self, CHAN_VOICE, sound_pain1, 1, ATTN_NORM, 0);
    } else {
        self->monsterinfo.currentmove = &medic_move_pain2;
        gi.sound(self, CHAN_VOICE, sound_pain2, 1, ATTN_NORM, 0);
    }
    // PMM - clear duck flag
    if (self->monsterinfo.aiflags & AI_DUCKED)
        monster_duck_up(self);
}

static void medic_fire_blaster(edict_t *self)
{
    vec3_t  start;
    vec3_t  forward, right;
    vec3_t  end;
    vec3_t  dir;
    int     effect;
    int     damage = 2;

    // paranoia checking
    if (!(self->enemy && self->enemy->inuse))
        return;

    if ((self->s.frame == FRAME_attack9) || (self->s.frame == FRAME_attack12))
        effect = EF_BLASTER;
    else if ((self->s.frame == FRAME_attack19) || (self->s.frame == FRAME_attack22) || (self->s.frame == FRAME_attack25) || (self->s.frame == FRAME_attack28))
        effect = EF_HYPERBLASTER;
    else
        effect = 0;

    AngleVectors(self->s.angles, forward, right, NULL);
    G_ProjectSource(self->s.origin, monster_flash_offset[MZ2_MEDIC_BLASTER_1], forward, right, start);

    VectorCopy(self->enemy->s.origin, end);
    end[2] += self->enemy->viewheight;
    VectorSubtract(end, start, dir);

    if (!strcmp(self->enemy->classname, "tesla"))
        damage = 3;

    // medic commander shoots blaster2
    if (self->mass > 400)
        monster_fire_blaster2(self, start, dir, damage, 1000, MZ2_MEDIC_BLASTER_2, effect);
    else
        monster_fire_blaster(self, start, dir, damage, 1000, MZ2_MEDIC_BLASTER_1, effect);
}

static void medic_dead(edict_t *self)
{
    VectorSet(self->mins, -16, -16, -24);
    VectorSet(self->maxs, 16, 16, -8);
    self->movetype = MOVETYPE_TOSS;
    self->svflags |= SVF_DEADMONSTER;
    self->nextthink = 0;
    gi.linkentity(self);
}

static const mframe_t medic_frames_death[] = {
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL }
};
const mmove_t medic_move_death = {FRAME_death1, FRAME_death30, medic_frames_death, medic_dead};

void medic_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    int     n;

    // Ground Zero's comment, and it is true of ITS claim only: the shared
    // Killed() clears `monsterinfo.healer`.  Nothing clears `owner`, so
    // baseq2's own release is restored here -- without it a base medic that
    // dies mid-heal leaves the corpse marked for good and no other medic will
    // ever take it (id's line, m_medic.c, immediately above the gib check).
    if (!medic_UsesRogueBehavior(self) &&
        (self->enemy) && (self->enemy->owner == self))
        self->enemy->owner = NULL;

// check for gib
    if (self->health <= self->gib_health) {
        gi.sound(self, CHAN_VOICE, gi.soundindex("misc/udeath.wav"), 1, ATTN_NORM, 0);
        for (n = 0; n < 2; n++)
            ThrowGib(self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
        for (n = 0; n < 4; n++)
            ThrowGib(self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
        ThrowHead(self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);
        self->deadflag = DEAD_DEAD;
        return;
    }

    if (self->deadflag == DEAD_DEAD)
        return;

// regular death
    //  PMM
    if (self->mass == 400)
        gi.sound(self, CHAN_VOICE, sound_die, 1, ATTN_NORM, 0);
    else
        gi.sound(self, CHAN_VOICE, commander_sound_die, 1, ATTN_NORM, 0);
    //
    self->deadflag = DEAD_DEAD;
    self->takedamage = DAMAGE_YES;

    self->monsterinfo.currentmove = &medic_move_death;
}


static const mframe_t medic_frames_duck[] = {
    { ai_move, -1,    NULL },
    { ai_move, -1,    NULL },
    { ai_move, -1,    monster_duck_down },
    { ai_move, -1,    monster_duck_hold },
    { ai_move, -1,    NULL },
    { ai_move, -1,    NULL },
    { ai_move, -1,    NULL },   // PMM - duck up used to be here
    { ai_move, -1,    NULL },
    { ai_move, -1,    NULL },
    { ai_move, -1,    NULL },
    { ai_move, -1,    NULL },
    { ai_move, -1,    NULL },
    { ai_move, -1,    NULL },
    { ai_move, -1,    monster_duck_up },
    { ai_move, -1,    NULL },
    { ai_move, -1,    NULL }
};
const mmove_t medic_move_duck = {FRAME_duck1, FRAME_duck16, medic_frames_duck, medic_run};

// PMM -- moved dodge code to after attack code so I can reference attack frames

static const mframe_t medic_frames_attackHyperBlaster[] = {
    { ai_charge, 0,   NULL },
    { ai_charge, 0,   NULL },
    { ai_charge, 0,   NULL },
    { ai_charge, 0,   NULL },
    { ai_charge, 0,   medic_fire_blaster },
    { ai_charge, 0,   medic_fire_blaster },
    { ai_charge, 0,   medic_fire_blaster },
    { ai_charge, 0,   medic_fire_blaster },
    { ai_charge, 0,   medic_fire_blaster },
    { ai_charge, 0,   medic_fire_blaster },
    { ai_charge, 0,   medic_fire_blaster },
    { ai_charge, 0,   medic_fire_blaster },
    { ai_charge, 0,   medic_fire_blaster },
    { ai_charge, 0,   medic_fire_blaster },
    { ai_charge, 0,   medic_fire_blaster },
    { ai_charge, 0,   medic_fire_blaster }
};
const mmove_t medic_move_attackHyperBlaster = {FRAME_attack15, FRAME_attack30, medic_frames_attackHyperBlaster, medic_run};

static void medic_continue(edict_t *self)
{
    if (visible(self, self->enemy))
        if (random() <= 0.95f)
            self->monsterinfo.currentmove = &medic_move_attackHyperBlaster;
}

static const mframe_t medic_frames_attackBlaster[] = {
    { ai_charge, 0,   NULL },
    { ai_charge, 5,   NULL },
    { ai_charge, 5,   NULL },
    { ai_charge, 3,   NULL },
    { ai_charge, 2,   NULL },
    { ai_charge, 0,   NULL },
    { ai_charge, 0,   NULL },
    { ai_charge, 0,   NULL },
    { ai_charge, 0,   medic_fire_blaster },
    { ai_charge, 0,   NULL },
    { ai_charge, 0,   NULL },
    { ai_charge, 0,   medic_fire_blaster },
    { ai_charge, 0,   NULL },
    { ai_charge, 0,   medic_continue }  // Change to medic_continue... Else, go to frame 32
};
const mmove_t medic_move_attackBlaster = {FRAME_attack1, FRAME_attack14, medic_frames_attackBlaster, medic_run};

static void medic_hook_launch(edict_t *self)
{
    // PMM - commander sounds
    if (self->mass == 400)
        gi.sound(self, CHAN_WEAPON, sound_hook_launch, 1, ATTN_NORM, 0);
    else
        gi.sound(self, CHAN_WEAPON, commander_sound_hook_launch, 1, ATTN_NORM, 0);
}

static const vec3_t medic_cable_offsets[] = {
    { 45.0,  -9.2, 15.5 },
    { 48.4,  -9.7, 15.2 },
    { 47.8,  -9.8, 15.8 },
    { 47.3,  -9.3, 14.3 },
    { 45.4, -10.1, 13.1 },
    { 41.9, -12.7, 12.0 },
    { 37.8, -15.8, 11.2 },
    { 34.3, -18.4, 10.7 },
    { 32.7, -19.7, 10.4 },
    { 32.7, -19.7, 10.4 }
};

static void medic_cable_attack(edict_t *self)
{
    vec3_t  offset, start, end, f, r;
    trace_t tr;
    vec3_t  dir;
//  vec3_t  angles;
    float   distance;

    if ((!self->enemy) || (!self->enemy->inuse) || (self->enemy->s.effects & EF_GIB)) {
//      if ((g_showlogic) && (g_showlogic->value))
//          gi.dprintf ("medic_commander - aborting heal due to target's disappearance\n");
        abortHeal(self, true, false, false);
        return;
    }

    // see if our enemy has changed to a client, or our target has more than 0 health,
    // abort it .. we got switched to someone else due to damage
    if ((self->enemy->client) || (self->enemy->health > 0)) {
        abortHeal(self, true, false, false);
        return;
    }
    AngleVectors(self->s.angles, f, r, NULL);
    VectorCopy(medic_cable_offsets[self->s.frame - FRAME_attack42], offset);
    G_ProjectSource(self->s.origin, offset, f, r, start);

    // check for max distance
    // not needed, done in checkattack
    // check for min distance
    VectorSubtract(start, self->enemy->s.origin, dir);
    distance = VectorLength(dir);
    if (distance < MEDIC_MIN_DISTANCE) {
//      if ((g_showlogic) && (g_showlogic->value))
//          gi.dprintf ("medic_commander - aborting heal due to proximity to target ");
        abortHeal(self, true, true, false);
        return;
    }
//  if ((g_showlogic)&&(g_showlogic->value))
//      gi.dprintf ("distance to target is %f\n", distance);
//  if (distance > 300)
//      return;

    // check for min/max pitch
    // PMM -- took out since it doesn't look bad when it fails
//  vectoangles (dir, angles);
//  if (angles[0] < -180)
//      angles[0] += 360;
//  if (fabsf(angles[0]) > 45)
//  {
//      if ((g_showlogic) && (g_showlogic->value))
//          gi.dprintf ("medic_commander - aborting heal due to bad angle\n");
//      abortHeal(self);
//      return;
//  }

    tr = gi.trace(start, NULL, NULL, self->enemy->s.origin, self, MASK_SOLID);
    if (tr.fraction != 1.0f && tr.ent != self->enemy) {
        if (tr.ent == world) {
            // give up on second try
            if (self->monsterinfo.medicTries > 1) {
                abortHeal(self, true, false, true);
                return;
            }
            self->monsterinfo.medicTries++;
            cleanupHeal(self, 1);
            return;
        }
//      if ((g_showlogic) && (g_showlogic->value))
//          gi.dprintf ("medic_commander - aborting heal due to beamus interruptus\n");
        abortHeal(self, true, false, false);
        return;
    }

    if (self->s.frame == FRAME_attack43) {
        // PMM - commander sounds
        if (self->mass == 400)
            gi.sound(self->enemy, CHAN_AUTO, sound_hook_hit, 1, ATTN_NORM, 0);
        else
            gi.sound(self->enemy, CHAN_AUTO, commander_sound_hook_hit, 1, ATTN_NORM, 0);

        self->enemy->monsterinfo.aiflags |= AI_RESURRECTING;
        self->enemy->takedamage = DAMAGE_NO;
        M_SetEffects(self->enemy);
    } else if (self->s.frame == FRAME_attack50) {
        vec3_t  maxs;
        self->enemy->spawnflags = 0;
        self->enemy->monsterinfo.aiflags = 0;
        self->enemy->target = NULL;
        self->enemy->targetname = NULL;
        self->enemy->combattarget = NULL;
        self->enemy->deathtarget = NULL;
        self->enemy->monsterinfo.healer = self;

        VectorCopy(self->enemy->maxs, maxs);
        maxs[2] += 48;   // compensate for change when they die

        tr = gi.trace(self->enemy->s.origin, self->enemy->mins, maxs, self->enemy->s.origin, self->enemy, MASK_MONSTERSOLID);
        if (tr.startsolid || tr.allsolid) {
//          if ((g_showlogic) && (g_showlogic->value))
//              gi.dprintf ("Spawn point obstructed, aborting heal!\n");
            abortHeal(self, true, true, false);
            return;
        } else if (tr.ent != world) {
//          if ((g_showlogic) && (g_showlogic->value))
//              gi.dprintf("heal in entity %s\n", tr.ent->classname);
            abortHeal(self, true, true, false);
            return;
        }
        /*      else if (tr.ent == world)
                {
                    if ((g_showlogic) && (g_showlogic->value))
                        gi.dprintf ("heal in world, aborting!\n");
                    abortHeal (self, 1);
                    return;
                }
        */      else {
            self->enemy->monsterinfo.aiflags |= AI_DO_NOT_COUNT;
            ED_CallSpawn(self->enemy);

            if (self->enemy->think) {
                self->enemy->nextthink = level.framenum;
                self->enemy->think(self->enemy);
            }
            //      self->enemy->monsterinfo.aiflags |= AI_RESURRECTING;
            self->enemy->monsterinfo.aiflags &= ~AI_RESURRECTING;
            self->enemy->monsterinfo.aiflags |= AI_IGNORE_SHOTS | AI_DO_NOT_COUNT;
            // turn off flies
            self->enemy->s.effects &= ~EF_FLIES;
            self->enemy->monsterinfo.healer = NULL;
            // and id's own release, which it does the line after ED_CallSpawn:
            // without it a monster a baseq2 medic revived carries `owner` for
            // the rest of the level, and bq2_medic_FindDeadMonster would refuse
            // to heal it a second time.
            if (self->enemy->owner == self)
                self->enemy->owner = NULL;

            if ((self->oldenemy) && (self->oldenemy->inuse) && (self->oldenemy->health > 0)) {
//              if ((g_showlogic) && (g_showlogic->value))
//                  gi.dprintf ("setting heal target's enemy to %s\n", self->oldenemy->classname);
                self->enemy->enemy = self->oldenemy;
//              HuntTarget (self->enemy);
                FoundTarget(self->enemy);
            } else {
//              if (g_showlogic && g_showlogic->value)
//                  gi.dprintf ("no valid enemy to set!\n");
                self->enemy->enemy = NULL;
                if (!FindTarget(self->enemy)) {
                    // no valid enemy, so stop acting
                    self->enemy->monsterinfo.pause_framenum = level.framenum + 100000000 * BASE_FRAMERATE;
                    self->enemy->monsterinfo.stand(self->enemy);
                }
                self->enemy = NULL;
                self->oldenemy = NULL;
                if (!FindTarget(self)) {
                    // no valid enemy, so stop acting
                    self->monsterinfo.pause_framenum = level.framenum + 100000000 * BASE_FRAMERATE;
                    self->monsterinfo.stand(self);
                    return;
                }
            }
        }
    } else {
        if (self->s.frame == FRAME_attack44) {
            // PMM - medic commander sounds
            if (self->mass == 400)
                gi.sound(self, CHAN_WEAPON, sound_hook_heal, 1, ATTN_NORM, 0);
            else
                gi.sound(self, CHAN_WEAPON, commander_sound_hook_heal, 1, ATTN_NORM, 0);
        }
    }

    // adjust start for beam origin being in middle of a segment
    VectorMA(start, 8, f, start);

    // adjust end z for end spot since the monster is currently dead
    VectorCopy(self->enemy->s.origin, end);
    end[2] = self->enemy->absmin[2] + self->enemy->size[2] / 2;

    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_MEDIC_CABLE_ATTACK);
    gi.WriteShort(self - g_edicts);
    gi.WritePosition(start);
    gi.WritePosition(end);
    gi.multicast(self->s.origin, MULTICAST_PVS);
}

static void medic_hook_retract(edict_t *self)
{
    if (self->mass == 400)
        gi.sound(self, CHAN_WEAPON, sound_hook_retract, 1, ATTN_NORM, 0);
    else
        gi.sound(self, CHAN_WEAPON, sound_hook_retract, 1, ATTN_NORM, 0);

    self->monsterinfo.aiflags &= ~AI_MEDIC;
    if ((self->oldenemy) && (self->oldenemy->inuse))
        self->enemy = self->oldenemy;
    else {
        self->enemy = NULL;
        self->oldenemy = NULL;
        if (!FindTarget(self)) {
            // no valid enemy, so stop acting
            self->monsterinfo.pause_framenum = level.framenum + 100000000 * BASE_FRAMERATE;
            self->monsterinfo.stand(self);
            return;
        }
    }
}

static const mframe_t medic_frames_attackCable[] = {
// ROGUE - negated 36-40 so he scoots back from his target a little
// ROGUE - switched 33-36 to ai_charge
// ROGUE - changed frame 52 to 0 to compensate for changes in 36-40
    { ai_charge, 2,       NULL },               //33
    { ai_charge, 3,       NULL },
    { ai_charge, 5,       NULL },
    { ai_charge, -4.4f,    NULL },               //36
    { ai_charge, -4.7f,    NULL },               //37
    { ai_charge, -5,  NULL },
    { ai_charge, -6,  NULL },
    { ai_charge, -4,  NULL },               //40
    { ai_charge, 0,   NULL },
    { ai_move, 0,     medic_hook_launch },  //42
    { ai_move, 0,     medic_cable_attack }, //43
    { ai_move, 0,     medic_cable_attack },
    { ai_move, 0,     medic_cable_attack },
    { ai_move, 0,     medic_cable_attack },
    { ai_move, 0,     medic_cable_attack },
    { ai_move, 0,     medic_cable_attack },
    { ai_move, 0,     medic_cable_attack },
    { ai_move, 0,     medic_cable_attack },
    { ai_move, 0,     medic_cable_attack }, //51
    { ai_move, 0,     medic_hook_retract }, //52
    { ai_move, -1.5f,  NULL },
    { ai_move, -1.2f,  NULL },
    { ai_move, -3,    NULL },
    { ai_move, -2,    NULL },
    { ai_move, 0.3f,   NULL },
    { ai_move, 0.7f,   NULL },
    { ai_move, 1.2f,   NULL },
    { ai_move, 1.3f,   NULL }                //60
};
const mmove_t medic_move_attackCable = {FRAME_attack33, FRAME_attack60, medic_frames_attackCable, medic_run};

// Baseq2's medic_move_attackCable, kept alongside Ground Zero's.
// Ground Zero rewrote the cable attack wholesale -- different frame count,
// different hook launch timing, and AI_MANUAL_STEERING handling.
static const mframe_t bq2_medic_frames_attackCable[] = {
    { ai_move, 2,     NULL },
    { ai_move, 3,     NULL },
    { ai_move, 5,     NULL },
    { ai_move, 4.4,   NULL },
    { ai_charge, 4.7, NULL },
    { ai_charge, 5,   NULL },
    { ai_charge, 6,   NULL },
    { ai_charge, 4,   NULL },
    { ai_charge, 0,   NULL },
    { ai_move, 0,     medic_hook_launch },
    { ai_move, 0,     medic_cable_attack },
    { ai_move, 0,     medic_cable_attack },
    { ai_move, 0,     medic_cable_attack },
    { ai_move, 0,     medic_cable_attack },
    { ai_move, 0,     medic_cable_attack },
    { ai_move, 0,     medic_cable_attack },
    { ai_move, 0,     medic_cable_attack },
    { ai_move, 0,     medic_cable_attack },
    { ai_move, 0,     medic_cable_attack },
    { ai_move, -15,   medic_hook_retract },
    { ai_move, -1.5,  NULL },
    { ai_move, -1.2,  NULL },
    { ai_move, -3,    NULL },
    { ai_move, -2,    NULL },
    { ai_move, 0.3,   NULL },
    { ai_move, 0.7,   NULL },
    { ai_move, 1.2,   NULL },
    { ai_move, 1.3,   NULL }
};

const mmove_t bq2_medic_move_attackCable = {FRAME_attack33, FRAME_attack60, bq2_medic_frames_attackCable, medic_run};

static void medic_start_spawn(edict_t *self)
{
    gi.sound(self, CHAN_WEAPON, commander_sound_spawn, 1, ATTN_NORM, 0);
    self->monsterinfo.nextframe = FRAME_attack48;
}

static void medic_determine_spawn(edict_t *self)
{
    vec3_t  f, r, offset, startpoint, spawnpoint;
    float   lucky;
    int     summonStr;
    int     count;
    int     inc;
    int     num_summoned; // should be 1, 3, or 5
    int     num_success = 0;

    lucky = random();
    summonStr = skill->value;

    // bell curve - 0 = 67%, 1 = 93%, 2 = 99% -- too steep
    // this ends up with
    // -3 = 5%
    // -2 = 10%
    // -1 = 15%
    // 0  = 40%
    // +1 = 15%
    // +2 = 10%
    // +3 = 5%
    if (lucky < 0.05f)
        summonStr -= 3;
    else if (lucky < 0.15f)
        summonStr -= 2;
    else if (lucky < 0.3f)
        summonStr -= 1;
    else if (lucky > 0.95f)
        summonStr += 3;
    else if (lucky > 0.85f)
        summonStr += 2;
    else if (lucky > 0.7f)
        summonStr += 1;

    if (summonStr < 0)
        summonStr = 0;

    //FIXME - need to remember this, might as well use this int that isn't used for monsters
    self->plat2flags = summonStr;
    AngleVectors(self->s.angles, f, r, NULL);

// this yields either 1, 3, or 5
    if (summonStr)
        num_summoned = (summonStr - 1) + (summonStr % 2);
    else
        num_summoned = 1;

//  if ((g_showlogic) && (g_showlogic->value))
//      gi.dprintf ("medic_commander: summonStr = %d num = %d\n", summonStr, num_summoned);

    for (count = 0; count < num_summoned; count++) {
        inc = count + (count % 2); // 0, 2, 2, 4, 4
        VectorCopy(reinforcement_position[count], offset);

        G_ProjectSource(self->s.origin, offset, f, r, startpoint);
        // a little off the ground
        startpoint[2] += 10;

        if (FindSpawnPoint(startpoint, reinforcement_mins[summonStr - inc], reinforcement_maxs[summonStr - inc], spawnpoint, 32)) {
            if (CheckGroundSpawnPoint(spawnpoint,
                                      reinforcement_mins[summonStr - inc], reinforcement_maxs[summonStr - inc],
                                      256, -1)) {
                num_success++;
                // we found a spot, we're done here
                count = num_summoned;
            }
//          else if ((g_showlogic) && (g_showlogic->value))
//              gi.dprintf ("medic_commander: CheckGroundSpawnPoint says bad stuff down there!\n");
        }
    }

    if (num_success == 0) {
        for (count = 0; count < num_summoned; count++) {
            inc = count + (count % 2); // 0, 2, 2, 4, 4
            VectorCopy(reinforcement_position[count], offset);

            // check behind
            offset[0] *= -1.0f;
            offset[1] *= -1.0f;
            G_ProjectSource(self->s.origin, offset, f, r, startpoint);
            // a little off the ground
            startpoint[2] += 10;

            if (FindSpawnPoint(startpoint, reinforcement_mins[summonStr - inc], reinforcement_maxs[summonStr - inc], spawnpoint, 32)) {
                if (CheckGroundSpawnPoint(spawnpoint,
                                          reinforcement_mins[summonStr - inc], reinforcement_maxs[summonStr - inc],
                                          256, -1)) {
                    num_success++;
                    // we found a spot, we're done here
                    count = num_summoned;
                }
            }
        }

        if (num_success) {
            self->monsterinfo.aiflags |= AI_MANUAL_STEERING;
            self->ideal_yaw = anglemod(self->s.angles[YAW]) + 180;
            if (self->ideal_yaw > 360.0f)
                self->ideal_yaw -= 360.0f;
//          self->plat2flags *= -1;
        }
    }

    if (num_success == 0) {
//      if ((g_showlogic) && (g_showlogic->value))
//          gi.dprintf ("medic_commander: failed to find any spawn points, aborting!\n");
        self->monsterinfo.nextframe = FRAME_attack53;
    }
}

static void medic_spawngrows(edict_t *self)
{
    vec3_t  f, r, offset, startpoint, spawnpoint;
    int     summonStr;
    int     count;
    int     inc;
    int     num_summoned; // should be 1, 3, or 5
    int     num_success = 0;
    float   current_yaw;

    // if we've been directed to turn around
    if (self->monsterinfo.aiflags & AI_MANUAL_STEERING) {
        current_yaw = anglemod(self->s.angles[YAW]);
        if (fabsf(current_yaw - self->ideal_yaw) > 0.1f) {
            self->monsterinfo.aiflags |= AI_HOLD_FRAME;
            return;
        }

        // done turning around
        self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
        self->monsterinfo.aiflags &= ~AI_MANUAL_STEERING;
    }

//  if (self->plat2flags < 0)
//  {
//      summonStr = -1.0 * self->plat2flags;
//      behind = true;
//  }
//  else
    summonStr = self->plat2flags;

    AngleVectors(self->s.angles, f, r, NULL);

//  num_summoned = ((((summonStr-1)/2)+1)*2)-1;  // this yields either 1, 3, or 5
    if (summonStr)
        num_summoned = (summonStr - 1) + (summonStr % 2);
    else
        num_summoned = 1;

    for (count = 0; count < num_summoned; count++) {
        inc = count + (count % 2); // 0, 2, 2, 4, 4
        VectorCopy(reinforcement_position[count], offset);

        G_ProjectSource(self->s.origin, offset, f, r, startpoint);
        // a little off the ground
        startpoint[2] += 10;

        if (FindSpawnPoint(startpoint, reinforcement_mins[summonStr - inc], reinforcement_maxs[summonStr - inc], spawnpoint, 32)) {
            if (CheckGroundSpawnPoint(spawnpoint,
                                      reinforcement_mins[summonStr - inc], reinforcement_maxs[summonStr - inc],
                                      256, -1)) {
                num_success++;
                if ((summonStr - inc) > 3)
                    SpawnGrow_Spawn(spawnpoint, 1);         // big monster
                else
                    SpawnGrow_Spawn(spawnpoint, 0);         // normal size
            }
        }
    }

    if (num_success == 0) {
//      if ((g_showlogic) && (g_showlogic->value))
//          gi.dprintf ("medic_commander: spawngrows bad, aborting!\n");
        self->monsterinfo.nextframe = FRAME_attack53;
    }
}

static void medic_finish_spawn(edict_t *self)
{
    edict_t *ent;
    vec3_t  f, r, offset, startpoint, spawnpoint;
    int     summonStr;
    int     count;
    int     inc;
    int     num_summoned; // should be 1, 3, or 5
    edict_t *designated_enemy;

//  trace_t     tr;
//  vec3_t mins, maxs;

    // this is one bigger than the soldier's real mins .. just for paranoia's sake
//  VectorSet (mins, -17, -17, -25);
//  VectorSet (maxs, 17, 17, 33);

    //FIXME - better place to store this info?
    if (self->plat2flags < 0) {
        self->plat2flags *= -1;
    }
    summonStr = self->plat2flags;

    AngleVectors(self->s.angles, f, r, NULL);

//  num_summoned = ((((summonStr-1)/2)+1)*2)-1;  // this yields either 1, 3, or 5
    if (summonStr)
        num_summoned = (summonStr - 1) + (summonStr % 2);
    else
        num_summoned = 1;

//  if ((g_showlogic) && (g_showlogic->value))
//      gi.dprintf ("medic_commander: summonStr = %d num = %d\n", summonStr, num_summoned);

    for (count = 0; count < num_summoned; count++) {
        inc = count + (count % 2); // 0, 2, 2, 4, 4
        VectorCopy(reinforcement_position[count], offset);

        G_ProjectSource(self->s.origin, offset, f, r, startpoint);

        // a little off the ground
        startpoint[2] += 10;

        ent = NULL;
        if (FindSpawnPoint(startpoint, reinforcement_mins[summonStr - inc], reinforcement_maxs[summonStr - inc], spawnpoint, 32)) {
            if (CheckSpawnPoint(spawnpoint, reinforcement_mins[summonStr - inc], reinforcement_maxs[summonStr - inc]))
                ent = CreateGroundMonster(spawnpoint, self->s.angles,
                                          reinforcement_mins[summonStr - inc], reinforcement_maxs[summonStr - inc],
                                          reinforcements[summonStr - inc], 256);
//          else if ((g_showlogic) && (g_showlogic->value))
//              gi.dprintf ("CheckSpawnPoint failed volume check!\n");
        } else {
//          if ((g_showlogic) && (g_showlogic->value))
//              gi.dprintf ("FindSpawnPoint failed to find a point!\n");
        }

        if (!ent) {
//          if ((g_showlogic) && (g_showlogic->value))
//              gi.dprintf ("Spawn point obstructed for %s, aborting!\n", reinforcements[summonStr-inc]);
            continue;
        }

//      gi.sound (self, CHAN_WEAPON, commander_sound_spawn, 1, ATTN_NORM, 0);

        if (ent->think) {
            ent->nextthink = level.framenum;
            ent->think(ent);
        }

        ent->monsterinfo.aiflags |= AI_IGNORE_SHOTS | AI_DO_NOT_COUNT | AI_SPAWNED_MEDIC_C;
        ent->monsterinfo.commander = self;
        self->monsterinfo.monster_slots--;
//      if ((g_showlogic) && (g_showlogic->value))
//          gi.dprintf ("medic_commander: %d slots remaining\n", self->monsterinfo.monster_slots);

        if (self->monsterinfo.aiflags & AI_MEDIC)
            designated_enemy = self->oldenemy;
        else
            designated_enemy = self->enemy;

        if (coop && coop->value) {
            designated_enemy = PickCoopTarget(ent);
            if (designated_enemy) {
                // try to avoid using my enemy
                if (designated_enemy == self->enemy) {
                    designated_enemy = PickCoopTarget(ent);
                    if (designated_enemy) {
//                      if ((g_showlogic) && (g_showlogic->value))
//                      {
//                          gi.dprintf ("PickCoopTarget returned a %s - ", designated_enemy->classname);
//                          if (designated_enemy->client)
//                              gi.dprintf ("with name %s\n", designated_enemy->client->pers.netname);
//                          else
//                              gi.dprintf ("NOT A CLIENT\n");
//                      }
                    } else {
//                      if ((g_showlogic) && (g_showlogic->value))
//                          gi.dprintf ("pick coop failed, using my current enemy\n");
                        designated_enemy = self->enemy;
                    }
                }
            } else {
//              if ((g_showlogic) && (g_showlogic->value))
//                  gi.dprintf ("pick coop failed, using my current enemy\n");
                designated_enemy = self->enemy;
            }
        }

        if ((designated_enemy) && (designated_enemy->inuse) && (designated_enemy->health > 0)) {
            // fixme
//          if ((g_showlogic) && (g_showlogic -> value))
//              gi.dprintf  ("setting enemy to %s\n", designated_enemy->classname);
            ent->enemy = designated_enemy;
            FoundTarget(ent);
        } else {
            ent->enemy = NULL;
            ent->monsterinfo.stand(ent);
        }
//      ent->s.event = EV_PLAYER_TELEPORT;
    }
}

static const mframe_t medic_frames_callReinforcements[] = {
    // ROGUE - 33-36 now ai_charge
    { ai_charge, 2,       NULL },               //33
    { ai_charge, 3,       NULL },
    { ai_charge, 5,       NULL },
    { ai_charge, 4.4, NULL },               //36
    { ai_charge, 4.7, NULL },
    { ai_charge, 5,   NULL },
    { ai_charge, 6,   NULL },
    { ai_charge, 4,   NULL },               //40
    { ai_charge, 0,   NULL },
    { ai_move, 0,     medic_start_spawn },  //42
    { ai_move, 0,     NULL },   //43 -- 43 through 47 are skipped
    { ai_move, 0,     NULL },
    { ai_move, 0,     NULL },
    { ai_move, 0,     NULL },
    { ai_move, 0,     NULL },
    { ai_move, 0,     medic_determine_spawn },  //48
    { ai_charge, 0,       medic_spawngrows },       //49
    { ai_move, 0,     NULL },   //50
    { ai_move, 0,     NULL },   //51
    { ai_move, -15,   medic_finish_spawn }, //52
    { ai_move, -1.5,  NULL },
    { ai_move, -1.2,  NULL },
    { ai_move, -3,    NULL },
    { ai_move, -2,    NULL },
    { ai_move, 0.3,   NULL },
    { ai_move, 0.7,   NULL },
    { ai_move, 1.2,   NULL },
    { ai_move, 1.3,   NULL }                //60
};
const mmove_t medic_move_callReinforcements = {FRAME_attack33, FRAME_attack60, medic_frames_callReinforcements, medic_run};

void medic_attack(edict_t *self)
{
    int     enemy_range;
    float   r;

    if (!medic_UsesRogueBehavior(self)) {
        if (self->monsterinfo.aiflags & AI_MEDIC)
            self->monsterinfo.currentmove = &bq2_medic_move_attackCable;
        else
            self->monsterinfo.currentmove = &medic_move_attackBlaster;
        return;
    }

    monster_done_dodge(self);

    enemy_range = range(self, self->enemy);

    // signal from checkattack to spawn
    if (self->monsterinfo.aiflags & AI_BLOCKED) {
        self->monsterinfo.currentmove = &medic_move_callReinforcements;
        self->monsterinfo.aiflags &= ~AI_BLOCKED;
    }

    r = random();
    if (self->monsterinfo.aiflags & AI_MEDIC) {
        if ((self->mass > 400) && (r > 0.8f) && (self->monsterinfo.monster_slots > 2))
            self->monsterinfo.currentmove = &medic_move_callReinforcements;
        else {
            if (medic_UsesRogueBehavior(self))
                self->monsterinfo.currentmove = &medic_move_attackCable;
            else
                self->monsterinfo.currentmove = &bq2_medic_move_attackCable;
        }
    } else {
        if (self->monsterinfo.attack_state == AS_BLIND) {
            self->monsterinfo.currentmove = &medic_move_callReinforcements;
            return;
        }
        if ((self->mass > 400) && (r > 0.2f) && (enemy_range != RANGE_MELEE) && (self->monsterinfo.monster_slots > 2))
            self->monsterinfo.currentmove = &medic_move_callReinforcements;
        else
            self->monsterinfo.currentmove = &medic_move_attackBlaster;
    }
}

bool medic_checkattack(edict_t *self)
{
    if (self->monsterinfo.aiflags & AI_MEDIC) {
        // if our target went away
        //
        // Kept for every flavour, because baseq2 has no guard here at all and
        // `medic_cable_attack` dereferences `self->enemy` on its first line --
        // id's arm reaches a null deref by the same road the supertank and
        // Chick guards are kept against.  It is belt-and-braces either way:
        // `ai_run` sets `hesDeadJim` on a gone or revived enemy before it calls
        // `checkattack`, so this is not reachable from that path.  `abortHeal`
        // is Ground Zero's and stays with it -- baseq2 has no `healer`,
        // `badMedic` or `medicTries` bookkeeping to unwind, so declining the
        // attack is the whole of what it has to do.
        if ((!self->enemy) || (!self->enemy->inuse)) {
//          if (g_showlogic && g_showlogic->value)
//              gi.dprintf ("aborting heal target due to gib\n");
            if (medic_UsesRogueBehavior(self))
                abortHeal(self, true, false, false);
            return false;
        }

        // The deadline and the walk-in are Ground Zero's, and they are a
        // different design from id's -- not an addition to it.
        //
        // baseq2 commits to the cable animation and lets `medic_cable_attack`
        // decline frame by frame on its own `distance > 256` test, so a medic
        // that cannot reach keeps swinging.  Ground Zero gives the attempt a
        // MEDIC_TRY_TIME deadline, gibs the target when it expires, and walks
        // closer with AS_STRAIGHT when the corpse is beyond
        // MEDIC_MAX_HEAL_DISTANCE.  Neither half exists in id's file:
        // `timestamp` is Ground Zero's field, and running its deadline under
        // `rogue 0` made every base medic answer to a clock its own donor
        // never set.
        if (!medic_UsesRogueBehavior(self)) {
            medic_attack(self);
            return true;
        }

        // if we ran out of time, give up
        if (self->timestamp < level.framenum) {
//          if (g_showlogic && g_showlogic->value)
//              gi.dprintf ("aborting heal target (%s) due to time\n", self->enemy->classname);
            abortHeal(self, true, false, true);
            self->timestamp = 0;
            return false;
        }

        if (realrange(self, self->enemy) < MEDIC_MAX_HEAL_DISTANCE + 10) {
            medic_attack(self);
            return true;
        } else {
            self->monsterinfo.attack_state = AS_STRAIGHT;
            return false;
        }
    }

    if (!medic_UsesRogueBehavior(self))
        return M_CheckAttack(self);

    if (self->enemy->client && !visible(self, self->enemy) && (self->monsterinfo.monster_slots > 2)) {
        self->monsterinfo.attack_state = AS_BLIND;
        return true;
    }

    // give a LARGE bias to spawning things when we have room
    // use AI_BLOCKED as a signal to attack to spawn
    if ((random() < 0.8f) && (self->monsterinfo.monster_slots > 5) && (realrange(self, self->enemy) > 150)) {
        self->monsterinfo.aiflags |= AI_BLOCKED;
        self->monsterinfo.attack_state = AS_MISSILE;
        return true;
    }

    // ROGUE
    // since his idle animation looks kinda bad in combat, if we're not in easy mode, always attack
    // when he's on a combat point
    if (medic_UsesRogueBehavior(self) && skill->value > 0)
        if (self->monsterinfo.aiflags & AI_STAND_GROUND) {
            self->monsterinfo.attack_state = AS_MISSILE;
            return true;
        }

    return M_CheckAttack(self);
}
/*
void medic_dodge (edict_t *self, edict_t *attacker, float eta, trace_t *tr)
{

    if (random() > 0.25)
        return;

    if (!self->enemy)
        self->enemy = attacker;

    self->monsterinfo.currentmove = &medic_move_duck;

//===========
//PMM - rogue rewrite of dodge code.
    float   r;
    float   height;
    int     shooting = 0;

    // this needs to be before the AI_MEDIC check, because
    //   a) if AI_MEDIC is set, we should have an enemy anyway and
    //   b) FoundTarget calls medic_run, which can set AI_MEDIC
    if (!self->enemy)
    {
        self->enemy = attacker;
        FoundTarget (self);
    }

//  don't dodge if you're healing
    if (self->monsterinfo.aiflags & AI_MEDIC)
        return;

    // PMM - don't bother if it's going to hit anyway; fix for weird in-your-face etas (I was
    // seeing numbers like 13 and 14)
    if ((eta < 0.1) || (eta > 5))
        return;

    r = random();
    if (r > (0.25*((skill->value)+1)))
        return;

    if ((self->monsterinfo.currentmove == &medic_move_attackHyperBlaster) ||
        (self->monsterinfo.currentmove == &medic_move_attackCable) ||
        (self->monsterinfo.currentmove == &medic_move_attackBlaster))
    {
        shooting = 1;
    }
    if (self->monsterinfo.aiflags & AI_DODGING)
    {
        height = self->absmax[2];
    }
    else
    {
        height = self->absmax[2]-32-1;  // the -1 is because the absmax is s.origin + maxs + 1
    }

    // check to see if it makes sense to duck
    if (tr->endpos[2] <= height)
    {
        vec3_t right, diff;
        if (shooting)
        {
            self->monsterinfo.attack_state = AS_SLIDING;
            return;
        }
        AngleVectors (self->s.angles, NULL, right, NULL);
        VectorSubtract (tr->endpos, self->s.origin, diff);
        if (DotProduct (right, diff) < 0)
        {
            self->monsterinfo.lefty = 1;
        }
        // if it doesn't sense to duck, try to strafe away
        monster_done_dodge (self);
        self->monsterinfo.currentmove = &medic_move_run;
        self->monsterinfo.attack_state = AS_SLIDING;
        return;
    }

    if (skill->value == 0)
    {
        self->monsterinfo.currentmove = &medic_move_duck;
        // PMM - stupid dodge
        self->monsterinfo.duck_wait_framenum = level.framenum + (eta + 1) * BASE_FRAMERATE;
        self->monsterinfo.aiflags |= AI_DODGING;
        return;
    }

    if (!shooting)
    {
        self->monsterinfo.currentmove = &medic_move_duck;
        self->monsterinfo.duck_wait_framenum = level.framenum + (eta + (0.1 * (3 - skill->value))) * BASE_FRAMERATE;
        self->monsterinfo.aiflags |= AI_DODGING;
    }
    return;

//PMM
//===========

}
*/
static void MedicCommanderCache(void)
{
    edict_t *newEnt;
    int i;

    //FIXME - better way to do this?  this is quick and dirty
    for (i = 0; i < 7; i++) {
        newEnt = G_Spawn();

        VectorCopy(vec3_origin, newEnt->s.origin);
        VectorCopy(vec3_origin, newEnt->s.angles);
        newEnt->classname = ED_NewString(reinforcements[i]);

        newEnt->monsterinfo.aiflags |= AI_DO_NOT_COUNT;

        ED_CallSpawn(newEnt);
        // FIXME - could copy mins/maxs into reinforcements from here
        G_FreeEdict(newEnt);
    }

    gi.modelindex("models/items/spawngro/tris.md2");
    gi.modelindex("models/items/spawngro2/tris.md2");
}

void medic_duck(edict_t *self, float eta)
{
//  don't dodge if you're healing
    if (self->monsterinfo.aiflags & AI_MEDIC)
        return;

    if ((self->monsterinfo.currentmove == &medic_move_attackHyperBlaster) ||
        (self->monsterinfo.currentmove == &medic_move_attackCable) ||
        (self->monsterinfo.currentmove == &medic_move_attackBlaster) ||
        (self->monsterinfo.currentmove == &medic_move_callReinforcements)) {
        // he ignores skill
        self->monsterinfo.aiflags &= ~AI_DUCKED;
        return;
    }

    if (skill->value == 0)
        // PMM - stupid dodge
        self->monsterinfo.duck_wait_framenum = level.framenum + (eta + 1) * BASE_FRAMERATE;
    else
        self->monsterinfo.duck_wait_framenum = level.framenum + (eta + (0.1f * (3 - skill->value))) * BASE_FRAMERATE;

    // has to be done immediately otherwise he can get stuck
    monster_duck_down(self);

    self->monsterinfo.nextframe = FRAME_duck1;
    self->monsterinfo.currentmove = &medic_move_duck;
    return;
}

void medic_sidestep(edict_t *self)
{
    if ((self->monsterinfo.currentmove == &medic_move_attackHyperBlaster) ||
        (self->monsterinfo.currentmove == &medic_move_attackCable) ||
        (self->monsterinfo.currentmove == &medic_move_attackBlaster) ||
        (self->monsterinfo.currentmove == &medic_move_callReinforcements)) {
        // if we're shooting, and not on easy, don't dodge
        if (skill->value) {
            self->monsterinfo.aiflags &= ~AI_DODGING;
            return;
        }
    }

    if (self->monsterinfo.currentmove != &medic_move_run)
        self->monsterinfo.currentmove = &medic_move_run;
}

//===========
//PGM
bool medic_blocked(edict_t *self, float dist)
{
    if (blocked_checkshot(self, 0.25f + (0.05f * skill->value)))
        return true;

    if (blocked_checkplat(self, dist))
        return true;

    return false;
}
//PGM
//===========

/*QUAKED monster_medic_commander (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
static void medic_precache(void)
{
    sound_idle1 = gi.soundindex("medic/idle.wav");
    sound_pain1 = gi.soundindex("medic/medpain1.wav");
    sound_pain2 = gi.soundindex("medic/medpain2.wav");
    sound_die = gi.soundindex("medic/meddeth1.wav");
    sound_sight = gi.soundindex("medic/medsght1.wav");
    sound_search = gi.soundindex("medic/medsrch1.wav");
    sound_hook_launch = gi.soundindex("medic/medatck2.wav");
    sound_hook_hit = gi.soundindex("medic/medatck3.wav");
    sound_hook_heal = gi.soundindex("medic/medatck4.wav");
    sound_hook_retract = gi.soundindex("medic/medatck5.wav");
}


// ---------------------------------------------------------------------------
// Baseq2's duck-and-dodge for the medic, restored alongside Ground
// Zero's rewrite so that both ship and the spawn-time latch selects.
//
// Ground Zero does not add to this monster, it replaces its evasion: baseq2's
// per-monster medic_dodge and medic_duck_* become the shared M_MonsterDodge with
// generic monster_duck_* plus a sidestep.  Without this block, `rogue 0` still
// got Ground Zero's AI.
//
// Reintroduced under a bq2_ prefix rather than by un-commenting the donor's dead
// copy, so the two sets are distinct symbols and both are visible to genptr.py
// -- the save_ptrs[] table is built by scanning for literal
// `currentmove = &name` assignments, which is also why the gate below is an
// if/else with two literal assignments rather than a ternary or a macro.
// ---------------------------------------------------------------------------

static void bq2_medic_duck_down(edict_t *self)
{
    if (self->monsterinfo.aiflags & AI_DUCKED)
        return;
    self->monsterinfo.aiflags |= AI_DUCKED;
    self->maxs[2] -= 32;
    self->takedamage = DAMAGE_YES;
    self->monsterinfo.pause_framenum = level.framenum + 1 * BASE_FRAMERATE;
    gi.linkentity(self);
}

static void bq2_medic_duck_hold(edict_t *self)
{
    if (level.framenum >= self->monsterinfo.pause_framenum)
        self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
    else
        self->monsterinfo.aiflags |= AI_HOLD_FRAME;
}

static void bq2_medic_duck_up(edict_t *self)
{
    self->monsterinfo.aiflags &= ~AI_DUCKED;
    self->maxs[2] += 32;
    self->takedamage = DAMAGE_AIM;
    gi.linkentity(self);
}

static const mframe_t bq2_medic_frames_duck[] = {
    { ai_move, -1,    NULL },
    { ai_move, -1,    NULL },
    { ai_move, -1,    bq2_medic_duck_down },
    { ai_move, -1,    bq2_medic_duck_hold },
    { ai_move, -1,    NULL },
    { ai_move, -1,    NULL },
    { ai_move, -1,    bq2_medic_duck_up },
    { ai_move, -1,    NULL },
    { ai_move, -1,    NULL },
    { ai_move, -1,    NULL },
    { ai_move, -1,    NULL },
    { ai_move, -1,    NULL },
    { ai_move, -1,    NULL },
    { ai_move, -1,    NULL },
    { ai_move, -1,    NULL },
    { ai_move, -1,    NULL }
};

const mmove_t bq2_medic_move_duck = {FRAME_duck1, FRAME_duck16, bq2_medic_frames_duck, medic_run};

void bq2_medic_dodge(edict_t *self, edict_t *attacker, float eta, trace_t *tr)
{
    if (random() > 0.25f)
        return;

    if (!self->enemy)
        self->enemy = attacker;

    self->monsterinfo.currentmove = &bq2_medic_move_duck;
}

/*QUAKED monster_medic (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_medic(edict_t *self)
{
    // deathmatch is 1 under ctf, which allows monsters: ask the ruleset.
    if (!G_MonstersAllowed()) {
        G_FreeEdict(self);
        return;
    }

    G_AddPrecache(medic_precache);

    gi.soundindex("medic/medatck1.wav");

    self->movetype = MOVETYPE_STEP;
    self->solid = SOLID_BBOX;
    self->s.modelindex = gi.modelindex("models/monsters/medic/tris.md2");
    VectorSet(self->mins, -24, -24, -24);
    VectorSet(self->maxs, 24, 24, 32);

//PMM
    if (medic_IsCommander(self)) {
        self->health = 600;         //  fixme
        self->gib_health = -130;
        self->mass = 600;
        self->yaw_speed = 40; // default is 20
        MedicCommanderCache();
//      self->s.skinnum = 2;
    } else {
//PMM
        self->health = 300;
        self->gib_health = -130;
        self->mass = 400;
//      self->s.skinnum = 0;
    }

    self->pain = medic_pain;
    self->die = medic_die;

    self->monsterinfo.stand = medic_stand;
    self->monsterinfo.walk = medic_walk;
    self->monsterinfo.run = medic_run;
    // pmm
    // Both evasion sets ship; the latch selects at spawn.  content_flavour is
    // latched in ED_CallSpawn, which runs before this function.
    //
    // An if/else with literal assignments, deliberately: genptr.py builds
    // save_ptrs[] by scanning the source for `= &name`, so a ternary or a macro
    // would hide one or both tables from the savegame pointer table.
    if (medic_UsesRogueBehavior(self)) {
        self->monsterinfo.dodge = M_MonsterDodge;
        self->monsterinfo.duck = medic_duck;
        self->monsterinfo.unduck = monster_duck_up;
        self->monsterinfo.sidestep = medic_sidestep;
    } else {
        self->monsterinfo.dodge = bq2_medic_dodge;
    }
//  self->monsterinfo.dodge = medic_dodge;
    // pmm
    self->monsterinfo.attack = medic_attack;
    self->monsterinfo.melee = NULL;
    self->monsterinfo.sight = medic_sight;
    self->monsterinfo.idle = medic_idle;
    self->monsterinfo.search = medic_search;
    self->monsterinfo.checkattack = medic_checkattack;
    if (medic_UsesRogueBehavior(self))
        self->monsterinfo.blocked = medic_blocked;

    gi.linkentity(self);

    self->monsterinfo.currentmove = &medic_move_stand;
    self->monsterinfo.scale = MODEL_SCALE;

    walkmonster_start(self);

    if (medic_UsesRogueBehavior(self))
        self->monsterinfo.aiflags |= AI_IGNORE_SHOTS;

    if (medic_UsesRogueBehavior(self) && self->mass > 400) {
        self->s.skinnum = 2;
        if (skill->value == 0)
            self->monsterinfo.monster_slots = 3;
        else if (skill->value == 1)
            self->monsterinfo.monster_slots = 4;
        else if (skill->value == 2)
            self->monsterinfo.monster_slots = 6;
        else if (skill->value == 3)
            self->monsterinfo.monster_slots = 6;
        // commander sounds
        commander_sound_idle1 = gi.soundindex("medic_commander/medidle.wav");
        commander_sound_pain1 = gi.soundindex("medic_commander/medpain1.wav");
        commander_sound_pain2 = gi.soundindex("medic_commander/medpain2.wav");
        commander_sound_die = gi.soundindex("medic_commander/meddeth.wav");
        commander_sound_sight = gi.soundindex("medic_commander/medsght.wav");
        commander_sound_search = gi.soundindex("medic_commander/medsrch.wav");
        commander_sound_hook_launch = gi.soundindex("medic_commander/medatck2c.wav");
        commander_sound_hook_hit = gi.soundindex("medic_commander/medatck3a.wav");
        commander_sound_hook_heal = gi.soundindex("medic_commander/medatck4a.wav");
        commander_sound_hook_retract = gi.soundindex("medic_commander/medatck5a.wav");
        commander_sound_spawn = gi.soundindex("medic_commander/monsterspawn1.wav");
        gi.soundindex("tank/tnkatck3.wav");
    } else {
        sound_idle1 = gi.soundindex("medic/idle.wav");
        sound_pain1 = gi.soundindex("medic/medpain1.wav");
        sound_pain2 = gi.soundindex("medic/medpain2.wav");
        sound_die = gi.soundindex("medic/meddeth1.wav");
        sound_sight = gi.soundindex("medic/medsght1.wav");
        sound_search = gi.soundindex("medic/medsrch1.wav");
        sound_hook_launch = gi.soundindex("medic/medatck2.wav");
        sound_hook_hit = gi.soundindex("medic/medatck3.wav");
        sound_hook_heal = gi.soundindex("medic/medatck4.wav");
        sound_hook_retract = gi.soundindex("medic/medatck5.wav");
        gi.soundindex("medic/medatck1.wav");

        self->s.skinnum = 0;
    }
    //pmm
}
