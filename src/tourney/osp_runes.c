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
// OSP Tourney DM v2.75, from osp-tourney@1d8427e (doc/provenance.md).
// Donor-only: baseq2 has no counterpart, so it lives in src/tourney/ rather
// than being merged into a spine file (R-CORE-7).  The reconstruction's
// asm-matching address comments are stripped -- SPECS.md N1 makes those oracles
// meaningless here, and they survive at the pin.
// osp_runes.c -- <INVENTED FILENAME>. The five Lithium-style runes.
//
// Resist / Strength / Haste / Regeneration / Vampire, held one at a time and
// carried in the `SID_OSP_RUNE_*` stats (tourney slots 22..26) so the client HUD
// can draw them -- reached through G_SetStat/G_GetStat, never by indexing
// `ps.stats[]` with the id, which is R-132.  `r_count[]` tracks how many of each
// are loose in the world and `rune_spawnpoint[]` is the pool of places one may
// reappear.

#include "g_local.h"
#include "tourney/osp_types.h"
#include "tourney/osp_stats.h"

int runespawn = 0;
int rune_spawncount = 0;
edict_t * rune_spawnpoint[50];
int r_count[5];

// `r_count[]` is indexed as `item->quantity - SID_OSP_RUNE_RESIST`, which is the
// donor's idiom (`quantity` holds the rune's id) and is correct only while the
// five ids are five CONSECUTIVE values in that order.  Nothing in STATSLOT_MAP's
// shape enforces that -- the ordinals come from the order rows happen to be
// written in, so inserting a row between two runes, or reordering them, would
// silently turn this subtraction into an out-of-bounds index the way R-132's
// mismatch did.  Asserted rather than commented, because the failure is a
// negative array index and the compiler will otherwise emit it happily.
_Static_assert(SID_OSP_RUNE_STRENGTH == SID_OSP_RUNE_RESIST + 1 &&
               SID_OSP_RUNE_HASTE    == SID_OSP_RUNE_RESIST + 2 &&
               SID_OSP_RUNE_REGEN    == SID_OSP_RUNE_RESIST + 3 &&
               SID_OSP_RUNE_VAMPIRE  == SID_OSP_RUNE_RESIST + 4,
               "the five SID_OSP_RUNE_* ids must stay consecutive and in order: "
               "r_count[] and item->quantity index by their difference");

// The five rune classnames, NULL-terminated.
static char *runenames[] = {
    "item_rune1",
    "item_rune2",
    "item_rune3",
    "item_rune4",
    "item_rune5",
    NULL
};

// Defined at the very end of the file, so it needs a forward declaration
// here.  Name <INVENTED>.
void OSP_runeSpawnThink(edict_t *self);

/*
==============
OSP_What_Rune

Which rune this client is carrying, or NULL.
==============
*/
const gitem_t *OSP_What_Rune(edict_t *ent)
{
    int     i;
    const gitem_t   *item;

    i = 0;
    while (runenames[i]) {
        item = FindItemByClassname(runenames[i]);
        if (item && ent->client->pers.inventory[ITEM_INDEX(item)])
            return item;
        i++;
    }
    return NULL;
}

/*
==============
OSP_Pickup_Rune

One rune at a time: refuse the pickup outright if the player already holds any
of the five.
==============
*/
bool OSP_Pickup_Rune(edict_t *ent, edict_t *other)
{
    int     i;
    const gitem_t   *item;

    if (!G_IsOspRuleset())
        return false;

    i = 0;
    while (runenames[i]) {
        item = FindItemByClassname(runenames[i]);
        if (item && other->client->pers.inventory[ITEM_INDEX(item)])
            return false;
        i++;
    }

    other->client->resp.osp_r23c = 0;
    other->client->pers.inventory[ITEM_INDEX(ent->item)]++;
    other->client->osp_t06c = level.time;
    G_SetStat(other, ent->item->quantity, 1);
    other->client->resp.osp_r200 = ent - g_edicts;

    item = FindItemByClassname(ent->classname);
    OSP_Stats_ItemPickup(item->pickup_name, ent - g_edicts, other);
    OSP_Stats_ItemUse(item->pickup_name, other);

    if (OSP_checkMaxRunes())
        OSP_checkMinRunes();

    return true;
}

