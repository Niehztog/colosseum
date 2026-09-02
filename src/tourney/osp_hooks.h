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

// R-OSP-12's four modes of play are four RULESETS -- `dm`, `dmpro`, `tdm`,
// `duel` -- and the `m_mode` global that used to select among them is gone.
// A site that means one of them tests the ruleset by name; the two questions
// below are the ones that meant a RANGE of modes and would otherwise be spelled
// as a list at every call site.
//
// The old spellings map exactly, which is how the rewrite was checked.  The
// left column is the DONOR's, and it is quoted rather than rewritten -- the
// pass that renamed the identifiers had run over this table too, leaving both
// columns saying the same thing and the document explaining nothing:
//
//      m_mode == 0 / !m_mode              ->  G_Ruleset() == RULESET_DM
//      m_mode == 1                        ->  G_Ruleset() == RULESET_DMPRO
//      m_mode == 2                        ->  G_Ruleset() == RULESET_TDM
//      m_mode == 3                        ->  G_Ruleset() == RULESET_DUEL
//      m_mode (bare, truthy) / m_mode > 0 ->  OSP_IsMatch()
//      m_mode > 1  / m_mode >= 2          ->  OSP_IsTeams()
//      m_mode < 2  / m_mode <= 1          ->  !OSP_IsTeams()

// Is there a match system -- a ready gate, a countdown, a referee?  Everything
// but `dm`, which is OSP's RegularDM and starts with sync_stat already at 8.
//
// Both are ordinary functions rather than inlines on purpose: this header is
// included by twelve shared files and does not itself include `g_ruleset.h`,
// which cannot be included before `edict_t` exists.  A body here would compile
// only because of the order `g_local.h` happens to use.
bool    OSP_IsMatch(void);

// Are the players on two OSP teams?  `tdm` and `duel` -- a duel is two teams of
// one.  NOT the same question as the brain's `teamplay` libvar, which is
// RULESET_TDM alone: see R-BOT-29 and the comment on G_TeamplayEnabled().
bool    OSP_IsTeams(void);

// The declared roster size of one OSP team: `team_maxplayers`, 4 by default and
// forced to 1 CVAR_NOSET under `duel`.  Reached through a function because the
// cvar's DEFAULT is ruleset-dependent, so a call site that spelled one would be
// R-COMPAT-6's collision -- one cvar registered twice with two values.
int     OSP_TeamMaxPlayers(void);

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
// Every slot with a connected client, bots included.  Recounted rather than
// decremented (OSP_clientLeft), which is what makes it survive a client that
// left without ever entering.
extern  int     connected_clients;
// The countdown bitmask: which of the 10/5/1-minute and 5..1-second marks have
// been announced.  A client caches the value it was last told about in
// resp.osp_r01c, which is why InitClientResp has to seed it.
extern  int     start_count;
// R-193: the two intermission timers ClientThink reads, and `manual_map`, which
// distinguishes a map an operator typed from one the rotation chose.
extern  cvar_t  *nextlevel_click;
extern  cvar_t  *nextlevel_lazy;
extern  int     manual_map;
// R-193: what tourney does to each client when the level ends -- the demo it
// was recording, the music it hears and the accuracy page it is shown.
extern  cvar_t  *match_endmusic;
extern  cvar_t  *demo_referee;
extern  cvar_t  *demo_player;
extern  char     wav_file[125];
void     OSP_accuracyInfo(struct edict_s *ent, char *name, int cid);
void     OSP_closeMenus(void);
bool     OSP_teamLost(int team);

// R-193: `numgibs` under tourney, 4 everywhere else -- one question so that
// p_client.c's two gib loops do not each carry a ruleset test.
int      OSP_GibCount(void);
// Sudden death: the number the team fraglimit is offset by once a drawn match
// runs out of overtime.  Non-zero means "the next frag ends it".
extern  int     frag_offset;
// Which of the four statusbar variants the server composed; a client's own
// choice starts here and `hud` toggles it.
extern  cvar_t  *client_hud;

#define RUNE_RESIST             1
#define RUNE_STRENGTH           2
#define RUNE_HASTE              4
#define RUNE_REGEN              8
#define RUNE_VAMPIRE            16

