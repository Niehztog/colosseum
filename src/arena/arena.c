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
// Rocket Arena 2 v2.25, from rocketarena2-public@d20e1ce.  RA2 has no baseq2
// counterpart, so it lives in src/arena/ rather than being merged into a spine
// file.  The reconstruction's asm-matching address comments are stripped.
#include "g_local.h"
#include "arena/arena.h"
#include "ctf/g_ctf.h"
#include "arena/ra2stats.h"
// One `botfill` for every ruleset, so the answer to "is the fill on" comes from
// where the fill lives.
#include "bot/bl_main.h"
// BotDestroy(), for the bot an arena has voted out that nowhere else will take.
#include "bot/bl_spawn.h"

// No local `extern` of an arena object: arena.h already declares
// `votetries_setting`, and a second declaration in a .c file is the shape that
// let RA2's `extern int botglobals;` resolve a four-byte int over the first
// member of a struct.
bool    allow_grapple;
bool    broken = false;

arena_t     arenas[MAX_ARENAS + 1];
int         num_arenas;
bool    idmap;

qmenu_t     *teams;

motd_t      motd;
cvar_t      *admincode;
cvar_t      *ra_playercycle;
cvar_t      *ra_botcycle;


char        *teamskins[MAX_ARENA_SKINS] = {
    "r2red", "r2blue", "r2dgre", "r2oran", "r2yell", "r2aqua", "r2lgre"
};

char        *vwepmodels[4] = {
    "male", "female", "cyborg", "crakhor"
};

int    teamskins_precachem[MAX_ARENA_SKINS];
int    teamskins_precachef[MAX_ARENA_SKINS];
int    teamskins_precachecw[MAX_ARENA_SKINS];
int    teamskins_precachecb[MAX_ARENA_SKINS];

char        *omode_descriptions[4] = {
    "Normal", "Free Flying", "Trackcam", "In Eyes"
};


void        teleporter_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf);

void        P_ProjectSource(gclient_t *client, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result);

void        load_config(int numarenas);
void        set_config(int first, int last);
void        load_motd(void);

void        show_observer_menu(edict_t *ent);
void        show_arena_menu(edict_t *ent);
void        show_teamconfirm_menu(edict_t *ent, int arenanum);

void
add_to_queue(qmenu_t *node, qmenu_t *head)
{
    for (; head->next; head = head->next)
        ;

    head->next = node;
    node->prev = head;
    node->next = NULL;
}

qmenu_t *
remove_from_queue(qmenu_t *node, qmenu_t *head)
{
    if (!node) {
        if (head)
            node = head->next;

        if (!node)
            return NULL;
    }

    if (node->prev)
        node->prev->next = node->next;
    if (node->next)
        node->next->prev = node->prev;

    node->prev = NULL;
    node->next = NULL;

    return node;
}

void
add_to_front_queue(qmenu_t *node, qmenu_t *head)
{
    remove_from_queue(node, NULL);

    node->prev = head;
    node->next = head->next;
    if (head->next)
        head->next->prev = node;
    head->next = node;
}

int count_queue(qmenu_t *head)
{
    int     count;

    count = 0;
    while (head->next) {
        head = head->next;
        count++;
    }

    return count;
}

int count_players_queue(qmenu_t *head)
{
    int         count;

    count = 0;
    while (head->next) {
        head = head->next;
        if (((edict_t *)head->it)->client->resp.fightstate == FIGHT_ALIVE)
            count++;
    }

    return count;
}

static q_unused team_t *TeamFromNode(qmenu_t *node)
{
    return (team_t *)((qmenu_t *)node->it)->it;
}

void set_damage(int arenanum, int state)
{
    qmenu_t     *tnode, *mnode;
    edict_t     *e;

    tnode = &arenas[arenanum].activeteams;

    while (tnode->next) {
        tnode = tnode->next;

        mnode = (qmenu_t *)tnode->it;

        while (mnode->next) {
            mnode = mnode->next;
            e = (edict_t *)mnode->it;
            if (e->client->resp.fightstate)
                e->takedamage = state;
        }
    }
}

/*
==================
RA_RoundFighting

Is the round in this client's arena actually being fought?

RA2 grants damage exactly ONCE per round and takes it away again: arena_think()
calls set_damage(arena, DAMAGE_AIM) on the frame the countdown reaches zero and
set_damage(arena, DAMAGE_NO) at ASTATE_NEXTROUND, so a fighter standing in a
countdown is `takedamage DAMAGE_NO` for the whole of it.  Every shot fired
before ASTATE_FIGHTING is therefore spent ammo and nothing else -- and the ammo
is the round's, handed out once by give_ammo() and not replaced until the next
one.

A person does not need to be told: the countdown is on their HUD and they can
hear it.  The brain has no concept of a round, sees an opponent standing five
feet away, and opens fire -- which is what this answers.

Arena 0 is the lobby: nothing in it can be damaged, so the answer there is no.
A context outside 1..num_arenas is not RA2's state at all and fails open.
==================
*/
bool RA_RoundFighting(edict_t *ent)
{
    int ctx;

    if (!ent->client)
        return true;

    ctx = ent->client->resp.context;

    if (ctx < 0 || ctx > num_arenas)
        return true;

    return ctx && arenas[ctx].state == ASTATE_FIGHTING;
}

/*
==================
RA_HoldFire

Should this client's ATTACK be taken away right now?  A person standing in a
countdown could empty a magazine into an opponent that cannot be hurt --
spending ammo `give_ammo` hands out once a round.

Gladiator's answer cannot be taken verbatim.  Its p_client.c guards both
`Think_Weapon` call sites with `!(ra->value && ent->takedamage == DAMAGE_NO)`,
and one of those two is `ClientBeginServerFrame`'s, which runs every frame and
is the weapon state machine's heartbeat: `Use_Weapon` sets `newweapon`,
`Weapon_Generic` walks WEAPON_DROPPING down to `ChangeWeapon`, and RA2's
`fastswitch` only skips the raise animation rather than moving the switch off
that path.  So that clause freezes weapon selection for the length of the
countdown -- and choosing a weapon during the countdown is not incidental to
Rocket Arena, it is how a round is prepared.

Rocket Arena has a gate of its own, one level down: RA2 adds `ent->takedamage
&& arenas[...].state == ASTATE_FIGHTING` inside four `p_weapon.c` fire arms
(`Weapon_Generic` 417, `Weapon_HyperBlaster_Fire` 818, `Machinegun_Fire` 897,
`Chaingun_Fire` 997), at the pin `d20e1ce` as well as at the bundle tip.  So
RA2 gates the firing, which is what this gates, one level up.

Taking its four verbatim would have been a regression here: this tree has nine
sites where a press becomes a shot, RA2's four cover four of them, and the five
it leaves -- `Throw_Generic` (the hand grenade and the tesla), the Trap, the
chainfist, the ETF rifle and the plasma beam -- are every one reachable under
`arena`, three by a pack grant bit and two by the ammunition that is also the
weapon.  The one thing RA2 does that this does not is queue the press: its
clear of `latched_buttons` sits inside the gated arm, so a tap during a
countdown fires on the bell.

So the think keeps running and the button is what goes, which is what the bot
side of the seam does too.  Every weaponthink in the merged tree -- baseq2's,
the mission packs' and Threewave's -- reads `client->buttons` or
`client->latched_buttons`, so clearing the bit at the latch reaches all of them
without a per-weapon gate.

Two exemptions, and both are somebody else's key.  An observer's ATTACK is what
cycles RA2's four camera modes, and that press is already stopped from reaching
a weapon; taking the button away here would take the camera with it.  And the
grapple is fired with the same button and is movement rather than damage, so a
player holding one keeps it -- otherwise `allow_grapple` would lose its hook
for the whole countdown.  The bot gate makes the same exemption, for the same
reason.
reason.
==================
*/
bool RA_HoldFire(edict_t *ent)
{
    const gitem_t *w;

    if (!ent->client)
        return false;
    if (ent->client->resp.fightstate == FIGHT_SPECTATING)
        return false;
    if (RA_RoundFighting(ent))
        return false;

    w = ent->client->pers.weapon;

    return !w || !w->classname || Q_stricmp(w->classname, "weapon_grapple");
}

/*
==================
give_pack_ammo

One of the five mission-pack ammunition grants, gated on the layer it
belongs to.

Why the gate, when the three ordinary ammunitions could have done without one.
`ammo_tesla` and `ammo_trap` are the WEAPON as well as the ammunition -- the
donor treats `ammo_grenades` the same way, which is why neither has a `weapons:`
bit -- so a count of them is not a number on a HUD, it is an item in the weapon
cycle.  Ungated, every arena on every server handed out fifty Teslas (Ground
Zero) and five Traps (Reckoning), including a server running NEITHER pack, and
`weapnext` walked through both.  Measured in a play test on `xatrix 1`, on
`rogue 1` and with both off; all three gave both.

Zero rather than skip.  The inventory is carried across a round -- give_ammo is
an assignment, not an addition -- so a layer that is off has to take the item
away rather than merely decline to add one.
==================
*/
static void give_pack_ammo(edict_t *e, const char *classname,
                           content_layer_t layer, int count)
{
    const gitem_t *it = FindItemByClassname(classname);

    if (!it)
        return;

    e->client->pers.inventory[ITEM_INDEX(it)] =
        G_LayerEnabled(layer) ? count : 0;
}

/*
==================
RA_ArenaGrantsMask

The weapon bits an arena actually hands out: its stored mask, minus every pack
weapon whose content layer is off.

one function because the difference between the stored mask and the honoured one
is a fact two callers need -- give_ammo(), which must not grant a Ground Zero
weapon on a server with no Ground Zero, and `sv arenadump`, whose whole job is to
report what an arena grants.  A diagnostic that recomputed it could be right
about a rule the game no longer follows.
==================
*/
int RA_ArenaGrantsMask(int arenanum)
{
    int i, mask;

    if (arenanum < 1 || arenanum > num_arenas || arenanum > MAX_ARENAS)
        return 0;

    // The bit is CARRIED rather than cleared: an arena.cfg is written once and
    // served by servers running the Reckoning, Ground Zero, both or neither, so
    // switching a layer back on must restore the arena as written -- and the
    // settings menu relies on the same carry (see RA_PackWeaponOffered).
    mask = arenas[arenanum].weapons;
    for (i = 0; i < RA_NUM_PACK_WEAPONS; i++)
        if (!RA_PackWeaponOffered(i))
            mask &= ~weapon_vals[9 + i];

    return mask;
}

void give_ammo(edict_t *e)
{
    const gitem_t   *w[RA_NUM_WEAPON_BITS];
    arena_t     *arena = &arenas[e->client->resp.context];
    //    0  2  3  4  5   6    9   8   7   -- and then the pack's six
    int         weapon_vals_x[RA_NUM_WEAPON_BITS] = {
        256, 1, 2, 4, 8, 16, 128, 64, 32,
        // The pack six, and not in bit order: the index is the preference and
        // the bit is a parallel value, which is a separation the donor's own
        // nine already use (w[6] is the railgun at bit 128, w[8] the rocket
        // launcher at bit 32).  Ordering these by bit made the CHAINFIST -- a
        // melee weapon -- the most preferred of the six, which a pack-only
        // arena promptly handed out; measured, then fixed.
        16384, 512, 4096, 2048, 1024, 8192,
    };
    const gitem_t   *it, *rl;
    bool    needswitch;
    int         i, pass;
    int         allowed;

    // give health
    if (arena->health)
        e->health = arena->health;
    else
        e->health = 100;

    // give weapons
    rl = NULL;
    memset(w, 0, sizeof(w));

    w[0] = FindItemByClassname("weapon_bfg");
    w[1] = FindItemByClassname("weapon_shotgun");
    w[2] = FindItemByClassname("weapon_supershotgun");
    w[3] = FindItemByClassname("weapon_machinegun");
    w[4] = FindItemByClassname("weapon_chaingun");
    w[5] = FindItemByClassname("weapon_grenadelauncher");
    w[6] = FindItemByClassname("weapon_railgun");
    w[7] = FindItemByClassname("weapon_hyperblaster");
    w[8] = FindItemByClassname("weapon_rocketlauncher");
    // The pack six, in ascending preference -- see weapon_vals_x above.
    w[9]  = FindItemByClassname("weapon_chainfist");
    w[10] = FindItemByClassname("weapon_boomer");
    w[11] = FindItemByClassname("weapon_proxlauncher");
    w[12] = FindItemByClassname("weapon_etf_rifle");
    w[13] = FindItemByClassname("weapon_phalanx");
    w[14] = FindItemByClassname("weapon_plasmabeam");

    needswitch = false;

    allowed = RA_ArenaGrantsMask(e->client->resp.context);

    // Two passes, and the order is the point.  `rl` is the weapon a player
    // spawns holding and it is the first enabled one this loop meets, so the
    // donor's descending 8..0 encodes a preference: rocket launcher, then
    // hyperblaster, railgun, grenade launcher, chaingun, machinegun, super
    // shotgun, shotgun, BFG.  The pack six are walked afterwards rather than
    // appended to that descent, so one of them becomes the spawn weapon only in
    // an arena that grants no baseq2 weapon at all -- otherwise adding a
    // chainfist to an arena would have made it the thing you spawn holding.
    for (pass = 0; pass < 2; pass++) {
        int lo = pass ? 9 : 0;
        int hi = pass ? RA_NUM_WEAPON_BITS - 1 : 8;

        for (i = hi; i >= lo; i--) {
            // The donor indexes w[i] unguarded; every one of its nine exists, so
            // it never bit, but ITEM_INDEX(NULL) is an out-of-range write and
            // the table is longer now.
            if (!w[i])
                continue;

            if (allowed & weapon_vals_x[i]) {
                if (!rl)
                    rl = w[i];

                if (!e->client->pers.inventory[ITEM_INDEX(rl)] || needswitch) {
                    e->client->newweapon = rl;
                    e->client->pers.selected_item =
                        e->client->ps.stats[STAT_SELECTED_ITEM] = ITEM_INDEX(rl);
                    needswitch = false;
                }

                e->client->pers.inventory[ITEM_INDEX(w[i])] = 1;
            } else {
                if (e->client->pers.weapon == w[i])
                    needswitch = true;

                e->client->pers.inventory[ITEM_INDEX(w[i])] = 0;
            }
        }
    }

    if (needswitch) {
        rl = FindItemByClassname("weapon_blaster");
        e->client->newweapon = rl;
        e->client->pers.selected_item =
            e->client->ps.stats[STAT_SELECTED_ITEM] = ITEM_INDEX(rl);
    }

    // give ammo
    if ((it = FindItemByClassname("ammo_shells"))) e->client->pers.inventory[ITEM_INDEX(it)] = arena->shells;
    if ((it = FindItemByClassname("ammo_bullets"))) e->client->pers.inventory[ITEM_INDEX(it)] = arena->bullets;
    if ((it = FindItemByClassname("ammo_slugs"))) e->client->pers.inventory[ITEM_INDEX(it)] = arena->slugs;
    if ((it = FindItemByClassname("ammo_grenades"))) e->client->pers.inventory[ITEM_INDEX(it)] = arena->grenades;
    if ((it = FindItemByClassname("ammo_rockets"))) e->client->pers.inventory[ITEM_INDEX(it)] = arena->rockets;
    if ((it = FindItemByClassname("ammo_cells"))) e->client->pers.inventory[ITEM_INDEX(it)] = arena->cells;
    // The pack five, each behind its own layer -- see give_pack_ammo above.
    give_pack_ammo(e, "ammo_magslug",    LAYER_XATRIX, arena->magslug);
    give_pack_ammo(e, "ammo_trap",       LAYER_XATRIX, arena->trap);
    give_pack_ammo(e, "ammo_flechettes", LAYER_ROGUE,  arena->flechettes);
    give_pack_ammo(e, "ammo_prox",       LAYER_ROGUE,  arena->prox);
    give_pack_ammo(e, "ammo_tesla",      LAYER_ROGUE,  arena->tesla);

    // give body armor
    if ((it = FindItemByClassname("item_armor_body")))
        e->client->pers.inventory[ITEM_INDEX(it)] = arena->armor;

    if (allow_grapple) {
        it = FindItem("Grapple");
        if (it)
            e->client->pers.inventory[ITEM_INDEX(it)] = 1;
    }
}

team_t *add_to_team(edict_t *ent, char *teamname)
{
    int     i;
    team_t  *t;

    for (i = 0; i < MAX_TEAMS; i++) {
        if (!teams[i].it)
            continue;

        t = teams[i].it;
        if (strcmp(t->name, teamname))
            continue;

        if (t->arenanum) {
            if (count_queue(&teams[i]) == arenas[t->arenanum].playersperteam)
                return NULL;

            if (arenas[t->arenanum].locked)
                return NULL;

            if (t->fighting)
                RA2_Stats_AddPlayer(arenas[t->arenanum].stats, ent, i);
        }

        add_to_queue(&ent->client->resp.teammember, &teams[i]);
        ent->client->resp.teamnum = i;

        if (t->skin != -1)
            setteamskin(ent, ent->client->pers.userinfo, t->skin);

        gi.bprintf(PRINT_MEDIUM, "%s has been added to team %d (%s)\n",
                   ent->client->pers.netname, i, teamname);

        return t;
    }

    for (i = 0; i < MAX_TEAMS; i++)
        if (!teams[i].it)
            break;

    // A kept bug, guarded: RA2 has no `i == MAX_TEAMS` guard after this scan,
    // so a full teams[] falls out with i == MAX_TEAMS and `teams[i].it = t`
    // twelve lines down writes one past the end of a 256-entry array.
    // Unreachable while every team is created by a human typing a name, and
    // reachable once one is created per bot.
    if (i == MAX_TEAMS) {
        gi.dprintf("add_to_team: all %d team slots are in use\n", MAX_TEAMS);
        return NULL;
    }

    t = gi.TagMalloc(sizeof(team_t), TAG_LEVEL);
    if (!t) {
        gi.error("Ateam malloc failed!\n");
        return NULL;
    }

    t->name = teamname;
    t->teamnum = i;
    t->arenanum = 0;
    t->wins = -1;
    t->skin = -1;
    t->arenalink.it = &teams[i];
    t->fighting = 0;
    teams[i].it = t;

    if (ent) {
        add_to_queue(&t->arenalink, &arenas[0].waitingteams);
        t->locked = false;
        t->side = -1;

        add_to_queue(&ent->client->resp.teammember, &teams[i]);
        ent->client->resp.teamnum = i;

        gi.bprintf(PRINT_MEDIUM, "%s has created team number %d (%s)\n",
                   ent->client->pers.netname, i, teamname);
    } else
        t->locked = true;

    return t;
}

// A team name that no other team is using, in TAG_LEVEL memory because
// add_to_team stores the POINTER rather than copying it -- the string has to
// outlive the call and go away with the level.  add_to_team retains it only
// when it creates a team; this helper's unique name makes every successful
// call that case, and a rejected name is freed by its caller.
//
// Factored out of menuNewTeam, which is also the only reason its bound is now
// checked.  RA2 uniquifies by appending "!" and restarting the scan, into a
// 100-byte buffer, with no test that anything still fits: 256 teams named alike
// append 256 characters.  Unreachable while a human has to type each collision
// in by hand, and reachable once team creation is automatic and per bot.  A
// counted suffix cannot grow without bound.
char *RA_NewTeamName(edict_t *ent)
{
    char *name = gi.TagMalloc(ARENA_TEAMNAME_SIZE, TAG_LEVEL);
    int  suffix, i;

    if (!name)
        return NULL;

    for (suffix = 0; suffix < MAX_TEAMS; suffix++) {
        if (suffix)
            Q_snprintf(name, ARENA_TEAMNAME_SIZE, "%s's Team %d",
                       ent->client->pers.netname, suffix + 1);
        else
            Q_snprintf(name, ARENA_TEAMNAME_SIZE, "%s's Team",
                       ent->client->pers.netname);

        for (i = 0; i < MAX_TEAMS; i++)
            if (teams[i].it && !strcmp(TEAM(&teams[i])->name, name))
                break;
        if (i == MAX_TEAMS)
            return name;
    }

    gi.TagFree(name);
    return NULL;
}

// New bots are initialised into the selected arena's queue.  Both of the steps
// a human takes through the menus -- pick a team, pick an arena -- are taken
// here instead, because a bot has no menu: without this a bot connects, lands
// in arena 0 as a noclip observer with the team list open, and stays there for
// the whole level.  That is what it did.
//
// The arena is the `arena` userinfo key BotAddDeathmatch wrote from the `arena`
// cvar.  BotCreate also copies it into resp.context, and that copy is dead --
// ClientBeginDeathmatch calls InitClientResp, which memsets resp before
// PutClientInServer ever reads it.  The userinfo is the only carrier that
// survives, as it is for `ctfteam`.
//
// Called after placement rather than during it, from ClientBeginDeathmatch,
// because that is where a human's menu click lands: the bot is already a placed
// observer in arena 0, and this moves it exactly once, through RA2's own
// SendTeamToArena.  Doing it inside init_player() would mean either placing
// twice or open-coding SendTeamToArena's tail minus the move.
// Which arena a new bot joins.
//
// 1999's answer is the `arena` cvar, default 1, clamped to 1..num_arenas --
// Gladiator's bl_spawn.c copies it into the bot's userinfo and this tree does
// the same, faithfully.  It is right for a deathmatch map, which has exactly
// one arena, and useless on a real RA2 map: there a person is wherever they
// picked, the pickup arena is wherever `arena.cfg` says -- on `ra2map6` it is
// 8 -- and every bot goes to arena 1, so nobody meets anybody and no round
// starts anywhere.  Measured: four bots in arena 1, one person in arena 8.
//
// So 0 becomes "follow the people": the lowest-numbered arena with a human on
// on a team, then the lowest-numbered pickup arena, then 1.  Zero is free to
// mean that because 1999 clamped it to 1 and it therefore meant nothing, and a
// server that sets 1..N still gets 1999's behaviour exactly.  The choice is made
// at join time rather than when the bot was added -- and, since 1.30, is re-asked
// while the bot is still somewhere it can be moved from, because "at join time"
// is still too early for the first bot `minimumplayers` adds.  See
// RA_BotFollowPeople() below.

