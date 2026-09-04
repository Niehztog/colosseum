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

INFANTRY

==============================================================================
*/

#include "g_local.h"
#include "m_infantry.h"

static void InfantryMachineGun(edict_t *self);

static int  sound_pain1;
static int  sound_pain2;
static int  sound_die1;
static int  sound_die2;

static int  sound_gunshot;
static int  sound_weapon_cock;
static int  sound_punch_swing;
static int  sound_punch_hit;
static int  sound_sight;
static int  sound_search;
static int  sound_idle;

static const mframe_t infantry_frames_stand[] = {
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
const mmove_t infantry_move_stand = {FRAME_stand50, FRAME_stand71, infantry_frames_stand, NULL};

void infantry_stand(edict_t *self)
{
    self->monsterinfo.currentmove = &infantry_move_stand;
}

static const mframe_t infantry_frames_fidget[] = {
    { ai_stand, 1,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 1,  NULL },
    { ai_stand, 3,  NULL },
    { ai_stand, 6,  NULL },
    { ai_stand, 3,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 1,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 1,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, -1, NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 1,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, -2, NULL },
    { ai_stand, 1,  NULL },
    { ai_stand, 1,  NULL },
    { ai_stand, 1,  NULL },
    { ai_stand, -1, NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, -1, NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, -1, NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 1,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, -1, NULL },
    { ai_stand, -1, NULL },
    { ai_stand, 0,  NULL },
    { ai_stand, -3, NULL },
    { ai_stand, -2, NULL },
    { ai_stand, -3, NULL },
    { ai_stand, -3, NULL },
    { ai_stand, -2, NULL }
};
const mmove_t infantry_move_fidget = {FRAME_stand01, FRAME_stand49, infantry_frames_fidget, infantry_stand};

void infantry_fidget(edict_t *self)
{
    self->monsterinfo.currentmove = &infantry_move_fidget;
    gi.sound(self, CHAN_VOICE, sound_idle, 1, ATTN_IDLE, 0);
}

static const mframe_t infantry_frames_walk[] = {
    { ai_walk, 5,  NULL },
    { ai_walk, 4,  NULL },
    { ai_walk, 4,  NULL },
    { ai_walk, 5,  NULL },
    { ai_walk, 4,  NULL },
    { ai_walk, 5,  NULL },
    { ai_walk, 6,  NULL },
    { ai_walk, 4,  NULL },
    { ai_walk, 4,  NULL },
    { ai_walk, 4,  NULL },
    { ai_walk, 4,  NULL },
    { ai_walk, 5,  NULL }
};
const mmove_t infantry_move_walk = {FRAME_walk03, FRAME_walk14, infantry_frames_walk, NULL};

void infantry_walk(edict_t *self)
{
    self->monsterinfo.currentmove = &infantry_move_walk;
}

static const mframe_t infantry_frames_run[] = {
    { ai_run, 10, NULL },
    { ai_run, 20, NULL },
    { ai_run, 5,  NULL },
    { ai_run, 7,  monster_done_dodge },
    { ai_run, 30, NULL },
    { ai_run, 35, NULL },
    { ai_run, 2,  NULL },
    { ai_run, 6,  NULL }
};
const mmove_t infantry_move_run = {FRAME_run01, FRAME_run08, infantry_frames_run, NULL};

void infantry_run(edict_t *self)
{
    monster_done_dodge(self);

    if (self->monsterinfo.aiflags & AI_STAND_GROUND)
        self->monsterinfo.currentmove = &infantry_move_stand;
    else
        self->monsterinfo.currentmove = &infantry_move_run;
}

static const mframe_t infantry_frames_pain1[] = {
    { ai_move, -3, NULL },
    { ai_move, -2, NULL },
    { ai_move, -1, NULL },
    { ai_move, -2, NULL },
    { ai_move, -1, NULL },
    { ai_move, 1,  NULL },
    { ai_move, -1, NULL },
    { ai_move, 1,  NULL },
    { ai_move, 6,  NULL },
    { ai_move, 2,  NULL }
};
const mmove_t infantry_move_pain1 = {FRAME_pain101, FRAME_pain110, infantry_frames_pain1, infantry_run};

static const mframe_t infantry_frames_pain2[] = {
    { ai_move, -3, NULL },
    { ai_move, -3, NULL },
    { ai_move, 0,  NULL },
    { ai_move, -1, NULL },
    { ai_move, -2, NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 2,  NULL },
    { ai_move, 5,  NULL },
    { ai_move, 2,  NULL }
};
const mmove_t infantry_move_pain2 = {FRAME_pain201, FRAME_pain210, infantry_frames_pain2, infantry_run};

void infantry_pain(edict_t *self, edict_t *other, float kick, int damage)
{
    int     n;

    if (self->health < (self->max_health / 2))
        self->s.skinnum = 1;

    if (!self->groundentity) {
//      if ((g_showlogic) && (g_showlogic->value))
//          gi.dprintf ("infantry: pain avoided due to no ground\n");
        return;
    }

    monster_done_dodge(self);

    if (level.framenum < self->pain_debounce_framenum)
        return;

    self->pain_debounce_framenum = level.framenum + 3 * BASE_FRAMERATE;

    if (skill->value == 3)
        return;     // no pain anims in nightmare

    n = Q_rand() % 2;
    if (n == 0) {
        self->monsterinfo.currentmove = &infantry_move_pain1;
        gi.sound(self, CHAN_VOICE, sound_pain1, 1, ATTN_NORM, 0);
    } else {
        self->monsterinfo.currentmove = &infantry_move_pain2;
        gi.sound(self, CHAN_VOICE, sound_pain2, 1, ATTN_NORM, 0);
    }

    // PMM - clear duck flag
    if (self->monsterinfo.aiflags & AI_DUCKED)
        monster_duck_up(self);
}

static const vec3_t aimangles[] = {
    { 0.0, 5.0, 0.0 },
    { 10.0, 15.0, 0.0 },
    { 20.0, 25.0, 0.0 },
    { 25.0, 35.0, 0.0 },
    { 30.0, 40.0, 0.0 },
    { 30.0, 45.0, 0.0 },
    { 25.0, 50.0, 0.0 },
    { 20.0, 40.0, 0.0 },
    { 15.0, 35.0, 0.0 },
    { 40.0, 35.0, 0.0 },
    { 70.0, 35.0, 0.0 },
    { 90.0, 35.0, 0.0 }
};

static void InfantryMachineGun(edict_t *self)
{
    vec3_t  start, target;
    vec3_t  forward, right;
    vec3_t  vec;
    int     flash_number;

    if (!self->enemy || !self->enemy->inuse)    //PGM
        return;                                 //PGM

    // *** WHICH ANIMATION IS RUNNING, not which frame number. ***
    //
    // Three donors give the infantry three different fire frames, because each
    // reordered the attack animation: baseq2 FRAME_attak111, The Reckoning
    // FRAME_attak103, Ground Zero FRAME_attak104. The content latch chooses
    // among all three tables.
    //
    // With `rogue 0`, which is plain dm and sp, the baseq2 sequence fires on
    // frame 111, this test failed, and the shot fell into the DEATH-SPRAY
    // branch below: `flash_number = MZ2_INFANTRY_MACHINEGUN_2 + (194 - 155)`,
    // a muzzle flash belonging to another monster entirely, and
    // `aimangles[39]` on a twelve-element array.
    //
    // The two branches do not mean "frame 104" and "not frame 104".  They mean
    // "the attack shot" and "the death spray", and only two animations reach
    // here: infantry_frames_attack1 (either donor's) and
    // infantry_frames_death2.  Asking which one is running is correct for all
    // three donors and needs no constant of its own.
    if (self->s.frame >= FRAME_attak101 && self->s.frame <= FRAME_attak115) {
        flash_number = MZ2_INFANTRY_MACHINEGUN_1;
        AngleVectors(self->s.angles, forward, right, NULL);
        G_ProjectSource(self->s.origin, monster_flash_offset[flash_number], forward, right, start);

        if (self->enemy) {
            VectorMA(self->enemy->s.origin, -0.2f, self->enemy->velocity, target);
            target[2] += self->enemy->viewheight;
            VectorSubtract(target, start, forward);
            VectorNormalize(forward);
        } else {
            AngleVectors(self->s.angles, forward, right, NULL);
        }
    } else {
        flash_number = MZ2_INFANTRY_MACHINEGUN_2 + (self->s.frame - FRAME_death211);

        AngleVectors(self->s.angles, forward, right, NULL);
        G_ProjectSource(self->s.origin, monster_flash_offset[flash_number], forward, right, start);

        VectorSubtract(self->s.angles, aimangles[flash_number - MZ2_INFANTRY_MACHINEGUN_2], vec);
        AngleVectors(vec, forward, NULL, NULL);
    }

    monster_fire_bullet(self, start, forward, 3, 4, DEFAULT_BULLET_HSPREAD, DEFAULT_BULLET_VSPREAD, flash_number);
}

void infantry_sight(edict_t *self, edict_t *other)
{
    gi.sound(self, CHAN_BODY, sound_sight, 1, ATTN_NORM, 0);
}

static void infantry_dead(edict_t *self)
{
    VectorSet(self->mins, -16, -16, -24);
    VectorSet(self->maxs, 16, 16, -8);
    self->movetype = MOVETYPE_TOSS;
    self->svflags |= SVF_DEADMONSTER;
    gi.linkentity(self);

    M_FlyCheck(self);
}

static const mframe_t infantry_frames_death1[] = {
    { ai_move, -4, NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, -1, NULL },
    { ai_move, -4, NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, 0,  NULL },
    { ai_move, -1, NULL },
    { ai_move, 3,  NULL },
    { ai_move, 1,  NULL },
    { ai_move, 1,  NULL },
    { ai_move, -2, NULL },
    { ai_move, 2,  NULL },
    { ai_move, 2,  NULL },
    { ai_move, 9,  NULL },
    { ai_move, 9,  NULL },
    { ai_move, 5,  NULL },
    { ai_move, -3, NULL },
    { ai_move, -3, NULL }
};
const mmove_t infantry_move_death1 = {FRAME_death101, FRAME_death120, infantry_frames_death1, infantry_dead};

// Off with his head
static const mframe_t infantry_frames_death2[] = {
    { ai_move, 0,   NULL },
    { ai_move, 1,   NULL },
    { ai_move, 5,   NULL },
    { ai_move, -1,  NULL },
    { ai_move, 0,   NULL },
    { ai_move, 1,   NULL },
    { ai_move, 1,   NULL },
    { ai_move, 4,   NULL },
    { ai_move, 3,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, -2,  InfantryMachineGun },
    { ai_move, -2,  InfantryMachineGun },
    { ai_move, -3,  InfantryMachineGun },
    { ai_move, -1,  InfantryMachineGun },
    { ai_move, -2,  InfantryMachineGun },
    { ai_move, 0,   InfantryMachineGun },
    { ai_move, 2,   InfantryMachineGun },
    { ai_move, 2,   InfantryMachineGun },
    { ai_move, 3,   InfantryMachineGun },
    { ai_move, -10, InfantryMachineGun },
    { ai_move, -7,  InfantryMachineGun },
    { ai_move, -8,  InfantryMachineGun },
    { ai_move, -6,  NULL },
    { ai_move, 4,   NULL },
    { ai_move, 0,   NULL }
};
const mmove_t infantry_move_death2 = {FRAME_death201, FRAME_death225, infantry_frames_death2, infantry_dead};

static const mframe_t infantry_frames_death3[] = {
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL },
    { ai_move, -6,  NULL },
    { ai_move, -11, NULL },
    { ai_move, -3,  NULL },
    { ai_move, -11, NULL },
    { ai_move, 0,   NULL },
    { ai_move, 0,   NULL }
};
const mmove_t infantry_move_death3 = {FRAME_death301, FRAME_death309, infantry_frames_death3, infantry_dead};

