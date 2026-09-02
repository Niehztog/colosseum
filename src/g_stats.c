// Colosseum per-ruleset stat slot map and composed statusbar -- R-OSP-7,
// R-OSP-7a.  The map itself is in g_stats.h; this is the resolution, the
// accessors and the emitter.

#include "g_local.h"
#include "tourney/osp_types.h"

// ---------------------------------------------------------------- the map

typedef struct {
    statkind_t  kind;
    int8_t      slot[RULESET_COUNT];
} slotdef_t;

// A column is a NUMBERING, not a ruleset, and the four OSP rulesets share one:
// they are one code path emitting one bar (R-OSP-12), so a slot that meant
// something different under `tdm` than under `dm` would be a bar addressing a
// number its own emitter did not choose.
//
// baseq2's own numbering is gone with this: it was read by RULESET_DM alone,
// and `dm` is OSP's RegularDM now.  Five columns became four.
//
// A ruleset MISSING from this expansion does not fail to compile -- it gets 0
// for every stat, which is a real slot (STAT_HEALTH_ICON) and not the -1
// sentinel, so every logical stat would silently collide on one universal slot.
#define STATSLOT_ROW(id, kind, osp, ctf, arena, sp) \
    [id] = { kind, { [RULESET_DM] = osp, [RULESET_DMPRO] = osp, \
                     [RULESET_TDM] = osp, [RULESET_DUEL] = osp, \
                     [RULESET_CTF] = ctf, [RULESET_ARENA] = arena, \
                     [RULESET_SP] = sp } },

static const slotdef_t slotdefs[SID_COUNT] = {
    STATSLOT_MAP(STATSLOT_ROW)
};

#undef STATSLOT_ROW

#define STATSLOT_NAME(id, kind, osp, ctf, arena, sp)    #id,

static const char *const slotnames[SID_COUNT] = {
    STATSLOT_MAP(STATSLOT_NAME)
};

#undef STATSLOT_NAME

// Resolved once for the active ruleset.  A lookup rather than a map walk
// because G_SetStat runs per client per frame.
static int8_t g_slot[SID_COUNT];

// ---------------------------------------------------------------- resolution

// The highest slot + 1 that is reachable right now, and it is TWO bounds at
// once rather than one.
//
// R-OSP-7 clause 6 reads as a single question -- did the client negotiate the
// protocol extension? -- and that was the whole story while USE_NEW_GAME_API
// was fixed on.  It stopped being the whole story when R-ENG-1a made the game
// API a build switch, because the two switches are INDEPENDENT in both
// directions.  g_local.h's PM_TIME_SHIFT already handles one direction (the new
// API without extensions); this is the other, and it is the dangerous one:
// G_InitGame sets game.csr from sv_features and g_protocol_extensions alone, so
// `extended` can be true on a library whose player_state_t is
// player_state_old_t with stats[MAX_STATS_OLD].
//
//     what the ARRAY holds    MAX_STATS -- 64 or 32, fixed at compile time by
//                             USE_NEW_GAME_API
//     what the WIRE carries   64 only for a client that negotiated the
//                             extension, otherwise 32
//
// A check that asks only about the wire keeps CTF's timer pair at 32/33 on an
// old-API build talking to an extended client -- two writes off the end of the
// struct, every frame, for every client holding a second powerup, on the one
// ruleset that uses the pair.  The lower of the two bounds wins, and
// MAX_STATS >= MAX_STATS_OLD always, so the expression below IS that minimum
// rather than an approximation of it.
//
// Both bounds are enforced in this one place on purpose: `used[]` below is
// sized MAX_STATS and indexed by the resolved slot, so a ceiling that let 32
// through on a 32-entry array would overrun the checker as well as the client.
static int stat_ceiling(void)
{
    return game.csr.extended ? MAX_STATS : MAX_STATS_OLD;
}

void G_InitStats(void)
{
    ruleset_t r = G_Ruleset();
    int used[MAX_STATS];
    int ceiling = stat_ceiling();

    memset(used, -1, sizeof(used));

    for (int i = 0; i < SID_COUNT; i++) {
        int slot = slotdefs[i].slot[r];

        // R-OSP-7 clause 6, and it is the reason CTF's timer pair may sit at
        // 32/33 at all: those slots exist only for a client that negotiated the
        // protocol extension AND only on a library built against the new game
        // API, so short of both the stat is dropped -- from the bar and from the
        // write together, because both go through this array.
        if (slot >= ceiling)
            slot = -1;

        g_slot[i] = slot;

        if (slot < 0)
            continue;

        // The two invariants the build-time check (tools/slotkind.py) also
        // asserts, restated here so that a bad map is loud at InitGame rather
        // than a wrong number on a HUD.  Neither is reachable from a config, so
        // neither is a gi.error case: R-MODE-1's "never abort startup" is about
        // user input, and a programming error the build should have caught is
        // better reported than fatal on a live server.
        if (slot < 16) {
            gi.dprintf("Colosseum: %s claims universal slot %d -- slots 0..15 "
                       "are unclaimable (R-OSP-7 clause 1)\n",
                       slotnames[i], slot);
            g_slot[i] = -1;
            continue;
        }
        if (used[slot] >= 0) {
            gi.dprintf("Colosseum: slot %d claimed by both %s and %s in "
                       "ruleset '%s' (R-OSP-7)\n", slot, slotnames[used[slot]],
                       slotnames[i], G_RulesetName(r));
            g_slot[i] = -1;
            continue;
        }
        used[slot] = i;
    }
}

