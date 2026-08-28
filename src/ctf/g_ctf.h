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

// Threewave CTF 1.52.  Included from the bottom of g_local.h, as the donor has
// it; §5.2 gives it src/ctf/ and R-25 item 3 requires the qualified include.

#ifndef CTF_G_CTF_H
#define CTF_G_CTF_H

#include "ctf/p_menu.h"

#define CTF_VERSION         1.52
#define CTF_VSTRING2(x) #x
#define CTF_VSTRING(x) CTF_VSTRING2(x)
#define CTF_STRING_VERSION  CTF_VSTRING(CTF_VERSION)

// Threewave's fourteen stat slots were `#define STAT_CTF_TEAM1_PIC 17` here.
// They are now rows in g_stats.h's per-ruleset map, as SID_CTF_* (R-OSP-7
// clause 3), because 17, 18 and 19 are baseq2's STAT_SPECTATOR, STAT_TIMER2_ICON
// and STAT_TIMER2 and one library has to hold both meanings.  A slot number
// appears in exactly one place in this tree and it is not here.

// The two configstrings CTF steals from the tail of the statusbar span.  They
// must be taken from the **runtime** remap, not from the compile-time constant:
// this library is compiled with USE_PROTOCOL_EXTENSIONS, so CS_AIRACCEL is 59,
// but a server that did not negotiate the extensions runs on cs_remap_old where
// airaccel is 29 -- and 57/58 then land inside the old model table rather than
// in the statusbar span.  Same defect and same fix as the game.csr.general
// sites in p_client.c and g_ctf.c (doc/reconciliation.md R-62); the idiom is
// g_local.h's PM_TIME_SHIFT.
#define CONFIG_CTF_MATCH    (game.csr.airaccel - 1)
#define CONFIG_CTF_TEAMINFO (game.csr.airaccel - 2)

typedef enum {
    CTF_NOTEAM,
    CTF_TEAM1,
    CTF_TEAM2
} ctfteam_t;

typedef enum {
    CTF_GRAPPLE_STATE_FLY,
    CTF_GRAPPLE_STATE_PULL,
    CTF_GRAPPLE_STATE_HANG
} ctfgrapplestate_t;

// The typedef name is in g_local.h, forward-declared, because client_respawn_t
// holds one.  Defining it again here would be a duplicate typedef.
struct ghost_s {
    char netname[16];
    int number;

    // stats
    int deaths;
    int kills;
    int caps;
    int basedef;
    int carrierdef;

    int code; // ghost code
    int team; // team
    int score; // frags at time of disconnect
    edict_t *ent;
};

#define CTF_TEAM1_SKIN "ctf_r"
#define CTF_TEAM2_SKIN "ctf_b"

#define DF_CTF_FORCEJOIN    131072
#define DF_ARMOR_PROTECT    262144
#define DF_CTF_NO_TECH      524288

#define CTF_CAPTURE_BONUS       15  // what you get for capture
#define CTF_TEAM_BONUS          10  // what your team gets for capture
#define CTF_RECOVERY_BONUS      1   // what you get for recovery
#define CTF_FLAG_BONUS          0   // what you get for picking up enemy flag
#define CTF_FRAG_CARRIER_BONUS  2   // what you get for fragging enemy flag carrier
#define CTF_FLAG_RETURN_TIME    40  // seconds until auto return

#define CTF_CARRIER_DANGER_PROTECT_BONUS    2   // bonus for fraggin someone who has recently hurt your flag carrier
#define CTF_CARRIER_PROTECT_BONUS           1   // bonus for fraggin someone while either you or your target are near your flag carrier
#define CTF_FLAG_DEFENSE_BONUS              1   // bonus for fraggin someone while either you or your target are near your flag
#define CTF_RETURN_FLAG_ASSIST_BONUS        1   // awarded for returning a flag that causes a capture to happen almost immediately
#define CTF_FRAG_CARRIER_ASSIST_BONUS       2   // award for fragging a flag carrier if a capture happens almost immediately

#define CTF_TARGET_PROTECT_RADIUS           400 // the radius around an object being defended where a target will be worth extra frags
#define CTF_ATTACKER_PROTECT_RADIUS         400 // the radius around an object being defended where an attacker will get extra frags when making kills

#define CTF_CARRIER_DANGER_PROTECT_TIMEOUT  8
#define CTF_FRAG_CARRIER_ASSIST_TIMEOUT     10
#define CTF_RETURN_FLAG_ASSIST_TIMEOUT      10

#define CTF_AUTO_FLAG_RETURN_TIMEOUT        30  // number of seconds before dropped flag auto-returns

#define CTF_TECH_TIMEOUT                    60  // seconds before techs spawn again

#define CTF_GRAPPLE_SPEED                   650 // speed of grapple in flight
#define CTF_GRAPPLE_PULL_SPEED              650 // speed player is pulled at

void CTFInit(void);
void CTFSpawn(void);
void CTFPrecache(void);

