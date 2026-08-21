#include "g_local.h"
#include "m_player.h"


// gamex86.dll: 10009780..10009815
// gamei386.so: 000153C4..00015450
char *ClientTeam (edict_t *ent)
{
	char		*p;
	static char	value[512];

	value[0] = 0;

	if (!ent->client)
		return value;

	strcpy(value, Info_ValueForKey (ent->client->pers.userinfo, "skin"));
	p = strchr(value, '/');
	if (!p)
		return value;

	if ((int)(dmflags->value) & DF_MODELTEAMS)
	{
		*p = 0;
		return value;
	}

	// if ((int)(dmflags->value) & DF_SKINTEAMS)
	return ++p;
}

// gamex86.dll: 10009815..10009897
// gamei386.so: 00015450..000155EC
qboolean OnSameTeam (edict_t *ent1, edict_t *ent2)
{
	char	ent1Team [512];
	char	ent2Team [512];

	if (!((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS)))
		return false;

	strcpy (ent1Team, ClientTeam (ent1));
	strcpy (ent2Team, ClientTeam (ent2));

	if (strcmp(ent1Team, ent2Team) == 0)
		return true;
	return false;
}


// gamex86.dll: 10009897..10009A67
// gamei386.so: 000155EC..00015777
void SelectNextItem (edict_t *ent, int itflags)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;

	cl = ent->client;

	if (cl->inmenu || (cl->chase_target && !cl->showscores))
	{
		if (cl->resp.osp_r010 <= level.framenum)
		{
			cl->resp.osp_r010 = level.framenum + 2;
			if (cl->inmenu)
			{
				PMenu_Next (ent);
				return;
			}
			ChaseNext (ent);
			return;
		}
		return;
	}

	// Two independent top-level ifs, not if/else if.
	if (m_mode)
	{
		if (level.intermissiontime)
			return;
	}
	if (!m_mode && cl->showscores && cl->resp.osp_r24c != 1
		&& cl->resp.osp_r010 <= level.framenum && active_clients)
	{
		cl->resp.osp_r010 = level.framenum + 2;
		cl->resp.osp_r2b0 = (cl->resp.osp_r2b0 + 1) % active_clients;
		DeathmatchScoreboardMessage (ent, ent->enemy);
		gi.unicast (ent, true);
	}

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (cl->pers.selected_item + i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (!(it->flags & itflags))
			continue;

		cl->pers.selected_item = index;
		return;
	}

	cl->pers.selected_item = -1;
}

// gamex86.dll: 10009A67..10009C55
// gamei386.so: 00015778..00015906
void SelectPrevItem (edict_t *ent, int itflags)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;

	cl = ent->client;

	if (cl->inmenu || cl->chase_target)
	{
		if (cl->resp.osp_r010 <= level.framenum)
		{
			cl->resp.osp_r010 = level.framenum + 2;
			if (cl->inmenu)
			{
				PMenu_Prev (ent);
				return;
			}
			ChasePrev (ent);
			return;
		}
		return;
	}

	// Two independent top-level ifs, not if/else if (see SelectNextItem).
	if (m_mode)
	{
		if (level.intermissiontime)
			return;
	}
	if (!m_mode && cl->showscores && cl->resp.osp_r24c != 1
		&& cl->resp.osp_r010 <= level.framenum && active_clients)
	{
		cl->resp.osp_r010 = level.framenum + 2;
		cl->resp.osp_r2b0 = (cl->resp.osp_r2b0 - 1) % active_clients;
		if (cl->resp.osp_r2b0 < 0)
			cl->resp.osp_r2b0 = active_clients - 1;
		DeathmatchScoreboardMessage (ent, ent->enemy);
		gi.unicast (ent, true);
	}

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (cl->pers.selected_item + MAX_ITEMS - i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (!(it->flags & itflags))
			continue;

		cl->pers.selected_item = index;
		return;
	}

	cl->pers.selected_item = -1;
}

// gamex86.dll: 10009C55..10009C8C
// gamei386.so: 00015908..0001593B
void ValidateSelectedItem (edict_t *ent)
{
	gclient_t	*cl;

	cl = ent->client;

	if (cl->pers.inventory[cl->pers.selected_item])
		return;		// valid

	SelectNextItem (ent, -1);
}


