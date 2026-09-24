// ospimage -- what OSP Tourney's 1999 image answers, asked of the running game.
//
// osp-tourney's reconstruction is byte-identical to the 1999 Linux image at
// 1895f8e, and five of its answers are not the ones the older reconstruction
// gave (R-OSP-1).  Each row below is a DIFFERENCE, and every one of them fails
// on a library built without the change -- run it against both:
//
//	A. dm, timelimit 1.  `highscores` during play draws the table rather than
//	   the board, because only the table's ALTERNATION is intermission's.  A
//	   death is logged and drops the weapon in hand, because dm is always
//	   live.  And the match-state slot is blank at the intermission:
//	   OSP_clearStats clears it in every mode, not only the team ones.
//	B. tdm, fraglimit 20.  The table is not kept, the console says so, and
//	   `client_highscores` is written to 0 -- the donor turns it off by writing
//	   the cvar.  And a WARMUP death is not logged and drops nothing, which is
//	   dm's row turned inside out.
//	C. duel.  The board skips an empty seat: the player left behind still has
//	   a card after the seat-0 player has gone.
//	D. dm.  An autocam observer's menu is flushed.  OSP_clientThink answers for
//	   the autocam observer's whole frame, so a flush at the tail of
//	   ClientThink never ran for it, and a second cursor press inside a second
//	   waited for the 32-frame repaint -- up to 3.2 s.  The flush lives in
//	   ClientEndServerFrame now, which every client passes once a frame.
//	E. ctf.  Threewave's menu engine shares that flush, so the same second
//	   press is drawn within a second there too.  E passes on both libraries:
//	   nothing skipped ClientThink's tail for a ctf client.  It is the guard
//	   that moving the flush did not take it away from the other engine.
//
// A death is confirmed by its obituary, not by the health stat: under the OSP
// four `kill` respawns the player in the same call, as the donor's Cmd_Kill_f
// does, so a suicide is never seen dead on the wire.  That is also why A reads
// the match-state slot at the intermission rather than on a dead player's
// board.
//
// D and E are timing rows on a slow host, so each asks three times and judges
// the worst: a repaint that happened to land early is one lucky sample, not
// three.
package main

import (
	"flag"
	"fmt"
	"os"
	"regexp"
	"strconv"
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
	dir      = flag.String("dir", "/tmp/ospimage", "scratch install dir")
	port     = flag.Int("port", 27996, "UDP port")
	mapname  = flag.String("map", "q2dm1", "map to run")
	keep     = flag.Bool("keep", false, "keep the server log on success")
)

var failed, total, skipped int

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
	fmt.Printf("  %s %-52s %s\n", tag, name, note)
	return ok
}

func skip(name, why string) {
	total++
	skipped++
	fmt.Printf("  [skip] %-52s %s\n", name, why)
}

func report() {
	fmt.Printf("\n%d check(s), %d failed, %d skipped\n", total, failed, skipped)
	if failed > 0 {
		fmt.Printf("server logs under %s\n", *dir)
		os.Exit(1)
	}
	if !*keep {
		os.RemoveAll(*dir)
	}
}

const (
	statHealth     = 1
	teamA, teamB   = "Hometeam", "Visitors"
	dfForceRespawn = 1 << 10
)

// boot starts one server for one phase.  Every phase gets its own, because
// g_ruleset is latched and the rows compare rulesets.
func boot(ruleset, mapName, stats string, extra map[string]string) *playtest.Server {
	cvars := map[string]string{
		"g_ruleset":         ruleset,
		"deathmatch":        "1",
		"coop":              "0",
		"bots":              "0",
		"flood_msgs":        "0",
		"timelimit":         "0",
		"fraglimit":         "20",
		"client_highscores": "1",
		"statsfile":         "1",
		"statsname":         stats,
	}
	for k, v := range extra {
		cvars[k] = v
	}
	srv := &playtest.Server{
		Binary: *q2proded, Dir: *dir, Game: "colosseum",
		Map: mapName, Port: *port, MaxClients: 8,
		Cvars:   cvars,
		LogPath: *dir + "/server-" + strings.TrimSuffix(stats, ".jsonl") + ".log",
	}
	if err := srv.Start(); err != nil {
		fmt.Fprintln(os.Stderr, "server:", err)
		os.Exit(2)
	}
	time.Sleep(3 * time.Second)
	if boom := srv.Grep(`Game Error|ERROR:`); len(boom) > 0 {
		check(ruleset+"/boot: no game error", false, strings.TrimSpace(boom[0]))
		srv.Stop()
		report()
		os.Exit(1)
	}
	return srv
}

