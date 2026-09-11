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
// osp_config.c -- filename assigned by this tree.  The serverconfigs.txt loader and its
// three lookup helpers.

#include "g_local.h"
#include "tourney/osp_types.h"
#include "bot/bl_main.h"
#include "bot/bl_botcfg.h"
// The two bot entry points, from the headers that own them.  These were once
// re-declared here, which is the shape to avoid: a donor's name declared
// outside the header that declares it.  The seam file they resolved to is
// gone.
#include "bot/bl_redirgi.h"
#include "bot/bl_spawn.h"

// The alternate server configs, loaded from configs.txt: conf_name[i] is the
// .cfg filename and conf_info[i] an optional friendly description.  Either one
// may be used to name a config in a vote, which is what OSP_configExists is
// resolving.
// Read the server-config list (serverconfigs.txt unless vote_config_list says
// otherwise) and fill conf_name/conf_info from it.  A line is
// "<file>\t<description>"; the description is optional.  A blank line, a
// comment and a line naming a file that is not there are all skipped WITHOUT
// consuming a table slot -- that is what the i-- before each continue is.
void OSP_configLoad(void)
{
    char    path[MAX_OSPATH];
    char    line[1024];
    int     i;
    FILE    *f = NULL;
    // DECLARATION INITIALISERS, not statements: gcc creates a temp while
    // expanding an initialiser and it lands between the variable it
    // initialises and the next declaration, which is what puts the pooled
    // "serverconfigs.txt" address between `list` and `cdefault` in real's
    // frame.  Written as plain assignments the temp comes after every
    // declared local instead, and the three slots rotate.
    cvar_t  *list = gi.cvar("vote_config_list", "serverconfigs.txt", 0);
    cvar_t  *cdefault = gi.cvar("vote_config_default", "0", 0);
    cvar_t  *cdefname = gi.cvar("vote_config_defaultname", "default", 0);
    conf_size = 0;

    {
        if (!G_FsGamePath(path, sizeof(path),
                          list ? list->string : "serverconfigs.txt")) {
            gi.dprintf("Server config list path is too long. No configs loaded.\n\n");
            gi.cvar_set("vote_enable_config", "0");
            return;
        }

        f = fopen(path, "r");
        if (f) {
            for (i = 0; i < 32; i++) {
                // `p` is FUNCTION-scope in the original.
                char    *p;

                if (!fgets(line, 1024, f))
                    break;

                line[1023] = 0;
                if ((p = strchr(line, '\r')))
                    * p = 0;
                if ((p = strchr(line, '\n')))
                    * p = 0;
                if ((p = strchr(line, '#')))
                    * p = 0;

                // A positive `if` around the whole remainder with `i--` as its
                // `else`.
                if (strlen(line) > 1) {
                    conf_info[i][0] = 0;
                    if ((p = strchr(line, '\t'))) {
                        *p = 0;
                        p++;
                        Q_strlcpy(conf_info[i], p, sizeof(conf_info[i]));
                    }

                    if (!G_FsGamePath(path, sizeof(path), line)) {
                        gi.dprintf("Server config path is too long, skipping \"%s\".\n",
                                   line);
                        i--;
                    } else if (OSP_configFileExists(path)) {
                        Q_strlcpy(conf_name[i], line, sizeof(conf_name[i]));
                    } else {
                        i--;
                    }
                } else
                    i--;
            }

            fclose(f);
            conf_size = i > 0 ? i : 0;

            if (!conf_size) {
                gi.dprintf("No server configs found.\n\n");
                gi.cvar_set("vote_enable_config", "0");
            } else {
                gi.dprintf("%d server configs found:\n", conf_size);

                for (i = 0; i < conf_size; i++) {
                    if (conf_info[i][0])
                        gi.dprintf("- %s [%s]\n", conf_info[i], conf_name[i]);
                    else
                        gi.dprintf("- [%s]\n", conf_name[i]);
                }

                if ((int)cdefault->value && cdefname->string &&
                    strcmp(cdefname->string, "default")) {
                    if (!G_FsGamePath(path, sizeof(path), cdefname->string)) {
                        gi.dprintf("** Default config path is too long!\n");
                        gi.dprintf("** No default config will be used.\n");
                        gi.cvar_set("vote_config_default", "0");
                        gi.cvar_set("vote_config_defaultname", "default");
                    } else if (OSP_configFileExists(path)) {
                        gi.dprintf("** Default config is: %s\n",
                                   cdefname->string);
                    } else {
                        gi.dprintf("** Default config \"%s\" not found!\n",
                                   cdefname->string);
                        gi.dprintf("** No default config will be used.\n");
                        gi.cvar_set("vote_config_default", "0");
                        gi.cvar_set("vote_config_defaultname", "default");
                    }
                } else
                    gi.dprintf("** No default config will be used.\n");

                gi.dprintf("\n");
            }
        } else {
            gi.dprintf("\n\"%s\" server config list not found. No configs loaded.\n\n",
                       path);
            gi.cvar_set("vote_enable_config", "0");
        }
    }
}

