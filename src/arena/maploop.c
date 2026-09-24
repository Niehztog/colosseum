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
// Rocket Arena 2 v2.25, from rocketarena2@358b325.
// Donor-only: baseq2 has no counterpart, so it lives in src/arena/ rather than
// being merged into a spine file.  The reconstruction's asm-matching
// address comments are stripped.
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

#include "g_local.h"
#include "arena/arena.h"

#define MAX_DEFS    256
#define CONFIG_TOKEN_SIZE 4096
// The size of one definition's accumulated value text.  It was a bare
// 0x400 at the allocation and nothing at all at the append, so a long
// enough arena.cfg walked off the end of a TAG_LEVEL block.
#define VAL_BLOCK_SIZE  0x400

typedef struct definition_s {
    int             count;
    int             count2;
    char            *value;
    int             type;
    void            *value2;
} definition_t;

typedef struct {
    int             count;
    definition_t    *cursor;
    definition_t    *item;
} blockstack_t;

definition_t    *find_key(char *key, int type, definition_t *items, int count);

// The donor's `gamedir` was here and is gone with the two relative paths
// that were the only things reading it -- G_FsGamePath() asks the engine which
// directory the game is installed in rather than assuming it is the one the
// server was started from.
static  cvar_t  *arenacfg;
static  char    *line;

definition_t    *map_loop;
definition_t    *map_block;
definition_t    **arena_blocks;
int             votetries_setting = 3;
int             num_definition_blocks = 0;
definition_t    *definition_blocks;

int     weapon_vals[RA_NUM_WEAPON_BITS] = {
    // 0..8, the donor's, paired with the digits 2..9 and 0 below
    1, 2, 4, 8, 16, 32, 64, 128, 256,
    // 9..14, the pack weapons, paired with the names in ra_pack_weapnames
    512, 1024, 2048, 4096, 8192, 16384,
};

// Names rather than digits because the digit row is used up at nine,
// and in this order because arena.h pins it: row 0 is bit 9.  The menu labels
// are padded to 23 characters like the donor's nine, because ra2menus.c reads a
// row back by comparing its whole label.
const ra_pack_weapon_t ra_pack_weapons[RA_NUM_PACK_WEAPONS] = {
    // cfg token       menu label                 layer         ammo
    { "ripper",       "Allow Ionripper:       ", LAYER_XATRIX }, // cells
    { "phalanx",      "Allow Phalanx:         ", LAYER_XATRIX }, // magslug
    { "etfrifle",     "Allow ETF Rifle:       ", LAYER_ROGUE  }, // flechettes
    { "proxlauncher", "Allow Prox Launcher:   ", LAYER_ROGUE  }, // prox
    { "plasmabeam",   "Allow Plasma Beam:     ", LAYER_ROGUE  }, // cells
    { "chainfist",    "Allow Chainfist:       ", LAYER_ROGUE  }, // none
};

bool RA_PackWeaponOffered(int i)
{
    if (i < 0 || i >= RA_NUM_PACK_WEAPONS)
        return false;
    return G_LayerEnabled(ra_pack_weapons[i].layer);
}

int RA_PackWeaponRow(const char *label)
{
    int i;

    for (i = 0; i < RA_NUM_PACK_WEAPONS; i++)
        if (!Q_stricmp(label, ra_pack_weapons[i].menulabel))
            return i;

    return -1;
}

int RA_LayerWeaponBits(void)
{
    int i, mask;

    mask = 0;
    for (i = 0; i < RA_NUM_PACK_WEAPONS; i++)
        if (RA_PackWeaponOffered(i))
            mask |= weapon_vals[9 + i];

    return mask;
}

int     weapons;
int     armor;
int     health;
int     minping;
int     maxping;
int     playersperteam;
int     rounds;
int     max_teams;
int     pickup;
int     rocket_speed;
int     shells;
int     bullets;
int     slugs;
int     grenades;
int     rockets;
int     cells;
// The mission packs' five.  Tesla and Trap are ammo and weapon in one
// item, exactly as `ammo_grenades` is, so like `grenades` above they are a
// count and not a `weapons:` bit.
int     magslug;
int     flechettes;
int     prox;
int     tesla;
int     trap;
int     allow_voting_packweapons;
int     fastswitch;
int     armorprotect;
int     healthprotect;
int     fallingdamage;
int     allow_voting_armor;
int     allow_voting_health;
int     allow_voting_minping;
int     allow_voting_maxping;
int     allow_voting_playersperteam;
int     allow_voting_rounds;
int     allow_voting_maxteams;
int     allow_voting_armorprotect;
int     allow_voting_healthprotect;
int     allow_voting_shotgun;
int     allow_voting_supershotgun;
int     allow_voting_machinegun;
int     allow_voting_chaingun;
int     allow_voting_grenadelauncher;
int     allow_voting_rocketlauncher;
int     allow_voting_hyperblaster;
int     allow_voting_railgun;
int     allow_voting_bfg;
int     allow_voting_fallingdamage;
int     lock_arena;
int     competition_mode;
int     damage_scoring;
int     arena_bots;
int     allow_voting_bots;

