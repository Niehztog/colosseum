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

chick

==============================================================================
*/

#include "g_local.h"
#include "m_chick.h"

// ROGUE
#define LEAD_TARGET     1
// ROGUE

void chick_stand(edict_t *self);
void chick_run(edict_t *self);

static void chick_reslash(edict_t *self);
static void chick_rerocket(edict_t *self);
static void chick_attack1(edict_t *self);

static int  sound_missile_prelaunch;
static int  sound_missile_launch;
static int  sound_melee_swing;
static int  sound_melee_hit;
static int  sound_missile_reload;
static int  sound_death1;
static int  sound_death2;
static int  sound_fall_down;
static int  sound_idle1;
static int  sound_idle2;
static int  sound_pain1;
static int  sound_pain2;
static int  sound_pain3;
static int  sound_sight;
static int  sound_search;

static void ChickMoan(edict_t *self)
{
    if (random() < 0.5f)
        gi.sound(self, CHAN_VOICE, sound_idle1, 1, ATTN_IDLE, 0);
    else
        gi.sound(self, CHAN_VOICE, sound_idle2, 1, ATTN_IDLE, 0);
}

static const mframe_t chick_frames_fidget[] = {
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  ChickMoan },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL }
};
const mmove_t chick_move_fidget = {FRAME_stand201, FRAME_stand230, chick_frames_fidget, chick_stand};

static void chick_fidget(edict_t *self)
{
    if (self->monsterinfo.aiflags & AI_STAND_GROUND)
        return;
    if (random() <= 0.3f)
        self->monsterinfo.currentmove = &chick_move_fidget;
}

static const mframe_t chick_frames_stand[] = {
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
    { ai_stand, 0, chick_fidget },

};
const mmove_t chick_move_stand = {FRAME_stand101, FRAME_stand130, chick_frames_stand, NULL};

void chick_stand(edict_t *self)
{
    self->monsterinfo.currentmove = &chick_move_stand;
}

static const mframe_t chick_frames_start_run[] = {
    { ai_run, 1,  NULL },
    { ai_run, 0,  NULL },
    { ai_run, 0,   NULL },
    { ai_run, -1, NULL },
    { ai_run, -1, NULL },
    { ai_run, 0,  NULL },
    { ai_run, 1,  NULL },
    { ai_run, 3,  NULL },
    { ai_run, 6,   NULL },
    { ai_run, 3,   NULL }
};
const mmove_t chick_move_start_run = {FRAME_walk01, FRAME_walk10, chick_frames_start_run, chick_run};

static const mframe_t chick_frames_run[] = {
    { ai_run, 6,  NULL },
    { ai_run, 8,  NULL },
    { ai_run, 13, NULL },
    { ai_run, 5,  monster_done_dodge }, // make sure to clear dodge bit
    { ai_run, 7,  NULL },
    { ai_run, 4,  NULL },
    { ai_run, 11, NULL },
    { ai_run, 5,  NULL },
    { ai_run, 9,  NULL },
    { ai_run, 7,  NULL }
};

const mmove_t chick_move_run = {FRAME_walk11, FRAME_walk20, chick_frames_run, NULL};

static const mframe_t chick_frames_walk[] = {
    { ai_walk, 6,  NULL },
    { ai_walk, 8,  NULL },
    { ai_walk, 13, NULL },
    { ai_walk, 5,  NULL },
    { ai_walk, 7,  NULL },
    { ai_walk, 4,  NULL },
    { ai_walk, 11, NULL },
    { ai_walk, 5,  NULL },
    { ai_walk, 9,  NULL },
    { ai_walk, 7,  NULL }
};

const mmove_t chick_move_walk = {FRAME_walk11, FRAME_walk20, chick_frames_walk, NULL};

void chick_walk(edict_t *self)
{
    self->monsterinfo.currentmove = &chick_move_walk;
}

void chick_run(edict_t *self)
{
    monster_done_dodge(self);

    if (self->monsterinfo.aiflags & AI_STAND_GROUND) {
        self->monsterinfo.currentmove = &chick_move_stand;
        return;
    }

    if (self->monsterinfo.currentmove == &chick_move_walk ||
        self->monsterinfo.currentmove == &chick_move_start_run) {
        self->monsterinfo.currentmove = &chick_move_run;
    } else {
        self->monsterinfo.currentmove = &chick_move_start_run;
    }
}

