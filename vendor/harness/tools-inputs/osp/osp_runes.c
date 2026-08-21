// osp_runes.c -- <INVENTED FILENAME>. The five Lithium-style runes.
//
// Resist / Strength / Haste / Regeneration / Vampire, held one at a time and
// carried in `ps.stats[STAT_RUNE_*]` (slots 22..26) so the client HUD can draw
// them.  `r_count[]` tracks how many of each are loose in the world and
// `rune_spawnpoint[]` is the pool of places one may reappear.

#include "g_local.h"

int	runespawn = 0;
int	rune_spawncount = 0;
edict_t * rune_spawnpoint[50];
int	r_count[5];


// The five rune classnames, NULL-terminated.
static char	*runenames[] =
{
	"item_rune1",
	"item_rune2",
	"item_rune3",
	"item_rune4",
	"item_rune5",
	NULL
};

// Defined at the very end of the file, so it needs a forward declaration
// here.  Name <INVENTED>.
static void OSP_runeSpawnThink (edict_t *self);

/*
==============
OSP_What_Rune

Which rune this client is carrying, or NULL.
==============
*/
// gamex86.dll: 10035FF0..1003605E
// gamei386.so: 000621F4..00062277
gitem_t *OSP_What_Rune (edict_t *ent)
{
	int		i;
	gitem_t	*item;

	i = 0;
	while (runenames[i])
	{
		item = FindItemByClassname (runenames[i]);
		if (item && ent->client->pers.inventory[ITEM_INDEX(item)])
			return item;
		i++;
	}
	return NULL;
}

/*
==============
OSP_Pickup_Rune

One rune at a time: refuse the pickup outright if the player already holds any
of the five.
==============
*/
// gamex86.dll: 1003605E..100361CD
// gamei386.so: 00062278..00062416
qboolean OSP_Pickup_Rune (edict_t *ent, edict_t *other)
{
	int		i;
	gitem_t	*item;

	i = 0;
	while (runenames[i])
	{
		item = FindItemByClassname (runenames[i]);
		if (item && other->client->pers.inventory[ITEM_INDEX(item)])
			return false;
		i++;
	}

	other->client->resp.osp_r23c = 0;
	other->client->pers.inventory[ITEM_INDEX(ent->item)]++;
	other->client->osp_t06c = level.time;
	other->client->ps.stats[ent->item->quantity] = 1;
	other->client->resp.osp_r200 = ent - g_edicts;

	item = FindItemByClassname (ent->classname);
	q2log_pickupItem (item->pickup_name, ent - g_edicts, other);
	q2log_useItem (item->pickup_name, other);

	if (OSP_checkMaxRunes ())
		OSP_checkMinRunes ();

	return true;
}

/*
==============
OSP_randomRuneSpot     <INVENTED NAME>

Returns a random entry of the rune spawn pool, or NULL when it is empty.
`static` in the original, and inlined at all three of its call sites, which is
why it has to be defined here ahead of them.
==============
*/
// gamex86.dll: 10036241..10036278
// gamei386.so: absent
static edict_t *OSP_randomRuneSpot (void)
{
	edict_t	*spot = NULL;
	int		n;

	if (!rune_spawncount)
		return NULL;

	n = rand () % rune_spawncount;
	return rune_spawnpoint[n];
}

/*
==============
OSP_runeThink

A rune lying in the world. If there is somewhere for it to go it is taken out
of play and the minimum-rune check reseeds the level; otherwise it waits a
minute and asks again.
==============
*/
// gamex86.dll: 100361CD..10036241
// gamei386.so: 00062418..000624A3
void OSP_runeThink (edict_t *self)
{
	edict_t	*spot;

	spot = OSP_randomRuneSpot ();

	if (spot)
	{
		r_count[self->item->quantity - STAT_RUNE_RESIST]--;
		G_FreeEdict (self);
		OSP_checkMinRunes ();
	}
	else
	{
		self->nextthink = level.time + 60;
		self->think = OSP_runeThink;
	}
}

