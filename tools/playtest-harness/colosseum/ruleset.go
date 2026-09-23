// Package colosseum knows the things that are true of a Colosseum server and
// of no other Quake II mod: one game library serves five rulesets, chosen by
// the `g_ruleset` cvar, and it will tell you which one it is on.
//
// A single-ruleset mod can be tested by booting it and looking at the result.
// A dispatching one cannot: booting `dm` and finding dm behaviour proves
// nothing, because the arena code might simply not have run yet.  Every check
// here is therefore a DIFFERENCE -- the thing one ruleset does that another
// must not -- and the two server commands below are what makes those
// differences readable from outside the process.
package colosseum

import (
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"runtime"
	"strconv"
	"strings"
)

// The seven rulesets, and a map each one can actually hold a match on.
//
// dm, dmpro, tdm and duel are OSP Tourney DM's four modes of play, which spec
// 1.36 promoted from `match_mode` values inside a `tourney` ruleset to
// g_ruleset values of their own.  They share one code path, so a
// check that passes under all four proves only that the path runs -- each row
// has to assert what its ruleset does and the other three do not.
type Ruleset struct {
	Name string
	Map  string
	// Cvars beyond g_ruleset that this ruleset needs to be itself.
	Cvars map[string]string
}

var All = []Ruleset{
	{"dm", "q2dm1", map[string]string{"deathmatch": "1", "coop": "0"}},
	{"dmpro", "q2dm1", map[string]string{"deathmatch": "1", "coop": "0"}},
	{"tdm", "q2dm1", map[string]string{"deathmatch": "1", "coop": "0"}},
	{"duel", "q2dm1", map[string]string{"deathmatch": "1", "coop": "0"}},
	{"ctf", "q2ctf1", map[string]string{"deathmatch": "1", "coop": "0"}},
	{"arena", "q2dm1", map[string]string{"deathmatch": "1", "coop": "0"}},
	{"sp", "base1", map[string]string{"deathmatch": "0", "coop": "0"}},
}

// IsOSP reports whether a ruleset runs OSP Tourney DM's code path -- the four
// that share one stat numbering and one composed statusbar.
func IsOSP(name string) bool {
	switch name {
	case "dm", "dmpro", "tdm", "duel":
		return true
	}
	return false
}

// IsTeams reports whether a ruleset's clients belong to NAMED teams, which is
// what decides how a client enters the game: `join` alone in a free-for-all,
// `join <team name>` where there are teams (OSP_teamjoin_cmd matches its
// argument against the team's netname, so a number is not a team).
func IsTeams(name string) bool {
	switch name {
	case "tdm", "duel":
		return true
	}
	return false
}

// Find returns the named ruleset.
//
// `sp`'s map honours $SPMAP, the same override tools/bootmatrix.sh and
// tools/smoke.sh read.  base1 is retail pak0's; id's free demo carries the same
// level as demo1, and CI -- which has only the demo and the 3.20 point release
// to hand -- runs on that.  The multiplayer maps need no such knob: q2dm1 is in
// the point release's pak1 and q2ctf1 in its ctf/pak0.
func Find(name string) (Ruleset, bool) {
	for _, r := range All {
		if r.Name == name {
			if name == "sp" {
				if m := os.Getenv("SPMAP"); m != "" {
					r.Map = m
				}
			}
			return r, true
		}
	}
	return Ruleset{}, false
}

// State is `sv ruleset` parsed: what the library says it resolved to, and what
// is actually in the world because of it.
type State struct {
	Ruleset            string
	Xatrix, Rogue      bool
	Teamplay, Hook     bool
	Runes, Bots        bool
	Monsters, Campaign bool
	Saves              bool
	Deathmatch, Coop   int
	Frame              int
	Edicts, Clients    int
	LiveMonsters       int
	Corpses, Gibs      int
	Complaints         []string // the "  !! ..." lines, which are defects by construction
	CTF                string   // the ctf line, empty off ctf
	Raw                string
}

