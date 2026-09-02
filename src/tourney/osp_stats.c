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
// OSP Tourney DM v2.75, from osp-tourney@1d8427e (doc/provenance.md).
// Donor-only: baseq2 has no counterpart, so it lives in src/tourney/ rather
// than being merged into a spine file (R-CORE-7).  The reconstruction's
// asm-matching address comments are stripped -- SPECS.md N1 makes those oracles
// meaningless here, and they survive at the pin.
// osp_stats.c -- the local game-event / statistics log.  See osp_stats.h for
// what it replaces and why.

#include "g_local.h"
#include "tourney/osp_types.h"
#include "tourney/osp_stats.h"


cvar_t  *stats_logchat;
cvar_t  *stats_logallpickups;

static char     stats_path[MAX_OSPATH];
static FILE     *stats_f;

// The per-weapon accuracy slots, in ACC_* order, with the names the ngLog
// Player_Accuracy line used so an old parser needs only be repointed.
static const char *const acc_names[ACC_COUNT] = {
    "Blaster", "Shotgun", "Super Shotgun", "Machinegun", "Chaingun",
    "Grenade Launcher", "Rocket Launcher", "HyperBlaster", "Railgun",
    "BFG10K", "Grenades",
    // The content layers' nine, appended (R-MODE-3, R-181).  A parser reads
    // the eleven above unchanged and gains these only when they are used.
    "Ionripper", "Phalanx", "Trap",
    "ETF Rifle", "Prox Launcher", "Plasma Beam", "Chainfist", "Tesla",
    "Disruptor"
};

// The allow_* cvars and the names this log reports them under are
// osp_main.c's `osp_allow_items[]` since R-183.  There were three copies of
// that correspondence -- SpawnItem's inhibit, the scoreboard banner and this
// list -- and they had already drifted apart before either content layer made
// them wrong: two rows the inhibit honours are named by neither of the others.

/*
=================
json_string

Writes a JSON string literal, quotes included.  Player, team and map names
arrive straight off the network or out of a config file, so everything outside
printable ASCII is escaped -- tourney's own "green" names are ordinary text
with 0x80 added to every byte, and a raw control byte or a stray quote would
otherwise make the whole line unparseable.
=================
*/
static void json_string(FILE *f, const char *s)
{
    fputc('"', f);
    if (s) {
        for (; *s; s++) {
            byte c = *s;
            if (c == '"' || c == '\\')
                fprintf(f, "\\%c", c);
            else if (c >= 0x20 && c < 0x7f)
                fputc(c, f);
            else
                fprintf(f, "\\u%04x", c);
        }
    }
    fputc('"', f);
}

static void json_key_string(FILE *f, const char *key, const char *value)
{
    fprintf(f, ",\"%s\":", key);
    json_string(f, value);
}

/*
=================
begin_event

Opens the line and writes the fields every record carries: the event name, the
wall clock, and the level time the ngLog lines led with.  Returns NULL when
logging is off, which is what lets every entry point below be a no-op without
its caller testing a cvar.
=================
*/
static FILE *begin_event(const char *event)
{
    // stats_path is the "logging is on" flag; the handle is reopened if
    // something closed it, so that a shutdown/restart cycle inside one game
    // library load does not silently stop the log.
    if (!stats_f) {
        if (!stats_path[0])
            return NULL;
        stats_f = fopen(stats_path, "a");
        if (!stats_f) {
            stats_path[0] = 0;
            return NULL;
        }
    }

    fprintf(stats_f, "{\"event\":\"%s\",\"time\":%lld,\"t\":%.1f",
            event, (long long)time(NULL), level.time);
    return stats_f;
}

static void end_event(FILE *f)
{
    fputs("}\n", f);
    fflush(f);
}

// Every player record carries the same three fields, so that a consumer can
// join events without tracking name changes itself.
static void json_player(FILE *f, edict_t *ent)
{
    fprintf(f, ",\"id\":%d", ent->client->resp.clientid);
    json_key_string(f, "name", ent->client->pers.netname);
    fprintf(f, ",\"ping\":%d", ent->client->ping);
}

/*
=================
OSP_Stats_DateString

"YY.MM.DD.HH.MM".  This is ngLog_getDateInfo's short form, kept because the
admin log and the auto-record demo names are built from it.
=================
*/
void OSP_Stats_DateString(char *out, size_t size)
{
    time_t      t;
    struct tm   *tm;

    time(&t);
    tm = localtime(&t);

    if (!tm) {
        Q_strlcpy(out, "0.00.00.00.00", size);
        return;
    }

    Q_snprintf(out, size, "%d.%.2d.%.2d.%.2d.%.2d", tm->tm_year,
               tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min);
}

