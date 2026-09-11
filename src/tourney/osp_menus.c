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
// than being merged into a spine file.  The reconstruction's asm-matching
// address comments are stripped.  osp_menus.c -- filename assigned by this
// tree.  The mod's pop-up menu callbacks.
//
// Thirteen osp_pmenu_t tables (id CTF's p_menu.c engine) and the callbacks behind
// their entries: an opener per menu, an `OSP_update*Menu` per menu that
// rebuilds the entry text for one client, and a leaf callback per selectable
// line.  OSP_IsTeams() picks team play from plain DM, which is what
// decides whether "back" goes to Team_Menu or RegDM_Menu.

#include "g_local.h"
#include "tourney/osp_types.h"

// Defined below its first use in the donor, whose g_local.h declared it.
void OSP_menuVotePercent(char *pct, size_t pctsize, char *out, size_t outsize);

#include "tourney/osp_stats.h"
#include "bot/bl_main.h"
#include "bot/bl_botcfg.h"
// The two bot entry points.
void BotServerCommand(char *str, ...);
void BotDestroy(edict_t *bot);

osp_pmenu_t Team_Menu[18] = {
    { "*OSP Tourney DeathMatch",                 osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "[ Teamplay Mode ]",                       osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "*Join Team A 1234567890",                 osp_PMENU_ALIGN_LEFT,    NULL, OSP_joinTeam_menu },
    { NULL,                                      osp_PMENU_ALIGN_RIGHT,   NULL, NULL },
    { "*Join Team B 1234567890",                 osp_PMENU_ALIGN_LEFT,    NULL, OSP_joinTeam_menu },
    { NULL,                                      osp_PMENU_ALIGN_RIGHT,   NULL, NULL },
    { "*Admin Menu",                             osp_PMENU_ALIGN_LEFT,    NULL, OSP_returnMainAdmin_menu },
    { "*Voting Menu",                            osp_PMENU_ALIGN_LEFT,    NULL, OSP_voteMenu },
    { "*Enter Observer Mode",                    osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeObserve },
    { "*Enter Chasecam Mode",                    osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeChase },
    { "*Player ID",                              osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*Change HUD Layout",                      osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeHUD },
    { "*Help",                                   osp_PMENU_ALIGN_LEFT,    NULL, OSP_helpMenu },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Use [ and ] to move cursor",              osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Press ENTER to select",                   osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "*v(2.75)",                                osp_PMENU_ALIGN_RIGHT,   NULL, NULL },
};
osp_pmenu_t RegDM_Menu[18] = {
    { "*OSP Tourney DeathMatch",                 osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "[ Regular DM Mode ]",                     osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*Enter the Game",                         osp_PMENU_ALIGN_LEFT,    NULL, OSP_dmReturn_menu },
    { "*Voting Menu",                            osp_PMENU_ALIGN_LEFT,    NULL, OSP_voteMenu },
    { "*Admin Menu",                             osp_PMENU_ALIGN_LEFT,    NULL, OSP_returnMainAdmin_menu },
    { "*Enter Observer Mode",                    osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeObserve },
    { "*Enter Chasecam Mode",                    osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeChase },
    { "*Change HUD Layout",                      osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeHUD },
    { "*Player ID",                              osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*Help",                                   osp_PMENU_ALIGN_LEFT,    NULL, OSP_helpMenu },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Use [ and ] to move cursor",              osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Press ENTER to select",                   osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "*v(2.75)",                                osp_PMENU_ALIGN_RIGHT,   NULL, NULL },
};
osp_pmenu_t AdminMain_Menu[17] = {
    { "*OSP Tourney DeathMatch",                 osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "[ Main Admin Menu ]",                     osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*Kick Player",                            osp_PMENU_ALIGN_LEFT,    NULL, OSP_adminSelectMenu },
    { "*Ban Player",                             osp_PMENU_ALIGN_LEFT,    NULL, OSP_adminSelectMenu },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*Change Map",                             osp_PMENU_ALIGN_LEFT,    NULL, OSP_adminSelectMenu },
    { "*Change Server Settings",                 osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*Match Controls",                         osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*Exit Admin Controls",                    osp_PMENU_ALIGN_LEFT,    NULL, OSP_returnMainTeam_menu },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Use [ and ] to move cursor",              osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Press ENTER to select",                   osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "*v(2.75)",                                osp_PMENU_ALIGN_RIGHT,   NULL, NULL },
};
osp_pmenu_t AdminSelect_Menu[17] = {
    { "*OSP Tourney DeathMatch",                 osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "[ Map Selection Menu 000 ]",              osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*Select new map to load:00",              osp_PMENU_ALIGN_LEFT,    NULL, OSP_mapAdminSelect_menu },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Mapname1234567890123456789",              osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*Load selected map",                      osp_PMENU_ALIGN_LEFT,    NULL, OSP_mapAdminChoose },
    { "*Return to main menu",                    osp_PMENU_ALIGN_LEFT,    NULL, OSP_returnMainAdmin_menu },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Use [ and ] to move cursor",              osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Press ENTER to select",                   osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "*v(2.75)",                                osp_PMENU_ALIGN_RIGHT,   NULL, NULL },
};
osp_pmenu_t Vote_Menu[19] = {
    { "*OSP Tourney DeathMatch",                 osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "[ Voting Menu ]",                         osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "Map: q2dm12345678901234",                 osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeMap_menu },
    { "Config: 1234567890123456789012345689012", osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeConfig_menu },
    { "Item toggles...",                         osp_PMENU_ALIGN_LEFT,    NULL, OSP_voteMenu2 },
    { "Gladiator Bots...",                       osp_PMENU_ALIGN_LEFT,    NULL, OSP_botMenu },
    { "TimeLimit: 9999",                         osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeTime_menu },
    { "FragLimit: 9999",                         osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeFrag_menu },
    { "The Hook: [DISABLED]",                    osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeHook_menu },
    { "Runes: [DISABLED]",                       osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeRunes_menu },
    { "Kick: 12345678901234567",                 osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeKick_menu },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*Propose Change",                         osp_PMENU_ALIGN_LEFT,    NULL, OSP_proposeVote_menu },
    { "*Return to Main Menu",                    osp_PMENU_ALIGN_LEFT,    NULL, OSP_returnMainTeam_menu },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Use [ and ] to move cursor",              osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "ENTR/BKSP selects fwd/back",              osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*v(2.75)",                                osp_PMENU_ALIGN_RIGHT,   NULL, NULL },
};
osp_pmenu_t Vote_Menu2[18] = {
    { "*OSP Tourney DeathMatch",                 osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "[ Item Voting Menu ]",                    osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "Allow Quad: YES",                         osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeItems_menu },
    { "Allow Invul: YES",                        osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeItems_menu },
    { "Quad Drop: YES",                          osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeItems_menu },
    { "Allow BFG: YES",                          osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeItems_menu },
    { "Allow Power Armor: YES",                  osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeItems_menu },
    { "Weapons Stay: YES",                       osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeItems_menu },
    { "Hurt Self: YES",                          osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeItems_menu },
    { "Hurt Team: YES",                          osp_PMENU_ALIGN_LEFT,    NULL, OSP_changeItems_menu },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*Propose Change",                         osp_PMENU_ALIGN_LEFT,    NULL, OSP_proposeVote_menu },
    { "*Return to Main Menu",                    osp_PMENU_ALIGN_LEFT,    NULL, OSP_returnMainTeam_menu },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Use [ and ] to move cursor",              osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "ENTR/BKSP selects fwd/back",              osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*v(2.75)",                                osp_PMENU_ALIGN_RIGHT,   NULL, NULL },
};
osp_pmenu_t Bot_Menu[18] = {
    { "*OSP Tourney DeathMatch",                 osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "[ Gladiator Bots Menu ]",                 osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "*Add specific bot:",                      osp_PMENU_ALIGN_LEFT,    NULL, OSP_addSpecificBot_menu },
    { "GB|12345678901234567",                    osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Add random bots: 99",                     osp_PMENU_ALIGN_LEFT,    NULL, OSP_addBots_menu },
    { "Remove random bots: 99",                  osp_PMENU_ALIGN_LEFT,    NULL, OSP_removeBots_menu },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Total active bots: 99",                   osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*Propose Change",                         osp_PMENU_ALIGN_LEFT,    NULL, OSP_proposeVote_menu },
    { "*Return to Main Menu",                    osp_PMENU_ALIGN_LEFT,    NULL, OSP_returnMainTeam_menu },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Use [ and ] to move cursor",              osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "ENTR/BKSP selects fwd/back",              osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*v(2.75)",                                osp_PMENU_ALIGN_RIGHT,   NULL, NULL },
};
osp_pmenu_t Proposal_Menu[18] = {
    { "*OSP Tourney DeathMatch",                 osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "[ Proposal Selection ]",                  osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "*Proposal: ",                             osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Change map to q2dm8 blah",                osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "12345678901234567890123",                 osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "50% Accepted, 50% Declined",              osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "51% Needed to Decide",                    osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*Accept Proposal",                        osp_PMENU_ALIGN_LEFT,    NULL, OSP_acceptVote_menu },
    { "*Decline Proposal",                       osp_PMENU_ALIGN_LEFT,    NULL, OSP_declineVote_menu },
    { "*Return to Main Menu",                    osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Use [ and ] to move cursor",              osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Press ENTER to select",                   osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "*v(2.75)",                                osp_PMENU_ALIGN_RIGHT,   NULL, NULL },
};
osp_pmenu_t Proposal_Menu2[18] = {
    { "*OSP Tourney DeathMatch",                 osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "[ Proposal Selection ]",                  osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "Allow Quad: YESNO",                       osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Allow Invul: YESNO",                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Drop Quad: YESNO",                        osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Allow BFG: YESNO",                        osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Allow Power Armor: YESNO",                osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Weapons Stay: YESNO",                     osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Hurt Self: YESNO",                        osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Hurt Team: YESNO",                        osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "50% Accepted, 50% Declined",              osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "51% Needed to Decide",                    osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*Accept Proposal",                        osp_PMENU_ALIGN_LEFT,    NULL, OSP_acceptVote_menu },
    { "*Decline Proposal",                       osp_PMENU_ALIGN_LEFT,    NULL, OSP_declineVote_menu },
    { "*Return to Main Menu",                    osp_PMENU_ALIGN_CENTER,  NULL, NULL },
};
osp_pmenu_t Help_Menu[18] = {
    { "*OSP Tourney DeathMatch",                 osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "*[ Console Commands ]",                   osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "vote: Vote on/make changes",              osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "yes/no: Accept/deny vote",                osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "hook/unhook: Uses grapple",               osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "chasecam: Toggle chasecam",               osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "observe: Toggle observer",                osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "id: Toggle player ID",                    osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "motd: Message Of The Day",                osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "referee: Enable ref cmds",                osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "stats: Show player stats",                osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "oldscore: Show last scores",              osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "oldstats: Show last stats",               osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*Show Match Commands",                    osp_PMENU_ALIGN_LEFT,    NULL, OSP_help2Menu },
    { "*Show Team/1v1 Commands",                 osp_PMENU_ALIGN_LEFT,    NULL, OSP_help3Menu },
    { "*Return to Main Menu",                    osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*v(2.75)",                                osp_PMENU_ALIGN_RIGHT,   NULL, NULL },
};
osp_pmenu_t Help2_Menu[18] = {
    { "*OSP Tourney DeathMatch",                 osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "*[ Match Commands ]",                     osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "ready: Switch to READY",                  osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "       status",                           osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "notready: Switch to",                     osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "          NOTREADY status",               osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "time: Call timeout/timein",               osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "matchinfo: Show all match",               osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "           parameters",                   osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*Show Console Commands",                  osp_PMENU_ALIGN_LEFT,    NULL, OSP_helpMenu },
    { "*Show Team/1v1 Commands",                 osp_PMENU_ALIGN_LEFT,    NULL, OSP_help3Menu },
    { "*Return to Main Menu",                    osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*v(2.75)",                                osp_PMENU_ALIGN_RIGHT,   NULL, NULL },
};
osp_pmenu_t Help3_Menu[18] = {
    { "*OSP Tourney DeathMatch",                 osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "*[ Team/1v1 Commands ]",                  osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "teamname: Change teamname",               osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "teamskin: Change teamskin",               osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "join: Join specified team",               osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "switchteam: Switch teams",                osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "captain: Show/set team capt",             osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "lockteam: Lock team",                     osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "unlockteam: Unlock team",                 osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "invite: Invite a player",                 osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "remove: Remove teammate",                 osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "joincode: View/set/use",                  osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "          team's joincode",               osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*Show Console Commands",                  osp_PMENU_ALIGN_LEFT,    NULL, OSP_helpMenu },
    { "*Show Match Commands",                    osp_PMENU_ALIGN_LEFT,    NULL, OSP_help2Menu },
    { "*Return to Main Menu",                    osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*v(2.75)",                                osp_PMENU_ALIGN_RIGHT,   NULL, NULL },
};
osp_pmenu_t Invite_Menu[18] = {
    { "*OSP Tourney DeathMatch",                 osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "[ Invitation Menu ]",                     osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "*You have been invited",                  osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "*to join team:",                          osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_RIGHT,   NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_RIGHT,   NULL, NULL },
    { "Team_A Team_B Team_C oo",                 osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_RIGHT,   NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "*Accept Invitation",                      osp_PMENU_ALIGN_LEFT,    NULL, OSP_joinTeam_menu },
    { "*Deny Invitation",                        osp_PMENU_ALIGN_LEFT,    NULL, OSP_inviteClose_menu },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { NULL,                                      osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Use [ and ] to move cursor",              osp_PMENU_ALIGN_LEFT,    NULL, NULL },
    { "Press ENTER to select",                   osp_PMENU_ALIGN_CENTER,  NULL, NULL },
    { "*v(2.75)",                                osp_PMENU_ALIGN_RIGHT,   NULL, NULL },
};
char    voted_botname[32];

// The menu text the update functions build.  Every *_Menu table entry's `text`
// is just a pointer, so the builders sprintf into file statics and point the
// entries at them.  Names reconstructed.
static char pm_line1[32];
static char pm_line2[32];
static char pm_pct[32];
static char pm_needed[32];
static char pm2_pct[32];
static char pm2_needed[32];
static char pm2_line0[32];
static char pm2_line1[32];
static char pm2_line2[32];
static char pm2_line3[32];
static char pm2_line4[32];
static char pm2_line5[32];
static char pm2_line6[32];
static char pm2_line7[32];
static char vm_map[32];
static char vm_config[32];
static char vm_toggles[32];
static char vm_bots[32];
static char vm_time[32];
static char vm_frag[32];
static char vm_hook[32];
static char vm_runes[32];
static char vm_kick[32];
static int  tm_teamnum0;
static int  tm_teamnum1;
static char tm_title[32];
static char tm_join[2][32];
static char tm_count0[32];
static char tm_count1[32];
static char tm_admin[32];
static char tm_obs[32];
static char tm_chase[32];
static char tm_hud[32];
static char tm_id[32];
static char tm_vote[32];
static char as_title[32];
static char as_prompt[32];
static char as_choice[32];
static char as_addr[32];
static char as_action[32];
static char dm_play_line[32];
static char dm_admin_line[32];
static char dm_id_line[32];
static char dm_vote_line[32];
static char dm_obs_line[32];
static char dm_chase_line[32];
static char dm_hud_line[32];
static int  v2_bits0;
static int  v2_bits1;
static int  v2_bits2;
static int  v2_bits3;
static int  v2_bits4;
static int  v2_bits5;
static int  v2_bits6;
static int  v2_bits7;
static char v2_line0[32];
static char v2_line1[32];
static char v2_line2[32];
static char v2_line3[32];
static char v2_line4[32];
static char v2_line5[32];
static char v2_line6[32];
static char v2_line7[32];
static char bot_name_line[32];
static char bot_add_line[32];
static char bot_rem_line[32];
static char bot_total_line[32];
static int  bot_add_arg;
static int  bot_rem_arg;
static char invite_teamname[32];
static int  invite_teamnum;
static char admin_title[32];
// A SECOND 32-byte line static here, and nothing in the image references it:
// real's .bss gives this position 64 bytes where one line needs 32, and the
// next TU's block starts exactly 0x40 on.  Only its existence and size are
// evidence -- the name is reconstructed.
static q_unused char admin_unused_line[32];

// Names reconstructed: the three ints the AdminMain_Menu select entries carry,
// dereferenced into resp.osp_r238.
static int  admin_mode_map = 1;
static int  admin_mode_ban = 2;
static int  admin_mode_kick = 4;

/*
================
OSP_refereeOnly

The menu route into the admin tree had no check of its own.

The text route does: osp_clientcmd.c gates `r_kick`, `r_ban`, `r_map` and the
rest behind `else if (ent->osp_e39c)`, so a player who types them is refused by
the dispatcher before the command function is reached -- which is why
OSP_rban_cmd and friends never needed a check inside them.  The menu route
reached the same functions and was gated by one thing only: whether
OSP_updateTeamMenu/OSP_updateDMMenu had left a SelectFunc on the "*Admin Menu"
row.  That is a field in a file-scope table, written from this client's
`osp_e39c` and read by every client, which is the bug p_menu.c departure 2 is
about; before the per-client copy, a referee opening their menu handed the row
to anyone else who had one open and then moved the cursor.

So the copy closes the reachable path and this closes the CLASS.  A row's
SelectFunc is a drawing decision; it is not an authorisation, and nothing below
it should have been treating it as one.  Six entry points ask here now: the two
that open an admin menu, the two that move its selection, and the two that act.

Truthy, not `== 1`, because that is what the flag means: 1 is a referee who
came in through the match's ref_passwd and 2 is one who used `referee` with
referee_password or rcon (osp_cmds.c).  Both are referees, both are what
osp_clientcmd.c's dispatcher accepts, and both are what the menu row itself
tests -- so this predicate adds no new policy, it just applies the existing one
where it was missing.
================
*/
static bool OSP_refereeOnly(edict_t *ent)
{
    if (ent->osp_e39c)
        return true;

    gi.cprintf(ent, PRINT_HIGH, "You are not a referee.\n");
    return false;
}

void OSP_teamMenu(edict_t *ent)
{
    int     cur;

    if (ent->client->menu_owner == MENU_TOURNEY)
        osp_PMenu_Close(ent);
    else {
        cur = OSP_updateTeamMenu(ent);
        osp_PMenu_Open(ent, Team_Menu, cur, 18);
    }
}

void OSP_DMMenu(edict_t *ent)
{
    int     cur;

    if (ent->client->menu_owner == MENU_TOURNEY)
        osp_PMenu_Close(ent);
    else {
        cur = OSP_updateDMMenu(ent);
        osp_PMenu_Open(ent, RegDM_Menu, cur, 18);
    }
}

void OSP_adminMenu(edict_t *ent)
{
    int     cur;

    // Before the toggle, not inside it: a non-referee has no admin menu to
    // close, and `inven` closes whatever is open through the arbiter anyway.
    if (!OSP_refereeOnly(ent))
        return;

    if (ent->client->menu_owner == MENU_TOURNEY)
        osp_PMenu_Close(ent);
    else {
        cur = OSP_updateAdminMenu(ent);
        osp_PMenu_Open(ent, AdminMain_Menu, cur, 17);
    }
}

void OSP_adminSelectMenu(edict_t *ent, osp_pmenu_t *p)
{
    int     which;

    if (!OSP_refereeOnly(ent))
        return;

    // Read the arg before the close.  `p` points into this client's private
    // copy of the rows, and osp_PMenu_Close() frees that copy -- so the
    // donor's order, close and then dereference, became a read of released
    // memory the moment the entries stopped being the global table
    // (p_menu.c departure 2).  It is the only leaf that does this: the other
    // two `*(int *)p->arg` sites, OSP_joinTeam_menu and OSP_changeItems_menu,
    // read before they close anything.
    which = *(int *)p->arg;

    osp_PMenu_Close(ent);
    ent->client->resp.osp_r238 = which;
    ent->client->resp.osp_r290 = -1;
    OSP_updateAdminSelectMenu(ent);
    osp_PMenu_Open(ent, AdminSelect_Menu, 6, 17);
}

// The voting menu opener. An observer may not open it while there are humans
// playing -- `active_clients - botglobals.numbots` is "somebody other than a
// bot is in the game". Once a vote is running the menu becomes read-only: the
// Proposal menu shows what is on the table, and vote_item 0x20 (the item
// toggles) gets its own wider variant.
void OSP_voteMenu(edict_t *ent, osp_pmenu_t *p)
{
    if (OSP_IsMatch() && active_clients - botglobals.numbots && !ent->osp_e39c &&
        ent->client->resp.osp_entered != ENTERED_ENTERED) {
        gi.cprintf(ent, PRINT_HIGH, "Observers cannot vote with active\n");
        gi.cprintf(ent, PRINT_HIGH, "players in the game.\n");
        return;
    }

    osp_PMenu_Close(ent);

    if (!vote_inprogress) {
        // Seed the staging copy of everything that can be voted on with what
        // is in force now, so the menu opens showing no pending change.
        ent->client->resp.osp_r254 = 0;
        ent->client->resp.osp_r290 = -1;
        ent->client->resp.osp_r258 = -1;
        ent->client->resp.osp_r2a0 = (int)timelimit->value;
        ent->client->resp.osp_r25c = (int)fraglimit->value;
        ent->client->resp.osp_r298 = rune_stat;
        ent->client->resp.osp_r268 = -1;
        if ((int)hook_enable->value)
            ent->client->resp.osp_r260 = 1;
        else
            ent->client->resp.osp_r260 = 0;

        OSP_updateVoteMenu(ent);
        osp_PMenu_Open(ent, Vote_Menu, 0, 19);
    } else if (vote_item != 0x20) {
        OSP_updateProposalMenu(ent);
        osp_PMenu_Open(ent, Proposal_Menu, 0, 18);
    } else {
        OSP_updateProposalMenu2(ent);
        osp_PMenu_Open(ent, Proposal_Menu2, 0, 18);
    }
}

void OSP_voteMenu2(edict_t *ent, osp_pmenu_t *p)
{
    if (ent->client->resp.osp_r254) {
        gi.cprintf(ent, PRINT_HIGH, "Sorry, you can change only one item at a time!\n");
        return;
    }

    osp_PMenu_Close(ent);
    if (!vote_inprogress) {
        ent->client->resp.osp_r2a4 = item_settings;
        OSP_updateVoteMenu2(ent);
        osp_PMenu_Open(ent, Vote_Menu2, 0, 18);
    } else {
        OSP_updateProposalMenu(ent);
        osp_PMenu_Open(ent, Proposal_Menu, 0, 18);
    }
}

void OSP_botMenu(edict_t *ent, osp_pmenu_t *p)
{
    if (ent->client->resp.osp_r254) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Sorry, you can change only one item at a time!\n");
        return;
    }

    osp_PMenu_Close(ent);

    if (!vote_inprogress) {
        ent->client->resp.osp_r29c = -1;
        ent->client->resp.osp_r250 = 0;
        ent->client->resp.osp_r294 = 0;
        CheckForNewBotFile();
        OSP_updateBotMenu(ent);
        osp_PMenu_Open(ent, Bot_Menu, 0, 18);
    } else {
        OSP_updateProposalMenu(ent);
        osp_PMenu_Open(ent, Proposal_Menu, 0, 18);
    }
}

void OSP_helpMenu(edict_t *ent, osp_pmenu_t *p)
{
    osp_PMenu_Close(ent);
    if (!OSP_IsTeams())
        Help_Menu[16].SelectFunc = OSP_returnMainDM_menu;
    else
        Help_Menu[16].SelectFunc = OSP_returnMainTeam_menu;
    osp_PMenu_Open(ent, Help_Menu, 2, 18);
}

void OSP_help2Menu(edict_t *ent, osp_pmenu_t *p)
{
    osp_PMenu_Close(ent);
    if (!OSP_IsTeams())
        Help2_Menu[16].SelectFunc = OSP_returnMainDM_menu;
    else
        Help2_Menu[16].SelectFunc = OSP_returnMainTeam_menu;
    osp_PMenu_Open(ent, Help2_Menu, 2, 18);
}

void OSP_help3Menu(edict_t *ent, osp_pmenu_t *p)
{
    osp_PMenu_Close(ent);
    if (!OSP_IsTeams())
        Help3_Menu[16].SelectFunc = OSP_returnMainDM_menu;
    else
        Help3_Menu[16].SelectFunc = OSP_returnMainTeam_menu;
    osp_PMenu_Open(ent, Help3_Menu, 2, 18);
}

void OSP_inviteMenu(edict_t *ent)
{
    int     cur;

    if (ent->client->menu_owner == MENU_TOURNEY)
        osp_PMenu_Close(ent);
    cur = OSP_updateInviteMenu(ent);
    osp_PMenu_Open(ent, Invite_Menu, cur, 18);
}

// Rebuild the team-mode main menu and return where the cursor should sit.
// `pick` is an EMPTY team the client could found: if they carry a default team
// name (team mode) or in 1v1, that slot's "*Join" line is relabelled with the
// name they would create rather than the team's current one. A team whose name
// already matches cancels the offer, which is what the shared `pick = -1`
// break at the bottom of the search loop does.
int OSP_updateTeamMenu(edict_t *ent)
{
    int     i;
    int     pick;

    pick = -1;
    Q_snprintf(tm_join[0], sizeof(tm_join[0]), "*Join %s", osp_teams[0].netname);
    Q_snprintf(tm_join[1], sizeof(tm_join[1]), "*Join %s", osp_teams[1].netname);
    Q_snprintf(tm_count0, sizeof(tm_count0), "*(%d players)", OSP_teamCount(0));
    Q_snprintf(tm_count1, sizeof(tm_count1), "*(%d players)", OSP_teamCount(1));

    if (G_Ruleset() == RULESET_TDM)
        Q_snprintf(tm_title, sizeof(tm_title), "[ Teamplay Mode ]");
    else
        Q_snprintf(tm_title, sizeof(tm_title), "[ 1v1 Mode ]");

    for (i = 1; i >= 0; i--) {
        if (!OSP_teamCount(i))
            pick = i;

        if (ent->osp_e3a0[0] && G_Ruleset() == RULESET_TDM &&
            !Q_stricmp(osp_teams[i].netname, ent->osp_e3a0)) {
            pick = -1;
            break;
        }
        if (G_Ruleset() == RULESET_DUEL &&
            !Q_stricmp(osp_teams[i].netname, ent->client->pers.netname)) {
            pick = -1;
            break;
        }
    }

    if (pick >= 0 && G_Ruleset() == RULESET_TDM && ent->osp_e3a0[0])
        Q_snprintf(tm_join[pick], sizeof(tm_join[pick]), "*Join %s", ent->osp_e3a0);
    else if (pick >= 0 && G_Ruleset() == RULESET_DUEL)
        Q_snprintf(tm_join[pick], sizeof(tm_join[pick]), "*Join %s", ent->client->pers.netname);

    Team_Menu[1].text = tm_title;
    Team_Menu[3].text = tm_join[0];
    Team_Menu[4].text = tm_count0;
    Team_Menu[5].text = tm_join[1];
    Team_Menu[6].text = tm_count1;

    tm_teamnum0 = 0;
    tm_teamnum1 = 1;
    Team_Menu[3].arg = &tm_teamnum0;
    Team_Menu[5].arg = &tm_teamnum1;

    if (ent->osp_e39c) {
        Q_snprintf(tm_admin, sizeof(tm_admin), "*Admin Menu");
        Team_Menu[7].SelectFunc = OSP_returnMainAdmin_menu;
    } else {
        Q_snprintf(tm_admin, sizeof(tm_admin), " ");
        Team_Menu[7].SelectFunc = NULL;
    }
    Team_Menu[7].text = tm_admin;

    if (ent->client->resp.osp_entered != 2)
        Q_snprintf(tm_obs, sizeof(tm_obs), "*Enter OBSERVER Mode");
    else
        Q_snprintf(tm_obs, sizeof(tm_obs), "*Leave OBSERVER Mode");
    Team_Menu[9].text = tm_obs;

    if (ent->client->resp.osp_entered != 4 && ent->client->resp.osp_entered != 8)
        Q_snprintf(tm_chase, sizeof(tm_chase), "*Enter CHASECAM Mode");
    else
        Q_snprintf(tm_chase, sizeof(tm_chase), "*Leave CHASECAM Mode");
    Team_Menu[10].text = tm_chase;

    if ((int)allow_id->value == 2) {
        Q_snprintf(tm_id, sizeof(tm_id), "*Player ID: OFF [LOCKED]");
        Team_Menu[11].SelectFunc = NULL;
    } else if ((int)allow_id->value == 3) {
        Q_snprintf(tm_id, sizeof(tm_id), "*Player ID: ON [LOCKED]");
        Team_Menu[11].SelectFunc = NULL;
    } else {
        Team_Menu[11].SelectFunc = OSP_toggleID_menu;
        if (ent->client->resp.osp_r204)
            Q_snprintf(tm_id, sizeof(tm_id), "*Player ID: ON");
        else
            Q_snprintf(tm_id, sizeof(tm_id), "*Player ID: OFF");
    }
    Team_Menu[11].text = tm_id;

    Q_strlcpy(tm_hud, "*Change HUD Layout", sizeof(tm_hud));
    Team_Menu[12].text = tm_hud;
    Team_Menu[12].SelectFunc = OSP_changeHUD;

    if (!(int)vote_enable->value) {
        Q_snprintf(tm_vote, sizeof(tm_vote), "*Voting Menu [DISABLED]");
        Team_Menu[8].text = tm_vote;
        Team_Menu[8].SelectFunc = NULL;
    } else {
        Q_snprintf(tm_vote, sizeof(tm_vote), "*Voting Menu");
        Team_Menu[8].text = tm_vote;
        Team_Menu[8].SelectFunc = OSP_voteMenu;
    }

    if (ent->osp_e39c) {
        if (sync_stat > 2 || ent->client->resp.osp_entered == ENTERED_ENTERED)
            return 7;
    }

    if (ent->client->resp.osp_entered == ENTERED_ENTERED ||
        (vote_inprogress && !ent->client->resp.osp_r2d8) ||
        (G_Ruleset() == RULESET_DUEL && OSP_teamCount(0) && OSP_teamCount(1)))
        return 8;

    if (pick >= 0)
        return pick * 2 + 3;

    // Neither team is empty: sit on the smaller one, and toss a coin on a tie.
    if (OSP_teamCount(0) > OSP_teamCount(1))
        return 5;
    if (OSP_teamCount(1) > OSP_teamCount(0))
        return 3;

    return (Q_rand() & 1) ? 3 : 5;
}

void OSP_returnMainTeam_menu(edict_t *ent, osp_pmenu_t *p)
{
    osp_PMenu_Close(ent);
    OSP_teamMenu(ent);
}

void OSP_toggleID_menu(edict_t *ent, osp_pmenu_t *p)
{
    OSP_id_cmd(ent);
    if (OSP_IsTeams()) {
        OSP_updateTeamMenu(ent);
        osp_PMenu_Sync(ent, Team_Menu);
    } else {
        OSP_updateDMMenu(ent);
        osp_PMenu_Sync(ent, RegDM_Menu);
    }
    osp_PMenu_Update(ent);
}

void OSP_changeHUD(edict_t *ent, osp_pmenu_t *p)
{
    OSP_hud_cmd(ent);
}

void OSP_changeObserve(edict_t *ent, osp_pmenu_t *p)
{
    OSP_startObserve(ent);
    osp_PMenu_Close(ent);
    ent->client->resp.osp_r24c = 2;
}

void OSP_changeChase(edict_t *ent, osp_pmenu_t *p)
{
    if ((!active_clients && ent->client->resp.osp_entered == 2) ||
        (active_clients == 1 && ent->client->resp.osp_entered == 1)) {
        gi.cprintf(ent, PRINT_HIGH, "No clients to chase.\n");
        return;
    }

    OSP_ChaseCam(ent);
    ent->client->resp.osp_r24c = 2;
    osp_PMenu_Close(ent);
}

// Rebuild the plain-DM main menu for one client and return where the cursor
// should sit. Every visible line is a static buffer this fills in; an entry is
// greyed out by nulling its SelectFunc rather than by hiding it.
// "*Join <team>" from the team menu. `p->arg` is the team index. resp.osp_r078
// is the pending invitation (team + 1), which is what gets a client past a
// locked or full team; resp.osp_r030 is "has been in the game before", so a
// first-time joiner can still found an empty team from their default name.
void OSP_joinTeam_menu(edict_t *ent, osp_pmenu_t *p)
{
    int     tnum;
    int     pick;
    int     invited;
    int     i;

    tnum = *(int *)p->arg;
    invited = ent->client->resp.osp_r078;

    if (ent->client->resp.team == tnum) {
        gi.cprintf(ent, PRINT_HIGH, "You are already on \"%s\"!\n",
                   osp_teams[tnum].netname);
        return;
    }

    if (ent->osp_e39c == 1) {
        gi.cprintf(ent, PRINT_HIGH, "Sorry, referees cannot enter the game!\n");
        osp_PMenu_Close(ent);
        return;
    }

    if (who_paused == -2) {
        gi.cprintf(ent, PRINT_HIGH, "Sorry, cannot join during a forced pause.\n");
        return;
    }

    if (sync_stat == 4 && !invited && (int)match_latejoin->value <= 1) {
        gi.cprintf(ent, PRINT_HIGH, "Match in progress. You can only observe.\n");
        osp_PMenu_Close(ent);
        return;
    }

    if (G_Ruleset() == RULESET_DUEL && !OSP_1v1AllowJoin(ent)) {
        osp_PMenu_Close(ent);
        return;
    }

    for (i = 1, pick = -2; i >= 0; i--) {
        if (!OSP_teamCount(i))
            pick = i;
        if (ent->osp_e3a0[0] && G_Ruleset() == RULESET_TDM &&
            !Q_stricmp(osp_teams[i].netname, ent->osp_e3a0)) {
            pick = -1;
            break;
        }
    }

    if (!ent->client->resp.osp_r030 && G_Ruleset() == RULESET_TDM &&
        (!osp_teams[tnum].osp_m0f4 || invited) && pick == tnum) {
        if (OSP_defaultTeam(ent))
            goto joined;
    }

    if (G_Ruleset() == RULESET_DUEL && OSP_1v1Team(ent))
        goto joined;

    if ((!(OSP_teamCount(tnum) >= (int)team_maxplayers->value && !invited)
         || (G_Ruleset() == RULESET_TDM && ((int)match_latejoin->value > 2
                             || (sync_stat > 2 && (int)match_latejoin->value == 2 &&
                                 OSP_teamCount(tnum) < (int)team_maxplayers->value))))
        && !(osp_teams[tnum].osp_m0f4 && !invited)) {
        if (invited) {
            if (tnum != invited - 1 &&
                OSP_teamCount(tnum) >= (int)team_maxplayers->value) {
                gi.cprintf(ent, PRINT_HIGH,
                           "You've been invited to join only team %s\n",
                           osp_teams[invited - 1].greenname);
                return;
            }
            ent->client->resp.osp_r078 = 0;
        }

        if (!OSP_addTeamMember(ent, tnum))
            return;
        goto joined;
    } else {
        if (osp_teams[tnum].osp_m0f4 && !invited)
            gi.cprintf(ent, PRINT_HIGH, "\"%s\" is locked.\n", osp_teams[tnum].netname);
        else
            gi.cprintf(ent, PRINT_HIGH, "\"%s\" is full.\n", osp_teams[tnum].netname);
        return;
    }

joined:
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
            OSP_setSingleAccuracy(ent);
        } else
            ent->client->resp.enterframe =
                level.framenum - ent->client->resp.osp_r2d4;

        gi.bprintf(PRINT_HIGH, "%s entered the game (clients = %i)\n",
                   ent->client->pers.netname, active_clients);
        EntityListAdd(ent);
        OSP_Stats_PlayerEnter(ent);
    }

    ent->client->resp.score = ent->client->resp.osp_r248;
    ent->client->resp.osp_r0a0--;
    ent->client->resp.osp_r010 -= 2;
    OSP_notready_cmd(ent, true);

    if (sync_stat > 2)
        OSP_initTeamFrags(ent);
    OSP_setShowParams();
    osp_PMenu_Close(ent);
}

int OSP_updateDMMenu(edict_t *ent)
{
    if (ent->client->resp.osp_entered == ENTERED_ENTERED)
        Q_snprintf(dm_play_line, sizeof(dm_play_line), "*Return to Play");
    else
        Q_snprintf(dm_play_line, sizeof(dm_play_line), "*Enter the Game");
    RegDM_Menu[4].text = dm_play_line;

    // THE `else` is not cosmetic.  These tables are file-scope globals and the
    // builders restage them per client, so a branch that only ever CLEARS a
    // SelectFunc is a one-way latch: the donor's version of this test has no
    // else, and `RegDM_Menu[4]` -- "Enter the Game" -- is assigned nowhere
    // else in the mod, so its only non-NULL value is the initialiser above.
    // One referee opening the DM menu, or anyone opening it during a match
    // with late joining locked, killed that row for every client; and because
    // the table has static storage duration and nothing re-initialises it, the
    // row stayed dead across map changes for the life of the loaded library.
    // Deep-copying the rows per client (see p_menu.c, departure 2) does
    // not reach this: what is latched is the template every copy is taken
    // from.
    if (ent->osp_e39c == 1 ||
        (sync_stat == 4 && (int)match_latejoin->value <= 1))
        RegDM_Menu[4].SelectFunc = NULL;
    else
        RegDM_Menu[4].SelectFunc = OSP_dmReturn_menu;

    if (ent->osp_e39c) {
        Q_snprintf(dm_admin_line, sizeof(dm_admin_line), "*Admin Menu");
        RegDM_Menu[6].SelectFunc = OSP_returnMainAdmin_menu;
    } else {
        Q_snprintf(dm_admin_line, sizeof(dm_admin_line), " ");
        RegDM_Menu[6].SelectFunc = NULL;
    }
    RegDM_Menu[6].text = dm_admin_line;

    if (ent->client->resp.osp_entered != 2)
        Q_snprintf(dm_obs_line, sizeof(dm_obs_line), "*Enter OBSERVER Mode");
    else
        Q_snprintf(dm_obs_line, sizeof(dm_obs_line), "*Leave OBSERVER Mode");
    RegDM_Menu[7].text = dm_obs_line;

    if (ent->client->resp.osp_entered != 4 &&
        ent->client->resp.osp_entered != 8)
        Q_snprintf(dm_chase_line, sizeof(dm_chase_line), "*Enter CHASECAM Mode");
    else
        Q_snprintf(dm_chase_line, sizeof(dm_chase_line), "*Leave CHASECAM Mode");
    RegDM_Menu[8].text = dm_chase_line;

    // allow_id 2 and 3 are the server-locked off/on settings, so the line
    // still shows the state but cannot be selected.
    if ((int)allow_id->value == 2) {
        Q_snprintf(dm_id_line, sizeof(dm_id_line), "*Player ID: OFF [LOCKED]");
        RegDM_Menu[10].SelectFunc = NULL;
    } else if ((int)allow_id->value == 3) {
        Q_snprintf(dm_id_line, sizeof(dm_id_line), "*Player ID: ON [LOCKED]");
        RegDM_Menu[10].SelectFunc = NULL;
    } else {
        RegDM_Menu[10].SelectFunc = OSP_toggleID_menu;
        if (ent->client->resp.osp_r204)
            Q_snprintf(dm_id_line, sizeof(dm_id_line), "*Player ID: ON");
        else
            Q_snprintf(dm_id_line, sizeof(dm_id_line), "*Player ID: OFF");
    }
    RegDM_Menu[10].text = dm_id_line;

    if (!(int)vote_enable->value) {
        Q_snprintf(dm_vote_line, sizeof(dm_vote_line), "*Voting Menu [DISABLED]");
        RegDM_Menu[5].text = dm_vote_line;
        RegDM_Menu[5].SelectFunc = NULL;
    } else {
        Q_snprintf(dm_vote_line, sizeof(dm_vote_line), "*Voting Menu");
        RegDM_Menu[5].text = dm_vote_line;
        RegDM_Menu[5].SelectFunc = OSP_voteMenu;
    }

    Q_strlcpy(dm_hud_line, "*Change HUD Layout", sizeof(dm_hud_line));
    RegDM_Menu[9].text = dm_hud_line;
    RegDM_Menu[9].SelectFunc = OSP_changeHUD;

    OSP_setShowParams();

    // Park the cursor on the vote line while a vote this client has not
    // answered yet is running.
    if (vote_inprogress && !ent->client->resp.osp_r2d8)
        return 5;
    if (ent->osp_e39c)
        return 6;
    return 4;
}

void OSP_returnMainDM_menu(edict_t *ent, osp_pmenu_t *p)
{
    osp_PMenu_Close(ent);
    OSP_DMMenu(ent);
}

// "*Return to Play" / "*Enter the Game" from the DM menu. Everything from
// `active_clients++` down is the same join sequence osp_teams.c's
// OSP_teamjoin_cmd runs; the difference is that a first-time entrant also gets
// its accuracy slot seeded, where a re-entrant keeps its old enterframe.
void OSP_dmReturn_menu(edict_t *ent, osp_pmenu_t *p)
{
    if (ent->client->resp.osp_entered != ENTERED_ENTERED) {
        if (sync_stat == 4 && (int)match_latejoin->value <= 1) {
            gi.cprintf(ent, PRINT_HIGH,
                       "Sorry, you cannot enter a match in progress\n");
            return;
        }

        active_clients++;
        ent->client->chase_target = NULL;
        ent->client->resp.osp_entered = ENTERED_ENTERED;
        ent->client->resp.osp_r240 = 0;
        ent->client->osp_t040 = 0;
        ent->client->osp_t03c = NULL;

        if (!ent->client->resp.osp_r030) {
            ent->client->resp.osp_r030 = 1;
            ent->client->resp.enterframe = level.framenum;
            OSP_setSingleAccuracy(ent);
        } else
            ent->client->resp.enterframe =
                level.framenum - ent->client->resp.osp_r2d4;

        if (sync_stat < 4) {
            ent->client->resp.osp_r010 -= 2;
            OSP_notready_cmd(ent, true);
        }

        gi.bprintf(PRINT_HIGH, "%s entered the game (clients = %i)\n",
                   ent->client->pers.netname, active_clients);

        ent->client->resp.score = ent->client->resp.osp_r248;
        ent->client->resp.osp_r0a0--;
        ent->client->resp.osp_r09c--;
        EntityListAdd(ent);
        OSP_DoRankSort();
        OSP_Stats_PlayerEnter(ent);
    }

    osp_PMenu_Close(ent);
}

// The item-toggle voting page. resp.osp_r2a4 is the staged copy of
// `item_settings`, one bit per line, and each menu entry's `arg` is a static
// int holding that line's bit -- which is what OSP_changeItems_menu XORs in.
// Rebuild the voting menu. Each of the nine votable things has its own
// `vote_enable_*` cvar: zero shows "[LOCKED]" and nulls the entry's
// SelectFunc, otherwise the line shows the client's STAGED value (the
// resp.osp_r2xx copy the leaves cycle), not what is in force.
void OSP_updateVoteMenu(edict_t *ent)
{
    if (!(int)vote_enable_map->value) {
        Q_snprintf(vm_map, sizeof(vm_map), "Map: [LOCKED]");
        Vote_Menu[3].SelectFunc = NULL;
    } else if (!map_size) {
        Q_snprintf(vm_map, sizeof(vm_map), "Map: [NOT AVAILABLE]");
        Vote_Menu[3].SelectFunc = NULL;
    } else if (ent->client->resp.osp_r290 == -1) {
        Q_snprintf(vm_map, sizeof(vm_map), "Map: [SELECT]");
        Vote_Menu[3].SelectFunc = OSP_changeMap_menu;
    } else {
        Q_snprintf(vm_map, sizeof(vm_map), "Map: %s",
                   ent->client->resp.osp_r290 < map_size ?
                   map[ent->client->resp.osp_r290].name : "?");
        Vote_Menu[3].SelectFunc = OSP_changeMap_menu;
    }

    if (!(int)vote_enable_config->value) {
        Q_snprintf(vm_config, sizeof(vm_config), "Config: [LOCKED]");
        Vote_Menu[4].SelectFunc = NULL;
    } else if (!conf_size) {
        Q_snprintf(vm_config, sizeof(vm_config), "Config: [NOT AVAILABLE]");
        Vote_Menu[4].SelectFunc = NULL;
    } else if (ent->client->resp.osp_r258 == -1) {
        Q_snprintf(vm_config, sizeof(vm_config), "Config: [SELECT]");
        Vote_Menu[4].SelectFunc = OSP_changeConfig_menu;
    } else {
        // The description if the config has one, otherwise its filename, and
        // either way clipped to 22 characters so the line still fits.
        char cfg[24];

        if (ent->client->resp.osp_r258 >= conf_size)
            Q_strlcpy(cfg, "?", sizeof(cfg));
        else if (conf_info[ent->client->resp.osp_r258][0])
            Q_strlcpy(cfg, conf_info[ent->client->resp.osp_r258], 23);
        else
            Q_strlcpy(cfg, conf_name[ent->client->resp.osp_r258], 23);
        Q_snprintf(vm_config, sizeof(vm_config), "Config: %s", cfg);
        Vote_Menu[4].SelectFunc = OSP_changeConfig_menu;
    }

    if (!(int)vote_enable_toggles->value) {
        Q_snprintf(vm_toggles, sizeof(vm_toggles), "Item toggles [LOCKED]");
        Vote_Menu[5].SelectFunc = NULL;
    } else {
        Q_snprintf(vm_toggles, sizeof(vm_toggles), "Item toggles...");
        Vote_Menu[5].SelectFunc = OSP_voteMenu2;
    }

    if (!(int)vote_enable_bots->value) {
        Q_snprintf(vm_bots, sizeof(vm_bots), "Gladiator Bots [LOCKED]");
        Vote_Menu[6].SelectFunc = NULL;
    } else {
        Q_snprintf(vm_bots, sizeof(vm_bots), "Gladiator Bots...");
        Vote_Menu[6].SelectFunc = OSP_botMenu;
    }

    if (!(int)vote_enable_time->value) {
        Q_snprintf(vm_time, sizeof(vm_time), "Timelimit: [LOCKED]");
        Vote_Menu[7].SelectFunc = NULL;
    } else if (!ent->client->resp.osp_r2a0) {
        Q_snprintf(vm_time, sizeof(vm_time), "Timelimit: OFF");
        Vote_Menu[7].SelectFunc = OSP_changeTime_menu;
    } else {
        Q_snprintf(vm_time, sizeof(vm_time), "Timelimit: %d", ent->client->resp.osp_r2a0);
        Vote_Menu[7].SelectFunc = OSP_changeTime_menu;
    }

    if (!(int)vote_enable_frag->value) {
        Q_snprintf(vm_frag, sizeof(vm_frag), "Fraglimit: [LOCKED]");
        Vote_Menu[8].SelectFunc = NULL;
    } else if (!ent->client->resp.osp_r25c) {
        Q_snprintf(vm_frag, sizeof(vm_frag), "Fraglimit: NONE");
        Vote_Menu[8].SelectFunc = OSP_changeFrag_menu;
    } else {
        Q_snprintf(vm_frag, sizeof(vm_frag), "Fraglimit: %d", ent->client->resp.osp_r25c);
        Vote_Menu[8].SelectFunc = OSP_changeFrag_menu;
    }

    if (!(int)vote_enable_hook->value) {
        Q_snprintf(vm_hook, sizeof(vm_hook), "The Hook: [LOCKED]");
        Vote_Menu[9].SelectFunc = NULL;
    } else if (!ent->client->resp.osp_r260) {
        Q_snprintf(vm_hook, sizeof(vm_hook), "The Hook: DISABLED");
        Vote_Menu[9].SelectFunc = OSP_changeHook_menu;
    } else {
        Q_snprintf(vm_hook, sizeof(vm_hook), "The Hook: ENABLED");
        Vote_Menu[9].SelectFunc = OSP_changeHook_menu;
    }

    if (!(int)vote_enable_runes->value) {
        Q_snprintf(vm_runes, sizeof(vm_runes), "Runes: [LOCKED]");
        Vote_Menu[10].SelectFunc = NULL;
    } else if (!ent->client->resp.osp_r298) {
        Q_snprintf(vm_runes, sizeof(vm_runes), "Runes: DISABLED");
        Vote_Menu[10].SelectFunc = OSP_changeRunes_menu;
    } else {
        Q_snprintf(vm_runes, sizeof(vm_runes), "Runes: ENABLED");
        Vote_Menu[10].SelectFunc = OSP_changeRunes_menu;
    }

    if (!(int)vote_enable_kick->value) {
        Q_snprintf(vm_kick, sizeof(vm_kick), "Kick: [LOCKED]");
        Vote_Menu[11].SelectFunc = NULL;
    } else if (ent->client->resp.osp_r268 == -1) {
        Q_snprintf(vm_kick, sizeof(vm_kick), "Kick: [SELECT]");
        Vote_Menu[11].SelectFunc = OSP_changeKick_menu;
    } else {
        Q_snprintf(vm_kick, sizeof(vm_kick), "Kick: %s",
                (char *)&ent->client->resp.osp_r26c[1]);
        Vote_Menu[11].SelectFunc = OSP_changeKick_menu;
    }

    Vote_Menu[3].text = vm_map;
    Vote_Menu[4].text = vm_config;
    Vote_Menu[5].text = vm_toggles;
    Vote_Menu[6].text = vm_bots;
    Vote_Menu[7].text = vm_time;
    Vote_Menu[8].text = vm_frag;
    Vote_Menu[9].text = vm_hook;
    Vote_Menu[10].text = vm_runes;
    Vote_Menu[11].text = vm_kick;

    if (OSP_IsTeams())
        Vote_Menu[14].SelectFunc = OSP_returnMainTeam_menu;
    else
        Vote_Menu[14].SelectFunc = OSP_returnMainDM_menu;
}

void OSP_updateVoteMenu2(edict_t *ent)
{
    {
        if (ent->client->resp.osp_r2a4 & 1)
            Q_snprintf(v2_line0, sizeof(v2_line0), "Allow Quad: YES");
        else
            Q_snprintf(v2_line0, sizeof(v2_line0), "Allow Quad: NO");
    }

    {
        if (ent->client->resp.osp_r2a4 & 2)
            Q_snprintf(v2_line1, sizeof(v2_line1), "Allow Invul: YES");
        else
            Q_snprintf(v2_line1, sizeof(v2_line1), "Allow Invul: NO");
    }

    {
        if (ent->client->resp.osp_r2a4 & 4)
            Q_snprintf(v2_line2, sizeof(v2_line2), "Drop Quad: YES");
        else
            Q_snprintf(v2_line2, sizeof(v2_line2), "Drop Quad: NO");
    }

    {
        if (ent->client->resp.osp_r2a4 & 8)
            Q_snprintf(v2_line3, sizeof(v2_line3), "Allow BFG: YES");
        else
            Q_snprintf(v2_line3, sizeof(v2_line3), "Allow BFG: NO");
    }

    {
        if (ent->client->resp.osp_r2a4 & 0x10)
            Q_snprintf(v2_line4, sizeof(v2_line4), "Allow Power Armor: YES");
        else
            Q_snprintf(v2_line4, sizeof(v2_line4), "Allow Power Armor: NO");
    }

    {
        if (ent->client->resp.osp_r2a4 & 0x20)
            Q_snprintf(v2_line5, sizeof(v2_line5), "Weapons Stay: YES");
        else
            Q_snprintf(v2_line5, sizeof(v2_line5), "Weapons Stay: NO");
    }

    {
        if (ent->client->resp.osp_r2a4 & 0x40)
            Q_snprintf(v2_line6, sizeof(v2_line6), "Hurt Self: YES");
        else
            Q_snprintf(v2_line6, sizeof(v2_line6), "Hurt Self: NO");
    }

    // The second one-way latch, and the same reasoning as RegDM_Menu[4] in
    // OSP_updateDMMenu: the donor blanks "Hurt Team" outside team play and
    // never puts it back, so a DM map followed by a team map left the row
    // drawn -- its text IS restaged below -- but dead, for the life of the
    // loaded library.  The restore has to be explicit because the table is a
    // global, not because of anything this client did.
    if (OSP_IsTeams()) {
        if (ent->client->resp.osp_r2a4 & 0x80)
            Q_snprintf(v2_line7, sizeof(v2_line7), "Hurt Team: YES");
        else
            Q_snprintf(v2_line7, sizeof(v2_line7), "Hurt Team: NO");
        Vote_Menu2[10].SelectFunc = OSP_changeItems_menu;
    } else {
        v2_line7[0] = 0;
        Vote_Menu2[10].SelectFunc = NULL;
    }

    v2_bits0 = 1;
    v2_bits1 = 2;
    v2_bits2 = 4;
    v2_bits3 = 8;
    v2_bits4 = 0x10;
    v2_bits5 = 0x20;
    v2_bits6 = 0x40;
    v2_bits7 = 0x80;

    Vote_Menu2[3].arg = &v2_bits0;
    Vote_Menu2[4].arg = &v2_bits1;
    Vote_Menu2[5].arg = &v2_bits2;
    Vote_Menu2[6].arg = &v2_bits3;
    Vote_Menu2[7].arg = &v2_bits4;
    Vote_Menu2[8].arg = &v2_bits5;
    Vote_Menu2[9].arg = &v2_bits6;
    Vote_Menu2[10].arg = &v2_bits7;

    Vote_Menu2[3].text = v2_line0;
    Vote_Menu2[4].text = v2_line1;
    Vote_Menu2[5].text = v2_line2;
    Vote_Menu2[6].text = v2_line3;
    Vote_Menu2[7].text = v2_line4;
    Vote_Menu2[8].text = v2_line5;
    Vote_Menu2[9].text = v2_line6;
    Vote_Menu2[10].text = v2_line7;

    if (OSP_IsTeams())
        Vote_Menu2[13].SelectFunc = OSP_returnMainTeam_menu;
    else
        Vote_Menu2[13].SelectFunc = OSP_returnMainDM_menu;
}

void OSP_updateBotMenu(edict_t *ent)
{
    bot_t       *b;
    int         ncount;
    int         t;

    if (ent->client->resp.osp_r29c == -1)
        Q_snprintf(bot_name_line, sizeof(bot_name_line), "[SELECT]");
    else {
        for (ncount = 0, b = botlist; b; b = b->next, ncount++)
            ;

        if (!ncount)
            Q_snprintf(bot_name_line, sizeof(bot_name_line), "[NONE AVAILABLE]");
        else {
            for (t = 0, b = botlist; b && t < ent->client->resp.osp_r29c; b = b->next, t++)
                ;
            if (b)
                Q_strlcpy(bot_name_line, b->name, sizeof(bot_name_line));
        }
    }

    Q_snprintf(bot_add_line, sizeof(bot_add_line), "*Add random bots: %d", ent->client->resp.osp_r250);
    Q_snprintf(bot_rem_line, sizeof(bot_rem_line), "*Remove random bots: %d", ent->client->resp.osp_r294);
    Q_snprintf(bot_total_line, sizeof(bot_total_line), "Total active bots: %d", botglobals.numbots);

    bot_add_arg = ent->client->resp.osp_r250;
    bot_rem_arg = ent->client->resp.osp_r294;

    Bot_Menu[6].arg = &bot_add_arg;
    Bot_Menu[7].arg = &bot_rem_arg;
    Bot_Menu[4].text = bot_name_line;
    Bot_Menu[6].text = bot_add_line;
    Bot_Menu[7].text = bot_rem_line;
    Bot_Menu[9].text = bot_total_line;

    if (OSP_IsTeams())
        Bot_Menu[13].SelectFunc = OSP_returnMainTeam_menu;
    else
        Bot_Menu[13].SelectFunc = OSP_returnMainDM_menu;
}

// The read-only "a vote is running" page. `vote_item` is the global the vote
// itself carries and its bits are NOT resp.osp_r254's: 1 map, 2 config,
// 4 timelimit, 8 fraglimit, 0x10 hook, 0x20 item toggles (which gets
// Proposal_Menu2 instead of this), 0x40 BFG, 0x80 Quad, 0x100 add one bot,
// 0x200 add N bots, 0x400 remove N bots, 0x800 runes, 0x1000 kick.
// `vote_value` is always the value as a string, so the numeric arms Q_atoi() it.
void OSP_updateProposalMenu(edict_t *ent)
{
    int         bots;
    edict_t     *other;

    // Recount from scratch -- a vote can outlive the clients that started it.
    connected_clients = 0;
    active_clients = 0;
    bots = 0;
    {
        int         i;

        for (i = 1; i <= game.maxclients; i++) {
            other = g_edicts + i;
            if (other->inuse && other->client && other->client->pers.connected) {
                connected_clients++;
                if (other->client->resp.osp_entered == ENTERED_ENTERED)
                    active_clients++;
                if (other->flags & FL_BOTCLIENT)
                    bots++;
            }
        }
    }
    botglobals.numbots = bots;

    if (!connected_clients)
        return;

    pm_line2[0] = 0;

    switch (vote_item) {
    case 1:
        Q_snprintf(pm_line1, sizeof(pm_line1), "Change map to %s", vote_value);
        break;
    case 2:
        Q_snprintf(pm_line1, sizeof(pm_line1), "Set server configuration to");
        Q_snprintf(pm_line2, sizeof(pm_line2), "%s", vote_value);
        break;
    case 4:
        if (!Q_atoi(vote_value))
            Q_snprintf(pm_line1, sizeof(pm_line1), "Change timelimit to OFF");
        else
            Q_snprintf(pm_line1, sizeof(pm_line1), "Change timelimit to %s", vote_value);
        break;
    case 8:
        if (!Q_atoi(vote_value))
            Q_snprintf(pm_line1, sizeof(pm_line1), "Change fraglimit to NONE");
        else
            Q_snprintf(pm_line1, sizeof(pm_line1), "Change fraglimit to %s", vote_value);
        break;
    case 0x10:
        if (Q_atoi(vote_value))
            Q_snprintf(pm_line1, sizeof(pm_line1), "Set the hook to ENABLED");
        else
            Q_snprintf(pm_line1, sizeof(pm_line1), "Set the hook to DISABLED");
        break;
    case 0x800:
        if (Q_atoi(vote_value))
            Q_snprintf(pm_line1, sizeof(pm_line1), "Set runes to ENABLED");
        else
            Q_snprintf(pm_line1, sizeof(pm_line1), "Set runes to DISABLED");
        break;
    case 0x40:
        if (Q_atoi(vote_value))
            Q_snprintf(pm_line1, sizeof(pm_line1), "Set the BFG to ENABLED");
        else
            Q_snprintf(pm_line1, sizeof(pm_line1), "Set the BFG to DISABLED");
        break;
    case 0x80:
        if (Q_atoi(vote_value))
            Q_snprintf(pm_line1, sizeof(pm_line1), "Set the Quad to ENABLED");
        else
            Q_snprintf(pm_line1, sizeof(pm_line1), "Set the Quad to DISABLED");
        break;
    case 0x1000:
        Q_snprintf(pm_line1, sizeof(pm_line1), "Kick UNKNOWN");
        {
            edict_t     *other;
            int         i;

            for (i = 1; i <= game.maxclients; i++) {
                other = g_edicts + i;
                if (!other->inuse || !other->client ||
                    other->client->resp.clientid != Q_atoi(vote_value))
                    continue;

                Q_snprintf(pm_line1, sizeof(pm_line1), "Kick %s", other->client->pers.greenname);
                break;
            }
        }
        break;
    case 0x100:
        Q_snprintf(pm_line1, sizeof(pm_line1), "Add 1 Gladiator bot");
        break;
    case 0x200:
        Q_snprintf(pm_line1, sizeof(pm_line1), "Add %s Gladiator bots", vote_value);
        break;
    case 0x400:
        Q_snprintf(pm_line1, sizeof(pm_line1), "Remove %s Gladiator bots", vote_value);
        break;
    default:
        Q_snprintf(pm_line1, sizeof(pm_line1), "Umm, what were we voting for?");
        break;
    }

    OSP_menuVotePercent(pm_pct, sizeof(pm_pct), pm_needed, sizeof(pm_needed));

    Proposal_Menu[4].text = pm_line1;
    Proposal_Menu[5].text = pm_line2;
    Proposal_Menu[7].text = pm_pct;
    Proposal_Menu[8].text = pm_needed;

    if (OSP_IsTeams())
        Proposal_Menu[12].SelectFunc = OSP_returnMainTeam_menu;
    else
        Proposal_Menu[12].SelectFunc = OSP_returnMainDM_menu;
}

// The item-toggle variant of the proposal page: it has to show all eight
// toggles at once, so it marks the ones the vote would NOT change with a
// leading "*" rather than listing only the differences.
void OSP_updateProposalMenu2(edict_t *ent)
{
    edict_t     *other;
    int         i;
    int         bots;

    connected_clients = 0;
    active_clients = 0;
    bots = 0;
    for (i = 1; i <= game.maxclients; i++) {
        other = g_edicts + i;
        if (other->inuse && other->client && other->client->pers.connected) {
            connected_clients++;
            if (other->client->resp.osp_entered == ENTERED_ENTERED)
                active_clients++;
            if (other->flags & FL_BOTCLIENT)
                bots++;
        }
    }
    botglobals.numbots = bots;

    if (!connected_clients)
        return;

    pm2_line0[0] = 0;
    pm2_line1[0] = 0;
    pm2_line2[0] = 0;
    pm2_line3[0] = 0;
    pm2_line4[0] = 0;
    pm2_line5[0] = 0;
    pm2_line6[0] = 0;
    pm2_line7[0] = 0;

    if ((Q_atoi(vote_value) & 1) == (item_settings & 1))
        Q_strlcpy(pm2_line0, "*", sizeof(pm2_line0));
    if (Q_atoi(vote_value) & 1)
        Q_strlcat(pm2_line0, "Allow Quad: YES", sizeof(pm2_line0));
    else
        Q_strlcat(pm2_line0, "Allow Quad: NO", sizeof(pm2_line0));

    if ((Q_atoi(vote_value) & 2) == (item_settings & 2))
        Q_strlcpy(pm2_line1, "*", sizeof(pm2_line1));
    if (Q_atoi(vote_value) & 2)
        Q_strlcat(pm2_line1, "Allow Invul: YES", sizeof(pm2_line1));
    else
        Q_strlcat(pm2_line1, "Allow Invul: NO", sizeof(pm2_line1));

    if ((Q_atoi(vote_value) & 4) == (item_settings & 4))
        Q_strlcpy(pm2_line2, "*", sizeof(pm2_line2));
    if (Q_atoi(vote_value) & 4)
        Q_strlcat(pm2_line2, "Drop Quad: YES", sizeof(pm2_line2));
    else
        Q_strlcat(pm2_line2, "Drop Quad: NO", sizeof(pm2_line2));

    if ((Q_atoi(vote_value) & 8) == (item_settings & 8))
        Q_strlcpy(pm2_line3, "*", sizeof(pm2_line3));
    if (Q_atoi(vote_value) & 8)
        Q_strlcat(pm2_line3, "Allow BFG: YES", sizeof(pm2_line3));
    else
        Q_strlcat(pm2_line3, "Allow BFG: NO", sizeof(pm2_line3));

    if ((Q_atoi(vote_value) & 0x10) == (item_settings & 0x10))
        Q_strlcpy(pm2_line4, "*", sizeof(pm2_line4));
    if (Q_atoi(vote_value) & 0x10)
        Q_strlcat(pm2_line4, "Allow Power Armor: YES", sizeof(pm2_line4));
    else
        Q_strlcat(pm2_line4, "Allow Power Armor: NO", sizeof(pm2_line4));

    if ((Q_atoi(vote_value) & 0x20) == (item_settings & 0x20))
        Q_strlcpy(pm2_line5, "*", sizeof(pm2_line5));
    if (Q_atoi(vote_value) & 0x20)
        Q_strlcat(pm2_line5, "Weapons Stay: YES", sizeof(pm2_line5));
    else
        Q_strlcat(pm2_line5, "Weapons Stay: NO", sizeof(pm2_line5));

    if ((Q_atoi(vote_value) & 0x40) == (item_settings & 0x40))
        Q_strlcpy(pm2_line6, "*", sizeof(pm2_line6));
    if (Q_atoi(vote_value) & 0x40)
        Q_strlcat(pm2_line6, "Hurt Self: YES", sizeof(pm2_line6));
    else
        Q_strlcat(pm2_line6, "Hurt Self: NO", sizeof(pm2_line6));

    if (OSP_IsTeams()) {
        if ((Q_atoi(vote_value) & 0x80) == (item_settings & 0x80))
            Q_strlcpy(pm2_line7, "*", sizeof(pm2_line7));
        if (Q_atoi(vote_value) & 0x80)
            Q_strlcat(pm2_line7, "Hurt Team: YES", sizeof(pm2_line7));
        else
            Q_strlcat(pm2_line7, "Hurt Team: NO", sizeof(pm2_line7));
    } else
        pm2_line7[0] = 0;

    OSP_menuVotePercent(pm2_pct, sizeof(pm2_pct), pm2_needed,
                        sizeof(pm2_needed));

    Proposal_Menu2[3].text = pm2_line0;
    Proposal_Menu2[4].text = pm2_line1;
    Proposal_Menu2[5].text = pm2_line2;
    Proposal_Menu2[6].text = pm2_line3;
    Proposal_Menu2[7].text = pm2_line4;
    Proposal_Menu2[8].text = pm2_line5;
    Proposal_Menu2[9].text = pm2_line6;
    Proposal_Menu2[10].text = pm2_line7;
    Proposal_Menu2[12].text = pm2_pct;
    Proposal_Menu2[13].text = pm2_needed;

    if (OSP_IsTeams())
        Proposal_Menu2[17].SelectFunc = OSP_returnMainTeam_menu;
    else
        Proposal_Menu2[17].SelectFunc = OSP_returnMainDM_menu;
}

void OSP_menuVotePercent(char *pct, size_t pctsize, char *out, size_t outsize)
{
    OSP_voteSummary(pct, pctsize);
    Q_snprintf(out, outsize, "%d%% Needed to Decide",
               (int)vote_threshold->value);
}

// The five "cycle a value" vote-menu leaves. They share one shape: refuse if
// another item is already staged, step the staged value with resp.osp_r264 as
// the direction, wrap it, then clear the staged-item marker again if the value
// has landed back on what is already in force. resp.osp_r254 holds which item
// is staged, one bit each: 1 map, 2 config, 4 timelimit, 8 fraglimit,
// 0x10 hook, 0x40 named bot, 0x100 bot count, 0x200 runes, 0x20 item toggles.
// "[ SELECT ]" on the voting menu: turn whatever is staged into the equivalent
// `vote <what> <value>` and run it. resp.osp_r254 is the staged-item bit, and
// every arm reads the matching staged value out of client_respawn_t.
void OSP_proposeVote_menu(edict_t *ent, osp_pmenu_t *p)
{
    char        value[16];

    if (!ent->client->resp.osp_r254) {
        gi.cprintf(ent, PRINT_HIGH, "No changes.  No proposal initiated.\n");
        osp_PMenu_Close(ent);
        return;
    }
    if (ent->client->resp.osp_r254 == 1) {
        // the map list can be reloaded by a config vote between the menu
        // opening and this key press
        if (ent->client->resp.osp_r290 >= 0 &&
            ent->client->resp.osp_r290 < map_size && map)
            OSP_vote_cmd(ent, 1, 3, "map",
                         map[ent->client->resp.osp_r290].name);
        osp_PMenu_Close(ent);
        return;
    }
    if (ent->client->resp.osp_r254 == 2 &&
        ent->client->resp.osp_r258 >= 0 &&
        ent->client->resp.osp_r258 < conf_size) {
        // Prefer the config's description; fall back to its filename.
        if (conf_info[ent->client->resp.osp_r258][0])
            OSP_vote_cmd(ent, 1, 3, "config",
                         conf_info[ent->client->resp.osp_r258]);
        else
            OSP_vote_cmd(ent, 1, 3, "config",
                         conf_name[ent->client->resp.osp_r258]);
        osp_PMenu_Close(ent);
        return;
    }
    if (ent->client->resp.osp_r254 == 4) {
        Q_snprintf(value, sizeof(value), "%d", ent->client->resp.osp_r2a0);
        OSP_vote_cmd(ent, 1, 3, "timelimit", value);
        osp_PMenu_Close(ent);
        return;
    }
    if (ent->client->resp.osp_r254 == 8) {
        Q_snprintf(value, sizeof(value), "%d", ent->client->resp.osp_r25c);
        OSP_vote_cmd(ent, 1, 3, "fraglimit", value);
        osp_PMenu_Close(ent);
        return;
    }
    if (ent->client->resp.osp_r254 == 0x10) {
        Q_snprintf(value, sizeof(value), "%d", ent->client->resp.osp_r260);
        OSP_vote_cmd(ent, 1, 3, "hook", value);
        osp_PMenu_Close(ent);
        return;
    }
    if (ent->client->resp.osp_r254 == 0x200) {
        Q_snprintf(value, sizeof(value), "%d", ent->client->resp.osp_r298);
        OSP_vote_cmd(ent, 1, 3, "runes", value);
        osp_PMenu_Close(ent);
        return;
    }
    if (ent->client->resp.osp_r254 == 0x20) {
        Q_snprintf(value, sizeof(value), "%d", ent->client->resp.osp_r2a4);
        OSP_vote_cmd(ent, 1, 3, "toggles", value);
        osp_PMenu_Close(ent);
        return;
    }
    if (ent->client->resp.osp_r254 == 0x400) {
        Q_snprintf(value, sizeof(value), "%d", (char)ent->client->resp.osp_r26c[0]);
        OSP_vote_cmd(ent, 1, 3, "kick", value);
        osp_PMenu_Close(ent);
        return;
    }
    if (ent->client->resp.osp_r254 == 0x40) {
        Q_snprintf(value, sizeof(value), "%d", ent->client->resp.osp_r29c);
        OSP_vote_cmd(ent, 1, 3, "specbot", value);
        osp_PMenu_Close(ent);
        return;
    }
    if (ent->client->resp.osp_r254 == 0x80) {
        Q_snprintf(value, sizeof(value), "%d", ent->client->resp.osp_r250);
        OSP_vote_cmd(ent, 1, 3, "addbots", value);
        osp_PMenu_Close(ent);
        return;
    }

    Q_snprintf(value, sizeof(value), "%d", ent->client->resp.osp_r294);
    OSP_vote_cmd(ent, 1, 3, "rembots", value);
    osp_PMenu_Close(ent);
}

void OSP_changeMap_menu(edict_t *ent, osp_pmenu_t *p)
{
    if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 1) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Sorry, you can change only one item at a time!\n");
        return;
    }

    if (ent->client->resp.osp_r264)
        ent->client->resp.osp_r290--;
    else
        ent->client->resp.osp_r290++;

    ent->client->resp.osp_r254 = 1;

    if (ent->client->resp.osp_r290 == -1)
        ent->client->resp.osp_r254 = 0;
    else if (ent->client->resp.osp_r290 < -1)
        ent->client->resp.osp_r290 = map_size - 1;
    else if (ent->client->resp.osp_r290 == map_size) {
        ent->client->resp.osp_r290 = -1;
        ent->client->resp.osp_r254 = 0;
    }

    OSP_updateVoteMenu(ent);
    osp_PMenu_Sync(ent, Vote_Menu);
    osp_PMenu_Update(ent);
}

void OSP_changeConfig_menu(edict_t *ent, osp_pmenu_t *p)
{
    if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 2) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Sorry, you can change only one item at a time!\n");
        return;
    }

    if (ent->client->resp.osp_r264)
        ent->client->resp.osp_r258--;
    else
        ent->client->resp.osp_r258++;

    ent->client->resp.osp_r254 = 2;

    if (ent->client->resp.osp_r258 == -1)
        ent->client->resp.osp_r254 = 0;
    else if (ent->client->resp.osp_r258 < -1)
        ent->client->resp.osp_r258 = conf_size - 1;
    else if (ent->client->resp.osp_r258 == conf_size) {
        ent->client->resp.osp_r258 = -1;
        ent->client->resp.osp_r254 = 0;
    }

    OSP_updateVoteMenu(ent);
    osp_PMenu_Sync(ent, Vote_Menu);
    osp_PMenu_Update(ent);
}

