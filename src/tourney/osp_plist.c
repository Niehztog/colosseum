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
// OSP Tourney DM v2.75, from osp-tourney@1d8427e.
// Donor-only: baseq2 has no counterpart, so it lives in src/tourney/ rather
// than being merged into a spine file.  The reconstruction's
// asm-matching address comments are stripped.
// osp_plist.c -- filename assigned by this tree.  players.txt: the allow/deny list and
// the ban commands' backing store.

#include "g_local.h"
#include "tourney/osp_types.h"

int num_names = 0;
char    pl_bname[200][16];
char    pl_names[200][16];
char    pl_pass[200][32];
char    pl_addr[200][16];

// `filename` is the third %s of the path, i.e. player_file's value; the caller
// passes it, this function does not read the cvar itself.
void OSP_loadPlayers(char *filename)
{
    char    pathbuf[MAX_OSPATH];
    FILE    *f = NULL;
    cvar_t  *pbmode;
    int     res;

    pbmode = gi.cvar("player_ban", "0", 0);
    num_names = 0;

    {
        if (!G_FsGamePath(pathbuf, sizeof(pathbuf), filename)) {
            gi.dprintf("\nPlayer list path too long, no players loaded.\n\n");
            return;
        }
        f = fopen(pathbuf, "r");
        if (f) {
            if (!(int)pbmode->value)
                gi.dprintf("\nLoading player DENY list from: \"%s\"\n", filename);
            else
                gi.dprintf("\nLoading player ALLOW list from: \"%s\"\n", filename);

            do {
                pl_names[num_names][0] = 0;
                pl_pass[num_names][0] = 0;
                pl_addr[num_names][0] = 0;

                res = read_player_entry(f, pl_names[num_names], pl_pass[num_names],
                                        pl_addr[num_names]);
                if (res == -1)
                    break;

                if (!res)
                    continue;

                if (pl_names[num_names][0]) {
                    gi.dprintf("%s", pl_names[num_names]);
                    if (pl_pass[num_names][0] && strcmp(pl_pass[num_names], "none"))
                        gi.dprintf(", (Pswd: %s)", pl_pass[num_names]);
                    if (pl_addr[num_names][0])
                        gi.dprintf(", (Addr: %s)", pl_addr[num_names]);
                    gi.dprintf("\n");
                } else if (pl_addr[num_names][0])
                    gi.dprintf("Address: %s\n", pl_addr[num_names]);

                pl_bname[num_names][0] = 0;
                num_names++;
            } while (num_names < 50);

            fclose(f);
            gi.dprintf("%d names/addresses found.\n\n", num_names);
        } else
            gi.dprintf("\n\"%s\" player list not found, no players loaded.\n\n", pathbuf);
    }
}

// -1 = end of file, 0 = comment or blank, 1 = name only, 2 = name + password +
// address. The documented line format is `<name> <tab> <password> <tab>
// <address>` and a comment is a line containing sixteen '#'.
int read_player_entry(FILE *f, char *name, char *pass, char *addr)
{
    char    line[1024];
    char    *scanp;
    char    *s;
    // FOUR pointers past the buffer: one aliasing `line` for the two strncpys
    // and the first strchr, and a second post-tab pointer for the address field.
    char    *token;                     // invented name
    char    *u;                     // invented name

    if (!fgets(line, 1024, f))
        return -1;

    if ((scanp = strchr(line, '\r')) != NULL)
        * scanp = 0;
    if ((scanp = strchr(line, '\n')) != NULL)
        * scanp = 0;

    if (!strlen(line))
        return 0;

    token = line;
    Q_strlcpy(name, token, 16);
    if (strstr(line, "################"))
        return 0;

    if ((scanp = strchr(token, '\t')) == NULL)
        return 1;
    *scanp = 0;
    scanp++;
    s = scanp;

    Q_strlcpy(name, token, 16);
    Q_strlcpy(pass, s, 32);

    if ((scanp = strchr(s, '\t')) == NULL)
        return 1;
    *scanp = 0;
    scanp++;
    u = scanp;

    Q_strlcpy(pass, s, 32);
    Q_strlcpy(addr, u, 16);
    return 2;
}

// 0 = no entry matched, 1 = name matched with no password/address constraint,
// 2 = name matched but the password/address did not, 3 = an address-only entry
// matched, -1 = that name is already on the server. Which of those admits the
// player is player_ban's business, not this function's.
// An entry is a whole address or a leading part of one, but a partial match
// has to stop on a dot: v2.75 used a plain prefix test, so banning "1.2.3.4"
// also banned 1.2.3.40 through 1.2.3.49.
bool OSP_addrMatch(const char *addr, const char *entry)
{
    size_t  len = strlen(entry);

    if (!len || strncmp(addr, entry, len))
        return false;

    return addr[len] == 0 || addr[len] == '.' || addr[len] == ':' ||
           entry[len - 1] == '.';
}

