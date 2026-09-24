/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "g_local.h"
#include "bot/p_observer.h"
#include "arena/arena.h"
// BeginIntermission closes every arena's round record, which is
// ra2stats.c's and not arena.h's.
#include "arena/ra2stats.h"
#include "tourney/p_menu.h"
// The intermission arm asks the client census and clears the rune HUD.
#include "tourney/osp_hooks.h"
#include "bot/bl_main.h"
#include "bot/p_botmenu.h"

/*
===============================================================================

Menu ownership and the observer question

Two small arbiters, here because p_hud.c is where the other claimants of the
layout channel already live -- the scoreboard, the inventory and the help
computer -- and because neither belongs inside any one donor's engine.

===============================================================================
*/

// One owner per client, and the open path closes the incumbent.
// Which engine that is is the owner field's business, not the caller's: an
// engine does not know the others exist, so nothing but this function can close
// a menu it did not open.
void G_MenuClose(edict_t *ent)
{
    menu_owner_t who;

    if (!ent->client)
        return;

    // The owner is cleared before the engine's own close runs, not after,
    // because an engine that draws into the statusbar has to put the real bar
    // back on the way out and DisplayMenu() decides which bar that is by asking
    // this field.  With the order the other way round it would redraw the menu
    // it was closing.
    who = ent->client->menu_owner;
    ent->client->menu_owner = MENU_NONE;

    switch (who) {
    case MENU_CTF:
        ctf_PMenu_Close(ent);
        break;
    case MENU_ARENA:
        ra_MenuClose(ent);
        break;
    case MENU_TOURNEY:
        osp_PMenu_Close(ent);
        break;
    case MENU_BOT:
        bot_MenuClose(ent);
        break;
    case MENU_NONE:
        break;
    }
}

// One undo for the layout channel.  Four menu engines and the SDK's loading
// image all write into it and every one of them needs the same take-back; the
// donors each open-coded it against their own idea of which statusbar to
// repaint, which is the same defect in the small.
//
// An empty layout is what stops the client drawing one.  It is not a statusbar
// write: CS_STATUSBAR is the composed bar already put there and it has
// not moved -- only RA2's menu overwrites that, and ra_MenuClose is what puts
// it back.
void G_LayoutClear(edict_t *ent)
{
    if (!ent->client)
        return;
    if (ent->flags & FL_BOT)
        return;     // a bot has no network connection to unicast to
    gi.WriteByte(svc_layout);
    gi.WriteString("");
    gi.unicast(ent, true);
}

void G_MenuOpen(edict_t *ent, menu_owner_t who)
{
    G_MenuClose(ent);
    ent->client->menu_owner = who;
}

bool G_MenuActive(edict_t *ent)
{
    return ent->client && ent->client->menu_owner != MENU_NONE;
}

// "Is this client asking for a scoreboard?" -- one question, two fields, for
// the same reason G_IsObserver() exists.  baseq2, ctf and tourney toggle
// `showscores`; RA2 deleted it and put a three-state `scoremode` in its place
// -- 0 off, 1 the arena board, 2 the server-wide one -- because it has two
// boards to cycle between and a bool cannot say which.  The merged gclient_t
// keeps both, so the sites that must know ask by name rather than each picking
// a field and being right for one ruleset.
bool G_ScoreboardUp(edict_t *ent)
{
    if (!ent->client)
        return false;
    if (G_Ruleset() == RULESET_ARENA)
        return ent->client->scoremode != 0;
    return ent->client->showscores;
}

