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
// The Gladiator Bot SDK's debug-line interface, from osp-tourney@1d8427e --
// which carries a working Q2PRO port of the 1999 glue (SPECS.md sec 3).
// bl_debug.c is code-identical to the 1999 original (R-BOT-30) and transfers
// unchanged apart from R-BOT-27's engine-extension path.
// The reconstruction's asm-matching address comments are stripped -- SPECS.md
// N1 makes those oracles meaningless here, and they survive at the pin.
//===========================================================================
//
// Name:         bl_debug.h
// Function:     debug functions
// Programmer:   Mr Elusive (MrElusive@demigod.demon.nl)
// Last update:  1999-02-10
// Tab Size:     3
//===========================================================================

#ifndef BL_DEBUG_H
#define BL_DEBUG_H

// The LINECOLOR_* values live in botlib.h, which is the contract; they are not
// redefined here.  The donor's copy of that block is commented out for the same
// reason and is dropped rather than carried.

#define BBOX_LINES              14

typedef struct visiblebbox_s {
    // true if line edicts are created
    bool     created;
    // bounding box composed of BBOX_LINES lines
    edict_t *lines[BBOX_LINES];
} visiblebbox_t;

// functions to deal with debug lines
int  DebugLineCreate(void);
void DebugLineDelete(int line);
void DebugLineShow(int line, vec3_t start, vec3_t end, int color);
// visualizes the bounding box of the given entity
void SetVisibleBoundingBox(visiblebbox_t *box, edict_t *ent);
// toggles a visible bounding box
void ToggleVisibleBoundingBox(edict_t *ent);
// R-BOT-27: bound to Q2PRO's DEBUG_DRAW_API_V1 at InitGame when the engine
// offers it, and to the 1999 beam-entity scheme when it does not.  Reported
// once, because which one is running changes what a debug line costs.
void BotDebugInit(void);
// The extension's lines are per-frame rather than persistent, so they are
// re-issued every frame from the same handles the beam scheme uses.
void BotDebugFrame(void);

#endif // BL_DEBUG_H
