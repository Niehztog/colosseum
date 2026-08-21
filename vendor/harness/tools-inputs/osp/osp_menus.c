// osp_menus.c -- <INVENTED FILENAME>. The mod's pop-up menu callbacks.
//
// Thirteen pmenu_t tables (id CTF's p_menu.c engine) and the callbacks behind
// their entries: an opener per menu, an `OSP_update*Menu` per menu that
// rebuilds the entry text for one client, and a leaf callback per selectable
// line.  The `m_mode` global picks team play from plain DM, which is what
// decides whether "back" goes to Team_Menu or RegDM_Menu.

#include "g_local.h"
#include "bl_main.h"
#include "bl_botcfg.h"

pmenu_t	Team_Menu[18] =
{
	{ "*OSP Tourney DeathMatch",                 PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "[ Teamplay Mode ]",                       PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "*Join Team A 1234567890",                 PMENU_ALIGN_LEFT,    NULL, OSP_joinTeam_menu },
	{ NULL,                                      PMENU_ALIGN_RIGHT,   NULL, NULL },
	{ "*Join Team B 1234567890",                 PMENU_ALIGN_LEFT,    NULL, OSP_joinTeam_menu },
	{ NULL,                                      PMENU_ALIGN_RIGHT,   NULL, NULL },
	{ "*Admin Menu",                             PMENU_ALIGN_LEFT,    NULL, OSP_returnMainAdmin_menu },
	{ "*Voting Menu",                            PMENU_ALIGN_LEFT,    NULL, OSP_voteMenu },
	{ "*Enter Observer Mode",                    PMENU_ALIGN_LEFT,    NULL, OSP_changeObserve },
	{ "*Enter Chasecam Mode",                    PMENU_ALIGN_LEFT,    NULL, OSP_changeChase },
	{ "*Player ID",                              PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*View ngStats (browser)",                 PMENU_ALIGN_LEFT,    NULL, OSP_ngStatsView },
	{ "*Help",                                   PMENU_ALIGN_LEFT,    NULL, OSP_helpMenu },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Use [ and ] to move cursor",              PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Press ENTER to select",                   PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "*v(2.75)",                                PMENU_ALIGN_RIGHT,   NULL, NULL },
};
pmenu_t	RegDM_Menu[18] =
{
	{ "*OSP Tourney DeathMatch",                 PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "[ Regular DM Mode ]",                     PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*Enter the Game",                         PMENU_ALIGN_LEFT,    NULL, OSP_dmReturn_menu },
	{ "*Voting Menu",                            PMENU_ALIGN_LEFT,    NULL, OSP_voteMenu },
	{ "*Admin Menu",                             PMENU_ALIGN_LEFT,    NULL, OSP_returnMainAdmin_menu },
	{ "*Enter Observer Mode",                    PMENU_ALIGN_LEFT,    NULL, OSP_changeObserve },
	{ "*Enter Chasecam Mode",                    PMENU_ALIGN_LEFT,    NULL, OSP_changeChase },
	{ "*View ngStats (browser)",                 PMENU_ALIGN_LEFT,    NULL, OSP_ngStatsView },
	{ "*Player ID",                              PMENU_ALIGN_LEFT,    NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*Help",                                   PMENU_ALIGN_LEFT,    NULL, OSP_helpMenu },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Use [ and ] to move cursor",              PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Press ENTER to select",                   PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "*v(2.75)",                                PMENU_ALIGN_RIGHT,   NULL, NULL },
};
pmenu_t	AdminMain_Menu[17] =
{
	{ "*OSP Tourney DeathMatch",                 PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "[ Main Admin Menu ]",                     PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*Kick Player",                            PMENU_ALIGN_LEFT,    NULL, OSP_adminSelectMenu },
	{ "*Ban Player",                             PMENU_ALIGN_LEFT,    NULL, OSP_adminSelectMenu },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*Change Map",                             PMENU_ALIGN_LEFT,    NULL, OSP_adminSelectMenu },
	{ "*Change Server Settings",                 PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*Match Controls",                         PMENU_ALIGN_LEFT,    NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*Exit Admin Controls",                    PMENU_ALIGN_LEFT,    NULL, OSP_returnMainTeam_menu },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Use [ and ] to move cursor",              PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Press ENTER to select",                   PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "*v(2.75)",                                PMENU_ALIGN_RIGHT,   NULL, NULL },
};
pmenu_t	AdminSelect_Menu[17] =
{
	{ "*OSP Tourney DeathMatch",                 PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "[ Map Selection Menu 000 ]",              PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*Select new map to load:00",              PMENU_ALIGN_LEFT,    NULL, OSP_mapAdminSelect_menu },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Mapname1234567890123456789",              PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*Load selected map",                      PMENU_ALIGN_LEFT,    NULL, OSP_mapAdminChoose },
	{ "*Return to main menu",                    PMENU_ALIGN_LEFT,    NULL, OSP_returnMainAdmin_menu },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Use [ and ] to move cursor",              PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Press ENTER to select",                   PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "*v(2.75)",                                PMENU_ALIGN_RIGHT,   NULL, NULL },
};
pmenu_t	Vote_Menu[19] =
{
	{ "*OSP Tourney DeathMatch",                 PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "[ Voting Menu ]",                         PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "Map: q2dm12345678901234",                 PMENU_ALIGN_LEFT,    NULL, OSP_changeMap_menu },
	{ "Config: 1234567890123456789012345689012", PMENU_ALIGN_LEFT,    NULL, OSP_changeConfig_menu },
	{ "Item toggles...",                         PMENU_ALIGN_LEFT,    NULL, OSP_voteMenu2 },
	{ "Gladiator Bots...",                       PMENU_ALIGN_LEFT,    NULL, OSP_botMenu },
	{ "TimeLimit: 9999",                         PMENU_ALIGN_LEFT,    NULL, OSP_changeTime_menu },
	{ "FragLimit: 9999",                         PMENU_ALIGN_LEFT,    NULL, OSP_changeFrag_menu },
	{ "The Hook: [DISABLED]",                    PMENU_ALIGN_LEFT,    NULL, OSP_changeHook_menu },
	{ "Runes: [DISABLED]",                       PMENU_ALIGN_LEFT,    NULL, OSP_changeRunes_menu },
	{ "Kick: 12345678901234567",                 PMENU_ALIGN_LEFT,    NULL, OSP_changeKick_menu },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*Propose Change",                         PMENU_ALIGN_LEFT,    NULL, OSP_proposeVote_menu },
	{ "*Return to Main Menu",                    PMENU_ALIGN_LEFT,    NULL, OSP_returnMainTeam_menu },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Use [ and ] to move cursor",              PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "ENTR/BKSP selects fwd/back",              PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*v(2.75)",                                PMENU_ALIGN_RIGHT,   NULL, NULL },
};
pmenu_t	Vote_Menu2[18] =
{
	{ "*OSP Tourney DeathMatch",                 PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "[ Item Voting Menu ]",                    PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "Allow Quad: YES",                         PMENU_ALIGN_LEFT,    NULL, OSP_changeItems_menu },
	{ "Allow Invul: YES",                        PMENU_ALIGN_LEFT,    NULL, OSP_changeItems_menu },
	{ "Quad Drop: YES",                          PMENU_ALIGN_LEFT,    NULL, OSP_changeItems_menu },
	{ "Allow BFG: YES",                          PMENU_ALIGN_LEFT,    NULL, OSP_changeItems_menu },
	{ "Allow Power Armor: YES",                  PMENU_ALIGN_LEFT,    NULL, OSP_changeItems_menu },
	{ "Weapons Stay: YES",                       PMENU_ALIGN_LEFT,    NULL, OSP_changeItems_menu },
	{ "Hurt Self: YES",                          PMENU_ALIGN_LEFT,    NULL, OSP_changeItems_menu },
	{ "Hurt Team: YES",                          PMENU_ALIGN_LEFT,    NULL, OSP_changeItems_menu },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*Propose Change",                         PMENU_ALIGN_LEFT,    NULL, OSP_proposeVote_menu },
	{ "*Return to Main Menu",                    PMENU_ALIGN_LEFT,    NULL, OSP_returnMainTeam_menu },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Use [ and ] to move cursor",              PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "ENTR/BKSP selects fwd/back",              PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*v(2.75)",                                PMENU_ALIGN_RIGHT,   NULL, NULL },
};
pmenu_t	Bot_Menu[18] =
{
	{ "*OSP Tourney DeathMatch",                 PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "[ Gladiator Bots Menu ]",                 PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "*Add specific bot:",                      PMENU_ALIGN_LEFT,    NULL, OSP_addSpecificBot_menu },
	{ "GB|12345678901234567",                    PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Add random bots: 99",                     PMENU_ALIGN_LEFT,    NULL, OSP_addBots_menu },
	{ "Remove random bots: 99",                  PMENU_ALIGN_LEFT,    NULL, OSP_removeBots_menu },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Total active bots: 99",                   PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*Propose Change",                         PMENU_ALIGN_LEFT,    NULL, OSP_proposeVote_menu },
	{ "*Return to Main Menu",                    PMENU_ALIGN_LEFT,    NULL, OSP_returnMainTeam_menu },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Use [ and ] to move cursor",              PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "ENTR/BKSP selects fwd/back",              PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*v(2.75)",                                PMENU_ALIGN_RIGHT,   NULL, NULL },
};
pmenu_t	Proposal_Menu[18] =
{
	{ "*OSP Tourney DeathMatch",                 PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "[ Proposal Selection ]",                  PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "*Proposal: ",                             PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Change map to q2dm8 blah",                PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "12345678901234567890123",                 PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "50% Accepted, 50% Declined",              PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "51% Needed to Decide",                    PMENU_ALIGN_LEFT,    NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*Accept Proposal",                        PMENU_ALIGN_LEFT,    NULL, OSP_acceptVote_menu },
	{ "*Decline Proposal",                       PMENU_ALIGN_LEFT,    NULL, OSP_declineVote_menu },
	{ "*Return to Main Menu",                    PMENU_ALIGN_LEFT,    NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Use [ and ] to move cursor",              PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Press ENTER to select",                   PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "*v(2.75)",                                PMENU_ALIGN_RIGHT,   NULL, NULL },
};
pmenu_t	Proposal_Menu2[18] =
{
	{ "*OSP Tourney DeathMatch",                 PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "[ Proposal Selection ]",                  PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "Allow Quad: YESNO",                       PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Allow Invul: YESNO",                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Drop Quad: YESNO",                        PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Allow BFG: YESNO",                        PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Allow Power Armor: YESNO",                PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Weapons Stay: YESNO",                     PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Hurt Self: YESNO",                        PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Hurt Team: YESNO",                        PMENU_ALIGN_LEFT,    NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "50% Accepted, 50% Declined",              PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "51% Needed to Decide",                    PMENU_ALIGN_LEFT,    NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*Accept Proposal",                        PMENU_ALIGN_LEFT,    NULL, OSP_acceptVote_menu },
	{ "*Decline Proposal",                       PMENU_ALIGN_LEFT,    NULL, OSP_declineVote_menu },
	{ "*Return to Main Menu",                    PMENU_ALIGN_CENTER,  NULL, NULL },
};
pmenu_t	Help_Menu[18] =
{
	{ "*OSP Tourney DeathMatch",                 PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "*[ Console Commands ]",                   PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "vote: Vote on/make changes",              PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "yes/no: Accept/deny vote",                PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "hook/unhook: Uses grapple",               PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "chasecam: Toggle chasecam",               PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "observe: Toggle observer",                PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "id: Toggle player ID",                    PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "motd: Message Of The Day",                PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "referee: Enable ref cmds",                PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "stats: Show player stats",                PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "oldscore: Show last scores",              PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "oldstats: Show last stats",               PMENU_ALIGN_LEFT,    NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*Show Match Commands",                    PMENU_ALIGN_LEFT,    NULL, OSP_help2Menu },
	{ "*Show Team/1v1 Commands",                 PMENU_ALIGN_LEFT,    NULL, OSP_help3Menu },
	{ "*Return to Main Menu",                    PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*v(2.75)",                                PMENU_ALIGN_RIGHT,   NULL, NULL },
};
pmenu_t	Help2_Menu[18] =
{
	{ "*OSP Tourney DeathMatch",                 PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "*[ Match Commands ]",                     PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "ready: Switch to READY",                  PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "       status",                           PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "notready: Switch to",                     PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "          NOTREADY status",               PMENU_ALIGN_LEFT,    NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "time: Call timeout/timein",               PMENU_ALIGN_LEFT,    NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "matchinfo: Show all match",               PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "           parameters",                   PMENU_ALIGN_LEFT,    NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*Show Console Commands",                  PMENU_ALIGN_LEFT,    NULL, OSP_helpMenu },
	{ "*Show Team/1v1 Commands",                 PMENU_ALIGN_LEFT,    NULL, OSP_help3Menu },
	{ "*Return to Main Menu",                    PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*v(2.75)",                                PMENU_ALIGN_RIGHT,   NULL, NULL },
};
pmenu_t	Help3_Menu[18] =
{
	{ "*OSP Tourney DeathMatch",                 PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "*[ Team/1v1 Commands ]",                  PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "teamname: Change teamname",               PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "teamskin: Change teamskin",               PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "join: Join specified team",               PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "switchteam: Switch teams",                PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "captain: Show/set team capt",             PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "lockteam: Lock team",                     PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "unlockteam: Unlock team",                 PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "invite: Invite a player",                 PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "remove: Remove teammate",                 PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "joincode: View/set/use",                  PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "          team's joincode",               PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*Show Console Commands",                  PMENU_ALIGN_LEFT,    NULL, OSP_helpMenu },
	{ "*Show Match Commands",                    PMENU_ALIGN_LEFT,    NULL, OSP_help2Menu },
	{ "*Return to Main Menu",                    PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*v(2.75)",                                PMENU_ALIGN_RIGHT,   NULL, NULL },
};
pmenu_t	Invite_Menu[18] =
{
	{ "*OSP Tourney DeathMatch",                 PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "[ Invitation Menu ]",                     PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "*You have been invited",                  PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "*to join team:",                          PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_RIGHT,   NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_RIGHT,   NULL, NULL },
	{ "Team_A Team_B Team_C oo",                 PMENU_ALIGN_CENTER,  NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_RIGHT,   NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "*Accept Invitation",                      PMENU_ALIGN_LEFT,    NULL, OSP_joinTeam_menu },
	{ "*Deny Invitation",                        PMENU_ALIGN_LEFT,    NULL, OSP_inviteClose_menu },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ NULL,                                      PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Use [ and ] to move cursor",              PMENU_ALIGN_LEFT,    NULL, NULL },
	{ "Press ENTER to select",                   PMENU_ALIGN_CENTER,  NULL, NULL },
	{ "*v(2.75)",                                PMENU_ALIGN_RIGHT,   NULL, NULL },
};
char	voted_botname[32];


// The menu text the update functions build.  Every *_Menu table entry's `text`
// is just a pointer, so the builders sprintf into file statics and point the
// entries at them.  <INVENTED NAMES>.
static char	pm_line1[32];
static char	pm_line2[32];
static char	pm_pct[32];
static char	pm_needed[32];
static char	pm2_pct[32];
static char	pm2_needed[32];
static char	pm2_line0[32];
static char	pm2_line1[32];
static char	pm2_line2[32];
static char	pm2_line3[32];
static char	pm2_line4[32];
static char	pm2_line5[32];
static char	pm2_line6[32];
static char	pm2_line7[32];
static char	vm_map[32];
static char	vm_config[32];
static char	vm_toggles[32];
static char	vm_bots[32];
static char	vm_time[32];
static char	vm_frag[32];
static char	vm_hook[32];
static char	vm_runes[32];
static char	vm_kick[32];
static int	tm_teamnum0;
static int	tm_teamnum1;
static char	tm_title[32];
static char	tm_join[2][32];
static char	tm_count0[32];
static char	tm_count1[32];
static char	tm_admin[32];
static char	tm_obs[32];
static char	tm_chase[32];
static char	tm_hud[32];
static char	tm_id[32];
static char	tm_vote[32];
static char	as_title[32];
static char	as_prompt[32];
static char	as_choice[32];
static char	as_addr[32];
static char	as_action[32];
static char	dm_play_line[32];
static char	dm_admin_line[32];
static char	dm_id_line[32];
static char	dm_vote_line[32];
static char	dm_obs_line[32];
static char	dm_chase_line[32];
static char	dm_hud_line[32];
static int	v2_bits0;
static int	v2_bits1;
static int	v2_bits2;
static int	v2_bits3;
static int	v2_bits4;
static int	v2_bits5;
static int	v2_bits6;
static int	v2_bits7;
static char	v2_line0[32];
static char	v2_line1[32];
static char	v2_line2[32];
static char	v2_line3[32];
static char	v2_line4[32];
static char	v2_line5[32];
static char	v2_line6[32];
static char	v2_line7[32];
static char	bot_name_line[32];
static char	bot_add_line[32];
static char	bot_rem_line[32];
static char	bot_total_line[32];
static int	bot_add_arg;
static int	bot_rem_arg;
static char	invite_teamname[32];
static int	invite_teamnum;
static char	admin_title[32];
// A SECOND 32-byte line static here, and nothing in the image references it:
// real's .bss gives this position 64 bytes where one line needs 32, and the
// next TU's block starts exactly 0x40 on.  Only its existence and size are
// evidence -- the name is <INVENTED>.
static char	admin_unused_line[32];

// <INVENTED NAMES>: the three ints the AdminMain_Menu select entries carry,
// dereferenced into resp.osp_r238.
static int	admin_mode_map = 1;
static int	admin_mode_ban = 2;
static int	admin_mode_kick = 4;


// gamex86.dll: 100309E0..10030A2B
// gamei386.so: 0005DAFC..0005DB40
void OSP_teamMenu (edict_t *ent)
{
	int		cur;

	if (ent->client->inmenu)
		PMenu_Close (ent);
	else
	{
		cur = OSP_updateTeamMenu (ent);
		PMenu_Open (ent, Team_Menu, cur, 18);
	}
}

// gamex86.dll: 10030A2B..10030A76
// gamei386.so: 0005DB40..0005DB84
void OSP_DMMenu (edict_t *ent)
{
	int		cur;

	if (ent->client->inmenu)
		PMenu_Close (ent);
	else
	{
		cur = OSP_updateDMMenu (ent);
		PMenu_Open (ent, RegDM_Menu, cur, 18);
	}
}

// gamex86.dll: 10030A76..10030AC1
// gamei386.so: 0005DB84..0005DBC8
void OSP_adminMenu (edict_t *ent)
{
	int		cur;

	if (ent->client->inmenu)
		PMenu_Close (ent);
	else
	{
		cur = OSP_updateAdminMenu (ent);
		PMenu_Open (ent, AdminMain_Menu, cur, 17);
	}
}

// gamex86.dll: 10030AC1..10030B17
// gamei386.so: 0005DBC8..0005DC1F
void OSP_adminSelectMenu (edict_t *ent, pmenu_t *p)
{
	PMenu_Close (ent);
	ent->client->resp.osp_r238 = *(int *)p->arg;
	ent->client->resp.osp_r290 = -1;
	OSP_updateAdminSelectMenu (ent);
	PMenu_Open (ent, AdminSelect_Menu, 6, 17);
}

// The voting menu opener. An observer may not open it while there are humans
// playing -- `active_clients - botglobals.numbots` is "somebody other than a
// bot is in the game". Once a vote is running the menu becomes read-only: the
// Proposal menu shows what is on the table, and vote_item 0x20 (the item
// toggles) gets its own wider variant.
// gamex86.dll: 10030B17..10030CBF
// gamei386.so: 0005DC20..0005DDD2
void OSP_voteMenu (edict_t *ent, pmenu_t *p)
{
	if (m_mode && active_clients - botglobals.numbots && !ent->osp_e39c &&
		ent->client->resp.entered != ENTERED_ENTERED)
	{
		gi.cprintf (ent, PRINT_HIGH, "Observers cannot vote with active\n");
		gi.cprintf (ent, PRINT_HIGH, "players in the game.\n");
		return;
	}

	PMenu_Close (ent);

	if (!vote_inprogress)
	{
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

		OSP_updateVoteMenu (ent);
		PMenu_Open (ent, Vote_Menu, 0, 19);
	}
	else if (vote_item != 0x20)
	{
		OSP_updateProposalMenu (ent);
		PMenu_Open (ent, Proposal_Menu, 0, 18);
	}
	else
	{
		OSP_updateProposalMenu2 (ent);
		PMenu_Open (ent, Proposal_Menu2, 0, 18);
	}
}

// gamex86.dll: 10030CBF..10030D53
// gamei386.so: 0005DDD4..0005DE61
void OSP_voteMenu2 (edict_t *ent, pmenu_t *p)
{
	if (ent->client->resp.osp_r254)
	{
		gi.cprintf (ent, PRINT_HIGH, "Sorry, you can change only one item at a time!\n");
		return;
	}

	PMenu_Close (ent);
	if (!vote_inprogress)
	{
		ent->client->resp.osp_r2a4 = item_settings;
		OSP_updateVoteMenu2 (ent);
		PMenu_Open (ent, Vote_Menu2, 0, 18);
	}
	else
	{
		OSP_updateProposalMenu (ent);
		PMenu_Open (ent, Proposal_Menu, 0, 18);
	}
}

// gamex86.dll: 10030D53..10030E0E
// gamei386.so: 0005DE64..0005DF0D
void OSP_botMenu (edict_t *ent, pmenu_t *p)
{
	if (ent->client->resp.osp_r254)
	{
		gi.cprintf (ent, PRINT_HIGH,
					"Sorry, you can change only one item at a time!\n");
		return;
	}

	PMenu_Close (ent);

	if (!vote_inprogress)
	{
		ent->client->resp.osp_r29c = -1;
		ent->client->resp.osp_r250 = 0;
		ent->client->resp.osp_r294 = 0;
		CheckForNewBotFile ();
		OSP_updateBotMenu (ent);
		PMenu_Open (ent, Bot_Menu, 0, 18);
	}
	else
	{
		OSP_updateProposalMenu (ent);
		PMenu_Open (ent, Proposal_Menu, 0, 18);
	}
}

// gamex86.dll: 10030E0E..10030E53
// gamei386.so: 0005DF10..0005DF71
void OSP_helpMenu (edict_t *ent, pmenu_t *p)
{
	PMenu_Close (ent);
	if (m_mode < 2)
		Help_Menu[16].SelectFunc = OSP_returnMainDM_menu;
	else
		Help_Menu[16].SelectFunc = OSP_returnMainTeam_menu;
	PMenu_Open (ent, Help_Menu, 2, 18);
}

// gamex86.dll: 10030E53..10030E98
// gamei386.so: 0005DF74..0005DFD5
void OSP_help2Menu (edict_t *ent, pmenu_t *p)
{
	PMenu_Close (ent);
	if (m_mode < 2)
		Help2_Menu[16].SelectFunc = OSP_returnMainDM_menu;
	else
		Help2_Menu[16].SelectFunc = OSP_returnMainTeam_menu;
	PMenu_Open (ent, Help2_Menu, 2, 18);
}

// gamex86.dll: 10030E98..10030EDD
// gamei386.so: 0005DFD8..0005E039
void OSP_help3Menu (edict_t *ent, pmenu_t *p)
{
	PMenu_Close (ent);
	if (m_mode < 2)
		Help3_Menu[16].SelectFunc = OSP_returnMainDM_menu;
	else
		Help3_Menu[16].SelectFunc = OSP_returnMainTeam_menu;
	PMenu_Open (ent, Help3_Menu, 2, 18);
}

// gamex86.dll: 10030EDD..10030F26
// gamei386.so: 0005E03C..0005E081
void OSP_inviteMenu (edict_t *ent)
{
	int		cur;

	if (ent->client->inmenu)
		PMenu_Close (ent);
	cur = OSP_updateInviteMenu (ent);
	PMenu_Open (ent, Invite_Menu, cur, 18);
}

// Rebuild the team-mode main menu and return where the cursor should sit.
// `pick` is an EMPTY team the client could found: if they carry a default team
// name (team mode) or in 1v1, that slot's "*Join" line is relabelled with the
// name they would create rather than the team's current one. A team whose name
// already matches cancels the offer, which is what the shared `pick = -1`
// break at the bottom of the search loop does.
// gamex86.dll: 10030F26..100314B2
// gamei386.so: 0005E084..0005E609
int OSP_updateTeamMenu (edict_t *ent)
{
	int		i;
	int		pick;

	pick = -1;
	sprintf (tm_join[0], "*Join %s", teams[0].netname);
	sprintf (tm_join[1], "*Join %s", teams[1].netname);
	sprintf (tm_count0, "*(%d players)", OSP_teamCount (0));
	sprintf (tm_count1, "*(%d players)", OSP_teamCount (1));

	if (m_mode == 2)
		sprintf (tm_title, "[ Teamplay Mode ]");
	else
		sprintf (tm_title, "[ 1v1 Mode ]");

	for (i = 1; i >= 0; i--)
	{
		if (!OSP_teamCount (i))
			pick = i;

		if (ent->osp_e3a0[0] && m_mode == 2 &&
			!Q_stricmp (teams[i].netname, ent->osp_e3a0))
		{
			pick = -1;
			break;
		}
		if (m_mode == 3 &&
			!Q_stricmp (teams[i].netname, ent->client->pers.netname))
		{
			pick = -1;
			break;
		}
	}

	if (pick >= 0 && m_mode == 2 && ent->osp_e3a0[0])
		sprintf (tm_join[pick], "*Join %s", ent->osp_e3a0);
	else if (pick >= 0 && m_mode == 3)
		sprintf (tm_join[pick], "*Join %s", ent->client->pers.netname);

	Team_Menu[1].text = tm_title;
	Team_Menu[3].text = tm_join[0];
	Team_Menu[4].text = tm_count0;
	Team_Menu[5].text = tm_join[1];
	Team_Menu[6].text = tm_count1;

	tm_teamnum0 = 0;
	tm_teamnum1 = 1;
	Team_Menu[3].arg = &tm_teamnum0;
	Team_Menu[5].arg = &tm_teamnum1;

	if (ent->osp_e39c)
	{
		sprintf (tm_admin, "*Admin Menu");
		Team_Menu[7].SelectFunc = OSP_returnMainAdmin_menu;
	}
	else
	{
		sprintf (tm_admin, " ");
		Team_Menu[7].SelectFunc = NULL;
	}
	Team_Menu[7].text = tm_admin;

	if (ent->client->resp.entered != 2)
		sprintf (tm_obs, "*Enter OBSERVER Mode");
	else
		sprintf (tm_obs, "*Leave OBSERVER Mode");
	Team_Menu[9].text = tm_obs;

	if (ent->client->resp.entered != 4 && ent->client->resp.entered != 8)
		sprintf (tm_chase, "*Enter CHASECAM Mode");
	else
		sprintf (tm_chase, "*Leave CHASECAM Mode");
	Team_Menu[10].text = tm_chase;

	if ((int)allow_id->value == 2)
	{
		sprintf (tm_id, "*Player ID: OFF [LOCKED]");
		Team_Menu[11].SelectFunc = NULL;
	}
	else if ((int)allow_id->value == 3)
	{
		sprintf (tm_id, "*Player ID: ON [LOCKED]");
		Team_Menu[11].SelectFunc = NULL;
	}
	else
	{
		Team_Menu[11].SelectFunc = OSP_toggleID_menu;
		if (ent->client->resp.osp_r204)
			sprintf (tm_id, "*Player ID: ON");
		else
			sprintf (tm_id, "*Player ID: OFF");
	}
	Team_Menu[11].text = tm_id;

	// Windows-only: on a listen server the host player (address "loopback")
	// gets the ngWorldStats browser launcher in the HUD row's place instead.
#ifdef _WIN32
	if (!dedicated->value && !Q_stricmp (ent->osp_e37c, "loopback") &&
		(int)nglog_logstyle->value == 4)
	{
		sprintf (tm_hud, "*View ngStats (browser)");
		Team_Menu[12].text = tm_hud;
		Team_Menu[12].SelectFunc = OSP_ngStatsView;
	}
	else
#endif
	{
		sprintf (tm_hud, "*Change HUD Layout");
		Team_Menu[12].text = tm_hud;
		Team_Menu[12].SelectFunc = OSP_changeHUD;
	}

	if (!(int)vote_enable->value)
	{
		sprintf (tm_vote, "*Voting Menu [DISABLED]");
		Team_Menu[8].text = tm_vote;
		Team_Menu[8].SelectFunc = NULL;
	}
	else
	{
		sprintf (tm_vote, "*Voting Menu");
		Team_Menu[8].text = tm_vote;
		Team_Menu[8].SelectFunc = OSP_voteMenu;
	}

	if (ent->osp_e39c)
	{
		if (sync_stat > 2 || ent->client->resp.entered == ENTERED_ENTERED)
			return 7;
	}

	if (ent->client->resp.entered == ENTERED_ENTERED ||
		(vote_inprogress && !ent->client->resp.osp_r2d8) ||
		(m_mode == 3 && OSP_teamCount (0) && OSP_teamCount (1)))
		return 8;

	if (pick >= 0)
		return pick * 2 + 3;

	// Neither team is empty: sit on the smaller one, and toss a coin on a tie.
	if (OSP_teamCount (0) > OSP_teamCount (1))
		return 5;
	if (OSP_teamCount (1) > OSP_teamCount (0))
		return 3;

	return (rand () & 1) ? 3 : 5;
}

// gamex86.dll: 100314B2..100314CF
// gamei386.so: 0005E60C..0005E65C
void OSP_returnMainTeam_menu (edict_t *ent, pmenu_t *p)
{
	PMenu_Close (ent);
	OSP_teamMenu (ent);
}

// gamex86.dll: 100314CF..1003151E
// gamei386.so: 0005E65C..0005E6B0
void OSP_toggleID_menu (edict_t *ent, pmenu_t *p)
{
	OSP_id_cmd (ent);
	if (m_mode > 1)
		OSP_updateTeamMenu (ent);
	else
		OSP_updateDMMenu (ent);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

// gamex86.dll: 1003151E..1003152F
// gamei386.so: 0005E6B0..0005E6CD
void OSP_changeHUD (edict_t *ent, pmenu_t *p)
{
	OSP_hud_cmd (ent);
}

// gamex86.dll: 1003152F..10031578
// gamei386.so: 0005E6D0..0005E72F
void OSP_ngStatsView (edict_t *ent, pmenu_t *p)
{
	ngLog_ngStatsCall (1);
	who_paused = -3;
	match_paused = 1;
	pause_time = 100000;
	gi.bprintf (PRINT_CHAT, "Admin is viewing ngStats.  Please Wait!\n");
	PMenu_Close (ent);
}

// gamex86.dll: 10031578..100315A5
// gamei386.so: 0005E730..0005E764
void OSP_changeObserve (edict_t *ent, pmenu_t *p)
{
	OSP_startObserve (ent);
	PMenu_Close (ent);
	ent->client->resp.osp_r24c = 2;
}

// gamex86.dll: 100315A5..10031618
// gamei386.so: 0005E764..0005E7D8
void OSP_changeChase (edict_t *ent, pmenu_t *p)
{
	if ((!active_clients && ent->client->resp.entered == 2) ||
		(active_clients == 1 && ent->client->resp.entered == 1))
	{
		gi.cprintf (ent, PRINT_HIGH, "No clients to chase.\n");
		return;
	}

	OSP_ChaseCam (ent);
	ent->client->resp.osp_r24c = 2;
	PMenu_Close (ent);
}

// Rebuild the plain-DM main menu for one client and return where the cursor
// should sit. Every visible line is a static buffer this fills in; an entry is
// greyed out by nulling its SelectFunc rather than by hiding it.
// "*Join <team>" from the team menu. `p->arg` is the team index. resp.osp_r078
// is the pending invitation (team + 1), which is what gets a client past a
// locked or full team; resp.osp_r030 is "has been in the game before", so a
// first-time joiner can still found an empty team from their default name.
// gamex86.dll: 10031618..10031B46
// gamei386.so: 0005E7D8..0005ED13
void OSP_joinTeam_menu (edict_t *ent, pmenu_t *p)
{
	int		tnum;
	int		pick;
	int		invited;
	int		i;

	tnum = *(int *)p->arg;
	invited = ent->client->resp.osp_r078;

	if (ent->client->resp.team == tnum)
	{
		gi.cprintf (ent, PRINT_HIGH, "You are already on \"%s\"!\n",
					teams[tnum].netname);
		return;
	}

	if (ent->osp_e39c == 1)
	{
		gi.cprintf (ent, PRINT_HIGH, "Sorry, referees cannot enter the game!\n");
		PMenu_Close (ent);
		return;
	}

	if (who_paused == -2)
	{
		gi.cprintf (ent, PRINT_HIGH, "Sorry, cannot join during a forced pause.\n");
		return;
	}

	if (sync_stat == 4 && !invited && (int)match_latejoin->value <= 1)
	{
		gi.cprintf (ent, PRINT_HIGH, "Match in progress. You can only observe.\n");
		PMenu_Close (ent);
		return;
	}

	if (m_mode == 3 && !OSP_1v1AllowJoin (ent))
	{
		PMenu_Close (ent);
		return;
	}

	for (i = 1, pick = -2; i >= 0; i--)
	{
		if (!OSP_teamCount (i))
			pick = i;
		if (ent->osp_e3a0[0] && m_mode == 2 &&
			!Q_stricmp (teams[i].netname, ent->osp_e3a0))
		{
			pick = -1;
			break;
		}
	}

	if (!ent->client->resp.osp_r030 && m_mode == 2 &&
		(!teams[tnum].osp_m0f4 || invited) && pick == tnum)
	{
		if (OSP_defaultTeam (ent))
			goto joined;
	}

	if (m_mode == 3 && OSP_1v1Team (ent))
		goto joined;

	if ((!(OSP_teamCount (tnum) >= (int)team_maxplayers->value && !invited)
		 || (m_mode == 2 && ((int)match_latejoin->value > 2
							 || (sync_stat > 2 && (int)match_latejoin->value == 2 &&
								 OSP_teamCount (tnum) < (int)team_maxplayers->value))))
		&& !(teams[tnum].osp_m0f4 && !invited))
	{
		if (invited)
		{
			if (tnum != invited - 1 &&
				OSP_teamCount (tnum) >= (int)team_maxplayers->value)
			{
				gi.cprintf (ent, PRINT_HIGH,
							"You've been invited to join only team %s\n",
							teams[invited - 1].greenname);
				return;
			}
			ent->client->resp.osp_r078 = 0;
		}

		if (!OSP_addTeamMember (ent, tnum))
			return;
		goto joined;
	}
	else
	{
	full:
		if (teams[tnum].osp_m0f4 && !invited)
			gi.cprintf (ent, PRINT_HIGH, "\"%s\" is locked.\n", teams[tnum].netname);
		else
			gi.cprintf (ent, PRINT_HIGH, "\"%s\" is full.\n", teams[tnum].netname);
		return;
	}

joined:
	if (ent->client->resp.entered != ENTERED_ENTERED)
	{
		active_clients++;
		ent->client->chase_target = NULL;
		ent->client->resp.entered = ENTERED_ENTERED;
		ent->client->resp.osp_r240 = 0;
		ent->client->osp_t040 = 0;
		ent->client->osp_t03c = NULL;

		if (!ent->client->resp.osp_r030)
		{
			ent->client->resp.osp_r030 = 1;
			ent->client->resp.enterframe = level.framenum;
			OSP_setSingleAccuracy (ent);
		}
		else
			ent->client->resp.enterframe =
				level.framenum - ent->client->resp.osp_r2d4;

		gi.bprintf (PRINT_HIGH, "%s entered the game (clients = %i)\n",
					ent->client->pers.netname, active_clients);
		EntityListAdd (ent);
		q2log_playerEntered (ent);
	}

	ent->client->resp.score = ent->client->resp.osp_r248;
	ent->client->resp.osp_r0a0--;
	ent->client->resp.osp_r010 -= 2;
	OSP_notready_cmd (ent, true);

	if (sync_stat > 2)
		OSP_initTeamFrags (ent);
	OSP_setShowParams ();
	PMenu_Close (ent);
}

// gamex86.dll: 10031B46..10031E6E
// gamei386.so: 0005ED14..0005F049
int OSP_updateDMMenu (edict_t *ent)
{
	if (ent->client->resp.entered == ENTERED_ENTERED)
		sprintf (dm_play_line, "*Return to Play");
	else
		sprintf (dm_play_line, "*Enter the Game");
	RegDM_Menu[4].text = dm_play_line;

	if (ent->osp_e39c == 1 ||
		(sync_stat == 4 && (int)match_latejoin->value <= 1))
		RegDM_Menu[4].SelectFunc = NULL;

	if (ent->osp_e39c)
	{
		sprintf (dm_admin_line, "*Admin Menu");
		RegDM_Menu[6].SelectFunc = OSP_returnMainAdmin_menu;
	}
	else
	{
		sprintf (dm_admin_line, " ");
		RegDM_Menu[6].SelectFunc = NULL;
	}
	RegDM_Menu[6].text = dm_admin_line;

	if (ent->client->resp.entered != 2)
		sprintf (dm_obs_line, "*Enter OBSERVER Mode");
	else
		sprintf (dm_obs_line, "*Leave OBSERVER Mode");
	RegDM_Menu[7].text = dm_obs_line;

	if (ent->client->resp.entered != 4 &&
		ent->client->resp.entered != 8)
		sprintf (dm_chase_line, "*Enter CHASECAM Mode");
	else
		sprintf (dm_chase_line, "*Leave CHASECAM Mode");
	RegDM_Menu[8].text = dm_chase_line;

	// allow_id 2 and 3 are the server-locked off/on settings, so the line
	// still shows the state but cannot be selected.
	if ((int)allow_id->value == 2)
	{
		sprintf (dm_id_line, "*Player ID: OFF [LOCKED]");
		RegDM_Menu[10].SelectFunc = NULL;
	}
	else if ((int)allow_id->value == 3)
	{
		sprintf (dm_id_line, "*Player ID: ON [LOCKED]");
		RegDM_Menu[10].SelectFunc = NULL;
	}
	else
	{
		RegDM_Menu[10].SelectFunc = OSP_toggleID_menu;
		if (ent->client->resp.osp_r204)
			sprintf (dm_id_line, "*Player ID: ON");
		else
			sprintf (dm_id_line, "*Player ID: OFF");
	}
	RegDM_Menu[10].text = dm_id_line;

	if (!(int)vote_enable->value)
	{
		sprintf (dm_vote_line, "*Voting Menu [DISABLED]");
		RegDM_Menu[5].text = dm_vote_line;
		RegDM_Menu[5].SelectFunc = NULL;
	}
	else
	{
		sprintf (dm_vote_line, "*Voting Menu");
		RegDM_Menu[5].text = dm_vote_line;
		RegDM_Menu[5].SelectFunc = OSP_voteMenu;
	}

	// Same Windows-only ngStats row as OSP_updateTeamMenu.
#ifdef _WIN32
	if (!dedicated->value && !Q_stricmp (ent->osp_e37c, "loopback") &&
		(int)nglog_logstyle->value == 4)
	{
		sprintf (dm_hud_line, "*View ngStats (browser)");
		RegDM_Menu[9].text = dm_hud_line;
		RegDM_Menu[9].SelectFunc = OSP_ngStatsView;
	}
	else
#endif
	{
		sprintf (dm_hud_line, "*Change HUD Layout");
		RegDM_Menu[9].text = dm_hud_line;
		RegDM_Menu[9].SelectFunc = OSP_changeHUD;
	}

	OSP_setShowParams ();

	// Park the cursor on the vote line while a vote this client has not
	// answered yet is running.
	if (vote_inprogress && !ent->client->resp.osp_r2d8)
		return 5;
	if (ent->osp_e39c)
		return 6;
	return 4;
}

// gamex86.dll: 10031E6E..10031E8B
// gamei386.so: 0005F04C..0005F09C
void OSP_returnMainDM_menu (edict_t *ent, pmenu_t *p)
{
	PMenu_Close (ent);
	OSP_DMMenu (ent);
}

// "*Return to Play" / "*Enter the Game" from the DM menu. Everything from
// `active_clients++` down is the same join sequence osp_teams.c's
// OSP_teamjoin_cmd runs; the difference is that a first-time entrant also gets
// its accuracy slot seeded, where a re-entrant keeps its old enterframe.
// gamex86.dll: 10031E8B..1003205F
// gamei386.so: 0005F09C..0005F226
void OSP_dmReturn_menu (edict_t *ent, pmenu_t *p)
{
	if (ent->client->resp.entered != ENTERED_ENTERED)
	{
		if (sync_stat == 4 && (int)match_latejoin->value <= 1)
		{
			gi.cprintf (ent, PRINT_HIGH,
						"Sorry, you cannot enter a match in progress\n");
			return;
		}

		active_clients++;
		ent->client->chase_target = NULL;
		ent->client->resp.entered = ENTERED_ENTERED;
		ent->client->resp.osp_r240 = 0;
		ent->client->osp_t040 = 0;
		ent->client->osp_t03c = NULL;

		if (!ent->client->resp.osp_r030)
		{
			ent->client->resp.osp_r030 = 1;
			ent->client->resp.enterframe = level.framenum;
			OSP_setSingleAccuracy (ent);
		}
		else
			ent->client->resp.enterframe =
				level.framenum - ent->client->resp.osp_r2d4;

		if (sync_stat < 4)
		{
			ent->client->resp.osp_r010 -= 2;
			OSP_notready_cmd (ent, true);
		}

		gi.bprintf (PRINT_HIGH, "%s entered the game (clients = %i)\n",
					ent->client->pers.netname, active_clients);

		ent->client->resp.score = ent->client->resp.osp_r248;
		ent->client->resp.osp_r0a0--;
		ent->client->resp.osp_r09c--;
		EntityListAdd (ent);
		OSP_DoRankSort ();
		q2log_playerEntered (ent);
	}

	PMenu_Close (ent);
}

// The item-toggle voting page. resp.osp_r2a4 is the staged copy of
// `item_settings`, one bit per line, and each menu entry's `arg` is a static
// int holding that line's bit -- which is what OSP_changeItems_menu XORs in.
// Rebuild the voting menu. Each of the nine votable things has its own
// `vote_enable_*` cvar: zero shows "[LOCKED]" and nulls the entry's
// SelectFunc, otherwise the line shows the client's STAGED value (the
// resp.osp_r2xx copy the leaves cycle), not what is in force.
// gamex86.dll: 1003205F..100325CB
// gamei386.so: 0005F228..0005F8B5
void OSP_updateVoteMenu (edict_t *ent)
{
	if (!(int)vote_enable_map->value)
	{
		sprintf (vm_map, "Map: [LOCKED]");
		Vote_Menu[3].SelectFunc = NULL;
	}
	else if (!map_size)
	{
		sprintf (vm_map, "Map: [NOT AVAILABLE]");
		Vote_Menu[3].SelectFunc = NULL;
	}
	else if (ent->client->resp.osp_r290 == -1)
	{
		sprintf (vm_map, "Map: [SELECT]");
		Vote_Menu[3].SelectFunc = OSP_changeMap_menu;
	}
	else
	{
		sprintf (vm_map, "Map: %s", map[ent->client->resp.osp_r290].name);
		Vote_Menu[3].SelectFunc = OSP_changeMap_menu;
	}

	if (!(int)vote_enable_config->value)
	{
		sprintf (vm_config, "Config: [LOCKED]");
		Vote_Menu[4].SelectFunc = NULL;
	}
	else if (!conf_size)
	{
		sprintf (vm_config, "Config: [NOT AVAILABLE]");
		Vote_Menu[4].SelectFunc = NULL;
	}
	else if (ent->client->resp.osp_r258 == -1)
	{
		sprintf (vm_config, "Config: [SELECT]");
		Vote_Menu[4].SelectFunc = OSP_changeConfig_menu;
	}
	else
	{
		// The description if the config has one, otherwise its filename, and
		// either way clipped to 22 characters so the line still fits.
		char cfg[24];

		if (conf_info[ent->client->resp.osp_r258][0])
			strncpy (cfg, conf_info[ent->client->resp.osp_r258], 22);
		else
			strncpy (cfg, conf_name[ent->client->resp.osp_r258], 22);
		cfg[22] = 0;
		sprintf (vm_config, "Config: %s", cfg);
		Vote_Menu[4].SelectFunc = OSP_changeConfig_menu;
	}

	if (!(int)vote_enable_toggles->value)
	{
		sprintf (vm_toggles, "Item toggles [LOCKED]");
		Vote_Menu[5].SelectFunc = NULL;
	}
	else
	{
		sprintf (vm_toggles, "Item toggles...");
		Vote_Menu[5].SelectFunc = OSP_voteMenu2;
	}

	if (!(int)vote_enable_bots->value)
	{
		sprintf (vm_bots, "Gladiator Bots [LOCKED]");
		Vote_Menu[6].SelectFunc = NULL;
	}
	else
	{
		sprintf (vm_bots, "Gladiator Bots...");
		Vote_Menu[6].SelectFunc = OSP_botMenu;
	}

	if (!(int)vote_enable_time->value)
	{
		sprintf (vm_time, "Timelimit: [LOCKED]");
		Vote_Menu[7].SelectFunc = NULL;
	}
	else if (!ent->client->resp.osp_r2a0)
	{
		sprintf (vm_time, "Timelimit: OFF");
		Vote_Menu[7].SelectFunc = OSP_changeTime_menu;
	}
	else
	{
		sprintf (vm_time, "Timelimit: %d", ent->client->resp.osp_r2a0);
		Vote_Menu[7].SelectFunc = OSP_changeTime_menu;
	}

	if (!(int)vote_enable_frag->value)
	{
		sprintf (vm_frag, "Fraglimit: [LOCKED]");
		Vote_Menu[8].SelectFunc = NULL;
	}
	else if (!ent->client->resp.osp_r25c)
	{
		sprintf (vm_frag, "Fraglimit: NONE");
		Vote_Menu[8].SelectFunc = OSP_changeFrag_menu;
	}
	else
	{
		sprintf (vm_frag, "Fraglimit: %d", ent->client->resp.osp_r25c);
		Vote_Menu[8].SelectFunc = OSP_changeFrag_menu;
	}

	if (!(int)vote_enable_hook->value)
	{
		sprintf (vm_hook, "The Hook: [LOCKED]");
		Vote_Menu[9].SelectFunc = NULL;
	}
	else if (!ent->client->resp.osp_r260)
	{
		sprintf (vm_hook, "The Hook: DISABLED");
		Vote_Menu[9].SelectFunc = OSP_changeHook_menu;
	}
	else
	{
		sprintf (vm_hook, "The Hook: ENABLED");
		Vote_Menu[9].SelectFunc = OSP_changeHook_menu;
	}

	if (!(int)vote_enable_runes->value)
	{
		sprintf (vm_runes, "Runes: [LOCKED]");
		Vote_Menu[10].SelectFunc = NULL;
	}
	else if (!ent->client->resp.osp_r298)
	{
		sprintf (vm_runes, "Runes: DISABLED");
		Vote_Menu[10].SelectFunc = OSP_changeRunes_menu;
	}
	else
	{
		sprintf (vm_runes, "Runes: ENABLED");
		Vote_Menu[10].SelectFunc = OSP_changeRunes_menu;
	}

	if (!(int)vote_enable_kick->value)
	{
		sprintf (vm_kick, "Kick: [LOCKED]");
		Vote_Menu[11].SelectFunc = NULL;
	}
	else if (ent->client->resp.osp_r268 == -1)
	{
		sprintf (vm_kick, "Kick: [SELECT]");
		Vote_Menu[11].SelectFunc = OSP_changeKick_menu;
	}
	else
	{
		sprintf (vm_kick, "Kick: %s",
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

	if (m_mode > 1)
		Vote_Menu[14].SelectFunc = OSP_returnMainTeam_menu;
	else
		Vote_Menu[14].SelectFunc = OSP_returnMainDM_menu;
}

// gamex86.dll: 100325CB..100328C6
// gamei386.so: 0005F8B8..0005FBF6
void OSP_updateVoteMenu2 (edict_t *ent)
{
	{
		if (ent->client->resp.osp_r2a4 & 1)
			sprintf (v2_line0, "Allow Quad: YES");
		else
			sprintf (v2_line0, "Allow Quad: NO");
	}

	{
		if (ent->client->resp.osp_r2a4 & 2)
			sprintf (v2_line1, "Allow Invul: YES");
		else
			sprintf (v2_line1, "Allow Invul: NO");
	}

	{
		if (ent->client->resp.osp_r2a4 & 4)
			sprintf (v2_line2, "Drop Quad: YES");
		else
			sprintf (v2_line2, "Drop Quad: NO");
	}

	{
		if (ent->client->resp.osp_r2a4 & 8)
			sprintf (v2_line3, "Allow BFG: YES");
		else
			sprintf (v2_line3, "Allow BFG: NO");
	}

	{
		if (ent->client->resp.osp_r2a4 & 0x10)
			sprintf (v2_line4, "Allow Power Armor: YES");
		else
			sprintf (v2_line4, "Allow Power Armor: NO");
	}

	{
		if (ent->client->resp.osp_r2a4 & 0x20)
			sprintf (v2_line5, "Weapons Stay: YES");
		else
			sprintf (v2_line5, "Weapons Stay: NO");
	}

	{
		if (ent->client->resp.osp_r2a4 & 0x40)
			sprintf (v2_line6, "Hurt Self: YES");
		else
			sprintf (v2_line6, "Hurt Self: NO");
	}

	if (m_mode > 1)
	{
		if (ent->client->resp.osp_r2a4 & 0x80)
			sprintf (v2_line7, "Hurt Team: YES");
		else
			sprintf (v2_line7, "Hurt Team: NO");
	}
	else
	{
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

	if (m_mode > 1)
		Vote_Menu2[13].SelectFunc = OSP_returnMainTeam_menu;
	else
		Vote_Menu2[13].SelectFunc = OSP_returnMainDM_menu;
}

// gamex86.dll: 100328C6..10032A6B
// gamei386.so: 0005FBF8..0005FDAE
void OSP_updateBotMenu (edict_t *ent)
{
	bot_t		*b;
	int			ncount;
	int			t;

	if (ent->client->resp.osp_r29c == -1)
		sprintf (bot_name_line, "[SELECT]");
	else
	{
		for (ncount = 0, b = botlist; b; b = b->next, ncount++)
			;

		if (!ncount)
			sprintf (bot_name_line, "[NONE AVAILABLE]");
		else
		{
			for (t = 0, b = botlist; b && t < ent->client->resp.osp_r29c; b = b->next, t++)
				;
			if (b)
				strcpy (bot_name_line, b->name);
		}
	}

	sprintf (bot_add_line, "*Add random bots: %d", ent->client->resp.osp_r250);
	sprintf (bot_rem_line, "*Remove random bots: %d", ent->client->resp.osp_r294);
	sprintf (bot_total_line, "Total active bots: %d", botglobals.numbots);

	bot_add_arg = ent->client->resp.osp_r250;
	bot_rem_arg = ent->client->resp.osp_r294;

	Bot_Menu[6].arg = &bot_add_arg;
	Bot_Menu[7].arg = &bot_rem_arg;
	Bot_Menu[4].text = bot_name_line;
	Bot_Menu[6].text = bot_add_line;
	Bot_Menu[7].text = bot_rem_line;
	Bot_Menu[9].text = bot_total_line;

	if (m_mode > 1)
		Bot_Menu[13].SelectFunc = OSP_returnMainTeam_menu;
	else
		Bot_Menu[13].SelectFunc = OSP_returnMainDM_menu;
}

// The read-only "a vote is running" page. `vote_item` is the global the vote
// itself carries and its bits are NOT resp.osp_r254's: 1 map, 2 config,
// 4 timelimit, 8 fraglimit, 0x10 hook, 0x20 item toggles (which gets
// Proposal_Menu2 instead of this), 0x40 BFG, 0x80 Quad, 0x100 add one bot,
// 0x200 add N bots, 0x400 remove N bots, 0x800 runes, 0x1000 kick.
// `vote_value` is always the value as a string, so the numeric arms atoi() it.
// gamex86.dll: 10032A6B..10032F46
// gamei386.so: 0005FDB0..000601F6
void OSP_updateProposalMenu (edict_t *ent)
{
	int			bots;
	edict_t		*other;

	// Recount from scratch -- a vote can outlive the clients that started it.
	connected_clients = 0;
	active_clients = 0;
	bots = 0;
	{
		int			i;

		for (i = 1; i <= game.maxclients; i++)
		{
			other = g_edicts + i;
			if (other->inuse && other->client && other->client->pers.connected)
			{
				connected_clients++;
				if (other->client->resp.entered == ENTERED_ENTERED)
					active_clients++;
				if (other->flags & FL_OSP_BOT)
					bots++;
			}
		}
	}
	botglobals.numbots = bots;

	if (!connected_clients)
		return;

	pm_line2[0] = 0;

	switch (vote_item)
	{
	case 1:
		sprintf (pm_line1, "Change map to %s", vote_value);
		break;
	case 2:
		sprintf (pm_line1, "Set server configuration to");
		sprintf (pm_line2, "%s", vote_value);
		break;
	case 4:
		if (!atoi (vote_value))
			sprintf (pm_line1, "Change timelimit to OFF");
		else
			sprintf (pm_line1, "Change timelimit to %s", vote_value);
		break;
	case 8:
		if (!atoi (vote_value))
			sprintf (pm_line1, "Change fraglimit to NONE");
		else
			sprintf (pm_line1, "Change fraglimit to %s", vote_value);
		break;
	case 0x10:
		if (atoi (vote_value))
			sprintf (pm_line1, "Set the hook to ENABLED");
		else
			sprintf (pm_line1, "Set the hook to DISABLED");
		break;
	case 0x800:
		if (atoi (vote_value))
			sprintf (pm_line1, "Set runes to ENABLED");
		else
			sprintf (pm_line1, "Set runes to DISABLED");
		break;
	case 0x40:
		if (atoi (vote_value))
			sprintf (pm_line1, "Set the BFG to ENABLED");
		else
			sprintf (pm_line1, "Set the BFG to DISABLED");
		break;
	case 0x80:
		if (atoi (vote_value))
			sprintf (pm_line1, "Set the Quad to ENABLED");
		else
			sprintf (pm_line1, "Set the Quad to DISABLED");
		break;
	case 0x1000:
		sprintf (pm_line1, "Kick UNKNOWN");
		{
			edict_t		*other;
			int			i;

			for (i = 1; i <= game.maxclients; i++)
			{
				other = g_edicts + i;
				if (!other->inuse || !other->client ||
					other->client->resp.clientid != atoi (vote_value))
					continue;

				sprintf (pm_line1, "Kick %s", other->client->pers.greenname);
				break;
			}
		}
		break;
	case 0x100:
		sprintf (pm_line1, "Add 1 Gladiator bot");
		break;
	case 0x200:
		sprintf (pm_line1, "Add %s Gladiator bots", vote_value);
		break;
	case 0x400:
		sprintf (pm_line1, "Remove %s Gladiator bots", vote_value);
		break;
	default:
		sprintf (pm_line1, "Umm, what were we voting for?");
		break;
	}

	OSP_menuVotePercent ((edict_t *)pm_pct, pm_needed);

	Proposal_Menu[4].text = pm_line1;
	Proposal_Menu[5].text = pm_line2;
	Proposal_Menu[7].text = pm_pct;
	Proposal_Menu[8].text = pm_needed;

	if (m_mode > 1)
		Proposal_Menu[12].SelectFunc = OSP_returnMainTeam_menu;
	else
		Proposal_Menu[12].SelectFunc = OSP_returnMainDM_menu;
}

// The item-toggle variant of the proposal page: it has to show all eight
// toggles at once, so it marks the ones the vote would NOT change with a
// leading "*" rather than listing only the differences.
// gamex86.dll: 10032F46..10033452
// gamei386.so: 000601F8..0006073A
void OSP_updateProposalMenu2 (edict_t *ent)
{
	edict_t		*other;
	int			i;
	int			bots;

	connected_clients = 0;
	active_clients = 0;
	bots = 0;
	for (i = 1; i <= game.maxclients; i++)
	{
		other = g_edicts + i;
		if (other->inuse && other->client && other->client->pers.connected)
		{
			connected_clients++;
			if (other->client->resp.entered == ENTERED_ENTERED)
				active_clients++;
			if (other->flags & FL_OSP_BOT)
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

	if ((atoi (vote_value) & 1) == (item_settings & 1))
		strcpy (pm2_line0, "*");
	if (atoi (vote_value) & 1)
		strcat (pm2_line0, "Allow Quad: YES");
	else
		strcat (pm2_line0, "Allow Quad: NO");

	if ((atoi (vote_value) & 2) == (item_settings & 2))
		strcpy (pm2_line1, "*");
	if (atoi (vote_value) & 2)
		strcat (pm2_line1, "Allow Invul: YES");
	else
		strcat (pm2_line1, "Allow Invul: NO");

	if ((atoi (vote_value) & 4) == (item_settings & 4))
		strcpy (pm2_line2, "*");
	if (atoi (vote_value) & 4)
		strcat (pm2_line2, "Drop Quad: YES");
	else
		strcat (pm2_line2, "Drop Quad: NO");

	if ((atoi (vote_value) & 8) == (item_settings & 8))
		strcpy (pm2_line3, "*");
	if (atoi (vote_value) & 8)
		strcat (pm2_line3, "Allow BFG: YES");
	else
		strcat (pm2_line3, "Allow BFG: NO");

	if ((atoi (vote_value) & 0x10) == (item_settings & 0x10))
		strcpy (pm2_line4, "*");
	if (atoi (vote_value) & 0x10)
		strcat (pm2_line4, "Allow Power Armor: YES");
	else
		strcat (pm2_line4, "Allow Power Armor: NO");

	if ((atoi (vote_value) & 0x20) == (item_settings & 0x20))
		strcpy (pm2_line5, "*");
	if (atoi (vote_value) & 0x20)
		strcat (pm2_line5, "Weapons Stay: YES");
	else
		strcat (pm2_line5, "Weapons Stay: NO");

	if ((atoi (vote_value) & 0x40) == (item_settings & 0x40))
		strcpy (pm2_line6, "*");
	if (atoi (vote_value) & 0x40)
		strcat (pm2_line6, "Hurt Self: YES");
	else
		strcat (pm2_line6, "Hurt Self: NO");

	if (m_mode > 1)
	{
		if ((atoi (vote_value) & 0x80) == (item_settings & 0x80))
			strcpy (pm2_line7, "*");
		if (atoi (vote_value) & 0x80)
			strcat (pm2_line7, "Hurt Team: YES");
		else
			strcat (pm2_line7, "Hurt Team: NO");
	}
	else
		pm2_line7[0] = 0;

	OSP_menuVotePercent ((edict_t *)pm2_pct, pm2_needed);

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

	if (m_mode > 1)
		Proposal_Menu2[17].SelectFunc = OSP_returnMainTeam_menu;
	else
		Proposal_Menu2[17].SelectFunc = OSP_returnMainDM_menu;
}

// gamex86.dll: 10033452..10033485
// gamei386.so: 0006073C..00060795
void OSP_menuVotePercent (edict_t *ent, char *out)
{
	OSP_votePercent (ent, 3);
	sprintf (out, "%d%% Needed to Decide", (int)vote_threshold->value);
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
// gamex86.dll: 10033485..100338A8
// gamei386.so: 00060798..000609D1
void OSP_proposeVote_menu (edict_t *ent, pmenu_t *p)
{
	char		value[16];

	if (!ent->client->resp.osp_r254)
	{
		gi.cprintf (ent, PRINT_HIGH, "No changes.  No proposal initiated.\n");
		PMenu_Close (ent);
		return;
	}
	if (ent->client->resp.osp_r254 == 1)
	{
		OSP_vote_cmd (ent, 1, 3, "map", map[ent->client->resp.osp_r290].name);
		PMenu_Close (ent);
		return;
	}
	if (ent->client->resp.osp_r254 == 2)
	{
		// Prefer the config's description; fall back to its filename.
		if (conf_info[ent->client->resp.osp_r258][0])
			OSP_vote_cmd (ent, 1, 3, "config",
						  conf_info[ent->client->resp.osp_r258]);
		else
			OSP_vote_cmd (ent, 1, 3, "config",
						  conf_name[ent->client->resp.osp_r258]);
		PMenu_Close (ent);
		return;
	}
	if (ent->client->resp.osp_r254 == 4)
	{
		sprintf (value, "%d", ent->client->resp.osp_r2a0);
		OSP_vote_cmd (ent, 1, 3, "timelimit", value);
		PMenu_Close (ent);
		return;
	}
	if (ent->client->resp.osp_r254 == 8)
	{
		sprintf (value, "%d", ent->client->resp.osp_r25c);
		OSP_vote_cmd (ent, 1, 3, "fraglimit", value);
		PMenu_Close (ent);
		return;
	}
	if (ent->client->resp.osp_r254 == 0x10)
	{
		sprintf (value, "%d", ent->client->resp.osp_r260);
		OSP_vote_cmd (ent, 1, 3, "hook", value);
		PMenu_Close (ent);
		return;
	}
	if (ent->client->resp.osp_r254 == 0x200)
	{
		sprintf (value, "%d", ent->client->resp.osp_r298);
		OSP_vote_cmd (ent, 1, 3, "runes", value);
		PMenu_Close (ent);
		return;
	}
	if (ent->client->resp.osp_r254 == 0x20)
	{
		sprintf (value, "%d", ent->client->resp.osp_r2a4);
		OSP_vote_cmd (ent, 1, 3, "toggles", value);
		PMenu_Close (ent);
		return;
	}
	if (ent->client->resp.osp_r254 == 0x400)
	{
		sprintf (value, "%d", (char)ent->client->resp.osp_r26c[0]);
		OSP_vote_cmd (ent, 1, 3, "kick", value);
		PMenu_Close (ent);
		return;
	}
	if (ent->client->resp.osp_r254 == 0x40)
	{
		sprintf (value, "%d", ent->client->resp.osp_r29c);
		OSP_vote_cmd (ent, 1, 3, "specbot", value);
		PMenu_Close (ent);
		return;
	}
	if (ent->client->resp.osp_r254 == 0x80)
	{
		sprintf (value, "%d", ent->client->resp.osp_r250);
		OSP_vote_cmd (ent, 1, 3, "addbots", value);
		PMenu_Close (ent);
		return;
	}

	sprintf (value, "%d", ent->client->resp.osp_r294);
	OSP_vote_cmd (ent, 1, 3, "rembots", value);
	PMenu_Close (ent);
}

// gamex86.dll: 100338A8..100339DD
// gamei386.so: 000609D4..00060AAE
void OSP_changeMap_menu (edict_t *ent, pmenu_t *p)
{
	if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 1)
	{
		gi.cprintf (ent, PRINT_HIGH,
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
	else if (ent->client->resp.osp_r290 == map_size)
	{
		ent->client->resp.osp_r290 = -1;
		ent->client->resp.osp_r254 = 0;
	}

	OSP_updateVoteMenu (ent);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

// gamex86.dll: 100339DD..10033B12
// gamei386.so: 00060AB0..00060B8E
void OSP_changeConfig_menu (edict_t *ent, pmenu_t *p)
{
	if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 2)
	{
		gi.cprintf (ent, PRINT_HIGH,
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
	else if (ent->client->resp.osp_r258 == conf_size)
	{
		ent->client->resp.osp_r258 = -1;
		ent->client->resp.osp_r254 = 0;
	}

	OSP_updateVoteMenu (ent);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

// gamex86.dll: 10033B12..10033C6A
// gamei386.so: 00060B90..00060D09
void OSP_changeTime_menu (edict_t *ent, pmenu_t *p)
{
	if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 4)
	{
		gi.cprintf (ent, PRINT_HIGH,
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

	OSP_updateVoteMenu (ent);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

// gamex86.dll: 10033C6A..10033DC2
// gamei386.so: 00060D0C..00060E85
void OSP_changeFrag_menu (edict_t *ent, pmenu_t *p)
{
	if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 8)
	{
		gi.cprintf (ent, PRINT_HIGH,
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

	OSP_updateVoteMenu (ent);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

// gamex86.dll: 10033DC2..10033E80
// gamei386.so: 00060E88..00060F45
void OSP_changeHook_menu (edict_t *ent, pmenu_t *p)
{
	if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 0x10)
	{
		gi.cprintf (ent, PRINT_HIGH,
					"Sorry, you can change only one item at a time!\n");
		return;
	}

	ent->client->resp.osp_r260 = 1 - ent->client->resp.osp_r260;
	ent->client->resp.osp_r254 = 0x10;
	if (ent->client->resp.osp_r260 == (int)hook_enable->value)
		ent->client->resp.osp_r254 = 0;

	OSP_updateVoteMenu (ent);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

// gamex86.dll: 10033E80..10033F63
// gamei386.so: 00060F48..00060FFF
void OSP_changeRunes_menu (edict_t *ent, pmenu_t *p)
{
	if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 0x200)
	{
		gi.cprintf (ent, PRINT_HIGH,
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

	OSP_updateVoteMenu (ent);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

// Step the kick target to the next/previous connected client. resp.osp_r268 is
// the client NUMBER, and 0 / -2 are the two ends the backward walk stops and
// wraps at. The chosen client's id and name are cached in resp.osp_r26c so the
// proposal can name them even after they leave.
// gamex86.dll: 10033F63..1003426E
// gamei386.so: 00061000..0006122F
void OSP_changeKick_menu (edict_t *ent, pmenu_t *p)
{
	edict_t		*other;
		// Dead: set after each strcpy, never read.
	int			res;					// invented name
	int			i;
	int			n;
	int			was;

	res = 0;
	was = ent->client->resp.osp_r268;
	if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 0x400)
	{
		gi.cprintf (ent, PRINT_HIGH,
					"Sorry, you can change only one item at a time!\n");
		return;
	}

	if (ent->client->resp.osp_r264)
	{
		for (i = 1, n = ent->client->resp.osp_r268 - 1;
			 i <= game.maxclients; i++, n--)
		{
			if (!n)
			{
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
			strcpy ((char *)&ent->client->resp.osp_r26c[1],
					other->client->pers.netname);
			res = 1;
			break;
		}
		if (ent->client->resp.osp_r268 == was)
			ent->client->resp.osp_r268 = -1;
	}
	else
	{
		for (i = 1, n = ent->client->resp.osp_r268 + 1;
			 i <= game.maxclients; i++, n++)
		{
			if (n > game.maxclients)
			{
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
			strcpy ((char *)&ent->client->resp.osp_r26c[1],
					other->client->pers.netname);
			res = 1;
			break;
		}
		if (ent->client->resp.osp_r268 == was)
			ent->client->resp.osp_r268 = -1;
	}

	ent->client->resp.osp_r254 = 0x400;
	if (ent->client->resp.osp_r268 == -1)
		ent->client->resp.osp_r254 = 0;

	OSP_updateVoteMenu (ent);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

// gamex86.dll: 1003426E..1003432B
// gamei386.so: 00061230..000612CC
void OSP_changeItems_menu (edict_t *ent, pmenu_t *p)
{
	int		bit;

	bit = *(int *)p->arg;
	if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 0x20)
	{
		gi.cprintf (ent, PRINT_HIGH,
					"Sorry, you can change only one item at a time!\n");
		return;
	}

	ent->client->resp.osp_r2a4 ^= bit;
	ent->client->resp.osp_r254 = 0x20;
	if (ent->client->resp.osp_r2a4 == item_settings)
		ent->client->resp.osp_r254 = 0;

	OSP_updateVoteMenu2 (ent);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

// Cycle through the names in the bot config file. Selecting a name clears the
// "add N random bots" staging and vice versa, which is why each of the two
// resets the other's field before touching its own.
// gamex86.dll: 1003432B..100344FB
// gamei386.so: 000612CC..00061401
void OSP_addSpecificBot_menu (edict_t *ent, pmenu_t *p)
{
	bot_t		*b;
	int			ncount;
	int			t;

	if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 0x40)
	{
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

	if (ent->client->resp.osp_r29c == -1 || !ncount)
	{
		ent->client->resp.osp_r254 = 0;
		voted_botname[0] = 0;
	}
	else
	{
		for (t = 0, b = botlist; b && t < ent->client->resp.osp_r29c; b = b->next, t++)
			;
		strcpy (voted_botname, b->name);
		ent->client->resp.osp_r254 = 0x40;
	}

	OSP_updateBotMenu (ent);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

// gamex86.dll: 100344FB..10034689
// gamei386.so: 00061404..00061598
void OSP_addBots_menu (edict_t *ent, pmenu_t *p)
{
	if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 0x80)
	{
		ent->client->resp.osp_r294 = 0;
		ent->client->resp.osp_r29c = -1;
	}

	if (ent->client->resp.osp_r264)
		ent->client->resp.osp_r250--;
	else
		ent->client->resp.osp_r250++;

	if (ent->client->resp.osp_r250 < 0)
		ent->client->resp.osp_r250 = (int)vote_bots_max->value - bots_votedin;

	if (ent->client->resp.osp_r250 + connected_clients > (int)maxclients->value)
		ent->client->resp.osp_r250 = (int)maxclients->value - connected_clients;
	else if (ent->client->resp.osp_r250 >
			 (int)vote_bots_max->value - bots_votedin)
		ent->client->resp.osp_r250 = 0;

	if (!ent->client->resp.osp_r250)
		ent->client->resp.osp_r254 = 0;
	else
		ent->client->resp.osp_r254 = 0x80;

	OSP_updateBotMenu (ent);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

// gamex86.dll: 10034689..100347B4
// gamei386.so: 00061598..00061667
void OSP_removeBots_menu (edict_t *ent, pmenu_t *p)
{
	if (ent->client->resp.osp_r254 && ent->client->resp.osp_r254 != 0x100)
	{
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

	OSP_updateBotMenu (ent);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

// gamex86.dll: 100347B4..100347D1
// gamei386.so: 00061668..0006168F
void OSP_acceptVote_menu (edict_t *ent, pmenu_t *p)
{
	OSP_yes_cmd (ent);
	PMenu_Close (ent);
}

// gamex86.dll: 100347D1..100347EE
// gamei386.so: 00061690..000616B7
void OSP_declineVote_menu (edict_t *ent, pmenu_t *p)
{
	OSP_no_cmd (ent);
	PMenu_Close (ent);
}

// gamex86.dll: 100347EE..1003484C
// gamei386.so: 000616B8..00061731
int OSP_updateInviteMenu (edict_t *ent)
{
	sprintf (invite_teamname, "%s", teams[ent->client->resp.osp_r2cc].netname);
	invite_teamnum = ent->client->resp.osp_r078 - 1;
	Invite_Menu[7].text = invite_teamname;
	Invite_Menu[11].arg = &invite_teamnum;
	return 11;
}

// gamex86.dll: 1003484C..10034944
// gamei386.so: 00061734..0006181F
void OSP_inviteClose_menu (edict_t *ent, pmenu_t *p)
{
	edict_t		*other;
	int			i;
	int			tnum;

	// The same value is recomputed just below, unread in between -- a dead
	// first store, faithfully reproduced.
	tnum = ent->client->resp.osp_r078 - 1;
	PMenu_Close (ent);
	gi.cprintf (ent, PRINT_HIGH, "Invitation declined.\n");

	tnum = ent->client->resp.osp_r078 - 1;
	for (i = 1; i <= game.maxclients; i++)
	{
		other = g_edicts + i;
		if (!other->inuse || !other->client ||
			other->client->resp.entered != ENTERED_ENTERED ||
			other->client->resp.team != tnum ||
			!other->client->resp.osp_r2c4)
			continue;

		gi.cprintf (other, PRINT_HIGH, "%s has declined your invitation.\n",
					ent->client->pers.greenname);
	}

	ent->client->resp.osp_r078 = 0;
}

// gamex86.dll: 10034944..100349F2
// gamei386.so: 00061820..0006190A
int OSP_updateAdminMenu (edict_t *ent)
{
	if (m_mode > 0)
	{
		sprintf (admin_title, "*Match Control");
		AdminMain_Menu[9].SelectFunc = NULL;
		if (m_mode > 1)
			AdminMain_Menu[12].SelectFunc = OSP_returnMainTeam_menu;
		else
			AdminMain_Menu[12].SelectFunc = OSP_returnMainDM_menu;
	}
	else
	{
		sprintf (admin_title, " ");
		AdminMain_Menu[9].SelectFunc = NULL;
		AdminMain_Menu[12].SelectFunc = OSP_returnMainDM_menu;
	}

	AdminMain_Menu[9].text = admin_title;
	AdminMain_Menu[4].arg = &admin_mode_map;
	AdminMain_Menu[5].arg = &admin_mode_ban;
	AdminMain_Menu[7].arg = &admin_mode_kick;

	if (m_mode > 0)
		return 9;
	return 4;
}

// gamex86.dll: 100349F2..10034A0F
// gamei386.so: 0006190C..0006195C
void OSP_returnMainAdmin_menu (edict_t *ent, pmenu_t *p)
{
	PMenu_Close (ent);
	OSP_adminMenu (ent);
}

// The referee's select-a-thing page. resp.osp_r238 says which of the three
// admin actions is being set up and rewrites the whole menu for it; anything
// else is the mod's own "[ ERROR ERROR ]" fallback. Note the player arms index
// `game.clients` by client NUMBER while the address line indexes `g_edicts` by
// the same number plus one.
// gamex86.dll: 10034A0F..10034E00
// gamei386.so: 0006195C..00061DB5
int OSP_updateAdminSelectMenu (edict_t *ent)
{
	int		which = ent->client->resp.osp_r238;		// invented name

	if (which == 1)
	{
		strcpy (as_title, "[ KICK Player Menu ]");
		strcpy (as_prompt, "*Select player to KICK:");

		if (ent->client->resp.osp_r290 == -1)
		{
			strcpy (as_choice, "*[ SELECT ]");
			strcpy (as_addr, " ");
			strcpy (as_action, " ");
			AdminSelect_Menu[11].SelectFunc = NULL;
		}
		else
		{
			sprintf (as_choice, "%s",
					 game.clients[ent->client->resp.osp_r290].pers.netname);
			sprintf (as_addr, "[ %s ]",
					 g_edicts[ent->client->resp.osp_r290 + 1].osp_e37c);
			strcpy (as_action, "*KICK selected player");
			AdminSelect_Menu[11].SelectFunc = OSP_playerAdminChoose;
		}
		AdminSelect_Menu[4].SelectFunc = OSP_playerAdminSelect_menu;
	}
	else if (which == 2)
	{
		strcpy (as_title, "[ *BAN* Player Menu ]");
		strcpy (as_prompt, "*Select player to *BAN*:");

		if (ent->client->resp.osp_r290 == -1)
		{
			strcpy (as_choice, "*[ SELECT ]");
			strcpy (as_addr, " ");
			strcpy (as_action, " ");
			AdminSelect_Menu[11].SelectFunc = NULL;
		}
		else
		{
			sprintf (as_choice, "%s",
					 game.clients[ent->client->resp.osp_r290].pers.netname);
			sprintf (as_addr, "[ %s ]",
					 g_edicts[ent->client->resp.osp_r290 + 1].osp_e37c);
			strcpy (as_action, "*BAN selected player");
			AdminSelect_Menu[11].SelectFunc = OSP_playerAdminChoose;
		}
		AdminSelect_Menu[4].SelectFunc = OSP_playerAdminSelect_menu;
	}
	else if (which == 4)
	{
		strcpy (as_title, "[ Map Selection Menu ]");
		strcpy (as_prompt, "*Select new map to load:");
		strcpy (as_addr, " ");
		AdminSelect_Menu[4].SelectFunc = OSP_mapAdminSelect_menu;
		AdminSelect_Menu[11].SelectFunc = OSP_mapAdminChoose;

		if (!map_size)
		{
			sprintf (as_choice, "[ NO MAPS AVAILABLE ]");
			sprintf (as_action, " ");
			AdminSelect_Menu[4].SelectFunc = NULL;
			AdminSelect_Menu[11].SelectFunc = NULL;
		}
		else if (ent->client->resp.osp_r290 == -1)
		{
			strcpy (as_choice, "*[ SELECT ]");
			sprintf (as_action, " ");
			AdminSelect_Menu[11].SelectFunc = NULL;
		}
		else
		{
			sprintf (as_choice, "%s", map[ent->client->resp.osp_r290].name);
			strcpy (as_action, "*Load selected map");
			AdminSelect_Menu[11].SelectFunc = OSP_mapAdminChoose;
		}
	}
	else
	{
		strcpy (as_title, "[ ERROR ERROR ]");
		strcpy (as_prompt, "*ERROR IN ADMIN MENU");
		strcpy (as_choice, " ");
		strcpy (as_addr, " ");
	}

	AdminSelect_Menu[1].text = as_title;
	AdminSelect_Menu[4].text = as_prompt;
	AdminSelect_Menu[6].text = as_choice;
	AdminSelect_Menu[7].text = as_addr;
	AdminSelect_Menu[11].text = as_action;

	if (m_mode > 0)
		return 9;
	return 4;
}

// gamex86.dll: 10034E00..10034ECD
// gamei386.so: 00061DB8..00061E4A
void OSP_mapAdminSelect_menu (edict_t *ent, pmenu_t *p)
{
	if (ent->client->resp.osp_r264)
		ent->client->resp.osp_r290--;
	else
		ent->client->resp.osp_r290++;

	if (ent->client->resp.osp_r290 < -1)
		ent->client->resp.osp_r290 = map_size - 1;
	else if (ent->client->resp.osp_r290 == map_size)
	{
		ent->client->resp.osp_r290 = -1;
		ent->client->resp.osp_r254 = 0;
	}

	OSP_updateAdminSelectMenu (ent);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

// Step to the next/previous connected client. resp.osp_r290 holds a client
// NUMBER here rather than an index into a table, so the search skips free
// edicts and half-connected slots in whichever direction it is going.
// gamex86.dll: 10034ECD..10034FF4
// gamei386.so: 00061E4C..00061F4E
void OSP_playerAdminSelect_menu (edict_t *ent, pmenu_t *p)
{
	edict_t		*other;
	int			i;
	int			found;

	found = -1;
	if (ent->client->resp.osp_r264)
	{
		for (i = ent->client->resp.osp_r290 - 1; i >= 0; i--)
		{
			other = g_edicts + i + 1;
			if (!(other->inuse && other->client && other->client->pers.connected))
				continue;
			found = i;
			break;
		}
	}
	else
	{
		for (i = ent->client->resp.osp_r290 + 1; i < game.maxclients; i++)
		{
			other = g_edicts + i + 1;
			if (!(other->inuse && other->client && other->client->pers.connected))
				continue;
			found = i;
			break;
		}
	}

	ent->client->resp.osp_r290 = found;
	OSP_updateAdminSelectMenu (ent);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

// A referee picking a map out of the admin menu ends the level immediately --
// the same "soft" end a map vote uses, so the stats logs get a game-end record
// before the change.
// gamex86.dll: 10034FF4..10035094
// gamei386.so: 00061F50..00062019
void OSP_mapAdminChoose (edict_t *ent, pmenu_t *p)
{
	int			sel;

	sel = ent->client->resp.osp_r290;
	PMenu_Close (ent);

	if (sel > -1 && OSP_mapExists (ent, map[sel].name, true))
	{
		sl_SoftGameEnd (&gi, level);
		q2log_gameEnd ("referee map change", 0);
		manual_map = 1;
		EndDMLevel ();
		return;
	}

	OSP_adminMenu (ent);
}

// The referee's admin-select leaf. resp.osp_r238 says which admin action the
// selection is for -- 2 is a ban (which hands off to OSP_rban_cmd), anything
// else is a kick, and a kicked bot goes through the Gladiator SDK's
// `removebot` rather than an svc_disconnect.
// gamex86.dll: 10035094..10035260
// gamei386.so: 0006201C..000621F1
void OSP_playerAdminChoose (edict_t *ent, pmenu_t *p)
{
	edict_t		*target = g_edicts + ent->client->resp.osp_r290 + 1;

	if (ent->client->resp.osp_r290 > -1)
	{
		if (ent->client->resp.osp_r238 == 2)
			OSP_rban_cmd (ent, target->client->pers.netname);
		else if (target == ent)
			gi.cprintf (ent, PRINT_HIGH, "Sorry, you can't kick yourself!\n");
		else if (!target->inuse || !target->client ||
				 !target->client->pers.connected)
			gi.cprintf (ent, PRINT_HIGH,
						"Sorry, player is no longer connected!\n");
		else
		{
			gi.bprintf (PRINT_CHAT, "%s has been kicked!\n",
						target->client->pers.netname);

			if (server_log)
			{
				OSP_getPlayerAddr (target);
				OSP_logAdminLog ("Referee_Kick: %s -> %s [%s]",
								 ent->client->pers.netname,
								 target->client->pers.netname,
								 target->osp_e37c);
			}

			if (target->flags & FL_OSP_NOCMD)
			{
				BotServerCommand ("sv", "removebot",
								  target->client->pers.netname, 0);
				// The target's own oddity, the same five instructions as in
				// OSP_kickplayer_cmd: the counter is subtracted from itself and
				// the (always zero) result clamped.
				bots_votedin -= bots_votedin;
				if (bots_votedin < 0)
					bots_votedin = 0;
			}
			else
			{
				OSP_startObserve (target);
				gi.WriteByte (svc_disconnect);
				gi.unicast (target, true);
				ClientDisconnect (target);
			}
		}
	}

	PMenu_Close (ent);
	OSP_adminMenu (ent);
}