void infantry_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
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

    n = Q_rand() % 3;
    if (n == 0) {
        self->monsterinfo.currentmove = &infantry_move_death1;
        gi.sound(self, CHAN_VOICE, sound_die2, 1, ATTN_NORM, 0);
    } else if (n == 1) {
        self->monsterinfo.currentmove = &infantry_move_death2;
        gi.sound(self, CHAN_VOICE, sound_die1, 1, ATTN_NORM, 0);
    } else {
        self->monsterinfo.currentmove = &infantry_move_death3;
        gi.sound(self, CHAN_VOICE, sound_die2, 1, ATTN_NORM, 0);
    }
}


static const mframe_t infantry_frames_duck[] = {
    { ai_move, -2, monster_duck_down },
    { ai_move, -5, monster_duck_hold },
    { ai_move, 3,  NULL },
    { ai_move, 4,  monster_duck_up },
    { ai_move, 0,  NULL }
};
const mmove_t infantry_move_duck = {FRAME_duck01, FRAME_duck05, infantry_frames_duck, infantry_run};

// PMM - dodge code moved below so I can see the attack frames

// baseq2's attack timing helper.  Ground Zero's attack frame table does not
// call it, and R-CORE-11 says both tables ship and the gate selects -- so this
// is kept for that gating rather than deleted, and tagged meanwhile.
static q_unused void infantry_set_firetime(edict_t *self)
{
    // pmm .. code that was here no longer needed
    gi.sound(self, CHAN_WEAPON, sound_weapon_cock, 1, ATTN_NORM, 0);
}