// "Is this client watching rather than playing?" has two answers in
// one library and thirteen call sites that must not have to know which.
//
// baseq2 has a `spectator` userinfo key, a password, a limit and a pers/resp
// pair that ClientBeginServerFrame watches for a change.  Threewave has none of
// that: it deleted both fields and expressed the same state as
// `ctf_team == CTF_NOTEAM`, joined and left through the menu.  Both survive
// (the merged union, and the campaign), so the question is asked by name.
bool G_IsObserver(edict_t *ent)
{
    if (!ent->client)
        return false;
    // The Gladiator observer is a fourth spelling, and it is the one that is a
    // flag rather than a field.  Asked first and for every ruleset: it is only
    // ever set under dm/sp/ctf, and asking it unconditionally means the
    // thirteen sites that already call this predicate see it without any of
    // them being edited.
    if (ent->flags & FL_OBSERVER)
        return true;
    if (G_Ruleset() == RULESET_CTF)
        return ent->client->resp.ctf_team == CTF_NOTEAM;
    // RA2's supported observer state is fightstate. Its donor rejects a fresh
    // generic spectator request despite retaining related inherited plumbing,
    // so this predicate uses the arena's native state.
    if (G_Ruleset() == RULESET_ARENA)
        return ent->client->resp.fightstate == FIGHT_SPECTATING;
    // ...and tourney's is a fifth spelling, added because it was blocking six
    // sites: `resp.spectator` is dead under the OSP four -- the donor deleted
    // baseq2's spectator system -- and "watching" is `resp.osp_entered !=
    // ENTERED_ENTERED`, which is also the donor's own test at every one of
    // those sites.  Two callers diverge from the donor once this answers, and
    // both carry an explicit arm rather than being left to it:
    // ClientBeginDeathmatch announces an ARRIVING observer (the donor's
    // OSP_playerAnnounce is unconditional there) and p_view.c keeps G_SetStats
    // for one, because the donor has no G_SetSpectatorStats at all.
    if (G_IsOspRuleset())
        return ent->client->resp.osp_entered != ENTERED_ENTERED;
    return ent->client->resp.spectator;
}

/*
======================================================================

INTERMISSION

======================================================================
*/

void MoveClientToIntermission(edict_t *ent)
{
    // The donor's first line here is `clear_menus(ent)`, and the merge
    // dropped it: an RA2 menu IS the client's statusbar (menu.c SendMenu
    // overwrites CS_STATUSBAR for the one client), so a menu left open is a
    // menu still on screen over the end-of-level board, with the real bar never
    // written back.  A menu is open on almost every one of these, because
    // move_to_arena(..., 1) reopens the observer menu on every placement.
    //
    // `close_menus` rather than the donor's `clear_menus`: it does the same two
    // things -- drop the queue, repaint through the arbiter -- and FREES the
    // nodes on the way, which `rocketarena2@5f017dc` established is this tree's
    // one teardown.  clear_menus() could afford to forget because the level was
    // about to end; there is no reason to.
    //
    // It also makes g_spawn.c's clearing loop true again.  That loop says "the
    // ordinary map change arrives here with nothing to clear, because
    // MoveClientToIntermission nulled them" -- which stopped being so when this
    // call went missing, leaving it to carry every map change rather than the
    // console `map` it was written for.
    if (G_Ruleset() == RULESET_ARENA)
        close_menus(ent);

    if (deathmatch->value || coop->value) {
        ent->client->showscores = true;
        // RA2 writes `scoremode = 2` here -- the server-wide board, which is
        // the only one that means anything once the level is over.
        if (G_Ruleset() == RULESET_ARENA)
            ent->client->scoremode = 2;
    }
    VectorCopy(level.intermission_origin, ent->s.origin);
    ent->client->ps.pmove.origin[0] = COORD2SHORT(level.intermission_origin[0]);
    ent->client->ps.pmove.origin[1] = COORD2SHORT(level.intermission_origin[1]);
    ent->client->ps.pmove.origin[2] = COORD2SHORT(level.intermission_origin[2]);
    VectorCopy(level.intermission_angle, ent->client->ps.viewangles);
    ent->client->ps.pmove.pm_type = PM_FREEZE;
    ent->client->ps.gunindex = 0;
    ent->client->ps.blend[3] = 0;
    ent->client->ps.rdflags &= ~RDF_UNDERWATER;

    // clean up powerup info
    ent->client->quad_framenum = 0;
    ent->client->invincible_framenum = 0;
    ent->client->breather_framenum = 0;
    ent->client->enviro_framenum = 0;
    ent->client->grenade_blew_up = false;
    ent->client->grenade_framenum = 0;

    // RAFAEL
    ent->client->quadfire_framenum = 0;

    // RAFAEL
    ent->client->trap_blew_up = false;
    ent->client->trap_time = 0;
    ent->client->ps.rdflags &= ~RDF_IRGOGGLES;      // PGM
    ent->client->ir_framenum = 0;                   // PGM
    ent->client->nuke_framenum = 0;                 // PMM
    ent->client->double_framenum = 0;               // PMM

    ent->watertype = 0;
    ent->waterlevel = 0;
    ent->viewheight = 0;
    ent->s.modelindex = 0;
    ent->s.modelindex2 = 0;
    ent->s.modelindex3 = 0;
    ent->s.modelindex4 = 0;
    ent->s.effects = 0;
    ent->s.renderfx = 0;
    ent->s.sound = 0;
    ent->s.event = 0;
    ent->s.solid = 0;
    ent->solid = SOLID_NOT;
    ent->svflags = SVF_NOCLIENT;
    gi.unlinkentity(ent);

    gi.linkentity(ent);

    // add the layout

    if ((deathmatch->value || coop->value) &&
        !(G_IsOspRuleset() && ent->client->resp.osp_r2dc)) {
        G_ScoreboardMessage(ent, NULL);
        gi.unicast(ent, true);
    }

}

