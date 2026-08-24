// Colosseum ruleset dispatch -- SPECS.md §5.3, R-MODE-1..7.
//
// The 1999 Gladiator build used three independent booleans (`ctf`,
// `rocketarena`, `ch`) and then had to police them: InitGame force-reset them
// and called gi.error on ctf && ra, ctf && ch, ra && ch.  That is a constraint
// expressed as an error message.  This expresses it as a type: exactly one
// ruleset is active, chosen once, and it cannot be in two states at all.
//
// THE SHAPE OF A GATE, and why there is no 26-entry vtable here.
//
// R-MODE-5 sizes the gate to what the ruleset does: a dispatch-table hook where
// a ruleset *replaces* the baseq2 implementation, a named predicate or a
// comment-fenced inline test where it only *adjusts* it.  It says so against
// measured prior art -- the only ruleset vtable in any Quake II tree is Rogue's
// `dm_game_rt`, which has 12 hooks, has been copied unchanged into five trees
// since 1998, and has exactly two rows ever filled.  Everything outside those
// twelve points is gated in q2pro-ng and quake2-rerelease-dll by a single named
// predicate or an inline test behind a donor-name comment fence, with zero
// #ifdef in the whole game tree.
//
// So: predicates below, and an ops table that grows a row when a phase produces
// a ruleset that genuinely replaces something.  A row per gate location named in
// R-MODE-5 would be 26 rows with one filled -- exactly the mistake `dm_game_rt`
// documents.  R-MODE-5 anticipates this: "The hook count therefore falls out of
// the merge rather than being fixed here."

#ifndef G_RULESET_H
#define G_RULESET_H

typedef enum {
    RULESET_DM,             // baseq2 deathmatch.  Implements every hook (R-MODE-6)
    RULESET_CTF,            // Threewave CTF 1.52          -- Phase 3
    RULESET_ARENA,          // Rocket Arena 2 v2.25        -- Phase 4
    RULESET_TOURNEY,        // OSP Tourney DM v2.75        -- Phase 5
    RULESET_SP,             // single player and co-op
    RULESET_COUNT
} ruleset_t;

// Composable options within a ruleset (R-MODE-4).  `ch` and `sp_dm` were
// modifiers in spec 1.0-1.1 and are struck (N7, N8).
typedef enum {
    MOD_TEAMPLAY,
    MOD_HOOK,
    MOD_RUNES,              // techs under ctf, runes under tourney
    MOD_BOTS,
    MOD_COUNT
} modifier_t;

// Content layers (R-MODE-3): orthogonal to the ruleset, valid with every one of
// them, and both may be on at once.  Latched, like the ruleset.
typedef enum {
    LAYER_XATRIX,
    LAYER_ROGUE,
    LAYER_COUNT
} content_layer_t;

// ---------------------------------------------------------------- dispatch
//
// A NULL row means "inherit the dm implementation" (R-MODE-6).  Rows are added
// when a phase brings a ruleset that replaces the behaviour, not in advance.
typedef struct {
    const char  *name;

    // Match rules: fraglimit/timelimit for dm, capturelimit for ctf, the round
    // state machine for arena, the match system for tourney, nothing for sp.
    void        (*CheckRules)(void);

    // Where the level goes next.  dm rotates or repeats; sp follows
    // target_changelevel chains; arena and tourney own their own rotation
    // (R-OSP-9, and note that is one of §7 rule 6's four exemptions).
    void        (*EndLevel)(void);

    // The scoreboard layout string, and the intermission that shows it.
    void        (*ScoreboardMessage)(edict_t *ent, edict_t *killer);
    void        (*BeginIntermission)(edict_t *targ);

    // Spawn selection, and the separate question of what happens once the
    // player has a final origin -- R-MODE-5's 26th gate.  It exists because the
    // donors prove the need: q2pro's spawn floor-clip trace sits in
    // PutClientInServer, RA2's port had to move it to move_to_arena(), and
    // tourney's to ClientBeginDeathmatch.  All three placement paths will
    // coexist here, so anything that must run after placement hangs off one
    // hook instead of being copied into three functions.
    void        (*SelectSpawnPoint)(edict_t *ent, vec3_t origin, vec3_t angles);
    void        (*ClientPlaced)(edict_t *ent);
} ruleset_ops_t;

// ---------------------------------------------------------------- accessors

void        G_InitRuleset(void);        // once, from InitGame, before anything reads a ruleset
// ...and once at the END of InitGame, after the ruleset's own init has
// registered its cvars.  R-88's decision: `runes` is derived from the switch
// the ruleset's own code reads rather than gating nothing.  See the comment on
// the implementation.
void        G_ResolveModifiers(void);
ruleset_t   G_Ruleset(void);
const char *G_RulesetName(ruleset_t r);
const ruleset_ops_t *G_Ops(void);       // never NULL after G_InitRuleset()

bool        G_ModifierEnabled(modifier_t m);
bool        G_LayerEnabled(content_layer_t l);

// ---------------------------------------------------------------- gates
//
// Call these, not G_Ops()->row directly.  R-MODE-6's "a ruleset that does not
// implement a hook inherits the dm implementation" then lives in exactly one
// place instead of being re-implemented as a null check at every call site --
// which is how Rogue's dm_game_rt ended up with ~21 hand-written null checks
// around a table that has no default row.
void     G_CheckRules(void);
void     G_EndLevel(void);
void     G_ScoreboardMessage(edict_t *ent, edict_t *killer);
void     G_BeginIntermission(edict_t *targ);
void     G_SelectSpawnPoint(edict_t *ent, vec3_t origin, vec3_t angles);
void     G_ClientPlaced(edict_t *ent);

// `sv ruleset` -- see the comment on the implementation.  The only way to
// observe R-MODE-* from outside the library.
void     G_Svcmd_Ruleset_f(void);

// ---------------------------------------------------------------- predicates
//
// One named question per concept, per R-MODE-5.  A call site asks the question
// it means; it does not test a cvar and hope the cvar still means that.

// Do monsters spawn?  R-MODE-7: yes for dm, ctf and sp; no for arena
// (R-RA-6) and tourney (R-OSP-8).  This one is load-bearing rather than
// cosmetic: baseq2 asks `if (deathmatch->value) G_FreeEdict(self)` in 24 places,
// and `deathmatch` is 1 under ctf, so the inherited test would suppress the
// monsters R-MODE-7 promises under ctf.  See doc/reconciliation.md R-6.
bool G_MonstersAllowed(void);

// Is this a campaign -- baseq2, Reckoning or Ground Zero, single or co-op?
// The question `sp` really asks, and not the same as !deathmatch: a dedicated
// server forces deathmatch 1 unless coop is set (R-VER-17).
bool G_IsCampaign(void);

// Are players on teams?  The q2pro-ng/quake2-rerelease shape, which is one
// predicate with ~20 call sites rather than a hook.  Under tourney this must
// stay `m_mode == MODE_TEAM` exactly -- 1v1 is two teams of one, the brain has
// no ally, and generalising the test would give every duel bot an imaginary
// teammate (R-BOT-29).
bool G_TeamplayEnabled(void);

// May bots exist?  Every ruleset but sp (R-MODE-7, N6: the Gladiator botlib is
// deathmatch-only), AND the `bots` modifier, which defaults to on and is the
// operator's switch for the whole layer (R-88's decision, 1.22).
bool G_BotsAllowed(void);

// May the game be saved?  sp only, and R-ENG-6 additionally forbids it whenever
// a bot exists.
bool G_SavegamesAllowed(void);

#endif // G_RULESET_H