/*
==============
OSP_randomRuneSpot     <INVENTED NAME>

Returns a random entry of the rune spawn pool, or NULL when it is empty.
`static` in the original, and inlined at all three of its call sites, which is
why it has to be defined here ahead of them.
==============
*/
static edict_t *OSP_randomRuneSpot(void)
{
    int     n;

    if (!rune_spawncount)
        return NULL;

    n = Q_rand() % rune_spawncount;
    return rune_spawnpoint[n];
}

/*
==============
OSP_runeThink

A rune lying in the world. If there is somewhere for it to go it is taken out
of play and the minimum-rune check reseeds the level; otherwise it waits a
minute and asks again.
==============
*/
void OSP_runeThink(edict_t *self)
{
    edict_t *spot;

    spot = OSP_randomRuneSpot();

    if (spot) {
        r_count[self->item->quantity - SID_OSP_RUNE_RESIST]--;
        G_FreeEdict(self);
        OSP_checkMinRunes();
    } else {
        self->nextthink = level.framenum + 60 * BASE_FRAMERATE;
        self->think = OSP_runeThink;
    }
}

/*
==============
OSP_Drop_Rune

The five runes are told apart in the world by their colour shell, not by
model -- runes_model gives them all the same one.
==============
*/
void OSP_Drop_Rune(edict_t *ent, const gitem_t *item)
{
    edict_t *dropped;

    if (!G_IsOspRuleset())
        return;

    dropped = Drop_Item(ent, item);

    if (dropped->item->quantity == SID_OSP_RUNE_RESIST)
        dropped->s.renderfx |= RF_SHELL_BLUE;
    else if (dropped->item->quantity == SID_OSP_RUNE_STRENGTH)
        dropped->s.renderfx |= RF_SHELL_RED;
    else if (dropped->item->quantity == SID_OSP_RUNE_HASTE)
        dropped->s.renderfx |= RF_SHELL_RED | RF_SHELL_GREEN;
    else if (dropped->item->quantity == SID_OSP_RUNE_REGEN)
        dropped->s.renderfx |= RF_SHELL_GREEN;
    else if (dropped->item->quantity == SID_OSP_RUNE_VAMPIRE)
        dropped->s.renderfx |= RF_SHELL_BLUE | RF_SHELL_RED;

    OSP_checkMinRunes();
    ent->client->pers.inventory[ITEM_INDEX(item)] = 0;
    OSP_zeroRuneStats(ent);
    OSP_Stats_ItemDrop(item->pickup_name, dropped - g_edicts, ent);
}

/*
==============
OSP_deadDropRune

Drop whichever rune the player is holding where they died, scattered and with
a minute on the clock.  Same colour-shell mapping as OSP_Drop_Rune.
==============
*/
void OSP_deadDropRune(edict_t *ent)
{
    int     i;
    const gitem_t   *item;
    edict_t *dropped;

    i = 0;
    while (runenames[i]) {
        item = FindItemByClassname(runenames[i]);
        if (item && ent->client->pers.inventory[ITEM_INDEX(item)]) {
            dropped = Drop_Item(ent, item);

            if (dropped->item->quantity == SID_OSP_RUNE_RESIST)
                dropped->s.renderfx |= RF_SHELL_BLUE;
            else if (dropped->item->quantity == SID_OSP_RUNE_STRENGTH)
                dropped->s.renderfx |= RF_SHELL_RED;
            else if (dropped->item->quantity == SID_OSP_RUNE_HASTE)
                dropped->s.renderfx |= RF_SHELL_RED | RF_SHELL_GREEN;
            else if (dropped->item->quantity == SID_OSP_RUNE_REGEN)
                dropped->s.renderfx |= RF_SHELL_GREEN;
            else if (dropped->item->quantity == SID_OSP_RUNE_VAMPIRE)
                dropped->s.renderfx |= RF_SHELL_BLUE | RF_SHELL_RED;

            dropped->velocity[0] = Q_rand() % 600 - 300;
            dropped->velocity[1] = Q_rand() % 600 - 300;
            dropped->nextthink = level.framenum + 60 * BASE_FRAMERATE;
            dropped->think = OSP_runeThink;
            dropped->owner = NULL;
            ent->client->pers.inventory[ITEM_INDEX(item)] = 0;
            OSP_zeroRuneStats(ent);
            OSP_Stats_ItemDrop(item->pickup_name, dropped - g_edicts, ent);
            OSP_checkMinRunes();
        }
        i++;
    }
}

