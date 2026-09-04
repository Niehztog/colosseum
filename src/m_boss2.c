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

boss2

==============================================================================
*/

#include "g_local.h"
#include "m_boss2.h"

void BossExplode(edict_t *self);

bool infront(edict_t *self, edict_t *other);

static int  sound_pain1;
static int  sound_pain2;
static int  sound_pain3;
static int  sound_death;
static int  sound_search1;

void boss2_search(edict_t *self)
{
    if (random() < 0.5f)
        gi.sound(self, CHAN_VOICE, sound_search1, 1, ATTN_NONE, 0);
}

void boss2_run(edict_t *self);
void boss2_stand(edict_t *self);
void boss2_attack(edict_t *self);
void boss2_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point);

#define BOSS2_ROCKET_SPEED  750

static void Boss2PredictiveRocket(edict_t *self)
{
    vec3_t  forward, right;
    vec3_t  start;
    vec3_t  dir;
    vec3_t  vec;
    float   time, dist;

    gi.dprintf("predictive fire\n");

    AngleVectors(self->s.angles, forward, right, NULL);

//1
    G_ProjectSource(self->s.origin, monster_flash_offset[MZ2_BOSS2_ROCKET_1], forward, right, start);
    VectorSubtract(self->enemy->s.origin, start, dir);
//      dir[2] += self->enemy->viewheight;
    dist = VectorLength(dir);
    time = dist / BOSS2_ROCKET_SPEED;
    VectorMA(self->enemy->s.origin, time - 0.3f, self->enemy->velocity, vec);

//  VectorCopy (self->enemy->s.origin, vec);
//  vec[2] += self->enemy->viewheight;
    VectorSubtract(vec, start, dir);
    VectorNormalize(dir);
    monster_fire_rocket(self, start, dir, 50, BOSS2_ROCKET_SPEED, MZ2_BOSS2_ROCKET_1);

//2
    G_ProjectSource(self->s.origin, monster_flash_offset[MZ2_BOSS2_ROCKET_2], forward, right, start);
    VectorSubtract(self->enemy->s.origin, start, dir);
//      dir[2] += self->enemy->viewheight;
    dist = VectorLength(dir);
    time = dist / BOSS2_ROCKET_SPEED;
    VectorMA(self->enemy->s.origin, time - 0.15f, self->enemy->velocity, vec);

//  VectorCopy (self->enemy->s.origin, vec);
//  vec[2] += self->enemy->viewheight;
    VectorSubtract(vec, start, dir);
    VectorNormalize(dir);
    monster_fire_rocket(self, start, dir, 50, BOSS2_ROCKET_SPEED, MZ2_BOSS2_ROCKET_2);

//3
    G_ProjectSource(self->s.origin, monster_flash_offset[MZ2_BOSS2_ROCKET_3], forward, right, start);
    VectorSubtract(self->enemy->s.origin, start, dir);
//      dir[2] += self->enemy->viewheight;
    dist = VectorLength(dir);
    time = dist / BOSS2_ROCKET_SPEED;
    VectorMA(self->enemy->s.origin, time, self->enemy->velocity, vec);

//  VectorCopy (self->enemy->s.origin, vec);
//  vec[2] += self->enemy->viewheight;
    VectorSubtract(vec, start, dir);
    VectorNormalize(dir);
    monster_fire_rocket(self, start, dir, 50, BOSS2_ROCKET_SPEED, MZ2_BOSS2_ROCKET_3);

//4
    G_ProjectSource(self->s.origin, monster_flash_offset[MZ2_BOSS2_ROCKET_4], forward, right, start);
    VectorSubtract(self->enemy->s.origin, start, dir);
//      dir[2] += self->enemy->viewheight;
    dist = VectorLength(dir);
    time = dist / BOSS2_ROCKET_SPEED;
    VectorMA(self->enemy->s.origin, time + 0.15f, self->enemy->velocity, vec);

//  VectorCopy (self->enemy->s.origin, vec);
//  vec[2] += self->enemy->viewheight;
    VectorSubtract(vec, start, dir);
    VectorNormalize(dir);
    monster_fire_rocket(self, start, dir, 50, BOSS2_ROCKET_SPEED, MZ2_BOSS2_ROCKET_4);
}

