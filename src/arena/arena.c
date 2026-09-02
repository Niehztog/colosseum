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
// Rocket Arena 2 v2.25, from rocketarena2-public@d20e1ce (doc/provenance.md).
// Donor-only: baseq2 has no counterpart, so it lives in src/arena/ rather than
// being merged into a spine file (R-CORE-7).  The reconstruction's asm-matching
// address comments are stripped -- SPECS.md N1 makes those oracles meaningless
// here, and they survive at the pin.
#include "g_local.h"
#include "arena/arena.h"
#include "ctf/g_ctf.h"
#include "arena/ra2stats.h"
// R-RA-7's switch is the bot layer's now: one `botfill` for every ruleset, so
// the answer to "is the fill on" comes from where the fill lives.
#include "bot/bl_main.h"
// R-RA-8: BotDestroy(), for the bot an arena has voted out that nowhere else
// will take.  osp_cmds.c reaches the bot layer from a ruleset file the same way.
#include "bot/bl_spawn.h"

// R-OSP-5: no local `extern` of a donor object.  arena.h already declares
// `votetries_setting`, and a second declaration in a .c file is the shape that
// let the donor's `extern int botglobals;` resolve a four-byte int over the
// first member of a struct.  Found by donorgate.py's stray-extern pass.
bool    allow_grapple;
bool    broken = false;

arena_t     arenas[MAX_ARENAS];
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
feet away, and opens fire -- which is R-140 and what this answers.

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

Should this client's ATTACK be taken away right now?  R-149's open half: a
PERSON standing in a countdown, who could empty a magazine into an opponent that
cannot be hurt -- spending ammo `give_ammo` hands out once a round.

*The Gladiator donor's own answer cannot be taken verbatim, and saying why is
the whole design.*  `gladq2_src/p_client.c` guards both `Think_Weapon` call
sites with `!(ra->value && ent->takedamage == DAMAGE_NO)`, and one of those two
is `ClientBeginServerFrame`'s, which runs EVERY frame and is the weapon state
machine's heartbeat: `Use_Weapon` sets `newweapon`, `Weapon_Generic` walks
WEAPON_DROPPING down to `ChangeWeapon`, and RA2's `fastswitch` only skips the
raise animation rather than moving the switch off that path.  So the donor's
clause freezes weapon SELECTION for the length of the countdown -- and choosing
a weapon during the countdown is not incidental to Rocket Arena, it is how a
round is prepared.

*Corrected in R-195.1: ROCKET ARENA HAS A GATE TOO, and it is this one's
design rather than the Gladiator SDK's.*  What stood here -- that RA2 gates
neither site, so a person can pre-fire a countdown in 1999's mod as well -- is
true of its two `Think_Weapon` calls and false of the mod: RA2 adds
`ent->takedamage && arenas[...].state == ASTATE_FIGHTING` inside four
`p_weapon.c` fire arms (`Weapon_Generic` 417, `Weapon_HyperBlaster_Fire` 818,
`Machinegun_Fire` 897, `Chaingun_Fire` 997), at the pin `d20e1ce` as well as
at the bundle tip.  So the donor gates the FIRING, which is what this gates,
one level up.

Taking its four verbatim would have been a REGRESSION here: this tree has NINE
sites where a press becomes a shot, RA2's four cover four of them, and the
five it leaves -- `Throw_Generic` (the hand grenade and the tesla), the Trap,
the chainfist, the ETF rifle and the plasma beam -- are every one reachable
under `arena`, three by an R-182 grant bit and two by the ammunition that is
also the weapon.  The one thing the donor does that this does not is QUEUE the
press: its clear of `latched_buttons` sits inside the gated arm, so a tap
during a countdown fires on the bell.  R-195.1 records why that is not
followed.

So the think keeps running and the BUTTON is what goes, which is exactly what
R-140 does on the other side of the seam for bots.  Every weaponthink in the
merged tree -- baseq2's, the mission packs' and Threewave's -- reads
`client->buttons` or `client->latched_buttons`, so clearing the bit at the latch
reaches all of them without a per-weapon gate.

Two exemptions, and both are somebody else's key.  An OBSERVER's ATTACK is what
cycles RA2's four camera modes (R-RA-4); R-143 is what already stops that press
reaching a weapon, and taking the button away here would take the camera with
it.  And the GRAPPLE is fired with the same button and is movement rather than
damage, so a player holding one keeps it -- otherwise `allow_grapple` would lose
its hook for the whole countdown.  R-140 makes the same exemption, for the same
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

One of R-182's five mission-pack ammunition grants, gated on the layer it
belongs to.

WHY THE GATE, when the three ordinary ammunitions could have done without one.
`ammo_tesla` and `ammo_trap` are the WEAPON as well as the ammunition -- the
donor treats `ammo_grenades` the same way, which is why neither has a `weapons:`
bit -- so a count of them is not a number on a HUD, it is an item in the weapon
cycle.  Ungated, every arena on every server handed out fifty Teslas (Ground
Zero) and five Traps (Reckoning), including a server running NEITHER pack, and
`weapnext` walked through both.  Measured in a play test on `xatrix 1`, on
`rogue 1` and with both off; all three gave both.

ZERO RATHER THAN SKIP.  The inventory is carried across a round -- give_ammo is
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

