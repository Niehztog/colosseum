#include "g_local.h"


/*
==============================================================================

PLAYER TRAIL

==============================================================================

This is a circular list containing the a list of points of where
the player has been recently.  It is used by monsters for pursuit.

.origin		the spot
.owner		forward link
.aiment		backward link
*/


#define	TRAIL_LENGTH	8

edict_t		*trail[TRAIL_LENGTH];
int			trail_head;
qboolean	trail_active = false;

#define NEXT(n)		(((n) + 1) & (TRAIL_LENGTH - 1))
#define PREV(n)		(((n) - 1) & (TRAIL_LENGTH - 1))


// gamex86.dll: 1005A650..1005A6C0
// gamei386.so: 0004023C..000402A9
void PlayerTrail_Init (void)
{
	int		n;

	if (deathmatch->value /* FIXME || coop */)
		return;

	for (n = 0; n < TRAIL_LENGTH; n++)
	{
		trail[n] = G_Spawn();
		trail[n]->classname = "player_trail";
	}

	trail_head = 0;
	trail_active = true;
}


// gamex86.dll: 1005A6C0..1005A7B5
// gamei386.so: 000402AC..00040389
void PlayerTrail_Add (vec3_t spot)
{
	vec3_t	temp;

	if (!trail_active)
		return;

	VectorCopy (spot, trail[trail_head]->s.origin);

	trail[trail_head]->timestamp = level.time;

	VectorSubtract (spot, trail[PREV(trail_head)]->s.origin, temp);
	trail[trail_head]->s.angles[1] = vectoyaw (temp);

	trail_head = NEXT(trail_head);
}


// gamex86.dll: 1005A7B5..1005A7D6
// gamei386.so: 0004038C..0004040D
void PlayerTrail_New (vec3_t spot)
{
	if (!trail_active)
		return;

	PlayerTrail_Init ();
	PlayerTrail_Add (spot);
}


// gamex86.dll: 1005A7D6..1005A8A6
// gamei386.so: 00040410..000404BD
edict_t *PlayerTrail_PickFirst (edict_t *self)
{
	int		marker;
	int		n;

	if (!trail_active)
		return NULL;

	for (marker = trail_head, n = TRAIL_LENGTH; n; n--)
	{
		if(trail[marker]->timestamp <= self->monsterinfo.trail_time)
			marker = NEXT(marker);
		else
			break;
	}

	if (visible(self, trail[marker]))
	{
		return trail[marker];
	}

	if (visible(self, trail[PREV(marker)]))
	{
		return trail[PREV(marker)];
	}

	return trail[marker];
}

// gamex86.dll: 1005A8A6..1005A919
// gamei386.so: 000404C0..00040525
edict_t *PlayerTrail_PickNext (edict_t *self)
{
	int		marker;
	int		n;

	if (!trail_active)
		return NULL;

	for (marker = trail_head, n = TRAIL_LENGTH; n; n--)
	{
		if(trail[marker]->timestamp <= self->monsterinfo.trail_time)
			marker = NEXT(marker);
		else
			break;
	}

	return trail[marker];
}

// gamex86.dll: 1005A919..1005A930
// gamei386.so: 00040528..00040552
edict_t *PlayerTrail_LastSpot (void)
{
	return trail[PREV(trail_head)];
}