static const mframe_t chick_frames_pain1[] = {
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL }
};
const mmove_t chick_move_pain1 = {FRAME_pain101, FRAME_pain105, chick_frames_pain1, chick_run};

static const mframe_t chick_frames_pain2[] = {
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL }
};
const mmove_t chick_move_pain2 = {FRAME_pain201, FRAME_pain205, chick_frames_pain2, chick_run};

static const mframe_t chick_frames_pain3[] = {
    { ai_move, 0,     NULL },
    { ai_move, 0,     NULL },
    { ai_move, -6,    NULL },
    { ai_move, 3,     NULL },
    { ai_move, 11,    NULL },
    { ai_move, 3,     NULL },
    { ai_move, 0,     NULL },
    { ai_move, 0,     NULL },
    { ai_move, 4,     NULL },
    { ai_move, 1,     NULL },
    { ai_move, 0,     NULL },
    { ai_move, -3,    NULL },
    { ai_move, -4,    NULL },
    { ai_move, 5,     NULL },
    { ai_move, 7,     NULL },
    { ai_move, -2,    NULL },
    { ai_move, 3,     NULL },
    { ai_move, -5,    NULL },
    { ai_move, -2,    NULL },
    { ai_move, -8,    NULL },
    { ai_move, 2,     NULL }
};
const mmove_t chick_move_pain3 = {FRAME_pain301, FRAME_pain321, chick_frames_pain3, chick_run};

void chick_pain(edict_t *self, edict_t *other, float kick, int damage)
{
    float   r;

    if (self->content_flavour & CONTENT_ROGUE)
        monster_done_dodge(self);

    if (self->health < (self->max_health / 2))
        self->s.skinnum = 1;

    if (level.framenum < self->pain_debounce_framenum)
        return;

    self->pain_debounce_framenum = level.framenum + 3 * BASE_FRAMERATE;

    r = random();
    if (r < 0.33f)
        gi.sound(self, CHAN_VOICE, sound_pain1, 1, ATTN_NORM, 0);
    else if (r < 0.66f)
        gi.sound(self, CHAN_VOICE, sound_pain2, 1, ATTN_NORM, 0);
    else
        gi.sound(self, CHAN_VOICE, sound_pain3, 1, ATTN_NORM, 0);

    if (skill->value == 3 &&
        !(self->content_flavour & CONTENT_XATRIX))
        return;     // no pain anims in nightmare

    if (damage <= 10)
        self->monsterinfo.currentmove = &chick_move_pain1;
    else if (damage <= 25)
        self->monsterinfo.currentmove = &chick_move_pain2;
    else
        self->monsterinfo.currentmove = &chick_move_pain3;

    if (self->content_flavour & CONTENT_ROGUE) {
        self->monsterinfo.aiflags &= ~AI_MANUAL_STEERING;

        if (self->monsterinfo.aiflags & AI_DUCKED)
            monster_duck_up(self);
    }
}

static void chick_dead(edict_t *self)
{
    VectorSet(self->mins, -16, -16, 0);
    VectorSet(self->maxs, 16, 16, 16);
    self->movetype = MOVETYPE_TOSS;
    self->svflags |= SVF_DEADMONSTER;
    self->nextthink = 0;
    gi.linkentity(self);
}

static const mframe_t chick_frames_death2[] = {
    { ai_move, -6, NULL },
    { ai_move, 0,  NULL },
    { ai_move, -1,  NULL },
    { ai_move, -5, NULL },
    { ai_move, 0, NULL },
    { ai_move, -1,  NULL },
    { ai_move, -2,  NULL },
    { ai_move, 1,  NULL },
    { ai_move, 10, NULL },
    { ai_move, 2,  NULL },
    { ai_move, 3,  NULL },
    { ai_move, 1,  NULL },
    { ai_move, 2, NULL },
    { ai_move, 0,  NULL },
    { ai_move, 3,  NULL },
    { ai_move, 3,  NULL },
    { ai_move, 1,  NULL },
    { ai_move, -3,  NULL },
    { ai_move, -5, NULL },
    { ai_move, 4, NULL },
    { ai_move, 15, NULL },
    { ai_move, 14, NULL },
    { ai_move, 1, NULL }
};
const mmove_t chick_move_death2 = {FRAME_death201, FRAME_death223, chick_frames_death2, chick_dead};

