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

SOLDIER

==============================================================================
*/

#include "g_local.h"
#include "m_soldier.h"

//ROGUE
#define RUN_SHOOT       1
#define CHECK_TARGET    1
//ROGUE

static int  sound_idle;
static int  sound_sight1;
static int  sound_sight2;
static int  sound_pain_light;
static int  sound_pain;
static int  sound_pain_ss;
static int  sound_death_light;
static int  sound_death;
static int  sound_death_ss;
static int  sound_cock;

static void bq2_soldier_duck_down(edict_t *self);
static void bq2_soldier_duck_up(edict_t *self);

static void soldier_start_charge(edict_t *self)
{
    self->monsterinfo.aiflags |= AI_CHARGING;
}

void soldier_stop_charge(edict_t *self)
{
    self->monsterinfo.aiflags &= ~AI_CHARGING;
}

static void soldier_idle(edict_t *self)
{
    if (random() > 0.8f)
        gi.sound(self, CHAN_VOICE, sound_idle, 1, ATTN_IDLE, 0);
}

static void soldier_cock(edict_t *self)
{
    if (self->s.frame == FRAME_stand322)
        gi.sound(self, CHAN_WEAPON, sound_cock, 1, ATTN_IDLE, 0);
    else
        gi.sound(self, CHAN_WEAPON, sound_cock, 1, ATTN_NORM, 0);
}

// STAND

void soldier_stand(edict_t *self);

static const mframe_t soldier_frames_stand1[] = {
    { ai_stand, 0, soldier_idle },
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
const mmove_t soldier_move_stand1 = {FRAME_stand101, FRAME_stand130, soldier_frames_stand1, soldier_stand};

static const mframe_t soldier_frames_stand3[] = {
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
    { ai_stand, 0, soldier_cock },
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
const mmove_t soldier_move_stand3 = {FRAME_stand301, FRAME_stand339, soldier_frames_stand3, soldier_stand};

void soldier_stand(edict_t *self)
{
    if ((self->monsterinfo.currentmove == &soldier_move_stand3) || (random() < 0.8f))
        self->monsterinfo.currentmove = &soldier_move_stand1;
    else
        self->monsterinfo.currentmove = &soldier_move_stand3;
}

//
// WALK
//

static void soldier_walk1_random(edict_t *self)
{
    if (random() > 0.1f)
        self->monsterinfo.nextframe = FRAME_walk101;
}

static const mframe_t soldier_frames_walk1[] = {
    { ai_walk, 3,  NULL },
    { ai_walk, 6,  NULL },
    { ai_walk, 2,  NULL },
    { ai_walk, 2,  NULL },
    { ai_walk, 2,  NULL },
    { ai_walk, 1,  NULL },
    { ai_walk, 6,  NULL },
    { ai_walk, 5,  NULL },
    { ai_walk, 3,  NULL },
    { ai_walk, -1, soldier_walk1_random },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL }
};
const mmove_t soldier_move_walk1 = {FRAME_walk101, FRAME_walk133, soldier_frames_walk1, NULL};

static const mframe_t soldier_frames_walk2[] = {
    { ai_walk, 4,  NULL },
    { ai_walk, 4,  NULL },
    { ai_walk, 9,  NULL },
    { ai_walk, 8,  NULL },
    { ai_walk, 5,  NULL },
    { ai_walk, 1,  NULL },
    { ai_walk, 3,  NULL },
    { ai_walk, 7,  NULL },
    { ai_walk, 6,  NULL },
    { ai_walk, 7,  NULL }
};
const mmove_t soldier_move_walk2 = {FRAME_walk209, FRAME_walk218, soldier_frames_walk2, NULL};

void soldier_walk(edict_t *self)
{
    if (random() < 0.5f)
        self->monsterinfo.currentmove = &soldier_move_walk1;
    else
        self->monsterinfo.currentmove = &soldier_move_walk2;
}

//
// RUN
//

void soldier_run(edict_t *self);

static const mframe_t soldier_frames_start_run[] = {
    { ai_run, 7,  NULL },
    { ai_run, 5,  NULL }
};
const mmove_t soldier_move_start_run = {FRAME_run01, FRAME_run02, soldier_frames_start_run, soldier_run};

#ifdef RUN_SHOOT
static void soldier_fire(edict_t *self, int);

void soldier_fire_run(edict_t *self)
{
    if ((self->s.skinnum <= 1) && (self->enemy) && visible(self, self->enemy)) {
        soldier_fire(self, 0);
    }
}
#endif

static const mframe_t soldier_frames_run[] = {
    { ai_run, 10, NULL },
    { ai_run, 11, monster_done_dodge },
    { ai_run, 11, NULL },
    { ai_run, 16, NULL },
    { ai_run, 10, NULL },
    { ai_run, 15, monster_done_dodge }
};

const mmove_t soldier_move_run = {FRAME_run03, FRAME_run08, soldier_frames_run, NULL};

void soldier_run(edict_t *self)
{
    monster_done_dodge(self);

    if (self->monsterinfo.aiflags & AI_STAND_GROUND) {
        self->monsterinfo.currentmove = &soldier_move_stand1;
        return;
    }

    if (self->monsterinfo.currentmove == &soldier_move_walk1 ||
        self->monsterinfo.currentmove == &soldier_move_walk2 ||
        self->monsterinfo.currentmove == &soldier_move_start_run) {
        self->monsterinfo.currentmove = &soldier_move_run;
    } else {
        self->monsterinfo.currentmove = &soldier_move_start_run;
    }
}

//
// PAIN
//

static const mframe_t soldier_frames_pain1[] = {
    { ai_move, -3, NULL },
    { ai_move, 4,  NULL },
    { ai_move, 1,  NULL },
    { ai_move, 1,  NULL },
    { ai_move, 0,  NULL }
};
const mmove_t soldier_move_pain1 = {FRAME_pain101, FRAME_pain105, soldier_frames_pain1, soldier_run};

static const mframe_t soldier_frames_pain2[] = {
    { ai_move, -13, NULL },
    { ai_move, -1,  NULL },
    { ai_move, 2,   NULL },
    { ai_move, 4,   NULL },
    { ai_move, 2,   NULL },
    { ai_move, 3,   NULL },
    { ai_move, 2,   NULL }
};
const mmove_t soldier_move_pain2 = {FRAME_pain201, FRAME_pain207, soldier_frames_pain2, soldier_run};

static const mframe_t soldier_frames_pain3[] = {
    { ai_move, -8, NULL },
    { ai_move, 10, NULL },
    { ai_move, -4, NULL },
    { ai_move, -1, NULL },
    { ai_move, -3, NULL },
    { ai_move, 0,  NULL },
    { ai_move, 3,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 1,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 1,  NULL },
    { ai_move, 2,  NULL },
    { ai_move, 4,  NULL },
    { ai_move, 3,  NULL },
    { ai_move, 2,  NULL }
};
const mmove_t soldier_move_pain3 = {FRAME_pain301, FRAME_pain318, soldier_frames_pain3, soldier_run};

static const mframe_t soldier_frames_pain4[] = {
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, -10, NULL },
    { ai_move, -6,  NULL },
    { ai_move, 8,   NULL },
    { ai_move, 4,   NULL },
    { ai_move, 1,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 2,   NULL },
    { ai_move, 5,   NULL },
    { ai_move, 2,   NULL },
    { ai_move, -1,  NULL },
    { ai_move, -1,  NULL },
    { ai_move, 3,   NULL },
    { ai_move, 2,   NULL },
    { ai_move, 0,   NULL }
};
const mmove_t soldier_move_pain4 = {FRAME_pain401, FRAME_pain417, soldier_frames_pain4, soldier_run};

void soldier_pain(edict_t *self, edict_t *other, float kick, int damage)
{
    float   r;
    int     n;

    if (self->health < (self->max_health / 2))
        self->s.skinnum |= 1;

    if (self->content_flavour & CONTENT_ROGUE) {
        monster_done_dodge(self);
        soldier_stop_charge(self);
        self->monsterinfo.aiflags &= ~AI_MANUAL_STEERING;
    }

    if (level.framenum < self->pain_debounce_framenum) {
        if ((self->velocity[2] > 100) && ((self->monsterinfo.currentmove == &soldier_move_pain1) || (self->monsterinfo.currentmove == &soldier_move_pain2) || (self->monsterinfo.currentmove == &soldier_move_pain3))) {
            if ((self->content_flavour & CONTENT_ROGUE) &&
                (self->monsterinfo.aiflags & AI_DUCKED))
                monster_duck_up(self);
            self->monsterinfo.currentmove = &soldier_move_pain4;
        }
        return;
    }

    self->pain_debounce_framenum = level.framenum + 3 * BASE_FRAMERATE;

    n = self->s.skinnum | 1;
    if (n == 1)
        gi.sound(self, CHAN_VOICE, sound_pain_light, 1, ATTN_NORM, 0);
    else if (n == 3)
        gi.sound(self, CHAN_VOICE, sound_pain, 1, ATTN_NORM, 0);
    else
        gi.sound(self, CHAN_VOICE, sound_pain_ss, 1, ATTN_NORM, 0);

    if (self->velocity[2] > 100) {
        if ((self->content_flavour & CONTENT_ROGUE) &&
            (self->monsterinfo.aiflags & AI_DUCKED))
            monster_duck_up(self);
        self->monsterinfo.currentmove = &soldier_move_pain4;
//      self->monsterinfo.pause_framenum = 0;
        return;
    }

    if (skill->value == 3)
        return;     // no pain anims in nightmare

    r = random();

    if (r < 0.33f)
        self->monsterinfo.currentmove = &soldier_move_pain1;
    else if (r < 0.66f)
        self->monsterinfo.currentmove = &soldier_move_pain2;
    else
        self->monsterinfo.currentmove = &soldier_move_pain3;

    if ((self->content_flavour & CONTENT_ROGUE) &&
        (self->monsterinfo.aiflags & AI_DUCKED))
        monster_duck_up(self);
//  self->monsterinfo.pause_framenum = 0;

}

//
// ATTACK
//

static const byte blaster_flash[] = {
    MZ2_SOLDIER_BLASTER_1,
    MZ2_SOLDIER_BLASTER_2,
    MZ2_SOLDIER_BLASTER_3,
    MZ2_SOLDIER_BLASTER_4,
    MZ2_SOLDIER_BLASTER_5,
    MZ2_SOLDIER_BLASTER_6,
    MZ2_SOLDIER_BLASTER_7,
    MZ2_SOLDIER_BLASTER_8
};