// The lowest-numbered arena a person is on a team in, or 0 if there is none.
// Split out of RA_AutoArena because RA_BotFollowPeople asks the same question
// and must be able to tell "arena 1, because somebody is in it" from "arena 1,
// because there was nobody to follow".
// half.
//
// `botsonly` narrows it to the people bots may join, which is the
// question every caller here actually has: an arena that has voted bots out is
// not an answer to "where should the bots be", and answering it anyway sends
// every bot to a door RA_BotArenaOpen then refuses.  Both forms are wanted, and
// by one function rather than two: the difference between "nobody is playing"
// and "everybody is playing somewhere bots are not welcome" is the whole of the
// decision in RA_AutoArena below, and two copies of this loop could disagree
// about it.
static int RA_HumanArena(bool botsonly)
{
    edict_t *e;
    int     i, n, human = 0;

    for (i = 0; i < game.maxclients; i++) {
        e = &g_edicts[i + 1];
        if (!e->inuse || !e->client)
            continue;
        if (e->flags & FL_BOT)
            continue;
        if (e->client->resp.teamnum < 0 || !teams[e->client->resp.teamnum].it)
            continue;
        n = TEAM(&teams[e->client->resp.teamnum])->arenanum;
        if (n < 1 || n > num_arenas)
            continue;
        if (botsonly && !arenas[n].bots)
            continue;
        if (!human || n < human)
            human = n;
    }

    return human;
}

// Does this arena have a person on a team in it?  Asked of the arena a bot is
// Already in, so that a bot playing with somebody is never taken away from
// them, however the numbering falls.
static bool RA_ArenaHasHuman(int arenanum)
{
    edict_t *e;
    int     i;

    for (i = 0; i < game.maxclients; i++) {
        e = &g_edicts[i + 1];
        if (!e->inuse || !e->client)
            continue;
        if (e->flags & FL_BOT)
            continue;
        if (e->client->resp.teamnum < 0 || !teams[e->client->resp.teamnum].it)
            continue;
        if (TEAM(&teams[e->client->resp.teamnum])->arenanum == arenanum)
            return true;
    }

    return false;
}

// The bot half of the question was RA_ArenaHasBot() here.  RA_StagingArena()
// was its only caller and now needs the COUNT and not the predicate -- which
// arena has most of them, rather than whether this one has any -- so it asks
// RA_ArenaPlayers() and this is gone rather than being kept beside a second
// walk of the same list.

// A bot-only team in this arena with room left on it, or NULL.
//
// An arena whose `playersperteam` is above one is a TEAM arena, and a bot that
// always starts a team of its own can never fill one: two bots meet as two
// teams of one and the round is played 1v1 whatever `arena.cfg` asked for.
// Fifteen of RA2's arenas declare 2 and `ra2map1` arena 5 declares 3, and none
// of them could ever be reached with bots.  So a bot looks for a team to join
// before it invents one.
//
// Bot-only, and that is the whole of the courtesy: a person's team is joined by
// a person clicking its name in "Choose your team" (menuAddtoTeam), and a bot
// letting itself in would be taking a seat on somebody's team uninvited.  An
// EMPTY team is skipped as well -- check_teams() frees those, and pairing with
// one would be pairing with nobody.
//
// Gated on `botfill` so that off means off: with a flat `minimumplayers` the
// four bots it adds to a 2v2 arena were four teams of one, two fighting and
// two queued, and that is what a server which asked for nothing keeps.
static team_t *RA_BotTeamWithRoom(int arenanum)
{
    qmenu_t *mnode;
    edict_t *e;
    int     i, members;
    bool    allbots;

    if (!BotFillEnabled())
        return NULL;
    if (arenas[arenanum].playersperteam <= 1)
        return NULL;

    for (i = 0; i < MAX_TEAMS; i++) {
        if (!teams[i].it)
            continue;
        if (TEAM(&teams[i])->arenanum != arenanum)
            continue;
        if (TEAM(&teams[i])->locked)
            continue;

        members = count_queue(&teams[i]);
        if (members < 1 || members >= arenas[arenanum].playersperteam)
            continue;

        allbots = true;
        mnode = &teams[i];
        while (mnode->next) {
            mnode = mnode->next;
            e = (edict_t *)mnode->it;
            if (!e || !(e->flags & FL_BOT)) {
                allbots = false;
                break;
            }
        }

        if (allbots)
            return TEAM(&teams[i]);
    }

    return NULL;
}

// Would a bot be seated if it asked to join this arena right now?  The silent
// half of RA_BotJoinArena's own checks: the join prints why it refused, and a
// move that is going to be refused must not remove the bot from the team it is
// already on first.
static bool RA_BotArenaOpen(int arenanum)
{
    int i;

    if (arenanum < 1 || arenanum > num_arenas)
        return false;

    // add_to_team refuses a locked arena whichever kind it is, so this is asked
    // before the split rather than inside one arm of it.
    if (arenas[arenanum].locked)
        return false;

    // An arena that has voted bots out is shut to them for the same
    // reason a locked one is shut to everybody -- and asked here, so that
    // RA_BotFollowPeople never walks a bot up to a door that will not open.
    if (!arenas[arenanum].bots)
        return false;

    if (arenas[arenanum].idarena) {
        if (!arenas[arenanum].pickupteam[0] || !arenas[arenanum].pickupteam[1])
            return false;
        return count_queue(arenas[arenanum].pickupteam[0]->arenalink.it) <
               arenas[arenanum].playersperteam ||
               count_queue(arenas[arenanum].pickupteam[1]->arenalink.it) <
               arenas[arenanum].playersperteam;
    }

    // A seat on a team that is already here costs neither a team slot nor a
    // place in the arena's team queue, so it is asked before both refusals.
    if (RA_BotTeamWithRoom(arenanum))
        return true;

    if (count_queue(&arenas[arenanum].waitingteams) +
        count_queue(&arenas[arenanum].activeteams) >= arenas[arenanum].maxteams)
        return false;

    // A non-pickup arena means a team of the bot's own, and `teams[]` is a
    // fixed 256.  Asked here rather than left to add_to_team, because by then
    // the bot has already been taken off the team it had.
    for (i = 0; i < MAX_TEAMS; i++)
        if (!teams[i].it)
            return true;

    return false;
}

// Is this bot's arena the operator's choice rather than ours?  `arena` 1..N is
// 1999's request and is obeyed literally; only `arena 0` -- "follow the people"
// -- is a request this function is allowed to re-answer.
static bool RA_BotFollowsPeople(edict_t *ent)
{
    int n = Q_atoi(Info_ValueForKey(ent->client->pers.userinfo, "arena"));

    return n < 1 || n > num_arenas;
}

// Where the bots belong when there is nobody to follow, or 0 -- there are
// people here, and following them is the answer instead.
//
// This is RA_AutoArena's staging tail, asked as its own question because the
// FILL has to ask it too.  `botfill` exists to give people opponents and an
// arena with nobody in it wants none (RA_BotFillTarget), and those two together
// said that an EMPTY SERVER wants no bots at all: every arena's target was 0
// until somebody arrived, so `botfill 1` stood the server empty and waited,
// while the flat `minimumplayers` beside it seats its four 3.2 seconds into
// the level and has since 1999.  One of the two sizing the server to the game
// and the other leaving it empty is a difference nobody asked for: the number
// is the arena's, but "which arena" is answerable with nobody on the map, and
// it has always been answered -- this is the same staging arena RA_AutoArena
// sends a bot to when it is added before anybody else has joined.  So the
// staging arena, and only it, has a target while the map is empty.
//
// The moment anybody is on a team anywhere this is 0 again and the question
// goes back to "follow the people": their arena is the one with a target and
// every other arena, this one included, wants nobody.  Which is what leaves
// the refusal above untouched -- people on the map with every arena they are
// in refusing bots is still 0 and not "some empty arena, then", because that
// answer was about a map with people on it and this is about one without.
//
// The arena the bots are ALREADY in comes first, and that is the last person
// leaving rather than the first arriving: the game they left keeps its players
// instead of being drained one bot per fill tick and rebuilt somewhere else.
static int RA_StagingArena(void)
{
    int i, nbots, best = 0, bestbots = 0;

    // There are people on this map, so they are what the bots follow and an
    // arena they are not in has nobody to give them to.  RA_AutoArena's own
    // `return 0` below, reached from the other side.
    if (RA_HumanArena(false))
        return 0;

    // The arena the bots are already in -- and when they are in more than one,
    // the arena that has MOST of them, because that is the game.  It was the
    // lowest-numbered one holding any bot at all, which is a coin toss a
    // visitor gets to decide: a person who joins a small arena pulls one bot
    // after them (RA_BotFollowPeople), and when they leave that stray is in
    // arena 1 while the game everybody else was playing is in arena 8.  Lowest
    // wins, the stray's arena becomes the staging arena, its ppt=1 target is
    // all the server will hold, and the ten-bot game is never rebuilt.
    // Measured live: a server stuck at two bots for the rest of the map.
    //
    // A strict `>` keeps the lowest-numbered arena on a tie, which is the old
    // answer everywhere the counts are equal -- one arena holding bots, the
    // case this loop was written for, included.
    //
    // RA_ArenaPlayers' transit accounting cannot disagree with a plain count
    // here: it remaps a bot to the arena it is walking to only when there is
    // somebody to follow, and the guard above has already established there is
    // not.
    for (i = 1; i <= num_arenas; i++) {
        if (!arenas[i].bots)
            continue;
        RA_ArenaPlayers(i, &nbots);
        if (nbots > bestbots) {
            bestbots = nbots;
            best = i;
        }
    }
    if (best)
        return best;

    // A pickup arena is the one a lone bot can be joined to without inventing a
    // team, and it is where a person arriving later will be offered a place
    // opposite it.
    for (i = 1; i <= num_arenas; i++)
        if (arenas[i].idarena && arenas[i].bots)
            return i;

    // The donor ended at `return 1`, an unconditional fallback that predates
    // any arena being able to refuse.  It is a search now, because arena 1 is
    // exactly as able to say no as any other.
    for (i = 1; i <= num_arenas; i++)
        if (arenas[i].bots)
            return i;

    return 0;
}

static int RA_AutoArena(void)
{
    int     human;

    // Every answer below is qualified by the arena's own `bots` switch,
    // and 0 -- "nowhere the bots should be" -- becomes a possible answer where
    // it never used to be.  Both callers handle it: RA_BotFillArena hands it to
    // CheckMinimumPlayers as "this ruleset declines", and RA_BotJoinArena
    // leaves the bot unseated, which RA_BotsVotedOut then collects.
    human = RA_HumanArena(true);
    if (human)
        return human;

    // Following the people means following the people, and sometimes that is
    // NOWHERE.  If there are people on this map and every arena they are in has
    // voted bots out, the honest answer is 0 -- not "some other arena, then".
    //
    // An earlier draft fell through to the search below in this case,
    // and it was wrong in a way worth recording: the fill would pick an EMPTY
    // arena nobody had asked about and populate it to capacity, so voting the
    // bots out of the arena you are standing in moved eight of them next door
    // to play each other for the rest of the map.  `botfill` under `arena`
    // exists to give the people opponents; an arena with nobody in it
    // has nobody to give them to.
    if (RA_HumanArena(false))
        return 0;

    // Nobody to follow at all.  This is a STAGING answer, not a destination:
    // `minimumplayers` adds its first bot 3.2 seconds into a level, while the
    // person who typed `map` is still loading, and RA_BotFollowPeople moves it
    // to them when they arrive.  RA_StagingArena() is the one answer, and one
    // is what it has to be: the fill asks it "does this arena want bots with
    // nobody on the map" and this asks it "where does this bot go", and two
    // copies could send a bot somewhere the fill was not counting.
    return RA_StagingArena();
}

// A placement is not a resurrection.
//
// Every arena placement ends in SetObserverMode() writing `movetype` straight
// at the edict: a fighter walks (OMODE_NORMAL), and so does a lounge observer
// on a map whose arenas have an observer area of their own -- `active` false,
// which is every real RA2 map.  Neither arm consults `deadflag`, and nothing
// else on the round-start path does either: give_ammo() restores the health
// and the inventory, not the death.
//
// player_die() holds the respawn back a full second -- `respawn_framenum =
// level.framenum + 1.0f * BASE_FRAMERATE`, which is the death animation -- and
// a placement inside that second leaves a client walking and still dead.
// ClientBeginServerFrame's arena arm then respawns it on the frame the timer
// expires and pulls it straight back out of the round it has just been put in,
// leaving its team in `activeteams` with nobody in the arena, which
// fight_done() reads as a wipe and ends the round on.  It used to do worse:
// the respawn queued a WALKING corpse and the next G_RunEntity() ended the
// game library with "SV_Physics: bad movetype 4" (see CopyToBodyQue,
// p_client.c, which is where that stopped being fatal).
//
// So finish the previous life before starting the next one -- the call the
// timer would have made, made now.  The corpse is left where the client died
// rather than where it is being sent, and PutClientInServer clears everything
// player_die() stamped, which is more of it than a hand-written list would
// remember: `svflags & SVF_DEADMONSTER`, the death animation, the blanked
// weapon model, `pm_type`.  It is the sequence an ordinary round already runs
// -- die, respawn into the lounge, be placed by the next fill -- with the
// frames between it removed, and it cannot recurse, because respawn() clears
// `deadflag` before PutClientInServer's own tail re-enters move_to_arena().
//
// Two callers, and only two place a client that may have been alive a moment
// ago: SendTeamToArena is every fill and every team join, and RA_BotJoinArena
// is the bot arm RA_BotFollowPeople reaches with a bot it has taken out of a
// round in another arena -- which is the sequence that found this, live on
// `ra2map27`.  Before the caller's own state writes, because
// PutClientInServer runs ClientUserinfoChanged and reinit_player and would
// otherwise undo the skin and the FIGHT_ALIVE they had just set.
static void RA_ResolvePendingDeath(edict_t *ent)
{
    if (ent->deadflag)
        respawn(ent);
}

void RA_BotJoinArena(edict_t *ent)
{
    int     arenanum, k;
    char    *name;
    team_t  *t;

    if (!(ent->flags & FL_BOT))
        return;
    if (ent->client->resp.teamnum >= 0)     // already on a team: a level change
        return;
    if (num_arenas <= 0)
        return;

    // Ahead of the three refusals below as well as the three placements: a bot
    // that stays in arena 0 has still been taken off a team mid-round by
    // RA_BotFollowPeople, and leaving it dead there is the same unresolved
    // death one room over.
    RA_ResolvePendingDeath(ent);

    arenanum = Q_atoi(Info_ValueForKey(ent->client->pers.userinfo, "arena"));
    if (arenanum < 1 || arenanum > num_arenas)
        arenanum = RA_AutoArena();

    // RA_AutoArena can now answer 0 -- nowhere takes bots -- and an
    // explicit `arena N` can name an arena that has voted them out since the
    // cvar was set.  Both leave the bot an observer in arena 0, which is what
    // a locked arena below already does with one, and the fill stops counting
    // it as a player there.
    if (arenanum < 1 || arenanum > num_arenas)
        return;

    if (!arenas[arenanum].bots) {
        gi.dprintf("%s: arena %d has bots switched off, staying in arena 0\n",
                   ent->client->pers.netname, arenanum);
        return;
    }

    // An idarena is a deathmatch map running as one arena: it has no
    // misc_teleporter_dest, so instead of teams it has the two pickup teams
    // arena_init() made, both already in its waiting queue, and AddtoArena
    // refuses it outright ("You must join a pickup team to enter that arena").
    // The bot joins the smaller of the two, which is also rows 3 and 4 -- no
    // third team, and no over-filling -- for free.
    if (arenas[arenanum].idarena) {
        if (!arenas[arenanum].pickupteam[0] || !arenas[arenanum].pickupteam[1])
            return;
        k = count_queue(arenas[arenanum].pickupteam[0]->arenalink.it) >
            count_queue(arenas[arenanum].pickupteam[1]->arenalink.it);
        if (!add_to_team(ent, arenas[arenanum].pickupteam[k]->name))
            return;
        ent->client->resp.fightstate = FIGHT_SPECTATING;
        ent->takedamage = DAMAGE_NO;
        move_to_arena(ent, arenanum, 1);
        return;
    }

    // A real arena map: the bot gets a team of its own, which is what
    // menuNewTeam gives a lone human, and playersperteam defaults to 1 so one
    // bot is a whole team.  add_to_team leaves it in arena 0's waiting queue;
    // SendTeamToArena is what moves it, and is asked for the observer form so
    // the bot waits its turn instead of being dropped into a running round
    // (bots are noclip observers, never parked in a waiting room).
    if (arenas[arenanum].locked) {
        gi.dprintf("%s: arena %d is locked, staying in arena 0\n",
                   ent->client->pers.netname, arenanum);
        return;
    }

    // Pair up before inventing a team, where the arena asks for more than one
    // player a side.  This is menuAddtoTeam's shape and the pickup arm's three
    // lines above: the team is already in the arena's queue, so only the bot is
    // placed and the team is left where it stands -- calling SendTeamToArena
    // again would re-place and re-announce every member it already has.
    t = RA_BotTeamWithRoom(arenanum);
    if (t) {
        if (!add_to_team(ent, t->name))
            return;
        ent->client->resp.fightstate = FIGHT_SPECTATING;
        ent->takedamage = DAMAGE_NO;
        move_to_arena(ent, arenanum, 1);
        return;
    }

    if (count_queue(&arenas[arenanum].waitingteams) +
        count_queue(&arenas[arenanum].activeteams) >= arenas[arenanum].maxteams) {
        gi.dprintf("%s: arena %d is full, staying in arena 0\n",
                   ent->client->pers.netname, arenanum);
        return;
    }

    name = RA_NewTeamName(ent);
    if (!name)
        return;

    t = add_to_team(ent, name);
    if (!t) {
        gi.TagFree(name);
        return;
    }

    remove_from_queue(&t->arenalink, NULL);
    SendTeamToArena(&teams[t->teamnum], arenanum, true, true);
}

// Following the people is a standing rule, not a one-off.
//
// `arena 0` means "the lowest-numbered arena with a human on a team", and it
// used to be resolved at the bot's join.  That is the right answer asked at the
// wrong moment.  `minimumplayers` adds its first bot at level.framenum 32 --
// 3.2 seconds after the map spawns -- and a person who typed `map` is still
// loading, or reading the motd, or looking at the team list then.  So
// RA_AutoArena has nobody to follow, falls through to "the lowest-numbered
// pickup arena", and that bot is stranded there for the whole level while every
// later bot follows the person into the arena they eventually chose.
//
// Measured on `ra2map9`, whose two pickup arenas are 1 and 2: the person joins
// `#2 Pickup Blue`, the first bot is already on `#1 Pickup Red`, and the two
// bots that follow both land on `#2 Pickup Red` -- one person against two, with
// a third bot alone in an arena nobody is in.  The balance is not the defect
// and needs no fix: three bots joining the smaller of two pickup teams settles
// at 2v2 on its own.  Losing one of the three to the other arena is what made
// it 1v2.
//
// So the question is re-asked while it can still be acted on.  A bot is moved
// only when all four of these hold, which is what keeps it from oscillating:
//
//   * nobody is on a team in the arena the bot is in, so it is never taken away
//     literal request and is never second-guessed, only `arena 0` is ours;
//   * somebody is on a team somewhere, so there is a real answer to follow;
//   * NOBODY is on a team in the arena the bot is in, so it is never taken away
//     from a person it is already playing with;
//   * the target would accept it, checked before the bot is taken off the team
//     it has, so a refused move cannot strand it in arena 0.
//
// The target is RA_HumanArena()'s single lowest-numbered answer, so every bot
// that moves moves to the same place and the condition is false for all of them
// afterwards.  A bot in the middle of a round is moved too, by the same path a
// disconnecting player takes (remove_from_team) -- otherwise two bots stranded
// on opposite sides of an empty arena would hold each other there for a
// nine-round match while a person waited alone.
//
// The four are a predicate of their own rather than four `continue`s in the
// loop, because RA_ArenaPlayers() asks the same question for a different reason
// and the two must not be able to disagree: a bot this returns true for is a
// bot that is about to be in `target`, and the fill has to count it as already
// there or it adds a replacement for a bot that has not moved yet.  The
// target-is-open test stays in the caller -- that is one question about the
// arena, not one per bot.
static bool RA_BotFreeToFollow(edict_t *e, int target)
{
    if (!e->inuse || !e->client || !(e->flags & FL_BOT))
        return false;
    if (e->client->resp.teamnum < 0)
        return false;
    if (!teams[e->client->resp.teamnum].it)
        return false;
    if (TEAM(&teams[e->client->resp.teamnum])->arenanum == target)
        return false;
    if (!RA_BotFollowsPeople(e))
        return false;
    if (RA_ArenaHasHuman(TEAM(&teams[e->client->resp.teamnum])->arenanum))
        return false;

    return true;
}

// Who is standing in this arena, counted where they ARE.
//
// RA_ArenaPlayers() is the same walk with the transit remap applied, and that
// is exactly why this one exists: the remap is defined in terms of
// RA_BotBoundFor, and RA_BotBoundFor now needs a head count to answer with.  A
// count that went through the remap would be asking the question of its own
// answer.
static int RA_ArenaResidents(int arenanum)
{
    edict_t *e;
    int     i, n = 0;

    for (i = 0; i < game.maxclients; i++) {
        e = &g_edicts[i + 1];
        if (!e->inuse || !e->client)
            continue;
        if (e->client->resp.teamnum < 0 || !teams[e->client->resp.teamnum].it)
            continue;
        if (TEAM(&teams[e->client->resp.teamnum])->arenanum == arenanum)
            n++;
    }

    return n;
}

// ...and how many of the free may actually go: AS MANY AS THE ARENA WANTS, and
// no more.
//
// Being free to follow was the whole of it, and every bot that was free went,
// in one tick.  RA_BotFollowPeople asked RA_BotArenaOpen once -- a question
// about the arena, asked before the loop and true for the first bot -- and then
// moved all of them through a door that shut after the first.  Measured on
// ra2map27 with seven bots staged in the pickup arena and one person joining
// arena 1, whose `playersperteam` is 1 and whose `maxteams` is 2: SIX of them
// followed, created six teams of their own in a two-team arena, and the fill
// then disconnected five to get back to a target of two.  The pickup game was
// gone, and what the server had instead was two bots in a duel arena.  That is
// the state the live report describes, reproduced by scenarios/ra2botkeep.
//
// The cap is the fill's own target, so "how many bots does this arena want" has
// one answer and the mover and the census read the same one.  RANK IN CLIENT
// ORDER decides which of them go, because both readers must pick the same bots:
// the census has to count a bot against the arena it is about to be in, and it
// can only do that if "about to" is a fact and not a race.
//
// RA_BotFillTarget() cannot recurse back into here.  Its only path to
// RA_ArenaPlayers() is RA_StagingArena(), which it reaches solely when the
// arena has no human on a team -- and `target` is RA_HumanArena()'s answer, so
// it always has one.
static bool RA_BotBoundFor(edict_t *e, int target)
{
    int i, room, rank = 0;

    if (!RA_BotFreeToFollow(e, target))
        return false;

    room = RA_BotFillTarget(target) - RA_ArenaResidents(target);
    if (room <= 0)
        return false;

    for (i = 0; i < game.maxclients; i++) {
        if (&g_edicts[i + 1] == e)
            break;
        if (RA_BotFreeToFollow(&g_edicts[i + 1], target))
            rank++;
    }

    return rank < room;
}

