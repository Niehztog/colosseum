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

// RotatePointAroundVector() lives in the engine's math.c, which the game library
// does not link against; the mission pack needs its own copy.
static void SetupRotationMatrix(vec3_t matrix[3], const vec3_t dir, float degrees)
{
    vec_t   angle, s, c, one_c, xx, yy, zz, xy, yz, zx, xs, ys, zs;

    angle = DEG2RAD(degrees);
    s = sinf(angle);
    c = cosf(angle);
    one_c = 1.0F - c;

    xx = dir[0] * dir[0];
    yy = dir[1] * dir[1];
    zz = dir[2] * dir[2];
    xy = dir[0] * dir[1];
    yz = dir[1] * dir[2];
    zx = dir[2] * dir[0];
    xs = dir[0] * s;
    ys = dir[1] * s;
    zs = dir[2] * s;

    matrix[0][0] = (one_c * xx) + c;
    matrix[0][1] = (one_c * xy) - zs;
    matrix[0][2] = (one_c * zx) + ys;

    matrix[1][0] = (one_c * xy) + zs;
    matrix[1][1] = (one_c * yy) + c;
    matrix[1][2] = (one_c * yz) - xs;

    matrix[2][0] = (one_c * zx) - ys;
    matrix[2][1] = (one_c * yz) + xs;
    matrix[2][2] = (one_c * zz) + c;
}

static void RotatePointAroundVector(vec3_t out, const vec3_t dir, const vec3_t in, float degrees)
{
    vec3_t matrix[3];
    vec3_t temp;

    SetupRotationMatrix(matrix, dir, degrees);

    VectorCopy(in, temp);
    VectorRotate(temp, matrix, out);
}


/*
=================
check_dodge

This is a support routine used when a client is firing
a non-instant attack weapon.  It checks to see if a
monster's dodge function should be called.
=================
*/
//static void check_dodge (edict_t *self, vec3_t start, vec3_t dir, int speed)
void check_dodge(edict_t *self, vec3_t start, vec3_t dir, int speed)        //PGM
{
    vec3_t  end;
    vec3_t  v;
    trace_t tr;
    float   eta;

    // easy mode only ducks one quarter the time
    if (skill->value == 0 && random() > 0.25f)
        return;
    VectorMA(start, 8192, dir, end);
    tr = gi.trace(start, NULL, NULL, end, self, MASK_SHOT);
    if ((tr.ent) && (tr.ent->svflags & SVF_MONSTER) && (tr.ent->health > 0) && (tr.ent->monsterinfo.dodge) && infront(tr.ent, self)) {
        VectorSubtract(tr.endpos, start, v);
        eta = (VectorLength(v) - tr.ent->maxs[0]) / speed;
        // Ground Zero's dodge takes the trace: its dodge AI needs the impact
        // point to decide which way to duck (adopting Rogue's
        // monsterinfo_t means adopting its signature, at every call site).
        tr.ent->monsterinfo.dodge(tr.ent, self, eta, &tr);
    }
}

/*
=================
fire_hit

Used for all impact (hit/punch/slash) attacks
=================
*/
bool fire_hit(edict_t *self, vec3_t aim, int damage, int kick)
{
    trace_t     tr;
    vec3_t      forward, right, up;
    vec3_t      v;
    vec3_t      point;
    float       range;
    vec3_t      dir;

    //see if enemy is in range
    VectorSubtract(self->enemy->s.origin, self->s.origin, dir);
    range = VectorLength(dir);
    if (range > aim[0])
        return false;

    if (aim[1] > self->mins[0] && aim[1] < self->maxs[0]) {
        // the hit is straight on so back the range up to the edge of their bbox
        range -= self->enemy->maxs[0];
    } else {
        // this is a side hit so adjust the "right" value out to the edge of their bbox
        if (aim[1] < 0)
            aim[1] = self->enemy->mins[0];
        else
            aim[1] = self->enemy->maxs[0];
    }

    VectorMA(self->s.origin, range, dir, point);

    tr = gi.trace(self->s.origin, NULL, NULL, point, self, MASK_SHOT);
    if (tr.fraction < 1) {
        if (!tr.ent->takedamage)
            return false;
        // if it will hit any client/monster then hit the one we wanted to hit
        if ((tr.ent->svflags & SVF_MONSTER) || (tr.ent->client))
            tr.ent = self->enemy;
    }

    AngleVectors(self->s.angles, forward, right, up);
    VectorMA(self->s.origin, range, forward, point);
    VectorMA(point, aim[1], right, point);
    VectorMA(point, aim[2], up, point);
    VectorSubtract(point, self->enemy->s.origin, dir);

    // do the damage
    T_Damage(tr.ent, self, self, dir, point, vec3_origin, damage, kick / 2, DAMAGE_NO_KNOCKBACK, MOD_HIT);

    if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client))
        return false;

    // do our special form of knockback here
    VectorMA(self->enemy->absmin, 0.5f, self->enemy->size, v);
    VectorSubtract(v, point, v);
    VectorNormalize(v);
    VectorMA(self->enemy->velocity, kick, v, self->enemy->velocity);
    if (self->enemy->velocity[2] > 0)
        self->enemy->groundentity = NULL;
    return true;
}

/*
=================
fire_lead

This is an internal support routine used for bullet/pellet based weapons.
=================
*/
static void fire_lead(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int te_impact, int hspread, int vspread, int mod)
{
    trace_t     tr;
    vec3_t      dir;
    vec3_t      forward, right, up;
    vec3_t      end;
    float       r;
    float       u;
    vec3_t      water_start;
    bool        water = false;
    int         content_mask = MASK_SHOT | MASK_WATER;

    tr = gi.trace(self->s.origin, NULL, NULL, start, self, MASK_SHOT);
    if (!(tr.fraction < 1.0f)) {
        vectoangles(aimdir, dir);
        AngleVectors(dir, forward, right, up);

        r = crandom() * hspread;
        u = crandom() * vspread;
        VectorMA(start, 8192, forward, end);
        VectorMA(end, r, right, end);
        VectorMA(end, u, up, end);

        if (gi.pointcontents(start) & MASK_WATER) {
            water = true;
            VectorCopy(start, water_start);
            content_mask &= ~MASK_WATER;
        }

        tr = gi.trace(start, NULL, NULL, end, self, content_mask);

        // see if we hit water
        if (tr.contents & MASK_WATER) {
            int     color;

            water = true;
            VectorCopy(tr.endpos, water_start);

            if (!VectorCompare(start, tr.endpos)) {
                if (tr.contents & CONTENTS_WATER) {
                    if (strcmp(tr.surface->name, "*brwater") == 0)
                        color = SPLASH_BROWN_WATER;
                    else
                        color = SPLASH_BLUE_WATER;
                } else if (tr.contents & CONTENTS_SLIME)
                    color = SPLASH_SLIME;
                else if (tr.contents & CONTENTS_LAVA)
                    color = SPLASH_LAVA;
                else
                    color = SPLASH_UNKNOWN;

                if (color != SPLASH_UNKNOWN) {
                    gi.WriteByte(svc_temp_entity);
                    gi.WriteByte(TE_SPLASH);
                    gi.WriteByte(8);
                    gi.WritePosition(tr.endpos);
                    gi.WriteDir(tr.plane.normal);
                    gi.WriteByte(color);
                    gi.multicast(tr.endpos, MULTICAST_PVS);
                }

                // change bullet's course when it enters water
                VectorSubtract(end, start, dir);
                vectoangles(dir, dir);
                AngleVectors(dir, forward, right, up);
                r = crandom() * hspread * 2;
                u = crandom() * vspread * 2;
                VectorMA(water_start, 8192, forward, end);
                VectorMA(end, r, right, end);
                VectorMA(end, u, up, end);
            }

            // re-trace ignoring water this time
            tr = gi.trace(water_start, NULL, NULL, end, self, MASK_SHOT);
        }
    }

    // send gun puff / flash
    if (!((tr.surface) && (tr.surface->flags & SURF_SKY))) {
        if (tr.fraction < 1.0f) {
            if (tr.ent->takedamage) {
                T_Damage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal, damage, kick, DAMAGE_BULLET, mod);
            } else {
                if (strncmp(tr.surface->name, "sky", 3) != 0) {
                    gi.WriteByte(svc_temp_entity);
                    gi.WriteByte(te_impact);
                    gi.WritePosition(tr.endpos);
                    gi.WriteDir(tr.plane.normal);
                    gi.multicast(tr.endpos, MULTICAST_PVS);

                    if (self->client)
                        PlayerNoise(self, tr.endpos, PNOISE_IMPACT);
                }
            }
        }
    }

    // if went through water, determine where the end and make a bubble trail
    if (water) {
        vec3_t  pos;

        VectorSubtract(tr.endpos, water_start, dir);
        VectorNormalize(dir);
        VectorMA(tr.endpos, -2, dir, pos);
        if (gi.pointcontents(pos) & MASK_WATER)
            VectorCopy(pos, tr.endpos);
        else
            tr = gi.trace(pos, NULL, NULL, water_start, tr.ent, MASK_WATER);

        VectorAdd(water_start, tr.endpos, pos);
        VectorScale(pos, 0.5f, pos);

        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_BUBBLETRAIL);
        gi.WritePosition(water_start);
        gi.WritePosition(tr.endpos);
        gi.multicast(pos, MULTICAST_PVS);
    }
}