static const mframe_t chick_frames_death1[] = {
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, -7, NULL },
    { ai_move, 4,  NULL },
    { ai_move, 11, NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL }

};
const mmove_t chick_move_death1 = {FRAME_death101, FRAME_death112, chick_frames_death1, chick_dead};

void chick_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    int     n;

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
    self->deadflag = DEAD_DEAD;
    self->takedamage = DAMAGE_YES;

    n = Q_rand() % 2;
    if (n == 0) {
        self->monsterinfo.currentmove = &chick_move_death1;
        gi.sound(self, CHAN_VOICE, sound_death1, 1, ATTN_NORM, 0);
    } else {
        self->monsterinfo.currentmove = &chick_move_death2;
        gi.sound(self, CHAN_VOICE, sound_death2, 1, ATTN_NORM, 0);
    }
}

// PMM - changes to duck code for new dodge

static const mframe_t chick_frames_duck[] = {
    { ai_move, 0, monster_duck_down },
    { ai_move, 1, NULL },
    { ai_move, 4, monster_duck_hold },
    { ai_move, -4,  NULL },
    { ai_move, -5,  monster_duck_up },
    { ai_move, 3, NULL },
    { ai_move, 1,  NULL }
};
const mmove_t chick_move_duck = {FRAME_duck01, FRAME_duck07, chick_frames_duck, chick_run};

/*
void chick_dodge (edict_t *self, edict_t *attacker, float eta, trace_t *tr)
{
// begin orig code
    if (random() > 0.25)
        return;

    if (!self->enemy)
        self->enemy = attacker;

    self->monsterinfo.currentmove = &chick_move_duck;
// end

    float   r;
    float   height;
    int     shooting = 0;

    if (!self->enemy)
    {
        self->enemy = attacker;
        FoundTarget (self);
    }

    // PMM - don't bother if it's going to hit anyway; fix for weird in-your-face etas (I was
    // seeing numbers like 13 and 14)
    if ((eta < 0.1) || (eta > 5))
        return;

    r = random();
    if (r > (0.25*((skill->value)+1)))
        return;

    if ((self->monsterinfo.currentmove == &chick_move_start_attack1) ||
        (self->monsterinfo.currentmove == &chick_move_attack1))
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
        self->monsterinfo.currentmove = &chick_move_run;
        self->monsterinfo.attack_state = AS_SLIDING;
        return;
    }

    if (skill->value == 0)
    {
        self->monsterinfo.currentmove = &chick_move_duck;
        // PMM - stupid dodge
        self->monsterinfo.duck_wait_framenum = level.framenum + (eta + 1) * BASE_FRAMERATE;
        self->monsterinfo.aiflags |= AI_DODGING;
        return;
    }

    if (!shooting)
    {
        self->monsterinfo.currentmove = &chick_move_duck;
        self->monsterinfo.duck_wait_framenum = level.framenum + (eta + (0.1 * (3 - skill->value))) * BASE_FRAMERATE;
        self->monsterinfo.aiflags |= AI_DODGING;
    }
    return;

}
*/
static void ChickSlash(edict_t *self)
{
    vec3_t  aim = { MELEE_DISTANCE, self->mins[0], 10 };

    gi.sound(self, CHAN_WEAPON, sound_melee_swing, 1, ATTN_NORM, 0);
    fire_hit(self, aim, (10 + (Q_rand() % 6)), 100);
}