// resp.osp_entered's five states.  R-58: this is NOT baseq2's `resp.entered`
// bool, and the two share a name in the merged struct (SPECS.md 1.19).
//
// They are BITS in the donor's numbering -- 1, 2, 4, 8, 16 -- and every donor
// site compares them for EQUALITY, so they are five states rather than a mask.
// `ENTERED_QUEUED 3` stood here and is not one of them: no donor site writes or
// reads a 3, and the four states this comment claimed were three plus an
// invention (doc/reconciliation.md R-191).  The literals still appear as
// literals at most sites, which is the donor's own text; these names are for
// the sites that were written with them.
#define ENTERED_NO              0   // the InitClientResp memset value, which
                                    // the donor never writes by hand
#define ENTERED_ENTERED         1   // playing
#define ENTERED_OBSERVER        2   // observing, free-flying
#define ENTERED_CHASECAM        4   // chasing one player, with free-look and
                                    // zoom (g_chase.c reads exactly this)
#define ENTERED_INEYES          8   // chasing in-eyes: no free-look, no zoom
#define ENTERED_AUTOCAM         16  // the camera picks its own subject

// The three the BOT LAYER reads through its ruleset-neutral accessors
// (R-BOT-29).  They are tourney's own globals; bl_main.c must never see them
// except behind BotTourneyHook()/BotTourneyVotedIn(), because an extern here
// resolves in every ruleset and answers with tourney's state whether or not
// tourney is running.
extern  cvar_t  *hook_enable;
extern  int      bots_votedin;

// R-193: the chase camera's two settings, read by g_chase.c -- `camera_depth`
// is the distance a new target is watched from and `camera_pitch` the free-look
// pitch offset.  Both were registered and clamped and neither value was read
// until the camera's consumer arrived.
extern  cvar_t  *camera_depth;
extern  cvar_t  *camera_pitch;

extern  cvar_t  *runes_model;
extern  cvar_t  *ffa_hurtself;
extern  cvar_t  *client_deathweapdrop;
// Three frames of the weapon raise/lower animation per server frame instead of
// one (R-OSP-1).  Read by p_weapon.c's Think_Weapon.
extern  cvar_t  *client_fastweap;
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

// "Is somebody standing on this spawn point?"  True refuses the placement, which
// PutClientInServer answers with a frozen, bodiless client that R-191's respawn
// trigger retries.
bool     OSP_spawnRefused(struct edict_s *ent, struct edict_s *spot);
// R-185: the two reads the import dropped.  Both answer with the donor's fixed
// value outside RegularDM, which is what upstream's `!m_mode` gate means.
int      OSP_forcedRespawnDelay(void);
float    OSP_powerArmorPerCell(bool screen);
struct edict_s *OSP_pickRespawnMember(struct edict_s *master);
void     OSP_packPlayer(struct edict_s *ent);
// The armour ceiling and the shard's worth, as Pickup_Armor asks them.  0 and 2
// outside tourney, which is baseq2's own behaviour.
// A powerup freed in the world: logs it if its own timer ran out (R-OSP-3).
void     OSP_itemFreed(struct edict_s *ed);
int      OSP_armorCeiling(struct edict_s *ent);
int      OSP_armorShard(void);

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
// The ping / inactivity / framerate rules (R-OSP-4).  True means the client is
// gone and the caller must not touch it again.
bool     OSP_clientPolice(struct edict_s *ent, usercmd_t *ucmd);
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
// R-193's observer input, which ClientThink reaches: the autocam (true when it
// took the client) and the mode line the stats log records.
bool     CameraCmd(struct edict_s *ent, bool force);
void     OSP_Stats_PlayerMode(struct edict_s *ent, const char *mode);
void     OSP_1v1Add(struct edict_s *ent);
void     OSP_1v1Remove(struct edict_s *ent, int mode);
void     OSP_initTeamFrags(struct edict_s *ent);
void     OSP_playerTeamFrags(struct edict_s *ent);
// The obituary, in the donor's three shapes.  Each owns both the printing and
// the frag accounting for its shape -- see src/tourney/osp_teams.c for why they
// cannot be one `score += delta`.
bool     OSP_obituaryHush(void);
void     OSP_obituarySelf(struct edict_s *self, const char *message);
void     OSP_obituaryFrag(struct edict_s *self, struct edict_s *attacker,
                          const char *message, const char *message2, bool ff);
void     OSP_obituaryDied(struct edict_s *self);
void     OSP_removeTeamMember(struct edict_s *ent, bool quiet);
void     OSP_notready_cmd(struct edict_s *ent, int quiet);
void     OSP_logAdminLog(char *fmt, ...);
// Caches ent->osp_e37c, the client's address with the port stripped.
void     OSP_getPlayerAddr(struct edict_s *ent);
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