void OSP_configList(edict_t *ent)
{
    int     i;

    if (!conf_size) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Sorry, no alternate server configs available.\n");
        return;
    }

    gi.cprintf(ent, PRINT_HIGH, "\nAvailable alternate server configs:\n");
    for (i = 0; i < conf_size; i++) {
        if (conf_info[i][0])
            gi.cprintf(ent, PRINT_HIGH, "  %s [%s]\n", conf_info[i],
                       conf_name[i]);
        else
            gi.cprintf(ent, PRINT_HIGH, "  %s\n", conf_name[i]);
    }
    gi.cprintf(ent, PRINT_HIGH, "\n");
}

// `ent` NULL means "this is a vote, not a client command": the description is
// then rewritten in place to the real filename, and the complaint goes to the
// console instead of to a player.
bool OSP_configExists(edict_t *ent, char *name)
{
    int     i;

    for (i = 0; i < conf_size; i++) {
        // Both arms normalise, and the name arm is the one that matters.
        // The match is case-INSENSITIVE and the caller then hands `name`
        // straight to `exec`, so the donor -- which rewrites `name` on the
        // description arm only -- execs the spelling the CLIENT typed rather
        // than the one the operator listed.
        //
        // The exposure is narrower than it looks, and the reason is worth
        // writing down because it is what a first draft of the check got
        // wrong: Q2PRO retries a mixed-case path in lower case on non-Windows
        // (`common/files.c`, PATH_MIXED_CASE), so an all-lower-case config file
        // resolves whatever the client shouted.  What the engine cannot do is
        // re-capitalise a request.  So the case that bites is a
        // `serverconfigs.txt` naming a MIXED-CASE file: a vote spelled any
        // other way then execs a path that does not exist, the map still
        // changes, and the configuration is silently never applied.  Measured
        // in `scenarios/ospmapchange` row 6, which needed a mixed-case fixture
        // before it could tell the two builds apart at all.
        //
        // `name` is `vote_value` and both are [64], so the bound is the
        // destination's as well as the source's.  Still gated on `!ent`: with a
        // client the caller passed `gi.argv()`, which is the engine's and is
        // not ours to write.
        if (!Q_stricmp(name, conf_name[i])) {
            if (!ent)
                Q_strlcpy(name, conf_name[i], sizeof(conf_name[i]));
            return true;
        }

        if (conf_info[i][0] && !Q_stricmp(name, conf_info[i])) {
            if (!ent)
                Q_strlcpy(name, conf_name[i], sizeof(conf_name[i]));
            return true;
        }
    }

    if (ent)
        gi.cprintf(ent, PRINT_HIGH, "\"%s\" is not a valid server config.\n",
                   name);
    else
        gi.dprintf("(vote) Invalid \"%s\" server config specified.\n", name);
    return false;
}

bool OSP_configFileExists(char *path)
{
    FILE        *f;

    f = fopen(path, "r");
    if (!f)
        return false;
    fclose(f);
    return true;
}