static void boss2_dead(edict_t *self);
static void boss2_attack_mg(edict_t *self);
static void boss2_reattack_mg(edict_t *self);

static void Boss2Rocket(edict_t *self)
{
    vec3_t  forward, right;
    vec3_t  start;
    vec3_t  dir;
    vec3_t  vec;

    if ((self->content_flavour & CONTENT_ROGUE) && self->enemy) {
        if (self->enemy->client && random() < 0.9f) {
            Boss2PredictiveRocket(self);
            return;
        }
    }

    AngleVectors(self->s.angles, forward, right, NULL);

//1
    G_ProjectSource(self->s.origin, monster_flash_offset[MZ2_BOSS2_ROCKET_1], forward, right, start);
    VectorCopy(self->enemy->s.origin, vec);
    if (self->content_flavour & CONTENT_ROGUE)
        vec[2] -= 15;
    else
        vec[2] += self->enemy->viewheight;
    VectorSubtract(vec, start, dir);
    VectorNormalize(dir);
    if (self->content_flavour & CONTENT_ROGUE) {
        VectorMA(dir, 0.4f, right, dir);
        VectorNormalize(dir);
    }
    monster_fire_rocket(self, start, dir, 50, 500, MZ2_BOSS2_ROCKET_1);

//2
    G_ProjectSource(self->s.origin, monster_flash_offset[MZ2_BOSS2_ROCKET_2], forward, right, start);
    VectorCopy(self->enemy->s.origin, vec);
    if (!(self->content_flavour & CONTENT_ROGUE))
        vec[2] += self->enemy->viewheight;
    VectorSubtract(vec, start, dir);
    VectorNormalize(dir);
    if (self->content_flavour & CONTENT_ROGUE) {
        VectorMA(dir, 0.025f, right, dir);
        VectorNormalize(dir);
    }
    monster_fire_rocket(self, start, dir, 50, 500, MZ2_BOSS2_ROCKET_2);

//3
    G_ProjectSource(self->s.origin, monster_flash_offset[MZ2_BOSS2_ROCKET_3], forward, right, start);
    VectorCopy(self->enemy->s.origin, vec);
    if (!(self->content_flavour & CONTENT_ROGUE))
        vec[2] += self->enemy->viewheight;
    VectorSubtract(vec, start, dir);
    VectorNormalize(dir);
    if (self->content_flavour & CONTENT_ROGUE) {
        VectorMA(dir, -0.025f, right, dir);
        VectorNormalize(dir);
    }
    monster_fire_rocket(self, start, dir, 50, 500, MZ2_BOSS2_ROCKET_3);

//4
    G_ProjectSource(self->s.origin, monster_flash_offset[MZ2_BOSS2_ROCKET_4], forward, right, start);
    VectorCopy(self->enemy->s.origin, vec);
    if (self->content_flavour & CONTENT_ROGUE)
        vec[2] -= 15;
    else
        vec[2] += self->enemy->viewheight;
    VectorSubtract(vec, start, dir);
    VectorNormalize(dir);
    if (self->content_flavour & CONTENT_ROGUE) {
        VectorMA(dir, -0.4f, right, dir);
        VectorNormalize(dir);
    }
    monster_fire_rocket(self, start, dir, 50, 500, MZ2_BOSS2_ROCKET_4);

//5
//  G_ProjectSource (self->s.origin, monster_flash_offset[MZ2_BOSS2_ROCKET_4], forward, right, start);
//  VectorCopy (self->enemy->s.origin, vec);
//  vec[2] += self->enemy->viewheight;
//  VectorSubtract (vec, start, dir);
//  VectorNormalize (dir);
//  monster_fire_rocket (self, start, dir, 50, 500, MZ2_BOSS2_ROCKET_2);
}