//=================================================================================

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
// gamex86.dll: 10009C8C..1000A16A
// gamei386.so: 0001593C..00015E0A
void Cmd_Give_f (edict_t *ent)
{
	char		*name;
	gitem_t		*it;
	int			index;
	int			i;
	qboolean	give_all;
	edict_t		*it_ent;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	name = gi.args();

	if (Q_stricmp(name, "all") == 0)
		give_all = true;
	else
		give_all = false;

	if (give_all || Q_stricmp(gi.argv(1), "health") == 0)
	{
		if (gi.argc() == 3)
			ent->health = atoi(gi.argv(2));
		else
			ent->health = ent->max_health;
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "weapons") == 0)
	{
		for (i=0 ; i<game.num_items ; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (!(it->flags & IT_WEAPON))
				continue;
			ent->client->pers.inventory[i] += 1;
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "ammo") == 0)
	{
		for (i=0 ; i<game.num_items ; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (!(it->flags & IT_AMMO))
				continue;
			Add_Ammo (ent, it, 1000);
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "armor") == 0)
	{
		gitem_armor_t	*info;

		it = FindItem("Jacket Armor");
		ent->client->pers.inventory[ITEM_INDEX(it)] = 0;

		it = FindItem("Combat Armor");
		ent->client->pers.inventory[ITEM_INDEX(it)] = 0;

		it = FindItem("Body Armor");
		info = (gitem_armor_t *)it->info;
		ent->client->pers.inventory[ITEM_INDEX(it)] = info->max_count;

		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "Power Shield") == 0)
	{
		it = FindItem("Power Shield");
		it_ent = G_Spawn();
		it_ent->classname = it->classname;
		SpawnItem (it_ent, it);
		Touch_Item (it_ent, ent, NULL, NULL);
		if (it_ent->inuse)
			G_FreeEdict(it_ent);

		if (!give_all)
			return;
	}

	if (give_all)
	{
		for (i=0 ; i<game.num_items ; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (it->flags & (IT_ARMOR|IT_WEAPON|IT_AMMO))
				continue;
			ent->client->pers.inventory[i] = 1;
		}
		return;
	}

	it = FindItem (name);
	if (!it)
	{
		name = gi.argv(1);
		it = FindItem (name);
		if (!it)
		{
			gi.dprintf ("unknown item\n");
			return;
		}
	}

	if (!it->pickup)
	{
		gi.dprintf ("non-pickup item\n");
		return;
	}

	index = ITEM_INDEX(it);

	if (it->flags & IT_AMMO)
	{
		if (gi.argc() == 3)
			ent->client->pers.inventory[index] = atoi(gi.argv(2));
		else
			ent->client->pers.inventory[index] += it->quantity;
	}
	else
	{
		it_ent = G_Spawn();
		it_ent->classname = it->classname;
		SpawnItem (it_ent, it);
		Touch_Item (it_ent, ent, NULL, NULL);
		if (it_ent->inuse)
			G_FreeEdict(it_ent);
	}
}