/*
=================
fire_bullet

Fires a single round.  Used for machinegun and chaingun.  Would be fine for
pistols, rifles, etc....
=================
*/
void fire_bullet(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int mod)
{
    // Tourney's accuracy report counts shots fired as well as shots that hit,
    // and only the weapon knows it fired.  One line per weapon here, against
    // fifteen inline p_acc[] writes in the donor -- src/tourney/osp_acc.c has
    // the reasoning.  Damage credit is not here: it is one hook in T_Damage.
    if (G_IsOspRuleset())
        OSP_accShot(self, mod, 1);

    fire_lead(self, start, aimdir, damage, kick, TE_GUNSHOT, hspread, vspread, mod);
}

/*
=================
fire_shotgun

Shoots shotgun pellets.  Used by shotgun and super shotgun.
=================
*/
void fire_shotgun(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, int mod)
{
    if (G_IsOspRuleset())
        OSP_accShot(self, mod, count);

    int     i;

    for (i = 0; i < count; i++)
        fire_lead(self, start, aimdir, damage, kick, TE_SHOTGUN, hspread, vspread, mod);
}

/*
=================
fire_blaster

Fires a single blaster bolt.  Used by the blaster and hyper blaster.
=================
*/
void blaster_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    vec_t   *normal = NULL;
    int     mod;

    if (other == self->owner)
        return;

    if (!other->solid)
        return;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(self);
        return;
    }

    // PMM - crash prevention
    if (self->owner && self->owner->client)
        PlayerNoise(self->owner, self->s.origin, PNOISE_IMPACT);

    if (plane)
        normal = plane->normal;

    if (other->takedamage) {
        if (self->spawnflags & 1)
            mod = MOD_HYPERBLASTER;
        else
            mod = MOD_BLASTER;
        T_Damage(other, self, self->owner, self->velocity, self->s.origin, normal, self->dmg, 1, DAMAGE_ENERGY, mod);
    } else {
        gi.WriteByte(svc_temp_entity);
        // RAFAEL
        //if (self->s.effects & TE_BLUEHYPERBLASTER)
        //  gi.WriteByte (TE_BLUEHYPERBLASTER);
        //else
        gi.WriteByte(TE_BLASTER);
        gi.WritePosition(self->s.origin);
        gi.WriteDir(normal);
        gi.multicast(self->s.origin, MULTICAST_PVS);
    }

    G_FreeEdict(self);
}

void fire_blaster(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int effect, bool hyper)
{
    if (G_IsOspRuleset())
        OSP_accShot(self, hyper ? MOD_HYPERBLASTER : MOD_BLASTER, 1);

    edict_t *bolt;
    trace_t tr;

    VectorNormalize(dir);

    bolt = G_Spawn();
    // Q2PRO'S own divergence between two of its own game libraries:
    // its `ctf` sets `SVF_PROJECTILE` here ("special net code is used for
    // projectiles", `port_ctf:g_weapon.c:320`) and its baseq2, xatrix and
    // rogue all set `SVF_DEADMONSTER`.  Not Threewave's feature, which is why
    // the line was counted among CTF's 167 stale hunks -- but a rule that
    // names 167 hunks has not looked at this one, and under `ctf` upstream
    // really does set it.
    //
    // Both, and not the donor's one, because the two flags are not
    // interchangeable in the engine.  q2pro drops an entity from a trace whose
    // contentmask lacks the matching bit (`sv/world.c:541`), and the
    // SVF_PROJECTILE arm is inside `if (svs.csr.extended)` while the
    // SVF_DEADMONSTER arm is not.  Taking the donor's line verbatim would
    // therefore leave a bolt on a NON-extended server flagged with nothing at
    // all -- solid to MASK_PLAYERSOLID, so a player could walk into a blaster
    // bolt.  Adding it keeps the base behaviour where the engine cannot honour
    // the flag and gains upstream's where it can; SVF_PROJECTILE only ever
    // removes the bolt from more traces, never adds it to one.
    bolt->svflags = SVF_DEADMONSTER;
    if (G_Ruleset() == RULESET_CTF)
        bolt->svflags |= SVF_PROJECTILE;
    // yes, I know it looks weird that projectiles are deadmonsters
    // what this means is that when prediction is used against the object
    // (blaster/hyperblaster shots), the player won't be solid clipped against
    // the object.  Right now trying to run into a firing hyperblaster
    // is very jerky since you are predicted 'against' the shots.
    VectorCopy(start, bolt->s.origin);
    VectorCopy(start, bolt->s.old_origin);
    vectoangles(dir, bolt->s.angles);
    VectorScale(dir, speed, bolt->velocity);
    bolt->movetype = MOVETYPE_FLYMISSILE;
    bolt->clipmask = MASK_SHOT;
    bolt->solid = SOLID_BBOX;
    bolt->s.effects |= effect;
    bolt->s.renderfx |= RF_NOSHADOW;
    VectorClear(bolt->mins);
    VectorClear(bolt->maxs);
    bolt->s.modelindex = gi.modelindex("models/objects/laser/tris.md2");
    bolt->s.sound = gi.soundindex("misc/lasfly.wav");
    bolt->owner = self;
    bolt->touch = blaster_touch;
    bolt->nextthink = level.framenum + 2 * BASE_FRAMERATE;
    bolt->think = G_FreeEdict;
    bolt->dmg = damage;
    bolt->classname = "bolt";
    if (hyper)
        bolt->spawnflags = 1;
    gi.linkentity(bolt);

    if (self->client)
        check_dodge(self, bolt->s.origin, dir, speed);

    tr = gi.trace(self->s.origin, NULL, NULL, bolt->s.origin, bolt, MASK_SHOT);
    if (tr.fraction < 1.0f) {
        VectorMA(bolt->s.origin, -10, dir, bolt->s.origin);
        bolt->touch(bolt, tr.ent, NULL, NULL);
    }
}

