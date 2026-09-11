// ospclock -- is OSP Tourney's match clock on screen during a running match?
//
// WHY THIS SCENARIO EXISTS.  A HUD widget that is missing has four independent
// places it can be missing FROM, and reading any one of them proves nothing
// about the other three:
//
//	the BAR        `sb_tourney_tail` draws the "Time" label and the clock
//	               inside `if <SID_OSP_MATCHSTATE>`, so a bar composed without
//	               that block has no clock however good the data is.
//	the STAT       the stat does not hold the time.  It holds the INDEX of the
//	               configstring that does (OSP_CS(1)), and `if` shows the block
//	               only while that index is non-zero -- so a zero here hides a
//	               perfectly correct clock.
//	the STRING     OSP_updateClock writes "MM:SS" into that configstring once a
//	               second.  With `timelimit 0` it writes "  OFF" instead, which
//	               is a clock that is working and says so.
//	the CLIENT     the bar arrives split across consecutive configstrings and
//	               a reader that takes only the first gets 64 bytes of it.
//
// So all four are read, separately, off one running match, and each is its own
// row.  The point is that a failure NAMES THE LAYER: "the stat is zero" and
// "the bar has no clock block" are different defects with different fixes, and
// a scenario that only asserted "the player can see a clock" would report
// either as the other.
//
// Both signs, twice over.  The clock is read during WARMUP as well as during
// the match, because the two are written by different arms of OSP_updateClock
// (the countdown counts DOWN to sync_startframe; the match counts down the
// timelimit) and a build that lost one keeps the other.
//
// AND THE MATCH START'S SILENCE, which is the same match seen through a fifth
// channel.  OSP begins a match by killing every player into a fresh spawn, and
// the donor puts `if (sync_stat != 2)` in front of BOTH of player_die's death
// sounds so the bell is not a chorus of screams (`port_osp:p_client.c:642` and
// `:672`).  That guard is an ADDED line around a call that was already there,
// which is exactly the shape no line-level or call-level sweep can see, so the
// only instrument is a client counting sounds.  Both signs again: no CHAN_VOICE
// sound from a player across the match start, and one when the same player
// suicides a moment later.
//
// TOURNEY MECHANICS THIS DEPENDS ON, from osprunes' notes:
//   - a real match needs two clients, one per team, both `ready`, then
//     match_countdown seconds -- clamped UP to 14 by osp_main.c.
//   - DF_FORCE_RESPAWN, or the match start's own kill leaves every libq2 client
//     dead for the rest of the run.
package main

import (
	"flag"
	"fmt"
	"os"
	"regexp"
	"strings"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
)

var (
	q2proded = flag.String("q2proded", "", "dedicated server binary")
	ref      = flag.String("ref", "/usr/share/games/quake2/baseq2", "baseq2 paks")
	ctfref   = flag.String("ctf", "/usr/share/games/quake2/ctf", "threewave ctf paks")
	lib      = flag.String("lib", "", "game library under test")
	dir      = flag.String("dir", "/tmp/ospclock", "scratch install dir")
	port     = flag.Int("port", 27998, "UDP port")
	mapname  = flag.String("map", "q2dm1", "map to run")
	ruleset  = flag.String("ruleset", "tdm", "OSP ruleset to run")
	tl       = flag.String("timelimit", "10", "timelimit cvar")
	keep     = flag.Bool("keep", false, "keep the server log on success")
	_        = flag.String("gladdir", "", "unused; accepted for tools/playtest.sh")
)

var failed, total, skipped int
var servers []*playtest.Server

func check(name string, ok bool, note string, a ...any) bool {
	total++
	if len(a) > 0 {
		note = fmt.Sprintf(note, a...)
	}
	tag := "[ ok ]"
	if !ok {
		tag = "[FAIL]"
		failed++
	}
	fmt.Printf("  %s %-46s %s\n", tag, name, note)
	return ok
}

func skip(name, why string) {
	total++
	skipped++
	fmt.Printf("  [skip] %-46s %s\n", name, why)
}

const (
	statHealth = 1
	statArmor  = 5

	teamA, teamB   = "Hometeam", "Visitors"
	matchCountdown = 14
	dfForceRespawn = 1 << 10
)

// clockRe matches what OSP_updateClock writes: "MM:SS", " 9:59", "  OFF",
// "Pause", " Wait", "DEATH".  The high bit is stripped by Decode first -- the
// last minute is written with every byte + 128 so the client draws it green.
var clockRe = regexp.MustCompile(`^\s*(\d+:\d\d|OFF|Pause|Wait|DEATH)\s*$`)