/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
// gamex86.dll: 1000A16A..1000A1FB
// gamei386.so: 00015E0C..00015E96
void Cmd_God_f (edict_t *ent)
{
	char	*msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	ent->flags ^= FL_GODMODE;
	if (!(ent->flags & FL_GODMODE) )
		msg = "godmode OFF\n";
	else
		msg = "godmode ON\n";

	gi.cprintf (ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
// gamex86.dll: 1000A1FB..1000A28C
// gamei386.so: 00015E98..00015F22
void Cmd_Notarget_f (edict_t *ent)
{
	char	*msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	ent->flags ^= FL_NOTARGET;
	if (!(ent->flags & FL_NOTARGET) )
		msg = "notarget OFF\n";
	else
		msg = "notarget ON\n";

	gi.cprintf (ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
// gamex86.dll: 1000A28C..1000A31E
// gamei386.so: 00015F24..00015FBC
void Cmd_Noclip_f (edict_t *ent)
{
	char	*msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	if (ent->movetype == MOVETYPE_NOCLIP)
	{
		ent->movetype = MOVETYPE_WALK;
		msg = "noclip OFF\n";
	}
	else
	{
		ent->movetype = MOVETYPE_NOCLIP;
		msg = "noclip ON\n";
	}

	gi.cprintf (ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Use_f

Use an inventory item
==================
*/
// gamex86.dll: 1000A31E..1000A3E4
// gamei386.so: 00015FBC..00016095
void Cmd_Use_f (edict_t *ent)
{
	int			index;
	gitem_t		*it;
	char		*s;

	if (ent->client->resp.entered != ENTERED_ENTERED)
		return;

	s = gi.args();
	it = FindItem (s);
	if (!it)
	{
		gi.cprintf (ent, PRINT_HIGH, "unknown item: %s\n", s);
		return;
	}
	if (!it->use)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not usable.\n");
		return;
	}
	index = ITEM_INDEX(it);
	if (!ent->client->pers.inventory[index])
	{
		gi.cprintf (ent, PRINT_HIGH, "Out of item: %s\n", s);
		return;
	}

	it->use (ent, it);
}


/*
==================
Cmd_Drop_f

Drop an inventory item
==================
*/
// gamex86.dll: 1000A3E4..1000A549
// gamei386.so: 00016098..00016203
void Cmd_Drop_f (edict_t *ent)
{
	int			index;
	gitem_t		*it;
	char		*s;

	if (Q_stricmp (gi.args(), "tech") == 0 || Q_stricmp (gi.args(), "rune") == 0)
	{
		it = OSP_What_Rune (ent);
		if (it)
			it->drop (ent, it);
		else
			gi.cprintf (ent, PRINT_HIGH, "No runes to drop.\n");
		return;
	}

	if (sync_stat < 4)
	{
		gi.cprintf (ent, PRINT_HIGH, "Cannot drop items during warmup!\n");
		return;
	}

	if (ent->client->resp.entered != ENTERED_ENTERED || ent->health <= 0)
		return;

	s = gi.args();
	it = FindItem (s);
	if (!it)
	{
		gi.cprintf (ent, PRINT_HIGH, "Unknown item: %s\n", s);
		return;
	}
	if (!it->drop)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not droppable.\n");
		return;
	}
	index = ITEM_INDEX(it);
	if (!ent->client->pers.inventory[index])
	{
		gi.cprintf (ent, PRINT_HIGH, "Out of item: %s\n", s);
		return;
	}

	it->drop (ent, it);
}


/*
=================
Cmd_Inven_f
=================
*/
// gamex86.dll: 1000A549..1000A5A9
// gamei386.so: 00016204..0001625F
void Cmd_Inven_f (edict_t *ent)
{
	if (ent->client->resp.osp_r010 <= level.framenum)
	{
		ent->client->resp.osp_r010 = level.framenum + 2;
		ent->client->resp.osp_r02c = 1;

		if (m_mode > 1)
			OSP_teamMenu (ent);
		else
			OSP_DMMenu (ent);
	}
}

/*
=================
Cmd_InvUse_f
=================
*/
// gamex86.dll: 1000A5A9..1000A768
// gamei386.so: 00016260..000163D8
void Cmd_InvUse_f (edict_t *ent)
{
	// `cl` BEFORE `it`: the declaration order carries the frame layout.
	gclient_t	*cl;
	gitem_t		*it;

	cl = ent->client;

	if (cl->inmenu)
	{
		if (cl->resp.osp_r010 <= level.framenum)
		{
			cl->resp.osp_r010 = level.framenum + 2;
			cl->resp.osp_r264 = 0;
			PMenu_Select (ent);
			return;
		}
		return;
	}

	if (m_mode)
	{
		if (level.intermissiontime)
			return;
	}

	if (!m_mode && cl->showscores && cl->resp.osp_r24c != 1
		&& cl->resp.osp_r010 <= level.framenum && active_clients)
	{
		cl->resp.osp_r010 = level.framenum + 2;
		if (!cl->resp.osp_r24c)
			cl->resp.osp_r24c = 8;
		else
		{
			cl->resp.osp_r24c = 0;
			cl->resp.osp_r2ac = -1;
		}
		DeathmatchScoreboardMessage (ent, ent->enemy);
		gi.unicast (ent, true);
		return;
	}

	if (cl->resp.entered != ENTERED_ENTERED)
		return;

	ValidateSelectedItem (ent);

	if (cl->pers.selected_item == -1)
	{
		gi.cprintf (ent, PRINT_HIGH, "No item to use.\n");
		return;
	}

	it = &itemlist[cl->pers.selected_item];
	if (!it->use)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not usable.\n");
		return;
	}
	it->use (ent, it);
}

/*
=================
Cmd_WeapPrev_f
=================
*/
// gamex86.dll: 1000A768..1000A837
// gamei386.so: 000163D8..0001648D
void Cmd_WeapPrev_f (edict_t *ent)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;
	int			selected_weapon;

	cl = ent->client;

	if (!cl->pers.weapon)
		return;

	selected_weapon = ITEM_INDEX(cl->pers.weapon);

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (selected_weapon + i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (! (it->flags & IT_WEAPON) )
			continue;
		it->use (ent, it);
		if (cl->pers.weapon == it)
			return;	// successful
	}
}

/*
=================
Cmd_WeapNext_f
=================
*/
// gamex86.dll: 1000A837..1000A90B
// gamei386.so: 00016490..0001654B
void Cmd_WeapNext_f (edict_t *ent)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;
	int			selected_weapon;

	cl = ent->client;

	if (!cl->pers.weapon)
		return;

	selected_weapon = ITEM_INDEX(cl->pers.weapon);

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (selected_weapon + MAX_ITEMS - i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (! (it->flags & IT_WEAPON) )
			continue;
		it->use (ent, it);
		if (cl->pers.weapon == it)
			return;	// successful
	}
}

/*
=================
Cmd_WeapLast_f
=================
*/
// gamex86.dll: 1000A90B..1000A99D
// gamei386.so: 0001654C..000165C4
void Cmd_WeapLast_f (edict_t *ent)
{
	gclient_t	*cl;
	int			index;
	gitem_t		*it;

	cl = ent->client;

	if (!cl->pers.weapon || !cl->pers.lastweapon)
		return;

	index = ITEM_INDEX(cl->pers.lastweapon);
	if (!cl->pers.inventory[index])
		return;
	it = &itemlist[index];
	if (!it->use)
		return;
	if (! (it->flags & IT_WEAPON) )
		return;
	it->use (ent, it);
}

/*
=================
Cmd_InvDrop_f
=================
*/
// gamex86.dll: 1000A99D..1000AABA
// gamei386.so: 000165C4..000166BF
void Cmd_InvDrop_f (edict_t *ent)
{
	gitem_t		*it;

	if (ent->client->inmenu)
	{
		if (ent->client->resp.osp_r010 <= level.framenum)
		{
			ent->client->resp.osp_r010 = level.framenum + 2;
			ent->client->resp.osp_r264 = 1;
			PMenu_Select (ent);
			return;
		}
		return;
	}

	if (sync_stat < 4)
	{
		gi.cprintf (ent, PRINT_HIGH, "Cannot drop items during warmup!\n");
		return;
	}

	if (ent->client->resp.entered != ENTERED_ENTERED || ent->health <= 0)
		return;

	ValidateSelectedItem (ent);

	if (ent->client->pers.selected_item == -1)
	{
		gi.cprintf (ent, PRINT_HIGH, "No item to drop.\n");
		return;
	}

	it = &itemlist[ent->client->pers.selected_item];
	if (!it->drop)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not droppable.\n");
		return;
	}
	it->drop (ent, it);
}