static void RA_BotFollowPeople(void)
{
    edict_t *e;
    int     i, target;

    // The same cadence as CheckMinimumPlayers, which is what puts the bots
    // here: re-asking every frame would cost a client scan per frame to answer
    // "no" 31 times out of 32.
    if (level.framenum & 31)
        return;

    // An arena that refuses bots is not a place to follow people to.
    // RA_BotArenaOpen below would refuse it anyway, but then no bot moves
    // anywhere -- this way the search passes over it and finds the next arena
    // with people in it that will have them.
    target = RA_HumanArena(true);
    if (!target)
        return;

    if (!RA_BotArenaOpen(target))
        return;

    for (i = 0; i < game.maxclients; i++) {
        e = &g_edicts[i + 1];

        if (!RA_BotBoundFor(e, target))
            continue;

        // Re-asked per bot, not once above: the door shuts as they go through
        // it.  RA_BotBoundFor's cap is the fill's target and this is the
        // arena's own admission rule -- a lock, a team queue at `maxteams`, a
        // pickup side already full -- and the two are different refusals.  The
        // check above stays so that a shut arena costs no client scan at all.
        if (!RA_BotArenaOpen(target))
            return;

        gi.dprintf("%s follows the players to arena %d\n",
                   e->client->pers.netname, target);

        remove_from_team(e);
        RA_BotJoinArena(e);
    }
}

// Which arena would seat this bot if its ClientBegin ran right now, or 0 --
// nowhere would.  RA_BotJoinArena's own first three lines, asked without doing
// anything: the same relationship RA_BotArenaOpen has to the rest of it, and
// for the same reason -- the caller needs the answer about a client it must not
// touch, in this case one whose ClientBegin may not have run yet.
static int RA_BotWouldJoin(edict_t *e)
{
    int n = Q_atoi(Info_ValueForKey(e->client->pers.userinfo, "arena"));

    if (n < 1 || n > num_arenas)
        n = RA_AutoArena();
    if (n < 1 || n > num_arenas)
        return 0;

    return arenas[n].bots ? n : 0;
}

// The bots that were already there when the vote passed.
//
// Every other half of this feature is a refusal -- RA_BotFillArena will not
// select the arena, RA_BotArenaOpen will not admit a bot to it, RA_BotJoinArena
// will not seat one in it -- and refusals only govern bots that have yet to
// arrive.  The vote itself is taken in an arena that has them, by the people
// playing against them, so without this the switch reads "no NEW bots" and the
// four already in the round stay for the rest of the map.
//
// Nothing else can reach them either: the fill's removal arm only works the
// arena RA_BotFillArena selected, and this arena is precisely the one it has
// just stopped selecting.
//
// They are removed, not moved somewhere else, and the scheduler is the reason
// be that simple.  An earlier draft offered each one a seat in another arena
// first and deleted only the ones nobody would take, on the argument that "the
// vote was about our arena, not about this server".  That argument assumes
// there is an arena somewhere that is SHORT of bots -- and under the scheduler
// there is not: every populated arena that allows bots is already being walked
// to its own target, one bot a tick.  Pushing this arena's cast-offs into a
// game that is already the size it asked to be overfills it, and the surplus
// arm then removes the same bots a tick or two later, somewhere the people who
// voted cannot see it happening.  So the fill decides where bots are wanted,
// and this only decides that they are not wanted here.
//
// Which also disposes of the case that made the earlier draft look necessary.
// A bot left standing in arena 0 is a connected client holding a slot a person
// could have used, in no arena, running no round -- and nothing would ever
// collect it, because CheckMinimumPlayers returns before reaching its removal
// arm once RA_BotFillArena answers 0.  On a one-arena map -- every idmap, so
// every deathmatch map running under `arena` -- voting the bots out left
// exactly that many permanent spectators until the map changed.
//
// Declarative rather than edge-triggered on the vote passing, because there are
// two ways the switch moves -- check_voting()'s memcpy and the admin's Apply in
// menuApplyArenaAdmin() -- and a sweep that reads the settled state cannot miss
// one of them or fire twice on the other.
static void RA_BotsVotedOut(void)
{
    edict_t *e;
    char    name[sizeof(((gclient_t *)0)->pers.netname)];
    int     i, n;

    // The same cadence as RA_BotFollowPeople and CheckMinimumPlayers, and for
    // the same reason: this answers "no" for every bot on the server, on every
    // frame that it runs.
    if (level.framenum & 31)
        return;

    for (i = 0; i < game.maxclients; i++) {
        e = &g_edicts[i + 1];

        if (!e->inuse || !e->client || !(e->flags & FL_BOT))
            continue;

        // Copied first, because BotDestroy() memsets the edict and the name is
        // wanted for the line that says what became of it.
        Q_strlcpy(name, e->client->pers.netname, sizeof(name));

        // The bot that was still being built when the vote passed, which the
        // team-based test below cannot see: it has no team yet, and no arena.
        //
        // Its ClientBegin is deferred until the library reports the bot
        // initialised -- twenty seconds of frames on the first visit to a map
        // -- and when it finally runs, RA_BotJoinArena refuses it, because the
        // arena it was added for has voted bots out in the meantime.  It then
        // stands in arena 0 forever, holding a client slot, exactly like the
        // bots this sweep was written to collect.  Measured: with the fill
        // running, one bot in flight at the moment of the vote survived it.
        //
        // BotStarted() is the guard that makes this safe.  A bot the library
        // has not finished is one whose ClientBegin has not been ALLOWED to
        // run, and it may yet be seated; a started one has either run it or is
        // about to, and RA_BotWouldJoin() answers the same question ClientBegin
        // is going to ask.  When that answer is "nowhere", it is nowhere now
        // and nowhere a frame later, so the race between the two does not
        // matter -- which is why this reads the destination rather than calling
        // RA_BotJoinArena on a client that may not have spawned yet.
        if (e->client->resp.teamnum < 0) {
            if (!BotStarted(e))
                continue;
            if (RA_BotWouldJoin(e))
                continue;

            gi.dprintf("%s leaves the server: no arena will have bots\n", name);
            BotDestroy(e);
            continue;
        }

        if (!teams[e->client->resp.teamnum].it)
            continue;

        n = TEAM(&teams[e->client->resp.teamnum])->arenanum;
        if (n < 1 || n > num_arenas || arenas[n].bots)
            continue;

        // Off the team before the disconnect, so check_teams() settles the
        // round this bot was in while there is still a team to settle -- the
        // same order RA_BotFollowPeople uses for a move.
        remove_from_team(e);

        // BotDestroy rather than the fill's `sv removebot <name>`: that scans
        // the client list for a name this already holds the edict for, and two
        // bots from one roster can carry the same one.
        gi.dprintf("%s leaves arena %d: bots are switched off there\n",
                   name, n);
        BotDestroy(e);
    }
}

void remove_from_team(edict_t *ent)
{
    qmenu_t *node;

    if (ent->client->resp.teamnum < 0)
        return;

    node = &ent->client->resp.teammember;

    if (!TEAM(&teams[ent->client->resp.teamnum])->name) {
        gi.dprintf("ERROR in remove_from_team -- please e-mail crt\n");
        return;
    }

    gi.bprintf(PRINT_MEDIUM, "%s has been removed from team %d (%s)\n",
               ent->client->pers.netname, ent->client->resp.teamnum,
               TEAM(&teams[ent->client->resp.teamnum])->name);

    if (TEAM(&teams[ent->client->resp.teamnum])->fighting)
        RA2_Stats_RemovePlayer(arenas[ent->client->resp.context].stats,
                               ent - g_edicts);

    remove_from_queue(node, NULL);

    check_teams(ent->client->resp.context);

    ent->client->resp.teamnum = -1;
}

// "Farthest from any player" is a minimum, and which players it is taken over
// is three separate decisions.
//
// One of them: an arena observer is alive, noclipping and parked on a spawn
// point, so counting it chooses spawns by where the audience stood.  That is
// answered inside `PlayersRangeFromSpot`, which the shared deathmatch
// selectors also use.  RA2 upstream reached the same finding later
// (`rocketarena2@6b8d058`) and answered all three at once.  The other two are
// still open here, and both are cheap:
//
//   * the scan covers every client on the server, so a fight in another arena
//     skews the pick.  Distance makes that rare rather than impossible -- two
//     arenas can be neighbours in one BSP -- and "rare" is not a property worth
//     relying on;
//   * `SendTeamToArena` sets FIGHT_ALIVE before calling move_to_arena, so the
//     player being placed is ranged against the position it is standing in
//     right now, which is the observer spot it is about to leave.
//
// So the arena selectors ask their own question instead of the shared one.
// `PlayersRangeFromSpot` keeps the observer arm because `PutClientInServer`
// still reaches the shared deathmatch selectors under `arena`, before this
// file's placement runs.
// reaches the shared deathmatch selectors under `arena`, before this file's
// placement runs.
static bool ArenaLiveBody(edict_t *e, edict_t *ignore)
{
    if (e == ignore)
        return false;
    if (!e->inuse || !e->client)
        return false;
    if (e->health <= 0)
        return false;

    return true;
}

static bool ArenaFighter(edict_t *e, int arenanum, edict_t *ignore)
{
    if (!ArenaLiveBody(e, ignore))
        return false;
    if (e->client->resp.fightstate != FIGHT_ALIVE)
        return false;
    if (e->client->resp.context != arenanum && idmap == false)
        return false;

    return true;
}

// Distance from `place` -- where a body put on this candidate actually STANDS,
// see ArenaSpawnLanding -- to the nearest client that counts: with
// `fighters_only`, the ones fighting in this arena; otherwise every live body on
// the server.  False means it counted nobody, which is the caller's cue that
// this measure has nothing to say about this spot.
static bool ArenaRangeFromSpot(const vec3_t place, int arenanum, edict_t *ignore,
                               bool fighters_only, float *range)
{
    edict_t *player;
    float   dist;
    vec3_t  v;
    bool    found = false;
    int     n;

    for (n = 0; n < game.maxclients; n++) {
        player = &g_edicts[n + 1];

        if (fighters_only) {
            if (!ArenaFighter(player, arenanum, ignore))
                continue;
        } else if (!ArenaLiveBody(player, ignore))
            continue;

        VectorSubtract(place, player->s.origin, v);
        dist = VectorLength(v);

        if (!found || dist < *range) {
            *range = dist;
            found = true;
        }
    }

    return found;
}

// The distance to the nearest player fighting in this arena -- and where nobody
// is fighting here, to the nearest live body anywhere.
//
// That fallback is not optional.  Zero scores every spot below
// SelectFarthestArenaSpawnPoint's floor of 50, so the caller falls through to
// SelectRandomArenaSpawnPoint, and random spots collide.  In the staging area
// that is not an edge case but the norm: everybody there is FIGHT_SPECTATING,
// so the fighter pass counts nobody every single time and every arrival is
// placed at random.
//
// Two arrivals on one spot is not cosmetic.  An observer in OMODE_NORMAL is
// SOLID_BBOX on MOVETYPE_WALK, and RA2's own separators both decline: KillBox
// returns early for a FIGHT_SPECTATING client and check_telefrag skips one, so
// once two of them are inside each other nothing in the mod ever pulls them
// apart.  The spawn picker was the only thing keeping them out of each other,
// which is what RA2's PlayersRangeFromSpot did by measuring against every live
// client.
//
// The score remains fighter-first: an observer must not make a candidate rank
// farther from the fight.  Candidate safety is a separate question, answered
// below without changing this measure.  Taken from `rocketarena2@811af42`.
static float ArenaFightersRangeFromSpot(const vec3_t place, int arenanum,
                                       edict_t *ignore)
{
    float   range = 0;

    if (ArenaRangeFromSpot(place, arenanum, ignore, true, &range))
        return range;

    ArenaRangeFromSpot(place, arenanum, ignore, false, &range);

    return range;
}

#define ARENA_SPAWN_CLEARANCE 50.0f

// A spawn entity is not where a body put on it stands, and every question this
// file asks about a candidate is a question about where it stands.
//
// move_to_arena clips a placement to the floor -- it traces the player box from
// 16 above the entity down to 64 below it -- so the drop from the entity to the
// body can be most of that 64, and a mapper has no reason to keep it small.
// Measured on `ra2map26` arena 1: five of its six pads put a body 24 units
// below their own origin and the sixth, at (1024 544 240), puts one at z=184 --
// 56 units, past ARENA_SPAWN_CLEARANCE.  A pad measured from its entity
// therefore reported itself clear while a fighter stood on it, and the next
// arrival on that side was sent to the same pad; on that one pad of that one
// arena, every round.  So the landing point is computed once per candidate and
// everything downstream is measured from it.
//
// The trace is the placement's own, `ent` and mask included, which is what
// makes the answer the same answer -- and its `ent` is the second half of the
// fix, because a body already in the column is what it reports.  Callers keep
// the trace rather than only the point (see move_to_arena).
//
// Provenance, because it explains how a correct measure became a wrong one.
// 1999 (`v_ra2`) places a body at `dest->s.origin` with `[2] += 10` and no
// trace at all -- its own `mins`/`maxs` are declared and never used -- so the
// entity was where the body stood, and `SelectFarthestArenaSpawnPoint`'s
// `bestdistance = 50` over `PlayersRangeFromSpot(spot)` meant what it said.
// The floor clip below is `port_ra2`'s addition, comment and all, and it left
// that selector measuring from the entity; the occupancy predicate was then
// built on the same measure, which carried the mismatch onto the pickup path.
// Neither step was wrong on its own.
static trace_t ArenaSpawnLanding(edict_t *spot, edict_t *ent, vec3_t place)
{
    vec3_t      mins = {-16, -16, -24};
    vec3_t      maxs = {16, 16, 32};
    vec3_t      from, to;
    trace_t     tr;

    VectorCopy(spot->s.origin, from);
    from[2] += 16;
    VectorCopy(spot->s.origin, to);
    to[2] -= 64;

    tr = gi.trace(from, mins, maxs, to, ent, MASK_PLAYERSOLID);

    if (!tr.allsolid && !tr.startsolid) {
        VectorCopy(tr.endpos, place);
    } else {
        VectorCopy(spot->s.origin, place);
        place[2] += 10;
    }

    return tr;
}

// A live, collidable body already standing on this arena's pad is not a safe
// placement target.  This is deliberately separate from the fighter-only
// score: observers do not affect which clear pad ranks farthest from a fight,
// but they do reserve the pad they physically occupy.  An id map has one
// arena, so its untagged bodies all belong to the requested arena.
//
// `place` is written for the caller: the landing point costs a trace and both
// the clearance below and the caller's fighter distance are measured from it.
//
// Two tests, and the column one is the load-bearing half.  A radius around the
// landing point cannot see a body directly underneath it -- the landing trace
// stops on that body, so the point it reports is one box-height above it and
// reads as 56 clear units.  What the trace hit is the exact answer to "would
// this put me in somebody", so it is asked first, and the radius stays for the
// body standing beside the pad, which no trace down the column can see.
//
// One case the pair answers by assumption rather than by measurement: where the
// trace starts solid it can report nothing about the column, and the landing
// point is then the placement's own `entity + 10` guess -- a point in the air.
// `ra2map26` arena 1 has such a pad at (-1024 480 240): a body placed there is
// at z=250 for as long as it takes to fall to 184, so a radius around 250 would
// call the pad clear once its occupant had dropped.  It is sound because every
// fighter placement in a round happens in one frame -- SendTeamToArena is
// called from fill_arena and from the next-round loop and nothing else places a
// fighter -- so the occupant of such a pad has not fallen yet when the next
// candidate is measured.  A placement path that ran a frame later would need
// the trace's whole extent as an envelope.
static bool ArenaSpawnSpotClear(edict_t *spot, int arenanum, edict_t *ignore,
                                vec3_t place)
{
    edict_t *player;
    vec3_t  v;
    int     n;
    trace_t tr;

    tr = ArenaSpawnLanding(spot, ignore, place);

    if (tr.ent && tr.ent->client && ArenaLiveBody(tr.ent, ignore) &&
        tr.ent->solid != SOLID_NOT &&
        (tr.ent->client->resp.context == arenanum || idmap != false))
        return false;

    for (n = 0; n < game.maxclients; n++) {
        player = &g_edicts[n + 1];
        if (!ArenaLiveBody(player, ignore))
            continue;
        if (player->solid == SOLID_NOT)
            continue;
        if (player->client->resp.context != arenanum && idmap == false)
            continue;

        VectorSubtract(place, player->s.origin, v);
        if (VectorLength(v) <= ARENA_SPAWN_CLEARANCE)
            return false;
    }

    return true;
}

// The n-th (0-based) spawn point of this arena, or NULL if there is no n-th.
//
// It replaces a do/while that walked the GLOBAL entity list and incremented its
// own index every time it stepped over a spot belonging to another arena.  That
// loop dereferences whatever G_Find returns without testing it, so an index it
// cannot satisfy walks off the end of the list and reads `spot->arena` through
// NULL.  `side == 1` on an arena with one spawn point asks for index 1 of one
// (see the clamp below), which is exactly that -- a server-killing fault, from
// arena.cfg alone, on any arena with `pickup: 1` and a single
// info_player_deathmatch.
static edict_t *ArenaSpawnSpot(char *classn, int arenanum, int n)
{
    edict_t *spot = NULL;

    while ((spot = G_Find(spot, FOFS(classname), classn)) != NULL) {
        if (spot->arena != arenanum && idmap == false)
            continue;
        if (!n--)
            return spot;
    }

    return NULL;
}

// How many of `classn` belong to this arena.  Lifted out of
// SelectRandomArenaSpawnPoint, which opened with exactly this walk, so that
// RA_BotFillTarget() can ask the same question of the same entities: the
// `idmap == false` clause is what makes a stock deathmatch map -- one arena,
// no `arena` key on anything -- count every spawn point it has for that arena.
static int ArenaSpawnCount(char *classn, int arenanum)
{
    edict_t *spot = NULL;
    int     count = 0;

    while ((spot = G_Find(spot, FOFS(classname), classn)) != NULL) {
        if (spot->arena != arenanum && idmap == false)
            continue;
        count++;
    }

    return count;
}
// A pickup arena puts its two sides on alternate spawn points and picks among
// them at random, which is a birthday problem.
//
// `ra2map9` arena 2 has twelve spawn points, so a side draws from six; three
// bots on that side collide 44% of the time, and RA2's whole answer to a
// collision is KillBox -- which during a countdown cannot telefrag, because an
// arena fighter is `takedamage DAMAGE_NO` until ASTATE_FIGHTING, and whose
// push-apart pushes both bodies along the same vector (see g_utils.c).  So two
// players who draw the same point stay standing inside each other, which is
// what a play test reported: "both enemy bots spawned at exactly the same spawn
// point, they did not telefrag or push away each other, their bodies overlapped
// like siamese twins".  Measured here at three deep -- three members of `#2
// Pickup Red` placed within 60 units of each other, two of them left SOLID_NOT
// with a spawn_recheck, which only KillBox's push branch sets.
//
// The fix is not to collide.  Fighter distance chooses the preferred clear
// point, and ArenaSpawnSpotClear reserves a same-arena solid body's actual
// pad without letting that body influence the ranking.  If no clear point has
// fighter clearance, the first clear point is still preferable to a collision.
// Only when every point on the side IS taken is the first candidate returned,
// because that is RA2's stated fallback -- "if there is a player just spawned
// on each and every start spot we have no choice to turn one into a telefrag
// meltdown".
edict_t *SelectRandomArenaSpawnPoint(char *classn, int arenanum, int side, edict_t *ignore)
{
    edict_t     *spot, *first = NULL, *clear = NULL;
    vec3_t      place;
    int         count;
    int         selection, step, i;

    count = ArenaSpawnCount(classn, arenanum);

    if (!count)
        return NULL;

    selection = rand() % count;

    //gi.dprintf("%d spots, %d selected\n",count,selection);

    step = 1;
    if (side) {
        selection &= ~1;
        if (side == 1) {
            selection++;
            if (selection >= count)
                selection = 1;
        }
        step = 2;
    }

    // One point is one lap for a side that has the arena to itself; two laps of
    // `step` cover every candidate the parity allows either way.
    for (i = 0; i < count; i++) {
        spot = ArenaSpawnSpot(classn, arenanum, (selection + i * step) % count);
        if (!spot)
            continue;
        if (!first)
            first = spot;
        if (!ArenaSpawnSpotClear(spot, arenanum, ignore, place))
            continue;
        if (!clear)
            clear = spot;
        if (ArenaFightersRangeFromSpot(place, arenanum, ignore) > ARENA_SPAWN_CLEARANCE)
            return spot;
    }

    return clear ? clear : first;
}

edict_t *SelectFarthestArenaSpawnPoint(char *classn, int arenanum, edict_t *ignore)
{
    edict_t     *bestspot;
    float       bestdistance, bestplayerdistance;
    edict_t     *spot;
    vec3_t      place;

    spot = NULL;
    bestspot = NULL;
    bestdistance = ARENA_SPAWN_CLEARANCE;
    while ((spot = G_Find(spot, FOFS(classname), classn)) != NULL) {
        //gi.bprintf (PRINT_HIGH,"arena %d spot %d\n", arenanum, spot->arena);
        if (spot->arena != arenanum && idmap == false) continue;
        if (!ArenaSpawnSpotClear(spot, arenanum, ignore, place)) continue;
        bestplayerdistance = ArenaFightersRangeFromSpot(place, arenanum, ignore);

        if (bestplayerdistance > bestdistance) {
            bestspot = spot;
            bestdistance = bestplayerdistance;
        }
    }

    if (bestspot) {
        return bestspot;
    }

    // if there is a player just spawned on each and every start spot
    // we have no choice to turn one into a telefrag meltdown
    return SelectRandomArenaSpawnPoint(classn, arenanum, 0, ignore);
}