var (
	reRuleset    = regexp.MustCompile(`(?m)^ruleset\s+(\S+)`)
	reLayers     = regexp.MustCompile(`(?m)^layers\s+xatrix=(\d+)\s+rogue=(\d+)`)
	reModifiers  = regexp.MustCompile(`(?m)^modifiers\s+teamplay=(\d+)\s+hook=(\d+)\s+runes=(\d+)\s+bots=(\d+)`)
	rePredicates = regexp.MustCompile(`(?m)^predicates\s+monsters=(\d+)\s+campaign=(\d+)\s+teamplay=(\d+)\s+bots=(\d+)\s+saves=(\d+)`)
	reLegacy     = regexp.MustCompile(`(?m)^legacy\s+deathmatch=(\d+)\s+coop=(\d+)`)
	reWorld      = regexp.MustCompile(`(?m)^world\s+frame (\d+), (\d+) edicts in use, (\d+) clients, (\d+) live monsters, (\d+) corpses, (\d+) gibs`)
	reCTF        = regexp.MustCompile(`(?m)^ctf\s+(.*)$`)
	reComplaint  = regexp.MustCompile(`(?m)^\s*!!\s*(.*)$`)
)

// ParseRuleset reads the block `sv ruleset` prints.  Give it the whole server
// log; it reads the LAST block, so a scenario can ask more than once.
func ParseRuleset(log string) (State, error) {
	i := strings.LastIndex(log, "ruleset      ")
	if i < 0 {
		return State{}, fmt.Errorf("no `sv ruleset` output in the log")
	}
	s := log[i:]
	st := State{Raw: s}

	if m := reRuleset.FindStringSubmatch(s); m != nil {
		st.Ruleset = m[1]
	} else {
		return st, fmt.Errorf("`sv ruleset` output did not start with a ruleset name")
	}
	if m := reLayers.FindStringSubmatch(s); m != nil {
		st.Xatrix, st.Rogue = m[1] == "1", m[2] == "1"
	}
	if m := reModifiers.FindStringSubmatch(s); m != nil {
		st.Teamplay, st.Hook = m[1] == "1", m[2] == "1"
		st.Runes, st.Bots = m[3] == "1", m[4] == "1"
	}
	if m := rePredicates.FindStringSubmatch(s); m != nil {
		st.Monsters, st.Campaign = m[1] == "1", m[2] == "1"
		st.Saves = m[5] == "1"
	}
	if m := reLegacy.FindStringSubmatch(s); m != nil {
		st.Deathmatch, _ = strconv.Atoi(m[1])
		st.Coop, _ = strconv.Atoi(m[2])
	}
	if m := reWorld.FindStringSubmatch(s); m != nil {
		st.Frame, _ = strconv.Atoi(m[1])
		st.Edicts, _ = strconv.Atoi(m[2])
		st.Clients, _ = strconv.Atoi(m[3])
		st.LiveMonsters, _ = strconv.Atoi(m[4])
		st.Corpses, _ = strconv.Atoi(m[5])
		st.Gibs, _ = strconv.Atoi(m[6])
	}
	if m := reCTF.FindStringSubmatch(s); m != nil {
		st.CTF = strings.TrimSpace(m[1])
	}
	for _, m := range reComplaint.FindAllStringSubmatch(s, -1) {
		st.Complaints = append(st.Complaints, strings.TrimSpace(m[1]))
	}
	return st, nil
}

// Slots is `sv slots` parsed: the stat-slot map the library resolved for this
// ruleset, and the statusbar it composed out of it.
type Slots struct {
	Ruleset  string
	Extended bool
	// Api is the game ABI the library was built against: 3302 for Q2PRO's
	// (GAME_API_VERSION_NEW, 64 stat slots) or 3 for the classic id one
	// (GAME_API_VERSION_OLD, 32).  Zero if the server predates the field.
	//
	// It matters because it is HALF of how many slots exist.  `Extended` is the
	// other half, and neither implies the other: the two are independent build
	// and runtime switches, so a client can negotiate 64 slots from a library
	// whose array holds 32.  Anything asserting on a slot at or above 32 has to
	// consult both -- see Reach.
	Api      int
	ByName   map[string]int    // stat name -> slot
	Kind     map[string]string // stat name -> num|pic|cs
	Dropped  map[string]int    // stat name -> the slot the map asked for
	Mapped   int
	Top      int
	Bar      string
	BarLen   int
	BarMax   int
	Overflow bool
}