static void infantry_cock_gun(edict_t *self)
{
    gi.sound(self, CHAN_WEAPON, sound_weapon_cock, 1, ATTN_NORM, 0);
}

// baseq2's, which is the same sound AND the hold timer -- Ground Zero split the
// two, moving the timer into infantry_fire_prep at an earlier frame and leaving
// the sound behind.  The baseq2 sequence has no fire_prep, so pointing its table
// at Ground Zero's sound-only version left `pause_framenum` at whatever the
// previous burst had set: infantry_fire's AI_HOLD_FRAME test then held for the
// wrong length, or not at all.
static void bq2_infantry_cock_gun(edict_t *self)
{
    int n;

    gi.sound(self, CHAN_WEAPON, sound_weapon_cock, 1, ATTN_NORM, 0);
    n = (Q_rand() & 15) + 3 + 7;
    self->monsterinfo.pause_framenum = level.framenum + n;
}

static void infantry_fire(edict_t *self)
{
    InfantryMachineGun(self);
    if (level.framenum >= self->monsterinfo.pause_framenum)
        self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
    else
        self->monsterinfo.aiflags |= AI_HOLD_FRAME;
}

// this is here instead of cock_gun

static void infantry_fire_prep(edict_t *self)
{
    int n;
    n = (Q_rand() & 15) + 3 + 1;
    self->monsterinfo.pause_framenum = level.framenum + n;
}