/*
=================
OSP_Stats_Init

Registers the cvars and opens the log.  Called once from OSP_gameInit, before
anything can log.
=================
*/
void OSP_Stats_Init(void)
{
    cvar_t  *port;
    cvar_t  *hostname;

    stats_logchat = gi.cvar("stats_logchat", "0", 0);
    stats_logallpickups = gi.cvar("stats_logallpickups", "0", 0);

    if (stats_f) {
        fclose(stats_f);
        stats_f = NULL;
    }
    stats_path[0] = 0;

    if (!(int)g_statsfile->value || !g_statsname->string[0]) {
        gi.dprintf("Local stats logging disabled.\n");
        return;
    }


    // R-COMPAT-6: composed by g_fs.c, which owns the two cvars' defaults.
    if (!G_FsGamePath(stats_path, sizeof(stats_path), g_statsname->string)) {
        gi.dprintf("Stats log path too long, logging disabled.\n");
        stats_path[0] = 0;
        return;
    }

    stats_f = fopen(stats_path, "a");
    if (!stats_f) {
        gi.dprintf("Couldn't open stats log \"%s\", logging disabled.\n",
                   stats_path);
        stats_path[0] = 0;
        return;
    }

    gi.dprintf("Stats log for server is \"%s\".\n", stats_path);

    port = gi.cvar("port", "27910", CVAR_NOSET);
    hostname = gi.cvar("hostname", "", CVAR_SERVERINFO);

    fprintf(stats_f, "{\"event\":\"init\",\"time\":%lld,\"version\":%d",
            (long long)time(NULL), OSP_STATS_VERSION);
    json_key_string(stats_f, "game", "OSP Tourney DM");
    json_key_string(stats_f, "patch", "tourney");
    json_key_string(stats_f, "patch_version", "2.75");
    json_key_string(stats_f, "author", "Orange Smoothie Productions");
    json_key_string(stats_f, "hostname", hostname->string);
    fprintf(stats_f, ",\"port\":%d", (int)port->value);
    end_event(stats_f);
}

/*
=================
OSP_Stats_Shutdown
=================
*/
void OSP_Stats_Shutdown(const char *reason)
{
    FILE    *f;

    f = begin_event("shutdown");
    if (f) {
        json_key_string(f, "reason", reason);
        end_event(f);
    }

    if (stats_f) {
        fclose(stats_f);
        stats_f = NULL;
    }
}

/*
=================
OSP_Stats_GameInit

The settings snapshot: everything the ngLog "0.0 info <key> <value>" header
block carried, as one object.  The ngLog stack closed and reopened its file
here so that whatever consumed it saw one complete game per file; appending to
one line-oriented log needs no such thing, and this record is what delimits a
game in it.
=================
*/
void OSP_Stats_GameInit(void)
{
    FILE    *f;
    int     i;
    bool    first;

    f = begin_event("game_init");
    if (!f)
        return;

    json_key_string(f, "map", level.mapname);
    json_key_string(f, "level_name", level.level_name);
    // The record field keeps its 1999 name; the cvar behind it is gone and
    // `match_type` is derived from the ruleset now (R-OSP-12).
    json_key_string(f, "match_mode", match_type->string);
    fprintf(f, ",\"timelimit\":%d,\"fraglimit\":%d,\"dmflags\":%d",
            (int)timelimit->value, (int)fraglimit->value,
            (int)dmflags->value);
    fprintf(f, ",\"cheats\":%d,\"maxclients\":%d",
            (int)sv_cheats->value, (int)game.maxclients);
    fprintf(f, ",\"hook\":%s", (int)hook_enable->value ? "true" : "false");
    fprintf(f, ",\"runes\":%d", rune_stat);
    fprintf(f, ",\"respawn_protection\":%d", (int)client_protect->value);
    fprintf(f, ",\"railgun_damage\":%d", (int)damage_railgun->value);

    if (OSP_IsTeams()) {
        fprintf(f, ",\"teams\":[");
        json_string(f, osp_teams[0].netname);
        fputc(',', f);
        json_string(f, osp_teams[1].netname);
        fputc(']', f);
        fprintf(f, ",\"team_hurtteam\":%s",
                (int)team_hurtteam->value ? "true" : "false");
        fprintf(f, ",\"team_hurtself\":%s",
                (int)team_hurtself->value ? "true" : "false");
    }

    fprintf(f, ",\"disabled_items\":[");
    for (i = 0, first = true; osp_allow_items[i].cvar; i++) {
        if (!osp_allow_items[i].logname)
            continue;   // inhibited but deliberately not named -- R-183
        if ((int)gi.cvar(osp_allow_items[i].cvar, "1", 0)->value)
            continue;
        if (!first)
            fputc(',', f);
        json_string(f, osp_allow_items[i].logname);
        first = false;
    }
    fputc(']', f);

    end_event(f);
}

