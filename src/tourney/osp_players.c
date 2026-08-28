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
// osp_players.c -- <INVENTED FILENAME>. The three scoreboard renderers and
// the team-chat command.
//
// Four named functions plus four file-statics that have no symbol and whose
// names here are therefore <INVENTED>: sayteam_location (id CTF's
// CTFSay_Team_Location), sayteam_armor (CTF's CTFSay_Team_Armor),
// sayteam_sight (the mod's own, %n) and loc_findradius (CTF's, unchanged).
//
// OSP_sayteam_cmd is id CTF's CTFSay_Team with the %-expansion table rebuilt:
// CTF's l/a/h/t/w survive, the mod adds %n, and %t -- CTF's tech report --
// now reports RUNES instead.  %r and %t are two separate case bodies with
// identical contents.

#include "g_local.h"
#include "tourney/osp_types.h"

static edict_t *loc_findradius(edict_t *from, vec3_t org, float rad);

// <INVENTED NAME>.  Counts the members of team `team`, used only by the
// scoreboard renderers' "# Players: %i" cell.  Deliberately written in
// OSP_teamCount's exact shape -- the real PE link folds the two bodies onto
// one address.
static int OSP_countTeamPlayers(int team)
{
    int         i;
    int         count;

    count = 0;
    for (i = 1; i <= game.maxclients; i++) {
        // Must stay byte-identical to OSP_teamCount -- see the note above.
        if (!g_edicts[i].inuse || !g_edicts[i].client ||
            g_edicts[i].client->resp.team != team)
            continue;

        count++;
    }
    return count;
}