func main() {
	flag.Parse()
	if *q2proded == "" || *lib == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded and -lib")
		os.Exit(2)
	}
	if err := colosseum.Install(*dir, *ref, *ctfref, *lib); err != nil {
		fmt.Fprintln(os.Stderr, "install:", err)
		os.Exit(2)
	}

	srv := &playtest.Server{
		Binary: *q2proded, Dir: *dir, Game: "colosseum",
		Map: *mapname, Port: *port, MaxClients: 8,
		Cvars: map[string]string{
			"g_ruleset": *ruleset, "cheats": "1",
			"deathmatch": "1", "coop": "0", "bots": "0",
			"match_countdown": fmt.Sprint(matchCountdown),
			"flood_msgs":      "0",
			"timelimit":       *tl,
			"dmflags":         fmt.Sprint(dfForceRespawn),
		},
		LogPath: *dir + "/server.log",
	}
	if err := srv.Start(); err != nil {
		fmt.Fprintln(os.Stderr, "server:", err)
		os.Exit(2)
	}
	servers = append(servers, srv)
	time.Sleep(3 * time.Second)

	fmt.Printf("\n##### the bar the SERVER says it composed\n")
	if boom := srv.Grep(`Game Error|ERROR:`); len(boom) > 0 {
		check("boot/no game error", false, strings.TrimSpace(boom[0]))
		report()
		return
	}
	check("boot/no game error", true, "")
	// A dropped item is the failure mode this whole scenario exists for and the
	// game announces it, so read the announcement before reading the bar.
	drop := srv.Grep(`exceeded .* bytes|items were dropped|not if/endif balanced`)
	check("bar/nothing was dropped composing it", len(drop) == 0,
		"%s", firstOr(drop, "no overflow and no unbalanced endif"))

	srv.Console("sv slots")
	if _, err := srv.WaitLog(`^statusbar `, 5*time.Second); err != nil {
		check("bar/sv slots answers", false, "no `sv slots` block")
		report()
		return
	}
	time.Sleep(400 * time.Millisecond)
	sl, err := colosseum.ParseSlots(strings.Join(srv.Log(), "\n"))
	if err != nil {
		fmt.Fprintln(os.Stderr, "sv slots:", err)
		os.Exit(2)
	}
	slot, mapped := sl.ByName["SID_OSP_MATCHSTATE"]
	check("bar/SID_OSP_MATCHSTATE is in the map", mapped && slot >= 16,
		"slot %d (ruleset %s, api %d, reach %d)", slot, sl.Ruleset, sl.Api, sl.Reach())
	// THE BAR ITSELF.  `if <slot>` ... `stat_string <slot>` is the block the
	// clock lives in; without it the other three rows cannot matter.
	served := sl.Bar
	check("bar/it carries the clock block",
		strings.Contains(served, fmt.Sprintf("if %d", slot)) &&
			strings.Contains(served, fmt.Sprintf("stat_string %d", slot)),
		"%d bytes, `if %d` %v, `stat_string %d` %v", len(served),
		slot, strings.Contains(served, fmt.Sprintf("if %d", slot)),
		slot, strings.Contains(served, fmt.Sprintf("stat_string %d", slot)))
	check("bar/it carries the \"Time\" label",
		strings.Contains(served, `"Time"`), "%q", excerpt(served, "Time"))

	// ------------------------------------------------------------------ warmup
	fmt.Printf("\n##### two clients, and the clock during WARMUP\n")
	a := playtest.NewBot("clockwatch", "127.0.0.1", *port)
	if err := a.Start(30 * time.Second); err != nil {
		fmt.Fprintln(os.Stderr, "bot a:", err)
		os.Exit(2)
	}
	b := playtest.NewBot("sparring", "127.0.0.1", *port)
	if err := b.Start(30 * time.Second); err != nil {
		fmt.Fprintln(os.Stderr, "bot b:", err)
		os.Exit(2)
	}
	a.WaitFrames(10, 5*time.Second)
	b.WaitFrames(10, 5*time.Second)

	// THE CLIENT'S OWN COPY OF THE BAR, rejoined across configstrings.  Compared
	// byte for byte with what the server said it composed: a bar truncated on
	// the way out is a defect only this comparison can see.
	got := playtest.Decode(a.StatusBar())
	check("bar/the client received the same bytes", got == served,
		"client %d bytes, server %d bytes", len(got), len(served))

	joinedA, joinedB := joinTeam(a, teamA), joinTeam(b, teamB)
	check("match/both joined a team", joinedA && joinedB,
		"%q | %q", lastTeamLine(a), lastTeamLine(b))

	a.Cmd("ready")
	b.Cmd("ready")

	// During the COUNTDOWN the other arm of OSP_updateClock runs, counting down
	// to sync_startframe.  Polled, because the stat and the string are written
	// on different frames.
	warmSlot, warmStr := pollClock(a, slot, 20*time.Second)
	check("warmup/the stat points at a configstring", warmSlot != 0,
		"stats[%d]=%d", slot, warmSlot)
	check("warmup/the configstring holds a clock", clockRe.MatchString(warmStr),
		"cs[%d]=%q", warmSlot, warmStr)

	// ------------------------------------------------------------------- match
	fmt.Printf("\n##### and the clock during the RUNNING match\n")
	live := false
	deadline := time.Now().Add(time.Duration(matchCountdown+25) * time.Second)
	for time.Now().Before(deadline) && !live {
		a.Cmd("give Body Armor")
		time.Sleep(800 * time.Millisecond)
		live = a.Stat(statArmor) > 0
	}
	check("match/client is alive", a.Stat(statHealth) > 0,
		"stats[%d]=%d (STAT_HEALTH)", statHealth, a.Stat(statHealth))
	check("match/the match is running (sync_stat >= 4)", live,
		"stats[%d]=%d after ready + up to %ds", statArmor, a.Stat(statArmor),
		matchCountdown+25)
	if !live {
		for _, n := range []string{
			"match/the stat still points at a configstring",
			"match/the configstring still holds a clock",
			"match/the clock is counting",
			"match/it survives a death",
		} {
			skip(n, "the match never started")
		}
		report()
		return
	}

	matchSlot, matchStr := pollClock(a, slot, 15*time.Second)
	check("match/the stat still points at a configstring", matchSlot != 0,
		"stats[%d]=%d", slot, matchSlot)
	check("match/the configstring still holds a clock", clockRe.MatchString(matchStr),
		"cs[%d]=%q", matchSlot, matchStr)

	// COUNTING, not merely present.  A clock frozen at its first value is a
	// clock that stopped being written, which reads identically to a working one
	// in a single sample.  Skipped rather than failed with `timelimit 0`, where
	// "  OFF" is the correct and constant answer.
	if *tl == "0" {
		skip("match/the clock is counting", `timelimit 0, so "  OFF" is correct and constant`)
	} else {
		first := matchStr
		moved := false
		for i := 0; i < 40 && !moved; i++ {
			time.Sleep(500 * time.Millisecond)
			if s := csAt(a, matchSlot); s != first && clockRe.MatchString(s) {
				moved = true
				matchStr = s
			}
		}
		check("match/the clock is counting", moved, "%q -> %q", first, matchStr)
	}

	// ------------------------------------------------------------ the silence
	//
	// Counted across the whole run so far, which spans the match start: the
	// only CHAN_VOICE noise two idle clients can make is the death scream the
	// guard suppresses.  Zero is the assertion; the row below gives it its
	// opposite sign so a client that simply never hears sounds cannot pass it.
	fmt.Printf("\n##### the match start is silent, and an ordinary death is not\n")
	quiet := a.VoiceSounds() + b.VoiceSounds()

	// AND IT SURVIVES A DEATH, which is the state that clears the HUD panels:
	// p_view.c blanks them while `osp_r2dc` is set, and PutClientInServer is
	// what points them at their configstrings again.  A clock that goes away on
	// the first death and never comes back is the shape this row is here for.
	died := colosseum.KillAndConfirm(a, func() bool { return a.Stat(statHealth) > 0 })
	// Reported, because the two rows below are about what a death does and a
	// refused `kill` would make both of them vacuous.
	check("match/the suicide actually happened", died,
		"the server confirmed a death for %s (last prints: %s)", a.Name,
		lastPrints(a, 4))

	deathSlot, deathStr := pollClock(a, slot, 15*time.Second)
	check("match/it survives a death", deathSlot != 0 && clockRe.MatchString(deathStr),
		"stats[%d]=%d, cs=%q", slot, deathSlot, deathStr)

	// THE OTHER SIGN.  The same client, the same function, one `kill` into a
	// running match -- `sync_stat` is 4 now, so the guard is open and the
	// scream is heard.  Without this row a build that had lost the sound
	// entirely would pass the silence row above, which would be the wrong
	// defect reported as the right fix.
	// CALIBRATION FIRST, AND THE SILENCE ROW HANGS OFF IT.
	//
	// "No death scream at the match start" is satisfied by a guard that works
	// AND by a client that cannot hear a death at all, and those are opposite
	// facts.  So the ordinary death is the calibration: it is the same
	// function, the same client and the same channel with `sync_stat` at 4
	// instead of 2, and only if IT is audible does the silence above mean
	// anything.  When it is not, both rows skip and say so -- a row that cannot
	// fail is worse than no row.
	loud := a.VoiceSounds() + b.VoiceSounds()
	audible := died && loud > quiet
	if !died {
		skip("silence/an ordinary death is audible at all",
			"the suicide was refused, so there was no death to hear")
	} else {
		check("silence/an ordinary death is audible at all", audible,
			"%d CHAN_VOICE sound(s) after the suicide, was %d (%s)",
			loud, quiet, soundSummary(a, b))
	}
	if audible {
		check("silence/no death scream at the match start", quiet == 0,
			"%d CHAN_VOICE sound(s) from a player across the countdown and the bell",
			quiet)
	} else {
		skip("silence/no death scream at the match start",
			"no player death sound reaches this client even for an ordinary "+
				"mid-match death, so silence here proves nothing about the guard")
	}

	report()
}

