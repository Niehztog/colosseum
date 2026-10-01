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
// g_utils.c -- misc utility functions for game module

#include "g_local.h"
#include "arena/arena.h"
#include "tourney/osp_hooks.h"

// Send a console command to one client.  Threewave and RA2 each shipped a
// byte-identical copy of this; it is a generic engine helper, so it lives here
// where the other generic helpers are rather than in either donor's file.
void stuffcmd(edict_t *ent, char *s)
{
    gi.WriteByte(svc_stufftext);
    gi.WriteString(s);
    gi.unicast(ent, true);
}

void G_ProjectSource(const vec3_t point, const vec3_t distance, const vec3_t forward, const vec3_t right, vec3_t result)
{
    result[0] = point[0] + forward[0] * distance[0] + right[0] * distance[1];
    result[1] = point[1] + forward[1] * distance[0] + right[1] * distance[1];
    result[2] = point[2] + forward[2] * distance[0] + right[2] * distance[1] + distance[2];
}

void G_ProjectSource2(const vec3_t point, const vec3_t distance, const vec3_t forward, const vec3_t right, const vec3_t up, vec3_t result)
{
    result[0] = point[0] + forward[0] * distance[0] + right[0] * distance[1] + up[0] * distance[2];
    result[1] = point[1] + forward[1] * distance[0] + right[1] * distance[1] + up[1] * distance[2];
    result[2] = point[2] + forward[2] * distance[0] + right[2] * distance[1] + up[2] * distance[2];
}

/*
=============
G_Find

Searches all active entities for the next one that holds
the matching string at fieldofs (use the FOFS() macro) in the structure.

Searches beginning at the edict after from, or the beginning if NULL
NULL will be returned if the end of the list is reached.

A NULL match finds nothing.  In baseq2 every caller passes a key it has
already required, but the merged donors hand it optional ones -- a target
that is legitimately absent under one ruleset and not another -- and
Q_stricmp dereferences both arguments, so "not found" is the only answer
that does not take the server with it.

=============
*/
edict_t *G_Find(edict_t *from, int fieldofs, char *match)
{
    char    *s;

    if (!match)
        return NULL;

    if (!from)
        from = g_edicts;
    else
        from++;

    for (; from < &g_edicts[globals.num_edicts]; from++) {
        if (!from->inuse)
            continue;
        s = *(char **)((byte *)from + fieldofs);
        if (!s)
            continue;
        if (!Q_stricmp(s, match))
            return from;
    }

    return NULL;
}

/*
=================
G_SpawnPointPool

How many spawn points of `classname` a random selection can actually draw from,
which is not the number the map carries.

The dm and ctf bot fills both need this number and neither may guess it.  Both random
selectors in this tree -- SelectRandomDeathmatchSpawnPoint and
SelectCTFSpawnPoint -- find the two spots nearest a player, refuse them, and
choose among `count - 2`; the arm that keeps both is `if (count <= 2)`, for a
map with nothing to spare.  So a map's usable pool is two short of its count and
the two rules that size a bot fill to a map ask for it here rather than each
writing the loop and the subtraction.

Arena's answer is its own -- `ArenaSpawnCount` filters by arena number, and
SelectRandomArenaSpawnPoint refuses nothing: it walks every candidate on the
side's parity and takes the first with 50 units of clearance, so a pickup arena
seats its whole even count.  Two selectors, two pools, one rule.
=================
*/
int G_SpawnPointPool(char *classname)
{
    edict_t *spot = NULL;
    int     count = 0;

    while ((spot = G_Find(spot, FOFS(classname), classname)) != NULL)
        count++;

    if (count > 2)
        count -= 2;

    return count;
}

/*
=================
findradius

Returns entities that have origins within a spherical area

findradius (origin, radius)
=================
*/
edict_t *findradius(edict_t *from, vec3_t org, float rad)
{
    vec3_t  eorg;
    vec3_t  mid;

    if (!from)
        from = g_edicts;
    else
        from++;
    for (; from < &g_edicts[globals.num_edicts]; from++) {
        if (!from->inuse)
            continue;
        if (from->solid == SOLID_NOT)
            continue;
        VectorAvg(from->mins, from->maxs, mid);
        VectorAdd(from->s.origin, mid, eorg);
        if (Distance(eorg, org) > rad)
            continue;
        return from;
    }

    return NULL;
}