// Reach is how many stat slots this server can actually deliver: the lower of
// what the library's array holds and what the wire carries.
//
// Use this rather than `Extended` alone when asserting that an extension-only
// row resolved.  A check that reads "extensions are on, therefore slot 33 is
// live" is true only on the new ABI, and it fails on a correct old-ABI build --
// which is a real supported configuration, not a broken one.
func (s Slots) Reach() int {
	if s.Api == 3 { // GAME_API_VERSION_OLD: player_state_t holds 32, full stop
		return 32
	}
	if s.Extended {
		return 64
	}
	return 32
}

var (
	// `(extensions <on|off>,` is the anchor, and the api field comes after it on
	// purpose: this regex is why.  Colosseum appends to this line rather than
	// inserting, because a field inserted ahead of the anchor parses as
	// "extensions off" on a server that had them on -- silently, since the zero
	// value of a bool is a plausible answer.
	reSlotHead = regexp.MustCompile(`(?m)^ruleset\s+(\S+)\s+\(extensions (on|off)(?:,\s*api (\d+))?`)
	reSlot     = regexp.MustCompile(`(?m)^\s*slot (\d+)\s+(num|pic|cs )\s+(\S+)`)
	reDropped  = regexp.MustCompile(`(?m)^\s*\(dropped\)\s+(\S+) -- map says (-?\d+)`)
	reMapped   = regexp.MustCompile(`(?m)^mapped\s+(\d+) stat\(s\), highest slot (\d+)`)
	reBar      = regexp.MustCompile(`(?m)^statusbar\s+(\d+) of (\d+) bytes(.*)$`)
)

// ParseSlots reads the LAST `sv slots` block out of a server log.
func ParseSlots(log string) (Slots, error) {
	i := strings.LastIndex(log, "mapped       ")
	if i < 0 {
		return Slots{}, fmt.Errorf("no `sv slots` output in the log")
	}
	// walk back to that block's own header
	j := strings.LastIndex(log[:i], "ruleset      ")
	if j < 0 {
		return Slots{}, fmt.Errorf("`sv slots` output had no header")
	}
	s := log[j:]
	sl := Slots{ByName: map[string]int{}, Kind: map[string]string{}, Dropped: map[string]int{}}

	if m := reSlotHead.FindStringSubmatch(s); m != nil {
		sl.Ruleset, sl.Extended = m[1], m[2] == "on"
		if m[3] != "" {
			sl.Api, _ = strconv.Atoi(m[3])
		}
	}
	for _, m := range reSlot.FindAllStringSubmatch(s, -1) {
		n, _ := strconv.Atoi(m[1])
		sl.ByName[m[3]] = n
		sl.Kind[m[3]] = strings.TrimSpace(m[2])
	}
	for _, m := range reDropped.FindAllStringSubmatch(s, -1) {
		n, _ := strconv.Atoi(m[2])
		sl.Dropped[m[1]] = n
	}
	if m := reMapped.FindStringSubmatch(s); m != nil {
		sl.Mapped, _ = strconv.Atoi(m[1])
		sl.Top, _ = strconv.Atoi(m[2])
	}
	if m := reBar.FindStringSubmatch(s); m != nil {
		sl.BarLen, _ = strconv.Atoi(m[1])
		sl.BarMax, _ = strconv.Atoi(m[2])
		sl.Overflow = strings.Contains(m[3], "OVERFLOW")
		// the bar itself is everything after that line
		if k := strings.Index(s[strings.Index(s, m[0]):], "\n"); k >= 0 {
			rest := s[strings.Index(s, m[0])+k+1:]
			sl.Bar = rest
		}
	}
	return sl, nil
}

// PlayerSkinBase is the first player configstring.  Which number that is says
// which configstring layout the server negotiated, and a mod that writes a
// compile-time CS_ constant instead of the runtime one lands on the wrong side
// of this: the server kills the connection with "bad index".
func PlayerSkinBase(extended bool) int {
	if extended {
		return 12862
	}
	return 1312
}