int G_Stat(statslot_t id)
{
    return (id >= 0 && id < SID_COUNT) ? g_slot[id] : -1;
}

void G_SetStat(edict_t *ent, statslot_t id, int value)
{
    int slot = G_Stat(id);

    if (slot >= 0)
        ent->client->ps.stats[slot] = value;
}

int G_GetStat(edict_t *ent, statslot_t id)
{
    int slot = G_Stat(id);

    return (slot >= 0) ? ent->client->ps.stats[slot] : 0;
}

// ---------------------------------------------------------------- emitter
//
// One complete item at a time.  A statusbar program that is cut mid-token does
// not fail loudly -- the client stops interpreting at the break and the rest of
// the HUD silently vanishes -- so a dropped item is recorded and the bar stays
// syntactically whole.  Same discipline as R-MENU-5 asks of the menu core.

void sb_init(statusbar_t *sb)
{
    sb->data[0] = 0;
    sb->len = 0;
    sb->overflow = false;
    sb->skip = 0;
}

// SUPPRESSING A WHOLE BLOCK, and why this is not a detail.
//
// An `if` whose slot the active ruleset does not have must take its entire body
// with it.  The first version emitted nothing for the `if` and then emitted the
// body anyway, so the ctf bar came out as
//
//     ... if 9 xv 262 num 2 10 xv 296 pic 9 endif yb -76 xv 262 xv 296 yb -50
//         endif if 11 ...
//
// -- two stray cursor moves and an `endif` with no `if`, which unbalances every
// conditional after it.  Found by running `sv slots` under ctf; the dm bar is
// unaffected because dm maps every slot its bar mentions, which is exactly the
// kind of blind spot R-VER-19 exists to remove.
static bool sb_suppressed(statusbar_t *sb)
{
    return sb->skip > 0;
}

static void sb_append(statusbar_t *sb, const char *fmt, ...)
{
    char    item[64];
    va_list argptr;
    size_t  n;

    va_start(argptr, fmt);
    n = Q_vsnprintf(item, sizeof(item), fmt, argptr);
    va_end(argptr);

    if (n >= sizeof(item) || sb->len + n >= sizeof(sb->data)) {
        sb->overflow = true;
        return;
    }

    memcpy(sb->data + sb->len, item, n + 1);
    sb->len += n;
}

void sb_raw(statusbar_t *sb, const char *token)
{
    if (sb_suppressed(sb))
        return;
    sb_append(sb, "%s ", token);
}

void sb_layout(statusbar_t *sb, const char *op, int v)
{
    if (sb_suppressed(sb))
        return;
    sb_append(sb, "%s %d ", op, v);
}

void sb_endif(statusbar_t *sb)
{
    if (sb->skip > 0) {
        sb->skip--;
        return;
    }
    sb_append(sb, "endif ");
}

// The kind check.  A mismatch here is R-OSP-7 clause 7's type error, caught at
// the one place that knows both the intent and the declaration.
static bool sb_kind_ok(statslot_t id, statkind_t want, const char *op)
{
    if (slotdefs[id].kind == want)
        return true;
    gi.dprintf("Colosseum: statusbar draws %s with `%s` but the slot map "
               "declares it kind %d (R-OSP-7 clause 7)\n",
               slotnames[id], op, slotdefs[id].kind);
    return false;
}

void sb_if(statusbar_t *sb, statslot_t id)
{
    int slot = G_Stat(id);

    // Already inside a suppressed block: nest, so the matching endif does not
    // close the outer one.
    if (sb_suppressed(sb)) {
        sb->skip++;
        return;
    }
    if (slot < 0) {
        sb->skip = 1;
        return;
    }
    sb_append(sb, "if %d ", slot);
}

void sb_pic(statusbar_t *sb, statslot_t id)
{
    int slot = G_Stat(id);

    if (sb_suppressed(sb))
        return;
    if (slot >= 0 && sb_kind_ok(id, SK_PIC, "pic"))
        sb_append(sb, "pic %d ", slot);
}

void sb_num(statusbar_t *sb, int width, statslot_t id)
{
    int slot = G_Stat(id);

    if (sb_suppressed(sb))
        return;
    if (slot >= 0 && sb_kind_ok(id, SK_NUM, "num"))
        sb_append(sb, "num %d %d ", width, slot);
}

void sb_stat_string(statusbar_t *sb, statslot_t id)
{
    int slot = G_Stat(id);

    if (sb_suppressed(sb))
        return;
    if (slot >= 0 && sb_kind_ok(id, SK_CS, "stat_string"))
        sb_append(sb, "stat_string %d ", slot);
}

void sb_uif(statusbar_t *sb, int slot)
{
    if (sb_suppressed(sb)) {
        sb->skip++;
        return;
    }
    sb_append(sb, "if %d ", slot);
}

void sb_upic(statusbar_t *sb, int slot)
{
    if (sb_suppressed(sb))
        return;
    sb_append(sb, "pic %d ", slot);
}

