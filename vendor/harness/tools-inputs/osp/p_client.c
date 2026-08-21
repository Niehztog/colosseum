#include "g_local.h"
#include "m_player.h"
#include "bl_main.h"

void ClientUserinfoChanged (edict_t *ent, char *userinfo);
void ClientDisconnect (edict_t *ent);

void SP_misc_teleporter_dest (edict_t *ent);

//
// Gross, ugly, disgustuing hack section
//

// this function is an ugly as hell hack to fix some map flaws
//
// the coop spawn spots on some maps are SNAFU.  There are coop spots
// with the wrong targetname as well as spots with no name at all
//
// we use carnal knowledge of the maps to fix the coop spot targetnames to match
// that of the nearest named single player spot

// gamex86.dll: 10052A89..10052B64
// gamei386.so: 0003EE0E..0003EEB4
static void SP_FixCoopSpots (edict_t *self)
{
	edict_t	*spot;
	vec3_t	d;

	spot = NULL;

	while(1)
	{
		spot = G_Find(spot, FOFS(classname), "info_player_start");
		if (!spot)
			return;
		if (!spot->targetname)
			continue;
		VectorSubtract(self->s.origin, spot->s.origin, d);
		if (VectorLength(d) < 384)
		{
			if ((!self->targetname) || Q_stricmp(self->targetname, spot->targetname) != 0)
			{
//				gi.dprintf("FixCoopSpots changed %s at %s targetname from %s to %s\n", self->classname, vtos(self->s.origin), self->targetname, spot->targetname);
				self->targetname = spot->targetname;
			}
			return;
		}
	}
}

// now if that one wasn't ugly enough for you then try this one on for size
// some maps don't have any coop spots at all, so we need to create them
// where they should have been

// gamex86.dll: 100527B4..100528B4
// gamei386.so: 0003EEB4..0003EF84
static void SP_CreateCoopSpots (edict_t *self)
{
	edict_t	*spot;

	if(strcmp(level.mapname, "security") == 0)
	{
		spot = G_Spawn();
		spot->classname = "info_player_coop";
		spot->s.origin[0] = 188 - 64;
		spot->s.origin[1] = -164;
		spot->s.origin[2] = 80;
		spot->targetname = "jail3";
		spot->s.angles[1] = 90;

		spot = G_Spawn();
		spot->classname = "info_player_coop";
		spot->s.origin[0] = 188 + 64;
		spot->s.origin[1] = -164;
		spot->s.origin[2] = 80;
		spot->targetname = "jail3";
		spot->s.angles[1] = 90;

		spot = G_Spawn();
		spot->classname = "info_player_coop";
		spot->s.origin[0] = 188 + 128;
		spot->s.origin[1] = -164;
		spot->s.origin[2] = 80;
		spot->targetname = "jail3";
		spot->s.angles[1] = 90;

		return;
	}
}


/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 32)
The normal starting point for a level.
*/
// gamex86.dll: 10052760..100527B4
// gamei386.so: 00038480..000384FA
void SP_info_player_start(edict_t *self)
{
	if (!coop->value)
		return;
	if(strcmp(level.mapname, "security") == 0)
	{
		// invoke one of our gross, ugly, disgusting hacks
		self->think = SP_CreateCoopSpots;
		self->nextthink = level.time + FRAMETIME;
	}
}

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for deathmatch games
*/
// gamex86.dll: 100528B4..100528E8
// gamei386.so: 000384FC..0003853B
void SP_info_player_deathmatch(edict_t *self)
{
	if (!deathmatch->value)
	{
		G_FreeEdict (self);
		return;
	}
	SP_misc_teleporter_dest (self);
}

/*QUAKED info_player_coop (1 0 1) (-16 -16 -24) (16 16 32)
potential spawning position for coop games
*/

// gamex86.dll: 100528E8..10052A89
// gamei386.so: 0003853C..000386F1
void SP_info_player_coop(edict_t *self)
{
	if (!coop->value)
	{
		G_FreeEdict (self);
		return;
	}

	if((strcmp(level.mapname, "jail2") == 0)   ||
	   (strcmp(level.mapname, "jail4") == 0)   ||
	   (strcmp(level.mapname, "mine1") == 0)   ||
	   (strcmp(level.mapname, "mine2") == 0)   ||
	   (strcmp(level.mapname, "mine3") == 0)   ||
	   (strcmp(level.mapname, "mine4") == 0)   ||
	   (strcmp(level.mapname, "lab") == 0)     ||
	   (strcmp(level.mapname, "boss1") == 0)   ||
	   (strcmp(level.mapname, "fact3") == 0)   ||
	   (strcmp(level.mapname, "biggun") == 0)  ||
	   (strcmp(level.mapname, "space") == 0)   ||
	   (strcmp(level.mapname, "command") == 0) ||
	   (strcmp(level.mapname, "power2") == 0) ||
	   (strcmp(level.mapname, "strike") == 0))
	{
		// invoke one of our gross, ugly, disgusting hacks
		self->think = SP_FixCoopSpots;
		self->nextthink = level.time + FRAMETIME;
	}
}


/*QUAKED info_player_intermission (1 0 1) (-16 -16 -24) (16 16 32)
The deathmatch intermission point will be at one of these
Use 'angles' instead of 'angle', so you can set pitch or roll as well as yaw.  'pitch yaw roll'
*/
// gamex86.dll: 10052B64..10052B69
// gamei386.so: 000386F4..000386F9
void SP_info_player_intermission(void)
{
}


//=======================================================================


// gamex86.dll: 10052B69..10052B6E
// gamei386.so: 000386FC..00038701
void player_pain (edict_t *self, edict_t *other, float kick, int damage)
{
	// player pain is handled at the end of the frame in P_DamageFeedback
}


// gamex86.dll: 10052B6E..10052BBF
// gamei386.so: 00038704..0003874B
qboolean IsFemale (edict_t *ent)
{
	char		*info;

	if (!ent->client)
		return false;

	info = Info_ValueForKey (ent->client->pers.userinfo, "skin");
	if (info[0] == 'f' || info[0] == 'F')
		return true;
	return false;
}

