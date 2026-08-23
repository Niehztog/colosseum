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
#include "arena/arena.h"

bool        Pickup_Weapon(edict_t *ent, edict_t *other);
void        Use_Weapon(edict_t *ent, const gitem_t *inv);
void        Use_Weapon2(edict_t *ent, const gitem_t *inv);
void        Drop_Weapon(edict_t *ent, const gitem_t *inv);

void Weapon_Blaster(edict_t *ent);
void Weapon_Shotgun(edict_t *ent);
void Weapon_SuperShotgun(edict_t *ent);
void Weapon_Machinegun(edict_t *ent);
void Weapon_Chaingun(edict_t *ent);
void Weapon_HyperBlaster(edict_t *ent);
void Weapon_RocketLauncher(edict_t *ent);
void Weapon_Grenade(edict_t *ent);
void Weapon_GrenadeLauncher(edict_t *ent);
void Weapon_Railgun(edict_t *ent);
void Weapon_BFG(edict_t *ent);
// RAFAEL
void Weapon_Ionripper(edict_t *ent);
void Weapon_Phalanx(edict_t *ent);
void Weapon_Trap(edict_t *ent);

//=========
//Rogue Weapons
void Weapon_ChainFist(edict_t *ent);
void Weapon_Disintegrator(edict_t *ent);
void Weapon_ETF_Rifle(edict_t *ent);
void Weapon_Heatbeam(edict_t *ent);
void Weapon_Prox(edict_t *ent);
void Weapon_Tesla(edict_t *ent);
void Weapon_ProxLauncher(edict_t *ent);
//void Weapon_Nuke (edict_t *ent);
//Rogue Weapons
//=========

static const gitem_armor_t jacketarmor_info = { 25,  50, .30f, .00f, ARMOR_JACKET};
static const gitem_armor_t combatarmor_info = { 50, 100, .60f, .30f, ARMOR_COMBAT};
static const gitem_armor_t bodyarmor_info   = {100, 200, .80f, .60f, ARMOR_BODY};

static int  jacket_armor_index;
static int  combat_armor_index;
static int  body_armor_index;
static int  power_screen_index;
static int  power_shield_index;

#define HEALTH_IGNORE_MAX   1
#define HEALTH_TIMED        2

static void Use_Quad(edict_t *ent, const gitem_t *item);
// RAFAEL
static void Use_QuadFire(edict_t *ent, const gitem_t *item);

static int  quad_drop_timeout_hack;
// RAFAEL
static int  quad_fire_drop_timeout_hack;

//======================================================================

/*
===============
GetItemByIndex
===============
*/
const gitem_t *GetItemByIndex(int index)
{
    if (index == 0 || index >= game.num_items)
        return NULL;

    return &itemlist[index];
}

/*
===============
FindItemByClassname

===============
*/
const gitem_t *FindItemByClassname(const char *classname)
{
    int     i;
    const gitem_t   *it;

    it = itemlist;
    for (i = 0; i < game.num_items; i++, it++) {
        if (!it->classname)
            continue;
        if (!Q_stricmp(it->classname, classname))
            return it;
    }

    return NULL;
}

/*
===============
FindItem

===============
*/
const gitem_t *FindItem(const char *pickup_name)
{
    int     i;
    const gitem_t   *it;

    it = itemlist;
    for (i = 0; i < game.num_items; i++, it++) {
        if (!it->pickup_name)
            continue;
        if (!Q_stricmp(it->pickup_name, pickup_name))
            return it;
    }

    return NULL;
}

//======================================================================

void DoRespawn(edict_t *ent)
{
    if (ent->team) {
        edict_t *master;
        int count;
        int choice;

        master = ent->teammaster;

        // CTF with weapons-stay respawns the team master rather than a random
        // member, so a weapon stack does not migrate around the map.
        if (G_Ruleset() == RULESET_CTF &&
            ((int)dmflags->value & DF_WEAPONS_STAY) &&
            master->item && (master->item->flags & IT_WEAPON)) {
            ent = master;
        } else {
            for (count = 0, ent = master; ent; ent = ent->chain, count++)
                ;

            choice = Q_rand_uniform(count);

            for (count = 0, ent = master; count < choice; ent = ent->chain, count++)
                ;
        }
    }

//=====
//ROGUE
    if (randomrespawn && randomrespawn->value) {
        edict_t *newEnt;

        newEnt = DoRandomRespawn(ent);

        // if we've changed entities, then do some sleight of hand.
        // otherwise, the old entity will respawn
        if (newEnt) {
            G_FreeEdict(ent);
            ent = newEnt;
        }
    }
//ROGUE
//=====

    ent->svflags &= ~SVF_NOCLIENT;
    ent->solid = SOLID_TRIGGER;
    gi.linkentity(ent);

    // send an effect
    ent->s.event = EV_ITEM_RESPAWN;
}

void SetRespawn(edict_t *ent, float delay)
{
    ent->flags |= FL_RESPAWN;
    ent->svflags |= SVF_NOCLIENT;
    ent->solid = SOLID_NOT;
    ent->nextthink = level.framenum + delay * BASE_FRAMERATE;
    ent->think = DoRespawn;
    gi.linkentity(ent);
}

//======================================================================

static bool Pickup_Powerup(edict_t *ent, edict_t *other)
{
    int     quantity;

    quantity = other->client->pers.inventory[ITEM_INDEX(ent->item)];
    if ((skill->value == 1 && quantity >= 2) || (skill->value >= 2 && quantity >= 1))
        return false;

    if ((coop->value) && (ent->item->flags & IT_STAY_COOP) && (quantity > 0))
        return false;

    other->client->pers.inventory[ITEM_INDEX(ent->item)]++;

    if (deathmatch->value) {
        if (!(ent->spawnflags & DROPPED_ITEM))
            SetRespawn(ent, ent->item->quantity);
        if (((int)dmflags->value & DF_INSTANT_ITEMS) || ((ent->item->use == Use_Quad) && (ent->spawnflags & DROPPED_PLAYER_ITEM))) {
            if ((ent->item->use == Use_Quad) && (ent->spawnflags & DROPPED_PLAYER_ITEM))
                quad_drop_timeout_hack = ent->nextthink - level.framenum;
//PGM
            if (ent->item->use)
                ent->item->use(other, ent->item);
            else
                gi.dprintf("Powerup has no use function!\n");
//PGM
        }
        // RAFAEL
        else if (((int)dmflags->value & DF_INSTANT_ITEMS) || ((ent->item->use == Use_QuadFire) && (ent->spawnflags & DROPPED_PLAYER_ITEM))) {
            if ((ent->item->use == Use_QuadFire) && (ent->spawnflags & DROPPED_PLAYER_ITEM))
                quad_fire_drop_timeout_hack = ent->nextthink - level.framenum;
            ent->item->use(other, ent->item);
        }
    }

    return true;
}

static void Drop_General(edict_t *ent, const gitem_t *item)
{
    Drop_Item(ent, item);
    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);
}

//======================================================================

bool Pickup_Adrenaline(edict_t *ent, edict_t *other)
{
    if (!deathmatch->value)
        other->max_health += 1;

    if (other->health < other->max_health)
        other->health = other->max_health;

    if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->value))
        SetRespawn(ent, ent->item->quantity);

    return true;
}

static bool Pickup_AncientHead(edict_t *ent, edict_t *other)
{
    other->max_health += 2;

    if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->value))
        SetRespawn(ent, ent->item->quantity);

    return true;
}

static bool Pickup_Bandolier(edict_t *ent, edict_t *other)
{
    const gitem_t   *item;
    int     index;

    if (other->client->pers.max_bullets < 250)
        other->client->pers.max_bullets = 250;
    if (other->client->pers.max_shells < 150)
        other->client->pers.max_shells = 150;
    if (other->client->pers.max_cells < 250)
        other->client->pers.max_cells = 250;
    if (other->client->pers.max_slugs < 75)
        other->client->pers.max_slugs = 75;
    // RAFAEL
    if (other->client->pers.max_magslug < 75)
        other->client->pers.max_magslug = 75;
    //PMM
    if (other->client->pers.max_flechettes < 250)
        other->client->pers.max_flechettes = 250;
    //pmm

    item = FindItem("Bullets");
    if (item) {
        index = ITEM_INDEX(item);
        other->client->pers.inventory[index] += item->quantity;
        if (other->client->pers.inventory[index] > other->client->pers.max_bullets)
            other->client->pers.inventory[index] = other->client->pers.max_bullets;
    }

    item = FindItem("Shells");
    if (item) {
        index = ITEM_INDEX(item);
        other->client->pers.inventory[index] += item->quantity;
        if (other->client->pers.inventory[index] > other->client->pers.max_shells)
            other->client->pers.inventory[index] = other->client->pers.max_shells;
    }

    if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->value))
        SetRespawn(ent, ent->item->quantity);

    return true;
}

static bool Pickup_Pack(edict_t *ent, edict_t *other)
{
    const gitem_t   *item;
    int     index;

    if (other->client->pers.max_bullets < 300)
        other->client->pers.max_bullets = 300;
    if (other->client->pers.max_shells < 200)
        other->client->pers.max_shells = 200;
    if (other->client->pers.max_rockets < 100)
        other->client->pers.max_rockets = 100;
    if (other->client->pers.max_grenades < 100)
        other->client->pers.max_grenades = 100;
    if (other->client->pers.max_cells < 300)
        other->client->pers.max_cells = 300;
    if (other->client->pers.max_slugs < 100)
        other->client->pers.max_slugs = 100;
    // RAFAEL
    if (other->client->pers.max_magslug < 100)
        other->client->pers.max_magslug = 100;
    //PMM
    if (other->client->pers.max_flechettes < 200)
        other->client->pers.max_flechettes = 200;
    //pmm

    item = FindItem("Bullets");
    if (item) {
        index = ITEM_INDEX(item);
        other->client->pers.inventory[index] += item->quantity;
        if (other->client->pers.inventory[index] > other->client->pers.max_bullets)
            other->client->pers.inventory[index] = other->client->pers.max_bullets;
    }

    item = FindItem("Shells");
    if (item) {
        index = ITEM_INDEX(item);
        other->client->pers.inventory[index] += item->quantity;
        if (other->client->pers.inventory[index] > other->client->pers.max_shells)
            other->client->pers.inventory[index] = other->client->pers.max_shells;
    }

    item = FindItem("Cells");
    if (item) {
        index = ITEM_INDEX(item);
        other->client->pers.inventory[index] += item->quantity;
        if (other->client->pers.inventory[index] > other->client->pers.max_cells)
            other->client->pers.inventory[index] = other->client->pers.max_cells;
    }

    item = FindItem("Grenades");
    if (item) {
        index = ITEM_INDEX(item);
        other->client->pers.inventory[index] += item->quantity;
        if (other->client->pers.inventory[index] > other->client->pers.max_grenades)
            other->client->pers.inventory[index] = other->client->pers.max_grenades;
    }

    item = FindItem("Rockets");
    if (item) {
        index = ITEM_INDEX(item);
        other->client->pers.inventory[index] += item->quantity;
        if (other->client->pers.inventory[index] > other->client->pers.max_rockets)
            other->client->pers.inventory[index] = other->client->pers.max_rockets;
    }

    item = FindItem("Slugs");
    if (item) {
        index = ITEM_INDEX(item);
        other->client->pers.inventory[index] += item->quantity;
        if (other->client->pers.inventory[index] > other->client->pers.max_slugs)
            other->client->pers.inventory[index] = other->client->pers.max_slugs;
    }

    // RAFAEL
    item = FindItem("Mag Slug");
    if (item) {
        index = ITEM_INDEX(item);
        other->client->pers.inventory[index] += item->quantity;
        if (other->client->pers.inventory[index] > other->client->pers.max_magslug)
            other->client->pers.inventory[index] = other->client->pers.max_magslug;
    }

//PMM
    item = FindItem("Flechettes");
    if (item) {
        index = ITEM_INDEX(item);
        other->client->pers.inventory[index] += item->quantity;
        if (other->client->pers.inventory[index] > other->client->pers.max_flechettes)
            other->client->pers.inventory[index] = other->client->pers.max_flechettes;
    }
//pmm
    if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->value))
        SetRespawn(ent, ent->item->quantity);

    return true;
}
// ================
// PMM
bool Pickup_Nuke(edict_t *ent, edict_t *other)
{
    int     quantity;

//  if (!deathmatch->value)
//      return;
    quantity = other->client->pers.inventory[ITEM_INDEX(ent->item)];
//  if ((skill->value == 1 && quantity >= 2) || (skill->value >= 2 && quantity >= 1))
//      return false;

    if (quantity >= 1)
        return false;

    if ((coop->value) && (ent->item->flags & IT_STAY_COOP) && (quantity > 0))
        return false;

    other->client->pers.inventory[ITEM_INDEX(ent->item)]++;

    if (deathmatch->value) {
        if (!(ent->spawnflags & DROPPED_ITEM))
            SetRespawn(ent, ent->item->quantity);
    }

    return true;
}