static void boss2_firebullet_right(edict_t *self)
{
    vec3_t  forward, right, target;
    vec3_t  start;

    AngleVectors(self->s.angles, forward, right, NULL);
    G_ProjectSource(self->s.origin, monster_flash_offset[MZ2_BOSS2_MACHINEGUN_R1], forward, right, start);

//  VectorMA (self->enemy->s.origin, -0.2, self->enemy->velocity, target);
    VectorMA(self->enemy->s.origin,
             (self->content_flavour & CONTENT_ROGUE) ? 0.2f : -0.2f,
             self->enemy->velocity, target);
    target[2] += self->enemy->viewheight;
    VectorSubtract(target, start, forward);
    VectorNormalize(forward);

    monster_fire_bullet(self, start, forward, 6, 4,
                        (self->content_flavour & CONTENT_ROGUE) ? DEFAULT_BULLET_HSPREAD * 3 : DEFAULT_BULLET_HSPREAD,
                        DEFAULT_BULLET_VSPREAD, MZ2_BOSS2_MACHINEGUN_R1);
//  monster_fire_bullet (self, start, forward, 6, 4, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, MZ2_BOSS2_MACHINEGUN_R1);
}

static void boss2_firebullet_left(edict_t *self)
{
    vec3_t  forward, right, target;
    vec3_t  start;

    AngleVectors(self->s.angles, forward, right, NULL);
    G_ProjectSource(self->s.origin, monster_flash_offset[MZ2_BOSS2_MACHINEGUN_L1], forward, right, start);

//  VectorMA (self->enemy->s.origin, 0.2, self->enemy->velocity, target);
    VectorMA(self->enemy->s.origin, -0.2f, self->enemy->velocity, target);

    target[2] += self->enemy->viewheight;
    VectorSubtract(target, start, forward);
    VectorNormalize(forward);

    monster_fire_bullet(self, start, forward, 6, 4,
                        (self->content_flavour & CONTENT_ROGUE) ? DEFAULT_BULLET_HSPREAD * 3 : DEFAULT_BULLET_HSPREAD,
                        DEFAULT_BULLET_VSPREAD, MZ2_BOSS2_MACHINEGUN_L1);
//  monster_fire_bullet (self, start, forward, 6, 4, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, MZ2_BOSS2_MACHINEGUN_L1);
}

static void Boss2MachineGun(edict_t *self)
{
    boss2_firebullet_left(self);
    boss2_firebullet_right(self);
}

static const mframe_t boss2_frames_stand[] = {
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
    { ai_stand, 0, NULL }
};
const mmove_t boss2_move_stand = {FRAME_stand30, FRAME_stand50, boss2_frames_stand, NULL};

static const mframe_t boss2_frames_fidget[] = {
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
    { ai_stand, 0, NULL }
};
const mmove_t boss2_move_fidget = {FRAME_stand1, FRAME_stand30, boss2_frames_fidget, NULL};

static const mframe_t boss2_frames_walk[] = {
    /* PMM - normally, this is all 8's .. I upped it */
    { ai_walk,    10, NULL },
    { ai_walk,    10, NULL },
    { ai_walk,    10, NULL },
    { ai_walk,    10, NULL },
    { ai_walk,    10, NULL },
    { ai_walk,    10, NULL },
    { ai_walk,    10, NULL },
    { ai_walk,    10, NULL },
    { ai_walk,    10, NULL },
    { ai_walk,    10, NULL },
    { ai_walk,    10, NULL },
    { ai_walk,    10, NULL },
    { ai_walk,    10, NULL },
    { ai_walk,    10, NULL },
    { ai_walk,    10, NULL },
    { ai_walk,    10, NULL },
    { ai_walk,    10, NULL },
    { ai_walk,    10, NULL },
    { ai_walk,    10, NULL },
    { ai_walk,    10, NULL }
};
const mmove_t boss2_move_walk = {FRAME_walk1, FRAME_walk20, boss2_frames_walk, NULL};

static const mframe_t boss2_frames_run[] = {
    /* PMM - normally, this is all 8's .. I upped it */
    { ai_run, 10, NULL },
    { ai_run, 10, NULL },
    { ai_run, 10, NULL },
    { ai_run, 10, NULL },
    { ai_run, 10, NULL },
    { ai_run, 10, NULL },
    { ai_run, 10, NULL },
    { ai_run, 10, NULL },
    { ai_run, 10, NULL },
    { ai_run, 10, NULL },
    { ai_run, 10, NULL },
    { ai_run, 10, NULL },
    { ai_run, 10, NULL },
    { ai_run, 10, NULL },
    { ai_run, 10, NULL },
    { ai_run, 10, NULL },
    { ai_run, 10, NULL },
    { ai_run, 10, NULL },
    { ai_run, 10, NULL },
    { ai_run, 10, NULL }
};
const mmove_t boss2_move_run = {FRAME_walk1, FRAME_walk20, boss2_frames_run, NULL};

