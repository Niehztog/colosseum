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
// osp_hiscore.c -- filename assigned by this tree.  The persistent per-map high score
// table.
//
// Ten entries of name/score/date plus a "set this session" flag, kept in the
// global p_table, rendered into the layout string hs_table, and written to
//   <basedir>/<gamedir>/<client_highscoredir>/<port>/<mapname>
// as one "FL\t<n>" or "TL\t<n>" header line followed by ten tab-separated
// rows.  hs_mode 1 = fraglimit (score is frags per hour), 2 = timelimit (score
// is the raw frag count); hs_limit caches whichever limit is in force, and a
// mismatch against the header line resets the file.

#include "g_local.h"
#include "tourney/osp_types.h"
#include <errno.h>

// The globals this TU owns, in .bss order.
int         hs_mode;
hs_player_t p_table[10];
char        hs_table[1400];
int         hs_limit;

void OSP_initHighScores(void)
{
    int     i;

    if (!(int)client_highscores->value) {
        hs_mode = 0;
        return;
    }

    if ((int)fraglimit->value) {
        hs_limit = (int)fraglimit->value;
        hs_mode = 1;
    } else if ((int)timelimit->value) {
        hs_limit = (int)timelimit->value;
        hs_mode = 2;
    } else {
        hs_mode = 0;
        gi.cvar_set("client_highscores", "0");
        return;
    }

    for (i = 0; i < 10; i++) {
        Q_strlcpy(p_table[i].name, "<empty>", sizeof(p_table[i].name));
        Q_strlcpy(p_table[i].score, "0", sizeof(p_table[i].score));
        Q_strlcpy(p_table[i].date, "01_Jan_70", sizeof(p_table[i].date));
        p_table[i].isnew = 0;
    }

    OSP_loadHighScores();
    OSP_formatHighScores();
}

void OSP_formatHighScores(void)
{
    char    line[1400];
    int     i;
    int     yvpos;
    char    tag;

    Q_snprintf(hs_table, sizeof(hs_table),
               "xv 0 yv 0 cstring \"High scores (%s)\"", level.mapname);

    if (hs_mode == 1)
        Q_snprintf(line, sizeof(line), "yv 8 cstring2 \"Fraglimit: %d\"",
                   hs_limit);
    else
        Q_snprintf(line, sizeof(line), "yv 8 cstring2 \"Timelimit: %d\"",
                   hs_limit);
    Q_strlcat(hs_table, line, sizeof(hs_table));

    if (hs_mode == 1)
        Q_strlcpy(line, "yv 24 cstring2 \"  # Name              FPH  Date     \"",
                  sizeof(line));
    else
        Q_strlcpy(line, "yv 24 cstring2 \"  # Name            Score  Date     \"",
                  sizeof(line));
    Q_strlcat(hs_table, line, sizeof(hs_table));

    yvpos = 34;
    i = 0;

    {
        for (; i < 10; i++) {
            if (p_table[i].isnew)
                tag = '*';
            else
                tag = ' ';

            Q_snprintf(line, sizeof(line),
                       "yv %d cstring \"%c%2d %-16s%5s  %s\"", yvpos, tag,
                       i + 1, p_table[i].name, p_table[i].score,
                       p_table[i].date);
            Q_strlcat(hs_table, line, sizeof(hs_table));
            yvpos += 8;
        }
    }
}

void OSP_showHighScores(void)
{
    gi.WriteByte(svc_layout);
    gi.WriteString(hs_table);
}