// gamex86.dll: 10052BBF..100537AB
// gamei386.so: 0003874C..000392A9
void ClientObituary (edict_t *self, edict_t *inflictor, edict_t *attacker)
{
	int			mod;
	// `ff` before the two message pointers is a deliberate declaration order.
	qboolean	ff;
	char		*message;
	char		*message2;
	int			j;		// Invented name: notification-recipient index.
	edict_t		*e;		// Invented name: notification recipient.

	if (frag_offset && teams[0].osp_m0f8 != teams[1].osp_m0f8)
		return;

	if (deathmatch->value)
	{
		ff = meansOfDeath & MOD_FRIENDLY_FIRE;
		mod = meansOfDeath & ~MOD_FRIENDLY_FIRE;
		message = NULL;
		message2 = "";

		switch (mod)
		{
		case MOD_SUICIDE:
			message = "suicides";
			break;
		case MOD_FALLING:
			message = "cratered";
			break;
		case MOD_CRUSH:
			message = "was squished";
			break;
		case MOD_WATER:
			message = "sank like a rock";
			break;
		case MOD_SLIME:
			message = "melted";
			break;
		case MOD_LAVA:
			message = "does a back flip into the lava";
			break;
		case MOD_EXPLOSIVE:
		case MOD_BARREL:
			message = "blew up";
			break;
		case MOD_EXIT:
			message = "found a way out";
			break;
		case MOD_TARGET_LASER:
			message = "saw the light";
			break;
		case MOD_TARGET_BLASTER:
			message = "got blasted";
			break;
		case MOD_BOMB:
		case MOD_SPLASH:
		case MOD_TRIGGER_HURT:
			message = "was in the wrong place";
			break;
		}
		if (attacker == self)
		{
			switch (mod)
			{
			case MOD_HELD_GRENADE:
				message = "tried to put the pin back in";
				break;
			case MOD_HG_SPLASH:
			case MOD_G_SPLASH:
				if (IsFemale(self))
					message = "tripped on her own grenade";
				else
					message = "tripped on his own grenade";
				break;
			case MOD_R_SPLASH:
				if (IsFemale(self))
					message = "blew herself up";
				else
					message = "blew himself up";
				break;
			case MOD_BFG_BLAST:
				message = "should have used a smaller gun";
				break;
			default:
				if (IsFemale(self))
					message = "killed herself";
				else
					message = "killed himself";
				break;
			}
		}
		if (message)
		{
			if (sync_stat != 2)
			{
				if (!(self->flags & FL_OSP_NOCMD))
					gi.cprintf (self, PRINT_MEDIUM, "%s %s.\n",
						self->client->pers.greenname, message);

				for (j = 1; j <= game.maxclients; j++)
				{
					e = g_edicts + j;
					if (!e->inuse || !e->client)
						continue;
					if (e != self && !(e->flags & FL_OSP_NOCMD))
						gi.cprintf (e, PRINT_MEDIUM, "%s %s.\n",
							self->client->pers.netname, message);
				}
			}

			if (sync_stat > 2)
			{
				if (deathmatch->value)
				{
					self->client->resp.score--;
					self->client->resp.osp_r2c0++;

					if (m_mode > 1)
					{
						teams[self->client->resp.team].osp_m108++;
						teams[self->client->resp.team].osp_m0f8--;

						if (m_mode == 2)
							OSP_playerTeamFrags (self);

						if (frag_offset)
							frag_offset--;
					}

					OSP_DoRankSort ();
				}
			}

			self->enemy = NULL;
			return;
		}

		self->enemy = attacker;
		if (attacker && attacker->client)
		{
			switch (mod)
			{
			case MOD_GRAPPLE:
				message = "was hooked to death by";
				break;
			case MOD_BLASTER:
				message = "was blasted by";
				break;
			case MOD_SHOTGUN:
				message = "was gunned down by";
				break;
			case MOD_SSHOTGUN:
				message = "was blown away by";
				message2 = "'s super shotgun";
				break;
			case MOD_MACHINEGUN:
				message = "was machinegunned by";
				break;
			case MOD_CHAINGUN:
				message = "was cut in half by";
				message2 = "'s chaingun";
				break;
			case MOD_GRENADE:
				message = "was popped by";
				message2 = "'s grenade";
				break;
			case MOD_G_SPLASH:
				message = "was shredded by";
				message2 = "'s shrapnel";
				break;
			case MOD_ROCKET:
				message = "ate";
				message2 = "'s rocket";
				break;
			case MOD_R_SPLASH:
				message = "almost dodged";
				message2 = "'s rocket";
				break;
			case MOD_HYPERBLASTER:
				message = "was melted by";
				message2 = "'s hyperblaster";
				break;
			case MOD_RAILGUN:
				message = "was railed by";
				break;
			case MOD_BFG_LASER:
				message = "saw the pretty lights from";
				message2 = "'s BFG";
				break;
			case MOD_BFG_BLAST:
				message = "was disintegrated by";
				message2 = "'s BFG blast";
				break;
			case MOD_BFG_EFFECT:
				message = "couldn't hide from";
				message2 = "'s BFG";
				break;
			case MOD_HANDGRENADE:
				message = "caught";
				message2 = "'s handgrenade";
				break;
			case MOD_HG_SPLASH:
				message = "didn't see";
				message2 = "'s handgrenade";
				break;
			case MOD_HELD_GRENADE:
				message = "feels";
				message2 = "'s pain";
				break;
			case MOD_TELEFRAG:
				message = "tried to invade";
				message2 = "'s personal space";
				break;
			}
			if (message)
			{
				if (!ff)
				{
					if (!(self->flags & FL_OSP_NOCMD))
						gi.cprintf (self, PRINT_MEDIUM, "%s %s %s%s\n",
							self->client->pers.netname, message,
							attacker->client->pers.greenname, message2);
					if (!(attacker->flags & FL_OSP_NOCMD))
						gi.cprintf (attacker, PRINT_MEDIUM, "%s %s %s%s\n",
							self->client->pers.greenname, message,
							attacker->client->pers.netname, message2);
				}
				else
				{
					if (!(self->flags & FL_OSP_NOCMD))
						gi.cprintf (self, PRINT_MEDIUM,
							"%s %s %s%s  ** Teammate Kill **\n",
							self->client->pers.netname, message,
							attacker->client->pers.greenname, message2);
					if (!(attacker->flags & FL_OSP_NOCMD))
						gi.cprintf (attacker, PRINT_MEDIUM,
							"%s %s %s%s  ** Teammate Kill **\n",
							self->client->pers.greenname, message,
							attacker->client->pers.netname, message2);
				}

				for (j = 1; j <= game.maxclients; j++)
				{
					e = g_edicts + j;
					if (!e->inuse || !e->client)
						continue;
					if (e != self && e != attacker &&
						!(e->flags & FL_OSP_NOCMD))
					{
						if (!ff)
							gi.cprintf (e, PRINT_MEDIUM, "%s %s %s%s\n",
								self->client->pers.netname, message,
								attacker->client->pers.netname, message2);
						else
							gi.cprintf (e, PRINT_MEDIUM,
								"%s %s %s%s  ** Teammate Kill **\n",
								self->client->pers.netname, message,
								attacker->client->pers.netname, message2);
					}
				}

				if ((int)dedicated->value)
					gi.dprintf ("%s %s %s%s\n", self->client->pers.netname,
						message, attacker->client->pers.netname, message2);

				if (sync_stat > 2)
				{
					if (m_mode > 1)
					{
						if (ff)
						{
							attacker->client->resp.score--;
							attacker->client->resp.osp_r028++;
							teams[self->client->resp.team].osp_m0fc++;
							teams[attacker->client->resp.team].osp_m0f8--;
							teams[attacker->client->resp.team].osp_m104++;
						}
						else
						{
							attacker->client->resp.score++;
							self->client->resp.osp_r014++;
							teams[self->client->resp.team].osp_m0fc++;
							teams[attacker->client->resp.team].osp_m0f8++;
							teams[attacker->client->resp.team].osp_m100++;

							if ((int)fraglimit->value &&
								attacker->client->resp.score >= fraglimit->value)
								self->client->resp.osp_r2dc = 2;
						}

						if (m_mode == 2)
							OSP_playerTeamFrags (attacker);
					}
					else
					{
						attacker->client->resp.score++;
						self->client->resp.osp_r014++;

						if ((int)fraglimit->value &&
							attacker->client->resp.score >= fraglimit->value)
							self->client->resp.osp_r2dc = 2;
					}

					OSP_DoRankSort ();
				}

				return;
			}
		}
	}

	if (sync_stat != 2)
		gi.bprintf (PRINT_MEDIUM, "%s died.\n", self->client->pers.netname);

	if (sync_stat > 2)
	{
		if (deathmatch->value)
		{
			self->client->resp.score--;
			self->client->resp.osp_r2c0++;

			if (m_mode > 1)
			{
				teams[self->client->resp.team].osp_m108++;
				teams[self->client->resp.team].osp_m0f8--;

				if (m_mode == 2)
					OSP_playerTeamFrags (self);

				if (frag_offset)
					frag_offset--;
			}
		}
	}
}


void Touch_Item (edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf);

// gamex86.dll: 100537AB..10053A35
// gamei386.so: 000392AC..00039516
void TossClientWeapon (edict_t *self)
{
	gitem_t		*item;
	edict_t		*drop;
	qboolean	quad;
	float		spread;

	if (!deathmatch->value)
		return;

	item = self->client->pers.weapon;
	if (! self->client->pers.inventory[self->client->ammo_index] )
		item = NULL;
	if (item && (strcmp (item->pickup_name, "Blaster") == 0))
		item = NULL;

	if (!((int)(dmflags->value) & DF_QUAD_DROP))
	{
		if (self->client->resp.osp_r200 &&
			self->client->quad_framenum < level.framenum)
		{
			q2log_expireItem ("Quad", self, self->client->resp.osp_r200);
			self->client->resp.osp_r200 = 0;
		}
		quad = false;
	}
	else
		quad = (self->client->quad_framenum > (level.framenum + 10));

	if (item && quad)
		spread = 22.5;
	else
		spread = 0.0;

	if (item)
	{
		self->client->v_angle[YAW] -= spread;
		drop = Drop_Item (self, item);
		self->client->v_angle[YAW] += spread;
		drop->spawnflags = DROPPED_PLAYER_ITEM;
	}

	if (quad)
	{
		self->client->v_angle[YAW] += spread;
		drop = Drop_Item (self, FindItemByClassname ("item_quad"));
		self->client->v_angle[YAW] -= spread;
		drop->spawnflags |= DROPPED_PLAYER_ITEM;

		q2log_dropItem ("Quad", drop - g_edicts, self);
		self->client->resp.osp_r200 = 0;

		drop->touch = Touch_Item;
		drop->nextthink = level.time + (self->client->quad_framenum - level.framenum) * FRAMETIME;
		drop->think = G_FreeEdict;
	}
}


/*
==================
LookAtKiller
==================
*/
// gamex86.dll: 10053A35..10053BAD
// gamei386.so: 00039518..0003965A
void LookAtKiller (edict_t *self, edict_t *inflictor, edict_t *attacker)
{
	vec3_t		dir;

	if (attacker && attacker != world && attacker != self)
	{
		VectorSubtract (attacker->s.origin, self->s.origin, dir);
	}
	else if (inflictor && inflictor != world && inflictor != self)
	{
		VectorSubtract (inflictor->s.origin, self->s.origin, dir);
	}
	else
	{
		self->client->killer_yaw = self->s.angles[YAW];
		return;
	}

	if (dir[0])
		self->client->killer_yaw = 180/M_PI*atan2(dir[1], dir[0]);
	else {
		self->client->killer_yaw = 0;
		if (dir[1] > 0)
			self->client->killer_yaw = 90;
		else if (dir[1] < 0)
			self->client->killer_yaw = -90;
	}
	if (self->client->killer_yaw < 0)
		self->client->killer_yaw += 360;
	

}

/*
==================
player_die
==================
*/
// gamex86.dll: 10053BAD..10053F65
// gamei386.so: 0003965C..000399ED
void player_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int		n;

	VectorClear (self->avelocity);

	self->takedamage = DAMAGE_YES;
	self->movetype = MOVETYPE_TOSS;

	self->s.modelindex2 = 0;	// remove linked weapon model

	self->s.angles[0] = 0;
	self->s.angles[2] = 0;

	self->s.sound = 0;
	self->client->weapon_sound = 0;

	self->maxs[2] = -8;

//	self->solid = SOLID_NOT;
	self->svflags |= SVF_DEADMONSTER;

	if (!self->deadflag)
	{
		self->client->respawn_time = level.time + 1.0;
		LookAtKiller (self, inflictor, attacker);
		self->client->ps.pmove.pm_type = PM_DEAD;
		ClientObituary (self, inflictor, attacker);

		if (sync_stat > 2)
			sl_WriteStdLogDeath (&gi, level, self, inflictor, attacker);
		q2log_logDeath (self, inflictor, attacker);

		if ((int)client_deathweapdrop->value)
			TossClientWeapon (self);
		if (rune_stat)
			OSP_deadDropRune (self);

		if (sync_stat != 2 && !(self->flags & FL_OSP_NOCMD))
		{
			self->client->resp.osp_r2dc = 1;
		}
	}

	// remove powerups
	self->client->quad_framenum = 0;
	self->client->invincible_framenum = 0;
	self->client->breather_framenum = 0;
	self->client->enviro_framenum = 0;
	self->flags &= ~FL_POWER_ARMOR;

	if (self->health < -40)
	{	// gib
		if (sync_stat != 2)
			gi.sound (self, CHAN_BODY, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		for (n= 0; n < (int)numgibs->value; n++)
			ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
		ThrowClientHead (self, damage);

		self->takedamage = DAMAGE_NO;
	}
	else
	{	// normal death
		if (!self->deadflag)
		{
			static int i;

			i = (i+1)%3;
			// start a death animation
			self->client->anim_priority = ANIM_DEATH;
			switch (i)
			{
			case 0:
				self->s.frame = FRAME_death101-1;
				self->client->anim_end = FRAME_death106;
				break;
			case 1:
				self->s.frame = FRAME_death201-1;
				self->client->anim_end = FRAME_death206;
				break;
			case 2:
				self->s.frame = FRAME_death301-1;
				self->client->anim_end = FRAME_death308;
				break;
			}
			if (sync_stat != 2)
				gi.sound (self, CHAN_VOICE, gi.soundindex(va("*death%i.wav", (rand()%4)+1)), 1, ATTN_NORM, 0);
		}
	}

	self->deadflag = DEAD_DEAD;

	gi.linkentity (self);
}

//=======================================================================

/*
==============
InitClientPersistant

This is only called when the game first initializes in single player,
but is called after each death and level change in deathmatch
==============
*/
// gamex86.dll: 10053F65..10053FCE
// gamei386.so: 000399F0..00039A49
void InitClientPersistant (gclient_t *client, qboolean full)
{
	int		keep;					// invented name

	if (full)
		memset (&client->pers, 0, sizeof(client->pers));
	else
	{
		keep = sizeof(client->pers.userinfo) + sizeof(client->pers.netname)
				+ sizeof(client->pers.greenname);
		memset ((byte *)&client->pers + keep, 0,
				sizeof(client->pers) - keep);
	}

	OSP_seedPlayer (client);

	client->pers.connected = true;
}