static const mframe_t boss2_frames_attack_pre_mg[] = {
    /* used to be all 1's .. what a slow guy */
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  boss2_attack_mg }
};
const mmove_t boss2_move_attack_pre_mg = {FRAME_attack1, FRAME_attack9, boss2_frames_attack_pre_mg, NULL};

// Loop this
static const mframe_t boss2_frames_attack_mg[] = {
    /* used to be all 1's .. what a slow guy */
    { ai_charge,  2,  Boss2MachineGun },
    { ai_charge,  2,  Boss2MachineGun },
    { ai_charge,  2,  Boss2MachineGun },
    { ai_charge,  2,  Boss2MachineGun },
    { ai_charge,  2,  Boss2MachineGun },
    { ai_charge,  2,  boss2_reattack_mg }
};
const mmove_t boss2_move_attack_mg = {FRAME_attack10, FRAME_attack15, boss2_frames_attack_mg, NULL};

static const mframe_t boss2_frames_attack_post_mg[] = {
    /* used to be all 1's .. what a slow guy */
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL }
};
const mmove_t boss2_move_attack_post_mg = {FRAME_attack16, FRAME_attack19, boss2_frames_attack_post_mg, boss2_run};

static const mframe_t boss2_frames_attack_rocket[] = {
    /* used to be all 1's .. except the Boss2Rocket frame, which was -20(!) */
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_move,    -5, Boss2Rocket },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL },
    { ai_charge,  2,  NULL }
};
const mmove_t boss2_move_attack_rocket = {FRAME_attack20, FRAME_attack40, boss2_frames_attack_rocket, boss2_run};

// baseq2's six tables, which The Reckoning inherits byte-for-byte -- `m_boss2.c`
// is identical between the spine and `port_xatrix`, so the `bq2_` prefix is the
// accurate one and the file's other split tables (`bq2_gunner_*`,
// `bq2_infantry_*`) already use it.  Ground Zero's own versions keep the
// unprefixed names above: it raised walk/run from 8 to 10 and gave the machine
// gun and rocket frames different distances, so both sets have to ship and
// `content_flavour` selects (R-CORE-11).
#define BQ2_BOSS2_WALK_FRAME { ai_walk, 8, NULL }
#define BQ2_BOSS2_RUN_FRAME { ai_run, 8, NULL }
#define BQ2_BOSS2_CHARGE_FRAME { ai_charge, 1, NULL }

static const mframe_t bq2_boss2_frames_walk[] = {
    BQ2_BOSS2_WALK_FRAME, BQ2_BOSS2_WALK_FRAME, BQ2_BOSS2_WALK_FRAME, BQ2_BOSS2_WALK_FRAME, BQ2_BOSS2_WALK_FRAME,
    BQ2_BOSS2_WALK_FRAME, BQ2_BOSS2_WALK_FRAME, BQ2_BOSS2_WALK_FRAME, BQ2_BOSS2_WALK_FRAME, BQ2_BOSS2_WALK_FRAME,
    BQ2_BOSS2_WALK_FRAME, BQ2_BOSS2_WALK_FRAME, BQ2_BOSS2_WALK_FRAME, BQ2_BOSS2_WALK_FRAME, BQ2_BOSS2_WALK_FRAME,
    BQ2_BOSS2_WALK_FRAME, BQ2_BOSS2_WALK_FRAME, BQ2_BOSS2_WALK_FRAME, BQ2_BOSS2_WALK_FRAME, BQ2_BOSS2_WALK_FRAME
};
const mmove_t bq2_boss2_move_walk = {FRAME_walk1, FRAME_walk20, bq2_boss2_frames_walk, NULL};