void BeginIntermission(edict_t *targ)
{
    int     i, n;
    edict_t *ent, *client;

    if (level.intermission_framenum)
        return;     // already activated

    game.autosaved = false;

    // respawn any dead clients
    for (i = 0; i < game.maxclients; i++) {
        client = g_edicts + 1 + i;
        if (!client->inuse)
            continue;
        if (client->health <= 0 &&
            (!G_IsOspRuleset() ||
             client->client->resp.osp_entered == ENTERED_ENTERED))
            respawn(client);
    }

    level.intermission_framenum = level.framenum;
    level.changemap = targ->map;

    if (strchr(level.changemap, '*')) {
        if (coop->value) {
            for (i = 0; i < game.maxclients; i++) {
                client = g_edicts + 1 + i;
                if (!client->inuse)
                    continue;
                // strip players of all keys between units
                for (n = 0; n < game.num_items; n++) {
                    if (itemlist[n].flags & IT_KEY)
                        client->client->pers.inventory[n] = 0;
                }
                client->client->pers.power_cubes = 0;
            }
        }
    } else {
        if (!deathmatch->value) {
            level.exitintermission = 1;     // go immediately to the next level
            return;
        }
    }

    // Nothing else can end a tourney intermission.  The only writer of
    // `exitintermission` for a deathmatch ruleset is ClientThink, on a button
    // press from a connected client -- so a server that reaches its timelimit
    // with nobody on it (or with nobody but bots, which press nothing) stops
    // here and never changes map again, and OSP_exitLevel's "empty server, go
    // back to the default config" arm becomes unreachable with it.  The donor
    // closes it in this function.
    if (G_IsOspRuleset() && connected_clients - botglobals.numbots <= 0) {
        level.exitintermission = 1;
        return;
    }

    level.exitintermission = 0;

    // find an intermission spot
    ent = G_Find(NULL, FOFS(classname), "info_player_intermission");
    if (!ent) {
        // the map creator forgot to put in an intermission point...
        ent = G_Find(NULL, FOFS(classname), "info_player_start");
        if (!ent)
            ent = G_Find(NULL, FOFS(classname), "info_player_deathmatch");
    } else {
        // chose one of four spots
        i = Q_rand() & 3;
        while (i--) {
            ent = G_Find(ent, FOFS(classname), "info_player_intermission");
            if (!ent)   // wrap around the list
                ent = G_Find(ent, FOFS(classname), "info_player_intermission");
        }
    }

    if (ent) {
        VectorCopy(ent->s.origin, level.intermission_origin);
        VectorCopy(ent->s.angles, level.intermission_angle);
    }

    // move all clients to the intermission point
    n = 0;
    for (i = 0; i < game.maxclients; i++) {
        client = g_edicts + 1 + i;
        if (!client->inuse)
            continue;
        n++;

        // The OSP mover tests this state before choosing whether to send its
        // immediate board, so it must be established before calling it.
        if (G_IsOspRuleset()) {
            client->client->resp.osp_r2dc = 2;
            client->client->resp.osp_r034 = 0;
            OSP_zeroRuneStats(client);
        }
        MoveClientToIntermission(client);
    }

    if (G_Ruleset() == RULESET_ARENA) {
        // First half.  Every arena's round record is closed here in the
        // donor, and the merge left RA2_Stats_End reachable only from
        // arena_think's round boundaries -- so a round still being fought when
        // `timelimit` or `fraglimit` ended the level was never written out.
        // That is the round a report most wants, because it is the one that got
        // cut short.
        for (i = 0; i <= num_arenas; i++) {
            RA2_Stats_End(arenas[i].stats);
            arenas[i].stats = NULL;
        }

        // Second half.  A server that reaches its timelimit with NOBODY on it
        // would otherwise stop here: the donor closes that by asking whether it
        // moved anybody, and this is that question.
        //
        // It used to be the whole of the answer, and the comment here used to
        // say the remaining hole -- an intermission only a press can end -- was
        // "the same under dm and ctf".  That was wrong about `dm`, which is
        // tourney's RegularDM in this tree and so inside G_IsOspRuleset(); the
        // guard above already returns for it.  R-RA-10 closed the rest for
        // `arena` with tourney's own lazy timer, driven from G_RunFrame so that
        // it does not need a client to run it, and this line is now only the
        // EMPTY-server shortcut it always was -- reached before any timer has
        // to tick, and worth keeping for that.
        //
        // `ctf` took the clock too (R-CTF-9), so nothing deathmatch is left on
        // a press alone; `sp` is, and should be.
        if (!n)
            level.exitintermission = 1;
        else
            gi.dprintf("%d clients on level change\n", n);
    }
}

