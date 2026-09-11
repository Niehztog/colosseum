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
// than being merged into a spine file.  The reconstruction's
// asm-matching address comments are stripped.
// osp_display.c -- filename assigned by this tree.  The MOTD, match-params and
// scoreboard/player text builders.

#include "g_local.h"
#include "tourney/osp_types.h"
#include "bot/bl_main.h"
#include "bot/bl_botcfg.h"
// The two bot entry points, from the headers that own them.  These were once
// re-declared here, which is the shape to avoid: a donor's name declared
// outside the header that declares it.  The seam file they resolved to is
// gone.
#include "bot/bl_redirgi.h"
#include "bot/bl_spawn.h"

// Read motd.txt into nine 32-column lines and build the whole layout string
// match_motd out of them, followed by the fixed OSP credit block.  motd_center
// picks between a left-aligned "xl 4 " / "yb %d" block that grows upwards from
// the bottom of the screen and a centred "xv 32 " / "yv %d" one that starts at
// y = 64.
void OSP_setMOTD(void)
{
    char    motdpage[9][33];
    char    buf[1024];
    int     y;
    int     len = 0;
    int     lines;
    FILE    *f = NULL;
    // The four cvar lookups are DECLARATION INITIALISERS, which is what puts
    // the pooled "motd.txt" address between `motdfile` and `center` in the
    // frame: gcc creates a temp while expanding an initialiser, so it lands
    // between the variable it initialises and the next declaration.  Written
    // as plain statements the temp comes after every declared local instead,
    // and real's ELF puts it fourth of five.  The literal is repeated rather
    // than cached -- real's PE pushes two distinct .rdata copies.
    cvar_t  *motdfile = gi.cvar("motd_file", "motd.txt", 0);
    cvar_t  *center = gi.cvar("motd_center", "0", 0);
    int     i;
    int     c = -1;

    {
        char    path[MAX_OSPATH];
        char    *p = path;

        if (!G_FsGamePath(path, sizeof(path),
                          motdfile ? motdfile->string : "motd.txt")) {
            gi.dprintf("MOTD: Path too long for \"%s\"\n",
                       motdfile ? motdfile->string : "motd.txt");
            lines = 0;
        } else {
            f = fopen(p, "r");
            if (f) {
                if (!motd_read) {
                    gi.dprintf("MOTD: Reading from \"%s\"\n", motdfile->string);
                    motd_read = 1;
                }
                for (lines = 0; lines < 9; lines++) {
                    for (i = 0; i < 33; i++)
                        motdpage[lines][i] = 0;

                    for (i = 0; i < 33; i++) {
                        c = fgetc(f);
                        if (c == -1 || c == '\n')
                            break;
                        motdpage[lines][i] = c;
                    }

                    // Windows' CRT translates CRLF to LF on a text-mode fgetc, so a
                    // motd.txt with Windows line endings never shows the CR to this
                    // loop there; on Unix fopen's "r" does no such translation.
#ifndef _WIN32
                    if (i && motdpage[lines][i - 1] == '\r')
                        motdpage[lines][i - 1] = 0;
#endif

                    if (i == 33) {
                        motdpage[lines][32] = 0;
                        while (c != '\n' && c != -1)
                            c = fgetc(f);
                    }

                    if (c == -1)
                        break;
                }

                if (i)
                    lines++;
                if (lines > 9)
                    lines = 9;

                fclose(f);
            } else {
                gi.dprintf("MOTD: Couldn't open \"%s\"\n", motdfile->string);
                lines = 0;
            }
        }
    }

    if (!(int)center->value) {
        y = (9 - lines) * 8 - 136;
        Q_strlcpy(match_motd, "xl 4 ", sizeof(match_motd));

        for (i = 0; i < lines; i++, y += 8) {
            Q_snprintf(buf, sizeof(buf), "yb %d string \"%s\"", y, motdpage[i]);
            Q_strlcat(match_motd, buf, sizeof(match_motd));
        }
    } else {
        y = 64;
        Q_strlcpy(match_motd, "xv 32 ", sizeof(match_motd));

        for (i = 0; i < lines; i++, y += 8) {
            Q_snprintf(buf, sizeof(buf), "yv %d string \"%s\"", y, motdpage[i]);
            Q_strlcat(match_motd, buf, sizeof(match_motd));
        }

        Q_strlcat(match_motd, "xl 4 ", sizeof(match_motd));
    }

    Q_strlcat(match_motd,
              "yb -56 string \"OSP Tourney DM v(2.75)\""
              " yb -48 string2 \"Orange Smoothie Productions\""
              "yb -40 string2 \"http://www.OrangeSmoothie.org\""
              "yb -32 string2 \"rhea@OrangeSmoothie.org\"",
              sizeof(match_motd));
    (void)len;
}

void OSP_showMOTD(void)
{
    gi.WriteByte(svc_layout);
    gi.WriteString(match_motd);
}