// RAFAEL
void fire_blueblaster(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int effect)
{
    edict_t *bolt;
    trace_t tr;

    VectorNormalize(dir);

    bolt = G_Spawn();
    VectorCopy(start, bolt->s.origin);
    VectorCopy(start, bolt->s.old_origin);
    vectoangles(dir, bolt->s.angles);
    VectorScale(dir, speed, bolt->velocity);
    bolt->movetype = MOVETYPE_FLYMISSILE;
    bolt->clipmask = MASK_SHOT;
    bolt->solid = SOLID_BBOX;
    bolt->s.effects |= effect;
    VectorClear(bolt->mins);
    VectorClear(bolt->maxs);

    bolt->s.modelindex = gi.modelindex("models/objects/blaser/tris.md2");
    bolt->s.sound = gi.soundindex("misc/lasfly.wav");
    bolt->owner = self;
    bolt->touch = blaster_touch;
    bolt->nextthink = level.framenum + 2 * BASE_FRAMERATE;
    bolt->think = G_FreeEdict;
    bolt->dmg = damage;
    bolt->classname = "bolt";
    gi.linkentity(bolt);

    if (self->client)
        check_dodge(self, bolt->s.origin, dir, speed);

    tr = gi.trace(self->s.origin, NULL, NULL, bolt->s.origin, bolt, MASK_SHOT);

    if (tr.fraction < 1.0) {
        VectorMA(bolt->s.origin, -10, dir, bolt->s.origin);
        bolt->touch(bolt, tr.ent, NULL, NULL);
    }

}

/*
=================
fire_grenade
=================
*/
//static void Grenade_Explode (edict_t *ent)
void Grenade_Explode(edict_t *ent)
{
    vec3_t      origin;
    int         mod;

    if (ent->owner->client)
        PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);

    //FIXME: if we are onground then raise our Z just a bit since we are a point?
    if (ent->enemy) {
        float   points;
        vec3_t  v;
        vec3_t  dir;

        VectorAvg(ent->enemy->mins, ent->enemy->maxs, v);
        VectorAdd(ent->enemy->s.origin, v, v);
        VectorSubtract(ent->s.origin, v, v);
        points = ent->dmg - 0.5f * VectorLength(v);
        VectorSubtract(ent->enemy->s.origin, ent->s.origin, dir);
        if (ent->spawnflags & 1)
            mod = MOD_HANDGRENADE;
        else
            mod = MOD_GRENADE;
        T_Damage(ent->enemy, ent, ent->owner, dir, ent->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, mod);
    }

    if (ent->spawnflags & 2)
        mod = MOD_HELD_GRENADE;
    else if (ent->spawnflags & 1)
        mod = MOD_HG_SPLASH;
    else
        mod = MOD_G_SPLASH;
    T_RadiusDamage(ent, ent->owner, ent->dmg, ent->enemy, ent->dmg_radius, mod);

    VectorMA(ent->s.origin, -0.02f, ent->velocity, origin);
    gi.WriteByte(svc_temp_entity);
    if (ent->waterlevel) {
        if (ent->groundentity)
            gi.WriteByte(TE_GRENADE_EXPLOSION_WATER);
        else
            gi.WriteByte(TE_ROCKET_EXPLOSION_WATER);
    } else {
        if (ent->groundentity)
            gi.WriteByte(TE_GRENADE_EXPLOSION);
        else
            gi.WriteByte(TE_ROCKET_EXPLOSION);
    }
    gi.WritePosition(origin);
    gi.multicast(ent->s.origin, MULTICAST_PHS);

    G_FreeEdict(ent);
}

void Grenade_Touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (other == ent->owner)
        return;

    if (!other->solid)
        return;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(ent);
        return;
    }

    if (!other->takedamage) {
        if (ent->spawnflags & 1) {
            if (random() > 0.5f)
                gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/hgrenb1a.wav"), 1, ATTN_NORM, 0);
            else
                gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/hgrenb2a.wav"), 1, ATTN_NORM, 0);
        } else {
            gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/grenlb1b.wav"), 1, ATTN_NORM, 0);
        }
        return;
    }

    ent->enemy = other;
    Grenade_Explode(ent);
}

void fire_grenade(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, float timer, float damage_radius)
{
    if (G_IsOspRuleset())
        OSP_accShot(self, MOD_GRENADE, 1);

    edict_t *grenade;
    vec3_t  dir;
    vec3_t  forward, right, up;
    float   scale;

    vectoangles(aimdir, dir);
    AngleVectors(dir, forward, right, up);

    grenade = G_Spawn();
    VectorCopy(start, grenade->s.origin);
    VectorScale(aimdir, speed, grenade->velocity);
    scale = 200 + crandom() * 10.0f;
    VectorMA(grenade->velocity, scale, up, grenade->velocity);
    scale = crandom() * 10.0f;
    VectorMA(grenade->velocity, scale, right, grenade->velocity);
    VectorSet(grenade->avelocity, 300, 300, 300);
    grenade->movetype = MOVETYPE_BOUNCE;
    grenade->clipmask = MASK_SHOT;
    grenade->solid = SOLID_BBOX;
    grenade->s.effects |= EF_GRENADE;
    VectorClear(grenade->mins);
    VectorClear(grenade->maxs);
    grenade->s.modelindex = gi.modelindex("models/objects/grenade/tris.md2");
    grenade->owner = self;
    grenade->touch = Grenade_Touch;
    grenade->nextthink = level.framenum + timer * BASE_FRAMERATE;
    grenade->think = Grenade_Explode;
    grenade->dmg = damage;
    grenade->dmg_radius = damage_radius;
    grenade->classname = "grenade";

    gi.linkentity(grenade);
}

void fire_grenade2(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, float timer, float damage_radius, bool held)
{
    if (G_IsOspRuleset())
        OSP_accShot(self, MOD_HANDGRENADE, 1);

    edict_t *grenade;
    vec3_t  dir;
    vec3_t  forward, right, up;
    float   scale;

    vectoangles(aimdir, dir);
    AngleVectors(dir, forward, right, up);

    grenade = G_Spawn();
    VectorCopy(start, grenade->s.origin);
    VectorScale(aimdir, speed, grenade->velocity);
    scale = 200 + crandom() * 10.0f;
    VectorMA(grenade->velocity, scale, up, grenade->velocity);
    scale = crandom() * 10.0f;
    VectorMA(grenade->velocity, scale, right, grenade->velocity);
    VectorSet(grenade->avelocity, 300, 300, 300);
    grenade->movetype = MOVETYPE_BOUNCE;
    grenade->clipmask = MASK_SHOT;
    grenade->solid = SOLID_BBOX;
    grenade->s.effects |= EF_GRENADE;
    VectorClear(grenade->mins);
    VectorClear(grenade->maxs);
    grenade->s.modelindex = gi.modelindex("models/objects/grenade2/tris.md2");
    grenade->owner = self;
    grenade->touch = Grenade_Touch;
    grenade->nextthink = level.framenum + timer * BASE_FRAMERATE;
    grenade->think = Grenade_Explode;
    grenade->dmg = damage;
    grenade->dmg_radius = damage_radius;
    grenade->classname = "hgrenade";
    if (held)
        grenade->spawnflags = 3;
    else
        grenade->spawnflags = 1;
    grenade->s.sound = gi.soundindex("weapons/hgrenc1b.wav");

    if (timer <= 0.0f)
        Grenade_Explode(grenade);
    else {
        gi.sound(self, CHAN_WEAPON, gi.soundindex("weapons/hgrent1a.wav"), 1, ATTN_NORM, 0);
        gi.linkentity(grenade);
    }
}