/*
=================
`botfill` -- the bot count follows the arena, not the server

`minimumplayers` is one number for the whole server, and under every other
ruleset that is the right shape: one map, one game, one roster.  Rocket Arena
runs up to 32 games at once and they are not the same size.  `arena.cfg`
declares `ra2map8` arena 3 a 1v1 and `ra2map9` arena 7 a 2v2, and `ra2map9`
arena 2 -- a pickup arena -- seats twelve.  A flat four is a crowd in the first
and an empty room in the third.

So `botfill 1` asks the arena instead, and the answer comes from two places
because the mod keeps it in two places:

  * A NON-PICKUP arena is `arena.cfg`'s own `playersperteam`, times the two
    teams a round is fought between.  That number is not advisory: AddtoArena
    admits a team of exactly that size and check_teams ejects one that grows
    past it, so it IS the arena's capacity.
  * A PICKUP arena has no such number.  arena_init() overwrites
    `playersperteam` with 128 for every one of them -- RA2 wants a pickup team
    unbounded -- and all thirty of RA2's own pickup arenas leave the key unset
    anyway.  What is left is the map: the `info_player_deathmatch` entities
    carrying this arena's number, which the two sides take ALTERNATELY
    (SelectRandomArenaSpawnPoint), so the arena seats two out of every pair.

Counting spawn points would be wrong for the first kind, and that is worth
writing down because it is the obvious thing to try.  RA2's mappers used spawn
points for variety, not capacity: `ra2map8` arena 3 has thirteen of them and is
declared 1v1, and only 55 of 141 non-pickup arenas have as many spawn points as
`2 * playersperteam`.  Filling one to its spawn count would build teams that
check_teams then deletes.

`0` is off and nothing here runs.  The ceilings when it is on are
`game.maxclients` -- which is LATCHED and defaults to 4 -- and the roster in
`botcfg/bots.cfg`.
=================
*/
static int RA_NeediestArena(int *gapout);    // defined just below

int RA_BotFillArena(void)
{
    int n;

    if (!BotFillEnabled())
        return 0;
    if (num_arenas <= 0)
        return 0;

    // The arena this counts must be the arena the bots are sent to, and there
    // are two answers to that, not one.  `bl_spawn.c` writes the `arena` cvar
    // into a new bot's userinfo and RA_BotJoinArena obeys it, so 1..N is where
    // the bots actually go -- and a fill that counted somewhere else would find
    // that arena empty at every tick and add until the roster or `maxclients`
    // ran out.  Only 0, the absence of a request, is resolved by following the
    // people, and through RA_AutoArena rather than a second copy of it.
    //
    // And it must be an arena that still takes them.  `bots` is the arena's own
    // switch -- `arena.cfg`'s `bots` key, and the "Allow Bots" row the people
    // in it can vote on -- so an explicit `arena N` pointed at an arena that
    // has turned bots off resolves to "nowhere" rather than being obeyed over
    // the top of it.  0 is the answer CheckMinimumPlayers reads as "this
    // ruleset declines", which is why it may be returned here at all.
    //
    // The default is 0, "no arena named".  It was "1", which this test cannot
    // tell apart from an operator who meant arena 1 -- so a server that did not
    // exec configs/arena.cfg pinned the whole fill to arena 1 and got neither
    // the scheduler below nor follow-the-people, while a server that did exec
    // it got both.  All four readers of this cvar default to "0" together:
    // whichever runs first is the one that creates it, so a single "1" left
    // anywhere would decide for all of them.
    n = (int)gi.cvar("arena", "0", 0)->value;
    if (n >= 1 && n <= num_arenas)
        return arenas[n].bots ? n : 0;

    // Otherwise, the arena furthest from its own target.  See
    // RA_NeediestArena() -- one arena a tick, but not the same arena every
    // tick, which is what "botfill fills the arenas" has to mean on a map that
    // has more than one of them.
    n = RA_NeediestArena(NULL);
    if (n)
        return n;

    // Nobody is playing anywhere: staging, exactly as before.
    return RA_AutoArena();
}

// Which arena the fill should work this tick.
//
// The fill used to have one arena and one census, and asked RA_AutoArena for
// the number: "the arena the people are in".  On a one-arena map that is the
// whole truth, and RA2's own maps have up to 32.  With two crowded arenas the
// fill sized itself to the lowest-numbered one, filled it, and then sat at its
// target with nothing left to say -- the second arena never saw a bot, however
// short-handed it was, because the question "which arena" had an answer that
// never changed.
//
// So the answer moves.  Each tick this picks the arena furthest from its own
// `RA_BotFillTarget()`, the add/remove arms in CheckMinimumPlayers act on that
// one, and 32 frames later the question is asked again -- so every arena walks
// to its own target, one bot at a time, off the machinery that already existed
// for one.
//
// An arena with nobody in it wants no bots, and that is the whole of "the bots
// follow the people" expressed as a number rather than as a migration.  An
// empty arena's target is 0, so the bots left standing in one a person has just
// walked out of are a surplus and the removal arm takes them, one a tick, while
// the arena that person walked into shows a deficit and the add arm fills it.
// Nothing has to move a bot from one arena to another: both halves fall out of
// asking every arena the same question.  It is also why filling empty arenas is
// not a policy that has to be forbidden -- an empty arena simply never wants
// anybody, so `maxclients` and the roster are spent only where people are.
//
// Surpluses are taken before deficits are filled, which is the difference
// between converging and deadlocking.  The bots standing in the arena everyone
// just left are holding the very client slots the arena they left for needs;
// preferring the deficit asks for a slot that only the surplus can release, and
// on a full server neither side ever moves.
//
// ...but only a surplus something can be done about.  Four people in a 1v1
// arena are permanently over its target with no bot to remove, and a candidate
// that cannot be acted on would be re-selected every tick and starve every
// other arena of the fill.  So the surplus arm asks for a bot to take.
//
// Parameter:               gapout: if non-NULL, the selected arena's gap --
//                          positive means it is short by that many, negative
//                          means it is carrying that many too many
// Returns:                 the arena to work, or 0 -- nothing to do anywhere
// Changes Globals:     -
// Is there a client slot the fill could put a NEW bot in?
//
// The question a drain has to answer before it takes a bot out of a game: with
// a seat free the roster serves the shortage and the game is left alone, and
// without one those bots are the only ones there are.  `inuse` is
// G_SpawnClient()'s own predicate, so this cannot say there is room where the
// allocator finds none -- the same reckoning bl_spawn.c's `seated` makes.
//
// A bot still in the creation queue holds no edict yet and is not counted, so
// this can say `free` about a seat that is already spoken for.  That errs
// toward NOT draining, which is the direction a wrong answer should fall in:
// the cost is one tick of a shortage going unserved, against taking a bot out
// of a running game for nothing.
static bool RA_SeatsFree(void)
{
    int i, seated = 0;

    for (i = 0; i < game.maxclients; i++)
        if (g_edicts[i + 1].inuse)
            seated++;

    return seated < game.maxclients;
}

static int RA_NeediestArena(int *gapout)
{
    int i, here, want, gap, nbots;
    int best = 0, bestgap = 0;
    int spare = 0, sparegap = 0;

    for (i = 1; i <= num_arenas; i++) {
        // An arena that has voted bots out is RA_BotsVotedOut's to empty, not
        // this one's: it wants no bots and will not have the fill add any, so
        // selecting it here would only spend the tick.
        if (!arenas[i].bots)
            continue;

        here = RA_ArenaPlayers(i, &nbots);

        // RA_BotFillTarget() is 0 for an arena with nobody in it, which is what
        // empties the one the last person has just left: `here` is then the
        // bots standing in it and the gap is the whole of that.  The rule lives
        // there rather than here because CheckMinimumPlayers reads the same
        // function, and the two must not be able to disagree about it.
        want = RA_BotFillTarget(i);
        gap = want - here;

        if (gap > bestgap) {
            bestgap = gap;
            best = i;
        } else if (gap < sparegap && nbots > 0) {
            sparegap = gap;
            spare = i;
        }
    }

    // The surplus first; see the block above for why that way round -- but only
    // when draining it is the only way to serve the shortage.
    //
    // A drain is not a MOVE.  The removal arm disconnects the bot and the add
    // arm creates another from the roster, so "the bots go where the people
    // are" is an eviction followed by an unrelated arrival -- and the two do
    // not take turns, because a surplus stays a surplus until the arena is
    // EMPTY.  One person joining an arena that is short by one therefore
    // emptied an arena of ten: every tick found the same surplus, took another
    // bot out of a game that was running, and never once added to the arena
    // that was short.
    //
    // Measured on the live arena server, ra2map27: ten bots playing the pickup
    // arena, one person joined a ppt=1 arena elsewhere, and 32 frames apart
    // the removal arm took all nine bots that had not followed them.  The map
    // spent the rest of its life with that arena looping `It was a tie!` and
    // nobody in it to fight.  Reproduced by scenarios/ra2botkeep.
    //
    // So the shortage is served from the ROSTER while the server has a seat
    // for one, and a running game is taken apart only when it has not -- which
    // is the one case where those bots really are the only ones available.
    if (RA_SeatsFree())
        spare = 0;

    if (gapout)
        *gapout = (spare && best) ? sparegap : bestgap;

    return (spare && best) ? spare : best;
}

// ...and which arena the bot it is about to add must be told to join.
//
// `bl_spawn.c` writes an `arena` userinfo key at bot creation and
// RA_BotJoinArena obeys it.  While the fill only ever worked one arena, 0 --
// "follow the people" -- was a good enough instruction, because following the
// people and being sent to the fill's arena were the same journey.  Under a
// scheduler they are not: RA_AutoArena resolves 0 to the lowest-numbered
// populated arena, so every bot the fill added for arena 5 would walk into
// arena 1 instead and the scheduler would ask for another one next tick,
// forever.
//
// 0 is still returned where it is still right, and that is not a detail: a
// staging bot -- added before anybody had joined -- must keep it, because
// RA_BotFollowPeople only moves bots that have it (RA_BotFollowsPeople).
// Pinning a staging bot to the arena it was parked in would strand it there for
// the level.
//
// Parameter:               -
// Returns:                 the arena to pin the next filled bot to, or 0
// Changes Globals:     -
int RA_BotFillDestination(void)
{
    int n, gap = 0;

    if (!BotFillEnabled())
        return 0;
    if (num_arenas <= 0)
        return 0;

    // Nobody to follow: staging, and staging bots stay followers.
    if (!RA_HumanArena(false))
        return 0;

    // A new bot only ever belongs in an arena that is short.  RA_NeediestArena
    // answers for both arms of the fill and takes surpluses first, so it can
    // name an arena that has too many -- which is the right answer for the arm
    // that removes one and the wrong one for this.  CheckMinimumPlayers does
    // not in fact add on a tick it is draining, so this cannot currently be
    // reached with a surplus; asking anyway is what stops that from being a
    // fact this depends on without saying so.
    n = RA_NeediestArena(&gap);
    if (n < 1 || gap <= 0)
        return 0;

    return n;
}

// The arena's own number, unclamped.  The ceilings -- two sides make a round,
// the roster, `game.maxclients` -- are BotFillTarget()'s, in bl_spawn.c, and
// they used to be here as well: the same three lines over a second
// `botfill_ceiling` private to this file.
int RA_BotFillTarget(int arenanum)
{
    if (arenanum < 1 || arenanum > num_arenas)
        return 0;

    // An arena with nobody in it wants none, and that belongs here and not in
    // the scheduler that first needed it.  This is the number
    // CheckMinimumPlayers compares its census against, so a rule the scheduler
    // applied privately was a rule the two of them disagreed about: the
    // scheduler scored the arena a person had just left as a surplus and
    // selected it, CheckMinimumPlayers then asked this function, got the
    // arena's capacity back, and concluded it was short.  Measured: an arena
    // everybody had left sat at two bots for as long as the map ran, selected
    // every tick and neither filled nor drained.
    //
    // `botfill` exists to give people opponents.  An arena's capacity is what
    // it can seat; what it wants is that number only while somebody is in it.
    //
    // ...or while there is nobody on the MAP, which is not a hole in that rule
    // but the same rule asked where it has no people to point at.  With the
    // server empty the staging arena is where a bot goes, where the bots
    // already here are, and where a person arriving will find them, so it is
    // the arena that wants them -- and RA_StagingArena() is 0 the moment
    // anybody is on a team anywhere, which hands every arena straight back to
    // the line above.  This is what `botfill 1` was missing against the flat
    // `minimumplayers` beside it: see RA_StagingArena() for the whole of it.
    if (!RA_ArenaHasHuman(arenanum) && arenanum != RA_StagingArena())
        return 0;

    if (arenas[arenanum].idarena)
        return 2 * (ArenaSpawnCount("info_player_deathmatch", arenanum) / 2);

    return arenas[arenanum].numteams * arenas[arenanum].playersperteam;
}

// Is this the arena the fill is holding for people who have not arrived yet?
// A diagnostic, for the same reason RA_ArenaIsPickup() below is one: `sv
// ruleset`'s botfill row reads "arena 1 want=4" whether four people are in that
// arena or nobody is on the map at all, and an operator wondering why a server
// nobody has joined has bots in it deserves to be told which of the two it is.
bool RA_ArenaIsStaging(int arenanum)
{
    return arenanum >= 1 && arenanum == RA_StagingArena();
}

// Is this a pickup arena -- one whose size comes from the map rather than from
// `arena.cfg`?  `sv ruleset` says which of the two answered, and reaching into
// `arenas[]` from the bot layer to find out would put an arena struct in a
// shared file for one boolean.
bool RA_ArenaIsPickup(int arenanum)
{
    if (arenanum < 1 || arenanum > num_arenas)
        return false;

    return arenas[arenanum].idarena;
}

// Who is playing in this arena, by the same definition BotCountsAsPlayer uses
// under `arena`: being on a team, not being alive in the round.
//
// Plus the bots RA_BotFollowPeople is about to bring here, which is not a
// refinement but the reason this is a function.  CheckMinimumPlayers runs from
// G_RunFrame before G_CheckRules, and both gate on `level.framenum & 31`, so on
// a fill tick the bots in transit have not moved yet.  Counted where they
// stand, the arena looks emptier than it is about to be, the fill adds
// replacements for bots that are already on their way, and the removal arm
// throws those out 32 frames later -- an add/remove oscillation at every level
// start.
int RA_ArenaPlayers(int arenanum, int *bots)
{
    edict_t *e;
    int     i, n, follow, players = 0, nbots = 0;

    // Where a bot in transit is going, asked once for the whole scan rather
    // than once per bot: RA_BotFollowPeople moves bots to the arena the people
    // are in and nowhere else, and not at all when there is nobody to follow.
    // That is a question about the arena, which is the half RA_BotBoundFor
    // leaves to its caller.
    follow = RA_HumanArena(true);

    for (i = 0; i < game.maxclients; i++) {
        e = &g_edicts[i + 1];

        if (!e->inuse || !e->client)
            continue;
        if (e->client->resp.teamnum < 0 || !teams[e->client->resp.teamnum].it)
            continue;

        n = TEAM(&teams[e->client->resp.teamnum])->arenanum;

        // A bot in transit is counted ONCE, against the arena it is going to
        // rather than the one it is leaving.  Counting it in both -- which is
        // what asking only "is it bound for HERE" did -- costs a bot the moment
        // staging ends: the first person to arrive joins an arena, the staging
        // bots are still standing where they were on this tick because
        // CheckMinimumPlayers runs before G_CheckRules, and the arena they are
        // walking out of reads as a surplus while the one they are walking into
        // reads as filled.  So the removal arm throws one of them out and the
        // fill adds a replacement a tick later, in the arena they were already
        // on their way to.
        if (follow && RA_BotBoundFor(e, follow))
            n = follow;

        if (n != arenanum)
            continue;

        players++;
        if (e->flags & FL_BOT)
            nbots++;
    }

    if (bots)
        *bots = nbots;

    return players;
}

// A bot to take out of this arena, named rather than left to `removebot`'s own
// scan -- which takes the lowest client slot, and that bot may be playing in an
// arena nobody asked to shrink.
//
// ...and off the team that is carrying ONE TOO MANY, which is the other half of
// the same sentence and was missing.  Scoping the name to the arena keeps the
// fill from shrinking somebody else's game; it says nothing about which of this
// arena's sides gives the bot up, and the first bot in client-slot order is not
// a neutral answer to that.  RA_BotJoinArena seats every bot on the SMALLER
// pickup team, so slot order and side order are one alternation -- an odd
// number of removals therefore takes one more off whichever side holds the
// lowest slot, and a person in the arena is never counted against his own side
// at all.
//
// Measured, `ra2map18` -> `ra2map2` with `botfill 1`: arena 6 of the outgoing
// map seats 14 (15 info_player_deathmatch, R-RA-7's `2 * (spawns / 2)`) and
// arena 5 of the incoming one seats 8 (9), so the level change drains seven
// bots.  They came off Red, Blue, Red, Blue, Red, Blue, Red -- four and three
// -- and the person who picked Blue in the middle of it left the arena 3v5 at
// its correct total of 8.  Nothing corrects it afterwards, which is what makes
// this worth a scan of its own rather than a tick's inaccuracy: the arena is AT
// its target, so RA_NeediestArena() scores it 0 and neither arm of the fill
// selects it again for the rest of the map.
//
// So the team with the most members gives the bot up, which is
// CTFBotFillName()'s rule one ruleset over -- R-CTF-8's consequence block names
// this function as its twin, and until now only the arena half of it was here.
// It is asked per TEAM rather than per side because that is one question in
// both kinds of arena: a pickup arena's two sides ARE its two teams, and a real
// arena map's teams have no `side` until sidepick at the whistle.  A team with
// no bot on it is passed over -- the fill cannot remove a person -- which is
// the same fall-through CTF's NULL gets when the only bots left are on the
// smaller side.
//
// A TIE is broken by team slot, and NOT by returning NULL.  CTF returns NULL
// and lets the caller fall through to a bare `sv removebot`, which is the whole
// server under `ctf` and is the one thing that must not happen here:
// `bl_spawn.c` passes this straight into BotServerCommand, where a NULL
// terminates the vararg list (bl_redirgi.c) and the engine's own scan then
// takes the lowest client slot ON THE SERVER -- a bot out of some other arena's
// running round.
//
// The same reckoning of where a bot in transit belongs as RA_ArenaPlayers
// above, and by the same two lines, because this names the bot that census
// counted: an arena over its target that was handed the name of a bot counted
// somewhere else would still be over it on the next tick, and ask again.  It
// governs both halves here.  A bot on its way OUT is on this arena's team and
// in the other arena's census, so it is left out of the tally and out of the
// pick.  A bot on its way IN is in this arena's census and on no team of it, so
// no tally can hold it -- which is what the second loop is for, unchanged: when
// the arena's own teams have no bot to give, the bot walking towards it is the
// one it can.
char *RA_ArenaBotName(int arenanum)
{
    qmenu_t *mnode;
    edict_t *e, *cand, *pick = NULL;
    int     i, n, follow, members, most = 0;

    follow = RA_HumanArena(true);

    for (i = 0; i < MAX_TEAMS; i++) {
        if (!teams[i].it)
            continue;
        if (TEAM(&teams[i])->arenanum != arenanum)
            continue;

        // Counted here rather than by count_queue(), because the members this
        // arena HAS and the members standing in its queue are not the same set
        // while a bot is walking out of it -- and one walk answers both halves.
        members = 0;
        cand = NULL;
        mnode = &teams[i];
        while (mnode->next) {
            mnode = mnode->next;
            e = (edict_t *)mnode->it;
            if (!e)
                continue;
            if (follow && RA_BotBoundFor(e, follow))
                continue;

            members++;
            if (!cand && (e->flags & FL_BOT))
                cand = e;
        }

        if (!cand || members <= most)
            continue;

        most = members;
        pick = cand;
    }

    if (pick)
        return pick->client->pers.netname;

    for (i = 0; i < game.maxclients; i++) {
        e = &g_edicts[i + 1];

        if (!e->inuse || !e->client || !(e->flags & FL_BOT))
            continue;
        if (e->client->resp.teamnum < 0 || !teams[e->client->resp.teamnum].it)
            continue;

        n = TEAM(&teams[e->client->resp.teamnum])->arenanum;

        if (follow && RA_BotBoundFor(e, follow))
            n = follow;

        if (n != arenanum)
            continue;

        return e->client->pers.netname;
    }

    return NULL;
}

void track_SetStats(edict_t *ent)
{
    edict_t *target;
    int     score;

    target = ent->client->resp.track_target;
    score = ent->client->resp.score;

    memcpy(ent->client->ps.stats, target->client->ps.stats,
           sizeof(ent->client->ps.stats));

    ent->client->ps.stats[STAT_FRAGS] = score;

    if (ent->client->scoremode)
        ent->client->ps.stats[STAT_LAYOUTS] |= LAYOUTS_LAYOUT;
    else
        ent->client->ps.stats[STAT_LAYOUTS] &= ~LAYOUTS_LAYOUT;

    CTFSetIDView(ent);
}

void eyecam_think(edict_t *ent, usercmd_t *ucmd)
{
    edict_t     *target;
    vec3_t      forward;
    vec3_t      dest;
    vec3_t      zero = {0, 0, 0};
    int         i;

    target = ent->client->resp.track_target;
    if (!target || target->client->resp.fightstate != FIGHT_ALIVE) {
        track_next(ent);
        return;
    }

    gi.unlinkentity(ent);

    VectorCopy(target->s.origin, ent->s.origin);

    AngleVectors(target->client->v_angle, forward, NULL, NULL);
    VectorScale(forward, 20, dest);

    ent->s.origin[0] = ent->s.origin[0] + dest[0];
    ent->s.origin[1] = ent->s.origin[1] + dest[1];
    ent->s.origin[2] = ent->s.origin[2] + dest[2] + 22;

    VectorCopy(zero, ent->velocity);

    VectorCopy(target->client->v_angle, ent->s.angles);
    VectorCopy(target->client->v_angle, ent->client->ps.viewangles);
    VectorCopy(target->client->v_angle, ent->client->v_angle);

    for (i = 0; i < 3; i++)
        ent->client->ps.pmove.delta_angles[i] =
            ANGLE2SHORT(ent->s.angles[i] - ent->client->resp.cmd_angles[i]);

    gi.linkentity(ent);

    track_SetStats(ent);
}

void track_think(edict_t *ent, usercmd_t *ucmd)
{
    edict_t     *target;
    vec3_t      forward;
    vec3_t      dest;
    int         stuck = 0;
    vec3_t      zero = {0, 0, 0};
    vec3_t      mins = {-16, -16, -24};
    vec3_t      maxs = {16, 16, 32};
    trace_t     tr;
    int         i;

    target = ent->client->resp.track_target;
    if (!target || target->client->resp.fightstate != FIGHT_ALIVE) {
        track_next(ent);
        return;
    }

    VectorCopy(ent->client->ps.viewangles, forward);
    AngleVectors(forward, forward, NULL, NULL);

    VectorScale(forward, 150, dest);
    VectorSubtract(target->s.origin, dest, dest);

    tr = gi.trace(target->s.origin, zero, zero, dest, target, MASK_SOLID);
    if (tr.fraction < 1.0f) {
        VectorScale(forward, tr.fraction * -130, dest);
        VectorAdd(target->s.origin, dest, dest);
    }

    tr = gi.trace(ent->s.origin, mins, maxs, ent->s.origin, ent,
                  MASK_PLAYERSOLID);
    if (tr.contents & MASK_SOLID)
        stuck = 1;

    if (!stuck)
        tr = gi.trace(ent->s.origin, mins, maxs, dest, ent, MASK_SOLID);

    if (tr.fraction < 1.0f || stuck) {
        gi.unlinkentity(ent);
        VectorCopy(dest, ent->s.origin);
        gi.linkentity(ent);
        VectorCopy(zero, ent->velocity);
    } else {
        VectorSubtract(dest, ent->s.origin, dest);
        for (i = 0; i < 3; i++)
            ent->velocity[i] = dest[i] * 10;
    }

    track_SetStats(ent);
}