// A team skin is "name\model/skin" where the skin half is the mod's, not the
// player's.  Reading it is how the userinfo half is checked without a
// human looking at a screen.
func SkinOf(cs string) string {
	if i := strings.Index(cs, "\\"); i >= 0 {
		return cs[i+1:]
	}
	return ""
}

// Install lays out a Colosseum install for the harness.
//
// Colosseum is not one mod dir's worth of content: it serves baseq2 maps AND
// the CTF ones from a single gamedir, and both distributions call their archive
// pak0.pak.  So the CTF paks are linked in under a name that sorts after
// baseq2's, which is also the load order Quake II gives them.  Everything lands
// in the gamedir rather than baseq2 because the game library must be found in
// homedir/<game>, never in basedir (SPECS.md 1.5).
func Install(dir, baseq2, ctf, lib string) error {
	game := filepath.Join(dir, "colosseum")
	for _, d := range []string{game, filepath.Join(dir, "baseq2")} {
		if err := os.MkdirAll(d, 0o755); err != nil {
			return err
		}
	}
	link := func(src, dst string) error {
		abs, err := filepath.Abs(src)
		if err != nil {
			return err
		}
		os.Remove(dst)
		return os.Symlink(abs, dst)
	}
	paks, err := filepath.Glob(filepath.Join(baseq2, "pak*.pak"))
	if err != nil {
		return err
	}
	if len(paks) == 0 {
		return fmt.Errorf("no pak files in %s", baseq2)
	}
	for _, p := range paks {
		if err := link(p, filepath.Join(game, filepath.Base(p))); err != nil {
			return err
		}
	}
	if ctf != "" {
		cpaks, _ := filepath.Glob(filepath.Join(ctf, "pak*.pak"))
		for i, p := range cpaks {
			// pak8, pak9... -- after every baseq2 pak, so CTF's own
			// player skins win where the two archives overlap
			if err := link(p, filepath.Join(game, fmt.Sprintf("pak%d.pak", 8+i))); err != nil {
				return err
			}
		}
		// loose files a CTF distribution ships next to the pak
		loose, _ := filepath.Glob(filepath.Join(ctf, "*.bsp"))
		if len(loose) > 0 {
			maps := filepath.Join(game, "maps")
			if err := os.MkdirAll(maps, 0o755); err != nil {
				return err
			}
			for _, p := range loose {
				if err := link(p, filepath.Join(maps, filepath.Base(p))); err != nil {
					return err
				}
			}
		}
	}
	if lib != "" {
		return link(lib, filepath.Join(game, "game"+hostArch()+".so"))
	}
	return nil
}

// hostArch is q2pro's CPU tag in the game library's file name.
func hostArch() string {
	switch runtime.GOARCH {
	case "amd64":
		return "x86_64"
	case "386":
		return "i386"
	case "arm64":
		return "arm64"
	default:
		return runtime.GOARCH
	}
}