/*
=================
Cmd_Kill_f
=================
*/
// gamex86.dll: 1000AABA..1000AB8E
// gamei386.so: 000166C0..00016770
void Cmd_Kill_f (edict_t *ent)
{
	// ONE combined condition with ONE return.
	if (((level.time - ent->client->respawn_time) < 5 && sync_stat != 2) ||
		ent->client->resp.entered != ENTERED_ENTERED)
		return;

	ent->client->resp.osp_r23c = 0;
	ent->s.effects = 0;
	ent->s.renderfx = 0;
	PlayerResetGrapple (ent);
	ent->flags &= ~FL_GODMODE;
	ent->health = 0;
	meansOfDeath = MOD_SUICIDE;
	player_die (ent, ent, ent, 100000, vec3_origin);
	// don't even bother waiting for death frames
	ent->deadflag = DEAD_DEAD;
	respawn (ent);
}

/*
=================
Cmd_PutAway_f
=================
*/
// gamex86.dll: 1000AB8E..1000ABFC
// gamei386.so: 00016770..000167DC
void Cmd_PutAway_f (edict_t *ent)
{
	if (ent->client->inmenu)
		PMenu_Close (ent);
	ent->client->showscores = false;
	ent->client->showhelp = false;
	ent->client->showinventory = false;
	ent->client->ps.stats[STAT_OSP_LAYOUT1] = 0;
	ent->client->ps.stats[STAT_OSP_LAYOUT2] = 0;
}


// gamex86.dll: 1000ABFC..1000AC68
// gamei386.so: 000167DC..00016855
int PlayerSort (void const *a, void const *b)
{
	int		anum, bnum;

	anum = *(int *)a;
	bnum = *(int *)b;

	anum = game.clients[anum].ps.stats[STAT_FRAGS];
	bnum = game.clients[bnum].ps.stats[STAT_FRAGS];

	if (anum < bnum)
		return -1;
	if (anum > bnum)
		return 1;
	return 0;
}

/*
=================
Cmd_Players_f
=================
*/
// gamex86.dll: 1000AC68..1000AE04
// gamei386.so: 00016858..000169FA
void Cmd_Players_f (edict_t *ent)
{
	// No cached `cl` and no `id`: every `e->client` and every `resp.clientid`
	// below is re-derived.
	int			i;
	edict_t		*e;

	if (!ent->osp_e39c)
		gi.cprintf (ent, PRINT_HIGH, "\nID:Name\n");
	else
		gi.cprintf (ent, PRINT_HIGH, "\nID:Name [Address]\n");
	gi.cprintf (ent, PRINT_HIGH, "---------------------\n");

	for (i = 1; i <= game.maxclients; i++)
	{
		e = g_edicts + i;
		if (!e->inuse || !e->client)
			continue;
		if (!(e->flags & FL_BOT))
		{
			if (e->client->resp.clientid != -1)
			{
				if (!ent->osp_e39c)
					gi.cprintf (ent, PRINT_HIGH, "%2d:\"%s\"\n",
						e->client->resp.clientid, e->client->pers.netname);
				else
				{
					OSP_getPlayerAddr (e);
					gi.cprintf (ent, PRINT_HIGH, "%2d:\"%s\" [%s]\n",
						e->client->resp.clientid, e->client->pers.netname, e->osp_e37c);
				}
			}
			else
				gi.cprintf (ent, PRINT_HIGH, "XX:\"%s\" (Connecting)\n",
					e->client->pers.netname);
		}
		else
			gi.cprintf (ent, PRINT_HIGH, "%2d:\"%s\" [BOT]\n",
				e->client->resp.clientid, e->client->pers.netname);
	}
}

/*
=================
Cmd_Wave_f
=================
*/
// gamex86.dll: 1000AE04..1000AF7E
// gamei386.so: 000169FC..00016B68
void Cmd_Wave_f (edict_t *ent)
{
	int		i;

	i = atoi (gi.argv(1));

	// can't wave when ducked
	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
		return;

	if (ent->client->anim_priority > ANIM_WAVE)
		return;

	ent->client->anim_priority = ANIM_WAVE;

	switch (i)
	{
	case 0:
		gi.cprintf (ent, PRINT_HIGH, "flipoff\n");
		ent->s.frame = FRAME_flip01-1;
		ent->client->anim_end = FRAME_flip12;
		break;
	case 1:
		gi.cprintf (ent, PRINT_HIGH, "salute\n");
		ent->s.frame = FRAME_salute01-1;
		ent->client->anim_end = FRAME_salute11;
		break;
	case 2:
		gi.cprintf (ent, PRINT_HIGH, "taunt\n");
		ent->s.frame = FRAME_taunt01-1;
		ent->client->anim_end = FRAME_taunt17;
		break;
	case 3:
		gi.cprintf (ent, PRINT_HIGH, "wave\n");
		ent->s.frame = FRAME_wave01-1;
		ent->client->anim_end = FRAME_wave11;
		break;
	case 4:
	default:
		gi.cprintf (ent, PRINT_HIGH, "point\n");
		ent->s.frame = FRAME_point01-1;
		ent->client->anim_end = FRAME_point12;
		break;
	}
}