// The team scoreboard, small-roster variant.  With more than six players in
// the game it hands straight over to OSP_showBIGTeamScores, which is the same
// board without the efficiency column.
//
// Each team gets a `client 80` banner card, a column header and up to FOUR
// rows; row 3 is a "...and you" promotion that swaps the viewer in when he is
// on that team and ranked below the visible four, exactly as OSP_showScores
// does at row 9.  The two osp_teams' sorted lists live in one pair of 2-D arrays
// and are built here rather than by OSP_DoRankSort, but with the same three
// keys: score descending, then deaths and suicides ascending.
void OSP_showTeamScores(edict_t *ent)
{
    int         rank[2][256];
    int         pscore[2][256];
    int         viewers[256];
    char        rowline[200];
    char        str[256];
    char        temp[1024];
    char        buf[1400];
    char        time[32];
    int         tarr[2];
    int         count[2];
    int         i;
    int         eff;
    int         nframes;
    int         m;
    int         y;
    int         sideno;
    int         kk;
    int         basey;
    int         cscore;
    int         obscount;
    int         size;
    gclient_t   *cl;
    edict_t     *player;

    y = 0;
    obscount = 0;
    size = 0;

    if (active_clients > 6) {
        OSP_showBIGTeamScores(ent);
        return;
    }

    tarr[0] = 0;
    tarr[1] = 1;

    for (sideno = 0; sideno < 2; sideno++) {
        count[sideno] = 0;

        for (i = 0; i <= game.maxclients; i++) {
            player = g_edicts + 1 + i;

            if (!player->inuse || !player->client)
                continue;

            // observers are collected once, on the first team's pass
            if (!sideno && player->client->resp.osp_entered != ENTERED_ENTERED) {
                viewers[obscount] = i;
                obscount++;
                continue;
            }

            if (player->client->resp.team != tarr[sideno])
                continue;

            cscore = game.clients[i].resp.score;

            for (kk = 0; kk < count[sideno]; kk++) {
                if (cscore > pscore[sideno][kk])
                    break;
                if (cscore == pscore[sideno][kk]) {
                    if (game.clients[i].resp.osp_r014 < game.clients[rank[sideno][kk]].resp.osp_r014)
                        break;
                    if (game.clients[i].resp.osp_r014 == game.clients[rank[sideno][kk]].resp.osp_r014 &&
                        game.clients[i].resp.osp_r2c0 < game.clients[rank[sideno][kk]].resp.osp_r2c0)
                        break;
                }
            }

            for (m = count[sideno]; m > kk; m--) {
                rank[sideno][m] = rank[sideno][m - 1];
                pscore[sideno][m] = pscore[sideno][m - 1];
            }

            rank[sideno][kk] = i;
            pscore[sideno][kk] = cscore;
            count[sideno]++;
        }

        for (i = 0; i < count[sideno]; i++) {
            player = g_edicts + 1 + rank[sideno][i];
            player->client->resp.osp_r208 = i + 1;
        }
    }

    buf[0] = 0;

    if (level.intermission_framenum != 0)
        ent->client->ps.stats[27] = OSP_CS(10);
    else
        ent->client->ps.stats[27] = OSP_CS(9);

    size = strlen(buf);
    basey = 0;

    for (sideno = 0; sideno < 2; sideno++) {
        if (count[sideno] > 4)
            count[sideno] = 4;

        for (i = 0; i < count[sideno]; i++) {
            cl = game.clients + rank[sideno][i];
            player = g_edicts + 1 + rank[sideno][i];
            y = basey + i * 8;

            if (i == 3 && ent->client->resp.team == sideno &&
                ent->client->resp.osp_r208 > 4) {
                player = ent;
                cl = ent->client;
                y += 2;
                i = cl->resp.osp_r208 - 1;
            }

            if (cl->resp.enterframe < sync_frame)
                nframes = level.framenum - sync_frame + 1;
            else
                nframes = level.framenum - cl->resp.enterframe + 1;

            if (nframes < 1)
                nframes = 1;

            if (cl->resp.score < 1)
                eff = 0;
            else if (!cl->resp.osp_r014 ||
                     !(cl->resp.osp_r014 + cl->resp.score))
                eff = 100;
            else
                eff = cl->resp.score * 100 /
                      (cl->resp.score + cl->resp.osp_r014);

            // The team card is emitted once, above the first rowline.
            if (!i) {
                Q_snprintf(rowline, sizeof(rowline), "%i", osp_teams[tarr[sideno]].osp_m0f8);
                for (m = 0; m < strlen(rowline); m++)
                    rowline[m] += 128;

                if (level.intermission_framenum == 0 || sideno) {
                    Q_snprintf(temp, sizeof(temp),
                               "client 80 %i %i %i %i %i xv 112 picn tag1 xv 114 string \"%s\""
                               "yv %i string2 \"Score: %s\"yv %i string2 \"Blunders: %i\""
                               "yv %i string2 \"# Players: %i\"",
                               basey - 16, rank[sideno][i], 0, 0, 0,
                               osp_teams[tarr[sideno]].netname, basey - 8, rowline,
                               basey, osp_teams[tarr[sideno]].osp_m104 + osp_teams[tarr[sideno]].osp_m108,
                               basey + 8, OSP_countTeamPlayers(tarr[sideno]));

                    y += 26;
                    basey += 26;
                } else {
                    OSP_getDateInfo(time);

                    if (manual_map == 1)
                        Q_snprintf(str, sizeof(str), "[ Voted map change ]");
                    else if (manual_map == 2)
                        Q_snprintf(str, sizeof(str), "[ Voted server config change ]");
                    else if (osp_teams[0].osp_m124 == 1)
                        Q_snprintf(str, sizeof(str), "[ %s defeats %s: %d to %d ]",
                                osp_teams[0].greenname, osp_teams[1].greenname,
                                osp_teams[0].osp_m0f8, osp_teams[1].osp_m0f8);
                    else if (osp_teams[1].osp_m124 == 1)
                        Q_snprintf(str, sizeof(str), "[ %s defeats %s: %d to %d ]",
                                osp_teams[1].greenname, osp_teams[0].greenname,
                                osp_teams[1].osp_m0f8, osp_teams[0].osp_m0f8);
                    else
                        Q_snprintf(str, sizeof(str), "[ Tied match! (%d to %d) ]",
                                osp_teams[1].osp_m0f8, osp_teams[0].osp_m0f8);

                    Q_snprintf(temp, sizeof(temp),
                               "client 80 %i %i %i %i %i xv 112 picn tag1 xv 114 string \"%s\""
                               "yv %i string2 \"Score: %s\"yv %i string2 \"Blunders: %i\""
                               "yv %i string2 \"# Players: %i\""
                               "xv 0 yv -43 cstring2 \"%s\"yv -25 cstring2 \"%s\"",
                               basey - 16, rank[sideno][i], 0, 0, 0,
                               osp_teams[tarr[sideno]].netname, basey - 8, rowline,
                               basey, osp_teams[tarr[sideno]].osp_m104 + osp_teams[tarr[sideno]].osp_m108,
                               basey + 8, OSP_countTeamPlayers(tarr[sideno]), str, time);

                    y += 26;
                    basey += 26;
                }

                kk = strlen(temp);
                if (size + kk >= sizeof(buf))
                    break;
                memcpy(buf + size, temp, kk + 1);
                size += kk;

                if (level.intermission_framenum != 0 && sync_stat > 2)
                    Q_snprintf(temp, sizeof(temp),
                               "xv 120 yv %i string \"Frg Dth Frt Su Eff%% Ping\"xv -16 ",
                               y);
                else if (sync_stat == 4)
                    Q_snprintf(temp, sizeof(temp),
                               "xv 24 yv %i string \"Player          Frags Deaths Ping\"xv 8 ",
                               y);
                else
                    Q_snprintf(temp, sizeof(temp),
                               "xv 8 yv %i string \"Player          MATCH_STATUS Time Ping\"xv 8 ",
                               y);

                y += 8;
                basey += 8;
                kk = strlen(temp);
                if (size + kk >= sizeof(buf))
                    break;
                memcpy(buf + size, temp, kk + 1);
                size += kk;
            }

            if (sync_stat > 2) {
                if (level.intermission_framenum != 0)
                    Q_snprintf(rowline, sizeof(rowline), "%-16s%4i%4i%4i%3i%4i%%%5i",
                            cl->pers.netname, cl->resp.score,
                            cl->resp.osp_r014, cl->resp.osp_r028,
                            cl->resp.osp_r2c0, eff, cl->ping);
                else
                    Q_snprintf(rowline, sizeof(rowline), "%i %-16s%4i   %3i   %4i", i + 1,
                            cl->pers.netname, cl->resp.score,
                            cl->resp.osp_r014, cl->ping);

                if (player != ent)
                    Q_snprintf(temp, sizeof(temp), "yv %i string2 \"%s\"", y, rowline);
                else
                    Q_snprintf(temp, sizeof(temp), "yv %i string \"%s\"", y, rowline);
            } else if (cl->resp.osp_r20c) {
                Q_snprintf(rowline, sizeof(rowline), "%-16s*** READY ***%3i  %4i", cl->pers.netname,
                        nframes / 600, cl->ping);
                Q_snprintf(temp, sizeof(temp), "yv %i string \"%s\"", y, rowline);
            } else {
                Q_snprintf(rowline, sizeof(rowline), "%-16s [NOT READY] %3i  %4i", cl->pers.netname,
                        nframes / 600, cl->ping);
                Q_snprintf(temp, sizeof(temp), "yv %i string2 \"%s\"", y, rowline);
            }

            kk = strlen(temp);
            if (size + kk >= sizeof(buf))
                break;

            memcpy(buf + size, temp, kk + 1);
            size += kk;
        }

        if (OSP_countTeamPlayers(tarr[sideno]) > 1 && level.intermission_framenum != 0) {
            if (OSP_countTeamPlayers(0) + OSP_countTeamPlayers(1) < 8 &&
                sync_stat > 2) {
                y += 11;

                if (osp_teams[tarr[sideno]].osp_m0f8 < 1)
                    eff = 0;
                else if (osp_teams[tarr[sideno]].osp_m0fc == 0 ||
                         osp_teams[tarr[sideno]].osp_m0f8 + osp_teams[tarr[sideno]].osp_m0fc == 0)
                    eff = 100;
                else
                    eff = osp_teams[tarr[sideno]].osp_m0f8 * 100 /
                          (osp_teams[tarr[sideno]].osp_m0f8 + osp_teams[tarr[sideno]].osp_m0fc);

                Q_snprintf(rowline, sizeof(rowline), " *** TOTALS:    %4i %3i  %2i %2i %3i%%",
                        osp_teams[tarr[sideno]].osp_m0f8, osp_teams[tarr[sideno]].osp_m0fc,
                        osp_teams[tarr[sideno]].osp_m104, osp_teams[tarr[sideno]].osp_m108, eff);
                Q_snprintf(temp, sizeof(temp), "yv %i string \"%s\"", y, rowline);

                kk = strlen(temp);
                if (size + kk >= sizeof(buf))
                    break;

                memcpy(buf + size, temp, kk + 1);
                size += kk;
            }
        }

        basey = y + 40;
    }

    if (active_clients < 8) {
        y += 24;

        for (i = 0; i < obscount; i++) {
            player = g_edicts + 1 + viewers[i];

            if (!i) {
                Q_snprintf(temp, sizeof(temp),
                           "xv 32 yv %i string2 \"Observers:\"xv 40 ", y);

                kk = strlen(temp);
                if (size + kk >= sizeof(buf))
                    break;

                memcpy(buf + size, temp, kk + 1);
                size += kk;
                y += 12;
            }

            if (player->osp_e39c)
                Q_snprintf(temp, sizeof(temp), "yv %i string2 \"[Ref]%s (p:%d)\"", y,
                           player->client->pers.netname, player->client->ping);
            else
                Q_snprintf(temp, sizeof(temp), "yv %i string2 \"%s (p:%d)\"", y,
                           player->client->pers.netname, player->client->ping);

            kk = strlen(temp);
            if (size + kk >= sizeof(buf))
                break;

            memcpy(buf + size, temp, kk + 1);
            size += kk;
            y += 8;
        }
    }

    gi.WriteByte(svc_layout);
    gi.WriteString(buf);

    if (level.intermission_framenum != 0 &&
        ent->client->resp.osp_entered == ENTERED_ENTERED)
        Q_strlcpy(old_scores, buf, sizeof(old_scores));
}

