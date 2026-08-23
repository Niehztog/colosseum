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
// sl_write.c -- the Standard Log's low-level writers: one function per record
// field, plus the log file's own open/close.
//
// StdLog 1.2 is not a NetGames USA format and has nothing to do with ngLog
// beyond having borrowed its file writer in v2.75 -- which is why the two were
// mutually exclusive there, and why `sl_log_method` was force-cleared whenever
// ngLog logging was on.  The writer is local to this file now, so the Standard
// Log and the JSON stats log (osp_stats.c) run independently of each other.

#include "g_local.h"
#include "tourney/osp_types.h"

#include <errno.h>

static FILE     *sl_file;
static int      sl_buffered;        // lines written since the last flush

// sl_log_flush: 0 = let the system buffer decide, 1 = flush every 40 lines,
// 2 (the default) = flush every line.
#define SL_BUFFER_LINES 40

void sl_LogMapName(game_import_t *import, char *mapname)
{
    char    output[1024];

    Q_snprintf(output, sizeof(output), "\t\tMap\t%s", mapname);
    sl_logWrite(output);
}

void sl_LogGameStart(game_import_t *import, float time)
{
    char    output[1024];

    Q_snprintf(output, sizeof(output), "\t\tGameStart\t\t\t%d", (int)time);
    sl_logWrite(output);
}

// The Standard Log dialects, and there is one.  v2.75's own documentation says
// so in as many words -- `sl_log_style: * 0 = Standard Logging v1.2 (only
// available option)` -- and the two halves of that sentence are named here,
// beside the record that carries the version into the log, so the number an
// operator sets and the string the log says cannot come apart.
#define SL_STYLE_1_2    0
#define SL_VERSION_1_2  "1.2"

void sl_LogVers(game_import_t *import)
{
    sl_logWrite("\t\tStdLog\t" SL_VERSION_1_2);
}

void sl_LogPatch(game_import_t *import, char *patch)
{
    char    output[1024];

    Q_snprintf(output, sizeof(output), "\t\tPatchName\t%s", patch);
    sl_logWrite(output);
}