/*
==================
Cmd_Say_f
==================
*/
// gamex86.dll: 1000AF7E..1000B508
// gamei386.so: 00016B68..0001711E
void Cmd_Say_f (edict_t *ent, qboolean team, qboolean arg0)
{
	int		i;
	char	*p;
	char	text[2048];
	gclient_t *cl;
	edict_t	*other;
	int		quiet;

	if (gi.argc () < 2 && !arg0)
		return;

	ent->client->resp.osp_r0d8 = 0;

	if (team)
		Com_sprintf (text, sizeof(text), "(%s): ", ent->client->pers.netname);
	else
		Com_sprintf (text, sizeof(text), "%s: ", ent->client->pers.netname);

	if (arg0)
	{
		strcat (text, gi.argv(0));
		strcat (text, " ");
		strcat (text, gi.args());
	}
	else
	{
		p = gi.args();

		if (*p == '"')
		{
			p++;
			p[strlen(p)-1] = 0;
		}
		strcat(text, p);
	}

	// don't let text be too long for malicious reasons
	text[150] = 0;

	if (flood_msgs->value && !team && !match_paused) {
		cl = ent->client;

        if (level.time < cl->flood_locktill) {
			gi.cprintf(ent, PRINT_HIGH, "You can't talk for %d more seconds\n",
				(int)(cl->flood_locktill - level.time) + 1);
            return;
        }
        i = cl->flood_whenhead - flood_msgs->value + 1;
        if (i < 0)
            i = (sizeof(cl->flood_when)/sizeof(cl->flood_when[0])) + i;
		if (cl->flood_when[i] && 
			level.time - cl->flood_when[i] < flood_persecond->value) {
			cl->flood_locktill = level.time + flood_waitdelay->value;
			gi.cprintf(ent, PRINT_CHAT, "Flood protection:  You can't talk for %d seconds.\n",
				(int)flood_waitdelay->value);
            return;
        }
		cl->flood_whenhead = (cl->flood_whenhead + 1) %
			(sizeof(cl->flood_when)/sizeof(cl->flood_when[0]));
		cl->flood_when[cl->flood_whenhead] = level.time;
	}

	q2log_playerChat (text);
	strcat (text, "\n");

	if (!team)
	{
		if (sync_stat > 2 && !match_paused)
		{
			if (dedicated->value)
				gi.dprintf ("%s", text);

			// ONE i/other pair is shared across all three broadcast loops below,
			// and `i` doubles as the flood-index scratch above.  The
			// quiet/entered/self test is a positive "should I print" condition,
			// De Morgan-negated from the natural "should I skip" reading.
			for (i = 1; i <= game.maxclients; i++)
			{
				other = &g_edicts[i];
				if (!other->inuse || !other->client)
					continue;
				quiet = other->client->resp.osp_r0b0;
				if (other->client->resp.entered != ENTERED_ENTERED || !quiet ||
					(quiet == 1 && ent->client->resp.entered == ENTERED_ENTERED) ||
					other == ent)
					gi.cprintf (other, PRINT_CHAT, "%s", text);
			}
		}
		else
			gi.bprintf (PRINT_CHAT, "%s", text);
	}
	else
	{
		if (dedicated->value)
			gi.dprintf ("%s", text);

		if (m_mode == 2)
		{
			for (i = 1; i <= game.maxclients; i++)
			{
				other = &g_edicts[i];
				if (!other->inuse || !other->client)
					continue;
				if (ent->client->resp.team != other->client->resp.team)
					continue;
				gi.cprintf (other, PRINT_CHAT, "%s", text);
			}
		}
		else
		{
			for (i = 1; i <= game.maxclients; i++)
			{
				other = &g_edicts[i];
				if (!other->inuse || !other->client)
					continue;
				if ((ent->client->resp.entered == ENTERED_ENTERED && other->client->resp.entered != ENTERED_ENTERED)
					|| (ent->client->resp.entered != ENTERED_ENTERED && other->client->resp.entered == ENTERED_ENTERED))
					continue;
				gi.cprintf (other, PRINT_CHAT, "%s", text);
			}
		}
	}
}


