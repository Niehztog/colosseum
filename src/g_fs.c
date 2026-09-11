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
// g_fs.c -- the engine's extended API, as the game library is allowed to see
// it.  Written for this tree.
//
// Why it is its own file and not four functions at the bottom of g_main.c,
// where GetGameAPIEx lives.  `G_FsFreeFile` releases a buffer the ENGINE
// allocated, so the free's operand has no allocation site in the calling file
// -- and g_main.c is one of the two files in the tree that uses both allocator
// families (it strdups sv_maplist and gi.TagMallocs g_edicts).  The audit's
// UNKNOWN check is exactly "a free whose operand was never allocated here, in a
// file that has already shown it can confuse the families", and it reported
// this the moment the shim landed.  The check is right about the shape and
// wrong about this instance; the answer is to put the shim somewhere the
// precondition is false rather than to weaken the check.
//
// Everything here answers sensibly when the engine offers no extension at all.
// q2pro exposes DEBUG_DRAW_API_V1 only in a non-dedicated build with a
// renderer and USE_DEBUG on, so for a dedicated server the fallback is the
// normal case rather than the exception.
//
// And "no extension at all" is not only the old engines.  A PROXY GAME LIBRARY
// puts the same condition in front of a current one: q2admin is loaded as the
// game, dlopens this library and forwards `GetGameAPI` -- only that one,
// because `GetGameAPIEx` is q2pro's own addition and q2admin does not export
// it.  `gex` is then NULL on a current engine and every extension in this
// file reports absent.

#include "g_local.h"
#include "shared/files.h"

static const filesystem_api_v1_t *G_Fs(void)
{
    static const filesystem_api_v1_t *fs;
    static bool asked;

    if (!asked) {
        asked = true;
        if (gex && gex->GetExtension)
            fs = gex->GetExtension(FILESYSTEM_API_V1);
        gi.dprintf("Colosseum: filesystem extension %s\n",
                   fs ? "available"
                      : "ABSENT -- reads fall back to stdio under "
                        "basedir/gamedir, and nothing inside a pak is visible");
    }
    return fs;
}

// `basedir` and `gamedir` are the ENGINE's, and this library asks for both
// from six other files as well.  A re-obtain with a different default is a
// collision -- whichever registration runs first decides, and `counts.py
// --duplicates` reported exactly that pair.  So the defaults live here, once,
// and every other site goes through these two functions.  On a real server the
// engine has registered both long before, and the defaults are what a tool
// sees rather than what a player does; that is still a difference that should
// not depend on call order.
#define FS_BASEDIR_DEFAULT  "."
#define FS_GAMEDIR_DEFAULT  ""

const char *G_FsBaseDir(void)
{
    cvar_t *cvar = gi.cvar("basedir", FS_BASEDIR_DEFAULT, 0);

    // Q2PRO's own name for it.  `basedir` is the 1997 spelling and q2pro keeps
    // it only as an alias on some builds, so ask for both and take the first
    // that answers rather than handing the brain an empty path.
    if (cvar && *cvar->string)
        return cvar->string;
    cvar = gi.cvar("fs_basedir", "", 0);
    if (cvar && *cvar->string)
        return cvar->string;
    return ".";
}

const char *G_FsGameDir(void)
{
    cvar_t *cvar = gi.cvar("gamedir", FS_GAMEDIR_DEFAULT, 0);

    if (cvar && *cvar->string)
        return cvar->string;
    cvar = gi.cvar("game", "", 0);
    if (cvar && *cvar->string)
        return cvar->string;
    return GAMEVERSION;
}

// `homedir` the same way, and for the same reason: two sites in this file now
// ask for it -- the writer below and the reader further down -- and a second
// `gi.cvar` with a different default would be the collision the paragraph above
// describes.  NULL rather than "" when it is unset, because both callers have
// to branch on that rather than compose a path against an empty root.
static const char *G_FsHomeDir(void)
{
    cvar_t *cvar = gi.cvar("homedir", "", 0);

    return (cvar && *cvar->string) ? cvar->string : NULL;
}

// Where the library WRITES.  Three writers in this tree built their paths from
// the gamedir alone -- `fopen("colosseum/stdlog.log")` -- which resolves
// against the server's working directory rather than against the installation,
// so a server started from anywhere else wrote its log into a directory that
// may not exist and reported "Error opening log file" with the right name in
// it.  The engine writes under `homedir` when there is one and under `basedir`
// otherwise, and so does this.
//
// Returns false when the composed path did not fit, so a caller can say so
// rather than opening a truncated name.
bool G_FsGamePath(char *out, size_t size, const char *name)
{
    const char *home = G_FsHomeDir();
    const char *base = home ? home : G_FsBaseDir();

    return Q_snprintf(out, size, "%s/%s/%s", base, G_FsGameDir(), name) < size;
}