// *** THE SKIN IS THE SELECTOR, AND THE CONTENT LATCH HAS NO BUSINESS HERE. ***
//
// The Reckoning's heat-seeking chick is a SEPARATE ENTITY, not a variant of the
// shared one: `monster_chick_heat` has its own spawn-table row, and
// `SP_monster_chick_heat` is `SP_monster_chick` plus `s.skinnum = 3`.  Nothing
// else on a chick ever writes a skin above 1 -- `chick_pain`'s is `= 1` -- so
// `skinnum > 1` means "this is that entity" and cannot be true of id's chick.
// That is the donor's own test, `port_xatrix:m_chick.c:437`, verbatim.
//
// It carried `content_flavour & CONTENT_XATRIX` as well, and that was wrong.
// The latch is set in ED_CallSpawn from `G_LayerEnabled`, so it says whether the
// SERVER has the layer on -- it is not a property of the entity.  Its job is to
// choose between two donors' versions of a monster BOTH of them have; a
// pack-exclusive entity is not that case.  The spawn path refuses nothing when a
// layer is off, so `monster_gekk`, `monster_widow` and the rest keep working;
// only the heat chick silently lost its weapon and threw plain rockets, which is
// the silent-wrong-answer shape ED_CallSpawn's own comment was written against.
//
// NOT a defect, and not to be "fixed": The Reckoning left `chick_pain`'s
// `s.skinnum = 1` alone, so a heat chick hurt below half health drops to skin 1
// and fires rockets for the rest of its life.  That is the donor's behaviour --
// the 2023 rerelease is the tree that changed it, to `|= 1` / `&= ~1` -- and
// R-200 does not take that tree as a behaviour source.
static void ChickFireProjectile(edict_t *self, vec3_t start, vec3_t dir, int speed)
{
    if (self->s.skinnum > 1)
        xatrix_monster_fire_heat(self, start, dir, 50, speed, MZ2_CHICK_ROCKET_1);
    else
        monster_fire_rocket(self, start, dir, 50, speed, MZ2_CHICK_ROCKET_1);
}

static void ChickRocket(edict_t *self)
{
    vec3_t  forward, right;
    vec3_t  start;
    vec3_t  dir;
    vec3_t  vec;
    trace_t trace;  // PMM - check target
    int     rocketSpeed = 0;
    float   dist;
    // pmm - blindfire
    vec3_t  target;
    bool blindfire = false;

    if (!self->enemy || !self->enemy->inuse)    //PGM
        return;                                 //PGM

    AngleVectors(self->s.angles, forward, right, NULL);
    G_ProjectSource(self->s.origin, monster_flash_offset[MZ2_CHICK_ROCKET_1], forward, right, start);

    // Rogue replaces the direct base/Xatrix shot with blindfire, leading and
    // trace retries. The projectile type still composes with the Xatrix layer.
    if (!(self->content_flavour & CONTENT_ROGUE)) {
        VectorCopy(self->enemy->s.origin, vec);
        vec[2] += self->enemy->viewheight;
        VectorSubtract(vec, start, dir);
        VectorNormalize(dir);
        ChickFireProjectile(self, start, dir, 500);
        return;
    }

    if (self->monsterinfo.aiflags & AI_MANUAL_STEERING)
        blindfire = true;
    else
        blindfire = false;

    rocketSpeed = 500 + (100 * skill->value);   // PGM rock & roll.... :)

    // put a debug trail from start to endpoint, confirm that the start point is
    // correct for the trace

    // PMM
    if (blindfire)
        VectorCopy(self->monsterinfo.blind_fire_target, target);
    else
        VectorCopy(self->enemy->s.origin, target);
    // pmm
//PGM
    // PMM - blindfire shooting
    if (blindfire) {
        VectorCopy(target, vec);
        VectorSubtract(vec, start, dir);
    }
    // pmm
    // don't shoot at feet if they're above where i'm shooting from.
    else if (random() < 0.33f || (start[2] < self->enemy->absmin[2])) {
//      gi.dprintf("normal shot\n");
        VectorCopy(target, vec);
        vec[2] += self->enemy->viewheight;
        VectorSubtract(vec, start, dir);
    } else {
//      gi.dprintf("shooting at feet!\n");
        VectorCopy(target, vec);
        vec[2] = self->enemy->absmin[2];
        VectorSubtract(vec, start, dir);
    }
//PGM

//======
//PMM - lead target  (not when blindfiring)
    // 20, 35, 50, 65 chance of leading
    if ((!blindfire) && ((random() < (0.2f + ((3 - skill->value) * 0.15f))))) {
        float   time;

//      gi.dprintf ("leading target\n");
        dist = VectorLength(dir);
        time = dist / rocketSpeed;
        VectorMA(vec, time, self->enemy->velocity, vec);
        VectorSubtract(vec, start, dir);
    }
//PMM - lead target
//======

    VectorNormalize(dir);

    // pmm blindfire doesn't check target (done in checkattack)
    // paranoia, make sure we're not shooting a target right next to us
    trace = gi.trace(start, vec3_origin, vec3_origin, vec, self, MASK_SHOT);
    if (blindfire) {
        // blindfire has different fail criteria for the trace
        if (!(trace.startsolid || trace.allsolid || (trace.fraction < 0.5f)))
            ChickFireProjectile(self, start, dir, rocketSpeed);
        else {
            // geez, this is bad.  she's avoiding about 80% of her blindfires due to hitting things.
            // hunt around for a good shot
            // try shifting the target to the left a little (to help counter her large offset)
            VectorCopy(target, vec);
            VectorMA(vec, -10, right, vec);
            VectorSubtract(vec, start, dir);
            VectorNormalize(dir);
            trace = gi.trace(start, vec3_origin, vec3_origin, vec, self, MASK_SHOT);
            if (!(trace.startsolid || trace.allsolid || (trace.fraction < 0.5f)))
                ChickFireProjectile(self, start, dir, rocketSpeed);
            else {
                // ok, that failed.  try to the right
                VectorCopy(target, vec);
                VectorMA(vec, 10, right, vec);
                VectorSubtract(vec, start, dir);
                VectorNormalize(dir);
                trace = gi.trace(start, vec3_origin, vec3_origin, vec, self, MASK_SHOT);
                if (!(trace.startsolid || trace.allsolid || (trace.fraction < 0.5f)))
                    ChickFireProjectile(self, start, dir, rocketSpeed);
//              else if ((g_showlogic) && (g_showlogic->value))
//                  // ok, I give up
//                  gi.dprintf ("chick avoiding blindfire shot\n");
            }
        }
    } else {
        trace = gi.trace(start, vec3_origin, vec3_origin, vec, self, MASK_SHOT);
        if (trace.ent == self->enemy || trace.ent == world) {
            if (trace.fraction > 0.5f || (trace.ent && trace.ent->client))
                ChickFireProjectile(self, start, dir, rocketSpeed);
            //      else
            //          gi.dprintf("didn't make it halfway to target...aborting\n");
        }
    }
}

