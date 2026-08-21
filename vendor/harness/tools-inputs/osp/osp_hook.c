// osp_hook.c -- <INVENTED FILENAME>. The mod's grappling hook.
//
// id's Quake II CTF `g_ctf.c` grapple section with the `CTF` prefix stripped,
// plus two of the mod's own, OSP_hookAliases and G_Spawn_Sparks.
//
// FireGrapple, GrappleFire, GrappleTouch and GrapplePull are substantially
// REWRITTEN, not just renamed: new `hook_*` cvars drive the skin colour
// (team colour in team play, `hook_color` otherwise), the sky-block test,
// the hold timers, the vampire-hook damage-over-time and its cap, and the
// pull speed; the state machine gates on `sync_stat`/`match_paused`/
// `resp.entered` rather than a weapon-classname check; `CheckTeamDamage` is
// gone; every CTF sound file is replaced (`flyer/flyatck2.wav` on grab,
// `flyer/flyatck3.wav` on fire, none on pull); and `SV_AddGravity` is no
// longer called from the pull.  CTF's `GrappleDrawCable` is dead code here
// and is dropped entirely.

#include "g_local.h"

// gamex86.dll: 1002C9C0..1002CA90
// gamei386.so: 0005BD98..0005BE8A
void OSP_hookon_cmd (edict_t *ent)
{
	if (ent->client->resp.entered != ENTERED_ENTERED ||
		ent->client->resp.osp_r2dc ||
		level.intermissiontime ||
		sync_stat == 2 || sync_stat == 1 ||
		(ent->client && ent->client->grapple) ||
		match_paused ||
		level.time - ent->client->osp_t00c < hook_wait->value)
		return;

	if (!(int)hook_enable->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "Hook is currently disabled.\n");
		return;
	}

	ent->client->resp.osp_r23c = 0;
	Grapple_Fire (ent);
}

// gamex86.dll: 1002CA90..1002CAA1
// gamei386.so: 0005BE8C..0005BEA9
void OSP_hookoff_cmd (edict_t *ent)
{
	PlayerResetGrapple (ent);
}


// ent is player
// gamex86.dll: 1002CAA1..1002CB19
// gamei386.so: 0005BEAC..0005BF11
void PlayerResetGrapple (edict_t *ent)
{
	if (ent->client && ent->client->grapple)
	{
		ResetGrapple (ent->client->grapple);
		ent->client->grapple = NULL;
		ent->client->grapplereleasetime = level.time;
		ent->client->grapplestate = CTF_GRAPPLE_STATE_FLY;
		ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
	}
}

// self is grapple, not player
// gamex86.dll: 1002CB19..1002CBD0
// gamei386.so: 0005BF14..0005BF97
void ResetGrapple (edict_t *self)
{
	if (self->inuse)
	{
		if (self->owner && self->owner->client && self->owner->client->grapple) {
			self->owner->client->grapple = NULL;
			self->owner->client->grapplereleasetime = level.time;
			self->owner->client->grapplestate = CTF_GRAPPLE_STATE_FLY;	// firing, not on hook
			self->owner->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
		}
		G_FreeEdict (self);
	}
}

// gamex86.dll: 1002CBD0..1002CE5C
// gamei386.so: 0005BF98..0005C27E
void GrappleTouch (edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	float volume = 1.0;

	if (other == self->owner)
		return;

	if (self->owner->client->grapplestate != CTF_GRAPPLE_STATE_FLY)
		return;

	if ((surf && (surf->flags & SURF_SKY) && (int)hook_sky->value == 0) ||
		self->owner->client->grapple != self)
	{
		ResetGrapple (self);
		return;
	}

	VectorCopy (vec3_origin, self->velocity);

	PlayerNoise (self->owner, self->s.origin, PNOISE_IMPACT);

	if (other->takedamage) {
		G_Spawn_Sparks (TE_BLOOD, self->s.origin, plane->normal, self->s.origin);

		if (self->dmg) {
			T_Damage (other, self, self->owner, self->velocity, self->s.origin, plane->normal, self->dmg, 1, 0, MOD_GRAPPLE);
			self->health = self->dmg;
			self->count = level.framenum + (int)(10.0 * hook_holdplayertime->value);
		}
		else
		{
			ResetGrapple (self);
			return;
		}
	}
	else
	{
		self->count = level.framenum + (int)(10.0 * hook_holdtime->value);
	}

	self->owner->client->grapplestate = CTF_GRAPPLE_STATE_PULL;	// we're on hook
	self->enemy = other;

	self->solid = SOLID_NOT;

	if (self->owner->client->silencer_shots)
		volume = 0.2;

	gi.sound (self->owner, CHAN_BODY+CHAN_WEAPON, gi.soundindex("flyer/flyatck2.wav"), volume, ATTN_NORM, 0);

	gi.WriteByte (svc_temp_entity);
	gi.WriteByte (TE_SPARKS);
	gi.WritePosition (self->s.origin);
	if (!plane)
		gi.WriteDir (vec3_origin);
	else
		gi.WriteDir (plane->normal);
	gi.multicast (self->s.origin, MULTICAST_PVS);
}

