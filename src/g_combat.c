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
// g_combat.c

#include "g_local.h"
#include "tourney/osp_hooks.h"
#include "arena/arena.h"
#include "arena/ra2stats.h"

void M_SetEffects(edict_t *self);

/*
ROGUE
clean up heal targets for medic
*/
void cleanupHealTarget(edict_t *ent)
{
    ent->monsterinfo.healer = NULL;
    ent->takedamage = DAMAGE_YES;
    ent->monsterinfo.aiflags &= ~AI_RESURRECTING;
    M_SetEffects(ent);
}
/*
============
CanDamage

Returns true if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
bool CanDamage(edict_t *targ, edict_t *inflictor)
{
    vec3_t  dest;
    trace_t trace;

// bmodels need special checking because their origin is 0,0,0
    if (targ->movetype == MOVETYPE_PUSH) {
        VectorAvg(targ->absmin, targ->absmax, dest);
        trace = gi.trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
        if (trace.fraction == 1.0f)
            return true;
        if (trace.ent == targ)
            return true;
        return false;
    }

    trace = gi.trace(inflictor->s.origin, vec3_origin, vec3_origin, targ->s.origin, inflictor, MASK_SOLID);
    if (trace.fraction == 1.0f)
        return true;

    VectorCopy(targ->s.origin, dest);
    dest[0] += 15.0f;
    dest[1] += 15.0f;
    trace = gi.trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
    if (trace.fraction == 1.0f)
        return true;

    VectorCopy(targ->s.origin, dest);
    dest[0] += 15.0f;
    dest[1] -= 15.0f;
    trace = gi.trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
    if (trace.fraction == 1.0f)
        return true;

    VectorCopy(targ->s.origin, dest);
    dest[0] -= 15.0f;
    dest[1] += 15.0f;
    trace = gi.trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
    if (trace.fraction == 1.0f)
        return true;

    VectorCopy(targ->s.origin, dest);
    dest[0] -= 15.0f;
    dest[1] -= 15.0f;
    trace = gi.trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
    if (trace.fraction == 1.0f)
        return true;

    return false;
}

/*
============
Killed
============
*/
static void Killed(edict_t *targ, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    if (targ->health < -999)
        targ->health = -999;

    if (targ->monsterinfo.aiflags & AI_MEDIC) {
        if (targ->enemy) { // god, I hope so
            cleanupHealTarget(targ->enemy);
        }

        // clean up self
        targ->monsterinfo.aiflags &= ~AI_MEDIC;
        targ->enemy = attacker;
    } else {
        targ->enemy = attacker;
    }

    if ((targ->svflags & SVF_MONSTER) && (targ->deadflag != DEAD_DEAD)) {
//      targ->svflags |= SVF_DEADMONSTER;   // now treat as a different content type
        //ROGUE - free up slot for spawned monster if it's spawned
        if (targ->monsterinfo.aiflags & AI_SPAWNED_CARRIER) {
            if (targ->monsterinfo.commander && targ->monsterinfo.commander->inuse &&
                !strcmp(targ->monsterinfo.commander->classname, "monster_carrier")) {
                targ->monsterinfo.commander->monsterinfo.monster_slots++;
//              if ((g_showlogic) && (g_showlogic->value))
//                  gi.dprintf ("g_combat: freeing up carrier slot - %d left\n", targ->monsterinfo.commander->monsterinfo.monster_slots);
            }
        }
        if (targ->monsterinfo.aiflags & AI_SPAWNED_MEDIC_C) {
            if (targ->monsterinfo.commander) {
                if (targ->monsterinfo.commander->inuse && !strcmp(targ->monsterinfo.commander->classname, "monster_medic_commander")) {
                    targ->monsterinfo.commander->monsterinfo.monster_slots++;
//                  if ((g_showlogic) && (g_showlogic->value))
//                      gi.dprintf ("g_combat: freeing up medic slot - %d left\n", targ->monsterinfo.commander->monsterinfo.monster_slots);
                }
//              else
//                  if ((g_showlogic) && (g_showlogic->value))
//                      gi.dprintf ("my commander is dead!  he's a %s\n", targ->monsterinfo.commander->classname);
            }
//          else if ((g_showlogic) && (g_showlogic->value))
//              gi.dprintf ("My commander is GONE\n");

        }
        if (targ->monsterinfo.aiflags & AI_SPAWNED_WIDOW) {
            // need to check this because we can have variable numbers of coop players
            if (targ->monsterinfo.commander && targ->monsterinfo.commander->inuse &&
                !strncmp(targ->monsterinfo.commander->classname, "monster_widow", 13)) {
                if (targ->monsterinfo.commander->monsterinfo.monster_used > 0)
                    targ->monsterinfo.commander->monsterinfo.monster_used--;
//              if ((g_showlogic) && (g_showlogic->value))
//                  gi.dprintf ("g_combat: freeing up black widow slot - %d used\n", targ->monsterinfo.commander->monsterinfo.monster_used);
            }
        }
        //rogue
        if ((!(targ->monsterinfo.aiflags & AI_GOOD_GUY)) && (!(targ->monsterinfo.aiflags & AI_DO_NOT_COUNT))) {
            level.killed_monsters++;
            if (coop->value && attacker->client)
                attacker->client->resp.score++;
            // medics won't heal monsters that they kill themselves
            // PMM - now they will
//          if (strcmp(attacker->classname, "monster_medic") == 0)
//              targ->owner = attacker;
        }
    }

    if (targ->movetype == MOVETYPE_PUSH || targ->movetype == MOVETYPE_STOP || targ->movetype == MOVETYPE_NONE) {
        // doors, triggers, etc
        targ->die(targ, inflictor, attacker, damage, point);
        return;
    }

    if ((targ->svflags & SVF_MONSTER) && (targ->deadflag != DEAD_DEAD)) {
        targ->touch = NULL;
        monster_death_use(targ);
    }

    // R-EXTRA-6: a player who dies stops being a camera subject, and every
    // camera watching them has to be told before the body is turned into a
    // corpse -- afterwards there is nothing left to hand the next subject.
    if (G_Ruleset() == RULESET_TOURNEY)
        PlayerDied(targ);

    targ->die(targ, inflictor, attacker, damage, point);
}