ONE function because the difference between the stored mask and the honoured one
is a fact two callers need -- give_ammo(), which must not grant a Ground Zero
weapon on a server with no Ground Zero, and `sv arenadump`, whose whole job is to
report what an arena grants.  A diagnostic that recomputed it could be right
about a rule the game no longer follows.
==================
*/
int RA_ArenaGrantsMask(int arenanum)
{
    int i, mask;

    if (arenanum < 0 || arenanum >= MAX_ARENAS)
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
    //    0  2  3  4  5   6    9   8   7   -- and then R-182's six
    int         weapon_vals_x[RA_NUM_WEAPON_BITS] = {
        256, 1, 2, 4, 8, 16, 128, 64, 32,
        // R-182's six, and NOT in bit order: the index is the preference and
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
    // R-182's six, in ASCENDING PREFERENCE -- see weapon_vals_x above.
    w[9]  = FindItemByClassname("weapon_chainfist");
    w[10] = FindItemByClassname("weapon_boomer");
    w[11] = FindItemByClassname("weapon_proxlauncher");
    w[12] = FindItemByClassname("weapon_etf_rifle");
    w[13] = FindItemByClassname("weapon_phalanx");
    w[14] = FindItemByClassname("weapon_plasmabeam");

    needswitch = false;

    allowed = RA_ArenaGrantsMask(e->client->resp.context);

    // TWO PASSES, AND THE ORDER IS THE POINT.  `rl` is the weapon a player
    // spawns holding and it is the FIRST enabled one this loop meets, so the
    // donor's descending 8..0 encodes a preference: rocket launcher, then
    // hyperblaster, railgun, grenade launcher, chaingun, machinegun, super
    // shotgun, shotgun, BFG.  R-182's six are walked afterwards rather than
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
    // R-182's five, each behind its own layer -- see give_pack_ammo above.
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

    // R-SEC-2's kept-bug list, entry 4: the donor has no `i == MAX_TEAMS`
    // guard after this scan, so a full teams[] falls out with i == MAX_TEAMS
    // and `teams[i].it = t` twelve lines down writes one past the end of a
    // 256-entry array.  Unreachable while every team is created by a human
    // typing a name; Phase 7 creates one per bot.
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
// outlive the call and go away with the level.
//
// Factored out of menuNewTeam, which is also the only reason its bound is now
// checked.  The donor uniquifies by appending "!" and restarting the scan, into
// a 100-byte buffer, with no test that anything still fits: 256 teams named
// alike append 256 characters.  Unreachable while a human has to type each
// collision in by hand; Phase 7 makes team creation automatic and per bot, so
// it stops being unreachable.  A counted suffix cannot grow without bound.
char *RA_NewTeamName(edict_t *ent)
{
    char *name = gi.TagMalloc(100, TAG_LEVEL);
    int  suffix, i;

    for (suffix = 0; suffix < MAX_TEAMS; suffix++) {
        if (suffix)
            Q_snprintf(name, 100, "%s's Team %d", ent->client->pers.netname, suffix + 1);
        else
            Q_snprintf(name, 100, "%s's Team", ent->client->pers.netname);

        for (i = 0; i < MAX_TEAMS; i++)
            if (teams[i].it && !strcmp(TEAM(&teams[i])->name, name))
                break;
        if (i == MAX_TEAMS)
            return name;
    }

    return name;
}

// R-ARENA-2 and R-RA-4's fifteenth row, "new bots initialised into the selected
// arena's queue".  Both of the steps a human takes through the menus -- pick a
// team, pick an arena -- are taken here instead, because a bot has no menu:
// without this a bot connects, lands in arena 0 as a noclip observer with the
// team list open, and stays there for the whole level.  That is what it did.
//
// The arena is the `arena` userinfo key BotAddDeathmatch wrote from the `arena`
// cvar.  BotCreate also copies it into resp.context, and that copy is DEAD --
// ClientBeginDeathmatch calls InitClientResp, which memsets resp before
// PutClientInServer ever reads it.  The userinfo is the only carrier that
// survives, which is the same thing R-CTF-4 found about `ctfteam`.
//
// Called after placement rather than during it, from ClientBeginDeathmatch,
// because that is where a human's menu click lands: the bot is already a placed
// observer in arena 0, and this moves it exactly once, through the donor's own
// SendTeamToArena.  Doing it inside init_player() would mean either placing
// twice or open-coding SendTeamToArena's tail minus the move (R-87's shape).
// R-131: WHICH ARENA A NEW BOT JOINS.
//
// 1999's answer is the `arena` cvar, default 1, clamped to 1..num_arenas --
// `gladq2_src/bl_spawn.c` copies it into the bot's userinfo and this tree does
// the same, faithfully.  It is right for a deathmatch map, which has exactly
// one arena, and useless on a real RA2 map: there a person is wherever they
// picked, the PICKUP arena is wherever `arena.cfg` says -- on `ra2map6` it is
// 8 -- and every bot goes to arena 1, so nobody meets anybody and no round
// starts anywhere.  Measured: four bots in arena 1, one person in arena 8.
//
// So **0 becomes "follow the people"**: the lowest-numbered arena with a human
// on a team, then the lowest-numbered pickup arena, then 1.  Zero is free to
// mean that because 1999 clamped it to 1 and it therefore meant nothing, and a
// server that sets 1..N still gets 1999's behaviour exactly.  The choice is made
// at join time rather than when the bot was added -- and, since 1.30, is re-asked
// while the bot is still somewhere it can be moved from, because "at join time"
// is still too early for the first bot `minimumplayers` adds.  See
// RA_BotFollowPeople() below.

// The lowest-numbered arena a PERSON is on a team in, or 0 if there is none.
// Split out of RA_AutoArena because RA_BotFollowPeople asks the same question
// and must be able to tell "arena 1, because somebody is in it" from "arena 1,
// because there was nobody to follow" -- which is the whole of R-131's second
// half.
//
// `botsonly` narrows it to the people BOTS MAY JOIN (R-RA-8), which is the
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
// ALREADY in, so that a bot playing with somebody is never taken away from
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
// Gated on `botfill` with the rest of R-RA-7, so that OFF means off: with a
// flat `minimumplayers` the four bots it adds to a 2v2 arena were four teams of
// one, two fighting and two queued, and that is what a server which asked for
// nothing keeps.
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

    // R-RA-8: an arena that has voted bots out is shut to them for the same
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

static int RA_AutoArena(void)
{
    int     i, human;

    // R-RA-8 qualifies every answer below with the arena's own `bots` switch,
    // and 0 -- "nowhere the bots should be" -- becomes a possible answer where
    // it never used to be.  Both callers handle it: RA_BotFillArena hands it to
    // CheckMinimumPlayers as "this ruleset declines", and RA_BotJoinArena
    // leaves the bot unseated, which RA_BotsVotedOut then collects.
    human = RA_HumanArena(true);
    if (human)
        return human;

    // FOLLOWING THE PEOPLE MEANS FOLLOWING THE PEOPLE, AND SOMETIMES THAT IS
    // NOWHERE.  If there are people on this map and every arena they are in has
    // voted bots out, the honest answer is 0 -- not "some other arena, then".
    //
    // The first draft of R-RA-8 fell through to the search below in this case,
    // and it was wrong in a way worth recording: the fill would pick an EMPTY
    // arena nobody had asked about and populate it to capacity, so voting the
    // bots out of the arena you are standing in moved eight of them next door
    // to play each other for the rest of the map.  `botfill` under `arena`
    // exists to give the PEOPLE opponents (R-131); an arena with nobody in it
    // has nobody to give them to.
    if (RA_HumanArena(false))
        return 0;

    // Nobody to follow at all.  This is a STAGING answer, not a destination:
    // `minimumplayers` adds its first bot 3.2 seconds into a level, while the
    // person who typed `map` is still loading, and RA_BotFollowPeople moves it
    // to them when they arrive.  A pickup arena is the one a lone bot can be
    // joined to without inventing a team, and it is where a person arriving
    // later will be offered a place opposite it.
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

void RA_BotJoinArena(edict_t *ent)
{
    int     arenanum, k;
    team_t  *t;

    if (!(ent->flags & FL_BOT))
        return;
    if (ent->client->resp.teamnum >= 0)     // already on a team: a level change
        return;
    if (num_arenas <= 0)
        return;

    arenanum = Q_atoi(Info_ValueForKey(ent->client->pers.userinfo, "arena"));
    if (arenanum < 1 || arenanum > num_arenas)
        arenanum = RA_AutoArena();

    // R-RA-8.  RA_AutoArena can now answer 0 -- nowhere takes bots -- and an
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
    // (R-ARENA-3: bots are noclip observers, never parked in a waiting room).
    if (arenas[arenanum].locked) {
        gi.dprintf("%s: arena %d is locked, staying in arena 0\n",
                   ent->client->pers.netname, arenanum);
        return;
    }

    // Pair up before inventing a team, where the arena asks for more than one
    // player a side.  This is menuAddtoTeam's shape and the pickup arm's three
    // lines above: the team is ALREADY in the arena's queue, so only the bot is
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

    t = add_to_team(ent, RA_NewTeamName(ent));
    if (!t)
        return;

    remove_from_queue(&t->arenalink, NULL);
    SendTeamToArena(&teams[t->teamnum], arenanum, true, true);
}

// R-131's SECOND HALF: FOLLOWING THE PEOPLE IS A STANDING RULE, NOT A ONE-OFF.
//
// 1.28 made `arena 0` mean "the lowest-numbered arena with a human on a team",
// and resolved it at the bot's join.  That is the right answer asked at the
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
//   * the bot did not have an arena chosen for it -- `arena` 1..N is 1999's
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
// disconnecting player takes (remove_from_team, R-107) -- otherwise two bots
// stranded on opposite sides of an empty arena would hold each other there for
// a nine-round match while a person waited alone.
//
// The four are a predicate of their own rather than four `continue`s in the
// loop, because RA_ArenaPlayers() asks the same question for a different reason
// and the two must not be able to disagree: a bot this returns true for is a
// bot that is ABOUT TO BE in `target`, and R-RA-7's fill has to count it as
// already there or it adds a replacement for a bot that has not moved yet.  The
// target-is-open test stays in the caller -- that is one question about the
// arena, not one per bot.
static bool RA_BotBoundFor(edict_t *e, int target)
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

static void RA_BotFollowPeople(void)
{
    edict_t *e;
    int     i, target;

    // The same cadence as CheckMinimumPlayers, which is what puts the bots
    // here: re-asking every frame would cost a client scan per frame to answer
    // "no" 31 times out of 32.
    if (level.framenum & 31)
        return;

    // R-RA-8: an arena that refuses bots is not a place to follow people TO.
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

// R-RA-8: THE BOTS THAT WERE ALREADY THERE WHEN THE VOTE PASSED.
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
// THEY ARE REMOVED, NOT MOVED SOMEWHERE ELSE, and R-RA-9 is the reason it can
// be that simple.  An earlier draft offered each one a seat in another arena
// first and deleted only the ones nobody would take, on the argument that "the
// vote was about our arena, not about this server".  That argument assumes
// there is an arena somewhere that is SHORT of bots -- and under the scheduler
// there is not: every populated arena that allows bots is already being walked
// to its own target, one bot a tick.  Pushing this arena's cast-offs into a
// game that is already the size it asked to be overfills it, and the surplus
// arm then removes the same bots a tick or two later, somewhere the people who
// voted cannot see it happening.  So the fill decides where bots are wanted,
// and this only decides that they are not wanted HERE.
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

        // THE BOT THAT WAS STILL BEING BUILT WHEN THE VOTE PASSED, which the
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

// *** "FARTHEST FROM ANY PLAYER" IS A MINIMUM, AND WHICH PLAYERS IT IS TAKEN
// OVER IS THREE SEPARATE DECISIONS. ***
//
// R-77 answered one of them in 1.18 -- an arena observer is alive, noclipping
// and parked on a spawn point, so counting it chooses spawns by where the
// audience stood -- and answered it inside `PlayersRangeFromSpot`, which the
// shared deathmatch selectors also use.  RA2 upstream reached the same finding
// later (`rocketarena2@6b8d058`) and answered all three at once.  The other two
// are still open here, and both are cheap:
//
//   * the scan covers every client on the SERVER, so a fight in another arena
//     skews the pick.  Distance makes that rare rather than impossible -- two
//     arenas can be neighbours in one BSP -- and "rare" is not a property worth
//     relying on;
//   * `SendTeamToArena` sets FIGHT_ALIVE before calling move_to_arena, so the
//     player being placed is ranged against the position it is standing in
//     right now, which is the observer spot it is about to leave.
//
// So the arena selectors ask their own question instead of the shared one.
// `PlayersRangeFromSpot` keeps R-77's arm because `PutClientInServer` still
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

// Distance from `spot` to the nearest client that counts: with `fighters_only`,
// the ones fighting in this arena; otherwise every live body on the server.
// False means it counted nobody, which is the caller's cue that this measure has
// nothing to say about this spot.
static bool ArenaRangeFromSpot(edict_t *spot, int arenanum, edict_t *ignore,
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

        VectorSubtract(spot->s.origin, player->s.origin, v);
        dist = VectorLength(v);

        if (!found || dist < *range) {
            *range = dist;
            found = true;
        }
    }

    return found;
}

// The distance to the nearest player FIGHTING IN THIS ARENA -- and where nobody
// is fighting here, to the nearest live body anywhere.
//
// R-146: THAT FALLBACK IS NOT OPTIONAL, and the note that used to stand here
// said the opposite -- that returning 0 with nobody to measure against was
// deliberate, because it drops the caller through to the random selector and so
// keeps a roomful of arriving observers off one spot.  It does the reverse.
// Zero scores every spot below SelectFarthestArenaSpawnPoint's floor of 50, so
// the caller falls through to SelectRandomArenaSpawnPoint, and random spots
// COLLIDE.  In the staging area that is not an edge case but the norm:
// everybody there is FIGHT_SPECTATING, so the fighter pass counts nobody every
// single time and every arrival is placed at random.
//
// Two arrivals on one spot is not cosmetic.  An observer in OMODE_NORMAL is
// SOLID_BBOX on MOVETYPE_WALK, and RA2's own separators both decline: KillBox
// returns early for a FIGHT_SPECTATING client and check_telefrag skips one, so
// once two of them are inside each other nothing in the mod ever pulls them
// apart -- which is R-135's "siamese twins" reached by the other road.  The
// spawn picker was the only thing keeping them out of each other, which is what
// the donor's PlayersRangeFromSpot did by measuring against every live client.
//
// Where there ARE fighters nothing changes, so R-139's answer to "spawns by
// where the fighters are, not the audience" is untouched, and so is R-77's
// observers-do-not-repel-each-other.  Taken from `rocketarena2@811af42`.
static float ArenaFightersRangeFromSpot(edict_t *spot, int arenanum, edict_t *ignore)
{
    float   range = 0;

    if (ArenaRangeFromSpot(spot, arenanum, ignore, true, &range))
        return range;

    ArenaRangeFromSpot(spot, arenanum, ignore, false, &range);

    return range;
}

// The n-th (0-based) spawn point of this arena, or NULL if there is no n-th.
//
// It replaces a do/while that walked the GLOBAL entity list and incremented its
// own index every time it stepped over a spot belonging to another arena.  That
// loop dereferences whatever G_Find returns without testing it, so an index it
// cannot satisfy walks off the end of the list and reads `spot->arena` through
// NULL.  `side == 1` on an arena with ONE spawn point asks for index 1 of one
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
// `idmap == false` clause is what makes a stock deathmatch map -- one arena, no
// `arena` key on anything -- count every spawn point it has for that arena.
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

// *** A PICKUP ARENA PUTS ITS TWO SIDES ON ALTERNATE SPAWN POINTS AND PICKS
// AMONG THEM AT RANDOM, WHICH IS A BIRTHDAY PROBLEM. ***
//
// `ra2map9` arena 2 has twelve spawn points, so a side draws from six; three
// bots on that side collide 44% of the time, and RA2's whole answer to a
// collision is KillBox -- which during a countdown cannot telefrag, because an
// arena fighter is `takedamage DAMAGE_NO` until ASTATE_FIGHTING, and whose
// push-apart pushes BOTH bodies along the SAME vector (see g_utils.c).  So two
// players who draw the same point stay standing inside each other, which is
// what a play test reported: "both enemy bots spawned at exactly the same spawn
// point, they did not telefrag or push away each other, their bodies overlapped
// like siamese twins".  Measured here at three deep -- three members of `#2
// Pickup Red` placed within 60 units of each other, two of them left SOLID_NOT
// with a spawn_recheck, which only KillBox's push branch sets.
//
// The fix is not to collide.  The preference is the donor's own and is written
// down in SelectFarthestArenaSpawnPoint: 50 units of clearance from the nearest
// live player is what that function calls a usable spot, and
// PlayersRangeFromSpot is where R-RA-4's "observers ignored for spawn points"
// already lives, so an audience standing on a point does not reserve it.  When
// every point on the side IS taken the first candidate is returned anyway,
// because that is RA2's stated fallback in the other selector -- "if there is a
// player just spawned on each and every start spot we have no choice to turn
// one into a telefrag meltdown".
edict_t *SelectRandomArenaSpawnPoint(char *classn, int arenanum, int side, edict_t *ignore)
{
    edict_t     *spot, *first = NULL;
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
        if (ArenaFightersRangeFromSpot(spot, arenanum, ignore) > 50)
            return spot;
    }

    return first;
}

edict_t *SelectFarthestArenaSpawnPoint(char *classn, int arenanum, edict_t *ignore)
{
    edict_t     *bestspot;
    float       bestdistance, bestplayerdistance;
    edict_t     *spot;

    spot = NULL;
    bestspot = NULL;
    bestdistance = 50;
    while ((spot = G_Find(spot, FOFS(classname), classn)) != NULL) {
        //gi.bprintf (PRINT_HIGH,"arena %d spot %d\n", arenanum, spot->arena);
        if (spot->arena != arenanum && idmap == false) continue;
        bestplayerdistance = ArenaFightersRangeFromSpot(spot, arenanum, ignore);

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
`botfill` -- THE BOT COUNT FOLLOWS THE ARENA, NOT THE SERVER

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

Counting spawn points would be WRONG for the first kind, and that is worth
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
static int RA_NeediestArena(int *gapout);    // R-RA-9, defined just below

int RA_BotFillArena(void)
{
    int n;

    if (!BotFillEnabled())
        return 0;
    if (num_arenas <= 0)
        return 0;

    // THE ARENA THIS COUNTS MUST BE THE ARENA THE BOTS ARE SENT TO, and there
    // are two answers to that, not one.  `bl_spawn.c` writes the `arena` cvar
    // into a new bot's userinfo and RA_BotJoinArena obeys it, so 1..N is where
    // the bots actually go -- and a fill that counted somewhere else would find
    // that arena empty at every tick and add until the roster or `maxclients`
    // ran out.  Only 0, the absence of a request, is resolved by following the
    // people, and through RA_AutoArena rather than a second copy of it.
    //
    // R-RA-8: AND IT MUST BE AN ARENA THAT STILL TAKES THEM.  `bots` is the
    // arena's own switch -- `arena.cfg`'s `bots` key, and the "Allow Bots" row
    // the people in it can vote on -- so an explicit `arena N` pointed at an
    // arena that has turned bots off resolves to "nowhere" rather than being
    // obeyed over the top of it.  0 is the answer CheckMinimumPlayers reads as
    // "this ruleset declines", which is why it may be returned here at all.
    // R-RA-9: THE DEFAULT IS 0, "no arena named".  It was "1", which this test
    // cannot tell apart from an operator who meant arena 1 -- so a server that
    // did not exec configs/arena.cfg pinned the whole fill to arena 1 and got
    // neither the scheduler below nor R-131's follow-the-people, while a server
    // that did exec it got both.  One of those two was the intended behaviour
    // and it was not the one you got by saying nothing.  All four readers of
    // this cvar default to "0" together: whichever runs first is the one that
    // creates it, so a single "1" left anywhere would decide for all of them.
    n = (int)gi.cvar("arena", "0", 0)->value;
    if (n >= 1 && n <= num_arenas)
        return arenas[n].bots ? n : 0;

    // R-RA-9: OTHERWISE, THE ARENA FURTHEST FROM ITS OWN TARGET.  See
    // RA_NeediestArena() -- one arena a tick, but not the SAME arena every
    // tick, which is what "botfill fills the arenas" has to mean on a map that
    // has more than one of them.
    n = RA_NeediestArena(NULL);
    if (n)
        return n;

    // Nobody is playing anywhere: staging, exactly as before.
    return RA_AutoArena();
}

// R-RA-9: WHICH ARENA THE FILL SHOULD WORK THIS TICK.
//
// R-RA-7 gave the fill one arena and one census, and asked RA_AutoArena for the
// number: "the arena the people are in".  On a one-arena map that is the whole
// truth, and RA2's own maps have up to 32.  With two crowded arenas the fill
// sized itself to the LOWEST-numbered one, filled it, and then sat at its
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
// AN ARENA WITH NOBODY IN IT WANTS NO BOTS, and that is the whole of "the bots
// follow the people" expressed as a number rather than as a migration.  An
// empty arena's target is 0, so the bots left standing in one a person has just
// walked out of are a SURPLUS and the removal arm takes them, one a tick, while
// the arena that person walked into shows a deficit and the add arm fills it.
// Nothing has to move a bot from one arena to another: both halves fall out of
// asking every arena the same question.  It is also why filling empty arenas is
// not a policy that has to be forbidden -- an empty arena simply never wants
// anybody, so `maxclients` and the roster are spent only where people are.
//
// SURPLUSES ARE TAKEN BEFORE DEFICITS ARE FILLED, which is the opposite of the
// first draft and the difference between converging and deadlocking.  The bots
// standing in the arena everyone just left are holding the very client slots
// the arena they left for needs; preferring the deficit asks for a slot that
// only the surplus can release, and on a full server neither side ever moves.
//
// ...but only a surplus something can be DONE about.  Four people in a 1v1
// arena are permanently over its target with no bot to remove, and a candidate
// that cannot be acted on would be re-selected every tick and starve every
// other arena of the fill.  So the surplus arm asks for a bot to take.
//
// Parameter:               gapout: if non-NULL, the selected arena's gap --
//                          positive means it is short by that many, negative
//                          means it is carrying that many too many
// Returns:                 the arena to work, or 0 -- nothing to do anywhere
// Changes Globals:     -
static int RA_NeediestArena(int *gapout)
{
    int i, here, want, gap, nbots;
    int best = 0, bestgap = 0;
    int spare = 0, sparegap = 0;

    for (i = 1; i <= num_arenas; i++) {
        // An arena that has voted bots out is RA_BotsVotedOut's to empty, not
        // this one's: it wants no bots AND will not have the fill add any, so
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

    // The surplus first; see the block above for why that way round.
    if (gapout)
        *gapout = spare ? sparegap : bestgap;

    return spare ? spare : best;
}

// R-RA-9: ...AND WHICH ARENA THE BOT IT IS ABOUT TO ADD MUST BE TOLD TO JOIN.
//
// `bl_spawn.c` writes an `arena` userinfo key at bot creation and
// RA_BotJoinArena obeys it.  While the fill only ever worked one arena, 0 --
// "follow the people" -- was a good enough instruction, because following the
// people and being sent to the fill's arena were the same journey.  Under a
// scheduler they are not: RA_AutoArena resolves 0 to the LOWEST-numbered
// populated arena, so every bot the fill added for arena 5 would walk into
// arena 1 instead and the scheduler would ask for another one next tick,
// forever.
//
// 0 is still returned where it is still right, and that is not a detail: a
// STAGING bot -- added before anybody had joined -- must keep it, because
// RA_BotFollowPeople only moves bots that have it (RA_BotFollowsPeople), and
// moving those is the whole of R-131.  Pinning a staging bot to the arena it
// was parked in would strand it there for the level.
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

    // A NEW BOT ONLY EVER BELONGS IN AN ARENA THAT IS SHORT.  RA_NeediestArena
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

// THE ARENA'S OWN NUMBER, unclamped.  The ceilings -- two sides make a round,
// the roster, `game.maxclients` -- are BotFillTarget()'s, in bl_spawn.c, and
// they used to be here as well: the same three lines over a second
// `botfill_ceiling` private to this file.  Two copies of one concept is what
// sec 7 rule 6 forbids, and the unification of the three `*_botfill` cvars is
// what made keeping them apart indefensible.
int RA_BotFillTarget(int arenanum)
{
    if (arenanum < 1 || arenanum > num_arenas)
        return 0;

    // R-RA-9: AND AN ARENA WITH NOBODY IN IT WANTS NONE, which belongs HERE and
    // not in the scheduler that first needed it.  This is the number
    // CheckMinimumPlayers compares its census against, so a rule the scheduler
    // applied privately was a rule the two of them disagreed about: the
    // scheduler scored the arena a person had just left as a surplus and
    // selected it, CheckMinimumPlayers then asked this function, got the
    // arena's CAPACITY back, and concluded it was short.  Measured: an arena
    // everybody had left sat at two bots for as long as the map ran, selected
    // every tick and neither filled nor drained.
    //
    // `botfill` exists to give PEOPLE opponents.  An arena's capacity is what
    // it can seat; what it wants is that number only while somebody is in it.
    if (!RA_ArenaHasHuman(arenanum))
        return 0;

    if (arenas[arenanum].idarena)
        return 2 * (ArenaSpawnCount("info_player_deathmatch", arenanum) / 2);

    return arenas[arenanum].numteams * arenas[arenanum].playersperteam;
}

// Is this a PICKUP arena -- one whose size comes from the map rather than from
// `arena.cfg`?  `sv ruleset` says which of the two answered, and reaching into
// `arenas[]` from the bot layer to find out would put a donor's struct in a
// shared file for one boolean (R-VER-25).
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
// G_RunFrame BEFORE G_CheckRules, and both gate on `level.framenum & 31`, so on
// a fill tick the bots in transit have not moved yet.  Counted where they
// stand, the arena looks emptier than it is about to be, the fill adds
// replacements for bots that are already on their way, and the removal arm
// throws those out 32 frames later -- an add/remove oscillation at every level
// start, which is the shape R-134 already had to fix once.
int RA_ArenaPlayers(int arenanum, int *bots)
{
    edict_t *e;
    int     i, n, players = 0, nbots = 0;

    for (i = 0; i < game.maxclients; i++) {
        e = &g_edicts[i + 1];

        if (!e->inuse || !e->client)
            continue;
        if (e->client->resp.teamnum < 0 || !teams[e->client->resp.teamnum].it)
            continue;

        n = TEAM(&teams[e->client->resp.teamnum])->arenanum;
        if (n != arenanum && !RA_BotBoundFor(e, arenanum))
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
char *RA_ArenaBotName(int arenanum)
{
    edict_t *e;
    int     i, n;

    for (i = 0; i < game.maxclients; i++) {
        e = &g_edicts[i + 1];

        if (!e->inuse || !e->client || !(e->flags & FL_BOT))
            continue;
        if (e->client->resp.teamnum < 0 || !teams[e->client->resp.teamnum].it)
            continue;

        n = TEAM(&teams[e->client->resp.teamnum])->arenanum;
        if (n != arenanum && !RA_BotBoundFor(e, arenanum))
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

    // R-ARENA-3's invariant, stated where the mode becomes real rather than at
    // each place that picks one: A BOT IS A FREE-FLYING OBSERVER AND NOTHING
    // ELSE.  The two camera modes exist to be looked at, and a bot has no
    // screen: it would spend the round flying a camera nobody watches, and
    // R-140's fire gate is the only other thing that would notice.  The
    // sharper reason this guard was written no longer holds and is recorded
    // rather than repeated -- ClientThink used to put a track_target client on
    // PM_FREEZE, which is what the brain's BotIntermission() tests for, so a
    // bot handed a camera decided the level had ended and said its end-of-level
    // line every frame.  R-145 restored the donor's PM_GIB there, so that
    // particular consequence is gone; the guard stays because the first reason
    // was always the real one.  `arena.cfg`'s `competition: 1` reaches this the
    // same way the observer key does, so it belongs here and not only at the
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
        // THE ID ROW OUTLIVES THE CAMERA THAT WROTE IT.  CTFSetIDView is
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
    vec3_t      temp, temp2;
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

    // try to properly clip to the floor / spawn.  Q2PRO added this to
    // PutClientInServer(); in Rocket Arena the arena spot, not the spawn
    // point, is where the player actually lands, so it belongs here.
    VectorCopy(dest->s.origin, temp);
    VectorCopy(dest->s.origin, temp2);
    temp[2] -= 64;
    temp2[2] += 16;
    tr = gi.trace(temp2, mins, maxs, temp, ent, MASK_PLAYERSOLID);
    if (!tr.allsolid && !tr.startsolid) {
        VectorCopy(tr.endpos, ent->s.origin);
        ent->groundentity = tr.ent;
    } else {
        VectorCopy(dest->s.origin, ent->s.origin);
        ent->s.origin[2] += 10;
    }
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

    membercount = count_queue(&teams[ent->client->resp.teamnum]);

    if (!(membercount != arenas[arenanum].playersperteam
          && (membercount > arenas[arenanum].playersperteam || !allow_partial))) {
        TEAM(&teams[ent->client->resp.teamnum])->outofline = skip_checks;

        if (!skip_checks) {
            remove_from_queue(&TEAM(&teams[ent->client->resp.teamnum])->arenalink, NULL);
            SendTeamToArena(&teams[ent->client->resp.teamnum], arenanum, true, true);
        } else
            SendTeamToArena(&teams[ent->client->resp.teamnum], arenanum, true, false);

        return 0;
    }

    if (count_queue(&teams[ent->client->resp.teamnum]) < arenas[arenanum].playersperteam) {
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
    // and R-MENU-4's second half says a bot is never sent a layout.  Its two
    // clicks are made for it in RA_BotJoinArena, after placement.
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
TWO facts and the sites that read it kept only one of them.

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

// R-RA-5's `ra_botcycle`, and it is the half RA2 does NOT have, because RA2
// has no bots: "no bot hogs the arena while a person waits".
//
// Gladiator's own arena spells it as RA2_GetLongestWaitingHuman() -- pick the
// human who has been waiting longest, then everyone on their team, then fill
// the other side from the queue.  RA2's queue already IS "longest waiting"
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
        // Only the FIRST side is picked for a person: the donor's own shape,
        // where RA2_StartMatch asks for the longest-waiting human once and
        // then takes the longest-waiting anyone for the other side.  Doing it
        // for every side would empty the arena of bots and there would be
        // nothing to play against.
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

THE CONFIGSTRING IS `general`, NOT `playerskins`, and that is the whole defect
this used to have.  `stat_string` draws a configstring VERBATIM, and
`playerskins + n` is "name\model/skin" -- ClientUserinfoChanged and
setteamskin() both write it in exactly that form -- so the row read
"Sarge\male/red" instead of "Sarge".  Threewave's own id view answers this by
keeping the bare name in a configstring of its own (R-CTF-6, and the comment on
the write in p_client.c); RA2 reached for the one it already had.  The write is
now made under arena too, so the base moves here and nothing else changes.
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
        // R-SEC-4: `numteams` is the index about to be WRITTEN, so the guard is
        // the last valid index and not the count.  The donor tested against the
        // count, so a third active team wrote teamname[2], membercount[2] and
        // two rows of names[2][]/health[2][] one past three stack arrays -- and
        // the member guard three lines down has the right shape to compare it
        // against.
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

    // THE SLOT NUMBERS ARE RESOLVED, NOT SPELLED.  This function composes a bar
    // of its own rather than going through G_Statusbar(), and the donor wrote
    // `num 2 19` and `stat_string 20` as literals -- the two numbers arena's
    // column of STATSLOT_MAP happens to give SID_RA_LINEPOSITION and
    // SID_RA_ID_VIEW.  The writes a few lines below go through G_SetStat, which
    // reads that map, so a literal here is a second copy of a number only one
    // of the two would notice changing; and G_InitStats() may DROP a slot the
    // running configuration cannot reach, which a literal cannot express at all
    // (R-OSP-7, R-COMPAT-5).  Ask the map, and omit the row it has no slot for.
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

                // R-RA-5's `ra_playercycle`, and RA2 already implements the
                // behaviour: "winners kept between matches" IS the winning
                // team going to the FRONT of the waiting queue, because
                // fill_arena pops from the front.  What was missing is the
                // cvar, so a server could not turn it off -- with
                // `ra_playercycle 0` a winning team queues like everybody
                // else and the arena rotates strictly by arrival.
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

    // A NON-NEGATIVE `resp.teamnum` MUST INDEX A LIVE TEAM, and the line above
    // is where that would stop being true if nothing had already seen to it.
    // R-KEY-5 puts the clear where the ARRAY IS DROPPED rather than where the
    // replacement is made, so it is in g_spawn.c's loop at the
    // gi.FreeTags(TAG_LEVEL) -- beside the four menu handles that are the same
    // contract -- and not here.  The read side is RA_TeamOf(), which asks both
    // halves of the question.  See that loop, and doc/reconciliation.md R-186.

    admincode = gi.cvar("admincode", "0", 0);
    // R-RA-5's two, defaulting on, which is RA2's own unconditional behaviour.
    // p_botmenu.c obtains both with the same default and toggles them, which is
    // what R-BOT-28's arena menu page is for; the default agreeing is what
    // R-COMPAT-6 asks of a name obtained in two translation units.
    ra_playercycle = gi.cvar("ra_playercycle", "1", 0);
    ra_botcycle = gi.cvar("ra_botcycle", "1", 0);
    // R-RA-7, and OFF by default: `minimumplayers` is what a server that says
    // nothing gets, exactly as it always has.

    num_arenas = wsent->arena;  //worldspawn arena flag is # of arenas
    if (!num_arenas) {
        num_arenas = 1;
        idmap = true;
    } else idmap = false;

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
            if (!t)
                continue;
            t->side = 0;
            SendTeamToArena(t->arenalink.it, i, true, true);
            arenas[i].pickupteam[0] = t;

            name = gi.TagMalloc(ARENA_TEAMNAME_SIZE, TAG_LEVEL);
            Q_snprintf(name, ARENA_TEAMNAME_SIZE, "#%d Pickup Blue", i);
            t = add_to_team(NULL, name);
            if (!t)
                continue;
            t->side = 1;
            SendTeamToArena(t->arenalink.it, i, true, true);
            arenas[i].pickupteam[1] = t;

            arenas[i].maxteams = 2;
            arenas[i].playersperteam = 128;
        }
    }

    load_motd();
}

// Prefixed per sec 7 rule 4 and reached from g_misc.c's per-ruleset dispatcher,
// which already chose between Threewave's and Ground Zero's (R-44).
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

// RA2's SP_info_teleport_destination was here and was an EMPTY body -- the
// classname has to exist so ED_CallSpawn does not reject the entity, and RA2
// needs nothing else from it.  g_misc.c already owns that classname and
// dispatches it per ruleset (R-44), so an empty stub here is a link collision
// rather than a feature; the arena arm is a row in that dispatcher.

// RA2's grapple was here -- 280 lines, CTFPlayerResetGrapple through
// CTFWeapon_Grapple, function for function the same set Threewave ships.  That
// is the measurement R-50 made and sec 7 rule 6's resolution: ONE grapple, and
// it is src/ctf/g_ctf.c's.  RA2's is a fork of it, not a rival design, so this
// is a deletion rather than a choice between two implementations.  What RA2
// tunes on top of it -- `allow_grapple` -- is a parameter on that code.


/*
==================
RA_Obituary

RA2's side of a death: the round statistics, the SCORE, and the announcer.  The
obituary TEXT stays baseq2's (merged with Threewave's and both mission packs');
RA2's copy of it is a stale fork of id's with sounds bolted on, which sec 7 rule
1 decides -- so this is the part that is RA2's feature and nothing else.

*THE SCORE IS ALL OF IT, and that is R-158.*  This used to take only the suicide
half and leave the rest to the baseq2 obituary underneath, which double-counted
every one of them: `ClientObituary` calls this first and then reaches its own
`resp.score--` on the very same paths, so a fall, a lava bath, a drowning, a
trigger_hurt or a self-rocket cost TWO frags where 1999 costs one.  The kill
half was worse than double-counted, it was wrong: baseq2 keys the team-kill
penalty on `MOD_FRIENDLY_FIRE`, which `T_Damage` only raises when `dmflags`
carries the team bits, and arena never sets them -- so a team kill scored +1.
And `scorebydamage` suppresses frag scoring altogether in the donor, because
there the number is `damagedealt / 100` and g_combat.c owns it; baseq2 knows
nothing about that and kept adding to it.

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

        // The donor counts this one differently in the two arms this branch
        // merges, and the asymmetry is deliberate rather than an oversight: a
        // real self-kill (`attacker == self`) is a suicide however the arena
        // scores, but a death with no attacker at all -- a fall, lava, a
        // trigger_hurt -- is counted only when the arena scores by frags.  Both
        // arms of ClientObituary are a byte-exact match against the 1999
        // binary, so this is what the statsfile is supposed to say.
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
    // RA2 buckets them by the INFLICTOR'S MODEL -- grenade, rocket, then the
    // attacker's held weapon for the rail -- which cannot see the forty-odd
    // means of death this merged tree has, and which mis-files a held grenade
    // as whatever the shooter happened to be carrying.  `meansOfDeath` is the
    // same question asked by name (sec 7 rule 3), and is what the obituary text
    // above already switches on.
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

Kept as the donor has it and NOT acted on here -- `resp.isbot` is only read by
RA2's own reporting.  Whether a heuristic like this should be enabled by default
on a public server is R-SEC's question in Phase 8, not this merge's.
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
conditions RA2 puts on firing it are RA2's, and R-164 is that they had gone
missing with the fork.

`grap_on` latched `ctf_hookstate` and the shared CTFHookThink() did the rest,
whose gate is `ctf_hook`: a cvar CTFInit registers "1" for EVERY ruleset,
because Threewave's flag and tech paths are reached from shared code.  So under
arena the offhand hook was on unconditionally -- in an arena whose `arena.cfg`
says `grapple: 0` (which the shipped file does), during a countdown, and for an
observer.  `give_ammo` is what made it hard to see: it withholds the Grapple
ITEM correctly, so the weapon-slot grapple obeyed the setting and only the
offhand one did not.

The donor's two conditions are `allow_grapple` -- arena.cfg's `grapple:` key --
and FIGHT_ALIVE, which under arena is not the same question as "not an
observer": FIGHT_DEAD is a third state and a corpse must not grapple.

One thing is deliberately NOT the donor's.  RA2 refires while the button is held
and the hook is not out; this keeps R-CTF-3's FIRED bit, so a press is one
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

*R-161: THE TABLE WAS NEVER FILLED.*  The four arrays are declared at the top of
this file, read below, and were written nowhere -- the donor fills them in
SP_worldspawn and the merge dropped the loop.  All four were therefore zero,
`gi.imageindex` never returns zero for a non-empty name, the match below could
not succeed, and RA_SkinIcon was an unconditional `return level.pic_health`:
RA2's team colour in the health-icon slot has never once appeared.

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

R-OSP-7's table records slot 0 as "also STAT_SKIN_ICON": RA2 draws the player's
team skin where baseq2 draws the health icon.  Same slot, same `pic 0` in the
bar, different writer -- so this is a value, not a second slot.  Falls back to
the health icon for a skin that is not one of the seven team skins.
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
// a ScoreboardMessage row for exactly this (R-MODE-5).  RA2 wrote them in its
// own p_hud.c, where they would have had to be gated three times over.

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
        // so the donor indexed `teams[]` here with no test at all, and
        // `resp.teamnum` is -1 for a client that is on no team: teams[-1],
        // read as a team_t *, dereferenced.  RA_TeamOf answers both halves,
        // and a client with no team belongs to neither side.
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

R-OSP-9.  RA2 has its own map rotation (`maploop.c`) and baseq2 has `sv_maplist`
inside EndDMLevel; the double-rotation bug is what happens when both are live,
and the port that put `get_next_map()` into EndDMLevel had exactly that.  Here
it is the arena ruleset's EndLevel row, so only one rotation can run and which
one is a property of the ruleset rather than of call order (sec 7 rule 6's third
exemption, R-MODE-5's EndLevel hook).

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

R-RA-3's row.  The seven-state round machine is the arena ruleset's match rule,
so it is reached through the dispatch rather than bolted onto G_RunFrame beside
CheckDMRules -- which is where RA2's own port called it, and which left two
match-rule systems with no statement of how they compose.

They DO compose, and the statement is here: `arena_think` runs a **round**
inside one arena, `CheckDMRules` ends the **level**.  RA2 keeps `timelimit` and
`fraglimit` working at level scope and so does this; what changes is that both
now happen on one row rather than at two call sites in G_RunFrame.  The order
between them is not this row's to pick -- it is the donor's, and it is kept
(see below).

`multi_arena_think` walks one arena per frame -- `level.framenum % (num_arenas *
2)` -- so a server with 32 arenas costs one arena's think per frame rather than
32.  `num_arenas` is at least 1 after arena_init(), which is what makes that
modulo safe; the guard says so rather than trusting it, because a divide by zero
here is a server-killing fault in the frame loop.
=================
*/
void RA_CheckRules(void)
{
    // Level scope FIRST, and that is the donor's own order -- CheckDMRules()
    // ahead of multi_arena_think(), in a G_RunFrame that matches the 1999
    // binary byte for byte.  It matters on exactly one frame -- the one
    // where `timelimit` or `fraglimit` trips -- because BeginIntermission sets
    // `level.intermission_framenum` and multi_arena_think returns on it.  With
    // the arena thinking first, that last tick could centerprint "X has won the
    // round!" or restart a countdown on the frame the map ends, and a tick that
    // landed on ASTATE_NEXTROUND wrote a round record and bumped the round --
    // so BeginIntermission's RA2_Stats_End then wrote a second record for a
    // round nobody played.
    CheckDMRules();

    if (num_arenas > 0) {
        multi_arena_think();
        // R-RA-8 before R-131: a bot standing in an arena that has switched
        // bots off is not a bot that should be considered for following the
        // people anywhere, and both sweeps end in the same two calls, so the
        // eviction gets the first word rather than undoing a move just made.
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
    // nothing for those rows to do.  R-MODE-6 makes a NULL row inherit.
};

/*
=================
G_Svcmd_ArenaDump_f

`sv arenadump` -- one line per connected client under `arena`, naming the four
facts no other channel carries together: which ARENA it is in, which TEAM it is
on and whether that team is fighting, the round-machine state of that arena, and
the three edict fields the round's placement writes -- `solid`, `takedamage` and
the origin it was placed at.

It exists because three defects in one play test were invisible to every other
instrument.  A client's own HUD reports its arena; the scoreboard reports its
team; `sv ruleset` counts bots per arena.  None of them can say that two clients
are at the SAME origin with `solid` SOLID_BBOX on both -- which is a wedged
telefrag -- or that a bot is on a team in an arena the people are not in.
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
        // `want` and `here` are printed for EVERY arena and not only the one
        // the fill is aimed at: the point of the row is that the number is the
        // arena's own, so a reader can see all of them differ (R-RA-7).
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

        // THE LOADOUT, WHICH NOTHING ELSE COULD SAY.  R-182 recorded that the
        // whole feature is `sv`-invisible and had to be measured with a probe
        // compiled into give_ammo(); two of its defects then shipped -- pack
        // weapons that no arena ever granted, and Teslas and Traps handed out
        // by servers running neither pack -- and both are exactly what one
        // line here reports.
        //
        // `mask` is what arena.cfg and the settings menu STORE; `grants` is
        // what give_ammo() honours, which differs by every pack weapon whose
        // content layer is off (the bit is carried, not cleared -- see
        // give_ammo).  Printing both is the point: equal means the layers agree
        // with the config, and a difference names which weapon the layer took.
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

    // Two clients whose bounding boxes overlap is the state a wedged telefrag
    // leaves behind, and it is worth naming rather than leaving to be read off
    // the coordinates above.
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
