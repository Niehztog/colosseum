
#include "g_local.h"

//
// monster weapons
//

//
// Monster utility functions
//

// gamex86.dll: 1001BDF0..1001BF10
// gamei386.so: 00028934..00028A21
void M_CheckGround(edict_t *ent)
{
    vec3_t      point;
    trace_t     trace;

    if (ent->flags & (FL_SWIM | FL_FLY))
        return;

    if (ent->velocity[2] > 100) {
        ent->groundentity = NULL;
        return;
    }

// if the hull point one-quarter unit down is solid the entity is on ground
    point[0] = ent->s.origin[0];
    point[1] = ent->s.origin[1];
    point[2] = ent->s.origin[2] - 0.25f;

    trace = gi.trace(ent->s.origin, ent->mins, ent->maxs, point, ent, MASK_MONSTERSOLID);

    // check steepness
    if (trace.plane.normal[2] < 0.7f && !trace.startsolid) {
        ent->groundentity = NULL;
        return;
    }

//  ent->groundentity = trace.ent;
//  ent->groundentity_linkcount = trace.ent->linkcount;
//  if (!trace.startsolid && !trace.allsolid)
//      VectorCopy (trace.endpos, ent->s.origin);
    if (!trace.startsolid && !trace.allsolid) {
        VectorCopy(trace.endpos, ent->s.origin);
        ent->groundentity = trace.ent;
        ent->groundentity_linkcount = trace.ent->linkcount;
        ent->velocity[2] = 0;
    }
}

// gamex86.dll: 1001BF10..1001BFFE
// gamei386.so: 00028A24..00028AED
void M_CatagorizePosition(edict_t *ent)
{
    vec3_t      point;
    int         cont;

//
// get waterlevel
//
    point[0] = ent->s.origin[0];
    point[1] = ent->s.origin[1];
    point[2] = ent->s.origin[2] + ent->mins[2] + 1;
    cont = gi.pointcontents(point);

    if (!(cont & MASK_WATER)) {
        ent->waterlevel = 0;
        ent->watertype = 0;
        return;
    }

    ent->watertype = cont;
    ent->waterlevel = 1;
    point[2] += 26;
    cont = gi.pointcontents(point);
    if (!(cont & MASK_WATER))
        return;

    ent->waterlevel = 2;
    point[2] += 22;
    cont = gi.pointcontents(point);
    if (cont & MASK_WATER)
        ent->waterlevel = 3;
}

// gamex86.dll: 1001BFFE..1001C0E0
// gamei386.so: 00028AF0..00028D72
void M_droptofloor(edict_t *ent)
{
    vec3_t      end;
    trace_t     trace;

    ent->s.origin[2] += 1;
    VectorCopy(ent->s.origin, end);
    end[2] -= 256;

    trace = gi.trace(ent->s.origin, ent->mins, ent->maxs, end, ent, MASK_MONSTERSOLID);

    if (trace.fraction == 1 || trace.allsolid)
        return;

    VectorCopy(trace.endpos, ent->s.origin);

    gi.linkentity(ent);
    M_CheckGround(ent);
    M_CatagorizePosition(ent);
}


void monster_start_go(edict_t *self);



//============================================================================