/*
=================
ClientCommand
=================
*/
// gamex86.dll: 1000B508..1000C950
// gamei386.so: 00017120..00018A12
void ClientCommand (edict_t *ent)
{
	char		*cmdstr;
	gitem_t		*it;

	if (!ent->client)
		return;

	cmdstr = gi.argv (0);
	if (!Q_stricmp (cmdstr, "say"))
	{
		Cmd_Say_f (ent, false, false);
		return;
	}
	if (!Q_stricmp (cmdstr, "score"))
	{
		Cmd_Score_f (ent);
		return;
	}
	if (!Q_stricmp (cmdstr, "help"))
	{
		Cmd_Help_f (ent);
		return;
	}
	if (!Q_stricmp (cmdstr, "hook") || !Q_stricmp (cmdstr, "hookon"))
	{
		OSP_hookon_cmd (ent);
		return;
	}
	if (!Q_stricmp (cmdstr, "unhook") || !Q_stricmp (cmdstr, "hookoff"))
	{
		OSP_hookoff_cmd (ent);
		return;
	}
	if (!Q_stricmp (cmdstr, "players"))
	{
		Cmd_Players_f (ent);
		return;
	}
	if (!Q_stricmp (cmdstr, "talkto") || !Q_stricmp (cmdstr, "talk") ||
		!Q_stricmp (cmdstr, "tell"))
	{
		OSP_talkto_cmd (ent);
		return;
	}
	if (m_mode == 2 && (!Q_stricmp (cmdstr, "say_team") ||
		!Q_stricmp (cmdstr, "steam")))
	{
		OSP_sayteam_cmd (ent, gi.args ());
		return;
	}
	if (!Q_stricmp (cmdstr, "say_team"))
	{
		Cmd_Say_f (ent, true, false);
		return;
	}
	if (!Q_stricmp (cmdstr, "accuracy") || !Q_stricmp (cmdstr, "stats"))
	{
		OSP_accuracy_cmd (ent);
		return;
	}
	if (!Q_stricmp (cmdstr, "oldaccuracy") || !Q_stricmp (cmdstr, "oldstats") ||
		!Q_stricmp (cmdstr, "laststats"))
	{
		OSP_oldaccuracy_cmd (ent);
		return;
	}
	if (ent->osp_e39c != 1 && (!Q_stricmp (cmdstr, "referee") ||
		!Q_stricmp (cmdstr, "admin") || !Q_stricmp (cmdstr, "ref")))
	{
		OSP_referee_cmd (ent);
		return;
	}
	if (!Q_stricmp (cmdstr, "joincode"))
	{
		OSP_joincode_cmd (ent);
		return;
	}
	if (m_mode > 0 && !Q_stricmp (cmdstr, "_is_referee"))
	{
		OSP_isreferee_cmd (ent);
		return;
	}
	if (m_mode > 1 && !Q_stricmp (cmdstr, "_default_team_info"))
	{
		OSP_defaultteam_cmd (ent);
		return;
	}
	if (m_mode == 2 && !Q_stricmp (cmdstr, "_default_join_code"))
	{
		OSP_defaultjoincode_cmd (ent);
		return;
	}
	if (!Q_stricmp (cmdstr, "_ngws_client_id"))
	{
		q2log_clientid_cmd (ent);
		return;
	}
	if (bot_watch && !Q_stricmp (cmdstr, "_init_state"))
	{
		OSP_speedCheat_cmd (ent);
		return;
	}

	if ((!match_paused || ent->client->inmenu) && !Q_stricmp (cmdstr, "invnext"))
	{
		SelectNextItem (ent, -1);
		return;
	}
	if ((!match_paused || ent->client->inmenu) && !Q_stricmp (cmdstr, "invprev"))
	{
		SelectPrevItem (ent, -1);
		return;
	}
	if (!Q_stricmp (cmdstr, "invuse"))
	{
		Cmd_InvUse_f (ent);
		return;
	}
	if (level.intermissiontime)
		return;

	if ((!match_paused || ent->client->inmenu) && !Q_stricmp (cmdstr, "use"))
		Cmd_Use_f (ent);
	else if (!Q_stricmp (cmdstr, "invnextw") && !match_paused)
		SelectNextItem (ent, IT_WEAPON);
	else if (!Q_stricmp (cmdstr, "invprevw") && !match_paused)
		SelectPrevItem (ent, IT_WEAPON);
	else if (!Q_stricmp (cmdstr, "invdrop"))
		Cmd_InvDrop_f (ent);
	else if (!Q_stricmp (cmdstr, "weapprev") && !match_paused)
		Cmd_WeapPrev_f (ent);
	else if (!Q_stricmp (cmdstr, "weapnext") && !match_paused)
		Cmd_WeapNext_f (ent);
	else if (!Q_stricmp (cmdstr, "weaplast"))
		Cmd_WeapLast_f (ent);
	else if (!Q_stricmp (cmdstr, "kill") && !match_paused)
		Cmd_Kill_f (ent);
	else if (!Q_stricmp (cmdstr, "putaway"))
		Cmd_PutAway_f (ent);
	else if (!Q_stricmp (cmdstr, "wave"))
		Cmd_Wave_f (ent);
	else if (!Q_stricmp (cmdstr, "invnextp"))
		SelectNextItem (ent, IT_POWERUP);
	else if (!Q_stricmp (cmdstr, "invprevp"))
		SelectPrevItem (ent, IT_POWERUP);
	else if (!Q_stricmp (cmdstr, "god"))
		Cmd_God_f (ent);
	else if (!Q_stricmp (cmdstr, "notarget"))
		Cmd_Notarget_f (ent);
	else if (!Q_stricmp (cmdstr, "noclip"))
		Cmd_Noclip_f (ent);
	else if (!Q_stricmp (cmdstr, "drop"))
		Cmd_Drop_f (ent);
	else if (!Q_stricmp (cmdstr, "give"))
		Cmd_Give_f (ent);
	else if (!Q_stricmp (cmdstr, "id"))
		OSP_id_cmd (ent);
	else if (!Q_stricmp (cmdstr, "motd"))
		OSP_motd_cmd (ent);
	else if (!Q_stricmp (cmdstr, "hud") || !Q_stricmp (cmdstr, "display"))
		OSP_hud_cmd (ent);
	else if (m_mode == 0 && (!Q_stricmp (cmdstr, "highscores") ||
		!Q_stricmp (cmdstr, "highscore") || !Q_stricmp (cmdstr, "hiscores") ||
		!Q_stricmp (cmdstr, "hiscore")))
		OSP_highscores_cmd (ent);
	else if (!(int)match_strictmode->value && !Q_stricmp (cmdstr, "ready"))
		OSP_ready_cmd (ent, false);
	else if (!(int)match_strictmode->value && (!Q_stricmp (cmdstr, "notready") ||
		!Q_stricmp (cmdstr, "unready") || !Q_stricmp (cmdstr, "noready")))
		OSP_notready_cmd (ent, false);
	else if ((!(int)match_strictmode->value ||
		ent->client->resp.entered != ENTERED_ENTERED) &&
		(!Q_stricmp (cmdstr, "chasecam") || !Q_stricmp (cmdstr, "chase")))
		OSP_ChaseCam (ent);
	else if ((!(int)match_strictmode->value ||
		ent->client->resp.entered != ENTERED_ENTERED) &&
		(!Q_stricmp (cmdstr, "observer") || !Q_stricmp (cmdstr, "observe")))
		OSP_startObserve (ent);
	else if ((!(int)match_strictmode->value ||
		ent->client->resp.entered != ENTERED_ENTERED) && !Q_stricmp (cmdstr, "autocam"))
		CameraCmd (ent, true);
	else if ((!(int)match_strictmode->value ||
		ent->client->resp.entered != ENTERED_ENTERED) &&
		(!Q_stricmp (cmdstr, "menu") || !Q_stricmp (cmdstr, "ctfmenu")))
		Cmd_Inven_f (ent);
	else if ((!(int)match_strictmode->value ||
		ent->client->resp.entered != ENTERED_ENTERED) && !Q_stricmp (cmdstr, "inven"))
		Cmd_Inven_f (ent);
	else if (!Q_stricmp (cmdstr, "matchinfo"))
		OSP_showinfo_cmd (ent);
	else if (!Q_stricmp (cmdstr, "vote"))
		OSP_vote_cmd (ent, 0, 0, NULL, NULL);
	else if (!Q_stricmp (cmdstr, "yes"))
		OSP_yes_cmd (ent);
	else if (!Q_stricmp (cmdstr, "no"))
		OSP_no_cmd (ent);
	else if (m_mode < 2 && (!Q_stricmp (cmdstr, "join") || !Q_stricmp (cmdstr, "joingame")))
		OSP_ffajoin_cmd (ent);
	else if (!Q_stricmp (cmdstr, "oldscores") || !Q_stricmp (cmdstr, "oldscore") ||
		!Q_stricmp (cmdstr, "lastscores") || !Q_stricmp (cmdstr, "lastscore"))
		OSP_oldscores_cmd (ent);
	else if (!Q_stricmp (cmdstr, "ignore") || !Q_stricmp (cmdstr, "mute") ||
		!Q_stricmp (cmdstr, "muzzle") || !Q_stricmp (cmdstr, "filter"))
		OSP_muzzle_cmd (ent);
	else if (!Q_stricmp (cmdstr, "droptech") || !Q_stricmp (cmdstr, "droprune"))
	{
		it = OSP_What_Rune (ent);
		if (it)
			it->drop (ent, it);
		return;
	}
	else if (m_mode > 1 && !Q_stricmp (cmdstr, "teamname"))
		OSP_teamname_cmd (ent);
	else if (m_mode > 1 && !Q_stricmp (cmdstr, "teamskin"))
		OSP_teamskin_cmd (ent);
	else if (m_mode > 1 && (!Q_stricmp (cmdstr, "join") ||
		!Q_stricmp (cmdstr, "jointeam") || !Q_stricmp (cmdstr, "team")))
		OSP_teamjoin_cmd (ent, NULL);
	else if (m_mode == 2 && (!Q_stricmp (cmdstr, "invite") ||
		!Q_stricmp (cmdstr, "pick") || !Q_stricmp (cmdstr, "pickplayer")))
		OSP_teaminvite_cmd (ent);
	else if (m_mode == 2 && (!Q_stricmp (cmdstr, "switchteam") ||
		!Q_stricmp (cmdstr, "switchteams")))
		OSP_switchteam_cmd (ent);
	else if (m_mode == 2 && (!Q_stricmp (cmdstr, "lockteam") ||
		!Q_stricmp (cmdstr, "teamlock") || !Q_stricmp (cmdstr, "lock")))
		OSP_lockteam_cmd (ent);
	else if (m_mode == 2 && (!Q_stricmp (cmdstr, "unlockteam") ||
		!Q_stricmp (cmdstr, "teamunlock") || !Q_stricmp (cmdstr, "unlock")))
		OSP_unlockteam_cmd (ent);
	else if (m_mode == 2 && !(int)match_strictmode->value &&
		(!Q_stricmp (cmdstr, "readyteam") || !Q_stricmp (cmdstr, "teamready") ||
		 !Q_stricmp (cmdstr, "teamallready")))
		OSP_readyteam_cmd (ent);
	else if (m_mode == 2 && !(int)match_strictmode->value &&
		(!Q_stricmp (cmdstr, "notreadyteam") || !Q_stricmp (cmdstr, "unreadyteam") ||
		 !Q_stricmp (cmdstr, "noreadyteam") || !Q_stricmp (cmdstr, "teamnotready")))
		OSP_notreadyteam_cmd (ent);
	else if (m_mode == 2 && (!Q_stricmp (cmdstr, "captain") ||
		!Q_stricmp (cmdstr, "leader") || !Q_stricmp (cmdstr, "teamcaptain")))
		OSP_captain_cmd (ent);
	else if (m_mode == 2 && !Q_stricmp (cmdstr, "captains"))
		OSP_captains_cmd (ent);
	else if (m_mode == 2 && (!Q_stricmp (cmdstr, "kickplayer") ||
		!Q_stricmp (cmdstr, "remove") || !Q_stricmp (cmdstr, "removeplayer")))
		OSP_kickplayer_cmd (ent);
	else if (m_mode > 1 && (!Q_stricmp (cmdstr, "time") ||
		!Q_stricmp (cmdstr, "matchpause") || !Q_stricmp (cmdstr, "timeout") ||
		!Q_stricmp (cmdstr, "timein")))
		OSP_playertime_cmd (ent);
	else if (m_mode == 3 && (!Q_stricmp (cmdstr, "queue") ||
		!Q_stricmp (cmdstr, "line") || !Q_stricmp (cmdstr, "order")))
		OSP_1v1queue_cmd (ent);
	else if (ent->osp_e39c)
	{
		if (!Q_stricmp (cmdstr, "r_help"))
			OSP_rhelp_cmd (ent);
		else if (!Q_stricmp (cmdstr, "r_kick"))
			OSP_rkick_cmd (ent);
		else if (!Q_stricmp (cmdstr, "r_mpause"))
			OSP_rmpause_cmd ();
		else if (!Q_stricmp (cmdstr, "r_map"))
			OSP_rmap_cmd (ent);
		else if (!Q_stricmp (cmdstr, "r_timelimit"))
			OSP_rtimelimit_cmd (ent);
		else if (!Q_stricmp (cmdstr, "r_fraglimit"))
			OSP_rfraglimit_cmd (ent);
		else if (!Q_stricmp (cmdstr, "r_allready") && m_mode > 0)
			OSP_allready_svcmd ();
		else if (!Q_stricmp (cmdstr, "r_allnotready") && m_mode > 0)
			OSP_allnotready_svcmd ((edict_t *)true);
		else if ((!Q_stricmp (cmdstr, "r_stopmatch") ||
			!Q_stricmp (cmdstr, "r_endmatch")) && m_mode > 0)
			OSP_rstopmatch_cmd (ent);
		else if (!Q_stricmp (cmdstr, "r_players") ||
			!Q_stricmp (cmdstr, "r_plist"))
			Cmd_Players_f (ent);
		else if (!Q_stricmp (cmdstr, "r_banlist") ||
			!Q_stricmp (cmdstr, "r_listban") || !Q_stricmp (cmdstr, "r_blist"))
			OSP_rbanlist_cmd (ent);
		else if (!Q_stricmp (cmdstr, "r_ban"))
			OSP_rban_cmd (ent, NULL);
		else if (!Q_stricmp (cmdstr, "r_banaddr"))
			OSP_rbanaddr_cmd (ent);
		else if (!Q_stricmp (cmdstr, "r_unban"))
			OSP_runban_cmd (ent);
		else if (!Q_stricmp (cmdstr, "r_unbanaddr"))
			OSP_runbanaddr_cmd (ent);
		// A referee's unrecognised command still falls through to the bot
		// command table and then to chat -- the same tail as the outer chain,
		// written out twice.
		else
		{
			if (BotCmd (cmdstr, ent, false))
				goto clear_args;
			Cmd_Say_f (ent, false, true);
		}
	}
	else
	{
		if (BotCmd (cmdstr, ent, false))
			goto clear_args;
		Cmd_Say_f (ent, false, true);
	}

clear_args:
	BotClearCommandArguments ();
}