/*
==============
OSP_spawnRuneAt

Put one rune into the world at `spot`, tossed a little way out along a random
yaw.  `static` in the original.  The tail call back into OSP_checkMinRunes
makes the pair mutually recursive: spawning one rune re-tests whether the
minimum is met.
==============
*/
static void OSP_spawnRuneAt(const gitem_t *item, edict_t *spot)
{
    edict_t *ent;
    vec3_t  forward, right;
    vec3_t  angles;

    ent = G_Spawn();
    ent->classname = item->classname;
    ent->item = item;
    ent->spawnflags = DROPPED_ITEM;
    ent->s.effects = item->world_model_flags;
    ent->s.renderfx = RF_GLOW;

    if (ent->item->quantity == SID_OSP_RUNE_RESIST)
        ent->s.renderfx |= RF_SHELL_BLUE;
    else if (ent->item->quantity == SID_OSP_RUNE_STRENGTH)
        ent->s.renderfx |= RF_SHELL_RED;
    else if (ent->item->quantity == SID_OSP_RUNE_HASTE)
        ent->s.renderfx |= RF_SHELL_RED | RF_SHELL_GREEN;
    else if (ent->item->quantity == SID_OSP_RUNE_REGEN)
        ent->s.renderfx |= RF_SHELL_GREEN;
    else if (ent->item->quantity == SID_OSP_RUNE_VAMPIRE)
        ent->s.renderfx |= RF_SHELL_BLUE | RF_SHELL_RED;

    r_count[ent->item->quantity - SID_OSP_RUNE_RESIST]++;

    VectorSet(ent->mins, -15, -15, -15);
    VectorSet(ent->maxs, 15, 15, 15);
    gi.setmodel(ent, runes_model->string);
    ent->solid = SOLID_TRIGGER;
    ent->movetype = MOVETYPE_TOSS;
    ent->touch = Touch_Item;
    ent->owner = ent;

    VectorSet(angles, 0, Q_rand() % 360, 0);
    AngleVectors(angles, forward, right, NULL);
    VectorCopy(spot->s.origin, ent->s.origin);
    ent->s.origin[2] += 16;
    VectorScale(forward, 100, ent->velocity);
    ent->velocity[2] = 300;
    ent->nextthink = level.framenum + 60 * BASE_FRAMERATE;
    ent->think = OSP_runeThink;

    gi.linkentity(ent);
    OSP_checkMinRunes();
}

void OSP_respawnRune(edict_t *ent)
{
    edict_t *spot;

    spot = OSP_randomRuneSpot();
    if (spot)
        OSP_spawnRuneAt(ent->item, spot);
    G_FreeEdict(ent);
}

/*
==============
OSP_setupRuneSpawn

Build the pool of places a rune may appear from the map's own item and spawn
entities, then hang a one-shot thinker off `delay` seconds from now to seed the
level.  Runs once per map -- `runespawn` is the latch.
==============
*/
void OSP_setupRuneSpawn(int delay)
{
    int     i;
    edict_t *ent;

    if (runespawn)
        return;

    for (i = 0; i < 5; i++)
        r_count[i] = 0;

    ent = &g_edicts[(int)game.maxclients + 1];
    rune_spawncount = 0;

    for (i = game.maxclients + 1; i < globals.num_edicts; i++, ent++) {
        if (!ent->inuse)
            continue;

        if (!strstr(ent->classname, "ammo_") &&
            !strstr(ent->classname, "weapon_") &&
            !strstr(ent->classname, "item_") &&
            !strstr(ent->classname, "misc_teleporter") &&
            !strstr(ent->classname, "info_player"))
            continue;

        rune_spawnpoint[rune_spawncount++] = ent;

        if (rune_spawncount >= 50)
            break;
    }

    ent = G_Spawn();
    ent->nextthink = level.framenum + (2 + delay) * BASE_FRAMERATE;
    ent->think = OSP_runeSpawnThink;
    runespawn = 1;
}

