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
// OSP Tourney DM v2.75, from osp-tourney@1895f8e.
// Donor-only: baseq2 has no counterpart, so it lives in src/tourney/ rather
// than being merged into a spine file.  The reconstruction's
// asm-matching address comments are stripped.
// osp_teams.c -- filename assigned by this tree.  Team play: joining, leaving, the 1v1
// queue, per-team frag accounting and the team client commands.
//
// A client is sent its OWN team's configstring from the plain team name and
// the OTHER team's from the green one; the asymmetry is deliberate.

#include "g_local.h"
#include "tourney/osp_types.h"
#include "tourney/osp_stats.h"
#include "bot/bl_main.h"
#include "bot/bl_botcfg.h"
// The two bot entry points.
void BotServerCommand(char *str, ...);

// A team's name and skin leave the server through two channels a client's
// string must not reach unfiltered: other clients' consoles, by stufftext
// (`skin`, `set default_teamname`, `set default_teamskin`), and every HUD, by
// `string "%s"` layouts.  They arrive as command ARGUMENTS -- `teamname`,
// `teamskin`, `_default_team_info` -- or from a captain's netname, and only the
// netname has been through the engine's userinfo filter, which passes `$`.  A
// Q2PRO client expands `$cvar` in whatever it executes, so a team skin of
// `$rcon_password` would publish each teammate's own password in their
// userinfo, and a `;` in one runs any command on their console.  The donor
// copies all of it raw.  So the characters are chosen once, here, and every
// way in goes through these two.
void OSP_CleanTeamName(char *dst, size_t size, const char *src)
{
    size_t  n = 0;
    int     c;

    while ((c = (unsigned char)*src++) && n + 1 < size)
        if (c > ' ' && c < 127 && c != '"' && c != ';' && c != '\\' && c != '$')
            dst[n++] = c;
    dst[n] = 0;
}

// `model/skin`, each half a path component.  Shorter than MAX_QPATH, which is
// what a skin in userinfo may hold.
bool OSP_ValidTeamSkin(const char *s)
{
    const char  *slash = strchr(s, '/');

    if (!slash || slash == s || !slash[1] || strchr(slash + 1, '/') ||
        strlen(s) >= MAX_QPATH)
        return false;
    for (; *s; s++)
        if (*s != '/' && !Q_ispath(*s))
            return false;
    return true;
}
void BotDestroy(edict_t *bot);

int overtime_timer;
osp_team_t  osp_teams[2];
int frag_offset;

/*
==============
OSP_teamNameFor

resp.team is 2 for a client on no team, so every read of osp_teams[] indexed by it
needs a range test.  This is that test, once.
==============
*/
const char *OSP_teamNameFor(int team)
{
    if (team == 0 || team == 1)
        return osp_teams[team].netname;
    return "no team";
}

int OSP_teamCount(int team)
{
    int         i;
    int         count;

    count = 0;
    for (i = 1; i <= game.maxclients; i++) {
        if (!g_edicts[i].inuse || !g_edicts[i].client ||
            g_edicts[i].client->resp.team != team)
            continue;

        count++;
    }
    return count;
}

int OSP_teamReady(int team)
{
    int         i;
    int         count;

    count = 0;
    for (i = 1; i <= game.maxclients; i++) {
        if (!g_edicts[i].inuse || !g_edicts[i].client ||
            g_edicts[i].client->resp.team != team)
            continue;

        if (g_edicts[i].client->resp.osp_r20c)
            count++;
    }
    return count;
}

// Put a client on a team. `team` == 2 means "no team yet, pick one": the mod
// then tries the client's remembered default team first (OSP_defaultTeam), and
// failing that balances by head count, breaking ties towards team 0 and never
// picking a locked team.
bool OSP_addTeamMember(edict_t *ent, int requested_team)
{
    char        tmp[164];
    edict_t     *p;
    int         t;
    int         team;

    team = requested_team;
    if (requested_team == 2) {
        if (osp_teams[0].osp_m0f4 && osp_teams[1].osp_m0f4) {
            if (!(ent->flags & FL_BOT))
                gi.cprintf(ent, PRINT_HIGH, "Sorry, both teams are locked!\n");
            else
                BotDestroy(ent);
            return false;
        }

        if (OSP_teamCount(0) >= OSP_TeamMaxPlayers() &&
            OSP_teamCount(1) >= OSP_TeamMaxPlayers()) {
            if (!(ent->flags & FL_BOT))
                gi.cprintf(ent, PRINT_HIGH, "Sorry, both teams are full!\n");
            else
                BotDestroy(ent);
            return false;
        }

        if (G_Ruleset() == RULESET_DUEL) {
            if (OSP_1v1Team(ent))
                return true;
            return false;
        }

        if (OSP_defaultTeam(ent))
            return true;

        if ((OSP_teamCount(0) > OSP_teamCount(1) || osp_teams[0].osp_m0f4) &&
            !osp_teams[1].osp_m0f4)
            team = 1;
        else
            team = 0;

        // A lock on one team leaves the other as the only choice, and that one
        // can be full.  Refused like the both-full case above; the donor seated
        // the joiner anyway, one over team_maxplayers.  Unlocked, the smaller
        // team is never the full one, so this is only ever the lock's case.
        if (OSP_teamCount(team) >= OSP_TeamMaxPlayers()) {
            if (!(ent->flags & FL_BOT))
                gi.cprintf(ent, PRINT_HIGH,
                           "Sorry, one team is locked and the other is full!\n");
            else
                BotDestroy(ent);
            return false;
        }
    }

    ent->client->resp.team = team;
    OSP_Stats_TeamJoin(ent);

    if (!(ent->flags & FL_BOT)) {
        Q_snprintf(tmp, sizeof(tmp), "skin \"%s\"\n", osp_teams[team].skin);
        gi.WriteByte(svc_stufftext);
        gi.WriteString(tmp);
        gi.unicast(ent, true);

        Q_snprintf(tmp, sizeof(tmp), "set default_teamname \"%s\"\n",
                   osp_teams[team].netname);
        Q_strlcpy(ent->osp_e3a0, osp_teams[team].netname, sizeof(ent->osp_e3a0));
        gi.WriteByte(svc_stufftext);
        gi.WriteString(tmp);
        gi.unicast(ent, true);

        Q_snprintf(tmp, sizeof(tmp), "set default_teamskin \"%s\"\n",
                   osp_teams[team].skin);
        Q_strlcpy(ent->osp_e3b0, osp_teams[team].skin, sizeof(ent->osp_e3b0));
        gi.WriteByte(svc_stufftext);
        gi.WriteString(tmp);
        gi.unicast(ent, true);
    } else {
        char        userinfo[MAX_INFO_STRING];

        Q_strlcpy(userinfo, ent->client->pers.userinfo, sizeof(userinfo));
        Info_SetValueForKey(userinfo, "skin", osp_teams[team].skin);
        ClientUserinfoChanged(ent, userinfo);
    }

    Q_snprintf(tmp, sizeof(tmp), "%15s", osp_teams[team].netname);
    OSP_clientConfigString(ent, OSP_CS(5) + team * 2, tmp);
    Q_snprintf(tmp, sizeof(tmp), "%15s", osp_teams[1 - team].greenname);
    OSP_clientConfigString(ent, OSP_CS(5) + (1 - team) * 2, tmp);

    if (G_Ruleset() == RULESET_TDM)
        gi.bprintf(PRINT_HIGH, "%s joined team \"%s\"\n",
                   ent->client->pers.greenname, osp_teams[team].netname);

    if (!(ent->flags & FL_BOTCLIENT)) {
        if (G_Ruleset() == RULESET_TDM) {
            // First human on the team becomes captain.
            ent->client->resp.osp_r2c4 = 1;
            for (t = 1; t <= game.maxclients; t++) {
                p = g_edicts + t;
                if (!p->inuse || !p->client ||
                    p->client->resp.osp_entered != ENTERED_ENTERED ||
                    p->client->resp.team != team || p == ent ||
                    (p->flags & FL_BOTCLIENT))
                    continue;
                if (p->client->resp.osp_r2c4) {
                    ent->client->resp.osp_r2c4 = 0;
                    break;
                }
            }

            if (ent->client->resp.osp_r2c4) {
                gi.cprintf(ent, PRINT_CHAT, "*** You are team captain of \"%s\". ***\n",
                           osp_teams[team].greenname);
                if (ent->client->resp.osp_r07d[0])
                    Q_strlcpy(osp_teams[team].joincode,
                              ent->client->resp.osp_r07d,
                              sizeof(osp_teams[team].joincode));
            }
        }
    } else if (G_Ruleset() == RULESET_TDM)
        ent->client->resp.osp_r2c4 = 0;

    if (G_Ruleset() == RULESET_TDM && !(ent->flags & FL_BOT)) {
        if (osp_teams[team].joincode[0])
            gi.centerprintf(ent, "Team joincode is \"%s\"\n", osp_teams[team].joincode);
        else
            gi.centerprintf(ent, "The team joincode has not been set.\n");
    }
    return true;
}