static void xatrix_infantry_set_firetime(edict_t *self)
{
    int n;

    n = (Q_rand() & 15) + 5;
    self->monsterinfo.pause_framenum = level.framenum + n;
}

static const mframe_t xatrix_infantry_frames_attack1[] = {
    { ai_charge, 10, xatrix_infantry_set_firetime },
    { ai_charge,  6, NULL },
    { ai_charge,  0, infantry_fire },
    { ai_charge,  0, NULL },
    { ai_charge,  1, NULL },
    { ai_charge, -7, NULL },
    { ai_charge, -6, NULL },
    { ai_charge, -1, NULL },
    { ai_charge,  0, infantry_cock_gun },
    { ai_charge,  0, NULL },
    { ai_charge,  0, NULL },
    { ai_charge,  0, NULL },
    { ai_charge,  0, NULL },
    { ai_charge, -1, NULL },
    { ai_charge, -1, NULL }
};
const mmove_t xatrix_infantry_move_attack1 = {FRAME_attak101, FRAME_attak115, xatrix_infantry_frames_attack1, infantry_run};

// pmm
// frames reordered, tweaked for new frames

static const mframe_t infantry_frames_attack1[] = {
    { ai_charge, -3, NULL },                //101
    { ai_charge, -2, NULL },                //102
    { ai_charge, -1, infantry_fire_prep },  //103
    { ai_charge, 5,  infantry_fire },       //104
    { ai_charge, 1,  NULL },                //105
    { ai_charge, -3, NULL },                //106
    { ai_charge, -2, NULL },                //107
    { ai_charge, 2,  infantry_cock_gun },   //108
    { ai_charge, 1,  NULL },                //109
    { ai_charge, 1,  NULL },                //110
    { ai_charge, -1, NULL },                //111
    { ai_charge, 0,  NULL },                //112
    { ai_charge, -1, NULL },                //113
    { ai_charge, -1, NULL },                //114
    { ai_charge, 4,  NULL }                 //115
};
const mmove_t infantry_move_attack1 = {FRAME_attak101, FRAME_attak115, infantry_frames_attack1, infantry_run};

