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
#include "bot/bl_main.h"
#include "bot/bl_spawn.h"
#include "bot/p_menulib.h"
#include "tourney/osp_hooks.h"
#include "m_player.h"
#include "arena/arena.h"
#include "arena/ra2stats.h"

void SP_misc_teleporter_dest(edict_t *ent);

//
// Gross, ugly, disgustuing hack section
//

// this function is an ugly as hell hack to fix some map flaws
//
// the coop spawn spots on some maps are SNAFU.  There are coop spots
// with the wrong targetname as well as spots with no name at all
//
// we use carnal knowledge of the maps to fix the coop spot targetnames to match
// that of the nearest named single player spot

void SP_FixCoopSpots(edict_t *self)
{
    edict_t *spot;
    vec3_t  d;

    spot = NULL;

    while (1) {
        spot = G_Find(spot, FOFS(classname), "info_player_start");
        if (!spot)
            return;
        if (!spot->targetname)
            continue;
        VectorSubtract(self->s.origin, spot->s.origin, d);
        if (VectorLength(d) < 384) {
            if ((!self->targetname) || Q_stricmp(self->targetname, spot->targetname) != 0) {
//              gi.dprintf("FixCoopSpots changed %s at %s targetname from %s to %s\n", self->classname, vtos(self->s.origin), self->targetname, spot->targetname);
                self->targetname = spot->targetname;
            }
            return;
        }
    }
}

// now if that one wasn't ugly enough for you then try this one on for size
// some maps don't have any coop spots at all, so we need to create them
// where they should have been

void SP_CreateCoopSpots(edict_t *self)
{
    edict_t *spot;

    if (Q_stricmp(level.mapname, "security") == 0) {
        spot = G_Spawn();
        spot->classname = "info_player_coop";
        spot->s.origin[0] = 188 - 64;
        spot->s.origin[1] = -164;
        spot->s.origin[2] = 80;
        spot->targetname = "jail3";
        spot->s.angles[1] = 90;

        spot = G_Spawn();
        spot->classname = "info_player_coop";
        spot->s.origin[0] = 188 + 64;
        spot->s.origin[1] = -164;
        spot->s.origin[2] = 80;
        spot->targetname = "jail3";
        spot->s.angles[1] = 90;

        spot = G_Spawn();
        spot->classname = "info_player_coop";
        spot->s.origin[0] = 188 + 128;
        spot->s.origin[1] = -164;
        spot->s.origin[2] = 80;
        spot->targetname = "jail3";
        spot->s.angles[1] = 90;

        return;
    }
}

/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
The normal starting point for a level.
*/
void SP_info_player_start(edict_t *self)
{
    if (!coop->value)
        return;
    if (Q_stricmp(level.mapname, "security") == 0) {
        // invoke one of our gross, ugly, disgusting hacks
        self->think = SP_CreateCoopSpots;
        self->nextthink = level.framenum + 1;
    }
}

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for deathmatch games
*/
void SP_info_player_deathmatch(edict_t *self)
{
    if (!deathmatch->value) {
        G_FreeEdict(self);
        return;
    }
    SP_misc_teleporter_dest(self);
}

/*QUAKED info_player_coop (1 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for coop games
*/

void SP_info_player_coop(edict_t *self)
{
    if (!coop->value) {
        G_FreeEdict(self);
        return;
    }

    if ((Q_stricmp(level.mapname, "jail2") == 0)   ||
        (Q_stricmp(level.mapname, "jail4") == 0)   ||
        (Q_stricmp(level.mapname, "mine1") == 0)   ||
        (Q_stricmp(level.mapname, "mine2") == 0)   ||
        (Q_stricmp(level.mapname, "mine3") == 0)   ||
        (Q_stricmp(level.mapname, "mine4") == 0)   ||
        (Q_stricmp(level.mapname, "lab") == 0)     ||
        (Q_stricmp(level.mapname, "boss1") == 0)   ||
        (Q_stricmp(level.mapname, "fact3") == 0)   ||
        (Q_stricmp(level.mapname, "biggun") == 0)  ||
        (Q_stricmp(level.mapname, "space") == 0)   ||
        (Q_stricmp(level.mapname, "command") == 0) ||
        (Q_stricmp(level.mapname, "power2") == 0) ||
        (Q_stricmp(level.mapname, "strike") == 0)) {
        // invoke one of our gross, ugly, disgusting hacks
        self->think = SP_FixCoopSpots;
        self->nextthink = level.framenum + 1;
    }
}

/*QUAKED info_player_coop_lava (1 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for coop games on rmine2 where lava level
needs to be checked
*/
void SP_info_player_coop_lava(edict_t *self)
{
    if (!coop->value) {
        G_FreeEdict(self);
        return;
    }
}

/*QUAKED info_player_intermission (1 0 1) (-16 -16 -24) (16 16 32)
The deathmatch intermission point will be at one of these
Use 'angles' instead of 'angle', so you can set pitch or roll as well as yaw.  'pitch yaw roll'
*/
void SP_info_player_intermission(edict_t *ent)
{
}

//=======================================================================

void player_pain(edict_t *self, edict_t *other, float kick, int damage)
{
    // player pain is handled at the end of the frame in P_DamageFeedback
}

static bool IsFemale(edict_t *ent)
{
    char        *info;

    if (!ent->client)
        return false;

    info = Info_ValueForKey(ent->client->pers.userinfo, "gender");
    if (info[0] == 'f' || info[0] == 'F')
        return true;
    return false;
}

static bool IsNeutral(edict_t *ent)
{
    char        *info;

    if (!ent->client)
        return false;

    info = Info_ValueForKey(ent->client->pers.userinfo, "gender");
    if (info[0] != 'f' && info[0] != 'F' && info[0] != 'm' && info[0] != 'M')
        return true;
    return false;
}

// Non-static: CTF's ghost and match code report obituaries too (R-CORE-13).
void ClientObituary(edict_t *self, edict_t *inflictor, edict_t *attacker)
{
    int         mod;
    char        *message;
    char        *message2;
    int         ff;

    if (G_Ruleset() == RULESET_ARENA)
        RA_Obituary(self, inflictor, attacker);

    if (coop->value && attacker->client)
        meansOfDeath |= MOD_FRIENDLY_FIRE;

    if (deathmatch->value || coop->value) {
        ff = meansOfDeath & MOD_FRIENDLY_FIRE;
        mod = meansOfDeath & ~MOD_FRIENDLY_FIRE;
        message = NULL;
        message2 = "";

        switch (mod) {
        case MOD_SUICIDE:
            message = "suicides";
            break;
        case MOD_FALLING:
            message = "cratered";
            break;
        case MOD_CRUSH:
            message = "was squished";
            break;
        case MOD_WATER:
            message = "sank like a rock";
            break;
        case MOD_SLIME:
            message = "melted";
            break;
        case MOD_LAVA:
            message = "does a back flip into the lava";
            break;
        case MOD_EXPLOSIVE:
        case MOD_BARREL:
            message = "blew up";
            break;
        case MOD_EXIT:
            message = "found a way out";
            break;
        case MOD_TARGET_LASER:
            message = "saw the light";
            break;
        case MOD_TARGET_BLASTER:
            message = "got blasted";
            break;
        case MOD_BOMB:
        case MOD_SPLASH:
        case MOD_TRIGGER_HURT:
            message = "was in the wrong place";
            break;
        // RAFAEL
        case MOD_GEKK:
        case MOD_BRAINTENTACLE:
            message = "that's gotta hurt";
            break;
        }
        if (attacker == self) {
            switch (mod) {
            case MOD_HELD_GRENADE:
                message = "tried to put the pin back in";
                break;
            case MOD_HG_SPLASH:
            case MOD_G_SPLASH:
                if (IsNeutral(self))
                    message = "tripped on its own grenade";
                else if (IsFemale(self))
                    message = "tripped on her own grenade";
                else
                    message = "tripped on his own grenade";
                break;
            case MOD_R_SPLASH:
                if (IsNeutral(self))
                    message = "blew itself up";
                else if (IsFemale(self))
                    message = "blew herself up";
                else
                    message = "blew himself up";
                break;
            case MOD_BFG_BLAST:
                message = "should have used a smaller gun";
                break;
            // RAFAEL 03-MAY-98
            case MOD_TRAP:
                message = "sucked into his own trap";
                break;
//ROGUE
            case MOD_DOPPLE_EXPLODE:
                if (IsNeutral(self))
                    message = "got caught in it's own trap";
                else if (IsFemale(self))
                    message = "got caught in her own trap";
                else
                    message = "got caught in his own trap";
                break;
//ROGUE
            default:
                if (IsNeutral(self))
                    message = "killed itself";
                else if (IsFemale(self))
                    message = "killed herself";
                else
                    message = "killed himself";
                break;
            }
        }
        if (message) {
            gi.bprintf(PRINT_MEDIUM, "%s %s.\n", self->client->pers.netname, message);
            // R-OSP-1: tourney refuses score changes during the countdown and
            // updates its team totals and rank order on every one that lands.
            if (G_Ruleset() == RULESET_TOURNEY)
                OSP_scoreChange(self, -1);
            else if (deathmatch->value)
                self->client->resp.score--;
            self->enemy = NULL;
            return;
        }

        self->enemy = attacker;
        if (attacker && attacker->client) {
            switch (mod) {
            case MOD_BLASTER:
                message = "was blasted by";
                break;
            case MOD_SHOTGUN:
                message = "was gunned down by";
                break;
            case MOD_SSHOTGUN:
                message = "was blown away by";
                message2 = "'s super shotgun";
                break;
            case MOD_MACHINEGUN:
                message = "was machinegunned by";
                break;
            case MOD_CHAINGUN:
                message = "was cut in half by";
                message2 = "'s chaingun";
                break;
            case MOD_GRENADE:
                message = "was popped by";
                message2 = "'s grenade";
                break;
            case MOD_G_SPLASH:
                message = "was shredded by";
                message2 = "'s shrapnel";
                break;
            case MOD_ROCKET:
                message = "ate";
                message2 = "'s rocket";
                break;
            case MOD_R_SPLASH:
                message = "almost dodged";
                message2 = "'s rocket";
                break;
            case MOD_HYPERBLASTER:
                message = "was melted by";
                message2 = "'s hyperblaster";
                break;
            case MOD_RAILGUN:
                message = "was railed by";
                break;
            case MOD_BFG_LASER:
                message = "saw the pretty lights from";
                message2 = "'s BFG";
                break;
            case MOD_BFG_BLAST:
                message = "was disintegrated by";
                message2 = "'s BFG blast";
                break;
            case MOD_BFG_EFFECT:
                message = "couldn't hide from";
                message2 = "'s BFG";
                break;
            case MOD_HANDGRENADE:
                message = "caught";
                message2 = "'s handgrenade";
                break;
            case MOD_HG_SPLASH:
                message = "didn't see";
                message2 = "'s handgrenade";
                break;
            case MOD_HELD_GRENADE:
                message = "feels";
                message2 = "'s pain";
                break;
            case MOD_TELEFRAG:
                message = "tried to invade";
                message2 = "'s personal space";
                break;
            case MOD_GRAPPLE:
                message = "was caught by";
                message2 = "'s grapple";
                break;
            // RAFAEL 14-APR-98
            case MOD_RIPPER:
                message = "ripped to shreds by";
                message2 = "'s ripper gun";
                break;
            case MOD_PHALANX:
                message = "was evaporated by";
                break;
            case MOD_TRAP:
                message = "caught in trap by";
                break;
                // END 14-APR-98

//===============
//ROGUE
            case MOD_CHAINFIST:
                message = "was shredded by";
                message2 = "'s ripsaw";
                break;
            case MOD_DISINTEGRATOR:
                message = "lost his grip courtesy of";
                message2 = "'s disintegrator";
                break;
            case MOD_ETF_RIFLE:
                message = "was perforated by";
                break;
            case MOD_HEATBEAM:
                message = "was scorched by";
                message2 = "'s plasma beam";
                break;
            case MOD_TESLA:
                message = "was enlightened by";
                message2 = "'s tesla mine";
                break;
            case MOD_PROX:
                message = "got too close to";
                message2 = "'s proximity mine";
                break;
            case MOD_NUKE:
                message = "was nuked by";
                message2 = "'s antimatter bomb";
                break;
            case MOD_VENGEANCE_SPHERE:
                message = "was purged by";
                message2 = "'s vengeance sphere";
                break;
            case MOD_DEFENDER_SPHERE:
                message = "had a blast with";
                message2 = "'s defender sphere";
                break;
            case MOD_HUNTER_SPHERE:
                message = "was killed like a dog by";
                message2 = "'s hunter sphere";
                break;
            case MOD_TRACKER:
                message = "was annihilated by";
                message2 = "'s disruptor";
                break;
            case MOD_DOPPLE_EXPLODE:
                message = "was blown up by";
                message2 = "'s doppleganger";
                break;
            case MOD_DOPPLE_VENGEANCE:
                message = "was purged by";
                message2 = "'s doppleganger";
                break;
            case MOD_DOPPLE_HUNTER:
                message = "was hunted down by";
                message2 = "'s doppleganger";
                break;
//ROGUE
//===============
            }
            if (message) {
                gi.bprintf(PRINT_MEDIUM, "%s %s %s%s\n", self->client->pers.netname, message, attacker->client->pers.netname, message2);
//ROGUE
                if (gamerules && gamerules->value) {
                    if (DMGame.Score) {
                        if (ff)
                            DMGame.Score(attacker, self, -1);
                        else
                            DMGame.Score(attacker, self, 1);
                    }
                    return;
                }
//ROGUE

                if (G_Ruleset() == RULESET_TOURNEY)
                    OSP_scoreChange(attacker, ff ? -1 : 1);
                else if (deathmatch->value) {
                    if (ff)
                        attacker->client->resp.score--;
                    else
                        attacker->client->resp.score++;
                }
                return;
            }
        }
    }

    gi.bprintf(PRINT_MEDIUM, "%s died.\n", self->client->pers.netname);

//ROGUE
//  if (g_showlogic && g_showlogic->value)
//  {
//      if (mod == MOD_UNKNOWN)
//          gi.dprintf ("Player killed by MOD_UNKNOWN\n");
//      else
//          gi.dprintf ("Player killed by undefined mod %d\n", mod);
//  }
//ROGUE

    if (G_Ruleset() == RULESET_TOURNEY) {
        OSP_scoreChange(self, -1);
    } else if (deathmatch->value)
//ROGUE
    {
        if (gamerules && gamerules->value) {
            if (DMGame.Score) {
                DMGame.Score(self, self, -1);
            }
            return;
        } else
            self->client->resp.score--;
    }
//ROGUE

}