/*
==============
OSP_Drop_Rune

The five runes are told apart in the world by their colour shell, not by
model -- runes_model gives them all the same one.
==============
*/
// gamex86.dll: 10036278..1003638A
// gamei386.so: 000624A4..00062598
void OSP_Drop_Rune (edict_t *ent, gitem_t *item)
{
	edict_t	*dropped;

	dropped = Drop_Item (ent, item);

	if (dropped->item->quantity == STAT_RUNE_RESIST)
		dropped->s.renderfx |= RF_SHELL_BLUE;
	else if (dropped->item->quantity == STAT_RUNE_STRENGTH)
		dropped->s.renderfx |= RF_SHELL_RED;
	else if (dropped->item->quantity == STAT_RUNE_HASTE)
		dropped->s.renderfx |= RF_SHELL_RED|RF_SHELL_GREEN;
	else if (dropped->item->quantity == STAT_RUNE_REGEN)
		dropped->s.renderfx |= RF_SHELL_GREEN;
	else if (dropped->item->quantity == STAT_RUNE_VAMPIRE)
		dropped->s.renderfx |= RF_SHELL_BLUE|RF_SHELL_RED;

	OSP_checkMinRunes ();
	ent->client->pers.inventory[ITEM_INDEX(item)] = 0;
	OSP_zeroRuneStats (ent);
	q2log_dropItem (item->pickup_name, dropped - g_edicts, ent);
}

/*
==============
OSP_deadDropRune

Drop whichever rune the player is holding where they died, scattered and with
a minute on the clock.  Same colour-shell mapping as OSP_Drop_Rune.
==============
*/
// gamex86.dll: 1003638A..1003657B
// gamei386.so: 00062598..00062779
void OSP_deadDropRune (edict_t *ent)
{
	int		i;
	gitem_t	*item;
	edict_t	*dropped;

	i = 0;
	while (runenames[i])
	{
		item = FindItemByClassname (runenames[i]);
		if (item && ent->client->pers.inventory[ITEM_INDEX(item)])
		{
			dropped = Drop_Item (ent, item);

			if (dropped->item->quantity == STAT_RUNE_RESIST)
				dropped->s.renderfx |= RF_SHELL_BLUE;
			else if (dropped->item->quantity == STAT_RUNE_STRENGTH)
				dropped->s.renderfx |= RF_SHELL_RED;
			else if (dropped->item->quantity == STAT_RUNE_HASTE)
				dropped->s.renderfx |= RF_SHELL_RED|RF_SHELL_GREEN;
			else if (dropped->item->quantity == STAT_RUNE_REGEN)
				dropped->s.renderfx |= RF_SHELL_GREEN;
			else if (dropped->item->quantity == STAT_RUNE_VAMPIRE)
				dropped->s.renderfx |= RF_SHELL_BLUE|RF_SHELL_RED;

			dropped->velocity[0] = rand () % 600 - 300;
			dropped->velocity[1] = rand () % 600 - 300;
			dropped->nextthink = level.time + 60;
			dropped->think = OSP_runeThink;
			dropped->owner = NULL;
			ent->client->pers.inventory[ITEM_INDEX(item)] = 0;
			OSP_zeroRuneStats (ent);
			q2log_dropItem (item->pickup_name, dropped - g_edicts, ent);
			OSP_checkMinRunes ();
		}
		i++;
	}
}

/*
==============
OSP_spawnRuneAt

Put one rune into the world at `spot`, tossed a little way out along a random
yaw.  `static` in the original.  The tail call back into OSP_checkMinRunes
makes the pair mutually recursive: spawning one rune re-tests whether the
minimum is met.
==============
*/
// gamex86.dll: 100365B3..1003682D
// gamei386.so: 00062779..00062958
static void OSP_spawnRuneAt (gitem_t *item, edict_t *spot)
{
	edict_t	*ent;
	vec3_t	forward, right;
	vec3_t	angles;

	ent = G_Spawn ();
	ent->classname = item->classname;
	ent->item = item;
	ent->spawnflags = DROPPED_ITEM;
	ent->s.effects = item->world_model_flags;
	ent->s.renderfx = RF_GLOW;

	if (ent->item->quantity == STAT_RUNE_RESIST)
		ent->s.renderfx |= RF_SHELL_BLUE;
	else if (ent->item->quantity == STAT_RUNE_STRENGTH)
		ent->s.renderfx |= RF_SHELL_RED;
	else if (ent->item->quantity == STAT_RUNE_HASTE)
		ent->s.renderfx |= RF_SHELL_RED|RF_SHELL_GREEN;
	else if (ent->item->quantity == STAT_RUNE_REGEN)
		ent->s.renderfx |= RF_SHELL_GREEN;
	else if (ent->item->quantity == STAT_RUNE_VAMPIRE)
		ent->s.renderfx |= RF_SHELL_BLUE|RF_SHELL_RED;

	r_count[ent->item->quantity - STAT_RUNE_RESIST]++;

	VectorSet (ent->mins, -15, -15, -15);
	VectorSet (ent->maxs, 15, 15, 15);
	gi.setmodel (ent, runes_model->string);
	ent->solid = SOLID_TRIGGER;
	ent->movetype = MOVETYPE_TOSS;
	ent->touch = Touch_Item;
	ent->owner = ent;

	VectorSet (angles, 0, rand () % 360, 0);
	AngleVectors (angles, forward, right, NULL);
	VectorCopy (spot->s.origin, ent->s.origin);
	ent->s.origin[2] += 16;
	VectorScale (forward, 100, ent->velocity);
	ent->velocity[2] = 300;
	ent->nextthink = level.time + 60;
	ent->think = OSP_runeThink;

	gi.linkentity (ent);
	OSP_checkMinRunes ();
}

