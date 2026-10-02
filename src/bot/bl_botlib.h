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
// The botlibs this game can run, side by side.  Colosseum's own file, not a
// donor's: the Gladiator SDK knows one bot library per server.
//
// A botlib here is a bot library together with everything it needs from the
// game that the bot_import_t/bot_export_t ABI does not say -- where its files
// are, which bot list drives it, how it numbers the inventory, which HUD stats
// it reads.  Both botlibs speak the one ABI (docs/botlib-contract.md); this
// table is the half of the contract that differs between them (R-BOT-31).
//
// `botlib_t` is the botlib as a kind; `bot_library_t` (bl_main.h) is one
// loaded copy of it, and at most one is loaded per botlib (R-BOT-33).

#ifndef BL_BOTLIB_H
#define BL_BOTLIB_H

typedef enum {
    BOTLIB_GLADIATOR,       // gladiator-bot-restored, Mr Elusive's 1999 botlib
    BOTLIB_Q3,              // q3a_bot_backport_for_q2, the Quake III bot
    BOTLIB_COUNT
} botlibnum_t;

typedef struct botlib_s {
    botlibnum_t num;
    // The word `botlibs` lists and `addbot` and `sv botlibs` print.
    const char *id;
    // The heading its bot list has in the menus, and the two letters OSP's bot
    // picker prefixes a name with ("GB|" was always there for Gladiator).
    const char *title;
    const char *tag;
    // Its library's file name, without the platform's extension.
    const char *library;
    // What its BotVersion() starts with: the handshake (R-BOT-31).
    const char *version;
    // The directory under the gamedir that holds every file it reads, pushed
    // to it as the `datadir` libvar.  "" is the gamedir itself, which is where
    // the 1999 layout puts Gladiator's (pak7.pak, maps/*.aas), and is not
    // pushed at all.
    const char *datadir;
    // Its bot list, a gamedir path.  NULL is Gladiator's own: the
    // `botfile`/`bots_botfile` cvar and every bots/*.cfg (R-BOT-26).
    const char *roster;
    // The AAS file versions it loads.  Gladiator's bspc writes 3 and the
    // Quake III one 5 -- and both botlibs look for maps/<map>.aas.
    int         aasmin, aasmax;
    // Whether a bot of it has a skill of its own, 1..5.  Gladiator's
    // characters carry theirs inside the character file.
    bool        skills;
} botlib_t;

extern const botlib_t botlibs[BOTLIB_COUNT];

// lookups
const botlib_t *BotlibNamed(const char *id);
const botlib_t *BotlibForVersion(const char *version);
// the file a botlib is loaded from by default, e.g. "q3bot.so"
const char *BotlibFile(const botlib_t *botlib);

// `botlibs`: the botlibs this server offers, in order of preference.  The
// cvar is obtained here and nowhere else, so its default is said once.
cvar_t *BotlibsCvar(void);
int  BotlibsOffered(const botlib_t **list);
bool BotlibOffered(const botlib_t *botlib);
// More than one: whether a label that named Gladiator's bots has to say
// whose a bot is instead (R-BOT-34).
bool BotlibsMany(void);
// The word a label puts before "bot": the one offered botlib's title and a
// space -- "Gladiator ", 1999's word -- or "" when several are offered and a
// bot's own label says whose it is.
const char *BotlibsQualifier(void);
// The first botlib offered: what drives a bot nobody chose one for.
const botlib_t *BotlibDefault(void);
// The first botlib offered that can play the map being played -- the one the
// fill and `addrandom` use -- or NULL when none can.
const botlib_t *BotlibPreferred(void);
// ...and the question asked of one: its library has not refused this map,
// and the AAS file it would look for is there, in a version it reads.
bool BotlibPlayable(const botlib_t *botlib);
// The AAS file a botlib finds for `map`, searched for the way that botlib
// searches.  Returns its version (> 0), 0 when there is none, -1 when the
// file is there and is not an AAS file.  `path` may be NULL.
int  BotlibMesh(const botlib_t *botlib, const char *map, char *path, size_t size);
// A botlib's library refused this level's map; cleared by BotlibNewLevel.
void BotlibSetMapFailed(const botlib_t *botlib);
bool BotlibMapFailed(const botlib_t *botlib);
void BotlibNewLevel(void);

// `botskill`, and a bot's own: the `skill` key in its userinfo when it has
// one, 1..5, else the cvar.
cvar_t *BotSkill(void);
int  BotSkillOf(const char *userinfo);
const char *BotSkillName(int skill);

// `sv botlibs`
void BotlibDump(void);

#endif // BL_BOTLIB_H