static void Chick_PreAttack1(edict_t *self)
{
    gi.sound(self, CHAN_VOICE, sound_missile_prelaunch, 1, ATTN_NORM, 0);
}

static void ChickReload(edict_t *self)
{
    gi.sound(self, CHAN_VOICE, sound_missile_reload, 1, ATTN_NORM, 0);
}

static const mframe_t chick_frames_start_attack1[] = {
    { ai_charge, 0,   Chick_PreAttack1 },
    { ai_charge, 0,   NULL },
    { ai_charge, 0,   NULL },
    { ai_charge, 4,   NULL },
    { ai_charge, 0,   NULL },
    { ai_charge, -3,  NULL },
    { ai_charge, 3,   NULL },
    { ai_charge, 5,   NULL },
    { ai_charge, 7,   NULL },
    { ai_charge, 0,   NULL },
    { ai_charge, 0,   NULL },
    { ai_charge, 0,   NULL },
    { ai_charge, 0,   chick_attack1 }
};
const mmove_t chick_move_start_attack1 = {FRAME_attak101, FRAME_attak113, chick_frames_start_attack1, NULL};

static const mframe_t chick_frames_attack1[] = {
    { ai_charge, 19,  ChickRocket },
    { ai_charge, -6,  NULL },
    { ai_charge, -5,  NULL },
    { ai_charge, -2,  NULL },
    { ai_charge, -7,  NULL },
    { ai_charge, 0,   NULL },
    { ai_charge, 1,   NULL },
    { ai_charge, 10,  ChickReload },
    { ai_charge, 4,   NULL },
    { ai_charge, 5,   NULL },
    { ai_charge, 6,   NULL },
    { ai_charge, 6,   NULL },
    { ai_charge, 4,   NULL },
    { ai_charge, 3,   chick_rerocket }

};
const mmove_t chick_move_attack1 = {FRAME_attak114, FRAME_attak127, chick_frames_attack1, NULL};

static const mframe_t chick_frames_end_attack1[] = {
    { ai_charge, -3,  NULL },
    { ai_charge, 0,   NULL },
    { ai_charge, -6,  NULL },
    { ai_charge, -4,  NULL },
    { ai_charge, -2,  NULL }
};
const mmove_t chick_move_end_attack1 = {FRAME_attak128, FRAME_attak132, chick_frames_end_attack1, chick_run};