/*
=================
fire_rocket
=================
*/
void rocket_touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    vec3_t      origin;
    int         n;

    if (other == ent->owner)
        return;

    if (!other->solid)
        return;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(ent);
        return;
    }

    if (ent->owner->client)
        PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);

    // calculate position for the explosion entity
    VectorMA(ent->s.origin, -0.02f, ent->velocity, origin);

    if (other->takedamage) {
        T_Damage(other, ent, ent->owner, ent->velocity, ent->s.origin, plane ? plane->normal : NULL, ent->dmg, 0, 0, MOD_ROCKET);
    } else {
        // don't throw any debris in net games
        if (!deathmatch->value && !coop->value) {
            if ((surf) && !(surf->flags & (SURF_WARP | SURF_TRANS33 | SURF_TRANS66 | SURF_FLOWING))) {
                n = Q_rand() % 5;
                while (n--)
                    ThrowDebris(ent, "models/objects/debris2/tris.md2", 2, ent->s.origin);
            }
        }
    }

    T_RadiusDamage(ent, ent->owner, ent->radius_dmg, other, ent->dmg_radius, MOD_R_SPLASH);

    gi.WriteByte(svc_temp_entity);
    if (ent->waterlevel)
        gi.WriteByte(TE_ROCKET_EXPLOSION_WATER);
    else
        gi.WriteByte(TE_ROCKET_EXPLOSION);
    gi.WritePosition(origin);
    gi.multicast(ent->s.origin, MULTICAST_PHS);

    G_FreeEdict(ent);
}

void fire_rocket(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius, int radius_damage)
{
    if (G_IsOspRuleset())
        OSP_accShot(self, MOD_ROCKET, 1);

    edict_t *rocket;

    rocket = G_Spawn();
    VectorCopy(start, rocket->s.origin);
    VectorCopy(dir, rocket->movedir);
    vectoangles(dir, rocket->s.angles);
    VectorScale(dir, speed, rocket->velocity);
    rocket->movetype = MOVETYPE_FLYMISSILE;
    rocket->clipmask = MASK_SHOT;
    rocket->solid = SOLID_BBOX;
    rocket->s.effects |= EF_ROCKET;
    VectorClear(rocket->mins);
    VectorClear(rocket->maxs);
    rocket->s.modelindex = gi.modelindex("models/objects/rocket/tris.md2");
    rocket->owner = self;
    rocket->touch = rocket_touch;
    rocket->nextthink = level.framenum + BASE_FRAMERATE * 8000 / speed;
    rocket->think = G_FreeEdict;
    rocket->dmg = damage;
    rocket->radius_dmg = radius_damage;
    rocket->dmg_radius = damage_radius;
    rocket->s.sound = gi.soundindex("weapons/rockfly.wav");
    rocket->classname = "rocket";

    if (self->client)
        check_dodge(self, rocket->s.origin, dir, speed);

    gi.linkentity(rocket);
}

/*
=================
fire_rail
=================
*/
void fire_rail(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick)
{
    if (G_IsOspRuleset())
        OSP_accShot(self, MOD_RAILGUN, 1);

    vec3_t      from;
    vec3_t      end;
    trace_t     tr;
    edict_t     *ignore;
    int         mask;
    bool        water;
    float       lastfrac;

    VectorMA(start, 8192, aimdir, end);
    VectorCopy(start, from);
    ignore = self;
    water = false;
    mask = MASK_SHOT | CONTENTS_SLIME | CONTENTS_LAVA;
    lastfrac = 1;
    while (ignore) {
        tr = gi.trace(from, NULL, NULL, end, ignore, mask);

        // A liquid is passed by taking its bits out of the mask and tracing
        // on.  A brush that is a liquid AND solid to the shot -- a SOLID|LAVA
        // floor, which custom maps carry and no retail map does -- stops that
        // retrace all the same, at no distance and reporting the same liquid,
        // so id's test took this branch for it forever and the frame never
        // ended.  To the shot such a brush is a wall.
        if ((tr.contents & (CONTENTS_SLIME | CONTENTS_LAVA)) && !(tr.contents & MASK_SHOT)) {
            mask &= ~(CONTENTS_SLIME | CONTENTS_LAVA);
            water = true;
        } else {
            //ZOID--added so rail goes through SOLID_BBOX entities (gibs, etc)
            if (((tr.ent->svflags & SVF_MONSTER) || (tr.ent->client) ||
                 (tr.ent->svflags & SVF_DAMAGEABLE) ||
                 (tr.ent->solid == SOLID_BBOX)) && (lastfrac + tr.fraction > 0))
                ignore = tr.ent;
            else
                ignore = NULL;

            if ((tr.ent != self) && (tr.ent->takedamage))
                T_Damage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal, damage, kick, 0, MOD_RAILGUN);
        }

        VectorCopy(tr.endpos, from);
        lastfrac = tr.fraction;
    }

    // send gun puff / flash
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_RAILTRAIL);
    gi.WritePosition(start);
    gi.WritePosition(tr.endpos);
    gi.multicast(self->s.origin, MULTICAST_PHS);
//  gi.multicast (start, MULTICAST_PHS);
    if (water) {
        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_RAILTRAIL);
        gi.WritePosition(start);
        gi.WritePosition(tr.endpos);
        gi.multicast(tr.endpos, MULTICAST_PHS);
    }

    if (self->client)
        PlayerNoise(self, tr.endpos, PNOISE_IMPACT);
}

/*
=================
fire_bfg
=================
*/
void bfg_explode(edict_t *self)
{
    edict_t *ent;
    float   points;
    vec3_t  v;
    float   dist;

    if (self->s.frame == 0) {
        // the BFG effect
        ent = NULL;
        while ((ent = findradius(ent, self->s.origin, self->dmg_radius)) != NULL) {
            if (!ent->takedamage)
                continue;
            if (ent == self->owner)
                continue;
            if (!CanDamage(ent, self))
                continue;
            if (!CanDamage(ent, self->owner))
                continue;

            VectorAvg(ent->mins, ent->maxs, v);
            VectorAdd(ent->s.origin, v, v);
            VectorSubtract(self->s.origin, v, v);
            dist = VectorLength(v);
            points = self->radius_dmg * (1.0f - sqrtf(dist / self->dmg_radius));
// PMM - happened to notice this copy/paste bug
//          if (ent == self->owner)
//              points = points * 0.5;

            gi.WriteByte(svc_temp_entity);
            gi.WriteByte(TE_BFG_EXPLOSION);
            gi.WritePosition(ent->s.origin);
            gi.multicast(ent->s.origin, MULTICAST_PHS);
            T_Damage(ent, self, self->owner, self->velocity, ent->s.origin, vec3_origin, (int)points, 0, DAMAGE_ENERGY, MOD_BFG_EFFECT);
        }
    }

    self->nextthink = level.framenum + 1;
    self->s.frame++;
    if (self->s.frame == 5)
        self->think = G_FreeEdict;
}

void bfg_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (other == self->owner)
        return;

    if (!other->solid)
        return;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(self);
        return;
    }

    if (self->owner->client)
        PlayerNoise(self->owner, self->s.origin, PNOISE_IMPACT);

    // core explosion - prevents firing it into the wall/floor
    if (other->takedamage)
        T_Damage(other, self, self->owner, self->velocity, self->s.origin, plane ? plane->normal : NULL, 200, 0, 0, MOD_BFG_BLAST);
    T_RadiusDamage(self, self->owner, 200, other, 100, MOD_BFG_BLAST);

    gi.sound(self, CHAN_VOICE, gi.soundindex("weapons/bfg__x1b.wav"), 1, ATTN_NORM, 0);
    self->solid = SOLID_NOT;
    self->touch = NULL;
    VectorMA(self->s.origin, -1 * FRAMETIME, self->velocity, self->s.origin);
    VectorClear(self->velocity);
    self->s.modelindex = gi.modelindex("sprites/s_bfg3.sp2");
    self->s.frame = 0;
    self->s.sound = 0;
    self->s.effects &= ~EF_ANIM_ALLFAST;
    self->think = bfg_explode;
    self->nextthink = level.framenum + 1;
    self->enemy = other;

    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_BFG_BIGEXPLOSION);
    gi.WritePosition(self->s.origin);
    gi.multicast(self->s.origin, MULTICAST_PVS);
}