static const byte shotgun_flash[] = {
    MZ2_SOLDIER_SHOTGUN_1,
    MZ2_SOLDIER_SHOTGUN_2,
    MZ2_SOLDIER_SHOTGUN_3,
    MZ2_SOLDIER_SHOTGUN_4,
    MZ2_SOLDIER_SHOTGUN_5,
    MZ2_SOLDIER_SHOTGUN_6,
    MZ2_SOLDIER_SHOTGUN_7,
    MZ2_SOLDIER_SHOTGUN_8
};

static const byte machinegun_flash[] = {
    MZ2_SOLDIER_MACHINEGUN_1,
    MZ2_SOLDIER_MACHINEGUN_2,
    MZ2_SOLDIER_MACHINEGUN_3,
    MZ2_SOLDIER_MACHINEGUN_4,
    MZ2_SOLDIER_MACHINEGUN_5,
    MZ2_SOLDIER_MACHINEGUN_6,
    MZ2_SOLDIER_MACHINEGUN_7,
    MZ2_SOLDIER_MACHINEGUN_8
};

//void soldier_fire (edict_t *self, int flash_number)  PMM
static void soldier_fire(edict_t *self, int in_flash_number)
{
    vec3_t  start;
    vec3_t  forward, right, up;
    vec3_t  aim;
    vec3_t  dir;
    vec3_t  end;
    float   r, u;
    int     flash_index;
    int     flash_number;
#ifdef RUN_SHOOT
    vec3_t  aim_norm;
    float   angle;
#endif
#ifdef CHECK_TARGET
    trace_t tr;
    vec3_t aim_good;
#endif

    if ((!self->enemy) || (!self->enemy->inuse)) {
        self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
        return;
    }

    if ((self->content_flavour & CONTENT_ROGUE) && in_flash_number < 0) {
        flash_number = -1 * in_flash_number;
    } else
        flash_number = in_flash_number;

    if (self->s.skinnum < 2)
        flash_index = blaster_flash[flash_number];
    else if (self->s.skinnum < 4)
        flash_index = shotgun_flash[flash_number];
    else
        flash_index = machinegun_flash[flash_number];

    AngleVectors(self->s.angles, forward, right, NULL);
    G_ProjectSource(self->s.origin, monster_flash_offset[flash_index], forward, right, start);

    if (flash_number == 5 || flash_number == 6) { // he's dead
        VectorCopy(forward, aim);
    } else {
        VectorCopy(self->enemy->s.origin, end);
        end[2] += self->enemy->viewheight;
        VectorSubtract(end, start, aim);
#ifdef CHECK_TARGET
        VectorCopy(end, aim_good);
#endif
#ifdef RUN_SHOOT
        //PMM
        if (in_flash_number < 0) {
            VectorCopy(aim, aim_norm);
            VectorNormalize(aim_norm);
            angle = DotProduct(aim_norm, forward);
            //gi.dprintf ("Dot Product:  %f", DotProduct (aim_norm, forward));
            if (angle < 0.9f) { // ~25 degree angle
//              if(g_showlogic && g_showlogic->value)
//                  gi.dprintf (" not firing due to bad dotprod %f\n", angle);
                return;
            }
//          else
//          {
//              if(g_showlogic && g_showlogic->value)
//                  gi.dprintf (" firing:  dotprod = %f\n", angle);
//          }
        }
        //-PMM
#endif
        vectoangles(aim, dir);
        AngleVectors(dir, forward, right, up);

        if (!(self->content_flavour & CONTENT_ROGUE) || skill->value < 2) {
            r = crandom() * 1000;
            u = crandom() * 500;
        } else {
            r = crandom() * 500;
            u = crandom() * 250;
        }
        VectorMA(start, 8192, forward, end);
        VectorMA(end, r, right, end);
        VectorMA(end, u, up, end);

        VectorSubtract(end, start, aim);
        VectorNormalize(aim);
    }
#ifdef CHECK_TARGET
    if ((self->content_flavour & CONTENT_ROGUE) &&
        !(flash_number == 5 || flash_number == 6)) {
        tr = gi.trace(start, NULL, NULL, aim_good, self, MASK_SHOT);
        if ((tr.ent != self->enemy) && (tr.ent != world)) {
//          if(g_showlogic && g_showlogic->value)
//              gi.dprintf ("infantry shot aborted due to bad target\n");
            return;
        }
    }
#endif
    if (self->s.skinnum <= 1) {
        monster_fire_blaster(self, start, aim, 5, 600, flash_index, EF_BLASTER);
    } else if (self->s.skinnum <= 3) {
        monster_fire_shotgun(self, start, aim, 2, 1, DEFAULT_SHOTGUN_HSPREAD, DEFAULT_SHOTGUN_VSPREAD, DEFAULT_SHOTGUN_COUNT, flash_index);
    } else {
        if (!(self->monsterinfo.aiflags & AI_HOLD_FRAME)) {
            if (self->content_flavour & CONTENT_ROGUE)
                self->wait = level.time + (3 + Q_rand() % 8) * FRAMETIME;
            else
                self->monsterinfo.pause_framenum = level.framenum + (3 + Q_rand() % 8);
        }

        monster_fire_bullet(self, start, aim, 2, 4, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, flash_index);

        if ((self->content_flavour & CONTENT_ROGUE) ?
            level.time >= self->wait :
            level.framenum >= self->monsterinfo.pause_framenum)
            self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
        else
            self->monsterinfo.aiflags |= AI_HOLD_FRAME;
    }
}

// ATTACK1 (blaster/shotgun)

static void soldier_fire1(edict_t *self)
{
    soldier_fire(self, 0);
}

static void soldier_attack1_refire1(edict_t *self)
{
    // PMM - blindfire
    if (self->monsterinfo.aiflags & AI_MANUAL_STEERING) {
        self->monsterinfo.aiflags &= ~AI_MANUAL_STEERING;
        return;
    }
    // pmm

    if (!self->enemy)
        return;

    if (self->s.skinnum > 1)
        return;

    if (self->enemy->health <= 0)
        return;

    if (((skill->value == 3) && (random() < 0.5f)) || (range(self, self->enemy) == RANGE_MELEE))
        self->monsterinfo.nextframe = FRAME_attak102;
    else
        self->monsterinfo.nextframe = FRAME_attak110;
}

static void soldier_attack1_refire2(edict_t *self)
{
    if (!self->enemy)
        return;

    if (self->s.skinnum < 2)
        return;

    if (self->enemy->health <= 0)
        return;

    if (((skill->value == 3) && (random() < 0.5f)) || (range(self, self->enemy) == RANGE_MELEE))
        self->monsterinfo.nextframe = FRAME_attak102;
}

static const mframe_t soldier_frames_attack1[] = {
    { ai_charge, 0,  NULL },
    { ai_charge, 0,  NULL },
    { ai_charge, 0,  soldier_fire1 },
    { ai_charge, 0,  NULL },
    { ai_charge, 0,  NULL },
    { ai_charge, 0,  soldier_attack1_refire1 },
    { ai_charge, 0,  NULL },
    { ai_charge, 0,  soldier_cock },
    { ai_charge, 0,  soldier_attack1_refire2 },
    { ai_charge, 0,  NULL },
    { ai_charge, 0,  NULL },
    { ai_charge, 0,  NULL }
};
const mmove_t soldier_move_attack1 = {FRAME_attak101, FRAME_attak112, soldier_frames_attack1, soldier_run};

// ATTACK2 (blaster/shotgun)

static void soldier_fire2(edict_t *self)
{
    soldier_fire(self, 1);
}

static void soldier_attack2_refire1(edict_t *self)
{
    if (!self->enemy)
        return;

    if (self->s.skinnum > 1)
        return;

    if (self->enemy->health <= 0)
        return;

    if (((skill->value == 3) && (random() < 0.5f)) || (range(self, self->enemy) == RANGE_MELEE))
        self->monsterinfo.nextframe = FRAME_attak204;
    else
        self->monsterinfo.nextframe = FRAME_attak216;
}

static void soldier_attack2_refire2(edict_t *self)
{
    if (!self->enemy)
        return;

    if (self->s.skinnum < 2)
        return;

    if (self->enemy->health <= 0)
        return;

    if (((skill->value == 3) && (random() < 0.5f)) || (range(self, self->enemy) == RANGE_MELEE))
        self->monsterinfo.nextframe = FRAME_attak204;
}

static const mframe_t soldier_frames_attack2[] = {
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, soldier_fire2 },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, soldier_attack2_refire1 },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, soldier_cock },
    { ai_charge, 0, NULL },
    { ai_charge, 0, soldier_attack2_refire2 },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL }
};
const mmove_t soldier_move_attack2 = {FRAME_attak201, FRAME_attak218, soldier_frames_attack2, soldier_run};

// ATTACK3 (duck and shoot)
/*
static void soldier_duck_down (edict_t *self)
{
    if ((g_showlogic) && (g_showlogic->value))
        gi.dprintf ("duck down - %d!\n", self->s.frame);

    self->monsterinfo.aiflags |= AI_DUCKED;
//  self->maxs[2] -= 32;
    self->maxs[2] =  self->monsterinfo.base_height - 32;
    self->takedamage = DAMAGE_YES;
    if (self->monsterinfo.duck_wait_framenum < level.framenum)
    {
        if ((g_showlogic) && (g_showlogic->value))
            gi.dprintf ("soldier duck with no time!\n");
        self->monsterinfo.duck_wait_framenum = level.framenum + 1 * BASE_FRAMERATE;
    }
    gi.linkentity (self);
}

static void soldier_duck_up (edict_t *self)
{
    if ((g_showlogic) && (g_showlogic->value))
        gi.dprintf ("duck up - %d!\n", self->s.frame);
    self->monsterinfo.aiflags &= ~AI_DUCKED;
//  self->maxs[2] += 32;
    self->maxs[2] = self->monsterinfo.base_height;
    self->takedamage = DAMAGE_AIM;
    gi.linkentity (self);
}
*/
static void soldier_fire3(edict_t *self)
{
    if (self->content_flavour & CONTENT_ROGUE)
        monster_duck_down(self);
    else
        bq2_soldier_duck_down(self);
    soldier_fire(self, 2);
}

