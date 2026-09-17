// Colosseum ruleset dispatch
//
// Resolution runs once, from InitGame, and never calls gi.error.  The reason
// is in the 1999 build this replaces: Gladiator's g_save.c policed its three
// ruleset booleans by calling gi.error on each conflicting pair, so a bad
// config did not start a server with a warning -- it aborted the process.
// Every branch below corrects the value, says so once, and continues.

#include "g_local.h"
// The runes modifier is derived from these two switches:
// Threewave's DF_CTF_NO_TECH and tourney's rune_stat.  See G_ResolveModifiers.
#include "ctf/g_ctf.h"
#include "tourney/osp_hooks.h"
// No savegame while a bot exists.
#include "bot/bl_main.h"
// The bot-fill target, which the botfill rows below print.
#include "bot/bl_spawn.h"
// The bot-placement census reads RA2's FIGHT_* and tourney's ENTERED_*.
#include "arena/arena.h"

// ---------------------------------------------------------------- state
//
// Resolved once by G_InitRuleset() and read everywhere.  Not a cvar lookup at
// the point of use: the legacy cvars are evaluated once, at InitGame, and a
// mid-map cvar change must not be able to move the ruleset out from under code
// that already branched on it.
static ruleset_t    g_active_ruleset = RULESET_DM;
static bool         g_modifier[MOD_COUNT];
static bool         g_layer[LAYER_COUNT];
// OSP configuration changes execute before the next map's SpawnEntities().
static bool         g_osp_hook_request_queued;

// See reconcile above: one pair, two donors.
cvar_t             *g_statsfile;
cvar_t             *g_statsname;
static const ruleset_ops_t *g_active_ops;

static const char *ruleset_names[RULESET_COUNT] = {
    [RULESET_DM]      = "dm",
    [RULESET_DMPRO]   = "dmpro",
    [RULESET_TDM]     = "tdm",
    [RULESET_DUEL]    = "duel",
    [RULESET_CTF]     = "ctf",
    [RULESET_ARENA]   = "arena",
    [RULESET_SP]      = "sp",
};

// The one place the seven are spelled for a human.  Used by the unknown-value
// warning, so the message cannot list a set the parser does not accept.
#define RULESET_NAME_LIST   "dm dmpro tdm duel ctf arena sp"

static const char *modifier_names[MOD_COUNT] = {
    [MOD_TEAMPLAY] = "teamplay",
    [MOD_HOOK]     = "hook",
    [MOD_RUNES]    = "runes/techs",
    [MOD_BOTS]     = "bots",
};

// The composability matrix, as data rather than prose.  A matrix in a
// comment is a matrix nothing checks; this one is what G_ModifierEnabled()
// actually consults, so the documented table and the behaviour cannot drift
// apart without a test noticing.
//
// Every cell is designated, and that is not a style choice.  These rows were
// positional five-element brace lists until the flattening, so adding three
// rulesets would have padded the new columns with `false` -- silently, because
// the build sets -Wno-missing-field-initializers, and invisibly, because no
// audit in the tree reads this table.  `tdm` would have come up with teamplay
// refused, which is the one cell in the matrix that ruleset cannot do without.
// A designated cell that is missing reads false too, but it reads false where a
// reader is looking for it.
static const bool modifier_ok[MOD_COUNT][RULESET_COUNT] = {
    // Team play is a RULESET now (`tdm`, `duel`), so the modifier that also
    // reached it is refused across the OSP four -- the whole argument of the
    // flattening applied to itself.  arena keeps it: its teams are the
    // arena's, and the modifier is how an operator asks for them.
    [MOD_TEAMPLAY] = {
        [RULESET_DM] = false, [RULESET_DMPRO] = false,
        [RULESET_TDM] = false, [RULESET_DUEL] = false,
        [RULESET_CTF] = true, [RULESET_ARENA] = true, [RULESET_SP] = false,
    },
    [MOD_HOOK] = {
        [RULESET_DM] = true, [RULESET_DMPRO] = true,
        [RULESET_TDM] = true, [RULESET_DUEL] = true,
        [RULESET_CTF] = true, [RULESET_ARENA] = true, [RULESET_SP] = false,
    },
    // Runes under the OSP four, techs under ctf.  `dm` gains them by becoming
    // OSP's RegularDM; baseq2's deathmatch had neither.
    [MOD_RUNES] = {
        [RULESET_DM] = true, [RULESET_DMPRO] = true,
        [RULESET_TDM] = true, [RULESET_DUEL] = true,
        [RULESET_CTF] = true, [RULESET_ARENA] = false, [RULESET_SP] = false,
    },
    [MOD_BOTS] = {
        [RULESET_DM] = true, [RULESET_DMPRO] = true,
        [RULESET_TDM] = true, [RULESET_DUEL] = true,
        [RULESET_CTF] = true, [RULESET_ARENA] = true, [RULESET_SP] = false,
    },
};

