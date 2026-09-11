// Colosseum ruleset dispatch
//
// The 1999 Gladiator build used three independent booleans (`ctf`,
// `rocketarena`, `ch`) and then had to police them: InitGame force-reset them
// and called gi.error on ctf && ra, ctf && ch, ra && ch.  That is a constraint
// expressed as an error message.  This expresses it as a type: exactly one
// ruleset is active, chosen once, and it cannot be in two states at all.
//
// The shape of a gate, and why there is no 26-entry vtable here.
//
// The gate is sized to what the ruleset does: a dispatch-table hook where a
// ruleset replaces the baseq2 implementation, a named predicate or a
// comment-fenced inline test where it only adjusts it.  The prior art says the
// same -- the only ruleset vtable in any Quake II tree is Rogue's
// `dm_game_rt`, which has 12 hooks, has been copied unchanged into five trees
// since 1998, and has exactly two rows ever filled.  Everything outside those
// twelve points is gated in q2pro-ng and quake2-rerelease-dll by a single named
// predicate or an inline test behind a donor-name comment fence, with zero
// #ifdef in the whole game tree.
//
// So: predicates below, and an ops table that grows a row when a phase produces
// a ruleset that genuinely replaces something.  A row per possible gate
// location would be 26 rows with one filled -- exactly the mistake
// `dm_game_rt` documents.  The hook count falls out of the merge rather than
// being fixed up front.

#ifndef G_RULESET_H
#define G_RULESET_H

// The four OSP rulesets are contiguous and first, and both halves are load
// bearing.  Contiguous makes G_IsOspRuleset() a range test rather than a
// four-way or that a later ruleset could be forgotten from.  First keeps
// RULESET_DM at zero, which `g_active_ruleset`'s initialiser and every
// zero-initialised copy of a ruleset_t already assume.
typedef enum {
    RULESET_DM,             // OSP RegularDM   -- was tourney + match_mode 0
    RULESET_DMPRO,          // OSP QualifierDM -- was match_mode 1
    RULESET_TDM,            // OSP TeamPlay    -- was match_mode 2
    RULESET_DUEL,           // OSP 1-vs-1      -- was match_mode 3
    RULESET_CTF,            // Threewave CTF 1.52
    RULESET_ARENA,          // Rocket Arena 2 v2.25
    RULESET_SP,             // single player and co-op
    RULESET_COUNT
} ruleset_t;

// Composable options within a ruleset.  `ch` and `sp_dm` were
// modifiers in spec 1.0-1.1 and are struck (N7, N8).
typedef enum {
    MOD_TEAMPLAY,
    MOD_HOOK,
    MOD_RUNES,              // techs under ctf, runes under the OSP four
    MOD_BOTS,
    MOD_COUNT
} modifier_t;

// Content layers: orthogonal to the ruleset, valid with every one of
// them, and both may be on at once.  Latched, like the ruleset.
typedef enum {
    LAYER_XATRIX,
    LAYER_ROGUE,
    LAYER_COUNT
} content_layer_t;

// ---------------------------------------------------------------- dispatch
//
// A NULL row means "inherit the BASE implementation" -- `ops_base`
// below, which is Q2PRO's baseq2 code and which no g_ruleset value selects.
// Rows are added when a phase brings a ruleset that replaces the behaviour, not
// in advance.
typedef struct {
    const char  *name;

    // Match rules: fraglimit/timelimit for the base, capturelimit for ctf, the
    // round state machine for arena, the match system for the OSP four, nothing
    // for sp.
    void        (*CheckRules)(void);

    // Where the level goes next.  The base rotates or repeats; sp follows
    // target_changelevel chains; arena and the OSP four own their own rotation
    // -- one of the four cases where two implementations both ship.
    void        (*EndLevel)(void);

    // The scoreboard layout string, and the intermission that shows it.
    void        (*ScoreboardMessage)(edict_t *ent, edict_t *killer);
    void        (*BeginIntermission)(edict_t *targ);

    // Spawn selection, and the separate question of what happens once the
    // player has a final origin.  It exists because the donors prove the need:
    // q2pro's spawn floor-clip trace sits in PutClientInServer, RA2's port had
    // to move it to move_to_arena(), and tourney's to ClientBeginDeathmatch.
    // All three placement paths will coexist here, so anything that must run
    // after placement hangs off one hook instead of being copied into three
    // functions.
    bool        (*SelectSpawnPoint)(edict_t *ent, vec3_t origin, vec3_t angles);
    void        (*ClientPlaced)(edict_t *ent);
} ruleset_ops_t;