void OSP_changeTime_menu(edict_t *ent, osp_pmenu_t *p)
{
    if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 4) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Sorry, you can change only one item at a time!\n");
        return;
    }

    if (ent->client->resp.osp_r264)
        ent->client->resp.osp_r2a0 -= (int)menu_timestep->value;
    else
        ent->client->resp.osp_r2a0 += (int)menu_timestep->value;

    if (ent->client->resp.osp_r2a0 < 0)
        ent->client->resp.osp_r2a0 = (int)menu_maxtime->value;
    else if (ent->client->resp.osp_r2a0 > (int)menu_maxtime->value)
        ent->client->resp.osp_r2a0 = 0;

    ent->client->resp.osp_r254 = 4;
    if (ent->client->resp.osp_r2a0 == (int)timelimit->value)
        ent->client->resp.osp_r254 = 0;

    OSP_updateVoteMenu(ent);
    osp_PMenu_Sync(ent, Vote_Menu);
    osp_PMenu_Update(ent);
}

void OSP_changeFrag_menu(edict_t *ent, osp_pmenu_t *p)
{
    if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 8) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Sorry, you can change only one item at a time!\n");
        return;
    }

    if (ent->client->resp.osp_r264)
        ent->client->resp.osp_r25c -= (int)menu_fragstep->value;
    else
        ent->client->resp.osp_r25c += (int)menu_fragstep->value;

    if (ent->client->resp.osp_r25c < 0)
        ent->client->resp.osp_r25c = (int)menu_maxfrag->value;
    else if (ent->client->resp.osp_r25c > (int)menu_maxfrag->value)
        ent->client->resp.osp_r25c = 0;

    ent->client->resp.osp_r254 = 8;
    if (ent->client->resp.osp_r25c == (int)fraglimit->value)
        ent->client->resp.osp_r254 = 0;

    OSP_updateVoteMenu(ent);
    osp_PMenu_Sync(ent, Vote_Menu);
    osp_PMenu_Update(ent);
}

