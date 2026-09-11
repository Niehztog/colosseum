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
#include "bot/p_observer.h"
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

// Non-static: CTF's ghost and match code report obituaries too.
void ClientObituary(edict_t *self, edict_t *inflictor, edict_t *attacker)
{
    int         mod;
    char        *message;
    char        *message2;
    int         ff;

    // RA_Obituary() is RA2's whole obituary minus the text -- the round
    // statistics, the announcer and every point that changes hands.  So the
    // three `resp.score` sites below are somebody else's under arena and are
    // gated off one by one rather than here, because the printing between them
    // is still wanted and is what this function is being kept for.
    if (G_Ruleset() == RULESET_ARENA)
        RA_Obituary(self, inflictor, attacker);

    // Once sudden death has produced a winner the match is decided and
    // the rules row is about to end the level, so nothing further is announced.
    // The donor's own first statement.
    if (G_IsOspRuleset() && OSP_obituaryHush())
        return;

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
            // Tourney prints this per client rather than broadcasting
            // it, refuses the score change during the countdown, and charges
            // the death to the team as well as to the player.
            if (G_IsOspRuleset()) {
                OSP_obituarySelf(self, message);
            } else {
                gi.bprintf(PRINT_MEDIUM, "%s %s.\n", self->client->pers.netname, message);
                // RA_Obituary() has already charged this death, and the
                // arena decides whether it costs anything at all
                // (`scorebydamage`).
                if (deathmatch->value && G_Ruleset() != RULESET_ARENA)
                    self->client->resp.score--;
            }
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
                // Two donors claim one means of death, and this tree used
                // Threewave's under every ruleset -- so a hook kill in `tdm`
                // was announced as CTF's, while the stats log for the same
                // kill already said tourney's ("Hook", osp_stats.c:760).
                // Threewave: "X was caught by Y's grapple"
                // (`port_ctf:p_client.c:344`).  Tourney: "X was hooked to
                // death by Y", with no second half at all
                // (`port_osp:p_client.c:316`).  Section 7 rule 4 gives each
                // donor its own words where the rulesets do not share a text.
                if (G_IsOspRuleset()) {
                    message = "was hooked to death by";
                } else {
                    message = "was caught by";
                    message2 = "'s grapple";
                }
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
                // Tourney again, and this is the shape that carries the whole
                // team ledger: the frag or the team kill, the victim's death,
                // the fraglimit trigger, and both teams' totals.
                if (G_IsOspRuleset()) {
                    OSP_obituaryFrag(self, attacker, message, message2, ff != 0);
                    return;
                }

                gi.bprintf(PRINT_MEDIUM, "%s %s %s%s\n", self->client->pers.netname, message, attacker->client->pers.netname, message2);
//ROGUE
                if (G_UsesRogueGameRules()) {
                    if (DMGame.Score) {
                        if (ff)
                            DMGame.Score(attacker, self, -1);
                        else
                            DMGame.Score(attacker, self, 1);
                    }
                    return;
                }
//ROGUE

                // RA_Obituary() has already paid this frag, and it pays
                // a team kill with `OnSameTeam` rather than with `ff` -- which
                // is MOD_FRIENDLY_FIRE, which T_Damage only raises when dmflags
                // carries the team bits, which arena never sets.  Left here, a
                // team kill scored +1.
                if (deathmatch->value && G_Ruleset() != RULESET_ARENA) {
                    if (ff)
                        attacker->client->resp.score--;
                    else
                        attacker->client->resp.score++;
                }
                return;
            }
        }
    }

//ROGUE
//  if (g_showlogic && g_showlogic->value)
//  {
//      if (mod == MOD_UNKNOWN)
//          gi.dprintf ("Player killed by MOD_UNKNOWN\n");
//      else
//          gi.dprintf ("Player killed by undefined mod %d\n", mod);
//  }
//ROGUE

    // Tourney's third shape: a death the tables could not name.  It owns the
    // announcement too, because tourney suppresses it during the countdown.
    if (G_IsOspRuleset()) {
        OSP_obituaryDied(self);
        return;
    }

    gi.bprintf(PRINT_MEDIUM, "%s died.\n", self->client->pers.netname);

    // The third and last of them -- a death with no attacker the switch
    // above could name.  RA_Obituary() charged it on the way in.
    if (deathmatch->value && G_Ruleset() != RULESET_ARENA)