void SP_info_player_team1(edict_t *self);
void SP_info_player_team2(edict_t *self);

char *CTFTeamName(int team);
char *CTFOtherTeamName(int team);
void CTFAssignSkin(edict_t *ent, char *s);
void CTFForceAssignTeam(gclient_t *who);
void CTFAssignTeam(gclient_t *who);
// R-CTF-8: `ctf_botfill`'s two halves -- how many players the map and its two
// bases seat, and which bot to remove when a person takes one of the seats.
// Both unclamped and neither reads the switch; BotFillTarget() owns that.
int  CTF_BotFillSeats(void);
char *CTFBotFillName(void);
edict_t *SelectCTFSpawnPoint(edict_t *ent);
bool CTFPickup_Flag(edict_t *ent, edict_t *other);
void CTFDrop_Flag(edict_t *ent, const gitem_t *item);
void CTFEffects(edict_t *player);
void CTFCalcScores(void);
void SetCTFStats(edict_t *ent);
void CTFDeadDropFlag(edict_t *self);
void CTFScoreboardMessage(edict_t *ent, edict_t *killer);
void CTFTeam_f(edict_t *ent);
void CTFID_f(edict_t *ent);
void CTFSay_Team(edict_t *who, char *msg);
void CTFFlagSetup(edict_t *ent);
void CTFResetFlag(int ctf_team);
void CTFFragBonuses(edict_t *targ, edict_t *inflictor, edict_t *attacker);
void CTFCheckHurtCarrier(edict_t *targ, edict_t *attacker);

// GRAPPLE
void CTFWeapon_Grapple(edict_t *ent);
void CTFPlayerResetGrapple(edict_t *ent);
void CTFGrapplePull(edict_t *self);
void CTFResetGrapple(edict_t *self);

//TECH
const gitem_t   *CTFWhat_Tech(edict_t *ent);
bool CTFPickup_Tech(edict_t *ent, edict_t *other);
void CTFDrop_Tech(edict_t *ent, const gitem_t *item);
void CTFDeadDropTech(edict_t *ent);
void CTFSetupTechSpawn(void);
int CTFApplyResistance(edict_t *ent, int dmg);
int CTFApplyStrength(edict_t *ent, int dmg);
bool CTFApplyStrengthSound(edict_t *ent);
bool CTFApplyHaste(edict_t *ent);
void CTFApplyHasteSound(edict_t *ent);
void CTFApplyRegeneration(edict_t *ent);
bool CTFHasRegeneration(edict_t *ent);
void CTFRespawnTech(edict_t *ent);
void CTFResetTech(void);

void CTFOpenJoinMenu(edict_t *ent);
bool CTFStartClient(edict_t *ent);
void CTFVoteYes(edict_t *ent);
void CTFVoteNo(edict_t *ent);
void CTFReady(edict_t *ent);
void CTFNotReady(edict_t *ent);
bool CTFNextMap(void);
bool CTFMatchSetup(void);
bool CTFMatchOn(void);
void CTFGhost(edict_t *ent);
void CTFAdmin(edict_t *ent);
bool CTFInMatch(void);
void CTFStats(edict_t *ent);
void CTFWarp(edict_t *ent);
void CTFBoot(edict_t *ent);
void CTFPlayerList(edict_t *ent);

bool CTFCheckRules(void);

void SP_misc_ctf_banner(edict_t *ent);
void SP_misc_ctf_small_banner(edict_t *ent);

// Threewave's own grapple, chosen as the one implementation of the concept
// (§7 rule 6); the offhand `ctf_hook` and `laserhook` variants of R-CTF-3.
void CTFHook_f(edict_t *ent);
void CTFUnhook_f(edict_t *ent);
void CTFHookThink(edict_t *ent);
bool CTFHookIsOffhand(void);

#define CTF_HOOK_STATE_ON       1
#define CTF_HOOK_STATE_TURNOFF  2
#define CTF_HOOK_STATE_FIRED    4

extern cvar_t *ctf_hook;
extern cvar_t *laserhook;

// The ctf row of the ruleset dispatch (R-MODE-6).  Lives with the ruleset it
// implements rather than in g_ruleset.c, so that adding a ruleset means adding
// a file and one table entry.
extern const ruleset_ops_t ops_ctf;



// `extern char *ctf_statusbar;` -- gone.  R-OSP-7a: the bar is emitted from the
// slot map by g_stats.c, not stored as a literal.

void CTFObserver(edict_t *ent);

// Threewave's teleporter, prefixed: Ground Zero ships a `trigger_teleport` of
// its own with different semantics and the same classname, so §7 rule 4 gives
// each the donor prefix and g_misc.c dispatches (doc/reconciliation.md R-44).
void ctf_SP_trigger_teleport(edict_t *ent);
void ctf_SP_info_teleport_destination(edict_t *ent);

void CTFSetPowerUpEffect(edict_t *ent, int def);

#endif // CTF_G_CTF_H