static const mframe_t bq2_boss2_frames_run[] = {
    BQ2_BOSS2_RUN_FRAME, BQ2_BOSS2_RUN_FRAME, BQ2_BOSS2_RUN_FRAME, BQ2_BOSS2_RUN_FRAME, BQ2_BOSS2_RUN_FRAME,
    BQ2_BOSS2_RUN_FRAME, BQ2_BOSS2_RUN_FRAME, BQ2_BOSS2_RUN_FRAME, BQ2_BOSS2_RUN_FRAME, BQ2_BOSS2_RUN_FRAME,
    BQ2_BOSS2_RUN_FRAME, BQ2_BOSS2_RUN_FRAME, BQ2_BOSS2_RUN_FRAME, BQ2_BOSS2_RUN_FRAME, BQ2_BOSS2_RUN_FRAME,
    BQ2_BOSS2_RUN_FRAME, BQ2_BOSS2_RUN_FRAME, BQ2_BOSS2_RUN_FRAME, BQ2_BOSS2_RUN_FRAME, BQ2_BOSS2_RUN_FRAME
};
const mmove_t bq2_boss2_move_run = {FRAME_walk1, FRAME_walk20, bq2_boss2_frames_run, NULL};

static const mframe_t bq2_boss2_frames_attack_pre_mg[] = {
    BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME,
    BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME,
    BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME, { ai_charge, 1, boss2_attack_mg }
};
const mmove_t bq2_boss2_move_attack_pre_mg = {FRAME_attack1, FRAME_attack9, bq2_boss2_frames_attack_pre_mg, NULL};

static const mframe_t bq2_boss2_frames_attack_mg[] = {
    { ai_charge, 1, Boss2MachineGun }, { ai_charge, 1, Boss2MachineGun }, { ai_charge, 1, Boss2MachineGun },
    { ai_charge, 1, Boss2MachineGun }, { ai_charge, 1, Boss2MachineGun }, { ai_charge, 1, boss2_reattack_mg }
};
const mmove_t bq2_boss2_move_attack_mg = {FRAME_attack10, FRAME_attack15, bq2_boss2_frames_attack_mg, NULL};

static const mframe_t bq2_boss2_frames_attack_post_mg[] = {
    BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME
};
const mmove_t bq2_boss2_move_attack_post_mg = {FRAME_attack16, FRAME_attack19, bq2_boss2_frames_attack_post_mg, boss2_run};

static const mframe_t bq2_boss2_frames_attack_rocket[] = {
    BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME,
    BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME,
    BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME,
    { ai_move, -20, Boss2Rocket },
    BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME,
    BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME, BQ2_BOSS2_CHARGE_FRAME
};
const mmove_t bq2_boss2_move_attack_rocket = {FRAME_attack20, FRAME_attack40, bq2_boss2_frames_attack_rocket, boss2_run};

#undef BQ2_BOSS2_CHARGE_FRAME
#undef BQ2_BOSS2_RUN_FRAME
#undef BQ2_BOSS2_WALK_FRAME

static const mframe_t boss2_frames_pain_heavy[] = {
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL }
};
const mmove_t boss2_move_pain_heavy = {FRAME_pain2, FRAME_pain19, boss2_frames_pain_heavy, boss2_run};

static const mframe_t boss2_frames_pain_light[] = {
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL }
};
const mmove_t boss2_move_pain_light = {FRAME_pain20, FRAME_pain23, boss2_frames_pain_light, boss2_run};

static const mframe_t boss2_frames_death[] = {
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  NULL },
    { ai_move,    0,  BossExplode }
};
const mmove_t boss2_move_death = {FRAME_death2, FRAME_death50, boss2_frames_death, boss2_dead};

void boss2_stand(edict_t *self)
{
    self->monsterinfo.currentmove = &boss2_move_stand;
}

void boss2_run(edict_t *self)
{
    if (self->monsterinfo.aiflags & AI_STAND_GROUND)
        self->monsterinfo.currentmove = &boss2_move_stand;
    else if (self->content_flavour & CONTENT_ROGUE)
        self->monsterinfo.currentmove = &boss2_move_run;
    else
        self->monsterinfo.currentmove = &bq2_boss2_move_run;
}

