// Colosseum ruleset dispatch -- R-MODE-1..7.
//
// Resolution runs once, from InitGame, and never calls gi.error.  R-MODE-1 and
// R-MODE-4 both say so explicitly, and the reason is in the 1999 build this
// replaces: gladq2_src/g_save.c policed its three ruleset booleans by calling
// gi.error on each conflicting pair, so a bad config did not start a server with
// a warning -- it aborted the process.  Every branch below corrects the value,
// says so once, and continues.

#include "g_local.h"
// R-88's decision needs the two switches the runes modifier is derived FROM:
// Threewave's DF_CTF_NO_TECH and tourney's rune_stat.  See G_ResolveModifiers.
#include "ctf/g_ctf.h"
#include "tourney/osp_hooks.h"
// R-ENG-6's second half: no savegame while a bot exists.
#include "bot/bl_main.h"
// R-CTF-8 / R-DM-1's target, which the botfill rows below print.
#include "bot/bl_spawn.h"
// Phase 7's bot-placement census reads RA2's FIGHT_* and tourney's ENTERED_*.
#include "arena/arena.h"

// ---------------------------------------------------------------- state
//
// Resolved once by G_InitRuleset() and read everywhere.  Not a cvar lookup at
// the point of use: R-MODE-2 says the legacy cvars are evaluated *once*, at
// InitGame, and a mid-map cvar change must not be able to move the ruleset out
// from under code that already branched on it.
static ruleset_t    g_active_ruleset = RULESET_DM;
static bool         g_modifier[MOD_COUNT];
static bool         g_layer[LAYER_COUNT];

