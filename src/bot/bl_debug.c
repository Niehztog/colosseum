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
// Debug lines, from osp-tourney@1d8427e -- code-identical to the 1999 original
// apart from the engine-extension path.
//===========================================================================
//
// Name:                bl_debug.c
// Function:        debug functions
// Programmer:      Mr Elusive (MrElusive@demigod.demon.nl)
// Last update: 1999-02-10
// Tab Size:        3
//===========================================================================

#include "g_local.h"
#include "bot/bl_main.h"
#include "bot/bl_debug.h"

/*
===============================================================================

Two implementations of one interface

The 1999 scheme gives every debug line an EDICT: a non-solid RF_BEAM entity
whose origin and old_origin are the endpoints and whose skinnum is a packed
colour.  It works on any engine and against any client, and it costs an edict
and a network entity per line -- the brain draws hundreds when its area
awareness is being watched.

Q2PRO offers DEBUG_DRAW_API_V1, which draws on the LOCAL client only and costs
nothing on the wire.  It is available only in a non-dedicated build with a
renderer and USE_DEBUG on (q2pro src/server/game.c PF_GetExtension), so on a
dedicated server -- which is what this library is for -- the beam scheme is the
one that runs, and the fallback is the normal case rather
than the exception.

DebugLineCreate/Delete/Show keep their bot_import_t signatures either way, so
the brain cannot tell which is running.  What differs is the lifetime: the
extension's lines last a stated number of milliseconds and the beams last until
deleted, so the extension path re-issues every live line each frame from
BotDebugFrame().

===============================================================================
*/

// The 1999 colours are packed byte quadruples for the Q2 palette-indexed beam
// renderer.  The extension wants RGBA, so the five the contract names get a
// colour each and anything else comes out white: the LINECOLOR_* values map
// to RGBA.
//
// The switch is on an UNSIGNED value and the labels are cast to match, and that
// is not tidying.  `LINECOLOR_RED` is 0xf2f2f0f0L: it does not fit in an `int`,
// so it is an unsigned constant, while the contract's `DebugLineShow` takes
// `int color` -- and a `case` label whose value cannot be represented in the
// condition's type is a constraint violation.  gcc converts it silently; clang
// says `overflow converting case value to switch condition type (4076007664 to
// -218959632)` and, under -Werror, refuses to compile.  Five labels, five
// errors, and only on one of the two compilers this tree builds with.
static uint32_t BotLineColorRGBA(int color)
{
    switch ((uint32_t)color) {
    case (uint32_t)LINECOLOR_RED:       return 0xff0000ffu;
    case (uint32_t)LINECOLOR_GREEN:     return 0xff00ff00u;
    case (uint32_t)LINECOLOR_BLUE:      return 0xffff0000u;
    case (uint32_t)LINECOLOR_YELLOW:    return 0xff00ffffu;
    case (uint32_t)LINECOLOR_ORANGE:    return 0xff0080ffu;
    default:                            return 0xffffffffu;
    }
}

// The extension draws per frame, so the game has to remember what is live.
// One row per line handle, indexed the same way the beam scheme indexes edicts,
// which is what lets the two share DebugLineCreate's return value.
#define MAX_DEBUGLINES  4096

typedef struct {
    bool    used;
    bool    shown;
    vec3_t  start, end;
    int     color;
} debugline_t;

static debugline_t  debuglines[MAX_DEBUGLINES];
static int          numdebuglines;
static bool         use_extension;