// The team scoreboard, large-roster variant -- reached ONLY from
// OSP_showTeamScores' opening `if (active_clients > 6)`.  Same board, three
// differences: the team card is a text line instead of the `client 80`
// picture, the efficiency column is gone from the header and the row, and
// TOTALS loses its trailing percentage.  With the bigger roster it also shows
// SIX rows per team rather than four, and the "...and you" promotion moves to
// row 5 with a rank threshold of 6.
void OSP_showBIGTeamScores(edict_t *ent)
{
    int         rank[2][256];
    int         pscore[2][256];
    int         viewers[256];
    char        rowline[512];
    char        str[256];
    char        temp[1024];
    char        buf[1400];
    char        time[32];
    int         tarr[2];
    int         count[2];
    int         i;
    int         nframes;
    int         m;
    int         y;
    int         sideno;
    int         kk;
    int         basey;
    int         cscore; /* invented: insertion-sort score snapshot */
    int         obscount;
    int         size;
    gclient_t   *cl;
    edict_t     *player;

    y = 0;
    obscount = 0;
    size = 0;

    tarr[0] = 0;
    tarr[1] = 1;

    for (sideno = 0; sideno < 2; sideno++) {
        count[sideno] = 0;

        for (i = 0; i <= game.maxclients; i++) {
            player = g_edicts + 1 + i;

            if (!player->inuse || !player->client)
                continue;

            // observers are collected once, on the first team's pass
            if (!sideno && player->client->resp.osp_entered != ENTERED_ENTERED) {
                viewers[obscount] = i;
                obscount++;
                continue;
            }

            if (player->client->resp.team != tarr[sideno])
                continue;

            cscore = game.clients[i].resp.score;

            for (kk = 0; kk < count[sideno]; kk++) {
                if (cscore > pscore[sideno][kk])
                    break;
                if (cscore == pscore[sideno][kk]) {
                    if (game.clients[i].resp.osp_r014 < game.clients[rank[sideno][kk]].resp.osp_r014)
                        break;
                    if (game.clients[i].resp.osp_r014 == game.clients[rank[sideno][kk]].resp.osp_r014 &&
                        game.clients[i].resp.osp_r2c0 < game.clients[rank[sideno][kk]].resp.osp_r2c0)
                        break;
                }
            }

            for (m = count[sideno]; m > kk; m--) {
                rank[sideno][m] = rank[sideno][m - 1];
                pscore[sideno][m] = pscore[sideno][m - 1];
            }

            rank[sideno][kk] = i;
            pscore[sideno][kk] = cscore;
            count[sideno]++;
        }

        for (i = 0; i < count[sideno]; i++) {
            player = g_edicts + 1 + rank[sideno][i];
            player->client->resp.osp_r208 = i + 1;
        }
    }

    buf[0] = 0;

    if (level.intermission_framenum != 0)
        ent->client->ps.stats[27] = OSP_CS(10);
    else
        ent->client->ps.stats[27] = OSP_CS(9);

    size = strlen(buf);
    basey = 0;

    for (sideno = 0; sideno < 2; sideno++) {
        if (count[sideno] > 6)
            count[sideno] = 6;

        for (i = 0; i < count[sideno]; i++) {
            cl = game.clients + rank[sideno][i];
            player = g_edicts + 1 + rank[sideno][i];
            y = basey + i * 8;

            if (i == 5 && ent->client->resp.team == sideno &&
                ent->client->resp.osp_r208 > 6) {
                player = ent;
                cl = ent->client;
                y += 2;
                i = cl->resp.osp_r208 - 1;
            }

            if (cl->resp.enterframe < sync_frame)
                nframes = level.framenum - sync_frame + 1;
            else
                nframes = level.framenum - cl->resp.enterframe + 1;

            if (nframes < 1)
                nframes = 1;

            // The team card is emitted once, above the first rowline.
            if (!i) {
                Q_snprintf(rowline, sizeof(rowline), "%i", osp_teams[tarr[sideno]].osp_m0f8);
                for (m = 0; m < strlen(rowline); m++)
                    rowline[m] += 128;

                // The target passes the three y positions as basey-16, basey-8,
                // and basey, respectively.
                if (level.intermission_framenum == 0 || sideno) {
                    Q_snprintf(temp, sizeof(temp),
                               "xv 78 yv %i string \"%s\"yv %i string2 \"Score: %s\""
                               "yv %i string2 \"Skin: %s\"",
                               basey - 16, osp_teams[tarr[sideno]].netname, basey - 8, rowline,
                               basey, osp_teams[tarr[sideno]].skin);

                    y += 18;
                    basey += 18;
                } else {
                    OSP_getDateInfo(time);

                    if (manual_map == 1)
                        Q_snprintf(str, sizeof(str), "[ Voted map change ]");
                    else if (manual_map == 2)
                        Q_snprintf(str, sizeof(str), "[ Voted server config change ]");
                    else if (osp_teams[0].osp_m124 == 1)
                        Q_snprintf(str, sizeof(str), "[ %s defeats %s: %d to %d ]",
                                osp_teams[0].greenname, osp_teams[1].greenname,
                                osp_teams[0].osp_m0f8, osp_teams[1].osp_m0f8);
                    else if (osp_teams[1].osp_m124 == 1)
                        Q_snprintf(str, sizeof(str), "[ %s defeats %s: %d to %d ]",
                                osp_teams[1].greenname, osp_teams[0].greenname,
                                osp_teams[1].osp_m0f8, osp_teams[0].osp_m0f8);
                    else
                        Q_snprintf(str, sizeof(str), "[ Tied match! (%d to %d) ]",
                                osp_teams[1].osp_m0f8, osp_teams[0].osp_m0f8);

                    Q_snprintf(temp, sizeof(temp),
                               "xv 78 yv %i string \"%s\"yv %i string2 \"Score: %s\""
                               "yv %i string2 \"Skin: %s\""
                               "xv 0 yv -43 cstring2 \"%s\"yv -33 cstring2 \"%s\"",
                               basey - 16, osp_teams[tarr[sideno]].netname, basey - 8, rowline,
                               basey, osp_teams[tarr[sideno]].skin, str, time);

                    y += 18;
                    basey += 18;
                }

                kk = strlen(temp);
                if (size + kk >= sizeof(buf))
                    break;
                memcpy(buf + size, temp, kk + 1);
                size += kk;

                if (level.intermission_framenum != 0 && sync_stat > 2)
                    Q_snprintf(temp, sizeof(temp),
                               "xv 140 yv %i string \"Frg Dth Frt Su Ping\"xv 4 ",
                               y);
                else if (sync_stat == 4)
                    Q_snprintf(temp, sizeof(temp),
                               "xv 24 yv %i string \"Player          Frags Deaths Ping\"xv 8 ",
                               y);
                else
                    Q_snprintf(temp, sizeof(temp),
                               "xv 8 yv %i string \"Player          MATCH_STATUS Time Ping\"xv 8 ",
                               y);

                y += 8;
                basey += 8;

                kk = strlen(temp);
                if (size + kk >= sizeof(buf))
                    break;
                memcpy(buf + size, temp, kk + 1);
                size += kk;
            }

            if (sync_stat > 2) {
                if (level.intermission_framenum != 0)
                    Q_snprintf(rowline, sizeof(rowline), "%-16s%4i%4i%4i%3i%5i",
                            cl->pers.netname, cl->resp.score,
                            cl->resp.osp_r014, cl->resp.osp_r028,
                            cl->resp.osp_r2c0, cl->ping);
                else
                    Q_snprintf(rowline, sizeof(rowline), "%i %-16s%4i   %3i   %4i", i + 1,
                            cl->pers.netname, cl->resp.score,
                            cl->resp.osp_r014, cl->ping);

                if (player != ent)
                    Q_snprintf(temp, sizeof(temp), "yv %i string2 \"%s\"", y, rowline);
                else
                    Q_snprintf(temp, sizeof(temp), "yv %i string \"%s\"", y, rowline);
            } else if (cl->resp.osp_r20c) {
                Q_snprintf(rowline, sizeof(rowline), "%-16s*** READY ***%3i  %4i", cl->pers.netname,
                        nframes / 600, cl->ping);
                Q_snprintf(temp, sizeof(temp), "yv %i string \"%s\"", y, rowline);
            } else {
                Q_snprintf(rowline, sizeof(rowline), "%-16s [NOT READY] %3i  %4i", cl->pers.netname,
                        nframes / 600, cl->ping);
                Q_snprintf(temp, sizeof(temp), "yv %i string2 \"%s\"", y, rowline);
            }

            kk = strlen(temp);
            if (size + kk >= sizeof(buf))
                break;

            memcpy(buf + size, temp, kk + 1);
            size += kk;
        }

        if (OSP_countTeamPlayers(tarr[sideno]) > 1 && level.intermission_framenum != 0) {
            if (OSP_countTeamPlayers(0) + OSP_countTeamPlayers(1) < 12 &&
                sync_stat > 2) {
                y += 11;

                Q_snprintf(rowline, sizeof(rowline), " *** TOTALS:    %4i %3i  %2i %2i",
                        osp_teams[tarr[sideno]].osp_m0f8, osp_teams[tarr[sideno]].osp_m0fc,
                        osp_teams[tarr[sideno]].osp_m104, osp_teams[tarr[sideno]].osp_m108);
                Q_snprintf(temp, sizeof(temp), "yv %i string \"%s\"", y, rowline);

                kk = strlen(temp);
                if (size + kk >= sizeof(buf))
                    break;

                memcpy(buf + size, temp, kk + 1);
                size += kk;
            }
        }

        basey = y + 40;
    }

    if (active_clients < 12) {
        y += 24;

        for (i = 0; i < obscount; i++) {
            player = g_edicts + 1 + viewers[i];

            if (!i) {
                Q_snprintf(temp, sizeof(temp),
                           "xv 32 yv %i string2 \"Observers:\"xv 40 ", y);

                kk = strlen(temp);
                if (size + kk >= sizeof(buf))
                    break;

                memcpy(buf + size, temp, kk + 1);
                size += kk;
                y += 12;
            }

            if (player->osp_e39c)
                Q_snprintf(temp, sizeof(temp), "yv %i string2 \"[Ref]%s (p:%d)\"", y,
                           player->client->pers.netname, player->client->ping);
            else
                Q_snprintf(temp, sizeof(temp), "yv %i string2 \"%s (p:%d)\"", y,
                           player->client->pers.netname, player->client->ping);

            kk = strlen(temp);
            if (size + kk >= sizeof(buf))
                break;

            memcpy(buf + size, temp, kk + 1);
            size += kk;
            y += 8;
        }
    }

    gi.WriteByte(svc_layout);
    gi.WriteString(buf);

    if (level.intermission_framenum != 0 &&
        ent->client->resp.osp_entered == ENTERED_ENTERED)
        Q_strlcpy(old_scores, buf, sizeof(old_scores));
}

