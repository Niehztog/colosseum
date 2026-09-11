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
// OSP Tourney DM v2.75, from osp-tourney@1d8427e.
// Donor-only: baseq2 has no counterpart, so it lives in src/tourney/ rather
// than being merged into a spine file.  The reconstruction's asm-matching
// address comments are stripped.  stdlog.c -- filename assigned by this tree.
// Standard Log 1.2 writer.

#include "g_local.h"
#include "tourney/osp_types.h"

cvar_t  *sl_log_logbots;
int     sl_status;
cvar_t  *sl_log_style;
cvar_t  *sl_filename;
cvar_t  *sl_log_flush;
cvar_t  *sl_log_method;

static int      sl_started = 0;
static char     *sl_patch = NULL;

int sl_Logging(game_import_t *import, char *patch)
{
    int status;

    status = sl_OpenLogFile(import);
    sl_log_logbots = import->cvar("sl_log_logbots", "1", 0);

    if (status && !sl_started) {
        sl_patch = patch;
        sl_started = status;
    }

    return status;
}

void sl_GameStart(game_import_t *import, level_locals_t level)
{
    // No `patch` local anywhere in this family: every one of these functions
    // passes `sl_patch` straight into sl_Logging and wraps its whole body in
    // `if (sl_Logging (...))`.
    cvar_t      *flags;

    if (sl_Logging(import, sl_patch)) {
        flags = import->cvar("dmflags", "0", CVAR_SERVERINFO);
        sl_LogVers(import);
        sl_LogPatch(import, sl_patch);
        sl_LogDate(import);
        sl_LogTime(import);
        sl_LogDeathFlags(import, (unsigned long)flags->value);
        sl_LogMapName(import, level.level_name);
        sl_LogGameStart(import, level.time);
    }
}

void sl_GameEnd(game_import_t *import, level_locals_t level)
{
    if (sl_Logging(import, sl_patch)) {
        sl_LogGameEnd(import, level.time);
        sl_CloseLogFile();
        sl_started = 0;
    }
}