int OSP_playerAllow(char *name, char *userinfo)
{
    int     i;
    cvar_t  *bancv;
    char    plname[16];
    char    *value;
    edict_t *ent;
    int     retval;

    bancv = gi.cvar("player_ban", "0", 0);
    Q_strlcpy(plname, name, sizeof(plname));

    if ((int)bancv->value) {
        for (i = 1; i <= game.maxclients; i++) {
            ent = g_edicts + i;
            if (!ent->inuse || !ent->client || !ent->client->pers.connected)
                continue;
            if (!strncmp(plname, ent->client->pers.netname, 15))
                return -1;
        }
    }

    retval = (int)bancv->value;

    for (i = 0; i < num_names; i++) {
        if (pl_names[i][0] && !Q_stricmp(plname, pl_names[i])) {
            if (!pl_pass[i][0] && !pl_addr[i][0]) {
                if (!(int)bancv->value)
                    return 1;
                return 0;
            }

            value = Info_ValueForKey(userinfo, "password");
            if (pl_pass[i][0] && strcmp("none", pl_pass[i])) {
                if (!strcmp(value, pl_pass[i]))
                    return 0;
            }

            value = Info_ValueForKey(userinfo, "ip");
            if (pl_addr[i][0] && OSP_addrMatch(value, pl_addr[i]))
                return 0;

            retval = 2;
        } else if (!pl_names[i][0]) {
            value = Info_ValueForKey(userinfo, "ip");
            if (!pl_pass[i][0] && pl_addr[i][0] &&
                OSP_addrMatch(value, pl_addr[i])) {
                if (!(int)bancv->value)
                    return 3;
                return 0;
            }
        }
    }

    return retval;
}

// Either half may be NULL. -1 = the table is full, -2 = the name went in but
// the address did not.
int OSP_addBan(char *name, char *addr)
{
    int         i;
    bool    found;
    bool    added;

    added = false;

    if (name) {
        found = false;
        for (i = 0; i < num_names; i++) {
            if (!strcmp(pl_names[i], name)) {
                found = true;
                break;
            }
        }
        if (!found) {
            for (i = 0; i < num_names; i++)
                if (!pl_names[i][0] && !pl_pass[i][0] && !pl_addr[i][0])
                    break;
            if (i == 200)
                return -1;
            if (i == num_names)
                num_names++;
            Q_strlcpy(pl_names[i], name, sizeof(pl_names[i]));
            pl_pass[i][0] = 0;
            pl_addr[i][0] = 0;
            pl_bname[i][0] = 0;
            added = true;
        }
    }

    if (addr) {
        found = false;
        for (i = 0; i < num_names; i++) {
            if (!strcmp(pl_addr[i], addr)) {
                found = true;
                break;
            }
        }
        if (!found) {
            for (i = 0; i < num_names; i++)
                if (!pl_names[i][0] && !pl_pass[i][0] && !pl_addr[i][0])
                    break;
            if (i == 200) {
                if (added)
                    return -2;
                return -1;
            }
            if (i == num_names)
                num_names++;
            Q_strlcpy(pl_addr[i], addr, sizeof(pl_addr[i]));
            if (name)
                Q_strlcpy(pl_bname[i], name, sizeof(pl_bname[i]));
            else
                pl_bname[i][0] = 0;
            pl_names[i][0] = 0;
            pl_pass[i][0] = 0;
            added = true;
        }
    }

    return added;
}

bool OSP_removeBan(char *name, char *addr)
{
    // `t` doubles as the trim loop's done-flag.  There is no cached
    // `num_names - 1` either -- it is recomputed at all three uses.
    int         t;
    bool    removed;

    removed = false;

    if (name) {
        for (t = 0; t < num_names; t++) {
            if (!strcmp(pl_names[t], name) || !strcmp(pl_bname[t], name)) {
                pl_names[t][0] = 0;
                pl_pass[t][0] = 0;
                pl_addr[t][0] = 0;
                pl_bname[t][0] = 0;
                removed = true;
            }
        }
    }

    if (addr) {
        for (t = 0; t < num_names; t++) {
            if (!strcmp(pl_addr[t], addr)) {
                pl_names[t][0] = 0;
                pl_pass[t][0] = 0;
                pl_addr[t][0] = 0;
                pl_bname[t][0] = 0;
                removed = true;
            }
        }
    }

    // trim the empty tail back off
    t = false;
    while (num_names && !t) {
        if (!pl_names[num_names - 1][0] && !pl_addr[num_names - 1][0])
            num_names = num_names - 1;
        else
            t = true;
    }

    return removed;
}

void OSP_listbans(edict_t *ent)
{
    char    text[128];
    int     i;
    int     count;

    count = 0;
    for (i = 0; i < num_names; i++) {
        if (pl_names[i][0]) {
            Q_snprintf(text, sizeof(text), "Player: %s", pl_names[i]);
            if (pl_pass[i][0]) {
                Q_strlcat(text, ", [", sizeof(text));
                Q_strlcat(text, pl_pass[i], sizeof(text));
                Q_strlcat(text, "]", sizeof(text));
            }
            if (pl_addr[i][0]) {
                Q_strlcat(text, ", [", sizeof(text));
                Q_strlcat(text, pl_addr[i], sizeof(text));
                Q_strlcat(text, "]", sizeof(text));
            }
            count++;
            gi.cprintf(ent, PRINT_HIGH, "%s\n", text);
        }
        // Each arm prints for itself; there is no `else continue;`.
        else if (pl_addr[i][0]) {
            Q_snprintf(text, sizeof(text), "Address: %s", pl_addr[i]);
            if (pl_bname[i][0]) {
                Q_strlcat(text, ", [", sizeof(text));
                Q_strlcat(text, pl_bname[i], sizeof(text));
                Q_strlcat(text, "]", sizeof(text));
            }
            count++;
            gi.cprintf(ent, PRINT_HIGH, "%s\n", text);
        }
    }

    gi.cprintf(ent, PRINT_HIGH, "\n%d total ban entries.\n", count);
}

// ==========================================================================
// maps.txt -- the map queue
// ==========================================================================