// ================
// PGM
void Use_IR(edict_t *ent, const gitem_t *item)
{
    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

    if (ent->client->ir_framenum > level.framenum)
        ent->client->ir_framenum += 600;
    else
        ent->client->ir_framenum = level.framenum + 600;

    gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/ir_start.wav"), 1, ATTN_NORM, 0);
}

void Use_Double(edict_t *ent, const gitem_t *item)
{
    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

    if (ent->client->double_framenum > level.framenum)
        ent->client->double_framenum += 300;
    else
        ent->client->double_framenum = level.framenum + 300;

    gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/ddamage1.wav"), 1, ATTN_NORM, 0);
}

/*
void Use_Torch (edict_t *ent, const gitem_t *item)
{
    ent->client->torch_framenum = level.framenum + 600;
}
*/

void Use_Compass(edict_t *ent, const gitem_t *item)
{
    int ang;

    ang = (int)(ent->client->v_angle[1]);
    if (ang < 0)
        ang += 360;

    gi.cprintf(ent, PRINT_HIGH, "Origin: %0.0f,%0.0f,%0.0f    Dir: %d\n", ent->s.origin[0], ent->s.origin[1],
               ent->s.origin[2], ang);
}

void Use_Nuke(edict_t *ent, const gitem_t *item)
{
    vec3_t  forward, right, start;
    float   speed;

    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

    AngleVectors(ent->client->v_angle, forward, right, NULL);

    VectorCopy(ent->s.origin, start);
    speed = 100;
    fire_nuke(ent, start, forward, speed);
}

void Use_Doppleganger(edict_t *ent, const gitem_t *item)
{
    vec3_t      forward, right;
    vec3_t      createPt, spawnPt;
    vec3_t      ang;

    VectorClear(ang);
    ang[YAW] = ent->client->v_angle[YAW];
    AngleVectors(ang, forward, right, NULL);

    VectorMA(ent->s.origin, 48, forward, createPt);

    if (!FindSpawnPoint(createPt, ent->mins, ent->maxs, spawnPt, 32))
        return;

    if (!CheckGroundSpawnPoint(spawnPt, ent->mins, ent->maxs, 64, -1))
        return;

    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

    SpawnGrow_Spawn(spawnPt, 0);
    fire_doppleganger(ent, spawnPt, forward);
}

bool Pickup_Doppleganger(edict_t *ent, edict_t *other)
{
    int     quantity;

    if (!(deathmatch->value))       // item is DM only
        return false;

    quantity = other->client->pers.inventory[ITEM_INDEX(ent->item)];
    if (quantity >= 1)      // FIXME - apply max to dopplegangers
        return false;

    other->client->pers.inventory[ITEM_INDEX(ent->item)]++;

    if (!(ent->spawnflags & DROPPED_ITEM))
        SetRespawn(ent, ent->item->quantity);

    return true;
}

bool Pickup_Sphere(edict_t *ent, edict_t *other)
{
    int     quantity;

    if (other->client && other->client->owned_sphere) {
//      gi.cprintf(other, PRINT_HIGH, "Only one sphere to a customer!\n");
        return false;
    }

    quantity = other->client->pers.inventory[ITEM_INDEX(ent->item)];
    if ((skill->value == 1 && quantity >= 2) || (skill->value >= 2 && quantity >= 1))
        return false;

    if ((coop->value) && (ent->item->flags & IT_STAY_COOP) && (quantity > 0))
        return false;

    other->client->pers.inventory[ITEM_INDEX(ent->item)]++;

    if (deathmatch->value) {
        if (!(ent->spawnflags & DROPPED_ITEM))
            SetRespawn(ent, ent->item->quantity);
        if (((int)dmflags->value & DF_INSTANT_ITEMS)) {
//PGM
            if (ent->item->use)
                ent->item->use(other, ent->item);
            else
                gi.dprintf("Powerup has no use function!\n");
//PGM
        }
    }

    return true;
}

void Use_Defender(edict_t *ent, const gitem_t *item)
{
    if (ent->client && ent->client->owned_sphere) {
        gi.cprintf(ent, PRINT_HIGH, "Only one sphere at a time!\n");
        return;
    }

    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

    Defender_Launch(ent);
}

void Use_Hunter(edict_t *ent, const gitem_t *item)
{
    if (ent->client && ent->client->owned_sphere) {
        gi.cprintf(ent, PRINT_HIGH, "Only one sphere at a time!\n");
        return;
    }

    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

    Hunter_Launch(ent);
}

void Use_Vengeance(edict_t *ent, const gitem_t *item)
{
    if (ent->client && ent->client->owned_sphere) {
        gi.cprintf(ent, PRINT_HIGH, "Only one sphere at a time!\n");
        return;
    }

    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

    Vengeance_Launch(ent);
}

// PGM
// ================

//======================================================================

static void Use_Quad(edict_t *ent, const gitem_t *item)
{
    int     timeout;

    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

    if (quad_drop_timeout_hack) {
        timeout = quad_drop_timeout_hack;
        quad_drop_timeout_hack = 0;
    } else {
        timeout = 300;
    }

    if (ent->client->quad_framenum > level.framenum)
        ent->client->quad_framenum += timeout;
    else
        ent->client->quad_framenum = level.framenum + timeout;

    gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage.wav"), 1, ATTN_NORM, 0);
}

// =====================================================================