/*
=================
OSP_Stats_MatchStart
=================
*/
void OSP_Stats_MatchStart(void)
{
    FILE    *f = begin_event("match_start");

    if (f)
        end_event(f);
}

/*
=================
OSP_Stats_MatchEnd
=================
*/
void OSP_Stats_MatchEnd(const char *reason)
{
    FILE    *f = begin_event("match_end");

    if (!f)
        return;

    json_key_string(f, "reason", reason);

    if (OSP_IsTeams()) {
        fprintf(f, ",\"team_scores\":[%d,%d]",
                osp_teams[0].osp_m0f8, osp_teams[1].osp_m0f8);
    }

    end_event(f);
}

/*
=================
OSP_Stats_PlayerConnect
=================
*/
void OSP_Stats_PlayerConnect(edict_t *ent)
{
    FILE    *f = begin_event("connect");

    if (!f)
        return;

    json_player(f, ent);
    json_key_string(f, "addr", (ent->flags & FL_BOT) ? "" : ent->osp_e37c);
    fprintf(f, ",\"bot\":%s", (ent->flags & FL_BOT) ? "true" : "false");
    end_event(f);
}

/*
=================
OSP_Stats_PlayerReconnect
=================
*/
void OSP_Stats_PlayerReconnect(edict_t *ent)
{
    FILE    *f = begin_event("reconnect");

    if (!f)
        return;

    json_player(f, ent);
    json_key_string(f, "addr", (ent->flags & FL_BOT) ? "" : ent->osp_e37c);
    fprintf(f, ",\"bot\":%s", (ent->flags & FL_BOT) ? "true" : "false");
    end_event(f);
}

/*
=================
OSP_Stats_PlayerEnter
=================
*/
void OSP_Stats_PlayerEnter(edict_t *ent)
{
    FILE    *f;

    if (!ent->client->resp.osp_r2a8) {
        ent->client->resp.osp_r2a8 = 1;
        OSP_Stats_PlayerConnect(ent);
    }

    f = begin_event("enter");
    if (!f)
        return;

    json_player(f, ent);
    end_event(f);
}

/*
=================
OSP_Stats_PlayerLeave
=================
*/
void OSP_Stats_PlayerLeave(edict_t *ent)
{
    FILE    *f;

    if (!ent->client->resp.osp_r2a8) {
        ent->client->resp.osp_r2a8 = 1;
        OSP_Stats_PlayerConnect(ent);
    }

    f = begin_event("leave");
    if (!f)
        return;

    json_player(f, ent);
    end_event(f);
}

/*
=================
OSP_Stats_PlayerRename
=================
*/
void OSP_Stats_PlayerRename(edict_t *ent, const char *oldname)
{
    FILE    *f = begin_event("rename");

    if (!f)
        return;

    json_player(f, ent);
    json_key_string(f, "old_name", oldname);
    end_event(f);
}

/*
=================
OSP_Stats_PlayerRespawn
=================
*/
void OSP_Stats_PlayerRespawn(edict_t *ent)
{
    FILE    *f = begin_event("respawn");

    if (!f)
        return;

    json_player(f, ent);
    end_event(f);
}

/*
=================
OSP_Stats_PlayerMode

"Observe", "Chasecam" or "Autocam".
=================
*/
void OSP_Stats_PlayerMode(edict_t *ent, const char *mode)
{
    FILE    *f = begin_event("mode");

    if (!f)
        return;

    json_player(f, ent);
    json_key_string(f, "mode", mode);
    end_event(f);
}

/*
=================
OSP_Stats_Chat
=================
*/
void OSP_Stats_Chat(const char *text)
{
    FILE    *f;

    if (!(int)stats_logchat->value)
        return;

    f = begin_event("chat");
    if (!f)
        return;

    json_key_string(f, "text", text);
    end_event(f);
}