// The 1v1 scoreboard.  Unlike the deathmatch board this one is two player
// CARDS, not a table: each of the two team slots gets its own "client 80"
// banner with the player's icon, frag total and suicide count, then its own
// column header and its own single row, 48 units apart.  Observers are listed
// underneath.  Only the row strcpy is length-guarded; the banner and header
// go on unconditionally.
void OSP_show1v1Scores(edict_t *ent)
{
    int         viewers[256];
    char        rowline[512];
    char        str[256];
    char        temp[1024];
    char        buf[1400];
    char        time[32];
    int         cids[2];
    int         tarr[2];
    int         i;
    int         eff;
    int         nframes;
    int         y;
    int         sideno;
    int         kk;
    int         basey;
    int         obscount;
    int         size;
    gclient_t   *cl;
    edict_t     *player;

    y = 0;
    obscount = 0;
    size = 0;
    tarr[0] = 0;
    tarr[1] = 1;
    cids[0] = -1;
    cids[1] = -1;

    for (i = 0; i < game.maxclients; i++) {
        player = g_edicts + i + 1;

        if (!player->inuse || !player->client)
            continue;

        if (player->client->resp.osp_entered != ENTERED_ENTERED) {
            viewers[obscount] = i;
            obscount++;
            continue;
        }

        if (!player->client->resp.team)
            cids[0] = i;
        else if (player->client->resp.team == 1)
            cids[1] = i;
    }

    buf[0] = 0;

    if (level.intermission_framenum != 0)
        ent->client->ps.stats[27] = OSP_CS(10);
    else
        ent->client->ps.stats[27] = OSP_CS(9);

    size = strlen(buf);
    basey = 0;

    for (sideno = 0; sideno < 2; sideno++) {
        if (cids[sideno] == -1)
            break;

        cl = game.clients + cids[sideno];
        // Computed and never read again within the loop -- dead, but
        // faithfully reproduced.
        player = g_edicts + 1 + cids[sideno];
        y = basey;

        if (cl->resp.enterframe < sync_frame)
            nframes = level.framenum - sync_frame + 1;
        else
            nframes = level.framenum - cl->resp.enterframe + 1;

        if (nframes < 1)
            nframes = 1;

        if (cl->resp.score < 1)
            eff = 0;
        else if (!cl->resp.osp_r014 ||
                 !(cl->resp.osp_r014 + cl->resp.score))
            eff = 100;
        else
            eff = cl->resp.score * 100 /
                  (cl->resp.score + cl->resp.osp_r014);

        // the team's frag total, in green
        Q_snprintf(rowline, sizeof(rowline), "%i", osp_teams[tarr[sideno]].osp_m0f8);
        for (kk = 0; kk < strlen(rowline); kk++)
            rowline[kk] += 128;

        // Only the first card at intermission carries the result line and
        // the time; everything else uses the short banner.
        if (level.intermission_framenum == 0 || sideno)
            Q_snprintf(temp, sizeof(temp),
                       "client 80 %i %i %i %i %i xv 112 picn tag1 xv 114 string \"%s\""
                       "yv %i string2 \"Frags: %s\"yv %i string2 \"Suicides: %i\"",
                       basey - 16, cids[sideno], 0, 0, 0, osp_teams[tarr[sideno]].netname,
                       basey - 4, rowline, basey + 4, osp_teams[tarr[sideno]].osp_m108);
        else {
            OSP_getDateInfo(time);

            if (manual_map == 1)
                Q_snprintf(str, sizeof(str), "[ Voted map change ]");
            else if (manual_map == 2)
                Q_snprintf(str, sizeof(str), "[ Voted server config change ]");
            else if (osp_teams[0].osp_m124 == 1)
                Q_snprintf(str, sizeof(str), "[ %s defeats %s: %d to %d ]",
                        osp_teams[0].greenname, osp_teams[1].greenname,
                        osp_teams[0].osp_m0f8, osp_teams[1].osp_m0f8);
            else if (osp_teams[1].osp_m124 == 1)
                Q_snprintf(str, sizeof(str), "[ %s defeats %s: %d to %d ]",
                        osp_teams[1].greenname, osp_teams[0].greenname,
                        osp_teams[1].osp_m0f8, osp_teams[0].osp_m0f8);
            else
                Q_snprintf(str, sizeof(str), "[ Tied match! (%d to %d) ]",
                        osp_teams[1].osp_m0f8, osp_teams[0].osp_m0f8);

            Q_snprintf(temp, sizeof(temp),
                       "client 80 %i %i %i %i %i xv 112 picn tag1 xv 114 string \"%s\""
                       "yv %i string2 \"Frags: %s\"yv %i string2 \"Suicides: %i\""
                       "xv 0 yv -43 cstring2 \"%s\"yv -25 cstring2 \"%s\"",
                       basey - 16, cids[sideno], 0, 0, 0, osp_teams[tarr[sideno]].netname,
                       basey - 4, rowline, basey + 4, osp_teams[tarr[sideno]].osp_m108, str, time);
        }

        // Real advances BOTH accumulators at each step, even though `basey` is
        // dead from here until it is reassigned `y + 48` below.
        y += 26;
        basey += 26;
        kk = strlen(temp);
        if (size + kk >= sizeof(buf))
            break;
        memcpy(buf + size, temp, kk + 1);
        size += kk;

        if (level.intermission_framenum != 0 && sync_stat > 2)
            Q_snprintf(temp, sizeof(temp),
                       "xv -8 yv %i string \"Player          Frags Deaths Eff%% FPH Ping\"xv -8 ",
                       y);
        else if (sync_stat == 4)
            Q_snprintf(temp, sizeof(temp),
                       "xv 0 yv %i string \"Player          Frags Deaths Ping\"xv 0 ",
                       y);
        else
            Q_snprintf(temp, sizeof(temp),
                       "xv 8 yv %i string \"Player          Frags Deaths Time Ping\"xv 8 ",
                       y);

        y += 8;
        basey += 8;
        kk = strlen(temp);
        if (size + kk >= sizeof(buf))
            break;
        memcpy(buf + size, temp, kk + 1);
        size += kk;

        if (sync_stat > 2) {
            if (level.intermission_framenum != 0)
                Q_snprintf(rowline, sizeof(rowline), "%-16s%4i%6i%6i%%%4i%5i", cl->pers.netname,
                        cl->resp.score, cl->resp.osp_r014, eff,
                        cl->resp.score * 36000 / nframes, cl->ping);
            else
                Q_snprintf(rowline, sizeof(rowline), "%-16s%4i   %3i   %4i", cl->pers.netname,
                        cl->resp.score, cl->resp.osp_r014, cl->ping);

            Q_snprintf(temp, sizeof(temp), "yv %i string2 \"%s\"", y, rowline);
        } else if (cl->resp.osp_r20c) {
            Q_snprintf(rowline, sizeof(rowline), "%-16s*** READY ***%3i  %4i", cl->pers.netname,
                    nframes / 600, cl->ping);
            Q_snprintf(temp, sizeof(temp), "yv %i string \"%s\"", y, rowline);
        } else {
            Q_snprintf(rowline, sizeof(rowline), "%-16s [NOT READY] %3i  %4i", cl->pers.netname,
                    nframes / 600, cl->ping);
            Q_snprintf(temp, sizeof(temp), "yv %i string2 \"%s\"", y, rowline);
        }

        kk = strlen(temp);
        if (size + kk >= sizeof(buf))
            break;

        memcpy(buf + size, temp, kk + 1);
        size += kk;
        basey = y + 48;
    }

    y += 24;

    for (i = 0; i < obscount; i++) {
        player = g_edicts + 1 + viewers[i];

        if (!i) {
            Q_snprintf(temp, sizeof(temp),
                       "xv 32 yv %i string2 \"Observers:\"xv 40 ", y);

            kk = strlen(temp);
            if (size + kk >= sizeof(buf))
                break;

            memcpy(buf + size, temp, kk + 1);
            size += kk;
            y += 12;
        }

        Q_snprintf(temp, sizeof(temp), "yv %i string2 \"%s (p:%d)\"", y,
                   player->client->pers.netname, player->client->ping);

        kk = strlen(temp);
        if (size + kk >= sizeof(buf))
            break;

        memcpy(buf + size, temp, kk + 1);
        size += kk;
        y += 8;
    }

    gi.WriteByte(svc_layout);
    gi.WriteString(buf);

    if (level.intermission_framenum != 0 &&
        ent->client->resp.osp_entered == ENTERED_ENTERED)
        Q_strlcpy(old_scores, buf, sizeof(old_scores));
}