// Build the "match parameters" page shown at the start of a match into
// match_info.  Three layouts, one per mode: the qualifier lists the number of
// qualifying spots, team play lists both teams' head counts and skins and both
// friendly-fire switches, and 1v1 lists the two team names either side of a
// "vs." plus the overtime rule.  Every value is written into tmp, greened by
// adding 128 to each byte, and then substituted into the layout line.
// `dm` (plain deathmatch) builds nothing at all.
void OSP_setShowParams(void)
{
    // Both grow with cvars and with the map's descriptive name, so they are
    // sized for the worst case rather than for the common one.
    char    buf[512];
    char    tmp[256];
    cvar_t  *host;
    int     x;

    host = gi.cvar("hostname", "", 0);

    if (G_Ruleset() == RULESET_DMPRO) {
        Q_snprintf(buf, sizeof(buf), "xv 2 yv 0 string \"Match: %s\"", host->string);
        Q_strlcpy(match_info, buf, sizeof(match_info));

        Q_snprintf(buf, sizeof(buf), "yv 8 string2 \"------------------------------\"");
        Q_strlcat(match_info, buf, sizeof(match_info));

        Q_snprintf(buf, sizeof(buf), "yv 16 string \"Number of connected players: %i\"", active_clients);
        Q_strlcat(match_info, buf, sizeof(match_info));

        Q_snprintf(buf, sizeof(buf), "yv 24 string \"Number of qualifying spots : %d\"", (int)qualifier_numspots->value);
        Q_strlcat(match_info, buf, sizeof(match_info));

        Q_snprintf(tmp, sizeof(tmp), "%s (%s)", level.level_name, level.mapname);
        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 40 string2 \"Map: %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        Q_snprintf(tmp, sizeof(tmp), "%d", (int)dmflags->value);
        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 48 string2 \"DM Flags: %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        if ((int)fraglimit->value)
            Q_snprintf(tmp, sizeof(tmp), "%d", (int)fraglimit->value);
        else
            Q_snprintf(tmp, sizeof(tmp), "NONE");

        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 56 string2 \"Fraglimit: %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        if ((int)timelimit->value) {
            if ((int)timelimit->value == 1)
                Q_snprintf(tmp, sizeof(tmp), "1 minute");
            else
                Q_snprintf(tmp, sizeof(tmp), "%d minutes", (int)timelimit->value);
        } else
            Q_snprintf(tmp, sizeof(tmp), "NONE");

        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 64 string2 \"Timelimit: %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        if ((int)hook_enable->value)
            Q_snprintf(tmp, sizeof(tmp), "ENABLED");
        else
            Q_snprintf(tmp, sizeof(tmp), "DISABLED");

        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 72 string2 \"The Hook : %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        Q_snprintf(buf, sizeof(buf), "yv 104 string \"Good Luck!!\"");
        Q_strlcat(match_info, buf, sizeof(match_info));

        Q_snprintf(buf, sizeof(buf), "yv 80 string2 \"Removed Items:\"xv 10 yv 88 string \"");
        Q_strlcat(match_info, buf, sizeof(match_info));

        buf[0] = 0;
        OSP_listDisabledItems(buf);
        Q_strlcat(buf, "\"", sizeof(buf));
        Q_strlcat(match_info, buf, sizeof(match_info));
    } else if (G_Ruleset() == RULESET_TDM) {
        Q_snprintf(buf, sizeof(buf), "xv 2 yv 0 string \"Match: %s\"", host->string);
        Q_strlcpy(match_info, buf, sizeof(match_info));

        Q_snprintf(buf, sizeof(buf), "yv 8 string2 \"------------------------------\"");
        Q_strlcat(match_info, buf, sizeof(match_info));

        Q_snprintf(tmp, sizeof(tmp), "# of players:");
        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 16 string \"%s %s %i\"", osp_teams[0].netname, tmp,
                OSP_teamCount(0));
        Q_strlcat(match_info, buf, sizeof(match_info));

        Q_snprintf(buf, sizeof(buf), "yv 24 string \"%s %s %i\"", osp_teams[1].netname, tmp,
                OSP_teamCount(1));
        Q_strlcat(match_info, buf, sizeof(match_info));

        Q_snprintf(tmp, sizeof(tmp), "skin:");
        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 40 string \"%s %s %s\"", osp_teams[0].netname, tmp, osp_teams[0].skin);
        Q_strlcat(match_info, buf, sizeof(match_info));

        Q_snprintf(buf, sizeof(buf), "yv 48 string \"%s %s %s\"", osp_teams[1].netname, tmp, osp_teams[1].skin);
        Q_strlcat(match_info, buf, sizeof(match_info));

        Q_snprintf(tmp, sizeof(tmp), "%s (%s)", level.level_name, level.mapname);
        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 64 string2 \"Map: %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        Q_snprintf(tmp, sizeof(tmp), "%d", (int)dmflags->value);
        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 72 string2 \"DM Flags : %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        if ((int)fraglimit->value)
            Q_snprintf(tmp, sizeof(tmp), "%d", (int)fraglimit->value);
        else
            Q_snprintf(tmp, sizeof(tmp), "NONE");

        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 80 string2 \"Fraglimit: %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        if ((int)timelimit->value) {
            if ((int)timelimit->value == 1)
                Q_snprintf(tmp, sizeof(tmp), "1 minute");
            else
                Q_snprintf(tmp, sizeof(tmp), "%d minutes", (int)timelimit->value);
        } else
            Q_snprintf(tmp, sizeof(tmp), "NONE");

        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 88 string2 \"Timelimit: %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        if ((int)hook_enable->value)
            Q_snprintf(tmp, sizeof(tmp), "ENABLED");
        else
            Q_snprintf(tmp, sizeof(tmp), "DISABLED");

        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 96 string2 \"The Hook : %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        if (!(int)team_overtime_mode->value)
            Q_snprintf(tmp, sizeof(tmp), "NONE (match can end in a tie)");
        else if ((int)team_overtime_mode->value == 1)
            Q_snprintf(tmp, sizeof(tmp), "Sudden Death (first death decides)");
        else if ((int)team_overtime_time->value == 1)
            Q_snprintf(tmp, sizeof(tmp), "Timed round (1 minute)");
        else
            Q_snprintf(tmp, sizeof(tmp), "Timed round (%d minutes)",
                    (int)team_overtime_time->value);

        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 104 string2 \"Overtime : %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        if (osp_teams[0].osp_m11c)
            Q_snprintf(tmp, sizeof(tmp), "YES");
        else
            Q_snprintf(tmp, sizeof(tmp), "NO");

        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 112 string2 \"Hurt Team: %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        if (osp_teams[0].osp_m120)
            Q_snprintf(tmp, sizeof(tmp), "YES");
        else
            Q_snprintf(tmp, sizeof(tmp), "NO");

        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 120 string2 \"Hurt Self: %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        Q_snprintf(buf, sizeof(buf), "yv 152 string \"Good Luck!!\"");
        Q_strlcat(match_info, buf, sizeof(match_info));

        Q_snprintf(buf, sizeof(buf), "yv 128 string2 \"Removed Items:\"xv 10 yv 136 string \"");
        Q_strlcat(match_info, buf, sizeof(match_info));

        buf[0] = 0;
        OSP_listDisabledItems(buf);
        Q_strlcat(buf, "\"", sizeof(buf));
        Q_strlcat(match_info, buf, sizeof(match_info));
    } else if (G_Ruleset() == RULESET_DUEL) {
        Q_snprintf(buf, sizeof(buf), "xv 2 yv 0 string \"Match: %s\"", host->string);
        Q_strlcpy(match_info, buf, sizeof(match_info));

        Q_snprintf(buf, sizeof(buf), "yv 8 string2 \"------------------------------\"");
        Q_strlcat(match_info, buf, sizeof(match_info));

        Q_snprintf(tmp, sizeof(tmp), "vs.");
        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 16 string \"*** %s %s %s ***\"", osp_teams[0].netname, tmp,
                osp_teams[1].netname);
        Q_strlcat(match_info, buf, sizeof(match_info));

        Q_snprintf(tmp, sizeof(tmp), "%s (%s)", level.level_name, level.mapname);
        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 32 string2 \"Map: %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        Q_snprintf(tmp, sizeof(tmp), "%d", (int)dmflags->value);
        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 40 string2 \"DM Flags : %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        if ((int)fraglimit->value)
            Q_snprintf(tmp, sizeof(tmp), "%d", (int)fraglimit->value);
        else
            Q_snprintf(tmp, sizeof(tmp), "NONE");

        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 48 string2 \"Fraglimit: %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        if ((int)timelimit->value) {
            if ((int)timelimit->value == 1)
                Q_snprintf(tmp, sizeof(tmp), "1 minute");
            else
                Q_snprintf(tmp, sizeof(tmp), "%d minutes", (int)timelimit->value);
        } else
            Q_snprintf(tmp, sizeof(tmp), "NONE");

        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 56 string2 \"Timelimit: %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        if ((int)hook_enable->value)
            Q_snprintf(tmp, sizeof(tmp), "ENABLED");
        else
            Q_snprintf(tmp, sizeof(tmp), "DISABLED");

        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 64 string2 \"The Hook : %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        if (!(int)team_overtime_mode->value)
            Q_snprintf(tmp, sizeof(tmp), "NONE (match can end in a tie)");
        else if ((int)team_overtime_mode->value == 1)
            Q_snprintf(tmp, sizeof(tmp), "Sudden Death (first death decides)");
        else if ((int)team_overtime_time->value == 1)
            Q_snprintf(tmp, sizeof(tmp), "Timed round (1 minute)");
        else
            Q_snprintf(tmp, sizeof(tmp), "Timed round (%d minutes)",
                    (int)team_overtime_time->value);

        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 72 string2 \"Overtime : %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        if (osp_teams[0].osp_m120)
            Q_snprintf(tmp, sizeof(tmp), "YES");
        else
            Q_snprintf(tmp, sizeof(tmp), "NO");

        for (x = 0; x < strlen(tmp); x++)
            tmp[x] += 128;
        Q_snprintf(buf, sizeof(buf), "yv 80 string2 \"Hurt Self: %s\"", tmp);
        Q_strlcat(match_info, buf, sizeof(match_info));

        Q_snprintf(buf, sizeof(buf), "yv 112 string \"Good Luck!!\"");
        Q_strlcat(match_info, buf, sizeof(match_info));

        Q_snprintf(buf, sizeof(buf), "yv 88 string2 \"Removed Items:\"xv 10 yv 96 string \"");
        Q_strlcat(match_info, buf, sizeof(match_info));

        buf[0] = 0;
        OSP_listDisabledItems(buf);
        Q_strlcat(buf, "\"", sizeof(buf));
        Q_strlcat(match_info, buf, sizeof(match_info));
    }
}