/*
================
SpawnDamage
================
*/
static void SpawnDamage(int type, const vec3_t origin, const vec3_t normal, int damage)
{
    if (damage > 255)
        damage = 255;
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(type);
//  gi.WriteByte (damage);
    gi.WritePosition(origin);
    gi.WriteDir(normal);
    gi.multicast(origin, MULTICAST_PVS);
}

/*
============
T_Damage

targ        entity that is being damaged
inflictor   entity that is causing the damage
attacker    entity that caused the inflictor to damage targ
    example: targ=monster, inflictor=rocket, attacker=player

dir         direction of the attack
point       point at which the damage is being inflicted
normal      normal vector from that point
damage      amount of damage being inflicted
knockback   force to be applied against targ as a result of the damage

dflags      these flags are used to control how T_Damage works
    DAMAGE_RADIUS           damage was indirect (from a nearby explosion)
    DAMAGE_NO_ARMOR         armor does not protect from this damage
    DAMAGE_ENERGY           damage is from an energy based weapon
    DAMAGE_NO_KNOCKBACK     do not affect velocity, just view angles
    DAMAGE_BULLET           damage is from a bullet (used for ricochets)
    DAMAGE_NO_PROTECTION    kills godmode, armor, everything
============
*/
static int CheckPowerArmor(edict_t *ent, const vec3_t point, const vec3_t normal, int damage, int dflags)
{
    gclient_t   *client;
    int         save;
    int         power_armor_type;
    int         index;
    int         damagePerCell;
    int         pa_te_type;
    int         power;
    int         power_used;

    if (!damage)
        return 0;

    client = ent->client;

    if (dflags & (DAMAGE_NO_ARMOR | DAMAGE_NO_POWER_ARMOR))     // PGM
        return 0;

    index = 0;  // shut up gcc

    if (client) {
        power_armor_type = PowerArmorType(ent);
        if (power_armor_type != POWER_ARMOR_NONE) {
            index = ITEM_INDEX(FindItem("Cells"));
            power = client->pers.inventory[index];
        }
    } else if (ent->svflags & SVF_MONSTER) {
        power_armor_type = ent->monsterinfo.power_armor_type;
        power = ent->monsterinfo.power_armor_power;
    } else
        return 0;

    if (power_armor_type == POWER_ARMOR_NONE)
        return 0;
    if (!power)
        return 0;

    if (power_armor_type == POWER_ARMOR_SCREEN) {
        vec3_t      vec;
        float       dot;
        vec3_t      forward;

        // only works if damage point is in front
        AngleVectors(ent->s.angles, forward, NULL, NULL);
        VectorSubtract(point, ent->s.origin, vec);
        VectorNormalize(vec);
        dot = DotProduct(vec, forward);
        if (dot <= 0.3f)
            return 0;

        damagePerCell = 1;
        pa_te_type = TE_SCREEN_SPARKS;
        damage = damage / 3;
    } else {
        // CTF halves the power shield's efficiency -- "power armor is weaker in
        // CTF", the donor's own comment.  A balance decision that belongs to the
        // ruleset, so it is gated rather than taken globally.
        damagePerCell = (G_Ruleset() == RULESET_CTF) ? 1 : 2;
        pa_te_type = TE_SHIELD_SPARKS;
        damage = (2 * damage) / 3;
    }

    // etf rifle
    if (dflags & DAMAGE_NO_REG_ARMOR)
        save = (power * damagePerCell) / 2;
    else
        save = power * damagePerCell;

    if (!save)
        return 0;
    if (save > damage)
        save = damage;

    SpawnDamage(pa_te_type, point, normal, save);
    ent->powerarmor_framenum = level.framenum + 0.2f * BASE_FRAMERATE;

    if (dflags & DAMAGE_NO_REG_ARMOR)
        power_used = (save / damagePerCell) * 2;
    else
        power_used = save / damagePerCell;

    if (client)
        client->pers.inventory[index] -= power_used;
    else
        ent->monsterinfo.power_armor_power -= power_used;
    return save;
}