// id CTF's 23-entry table, a NAMED global here rather than CTF's file-static.
// Two changes from CTF's: the two `item_flag_team*` rows are gone (this mod
// has no flags to stand near) and every priority is therefore one lower.
loc_t   osp_loc_names[23] = {
    {   "item_quad",                1   },
    {   "item_invulnerability",     1   },
    {   "weapon_bfg",               2   },
    {   "weapon_railgun",           3   },
    {   "weapon_rocketlauncher",    3   },
    {   "weapon_hyperblaster",      3   },
    {   "weapon_chaingun",          3   },
    {   "weapon_grenadelauncher",   3   },
    {   "weapon_machinegun",        3   },
    {   "weapon_supershotgun",      3   },
    {   "weapon_shotgun",           3   },
    {   "item_power_screen",        4   },
    {   "item_power_shield",        4   },
    {   "item_armor_body",          5   },
    {   "item_armor_combat",        5   },
    {   "item_armor_jacket",        5   },
    {   "item_silencer",            6   },
    {   "item_breather",            6   },
    {   "item_enviro",              6   },
    {   "item_adrenaline",          6   },
    {   "item_bandolier",           7   },
    {   "item_pack",                7   },
    {   NULL,                       0   }
};

// %l -- name the nearest landmark.  CTF's CTFSay_Team_Location with the
// capture-the-flag half removed, so there is no "the red " / "the blue ".
// Every sayteam_* helper writes into OSP_sayteam_cmd's `scratch`.
#define OSP_SAYTEAM_BUF 1024