void boss2_walk(edict_t *self)
{
    if (self->content_flavour & CONTENT_ROGUE)
        self->monsterinfo.currentmove = &boss2_move_walk;
    else
        self->monsterinfo.currentmove = &bq2_boss2_move_walk;
}

void boss2_attack(edict_t *self)
{
    vec3_t  vec;
    float   range;

    VectorSubtract(self->enemy->s.origin, self->s.origin, vec);
    range = VectorLength(vec);

    if (range <= 125) {
        if (self->content_flavour & CONTENT_ROGUE)
            self->monsterinfo.currentmove = &boss2_move_attack_pre_mg;
        else
            self->monsterinfo.currentmove = &bq2_boss2_move_attack_pre_mg;
    } else {
        if (random() <= 0.6f) {
            if (self->content_flavour & CONTENT_ROGUE)
                self->monsterinfo.currentmove = &boss2_move_attack_pre_mg;
            else
                self->monsterinfo.currentmove = &bq2_boss2_move_attack_pre_mg;
        } else {
            if (self->content_flavour & CONTENT_ROGUE)
                self->monsterinfo.currentmove = &boss2_move_attack_rocket;
            else
                self->monsterinfo.currentmove = &bq2_boss2_move_attack_rocket;
        }
    }
}

static void boss2_attack_mg(edict_t *self)
{
    if (self->content_flavour & CONTENT_ROGUE)
        self->monsterinfo.currentmove = &boss2_move_attack_mg;
    else
        self->monsterinfo.currentmove = &bq2_boss2_move_attack_mg;
}

static void boss2_reattack_mg(edict_t *self)
{
    if (infront(self, self->enemy))
        if (random() <= 0.7f) {
            if (self->content_flavour & CONTENT_ROGUE)
                self->monsterinfo.currentmove = &boss2_move_attack_mg;
            else
                self->monsterinfo.currentmove = &bq2_boss2_move_attack_mg;
        } else {
            if (self->content_flavour & CONTENT_ROGUE)
                self->monsterinfo.currentmove = &boss2_move_attack_post_mg;
            else
                self->monsterinfo.currentmove = &bq2_boss2_move_attack_post_mg;
        }
    else if (self->content_flavour & CONTENT_ROGUE)
        self->monsterinfo.currentmove = &boss2_move_attack_post_mg;
    else
        self->monsterinfo.currentmove = &bq2_boss2_move_attack_post_mg;
}

void boss2_pain(edict_t *self, edict_t *other, float kick, int damage)
{
    if (self->health < (self->max_health / 2))
        self->s.skinnum = 1;

    if (level.framenum < self->pain_debounce_framenum)
        return;

    self->pain_debounce_framenum = level.framenum + 3 * BASE_FRAMERATE;
// American wanted these at no attenuation
    if (damage < 10) {
        gi.sound(self, CHAN_VOICE, sound_pain3, 1, ATTN_NONE, 0);
        self->monsterinfo.currentmove = &boss2_move_pain_light;
    } else if (damage < 30) {
        gi.sound(self, CHAN_VOICE, sound_pain1, 1, ATTN_NONE, 0);
        self->monsterinfo.currentmove = &boss2_move_pain_light;
    } else {
        gi.sound(self, CHAN_VOICE, sound_pain2, 1, ATTN_NONE, 0);
        self->monsterinfo.currentmove = &boss2_move_pain_heavy;
    }
}

static void boss2_dead(edict_t *self)
{
    VectorSet(self->mins, -56, -56, 0);
    VectorSet(self->maxs, 56, 56, 80);
    self->movetype = MOVETYPE_TOSS;
    self->svflags |= SVF_DEADMONSTER;
    self->nextthink = 0;
    gi.linkentity(self);
}

void boss2_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    gi.sound(self, CHAN_VOICE, sound_death, 1, ATTN_NONE, 0);
    self->deadflag = DEAD_DEAD;
    self->takedamage = DAMAGE_NO;
    self->count = 0;
    self->monsterinfo.currentmove = &boss2_move_death;
}