// The read roots, in the engine's own order: `homedir` before `basedir`, with
// the gamedir under each.  `G_FsGamePath` composes that same pair for the
// WRITERS and is deliberately not reused here -- a writer picks one root and
// stops, a reader has to try both.
//
// The third arm is the path exactly as the caller wrote it, resolved against
// the server's working directory.  That is the literal 1999 `fopen`, and it is
// LAST rather than first for the reason maploop.c, gslog.c and ra2stats.c each
// carry against their own readers: a server started from anywhere but the
// installation resolves a bare relative path somewhere else entirely.  It is
// present at all because an operator who points `botfile` at an absolute path
// means that path, and no composed root can reach it.
static FILE *G_FsOpenStdio(const char *path, char *out, size_t size)
{
    const char *roots[2];
    FILE *fp;
    int i;

    roots[0] = G_FsHomeDir();
    roots[1] = G_FsBaseDir();

    for (i = 0; i < 2; i++) {
        if (!roots[i] || !*roots[i])
            continue;
        // Truncation is a miss, not a shorter name to try: a composed path cut
        // at 255 bytes names a different file.
        if (Q_snprintf(out, size, "%s/%s/%s", roots[i], G_FsGameDir(), path) >= size)
            continue;
        fp = fopen(out, "rb");
        if (fp)
            return fp;
    }

    if (Q_strlcpy(out, path, size) >= size)
        return NULL;
    return fopen(out, "rb");
}

// The engine caps a load at MAX_LOADFILE -- `inc/common/files.h`, 64 MiB and
// some slop.  The number is repeated rather than the header included, because
// `inc/common/` is the engine's own and nothing in `src/` reaches into it; the
// cap is here because this arm has no pak directory to ask how large an entry
// is before reading it, and `botfile` is an operator's cvar naming any file on
// the disk.
#define G_FS_MAX_LOADFILE   0x4001000

// R-ENG-4.  The extension when the engine offers one -- that is the arm that
// can see inside a `.pak` or `.pkz`, and it is why the extension is preferred
// whenever it is there -- and the 1999 stdio path when it does not.
//
// The requirement always said the stdio path "remains as fallback" and this
// function never had one: it returned -1, and its one caller answered with
// `error opening botcfg/bots.cfg` about a file lying in the gamedir where it
// belongs.  Behind a proxy that is every server, and `No configured bots to
// add!` is every `addbot` on it -- R-COMPAT-2 failing for a reason that has
// nothing to do with the assets it names.
int G_FsLoadFile(const char *path, void **buffer)
{
    const filesystem_api_v1_t *fs = G_Fs();
    char resolved[MAX_OSPATH];
    FILE *fp;
    long len;
    void *data;

    *buffer = NULL;

    if (fs)
        return fs->LoadFile(path, buffer, 0, TAG_LEVEL);

    // Binary, and not as a habit: the length returned here is the length the
    // caller parses against, and a text-mode read on Windows drops the CR of
    // every CRLF -- which is what a bots.cfg edited there is full of -- so the
    // `fread` below would come up short of `ftell` and every load would fail.
    // The extension's arm hands over raw bytes; so does this one.
    fp = G_FsOpenStdio(path, resolved, sizeof(resolved));
    if (!fp)
        return -1;

    if (fseek(fp, 0, SEEK_END) || (len = ftell(fp)) < 0 ||
        fseek(fp, 0, SEEK_SET)) {
        gi.dprintf("couldn't measure %s\n", resolved);
        fclose(fp);
        return -1;
    }
    if (len > G_FS_MAX_LOADFILE) {
        gi.dprintf("%s is %ld bytes, past the %d-byte load limit\n",
                   resolved, len, G_FS_MAX_LOADFILE);
        fclose(fp);
        return -1;
    }

    // gi.TagMalloc and not malloc, because G_FsFreeFile frees BOTH arms with
    // gi.TagFree and the extension's arm allocated through the engine.  One
    // free means one allocator.  The trailing NUL is the engine loader's own
    // contract -- a caller is allowed to treat the buffer as a string -- and is
    // not counted in the returned length.
    data = gi.TagMalloc((unsigned)len + 1, TAG_LEVEL);
    if (fread(data, 1, len, fp) != (size_t)len) {
        // A directory arrives here rather than at the fopen: opening one
        // succeeds on Linux and only the read refuses.
        gi.dprintf("error reading %s\n", resolved);
        gi.TagFree(data);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    ((byte *)data)[len] = 0;

    *buffer = data;
    return (int)len;
}

void G_FsFreeFile(void *buffer)
{
    if (buffer)
        gi.TagFree(buffer);
}

// No stdio arm here, and that is a decision rather than the same omission
// again.  R-ENG-4's fallback is the 1999 `fopen`, and 1999 had no directory
// enumeration to fall back to: the `bots/*.cfg` sweep is this tree's own
// addition (R-BOT-26), whose whole point is that the engine can see those files
// inside a pak.  Returning no list degrades to exactly the donor's behaviour --
// `bots.cfg` alone, which the loader above still reads -- so a server behind a
// proxy gets its bots, and only the sub-folder sweep is missing.
char **G_FsListFiles(const char *path, const char *ext, int *count)
{
    const filesystem_api_v1_t *fs = G_Fs();

    *count = 0;
    if (!fs)
        return NULL;
    return (char **)fs->ListFiles(path, ext, FS_SEARCH_SAVEPATH, count);
}

void G_FsFreeFileList(char **list)
{
    const filesystem_api_v1_t *fs = G_Fs();

    if (fs && list)
        fs->FreeFileList((void **)list);
}

const debug_draw_api_v1_t *G_DebugDraw(void)
{
    static const debug_draw_api_v1_t *dd;
    static bool asked;

    if (!asked) {
        asked = true;
        if (gex && gex->GetExtension)
            dd = gex->GetExtension(DEBUG_DRAW_API_V1);
    }
    return dd;
}