void BotDebugInit(void)
{
    // Announced once per process, not once per call: ReadGame calls BotSetup
    // again after its FreeTags(TAG_GAME), and which implementation is in use is
    // a fact about the engine rather than about the call.
    static bool announced;

    use_extension = G_DebugDraw() != NULL;
    if (!announced) {
        gi.dprintf("Colosseum: bot debug lines via %s\n",
                   use_extension ? "the engine's DEBUG_DRAW_API_V1"
                                 : "the 1999 beam entities");
        announced = true;
    }
    memset(debuglines, 0, sizeof(debuglines));
    numdebuglines = 0;
}
//===========================================================================
// shows a line edict at the given position and with the given color
//
// Parameter:               -
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void ShowLine(edict_t *line, vec3_t start, vec3_t end, int color)
{
    //if no color make the line invisible for clients
    if (color == LINECOLOR_NONE)
    {
        line->svflags |= SVF_NOCLIENT;
        return;
    } //end if
    //line is visible to clients
    line->svflags &= ~SVF_NOCLIENT;
    //set the color
    line->s.skinnum = color;
    //start point of the line
    VectorCopy(start, line->s.origin);
    //end point of the line
    VectorCopy(end, line->s.old_origin);
    //relink edict after moving
    gi.linkentity(line);
} //end of the function ShowLine
//===========================================================================
// creates a line edict
//
// Parameter:               -
// Returns:                 the line edict
// Changes Globals:     -
//===========================================================================
static edict_t *CreateLine(void)
{
    edict_t *line;
    line = G_Spawn();
    //classname
    line->classname = "debugline";
    //not moving, position only changed by setting the origin
    line->movetype = MOVETYPE_NONE;
    //don't set the old_origin in G_RunFrame
    line->flags |= FL_OLDORGNOTSET;
    //not solid
    line->solid = SOLID_NOT;
    //translucent beam
    line->s.renderfx |= RF_BEAM | RF_TRANSLUCENT;
    //must be non-zero
    line->s.modelindex = 1;
    //diameter of beam (2 seems to be smallest)
    line->s.frame = 2;
    //set the default color (red)
    line->s.skinnum = LINECOLOR_RED;
    //size of the beam??
    VectorSet(line->mins, -8, -8, -8);
    VectorSet(line->maxs, 8, 8, 8);
    //link the edict
    gi.linkentity(line);
    return line;
} //end of the function CreateLine
//===========================================================================
// creates a debug line
//
// Parameter:               -
// Returns:                 handle to the debug line
// Changes Globals:     -
//===========================================================================
int DebugLineCreate(void)
{
    edict_t *line;
    int i;

    if (use_extension)
    {
        for (i = 1; i < MAX_DEBUGLINES; i++)
        {
            if (debuglines[i].used) continue;
            memset(&debuglines[i], 0, sizeof(debuglines[i]));
            debuglines[i].used = true;
            if (i >= numdebuglines) numdebuglines = i + 1;
            return i;
        } //end for
        gi.dprintf("DebugLineCreate: out of debug lines\n");
        return 0;
    } //end if
    line = CreateLine();
    return DF_ENTNUMBER(line);
} //end of the function DebugLineCreate;
//===========================================================================
// deletes a debug line
//
// Parameter:               line : handle to the debug line
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void DebugLineDelete(int line)
{
    edict_t *l;

    if (use_extension)
    {
        if (line <= 0 || line >= MAX_DEBUGLINES || !debuglines[line].used)
        {
            gi.dprintf("DebugLineDelete: invalid line\n");
            return;
        } //end if
        debuglines[line].used = false;
        debuglines[line].shown = false;
        return;
    } //end if
    if (line < 0 || line >= game.maxentities)
    {
        gi.dprintf("DebugLineDelete: invalid line entity\n");
        return;
    } //end if
    l = DF_NUMBERENT(line);
    if (!l->inuse || !l->classname || Q_strcasecmp(l->classname, "debugline"))
    {
        gi.dprintf("DebugLineDelete: not a line entity\n");
        return;
    } //end if
    G_FreeEdict(l);
} //end of the function DebugLineDelete
//===========================================================================
// shows a debug line with a specific color
//
// Parameter:               line  : handle to the debug line
//                              start : start of the line
//                              end : end of the line
//                              color : color of the line
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void DebugLineShow(int line, vec3_t start, vec3_t end, int color)
{
    edict_t *l;

    if (use_extension)
    {
        if (line <= 0 || line >= MAX_DEBUGLINES || !debuglines[line].used)
        {
            gi.dprintf("DebugLineShow: invalid line\n");
            return;
        } //end if
        VectorCopy(start, debuglines[line].start);
        VectorCopy(end, debuglines[line].end);
        debuglines[line].color = color;
        debuglines[line].shown = (color != LINECOLOR_NONE);
        return;
    } //end if
    if (line < 0 || line >= game.maxentities)
    {
        gi.dprintf("DebugLineShow: invalid line entity\n");
        return;
    } //end if
    l = DF_NUMBERENT(line);
    if (!l->inuse || !l->classname || Q_strcasecmp(l->classname, "debugline"))
    {
        gi.dprintf("DebugLineShow: not a line entity\n");
        return;
    } //end if
    ShowLine(l, start, end, color);
} //end of the function DebugLineShow
//===========================================================================
// Re-issues every live line.  Only the extension path needs it -- a beam edict
// stays where it was put until something moves it.
//===========================================================================
void BotDebugFrame(void)
{
    const debug_draw_api_v1_t *dd;
    int i;

    if (!use_extension) return;
    dd = G_DebugDraw();
    if (!dd) return;
    for (i = 1; i < numdebuglines; i++)
    {
        if (!debuglines[i].used || !debuglines[i].shown) continue;
        //one frame's worth of milliseconds, so a line the brain stops showing
        //disappears instead of lingering
        dd->AddDebugLine(debuglines[i].start, debuglines[i].end,
                         BotLineColorRGBA(debuglines[i].color),
                         (uint32_t)(FRAMETIME * 1000), false);
    } //end for
} //end of the function BotDebugFrame
//===========================================================================
// creates a visible bounding box
//
// Parameter:               box : bounding box to create
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void CreateVisibleBoundingBox(visiblebbox_t *box)
{
    int i;

    if (box->created) return;
    for (i = 0; i < BBOX_LINES; i++)
    {
        box->lines[i] = CreateLine();
    } //end for
    box->created = true;
} //end of the function CreateVisibleBoundingBox
//===========================================================================
// destroys a visible bounding box
//
// Parameter:               box : bounding box to delete
// Returns:                 -
// Changes Globals:     -
//===========================================================================
static void DestroyVisibleBoundingBox(visiblebbox_t *box)
{
    int i;

    if (!box->created) return;
    for (i = 0; i < BBOX_LINES; i++)
    {
        if (box->lines[i]) G_FreeEdict(box->lines[i]);
        box->lines[i] = NULL;
    } //end for
    box->created = false;
} //end of the function DestroyVisibleBoundingBox
//===========================================================================
// toggles a visible bounding box of a specific entity
//
// Parameter:               ent : entity to show bounding box of
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void ToggleVisibleBoundingBox(edict_t *ent)
{
    edict_t *cam;

    // The donor's #ifdef OBSERVER: someone watching through the chase camera
    // wants the box on what they are looking at, not on their own invisible
    // body.  G_IsObserver() is the ruleset-neutral form of the test.
    if (G_IsObserver(ent) && ent->client)
    {
        cam = ent->client->chase_target;
        if (cam && cam != ent)
        {
            if (cam->box.created) DestroyVisibleBoundingBox(&cam->box);
            else CreateVisibleBoundingBox(&cam->box);
            return;
        } //end if
    } //end if
    if (ent->box.created) DestroyVisibleBoundingBox(&ent->box);
    else CreateVisibleBoundingBox(&ent->box);
} //end of the function ToggleVisibleBoundingBox
//===========================================================================
// sets the bouding box of the given ent
//
// Parameter:               box : bounding box used for visualization
//                              ent : entity to visualize the bounding box of
// Returns:                 -
// Changes Globals:     -
//===========================================================================
void SetVisibleBoundingBox(visiblebbox_t *box, edict_t *ent)
{
    vec3_t bboxcorners[8], start, end;
    int i;

    // The donor calls gi.error() here, twice, which takes the server down over
    // a debug overlay.  Both conditions are "the caller asked before creating
    // it", which is a no-op, not a fatal.
    if (!box->created) return;
    //check if the box has line edicts
    for (i = 0; i < BBOX_LINES; i++)
    {
        if (!box->lines[i]) return;
    } //end for
    //upper corners
    bboxcorners[0][0] = ent->s.origin[0] + ent->maxs[0];
    bboxcorners[0][1] = ent->s.origin[1] + ent->maxs[1];
    bboxcorners[0][2] = ent->s.origin[2] + ent->maxs[2];
    //
    bboxcorners[1][0] = ent->s.origin[0] + ent->mins[0];
    bboxcorners[1][1] = ent->s.origin[1] + ent->maxs[1];
    bboxcorners[1][2] = ent->s.origin[2] + ent->maxs[2];
    //
    bboxcorners[2][0] = ent->s.origin[0] + ent->mins[0];
    bboxcorners[2][1] = ent->s.origin[1] + ent->mins[1];
    bboxcorners[2][2] = ent->s.origin[2] + ent->maxs[2];
    //
    bboxcorners[3][0] = ent->s.origin[0] + ent->maxs[0];
    bboxcorners[3][1] = ent->s.origin[1] + ent->mins[1];
    bboxcorners[3][2] = ent->s.origin[2] + ent->maxs[2];
    //lower corners
    memcpy(bboxcorners[4], bboxcorners[0], sizeof(vec3_t) * 4);
    for (i = 0; i < 4; i++) bboxcorners[4 + i][2] = ent->s.origin[2] + ent->mins[2];
    //draw bounding box
    for (i = 0; i < 4; i++)
    {
        //top plane
        ShowLine(box->lines[i], bboxcorners[i], bboxcorners[(i+1)&3], LINECOLOR_RED);
        //bottom plane
        ShowLine(box->lines[4+i], bboxcorners[4+i], bboxcorners[4+((i+1)&3)], LINECOLOR_RED);
        //vertical lines
        ShowLine(box->lines[8+i], bboxcorners[i], bboxcorners[4+i], LINECOLOR_RED);
    } //end for
    //mark origin with cross
    for (i = 0; i < 2; i++)
    {
        VectorCopy(ent->s.origin, start);
        start[i] += ent->maxs[i];
        VectorCopy(ent->s.origin, end);
        end[i] += ent->mins[i];
        ShowLine(box->lines[12+i], start, end, LINECOLOR_BLUE);
    } //end for
} //end of the function SetVisibleBoundingBox