/*
==============
OSP_runesApplyResistance

Soak incoming damage by the resist factor, with a sound and an optional HUD
flash.  The 0.2 that replaces the volume when the player is silenced is a float
constant; the 0.2 added to level.time is a double.
==============
*/
int OSP_runesApplyResistance(edict_t *ent, int damage)
{
    float   volume = 1.0f;

    if (ent->client && ent->client->silencer_shots)
        volume = 0.2f;

    if (damage && ent->client && G_GetStat(ent, SID_OSP_RUNE_RESIST)) {
        gi.sound(ent, CHAN_VOICE, gi.soundindex("world/force2.wav"), volume, ATTN_NORM, 0);
        if ((int)runes_flash->value)
            ent->client->osp_t074 = level.time + 0.2f;
        // runes_resist is a cvar; a zero or negative one would make this
        // division produce infinity or flip the damage's sign, and the
        // implicit conversion back to int is undefined for both.
        if (runes_resist->value > 0)
            return damage / runes_resist->value;
        return 0;
    }
    return damage;
}

int OSP_runesApplyStrength(edict_t *ent, int damage)
{
    if (damage && ent->client && G_GetStat(ent, SID_OSP_RUNE_STRENGTH))
        return (int)(runes_strength->value * damage);
    return damage;
}

bool OSP_runesApplyStrengthSound(edict_t *ent)
{
    float   volume = 1.0f;

    if (ent->client && G_GetStat(ent, SID_OSP_RUNE_STRENGTH)) {
        if (ent->client->silencer_shots)
            volume = 0.2f;

        if ((int)runes_flash->value)
            ent->client->osp_t078 = level.time + 0.2f;

        gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage3.wav"), volume, ATTN_NORM, 0);
        return true;
    }
    return false;
}

bool OSP_runesHasHaste(edict_t *ent)
{
    if (ent->client && G_GetStat(ent, SID_OSP_RUNE_HASTE))
        return true;
    return false;
}

void OSP_runesApplyHasteSound(edict_t *ent)
{
    float   volume = 1.0f;

    if (ent->client && ent->client->silencer_shots)
        volume = 0.2f;

    if (ent->client) {
        if (G_GetStat(ent, SID_OSP_RUNE_HASTE)) {
            if ((int)runes_flash->value)
                ent->client->osp_t07c = level.time + 0.2f;

            // rate-limit the whine to ten a second
            if (ent->client->osp_t070 < level.time) {
                ent->client->osp_t070 = level.time + 0.1f;
                gi.sound(ent, CHAN_VOICE, gi.soundindex("world/x_light.wav"), volume, ATTN_NORM, 0);
            }
        }
    }
}

void OSP_runesApplyRegeneration(edict_t *ent)
{
    int     healamt = 0;
    float   volume = 1.0f;
    gclient_t   *clientp;
    int     armor;

    clientp = ent->client;
    if (!clientp)
        return;

    if (clientp->silencer_shots)
        volume = 0.2f;

    if (G_GetStat(ent, SID_OSP_RUNE_REGEN)) {
        if (clientp->osp_t06c < level.time) {
            clientp->osp_t06c = level.time;

            if (ent->health < (int)runes_regen_hmax->value) {
                ent->health += 3;
                if (ent->health > (int)runes_regen_hmax->value)
                    ent->health = (int)runes_regen_hmax->value;

                clientp->osp_t06c += 0.8f;
                healamt = 1;

                if ((int)runes_flash->value)
                    ent->client->osp_t080 = level.time + 0.2f;
            }

            armor = ArmorIndex(ent);
            if (armor) {
                if (clientp->pers.inventory[armor] <
                    (int)runes_regen_amax->value) {
                    clientp->pers.inventory[armor] += 3;
                    if (clientp->pers.inventory[armor] >
                        (int)runes_regen_amax->value)
                        clientp->pers.inventory[armor] =
                            (int)runes_regen_amax->value;

                    clientp->osp_t06c += 0.8f;
                    healamt = 1;

                    if ((int)runes_flash->value)
                        ent->client->osp_t080 = level.time + 0.2f;
                }
            }
        }

        if (healamt) {
            if (ent->client->osp_t070 < level.time) {
                ent->client->osp_t070 = level.time + 0.1f;
                gi.sound(ent, CHAN_VOICE, gi.soundindex("items/l_health.wav"),
                         volume, ATTN_NORM, 0);
            }
        }
    }
}