void bfg_think(edict_t *self)
{
    edict_t *ent;
    edict_t *ignore;
    vec3_t  point;
    vec3_t  dir;
    vec3_t  start;
    vec3_t  end;
    int     dmg;
    trace_t tr;

    if (deathmatch->value)
        dmg = 5;
    else
        dmg = 10;

    ent = NULL;
    while ((ent = findradius(ent, self->s.origin, 256)) != NULL) {
        if (ent == self)
            continue;

        if (!ent->solid)
            continue;

        if (ent == self->owner)
            continue;

        if (!ent->takedamage)
            continue;

        //ROGUE - make tesla hurt by bfg
        if (!(ent->svflags & SVF_MONSTER) && !(ent->svflags & SVF_DAMAGEABLE) && (!ent->client) && (strcmp(ent->classname, "misc_explobox") != 0))
//      if (!(ent->svflags & SVF_MONSTER) && (!ent->client) && (strcmp(ent->classname, "misc_explobox") != 0)
//          && (strcmp(ent->classname, "tesla") != 0))
            continue;

        // CTF: the BFG does not chase your own team.  Reachable only under ctf
        // -- resp.ctf_team is CTF_NOTEAM for everyone otherwise -- but the
        // ruleset test states the intent rather than relying on that.
        if (G_Ruleset() == RULESET_CTF && ent->client &&
            self->owner->client &&
            ent->client->resp.ctf_team == self->owner->client->resp.ctf_team)
            continue;

        VectorMA(ent->absmin, 0.5f, ent->size, point);

        VectorSubtract(point, self->s.origin, dir);
        VectorNormalize(dir);

        ignore = self;
        VectorCopy(self->s.origin, start);
        VectorMA(start, 2048, dir, end);
        while (1) {
            tr = gi.trace(start, NULL, NULL, end, ignore, CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_DEADMONSTER);

            if (!tr.ent)
                break;

            // hurt it if we can
            if ((tr.ent->takedamage) && !(tr.ent->flags & FL_IMMUNE_LASER) && (tr.ent != self->owner))
                T_Damage(tr.ent, self, self->owner, dir, tr.endpos, vec3_origin, dmg, 1, DAMAGE_ENERGY, MOD_BFG_LASER);

            // if we hit something that's not a monster or player we're done
//          if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client))
            if (!(tr.ent->svflags & SVF_MONSTER) && !(tr.ent->svflags & SVF_DAMAGEABLE) && (!tr.ent->client)) {
                gi.WriteByte(svc_temp_entity);
                gi.WriteByte(TE_LASER_SPARKS);
                gi.WriteByte(4);
                gi.WritePosition(tr.endpos);
                gi.WriteDir(tr.plane.normal);
                gi.WriteByte(0xd0);
                gi.multicast(tr.endpos, MULTICAST_PVS);
                break;
            }

            ignore = tr.ent;
            VectorCopy(tr.endpos, start);
        }

        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_BFG_LASER);
        gi.WritePosition(self->s.origin);
        gi.WritePosition(tr.endpos);
        gi.multicast(self->s.origin, MULTICAST_PHS);
    }

    self->nextthink = level.framenum + 1;
}

void fire_bfg(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius)
{
    if (G_IsOspRuleset())
        OSP_accShot(self, MOD_BFG_BLAST, 1);

    edict_t *bfg;

    bfg = G_Spawn();
    VectorCopy(start, bfg->s.origin);
    VectorCopy(dir, bfg->movedir);
    vectoangles(dir, bfg->s.angles);
    VectorScale(dir, speed, bfg->velocity);
    bfg->movetype = MOVETYPE_FLYMISSILE;
    bfg->clipmask = MASK_SHOT;
    bfg->solid = SOLID_BBOX;
    bfg->s.effects |= EF_BFG | EF_ANIM_ALLFAST;
    VectorClear(bfg->mins);
    VectorClear(bfg->maxs);
    bfg->s.modelindex = gi.modelindex("sprites/s_bfg1.sp2");
    bfg->owner = self;
    bfg->touch = bfg_touch;
    bfg->nextthink = level.framenum + BASE_FRAMERATE * 8000 / speed;
    bfg->think = G_FreeEdict;
    bfg->radius_dmg = damage;
    bfg->dmg_radius = damage_radius;
    bfg->classname = "bfg blast";
    bfg->s.sound = gi.soundindex("weapons/bfg__l1a.wav");

    bfg->think = bfg_think;
    bfg->nextthink = level.framenum + 1;
    bfg->teammaster = bfg;
    bfg->teamchain = NULL;

    if (self->client)
        check_dodge(self, bfg->s.origin, dir, speed);

    gi.linkentity(bfg);
}

// RAFAEL

/*
    fire_ionripper
*/

void ionripper_sparks(edict_t *self)
{
    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_WELDING_SPARKS);
    gi.WriteByte(0);
    gi.WritePosition(self->s.origin);
    gi.WriteDir(vec3_origin);
    gi.WriteByte(0xe4 + (Q_rand() & 3));
    gi.multicast(self->s.origin, MULTICAST_PVS);

    G_FreeEdict(self);
}

// RAFAEL
void ionripper_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    edict_t *shooter = self->obitowner;
    int     damage = self->dmg;

    if (other == self->owner)
        return;

    // R-MP-28: with `ripper_self` above 0, a player's ion stops naming its
    // shooter as `owner` at its first touch, so that it can come back.  That
    // touch is a wall it bounces off, or the point-blank call in
    // fire_ionripper.  The engine never clips a move against the mover's own
    // `owner`, and that was the only thing keeping an ion off its shooter.
    // `obitowner` still names the shooter for every credit.  A monster's ion
    // keeps its owner: the setting is about players, and openffa-xatrix, where
    // it comes from, had no monsters.
    if (ripper_self->value > 0 && shooter->client)
        self->owner = self;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(self);
        return;
    }

    if (shooter->client)
        PlayerNoise(shooter, self->s.origin, PNOISE_IMPACT);

    // R-MP-28: an ion that comes back deals its shooter `ripper_self`.  If
    // Quad or Double Damage multiplied the shot, it deals the shot's full
    // damage instead, which is openffa-xatrix's rule ("the full damage will
    // occur no matter who you hit").  The damage is credited to the shooter,
    // so the obituary reads a suicide, and the accuracy report does not count
    // a hit, because it ignores damage a player does to themselves.
    if (other == shooter && damage <= G_IonRipperDamage())
        damage = ripper_self->value;

    // `plane` is NULL on fire_ionripper's point-blank path, which calls this
    // directly, and on any touch that comes from the other entity's move;
    // guarded the way rocket_touch and bfg_touch are.
    if (other->takedamage) {
        T_Damage(other, self, shooter, self->velocity, self->s.origin, plane ? plane->normal : NULL, damage, 1, DAMAGE_ENERGY, MOD_RIPPER);

    } else {
        return;
    }

    G_FreeEdict(self);
}

