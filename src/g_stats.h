// Colosseum per-ruleset stat slot map and composed statusbar -- R-OSP-7,
// R-OSP-7a.
//
// THE PROBLEM, measured.  A statusbar is a program in a configstring and it
// addresses player_state_t.stats[] by NUMBER: `pic 17`, `num 2 19`,
// `stat_string 27`.  Every donor invented its own numbering above slot 15 and
// they collide almost completely (SPECS.md R-OSP-7's table).  CTF is the case
// that forces the issue: slots 17, 18 and 19 each carry two assigned names --
// STAT_SPECTATOR/STAT_CTF_TEAM1_PIC, STAT_TIMER2_ICON/STAT_CTF_TEAM1_CAPS,
// STAT_TIMER2/STAT_CTF_TEAM2_PIC -- and CTF already uses 0..30 of
// MAX_STATS_OLD's 32, so there is no free pair to move the second powerup timer
// to.  Upstream resolved it by gating the writes on `!ctf->value`, which works
// only because the bar is a static string chosen once.
//
// THE SHAPE HERE.  A slot is not a #define.  It is a *logical id* (statslot_t)
// that resolves through the active ruleset's row of the map below, and the
// statusbar is EMITTED from that same map rather than stored as a literal
// (R-OSP-7a).  So a slot number exists in exactly one place, the bar and the
// code that writes it cannot disagree, and the shared second-powerup-timer
// mechanic stops needing one #define per ruleset.
//
// The map is one table, in this header, per R-OSP-7 clause 3 -- "18 upward is
// ruleset-private, one numbering per ruleset, declared in one header, including
// dm's own".  It is an X-macro so that the C tables and tools/slotkind.py read
// the same text; a map a tool cannot parse is a map nothing checks.

#ifndef G_STATS_H
#define G_STATS_H

// What a slot HOLDS.  R-OSP-7 clause 7: the check is on kind as well as number,
// because writing an image index where the bar draws a configstring is a type
// error across a boundary with no compiler on it, and a name-collision test
// cannot see it when the bar uses a bare number.  That is how RA2's slot 20 got
// through.  Here the kind travels *with* the number, and the emitter API is
// kind-typed, so the two cannot drift.
typedef enum {
    SK_NUM,             // a plain number       -- `num W N`, `if N`
    SK_PIC,             // an image index       -- `pic N`
    SK_CS,              // a configstring index -- `stat_string N`
} statkind_t;