// gamex86.dll: 1003657B..100365B3
// gamei386.so: 00062958..000629B5
void OSP_respawnRune (edict_t *ent)
{
	edict_t	*spot;

	spot = OSP_randomRuneSpot ();
	if (spot)
		OSP_spawnRuneAt (ent->item, spot);
	G_FreeEdict (ent);
}

/*
==============
OSP_setupRuneSpawn

Build the pool of places a rune may appear from the map's own item and spawn
entities, then hang a one-shot thinker off `delay` seconds from now to seed the
level.  Runs once per map -- `runespawn` is the latch.
==============
*/
// gamex86.dll: 1003682D..100369D3
// gamei386.so: 000629B8..00062B73
void OSP_setupRuneSpawn (int delay)
{
	int		i;
	edict_t	*ent;

	if (runespawn)
		return;

	for (i = 0; i < 5; i++)
		r_count[i] = 0;

	ent = &g_edicts[(int)maxclients->value + 1];
	rune_spawncount = 0;

	for (i = maxclients->value + 1; i < globals.num_edicts; i++, ent++)
	{
		if (!ent->inuse)
			continue;

		if (!strstr (ent->classname, "ammo_") &&
			!strstr (ent->classname, "weapon_") &&
			!strstr (ent->classname, "item_") &&
			!strstr (ent->classname, "misc_teleporter") &&
			!strstr (ent->classname, "info_player"))
			continue;

		rune_spawnpoint[rune_spawncount++] = ent;

		if (rune_spawncount >= 50)
			break;
	}

	ent = G_Spawn ();
	ent->nextthink = level.time + 2 + delay;
	ent->think = OSP_runeSpawnThink;
	runespawn = 1;
}

/*
==============
OSP_runesApplyResistance

Soak incoming damage by the resist factor, with a sound and an optional HUD
flash.  The 0.2 that replaces the volume when the player is silenced is a float
constant; the 0.2 added to level.time is a double.
==============
*/
// gamex86.dll: 100369E9..10036A9E
// gamei386.so: 00062B74..00062C71
int OSP_runesApplyResistance (edict_t *ent, int damage)
{
	float	volume = 1.0;

	if (ent->client && ent->client->silencer_shots)
		volume = 0.2f;

	if (damage && ent->client && ent->client->ps.stats[STAT_RUNE_RESIST])
	{
		gi.sound (ent, CHAN_VOICE, gi.soundindex("world/force2.wav"), volume, ATTN_NORM, 0);
		if ((int)runes_flash->value)
			ent->client->osp_t074 = level.time + 0.2;
		return damage / runes_resist->value;
	}
	return damage;
}

// gamex86.dll: 10036A9E..10036AD9
// gamei386.so: 00062C74..00062CD3
int OSP_runesApplyStrength (edict_t *ent, int damage)
{
	if (damage && ent->client && ent->client->ps.stats[STAT_RUNE_STRENGTH])
		return (int)(runes_strength->value * damage);
	return damage;
}

// gamex86.dll: 10036AD9..10036B77
// gamei386.so: 00062CD4..00062D9C
qboolean OSP_runesApplyStrengthSound (edict_t *ent)
{
	float	volume = 1.0;

	if (ent->client && ent->client->ps.stats[STAT_RUNE_STRENGTH])
	{
		if (ent->client->silencer_shots)
			volume = 0.2f;

		if ((int)runes_flash->value)
			ent->client->osp_t078 = level.time + 0.2;

		gi.sound (ent, CHAN_ITEM, gi.soundindex("items/damage3.wav"), volume, ATTN_NORM, 0);
		return true;
	}
	return false;
}