// R-CORE-11: baseq2's infantry_move_attack1, kept alongside Ground Zero's.
// Ground Zero adds infantry_fire_prep and moves the fire frame, changing the
// attack's start and timing.
static const mframe_t bq2_infantry_frames_attack1[] = {
    { ai_charge, 4,  NULL },
    { ai_charge, -1, NULL },
    { ai_charge, -1, NULL },
    { ai_charge, 0,  bq2_infantry_cock_gun },
    { ai_charge, -1, NULL },
    { ai_charge, 1,  NULL },
    { ai_charge, 1,  NULL },
    { ai_charge, 2,  NULL },
    { ai_charge, -2, NULL },
    { ai_charge, -3, NULL },
    { ai_charge, 1,  infantry_fire },
    { ai_charge, 5,  NULL },
    { ai_charge, -1, NULL },
    { ai_charge, -2, NULL },
    { ai_charge, -3, NULL }
};

const mmove_t bq2_infantry_move_attack1 = {FRAME_attak101, FRAME_attak115, bq2_infantry_frames_attack1, infantry_run};

static void infantry_swing(edict_t *self)
{
    gi.sound(self, CHAN_WEAPON, sound_punch_swing, 1, ATTN_NORM, 0);
}

static void infantry_smack(edict_t *self)
{
    vec3_t  aim = { MELEE_DISTANCE, 0, 0 };

    if (fire_hit(self, aim, (5 + (Q_rand() % 5)), 50))
        gi.sound(self, CHAN_WEAPON, sound_punch_hit, 1, ATTN_NORM, 0);
}

static const mframe_t infantry_frames_attack2[] = {
    { ai_charge, 3, NULL },
    { ai_charge, 6, NULL },
    { ai_charge, 0, infantry_swing },
    { ai_charge, 8, NULL },
    { ai_charge, 5, NULL },
    { ai_charge, 8, infantry_smack },
    { ai_charge, 6, NULL },
    { ai_charge, 3, NULL },
};
const mmove_t infantry_move_attack2 = {FRAME_attak201, FRAME_attak208, infantry_frames_attack2, infantry_run};

void infantry_attack(edict_t *self)
{
    monster_done_dodge(self);

    if (range(self, self->enemy) == RANGE_MELEE)
        self->monsterinfo.currentmove = &infantry_move_attack2;
    else {
        // R-CORE-11: all three sequences ship; the latch selects (if/else with
        // literal assignments so genptr.py sees both -- R-CORE-11b).
        if (self->content_flavour & CONTENT_ROGUE)
            self->monsterinfo.currentmove = &infantry_move_attack1;
        else if (self->content_flavour & CONTENT_XATRIX)
            self->monsterinfo.currentmove = &xatrix_infantry_move_attack1;
        else
            self->monsterinfo.currentmove = &bq2_infantry_move_attack1;
    }
}

//===========
//PGM
static void infantry_jump_now(edict_t *self)
{
    vec3_t  forward, up;

    monster_jump_start(self);

    AngleVectors(self->s.angles, forward, NULL, up);
    VectorMA(self->velocity, 100, forward, self->velocity);
    VectorMA(self->velocity, 300, up, self->velocity);
}

void infantry_jump2_now(edict_t *self)
{
    vec3_t  forward, up;

    monster_jump_start(self);

    AngleVectors(self->s.angles, forward, NULL, up);
    VectorMA(self->velocity, 150, forward, self->velocity);
    VectorMA(self->velocity, 400, up, self->velocity);
}

static void infantry_jump_wait_land(edict_t *self)
{
    if (self->groundentity == NULL) {
        self->monsterinfo.nextframe = self->s.frame;

        if (monster_jump_finished(self))
            self->monsterinfo.nextframe = self->s.frame + 1;
    } else
        self->monsterinfo.nextframe = self->s.frame + 1;
}

static const mframe_t infantry_frames_jump[] = {
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, infantry_jump_now },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, infantry_jump_wait_land },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL }
};
const mmove_t infantry_move_jump = { FRAME_jump01, FRAME_jump10, infantry_frames_jump, infantry_run };

static const mframe_t infantry_frames_jump2[] = {
    { ai_move, -8, NULL },
    { ai_move, -4, NULL },
    { ai_move, -4, NULL },
    { ai_move, 0, infantry_jump_now },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, infantry_jump_wait_land },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL }
};
const mmove_t infantry_move_jump2 = { FRAME_jump01, FRAME_jump10, infantry_frames_jump2, infantry_run };