// R-COMPAT-6, and see reconcile above: one pair, two donors.
cvar_t             *g_statsfile;
cvar_t             *g_statsname;
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
    [RULESET_CTF]     = &ops_ctf,   // src/ctf/g_ctf.c
    [RULESET_ARENA]   = &ops_arena, // src/arena/arena.c
    [RULESET_TOURNEY] = &ops_tourney, // src/tourney/osp_main.c
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

    // `ctf` is serverinfo that a Threewave-aware client reads to decide what to
    // draw, so it must agree with the resolved ruleset in both directions --
    // `g_ruleset ctf` without `ctf 1` has to advertise CTF, and `ctf 1` with
    // `g_ruleset dm` must not.  Threewave registers this cvar itself with
    // default 1; it is registered here instead, because resolution owns it
    // (R-MODE-2) and a second registration would re-assert a default that
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
    // The REQUEST, so that the refusal below still fires under dm and arena.
    // G_ResolveModifiers() overwrites it at the end of InitGame with what the
    // ruleset's own switch actually says -- see there.
    g_modifier[MOD_RUNES]    = gi.cvar("runes", "0", CVAR_LATCH)->value != 0;
    // R-88's other half.  `bots` is the operator's switch for the whole layer
    // and DEFAULTS TO 1 where the ruleset accepts it, so a server that says
    // nothing behaves exactly as it did before the modifier existed.  It is not
    // a second answer to G_BotsAllowed() -- it is what G_BotsAllowed() reads,
    // which is why there is still one question asked by name.  The reason it
    // exists at all is R-SEC-2's staged exposure: a public server wants to be
    // able to turn the bot layer off without turning the ruleset off.
    g_modifier[MOD_BOTS]     = gi.cvar("bots", "1", CVAR_LATCH)->value != 0;

    // R-COMPAT-6: `statsfile` and `statsname` are registered ONCE, here, with a
    // default chosen by the ruleset that will use them.  Both donors ship a
    // local-file statistics log (R-RA-7, R-OSP-3) and both named their cvars
    // the same, with different defaults -- which is a cvar whose value depends
    // on which translation unit happened to register it first.  Resolution owns
    // the pair because resolution is what decides which log is going to run.
    g_statsfile = gi.cvar("statsfile", "1", 0);
    g_statsname = gi.cvar("statsname",
                          g_active_ruleset == RULESET_ARENA ? "ra2stats.jsonl"
                          : "osptourney.jsonl", 0);

    for (int m = 0; m < MOD_COUNT; m++) {
        if (g_modifier[m] && !modifier_ok[m][g_active_ruleset]) {
            gi.dprintf("Colosseum: ruleset '%s' does not accept modifier '%s'; "
                       "disabling it (R-MODE-4)\n",
                       ruleset_names[g_active_ruleset], modifier_names[m]);
            g_modifier[m] = false;
        }
    }

    // The slot map is per ruleset (R-OSP-7) and must be resolved before any
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
            // R-CORE-11's gate, observed rather than asserted: which evasion
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
               "flavour      %d monster(s) latched xatrix, %d rogue "
               "(R-CORE-11a)\n"
               "evasion      %d monster(s) on Ground Zero's dodge, %d on "
               "baseq2's (R-CORE-11)\n"
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

    // R-CTF-1's content, counted rather than assumed.  Nine CTF maps booting
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

    // R-VER-19's argument, applied to Phase 7: a bot's PLACEMENT has to be
    // observable from outside the library, or "bots play in ctf" is a claim
    // about source rather than a fact about a server.  `sv clientdump` says
    // which slots hold bots and which library each uses; it cannot say which
    // team, which arena or whether the match counts them, and those are exactly
    // what R-CTF-4, R-RA-4 and R-OSP-11 are about.
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

        // FL_BOT and FL_BOTCLIENT are two different bits (R-CORE-14) and Phase
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
                // WHAT THE BRAIN IS TOLD, which is not what the wire carries.
                // `clientsettings[].skin` is the only currency the Gladiator
                // brain has for CTF teams -- BotCTFTeam() is
                // `strstr(skin, "ctf_r") ? RED : BLUE` and BotSameTeam()
                // compares the half after the '/' -- and bl_main.c fills it
                // from THIS string, the game's own copy of the userinfo.
                // CTFAssignSkin writes the playerskins configstring either way,
                // so a client reading the wire cannot tell the two apart: a bot
                // wearing `ctf_r` on every screen while its brain still reads
                // `male/viper` is a bot that thinks the whole server is on the
                // other side.  Nothing else in this tree can report it, which
                // is why it is a field here (R-CTF-4).
                if (strstr(Info_ValueForKey(e->client->pers.userinfo, "skin"),
                           "ctf_"))
                    teamskin++;
            }
            gi.cprintf(NULL, PRINT_HIGH,
                       "botplace     ctf red=%d blue=%d noteam=%d teamskin=%d, "
                       "FL_BOTCLIENT=%d (R-CTF-4)\n",
                       red, blue, noteam, teamskin, nbotclient);
            // R-CTF-8, and a line of its own for R-VER-19's reason: the target
            // is computed from the map's three spawn pools rather than read from
            // a cvar, so a play test has nowhere else to read it back from.
            // `seats` is what the map says and `want` what it survives the
            // ceilings as -- printing only the second hides a clamp.
            if (BotFillEnabled())
                gi.cprintf(NULL, PRINT_HIGH,
                           "botfill      ctf want=%d of seats=%d, shared=%d "
                           "base=%d+%d (R-CTF-8)\n",
                           BotFillTarget(), CTF_BotFillSeats(),
                           G_SpawnPointPool("info_player_deathmatch"),
                           G_SpawnPointPool("info_player_team1"),
                           G_SpawnPointPool("info_player_team2"));
            else
                gi.cprintf(NULL, PRINT_HIGH,
                           "botfill      off -- minimumplayers %d is the "
                           "target (R-CTF-8)\n", (int)BotMinPlayers()->value);
            break;
        }
        case RULESET_ARENA: {
            int placed = 0, teamed = 0, fighting = 0, arena1 = 0;
            int fill = RA_BotFillArena();

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
                       "fighting=%d, FL_BOTCLIENT=%d (R-RA-4)\n",
                       placed, arena1, teamed, fighting, nbotclient);
            // R-RA-7, and a line of its own for R-VER-19's reason: the target
            // is computed from the arena rather than read from a cvar, so a
            // play test has nowhere else to read it back from.
            if (fill)
                gi.cprintf(NULL, PRINT_HIGH,
                           "botfill      arena %d, %d/%d players%s (R-RA-7)\n",
                           fill, RA_ArenaPlayers(fill, NULL),
                           RA_BotFillTarget(fill),
                           arenas[fill].idarena ? ", pickup: by spawn points"
                                                : ", duel: by playersperteam");
            else
                gi.cprintf(NULL, PRINT_HIGH,
                           "botfill      off -- minimumplayers %d is the "
                           "target (R-RA-7)\n", (int)BotMinPlayers()->value);
            break;
        }
        case RULESET_TOURNEY: {
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
                       "botplace     tourney m_mode=%d entered=%d ready=%d "
                       "team0=%d team1=%d, FL_BOTCLIENT=%d (R-OSP-11)\n",
                       m_mode, entered, ready, t0, t1, nbotclient);
            // R-156.  Tourney has no `botfill` switch and does not need one, and
            // the row says why rather than printing nothing: `team_maxplayers`
            // IS a declared capacity -- 4 by default, forced to 1 CVAR_NOSET
            // under mode 3, clamped so that twice it fits `maxclients` -- which
            // is precisely what dm and ctf lack and what R-DM-1 / R-CTF-8 had to
            // read off the map instead.  Modes 0 and 1 have no teams and would
            // fall to the same map-sized answer dm gets; that is a change to
            // tourney's bot contract, which R-OSP-11 preserves rather than
            // redesigns, so it is recorded here and not made.
            //
            // `team_maxplayers` is deliberately NOT printed here: its default is
            // itself mode-dependent (1 under mode 3, 4 otherwise), so naming a
            // default at this call site would be R-COMPAT-6's collision -- one
            // cvar registered twice with two values.  `bots_autoload` has one
            // default and `bl_spawn.c` already reads it exactly this way.
            gi.cprintf(NULL, PRINT_HIGH,
                       "botfill      tourney has no fill switch -- %s %d is the "
                       "target, bots_autoload %d, capacity is "
                       "team_maxplayers (R-156)\n",
                       BotMinPlayersCvar(), (int)BotMinPlayers()->value,
                       (int)gi.cvar("bots_autoload", "0", 0)->value);
            break;
        }
        default:
            gi.cprintf(NULL, PRINT_HIGH,
                       "botplace     %s has no per-ruleset bot placement, "
                       "FL_BOTCLIENT=%d\n", G_RulesetName(G_Ruleset()),
                       nbotclient);
            // R-DM-1.  `dm` is the arm this reaches, and `sp` has no bots to
            // report on; BotFillEnabled() is false under both `sp` and
            // `tourney`, which have no such switch (see BotFillCvar()).
            if (BotFillEnabled())
                gi.cprintf(NULL, PRINT_HIGH,
                           "botfill      dm want=%d of seats=%d, spawns=%d%s "
                           "(R-DM-1)\n",
                           BotFillTarget(), DM_BotFillSeats(),
                           G_SpawnPointPool("info_player_deathmatch"),
                           G_TeamplayEnabled() ? ", teamplay: even" : "");
            else
                gi.cprintf(NULL, PRINT_HIGH,
                           "botfill      off -- %s %d is the target (R-DM-1)\n",
                           BotMinPlayersCvar(), (int)BotMinPlayers()->value);
            break;
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
    // N6, R-MODE-7: every ruleset but sp -- and then the operator's own switch,
    // which is refused under sp anyway so the first test is what decides there.
    return g_active_ruleset != RULESET_SP && g_modifier[MOD_BOTS];
}