// ---------------------------------------------------------------- base ops
//
// The BASE fills every row, and every row here is baseq2's own
// implementation -- cutting the seam did not change what happens on
// either side of it.  A ruleset that replaces one of these fills its own row;
// one that only adjusts it uses a predicate instead.
//
// Nothing selects this table.  It was `ops_dm` and was two things wearing one
// name: the `dm` ruleset's row set, and the table every NULL row falls back to.
// The flattening separated them -- `dm` is OSP's RegularDM now and has its own
// rows -- and this is only the second.
//
// All five functions stay, and not merely for the fallback.  ctf, arena and the
// OSP four call them by name from their own C: ctf_CheckRules and RA_CheckRules
// both call CheckDMRules, OSP_EndLevel and RA_EndLevel both fall back to
// EndDMLevel, and PutClientInServer calls G_SelectSpawnPoint() for every
// ruleset before arena and the OSP four re-place the client.  Deleting one is
// not a table edit.
//
// DeathmatchScoreboardMessage IS NOT ONE OF THEM, and tourney is the reason it
// looks as though it should be.  Three of tourney's own functions draw a page
// they have just selected -- the MOTD window in OSP_setStats, `highscores` and
// `showinfo` -- and the donor draws it by calling the name it gives its OWN
// five-page dispatcher.  Here that dispatcher is the ScoreboardMessage row, so
// all three reach it through G_ScoreboardMessage() and this row is gate-only
// (R-OSP-14).
static const ruleset_ops_t ops_base = {
    .name              = "base",
    .CheckRules        = CheckDMRules,
    .EndLevel          = EndDMLevel,
    .ScoreboardMessage = DeathmatchScoreboardMessage,
    .BeginIntermission = BeginIntermission,
    .SelectSpawnPoint  = SelectSpawnPoint,
    .ClientPlaced      = NULL,      // baseq2 has nothing to do after placement
};

// sp differs from dm inside these functions rather than replacing them --
// baseq2 already branches internally on deathmatch/coop -- so it inherits every
// row and the difference stays where baseq2 put it.  Converting
// those internal branches into replaced rows now would be churn with no donor
// to justify it, which is the trap dm_game_rt documents.
static const ruleset_ops_t ops_sp = {
    .name = "sp",
};

// A missing row here is not a compile error.  It is NULL, and G_InitRuleset's
// `?:` below then hands that ruleset the BASE -- i.e. baseq2 deathmatch, quietly,
// under whatever name the operator asked for.  Every ruleset gets a row.
static const ruleset_ops_t *ruleset_ops[RULESET_COUNT] = {
    // The OSP four are one code path selected four ways; what differs
    // between them lives inside tourney's own functions, not in the row set.
    [RULESET_DM]      = &ops_tourney, // src/tourney/osp_main.c
    [RULESET_DMPRO]   = &ops_tourney,
    [RULESET_TDM]     = &ops_tourney,
    [RULESET_DUEL]    = &ops_tourney,
    [RULESET_CTF]     = &ops_ctf,   // src/ctf/g_ctf.c
    [RULESET_ARENA]   = &ops_arena, // src/arena/arena.c
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

// The legacy cvars of the 1999 build and of every donor's own
// documentation keep working, so an existing config and a twenty-year-old readme
// still select what they always selected.  Evaluated once, here.
//
// The tie-break order is fixed -- ctf, then arena -- so two conflicting
// aliases produce the same ruleset on every machine rather than depending on
// which cvar the engine happened to register first.
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
                       "using '%s' (order: ctf, arena)\n",
                       first, aliases[i].cvar, ruleset_names[*out]);
        }
    }

    if (found)
        *why = first;
    return found;
}