// gamex86.dll: 10053FCE..1005407E
// gamei386.so: 00039A4C..00039AFF
void InitClientResp (gclient_t *client)
{
	int		clientid;
	int		save;

	clientid = client->resp.clientid;
	save = client->resp.osp_r008;

	memset (&client->resp, 0, sizeof(client->resp));

	client->resp.clientid = clientid;
	client->resp.osp_r008 = save;
	client->resp.enterframe = level.framenum;
	client->resp.coop_respawn = client->pers;
	client->resp.team = 2;
	client->resp.osp_r00c = (int)client_hud->value;
	client->resp.osp_r01c = start_count;
}

/*
==================
SaveClientData

Some information that should be persistant, like health, 
is still stored in the edict structure, so it needs to
be mirrored out to the client structure before all the
edicts are wiped.
==================
*/
// gamex86.dll: 1005407E..10054166
// gamei386.so: 00039B00..00039BF9
void SaveClientData (void)
{
	int		i;
	edict_t	*ent;

	for (i=0 ; i<game.maxclients ; i++)
	{
		ent = &g_edicts[1+i];
		if (!ent->inuse)
			continue;
		game.clients[i].pers.health = ent->health;
		game.clients[i].pers.max_health = ent->max_health;
		game.clients[i].pers.savedFlags = (ent->flags & (FL_GODMODE|FL_NOTARGET|FL_POWER_ARMOR));
		if (coop->value)
			game.clients[i].pers.score = ent->client->resp.score;
	}
}

// gamex86.dll: 10054166..100541E1
// gamei386.so: 00039BFC..00039C69
void FetchClientEntData (edict_t *ent)
{
	ent->health = ent->client->pers.health;
	ent->max_health = ent->client->pers.max_health;
	ent->flags |= ent->client->pers.savedFlags;
	if (coop->value)
		ent->client->resp.score = ent->client->pers.score;
}



/*
=======================================================================

  SelectSpawnPoint

=======================================================================
*/

/*
================
PlayersRangeFromSpot

Returns the distance to the nearest player from the given spot
================
*/
// gamex86.dll: 100541E1..100542BB
// gamei386.so: 00039C6C..00039D56
float	PlayersRangeFromSpot (edict_t *spot, edict_t *ent)
{
	edict_t	*player;
	float	bestplayerdistance;
	vec3_t	v;
	int		n;
	float	playerdistance;


	bestplayerdistance = 9999999;

	for (n = 1; n <= maxclients->value; n++)
	{
		player = &g_edicts[n];

		if (!player->inuse || !player->client || player == ent ||
			player->client->resp.entered != ENTERED_ENTERED)
			continue;

		if (player->health <= 0)
			continue;

		VectorSubtract (spot->s.origin, player->s.origin, v);
		playerdistance = VectorLength (v);

		if (playerdistance < bestplayerdistance)
			bestplayerdistance = playerdistance;
	}

	return bestplayerdistance;
}

/*
================
SelectRandomDeathmatchSpawnPoint

go to a random point, but NOT the two points closest
to other players
================
*/
// gamex86.dll: 100542BB..1005440B
// gamei386.so: 00039D58..00039F50
edict_t *SelectRandomDeathmatchSpawnPoint (edict_t *ent)
{
	edict_t	*spot, *spot1, *spot2;
	int		count = 0;
	int		selection;
	float	range, range1, range2;

	spot = NULL;
	range1 = range2 = 99999;
	spot1 = spot2 = NULL;

	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL)
	{
		count++;
		range = PlayersRangeFromSpot(spot, ent);
		if (range < range1)
		{
			range1 = range;
			spot1 = spot;
		}
		else if (range < range2)
		{
			range2 = range;
			spot2 = spot;
		}
	}

	if (!count)
		return NULL;

	if (count <= 2)
	{
		spot1 = spot2 = NULL;
	}
	else
		count -= 2;

	if (range1 == 0 && range2 == 0)
		spot1 = spot2 = NULL;

	selection = rand() % count;

	spot = NULL;
	do
	{
		spot = G_Find (spot, FOFS(classname), "info_player_deathmatch");
		if (spot == spot1 || spot == spot2)
			selection++;
	} while(selection--);

	return spot;
}

/*
================
SelectFarthestDeathmatchSpawnPoint

================
*/
// gamex86.dll: 1005440B..100544DB
// gamei386.so: 00039F50..0003A0CB
edict_t *SelectFarthestDeathmatchSpawnPoint (edict_t *ent)
{
	edict_t	*bestspot;
	float	bestdistance, bestplayerdistance;
	edict_t	*spot;
	int		count = 0;
	int		selection;

	spot = NULL;
	bestspot = NULL;
	bestdistance = 0;
	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL)
	{
		bestplayerdistance = PlayersRangeFromSpot (spot, ent);
		count++;

		if (bestplayerdistance > bestdistance)
		{
			bestspot = spot;
			bestdistance = bestplayerdistance;
		}
	}

	if (bestspot)
	{
		return bestspot;
	}

	// if there is a player just spawned on each and every start spot
	// we have no choice to turn one into a telefrag meltdown
	if (!count)
		return NULL;

	selection = rand() % count;

	spot = NULL;
	do
	{
		spot = G_Find (spot, FOFS(classname), "info_player_deathmatch");
	} while (selection--);

	return spot;
}

// gamex86.dll: 100544DB..10054510
// gamei386.so: 0003A0CC..0003A11F
edict_t *SelectDeathmatchSpawnPoint (edict_t *ent)
{
	if ( (int)(dmflags->value) & DF_SPAWN_FARTHEST)
		return SelectFarthestDeathmatchSpawnPoint (ent);
	else
		return SelectRandomDeathmatchSpawnPoint (ent);
}


// `static` and unreferenced -- SelectSpawnPoint's coop arm is gone, so
// nothing calls this, but the original binary keeps it too.
// gamex86.dll: absent
// gamei386.so: absent
static edict_t *SelectCoopSpawnPoint (edict_t *ent)
{
	int		index;
	edict_t	*spot = NULL;
	char	*target;

	index = ent->client - game.clients;

	// player 0 starts in normal player spawn point
	if (!index)
		return NULL;

	spot = NULL;

	// assume there are four coop spots at each spawnpoint
	while (1)
	{
		spot = G_Find (spot, FOFS(classname), "info_player_coop");
		if (!spot)
			return NULL;	// we didn't have enough...

		target = spot->targetname;
		if (!target)
			target = "";
		if ( Q_stricmp(game.spawnpoint, target) == 0 )
		{	// this is a coop spawn point for one of the clients here
			index--;
			if (!index)
				return spot;		// this is it
		}
	}


	return spot;
}


/*
===========
SelectSpawnPoint

Chooses a player start, deathmatch start, coop start, etc
============
*/
// gamex86.dll: 10054510..10054678
// gamei386.so: 0003A120..0003A34E
qboolean SelectSpawnPoint (edict_t *ent, vec3_t origin, vec3_t angles)
{
	edict_t	*spot = NULL;

	spot = SelectDeathmatchSpawnPoint (ent);

	// find a single player start spot
	if (!spot)
	{
		while ((spot = G_Find (spot, FOFS(classname), "info_player_start")) != NULL)
		{
			if (!game.spawnpoint[0] && !spot->targetname)
				break;

			if (!game.spawnpoint[0] || !spot->targetname)
				continue;

			if (Q_stricmp(game.spawnpoint, spot->targetname) == 0)
				break;
		}

		if (!spot)
		{
			if (!game.spawnpoint[0])
			{	// there wasn't a spawnpoint without a target, so use any
				spot = G_Find (spot, FOFS(classname), "info_player_start");
			}
			if (!spot)
				gi.error ("Couldn't find spawn point %s\n", game.spawnpoint);
		}
	}

	// 60.0 is a DOUBLE literal here, not 0.0.
	if (60.0 > PlayersRangeFromSpot (spot, ent) &&
		ent->client->resp.entered == ENTERED_ENTERED)
		return false;

	VectorCopy (spot->s.origin, origin);
	origin[2] += 9;
	VectorCopy (spot->s.angles, angles);

	return true;
}

//======================================================================


// gamex86.dll: 10054678..100546BB
// gamei386.so: 0003A350..0003A394
void InitBodyQue (void)
{
	int		i;
	edict_t	*ent;

	level.body_que = 0;
	for (i=0; i<BODY_QUEUE_SIZE ; i++)
	{
		ent = G_Spawn();
		ent->classname = "bodyque";
	}
}