void infantry_jump(edict_t *self)
{
    if (!self->enemy)
        return;

    monster_done_dodge(self);

    if (self->enemy->s.origin[2] > self->s.origin[2])
        self->monsterinfo.currentmove = &infantry_move_jump2;
    else
        self->monsterinfo.currentmove = &infantry_move_jump;
}

bool infantry_blocked(edict_t *self, float dist)
{
    if (blocked_checkshot(self, 0.25f + (0.05f * skill->value)))
        return true;

    if (blocked_checkjump(self, dist, 192, 40)) {
        infantry_jump(self);
        return true;
    }

    if (blocked_checkplat(self, dist))
        return true;

    return false;
}
//PGM
//===========
/*
void infantry_dodge (edict_t *self, edict_t *attacker, float eta, trace_t *tr)
{
//===========
//PMM - rogue rewrite of gunner dodge code.
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

    if ((self->monsterinfo.currentmove == &infantry_move_attack1) ||
        (self->monsterinfo.currentmove == &infantry_move_attack2))
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
        vec3_t right,diff;

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
        self->monsterinfo.currentmove = &infantry_move_run;
        self->monsterinfo.attack_state = AS_SLIDING;
        return;
    }

    if (skill->value == 0)
    {
        self->monsterinfo.currentmove = &infantry_move_duck;
        // PMM - stupid dodge
        self->monsterinfo.duck_wait_framenum = level.framenum + (eta + 1) * BASE_FRAMERATE;
        self->monsterinfo.aiflags |= AI_DODGING;
        return;
    }

    if (!shooting)
    {
        self->monsterinfo.currentmove = &infantry_move_duck;
        self->monsterinfo.duck_wait_framenum = level.framenum + (eta + (0.1 * (3 - skill->value))) * BASE_FRAMERATE;
        self->monsterinfo.aiflags |= AI_DODGING;
    }
    return;
//PMM
//===========
*/