// The "I always play for <name>/<skin>" path. `defaultteam` stores a name and
// a skin on the edict; on connect this matches them against the two teams and,
// if the team it picks is still empty, renames/reskins that team to suit --
// swapping the two teams' names or skins over if the other one is in the way.
bool OSP_defaultTeam(edict_t *ent)
{
    char        msgbuf[64];
    int         team;
    int         i;
    edict_t     *p;
    int         k;

    team = 2;
    if (!ent->osp_e3a0[0])
        return false;

    // A default is a name or a skin to MATCH, and a match is all it is: a
    // locked team is passed over unless this client is invited to it, and a
    // default that matches neither team goes to the balancer in
    // OSP_addTeamMember, as no default does.  The donor fell back to the
    // lowest-numbered team with anybody on it, locked or bigger, and
    // `_default_team_info` is a client command, so that was a way onto either
    // team at will.
    for (k = 1; k >= 0; k--) {
        if (osp_teams[k].osp_m0f4 && ent->client->resp.osp_r078 != k + 1)
            continue;
        if (!Q_stricmp(osp_teams[k].skin, ent->osp_e3b0) ||
            !Q_stricmp(osp_teams[k].netname, ent->osp_e3a0)) {
            team = k;
            break;
        }
    }

    if (team == 2)
        return false;

    if (!OSP_teamCount(team)) {
        if (Q_stricmp(osp_teams[1 - team].netname, ent->osp_e3a0)) {
            if (Q_stricmp(osp_teams[team].netname, ent->osp_e3a0))
                OSP_Stats_TeamRename(osp_teams[team].netname, ent->osp_e3a0);
            Q_strlcpy(osp_teams[team].netname, ent->osp_e3a0, 16);
            Q_strlcpy(osp_teams[team].greenname, ent->osp_e3a0, 16);
            {

                for (i = 0; i < strlen(osp_teams[team].greenname); i++)
                    osp_teams[team].greenname[i] += 128;
            }
            Q_snprintf(msgbuf, sizeof(msgbuf), "%15s", osp_teams[team].greenname);
            gi.configstring(OSP_CS(5) + team * 2, msgbuf);
        } else if (!OSP_teamCount(1 - team)) {
            // The name we want is the OTHER team's and that team is empty, so
            // hand it our name and take theirs.  Its half of the swap is a
            // rename like ours -- logged, re-greened and re-sent; the donor
            // changed its plain name only, so its green name and its HUD cell
            // went on showing the name this team was about to take.
            OSP_Stats_TeamRename(osp_teams[1 - team].netname, osp_teams[team].netname);
            Q_strlcpy(osp_teams[1 - team].netname, osp_teams[team].netname, 16);
            Q_strlcpy(osp_teams[1 - team].greenname, osp_teams[team].netname, 16);
            for (i = 0; i < strlen(osp_teams[1 - team].greenname); i++)
                osp_teams[1 - team].greenname[i] += 128;
            Q_snprintf(msgbuf, sizeof(msgbuf), "%15s", osp_teams[1 - team].greenname);
            gi.configstring(OSP_CS(5) + (1 - team) * 2, msgbuf);

            OSP_Stats_TeamRename(osp_teams[team].netname, ent->osp_e3a0);
            Q_strlcpy(osp_teams[team].netname, ent->osp_e3a0, 16);
            Q_strlcpy(osp_teams[team].greenname, ent->osp_e3a0, 16);
            {

                for (i = 0; i < strlen(osp_teams[team].greenname); i++)
                    osp_teams[team].greenname[i] += 128;
            }
            Q_snprintf(msgbuf, sizeof(msgbuf), "%15s", osp_teams[team].greenname);
            gi.configstring(OSP_CS(5) + team * 2, msgbuf);
        }

        if (Q_stricmp(osp_teams[1 - team].skin, ent->osp_e3b0))
            Q_strlcpy(osp_teams[team].skin, ent->osp_e3b0, sizeof(osp_teams[team].skin));
        else if (!OSP_teamCount(1 - team)) {
            Q_strlcpy(osp_teams[1 - team].skin, osp_teams[team].skin,
                      sizeof(osp_teams[1 - team].skin));
            Q_strlcpy(osp_teams[team].skin, ent->osp_e3b0, sizeof(osp_teams[team].skin));
        }
    } else if (OSP_teamCount(team) >= OSP_TeamMaxPlayers())
        return false;

    ent->client->resp.team = team;
    OSP_Stats_TeamJoin(ent);

    if (!(ent->flags & FL_BOT)) {
        Q_snprintf(msgbuf, sizeof(msgbuf), "skin \"%s\"\n", osp_teams[team].skin);
        gi.WriteByte(svc_stufftext);
        gi.WriteString(msgbuf);
        gi.unicast(ent, true);

        Q_snprintf(msgbuf, sizeof(msgbuf), "%15s", osp_teams[team].netname);
        OSP_clientConfigString(ent, OSP_CS(5) + team * 2, msgbuf);
        Q_snprintf(msgbuf, sizeof(msgbuf), "%15s", osp_teams[1 - team].greenname);
        OSP_clientConfigString(ent, OSP_CS(5) + (1 - team) * 2, msgbuf);
    } else {
        char        userinfo[MAX_INFO_STRING];

        Q_strlcpy(userinfo, ent->client->pers.userinfo, sizeof(userinfo));
        Info_SetValueForKey(userinfo, "skin", osp_teams[team].skin);
        ClientUserinfoChanged(ent, userinfo);
    }

    if (G_Ruleset() == RULESET_TDM)
        gi.bprintf(PRINT_HIGH, "%s joined team \"%s\"\n",
                   ent->client->pers.greenname, osp_teams[team].netname);

    if (!(ent->flags & FL_BOTCLIENT)) {
        ent->client->resp.osp_r2c4 = 1;
        {

            for (i = 1; i <= game.maxclients; i++) {
                p = g_edicts + i;
                if (!p->inuse || !p->client ||
                    p->client->resp.osp_entered != ENTERED_ENTERED ||
                    p->client->resp.team != team || p == ent ||
                    (p->flags & FL_BOTCLIENT))
                    continue;
                if (p->client->resp.osp_r2c4) {
                    ent->client->resp.osp_r2c4 = 0;
                    break;
                }
            }
        }

        if (ent->client->resp.osp_r2c4) {
            gi.cprintf(ent, PRINT_CHAT, "*** You are team captain of \"%s\". ***\n",
                       osp_teams[team].greenname);
            if (ent->client->resp.osp_r07d[0])
                Q_strlcpy(osp_teams[team].joincode, ent->client->resp.osp_r07d,
                          sizeof(osp_teams[team].joincode));
        }
    } else
        ent->client->resp.osp_r2c4 = 0;

    if (G_Ruleset() == RULESET_TDM && !(ent->flags & FL_BOT)) {
        if (osp_teams[team].joincode[0])
            gi.centerprintf(ent, "Team joincode is \"%s\"\n", osp_teams[team].joincode);
        else
            gi.centerprintf(ent, "The team joincode has not been set.\n");
    }
    return true;
}

// `duel`: the "teams" are the two duellists, so the only choice is
// which of the two slots is free. The winner keeps their slot between rounds,
// which is why this renames the team to the player rather than the reverse.
bool OSP_1v1Team(edict_t *ent)
{
    char        tmp[64];
    char        clean[16];
    int         t;
    int         team;

    team = 2;
    for (t = 1; t >= 0; t--)
        if (!OSP_teamCount(t))
            team = t;

    if (team == 2)
        return false;

    // The duellist's slot takes the duellist's name -- cleaned, because a
    // team name is drawn in layouts and stuffed to whoever joins the team
    // (OSP_CleanTeamName).  A name that cleans to nothing leaves the slot the
    // name it had.
    OSP_CleanTeamName(clean, sizeof(clean), ent->client->pers.netname);
    if (clean[0] && Q_stricmp(osp_teams[1 - team].netname, clean)) {
        if (strcmp(osp_teams[team].netname, clean))
            OSP_Stats_TeamRename(osp_teams[team].netname, clean);
        Q_strlcpy(osp_teams[team].netname, clean, 16);
        Q_strlcpy(osp_teams[team].greenname, clean, 16);
        for (t = 0; osp_teams[team].greenname[t]; t++)
            osp_teams[team].greenname[t] += 128;
        Q_snprintf(tmp, sizeof(tmp), "%15s", osp_teams[team].greenname);
        gi.configstring(OSP_CS(5) + team * 2, tmp);
    }

    ent->client->resp.team = team;
    OSP_Stats_TeamJoin(ent);

    if (!(ent->flags & FL_BOT)) {
        Q_snprintf(tmp, sizeof(tmp), "%15s", osp_teams[team].netname);
        OSP_clientConfigString(ent, OSP_CS(5) + team * 2, tmp);
        Q_snprintf(tmp, sizeof(tmp), "%15s", osp_teams[1 - team].greenname);
        OSP_clientConfigString(ent, OSP_CS(5) + (1 - team) * 2, tmp);
    }
    return true;
}

// The 1v1 waiting line. p_order[] is one 112-byte symbol used as four things:
// [0..24] the queue of client numbers, [25] how many are in it, and[26]/[27]
// a "claim your slot by this framenum" deadline for the two players at the
// head of it. A slot whose deadline passes is dropped back into the queue.
bool OSP_1v1AllowJoin(edict_t *ent)
{
    int         i;
    int         until;

    OSP_1v1QueueCheck();

    if (p_order[25] < 2 || !(int)team_nextuptime->value)
        return true;

    if (p_order[27] > 0 && p_order[27] < level.framenum)
        OSP_1v1Remove(&g_edicts[p_order[1] + 1], 0);
    if (p_order[26] > 0 && p_order[26] < level.framenum)
        OSP_1v1Remove(&g_edicts[p_order[0] + 1], 0);

    if (ent - g_edicts - 1 == p_order[0]) {
        p_order[26] = -1;
        return true;
    }
    if (ent - g_edicts - 1 == p_order[1]) {
        p_order[27] = -1;
        return true;
    }

    for (i = 0; i < p_order[25]; i++)
        if (ent - g_edicts - 1 == p_order[i])
            break;

    gi.cprintf(ent, PRINT_CHAT, "*** It is not your turn! ***\n");
    gi.cprintf(ent, PRINT_HIGH, "%d players are ahead of you in line.\n", i);

    until = -1;
    if (!p_order[27])
        p_order[27] = until = level.framenum + (int)team_nextuptime->value * 10;
    else if (p_order[27] > 0)
        until = p_order[27];

    if (!p_order[26])
        p_order[26] = until = level.framenum + (int)team_nextuptime->value * 10;
    else if (p_order[26] > 0 && p_order[26] < until)
        until = p_order[26];

    if (until >= 0 && i == 2)
        gi.cprintf(ent, PRINT_HIGH,
                   "Try again in %d seconds if they have not joined.\n",
                   (until - level.framenum) / 10);
    return false;
}

void OSP_1v1Add(edict_t *ent)
{
    if (G_Ruleset() != RULESET_DUEL || p_order[25] >= 25 || !(int)team_nextuptime->value)
        return;

    p_order[p_order[25]] = ent - g_edicts - 1;
    p_order[25]++;
    OSP_1v1QueueCheck();
}

// mode 1 drops the client out of the queue entirely; anything else moves them
// to the back of it. Only mode 0 also takes them off their team.
void OSP_1v1Remove(edict_t *ent, int mode)
{
    int         i;
    int         j;

    if (!(int)team_nextuptime->value)
        return;

    for (i = 0; i < p_order[25]; i++) {
        if (p_order[i] == ent - g_edicts - 1) {
            if (!i || !(i - 1))
                p_order[26 + i] = 0;
            for (j = i; j < p_order[25] - 1; j++)
                p_order[j] = p_order[j + 1];
            break;
        }
    }

    // Only a client the queue holds has a place in it to give up.  Q2PRO
    // reports every departure (GMF_WANT_ALL_DISCONNECTS), so this also runs for
    // a client that never reached ClientBegin and was never queued, and for one
    // the full queue turned away; the donor ran the tail for them too, dropping
    // the last player in line (mode 1) or writing over them (otherwise).
    if (i < p_order[25]) {
        if (mode == 1) {
            if (p_order[25] > 0)
                p_order[25]--;
        } else if (p_order[25] > 0)
            p_order[p_order[25] - 1] = ent - g_edicts - 1;
    }

    if (!mode)
        ent->client->resp.team = 2;

    OSP_1v1QueueCheck();
}

// Compact the queue: drop any entry that duplicates one ahead of it, and any
// whose client has gone away -- gone meaning not in the game, and either the
// edict is free and we are more than 30 seconds into the level, or the client
// slot is null, or the client is no longer connected.
//
// The gone test is the entry's own and is asked outside the duplicate loop.
// The donor asked it inside, which runs no times for entry 0, so a head who
// had gone was never dropped here.  A dropped head's claim deadline goes with
// it, as in OSP_1v1Remove, rather than passing to whoever moves up.
void OSP_1v1QueueCheck(void)
{
    int         i;
    int         j;
    int         k;
    bool        drop;

    if (!(int)team_nextuptime->value)
        return;

    for (i = 0; i < p_order[25]; i++) {
        edict_t *queued = &g_edicts[p_order[i] + 1];

        drop = !queued->client ||
               (queued->client->resp.osp_entered != ENTERED_ENTERED &&
                ((!queued->inuse && level.framenum - level_start >= 300) ||
                 !queued->client->pers.connected));
        for (j = 0; j < i && !drop; j++)
            drop = p_order[i] == p_order[j];

        if (!drop)
            continue;

        if (i < 2)
            p_order[26 + i] = 0;
        for (k = i; k < p_order[25] - 1; k++)
            p_order[k] = p_order[k + 1];
        i--;
        p_order[25]--;
    }
}