void OSP_changeHook_menu(edict_t *ent, osp_pmenu_t *p)
{
    if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 0x10) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Sorry, you can change only one item at a time!\n");
        return;
    }

    ent->client->resp.osp_r260 = 1 - ent->client->resp.osp_r260;
    ent->client->resp.osp_r254 = 0x10;
    if (ent->client->resp.osp_r260 == (int)hook_enable->value)
        ent->client->resp.osp_r254 = 0;

    OSP_updateVoteMenu(ent);
    osp_PMenu_Sync(ent, Vote_Menu);
    osp_PMenu_Update(ent);
}

void OSP_changeRunes_menu(edict_t *ent, osp_pmenu_t *p)
{
    if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 0x200) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Sorry, you can change only one item at a time!\n");
        return;
    }

    // All five runes on or all five off; there is no per-rune vote.
    if (ent->client->resp.osp_r298)
        ent->client->resp.osp_r298 = 0;
    else
        ent->client->resp.osp_r298 = 0x1f;

    ent->client->resp.osp_r254 = 0x200;
    if (ent->client->resp.osp_r298 == rune_stat ||
        (ent->client->resp.osp_r298 && rune_stat))
        ent->client->resp.osp_r254 = 0;

    OSP_updateVoteMenu(ent);
    osp_PMenu_Sync(ent, Vote_Menu);
    osp_PMenu_Update(ent);
}

