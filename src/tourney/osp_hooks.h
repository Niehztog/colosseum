// osp_hooks.h -- the tourney surface a SHARED file is allowed to see.
//
// WHY THIS IS NOT osp_types.h.  osp_types.h is the donor's own private header:
// it declares tourney's internals AND, because the donor is one flat tree, a
// number of names that baseq2 defines as `static` in its own files --
// `Use_Quad`, `Use_Invulnerability` and friends.  Including it from g_items.c
// makes those static definitions follow a non-static declaration, which is an
// error, and papering over that by reordering includes would make the spine
// depend on the donor's declaration order.
//
// So the spine sees this instead: the entry points and the state that
// R-MODE-5's gates actually name, and nothing else.  osp_types.h includes it,
// so there is one declaration of each, and a tourney file that changes a
// signature cannot drift from what the spine calls.

#ifndef OSP_HOOKS_H
#define OSP_HOOKS_H

// ---- match state the gates read -------------------------------------------

// R-OSP-12's four match modes: 0 free-for-all, 1 qualifier, 2 team, 3 1-vs-1.
extern  int     m_mode;
// The match state machine: <2 warmup, 2 countdown, >2 live, 4 fully underway.
extern  int     sync_stat;
// Which of the five runes are in play, as RUNE_* bits.  0 disables them.
extern  int     rune_stat;
// 0 running, 1 paused by a rule, 2 frozen, 3 paused by a player.
extern  int     match_paused;
extern  float   pause_time;
// The hi-score board's mode; 0 means there is none.
extern  int     hs_mode;
extern  int     active_clients;

#define RUNE_RESIST             1
#define RUNE_STRENGTH           2
#define RUNE_HASTE              4
#define RUNE_REGEN              8
#define RUNE_VAMPIRE            16

// resp.osp_entered's four states.  R-58: this is NOT baseq2's `resp.entered` bool,
// and the two share a name in the merged struct (SPECS.md 1.19).
#define ENTERED_NO              0
#define ENTERED_ENTERED         1
#define ENTERED_OBSERVER        2
#define ENTERED_QUEUED          3

// The three the BOT LAYER reads through its ruleset-neutral accessors
// (R-BOT-29).  They are tourney's own globals; bl_main.c must never see them
// except behind BotTourneyHook()/BotTourneyVotedIn(), because an extern here
// resolves in every ruleset and answers with tourney's state whether or not
// tourney is running.
extern  cvar_t  *hook_enable;
extern  int      bots_votedin;

extern  cvar_t  *runes_model;
extern  cvar_t  *ffa_hurtself;
extern  cvar_t  *client_deathweapdrop;
extern  int      bot_watch;

// The Standard Log's four spine-facing writers (R-OSP-3).  They take the game
// import and the level by value because that is the log library's own
// interface, not this tree's.
void     sl_GameStart(game_import_t *import, level_locals_t lev);
void     sl_GameEnd(game_import_t *import, level_locals_t lev);
void     sl_LogPlayerDisconnect(game_import_t *import, level_locals_t lev,
                                struct edict_s *ent);
void     sl_WriteStdLogDeath(game_import_t *import, level_locals_t lev,
                             struct edict_s *self, struct edict_s *inflictor,
                             struct edict_s *attacker);
void     sl_WriteStdLogPlayerEntered(game_import_t *import, level_locals_t lev,
                                     struct edict_s *ent);

// ---- entry points ---------------------------------------------------------

struct edict_s;

// The delegated client-command dispatcher (R-OSP-2).  True when it handled it.
bool     OSP_ClientCommand(struct edict_s *ent);
bool     OSP_ServerCommand(const char *cmd);

// Runes, as the spine reaches them.
void     OSP_runesShell(struct edict_s *ent);
int      OSP_runesApplyStrength(struct edict_s *ent, int damage);
int      OSP_runesApplyResistance(struct edict_s *ent, int damage);
void     OSP_runesApplyVampire(struct edict_s *ent, int damage);
bool     OSP_runesApplyStrengthSound(struct edict_s *ent);
void     OSP_runesApplyHasteSound(struct edict_s *ent);
bool     OSP_runesHasHaste(struct edict_s *ent);
bool     OSP_runesHasRegeneration(struct edict_s *ent);
bool     OSP_runesHasVampire(struct edict_s *ent);
bool     OSP_runesHoldHealth(struct edict_s *ent);
void     OSP_runesApplyRegeneration(struct edict_s *ent);
bool     OSP_Pickup_Rune(struct edict_s *ent, struct edict_s *other);
void     OSP_Drop_Rune(struct edict_s *ent, const gitem_t *item);
void     OSP_runeThink(struct edict_s *self);
void     OSP_deadDropRune(struct edict_s *ent);

// The accuracy table's two entry points -- src/tourney/osp_acc.c.
void     OSP_accShot(struct edict_s *self, int mod, int count);
void     OSP_accDamage(struct edict_s *targ, struct edict_s *attacker,
                       int mod, int take);