// Take a client off their team. `quiet` suppresses both the announcement and
// the scoreboard configstring update. The captaincy passes to the first other
// human still on the team, and an emptied team is unlocked.
void OSP_removeTeamMember(edict_t *ent, bool quiet)
{
    char        buf[32];
    edict_t     *other;
    int         i;
    int         tno;

    tno = ent->client->resp.team;
    if (tno == 2 || ent->client->resp.osp_entered != ENTERED_ENTERED)
        return;

    if (G_Ruleset() == RULESET_TDM)
        gi.bprintf(PRINT_HIGH, "%s removed from team \"%s\"\n",
                   ent->client->pers.greenname, osp_teams[tno].netname);
    else if (!quiet)
        gi.bprintf(PRINT_HIGH,
                   "%s has become a spectator and moves to the end of the line.\n",
                   ent->client->pers.greenname);

    if (!quiet && !(ent->flags & FL_BOTCLIENT)) {
        Q_snprintf(buf, sizeof(buf), "%15s", osp_teams[tno].greenname);
        OSP_clientConfigString(ent, OSP_CS(5) + tno * 2, buf);
    }

    OSP_Stats_TeamLeave(ent);

    if (ent->client->resp.osp_r2c4) {
        for (i = 1; i <= game.maxclients; i++) {
            other = g_edicts + i;

            if (!other->inuse || !other->client ||
                other->client->resp.osp_entered != ENTERED_ENTERED ||
                other->client->resp.team != tno || other == ent ||
                (other->flags & FL_BOTCLIENT))
                continue;

            other->client->resp.osp_r2c4 = 1;
            gi.cprintf(other, PRINT_CHAT,
                       "*** You are now team captain of \"%s\". ***\n",
                       osp_teams[tno].greenname);
            break;
        }
    }

    ent->client->resp.osp_r2cc = tno;
    ent->client->resp.team = 2;
    ent->client->resp.osp_r2c4 = 0;

    if (!OSP_teamCount(tno))
        osp_teams[tno].osp_m0f4 = 0;
}

// Rejoin the team the client was last on -- resp.osp_r2cc is where
// OSP_removeTeamMember parked it. resp.osp_r078 non-zero means the client got
// here from an invitation, which only changes which "team is full" text they
// get back.
bool OSP_readdTeamMember(edict_t *ent)
{
    char        tmp[64];
    edict_t     *p;
    int         t;
    int         team;

    team = ent->client->resp.osp_r2cc;
    if (team == 2)
        return false;

    if (OSP_teamCount(team) >= OSP_TeamMaxPlayers()) {
        if (ent->client->resp.osp_r078) {
            ent->client->resp.osp_r078 = 0;
            gi.cprintf(ent, PRINT_HIGH, "Sorry, the inviting team is now full!\n");
        } else
            gi.cprintf(ent, PRINT_HIGH, "Sorry, your team is now full!\n");
        return false;
    }

    // The team first and then the log line, as every other join writes them:
    // OSP_Stats_TeamJoin reads resp.team, and in the donor's order it read the
    // team the client was coming from -- "no team" for every switchteam and
    // every rejoin from observer.
    ent->client->resp.team = ent->client->resp.osp_r2cc;
    OSP_Stats_TeamJoin(ent);

    if (!(ent->flags & FL_BOT)) {
        Q_snprintf(tmp, sizeof(tmp), "skin \"%s\"\n", osp_teams[team].skin);
        gi.WriteByte(svc_stufftext);
        gi.WriteString(tmp);
        gi.unicast(ent, true);

        Q_snprintf(tmp, sizeof(tmp), "%15s", osp_teams[team].netname);
        OSP_clientConfigString(ent, OSP_CS(5) + team * 2, tmp);
        Q_snprintf(tmp, sizeof(tmp), "%15s", osp_teams[1 - team].greenname);
        OSP_clientConfigString(ent, OSP_CS(5) + (1 - team) * 2, tmp);
    } else {
        char    userinfo[MAX_INFO_STRING];

        Q_strlcpy(userinfo, ent->client->pers.userinfo, sizeof(userinfo));
        Info_SetValueForKey(userinfo, "skin", osp_teams[team].skin);
        ClientUserinfoChanged(ent, userinfo);
    }

    gi.bprintf(PRINT_HIGH, "%s rejoined team \"%s\"\n",
               ent->client->pers.greenname, osp_teams[team].netname);

    if (!(ent->flags & FL_BOTCLIENT)) {
        ent->client->resp.osp_r2c4 = 1;
        for (t = 1; t <= game.maxclients; t++) {
            p = g_edicts + t;
            if (!p->inuse || !p->client ||
                p->client->resp.osp_entered != ENTERED_ENTERED ||
                p->client->resp.team != team || p == ent ||
                (p->flags & FL_BOTCLIENT))
                continue;
            if (p->client->resp.osp_r2c4) {
                ent->client->resp.osp_r2c4 = 0;
                break;
            }
        }

        if (ent->client->resp.osp_r2c4) {
            gi.cprintf(ent, PRINT_CHAT, "*** You are team captain of \"%s\". ***\n",
                       osp_teams[team].greenname);
            if (ent->client->resp.osp_r07d[0])
                Q_strlcpy(osp_teams[team].joincode, ent->client->resp.osp_r07d,
                          sizeof(osp_teams[team].joincode));
        }
    } else
        ent->client->resp.osp_r2c4 = 0;
    return true;
}

// The two team-score status bar cells are configstrings OSP_CS(6) and OSP_CS(8); a
// client sees its OWN as "(own frags) team [/fraglimit]" and the other team's
// as a bare count. The three functions below are the same formatting for one
// client, for a whole team, and for an observer.
void OSP_initTeamFrags(edict_t *ent)
{
    char        buf[32];
    char        tmp[32];
    int         teamidx;

    teamidx = ent->client->resp.team;
    // R-SEC-4: a client on no team has no own cell to send, and reaches here
    // after a rejoin that was refused (a reconnect onto a full team).
    if (teamidx != 0 && teamidx != 1)
        return;
    if (!(ent->flags & FL_BOT)) {
        if (!(int)fraglimit->value) {
            Q_snprintf(tmp, sizeof(tmp), "(%i) %i", ent->client->resp.score, osp_teams[teamidx].osp_m0f8);
            Q_snprintf(buf, sizeof(buf), "%13s", tmp);
        } else {
            Q_snprintf(tmp, sizeof(tmp), "(%i) %i/%i", ent->client->resp.score, osp_teams[teamidx].osp_m0f8,
                    (int)fraglimit->value);
            Q_snprintf(buf, sizeof(buf), "%13s", tmp);
        }
        OSP_clientConfigString(ent, OSP_CS(6) + teamidx * 2, buf);

        if (ent->client->resp.osp_r210) {
            if (!(int)fraglimit->value)
                Q_snprintf(buf, sizeof(buf), "%13i", osp_teams[1 - teamidx].osp_m0f8);
            else {
                Q_snprintf(tmp, sizeof(tmp), "%i/%i", osp_teams[1 - teamidx].osp_m0f8, (int)fraglimit->value);
                Q_snprintf(buf, sizeof(buf), "%13s", tmp);
            }
            OSP_clientConfigString(ent, OSP_CS(6) + (1 - teamidx) * 2, buf);
        }
    }
}

void OSP_playerTeamFrags(edict_t *ent)
{
    char        buf[32];
    char        tmp[32];
    edict_t     *other;
    int         i;
    int         teamidx;

    teamidx = ent->client->resp.team;
    for (i = 1; i <= game.maxclients; i++) {
        other = g_edicts + i;
        if (!other->inuse || !other->client || (other->flags & FL_BOTCLIENT) ||
            other->client->resp.team != teamidx)
            continue;

        if (!(int)fraglimit->value) {
            Q_snprintf(tmp, sizeof(tmp), "(%i) %i", other->client->resp.score,
                    osp_teams[teamidx].osp_m0f8);
            Q_snprintf(buf, sizeof(buf), "%13s", tmp);
        } else {
            Q_snprintf(tmp, sizeof(tmp), "(%i) %i/%i", other->client->resp.score,
                    osp_teams[teamidx].osp_m0f8, (int)fraglimit->value);
            Q_snprintf(buf, sizeof(buf), "%13s", tmp);
        }
        OSP_clientConfigString(other, OSP_CS(6) + teamidx * 2, buf);
    }
}

void OSP_observerTeamFrags(edict_t *ent)
{
    char        num[32];
    char        msg[32];
    int         n;

    if (sync_stat > 2 && G_Ruleset() == RULESET_TDM) {
        for (n = 0; n < 2; n++) {
            if (!(int)fraglimit->value)
                Q_snprintf(num, sizeof(num), "%13i", osp_teams[n].osp_m0f8);
            else {
                Q_snprintf(msg, sizeof(msg), "%i/%i", osp_teams[n].osp_m0f8, (int)fraglimit->value);
                Q_snprintf(num, sizeof(num), "%13s", msg);
            }
            if (!(ent->flags & FL_BOT))
                OSP_clientConfigString(ent, OSP_CS(6) + n * 2, num);
        }
    }
}

// Push a changed team score out. osp_teams[].osp_m110/osp_m118 cache what was last
// sent so an unchanged score costs nothing. In team mode each client is sent
// only the OTHER team's cell here (its own comes from OSP_playerTeamFrags) and
// the score also goes into the Score_A/Score_B cvars for the server browser;
// otherwise it is one broadcast configstring.
void OSP_updateTeamFrags(void)
{
    char        buf[80];
    char        tmp[32];
    edict_t     *other;
    int         i;
    int         j;

    for (i = 0; i < 2; i++) {
        if (sync_stat > 2) {
            if (osp_teams[i].osp_m110 != osp_teams[i].osp_m0f8 ||
                osp_teams[i].osp_m118 != (int)fraglimit->value) {
                if (!(int)fraglimit->value)
                    Q_snprintf(buf, sizeof(buf), "%13i", osp_teams[i].osp_m0f8);
                else {
                    Q_snprintf(tmp, sizeof(tmp), "%i/%i", osp_teams[i].osp_m0f8, (int)fraglimit->value);
                    Q_snprintf(buf, sizeof(buf), "%13s", tmp);
                }

                if (G_Ruleset() == RULESET_TDM) {
                    for (j = 1; j <= game.maxclients; j++) {
                        other = g_edicts + j;
                        if (!other->inuse || !other->client ||
                            other->client->resp.team == i ||
                            (other->flags & FL_BOT))
                            continue;
                        OSP_clientConfigString(other, OSP_CS(6) + i * 2, buf);
                    }

                    if (!(int)fraglimit->value)
                        Q_snprintf(buf, sizeof(buf), "%i-%s", osp_teams[i].osp_m0f8, osp_teams[i].netname);
                    else
                        Q_snprintf(buf, sizeof(buf), "%i/%i-%s", osp_teams[i].osp_m0f8,
                                (int)fraglimit->value, osp_teams[i].netname);

                    if (!i)
                        gi.cvar_set("Score_A", buf);
                    else
                        gi.cvar_set("Score_B", buf);
                } else
                    gi.configstring(OSP_CS(6) + i * 2, buf);

                osp_teams[i].osp_m110 = osp_teams[i].osp_m0f8;
                osp_teams[i].osp_m118 = (int)fraglimit->value;
            }
        }
    }
}