//ROGUE
    {
        if (G_UsesRogueGameRules()) {
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

    // The donor closes the quad's pickup record here, in both arms,
    // and this tree closed it in neither (`port_osp:p_client.c:513` and `:543`).
    //
    // This arm is the last chance the log gets.  player_die zeroes
    // `quad_framenum` a few lines further down, and p_view.c's per-frame expiry
    // test needs a NON-zero one -- so whatever is not written here is never
    // written at all.
    if (!((int)(dmflags->value) & DF_QUAD_DROP)) {
        // One token of this is not the donor's, and it is `quad_framenum &&`.
        //
        // The donor's guard is `osp_r200 && quad_framenum < level.framenum`,
        // and `osp_r200` is set by the INVULNERABILITY pickup as well as the
        // quad -- in the donor too (`port_osp:g_items.c:183` and `:187`).  So
        // the donor's arm has three reachable cases and only one of them is the
        // event it names:
        //
        //   held a quad that ran out this frame -- p_view.c has not seen it
        //   yet, `quad_framenum` is still set, and player_die zeroes it below,
        //   so this is the only place the expiry can ever be written.  Right,
        //   and the case the arm exists for.
        //
        //   held a quad that ran out EARLIER while also holding invulnerability
        //   -- p_view.c logged the expiry and zeroed `quad_framenum`, but kept
        //   `osp_r200` for the second powerup.  The donor logs the same expiry
        //   a second time, same entity.
        //
        //   never held a quad at all, only invulnerability -- `osp_r200` is the
        //   INVULNERABILITY's entity and `quad_framenum` is 0, which is less
        //   than any frame.  The donor writes a "Quad" expiry for it.
        //
        // p_view.c's own reader tests `quad_framenum &&` before the comparison
        // for exactly this reason.  Adding that conjunct here selects the first
        // case and excludes the other two; nothing correct is lost, because a
        // still-running quad fails `< level.framenum` in both versions.
        if (G_IsOspRuleset() && self->client->resp.osp_r200 &&
            self->client->quad_framenum &&
            self->client->quad_framenum < level.framenum) {
            OSP_Stats_ItemExpire("Quad", self, self->client->resp.osp_r200);
            self->client->resp.osp_r200 = 0;
        }
        quad = false;
    } else
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

        // The other arm: a quad dropped on death is a drop event, at the
        // moment of death, naming the edict it became -- which is the entity
        // OSP_itemFreed will later expire when the dropped quad times out on
        // the ground (osp_main.c:1299).  The two events are the same object
        // seen twice, not a double log.
        //
        // The clear is unconditional, as the donor's is, even though this
        // tree's `osp_r200` is shared with Invulnerability and p_view.c clears
        // it only once both are gone: player_die zeroes `invincible_framenum`
        // below without an expiry event either way, so there is no second
        // reader left to keep it for.
        if (G_IsOspRuleset()) {
            OSP_Stats_ItemDrop("Quad", drop - g_edicts, self);
            self->client->resp.osp_r200 = 0;
        }

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

        // Both logs record the death, and only while a match is live
        // -- a warmup death is not a statistic.
        if (G_IsOspRuleset()) {
            if (sync_stat > 2)
                sl_WriteStdLogDeath(&gi, level, self, inflictor, attacker);
            OSP_Stats_Death(self, inflictor, attacker);
        }

        // `client_deathweapdrop` decides whether a tourney player drops the
        // weapon they were holding; everywhere else it is unconditional --
        // except under arena, which does not drop weapons at all.  RA2's own
        // TossClientWeapon is `static q_unused`, and the reconstruction's note
        // against the shipped DLL is "no real counterpart -- confirmed dead
        // code": id's function survives in the object and nothing reaches it.
        // That is not an oversight there.  An arena hands out a fixed loadout
        // at the start of the round and frees every pickup item on the map, so
        // a dropped weapon is an item in a ruleset that has none.
        if (G_Ruleset() == RULESET_ARENA) {
            // nothing: the body keeps what it was holding
        } else if (!G_IsOspRuleset() ||
                   (int)client_deathweapdrop->value) {
            TossClientWeapon(self);
        }

        // ...and the rune goes with the body, into the rune pool.
        if (G_IsOspRuleset() && rune_stat)
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
        if (G_IsOspRuleset()) {
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

    if (G_UsesRogueGameRules()) {
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
            //
            // Silent at `sync_stat == 2`, which is the frame OSP kills
            // everybody into a fresh spawn at the start of a match
            // (osp_main.c:2456, `Cmd_Kill_f(ent)` per client).  The donor
            // guards both death sounds with this and nothing else
            // (`port_osp:p_client.c:642` and `:672`), so a tourney match starts
            // on the countdown's last tick and not on a chorus of eight death
            // screams -- which is what the merge shipped, because the guard is
            // an added line around a call that was already here and no
            // line-level or call-level sweep can see one of those.
            if (!(G_IsOspRuleset() && sync_stat == 2))
                gi.sound(self, CHAN_BODY, gi.soundindex("misc/udeath.wav"), 1, ATTN_NORM, 0);

            // more meaty gibs for your dollar!
            if ((deathmatch->value) && (self->health < -80)) {
                for (n = 0; n < 4; n++)
                    ThrowGib(self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
            }

            // `numgibs` is what the count is under tourney -- a
            // registered, documented cvar (default 4, which is why nothing
            // looked wrong) whose value nothing read.  The extra -80 loop above
            // is baseq2's and stays baseq2's: the donor has one loop, and
            // deleting the other under tourney would take gibs away rather than
            // give the cvar its meaning.
            for (n = 0; n < (G_IsOspRuleset() ? OSP_GibCount() : 4); n++)
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
            // The other half of the same guard.  This is the arm the
            // match start actually takes: Cmd_Kill_f sets `health = 0`, which
            // is not below -40, so every client dies the ordinary way and
            // screams.  The gib arm above is guarded too because the donor
            // guards it, not because the match start reaches it.
            if (!(G_IsOspRuleset() && sync_stat == 2))
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
// Non-static: CTF re-initialises a client on a team change.
// `full` is OSP's: a partial reset keeps the client's identity --
// userinfo, netname and the referee "green name" -- and clears everything else,
// which is what a team change or a match reset wants.
//
// The donor computes it as `sizeof(userinfo) + sizeof(netname) +
// sizeof(greenname)` and memsets from that offset onward, which is only correct
// while those three are the first members of client_persistant_t.  In this tree
// they are not: the merged union put Ground Zero's ammo maxima and CTF's fields
// in between, so the donor's arithmetic would clear the identity it means to
// keep and preserve three unrelated ints.  Saved and restored by name instead.
void InitClientPersistant(gclient_t *client, bool full)
{
    const gitem_t   *item;
    char            userinfo[MAX_INFO_STRING];
    char            netname[16];
    char            greenname[16];
    bool            keep_motd = false;
    bool            keep_listenhost;
    char            keep_address[MAX_CLIENT_ADDRESS];

//  gi.dprintf("InitClientPersistant()\n");

    if (!full) {
        memcpy(userinfo, client->pers.userinfo, sizeof(userinfo));
        memcpy(netname, client->pers.netname, sizeof(netname));
        memcpy(greenname, client->pers.greenname, sizeof(greenname));
    }

    // The motd flag survives the wipe on both arms, which is the donor's own
    // shape (`rocketarena2-public/p_client.c` saves and restores it around the
    // memset unconditionally).  ClientConnect sets it once and init_player()
    // consumes it; anything that re-initialises `pers` in between -- and
    // PutClientInServer does, whenever `pers.health <= 0` -- would otherwise
    // eat the motd before it was ever drawn.
    if (G_Ruleset() == RULESET_ARENA)
        keep_motd = client->pers.showmotd;

    // And the listen server's host survives the wipe on both arms and
    // under every ruleset, which the motd deliberately does not.  Who is sitting
    // at the server's own console is a property of the CONNECTION, not of a
    // life: ClientConnect is the only writer, every spawn passes through here,
    // and PutClientInServer calls this on the deathmatch arm of every single
    // one -- so a wipe would mean the host lost the exemption the first time
    // they spawned and never got it back until the next map.
    keep_listenhost = client->pers.listenhost;

    // ...and so does where they connected from, for exactly the same reason
    // and on both arms: ClientConnect is the only writer, and every spawn
    // comes through here.  A wipe would leave the disconnect record with an
    // empty address for anybody who had respawned once (R-LOG-1).
    memcpy(keep_address, client->pers.address, sizeof(keep_address));

    memset(&client->pers, 0, sizeof(client->pers));

    if (G_Ruleset() == RULESET_ARENA)
        client->pers.showmotd = keep_motd;

    client->pers.listenhost = keep_listenhost;
    memcpy(client->pers.address, keep_address, sizeof(client->pers.address));

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
    // dm and sp the item exists in the merged itemlist but nobody
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
    // #defines to 1, so it was already dead in the donor.
//ROGUE

    // Tourney seeds its own per-player state here -- the ammo ceilings
    // a referee has set, the accuracy row, the rune counters -- which is the
    // one place that runs on both a fresh connect and a respawn.
    if (G_IsOspRuleset())
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

    // And one outlives it under tourney, for a different reason: `clientid` is
    // an INDEX handed out once per connection, not per life.  p_acc[] (the
    // accuracy table), saved_clients[] (the client_recover seat) and every
    // command that names a player by number are addressed through it, and
    // OSP_giveClientID() runs in ClientBegin -- before ClientBeginDeathmatch
    // gets here.  Letting the memset take it gave every client id 0 and one
    // shared row of each table.  The donor's InitClientResp carries it across
    // its own memset for exactly this reason.
    int clientid = client->resp.clientid;

    memset(&client->resp, 0, sizeof(client->resp));

    client->resp.ctf_team = ctf_team;
    client->resp.id_state = id_state;

    if (G_IsOspRuleset()) {
        client->resp.clientid = clientid;
        // 2 is "no team"; 0 and 1 are the two teams, so the zero the memset
        // leaves would read as membership of team A.
        client->resp.team = 2;
        // The per-client statusbar variant starts at the server's default
        // rather than at zero -- the composed bar in g_stats.c is built from
        // the same cvar, so a client that started at 0 on a `client_hud 1`
        // server had its first `hud` press move it AWAY from what it was
        // already looking at.
        client->resp.osp_r00c = client_hud ? (int)client_hud->value : 0;
        // Which countdown step this client has already been told about, so the
        // announcer does not replay the whole countdown on a respawn.
        client->resp.osp_r01c = start_count;
    }

    client->resp.enterframe = level.framenum;
    client->resp.coop_respawn = client->pers;

    if (G_Ruleset() == RULESET_CTF && client->resp.ctf_team < CTF_TEAM1)
        CTFAssignTeam(client);

    // RA2 adds ten assignments after its own memset and the merge kept
    // none of them, because every one is a statement the base does not have and
    // rule 2 takes the base where the base is unchanged.  Seven are what the
    // memset already gives (`context` 0, `fightstate` FIGHT_SPECTATING,
    // `omode` OMODE_NORMAL, `teammember` NULL, `zbotcount`, `zbotlastcheck`,
    // `damagedealt`), which is why nothing noticed.  Three are not:
    //
    //   * `teamnum` must be -1, because 0 is a valid team index and
    //     PutClientInServer branches on `>= 0`.  Measured: every client
    //     connecting under `arena` arrived with teamnum 0, took reinit_player()
    //     instead of init_player(), and so was never given the team menu, never
    //     had its menu queue reset for a reused slot, and claimed membership of
    //     whatever team happens to be in slot 0 -- which on a deathmatch map is
    //     "#N Pickup Red", allocated first by arena_init(), so "arena menu on
    //     connect" has been structurally false ever since.
    //   * `ra_votes` must be votetries_setting, or the client starts with zero
    //     votes and menuVote refuses every one.  arena.c resets it per arena
    //     config change, which is a top-up and not the initial grant.
    //   * `isbot` is not the memset's zero.  RA2 writes it twice -- a zero and
    //     then a CONDITIONAL seed from `zbotscore`, the port the client
    //     connected from, which ClientConnect stamps.  Reading it as the
    //     redundant half cost RA2's ZBot detection its connect-time arm and
    //     left `zbotscore` written nowhere in the tree; the runtime arm
    //     (RA_ZBotSample) had been carried all along.
    //
    // The other seven stay unwritten: a redundant assignment is a claim that
    // the memset above might not do it, and the merged union makes that claim
    // false for a field another ruleset owns.
    if (G_Ruleset() == RULESET_ARENA) {
        client->resp.teamnum = -1;
        client->resp.ra_votes = votetries_setting;
        if (client->zbotscore == RA_ZBOT_PORT)
            client->resp.isbot = 1;
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
// `ent` is the client being PLACED, or NULL where there is none --
// the two exclusions below need to know which body not to measure against, and
// a caller asking "how close is the nearest player to this spot" in the
// abstract (dm_ball's ball spawn, the coop lava scan) has no such body.
//
// The donor threads it all the way down -- SelectDeathmatchSpawnPoint(ent) into
// both selectors into PlayersRangeFromSpot(spot, ent) -- and skips
// `player == ent` and `resp.entered != ENTERED_ENTERED`.  The same
// two into OSP_spawnRefused, for the 60-unit refusal one call further down, and
// argued they are "load-bearing rather than tidy"; the argument is unchanged
// one call up, where it decides which spot instead of whether to take it.
// With DF_SPAWN_FARTHEST under the OSP four, "farthest from any player" was
// measured against the body of the client being placed -- so a client joining
// from the queue was sent as far as possible from where it had been watching --
// and against every observer that has one, so an observer parked on a point
// moved everybody off it.
//
// BEHIND G_IsOspRuleset(), and that is not tidiness either: `osp_entered` is
// only maintained by the OSP four -- connecting is not entering -- so
// applying the second test anywhere else would exclude every player on the
// server and hand every spot the same 9999999.  Section 7 rule 1 keeps q2pro's
// behaviour where a donor has no feature, and the self-exclusion is inside the
// same gate for that reason rather than because it is wrong elsewhere.
float PlayersRangeFromSpot(edict_t *spot, edict_t *ent)
{
    edict_t *player;
    float   bestplayerdistance;
    vec3_t  v;
    int     n;
    float   playerdistance;
    bool    osp = ent && G_IsOspRuleset();

    bestplayerdistance = 9999999;

    for (n = 1; n <= game.maxclients; n++) {
        player = &g_edicts[n];

        if (!player->inuse || !player->client)
            continue;

        if (osp && (player == ent ||
                    player->client->resp.osp_entered != ENTERED_ENTERED))
            continue;

        if (player->health <= 0)
            continue;

        // An arena observer is alive, noclipping and usually parked over the
        // spawn points; counting it makes the "farthest from any player" spawn
        // choose by where the audience is, which 1999 fixed.  baseq2's own
        // spectators have the same quirk and keep it: q2pro has not fixed it,
        // and q2pro wins on behaviour that is not a donor's feature.
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
edict_t *SelectRandomDeathmatchSpawnPoint(edict_t *ent)
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
        range = PlayersRangeFromSpot(spot, ent);
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
edict_t *SelectFarthestDeathmatchSpawnPoint(edict_t *ent)
{
    edict_t *bestspot;
    float   bestdistance, bestplayerdistance;
    edict_t *spot;

    spot = NULL;
    bestspot = NULL;
    bestdistance = 0;
    while ((spot = G_Find(spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
        bestplayerdistance = PlayersRangeFromSpot(spot, ent);

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

/*
=================
`botfill` under `dm`/`dmpro` -- the count follows the map, not the server

The same argument as the arena fill, one ruleset over.  `minimumplayers` is one number
and a server plays a rotation: eight is a full house on `q2dm1` and four more
bodies than `q2dm7` has anywhere to put.  Deathmatch declares no capacity
anywhere -- there is no `arena.cfg` and no `team_maxplayers` -- so the MAP is the
only signal there is, and the spawn points are the map saying how many people it
was built for.

The number is `G_SpawnPointPool`'s and not the raw count, because the selector
refuses the two spots nearest a player.  Measured over the eight `q2dm` maps:
**8, 5, 5, 9, 7, 6, 4, 4**, against raw counts of 10, 7, 7, 11, 9, 8, 6, 6 --
and the first row is the number each of those maps is actually played at.

`DF_SPAWN_FARTHEST` is the other selector and it does use every spot, so under
that flag the map has two more seats than this reports.  They are not given
back, and that is a decision: `dmflags` is not latched, so a target that moved
with the flag would add two bots on the write and remove them again on the next
one.  Two seats short is a quieter answer than a server that twitches.

Under `teamplay` the target is rounded down to even, because two teams that
cannot be the same size is the one thing a fill is able to get wrong for free.

`0` is off and nothing here runs.  The ceilings -- `game.maxclients` and the
roster in `bots.cfg` -- are the caller's, in `bl_spawn.c`, so that every fill
settles on one number the same way.
=================
*/
int DM_BotFillSeats(void)
{
    int want = G_SpawnPointPool("info_player_deathmatch");

    if (G_TeamplayEnabled())
        want &= ~1;

    return want;
}

static edict_t *SelectDeathmatchSpawnPoint(edict_t *ent)
{
    if ((int)(dmflags->value) & DF_SPAWN_FARTHEST)
        return SelectFarthestDeathmatchSpawnPoint(ent);
    else
        return SelectRandomDeathmatchSpawnPoint(ent);
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

        if (PlayersRangeFromSpot(spawnPoints[index], NULL) > 32) {
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
bool SelectSpawnPoint(edict_t *ent, vec3_t origin, vec3_t angles)
{
    edict_t *spot = NULL;

    if (deathmatch->value)
        spot = SelectDeathmatchSpawnPoint(ent);
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

    // And tourney may REFUSE the spot it just chose.  The donor's own
    // last two lines of this function: a spawn with a player within 60 units of
    // it is not taken, and PutClientInServer leaves the client frozen and
    // bodiless instead of telefragging whoever is standing there; the respawn
    // trigger is what tries again.  Nothing is written to `origin` on a
    // refusal, because the caller does not place anybody.
    if (G_IsOspRuleset() && OSP_spawnRefused(ent, spot))
        return false;

    VectorCopy(spot->s.origin, origin);
    VectorCopy(spot->s.angles, angles);
    return true;
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
        for (n = 0; n < (G_IsOspRuleset() ? OSP_GibCount() : 4); n++)
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

    // The dead player points at its own corpse, and the observer's
    // death camera follows that link -- Cam_DeathThink walks `goalentity` from
    // the body it was watching to the player who left it, so a viewer whose
    // subject dies keeps watching rather than dropping to idle.
    if (ent->client)
        ent->goalentity = body;

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

        // hold in place briefly -- but only for a body that walks.
        //
        // PMF_TIME_TELEPORT is not a flag that expires on its own: Pmove()
        // clears it from the `pm_time` countdown, and that countdown sits
        // BELOW the `if (pm_type == PM_SPECTATOR) { PM_FlyMove(); return; }`
        // early-out.  PM_ClampAngles(), which runs above it, answers the flag
        // by pinning PITCH and ROLL to zero and letting only YAW follow the
        // mouse.  Stamp it on a client whose pmove type is PM_SPECTATOR or
        // PM_FREEZE and it is stamped there for good.
        //
        // Under arena that is every death: PutClientInServer() ends in
        // move_to_arena(), the observer comes back as a free-flying
        // MOVETYPE_NOCLIP body, and these two lines then ran on top of it --
        // an observer who could turn left and right and never look up or
        // down.  Rocket Arena deleted them from respawn() outright for exactly
        // this reason; the guard keeps them for the rulesets that walk.
        //
        // It is an assignment rather than an |=, so it also wiped the
        // PMF_NO_PREDICTION that SetObserverMode() had just set for the two
        // camera modes.
        if (self->movetype != MOVETYPE_NOCLIP) {
            self->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
            self->client->ps.pmove.pm_time = 112 >> PM_TIME_SHIFT;
        }

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
    // Being put in as a PLAYER ends the watch.  Here rather than at
    // each caller because there are four of them under ctf alone -- the join
    // menu, `team red`, the ghost rejoin and the Gladiator toggle's own leave
    // path -- and one of them (CTFTeam_f's spectator arm) is the one that was
    // missed: the flag stayed set, DoObserver kept clearing the attack button,
    // and the client had joined a team it could not shoot for.
    ent->flags &= ~FL_OBSERVER;
    memset(&ent->client->camera, 0, sizeof(ent->client->camera));

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
    // RA2 and tourney both replace spawn selection outright -- RA2
    // places into an arena, tourney into its match slots -- so this is a hook
    // rather than a predicate.
    // A refused placement is a state of its own, and it is tourney's: the client
    // keeps no body, cannot move, and the respawn trigger tries again on the
    // next think, because `osp_r240` stays 0.  The donor's own arm, including
    // the second reading of `osp_entered` -- SelectSpawnPoint refuses only an
    // entered client, so this cannot fire for an observer, and the donor tests
    // it at both ends anyway.
    if (!G_SelectSpawnPoint(ent, spawn_origin, spawn_angles) &&
        G_IsOspRuleset() &&
        ent->client->resp.osp_entered == ENTERED_ENTERED) {
        ent->movetype = MOVETYPE_NOCLIP;
        ent->solid = SOLID_NOT;
        ent->clipmask = 0;
        ent->svflags |= SVF_NOCLIENT;
        ent->client->resp.osp_r240 = 0;
        ent->client->ps.pmove.pm_type = PM_FREEZE;
        return;
    }

    // And tourney's own answer to "is this client a body?", which the
    // donor writes here, one line after the spawn point.  `osp_r240` is 2 for a
    // client that has been PLACED and 0 for one that has not -- an observer, or
    // a player whose spawn was refused -- and it is what ChangeWeapon, the
    // KillBox below and OSP_bestTrackTarget all read.  The merge kept the field
    // and all three readers and dropped every write of 2, so no tourney client
    // has ever been a body by that test -- the same shape as `pers.showmotd`,
    // had a reader, a clear and no write), and it survives the memset below
    // because `resp` is saved and restored across it.
    if (G_IsOspRuleset())
        ent->client->resp.osp_r240 = 2;

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
        InitClientPersistant(client, !G_IsOspRuleset());
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
        // ...and the two facts about the CONNECTION rather than about a life,
        // which `coop_respawn` cannot supply and which this assignment would
        // otherwise zero on the client's very first spawn.
        //
        // `InitClientResp` takes the `coop_respawn` snapshot, and it runs
        // BEFORE either of these is written: the address is latched at the end
        // of ClientConnect (R-LOG-4) and `listenhost` a few lines above that,
        // so the snapshot is of a `pers` that has neither.  The deathmatch arm
        // is not exposed to this -- `InitClientPersistant` preserves both
        // across its own wipe -- which is why a coop server was the one place
        // the address went missing, and a dedicated `sp` server IS coop
        // (README: single player needs a client).
        //
        // `listenhost` is the same defect and is fixed in the same line rather
        // than left for its own: on a coop listen server the host lost the bot
        // menu's exemption on their first spawn and did not get it back until
        // the next map.
        resp.coop_respawn.listenhost = client->pers.listenhost;
        memcpy(resp.coop_respawn.address, client->pers.address,
               sizeof(resp.coop_respawn.address));
        client->pers = resp.coop_respawn;
        if (resp.score > client->pers.score)
            client->pers.score = resp.score;
    } else {
        memset(&resp, 0, sizeof(resp));
    }

    ClientUserinfoChanged(ent, userinfo);

    // clear everything but the persistant data.
    //
    // And the menu handle lives in the part about to be zeroed, so it is
    // freed first or the allocation is orphaned.  `client->menu` is CTF's
    // PMenu, `curmenulink`/`menuqueue` are RA2's, and every one of them is
    // outside `pers`; a client that respawns with a menu open therefore lost
    // whatever it was holding.  The arbiter already closed the menu on
    // ClientDisconnect, which is the same thought one path short -- this is the
    // other path, and `q2pro@21381ffa` fixes it in the CTF donor the same way.
    //
    // G_MenuClose rather than ctf_PMenu_Close: it dispatches on
    // `client->menu_owner`, so it is right for all three menu systems and is a
    // no-op for a client that has none.
    G_MenuClose(ent);

    // ...and for arena the close above is not the whole of it.  Under arena
    // closing is hiding by design (ra_MenuClose):
    // the arbiter repaints the statusbar and frees nothing, so the queue is
    // still standing when the memset below forgets the head.  A menu is open on
    // every one of these, because this function ends in move_to_arena(..., 1)
    // and that reopens the observer menu -- so each respawn orphaned the menu
    // the previous respawn had opened, for the rest of the map.
    // `rocketarena2@28a8af7` reports 15 blocks and 819 bytes per respawn.
    if (G_Ruleset() == RULESET_ARENA)
        close_menus(ent);

    saved = client->pers;
    memset(client, 0, sizeof(*client));
    client->pers = saved;
    if (client->pers.health <= 0)
        InitClientPersistant(client, !G_IsOspRuleset());
    client->resp = resp;

    // copy some data from the client to the entity
    FetchClientEntData(ent);

    // clear entity values
    ent->groundentity = NULL;
    ent->client = &game.clients[index];
    // RA2 spawns an arena client undamageable, and this one line is what
    // makes its round machine work. ***  Under arena, `takedamage` is not a
    // property of being alive -- it is the round's grant: set_damage(arena,
    // DAMAGE_AIM) at ASTATE_FIGHTING is the only thing that makes a fighter
    // hittable, and SendTeamToArena, init_player and RA_BotJoinArena all put it
    // back to DAMAGE_NO on the way out.  fight_done() then reads
    // `takedamage == DAMAGE_AIM && deadflag == DEAD_NO` as "this team still has
    // somebody in the fight".
    //
    // baseq2's DAMAGE_AIM here breaks that, and the way it breaks is a round
    // that never ends.  A dead arena player is respawned by respawn() ->
    // PutClientInServer, which cleared deadflag and handed out DAMAGE_AIM, and
    // then the arena tail below turned them into an observer -- reinit_player()
    // sets fightstate and nothing else.  The result is a client fight_done()
    // counts as a living fighter on a team they are no longer playing for, for
    // the rest of the map: wipe the other side and the round sits in
    // ASTATE_FIGHTING forever.  Found by playing a pickup round to its end.
    ent->takedamage = (G_Ruleset() == RULESET_ARENA) ? DAMAGE_NO : DAMAGE_AIM;
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

    // Spawning ends whatever menu was open.  Threewave leaves the
    // handle live and relies on the join menu being reopened over it.
    G_MenuClose(ent);

    // Before the match is live a player spawns with the warmup
    // loadout rather than a blaster, so that warmup is practice rather than a
    // different game; and the grapple is let go and the accuracy row is opened.
    //
    // The HUD panels are not pointed here, and used to be: OSP_restartStats is
    // at the END of this function, next to the weapon, because `ps` is
    // memset to zero forty lines below and the panels live in `ps.stats`
    //These three are safe here -- the loadout is `pers.inventory`,
    // the accuracy row is p_acc[] and the grapple is an entity.
    if (G_IsOspRuleset()) {
        if (sync_stat < 2)
            OSP_warmupItems(ent);
        OSP_setSingleAccuracy(ent);
        OSP_hookoff_cmd(ent);

        // Tourney's fourth placement, and connecting is not entering.
        //
        // A tourney client arrives as an OBSERVER (OSP_clientBeginPre sets
        // `osp_entered` to 2) and ENTERS through one of six paths -- `join`, the
        // team menus, a team command, the 1v1 queue, a bot's join, a recovered
        // seat.  The donor's PutClientInServer therefore has two arms here and
        // the merge took the second one unconditionally, which left a client
        // that had not entered standing at a deathmatch spawn as a SOLID,
        // VISIBLE, walking body while every tourney predicate went on treating
        // it as an observer.  Played: the scoreboard says observer, the player
        // collects items, and under `dm` -- where `sync_stat` is 8 and the match
        // is live from the first frame -- shoots and kills while g_combat.c's
        // `entered != ENTERED_ENTERED` test makes them unkillable in return.
        //
        // This is the same defect one ruleset over, and its own comment
        // three arms below says how it was found there: a headless client
        // reading back its own pmove type.  `ops_tourney` says "SelectSpawnPoint
        // stays dm's: tourney places players through OSP_startObserve()", which
        // is true of the `observer` COMMAND and was never true of a placement --
        // that function prints, scores and re-sorts, so it is not what a spawn
        // may call.  The donor's own five lines are.
        if (client->resp.osp_entered != ENTERED_ENTERED) {
            ent->movetype = MOVETYPE_NOCLIP;
            ent->solid = SOLID_NOT;
            ent->clipmask = 0;
            ent->svflags |= SVF_NOCLIENT;
            client->resp.osp_r2bc = 1;
            client->resp.osp_r240 = 0;
        } else {
            // A client placed as a PLAYER is no longer looking at the
            // death scoreboard, and its speed watch is re-armed ten seconds
            // out.  `resp` is carried across the memset above, so these three do
            // not come free with it.
            ent->svflags &= ~SVF_NOCLIENT;
            client->osp_t040 = 0;
            client->osp_t03c = NULL;
            client->resp.osp_r2dc = 0;
            client->resp.osp_r2b8 = 0;
            client->resp.osp_r2b4 = level.framenum + 100;
        }
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
    // ...and an observer sees no weapon, which under tourney is `osp_r240`
    // rather than the weapon it is still carrying (the donor's own condition,
    // here and again in ChangeWeapon).
    if (client->pers.weapon &&
        !(G_IsOspRuleset() && client->resp.osp_r240 != 2))
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

    // The player now has a final origin and final angles.
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

    // RA2 applies post-connect spectator updates through its native placement
    // path, which establishes its fightstate observer representation.
    if (client->pers.spectator && G_Ruleset() != RULESET_ARENA &&
        !G_IsOspRuleset()) {
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
        // A new client lands in observer mode with the menu up, and one
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

    // An observer telefrags nobody: the donor's KillBox is behind the same
    // `osp_r240 == 2` that decides whether this client is a body at all.
    if (!(G_IsOspRuleset() && client->resp.osp_r240 != 2) && !KillBox(ent)) {
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

    // After the `ps` memset, which is where the donor has it.
    //
    // Tourney's HUD panels are `ps.stats` slots holding CONFIGSTRING INDICES --
    // the match clock at SID_OSP_MATCHSTATE, the frag/rank pair or the two team
    // columns, the id line -- and OSP_restartStats is what points them at their
    // strings.  The merge called it in the tourney block near the top of this
    // function, which is before `memset(&ent->client->ps, 0, sizeof(client->ps))`
    // clears the whole playerstate.  So every placement wrote the four or six
    // indices and then wiped them, and `if <stat>` in the statusbar hid every
    // panel that depended on one.
    //
    // Played, which is the only way it shows: the clock is correct through the
    // countdown -- OSP_CheckReady sets the same stat when the countdown starts
    // and nothing respawns anybody between then and the bell -- and vanishes on
    // the frame the match begins, because the match begins by killing and
    // respawning every player.  It never comes back, because every later
    // respawn wipes it again.  `scenarios/ospclock` reads all four layers
    // separately (the composed bar, the client's copy of it, the stat, the
    // configstring) so that the failure names this one.
    //
    // The donor's own position is `port_osp:p_client.c:1307`, immediately
    // before these two lines.  The arena arm returns above and needs none of
    // this.
    if (G_IsOspRuleset())
        OSP_restartStats(ent);

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

    // Not for a recovered tourney client.  `resp.osp_r210` is set by
    // OSP_recoverClient at connect time and means "this player is coming back
    // to a seat they already had"; everything that seat is made of --
    // resp.score, resp.team, resp.osp_entered and osp_r210 itself -- lives in
    // client_respawn_t, which InitClientResp memsets.  Without the guard
    // `client_recover` cannot fire at all: OSP_clientBeginPre and
    // OSP_clientBegunPost both read osp_r210 after this point and would see 0.
    //
    // The id is a separate half of the same defect and is fixed inside
    // InitClientResp, which carries resp.clientid across its own memset the way
    // the donor's does -- ClientBegin hands the id out through
    // OSP_clientBeginLevel() before this runs.
    if (!(G_IsOspRuleset() && ent->client->resp.osp_r210))
        InitClientResp(ent->client);

    //PGM
    if (G_UsesRogueGameRules() && DMGame.ClientBegin) {
        DMGame.ClientBegin(ent);
    }
    //PGM

    // The four things tourney asks a connecting client for, and the
    // entered/active bookkeeping that has to happen before placement.
    if (G_IsOspRuleset())
        OSP_clientBeginPre(ent);

    // locate ent at a spawn point
    PutClientInServer(ent);

    // After placement, not inside it: the bot is now the noclip observer in
    // arena 0 that init_player() made, which is exactly the state a human is
    // in when they pick a team off the menu, and this is the click they cannot
    // make.
    if (G_Ruleset() == RULESET_ARENA)
        RA_BotJoinArena(ent);

    if (level.intermission_framenum) {
        MoveClientToIntermission(ent);
    } else if (G_IsOspRuleset()) {
        // Unconditionally -- an arriving tourney client is an
        // observer and the donor announces it anyway, because
        // OSP_playerAnnounce carries the only condition there is: in a live
        // 1-vs-1 only the two duellists make a noise.  This arm is needed
        // written out, because G_IsObserver() now answers for this ruleset and
        // the test below would have silenced every arrival.
        //
        // And NO teleport hold: the donor's ClientBeginDeathmatch has no such
        // line -- respawn() is where it belongs -- and stamping
        // PMF_TIME_TELEPORT on a client whose pmove type is PM_SPECTATOR pins
        // its pitch for good (see respawn()'s own note).
        OSP_playerAnnounce(ent, MZ_LOGIN);
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

    // Not under tourney, where connecting is not entering: a client arrives as
    // an OBSERVER and announces itself as "%s entered the game (clients = N)"
    // from the six paths that actually put it in the match.  Announcing here
    // as well called every connection a join and every join twice.  The donor
    // deletes this line outright.
    if (!G_IsOspRuleset())
        gi.bprintf(PRINT_HIGH, "%s entered the game\n", ent->client->pers.netname);

    // And everything after placement, including the one arm that can
    // refuse the connection outright -- a match already running with
    // `match_latejoin` off.  True means the client is gone.
    if (G_IsOspRuleset() && OSP_clientBegunPost(ent))
        return;

    // Tourney's bot half, and the same argument as arena's above: a tourney
    // client connects as an OBSERVER and enters by pressing a key, which a bot
    // cannot do.  After OSP_clientBegunPost rather than before it, because that
    // is where `resp.team` becomes 2 ("no team") and the team join reads it --
    // run before, the mode-2 join put every bot on team 0.
    if (G_IsOspRuleset())
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
    // The donor resets the lag queue here, before the client is
    // hooked up.  Any command still queued belongs to the previous life on the
    // previous map and would replay a shot from an origin in another level.
    Lag_BeginGame(ent);

    int     i;

    ent->client = game.clients + (ent - g_edicts - 1);

    // A feature whose client half is an alias is unreachable until the server
    // stuffs it.
    //
    // `+hook`, `-hook`, `+grap` and `-grap` are console ALIASES and no Quake II
    // client ships one, so `bind e "+hook"` -- which is what every 1999 config
    // and every player types -- binds a key to nothing.  Both offhand grapples
    // in this tree had their commands ported and their aliases dropped:
    // ctf's `hookon`/`hookoff` (1999, stuffed at the top of
    // its ClientBegin) and arena's `grap_on`/`grap_off` (RA2, stuffed at the top
    // of its ClientBeginDeathmatch).  `tourney` did not, which is why the gap
    // was ruleset-shaped rather than visible; OSP_hookAliases does it now.
    //
    // No `cmd` prefix, because a console command the client does not recognise
    // is forwarded to the server -- that is what makes `hookon` arrive at
    // ClientCommand.  A bot has no console and is skipped, as both donors skip
    // it.  The aliases are stuffed whether or not the ruleset's hook cvar is
    // on, because the command tests the cvar itself: a server that turns the
    // hook off mid-map does not leave a stale alias firing it, and one that
    // turns it on does not need every client to reconnect.
    if (!(ent->flags & FL_BOT)) {
        if (G_Ruleset() == RULESET_CTF) {
            stuffcmd(ent, "alias +hook hookon\n");
            stuffcmd(ent, "alias -hook hookoff\n");
        } else if (G_Ruleset() == RULESET_ARENA) {
            stuffcmd(ent, "alias +grap grap_on\nalias -grap grap_off\n");
            stuffcmd(ent, "alias +hook grap_on\nalias -hook grap_off\n");
        }
    }

    // The per-LEVEL half -- a client id, a place back on their team,
    // and the 1-vs-1 queue.  Before ClientBeginDeathmatch rather than inside
    // it, because it also has to run for a client who was already here when
    // the level changed.
    if (G_IsOspRuleset())
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
        Q_strlcpy(userinfo, "\\name\\badinfo\\skin\\male/grunt", MAX_INFO_STRING);
    }

    // Tourney edits the userinfo it was handed -- refusing a forbidden
    // or too-frequent rename, forcing a qualifier skin -- and everything below
    // reads the edited copy, which is what makes the edit take effect.  It also
    // owns the rename logging and the green-text copy of the name.
    if (G_IsOspRuleset())
        OSP_userinfoChanged(ent, userinfo);

    // set name
    s = Info_ValueForKey(userinfo, "name");
    Q_strlcpy(ent->client->pers.netname, s, sizeof(ent->client->pers.netname));

    // set spectator
    s = Info_ValueForKey(userinfo, "spectator");
    // spectators are only supported in deathmatch
    // if (deathmatch->value && strcmp(s, "0"))
    //
    // ...and not under ctf, which has its own observer mechanism:
    // the `observer` command and the join menu, keyed on ctf_team.  Leaving
    // baseq2's flag reachable there would put two observer systems on one
    // client -- `spectator 1` in userinfo would noclip a player that
    // G_IsObserver() still reports as playing, because it asks about the team.
    // Threewave deletes the key outright; here it is simply inert.
    if (deathmatch->value && G_Ruleset() != RULESET_CTF &&
        !G_IsOspRuleset() && *s && strcmp(s, "0"))
        ent->client->pers.spectator = true;
    else
        ent->client->pers.spectator = false;

    // set skin
    s = Info_ValueForKey(userinfo, "skin");

    playernum = ent - g_edicts - 1;

    // combine name and skin into a configstring.  Under ctf the skin is the
    // team's, not the player's choice -- CTFAssignSkin writes the same
    // configstring with ctf_r or ctf_b substituted, which is also where the 1999
    // bot-model bug lived.  CTFAssignSkin is not called here, which is
    // why `ctf` is an exclusion below rather than an arm: it also writes
    // `pers.userinfo`, and the save at the tail of this function would put the
    // player's own skin straight back over it, so the call is the last thing
    // this function does.  The Gladiator donor moved it for the same reason and
    // left the note saying so.
    //
    // `teams[]` is TAG_LEVEL and arena_init() reallocates it empty on every map,
    // while `resp.teamnum` is on the client and outlives the level -- so the
    // slot a returning client names may be NULL, and this dereferenced it.
    // Unreachable while nothing was ever on an arena team, and still not reached on a deathmatch map, where arena_init
    // rebuilds the two pickup teams into slots 0 and 1 and the stale index
    // happens to be valid again.  On a real arena map, where each client makes
    // a team of its own, it is a null dereference in ClientUserinfoChanged --
    // which BotSpawn calls for every bot before ClientBegin has reset resp.
    // `/nullxxx` is written by this tree.
    //
    // `setteamskin()` ends every one of its four arms with `stuffcmd(ent,
    // "skin <model>/nullxxx\n")`.  That is 1999's way of forcing a client to
    // stop drawing ITSELF in its own `skin` cvar and fall back to the
    // configstring the server just wrote: point the cvar at a skin that does
    // not exist.  The client obeys, reports the new userinfo, and arrives back
    // here saying its skin is `male/nullxxx` -- which is exactly the case the
    // donor's second branch is for, and which the merge dropped as
    // unreachable.
    //
    // What it costs: a player who has since LEFT their team publishes
    // `playerskins` as `<name>\<model>/nullxxx`, because their `skin` cvar is
    // still whatever the last stuff set it to and nothing puts it back.  The
    // donor substitutes `male/grunt` for exactly that client and restores the
    // saved skin into `userinfo` for everyone else, so the server-side copy
    // stops carrying a placeholder.
    if (G_Ruleset() == RULESET_ARENA && strstr(s, "/nullxxx")) {
        char    saved[MAX_QPATH];
        team_t  *t = NULL;

        if (ent->client->resp.teamnum != -1)
            t = teams[ent->client->resp.teamnum].it;

        // Copied out rather than aliased: Info_ValueForKey returns a rotating
        // static and Info_RemoveKey below is entitled to use it too.
        Q_strlcpy(saved, Info_ValueForKey(ent->client->pers.userinfo, "skin"),
                  sizeof(saved));
        Info_RemoveKey(userinfo, "skin");

        if (!t || t->skin == -1) {
            Q_strlcpy(saved, "male/grunt", sizeof(saved));
            gi.configstring(game.csr.playerskins + playernum,
                            va("%s\\%s", ent->client->pers.netname, saved));
        }

        Q_strlcat(userinfo, va("\\skin\\%s", saved), MAX_INFO_STRING);
    } else if (G_Ruleset() == RULESET_ARENA && ent->client->resp.teamnum != -1 &&
               teams[ent->client->resp.teamnum].it &&
               ((team_t *)teams[ent->client->resp.teamnum].it)->skin != -1)
        // RA2 does the same thing for the same reason: on a team, the skin is
        // the team's.
        setteamskin(ent, userinfo, ((team_t *)teams[ent->client->resp.teamnum].it)->skin);
    else if (G_Ruleset() != RULESET_CTF)
        gi.configstring(game.csr.playerskins + playernum, va("%s\\%s", ent->client->pers.netname, s));

    // CTF's player-id view reads the name out of a configstring of its own,
    // because the playerskins string carries the skin as well.
    // game.csr.general, not CS_GENERAL: the compile-time constant is the
    // extended one (13118) and a server without the extensions runs on
    // cs_remap_old, whose end is 2080 -- so the literal drops the server the
    // first time a client connects.
    //
    // ...and arena's ID view had the same problem and no answer to it.  RA2
    // points its `stat_string` row straight at `playerskins + n`, and
    // `stat_string` draws the configstring VERBATIM -- so an arena observer
    // watching somebody read "Sarge\male/red" rather than "Sarge", the skin
    // path and the separator included, and read it wider the longer the model
    // name was.  Threewave hit that in 1998 and answered it with this line;
    // 1999's Rocket Arena did not, and the answer transfers unchanged.
    if (G_Ruleset() == RULESET_CTF || G_Ruleset() == RULESET_ARENA)
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

    // ...and only NOW the CTF skin, because CTFAssignSkin's second write goes
    // into the copy above.  The skin is re-read rather than reusing `s`:
    // Info_ValueForKey hands back a rotating static buffer and the fov and hand
    // lookups between here and there have taken it twice.
    if (G_Ruleset() == RULESET_CTF)
        CTFAssignSkin(ent, Info_ValueForKey(userinfo, "skin"));
}

/*
===========
G_LatchClientAddress

Where this client connected from, remembered for as long as the connection
lasts.  Latched here and read everywhere else, because HERE is the one moment
the value is the engine's: q2pro force-sets userinfo `ip` in the connect packet
(`parse_userinfo`, from `NET_AdrToString`) and never again, so every later copy
of the key is whatever the client last sent -- `setu ip 1.2.3.4` is a command
every client has.  A log that re-read it would print what the player typed.

Tourney derived this string twice, in ClientConnect and lazily in
OSP_getPlayerAddr, and stored it on the edict; the lazy half read the client's
own userinfo and so could be talked into a wrong ban line.  One writer, one
field (`pers.address`), four rulesets reading it.  R-LOG-1.
============
*/
static void G_LatchClientAddress(edict_t *ent, const char *userinfo)
{
    char *port;

    // A bot has no `ip` at all -- its userinfo is bl_spawn.c's own -- and gets
    // tourney's name for one, because "no address" and "not a person" are
    // precisely the two cases a log reader must be able to tell apart.
    //
    // FL_BOTCLIENT and not FL_BOT: BotCreate clears FL_BOT across this call so
    // that BotMoveToFreeClientEdict does not recurse, so FL_BOT is false for
    // every bot at exactly this point.
    if (ent->flags & FL_BOTCLIENT) {
        Q_strlcpy(ent->client->pers.address, "SERVER_BOT",
                  sizeof(ent->client->pers.address));
        return;
    }

    Q_strlcpy(ent->client->pers.address, Info_ValueForKey(userinfo, "ip"),
              sizeof(ent->client->pers.address));

    // "<address>:<port>", and it is the LAST colon that is the port's: an IPv6
    // peer arrives as "[::1]:27910" and cutting at the first would leave "[".
    // An address with no port is not an error -- a listen server's own client
    // is the bare string "loopback" -- and has no colon to cut at.
    port = strrchr(ent->client->pers.address, ':');
    if (port)
        *port = 0;
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
    // Never connect as an observer.  The flag lives on the edict and
    // an edict is reused, so a client taking a slot an observer left would
    // inherit FL_OBSERVER and its camera.
    ent->flags &= ~FL_OBSERVER;

    char    *value;

    // The connect-time half of RA2's ZBot detection.  The cheat client
    // of the day announced itself by the port it connected from, so RA2 keeps
    // the port and InitClientResp seeds `resp.isbot` from it.  Before the ban
    // check, as the donor has it -- a banned client never reaches either.
    if (G_Ruleset() == RULESET_ARENA) {
        value = Info_ValueForKey(userinfo, "ip");
        if (*value) {
            // "<addr>:<port>", and an address with no port is not an error --
            // a local client connects without one.
            char *colon = strchr(value, ':');

            if (colon) {
                ent->client->zbotscore = atoi(colon + 1);
                // A cheat REPORT, and it is tagged so that it cannot be read
                // as the connect record below.  The merge had it as
                // `"%s connected with ZBOT\n"` with the whole USERINFO
                // substituted for the name -- a line shaped like an arrival,
                // naming nobody, carrying a backslash-separated blob of
                // client-supplied text through the console log and past
                // anything parsing it.  RA2's own is two lines and prints the
                // userinfo deliberately, so the detail stays; what changes is
                // that the player is named and the line says what it is.
                //
                // `netname` does not exist yet -- ClientUserinfoChanged has
                // not run -- so the name comes from the userinfo, which is
                // what that function is about to read it out of anyway.
                if (ent->client->zbotscore == RA_ZBOT_PORT) {
                    char addr[MAX_CLIENT_ADDRESS];

                    // Info_ValueForKey hands back a rotating static buffer, so
                    // the address is copied before the name is asked for.
                    Q_strlcpy(addr, value, sizeof(addr));
                    gi.dprintf("ZBOT: %s from %s -- userinfo \"%s\"\n",
                               Info_ValueForKey(userinfo, "name"), addr,
                               userinfo);
                }
            }
        }
    }

    // check to see if they are on the banned IP list
    value = Info_ValueForKey(userinfo, "ip");
    if (SV_FilterPacket(value)) {
        Info_SetValueForKey(userinfo, "rejmsg", "Banned.");
        return false;
    }

    // check for a spectator
    value = Info_ValueForKey(userinfo, "spectator");
    if (G_Ruleset() == RULESET_ARENA && *value && strcmp(value, "0")) {
        Info_SetValueForKey(userinfo, "rejmsg", "id Spectator Mode not Supported");
        return false;
    }
//  if (deathmatch->value && strcmp(value, "0"))
    // CTF does not use baseq2's generic spectator admission. Arena's nonzero
    // fresh request was rejected above; its post-connect path is native.
    if (deathmatch->value && G_Ruleset() != RULESET_CTF &&
        G_Ruleset() != RULESET_ARENA && !G_IsOspRuleset() &&
        *value && strcmp(value, "0")) {
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

    // The player list gets a veto, with its own reason in `rejmsg`.
    if (G_IsOspRuleset() && !OSP_clientAllowed(ent, userinfo))
        return false;

    // This edict may be a BOT's -- G_SpawnClient hands bots the high
    // slots but the engine hands a connecting human whichever slot
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

    // Is this client the host of a LISTEN server?  Latched here and
    // nowhere else, because here is the one place the value is the engine's:
    // q2pro force-sets userinfo `ip` in the connect packet (`parse_userinfo`,
    // from `NET_AdrToString`, which is the bare string "loopback" for
    // NA_LOOPBACK and has no port), and it does so because G_FEATURES does not
    // carry GMF_EXTRA_USERINFO -- with that bit the key arrives in the extra
    // userinfo block instead.  Afterwards the key is the CLIENT's: a full
    // userinfo update replaces the whole string and a delta writes any key at
    // all, unfiltered, so `setu ip loopback` from any remote client would
    // otherwise be a way into the menu that adds and removes bots.
    //
    // Written unconditionally, not set-if-true: this edict's slot may have
    // belonged to the host before, and the preserve in InitClientPersistant
    // would carry their exemption to whoever takes the seat next.
    //
    // `dedicated` is CVAR_NOSET, so the first half cannot be turned on under a
    // running listen server.  A bot is never mistaken for the host: a fake
    // client's userinfo is bl_spawn.c's own and has no `ip` at all.
    //
    // The force-set is what makes the key TRUSTWORTHY at connect -- it
    // overwrites whatever the client sent -- and it is conditional on the
    // library not asking for extra userinfo, so the condition is asserted
    // rather than described: under GMF_EXTRA_USERINFO the engine leaves the
    // normal userinfo alone and passes `ip` in a second infostring after the
    // terminating NUL, and a client could then supply `\ip\loopback` in its
    // own connect packet and be taken for the host.
    _Static_assert(!(G_FEATURES & GMF_EXTRA_USERINFO),
                   "the host exemption reads userinfo `ip`, which the engine "
                   "force-sets only while the game does not want extra "
                   "userinfo -- see ClientConnect");
    ent->client->pers.listenhost =
        !dedicated->value &&
        !strcmp(Info_ValueForKey(userinfo, "ip"), "loopback");

    // A player who dropped mid-match gets their seat, score and team back if
    // they come back inside `team_recovertime` -- which is what the pause in
    // ClientDisconnect is waiting for.  Before InitClientResp, because that is
    // what it has to survive.
    if (G_IsOspRuleset())
        OSP_recoverClient(ent, userinfo);

    // if there is already a body waiting for us (a loadgame), just
    // take it, otherwise spawn one from scratch
    //
    // Not for a client that is taking its own seat back.  `osp_r210`
    // is what OSP_recoverClient just set, and the seat -- score, team, entered,
    // client id -- is exactly what these two calls would clear.  The donor
    // carries the same guard, in this same place.
    if (ent->inuse == false &&
        !(G_IsOspRuleset() && ent->client->resp.osp_r210)) {
        // ...and a client that is not recovering must not inherit the previous
        // occupant of this slot either.  SpawnEntities deliberately carries
        // three edict fields across a level change (see g_spawn.c), so the
        // remembered default team and its skin have to be cleared for a new
        // arrival here, where the donor clears them.  The address was the
        // fourth and needs no clear: `G_LatchClientAddress` writes
        // `pers.address` unconditionally at the end of this function, on both
        // arms, so a new arrival cannot inherit one (R-LOG-4).
        if (G_IsOspRuleset()) {
            ent->osp_e3a0[0] = 0;
            ent->osp_e3b0[0] = 0;
        }
        // clear the respawning variables.  Under ctf, force a team join and
        // turn the player-id display on: id is on by default, and
        // InitClientResp preserves both fields rather than setting them, so
        // they have to be seeded here.
        if (G_Ruleset() == RULESET_CTF) {
            ent->client->resp.ctf_team = -1;
            ent->client->resp.id_state = true;
        }
        InitClientResp(ent->client);
        if (!game.autosaved || !ent->client->pers.weapon)
            InitClientPersistant(ent->client, true);

        // RA2's motd, and the only thing that ever set this true.  The
        // donor sets it here, in this same `inuse == false` arm and after the
        // two Init calls for the same reason they are ordered that way; the
        // merge kept `showmotd`'s declaration, kept init_player()'s read of it
        // and kept menuMotdContinue()'s clear, and dropped the one write that
        // made the field ever be true.  So `motd.txt` was loaded at every map
        // load, reported "Sucessfully read arena/motd.txt", and was shown to
        // nobody -- init_player() took its `else` branch every time.
        //
        // Under arena only: motd.txt is RA2's file and the menu that draws it
        // is RA2's (a donor's feature stays in its ruleset).
        if (G_Ruleset() == RULESET_ARENA)
            ent->client->pers.showmotd = true;
    }

    ClientUserinfoChanged(ent, userinfo);

    // RA2's stdlog gets the arrival, after the rename that decides what name is
    // written.  Three of `gslog.c`'s six entry points had no caller: the log
    // recorded GameStart, the kills and GameEnd, and never a PlayerConnect, a
    // PlayerLeft or a MAP line -- the file was complete, ported and half wired.
    if (G_Ruleset() == RULESET_ARENA)
        GSLogEnter(ent);

    // THE CONSOLE RECORD OF THE ARRIVAL -- one line, one wording, every
    // ruleset (R-LOG-1).  Three donors wrote this line three ways: baseq2 and
    // Threewave "%s connected", RA2 "%s connected from <ip:port>", tourney
    // "(%s connected from <ip>)" beside a bprintf announcement of its own.
    // Section 7 rule 6 keeps one, the most capable, and that is tourney's: the
    // brackets make it greppable, the port is off, and a bot says so.
    //
    // It REPLACES the bare line rather than joining it.  Under the OSP four
    // both used to print and both are gi.dprintf here -- baseq2's line took
    // the place of tourney's player-facing gi.bprintf -- so a dedicated
    // console carried the same arrival twice.
    G_LatchClientAddress(ent, userinfo);
    if (game.maxclients > 1)
        gi.dprintf("(%s connected from %s)\n", ent->client->pers.netname,
                   ent->client->pers.address);

    if (G_IsOspRuleset())
        OSP_clientConnected(ent);

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

    // ...and NOT TWICE FOR ONE DEPARTURE.  Tourney's referee kick, its ban,
    // its vote-kick and both of its cheat detectors write `svc_disconnect` at
    // the client and then call this function themselves; the engine's own
    // `SV_DropClient` calls it again when the connection actually goes, and
    // the "called recursively?" guard there is on the ENGINE's client state,
    // which the mod's direct call never touched.  So everything below ran a
    // second time against a client that had already been torn down: a second
    // departure record with an EMPTY name (OSP_clientLeft clears `netname`), a
    // second "wimped out and left" to every player, and a second
    // OSP_clientLeft -- whose `active_clients--` is guarded but whose
    // OSP_Stats and accuracy rows are not.
    //
    // Measured on a running server before it was fixed, by the referee-kick
    // row of `scenarios/connectlog`:
    //
    //   (kickme disconnected from 127.0.0.1)
    //   ( disconnected from 127.0.0.1)
    //
    // `pers.connected` is the right flag and not an invented one: ClientConnect
    // sets it last, InitClientPersistant sets it on every spawn, and the tail
    // of this function is the only thing that clears it -- so it means exactly
    // "this client has a session open".
    if (!ent->client->pers.connected)
        return;

    // ...and the departure, before anything else may return early.
    if (G_Ruleset() == RULESET_ARENA)
        GSLogExit(ent);

    // ...and the console record of the departure, which mirrors the arrival's
    // word for word and is gated the same way (R-LOG-1).  No donor has one:
    // every one of them announces the departure to the PLAYERS and leaves the
    // server's own log with a name and no address, so a log cannot pair an
    // arrival with its departure and cannot say whether a session was a person
    // or a bot.  That pairing is the whole of what the record is for.
    if (game.maxclients > 1)
        gi.dprintf("(%s disconnected from %s)\n", ent->client->pers.netname,
                   ent->client->pers.address);

    if (G_IsOspRuleset()) {
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

    // A client who quits with a menu open must not leave the handle
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

    if (G_UsesRogueGameRules()) {
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
    // Invisible while nothing ever put a client on an arena team, because
    // InitClientResp left `teamnum` at 0 and PutClientInServer therefore never
    // ran init_player().  With bots joining, `sv removebot all` left sixteen
    // dead members linked into two pickup teams and UpdateStatusBars
    // dereferenced the first of them on the next frame:
    //
    //   UpdateStatusBars (arenanum=1) at src/arena/arena.c:1497
    //   arena_think / multi_arena_think / RA_CheckRules / G_RunFrame
    //
    // `resp.entered = false` is the donor's next line and is not carried: its
    // only reader in RA2 is a "reconnect without disconnect" arm in
    // ClientConnect that this tree does not have, and the field is shared with
    // tourney's four-state enum, whose owner clears it its own way.
    if (G_Ruleset() == RULESET_ARENA) {
        remove_from_team(ent);
        ent->client->resp.fightstate = FIGHT_SPECTATING;
    }

    ent->inuse = false;
    ent->classname = "disconnected";
    ent->client->pers.connected = false;

    // The brain keeps its own copy of every client and nothing clears it when
    // one leaves, because BotLib_UpdateAllClientSettings skips a slot whose
    // edict is not `inuse` -- which is the line above.  Here rather than at the
    // tail of this function because tourney's arm below can return, and after
    // `inuse` rather than before it so the state pushed is the one that lasts.
    BotLib_ClientDisconnected(ent);

    // The half that needs the slot already free -- the recount, the
    // recover seat, the chase cams that were watching this player, and the
    // pause that waits for the last member of a team to come back.
    if (G_IsOspRuleset() && OSP_clientLeft(ent, osp_team))
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
ClientLagThink

The half of ClientThink that the lag simulation replays.  Called once with the live
usercmd on a server with `g_clientlag 0`, which is every server that has not
asked for the simulation, and once per delayed command otherwise -- with
`ent->s.origin` and `client->v_angle` already put back to what they were when
that command arrived, so a shot leaves from where the player was rather than
from where they are.
==============
*/
static void ClientLagThink(edict_t *ent, usercmd_t *ucmd)
{
    gclient_t *client = ent->client;

    client->oldbuttons = client->buttons;
    client->buttons = ucmd->buttons;
    client->latched_buttons |= client->buttons & ~client->oldbuttons;

    // Closed at the button rather than at the think -- see RA_HoldFire() for
    // why the donor's `Think_Weapon` guard cannot be taken verbatim.  Here
    // rather than beside either call site because both of them read these two
    // fields, as does every weaponthink underneath them, so one clear covers
    // baseq2's weapons, the mission packs' and Threewave's.
    //
    // This runs before the observer mode cycle below, which also reads
    // BUTTON_ATTACK -- safe only because RA_HoldFire() answers false for a
    // FIGHT_SPECTATING client, which is every observer.  Moving it after that
    // block would work too; moving the exemption out of RA_HoldFire() would not.
    if (G_Ruleset() == RULESET_ARENA && RA_HoldFire(ent)) {
        client->buttons &= ~BUTTON_ATTACK;
        client->latched_buttons &= ~BUTTON_ATTACK;
    }

    // save light level the player is standing on for
    // monster sighting AI
    ent->light_level = ucmd->lightlevel;

    // RA2's observer key, and the half of it that was missing.  RA2 has two
    // observer inputs and this tree carried only one: jump/crouch cycles WHO
    // you are watching (in ClientThink, where it reads the movement axes), and
    // ATTACK cycles HOW -- normal, free-flying, trackcam, eyecam.  ChangeOMode()
    // was ported, declared and never called, so an arena observer was pinned in
    // whichever mode move_to_arena() had left them in: on a pickup arena that
    // is always OMODE_FREEFLYING, the two camera modes were unreachable, and
    // with them the jump/crouch cycle that only runs inside them.  There was no
    // way to watch a team-mate at all.
    //
    // `context != 0` is the donor's and it is load-bearing: arena 0 is the
    // lobby, it has nobody to track, and cycling there would move the client
    // out of the team menu it is standing in.  Not for a bot, and this is the
    // second half of the same rule that keeps a layout away from one: the
    // observer inputs are a PERSON's, read off a screen a bot does not have.
    // A bot holds ATTACK for a different reason entirely -- EA_Respawn is
    // "press fire to respawn" -- so wiring this arm without the gate handed
    // every dead bot a camera mode it never asked for, and the consequences
    // ran a long way.  SetObserverMode gives a camera mode a track_target;
    // ClientThink used to answer a track_target on a spectating client with
    // PM_FREEZE; and PM_FREEZE is exactly what the brain's BotIntermission()
    // tests for.  The bot decided the level had ended, re-entered its
    // intermission node every frame, and said its end-of-level line every
    // frame with it.  Flood protection let four through, which is what a
    // player sees: the same sentence, four times, instantly.  RA2's own PM_GIB
    // is restored in that branch, so the PM_FREEZE half of this is history;
    // the gate stays, because a bot has no screen to point a camera at either
    // way.
    if (G_Ruleset() == RULESET_ARENA && !(ent->flags & FL_BOT)) {
        if (!(client->latched_buttons & BUTTON_ATTACK))
            client->resp.omode_buttons &= ~1;
        else if (client->resp.fightstate == FIGHT_SPECTATING &&
                 client->resp.context != 0 &&
                 !(client->resp.omode_buttons & 1)) {
            ChangeOMode(ent);
            client->resp.omode_buttons |= 1;
        }
    }

    // fire weapon from final position if needed
    if (client->latched_buttons & BUTTON_ATTACK) {
        // Tourney's observer input is not baseq2's, and this is where it
        // arrives. ***  baseq2 gives a spectator one key: ATTACK toggles the
        // chase cam.  The donor gives a free-flying observer a MENU on the
        // first press and the AUTOCAM on every one after it, and reaches the
        // chase camera from the autocam rather than from here -- so this arm
        // has to run instead of the two below, which the widened G_IsObserver()
        // would otherwise send an OSP observer into.
        //
        // `osp_r02c` is "this client has been offered its menu once", which is
        // what makes the first press different from the rest; `sync_stat != 4`
        // is "no match is running", because during a match the menu is not what
        // ATTACK is for.  The three menus are the donor's own three-way choice:
        // teams get the team menu, a client that has never entered gets put into
        // OBSERVE properly, and everybody else gets the DM menu.
        if (G_IsOspRuleset() && client->resp.osp_entered == ENTERED_OBSERVER &&
            client->resp.osp_r010 <= level.framenum) {
            if (G_MenuActive(ent)) {
                Cmd_InvUse_f(ent);
            } else if (sync_stat != 4 && !client->resp.osp_r02c) {
                client->resp.osp_r02c = 1;
                if (OSP_IsTeams())
                    OSP_teamMenu(ent);
                else if (!client->resp.osp_r030)
                    OSP_startObserve(ent);
                else
                    OSP_DMMenu(ent);
            } else if (active_clients) {
                client->resp.score = client->resp.osp_r248;
                CameraCmd(ent, false);
                gi.cprintf(ent, PRINT_HIGH, "Changing to AUTOCAM mode.\n");
                client->resp.osp_r010 = level.framenum + 8;
                return;
            } else {
                gi.cprintf(ent, PRINT_HIGH, "No clients to track.\n");
                client->resp.osp_r010 = level.framenum + 8;
                return;
            }
            client->resp.osp_r010 = level.framenum + 2;
            client->latched_buttons = 0;
        } else if (G_IsOspRuleset() && G_IsObserver(ent)) {
            // A tourney client in a CAMERA -- chasecam, in-eyes or autocam --
            // had its press read before pmove (the chase branch above, and
            // OSP_clientThink for the autocam), so there is nothing left to do
            // with it here and baseq2's chase toggle must not have it.
            client->latched_buttons = 0;
        } else if (G_Ruleset() == RULESET_CTF && G_IsObserver(ent)) {
            // Threewave reserves CTF chase acquisition for its menu action.
            client->latched_buttons &= ~BUTTON_ATTACK;
        // G_IsObserver, not resp.spectator: under ctf an observer is a
        // CTF_NOTEAM player, so the inherited test would let him shoot
        // (observers cannot fire it).
        } else if (G_IsObserver(ent) && G_Ruleset() != RULESET_ARENA) {
            client->latched_buttons = 0;

            if (client->chase_target) {
                client->chase_target = NULL;
                client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
            } else
                GetChaseTarget(ent);
        } else if (G_IsObserver(ent)) {
            // And an ARENA observer does not fire either.  It reaches
            // this line because the arm above is deliberately not its own --
            // RA2 ships four observer modes and no chase cam, so ATTACK is the
            // key that cycles them (handled at the top of this
            // function) -- and the donor then lets the same press fall through
            // to Think_Weapon.  An arena observer is invisible, non-solid and
            // has no ammo, but ClientSpawn leaves it the Blaster, which needs
            // none: so a player waiting in the queue could plink the fighters
            // in a live round, from inside the arena, with no way for them to
            // answer.  `rocketarena2/p_client.c` has the same fall-through, so
            // this is 1999's and is fixed at the root rather than kept: the
            // whole point of the mode is to watch.
            //
            // The latch is not cleared here.  Clearing it is the chase arm's
            // business -- it consumes the press to change target -- whereas
            // The mode cycle has already read this one, and
            // ClientBeginServerFrame clears it at the end of the frame.
        } else if (!client->weapon_thunk) {
            client->weapon_thunk = true;
            Think_Weapon(ent);
        }
    }

    // Tourney is excluded for the same reason as the ATTACK block above: jump
    // cycles the chase target INSIDE the chase branch there, before
    // pmove, and running baseq2's cycle as well would step two targets per
    // press.
    if (G_IsObserver(ent) && G_Ruleset() != RULESET_ARENA &&
        G_Ruleset() != RULESET_CTF && !G_IsOspRuleset()) {
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
}

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

        // What tourney does to a client when the level ends, and this is
        // where it arrives: the whole per-client block was missing. ***  Once,
        // two seconds in, and never for a bot:
        //
        //   * the demo it was told to record is STOPPED -- and a referee's or a
        //     player's demo at setting 2 gets a screenshot of the final board,
        //     which is the point of taking one at all;
        //   * it hears the end-of-match music.  `wav_file` -- five 25-byte
        //     tunes in one array, which this tree carried and never read -- is
        //     the free-for-all default, `match_endmusic` overrides it, and a
        //     TEAM ruleset plays the winner and the loser different ones;
        //   * an entered client is shown its accuracy page, which until now was
        //     only reachable by typing `accuracy`;
        //   * whatever menu it had open is closed, and the recover latch is
        //     dropped so the next level starts it fresh.
        //
        // `osp_r01c` bit 0x10 is the "already done" latch -- the same field
        // whose low three bits are the countdown announcer's, which is why it is
        // ORed rather than assigned.
        if (G_IsOspRuleset() &&
            !(client->resp.osp_r01c & 0x10) &&
            level.framenum > level.intermission_framenum + 2 * BASE_FRAMERATE &&
            !(ent->flags & FL_BOT)) {
            char cmd[64];

            if (client->resp.osp_r234)
                stuffcmd(ent, ((ent->osp_e39c == 1 && demo_referee->value > 1) ||
                               demo_player->value > 1)
                              ? "stop; screenshot\n" : "stop\n");

            if (!OSP_IsTeams()) {
                if (!match_endmusic || !match_endmusic->string[0] ||
                    !strcmp(match_endmusic->string, "default"))
                    Q_snprintf(cmd, sizeof(cmd), "play %s\n",
                               wav_file + (Q_rand() % 5) * 25);
                else
                    Q_snprintf(cmd, sizeof(cmd), "play %s\n",
                               match_endmusic->string);
            } else if (OSP_teamLost(client->resp.team)) {
                Q_strlcpy(cmd, "play makron/laf4.wav\n", sizeof(cmd));
            } else {
                Q_strlcpy(cmd, "play world/xian1.wav\n", sizeof(cmd));
            }
            stuffcmd(ent, cmd);

            client->resp.osp_r01c |= 0x10;
            OSP_hookoff_cmd(ent);
            if (client->resp.osp_entered == ENTERED_ENTERED)
                OSP_accuracyInfo(ent, client->pers.netname,
                                 client->resp.clientid);
            OSP_closeMenus();
            client->resp.osp_r210 = 0;
        }

        // ...and the board a player who died into the intermission is owed.
        // `osp_r2dc` 2 is written by BeginIntermission for exactly that client
        // and this is its reader -- see p_hud.c's comment beside
        // the write named OSP_clientThink, which reads the value 1 and not this
        // one.  1.25 seconds, so the board arrives after the level-end board
        // rather than under it.
        if (G_IsOspRuleset() && client->resp.osp_r2dc == 2 &&
            level.framenum > level.intermission_framenum + 1.25f * BASE_FRAMERATE &&
            !(ent->flags & FL_BOT)) {
            client->resp.osp_r2dc = 0;
            DeathmatchScoreboard(ent);
        }

        // Tourney's intermission has two timers and neither was wired.
        // `nextlevel_click` (15s) is how long a press is ignored for, and
        // `nextlevel_lazy` -- registered under the donor's own spelling
        // `nextlevel_default`, 45s -- ends the intermission with no press at
        // all, which is what a server with nobody willing to click needs.  Both
        // were registered, one of them was even written by the map loop, and
        // nothing read either: an intermission ended on the first press after
        // baseq2's five seconds, or never.  A live match (`sync_stat` 4) waits
        // the full settings; anything else -- warmup, or a manual `map` -- takes
        // the donor's short 7 and 15.
        //
        // This is the PRESS half, and it is the OSP four's alone -- `dm` among
        // them because it is RegularDM here.  `arena`, `ctf` and `sp` fall past
        // to baseq2's five seconds below, deliberately in all three cases: the
        // defect was never how long a press is ignored for, so they keep the
        // shorter wait and take only the CLOCK (R-CTF-9, R-RA-10a).  RA2's own
        // intermission is five seconds and a BUTTON_ANY -- `port_ra2` carries
        // baseq2's rule untouched -- so `arena` reading this pair put fifteen
        // in front of a key press because that is tourney's default, not
        // because anything measured an arena and asked for it.
        //
        // The LAZY half is not here: it is G_CheckIntermissionExit()
        // in g_main.c, because a timer whose only caller is ClientThink cannot
        // fire on a server that has nobody to think for, and that server --
        // every client a bot, or the last one gone during the intermission --
        // is the one the timer exists for.  Measured on the live arena server:
        // 23.8 hours of a 26.9-hour uptime frozen at a level end, and of the
        // eleven map loads in it, one a container restart and every other one
        // inside half a minute of somebody being there to press a key.
        //
        // What stays tourney's is `warmup`.  `arena` has no pre-match state to
        // shorten the wait for, so it waits the setting.
        if (G_IntermissionOnTimer()) {
            float t = (level.framenum - level.intermission_framenum) /
                      (float)BASE_FRAMERATE;
            bool warmup = G_IsOspRuleset() && (sync_stat < 4 || manual_map);

            if (((t > nextlevel_click->value && (int)nextlevel_click->value) ||
                 (warmup && t > 7.0f)) && (ucmd->buttons & BUTTON_ANY)) {
                level.exitintermission = true;
                if (G_IsOspRuleset())
                    start_count = 0;
            }
            return;
        }

        // can exit intermission after five seconds
        if (level.framenum > level.intermission_framenum + 5.0f * BASE_FRAMERATE
            && (ucmd->buttons & BUTTON_ANY))
            level.exitintermission = true;
        return;
    }

    // The Gladiator menu is driven by the movement axes, so it has to see the
    // usercmd before pmove does and it edits the command it was given --
    // clearing the movement it consumed, the attack and use buttons, and the
    // gravity, so the cursor does not also walk the player off a ledge.  Ahead
    // of tourney's autocam for the same reason menu input is consumed first
    // everywhere else: whichever menu is open owns the keys.
    if (ent->client->menu_owner == MENU_BOT)
        bot_MenuThink(ent, ucmd);

    // Tourney's autocam is not a chase cam -- it picks its own subject and its
    // own position -- so it takes the whole frame, before either the chase-cam
    // branch or pmove.
    if (G_IsOspRuleset() && OSP_clientThink(ent, ucmd))
        return;

    // The Gladiator observer, in the place the donor put it -- after
    // the menus and before pmove, because it EDITS the command (it clears the
    // attack and use buttons, and in a camera mode the movement axes too) and
    // sets the pm_type pmove is about to run with.  It does not return: the
    // donor let the rest of ClientThink run on the edited command, which is
    // what makes a free-flying observer noclip rather than freeze.
    if (G_GladiatorObserver())
        DoObserver(ent, ucmd);

    if (ent->client->chase_target) {
        client->resp.cmd_angles[0] = SHORT2ANGLE(ucmd->angles[0]);
        client->resp.cmd_angles[1] = SHORT2ANGLE(ucmd->angles[1]);
        client->resp.cmd_angles[2] = SHORT2ANGLE(ucmd->angles[2]);

        // Tourney's chase camera has controls, and this is where they
        // arrive. ***  Four inputs, and the observer's own edict holds their
        // state because a frozen camera has no use for the fields: `speed` is
        // the zoom distance (seeded from `camera_depth` by OSP_ChaseCam),
        // `osp_t018` the free-look yaw offset, and `movedir` the pair
        // UpdateChaseCam adds to the target's view angles.
        //
        // The donor stores that yaw as a FLOAT punned into an int field
        // (`*(float *)&client->osp_t018`) and then takes `% 360` of it as an
        // int, so the value is always an integral number of degrees; it is kept
        // as the int it is declared to be, which is the same number without the
        // type punning.  Its `avelocity` write is not carried: the donor's own
        // comment calls it the frame-to-frame delta for a chained chasecam and
        // nothing in the donor ever reads it, so porting it would add exactly
        // the dead state this exists to find.
        if (G_IsOspRuleset()) {
            // The latch is computed here, as the donor's chase branch does,
            // because this runs before pmove and ClientLagThink -- which is
            // where the rest of the tree latches -- runs after it.  Reading the
            // latch without setting it reads the previous frame's, which
            // ClientBeginServerFrame has already cleared: measured as a chase
            // camera whose ATTACK did nothing at all.  ClientLagThink's own
            // `|=` then adds nothing, since `buttons & ~oldbuttons` is 0 once
            // this has run.
            client->oldbuttons = client->buttons;
            client->buttons = ucmd->buttons;
            client->latched_buttons = client->buttons & ~client->oldbuttons;

            if (ucmd->forwardmove < 0) {
                ent->speed += 1.0f;
            } else if (ucmd->forwardmove > 0) {
                // Floors at zero, which is the donor's post-decrement written
                // out: at 0 the test passes, the decrement runs and the clamp
                // puts it back.
                if (ent->speed <= 0)
                    ent->speed = 0;
                else
                    ent->speed -= 1.0f;
            }

            if (ucmd->sidemove > 0)
                client->osp_t018 = (client->osp_t018 + 4) % 360;
            else if (ucmd->sidemove < 0)
                client->osp_t018 = (client->osp_t018 - 4) % 360;

            if ((client->latched_buttons & BUTTON_ATTACK) &&
                client->resp.osp_r010 <= level.framenum) {
                if (G_MenuActive(ent)) {
                    Cmd_InvUse_f(ent);
                } else if (client->resp.osp_entered == ENTERED_CHASECAM) {
                    // ...and the mode cycle: chasecam -> in-eyes -> out.
                    gi.cprintf(ent, PRINT_HIGH, "Changing to IN-EYES mode.\n");
                    client->resp.osp_entered = ENTERED_INEYES;
                    OSP_Stats_PlayerMode(ent, "In-eyes");
                } else {
                    OSP_removeChaseCam(ent);
                }
                client->resp.osp_r010 = level.framenum + 2;
                client->latched_buttons &= ~BUTTON_ATTACK;
            } else if (ucmd->upmove && !G_MenuActive(ent) &&
                       client->resp.osp_r010 <= level.framenum) {
                ChaseNext(ent);
                client->resp.osp_r010 = level.framenum + 8;
            }
        }
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

        // Entered but not yet placed, which is a state of its own and
        // the donor freezes it.  Tourney's join commands do not place anybody:
        // they set `osp_entered` and clear `osp_r240`, and the respawn below is
        // what turns that into a body on the next think.  For the frame or two
        // in between -- and for as long as a refused spawn keeps retrying -- the
        // client must not be able to walk, which is what this override is.
        if (G_IsOspRuleset() &&
            client->resp.osp_entered == ENTERED_ENTERED &&
            !client->resp.osp_r240)
            client->ps.pmove.pm_type = PM_FREEZE;

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
                // PM_GIB, and it is the donor's literal `3` restored.
                //
                // This port read `pm_type = 3` as "stop this client moving" and
                // wrote PM_FREEZE, which is 4.  They are not the same answer:
                // pmove returns early for PM_FREEZE -- `// no movement at all`,
                // before PM_CheckDuck and everything after it -- while PM_GIB
                // only narrows the bounding box and then falls through friction,
                // air move and step-slide like any other type.
                //
                // Which matters because the camera's only integrator is pmove.
                // track_think has two branches: when the line to the goal is
                // blocked it teleports the camera and zeroes the velocity, and
                // when the line is clear it sets `velocity = (goal - here) * 10`
                // and expects pmove to cover the distance.  Under PM_FREEZE that
                // velocity was never consumed, so the clear-path branch moved the
                // camera not at all and the blocked branch -- the escape hatch --
                // was the only thing that ever moved it.  The trackcam did not
                // follow; it snapped when something got in the way.  Not by
                // sitting still, which is worth knowing before measuring it: the
                // blocked branch fires often enough on a real map that the
                // pre-fix camera still covered 1837 units in a sample, in jumps.
                // What was wrong is that it stalled exactly when the path was
                // clear.
                //
                // SV_Physics_Noclip is indeed never reached for a client edict,
                // which is what the earlier note here reasoned from.  It was
                // never the integrator.
                client->ps.pmove.pm_type = PM_GIB;
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

        // The aimbot detector, which watches the view-angle deltas in
        // the usercmd for the signature a ZBOT leaves.  It returns true when it
        // has KICKED the client, and the frame ends there rather than running a
        // pmove for an edict that is on its way out -- the corrupt unicast
        // OSP reported came from doing it the other way round.
        if (G_IsOspRuleset() && bot_watch &&
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

        // RA2 adds `fightstate == FIGHT_ALIVE` here, and it is the same
        // thought as the landing sound g_phys.c already gates -- an observer is
        // a body in the room that nobody is supposed to hear.  It matters in
        // the lobby rather than in a live arena: move_to_arena() puts observers
        // in an ACTIVE arena on OMODE_FREEFLYING, which is PM_SPECTATOR and
        // never raises PMF_JUMP_HELD, while arena 0 leaves them walking.
        //
        // Ruleset-gated because `fightstate` is FIGHT_SPECTATING for every
        // client under every other ruleset, where it means nothing.
        if (~client->ps.pmove.pm_flags & pm.s.pm_flags & PMF_JUMP_HELD && pm.waterlevel == 0 &&
            (G_Ruleset() != RULESET_ARENA ||
             client->resp.fightstate == FIGHT_ALIVE)) {
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
        // One field, two pulls.  Threewave's hook and tourney's are the same
        // mechanic with different physics -- tourney's is faster, does its own
        // damage and lets go on its own timer -- so one implementation is kept
        // `ctf_grapple` edict and the ruleset chooses which one moves the
        // player on it.  A single pull with tunables would have to reconcile
        // two different state machines rather than one.
        if (client->ctf_grapple) {
            if (G_IsOspRuleset())
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

    if (G_Ruleset() == RULESET_ARENA) {
        // The latch clear is the donor's own separate statement rather than an
        // arm of the chain below, because the chain has a second arm now and a
        // released key has to reach this whichever of them ran.
        if (ucmd->upmove == 0)
            client->resp.omode_buttons &= ~2;

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
        // RA2's one map-specific hack, and it is an ESCAPE HATCH.
        // `ra2map13` arena 1 has somewhere above z 388 that a player can reach
        // and cannot leave, so crouching up there kills them.  The merge
        // dropped it.  `kill` is live here where it is not in RA2, so
        // the player was not actually trapped -- but a 1999 client is bound for
        // crouch and not for `kill`, and this is the way out the map was built
        // with.  `ra2map13` is in the shipped maploop.
        //
        // Kept exactly as the donor has it, map name and constant included:
        // this is a fact about one BSP, not a rule, and generalising it would
        // be inventing a rule RA2 does not have.
        } else if (ucmd->upmove < 0 && client->resp.context == 1 &&
                   ent->s.origin[2] >= 388 &&
                   !Q_stricmp(level.mapname, "ra2map13")) {
            T_Damage(ent, ent, ent, vec3_origin, ent->s.origin, vec3_origin,
                     100000, 0, DAMAGE_NO_PROTECTION, MOD_TELEFRAG);
        }
    }

    // The ping, inactivity and framerate rules, from the position the
    // pmove above just settled on.  True means the client has been
    // disconnected or moved to observer and nothing below may touch it.
    //
    // Not for a client on a chase cam: the donor's block sits inside the arm
    // that ran a pmove, and the chase arm returns before reaching it.  A
    // spectator is not holding a match up and is not worth kicking for ping.
    if (G_IsOspRuleset() && !client->chase_target &&
        OSP_clientPolice(ent, ucmd))
        return;

    // Everything from here to the end of ClientLagThink is what the
    // lag simulation REPLAYS.  It is the button half -- the latch, the light
    // level, the shot and the observer's jump-cycle -- and it is the half that
    // has to happen at the position and view angles the player had `delay`
    // milliseconds ago.  pmove is deliberately not in it: a lagged player still
    // walks in real time, which is the point of simulating the lag rather than
    // simply stalling the client.
    if (g_clientlag->value && !(ent->flags & FL_BOTCLIENT)) {
        usercmd_t laggeducmd;
        vec3_t v_angle, origin;

        VectorCopy(ent->s.origin, origin);
        VectorCopy(client->v_angle, v_angle);
        Lag_StoreClientInput(ent, ucmd, ent->s.origin, client->v_angle);
        while (Lag_GetClientInput(ent, &laggeducmd, ent->s.origin, client->v_angle))
            ClientLagThink(ent, &laggeducmd);
        VectorCopy(v_angle, client->v_angle);
        VectorCopy(origin, ent->s.origin);
    } else {
        ClientLagThink(ent, ucmd);
    }

    // And this is how a tourney client gets a body.
    //
    // None of the six paths that enter the game place anybody: `join`, the team
    // join, the team menus, the 1v1 queue, a bot's join and a recovered seat all
    // set `osp_entered` to ENTERED_ENTERED and `osp_r240` to 0, and the donor
    // lets the next think notice the pair and respawn.  One trigger serves all
    // six, and it serves a seventh case as well -- a placement that was refused
    // leaves `osp_r240` 0, so the retry is the same line.
    //
    // The merge dropped it along with every write of `osp_r240` 2, which is why
    // the two halves hid each other: with the flag never set, `!osp_r240` was
    // always true and a trigger on its own would have respawned every client on
    // every frame.  Both halves or neither.
    //
    // Not inside ClientLagThink, which is the half the lag simulation replays once per
    // delayed command: a respawn is a placement rather than an input, and the
    // donor's own position for it -- after the buttons, before the ping
    // sampler -- is this one relative to pmove.
    if (G_IsOspRuleset() &&
        client->resp.osp_entered == ENTERED_ENTERED &&
        !client->resp.osp_r240)
        respawn(ent);

    // CTF regeneration tech.  A no-op without it.
    CTFApplyRegeneration(ent);

    // Tourney's regeneration rune is the same concept on the same frame.
    if (G_IsOspRuleset() && (rune_stat & RUNE_REGEN))
        OSP_runesApplyRegeneration(ent);

    // The offhand hook fires from here rather than from a weapon think, which
    // is what makes it offhand.  Under arena the switch is not
    // `ctf_hook` -- which is registered for every ruleset and would never say
    // no -- but arena.cfg's `grapple:` key, and there is a second condition on
    // it besides.  RA2 asks both in its own Think_Weapon; this is that block.
    if (G_Ruleset() == RULESET_ARENA)
        RA_HookThink(ent);
    else
        CTFHookThink(ent);

    // update chase cam if being followed
    for (i = 1; i <= game.maxclients; i++) {
        other = g_edicts + i;
        if (other->inuse && other->client->chase_target == ent)
            UpdateChaseCam(other);
    }

    // A menu redraw this frame's input earned, rate-limited by the
    // engine rather than by how fast the player presses the key.
    if (client->menudirty && client->menutime <= level.time) {
        // Two engines share the pair, because one menu is open at a time
        // and a per-engine `menutime` would be a second answer to
        // the question `menu_owner` already answers.  MENU_ARENA is absent on
        // purpose: it repaints CS_STATUSBAR from its own MenuThink cadence and
        // never marks this flag.
        if (G_MenuActive(ent)) {
            if (client->menu_owner == MENU_CTF) {
                ctf_PMenu_Do_Update(ent);
                gi.unicast(ent, true);
            } else if (client->menu_owner == MENU_TOURNEY) {
                osp_PMenu_Do_Update(ent);
                gi.unicast(ent, true);
            }
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

    // baseq2's spectator toggle. Not under an OSP ruleset: the donor reused
    // `pers.spectator` as a speed-cheat counter, while this merged tree keeps
    // the base spectator flag and carries that counter as osp_speedstrikes.
    // Tourney's own observer path is `observe` / OSP_startObserve.
    if (!G_IsOspRuleset() && deathmatch->value &&

        client->pers.spectator != client->resp.spectator &&

        (level.framenum - client->respawn_framenum) >= 5 * BASE_FRAMERATE) {

        spectator_respawn(ent);

        return;

    }

    // run weapon animations if it hasn't been done by a ucmd_t
    //
    // "Is this client watching" and "does this client have a body" are not
    // The same question under tourney, and only the second one may open the
    // weapon. ***  The donor asks `resp.osp_r240 == 2` here -- placed as a body
    // -- and G_IsObserver() asks `osp_entered != ENTERED_ENTERED`, which is
    // "has not joined".  They agree for every client except the one state that
    // is both: a player whose placement was refused.  SelectSpawnPoint
    // says no when every spawn point on the map has somebody standing on it, and
    // PutClientInServer then returns early leaving that client MOVETYPE_NOCLIP,
    // SOLID_NOT, SVF_NOCLIENT and PM_FREEZE -- invisible, intangible, exactly an
    // observer to look at -- with `osp_entered` ENTERED_ENTERED, so this test
    // called it a player and ran its weaponthink.  The early return also skips
    // the `gunindex` write and the ChangeWeapon at the end of the function, so
    // the view model it had before is still on its screen: a client that is an
    // observer in every visible respect, holding a weapon, able to fire it.
    //
    // The retry at the end of ClientThink shortens the window to whatever a
    // full map lasts, which is why it is a narrow bug rather than a permanent
    // one -- and is not a reason to leave the gate reading the wrong field.
    if (!client->weapon_thunk &&
        (G_IsOspRuleset() ? client->resp.osp_r240 == 2 : !G_IsObserver(ent)))
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

            // `respawn_delay` is the term the import dropped from the
            // forced-respawn arm -- upstream's `p_client.c:2625` has it and
            // ours did not, so the cvar was registered and read by nobody.
            // It answers 0 outside the OSP rulesets, and 0 makes this test the
            // same one as the `if` above, so nothing changes by default.
            //
            // Only the DF_FORCE_RESPAWN arm is delayed.  CTF's match respawn
            // and the arena's are not the donor's and have no such cvar.
            //
            // So the first arm makes this cvar invisible to a bot.
            // A dead player who presses attack respawns through
            // `latched_buttons` no matter what the delay is, and a bot holds
            // attack -- which is why no bot test can measure `respawn_delay`
            // and why no such test is claimed.  The cvar's whole purpose is
            // the player who does not press a button, which is the case
            // DF_FORCE_RESPAWN exists for.  Upstream reads the same way.
            int forced_wait = G_IsOspRuleset() ? OSP_forcedRespawnDelay() : 0;

            if ((client->latched_buttons & buttonMask) ||
                (deathmatch->value && ((int)dmflags->value & DF_FORCE_RESPAWN) &&
                 level.framenum > client->respawn_framenum + forced_wait) ||
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

    // OSP's speed-cheat sampler, last in the frame because it can end with
    // ClientDisconnect(ent) -- nothing below may then touch the edict.
    //
    // All five terms matter, and the first is the schedule.  OSP_speedDetect
    // opens with a reliable `cmd _init_state $timescale` stufftext and ends by
    // re-arming itself at `level.framenum + rand(0..30) + 200` -- twenty-odd
    // seconds -- so `osp_r2b4 == level.framenum` is what makes it a sample
    // rather than a broadcast.  Called under the ruleset gate alone it stuffed
    // a console command into every client ten times a second (measured: 30 in
    // three seconds), and `osp_r2b4` was written and read by nobody.
    // `osp_r2b8 == 16` is "not being watched": set for a bot, and for a client
    // that connected while `bot_watch` was off.
    if (G_IsOspRuleset() &&
        client->resp.osp_r2b4 == level.framenum &&
        client->resp.osp_r2b8 != 16 &&
        client->resp.osp_entered == ENTERED_ENTERED &&
        bot_watch &&
        !(ent->flags & FL_BOTCLIENT))
        OSP_speedDetect(ent);
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
