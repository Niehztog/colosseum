// stdlog.c -- <INVENTED FILENAME>. Standard Log 1.2 writer.

#include "g_local.h"

cvar_t	*sl_log_logbots;
int		sl_ngloglog_status;
cvar_t	*sl_log_style;
cvar_t	*sl_filename;
cvar_t	*sl_log_flush;
cvar_t	*sl_log_method;

static int		sl_started = 0;
static char		*sl_patch = NULL;

// gamex86.dll: 1004B7C0..1004B817
// gamei386.so: 00073338..0007339D
int sl_Logging (game_import_t *import, char *patch)
{
	int	status;

	status = sl_OpenLogFile (import);
	sl_log_logbots = import->cvar ("sl_log_logbots", "1", 0);

	if (status && !sl_started)
	{
		sl_patch = patch;
		sl_started = status;
	}

	return status;
}

// gamex86.dll: 1004B817..1004B8C3
// gamei386.so: 000733A0..0007348A
void sl_GameStart (game_import_t *import, level_locals_t level)
{
	// No `patch` local anywhere in this family: every one of these functions
	// passes `sl_patch` straight into sl_Logging and wraps its whole body in
	// `if (sl_Logging (...))`.
	cvar_t		*flags;

	if (sl_Logging (import, sl_patch))
	{
		flags = import->cvar ("dmflags", "0", CVAR_SERVERINFO);
		sl_LogVers (import);
		sl_LogPatch (import, sl_patch);
		sl_LogDate (import);
		sl_LogTime (import);
		sl_LogDeathFlags (import, (unsigned long)flags->value);
		sl_LogMapName (import, level.level_name);
		sl_LogGameStart (import, level.time);
	}
}

// gamex86.dll: 1004B8C3..1004B8FD
// gamei386.so: 0007348C..0007351B
void sl_GameEnd (game_import_t *import, level_locals_t level)
{
	if (sl_Logging (import, sl_patch))
	{
		sl_LogGameEnd (import, level.time);
		sl_CloseLogFile ();
		sl_started = 0;
	}
}