static void soldier_attack3_refire(edict_t *self)
{
    if ((level.framenum + 0.4f * BASE_FRAMERATE) <
        ((self->content_flavour & CONTENT_ROGUE) ?
         self->monsterinfo.duck_wait_framenum : self->monsterinfo.pause_framenum))
        self->monsterinfo.nextframe = FRAME_attak303;
}

static void soldier_attack3_duck_up(edict_t *self)
{
    if (self->content_flavour & CONTENT_ROGUE)
        monster_duck_up(self);
    else
        bq2_soldier_duck_up(self);
}

static const mframe_t soldier_frames_attack3[] = {
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, soldier_fire3 },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, soldier_attack3_refire },
    { ai_charge, 0, soldier_attack3_duck_up },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL }
};
const mmove_t soldier_move_attack3 = {FRAME_attak301, FRAME_attak309, soldier_frames_attack3, soldier_run};

// ATTACK4 (machinegun)

static void soldier_fire4(edict_t *self)
{
    soldier_fire(self, 3);
}

static const mframe_t soldier_frames_attack4[] = {
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, soldier_fire4 },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL }
};
const mmove_t soldier_move_attack4 = {FRAME_attak401, FRAME_attak406, soldier_frames_attack4, soldier_run};

// ATTACK6 (run & shoot)

static void soldier_fire8(edict_t *self)
{
    soldier_fire(self, (self->content_flavour & CONTENT_ROGUE) ? -7 : 7);
//  self->monsterinfo.aiflags |= AI_HOLD_FRAME;
//  self->monsterinfo.pause_framenum = level.framenum + 1000000 * BASE_FRAMERATE;
}

static void soldier_attack6_refire(edict_t *self)
{
    // PMM - make sure dodge & charge bits are cleared
    if (self->content_flavour & CONTENT_ROGUE) {
        monster_done_dodge(self);
        soldier_stop_charge(self);
    }

    if (!self->enemy)
        return;

    if (self->enemy->health <= 0)
        return;

//  if (range(self, self->enemy) < RANGE_MID)
    if (range(self, self->enemy) <
        ((self->content_flavour & CONTENT_ROGUE) ? RANGE_NEAR : RANGE_MID))
        return;

    if ((self->content_flavour & CONTENT_ROGUE) ?
        ((skill->value == 3) || (random() < (0.25f * skill->value))) :
        skill->value == 3)
        self->monsterinfo.nextframe = FRAME_runs03;
}

static const mframe_t soldier_frames_attack6[] = {
//  PMM
//  ai_run, 10, NULL,
    { ai_run, 10, soldier_start_charge },
    { ai_run,  4, NULL },
    { ai_run, 12, soldier_fire8 },
    { ai_run, 11, NULL },
    { ai_run, 13, monster_done_dodge },
    { ai_run, 18, NULL },
    { ai_run, 15, NULL },
    { ai_run, 14, NULL },
    { ai_run, 11, NULL },
    { ai_run,  8, NULL },
    { ai_run, 11, NULL },
    { ai_run, 12, NULL },
    { ai_run, 12, NULL },
    { ai_run, 17, soldier_attack6_refire }
};
const mmove_t soldier_move_attack6 = {FRAME_runs01, FRAME_runs14, soldier_frames_attack6, soldier_run};

// R-CORE-11: baseq2's soldier_move_attack6, kept alongside Ground Zero's.
// Ground Zero swaps ai_charge for ai_run over part of the run-and-shoot and
// adds soldier_start_charge, so the soldier closes differently.
static const mframe_t bq2_soldier_frames_attack6[] = {
    { ai_charge, 10, NULL },
    { ai_charge,  4, NULL },
    { ai_charge, 12, NULL },
    { ai_charge, 11, soldier_fire8 },
    { ai_charge, 13, NULL },
    { ai_charge, 18, NULL },
    { ai_charge, 15, NULL },
    { ai_charge, 14, NULL },
    { ai_charge, 11, NULL },
    { ai_charge,  8, NULL },
    { ai_charge, 11, NULL },
    { ai_charge, 12, NULL },
    { ai_charge, 12, NULL },
    { ai_charge, 17, soldier_attack6_refire }
};

const mmove_t bq2_soldier_move_attack6 = {FRAME_runs01, FRAME_runs14, bq2_soldier_frames_attack6, soldier_run};

void soldier_attack(edict_t *self)
{
    float r, chance;

    if (!(self->content_flavour & CONTENT_ROGUE)) {
        if (self->s.skinnum < 4) {
            if (random() < 0.5f)
                self->monsterinfo.currentmove = &soldier_move_attack1;
            else
                self->monsterinfo.currentmove = &soldier_move_attack2;
        } else {
            self->monsterinfo.currentmove = &soldier_move_attack4;
        }
        return;
    }

    monster_done_dodge(self);

    // PMM - blindfire!
    if (self->monsterinfo.attack_state == AS_BLIND) {
        // setup shot probabilities
        if (self->monsterinfo.blind_fire_delay < 1.0f)
            chance = 1.0f;
        else if (self->monsterinfo.blind_fire_delay < 7.5f)
            chance = 0.4f;
        else
            chance = 0.1f;

        r = random();

        // minimum of 2 seconds, plus 0-3, after the shots are done
        self->monsterinfo.blind_fire_delay += 2.1f + 2.0f + random() * 3.0f;

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
        self->monsterinfo.currentmove = &soldier_move_attack1;
        self->monsterinfo.attack_finished = level.time + 1.5f + random();
        return;
    }
    // pmm

// PMM - added this so the soldiers now run toward you and shoot instead of just stopping and shooting
//  if ((range(self, self->enemy) >= RANGE_MID) && (r < (skill->value*0.25) && (self->s.skinnum <= 3)))

    r = random();

    if ((!(self->monsterinfo.aiflags & (AI_BLOCKED | AI_STAND_GROUND))) &&
        (range(self, self->enemy) >= RANGE_NEAR) &&
        (r < (skill->value * 0.25f) &&
         (self->s.skinnum <= 3))) {
        // R-CORE-11: both sequences ship; the latch selects (if/else with
        // literal assignments so genptr.py sees both -- R-CORE-11b).
        if (self->content_flavour & CONTENT_ROGUE)
            self->monsterinfo.currentmove = &soldier_move_attack6;
        else
            self->monsterinfo.currentmove = &bq2_soldier_move_attack6;
    } else {
        if (self->s.skinnum < 4) {
            if (random() < 0.5f)
                self->monsterinfo.currentmove = &soldier_move_attack1;
            else
                self->monsterinfo.currentmove = &soldier_move_attack2;
        } else {
            self->monsterinfo.currentmove = &soldier_move_attack4;
        }
    }
}

//
// SIGHT
//

void soldier_sight(edict_t *self, edict_t *other)
{
    if (random() < 0.5f)
        gi.sound(self, CHAN_VOICE, sound_sight1, 1, ATTN_NORM, 0);
    else
        gi.sound(self, CHAN_VOICE, sound_sight2, 1, ATTN_NORM, 0);

    if (!(self->content_flavour & CONTENT_ROGUE)) {
        if (skill->value > 0 && range(self, self->enemy) >= RANGE_MID &&
            random() > 0.5f)
            self->monsterinfo.currentmove = &bq2_soldier_move_attack6;
        return;
    }

    if ((skill->value > 0) && (self->enemy) && (range(self, self->enemy) >= RANGE_NEAR)) {
//  PMM - don't let machinegunners run & shoot
        if ((random() > 0.75f) && (self->s.skinnum <= 3)) {
            // R-CORE-11: both sequences ship; the latch selects (if/else with
            // literal assignments so genptr.py sees both -- R-CORE-11b).
            if (self->content_flavour & CONTENT_ROGUE)
                self->monsterinfo.currentmove = &soldier_move_attack6;
            else
                self->monsterinfo.currentmove = &bq2_soldier_move_attack6;
        }
    }
}

//
// DUCK
//
/*
static void soldier_duck_hold (edict_t *self)
{
    if (level.framenum >= self->monsterinfo.duck_wait_framenum)
        self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
    else
        self->monsterinfo.aiflags |= AI_HOLD_FRAME;
}
*/
static const mframe_t soldier_frames_duck[] = {
    { ai_move, 5, monster_duck_down },
    { ai_move, -1, monster_duck_hold },
    { ai_move, 1,  NULL },
    { ai_move, 0,  monster_duck_up },
    { ai_move, 5,  NULL }
};
const mmove_t soldier_move_duck = {FRAME_duck01, FRAME_duck05, soldier_frames_duck, soldier_run};