// RAFAEL
void fire_ionripper(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int effect)
{
    // The content layers' weapons reach the accuracy report too --
    // both are valid with every ruleset, so a match fought with
    // them is a match the report has to be able to describe.
    if (G_IsOspRuleset())
        OSP_accShot(self, MOD_RIPPER, 1);

    edict_t *ion;
    trace_t tr;

    VectorNormalize(dir);

    ion = G_Spawn();
    VectorCopy(start, ion->s.origin);
    VectorCopy(start, ion->s.old_origin);
    vectoangles(dir, ion->s.angles);
    VectorScale(dir, speed, ion->velocity);

    ion->movetype = MOVETYPE_WALLBOUNCE;
    ion->clipmask = MASK_SHOT;
    ion->solid = SOLID_BBOX;
    ion->s.effects |= effect;

    ion->s.renderfx |= RF_FULLBRIGHT;

    VectorClear(ion->mins);
    VectorClear(ion->maxs);
    ion->s.modelindex = gi.modelindex("models/objects/boomrang/tris.md2");
    ion->s.sound = gi.soundindex("misc/lasfly.wav");
    ion->owner = self;
    // The shooter, for every credit, even after ripper_self makes the ion
    // stop naming them as `owner` (R-MP-28).
    ion->obitowner = self;
    ion->touch = ionripper_touch;
    ion->nextthink = level.framenum + 3 * BASE_FRAMERATE;
    ion->think = ionripper_sparks;
    ion->dmg = damage;
    ion->dmg_radius = 100;
    gi.linkentity(ion);

    if (self->client)
        check_dodge(self, ion->s.origin, dir, speed);

    tr = gi.trace(self->s.origin, NULL, NULL, ion->s.origin, ion, MASK_SHOT);
    if (tr.fraction < 1.0) {
        VectorMA(ion->s.origin, -10, dir, ion->s.origin);
        ion->touch(ion, tr.ent, NULL, NULL);
    }

}

// RAFAEL
/*
fire_heat
*/

void heat_think(edict_t *self)
{
    edict_t     *target = NULL;
    edict_t     *aquire = NULL;
    vec3_t      vec;
    int         len;
    int         oldlen = 0;

    VectorClear(vec);

    // aquire new target
    while ((target = findradius(target, self->s.origin, 1024)) != NULL) {

        if (self->owner == target)
            continue;
        // A player, which is what the shipped missile tracked.  The Reckoning
        // wrote `if (!target->svflags & SVF_MONSTER)` here, which precedence
        // makes a test that never passes, so only the `client` line below
        // decided (gladq2_src has it the same way).  q2pro's port parenthesised
        // it into `!(svflags & SVF_MONSTER)`, and next to `!client` that asks
        // for an entity that is both a monster and a client: nothing, so the
        // heat chick's rockets flew straight.  The line is gone rather than
        // "fixed", because the behaviour that shipped is the one kept.
        if (!target->client)
            continue;
        if (target->health <= 0)
            continue;
        if (!visible(self, target))
            continue;

        // if we need to reduce the tracking cone
        /*
        {
            vec3_t  vec;
            float   dot;
            vec3_t  forward;

            AngleVectors (self->s.angles, forward, NULL, NULL);
            VectorSubtract (target->s.origin, self->s.origin, vec);
            VectorNormalize (vec);
            dot = DotProduct (vec, forward);

            if (dot > 0.6)
                continue;
        }
        */

        if (!infront(self, target))
            continue;

        VectorSubtract(self->s.origin, target->s.origin, vec);
        len = VectorLength(vec);

        if (aquire == NULL || len < oldlen) {
            aquire = target;
            self->target_ent = aquire;
            oldlen = len;
        }
    }

    if (aquire != NULL) {
        VectorSubtract(aquire->s.origin, self->s.origin, vec);

        vectoangles(vec, self->s.angles);

        VectorNormalize(vec);
        VectorCopy(vec, self->movedir);
        VectorScale(vec, 500, self->velocity);
    }

    self->nextthink = level.framenum + 0.1 * BASE_FRAMERATE;
}

// RAFAEL
void xatrix_fire_heat(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius, int radius_damage)
{
    edict_t *heat;

    heat = G_Spawn();
    VectorCopy(start, heat->s.origin);
    VectorCopy(dir, heat->movedir);
    vectoangles(dir, heat->s.angles);
    VectorScale(dir, speed, heat->velocity);
    heat->movetype = MOVETYPE_FLYMISSILE;
    heat->clipmask = MASK_SHOT;
    heat->solid = SOLID_BBOX;
    heat->s.effects |= EF_ROCKET;
    VectorClear(heat->mins);
    VectorClear(heat->maxs);
    heat->s.modelindex = gi.modelindex("models/objects/rocket/tris.md2");
    heat->owner = self;
    heat->touch = rocket_touch;

    heat->nextthink = level.framenum + 0.1 * BASE_FRAMERATE;
    heat->think = heat_think;

    heat->dmg = damage;
    heat->radius_dmg = radius_damage;
    heat->dmg_radius = damage_radius;
    heat->s.sound = gi.soundindex("weapons/rockfly.wav");

    if (self->client)
        check_dodge(self, heat->s.origin, dir, speed);

    gi.linkentity(heat);
}

// RAFAEL

/*
    fire_plasma
*/

void plasma_touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    vec3_t      origin;

    if (other == ent->owner)
        return;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(ent);
        return;
    }

    if (ent->owner->client)
        PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);

    // calculate position for the explosion entity
    VectorMA(ent->s.origin, -0.02, ent->velocity, origin);

    // NULL-safe like ionripper_touch: a touch that comes from the OTHER
    // entity's move -- SV_Impact's second call, a player's pmove running into
    // the ball -- has no plane.
    if (other->takedamage) {
        T_Damage(other, ent, ent->owner, ent->velocity, ent->s.origin, plane ? plane->normal : NULL, ent->dmg, 0, 0, MOD_PHALANX);
    }

    T_RadiusDamage(ent, ent->owner, ent->radius_dmg, other, ent->dmg_radius, MOD_PHALANX);

    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_PLASMA_EXPLOSION);
    gi.WritePosition(origin);
    gi.multicast(ent->s.origin, MULTICAST_PVS);

    G_FreeEdict(ent);
}

// RAFAEL
void fire_plasma(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius, int radius_damage)
{
    // The Phalanx.  `fire_plasma` is Xatrix's name for it and MOD_PHALANX is
    // what it damages with.
    if (G_IsOspRuleset())
        OSP_accShot(self, MOD_PHALANX, 1);

    edict_t *plasma;

    plasma = G_Spawn();
    VectorCopy(start, plasma->s.origin);
    VectorCopy(dir, plasma->movedir);
    vectoangles(dir, plasma->s.angles);
    VectorScale(dir, speed, plasma->velocity);
    plasma->movetype = MOVETYPE_FLYMISSILE;
    plasma->clipmask = MASK_SHOT;
    plasma->solid = SOLID_BBOX;

    VectorClear(plasma->mins);
    VectorClear(plasma->maxs);

    plasma->owner = self;
    plasma->touch = plasma_touch;
    plasma->nextthink = level.framenum + BASE_FRAMERATE * 8000 / speed;
    plasma->think = G_FreeEdict;
    plasma->dmg = damage;
    plasma->radius_dmg = radius_damage;
    plasma->dmg_radius = damage_radius;
    plasma->s.sound = gi.soundindex("weapons/rockfly.wav");

    plasma->s.modelindex = gi.modelindex("sprites/s_photon.sp2");
    plasma->s.effects |= EF_PLASMA | EF_ANIM_ALLFAST;

    if (self->client)
        check_dodge(self, plasma->s.origin, dir, speed);

    gi.linkentity(plasma);

}