// Stats is what each ruleset's slot map must contain, and what it must not.
//
// The Forbid half is the point.  One library holds every ruleset's stat rows,
// so "ctf maps CTF_TEAM1_PIC" proves only that the table has a ctf column --
// it says nothing about whether the dispatch is live.  "dm does NOT map it"
// is the half that fails when a gate goes missing.
var Stats = map[string]struct {
	Want   []string
	Forbid []string
}{
	// `sp` is the only ruleset left on baseq2's own numbering.  `dm` shared this
	// entry until spec 1.36 moved it onto OSP's column, and the pair being
	// identical was exactly what made the move invisible from here.
	"sp": {
		[]string{"SID_CHASE", "SID_SPECTATOR", "SID_TIMER2"},
		[]string{"SID_CTF_TEAM1_PIC", "SID_RA_ARENASTATUS", "SID_OSP_MATCHSTATE"},
	},
	"ctf": {
		[]string{"SID_CTF_TEAM1_PIC", "SID_CTF_TEAM2_PIC", "SID_CTF_TECH",
			"SID_CTF_JOINED_TEAM1_PIC", "SID_CTF_MATCH", "SID_CTF_TEAMINFO"},
		[]string{"SID_RA_ARENASTATUS", "SID_OSP_MATCHSTATE", "SID_SPECTATOR"},
	},
	"arena": {
		[]string{"SID_RA_ARENASTATUS", "SID_RA_ROUNDINFO", "SID_RA_COUNTDOWN",
			"SID_RA_QUEUE1", "SID_RA_LINEPOSITION"},
		[]string{"SID_CTF_TEAM1_PIC", "SID_OSP_MATCHSTATE", "SID_SPECTATOR"},
	},
	// One entry per OSP ruleset: they share the `osp` column of STATSLOT_MAP,
	// so all four want the same slots -- and `dm` wanting them is the check
	// that `dm` really did move onto OSP's numbering rather than keeping
	// baseq2's, whose column spec 1.36 deleted.
	"dm": {
		[]string{"SID_OSP_MATCHSTATE", "SID_OSP_STATUS1", "SID_OSP_LAYOUT1",
			"SID_OSP_RUNE_HASTE", "SID_CHASE"},
		[]string{"SID_CTF_TEAM1_PIC", "SID_RA_ARENASTATUS", "SID_SPECTATOR"},
	},
	"dmpro": {
		[]string{"SID_OSP_MATCHSTATE", "SID_OSP_STATUS1", "SID_OSP_LAYOUT1",
			"SID_OSP_RUNE_HASTE", "SID_CHASE"},
		[]string{"SID_CTF_TEAM1_PIC", "SID_RA_ARENASTATUS", "SID_SPECTATOR"},
	},
	"tdm": {
		[]string{"SID_OSP_MATCHSTATE", "SID_OSP_STATUS1", "SID_OSP_LAYOUT1",
			"SID_OSP_RUNE_HASTE", "SID_CHASE"},
		[]string{"SID_CTF_TEAM1_PIC", "SID_RA_ARENASTATUS", "SID_SPECTATOR"},
	},
	"duel": {
		[]string{"SID_OSP_MATCHSTATE", "SID_OSP_STATUS1", "SID_OSP_LAYOUT1",
			"SID_OSP_RUNE_HASTE", "SID_CHASE"},
		[]string{"SID_CTF_TEAM1_PIC", "SID_RA_ARENASTATUS", "SID_SPECTATOR"},
	},
}

// DroppedWithoutExtensions: ctf's numbering runs to 30, so the shared
// second powerup timer has nowhere below MAX_STATS_OLD to live and goes to
// 32/33.  On a server without protocol extensions those two rows resolve to
// nothing and `sv slots` says so -- which is the designed outcome, not a
// defect, and the reason the check is a named expectation rather than "no row
// may ever be dropped".  Turn g_protocol_extensions on and the list is empty.
var DroppedWithoutExtensions = map[string][]string{
	"ctf": {"SID_TIMER2", "SID_TIMER2_ICON"},
}

// Missing returns the names in want that the slot map does not have.
func (s Slots) Missing(want []string) []string {
	var out []string
	for _, n := range want {
		if _, ok := s.ByName[n]; !ok {
			out = append(out, n)
		}
	}
	return out
}

// Leaked returns the names in forbid that the slot map does have.
func (s Slots) Leaked(forbid []string) []string {
	var out []string
	for _, n := range forbid {
		if _, ok := s.ByName[n]; ok {
			out = append(out, n)
		}
	}
	return out
}

// ---------------------------------------------------------------- bots
//
// Colosseum 1.23 adds two lines to `sv ruleset`, and they are the only way to
// see from outside the library WHERE a bot ended up: on which CTF team, in
// which arena, counted by which match.  `sv clientdump` says which slots hold
// bots and which library each uses, which is the loader's question; this is the
// ruleset one.
//
//	bots         4 bot(s) of 4 client(s) in 20 slot(s), at 16,17,18,19
//	botplace     ctf red=2 blue=2 noteam=0, FL_BOTCLIENT=4
type BotCensus struct {
	Bots, Clients, MaxClients int
	Slots                     []int
	// Place is the botplace line with its leading keyword removed, kept whole
	// so a check can assert a substring and print what it actually got.
	Place string
	// Fields is the same line as key=value pairs, for the numeric assertions.
	Fields map[string]int
}