bool OSP_runesHasRegeneration(edict_t *ent)
{
    if (G_GetStat(ent, SID_OSP_RUNE_REGEN))
        return true;
    return false;
}

bool OSP_runesHasVampire(edict_t *ent)
{
    if (G_GetStat(ent, SID_OSP_RUNE_VAMPIRE))
        return true;
    return false;
}

void OSP_runesApplyVampire(edict_t *ent, int damage)
{
    float   volume = 1.0f;

    if (ent->client && ent->client->silencer_shots)
        volume = 0.2f;

    if (ent->client && G_GetStat(ent, SID_OSP_RUNE_VAMPIRE) &&
        ent->health < (int)runes_vampire_max->value) {
        ent->health += (int)(runes_vampire->value * damage);

        if (ent->health > (int)runes_vampire_max->value)
            ent->health = (int)runes_vampire_max->value;

        if ((int)runes_flash->value)
            ent->client->osp_t084 = level.time + 0.2f;

        gi.sound(ent, CHAN_VOICE, gi.soundindex("makron/pain2.wav"), volume,
                 ATTN_NORM, 0);
    }
}

void OSP_zeroRuneStats(edict_t *ent)
{
    G_SetStat(ent, SID_OSP_RUNE_RESIST, 0);
    G_SetStat(ent, SID_OSP_RUNE_STRENGTH, 0);
    G_SetStat(ent, SID_OSP_RUNE_HASTE, 0);
    G_SetStat(ent, SID_OSP_RUNE_REGEN, 0);
    G_SetStat(ent, SID_OSP_RUNE_VAMPIRE, 0);
}

/*
==============
OSP_removeRunes

Sweep every rune out of the world and out of every inventory.
==============
*/
void OSP_removeRunes(void)
{
    int     i;
    int     j;
    const gitem_t   *item= NULL;
    edict_t *ent;

    ent = g_edicts;
    for (i = 0; i < globals.num_edicts; i++, ent++) {
        if (ent->inuse) {
            if (!strstr(ent->classname, "item_rune"))
                continue;
            G_FreeEdict(ent);
        }
    }

    for (i = 1; i <= game.maxclients; i++) {
        ent = g_edicts + i;
        if (ent->inuse) {
            if (!ent->client)
                continue;

            for (j = 0; j < 5; j++) {
                item = FindItemByClassname(runenames[j]);
                ent->client->pers.inventory[ITEM_INDEX(item)] = 0;
            }

            OSP_zeroRuneStats(ent);
        }
    }

    for (i = 0; i < 5; i++)
        r_count[i] = 0;
}

/*
==============
OSP_findMinRune

Which enabled rune type has the fewest instances loose in the world.  Ties are
broken at random, which is why the second pass collects every index that hits
the minimum instead of stopping at the first.
==============
*/
int OSP_findMinRune(void)
{
    int     rmins[5];
    int     count;
    int     i;
    int     bit;
    int     minrune;

    bit = 1;
    minrune = 9999999;
    count = 0;
    for (i = 0; i < 5; i++) {
        if ((rune_stat & bit) && minrune > r_count[i])
            minrune = r_count[i];
        bit <<= 1;
    }

    bit = 1;
    for (i = 0; i < 5; i++) {
        if ((rune_stat & bit) && r_count[i] == minrune)
            rmins[count++] = i;
        bit <<= 1;
    }

    if (!count)
        return -1;

    return rmins[Q_rand() % count];
}

