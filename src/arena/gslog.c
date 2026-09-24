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
#include "g_local.h"


FILE        *StdLogFile;

/*
=================
netlog, and why there is no socket here

RA2 forwarded every kill, connect and disconnect line to a remote host over UDP
-- `netlog <host:port>`, one datagram per event, through its own copies of
gethostbyname/socket/connect/send.  The game library is allowed exactly one
outbound process and no outbound network at all, so the forwarding is gone: the
seven net_* helpers, GSSendLine, the winsock startup pair and net_compat.h with
them.  Three things went with it that were worth losing on their own terms --
`net_open_socket`, `net_close_socket` and `net_connect_socket` each called
`exit(1)` on failure, from inside a game library, which turns a DNS hiccup on
the operator's log host into a dead server.

The LOCAL log is untouched: `logfile 2` still writes every line to
`<gamedir>/<logname>`, which is what every RA2 log parser reads anyway.  The
`netlog` cvar itself stays registered so a legacy name keeps resolving, and
InitGame says once that setting it does nothing.
=================
*/

void GSOpenLog(void)
{
    cvar_t  *logname;
    char    path[MAX_QPATH * 2];

    logname = gi.cvar("logname", "stdlog.log", 0);

    // Same correction as ra2stats: the donor composed "<gamedir>/<name>", which
    // is relative to the server's working directory rather than to the
    // installation.
    if (!G_FsGamePath(path, sizeof(path), logname->string)) {
        StdLogFile = NULL;
        return;
    }

    StdLogFile = fopen(path, "a+t");
}

void GSCloseLog(void)
{
    // The donor called fclose(NULL) whenever the open had failed, which is
    // undefined and on glibc is a null dereference.  It could not fail for the
    // donor because its path was always creatable; it can here.
    if (StdLogFile) {
        fclose(StdLogFile);
        StdLogFile = NULL;
    }
}

void GSLogShutdown(void)
{
    if (logfile->value != 2)
        return;

    GSOpenLog();

    if (!StdLogFile)
        return;

    fprintf(StdLogFile, "\t\tGameEnd\t\t\t%d\n", (int)level.time);

    GSCloseLog();
}

void GSLogStartup(void)
{
    if (logfile->value != 2)
        return;

    GSOpenLog();

    if (!StdLogFile)
        return;

    fprintf(StdLogFile, "\t\tStdLog\t1.22\n");
    fprintf(StdLogFile, "\t\tPatchName\tRocket Arena 2 %s\n", "v2.25");

    GSCloseLog();
}

void GSLogNewmap(void)
{
    if (logfile->value != 2)
        return;

    GSOpenLog();

    if (!StdLogFile)
        return;

    fprintf(StdLogFile, "\t\tMAP\t%s\n", level.level_name);
    fprintf(StdLogFile, "\t\tGameStart\t\t\t%d\n", (int)level.time);

    GSCloseLog();
}

void GSdodeathlog(char *line)
{
    if (StdLogFile)
        fprintf(StdLogFile, "%s", line);
}

/*
==================
splash_weapon

Can a player kill themselves with this weapon's own blast?  RA2 asks the question
by listing three classnames inline, and names the weapon in the suicide record
only for those three -- so a suicide with anything else is logged with an empty
weapon field.

The mission packs bring five more weapons that can do it, and the list had
none of them, so a Phalanx or Prox Launcher suicide went into the round log
unattributed.  Named here rather than inline because the answer is a property of
the weapon and the caller already has three ways to reach the same record.

The TRAP and the TESLA are here although they are thrown rather than fired: both
damage whatever is nearby including the thrower, which is the only thing this
predicate is asking, and both carry IT_WEAPON so both can be `pers.weapon`.

The A-M bomb is not, and checking why is what removed it: `ammo_nuke` is
IT_POWERUP in the itemlist, with no IT_WEAPON bit, so it is a use-item and never
becomes `pers.weapon`.  A row for it would have read plausibly and matched
nothing -- the same dead-branch shape as item 5's `ionrippergun`, arrived at from
the other direction.
==================
*/
static bool splash_weapon(const char *classname)
{
    static const char *const splash[] = {
        "weapon_grenadelauncher",   // RA2's three
        "weapon_rocketlauncher",
        "weapon_bfg",
        "weapon_phalanx",           // Xatrix
        "ammo_trap",
        "weapon_proxlauncher",      // Ground Zero
        "ammo_tesla",
    };
    int i;

    for (i = 0; i < q_countof(splash); i++)
        if (!strcmp(classname, splash[i]))
            return true;

    return false;
}

void GSLogDeath(edict_t *self, edict_t *inflictor, edict_t *attacker)
{
    char    line[1000];
    const gitem_t   *weap;
    char    *weapname;

    if (logfile->value != 2)
        return;

    GSOpenLog();
    if (!StdLogFile)
        return;

    if (attacker == self) {
        if (attacker->client->pers.weapon) {
            if (splash_weapon(self->client->pers.weapon->classname)) {
                Q_snprintf(line, sizeof(line), "%s\t\tSuicide\t%s\t-1\t%d\t%d\n",
                           self->client->pers.netname, self->client->pers.weapon->pickup_name,
                           (int)level.time, self->client->ping);
                GSdodeathlog(line);
                GSCloseLog();
                return;
            }

            Q_snprintf(line, sizeof(line), "%s\t\tSuicide\t\t-1\t%d\t%d\n",
                       self->client->pers.netname, (int)level.time, self->client->ping);
            GSdodeathlog(line);
            GSCloseLog();
            return;
        }

        Q_snprintf(line, sizeof(line), "%s\t\tSuicide\t\t-1\t%d\t%d\n",
                   self->client->pers.netname, (int)level.time, self->client->ping);
        GSdodeathlog(line);
        GSCloseLog();
        return;
    }

    if (attacker && attacker->client) {
        weap = attacker->client->pers.weapon;
        weapname = weap ? weap->pickup_name : "BFG10K";

        Q_snprintf(line, sizeof(line), "%s\t%s\tKill\t%s\t1\t%d\t%d\n",
                   attacker->client->pers.netname, self->client->pers.netname,
                   weapname, (int)level.time, attacker->client->ping);
        GSdodeathlog(line);
        GSCloseLog();
        return;
    }

    Q_snprintf(line, sizeof(line), "%s\t\tSuicide\t\t-1\t%d\t%d\n",
               self->client->pers.netname, (int)level.time, self->client->ping);
    GSdodeathlog(line);
    GSCloseLog();
}

void GSLogEnter(edict_t *ent)
{
    if (logfile->value != 2)
        return;

    GSOpenLog();

    if (!StdLogFile)
        return;

    fprintf(StdLogFile, "\t\tPlayerConnect\t%s\t\t%d\n",
            ent->client->pers.netname, (int)level.time);

    GSCloseLog();
}

void GSLogExit(edict_t *ent)
{
    if (logfile->value != 2)
        return;

    GSOpenLog();

    if (!StdLogFile)
        return;

    fprintf(StdLogFile, "\t\tPlayerLeft\t%s\t\t%d\n",
            ent->client->pers.netname, (int)level.time);

    GSCloseLog();
}
