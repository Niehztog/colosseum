// ctfidview -- can a CTF server hold a client at all, and does the id-view
// name string land inside the configstring layout the server negotiated?
//
// Threewave keeps a per-player name string for its "id" view (the name drawn
// under the crosshair) and addresses it as CS_GENERAL + playernum.  In q2pro
// CS_GENERAL is not a constant: meson defines USE_PROTOCOL_EXTENSIONS
// unconditionally, so the compile-time macro is the EXTENDED base (13118),
// while the base actually in force is whichever remap InitGame() chose --
// game.csr, old (1568) unless g_protocol_extensions is set.  Writing the
// extended index into an old-layout server is past svs.csr.end, and
// PF_configstring answers that with ERR_DROP, so the server dies on the first
// client's ClientUserinfoChanged.  The client half is just as sharp: the
// statusbar draws the id view with `stat_string`, which ERR_DROPs a client
// whose stat is >= cl.csr.end.
//
// Both halves need a real client to show up at all, which is why a source read
// does not settle it and a compile does not either.
//
// Four checks, all under the DEFAULT configuration (extensions off -- which is
// also the only one a vanilla-protocol client can connect under):
//
//  1. two clients connect, join opposite teams and are live players.  Before
//     the fix the server is gone before the first one finishes connecting.
//  2. the server console carries no "bad index" and no ERR_DROP.
//  3. each client's netname is at general + playernum, where the general base
//     is calibrated from where CTFAssignSkin's own playerskins string landed.
//     Consecutive indices for playernum 0 and 1 is the half that checks the
//     arithmetic, which is the same expression CTFSetIDView() feeds to
//     STAT_CTF_ID_VIEW.
//  4. no configstring the server sent is past the end of the layout it
//     negotiated -- the invariant the bug broke, stated without naming a
//     number.
//
// Then the same defect in its other pair of sites.  CONFIG_CTF_MATCH and
// CONFIG_CTF_TEAMINFO are CS_AIRACCEL-1 and -2, the last two slots of the
// statusbar block -- 28 and 27 in the layout in force, 58 and 57 through the
// extended macro.  58 and 57 are model slots, and on q2ctf1 they are taken:
// sm_meat and arm gibs.  warn_unbalanced defaults to 1, so an ordinary public
// server reaches the teaminfo write as soon as the teams go 4-v-2 -- no
// competition mode needed.  Three more checks, run by seating six clients:
//
//  5. the warning text lands at airaccel - 2, calibrated off the world model.
//  6. STAT_CTF_TEAMINFO carries that same index -- the reading half, which the
//     statusbar feeds to stat_string.
//  7. NOTHING that already existed between the models base and the playerskins
//     base changed value while the warning was raised.  This is the half that
//     fails on a build without the fix, and it names the model it clobbered.
//
// CONFIG_CTF_MATCH is the other slot of the pair and needs a competition
// server, so it gets one of its own on port+1 -- `competition 2` puts the game
// straight into MATCH_SETUP at boot, and CTFCheckRules then writes the setup
// timer once a second.  Checks 8 and 9 are 5 and 6 again for that slot.
//
// Check 7 does not transfer, though, and the reason is worth stating: the timer
// starts writing in the first second of the map, before any client can connect,
// so no client ever sees the original value of the slot it landed on and a
// before/after diff has nothing to compare -- it reports the timer overwriting
// its own previous text.  Check 10 asserts the invariant directly instead: the
// model table contains only model names.  That needs no snapshot and does not
// have to know which index the bug picks.
//
// Exit 0 all passed, 1 a check failed, 2 the scenario could not run.
package main

import (
	"flag"
	"fmt"
	"os"
	"sort"
	"strings"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
)

// shared.h: MAX_CLIENTS is 256 in both layouts and MAX_GENERAL is MAX_CLIENTS*2,
// so general = playerskins + 256 and end = general + 512 whichever remap is in
// force.  Only the base moves, and check 3 reads that off the wire.
const (
	maxClients = 256
	maxGeneral = maxClients * 2

	// g_ctf.h: the stats the CTF statusbar feeds to stat_string for the
	// unbalanced-teams warning and the match timer.
	statCTFTeamInfo = 30
	statCTFMatch    = 28
)