// Step the kick target to the next/previous connected client. resp.osp_r268 is
// the client NUMBER, and 0 / -2 are the two ends the backward walk stops and
// wraps at. The chosen client's id and name are cached in resp.osp_r26c so the
// proposal can name them even after they leave.
void OSP_changeKick_menu(edict_t *ent, osp_pmenu_t *p)
{
    edict_t     *other;
    int         i;
    int         n;
    int         was;

    was = ent->client->resp.osp_r268;
    if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 0x400) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Sorry, you can change only one item at a time!\n");
        return;
    }

    if (ent->client->resp.osp_r264) {
        for (i = 1, n = ent->client->resp.osp_r268 - 1;
             i <= game.maxclients; i++, n--) {
            if (!n) {
                // A dead `n = -1`, faithfully reproduced.
                n = -1;
                ent->client->resp.osp_r268 = -1;
                break;
            }
            if (n == -2)
                n = game.maxclients;

            other = g_edicts + n;
            if (!other->inuse || !other->client || other == ent ||
                n == ent->client->resp.osp_r268)
                continue;
            ent->client->resp.osp_r268 = n;
            ent->client->resp.osp_r26c[0] =
                (byte)other->client->resp.clientid;
            Q_strlcpy((char *)&ent->client->resp.osp_r26c[1],
                      other->client->pers.netname,
                      sizeof(ent->client->resp.osp_r26c) - 1);
            break;
        }
        if (ent->client->resp.osp_r268 == was)
            ent->client->resp.osp_r268 = -1;
    } else {
        for (i = 1, n = ent->client->resp.osp_r268 + 1;
             i <= game.maxclients; i++, n++) {
            if (n > game.maxclients) {
                n = -1;
                ent->client->resp.osp_r268 = -1;
                break;
            }
            if (!n)
                n = 1;

            other = g_edicts + n;
            if (!other->inuse || !other->client || other == ent ||
                n == ent->client->resp.osp_r268)
                continue;
            ent->client->resp.osp_r268 = n;
            ent->client->resp.osp_r26c[0] =
                (byte)other->client->resp.clientid;
            Q_strlcpy((char *)&ent->client->resp.osp_r26c[1],
                      other->client->pers.netname,
                      sizeof(ent->client->resp.osp_r26c) - 1);
            break;
        }
        if (ent->client->resp.osp_r268 == was)
            ent->client->resp.osp_r268 = -1;
    }

    ent->client->resp.osp_r254 = 0x400;
    if (ent->client->resp.osp_r268 == -1)
        ent->client->resp.osp_r254 = 0;

    OSP_updateVoteMenu(ent);
    osp_PMenu_Sync(ent, Vote_Menu);
    osp_PMenu_Update(ent);
}