// The two tables a donor defines.  Declared here rather than as a local
// `extern` in g_ruleset.c so the file that DEFINES each one sees the same
// declaration and the compiler checks the two against each other.
extern const ruleset_ops_t ops_arena;    // src/arena/arena.c
extern const ruleset_ops_t ops_tourney;  // src/tourney/osp_main.c -- all four
                                         // OSP rulesets share this one row set

// ---------------------------------------------------------------- accessors

void        G_InitRuleset(void);        // once, from InitGame, before anything reads a ruleset
// ...and once at the END of InitGame, after the ruleset's own init has
// registered its cvars: `runes` is derived from the switch the ruleset's own
// code reads rather than gating nothing.  See the comment on the
// implementation.
void        G_ResolveModifiers(void);
void        G_ApplyOspHookRequest(void);
void        G_QueueOspHookRequest(void);
bool        G_ApplyQueuedOspHookRequest(void);
ruleset_t   G_Ruleset(void);
const char *G_RulesetName(ruleset_t r);
const ruleset_ops_t *G_Ops(void);       // never NULL after G_InitRuleset()

bool        G_ModifierEnabled(modifier_t m);
bool        G_LayerEnabled(content_layer_t l);

// Ground Zero's DMGame callback table is a protocol between an active
// deathmatch ruleset and the Rogue game-rule implementation.  It is available
// under CTF and Arena only; OSP owns its own match protocol.
bool        G_UsesRogueGameRules(void);

// Is the OSP Tourney DM code path active?  `dm`, `dmpro`, `tdm` and `duel` are
// one donor's code selected four ways, so a gate that means "this is
// tourney's" asks this rather than naming four rulesets -- naming them is how
// the fifth one gets forgotten.  A gate that means one MODE still names it:
// `G_Ruleset() == RULESET_TDM` is a different question, and the bot layer
// depends on the two staying apart.
//
// `tools/donorgate.py` knows this predicate by name and accepts it as the gate
// for `src/tourney/`'s surface in a shared file.
bool        G_IsOspRuleset(void);

// ---------------------------------------------------------------- gates
//
// Call these, not G_Ops()->row directly.  "A ruleset that does not implement a
// hook inherits the base implementation" then lives in exactly one
// place instead of being re-implemented as a null check at every call site --
// which is how Rogue's dm_game_rt ended up with ~21 hand-written null checks
// around a table that has no default row.
void     G_CheckRules(void);
void     G_EndLevel(void);
void     G_ScoreboardMessage(edict_t *ent, edict_t *killer);
void     G_BeginIntermission(edict_t *targ);
bool     G_SelectSpawnPoint(edict_t *ent, vec3_t origin, vec3_t angles);
void     G_ClientPlaced(edict_t *ent);

// `sv ruleset` -- see the comment on the implementation.  The only way to
// observe the resolved ruleset from outside the library.
void     G_Svcmd_Ruleset_f(void);

// ---------------------------------------------------------------- predicates
//
// One named question per concept.  A call site asks the question
// it means; it does not test a cvar and hope the cvar still means that.

// Do monsters spawn?  Yes for ctf and sp; no for arena and no for the OSP four
// -- which includes `dm`, and did before the flattening too, by baseq2's own
// rule.  This one is load-bearing rather than cosmetic: baseq2 asks `if
// (deathmatch->value) G_FreeEdict(self)` in 24 places, and `deathmatch` is 1
// under ctf, so the inherited test would suppress the monsters that are
// promised under ctf.
bool G_MonstersAllowed(void);

// Is this a campaign -- baseq2, Reckoning or Ground Zero, single or co-op?
// The question `sp` really asks, and not the same as !deathmatch: a dedicated
// server forces deathmatch 1 unless coop is set.
bool G_IsCampaign(void);

// Are players on teams?  The q2pro-ng/quake2-rerelease shape -- one predicate
// rather than a hook.  Note the shape is borrowed and the ~20 call sites are
// Not: this tree has exactly two, DM_BotFillSeats() and the default arm of
// BotRulesetLibVars().
//
// Ruleset-derived since the flattening: ctf implies it, `tdm` and `duel` ARE it,
// and the MOD_TEAMPLAY modifier is refused across the OSP four so it
// only ever answers for arena now.
//
// Not the same question as the brain's `teamplay` libvar, which reads
// RULESET_TDM alone.  `duel` is two teams of one, the brain has no ally, and
// telling it otherwise gives every duellist an imaginary team-mate the moment
// both wear the same model.  Do not substitute one for the other.
bool G_TeamplayEnabled(void);

// May bots exist?  Every ruleset but sp (the Gladiator botlib is
// deathmatch-only), and the `bots` modifier, which defaults to on and is the
// operator's switch for the whole layer.
bool G_BotsAllowed(void);

// May the game be saved?  sp only, and additionally never while
// a bot exists.
bool G_SavegamesAllowed(void);

#endif // G_RULESET_H