/*
==============
OSP_checkMinRunes

Reseed the level if there are fewer runes about than the per-player ratio or
the floor asks for.
==============
*/
void OSP_checkMinRunes(void)
{
    int     totalnum;
    int     i;
    const gitem_t   *items;
    edict_t *spot;

    totalnum = 0;
    for (i = 0; i < 5; i++)
        totalnum += r_count[i];

    if (totalnum < active_clients * runes_perplayer->value ||
        totalnum < (int)runes_min->value) {
        i = OSP_findMinRune();
        if (i < 0)
            return;
        items = FindItemByClassname(runenames[i]);

        spot = OSP_randomRuneSpot();
        OSP_spawnRuneAt(items, spot);
    }
}

bool OSP_checkMaxRunes(void)
{
    int     total;
    int     i;

    total = 0;
    for (i = 0; i < 5; i++)
        total += r_count[i];

    if (total > active_clients * (int)runes_perplayer->value ||
        total > (int)runes_max->value) {
        if (total > (int)runes_min->value)
            return false;
    }
    return true;
}

// The one-shot timer OSP_setupRuneSpawn hangs on a spare edict: seed the
// level, then free itself.  `static` in the original.
void OSP_runeSpawnThink(edict_t *self)
{
    OSP_checkMinRunes();
    G_FreeEdict(self);
}

// The carrier's shell, drawn from p_view.c's G_SetClientEffects.
//
// It lives here rather than in the spine because every field it reads is
// tourney's: `osp_r23c` is the just-picked-up flash that OSP_Pickup_Rune sets,
// and `osp_t074..osp_t084` are the five per-rune flash timers the apply
// functions above stamp.  Putting the branch in p_view.c would have put five
// offset-named members of `client_respawn_t` into a shared file for no gain --
// R-MODE-5's rule is one gate per concept, and this is that gate.
//
// The colours are the donor's, and a tourney player reads them across a room:
//   blue          resistance      red           strength
//   red+green     haste           green         regeneration
//   blue+red      vampire
void OSP_runesShell(edict_t *ent)
{
    gclient_t *cl = ent->client;

    if (!cl)
        return;

    // Just picked one up: two frames of yellow, whichever rune it was.
    if (cl->resp.osp_r23c > level.framenum) {
        ent->s.effects |= EF_COLOR_SHELL;
        ent->s.renderfx |= (RF_SHELL_RED | RF_SHELL_GREEN);
    }

    if (!runes_flash || !(int)runes_flash->value)
        return;

    if (cl->osp_t074 > level.time) {
        ent->s.effects |= EF_COLOR_SHELL;
        ent->s.renderfx |= RF_SHELL_BLUE;
    } else if (cl->osp_t078 > level.time) {
        ent->s.effects |= EF_COLOR_SHELL;
        ent->s.renderfx |= RF_SHELL_RED;
    } else if (cl->osp_t07c > level.time) {
        ent->s.effects |= EF_COLOR_SHELL;
        ent->s.renderfx |= (RF_SHELL_RED | RF_SHELL_GREEN);
    } else if (cl->osp_t080 > level.time) {
        ent->s.effects |= EF_COLOR_SHELL;
        ent->s.renderfx |= RF_SHELL_GREEN;
    } else if (cl->osp_t084 > level.time) {
        ent->s.effects |= EF_COLOR_SHELL;
        ent->s.renderfx |= (RF_SHELL_BLUE | RF_SHELL_RED);
    }
}

// "Does a rune this player is carrying hold their overhealth?"  MegaHealth_think
// asks it once instead of testing two runes and two ceilings itself: the
// regeneration rune holds up to `runes_regen_hmax`, the vampire rune up to
// `runes_vampire_max`, and both are cvars a referee sets.
bool OSP_runesHoldHealth(edict_t *ent)
{
    if (!(rune_stat & RUNE_REGEN) || !ent->client)
        return false;

    if (OSP_runesHasRegeneration(ent) &&
        ent->health <= (int)runes_regen_hmax->value)
        return true;

    if (OSP_runesHasVampire(ent) &&
        ent->health <= (int)runes_vampire_max->value)
        return true;

    return false;
}