static void chick_rerocket(edict_t *self)
{
    float repeat_chance = 0.6f;

    if ((self->content_flavour & CONTENT_ROGUE) &&
        (self->monsterinfo.aiflags & AI_MANUAL_STEERING)) {
        self->monsterinfo.aiflags &= ~AI_MANUAL_STEERING;
        self->monsterinfo.currentmove = &chick_move_end_attack1;
        return;
    }
    if (self->content_flavour & CONTENT_ROGUE)
        repeat_chance += 0.05f * skill->value;

    if (self->enemy->health > 0) {
        if (range(self, self->enemy) > RANGE_MELEE)
            if (visible(self, self->enemy))
                if (random() <= repeat_chance) {
                    self->monsterinfo.currentmove = &chick_move_attack1;
                    return;
                }
    }
    self->monsterinfo.currentmove = &chick_move_end_attack1;
}

static void chick_attack1(edict_t *self)
{
    self->monsterinfo.currentmove = &chick_move_attack1;
}

static const mframe_t chick_frames_slash[] = {
    { ai_charge, 1,   NULL },
    { ai_charge, 7,   ChickSlash },
    { ai_charge, -7,  NULL },
    { ai_charge, 1,   NULL },
    { ai_charge, -1,  NULL },
    { ai_charge, 1,   NULL },
    { ai_charge, 0,   NULL },
    { ai_charge, 1,   NULL },
    { ai_charge, -2,  chick_reslash }
};
const mmove_t chick_move_slash = {FRAME_attak204, FRAME_attak212, chick_frames_slash, NULL};

static const mframe_t chick_frames_end_slash[] = {
    { ai_charge, -6,  NULL },
    { ai_charge, -1,  NULL },
    { ai_charge, -6,  NULL },
    { ai_charge, 0,   NULL }
};
const mmove_t chick_move_end_slash = {FRAME_attak213, FRAME_attak216, chick_frames_end_slash, chick_run};

static void chick_reslash(edict_t *self)
{
    if (self->enemy->health > 0) {
        if (range(self, self->enemy) == RANGE_MELEE) {
            if (random() <= 0.9f) {
                self->monsterinfo.currentmove = &chick_move_slash;
                return;
            } else {
                self->monsterinfo.currentmove = &chick_move_end_slash;
                return;
            }
        }
    }
    self->monsterinfo.currentmove = &chick_move_end_slash;
}

static void chick_slash(edict_t *self)
{
    self->monsterinfo.currentmove = &chick_move_slash;
}

static const mframe_t chick_frames_start_slash[] = {
    { ai_charge, 1,   NULL },
    { ai_charge, 8,   NULL },
    { ai_charge, 3,   NULL }
};
const mmove_t chick_move_start_slash = {FRAME_attak201, FRAME_attak203, chick_frames_start_slash, chick_slash};

void chick_melee(edict_t *self)
{
    self->monsterinfo.currentmove = &chick_move_start_slash;
}

void chick_attack(edict_t *self)
{
    float r, chance;

    if (self->content_flavour & CONTENT_ROGUE)
        monster_done_dodge(self);

    // PMM
    if ((self->content_flavour & CONTENT_ROGUE) &&
        self->monsterinfo.attack_state == AS_BLIND) {
        // setup shot probabilities
        if (self->monsterinfo.blind_fire_delay < 1.0f)
            chance = 1.0f;
        else if (self->monsterinfo.blind_fire_delay < 7.5f)
            chance = 0.4f;
        else
            chance = 0.1f;

        r = random();

        // minimum of 2 seconds, plus 0-3, after the shots are done
        self->monsterinfo.blind_fire_delay += 4.0f + 1.5f + random();

        // don't shoot at the origin
        if (VectorCompare(self->monsterinfo.blind_fire_target, vec3_origin))
            return;

        // don't shoot if the dice say not to
        if (r > chance) {
//          if ((g_showlogic) && (g_showlogic->value))
//              gi.dprintf ("blindfire - NO SHOT\n");
            return;
        }

        // turn on manual steering to signal both manual steering and blindfire
        self->monsterinfo.aiflags |= AI_MANUAL_STEERING;
        self->monsterinfo.currentmove = &chick_move_start_attack1;
        self->monsterinfo.attack_finished = level.time + 2 * random();
        return;
    }
    // pmm

    self->monsterinfo.currentmove = &chick_move_start_attack1;
}