void OSP_defaultteam_cmd(edict_t *ent)
{
    if (gi.argc() != 3)
        return;

    // Both halves are the CLIENT's cvars, expanded on its side and sent back
    // as arguments, and both are later stuffed to the team's other members.
    OSP_CleanTeamName(ent->osp_e3a0, sizeof(ent->osp_e3a0), gi.argv(1));
    if (OSP_ValidTeamSkin(gi.argv(2)))
        Q_strlcpy(ent->osp_e3b0, gi.argv(2), sizeof(ent->osp_e3b0));
    else
        ent->osp_e3b0[0] = 0;
}

void OSP_defaultjoincode_cmd(edict_t *ent)
{
    if (gi.argc() != 2)
        return;
    Q_strlcpy(ent->client->resp.osp_r07d, gi.argv(1), 16);
}

// `joincode` with no argument, or from a non-captain, prints the code; from a
// captain in the game it sets it; from someone not in the game it is the way
// in -- the code picks the team and hands off to OSP_teamjoin_cmd.
void OSP_joincode_cmd(edict_t *ent)
{
    edict_t     *p;
    int         t;
    int         teamidx;

    teamidx = ent->client->resp.team;
    if (G_Ruleset() != RULESET_TDM || level.intermission_framenum)
        return;

    if (ent->client->resp.osp_entered == ENTERED_ENTERED) {
        // R-SEC-4: an entered client is on team 0 or 1, but one a failed
        // reconnect re-add left teamless carries team 2, and osp_teams[] has
        // two rows.
        if (teamidx != 0 && teamidx != 1)
            return;
        if (!ent->client->resp.osp_r2c4 || gi.argc() == 1) {
            if (osp_teams[teamidx].joincode[0])
                gi.cprintf(ent, PRINT_HIGH, "You're team's joincode is \"%s\"\n",
                           osp_teams[teamidx].joincode);
            else
                gi.cprintf(ent, PRINT_HIGH, "No joincode set for your team.\n");
            return;
        }

        Q_strlcpy(osp_teams[teamidx].joincode, gi.argv(1),
                  sizeof(osp_teams[teamidx].joincode));
        for (t = 1; t <= game.maxclients; t++) {
            p = g_edicts + t;
            if (!p->inuse || !p->client ||
                p->client->resp.team != teamidx)
                continue;

            gi.centerprintf(p, "Team joincode is now \"%s\".\n",
                            gi.argv(1));
        }
        return;
    }

    if (gi.argc() == 1 || !gi.argv(1)) {
        gi.cprintf(ent, PRINT_HIGH, "Usage: joincode <team_joincode_string>\n");
        return;
    }

    if (osp_teams[0].joincode[0] && !Q_stricmp(gi.argv(1), osp_teams[0].joincode)) {
        ent->client->resp.osp_r078 = 1;
        OSP_teamjoin_cmd(ent, osp_teams[0].netname);
    } else if (osp_teams[1].joincode[0] && !Q_stricmp(gi.argv(1), osp_teams[1].joincode)) {
        ent->client->resp.osp_r078 = 2;
        OSP_teamjoin_cmd(ent, osp_teams[1].netname);
    } else
        gi.cprintf(ent, PRINT_HIGH, "Illegal joincode.\n");
}

// `teamname <words>` -- warmup only. The argument is squeezed to at most 15
// characters by OSP_CleanTeamName before it is accepted, so "Red Team" becomes
// "RedTeam".
void OSP_teamname_cmd(edict_t *ent)
{
    char        buf[128];
    char        pname[64];
    char        cmd[64];
    edict_t     *player;
    int         i;
    int         tnum;

    tnum = ent->client->resp.team;
    if (tnum == 2) {
        gi.cprintf(ent, PRINT_HIGH, "You have not joined any team yet.\n");
        return;
    }

    if (gi.argc() == 1) {
        gi.cprintf(ent, PRINT_HIGH, "Current teamname: \"%s\"\n",
                   osp_teams[tnum].netname);
        return;
    }

    if (sync_stat > 2) {
        gi.cprintf(ent, PRINT_HIGH, "Cannot change team's name during match!\n");
        return;
    }

    // Squeezed to at most 15 characters, spaces dropped, and nothing that
    // can escape the stufftext and layouts it is written into.
    OSP_CleanTeamName(pname, 16, gi.args());
    if (!pname[0]) {
        gi.cprintf(ent, PRINT_HIGH, "A team name needs a letter or a digit in it.\n");
        return;
    }

    if (!Q_stricmp(pname, osp_teams[1 - tnum].netname)) {
        gi.cprintf(ent, PRINT_HIGH, "Sorry, cannot use same name for both teams.\n");
        return;
    }

    gi.bprintf(PRINT_HIGH, "Team \"%s\" renamed to \"%s\"\n",
               osp_teams[tnum].netname, pname);
    OSP_Stats_TeamRename(osp_teams[tnum].netname, pname);
    Q_strlcpy(osp_teams[tnum].netname, pname, sizeof(osp_teams[tnum].netname));
    Q_strlcpy(osp_teams[tnum].greenname, pname, sizeof(osp_teams[tnum].greenname));
    for (i = 0; i < strlen(osp_teams[tnum].greenname); i++)
        osp_teams[tnum].greenname[i] += 128;

    Q_snprintf(buf, sizeof(buf), "%15s", osp_teams[tnum].greenname);
    gi.configstring(OSP_CS(5) + tnum * 2, buf);
    Q_snprintf(cmd, sizeof(cmd), "set default_teamname \"%s\"\n", pname);

    for (i = 1; i <= game.maxclients; i++) {
        player = g_edicts + i;
        if (!player->inuse || !player->client || (player->flags & FL_BOT))
            continue;
        if (player->client->resp.team == tnum) {
            Q_snprintf(buf, sizeof(buf), "%15s", osp_teams[tnum].netname);
            OSP_clientConfigString(player, OSP_CS(5) + tnum * 2, buf);
            gi.WriteByte(svc_stufftext);
            gi.WriteString(cmd);
            gi.unicast(player, true);
        }
    }

    if (G_Ruleset() == RULESET_TDM) {
        gi.cvar_set("Score_A", "WARMUP");
        gi.cvar_set("Score_B", "WARMUP");
    }
    OSP_setShowParams();
}

// `teamskin <skin>` -- warmup only, and only when the server has not set
// team_lockskin.  The real image has two faults here and both are fixed, as
// they are at the donor's pin (R-OSP-4): its bot arm rewrote the CALLER's
// userinfo rather than that of the client it was looping over, and installed
// the skin indexed by the client loop counter rather than by the team -- past
// the end of the two-team array from the second client on.
void OSP_teamskin_cmd(edict_t *ent)
{
    char        stuff[320];
    edict_t     *p;
    int         t;
    int         teamidx;

    teamidx = ent->client->resp.team;
    if (ent->client->resp.team == 2) {
        gi.cprintf(ent, PRINT_HIGH, "You have not joined any team yet.\n");
        return;
    }

    if (gi.argc() == 1) {
        gi.cprintf(ent, PRINT_HIGH, "Current teamskin: \"%s\"\n", osp_teams[teamidx].skin);
        return;
    }

    if ((int)team_lockskin->value) {
        gi.cprintf(ent, PRINT_HIGH, "Sorry, teamskins are locked by server.\n");
        return;
    }

    if (sync_stat > 0) {
        gi.cprintf(ent, PRINT_HIGH, "Cannot change team's skin after warmup!\n");
        return;
    }

    if (!OSP_ValidTeamSkin(gi.argv(1))) {
        gi.cprintf(ent, PRINT_HIGH, "A team skin is model/skin, in letters, "
                   "digits, '_' and '-'.\n");
        return;
    }

    if (!Q_stricmp(gi.argv(1), osp_teams[1 - teamidx].skin)) {
        gi.cprintf(ent, PRINT_HIGH, "Sorry, cannot use same skin for both teams.\n");
        return;
    }

    gi.bprintf(PRINT_HIGH, "Team %s skin changed to \"%s\"\n",
               osp_teams[teamidx].greenname, gi.argv(1));
    Q_strlcpy(osp_teams[teamidx].skin, gi.argv(1), sizeof(osp_teams[teamidx].skin));
    Q_snprintf(stuff, sizeof(stuff), "skin \"%s\"; set default_teamskin \"%s\"\n",
               osp_teams[teamidx].skin, osp_teams[teamidx].skin);

    for (t = 1; t <= game.maxclients; t++) {
        p = g_edicts + t;
        if (!p->inuse || !p->client ||
            p->client->resp.team != teamidx)
            continue;

        {
            if (p->flags & FL_BOT) {
                char    userinfo[MAX_INFO_STRING];

                Q_strlcpy(userinfo, p->client->pers.userinfo, sizeof(userinfo));
                Info_SetValueForKey(userinfo, "skin", osp_teams[teamidx].skin);
                ClientUserinfoChanged(p, userinfo);
            } else {
                gi.WriteByte(svc_stufftext);
                gi.WriteString(stuff);
                gi.unicast(p, true);
            }
        }
    }
    OSP_setShowParams();
}