// gamex86.dll: 10036B77..10036B9F
// gamei386.so: 00062D9C..00062DC0
qboolean OSP_runesHasHaste (edict_t *ent)
{
	if (ent->client && ent->client->ps.stats[STAT_RUNE_HASTE])
		return true;
	return false;
}

// gamex86.dll: 10036B9F..10036C73
// gamei386.so: 00062DC0..00062EAC
void OSP_runesApplyHasteSound (edict_t *ent)
{
	float	volume = 1.0;

	if (ent->client && ent->client->silencer_shots)
		volume = 0.2f;

	if (ent->client)
	{
		if (ent->client->ps.stats[STAT_RUNE_HASTE])
		{
			if ((int)runes_flash->value)
				ent->client->osp_t07c = level.time + 0.2;

			// rate-limit the whine to ten a second
			if (ent->client->osp_t070 < level.time)
			{
				ent->client->osp_t070 = level.time + 0.1;
				gi.sound (ent, CHAN_VOICE, gi.soundindex("world/x_light.wav"), volume, ATTN_NORM, 0);
			}
		}
	}
}

// gamex86.dll: 10036C73..10036ECF
// gamei386.so: 00062EAC..0006315E
void OSP_runesApplyRegeneration (edict_t *ent)
{
	int		healamt = 0;
	float	volume = 1.0;
	gclient_t	*clientp;
	int		armor;

	clientp = ent->client;
	if (!clientp)
		return;

	if (clientp->silencer_shots)
		volume = 0.2f;

	if (clientp->ps.stats[STAT_RUNE_REGEN])
	{
		if (clientp->osp_t06c < level.time)
		{
			clientp->osp_t06c = level.time;

			if (ent->health < (int)runes_regen_hmax->value)
			{
				ent->health += 3;
				if (ent->health > (int)runes_regen_hmax->value)
					ent->health = (int)runes_regen_hmax->value;

				clientp->osp_t06c += 0.8;
				healamt = 1;

				if ((int)runes_flash->value)
					ent->client->osp_t080 = level.time + 0.2;
			}

			armor = ArmorIndex (ent);
			if (armor)
			{
				if (clientp->pers.inventory[armor] <
					(int)runes_regen_amax->value)
				{
					clientp->pers.inventory[armor] += 3;
					if (clientp->pers.inventory[armor] >
						(int)runes_regen_amax->value)
						clientp->pers.inventory[armor] =
							(int)runes_regen_amax->value;

					clientp->osp_t06c += 0.8;
					healamt = 1;

					if ((int)runes_flash->value)
						ent->client->osp_t080 = level.time + 0.2;
				}
			}
		}

		if (healamt)
		{
			if (ent->client->osp_t070 < level.time)
			{
				ent->client->osp_t070 = level.time + 0.1;
				gi.sound (ent, CHAN_VOICE, gi.soundindex ("items/l_health.wav"),
					volume, ATTN_NORM, 0);
			}
		}
	}
}

// gamex86.dll: 10036ECF..10036EEE
// gamei386.so: 00063160..0006317F
qboolean OSP_runesHasRegeneration (edict_t *ent)
{
	if (ent->client->ps.stats[STAT_RUNE_REGEN])
		return true;
	return false;
}

// gamex86.dll: 10036EEE..10036F0D
// gamei386.so: 00063180..0006319F
qboolean OSP_runesHasVampire (edict_t *ent)
{
	if (ent->client->ps.stats[STAT_RUNE_VAMPIRE])
		return true;
	return false;
}

// gamex86.dll: 10036F0D..10037020
// gamei386.so: 000631A0..000632F6
void OSP_runesApplyVampire (edict_t *ent, int damage)
{
	float	volume = 1.0;

	if (ent->client && ent->client->silencer_shots)
		volume = 0.2f;

	if (ent->client && ent->client->ps.stats[STAT_RUNE_VAMPIRE] &&
		ent->health < (int)runes_vampire_max->value)
	{
		ent->health += (int)(runes_vampire->value * damage);

		if (ent->health > (int)runes_vampire_max->value)
			ent->health = (int)runes_vampire_max->value;

		if ((int)runes_flash->value)
			ent->client->osp_t084 = level.time + 0.2;

		gi.sound (ent, CHAN_VOICE, gi.soundindex ("makron/pain2.wav"), volume,
			ATTN_NORM, 0);
	}
}