static int CheckArmor(edict_t *ent, const vec3_t point, const vec3_t normal, int damage, int te_sparks, int dflags, edict_t *attacker)
{
    gclient_t   *client;
    int         save;
    int         take;
    int         index;
    const gitem_t   *armor;

    if (!damage)
        return 0;

    client = ent->client;

    if (!client)
        return 0;

    // ROGUE - added DAMAGE_NO_REG_ARMOR for atf rifle
    if (dflags & (DAMAGE_NO_ARMOR | DAMAGE_NO_REG_ARMOR))
        return 0;

    index = ArmorIndex(ent);
    if (!index)
        return 0;

    armor = GetItemByIndex(index);

    if (dflags & DAMAGE_ENERGY)
        save = ceilf(((const gitem_armor_t *)armor->info)->energy_protection * damage);
    else
        save = ceilf(((const gitem_armor_t *)armor->info)->normal_protection * damage);
    if (save >= client->pers.inventory[index])
        save = client->pers.inventory[index];

    if (!save)
        return 0;

    take = save;

    // R-ARENA-1: `armorprotect`, the armour half of RA2's friendly fire and a
    // per-arena SETTING rather than a dmflag -- 1 exempts anyone on your team
    // including yourself, 2 exempts a team-mate and leaves your own splash to
    // eat your armour, which is what makes a rocket jump cost something.  The
    // health half is in T_Damage.
    //
    // `save` is still returned when `take` is zeroed: the armour absorbs the
    // hit without being spent, which is the difference between "the shot does
    // nothing" and "the shot is free".  That is the donor's own shape.
    if (G_Ruleset() == RULESET_ARENA && attacker &&
        !(dflags & DAMAGE_NO_PROTECTION) && OnSameTeam(ent, attacker)) {
        int protect = arenas[ent->client->resp.context].armorprotect;

        if (protect == 1 || (protect == 2 && ent != attacker))
            take = 0;
    }

    client->pers.inventory[index] -= take;
    SpawnDamage(te_sparks, point, normal, take);

    return save;
}

static void M_ReactToDamage(edict_t *targ, edict_t *attacker, edict_t *inflictor)
{
    // pmm
    bool new_tesla;

    if (!(attacker->client) && !(attacker->svflags & SVF_MONSTER))
        return;

//=======
//ROGUE
    // logic for tesla - if you are hit by a tesla, and can't see who you should be mad at (attacker)
    // attack the tesla
    // also, target the tesla if it's a "new" tesla
    if ((inflictor) && (!strcmp(inflictor->classname, "tesla"))) {
        new_tesla = MarkTeslaArea(targ, inflictor);
        if (new_tesla)
            TargetTesla(targ, inflictor);
        return;
        // FIXME - just ignore teslas when you're TARGET_ANGER or MEDIC
        /*      if (!(targ->enemy && (targ->monsterinfo.aiflags & (AI_TARGET_ANGER|AI_MEDIC))))
                {
                    // FIXME - coop issues?
                    if ((!targ->enemy) || (!visible(targ, targ->enemy)))
                    {
                        gi.dprintf ("can't see player, switching to tesla\n");
                        TargetTesla (targ, inflictor);
                        return;
                    }
                    gi.dprintf ("can see player, ignoring tesla\n");
                }
                else if ((g_showlogic) && (g_showlogic->value))
                    gi.dprintf ("no enemy, or I'm doing other, more important things, than worrying about a damned tesla!\n");
        */
    }
//ROGUE
//=======

    if (attacker == targ || attacker == targ->enemy)
        return;

    // if we are a good guy monster and our attacker is a player
    // or another good guy, do not get mad at them
    if (targ->monsterinfo.aiflags & AI_GOOD_GUY) {
        if (attacker->client || (attacker->monsterinfo.aiflags & AI_GOOD_GUY))
            return;
    }

//PGM
    // if we're currently mad at something a target_anger made us mad at, ignore
    // damage
    if (targ->enemy && targ->monsterinfo.aiflags & AI_TARGET_ANGER) {
        float   percentHealth;

        // make sure whatever we were pissed at is still around.
        if (targ->enemy->inuse) {
            percentHealth = (float)(targ->health) / (float)(targ->max_health);
            if (targ->enemy->inuse && percentHealth > 0.33f)
                return;
        }

        // remove the target anger flag
        targ->monsterinfo.aiflags &= ~AI_TARGET_ANGER;
    }
//PGM

// PMM
// if we're healing someone, do like above and try to stay with them
    if ((targ->enemy) && (targ->monsterinfo.aiflags & AI_MEDIC)) {
        float   percentHealth;

        percentHealth = (float)(targ->health) / (float)(targ->max_health);
        // ignore it some of the time
        if (targ->enemy->inuse && percentHealth > 0.25f)
            return;

        // remove the medic flag
        targ->monsterinfo.aiflags &= ~AI_MEDIC;
        cleanupHealTarget(targ->enemy);
    }
// PMM

    // we now know that we are not both good guys

    // if attacker is a client, get mad at them because he's good and we're not
    if (attacker->client) {
        targ->monsterinfo.aiflags &= ~AI_SOUND_TARGET;

        // this can only happen in coop (both new and old enemies are clients)
        // only switch if can't see the current enemy
        if (targ->enemy && targ->enemy->client) {
            if (visible(targ, targ->enemy)) {
                targ->oldenemy = attacker;
                return;
            }
            targ->oldenemy = targ->enemy;
        }
        targ->enemy = attacker;
        if (!(targ->monsterinfo.aiflags & AI_DUCKED))
            FoundTarget(targ);
        return;
    }

    // it's the same base (walk/swim/fly) type and a different classname and it's not a tank
    // (they spray too much), get mad at them
    // PMM
    // added medics to this
    // FIXME -
    // this really should be turned into an AI flag marking appropriate monsters as "don't shoot me"
    //   this also leads to the problem of tanks and medics being able to, at will, kill monsters with
    //   no chance of retaliation.  My vote is to make those monsters who are designed as "don't shoot me"
    //   such that they also ignore being shot by monsters as well
    /*
    if (((targ->flags & (FL_FLY|FL_SWIM)) == (attacker->flags & (FL_FLY|FL_SWIM))) &&
         (strcmp (targ->classname, attacker->classname) != 0) &&
         (strcmp(attacker->classname, "monster_tank") != 0) &&
         (strcmp(attacker->classname, "monster_supertank") != 0) &&
         (strcmp(attacker->classname, "monster_makron") != 0) &&
         (strcmp(attacker->classname, "monster_jorg") != 0) &&
         (strcmp(attacker->classname, "monster_carrier") != 0) &&
         (strncmp(attacker->classname, "monster_medic", 12) != 0) ) // this should get medics & medic_commanders
    */
    if (((targ->flags & (FL_FLY | FL_SWIM)) == (attacker->flags & (FL_FLY | FL_SWIM))) &&
        (strcmp(targ->classname, attacker->classname) != 0) &&
        !(attacker->monsterinfo.aiflags & AI_IGNORE_SHOTS) &&
        !(targ->monsterinfo.aiflags & AI_IGNORE_SHOTS)) {
        if (targ->enemy && targ->enemy->client)
            targ->oldenemy = targ->enemy;
        targ->enemy = attacker;
        if (!(targ->monsterinfo.aiflags & AI_DUCKED))
            FoundTarget(targ);
    }
    // if they *meant* to shoot us, then shoot back
    else if (attacker->enemy == targ) {
        if (targ->enemy && targ->enemy->client)
            targ->oldenemy = targ->enemy;
        targ->enemy = attacker;
        if (!(targ->monsterinfo.aiflags & AI_DUCKED))
            FoundTarget(targ);
    }
    // otherwise get mad at whoever they are mad at (help our buddy) unless it is us!
    else if (attacker->enemy && attacker->enemy != targ) {
        if (targ->enemy && targ->enemy->client)
            targ->oldenemy = targ->enemy;
        targ->enemy = attacker->enemy;
        if (!(targ->monsterinfo.aiflags & AI_DUCKED))
            FoundTarget(targ);
    }
}