// R-MP-25: a trap's life in frames, from `sv_trap_duration` in seconds.  The
// Reckoning wrote 30 at both places that read this.
static int trap_lifetime(void)
{
    return sv_trap_duration->value * BASE_FRAMERATE;
}

// R-MP-27: the shell a shootable trap shows when it is hit.  These are all the
// bits trap_pain sets, so a new hit clears the previous hit's colour.
#define TRAP_SHELL_BITS (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | \
                         RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM)

/*
=================
Trap_Destroyed

R-MP-27: a shootable trap that was shot to pieces explodes here, one frame
after trap_die.  The delay is the one barrel_delay uses for a barrel, so that
the blast is not set off from inside the T_Damage call that killed the trap.

The blast is credited to the thrower, `obitowner`, and not to whoever shot the
trap; that is openffa-xatrix's choice.  `dmg` and `dmg_radius` are the
sv_trap_expl_* values from the moment the trap was thrown, with the thrower's
Quad or Double Damage already applied (weapon_trap_fire).  trap_die cleared
`takedamage`, so the blast does not hit the trap itself.  The two sounds are
openffa-xatrix's and come from baseq2's pak0.
=================
*/
void Trap_Destroyed(edict_t *ent)
{
    T_RadiusDamage(ent, ent->obitowner, ent->dmg, NULL, ent->dmg_radius, MOD_TRAP_SPLASH);
    gi.sound(ent, CHAN_BODY, gi.soundindex("flyer/flydeth1.wav"), 1, ATTN_NORM, 0);
    gi.sound(ent, CHAN_VOICE, gi.soundindex("world/fuseout.wav"), 1, ATTN_NORM, 0);
    BecomeExplosion1(ent);
}

// R-MP-27: openffa-xatrix's damage shell, which tells everyone how close the
// trap is to breaking.  It is white above two thirds of the trap's health,
// double and red above a third, half-damage and red above a tenth, and red
// below that.  It lasts half a second, and Trap_Think clears it.
// `pain_debounce_framenum`, which a trap does not otherwise use, records when
// the shell ends.
void trap_pain(edict_t *self, edict_t *other, float kick, int damage)
{
    int full = self->max_health;

    self->s.effects |= EF_COLOR_SHELL;
    self->s.renderfx &= ~TRAP_SHELL_BITS;
    if (self->health >= full * 2 / 3)
        self->s.renderfx |= RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE;
    else if (self->health >= full / 3)
        self->s.renderfx |= RF_SHELL_DOUBLE | RF_SHELL_RED;
    else if (self->health >= full / 10)
        self->s.renderfx |= RF_SHELL_HALF_DAM | RF_SHELL_RED;
    else
        self->s.renderfx |= RF_SHELL_RED;
    self->pain_debounce_framenum = level.framenum + 0.5f * BASE_FRAMERATE;

    if (self->health >= full / 10)
        gi.sound(self, CHAN_BODY, gi.soundindex((Q_rand() & 1) ? "world/airhiss2.wav" : "weapons/railgr1a.wav"), 1, ATTN_NORM, 0);
    else
        gi.sound(self, CHAN_BODY, gi.soundindex("tank/pain.wav"), 1, ATTN_NORM, 0);
}

// R-MP-27: Killed() calls this.  The trap stops taking damage, so the blast
// cannot hit it and a second hit cannot kill it again, and it hands its next
// think to Trap_Destroyed.
void trap_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    self->takedamage = DAMAGE_NO;
    self->nextthink = level.framenum + 1;
    self->think = Trap_Destroyed;
}

// R-MP-27: Trap_Think calls this on the think on which a trap arms, when
// `sv_trap_health` is above 0.  The trap takes damage, with the setting as its
// health; `max_health` keeps that number for trap_pain's shell thresholds even
// if the setting changes later.  The trap also stops naming its thrower as
// `owner`.  The engine never clips a move against the mover's own `owner`, so
// while `owner` names the thrower, the thrower's own shots pass straight
// through.  `owner` now names the trap itself, as in openffa-xatrix, and
// `obitowner` still names the thrower for every credit.
static void trap_make_shootable(edict_t *ent)
{
    ent->takedamage = DAMAGE_YES;
    ent->health = ent->max_health = sv_trap_health->value;
    ent->pain = trap_pain;
    ent->die = trap_die;
    ent->owner = ent;
}

