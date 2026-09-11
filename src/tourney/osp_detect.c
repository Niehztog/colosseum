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
// address comments are stripped.  osp_detect.c -- filename assigned by this
// tree.  Aim-bot and speed-cheat detection.
//
// OSP_botDetect is a ZBOT detector.  It watches the view-angle deltas in
// consecutive usercmd_t frames around an attack: a human's aim drifts between
// the frame before a shot and the frame of the shot, an aim-bot's snaps and
// then is bit-for-bit identical, so a delta of exactly zero on the release
// frame -- twice in a row -- is the tell.  It also flags the two fixed
// signatures ZBOT leaves behind: a non-zero `impulse` in a movement command,
// and the sentinel yaw 0x3f49.

#include "g_local.h"
#include "tourney/osp_types.h"
#include "tourney/osp_stats.h"

void ClientDisconnect(edict_t *ent);

// File statics.  The loop counter really is a static in the original, and the
// DECLARATION ORDER is read off real's .bss run, which lays these out in
// exactly this sequence -- including a 4-byte object between zb_delta and
// zb_attack that nothing in the image references.  Only that object's
// existence and size are evidence; its name and type are reconstructed.
static int          zb_i;
static float        zb_delta[2];
static q_unused int zb_unused;
static byte         zb_attack;
static float        zb_dist;
static gclient_t    *zb_client;

bool OSP_botDetect(edict_t *ent, usercmd_t *ucmd)
{
    char    why[32];

    zb_client = ent->client;

    if (zb_client->resp.osp_entered != ENTERED_ENTERED ||
        zb_client->ping > 500 ||
        zb_client->osp_t024 == level.framenum ||
        zb_client->resp.osp_r07c[0])
        return false;

    // a movement command never carries an impulse
    if (ucmd->impulse) {
        OnBotDetection(ent, "i");
        return true;
    }

    zb_attack = ucmd->buttons & BUTTON_ATTACK;
    // Written `^`, not `!=`: both operands are 0 or BUTTON_ATTACK, so the XOR
    // is the edge-detect spelling.
    if (zb_attack ^ zb_client->osp_t01c[0]) {
        zb_client->osp_t01c[0] = zb_attack;

        if (zb_attack || !(zb_client->osp_t020 < 39000)) {
            if (!ucmd->msec || (zb_attack && abs(ucmd->angles[0]) == 0x3f49)) {
                zb_client->osp_t020 = 0;
            } else {
                for (zb_i = 0; zb_i < 2; zb_i++) {
                    zb_delta[zb_i] = (float)(ucmd->angles[zb_i] - zb_client->osp_t028[zb_i]);
                    if (zb_delta[zb_i] > 32768)
                        zb_delta[zb_i] -= 65536;
                    else if (zb_delta[zb_i] < -32768)
                        zb_delta[zb_i] += 65536;
                }

                zb_dist = (zb_delta[0] * zb_delta[0] + zb_delta[1] * zb_delta[1]) / ucmd->msec;

                if (zb_attack) {
                    zb_client->osp_t020 = zb_dist;
                    return false;
                }

                if (zb_dist <= 0) {
                    zb_client->osp_t034[0]++;
                    zb_client->osp_t020 = 0;
                    zb_client->osp_t024 = level.framenum;
                    if (zb_client->osp_t034[0] >= 2) {
                        Q_snprintf(why, sizeof(why), "r (%f)", zb_dist);
                        OnBotDetection(ent, why);
                        return true;
                    }
                }
            }
        }
    }

    if (!zb_attack)
        for (zb_i = 0; zb_i < 2; zb_i++)
            zb_client->osp_t028[zb_i] = ucmd->angles[zb_i];

    return false;
}

void OnBotDetection(edict_t *ent, char *why)
{
    ent->client->resp.osp_r07c[0] = 1;
    ent->client->resp.score = -99;
    OSP_Stats_BotDetect(ent, why);
    gi.bprintf(PRINT_HIGH, "%s was kicked for using a BOT!\n",
               ent->client->pers.netname);

    if (server_log) {
        OSP_logAdminLog("BotDetect: %s (%s) [%s]", ent->client->pers.netname,
                        why, ent->client->pers.address);
    }

    ent->movetype = MOVETYPE_NOCLIP;
    ent->client->osp_t034[0] = 0;
    gi.WriteByte(svc_disconnect);
    gi.unicast(ent, true);
    ClientDisconnect(ent);
}

void OSP_speedDetect(edict_t *ent)
{
    // FUNCTION-scope, although only the `else` uses it: real's PE gives it the
    // SHALLOWEST slot, and MSVC lays every nested block's locals out below all
    // the function-scope ones.
    int     when;

    gi.WriteByte(svc_stufftext);
    gi.WriteString("cmd _init_state $timescale\n");
    gi.unicast(ent, true);

    if (ent->client->pers.osp_speedstrikes >= 3) {
        gi.centerprintf(ent, "Speed cheating not allowed!\n");
        gi.bprintf(PRINT_HIGH, "%s was kicked for SPEED CHEATING!\n",
                   ent->client->pers.netname);

        if (server_log) {
            OSP_logAdminLog("SpeedDetect: %s [%d]", ent->client->pers.netname,
                            ent->client->pers.osp_speedstrikes);
        }

        ent->movetype = MOVETYPE_NOCLIP;
        ent->client->osp_t034[0] = 0;
        gi.WriteByte(svc_disconnect);
        gi.unicast(ent, true);
        ClientDisconnect(ent);
    } else {
        // The + 200 belongs to `when`'s own initialiser; the store adds
        // level.framenum to it separately.
        when = (int)((Q_rand() & 0x7fff) / 32767.0f * 30.0f) + 200;
        ent->client->resp.osp_r2b4 = level.framenum + when;
    }
}

void OSP_speedCheat_cmd(edict_t *ent)
{
    if (Q_atoi(gi.argv(1)) > 1) {
        ent->client->pers.osp_speedstrikes++;
        gi.dprintf("Speed > 1!!! (%d)\n", Q_atoi(gi.argv(1)));
    }
}