void track_change(edict_t *ent, int dir)
{
    edict_t     *target, *e;
    int         i;
    bool    wrapped;

    wrapped = false;

    target = ent->client->resp.track_target;
    if (!target) {
        target = &g_edicts[1];
        wrapped = true;
    } else if (target->client->resp.fightstate != FIGHT_ALIVE ||
               target->client->resp.context != ent->client->resp.context)
        wrapped = true;

    i = target - g_edicts;

    do {
        i += dir;
        if ((float) i > game.maxclients)
            i = 1;
        if (i < 1)
            i = (int) game.maxclients;

        e = &g_edicts[i];

        if (e->inuse &&
            e->client->resp.fightstate == FIGHT_ALIVE &&
            e->client->resp.context == ent->client->resp.context &&
            (!arenas[ent->client->resp.context].competition ||
             ent->client->resp.teamnum == e->client->resp.teamnum) &&
            e->solid) {
            wrapped = false;
            break;
        }
    } while (e != target);

    if (e != target || !wrapped) {
        ent->client->resp.track_target = e;
        gi.cprintf(ent, PRINT_HIGH, "Tracking %s\n", e->client->pers.netname);
        return;
    }

    ent->client->resp.omode = ent->client->resp.lastomode;
    move_to_arena(ent, ent->client->resp.context, 2);
    gi.cprintf(ent, PRINT_HIGH, "No one to track\n");
}

void track_next(edict_t *ent)
{
    track_change(ent, 1);
}

void track_prev(edict_t *ent)
{
    track_change(ent, -1);
}

void SetObserverMode(edict_t *ent)
{
    int     i;

    // A bot is a free-flying observer and nothing else.  The two camera modes
    // exist to be looked at, and a bot has no screen: it would spend the round
    // flying a camera nobody watches, and the fire gate is the only other thing
    // that would notice.  `arena.cfg`'s `competition: 1` reaches this the same
    // way the observer key does, so the guard belongs here and not only at the
    // input.
    if ((ent->flags & FL_BOT) &&
        (ent->client->resp.omode == OMODE_TRACKCAM ||
         ent->client->resp.omode == OMODE_EYECAM))
        ent->client->resp.omode = OMODE_FREEFLYING;

    switch (ent->client->resp.omode) {
    case OMODE_NORMAL:
        ent->movetype = MOVETYPE_WALK;
        ent->solid = SOLID_BBOX;
        ent->clipmask = MASK_PLAYERSOLID;
        ent->svflags &= ~SVF_NOCLIENT;
        ent->client->resp.track_target = NULL;
        ent->s.modelindex = 255;
        ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
        // The id row outlives the camera that wrote it.  CTFSetIDView is
        // reached from track_SetStats and from nowhere else, so it runs only
        // while a camera is running: an observer who tracked somebody and then
        // pressed ATTACK back to a mode with no subject kept that name pinned
        // to the bottom of its bar for the rest of the round, naming a player
        // it was no longer watching.  Cleared where the subject is.
        G_SetStat(ent, SID_RA_ID_VIEW, 0);
        break;

    case OMODE_FREEFLYING:
        ent->movetype = MOVETYPE_NOCLIP;
        ent->solid = SOLID_NOT;
        ent->clipmask = 0;
        ent->svflags |= SVF_NOCLIENT;
        ent->client->resp.track_target = NULL;
        ent->client->ps.pmove.pm_time = 0;
        ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
        ent->client->ps.pmove.pm_flags &= ~PMF_TIME_TELEPORT;
        G_SetStat(ent, SID_RA_ID_VIEW, 0);      // see OMODE_NORMAL above
        break;

    case OMODE_TRACKCAM:
        ent->movetype = MOVETYPE_NOCLIP;
        ent->solid = SOLID_NOT;
        ent->clipmask = 0;
        ent->svflags |= SVF_NOCLIENT;
        ent->s.modelindex = 0;
        ent->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;

        for (i = 0; i < 3; i++) {
            ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(-ent->client->resp.cmd_angles[i]);
            ent->s.angles[i] = 0;
        }

        VectorCopy(ent->s.angles, ent->client->ps.viewangles);
        VectorCopy(ent->s.angles, ent->client->v_angle);

        if (!ent->client->resp.track_target ||
            ent->client->resp.track_target->client->resp.fightstate != FIGHT_ALIVE)
            track_next(ent);
        break;

    case OMODE_EYECAM:
        ent->movetype = MOVETYPE_NOCLIP;
        ent->solid = SOLID_NOT;
        ent->clipmask = 0;
        ent->svflags |= SVF_NOCLIENT;
        ent->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;

        for (i = 0; i < 3; i++) {
            ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(-ent->client->resp.cmd_angles[i]);
            ent->s.angles[i] = 0;
        }

        VectorCopy(ent->s.angles, ent->client->ps.viewangles);
        VectorCopy(ent->s.angles, ent->client->v_angle);

        if (!ent->client->resp.track_target ||
            ent->client->resp.track_target->client->resp.fightstate != FIGHT_ALIVE)
            track_next(ent);
        break;
    }
}

void move_to_arena(edict_t *ent, int arenanum, int mode)
{
    edict_t     *dest;
    int         i;
    vec3_t      mins = {-16, -16, -24};
    vec3_t      maxs = {16, 16, 32};
    trace_t     tr;

    if (ent->client->resp.isbot) {
        gi.dprintf("\n%s IS A ZBOT %d\n", ent->client->pers.netname, ent->client->resp.isbot);
        gi.centerprintf(ent, "The server seems to think you\nare a bot. If you aren't,\n you may wish to reconnect");
    }

    if (mode) {

        if (!arenas[arenanum].active)
            dest = SelectFarthestArenaSpawnPoint("misc_teleporter_dest", arenanum, ent);
        else
            dest = SelectFarthestArenaSpawnPoint("info_player_deathmatch", arenanum, ent);

        if (arenanum) {
            if (ent->client->resp.context == 0) {
                ent->client->resp.context = arenanum;
                show_observer_menu(ent);
            }
        } else {
            ent->client->resp.track_target = NULL;
            if (ent->client->resp.teamnum != -1)
                show_arena_menu(ent);
        }

        ent->client->resp.context = arenanum;
    } else {
        //get rid of all menus
        ent->client->resp.context = arenanum;
        ClientUserinfoChanged(ent, ent->client->pers.userinfo);

        if (arenas[arenanum].idarena)
            dest = SelectRandomArenaSpawnPoint("info_player_deathmatch", arenanum,
                                               (TEAM(&teams[ent->client->resp.teamnum])->side == arenas[arenanum].sidepick) ? 1 : 2,
                                               ent);
        else
            dest = SelectFarthestArenaSpawnPoint("info_player_deathmatch", arenanum, ent);
    }

    if (!dest) {
        gi.bprintf(PRINT_HIGH, "no dest found\n");
        return;
    }

    gi.unlinkentity(ent);

    // A body entering the world needs a body.  `ent->mins`/`maxs` hold whatever
    // the client's last Pmove left there, and an arena observer's Pmove leaves
    // nothing: the pair is written in PM_CheckDuck, which the spectator and
    // freeze paths return before reaching, so `pm.mins`/`maxs` come back as the
    // zeros ClientThink's own memset put there and ClientThink copies them into
    // the edict.  Every fighter SendTeamToArena places has just come off one of
    // those paths, so the box was empty for the whole of the placement, and
    // that cost two things:
    //
    //   * KillBox below traces `ent->mins`/`maxs` -- a point at the origin,
    //     which misses a body the arriving fighter is standing inside, so
    //     neither the push nor the telefrag ever fired and `spawn_recheck` was
    //     left at the zero KillBox clears it to;
    //   * a body already on the pad is clipped through the same pair, so it was
    //     a point-sized obstacle: the floor trace stopped the arriving box 24
    //     units above its origin instead of on top of it, which is inside it
    //     once the first PM_NORMAL frame restores both boxes.  Measured on
    //     `ra2map26`: two fighters at (1024 544 184) and (1024 544 208), boxes
    //     overlapping by 32 units, each one's Pmove allsolid in the other, and
    //     nothing in the mod able to separate them.
    //
    // PutClientInServer sets the same pair before its own floor trace for
    // exactly this reason; the round-start path has no PutClientInServer in it.
    // The next Pmove overwrites both, standing or ducked, so this only has to
    // hold for the placement.
    VectorCopy(mins, ent->mins);
    VectorCopy(maxs, ent->maxs);

    // try to properly clip to the floor / spawn.  Q2PRO added this to
    // PutClientInServer(); in Rocket Arena the arena spot, not the spawn
    // point, is where the player actually lands, so it belongs here -- and it
    // is ArenaSpawnLanding's trace, because the selector has to be able to ask
    // the same question about a candidate that this answers about the choice.
    tr = ArenaSpawnLanding(dest, ent, ent->s.origin);
    if (!tr.allsolid && !tr.startsolid)
        ent->groundentity = tr.ent;
    VectorCopy(ent->s.origin, ent->s.old_origin);

    // clear the velocity and hold them in place briefly
    VectorClear(ent->velocity);

    ent->client->ps.pmove.pm_time = 160 >> PM_TIME_SHIFT;   // hold time
    ent->client->ps.pmove.pm_flags |= PMF_TIME_TELEPORT;

    // draw the teleport splash at source and on the player
    if (mode == 0)
        ent->s.event = EV_PLAYER_TELEPORT;

    // set angles
    for (i = 0; i < 3; i++)
        ent->client->ps.pmove.delta_angles[i] =
            ANGLE2SHORT(dest->s.angles[i] - ent->client->resp.cmd_angles[i]);

    VectorClear(ent->s.angles);
    VectorClear(ent->client->ps.viewangles);
    VectorClear(ent->client->v_angle);

    // telefrag avoidance at destination
    if (!KillBox(ent)) {
    }

    // Landing on somebody is not an overlap yet, which is why it has to be
    // watched.  When every point on this side is taken the selector returns one
    // of them anyway -- RA2's stated fallback, "no choice to turn one into a
    // telefrag meltdown" -- and the landing trace then stops the arriving body
    // on the occupant: two boxes that touch, which KillBox reads as clear
    // because touching is not overlapping, and which one Pmove later can be two
    // boxes that overlap.  check_telefrag() is the thing that would separate
    // them and it only examines a client whose `spawn_recheck` is set, so the
    // pair is handed to it here.  After KillBox, which clears the field on
    // entry.
    if (mode == 0 && tr.ent && tr.ent->client)
        ent->client->resp.spawn_recheck = level.framenum + 0.5f / FRAMETIME;

    if (mode) {
        if (arenas[arenanum].active && ent->client->resp.omode == OMODE_NORMAL)
            ent->client->resp.omode = OMODE_FREEFLYING;

        if (arenas[arenanum].competition && mode != 2)
            ent->client->resp.omode = OMODE_EYECAM;

        SetObserverMode(ent);
    } else {
        ent->client->resp.omode = OMODE_NORMAL;
        SetObserverMode(ent);
    }

    gi.linkentity(ent);

    if (arenas[arenanum].proposetime > level.time && !ent->client->resp.ra_voted) {
        menu_centerprint(ent, va("Settings changes have been proposed\n by %s!\nGoto the observer menu (TAB) to vote",
                                 arenas[arenanum].proposer->client->pers.netname));
        stuffcmd(ent, "play misc/pc_up.wav\n");
    }
}

void ChangeOMode(edict_t *ent)
{
    if (!ent->client->resp.fightstate) {
        if (ent->client->resp.omode != OMODE_TRACKCAM && ent->client->resp.omode != OMODE_EYECAM)
            ent->client->resp.lastomode = ent->client->resp.omode;

        ent->client->resp.omode = (ent->client->resp.omode + 1) % 4;

        gi.cprintf(ent, PRINT_HIGH, "Switched Observer Mode to: %s\n",
                   omode_descriptions[ent->client->resp.omode]);

        move_to_arena(ent, ent->client->resp.context, 1);
    }
}

int getfreeskin(int arenanum)
{
    bool    used[MAX_ARENA_SKINS];
    int         i;
    team_t      *t;

    memset(used, 0, sizeof(used));

    for (i = 0; i < MAX_TEAMS; i++) {
        t = teams[i].it;
        if (!t)
            continue;
        if (t->arenanum != arenanum)
            continue;
        if (t->skin == -1)
            continue;

        used[t->skin] = true;
    }

    for (i = 0; i < MAX_ARENA_SKINS; i++)
        if (!used[i])
            return i;

    return rand() % MAX_ARENA_SKINS;
}

void setteamskin(edict_t *ent, char *userinfo, int skinnum)
{
    char    *val;
    int     pnum;

    pnum = ent - g_edicts - 1;
    val = Info_ValueForKey(userinfo, "skin");

    if (val[0] == 'f') {
        if (strcmp(val, va("female/%s", teamskins[skinnum])))
            gi.configstring(game.csr.playerskins + pnum,
                            va("%s\\female/%s", ent->client->pers.netname, teamskins[skinnum]));

        Info_RemoveKey(userinfo, "skin");
        Q_strlcat(userinfo, va("\\skin\\female/%s", teamskins[skinnum]),
                  MAX_INFO_STRING);

        stuffcmd(ent, "skin female/nullxxx\n");
    } else if (val[0] == 'c' && val[1] == 'r') {
        if (strcmp(val, va("crakhor/%s", teamskins[skinnum])))
            gi.configstring(game.csr.playerskins + pnum,
                            va("%s\\crakhor/%s", ent->client->pers.netname, teamskins[skinnum]));

        Info_RemoveKey(userinfo, "skin");
        Q_strlcat(userinfo, va("\\skin\\crakhor/%s", teamskins[skinnum]),
                  MAX_INFO_STRING);

        stuffcmd(ent, "skin crakhor/nullxxx\n");
    } else if (val[0] == 'c' && val[1] == 'y') {
        if (strcmp(val, va("cyborg/%s", teamskins[skinnum])))
            gi.configstring(game.csr.playerskins + pnum,
                            va("%s\\cyborg/%s", ent->client->pers.netname, teamskins[skinnum]));

        Info_RemoveKey(userinfo, "skin");
        Q_strlcat(userinfo, va("\\skin\\cyborg/%s", teamskins[skinnum]),
                  MAX_INFO_STRING);

        stuffcmd(ent, "skin cyborg/nullxxx\n");
    } else {
        if (strcmp(val, va("male/%s", teamskins[skinnum])))
            gi.configstring(game.csr.playerskins + pnum,
                            va("%s\\male/%s", ent->client->pers.netname, teamskins[skinnum]));

        Info_RemoveKey(userinfo, "skin");
        Q_strlcat(userinfo, va("\\skin\\male/%s", teamskins[skinnum]),
                  MAX_INFO_STRING);

        stuffcmd(ent, "skin male/nullxxx\n");
    }
}

void SendTeamToArena(qmenu_t *team, int arenanum, bool observer, bool announce)
{
    qmenu_t     *mnode;
    edict_t     *ent;
    int         statsteam;

    statsteam = -1;
    mnode = team;

    if (!TEAM(team)->outofline) {
        if (arenanum && TEAM(team)->skin == -1
            && (arenas[arenanum].playersperteam > 1 || arenas[arenanum].idarena))
            TEAM(team)->skin = getfreeskin(arenanum);
        else if (!arenanum
                 || (arenas[arenanum].playersperteam == 1 && !arenas[arenanum].idarena))
            TEAM(team)->skin = -1;
    }

    if (!observer && announce && arenas[arenanum].stats) {
        statsteam = team - teams;

        RA2_Stats_AddTeam(arenas[arenanum].stats, statsteam, TEAM(team)->name);
    }

    while (mnode->next) {
        mnode = mnode->next;
        ent = (edict_t *)mnode->it;

        RA_ResolvePendingDeath(ent);

        if (TEAM(team)->skin != -1)
            setteamskin(ent, ent->client->pers.userinfo, TEAM(team)->skin);

        if (observer) {
            ent->client->resp.fightstate = FIGHT_SPECTATING;
            ent->takedamage = DAMAGE_NO;
            move_to_arena(ent, arenanum, 1);
        } else {
            ent->client->resp.fightstate = FIGHT_ALIVE;
            ent->takedamage = DAMAGE_NO;
            move_to_arena(ent, arenanum, 0);
            give_ammo(ent);

            if (statsteam != -1)
                RA2_Stats_AddPlayer(arenas[arenanum].stats, ent, statsteam);
        }
    }

    if (announce) {
        if (observer) {
            TEAM(team)->fighting = false;
            add_to_queue(&TEAM(team)->arenalink, &arenas[arenanum].waitingteams);
        } else {
            add_to_queue(&TEAM(team)->arenalink, &arenas[arenanum].activeteams);
        }
    }

    TEAM(team)->arenanum = arenanum;

    gi.dprintf("%d: %d %s entered\n", arenanum, TEAM(team)->teamnum,
               TEAM(team)->name);
}

int AddtoArena(edict_t *ent, int arenanum, int allow_partial, int skip_checks)
{
    int     membercount;
    int     teamnum;
    qmenu_t *teamnode;
    team_t  *team;

    if (!ent || !ent->client || !teams)
        return 1;

    teamnum = ent->client->resp.teamnum;
    if (arenanum < 1 || arenanum > num_arenas || arenanum > MAX_ARENAS ||
        teamnum < 0 || teamnum >= MAX_TEAMS)
        return 1;

    teamnode = &teams[teamnum];
    team = TEAM(teamnode);
    if (!team || team->teamnum != teamnum)
        return 1;

    if (!skip_checks) {
        if (arenas[arenanum].minping && ent->client->ping < arenas[arenanum].minping) {
            menu_centerprint(ent, va("Your ping is too low\nMinimum ping for this arena: %d", arenas[arenanum].minping));
            return 1;
        }

        if (arenas[arenanum].maxping && ent->client->ping > arenas[arenanum].maxping) {
            menu_centerprint(ent, va("Your ping is too high\nMaximum ping for this arena: %d", arenas[arenanum].maxping));
            return 1;
        }

        if (arenas[arenanum].locked) {
            menu_centerprint(ent, "Sorry, that Arena is locked by an admin\n");
            return 1;
        }

        if (arenas[arenanum].idarena) {
            menu_centerprint(ent, "You must join a pickup team to\n enter that arena");
            return 1;
        }

        if (count_queue(&arenas[arenanum].waitingteams) + count_queue(&arenas[arenanum].activeteams) >= arenas[arenanum].maxteams) {
            menu_centerprint(ent, "Sorry, that arena is full");
            return 1;
        }
    }

    membercount = count_queue(teamnode);

    if (!(membercount != arenas[arenanum].playersperteam
          && (membercount > arenas[arenanum].playersperteam || !allow_partial))) {
        team->outofline = skip_checks;

        if (!skip_checks) {
            remove_from_queue(&team->arenalink, NULL);
            SendTeamToArena(teamnode, arenanum, true, true);
        } else
            SendTeamToArena(teamnode, arenanum, true, false);

        return 0;
    }

    if (membercount < arenas[arenanum].playersperteam) {
        show_teamconfirm_menu(ent, arenanum);
        return 1;
    }

    menu_centerprint(ent, va("You have the incorrect number\nof team members, you need %d to play \nin that arena", arenas[arenanum].playersperteam));

    return 1;
}

void check_teams(int arenanum)
{
    qmenu_t     *tnode, *prev_tnode, *mnode;
    int         i;
    int         ping;
    bool    rejected;

    for (i = 0; i < MAX_TEAMS; i++) {
        if (!teams[i].it)
            continue;
        if (count_queue(&teams[i]) != 0)
            continue;

        if (TEAM(&teams[i])->locked)
            continue;

        remove_from_queue(&TEAM(&teams[i])->arenalink, NULL);
        gi.dprintf("Clearing team %d (%s)\n", TEAM(&teams[i])->teamnum,
                   TEAM(&teams[i])->name);
        gi.TagFree(TEAM(&teams[i])->name);
        gi.TagFree(TEAM(&teams[i]));
        teams[i].it = NULL;
    }

    if (!arenanum)
        return;

    tnode = &arenas[arenanum].waitingteams;

    while (tnode->next) {
        tnode = tnode->next;
        mnode = (qmenu_t *)tnode->it;
        rejected = false;

        if (!arenas[arenanum].idarena) {
            while (mnode->next) {
                mnode = mnode->next;

                ping = ((edict_t *)mnode->it)->client->ping;

                if ((ping > arenas[arenanum].maxping && ping < 1000)
                    || ping < arenas[arenanum].minping) {
                    rejected = true;
                    gi.cprintf((edict_t *)mnode->it, PRINT_HIGH,
                               "Sorry, your ping of %d does not work in this arena\n", ping);
                }
            }
        }

        if (count_queue((qmenu_t *)tnode->it) > arenas[arenanum].playersperteam || rejected) {
            gi.bprintf(PRINT_MEDIUM, "Removing team %d (%s)\n",
                       TEAM((qmenu_t *)tnode->it)->teamnum,
                       TEAM((qmenu_t *)tnode->it)->name);

            prev_tnode = tnode->prev;
            remove_from_queue(tnode, NULL);
            SendTeamToArena((qmenu_t *)tnode->it, 0, true, true);
            tnode = prev_tnode;
        }
    }

    if (arenas[arenanum].changed && count_queue(&arenas[arenanum].waitingteams) + count_queue(&arenas[arenanum].activeteams) == 0) {
        set_config(arenanum, arenanum);
        gi.dprintf("%d: Reseting to default config\n", arenanum);
    }
}

void init_player(edict_t *ent)
{
    // A client slot is reused, and the menu queue is TAG_LEVEL memory that
    // belonged to whoever had this slot before.  ra_MenuClose() deliberately
    // does not drop it -- closing a menu is hiding it -- so the one place that
    // must is here, where a client starts from nothing.
    ent->client->curmenulink = NULL;
    ent->client->selected = NULL;
    ent->client->menuqueue.next = NULL;

    ent->client->resp.teammember.it = ent;
    ent->client->resp.fightstate = FIGHT_SPECTATING;
    ent->client->resp.context = 0;
    ent->client->resp.teamnum = -1;

    // A bot gets neither: the motd is a message and the team list is a menu,
    // and a bot is never sent a layout.  Its two clicks are made for it in
    // RA_BotJoinArena, after placement.
    if (!(ent->flags & FL_BOT)) {
        if (ent->client->pers.showmotd)
            motd_menu(ent);
        else
            menuRefreshTeamList(ent, NULL, NULL, 0);
    }

    ent->client->resp.track_target = NULL;
    ent->client->resp.lastomode = OMODE_FREEFLYING;
    ent->takedamage = DAMAGE_NO;

    send_configstring(ent, game.csr.items + game.num_items + 2, " Red");
    send_configstring(ent, game.csr.items + game.num_items + 3, "Blue");
}