// The engine and 138 inherited baseq2 sites read `deathmatch` and `coop`, and
// the engine reads them before the game library gets a say -- src/server/init.c
// forces `deathmatch 1` on a dedicated server unless `coop` is set, on the
// stated grounds that "dedicated servers can't be single player".
//
// So resolution runs in both directions, which is what is meant by
// honouring the legacy cvars as aliases: the legacy value can *select* the
// ruleset, and once selected the ruleset is authoritative and the legacy cvars
// are made to agree with it.  Nothing downstream has to know which way the
// information flowed.
static void reconcile_legacy_cvars(void)
{
    bool want_dm = (g_active_ruleset != RULESET_SP);

    // `ctf` is serverinfo that a Threewave-aware client reads to decide what to
    // draw, so it must agree with the resolved ruleset in both directions --
    // `g_ruleset ctf` without `ctf 1` has to advertise CTF, and `ctf 1` with
    // `g_ruleset dm` must not.  Threewave registers this cvar itself with
    // default 1; it is registered here instead, because resolution owns it
    // and a second registration would re-assert a default that
    // resolution has already decided.
    gi.cvar_forceset("ctf", g_active_ruleset == RULESET_CTF ? "1" : "0");
    gi.cvar("ctf", "0", CVAR_SERVERINFO);

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

    // Latched, so it cannot change under a running map.
    rs = gi.cvar("g_ruleset", "", CVAR_LATCH | CVAR_SERVERINFO);

    if (*rs->string) {
        if (ruleset_from_name(rs->string, &r)) {
            via = "g_ruleset";
        } else {
            // Invalid values fall back to dm with a warning; they
            // never abort startup.  `tourney` is one of those values and gets
            // no sentence of its own -- the message names the seven that are
            // valid, which is the answer to "what should I have written"
            // whatever the operator wrote instead.
            gi.dprintf("Colosseum: unknown g_ruleset '%s'; using 'dm'. "
                       "Valid: " RULESET_NAME_LIST "\n", rs->string);
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
    g_osp_hook_request_queued = false;

    reconcile_legacy_cvars();

    // Independent latched content layers, valid with every ruleset,
    // and both may be on at once.
    g_layer[LAYER_XATRIX] = gi.cvar("xatrix", "0", CVAR_LATCH)->value != 0;
    g_layer[LAYER_ROGUE]  = gi.cvar("rogue", "0", CVAR_LATCH)->value != 0;

    // A modifier declares which rulesets accept it, and an unsupported
    // combination is refused with one message naming the ruleset, the modifier
    // and what was done instead.  It never calls gi.error.
    g_modifier[MOD_TEAMPLAY] = gi.cvar("teamplay", "0", CVAR_LATCH)->value != 0;
    g_modifier[MOD_HOOK]     = gi.cvar("hook", "0", CVAR_LATCH)->value != 0;
    // The REQUEST, so that the refusal below still fires under dm and arena.
    // G_ResolveModifiers() overwrites it at the end of InitGame with what the
    // ruleset's own switch actually says -- see there.
    g_modifier[MOD_RUNES]    = gi.cvar("runes", "0", CVAR_LATCH)->value != 0;
    // The other half.  `bots` is the operator's switch for the whole layer
    // and defaults to 1 where the ruleset accepts it, so a server that says
    // nothing behaves exactly as it did before the modifier existed.  It is not
    // a second answer to G_BotsAllowed() -- it is what G_BotsAllowed() reads,
    // which is why there is still one question asked by name.  The reason it
    // exists at all is staged exposure: a public server wants to be
    // able to turn the bot layer off without turning the ruleset off.
    g_modifier[MOD_BOTS]     = gi.cvar("bots", "1", CVAR_LATCH)->value != 0;

    // `statsfile` and `statsname` are registered ONCE, here, with a default
    // chosen by the ruleset that will use them.  Both donors ship a local-file
    // statistics log and both named their cvars the same, with different
    // defaults -- which is a cvar whose value depends on which translation
    // unit happened to register it first.  Resolution owns the pair because
    // resolution is what decides which log is going to run.
    g_statsfile = gi.cvar("statsfile", "1", 0);
    g_statsname = gi.cvar("statsname",
                          g_active_ruleset == RULESET_ARENA ? "ra2stats.jsonl"
                          : "osptourney.jsonl", 0);

    for (int m = 0; m < MOD_COUNT; m++) {
        if (g_modifier[m] && !modifier_ok[m][g_active_ruleset]) {
            gi.dprintf("Colosseum: ruleset '%s' does not accept modifier '%s'; "
                       "disabling it\n",
                       ruleset_names[g_active_ruleset], modifier_names[m]);
            g_modifier[m] = false;

            // `teamplay` is the one refusal an operator is likely to have meant
            // something by, because it USED to work here: it reached team play
            // under the old `dm` and the old `tourney` alike.  Saying only "not
            // accepted" leaves them to guess where it went, so the line after
            // says -- and says something different depending on whether they
            // are already in the ruleset they were asking for.
            if (m == MOD_TEAMPLAY && G_IsOspRuleset()) {
                if (OSP_IsTeams())
                    gi.dprintf("Colosseum: ...'%s' IS team play; the modifier is "
                               "what it replaced\n",
                               ruleset_names[g_active_ruleset]);
                else
                    gi.dprintf("Colosseum: ...team play is a ruleset here -- "
                               "'g_ruleset tdm' or 'duel'\n");
            }
        }
    }

    // The slot map is per ruleset and must be resolved before any
    // stat is written or any statusbar composed, which is why it hangs off
    // resolution rather than off SP_worldspawn.
    G_InitStats();

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
    return g_active_ops ? g_active_ops : &ops_base;
}

bool G_ModifierEnabled(modifier_t m)
{
    if (m < 0 || m >= MOD_COUNT)
        return false;

    // These OSP switches can change through a vote after InitGame, so the
    // modifier reports the live ruleset authority rather than its old request.
    if (G_IsOspRuleset()) {
        if (m == MOD_HOOK)
            return hook_enable && (int)hook_enable->value != 0;
        if (m == MOD_RUNES)
            return rune_stat != 0;
    }

    return g_modifier[m];
}

bool G_LayerEnabled(content_layer_t l)
{
    return (l >= 0 && l < LAYER_COUNT) ? g_layer[l] : false;
}

bool G_UsesRogueGameRules(void)
{
    return gamerules && gamerules->value &&
           (g_active_ruleset == RULESET_CTF ||
            g_active_ruleset == RULESET_ARENA);
}

void G_ApplyOspHookRequest(void)
{
    cvar_t *request;

    if (G_IsOspRuleset() && hook_enable) {
        request = gi.cvar("hook", "0", CVAR_LATCH);
        if (request->value && !(int)hook_enable->value) {
            gi.cvar_set("hook_enable", "1");
            gi.dprintf("Colosseum: 'hook 1' enables hook_enable under OSP\n");
        }

        g_modifier[MOD_HOOK] = (int)hook_enable->value != 0;
    }
}

void G_QueueOspHookRequest(void)
{
    if (G_IsOspRuleset())
        g_osp_hook_request_queued = true;
}

bool G_ApplyQueuedOspHookRequest(void)
{
    if (!g_osp_hook_request_queued)
        return false;

    g_osp_hook_request_queued = false;
    G_ApplyOspHookRequest();
    return true;
}

// ---------------------------------------------------------------- diagnostic
//
// `sv ruleset` -- what resolution decided, what each predicate answers, and what
// the world actually contains.  Wired into ServerCommand (g_svcmds.c).
//
// This exists because the resolved ruleset is otherwise unobservable from
// outside.  The entity census is the part that matters: `entities inhibited`
// in SpawnEntities counts spawnflag filtering only, so it moves with
// `deathmatch` and `coop` and says nothing about whether G_MonstersAllowed()
// was consulted -- monsters that the gate rejects are spawned and then freed,
// which no engine message reports.  The boot matrix is 20 combinations; each
// one needs an answer to "did this do what the matrix says", and this is that
// answer.
void G_Svcmd_Ruleset_f(void)
{
    int monsters = 0, corpses = 0, gibs = 0, clients = 0, inuse = 0;
    int flav_x = 0, flav_r = 0;
    int evade_rogue = 0, evade_bq2 = 0;

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
            if (e->content_flavour & CONTENT_XATRIX)
                flav_x++;
            if (e->content_flavour & CONTENT_ROGUE)
                flav_r++;
            // The content gate, observed rather than asserted: which evasion
            // set did this monster actually get installed at spawn?
            if (e->monsterinfo.dodge == M_MonsterDodge)
                evade_rogue++;
            else if (e->monsterinfo.dodge)
                evade_bq2++;
        }
    }

    gi.cprintf(NULL, PRINT_HIGH,
               "ruleset      %s\n"
               "layers       xatrix=%d rogue=%d\n"
               "modifiers    teamplay=%d hook=%d runes=%d bots=%d\n"
               "predicates   monsters=%d campaign=%d teamplay=%d bots=%d saves=%d\n"
               "legacy       deathmatch=%d coop=%d\n"
               "flavour      %d monster(s) latched xatrix, %d rogue\n"
               "evasion      %d monster(s) on Ground Zero's dodge, %d on "
               "baseq2's\n"
               "world        frame %d, %d edicts in use, %d clients, "
               "%d live monsters, %d corpses, %d gibs\n",
               G_RulesetName(G_Ruleset()),
               G_LayerEnabled(LAYER_XATRIX), G_LayerEnabled(LAYER_ROGUE),
               G_ModifierEnabled(MOD_TEAMPLAY), G_ModifierEnabled(MOD_HOOK),
               G_ModifierEnabled(MOD_RUNES), G_ModifierEnabled(MOD_BOTS),
               G_MonstersAllowed(), G_IsCampaign(), G_TeamplayEnabled(),
               G_BotsAllowed(), G_SavegamesAllowed(),
               (int)deathmatch->value, (int)coop->value,
               flav_x, flav_r, evade_rogue, evade_bq2,
               level.framenum, inuse, clients, monsters, corpses, gibs);

    // The CTF content, counted rather than assumed.  Nine CTF maps booting
    // clean says nothing about whether the flags and techs are in the world:
    // both are ordinary itemlist entries whose absence looks exactly like a map
    // that has none.  Only printed under ctf, where the question means something.
    if (G_Ruleset() == RULESET_CTF) {
        static const char *const tech_names[] = {
            "item_tech1", "item_tech2", "item_tech3", "item_tech4"
        };
        int spawn1 = 0, spawn2 = 0, techs = 0, banners = 0;
        edict_t *f1 = G_Find(NULL, FOFS(classname), "item_flag_team1");
        edict_t *f2 = G_Find(NULL, FOFS(classname), "item_flag_team2");

        for (int i = 0; i < globals.num_edicts; i++) {
            edict_t *e = &g_edicts[i];
            if (!e->inuse || !e->classname)
                continue;
            if (!strcmp(e->classname, "info_player_team1"))
                spawn1++;
            else if (!strcmp(e->classname, "info_player_team2"))
                spawn2++;
            else if (!strncmp(e->classname, "misc_ctf_", 9))
                banners++;
            for (int t = 0; t < q_countof(tech_names); t++)
                if (!strcmp(e->classname, tech_names[t]))
                    techs++;
        }

        // A flag is "at base" when it is solid; SOLID_NOT means carried or
        // dropped, which is what SetCTFStats reads to pick its icon.
        gi.cprintf(NULL, PRINT_HIGH,
                   "ctf          flags %s/%s, %d tech(es) in world, "
                   "%d+%d team spawn(s), %d banner(s)\n",
                   f1 ? (f1->solid == SOLID_NOT ? "away" : "base") : "ABSENT",
                   f2 ? (f2->solid == SOLID_NOT ? "away" : "base") : "ABSENT",
                   techs, spawn1, spawn2, banners);
    }

    // The same argument applied to bots: a bot's placement has to be
    // observable from outside the library, or "bots play in ctf" is a claim
    // about source rather than a fact about a server.  `sv clientdump` says
    // which slots hold bots and which library each uses; it cannot say which
    // team, which arena or whether the match counts them, and those are exactly
    // what the three per-ruleset bot-placement paths are about.
    //
    // Two lines: the census, then the ruleset's own placement.  Both are
    // printed under every ruleset that accepts bots, including when there are
    // none -- a row that prints nothing and a row that prints zero are not the
    // same evidence.
    if (G_BotsAllowed()) {
        char slots[128];
        int  nbots = 0, len = 0;

        slots[0] = 0;
        for (int i = 0; i < game.maxclients; i++) {
            edict_t *e = &g_edicts[i + 1];
            if (!e->inuse || !e->client || !(e->flags & FL_BOT))
                continue;
            nbots++;
            if (len < (int)sizeof(slots) - 8)
                len += Q_snprintf(slots + len, sizeof(slots) - len, "%s%d",
                                  len ? "," : "", i);
        }

        gi.cprintf(NULL, PRINT_HIGH,
                   "bots         %d bot(s) of %d client(s) in %d slot(s), "
                   "at %s\n", nbots, clients, game.maxclients,
                   nbots ? slots : "-");

        // FL_BOT and FL_BOTCLIENT are two different bits and Phase
        // 6 sets both on every bot.  A site that tests one and means the other
        // compiles, so the two counts are printed separately rather than
        // assumed equal: if they ever diverge, this is where it shows.
        int nbotclient = 0;
        for (int i = 0; i < game.maxclients; i++) {
            edict_t *e = &g_edicts[i + 1];
            if (e->inuse && e->client && (e->flags & FL_BOTCLIENT))
                nbotclient++;
        }

        switch (G_Ruleset()) {
        case RULESET_CTF: {
            int red = 0, blue = 0, noteam = 0, teamskin = 0;

            for (int i = 0; i < game.maxclients; i++) {
                edict_t *e = &g_edicts[i + 1];
                if (!e->inuse || !e->client || !(e->flags & FL_BOT))
                    continue;
                if (e->client->resp.ctf_team == CTF_TEAM1)
                    red++;
                else if (e->client->resp.ctf_team == CTF_TEAM2)
                    blue++;
                else
                    noteam++;
                // What the brain is told, which is not what the wire carries.
                // `clientsettings[].skin` is the only currency the Gladiator
                // brain has for CTF teams -- BotCTFTeam() is `strstr(skin,
                // "ctf_r") ?  RED : BLUE` and BotSameTeam() compares the half
                // after the '/' -- and bl_main.c fills it from this string,
                // the game's own copy of the userinfo.  CTFAssignSkin writes
                // the playerskins configstring either way, so a client reading
                // the wire cannot tell the two apart: a bot wearing `ctf_r` on
                // every screen while its brain still reads `male/viper` is a
                // bot that thinks the whole server is on the other side.
                // Nothing else in this tree can report it, which is why it is
                // a field here.
                if (strstr(Info_ValueForKey(e->client->pers.userinfo, "skin"),
                           "ctf_"))
                    teamskin++;
            }
            gi.cprintf(NULL, PRINT_HIGH,
                       "botplace     ctf red=%d blue=%d noteam=%d teamskin=%d, "
                       "FL_BOTCLIENT=%d\n",
                       red, blue, noteam, teamskin, nbotclient);
            break;
        }
        case RULESET_ARENA: {
            int placed = 0, teamed = 0, fighting = 0, arena1 = 0;

            for (int i = 0; i < game.maxclients; i++) {
                edict_t *e = &g_edicts[i + 1];
                if (!e->inuse || !e->client || !(e->flags & FL_BOT))
                    continue;
                if (e->client->resp.context > 0)
                    placed++;
                if (e->client->resp.context == 1)
                    arena1++;
                if (e->client->resp.teamnum >= 0)
                    teamed++;
                if (e->client->resp.fightstate == FIGHT_ALIVE)
                    fighting++;
            }
            gi.cprintf(NULL, PRINT_HIGH,
                       "botplace     arena in-arena=%d (arena1=%d) on-team=%d "
                       "fighting=%d, FL_BOTCLIENT=%d\n",
                       placed, arena1, teamed, fighting, nbotclient);
            break;
        }
        case RULESET_DM:
        case RULESET_DMPRO:
        case RULESET_TDM:
        case RULESET_DUEL: {
            int entered = 0, ready = 0, t0 = 0, t1 = 0;

            for (int i = 0; i < game.maxclients; i++) {
                edict_t *e = &g_edicts[i + 1];
                if (!e->inuse || !e->client || !(e->flags & FL_BOT))
                    continue;
                if (e->client->resp.osp_entered == ENTERED_ENTERED)
                    entered++;
                if (e->client->resp.osp_r20c)
                    ready++;
                if (e->client->resp.team == 0)
                    t0++;
                else if (e->client->resp.team == 1)
                    t1++;
            }
            gi.cprintf(NULL, PRINT_HIGH,
                       "botplace     %s entered=%d ready=%d "
                       "team0=%d team1=%d, FL_BOTCLIENT=%d\n",
                       G_RulesetName(G_Ruleset()), entered, ready, t0, t1,
                       nbotclient);
            break;
        }
        default:
            // Unreachable: sp is the only ruleset left and G_BotsAllowed() is
            // false there, which is what this whole block is inside.  Kept so
            // that a ruleset added later is reported rather than silent.
            gi.cprintf(NULL, PRINT_HIGH,
                       "botplace     %s has no per-ruleset bot placement, "
                       "FL_BOTCLIENT=%d\n", G_RulesetName(G_Ruleset()),
                       nbotclient);
            break;
        }

        // One botfill line for every ruleset, which is the point of there
        // being one cvar.  It was four shapes in four arms while the switch
        // was three cvars, and four shapes is how a play test ends up with
        // three regexes and a gap.
        //
        // `want` is what survives the ceilings and the source text is where the
        // number came from -- printing only the first hides a clamp, which is
        // the reason this line exists at all.
        // What a passed `vote rembot` took off, on whichever of the two rows
        // is printed, because neither can say it for itself: `want=` already
        // has the cut subtracted and does not say how it got there, and the
        // flat count printed beside `off` is the cvar's value and not the
        // number in force (R-OSP-16).  Appended rather than spliced in, so
        // that a reader -- and a play test's regex -- finds the row it knew.
        char cut[32];

        cut[0] = 0;
        if (BotTourneyVotedOut())
            Q_snprintf(cut, sizeof(cut), ", %d voted out",
                       BotTourneyVotedOut());

        if (BotFillEnabled()) {
            char src[96];

            BotFillDescribe(src, sizeof(src));
            gi.cprintf(NULL, PRINT_HIGH,
                       "botfill      %s want=%d from %s%s\n",
                       G_RulesetName(G_Ruleset()), BotFillTarget(), src, cut);
        } else {
            gi.cprintf(NULL, PRINT_HIGH,
                       "botfill      off -- %s %d is the target%s\n",
                       BotMinPlayersCvar(), (int)BotMinPlayers()->value, cut);
        }
    }

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
// One place implements row inheritance.  The base fills every row, so falling back to
// ops_base is always defined; a NULL row on the base itself means "baseq2
// genuinely does nothing here", which is only ClientPlaced today.

#define GATE(row)   (G_Ops()->row ? G_Ops()->row : ops_base.row)

void G_CheckRules(void)
{
    if (GATE(CheckRules))
        GATE(CheckRules)();
}

/*
=================
G_EndLevel

Ending the level is `G_EndLevel()`.  A by-name `EndDMLevel()` is a fallback,
not a way out.

The rule, because this row has been got wrong twice in two rulesets and once on
the row next door (the `G_BeginIntermission` banner in g_main.c): a call to one
of `ops_base`'s five
functions by name is legitimate only from inside the override of the same row,
which is the "I have nothing better, use baseq2's" fallback -- `RA_EndLevel`
and `OSP_EndLevel` both end that way and both are correct.  A call from
anywhere else reaches past whatever the active ruleset put in the row, and the
symptom is silence: the row is filled, the override is referenced so every
name-based sweep sees it as live, and the behaviour simply never happens.

`tools/dispatch.py` is the standing check and carries the exemption list.
=================
*/
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

bool G_SelectSpawnPoint(edict_t *ent, vec3_t origin, vec3_t angles)
{
    // NO `DMGame.SelectSpawnPoint` BRANCH, and it is worth a note because rogue
    // has one: its PutClientInServer chooses between the row and
    // SelectSpawnPoint on the line this hook replaced.  Reported as a finding by
    // `tools/deadvalue.py`'s first draft and it is not one -- the row's only
    // installer is Ground Zero's `case RDM_DEATHBALL`, which is commented out
    // here and byte-identically commented out in q2pro, so nothing has ever set
    // it and a caller would be unreachable.  Recorded rather than written
    //A tree that restores DBall needs this
    // branch as well as that case.
    //
    // A NULL row inherits rather than crashes, and "no hook" means
    // "the spot stands" -- only tourney refuses one.
    if (GATE(SelectSpawnPoint))
        return GATE(SelectSpawnPoint)(ent, origin, angles);
    return true;
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
    case RULESET_ARENA:
    case RULESET_DM:                    // baseq2's own rule before
    case RULESET_DMPRO:                 // the flattening: no monsters in a
    case RULESET_TDM:                   // deathmatch
    case RULESET_DUEL:
        return false;
    case RULESET_CTF:
        // Monsters are promised under ctf, and the combination is in the boot
        // matrix explicitly, because q2pro/src/ctf deleted the
        // monster set so Threewave's edits to g_ai.c, g_monster.c, g_combat.c
        // and g_turret.c have never been compiled against live monster code.
        // Q14 closed it as "unproven rather than broken".  Answering true here
        // is what makes the boot matrix able to find out.
        return true;
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
    switch (g_active_ruleset) {
    case RULESET_CTF:                   // implied by the ruleset
    case RULESET_TDM:                   // team play IS the ruleset now
    case RULESET_DUEL:                  // two teams of one, forced
        return true;
    default:
        // arena is the only ruleset left that reaches team play through the
        // modifier; the OSP four refuse it and sp has no teams, so
        // both answer false here through modifier_ok rather than by name.
        return G_ModifierEnabled(MOD_TEAMPLAY);
    }
}

// A range test, which is why the four are contiguous and first in
// the enum -- an `||` chain is a list, and a list is a thing a later ruleset
// gets left off.
bool G_IsOspRuleset(void)
{
    return g_active_ruleset <= RULESET_DUEL;
}

bool G_BotsAllowed(void)
{
    // Every ruleset but sp -- and then the operator's own switch,
    // which is refused under sp anyway so the first test is what decides there.
    return g_active_ruleset != RULESET_SP && g_modifier[MOD_BOTS];
}

// ------------------------------------------------------------- modifiers
//
// What a modifier is, decided in 1.22 for the two that were not one.
//
// A modifier is requested by cvar and refused where unsupported.  That does not
// say what it does where it is accepted, and for `runes` the answer
// was "nothing": CTF's techs come from CTFSetupTechSpawn on DF_CTF_NO_TECH
// alone and tourney's runes from the `runes_enable` BITMASK read at the call
// site, so G_ModifierEnabled(MOD_RUNES) had exactly one caller -- `sv ruleset`,
// printing it.  1.20 recorded that rather than choosing, because every fix
// available then changed behaviour: gating CTF's techs on `runes` takes techs
// out of a default CTF server, and replacing `runes_enable` with a boolean
// loses the bitmask that selects which runes spawn.  `bots` joins the same
// question, and both are decided together.
//
// The decision is that a modifier is **the resolved answer, not the request**.
// Both of the switches above are already the authority on whether runes or
// techs are in play; the modifier reports what they say, and the cvar is folded
// into them beforehand as a request that can only ever turn something ON.
// Three properties fall out:
//
//   * a default server is untouched.  `runes` defaults to 0, which means "do
//     not ask", not "turn them off", so a CTF map's techs and an OSP
//     server's runes_enable keep deciding exactly what they decided before.
//   * `runes 1` means something under ctf and the OSP four for the first
//     time -- it
//     clears DF_CTF_NO_TECH, or turns all five runes on where none were.  The
//     bitmask survives: asking for runes where some are already selected
//     changes nothing.
//   * `sv ruleset` stops lying.  `runes=1` now means runes or techs will
//     actually spawn, which is a fact a play test can check.
//
// The refusal under dm and arena is unchanged and still fires from the request,
// which is why the request is what InitGame stores and this runs afterwards.
//
// `bots` is the same shape with the switch on the other side: it IS the switch,
// it defaults to on, and G_BotsAllowed() reads it.  It is resolved in InitGame
// rather than here because nothing has to be read back.
//
// Called at the END of InitGame, after the ruleset's own init: `runes_enable`
// is registered by OSP_gameInit and `rune_stat` computed from it there, and
// reading either earlier would be reading a cvar this file had registered first
// with a default of its own, which is the trap.
void G_ResolveModifiers(void)
{
    cvar_t *req = gi.cvar("runes", "0", CVAR_LATCH);
    bool wanted = req->value != 0;
    int flags;

    if (G_IsOspRuleset()) {
        // `hook` is a one-way request like `runes`: an explicit OSP
        // hook_enable setting remains authoritative when the request is off.
        G_ApplyOspHookRequest();

        if (wanted && !rune_stat) {
            // All five bits.  Which runes is runes_enable's business and this
            // only ever fires when it has chosen none.
            gi.cvar_set("runes_enable", "31");
            OSP_SyncRuneState();
            gi.dprintf("Colosseum: 'runes 1' sets runes_enable to all "
                       "five\n");
        }
        g_modifier[MOD_RUNES] = rune_stat != 0;
        return;
    }

    switch (g_active_ruleset) {
    case RULESET_CTF:
        flags = (int)dmflags->value;
        if (wanted && (flags & DF_CTF_NO_TECH)) {
            gi.cvar_set("dmflags", va("%d", flags & ~DF_CTF_NO_TECH));
            gi.dprintf("Colosseum: 'runes 1' clears DF_CTF_NO_TECH so the "
                       "techs spawn\n");
        }
        g_modifier[MOD_RUNES] = !((int)dmflags->value & DF_CTF_NO_TECH);
        break;
    default:
        // arena and sp: the refusal in G_InitRuleset has already cleared it and
        // said so.  Restating it here keeps the derived value the only thing
        // anything reads.
        g_modifier[MOD_RUNES] = false;
        break;
    }
}

bool G_SavegamesAllowed(void)
{
    // Both halves.  The second is a guard rather than a gate and it is worth
    // being honest about that: no bot can exist under sp, because
    // G_BotsAllowed() is false there and every path that creates one asks --
    // so `botglobals.numbots` is 0 whenever the first test passes and this
    // clause has never fired.  It is here because the requirement says so and
    // because the day something makes a bot reachable under sp, a savegame
    // written with a fake client in a slot is not a thing that can be
    // reloaded: the brain is not saved, and the client it describes would come
    // back without one.  game_export_ex_t's CanSave() is where the engine
    // could be told before the file is opened; that hook is declared and not
    // yet filled.
    return g_active_ruleset == RULESET_SP && botglobals.numbots == 0;
}