void sb_unum(statusbar_t *sb, int width, int slot)
{
    if (sb_suppressed(sb))
        return;
    sb_append(sb, "num %d %d ", width, slot);
}

void sb_ustat_string(statusbar_t *sb, int slot)
{
    if (sb_suppressed(sb))
        return;
    sb_append(sb, "stat_string %d ", slot);
}

// ---------------------------------------------------------------- the bars
//
// These reproduce, item for item, the three literals they replace:
// g_spawn.c's single_statusbar and dm_statusbar, and g_ctf.c's ctf_statusbar.
// The universal block is byte-identical in effect for every ruleset; what
// differs is which tail follows it and where the second powerup timer landed.

// Slots 0..15: health, ammo, armour, selected item, pickup, powerup timer,
// help/weapon icon.  Every ruleset draws this and every donor shipped its own
// copy of it -- which is why three complete bars existed to disagree.
// Two shape differences between the donors' copies of the block above.  They
// are coordinates and an ordering, not content, but R-OSP-7a asks the emitter
// to reproduce each donor's bar rather than approximate it, so they are
// parameters instead of a second copy of sixty lines.
typedef struct {
    bool    icon_above;     // RA2 lifts the health icon to its own row at -32
    // 262 in baseq2 ONLY.  Threewave's ctf_statusbar reads `if 9 xv 246 num 2
    // 10 xv 296 pic 9 endif` -- the same 246 RA2 and tourney use -- and this
    // comment said 262 for it, which is how ctf came to be handed
    // sb_shape_baseq2 and draw the powerup countdown sixteen pixels right of
    // where 1999 put it.  The composed bar is token-identical to the donor's
    // literal in every other respect, read off a client on q2ctf1, so this one
    // number was the whole divergence (R-175).
    int     timer1_x;
} sb_shape_t;

static const sb_shape_t sb_shape_baseq2  = { false, 262 };
static const sb_shape_t sb_shape_ctf     = { false, 246 };
static const sb_shape_t sb_shape_arena   = { true,  246 };
static const sb_shape_t sb_shape_tourney = { false, 246 };

static void sb_universal(statusbar_t *sb, const sb_shape_t *shape)
{
    // health
    if (shape->icon_above) {
        sb_layout(sb, "yb", -32);
        sb_layout(sb, "xv", 50);
        sb_upic(sb, STAT_HEALTH_ICON);
        sb_layout(sb, "yb", -24);
        sb_layout(sb, "xv", 0);
        sb_raw(sb, "hnum");
    } else {
        sb_layout(sb, "yb", -24);
        sb_layout(sb, "xv", 0);
        sb_raw(sb, "hnum");
        sb_layout(sb, "xv", 50);
        sb_upic(sb, STAT_HEALTH_ICON);
    }

    // ammo
    sb_uif(sb, STAT_AMMO_ICON);
    sb_layout(sb, "xv", 100);
    sb_raw(sb, "anum");
    sb_layout(sb, "xv", 150);
    sb_upic(sb, STAT_AMMO_ICON);
    sb_endif(sb);

    // armor
    sb_uif(sb, STAT_ARMOR_ICON);
    sb_layout(sb, "xv", 200);
    sb_raw(sb, "rnum");
    sb_layout(sb, "xv", 250);
    sb_upic(sb, STAT_ARMOR_ICON);
    sb_endif(sb);

    // selected item
    sb_uif(sb, STAT_SELECTED_ICON);
    sb_layout(sb, "xv", 296);
    sb_upic(sb, STAT_SELECTED_ICON);
    sb_endif(sb);

    sb_layout(sb, "yb", -50);

    // picked up item
    sb_uif(sb, STAT_PICKUP_ICON);
    sb_layout(sb, "xv", 0);
    sb_upic(sb, STAT_PICKUP_ICON);
    sb_layout(sb, "xv", 26);
    sb_layout(sb, "yb", -42);
    sb_ustat_string(sb, STAT_PICKUP_STRING);
    sb_layout(sb, "yb", -50);
    sb_endif(sb);

    // timer 1 (quad, quadfire, double, enviro, breather, sphere, ir)
    sb_uif(sb, STAT_TIMER_ICON);
    sb_layout(sb, "xv", shape->timer1_x);
    sb_unum(sb, 2, STAT_TIMER);
    sb_layout(sb, "xv", 296);
    sb_upic(sb, STAT_TIMER_ICON);
    sb_endif(sb);

    // timer 2 (pent).  The shared mechanic of R-OSP-7 clause 4: one call site,
    // one place in the bar, and the slot pair comes from the ruleset's map --
    // 18/19 under dm and sp, 32/33 under ctf, nothing at all on a server
    // without protocol extensions under ctf.
    sb_if(sb, SID_TIMER2_ICON);
    sb_layout(sb, "yb", -76);
    sb_layout(sb, "xv", 262);
    sb_num(sb, 2, SID_TIMER2);
    sb_layout(sb, "xv", 296);
    sb_pic(sb, SID_TIMER2_ICON);
    sb_layout(sb, "yb", -50);
    sb_endif(sb);

    // help / weapon icon
    sb_uif(sb, STAT_HELPICON);
    sb_layout(sb, "xv", 148);
    sb_upic(sb, STAT_HELPICON);
    sb_endif(sb);
}