// Non-static: CTF's grapple asks the same question before hurting whoever it
// hit, so the linkage follows the call (R-CORE-13).
bool CheckTeamDamage(edict_t *targ, edict_t *attacker)
{
    // CTF: teammates cannot hurt each other at all.
    if (G_Ruleset() == RULESET_CTF && targ->client && attacker->client)
        if (targ->client->resp.ctf_team == attacker->client->resp.ctf_team &&
            targ != attacker)
            return true;

    //FIXME make the next line real and uncomment this block
    // if ((ability to damage a teammate == OFF) && (targ's team == attacker's team))
    return false;
}

void T_Damage(edict_t *targ, edict_t *inflictor, edict_t *attacker, const vec3_t dir, vec3_t point, const vec3_t normal, int damage, int knockback, int dflags, int mod)
{
    gclient_t   *client;
    int         take;
    int         save;
    int         asave;
    int         psave;
    int         te_sparks;
    int         sphere_notified;    // PGM

    if (!targ->takedamage)
        return;

    // An attacker is an edict, never NULL, and this is where that becomes true.
    //
    // baseq2 leaves `activator` NULL on the way into damage: nothing in
    // g_func.c ever assigns that field on a func_door, so a door reversing
    // because something blocked it passes `ent->activator` to its targets, and
    // a `target_explosion` among them hands it straight to T_RadiusDamage as
    // the attacker.  id's own T_Damage survives that by accident rather than by
    // design -- its single `attacker->client` read sits behind
    // `!(dflags & DAMAGE_RADIUS)`, which short-circuits on exactly the path
    // that produces the NULL.  Killed() and M_ReactToDamage() have no such
    // luck: both dereference the attacker to decide what to do with it.
    //
    // An accident is not a contract, and this function is where every donor's
    // damage rules are going to land.  So the NULL is answered once, at the
    // boundary, rather than at each read.  `world` is g_edicts[0]; its `client`
    // is NULL, so every `attacker->client` test still answers exactly what the
    // missing attacker meant.
    if (!attacker)
        attacker = world;

    sphere_notified = false;        // PGM

    // easy mode takes half damage
    if (skill->value == 0 && deathmatch->value == 0 && targ->client) {
        damage *= 0.5f;
        if (!damage)
            damage = 1;
    }

    // friendly fire avoidance
    // if enabled you can't hurt teammates (but you can hurt yourself)
    // knockback still occurs
    if ((targ != attacker) && ((deathmatch->value && ((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS))) || (coop->value && targ->client))) {
        if (OnSameTeam(targ, attacker)) {
            // PMM - nukes kill everyone
            if (((int)(dmflags->value) & DF_NO_FRIENDLY_FIRE) && (mod != MOD_NUKE))
                damage = 0;
            else
                mod |= MOD_FRIENDLY_FIRE;
        }
    }
    if (G_Ruleset() == RULESET_ARENA && attacker->client && (attacker != targ))
        targ->enemy = attacker;

    // R-OSP-1: tourney decides friendly fire and self damage per TEAM and per
    // match mode rather than from dmflags, because both are things a referee
    // changes mid-match.  It is an addition to the block above, not a
    // replacement for it: the donor deleted baseq2's easy-mode halving and its
    // dmflags friendly-fire arm outright, and R-CORE-8 keeps both -- under
    // every other ruleset they are the only rules there are, and under tourney
    // dmflags teamplay is off, so the inherited arm cannot fire.
    if (G_Ruleset() == RULESET_TOURNEY && targ->client) {
        // A client who has not finished entering is not in the match yet, and
        // is where the donor returns rather than damaging.
        if (targ->client->resp.osp_entered != ENTERED_ENTERED)
            return;

        if (m_mode > 1 && targ != attacker && attacker->client &&
            targ->client->resp.team == attacker->client->resp.team) {
            if (!OSP_teamFriendlyFire(targ->client->resp.team) &&
                !(dflags & DAMAGE_NO_PROTECTION))
                damage = 0;
            else
                mod |= MOD_FRIENDLY_FIRE;
        }

        if (targ == attacker) {
            if (m_mode > 1) {
                if (!OSP_teamSelfDamage(targ->client->resp.team))
                    damage = 0;
            } else if (!(int)ffa_hurtself->value) {
                damage = 0;
            }
        }
    }

    meansOfDeath = mod;

    // R-OSP-1's strength rune multiplies the damage the CARRIER deals, which
    // is why it reads the attacker and runs before any of the target's
    // reductions.  A no-op unless the rune set is on and the attacker has it,
    // in the same shape as CTFApplyStrength below.
    if (G_Ruleset() == RULESET_TOURNEY && (rune_stat & RUNE_STRENGTH))
        damage = OSP_runesApplyStrength(attacker, damage);

//ROGUE
    // allow the deathmatch game to change values
    if (deathmatch->value && gamerules && gamerules->value) {
        if (DMGame.ChangeDamage)
            damage = DMGame.ChangeDamage(targ, attacker, damage, mod);
        if (DMGame.ChangeKnockback)
            knockback = DMGame.ChangeKnockback(targ, attacker, knockback, mod);

        if (!damage)
            return;
    }
//ROGUE

    client = targ->client;

    // PMM - defender sphere takes half damage
    if ((client) && (client->owned_sphere) && (client->owned_sphere->spawnflags == 1)) {
        damage *= 0.5f;
        if (!damage)
            damage = 1;
    }

    if (dflags & DAMAGE_BULLET)
        te_sparks = TE_BULLET_SPARKS;
    else
        te_sparks = TE_SPARKS;

// bonus damage for suprising a monster
    if (!(dflags & DAMAGE_RADIUS) && (targ->svflags & SVF_MONSTER) && (attacker->client) && (!targ->enemy) && (targ->health > 0))
        damage *= 2;

    // CTF strength tech.  A no-op without the tech, so no gate: CTFApplyStrength
    // returns its argument when the attacker holds nothing.  "Holds nothing"
    // and "is not there" are different states, and only the first is this
    // call's to answer -- the second is answered once at the top of T_Damage,
    // and a gate here instead would leave it unanswered for
    // CTFCheckHurtCarrier() below and for M_ReactToDamage(), which is id's own
    // and runs whatever the ruleset is.
    damage = CTFApplyStrength(attacker, damage);

    if (targ->flags & FL_NO_KNOCKBACK)
        knockback = 0;

// figure momentum add
    if (!(dflags & DAMAGE_NO_KNOCKBACK)) {
        if ((knockback) && (targ->movetype != MOVETYPE_NONE) && (targ->movetype != MOVETYPE_BOUNCE) && (targ->movetype != MOVETYPE_PUSH) && (targ->movetype != MOVETYPE_STOP)) {
            vec3_t  kvel;
            float   mass;

            if (targ->mass < 50)
                mass = 50;
            else
                mass = targ->mass;

            VectorNormalize2(dir, kvel);

            if (targ->client  && attacker == targ)
                VectorScale(kvel, 1600.0f * (float)knockback / mass, kvel);  // the rocket jump hack...
            else
                VectorScale(kvel, 500.0f * (float)knockback / mass, kvel);

            VectorAdd(targ->velocity, kvel, targ->velocity);
        }
    }

    take = damage;
    save = 0;

    // check for godmode
    if ((targ->flags & FL_GODMODE) && !(dflags & DAMAGE_NO_PROTECTION)) {
        take = 0;
        save = damage;
        SpawnDamage(te_sparks, point, normal, save);
    }

    // check for invincibility
    if ((client && client->invincible_framenum > level.framenum) && !(dflags & DAMAGE_NO_PROTECTION) && mod != MOD_TRAP) {
        if (targ->pain_debounce_framenum < level.framenum) {
            gi.sound(targ, CHAN_ITEM, gi.soundindex("items/protect4.wav"), 1, ATTN_NORM, 0);
            targ->pain_debounce_framenum = level.framenum + 2 * BASE_FRAMERATE;
        }
        take = 0;
        save = damage;
    }
    // ROGUE
    // check for monster invincibility
    if (((targ->svflags & SVF_MONSTER) && targ->monsterinfo.invincible_framenum > level.framenum) && !(dflags & DAMAGE_NO_PROTECTION)) {
        if (targ->pain_debounce_framenum < level.framenum) {
            gi.sound(targ, CHAN_ITEM, gi.soundindex("items/protect4.wav"), 1, ATTN_NORM, 0);
            targ->pain_debounce_framenum = level.framenum + 2 * BASE_FRAMERATE;
        }
        take = 0;
        save = damage;
    }
    // ROGUE

    // CTF's DF_ARMOR_PROTECT: with the dmflag set, a teammate's shot does not
    // eat your armour either.  Gated on the ruleset because the dmflags bit
    // itself is CTF's (0x40000) and means nothing elsewhere.
    if (G_Ruleset() == RULESET_CTF && targ->client && attacker->client &&
        targ->client->resp.ctf_team == attacker->client->resp.ctf_team &&
        targ != attacker && ((int)dmflags->value & DF_ARMOR_PROTECT)) {
        psave = asave = 0;
    } else {
        psave = CheckPowerArmor(targ, point, normal, take, dflags);
        take -= psave;

        asave = CheckArmor(targ, point, normal, take, te_sparks, dflags, attacker);
        take -= asave;
    }

    //treat cheat/powerup savings the same as armor
    asave += save;

    // R-ARENA-1: `healthprotect`, the health half of RA2's friendly fire, in
    // the donor's own position -- after the armour has had its say and before
    // anything is taken off.  1 means nobody on your team takes health off you
    // and OnSameTeam answers true for yourself, so that covers your own splash
    // too; 2 exempts only a team-mate.  Both are per-arena settings from
    // arena.cfg, defaulting to armorprotect 2 / healthprotect 1.
    //
    // This does NOT replace baseq2's dmflags friendly-fire arm at the top of
    // the function (R-CORE-8): that is a different rule with a different
    // switch, and under arena its dmflags gate is off, so the two never both
    // fire.  meansOfDeath is rewritten rather than only `mod`, because
    // colosseum publishes it before this point and Killed() reads the global.
    if (G_Ruleset() == RULESET_ARENA && targ->client &&
        !(dflags & DAMAGE_NO_PROTECTION) && OnSameTeam(targ, attacker)) {
        int protect = arenas[targ->client->resp.context].healthprotect;

        if (protect == 1 || (protect == 2 && targ != attacker))
            return;
        mod |= MOD_FRIENDLY_FIRE;
        meansOfDeath = mod;
    }

    // CTF resistance tech.  A no-op without it, like the strength tech above.
    take = CTFApplyResistance(targ, take);

    // Tourney's equivalent pair, and they are one block in the donor because
    // vampire is fed by what resistance let through: the attacker heals from
    // the damage that actually landed, capped at 40 so that a gib does not pay
    // out the overkill.
    if (G_Ruleset() == RULESET_TOURNEY && (rune_stat & (RUNE_RESIST | RUNE_VAMPIRE))) {
        take = OSP_runesApplyResistance(targ, take);
        if (targ != attacker)
            OSP_runesApplyVampire(attacker,
                                  targ->health - take < -40 ? 40 : take);
    }

    // team damage avoidance
    if (!(dflags & DAMAGE_NO_PROTECTION) && CheckTeamDamage(targ, attacker))
        return;

    // CTF: hurting a flag carrier's escort earns the carrier's killer a bonus
    // later.  Records the fact; the scoring happens in CTFFragBonuses.
    //
    // Gated, where the donor's call is not, and the gate is this project's
    // decision rather than the merge's.  The only reader of
    // `ctf_lasthurtcarrier` is CTFFragBonuses, which returns at
    // `CTFOtherTeam(...) < 0` before reaching it unless the victim is on a real
    // CTF team, so outside ctf this call wrote a field nobody read.  The donor
    // could leave it ungated because its library only ever ran CTF; this one
    // runs every ruleset from one binary, and an ungated donor entry point in a
    // shared T_Damage is exactly how a CTF rule reaches a deathmatch server.
    if (G_Ruleset() == RULESET_CTF)
        CTFCheckHurtCarrier(targ, attacker);

// ROGUE - this option will do damage both to the armor and person. originally for DPU rounds
    if (dflags & DAMAGE_DESTROY_ARMOR) {
        if (!(targ->flags & FL_GODMODE) && !(dflags & DAMAGE_NO_PROTECTION) &&
            !(client && client->invincible_framenum > level.framenum)) {
            take = damage;
        }
    }
// ROGUE

    // R-OSP-1's accuracy table, credited once, here, rather than at fifteen
    // sites across g_weapon.c and g_combat.c -- see src/tourney/osp_acc.c for
    // what that changes and why.  This is the point every one of those sites
    // was feeding: `take` is the damage that survived armour, powerups and the
    // team rules, which is what the report means by damage given and taken.
    if (G_Ruleset() == RULESET_TOURNEY)
        OSP_accDamage(targ, attacker, mod, take);

// do the damage
    if (take) {
        // Three damage-effect rules union here (§7 rule 3): baseq2's blood,
        // Xatrix's green gekk blood, and Ground Zero's mechanical sparks and
        // chainfist extra blood.  They compose -- each tests a different thing
        // about the target or the weapon -- so no gate is needed, only ordering
        // from most specific to least.
        if (targ->flags & FL_MECHANICAL) {              // ROGUE
            SpawnDamage(TE_ELECTRIC_SPARKS, point, normal, take);
        } else if ((targ->svflags & SVF_MONSTER) || (client)) {
            if (strcmp(targ->classname, "monster_gekk") == 0)   // XATRIX
                SpawnDamage(TE_GREENBLOOD, point, normal, take);
            else if (mod == MOD_CHAINFIST)              // ROGUE
                SpawnDamage(TE_MOREBLOOD, point, normal, 255);
            else
                SpawnDamage(TE_BLOOD, point, normal, take);
        } else
            SpawnDamage(te_sparks, point, normal, take);
//PGM

        // R-RA-1's score-by-damage, a per-arena setting.  Scored before the
        // health is applied, because the points are what this hit actually took
        // off rather than what it asked for.
        if (G_Ruleset() == RULESET_ARENA &&
            targ->client && attacker->client && (attacker != targ) &&
            !OnSameTeam(targ, attacker) &&
            arenas[attacker->client->resp.context].scorebydamage) {
            int hit = targ->health < take ? targ->health : take;
            int points = (asave < 0 ? 0 : asave) + (hit < 0 ? 0 : hit);

            attacker->client->resp.damagedealt += points > 500 ? 100 : points;
            attacker->client->resp.score = attacker->client->resp.damagedealt / 100;

            RA2_Stats_Set(arenas[attacker->client->resp.context].stats,
                          attacker - g_edicts, RA2_STAT_SCORE,
                          attacker->client->resp.score);
        }

        // CTF match setup freezes the players while captains pick teams, so no
        // damage lands.  CTFMatchSetup() is false in every other ruleset.
        if (!CTFMatchSetup())
            targ->health = targ->health - take;

//PGM - spheres need to know who to shoot at
        if (client && client->owned_sphere) {
            sphere_notified = true;
            if (client->owned_sphere->pain)
                client->owned_sphere->pain(client->owned_sphere, attacker, 0, 0);
        }
//PGM

        if (targ->health <= 0) {
            if ((targ->svflags & SVF_MONSTER) || (client))
                targ->flags |= FL_NO_KNOCKBACK;
            Killed(targ, inflictor, attacker, take, point);
            return;
        }
    }

//PGM - spheres need to know who to shoot at
    if (!sphere_notified) {
        if (client && client->owned_sphere) {
            sphere_notified = true;
            if (client->owned_sphere->pain)
                client->owned_sphere->pain(client->owned_sphere, attacker, 0, 0);
        }
    }
//PGM

    if (targ->svflags & SVF_MONSTER) {
        M_ReactToDamage(targ, attacker, inflictor);
        // PMM - fixme - if anyone else but the medic ever uses AI_MEDIC, check for it here instead
        // of in the medic's pain function
        if (!(targ->monsterinfo.aiflags & AI_DUCKED) && (take)) {
            targ->pain(targ, attacker, knockback, take);
            // nightmare mode monsters don't go into pain frames often
            if (skill->value == 3)
                targ->pain_debounce_framenum = level.framenum + 5 * BASE_FRAMERATE;
        }
    } else if (client) {
        if (!(targ->flags & FL_GODMODE) && (take) && !CTFMatchSetup())
            targ->pain(targ, attacker, knockback, take);
    } else if (take) {
        if (targ->pain)
            targ->pain(targ, attacker, knockback, take);
    }

    // add to the damage inflicted on a player this frame
    // the total will be turned into screen blends and view angle kicks
    // at the end of the frame
    if (client) {
        client->damage_parmor += psave;
        client->damage_armor += asave;
        client->damage_blood += take;
        client->damage_knockback += knockback;
        VectorCopy(point, client->damage_from);
    }
}