/*
=================
findradius2

Returns entities that have origins within a spherical area

ROGUE - tweaks for performance for tesla specific code
only returns entities that can be damaged
only returns entities that are SVF_DAMAGEABLE

findradius2 (origin, radius)
=================
*/
edict_t *findradius2(edict_t *from, vec3_t org, float rad)
{
    // rad must be positive
    vec3_t  eorg;
    int     j;

    if (!from)
        from = g_edicts;
    else
        from++;
    for (; from < &g_edicts[globals.num_edicts]; from++) {
        if (!from->inuse)
            continue;
        if (from->solid == SOLID_NOT)
            continue;
        if (!from->takedamage)
            continue;
        if (!(from->svflags & SVF_DAMAGEABLE))
            continue;
        for (j = 0; j < 3; j++)
            eorg[j] = org[j] - (from->s.origin[j] + (from->mins[j] + from->maxs[j]) * 0.5f);
        if (VectorLength(eorg) > rad)
            continue;
        return from;
    }

    return NULL;
}

/*
=============
G_PickTarget

Searches all active entities for the next one that holds
the matching string at fieldofs (use the FOFS() macro) in the structure.

Searches beginning at the edict after from, or the beginning if NULL
NULL will be returned if the end of the list is reached.

=============
*/
#define MAXCHOICES  8

edict_t *G_PickTarget(char *targetname)
{
    edict_t *ent = NULL;
    int     num_choices = 0;
    edict_t *choice[MAXCHOICES];

    if (!targetname) {
        gi.dprintf("G_PickTarget called with NULL targetname\n");
        return NULL;
    }

    while (1) {
        ent = G_Find(ent, FOFS(targetname), targetname);
        if (!ent)
            break;
        choice[num_choices++] = ent;
        if (num_choices == MAXCHOICES)
            break;
    }

    if (!num_choices) {
        gi.dprintf("G_PickTarget: target %s not found\n", targetname);
        return NULL;
    }

    return choice[Q_rand_uniform(num_choices)];
}

void Think_Delay(edict_t *ent)
{
    G_UseTargets(ent, ent->activator);
    G_FreeEdict(ent);
}