// gamex86.dll: 100546BB..10054769
// gamei386.so: 0003A394..0003A452
void body_die (edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
	int	n;

	if (self->health < -40)
	{
		gi.sound (self, CHAN_BODY, gi.soundindex ("misc/udeath.wav"), 1, ATTN_NORM, 0);
		for (n= 0; n < (int)numgibs->value; n++)
			ThrowGib (self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
		self->s.origin[2] -= 48;
		ThrowClientHead (self, damage);
		self->takedamage = DAMAGE_NO;
	}
}

// gamex86.dll: 10054769..10054984
// gamei386.so: 0003A454..0003A648
void CopyToBodyQue (edict_t *ent)
{
	edict_t		*body;

	// grab a body que and cycle to the next one
	body = &g_edicts[(int)maxclients->value + level.body_que + 1];
	level.body_que = (level.body_que + 1) % BODY_QUEUE_SIZE;

	// FIXME: send an effect on the removed body

	gi.unlinkentity (ent);

	gi.unlinkentity (body);
	body->s = ent->s;
	body->s.number = body - g_edicts;

	body->svflags = ent->svflags;
	VectorCopy (ent->mins, body->mins);
	VectorCopy (ent->maxs, body->maxs);
	VectorCopy (ent->absmin, body->absmin);
	VectorCopy (ent->absmax, body->absmax);
	VectorCopy (ent->size, body->size);
	body->solid = ent->solid;
	body->clipmask = ent->clipmask;
	body->owner = ent->owner;
	body->movetype = ent->movetype;

	body->die = body_die;
	body->takedamage = DAMAGE_YES;

	gi.linkentity (body);
}


// gamex86.dll: 10054984..100549D0
// gamei386.so: 0003A648..0003A696
void respawn (edict_t *self)
{
	CopyToBodyQue (self);
	PutClientInServer (self);

	// add a teleportation effect
	self->s.event = EV_PLAYER_TELEPORT;

	// hold in place briefly
	self->client->ps.pmove.pm_flags = PMF_TIME_TELEPORT;
	self->client->ps.pmove.pm_time = 14;

	self->client->respawn_time = level.time;
}

//==============================================================


/*
===========
PutClientInServer

Called when a player connects to a server or respawns in
a deathmatch.
============
*/
// gamex86.dll: 100549D0..10055218
// gamei386.so: 0003A698..0003AEBC
void PutClientInServer (edict_t *ent)
{
	vec3_t	mins = {-16, -16, -24};
	vec3_t	maxs = {16, 16, 32};
	int		index;
	vec3_t	spawn_origin, spawn_angles;
	gclient_t	*client;
	int		i;
	client_persistant_t	saved;
	client_respawn_t	resp;
	char		userinfo[MAX_INFO_STRING];

	// find a spawn point
	// do it before setting health back up, so farthest
	// ranging doesn't count this client
	if (!SelectSpawnPoint (ent, spawn_origin, spawn_angles) &&
		ent->client->resp.entered == ENTERED_ENTERED)
	{
		ent->movetype = MOVETYPE_NOCLIP;
		ent->svflags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;
		ent->clipmask = 0;
		ent->client->resp.osp_r240 = 0;
		ent->client->ps.pmove.pm_type = PM_FREEZE;
		return;
	}

	ent->client->resp.osp_r240 = 2;

	index = ent-g_edicts-1;
	client = ent->client;

	memcpy (userinfo, client->pers.userinfo, sizeof(userinfo) - 1);
	userinfo[sizeof(userinfo) - 1] = 0;

	InitClientPersistant (client, false);
	if (sync_stat < 2)
		OSP_warmupItems (ent);
	ClientUserinfoChanged (ent, userinfo);

	resp = client->resp;

	// clear everything but the persistant data
	saved = client->pers;
	memset (client, 0, sizeof(*client));
	client->pers = saved;
	if (client->pers.health <= 0)
		InitClientPersistant (client, false);
	client->resp = resp;

	// copy some data from the client to the entity
	FetchClientEntData (ent);

	// clear entity values
	ent->groundentity = NULL;
	ent->client = &game.clients[index];
	ent->takedamage = DAMAGE_AIM;

	if (client->resp.entered != ENTERED_ENTERED)
	{
		ent->movetype = MOVETYPE_NOCLIP;
		ent->svflags |= SVF_NOCLIENT;
		ent->solid = SOLID_NOT;
		ent->clipmask = 0;
		ent->client->resp.osp_r2bc = 1;
		OSP_setSingleAccuracy (ent);
		client->resp.osp_r240 = 0;

		if (!worldlog_file)
			q2log_playerMode (ent, "Observe");
	}
	else
	{
		ent->movetype = MOVETYPE_WALK;
		ent->solid = SOLID_BBOX;
		ent->clipmask = MASK_PLAYERSOLID;
		ent->svflags &= ~SVF_NOCLIENT;
		client->osp_t040 = 0;
		client->osp_t03c = NULL;
		client->resp.osp_r2dc = 0;
		client->resp.osp_r2b8 = 0;
		client->resp.osp_r2b4 = level.framenum + 100;
	}

	ent->viewheight = 22;
	ent->inuse = true;
	ent->classname = "player";
	ent->mass = 200;
	ent->deadflag = DEAD_NO;
	ent->air_finished = level.time + 12;
	ent->model = "players/male/tris.md2";
	ent->pain = player_pain;
	ent->die = player_die;
	ent->waterlevel = 0;
	ent->watertype = 0;
	ent->flags &= ~FL_NO_KNOCKBACK;
	ent->svflags &= ~SVF_DEADMONSTER;
	ent->flags &= ~FL_POWER_ARMOR;

	VectorCopy (mins, ent->mins);
	VectorCopy (maxs, ent->maxs);
	VectorClear (ent->velocity);

	// clear playerstate values
	memset (&ent->client->ps, 0, sizeof(client->ps));

	client->ps.pmove.origin[0] = spawn_origin[0]*8;
	client->ps.pmove.origin[1] = spawn_origin[1]*8;
	client->ps.pmove.origin[2] = spawn_origin[2]*8;

	if ((int)dmflags->value & DF_FIXED_FOV)
	{
		client->ps.fov = 90;
	}
	else
	{
		client->ps.fov = atoi(Info_ValueForKey(client->pers.userinfo, "fov"));
		if (client->ps.fov < 1)
			client->ps.fov = 90;
		else if (client->ps.fov > 160)
			client->ps.fov = 160;
	}

	if (client->resp.osp_r240 == 2)
		client->ps.gunindex = gi.modelindex (client->pers.weapon->view_model);
	else
		client->ps.gunindex = 0;

	// clear entity state values
	ent->s.effects = 0;
	ent->s.modelindex = 255;		// will use the skin specified model
	ent->s.modelindex2 = 255;		// custom gun model
	// sknum is player num and weapon number
	// weapon number will be added in changeweapon
	ent->s.skinnum = ent - g_edicts - 1;

	ent->s.frame = 0;
	VectorCopy (spawn_origin, ent->s.origin);
	ent->s.origin[2] += 1;	// make sure off ground
	VectorCopy (ent->s.origin, ent->s.old_origin);

	// set the delta angle
	for (i=0 ; i<3 ; i++)
	{
		client->ps.pmove.delta_angles[i] = ANGLE2SHORT(spawn_angles[i] - client->resp.cmd_angles[i]);
	}

	ent->s.angles[PITCH] = 0;
	ent->s.angles[YAW] = spawn_angles[YAW];
	ent->s.angles[ROLL] = 0;
	VectorCopy (ent->s.angles, client->ps.viewangles);
	VectorCopy (ent->s.angles, client->v_angle);

	if (client->resp.osp_r240 == 2)
		KillBox (ent);

	gi.linkentity (ent);
	OSP_hookoff_cmd (ent);
	ent->client->inmenu = false;
	ent->client->menu = NULL;
	ent->client->resp.osp_r010 = level.framenum + 2;
	OSP_restartStats (ent);

	// force the current weapon up
	client->newweapon = client->pers.weapon;
	ChangeWeapon (ent);

	if (client->resp.entered == ENTERED_ENTERED &&
		sync_stat > 2 && !level.intermissiontime)
		q2log_playerRespawn (ent);
}

/*
=====================
ClientBeginDeathmatch

A client has just connected to the server in 
deathmatch mode, so clear everything out before starting them.
=====================
*/
// gamex86.dll: 10055218..10055889
// gamei386.so: 0003AEBC..0003B5E8
void ClientBeginDeathmatch (edict_t *ent)
{
	G_InitEdict (ent);

	if (!ent->client->resp.osp_r210)
	{
		InitClientResp (ent->client);
		ent->client->resp.team = 2;
	}

	if (worldlog_file && !(ent->flags & FL_OSP_NOCMD))
	{
		ent->client->resp.osp_r2a8 = 0;
		gi.WriteByte (svc_stufftext);
		gi.WriteString ("cmd _ngws_client_id $ngWorldStats_password $ngworldstats_password\n");
		gi.unicast (ent, true);
	}
	else if (!ent->client->resp.osp_r2a8)
	{
		ent->client->resp.osp_r2a8 = 1;
		q2log_playerConnect (ent);
	}

	if (m_mode > 0 && !ent->osp_e39c && !(ent->flags & FL_OSP_NOCMD))
	{
		gi.WriteByte (svc_stufftext);
		gi.WriteString ("cmd _is_referee $ref_status $ref_passwd\n");
		gi.unicast (ent, true);
	}

	if (m_mode > 1 && !ent->client->resp.osp_r210 &&
		!ent->osp_e3a0[0] && !(ent->flags & FL_OSP_NOCMD))
	{
		ent->osp_e3a0[0] = 0;
		ent->osp_e3b0[0] = 0;
		if (!(int)team_lockskin->value)
		{
			gi.WriteByte (svc_stufftext);
			gi.WriteString ("cmd _default_team_info $default_teamname $default_teamskin\n");
			gi.unicast (ent, true);
		}
		OSP_observerTeamFrags (ent);
	}

	if (m_mode == 2 && !ent->client->resp.osp_r210 &&
		!(ent->flags & FL_OSP_NOCMD))
	{
		ent->client->resp.osp_r07d[0] = 0;
		gi.WriteByte (svc_stufftext);
		gi.WriteString ("cmd _default_join_code $default_joincode\n");
		gi.unicast (ent, true);
	}
	if (!(ent->flags & FL_OSP_NOCMD))
		OSP_hookAliases (ent);

	if (!ent->client->resp.osp_r210)
	{
		ent->client->resp.entered = 2;
		ent->client->resp.osp_r240 = 0;
	}
	else if (ent->client->resp.entered == ENTERED_ENTERED)
	{
		active_clients++;
		OSP_DoRankSort ();
	}

	PutClientInServer (ent);

	if (level.intermissiontime)
		MoveClientToIntermission (ent);
	else
		OSP_playerAnnounce (ent, 9);

	connected_clients++;
	ent->client->resp.osp_r0a0 = -1;
	ent->client->resp.osp_r2ac = -1;
	ent->client->resp.osp_r09c = 0;
	ent->client->resp.osp_r0b0 = (int)client_muzzlemode->value;

	if (!ent->client->resp.osp_r210)
	{
		ent->client->resp.score = -100;
		ent->client->resp.osp_r248 = 0;
		ent->client->resp.osp_r0ac = level.framenum;
		ent->client->resp.osp_r24c = 0;
		ent->client->showscores = false;
		if (m_mode > 1)
			ent->client->resp.team = 2;
		ent->client->resp.osp_r204 = OSP_initID ();

		if (m_mode == 3 && OSP_teamCount (0) && OSP_teamCount (0))
			ent->client->resp.osp_r02c = 1;
	}

	if (m_mode < 2)
	{
		OSP_DoRankSort ();
		OSP_showFrags (ent);
	}
	sl_WriteStdLogPlayerEntered (&gi, level, ent);

	if (sync_stat == 4 && !(int)match_latejoin->value &&
		!ent->osp_e39c && !(ent->flags & FL_OSP_NOCMD))
	{
		int		mins;
		int		secs;

		mins = (int)(timelimit->value + overtime_timer -
			(level.framenum - sync_frame) / 600) - 1;
		secs = (int)((overtime_timer + timelimit->value) * 60 -
			(level.framenum - sync_frame) / 10) - mins * 60 - 1;
		if (secs == 60)
		{
			secs = 0;
			mins++;
		}
		else if (mins < 0)
		{
			secs = 0;
			mins = 0;
		}
		gi.cprintf (ent, PRINT_HIGH,
			"Match already started.\nTime left in match: %d:%.2d\n", mins, secs);
		gi.WriteByte (svc_disconnect);
		gi.unicast (ent, true);
		ClientDisconnect (ent);
		return;
	}

	OSP_setShowParams ();
	ent->client->resp.osp_r210 = 0;
	OSP_zeroRuneStats (ent);

	if (!(((int)client_minping->value || (int)client_maxping->value) &&
		!(ent->flags & FL_OSP_BOT)))
		ent->client->resp.osp_r1fc = -1;
	else
		ent->client->resp.osp_r1fc = 0;

	if (!(client_maxframes && !(ent->flags & FL_OSP_BOT)))
		ent->client->resp.osp_r024 = -1;
	else
		ent->client->resp.osp_r024 = 0;

	ent->client->resp.osp_r0d4 = level.framenum + 60;
	if (bot_watch && !(ent->flags & FL_OSP_NOCMD))
	{
		ent->client->resp.osp_r2b8 = 0;
		ent->client->resp.osp_r2b4 = level.framenum + 100;
	}
	else
		ent->client->resp.osp_r2b8 = 16;

	ClientEndServerFrame (ent);
}


/*
===========
ClientBegin

called when a client has finished connecting, and is ready
to be placed into the game.  This will happen every level load.
============
*/
// gamex86.dll: 10055889..10055B25
// gamei386.so: 0003B5E8..0003B871
void ClientBegin (edict_t *ent)
{
	ent->client = game.clients + (ent - g_edicts - 1);
	ent->client->resp.osp_r024 = 0;

	if (!ent->client->resp.osp_r210)
	{
		ent->client->resp.team = 2;
		ent->client->resp.osp_r20c = 0;
		ent->client->resp.osp_r030 = 0;
		ent->client->resp.osp_r00c = (int)client_hud->value;
	}

	ent->client->resp.osp_r2c4 = 0;
	ent->client->resp.osp_r01c = start_count;
	ent->client->menu = NULL;
	ent->client->inmenu = false;
	PlayerResetGrapple (ent);
	ent->client->osp_t00c = 0;
	ent->client->chase_target = NULL;
	ent->client->update_chase = false;

	if (!ent->client->resp.osp_r210)
	{
		OSP_giveClientID (ent);
		strcpy (p_acc[ent->client->resp.clientid].osp_a010, ent->osp_e37c);

		if (!worldlog_file || (ent->flags & FL_OSP_BOT))
		{
			ent->client->resp.osp_r2a8 = 1;
			q2log_playerConnect (ent);
		}
	}
	else
	{
		q2log_playerReconnect (ent);
		EntityListAdd (ent);

		if (m_mode == 2)
		{
			if (sync_stat > 2)
				ent->client->resp.osp_r078 = ent->client->resp.team + 1;
			OSP_readdTeamMember (ent);
			OSP_initTeamFrags (ent);
		}
		else if (m_mode == 3)
		{
			OSP_readdTeamMember (ent);
		}

		if (m_mode > 1 &&
			!Q_stricmp (ent->client->pers.netname, reconn_player))
		{
			char	message[64];

			who_paused = -1;
			match_paused = 3;
			sprintf (message, "%s has returned!\nMatch continues.\n", reconn_player);
			gi.bprintf (PRINT_CHAT, "%s", message);
			reconn_player[0] = 0;
		}
	}

	ent->inuse = true;
	OSP_1v1Add (ent);
	ClientBeginDeathmatch (ent);
}

/*
===========
ClientUserInfoChanged

called whenever the player updates a userinfo variable.

The game can override any of the settings in place
(forcing skins or names, etc) before copying it off.
============
*/
// gamex86.dll: 10055B25..10056307
// gamei386.so: 0003B874..0003C171
void ClientUserinfoChanged (edict_t *ent, char *userinfo)
{
	char	newnick[16];
	char	*s;
	int		tnum;
	int		didskin;
	// `i` and `playernum` are FUNCTION-scope in the original.
	unsigned	i;
	int		playernum;

	tnum = ent->client->resp.team;
	didskin = 0;

	if (!Info_Validate(userinfo))
		strcpy (userinfo, "\\name\\badinfo\\skin\\male/grunt");

	if ((int)client_maxrate->value && !(ent->flags & FL_OSP_BOT))
	{
		s = Info_ValueForKey (userinfo, "rate");
		if ((int)client_maxrate->value < atoi(s))
		{
			if (ent->client->pers.connected)
				gi.cprintf (ent, PRINT_HIGH,
					"*** Server max rate capped at %s\n", client_maxrate->string);
			Info_SetValueForKey (userinfo, "rate", client_maxrate->string);
		}
	}

	s = Info_ValueForKey (userinfo, "name");
	if (OSP_playerAllow (s, userinfo) ||
		(!m_mode && ent->charname && (int)ent->charname > level.framenum))
		s = ent->client->pers.netname;
	else
		s = Info_ValueForKey (userinfo, "name");

	if (ent->client->pers.netname[0])
	{
		strncpy (newnick, s, 15);
		newnick[15] = 0;
		if (Q_stricmp (ent->client->pers.netname, newnick) &&
			ent->client->resp.osp_r2a8)
		{
			q2log_playerRename (ent, s);
			sl_LogPlayerRename (&gi, ent->client->pers.netname, s, level.time);
			if (server_log)
				OSP_logAdminLog ("Rename: %s -> %s", ent->client->pers.netname, s);
		}
	}

	if (Q_stricmp (ent->client->pers.netname, s))
	{
		char	buf[24];

		if ((int)client_infochange->value)
			ent->charname = (char *)(level.framenum +
				(int)client_infochange->value * 10);
		else
			ent->charname = NULL;

		memcpy (ent->client->pers.netname, s, 15);
		if (ent->client->resp.clientid >= 0)
		{
			memcpy (p_acc[ent->client->resp.clientid].netname, s, 16);
			p_acc[ent->client->resp.clientid].netname[15] = 0;
		}

		for (i = 0; i < 16; i++)
			ent->client->pers.greenname[i] = 0;
		for (i = 0; i < strlen (ent->client->pers.netname); i++)
			ent->client->pers.greenname[i] =
				ent->client->pers.netname[i] + 128;

		if (m_mode == 3 && ent->client->resp.entered == ENTERED_ENTERED &&
			!ent->client->resp.osp_r210 && !level.intermissiontime)
		{
			memcpy (teams[tnum].netname, ent->client->pers.netname, 16);
			memcpy (teams[tnum].greenname, ent->client->pers.greenname, 16);
			sprintf (buf, "%15s", teams[tnum].greenname);
			gi.configstring (0x625 + tnum * 2, buf);

			if (ent->inuse && ent->client && !(ent->flags & FL_OSP_NOCMD))
			{
				sprintf (buf, "%15s", teams[tnum].netname);
				OSP_clientConfigString (ent, 0x625 + tnum * 2, buf);
			}
		}
		didskin = 1;
	}

	s = Info_ValueForKey (userinfo, "skin");
	if (sync_stat == 4 && m_mode == 1 && (int)qualifier_forceskins->value)
	{
		if (!strcmp (ent->client->resp.osp_r0f4, qualifier_skinname->string))
			s = ent->client->resp.osp_r0f4;
		else
		{
			s = qualifier_skinname->string;
			strncpy (ent->client->resp.osp_r0f4, s, 255);
			didskin = 1;
		}
	}
	else if ((m_mode > 1 && (int)team_lockskin->value) ||
			 (m_mode == 2 && tnum != 2))
	{
		if (!strcmp (ent->client->resp.osp_r0f4, teams[tnum].skin))
			s = ent->client->resp.osp_r0f4;
		else
		{
			s = teams[tnum].skin;
			strncpy (ent->client->resp.osp_r0f4, s, 255);
			didskin = 1;
		}
	}
	else
	{
		s = Info_ValueForKey (userinfo, "skin");
		if (!(m_mode || !ent->charname ||
			(int)ent->charname <= level.framenum || didskin) ||
			!strcmp (ent->client->resp.osp_r0f4, s))
		{
			s = ent->client->resp.osp_r0f4;
		}
		else
		{
			strncpy (ent->client->resp.osp_r0f4, s, 255);
			if ((int)client_infochange->value)
				ent->charname = (char *)(level.framenum +
					(int)client_infochange->value * 10);
			else
				ent->charname = NULL;
			didskin = 1;
		}
	}

	if (didskin)
	{
		playernum = ent - g_edicts - 1;

		gi.configstring (CS_PLAYERSKINS + playernum,
			va ("%s\\%s", ent->client->pers.netname, s));
	}

	if ((int)dmflags->value & DF_FIXED_FOV)
		ent->client->ps.fov = 90;
	else
	{
		ent->client->ps.fov = atoi (Info_ValueForKey (userinfo, "fov"));
		if (ent->client->ps.fov < 1)
			ent->client->ps.fov = 90;
		else if (ent->client->ps.fov > 160)
			ent->client->ps.fov = 160;
	}

	s = Info_ValueForKey (userinfo, "hand");
	if (strlen(s))
		ent->client->pers.hand = atoi(s);

	strncpy (ent->client->pers.userinfo, userinfo, sizeof(ent->client->pers.userinfo)-1);
}


/*
===========
ClientConnect

Called when a player begins connecting to the server.
The game can refuse entrance to a client by returning false.
If the client is allowed, the connection process will continue
and eventually get to ClientBegin()
Changing levels will NOT cause this to be called again, but
loadgames will.
============
*/
// gamex86.dll: 10056307..10056952
// gamei386.so: 0003C174..0003C81E
qboolean ClientConnect (edict_t *ent, char *userinfo)
{
	char	addr[1024];
	char	match[7] = { '`', 'r', 'e', 'q', 'i', '`', 0 };
	char	*value;
	int		idx;

	for (idx = 0; idx < 6; idx++)
		match[idx] -= 4;

	/* FL_OSP_NOCMD is the target's tested 0x2000 bit here (also FL_BOT). */
	if (ent->flags & FL_OSP_NOCMD)
	{
		if (!BotMoveToFreeClientEdict (ent))
			return false;
	}

	// check to see if they are on the banned IP list
	value = Info_ValueForKey (userinfo, "ip");
	if (SV_FilterPacket (value))
	{
		Info_SetValueForKey (userinfo, "rejmsg", "Banned.");
		return false;
	}

	value = Info_ValueForKey (userinfo, "name");
	idx = OSP_playerAllow (value, userinfo);
	if (idx)
	{
		if (idx < 0)
		{
			value = Info_ValueForKey (userinfo, "name");
			sprintf (addr, "%s is already connected.", value);
			Info_SetValueForKey (userinfo, "rejmsg", addr);
		}
		else if (idx == 1)
		{
			value = Info_ValueForKey (userinfo, "name");
			sprintf (addr, "%s is not allowed to play.", value);
			Info_SetValueForKey (userinfo, "rejmsg", addr);
		}
		else if (idx == 2)
		{
			value = Info_ValueForKey (userinfo, "name");
			sprintf (addr, "Incorrect password/address for %s", value);
			Info_SetValueForKey (userinfo, "rejmsg", addr);
		}
		else
			Info_SetValueForKey (userinfo, "rejmsg", "Your address has been banned!");

		return false;
	}

	value = Info_ValueForKey (userinfo, "password");
	if (*password->string && strcmp (password->string, "none") &&
		strcmp (password->string, value) && !(ent->flags & FL_BOTCLIENT))
	{
		Info_SetValueForKey (userinfo, "rejmsg", "Password required or incorrect.");
		return false;
	}

	ent->client = game.clients + (ent - g_edicts - 1);
	OSP_recoverClient (ent, userinfo);

	if (!ent->inuse)
	{
		if (!ent->client->resp.osp_r210)
		{
			ent->osp_e37c[0] = 0;
			ent->osp_e3a0[0] = 0;
			InitClientResp (ent->client);
			if (!game.autosaved || !ent->client->pers.weapon)
				InitClientPersistant (ent->client, true);
		}
	}

	if (!ent->client->resp.osp_r210)
	{
		ent->client->resp.entered = 2;
		ent->client->resp.team = 2;
		ent->client->resp.osp_r2c4 = 0;
		ent->client->resp.osp_r20c = 0;
		ent->client->resp.osp_r030 = 0;
		ent->client->resp.osp_r00c = (int)client_hud->value;
	}

	ent->client->menu = NULL;
	ent->client->inmenu = false;
	if ((int)hook_enable->value)
		PlayerResetGrapple (ent);
	ent->client->osp_t00c = 0;
	ent->client->chase_target = NULL;
	ent->client->update_chase = false;
	ent->client->osp_t040 = 0;
	ent->client->resp.osp_r01c = start_count;

	if (bot_watch && !(ent->flags & FL_BOT))
	{
		ent->client->resp.osp_r2b8 = 0;
		ent->client->resp.osp_r2b4 = level.framenum + 50;
	}
	else
		ent->client->resp.osp_r2b8 = 16;

	ent->client->pers.connected = false;
	ClientUserinfoChanged (ent, userinfo);
	if (game.maxclients > 1)
	{
		if (!ent->client->resp.osp_r210)
			gi.bprintf (PRINT_HIGH, "%s connected\n", ent->client->pers.netname);
		else
			gi.bprintf (PRINT_HIGH, "%s reconnected\n", ent->client->pers.netname);
	}

	if (ent->flags & FL_OSP_BOT)
	{
		strcpy (addr, "SERVER_BOT");
		ent->client->resp.osp_r008 = 0;
	}
	else
	{
		value = Info_ValueForKey (userinfo, "ip");
		sprintf (addr, "%s", value);
		value = strchr (addr, ':');
		if (value)
			*value = 0;
		if (strstr (userinfo, match) == userinfo)
			ent->client->resp.osp_r008 = 1;
		else
			ent->client->resp.osp_r008 = 0;
	}

	gi.dprintf ("(%s connected from %s)\n", ent->client->pers.netname, addr);
	strcpy (ent->osp_e37c, addr);
	if (server_log)
	{
		char	date[64];

		ngLog_getDateInfo (date, 0);
		OSP_logAdminLog ("Connect: %s - %s (%s)", addr,
			ent->client->pers.netname, date);
	}

	ent->osp_e39c = 0;
	ent->svflags = 0;
	ent->client->pers.connected = true;
	return true;
}

/*
===========
ClientDisconnect

Called when a player drops from the server.
Will not be called between levels.
============
*/
// gamex86.dll: 10056952..10057219
// gamei386.so: 0003C820..0003D17D
void ClientDisconnect (edict_t *ent)
{
	char	when[64];
	edict_t	*p;
	int		state;
	int		tno;
	int		playernum; /* invented */
	int		bots;

	if (!ent->client)
		return;

	if (server_log)
	{
		ngLog_getDateInfo (when, 0);
		if (!(ent->flags & FL_OSP_BOT))
			OSP_logAdminLog ("Disconnect: %s (%s)",
				ent->client->pers.netname, when);
		else
			OSP_logAdminLog ("Disconnect: %s (%s) [SERVER_BOT]",
				ent->client->pers.netname, when);
	}

	state = ent->client->resp.entered;
	if (m_mode == 3)
		OSP_1v1Remove (ent, 1);
	if (rune_stat)
		OSP_deadDropRune (ent);
	connected_clients--;

	if (state == ENTERED_ENTERED)
	{
		EntityListRemove (ent);
		tno = ent->client->resp.team;
		active_clients--;
		if (active_clients < 0)
			active_clients = 0;

		if (m_mode > 1)
		{
			if (tno != 2)
				OSP_removeTeamMember (ent, true);
			if (ent->client->resp.osp_r20c)
				OSP_notready_cmd (ent, true);
		}
	}
	else
		tno = 2;

	sl_LogPlayerDisconnect (&gi, level, ent);
	q2log_playerDisconnect (ent);
	if (!level.intermissiontime)
		q2log_logAccuracyStats (ent);

	OSP_playerAnnounce (ent, 10);
	PlayerResetGrapple (ent);
	gi.unlinkentity (ent);

	ent->s.modelindex = 0;
	ent->solid = SOLID_NOT;
	ent->svflags |= SVF_NOCLIENT;
	ent->inuse = false;
	ent->classname = "disconnected";
	ent->client->pers.connected = false;
	ent->client->menu = NULL;
	ent->client->inmenu = false;
	ent->client->osp_t00c = 0;
	ent->client->grapple = NULL;
	ent->client->chase_target = NULL;
	ent->client->update_chase = false;
	ent->client->osp_t040 = 0;
	ent->client->resp.osp_r0f4[0] = 0;

	if ((int)client_recover->value && state == ENTERED_ENTERED &&
		sync_stat > 2 && !ent->client->resp.osp_r07c[0] &&
		!level.intermissiontime && !(ent->flags & FL_OSP_BOT))
	{
		strcpy (ent->client->resp.osp_r214, ent->client->pers.netname);
		ent->client->resp.osp_r018 = level.framenum;
		OSP_saveClient (ent);
	}
	else
	{
		ent->client->resp.entered = 2;
		ent->client->resp.score = 0;
		ent->client->resp.team = 2;
		ent->client->resp.osp_r214[0] = 0;
		ent->client->resp.osp_r018 = 12345678;
		ent->client->resp.osp_r07d[0] = 0;
		ent->osp_e3a0[0] = 0;
		ent->osp_e3b0[0] = 0;
	}

	if (m_mode == 3 && (int)team_nextuptime->value)
		ent->client->resp.team = 2;

	for (state = 1; state <= game.maxclients; state++)
	{
		p = g_edicts + state;

		if (!p->inuse || !p->client ||
			p->client->chase_target != ent)
			continue;

		gi.cprintf (p, PRINT_HIGH, "Target disconnected.\n");
		OSP_removeChaseCam (p);
	}

	playernum = ent - g_edicts - 1;
	gi.configstring (CS_PLAYERSKINS + playernum, "");
	OSP_DoRankSort ();

	if (sync_stat > 2 && !level.intermissiontime &&
		!(ent->flags & FL_OSP_BOT) && (int)client_recover->value &&
		(int)team_duelrecover->value && (int)team_recovertime->value &&
		m_mode > 1 && tno != 2 && !ent->client->resp.osp_r07c[0] &&
		!OSP_teamCount (tno) && OSP_teamCount (1 - tno))
	{
		char	message[64];

		who_paused = -2;
		strncpy (reconn_player, ent->client->pers.netname, 15);
		reconn_player[15] = 0;
		reconn_index = tno;
		pause_time = team_recovertime->value;
		match_paused = 1;
		sprintf (message, "Waiting for %s to reconnect.\n(%d seconds)\n",
			reconn_player, (int)pause_time);

		for (state = 1; state <= game.maxclients; state++)
		{
			p = g_edicts + state;

			if (!p->inuse || !p->client || p == ent ||
				(p->flags & FL_OSP_BOT))
				continue;

			gi.centerprintf (p, message);
		}
		return;
	}

	if (vote_inprogress && !level.intermissiontime &&
		!(ent->flags & FL_OSP_BOT))
	{
		if (vote_item == 0x1000 &&
			ent->client->resp.clientid == atoi (vote_value))
		{
			gi.bprintf (PRINT_HIGH,
				"%s left on own accord.  Vote terminated.\n",
				ent->client->pers.netname);
			q2log_voteInfo ("Fail", "Player manually left", NULL);
			OSP_clearVotes ();
			OSP_closeMenus ();
		}
		else
			OSP_checkVote ();
	}

	connected_clients = 0;
	active_clients = 0;
	bots = 0;
	for (state = 1; state <= game.maxclients; state++)
	{
		p = g_edicts + state;

		if (p->inuse && p->client && p->client->pers.connected)
		{
			connected_clients++;
			if (p->client->resp.entered == ENTERED_ENTERED)
				active_clients++;
			if (p->flags & FL_OSP_BOT)
				bots++;
		}
	}
	botglobals.numbots = bots;
	if (bots_votedin > bots)
		bots_votedin = 0;

	gi.bprintf (PRINT_HIGH, "%s wimped out and left. (clients = %i)\n",
		ent->client->pers.netname, active_clients);
	strcpy (ent->client->pers.netname, "");
	Info_SetValueForKey (ent->client->pers.userinfo, "skin", "");
	BotLib_BotClientSettings (ent);

	if (m_mode > 1)
		OSP_checkHalt (tno);
	else if (m_mode == 1)
		OSP_checkHalt (2);

	if (!(ent->flags & FL_OSP_BOT) &&
		!(active_clients - botglobals.numbots) &&
		!(int)bots_noclients->value && bots_votedin)
	{
		for (state = 0; state < bots_votedin; state++)
			BotServerCommand ("sv", "removebot", NULL);
		bots_votedin = 0;
	}
}


//==============================================================


edict_t	*pm_passent;

// pmove doesn't need to know about passent and contentmask
// gamex86.dll: 10057219..100572A7
// gamei386.so: 0003D180..0003D1DE
trace_t	PM_trace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	if (pm_passent->health > 0)
		return gi.trace (start, mins, maxs, end, pm_passent, MASK_PLAYERSOLID);
	else
		return gi.trace (start, mins, maxs, end, pm_passent, MASK_DEADSOLID);
}