void Touch_Item(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf);

static void TossClientWeapon(edict_t *self)
{
    const gitem_t   *item;
    edict_t     *drop;
    bool    quad;
    // RAFAEL
    bool    quadfire;
    float       spread;

    if (!deathmatch->value)
        return;

    item = self->client->pers.weapon;
    if (! self->client->pers.inventory[self->client->ammo_index])
        item = NULL;
    if (item && (strcmp(item->pickup_name, "Blaster") == 0))
        item = NULL;

    if (!((int)(dmflags->value) & DF_QUAD_DROP))
        quad = false;
    else
        quad = (self->client->quad_framenum > (level.framenum + 10));

    // RAFAEL
    if (!((int)(dmflags->value) & DF_QUADFIRE_DROP))
        quadfire = false;
    else
        quadfire = (self->client->quadfire_framenum > (level.framenum + 10));

    if (item && quad)
        spread = 22.5f;
    else if (item && quadfire)
        spread = 12.5f;
    else
        spread = 0.0f;

    if (item) {
        self->client->v_angle[YAW] -= spread;
        drop = Drop_Item(self, item);
        self->client->v_angle[YAW] += spread;
        drop->spawnflags = DROPPED_PLAYER_ITEM;
    }

    if (quad) {
        self->client->v_angle[YAW] += spread;
        drop = Drop_Item(self, FindItemByClassname("item_quad"));
        self->client->v_angle[YAW] -= spread;
        drop->spawnflags |= DROPPED_PLAYER_ITEM;

        drop->touch = Touch_Item;
        drop->nextthink = level.framenum + (self->client->quad_framenum - level.framenum);
        drop->think = G_FreeEdict;
    }

    // RAFAEL
    if (quadfire) {
        self->client->v_angle[YAW] += spread;
        drop = Drop_Item(self, FindItemByClassname("item_quadfire"));
        self->client->v_angle[YAW] -= spread;
        drop->spawnflags |= DROPPED_PLAYER_ITEM;

        drop->touch = Touch_Item;
        drop->nextthink = level.framenum + (self->client->quadfire_framenum - level.framenum);
        drop->think = G_FreeEdict;
    }
}

/*
==================
LookAtKiller
==================
*/
void LookAtKiller(edict_t *self, edict_t *inflictor, edict_t *attacker)
{
    vec3_t      dir;

    if (attacker && attacker != world && attacker != self) {
        VectorSubtract(attacker->s.origin, self->s.origin, dir);
    } else if (inflictor && inflictor != world && inflictor != self) {
        VectorSubtract(inflictor->s.origin, self->s.origin, dir);
    } else {
        self->client->killer_yaw = self->s.angles[YAW];
        return;
    }
    // PMM - fixed to correct for pitch of 0
    if (dir[0])
        self->client->killer_yaw = RAD2DEG(atan2f(dir[1], dir[0]));
    else if (dir[1] > 0)
        self->client->killer_yaw = 90;
    else if (dir[1] < 0)
        self->client->killer_yaw = 270;
    else
        self->client->killer_yaw = 0;

}

/*
==================
player_die
==================
*/
void player_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    int     n;

    VectorClear(self->avelocity);

    self->takedamage = DAMAGE_YES;
    self->movetype = MOVETYPE_TOSS;

    self->s.modelindex2 = 0;    // remove linked weapon model
    self->s.modelindex3 = 0;    // CTF: remove the linked flag model

    self->s.angles[0] = 0;
    self->s.angles[2] = 0;

    self->s.sound = 0;
    self->client->weapon_sound = 0;

    self->maxs[2] = -8;

//  self->solid = SOLID_NOT;
    self->svflags |= SVF_DEADMONSTER;

    if (!self->deadflag) {
        if (G_Ruleset() == RULESET_ARENA)
            GSLogDeath(self, inflictor, attacker);
        self->client->respawn_framenum = level.framenum + 1.0f * BASE_FRAMERATE;
        LookAtKiller(self, inflictor, attacker);
        self->client->ps.pmove.pm_type = PM_DEAD;
        ClientObituary(self, inflictor, attacker);

        // CTF: telefragging your own teammate on his spawn costs the attacker
        // the frag it just earned, rather than rewarding a bad spawn point.
        if (G_Ruleset() == RULESET_CTF && meansOfDeath == MOD_TELEFRAG &&
            self->client->resp.ctf_state < 2 && attacker->client &&
            self->client->resp.ctf_team == attacker->client->resp.ctf_team) {
            attacker->client->resp.score--;
            self->client->resp.ctf_state = 0;
        }

        // Flag captures, carrier defence and the four assist bonuses.  Scores
        // nothing outside ctf.
        CTFFragBonuses(self, inflictor, attacker);

        // R-OSP-3: both logs record the death, and only while a match is live
        // -- a warmup death is not a statistic.
        if (G_Ruleset() == RULESET_TOURNEY) {
            if (sync_stat > 2)
                sl_WriteStdLogDeath(&gi, level, self, inflictor, attacker);
            OSP_Stats_Death(self, inflictor, attacker);
        }

        // `client_deathweapdrop` decides whether a tourney player drops the
        // weapon they were holding; everywhere else it is unconditional.
        if (G_Ruleset() != RULESET_TOURNEY ||
            (int)client_deathweapdrop->value)
            TossClientWeapon(self);

        // ...and the rune goes with the body, into the rune pool.
        if (G_Ruleset() == RULESET_TOURNEY && rune_stat)
            OSP_deadDropRune(self);

        // ...and the grapple, the flag and the tech go with the body.  All three
        // are no-ops when there is nothing to drop.
        CTFPlayerResetGrapple(self);
        CTFDeadDropFlag(self);
        CTFDeadDropTech(self);
        // CTF adds `&& !showscores`: Cmd_Help_f toggles, so a player who already
        // had the scoreboard up got it turned OFF by their own death.
        //
        // Tourney does the same thing through its own flag rather than through
        // the help computer: `osp_r2dc` is "this client is dead and looking at
        // the board", which p_view.c reads to stop pushing HUD panels at them.
        if (G_Ruleset() == RULESET_TOURNEY) {
            if (sync_stat != 2 && !(self->flags & FL_BOT))
                self->client->resp.osp_r2dc = 1;
        } else if (deathmatch->value && !self->client->showscores) {
            Cmd_Help_f(self);       // show scores
        }

        // clear inventory
        // this is kind of ugly, but it's how we want to handle keys in coop
        for (n = 0; n < game.num_items; n++) {
            if (coop->value && itemlist[n].flags & IT_KEY)
                self->client->resp.coop_respawn.inventory[n] = self->client->pers.inventory[n];
            self->client->pers.inventory[n] = 0;
        }
    }

    if (gamerules && gamerules->value) { // if we're in a dm game, alert the game
        if (DMGame.PlayerDeath)
            DMGame.PlayerDeath(self, inflictor, attacker);
    }

    // remove powerups
    self->client->quad_framenum = 0;
    self->client->invincible_framenum = 0;
    self->client->breather_framenum = 0;
    self->client->enviro_framenum = 0;
    self->flags &= ~FL_POWER_ARMOR;

    // RAFAEL
    self->client->quadfire_framenum = 0;