/*
=================
OSP_Stats_BotDetect
=================
*/
void OSP_Stats_BotDetect(edict_t *ent, const char *why)
{
    FILE    *f = begin_event("bot_detect");

    if (!f)
        return;

    json_player(f, ent);
    json_key_string(f, "addr", ent->osp_e37c);
    json_key_string(f, "reason", why ? why : "");
    end_event(f);
}

/*
=================
OSP_Stats_TeamName
=================
*/
void OSP_Stats_TeamName(const char *name)
{
    FILE    *f = begin_event("team_name");

    if (!f)
        return;

    json_key_string(f, "name", name);
    end_event(f);
}

/*
=================
OSP_Stats_TeamRename
=================
*/
void OSP_Stats_TeamRename(const char *oldname, const char *newname)
{
    FILE    *f = begin_event("team_rename");

    if (!f)
        return;

    json_key_string(f, "old_name", oldname);
    json_key_string(f, "name", newname);
    end_event(f);
}

/*
=================
OSP_Stats_TeamJoin
=================
*/
void OSP_Stats_TeamJoin(edict_t *ent)
{
    FILE    *f = begin_event("team_join");
    int     team;

    if (!f)
        return;

    team = ent->client->resp.team;
    json_player(f, ent);
    json_key_string(f, "team", OSP_teamNameFor(team));
    end_event(f);
}

/*
=================
OSP_Stats_TeamLeave
=================
*/
void OSP_Stats_TeamLeave(edict_t *ent)
{
    FILE    *f = begin_event("team_leave");
    int     team;

    if (!f)
        return;

    team = ent->client->resp.team;
    json_player(f, ent);
    json_key_string(f, "team", OSP_teamNameFor(team));
    end_event(f);
}

/*
=================
OSP_Stats_Vote

`result` is "Propose", "Pass" or "Fail"; `what` and `value` may both be NULL,
which is what a plain failure logs.
=================
*/
void OSP_Stats_Vote(const char *result, const char *what, const char *value)
{
    FILE    *f = begin_event("vote");

    if (!f)
        return;

    json_key_string(f, "result", result);
    if (what)
        json_key_string(f, "what", what);
    if (value)
        json_key_string(f, "value", value);
    end_event(f);
}

/*
=================
OSP_Stats_ItemPickup
=================
*/
void OSP_Stats_ItemPickup(const char *name, int entnum, edict_t *ent)
{
    FILE    *f = begin_event("item_pickup");

    if (!f)
        return;

    json_key_string(f, "item", name);
    fprintf(f, ",\"entity\":%d", entnum);
    json_player(f, ent);
    end_event(f);
}

/*
=================
OSP_statsPickupMinor

The cheap half of the item log.  `stats_logallpickups` lives in this file, so
the dozen call sites in g_items.c and p_weapon.c ask through this rather than
each carrying a copy of the test; the ruleset gate stays visible at the call
site, where R-VER-25 wants it.

The split between "minor" and the plain OSP_Stats_ItemPickup is the donor's own
and it is a judgement about what a match report is for: a quad, an
invulnerability, a mega health, body armour, power armour and a weapon are
events somebody replays a match to look at, and shells are not.
=================
*/
void OSP_statsPickupMinor(const char *name, edict_t *ent)
{
    if (!stats_logallpickups || !(int)stats_logallpickups->value)
        return;
    OSP_Stats_ItemPickup(name, 0, ent);
}

/*
=================
OSP_Stats_ItemUse
=================
*/
void OSP_Stats_ItemUse(const char *name, edict_t *ent)
{
    FILE    *f = begin_event("item_use");

    if (!f)
        return;

    json_key_string(f, "item", name);
    fprintf(f, ",\"entity\":%d", ent->client->resp.osp_r200);
    json_player(f, ent);
    end_event(f);
}

/*
=================
OSP_Stats_ItemExpire

`ent` is NULL when the item timed out with nobody holding it.
=================
*/
void OSP_Stats_ItemExpire(const char *name, edict_t *ent, int entnum)
{
    FILE    *f = begin_event("item_expire");

    if (!f)
        return;

    json_key_string(f, "item", name);
    fprintf(f, ",\"entity\":%d", entnum);
    if (ent && ent->client)
        json_player(f, ent);
    end_event(f);
}

/*
=================
OSP_Stats_ItemDrop
=================
*/
void OSP_Stats_ItemDrop(const char *name, int entnum, edict_t *ent)
{
    FILE    *f = begin_event("item_drop");

    if (!f)
        return;

    json_key_string(f, "item", name);
    fprintf(f, ",\"entity\":%d,\"from\":%d",
            entnum, ent->client->resp.osp_r200);
    json_player(f, ent);
    end_event(f);
}