// pollClock waits until the stat carries a configstring index AND that
// configstring reads as a clock, and reports the last thing it saw either way.
// Polled because the two are written on different frames and by different code.
func pollClock(b *playtest.Bot, slot int, timeout time.Duration) (int, string) {
	deadline := time.Now().Add(timeout)
	var n int
	var s string
	for time.Now().Before(deadline) {
		n = b.Stat(slot)
		if n != 0 {
			s = csAt(b, n)
			if clockRe.MatchString(s) {
				return n, s
			}
		}
		time.Sleep(100 * time.Millisecond)
	}
	return n, s
}

// csAt reads a configstring and strips the high bit, which OSP_updateClock sets
// on every byte during the last minute so the client draws the clock green.
func csAt(b *playtest.Bot, n int) string {
	return playtest.Decode(b.ConfigString(n))
}

// soundSummary is the diagnostic half of the two rows above: without it a
// harness that receives NO sounds at all and a game that correctly plays none
// are the same observation, and only one of them is evidence.
func soundSummary(bots ...*playtest.Bot) string {
	byChan := map[int]int{}
	for _, b := range bots {
		for _, s := range b.Sounds() {
			byChan[s.Channel]++
		}
	}
	if len(byChan) == 0 {
		return "no sounds on any channel"
	}
	var parts []string
	for c := 0; c < 16; c++ {
		if n := byChan[c]; n > 0 {
			parts = append(parts, fmt.Sprintf("chan%d=%d", c, n))
		}
	}
	var raw []string
	for _, b := range bots {
		for _, s := range b.Sounds() {
			raw = append(raw, fmt.Sprintf("%d/e%d/c%d", s.Index, s.Entity, s.Channel))
		}
	}
	return strings.Join(parts, " ") + " [" + strings.Join(raw, ",") + "]"
}