type joiner struct {
	bot  *playtest.Bot
	team string
	num  int // playernum, i.e. edict index - 1
}

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "retail baseq2 paks")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "Threewave CTF paks")
	lib := flag.String("lib", "", "game library under test")
	dir := flag.String("dir", "/tmp/q2playtest/ctfidview", "scratch install dir")
	mapname := flag.String("map", "q2ctf1", "map to test on")
	port := flag.Int("port", 27983, "server port")
	label := flag.String("label", "", "label for the report")
	flag.Parse()

	if *q2 == "" || *lib == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded and -lib")
		os.Exit(2)
	}
	bad, err := run(*q2, *ref, *ctf, *lib, *dir, *mapname, *port, *label)
	if err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	if bad {
		os.Exit(1)
	}
}

func run(q2, ref, ctf, lib, dir, mapname string, port int, label string) (bool, error) {
	if err := colosseum.Install(dir, ref, ctf, lib); err != nil {
		return false, err
	}
	srv := &playtest.Server{
		Binary: q2, Dir: dir, Game: "colosseum", Map: mapname, Port: port,
		MaxClients: 12,
		// `g_ruleset` because this library serves five rulesets and picks from
		// this cvar; without it the server boots `dm`, where `team red` reaches
		// no CTF join and every client stays a deathmatcher.
		Cvars:      map[string]string{"g_ruleset": "ctf", "cheats": "1", "dmflags": "1024"},
		LogPath:    dir + "/server.log",
	}
	if err := srv.Start(); err != nil {
		return false, err
	}
	defer srv.Stop()

	if label != "" {
		fmt.Printf("== %s ==\n", label)
	}

	bad := false
	fail := func(f string, a ...any) {
		bad = true
		fmt.Printf("FAIL  "+f+"\n", a...)
	}

	// ---- check 1: two clients, opposite teams, both live ----
	// Clients take edicts 1 and 2 in connect order, so playernum is 0 and 1.
	players := []*joiner{
		{bot: playtest.NewBot("redshirt", "127.0.0.1", port), team: "red", num: 0},
		{bot: playtest.NewBot("blueshirt", "127.0.0.1", port), team: "blue", num: 1},
	}
	for _, p := range players {
		if err := p.bot.Start(30 * time.Second); err != nil {
			// This is the failure mode of the bug, so report it as a finding
			// rather than as a scenario that could not run.
			fail("%s never spawned in: %v", p.bot.Name, err)
			fmt.Println("      server console:")
			for _, l := range srv.Grep(`ERROR|bad index|Shutdown`) {
				fmt.Printf("        %s\n", l)
			}
			return true, nil
		}
		defer p.bot.Disconnect()
	}
	time.Sleep(2 * time.Second)

	for _, p := range players {
		if err := join(p); err != nil {
			fail("%v", err)
			return true, nil
		}
	}
	for _, p := range players {
		p.bot.WaitFrames(10, 10*time.Second)
	}
	live := true
	for _, p := range players {
		if p.bot.Spectating() {
			fail("%s is still PM_SPECTATOR after joining %s", p.bot.Name, p.team)
			live = false
		}
	}
	if live {
		fmt.Printf("PASS  both clients connected, joined opposite teams and are live players\n")
	}

	// ---- check 2: the server said nothing about a bad index ----
	if hits := srv.Grep(`bad index|invalid string index|ERROR`); len(hits) > 0 {
		fail("server console reports an error:")
		for _, l := range hits {
			fmt.Printf("      %s\n", l)
		}
	} else {
		fmt.Println("PASS  no bad-index / ERR_DROP on the server console")
	}

	// ---- check 3: the name string is at general + playernum ----
	// CTFAssignSkin writes "<name>\<model>/ctf_r" to playerskins + playernum,
	// so finding redshirt's skin string gives the playerskins base without
	// hardcoding either layout, and general follows from it.
	cs := players[0].bot.ConfigStrings()
	skinIdx, ok := findPrefix(cs, "redshirt\\")
	if !ok {
		return false, fmt.Errorf("redshirt's playerskins string is not in the configstrings")
	}
	playerskins := skinIdx - players[0].num
	general := playerskins + maxClients
	fmt.Printf("      layout: playerskins %d, general %d (skin string at %d = %q)\n",
		playerskins, general, skinIdx, playtest.Decode(cs[skinIdx]))

	namesOK := true
	for _, p := range players {
		want := general + p.num
		got := playtest.Decode(cs[want])
		if got != p.bot.Name {
			fail("configstring %d (general + %d) is %q, want %q -- the id-view name string is not where CTFSetIDView() will look",
				want, p.num, got, p.bot.Name)
			namesOK = false
		}
	}
	if namesOK {
		fmt.Printf("PASS  id-view names at %d and %d, consecutive, matching both netnames\n",
			general, general+1)
	}

	// ---- check 4: nothing outside the negotiated layout ----
	end := general + maxGeneral
	worst, n := 0, 0
	for k := range cs {
		if k >= end {
			n++
			if k > worst {
				worst = k
			}
		}
	}
	if n > 0 {
		fail("%d configstring(s) past the end of the negotiated layout (end %d, worst %d = %q)",
			n, end, worst, playtest.Decode(cs[worst]))
	} else {
		fmt.Printf("PASS  all %d configstrings inside the negotiated layout (end %d)\n", len(cs), end)
	}

	// ---- checks 5-7: the unbalanced-teams warning ----
	// Calibrate the airaccel base off the world model: model index 1 is always
	// the BSP, and CS_MODELS is CS_AIRACCEL + 3 in both layouts.
	bspIdx, ok := findPrefix(cs, "maps/")
	if !ok {
		return false, fmt.Errorf("the world model is not in the configstrings")
	}
	models := bspIdx - 1
	airaccel := models - 3
	teaminfo := airaccel - 2
	fmt.Printf("      layout: airaccel %d, models %d, teaminfo slot %d (world model at %d = %q)\n",
		airaccel, models, teaminfo, bspIdx, playtest.Decode(cs[bspIdx]))

	// Everything from the models base up to the playerskins base is level
	// content: models, sounds, images, lightstyles, item names.  None of it may
	// change because two teams got lopsided.  Snapshot it before provoking the
	// warning; new indices appearing is fine, existing values changing is not.
	watched := map[int]string{}
	for k, v := range cs {
		if k >= models && k < playerskins {
			watched[k] = v
		}
	}

	// Seat four more clients: three red, then one blue LAST.  The gate is
	// team1-team2 >= 2 && team2 >= 2, so 4-v-1 does not trip it and 4-v-2 does
	// -- which makes the moment the warning is raised deterministic.
	extra := []*joiner{
		{bot: playtest.NewBot("red2", "127.0.0.1", port), team: "red", num: 2},
		{bot: playtest.NewBot("red3", "127.0.0.1", port), team: "red", num: 3},
		{bot: playtest.NewBot("red4", "127.0.0.1", port), team: "red", num: 4},
		{bot: playtest.NewBot("blue2", "127.0.0.1", port), team: "blue", num: 5},
	}
	for _, p := range extra {
		// Stagger the connects.  Six clients arriving at once occasionally
		// loses one to a challenge/connect race, which is a transient and
		// looks nothing like the failure this scenario is about.
		time.Sleep(700 * time.Millisecond)
		if err := p.bot.Start(30 * time.Second); err != nil {
			time.Sleep(2 * time.Second)
			if err = p.bot.Start(30 * time.Second); err != nil {
				return false, fmt.Errorf("%s never spawned in: %w", p.bot.Name, err)
			}
		}
		defer p.bot.Disconnect()
		if err := join(p); err != nil {
			fail("%v", err)
			return true, nil
		}
	}
	// CTFCheckRules runs the warning branch once a frame; give it a few.
	players[0].bot.WaitFrames(30, 10*time.Second)
	time.Sleep(time.Second)

	after := players[0].bot.ConfigStrings()
	warnIdx, warnFound := findPrefix(after, "WARNING: Red has too many")

	// check 5
	switch {
	case !warnFound:
		fail("4-v-2 raised no unbalanced-teams warning at all -- checks 6 and 7 prove nothing")
	case warnIdx != teaminfo:
		fail("the warning is at configstring %d, want %d (airaccel - 2); it says %q",
			warnIdx, teaminfo, playtest.Decode(after[warnIdx]))
	default:
		fmt.Printf("PASS  warning at configstring %d (airaccel - 2) = %q\n",
			warnIdx, playtest.Decode(after[warnIdx]))
	}

	// check 6 -- the reading half.  g_ctf.h: STAT_CTF_TEAMINFO is 30.
	if got := players[0].bot.Stat(statCTFTeamInfo); got != teaminfo {
		fail("STAT_CTF_TEAMINFO is %d, want %d -- stat_string would draw configstring %d",
			got, teaminfo, got)
	} else {
		fmt.Printf("PASS  STAT_CTF_TEAMINFO %d matches the slot the warning was written to\n", got)
	}

	// check 7 -- the negative half.
	var clobbered []int
	for k, was := range watched {
		if now, seen := after[k]; seen && now != was {
			clobbered = append(clobbered, k)
		}
	}
	sort.Ints(clobbered)
	if len(clobbered) > 0 {
		fail("%d level configstring(s) overwritten while the warning was raised:", len(clobbered))
		for _, k := range clobbered {
			fmt.Printf("      %d was %q, now %q\n",
				k, playtest.Decode(watched[k]), playtest.Decode(after[k]))
		}
	} else {
		fmt.Printf("PASS  all %d level configstrings (%d..%d) unchanged\n",
			len(watched), models, playerskins-1)
	}

	// ---- checks 8-10: the match timer, on a competition server ----
	if err := matchTimer(q2, ref, ctf, lib, dir+"-match", mapname, port+1, fail); err != nil {
		return false, err
	}
	return bad, nil
}