static const char *const sl_months[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

void sl_LogDate(game_import_t *import)
{
    char        output[1024];
    time_t      tval;
    struct tm   *lt;
    int         monthnum;

    time(&tval);
    lt = localtime(&tval);
    if (!lt)
        return;

    monthnum = lt->tm_mon;
    if (monthnum < 0 || monthnum > 11)
        monthnum = 11;

    Q_snprintf(output, sizeof(output), "\t\tLogDate\t%.2d %s %d", lt->tm_mday,
               sl_months[monthnum], lt->tm_year + 1900);
    sl_logWrite(output);
}

void sl_LogTime(game_import_t *import)
{
    char        output[1024];
    time_t      t;
    struct tm   *tmp;

    time(&t);
    tmp = localtime(&t);
    if (!tmp)
        return;

    Q_snprintf(output, sizeof(output), "\t\tLogTime\t%.2d:%.2d:%.2d",
               tmp->tm_hour, tmp->tm_min, tmp->tm_sec);
    sl_logWrite(output);
}

void sl_LogDeathFlags(game_import_t *import, unsigned long flags)
{
    char    output[1024];

    Q_snprintf(output, sizeof(output), "\t\tLogDeathFlags\t%lu", flags);
    sl_logWrite(output);
}

void sl_LogGameEnd(game_import_t *import, float time)
{
    char    output[1024];

    Q_snprintf(output, sizeof(output), "\t\tGameEnd\t\t\t%d", (int)time);
    sl_logWrite(output);
}

void sl_CloseLogFile(void)
{
    if (sl_file) {
        fflush(sl_file);
        fclose(sl_file);
        sl_file = NULL;
    }
    sl_buffered = 0;
    sl_status = 0;
}

void sl_LogPlayerConnect(game_import_t *import, char *name, int unused,
                         float time)
{
    char    output[1024];

    Q_snprintf(output, sizeof(output), "\t\tPlayerConnect\t%s\t\t%d", name,
               (int)time);
    sl_logWrite(output);
}

void sl_LogPlayerLeft(game_import_t *import, char *name, float time)
{
    char    output[1024];

    Q_snprintf(output, sizeof(output), "\t\tPlayerLeft\t%s\t\t%d", name,
               (int)time);
    sl_logWrite(output);
}

void sl_LogPlayerRename(game_import_t *import, char *oldname, char *newname,
                        float time)
{
    char    output[1024];

    Q_snprintf(output, sizeof(output), "\t\tPlayerRename\t%s\t%s\t%d", oldname,
               newname, (int)time);
    sl_logWrite(output);
}

// The record is "<scorer> <other> <event> <weapon> <score> <time> <ping>",
// with the second field left empty for a suicide.
void sl_LogScore(game_import_t *import, char *player, char *other, char *event,
                 char *weapon, int score, float time, int ping)
{
    char    output[1024];

    Q_snprintf(output, sizeof(output), "%s\t%s\t%s\t%s\t%d\t%d\t%d",
               player ? player : "", other ? other : "", event ? event : "",
               weapon ? weapon : "", score, (int)time, ping);

    sl_logWrite(output);
}

int sl_OpenLogFile(game_import_t *import)
{
    if (sl_status) {
        sl_status = 2;
        return 2;
    }

    sl_log_method = gi.cvar("sl_log_method", "0", 0);
    sl_filename = gi.cvar("sl_filename", "stdlog.log", 0);
    sl_log_style = gi.cvar("sl_log_style", "0", 0);
    sl_log_flush = gi.cvar("sl_log_flush", "2", 0);

    // Bit 0 is "record to a local file".  The UDP and TCP collector bits the
    // Standard Log defines were never implemented here, and the shipped
    // documentation says so.
    if (!((int)sl_log_method->value & 1) || !sl_filename->string[0]) {
        sl_status = 0;
        return 0;
    }

    // `sl_log_style`'s reader, and it is a CHECK rather than a switch because
    // there is one dialect to select.  Registering a cvar, documenting its one
    // legal value and then never reading it means a server that asks for a
    // dialect which does not exist gets a 1.2 log and no way to find out; a
    // second dialect invented to make the cvar interesting would be a feature
    // and not an import, so what the read owes the operator is the refusal.
    //
    // RESET, NOT REFUSE.  A bad value becoming the default is what the OSP
    // ratios already do, and it is the right arm here: the operator asked for a
    // log, and turning one mistyped number into no log at all answers a
    // different question.  Writing the cvar back is what makes this once per
    // config that sets it rather than once per level -- the next open reads the
    // corrected value and passes in silence.
    //
    // After the `sl_log_method` gate above, so a server that is not logging is
    // not lectured about the format it is not writing.
    if ((int)sl_log_style->value != SL_STYLE_1_2) {
        gi.dprintf("Standard Log style %d does not exist; using %d "
                   "(Standard Logging v%s), which is the only one.\n",
                   (int)sl_log_style->value, SL_STYLE_1_2, SL_VERSION_1_2);
        gi.cvar_set("sl_log_style", va("%d", SL_STYLE_1_2));
    }

    // sl_filename keeps v2.75's meaning: a path relative to the Quake II
    // base directory, not to the game directory.
    sl_file = fopen(sl_filename->string, "a");
    if (!sl_file) {
        gi.dprintf("Couldn't create Standard Log \"%s\": %d\n",
                   sl_filename->string, errno);
        sl_status = 0;
        return 0;
    }

    sl_buffered = 0;
    sl_status = 1;
    gi.dprintf("Standard Log logging enabled (%s).\n", sl_filename->string);

    return 1;
}

void sl_logWrite(char *line)
{
    if (!sl_status || !sl_file)
        return;

    if (fprintf(sl_file, "%s\n", line) < 0) {
        gi.dprintf("Error writing to Standard Log: %d\n", errno);
        sl_CloseLogFile();
        return;
    }

    if (!(int)sl_log_flush->value)
        return;

    if ((int)sl_log_flush->value == 1) {
        if (++sl_buffered < SL_BUFFER_LINES)
            return;
        sl_buffered = 0;
    }

    fflush(sl_file);
}