var (
	reBots     = regexp.MustCompile(`(?m)^bots +(\d+) bot\(s\) of (\d+) client\(s\) in (\d+) slot\(s\), at (\S+)`)
	reBotPlace = regexp.MustCompile(`(?m)^botplace +(.*)$`)
	// Hyphens are allowed in the key: the arena line reports `in-arena` and
	// `on-team`, and a regex that stopped at the hyphen captured `arena` and
	// `team` instead -- two fields that read 4 and 4 while the check asked for
	// `in-arena` and got the zero value.  Both assertions failed against a
	// line that said exactly what they wanted.
	reKV = regexp.MustCompile(`([A-Za-z_][A-Za-z0-9_-]*)=(-?\d+)`)
)

// ParseBots reads the last bots/botplace pair out of a server log.  The LAST,
// because a scenario asks `sv ruleset` more than once and only the most recent
// answer describes the state it has just arranged.
func ParseBots(log string) (BotCensus, error) {
	var b BotCensus
	ms := reBots.FindAllStringSubmatch(log, -1)
	if len(ms) == 0 {
		return b, fmt.Errorf("no `bots` line in the log")
	}
	m := ms[len(ms)-1]
	b.Bots, _ = strconv.Atoi(m[1])
	b.Clients, _ = strconv.Atoi(m[2])
	b.MaxClients, _ = strconv.Atoi(m[3])
	if m[4] != "-" {
		for _, s := range strings.Split(m[4], ",") {
			if n, err := strconv.Atoi(s); err == nil {
				b.Slots = append(b.Slots, n)
			}
		}
	}
	ps := reBotPlace.FindAllStringSubmatch(log, -1)
	if len(ps) > 0 {
		b.Place = strings.TrimSpace(ps[len(ps)-1][1])
	}
	b.Fields = map[string]int{}
	for _, kv := range reKV.FindAllStringSubmatch(b.Place, -1) {
		n, _ := strconv.Atoi(kv[2])
		b.Fields[kv[1]] = n
	}
	return b, nil
}

// InstallBrain adds everything a bot needs on top of Install: the botlib the
// roster names, the Gladiator asset pak that holds the character files, a
// writable bots.cfg and one .aas per map.
//
// The botlib reads bots.cfg and the character files through ITS OWN search --
// <basedir>/<gamedir> then <cddir>/<gamedir> -- not through the engine's, which
// is why bots.cfg is copied into the gamedir rather than linked from wherever
// the engine happens to look.
func InstallBrain(dir, gladdir string) error {
	game := filepath.Join(dir, "colosseum")
	link := func(src, dst string) error {
		abs, err := filepath.Abs(src)
		if err != nil {
			return err
		}
		os.Remove(dst)
		return os.Symlink(abs, dst)
	}
	so := filepath.Join(gladdir, "release", "gladiator.so")
	if _, err := os.Stat(so); err != nil {
		return fmt.Errorf("no brain at %s: %w", so, err)
	}
	if err := link(so, filepath.Join(game, "gladiator.so")); err != nil {
		return err
	}
	if err := link(filepath.Join(gladdir, "assets", "pak7.pak"),
		filepath.Join(game, "pak7.pak")); err != nil {
		return err
	}
	cfgdir := filepath.Join(game, "botcfg")
	if err := os.MkdirAll(cfgdir, 0o755); err != nil {
		return err
	}
	src, err := os.ReadFile(filepath.Join(gladdir, "assets", "bots.cfg"))
	if err != nil {
		return err
	}
	if err := os.WriteFile(filepath.Join(cfgdir, "bots.cfg"), src, 0o644); err != nil {
		return err
	}
	maps := filepath.Join(game, "maps")
	if err := os.MkdirAll(maps, 0o755); err != nil {
		return err
	}
	aas, _ := filepath.Glob(filepath.Join(gladdir, "assets", "maps", "*.aas"))
	for _, a := range aas {
		if strings.HasSuffix(a, ".original_baseline") {
			continue
		}
		if err := link(a, filepath.Join(maps, filepath.Base(a))); err != nil {
			return err
		}
	}
	return nil
}
