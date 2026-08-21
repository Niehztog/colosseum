// Colosseum ruleset dispatch -- R-MODE-1..7.
//
// Resolution runs once, from InitGame, and never calls gi.error.  R-MODE-1 and
// R-MODE-4 both say so explicitly, and the reason is in the 1999 build this
// replaces: gladq2_src/g_save.c policed its three ruleset booleans by calling
// gi.error on each conflicting pair, so a bad config did not start a server with
// a warning -- it aborted the process.  Every branch below corrects the value,
// says so once, and continues.

#include "g_local.h"

// ---------------------------------------------------------------- state
//
// Resolved once by G_InitRuleset() and read everywhere.  Not a cvar lookup at
// the point of use: R-MODE-2 says the legacy cvars are evaluated *once*, at
// InitGame, and a mid-map cvar change must not be able to move the ruleset out
// from under code that already branched on it.
static ruleset_t    g_active_ruleset = RULESET_DM;
static bool         g_modifier[MOD_COUNT];
static bool         g_layer[LAYER_COUNT];
static const ruleset_ops_t *g_active_ops;

static const char *ruleset_names[RULESET_COUNT] = {
    [RULESET_DM]      = "dm",
    [RULESET_CTF]     = "ctf",
    [RULESET_ARENA]   = "arena",
    [RULESET_TOURNEY] = "tourney",
    [RULESET_SP]      = "sp",
};

static const char *modifier_names[MOD_COUNT] = {
    [MOD_TEAMPLAY] = "teamplay",
    [MOD_HOOK]     = "hook",
    [MOD_RUNES]    = "runes/techs",
    [MOD_BOTS]     = "bots",
};

// R-MODE-7's composability matrix, as data rather than prose.  A matrix in a
// comment is a matrix nothing checks; this one is what G_ModifierEnabled()
// actually consults, so the table in SPECS.md and the behaviour cannot drift
// apart without a test noticing.
//
//                        dm     ctf    arena  tourney  sp
static const bool modifier_ok[MOD_COUNT][RULESET_COUNT] = {
    [MOD_TEAMPLAY] =    { true,  true,  true,  true,    false },
    [MOD_HOOK]     =    { true,  true,  true,  true,    false },
    [MOD_RUNES]    =    { false, true,  false, true,    false },
    [MOD_BOTS]     =    { true,  true,  true,  true,    false },
};

// ---------------------------------------------------------------- dm ops
//
// dm implements every row (R-MODE-6), and every row here is baseq2's own
// implementation -- Phase 1 cuts the seam, it does not change what happens on
// either side of it.  A later ruleset that replaces one of these fills its own
// row; one that only adjusts it uses a predicate instead.
static const ruleset_ops_t ops_dm = {
    .name              = "dm",
    .CheckRules        = CheckDMRules,
    .EndLevel          = EndDMLevel,
    .ScoreboardMessage = DeathmatchScoreboardMessage,
    .BeginIntermission = BeginIntermission,
    .SelectSpawnPoint  = SelectSpawnPoint,
    .ClientPlaced      = NULL,      // baseq2 has nothing to do after placement
};

// sp differs from dm inside these functions rather than replacing them --
// baseq2 already branches internally on deathmatch/coop -- so it inherits every
// row (R-MODE-6) and the difference stays where baseq2 put it.  Converting
// those internal branches into replaced rows now would be churn with no donor
// to justify it, which is the trap R-MODE-5 documents via dm_game_rt.
static const ruleset_ops_t ops_sp = {
    .name = "sp",
};

static const ruleset_ops_t *ruleset_ops[RULESET_COUNT] = {
    [RULESET_DM]      = &ops_dm,
    [RULESET_CTF]     = NULL,       // Phase 3
    [RULESET_ARENA]   = NULL,       // Phase 4
    [RULESET_TOURNEY] = NULL,       // Phase 5
    [RULESET_SP]      = &ops_sp,
};

// ---------------------------------------------------------------- resolution

static bool ruleset_from_name(const char *s, ruleset_t *out)
{
    if (!s || !*s)
        return false;
    for (int i = 0; i < RULESET_COUNT; i++) {
        if (!Q_stricmp(s, ruleset_names[i])) {
            *out = i;
            return true;
        }
    }
    return false;
}