bool Boss2_CheckAttack(edict_t *self)
{
    vec3_t  spot1, spot2;
    vec3_t  temp;
    float   chance = 0;
    trace_t tr;
    int         enemy_range;
    float       enemy_yaw;

    if (self->enemy->health > 0) {
        // see if any entities are in the way of the shot
        VectorCopy(self->s.origin, spot1);
        spot1[2] += self->viewheight;
        VectorCopy(self->enemy->s.origin, spot2);
        spot2[2] += self->enemy->viewheight;

        tr = gi.trace(spot1, NULL, NULL, spot2, self, CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_SLIME | CONTENTS_LAVA);

        // do we have a clear shot?
        if (tr.ent != self->enemy &&
            (!(self->content_flavour & CONTENT_ROGUE) ||
             self->enemy->solid != SOLID_NOT || tr.fraction < 1.0f))
            return false;
    }

    enemy_range = range(self, self->enemy);
    VectorSubtract(self->enemy->s.origin, self->s.origin, temp);
    enemy_yaw = vectoyaw(temp);

    self->ideal_yaw = enemy_yaw;

    // melee attack
    if (enemy_range == RANGE_MELEE) {
        if (self->monsterinfo.melee)
            self->monsterinfo.attack_state = AS_MELEE;
        else
            self->monsterinfo.attack_state = AS_MISSILE;
        return true;
    }

// missile attack
    if (!self->monsterinfo.attack)
        return false;

    if (level.time < self->monsterinfo.attack_finished)
        return false;

    if (enemy_range == RANGE_FAR)
        return false;

    if (self->monsterinfo.aiflags & AI_STAND_GROUND) {
        chance = 0.4f;
    } else if (enemy_range == RANGE_MELEE) {
        chance = 0.8f;
    } else if (enemy_range == RANGE_NEAR) {
        chance = 0.8f;
    } else if (enemy_range == RANGE_MID) {
        chance = 0.8f;
    } else {
        return false;
    }

    if ((random() < chance) ||
        ((self->content_flavour & CONTENT_ROGUE) && self->enemy->solid == SOLID_NOT)) {
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

static void boss2_precache(void)
{
    sound_pain1 = gi.soundindex("bosshovr/bhvpain1.wav");
    sound_pain2 = gi.soundindex("bosshovr/bhvpain2.wav");
    sound_pain3 = gi.soundindex("bosshovr/bhvpain3.wav");
    sound_death = gi.soundindex("bosshovr/bhvdeth1.wav");
    sound_search1 = gi.soundindex("bosshovr/bhvunqv1.wav");
}

/*QUAKED monster_boss2 (1 .5 0) (-56 -56 0) (56 56 80) Ambush Trigger_Spawn Sight
*/
void SP_monster_boss2(edict_t *self)
{
    // R-MODE-7 / R-CORE-8: the ruleset decides, not `deathmatch`.  The
    // inherited test was right for baseq2 and wrong here, because `deathmatch`
    // is 1 under ctf and R-MODE-7 promises monsters under ctf.
    if (!G_MonstersAllowed()) {
        G_FreeEdict(self);
        return;
    }

    G_AddPrecache(boss2_precache);

    self->s.sound = gi.soundindex("bosshovr/bhvengn1.wav");

    self->movetype = MOVETYPE_STEP;
    self->solid = SOLID_BBOX;
    self->s.modelindex = gi.modelindex("models/monsters/boss2/tris.md2");
    VectorSet(self->mins, -56, -56, 0);
    VectorSet(self->maxs, 56, 56, 80);

    self->health = 2000;
    self->gib_health = -200;
    self->mass = 1000;

    if (self->content_flavour & CONTENT_ROGUE)
        self->yaw_speed = 50;

    self->flags |= FL_IMMUNE_LASER;

    self->pain = boss2_pain;
    self->die = boss2_die;

    self->monsterinfo.stand = boss2_stand;
    self->monsterinfo.walk = boss2_walk;
    self->monsterinfo.run = boss2_run;
    self->monsterinfo.attack = boss2_attack;
    self->monsterinfo.search = boss2_search;
    self->monsterinfo.checkattack = Boss2_CheckAttack;
    gi.linkentity(self);

    self->monsterinfo.currentmove = &boss2_move_stand;
    self->monsterinfo.scale = MODEL_SCALE;

    flymonster_start(self);
}