/*
============
T_RadiusDamage
============
*/
void T_RadiusDamage(edict_t *inflictor, edict_t *attacker, float damage, edict_t *ignore, float radius, int mod)
{
    float   points;
    edict_t *ent = NULL;
    vec3_t  v;
    vec3_t  dir;

    while ((ent = findradius(ent, inflictor->s.origin, radius)) != NULL) {
        if (ent == ignore)
            continue;
        if (!ent->takedamage)
            continue;

        VectorAvg(ent->mins, ent->maxs, v);
        VectorAdd(ent->s.origin, v, v);
        VectorSubtract(inflictor->s.origin, v, v);
        points = damage - 0.5f * VectorLength(v);
        if (ent == attacker)
            points = points * 0.5f;
        if (points > 0) {
            if (CanDamage(ent, inflictor)) {
                VectorSubtract(ent->s.origin, inflictor->s.origin, dir);
                T_Damage(ent, inflictor, attacker, dir, inflictor->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, mod);
            }
        }
    }
}

// **********************
// ROGUE

/*
============
T_RadiusNukeDamage

Like T_RadiusDamage, but ignores walls (skips CanDamage check, among others)
// up to KILLZONE radius, do 10,000 points
// after that, do damage linearly out to KILLZONE2 radius
============
*/