/*
==============================
G_UseTargets

the global "activator" should be set to the entity that initiated the firing.

If self.delay is set, a DelayedUse entity will be created that will actually
do the SUB_UseTargets after that many seconds have passed.

Centerprints any self.message to the activator.

Search for (string)targetname in all entities that
match (string)self.target and call their .use function

==============================
*/
void G_UseTargets(edict_t *ent, edict_t *activator)
{
    edict_t     *t;
    edict_t     *master;

//
// check for a delay
//
    if (ent->delay) {
        // create a temp object to fire at a later time
        t = G_Spawn();
        t->classname = "DelayedUse";
        t->nextthink = level.framenum + ent->delay * BASE_FRAMERATE;
        t->think = Think_Delay;
        t->activator = activator;
        if (!activator)
            gi.dprintf("Think_Delay with no activator\n");
        t->message = ent->message;
        t->target = ent->target;
        t->killtarget = ent->killtarget;
        return;
    }

//
// print the message
//
    // R-SEC-10, the donors' way: the activator is legitimately NULL on some
    // paths -- the delay arm above warns about exactly that -- and it is
    // guarded where it is read rather than replaced where it enters.  Nothing
    // ever assigns `activator` on a `func_door`, and a `func_clock` that is
    // not START_OFF never has one either; both hand what they hold to here,
    // and target_explosion_explode calls back in with the one it was handed.
    // Without this test an empty server dies by itself a few seconds into a
    // map carrying a `target_explosion` with a `message` on it.  Every `use`
    // callback that reads its activator carries the same kind of test, and
    // tools/nullattacker.py sweeps them all (rocketarena2@99f8bb2,
    // osp-tourney@a8d1725, q2pro's feature/mission-packs@02857024).
    if ((ent->message) && activator && !(activator->svflags & SVF_MONSTER)) {
        // RA2 routes entity messages through its menu so one does not wipe an
        // open arena menu.  menu_centerprint falls back to gi.centerprintf when
        // no menu is open, so it WOULD be transparent everywhere -- but "it
        // happens to be transparent" is not a gate, and donorgate.py is right
        // to say so.
        if (G_Ruleset() == RULESET_ARENA)
            menu_centerprint(activator, ent->message);
        else
            gi.centerprintf(activator, "%s", ent->message);
        if (ent->noise_index)
            gi.sound(activator, CHAN_AUTO, ent->noise_index, 1, ATTN_NORM, 0);
        else
            gi.sound(activator, CHAN_AUTO, gi.soundindex("misc/talk1.wav"), 1, ATTN_NORM, 0);
    }

//
// kill killtargets
//
    if (ent->killtarget) {
        t = NULL;
        while ((t = G_Find(t, FOFS(targetname), ent->killtarget))) {
            // PMM - if this entity is part of a train, cleanly remove it
            //
            // Ground Zero's loop, made safe.  It walked on until it found the
            // target, with one `done` for the whole call.  G_Find answers in
            // edict order, so a killtarget naming a team's master and its slaves
            // -- biggun's "llama" doors, city2's "traphider" -- frees the master
            // first, and the next slave's walk ran off the end of the master's
            // cleared chain into NULL; and from the second slave on `done` was
            // already set and the slave stayed linked.  Each target now walks
            // its own master's chain, and a master that is gone has no chain
            // left to walk (R-MP-11).
            if (t->flags & FL_TEAMSLAVE) {
                for (master = t->teammaster; master && master->inuse; master = master->teamchain) {
                    if (master->teamchain == t) {
                        master->teamchain = t->teamchain;
                        break;
                    }
                }
            }
            // PMM
            G_FreeEdict(t);
            if (!ent->inuse) {
                gi.dprintf("entity was removed while using killtargets\n");
                return;
            }
        }
    }

//
// fire targets
//
    if (ent->target) {
        t = NULL;
        while ((t = G_Find(t, FOFS(targetname), ent->target))) {
            // doors fire area portals in a specific way
            if (!Q_stricmp(t->classname, "func_areaportal") &&
                (!Q_stricmp(ent->classname, "func_door") || !Q_stricmp(ent->classname, "func_door_rotating")))
                continue;

            if (t == ent) {
                gi.dprintf("WARNING: Entity used itself.\n");
            } else {
                if (t->use)
                    t->use(t, ent, activator);
            }
            if (!ent->inuse) {
                gi.dprintf("entity was removed while using targets\n");
                return;
            }
        }
    }
}

static const vec3_t VEC_UP       = {0, -1, 0};
static const vec3_t MOVEDIR_UP   = {0, 0, 1};
static const vec3_t VEC_DOWN     = {0, -2, 0};
static const vec3_t MOVEDIR_DOWN = {0, 0, -1};

void G_SetMovedir(vec3_t angles, vec3_t movedir)
{
    if (VectorCompare(angles, VEC_UP))
        VectorCopy(MOVEDIR_UP, movedir);
    else if (VectorCompare(angles, VEC_DOWN))
        VectorCopy(MOVEDIR_DOWN, movedir);
    else
        AngleVectors(angles, movedir, NULL, NULL);

    VectorClear(angles);
}

float vectoyaw(vec3_t vec)
{
    float   yaw;

    // PMM - fixed to correct for pitch of 0
    if (/*vec[YAW] == 0 &&*/ vec[PITCH] == 0)
        if (vec[YAW] == 0)
            yaw = 0;
        else if (vec[YAW] > 0)
            yaw = 90;
        else
            yaw = 270;
    else {
        yaw = (int)RAD2DEG(atan2f(vec[YAW], vec[PITCH]));
        if (yaw < 0)
            yaw += 360;
    }

    return yaw;
}

float vectoyaw2(vec3_t vec)
{
    float   yaw;

    // PMM - fixed to correct for pitch of 0
    if (/*vec[YAW] == 0 &&*/ vec[PITCH] == 0)
        if (vec[YAW] == 0)
            yaw = 0;
        else if (vec[YAW] > 0)
            yaw = 90;
        else
            yaw = 270;
    else {
        yaw = RAD2DEG(atan2f(vec[YAW], vec[PITCH]));
        if (yaw < 0)
            yaw += 360;
    }

    return yaw;
}