/*
void soldier_dodge (edict_t *self, edict_t *attacker, float eta, trace_t *tr)
{
//===========
//PMM - rogue rewrite of dodge code.
// lots o' changes in here.  Basically, they now check the tr and see if ducking would help,
// and if it doesn't, they dodge like mad
    float   r = random();
    float   height;

    if ((g_showlogic) && (g_showlogic->value))
    {
        if (self->monsterinfo.aiflags & AI_DODGING)
            gi.dprintf ("dodging - ");
        if (self->monsterinfo.aiflags & AI_DUCKED)
            gi.dprintf ("ducked - ");
    }
    if (!self->enemy)
    {
        self->enemy = attacker;
        FoundTarget (self);
    }

    // PMM - don't bother if it's going to hit anyway; fix for weird in-your-face etas (I was
    // seeing numbers like 13 and 14)
    if ((eta < 0.1) || (eta > 5))
    {
        if ((g_showlogic) && (g_showlogic->value))
            gi.dprintf ("timeout\n");
        return;
    }

    // skill level determination..
    if (r > (0.25*((skill->value)+1)))
    {
        if ((g_showlogic) && (g_showlogic->value))
            gi.dprintf ("skillout\n");
        return;
    }

    // stop charging, since we're going to dodge (somehow) instead
    soldier_stop_charge (self);

    height = self->absmax[2]-32-1;  // the -1 is because the absmax is s.origin + maxs + 1

    // if we're ducking already, or the shot is at our knees
    if ((tr->endpos[2] <= height) || (self->monsterinfo.aiflags & AI_DUCKED))
    {
        vec3_t right, diff;

        // if we're already dodging, just finish the sequence, i.e. don't do anything else
        if (self->monsterinfo.aiflags & AI_DODGING)
        {
            if ((g_showlogic) && (g_showlogic->value))
                gi.dprintf ("already dodging\n");
            return;
        }

        AngleVectors (self->s.angles, NULL, right, NULL);
        VectorSubtract (tr->endpos, self->s.origin, diff);

        if (DotProduct (right, diff) < 0)
        {
            self->monsterinfo.lefty = 1;
//          gi.dprintf ("left\n");
        } else {
//          gi.dprintf ("right\n");
        }
        // if it doesn't sense to duck, try to strafe and shoot
        // we don't want the machine gun guys running & shooting (looks bad)

        // if we are currently ducked, unduck
        if (self->monsterinfo.aiflags & AI_DUCKED)
        {
            if ((g_showlogic) && (g_showlogic->value))
                gi.dprintf ("unducking - ");
            soldier_duck_up(self);
        }

        self->monsterinfo.aiflags |= AI_DODGING;
        self->monsterinfo.attack_state = AS_SLIDING;

        if (self->s.skinnum <= 3)
        {
            if ((g_showlogic) && (g_showlogic->value))
                gi.dprintf ("shooting back!\n");
            // R-CORE-11: both sequences ship; the latch selects (if/else with
            // literal assignments so genptr.py sees both -- R-CORE-11b).
            if (self->content_flavour & CONTENT_ROGUE)
                self->monsterinfo.currentmove = &soldier_move_attack6;
            else
                self->monsterinfo.currentmove = &bq2_soldier_move_attack6;
        }
        else
        {
            if ((g_showlogic) && (g_showlogic->value))
                gi.dprintf ("strafing away!\n");
            self->monsterinfo.currentmove = &soldier_move_start_run;
        }
        return;
    }

    // if we're here, we're ducking, so clear the dodge bit if it's set

    if ((g_showlogic) && (g_showlogic->value))
        gi.dprintf ("ducking!\n");
    if (skill->value == 0)
    {
        // set this prematurely; it doesn't hurt, and prevents extra iterations
        self->monsterinfo.aiflags |= AI_DUCKED;
        monster_done_dodge (self);
        self->monsterinfo.currentmove = &soldier_move_duck;
        // PMM - stupid dodge
        self->monsterinfo.duck_wait_framenum = level.framenum + (eta + 1) * BASE_FRAMERATE;
        return;
    }
// PMM - since we're only ducking some of the time, this needs to be moved down below
//  self->monsterinfo.duck_wait_framenum = level.framenum + (eta + 0.3) * BASE_FRAMERATE;

    r = random();

    // set this prematurely; it doesn't hurt, and prevents extra iterations
    self->monsterinfo.aiflags |= AI_DUCKED;
    monster_done_dodge (self);

    if (r > (skill->value * 0.33))
    {
        self->monsterinfo.currentmove = &soldier_move_duck;
        self->monsterinfo.duck_wait_framenum = level.framenum + (eta + (0.1 * (3 - skill->value))) * BASE_FRAMERATE;
        // has to be done immediately otherwise he can get stuck
        soldier_duck_down(self);
    }
    else
    {
        // has to be done immediately otherwise he can get stuck
        soldier_duck_down(self);
        self->monsterinfo.duck_wait_framenum = level.framenum + (eta + 1) * BASE_FRAMERATE;
        self->monsterinfo.currentmove = &soldier_move_attack3;
        self->monsterinfo.nextframe = FRAME_attak301;
    }
    return;
//PMM
//===========

}
*/
// pmm - blocking code

bool soldier_blocked(edict_t *self, float dist)
{
    // don't do anything if you're dodging
    if ((self->monsterinfo.aiflags & AI_DODGING) || (self->monsterinfo.aiflags & AI_DUCKED))
        return false;

    if (blocked_checkshot(self, 0.25f + (0.05f * skill->value)))
        return true;

//  if(blocked_checkjump (self, dist, 192, 40))
//  {
//      soldier_jump(self);
//      return true;
//  }

    if (blocked_checkplat(self, dist))
        return true;

    return false;
}

//
// DEATH
//

static void soldier_fire6(edict_t *self)
{
    soldier_fire(self, 5);
}

static void soldier_fire7(edict_t *self)
{
    soldier_fire(self, 6);
}

static void soldier_dead(edict_t *self)
{
    VectorSet(self->mins, -16, -16, -24);
    VectorSet(self->maxs, 16, 16, -8);
    self->movetype = MOVETYPE_TOSS;
    self->svflags |= SVF_DEADMONSTER;
    self->nextthink = 0;
    gi.linkentity(self);
}

// pmm - this quickie does a location trace to try to grow the bounding box
//
// this is because the frames are off; the origin is at the guy's feet.
static void soldier_dead2(edict_t *self)
{
    vec3_t  tempmins, tempmaxs, temporg;
    trace_t tr;

    VectorCopy(self->s.origin, temporg);
    // this is because location traces done at the floor are guaranteed to hit the floor
    // (inside the sv_trace code it grows the bbox by 1 in all directions)
    temporg[2] += 1;

    VectorSet(tempmins, -32, -32, -24);
    VectorSet(tempmaxs, 32, 32, -8);

    tr = gi.trace(temporg, tempmins, tempmaxs, temporg, self, MASK_SOLID);
    if (tr.startsolid || tr.allsolid) {
        VectorSet(self->mins, -16, -16, -24);
        VectorSet(self->maxs, 16, 16, -8);
    } else {
        VectorCopy(tempmins, self->mins);
        VectorCopy(tempmaxs, self->maxs);
    }
    self->movetype = MOVETYPE_TOSS;
    self->svflags |= SVF_DEADMONSTER;
    self->nextthink = 0;
    gi.linkentity(self);
}

static const mframe_t soldier_frames_death1[] = {
    { ai_move, 0,   NULL },
    { ai_move, -10, NULL },
    { ai_move, -10, NULL },
    { ai_move, -10, NULL },
    { ai_move, -5,  NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   soldier_fire6 },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   soldier_fire7 },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL }
};
const mmove_t soldier_move_death1 = {FRAME_death101, FRAME_death136, soldier_frames_death1, soldier_dead};

static const mframe_t soldier_frames_death2[] = {
    { ai_move, -5,  NULL },
    { ai_move, -5,  NULL },
    { ai_move, -5,  NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL }
};
const mmove_t soldier_move_death2 = {FRAME_death201, FRAME_death235, soldier_frames_death2, soldier_dead};

static const mframe_t soldier_frames_death3[] = {
    { ai_move, -5,  NULL },
    { ai_move, -5,  NULL },
    { ai_move, -5,  NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
};
const mmove_t soldier_move_death3 = {FRAME_death301, FRAME_death345, soldier_frames_death3, soldier_dead};

static const mframe_t soldier_frames_death4[] = {
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL }
};
// PMM -changed to soldier_dead2 to get a larger bounding box
const mmove_t soldier_move_death4 = {FRAME_death401, FRAME_death453, soldier_frames_death4, soldier_dead2};

// R-CORE-11: baseq2's soldier_move_death4, kept alongside Ground Zero's.
// Ground Zero's death4 ends in soldier_dead2 rather than soldier_dead.
static const mframe_t bq2_soldier_frames_death4[] = {
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL }
};

const mmove_t bq2_soldier_move_death4 = {FRAME_death401, FRAME_death453, bq2_soldier_frames_death4, soldier_dead};

static const mframe_t soldier_frames_death5[] = {
    { ai_move, -5,  NULL },
    { ai_move, -5,  NULL },
    { ai_move, -5,  NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL }
};
const mmove_t soldier_move_death5 = {FRAME_death501, FRAME_death524, soldier_frames_death5, soldier_dead};

static const mframe_t soldier_frames_death6[] = {
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL }
};
const mmove_t soldier_move_death6 = {FRAME_death601, FRAME_death610, soldier_frames_death6, soldier_dead};

void soldier_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    int     n;

// check for gib
    if (self->health <= self->gib_health) {
        gi.sound(self, CHAN_VOICE, gi.soundindex("misc/udeath.wav"), 1, ATTN_NORM, 0);
        for (n = 0; n < 3; n++)
            ThrowGib(self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
        ThrowGib(self, "models/objects/gibs/chest/tris.md2", damage, GIB_ORGANIC);
        ThrowHead(self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);
        self->deadflag = DEAD_DEAD;
        return;
    }

    if (self->deadflag == DEAD_DEAD)
        return;

// regular death
    self->deadflag = DEAD_DEAD;
    self->takedamage = DAMAGE_YES;
    self->s.skinnum |= 1;

    if (self->s.skinnum == 1)
        gi.sound(self, CHAN_VOICE, sound_death_light, 1, ATTN_NORM, 0);
    else if (self->s.skinnum == 3)
        gi.sound(self, CHAN_VOICE, sound_death, 1, ATTN_NORM, 0);
    else // (self->s.skinnum == 5)
        gi.sound(self, CHAN_VOICE, sound_death_ss, 1, ATTN_NORM, 0);

    if (fabsf((self->s.origin[2] + self->viewheight) - point[2]) <= 4) {
        // head shot
        self->monsterinfo.currentmove = &soldier_move_death3;
        return;
    }

    n = Q_rand() % 5;
    if (n == 0)
        self->monsterinfo.currentmove = &soldier_move_death1;
    else if (n == 1)
        self->monsterinfo.currentmove = &soldier_move_death2;
    else if (n == 2)
        // R-CORE-11: both sequences ship; the latch selects (if/else with
        // literal assignments so genptr.py sees both -- R-CORE-11b).
        if (self->content_flavour & CONTENT_ROGUE)
            self->monsterinfo.currentmove = &soldier_move_death4;
        else
            self->monsterinfo.currentmove = &bq2_soldier_move_death4;
    else if (n == 3)
        self->monsterinfo.currentmove = &soldier_move_death5;
    else
        self->monsterinfo.currentmove = &soldier_move_death6;
}

//
// NEW DODGE CODE
//

void soldier_sidestep(edict_t *self)
{
    if (self->s.skinnum <= 3) {
//      if ((g_showlogic) && (g_showlogic->value))
//          gi.dprintf ("shooting back!\n");
        if (self->monsterinfo.currentmove != &soldier_move_attack6) {
            // R-CORE-11: both sequences ship; the latch selects (if/else with
            // literal assignments so genptr.py sees both -- R-CORE-11b).
            if (self->content_flavour & CONTENT_ROGUE)
                self->monsterinfo.currentmove = &soldier_move_attack6;
            else
                self->monsterinfo.currentmove = &bq2_soldier_move_attack6;
        }
    } else {
//      if ((g_showlogic) && (g_showlogic->value))
//          gi.dprintf ("strafing away!\n");
        if (self->monsterinfo.currentmove != &soldier_move_start_run)
            self->monsterinfo.currentmove = &soldier_move_start_run;
    }
}