// The frag counter.  Deathmatch in every ruleset that has one; the campaign has
// none, which is why baseq2 keeps it out of single_statusbar.
static void sb_frags(statusbar_t *sb)
{
    sb_layout(sb, "xr", -50);
    sb_layout(sb, "yt", 2);
    sb_unum(sb, 3, STAT_FRAGS);
}

// BASEQ2'S DM TAIL IS GONE, and the compiler is what said so.  It drew the
// spectator banner and the chase-cam name, and it was reached through the
// composer's `default:` arm, which only RULESET_DM ever took -- `sp` breaks
// early with no tail at all and the other rulesets have their own.  When `dm`
// became OSP's RegularDM the arm had no ruleset left, and -Werror=unused-function
// reported it on the first build after the switch was made exhaustive.
//
// Nothing inherits it: SID_SPECTATOR is mapped only under `sp` now (the OSP
// column claims 17 for SID_OSP_MATCHSTATE, which R-OSP-7 clause 2 permits for a
// ruleset with its own observer), and every remaining bar draws its own frags.

// Threewave's own block: tech icon, both team panels with their capture counts
// and "joined" overlays, the carried-flag badge, the id view, the match clock
// and the team warning line.  Order and coordinates are the donor's.
//
// CTF has no chase element here: it draws the chased player's name as a unicast
// layout from UpdateChaseCam instead, which is why SID_CHASE is unmapped under
// ctf and slot 16 stays unused rather than being claimed.
static void sb_ctf_tail(statusbar_t *sb)
{
    sb_frags(sb);

    // tech
    sb_layout(sb, "yb", -129);
    sb_if(sb, SID_CTF_TECH);
    sb_layout(sb, "xr", -26);
    sb_pic(sb, SID_CTF_TECH);
    sb_endif(sb);

    // red team
    sb_layout(sb, "yb", -102);
    sb_if(sb, SID_CTF_TEAM1_PIC);
    sb_layout(sb, "xr", -26);
    sb_pic(sb, SID_CTF_TEAM1_PIC);
    sb_endif(sb);
    sb_layout(sb, "xr", -62);
    sb_num(sb, 2, SID_CTF_TEAM1_CAPS);

    // joined-red overlay
    sb_if(sb, SID_CTF_JOINED_TEAM1_PIC);
    sb_layout(sb, "yb", -104);
    sb_layout(sb, "xr", -28);
    sb_pic(sb, SID_CTF_JOINED_TEAM1_PIC);
    sb_endif(sb);

    // blue team
    sb_layout(sb, "yb", -75);
    sb_if(sb, SID_CTF_TEAM2_PIC);
    sb_layout(sb, "xr", -26);
    sb_pic(sb, SID_CTF_TEAM2_PIC);
    sb_endif(sb);
    sb_layout(sb, "xr", -62);
    sb_num(sb, 2, SID_CTF_TEAM2_CAPS);

    // joined-blue overlay
    sb_if(sb, SID_CTF_JOINED_TEAM2_PIC);
    sb_layout(sb, "yb", -77);
    sb_layout(sb, "xr", -28);
    sb_pic(sb, SID_CTF_JOINED_TEAM2_PIC);
    sb_endif(sb);

    // carried flag badge
    sb_if(sb, SID_CTF_FLAG_PIC);
    sb_layout(sb, "yt", 26);
    sb_layout(sb, "xr", -24);
    sb_pic(sb, SID_CTF_FLAG_PIC);
    sb_endif(sb);

    // id view
    sb_if(sb, SID_CTF_ID_VIEW);
    sb_layout(sb, "xv", 112);
    sb_layout(sb, "yb", -58);
    sb_stat_string(sb, SID_CTF_ID_VIEW);
    sb_endif(sb);

    sb_if(sb, SID_CTF_ID_VIEW_COLOR);
    sb_layout(sb, "xv", 96);
    sb_layout(sb, "yb", -58);
    sb_pic(sb, SID_CTF_ID_VIEW_COLOR);
    sb_endif(sb);

    // match clock
    sb_if(sb, SID_CTF_MATCH);
    sb_layout(sb, "xl", 0);
    sb_layout(sb, "yb", -78);
    sb_stat_string(sb, SID_CTF_MATCH);
    sb_endif(sb);

    // unbalanced-teams warning
    sb_if(sb, SID_CTF_TEAMINFO);
    sb_layout(sb, "xl", 0);
    sb_layout(sb, "yb", -88);
    sb_stat_string(sb, SID_CTF_TEAMINFO);
    sb_endif(sb);
}

