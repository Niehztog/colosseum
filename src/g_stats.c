// Colosseum per-ruleset stat slot map and composed statusbar -- R-OSP-7,
// R-OSP-7a.  The map itself is in g_stats.h; this is the resolution, the
// accessors and the emitter.

#include "g_local.h"

// ---------------------------------------------------------------- the map

typedef struct {
    statkind_t  kind;
    int8_t      slot[RULESET_COUNT];
} slotdef_t;

#define STATSLOT_ROW(id, kind, dm, ctf, arena, tourney, sp) \
    [id] = { kind, { [RULESET_DM] = dm, [RULESET_CTF] = ctf, \
                     [RULESET_ARENA] = arena, [RULESET_TOURNEY] = tourney, \
                     [RULESET_SP] = sp } },

static const slotdef_t slotdefs[SID_COUNT] = {
    STATSLOT_MAP(STATSLOT_ROW)
};

#undef STATSLOT_ROW

#define STATSLOT_NAME(id, kind, dm, ctf, arena, tourney, sp)    #id,

static const char *const slotnames[SID_COUNT] = {
    STATSLOT_MAP(STATSLOT_NAME)
};

#undef STATSLOT_NAME

// Resolved once for the active ruleset.  A lookup rather than a map walk
// because G_SetStat runs per client per frame.
static int8_t g_slot[SID_COUNT];

// ---------------------------------------------------------------- resolution

void G_InitStats(void)
{
    ruleset_t r = G_Ruleset();
    int used[MAX_STATS];

    memset(used, -1, sizeof(used));

    for (int i = 0; i < SID_COUNT; i++) {
        int slot = slotdefs[i].slot[r];

        // R-OSP-7 clause 6, and it is the reason CTF's timer pair may sit at
        // 32/33 at all: 32..63 exist only for a client that negotiated the
        // protocol extension, so without one the stat is dropped -- from the
        // bar and from the write together, because both go through this array.
        if (slot >= MAX_STATS_OLD && !game.csr.extended)
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
static void sb_universal(statusbar_t *sb)
{
    sb_layout(sb, "yb", -24);

    // health
    sb_layout(sb, "xv", 0);
    sb_raw(sb, "hnum");
    sb_layout(sb, "xv", 50);
    sb_upic(sb, STAT_HEALTH_ICON);

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
    sb_layout(sb, "xv", 262);
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

// baseq2's dm tail: the spectator banner and the chase-cam name.
static void sb_dm_tail(statusbar_t *sb)
{
    sb_frags(sb);

    sb_if(sb, SID_SPECTATOR);
    sb_layout(sb, "xv", 0);
    sb_layout(sb, "yb", -58);
    sb_raw(sb, "string2 \"SPECTATOR MODE\"");
    sb_endif(sb);

    sb_if(sb, SID_CHASE);
    sb_layout(sb, "xv", 0);
    sb_layout(sb, "yb", -68);
    sb_raw(sb, "string \"Chasing\"");
    sb_layout(sb, "xv", 64);
    sb_stat_string(sb, SID_CHASE);
    sb_endif(sb);
}

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

void G_SetStatusbar(void)
{
    statusbar_t sb;

    sb_init(&sb);
    sb_universal(&sb);

    switch (G_Ruleset()) {
    case RULESET_CTF:
        sb_ctf_tail(&sb);
        break;
    case RULESET_SP:
        // The campaign has no frag counter, no spectators and no chase cam --
        // baseq2 installs single_statusbar alone here and so does this.
        break;
    default:
        sb_dm_tail(&sb);
        break;
    }

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

    gi.configstring(CS_STATUSBAR, sb.data);
}

// ---------------------------------------------------------------- diagnostic

void G_Svcmd_Slots_f(void)
{
    statusbar_t sb;
    ruleset_t   r = G_Ruleset();
    int         n = 0, top = 0;

    gi.cprintf(NULL, PRINT_HIGH, "ruleset      %s   (extensions %s, so slots "
               "0..%d are reachable)\n", G_RulesetName(r),
               game.csr.extended ? "on" : "off",
               (game.csr.extended ? MAX_STATS_NEW : MAX_STATS_OLD) - 1);

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

    sb_init(&sb);
    sb_universal(&sb);
    switch (r) {
    case RULESET_CTF:
        sb_ctf_tail(&sb);
        break;
    case RULESET_SP:
        break;
    default:
        sb_dm_tail(&sb);
        break;
    }

    gi.cprintf(NULL, PRINT_HIGH,
               "mapped       %d stat(s), highest slot %d\n"
               "statusbar    %d of %d bytes%s\n%s\n",
               n, top, sb.len, MAX_STATUSBAR,
               sb.overflow ? " -- OVERFLOWED, items dropped" : "", sb.data);
}