void soldier_duck(edict_t *self, float eta)
{
    float r;

    // has to be done immediately otherwise he can get stuck
    monster_duck_down(self);

    if (skill->value == 0) {
        // PMM - stupid dodge
        self->monsterinfo.nextframe = FRAME_duck01;
        self->monsterinfo.currentmove = &soldier_move_duck;
        self->monsterinfo.duck_wait_framenum = level.framenum + (eta + 1) * BASE_FRAMERATE;
        return;
    }

    r = random();

    if (r > (skill->value * 0.3f)) {
        self->monsterinfo.nextframe = FRAME_duck01;
        self->monsterinfo.currentmove = &soldier_move_duck;
        self->monsterinfo.duck_wait_framenum = level.framenum + (eta + (0.1f * (3 - skill->value))) * BASE_FRAMERATE;
    } else {
        self->monsterinfo.nextframe = FRAME_attak301;
        self->monsterinfo.currentmove = &soldier_move_attack3;
        self->monsterinfo.duck_wait_framenum = level.framenum + (eta + 1) * BASE_FRAMERATE;
    }
    return;
}

//=========
//ROGUE
void soldier_blind(edict_t *self);

static const mframe_t soldier_frames_blind[] = {
    { ai_move, 0, soldier_idle },
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
const mmove_t soldier_move_blind = {FRAME_stand101, FRAME_stand130, soldier_frames_blind, soldier_blind};

void soldier_blind(edict_t *self)
{
    self->monsterinfo.currentmove = &soldier_move_blind;
}
//ROGUE
//=========

//
// SPAWN
//

static void soldier_precache_x(void)
{
    sound_idle   = gi.soundindex("soldier/solidle1.wav");
    sound_sight1 = gi.soundindex("soldier/solsght1.wav");
    sound_sight2 = gi.soundindex("soldier/solsrch1.wav");
    sound_cock   = gi.soundindex("infantry/infatck3.wav");
}

// Forward declaration: the restored baseq2 evasion (R-CORE-11) is defined below,
// after the shared spawn helper whose gate references it.
void bq2_soldier_dodge(edict_t *self, edict_t *attacker, float eta, trace_t *tr);

static void SP_monster_soldier_x(edict_t *self)
{
    self->s.modelindex = gi.modelindex("models/monsters/soldier/tris.md2");
    //PMM
//  self->s.effects |= EF_SPLATTER;
    //PMM
    self->monsterinfo.scale = MODEL_SCALE;
    VectorSet(self->mins, -16, -16, -24);
    VectorSet(self->maxs, 16, 16, 32);
    self->movetype = MOVETYPE_STEP;
    self->solid = SOLID_BBOX;

    G_AddPrecache(soldier_precache_x);

    self->mass = 100;

    self->pain = soldier_pain;
    self->die = soldier_die;

    self->monsterinfo.stand = soldier_stand;
    self->monsterinfo.walk = soldier_walk;
    self->monsterinfo.run = soldier_run;
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
    } else {
        self->monsterinfo.dodge = bq2_soldier_dodge;
    }
    self->monsterinfo.attack = soldier_attack;
    self->monsterinfo.melee = NULL;
    self->monsterinfo.sight = soldier_sight;

    if (self->content_flavour & CONTENT_ROGUE) {
        self->monsterinfo.blocked = soldier_blocked;
        self->monsterinfo.duck = soldier_duck;
        self->monsterinfo.unduck = monster_duck_up;
        self->monsterinfo.sidestep = soldier_sidestep;

        if (self->spawnflags & 8)
            self->monsterinfo.stand = soldier_blind;
    }

    gi.linkentity(self);

    self->monsterinfo.stand(self);

    walkmonster_start(self);
}

static void soldier_precache_light(void)
{
    sound_pain_light = gi.soundindex("soldier/solpain2.wav");
    sound_death_light = gi.soundindex("soldier/soldeth2.wav");
}

/*QUAKED monster_soldier_light (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight Blind

Blind - monster will just stand there until triggered
*/
void SP_monster_soldier_light(edict_t *self)
{
    // R-MODE-7 / R-CORE-8: the ruleset decides, not `deathmatch`.  The
    // inherited test was right for baseq2 and wrong here, because `deathmatch`
    // is 1 under ctf and R-MODE-7 promises monsters under ctf.
    if (!G_MonstersAllowed()) {
        G_FreeEdict(self);
        return;
    }

    SP_monster_soldier_x(self);

    G_AddPrecache(soldier_precache_light);

    gi.modelindex("models/objects/laser/tris.md2");
    gi.soundindex("misc/lasfly.wav");
    gi.soundindex("soldier/solatck2.wav");

    self->s.skinnum = 0;
    self->max_health = self->health = 20;
    self->gib_health = -30;

    if (self->content_flavour & CONTENT_ROGUE)
        self->monsterinfo.blindfire = true;
}

static void soldier_precache(void)
{
    sound_pain = gi.soundindex("soldier/solpain1.wav");
    sound_death = gi.soundindex("soldier/soldeth1.wav");
}


// ---------------------------------------------------------------------------
// R-CORE-11: baseq2's duck-and-dodge for the soldier, restored alongside Ground
// Zero's rewrite so that BOTH ship and the spawn-time latch selects.
//
// Ground Zero does not add to this monster, it replaces its evasion: baseq2's
// per-monster soldier_dodge and soldier_duck_* become the shared M_MonsterDodge with
// generic monster_duck_* plus a sidestep.  Without this block, `rogue 0` still
// got Ground Zero's AI.
//
// Reintroduced under a bq2_ prefix rather than by un-commenting the donor's dead
// copy, so the two sets are distinct symbols and both are visible to genptr.py
// -- the save_ptrs[] table is built by scanning for literal
// `currentmove = &name` assignments, which is also why the gate below is an
// if/else with two literal assignments rather than a ternary or a macro.
// ---------------------------------------------------------------------------

static void bq2_soldier_duck_down(edict_t *self)
{
    if (self->monsterinfo.aiflags & AI_DUCKED)
        return;
    self->monsterinfo.aiflags |= AI_DUCKED;
    self->maxs[2] -= 32;
    self->takedamage = DAMAGE_YES;
    self->monsterinfo.pause_framenum = level.framenum + 1 * BASE_FRAMERATE;
    gi.linkentity(self);
}

static void bq2_soldier_duck_hold(edict_t *self)
{
    if (level.framenum >= self->monsterinfo.pause_framenum)
        self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
    else
        self->monsterinfo.aiflags |= AI_HOLD_FRAME;
}

static void bq2_soldier_duck_up(edict_t *self)
{
    self->monsterinfo.aiflags &= ~AI_DUCKED;
    self->maxs[2] += 32;
    self->takedamage = DAMAGE_AIM;
    gi.linkentity(self);
}

static const mframe_t bq2_soldier_frames_duck[] = {
    { ai_move, 5, bq2_soldier_duck_down },
    { ai_move, -1, bq2_soldier_duck_hold },
    { ai_move, 1,  NULL },
    { ai_move, 0,  bq2_soldier_duck_up },
    { ai_move, 5,  NULL }
};

const mmove_t bq2_soldier_move_duck = {FRAME_duck01, FRAME_duck05, bq2_soldier_frames_duck, soldier_run};

void bq2_soldier_dodge(edict_t *self, edict_t *attacker, float eta, trace_t *tr)
{
    float   r;

    r = random();
    if (r > 0.25f)
        return;

    if (!self->enemy)
        self->enemy = attacker;

    if (skill->value == 0) {
        self->monsterinfo.currentmove = &bq2_soldier_move_duck;
        return;
    }

    self->monsterinfo.pause_framenum = level.framenum + (eta + 0.3f) * BASE_FRAMERATE;
    r = random();

    if (skill->value == 1) {
        if (r > 0.33f)
            self->monsterinfo.currentmove = &bq2_soldier_move_duck;
        else
            self->monsterinfo.currentmove = &soldier_move_attack3;
        return;
    }

    if (skill->value >= 2) {
        if (r > 0.66f)
            self->monsterinfo.currentmove = &bq2_soldier_move_duck;
        else
            self->monsterinfo.currentmove = &soldier_move_attack3;
        return;
    }

    self->monsterinfo.currentmove = &soldier_move_attack3;
}

/*QUAKED monster_soldier (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight Blind

Blind - monster will just stand there until triggered
*/
void SP_monster_soldier(edict_t *self)
{
    // R-MODE-7 / R-CORE-8: the ruleset decides, not `deathmatch`.  The
    // inherited test was right for baseq2 and wrong here, because `deathmatch`
    // is 1 under ctf and R-MODE-7 promises monsters under ctf.
    if (!G_MonstersAllowed()) {
        G_FreeEdict(self);
        return;
    }

    SP_monster_soldier_x(self);

    G_AddPrecache(soldier_precache);

    gi.soundindex("soldier/solatck1.wav");

    self->s.skinnum = 2;
    self->max_health = self->health = 30;
    self->gib_health = -30;
}

static void soldier_precache_ss(void)
{
    sound_pain_ss = gi.soundindex("soldier/solpain3.wav");
    sound_death_ss = gi.soundindex("soldier/soldeth3.wav");
}

/*QUAKED monster_soldier_ss (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight Blind

Blind - monster will just stand there until triggered
*/
void SP_monster_soldier_ss(edict_t *self)
{
    // R-MODE-7 / R-CORE-8: the ruleset decides, not `deathmatch`.  The
    // inherited test was right for baseq2 and wrong here, because `deathmatch`
    // is 1 under ctf and R-MODE-7 promises monsters under ctf.
    if (!G_MonstersAllowed()) {
        G_FreeEdict(self);
        return;
    }

    SP_monster_soldier_x(self);

    G_AddPrecache(soldier_precache_ss);

    gi.soundindex("soldier/solatck3.wav");

    self->s.skinnum = 4;
    self->max_health = self->health = 40;
    self->gib_health = -30;
}

// RAFAEL 13-APR-98