// Rocket Arena's own blocks.  The countdown/status/roundinfo panel and the
// pickup-queue panel are drawn ABOVE the health row in RA2's literal, so they
// are a head rather than a tail; the id view is the only thing that follows.
//
// The queue panel's two `stat_string` slots are the ones whose names say _ICON:
// RA2 writes them as `game.csr.items + game.num_items + N`, which is a
// configstring index, and the map records SK_CS so the kind check agrees with
// the bar rather than with the name (R-OSP-7 clause 7).
static void sb_arena_head(statusbar_t *sb)
{
    // countdown, arena status line and round info
    sb_if(sb, SID_RA_COUNTDOWN);
    sb_layout(sb, "xv", 150);
    sb_layout(sb, "yt", 60);
    sb_num(sb, 2, SID_RA_COUNTDOWN);
    sb_layout(sb, "xv", 20);
    sb_layout(sb, "yt", 50);
    sb_stat_string(sb, SID_RA_ARENASTATUS);
    sb_layout(sb, "xv", 140);
    sb_layout(sb, "yt", 40);
    sb_stat_string(sb, SID_RA_ROUNDINFO);
    sb_endif(sb);

    // the two pickup-team queues
    sb_if(sb, SID_RA_SHOWQUEUE);
    sb_layout(sb, "xr", -34);
    sb_layout(sb, "yt", 32);
    sb_num(sb, 2, SID_RA_QUEUE1);
    sb_layout(sb, "xr", -34);
    sb_layout(sb, "yt", 62);
    sb_num(sb, 2, SID_RA_QUEUE2);
    sb_layout(sb, "xr", -64);
    sb_layout(sb, "yt", 40);
    sb_stat_string(sb, SID_RA_QUEUE1_ICON);
    sb_layout(sb, "xr", -64);
    sb_layout(sb, "yt", 70);
    sb_stat_string(sb, SID_RA_QUEUE2_ICON);
    sb_endif(sb);
}

// RA2 draws the frag counter and one more line: the name of whoever the
// crosshair or the tracking camera is on.  It has no spectator banner and no
// chase-cam element, because slots 16 and 17 are its own and its observer is
// arena.c's (R-EXTRA-6).
static void sb_arena_tail(statusbar_t *sb)
{
    sb_frags(sb);

    sb_if(sb, SID_RA_ID_VIEW);
    sb_layout(sb, "xv", 0);
    sb_layout(sb, "yb", -58);
    sb_stat_string(sb, SID_RA_ID_VIEW);
    sb_endif(sb);
}

// OSP Tourney's tail.  The donor ships FOUR complete bars and they differ only
// in where two panels sit: `client_hud` moves the match clock, and `OSP_IsTeams()`
// (team play and 1v1) replaces the frags/rank pair with a team layout.  Four
// literals in the donor, two booleans here -- which is the case R-OSP-7a was
// written for and the reason a literal bar cannot express a per-client option
// at all.
static void sb_tourney_tail(statusbar_t *sb, bool alt, bool team)
{
    // popup menu / layout line
    sb_layout(sb, "xl", 4);
    sb_if(sb, SID_OSP_LAYOUT1);
    sb_layout(sb, "yb", -34);
    sb_stat_string(sb, SID_OSP_LAYOUT1);
    sb_endif(sb);

    // the id / chase name
    sb_if(sb, SID_CHASE);
    sb_layout(sb, "xv", 44);
    sb_layout(sb, "yb", -67);
    sb_stat_string(sb, SID_CHASE);
    sb_endif(sb);

    if (!team) {
        // frags
        sb_if(sb, SID_OSP_STATUS1);
        sb_layout(sb, "xr", -44);
        sb_layout(sb, "yt", 2);
        sb_raw(sb, "string2 \"Frags\"");
        sb_layout(sb, "xr", -68);
        sb_layout(sb, "yt", 10);
        sb_stat_string(sb, SID_OSP_STATUS1);
        sb_layout(sb, "yt", 18);
        sb_stat_string(sb, SID_OSP_STATUS2);
        sb_endif(sb);

        // the match clock, which `client_hud` moves out of the corner
        sb_if(sb, SID_OSP_MATCHSTATE);
        if (alt) {
            sb_layout(sb, "xv", 180);
            sb_layout(sb, "yb", -38);
            sb_stat_string(sb, SID_OSP_MATCHSTATE);
            sb_layout(sb, "xv", 188);
            sb_layout(sb, "yb", -46);
            sb_raw(sb, "string2 \"Time\"");
        } else {
            sb_layout(sb, "xr", -44);
            sb_layout(sb, "yt", 66);
            sb_stat_string(sb, SID_OSP_MATCHSTATE);
            sb_layout(sb, "xr", -36);
            sb_layout(sb, "yt", 58);
            sb_raw(sb, "string2 \"Time\"");
        }
        sb_endif(sb);

        // rank
        sb_if(sb, SID_OSP_STATUS3);
        if (alt)
            sb_layout(sb, "xr", -36);
        sb_layout(sb, "yt", 34);
        sb_raw(sb, "string2 \"Rank\"");
        sb_layout(sb, "xr", -44);
        sb_layout(sb, "yt", 42);
        sb_stat_string(sb, SID_OSP_STATUS3);
        sb_endif(sb);
    } else {
        // the clock first in the team layout, and `client_hud` moves it the
        // same way it does above
        sb_if(sb, SID_OSP_MATCHSTATE);
        if (alt) {
            sb_layout(sb, "xv", 180);
            sb_layout(sb, "yb", -38);
            sb_stat_string(sb, SID_OSP_MATCHSTATE);
            sb_layout(sb, "xv", 188);
            sb_layout(sb, "yb", -46);
            sb_raw(sb, "string2 \"Time\"");
        } else {
            sb_layout(sb, "xr", -36);
            sb_layout(sb, "yt", 50);
            sb_raw(sb, "string2 \"Time\"");
            sb_layout(sb, "xr", -44);
            sb_layout(sb, "yt", 58);
            sb_stat_string(sb, SID_OSP_MATCHSTATE);
        }
        sb_endif(sb);

        // both teams' score lines
        sb_if(sb, SID_OSP_STATUS1);
        sb_layout(sb, "xr", -124);
        sb_layout(sb, "yt", 2);
        sb_stat_string(sb, SID_OSP_STATUS1);
        sb_layout(sb, "xr", -108);
        sb_layout(sb, "yt", 10);
        sb_stat_string(sb, SID_OSP_STATUS2);
        sb_endif(sb);

        sb_if(sb, SID_OSP_STATUS3);
        sb_layout(sb, "xr", -124);
        sb_layout(sb, "yt", 26);
        sb_stat_string(sb, SID_OSP_STATUS3);
        sb_layout(sb, "xr", -108);
        sb_layout(sb, "yt", 34);
        sb_stat_string(sb, SID_OSP_STATUS4);
        sb_endif(sb);
    }

    // the five rune indicators
    sb_layout(sb, "xr", -68);
    sb_layout(sb, "yt", 90);
    sb_if(sb, SID_OSP_RUNE_RESIST);
    sb_raw(sb, "string \"  RESIST\"");
    sb_endif(sb);
    sb_if(sb, SID_OSP_RUNE_STRENGTH);
    sb_raw(sb, "string \"STRENGTH\"");
    sb_endif(sb);
    sb_if(sb, SID_OSP_RUNE_HASTE);
    sb_raw(sb, "string \"   HASTE\"");
    sb_endif(sb);
    sb_if(sb, SID_OSP_RUNE_REGEN);
    sb_raw(sb, "string \"   REGEN\"");
    sb_endif(sb);
    sb_if(sb, SID_OSP_RUNE_VAMPIRE);
    sb_raw(sb, "string \" VAMPIRE\"");
    sb_endif(sb);
}