// matchTimer boots a second server in competition mode and checks the other
// slot of the pair.  A separate server because ctfgame.match is latched at map
// load from the competition cvar, so it cannot be reached from the first one.
func matchTimer(q2, ref, ctf, lib, dir, mapname string, port int, fail func(string, ...any)) error {
	if err := colosseum.Install(dir, ref, ctf, lib); err != nil {
		return err
	}
	srv := &playtest.Server{
		Binary: q2, Dir: dir, Game: "colosseum", Map: mapname, Port: port,
		MaxClients: 8,
		// competition 2 -> MATCH_SETUP at boot; matchsetuptime defaults to 10
		// minutes, which is far longer than this scenario needs.
		// `g_ruleset` for the same reason as the main server above; without it
		// this control booted `dm`, where `competition` means nothing and
		// `team red` reaches no CTF join at all.
		Cvars:   map[string]string{"g_ruleset": "ctf", "cheats": "1",
			"dmflags": "1024", "competition": "2"},
		LogPath: dir + "/server.log",
	}
	if err := srv.Start(); err != nil {
		return err
	}
	defer srv.Stop()

	p := &joiner{bot: playtest.NewBot("matchref", "127.0.0.1", port), team: "red", num: 0}
	if err := p.bot.Start(30 * time.Second); err != nil {
		return fmt.Errorf("%s never spawned in: %w", p.bot.Name, err)
	}
	defer p.bot.Disconnect()
	time.Sleep(2 * time.Second)
	// SetCTFStats only runs for a live player -- a spectator goes through
	// G_SetSpectatorStats, which never touches STAT_CTF_MATCH.
	if err := join(p); err != nil {
		fail("%v", err)
		return nil
	}
	p.bot.WaitFrames(20, 10*time.Second)

	before := p.bot.ConfigStrings()
	bspIdx, ok := findPrefix(before, "maps/")
	if !ok {
		return fmt.Errorf("the world model is not in the configstrings")
	}
	models := bspIdx - 1
	airaccel := models - 3
	slot := airaccel - 1
	fmt.Printf("      competition 2: airaccel %d, match slot %d\n", airaccel, slot)

	// The setup timer is rewritten every whole second, so two is plenty.
	time.Sleep(3 * time.Second)
	after := p.bot.ConfigStrings()
	idx, found := findPrefix(after, "SETUP:")
	if !found {
		// with competition < 3 the string is "MM:SS SETUP: N not ready"
		idx, found = findSubstring(after, "SETUP:")
	}

	// check 8
	switch {
	case !found:
		fail("competition 2 produced no MATCH_SETUP string -- checks 9 and 10 prove nothing")
	case idx != slot:
		fail("the match timer is at configstring %d, want %d (airaccel - 1); it says %q",
			idx, slot, playtest.Decode(after[idx]))
	default:
		fmt.Printf("PASS  match timer at configstring %d (airaccel - 1) = %q\n",
			idx, playtest.Decode(after[idx]))
	}

	// check 9 -- the reading half
	if got := p.bot.Stat(statCTFMatch); got != slot {
		fail("STAT_CTF_MATCH is %d, want %d -- stat_string would draw configstring %d",
			got, slot, got)
	} else {
		fmt.Printf("PASS  STAT_CTF_MATCH %d matches the slot the timer was written to\n", got)
	}

	// check 10 -- the negative half, an invariant rather than a diff.  The
	// models block is filled contiguously from index 1, so walk it to the first
	// gap and insist every entry still looks like a model name.  Prose written
	// into it -- "09:47 SETUP: 1 not ready" -- fails that.
	var prose []int
	last := models
	for k := models + 1; ; k++ {
		v, seen := after[k]
		if !seen {
			break
		}
		last = k
		if !looksLikeModel(playtest.Decode(v)) {
			prose = append(prose, k)
		}
	}
	sort.Ints(prose)
	if len(prose) > 0 {
		fail("%d entr(ies) in the model table are not model names:", len(prose))
		for _, k := range prose {
			fmt.Printf("      %d = %q\n", k, playtest.Decode(after[k]))
		}
	} else {
		fmt.Printf("PASS  model table %d..%d holds only model names (%d entries)\n",
			models+1, last, last-models)
	}
	return nil
}