#include "xatrix/m_soldierh.h"

void soldierh_idle(edict_t *self)
{
    if (random() > 0.8)
        gi.sound(self, CHAN_VOICE, sound_idle, 1, ATTN_IDLE, 0);
}

void soldierh_cock(edict_t *self)
{
    if (self->s.frame == FRAME_stand322)
        gi.sound(self, CHAN_WEAPON, sound_cock, 1, ATTN_IDLE, 0);
    else
        gi.sound(self, CHAN_WEAPON, sound_cock, 1, ATTN_NORM, 0);
}

// STAND

void soldierh_stand(edict_t *self);

static const mframe_t soldierh_frames_stand1[] = {
    { ai_stand, 0, soldierh_idle },
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
const mmove_t soldierh_move_stand1 = {FRAME_stand101, FRAME_stand130, soldierh_frames_stand1, soldierh_stand};

static const mframe_t soldierh_frames_stand3[] = {
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
    { ai_stand, 0, soldierh_cock },
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
const mmove_t soldierh_move_stand3 = {FRAME_stand301, FRAME_stand339, soldierh_frames_stand3, soldierh_stand};

void soldierh_stand(edict_t *self)
{
    if ((self->monsterinfo.currentmove == &soldierh_move_stand3) || (random() < 0.8))
        self->monsterinfo.currentmove = &soldierh_move_stand1;
    else
        self->monsterinfo.currentmove = &soldierh_move_stand3;
}

//
// WALK
//

void soldierh_walk1_random(edict_t *self)
{
    if (random() > 0.1)
        self->monsterinfo.nextframe = FRAME_walk101;
}

static const mframe_t soldierh_frames_walk1[] = {
    { ai_walk, 3,  NULL },
    { ai_walk, 6,  NULL },
    { ai_walk, 2,  NULL },
    { ai_walk, 2,  NULL },
    { ai_walk, 2,  NULL },
    { ai_walk, 1,  NULL },
    { ai_walk, 6,  NULL },
    { ai_walk, 5,  NULL },
    { ai_walk, 3,  NULL },
    { ai_walk, -1, soldierh_walk1_random },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL },
    { ai_walk, 0,  NULL }
};
const mmove_t soldierh_move_walk1 = {FRAME_walk101, FRAME_walk133, soldierh_frames_walk1, NULL};

static const mframe_t soldierh_frames_walk2[] = {
    { ai_walk, 4,  NULL },
    { ai_walk, 4,  NULL },
    { ai_walk, 9,  NULL },
    { ai_walk, 8,  NULL },
    { ai_walk, 5,  NULL },
    { ai_walk, 1,  NULL },
    { ai_walk, 3,  NULL },
    { ai_walk, 7,  NULL },
    { ai_walk, 6,  NULL },
    { ai_walk, 7,  NULL }
};
const mmove_t soldierh_move_walk2 = {FRAME_walk209, FRAME_walk218, soldierh_frames_walk2, NULL};

void soldierh_walk(edict_t *self)
{
    if (random() < 0.5)
        self->monsterinfo.currentmove = &soldierh_move_walk1;
    else
        self->monsterinfo.currentmove = &soldierh_move_walk2;
}

//
// RUN
//

void soldierh_run(edict_t *self);

static const mframe_t soldierh_frames_start_run[] = {
    { ai_run, 7,  NULL },
    { ai_run, 5,  NULL }
};
const mmove_t soldierh_move_start_run = {FRAME_run01, FRAME_run02, soldierh_frames_start_run, soldierh_run};

static const mframe_t soldierh_frames_run[] = {
    { ai_run, 10, NULL },
    { ai_run, 11, NULL },
    { ai_run, 11, NULL },
    { ai_run, 16, NULL },
    { ai_run, 10, NULL },
    { ai_run, 15, NULL }
};
const mmove_t soldierh_move_run = {FRAME_run03, FRAME_run08, soldierh_frames_run, NULL};

void soldierh_run(edict_t *self)
{
    if (self->monsterinfo.aiflags & AI_STAND_GROUND) {
        self->monsterinfo.currentmove = &soldierh_move_stand1;
        return;
    }

    if (self->monsterinfo.currentmove == &soldierh_move_walk1 ||
        self->monsterinfo.currentmove == &soldierh_move_walk2 ||
        self->monsterinfo.currentmove == &soldierh_move_start_run) {
        self->monsterinfo.currentmove = &soldierh_move_run;
    } else {
        self->monsterinfo.currentmove = &soldierh_move_start_run;
    }
}

//
// PAIN
//

static const mframe_t soldierh_frames_pain1[] = {
    { ai_move, -3, NULL },
    { ai_move, 4,  NULL },
    { ai_move, 1,  NULL },
    { ai_move, 1,  NULL },
    { ai_move, 0,  NULL }
};
const mmove_t soldierh_move_pain1 = {FRAME_pain101, FRAME_pain105, soldierh_frames_pain1, soldierh_run};

static const mframe_t soldierh_frames_pain2[] = {
    { ai_move, -13, NULL },
    { ai_move, -1,  NULL },
    { ai_move, 2,   NULL },
    { ai_move, 4,   NULL },
    { ai_move, 2,   NULL },
    { ai_move, 3,   NULL },
    { ai_move, 2,   NULL }
};
const mmove_t soldierh_move_pain2 = {FRAME_pain201, FRAME_pain207, soldierh_frames_pain2, soldierh_run};

static const mframe_t soldierh_frames_pain3[] = {
    { ai_move, -8, NULL },
    { ai_move, 10, NULL },
    { ai_move, -4, NULL },
    { ai_move, -1, NULL },
    { ai_move, -3, NULL },
    { ai_move, 0,  NULL },
    { ai_move, 3,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 1,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 1,  NULL },
    { ai_move, 2,  NULL },
    { ai_move, 4,  NULL },
    { ai_move, 3,  NULL },
    { ai_move, 2,  NULL }
};
const mmove_t soldierh_move_pain3 = {FRAME_pain301, FRAME_pain318, soldierh_frames_pain3, soldierh_run};

static const mframe_t soldierh_frames_pain4[] = {
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, -10, NULL },
    { ai_move, -6,  NULL },
    { ai_move, 8,   NULL },
    { ai_move, 4,   NULL },
    { ai_move, 1,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 2,   NULL },
    { ai_move, 5,   NULL },
    { ai_move, 2,   NULL },
    { ai_move, -1,  NULL },
    { ai_move, -1,  NULL },
    { ai_move, 3,   NULL },
    { ai_move, 2,   NULL },
    { ai_move, 0,   NULL }
};
const mmove_t soldierh_move_pain4 = {FRAME_pain401, FRAME_pain417, soldierh_frames_pain4, soldierh_run};

void soldierh_pain(edict_t *self, edict_t *other, float kick, int damage)
{
    float   r;
    int     n;

    if (self->health < (self->max_health / 2))
        self->s.skinnum |= 1;

    if (level.framenum < self->pain_debounce_framenum) {
        if ((self->velocity[2] > 100) && ((self->monsterinfo.currentmove == &soldierh_move_pain1) || (self->monsterinfo.currentmove == &soldierh_move_pain2) || (self->monsterinfo.currentmove == &soldierh_move_pain3)))
            self->monsterinfo.currentmove = &soldierh_move_pain4;
        return;
    }

    self->pain_debounce_framenum = level.framenum + 3 * BASE_FRAMERATE;

    n = self->s.skinnum | 1;
    if (n == 1)
        gi.sound(self, CHAN_VOICE, sound_pain_light, 1, ATTN_NORM, 0);
    else if (n == 3)
        gi.sound(self, CHAN_VOICE, sound_pain, 1, ATTN_NORM, 0);
    else
        gi.sound(self, CHAN_VOICE, sound_pain_ss, 1, ATTN_NORM, 0);

    if (self->velocity[2] > 100) {
        self->monsterinfo.currentmove = &soldierh_move_pain4;
        return;
    }

    if (skill->value == 3)
        return;     // no pain anims in nightmare

    r = random();

    if (r < 0.33)
        self->monsterinfo.currentmove = &soldierh_move_pain1;
    else if (r < 0.66)
        self->monsterinfo.currentmove = &soldierh_move_pain2;
    else
        self->monsterinfo.currentmove = &soldierh_move_pain3;
}

//
// ATTACK
//


void soldierh_laserbeam(edict_t *self, int flash_index)
{

    vec3_t forward, right, up;
    vec3_t tempang, start;
    vec3_t  dir, angles, end;
    vec3_t  tempvec;
    edict_t *ent;

    // RAFAEL
    // this sound can't be called this frequent
    if (random() > 0.8)
        gi.sound(self, CHAN_AUTO, gi.soundindex("misc/lasfly.wav"), 1, ATTN_STATIC, 0);

    VectorCopy(self->s.origin, start);
    VectorCopy(self->enemy->s.origin, end);
    VectorSubtract(end, start, dir);
    vectoangles(dir, angles);
    VectorCopy(monster_flash_offset[flash_index], tempvec);

    ent = G_Spawn();
    VectorCopy(self->s.origin, ent->s.origin);
    VectorCopy(angles, tempang);
    AngleVectors(tempang, forward, right, up);
    VectorCopy(tempang, ent->s.angles);
    VectorCopy(ent->s.origin, start);

    if (flash_index == 85) {
        VectorMA(start, tempvec[0] - 14, right, start);
        VectorMA(start, tempvec[2] + 8, up, start);
        VectorMA(start, tempvec[1], forward, start);
    } else {
        VectorMA(start, tempvec[0] + 2, right, start);
        VectorMA(start, tempvec[2] + 8, up, start);
        VectorMA(start, tempvec[1], forward, start);
    }

    VectorCopy(start, ent->s.origin);
    ent->enemy = self->enemy;
    ent->owner = self;

    ent->dmg = 1;

    monster_dabeam(ent);

}

