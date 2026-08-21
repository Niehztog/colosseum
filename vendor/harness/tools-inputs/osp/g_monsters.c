/*
 * g_monsters.c -- monster spawn stubs.  *** THE FILENAME IS INVENTED. ***
 *
 * OSP Tourney is pure PvP, so every monster entry point the vanilla spawn
 * table can reach is reduced to freeing the entity.  `spawns[]` still maps
 * every monster_* classname here, so a map with monsters loads and simply
 * has none.
 */

#include "g_local.h"

// gamex86.dll: 10017330..10017341
// gamei386.so: 00024164..00024181
void SP_monster_berserk (edict_t *self)			{ G_FreeEdict (self); }
// gamex86.dll: 10017341..10017352
// gamei386.so: 00024184..000241A1
void SP_monster_gladiator (edict_t *self)		{ G_FreeEdict (self); }
// gamex86.dll: 10017352..10017363
// gamei386.so: 000241A4..000241C1
void SP_monster_gunner (edict_t *self)			{ G_FreeEdict (self); }
// gamex86.dll: 10017363..10017374
// gamei386.so: 000241C4..000241E1
void SP_monster_infantry (edict_t *self)		{ G_FreeEdict (self); }
// gamex86.dll: 10017374..10017385
// gamei386.so: 000241E4..00024201
void SP_monster_soldier_light (edict_t *self)	{ G_FreeEdict (self); }
// gamex86.dll: 10017385..10017396
// gamei386.so: 00024204..00024221
void SP_monster_soldier (edict_t *self)			{ G_FreeEdict (self); }
// gamex86.dll: 10017396..100173A7
// gamei386.so: 00024224..00024241
void SP_monster_soldier_ss (edict_t *self)		{ G_FreeEdict (self); }
// gamex86.dll: 100173A7..100173B8
// gamei386.so: 00024244..00024261
void SP_monster_tank (edict_t *self)			{ G_FreeEdict (self); }
// gamex86.dll: 100173B8..100173C9
// gamei386.so: 00024264..00024281
void SP_monster_medic (edict_t *self)			{ G_FreeEdict (self); }
// gamex86.dll: 100173C9..100173DA
// gamei386.so: 00024284..000242A1
void SP_monster_flipper (edict_t *self)			{ G_FreeEdict (self); }
// gamex86.dll: 100173DA..100173EB
// gamei386.so: 000242A4..000242C1
void SP_monster_chick (edict_t *self)			{ G_FreeEdict (self); }
// gamex86.dll: 100173EB..100173FC
// gamei386.so: 000242C4..000242E1
void SP_monster_parasite (edict_t *self)		{ G_FreeEdict (self); }
// gamex86.dll: 100173FC..1001740D
// gamei386.so: 000242E4..00024301
void SP_monster_flyer (edict_t *self)			{ G_FreeEdict (self); }
// gamex86.dll: 1001740D..1001741E
// gamei386.so: 00024304..00024321
void SP_monster_brain (edict_t *self)			{ G_FreeEdict (self); }
// gamex86.dll: 1001741E..1001742F
// gamei386.so: 00024324..00024341
void SP_monster_floater (edict_t *self)			{ G_FreeEdict (self); }
// gamex86.dll: 1001742F..10017440
// gamei386.so: 00024344..00024361
void SP_monster_hover (edict_t *self)			{ G_FreeEdict (self); }
// gamex86.dll: 10017440..10017451
// gamei386.so: 00024364..00024381
void SP_monster_mutant (edict_t *self)			{ G_FreeEdict (self); }
// gamex86.dll: 10017451..10017462
// gamei386.so: 00024384..000243A1
void SP_monster_supertank (edict_t *self)		{ G_FreeEdict (self); }
// gamex86.dll: 10017462..10017473
// gamei386.so: 000243A4..000243C1
void SP_monster_boss2 (edict_t *self)			{ G_FreeEdict (self); }
// gamex86.dll: 10017473..10017484
// gamei386.so: 000243C4..000243E1
void SP_monster_jorg (edict_t *self)			{ G_FreeEdict (self); }
// gamex86.dll: 10017484..10017495
// gamei386.so: 000243E4..00024401
void SP_monster_boss3_stand (edict_t *self)		{ G_FreeEdict (self); }
// gamex86.dll: 10017495..100174A6
// gamei386.so: 00024404..00024421
void SP_monster_commander_body (edict_t *self)	{ G_FreeEdict (self); }
// gamex86.dll: 100174A6..100174B7
// gamei386.so: 00024424..00024441
void SP_misc_actor (edict_t *self)				{ G_FreeEdict (self); }
// gamex86.dll: 100174B7..100174C8
// gamei386.so: 00024444..00024461
void SP_target_actor (edict_t *self)			{ G_FreeEdict (self); }
// gamex86.dll: 100174C8..100174D9
// gamei386.so: 00024464..00024481
void SP_misc_insane (edict_t *self)				{ G_FreeEdict (self); }