static  blockstack_t    stack[32];

// `key` is const because the caller passes a string literal out of a
// `const char *const []`; the function only ever strcmp's it.
int has_val(char *str, const char *key)
{
    char    buf[1024];
    char    *tok;

    Q_strlcpy(buf, str, sizeof(buf));
    tok = strtok(buf, " ");

    while (tok) {
        if (!strcmp(tok, key))
            return 1;

        tok = strtok(NULL, " ");
    }

    return 0;
}

char *get_val(char *str, int index)
{
    static char fnd[1024];
    char        buf[1024];
    char        *tok;

    Q_strlcpy(buf, str, sizeof(buf));
    tok = strtok(buf, " ");

    while (tok && index) {
        index--;
        tok = strtok(NULL, " ");
    }

    if (!tok)
        fnd[0] = 0;
    else
        Q_strlcpy(fnd, tok, sizeof(fnd));

    return fnd;
}

void get_settings(definition_t *items, int count)
{
    definition_t    *key;
    unsigned int    mask;
    bool            packnamed;
    int             i, n;

    key = find_key("weapons", 1, items, count);
    if (key) {
        mask = 0;

        for (i = 0; i <= 8; i++) {
            if (i == 8)
                n = 0;
            else
                n = i + 2;

            if (has_val(key->value2, va("%d", n)))
                mask |= weapon_vals[i];
        }

        // The pack six, named rather than numbered -- and the halves are
        // decided separately.  The digits above span exactly the nine weapons
        // 1999 had, so a `weapons:` line written before the packs existed says
        // everything there was to say about the baseq2 half and nothing about
        // the pack half; reading its silence as "none of the six" is what made
        // `xatrix 1` a menu row and no weapon (see RA_LayerWeaponBits).  A line
        // that names a pack weapon -- or `nopack`, which is how an arena says
        // "none of them" out loud -- owns the pack half; a line that names none
        // inherits it, which is the enclosing block's or the layer default's.
        packnamed = has_val(key->value2, "nopack");

        for (i = 0; i < RA_NUM_PACK_WEAPONS; i++)
            if (has_val(key->value2, ra_pack_weapons[i].cfgname)) {
                mask |= weapon_vals[9 + i];
                packnamed = true;
            }

        if (!packnamed)
            mask |= weapons & RA_PACK_WEAPON_MASK;

        weapons = mask;
    }

    key = find_key("armor", 1, items, count);
    if (key)
        armor = atoi(get_val(key->value2, 0));

    key = find_key("health", 1, items, count);
    if (key)
        health = atoi(get_val(key->value2, 0));

    key = find_key("minping", 1, items, count);
    if (key)
        minping = atoi(get_val(key->value2, 0));

    key = find_key("maxping", 1, items, count);
    if (key)
        maxping = atoi(get_val(key->value2, 0));

    key = find_key("playersperteam", 1, items, count);
    if (key)
        playersperteam = atoi(get_val(key->value2, 0));

    key = find_key("rounds", 1, items, count);
    if (key)
        rounds = atoi(get_val(key->value2, 0));

    key = find_key("maxteams", 1, items, count);
    if (key)
        max_teams = atoi(get_val(key->value2, 0));

    key = find_key("pickup", 1, items, count);
    if (key)
        pickup = atoi(get_val(key->value2, 0));

    key = find_key("rocketspeed", 1, items, count);
    if (key)
        rocket_speed = atoi(get_val(key->value2, 0));

    key = find_key("shells", 1, items, count);
    if (key)
        shells = atoi(get_val(key->value2, 0));

    key = find_key("bullets", 1, items, count);
    if (key)
        bullets = atoi(get_val(key->value2, 0));

    key = find_key("slugs", 1, items, count);
    if (key)
        slugs = atoi(get_val(key->value2, 0));

    key = find_key("grenades", 1, items, count);
    if (key)
        grenades = atoi(get_val(key->value2, 0));

    key = find_key("rockets", 1, items, count);
    if (key)
        rockets = atoi(get_val(key->value2, 0));

    key = find_key("cells", 1, items, count);
    if (key)
        cells = atoi(get_val(key->value2, 0));

    // The pack five, in the shape of the six above them.
    key = find_key("magslug", 1, items, count);
    if (key)
        magslug = atoi(get_val(key->value2, 0));

    key = find_key("flechettes", 1, items, count);
    if (key)
        flechettes = atoi(get_val(key->value2, 0));

    key = find_key("prox", 1, items, count);
    if (key)
        prox = atoi(get_val(key->value2, 0));

    key = find_key("tesla", 1, items, count);
    if (key)
        tesla = atoi(get_val(key->value2, 0));

    key = find_key("trap", 1, items, count);
    if (key)
        trap = atoi(get_val(key->value2, 0));

    key = find_key("fastswitch", 1, items, count);
    if (key)
        fastswitch = atoi(get_val(key->value2, 0));

    key = find_key("armorprotect", 1, items, count);
    if (key)
        armorprotect = atoi(get_val(key->value2, 0));

    key = find_key("healthprotect", 1, items, count);
    if (key)
        healthprotect = atoi(get_val(key->value2, 0));

    key = find_key("fallingdamage", 1, items, count);
    if (key)
        fallingdamage = atoi(get_val(key->value2, 0));

    key = find_key("allowvotingarmor", 1, items, count);
    if (key)
        allow_voting_armor = atoi(get_val(key->value2, 0));

    key = find_key("allowvotinghealth", 1, items, count);
    if (key)
        allow_voting_health = atoi(get_val(key->value2, 0));

    key = find_key("allowvotingminping", 1, items, count);
    if (key)
        allow_voting_minping = atoi(get_val(key->value2, 0));

    key = find_key("allowvotingmaxping", 1, items, count);
    if (key)
        allow_voting_maxping = atoi(get_val(key->value2, 0));

    key = find_key("allowvotingplayersperteam", 1, items, count);
    if (key)
        allow_voting_playersperteam = atoi(get_val(key->value2, 0));

    key = find_key("allowvotingrounds", 1, items, count);
    if (key)
        allow_voting_rounds = atoi(get_val(key->value2, 0));

    key = find_key("allowvotingmaxteams", 1, items, count);
    if (key)
        allow_voting_maxteams = atoi(get_val(key->value2, 0));

    key = find_key("allowvotingarmorprotect", 1, items, count);
    if (key)
        allow_voting_armorprotect = atoi(get_val(key->value2, 0));

    key = find_key("allowvotinghealthprotect", 1, items, count);
    if (key)
        allow_voting_healthprotect = atoi(get_val(key->value2, 0));

    key = find_key("allowvotingshotgun", 1, items, count);
    if (key)
        allow_voting_shotgun = atoi(get_val(key->value2, 0));

    key = find_key("allowvotingsupershotgun", 1, items, count);
    if (key)
        allow_voting_supershotgun = atoi(get_val(key->value2, 0));

    key = find_key("allowvotingmachinegun", 1, items, count);
    if (key)
        allow_voting_machinegun = atoi(get_val(key->value2, 0));

    key = find_key("allowvotingchaingun", 1, items, count);
    if (key)
        allow_voting_chaingun = atoi(get_val(key->value2, 0));

    key = find_key("allowvotinggrenadelauncher", 1, items, count);
    if (key)
        allow_voting_grenadelauncher = atoi(get_val(key->value2, 0));

    key = find_key("allowvotingrocketlauncher", 1, items, count);
    if (key)
        allow_voting_rocketlauncher = atoi(get_val(key->value2, 0));

    key = find_key("allowvotinghyperblaster", 1, items, count);
    if (key)
        allow_voting_hyperblaster = atoi(get_val(key->value2, 0));

    key = find_key("allowvotingrailgun", 1, items, count);
    if (key)
        allow_voting_railgun = atoi(get_val(key->value2, 0));

    key = find_key("allowvotingbfg", 1, items, count);
    if (key)
        allow_voting_bfg = atoi(get_val(key->value2, 0));

    key = find_key("allowvotingfallingdamage", 1, items, count);
    if (key)
        allow_voting_fallingdamage = atoi(get_val(key->value2, 0));

    key = find_key("lockarena", 1, items, count);
    if (key)
        lock_arena = atoi(get_val(key->value2, 0));

    key = find_key("competitionmode", 1, items, count);
    if (key)
        competition_mode = atoi(get_val(key->value2, 0));

    key = find_key("damagescoring", 1, items, count);
    if (key)
        damage_scoring = atoi(get_val(key->value2, 0));

    // `bots` is per arena and defaults ON, so a server that has never
    // heard of this key behaves exactly as it did: `botfill` alone still
    // decides, and every arena remains a place bots may be sent.  Turning it
    // off is how one arena is kept for people while the rest of the map fills.
    key = find_key("bots", 1, items, count);
    if (key)
        arena_bots = atoi(get_val(key->value2, 0));

    // ...and whether the people in the arena may move that switch themselves.
    // OFF by default, which is the conservative half of the donor's own
    // convention -- `arena.cfg` ships `allowvotingarmorprotect: 0` in the same
    // spirit -- because a vote that can turn the server's bots off is a bigger
    // lever than a vote that changes the starting armour.
    key = find_key("allowvotingbots", 1, items, count);
    if (key)
        allow_voting_bots = atoi(get_val(key->value2, 0));

    // ON by default, which is where the donor's nine per-weapon
    // switches sit too (`allow_voting_shotgun = 1` and its eight siblings) --
    // so a pack weapon an arena grants is as votable as a baseq2 one.
    key = find_key("allowvotingpackweapons", 1, items, count);
    if (key)
        allow_voting_packweapons = atoi(get_val(key->value2, 0));
}