void vectoangles(vec3_t value1, vec3_t angles)
{
    float   forward;
    float   yaw, pitch;

    if (value1[1] == 0 && value1[0] == 0) {
        yaw = 0;
        if (value1[2] > 0)
            pitch = 90;
        else
            pitch = 270;
    } else {
        // PMM - fixed to correct for pitch of 0
        if (value1[0])
            yaw = (int)RAD2DEG(atan2f(value1[1], value1[0]));
        else if (value1[1] > 0)
            yaw = 90;
        else
            yaw = 270;
        if (yaw < 0)
            yaw += 360;

        forward = sqrtf(value1[0] * value1[0] + value1[1] * value1[1]);
        pitch = (int)RAD2DEG(atan2f(value1[2], forward));
        if (pitch < 0)
            pitch += 360;
    }

    angles[PITCH] = -pitch;
    angles[YAW] = yaw;
    angles[ROLL] = 0;
}

void vectoangles2(const vec3_t value1, vec3_t angles)
{
    float   forward;
    float   yaw, pitch;

    if (value1[1] == 0 && value1[0] == 0) {
        yaw = 0;
        if (value1[2] > 0)
            pitch = 90;
        else
            pitch = 270;
    } else {
        // PMM - fixed to correct for pitch of 0
        if (value1[0])
            yaw = RAD2DEG(atan2f(value1[1], value1[0]));
        else if (value1[1] > 0)
            yaw = 90;
        else
            yaw = 270;

        if (yaw < 0)
            yaw += 360;

        forward = sqrtf(value1[0] * value1[0] + value1[1] * value1[1]);
        pitch = RAD2DEG(atan2f(value1[2], forward));
        if (pitch < 0)
            pitch += 360;
    }

    angles[PITCH] = -pitch;
    angles[YAW] = yaw;
    angles[ROLL] = 0;
}

char *G_CopyString(char *in)
{
    char    *out;
    size_t  len = strlen(in) + 1;

    // The block is allocated from the source's own length, so this is a
    // sized copy rather than an unbounded one.
    out = gi.TagMalloc(len, TAG_LEVEL);
    memcpy(out, in, len);
    return out;
}

void G_InitEdict(edict_t *e)
{
    // ROGUE
    // FIXME -
    //   this fixes a bug somewhere that is settling "nextthink" for an entity that has
    //   already been released.  nextthink is being set to FRAMETIME after level.time,
    //   since freetime = nextthink - 0.1
    if (e->nextthink) {
//      if ((g_showlogic) && (g_showlogic->value))
//          gi.dprintf ("G_SPAWN:  Fixed bad nextthink time\n");
        e->nextthink = 0;
    }
    // ROGUE

    e->inuse = true;
    e->classname = "noclass";
    e->gravity = 1.0f;
    e->s.number = e - g_edicts;

//PGM - do this before calling the spawn function so it can be overridden.
#ifdef ROGUE_GRAVITY
    e->gravityVector[0] =  0.0f;
    e->gravityVector[1] =  0.0f;
    e->gravityVector[2] = -1.0f;
#endif
//PGM
}

/*
=================
G_Spawn

Either finds a free edict, or allocates a new one.
Try to avoid reusing an entity that was recently freed, because it
can cause the client to think the entity morphed into something else
instead of being removed and recreated, which can cause interpolated
angles and bad trails.
=================
*/
edict_t *G_Spawn(void)
{
    int         i;
    edict_t     *e;

    e = &g_edicts[game.maxclients + 1];
    for (i = game.maxclients + 1; i < globals.num_edicts; i++, e++) {
        // the first couple seconds of server time can involve a lot of
        // freeing and allocating, so relax the replacement policy
        if (!e->inuse && (e->freetime < 2 || level.time - e->freetime > 0.5f)) {
            G_InitEdict(e);
            return e;
        }
    }

    if (i == game.maxentities)
        gi.error("ED_Alloc: no free edicts");

    globals.num_edicts++;
    G_InitEdict(e);
    return e;
}

/*
=================
G_FreeEdict

Marks the edict as free
=================
*/
void G_FreeEdict(edict_t *ed)
{
    // A quad or an invulnerability that ran out its own clock in the
    // WORLD, with nobody holding it.  Asked in the tourney layer because the
    // two `use` functions this has to recognise are static to g_items.c; the
    // other half of the pair, a powerup that expired ON a player, is p_view.c's.
    if (G_IsOspRuleset() && ed->item)
        OSP_itemFreed(ed);

    gi.unlinkentity(ed);        // unlink from world

    if ((ed - g_edicts) <= (game.maxclients + BODY_QUEUE_SIZE)) {
//      gi.dprintf("tried to free special edict\n");
        return;
    }

    memset(ed, 0, sizeof(*ed));
    ed->classname = "freed";
    ed->freetime = level.time;
    ed->inuse = false;
}