void OSP_updateHighScores(void)
{
    char    stamp[OSP_HS_FIELD];
    int     i;
    int     j;
    int     rowi;
    int     framecnt;
    int     value;
    int     allowed;
    edict_t *ent;

    allowed = 1;
    rowi = 10;

    if (!active_clients)
        return;

    for (i = 1; i <= game.maxclients; i++) {
        ent = g_edicts + i;

        if (!ent->inuse || !ent->client ||
            ent->client->resp.osp_entered != ENTERED_ENTERED ||
            ent->client->resp.osp_r208 != 1 ||
            (ent->flags & FL_BOT))
            continue;

        if (hs_mode == 2)
            value = ent->client->resp.score;
        else {
            if (ent->client->resp.enterframe < sync_frame)
                framecnt = level.framenum - sync_frame + 1;
            else
                framecnt = level.framenum - ent->client->resp.enterframe + 1;

            if (framecnt < 1)
                framecnt = 1;

            value = ent->client->resp.score * 36000 / framecnt;

            // A short game cannot inflate the frags-per-hour past 1250.
            if (value > 1250 && framecnt < 600)
                value = 1250;

            if (!(int)fraglimit->value ||
                ent->client->resp.score * 100 / (int)fraglimit->value < 90)
                allowed = 0;
        }

        if (allowed) {
            for (rowi = 0; rowi < 10; rowi++) {
                if (value >= Q_atoi(p_table[rowi].score) ||
                    !strcmp(p_table[rowi].name, "<empty>")) {
                    for (j = 9; j > rowi; j--) {
                        memcpy(&p_table[j], &p_table[j - 1],
                               sizeof(p_table[j]));
                    }

                    Q_strlcpy(p_table[rowi].name, ent->client->pers.netname,
                              sizeof(p_table[rowi].name));
                    Q_snprintf(stamp, sizeof(stamp), "%d", value);
                    Q_strlcpy(p_table[rowi].score, stamp,
                              sizeof(p_table[rowi].score));
                    OSP_highscoreDate(stamp);
                    Q_strlcpy(p_table[rowi].date, stamp,
                              sizeof(p_table[rowi].date));
                    p_table[rowi].isnew = 1;
                    break;
                }
            }
        } else
            rowi = 10;

        break;
    }

    if (rowi < 10) {
        OSP_formatHighScores();
        OSP_writeHighScores();
    }
}

void OSP_loadHighScores(void)
{
    char    name[OSP_HS_FIELD];
    char    score[OSP_HS_FIELD];
    char    date[OSP_HS_FIELD];
    char    file[MAX_OSPATH];
    char    dir[MAX_OSPATH];
    int     i;
    FILE    *f = NULL;
    cvar_t  *port;
    cvar_t  *hsdir;

    port = gi.cvar("port", "27910", 0);
    hsdir = gi.cvar("client_highscoredir", "highscores", 0);

    {
        Q_snprintf(dir, sizeof(dir), "%s/%s", G_FsBaseDir(), G_FsGameDir());
        if (Q_snprintf(file, sizeof(file), "%s/%s/%d/%s", dir, hsdir->string,
                       (int)port->value, level.mapname) >= sizeof(file)) {
            gi.dprintf("High score path too long.\n");
            return;
        }

        f = fopen(file, "r");

        if (f) {
            if (!OSP_readLine(f, name, score, date)) {
                fclose(f);
                return;
            }

            if ((hs_mode == 1 && (strcmp(name, "FL") || hs_limit != Q_atoi(score))) ||
                (hs_mode == 2 && (strcmp(name, "TL") || hs_limit != Q_atoi(score)))) {
                gi.dprintf("Server parameters changed, resetting highscores.\n");
                fclose(f);
                OSP_writeHighScores();
                return;
            }

            for (i = 0; i < 10; i++) {
                if (OSP_readLine(f, name, score, date) != 3) {
                    gi.dprintf("Not all players (high scores) loaded.\n");
                    fclose(f);
                    return;
                }

                Q_strlcpy(p_table[i].name, name, sizeof(p_table[i].name));
                Q_strlcpy(p_table[i].score, score, sizeof(p_table[i].score));
                Q_strlcpy(p_table[i].date, date, sizeof(p_table[i].date));
            }

            fclose(f);
            gi.dprintf("High scores loaded.\n");
        } else {
            if (!OSP_makeHSDir(dir))
                return;

            gi.dprintf("\nNew \"%s\" created.\n\n", file);
            OSP_writeHighScores();
        }
    }
}