// pull the player toward the grapple
// gamex86.dll: 1002CE5C..1002D333
// gamei386.so: 0005C280..0005C6E9
void GrapplePull (edict_t *self)
{
	vec3_t hookdir, v;
	float vlen;

	// Note the order of the two sync_stat tests -- 2 before 1.
	if (!self->owner ||
		self->owner->deadflag ||
		!self->owner->client ||
		(level.framenum > self->count &&
		 (self->owner->client->grapplestate == CTF_GRAPPLE_STATE_HANG ||
		  self->owner->client->grapplestate == CTF_GRAPPLE_STATE_PULL)) ||
		level.intermissiontime ||
		self->owner->client->grapple != self ||
		self->owner->client->resp.entered != ENTERED_ENTERED ||
		self->owner->client->resp.osp_r2dc ||
		self->owner->s.event == EV_PLAYER_TELEPORT ||
		sync_stat == 2 || sync_stat == 1 ||
		match_paused)
	{
		ResetGrapple (self);
		return;
	}

	if (self->enemy) {
		if (self->enemy->solid == SOLID_NOT) {
			ResetGrapple (self);
			return;
		}
		if (self->enemy->solid == SOLID_BBOX) {
			VectorScale (self->enemy->size, 0.5, v);
			VectorAdd (v, self->enemy->s.origin, v);
			VectorAdd (v, self->enemy->mins, self->s.origin);
			gi.linkentity (self);
		} else
			VectorCopy (self->enemy->velocity, self->velocity);

		if (self->enemy->takedamage) {
			// Dead in the original: written, overwritten and never read.
			// Name invented.
			float damagescale = 1.0;

			if (self->owner->client->silencer_shots)
				damagescale = 0.2;

			if (self->health < (int)hook_maxdamage->value) {
				T_Damage (self->enemy, self, self->owner, self->velocity, self->s.origin, vec3_origin, (int)hook_incdamage->value, 1, 0, MOD_GRAPPLE);
				self->health += (int)hook_incdamage->value;
			}
		}

		if (self->enemy->deadflag) // he died
		{
			ResetGrapple (self);
			return;
		}
	}

	if (self->owner->client->grapplestate > CTF_GRAPPLE_STATE_FLY) {
		// pull player toward grapple
		vec3_t forward, up;

		AngleVectors (self->owner->client->v_angle, forward, NULL, up);
		VectorCopy (self->owner->s.origin, v);
		v[2] += self->owner->viewheight;
		VectorSubtract (self->s.origin, v, hookdir);

		vlen = VectorLength (hookdir);

		if (self->owner->client->grapplestate == CTF_GRAPPLE_STATE_PULL &&
			vlen < 64.0f) {
			self->owner->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
			self->owner->client->grapplestate = CTF_GRAPPLE_STATE_HANG;
		}

		VectorNormalize (hookdir);
		if (self->owner->client->grapplestate != CTF_GRAPPLE_STATE_HANG)
			VectorScale (hookdir, (int)hook_pullspeed->value, hookdir);
		else
			VectorScale (hookdir, 425.0f, hookdir);
		VectorCopy (hookdir, self->owner->velocity);
	}
}