/*
=============
G_RemoveDeployables

The mines, teslas, traps and thrown nukes the two mission packs let a player
leave in the world outlive the rounds and matches other rulesets are made of: a
prox mine lasts 45 seconds and a tesla or trap 30, against arena's three
seconds of results and five of countdown, and against OSP's match start.  Left
there, a device laid in one round kills in the next and credits its layer.  This
takes them out of the world without a bang, with the trigger fields a prox and
a tesla carry on `teamchain`, for every layer `mine` answers true for -- or
every one at all when `mine` is NULL.  A layer is the device's `teammaster` for
Ground Zero's three and its `owner` for The Reckoning's trap, and may be NULL.

Ground Zero's doppleganger and spheres are devices too.  A doppleganger lives
30 seconds, names its layer in `teammaster` and its body on `teamchain`, and
when shot spawns a hunter or vengeance sphere that hits for 10000 credited to
that layer -- by then perhaps the slot's next occupant.  A sphere names its
layer in `owner`, or in `teammaster` when a doppleganger spawned it, and can
outlive the `owned_sphere` link that would otherwise have freed it; that link
is cleared with it.  (R-RA-15, R-MP-8.)
=============
*/
void G_RemoveDeployables(bool (*mine)(edict_t *layer, void *arg), void *arg)
{
    edict_t *e, *c, *next, *layer;
    int     i;
    bool    tesla, prox, dopple, sphere;

    for (i = game.maxclients + 1, e = g_edicts + i; i < globals.num_edicts; i++, e++) {
        if (!e->inuse || !e->classname)
            continue;
        tesla = !strcmp(e->classname, "tesla");
        prox = !strcmp(e->classname, "prox");
        dopple = !strcmp(e->classname, "doppleganger");
        sphere = !strcmp(e->classname, "sphere");
        if (tesla || prox || dopple || !strcmp(e->classname, "nuke"))
            layer = e->teammaster;
        else if (!strcmp(e->classname, "htrap"))
            layer = e->owner;
        else if (sphere)
            layer = (e->spawnflags & SPHERE_DOPPLEGANGER) ? e->teammaster : e->owner;
        else
            continue;
        if (mine && !mine(layer, arg))
            continue;
        // tesla_remove frees its whole chain; Prox_Explode frees the one field
        // it owns.  The same here, without the explosion either would make.
        if (tesla || prox) {
            for (c = e->teamchain; c; c = next) {
                next = c->teamchain;
                if (tesla || c->owner == e)
                    G_FreeEdict(c);
                if (prox)
                    break;
            }
        }
        // doppleganger_timeout frees the body with the base.
        if (dopple && e->teamchain && e->teamchain->teammaster == e)
            G_FreeEdict(e->teamchain);
        if (sphere && layer && layer->client && layer->client->owned_sphere == e)
            layer->client->owned_sphere = NULL;
        G_FreeEdict(e);
    }
}

/*
=============
G_PlayerResetGrapple

Let go of the offhand hook, whichever one the ruleset hands out.  Under the
four OSP rulesets `ctf_grapple` holds tourney's hook, which tourney releases
quietly; everywhere else it is Threewave's grapple, whose reset also plays the
CTF pak's grreset.wav -- a sound the OSP rulesets neither ship nor precache,
so asking Threewave to release tourney's hook registered it mid-game.
=============
*/
void G_PlayerResetGrapple(edict_t *ent)
{
    if (G_IsOspRuleset())
        OSP_hookoff_cmd(ent);
    else
        CTFPlayerResetGrapple(ent);
}

/*
=============
G_LoginThrottled

R-SEC-12: no login is a password oracle.  The engine takes eight string
commands a packet and puts no limit of its own on them, so an unthrottled
password test is hundreds of guesses a second, and four of them in this library
hand out control of the server -- tourney's `referee`, which also accepts the
rcon password, Threewave's `admin`, RA2's admin code and the bot menu's rcon
password.  They share one backoff: a wrong answer costs 2 seconds, doubling to
30, on the connection's own clock (`pers.login_fails`), and a right one
clears it.
=============
*/
bool G_LoginThrottled(edict_t *ent)
{
    const client_persistant_t *p = &ent->client->pers;

    return p->login_fails && (int64_t)time(NULL) < p->login_retry;
}