// gamex86.dll: 10037020..10037070
// gamei386.so: 000632F8..0006333C
void OSP_zeroRuneStats (edict_t *ent)
{
	ent->client->ps.stats[STAT_RUNE_RESIST] = 0;
	ent->client->ps.stats[STAT_RUNE_STRENGTH] = 0;
	ent->client->ps.stats[STAT_RUNE_HASTE] = 0;
	ent->client->ps.stats[STAT_RUNE_REGEN] = 0;
	ent->client->ps.stats[STAT_RUNE_VAMPIRE] = 0;
}

/*
==============
OSP_removeRunes

Sweep every rune out of the world and out of every inventory.
==============
*/
// gamex86.dll: 10037070..100371BB
// gamei386.so: 0006333C..000634D3
void OSP_removeRunes (void)
{
	int		i;
	int		j;
	gitem_t	*item = NULL;
	edict_t	*ent;

	ent = g_edicts;
	for (i = 0; i < globals.num_edicts; i++, ent++)
	{
		if (ent->inuse)
		{
			if (!strstr (ent->classname, "item_rune"))
				continue;
			G_FreeEdict (ent);
		}
	}

	for (i = 1; i <= game.maxclients; i++)
	{
		ent = g_edicts + i;
		if (ent->inuse)
		{
			if (!ent->client)
				continue;

			for (j = 0; j < 5; j++)
			{
				item = FindItemByClassname (runenames[j]);
				ent->client->pers.inventory[ITEM_INDEX(item)] = 0;
			}

			OSP_zeroRuneStats (ent);
		}
	}

	for (i = 0; i < 5; i++)
		r_count[i] = 0;
}

/*
==============
OSP_findMinRune

Which enabled rune type has the fewest instances loose in the world.  Ties are
broken at random, which is why the second pass collects every index that hits
the minimum instead of stopping at the first.
==============
*/
// gamex86.dll: 100371BB..10037295
// gamei386.so: 000634D4..00063561
int OSP_findMinRune (void)
{
	int		rmins[5];
	int		count;
	int		i;
	int		bit;
	int		minrune;

	bit = 1;
	minrune = 9999999;
	count = 0;
	for (i = 0; i < 5; i++)
	{
		if ((rune_stat & bit) && minrune > r_count[i])
			minrune = r_count[i];
		bit <<= 1;
	}

	bit = 1;
	for (i = 0; i < 5; i++)
	{
		if ((rune_stat & bit) && r_count[i] == minrune)
			rmins[count++] = i;
		bit <<= 1;
	}

	if (!count)
		return -1;

	return rmins[rand () % count];
}

/*
==============
OSP_checkMinRunes

Reseed the level if there are fewer runes about than the per-player ratio or
the floor asks for.
==============
*/
// gamex86.dll: 10037295..1003733B
// gamei386.so: 00063564..000636A0
void OSP_checkMinRunes (void)
{
	int		totalnum;
	int		i;
	gitem_t	*items;
	edict_t	*spot;

	totalnum = 0;
	for (i = 0; i < 5; i++)
		totalnum += r_count[i];

	if (totalnum < active_clients * runes_perplayer->value ||
		totalnum < (int)runes_min->value)
	{
		i = OSP_findMinRune ();
		if (i < 0)
			return;
		items = FindItemByClassname (runenames[i]);

		/* Preserve the target's post-call basic block. */
rune_item_found:
		spot = OSP_randomRuneSpot ();
		OSP_spawnRuneAt (items, spot);
	}
}

// gamex86.dll: 1003733B..100373C0
// gamei386.so: 000636A0..00063758
qboolean OSP_checkMaxRunes (void)
{
	int		total;
	int		i;

	total = 0;
	for (i = 0; i < 5; i++)
		total += r_count[i];

	if (total > active_clients * (int)runes_perplayer->value ||
		total > (int)runes_max->value)
	{
		if (total > (int)runes_min->value)
			return false;
	}
	return true;
}

// The one-shot timer OSP_setupRuneSpawn hangs on a spare edict: seed the
// level, then free itself.  `static` in the original.
// gamex86.dll: 100369D3..100369E9
// gamei386.so: 00063758..00063780
static void OSP_runeSpawnThink (edict_t *self)
{
	OSP_checkMinRunes ();
	G_FreeEdict (self);
}