// `team [<name>]`. `name` non-NULL is the OSP_joincode_cmd entry, which has
// already picked the team; otherwise the name comes from the command line.
// resp.osp_r078 is the pending invitation: 1 or 2 meaning "invited to team
// 0 or 1", and it is what lets a player past a locked or full team.
void OSP_teamjoin_cmd(edict_t *ent, char *name)
{
    char        teamname[32];
    int         i;
    int         invited;

    // R-SEC-4: an invitation names team 0 or 1 and indexes osp_teams[] below,
    // so any other value is no invitation at all.
    invited = ent->client->resp.osp_r078;
    if (invited != 1 && invited != 2)
        invited = 0;

    if (G_Ruleset() == RULESET_DUEL && ent->client->resp.osp_entered != ENTERED_ENTERED) {
        if (!OSP_1v1AllowJoin(ent))
            return;
    }

    if (gi.argc() == 1) {
        if (ent->client->resp.team == 2)
            gi.cprintf(ent, PRINT_HIGH, "You aren't currently on any team.\n");
        else
            gi.cprintf(ent, PRINT_HIGH, "You are on team \"%s\"\n",
                       osp_teams[ent->client->resp.team].netname);
        return;
    }

    if (name)
        Q_strlcpy(teamname, name, sizeof(teamname));
    else
        Q_strlcpy(teamname, gi.args(), 16);

    if (who_paused == -2) {
        gi.cprintf(ent, PRINT_HIGH, "Sorry, cannot join on a forced pause.\n");
        return;
    }

    for (i = 0; i < 2; i++) {
        if (!Q_stricmp(teamname, osp_teams[i].netname)) {
            if (!((OSP_teamCount(i) >= OSP_TeamMaxPlayers() && !invited &&
                   (G_Ruleset() != RULESET_TDM ||
                    ((int)match_latejoin->value <= 2 &&
                     (sync_stat <= 2 ||
                      (int)match_latejoin->value != 2 ||
                      OSP_teamCount(i) >= OSP_TeamMaxPlayers())))) ||
                  // A lock is lifted by an invitation to THAT team only: the
                  // donor's `!invited` let an invitation to either team open
                  // the other one's lock, though its own "invited only to
                  // team %s" refusal below shows invitations are per team.
                  (osp_teams[i].osp_m0f4 && invited != i + 1))) {
                if (invited) {
                    if (i != invited - 1 &&
                        OSP_teamCount(i) >= OSP_TeamMaxPlayers()) {
                        gi.cprintf(ent, PRINT_HIGH,
                                   "You have been invited to join only team %s\n",
                                   osp_teams[invited - 1].greenname);
                        return;
                    }
                    ent->client->resp.osp_r078 = 0;
                }

                if (ent->client->resp.team != 2) {
                    ent->client->pers.score = 0;
                    ent->client->resp.osp_r0a0 = 0;
                }
                ent->client->resp.osp_r20c = 0;
                OSP_addTeamMember(ent, i);

                if (sync_stat < 4 && ent->client->resp.osp_entered == ENTERED_ENTERED &&
                    !(ent->flags & FL_BOTCLIENT))
                    OSP_notready_cmd(ent, true);

                if (ent->client->resp.osp_entered != ENTERED_ENTERED) {
                    active_clients++;
                    ent->client->chase_target = NULL;
                    ent->client->resp.osp_entered = ENTERED_ENTERED;
                    ent->client->resp.osp_r240 = 0;
                    ent->client->osp_t040 = 0;
                    ent->client->osp_t03c = NULL;
                    if (!ent->client->resp.osp_r030) {
                        ent->client->resp.osp_r030 = 1;
                        ent->client->resp.enterframe = level.framenum;
                    } else
                        ent->client->resp.enterframe =
                            level.framenum - ent->client->resp.osp_r2d4;
                    ent->client->resp.score = ent->client->resp.osp_r248;
                    ent->client->resp.osp_r0a0--;
                    ent->client->resp.osp_r09c--;
                    EntityListAdd(ent);
                    OSP_Stats_PlayerEnter(ent);
                }

                if (sync_stat > 2)
                    OSP_initTeamFrags(ent);
                OSP_setShowParams();
                return;
            }

            if (osp_teams[i].osp_m0f4 && invited != i + 1)
                gi.cprintf(ent, PRINT_HIGH, "\"%s\" is locked.\n", osp_teams[i].netname);
            else
                gi.cprintf(ent, PRINT_HIGH, "\"%s\" is full.\n", osp_teams[i].netname);
            return;
        }
    }

    gi.cprintf(ent, PRINT_HIGH, "Unknown team \"%s\"\n", teamname);
}

void OSP_switchteam_cmd(edict_t *ent)
{
    int         team;
    bool        invited;

    // R-SEC-4: `1 - team` indexes osp_teams[] below.
    team = ent->client->resp.team;
    if (team != 0 && team != 1) {
        gi.cprintf(ent, PRINT_HIGH, "You have not joined any team yet.\n");
        return;
    }

    // And the invitation that gets a player past the lock and the latejoin
    // rule is one to the team they are switching TO, resp.osp_r078 being that
    // team + 1.  The donor took any non-zero value, so the standing invitation
    // a reconnect is given back onto the player's OWN team (osp_main.c) opened
    // the other team's lock as well.
    invited = ent->client->resp.osp_r078 == 2 - team;

    if (who_paused == -2) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Sorry, cannot switch teams during a forced pause.\n");
        return;
    }

    if (OSP_teamCount(1 - team) < OSP_TeamMaxPlayers()) {
        // v2.75 refuses in warmup and says the other team is full, which it
        // is not -- the head count above just proved otherwise.  Only the
        // wording is corrected here: whether "switchteam" ought to work in
        // warmup at all is the mod's own call, and "team <name>" does the
        // same thing there.
        if (sync_stat < 4) {
            gi.cprintf(ent, PRINT_HIGH,
                       "Use \"team %s\" to change teams during warmup.\n",
                       osp_teams[1 - team].netname);
            return;
        }

        if (osp_teams[1 - team].osp_m0f4 && !invited) {
            gi.cprintf(ent, PRINT_HIGH, "Sorry, \"%s\" is locked.\n",
                       osp_teams[1 - team].netname);
            return;
        }

        if (!invited && (int)match_latejoin->value < 2) {
            gi.cprintf(ent, PRINT_HIGH,
                       "You need to be invited to switch teams.\n");
            return;
        }

        OSP_removeTeamMember(ent, false);
        ent->client->resp.osp_r2cc = 1 - team;
        OSP_readdTeamMember(ent);
        ent->client->resp.osp_r2c4 = 0;
        ent->client->resp.osp_r20c = 0;
        ent->client->pers.score = 0;

        if (sync_stat < 4)
            OSP_notready_cmd(ent, true);
        OSP_initTeamFrags(ent);
        OSP_setShowParams();
        return;
    }

    gi.cprintf(ent, PRINT_HIGH, "Sorry, the other team is full.\n");
}

// `invite <player>` -- a captain's way past a locked or full team. The
// invitation is stored on the TARGET as resp.osp_r078 = team + 1 (so that 0
// still means "no invitation") plus resp.osp_r2cc as the team to re-add to.
void OSP_teaminvite_cmd(edict_t *ent)
{
    edict_t     *target;

    if (ent->client->resp.osp_entered != ENTERED_ENTERED) {
        gi.cprintf(ent, PRINT_HIGH, "You must be in the game to invite others!\n");
        return;
    }
    if (ent->client->resp.team == 2) {
        gi.cprintf(ent, PRINT_HIGH, "You must be on a team to invite others!\n");
        return;
    }
    if (!ent->client->resp.osp_r2c4) {
        gi.cprintf(ent, PRINT_HIGH, "Only captains can invite others!\n");
        return;
    }

    target = OSP_findPlayer(gi.args());
    if (!target) {
        gi.cprintf(ent, PRINT_HIGH, "Player \"%s\" is not logged on.\n", gi.args());
        return;
    }
    if (target == ent) {
        gi.cprintf(ent, PRINT_HIGH, "You can't invite youself!\n");
        return;
    }
    if (target->client->resp.team == ent->client->resp.team) {
        gi.cprintf(ent, PRINT_HIGH, "\"%s\" is already on your team!\n",
                   target->client->pers.netname);
        return;
    }

    {
        if (OSP_teamCount(ent->client->resp.team) >=
            OSP_TeamMaxPlayers()) {
            gi.cprintf(ent, PRINT_HIGH,
                       "Sorry, your team is already full (max %d players).\n",
                       OSP_TeamMaxPlayers());
            return;
        }
        if (target->client->resp.osp_r078) {
            gi.cprintf(ent, PRINT_HIGH, "\"%s\" has already been invited.\n",
                       target->client->pers.netname);
            return;
        }

        target->client->resp.osp_r030 = 1;
        target->client->resp.osp_r078 = ent->client->resp.team + 1;
        target->client->resp.osp_r2cc = ent->client->resp.team;

        gi.cprintf(target, PRINT_HIGH, "You have been invited to join team %s\n",
                   osp_teams[ent->client->resp.team].greenname);
        gi.cprintf(ent, PRINT_HIGH, "%s has been sent a \"join\" invitation.\n",
                   target->client->pers.greenname);
        OSP_inviteMenu(target);
    }
}

// lock/unlock/readyteam/notreadyteam share one shape: a captain acts on their
// own team, a referee names the team on the command line.  The real image
// prints "Ref: Usage: unlockteam <teamname>" from BOTH lock and unlock -- the
// mod's own copy-paste, reproduced here.
void OSP_lockteam_cmd(edict_t *ent)
{
    int         team;

    team = ent->client->resp.team;

    if (!ent->osp_e39c && !ent->client->resp.osp_r2c4) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Only captains or referees can lock a team.\n");
        return;
    }

    if (ent->osp_e39c) {
        if (ent->client->resp.osp_entered != ENTERED_ENTERED && gi.argc() == 1) {
            gi.cprintf(ent, PRINT_HIGH, "Ref: Usage: unlockteam <teamname>\n");
            return;
        }

        if (gi.argc() > 1) {
            if (!Q_stricmp(gi.args(), osp_teams[0].netname))
                team = 0;
            else if (!Q_stricmp(gi.args(), osp_teams[1].netname))
                team = 1;
            else {
                gi.cprintf(ent, PRINT_HIGH,
                           "Ref (lockteam): unknown team \"%s\"\n", gi.args());
                return;
            }
        }
    }

    if (team == 2) {
        gi.cprintf(ent, PRINT_HIGH, "You have not joined any team yet.\n");
        return;
    }

    osp_teams[team].osp_m0f4 = 1;
    gi.cprintf(ent, PRINT_HIGH,
               "Team locked.  Use \"invite\" to allow others to join.\n");
}

void OSP_unlockteam_cmd(edict_t *ent)
{
    int         team;

    team = ent->client->resp.team;

    if (!ent->osp_e39c && !ent->client->resp.osp_r2c4) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Only captains or referees can unlock a team.\n");
        return;
    }

    if (ent->osp_e39c) {
        if (ent->client->resp.osp_entered != ENTERED_ENTERED && gi.argc() == 1) {
            gi.cprintf(ent, PRINT_HIGH, "Ref: Usage: unlockteam <teamname>\n");
            return;
        }

        if (gi.argc() > 1) {
            if (!Q_stricmp(gi.args(), osp_teams[0].netname))
                team = 0;
            else if (!Q_stricmp(gi.args(), osp_teams[1].netname))
                team = 1;
            else {
                gi.cprintf(ent, PRINT_HIGH,
                           "Ref (unlockteam): unknown team \"%s\"\n", gi.args());
                return;
            }
        }
    }

    if (team == 2) {
        gi.cprintf(ent, PRINT_HIGH, "You have not joined any team yet.\n");
        return;
    }

    osp_teams[team].osp_m0f4 = 0;
    gi.cprintf(ent, PRINT_HIGH, "Team unlocked.  Anybody can now join.\n");
}