void G_LoginFailed(edict_t *ent)
{
    client_persistant_t *p = &ent->client->pers;

    if (p->login_fails < 5)
        p->login_fails++;
    p->login_retry = (int64_t)time(NULL) +
                     (p->login_fails >= 5 ? 30 : 1 << p->login_fails);
}

void G_LoginSucceeded(edict_t *ent)
{
    ent->client->pers.login_fails = 0;
    ent->client->pers.login_retry = 0;
}

/*
============
G_TouchTriggers

============
*/
void    G_TouchTriggers(edict_t *ent)
{
    int         i, num;
    edict_t     *touch[MAX_EDICTS_OLD], *hit;

    // dead things don't activate triggers!
    if ((ent->client || (ent->svflags & SVF_MONSTER)) && (ent->health <= 0))
        return;

    num = gi.BoxEdicts(ent->absmin, ent->absmax, touch, q_countof(touch), AREA_TRIGGERS);

    // be careful, it is possible to have an entity in this
    // list removed before we get to it (killtriggered)
    for (i = 0; i < num; i++) {
        hit = touch[i];
        if (!hit->inuse)
            continue;
        if (!hit->touch)
            continue;
        hit->touch(hit, ent, NULL, NULL);
    }
}

/*

==============================================================================

Kill box

==============================================================================
*/

/*
=================
KillBox

Kills all entities that would touch the proposed new positioning
of ent.  Ent should be unlinked before calling this!
=================
*/
bool KillBox(edict_t *ent)
{
    trace_t     tr;

    if (G_Ruleset() == RULESET_ARENA) {
        // RA2 telefrags only between fighting players.  Its first test cannot
        // be inherited: KillBox has eight non-client callers in this tree --
        // monsters teleporting in, movers, target_spawner -- and `return true`
        // for all of them would stop every one of those telefragging.
        if (!ent->client)
            return true;
        ent->client->resp.spawn_recheck = 0;
        if (ent->client->resp.fightstate == FIGHT_SPECTATING)
            return true;
    }

    while (1) {
        tr = gi.trace(ent->s.origin, ent->mins, ent->maxs, ent->s.origin, NULL, MASK_PLAYERSOLID);
        if (!tr.ent)
            break;

        if (G_Ruleset() == RULESET_ARENA &&
            tr.ent->client && !tr.ent->takedamage && tr.ent->solid) {
            vec3_t  angle, forward;

            tr.ent->solid = SOLID_NOT;
            ent->solid = SOLID_NOT;

            angle[YAW] = rand() % 360;
            angle[PITCH] = 0;
            angle[ROLL] = 0;
            AngleVectors(angle, forward, NULL, NULL);
            VectorScale(forward, 600, forward);

            // Opposite directions, which is the whole point and which the
            // donor does not do.  RA2 adds the same vector to both bodies:
            // 600 units per second each, along one random yaw, so the pair
            // drifts as a pair and the distance between them never changes.
            // Two players who drew the same spawn point are therefore still
            // inside each other after the "push apart", which is exactly the
            // report -- "they did not telefrag or push away each other, their
            // bodies overlapped like siamese twins".  The telefrag underneath
            // cannot save it either: an arena fighter is `takedamage DAMAGE_NO`
            // until ASTATE_FIGHTING, and `!tr.ent->takedamage` is the condition
            // for taking this branch instead of that one, so during a countdown
            // this IS the whole mechanism.
            VectorAdd(tr.ent->velocity, forward, tr.ent->velocity);
            VectorSubtract(ent->velocity, forward, ent->velocity);

            tr.ent->client->resp.spawn_recheck = level.framenum + 0.5f / FRAMETIME;
            ent->client->resp.spawn_recheck = level.framenum + 0.5f / FRAMETIME;
            continue;
        }

        // nail it
        T_Damage(tr.ent, ent, ent, vec3_origin, ent->s.origin, vec3_origin, 100000, 0, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);

        // if we didn't kill it, fail
        if (tr.ent->solid)
            return false;
    }

    return true;        // all clear
}