// gamex86.dll: 100572A7..100572E9
// gamei386.so: 0003D1E0..0003D208
unsigned CheckBlock (void *b, int c)
{
	int	v,i;
	v = 0;
	for (i=0 ; i<c ; i++)
		v+= ((byte *)b)[i];
	return v;
}
// gamex86.dll: 100572E9..10057336
// gamei386.so: 0003D208..0003D265
void PrintPmove (pmove_t *pm)
{
	unsigned	c1, c2;

	c1 = CheckBlock (&pm->s, sizeof(pm->s));
	c2 = CheckBlock (&pm->cmd, sizeof(pm->cmd));
	Com_Printf ("sv %3i:%i %i\n", pm->cmd.impulse, c1, c2);
}

/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame.
==============
*/
// gamex86.dll: 10057336..10058C90
// gamei386.so: 0003D268..0003EC50
void ClientThink (edict_t *ent, usercmd_t *ucmd)
{
	gclient_t	*client;
	edict_t	*other;
	int		i, j;
	pmove_t	pm;

	if (match_paused < 2)
	{
		if (ent->flags & FL_BOT)
		{
			if (ent->client->resp.entered != ENTERED_ENTERED)
				OSP_startObserve (ent);
			else if (sync_stat < 4 && !ent->client->resp.osp_r20c &&
				(int)bots_warmuptime->value &&
				ent->client->resp.enterframe +
				(int)bots_warmuptime->value * 10 < level.framenum)
				OSP_ready_cmd (ent, 2);
		}
		if (!(ent->flags & FL_BOTINPUT))
			return;

		level.current_entity = ent;
		client = ent->client;

		if (level.intermissiontime)
		{
			char	intermission_command[52];	/* invented local name */

			if (!(client->resp.osp_r01c & 0x10) &&
				level.time > level.intermissiontime + 2.0 &&
				!(ent->flags & FL_BOT))
			{
				if (client->resp.osp_r234)
				{
					gi.WriteByte (svc_stufftext);
					strcpy (intermission_command, "stop\n");
					if ((ent->osp_e39c == 1 &&
						(int)demo_referee->value > 1) ||
						(int)demo_player->value > 1)
						strcpy (intermission_command, "stop; screenshot\n");
					gi.WriteString (intermission_command);
					gi.unicast (ent, true);
				}

				if (m_mode < 2)
				{
					if (!match_endmusic || !match_endmusic->string[0] ||
						!strcmp (match_endmusic->string, "default"))
					{
						i = rand () % 5;
						sprintf (intermission_command, "play %s\n",
							wav_file + i * 25);
					}
					else
						sprintf (intermission_command, "play %s\n",
							match_endmusic->string);
				}
				else if (client->resp.team == 2 ||
					teams[client->resp.team].osp_m124 == 2)
					sprintf (intermission_command, "play makron/laf4.wav\n");
				else
					sprintf (intermission_command, "play world/xian1.wav\n");

				gi.WriteByte (svc_stufftext);
				gi.WriteString (intermission_command);
				gi.unicast (ent, false);
				client->resp.osp_r01c |= 0x10;
				PlayerResetGrapple (ent);
				if (client->resp.entered == ENTERED_ENTERED)
					OSP_accuracyInfo (ent, client->pers.netname,
						client->resp.clientid);
				OSP_closeMenus ();
				client->resp.osp_r210 = 0;
			}

			if (client->resp.osp_r2dc == 2 &&
				level.time > level.intermissiontime + 1.25 &&
				!(ent->flags & FL_BOT))
			{
				client->resp.osp_r2dc = 0;
				DeathmatchScoreboard (ent);
			}

			client->ps.pmove.pm_type = PM_FREEZE;

			if ((level.time > level.intermissiontime + nextlevel_click->value &&
				 (ucmd->buttons & BUTTON_ANY) &&
				 (int)nextlevel_click->value) ||
				((sync_stat < 4 || manual_map) &&
				 level.time > level.intermissiontime + 7.0 &&
				 (ucmd->buttons & BUTTON_ANY)))
			{
				level.exitintermission = true;
				start_count = 0;
			}

			if ((level.time > level.intermissiontime + nextlevel_lazy->value &&
				 (int)nextlevel_lazy->value) ||
				((sync_stat < 4 || manual_map) &&
				 level.time > level.intermissiontime + 15.0))
			{
				level.exitintermission = true;
				start_count = 0;
			}
			return;
		}

		if (client->resp.osp_r2dc == 1 &&
			client->resp.entered == ENTERED_ENTERED &&
			level.time > client->respawn_time + 0.5)
		{
			OSP_clearStats (ent);
			client->resp.osp_r2dc = 0;
			ent->client->showscores = false;
			Cmd_Score_f (ent);
		}

		if (ent->client->osp_t040)
		{
			if (!active_clients)
			{
				gi.cprintf (ent, PRINT_HIGH,
					"No clients to track. Switching to OBSERVE mode.\n");
				OSP_startObserve (ent);
				return;
			}

			client->oldbuttons = client->buttons;
			client->buttons = ucmd->buttons;
			client->latched_buttons = client->buttons & ~client->oldbuttons;

			if ((client->latched_buttons & BUTTON_ATTACK) &&
				client->resp.osp_r010 <= level.framenum)
			{
				if (client->menu)
					Cmd_InvUse_f (ent);
				else
				{
					client->resp.score = client->resp.osp_r248;
					OSP_ChaseCam (ent);
					if (client->chase_target)
					{
						if (sync_stat > 2 && m_mode < 2)
							client->ps.stats[20] = 0x624;
						gi.cprintf (ent, PRINT_HIGH,
							"Changing to CHASECAM mode.\n");
					}
					else
						client->resp.score = -100;
				}
				client->resp.osp_r010 = level.framenum + 2;
				return;
			}

			if (ucmd->upmove && !client->inmenu &&
				client->resp.osp_r010 <= level.framenum)
			{
				if (!client->menu)
				{
					if (client->osp_t038 == 1)
					{
						gi.cprintf (ent, PRINT_HIGH,
							"Switching to Autocam FOLLOW mode.\n");
						client->osp_t038 = 0;
					}
					else
					{
						gi.cprintf (ent, PRINT_HIGH,
							"Switching to Autocam NORMAL mode.\n");
						client->osp_t038 = 1;
					}
				}
				else
					Cmd_InvUse_f (ent);
				client->resp.osp_r010 = level.framenum + 8;
			}
			CameraThink (ent, ucmd);
			return;
		}

		pm_passent = ent;

		if (client->chase_target) {

			client->resp.cmd_angles[0] = SHORT2ANGLE(ucmd->angles[0]);
			client->resp.cmd_angles[1] = SHORT2ANGLE(ucmd->angles[1]);
			client->resp.cmd_angles[2] = SHORT2ANGLE(ucmd->angles[2]);

			if (ucmd->forwardmove < 0)
				ent->speed += 1.0f;
			else if (ucmd->forwardmove > 0)
			{
				// A POST-DECREMENT in the test, which is what lets /Od compare
				// the old value before performing the decrement and reload
				// `speed` twice with no temp at all -- exactly real's
				// fld/fcomp/fnstsw, subtract, test/je sequence.
				if (ent->speed-- == 0)
					ent->speed = 0;
			}

			/* osp_t018 is accessed as a float by the target at this site. */
			if (ucmd->sidemove > 0)
				*(float *)&client->osp_t018 =
					(float)((int)(*(float *)&client->osp_t018 + 4.0f) % 360);
			else if (ucmd->sidemove < 0)
				*(float *)&client->osp_t018 =
					(float)((int)(*(float *)&client->osp_t018 - 4.0f) % 360);

			client->oldbuttons = client->buttons;
			client->buttons = ucmd->buttons;
			client->latched_buttons = client->buttons & ~client->oldbuttons;

			if ((client->latched_buttons & BUTTON_ATTACK) &&
				client->resp.osp_r010 <= level.framenum)
			{
				if (client->menu)
					Cmd_InvUse_f (ent);
				else if (client->resp.entered == 4)
				{
					gi.cprintf (ent, PRINT_HIGH, "Changing to IN-EYES mode.\n");
					client->resp.entered = 8;
				}
				else
					OSP_removeChaseCam (ent);
				client->resp.osp_r010 = level.framenum + 2;
				return;
			}

			if (ucmd->upmove && !client->inmenu &&
				client->resp.osp_r010 <= level.framenum)
			{
				if (!client->menu)
					ChaseNext (ent);
				else
					Cmd_InvUse_f (ent);
				client->resp.osp_r010 = level.framenum + 8;
			}
			return;

		} else {

			// set up for pmove
			memset (&pm, 0, sizeof(pm));

			if (ent->movetype == MOVETYPE_NOCLIP)
				client->ps.pmove.pm_type = PM_SPECTATOR;
			else if (ent->s.modelindex != 255)
				client->ps.pmove.pm_type = PM_GIB;
			else if (ent->deadflag)
				client->ps.pmove.pm_type = PM_DEAD;
			else
				client->ps.pmove.pm_type = PM_NORMAL;

			if (!client->resp.osp_r240 &&
				client->resp.entered == ENTERED_ENTERED)
				client->ps.pmove.pm_type = PM_FREEZE;

			client->ps.pmove.gravity = sv_gravity->value;
			pm.s = client->ps.pmove;

			for (i=0 ; i<3 ; i++)
			{
				pm.s.origin[i] = ent->s.origin[i]*8;
				pm.s.velocity[i] = ent->velocity[i]*8;
			}

			if (memcmp(&client->old_pmove, &pm.s, sizeof(pm.s)))
			{
				pm.snapinitial = true;
		//		gi.dprintf ("pmove changed!\n");
			}

			if (bot_watch && !(ent->flags & FL_OSP_NOCMD) &&
				OSP_botDetect (ent, ucmd))
				return;

			pm.cmd = *ucmd;

			pm.trace = PM_trace;	// adds default parms
			pm.pointcontents = gi.pointcontents;

			// perform a pmove
			gi.Pmove (&pm);

			// save results of pmove
			client->ps.pmove = pm.s;
			client->old_pmove = pm.s;

			for (i=0 ; i<3 ; i++)
			{
				ent->s.origin[i] = pm.s.origin[i]*0.125;
				ent->velocity[i] = pm.s.velocity[i]*0.125;
			}

			VectorCopy (pm.mins, ent->mins);
			VectorCopy (pm.maxs, ent->maxs);

			client->resp.cmd_angles[0] = SHORT2ANGLE(ucmd->angles[0]);
			client->resp.cmd_angles[1] = SHORT2ANGLE(ucmd->angles[1]);
			client->resp.cmd_angles[2] = SHORT2ANGLE(ucmd->angles[2]);

			if (ent->groundentity && !pm.groundentity && (pm.cmd.upmove >= 10) && (pm.waterlevel == 0))
			{
				gi.sound(ent, CHAN_VOICE, gi.soundindex("*jump1.wav"), 1, ATTN_NORM, 0);
				PlayerNoise(ent, ent->s.origin, PNOISE_SELF);
			}

			ent->viewheight = pm.viewheight;
			ent->waterlevel = pm.waterlevel;
			ent->watertype = pm.watertype;
			ent->groundentity = pm.groundentity;
			if (pm.groundentity)
				ent->groundentity_linkcount = pm.groundentity->linkcount;

			if (ent->deadflag)
			{
				client->ps.viewangles[ROLL] = 40;
				client->ps.viewangles[PITCH] = -15;
				client->ps.viewangles[YAW] = client->killer_yaw;
			}
			else
			{
				VectorCopy (pm.viewangles, client->v_angle);
				VectorCopy (pm.viewangles, client->ps.viewangles);
			}

			if (client->grapple)
				GrapplePull (client->grapple);

			gi.linkentity (ent);

			if (ent->movetype != MOVETYPE_NOCLIP)
				G_TouchTriggers (ent);

			// touch other objects
			for (i=0 ; i<pm.numtouch ; i++)
			{
				other = pm.touchents[i];
				for (j=0 ; j<i ; j++)
					if (pm.touchents[j] == other)
						break;
				if (j != i)
					continue;	// duplicated
				if (!other->touch)
					continue;
				other->touch (other, ent, NULL, NULL);
			}

		}

		client->oldbuttons = client->buttons;
		client->buttons = ucmd->buttons;
		client->latched_buttons |= client->buttons & ~client->oldbuttons;

		// save light level the player is standing on for
		// monster sighting AI
		ent->light_level = ucmd->lightlevel;

		if (!client->resp.osp_r240 &&
			client->resp.entered == ENTERED_ENTERED)
			respawn (ent);

		{
			if (client->resp.osp_r1fc >= 0 &&
				client->resp.osp_r1fc < level.framenum)
			{
				client->resp.osp_r1f4++;
				client->resp.osp_r1f8 += client->ping;
				client->resp.osp_r1fc = level.framenum + 30;

				if (client->resp.osp_r1f4 > 15)
				{
					i = client->resp.osp_r1f8 /
						client->resp.osp_r1f4;
					client->resp.osp_r1f4 = 0;
					client->resp.osp_r1f8 = 0;

					if ((int)client_minping->value &&
						i < (int)client_minping->value)
					{
						gi.cprintf (ent, PRINT_HIGH,
							"Minimum allowed server ping %d, yours is %d.\n",
							(int)client_minping->value, i);
						gi.WriteByte (svc_disconnect);
						gi.unicast (ent, true);
						ClientDisconnect (ent);
						return;
					}

					if ((int)client_maxping->value &&
						i > (int)client_maxping->value)
					{
						gi.cprintf (ent, PRINT_HIGH,
							"Maximum allowed server ping %d, yours is %d.\n",
							(int)client_maxping->value, i);
						gi.WriteByte (svc_disconnect);
						gi.unicast (ent, true);
						ClientDisconnect (ent);
						return;
					}
				}
			}
		}

		{
				// A declared variable, not a CSE temp -- real stores to a slot
				// distinct from the field in each arm and only then to the field.
			int		inactive_seconds;	/* invented local name */

			if ((int)client_nomove->value &&
				client->resp.entered == ENTERED_ENTERED &&
				m_mode > 1 && sync_stat < 4 &&
				client->resp.osp_r0d4 < level.framenum &&
				!(ent->flags & FL_OSP_BOT))
			{
				client->resp.osp_r0d4 = level.framenum + 60;

				if (!VectorCompare ((vec_t *)&client->resp.osp_r0dc,
					ent->s.angles) ||
					!VectorCompare ((vec_t *)&client->resp.osp_r0e8,
					ent->s.origin))
				{
					VectorCopy (ent->s.angles,
						((vec_t *)&client->resp.osp_r0dc));
					VectorCopy (ent->s.origin,
						((vec_t *)&client->resp.osp_r0e8));
					inactive_seconds = 0;
				}
				else
					inactive_seconds = client->resp.osp_r0d8 + 6;

				client->resp.osp_r0d8 = inactive_seconds;
				if (inactive_seconds >= (int)client_nomove->value)
				{
					gi.bprintf (PRINT_CHAT,
						"%s inactive for %d seconds, moved to OBSERVER mode.\n",
						client->pers.netname, inactive_seconds);
					OSP_startObserve (ent);
					return;
				}
			}
		}

		{
			char	maxfps_command[64];	/* invented local name */

			if (client->resp.osp_r024 >= 0 &&
				level.framenum > client->resp.osp_r024 &&
				ucmd->msec < client_maxframes - 2)
			{
				if (!client->resp.osp_r024)
					gi.cprintf (ent, PRINT_HIGH,
						"*** Server cl_maxfps capped at %d\n",
						(int)client_maxfps->value);

				client->resp.osp_r024 = level.framenum + 60;
				gi.WriteByte (svc_stufftext);
				sprintf (maxfps_command, "cl_maxfps %d\n",
					(int)client_maxfps->value);
				gi.WriteString (maxfps_command);
				gi.unicast (ent, false);
			}
		}

		// fire weapon from final position if needed
		if (client->latched_buttons & BUTTON_ATTACK)
		{
			if (!client->weapon_thunk && client->resp.osp_r240 == 2 &&
				client->resp.entered == ENTERED_ENTERED)
			{
				if (client->respawn_time + 0.2 < level.time)
					client->resp.osp_r23c = 0;
				if (bot_watch && ent->client->resp.osp_r008)
				{
					OnBotDetection (ent, "cr");
					return;
				}
				client->weapon_thunk = true;
				Think_Weapon (ent);
			}

			if (client->resp.entered == 2 &&
				client->resp.osp_r010 <= level.framenum)
			{
				if (sync_stat != 4 && !client->resp.osp_r02c)
				{
					if (client->menu)
						Cmd_InvUse_f (ent);
					else
					{
						client->resp.osp_r02c = 1;
						if (m_mode > 1)
							OSP_teamMenu (ent);
						else if (!client->resp.osp_r030 && m_mode < 2)
							OSP_startObserve (ent);
						else
							OSP_DMMenu (ent);
					}
				}
				else
				{
					if (client->menu)
						Cmd_InvUse_f (ent);
					else
					{
						if (active_clients)
						{
							client->resp.score = client->resp.osp_r248;
							CameraCmd (ent, false);
							gi.cprintf (ent, PRINT_HIGH,
								"Changing to AUTOCAM mode.\n");
							client->resp.osp_r010 = level.framenum + 8;
							return;
						}
						gi.cprintf (ent, PRINT_HIGH, "No clients to track.\n");
						client->resp.osp_r010 = level.framenum + 8;
						return;
					}
				}
				client->resp.osp_r010 = level.framenum + 2;
			}
		}

		// update chase cam if being followed
		if (client->resp.entered == ENTERED_ENTERED &&
			client->resp.osp_r000 && !level.intermissiontime)
		{
			j = 0;
			for (i = 1; i <= game.maxclients &&
				 j < client->resp.osp_r000; i++) {
				other = g_edicts + i;
				if (other->inuse && other->client &&
					other->client->chase_target == ent)
				{
					j++;
					UpdateChaseCam(other);
				}
			}
			client->resp.osp_r000 = j;
		}

		if (rune_stat & RUNE_REGEN)
			OSP_runesApplyRegeneration (ent);
	}
	else
	{
		client = ent->client;
		if (who_paused != -3)
			return;

		if (!Q_stricmp (ent->osp_e37c, "loopback"))
		{
			client->oldbuttons = client->buttons;
			client->buttons = ucmd->buttons;
			client->latched_buttons = client->buttons & ~client->oldbuttons;

			if (client->latched_buttons & BUTTON_ATTACK)
			{
				match_paused = 3;
				client->latched_buttons = 0;
				gi.bprintf (PRINT_CHAT, "Admin has returned!\n");
				gi.bprintf (PRINT_CHAT, "Admin has returned!\n");
				gi.bprintf (PRINT_CHAT, "Admin has returned!\n");
				if ((int)nglog_ngstats_vidrestart->value)
					gi.AddCommandString ("vid_restart\n");
			}
		}
	}
}