static void sayteam_location(edict_t *who, char *buf)
{
    edict_t     *what = NULL;
    edict_t     *hot = NULL;
    float       hotdist = 999999, newdist;
    vec3_t      v;
    int         hotindex = 999;
    int         i;
    const gitem_t   *item;
    bool    hotsee = false;
    bool    cansee;

    while ((what = loc_findradius(what, who->s.origin, 1024)) != NULL) {
        for (i = 0; osp_loc_names[i].classname; i++)
            if (strcmp(what->classname, osp_loc_names[i].classname) == 0)
                break;
        if (!osp_loc_names[i].classname)
            continue;

        // something we can see gets priority over something we can't
        cansee = loc_CanSee(what, who);
        if (cansee && !hotsee) {
            hotsee = true;
            hotindex = osp_loc_names[i].priority;
            hot = what;
            VectorSubtract(what->s.origin, who->s.origin, v);
            hotdist = VectorLength(v);
            continue;
        }

        if (hotsee && !cansee)
            continue;
        if (hotsee && hotindex < osp_loc_names[i].priority)
            continue;

        VectorSubtract(what->s.origin, who->s.origin, v);
        newdist = VectorLength(v);

        if (newdist < hotdist ||
            (cansee && osp_loc_names[i].priority < hotindex)) {
            hot = what;
            hotdist = newdist;
            hotindex = i;
            hotsee = loc_CanSee(hot, who);
        }
    }

    if (!hot) {
        Q_strlcpy(buf, "nowhere", OSP_SAYTEAM_BUF);
        return;
    }

    what = NULL;
    while ((what = G_Find(what, FOFS(classname), hot->classname)) != NULL) {
        if (what == hot)
            continue;
        break;
    }

    if ((item = FindItemByClassname(hot->classname)) == NULL) {
        Q_strlcpy(buf, "nowhere", OSP_SAYTEAM_BUF);
        return;
    }

    if (who->waterlevel)
        Q_strlcpy(buf, "in the water ", OSP_SAYTEAM_BUF);
    else
        *buf = 0;

    VectorSubtract(who->s.origin, hot->s.origin, v);
    if (fabs(v[2]) > fabs(v[0]) && fabs(v[2]) > fabs(v[1]))
        if (v[2] > 0)
            Q_strlcat(buf, "above ", OSP_SAYTEAM_BUF);
        else
            Q_strlcat(buf, "below ", OSP_SAYTEAM_BUF);
    else
        Q_strlcat(buf, "near ", OSP_SAYTEAM_BUF);

    Q_strlcat(buf, "the ", OSP_SAYTEAM_BUF);
    Q_strlcat(buf, item->pickup_name, OSP_SAYTEAM_BUF);
}