void OSP_showParams(void)
{
    gi.WriteByte(svc_layout);
    gi.WriteString(match_info);
}

// Render the scoreboard.  `list` is OSP_DoRankSort's ordered client numbers,
// `count` how many of them to draw (capped at ten), `ent` the viewer.  Four
// row layouts: (a) plain DM in progress, (b) intermission, (c) a running
// match, (d) warmup.  resp.osp_r2b0 is the viewer's row cursor and
// resp.osp_r2ac the client number it currently points at, which is what
// OSP_showPlayer then reads.  The page stops growing at 1400 bytes.
void OSP_showScores(int *list, int count, edict_t *ent)
{
    char        rline[200];
    char        chase[32];
    char        headbuf[1024];
    char        buf[1400];
    char        time[32];
    int         nchars;
    int         outlen;
    int         i;
    int         eff;
    int         nframes;
    int         y;
    int         endframe;
    gclient_t   *cl;
    edict_t     *other;

    outlen = 0;
    buf[0] = 0;
    outlen = strlen(buf);

    if (level.intermission_framenum != 0)
        endframe = endlvl_frame;
    else
        endframe = level.framenum;

    if (active_clients)
        ent->client->resp.osp_r2b0 %= active_clients;
    else
        ent->client->resp.osp_r2b0 = -1;

    if (count > 10) {
        count = 10;
        ent->client->resp.osp_r2b0 %= count;
    }

    for (i = 0; i < count; i++) {
        cl = game.clients + list[i];
        other = g_edicts + 1 + list[i];
        y = i * 8 + 34;

        if (i == ent->client->resp.osp_r2b0)
            ent->client->resp.osp_r2ac = list[i] + 1;

        if (i == 9 && ent->client->resp.osp_r208 > 10) {
            other = ent;
            cl = ent->client;
            y += 2;
            i = cl->resp.osp_r208 - 1;
        }

        if (cl->resp.enterframe < sync_frame)
            nframes = endframe - sync_frame + 1;
        else
            nframes = endframe - cl->resp.enterframe + 1;

        if (nframes < 1) {
            nframes = 1;
            cl->resp.enterframe = endframe + 1;
            cl->resp.osp_r2d4 = 1;
        }

        if (cl->resp.score < 1)
            eff = 0;
        else if (!cl->resp.osp_r014 || !(cl->resp.osp_r014 + cl->resp.score))
            eff = 100;
        else
            eff = cl->resp.score * 100 /
                  (cl->resp.score + cl->resp.osp_r014);

        // Row zero doubles as the leader/champion banner.
        if (!i) {
            if (cl->resp.osp_entered == ENTERED_ENTERED) {
                if (level.intermission_framenum == 0)
                    Q_snprintf(headbuf, 1024,
                               "client 80 -16 %i %i %i %i xv 112 picn tag1 xv 114 string \"%s\""
                               "yv -8 string2 \"Frags: %i\"yv 0 string2 \"Eff%% : %i%%\""
                               "yv 8 string2 \"FPH  : %i\"xv 0 yv -24 cstring2 \"Current Leader:\"",
                               list[i], 0, 0, 0, cl->pers.netname, cl->resp.score,
                               eff, cl->resp.score * 36000 / nframes);
                else {
                    OSP_getDateInfo(time);
                    Q_snprintf(headbuf, 1024,
                               "client 80 -16 %i %i %i %i xv 112 picn tag1 xv 114 string \"%s\""
                               "yv -8 string2 \"Frags: %i\"yv 0 string2 \"Eff%% : %i%%\""
                               "yv 8 string2 \"FPH  : %i\"xv 0 yv -24 cstring2 \"Champion:\""
                               "yv 17 cstring2 \"%s\"",
                               list[i], 0, 0, 0, cl->pers.netname, cl->resp.score,
                               eff, cl->resp.score * 36000 / nframes, time);
                }

                nchars = strlen(headbuf);
                memcpy(buf + outlen, headbuf, nchars + 1);
                outlen += nchars;
            }

            if (sync_stat == 8)
                Q_snprintf(headbuf, 1024, "xv -16 yv 26 string \"Player          Frgs Dths Eff%% FPH Time Ping\"xv -40 ");
            else if (level.intermission_framenum != 0)
                Q_snprintf(headbuf, 1024, "xv -24 yv 26 string \"Player          Frgs Dths Eff%% FPH Time Ping\"xv -56 ");
            else if (sync_stat == 4)
                Q_snprintf(headbuf, 1024, "xv 32 yv 26 string \"Player          Frags Deaths Ping\"xv 0 ");
            else
                Q_snprintf(headbuf, 1024, "xv 8 yv 26 string \"Player          Frags Deaths Time Ping\"xv 8 ");

            nchars = strlen(headbuf);
            if (outlen + nchars >= sizeof(buf))
                break;
            memcpy(buf + outlen, headbuf, nchars + 1);
            outlen += nchars;
        }

        if (sync_stat > 2 || level.intermission_framenum != 0) {
            char        mark;

            if ((int)qualifier_numspots->value &&
                cl->resp.osp_r208 <= (int)qualifier_numspots->value)
                mark = '*';
            else
                mark = ' ';

            if (sync_stat == 8) {
                // (a) plain DM: no rank marker, and the selected rline is
                // pulled out to xv -48 with a leading \r / 0x8d.
                if (cl->resp.osp_entered == ENTERED_ENTERED)
                    Q_snprintf(rline, sizeof(rline), "%2i %-16s%4i  %3i %3i%%%4i %3i  %4i",
                            i + 1, cl->pers.netname, cl->resp.score,
                            cl->resp.osp_r014, eff,
                            cl->resp.score * 36000 / nframes, nframes / 600,
                            cl->ping);
                else if (other->osp_e39c)
                    Q_snprintf(rline, sizeof(rline), "   %-16s<<<Referee>>>      %3i  %4i",
                            cl->pers.netname, nframes / 600, cl->ping);
                else if (cl->resp.osp_entered == 2)
                    Q_snprintf(rline, sizeof(rline), "   %-16s(Observing)        %3i  %4i",
                            cl->pers.netname, nframes / 600, cl->ping);
                else if (cl->resp.osp_entered == 16)
                    Q_snprintf(rline, sizeof(rline), "   %-16s(Autocam)          %3i  %4i",
                            cl->pers.netname, nframes / 600, cl->ping);
                else if (cl->chase_target && cl->chase_target->client) {
                    Q_snprintf(chase, sizeof(chase), "(Chasing %s)",
                               cl->chase_target->client->pers.greenname);

                    if (strlen(cl->chase_target->client->pers.netname) == 15)
                        Q_snprintf(rline, sizeof(rline), "   %-16s%-24s%3i",
                                   cl->pers.netname, chase, cl->ping);
                    else
                        Q_snprintf(rline, sizeof(rline), "   %-16s%-24s%4i",
                                   cl->pers.netname, chase, cl->ping);
                } else
                    Q_snprintf(rline, sizeof(rline), "   %-16s%-24s%4i",
                               cl->pers.netname, "(Waiting)", cl->ping);

                if (other != ent) {
                    if (i == ent->client->resp.osp_r2b0) {
                        Q_snprintf(headbuf, 1024,
                                   "xv -48 yv %i string2 \"\x8d%s\"xv -40 ",
                                   y, rline);
                        goto appended;
                    }
                    Q_snprintf(headbuf, 1024, "yv %i string2 \"%s\"", y, rline);
                } else {
                    if (i == ent->client->resp.osp_r2b0) {
                        Q_snprintf(headbuf, 1024,
                                   "xv -48 yv %i string \"\r%s\"xv -40 ",
                                   y, rline);
                        goto appended;
                    }
                    Q_snprintf(headbuf, 1024, "yv %i string \"%s\"", y, rline);
                }
            } else if (level.intermission_framenum != 0) {
                // (b) intermission: same columns, rank marker, four spaces.
                if (cl->resp.osp_entered == ENTERED_ENTERED)
                    Q_snprintf(rline, sizeof(rline), "%c%2i %-16s%4i  %3i %3i%%%4i %3i  %4i",
                            mark, i + 1, cl->pers.netname, cl->resp.score,
                            cl->resp.osp_r014, eff,
                            cl->resp.score * 36000 / nframes, nframes / 600,
                            cl->ping);
                else if (other->osp_e39c)
                    Q_snprintf(rline, sizeof(rline), "    %-16s<<<REFEREE>>>      %3i  %4i",
                            cl->pers.netname, nframes / 600, cl->ping);
                else if (cl->resp.osp_entered == 2)
                    Q_snprintf(rline, sizeof(rline), "    %-16s(Observing)        %3i  %4i",
                            cl->pers.netname, nframes / 600, cl->ping);
                else if (cl->resp.osp_entered == 16)
                    Q_snprintf(rline, sizeof(rline), "    %-16s(Autocam)          %3i  %4i",
                            cl->pers.netname, nframes / 600, cl->ping);
                else if (cl->chase_target && cl->chase_target->client) {
                    Q_snprintf(chase, sizeof(chase), "(Chasing %s)",
                               cl->chase_target->client->pers.greenname);

                    if (strlen(cl->chase_target->client->pers.netname) == 15)
                        Q_snprintf(rline, sizeof(rline), "    %-16s%-24s%3i",
                                   cl->pers.netname, chase, cl->ping);
                    else
                        Q_snprintf(rline, sizeof(rline), "    %-16s%-24s%4i",
                                   cl->pers.netname, chase, cl->ping);
                } else
                    Q_snprintf(rline, sizeof(rline), "    %-16s%-24s%4i",
                               cl->pers.netname, "(Waiting)", cl->ping);

                if (other != ent)
                    Q_snprintf(headbuf, 1024, "yv %i string2 \"%s\"", y, rline);
                else
                    Q_snprintf(headbuf, 1024, "yv %i string \"%s\"", y, rline);
            } else {
                // (c) match running: frags/deaths/ping only.
                if (cl->resp.osp_entered == ENTERED_ENTERED)
                    Q_snprintf(rline, sizeof(rline), "%c%2i %-16s%4i   %3i   %4i", mark, i + 1,
                            cl->pers.netname, cl->resp.score,
                            cl->resp.osp_r014, cl->ping);
                else if (other->osp_e39c)
                    Q_snprintf(rline, sizeof(rline), "    %-16s<<<Referee>>>%4i",
                            cl->pers.netname, cl->ping);
                else if (cl->resp.osp_entered == 2)
                    Q_snprintf(rline, sizeof(rline), "    %-16s(Observing)  %4i",
                            cl->pers.netname, cl->ping);
                else if (cl->resp.osp_entered == 16)
                    Q_snprintf(rline, sizeof(rline), "    %-16s(Autocam)    %4i",
                            cl->pers.netname, cl->ping);
                else {
                    if (cl->chase_target && cl->chase_target->client)
                        Q_snprintf(chase, sizeof(chase), "(Chasing #%d)",
                                   cl->chase_target->client->resp.osp_r208);
                    else
                        Q_strlcpy(chase, "(Waiting)", sizeof(chase));
                    Q_snprintf(rline, sizeof(rline), "    %-16s%-13s%4i",
                               cl->pers.netname, chase, cl->ping);
                }

                if (other != ent)
                    Q_snprintf(headbuf, 1024, "yv %i string2 \"%s\"", y, rline);
                else
                    Q_snprintf(headbuf, 1024, "yv %i string \"%s\"", y, rline);
            }
        } else {
            // (d) warmup: no rank column, ready state instead of scores.
            // Every arm wraps its OWN rline -- six separate Q_snprintf sites and
            // six separate wrap literals in the PE, no shared tail and no goto.
            if (other->osp_e39c == 1 ||
                (cl->resp.osp_entered != ENTERED_ENTERED && other->osp_e39c == 2)) {
                Q_snprintf(rline, sizeof(rline), "%-16s<<<Referee>>>%3i  %4i", cl->pers.netname,
                        nframes / 600, cl->ping);
                Q_snprintf(headbuf, 1024, "yv %i string2 \"%s\"", y, rline);
            } else if (cl->resp.osp_entered == 2) {
                Q_snprintf(rline, sizeof(rline), "%-16s(Observing)  %3i  %4i", cl->pers.netname,
                        nframes / 600, cl->ping);
                Q_snprintf(headbuf, 1024, "yv %i string2 \"%s\"", y, rline);
            } else if (cl->resp.osp_entered == 16) {
                Q_snprintf(rline, sizeof(rline), "%-16s(Autocam)    %3i  %4i", cl->pers.netname,
                        nframes / 600, cl->ping);
                Q_snprintf(headbuf, 1024, "yv %i string2 \"%s\"", y, rline);
            } else if (cl->chase_target && cl->chase_target->client) {
                Q_snprintf(chase, sizeof(chase), "(Chasing %s)",
                           cl->chase_target->client->pers.greenname);
                Q_snprintf(rline, sizeof(rline), "%-16s%-18s%3i  %4i",
                           cl->pers.netname, chase, nframes / 600, cl->ping);
                Q_snprintf(headbuf, 1024, "yv %i string2 \"%s\"", y, rline);
            } else if (cl->resp.osp_r20c) {
                Q_snprintf(rline, sizeof(rline), "%-16s*** READY ***%3i  %4i", cl->pers.netname,
                        nframes / 600, cl->ping);
                Q_snprintf(headbuf, 1024, "yv %i string \"%s\"", y, rline);
            } else {
                Q_snprintf(rline, sizeof(rline), "%-16s [NOT READY] %3i  %4i", cl->pers.netname,
                        nframes / 600, cl->ping);
                Q_snprintf(headbuf, 1024, "yv %i string2 \"%s\"", y, rline);
            }
        }

appended:
        nchars = strlen(headbuf);
        if (outlen + nchars >= sizeof(buf))
            break;

        memcpy(buf + outlen, headbuf, nchars + 1);
        outlen += nchars;
    }

    if (level.intermission_framenum != 0 && sync_stat != 8)
        G_SetStat(ent, SID_OSP_LAYOUT1, OSP_CS(10));
    else
        G_SetStat(ent, SID_OSP_LAYOUT1, OSP_CS(9));

    gi.WriteByte(svc_layout);
    gi.WriteString(buf);

    if (level.intermission_framenum != 0 &&
        ent->client->resp.osp_entered == ENTERED_ENTERED) {
        char        *cur;

        Q_strlcpy(old_scores, buf, sizeof(old_scores));

        if ((cur = strchr(old_scores, '\r')))
            * cur = ' ';
    }
}