// The HUD panels tourney owns above baseq2's stats.
void     OSP_clearStats(struct edict_s *ent);
void     OSP_setStats(struct edict_s *ent);
void     OSP_restartStats(struct edict_s *ent);

// Items a referee has switched off, and the respawn arithmetic that goes with
// them.
bool     OSP_disableItems(struct edict_s *ent);
bool     OSP_teamHasEnabled(struct edict_s *master);
float    OSP_respawnDelay(float delay);
struct edict_s *OSP_pickRespawnMember(struct edict_s *master);
void     OSP_packPlayer(struct edict_s *ent);

// The connect/begin/leave path.
void     OSP_giveClientID(struct edict_s *ent);
void     OSP_clientBeginPre(struct edict_s *ent);
bool     OSP_clientBegunPost(struct edict_s *ent);
void     OSP_clientBeginLevel(struct edict_s *ent);
// R-OSP-11's bot half: a bot has no key to press, so entering the game and
// readying up are done for it.  The join runs from ClientBeginDeathmatch after
// placement, the ready-up from the frame end.
void     OSP_botJoin(struct edict_s *ent);
void     OSP_botReady(void);
void     OSP_userinfoChanged(struct edict_s *ent, char *userinfo);
bool     OSP_clientAllowed(struct edict_s *ent, char *userinfo);
void     OSP_clientConnected(struct edict_s *ent, char *userinfo);
void     OSP_clientLeaving(struct edict_s *ent, int *out_team);
bool     OSP_clientLeft(struct edict_s *ent, int tno);
void     OSP_seedPlayer(gclient_t *client);
void     OSP_recoverClient(struct edict_s *ent, char *userinfo);
void     OSP_saveClient(struct edict_s *ent);
bool     OSP_readdTeamMember(struct edict_s *ent);
void     OSP_startObserve(struct edict_s *ent);
bool     OSP_clientThink(struct edict_s *ent, usercmd_t *ucmd);
void     OSP_warmupItems(struct edict_s *ent);
void     OSP_setSingleAccuracy(struct edict_s *ent);
void     OSP_DoRankSort(void);
bool     OSP_botDetect(struct edict_s *ent, usercmd_t *ucmd);
void     OSP_speedDetect(struct edict_s *ent);
void     OSP_playerAnnounce(struct edict_s *ent, char sound);
void     OSP_checkHalt(int reason);
void     OSP_clearVotes(void);
void     OSP_closeMenus(void);
void     OSP_checkVote(void);
void     OSP_removeChaseCam(struct edict_s *ent);
void     OSP_1v1Add(struct edict_s *ent);
void     OSP_1v1Remove(struct edict_s *ent, int mode);
void     OSP_initTeamFrags(struct edict_s *ent);
void     OSP_playerTeamFrags(struct edict_s *ent);
void     OSP_scoreChange(struct edict_s *who, int delta);
void     OSP_removeTeamMember(struct edict_s *ent, bool quiet);
void     OSP_notready_cmd(struct edict_s *ent, int quiet);
void     OSP_logAdminLog(char *fmt, ...);
void     OSP_zeroRuneStats(struct edict_s *ent);
int      OSP_teamCount(int team);
bool     OSP_teamFriendlyFire(int team);
bool     OSP_teamSelfDamage(int team);
const char *OSP_teamName(int team);
int      OSP_teamFrags(int team);
void     OSP_DMMenu(struct edict_s *ent);
void     OSP_teamMenu(struct edict_s *ent);
void     OSP_ChaseCam(struct edict_s *ent);
void     OSP_hookAliases(struct edict_s *ent);
void     OSP_hookoff_cmd(struct edict_s *ent);
void     GrapplePull(struct edict_s *self);
void     OSP_showFrags(struct edict_s *ent);
void     OSP_setShowParams(void);
void     OSP_accuracyInfo(struct edict_s *ent, char *name, int cid);
int      OSP_playerAllow(char *name, char *userinfo);
void     OSP_showScores(int *list, int count, struct edict_s *ent);
void     OSP_ScoreboardMessage(struct edict_s *ent, struct edict_s *killer);
void     OSP_showTeamScores(struct edict_s *ent);
void     OSP_show1v1Scores(struct edict_s *ent);
void     OSP_showHighScores(void);
void     OSP_showMOTD(void);
void     OSP_setMOTD(void);
void     OSP_initHighScores(void);
void     OSP_teamReset(void);
int      OSP_clampMatchMode(void);
void     OSP_worldspawn(void);
void     OSP_frameStart(void);
void     OSP_frameEnd(void);
void     OSP_pauseFrame(void);
bool     OSP_exitLevel(void);
void     OSP_levelSpawned(void);
void     CameraThink(struct edict_s *ent);
void     PlayerDied(struct edict_s *ent);

// The two statistics logs (R-OSP-3), at the spine sites that feed them.
// The stats log's spine-facing entry points; osp_stats.h owns the signatures.
#include "tourney/osp_stats.h"

#endif // OSP_HOOKS_H