// gamex86.dll: 1002D333..1002D5A4
// gamei386.so: 0005C6EC..0005C8DA
void FireGrapple (edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int effect)
{
	edict_t	*grapple;
	trace_t	tr;
	char	*tailp;

	VectorNormalize (dir);

	grapple = G_Spawn();
	VectorCopy (start, grapple->s.origin);
	VectorCopy (start, grapple->s.old_origin);
	vectoangles (dir, grapple->s.angles);
	VectorScale (dir, speed, grapple->velocity);
	grapple->movetype = MOVETYPE_FLYMISSILE;
	grapple->clipmask = MASK_SHOT;
	grapple->solid = SOLID_BBOX;
	VectorClear (grapple->mins);
	VectorClear (grapple->maxs);
	grapple->owner = self;
	grapple->touch = GrappleTouch;
	grapple->dmg = damage;
	grapple->classname = "hook";
	self->client->grapple = grapple;
	self->client->grapplestate = CTF_GRAPPLE_STATE_FLY;	// we're firing, not on hook
	self->client->osp_t00c = level.time;
	grapple->s.renderfx |= RF_BEAM|RF_TRANSLUCENT;
	grapple->s.frame = 4;
	grapple->s.modelindex = 1;

	if (m_mode < 2)
		grapple->s.skinnum = strtoul (hook_color->string, &tailp, 0);
	else
		grapple->s.skinnum = strtoul (teams[self->client->resp.team].osp_m0c0, &tailp, 0);

	gi.linkentity (grapple);

	tr = gi.trace (self->s.origin, NULL, NULL, grapple->s.origin, grapple, MASK_SHOT);
	if (tr.fraction < 1.0)
	{
		VectorMA (grapple->s.origin, -10, dir, grapple->s.origin);
		grapple->touch (grapple, tr.ent, NULL, NULL);
	}
}

// gamex86.dll: 1002D5A4..1002D6EC
// gamei386.so: 0005C8DC..0005CA28
void GrappleFire (edict_t *ent, vec3_t g_offset, int damage, int effect)
{
	vec3_t	forward, right;
	vec3_t	start;
	vec3_t	offset;
	float volume = 1.0;

	if (ent->client->grapplestate > CTF_GRAPPLE_STATE_FLY)
		return;		// it's already out

	AngleVectors (ent->client->v_angle, forward, right, NULL);
	VectorSet (offset, 24, 8, ent->viewheight-8+2);
	VectorAdd (offset, g_offset, offset);
	P_ProjectSource (ent->client, ent->s.origin, offset, forward, right, start);

	VectorScale (forward, -2, ent->client->kick_origin);
	ent->client->kick_angles[0] = -1;

	if (ent->client->silencer_shots)
		volume = 0.2;

	gi.sound (ent, CHAN_BODY+CHAN_WEAPON, gi.soundindex("flyer/flyatck3.wav"), volume, ATTN_NORM, 0);
	FireGrapple (ent, start, forward, damage, (int)hook_speed->value, effect);
}

// gamex86.dll: 1002D6EC..1002D71B
// gamei386.so: 0005CA28..0005CA75
void Grapple_Fire (edict_t *ent)
{
	int		damage;

	damage = hook_initdamage->value;
	GrappleFire (ent, vec3_origin, damage, 0);
}

// Bind +hook / +grapple for the client. gi.WriteByte(svc_stufftext) plus
// gi.WriteString plus a reliable unicast, four times.
// gamex86.dll: 1002D71B..1002D7C0
// gamei386.so: 0005CA78..0005CB2A
void OSP_hookAliases (edict_t *ent)
{
	gi.WriteByte (svc_stufftext);
	gi.WriteString ("alias +hook cmd hook\n");
	gi.unicast (ent, true);
	gi.WriteByte (svc_stufftext);
	gi.WriteString ("alias -hook cmd unhook\n");
	gi.unicast (ent, true);
	gi.WriteByte (svc_stufftext);
	gi.WriteString ("alias +grapple cmd hook\n");
	gi.unicast (ent, true);
	gi.WriteByte (svc_stufftext);
	gi.WriteString ("alias -grapple cmd unhook\n");
	gi.unicast (ent, true);
}

// gamex86.dll: 1002D7C0..1002D810
// gamei386.so: 0005CB2C..0005CB88
void G_Spawn_Sparks (int type, vec3_t pos, vec3_t dir, vec3_t org)
{
	gi.WriteByte (svc_temp_entity);
	gi.WriteByte (type);
	gi.WritePosition (pos);
	gi.WriteDir (dir);
	gi.multicast (org, MULTICAST_PVS);
}