// The per-player stats page behind "showinfo <n>": name, an underline as
// long as the name, frags/efficiency, deaths/frags-per-hour, suicides/rank,
// one line per weapon the player has fired, and the two damage totals.  Built
// as one layout string and pushed straight down the wire; the caller does the
// unicast.  Efficiency is frags * 100 / (frags + deaths), and frags-per-hour
// is the high-score table's own frags * 36000 / frames.
void OSP_showPlayer(edict_t *ent)
{
    char        line[256];
    char        name[256];
    char        buf[1400];
    int         cid;
    unsigned int    i;
    int         frames;
    float       eff;
    int         y;
    int         frags;
    int         deaths;
    int         suicides;
    edict_t     *other;
    int         found;

    found = 0;

    if (ent->client->resp.osp_r2ac < 1) {
        gi.cprintf(ent, PRINT_CHAT, "** Sorry, illegal player view!\n");
        Cmd_InvUse_f(ent);
        return;
    }

    other = g_edicts + ent->client->resp.osp_r2ac;

    if (!other->inuse || !other->client) {
        gi.cprintf(ent, PRINT_CHAT, "** Sorry, player has disconnected!\n");
        Cmd_InvUse_f(ent);
        return;
    }

    frags = other->client->resp.score;
    deaths = other->client->resp.osp_r014;
    suicides = other->client->resp.osp_r2c0;

    Q_snprintf(name, sizeof(name), "Player: %s (%s)", other->client->pers.greenname, other->client->resp.osp_r0f4);
    Q_snprintf(buf, sizeof(buf), "xv 0 yv 0 string2 \"%s\"", name);

    Q_strlcpy(line, "_", sizeof(line));
    {
        for (cid = 0; cid < strlen(name) - 1 && cid < 59; cid++)
            Q_strlcat(line, "_", sizeof(line));
    }

    Q_snprintf(name, sizeof(name), "yv 4 string2 \"%s\"", line);
    Q_strlcat(buf, name, sizeof(buf));

    y = 18;

    // The zero case is a two-part disjunction whose second half is dead --
    // `frags < 1` already covers `!frags` -- and the redundancy is the
    // original's.
    if (frags < 1 || (!deaths && !frags))
        eff = 0;
    else
        eff = 100.0f * frags / (0.0f + frags + deaths);

    Q_snprintf(line, sizeof(line), "yv %d string \"Frags   :%3d     Efficiency: %.1f%%\"",
            y, frags, eff);
    Q_strlcat(buf, line, sizeof(buf));
    y += 8;

    if (level.intermission_framenum != 0)
        frames = endlvl_frame;
    else
        frames = level.framenum;

    {
        int     i;
        int     fph;            // invented name

        if (other->client->resp.enterframe < sync_frame)
            i = frames - sync_frame + 1;
        else
            i = frames - other->client->resp.enterframe + 1;

        if (i < 1) {
            i = 1;
            other->client->resp.enterframe = frames + 1;
            other->client->resp.osp_r2d4 = 1;
        }

        fph = frags * 36000 / i;
        Q_snprintf(line, sizeof(line), "yv %d string \"Deaths  :%3d     Frags/Hour: %d\"",
                y, deaths, fph);
    }
    Q_strlcat(buf, line, sizeof(buf));
    y += 8;

    Q_snprintf(line, sizeof(line), "yv %d string \"Suicides: %2d     Rank: %d/%d\"",
            y, suicides, other->client->resp.osp_r208, active_clients);
    Q_strlcat(buf, line, sizeof(buf));
    y += 16;

    cid = other->client->resp.clientid;
    if (cid < 0 || cid >= q_countof(p_acc)) {
        gi.cprintf(ent, PRINT_CHAT, "** Sorry, no stats for that player!\n");
        Cmd_InvUse_f(ent);
        return;
    }

    {
        int             index;          // invented name

        for (i = 0; a_info[i].name[0]; i++) {
            index = a_info[i].index;
            if (p_acc[cid].shots[index]) {
                Q_snprintf(line, sizeof(line), "yv %d string \"%s %.1f%% (%d/%d hits)\"", y,
                        a_info[i].name,
                        (double)(100 * p_acc[cid].hits[index]) /
                        p_acc[cid].shots[index],
                        p_acc[cid].hits[index],
                        p_acc[cid].shots[index]);
                Q_strlcat(buf, line, sizeof(buf));
                found = 1;
                y += 8;
            }
        }
    }

    if (!found) {
        Q_snprintf(line, sizeof(line), "yv %d string \"Hasn't taken a shot.\"", y);
        Q_strlcat(buf, line, sizeof(buf));
        y += 8;
    } else {
        y += 8;
        Q_snprintf(line, sizeof(line), "yv %d string2 \"Total damage given: %d\"", y,
                p_acc[cid].dgiven);
        Q_strlcat(buf, line, sizeof(buf));
        y += 8;
        Q_snprintf(line, sizeof(line), "yv %d string2 \"Total damage rcvd : %d\"", y,
                p_acc[cid].dtaken);
        Q_strlcat(buf, line, sizeof(buf));
        y += 8;
    }

    y += 8;
    Q_snprintf(line, sizeof(line), "yv %d cstring \"\x90 CONTINUE \x91\"", y);
    Q_strlcat(buf, line, sizeof(buf));

    gi.WriteByte(svc_layout);
    gi.WriteString(buf);
}