void set_config(int first, int last)
{
    int     i;
    // THE DEPLOYMENT'S OWN SWITCH, and the whole point of it is that it is not
    // in arena.cfg.  `allowvotingbots` is a per-arena key like the other
    // twenty, so whether the people on a server may propose playing without
    // bots is decided by a file that is also the map rotation, the arena
    // geometry and every weapon table -- a file an operator replaces wholesale
    // when the rotation changes, and one that ships per-map and per-arena
    // blocks able to withdraw the key three layers down.  A server that fills
    // itself with bots wants that answer to survive all of it.
    //
    // Registered here rather than in load_config() because set_config() has a
    // second caller -- arena.c's reset when an arena empties -- that reaches it
    // without a file read; `gi.cvar` returns the existing cvar when there is
    // one, so the value an operator set in a config exec'd at startup is what
    // this reads.  R-RA-13.
    cvar_t  *votebots = gi.cvar("ra_allowvotingbots", "0", 0);

    i = first;
    if (i > last)
        return;

    for (; i <= last; i++) {
        // 0xff is the donor's -- every baseq2 weapon but the BFG.  The pack
        // half is every weapon of a layer that is ON, so switching a content
        // layer on arms its weapons instead of only drawing their menu rows;
        // an arena that wants none says `nopack` (see get_settings above).
        weapons = 0xff | RA_LayerWeaponBits();
        armor = 200;
        health = 100;
        minping = 0;
        maxping = 1000;
        playersperteam = 1;
        if (!idmap)
            rounds = 1;
        else
            rounds = 9;
        max_teams = 128;
        if (!idmap)
            pickup = 0;
        else
            pickup = 1;
        rocket_speed = 650;
        shells = 100;
        bullets = 200;
        slugs = 50;
        grenades = 50;
        rockets = 50;
        cells = 150;
        // Each pack ammo defaults to its own item ceiling, which is the
        // shape `shells`/`bullets` above already have.  An arena that grants
        // none of the pack weapons carries them the way a stock arena already
        // carries 150 cells with no cell weapon on it -- the donor's own
        // behaviour rather than a new one.
        magslug = 50;
        flechettes = 200;
        prox = 50;
        tesla = 50;
        trap = 5;
        fastswitch = 1;
        armorprotect = 2;
        healthprotect = 1;
        fallingdamage = 1;
        allow_voting_armor = 1;
        allow_voting_health = 1;
        allow_voting_minping = 1;
        allow_voting_maxping = 1;
        allow_voting_playersperteam = 1;
        allow_voting_rounds = 1;
        allow_voting_maxteams = 1;
        allow_voting_armorprotect = 1;
        allow_voting_healthprotect = 1;
        allow_voting_shotgun = 1;
        allow_voting_supershotgun = 1;
        allow_voting_machinegun = 1;
        allow_voting_chaingun = 1;
        allow_voting_grenadelauncher = 1;
        allow_voting_rocketlauncher = 1;
        allow_voting_hyperblaster = 1;
        allow_voting_railgun = 1;
        allow_voting_bfg = 1;
        allow_voting_fallingdamage = 1;
        lock_arena = 0;
        competition_mode = 0;
        damage_scoring = 0;
        arena_bots = 1;
        allow_voting_bots = 0;
        allow_voting_packweapons = 1;

        get_settings(definition_blocks, num_definition_blocks);

        if (map_block)
            get_settings(map_block->value2, map_block->count2);

        if (map_block && arena_blocks[i])
            get_settings(arena_blocks[i]->value2, arena_blocks[i]->count2);

        // AFTER all three layers, which is what makes it independent of them.
        // Applied to the global rather than to `arenas[i]` below so that it is
        // the value every consumer of this pass sees, and one way only: the
        // cvar is a floor the deployment puts under the file, not a second
        // place to turn the vote off.  A server that wants no bot vote says
        // nothing here and the file decides, exactly as before.  R-RA-13.
        if (votebots->value != 0)
            allow_voting_bots = 1;

        arenas[i].weapons = weapons;
        arenas[i].armor = armor;
        arenas[i].health = health;
        arenas[i].minping = minping;
        arenas[i].maxping = maxping;
        arenas[i].playersperteam = playersperteam;
        arenas[i].rounds = rounds;
        arenas[i].maxteams = max_teams;
        arenas[i].idarena = pickup;
        arenas[i].rocket_speed = rocket_speed;
        arenas[i].shells = shells;
        arenas[i].bullets = bullets;
        arenas[i].slugs = slugs;
        arenas[i].grenades = grenades;
        arenas[i].rockets = rockets;
        arenas[i].cells = cells;
        arenas[i].magslug = magslug;
        arenas[i].flechettes = flechettes;
        arenas[i].prox = prox;
        arenas[i].tesla = tesla;
        arenas[i].trap = trap;
        arenas[i].fastswitch = fastswitch;
        arenas[i].armorprotect = armorprotect;
        arenas[i].healthprotect = healthprotect;
        arenas[i].fallingdamage = fallingdamage;
        arenas[i].allow_voting_armor = allow_voting_armor;
        arenas[i].allow_voting_health = allow_voting_health;
        arenas[i].allow_voting_minping = allow_voting_minping;
        arenas[i].allow_voting_maxping = allow_voting_maxping;
        arenas[i].allow_voting_playersperteam = allow_voting_playersperteam;
        arenas[i].allow_voting_rounds = allow_voting_rounds;
        arenas[i].allow_voting_maxteams = allow_voting_maxteams;
        arenas[i].allow_voting_armorprotect = allow_voting_armorprotect;
        arenas[i].allow_voting_healthprotect = allow_voting_healthprotect;
        arenas[i].allow_voting_shotgun = allow_voting_shotgun;
        arenas[i].allow_voting_supershotgun = allow_voting_supershotgun;
        arenas[i].allow_voting_machinegun = allow_voting_machinegun;
        arenas[i].allow_voting_chaingun = allow_voting_chaingun;
        arenas[i].allow_voting_grenadelauncher = allow_voting_grenadelauncher;
        arenas[i].allow_voting_rocketlauncher = allow_voting_rocketlauncher;
        arenas[i].allow_voting_hyperblaster = allow_voting_hyperblaster;
        arenas[i].allow_voting_railgun = allow_voting_railgun;
        arenas[i].allow_voting_bfg = allow_voting_bfg;
        arenas[i].allow_voting_fallingdamage = allow_voting_fallingdamage;
        arenas[i].locked = lock_arena;
        arenas[i].competition = competition_mode;
        arenas[i].scorebydamage = damage_scoring;
        arenas[i].changed = 0;
        arenas[i].bots = arena_bots;
        arenas[i].allow_voting_bots = allow_voting_bots;
        arenas[i].allow_voting_packweapons = allow_voting_packweapons;
    }
}