// RAFAEL
static void Use_QuadFire(edict_t *ent, const gitem_t *item)
{
    int     timeout;

    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

    if (quad_fire_drop_timeout_hack) {
        timeout = quad_fire_drop_timeout_hack;
        quad_fire_drop_timeout_hack = 0;
    } else {
        timeout = 300;
    }

    if (ent->client->quadfire_framenum > level.framenum)
        ent->client->quadfire_framenum += timeout;
    else
        ent->client->quadfire_framenum = level.framenum + timeout;

    gi.sound(ent, CHAN_ITEM, gi.soundindex("items/quadfire1.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

static void Use_Breather(edict_t *ent, const gitem_t *item)
{
    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

    if (ent->client->breather_framenum > level.framenum)
        ent->client->breather_framenum += 300;
    else
        ent->client->breather_framenum = level.framenum + 300;

//  gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

static void Use_Envirosuit(edict_t *ent, const gitem_t *item)
{
    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

    if (ent->client->enviro_framenum > level.framenum)
        ent->client->enviro_framenum += 300;
    else
        ent->client->enviro_framenum = level.framenum + 300;

//  gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

static void Use_Invulnerability(edict_t *ent, const gitem_t *item)
{
    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

    if (ent->client->invincible_framenum > level.framenum)
        ent->client->invincible_framenum += 300;
    else
        ent->client->invincible_framenum = level.framenum + 300;

    gi.sound(ent, CHAN_ITEM, gi.soundindex("items/protect.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

static void Use_Silencer(edict_t *ent, const gitem_t *item)
{
    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);
    ent->client->silencer_shots += 30;

//  gi.sound(ent, CHAN_ITEM, gi.soundindex("items/damage.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

static bool Pickup_Key(edict_t *ent, edict_t *other)
{
    if (coop->value) {
        if (strcmp(ent->classname, "key_power_cube") == 0) {
            if (other->client->pers.power_cubes & ((ent->spawnflags & 0x0000ff00) >> 8))
                return false;
            other->client->pers.inventory[ITEM_INDEX(ent->item)]++;
            other->client->pers.power_cubes |= ((ent->spawnflags & 0x0000ff00) >> 8);
        } else {
            if (other->client->pers.inventory[ITEM_INDEX(ent->item)])
                return false;
            other->client->pers.inventory[ITEM_INDEX(ent->item)] = 1;
        }
        return true;
    }
    other->client->pers.inventory[ITEM_INDEX(ent->item)]++;
    return true;
}

//======================================================================

bool Add_Ammo(edict_t *ent, const gitem_t *item, int count)
{
    int         index;
    int         max;

    if (!ent->client)
        return false;

    if (item->tag == AMMO_BULLETS)
        max = ent->client->pers.max_bullets;
    else if (item->tag == AMMO_SHELLS)
        max = ent->client->pers.max_shells;
    else if (item->tag == AMMO_ROCKETS)
        max = ent->client->pers.max_rockets;
    else if (item->tag == AMMO_GRENADES)
        max = ent->client->pers.max_grenades;
    else if (item->tag == AMMO_CELLS)
        max = ent->client->pers.max_cells;
    else if (item->tag == AMMO_SLUGS)
        max = ent->client->pers.max_slugs;
    // RAFAEL
    else if (item->tag == AMMO_MAGSLUG)
        max = ent->client->pers.max_magslug;
    // RAFAEL
    else if (item->tag == AMMO_TRAP)
        max = ent->client->pers.max_trap;
// ROGUE
//  else if (item->tag == AMMO_MINES)
//      max = ent->client->pers.max_mines;
    else if (item->tag == AMMO_FLECHETTES)
        max = ent->client->pers.max_flechettes;
    else if (item->tag == AMMO_PROX)
        max = ent->client->pers.max_prox;
    else if (item->tag == AMMO_TESLA)
        max = ent->client->pers.max_tesla;
// ROGUE
    else {
        gi.dprintf("undefined ammo type\n");
        return false;
    }

    index = ITEM_INDEX(item);

    if (ent->client->pers.inventory[index] == max)
        return false;

    ent->client->pers.inventory[index] += count;

    if (ent->client->pers.inventory[index] > max)
        ent->client->pers.inventory[index] = max;

    return true;
}

static bool Pickup_Ammo(edict_t *ent, edict_t *other)
{
    int         oldcount;
    int         count;
    bool        weapon;

    weapon = (ent->item->flags & IT_WEAPON);
    if ((weapon) && ((int)dmflags->value & DF_INFINITE_AMMO))
        count = 1000;
    else if (ent->count)
        count = ent->count;
    else
        count = ent->item->quantity;

    oldcount = other->client->pers.inventory[ITEM_INDEX(ent->item)];

    if (!Add_Ammo(other, ent->item, count))
        return false;

    if (weapon && !oldcount) {
        // don't switch to tesla
        if (other->client->pers.weapon != ent->item
            && (!deathmatch->value || other->client->pers.weapon == FindItem("blaster"))
            && (strcmp(ent->classname, "ammo_tesla")))

            other->client->newweapon = ent->item;
    }

    if (!(ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_ITEM)) && (deathmatch->value))
        SetRespawn(ent, 30);
    return true;
}

static void Drop_Ammo(edict_t *ent, const gitem_t *item)
{
    edict_t *dropped;
    int     index;

    index = ITEM_INDEX(item);
    dropped = Drop_Item(ent, item);
    if (ent->client->pers.inventory[index] >= item->quantity)
        dropped->count = item->quantity;
    else
        dropped->count = ent->client->pers.inventory[index];

    if (ent->client->pers.weapon &&
        ent->client->pers.weapon->tag == AMMO_GRENADES &&
        item->tag == AMMO_GRENADES &&
        ent->client->pers.inventory[index] - dropped->count <= 0) {
        gi.cprintf(ent, PRINT_HIGH, "Can't drop current weapon\n");
        G_FreeEdict(dropped);
        return;
    }

    ent->client->pers.inventory[index] -= dropped->count;
    ValidateSelectedItem(ent);
}

//======================================================================

void MegaHealth_think(edict_t *self)
{
    // CTF regeneration tech holds the overhealth instead of bleeding it off.
    if (self->owner->health > self->owner->max_health
        && !CTFHasRegeneration(self->owner)) {
        self->nextthink = level.framenum + 1 * BASE_FRAMERATE;
        self->owner->health -= 1;
        return;
    }

    if (!(self->spawnflags & DROPPED_ITEM) && (deathmatch->value))
        SetRespawn(self, 20);
    else
        G_FreeEdict(self);
}

bool Pickup_Health(edict_t *ent, edict_t *other)
{
    if (!(ent->style & HEALTH_IGNORE_MAX))
        if (other->health >= other->max_health)
            return false;

    // CTF caps stacked mega-health at 250 rather than letting it run to 999.
    if (G_Ruleset() == RULESET_CTF && other->health >= 250 && ent->count > 25)
        return false;

    other->health += ent->count;

    if (G_Ruleset() == RULESET_CTF && other->health > 250 && ent->count > 25)
        other->health = 250;

    // PMM - health sound fix
    /*
    if (ent->count == 2)
        ent->item->pickup_sound = "items/s_health.wav";
    else if (ent->count == 10)
        ent->item->pickup_sound = "items/n_health.wav";
    else if (ent->count == 25)
        ent->item->pickup_sound = "items/l_health.wav";
    else // (ent->count == 100)
        ent->item->pickup_sound = "items/m_health.wav";
    */

    if (!(ent->style & HEALTH_IGNORE_MAX)) {
        if (other->health > other->max_health)
            other->health = other->max_health;
    }

    if ((ent->style & HEALTH_TIMED) && !CTFHasRegeneration(other)) {
        ent->think = MegaHealth_think;
        ent->nextthink = level.framenum + 5 * BASE_FRAMERATE;
        ent->owner = other;
        ent->flags |= FL_RESPAWN;
        ent->svflags |= SVF_NOCLIENT;
        ent->solid = SOLID_NOT;
    } else {
        if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->value))
            SetRespawn(ent, 30);
    }

    return true;
}

//======================================================================

int ArmorIndex(edict_t *ent)
{
    if (!ent->client)
        return 0;

    if (ent->client->pers.inventory[jacket_armor_index] > 0)
        return jacket_armor_index;

    if (ent->client->pers.inventory[combat_armor_index] > 0)
        return combat_armor_index;

    if (ent->client->pers.inventory[body_armor_index] > 0)
        return body_armor_index;

    return 0;
}

bool Pickup_Armor(edict_t *ent, edict_t *other)
{
    int             old_armor_index;
    const gitem_armor_t *oldinfo;
    const gitem_armor_t *newinfo;
    int             newcount;
    float           salvage;
    int             salvagecount;

    // get info on new armor
    newinfo = (const gitem_armor_t *)ent->item->info;

    old_armor_index = ArmorIndex(other);

    // handle armor shards specially
    if (ent->item->tag == ARMOR_SHARD) {
        if (!old_armor_index)
            other->client->pers.inventory[jacket_armor_index] = 2;
        else
            other->client->pers.inventory[old_armor_index] += 2;
    }

    // if player has no armor, just use it
    else if (!old_armor_index) {
        other->client->pers.inventory[ITEM_INDEX(ent->item)] = newinfo->base_count;
    }

    // use the better armor
    else {
        // get info on old armor
        if (old_armor_index == jacket_armor_index)
            oldinfo = &jacketarmor_info;
        else if (old_armor_index == combat_armor_index)
            oldinfo = &combatarmor_info;
        else // (old_armor_index == body_armor_index)
            oldinfo = &bodyarmor_info;

        if (newinfo->normal_protection > oldinfo->normal_protection) {
            // calc new armor values
            salvage = oldinfo->normal_protection / newinfo->normal_protection;
            salvagecount = salvage * other->client->pers.inventory[old_armor_index];
            newcount = newinfo->base_count + salvagecount;
            if (newcount > newinfo->max_count)
                newcount = newinfo->max_count;

            // zero count of old armor so it goes away
            other->client->pers.inventory[old_armor_index] = 0;

            // change armor to new item with computed value
            other->client->pers.inventory[ITEM_INDEX(ent->item)] = newcount;
        } else {
            // calc new armor values
            salvage = newinfo->normal_protection / oldinfo->normal_protection;
            salvagecount = salvage * newinfo->base_count;
            newcount = other->client->pers.inventory[old_armor_index] + salvagecount;
            if (newcount > oldinfo->max_count)
                newcount = oldinfo->max_count;

            // if we're already maxed out then we don't need the new armor
            if (other->client->pers.inventory[old_armor_index] >= newcount)
                return false;

            // update current armor value
            other->client->pers.inventory[old_armor_index] = newcount;
        }
    }

    if (!(ent->spawnflags & DROPPED_ITEM) && (deathmatch->value))
        SetRespawn(ent, 20);

    return true;
}

//======================================================================

int PowerArmorType(edict_t *ent)
{
    if (!ent->client)
        return POWER_ARMOR_NONE;

    if (!(ent->flags & FL_POWER_ARMOR))
        return POWER_ARMOR_NONE;

    if (ent->client->pers.inventory[power_shield_index] > 0)
        return POWER_ARMOR_SHIELD;

    if (ent->client->pers.inventory[power_screen_index] > 0)
        return POWER_ARMOR_SCREEN;

    return POWER_ARMOR_NONE;
}

static void Use_PowerArmor(edict_t *ent, const gitem_t *item)
{
    int     index;

    if (ent->flags & FL_POWER_ARMOR) {
        ent->flags &= ~FL_POWER_ARMOR;
        gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/power2.wav"), 1, ATTN_NORM, 0);
    } else {
        index = ITEM_INDEX(FindItem("cells"));
        if (!ent->client->pers.inventory[index]) {
            gi.cprintf(ent, PRINT_HIGH, "No cells for power armor.\n");
            return;
        }
        ent->flags |= FL_POWER_ARMOR;
        gi.sound(ent, CHAN_AUTO, gi.soundindex("misc/power1.wav"), 1, ATTN_NORM, 0);
    }
}

bool Pickup_PowerArmor(edict_t *ent, edict_t *other)
{
    int     quantity;

    quantity = other->client->pers.inventory[ITEM_INDEX(ent->item)];

    other->client->pers.inventory[ITEM_INDEX(ent->item)]++;

    if (deathmatch->value) {
        if (!(ent->spawnflags & DROPPED_ITEM))
            SetRespawn(ent, ent->item->quantity);
        // auto-use for DM only if we didn't already have one
        if (!quantity)
            ent->item->use(other, ent->item);
    }

    return true;
}

static void Drop_PowerArmor(edict_t *ent, const gitem_t *item)
{
    if ((ent->flags & FL_POWER_ARMOR) && (ent->client->pers.inventory[ITEM_INDEX(item)] == 1))
        Use_PowerArmor(ent, item);
    Drop_General(ent, item);
}

//======================================================================

/*
===============
Touch_Item
===============
*/
void Touch_Item(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    bool    taken;

    if (!other->client)
        return;
    if (other->health < 1)
        return;     // dead people can't pickup
    if (!ent->item->pickup)
        return;     // not a grabbable item?

    // CTF match setup: nothing is pickable while captains pick teams.  False in
    // every other ruleset.
    if (CTFMatchSetup())
        return;

    taken = ent->item->pickup(ent, other);

    if (taken) {
        // flash the screen
        other->client->bonus_alpha = 0.25f;

        // show icon and name on status bar
        other->client->ps.stats[STAT_PICKUP_ICON] = gi.imageindex(ent->item->icon);
        other->client->ps.stats[STAT_PICKUP_STRING] = game.csr.items + ITEM_INDEX(ent->item);
        other->client->pickup_msg_framenum = level.framenum + 3.0f * BASE_FRAMERATE;

        // change selected item
        if (ent->item->use)
            other->client->pers.selected_item = other->client->ps.stats[STAT_SELECTED_ITEM] = ITEM_INDEX(ent->item);

        // PMM - health sound fix
        if (ent->item->pickup == Pickup_Health) {
            if (ent->count == 2)
                gi.sound(other, CHAN_ITEM, gi.soundindex("items/s_health.wav"), 1, ATTN_NORM, 0);
            else if (ent->count == 10)
                gi.sound(other, CHAN_ITEM, gi.soundindex("items/n_health.wav"), 1, ATTN_NORM, 0);
            else if (ent->count == 25)
                gi.sound(other, CHAN_ITEM, gi.soundindex("items/l_health.wav"), 1, ATTN_NORM, 0);
            else // (ent->count == 100)
                gi.sound(other, CHAN_ITEM, gi.soundindex("items/m_health.wav"), 1, ATTN_NORM, 0);
        } else if (ent->item->pickup_sound) { // PGM - paranoia
            //
            gi.sound(other, CHAN_ITEM, gi.soundindex(ent->item->pickup_sound), 1, ATTN_NORM, 0);
        }
    }

    if (!(ent->spawnflags & ITEM_TARGETS_USED)) {
        G_UseTargets(ent, other);
        ent->spawnflags |= ITEM_TARGETS_USED;
    }

    if (!taken)
        return;

    if (!((coop->value) && (ent->item->flags & IT_STAY_COOP)) || (ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_ITEM))) {
        if (ent->flags & FL_RESPAWN)
            ent->flags &= ~FL_RESPAWN;
        else
            G_FreeEdict(ent);
    }
}

//======================================================================

void drop_temp_touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (other == ent->owner)
        return;

    Touch_Item(ent, other, plane, surf);
}

void drop_make_touchable(edict_t *ent)
{
    ent->touch = Touch_Item;
    if (deathmatch->value) {
        ent->nextthink = level.framenum + 29 * BASE_FRAMERATE;
        ent->think = G_FreeEdict;
    }
}

edict_t *Drop_Item(edict_t *ent, const gitem_t *item)
{
    edict_t *dropped;
    vec3_t  forward, right;
    vec3_t  offset;

    dropped = G_Spawn();

    dropped->classname = item->classname;
    dropped->item = item;
    dropped->spawnflags = DROPPED_ITEM;
    dropped->s.effects = item->world_model_flags;
    dropped->s.renderfx = RF_GLOW | RF_IR_VISIBLE;      // PGM
    VectorSet(dropped->mins, -15, -15, -15);
    VectorSet(dropped->maxs, 15, 15, 15);
    gi.setmodel(dropped, dropped->item->world_model);
    dropped->solid = SOLID_TRIGGER;
    dropped->movetype = MOVETYPE_TOSS;
    dropped->touch = drop_temp_touch;
    dropped->owner = ent;

    if (ent->client) {
        trace_t trace;

        AngleVectors(ent->client->v_angle, forward, right, NULL);
        VectorSet(offset, 24, 0, -16);
        G_ProjectSource(ent->s.origin, offset, forward, right, dropped->s.origin);
        trace = gi.trace(ent->s.origin, dropped->mins, dropped->maxs,
                         dropped->s.origin, ent, CONTENTS_SOLID);
        VectorCopy(trace.endpos, dropped->s.origin);
    } else {
        AngleVectors(ent->s.angles, forward, right, NULL);
        VectorCopy(ent->s.origin, dropped->s.origin);
    }

    VectorScale(forward, 100, dropped->velocity);
    dropped->velocity[2] = 300;

    dropped->think = drop_make_touchable;
    dropped->nextthink = level.framenum + 1 * BASE_FRAMERATE;

    gi.linkentity(dropped);

    return dropped;
}

void Use_Item(edict_t *ent, edict_t *other, edict_t *activator)
{
    ent->svflags &= ~SVF_NOCLIENT;
    ent->use = NULL;

    if (ent->spawnflags & ITEM_NO_TOUCH) {
        ent->solid = SOLID_BBOX;
        ent->touch = NULL;
    } else {
        ent->solid = SOLID_TRIGGER;
        ent->touch = Touch_Item;
    }

    gi.linkentity(ent);
}

//======================================================================

/*
================
droptofloor
================
*/
void droptofloor(edict_t *ent)
{
    trace_t     tr;
    vec3_t      dest;

    VectorSet(ent->mins, -15, -15, -15);
    VectorSet(ent->maxs, 15, 15, 15);

    if (ent->model)
        gi.setmodel(ent, ent->model);
    else if (ent->item->world_model)    // PGM we shouldn't need this check, but paranoia...
        gi.setmodel(ent, ent->item->world_model);
    ent->solid = SOLID_TRIGGER;
    ent->movetype = MOVETYPE_TOSS;
    ent->touch = Touch_Item;

    VectorCopy(ent->s.origin, dest);
    dest[2] -= 128;

    tr = gi.trace(ent->s.origin, ent->mins, ent->maxs, dest, ent, MASK_SOLID);
    if (tr.startsolid) {
        // RAFAEL
        if (strcmp(ent->classname, "foodcube") == 0) {
            VectorCopy(ent->s.origin, tr.endpos);
            ent->velocity[2] = 0;
        } else {
            gi.dprintf("droptofloor: %s startsolid at %s\n", ent->classname, vtos(ent->s.origin));
            G_FreeEdict(ent);
            return;
        }
    }

    VectorCopy(tr.endpos, ent->s.origin);

    if (ent->team) {
        ent->flags &= ~FL_TEAMSLAVE;
        ent->chain = ent->teamchain;
        ent->teamchain = NULL;

        ent->svflags |= SVF_NOCLIENT;
        ent->solid = SOLID_NOT;
        if (ent == ent->teammaster) {
            ent->nextthink = level.framenum + 1;
            ent->think = DoRespawn;
        }
    }

    if (ent->spawnflags & ITEM_NO_TOUCH) {
        ent->solid = SOLID_BBOX;
        ent->touch = NULL;
        ent->s.effects &= ~EF_ROTATE;
        ent->s.renderfx &= ~RF_GLOW;
    }

    if (ent->spawnflags & ITEM_TRIGGER_SPAWN) {
        ent->svflags |= SVF_NOCLIENT;
        ent->solid = SOLID_NOT;
        ent->use = Use_Item;
    }

    gi.linkentity(ent);
}

/*
===============
PrecacheItem

Precaches all data needed for a given item.
This will be called for each item spawned in a level,
and for each item in each client's inventory.
===============
*/
void PrecacheItem(const gitem_t *it)
{
    const char *const *s;
    const char *data;
    size_t len;

    if (!it)
        return;

    if (it->pickup_sound)
        gi.soundindex(it->pickup_sound);
    if (it->world_model)
        gi.modelindex(it->world_model);
    if (it->view_model)
        gi.modelindex(it->view_model);
    if (it->icon)
        gi.imageindex(it->icon);

    // parse everything for its ammo
    if (it->ammo && it->ammo[0]) {
        const gitem_t *ammo = FindItem(it->ammo);
        if (ammo != it)
            PrecacheItem(ammo);
    }

    // parse NULL terminated precache list for other items
    s = it->precaches;
    if (!s)
        return;

    while (*s) {
        data = *s++;
        len = strlen(data);
        if (len >= MAX_QPATH || len < 5)
            gi.error("PrecacheItem: %s has bad precache string", it->classname);

        // determine type based on extension
        if (!strcmp(data + len - 3, "md2"))
            gi.modelindex(data);
        else if (!strcmp(data + len - 3, "sp2"))
            gi.modelindex(data);
        else if (!strcmp(data + len - 3, "wav"))
            gi.soundindex(data);
        else if (!strcmp(data + len - 3, "pcx"))
            gi.imageindex(data);
    }
}

//=================
// Item_TriggeredSpawn - create the item marked for spawn creation
//=================
void Item_TriggeredSpawn(edict_t *self, edict_t *other, edict_t *activator)
{
//  self->nextthink = level.framenum + 2;    // items start after other solids
//  self->think = droptofloor;
    self->svflags &= ~SVF_NOCLIENT;
    self->use = NULL;
    if (strcmp(self->classname, "key_power_cube"))  // leave them be on key_power_cube..
        self->spawnflags = 0;
    droptofloor(self);
}

//=================
// SetTriggeredSpawn - set up an item to spawn in later.
//=================
void SetTriggeredSpawn(edict_t *ent)
{
    // don't do anything on key_power_cubes.
    if (!strcmp(ent->classname, "key_power_cube"))
        return;

    ent->think = NULL;
    ent->nextthink = 0;
    ent->use = Item_TriggeredSpawn;
    ent->svflags |= SVF_NOCLIENT;
    ent->solid = SOLID_NOT;
}

/*
============
SpawnItem

Sets the clipping size and plants the object on the floor.

Items can't be immediately dropped to floor, because they might
be on an entity that hasn't spawned yet.
============
*/
void SpawnItem(edict_t *ent, const gitem_t *item)
{
    if ((!strcmp(ent->classname, "ammo_disruptor")) || (!strcmp(ent->classname, "weapon_disintegrator"))) {
        G_FreeEdict(ent);
        return;
    }

// PGM - since the item may be freed by the following rules, go ahead
//       and move the precache until AFTER the following rules have been checked.
//       keep an eye on this.
//  PrecacheItem (item);

    if (ent->spawnflags > 1) {      // PGM
        if (strcmp(ent->classname, "key_power_cube") != 0) {
            ent->spawnflags = 0;
            gi.dprintf("%s at %s has invalid spawnflags set\n", ent->classname, vtos(ent->s.origin));
        }
    }

    // RA2 removes every item from the map: an arena hands out a fixed loadout
    // and anything lying on the floor is somebody's advantage.  Unconditional
    // in the donor -- there is no `noitems` cvar in RA2 v2.25, that is a
    // uGladQ2 addition (R-RA-4) -- so the gate is the ruleset, and baseq2's
    // dmflags filter below is what every other ruleset still needs.
    if (G_Ruleset() == RULESET_ARENA && item->pickup) {
        G_FreeEdict(ent);
        return;
    }

    // some items will be prevented in deathmatch
    if (deathmatch->value) {
        if ((int)dmflags->value & DF_NO_ARMOR) {
            if (item->pickup == Pickup_Armor || item->pickup == Pickup_PowerArmor) {
                G_FreeEdict(ent);
                return;
            }
        }
        if ((int)dmflags->value & DF_NO_ITEMS) {
            if (item->pickup == Pickup_Powerup) {
                G_FreeEdict(ent);
                return;
            }
            //=====
            //ROGUE
            if (item->pickup  == Pickup_Sphere) {
                G_FreeEdict(ent);
                return;
            }
            if (item->pickup == Pickup_Doppleganger) {
                G_FreeEdict(ent);
                return;
            }
            //ROGUE
            //=====
        }
        if ((int)dmflags->value & DF_NO_HEALTH) {
            if (item->pickup == Pickup_Health || item->pickup == Pickup_Adrenaline || item->pickup == Pickup_AncientHead) {
                G_FreeEdict(ent);
                return;
            }
        }
        if ((int)dmflags->value & DF_INFINITE_AMMO) {
            if ((item->flags == IT_AMMO) || (strcmp(ent->classname, "weapon_bfg") == 0)) {
                G_FreeEdict(ent);
                return;
            }
        }

//==========
//ROGUE
        if ((int)dmflags->value & DF_NO_MINES) {
            if (!strcmp(ent->classname, "ammo_prox") ||
                !strcmp(ent->classname, "ammo_tesla")) {
                G_FreeEdict(ent);
                return;
            }
        }
        if ((int)dmflags->value & DF_NO_NUKES) {
            if (!strcmp(ent->classname, "ammo_nuke")) {
                G_FreeEdict(ent);
                return;
            }
        }
        if ((int)dmflags->value & DF_NO_SPHERES) {
            if (item->pickup  == Pickup_Sphere) {
                G_FreeEdict(ent);
                return;
            }
        }
//ROGUE
//==========

    }
//==========
//ROGUE
// DM only items
    if (!deathmatch->value) {
        if (item->pickup == Pickup_Doppleganger || item->pickup == Pickup_Nuke) {
            G_FreeEdict(ent);
            return;
        }
        if ((item->use == Use_Vengeance) || (item->use == Use_Hunter)) {
            G_FreeEdict(ent);
            return;
        }
    }
//ROGUE
//==========

//PGM
    PrecacheItem(item);
//PGM

    if (coop->value && (strcmp(ent->classname, "key_power_cube") == 0)) {
        ent->spawnflags |= (1 << (8 + level.power_cubes));
        level.power_cubes++;
    }

    // CTF's two flags are ordinary itemlist entries (R-CORE-2's union), so a
    // map that places them is loadable in every ruleset; only ctf makes them
    // real.  Elsewhere they are removed rather than left as scenery.
    if (strcmp(ent->classname, "item_flag_team1") == 0 ||
        strcmp(ent->classname, "item_flag_team2") == 0) {
        if (G_Ruleset() != RULESET_CTF) {
            G_FreeEdict(ent);
            return;
        }
    }

    ent->item = item;
    ent->nextthink = level.framenum + 2;    // items start after other solids
    ent->think = droptofloor;
    ent->s.effects = item->world_model_flags;
    ent->s.renderfx = RF_GLOW;
    if (ent->model)
        gi.modelindex(ent->model);

    // ...and the flags are server-animated and carry their own setup, which
    // replaces droptofloor.
    if (strcmp(ent->classname, "item_flag_team1") == 0 ||
        strcmp(ent->classname, "item_flag_team2") == 0)
        ent->think = CTFFlagSetup;

    if (ent->spawnflags & 1)
        SetTriggeredSpawn(ent);
}

//======================================================================

const gitem_t itemlist[] = {
    {
        NULL
    },  // leave index 0 alone

    //
    // ARMOR
    //

    /*QUAKED item_armor_body (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "item_armor_body",
        .pickup             = Pickup_Armor,
        .pickup_sound       = "misc/ar1_pkup.wav",
        .world_model        = "models/items/armor/body/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "i_bodyarmor",
        .pickup_name        = "Body Armor",
        .count_width        = 3,
        .flags              = IT_ARMOR,
        .info               = &bodyarmor_info,
        .tag                = ARMOR_BODY,
    },

    /*QUAKED item_armor_combat (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "item_armor_combat",
        .pickup             = Pickup_Armor,
        .pickup_sound       = "misc/ar1_pkup.wav",
        .world_model        = "models/items/armor/combat/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "i_combatarmor",
        .pickup_name        = "Combat Armor",
        .count_width        = 3,
        .flags              = IT_ARMOR,
        .info               = &combatarmor_info,
        .tag                = ARMOR_COMBAT,
    },

    /*QUAKED item_armor_jacket (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "item_armor_jacket",
        .pickup             = Pickup_Armor,
        .pickup_sound       = "misc/ar1_pkup.wav",
        .world_model        = "models/items/armor/jacket/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "i_jacketarmor",
        .pickup_name        = "Jacket Armor",
        .count_width        = 3,
        .flags              = IT_ARMOR,
        .info               = &jacketarmor_info,
        .tag                = ARMOR_JACKET,
    },

    /*QUAKED item_armor_shard (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "item_armor_shard",
        .pickup             = Pickup_Armor,
        .pickup_sound       = "misc/ar2_pkup.wav",
        .world_model        = "models/items/armor/shard/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "i_jacketarmor",
        .pickup_name        = "Armor Shard",
        .count_width        = 3,
        .flags              = IT_ARMOR,
        .tag                = ARMOR_SHARD,
    },

    /*QUAKED item_power_screen (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "item_power_screen",
        .pickup             = Pickup_PowerArmor,
        .use                = Use_PowerArmor,
        .drop               = Drop_PowerArmor,
        .pickup_sound       = "misc/ar3_pkup.wav",
        .world_model        = "models/items/armor/screen/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "i_powerscreen",
        .pickup_name        = "Power Screen",
        .quantity           = 60,
        .flags              = IT_ARMOR,
        .precaches          = (const char *const[]) {
            "misc/power1.wav",
            "misc/power2.wav",
            NULL
        },
    },

    /*QUAKED item_power_shield (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "item_power_shield",
        .pickup             = Pickup_PowerArmor,
        .use                = Use_PowerArmor,
        .drop               = Drop_PowerArmor,
        .pickup_sound       = "misc/ar3_pkup.wav",
        .world_model        = "models/items/armor/shield/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "i_powershield",
        .pickup_name        = "Power Shield",
        .quantity           = 60,
        .flags              = IT_ARMOR,
        .precaches          = (const char *const[]) {
            "misc/power1.wav",
            "misc/power2.wav",
            NULL
        },
    },

    //
    // WEAPONS
    //

    /* weapon_grapple (.3 .3 1) (-16 -16 -16) (16 16 16)
    always owned, never in the world -- CTF
    */
    {
        .classname          = "weapon_grapple",
        .use                = Use_Weapon,
        .weaponthink        = CTFWeapon_Grapple,
        .pickup_sound       = "misc/w_pkup.wav",
        .view_model         = "models/weapons/grapple/tris.md2",
        .icon               = "w_grapple",
        .pickup_name        = "Grapple",
        .flags              = IT_WEAPON,
        .weapmodel          = WEAP_GRAPPLE,
        .precaches          = (const char *const[]) {
            "weapons/grapple/grfire.wav",
            "weapons/grapple/grpull.wav",
            "weapons/grapple/grhang.wav",
            "weapons/grapple/grreset.wav",
            "weapons/grapple/grhit.wav",
            // CTFGrapplePull() plays this and Threewave never precached it;
            // RA2's copy of the row does, and sec 7 rule 7 makes the fix ours.
            "weapons/grapple/grhurt.wav",
            NULL
        },
    },

    // RA2's own weapon_grapple row was here.  Dropped: the itemlist already
    // has Threewave's for the same classname and there is one grapple
    // (sec 7 rule 6, R-50).  Its one real difference -- the grhurt.wav
    // precache -- is on that entry instead.

    /* weapon_blaster (.3 .3 1) (-16 -16 -16) (16 16 16)
    always owned, never in the world
    */
    {
        .classname          = "weapon_blaster",
        .use                = Use_Weapon,
        .weaponthink        = Weapon_Blaster,
        .pickup_sound       = "misc/w_pkup.wav",
        .view_model         = "models/weapons/v_blast/tris.md2",
        .icon               = "w_blaster",
        .pickup_name        = "Blaster",
        .flags              = IT_WEAPON | IT_STAY_COOP,
        .weapmodel          = WEAP_BLASTER,
        .precaches          = (const char *const[]) {
            "models/objects/laser/tris.md2",
            "weapons/blastf1a.wav",
            "misc/lasfly.wav",
            NULL
        },
    },

    /*QUAKED weapon_shotgun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "weapon_shotgun",
        .pickup             = Pickup_Weapon,
        .use                = Use_Weapon,
        .drop               = Drop_Weapon,
        .weaponthink        = Weapon_Shotgun,
        .pickup_sound       = "misc/w_pkup.wav",
        .world_model        = "models/weapons/g_shotg/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .view_model         = "models/weapons/v_shotg/tris.md2",
        .icon               = "w_shotgun",
        .pickup_name        = "Shotgun",
        .quantity           = 1,
        .ammo               = "Shells",
        .flags              = IT_WEAPON | IT_STAY_COOP,
        .weapmodel          = WEAP_SHOTGUN,
        .precaches          = (const char *const[]) {
            "weapons/shotgf1b.wav",
            "weapons/shotgr1b.wav",
            NULL
        },
    },

    /*QUAKED weapon_supershotgun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "weapon_supershotgun",
        .pickup             = Pickup_Weapon,
        .use                = Use_Weapon,
        .drop               = Drop_Weapon,
        .weaponthink        = Weapon_SuperShotgun,
        .pickup_sound       = "misc/w_pkup.wav",
        .world_model        = "models/weapons/g_shotg2/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .view_model         = "models/weapons/v_shotg2/tris.md2",
        .icon               = "w_sshotgun",
        .pickup_name        = "Super Shotgun",
        .quantity           = 2,
        .ammo               = "Shells",
        .flags              = IT_WEAPON | IT_STAY_COOP,
        .weapmodel          = WEAP_SUPERSHOTGUN,
        .precaches          = (const char *const[]) {
            "weapons/sshotf1b.wav",
            NULL
        },
    },

    /*QUAKED weapon_machinegun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "weapon_machinegun",
        .pickup             = Pickup_Weapon,
        .use                = Use_Weapon,
        .drop               = Drop_Weapon,
        .weaponthink        = Weapon_Machinegun,
        .pickup_sound       = "misc/w_pkup.wav",
        .world_model        = "models/weapons/g_machn/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .view_model         = "models/weapons/v_machn/tris.md2",
        .icon               = "w_machinegun",
        .pickup_name        = "Machinegun",
        .quantity           = 1,
        .ammo               = "Bullets",
        .flags              = IT_WEAPON | IT_STAY_COOP,
        .weapmodel          = WEAP_MACHINEGUN,
        .precaches          = (const char *const[]) {
            "weapons/machgf1b.wav",
            "weapons/machgf2b.wav",
            "weapons/machgf3b.wav",
            "weapons/machgf4b.wav",
            "weapons/machgf5b.wav",
            NULL
        },
    },

    /*QUAKED weapon_chaingun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "weapon_chaingun",
        .pickup             = Pickup_Weapon,
        .use                = Use_Weapon,
        .drop               = Drop_Weapon,
        .weaponthink        = Weapon_Chaingun,
        .pickup_sound       = "misc/w_pkup.wav",
        .world_model        = "models/weapons/g_chain/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .view_model         = "models/weapons/v_chain/tris.md2",
        .icon               = "w_chaingun",
        .pickup_name        = "Chaingun",
        .quantity           = 1,
        .ammo               = "Bullets",
        .flags              = IT_WEAPON | IT_STAY_COOP,
        .weapmodel          = WEAP_CHAINGUN,
        .precaches          = (const char *const[]) {
            "weapons/machgf1b.wav",
            "weapons/machgf2b.wav",
            "weapons/machgf3b.wav",
            "weapons/machgf4b.wav",
            "weapons/machgf5b.wav",
            "weapons/chngnu1a.wav",
            "weapons/chngnl1a.wav",
            "weapons/chngnd1a.wav",
            NULL
        },
    },

    // ROGUE
    /*QUAKED weapon_etf_rifle (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "weapon_etf_rifle",
        .pickup             = Pickup_Weapon,
        .use                = Use_Weapon,
        .drop               = Drop_Weapon,
        .weaponthink        = Weapon_ETF_Rifle,
        .pickup_sound       = "misc/w_pkup.wav",
        .world_model        = "models/weapons/g_etf_rifle/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .view_model         = "models/weapons/v_etf_rifle/tris.md2",
        .icon               = "w_etf_rifle",
        .pickup_name        = "ETF Rifle",
        .quantity           = 1,
        .ammo               = "Flechettes",
        .flags              = IT_WEAPON,
        .weapmodel          = WEAP_ETFRIFLE,
        .precaches          = (const char *const[]) {
            "weapons/nail1.wav",
            "models/proj/flechette/tris.md2",
            NULL
        },
    },

    // rogue
    /*QUAKED ammo_grenades (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "ammo_grenades",
        .pickup             = Pickup_Ammo,
        .use                = Use_Weapon,
        .drop               = Drop_Ammo,
        .weaponthink        = Weapon_Grenade,
        .pickup_sound       = "misc/am_pkup.wav",
        .world_model        = "models/items/ammo/grenades/medium/tris.md2",
        .view_model         = "models/weapons/v_handgr/tris.md2",
        .icon               = "a_grenades",
        .pickup_name        = "Grenades",
        .count_width        = 3,
        .quantity           = 5,
        .ammo               = "grenades",
        .flags              = IT_AMMO | IT_WEAPON,
        .weapmodel          = WEAP_GRENADES,
        .tag                = AMMO_GRENADES,
        .precaches          = (const char *const[]) {
            "models/objects/grenade2/tris.md2",
            "weapons/hgrent1a.wav",
            "weapons/hgrena1b.wav",
            "weapons/hgrenc1b.wav",
            "weapons/hgrenb1a.wav",
            "weapons/hgrenb2a.wav",
            NULL
        },
    },

    /*QUAKED ammo_trap (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        .classname          = "ammo_trap",
        .pickup             = Pickup_Ammo,
        .use                = Use_Weapon,
        .drop               = Drop_Ammo,
        .weaponthink        = Weapon_Trap,
        .pickup_sound       = "misc/am_pkup.wav",
        .world_model        = "models/weapons/g_trap/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .view_model         = "models/weapons/v_trap/tris.md2",
        .icon               = "a_trap",
        .pickup_name        = "Trap",
        .count_width        = 3,
        .quantity           = 1,
        .ammo               = "trap",
        .flags              = IT_AMMO | IT_WEAPON,
        .tag                = AMMO_TRAP,
        .precaches          = (const char *const []) {
            "weapons/trapcock.wav",
            "weapons/traploop.wav",
            "weapons/trapsuck.wav",
            "weapons/trapdown.wav",
            NULL
        },
    },

    /*QUAKED weapon_grenadelauncher (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "weapon_grenadelauncher",
        .pickup             = Pickup_Weapon,
        .use                = Use_Weapon,
        .drop               = Drop_Weapon,
        .weaponthink        = Weapon_GrenadeLauncher,
        .pickup_sound       = "misc/w_pkup.wav",
        .world_model        = "models/weapons/g_launch/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .view_model         = "models/weapons/v_launch/tris.md2",
        .icon               = "w_glauncher",
        .pickup_name        = "Grenade Launcher",
        .quantity           = 1,
        .ammo               = "Grenades",
        .flags              = IT_WEAPON | IT_STAY_COOP,
        .weapmodel          = WEAP_GRENADELAUNCHER,
        .precaches          = (const char *const[]) {
            "models/objects/grenade/tris.md2",
            "weapons/grenlf1a.wav",
            "weapons/grenlr1b.wav",
            "weapons/grenlb1b.wav",
            NULL
        },
    },

    // ROGUE
    /*QUAKED weapon_proxlauncher (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "weapon_proxlauncher",
        .pickup             = Pickup_Weapon,
        .use                = Use_Weapon,
        .drop               = Drop_Weapon,
        .weaponthink        = Weapon_ProxLauncher,
        .pickup_sound       = "misc/w_pkup.wav",
        .world_model        = "models/weapons/g_plaunch/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .view_model         = "models/weapons/v_plaunch/tris.md2",
        .icon               = "w_proxlaunch",
        .pickup_name        = "Prox Launcher",
        .quantity           = 1,
        .ammo               = "Prox",
        .flags              = IT_WEAPON,
        .weapmodel          = WEAP_PROXLAUNCH,
        .tag                = AMMO_PROX,
        .precaches          = (const char *const[]) {
            "weapons/grenlf1a.wav",
            "weapons/grenlr1b.wav",
            "weapons/grenlb1b.wav",
            "weapons/proxwarn.wav",
            "weapons/proxopen.wav",
            NULL
        },
    },
    // rogue

    /*QUAKED weapon_rocketlauncher (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "weapon_rocketlauncher",
        .pickup             = Pickup_Weapon,
        .use                = Use_Weapon,
        .drop               = Drop_Weapon,
        .weaponthink        = Weapon_RocketLauncher,
        .pickup_sound       = "misc/w_pkup.wav",
        .world_model        = "models/weapons/g_rocket/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .view_model         = "models/weapons/v_rocket/tris.md2",
        .icon               = "w_rlauncher",
        .pickup_name        = "Rocket Launcher",
        .quantity           = 1,
        .ammo               = "Rockets",
        .flags              = IT_WEAPON | IT_STAY_COOP,
        .weapmodel          = WEAP_ROCKETLAUNCHER,
        .precaches          = (const char *const[]) {
            "models/objects/rocket/tris.md2",
            "models/objects/debris2/tris.md2",
            "weapons/rockfly.wav",
            "weapons/rocklf1a.wav",
            "weapons/rocklr1b.wav",
            NULL
        },
    },

    /*QUAKED weapon_hyperblaster (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "weapon_hyperblaster",
        .pickup             = Pickup_Weapon,
        .use                = Use_Weapon2,
        .drop               = Drop_Weapon,
        .weaponthink        = Weapon_HyperBlaster,
        .pickup_sound       = "misc/w_pkup.wav",
        .world_model        = "models/weapons/g_hyperb/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .view_model         = "models/weapons/v_hyperb/tris.md2",
        .icon               = "w_hyperblaster",
        .pickup_name        = "HyperBlaster",
        .quantity           = 1,
        .ammo               = "Cells",
        .flags              = IT_WEAPON | IT_STAY_COOP,
        .weapmodel          = WEAP_HYPERBLASTER,
        .precaches          = (const char *const[]) {
            "models/objects/laser/tris.md2",
            "weapons/hyprbu1a.wav",
            "weapons/hyprbl1a.wav",
            "weapons/hyprbf1a.wav",
            "weapons/hyprbd1a.wav",
            "misc/lasfly.wav",
            NULL
        },
    },

    /*QUAKED weapon_boomer (.3 .3 1) (-16 -16 -16) (16 16 16)
    */

    {
        .classname          = "weapon_boomer",
        .pickup             = Pickup_Weapon,
        .use                = Use_Weapon,
        .drop               = Drop_Weapon,
        .weaponthink        = Weapon_Ionripper,
        .pickup_sound       = "misc/w_pkup.wav",
        .world_model        = "models/weapons/g_boom/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .view_model         = "models/weapons/v_boomer/tris.md2",
        .icon               = "w_ripper",
        .pickup_name        = "Ionripper",
        .quantity           = 2,
        .ammo               = "Cells",
        .flags              = IT_WEAPON,
        .weapmodel          = WEAP_BOOMER,
        .precaches          = (const char *const []) {
            "weapons/rg_hum.wav",
            "weapons/rippfire.wav",
            NULL
        },
    },
// END 14-APR-98

    // ROGUE
    /*QUAKED weapon_plasmabeam (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "weapon_plasmabeam",
        .pickup             = Pickup_Weapon,
        .use                = Use_Weapon,
        .drop               = Drop_Weapon,
        .weaponthink        = Weapon_Heatbeam,
        .pickup_sound       = "misc/w_pkup.wav",
        .world_model        = "models/weapons/g_beamer/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .view_model         = "models/weapons/v_beamer/tris.md2",
        .icon               = "w_heatbeam",
        .pickup_name        = "Plasma Beam",
        .quantity           = 2,
        .ammo               = "Cells",
        .flags              = IT_WEAPON,
        .weapmodel          = WEAP_PLASMA,
        .precaches          = (const char *const[]) {
            "models/weapons/v_beamer2/tris.md2",
            "weapons/bfg__l1a.wav",
            NULL
        },
    },
    //rogue
    /*QUAKED weapon_railgun (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "weapon_railgun",
        .pickup             = Pickup_Weapon,
        .use                = Use_Weapon2,
        .drop               = Drop_Weapon,
        .weaponthink        = Weapon_Railgun,
        .pickup_sound       = "misc/w_pkup.wav",
        .world_model        = "models/weapons/g_rail/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .view_model         = "models/weapons/v_rail/tris.md2",
        .icon               = "w_railgun",
        .pickup_name        = "Railgun",
        .quantity           = 1,
        .ammo               = "Slugs",
        .flags              = IT_WEAPON | IT_STAY_COOP,
        .weapmodel          = WEAP_RAILGUN,
        .precaches          = (const char *const[]) {
            "weapons/railgf1a.wav",
            "weapons/rg_hum.wav",
            NULL
        },
    },

// RAFAEL 14-APR-98
    /*QUAKED weapon_phalanx (.3 .3 1) (-16 -16 -16) (16 16 16)
    */

    {
        .classname          = "weapon_phalanx",
        .pickup             = Pickup_Weapon,
        .use                = Use_Weapon,
        .drop               = Drop_Weapon,
        .weaponthink        = Weapon_Phalanx,
        .pickup_sound       = "misc/w_pkup.wav",
        .world_model        = "models/weapons/g_shotx/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .view_model         = "models/weapons/v_shotx/tris.md2",
        .icon               = "w_phallanx",
        .pickup_name        = "Phalanx",
        .quantity           = 1,
        .ammo               = "Mag Slug",
        .flags              = IT_WEAPON,
        .weapmodel          = WEAP_PHALANX,
        .precaches          = (const char *const []) {
            "weapons/plasshot.wav",
            NULL
        },
    },

    /*QUAKED weapon_bfg (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "weapon_bfg",
        .pickup             = Pickup_Weapon,
        .use                = Use_Weapon,
        .drop               = Drop_Weapon,
        .weaponthink        = Weapon_BFG,
        .pickup_sound       = "misc/w_pkup.wav",
        .world_model        = "models/weapons/g_bfg/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .view_model         = "models/weapons/v_bfg/tris.md2",
        .icon               = "w_bfg",
        .pickup_name        = "BFG10K",
        .quantity           = 50,
        .ammo               = "Cells",
        .flags              = IT_WEAPON | IT_STAY_COOP,
        .weapmodel          = WEAP_BFG,
        .precaches          = (const char *const[]) {
            "sprites/s_bfg1.sp2",
            "sprites/s_bfg2.sp2",
            "sprites/s_bfg3.sp2",
            "weapons/bfg__f1y.wav",
            "weapons/bfg__l1a.wav",
            "weapons/bfg__x1b.wav",
            "weapons/bfg_hum.wav",
            NULL
        },
    },

// =========================
// ROGUE WEAPONS
    /*QUAKED weapon_chainfist (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "weapon_chainfist",
        .pickup             = Pickup_Weapon,
        .use                = Use_Weapon,
        .drop               = Drop_Weapon,
        .weaponthink        = Weapon_ChainFist,
        .pickup_sound       = "misc/w_pkup.wav",
        .world_model        = "models/weapons/g_chainf/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .view_model         = "models/weapons/v_chainf/tris.md2",
        .icon               = "w_chainfist",
        .pickup_name        = "Chainfist",
        .flags              = IT_WEAPON | IT_MELEE,
        .weapmodel          = WEAP_CHAINFIST,
        .tag                = 1,
        .precaches          = (const char *const[]) {
            "weapons/sawidle.wav",
            "weapons/sawhit.wav",
            NULL
        },
    },

    /*QUAKED weapon_disintegrator (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "weapon_disintegrator",
        .pickup             = Pickup_Weapon,
        .use                = Use_Weapon,
        .drop               = Drop_Weapon,
        .weaponthink        = Weapon_Disintegrator,
        .pickup_sound       = "misc/w_pkup.wav",
        .world_model        = "models/weapons/g_dist/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .view_model         = "models/weapons/v_dist/tris.md2",
        .icon               = "w_disintegrator",
        .pickup_name        = "Disruptor",
        .quantity           = 1,
        .ammo               = "Rounds",
        .flags              = IT_NOT_GIVEABLE,
        .weapmodel          = WEAP_DISRUPTOR,
        .tag                = 1,
        .precaches          = (const char *const[]) {
            "models/items/spawngro/tris.md2",
            "models/proj/disintegrator/tris.md2",
            "weapons/disrupt.wav",
            "weapons/disint2.wav",
            "weapons/disrupthit.wav",
            NULL
        },
    },

// ROGUE WEAPONS
// =========================

    //
    // AMMO ITEMS
    //

    /*QUAKED ammo_shells (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "ammo_shells",
        .pickup             = Pickup_Ammo,
        .drop               = Drop_Ammo,
        .pickup_sound       = "misc/am_pkup.wav",
        .world_model        = "models/items/ammo/shells/medium/tris.md2",
        .icon               = "a_shells",
        .pickup_name        = "Shells",
        .count_width        = 3,
        .quantity           = 10,
        .flags              = IT_AMMO,
        .tag                = AMMO_SHELLS,
    },

    /*QUAKED ammo_bullets (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "ammo_bullets",
        .pickup             = Pickup_Ammo,
        .drop               = Drop_Ammo,
        .pickup_sound       = "misc/am_pkup.wav",
        .world_model        = "models/items/ammo/bullets/medium/tris.md2",
        .icon               = "a_bullets",
        .pickup_name        = "Bullets",
        .count_width        = 3,
        .quantity           = 50,
        .flags              = IT_AMMO,
        .tag                = AMMO_BULLETS,
    },

    /*QUAKED ammo_cells (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "ammo_cells",
        .pickup             = Pickup_Ammo,
        .drop               = Drop_Ammo,
        .pickup_sound       = "misc/am_pkup.wav",
        .world_model        = "models/items/ammo/cells/medium/tris.md2",
        .icon               = "a_cells",
        .pickup_name        = "Cells",
        .count_width        = 3,
        .quantity           = 50,
        .flags              = IT_AMMO,
        .tag                = AMMO_CELLS,
    },

    /*QUAKED ammo_rockets (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "ammo_rockets",
        .pickup             = Pickup_Ammo,
        .drop               = Drop_Ammo,
        .pickup_sound       = "misc/am_pkup.wav",
        .world_model        = "models/items/ammo/rockets/medium/tris.md2",
        .icon               = "a_rockets",
        .pickup_name        = "Rockets",
        .count_width        = 3,
        .quantity           = 5,
        .flags              = IT_AMMO,
        .tag                = AMMO_ROCKETS,
    },

    /*QUAKED ammo_slugs (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "ammo_slugs",
        .pickup             = Pickup_Ammo,
        .drop               = Drop_Ammo,
        .pickup_sound       = "misc/am_pkup.wav",
        .world_model        = "models/items/ammo/slugs/medium/tris.md2",
        .icon               = "a_slugs",
        .pickup_name        = "Slugs",
        .count_width        = 3,
        .quantity           = 10,
        .flags              = IT_AMMO,
        .tag                = AMMO_SLUGS,
    },

    /*QUAKED ammo_magslug (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        .classname          = "ammo_magslug",
        .pickup             = Pickup_Ammo,
        .drop               = Drop_Ammo,
        .pickup_sound       = "misc/am_pkup.wav",
        .world_model        = "models/objects/ammo/tris.md2",
        .icon               = "a_mslugs",
        .pickup_name        = "Mag Slug",
        .count_width        = 3,
        .quantity           = 10,
        .flags              = IT_AMMO,
        .tag                = AMMO_MAGSLUG,
    },

// =======================================
// ROGUE AMMO

    /*QUAKED ammo_flechettes (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "ammo_flechettes",
        .pickup             = Pickup_Ammo,
        .drop               = Drop_Ammo,
        .pickup_sound       = "misc/am_pkup.wav",
        .world_model        = "models/ammo/am_flechette/tris.md2",
        .icon               = "a_flechettes",
        .pickup_name        = "Flechettes",
        .count_width        = 3,
        .quantity           = 50,
        .flags              = IT_AMMO,
        .tag                = AMMO_FLECHETTES,
    },
    /*QUAKED ammo_prox (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "ammo_prox",
        .pickup             = Pickup_Ammo,
        .drop               = Drop_Ammo,
        .pickup_sound       = "misc/am_pkup.wav",
        .world_model        = "models/ammo/am_prox/tris.md2",
        .icon               = "a_prox",
        .pickup_name        = "Prox",
        .count_width        = 3,
        .quantity           = 5,
        .flags              = IT_AMMO,
        .tag                = AMMO_PROX,
        .precaches          = (const char *const[]) {
            "models/weapons/g_prox/tris.md2",
            "weapons/proxwarn.wav",
            NULL
        },
    },

    /*QUAKED ammo_tesla (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "ammo_tesla",
        .pickup             = Pickup_Ammo,
        .use                = Use_Weapon,
        .drop               = Drop_Ammo,
        .weaponthink        = Weapon_Tesla,
        .pickup_sound       = "misc/am_pkup.wav",
        .world_model        = "models/ammo/am_tesl/tris.md2",
        .view_model         = "models/weapons/v_tesla/tris.md2",
        .icon               = "a_tesla",
        .pickup_name        = "Tesla",
        .count_width        = 3,
        .quantity           = 5,
        .ammo               = "Tesla",
        .flags              = IT_AMMO | IT_WEAPON,
        .tag                = AMMO_TESLA,
        .precaches          = (const char *const[]) {
            "models/weapons/v_tesla2/tris.md2",
            "weapons/teslaopen.wav",
            "weapons/hgrenb1a.wav",
            "weapons/hgrenb2a.wav",
            "models/weapons/g_tesla/tris.md2",
            NULL
        },
    },

    /*QUAKED ammo_nuke (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "ammo_nuke",
        .pickup             = Pickup_Nuke,
        .use                = Use_Nuke,
        .drop               = Drop_Ammo,
        .pickup_sound       = "misc/am_pkup.wav",
        .world_model        = "models/weapons/g_nuke/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "p_nuke",
        .pickup_name        = "A-M Bomb",
        .count_width        = 3,
        .quantity           = 300,
        .ammo               = "A-M Bomb",
        .flags              = IT_POWERUP,
        .precaches          = (const char *const[]) {
            "weapons/nukewarn2.wav",
            "world/rumble.wav",
            NULL
        },
    },

    /*QUAKED ammo_disruptor (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "ammo_disruptor",
        .pickup             = Pickup_Ammo,
        .drop               = Drop_Ammo,
        .pickup_sound       = "misc/am_pkup.wav",
        .world_model        = "models/ammo/am_disr/tris.md2",
        .icon               = "a_disruptor",
        .pickup_name        = "Rounds",     // FIXME
        .count_width        = 3,
        .quantity           = 15,
        .flags              = IT_NOT_GIVEABLE,
        .tag                = 0,
    },
// ROGUE AMMO
// =======================================

    //
    // POWERUP ITEMS
    //
    /*QUAKED item_quad (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "item_quad",
        .pickup             = Pickup_Powerup,
        .use                = Use_Quad,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/quaddama/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "p_quad",
        .pickup_name        = "Quad Damage",
        .count_width        = 2,
        .quantity           = 60,
        .flags              = IT_POWERUP,
        .precaches          = (const char *const[]) {
            "items/damage.wav",
            "items/damage2.wav",
            "items/damage3.wav",
            NULL
        },
    },

    /*QUAKED item_quadfire (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        .classname          = "item_quadfire",
        .pickup             = Pickup_Powerup,
        .use                = Use_QuadFire,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/quadfire/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "p_quadfire",
        .pickup_name        = "DualFire Damage",
        .count_width        = 2,
        .quantity           = 60,
        .flags              = IT_POWERUP,
        .precaches          = (const char *const []) {
            "items/quadfire1.wav",
            "items/quadfire2.wav",
            "items/quadfire3.wav",
            NULL
        },
    },

    /*QUAKED item_invulnerability (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "item_invulnerability",
        .pickup             = Pickup_Powerup,
        .use                = Use_Invulnerability,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/invulner/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "p_invulnerability",
        .pickup_name        = "Invulnerability",
        .count_width        = 2,
        .quantity           = 300,
        .flags              = IT_POWERUP,
        .precaches          = (const char *const[]) {
            "items/protect.wav",
            "items/protect2.wav",
            "items/protect4.wav",
            NULL
        },
    },

    /*QUAKED item_silencer (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "item_silencer",
        .pickup             = Pickup_Powerup,
        .use                = Use_Silencer,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/silencer/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "p_silencer",
        .pickup_name        = "Silencer",
        .count_width        = 2,
        .quantity           = 60,
        .flags              = IT_POWERUP,
    },

    /*QUAKED item_breather (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "item_breather",
        .pickup             = Pickup_Powerup,
        .use                = Use_Breather,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/breather/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "p_rebreather",
        .pickup_name        = "Rebreather",
        .count_width        = 2,
        .quantity           = 60,
        .flags              = IT_STAY_COOP | IT_POWERUP,
        .precaches          = (const char *const[]) {
            "items/airout.wav",
            NULL
        },
    },

    /*QUAKED item_enviro (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "item_enviro",
        .pickup             = Pickup_Powerup,
        .use                = Use_Envirosuit,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/enviro/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "p_envirosuit",
        .pickup_name        = "Environment Suit",
        .count_width        = 2,
        .quantity           = 60,
        .flags              = IT_STAY_COOP | IT_POWERUP,
        .precaches          = (const char *const[]) {
            "items/airout.wav",
            NULL
        },
    },

    /*QUAKED item_ancient_head (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    Special item that gives +2 to maximum health
    */
    {
        .classname          = "item_ancient_head",
        .pickup             = Pickup_AncientHead,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/c_head/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "i_fixme",
        .pickup_name        = "Ancient Head",
        .count_width        = 2,
        .quantity           = 60,
    },

    /*QUAKED item_adrenaline (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    gives +1 to maximum health
    */
    {
        .classname          = "item_adrenaline",
        .pickup             = Pickup_Adrenaline,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/adrenal/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "p_adrenaline",
        .pickup_name        = "Adrenaline",
        .count_width        = 2,
        .quantity           = 60,
    },

    /*QUAKED item_bandolier (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "item_bandolier",
        .pickup             = Pickup_Bandolier,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/band/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "p_bandolier",
        .pickup_name        = "Bandolier",
        .count_width        = 2,
        .quantity           = 60,
    },

    /*QUAKED item_pack (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "item_pack",
        .pickup             = Pickup_Pack,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/pack/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "i_pack",
        .pickup_name        = "Ammo Pack",
        .count_width        = 2,
        .quantity           = 180,
    },

// ======================================
// PGM

    /*QUAKED item_ir_goggles (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    gives +1 to maximum health
    */
    {
        .classname          = "item_ir_goggles",
        .pickup             = Pickup_Powerup,
        .use                = Use_IR,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/goggles/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "p_ir",
        .pickup_name        = "IR Goggles",
        .count_width        = 2,
        .quantity           = 60,
        .flags              = IT_POWERUP,
        .precaches          = (const char *const[]) {
            "misc/ir_start.wav",
            NULL
        },
    },

    /*QUAKED item_double (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "item_double",
        .pickup             = Pickup_Powerup,
        .use                = Use_Double,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/ddamage/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "p_double",
        .pickup_name        = "Double Damage",
        .count_width        = 2,
        .quantity           = 60,
        .flags              = IT_POWERUP,
        .precaches          = (const char *const[]) {
            "misc/ddamage1.wav",
            "misc/ddamage2.wav",
            "misc/ddamage3.wav",
            NULL
        },
    },

    /*Q U A K E D item_torch (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    /*
        {
        .classname          = "item_torch",
        .pickup             = Pickup_Powerup,
        .use                = Use_Torch,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/objects/fire/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "p_torch",
        .pickup_name        = "torch",
        .count_width        = 2,
        .quantity           = 60,
        .flags              = IT_POWERUP,
    },
    */

    /*QUAKED item_compass (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "item_compass",
        .pickup             = Pickup_Powerup,
        .use                = Use_Compass,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/objects/fire/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "p_compass",
        .pickup_name        = "compass",
        .count_width        = 2,
        .quantity           = 60,
        .flags              = IT_POWERUP,
    },

    /*QUAKED item_sphere_vengeance (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "item_sphere_vengeance",
        .pickup             = Pickup_Sphere,
        .use                = Use_Vengeance,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/vengnce/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "p_vengeance",
        .pickup_name        = "vengeance sphere",
        .count_width        = 2,
        .quantity           = 60,
        .flags              = IT_POWERUP,
        .precaches          = (const char *const[]) {
            "spheres/v_idle.wav",
            NULL
        },
    },

    /*QUAKED item_sphere_hunter (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "item_sphere_hunter",
        .pickup             = Pickup_Sphere,
        .use                = Use_Hunter,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/hunter/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "p_hunter",
        .pickup_name        = "hunter sphere",
        .count_width        = 2,
        .quantity           = 120,
        .flags              = IT_POWERUP,
        .precaches          = (const char *const[]) {
            "spheres/h_idle.wav",
            "spheres/h_active.wav",
            "spheres/h_lurk.wav",
            NULL
        },
    },

    /*QUAKED item_sphere_defender (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "item_sphere_defender",
        .pickup             = Pickup_Sphere,
        .use                = Use_Defender,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/defender/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "p_defender",
        .pickup_name        = "defender sphere",
        .count_width        = 2,
        .quantity           = 60,
        .flags              = IT_POWERUP,
        .precaches          = (const char *const[]) {
            "models/proj/laser2/tris.md2",
            "models/items/shell/tris.md2",
            "spheres/d_idle.wav",
            NULL
        },
    },

    /*QUAKED item_doppleganger (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "item_doppleganger",
        .pickup             = Pickup_Doppleganger,
        .use                = Use_Doppleganger,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/dopple/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "p_doppleganger",
        .pickup_name        = "Doppleganger",
        .quantity           = 90,
        .flags              = IT_POWERUP,
        .precaches          = (const char *const[]) {
            "models/objects/dopplebase/tris.md2",
            "models/items/spawngro2/tris.md2",
            "models/items/hunter/tris.md2",
            "models/items/vengnce/tris.md2",
            NULL
        },
    },

    {
        .pickup             = Tag_PickupToken,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/tagtoken/tris.md2",
        .world_model_flags  = EF_ROTATE | EF_TAGTRAIL,
        .icon               = "i_tagtoken",
        .pickup_name        = "Tag Token",
        .flags              = IT_POWERUP | IT_NOT_GIVEABLE,
        .tag                = 1,
    },

// PGM
// ======================================

    //
    // KEYS
    //
    /*QUAKED key_data_cd (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    key for computer centers
    */
    {
        .classname          = "key_data_cd",
        .pickup             = Pickup_Key,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/keys/data_cd/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "k_datacd",
        .pickup_name        = "Data CD",
        .count_width        = 2,
        .flags              = IT_STAY_COOP | IT_KEY,
    },

    /*QUAKED key_power_cube (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN NO_TOUCH
    warehouse circuits
    */
    {
        .classname          = "key_power_cube",
        .pickup             = Pickup_Key,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/keys/power/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "k_powercube",
        .pickup_name        = "Power Cube",
        .count_width        = 2,
        .flags              = IT_STAY_COOP | IT_KEY,
    },

    /*QUAKED key_pyramid (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    key for the entrance of jail3
    */
    {
        .classname          = "key_pyramid",
        .pickup             = Pickup_Key,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/keys/pyramid/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "k_pyramid",
        .pickup_name        = "Pyramid Key",
        .count_width        = 2,
        .flags              = IT_STAY_COOP | IT_KEY,
    },

    /*QUAKED key_data_spinner (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    key for the city computer
    */
    {
        .classname          = "key_data_spinner",
        .pickup             = Pickup_Key,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/keys/spinner/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "k_dataspin",
        .pickup_name        = "Data Spinner",
        .count_width        = 2,
        .flags              = IT_STAY_COOP | IT_KEY,
    },

    /*QUAKED key_pass (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    security pass for the security level
    */
    {
        .classname          = "key_pass",
        .pickup             = Pickup_Key,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/keys/pass/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "k_security",
        .pickup_name        = "Security Pass",
        .count_width        = 2,
        .flags              = IT_STAY_COOP | IT_KEY,
    },

    /*QUAKED key_blue_key (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    normal door key - blue
    */
    {
        .classname          = "key_blue_key",
        .pickup             = Pickup_Key,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/keys/key/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "k_bluekey",
        .pickup_name        = "Blue Key",
        .count_width        = 2,
        .flags              = IT_STAY_COOP | IT_KEY,
    },

    /*QUAKED key_red_key (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    normal door key - red
    */
    {
        .classname          = "key_red_key",
        .pickup             = Pickup_Key,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/keys/red_key/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "k_redkey",
        .pickup_name        = "Red Key",
        .count_width        = 2,
        .flags              = IT_STAY_COOP | IT_KEY,
    },

// RAFAEL
    /*QUAKED key_green_key (0 .5 .8) (-16 -16 -16) (16 16 16)
    normal door key - blue
    */
    {
        .classname          = "key_green_key",
        .pickup             = Pickup_Key,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/keys/green_key/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "k_green",
        .pickup_name        = "Green Key",
        .count_width        = 2,
        .flags              = IT_STAY_COOP | IT_KEY,
    },

    /*QUAKED key_commander_head (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    tank commander's head
    */
    {
        .classname          = "key_commander_head",
        .pickup             = Pickup_Key,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/monsters/commandr/head/tris.md2",
        .world_model_flags  = EF_GIB,
        .icon               = "k_comhead",
        .pickup_name        = "Commander's Head",
        .count_width        = 2,
        .flags              = IT_STAY_COOP | IT_KEY,
    },

    /*QUAKED key_airstrike_target (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    tank commander's head
    */
    {
        .classname          = "key_airstrike_target",
        .pickup             = Pickup_Key,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/items/keys/target/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "i_airstrike",
        .pickup_name        = "Airstrike Marker",
        .count_width        = 2,
        .flags              = IT_STAY_COOP | IT_KEY,
    },

// ======================================
// PGM

    /*QUAKED key_nuke_container (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "key_nuke_container",
        .pickup             = Pickup_Key,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/weapons/g_nuke/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "i_contain",
        .pickup_name        = "Antimatter Pod",
        .count_width        = 2,
        .flags              = IT_STAY_COOP | IT_KEY,
    },

    /*QUAKED key_nuke (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
    */
    {
        .classname          = "key_nuke",
        .pickup             = Pickup_Key,
        .drop               = Drop_General,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/weapons/g_nuke/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "i_nuke",
        .pickup_name        = "Antimatter Bomb",
        .count_width        = 2,
        .flags              = IT_STAY_COOP | IT_KEY,
    },

// PGM
// ======================================

    {
        .pickup             = Pickup_Health,
        .pickup_sound       = "items/pkup.wav",
        .icon               = "i_health",
        .pickup_name        = "Health",
        .count_width        = 3,
        .precaches          = (const char *const[]) {      // PMM - health sound fix
            "items/s_health.wav",
            "items/n_health.wav",
            "items/l_health.wav",
            "items/m_health.wav",
            NULL
        },
    },

    // CTF (R-CTF-1): both flags and all five techs.  Content unions -- §7 rule 3
    // -- so they are in the one itemlist every ruleset compiles against and
    // SpawnItem removes the flags outside ctf.  Four tech classnames, five
    // techs: item_tech1..4 plus the flag-carrier bonus is not an item.
    /*QUAKED item_flag_team1 (1 0.2 0) (-16 -16 -24) (16 16 32)
    */
    {
        .classname          = "item_flag_team1",
        .pickup             = CTFPickup_Flag,
        .drop               = CTFDrop_Flag,
        .pickup_sound       = "ctf/flagtk.wav",
        .world_model        = "players/male/flag1.md2",
        .world_model_flags  = EF_FLAG1,
        .icon               = "i_ctf1",
        .pickup_name        = "Red Flag",
        .count_width        = 2,
        .precaches          = (const char *const[]) {
            "ctf/flagcap.wav",
            NULL
        },
    },

    /*QUAKED item_flag_team2 (1 0.2 0) (-16 -16 -24) (16 16 32)
    */
    {
        .classname          = "item_flag_team2",
        .pickup             = CTFPickup_Flag,
        .drop               = CTFDrop_Flag,
        .pickup_sound       = "ctf/flagtk.wav",
        .world_model        = "players/male/flag2.md2",
        .world_model_flags  = EF_FLAG2,
        .icon               = "i_ctf2",
        .pickup_name        = "Blue Flag",
        .count_width        = 2,
        .precaches          = (const char *const[]) {
            "ctf/flagcap.wav",
            NULL
        },
    },

    /* Resistance Tech */
    {
        .classname          = "item_tech1",
        .pickup             = CTFPickup_Tech,
        .drop               = CTFDrop_Tech,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/ctf/resistance/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "tech1",
        .pickup_name        = "Disruptor Shield",
        .count_width        = 2,
        .flags              = IT_TECH,
        .precaches          = (const char *const[]) {
            "ctf/tech1.wav",
            NULL
        },
    },

    /* Strength Tech */
    {
        .classname          = "item_tech2",
        .pickup             = CTFPickup_Tech,
        .drop               = CTFDrop_Tech,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/ctf/strength/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "tech2",
        .pickup_name        = "Power Amplifier",
        .count_width        = 2,
        .flags              = IT_TECH,
        .precaches          = (const char *const[]) {
            "ctf/tech2.wav",
            "ctf/tech2x.wav",
            NULL
        },
    },

    /* Haste Tech */
    {
        .classname          = "item_tech3",
        .pickup             = CTFPickup_Tech,
        .drop               = CTFDrop_Tech,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/ctf/haste/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "tech3",
        .pickup_name        = "Time Accel",
        .count_width        = 2,
        .flags              = IT_TECH,
        .precaches          = (const char *const[]) {
            "ctf/tech3.wav",
            NULL
        },
    },

    /* Regeneration Tech */
    {
        .classname          = "item_tech4",
        .pickup             = CTFPickup_Tech,
        .drop               = CTFDrop_Tech,
        .pickup_sound       = "items/pkup.wav",
        .world_model        = "models/ctf/regeneration/tris.md2",
        .world_model_flags  = EF_ROTATE,
        .icon               = "tech4",
        .pickup_name        = "AutoDoc",
        .count_width        = 2,
        .flags              = IT_TECH,
        .precaches          = (const char *const[]) {
            "ctf/tech4.wav",
            NULL
        },
    },

    // end of list marker
    { NULL }
};

/*QUAKED item_health (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
*/
void SP_item_health(edict_t *self)
{
    if (deathmatch->value && ((int)dmflags->value & DF_NO_HEALTH)) {
        G_FreeEdict(self);
        return;
    }

    self->model = "models/items/healing/medium/tris.md2";
    self->count = 10;
    SpawnItem(self, FindItem("Health"));
    gi.soundindex("items/n_health.wav");
}

/*QUAKED item_health_small (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
*/
void SP_item_health_small(edict_t *self)
{
    if (deathmatch->value && ((int)dmflags->value & DF_NO_HEALTH)) {
        G_FreeEdict(self);
        return;
    }

    self->model = "models/items/healing/stimpack/tris.md2";
    self->count = 2;
    SpawnItem(self, FindItem("Health"));
    self->style = HEALTH_IGNORE_MAX;
    gi.soundindex("items/s_health.wav");
}

/*QUAKED item_health_large (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
*/
void SP_item_health_large(edict_t *self)
{
    if (deathmatch->value && ((int)dmflags->value & DF_NO_HEALTH)) {
        G_FreeEdict(self);
        return;
    }

    self->model = "models/items/healing/large/tris.md2";
    self->count = 25;
    SpawnItem(self, FindItem("Health"));
    gi.soundindex("items/l_health.wav");
}

/*QUAKED item_health_mega (.3 .3 1) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN
*/
void SP_item_health_mega(edict_t *self)
{
    if (deathmatch->value && ((int)dmflags->value & DF_NO_HEALTH)) {
        G_FreeEdict(self);
        return;
    }

    self->model = "models/items/mega_h/tris.md2";
    self->count = 100;
    SpawnItem(self, FindItem("Health"));
    gi.soundindex("items/m_health.wav");
    self->style = HEALTH_IGNORE_MAX | HEALTH_TIMED;
}

// RAFAEL
void SP_item_foodcube(edict_t *self)
{
    if (deathmatch->value && ((int)dmflags->value & DF_NO_HEALTH)) {
        G_FreeEdict(self);
        return;
    }

    self->model = "models/objects/trapfx/tris.md2";
    SpawnItem(self, FindItem("Health"));
    self->spawnflags |= DROPPED_ITEM;
    self->style = HEALTH_IGNORE_MAX;
    gi.soundindex("items/s_health.wav");
    self->classname = "foodcube";
}

void InitItems(void)
{
    game.num_items = q_countof(itemlist) - 1;
}

/*
===============
SetItemNames

Called by worldspawn
===============
*/
void SetItemNames(void)
{
    for (int i = 0; i < game.num_items; i++)
        gi.configstring(game.csr.items + i, itemlist[i].pickup_name);

    jacket_armor_index = ITEM_INDEX(FindItem("Jacket Armor"));
    combat_armor_index = ITEM_INDEX(FindItem("Combat Armor"));
    body_armor_index   = ITEM_INDEX(FindItem("Body Armor"));
    power_screen_index = ITEM_INDEX(FindItem("Power Screen"));
    power_shield_index = ITEM_INDEX(FindItem("Power Shield"));
}

//===============
//ROGUE
void SP_xatrix_item(edict_t *self)
{
    const gitem_t   *item;
    int     i;
    char    *spawnClass = NULL;

    if (!self->classname)
        return;

    if (!strcmp(self->classname, "ammo_magslug"))
        spawnClass = "ammo_flechettes";
    else if (!strcmp(self->classname, "ammo_trap"))
        spawnClass = "weapon_proxlauncher";
    else if (!strcmp(self->classname, "item_quadfire")) {
        float   chance = 0;

        chance = random();
        if (chance < 0.2f)
            spawnClass = "item_sphere_hunter";
        else if (chance < 0.6f)
            spawnClass = "item_sphere_vengeance";
        else
            spawnClass = "item_sphere_defender";
    } else if (!strcmp(self->classname, "weapon_boomer"))
        spawnClass = "weapon_etf_rifle";
    else if (!strcmp(self->classname, "weapon_phalanx"))
        spawnClass = "weapon_plasmabeam";

    if (!spawnClass)
        return;

    // check item spawn functions
    for (i = 0, item = itemlist; i < game.num_items; i++, item++) {
        if (!item->classname)
            continue;
        if (!strcmp(item->classname, spawnClass)) {
            // found it
            SpawnItem(self, item);
            return;
        }
    }
}
//ROGUE
//===============