/*
==================
OSP_ScoreboardMessage

Tourney's ScoreboardMessage row.

The donor writes this dispatch inside DeathmatchScoreboardMessage in the shared
p_hud.c, which is where its scoreboard would collide with three other donors'.
Here it is one row of ruleset_ops_t and lives with the six writers it chooses
between, so p_hud.c keeps exactly one scoreboard -- baseq2's.

The order is the donor's and each arm is a different SCREEN, not a variant of
one: `resp.osp_r24c` is which alternate page the player asked for (player card,
MOTD, match parameters, previous match's scores), then the match mode decides
between the team board, the 1-vs-1 board and the deathmatch board, and finally
the hi-score table alternates with the deathmatch board during intermission on
a timer so both get seen.
==================
*/
void OSP_ScoreboardMessage(edict_t *ent, edict_t *killer)
{
    int     sorted[MAX_CLIENTS];
    int     sortedscores[MAX_CLIENTS];
    int     i, j, k, total, score;
    edict_t *cl_ent;

    // The alternate pages, in the donor's order.
    switch (ent->client->resp.osp_r24c) {
    case 8:
        OSP_showPlayer(ent);
        return;
    case 2:
        OSP_showMOTD();
        return;
    case 4:
        OSP_showParams();
        return;
    case 1:
        OSP_oldscores_cmd(ent);
        return;
    default:
        break;
    }

    if (G_Ruleset() == RULESET_TDM) {
        OSP_showTeamScores(ent);
        return;
    }
    if (G_Ruleset() == RULESET_DUEL) {
        OSP_show1v1Scores(ent);
        return;
    }

    // At intermission the hi-score table and the deathmatch board alternate:
    // 4 seconds on the board, 10 on the table, flipped by osp_r034.
    if (hs_mode && (int)client_highscores->value &&
        level.intermission_framenum) {
        if (level.framenum > ent->client->resp.osp_r244) {
            ent->client->resp.osp_r244 = level.framenum +
                                         (ent->client->resp.osp_r034 ? 40 : 100);
            ent->client->resp.osp_r034 = 1 - ent->client->resp.osp_r034;
        }
        if (!ent->client->resp.osp_r034) {
            OSP_showHighScores();
            return;
        }
    }

    total = 0;
    if (sync_stat < 4) {
        // Before the match is live there are no scores to sort by, so the order
        // is "players who have entered, then everyone else" -- which is what
        // the warmup board shows.
        for (i = 0; i < game.maxclients; i++) {
            cl_ent = g_edicts + i + 1;
            if (!cl_ent->inuse || !cl_ent->client)
                continue;

            for (j = 0; j < total; j++) {
                if (!cl_ent->client->resp.osp_r20c && cl_ent->osp_e39c != 1)
                    break;
                if (cl_ent->osp_e39c == 1 &&
                    (g_edicts[sorted[j] + 1].osp_e39c == 1 ||
                     game.clients[sorted[j]].resp.osp_r20c))
                    break;
            }

            for (k = total; k > j; k--)
                sorted[k] = sorted[k - 1];
            sorted[j] = i;
            total++;
        }
    } else {
        // Live: score first, then the two tie-breaks, both lower-is-better.
        for (i = 0; i < game.maxclients; i++) {
            cl_ent = g_edicts + i + 1;
            if (!cl_ent->inuse || !cl_ent->client)
                continue;

            score = cl_ent->client->resp.score;
            for (j = 0; j < total; j++) {
                if (score > sortedscores[j])
                    break;
                if (score == sortedscores[j]) {
                    if (game.clients[i].resp.osp_r014 <
                        game.clients[sorted[j]].resp.osp_r014)
                        break;
                    if (game.clients[i].resp.osp_r014 ==
                        game.clients[sorted[j]].resp.osp_r014 &&
                        game.clients[i].resp.osp_r2c0 <
                        game.clients[sorted[j]].resp.osp_r2c0)
                        break;
                }
            }

            for (k = total; k > j; k--) {
                sorted[k] = sorted[k - 1];
                sortedscores[k] = sortedscores[k - 1];
            }
            sorted[j] = i;
            sortedscores[j] = score;
            total++;
        }
    }

    OSP_showScores(sorted, total, ent);
}