// ---------------------------------------------------------------- R-88
//
// WHAT A MODIFIER IS, decided in 1.22 for the two that were not one.
//
// R-MODE-4 says a modifier is requested by cvar and refused where unsupported.
// It does not say what it does where it is ACCEPTED, and for `runes` the answer
// was "nothing": CTF's techs come from CTFSetupTechSpawn on DF_CTF_NO_TECH
// alone and tourney's runes from the `runes_enable` BITMASK read at the call
// site, so G_ModifierEnabled(MOD_RUNES) had exactly one caller -- `sv ruleset`,
// printing it.  1.20 recorded that rather than choosing, because every fix
// available then changed behaviour: gating CTF's techs on `runes` takes techs
// out of a default CTF server, and replacing `runes_enable` with a boolean
// loses the bitmask that selects WHICH runes spawn.  Phase 6 adds `bots` to the
// same question and 1.21's prompt asked for both to be decided together.
//
// The decision is that a modifier is **the resolved answer, not the request**.
// Both of the switches above are already the authority on whether runes or
// techs are in play; the modifier reports what they say, and the cvar is folded
// into them beforehand as a request that can only ever turn something ON.
// Three properties fall out:
//
//   * a default server is untouched.  `runes` defaults to 0, which means "do
//     not ask", not "turn them off", so a CTF map's techs and a tourney
//     server's runes_enable keep deciding exactly what they decided before.
//   * `runes 1` means something under ctf and tourney for the first time -- it
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
// with a default of its own -- R-COMPAT-6's exact trap.
void G_ResolveModifiers(void)
{
    cvar_t *req = gi.cvar("runes", "0", CVAR_LATCH);
    bool wanted = req->value != 0;
    int flags;

    switch (g_active_ruleset) {
    case RULESET_CTF:
        flags = (int)dmflags->value;
        if (wanted && (flags & DF_CTF_NO_TECH)) {
            gi.cvar_set("dmflags", va("%d", flags & ~DF_CTF_NO_TECH));
            gi.dprintf("Colosseum: 'runes 1' clears DF_CTF_NO_TECH so the techs "
                       "spawn (R-MODE-4, R-88)\n");
        }
        g_modifier[MOD_RUNES] = !((int)dmflags->value & DF_CTF_NO_TECH);
        break;
    case RULESET_TOURNEY:
        if (wanted && !rune_stat) {
            // All five bits.  Which runes is runes_enable's business and this
            // only ever fires when it has chosen none.
            gi.cvar_set("runes_enable", "31");
            rune_stat = 0x1f;
            gi.dprintf("Colosseum: 'runes 1' sets runes_enable to all five "
                       "(R-MODE-4, R-88)\n");
        }
        g_modifier[MOD_RUNES] = rune_stat != 0;
        break;
    default:
        // dm, arena and sp: the refusal in G_InitRuleset has already cleared it
        // and said so.  Restating it here keeps the derived value the only
        // thing anything reads.
        g_modifier[MOD_RUNES] = false;
        break;
    }
}

bool G_SavegamesAllowed(void)
{
    // R-ENG-6, both halves as of 1.22.  The second is a GUARD rather than a
    // gate and it is worth being honest about that: no bot can exist under sp,
    // because G_BotsAllowed() is false there and every path that creates one
    // asks -- so `botglobals.numbots` is 0 whenever the first test passes and
    // this clause has never fired.  It is here because the requirement says so
    // and because the day something makes a bot reachable under sp, a savegame
    // written with a fake client in a slot is not a thing that can be reloaded:
    // the brain is not saved, and the client it describes would come back
    // without one.  game_export_ex_t's CanSave() is where the engine could be
    // told BEFORE the file is opened; that hook is declared and not yet filled.
    return g_active_ruleset == RULESET_SP && botglobals.numbots == 0;
}