void infantry_duck(edict_t *self, float eta)
{
    // if we're jumping, don't dodge
    if ((self->monsterinfo.currentmove == &infantry_move_jump) ||
        (self->monsterinfo.currentmove == &infantry_move_jump2)) {
        return;
    }

    if ((self->monsterinfo.currentmove == &infantry_move_attack1) ||
        (self->monsterinfo.currentmove == &infantry_move_attack2)) {
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

    // has to be done immediately otherwise he can get stuck
    monster_duck_down(self);

    self->monsterinfo.nextframe = FRAME_duck01;
    self->monsterinfo.currentmove = &infantry_move_duck;
    return;
}

void infantry_sidestep(edict_t *self)
{
    // if we're jumping, don't dodge
    if ((self->monsterinfo.currentmove == &infantry_move_jump) ||
        (self->monsterinfo.currentmove == &infantry_move_jump2)) {
        return;
    }

    if ((self->monsterinfo.currentmove == &infantry_move_attack1) ||
        (self->monsterinfo.currentmove == &infantry_move_attack2)) {
        // if we're shooting, and not on easy, don't dodge
        if (skill->value) {
            self->monsterinfo.aiflags &= ~AI_DODGING;
            return;
        }
    }

    if (self->monsterinfo.currentmove != &infantry_move_run)
        self->monsterinfo.currentmove = &infantry_move_run;
}

static void infantry_precache(void)
{
    sound_pain1 = gi.soundindex("infantry/infpain1.wav");
    sound_pain2 = gi.soundindex("infantry/infpain2.wav");
    sound_die1 = gi.soundindex("infantry/infdeth1.wav");
    sound_die2 = gi.soundindex("infantry/infdeth2.wav");

    sound_gunshot = gi.soundindex("infantry/infatck1.wav");
    sound_weapon_cock = gi.soundindex("infantry/infatck3.wav");
    sound_punch_swing = gi.soundindex("infantry/infatck2.wav");
    sound_punch_hit = gi.soundindex("infantry/melee2.wav");

    sound_sight = gi.soundindex("infantry/infsght1.wav");
    sound_search = gi.soundindex("infantry/infsrch1.wav");
    sound_idle = gi.soundindex("infantry/infidle1.wav");
}


// ---------------------------------------------------------------------------
// R-CORE-11: baseq2's duck-and-dodge for the infantry, restored alongside Ground
// Zero's rewrite so that BOTH ship and the spawn-time latch selects.
//
// Ground Zero does not add to this monster, it replaces its evasion: baseq2's
// per-monster infantry_dodge and infantry_duck_* become the shared M_MonsterDodge with
// generic monster_duck_* plus a sidestep.  Without this block, `rogue 0` still
// got Ground Zero's AI.
//
// Reintroduced under a bq2_ prefix rather than by un-commenting the donor's dead
// copy, so the two sets are distinct symbols and both are visible to genptr.py
// -- the save_ptrs[] table is built by scanning for literal
// `currentmove = &name` assignments, which is also why the gate below is an
// if/else with two literal assignments rather than a ternary or a macro.
// ---------------------------------------------------------------------------

static void bq2_infantry_duck_down(edict_t *self)
{
    if (self->monsterinfo.aiflags & AI_DUCKED)
        return;
    self->monsterinfo.aiflags |= AI_DUCKED;
    self->maxs[2] -= 32;
    self->takedamage = DAMAGE_YES;
    self->monsterinfo.pause_framenum = level.framenum + 1 * BASE_FRAMERATE;
    gi.linkentity(self);
}

static void bq2_infantry_duck_hold(edict_t *self)
{
    if (level.framenum >= self->monsterinfo.pause_framenum)
        self->monsterinfo.aiflags &= ~AI_HOLD_FRAME;
    else
        self->monsterinfo.aiflags |= AI_HOLD_FRAME;
}

static void bq2_infantry_duck_up(edict_t *self)
{
    self->monsterinfo.aiflags &= ~AI_DUCKED;
    self->maxs[2] += 32;
    self->takedamage = DAMAGE_AIM;
    gi.linkentity(self);
}

static const mframe_t bq2_infantry_frames_duck[] = {
    { ai_move, -2, bq2_infantry_duck_down },
    { ai_move, -5, bq2_infantry_duck_hold },
    { ai_move, 3,  NULL },
    { ai_move, 4,  bq2_infantry_duck_up },
    { ai_move, 0,  NULL }
};

const mmove_t bq2_infantry_move_duck = {FRAME_duck01, FRAME_duck05, bq2_infantry_frames_duck, infantry_run};

void bq2_infantry_dodge(edict_t *self, edict_t *attacker, float eta, trace_t *tr)
{
    if (random() > 0.25f)
        return;

    if (!self->enemy)
        self->enemy = attacker;

    self->monsterinfo.currentmove = &bq2_infantry_move_duck;
}

/*QUAKED monster_infantry (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_infantry(edict_t *self)
{
    // R-MODE-7 / R-CORE-8: the ruleset decides, not `deathmatch`.  The
    // inherited test was right for baseq2 and wrong here, because `deathmatch`
    // is 1 under ctf and R-MODE-7 promises monsters under ctf.
    if (!G_MonstersAllowed()) {
        G_FreeEdict(self);
        return;
    }

    G_AddPrecache(infantry_precache);

    self->movetype = MOVETYPE_STEP;
    self->solid = SOLID_BBOX;
    self->s.modelindex = gi.modelindex("models/monsters/infantry/tris.md2");
    VectorSet(self->mins, -16, -16, -24);
    VectorSet(self->maxs, 16, 16, 32);

    self->health = 100;
    self->gib_health = -40;
    self->mass = 200;

    self->pain = infantry_pain;
    self->die = infantry_die;

    self->monsterinfo.stand = infantry_stand;
    self->monsterinfo.walk = infantry_walk;
    self->monsterinfo.run = infantry_run;
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
        self->monsterinfo.duck = infantry_duck;
        self->monsterinfo.unduck = monster_duck_up;
        self->monsterinfo.sidestep = infantry_sidestep;
    } else {
        self->monsterinfo.dodge = bq2_infantry_dodge;
    }
//  self->monsterinfo.dodge = infantry_dodge;
    // pmm
    self->monsterinfo.attack = infantry_attack;
    self->monsterinfo.melee = NULL;
    self->monsterinfo.sight = infantry_sight;
    self->monsterinfo.idle = infantry_fidget;
    if (self->content_flavour & CONTENT_ROGUE)
        self->monsterinfo.blocked = infantry_blocked;

    gi.linkentity(self);

    self->monsterinfo.currentmove = &infantry_move_stand;
    self->monsterinfo.scale = MODEL_SCALE;

    walkmonster_start(self);
}