void soldierh_fire(edict_t *self, int flash_number)
{
    vec3_t  start;
    vec3_t  forward, right, up;
    vec3_t  aim;
    vec3_t  dir;
    vec3_t  end;
    float   r, u;
    int     flash_index;

    if (self->s.skinnum < 2)
        flash_index = blaster_flash[flash_number]; // ripper
    else if (self->s.skinnum < 4)
        flash_index = blaster_flash[flash_number]; // hyperblaster
    else
        flash_index = machinegun_flash[flash_number]; // laserbeam

    AngleVectors(self->s.angles, forward, right, NULL);
    G_ProjectSource(self->s.origin, monster_flash_offset[flash_index], forward, right, start);

    if (flash_number == 5 || flash_number == 6) {
        VectorCopy(forward, aim);
    } else {
        VectorCopy(self->enemy->s.origin, end);
        end[2] += self->enemy->viewheight;
        VectorSubtract(end, start, aim);
        vectoangles(aim, dir);
        AngleVectors(dir, forward, right, up);

        r = crandom() * 100;
        u = crandom() * 50;
        VectorMA(start, 8192, forward, end);
        VectorMA(end, r, right, end);
        VectorMA(end, u, up, end);

        VectorSubtract(end, start, aim);
        VectorNormalize(aim);
    }

    if (self->s.skinnum <= 1) {
        // RAFAEL 24-APR-98
        // droped the damage from 15 to 5
        monster_fire_ionripper(self, start, aim, 5, 600, flash_index, EF_IONRIPPER);

    } else if (self->s.skinnum <= 3) {

        monster_fire_blueblaster(self, start, aim, 1, 600, MZ_BLUEHYPERBLASTER, EF_BLUEHYPERBLASTER);

    } else {
        if (!(self->monsterinfo.aiflags & AI_HOLD_FRAME))
            self->monsterinfo.pause_framenum = level.framenum + (3 + Q_rand() % 8);

        soldierh_laserbeam(self, flash_index);

        if (level.framenum >= self->monsterinfo.pause_framenum)
            self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
        else
            self->monsterinfo.aiflags |= AI_HOLD_FRAME;
    }
}

// ATTACK1 (blaster/shotgun)

void soldierh_hyper_refire1(edict_t *self)
{
    if (self->s.skinnum < 2)
        return;
    else if (self->s.skinnum < 4) {
        if (random() < 0.7)
            self->s.frame = FRAME_attak103;
        else
            gi.sound(self, CHAN_AUTO, gi.soundindex("weapons/hyprbd1a.wav"), 1, ATTN_NORM, 0);
    }
}

void soldierh_ripper1(edict_t *self)
{
    if (self->s.skinnum < 2)
        soldierh_fire(self, 0);
    else if (self->s.skinnum < 4)
        soldierh_fire(self, 0);
}

void soldierh_fire1(edict_t *self)
{
    soldierh_fire(self, 0);
}

void soldierh_attack1_refire1(edict_t *self)
{
    if (self->s.skinnum > 1)
        return;

    if (self->enemy->health <= 0)
        return;

    if (((skill->value == 3) && (random() < 0.5)) || (range(self, self->enemy) == RANGE_MELEE))
        self->monsterinfo.nextframe = FRAME_attak102;
    else
        self->monsterinfo.nextframe = FRAME_attak110;
}

void soldierh_attack1_refire2(edict_t *self)
{
    if (self->s.skinnum < 2)
        return;

    if (self->enemy->health <= 0)
        return;

    if (((skill->value == 3) && (random() < 0.5)) || (range(self, self->enemy) == RANGE_MELEE))
        self->monsterinfo.nextframe = FRAME_attak102;
}

void soldierh_hyper_sound(edict_t *self)
{
    if (self->s.skinnum < 2)
        return;
    else if (self->s.skinnum < 4)
        gi.sound(self, CHAN_AUTO, gi.soundindex("weapons/hyprbl1a.wav"), 1, ATTN_NORM, 0);
    else
        return;
}

static const mframe_t soldierh_frames_attack1[] = {
    { ai_charge, 0,  NULL },
    { ai_charge, 0,  soldierh_hyper_sound },
    { ai_charge, 0,  soldierh_fire1 },
    { ai_charge, 0,  soldierh_ripper1 },
    { ai_charge, 0,  soldierh_ripper1 },
    { ai_charge, 0,  soldierh_attack1_refire1 },
    { ai_charge, 0,  soldierh_hyper_refire1 },
    { ai_charge, 0,  soldierh_cock },
    { ai_charge, 0,  soldierh_attack1_refire2 },
    { ai_charge, 0,  NULL },
    { ai_charge, 0,  NULL },
    { ai_charge, 0,  NULL }
};
const mmove_t soldierh_move_attack1 = {FRAME_attak101, FRAME_attak112, soldierh_frames_attack1, soldierh_run};

// ATTACK2 (blaster/shotgun)

void soldierh_hyper_refire2(edict_t *self)
{
    if (self->s.skinnum < 2)
        return;
    else if (self->s.skinnum < 4) {
        if (random() < 0.7)
            self->s.frame = FRAME_attak205;
        else
            gi.sound(self, CHAN_AUTO, gi.soundindex("weapons/hyprbd1a.wav"), 1, ATTN_NORM, 0);
    }
}

void soldierh_ripper2(edict_t *self)
{
    if (self->s.skinnum < 2)
        soldierh_fire(self, 1);
    else if (self->s.skinnum < 4)
        soldierh_fire(self, 1);
}

void soldierh_fire2(edict_t *self)
{
    soldierh_fire(self, 1);
}

void soldierh_attack2_refire1(edict_t *self)
{
    if (self->s.skinnum > 1)
        return;

    if (self->enemy->health <= 0)
        return;

    if (((skill->value == 3) && (random() < 0.5)) || (range(self, self->enemy) == RANGE_MELEE))
        self->monsterinfo.nextframe = FRAME_attak204;
    else
        self->monsterinfo.nextframe = FRAME_attak216;
}

void soldierh_attack2_refire2(edict_t *self)
{
    if (self->s.skinnum < 2)
        return;

    if (self->enemy->health <= 0)
        return;

    if (((skill->value == 3) && (random() < 0.5)) || ((range(self, self->enemy) == RANGE_MELEE) && self->s.skinnum < 4))
        self->monsterinfo.nextframe = FRAME_attak204;
}

static const mframe_t soldierh_frames_attack2[] = {
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, soldierh_hyper_sound },
    { ai_charge, 0, soldierh_fire2 },
    { ai_charge, 0, soldierh_ripper2 },
    { ai_charge, 0, soldierh_ripper2 },
    { ai_charge, 0, soldierh_attack2_refire1 },
    { ai_charge, 0, soldierh_hyper_refire2 },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, soldierh_cock },
    { ai_charge, 0, NULL },
    { ai_charge, 0, soldierh_attack2_refire2 },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL }
};
const mmove_t soldierh_move_attack2 = {FRAME_attak201, FRAME_attak218, soldierh_frames_attack2, soldierh_run};

// ATTACK3 (duck and shoot)

void soldierh_duck_down(edict_t *self)
{
    if (self->monsterinfo.aiflags & AI_DUCKED)
        return;
    self->monsterinfo.aiflags |= AI_DUCKED;
    self->maxs[2] -= 32;
    self->takedamage = DAMAGE_YES;
    self->monsterinfo.pause_framenum = level.framenum + 1 * BASE_FRAMERATE;
    gi.linkentity(self);
}

void soldierh_duck_up(edict_t *self)
{
    self->monsterinfo.aiflags &= ~AI_DUCKED;
    self->maxs[2] += 32;
    self->takedamage = DAMAGE_AIM;
    gi.linkentity(self);
}

void soldierh_fire3(edict_t *self)
{
    soldierh_duck_down(self);
    soldierh_fire(self, 2);
}

void soldierh_attack3_refire(edict_t *self)
{
    if ((level.framenum + 0.4 * BASE_FRAMERATE) < self->monsterinfo.pause_framenum)
        self->monsterinfo.nextframe = FRAME_attak303;
}

static const mframe_t soldierh_frames_attack3[] = {
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, soldierh_fire3 },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, soldierh_attack3_refire },
    { ai_charge, 0, soldierh_duck_up },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL }
};
const mmove_t soldierh_move_attack3 = {FRAME_attak301, FRAME_attak309, soldierh_frames_attack3, soldierh_run};

// ATTACK4 (machinegun)

void soldierh_fire4(edict_t *self)
{
    soldierh_fire(self, 3);
//
//  if (self->enemy->health <= 0)
//      return;
//
//  if ( ((skill->value == 3) && (random() < 0.5)) || (range(self, self->enemy) == RANGE_MELEE) )
//      self->monsterinfo.nextframe = FRAME_attak402;
}

static const mframe_t soldierh_frames_attack4[] = {
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, soldierh_fire4 },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL }
};
const mmove_t soldierh_move_attack4 = {FRAME_attak401, FRAME_attak406, soldierh_frames_attack4, soldierh_run};

#if 0
// ATTACK5 (prone)

void soldierh_fire5(edict_t *self)
{
    soldierh_fire(self, 4);
}

void soldierh_attack5_refire(edict_t *self)
{
    if (self->enemy->health <= 0)
        return;

    if (((skill->value == 3) && (random() < 0.5)) || (range(self, self->enemy) == RANGE_MELEE))
        self->monsterinfo.nextframe = FRAME_attak505;
}

static const mframe_t soldierh_frames_attack5[] = {
    { ai_charge, 8, NULL },
    { ai_charge, 8, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, soldierh_fire5 },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, soldierh_attack5_refire }
};
const mmove_t soldierh_move_attack5 = {FRAME_attak501, FRAME_attak508, soldierh_frames_attack5, soldierh_run};
#endif

// ATTACK6 (run & shoot)

void soldierh_fire8(edict_t *self)
{
    soldierh_fire(self, 7);
}

void soldierh_attack6_refire(edict_t *self)
{
    if (self->enemy->health <= 0)
        return;

    if (range(self, self->enemy) < RANGE_MID)
        return;

    if (skill->value == 3)
        self->monsterinfo.nextframe = FRAME_runs03;
}

static const mframe_t soldierh_frames_attack6[] = {
    { ai_charge, 10, NULL },
    { ai_charge,  4, NULL },
    { ai_charge, 12, NULL },
    { ai_charge, 11, soldierh_fire8 },
    { ai_charge, 13, NULL },
    { ai_charge, 18, NULL },
    { ai_charge, 15, NULL },
    { ai_charge, 14, NULL },
    { ai_charge, 11, NULL },
    { ai_charge,  8, NULL },
    { ai_charge, 11, NULL },
    { ai_charge, 12, NULL },
    { ai_charge, 12, NULL },
    { ai_charge, 17, soldierh_attack6_refire }
};
const mmove_t soldierh_move_attack6 = {FRAME_runs01, FRAME_runs14, soldierh_frames_attack6, soldierh_run};

