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
// osp_clientcmd.c -- OSP Tourney DM's client commands, as a DELEGATED
// dispatcher rather than 137 arms in g_cmds.c.
//
// Threewave's twelve commands are arms in ClientCommand's chain, each gated
// `G_Ruleset() == RULESET_CTF && Q_stricmp(...)`, and that reads fine at twelve.
// Tourney has 137.  Writing them out would triple g_cmds.c and put a donor's
// entire command surface in a spine file, which is the wrong place for it.
//
// So ClientCommand asks once, under one gate: `OSP_ClientCommand(ent)` returns
// true if it handled the command and false if it did not, and the shared chain
// carries on.  The body below is the donor's own chain, with three changes:
// the tail returns false instead of falling through to chat (the caller owns
// that), Cmd_Say_f gained RA2's third argument, and the bot-command hook is
// added.

#include "g_local.h"
#include "tourney/osp_types.h"
#include "tourney/osp_stats.h"
#include "bot/bl_main.h"
#include "bot/bl_botcfg.h"

bool OSP_ClientCommand(edict_t *ent)
{
    char        *cmdstr;
    const gitem_t   *it;

    if (!ent->client)
        return false;

    cmdstr = gi.argv(0);
    if (!Q_stricmp(cmdstr, "say")) {
        Cmd_Say_f(ent, false, false, false);
        return true;
    }
    if (!Q_stricmp(cmdstr, "score")) {
        Cmd_Score_f(ent);
        return true;
    }
    if (!Q_stricmp(cmdstr, "help")) {
        Cmd_Help_f(ent);
        return true;
    }
    if (!Q_stricmp(cmdstr, "hook") || !Q_stricmp(cmdstr, "hookon")) {
        OSP_hookon_cmd(ent);
        return true;
    }
    if (!Q_stricmp(cmdstr, "unhook") || !Q_stricmp(cmdstr, "hookoff")) {
        OSP_hookoff_cmd(ent);
        return true;
    }
    if (!Q_stricmp(cmdstr, "players")) {
        Cmd_Players_f(ent);
        return true;
    }
    if (!Q_stricmp(cmdstr, "talkto") || !Q_stricmp(cmdstr, "talk") ||
        !Q_stricmp(cmdstr, "tell")) {
        OSP_talkto_cmd(ent);
        return true;
    }
    if (G_Ruleset() == RULESET_TDM && (!Q_stricmp(cmdstr, "say_team") ||
                        !Q_stricmp(cmdstr, "steam"))) {
        OSP_sayteam_cmd(ent, gi.args());
        return true;
    }
    if (!Q_stricmp(cmdstr, "say_team")) {
        Cmd_Say_f(ent, true, false, false);
        return true;
    }
    if (!Q_stricmp(cmdstr, "accuracy") || !Q_stricmp(cmdstr, "stats")) {
        OSP_accuracy_cmd(ent);
        return true;
    }
    if (!Q_stricmp(cmdstr, "oldaccuracy") || !Q_stricmp(cmdstr, "oldstats") ||
        !Q_stricmp(cmdstr, "laststats")) {
        OSP_oldaccuracy_cmd(ent);
        return true;
    }
    if (ent->osp_e39c != 1 && (!Q_stricmp(cmdstr, "referee") ||
                               !Q_stricmp(cmdstr, "admin") || !Q_stricmp(cmdstr, "ref"))) {
        OSP_referee_cmd(ent);
        return true;
    }
    if (!Q_stricmp(cmdstr, "joincode")) {
        OSP_joincode_cmd(ent);
        return true;
    }
    if (OSP_IsMatch() && !Q_stricmp(cmdstr, "_is_referee")) {
        OSP_isreferee_cmd(ent);
        return true;
    }
    if (OSP_IsTeams() && !Q_stricmp(cmdstr, "_default_team_info")) {
        OSP_defaultteam_cmd(ent);
        return true;
    }
    if (G_Ruleset() == RULESET_TDM && !Q_stricmp(cmdstr, "_default_join_code")) {
        OSP_defaultjoincode_cmd(ent);
        return true;
    }
    if (bot_watch && !Q_stricmp(cmdstr, "_init_state")) {
        OSP_speedCheat_cmd(ent);
        return true;
    }

    if ((!match_paused || ent->client->menu_owner == MENU_TOURNEY) && !Q_stricmp(cmdstr, "invnext")) {
        SelectNextItem(ent, -1);
        return true;
    }
    if ((!match_paused || ent->client->menu_owner == MENU_TOURNEY) && !Q_stricmp(cmdstr, "invprev")) {
        SelectPrevItem(ent, -1);
        return true;
    }
    if (!Q_stricmp(cmdstr, "invuse")) {
        Cmd_InvUse_f(ent);
        return true;
    }
    if (level.intermission_framenum)
        return true;

    if ((!match_paused || ent->client->menu_owner == MENU_TOURNEY) && !Q_stricmp(cmdstr, "use"))
        Cmd_Use_f(ent);
    else if (!Q_stricmp(cmdstr, "invnextw") && !match_paused)
        SelectNextItem(ent, IT_WEAPON);
    else if (!Q_stricmp(cmdstr, "invprevw") && !match_paused)
        SelectPrevItem(ent, IT_WEAPON);
    else if (!Q_stricmp(cmdstr, "invdrop"))
        Cmd_InvDrop_f(ent);
    else if (!Q_stricmp(cmdstr, "weapprev") && !match_paused)
        Cmd_WeapPrev_f(ent);
    else if (!Q_stricmp(cmdstr, "weapnext") && !match_paused)
        Cmd_WeapNext_f(ent);
    else if (!Q_stricmp(cmdstr, "weaplast"))
        Cmd_WeapLast_f(ent);
    else if (!Q_stricmp(cmdstr, "kill") && !match_paused)
        Cmd_Kill_f(ent);
    else if (!Q_stricmp(cmdstr, "putaway"))
        Cmd_PutAway_f(ent);
    else if (!Q_stricmp(cmdstr, "wave"))
        Cmd_Wave_f(ent);
    else if (!Q_stricmp(cmdstr, "invnextp"))
        SelectNextItem(ent, IT_POWERUP);
    else if (!Q_stricmp(cmdstr, "invprevp"))
        SelectPrevItem(ent, IT_POWERUP);
    else if (!Q_stricmp(cmdstr, "god"))
        Cmd_God_f(ent);
    else if (!Q_stricmp(cmdstr, "notarget"))
        Cmd_Notarget_f(ent);
    else if (!Q_stricmp(cmdstr, "noclip"))
        Cmd_Noclip_f(ent);
    else if (!Q_stricmp(cmdstr, "drop"))
        Cmd_Drop_f(ent);
    else if (!Q_stricmp(cmdstr, "give"))
        Cmd_Give_f(ent);
    else if (!Q_stricmp(cmdstr, "id"))
        OSP_id_cmd(ent);
    else if (!Q_stricmp(cmdstr, "motd"))
        OSP_motd_cmd(ent);
    else if (!Q_stricmp(cmdstr, "hud") || !Q_stricmp(cmdstr, "display"))
        OSP_hud_cmd(ent);
    else if (G_Ruleset() == RULESET_DM && (!Q_stricmp(cmdstr, "highscores") ||
                             !Q_stricmp(cmdstr, "highscore") || !Q_stricmp(cmdstr, "hiscores") ||
                             !Q_stricmp(cmdstr, "hiscore")))
        OSP_highscores_cmd(ent);
    else if (!(int)match_strictmode->value && !Q_stricmp(cmdstr, "ready"))
        OSP_ready_cmd(ent, false);
    else if (!(int)match_strictmode->value && (!Q_stricmp(cmdstr, "notready") ||
             !Q_stricmp(cmdstr, "unready") || !Q_stricmp(cmdstr, "noready")))
        OSP_notready_cmd(ent, false);
    else if ((!(int)match_strictmode->value ||
              ent->client->resp.osp_entered != ENTERED_ENTERED) &&
             (!Q_stricmp(cmdstr, "chasecam") || !Q_stricmp(cmdstr, "chase")))
        OSP_ChaseCam(ent);
    else if ((!(int)match_strictmode->value ||
              ent->client->resp.osp_entered != ENTERED_ENTERED) &&
             (!Q_stricmp(cmdstr, "observer") || !Q_stricmp(cmdstr, "observe")))
        OSP_startObserve(ent);
    else if ((!(int)match_strictmode->value ||
              ent->client->resp.osp_entered != ENTERED_ENTERED) && !Q_stricmp(cmdstr, "autocam"))
        CameraCmd(ent, true);
    else if ((!(int)match_strictmode->value ||
              ent->client->resp.osp_entered != ENTERED_ENTERED) &&
             (!Q_stricmp(cmdstr, "menu") || !Q_stricmp(cmdstr, "ctfmenu")))
        Cmd_Inven_f(ent);
    else if ((!(int)match_strictmode->value ||
              ent->client->resp.osp_entered != ENTERED_ENTERED) && !Q_stricmp(cmdstr, "inven"))
        Cmd_Inven_f(ent);
    else if (!Q_stricmp(cmdstr, "matchinfo"))
        OSP_showinfo_cmd(ent);
    else if (!Q_stricmp(cmdstr, "vote"))
        OSP_vote_cmd(ent, 0, 0, NULL, NULL);
    else if (!Q_stricmp(cmdstr, "yes"))
        OSP_yes_cmd(ent);
    else if (!Q_stricmp(cmdstr, "no"))
        OSP_no_cmd(ent);
    else if (!OSP_IsTeams() && (!Q_stricmp(cmdstr, "join") || !Q_stricmp(cmdstr, "joingame")))
        OSP_ffajoin_cmd(ent);
    else if (!Q_stricmp(cmdstr, "oldscores") || !Q_stricmp(cmdstr, "oldscore") ||
             !Q_stricmp(cmdstr, "lastscores") || !Q_stricmp(cmdstr, "lastscore"))
        OSP_oldscores_cmd(ent);
    else if (!Q_stricmp(cmdstr, "ignore") || !Q_stricmp(cmdstr, "mute") ||
             !Q_stricmp(cmdstr, "muzzle") || !Q_stricmp(cmdstr, "filter"))
        OSP_muzzle_cmd(ent);
    else if (!Q_stricmp(cmdstr, "droptech") || !Q_stricmp(cmdstr, "droprune")) {
        it = OSP_What_Rune(ent);
        if (it)
            it->drop(ent, it);
        return true;
    } else if (OSP_IsTeams() && !Q_stricmp(cmdstr, "teamname"))
        OSP_teamname_cmd(ent);
    else if (OSP_IsTeams() && !Q_stricmp(cmdstr, "teamskin"))
        OSP_teamskin_cmd(ent);
    else if (OSP_IsTeams() && (!Q_stricmp(cmdstr, "join") ||
                            !Q_stricmp(cmdstr, "jointeam") || !Q_stricmp(cmdstr, "team")))
        OSP_teamjoin_cmd(ent, NULL);
    else if (G_Ruleset() == RULESET_TDM && (!Q_stricmp(cmdstr, "invite") ||
                             !Q_stricmp(cmdstr, "pick") || !Q_stricmp(cmdstr, "pickplayer")))
        OSP_teaminvite_cmd(ent);
    else if (G_Ruleset() == RULESET_TDM && (!Q_stricmp(cmdstr, "switchteam") ||
                             !Q_stricmp(cmdstr, "switchteams")))
        OSP_switchteam_cmd(ent);
    else if (G_Ruleset() == RULESET_TDM && (!Q_stricmp(cmdstr, "lockteam") ||
                             !Q_stricmp(cmdstr, "teamlock") || !Q_stricmp(cmdstr, "lock")))
        OSP_lockteam_cmd(ent);
    else if (G_Ruleset() == RULESET_TDM && (!Q_stricmp(cmdstr, "unlockteam") ||
                             !Q_stricmp(cmdstr, "teamunlock") || !Q_stricmp(cmdstr, "unlock")))
        OSP_unlockteam_cmd(ent);
    else if (G_Ruleset() == RULESET_TDM && !(int)match_strictmode->value &&
             (!Q_stricmp(cmdstr, "readyteam") || !Q_stricmp(cmdstr, "teamready") ||
              !Q_stricmp(cmdstr, "teamallready")))
        OSP_readyteam_cmd(ent);
    else if (G_Ruleset() == RULESET_TDM && !(int)match_strictmode->value &&
             (!Q_stricmp(cmdstr, "notreadyteam") || !Q_stricmp(cmdstr, "unreadyteam") ||
              !Q_stricmp(cmdstr, "noreadyteam") || !Q_stricmp(cmdstr, "teamnotready")))
        OSP_notreadyteam_cmd(ent);
    else if (G_Ruleset() == RULESET_TDM && (!Q_stricmp(cmdstr, "captain") ||
                             !Q_stricmp(cmdstr, "leader") || !Q_stricmp(cmdstr, "teamcaptain")))
        OSP_captain_cmd(ent);
    else if (G_Ruleset() == RULESET_TDM && !Q_stricmp(cmdstr, "captains"))
        OSP_captains_cmd(ent);
    else if (G_Ruleset() == RULESET_TDM && (!Q_stricmp(cmdstr, "kickplayer") ||
                             !Q_stricmp(cmdstr, "remove") || !Q_stricmp(cmdstr, "removeplayer")))
        OSP_kickplayer_cmd(ent);
    else if (OSP_IsTeams() && (!Q_stricmp(cmdstr, "time") ||
                            !Q_stricmp(cmdstr, "matchpause") || !Q_stricmp(cmdstr, "timeout") ||
                            !Q_stricmp(cmdstr, "timein")))
        OSP_playertime_cmd(ent);
    else if (G_Ruleset() == RULESET_DUEL && (!Q_stricmp(cmdstr, "queue") ||
                             !Q_stricmp(cmdstr, "line") || !Q_stricmp(cmdstr, "order")))
        OSP_1v1queue_cmd(ent);
    else if (ent->osp_e39c) {
        if (!Q_stricmp(cmdstr, "r_help"))
            OSP_rhelp_cmd(ent);
        else if (!Q_stricmp(cmdstr, "r_kick"))
            OSP_rkick_cmd(ent);
        else if (!Q_stricmp(cmdstr, "r_mpause"))
            OSP_rmpause_cmd();
        else if (!Q_stricmp(cmdstr, "r_map"))
            OSP_rmap_cmd(ent);
        else if (!Q_stricmp(cmdstr, "r_timelimit"))
            OSP_rtimelimit_cmd(ent);
        else if (!Q_stricmp(cmdstr, "r_fraglimit"))
            OSP_rfraglimit_cmd(ent);
        else if (!Q_stricmp(cmdstr, "r_allready") && OSP_IsMatch())
            OSP_allready_svcmd();
        else if (!Q_stricmp(cmdstr, "r_allnotready") && OSP_IsMatch())
            OSP_allnotready_svcmd(true);
        else if ((!Q_stricmp(cmdstr, "r_stopmatch") ||
                  !Q_stricmp(cmdstr, "r_endmatch")) && OSP_IsMatch())
            OSP_rstopmatch_cmd(ent);
        else if (!Q_stricmp(cmdstr, "r_players") ||
                 !Q_stricmp(cmdstr, "r_plist"))
            Cmd_Players_f(ent);
        else if (!Q_stricmp(cmdstr, "r_banlist") ||
                 !Q_stricmp(cmdstr, "r_listban") || !Q_stricmp(cmdstr, "r_blist"))
            OSP_rbanlist_cmd(ent);
        else if (!Q_stricmp(cmdstr, "r_ban"))
            OSP_rban_cmd(ent, NULL);
        else if (!Q_stricmp(cmdstr, "r_banaddr"))
            OSP_rbanaddr_cmd(ent);
        else if (!Q_stricmp(cmdstr, "r_unban"))
            OSP_runban_cmd(ent);
        else if (!Q_stricmp(cmdstr, "r_unbanaddr"))
            OSP_runbanaddr_cmd(ent);
        // A referee's unrecognised command still falls through to the bot
        // command table and then to chat -- the same tail as the outer chain,
        // written out twice.
        else
            return false;
    } else {
        return false;
    }

    return true;
}
