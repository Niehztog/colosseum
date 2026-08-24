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
// it (sec 5.7).  Written for this tree.
//
// WHY IT IS ITS OWN FILE and not four functions at the bottom of g_main.c,
// where GetGameAPIEx lives.  `G_FsFreeFile` releases a buffer the ENGINE
// allocated, so the free's operand has no allocation site in the calling file
// -- and g_main.c is one of the two files in the tree that uses both allocator
// families (it strdups sv_maplist and gi.TagMallocs g_edicts).  R-VER-22's
// UNKNOWN check is exactly "a free whose operand was never allocated here, in a
// file that has already shown it can confuse the families", and it reported
// this the moment the shim landed.  The check is right about the shape and
// wrong about this instance; the answer is to put the shim somewhere the
// precondition is false rather than to weaken the check (doc/reconciliation.md
// R-55, R-63, R-95).
//
// Everything here answers sensibly when the engine offers no extension at all.
// q2pro exposes DEBUG_DRAW_API_V1 only in a non-dedicated build with a
// renderer and USE_DEBUG on, so for a dedicated server the fallback R-BOT-27
// asks for is the normal case rather than the exception.

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
        gi.dprintf("Colosseum: filesystem extension %s (R-BOT-8)\n",
                   fs ? "available" : "ABSENT -- falling back to the cvars");
    }
    return fs;
}

const char *G_FsBaseDir(void)
{
    cvar_t *cvar = gi.cvar("basedir", "", 0);

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
    cvar_t *cvar = gi.cvar("gamedir", "", 0);

    if (cvar && *cvar->string)
        return cvar->string;
    cvar = gi.cvar("game", "", 0);
    if (cvar && *cvar->string)
        return cvar->string;
    return GAMEVERSION;
}

int G_FsLoadFile(const char *path, void **buffer)
{
    const filesystem_api_v1_t *fs = G_Fs();

    if (!fs) {
        *buffer = NULL;
        return -1;
    }
    return fs->LoadFile(path, buffer, 0, TAG_LEVEL);
}

void G_FsFreeFile(void *buffer)
{
    if (buffer)
        gi.TagFree(buffer);
}

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