// The map.  One row per logical stat, one column per ruleset, -1 for "this
// ruleset does not have this stat at all".
//
// Slots 0..15 are universal and unclaimable (clause 1) and keep their shared.h
// names; they are not in this table.  16 and 17 are claimable only by a ruleset
// that does not use baseq2's STAT_CHASE/STAT_SPECTATOR semantics (clause 2) --
// Threewave qualifies, having no baseq2 spectator flag at all (R-CTF-5).
//
// A half-wired column -- a map that renumbers a slot while the bar still says
// 18 -- would be worse than inheriting, which is why each ruleset's column and
// its bar landed in the same commit.
//
// TOURNEY's timer pair goes to 29/30, where R-OSP-7's table put it, its five
// runes to 22..26 and the popup-menu layout to 27.  It keeps 16 as the chase/ID
// name -- the same meaning and the same SK_CS kind baseq2 gives STAT_CHASE, so
// there is nothing to claim -- but it DOES claim 17, which baseq2 uses as the
// spectator flag and tourney draws with `stat_string`.  Clause 2 permits that
// for a ruleset that does not use baseq2's spectator semantics, and tourney
// has its own observer and camera (R-EXTRA-6).
//
// 18..21 are four more status lines the donor addresses by bare number.  Their
// names here are neutral on purpose: the reconstruction did not name them and
// inventing a meaning would be a guess, the same reasoning client_respawn_t's
// osp_rNNN fields get.
//
// ARENA claims 16 and 17, which clause 2 permits only for a ruleset that does
// not use baseq2's STAT_CHASE/STAT_SPECTATOR semantics.  RA2 qualifies the same
// way Threewave does and for a stronger reason: it ships its own four-mode
// observer inside arena.c (R-EXTRA-6), so there is no chase-cam name to draw and
// no spectator flag to test.  Its own block runs 16..25 and the shared second
// powerup timer therefore goes to 26/27, which is where R-OSP-7's table put it.
//
// The kinds below are read off RA2's own statusbar literal rather than off the
// field names, because two of the names lie: STAT_QUEUE1_ICON and
// STAT_QUEUE2_ICON are drawn with `stat_string`, not `pic`, and are written as
// `game.csr.items + game.num_items + N` -- configstring indices.  That is
// exactly the confusion clause 7's kind check exists to catch.
//
// CTF's timer pair at 32/33 is the one place this table leaves MAX_STATS_OLD.
// It is legal under clause 6 *because* it is content a ruleset can drop: the
// pent countdown is a display, not a mechanic, and G_InitStats() removes any
// slot the running configuration cannot reach, so the bar and the writes
// disappear together (R-COMPAT-5).  This is the thing upstream could not do
// with a literal bar: under ctf the second powerup timer now displays at all,
// for clients that negotiated the extension.
//
// "Cannot reach" is two conditions, not one -- see g_stats.c's stat_ceiling().
// The wire carries 32..63 only to a client that negotiated the extension, and
// the ARRAY only holds them on a library built against the new game API, which
// R-ENG-1a made a build switch.  On an `API=old` build the pair is therefore
// dropped unconditionally and ctf loses the second powerup timer, exactly as
// upstream does; every other ruleset's column stays inside 32 and is unaffected.
//
//     id                        kind     dm  ctf  arena tourney  sp
#define STATSLOT_MAP(E) \
    E(SID_CHASE,                 SK_CS,   16,  -1,  -1,  16,  16) \
    E(SID_SPECTATOR,             SK_NUM,  17,  -1,  -1,  -1,  17) \
    E(SID_TIMER2_ICON,           SK_PIC,  18,  32,  26,  29,  18) \
    E(SID_TIMER2,                SK_NUM,  19,  33,  27,  30,  19) \
    E(SID_CTF_TEAM1_PIC,         SK_PIC,  -1,  17,  -1,  -1,  -1) \
    E(SID_CTF_TEAM1_CAPS,        SK_NUM,  -1,  18,  -1,  -1,  -1) \
    E(SID_CTF_TEAM2_PIC,         SK_PIC,  -1,  19,  -1,  -1,  -1) \
    E(SID_CTF_TEAM2_CAPS,        SK_NUM,  -1,  20,  -1,  -1,  -1) \
    E(SID_CTF_FLAG_PIC,          SK_PIC,  -1,  21,  -1,  -1,  -1) \
    E(SID_CTF_JOINED_TEAM1_PIC,  SK_PIC,  -1,  22,  -1,  -1,  -1) \
    E(SID_CTF_JOINED_TEAM2_PIC,  SK_PIC,  -1,  23,  -1,  -1,  -1) \
    E(SID_CTF_TEAM1_HEADER,      SK_PIC,  -1,  24,  -1,  -1,  -1) \
    E(SID_CTF_TEAM2_HEADER,      SK_PIC,  -1,  25,  -1,  -1,  -1) \
    E(SID_CTF_TECH,              SK_PIC,  -1,  26,  -1,  -1,  -1) \
    E(SID_CTF_ID_VIEW,           SK_CS,   -1,  27,  -1,  -1,  -1) \
    E(SID_CTF_MATCH,             SK_CS,   -1,  28,  -1,  -1,  -1) \
    E(SID_CTF_ID_VIEW_COLOR,     SK_PIC,  -1,  29,  -1,  -1,  -1) \
    E(SID_CTF_TEAMINFO,          SK_CS,   -1,  30,  -1,  -1,  -1) \
    E(SID_RA_COUNTDOWN,          SK_NUM,  -1,  -1,  16,  -1,  -1) \
    E(SID_RA_ARENASTATUS,        SK_CS,   -1,  -1,  17,  -1,  -1) \
    E(SID_RA_ROUNDINFO,          SK_CS,   -1,  -1,  18,  -1,  -1) \
    E(SID_RA_LINEPOSITION,       SK_NUM,  -1,  -1,  19,  -1,  -1) \
    E(SID_RA_ID_VIEW,            SK_CS,   -1,  -1,  20,  -1,  -1) \
    E(SID_RA_QUEUE1,             SK_NUM,  -1,  -1,  21,  -1,  -1) \
    E(SID_RA_QUEUE2,             SK_NUM,  -1,  -1,  22,  -1,  -1) \
    E(SID_RA_SHOWQUEUE,          SK_NUM,  -1,  -1,  23,  -1,  -1) \
    E(SID_RA_QUEUE1_ICON,        SK_CS,   -1,  -1,  24,  -1,  -1) \
    E(SID_RA_QUEUE2_ICON,        SK_CS,   -1,  -1,  25,  -1,  -1) \
    E(SID_OSP_RUNE_RESIST,       SK_NUM,  -1,  -1,  -1,  22,  -1) \
    E(SID_OSP_RUNE_STRENGTH,     SK_NUM,  -1,  -1,  -1,  23,  -1) \
    E(SID_OSP_RUNE_HASTE,        SK_NUM,  -1,  -1,  -1,  24,  -1) \
    E(SID_OSP_RUNE_REGEN,        SK_NUM,  -1,  -1,  -1,  25,  -1) \
    E(SID_OSP_RUNE_VAMPIRE,      SK_NUM,  -1,  -1,  -1,  26,  -1) \
    E(SID_OSP_LAYOUT1,           SK_CS,   -1,  -1,  -1,  27,  -1) \
    E(SID_OSP_MATCHSTATE,        SK_CS,   -1,  -1,  -1,  17,  -1) \
    E(SID_OSP_STATUS1,           SK_CS,   -1,  -1,  -1,  18,  -1) \
    E(SID_OSP_STATUS2,           SK_CS,   -1,  -1,  -1,  19,  -1) \
    E(SID_OSP_STATUS3,           SK_CS,   -1,  -1,  -1,  20,  -1) \
    E(SID_OSP_STATUS4,           SK_CS,   -1,  -1,  -1,  21,  -1) \