int ra_isalnum(char ch)
{
    int c;

    c = ch;

    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
        return 1;

    return 0;
}

char *next_token(char *str)
{
    static char *token = NULL;
    static char foo[CONFIG_TOKEN_SIZE];
    char        *out;
    char        c;

    if (str)
        token = str;
    else if (!token)
        return NULL;

    if (!*token || *token == '\n')
        return NULL;

    out = foo;

    if (!ra_isalnum(*token)) {
        *out++ = c = *token++;

        if (*token == '/' && c == '/')
            *out++ = *token++;

        *out = 0;
        return foo;
    }

    while (ra_isalnum(*token))
        *out++ = *token++;
    *out = 0;
    return foo;
}

definition_t *new_def_block(void)
{
    return gi.TagMalloc(sizeof(definition_t) * MAX_DEFS, TAG_LEVEL);
}

char *new_val_block(void)
{
    char    *buf;

    buf = gi.TagMalloc(VAL_BLOCK_SIZE, TAG_LEVEL);
    buf[0] = 0;

    return buf;
}

static bool add_val(char *dest, const char *token)
{
    if (strlen(dest) + strlen(token) + 1 >= VAL_BLOCK_SIZE) {
        gi.dprintf("Error reading config file: definition value exceeds %d characters\n",
                   VAL_BLOCK_SIZE - 1);
        return false;
    }

    Q_strlcat(dest, " ", VAL_BLOCK_SIZE);
    Q_strlcat(dest, token, VAL_BLOCK_SIZE);
    return true;
}