void OSP_changeItems_menu(edict_t *ent, osp_pmenu_t *p)
{
    int     bit;

    bit = *(int *)p->arg;
    if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 0x20) {
        gi.cprintf(ent, PRINT_HIGH,
                   "Sorry, you can change only one item at a time!\n");
        return;
    }

    ent->client->resp.osp_r2a4 ^= bit;
    ent->client->resp.osp_r254 = 0x20;
    if (ent->client->resp.osp_r2a4 == item_settings)
        ent->client->resp.osp_r254 = 0;

    OSP_updateVoteMenu2(ent);
    osp_PMenu_Sync(ent, Vote_Menu2);
    osp_PMenu_Update(ent);
}

// Cycle through the names in the bot config file. Selecting a name clears the
// "add N random bots" staging and vice versa, which is why each of the two
// resets the other's field before touching its own.
void OSP_addSpecificBot_menu(edict_t *ent, osp_pmenu_t *p)
{
    bot_t       *b;
    int         ncount;
    int         t;

    if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 0x40) {
        ent->client->resp.osp_r250 = 0;
        ent->client->resp.osp_r294 = 0;
    }

    for (ncount = 0, b = botlist; b; b = b->next, ncount++)
        ;

    if (ent->client->resp.osp_r264)
        ent->client->resp.osp_r29c--;
    else
        ent->client->resp.osp_r29c++;

    if (!ncount)
        ent->client->resp.osp_r29c = -1;
    else if (ent->client->resp.osp_r29c < -1)
        ent->client->resp.osp_r29c = ncount - 1;
    else if (ent->client->resp.osp_r29c >= ncount)
        ent->client->resp.osp_r29c = -1;

    if (ent->client->resp.osp_r29c == -1 || !ncount) {
        ent->client->resp.osp_r254 = 0;
        voted_botname[0] = 0;
    } else {
        for (t = 0, b = botlist; b && t < ent->client->resp.osp_r29c; b = b->next, t++)
            ;
        Q_strlcpy(voted_botname, b->name, sizeof(voted_botname));
        ent->client->resp.osp_r254 = 0x40;
    }

    OSP_updateBotMenu(ent);
    osp_PMenu_Sync(ent, Bot_Menu);
    osp_PMenu_Update(ent);
}