// RAFAEL
// RAFAEL
void Trap_Think(edict_t *ent)
{
    edict_t *target = NULL;
    edict_t *best = NULL;
    vec3_t  vec;
    int     len, i;
    int     oldlen = 8000;
    vec3_t  forward, right, up;
    vec3_t  trap_origin;

    // R-MP-27: a shootable trap's damage shell goes off half a second after
    // the hit that lit it (trap_pain).
    if (ent->pain_debounce_framenum && level.framenum >= ent->pain_debounce_framenum) {
        ent->s.effects &= ~EF_COLOR_SHELL;
        ent->s.renderfx &= ~TRAP_SHELL_BITS;
        ent->pain_debounce_framenum = 0;
    }

    if (ent->timestamp < level.framenum) {
        BecomeExplosion1(ent);
        // note to self
        // cause explosion damage???
        return;
    }

    ent->nextthink = level.framenum + 0.1 * BASE_FRAMERATE;

    if (!ent->groundentity)
        return;

    // ok lets do the blood effect
    if (ent->s.frame > 4) {
        if (ent->s.frame == 5) {
            if (ent->wait == 64)
                gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/trapdown.wav"), 1, ATTN_IDLE, 0);

            ent->wait -= 2;
            ent->delay += level.time;

            for (i = 0; i < 3; i++) {

                best = G_Spawn();

                if (strcmp(ent->enemy->classname, "monster_gekk") == 0) {
                    best->s.modelindex = gi.modelindex("models/objects/gekkgib/torso/tris.md2");
                    best->s.effects |= TE_GREENBLOOD;
                } else if (ent->mass > 200) {
                    best->s.modelindex = gi.modelindex("models/objects/gibs/chest/tris.md2");
                    best->s.effects |= TE_BLOOD;
                } else {
                    best->s.modelindex = gi.modelindex("models/objects/gibs/sm_meat/tris.md2");
                    best->s.effects |= TE_BLOOD;
                }

                AngleVectors(ent->s.angles, forward, right, up);

                RotatePointAroundVector(vec, up, right, ((360.0 / 3)* i) + ent->delay);
                VectorMA(vec, ent->wait / 2, vec, vec);
                VectorAdd(vec, ent->s.origin, vec);
                VectorAdd(vec, forward, best->s.origin);

                best->s.origin[2] = ent->s.origin[2] + ent->wait;

                VectorCopy(ent->s.angles, best->s.angles);

                best->solid = SOLID_NOT;
                best->s.effects |= EF_GIB;
                best->takedamage = DAMAGE_YES;

                best->movetype = MOVETYPE_TOSS;
                best->svflags |= SVF_MONSTER;
                best->deadflag = DEAD_DEAD;

                VectorClear(best->mins);
                VectorClear(best->maxs);

                best->watertype = gi.pointcontents(best->s.origin);
                if (best->watertype & MASK_WATER)
                    best->waterlevel = 1;

                best->nextthink = level.framenum + 0.1 * BASE_FRAMERATE;
                best->think = G_FreeEdict;
                gi.linkentity(best);
            }

            if (ent->wait < 19)
                ent->s.frame ++;

            return;
        }
        ent->s.frame ++;
        if (ent->s.frame == 8) {
            ent->nextthink = level.framenum + 1.0 * BASE_FRAMERATE;
            ent->think = G_FreeEdict;

            // No cube in an arena, which has no items: SpawnItem would only
            // precache it -- mid-round -- and free it.
            if (G_Ruleset() == RULESET_ARENA)
                return;

            best = G_Spawn();
            SP_item_foodcube(best);
            // And the cube need not survive its own spawn elsewhere either:
            // SP_item_foodcube frees it under DF_NO_HEALTH.  G_FreeEdict
            // zeroes the slot and G_InitEdict relies on that, so the writes
            // below would have been inherited by the next entity spawned
            // there -- and the link put a freed edict back in the world.
            if (!best->inuse)
                return;
            VectorCopy(ent->s.origin, best->s.origin);
            best->s.origin[2] += 16;
            best->velocity[2] = 400;
            best->count = ent->mass;
            gi.linkentity(best);
            return;
        }
        return;
    }

    ent->s.effects &= ~EF_TRAP;
    if (ent->s.frame >= 4) {
        ent->s.effects |= EF_TRAP;
        // R-MP-27: `sv_trap_health` is read once per trap, on the think on
        // which it arms.  That is the first think with frame 4, and the only
        // one on which its box still has a height, because the Reckoning
        // makes the trap a point here.  A shootable trap keeps its box,
        // because a point cannot be hit.  A trap that armed as a point stays
        // one, even if the setting is raised later.
        if (!ent->takedamage && ent->maxs[2] && sv_trap_health->value > 0)
            trap_make_shootable(ent);
        if (!ent->takedamage) {
            VectorClear(ent->mins);
            VectorClear(ent->maxs);
        }
    }

    if (ent->s.frame < 4)
        ent->s.frame++;

    while ((target = findradius(target, ent->s.origin, 256)) != NULL) {
        if (target == ent)
            continue;
        if (!(target->svflags & SVF_MONSTER) && !target->client)
            continue;
        // if (target == ent->owner)
        //  continue;
        if (target->health <= 0)
            continue;
        if (!visible(ent, target))
            continue;
        if (!best) {
            best = target;
            continue;
        }
        VectorSubtract(ent->s.origin, target->s.origin, vec);
        len = VectorLength(vec);
        if (len < oldlen) {
            oldlen = len;
            best = target;
        }
    }

    // pull the enemy in
    if (best) {
        vec3_t  forward;

        if (best->groundentity) {
            best->s.origin[2] += 1;
            best->groundentity = NULL;
        }
        // R-MP-27: a shootable trap kept its box, so a player pulled onto it
        // stands 8 units higher than on a point trap, and a 24-unit standing
        // height plus those 8 is never under 32.  For a target above the
        // trap, the distance is therefore measured from the top of the box,
        // so a shootable trap catches at the same distance a point trap does.
        // This is openffa-xatrix's correction.  A point trap has no box, and
        // its distance is the Reckoning's.
        VectorCopy(ent->s.origin, trap_origin);
        if (ent->takedamage && best->s.origin[2] > trap_origin[2])
            trap_origin[2] += ent->maxs[2];
        VectorSubtract(trap_origin, best->s.origin, vec);
        len = VectorLength(vec);
        if (best->client) {
            VectorNormalize(vec);
            // R-MP-29: 250, or more for a trap thrown with Quad (fire_trap).
            // A monster below is steered rather than pulled, at its own 256,
            // and openffa-xatrix leaves that alone too.
            VectorMA(best->velocity, ent->speed, vec, best->velocity);
        } else {
            best->ideal_yaw = vectoyaw(vec);
            M_ChangeYaw(best);
            AngleVectors(best->s.angles, forward, NULL, NULL);
            VectorScale(forward, 256, best->velocity);
        }

        gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/trapsuck.wav"), 1, ATTN_IDLE, 0);

        if (len < 32) {
            if (best->mass < 400) {
                // The thrower, from `obitowner`, because a shootable trap no
                // longer names them as `owner` (R-MP-27).
                T_Damage(best, ent, ent->obitowner, vec3_origin, best->s.origin, vec3_origin, 100000, 1, 0, MOD_TRAP);
                ent->enemy = best;
                ent->wait = 64;
                VectorCopy(ent->s.origin, ent->s.old_origin);
                ent->timestamp = level.framenum + trap_lifetime();   // R-MP-25
                if (deathmatch->value)
                    ent->mass = best->mass / 4;
                else
                    ent->mass = best->mass / 10;
                // ok spawn the food cube
                ent->s.frame = 5;
            } else {
                BecomeExplosion1(ent);
                // note to self
                // cause explosion damage???
                return;
            }

        }
    }

}

// RAFAEL
void fire_trap(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, float timer, float damage_radius, bool held)
{
    // A thrown device that damages, counted like the hand grenade the donor
    // already counts.
    if (G_IsOspRuleset())
        OSP_accShot(self, MOD_TRAP, 1);

    edict_t *trap;
    vec3_t  dir;
    vec3_t  forward, right, up;

    vectoangles(aimdir, dir);
    AngleVectors(dir, forward, right, up);

    trap = G_Spawn();
    VectorCopy(start, trap->s.origin);
    VectorScale(aimdir, speed, trap->velocity);
    VectorMA(trap->velocity, 200 + crandom() * 10.0, up, trap->velocity);
    VectorMA(trap->velocity, crandom() * 10.0, right, trap->velocity);
    VectorSet(trap->avelocity, 0, 300, 0);
    trap->movetype = MOVETYPE_BOUNCE;
    trap->clipmask = MASK_SHOT;
    trap->solid = SOLID_BBOX;
//  VectorClear (trap->mins);
//  VectorClear (trap->maxs);
    VectorSet(trap->mins, -4, -4, 0);
    VectorSet(trap->maxs, 4, 4, 8);
    trap->s.modelindex = gi.modelindex("models/weapons/z_trap/tris.md2");
    trap->owner = self;
    // The thrower, who every credit names, even after a shootable trap stops
    // naming them as `owner` (R-MP-27).
    trap->obitowner = self;
    // R-MP-29: how hard this trap pulls a player, in units a second it adds to
    // the player's velocity on every think.  It is fixed now, as the blast is:
    // a trap thrown with Quad keeps its pull after the Quad runs out.  The
    // Reckoning's is 250.  With Quad, and `sv_trap_quad_pull` above 1, it is
    // that many times 250, as openffa-xatrix's is 4 times.  Double Damage does
    // not count: openffa-xatrix has only Quad.  A trap has no other use for
    // `speed`.
    trap->speed = 250;
    if (self->client && self->client->quad_framenum > level.framenum &&
        sv_trap_quad_pull->value > 1)
        trap->speed *= sv_trap_quad_pull->value;
    trap->nextthink = level.framenum + 1.0 * BASE_FRAMERATE;
    trap->think = Trap_Think;
    // The blast if the trap goes off right now, or if it is later shot to
    // pieces: weapon_trap_fire chose which pair (R-MP-26, R-MP-27).
    trap->dmg = damage;
    trap->dmg_radius = damage_radius;
    trap->classname = "htrap";
    // RAFAEL 16-APR-98
    trap->s.sound = gi.soundindex("weapons/traploop.wav");
    // END 16-APR-98
    if (held)
        trap->spawnflags = 3;
    else
        trap->spawnflags = 1;

    if (timer <= 0.0) {
        // Gone: Grenade_Explode frees it, and the timestamp below would be
        // written into a zeroed slot for the next G_Spawn to inherit.
        Grenade_Explode(trap);
        return;
    } else {
        // gi.sound (self, CHAN_WEAPON, gi.soundindex ("weapons/trapdown.wav"), 1, ATTN_NORM, 0);
        gi.linkentity(trap);
    }

    trap->timestamp = level.framenum + trap_lifetime();    // R-MP-25

}