void chick_sight(edict_t *self, edict_t *other)
{
    gi.sound(self, CHAN_VOICE, sound_sight, 1, ATTN_NORM, 0);
}

//===========
//PGM
bool chick_blocked(edict_t *self, float dist)
{
    if (blocked_checkshot(self, 0.25f + (0.05f * skill->value)))
        return true;

    if (blocked_checkplat(self, dist))
        return true;

    return false;
}
//PGM
//===========

void chick_duck(edict_t *self, float eta)
{
    if ((self->monsterinfo.currentmove == &chick_move_start_attack1) ||
        (self->monsterinfo.currentmove == &chick_move_attack1)) {
        // if we're shooting, and not on easy, don't dodge
        if (skill->value) {
            self->monsterinfo.aiflags &= ~AI_DUCKED;
            return;
        }
    }

    if (skill->value == 0)
        // PMM - stupid dodge
        self->monsterinfo.duck_wait_framenum = level.framenum + (eta + 1) * BASE_FRAMERATE;
    else
        self->monsterinfo.duck_wait_framenum = level.framenum + (eta + (0.1f * (3 - skill->value))) * BASE_FRAMERATE;

    // has to be done immediately otherwise she can get stuck
    monster_duck_down(self);

    self->monsterinfo.nextframe = FRAME_duck01;
    self->monsterinfo.currentmove = &chick_move_duck;
    return;
}

void chick_sidestep(edict_t *self)
{
    if ((self->monsterinfo.currentmove == &chick_move_start_attack1) ||
        (self->monsterinfo.currentmove == &chick_move_attack1)) {
        // if we're shooting, and not on easy, don't dodge
        if (skill->value) {
            self->monsterinfo.aiflags &= ~AI_DODGING;
            return;
        }
    }

    if (self->monsterinfo.currentmove != &chick_move_run)
        self->monsterinfo.currentmove = &chick_move_run;
}

static void chick_precache(void)
{
    sound_missile_prelaunch = gi.soundindex("chick/chkatck1.wav");
    sound_missile_launch    = gi.soundindex("chick/chkatck2.wav");
    sound_melee_swing       = gi.soundindex("chick/chkatck3.wav");
    sound_melee_hit         = gi.soundindex("chick/chkatck4.wav");
    sound_missile_reload    = gi.soundindex("chick/chkatck5.wav");
    sound_death1            = gi.soundindex("chick/chkdeth1.wav");
    sound_death2            = gi.soundindex("chick/chkdeth2.wav");
    sound_fall_down         = gi.soundindex("chick/chkfall1.wav");
    sound_idle1             = gi.soundindex("chick/chkidle1.wav");
    sound_idle2             = gi.soundindex("chick/chkidle2.wav");
    sound_pain1             = gi.soundindex("chick/chkpain1.wav");
    sound_pain2             = gi.soundindex("chick/chkpain2.wav");
    sound_pain3             = gi.soundindex("chick/chkpain3.wav");
    sound_sight             = gi.soundindex("chick/chksght1.wav");
    sound_search            = gi.soundindex("chick/chksrch1.wav");
}


// ---------------------------------------------------------------------------
// R-CORE-11: baseq2's duck-and-dodge for the chick, restored alongside Ground
// Zero's rewrite so that BOTH ship and the spawn-time latch selects.
//
// Ground Zero does not add to this monster, it replaces its evasion: baseq2's
// per-monster chick_dodge and chick_duck_* become the shared M_MonsterDodge with
// generic monster_duck_* plus a sidestep.  Without this block, `rogue 0` still
// got Ground Zero's AI.
//
// Reintroduced under a bq2_ prefix rather than by un-commenting the donor's dead
// copy, so the two sets are distinct symbols and both are visible to genptr.py
// -- the save_ptrs[] table is built by scanning for literal
// `currentmove = &name` assignments, which is also why the gate below is an
// if/else with two literal assignments rather than a ternary or a macro.
// ---------------------------------------------------------------------------