/*
==================
DeathmatchScoreboardMessage

==================
*/
void DeathmatchScoreboardMessage(edict_t *ent, edict_t *killer)
{
    char    entry[1024];
    char    string[1400];
    int     stringlength;
    int     i, j, k;
    int     sorted[MAX_CLIENTS];
    int     sortedscores[MAX_CLIENTS];
    int     score, total;
    int     x, y;
    gclient_t   *cl;
    edict_t     *cl_ent;
    char    *tag;

    // sort the clients by score.  `G_IsObserver()` rather than baseq2's own
    // `resp.spectator`: the Gladiator observer is a flag and is set under sp
    // too, so the generic predicate is what keeps a watching client out of the
    // rankings whichever spelling it arrived in.
    //
    // WHO REACHES THIS BOARD IS A SHORT LIST: `sp` alone, meaning coop's
    // intermission and `score`.  Every deathmatch ruleset fills the
    // ScoreboardMessage row -- CTFScoreboardMessage, RA_ScoreboardMessage,
    // OSP_ScoreboardMessage -- so none of them inherits this one (R-OSP-14).
    total = 0;
    for (i = 0; i < game.maxclients; i++) {
        cl_ent = g_edicts + 1 + i;
        if (!cl_ent->inuse || G_IsObserver(cl_ent))
            continue;
        score = game.clients[i].resp.score;
        for (j = 0; j < total; j++) {
            if (score > sortedscores[j])
                break;
        }
        for (k = total; k > j; k--) {
            sorted[k] = sorted[k - 1];
            sortedscores[k] = sortedscores[k - 1];
        }
        sorted[j] = i;
        sortedscores[j] = score;
        total++;
    }

    // print level name and exit rules
    string[0] = 0;

    stringlength = strlen(string);

    // add the clients in sorted order
    if (total > 12)
        total = 12;

    for (i = 0; i < total; i++) {
        cl = &game.clients[sorted[i]];
        cl_ent = g_edicts + 1 + sorted[i];

        x = (i >= 6) ? 160 : 0;
        y = 32 + 32 * (i % 6);

        // add a dogtag
        if (cl_ent == ent)
            tag = "tag1";
        else if (cl_ent == killer)
            tag = "tag2";
        else
            tag = NULL;

//===============
//ROGUE
        // allow new DM games to override the tag picture
        if (G_UsesRogueGameRules()) {
            if (DMGame.DogTag)
                DMGame.DogTag(cl_ent, killer, &tag);
        }
//ROGUE
//===============

        if (tag) {
            Q_snprintf(entry, sizeof(entry),
                       "xv %i yv %i picn %s ", x + 32, y, tag);
            j = strlen(entry);
            if (stringlength + j > 1024)
                break;
            memcpy(string + stringlength, entry, j + 1);
            stringlength += j;
        }

        // The lag simulation's v0.93 half: the scoreboard shows the simulated ping
        // when it is worse than the real one, so a player who asked for lag
        // reads as lagged to everybody looking at the rankings.
        if (g_clientlag->value)
            Lag_SetClientPing(cl_ent);

        // send the layout
        Q_snprintf(entry, sizeof(entry),
                   "client %i %i %i %i %i %i ",
                   x, y, sorted[i], cl->resp.score, cl->ping, (level.framenum - cl->resp.enterframe) / 600);
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
DeathmatchScoreboard

Draw instead of help message.
Note that it isn't that hard to overflow the 1400 byte message limit!
==================
*/
// Non-static: CTF's admin menu and match code show the scoreboard too, and the
// ruleset picks which message it carries.
void DeathmatchScoreboard(edict_t *ent)
{
    G_ScoreboardMessage(ent, ent->enemy);
    // RA2 redraws the arena board every 32 frames, so it goes out unreliably;
    // only the server-wide board, which is a one-off, is worth a reliable slot.
    gi.unicast(ent, G_Ruleset() != RULESET_ARENA ||
               ent->client->scoremode == 2);
}

/*
==================
Cmd_Score_f

Display the scoreboard
==================
*/
void Cmd_Score_f(edict_t *ent)
{
    ent->client->showinventory = false;
    ent->client->showhelp = false;

    // The scoreboard and the menu are the same channel, so asking for
    // one closes the other -- EXCEPT under arena, where they are not.  Every
    // other menu engine here draws with svc_layout; RA2's draws by overwriting
    // CS_STATUSBAR for the one client (menu.c's SendMenu), so the board and the
    // menu occupy different channels and 1999 showed them together.  RA2's own
    // Cmd_Score_f has no menu test at all.  Keeping one meant an arena observer
    // could never open the board: move_to_arena() reopens the observer menu on
    // every placement, so `score` was always spent closing it.
    if (G_MenuActive(ent) && G_Ruleset() != RULESET_ARENA) {
        G_MenuClose(ent);
        return;
    }

    if (!deathmatch->value && !coop->value)
        return;

    if (G_Ruleset() == RULESET_ARENA) {
        // RA2's `score` cycles: arena board -> server-wide -> off.  A client
        // with no arena has only the server-wide one, so it toggles that.
        if (ent->client->scoremode == 2)
            ent->client->scoremode = 0;
        else if (!ent->client->resp.context)
            ent->client->scoremode = 2;
        else
            ent->client->scoremode++;
        ent->client->update_chase = true;
        if (!ent->client->scoremode)
            return;
        DeathmatchScoreboard(ent);
        return;
    }

    // The scoreboard channel shows five different pages under tourney,
    // and `resp.osp_r24c` says which -- 0 the scoreboard, 1 the previous
    // match's, 2 the MOTD, 4 the match parameters, 8 the player card.  So
    // `score` only DISMISSES from the first two; from any other page it returns
    // to the scoreboard, which is what a player pressing it there means.
    //
    // The four fields reset on the way are what the other pages left behind:
    // the hi-score alternation (osp_r034 / osp_r244), the player-card cursor
    // (osp_r2ac) and the layout slot the MOTD drew into.
    if (G_IsOspRuleset()) {
        if ((ent->client->resp.osp_r24c == 0 || ent->client->resp.osp_r24c == 1) &&
            ent->client->showscores) {
            ent->client->showscores = false;
            ent->client->update_chase = true;
            G_SetStat(ent, SID_OSP_LAYOUT1, 0);
            ent->client->resp.osp_r2ac = -1;
            return;
        }

        if (ent->client->resp.osp_r24c == 4) {
            // The parameters page is on a timer of its own; rewinding it is
            // what stops it drawing itself over the board we are opening.
            ent->client->resp.osp_r0ac = level.framenum - 100;
            if (ent->client->resp.osp_r0ac < 0)
                ent->client->resp.osp_r0ac = 0;
        }

        ent->client->resp.osp_r24c = 0;
        ent->client->showscores = true;
        ent->client->resp.osp_r034 = 1;
        ent->client->resp.osp_r244 = 0;
        DeathmatchScoreboard(ent);
        return;
    }

    if (ent->client->showscores) {
        ent->client->showscores = false;
        ent->client->update_chase = true;
        return;
    }

    ent->client->showscores = true;
    DeathmatchScoreboard(ent);
}

/*
==================
HelpComputer

Draw help computer.
==================
*/
static void HelpComputer(edict_t *ent)
{
    char    string[1024];
    char    *sk;

    if (skill->value == 0)
        sk = "easy";
    else if (skill->value == 1)
        sk = "medium";
    else if (skill->value == 2)
        sk = "hard";
    else
        sk = "hard+";

    // send the layout
    Q_snprintf(string, sizeof(string),
               "xv 32 yv 8 picn help "         // background
               "xv 202 yv 12 string2 \"%s\" "      // skill
               "xv 0 yv 24 cstring2 \"%s\" "       // level name
               "xv 0 yv 54 cstring2 \"%s\" "       // help 1
               "xv 0 yv 110 cstring2 \"%s\" "      // help 2
               "xv 50 yv 164 string2 \" kills     goals    secrets\" "
               "xv 50 yv 172 string2 \"%3i/%3i     %i/%i       %i/%i\" ",
               sk,
               level.level_name,
               game.helpmessage1,
               game.helpmessage2,
               level.killed_monsters, level.total_monsters,
               level.found_goals, level.total_goals,
               level.found_secrets, level.total_secrets);

    gi.WriteByte(svc_layout);
    gi.WriteString(string);
    gi.unicast(ent, true);
}

/*
==================
Cmd_Help_f

Display the current help message
==================
*/
void Cmd_Help_f(edict_t *ent)
{
    if (G_IsOspRuleset()) {
        if (ent->client->resp.osp_r010 <= level.framenum) {
            ent->client->resp.osp_r010 = level.framenum + 2;
            Cmd_Score_f(ent);
        } else if (match_paused) {
            Cmd_Score_f(ent);
        }
        return;
    }

    // this is for backwards compatability
    if (deathmatch->value) {
        Cmd_Score_f(ent);
        return;
    }

    ent->client->showinventory = false;
    ent->client->showscores = false;
    if (G_Ruleset() == RULESET_ARENA)
        ent->client->scoremode = 0;

    if (ent->client->showhelp && (ent->client->pers.game_helpchanged == game.helpchanged)) {
        ent->client->showhelp = false;
        return;
    }

    ent->client->showhelp = true;
    ent->client->pers.helpchanged = 0;
    HelpComputer(ent);
}

//=======================================================================

/*
===============
G_SetStats
===============
*/
void G_SetStats(edict_t *ent)
{
    const gitem_t   *item;
    int         index, cells;
    int         power_armor_type;

    //
    // health
    //
    ent->client->ps.stats[STAT_HEALTH_ICON] = level.pic_health;
    if (G_Ruleset() == RULESET_ARENA)
        ent->client->ps.stats[STAT_HEALTH_ICON] = RA_SkinIcon(ent);
    ent->client->ps.stats[STAT_HEALTH] = ent->health;

    //
    // ammo
    //
    if (!ent->client->ammo_index /* || !ent->client->pers.inventory[ent->client->ammo_index] */) {
        ent->client->ps.stats[STAT_AMMO_ICON] = 0;
        ent->client->ps.stats[STAT_AMMO] = 0;
    } else {
        item = &itemlist[ent->client->ammo_index];
        ent->client->ps.stats[STAT_AMMO_ICON] = gi.imageindex(item->icon);
        ent->client->ps.stats[STAT_AMMO] = ent->client->pers.inventory[ent->client->ammo_index];
    }

    //
    // armor
    //
    power_armor_type = PowerArmorType(ent);
    if (power_armor_type) {
        cells = ent->client->pers.inventory[ITEM_INDEX(FindItem("cells"))];
        if (cells == 0) {
            // ran out of cells for power armor
            ent->flags &= ~FL_POWER_ARMOR;
            gi.sound(ent, CHAN_ITEM, gi.soundindex("misc/power2.wav"), 1, ATTN_NORM, 0);
            power_armor_type = 0;
        }
    }

    index = ArmorIndex(ent);
    if (power_armor_type && (!index || (level.framenum & 8))) {
        // flash between power armor and other armor icon
        if (power_armor_type == POWER_ARMOR_SHIELD)
            ent->client->ps.stats[STAT_ARMOR_ICON] = gi.imageindex("i_powershield");
        else
            ent->client->ps.stats[STAT_ARMOR_ICON] = gi.imageindex("i_powerscreen");
        ent->client->ps.stats[STAT_ARMOR] = cells;
    } else if (index) {
        item = GetItemByIndex(index);
        ent->client->ps.stats[STAT_ARMOR_ICON] = gi.imageindex(item->icon);
        ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.inventory[index];
    } else {
        ent->client->ps.stats[STAT_ARMOR_ICON] = 0;
        ent->client->ps.stats[STAT_ARMOR] = 0;
    }

    //
    // pickup message
    //
    if (level.framenum > ent->client->pickup_msg_framenum) {
        ent->client->ps.stats[STAT_PICKUP_ICON] = 0;
        ent->client->ps.stats[STAT_PICKUP_STRING] = 0;
    }

    //
    // timer 1 (quad, quadfire, double, enviro, breather)
    //
    if (ent->client->quad_framenum > level.framenum) {
        ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_quad");
        ent->client->ps.stats[STAT_TIMER] = (ent->client->quad_framenum - level.framenum) / 10;
    }
    // RAFAEL
    else if (ent->client->quadfire_framenum > level.framenum) {
        // note to self
        // need to change imageindex
        ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_quadfire");
        ent->client->ps.stats[STAT_TIMER] = (ent->client->quadfire_framenum - level.framenum) / 10;
    }
    // ROGUE -- Double Damage
    else if (ent->client->double_framenum > level.framenum) {
        ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_double");
        ent->client->ps.stats[STAT_TIMER] = (ent->client->double_framenum - level.framenum) / 10;
    } else if (ent->client->enviro_framenum > level.framenum) {
        ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_envirosuit");
        ent->client->ps.stats[STAT_TIMER] = (ent->client->enviro_framenum - level.framenum) / 10;
    } else if (ent->client->breather_framenum > level.framenum) {
        ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_rebreather");
        ent->client->ps.stats[STAT_TIMER] = (ent->client->breather_framenum - level.framenum) / 10;
    }
// PGM
    else if (ent->client->owned_sphere) {
        if (ent->client->owned_sphere->spawnflags == 1)         // defender
            ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_defender");
        else if (ent->client->owned_sphere->spawnflags == 2)    // hunter
            ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_hunter");
        else if (ent->client->owned_sphere->spawnflags == 4)    // vengeance
            ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_vengeance");
        else                                                    // error case
            ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("i_fixme");

        ent->client->ps.stats[STAT_TIMER] = (int)(ent->client->owned_sphere->wait - level.time);
    } else if (ent->client->ir_framenum > level.framenum) {
        ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_ir");
        ent->client->ps.stats[STAT_TIMER] = (ent->client->ir_framenum - level.framenum) / 10;
    }
// PGM
    else {
        ent->client->ps.stats[STAT_TIMER_ICON] = 0;
        ent->client->ps.stats[STAT_TIMER] = 0;
    }

    //
    // timer 2 (pent)
    //
    // The reference case for a shared slot: a baseq2 mechanic written by the shared
    // G_SetStats that needs two private slots in every ruleset, and that each
    // donor had to place differently (baseq2 18/19, RA2 26/27, tourney 29/30,
    // and CTF had nowhere at all).  It asks the slot map where its pair landed
    // and does not care; where the ruleset has no pair, both writes and the bar
    // items vanish together and the pent falls back to timer 1 below.
    if (G_Ruleset() == RULESET_ARENA)
        RA_SetQueueStats(ent);

    G_SetStat(ent, SID_TIMER2_ICON, 0);
    G_SetStat(ent, SID_TIMER2, 0);
    if (ent->client->invincible_framenum > level.framenum) {
        if (ent->client->ps.stats[STAT_TIMER_ICON] && G_Stat(SID_TIMER2_ICON) >= 0) {
            G_SetStat(ent, SID_TIMER2_ICON, gi.imageindex("p_invulnerability"));
            G_SetStat(ent, SID_TIMER2, (ent->client->invincible_framenum - level.framenum) / 10);
        } else {
            ent->client->ps.stats[STAT_TIMER_ICON] = gi.imageindex("p_invulnerability");
            ent->client->ps.stats[STAT_TIMER] = (ent->client->invincible_framenum - level.framenum) / 10;
        }
    }

    //
    // selected item
    //
    if (ent->client->pers.selected_item == -1)
        ent->client->ps.stats[STAT_SELECTED_ICON] = 0;
    else
        ent->client->ps.stats[STAT_SELECTED_ICON] = gi.imageindex(itemlist[ent->client->pers.selected_item].icon);

    ent->client->ps.stats[STAT_SELECTED_ITEM] = ent->client->pers.selected_item;

    //
    // layouts
    //
    ent->client->ps.stats[STAT_LAYOUTS] = 0;

    // This bit is what makes a layout visible, and under arena the field that
    // decides it is `scoremode`, not `showscores` (the same substitution
    // p_view.c's redraw and Cmd_Score_f already make).  RA2's p_hud.c reads
    // `scoremode` here.  Reading `showscores` instead meant Cmd_Score_f built
    // the arena board, unicast it, and left the client with no reason to draw
    // it: `score` did nothing at all for a living arena player, and appeared
    // to work only while dead or in intermission, which are the two conditions
    // in the same test.
    if (deathmatch->value) {
        if (ent->client->pers.health <= 0 || level.intermission_framenum
            || G_ScoreboardUp(ent))
            ent->client->ps.stats[STAT_LAYOUTS] |= LAYOUTS_LAYOUT;
        if (ent->client->showinventory && ent->client->pers.health > 0)
            ent->client->ps.stats[STAT_LAYOUTS] |= LAYOUTS_INVENTORY;
    } else {
        if (G_ScoreboardUp(ent) || ent->client->showhelp)
            ent->client->ps.stats[STAT_LAYOUTS] |= LAYOUTS_LAYOUT;
        if (ent->client->showinventory && ent->client->pers.health > 0)
            ent->client->ps.stats[STAT_LAYOUTS] |= LAYOUTS_INVENTORY;
    }

    //
    // frags
    //
    // A tourney observer's score is -100 -- OSP_clientBegunPost sets it on
    // arrival and OSP_startObserve on every later exit -- and the donor
    // therefore draws a 0 rather than the sentinel for a client that has not
    // entered.  Read off the wire before this: an arriving client's HUD said
    // -100.
    if (G_IsOspRuleset() &&
        ent->client->resp.osp_entered != ENTERED_ENTERED)
        ent->client->ps.stats[STAT_FRAGS] = 0;
    else
        ent->client->ps.stats[STAT_FRAGS] = ent->client->resp.score;

    //
    // help icon / current weapon if not shown
    //
    if (ent->client->pers.helpchanged && (level.framenum & 8))
        ent->client->ps.stats[STAT_HELPICON] = gi.imageindex("i_help");
    else if ((ent->client->pers.hand == CENTER_HANDED || ent->client->ps.fov > 91)
             && ent->client->pers.weapon)
        ent->client->ps.stats[STAT_HELPICON] = gi.imageindex(ent->client->pers.weapon->icon);
    else
        ent->client->ps.stats[STAT_HELPICON] = 0;

    G_SetStat(ent, SID_SPECTATOR, 0);

    // CTF's fourteen stats.  A gated tail rather than an ops row: everything
    // above this line is shared and only the last step differs, which is exactly
    // the case to gate inline rather than to hook.
    if (G_Ruleset() == RULESET_CTF)
        SetCTFStats(ent);
}

/*
===============
G_CheckChaseStats
===============
*/
void G_CheckChaseStats(edict_t *ent)
{
    int i;
    gclient_t *cl;

    for (i = 1; i <= game.maxclients; i++) {
        cl = g_edicts[i].client;
        if (!g_edicts[i].inuse || cl->chase_target != ent)
            continue;
        memcpy(cl->ps.stats, ent->client->ps.stats, sizeof(cl->ps.stats));
        G_SetSpectatorStats(g_edicts + i);
    }
}

/*
===============
G_SetSpectatorStats
===============
*/
void G_SetSpectatorStats(edict_t *ent)
{
    gclient_t *cl = ent->client;

    if (!cl->chase_target)
        G_SetStats(ent);

    G_SetStat(ent, SID_SPECTATOR, 1);

    // layouts are independant in spectator
    cl->ps.stats[STAT_LAYOUTS] = 0;
    if (cl->pers.health <= 0 || level.intermission_framenum || G_ScoreboardUp(ent))
        cl->ps.stats[STAT_LAYOUTS] |= LAYOUTS_LAYOUT;
    if (cl->showinventory && cl->pers.health > 0)
        cl->ps.stats[STAT_LAYOUTS] |= LAYOUTS_INVENTORY;

    // A CTF chaser's name plate is a layout, so the layout bit is on.
    //
    // Threewave has no chase element in its statusbar -- SID_CHASE is unmapped
    // under ctf for that reason -- and draws "Chasing <name>" as a unicast
    // layout from UpdateChaseCam() instead.  What makes that visible is the
    // `stats[STAT_LAYOUTS] = 1` its own chase-stat copy forces, and the merge
    // replaced that copy with baseq2's G_CheckChaseStats(), which lands here
    // and recomputes the bit from three conditions a CTF observer meets none
    // of: pers.health is 100 (InitClientPersistant), the scoreboard is down
    // (ctf_PMenu_Close cleared showscores on the way into the chase cam) and
    // there is no intermission.  So the server unicast the line every 32 frames
    // and no client ever drew it -- measured off the wire, layout present,
    // STAT_LAYOUTS 0 -- and with it the whole `update_chase` path that exists
    // to re-send it after `score`, `inven` and `putaway`.
    //
    // Tourney draws the same plate and does not need this: p_view.c sets the
    // bit itself when it copies a tracked player's stats to its watchers.
    if (G_Ruleset() == RULESET_CTF && cl->chase_target)
        cl->ps.stats[STAT_LAYOUTS] |= LAYOUTS_LAYOUT;

    if (cl->chase_target && cl->chase_target->inuse) {
        G_SetStat(ent, SID_CHASE, game.csr.playerskins +
                  (cl->chase_target - g_edicts) - 1);
    } else
        G_SetStat(ent, SID_CHASE, 0);
}