definition_t *new_def_item(definition_t *item)
{
    item->count = 0;
    item->count2 = 0;
    item->value = new_val_block();
    item->type = 0;

    return item;
}

/*
 * Read one whitespace-delimited source token without treating an overlong
 * token as several valid ones.  A field width on fscanf() would keep the
 * write bounded but leave its tail for the next parse, changing the config.
 *
 *  1: token read, 0: clean EOF, -1: read or length failure.
 */
static int read_raw_token(FILE *fp, char *out, size_t out_size)
{
    int     c;
    size_t  len;

    do {
        c = fgetc(fp);
    } while (c != EOF && isspace((unsigned char)c));

    if (c == EOF)
        return ferror(fp) ? -1 : 0;

    len = 0;
    do {
        if (len + 1 >= out_size) {
            do {
                c = fgetc(fp);
            } while (c != EOF && !isspace((unsigned char)c));
            gi.dprintf("Error reading config file: token exceeds %d characters\n",
                       (int)out_size - 1);
            return -1;
        }
        out[len++] = c;
        c = fgetc(fp);
    } while (c != EOF && !isspace((unsigned char)c));

    out[len] = 0;
    if (c != EOF && ungetc(c, fp) == EOF)
        return -1;

    return ferror(fp) ? -1 : 1;
}