/*
=================
means_of_death_name

The weapon or hazard the kill is attributed to.  `self_inflicted` comes back
true for the causes that score against the victim no matter who set them off,
which is what the ngLog stack spelled as a separate Suicide line.
=================
*/
static const char *means_of_death_name(int mod, bool *self_inflicted)
{
    *self_inflicted = true;

    switch (mod) {
    case MOD_FALLING:           return "Fell";
    case MOD_CRUSH:             return "Crushed";
    // R-183: HERE, in the SELF-INFLICTED half, and not below with the weapons.
    // dm_ball.c raises MOD_DBALL_CRUSH from `T_Damage(other, ent, ent, ...)`
    // where `ent` is the ball, so there is no attacker to credit; it scores
    // against the victim like every cause around it.  stdlog.c has the same
    // row in the same half, for the same reason.
    case MOD_DBALL_CRUSH:       return "DBall";
    case MOD_WATER:             return "Drowned";
    case MOD_SLIME:             return "Melted";
    case MOD_LAVA:              return "Lava";
    case MOD_BOMB:              return "Bomb";
    case MOD_EXPLOSIVE:         return "Explosive";
    case MOD_BARREL:            return "Barrel";
    case MOD_TARGET_LASER:      return "Lasered";
    case MOD_TARGET_BLASTER:    return "Blasted";
    case MOD_SPLASH:            return "Splash";
    case MOD_TRIGGER_HURT:      return "Trigger_Hurt";
    case MOD_EXIT:              return "Exit";
    case MOD_SUICIDE:           return "Couldnt_Take_It_Anymore";
    }

    *self_inflicted = false;

    switch (mod) {
    case MOD_BLASTER:           return "Blaster";
    case MOD_SHOTGUN:           return "Shotgun";
    case MOD_SSHOTGUN:          return "Super Shotgun";
    case MOD_MACHINEGUN:        return "Machinegun";
    case MOD_CHAINGUN:          return "Chaingun";
    case MOD_GRENADE:
    case MOD_G_SPLASH:          return "Grenade Launcher";
    case MOD_HANDGRENADE:
    case MOD_HG_SPLASH:
    case MOD_HELD_GRENADE:      return "Grenades";
    case MOD_ROCKET:
    case MOD_R_SPLASH:          return "Rocket Launcher";
    case MOD_HYPERBLASTER:      return "HyperBlaster";
    case MOD_RAILGUN:           return "Railgun";
    case MOD_BFG_LASER:
    case MOD_BFG_BLAST:
    case MOD_BFG_EFFECT:        return "BFG10K";
    case MOD_TELEFRAG:          return "Telefrag";
    case MOD_GRAPPLE:           return "Hook";

    // R-183: the content layers, which R-MODE-3 makes valid with every ruleset
    // -- so a Reckoning or Ground Zero kill was reaching this table and falling
    // out of it as "Unknown".  The names are the items' own pickup names, which
    // is what `acc_names[]` in the accuracy record already uses, so a consumer
    // joining the two records matches on one spelling.
    //
    // The Disruptor's two MODs share a name the way the BFG's three do.  The
    // monsters' own -- MOD_BRAINTENTACLE, MOD_BLASTOFF, MOD_GEKK, MOD_BLASTER2
    // -- are deliberately absent: the donor names no monster attack either, and
    // a monster is not a weapon somebody chose.
    // Xatrix
    case MOD_RIPPER:            return "Ionripper";
    case MOD_PHALANX:           return "Phalanx";
    case MOD_TRAP:              return "Trap";
    // Ground Zero
    case MOD_ETF_RIFLE:         return "ETF Rifle";
    case MOD_PROX:              return "Prox Launcher";
    case MOD_HEATBEAM:          return "Plasma Beam";
    case MOD_CHAINFIST:         return "Chainfist";
    case MOD_TESLA:             return "Tesla";
    case MOD_TRACKER:
    case MOD_DISINTEGRATOR:     return "Disruptor";
    case MOD_NUKE:              return "A-M Bomb";
    // ...and Ground Zero's items that kill, which are as attributable as any
    // weapon: a player carried them and chose the moment.
    case MOD_VENGEANCE_SPHERE:  return "Vengeance Sphere";
    case MOD_HUNTER_SPHERE:     return "Hunter Sphere";
    case MOD_DEFENDER_SPHERE:   return "Defender Sphere";
    case MOD_DOPPLE_EXPLODE:
    case MOD_DOPPLE_VENGEANCE:
    case MOD_DOPPLE_HUNTER:     return "Doppleganger";
    }

    return "Unknown";
}