void soldierh_attack(edict_t *self)
{
    if (self->s.skinnum < 4) {
        if (random() < 0.5)
            self->monsterinfo.currentmove = &soldierh_move_attack1;
        else
            self->monsterinfo.currentmove = &soldierh_move_attack2;
    } else {
        self->monsterinfo.currentmove = &soldierh_move_attack4;
    }
}

//
// SIGHT
//

void soldierh_sight(edict_t *self, edict_t *other)
{
    if (random() < 0.5)
        gi.sound(self, CHAN_VOICE, sound_sight1, 1, ATTN_NORM, 0);
    else
        gi.sound(self, CHAN_VOICE, sound_sight2, 1, ATTN_NORM, 0);

    if ((skill->value > 0) && (range(self, self->enemy) >= RANGE_MID)) {
        if (random() > 0.5) {
            if (self->s.skinnum < 4)
                self->monsterinfo.currentmove = &soldierh_move_attack6;
            else
                self->monsterinfo.currentmove = &soldierh_move_attack4;
        }
    }
}

//
// DUCK
//

void soldierh_duck_hold(edict_t *self)
{
    if (level.framenum >= self->monsterinfo.pause_framenum)
        self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
    else
        self->monsterinfo.aiflags |= AI_HOLD_FRAME;
}

static const mframe_t soldierh_frames_duck[] = {
    { ai_move, 5, soldierh_duck_down },
    { ai_move, -1, soldierh_duck_hold },
    { ai_move, 1,  NULL },
    { ai_move, 0,  soldierh_duck_up },
    { ai_move, 5,  NULL }
};
const mmove_t soldierh_move_duck = {FRAME_duck01, FRAME_duck05, soldierh_frames_duck, soldierh_run};

void soldierh_dodge(edict_t *self, edict_t *attacker, float eta, trace_t *tr)
{
    float   r;

    r = random();
    if (r > 0.25)
        return;

    if (!self->enemy)
        self->enemy = attacker;

    if (skill->value == 0) {
        self->monsterinfo.currentmove = &soldierh_move_duck;
        return;
    }

    self->monsterinfo.pause_framenum = level.framenum + (eta + 0.3) * BASE_FRAMERATE;
    r = random();

    if (skill->value == 1) {
        if (r > 0.33)
            self->monsterinfo.currentmove = &soldierh_move_duck;
        else
            self->monsterinfo.currentmove = &soldierh_move_attack3;
        return;
    }

    if (skill->value >= 2) {
        if (r > 0.66)
            self->monsterinfo.currentmove = &soldierh_move_duck;
        else
            self->monsterinfo.currentmove = &soldierh_move_attack3;
        return;
    }

    self->monsterinfo.currentmove = &soldierh_move_attack3;
}

//
// DEATH
//

void soldierh_fire6(edict_t *self)
{

    // no fire laser
    if (self->s.skinnum < 4)
        soldierh_fire(self, 5);

}

void soldierh_fire7(edict_t *self)
{

    // no fire laser
    if (self->s.skinnum < 4)
        soldierh_fire(self, 6);

}

void soldierh_dead(edict_t *self)
{
    VectorSet(self->mins, -16, -16, -24);
    VectorSet(self->maxs, 16, 16, -8);
    self->movetype = MOVETYPE_TOSS;
    self->svflags |= SVF_DEADMONSTER;
    self->nextthink = 0;
    gi.linkentity(self);
}

static const mframe_t soldierh_frames_death1[] = {
    { ai_move, 0,   NULL },
    { ai_move, -10, NULL },
    { ai_move, -10, NULL },
    { ai_move, -10, NULL },
    { ai_move, -5,  NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   soldierh_fire6 },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   soldierh_fire7 },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL }
};
const mmove_t soldierh_move_death1 = {FRAME_death101, FRAME_death136, soldierh_frames_death1, soldierh_dead};

static const mframe_t soldierh_frames_death2[] = {
    { ai_move, -5,  NULL },
    { ai_move, -5,  NULL },
    { ai_move, -5,  NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL }
};
const mmove_t soldierh_move_death2 = {FRAME_death201, FRAME_death235, soldierh_frames_death2, soldierh_dead};

static const mframe_t soldierh_frames_death3[] = {
    { ai_move, -5,  NULL },
    { ai_move, -5,  NULL },
    { ai_move, -5,  NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
};
const mmove_t soldierh_move_death3 = {FRAME_death301, FRAME_death345, soldierh_frames_death3, soldierh_dead};

static const mframe_t soldierh_frames_death4[] = {
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL }
};
const mmove_t soldierh_move_death4 = {FRAME_death401, FRAME_death453, soldierh_frames_death4, soldierh_dead};

static const mframe_t soldierh_frames_death5[] = {
    { ai_move, -5,  NULL },
    { ai_move, -5,  NULL },
    { ai_move, -5,  NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },

    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL }
};
const mmove_t soldierh_move_death5 = {FRAME_death501, FRAME_death524, soldierh_frames_death5, soldierh_dead};

static const mframe_t soldierh_frames_death6[] = {
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL }
};
const mmove_t soldierh_move_death6 = {FRAME_death601, FRAME_death610, soldierh_frames_death6, soldierh_dead};

void soldierh_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    int     n;

// check for gib
    if (self->health <= self->gib_health) {
        gi.sound(self, CHAN_VOICE, gi.soundindex("misc/udeath.wav"), 1, ATTN_NORM, 0);
        for (n = 0; n < 3; n++)
            ThrowGib(self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
        ThrowGib(self, "models/objects/gibs/chest/tris.md2", damage, GIB_ORGANIC);
        ThrowHead(self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);

        self->deadflag = DEAD_DEAD;
        return;
    }

    if (self->deadflag == DEAD_DEAD)
        return;

// regular death
    self->deadflag = DEAD_DEAD;
    self->takedamage = DAMAGE_YES;
    self->s.skinnum |= 1;

    if (self->s.skinnum == 1)
        gi.sound(self, CHAN_VOICE, sound_death_light, 1, ATTN_NORM, 0);
    else if (self->s.skinnum == 3)
        gi.sound(self, CHAN_VOICE, sound_death, 1, ATTN_NORM, 0);
    else // (self->s.skinnum == 5)
        gi.sound(self, CHAN_VOICE, sound_death_ss, 1, ATTN_NORM, 0);

    if (fabs((self->s.origin[2] + self->viewheight) - point[2]) <= 4) {
        // head shot
        self->monsterinfo.currentmove = &soldierh_move_death3;
        return;
    }

    n = Q_rand() % 5;
    if (n == 0)
        self->monsterinfo.currentmove = &soldierh_move_death1;
    else if (n == 1)
        self->monsterinfo.currentmove = &soldierh_move_death2;
    else if (n == 2)
        self->monsterinfo.currentmove = &soldierh_move_death4;
    else if (n == 3)
        self->monsterinfo.currentmove = &soldierh_move_death5;
    else
        self->monsterinfo.currentmove = &soldierh_move_death6;
}

//
// SPAWN
//

void SP_monster_soldier_h(edict_t *self)
{

    self->s.modelindex = gi.modelindex("models/monsters/soldierh/tris.md2");
    self->monsterinfo.scale = MODEL_SCALE;
    VectorSet(self->mins, -16, -16, -24);
    VectorSet(self->maxs, 16, 16, 32);
    self->movetype = MOVETYPE_STEP;
    self->solid = SOLID_BBOX;

    G_AddPrecache(soldier_precache_x);

    self->mass = 100;

    self->pain = soldierh_pain;
    self->die = soldierh_die;

    self->monsterinfo.stand = soldierh_stand;
    self->monsterinfo.walk = soldierh_walk;
    self->monsterinfo.run = soldierh_run;
    self->monsterinfo.dodge = soldierh_dodge;
    self->monsterinfo.attack = soldierh_attack;
    self->monsterinfo.melee = NULL;
    self->monsterinfo.sight = soldierh_sight;

    gi.linkentity(self);

    // self->monsterinfo.stand (self);
    self->monsterinfo.currentmove = &soldierh_move_stand3;

    walkmonster_start(self);
}

/*QUAKED monster_soldier_ripper (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_soldier_ripper(edict_t *self)
{
    // R-MODE-7 / R-CORE-8: the ruleset decides, not `deathmatch`.  Converted on
    // import for the same reason as baseq2's monsters -- `deathmatch` is 1 under
    // ctf, and R-MODE-7 promises monsters there.
    if (!G_MonstersAllowed()) {
        G_FreeEdict(self);
        return;
    }

    SP_monster_soldier_h(self);

    G_AddPrecache(soldier_precache_light);

    gi.modelindex("models/objects/boomrang/tris.md2");
    gi.soundindex("misc/lasfly.wav");
    gi.soundindex("soldier/solatck2.wav");

    self->s.skinnum = 0;
    self->health = 50;
    self->gib_health = -30;
}

/*QUAKED monster_soldier_hypergun (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_soldier_hypergun(edict_t *self)
{
    // R-MODE-7 / R-CORE-8: the ruleset decides, not `deathmatch`.  Converted on
    // import for the same reason as baseq2's monsters -- `deathmatch` is 1 under
    // ctf, and R-MODE-7 promises monsters there.
    if (!G_MonstersAllowed()) {
        G_FreeEdict(self);
        return;
    }

    SP_monster_soldier_h(self);

    gi.modelindex("models/objects/blaser/tris.md2");
    G_AddPrecache(soldier_precache);
    gi.soundindex("soldier/solatck1.wav");

    self->s.skinnum = 2;
    self->health = 60;
    self->gib_health = -30;
}

/*QUAKED monster_soldier_lasergun (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_soldier_lasergun(edict_t *self)
{
    // R-MODE-7 / R-CORE-8: the ruleset decides, not `deathmatch`.  Converted on
    // import for the same reason as baseq2's monsters -- `deathmatch` is 1 under
    // ctf, and R-MODE-7 promises monsters there.
    if (!G_MonstersAllowed()) {
        G_FreeEdict(self);
        return;
    }

    SP_monster_soldier_h(self);

    G_AddPrecache(soldier_precache_ss);
    gi.soundindex("soldier/solatck3.wav");

    self->s.skinnum = 4;
    self->health = 70;
    self->gib_health = -30;

}

// END 13-APR-98