// gamex86.dll: 1004B8FD..1004BD5E
// gamei386.so: 0007351C..00073958
void sl_WriteStdLogDeath (game_import_t *import, level_locals_t level,
	edict_t *targ, edict_t *inflictor, edict_t *attacker)
{
	int		mod;
	char	*victim;
	char	*killer;
	char	*event;
	char	*weapon;
	int		score;
	int		ping;
	int		suicide;

	if (!(int)sl_log_logbots->value &&
		((targ->flags & 0x2000) || (attacker->flags & 0x2000)))
		return;

	if (deathmatch->value != 0 && sl_Logging (import, sl_patch))
	{
		mod = meansOfDeath & ~MOD_FRIENDLY_FIRE;
		victim = NULL;
		killer = NULL;
		event = NULL;
		// Real initialises `weapon` here too, between event and score.
		weapon = NULL;
		score = 0;
		ping = -1;

		if (attacker == targ)
		{
			victim = attacker->client->pers.netname;
			ping = attacker->client->ping;
			event = "Suicide";
			score = -1;
			// A ternary: it materialises into its own frame temp and is then
			// copied into weapon's slot.
			weapon = attacker->client->pers.weapon
				? attacker->client->pers.weapon->pickup_name : NULL;
		}
		else
		{
			suicide = 0;
			weapon = "UNKNOWN";

			switch (mod)
			{
		case MOD_FALLING:
				weapon = "Fell";
				suicide = 1;
				break;
		case MOD_CRUSH:
				weapon = "Crushed";
				suicide = 1;
				break;
		case MOD_WATER:
				weapon = "Drowned";
				suicide = 1;
				break;
		case MOD_SLIME:
				weapon = "Melted";
				suicide = 1;
				break;
		case MOD_LAVA:
				weapon = "Lava";
				suicide = 1;
				break;
		case MOD_EXPLOSIVE:
		case MOD_BARREL:
		case MOD_BOMB:
				weapon = "Explosion";
				suicide = 1;
				break;
		case MOD_TARGET_LASER:
				weapon = "Lasered";
				suicide = 1;
				break;
		case MOD_TARGET_BLASTER:
				weapon = "Blasted";
				suicide = 1;
				break;
		case MOD_SUICIDE:
		case MOD_EXIT:
		case MOD_SPLASH:
		case MOD_TRIGGER_HURT:
				suicide = 1;
				break;
			}

			if (suicide)
			{
				victim = targ->client->pers.netname;
				ping = targ->client->ping;
				event = "Suicide";
				score = -1;
			}
		}

		if (!victim || !event)
		{
			if (attacker && attacker->client)
			{
				weapon = "UNKNOWN";
				switch (mod)
				{
			case MOD_BLASTER:
					weapon = "Blaster";
					break;
			case MOD_SHOTGUN:
					weapon = "Shotgun";
					break;
			case MOD_SSHOTGUN:
					weapon = "Super Shotgun";
					break;
			case MOD_MACHINEGUN:
					weapon = "Machinegun";
					break;
			case MOD_CHAINGUN:
					weapon = "Chaingun";
					break;
			case MOD_GRENADE:
			case MOD_G_SPLASH:
					weapon = "Grenade Launcher";
					break;
			case MOD_HANDGRENADE:
			case MOD_HG_SPLASH:
			case MOD_HELD_GRENADE:
					weapon = "Grenades";
					break;
			case MOD_ROCKET:
			case MOD_R_SPLASH:
					weapon = "Rocket Launcher";
					break;
			case MOD_HYPERBLASTER:
					weapon = "HyperBlaster";
					break;
			case MOD_RAILGUN:
					weapon = "Railgun";
					break;
			case MOD_BFG_LASER:
			case MOD_BFG_BLAST:
			case MOD_BFG_EFFECT:
					weapon = "BFG10K";
					break;
			case MOD_GRAPPLE:
					weapon = "Grappling Hook";
					break;
			case MOD_TELEFRAG:
					weapon = "Telefrag";
					break;
				}

				killer = targ->client->pers.netname;
				victim = attacker->client->pers.netname;
				ping = attacker->client->ping;
				event = "Kill";
				score = 1;

				if (m_mode == 2 &&
					attacker->client->resp.team == targ->client->resp.team)
					score = -1;
			}
		}

		sl_LogScore (import, victim, killer, event, weapon, score, level.time,
			ping);
		return;
	}

	sl_LogScore (import, "", "", "ERROR", "", 0, level.time, -1);
}

// gamex86.dll: 1004BD5E..1004BDC3
// gamei386.so: 00073958..00073A1F
void sl_WriteStdLogPlayerEntered (game_import_t *import, level_locals_t level,
	edict_t *ent)
{
	if (sl_Logging (import, sl_patch))
	{
		if ((int)sl_log_logbots->value || !(ent->flags & 0x2000))
			sl_LogPlayerConnect (import, ent->client->pers.netname, 0,
								 level.time);
	}
}

// gamex86.dll: 1004BDC3..1004BE26
// gamei386.so: 00073A20..00073AE5
void sl_LogPlayerDisconnect (game_import_t *import, level_locals_t level,
	edict_t *ent)
{
	if (sl_Logging (import, sl_patch))
	{
		if ((int)sl_log_logbots->value || !(ent->flags & 0x10000))
			sl_LogPlayerLeft (import, ent->client->pers.netname, level.time);
	}
}

// gamex86.dll: 1004BE26..1004BE60
// gamei386.so: 00073AE8..00073B68
void sl_SoftGameEnd (game_import_t *import, level_locals_t level)
{
	if (sl_Logging (import, sl_patch))
		sl_LogGameEnd (import, level.time);
}