func lastPrints(b *playtest.Bot, n int) string {
	p := b.Prints()
	if len(p) > n {
		p = p[len(p)-n:]
	}
	for i := range p {
		p[i] = strings.TrimSpace(p[i])
	}
	return strings.Join(p, " | ")
}

func excerpt(s, around string) string {
	i := strings.Index(s, around)
	if i < 0 {
		return "<not in the bar>"
	}
	lo, hi := i-24, i+24
	if lo < 0 {
		lo = 0
	}
	if hi > len(s) {
		hi = len(s)
	}
	return s[lo:hi]
}

func firstOr(v []string, alt string) string {
	if len(v) == 0 {
		return alt
	}
	return strings.TrimSpace(v[0])
}

func joinTeam(b *playtest.Bot, name string) bool {
	for try := 0; try < 5; try++ {
		b.Cmd("team %s", name)
		time.Sleep(600 * time.Millisecond)
		if onTeam(b, name) {
			return true
		}
	}
	return false
}

func onTeam(b *playtest.Bot, name string) bool {
	before := len(b.Prints())
	b.Cmd("team")
	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) {
		p := b.Prints()
		for i := len(p) - 1; i >= before; i-- {
			if strings.Contains(p[i], `team "`+name+`"`) {
				return true
			}
		}
		time.Sleep(20 * time.Millisecond)
	}
	return false
}

func lastTeamLine(b *playtest.Bot) string {
	p := b.Prints()
	for i := len(p) - 1; i >= 0; i-- {
		if strings.Contains(p[i], "team") {
			return strings.TrimSpace(p[i])
		}
	}
	return "<nothing about a team>"
}

func report() {
	for _, s := range servers {
		s.Stop()
	}
	fmt.Printf("\n%d check(s), %d failed, %d skipped\n", total, failed, skipped)
	if failed > 0 {
		fmt.Println("  server log:", *dir+"/server.log")
		os.Exit(1)
	}
	if !*keep {
		os.Remove(*dir + "/server.log")
	}
}