void OSP_writeHighScores(void)
{
    char    line[256];
    char    file[MAX_OSPATH];
    char    dir[MAX_OSPATH];
    int     i;
    FILE    *f = NULL;
    cvar_t  *port;
    cvar_t  *hsdir;

    port = gi.cvar("port", "27910", 0);
    hsdir = gi.cvar("client_highscoredir", "highscores", 0);

    {
        Q_snprintf(dir, sizeof(dir), "%s/%s", G_FsBaseDir(), G_FsGameDir());
        if (Q_snprintf(file, sizeof(file), "%s/%s/%d/%s", dir, hsdir->string,
                       (int)port->value, level.mapname) >= sizeof(file)) {
            gi.dprintf("High score path too long.\n");
            return;
        }

        f = fopen(file, "w+");

        if (!f) {
            gi.dprintf("Couldn't write high score table (%d)\n", errno);
            return;
        }

        if (hs_mode == 2)
            Q_snprintf(line, sizeof(line), "TL\t%d\n", hs_limit);
        else
            Q_snprintf(line, sizeof(line), "FL\t%d\n", hs_limit);
        fputs(line, f);

        for (i = 0; i < 10; i++) {
            Q_snprintf(line, sizeof(line), "%s\t%s\t%s\n", p_table[i].name,
                       p_table[i].score, p_table[i].date);
            fputs(line, f);
        }

        fclose(f);
    }
}

bool OSP_makeHSDir(char *base)
{
    char    num[32];
    char    dir[MAX_OSPATH];
    cvar_t  *port;
    cvar_t  *hsdir;

    port = gi.cvar("port", "27910", 0);
    hsdir = gi.cvar("client_highscoredir", "highscores", 0);

    Q_snprintf(dir, sizeof(dir), "%s/%s", base, hsdir->string);

    // mkdir() is called unprototyped, and the original really did write two
    // different call shapes per platform.
#ifdef _WIN32
    if (mkdir(dir) && errno == ENOENT)
#else
    if (mkdir(dir, 0755) && errno == ENOENT)
#endif
    {
        gi.dprintf("Couldn't make %s, aborting.\n", dir);
        gi.cvar_set("client_highscores", "0");
        return false;
    }

    Q_snprintf(num, sizeof(num), "/%d", (int)port->value);
    Q_strlcat(dir, num, sizeof(dir));

#ifdef _WIN32
    if (mkdir(dir) && errno == ENOENT)
#else
    if (mkdir(dir, 0755) && errno == ENOENT)
#endif
    {
        gi.dprintf("Couldn't make %s, aborting.\n", dir);
        gi.cvar_set("client_highscores", "0");
        return false;
    }

    return true;
}

// `a`, `b` and `c` are OSP_HS_FIELD bytes each.
int OSP_readLine(FILE *f, char *a, char *b, char *c)
{
    char    line[1024];
    char    *linep;
    char    *pstr;
    char    *t;                     // invented name
    char    *val2;                      // invented name

    if (!fgets(line, 1024, f)) {
        gi.dprintf("Error reading highscores file\n");
        return 0;
    }

    linep = strchr(line, '\r');
    if (linep)
        *linep = 0;

    linep = strchr(line, '\n');
    if (linep)
        *linep = 0;

    t = line;
    Q_strlcpy(a, t, OSP_HS_FIELD);

    linep = strchr(t, '\t');
    if (!linep)
        return 1;

    *linep++ = 0;
    pstr = linep;
    Q_strlcpy(a, t, OSP_HS_FIELD);
    Q_strlcpy(b, pstr, OSP_HS_FIELD);

    linep = strchr(pstr, '\t');
    if (!linep)
        return 2;

    *linep++ = 0;
    val2 = linep;
    Q_strlcpy(b, pstr, OSP_HS_FIELD);
    Q_strlcpy(c, val2, OSP_HS_FIELD);
    return 3;
}

// `out` is OSP_HS_FIELD bytes.
void OSP_highscoreDate(char *out)
{
    time_t      tval;
    char        *month;
    struct tm   *tm;
    int         monidx;

    time(&tval);
    tm = localtime(&tval);
    monidx = tm->tm_mon;

    if (monidx == 0)
        month = "Jan";
    else if (monidx == 1)
        month = "Feb";
    else if (monidx == 2)
        month = "Mar";
    else if (monidx == 3)
        month = "Apr";
    else if (monidx == 4)
        month = "May";
    else if (monidx == 5)
        month = "Jun";
    else if (monidx == 6)
        month = "Jul";
    else if (monidx == 7)
        month = "Aug";
    else if (monidx == 8)
        month = "Sep";
    else if (monidx == 9)
        month = "Oct";
    else if (monidx == 10)
        month = "Nov";
    else
        month = "Dec";

    Q_snprintf(out, OSP_HS_FIELD, "%.2d_%s_%.2d", tm->tm_mday, month,
               tm->tm_year % 100);
}
