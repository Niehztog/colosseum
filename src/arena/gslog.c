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
#include "arena/net_compat.h"


FILE        *StdLogFile;

static fd_set   global_fds;

#ifdef _WIN32
/*
=================
GSNetStartup / GSNetShutdown

Winsock init for the netlog forwarding below.  RA2 did this inside the GameSpy
SDK's NetShutdown(); that SDK is gone, and netlog is the only socket user
left, so it lives here.
=================
*/
bool GSNetStartup(void)
{
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
        gi.dprintf("WS Error: %d\n", WSAGetLastError());
        return false;
    }

    return true;
}

void GSNetShutdown(void)
{
    WSACleanup();
}
#endif

struct sockaddr_in net_name_to_address(char *name)
{
    struct sockaddr_in  sin;
    struct hostent      *hp;
    char            *s, *portstr;
    unsigned long       a;

    memset(&sin, 0, sizeof(sin));

    s = strdup(name);
    strtok(s, ":");
    portstr = strtok(NULL, "");

    if (portstr) {
        int port = atoi(portstr);

        if (port <= 0 || port > 65535) {
            fprintf(stderr, "net_name_to_address: %s: invalid port number\n", portstr);
            free(s);
            return sin;
        }

        sin.sin_port = port;
    } else
        sin.sin_port = 0;

    a = inet_addr(s);
    if (a == INADDR_NONE) {
        hp = gethostbyname(s);
        if (hp)
            sin.sin_addr.s_addr = *(unsigned long *)hp->h_addr_list[0];
        else {
#ifdef _WIN32
            fprintf(stderr, "%s: %d", s, WSAGetLastError());
#else
            fprintf(stderr, "%s: %s", s, "net_name_to_addr");
#endif
            free(s);
            return sin;
        }
    }

    sin.sin_family = AF_INET;
    sin.sin_port = htons(sin.sin_port);
    free(s);

    return sin;
}

void net_send(int sock, char *buf, int len)
{
    int r;

    r = send(sock, buf, len, 0);
    if (r != len) {
        perror("send");
        if (errno)
            exit(1);
    }
}

int net_open_socket(void)
{
    int sock;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
#ifdef _WIN32
        printf("WSA %d\n", WSAGetLastError());
#else
        perror("socket");
#endif
        exit(1);
    }

    FD_SET(sock, &global_fds);

    return sock;
}

void net_close_socket(int sock)
{
    if (sock) {
        if (close(sock) < 0) {
            perror("close");
            exit(1);
        }
    }

    FD_CLR(sock, &global_fds);
}

void net_connect_socket(int sock, struct sockaddr_in *addr, unsigned short port)
{
    addr->sin_port = htons(port);

    if (connect(sock, (struct sockaddr *)addr, sizeof(*addr)) < 0) {
        perror("connect");
        exit(1);
    }
}

void GSSendLine(char *line)
{
    struct sockaddr_in  addr;
    int         sock;
    unsigned short      port;

    addr = net_name_to_address(netlog->string);
    if (!addr.sin_addr.s_addr)
        return;

    port = ntohs(addr.sin_port);

    sock = net_open_socket();
    net_connect_socket(sock, &addr, port);
    net_send(sock, line, strlen(line) + 1);
    net_close_socket(sock);
}

void GSOpenLog(void)
{
    cvar_t  *gamedir, *logname;
    char    path[80];

    gamedir = gi.cvar("game", "", CVAR_LATCH);
    logname = gi.cvar("logname", "stdlog.log", 0);

    strcpy(path, gamedir->string);
#ifdef _WIN32
    strcat(path, "\\");
#else
    strcat(path, "/");
#endif
    strcat(path, logname->string);

    StdLogFile = fopen(path, "a+t");
}

void GSCloseLog(void)
{
    fclose(StdLogFile);
}

void GSLogShutdown(void)
{
    if (logfile->value != 2)
        return;

    GSOpenLog();

    fprintf(StdLogFile, "\t\tGameEnd\t\t\t%d\n", (int)level.time);

    GSCloseLog();
}

void GSLogStartup(void)
{
    if (logfile->value != 2)
        return;

    GSOpenLog();

    fprintf(StdLogFile, "\t\tStdLog\t1.22\n");
    fprintf(StdLogFile, "\t\tPatchName\tRocket Arena 2 %s\n", "v2.25");

    GSCloseLog();
}

void GSLogNewmap(void)
{
    if (logfile->value != 2)
        return;

    GSOpenLog();

    fprintf(StdLogFile, "\t\tMAP\t%s\n", level.level_name);
    fprintf(StdLogFile, "\t\tGameStart\t\t\t%d\n", (int)level.time);

    GSCloseLog();
}

void GSdodeathlog(char *line)
{
    fprintf(StdLogFile, "%s", line);

    if (netlog->string[0])
        GSSendLine(line);
}

void GSLogDeath(edict_t *self, edict_t *inflictor, edict_t *attacker)
{
    char    line[1000];
    const gitem_t   *weap;
    char    *weapname;

    if (logfile->value != 2)
        return;

    GSOpenLog();

    if (attacker == self) {
        if (attacker->client->pers.weapon) {
            if (!strcmp(self->client->pers.weapon->classname, "weapon_grenadelauncher") ||
                !strcmp(self->client->pers.weapon->classname, "weapon_rocketlauncher") ||
                !strcmp(self->client->pers.weapon->classname, "weapon_bfg")) {
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

    fprintf(StdLogFile, "\t\tPlayerConnect\t%s\t\t%d\n",
            ent->client->pers.netname, (int)level.time);

    GSCloseLog();
}

void GSLogExit(edict_t *ent)
{
    if (logfile->value != 2)
        return;

    GSOpenLog();

    fprintf(StdLogFile, "\t\tPlayerLeft\t%s\t\t%d\n",
            ent->client->pers.netname, (int)level.time);

    GSCloseLog();
}