// Counts `if` against `endif` in the finished program.  Cheap, and it is the
// only check that sees the *result* rather than the intent.
static bool sb_unbalanced(const statusbar_t *sb)
{
    const char *p = sb->data;
    int depth = 0;

    while ((p = strstr(p, "if ")) != NULL) {
        // "endif " ends in "if " too, so look at what precedes the match
        if (p >= sb->data + 3 && !memcmp(p - 3, "end", 3))
            depth--;
        else
            depth++;
        p += 3;
    }
    return depth != 0 || sb->skip != 0;
}

// The whole composition, in one place.  `sv slots` prints the bar it is going
// to install, and it can only do that honestly if it runs the same code -- when
// arena's head block landed, a duplicated switch here would have installed one
// bar and reported another (R-VER-19).
static void sb_compose(statusbar_t *sb, ruleset_t r)
{
    sb_init(sb);

    // arena is the one ruleset whose own block precedes the shared one.
    if (r == RULESET_ARENA)
        sb_arena_head(sb);

    sb_universal(sb, r == RULESET_ARENA ? &sb_shape_arena
                 : r == RULESET_CTF     ? &sb_shape_ctf
                 : G_IsOspRuleset()     ? &sb_shape_tourney
                 : &sb_shape_baseq2);

    // EVERY ruleset has an arm.  There was a `default:` here that composed
    // baseq2's tail, and `dm` was the only ruleset that reached it -- so when
    // `dm` became OSP's RegularDM the default would have kept handing it
    // baseq2's bar while its stat map said OSP's, which is a statusbar
    // addressing slots that no longer mean what it thinks.
    switch (r) {
    case RULESET_CTF:
        sb_ctf_tail(sb);
        break;
    case RULESET_ARENA:
        sb_arena_tail(sb);
        break;
    case RULESET_DM:
    case RULESET_DMPRO:
    case RULESET_TDM:
    case RULESET_DUEL:
        // `client_hud` is per-CLIENT in the donor and the bar is one
        // configstring for everyone, so it is read here as a server default --
        // which is what the donor does too, installing one of its four at
        // SpawnEntities and unicasting another only from `hud`.
        //
        // The donor's four literal bars differ in two booleans, and both are
        // still booleans here: the clock position, and whether the frags/rank
        // pair is replaced by a team layout -- which is `tdm` and `duel` now
        // rather than `m_mode >= 2`.
        sb_tourney_tail(sb, client_hud && client_hud->value != 0,
                        OSP_IsTeams());
        break;
    case RULESET_SP:
        // The campaign has no frag counter, no spectators and no chase cam --
        // baseq2 installs single_statusbar alone here and so does this.
        break;
    case RULESET_COUNT:
        break;
    }
}

// The installed bar, kept so that G_Statusbar() hands out the same bytes the
// engine holds rather than recomposing and risking a difference.
static statusbar_t  g_installed;

const char *G_Statusbar(void)
{
    return g_installed.data;
}

const char *G_StatusbarVariant(bool alt, bool team)
{
    static statusbar_t sb;

    sb_init(&sb);
    sb_universal(&sb, &sb_shape_tourney);
    sb_tourney_tail(&sb, alt, team);
    return sb.data;
}