// %a -- CTF's CTFSay_Team_Armor, unchanged; its whole string set is present.
static void sayteam_armor(edict_t *who, char *buf)
{
    const gitem_t   *item;
    int         index, cells;
    int         power_armor_type;

    *buf = 0;

    power_armor_type = PowerArmorType(who);
    if (power_armor_type) {
        cells = who->client->pers.inventory[ITEM_INDEX(FindItem("cells"))];
        if (cells)
            Q_snprintf(buf + strlen(buf), OSP_SAYTEAM_BUF - strlen(buf),
                       "%s with %i cells ",
                    (power_armor_type == POWER_ARMOR_SCREEN) ?
                    "Power Screen" : "Power Shield", cells);
    }

    index = ArmorIndex(who);
    if (index) {
        item = GetItemByIndex(index);
        if (item) {
            if (*buf)
                Q_strlcat(buf, "and ", OSP_SAYTEAM_BUF);
            Q_snprintf(buf + strlen(buf), OSP_SAYTEAM_BUF - strlen(buf),
                    "%i units of %s",
                    who->client->pers.inventory[index], item->pickup_name);
        }
    }

    if (!*buf)
        Q_strlcpy(buf, "no armor", OSP_SAYTEAM_BUF);
}

// <INVENTED NAMES> for three file-statics the ELF cannot see: gcc -O3
// inlines all three back into OSP_sayteam_cmd.  The VC6 image has them as
// three separate functions.

// %h -- "dead" below zero, otherwise the health count.
static void sayteam_health(edict_t *who, char *buf)
{
    if (who->health <= 0)
        Q_strlcpy(buf, "dead", OSP_SAYTEAM_BUF);
    else
        Q_snprintf(buf, OSP_SAYTEAM_BUF, "%i health", who->health);
}

// %w -- the weapon in hand, or "none".
static void sayteam_weapon(edict_t *who, char *buf)
{
    if (who->client->pers.weapon)
        Q_strlcpy(buf, who->client->pers.weapon->pickup_name, OSP_SAYTEAM_BUF);
    else
        Q_strlcpy(buf, "none", OSP_SAYTEAM_BUF);
}

// %r and %t -- whichever rune is held. Both escapes share this one body; the
// two case labels in the caller are separate, the code behind them is not.
static void sayteam_runes(edict_t *who, char *buf)
{
    if (G_GetStat(who, SID_OSP_RUNE_RESIST))
        Q_strlcpy(buf, "the RESIST rune", OSP_SAYTEAM_BUF);
    else if (G_GetStat(who, SID_OSP_RUNE_STRENGTH))
        Q_strlcpy(buf, "the STRENGTH rune", OSP_SAYTEAM_BUF);
    else if (G_GetStat(who, SID_OSP_RUNE_HASTE))
        Q_strlcpy(buf, "the HASTE rune", OSP_SAYTEAM_BUF);
    else if (G_GetStat(who, SID_OSP_RUNE_REGEN))
        Q_strlcpy(buf, "the REGEN rune", OSP_SAYTEAM_BUF);
    else if (G_GetStat(who, SID_OSP_RUNE_VAMPIRE))
        Q_strlcpy(buf, "the VAMPIRE rune", OSP_SAYTEAM_BUF);
    else
        Q_strlcpy(buf, "no runes", OSP_SAYTEAM_BUF);
}