// looksLikeModel is true for everything the server puts in the models block --
// the BSP, its inline models, md2/sp2 paths and the "#name.md2" view-model
// icons -- and false for a sentence.
func looksLikeModel(v string) bool {
	return strings.HasPrefix(v, "maps/") || strings.HasPrefix(v, "*") ||
		strings.HasPrefix(v, "#") || strings.Contains(v, "/")
}

// findSubstring is findPrefix for a string the mod does not write at offset 0 --
// the setup timer is "MM:SS SETUP: N not ready".
func findSubstring(cs map[int]string, want string) (int, bool) {
	best, found := 0, false
	for k, v := range cs {
		if strings.Contains(playtest.Decode(v), want) && (!found || k < best) {
			best, found = k, true
		}
	}
	return best, found
}

// join sends "team <side>" until the server confirms it.  One command can land
// in the same frame as ClientBegin and be dropped; a join that is refused is
// refused every time, so this retries a transient rather than a result.
func join(p *joiner) error {
	for i := 0; i < 5; i++ {
		p.bot.Cmd("team " + p.team)
		if _, err := p.bot.WaitPrint(`(?i)joined the `+p.team+` team`, 4*time.Second); err == nil {
			return nil
		}
	}
	return fmt.Errorf("%s never joined the %s team", p.bot.Name, p.team)
}

// findPrefix returns the LOWEST index whose value starts with prefix.  Map
// iteration order is random in Go, so picking "a match" rather than "the first
// match" makes a scenario that passes and fails at random.
func findPrefix(cs map[int]string, prefix string) (int, bool) {
	best, found := 0, false
	for k, v := range cs {
		if strings.HasPrefix(playtest.Decode(v), prefix) && (!found || k < best) {
			best, found = k, true
		}
	}
	return best, found
}