void G_SetStatusbar(void)
{
    statusbar_t sb;

    sb_compose(&sb, G_Ruleset());

    // The invariant the suppression above exists to keep, checked rather than
    // assumed: a bar with an unbalanced `endif` draws a HUD with a hole in it
    // and nothing else reports that.
    if (sb_unbalanced(&sb)) {
        gi.dprintf("Colosseum: composed statusbar for ruleset '%s' is not "
                   "if/endif balanced -- an emitter suppressed a condition "
                   "without its body (R-OSP-7a)\n",
                   G_RulesetName(G_Ruleset()));
    }

    if (sb.overflow) {
        // Never silently: a bar that lost an item draws a HUD with a hole in
        // it, and the hole is the only symptom.
        gi.dprintf("Colosseum: statusbar for ruleset '%s' exceeded %d bytes; "
                   "items were dropped (R-MENU-5)\n",
                   G_RulesetName(G_Ruleset()), MAX_STATUSBAR);
    }

    g_installed = sb;
    gi.configstring(CS_STATUSBAR, sb.data);
}

// ---------------------------------------------------------------- diagnostic

void G_Svcmd_Slots_f(void)
{
    statusbar_t sb;
    ruleset_t   r = G_Ruleset();
    int         n = 0, top = 0;

    // The reachable range is read off stat_ceiling() rather than recomputed
    // from game.csr, so this line cannot claim a range resolution did not use.
    // It said `0..63` whenever extensions were on, which is a lie on an
    // old-API build (R-ENG-1a) -- and `sv slots` exists precisely because a
    // slot number that only lives inside the library cannot be checked from
    // outside it, so a wrong figure here is worse than no figure.  The api
    // version is printed for the same reason: since R-ENG-1a it is the other
    // half of the answer, and `extensions on` alone no longer implies 0..63.
    //
    // `(extensions <on|off>,` STAYS THE HEAD OF THE PARENTHESIS.  This line is
    // read from outside by the play-test harness (R-VER-27), which anchors on
    // exactly that prefix; putting the new field first parsed as "extensions
    // off" on a server that had them on, and the battery then failed the
    // extensions check while passing the slot check it contradicts.  A
    // diagnostic with a reader has a format, so the field is appended.
    gi.cprintf(NULL, PRINT_HIGH, "ruleset      %s   (extensions %s, api %d, so "
               "slots 0..%d are reachable)\n", G_RulesetName(r),
               game.csr.extended ? "on" : "off", GAME_API_VERSION,
               stat_ceiling() - 1);

    for (int i = 0; i < SID_COUNT; i++) {
        static const char *const kindname[] = { "num", "pic", "cs " };
        if (g_slot[i] < 0)
            continue;
        n++;
        if (g_slot[i] > top)
            top = g_slot[i];
        gi.cprintf(NULL, PRINT_HIGH, "  slot %-2d  %s  %s\n",
                   g_slot[i], kindname[slotdefs[i].kind], slotnames[i]);
    }

    // Anything the map declares for this ruleset but that resolution dropped --
    // the extension-only rows on a server without extensions, and any row a
    // collision disabled.  Reported rather than omitted: a silently absent
    // stat is the failure mode R-OSP-7a is about.
    for (int i = 0; i < SID_COUNT; i++) {
        if (g_slot[i] >= 0 || slotdefs[i].slot[r] < 0)
            continue;
        gi.cprintf(NULL, PRINT_HIGH, "  (dropped)  %s -- map says %d\n",
                   slotnames[i], slotdefs[i].slot[r]);
    }

    sb_compose(&sb, r);

    gi.cprintf(NULL, PRINT_HIGH,
               "mapped       %d stat(s), highest slot %d\n"
               "statusbar    %d of %d bytes%s\n%s\n",
               n, top, sb.len, MAX_STATUSBAR,
               sb.overflow ? " -- OVERFLOWED, items dropped" : "", sb.data);
}