// %n -- the mod's own: name every teammate the caller can actually see, as
// "a, b and c".  The pending-name buffer is what makes the last separator
// " and " rather than ", ".
static void sayteam_sight(edict_t *who, char *buf)
{
    char    list[1024];
    char    names[1024];
    edict_t *e;
    int     i;
    int     counts;

    // counts, then names[0], then list[0] -- and the last two are ONE chained
    // assignment.
    counts = 0;
    list[0] = names[0] = 0;

    for (i = 1; i <= game.maxclients; i++) {
        e = g_edicts + i;

        if (!e->inuse ||
            e->client->resp.osp_entered != ENTERED_ENTERED ||
            e == who ||
            !loc_CanSee(e, who))
            continue;

        if (names[0]) {
            if (strlen(list) + strlen(names) + 3 < 1024) {
                if (counts)
                    Q_strlcat(list, ", ", sizeof(list));
                Q_strlcat(list, names, sizeof(list));
                names[0] = 0;
            }
            counts++;
        }

        Q_strlcpy(names, e->client->pers.netname, sizeof(names));
    }

    if (names[0]) {
        if (strlen(list) + strlen(names) + 6 < 1024) {
            if (counts)
                Q_strlcat(list, " and ", sizeof(list));
            Q_strlcat(list, names, sizeof(list));
        }

        Q_strlcpy(buf, list, OSP_SAYTEAM_BUF);
    } else
        Q_strlcpy(buf, "no one", OSP_SAYTEAM_BUF);
}

// "say_team".  id CTF's CTFSay_Team: expand the % escapes into outmsg, then
// send the result to everyone on the caller's team.
void OSP_sayteam_cmd(edict_t *ent, char *msg)
{
    char    outmsg[1024];
    char    scratch[1024];
    char    tmp[2048];
    char    *p;
    int     t;
    edict_t *cp;

    outmsg[0] = 0;

    if (*msg == '"') {
        msg[strlen(msg) - 1] = 0;
        msg++;
    }

    for (p = outmsg; *msg && (p - outmsg) < sizeof(outmsg) - 1; msg++) {
        if (*msg == '%') {
            switch (*++msg) {
            case 'l':
            case 'L':
                sayteam_location(ent, scratch);
                p += Q_strlcpy(p, scratch, outmsg + sizeof(outmsg) - p);
                if (p > outmsg + sizeof(outmsg) - 1)
                    p = outmsg + sizeof(outmsg) - 1;
                break;

            case 'a':
            case 'A':
                sayteam_armor(ent, scratch);
                p += Q_strlcpy(p, scratch, outmsg + sizeof(outmsg) - p);
                if (p > outmsg + sizeof(outmsg) - 1)
                    p = outmsg + sizeof(outmsg) - 1;
                break;

            case 'h':
            case 'H':
                sayteam_health(ent, scratch);
                p += Q_strlcpy(p, scratch, outmsg + sizeof(outmsg) - p);
                if (p > outmsg + sizeof(outmsg) - 1)
                    p = outmsg + sizeof(outmsg) - 1;
                break;

            case 'w':
            case 'W':
                sayteam_weapon(ent, scratch);
                p += Q_strlcpy(p, scratch, outmsg + sizeof(outmsg) - p);
                if (p > outmsg + sizeof(outmsg) - 1)
                    p = outmsg + sizeof(outmsg) - 1;
                break;

            case 'n':
            case 'N':
                sayteam_sight(ent, scratch);
                p += Q_strlcpy(p, scratch, outmsg + sizeof(outmsg) - p);
                if (p > outmsg + sizeof(outmsg) - 1)
                    p = outmsg + sizeof(outmsg) - 1;
                break;

            case 'r':
            case 'R':
                sayteam_runes(ent, scratch);
                p += Q_strlcpy(p, scratch, outmsg + sizeof(outmsg) - p);
                if (p > outmsg + sizeof(outmsg) - 1)
                    p = outmsg + sizeof(outmsg) - 1;
                break;

            case 't':
            case 'T':
                sayteam_runes(ent, scratch);
                p += Q_strlcpy(p, scratch, outmsg + sizeof(outmsg) - p);
                if (p > outmsg + sizeof(outmsg) - 1)
                    p = outmsg + sizeof(outmsg) - 1;
                break;

            default:
                *p++ = *msg;
            }
        } else
            *p++ = *msg;
    }
    *p = 0;

    Q_snprintf(tmp, sizeof(tmp), "(%s): %s\n", ent->client->pers.netname,
               outmsg);

    for (t = 1; t <= game.maxclients; t++) {
        cp = g_edicts + t;

        if (!cp->inuse || !cp->client)
            continue;
        if (cp->client->resp.team == ent->client->resp.team)
            gi.cprintf(cp, PRINT_CHAT, "%s", tmp);
    }
}

// CTF's loc_findradius, unchanged, and it really is placed AFTER
// OSP_sayteam_cmd in the real link order.
static edict_t *loc_findradius(edict_t *from, vec3_t org, float rad)
{
    vec3_t  eorg;
    int     j;

    if (!from)
        from = g_edicts;
    else
        from++;

    for (; from < &g_edicts[globals.num_edicts]; from++) {
        if (!from->inuse)
            continue;

        for (j = 0; j < 3; j++)
            eorg[j] = org[j] - (from->s.origin[j] +
                                (from->mins[j] + from->maxs[j]) * 0.5f);

        if (VectorLength(eorg) > rad)
            continue;

        return from;
    }

    return NULL;
}