/*
=================
OSP_Stats_Death

One record per death: a "suicide" when the victim killed themselves or the
world did it, a "kill" otherwise.  A team kill is flagged rather than given
its own event name, so a consumer counting kills does not have to know about
tourney's Fratricide line.
=================
*/
void OSP_Stats_Death(edict_t *self, edict_t *inflictor, edict_t *attacker)
{
    FILE        *f;
    int         mod;
    bool        self_inflicted;
    const char  *weapon;
    bool        friendly;

    if (!self->client)
        return;

    mod = meansOfDeath & ~MOD_FRIENDLY_FIRE;
    weapon = means_of_death_name(mod, &self_inflicted);

    // a player who killed themselves with a weapon in hand is logged with
    // that weapon, the way the ngLog Suicide line was
    if (attacker == self && !self_inflicted) {
        weapon = self->client->pers.weapon ?
                 self->client->pers.weapon->pickup_name : "none";
        self_inflicted = true;
    }

    if (self_inflicted || !attacker || !attacker->client) {
        f = begin_event("suicide");
        if (!f)
            return;
        json_player(f, self);
        json_key_string(f, "weapon", weapon);
        fprintf(f, ",\"score\":%d", -1);
        end_event(f);
        return;
    }

    friendly = (OSP_IsTeams() && (meansOfDeath & MOD_FRIENDLY_FIRE)) != 0;

    f = begin_event("kill");
    if (!f)
        return;

    fprintf(f, ",\"attacker\":{\"id\":%d", attacker->client->resp.clientid);
    json_key_string(f, "name", attacker->client->pers.netname);
    fprintf(f, ",\"ping\":%d}", attacker->client->ping);

    fprintf(f, ",\"victim\":{\"id\":%d", self->client->resp.clientid);
    json_key_string(f, "name", self->client->pers.netname);
    fprintf(f, ",\"ping\":%d", self->client->ping);
    json_key_string(f, "weapon", self->client->pers.weapon ?
                    self->client->pers.weapon->pickup_name : "UNKNOWN");
    fputc('}', f);

    json_key_string(f, "weapon", weapon);
    fprintf(f, ",\"score\":%d", friendly ? -1 : 1);
    fprintf(f, ",\"friendly_fire\":%s", friendly ? "true" : "false");
    end_event(f);
}

/*
=================
OSP_Stats_Accuracy

One record per player, holding every weapon they fired or were hit by, plus
the two damage totals.  Nothing is written for a player who did neither.
=================
*/
void OSP_Stats_Accuracy(edict_t *ent)
{
    FILE            *f;
    const p_acc_t   *acc;
    int             cid;
    int             i;
    bool            any;

    if (!ent->client)
        return;

    cid = ent->client->resp.clientid;
    if (cid < 0 || cid >= q_countof(p_acc))
        return;

    acc = &p_acc[cid];

    for (i = 0, any = false; i < ACC_COUNT; i++) {
        if (acc->shots[i] || acc->taken[i] || acc->given[i]) {
            any = true;
            break;
        }
    }

    if (!any)
        return;

    f = begin_event("accuracy");
    if (!f)
        return;

    json_player(f, ent);
    fprintf(f, ",\"weapons\":{");

    for (i = 0, any = false; i < ACC_COUNT; i++) {
        if (!acc->shots[i] && !acc->taken[i] && !acc->given[i])
            continue;
        if (any)
            fputc(',', f);
        json_string(f, acc_names[i]);
        fprintf(f, ":{\"hits\":%d,\"shots\":%d,\"given\":%d,\"taken\":%d}",
                acc->hits[i], acc->shots[i], acc->given[i], acc->taken[i]);
        any = true;
    }

    fprintf(f, "},\"damage_given\":%d,\"damage_taken\":%d",
            acc->dgiven, acc->dtaken);
    end_event(f);
}

/*
=================
OSP_Stats_AccuracyAll
=================
*/
void OSP_Stats_AccuracyAll(void)
{
    edict_t *ent;
    int     i;

    for (i = 1; i <= game.maxclients; i++) {
        ent = g_edicts + i;
        if (!ent->inuse || !ent->client)
            continue;
        OSP_Stats_Accuracy(ent);
    }
}