static bool new_def_item_checked(definition_t **cur, definition_t **cursor,
                                 int count)
{
    if (*cur)
        return true;

    if (count >= MAX_DEFS) {
        gi.dprintf("Error reading config file: definition block exceeds %d entries\n",
                   MAX_DEFS);
        return false;
    }

    *cur = new_def_item(*cursor);
    (*cursor)++;
    return true;
}

static bool finish_read_block(int depth, int mode, const definition_t *cur,
                              int count, int *result)
{
    if (depth) {
        gi.dprintf("Error reading config file: unbalanced {}\n");
        return false;
    }
    if (mode || cur) {
        gi.dprintf("Error reading config file: unterminated definition\n");
        return false;
    }

    *result = count;
    return true;
}

static bool read_block(FILE *fp, definition_t *cursor, int *result)
{
    definition_t    *cur;
    int             depth;
    int             mode;
    int             count;
    char            *tok;
    int             c;

    depth = 0;
    count = 0;
    cur = NULL;
    mode = 0;

    while (1) {
        int status = read_raw_token(fp, line, CONFIG_TOKEN_SIZE);

        if (status < 0) {
            if (ferror(fp))
                gi.dprintf("Error reading config file\n");
            return false;
        }
        if (!status) {
            return finish_read_block(depth, mode, cur, count, result);
        }

        for (tok = next_token(line); tok; tok = next_token(NULL)) {
            if (tok[0] == '/' && tok[1] == '/') {
                do {
                    c = fgetc(fp);
                    if (c == EOF) {
                        if (ferror(fp))
                            gi.dprintf("Error reading config file\n");
                        else
                            return finish_read_block(depth, mode, cur, count,
                                                     result);
                        return false;
                    }
                } while (c != '\n');

                break;
            }

            if (!mode) {
                if (*tok == '{') {
                    if (!new_def_item_checked(&cur, &cursor, count))
                        return false;
                    if (depth >= (int)q_countof(stack)) {
                        gi.dprintf("Error reading config file: nesting exceeds %d levels\n",
                                   (int)q_countof(stack));
                        return false;
                    }

                    cur->type = 2;
                    cur->value2 = new_def_block();

                    stack[depth].count = count;
                    stack[depth].cursor = cursor;
                    stack[depth].item = cur;

                    count = 0;
                    cursor = cur->value2;
                    cur = NULL;
                    depth++;
                } else if (*tok == ':') {
                    if (!new_def_item_checked(&cur, &cursor, count))
                        return false;
                    cur->type = 1;
                    cur->value2 = new_val_block();
                    mode = 1;
                } else if (*tok == '}') {
                    if (!depth) {
                        gi.dprintf("Error reading config file: unbalanced {}\n");
                        return false;
                    }

                    depth--;
                    cur = stack[depth].item;
                    cur->count2 = count;
                    cursor = stack[depth].cursor;
                    count = stack[depth].count;

                    cur = NULL;
                    mode = 0;
                    count++;
                } else {
                    if (!new_def_item_checked(&cur, &cursor, count))
                        return false;
                    if (!add_val(cur->value, tok))
                        return false;
                    cur->count++;
                }
            } else if (mode == 1) {
                if (*tok == ';') {
                    mode = 0;
                    count++;
                    cur = NULL;
                } else {
                    if (!add_val(cur->value2, tok))
                        return false;
                    cur->count2++;
                }
            }
        }
    }
}