void sl_WriteStdLogDeath(game_import_t *import, level_locals_t level,
                         edict_t *targ, edict_t *inflictor, edict_t *attacker)
{
    int     mod;
    // The first field of a StdLog score record is the player whose score
    // changed and the second is the other party, so `scorer` is the attacker
    // on a kill and the victim on a suicide.  v2.75 called them `victim` and
    // `killer`, which is backwards for the kill case.
    char    *scorer;
    char    *other;
    char    *event;
    char    *weapon;
    int     score;
    int     ping;
    int     suicide;

    if (!targ->client)
        return;

    if (sl_log_logbots && !(int)sl_log_logbots->value &&
        ((targ->flags & FL_BOT) || (attacker && (attacker->flags & FL_BOT))))
        return;

    if (deathmatch->value != 0 && sl_Logging(import, sl_patch)) {
        mod = meansOfDeath & ~MOD_FRIENDLY_FIRE;
        scorer = NULL;
        other = NULL;
        event = NULL;
        // Real initialises `weapon` here too, between event and score.
        weapon = NULL;
        score = 0;
        ping = -1;

        if (attacker == targ) {
            scorer = attacker->client->pers.netname;
            ping = attacker->client->ping;
            event = "Suicide";
            score = -1;
            // A ternary: it materialises into its own frame temp and is then
            // copied into weapon's slot.
            weapon = attacker->client->pers.weapon
                     ? attacker->client->pers.weapon->pickup_name : NULL;
        } else {
            suicide = 0;
            weapon = "UNKNOWN";

            switch (mod) {
            case MOD_FALLING:
                weapon = "Fell";
                suicide = 1;
                break;
            case MOD_CRUSH:
                weapon = "Crushed";
                suicide = 1;
                break;
            // Here and not in the weapon switch below, which is where
            // reviewing this diff found it.  dm_ball.c raises MOD_DBALL_CRUSH
            // from `T_Damage(other, ent, ent, ...)` where `ent` is the ball, so
            // the attacker is not a client, the weapon switch below cannot be
            // reached, and a row there would have been as dead as the
            // `ionrippergun` lookup this entry set out to fix.  It is a world
            // death that costs the victim a point, like every case around it,
            // and ClientObituary does not name it either.
            case MOD_DBALL_CRUSH:
                weapon = "DBall";
                suicide = 1;
                break;
            case MOD_WATER:
                weapon = "Drowned";
                suicide = 1;
                break;
            case MOD_SLIME:
                weapon = "Melted";
                suicide = 1;
                break;
            case MOD_LAVA:
                weapon = "Lava";
                suicide = 1;
                break;
            case MOD_EXPLOSIVE:
            case MOD_BARREL:
            case MOD_BOMB:
                weapon = "Explosion";
                suicide = 1;
                break;
            case MOD_TARGET_LASER:
                weapon = "Lasered";
                suicide = 1;
                break;
            case MOD_TARGET_BLASTER:
                weapon = "Blasted";
                suicide = 1;
                break;
            case MOD_SUICIDE:
            case MOD_EXIT:
            case MOD_SPLASH:
            case MOD_TRIGGER_HURT:
                suicide = 1;
                break;
            }

            if (suicide) {
                scorer = targ->client->pers.netname;
                ping = targ->client->ping;
                event = "Suicide";
                score = -1;
            }
        }

        if (!scorer || !event) {
            if (attacker && attacker->client) {
                weapon = "UNKNOWN";
                switch (mod) {
                case MOD_BLASTER:
                    weapon = "Blaster";
                    break;
                case MOD_SHOTGUN:
                    weapon = "Shotgun";
                    break;
                case MOD_SSHOTGUN:
                    weapon = "Super Shotgun";
                    break;
                case MOD_MACHINEGUN:
                    weapon = "Machinegun";
                    break;
                case MOD_CHAINGUN:
                    weapon = "Chaingun";
                    break;
                case MOD_GRENADE:
                case MOD_G_SPLASH:
                    weapon = "Grenade Launcher";
                    break;
                case MOD_HANDGRENADE:
                case MOD_HG_SPLASH:
                case MOD_HELD_GRENADE:
                    weapon = "Grenades";
                    break;
                case MOD_ROCKET:
                case MOD_R_SPLASH:
                    weapon = "Rocket Launcher";
                    break;
                case MOD_HYPERBLASTER:
                    weapon = "HyperBlaster";
                    break;
                case MOD_RAILGUN:
                    weapon = "Railgun";
                    break;
                case MOD_BFG_LASER:
                case MOD_BFG_BLAST:
                case MOD_BFG_EFFECT:
                    weapon = "BFG10K";
                    break;
                case MOD_GRAPPLE:
                    weapon = "Grappling Hook";
                    break;

                // The content layers, which are valid with every ruleset
                // -- so a Reckoning or Ground Zero kill was reaching this table and falling
                // out of it as "UNKNOWN".  The names are the items' own pickup names, which
                // is what `acc_names[]` in the accuracy record already uses, so a consumer
                // joining the two records matches on one spelling.
                //
                // The Disruptor's two MODs share a name the way the BFG's three do.  The
                // monsters' own -- MOD_BRAINTENTACLE, MOD_BLASTOFF, MOD_GEKK, MOD_BLASTER2
                // -- are deliberately absent: the donor names no monster attack either, and
                // a monster is not a weapon somebody chose.
                // Xatrix
                case MOD_RIPPER:
                    weapon = "Ionripper";
                    break;
                case MOD_PHALANX:
                    weapon = "Phalanx";
                    break;
                case MOD_TRAP:
                    weapon = "Trap";
                    break;
                // Ground Zero
                case MOD_ETF_RIFLE:
                    weapon = "ETF Rifle";
                    break;
                case MOD_PROX:
                    weapon = "Prox Launcher";
                    break;
                case MOD_HEATBEAM:
                    weapon = "Plasma Beam";
                    break;
                case MOD_CHAINFIST:
                    weapon = "Chainfist";
                    break;
                case MOD_TESLA:
                    weapon = "Tesla";
                    break;
                case MOD_TRACKER:
                case MOD_DISINTEGRATOR:
                    weapon = "Disruptor";
                    break;
                case MOD_NUKE:
                    weapon = "A-M Bomb";
                    break;
                case MOD_VENGEANCE_SPHERE:
                    weapon = "Vengeance Sphere";
                    break;
                case MOD_HUNTER_SPHERE:
                    weapon = "Hunter Sphere";
                    break;
                case MOD_DEFENDER_SPHERE:
                    weapon = "Defender Sphere";
                    break;
                case MOD_DOPPLE_EXPLODE:
                case MOD_DOPPLE_VENGEANCE:
                case MOD_DOPPLE_HUNTER:
                    weapon = "Doppleganger";
                    break;
                case MOD_TELEFRAG:
                    weapon = "Telefrag";
                    break;
                }

                other = targ->client->pers.netname;
                scorer = attacker->client->pers.netname;
                ping = attacker->client->ping;
                event = "Kill";
                score = 1;

                if (G_Ruleset() == RULESET_TDM &&
                    attacker->client->resp.team == targ->client->resp.team)
                    score = -1;
            }
        }

        sl_LogScore(import, scorer, other, event, weapon, score, level.time,
                    ping);
        return;
    }

    sl_LogScore(import, "", "", "ERROR", "", 0, level.time, -1);
}

void sl_WriteStdLogPlayerEntered(game_import_t *import, level_locals_t level,
                                 edict_t *ent)
{
    if (sl_Logging(import, sl_patch)) {
        if ((int)sl_log_logbots->value || !(ent->flags & FL_BOT))
            sl_LogPlayerConnect(import, ent->client->pers.netname, 0,
                                level.time);
    }
}

void sl_LogPlayerDisconnect(game_import_t *import, level_locals_t level,
                            edict_t *ent)
{
    if (sl_Logging(import, sl_patch)) {
        if ((int)sl_log_logbots->value || !(ent->flags & FL_BOT))
            sl_LogPlayerLeft(import, ent->client->pers.netname, level.time);
    }
}

void sl_SoftGameEnd(game_import_t *import, level_locals_t level)
{
    if (sl_Logging(import, sl_patch))
        sl_LogGameEnd(import, level.time);
}