// R-MODE-2.  The legacy cvars of the 1999 build and of every donor's own
// documentation keep working, so an existing config and a twenty-year-old readme
// still select what they always selected.  Evaluated once, here.
//
// The tie-break order is fixed by R-MODE-2 -- ctf, then arena, then tourney --
// so two conflicting aliases produce the same ruleset on every machine rather
// than depending on which cvar the engine happened to register first.
static bool resolve_legacy_alias(ruleset_t *out, const char **why)
{
    static const struct {
        const char *cvar;
        ruleset_t   ruleset;
    } aliases[] = {
        { "ctf",         RULESET_CTF },
        { "rocketarena", RULESET_ARENA },
    };

    bool found = false;
    const char *first = NULL;

    for (int i = 0; i < q_countof(aliases); i++) {
        if (gi.cvar(aliases[i].cvar, "0", 0)->value == 0)
            continue;
        if (!found) {
            *out = aliases[i].ruleset;
            first = aliases[i].cvar;
            found = true;
        } else {
            gi.dprintf("Colosseum: legacy '%s 1' and '%s 1' both set; "
                       "using '%s' (R-MODE-2 order: ctf, arena, tourney)\n",
                       first, aliases[i].cvar, ruleset_names[*out]);
        }
    }

    if (found)
        *why = first;
    return found;
}

// Colored Hitman is out of scope (N7), but a 1999 config may still say `ch 1`.
// R-MODE-2: accept it, ignore it, warn once.  Never refuse to start over it.
static void warn_about_ch(void)
{
    if (gi.cvar("ch", "0", 0)->value != 0) {
        gi.dprintf("Colosseum: 'ch 1' is accepted and ignored -- Colored Hitman "
                   "is out of scope (SPECS.md N7).  The 'ch' libvar is still "
                   "pushed to the botlib as 0 (R-BOT-6).\n");
    }
}

// The engine and 138 inherited baseq2 sites read `deathmatch` and `coop`, and
// the engine reads them before the game library gets a say -- src/server/init.c
// forces `deathmatch 1` on a dedicated server unless `coop` is set, on the
// stated grounds that "dedicated servers can't be single player".
//
// So resolution runs in both directions, which is what R-MODE-2 means by
// honouring the legacy cvars as aliases: the legacy value can *select* the
// ruleset, and once selected the ruleset is authoritative and the legacy cvars
// are made to agree with it.  Nothing downstream has to know which way the
// information flowed.
static void reconcile_legacy_cvars(void)
{
    bool want_dm = (g_active_ruleset != RULESET_SP);

    if (want_dm && !deathmatch->value) {
        gi.dprintf("Colosseum: ruleset '%s' requires deathmatch; setting it\n",
                   ruleset_names[g_active_ruleset]);
        gi.cvar_forceset("deathmatch", "1");
    } else if (!want_dm && deathmatch->value) {
        // Always reachable on a dedicated server, and not because the user did
        // anything wrong: `deathmatch` *defaults* to 1 there (q2pro
        // src/server/main.c registers it that way), and the engine's own
        // forcing block only ever sets it to 1, never back to 0.  So state what
        // was done rather than advising a fix -- an earlier draft told the user
        // to '+set coop 1' when they already had.
        gi.dprintf("Colosseum: ruleset 'sp' -- forcing deathmatch 0 "
                   "(a dedicated server defaults it to 1)\n");
        gi.cvar_forceset("deathmatch", "0");
    }
}

void G_InitRuleset(void)
{
    cvar_t     *rs;
    ruleset_t   r = RULESET_DM;
    const char *via = NULL;

    // R-MODE-1: latched, so it cannot change under a running map.
    rs = gi.cvar("g_ruleset", "", CVAR_LATCH | CVAR_SERVERINFO);

    if (*rs->string) {
        if (ruleset_from_name(rs->string, &r)) {
            via = "g_ruleset";
        } else {
            // R-MODE-1: invalid values fall back to dm with a warning; they
            // never abort startup.
            gi.dprintf("Colosseum: unknown g_ruleset '%s'; using 'dm'. "
                       "Valid: dm ctf arena tourney sp\n", rs->string);
            r = RULESET_DM;
            via = "fallback";
        }
    } else if (resolve_legacy_alias(&r, &via)) {
        // via is the legacy cvar name
    } else if (gi.cvar("coop", "0", 0)->value) {
        // No explicit selection and co-op is on: that is the campaign, and
        // guessing dm here would drop the player into a deathmatch with the
        // monsters freed.
        r = RULESET_SP;
        via = "coop";
    } else {
        r = RULESET_DM;
        via = "default";
    }

    g_active_ruleset = r;
    g_active_ops = ruleset_ops[r] ? ruleset_ops[r] : ruleset_ops[RULESET_DM];

    warn_about_ch();
    reconcile_legacy_cvars();

    // R-MODE-3: independent latched content layers, valid with every ruleset,
    // and both may be on at once.
    g_layer[LAYER_XATRIX] = gi.cvar("xatrix", "0", CVAR_LATCH)->value != 0;
    g_layer[LAYER_ROGUE]  = gi.cvar("rogue", "0", CVAR_LATCH)->value != 0;

    // R-MODE-4: a modifier declares which rulesets accept it, and an
    // unsupported combination is refused with one message naming the ruleset,
    // the modifier and what was done instead.  It never calls gi.error.
    g_modifier[MOD_TEAMPLAY] = gi.cvar("teamplay", "0", CVAR_LATCH)->value != 0;
    g_modifier[MOD_HOOK]     = gi.cvar("hook", "0", CVAR_LATCH)->value != 0;
    g_modifier[MOD_RUNES]    = gi.cvar("runes", "0", CVAR_LATCH)->value != 0;
    // Bots are requested by minimumplayers / bots_minplayers, not by a boolean;
    // Phase 6 sets this from the bot layer.  Until then no bot can exist, so
    // claiming otherwise would make G_BotsAllowed() lie.
    g_modifier[MOD_BOTS]     = false;

    for (int m = 0; m < MOD_COUNT; m++) {
        if (g_modifier[m] && !modifier_ok[m][g_active_ruleset]) {
            gi.dprintf("Colosseum: ruleset '%s' does not accept modifier '%s'; "
                       "disabling it (R-MODE-4)\n",
                       ruleset_names[g_active_ruleset], modifier_names[m]);
            g_modifier[m] = false;
        }
    }

    gi.dprintf("Colosseum: ruleset '%s' (via %s)%s%s%s\n",
               ruleset_names[g_active_ruleset], via ? via : "default",
               g_layer[LAYER_XATRIX] ? ", xatrix" : "",
               g_layer[LAYER_ROGUE] ? ", rogue" : "",
               g_modifier[MOD_TEAMPLAY] ? ", teamplay" : "");
}