void OSP_readyteam_cmd(edict_t *ent)
{
    edict_t     *p;
    int         t;
    int         teamidx;

    teamidx = ent->client->resp.team;
    // The second disjunct re-tests osp_e39c, which the first has already
    // settled -- another of the target's duplicate-condition bugs.
    if (!ent->osp_e39c ||
        (ent->osp_e39c && !ent->client->resp.osp_r2c4 &&
         ent->client->resp.osp_entered == ENTERED_ENTERED && gi.argc() == 1)) {
        if (teamidx == 2) {
            gi.cprintf(ent, PRINT_HIGH, "You have not joined any team yet.\n");
            return;
        }
        if (!ent->client->resp.osp_r2c4 && !ent->osp_e39c) {
            gi.cprintf(ent, PRINT_HIGH,
                       "Only team captain can \"ready\" entire team.\n");
            return;
        }
    } else {
        if (gi.argc() == 1) {
            gi.cprintf(ent, PRINT_HIGH, "Ref: Usage: readyteam <teamname>\n");
            return;
        }
        if (!Q_stricmp(gi.args(), osp_teams[0].netname))
            teamidx = 0;
        else if (!Q_stricmp(gi.args(), osp_teams[1].netname))
            teamidx = 1;
        else {
            gi.cprintf(ent, PRINT_HIGH,
                       "Ref (readyteam): unknown team \"%s\"\n", gi.args());
            return;
        }
    }

    if (sync_stat >= 4)
        return;

    for (t = 1; t <= game.maxclients; t++) {
        p = g_edicts + t;
        if (!p->inuse || !p->client ||
            p->client->resp.team != teamidx ||
            p->client->resp.osp_entered != ENTERED_ENTERED ||
            p->client->resp.osp_r20c)
            continue;

        OSP_ready_cmd(p, true);
        if (sync_stat)
            break;
    }

    gi.bprintf(PRINT_HIGH, "Team \"%s\" is ready!\n", osp_teams[teamidx].greenname);
}

void OSP_notreadyteam_cmd(edict_t *ent)
{
    edict_t     *p;
    int         t;
    int         teamidx;

    teamidx = ent->client->resp.team;
    // The second disjunct re-tests osp_e39c, which the first has already
    // settled -- another of the target's duplicate-condition bugs.
    if (!ent->osp_e39c ||
        (ent->osp_e39c && !ent->client->resp.osp_r2c4 &&
         ent->client->resp.osp_entered == ENTERED_ENTERED && gi.argc() == 1)) {
        if (teamidx == 2) {
            gi.cprintf(ent, PRINT_HIGH, "You have not joined any team yet.\n");
            return;
        }
        if (!ent->client->resp.osp_r2c4 && !ent->osp_e39c) {
            gi.cprintf(ent, PRINT_HIGH,
                       "Only team captain can \"notready\" entire team.\n");
            return;
        }
    } else {
        if (gi.argc() == 1) {
            gi.cprintf(ent, PRINT_HIGH,
                       "Ref (notreadyteam): Usage: notreadyteam <teamname>\n");
            return;
        }
        if (!Q_stricmp(gi.args(), osp_teams[0].netname))
            teamidx = 0;
        else if (!Q_stricmp(gi.args(), osp_teams[1].netname))
            teamidx = 1;
        else {
            gi.cprintf(ent, PRINT_HIGH, "Ref: unknown team \"%s\"\n", gi.args());
            return;
        }
    }

    if (sync_stat >= 4)
        return;

    for (t = 1; t <= game.maxclients; t++) {
        p = g_edicts + t;
        if (!p->inuse || !p->client ||
            p->client->resp.team != teamidx ||
            p->client->resp.osp_entered != ENTERED_ENTERED ||
            !p->client->resp.osp_r20c)
            continue;

        OSP_notready_cmd(p, true);
    }

    gi.bprintf(PRINT_HIGH, "Team \"%s\" is NOT ready!\n", osp_teams[teamidx].greenname);
}

// `captain` with no argument reports the captain; a captain naming a player
// hands the job over; a referee names a team, and with two arguments names a
// team and a player. resp.osp_r2c4 is the captain flag throughout.
void OSP_captain_cmd(edict_t *ent)
{
    edict_t     *other;
    int         i;
    int         prevcap;
    int         tnum;

    tnum = ent->client->resp.team;
    if (ent->osp_e39c) {
        if (ent->client->resp.osp_entered != ENTERED_ENTERED && gi.argc() == 1) {
            gi.cprintf(ent, PRINT_HIGH, "Ref: Usage: captain <teamname>\n");
            return;
        }
        if (gi.argc() > 1 && !ent->client->resp.osp_r2c4) {
            if (!Q_stricmp(gi.argv(1), osp_teams[0].netname))
                tnum = 0;
            else if (!Q_stricmp(gi.argv(1), osp_teams[1].netname))
                tnum = 1;
            else {
                gi.cprintf(ent, PRINT_HIGH,
                           "Ref (captain): unknown team \"%s\"\n", gi.argv(1));
                return;
            }
        }
    }

    if (tnum == 2) {
        gi.cprintf(ent, PRINT_HIGH, "You have not joined any team yet.\n");
        return;
    }

    // Nobody is asking to change anything -- just report.
    if ((!ent->client->resp.osp_r2c4 && !ent->osp_e39c) || gi.argc() == 1 ||
        (!ent->client->resp.osp_r2c4 && ent->osp_e39c && gi.argc() == 2)) {
        for (i = 1; i <= game.maxclients; i++) {
            other = g_edicts + i;
            if (!other->inuse || !other->client ||
                other->client->resp.team != tnum ||
                other->client->resp.osp_entered != ENTERED_ENTERED ||
                !other->client->resp.osp_r2c4)
                continue;

            gi.cprintf(ent, PRINT_HIGH, "Current team captain is \"%s\"\n",
                       other->client->pers.netname);
            return;
        }
        gi.cprintf(ent, PRINT_HIGH, "Currently, there is no team captain.\n");
        return;
    }

    // A captain handing the job over names the new captain in gi.args().
    if (ent->client->resp.osp_r2c4 && gi.argc() > 1) {
        for (i = 1; i <= game.maxclients; i++) {
            other = g_edicts + i;
            if (!other->inuse || !other->client ||
                other->client->resp.team != tnum ||
                other->client->resp.osp_entered != ENTERED_ENTERED ||
                Q_stricmp(gi.args(), other->client->pers.netname))
                continue;

            gi.cprintf(ent, PRINT_HIGH, "Team captain is now \"%s\"\n",
                       other->client->pers.netname);
            gi.cprintf(other, PRINT_HIGH, "You are now team captain.\n");
            ent->client->resp.osp_r2c4 = 0;
            other->client->resp.osp_r2c4 = 1;
            return;
        }
        gi.cprintf(ent, PRINT_HIGH, "\"%s\" is not on team %s.\n",
                   gi.args(), osp_teams[tnum].netname);
        return;
    }

    // Referee form: `captain <tnum> <player>`.
    if (ent->osp_e39c && gi.argc() > 2) {
        prevcap = -1;
        for (i = 1; i <= game.maxclients; i++) {
            other = g_edicts + i;
            if (!other->inuse || !other->client ||
                other->client->resp.team != tnum ||
                other->client->resp.osp_entered != ENTERED_ENTERED ||
                !other->client->resp.osp_r2c4)
                continue;

            prevcap = i;
            break;
        }

        if (prevcap < 0)
            gi.cprintf(ent, PRINT_HIGH, "There is no team captain for \"%s\".\n",
                       osp_teams[tnum].netname);

        for (i = 1; i <= game.maxclients; i++) {
            other = g_edicts + i;
            if (!other->inuse || !other->client ||
                other->client->resp.team != tnum ||
                other->client->resp.osp_entered != ENTERED_ENTERED ||
                Q_stricmp(gi.argv(2), other->client->pers.netname))
                continue;

            gi.cprintf(ent, PRINT_HIGH, "Team captain is now \"%s\"\n",
                       other->client->pers.netname);
            gi.cprintf(other, PRINT_HIGH, "You are now team captain.\n");
            other->client->resp.osp_r2c4 = 1;
            if (prevcap >= 0) {
                g_edicts[prevcap].client->resp.osp_r2c4 = 0;
                gi.cprintf(g_edicts + prevcap, PRINT_HIGH, "Team captain is now \"%s\"\n",
                           other->client->pers.netname);
            }
            return;
        }

        gi.cprintf(ent, PRINT_HIGH, "\"%s\" is not on team %s.\n",
                   gi.argv(2), osp_teams[tnum].netname);
    } else
        gi.cprintf(ent, PRINT_HIGH, "Unknown captain request (%d)\n", gi.argc());
}

void OSP_captains_cmd(edict_t *ent)
{
    edict_t     *other;
    int         i;

    for (i = 1; i <= game.maxclients; i++) {
        other = g_edicts + i;
        if (!other->inuse || !other->client ||
            other->client->resp.osp_entered != ENTERED_ENTERED ||
            !other->client->resp.osp_r2c4)
            continue;

        gi.cprintf(ent, PRINT_HIGH, "Team captain for %s is \"%s\".\n",
                   osp_teams[other->client->resp.team].netname,
                   other->client->pers.netname);
    }
}

// A captain (or a referee, who must name the team first) drops a player from
// the team. A bot is destroyed outright through the Gladiator SDK's `removebot`
// server command; a human is dropped into observer mode.
void OSP_kickplayer_cmd(edict_t *ent)
{
    char        pname[32];
    edict_t     *victim;
    int         i;
    int         tnum;

    tnum = ent->client->resp.team;
    if (!ent->osp_e39c && !ent->client->resp.osp_r2c4) {
        gi.cprintf(ent, PRINT_HIGH, "Only team captains can kick players.\n");
        return;
    }

    if (ent->client->resp.osp_r2c4 && gi.argc() < 2) {
        gi.cprintf(ent, PRINT_HIGH, "Usage: kickplayer <player_name>\n");
        return;
    }

    if (ent->osp_e39c && gi.argc() < 3 && !ent->client->resp.osp_r2c4) {
        gi.cprintf(ent, PRINT_HIGH,
                   "(Referee) Usage: kickplayer <team_name> <player_name>\n");
        return;
    }

    if (ent->client->resp.osp_r2c4)
        Q_strlcpy(pname, gi.args(), sizeof(pname));
    else {
        if (!Q_stricmp(gi.argv(1), osp_teams[0].netname))
            tnum = 0;
        else if (!Q_stricmp(gi.argv(1), osp_teams[1].netname))
            tnum = 1;
        else {
            gi.cprintf(ent, PRINT_HIGH,
                       "Ref (kickplayer): unknown team \"%s\"\n", gi.argv(1));
            return;
        }
        Q_strlcpy(pname, gi.argv(2), sizeof(pname));
    }

    victim = OSP_findPlayer(pname);
    if (!victim) {
        for (i = 1; i <= game.maxclients; i++) {
            victim = g_edicts + i;
            if (!victim->inuse || !victim->client ||
                victim->client->resp.team != tnum ||
                victim->client->resp.osp_entered != ENTERED_ENTERED ||
                Q_stricmp(pname, victim->client->pers.netname))
                continue;
            break;
        }
        if (i > game.maxclients) {
            gi.cprintf(ent, PRINT_HIGH, "\"%s\" is not on team %s\n",
                       pname, osp_teams[tnum].greenname);
            return;
        }
    } else if (victim->client->resp.team != tnum ||
               victim->client->resp.osp_entered != ENTERED_ENTERED) {
        gi.cprintf(ent, PRINT_HIGH, "\"%s\" is not on team %s\n",
                   pname, osp_teams[tnum].greenname);
        return;
    }

    // The victim's own name from here on, not what was typed: OSP_findPlayer
    // also matches a client id, and `removebot` takes a name, so the donor's
    // `kickplayer 3` found the bot and then removed nobody.  The argument list
    // ends in NULL, not the donor's int 0, because it is read back as char *
    // through va_arg, which an int is not on LP64.  And the line is printed
    // once the removal has happened: the donor printed it first, and its human
    // arm was OSP_startObserve, which turns away an injured victim during a
    // match and one with no body yet, so a kick could fail after it had been
    // announced.  OSP_forceObserve has neither refusal.
    Q_strlcpy(pname, victim->client->pers.netname, sizeof(pname));

    if (victim->flags & FL_BOT) {
        BotServerCommand("sv", "removebot", pname, NULL);
        // The target's own oddity, reproduced: the counter is subtracted from
        // itself and the (always zero) result clamped.
        bots_votedin -= bots_votedin;
        if (bots_votedin < 0)
            bots_votedin = 0;
        if (victim->inuse)
            return;
    } else {
        OSP_forceObserve(victim);
        if (victim->client->resp.osp_entered == ENTERED_ENTERED)
            return;
    }

    gi.bprintf(PRINT_HIGH, "%s has been removed from \"%s\"\n",
               pname, osp_teams[tnum].netname);
}