func connect(name string) *playtest.Bot {
	b := playtest.NewBot(name, "127.0.0.1", *port)
	if err := b.Start(30 * time.Second); err != nil {
		fmt.Fprintf(os.Stderr, "client %s: %v\n", name, err)
		os.Exit(2)
	}
	b.WaitFrames(10, 5*time.Second)
	return b
}

// enterFFA is `join` until the playerstate says the client is not observing.
func enterFFA(b *playtest.Bot) bool {
	for i := 0; i < 6 && b.Spectating(); i++ {
		b.Cmd("join")
		time.Sleep(1500 * time.Millisecond)
	}
	return !b.Spectating()
}

func joinTeam(b *playtest.Bot, name string) bool {
	for try := 0; try < 5; try++ {
		b.Cmd("team %s", name)
		time.Sleep(700 * time.Millisecond)
		if onTeam(b, name) {
			return true
		}
	}
	return false
}

func onTeam(b *playtest.Bot, name string) bool {
	before := len(b.Prints())
	b.Cmd("team")
	deadline := time.Now().Add(3 * time.Second)
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

var cvarRe = regexp.MustCompile(`"([^"]+)" is "([^"]*)"`)

// cvar asks the console for one cvar and returns its value.
func cvar(srv *playtest.Server, name string) string {
	before := len(srv.Log())
	srv.Console(name)
	deadline := time.Now().Add(3 * time.Second)
	for time.Now().Before(deadline) {
		lines := srv.Log()
		for i := len(lines) - 1; i >= before && i >= 0; i-- {
			m := cvarRe.FindStringSubmatch(lines[i])
			if m != nil && m[1] == name {
				return m[2]
			}
		}
		time.Sleep(20 * time.Millisecond)
	}
	return "<no answer>"
}

var deathRe = regexp.MustCompile(`"event":"(suicide|kill)"`)

// deaths counts the death records the stats log holds for one player name.
// The log is one JSON object per line, and every player record carries
// "name"; a death names the victim there.
func deaths(path, name string) (int, int, error) {
	raw, err := os.ReadFile(path)
	if err != nil {
		return 0, 0, err
	}
	n, lines := 0, 0
	for _, l := range strings.Split(string(raw), "\n") {
		if strings.TrimSpace(l) == "" {
			continue
		}
		lines++
		if deathRe.MatchString(l) && strings.Contains(l, `"`+name+`"`) {
			n++
		}
	}
	return n, lines, nil
}

// slotOf reads one stat's slot out of `sv slots`, by name.
func slotOf(srv *playtest.Server, name string) (int, bool) {
	srv.Console("sv slots")
	if _, err := srv.WaitLog(`^statusbar `, 5*time.Second); err != nil {
		return 0, false
	}
	time.Sleep(400 * time.Millisecond)
	sl, err := colosseum.ParseSlots(strings.Join(srv.Log(), "\n"))
	if err != nil {
		return 0, false
	}
	n, ok := sl.ByName[name]
	return n, ok
}

var reEdicts = regexp.MustCompile(`(\d+) edicts in use, (\d+) clients`)

// props is `sv ruleset`'s edict census with the clients taken out.  A dropped
// weapon is a NEW edict that lives 30 seconds; a body comes out of the
// pre-allocated body queue and does not move the count (ospwarmup's witness).
func props(srv *playtest.Server) int {
	mark := len(srv.Log())
	srv.Console("sv ruleset")
	srv.WaitLog(`edicts in use`, 5*time.Second)
	time.Sleep(400 * time.Millisecond)
	lines := srv.Log()
	for i := mark; i < len(lines); i++ {
		if m := reEdicts.FindStringSubmatch(lines[i]); m != nil {
			n, _ := strconv.Atoi(m[1])
			c, _ := strconv.Atoi(m[2])
			return n - c
		}
	}
	return -1
}

var obitRe = regexp.MustCompile(`(?i)(killed (it|him|her)self|suicides)`)

// suicide sends `kill` until the obituary for it arrives.  Cmd_Kill_f refuses
// a suicide for five seconds after a spawn and says nothing, so it is retried
// across that window; the obituary is the confirmation, because under the OSP
// four the same call respawns the player and the health stat never reads 0.
func suicide(b *playtest.Bot, name string) bool {
	re := regexp.MustCompile(regexp.QuoteMeta(name) + ` `)
	deadline := time.Now().Add(9 * time.Second)
	for time.Now().Before(deadline) {
		before := len(b.Prints())
		b.Cmd("kill")
		for wait := time.Now().Add(time.Second); time.Now().Before(wait); {
			p := b.Prints()
			for i := before; i < len(p); i++ {
				if re.MatchString(p[i]) && obitRe.MatchString(p[i]) {
					return true
				}
			}
			time.Sleep(20 * time.Millisecond)
		}
	}
	return false
}

// ------------------------------------------------------------------- phases

func phaseDM() {
	fmt.Printf("\n##### A. dm, timelimit 1\n")
	stats := *dir + "/colosseum/ospimage-dm.jsonl"
	os.Remove(stats)
	srv := boot("dm", *mapname, "ospimage-dm.jsonl",
		map[string]string{"cheats": "1", "timelimit": "1", "fraglimit": "0"})
	defer srv.Stop()

	_, err := srv.WaitLog(`Client high scoring enabled!`, 2*time.Second)
	check("dm/the table is kept (console)", err == nil,
		"`Client high scoring enabled!` -- the positive arm of B1")

	a := connect("alpha")
	defer a.Disconnect()
	if !check("dm/setup: alpha entered the game", enterFFA(a), "") {
		return
	}

	// `highscores` clears osp_r034 and redraws; the table is drawn whenever
	// osp_r034 is clear.  The command also CLOSES a board that is already up,
	// which is why a press that drew nothing is sent again -- no number of
	// presses makes a build that gates the table on the intermission draw it.
	got := false
	for try := 0; try < 3 && !got; try++ {
		a.Cmd("highscores")
		if _, err := a.WaitLayout(`High scores \(`, 2*time.Second); err == nil {
			got = true
		}
	}
	check("dm/`highscores` during play draws the table", got,
		"layout carries `High scores (%s)`", *mapname)

	// dm is always live (sync_stat 8), so a death is logged and drops the
	// weapon in hand -- given here, because a dm spawn holds a blaster and
	// TossClientWeapon never drops that.
	a.Cmd("give Super Shotgun")
	a.Cmd("give Shells")
	time.Sleep(500 * time.Millisecond)
	a.Cmd("use Super Shotgun")
	time.Sleep(1500 * time.Millisecond)
	litter := props(srv)
	before, _, _ := deaths(stats, "alpha")
	if !check("dm/setup: `kill` killed alpha", suicide(a, "alpha"), "an obituary for alpha") {
		return
	}
	time.Sleep(1500 * time.Millisecond)
	if now := props(srv); litter < 0 || now < 0 {
		skip("dm/a death drops the weapon in hand", "no `sv ruleset` census")
	} else {
		check("dm/a death drops the weapon in hand", now > litter,
			"%d non-client edicts, %d before the death", now, litter)
	}
	after, lines, err := deaths(stats, "alpha")
	if err != nil {
		check("dm/a death is logged", false, "no stats log: %v", err)
	} else {
		check("dm/a death is logged", after > before,
			"%d death record(s) for alpha, %d line(s) in the log", after, lines)
	}

	// The match-state line is set while the level runs, and the intermission
	// runs OSP_clearStats every frame.  One minute of timelimit ends the
	// level by itself.
	slot, ok := slotOf(srv, "SID_OSP_MATCHSTATE")
	if !ok {
		skip("dm/match-state slot blank at the intermission", "no SID_OSP_MATCHSTATE in `sv slots`")
		return
	}
	playing := a.Stat(slot)
	if !check("dm/setup: match-state slot set during play", playing != 0,
		"slot %d = %d", slot, playing) {
		return
	}
	if _, err := srv.WaitLog(`Timelimit hit`, 90*time.Second); err != nil {
		check("dm/setup: the timelimit ends the level", false, "no `Timelimit hit` in 90 s")
		return
	}
	v, werr := a.WaitStat(slot, 0, 3*time.Second)
	check("dm/match-state slot blank at the intermission", werr == nil && v == 0,
		"slot %d = %d at the intermission (was %d in play)", slot, a.Stat(slot), playing)
}

func phaseTDM() {
	fmt.Printf("\n##### B. tdm, fraglimit 20\n")
	stats := *dir + "/colosseum/ospimage-tdm.jsonl"
	os.Remove(stats)
	srv := boot("tdm", *mapname, "ospimage-tdm.jsonl", nil)
	defer srv.Stop()

	_, err := srv.WaitLog(`High score tracking disabled!`, 2*time.Second)
	enabled := len(srv.Grep(`Client high scoring enabled!`)) > 0
	check("tdm/the table is not kept (console)", err == nil && !enabled,
		"disabled printed: %v, enabled printed: %v", err == nil, enabled)
	v := cvar(srv, "client_highscores")
	check("tdm/client_highscores is written to 0", v == "0", "reads %q", v)

	// A warmup death: nobody has readied, so sync_stat is below 3.
	b := connect("bravo")
	defer b.Disconnect()
	if !check("tdm/setup: bravo joined "+teamA, joinTeam(b, teamA), "") {
		return
	}
	time.Sleep(1500 * time.Millisecond)
	_, lines, err := deaths(stats, "bravo")
	if err != nil || lines == 0 {
		skip("tdm/a warmup death is not logged",
			fmt.Sprintf("the stats log wrote nothing to judge against (%v)", err))
		return
	}
	// The warmup loadout is every weapon but the BFG, so there is a weapon
	// worth dropping in hand once it is selected.
	b.Cmd("use Super Shotgun")
	time.Sleep(1500 * time.Millisecond)
	litter := props(srv)
	if !check("tdm/setup: `kill` killed bravo in warmup", suicide(b, "bravo"),
		"an obituary for bravo") {
		return
	}
	time.Sleep(1500 * time.Millisecond)
	if now := props(srv); litter < 0 || now < 0 {
		skip("tdm/a warmup death drops nothing", "no `sv ruleset` census")
	} else {
		check("tdm/a warmup death drops nothing", now <= litter,
			"%d non-client edicts, %d before the death", now, litter)
	}
	n, lines, _ := deaths(stats, "bravo")
	check("tdm/a warmup death is not logged", n == 0,
		"%d death record(s) for bravo among %d line(s)", n, lines)
}

var cardRe = regexp.MustCompile(`Frags: `)

func phaseDuel() {
	fmt.Printf("\n##### C. duel\n")
	srv := boot("duel", *mapname, "ospimage-duel.jsonl", nil)
	defer srv.Stop()

	left := connect("left")
	right := connect("right")
	defer right.Disconnect()
	okL := joinTeam(left, teamA)
	okR := joinTeam(right, teamB)
	if !check("duel/setup: one player in each seat", okL && okR,
		"%s: %v, %s: %v", teamA, okL, teamB, okR) {
		left.Disconnect()
		return
	}

	// Both seats drawn first, so the row below is known to be reading a
	// board that can carry cards at all.
	two := 0
	for try := 0; try < 4 && two != 2; try++ {
		right.Cmd("score")
		time.Sleep(1500 * time.Millisecond)
		two = len(cardRe.FindAllString(right.Layout(), -1))
	}
	if !check("duel/setup: the board draws both cards", two == 2, "%d card(s)", two) {
		left.Disconnect()
		return
	}

	// The seat-0 player leaves.  `score` toggles, so alternating presses
	// guarantee a layout composed after the departure within two of them.
	left.Disconnect()
	time.Sleep(2 * time.Second)
	cards := -1
	for try := 0; try < 4 && cards != 1; try++ {
		right.Cmd("score")
		time.Sleep(1500 * time.Millisecond)
		cards = len(cardRe.FindAllString(right.Layout(), -1))
	}
	check("duel/an empty seat 0 still draws seat 1's card", cards == 1,
		"%d card(s) on the board after the seat-0 player left", cards)
}

// The cursor row is the one the engine prefixed with \x0d (see ospmenu).
var reRow = regexp.MustCompile(`\bc?string2?\s+"([^"]*)"`)

func cursor(layout string) string {
	for _, m := range reRow.FindAllStringSubmatch(layout, -1) {
		if strings.HasPrefix(m[1], "\x0d") {
			return strings.TrimPrefix(m[1], "\x0d")
		}
	}
	return ""
}

const menuMark = `Regular DM Mode|Teamplay Mode|1v1 Mode`

func phaseAutocam() {
	fmt.Printf("\n##### D. dm, an autocam observer's menu\n")
	srv := boot("dm", *mapname, "ospimage-cam.jsonl", nil)
	defer srv.Stop()

	target := connect("target")
	defer target.Disconnect()
	if !check("cam/setup: a player to track", enterFFA(target), "") {
		return
	}
	w := connect("watcher")
	defer w.Disconnect()
	w.Cmd("autocam")
	time.Sleep(1500 * time.Millisecond)

	open := false
	for try := 0; try < 4 && !open; try++ {
		w.Cmd("inven")
		if _, err := w.WaitLayout(menuMark, 2*time.Second); err == nil {
			open = true
		}
	}
	if !check("cam/setup: the tourney menu opens in autocam", open, "") {
		return
	}

	worst, samples, ok := deferredPress(w, "cam")
	if !ok {
		return
	}
	check("cam/a deferred cursor press is flushed within 1 s", worst < time.Second,
		"latencies %s, worst %.2fs (the 32-frame repaint is up to 3.2 s)",
		strings.Join(samples, " "), worst.Seconds())
}

// deferredPress times three trials of: one cursor press, seen; then a second
// press straight after it, inside the engine's one-second window, so the second
// is deferred to the dirty flush.  The latency is from the second press to the
// cursor seen on its new row, and the worst of the three is what is judged.
func deferredPress(w *playtest.Bot, tag string) (time.Duration, []string, bool) {
	worst := time.Duration(0)
	samples := []string{}
	for trial := 0; trial < 3; trial++ {
		time.Sleep(1500 * time.Millisecond)
		c0 := cursor(w.Layout())
		w.Cmd("invnext")
		c1 := c0
		for deadline := time.Now().Add(4 * time.Second); c1 == c0 && time.Now().Before(deadline); {
			time.Sleep(20 * time.Millisecond)
			c1 = cursor(w.Layout())
		}
		if c1 == c0 {
			check(tag+"/setup: the cursor moves at all", false, "still on %q after 4 s", c0)
			return 0, nil, false
		}
		t := time.Now()
		w.Cmd("invnext")
		c2 := c1
		for deadline := time.Now().Add(5 * time.Second); c2 == c1 && time.Now().Before(deadline); {
			time.Sleep(20 * time.Millisecond)
			c2 = cursor(w.Layout())
		}
		lat := time.Since(t)
		if c2 == c1 {
			lat = 5 * time.Second
		}
		if lat > worst {
			worst = lat
		}
		samples = append(samples, strconv.FormatFloat(lat.Seconds(), 'f', 2, 64)+"s")
	}
	return worst, samples, true
}

func phaseCTF() {
	fmt.Printf("\n##### E. ctf, Threewave's join menu\n")
	srv := boot("ctf", "q2ctf1", "ospimage-ctf.jsonl", nil)
	defer srv.Stop()

	c := connect("charlie")
	defer c.Disconnect()
	// Threewave's front door is the join menu, opened at ClientBegin.
	_, err := c.WaitLayout(`ThreeWave Capture the Flag`, 5*time.Second)
	if !check("ctf/setup: the join menu is on screen", err == nil, "") {
		return
	}
	worst, samples, ok := deferredPress(c, "ctf")
	if !ok {
		return
	}
	check("ctf/a deferred cursor press is flushed within 1 s", worst < time.Second,
		"latencies %s, worst %.2fs", strings.Join(samples, " "), worst.Seconds())
}

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
	phaseDM()
	phaseTDM()
	phaseDuel()
	phaseAutocam()
	phaseCTF()
	report()
}