void reinit_player(edict_t *ent)
{
    ent->client->resp.fightstate = FIGHT_SPECTATING;
    ent->client->resp.track_target = NULL;
    ent->client->resp.lastomode = OMODE_FREEFLYING;
}

void show_stringc(char *s, int context)
{
    int     i;
    edict_t *e;

    for (i = 0; i < game.maxclients; i++) {
        e = &g_edicts[i + 1];
        if (e->inuse && e->client && e->client->resp.context == context) {
            gi.centerprintf(e, "%s", s);
        }
    }
}

void show_string(int priority, char *s, int context)
{
    int     i;
    edict_t *e;

    for (i = 0; i < game.maxclients; i++) {
        e = &g_edicts[i + 1];
        if (e->inuse && e->client && e->client->resp.context == context) {
            gi.cprintf(e, priority, "%s", s);
        }
    }
}

void send_sound_to_arena(char *soundname, int context)
{
    int     i;
    edict_t *e;

    for (i = 0; i < game.maxclients; i++) {
        e = &g_edicts[i + 1];
        if (e->inuse && e->client && e->client->resp.context == context) {
            stuffcmd(e, va("play %s\n", soundname));

        }
    }
}

void send_configstring(edict_t *e, int index, char *string)
{
    gi.WriteByte(svc_configstring);
    gi.WriteShort(index);
    gi.WriteString(string);
    gi.unicast(e, true);
}

void show_countdown(int countdown, int arenanum)
{
    int     i;
    edict_t *e;

    for (i = 0; i < game.maxclients; i++) {
        e = &g_edicts[i + 1];
        if (!e->inuse || !e->client)
            continue;
        if (e->client->resp.context != arenanum)
            continue;

        if (arenas[arenanum].state == 0)
            send_configstring(e, game.csr.items + game.num_items, "Waiting for match to start");
        else
            send_configstring(e, game.csr.items + game.num_items, arenas[arenanum].vs);

        if (arenas[arenanum].rounds > 1)
            send_configstring(e, game.csr.items + game.num_items + 1, va("Round %d of %d", arenas[arenanum].round, arenas[arenanum].rounds));
        else
            send_configstring(e, game.csr.items + game.num_items + 1, "");

        G_SetStat(e, SID_RA_ARENASTATUS, game.csr.items + game.num_items);
        G_SetStat(e, SID_RA_ROUNDINFO, game.csr.items + game.num_items + 1);
        G_SetStat(e, SID_RA_COUNTDOWN, countdown);

        if (countdown == 15 || countdown == 10 || countdown == 5) {
            if (e->client->menu_owner != MENU_ARENA)
                SendStatusBar(e, G_Statusbar(), true);
        }

        if (countdown > 0 && countdown < 4 && arenas[arenanum].state)
            stuffcmd(e, va("play ra/%d.wav\n", countdown));
        else if (!countdown && arenas[arenanum].state) {
            stuffcmd(e, "play ra/fight.wav\n");
            gi.centerprintf(e, "FIGHT!");
        }
    }
}

/*
==================
RA_TeamOf

The team a client is on, or NULL -- one named question, because "on a team" is
two facts and the sites that read it kept only one of them.

`resp.teamnum` is -1 for a client that has joined none, which is where
`teams[-1]` came from in Pickup_ScoreboardMessage; and a slot can be NULL for a
teamnum that is perfectly non-negative, which is where the level-change crash
came from (see arena_init).  A caller that asks this cannot get either wrong.
==================
*/
team_t *RA_TeamOf(edict_t *e)
{
    if (!e || !e->client)
        return NULL;
    if (e->client->resp.teamnum < 0 || e->client->resp.teamnum >= MAX_TEAMS)
        return NULL;

    return teams[e->client->resp.teamnum].it;
}

int show_rank(qmenu_t *node)
{
    int     count;

    count = 0;
    for (node = node->prev; node; node = node->prev)
        count++;

    return count;
}

bool check_for_teams(int arenanum)
{
    qmenu_t *tnode;
    int     i;

    if (count_queue(&arenas[arenanum].waitingteams) >= arenas[arenanum].numteams) {
        tnode = &arenas[arenanum].waitingteams;
        i = 0;
        while (tnode->next) {
            if (i >= arenas[arenanum].numteams)
                break;
            tnode = tnode->next;
            i++;
            if (count_queue((qmenu_t *)tnode->it) == 0)
                return false;
        }

        return true;
    }

    return false;
}

// `ra_botcycle`, the half RA2 does not have, because RA2 has no bots: "no bot
// hogs the arena while a person waits".
//
// Gladiator's own arena spells it as RA2_GetLongestWaitingHuman() -- pick the
// human who has been waiting longest, then everyone on their team, then fill
// the other side from the queue.  RA2's queue already is "longest waiting"
// (fill_arena pops the front and fight_done appends the losers), so the same
// rule here is: take the first waiting team that has a person on it, and fall
// back to the front of the queue when none does.  A server of only bots is
// therefore unaffected, and a person who joins never waits behind a bot.
static bool team_has_human(qmenu_t *tnode)
{
    qmenu_t *mnode = (qmenu_t *)tnode->it;

    while (mnode->next) {
        mnode = mnode->next;
        if (!(((edict_t *)mnode->it)->flags & FL_BOT))
            return true;
    }
    return false;
}

static qmenu_t *pop_next_team(int arenanum, bool prefer_human)
{
    qmenu_t *tnode;

    if (prefer_human) {
        for (tnode = arenas[arenanum].waitingteams.next; tnode; tnode = tnode->next) {
            if (team_has_human(tnode))
                return remove_from_queue(tnode, &arenas[arenanum].waitingteams);
        }
    }
    return remove_from_queue(NULL, &arenas[arenanum].waitingteams);
}

int fill_arena(int arenanum)
{
    qmenu_t *popped;
    int     count;
    int     firstskin;
    char    vs[256];

    firstskin = -1;
    vs[0] = 0;

    arenas[arenanum].sidepick = rand() % 2;

    for (count = 0; count < arenas[arenanum].numteams; count++) {
        // Only the first side is picked for a person: RA2's own shape, where
        // RA2_StartMatch asks for the longest-waiting human once and then takes
        // the longest-waiting anyone for the other side.  Doing it for every
        // side would empty the arena of bots and there would be nothing to play
        // against.
        popped = pop_next_team(arenanum,
                               count == 0 && ra_botcycle && ra_botcycle->value);

        if (!popped) {
            gi.dprintf("Team left during multi-round match\n");
            return 1;
        }

        if (firstskin == -1)
            firstskin = TEAM((qmenu_t *)popped->it)->skin;
        else if (firstskin == TEAM((qmenu_t *)popped->it)->skin) {
            gi.dprintf("Skin conflict in arena %d\n", arenanum);
            TEAM((qmenu_t *)popped->it)->skin = (firstskin + 1) % MAX_ARENA_SKINS;
        }

        SendTeamToArena((qmenu_t *)popped->it, arenanum, false, true);

        if (count)
            Q_strlcat(vs, " vs ", sizeof(vs));
        Q_strlcat(vs, TEAM((qmenu_t *)popped->it)->name, sizeof(vs));

        if (arenas[arenanum].round == 1)
            TEAM((qmenu_t *)popped->it)->wins = 0;

        TEAM((qmenu_t *)popped->it)->fighting = true;
    }

    Q_strlcpy(arenas[arenanum].vs, vs, sizeof(arenas[arenanum].vs));
    gi.dprintf("%d: %s\n", arenanum, arenas[arenanum].vs);

    return 1;
}

int fight_done(int arenanum)
{
    qmenu_t     *tnode, *mnode;
    edict_t     *e;
    int         winner;

    winner = -1;

    tnode = &arenas[arenanum].activeteams;

    while (tnode->next) {
        tnode = tnode->next;

        mnode = (qmenu_t *)tnode->it;

        while (mnode->next) {
            mnode = mnode->next;
            e = (edict_t *)mnode->it;

            if (e->takedamage != DAMAGE_AIM || e->deadflag != DEAD_NO)
                continue;

            if (winner == -1)
                winner = e->client->resp.teamnum;
            else if (winner != e->client->resp.teamnum)
                return -2;
        }
    }

    return winner;
}

static void loc_buildboxpoints(vec3_t p[8], vec3_t org, vec3_t mins, vec3_t maxs)
{
    VectorAdd(org, mins, p[0]);
    VectorCopy(p[0], p[1]);
    p[1][0] -= mins[0];
    VectorCopy(p[0], p[2]);
    p[2][1] -= mins[1];
    VectorCopy(p[0], p[3]);
    p[3][0] -= mins[0];
    p[3][1] -= mins[1];
    VectorAdd(org, maxs, p[4]);
    VectorCopy(p[4], p[5]);
    p[5][0] -= maxs[0];
    VectorCopy(p[0], p[6]);
    p[6][1] -= maxs[1];
    VectorCopy(p[0], p[7]);
    p[7][0] -= maxs[0];
    p[7][1] -= maxs[1];
}

static q_unused bool loc_CanSee(edict_t *targ, edict_t *inflictor)
{
    trace_t trace;
    vec3_t  targpoints[8];
    int     i;
    vec3_t  viewpoint;

// bmodels need special checking because their origin is 0,0,0
    if (targ->movetype == MOVETYPE_PUSH)
        return false;       // bmodels not supported

    loc_buildboxpoints(targpoints, targ->s.origin, targ->mins, targ->maxs);

    VectorCopy(inflictor->s.origin, viewpoint);
    viewpoint[2] += inflictor->viewheight;

    for (i = 0; i < 8; i++) {
        trace = gi.trace(viewpoint, vec3_origin, vec3_origin, targpoints[i], inflictor, MASK_SOLID);
        if (trace.fraction == 1.0f)
            return true;
    }

    return false;
}

/*
==================
CTFSetIDView

The name of whoever the tracking camera -- or, failing that, the crosshair -- is
on, for the `stat_string` row at the bottom of an observer's bar.

The configstring is `general`, not `playerskins`.  `stat_string` draws a
configstring verbatim, and `playerskins + n` is "name\model/skin" --
ClientUserinfoChanged and setteamskin() both write it in exactly that form -- so
the row read "Sarge\male/red" instead of "Sarge".  Threewave's own id view
answers this by keeping the bare name in a configstring of its own; RA2 reached
for the one it already had.  The write is now made under arena too, so the base
moves here and nothing else changes.
==================
*/
void CTFSetIDView(edict_t *ent)
{
    vec3_t      forward;
    trace_t     tr;

    G_SetStat(ent, SID_RA_ID_VIEW, 0);

    if (ent->client->resp.fightstate)
        return;

    if (ent->client->resp.track_target) {
        G_SetStat(ent, SID_RA_ID_VIEW,
                  game.csr.general + (ent->client->resp.track_target - g_edicts) - 1);
        return;
    }

    AngleVectors(ent->client->v_angle, forward, NULL, NULL);
    VectorScale(forward, 1024, forward);
    VectorAdd(ent->s.origin, forward, forward);

    tr = gi.trace(ent->s.origin, NULL, NULL, forward, ent, CONTENTS_SOLID | CONTENTS_MONSTER);

    if (tr.fraction < 1 && tr.ent && tr.ent->client && tr.ent->solid)
        G_SetStat(ent, SID_RA_ID_VIEW,
                  game.csr.general + (tr.ent - g_edicts) - 1);
}

void UpdateStatusBars(int arenanum)
{
    qmenu_t *tnode, *tslot, *mnode;
    edict_t *e;
    int     numteams;
    int     ti, i, y, n;
    int     membercount[MAX_STATUS_TEAMS];
    char    *teamname[MAX_STATUS_TEAMS];
    char    *names[MAX_STATUS_TEAMS][MAX_STATUS_MEMBERS];
    int     health[MAX_STATUS_TEAMS][MAX_STATUS_MEMBERS];
    char    *p;
    char    string[1400];
    int     linepos, idview;

    tnode = &arenas[arenanum].activeteams;
    numteams = -1;
    while (tnode->next) {
        // `numteams` is the index about to be written, so the guard is the last
        // valid index and not the count.  RA2 tested against the count, so a
        // third active team wrote teamname[2], membercount[2] and two rows of
        // names[2][]/health[2][] one past three stack arrays -- and the member
        // guard three lines down has the right shape to compare it against.
        if (numteams >= MAX_STATUS_TEAMS - 1)
            break;
        numteams++;
        tnode = tnode->next;

        tslot = (qmenu_t *)tnode->it;
        teamname[numteams] = ((team_t *)tslot->it)->name;
        membercount[numteams] = -1;

        for (mnode = tslot->next; mnode; mnode = mnode->next) {
            if (membercount[numteams] >= MAX_STATUS_MEMBERS - 1)
                break;

            e = (edict_t *)mnode->it;
            if (e->takedamage != DAMAGE_AIM)
                continue;
            if (e->deadflag != DEAD_NO)
                continue;

            membercount[numteams]++;
            names[numteams][membercount[numteams]] = e->client->pers.netname;
            health[numteams][membercount[numteams]] = e->health;
        }
    }

    y = 40;

    // The slot numbers are resolved, not spelled.  This function composes a bar
    // of its own rather than going through G_Statusbar(), and RA2 wrote `num 2
    // 19` and `stat_string 20` as literals -- the two numbers arena's column of
    // STATSLOT_MAP happens to give SID_RA_LINEPOSITION and SID_RA_ID_VIEW.  The
    // writes a few lines below go through G_SetStat, which reads that map, so a
    // literal here is a second copy of a number only one of the two would
    // notice changing; and G_InitStats() may drop a slot the running
    // configuration cannot reach, which a literal cannot express at all.  Ask
    // the map, and omit the row it has no slot for.
    linepos = G_Stat(SID_RA_LINEPOSITION);
    idview  = G_Stat(SID_RA_ID_VIEW);

    if (linepos >= 0)
        Q_snprintf(string, sizeof(string),
                   "xl 8 yb -10 string2 \"Line Position:\" xl 100 yb -24 num 2 %d ",
                   linepos);
    else
        Q_strlcpy(string, "xl 8 yb -10 string2 \"Line Position:\" ", sizeof(string));

    p = string + strlen(string);
    if (!arenas[arenanum].competition) {
        for (ti = 0; ti <= numteams; ti++) {
            Q_snprintf(p, sizeof(string) - (p - string),
                       "xl %d yt %d string2 \"%s\" ", 8, y, teamname[ti]);
            p = string + strlen(string);
            y += 8;

            for (i = 0; i <= membercount[ti]; i++) {
                Q_snprintf(p, sizeof(string) - (p - string),
                           "xl %d yt %d string2 \"%s: %d\" ", 8, y,
                           names[ti][i], health[ti][i]);
                p = string + strlen(string);
                y += 8;
            }

            y += 8;
        }
    }

    if (idview >= 0)
        Q_snprintf(p, sizeof(string) - (p - string),
                   "if %d xv 0 yb -58 stat_string %d endif ", idview, idview);

    for (n = 0; n < game.maxclients; n++) {
        e = &g_edicts[n + 1];

        if (!e->inuse || !e->client)
            continue;
        if (e->client->resp.context != arenanum)
            continue;
        if (e->client->resp.fightstate != FIGHT_SPECTATING)
            continue;
        if (e->client->menu_owner == MENU_ARENA)
            continue;

        if (e->client->resp.track_target || e->client->scoremode) {
            SendStatusBar(e, G_Statusbar(), true);
        } else {
            team_t *t = RA_TeamOf(e);

            // An observer in this arena with no team is not a contradiction --
            // it is a client whose ClientBegin has not run yet, or one that
            // just left a team -- and its line position is nothing rather than
            // a read through a NULL slot.
            G_SetStat(e, SID_RA_LINEPOSITION, t ? show_rank(&t->arenalink) : 0);
            SendStatusBar(e, string, true);
        }
    }
}

void check_telefrag(int arenanum)
{
    int         i;
    edict_t     *e;
    trace_t     tr;
    vec3_t      angles;
    vec3_t      forward;

    for (i = 0; i < game.maxclients; i++) {
        e = &g_edicts[i + 1];

        if (!e->inuse)
            continue;
        if (!e->client)
            continue;
        if (e->client->resp.context != arenanum)
            continue;
        if (!e->client->resp.fightstate)
            continue;
        if (!e->client->resp.spawn_recheck)
            continue;
        if (e->client->resp.spawn_recheck > level.framenum)
            continue;

        tr = gi.trace(e->s.origin, e->mins, e->maxs, e->s.origin, NULL, MASK_PLAYERSOLID);

        if (tr.contents == CONTENTS_SOLID) {
            e->solid = SOLID_NOT;

            angles[1] = rand() % 360;
            angles[0] = 0;
            angles[2] = 0;
            AngleVectors(angles, forward, NULL, NULL);
            VectorScale(forward, 600, forward);
            VectorAdd(e->velocity, forward, e->velocity);

            e->client->resp.spawn_recheck = level.framenum + 0.5f / FRAMETIME;
        } else {
            e->solid = SOLID_BBOX;

            gi.unlinkentity(e);
            KillBox(e);
            gi.linkentity(e);
        }
    }
}

void start_voting(edict_t *proposer, int arenanum)
{
    int         i;
    edict_t     *cl_ent;

    if (arenas[arenanum].state == ASTATE_FIGHTING || arenas[arenanum].state == ASTATE_COUNTDOWN)
        arenas[arenanum].proposetime = level.time + 30000;
    else
        arenas[arenanum].proposetime = level.time + 30;

    arenas[arenanum].votetries = arenas[arenanum].votes_no = arenas[arenanum].votes_yes = 0;
    arenas[arenanum].proposer = proposer;

    for (i = 0; i < game.maxclients; i++) {
        cl_ent = &g_edicts[i + 1];
        if (!cl_ent->inuse)
            continue;

        if (!cl_ent->client)
            continue;

        if (cl_ent->client->resp.context != arenanum)
            continue;

        cl_ent->client->resp.ra_voted = false;
        arenas[arenanum].votetries++;

        if (cl_ent->client->resp.fightstate != FIGHT_SPECTATING)
            continue;

        if (cl_ent != proposer) {
            menu_centerprint(cl_ent, va("Settings changes have been proposed\nby %s!\nGoto the observer menu (TAB) to vote",
                                        arenas[arenanum].proposer->client->pers.netname));
            stuffcmd(cl_ent, "play misc/pc_up.wav\n");
        } else {
            stuffcmd(cl_ent, "play misc/pc_up.wav\n");
        }
    }

    gi.dprintf("Starting Voting in Arena %d with %d voters\n", arenanum, arenas[arenanum].votetries);
}

void check_voting(int arenanum)
{
    int         i;
    edict_t     *cl_ent;
    char        msg[80];

    if (!arenas[arenanum].proposetime)
        return;

    if (arenas[arenanum].proposetime > level.time)
        return;

    arenas[arenanum].proposetime = 0;

    if (arenas[arenanum].votes_yes - arenas[arenanum].votes_no >= (float)arenas[arenanum].votetries * (1.0f / 3.0f)) {
        memcpy(&arenas[arenanum].playersperteam, &arenas[arenanum].proposed, sizeof(arena_settings_t));
        arenas[arenanum].changed = true;

        Q_snprintf(msg, sizeof(msg),
                   "Changes Passed! Yes votes: %d No votes: %d\n",
                   arenas[arenanum].votes_yes, arenas[arenanum].votes_no);
    } else {
        Q_snprintf(msg, sizeof(msg),
                   "Changes Failed! Yes votes: %d No votes: %d\n",
                   arenas[arenanum].votes_yes, arenas[arenanum].votes_no);
    }

    for (i = 0; i < game.maxclients; i++) {
        cl_ent = &g_edicts[i + 1];
        if (!cl_ent->inuse)
            continue;

        if (!cl_ent->client)
            continue;

        if (cl_ent->client->resp.context != arenanum)
            continue;

        gi.cprintf(cl_ent, PRINT_CHAT, "%s", msg);

        if (arenas[arenanum].changed)
            cl_ent->client->resp.ra_votes = votetries_setting;
    }

    gi.dprintf("%s", msg);

    check_teams(arenanum);
}