// `queue` -- print the 1v1 waiting line. The two slots at the head of it also
// get their claim deadline seeded here if it has not been set yet, which is
// why a read-only-looking command writes p_order[26]/[27].
void OSP_1v1queue_cmd(edict_t *ent)
{
    char        tmp[128];
    char        scratch[64];
    int         t;

    if (!(int)team_nextuptime->value) {
        gi.cprintf(ent, PRINT_HIGH, "Player queueing currently disabled.\n");
        return;
    }

    gi.cprintf(ent, PRINT_HIGH, "\nCurrent 1v1 queue:\n------------------\n");

    for (t = 0; t < p_order[25]; t++) {
        edict_t *queued = &g_edicts[p_order[t] + 1];

        if (!queued->client)
            continue;

        if (p_order[t] == ent - g_edicts - 1)
            Q_strlcpy(tmp, ent->client->pers.greenname, sizeof(tmp));
        else
            Q_strlcpy(tmp, queued->client->pers.netname, sizeof(tmp));

        if (t < 2) {
            if (queued->client->resp.osp_entered == ENTERED_ENTERED)
                Q_strlcat(tmp, " [Playing]", sizeof(tmp));
            else {
                if (!p_order[26 + t])
                    p_order[26 + t] =
                        level.framenum + (int)team_nextuptime->value * 10;

                if (p_order[26 + t] > 0) {
                    if (p_order[26 + t] < level.framenum)
                        Q_strlcat(tmp, " [Not yet joined --> will give up slot]", sizeof(tmp));
                    else {
                        if (!queued->inuse)
                            Q_snprintf(scratch, sizeof(scratch),
                                    " [Connecting --> must join in %d sec]",
                                    (p_order[26 + t] - level.framenum) / 10);
                        else
                            Q_snprintf(scratch, sizeof(scratch),
                                       " [Not yet joined --> must join in %d sec]",
                                       (p_order[26 + t] - level.framenum) / 10);
                        Q_strlcat(tmp, scratch, sizeof(tmp));
                    }
                } else
                    Q_strlcat(tmp, " [Not yet joined]", sizeof(tmp));
            }
        }

        gi.cprintf(ent, PRINT_HIGH, "%d. %s\n", t + 1, tmp);
    }

    gi.cprintf(ent, PRINT_HIGH, "\n");
}

// Wipe both teams back to the server's configured names. The green copy is
// rebuilt from the plain one by the same `+= 0x80` loop OSP_defaultTeam uses.
void OSP_teamReset(void)
{
    int         i;

    for (i = 0; i < 2; i++) {
        osp_teams[i].osp_m0f8 = 0;
        osp_teams[i].osp_m0f4 = 0;
        osp_teams[i].osp_m0f0 = 0;
        osp_teams[i].osp_m100 = 0;
        osp_teams[i].osp_m0fc = 0;
        osp_teams[i].osp_m104 = 0;
        osp_teams[i].osp_m108 = 0;
        osp_teams[i].joincode[0] = 0;
        osp_teams[i].osp_m124 = 0;
    }

    Q_strlcpy(osp_teams[0].netname, team_a_name->string, 16);
    Q_strlcpy(osp_teams[0].greenname, team_a_name->string, 16);
    for (i = 0; i < strlen(osp_teams[0].greenname); i++)
        osp_teams[0].greenname[i] += 128;

    Q_strlcpy(osp_teams[1].netname, team_b_name->string, 16);
    Q_strlcpy(osp_teams[1].greenname, team_b_name->string, 16);
    for (i = 0; i < strlen(osp_teams[1].greenname); i++)
        osp_teams[1].greenname[i] += 128;

    // The names a match is played under are the first thing the stats
    // file needs, and a reset is where they are decided -- a report that only
    // learns them from a later RENAME cannot label the match that was not
    // renamed.
    OSP_Stats_TeamName(osp_teams[0].netname);
    OSP_Stats_TeamName(osp_teams[1].netname);

    if (G_Ruleset() == RULESET_TDM) {
        gi.cvar_set("Score_A", "WARMUP");
        gi.cvar_set("Score_B", "WARMUP");
    }
}

// End of match: announce the result, stamp osp_teams[].osp_m124 with 1 = won,
// 2 = lost, 4 = tied, and print the two-line team summary. In 1v1 the loser
// goes to the back of the queue instead.
void OSP_findTeamWinner(void)
{
    edict_t     *ent;
    int         winpct;
    int         loserpct;
    int         winner;
    int         lose;

    winner = 0;
    lose = 1;
    if (osp_teams[0].osp_m0f8 < osp_teams[1].osp_m0f8) {
        winner = 1;
        lose = 0;
    }

    if (osp_teams[winner].osp_m0f8 < 1)
        winpct = 0;
    else if (!osp_teams[winner].osp_m0fc ||
             !(osp_teams[winner].osp_m0fc + osp_teams[winner].osp_m0f8))
        winpct = 100;
    else
        winpct = osp_teams[winner].osp_m0f8 * 100 /
                 (osp_teams[winner].osp_m0fc + osp_teams[winner].osp_m0f8);

    if (osp_teams[lose].osp_m0f8 < 1)
        loserpct = 0;
    else if (!osp_teams[lose].osp_m0fc ||
             !(osp_teams[lose].osp_m0fc + osp_teams[lose].osp_m0f8))
        loserpct = 100;
    else
        loserpct = osp_teams[lose].osp_m0f8 * 100 /
                   (osp_teams[lose].osp_m0fc + osp_teams[lose].osp_m0f8);

    if (osp_teams[winner].osp_m0f8 > osp_teams[lose].osp_m0f8) {
        osp_teams[winner].osp_m124 = 1;
        osp_teams[lose].osp_m124 = 2;
        gi.bprintf(PRINT_HIGH, "\n\n%s defeats %s: %d - %d\n\n",
                   osp_teams[winner].netname, osp_teams[lose].netname,
                   osp_teams[winner].osp_m0f8, osp_teams[lose].osp_m0f8);
    } else {
        osp_teams[winner].osp_m124 = 4;
        osp_teams[lose].osp_m124 = 4;
        gi.bprintf(PRINT_HIGH, "\n\nTied match! (%d to %d)\n\n",
                   osp_teams[winner].osp_m0f8, osp_teams[lose].osp_m0f8);
    }

    if (G_Ruleset() == RULESET_TDM) {
        gi.bprintf(PRINT_HIGH, "Frt: Fratricides          F  S  E\n");
        gi.bprintf(PRINT_HIGH, "EK : Enemy Kills       E  r  u  f\n");
        gi.bprintf(PRINT_HIGH, " S : Score         S   K  t  i  f\n");
        gi.bprintf(PRINT_HIGH, "====================================\n");
        gi.bprintf(PRINT_HIGH, "%-16s %3d %3d %2d %2d %d%%\n",
                   osp_teams[winner].netname, osp_teams[winner].osp_m0f8,
                   osp_teams[winner].osp_m100, osp_teams[winner].osp_m104,
                   osp_teams[winner].osp_m108, winpct);
        gi.bprintf(PRINT_HIGH, "%-16s %3d %3d %2d %2d %d%%\n\n",
                   osp_teams[lose].netname, osp_teams[lose].osp_m0f8,
                   osp_teams[lose].osp_m100, osp_teams[lose].osp_m104,
                   osp_teams[lose].osp_m108, loserpct);
        return;
    }

    gi.bprintf(PRINT_HIGH, "Sui: Suicides            S  E\n");
    gi.bprintf(PRINT_HIGH, " EK: Enemy Kills      E  u  f\n");
    gi.bprintf(PRINT_HIGH, "  S: Score        S   K  i  f\n");
    gi.bprintf(PRINT_HIGH, "================================\n");
    gi.bprintf(PRINT_HIGH, "%-15s %3d %3d %2d %d%%\n",
               osp_teams[winner].netname, osp_teams[winner].osp_m0f8,
               osp_teams[winner].osp_m100, osp_teams[winner].osp_m108, winpct);
    gi.bprintf(PRINT_HIGH, "%-15s %3d %3d %2d %d%%\n\n",
               osp_teams[lose].netname, osp_teams[lose].osp_m0f8,
               osp_teams[lose].osp_m100, osp_teams[lose].osp_m108, loserpct);

    for (winpct = 1; winpct <= game.maxclients; winpct++) {
        ent = g_edicts + winpct;
        if (!ent->inuse || !ent->client)
            continue;

        if (ent->client->resp.team == lose &&
            ent->client->resp.osp_entered == ENTERED_ENTERED) {
            OSP_1v1Remove(ent, 2);
            return;
        }
    }
}

// A tied match: mode 1 is sudden death straight away, mode 2 always adds time,
// anything else adds time until `count` reaches team_overtime_count and then
// falls back to sudden death. `frag_offset` is what makes the next frag win,
// and `osp_suddendeath` what says that it is sudden death -- set here, where
// the arm is chosen, rather than inferred from a period that added no time,
// which a runtime `team_overtime_time 0` makes of an ordinary period.
bool OSP_overtimeWork(int count)
{
    if (!(int)team_overtime_mode->value)
        return false;

    if ((int)team_overtime_mode->value == 1) {
        frag_offset = osp_teams[0].osp_m0f8 + 1;
        osp_suddendeath = true;
        gi.bprintf(PRINT_HIGH, "Tied match!! Sudden Death mode in effect!!!\n");
        return true;
    }

    // A period of no time cannot be played: the clock is still past the limit
    // on the next frame, which would add another, and another.  The clamp to 1
    // runs at InitGame and a config reload, and an operator's `set` afterwards
    // goes round it, so such a tie is sudden death and is announced as one.
    if ((int)team_overtime_time->value < 1) {
        frag_offset = osp_teams[0].osp_m0f8 + 1;
        osp_suddendeath = true;
        gi.bprintf(PRINT_HIGH, "Tied match!! Sudden Death mode now in effect!!!\n");
        return true;
    }

    if ((int)team_overtime_mode->value == 2) {
        overtime_timer += (int)team_overtime_time->value;
        gi.bprintf(PRINT_HIGH, "Tied match!! %d minutes added to time!\n",
                   (int)team_overtime_time->value);

        if ((int)team_overtime_time->value >= 1)
            start_count = 3;
        if ((int)team_overtime_time->value >= 5)
            start_count = 1;
        if ((int)team_overtime_time->value >= 10)
            start_count = 0;

        return true;
    }

    if (count >= (int)team_overtime_count->value) {
        frag_offset = osp_teams[0].osp_m0f8 + 1;
        osp_suddendeath = true;
        gi.bprintf(PRINT_HIGH, "Tied match!! Sudden Death mode now in effect!!!\n");
        return true;
    }

    overtime_timer += (int)team_overtime_time->value;
    gi.bprintf(PRINT_HIGH, "Tied match!! %d minutes added to time!\n",
               (int)team_overtime_time->value);

    if ((int)team_overtime_time->value >= 1)
        start_count = 3;
    if ((int)team_overtime_time->value >= 5)
        start_count = 1;
    if ((int)team_overtime_time->value >= 10)
        start_count = 0;

    return true;
}