void OSP_addBots_menu(edict_t *ent, osp_pmenu_t *p)
{
    if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 0x80) {
        ent->client->resp.osp_r294 = 0;
        ent->client->resp.osp_r29c = -1;
    }

    if (ent->client->resp.osp_r264)
        ent->client->resp.osp_r250--;
    else
        ent->client->resp.osp_r250++;

    if (ent->client->resp.osp_r250 < 0)
        ent->client->resp.osp_r250 = (int)vote_bots_max->value - bots_votedin;

    if (ent->client->resp.osp_r250 + connected_clients > (int)game.maxclients)
        ent->client->resp.osp_r250 = (int)game.maxclients - connected_clients;
    else if (ent->client->resp.osp_r250 >
             (int)vote_bots_max->value - bots_votedin)
        ent->client->resp.osp_r250 = 0;

    if (!ent->client->resp.osp_r250)
        ent->client->resp.osp_r254 = 0;
    else
        ent->client->resp.osp_r254 = 0x80;

    OSP_updateBotMenu(ent);
    osp_PMenu_Sync(ent, Bot_Menu);
    osp_PMenu_Update(ent);
}

void OSP_removeBots_menu(edict_t *ent, osp_pmenu_t *p)
{
    if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 0x100) {
        ent->client->resp.osp_r250 = 0;
        ent->client->resp.osp_r29c = -1;
    }

    if (ent->client->resp.osp_r264)
        ent->client->resp.osp_r294--;
    else
        ent->client->resp.osp_r294++;

    if (ent->client->resp.osp_r294 < 0)
        ent->client->resp.osp_r294 = bots_votedin;
    else if (ent->client->resp.osp_r294 > bots_votedin)
        ent->client->resp.osp_r294 = 0;

    if (!ent->client->resp.osp_r294)
        ent->client->resp.osp_r254 = 0;
    else
        ent->client->resp.osp_r254 = 0x100;

    OSP_updateBotMenu(ent);
    osp_PMenu_Sync(ent, Bot_Menu);
    osp_PMenu_Update(ent);
}