bool read_config(FILE *fp)
{
    definition_blocks = new_def_block();
    line = gi.TagMalloc(CONFIG_TOKEN_SIZE, TAG_LEVEL);
    num_definition_blocks = 0;

    if (!definition_blocks || !line) {
        gi.dprintf("Error reading config file: allocation failed\n");
        definition_blocks = NULL;
        line = NULL;
        return false;
    }

    return read_block(fp, definition_blocks, &num_definition_blocks);
}

definition_t *find_key(char *key, int type, definition_t *items, int count)
{
    int     i;
    char    buf[1024];
    char    *tok;

    for (i = 0; i < count; i++) {
        if (items[i].type == type) {
            Q_strlcpy(buf, items[i].value, sizeof(buf));

            for (tok = strtok(buf, " "); tok; tok = strtok(NULL, " "))
                if (!strcmp(tok, key))
                    return &items[i];
        }
    }

    return NULL;
}

void list_keys(edict_t *ent)
{
    definition_t    *items;
    definition_t    *key;
    int             i;
    int             count;
    int             argc;
    char            path[1024];

    count = num_definition_blocks;
    items = definition_blocks;
    argc = gi.argc();
    path[0] = 0;

    for (i = 1; i < argc; i++) {
        key = find_key(gi.argv(i), 2, items, count);

        if (!key) {
            gi.cprintf(ent, PRINT_HIGH, "Block not found: %s\n", gi.argv(i));
            return;
        }

        items = key->value2;
        count = key->count2;
    }

    for (i = 0; i < count; i++) {
        Q_strlcat(path, items[i].value, sizeof(path));
        Q_strlcat(path, "  ", sizeof(path));

        if (items[i].type == 1)
            Q_strlcat(path, va("V  %s\n", (char *)items[i].value2), sizeof(path));
        else if (items[i].type == 2)
            Q_strlcat(path, "B\n", sizeof(path));
        else
            Q_strlcat(path, "U\n", sizeof(path));
    }

    gi.cprintf(ent, PRINT_HIGH, "%s", path);
}