void arena_think(int arenanum)
{
    int         winner;
    int         morewins;
    qmenu_t     *tnode, *popped;
    arena_t     *arena;

    arena = &arenas[arenanum];

    check_teams(arenanum);
    check_voting(arenanum);
    check_telefrag(arenanum);

    if (arena->state == ASTATE_COUNTDOWN || arena->state == ASTATE_WARMUP) {
        if (arena->countdown_next_tick == 0) {
            arena->countdown_next_tick = level.framenum + 1 / FRAMETIME;

            if (arena->state == ASTATE_WARMUP)
                arena->countdown = 15;
            else if (arena->proposetime > level.time)
                arena->countdown = 10;
            else
                arena->countdown = 5;

            show_countdown(arena->countdown, arenanum);
            return;
        }

        if (arena->countdown_next_tick >= level.framenum)
            return;

        show_countdown(--arena->countdown, arenanum);

        if (arena->countdown == 0) {
            arena->countdown_next_tick = 0;

            if (arena->state == ASTATE_WARMUP) {
                if (!check_for_teams(arenanum)) {
                    arena->state = ASTATE_ROUNDEND;
                    show_stringc("Not enough teams to start", arenanum);
                    return;
                }

                if (arena->idarena == 1) {
                    RA2_Stats_End(arena->stats);
                    arena->stats = RA2_Stats_Begin(arenanum);
                }

                arena->state = ASTATE_COUNTDOWN;
                fill_arena(arenanum);
                return;
            }

            arena->state = ASTATE_FIGHTING;
            set_damage(arenanum, DAMAGE_AIM);
            return;
        }

        arena->countdown_next_tick = level.framenum + 1 / FRAMETIME;
        return;
    } else if (arena->state == ASTATE_FIGHTING && !broken) {
        UpdateStatusBars(arenanum);

        if (fight_done(arenanum) <= -2)
            return;

        arena->state = ASTATE_RESULTS;
        return;
    } else if (arena->state == ASTATE_ROUNDEND) {
        if (!check_for_teams(arenanum))
            return;

        arena->round = 1;

        if (arenas[arenanum].idarena) {
            arena->state = ASTATE_WARMUP;
            return;
        }

        if (arena->idarena == 1) {
            RA2_Stats_End(arena->stats);
            arena->stats = RA2_Stats_Begin(arenanum);
        }

        arena->state = ASTATE_COUNTDOWN;
        fill_arena(arenanum);
        return;
    } else if (arena->state == ASTATE_RESULTS) {
        UpdateStatusBars(arenanum);

        if (arena->countdown_next_tick == 0) {
            arena->countdown_next_tick = level.framenum + 3 / FRAMETIME;
            return;
        }

        if (arena->countdown_next_tick >= level.framenum)
            return;

        arena->state = ASTATE_NEXTROUND;
        arena->countdown_next_tick = 0;

        if (arena->proposetime - level.time <= 30)
            return;

        arena->proposetime = level.time + 30;
        return;
    } else if (arena->state == ASTATE_NEXTROUND) {
        winner = fight_done(arenanum);

        if (winner == -1)
            Q_strlcpy(arena->msg, "It was a tie!", sizeof(arena->msg));
        else {
            RA2_Stats_TeamScore(arena->stats, winner, 1);

            if (++((team_t *)teams[winner].it)->wins > arenas[arenanum].rounds / 2) {
                Q_snprintf(arena->msg, sizeof(arena->msg), "%s has won the match!!",
                           ((team_t *)teams[winner].it)->name);
                arena->round = arena->rounds;

                RA2_Stats_End(arena->stats);
                arena->stats = NULL;
            } else
                Q_snprintf(arena->msg, sizeof(arena->msg), "%s has won the round!",
                           ((team_t *)teams[winner].it)->name);
        }

        RA2_Stats_Write(arena->stats);

        if (winner == -1) {
            if (count_queue(&arenas[arenanum].activeteams) != 0) {
                if (count_queue((qmenu_t *)arenas[arenanum].activeteams.next->it) != 0)
                    arena->round--;
            }
        }

        gi.dprintf("%d: %d %s\n", arenanum, winner, arena->msg);
        set_damage(arenanum, DAMAGE_NO);
        show_stringc(arena->msg, arenanum);
        tnode = &arenas[arenanum].activeteams;
        arenas[arenanum].sidepick = rand() % 2;

        while (tnode->next) {
            tnode = tnode->next;

            if (arena->round < arenas[arenanum].rounds) {
                morewins = arenas[arenanum].rounds / 2 + 1 - ((team_t *)((qmenu_t *)tnode->it)->it)->wins;
                Q_snprintf(arena->msg, sizeof(arena->msg), "%s has %d wins and needs %d more to take the match\n", ((team_t *)((qmenu_t *)tnode->it)->it)->name, ((team_t *)((qmenu_t *)tnode->it)->it)->wins, morewins);
                show_string(2, arena->msg, arenanum);
                SendTeamToArena((qmenu_t *)tnode->it, arenanum, false, false);
            } else {
                popped = remove_from_queue(NULL, &arenas[arenanum].activeteams);
                ((team_t *)((qmenu_t *)popped->it)->it)->fighting = false;
                tnode = &arenas[arenanum].activeteams;

                // `ra_playercycle`, and RA2 already implements the behaviour:
                // "winners kept between matches" is the winning team going to
                // the front of the waiting queue, because fill_arena pops from
                // the front.  What was missing is the cvar, so a server could
                // not turn it off -- with `ra_playercycle 0` a winning team
                // queues like everybody else and the arena rotates strictly by
                // arrival.
                if ((((team_t *)((qmenu_t *)popped->it)->it)->teamnum == winner || winner == -1)
                    && (!ra_playercycle || ra_playercycle->value))
                    add_to_front_queue(popped, &arenas[arenanum].waitingteams);
                else
                    add_to_queue(popped, &arenas[arenanum].waitingteams);
            }
        }

        if (arena->round < arenas[arenanum].rounds) {
            RA2_Stats_NextRound(arena->stats);

            arena->round++;
            arena->state = ASTATE_COUNTDOWN;
            return;
        }

        arena->state = ASTATE_ROUNDEND;
        return;
    }
}

void multi_arena_think(void)
{
    int     i;

    if (level.intermission_framenum)
        return;

    i = level.framenum % (num_arenas * 2);
    if (i % 2)
        return;

    arena_think(i / 2 + 1);
}

void arena_init(edict_t *wsent)
{
    int     i;
    team_t  *t;
    char    *name;

    if (!wsent)
        return;

    teams = gi.TagMalloc(MAX_TEAMS * sizeof(qmenu_t), TAG_LEVEL);
    memset(teams, 0, MAX_TEAMS * sizeof(qmenu_t));
    memset(arenas, 0, sizeof(arenas));

    // A non-negative `resp.teamnum` must index a live team, and the line above
    // is where that would stop being true if nothing had already seen to it.
    // The clear goes where the array is dropped rather than where the
    // replacement is made -- g_spawn.c's loop at the gi.FreeTags(TAG_LEVEL),
    // beside the four menu handles that are the same contract -- and not here.
    // The read side is RA_TeamOf(), which asks both halves of the question.

    admincode = gi.cvar("admincode", "0", 0);
    // RA2's two, defaulting on, which is RA2's own unconditional behaviour.
    // p_botmenu.c obtains both with the same default and toggles them.
    ra_playercycle = gi.cvar("ra_playercycle", "1", 0);
    ra_botcycle = gi.cvar("ra_botcycle", "1", 0);

    num_arenas = wsent->arena;  //worldspawn arena flag is # of arenas
    if (num_arenas < 0 || num_arenas > MAX_ARENAS) {
        gi.dprintf("Invalid worldspawn arena count %d; using one id arena\n",
                   num_arenas);
        num_arenas = 1;
        idmap = true;
    } else if (!num_arenas) {
        num_arenas = 1;
        idmap = true;
    } else
        idmap = false;

    load_config(num_arenas + 1);
    set_config(1, num_arenas);

    for (i = 0; i <= num_arenas; i++) {
        arenas[i].state = ASTATE_ROUNDEND;
        arenas[i].stats = NULL;
        arenas[i].active = idmap;
        arenas[i].numteams = 2;
        arenas[i].waitingteams.prev = NULL;
        arenas[i].activeteams.prev = NULL;
        arenas[i].waitingteams.next = NULL;
        arenas[i].activeteams.next = NULL;
        arenas[i].countdown_next_tick = 0;
        arenas[i].countdown = 0;
        arenas[i].proposetime = 0;
        arenas[i].round = 0;

        if (!SelectFarthestArenaSpawnPoint("misc_teleporter_dest", i, NULL)) {
            gi.dprintf("Setting arena %d to idarena mode\n", i);
            arenas[i].active = true;
        }

        if (i && arenas[i].idarena) {
            name = gi.TagMalloc(ARENA_TEAMNAME_SIZE, TAG_LEVEL);
            Q_snprintf(name, ARENA_TEAMNAME_SIZE, "#%d Pickup Red", i);
            t = add_to_team(NULL, name);
            // add_to_team can refuse now (the MAX_TEAMS guard above), and this
            // caller dereferenced it unconditionally.
            if (!t) {
                gi.TagFree(name);
                continue;
            }
            t->side = 0;
            SendTeamToArena(t->arenalink.it, i, true, true);
            arenas[i].pickupteam[0] = t;

            name = gi.TagMalloc(ARENA_TEAMNAME_SIZE, TAG_LEVEL);
            Q_snprintf(name, ARENA_TEAMNAME_SIZE, "#%d Pickup Blue", i);
            t = add_to_team(NULL, name);
            if (!t) {
                gi.TagFree(name);
                continue;
            }
            t->side = 1;
            SendTeamToArena(t->arenalink.it, i, true, true);
            arenas[i].pickupteam[1] = t;

            arenas[i].maxteams = 2;
            arenas[i].playersperteam = 128;
        }
    }

    load_motd();
}

// Prefixed and reached from g_misc.c's per-ruleset dispatcher, which already
// chose between Threewave's and Ground Zero's.
void ra_SP_trigger_teleport(edict_t *ent)
{
    ent->touch = teleporter_touch;
    ent->movetype = MOVETYPE_NONE;
    ent->svflags |= SVF_NOCLIENT;
    ent->solid = SOLID_TRIGGER;
    ent->use = NULL;
    gi.setmodel(ent, ent->model);
    gi.linkentity(ent);
}

void ra_SP_func_illusionary(edict_t *ent)
{
    ent->movetype = MOVETYPE_NONE;
    ent->solid = SOLID_NOT;
    gi.setmodel(ent, ent->model);
    gi.linkentity(ent);
}

// RA2's SP_info_teleport_destination was here and was an empty body -- the
// classname has to exist so ED_CallSpawn does not reject the entity, and RA2
// needs nothing else from it.  g_misc.c already owns that classname and
// dispatches it per ruleset, so an empty stub here is a link collision rather
// than a feature; the arena arm is a row in that dispatcher.

// RA2's grapple was here -- 280 lines, CTFPlayerResetGrapple through
// CTFWeapon_Grapple, function for function the same set Threewave ships.  One
// grapple ships, and it is src/ctf/g_ctf.c's.  RA2's is a fork of it, not a
// rival design, so this is a deletion rather than a choice between two
// implementations.  What RA2 tunes on top of it -- `allow_grapple` -- is a
// parameter on that code.


/*
==================
RA_Obituary

RA2's side of a death: the round statistics, the score, and the announcer.  The
obituary text stays baseq2's (merged with Threewave's and both mission packs');
RA2's copy of it is a stale fork of id's with sounds bolted on -- so this is the
part that is RA2's feature and nothing else.

The score is all of it.  This used to take only the suicide half and leave the
rest to the baseq2 obituary underneath, which double-counted every one of them:
`ClientObituary` calls this first and then reaches its own `resp.score--` on the
very same paths, so a fall, a lava bath, a drowning, a trigger_hurt or a
self-rocket cost two frags where 1999 costs one.  The kill half was worse than
double-counted, it was wrong: baseq2 keys the team-kill penalty on
MOD_FRIENDLY_FIRE, which `T_Damage` only raises when `dmflags` carries the team
bits, and arena never sets them -- so a team kill scored +1.  And
`scorebydamage` suppresses frag scoring altogether in RA2, because there the
number is `damagedealt / 100` and g_combat.c owns it; baseq2 knows nothing about
that and kept adding to it.

So the arithmetic lives here, once, in the donor's own shape, and the three
baseq2 sites are gated off under arena (p_client.c).

The announcer grades a kill by how much health the winner had left, which is
why it needs the arena's configured starting health rather than a constant.
==================
*/
void RA_Obituary(edict_t *self, edict_t *inflictor, edict_t *attacker)
{
    int ctx = self->client->resp.context;
    int mod = meansOfDeath & ~MOD_FRIENDLY_FIRE;
    int stat;

    RA2_Stats_Add(arenas[ctx].stats, self - g_edicts, RA2_STAT_DEATHS, 1);

    if (!attacker || !attacker->client || attacker == self) {
        // The suicide announcer, and it is about the OPPONENT rather than the
        // dead player: `self->enemy` is whoever was hurting them, so a rocket
        // eaten at the wrong moment is graded by how close that fight had got.
        // Only for a real self-kill -- a fall or a trigger_hurt is an `attacker`
        // with no client and no arena to sound into.  `takedamage == DAMAGE_AIM`
        // is RA2's "still in this round" (set_damage), which is why the test is
        // against that constant rather than against `deadflag`.
        if (attacker == self && self->enemy && self->enemy != self &&
            self->enemy->inuse && self->enemy->client &&
            self->enemy->takedamage == DAMAGE_AIM) {
            if (self->enemy->health >= arenas[ctx].health)
                send_sound_to_arena("ra/outstand.wav", ctx);
            else if (self->enemy->health >= arenas[ctx].health - 20)
                send_sound_to_arena("ra/welldone.wav", ctx);
            else if (self->health < -40)
                send_sound_to_arena("ra/animality.wav", ctx);
        }

        // A suicide costs a frag unless the arena scores by damage, in which
        // case the score is not a frag count at all.
        if (!arenas[ctx].scorebydamage) {
            self->client->resp.score--;
            RA2_Stats_Add(arenas[ctx].stats, self - g_edicts, RA2_STAT_SCORE, -1);
        }

        // RA2 counts this one differently in the two arms this branch merges,
        // and the asymmetry is deliberate rather than an oversight: a real
        // self-kill (`attacker == self`) is a suicide however the arena scores,
        // but a death with no attacker at all -- a fall, lava, a trigger_hurt --
        // is counted only when the arena scores by frags.  Both arms of
        // ClientObituary are a byte-exact match against the 1999 binary, so this
        // is what the statsfile is supposed to say.
        if (attacker == self || !arenas[ctx].scorebydamage)
            RA2_Stats_Add(arenas[ctx].stats, self - g_edicts, RA2_STAT_SUICIDES, 1);
        return;
    }

    if (attacker->client->resp.isbot)
        gi.dprintf("%s is flagged as a ZBot (%d)\n",
                   attacker->client->pers.netname, attacker->client->resp.isbot);

    ctx = attacker->client->resp.context;

    if (attacker->health >= arenas[ctx].health) {
        if (self->health < -40)
            send_sound_to_arena("ra/fatality.wav", ctx);
        else
            send_sound_to_arena("ra/flawless.wav", ctx);
    } else if (attacker->health >= arenas[ctx].health - 20) {
        send_sound_to_arena("ra/excelent.wav", ctx);
    }

    // The per-weapon kill counters, and they had never been written: all four
    // were declared in ra2stats.h and incremented nowhere, so the breakdown in
    // every round record was a column of zeroes.
    //
    // RA2 buckets them by the inflictor's model -- grenade, rocket, then the
    // attacker's held weapon for the rail -- which cannot see the forty-odd
    // means of death this merged tree has, and which mis-files a held grenade
    // as whatever the shooter happened to be carrying.  `meansOfDeath` is the
    // same question asked by name, and is what the obituary text above already
    // switches on.
    switch (mod) {
    case MOD_GRENADE:
    case MOD_G_SPLASH:
    case MOD_HANDGRENADE:
    case MOD_HG_SPLASH:
    case MOD_HELD_GRENADE:
        stat = RA2_STAT_GRENADEKILLS;
        break;
    case MOD_ROCKET:
    case MOD_R_SPLASH:
        stat = RA2_STAT_ROCKETKILLS;
        break;
    case MOD_RAILGUN:
        stat = RA2_STAT_RAILKILLS;
        break;
    default:
        stat = RA2_STAT_OTHERKILLS;
        break;
    }
    RA2_Stats_Add(arenas[ctx].stats, attacker - g_edicts, stat, 1);

    // The frag, with RA2's own team rule on it.  `OnSameTeam` is teamnum
    // equality under arena (g_cmds.c), so this is the only thing in the tree
    // that can charge a team kill here -- baseq2's arm wants dmflags bits that
    // arena never sets.
    if (OnSameTeam(attacker, self)) {
        if (!arenas[ctx].scorebydamage) {
            attacker->client->resp.score--;
            RA2_Stats_Add(arenas[ctx].stats, attacker - g_edicts, RA2_STAT_SCORE, -1);
        }
        stuffcmd(self, "say As long as you're helping them, just shoot yourself!\n");
    } else if (!arenas[ctx].scorebydamage) {
        attacker->client->resp.score++;
        RA2_Stats_Add(arenas[ctx].stats, attacker - g_edicts, RA2_STAT_SCORE, 1);
    }
}

/*
==================
RA_ZBotSample

RA2's aim-cheat detector, sampled once per client command.  A ZBot snaps the
view to a target and back, which shows up as this frame's angles matching two
frames ago while last frame's did not; ten of those inside five seconds marks
the client.  Impulses 161..179 are the cooperating-bot handshake, which marks it
honestly instead.

Kept as RA2 has it and not acted on here -- `resp.isbot` is only read by RA2's
own reporting.  Whether a heuristic like this should be enabled by default on a
public server is a separate question.
==================
*/
void RA_ZBotSample(edict_t *ent, usercmd_t *ucmd)
{
    gclient_t *cl = ent->client;

    if (ucmd->impulse > 160 && ucmd->impulse < 180)
        cl->resp.isbot = 2;

    if (cl->resp.isbot)
        return;

    if (level.time - cl->resp.zbotlastcheck > 5.0f)
        cl->resp.zbotcount = 0;

    if (cl->oldangles[0][0] == ucmd->angles[0] && cl->oldangles[1][0] != ucmd->angles[0] &&
        cl->oldangles[0][1] == ucmd->angles[1] && cl->oldangles[1][1] != ucmd->angles[1]) {
        cl->resp.zbotlastcheck = level.time;
        cl->resp.zbotcount++;
    }

    cl->oldangles[0][0] = cl->oldangles[1][0];
    cl->oldangles[1][0] = ucmd->angles[0];
    cl->oldangles[0][1] = cl->oldangles[1][1];
    cl->oldangles[1][1] = ucmd->angles[1];

    if (cl->resp.zbotcount > 10)
        cl->resp.isbot = 3;
}

/*
==================
RA_HookThink

RA2's offhand grapple, once per client command.  The HOOK is Threewave's -- sec
7 rule 6 chose one implementation and deleted RA2's fork of it -- but the two
conditions RA2 puts on firing it are RA2's, and they had gone
missing with the fork.

`grap_on` latched `ctf_hookstate` and the shared CTFHookThink() did the rest,
whose gate is `ctf_hook`: a cvar CTFInit registers "1" for every ruleset,
because Threewave's flag and tech paths are reached from shared code.  So under
arena the offhand hook was on unconditionally -- in an arena whose `arena.cfg`
says `grapple: 0` (which the shipped file does), during a countdown, and for an
observer.  `give_ammo` is what made it hard to see: it withholds the Grapple
ITEM correctly, so the weapon-slot grapple obeyed the setting and only the
offhand one did not.

The donor's two conditions are `allow_grapple` -- arena.cfg's `grapple:` key --
and FIGHT_ALIVE, which under arena is not the same question as "not an
observer": FIGHT_DEAD is a third state and a corpse must not grapple.

One thing is deliberately not the donor's.  RA2 refires while the button is held
and the hook is not out; this keeps the FIRED bit, so a press is one
grapple, which is what `hookon` already means everywhere else in this tree.
==================
*/
void RA_HookThink(edict_t *ent)
{
    bool allowed;

    if (!ent->client)
        return;

    allowed = allow_grapple && ent->client->resp.fightstate == FIGHT_ALIVE;

    if (allowed && (ent->client->ctf_hookstate & CTF_HOOK_STATE_ON)) {
        CTFHook_Fire(ent);
        return;
    }

    // Letting go, and it releases only what the LATCH owns.  RA2 also gives the
    // Grapple ITEM when `grapple:` is on, so a player can fire one from the
    // weapon slot with ATTACK -- that hook is CTFWeapon_Grapple's to release,
    // and tearing it down from here would reset it on the frame after it fired
    // and make the weapon unusable.  A pending TURNOFF is `grap_off` saying the
    // offhand one is finished; `!allowed` overrides both, because a player who
    // may not have a grapple at all may not keep one either.
    //
    // Here rather than in CTFGrapplePull's own TURNOFF handshake because that
    // one runs only while a hook is out, so it cannot answer "the round ended
    // under you" or "`grapple` was voted off".
    if (!allowed || (ent->client->ctf_hookstate & CTF_HOOK_STATE_TURNOFF)) {
        ent->client->ctf_hookstate = 0;
        if (ent->client->ctf_grapple)
            CTFResetGrapple(ent->client->ctf_grapple);
    }
}

// ---------------------------------------------------------------- hud
//
// The two things RA2 writes into the shared HUD, and the table one of them
// reads.  They are here rather than in p_hud.c so that the arena-only state --
// the team skin table, the pickup queues -- stays on this side of the seam and
// p_hud.c asks by name.

/*
==================
RA_Precache

RA2's own worldspawn precache, and the whole of it is the team-skin icon table
that RA_SkinIcon() matches against.

The table was never filled.  The four arrays are declared at the top of this
file, read below, and were written nowhere -- RA2 fills them in SP_worldspawn
and the merge dropped the loop.  All four were therefore zero, `gi.imageindex`
never returns zero for a non-empty name, the match below could not succeed, and
RA_SkinIcon was an unconditional `return level.pic_health`.

Filling it also does what its name says.  These are the only images the mod
needs that no entity in the map asks for, so without this the first player to
wear a team skin registered its icon configstring mid-round.
==================
*/
void RA_Precache(void)
{
    int i;

    for (i = 0; i < MAX_ARENA_SKINS; i++) {
        teamskins_precachem[i]  = gi.imageindex(va("male/%s_i", teamskins[i]));
        teamskins_precachef[i]  = gi.imageindex(va("female/%s_i", teamskins[i]));
        teamskins_precachecw[i] = gi.imageindex(va("crakhor/%s_i", teamskins[i]));
        teamskins_precachecb[i] = gi.imageindex(va("cyborg/%s_i", teamskins[i]));
    }
}

/*
==================
RA_SkinIcon

RA2 draws the player's team skin where baseq2 draws the health icon: same slot,
same `pic 0` in the bar, different writer -- so this is a value, not a second
slot.  Falls back to the health icon for a skin that is not one of the seven
team skins.
==================
*/
int RA_SkinIcon(edict_t *ent)
{
    char    skinicon[MAX_QPATH];
    int     image, i;

    Q_snprintf(skinicon, sizeof(skinicon), "%s_i",
               Info_ValueForKey(ent->client->pers.userinfo, "skin"));
    image = gi.imageindex(skinicon);

    for (i = 0; i < MAX_ARENA_SKINS; i++) {
        if (teamskins_precachem[i] == image || teamskins_precachef[i] == image ||
            teamskins_precachecw[i] == image || teamskins_precachecb[i] == image)
            return image;
    }

    return level.pic_health;
}

/*
==================
RA_SetQueueStats

The pickup-team queue panel: how many are waiting on each side, and the two
configstrings naming them.  A client with no arena gets the panel switched off
rather than left holding the last arena's numbers.
==================
*/
void RA_SetQueueStats(edict_t *ent)
{
    int ctx = ent->client->resp.context;

    if (!ctx) {
        G_SetStat(ent, SID_RA_COUNTDOWN, 0);
        G_SetStat(ent, SID_RA_ARENASTATUS, 0);
        G_SetStat(ent, SID_RA_SHOWQUEUE, 0);
        return;
    }

    if (!arenas[ctx].idarena) {
        G_SetStat(ent, SID_RA_SHOWQUEUE, 0);
        return;
    }

    // Mid-round the queue is what is left to play; between rounds it is
    // everyone signed up.
    if (arenas[ctx].state == ASTATE_FIGHTING ||
        arenas[ctx].state == ASTATE_RESULTS ||
        arenas[ctx].state == ASTATE_NEXTROUND) {
        G_SetStat(ent, SID_RA_QUEUE1,
                  count_players_queue(arenas[ctx].pickupteam[0]->arenalink.it));
        G_SetStat(ent, SID_RA_QUEUE2,
                  count_players_queue(arenas[ctx].pickupteam[1]->arenalink.it));
    } else {
        G_SetStat(ent, SID_RA_QUEUE1,
                  count_queue(arenas[ctx].pickupteam[0]->arenalink.it));
        G_SetStat(ent, SID_RA_QUEUE2,
                  count_queue(arenas[ctx].pickupteam[1]->arenalink.it));
    }

    G_SetStat(ent, SID_RA_QUEUE1_ICON, game.csr.items + game.num_items + 2);
    G_SetStat(ent, SID_RA_QUEUE2_ICON, game.csr.items + game.num_items + 3);
    G_SetStat(ent, SID_RA_SHOWQUEUE, 1);
}