// ---------------------------------------------------------------- accessors

ruleset_t G_Ruleset(void)
{
    return g_active_ruleset;
}

const char *G_RulesetName(ruleset_t r)
{
    return (r >= 0 && r < RULESET_COUNT) ? ruleset_names[r] : "?";
}

const ruleset_ops_t *G_Ops(void)
{
    // G_InitRuleset() runs before anything can reach a gate, but a NULL here
    // would be a crash at the first frame rather than a diagnosable mistake.
    return g_active_ops ? g_active_ops : &ops_dm;
}

bool G_ModifierEnabled(modifier_t m)
{
    return (m >= 0 && m < MOD_COUNT) ? g_modifier[m] : false;
}

bool G_LayerEnabled(content_layer_t l)
{
    return (l >= 0 && l < LAYER_COUNT) ? g_layer[l] : false;
}

// ---------------------------------------------------------------- diagnostic
//
// `sv ruleset` -- what resolution decided, what each predicate answers, and what
// the world actually contains.  Wired into ServerCommand (g_svcmds.c).
//
// This exists because R-MODE-* is otherwise unobservable from outside. The
// entity census is the part that matters: `entities inhibited` in SpawnEntities
// counts spawnflag filtering only, so it moves with `deathmatch` and `coop` and
// says nothing about whether G_MonstersAllowed() was consulted -- monsters that
// the gate rejects are spawned and then freed, which no engine message reports.
// R-VER-2's boot matrix is 20 combinations; each one needs an answer to "did
// this do what the matrix says", and this is that answer.
void G_Svcmd_Ruleset_f(void)
{
    int monsters = 0, corpses = 0, gibs = 0, clients = 0, inuse = 0;

    for (int i = 0; i < globals.num_edicts; i++) {
        edict_t *e = &g_edicts[i];
        if (!e->inuse)
            continue;
        inuse++;
        if (e->client) {
            clients++;
        } else if (e->svflags & SVF_MONSTER) {
            // SVF_MONSTER is not the same question as "is a live monster", and
            // getting this wrong twice is what the comment is for.  Two kinds of
            // prop carry the flag so they collide like a monster body:
            //   misc_deadsoldier      SVF_MONSTER | SVF_DEADMONSTER -- a corpse
            //   misc_gib_arm/_leg/_head  SVF_MONSTER + EF_GIB       -- a gib
            // Neither is gated by G_MonstersAllowed(), correctly: a decorative
            // gib is map dressing and spawns in every ruleset.  Counting them
            // made `dm` report a live monster on base1 -- a `misc_gib_head` --
            // and made the gate look leaky when it was doing its job.
            if (e->svflags & SVF_DEADMONSTER)
                corpses++;
            else if (e->s.effects & EF_GIB)
                gibs++;
            else
                monsters++;
        }
    }

    gi.cprintf(NULL, PRINT_HIGH,
               "ruleset      %s\n"
               "layers       xatrix=%d rogue=%d\n"
               "modifiers    teamplay=%d hook=%d runes=%d bots=%d\n"
               "predicates   monsters=%d campaign=%d teamplay=%d bots=%d saves=%d\n"
               "legacy       deathmatch=%d coop=%d\n"
               "world        %d edicts in use, %d clients, "
               "%d live monsters, %d corpses, %d gibs\n",
               G_RulesetName(G_Ruleset()),
               G_LayerEnabled(LAYER_XATRIX), G_LayerEnabled(LAYER_ROGUE),
               G_ModifierEnabled(MOD_TEAMPLAY), G_ModifierEnabled(MOD_HOOK),
               G_ModifierEnabled(MOD_RUNES), G_ModifierEnabled(MOD_BOTS),
               G_MonstersAllowed(), G_IsCampaign(), G_TeamplayEnabled(),
               G_BotsAllowed(), G_SavegamesAllowed(),
               (int)deathmatch->value, (int)coop->value,
               inuse, clients, monsters, corpses, gibs);

    // A live monster in a ruleset that forbids them is a contradiction, and the
    // diagnostic names it rather than leaving a reader to spot an unexpected
    // number.  This is how the leak below was found: dm reported "1 live
    // monsters" on base1 and nothing said which entity it was.
    if (!G_MonstersAllowed() && monsters > 0) {
        gi.cprintf(NULL, PRINT_HIGH,
                   "  !! %d live monster(s) in ruleset '%s', which forbids "
                   "them:\n", monsters, G_RulesetName(G_Ruleset()));
        for (int i = 0; i < globals.num_edicts; i++) {
            edict_t *e = &g_edicts[i];
            if (!e->inuse || e->client)
                continue;
            if (!(e->svflags & SVF_MONSTER) || (e->svflags & SVF_DEADMONSTER))
                continue;
            if (e->s.effects & EF_GIB)
                continue;
            gi.cprintf(NULL, PRINT_HIGH, "     %s at %s\n",
                       e->classname ? e->classname : "(no classname)",
                       vtos(e->s.origin));
        }
    }
}

