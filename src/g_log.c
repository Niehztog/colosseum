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
//===========================================================================
//
// Name:        g_log.c
// Function:    the Gladiator game log
// Programmer:  Mr Elusive (MrElusive@demigod.demon.nl), 1997-12-31
//
// From gladiator-bot-restored/game/g_log.c, which the 1999 module carried
// behind `#define LOGFILE`.  Each of those defines becomes a cvar and none is
// compiled out, so the switch is `g_gamelog`: a FILENAME rather than a flag,
// because the feature's whole content is which file it writes to, and "" is
// the natural off.
//
// Three changes from the donor, each because this tree is not that one:
//
//   * the name is resolved under the gamedir, the way every other writer in
//     this tree does it (ra2stats, osp_stats, gslog).  The donor took the
//     console's argument as a path, which let `sv openlog ../../anything`
//     write outside the game directory.
//   * `Log_Write`/`Log_WriteTimeStamped` carry `q_printf` so the compiler
//     checks their callers, and `writelog` passes the operator's text as an
//     ARGUMENT rather than as the format.
//   * the timestamp is computed from `level.framenum`, because q2pro's
//     conversion made frame numbers the unit that cannot drift.
//     The donor's four-field h:mm:ss:cc shape is preserved exactly.
//===========================================================================

#include "g_local.h"

#define MAX_LOGFILENAMESIZE     1024

static char logfilename[MAX_LOGFILENAMESIZE];
static FILE *logfp;
static int  lognumwrites;

void Log_Open(const char *filename)
{
    char path[MAX_LOGFILENAMESIZE];

    if (!filename || !*filename) {
        gi.dprintf("openlog <filename>\n");
        return;
    }
    if (logfp) {
        gi.dprintf("The log file %s is already opened\n", logfilename);
        return;
    }

    // Under the gamedir, and no path of the caller's own: the console is the
    // only caller today, and a log that can be aimed anywhere is a foothold
    // rather than a feature.
    if (strchr(filename, '/') || strchr(filename, '\\') || strstr(filename, "..")) {
        gi.dprintf("openlog: %s -- the name may not contain a path\n", filename);
        return;
    }

    if (!G_FsGamePath(path, sizeof(path), filename)) {
        gi.dprintf("openlog: %s -- the composed path is too long\n", filename);
        return;
    }

    logfp = fopen(path, "wb");
    if (!logfp) {
        gi.dprintf("Error opening log file %s\n", path);
        return;
    }
    Q_strlcpy(logfilename, path, sizeof(logfilename));
    lognumwrites = 0;
    gi.dprintf("Opened log %s\n", logfilename);
}

void Log_Close(void)
{
    if (!logfp) {
        gi.dprintf("no log file to close\n");
        return;
    }
    if (fclose(logfp)) {
        gi.dprintf("Error closing log file %s\n", logfilename);
        logfp = NULL;
        return;
    }
    logfp = NULL;
    gi.dprintf("Closed log %s\n", logfilename);
}

// Called from ShutdownGame: the donor's own ShutdownGame did this, and a
// library that is dlclose()d with a FILE * still open loses whatever was
// buffered.
void Log_ShutDown(void)
{
    if (logfp)
        Log_Close();
}

void Log_Write(const char *fmt, ...)
{
    va_list ap;

    if (!logfp)
        return;
    va_start(ap, fmt);
    vfprintf(logfp, fmt, ap);
    va_end(ap);
    fprintf(logfp, "\r\n");
    fflush(logfp);
}

void Log_WriteTimeStamped(const char *fmt, ...)
{
    va_list ap;
    int     centis;

    if (!logfp)
        return;

    // level.framenum is 10 Hz, so hundredths come out in steps of ten -- which
    // is what the 1999 log did too, since level.time advanced by 0.1 as well.
    centis = level.framenum * 10;
    fprintf(logfp, "%d   %02d:%02d:%02d:%02d   ",
            lognumwrites,
            centis / 360000,
            (centis / 6000) % 60,
            (centis / 100) % 60,
            centis % 100);
    va_start(ap, fmt);
    vfprintf(logfp, fmt, ap);
    va_end(ap);
    fprintf(logfp, "\r\n");
    lognumwrites++;
    fflush(logfp);
}

// `sv openlog|closelog|writelog`, from g_svcmds.c's ServerCommand.  Returns
// true when it recognised the command, the shape the donor used so the caller
// can chain it against the bot commands.
bool LogCmd(const char *cmd)
{
    if (Q_stricmp(cmd, "openlog") == 0) {
        //NOTE: 2 because it's an "sv" command
        Log_Open(gi.argv(2));
    } else if (Q_stricmp(cmd, "closelog") == 0) {
        Log_Close();
    } else if (Q_stricmp(cmd, "writelog") == 0) {
        // Everything after the command, not gi.argv(2): the donor logged the
        // first WORD of the line, so `sv writelog round two starts` wrote
        // "round".
        //
        // And the text is an ARGUMENT.  The donor passed it as the format
        // string, so `sv writelog %n` wrote through a stack pointer.
        const char *args = gi.args();
        const char *text = args ? args : "";

        // gi.args() is everything from argv(1), which for `sv writelog ...`
        // starts with "writelog ".
        if (!Q_strncasecmp(text, "writelog", 8)) {
            text += 8;
            while (*text == ' ')
                text++;
        }
        Log_WriteTimeStamped("%s", text);
    } else {
        return false;
    }
    return true;
}

// For `sv extras`.  The state of the log, without exposing the
// FILE * to anybody who might close it.
bool Log_IsOpen(void)
{
    return logfp != NULL;
}

const char *Log_Path(void)
{
    return logfp ? logfilename : "";
}

int Log_Writes(void)
{
    return lognumwrites;
}