void OSP_acceptVote_menu(edict_t *ent, osp_pmenu_t *p)
{
    OSP_yes_cmd(ent);
    osp_PMenu_Close(ent);
}

void OSP_declineVote_menu(edict_t *ent, osp_pmenu_t *p)
{
    OSP_no_cmd(ent);
    osp_PMenu_Close(ent);
}

int OSP_updateInviteMenu(edict_t *ent)
{
    Q_snprintf(invite_teamname, sizeof(invite_teamname), "%s",
               OSP_teamNameFor(ent->client->resp.osp_r2cc));
    invite_teamnum = ent->client->resp.osp_r078 - 1;
    Invite_Menu[7].text = invite_teamname;
    Invite_Menu[11].arg = &invite_teamnum;
    return 11;
}

void OSP_inviteClose_menu(edict_t *ent, osp_pmenu_t *p)
{
    edict_t     *other;
    int         i;
    int         tnum;

    // The same value is recomputed just below, unread in between -- a dead
    // first store, faithfully reproduced.
    tnum = ent->client->resp.osp_r078 - 1;
    osp_PMenu_Close(ent);
    gi.cprintf(ent, PRINT_HIGH, "Invitation declined.\n");

    tnum = ent->client->resp.osp_r078 - 1;
    for (i = 1; i <= game.maxclients; i++) {
        other = g_edicts + i;
        if (!other->inuse || !other->client ||
            other->client->resp.osp_entered != ENTERED_ENTERED ||
            other->client->resp.team != tnum ||
            !other->client->resp.osp_r2c4)
            continue;

        gi.cprintf(other, PRINT_HIGH, "%s has declined your invitation.\n",
                   ent->client->pers.greenname);
    }

    ent->client->resp.osp_r078 = 0;
}