void T_RadiusNukeDamage(edict_t *inflictor, edict_t *attacker, float damage, edict_t *ignore, float radius, int mod)
{
    float   points;
    edict_t *ent = NULL;
    vec3_t  v;
    vec3_t  dir;
    float   len;
    float   killzone, killzone2;
    trace_t tr;
    float   dist;

    killzone = radius;
    killzone2 = radius * 2.0f;

    while ((ent = findradius(ent, inflictor->s.origin, killzone2)) != NULL) {
// ignore nobody
        if (ent == ignore)
            continue;
        if (!ent->takedamage)
            continue;
        if (!ent->inuse)
            continue;
        if (!(ent->client || (ent->svflags & SVF_MONSTER) || (ent->svflags & SVF_DAMAGEABLE)))
            continue;

        VectorAdd(ent->mins, ent->maxs, v);
        VectorMA(ent->s.origin, 0.5f, v, v);
        VectorSubtract(inflictor->s.origin, v, v);
        len = VectorLength(v);
        if (len <= killzone) {
            if (ent->client)
                ent->flags |= FL_NOGIB;
            points = 10000;
        } else if (len <= killzone2)
            points = (damage / killzone) * (killzone2 - len);
        else
            points = 0;
//      points = damage - 0.005 * len*len;
//      if (ent == attacker)
//          points = points * 0.5;
//      if ((g_showlogic) && (g_showlogic->value))
//      {
//          if (!(strcmp(ent->classname, "player")))
//              gi.dprintf ("dist = %2.2f doing %6.0f damage to %s\n", len, points, inflictor->teammaster->client->pers.netname);
//          else
//              gi.dprintf ("dist = %2.2f doing %6.0f damage to %s\n", len, points, ent->classname);
//      }
        if (points > 0) {
//          if (CanDamage (ent, inflictor))
//          {
            if (ent->client)
                ent->client->nuke_framenum = level.framenum + 20;
            VectorSubtract(ent->s.origin, inflictor->s.origin, dir);
            T_Damage(ent, inflictor, attacker, dir, inflictor->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, mod);
//          }
        }
    }
    ent = g_edicts + 1; // skip the worldspawn
    // cycle through players
    while (ent) {
        if ((ent->client) && (ent->client->nuke_framenum != level.framenum + 20) && (ent->inuse)) {
            tr = gi.trace(inflictor->s.origin, NULL, NULL, ent->s.origin, inflictor, MASK_SOLID);
            if (tr.fraction == 1.0f) {
//              if ((g_showlogic) && (g_showlogic->value))
//                  gi.dprintf ("Undamaged player in LOS with nuke, flashing!\n");
                ent->client->nuke_framenum = level.framenum + 20;
            } else {
                dist = realrange(ent, inflictor);
                if (dist < 2048)
                    ent->client->nuke_framenum = max(ent->client->nuke_framenum, level.framenum + 15);
                else
                    ent->client->nuke_framenum = max(ent->client->nuke_framenum, level.framenum + 10);
            }
            ent++;
        } else
            ent = NULL;
    }
}

/*
============
T_RadiusClassDamage

Like T_RadiusDamage, but ignores anything with classname=ignoreClass
============
*/
void T_RadiusClassDamage(edict_t *inflictor, edict_t *attacker, float damage, char *ignoreClass, float radius, int mod)
{
    float   points;
    edict_t *ent = NULL;
    vec3_t  v;
    vec3_t  dir;

    while ((ent = findradius(ent, inflictor->s.origin, radius)) != NULL) {
        if (ent->classname && !strcmp(ent->classname, ignoreClass))
            continue;
        if (!ent->takedamage)
            continue;

        VectorAdd(ent->mins, ent->maxs, v);
        VectorMA(ent->s.origin, 0.5f, v, v);
        VectorSubtract(inflictor->s.origin, v, v);
        points = damage - 0.5f * VectorLength(v);
        if (ent == attacker)
            points = points * 0.5f;
        if (points > 0) {
            if (CanDamage(ent, inflictor)) {
                VectorSubtract(ent->s.origin, inflictor->s.origin, dir);
                T_Damage(ent, inflictor, attacker, dir, inflictor->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, mod);
            }
        }
    }
}

// ROGUE
// ********************