// ---------------------------------------------------------------- scoreboard
//
// RA2's three scoreboard layouts and the chooser that picks between them.  They
// live here rather than in p_hud.c for the reason Threewave's CTFScoreboard
// does: a ruleset's scoreboard is that ruleset's, and the dispatch already has
// a ScoreboardMessage row for exactly this.  RA2 wrote them in its own p_hud.c,
// where they would have had to be gated three times over.

/*
==================
Serverwide_ScoreboardMessage

==================
*/
void Serverwide_ScoreboardMessage(edict_t *ent)
{
    char    entry[1024];
    char    string[1400];
    int     stringlength;
    int     i, j, k;
    int     sorted[MAX_CLIENTS];
    int     sortedscores[MAX_CLIENTS];
    int     score, total;
    int     y;
    char    teamname[100];
    char    line[1024];
    gclient_t   *cl;
    edict_t     *cl_ent;
    team_t      *t;

    total = 0;
    for (i = 0; i < game.maxclients; i++) {
        cl_ent = g_edicts + 1 + i;
        if (!cl_ent->inuse)
            continue;

        score = game.clients[i].resp.score;

        for (j = 0; j < total; j++)
            if (score > sortedscores[j])
                break;

        for (k = total; k > j; k--) {
            sorted[k] = sorted[k - 1];
            sortedscores[k] = sortedscores[k - 1];
        }

        sorted[j] = i;
        sortedscores[j] = score;
        total++;
    }

    string[0] = 0;
    stringlength = strlen(string);

    Q_snprintf(entry, sizeof(entry),
               "xv 0 yv 32 string2 \"Frags Ping   Name        Team       A\" xv 0 yv 40 string2 \""
               "\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b"
               "\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\x9b\" ");
    j = strlen(entry);
    if (stringlength + j < 1024) {
        memcpy(string + stringlength, entry, j + 1);
        stringlength += j;
    }

    if (total > 23)
        total = 23;

    for (i = 0; i < total; i++) {
        cl = &game.clients[sorted[i]];
        cl_ent = g_edicts + 1 + sorted[i];

        // "None" covers both halves of RA_TeamOf's question, and the second
        // half is the one this board died on -- see arena_init.
        t = RA_TeamOf(cl_ent);
        Q_strlcpy(teamname, t ? t->name : "None", sizeof(teamname));

        Q_snprintf(line, sizeof(line), "%3i %4i %12.12s %12.12s %1i",
                   cl->resp.score, cl->ping, cl->pers.netname, teamname, cl->resp.context);

        if (cl_ent == ent)
            HiPrint(line);

        y = i * 8 + 48;

        Q_snprintf(entry, sizeof(entry), "xv 8 yv %i string2 \"%s\"", y, line);

        j = strlen(entry);
        if (stringlength + j > 1024)
            break;
        memcpy(string + stringlength, entry, j + 1);
        stringlength += j;
    }

    gi.WriteByte(svc_layout);
    gi.WriteString(string);
}

/*
==================
Arena_ScoreboardMessage

==================
*/
void Arena_ScoreboardMessage(edict_t *ent)
{
    char    entry[1024];
    char    string[1400];
    int     sortedteams[MAX_TEAMS];
    int     teamscores[MAX_TEAMS];
    int     sortedplayers[MAX_CLIENTS];
    int     playerscores[MAX_CLIENTS];
    int     teampings[MAX_TEAMS];
    char    line[1024];
    int     stringlength;
    int     i, j, k;
    int     score;
    int     total;
    edict_t *cl_ent;
    int     totalplayers;
    int     arenanum;
    int     ping;
    int     row;
    int     n;
    qmenu_t *node;
    team_t  *t;
    gclient_t   *cl;

    arenanum = ent->client->resp.context;

    for (total = 0, i = 0; i < MAX_TEAMS; i++) {
        if (!teams[i].it)
            continue;
        if (((team_t *)teams[i].it)->arenanum != arenanum)
            continue;
        if (((team_t *)teams[i].it)->outofline)
            continue;

        node = &teams[i];
        score = 0;
        k = 0;
        ping = 0;
        while (node->next) {
            node = node->next;
            score += ((edict_t *)node->it)->client->resp.score;
            ping += ((edict_t *)node->it)->client->ping;
            k++;
        }

        if (!k)
            continue;

        ping /= k;

        for (j = 0; j < total; j++)
            if (score > teamscores[j])
                break;

        for (k = total; k > j; k--) {
            sortedteams[k] = sortedteams[k - 1];
            teamscores[k] = teamscores[k - 1];
            teampings[k] = teampings[k - 1];
        }

        sortedteams[j] = i;
        teamscores[j] = score;
        teampings[j] = ping;
        total++;
    }

    string[0] = 0;
    stringlength = strlen(string);

    Q_snprintf(entry, sizeof(entry), "xv 0 yv 40 string2 \"Teams\" xv 160 string2 \"Players\" ");
    j = strlen(entry);
    memcpy(string + stringlength, entry, j + 1);
    stringlength += j;

    row = 1;
    total = total > 20 ? 20 : total;

    for (n = 0; n < total; n++) {
        t = teams[sortedteams[n]].it;

        Q_snprintf(line, sizeof(line), "%-2d %-3d %.11s", teamscores[n], teampings[n], t->name);
        if (t->fighting)
            HiPrint(line);

        Q_snprintf(entry, sizeof(entry), "xv 0 yv %d string2 \"%s\" ", row * 8 + 40, line);
        j = strlen(entry);
        if (stringlength + j > 1024)
            break;
        memcpy(string + stringlength, entry, j + 1);
        stringlength += j;

        totalplayers = 0;
        node = t->arenalink.it;

        while (node->next) {
            node = node->next;
            cl_ent = node->it;
            score = cl_ent->client->resp.score;

            for (j = 0; j < totalplayers; j++)
                if (score > playerscores[j])
                    break;

            for (k = totalplayers; k > j; k--) {
                sortedplayers[k] = sortedplayers[k - 1];
                playerscores[k] = playerscores[k - 1];
            }

            sortedplayers[j] = cl_ent - g_edicts - 1;
            playerscores[j] = score;
            totalplayers++;
        }

        totalplayers = totalplayers > 20 ? 20 : totalplayers;

        for (i = 0; i < totalplayers; i++) {
            cl_ent = g_edicts + 1 + sortedplayers[i];
            cl = &game.clients[sortedplayers[i]];

            Q_snprintf(line, sizeof(line), "%-2d %-3d %.11s",
                       cl->resp.score, cl->ping, cl->pers.netname);
            if (cl_ent->takedamage)
                HiPrint(line);

            Q_snprintf(entry, sizeof(entry), "xv 160 yv %d string2 \"%s\" ", row * 8 + 40, line);
            j = strlen(entry);
            if (stringlength + j > 1024)
                break;
            memcpy(string + stringlength, entry, j + 1);
            stringlength += j;
            row++;
        }
    }

    gi.WriteByte(svc_layout);
    gi.WriteString(string);
}

/*
==================
Pickup_ScoreboardMessage

==================
*/
void Pickup_ScoreboardMessage(edict_t *ent)
{
    char    entry[1024];
    char    string[1400];
    int     redsorted[MAX_CLIENTS];
    int     redscores[MAX_CLIENTS];
    int     bluesorted[MAX_CLIENTS];
    int     bluescores[MAX_CLIENTS];
    char    line[1024];
    int     stringlength;
    int     i, j, k;
    int     score;
    int     redtotal;
    edict_t     *cl_ent;
    int     bluewins;
    int     redwins;
    int     bluetotal;
    gclient_t   *cl;
    team_t      *t;

    bluewins = 0;
    redwins = 0;
    redtotal = 0;

    for (i = 0; i < game.maxclients; i++) {
        cl_ent = &g_edicts[i + 1];
        if (!cl_ent->inuse)
            continue;
        if (cl_ent->client->resp.context != ent->client->resp.context)
            continue;
        // A client in a pickup arena is on one of its two teams by
        // construction -- AddtoArena refuses the arena to anyone who is not --
        // so RA2 indexed `teams[]` here with no test at all, and `resp.teamnum`
        // is -1 for a client on no team: teams[-1] read as a team_t *, and
        // dereferenced.  RA_TeamOf answers both halves.
        t = RA_TeamOf(cl_ent);
        if (!t || t->side)
            continue;

        score = game.clients[i].resp.score;

        for (j = 0; j < redtotal; j++)
            if (score > redscores[j])
                break;

        for (k = redtotal; k > j; k--) {
            redsorted[k] = redsorted[k - 1];
            redscores[k] = redscores[k - 1];
        }

        redsorted[j] = i;
        redscores[j] = score;
        redwins = t->wins;
        redtotal++;
    }

    bluetotal = 0;

    for (i = 0; i < game.maxclients; i++) {
        cl_ent = &g_edicts[i + 1];
        if (!cl_ent->inuse)
            continue;
        if (cl_ent->client->resp.context != ent->client->resp.context)
            continue;
        t = RA_TeamOf(cl_ent);
        if (!t || t->side != 1)
            continue;

        score = game.clients[i].resp.score;

        for (j = 0; j < bluetotal; j++)
            if (score > bluescores[j])
                break;

        for (k = bluetotal; k > j; k--) {
            bluesorted[k] = bluesorted[k - 1];
            bluescores[k] = bluescores[k - 1];
        }

        bluesorted[j] = i;
        bluescores[j] = score;
        bluewins = t->wins;
        bluetotal++;
    }

    string[0] = 0;
    stringlength = strlen(string);

    if (redwins < 0)
        redwins = 0;
    if (bluewins < 0)
        bluewins = 0;

    Q_snprintf(entry, sizeof(entry),
               "xv 0 yv 40 string2 \"Team Red  : %d\" xv 160 yv 40 string2 \"Team Blue : %d\" ",
               redwins, bluewins);
    j = strlen(entry);
    memcpy(string + stringlength, entry, j + 1);
    stringlength += j;

    redtotal = redtotal > 20 ? 20 : redtotal;
    bluetotal = bluetotal > 20 ? 20 : bluetotal;

    for (i = 0; i < redtotal || i < bluetotal; i++) {
        if (i < redtotal) {
            cl_ent = g_edicts + 1 + redsorted[i];
            cl = &game.clients[redsorted[i]];

            Q_strlcpy(line, cl->pers.netname, sizeof(line));
            if (!cl_ent->takedamage)
                LoPrint(line);
            else
                HiPrint(line);

            Q_snprintf(entry, sizeof(entry),
                       "xv 0 yv %d string2 \"%2d %3d %.12s\" ", i * 8 + 48, cl->resp.score,
                       cl->ping, line);

            j = strlen(entry);
            if (stringlength + j > 1024)
                break;
            memcpy(string + stringlength, entry, j + 1);
            stringlength += j;
        }

        if (i < bluetotal) {
            cl_ent = g_edicts + 1 + bluesorted[i];
            cl = &game.clients[bluesorted[i]];

            Q_strlcpy(line, cl->pers.netname, sizeof(line));
            if (!cl_ent->takedamage)
                LoPrint(line);
            else
                HiPrint(line);

            Q_snprintf(entry, sizeof(entry),
                       "xv 160 yv %d string2 \"%2d %3d %.12s\" ", i * 8 + 48, cl->resp.score,
                       cl->ping, line);

            j = strlen(entry);
            if (stringlength + j > 1024)
                break;
            memcpy(string + stringlength, entry, j + 1);
            stringlength += j;
        }

    }

    gi.WriteByte(svc_layout);
    gi.WriteString(string);
}

/*
==================
RA_ScoreboardMessage

The arena ruleset's ScoreboardMessage row.  `scoremode` is RA2's three-state
replacement for baseq2's `showscores` bool -- 0 off, 1 the arena board, 2 the
server-wide one -- and a client with no arena has only the server-wide one to
show.
==================
*/
void RA_ScoreboardMessage(edict_t *ent, edict_t *killer)
{
    if (!ent->client->resp.context && ent->client->scoremode == 1)
        ent->client->scoremode = 2;

    if (ent->client->scoremode == 2) {
        Serverwide_ScoreboardMessage(ent);
        return;
    }

    if (arenas[ent->client->resp.context].idarena)
        Pickup_ScoreboardMessage(ent);
    else
        Arena_ScoreboardMessage(ent);
}

/*
=================
RA_EndLevel

RA2 has its own map rotation (`maploop.c`) and baseq2 has `sv_maplist` inside
EndDMLevel; running both is the double-rotation bug, which the port that put
get_next_map() into EndDMLevel had.  Here it is the arena ruleset's EndLevel
row, so only one rotation can run, and which one is a property of the ruleset
rather than of call order.

**DF_SAME_LEVEL OUTRANKS THE LOOP, and the split is why that has to be said
here.**  The donor asks the dmflag FIRST and returns on it: `port_ra2:g_main.c`'s
`EndDMLevel` is `if (dmflags & DF_SAME_LEVEL) { same map; return; }` and only
then `n = get_next_map(...)`.  Those two tests live in one function there and in
two here, and this is the outer one, so the order between them is this row's to
keep.  It keeps it by asking the flag before the loop rather than by answering
it: with the bit set the loop is skipped and `EndDMLevel()` gives the donor's
arm, which is the same arm baseq2 would have given, and the answer stays in one
place.  R-RA-12.
=================
*/
void RA_EndLevel(void)
{
    char *next;

    // The dmflag first, which is RA2's order.  Skipped rather than answered
    // here: EndDMLevel() below opens with the same test and the same map, so
    // spelling it twice would be two places to get it wrong.
    if (!((int)dmflags->value & DF_SAME_LEVEL)) {
        next = get_next_map(level.mapname);

        if (next) {
            G_BeginIntermission(CreateTargetChangeLevel(next));
            return;
        }
    }

    // maploop had nothing to say, or was outranked -- fall back to baseq2's
    // rotation, which is where DF_SAME_LEVEL is answered.
    EndDMLevel();
}

/*
=================
RA_CheckRules

The seven-state round machine is the arena ruleset's match rule, so it is
reached through the dispatch rather than bolted onto G_RunFrame beside
CheckDMRules -- which is where RA2's own port called it, leaving two match-rule
systems with no statement of how they compose.

They do compose: `arena_think` runs a round inside one arena, `CheckDMRules`
ends the level.  `timelimit` and `fraglimit` keep working at level scope as they
do in RA2; what changes is that both now happen on one row.  The order between
them is RA2's and is kept -- see below.

`multi_arena_think` walks one arena per frame -- `level.framenum % (num_arenas *
2)` -- so a server with 32 arenas costs one arena's think per frame rather than
32.  `num_arenas` is at least 1 after arena_init(), which is what makes that
modulo safe; the guard says so rather than trusting it, because a divide by zero
in the frame loop kills the server.
=================
*/
void RA_CheckRules(void)
{
    // Level scope first, which is RA2's own order -- CheckDMRules() ahead of
    // multi_arena_think().  It matters on the one frame where `timelimit` or
    // `fraglimit` trips, because BeginIntermission sets
    // level.intermission_framenum and multi_arena_think returns on it.  With
    // the arena thinking first, that last tick could centerprint a round win
    // or restart a countdown on the frame the map ends, and a tick landing on
    // ASTATE_NEXTROUND wrote a round record that RA2_Stats_End then
    // duplicated.
    CheckDMRules();

    if (num_arenas > 0) {
        multi_arena_think();
        // Eviction first: a bot standing in an arena that has switched bots off
        // is not a candidate for following the people anywhere, and both sweeps
        // end in the same two calls, so this avoids undoing a move just made.
        RA_BotsVotedOut();
        RA_BotFollowPeople();
    }
}

const ruleset_ops_t ops_arena = {
    .name     = "arena",
    .CheckRules        = RA_CheckRules,
    .EndLevel          = RA_EndLevel,
    .ScoreboardMessage = RA_ScoreboardMessage,
    // BeginIntermission, SelectSpawnPoint and ClientPlaced are still dm's:
    // arena does its own placement inside init_player()/reinit_player(), which
    // PutClientInServer calls before the shared spawn path runs, so there is
    // nothing for those rows to do.  A NULL row inherits.
};

/*
=================
G_Svcmd_ArenaDump_f

`sv arenadump` -- one line per connected client under `arena`, naming the four
facts no other channel carries together: which arena it is in, which team it is
on and whether that team is fighting, the round-machine state of that arena, and
the three edict fields the round's placement writes -- `solid`, `takedamage` and
the origin it was placed at.

A client's own HUD reports its arena and the scoreboard reports its team, but
neither can say that two clients are at the same origin with `solid` SOLID_BBOX
on both -- a wedged telefrag -- or that a bot is on a team in an arena the
people are not in.
=================
*/
void G_Svcmd_ArenaDump_f(void)
{
    static const char *states[] = {
        "warmup", "countdown", "fighting", "roundend",
        "intermission", "results", "nextround"
    };
    int i, j, fill;

    fill = RA_BotFillArena();

    gi.cprintf(NULL, PRINT_HIGH, "arenas       %d, idmap %d, botfill %d",
               num_arenas, idmap, (int)gi.cvar("botfill", "0", 0)->value);
    if (fill)
        gi.cprintf(NULL, PRINT_HIGH, " -> arena %d", fill);
    gi.cprintf(NULL, PRINT_HIGH, "\n");

    for (i = 1; i <= num_arenas; i++) {
        // `want` and `here` are printed for every arena and not only the one the
        // fill is aimed at, so a reader can see that all of them differ.
        gi.cprintf(NULL, PRINT_HIGH,
                   "arena %-2d     %-12s pickup=%d round=%d/%d "
                   "waiting=%d active=%d sidepick=%d ppt=%d "
                   "spawns=%d want=%d here=%d aprot=%d hprot=%d\n",
                   i, states[arenas[i].state], arenas[i].idarena,
                   arenas[i].round, arenas[i].rounds,
                   count_queue(&arenas[i].waitingteams),
                   count_queue(&arenas[i].activeteams),
                   arenas[i].sidepick, arenas[i].playersperteam,
                   ArenaSpawnCount("info_player_deathmatch", i),
                   RA_BotFillTarget(i), RA_ArenaPlayers(i, NULL),
                   // The friendly-fire pair, which nothing else reports: they
                   // are per-arena settings out of arena.cfg and the only other
                   // way to see the value in force is to be hit by something.
                   arenas[i].armorprotect, arenas[i].healthprotect);

        // The loadout, which nothing else reports: pack weapons that no arena
        // granted, and Teslas and Traps handed out by servers running neither
        // pack, are both invisible to `sv` without this line.
        //
        // `mask` is what arena.cfg and the settings menu store; `grants` is what
        // give_ammo() honours, which differs by every pack weapon whose content
        // layer is off (the bit is carried, not cleared -- see give_ammo).
        gi.cprintf(NULL, PRINT_HIGH,
                   "  loadout    mask=%#06x grants=%#06x pack:", arenas[i].weapons,
                   RA_ArenaGrantsMask(i));
        for (j = 0; j < RA_NUM_PACK_WEAPONS; j++)
            if (RA_ArenaGrantsMask(i) & weapon_vals[9 + j])
                gi.cprintf(NULL, PRINT_HIGH, " %s", ra_pack_weapons[j].cfgname);
        if (!(RA_ArenaGrantsMask(i) & RA_PACK_WEAPON_MASK))
            gi.cprintf(NULL, PRINT_HIGH, " none");
        gi.cprintf(NULL, PRINT_HIGH,
                   " ammo: magslug=%d trap=%d flechettes=%d prox=%d tesla=%d\n",
                   G_LayerEnabled(LAYER_XATRIX) ? arenas[i].magslug : 0,
                   G_LayerEnabled(LAYER_XATRIX) ? arenas[i].trap : 0,
                   G_LayerEnabled(LAYER_ROGUE) ? arenas[i].flechettes : 0,
                   G_LayerEnabled(LAYER_ROGUE) ? arenas[i].prox : 0,
                   G_LayerEnabled(LAYER_ROGUE) ? arenas[i].tesla : 0);
    }

    for (i = 0; i < MAX_TEAMS; i++) {
        if (!teams[i].it)
            continue;
        gi.cprintf(NULL, PRINT_HIGH,
                   "team %-3d     arena=%d side=%d members=%d fighting=%d "
                   "wins=%d locked=%d \"%s\"\n",
                   i, TEAM(&teams[i])->arenanum, TEAM(&teams[i])->side,
                   count_queue(&teams[i]), TEAM(&teams[i])->fighting,
                   TEAM(&teams[i])->wins, TEAM(&teams[i])->locked,
                   TEAM(&teams[i])->name);
    }

    for (i = 0; i < game.maxclients; i++) {
        edict_t *e = &g_edicts[i + 1];

        if (!e->inuse || !e->client)
            continue;

        gi.cprintf(NULL, PRINT_HIGH,
                   "client %-2d    %-14s %s arena=%d team=%d fight=%d "
                   "solid=%d dmg=%d dead=%d hp=%d svf=%x recheck=%d "
                   "at (%.0f %.0f %.0f)\n",
                   i, e->client->pers.netname,
                   (e->flags & FL_BOT) ? "bot  " : "human",
                   e->client->resp.context, e->client->resp.teamnum,
                   e->client->resp.fightstate, e->solid, e->takedamage,
                   e->deadflag, e->health, e->svflags,
                   e->client->resp.spawn_recheck,
                   e->s.origin[0], e->s.origin[1], e->s.origin[2]);
    }

    // Two clients whose bounding boxes overlap is what a wedged telefrag leaves
    // behind, so name it rather than leaving it to be read off the coordinates.
    for (i = 0; i < game.maxclients; i++) {
        edict_t *a = &g_edicts[i + 1];

        if (!a->inuse || !a->client || a->solid == SOLID_NOT)
            continue;

        for (j = i + 1; j < game.maxclients; j++) {
            edict_t *b = &g_edicts[j + 1];

            if (!b->inuse || !b->client || b->solid == SOLID_NOT)
                continue;
            if (fabsf(a->s.origin[0] - b->s.origin[0]) >= 32 ||
                fabsf(a->s.origin[1] - b->s.origin[1]) >= 32 ||
                fabsf(a->s.origin[2] - b->s.origin[2]) >= 56)
                continue;

            gi.cprintf(NULL, PRINT_HIGH,
                       "  !! %s and %s are both solid and overlapping\n",
                       a->client->pers.netname, b->client->pers.netname);
        }
    }
}