int OSP_updateAdminMenu(edict_t *ent)
{
    if (OSP_IsMatch()) {
        Q_snprintf(admin_title, sizeof(admin_title), "*Match Control");
        AdminMain_Menu[9].SelectFunc = NULL;
        if (OSP_IsTeams())
            AdminMain_Menu[12].SelectFunc = OSP_returnMainTeam_menu;
        else
            AdminMain_Menu[12].SelectFunc = OSP_returnMainDM_menu;
    } else {
        Q_snprintf(admin_title, sizeof(admin_title), " ");
        AdminMain_Menu[9].SelectFunc = NULL;
        AdminMain_Menu[12].SelectFunc = OSP_returnMainDM_menu;
    }

    AdminMain_Menu[9].text = admin_title;
    AdminMain_Menu[4].arg = &admin_mode_map;
    AdminMain_Menu[5].arg = &admin_mode_ban;
    AdminMain_Menu[7].arg = &admin_mode_kick;

    if (OSP_IsMatch())
        return 9;
    return 4;
}

void OSP_returnMainAdmin_menu(edict_t *ent, osp_pmenu_t *p)
{
    osp_PMenu_Close(ent);
    OSP_adminMenu(ent);
}

// The referee's select-a-thing page. resp.osp_r238 says which of the three
// admin actions is being set up and rewrites the whole menu for it; anything
// else is the mod's own "[ ERROR ERROR ]" fallback. Note the player arms index
// `game.clients` by client NUMBER while the address line indexes `g_edicts` by
// the same number plus one.
int OSP_updateAdminSelectMenu(edict_t *ent)
{
    int     which = ent->client->resp.osp_r238;     // invented name

    if (which == 1) {
        Q_strlcpy(as_title, "[ KICK Player Menu ]", sizeof(as_title));
        Q_strlcpy(as_prompt, "*Select player to KICK:", sizeof(as_prompt));

        if (ent->client->resp.osp_r290 == -1) {
            Q_strlcpy(as_choice, "*[ SELECT ]", sizeof(as_choice));
            Q_strlcpy(as_addr, " ", sizeof(as_addr));
            Q_strlcpy(as_action, " ", sizeof(as_action));
            AdminSelect_Menu[11].SelectFunc = NULL;
        } else {
            Q_snprintf(as_choice, sizeof(as_choice), "%s",
                    game.clients[ent->client->resp.osp_r290].pers.netname);
            Q_snprintf(as_addr, sizeof(as_addr), "[ %s ]",
                    game.clients[ent->client->resp.osp_r290].pers.address);
            Q_strlcpy(as_action, "*KICK selected player", sizeof(as_action));
            AdminSelect_Menu[11].SelectFunc = OSP_playerAdminChoose;
        }
        AdminSelect_Menu[4].SelectFunc = OSP_playerAdminSelect_menu;
    } else if (which == 2) {
        Q_strlcpy(as_title, "[ *BAN* Player Menu ]", sizeof(as_title));
        Q_strlcpy(as_prompt, "*Select player to *BAN*:", sizeof(as_prompt));

        if (ent->client->resp.osp_r290 == -1) {
            Q_strlcpy(as_choice, "*[ SELECT ]", sizeof(as_choice));
            Q_strlcpy(as_addr, " ", sizeof(as_addr));
            Q_strlcpy(as_action, " ", sizeof(as_action));
            AdminSelect_Menu[11].SelectFunc = NULL;
        } else {
            Q_snprintf(as_choice, sizeof(as_choice), "%s",
                    game.clients[ent->client->resp.osp_r290].pers.netname);
            Q_snprintf(as_addr, sizeof(as_addr), "[ %s ]",
                    game.clients[ent->client->resp.osp_r290].pers.address);
            Q_strlcpy(as_action, "*BAN selected player", sizeof(as_action));
            AdminSelect_Menu[11].SelectFunc = OSP_playerAdminChoose;
        }
        AdminSelect_Menu[4].SelectFunc = OSP_playerAdminSelect_menu;
    } else if (which == 4) {
        Q_strlcpy(as_title, "[ Map Selection Menu ]", sizeof(as_title));
        Q_strlcpy(as_prompt, "*Select new map to load:", sizeof(as_prompt));
        Q_strlcpy(as_addr, " ", sizeof(as_addr));
        AdminSelect_Menu[4].SelectFunc = OSP_mapAdminSelect_menu;
        AdminSelect_Menu[11].SelectFunc = OSP_mapAdminChoose;

        if (!map_size) {
            Q_snprintf(as_choice, sizeof(as_choice), "[ NO MAPS AVAILABLE ]");
            Q_snprintf(as_action, sizeof(as_action), " ");
            AdminSelect_Menu[4].SelectFunc = NULL;
            AdminSelect_Menu[11].SelectFunc = NULL;
        } else if (ent->client->resp.osp_r290 == -1) {
            Q_strlcpy(as_choice, "*[ SELECT ]", sizeof(as_choice));
            Q_snprintf(as_action, sizeof(as_action), " ");
            AdminSelect_Menu[11].SelectFunc = NULL;
        } else {
            Q_snprintf(as_choice, sizeof(as_choice), "%s",
                   ent->client->resp.osp_r290 < map_size ?
                   map[ent->client->resp.osp_r290].name : "?");
            Q_strlcpy(as_action, "*Load selected map", sizeof(as_action));
            AdminSelect_Menu[11].SelectFunc = OSP_mapAdminChoose;
        }
    } else {
        Q_strlcpy(as_title, "[ ERROR ERROR ]", sizeof(as_title));
        Q_strlcpy(as_prompt, "*ERROR IN ADMIN MENU", sizeof(as_prompt));
        Q_strlcpy(as_choice, " ", sizeof(as_choice));
        Q_strlcpy(as_addr, " ", sizeof(as_addr));
    }

    AdminSelect_Menu[1].text = as_title;
    AdminSelect_Menu[4].text = as_prompt;
    AdminSelect_Menu[6].text = as_choice;
    AdminSelect_Menu[7].text = as_addr;
    AdminSelect_Menu[11].text = as_action;

    if (OSP_IsMatch())
        return 9;
    return 4;
}

void OSP_mapAdminSelect_menu(edict_t *ent, osp_pmenu_t *p)
{
    if (!OSP_refereeOnly(ent))
        return;

    if (ent->client->resp.osp_r264)
        ent->client->resp.osp_r290--;
    else
        ent->client->resp.osp_r290++;

    if (ent->client->resp.osp_r290 < -1)
        ent->client->resp.osp_r290 = map_size - 1;
    else if (ent->client->resp.osp_r290 == map_size) {
        ent->client->resp.osp_r290 = -1;
        ent->client->resp.osp_r254 = 0;
    }

    OSP_updateAdminSelectMenu(ent);
    osp_PMenu_Sync(ent, AdminSelect_Menu);
    osp_PMenu_Update(ent);
}

// Step to the next/previous connected client. resp.osp_r290 holds a client
// NUMBER here rather than an index into a table, so the search skips free
// edicts and half-connected slots in whichever direction it is going.
void OSP_playerAdminSelect_menu(edict_t *ent, osp_pmenu_t *p)
{
    edict_t     *other;
    int         i;
    int         found;

    if (!OSP_refereeOnly(ent))
        return;

    found = -1;
    if (ent->client->resp.osp_r264) {
        for (i = ent->client->resp.osp_r290 - 1; i >= 0; i--) {
            other = g_edicts + i + 1;
            if (!(other->inuse && other->client && other->client->pers.connected))
                continue;
            found = i;
            break;
        }
    } else {
        for (i = ent->client->resp.osp_r290 + 1; i < game.maxclients; i++) {
            other = g_edicts + i + 1;
            if (!(other->inuse && other->client && other->client->pers.connected))
                continue;
            found = i;
            break;
        }
    }

    ent->client->resp.osp_r290 = found;
    OSP_updateAdminSelectMenu(ent);
    osp_PMenu_Sync(ent, AdminSelect_Menu);
    osp_PMenu_Update(ent);
}

// A referee picking a map out of the admin menu ends the level immediately --
// the same "soft" end a map vote uses, so the stats logs get a game-end record
// before the change.
void OSP_mapAdminChoose(edict_t *ent, osp_pmenu_t *p)
{
    int         sel;

    // The acting leaf: this one ends the level.
    if (!OSP_refereeOnly(ent))
        return;

    sel = ent->client->resp.osp_r290;
    osp_PMenu_Close(ent);

    if (sel > -1 && sel < map_size && map &&
        OSP_mapExists(ent, map[sel].name, true)) {
        sl_SoftGameEnd(&gi, level);
        OSP_Stats_MatchEnd("referee map change");
        manual_map = 1;
        G_EndLevel();
        return;
    }

    OSP_adminMenu(ent);
}

// The referee's admin-select leaf. resp.osp_r238 says which admin action the
// selection is for -- 2 is a ban (which hands off to OSP_rban_cmd), anything
// else is a kick, and a kicked bot goes through the Gladiator SDK's
// `removebot` rather than an svc_disconnect.
void OSP_playerAdminChoose(edict_t *ent, osp_pmenu_t *p)
{
    edict_t     *target = g_edicts + ent->client->resp.osp_r290 + 1;

    // The acting leaf: this one kicks or bans, and OSP_rban_cmd has no check
    // of its own because the dispatcher is what gates `r_ban`.
    if (!OSP_refereeOnly(ent))
        return;

    if (ent->client->resp.osp_r290 > -1 && target->client) {
        if (ent->client->resp.osp_r238 == 2)
            OSP_rban_cmd(ent, target->client->pers.netname);
        else if (target == ent)
            gi.cprintf(ent, PRINT_HIGH, "Sorry, you can't kick yourself!\n");
        else if (!target->inuse || !target->client ||
                 !target->client->pers.connected)
            gi.cprintf(ent, PRINT_HIGH,
                       "Sorry, player is no longer connected!\n");
        else {
            gi.bprintf(PRINT_CHAT, "%s has been kicked!\n",
                       target->client->pers.netname);

            if (server_log) {
                OSP_logAdminLog("Referee_Kick: %s -> %s [%s]",
                                ent->client->pers.netname,
                                target->client->pers.netname,
                                target->client->pers.address);
            }

            if (target->flags & FL_BOT) {
                BotServerCommand("sv", "removebot",
                                 target->client->pers.netname, 0);
                // The target's own oddity, the same five instructions as in
                // OSP_kickplayer_cmd: the counter is subtracted from itself and
                // the (always zero) result clamped.
                bots_votedin -= bots_votedin;
                if (bots_votedin < 0)
                    bots_votedin = 0;
            } else {
                OSP_startObserve(target);
                gi.WriteByte(svc_disconnect);
                gi.unicast(target, true);
                ClientDisconnect(target);
            }
        }
    }

    osp_PMenu_Close(ent);
    OSP_adminMenu(ent);
}