//==============
// ROGUE stuff
    self->client->double_framenum = 0;

    // if there's a sphere around, let it know the player died.
    // vengeance and hunter will die if they're not attacking,
    // defender should always die
    if (self->client->owned_sphere) {
        edict_t *sphere;

        vec3_t zero = { 0 };

        sphere = self->client->owned_sphere;
        sphere->die(sphere, self, self, 0, zero);
    }

    // if we've been killed by the tracker, GIB!
    if ((meansOfDeath & ~MOD_FRIENDLY_FIRE) == MOD_TRACKER) {
        self->health = -100;
        damage = 400;
    }

    // make sure no trackers are still hurting us.
    if (self->client->tracker_pain_framenum) {
        RemoveAttackingPainDaemons(self);
    }

    // if we got obliterated by the nuke, don't gib
    if ((self->health < -80) && (meansOfDeath == MOD_NUKE))
        self->flags |= FL_NOGIB;

// ROGUE
//==============

    if (self->health < -40) {
        // PMM
        // don't toss gibs if we got vaped by the nuke
        if (!(self->flags & FL_NOGIB)) {
            // pmm
            // gib
            gi.sound(self, CHAN_BODY, gi.soundindex("misc/udeath.wav"), 1, ATTN_NORM, 0);

            // more meaty gibs for your dollar!
            if ((deathmatch->value) && (self->health < -80)) {
                for (n = 0; n < 4; n++)
                    ThrowGib(self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
            }

            for (n = 0; n < 4; n++)
                ThrowGib(self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
            // PMM
        }
        self->flags &= ~FL_NOGIB;
        // pmm

        ThrowClientHead(self, damage);

        self->takedamage = DAMAGE_NO;
    } else {
        // normal death
        if (!self->deadflag) {
            static int i;

            i = (i + 1) % 3;
            // start a death animation
            self->client->anim_priority = ANIM_DEATH;
            if (self->client->ps.pmove.pm_flags & PMF_DUCKED) {
                self->s.frame = FRAME_crdeath1 - 1;
                self->client->anim_end = FRAME_crdeath5;
            } else switch (i) {
                case 0:
                    self->s.frame = FRAME_death101 - 1;
                    self->client->anim_end = FRAME_death106;
                    break;
                case 1:
                    self->s.frame = FRAME_death201 - 1;
                    self->client->anim_end = FRAME_death206;
                    break;
                case 2:
                    self->s.frame = FRAME_death301 - 1;
                    self->client->anim_end = FRAME_death308;
                    break;
                }
            gi.sound(self, CHAN_VOICE, gi.soundindex(va("*death%i.wav", (Q_rand() % 4) + 1)), 1, ATTN_NORM, 0);
        }
    }

    self->deadflag = DEAD_DEAD;

    gi.linkentity(self);
}

//=======================================================================

/*
==============
InitClientPersistant

This is only called when the game first initializes in single player,
but is called after each death and level change in deathmatch
==============
*/
// Non-static: CTF re-initialises a client on a team change (R-CORE-13).
// `full` is OSP's (R-OSP-1): a partial reset keeps the client's identity --
// userinfo, netname and the referee "green name" -- and clears everything else,
// which is what a team change or a match reset wants.
//
// The donor computes it as `sizeof(userinfo) + sizeof(netname) +
// sizeof(greenname)` and memsets from that offset onward, which is only correct
// while those three are the FIRST members of client_persistant_t.  In this tree
// they are not: R-CORE-6's union put Ground Zero's ammo maxima and CTF's fields
// in between, so the donor's arithmetic would clear the identity it means to
// keep and preserve three unrelated ints.  Saved and restored by name instead
// (doc/reconciliation.md R-80).
void InitClientPersistant(gclient_t *client, bool full)
{
    const gitem_t   *item;
    char            userinfo[MAX_INFO_STRING];
    char            netname[16];
    char            greenname[16];

//  gi.dprintf("InitClientPersistant()\n");

    if (!full) {
        memcpy(userinfo, client->pers.userinfo, sizeof(userinfo));
        memcpy(netname, client->pers.netname, sizeof(netname));
        memcpy(greenname, client->pers.greenname, sizeof(greenname));
    }

    memset(&client->pers, 0, sizeof(client->pers));

    if (!full) {
        memcpy(client->pers.userinfo, userinfo, sizeof(userinfo));
        memcpy(client->pers.netname, netname, sizeof(netname));
        memcpy(client->pers.greenname, greenname, sizeof(greenname));
    }

    item = FindItem("Blaster");
    client->pers.selected_item = ITEM_INDEX(item);
    client->pers.inventory[client->pers.selected_item] = 1;

    client->pers.weapon = item;
    client->pers.lastweapon = item;

    // CTF: everyone owns the grapple from the first frame.  Gated, because in
    // dm and sp the item exists in the itemlist (R-CORE-2's union) but nobody
    // should be carrying it.
    if (G_Ruleset() == RULESET_CTF) {
        item = FindItem("Grapple");
        client->pers.inventory[ITEM_INDEX(item)] = 1;
    }

    client->pers.health         = 100;
    client->pers.max_health     = 100;

    client->pers.max_bullets    = 200;
    client->pers.max_shells     = 100;
    client->pers.max_rockets    = 50;
    client->pers.max_grenades   = 50;
    client->pers.max_cells      = 200;
    client->pers.max_slugs      = 50;

    // RAFAEL
    client->pers.max_magslug    = 50;
    client->pers.max_trap       = 5;
//ROGUE
    // FIXME - give these real numbers....
    client->pers.max_prox       = 50;
    client->pers.max_tesla      = 50;
    client->pers.max_flechettes = 200;
    // `max_rounds` sat behind #ifndef KILL_DISRUPTOR, which Ground Zero
    // #defines to 1, so it was already dead in the donor (R-CORE-3).
//ROGUE

    // R-OSP-1: tourney seeds its own per-player state here -- the ammo ceilings
    // a referee has set, the accuracy row, the rune counters -- which is the
    // one place that runs on both a fresh connect and a respawn.
    if (G_Ruleset() == RULESET_TOURNEY)
        OSP_seedPlayer(client);

    client->pers.connected = true;
}

void InitClientResp(gclient_t *client)
{
    // Two fields outlive a respawn under ctf: the team you are on, and whether
    // you asked for the player-id display.  Both live in resp rather than pers
    // because CTF re-initialises pers on a team change, so this is the only
    // place that can preserve them.
    int ctf_team = client->resp.ctf_team;
    bool id_state = client->resp.id_state;

    memset(&client->resp, 0, sizeof(client->resp));

    client->resp.ctf_team = ctf_team;
    client->resp.id_state = id_state;

    client->resp.enterframe = level.framenum;
    client->resp.coop_respawn = client->pers;

    if (G_Ruleset() == RULESET_CTF && client->resp.ctf_team < CTF_TEAM1)
        CTFAssignTeam(client);

    // RA2 adds ten assignments after its own memset and the Phase 4 merge kept
    // none of them, because every one is a statement the base does not have and
    // rule 2 takes the base where the base is unchanged.  Eight are what the
    // memset already gives (`context` 0, `fightstate` FIGHT_SPECTATING,
    // `omode` OMODE_NORMAL, `teammember` NULL, `isbot`, `zbotcount`,
    // `damagedealt`), which is why nothing noticed.  Two are not:
    //
    //   * `teamnum` MUST be -1, because 0 is a valid team index and
    //     PutClientInServer branches on `>= 0`.  Measured: every client
    //     connecting under `arena` arrived with teamnum 0, took reinit_player()
    //     instead of init_player(), and so was never given the team menu, never
    //     had its menu queue reset for a reused slot, and claimed membership of
    //     whatever team happens to be in slot 0 -- which on a deathmatch map is
    //     "#N Pickup Red", allocated first by arena_init().  R-RA-4's first row
    //     ("arena menu on connect") has been structurally false since Phase 4.
    //   * `ra_votes` MUST be votetries_setting, or the client starts with zero
    //     votes and menuVote refuses every one.  arena.c resets it per arena
    //     config change, which is a top-up and not the initial grant.
    //
    // The other eight stay unwritten: a redundant assignment is a claim that
    // the memset above might not do it, and R-CORE-6's union makes that claim
    // false for a field another ruleset owns.
    if (G_Ruleset() == RULESET_ARENA) {
        client->resp.teamnum = -1;
        client->resp.ra_votes = votetries_setting;
    }
}

/*
==================
SaveClientData

Some information that should be persistant, like health,
is still stored in the edict structure, so it needs to
be mirrored out to the client structure before all the
edicts are wiped.
==================
*/
void SaveClientData(void)
{
    int     i;
    edict_t *ent;

    for (i = 0; i < game.maxclients; i++) {
        ent = &g_edicts[1 + i];
        if (!ent->inuse)
            continue;
        game.clients[i].pers.health = ent->health;
        game.clients[i].pers.max_health = ent->max_health;
        game.clients[i].pers.savedFlags = (ent->flags & (FL_GODMODE | FL_NOTARGET | FL_POWER_ARMOR));
        if (coop->value)
            game.clients[i].pers.score = ent->client->resp.score;
    }
}

void FetchClientEntData(edict_t *ent)
{
    ent->health = ent->client->pers.health;
    ent->max_health = ent->client->pers.max_health;
    ent->flags |= ent->client->pers.savedFlags;
    if (coop->value)
        ent->client->resp.score = ent->client->pers.score;
}

/*
=======================================================================

  SelectSpawnPoint

=======================================================================
*/

/*
================
PlayersRangeFromSpot

Returns the distance to the nearest player from the given spot
================
*/
float PlayersRangeFromSpot(edict_t *spot)
{
    edict_t *player;
    float   bestplayerdistance;
    vec3_t  v;
    int     n;
    float   playerdistance;

    bestplayerdistance = 9999999;

    for (n = 1; n <= game.maxclients; n++) {
        player = &g_edicts[n];

        if (!player->inuse)
            continue;

        if (player->health <= 0)
            continue;

        // An arena observer is alive, noclipping and usually parked over the
        // spawn points; counting it makes the "farthest from any player" spawn
        // choose by where the audience is (R-RA-4, uGladQ2 v0.98.1u).  baseq2's
        // own spectators have the same quirk and keep it: q2pro has not fixed
        // it and sec 7 rule 1 says q2pro wins on behaviour that is not a
        // donor's feature.
        if (G_Ruleset() == RULESET_ARENA && G_IsObserver(player))
            continue;

        VectorSubtract(spot->s.origin, player->s.origin, v);
        playerdistance = VectorLength(v);

        if (playerdistance < bestplayerdistance)
            bestplayerdistance = playerdistance;
    }

    return bestplayerdistance;
}

/*
================
SelectRandomDeathmatchSpawnPoint

go to a random point, but NOT the two points closest
to other players
================
*/
edict_t *SelectRandomDeathmatchSpawnPoint(void)
{
    edict_t *spot, *spot1, *spot2;
    int     count = 0;
    int     selection;
    float   range, range1, range2;

    spot = NULL;
    range1 = range2 = 99999;
    spot1 = spot2 = NULL;

    while ((spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
        count++;
        range = PlayersRangeFromSpot(spot);
        if (range < range1) {
            range1 = range;
            spot1 = spot;
        } else if (range < range2) {
            range2 = range;
            spot2 = spot;
        }
    }

    if (!count)
        return NULL;

    if (count <= 2) {
        spot1 = spot2 = NULL;
    } else
        count -= 2;

    selection = Q_rand_uniform(count);

    spot = NULL;
    do {
        spot = G_Find(spot, FOFS(classname), "info_player_deathmatch");
        if (spot == spot1 || spot == spot2)
            selection++;
    } while (selection--);

    return spot;
}

/*
================
SelectFarthestDeathmatchSpawnPoint

================
*/
edict_t *SelectFarthestDeathmatchSpawnPoint(void)
{
    edict_t *bestspot;
    float   bestdistance, bestplayerdistance;
    edict_t *spot;

    spot = NULL;
    bestspot = NULL;
    bestdistance = 0;
    while ((spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
        bestplayerdistance = PlayersRangeFromSpot(spot);

        if (bestplayerdistance > bestdistance) {
            bestspot = spot;
            bestdistance = bestplayerdistance;
        }
    }

    if (bestspot) {
        return bestspot;
    }

    // if there is a player just spawned on each and every start spot
    // we have no choice to turn one into a telefrag meltdown
    spot = G_Find(NULL, FOFS(classname), "info_player_deathmatch");

    return spot;
}

static edict_t *SelectDeathmatchSpawnPoint(void)
{
    if ((int)(dmflags->value) & DF_SPAWN_FARTHEST)
        return SelectFarthestDeathmatchSpawnPoint();
    else
        return SelectRandomDeathmatchSpawnPoint();
}

//===============
//ROGUE
edict_t *SelectLavaCoopSpawnPoint(edict_t *ent)
{
    int     index;
    edict_t *spot = NULL;
    float   lavatop;
    edict_t *lava;
    edict_t *pointWithLeastLava;
    float   lowest;
    edict_t *spawnPoints[64];
    vec3_t  center;
    int     numPoints;
    edict_t *highestlava;

    lavatop = -99999;
    highestlava = NULL;

    // first, find the highest lava
    // remember that some will stop moving when they've filled their
    // areas...
    lava = NULL;
    while (1) {
        lava = G_Find(lava, FOFS(classname), "func_door");
        if (!lava)
            break;

        VectorAdd(lava->absmax, lava->absmin, center);
        VectorScale(center, 0.5f, center);

        if (lava->spawnflags & 2 && (gi.pointcontents(center) & MASK_WATER)) {
            if (lava->absmax[2] > lavatop) {
                lavatop = lava->absmax[2];
                highestlava = lava;
            }
        }
    }

    // if we didn't find ANY lava, then return NULL
    if (!highestlava)
        return NULL;

    // find the top of the lava and include a small margin of error (plus bbox size)
    lavatop = highestlava->absmax[2] + 64;

    // find all the lava spawn points and store them in spawnPoints[]
    spot = NULL;
    numPoints = 0;
    while ((spot = G_Find(spot, FOFS(classname), "info_player_coop_lava")) != NULL) {
        if (numPoints == 64)
            break;

        spawnPoints[numPoints++] = spot;
    }

    if (numPoints < 1)
        return NULL;

    // walk up the sorted list and return the lowest, open, non-lava spawn point
    spot = NULL;
    lowest = 999999;
    pointWithLeastLava = NULL;
    for (index = 0; index < numPoints; index++) {
        if (spawnPoints[index]->s.origin[2] < lavatop)
            continue;

        if (PlayersRangeFromSpot(spawnPoints[index]) > 32) {
            if (spawnPoints[index]->s.origin[2] < lowest) {
                // save the last point
                pointWithLeastLava = spawnPoints[index];
                lowest = spawnPoints[index]->s.origin[2];
            }
        }
    }

    // FIXME - better solution????
    // well, we may telefrag someone, but oh well...
    if (pointWithLeastLava)
        return pointWithLeastLava;

    return NULL;
}
//ROGUE
//===============

static edict_t *SelectCoopSpawnPoint(edict_t *ent)
{
    int     index;
    edict_t *spot = NULL;
    char    *target;

//ROGUE
    // rogue hack, but not too gross...
    if (!Q_stricmp(level.mapname, "rmine2p") || !Q_stricmp(level.mapname, "rmine2"))
        return SelectLavaCoopSpawnPoint(ent);
//ROGUE

    index = ent->client - game.clients;

    // player 0 starts in normal player spawn point
    if (!index)
        return NULL;

    spot = NULL;

    // assume there are four coop spots at each spawnpoint
    while (1) {
        spot = G_Find(spot, FOFS(classname), "info_player_coop");
        if (!spot)
            return NULL;    // we didn't have enough...

        target = spot->targetname;
        if (!target)
            target = "";
        if (Q_stricmp(game.spawnpoint, target) == 0) {
            // this is a coop spawn point for one of the clients here
            index--;
            if (!index)
                return spot;        // this is it
        }
    }

    return spot;
}

/*
===========
SelectSpawnPoint

Chooses a player start, deathmatch start, coop start, etc
============
*/
void SelectSpawnPoint(edict_t *ent, vec3_t origin, vec3_t angles)
{
    edict_t *spot = NULL;

    if (deathmatch->value)
        spot = SelectDeathmatchSpawnPoint();
    else if (coop->value)
        spot = SelectCoopSpawnPoint(ent);

    // find a single player start spot
    if (!spot) {
        while ((spot = G_Find(spot, FOFS(classname), "info_player_start")) != NULL) {
            if (!game.spawnpoint[0] && !spot->targetname)
                break;

            if (!game.spawnpoint[0] || !spot->targetname)
                continue;

            if (Q_stricmp(game.spawnpoint, spot->targetname) == 0)
                break;
        }

        if (!spot) {
            if (!game.spawnpoint[0]) {
                // there wasn't a spawnpoint without a target, so use any
                spot = G_Find(spot, FOFS(classname), "info_player_start");
            }
            if (!spot)
                gi.error("Couldn't find spawn point %s", game.spawnpoint);
        }
    }

    VectorCopy(spot->s.origin, origin);
    VectorCopy(spot->s.angles, angles);
}

//======================================================================

void InitBodyQue(void)
{
    int     i;
    edict_t *ent;

    level.body_que = 0;
    for (i = 0; i < BODY_QUEUE_SIZE; i++) {
        ent = G_Spawn();
        ent->classname = "bodyque";
    }
}

void body_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    int n;

    if (self->health < -40) {
        gi.sound(self, CHAN_BODY, gi.soundindex("misc/udeath.wav"), 1, ATTN_NORM, 0);
        for (n = 0; n < 4; n++)
            ThrowGib(self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
        self->s.origin[2] -= 48;
        ThrowClientHead(self, damage);
        self->takedamage = DAMAGE_NO;
    }
}

static void CopyToBodyQue(edict_t *ent)
{
    edict_t     *body;

    gi.unlinkentity(ent);

    // grab a body que and cycle to the next one
    body = &g_edicts[game.maxclients + level.body_que + 1];
    level.body_que = (level.body_que + 1) % BODY_QUEUE_SIZE;

    // send an effect on the removed body
    if (body->s.modelindex) {
        gi.WriteByte(svc_temp_entity);
        gi.WriteByte(TE_BLOOD);
        gi.WritePosition(body->s.origin);
        gi.WriteDir(vec3_origin);
        gi.multicast(body->s.origin, MULTICAST_PVS);
    }

    gi.unlinkentity(body);

    body->s.number = body - g_edicts;
    VectorCopy(ent->s.origin, body->s.origin);
    VectorCopy(ent->s.origin, body->s.old_origin);
    VectorCopy(ent->s.angles, body->s.angles);
    body->s.modelindex = ent->s.modelindex;
    body->s.frame = ent->s.frame;
    body->s.skinnum = ent->s.skinnum;
    body->s.event = EV_OTHER_TELEPORT;

    body->svflags = ent->svflags;
    VectorCopy(ent->mins, body->mins);
    VectorCopy(ent->maxs, body->maxs);
    VectorCopy(ent->absmin, body->absmin);
    VectorCopy(ent->absmax, body->absmax);
    VectorCopy(ent->size, body->size);
    VectorCopy(ent->velocity, body->velocity);
    VectorCopy(ent->avelocity, body->avelocity);
    body->solid = ent->solid;
    body->clipmask = ent->clipmask;
    body->owner = ent->owner;
    body->movetype = ent->movetype;
    body->groundentity = ent->groundentity;

    body->die = body_die;
    body->takedamage = DAMAGE_YES;

    gi.linkentity(body);
}

void PutClientInServer(edict_t *ent);

void respawn(edict_t *self)
{
    if (deathmatch->value || coop->value) {
        // spectators don't leave bodies
        if (self->movetype != MOVETYPE_NOCLIP)
            CopyToBodyQue(self);
        self->svflags &= ~SVF_NOCLIENT;
        PutClientInServer(self);

        // add a teleportation effect
        self->s.event = EV_PLAYER_TELEPORT;

        // hold in place briefly
        self->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
        self->client->ps.pmove.pm_time = 112 >> PM_TIME_SHIFT;

        self->client->respawn_framenum = level.framenum;

        return;
    }

    // restart the entire server
    gi.AddCommandString("menu_loadgame\n");
}

/*
 * only called when pers.spectator changes
 * note that resp.spectator should be the opposite of pers.spectator here
 */
static void spectator_respawn(edict_t *ent)
{
    int i, numspec;

    // if the user wants to become a spectator, make sure he doesn't
    // exceed max_spectators

    if (ent->client->pers.spectator) {
        char *value = Info_ValueForKey(ent->client->pers.userinfo, "spectator");
        if (*spectator_password->string &&
            strcmp(spectator_password->string, "none") &&
            strcmp(spectator_password->string, value)) {
            gi.cprintf(ent, PRINT_HIGH, "Spectator password incorrect.\n");
            ent->client->pers.spectator = false;
            gi.WriteByte(svc_stufftext);
            gi.WriteString("spectator 0\n");
            gi.unicast(ent, true);
            return;
        }

        // count spectators
        for (i = 1, numspec = 0; i <= game.maxclients; i++) {
            if (g_edicts[i].inuse && g_edicts[i].client->pers.spectator)
                numspec++;
        }

        if (numspec >= maxspectators->value) {
            gi.cprintf(ent, PRINT_HIGH, "Server spectator limit is full.");
            ent->client->pers.spectator = false;
            // reset his spectator var
            gi.WriteByte(svc_stufftext);
            gi.WriteString("spectator 0\n");
            gi.unicast(ent, true);
            return;
        }
    } else {
        // he was a spectator and wants to join the game
        // he must have the right password
        char *value = Info_ValueForKey(ent->client->pers.userinfo, "password");
        if (*password->string && strcmp(password->string, "none") &&
            strcmp(password->string, value)) {
            gi.cprintf(ent, PRINT_HIGH, "Password incorrect.\n");
            ent->client->pers.spectator = true;
            gi.WriteByte(svc_stufftext);
            gi.WriteString("spectator 1\n");
            gi.unicast(ent, true);
            return;
        }
    }

    // clear score on respawn
    ent->client->pers.score = ent->client->resp.score = 0;

    ent->svflags &= ~SVF_NOCLIENT;
    PutClientInServer(ent);

    // add a teleportation effect
    if (!ent->client->pers.spectator) {
        // send effect
        gi.WriteByte(svc_muzzleflash);
        gi.WriteShort(ent - g_edicts);
        gi.WriteByte(MZ_LOGIN);
        gi.multicast(ent->s.origin, MULTICAST_PVS);

        // hold in place briefly
        ent->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
        ent->client->ps.pmove.pm_time = 112 >> PM_TIME_SHIFT;
    }

    ent->client->respawn_framenum = level.framenum;

    if (ent->client->pers.spectator)
        gi.bprintf(PRINT_HIGH, "%s has moved to the sidelines\n", ent->client->pers.netname);
    else
        gi.bprintf(PRINT_HIGH, "%s joined the game\n", ent->client->pers.netname);
}

//==============================================================

/*
===========
PutClientInServer

Called when a player connects to a server or respawns in
a deathmatch.
============
*/
void PutClientInServer(edict_t *ent)
{
    char    userinfo[MAX_INFO_STRING];
    vec3_t  mins = { -16, -16, -24};
    vec3_t  maxs = {16, 16, 32};
    int     index;
    vec3_t  spawn_origin, spawn_angles;
    gclient_t   *client;
    int     i;
    client_persistant_t saved;
    client_respawn_t    resp;
    vec3_t temp, temp2;
    trace_t tr;
    int     arenanum;

    // find a spawn point
    // do it before setting health back up, so farthest
    // ranging doesn't count this client
    // R-MODE-5: RA2 and tourney both replace spawn selection outright -- RA2
    // places into an arena, tourney into its match slots -- so this is a hook
    // rather than a predicate.
    G_SelectSpawnPoint(ent, spawn_origin, spawn_angles);

    index = ent - g_edicts - 1;
    client = ent->client;

    memcpy(userinfo, client->pers.userinfo, sizeof(userinfo));

    // deathmatch wipes most client data every spawn -- but not under tourney,
    // where the reset is the PARTIAL one and that is what `full` exists for.
    //
    // The donor passes false at both of this function's two reset sites, and
    // the merge kept baseq2's true at both, which is rule 2 taking the base
    // where the base is unchanged.  It is not cosmetic: tourney's
    // ClientUserinfoChanged may REFUSE a rename, and refusing means "put back
    // what they had" -- `Info_SetValueForKey(userinfo, "name", pers.netname)`.
    // With the full wipe, `pers.netname` is the empty string by the time the
    // refusal reads it, so the refusal installs an empty name and the userinfo
    // carries it from then on.  The refusal always fires here, because
    // ClientConnect's own userinfo change stamps the `client_infochange`
    // cooldown four seconds ahead and this runs inside it.
    //
    // Measured: every bot under `tourney` announced itself as " entered the
    // game" and every scoreboard row was blank.  It is not a bot defect -- a
    // human's first spawn takes the same path -- but a bot is what respawns
    // often enough to make it obvious.
    if (deathmatch->value) {
        resp = client->resp;
        InitClientPersistant(client, G_Ruleset() != RULESET_TOURNEY);
    } else if (coop->value) {
//      int         n;

        resp = client->resp;
        // this is kind of ugly, but it's how we want to handle keys in coop
//      for (n = 0; n < game.num_items; n++)
//      {
//          if (itemlist[n].flags & IT_KEY)
//              resp.coop_respawn.inventory[n] = client->pers.inventory[n];
//      }
        resp.coop_respawn.game_helpchanged = client->pers.game_helpchanged;
        resp.coop_respawn.helpchanged = client->pers.helpchanged;
        client->pers = resp.coop_respawn;
        if (resp.score > client->pers.score)
            client->pers.score = resp.score;
    } else {
        memset(&resp, 0, sizeof(resp));
    }

    ClientUserinfoChanged(ent, userinfo);

    // clear everything but the persistant data
    saved = client->pers;
    memset(client, 0, sizeof(*client));
    client->pers = saved;
    if (client->pers.health <= 0)
        InitClientPersistant(client, G_Ruleset() != RULESET_TOURNEY);
    client->resp = resp;

    // copy some data from the client to the entity
    FetchClientEntData(ent);

    // clear entity values
    ent->groundentity = NULL;
    ent->client = &game.clients[index];
    ent->takedamage = DAMAGE_AIM;
    ent->movetype = MOVETYPE_WALK;
    ent->viewheight = 22;
    ent->inuse = true;
    ent->classname = "player";
    ent->mass = 200;
    ent->solid = SOLID_BBOX;
    ent->deadflag = DEAD_NO;
    ent->air_finished_framenum = level.framenum + 12 * BASE_FRAMERATE;
    ent->clipmask = MASK_PLAYERSOLID;
    ent->model = "players/male/tris.md2";
    ent->pain = player_pain;
    ent->die = player_die;
    ent->waterlevel = 0;
    ent->watertype = 0;
    ent->flags &= ~FL_NO_KNOCKBACK;
    ent->svflags &= ~SVF_DEADMONSTER;

    ent->flags &= ~FL_SAM_RAIMI;        // PGM - turn off sam raimi flag

    VectorCopy(mins, ent->mins);
    VectorCopy(maxs, ent->maxs);
    VectorClear(ent->velocity);

    // R-MENU-3: spawning ends whatever menu was open.  Threewave leaves the
    // handle live and relies on the join menu being reopened over it.
    G_MenuClose(ent);

    // R-OSP-1: before the match is live a player spawns with the warmup
    // loadout rather than a blaster, so that warmup is practice rather than a
    // different game; and the grapple is let go, the accuracy row is opened and
    // the HUD panels are pointed at their configstrings.
    if (G_Ruleset() == RULESET_TOURNEY) {
        if (sync_stat < 2)
            OSP_warmupItems(ent);
        OSP_setSingleAccuracy(ent);
        OSP_hookoff_cmd(ent);
        OSP_restartStats(ent);
    }

    // clear playerstate values
    memset(&ent->client->ps, 0, sizeof(client->ps));

    // A client who was observing has PMF_NO_PREDICTION set from the chase cam;
    // spawning clears it.  Harmless where it was never set.
    client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;

    if (deathmatch->value && ((int)dmflags->value & DF_FIXED_FOV)) {
        client->ps.fov = 90;
    } else {
        client->ps.fov = Q_atoi(Info_ValueForKey(client->pers.userinfo, "fov"));
        if (client->ps.fov < 1)
            client->ps.fov = 90;
        else if (client->ps.fov > 160)
            client->ps.fov = 160;
    }

//PGM
    if (client->pers.weapon)
        client->ps.gunindex = gi.modelindex(client->pers.weapon->view_model);
    else
        client->ps.gunindex = 0;
//PGM

    // clear entity state values
    ent->s.sound = 0;
    ent->s.effects = 0;
    ent->s.renderfx = 0;
        ent->s.skinnum = ent - g_edicts - 1;
        ent->s.modelindex = MODELINDEX_PLAYER;  // will use the skin specified model
        ent->s.modelindex2 = MODELINDEX_PLAYER; // custom gun model

    ent->s.frame = 0;

    // try to properly clip to the floor / spawn
    VectorCopy(spawn_origin, temp);
    VectorCopy(spawn_origin, temp2);
    temp[2] -= 64;
    temp2[2] += 16;
    tr = gi.trace(temp2, ent->mins, ent->maxs, temp, ent, MASK_PLAYERSOLID);
    if (!tr.allsolid && !tr.startsolid && Q_stricmp(level.mapname, "tech5")) {
        VectorCopy(tr.endpos, ent->s.origin);
        ent->groundentity = tr.ent;
    } else {
        VectorCopy(spawn_origin, ent->s.origin);
        ent->s.origin[2] += 10; // make sure off ground
    }

    VectorCopy(ent->s.origin, ent->s.old_origin);

    for (i = 0; i < 3; i++) {
        client->ps.pmove.origin[i] = COORD2SHORT(ent->s.origin[i]);
    }

    spawn_angles[PITCH] = 0;
    spawn_angles[ROLL] = 0;

    // set the delta angle
    for (i = 0; i < 3; i++)
        client->ps.pmove.delta_angles[i] = ANGLE2SHORT(spawn_angles[i] - client->resp.cmd_angles[i]);

    VectorCopy(spawn_angles, ent->s.angles);
    VectorCopy(spawn_angles, client->ps.viewangles);
    VectorCopy(spawn_angles, client->v_angle);

    // R-MODE-5's 26th gate: the player now has a final origin and final angles.
    // The floor-clip trace above is the reason it exists -- RA2's port had to
    // move that trace into move_to_arena() and tourney's into
    // ClientBeginDeathmatch, so all three placement paths will coexist here and
    // anything that must run after placement hangs off one hook rather than
    // being copied into three functions.
    G_ClientPlaced(ent);

    // CTF decides here whether this client is a body at all: a player with no
    // team gets the join menu and stays an observer, and CTFStartClient says so
    // by returning true.  It is not an ops row because it must run *between* the
    // spawn point and KillBox, which no hook in ruleset_ops_t straddles --
    // ClientPlaced runs after placement, and this can prevent placement.
    if (G_Ruleset() == RULESET_CTF && CTFStartClient(ent))
        return;

    // spawn a spectator
    if (client->pers.spectator) {
        client->chase_target = NULL;

        client->resp.spectator = true;

        ent->movetype = MOVETYPE_NOCLIP;
        ent->solid = SOLID_NOT;
        ent->svflags |= SVF_NOCLIENT;
        ent->client->ps.gunindex = 0;
        gi.linkentity(ent);
        return;
    } else

        client->resp.spectator = false;

    if (G_Ruleset() == RULESET_ARENA) {
        // R-RA-4: a new client lands in observer mode with the menu up, and one
        // that already has a team is put back into its arena.  Both paths do
        // their own placement, which is why they run instead of the KillBox
        // below rather than after it.
        //
        // The tail below is the donor's, in the donor's order, and it is not
        // decoration: init_player() sets `fightstate` and opens the menu but
        // touches neither movetype nor solid, so the free-flying observer body
        // comes from SetObserverMode() *inside* move_to_arena().  Without this
        // call a connecting player was left standing at a deathmatch spawn as a
        // solid, visible, PM_NORMAL body in a ruleset that has no such thing --
        // found by a headless client reading back its own pmove type, which is
        // the one question about observing that a mod's own HUD cannot fake.
        client->resp.spawn_recheck = 0;

        gi.linkentity(ent);

        // ChangeWeapon before placement, as the donor has it: move_to_arena()
        // can centerprint and stuff a sound at the destination, and the weapon
        // must already be up when it does.
        client->newweapon = client->pers.weapon;
        ChangeWeapon(ent);

        if (client->resp.teamnum >= 0)
            reinit_player(ent);
        else
            init_player(ent);

        gi.linkentity(ent);

        // `context` is the arena this client belongs to; it is consumed here so
        // that a later menu action starts from arena 0 rather than repeating
        // this one.
        arenanum = client->resp.context;
        client->resp.context = 0;
        move_to_arena(ent, arenanum, 1);
        return;
    }

    if (!KillBox(ent)) {
        // could't spawn in?
    }

    gi.linkentity(ent);

    // my tribute to cash's level-specific hacks. I hope I live
    // up to his trailblazing cheese.
    if (Q_stricmp(level.mapname, "rboss") == 0) {
        // if you get on to rboss in single player or coop, ensure
        // the player has the nuke key. (not in DM)
        if (!(deathmatch->value)) {
            const gitem_t   *item;

            item = FindItem("Antimatter Bomb");
            client->pers.selected_item = ITEM_INDEX(item);
            client->pers.inventory[client->pers.selected_item] = 1;
        }
    }

    // force the current weapon up
    client->newweapon = client->pers.weapon;
    ChangeWeapon(ent);
}

/*
=====================
ClientBeginDeathmatch

A client has just connected to the server in
deathmatch mode, so clear everything out before starting them.
=====================
*/
static void ClientBeginDeathmatch(edict_t *ent)
{
    G_InitEdict(ent);

    InitClientResp(ent->client);

    //PGM
    if (gamerules && gamerules->value && DMGame.ClientBegin) {
        DMGame.ClientBegin(ent);
    }
    //PGM

    // R-OSP-1/2: the four things tourney asks a connecting client for, and the
    // entered/active bookkeeping that has to happen before placement.
    if (G_Ruleset() == RULESET_TOURNEY)
        OSP_clientBeginPre(ent);

    // locate ent at a spawn point
    PutClientInServer(ent);

    // R-ARENA-2 / R-RA-4 row 15.  After placement, not inside it: the bot is
    // now the noclip observer in arena 0 that init_player() made, which is
    // exactly the state a human is in when they pick a team off the menu, and
    // this is the click they cannot make.
    if (G_Ruleset() == RULESET_ARENA)
        RA_BotJoinArena(ent);

    if (level.intermission_framenum) {
        MoveClientToIntermission(ent);
    } else if (!G_IsObserver(ent)) {
        // send effect
        gi.WriteByte(svc_muzzleflash);
        gi.WriteShort(ent - g_edicts);
        gi.WriteByte(MZ_LOGIN);
        gi.multicast(ent->s.origin, MULTICAST_PVS);

        // hold in place briefly
        ent->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
        ent->client->ps.pmove.pm_time = 200 >> PM_TIME_SHIFT;
    }

    gi.bprintf(PRINT_HIGH, "%s entered the game\n", ent->client->pers.netname);

    // R-OSP-1: and everything after placement, including the one arm that can
    // refuse the connection outright -- a match already running with
    // `match_latejoin` off.  True means the client is gone.
    if (G_Ruleset() == RULESET_TOURNEY && OSP_clientBegunPost(ent))
        return;

    // R-OSP-11's bot half, and the same argument as arena's above: a tourney
    // client connects as an OBSERVER and enters by pressing a key, which a bot
    // cannot do.  After OSP_clientBegunPost rather than before it, because that
    // is where `resp.team` becomes 2 ("no team") and the team join reads it --
    // run before, the mode-2 join put every bot on team 0.
    if (G_Ruleset() == RULESET_TOURNEY)
        OSP_botJoin(ent);

    // make sure all view stuff is valid
    ClientEndServerFrame(ent);
}

/*
===========
ClientBegin

called when a client has finished connecting, and is ready
to be placed into the game.  This will happen every level load.
============
*/
void ClientBegin(edict_t *ent)
{
    int     i;

    ent->client = game.clients + (ent - g_edicts - 1);

    // R-OSP-1: the per-LEVEL half -- a client id, a place back on their team,
    // and the 1-vs-1 queue.  Before ClientBeginDeathmatch rather than inside
    // it, because it also has to run for a client who was already here when
    // the level changed.
    if (G_Ruleset() == RULESET_TOURNEY)
        OSP_clientBeginLevel(ent);

    if (deathmatch->value) {
        ClientBeginDeathmatch(ent);
        return;
    }

    // if there is already a body waiting for us (a loadgame), just
    // take it, otherwise spawn one from scratch
    if (ent->inuse == true) {
        // the client has cleared the client side viewangles upon
        // connecting to the server, which is different than the
        // state when the game is saved, so we need to compensate
        // with deltaangles
        for (i = 0; i < 3; i++)
            ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(ent->client->ps.viewangles[i]);
    } else {
        // a spawn point will completely reinitialize the entity
        // except for the persistant data that was initialized at
        // ClientConnect() time
        G_InitEdict(ent);
        ent->classname = "player";
        InitClientResp(ent->client);
        PutClientInServer(ent);

        // hold in place briefly
        ent->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
        ent->client->ps.pmove.pm_time = 200 >> PM_TIME_SHIFT;
    }

    if (level.intermission_framenum) {
        MoveClientToIntermission(ent);
    } else {
        // send effect if in a multiplayer game
        if (game.maxclients > 1) {
            gi.WriteByte(svc_muzzleflash);
            gi.WriteShort(ent - g_edicts);
            gi.WriteByte(MZ_LOGIN);
            gi.multicast(ent->s.origin, MULTICAST_PVS);

            gi.bprintf(PRINT_HIGH, "%s entered the game\n", ent->client->pers.netname);
        }
    }

    // make sure all view stuff is valid
    ClientEndServerFrame(ent);
}

/*
===========
ClientUserInfoChanged

called whenever the player updates a userinfo variable.

The game can override any of the settings in place
(forcing skins or names, etc) before copying it off.
============
*/
void ClientUserinfoChanged(edict_t *ent, char *userinfo)
{
    char    *s;
    int     playernum;

    // check for malformed or illegal info strings
    if (!Info_Validate(userinfo)) {
        strcpy(userinfo, "\\name\\badinfo\\skin\\male/grunt");
    }

    // R-OSP-1: tourney edits the userinfo it was handed -- refusing a forbidden
    // or too-frequent rename, forcing a qualifier skin -- and everything below
    // reads the edited copy, which is what makes the edit take effect.  It also
    // owns the rename logging and the green-text copy of the name.
    if (G_Ruleset() == RULESET_TOURNEY)
        OSP_userinfoChanged(ent, userinfo);

    // set name
    s = Info_ValueForKey(userinfo, "name");
    Q_strlcpy(ent->client->pers.netname, s, sizeof(ent->client->pers.netname));

    // set spectator
    s = Info_ValueForKey(userinfo, "spectator");
    // spectators are only supported in deathmatch
    // if (deathmatch->value && strcmp(s, "0"))
    //
    // ...and not under ctf, which has its own observer mechanism (R-CTF-5):
    // the `observer` command and the join menu, keyed on ctf_team.  Leaving
    // baseq2's flag reachable there would put two observer systems on one
    // client -- `spectator 1` in userinfo would noclip a player that
    // G_IsObserver() still reports as playing, because it asks about the team.
    // Threewave deletes the key outright; here it is simply inert.
    if (deathmatch->value && G_Ruleset() != RULESET_CTF && *s && strcmp(s, "0"))
        ent->client->pers.spectator = true;
    else
        ent->client->pers.spectator = false;

    // set skin
    s = Info_ValueForKey(userinfo, "skin");

    playernum = ent - g_edicts - 1;

    // combine name and skin into a configstring.  Under ctf the skin is the
    // team's, not the player's choice -- CTFAssignSkin writes the same
    // configstring with ctf_r or ctf_b substituted, which is also where the 1999
    // bot-model bug lived (R-CTF-7).
    if (G_Ruleset() == RULESET_CTF)
        CTFAssignSkin(ent, s);
    // `teams[]` is TAG_LEVEL and arena_init() reallocates it empty on every map,
    // while `resp.teamnum` is on the client and outlives the level -- so the
    // slot a returning client names may be NULL, and this dereferenced it.
    // Unreachable until Phase 7 for the usual reason (nothing was ever on an
    // arena team) and still not reached on a deathmatch map, where arena_init
    // rebuilds the two pickup teams into slots 0 and 1 and the stale index
    // happens to be valid again.  On a real arena map, where each client makes
    // a team of its own, it is a null dereference in ClientUserinfoChanged --
    // which BotSpawn calls for every bot before ClientBegin has reset resp.
    else if (G_Ruleset() == RULESET_ARENA && ent->client->resp.teamnum != -1 &&
             teams[ent->client->resp.teamnum].it &&
             ((team_t *)teams[ent->client->resp.teamnum].it)->skin != -1)
        // RA2 does the same thing for the same reason: on a team, the skin is
        // the team's.  Its `/nullxxx` placeholder branch is not carried -- that
        // string is written by RA2's own menu and means "no skin chosen", and
        // nothing in this tree writes it.
        setteamskin(ent, userinfo, ((team_t *)teams[ent->client->resp.teamnum].it)->skin);
    else
        gi.configstring(game.csr.playerskins + playernum, va("%s\\%s", ent->client->pers.netname, s));

    // CTF's player-id view reads the name out of a configstring of its own,
    // because the playerskins string carries the skin as well (R-CTF-6).
    // game.csr.general, not CS_GENERAL: the compile-time constant is the
    // extended one (13118) and a server without the extensions runs on
    // cs_remap_old, whose end is 2080 -- so the literal drops the server the
    // first time a client connects (doc/reconciliation.md R-62).
    if (G_Ruleset() == RULESET_CTF)
        gi.configstring(game.csr.general + playernum, ent->client->pers.netname);

    // fov
    if (deathmatch->value && ((int)dmflags->value & DF_FIXED_FOV)) {
        ent->client->ps.fov = 90;
    } else {
        ent->client->ps.fov = Q_atoi(Info_ValueForKey(userinfo, "fov"));
        if (ent->client->ps.fov < 1)
            ent->client->ps.fov = 90;
        else if (ent->client->ps.fov > 160)
            ent->client->ps.fov = 160;
    }

    // handedness
    s = Info_ValueForKey(userinfo, "hand");
    if (strlen(s)) {
        ent->client->pers.hand = Q_atoi(s);
    }

    // save off the userinfo in case we want to check something later
    Q_strlcpy(ent->client->pers.userinfo, userinfo, sizeof(ent->client->pers.userinfo));
}

/*
===========
ClientConnect

Called when a player begins connecting to the server.
The game can refuse entrance to a client by returning false.
If the client is allowed, the connection process will continue
and eventually get to ClientBegin()
Changing levels will NOT cause this to be called again, but
loadgames will.
============
*/
qboolean ClientConnect(edict_t *ent, char *userinfo)
{
    char    *value;

    // check to see if they are on the banned IP list
    value = Info_ValueForKey(userinfo, "ip");
    if (SV_FilterPacket(value)) {
        Info_SetValueForKey(userinfo, "rejmsg", "Banned.");
        return false;
    }

    // check for a spectator
    value = Info_ValueForKey(userinfo, "spectator");
//  if (deathmatch->value && strcmp(value, "0"))
    // Same gate as ClientUserinfoChanged's, and for the same reason: under ctf
    // and arena the spectator password and limit guard a state no client can
    // enter -- both donors deleted baseq2's spectator and have their own.
    if (deathmatch->value && G_Ruleset() != RULESET_CTF &&
        G_Ruleset() != RULESET_ARENA && *value && strcmp(value, "0")) {
        int i, numspec;

        if (*spectator_password->string &&
            strcmp(spectator_password->string, "none") &&
            strcmp(spectator_password->string, value)) {
            Info_SetValueForKey(userinfo, "rejmsg", "Spectator password required or incorrect.");
            return false;
        }

        // count spectators
        for (i = numspec = 0; i < game.maxclients; i++) {
            if (g_edicts[i + 1].inuse && g_edicts[i + 1].client->pers.spectator)
                numspec++;
        }

        if (numspec >= maxspectators->value) {
            Info_SetValueForKey(userinfo, "rejmsg", "Server spectator limit is full.");
            return false;
        }
    } else {
        // check for a password
        value = Info_ValueForKey(userinfo, "password");
        if (*password->string && strcmp(password->string, "none") &&
            strcmp(password->string, value)) {
            Info_SetValueForKey(userinfo, "rejmsg", "Password required or incorrect.");
            return false;
        }
    }

    // R-OSP-1: the player list gets a veto, with its own reason in `rejmsg`.
    if (G_Ruleset() == RULESET_TOURNEY && !OSP_clientAllowed(ent, userinfo))
        return false;

    // R-BOT-15.  This edict may be a BOT's -- G_SpawnClient hands bots the high
    // slots (R-BOT-14) but the engine hands a connecting human whichever slot
    // it picked, and on a full-but-for-one-bot server that is the bot's.  Move
    // the bot rather than overwriting it, and REFUSE the human if there is
    // nowhere to move it to; stealing the slot would leave the brain talking
    // about a client that is now somebody else.
    //
    // Not for a bot's own ClientConnect: BotCreate clears FL_BOT across the
    // call precisely so this does not recurse.
    if ((ent->flags & FL_BOT) && !BotMoveToFreeClientEdict(ent)) {
        Info_SetValueForKey(userinfo, "rejmsg", "Server is full.");
        return false;
    }

    // they can connect
    ent->client = game.clients + (ent - g_edicts - 1);

    // A player who dropped mid-match gets their seat, score and team back if
    // they come back inside `team_recovertime` -- which is what the pause in
    // ClientDisconnect is waiting for.  Before InitClientResp, because that is
    // what it has to survive.
    if (G_Ruleset() == RULESET_TOURNEY)
        OSP_recoverClient(ent, userinfo);

    // if there is already a body waiting for us (a loadgame), just
    // take it, otherwise spawn one from scratch
    if (ent->inuse == false) {
        // clear the respawning variables.  Under ctf, force a team join and turn
        // the player-id display on: R-CTF-6 wants id on by default, and
        // InitClientResp preserves both fields rather than setting them, so they
        // have to be seeded here.
        if (G_Ruleset() == RULESET_CTF) {
            ent->client->resp.ctf_team = -1;
            ent->client->resp.id_state = true;
        }
        InitClientResp(ent->client);
        if (!game.autosaved || !ent->client->pers.weapon)
            InitClientPersistant(ent->client, true);
    }

    ClientUserinfoChanged(ent, userinfo);

    if (game.maxclients > 1)
        gi.dprintf("%s connected\n", ent->client->pers.netname);

    if (G_Ruleset() == RULESET_TOURNEY)
        OSP_clientConnected(ent, userinfo);

    ent->svflags = 0; // make sure we start with known default
    ent->client->pers.connected = true;
    return true;
}

/*
===========
ClientDisconnect

Called when a player drops from the server.
Will not be called between levels.
============
*/
void ClientDisconnect(edict_t *ent)
{
    //int     playernum;
    int     osp_team = 2;

    if (!ent->client)
        return;

    if (G_Ruleset() == RULESET_TOURNEY) {
        // Tourney announces the departure itself, at the end and with the
        // client count -- "wimped out and left. (clients = 3)" -- so the plain
        // line here would be a duplicate.
        OSP_clientLeaving(ent, &osp_team);
    } else {
        gi.bprintf(PRINT_HIGH, "%s disconnected\n", ent->client->pers.netname);
    }

    // The flag and the tech stay in the world.  No-ops when the player holds
    // neither, so no gate.
    CTFDeadDropFlag(ent);
    CTFDeadDropTech(ent);

    // R-MENU-3: a client who quits with a menu open must not leave the handle
    // behind it.  Threewave leaks it.
    G_MenuClose(ent);

//============
//ROGUE
    // make sure no trackers are still hurting us.
    if (ent->client->tracker_pain_framenum)
        RemoveAttackingPainDaemons(ent);

    if (ent->client->owned_sphere) {
        if (ent->client->owned_sphere->inuse)
            G_FreeEdict(ent->client->owned_sphere);
        ent->client->owned_sphere = NULL;
    }

    if (gamerules && gamerules->value) {
        if (DMGame.PlayerDisconnect)
            DMGame.PlayerDisconnect(ent);
    }
//ROGUE
//============

    // send effect
    if (ent->inuse) {
        gi.WriteByte(svc_muzzleflash);
        gi.WriteShort(ent - g_edicts);
        gi.WriteByte(MZ_LOGOUT);
        gi.multicast(ent->s.origin, MULTICAST_PVS);
    }

    gi.unlinkentity(ent);
    ent->s.modelindex = 0;
    ent->s.modelindex2 = 0;
    ent->s.sound = 0;
    ent->s.event = 0;
    ent->s.effects = 0;
    ent->s.renderfx = 0;
    ent->s.solid = 0;
    ent->solid = SOLID_NOT;

    // RA2 unlinks the player from its arena team here, in exactly this place --
    // after the entity is stripped and before `inuse` goes false, because
    // check_teams() walks the roster and a half-torn-down member must already
    // be off it.  The merge dropped the call, so `remove_from_team` had NO
    // caller in the tree and every client that left an arena stayed on its
    // team's member list.
    //
    // Invisible until Phase 7, and then immediate: nothing ever put a client on
    // an arena team before, because InitClientResp left `teamnum` at 0 and
    // PutClientInServer therefore never ran init_player().  With bots joining,
    // `sv removebot all` left sixteen dead members linked into two pickup teams
    // and UpdateStatusBars dereferenced the first of them on the next frame:
    //
    //   UpdateStatusBars (arenanum=1) at src/arena/arena.c:1497
    //   arena_think / multi_arena_think / RA_CheckRules / G_RunFrame
    //
    // `resp.entered = false` is the donor's next line and is NOT carried: its
    // only reader in RA2 is a "reconnect without disconnect" arm in
    // ClientConnect that this tree does not have, and the field is shared with
    // tourney's four-state enum (R-58), whose owner clears it its own way.
    if (G_Ruleset() == RULESET_ARENA)
        remove_from_team(ent);

    ent->inuse = false;
    ent->classname = "disconnected";
    ent->client->pers.connected = false;

    // R-OSP-1: the half that needs the slot already free -- the recount, the
    // recover seat, the chase cams that were watching this player, and the
    // pause that waits for the last member of a team to come back.
    if (G_Ruleset() == RULESET_TOURNEY && OSP_clientLeft(ent, osp_team))
        return;

    // FIXME: don't break skins on corpses, etc
    //playernum = ent-g_edicts-1;
    //gi.configstring (CS_PLAYERSKINS+playernum, "");
}

//==============================================================

static edict_t  *pm_passent;
static int      pm_clipmask;

// pmove doesn't need to know about passent and contentmask
#if USE_NEW_GAME_API
static trace_t q_gameabi PM_trace(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int contentmask)
{
    return gi.trace(start, mins, maxs, end, pm_passent, (game.csr.extended && contentmask) ? contentmask : pm_clipmask);
}
#else
static trace_t q_gameabi PM_trace(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end)
{
    return gi.trace(start, mins, maxs, end, pm_passent, pm_clipmask);
}
#endif

/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame.
==============
*/
void ClientThink(edict_t *ent, usercmd_t *ucmd)
{
    gclient_t   *client;
    edict_t *other;
    int     i, j;
    pmove_t pm;

    level.current_entity = ent;
    client = ent->client;

    if (level.intermission_framenum) {
        client->ps.pmove.pm_type = PM_FREEZE;
        // can exit intermission after five seconds
        if (level.framenum > level.intermission_framenum + 5.0f * BASE_FRAMERATE
            && (ucmd->buttons & BUTTON_ANY))
            level.exitintermission = true;
        return;
    }

    // R-MENU-4/R-BOT-28: the Gladiator menu is driven by the movement axes, so
    // it has to see the usercmd BEFORE pmove does and it edits the command it
    // was given -- clearing the movement it consumed, the attack and use
    // buttons, and the gravity, so the cursor does not also walk the player off
    // a ledge.  Ahead of tourney's autocam for the same reason menu input is
    // consumed first everywhere else: whichever menu is open owns the keys.
    if (ent->client->menu_owner == MENU_BOT)
        bot_MenuThink(ent, ucmd);

    // R-OSP-1/R-EXTRA-6: tourney's autocam is not a chase cam -- it picks its
    // own subject and its own position -- so it takes the whole frame, before
    // either the chase-cam branch or pmove.
    if (G_Ruleset() == RULESET_TOURNEY && OSP_clientThink(ent, ucmd))
        return;

    if (ent->client->chase_target) {
        client->resp.cmd_angles[0] = SHORT2ANGLE(ucmd->angles[0]);
        client->resp.cmd_angles[1] = SHORT2ANGLE(ucmd->angles[1]);
        client->resp.cmd_angles[2] = SHORT2ANGLE(ucmd->angles[2]);
    } else {
        // set up for pmove
        memset(&pm, 0, sizeof(pm));

        if (ent->movetype == MOVETYPE_NOCLIP)
            client->ps.pmove.pm_type = PM_SPECTATOR;
        else if (ent->s.modelindex != MODELINDEX_PLAYER)
            client->ps.pmove.pm_type = PM_GIB;
        else if (ent->deadflag)
            client->ps.pmove.pm_type = PM_DEAD;
        else
            client->ps.pmove.pm_type = PM_NORMAL;

        pm_passent = ent;
        if (ent->health > 0)
            pm_clipmask = MASK_PLAYERSOLID;
        else
            pm_clipmask = MASK_DEADSOLID;

        //PGM   trigger_gravity support
        //  client->ps.pmove.gravity = sv_gravity->value;
        client->ps.pmove.gravity = sv_gravity->value * ent->gravity;
        //PGM

        if (G_Ruleset() == RULESET_ARENA) {
            RA_ZBotSample(ent, ucmd);

            if (client->resp.track_target &&
                client->resp.fightstate == FIGHT_SPECTATING) {
                // TRACKCAM and EYECAM drive the view from the tracked player,
                // so this client's own movement must not (R-EXTRA-6).
                client->ps.pmove.pm_type = PM_FREEZE;
                client->ps.pmove.gravity = 0;

                if (client->resp.omode == OMODE_TRACKCAM)
                    track_think(ent, ucmd);
                else if (client->resp.omode == OMODE_EYECAM)
                    eyecam_think(ent, ucmd);
            }
        }
        pm.s = client->ps.pmove;

        for (i = 0; i < 3; i++) {
            pm.s.origin[i] = COORD2SHORT(ent->s.origin[i]);
            pm.s.velocity[i] = COORD2SHORT(ent->velocity[i]);
        }

        if (memcmp(&client->old_pmove, &pm.s, sizeof(pm.s))) {
            pm.snapinitial = true;
            //      gi.dprintf ("pmove changed!\n");
        }

        // R-OSP-4: the aimbot detector, which watches the view-angle deltas in
        // the usercmd for the signature a ZBOT leaves.  It returns true when it
        // has KICKED the client, and the frame ends there rather than running a
        // pmove for an edict that is on its way out -- the corrupt unicast
        // R-OSP-4 names came from doing it the other way round.
        if (G_Ruleset() == RULESET_TOURNEY && bot_watch &&
            !(ent->flags & FL_BOT) && OSP_botDetect(ent, ucmd))
            return;

        pm.cmd = *ucmd;

        pm.trace = PM_trace;    // adds default parms
        pm.pointcontents = gi.pointcontents;

        // perform a pmove
        gi.Pmove(&pm);

        for (i = 0; i < 3; i++) {
            ent->s.origin[i] = SHORT2COORD(pm.s.origin[i]);
            ent->velocity[i] = SHORT2COORD(pm.s.velocity[i]);
        }

        VectorCopy(pm.mins, ent->mins);
        VectorCopy(pm.maxs, ent->maxs);

        client->resp.cmd_angles[0] = SHORT2ANGLE(ucmd->angles[0]);
        client->resp.cmd_angles[1] = SHORT2ANGLE(ucmd->angles[1]);
        client->resp.cmd_angles[2] = SHORT2ANGLE(ucmd->angles[2]);

        if (~client->ps.pmove.pm_flags & pm.s.pm_flags & PMF_JUMP_HELD && pm.waterlevel == 0) {
            gi.sound(ent, CHAN_VOICE, gi.soundindex("*jump1.wav"), 1, ATTN_NORM, 0);
            PlayerNoise(ent, ent->s.origin, PNOISE_SELF);
        }

        // save results of pmove
        client->ps.pmove = pm.s;
        client->old_pmove = pm.s;

        //ROGUE sam raimi cam support
        if (ent->flags & FL_SAM_RAIMI)
            ent->viewheight = 8;
        else
            ent->viewheight = pm.viewheight;
        //ROGUE

        ent->waterlevel = pm.waterlevel;
        ent->watertype = pm.watertype;
        ent->groundentity = pm.groundentity;
        if (pm.groundentity)
            ent->groundentity_linkcount = pm.groundentity->linkcount;

        if (ent->deadflag) {
            client->ps.viewangles[ROLL] = 40;
            client->ps.viewangles[PITCH] = -15;
            client->ps.viewangles[YAW] = client->killer_yaw;
        } else {
            VectorCopy(pm.viewangles, client->v_angle);
            VectorCopy(pm.viewangles, client->ps.viewangles);
        }

        // The grapple pulls from the post-pmove position, so it runs here
        // rather than as an entity think.  NULL for everyone who is not on one.
        //
        // ONE field, TWO pulls.  Threewave's hook and tourney's are the same
        // mechanic with different physics -- tourney's is faster, does its own
        // damage and lets go on its own timer -- so sec 7 rule 6 keeps one
        // `ctf_grapple` edict and the ruleset chooses which one moves the
        // player on it.  A single pull with tunables would have to reconcile
        // two different state machines, which is the merit choice R-50 refused.
        if (client->ctf_grapple) {
            if (G_Ruleset() == RULESET_TOURNEY)
                GrapplePull(client->ctf_grapple);
            else
                CTFGrapplePull(client->ctf_grapple);
        }

        gi.linkentity(ent);

        //PGM trigger_gravity support
        ent->gravity = 1.0f;
        //PGM
        if (ent->movetype != MOVETYPE_NOCLIP)
            G_TouchTriggers(ent);

        // touch other objects
        for (i = 0; i < pm.numtouch; i++) {
            other = pm.touchents[i];
            for (j = 0; j < i; j++)
                if (pm.touchents[j] == other)
                    break;
            if (j != i)
                continue;   // duplicated
            if (!other->touch)
                continue;
            other->touch(other, ent, NULL, NULL);
        }
    }

    client->oldbuttons = client->buttons;
    client->buttons = ucmd->buttons;
    client->latched_buttons |= client->buttons & ~client->oldbuttons;

    // save light level the player is standing on for
    // monster sighting AI
    ent->light_level = ucmd->lightlevel;

    if (G_Ruleset() == RULESET_ARENA) {
        if (client->resp.fightstate == FIGHT_SPECTATING &&
            (client->resp.omode == OMODE_TRACKCAM ||
             client->resp.omode == OMODE_EYECAM) && ucmd->upmove != 0) {
            // jump/crouch cycles who you are watching; the latch stops one
            // press cycling once per frame.
            if (!(client->resp.omode_buttons & 2)) {
                if (ucmd->upmove > 0)
                    track_next(ent);
                else
                    track_prev(ent);
                client->resp.omode_buttons |= 2;
            }
        } else if (ucmd->upmove == 0) {
            client->resp.omode_buttons &= ~2;
        }
    }

    // fire weapon from final position if needed
    if (client->latched_buttons & BUTTON_ATTACK) {
        // G_IsObserver, not resp.spectator: under ctf an observer is a
        // CTF_NOTEAM player, so the inherited test would let him shoot
        // (R-CTF-3's "observers cannot fire it", R-CTF-5).
        if (G_IsObserver(ent) && G_Ruleset() != RULESET_ARENA) {
            client->latched_buttons = 0;

            if (client->chase_target) {
                client->chase_target = NULL;
                client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
            } else
                GetChaseTarget(ent);
        } else if (!client->weapon_thunk) {
            client->weapon_thunk = true;
            Think_Weapon(ent);
        }
    }

    if (G_IsObserver(ent) && G_Ruleset() != RULESET_ARENA) {
        if (ucmd->upmove >= 10) {
            if (!(client->ps.pmove.pm_flags & PMF_JUMP_HELD)) {
                client->ps.pmove.pm_flags |= PMF_JUMP_HELD;
                if (client->chase_target)
                    ChaseNext(ent);
                else
                    GetChaseTarget(ent);
            }
        } else
            client->ps.pmove.pm_flags &= ~PMF_JUMP_HELD;
    }

    // CTF regeneration tech.  A no-op without it.
    CTFApplyRegeneration(ent);

    // Tourney's regeneration rune is the same concept on the same frame.
    if (G_Ruleset() == RULESET_TOURNEY && (rune_stat & RUNE_REGEN))
        OSP_runesApplyRegeneration(ent);

    // R-CTF-3's offhand hook fires from here rather than from a weapon think,
    // which is what makes it offhand.
    CTFHookThink(ent);

    // update chase cam if being followed
    for (i = 1; i <= game.maxclients; i++) {
        other = g_edicts + i;
        if (other->inuse && other->client->chase_target == ent)
            UpdateChaseCam(other);
    }

    // R-MENU-4: a menu redraw this frame's input earned, rate-limited by the
    // engine rather than by how fast the player presses the key.
    if (client->menudirty && client->menutime <= level.time) {
        if (G_MenuActive(ent) && client->menu_owner == MENU_CTF) {
            ctf_PMenu_Do_Update(ent);
            gi.unicast(ent, true);
        }
        client->menutime = level.time;
        client->menudirty = false;
    }
}

/*
==============
ClientBeginServerFrame

This will be called once for each server frame, before running
any other entities in the world.
==============
*/
void ClientBeginServerFrame(edict_t *ent)
{
    gclient_t   *client;
    int         buttonMask;

    if (level.intermission_framenum)
        return;

    client = ent->client;

    // R-OSP-4: the speed-cheat detector, which samples what the client claims
    // it moved against what the server let it move.  `pers.spectator` is
    // tourney's strike counter for it and NOT baseq2's spectator flag -- the
    // two share a name in the merged struct and the first strike would
    // otherwise have made the player a spectator (SPECS.md 1.19, R-58).
    if (G_Ruleset() == RULESET_TOURNEY) {
        OSP_speedDetect(ent);
    } else if (deathmatch->value &&

        client->pers.spectator != client->resp.spectator &&

        (level.framenum - client->respawn_framenum) >= 5 * BASE_FRAMERATE) {

        spectator_respawn(ent);

        return;

    }

    // run weapon animations if it hasn't been done by a ucmd_t
    if (!client->weapon_thunk && !G_IsObserver(ent))
        Think_Weapon(ent);
    else
        client->weapon_thunk = false;

    if (ent->deadflag) {
        // wait for any button just going down
        if (level.framenum > client->respawn_framenum) {
            // in deathmatch, only wait for attack button
            if (deathmatch->value)
                buttonMask = BUTTON_ATTACK;
            else
                buttonMask = -1;

            if ((client->latched_buttons & buttonMask) ||
                (deathmatch->value && ((int)dmflags->value & DF_FORCE_RESPAWN)) ||
                CTFMatchOn() || G_Ruleset() == RULESET_ARENA) {
                respawn(ent);
                client->latched_buttons = 0;
            }
        }
        return;
    }

    // add player trail so monsters can follow
    if (!deathmatch->value)
        if (!visible(ent, PlayerTrail_LastSpot()))
            PlayerTrail_Add(ent->s.old_origin);

    client->latched_buttons = 0;
}

/*
==============
RemoveAttackingPainDaemons

This is called to clean up the pain daemons that the disruptor attaches
to clients to damage them.
==============
*/
void RemoveAttackingPainDaemons(edict_t *self)
{
    edict_t *tracker;

    tracker = G_Find(NULL, FOFS(classname), "pain daemon");
    while (tracker) {
        if (tracker->enemy == self)
            G_FreeEdict(tracker);
        tracker = G_Find(tracker, FOFS(classname), "pain daemon");
    }

    if (self->client)
        self->client->tracker_pain_framenum = 0;
}