// ---------------------------------------------------------------- gates
//
// One place implements R-MODE-6.  `dm` fills every row, so falling back to
// ops_dm is always defined; a NULL row on dm itself means "baseq2 genuinely does
// nothing here", which is only ClientPlaced today.

#define GATE(row)   (G_Ops()->row ? G_Ops()->row : ops_dm.row)

void G_CheckRules(void)
{
    if (GATE(CheckRules))
        GATE(CheckRules)();
}

void G_EndLevel(void)
{
    if (GATE(EndLevel))
        GATE(EndLevel)();
}

void G_ScoreboardMessage(edict_t *ent, edict_t *killer)
{
    if (GATE(ScoreboardMessage))
        GATE(ScoreboardMessage)(ent, killer);
}

void G_BeginIntermission(edict_t *targ)
{
    if (GATE(BeginIntermission))
        GATE(BeginIntermission)(targ);
}

void G_SelectSpawnPoint(edict_t *ent, vec3_t origin, vec3_t angles)
{
    if (GATE(SelectSpawnPoint))
        GATE(SelectSpawnPoint)(ent, origin, angles);
}

void G_ClientPlaced(edict_t *ent)
{
    if (GATE(ClientPlaced))
        GATE(ClientPlaced)(ent);
}

// ---------------------------------------------------------------- predicates

bool G_MonstersAllowed(void)
{
    switch (g_active_ruleset) {
    case RULESET_ARENA:                 // R-RA-6
    case RULESET_TOURNEY:               // R-OSP-8
        return false;
    case RULESET_CTF:
        // R-MODE-7 promises monsters under ctf and R-VER-2 adds the combination
        // to the boot matrix explicitly, because q2pro/src/ctf deleted the
        // monster set so Threewave's edits to g_ai.c, g_monster.c, g_combat.c
        // and g_turret.c have never been compiled against live monster code.
        // Q14 closed it as "unproven rather than broken".  Answering true here
        // is what makes the boot matrix able to find out.
        return true;
    case RULESET_DM:
        // baseq2's own rule, unchanged: no monsters in deathmatch.
        return false;
    case RULESET_SP:
        return true;
    default:
        return false;
    }
}

bool G_IsCampaign(void)
{
    return g_active_ruleset == RULESET_SP;
}

bool G_TeamplayEnabled(void)
{
    if (g_active_ruleset == RULESET_CTF)
        return true;                    // implied, per R-MODE-7
    return G_ModifierEnabled(MOD_TEAMPLAY);
}

bool G_BotsAllowed(void)
{
    return g_active_ruleset != RULESET_SP;      // N6, R-MODE-7
}

bool G_SavegamesAllowed(void)
{
    // R-ENG-6.  The "and false whenever a bot exists" half arrives with the bot
    // layer in Phase 6; there are no bots yet, so stating it now would be a
    // check that cannot fail.
    return g_active_ruleset == RULESET_SP;
}