#define STATSLOT_ENUM(id, kind, dm, ctf, arena, tourney, sp)    id,
typedef enum {
    STATSLOT_MAP(STATSLOT_ENUM)
    SID_COUNT
} statslot_t;
#undef STATSLOT_ENUM

// ---------------------------------------------------------------- accessors

// Resolve the map for the active ruleset.  Called from G_InitRuleset(), after
// the ruleset is latched and before anything can write a stat.
void G_InitStats(void);

// The slot this logical stat occupies in the active ruleset, or -1.
int  G_Stat(statslot_t id);

// Write / read one mapped stat.  A stat the active ruleset does not have is a
// silent no-op rather than an out-of-bounds write on stats[-1], which is what
// makes it safe for shared code -- p_hud.c writes the second powerup timer in
// every ruleset and does not need to know where it landed, or whether it
// landed at all.
void G_SetStat(edict_t *ent, statslot_t id, int value);
int  G_GetStat(edict_t *ent, statslot_t id);

// ---------------------------------------------------------------- statusbar
//
// The `sb_*` emitter.  1400 is Quake II's own statusbar/layout budget and the
// number R-MENU-5 names; the engine spreads CS_STATUSBAR across configstrings
// up to csr.airaccel, so more would fit, but a bar that only fits under
// protocol extensions would break R-COMPAT-5.
#define MAX_STATUSBAR   1400

typedef struct {
    char    data[MAX_STATUSBAR];
    int     len;
    bool    overflow;       // a whole item was dropped, never half of one
    int     skip;           // depth inside a suppressed `if` block, see below
} statusbar_t;

void sb_init(statusbar_t *sb);
void sb_raw(statusbar_t *sb, const char *token);        // one complete item
void sb_layout(statusbar_t *sb, const char *op, int v); // `xv 32`, `yb -24`
void sb_endif(statusbar_t *sb);

// Mapped: the slot comes from the active ruleset's row and the kind is checked
// against the map, so `sb_pic(sb, SID_CTF_TECH)` cannot be emitted for a slot
// the map declares SK_NUM.  An unmapped id emits nothing -- which is how the
// same emitter produces a bar with a second powerup timer under dm and one
// without under ctf, from the same call sequence.
void sb_if(statusbar_t *sb, statslot_t id);
void sb_pic(statusbar_t *sb, statslot_t id);
void sb_num(statusbar_t *sb, int width, statslot_t id);
void sb_stat_string(statusbar_t *sb, statslot_t id);

// Universal: slots 0..15 of shared.h, unclaimable in every ruleset (clause 1),
// so they are written by their shared.h name and need no map.
void sb_uif(statusbar_t *sb, int slot);
void sb_upic(statusbar_t *sb, int slot);
void sb_unum(statusbar_t *sb, int width, int slot);
void sb_ustat_string(statusbar_t *sb, int slot);

// Compose the active ruleset's bar and install it.  Replaces baseq2's two
// string literals and CTF's third (R-OSP-7a).
void G_SetStatusbar(void);

// The composed bar as text.  RA2's menu engine draws its menu INTO the
// statusbar configstring, unicast per client, and has to put the real bar back
// when the menu closes -- it cannot use a literal any more, because there is no
// literal (R-OSP-7a).
const char *G_Statusbar(void);

// One tourney bar variant, composed on demand.  Its `hud` client command
// unicasts a different bar per client, which is four literals in the donor and
// two booleans here -- the case R-OSP-7a exists for.  The buffer is static and
// is overwritten by the next call, which is enough because the caller unicasts
// it immediately.
const char *G_StatusbarVariant(bool alt, bool team);

// `sv slots` -- the resolved map and the composed bar, for the same reason
// R-VER-18 gave `sv ruleset`: a slot number that only exists inside the library
// cannot be checked from outside it, and "the HUD looks right" is not evidence
// about slot 32.
void G_Svcmd_Slots_f(void);
// `sv extras` -- R-EXTRA-1..7 made observable from outside (R-VER-33).
void G_Svcmd_Extras_f(void);
// `sv census <classname>` -- R-VER-20's temporal check, twice over.
void G_Svcmd_Census_f(void);

#endif // G_STATS_H