void load_config(int num_arenas)
{
    FILE            *fp;
    char            path[MAX_OSPATH];
    definition_t    *key, *block;
    int             i;

    arenacfg = gi.cvar("arenacfg", "arena.cfg", 0);

    // every block below hangs off TAG_LEVEL memory the engine has already
    // freed, so clear it before the read -- an unreadable arena.cfg returns
    // early, and the previous level's pointers must not outlive it
    map_loop = NULL;
    map_block = NULL;
    arena_blocks = NULL;
    definition_blocks = NULL;
    num_definition_blocks = 0;
    votetries_setting = 3;
    allow_grapple = false;
    line = NULL;

    // `<homedir-or-basedir>/<gamedir>/<name>`, not the donor's bare
    // `<gamedir>/<name>`: that resolves against the server's working directORY,
    // so a server started from anywhere but the installation read no arena.cfg
    // at all -- every per-arena setting silently back to its built-in default,
    // announced by one dprintf nobody is reading at map load.
    //
    // This is the same correction ra2stats.c and gslog.c already carry, with
    // the same reasoning written against them; these two were the ones the
    // sweep missed.  `path` grows with it -- 80 bytes could not hold an
    // absolute path and the composer says so by failing.
    if (!G_FsGamePath(path, sizeof(path), arenacfg->string)) {
        gi.dprintf("Error: arena config path too long for %s\n",
                   arenacfg->string);
        return;
    }

    fp = fopen(path, "r");
    if (!fp) {
        gi.dprintf("Error: Couldn't read %s\n", path);
        return;
    }

    if (!read_config(fp)) {
        fclose(fp);
        definition_blocks = NULL;
        num_definition_blocks = 0;
        line = NULL;
        gi.dprintf("Error: Couldn't parse %s\n", path);
        return;
    }
    fclose(fp);

    arena_blocks = gi.TagMalloc(sizeof(definition_t *) * num_arenas, TAG_LEVEL);

    block = find_key(level.mapname, 2, definition_blocks, num_definition_blocks);

    if (block) {
        gi.dprintf("arena.cfg info for map found: %s\n", level.mapname);
        map_block = block;

        for (i = 0; i < num_arenas; i++)
            arena_blocks[i] = find_key(va("%d", i), 2, block->value2, block->count2);
    } else {
        gi.dprintf("arena.cfg info for map not found: %s\n", level.mapname);
        map_block = 0;
    }

    key = find_key("votetries", 1, definition_blocks, num_definition_blocks);
    if (key)
        votetries_setting = atoi(get_val(key->value2, 0));

    key = find_key("grapple", 1, definition_blocks, num_definition_blocks);
    if (key)
        allow_grapple = atoi(get_val(key->value2, 0));

    map_loop = find_key("maploop", 1, definition_blocks, num_definition_blocks);

    if (map_loop)
        gi.dprintf("Map loop read\n");
}

char *get_next_map(char *current)
{
    int     i;
    char    *val;

    if (!map_loop)
        return NULL;

    if (!has_val(map_loop->value2, current))
        return get_val(map_loop->value2, 0);

    for (i = 0; i < map_loop->count2; i++) {
        val = get_val(map_loop->value2, i);

        if (!strcmp(current, val)) {
            val = get_val(map_loop->value2, i + 1);

            if (!strlen(val))
                return get_val(map_loop->value2, 0);

            return val;
        }
    }

    return level.mapname;
}

void print_map_loop(edict_t *ent)
{
    if (map_loop)
        gi.cprintf(ent, PRINT_MEDIUM, "%s\n", (char *)map_loop->value2);
    else
        gi.cprintf(ent, PRINT_MEDIUM, "No map loop set\n");
}

void load_motd(void)
{
    FILE        *fp;
    struct stat st;
    char        *buf;
    char        *p;
    char        *end;
    size_t      size;
    motd_t      *node;
    char        path[MAX_OSPATH];

    motd.next = motd.prev = NULL;

    // The second of the two.  See load_config() above for why the
    // donor's relative path cannot be kept.
    if (!G_FsGamePath(path, sizeof(path), "motd.txt")) {
        gi.dprintf("Error: motd path too long\n");
        return;
    }

    fp = fopen(path, "r");
    if (!fp) {
        gi.dprintf("Error: Couldn't read %s\n", path);
        return;
    } else
        gi.dprintf("Sucessfully read %s\n", path);

    if (fstat(fileno(fp), &st)) {
        gi.dprintf("Error: Couldn't stat %s\n", path);
        fclose(fp);
        return;
    }

    size = st.st_size + 2;

    buf = gi.TagMalloc(size, TAG_LEVEL);
    if (!buf) {
        gi.dprintf("Error: Couldn't malloc %d\n", (int)st.st_size);
        fclose(fp);
        return;
    }

    p = buf;
    end = buf + size;

    // one line per node, each terminated in place, so the running pointer has
    // to stay inside the single allocation the whole file was sized for
    while (p + 1 < end && fgets(p, end - p, fp)) {
        size_t  len = strlen(p);

        if (!len)
            break;

        if (p[len - 1] == '\n')
            p[--len] = 0;

        node = gi.TagMalloc(sizeof(motd_t), TAG_LEVEL);
        node->line = p;
        add_to_queue((qmenu_t *)node, (qmenu_t *)&motd);

        p += len + 1;
    }

    fclose(fp);
}