// The two per-team damage switches, asked by name so that g_combat.c does not
// need osp_team_t.  Both are set by the team menu and by a referee, which is
// why they are per team rather than a server cvar: in a tourney match one side
// can be practising with friendly fire on while the other is not.
bool OSP_teamFriendlyFire(int team)
{
    if (team < 0 || team >= (int)q_countof(osp_teams))
        return false;
    return osp_teams[team].osp_m11c != 0;
}

bool OSP_teamSelfDamage(int team)
{
    if (team < 0 || team >= (int)q_countof(osp_teams))
        return false;
    return osp_teams[team].osp_m120 != 0;
}

// A team's display name, asked by index so that a shared file does not need
// osp_team_t.  Out-of-range answers "" rather than reading past the array: the
// only caller is the chase-cam banner, and a banner is not worth a crash.
const char *OSP_teamName(int team)
{
    if (team < 0 || team >= (int)q_countof(osp_teams))
        return "";
    return osp_teams[team].netname;
}

/*
=================
OSP_obituarySelf / OSP_obituaryFrag / OSP_obituaryDied

Tourney's ClientObituary, as the three shapes the donor's own has:
a death nobody else caused, a death an attacking client caused, and the
unnamed-cause fallback.  Each does the printing and the accounting for its
shape, because in the donor they are the same block and splitting them would
put half of a rule in a shared file.

What an earlier merge lost, and why it is one function per shape rather than
one `resp.score += delta` for all three.  A frag under tourney moves nine
numbers, not one:

    resp.score          the player's own, which is all the merge kept
    resp.osp_r014       deaths, the Deaths column and the first rank tie-break
    resp.osp_r2c0       suicides, the second tie-break
    resp.osp_r028       team kills, a team-scoreboard column
    resp.osp_r2dc = 2   "the attacker just hit the fraglimit", which pops the
                        scoreboard at intermission
    osp_teams[].osp_m0f8   the TEAM score -- what the team fraglimit, the
                        overtime test and sudden death all compare
    osp_teams[].osp_m0fc   team deaths, and the divisor of team efficiency
    osp_teams[].osp_m100   team frags        osp_teams[].osp_m104  team kills
    osp_teams[].osp_m108   team suicides

With the five team counters only ever zeroed, `tdm` and `duel` could not be
won: the team fraglimit never fired, every timed match drew 0-0 and went to
overtime, and sudden death could never resolve because the two totals could
never differ.

The three gates are the donor's, and they are not the same gate:

  * `sync_stat != 2` guards the PRINTING.  2 is the ten-second countdown, and a
    kill during it is not part of the match, so it is not announced.
  * `sync_stat > 2` guards the ACCOUNTING -- a match that is actually live.
  * `osp_suddendeath && teams differ` is checked by the CALLER (OSP_obituaryHush),
    because once sudden death has a winner the match is over and further
    obituaries would announce a match nobody is playing.

The printing is per client rather than one gi.bprintf, which is the other half
of what was lost: each of the two parties is sent the OTHER one's name in
`pers.greenname` -- the high-bit spelling Quake II draws in the alternate
charset -- and everyone else gets both names plain.  A bot is skipped
throughout: FL_BOT has no connection to unicast to.
=================
*/

// True when the obituary should be suppressed entirely: sudden death has
// already produced a winner and the rules row is about to end the level.
bool OSP_obituaryHush(void)
{
    // osp_suddendeath, not frag_offset: the offset is 0 for teams tied at
    // -1, which is a sudden death all the same.
    return osp_suddendeath && osp_teams[0].osp_m0f8 != osp_teams[1].osp_m0f8;
}

// The team half of a death charged to `self`.  Shared by the two shapes that
// have no attacker.
static void osp_selfDeathTeams(edict_t *self)
{
    int team = self->client->resp.team;

    if (!OSP_IsTeams() || team < 0 || team >= (int)q_countof(osp_teams))
        return;

    osp_teams[team].osp_m108++;
    osp_teams[team].osp_m0f8--;

    if (G_Ruleset() == RULESET_TDM)
        OSP_playerTeamFrags(self);

    // The donor's sudden-death bar, moved with a frag given back.  Nothing
    // reads it while sudden death runs -- OSP_CheckRules ends that the moment
    // the totals differ, by `osp_suddendeath` -- and it is 0 at every other
    // time, so this keeps the donor's arithmetic and decides nothing.
    if (frag_offset)
        frag_offset--;
}

// The bots' copy of an obituary (R-OSP-41).  Tourney unicasts its
// obituaries and skips every bot -- osp-tourney's FL_OSP_NOCMD is the bot SDK's
// FL_BOT bit, so the donor did too -- which left both botlibs, which learn of a
// death only by reading it off a bot's console, blind to every death under
// the four rulesets: no death or kill chats, and the Quake III bot's kill
// bookkeeping (its EV_OBITUARY) never ran.  Each bot gets the line ctf and
// arena broadcast, `victim message killer message2` with plain names and no
// teammate tag, because that is the line both botlibs' match templates read.
// Through gi.cprintf, which the bot layer turns into BotConsoleMessage for a
// bot and never puts on a wire, so no person sees anything new.
static void q_printf(1, 2) OSP_obituaryBots(const char *fmt, ...)
{
    char     line[MAX_STRING_CHARS];
    va_list  ap;
    edict_t *e;
    int      i;

    va_start(ap, fmt);
    Q_vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);

    for (i = 1; i <= game.maxclients; i++) {
        e = g_edicts + i;
        if (e->inuse && e->client && (e->flags & FL_BOT))
            gi.cprintf(e, PRINT_MEDIUM, "%s", line);
    }
}

void OSP_obituarySelf(edict_t *self, const char *message)
{
    edict_t *e;
    int      i;

    if (sync_stat != 2) {
        if (!(self->flags & FL_BOT))
            gi.cprintf(self, PRINT_MEDIUM, "%s %s.\n",
                       self->client->pers.greenname, message);

        for (i = 1; i <= game.maxclients; i++) {
            e = g_edicts + i;
            if (!e->inuse || !e->client)
                continue;
            if (e != self && !(e->flags & FL_BOT))
                gi.cprintf(e, PRINT_MEDIUM, "%s %s.\n",
                           self->client->pers.netname, message);
        }

        OSP_obituaryBots("%s %s.\n", self->client->pers.netname, message);
    }

    if (sync_stat > 2) {
        self->client->resp.score--;
        self->client->resp.osp_r2c0++;
        osp_selfDeathTeams(self);
        OSP_DoRankSort();
    }
}

void OSP_obituaryFrag(edict_t *self, edict_t *attacker, const char *message,
                      const char *message2, bool ff)
{
    edict_t *e;
    int      i, steam, ateam;

    if (sync_stat != 2) {
        const char *tag = ff ? "  ** Teammate Kill **\n" : "\n";

        if (!(self->flags & FL_BOT))
            gi.cprintf(self, PRINT_MEDIUM, "%s %s %s%s%s",
                       self->client->pers.netname, message,
                       attacker->client->pers.greenname, message2, tag);
        if (!(attacker->flags & FL_BOT))
            gi.cprintf(attacker, PRINT_MEDIUM, "%s %s %s%s%s",
                       self->client->pers.greenname, message,
                       attacker->client->pers.netname, message2, tag);

        for (i = 1; i <= game.maxclients; i++) {
            e = g_edicts + i;
            if (!e->inuse || !e->client)
                continue;
            if (e != self && e != attacker && !(e->flags & FL_BOT))
                gi.cprintf(e, PRINT_MEDIUM, "%s %s %s%s%s",
                           self->client->pers.netname, message,
                           attacker->client->pers.netname, message2, tag);
        }

        OSP_obituaryBots("%s %s %s%s\n", self->client->pers.netname, message,
                         attacker->client->pers.netname, message2);

        // Nobody unicasts to the console, so a dedicated server logs it here.
        if ((int)dedicated->value)
            gi.dprintf("%s %s %s%s\n", self->client->pers.netname, message,
                       attacker->client->pers.netname, message2);
    }

    if (sync_stat <= 2)
        return;

    steam = self->client->resp.team;
    ateam = attacker->client->resp.team;

    if (OSP_IsTeams() && steam >= 0 && steam < (int)q_countof(osp_teams) &&
        ateam >= 0 && ateam < (int)q_countof(osp_teams)) {
        if (ff) {
            attacker->client->resp.score--;
            attacker->client->resp.osp_r028++;
            osp_teams[steam].osp_m0fc++;
            osp_teams[ateam].osp_m0f8--;
            osp_teams[ateam].osp_m104++;
        } else {
            attacker->client->resp.score++;
            self->client->resp.osp_r014++;
            osp_teams[steam].osp_m0fc++;
            osp_teams[ateam].osp_m0f8++;
            osp_teams[ateam].osp_m100++;

            if ((int)fraglimit->value &&
                attacker->client->resp.score >= fraglimit->value)
                self->client->resp.osp_r2dc = 2;
        }

        if (G_Ruleset() == RULESET_TDM)
            OSP_playerTeamFrags(attacker);
    } else {
        attacker->client->resp.score++;
        self->client->resp.osp_r014++;

        if ((int)fraglimit->value &&
            attacker->client->resp.score >= fraglimit->value)
            self->client->resp.osp_r2dc = 2;
    }

    OSP_DoRankSort();
}

// The fallback: a death the message tables could not name.  Broadcast rather
// than unicast per client -- there is no second party to green -- and it does
// Not rank-sort, which is the donor's own asymmetry with OSP_obituarySelf.
void OSP_obituaryDied(edict_t *self)
{
    if (sync_stat != 2)
        gi.bprintf(PRINT_MEDIUM, "%s died.\n", self->client->pers.netname);

    if (sync_stat > 2) {
        self->client->resp.score--;
        self->client->resp.osp_r2c0++;
        osp_selfDeathTeams(self);
    }
}

// A team's frag total, asked by index for the same reason as the two switches
// above: the rules row should not need osp_team_t to compare two numbers.
int OSP_teamFrags(int team)
{
    if (team < 0 || team >= (int)q_countof(osp_teams))
        return 0;
    return osp_teams[team].osp_m0f8;
}