static void bq2_chick_duck_down(edict_t *self)
{
    if (self->monsterinfo.aiflags & AI_DUCKED)
        return;
    self->monsterinfo.aiflags |= AI_DUCKED;
    self->maxs[2] -= 32;
    self->takedamage = DAMAGE_YES;
    self->monsterinfo.pause_framenum = level.framenum + 1 * BASE_FRAMERATE;
    gi.linkentity(self);
}

static void bq2_chick_duck_hold(edict_t *self)
{
    if (level.framenum >= self->monsterinfo.pause_framenum)
        self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
    else
        self->monsterinfo.aiflags |= AI_HOLD_FRAME;
}

static void bq2_chick_duck_up(edict_t *self)
{
    self->monsterinfo.aiflags &= ~AI_DUCKED;
    self->maxs[2] += 32;
    self->takedamage = DAMAGE_AIM;
    gi.linkentity(self);
}

static const mframe_t bq2_chick_frames_duck[] = {
    { ai_move, 0, bq2_chick_duck_down },
    { ai_move, 1, NULL },
    { ai_move, 4, bq2_chick_duck_hold },
    { ai_move, -4,  NULL },
    { ai_move, -5,  bq2_chick_duck_up },
    { ai_move, 3, NULL },
    { ai_move, 1,  NULL }
};

const mmove_t bq2_chick_move_duck = {FRAME_duck01, FRAME_duck07, bq2_chick_frames_duck, chick_run};

void bq2_chick_dodge(edict_t *self, edict_t *attacker, float eta, trace_t *tr)
{
    if (random() > 0.25f)
        return;

    if (!self->enemy)
        self->enemy = attacker;

    self->monsterinfo.currentmove = &bq2_chick_move_duck;
}

/*QUAKED monster_chick (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_chick(edict_t *self)
{
    // R-MODE-7 / R-CORE-8: the ruleset decides, not `deathmatch`.  The
    // inherited test was right for baseq2 and wrong here, because `deathmatch`
    // is 1 under ctf and R-MODE-7 promises monsters under ctf.
    if (!G_MonstersAllowed()) {
        G_FreeEdict(self);
        return;
    }

    G_AddPrecache(chick_precache);

    self->movetype = MOVETYPE_STEP;
    self->solid = SOLID_BBOX;
    self->s.modelindex = gi.modelindex("models/monsters/bitch2/tris.md2");
    VectorSet(self->mins, -16, -16, 0);
    VectorSet(self->maxs, 16, 16, 56);

    self->health = 175;
    self->gib_health = -70;
    self->mass = 200;

    self->pain = chick_pain;
    self->die = chick_die;

    self->monsterinfo.stand = chick_stand;
    self->monsterinfo.walk = chick_walk;
    self->monsterinfo.run = chick_run;
    // pmm
    // *** R-CORE-11's gate. ***  Both evasion sets ship; the latch selects at
    // spawn.  content_flavour is latched in ED_CallSpawn, which runs BEFORE this
    // function -- R-CORE-11 names monster_start as the latch point and that is
    // too late, see doc/reconciliation.md R-31.
    //
    // An if/else with literal assignments, deliberately: genptr.py builds
    // save_ptrs[] by scanning the source for `= &name`, so a ternary or a macro
    // would hide one or both tables from the savegame pointer table.
    if (self->content_flavour & CONTENT_ROGUE) {
        self->monsterinfo.dodge = M_MonsterDodge;
        self->monsterinfo.duck = chick_duck;
        self->monsterinfo.unduck = monster_duck_up;
        self->monsterinfo.sidestep = chick_sidestep;
        self->monsterinfo.blocked = chick_blocked;
        self->monsterinfo.blindfire = true;
    } else {
        self->monsterinfo.dodge = bq2_chick_dodge;
    }
//  self->monsterinfo.dodge = chick_dodge;
    // pmm
    self->monsterinfo.attack = chick_attack;
    self->monsterinfo.melee = chick_melee;
    self->monsterinfo.sight = chick_sight;

    gi.linkentity(self);

    self->monsterinfo.currentmove = &chick_move_stand;
    self->monsterinfo.scale = MODEL_SCALE;

    walkmonster_start(self);
}

/*QUAKED monster_chick_heat (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_chick_heat(edict_t *self)
{
    SP_monster_chick(self);
    self->s.skinnum = 3;
}