/*
==============
ClientBeginServerFrame

This will be called once for each server frame, before running
any other entities in the world.
==============
*/
// gamex86.dll: 10058C90..10058E30
// gamei386.so: 0003EC50..0003EE0E
void ClientBeginServerFrame (edict_t *ent)
{
	gclient_t	*client;
	int			buttonMask;

	if (level.intermissiontime)
		return;

	if (ent->client->osp_t040)
		return;

	client = ent->client;

	// run weapon animations if it hasn't been done by a ucmd_t.
	// The second conjunct re-reads ent->client rather than using the local
	// just assigned -- real loads it again (`mov eax,[edx+0x54]`).
	if (!client->weapon_thunk && ent->client->resp.osp_r240 == 2)
		Think_Weapon (ent);
	else
		client->weapon_thunk = false;

	if (ent->deadflag)
	{
		// wait for any button just going down
		if ( level.time > client->respawn_time)
		{
			// in deathmatch, only wait for attack button
			if (deathmatch->value)
				buttonMask = BUTTON_ATTACK;
			else
				buttonMask = -1;

			if ( ( client->latched_buttons & buttonMask ) ||
				(deathmatch->value && ((int)dmflags->value & DF_FORCE_RESPAWN) &&
				level.time > client->respawn_time + resp_delay->value) )
			{
				respawn (ent);
				client->latched_buttons = 0;
			}
		}
		return;
	}

	client->latched_buttons = 0;

	if (client->resp.osp_r2b4 == level.framenum &&
		client->resp.osp_r2b8 != 16 &&
		client->resp.entered == ENTERED_ENTERED &&
		bot_watch &&
		!(ent->flags & FL_OSP_BOT))
		OSP_speedDetect (ent);
}
