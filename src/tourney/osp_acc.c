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
// osp_acc.c -- the accuracy table's two entry points.
//
// Why this file exists.  Tourney's accuracy report needs three numbers per
// weapon per player: shots fired, shots that hit, and damage given and taken.
// The donor collects them by writing `p_acc[client->resp.clientid]` inline at
// **fifteen** sites across g_weapon.c and g_combat.c -- one per weapon, each
// repeating the same five-line shape with a different ACC_ constant, and each
// guarded by its own copy of `sync_stat > 2` (except three that forgot).
//
// Fifteen copies of a table write in shared files is exactly what the gate
// discipline avoids, and the fifteen are not even consistent with each other.  So the
// concept is expressed once, here, and the spine calls two functions:
//
//   OSP_accShot(self, mod, count)             count shots left the weapon
//   OSP_accDamage(targ, attacker, mod, take)  damage landed on a player
//
// The mapping from `mod` to the ACC_ column lives here too, which is what makes
// the call sites one line: the spine already knows the MOD it is firing or
// damaging with, and it should not have to know tourney's column numbering.
//
// What this changes, measured.  Damage credit moves from fifteen scattered
// sites to the single choke point every one of them was feeding: T_Damage.
// Rocket splash, grenade splash and BFG blast reached the donor's table twice
// on a direct hit -- once inline and once through T_RadiusDamage's own block --
// and here they are credited once, at the point the damage is actually taken.
// The three ungated sites (hand grenade, launched grenade, direct rocket) are
// now gated like the other twelve, which is the behaviour the report assumes:
// `sync_stat > 2` is "a match is running", and accuracy outside a match is not
// a number the mod ever shows.
//
// Two consequences that change the numbers, not only where they are written --
// both improvements, both worth knowing before comparing a report with a 1999
// one:
//
//   * `given`, `taken`, `dgiven` and `dtaken` now carry `take` -- the damage
//     that survived armour, powerups and the team rules -- where the donor's
//     inline sites carried `damage`, the amount the weapon set out to do.  A
//     rail through body armour counted 100 there and counts what it actually
//     removed here.  T_Damage is the only place the difference is known.
//   * the donor leaves the damage totals ungated at two sites: fire_lead and
//     fire_rail put only the per-weapon HIT counter behind `sync_stat > 2`,
//     so a warmup shot still adds to that weapon's `given`/`taken` and to the
//     running `dgiven`/`dtaken` (`osp-tourney@1895f8e`, g_weapon.c).  Here
//     everything is gated together, so a warmup shot cannot land in a match's
//     damage total.

#include "g_local.h"
#include "tourney/osp_types.h"

// -1 for a means of death that is not a weapon: falling, drowning, telefrag,
// a laser, the hook, a monster, a powerup.  Those have no accuracy column and
// never had.
//
// The second half of the table is the content layers'.  The donor stops at the
// BFG because osp-tourney's itemlist does; both layers are valid with every
// ruleset here, so a `tdm` match on a Reckoning or Ground Zero map was fought
// with weapons this function answered -1 about -- and the layer fire functions
// called OSP_accShot from nowhere at all, so both halves had to be added.
// MOD_NUKE is deliberately absent: the A-M Bomb is `ammo_nuke`, IT_POWERUP,
// and a powerup that does damage is not a weapon.  MOD_BLASTER2 too -- that is
// the monsters' green blaster, not a player weapon.
static int acc_column(int mod)
{
    switch (mod & ~MOD_FRIENDLY_FIRE) {
    case MOD_BLASTER:       return ACC_BLASTER;
    case MOD_SHOTGUN:       return ACC_SHOTGUN;
    case MOD_SSHOTGUN:      return ACC_SSHOTGUN;
    case MOD_MACHINEGUN:    return ACC_MACHINEGUN;
    case MOD_CHAINGUN:      return ACC_CHAINGUN;
    case MOD_GRENADE:
    case MOD_G_SPLASH:      return ACC_GRENADELAUNCHER;
    case MOD_HANDGRENADE:
    case MOD_HG_SPLASH:
    case MOD_HELD_GRENADE:  return ACC_GRENADE;
    case MOD_ROCKET:
    case MOD_R_SPLASH:      return ACC_ROCKET;
    case MOD_HYPERBLASTER:  return ACC_HYPERBLASTER;
    case MOD_RAILGUN:       return ACC_RAILGUN;
    case MOD_BFG_LASER:
    case MOD_BFG_BLAST:
    case MOD_BFG_EFFECT:    return ACC_BFG;

    // Xatrix
    case MOD_RIPPER:        return ACC_RIPPER;
    case MOD_PHALANX:       return ACC_PHALANX;
    case MOD_TRAP:          return ACC_TRAP;

    // Ground Zero.  The Disruptor's two MODs share one column the way the BFG's
    // three do: MOD_TRACKER is the beam, MOD_DISINTEGRATOR the finishing blow.
    case MOD_ETF_RIFLE:     return ACC_ETF_RIFLE;
    case MOD_PROX:          return ACC_PROX;
    case MOD_HEATBEAM:      return ACC_HEATBEAM;
    case MOD_CHAINFIST:     return ACC_CHAINFIST;
    case MOD_TESLA:         return ACC_TESLA;
    case MOD_TRACKER:
    case MOD_DISINTEGRATOR: return ACC_DISRUPTOR;

    default:                return -1;
    }
}

// A player is in the table only while they hold a client id, which
// OSP_giveClientID hands out on ClientBegin and OSP_initID resets.  Anything
// else -- a monster, a turret, the world -- is not.
static bool accountable(edict_t *ent)
{
    return ent && ent->inuse && ent->client &&
           ent->client->resp.clientid >= 0 &&
           ent->client->resp.clientid < (int)q_countof(p_acc);
}

// `count` is pellets, not trigger pulls: the donor adds
// DEFAULT_DEATHMATCH_SHOTGUN_COUNT for one shotgun blast and one per bullet
// for the machinegun, because the hits it counts are per pellet too.  An
// accuracy figure whose numerator and denominator disagree about what a shot
// is would read as 8% for a shotgun that hit with every pellet.
void OSP_accShot(edict_t *self, int mod, int count)
{
    int col;

    if (sync_stat <= 2 || !accountable(self) || count <= 0)
        return;

    col = acc_column(mod);
    if (col >= 0)
        p_acc[self->client->resp.clientid].shots[col] += count;
}

void OSP_accDamage(edict_t *targ, edict_t *attacker, int mod, int take)
{
    int col;

    if (sync_stat <= 2 || take <= 0)
        return;
    if (!accountable(targ) || !accountable(attacker) || targ == attacker)
        return;

    // The running totals are per player, not per weapon, so they are credited
    // for anything that damaged a player -- including a means of death with no
    // accuracy column.
    p_acc[attacker->client->resp.clientid].dgiven += take;
    p_acc[targ->client->resp.clientid].dtaken += take;

    col = acc_column(mod);
    if (col < 0)
        return;

    p_acc[attacker->client->resp.clientid].hits[col]++;
    p_acc[attacker->client->resp.clientid].given[col] += take;
    p_acc[targ->client->resp.clientid].taken[col] += take;
}