/*
=================
G_Svcmd_Extras_f      `sv extras`, R-VER-33

R-EXTRA-1..7 are seven features behind seven `#define`s in the 1999 module and
seven cvars here, and six of the seven are invisible from outside the library:
a log that is open, a lag pool that is empty, two entity classnames that a
shipped map never uses, a visible weapon that looks like a skin, and an
observer implementation chosen per ruleset.  `sv ruleset` exists for exactly
this reason on the dispatch (R-VER-18) and `sv slots` on the stat map
(R-VER-19); this is the same answer for the extras.

Every line is a MEASUREMENT rather than a restatement of the cvar: the log line
says whether the file is open and what it is called, the trigger lines say
whether the classname is in the spawn table, the vwep line counts the itemlist
rows that actually carry a weapon model.
=================
*/
void G_Svcmd_Extras_f(void)
{
    int i, weapmodels = 0, lagged = 0;
    const char *observer;

    for (i = 0; i < game.num_items; i++)
        if (itemlist[i].weapmodel)
            weapmodels++;

    for (i = 1; i <= game.maxclients; i++) {
        edict_t *e = g_edicts + i;
        int lag, variance, delay, queued;

        if (!e->inuse || !e->client)
            continue;
        Lag_ClientState(e, &lag, &variance, &delay, &queued);
        if (lag || variance || queued)
            lagged++;
    }

    if (G_IsOspRuleset()) {
        observer = "osp: osp_observe.c + p_camera.c";
    } else switch (G_Ruleset()) {
    case RULESET_ARENA:
        observer = "arena: OMODE_NORMAL/FREEFLYING/TRACKCAM/EYECAM in arena.c";
        break;
    default:
        observer = g_observer->value
                   ? "ctf/sp: p_observer.c autocam/chasecam + g_chase.c"
                   : "ctf/sp: g_chase.c chasecam only (g_observer 0)";
        break;
    }

    gi.cprintf(NULL, PRINT_HIGH, "extras: ruleset %s\n",
               G_RulesetName(G_Ruleset()));
    gi.cprintf(NULL, PRINT_HIGH, "  R-EXTRA-1 gamelog       g_gamelog \"%s\" %s%s%s writes %d\n",
               g_gamelog->string,
               Log_IsOpen() ? "open" : "closed",
               Log_IsOpen() ? " " : "",
               Log_IsOpen() ? Log_Path() : "",
               Log_Writes());
    gi.cprintf(NULL, PRINT_HIGH, "  R-EXTRA-2 clientlag     g_clientlag %d pool %d lagged %d\n",
               (int)g_clientlag->value, Lag_PoolBlocks(), lagged);
    gi.cprintf(NULL, PRINT_HIGH, "  R-EXTRA-3 triggers      g_triggercounting %d trigger_counting %s, "
               "g_triggerlog %d trigger_log %s\n",
               (int)g_triggercounting->value,
               G_SpawnFuncExists("trigger_counting") ? "registered" : "MISSING",
               (int)g_triggerlog->value,
               G_SpawnFuncExists("trigger_log") ? "registered" : "MISSING");
    gi.cprintf(NULL, PRINT_HIGH, "  R-EXTRA-4 rotatingbutton g_rotatingbutton %d func_button_rotating %s\n",
               (int)g_rotatingbutton->value,
               G_SpawnFuncExists("func_button_rotating") ? "registered" : "MISSING");
    // R-EXTRA-5 has no cvar and says why: VWep is not a Gladiator patch in this
    // tree, it is what id shipped from 3.20 on and what q2pro carries.  The
    // measurement is the data the feature IS -- `weapmodel` on the weapon rows,
    // which PutClientInServer turns into `s.modelindex2` and ChangeWeapon packs
    // into the top byte of `s.skinnum`.
    gi.cprintf(NULL, PRINT_HIGH, "  R-EXTRA-5 vwep          always on, %d item(s) carry a weapmodel\n",
               weapmodels);
    gi.cprintf(NULL, PRINT_HIGH, "  R-EXTRA-6 observer      %s\n", observer);
    // R-EXTRA-7 is a regression entry rather than a feature; the verdict lives
    // in doc/regression.md and this line says which one it is.
    gi.cprintf(NULL, PRINT_HIGH, "  R-EXTRA-7 ztn2dm2 plat  not isolable from any tree here -- see doc/regression.md\n");
}

/*
=================
G_Svcmd_Census_f      `sv census <classname>`, R-VER-20

R-VER-20 asks that at least one check WAIT for something and then look again --
"spawn a level, advance past a known think deadline, and assert the thing that
think was supposed to do".  CTF's techs were the first, found by counting them
two seconds late.  An item's respawn is the second and it is a better one,
because the deadline is thirty seconds rather than two and because getting there
needs somebody to pick the item up first.

Nobody has to: bots do it on their own.  So the shape of the check is a census
that can be taken twice, and what it counts is the distinction a respawn is
about -- an item in the world, an item taken and waiting on its own think, and
the frame the count was taken on so the two are comparable.

`sv ruleset` reports the whole world in one line and cannot answer this: an item
that has been picked up is still `inuse`, still counted, and still at the same
origin.  What changed is `solid`.
=================
*/
void G_Svcmd_Census_f(void)
{
    const char *want = gi.argc() > 2 ? gi.argv(2) : NULL;
    int spawned = 0, inworld = 0, waiting = 0;
    size_t len;
    int i;

    if (!want || !*want) {
        gi.cprintf(NULL, PRINT_HIGH, "usage: sv census <classname-or-prefix>\n");
        return;
    }

    // A PREFIX, not an exact name: `sv census item_` counts every item on the
    // map and `sv census weapon_` every weapon, which is what a question about
    // respawns actually wants.  Asking about one classname is the narrow case
    // and still works -- `item_health` matches only itself, because no other
    // classname begins with it.
    len = strlen(want);

    for (i = 0; i < globals.num_edicts; i++) {
        edict_t *e = &g_edicts[i];

        if (!e->inuse || !e->classname)
            continue;
        if (strncmp(e->classname, want, len))
            continue;

        spawned++;
        if (e->solid == SOLID_NOT) {
            // Taken, and on its way back if something is going to bring it.
            // `nextthink` is the deadline; without one it is gone for good,
            // which is what DF_NO_ITEMS and arena's item sweep leave behind.
            if (e->nextthink)
                waiting++;
        } else {
            inworld++;
        }
    }

    gi.cprintf(NULL, PRINT_HIGH,
               "census %s: %d spawned, %d in world, %d waiting, frame %d\n",
               want, spawned, inworld, waiting, level.framenum);
}
