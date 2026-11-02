// ra2forfeit -- does an arena match end when one of its sides has nobody left
// on it (R-RA-27)?
//
// RA2 does not ask once a match is under way.  fight_done() reads a side with
// nobody on it as a side wiped out, so the side still standing was handed every
// round after, a countdown and a results pause apart, until it had won the
// match alone -- a pickup side is locked, so the empty one even stays in the
// match -- and an arena both sides had left ran tie after tie to its last
// round.  A person leaving and the `bots` switch sweeping the bots out reach it
// the same way, through remove_from_team().
//
// q2dm1 under `arena` is one pickup arena, given 9 rounds here, so a side needs
// five wins.  The client "solo" is a person on Blue in god mode: nothing it
// stands among ends a round, so every outcome after a departure is the
// departure's.
//
//	people  `bots: 0`.  solo and b2 on Blue, r1 and r2 on Red; once they
//	        are fighting, r1, r2 and b2 disconnect.  Blue wins the match by
//	        forfeit at the next round boundary with no round handed out first,
//	        solo is told, the arena then waits with Red empty, and the round
//	        log's record says forfeit.
//	bots    `bots: 1` and the fill.  solo on Blue, bots on both sides; once a
//	        bot is fighting on Red, the admin menu's Apply switches the arena's
//	        bots off -- the settings write a passed vote makes, and the same
//	        sweep (RA_BotsVotedOut).  The same forfeit and the same wait.  Then
//	        Apply switches them back on, and once both sides have somebody the
//	        next match starts from its first round.
//	empty   `bots: 0`.  r1 on Red, b1 on Blue, and "late" on the server on
//	        no team.  Both fighters disconnect: the match ends once, with no
//	        winner, and the arena waits rather than running ties to its last
//	        round.  Then late joins Red and stays an observer: there is no
//	        match to be placed in alone, where RA2's next tie round placed it
//	        and Red won every round after.
//
// Exit 0 all checks passed, 1 a check failed, 2 the scenario could not run.
package main

import (
	"bufio"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
	"q2playtest/ra2"
)

const rounds = 9

var (
	q2   = flag.String("q2proded", "", "path to q2proded")
	ref  = flag.String("ref", "/usr/share/games/quake2/baseq2", "retail baseq2 paks")
	lib  = flag.String("lib", "", "game library under test")
	glad = flag.String("gladdir", "", "composed GLADDIR: brain, pak7, bots.cfg and assets/maps/q2dm1.aas")
	root = flag.String("dir", "/tmp/q2playtest/ra2forfeit", "scratch install root")
	port = flag.Int("port", 28050, "first server port; each phase takes the next")
	only = flag.String("only", "", "run only these phases, e.g. people,empty")
	hold = flag.Duration("hold", 30*time.Second, "how long to watch after a departure")

	checks, failed, skips int

	// Every way a round can end in arena 1, as arena_think logs it: the team
	// credited, -1 for nobody, and the centerprint.
	reOutcome = regexp.MustCompile(`^1: (-?\d+) (.*(?:has won the round!|has won the match!!|` +
		`has won the match by forfeit!|It was a tie!|Not enough teams to go on))\s*$`)
	// fill_arena's line, which is a match being started.
	reVs     = regexp.MustCompile(`^1: #1 Pickup (?:Red|Blue) vs #1 Pickup (?:Red|Blue)\s*$`)
	reArena  = regexp.MustCompile(`^arena 1 +(\S+) +pickup=\d+ round=(\d+)/(\d+) waiting=(\d+) active=(\d+) .*here=(\d+)`)
	reTeam   = regexp.MustCompile(`^team (\d+) +arena=1 side=-?\d+ members=(\d+) fighting=(\d+) wins=(-?\d+) locked=\d "(.*)"`)
	reClient = regexp.MustCompile(`^client \d+ +(.+?) +(bot|human) +arena=(\d+) team=(-?\d+) fight=(\d+) solid=\d+ dmg=\d+ dead=(\d+)`)
)

func check(name string, ok bool, detail string) {
	checks++
	tag := " ok "
	if !ok {
		tag = "FAIL"
		failed++
	}
	fmt.Printf("  [%s] %-40s %s\n", tag, name, detail)
}

func skip(name, why string) {
	skips++
	fmt.Printf("  [skip] %-39s %s\n", name, why)
}

func want(phase string) bool {
	if *only == "" {
		return true
	}
	for _, p := range strings.Split(*only, ",") {
		if strings.TrimSpace(p) == phase {
			return true
		}
	}
	return false
}

// ---------------------------------------------------------------- the server

func boot(tag string, bots bool, p int) (*playtest.Server, error) {
	d := filepath.Join(*root, tag)
	os.RemoveAll(d)
	if err := colosseum.Install(d, *ref, "", *lib); err != nil {
		return nil, err
	}
	on, fill := 0, "0"
	if bots {
		if err := colosseum.InstallBrain(d, *glad); err != nil {
			return nil, err
		}
		on, fill = 1, "1"
	}
	// q2dm1 is an id map, which RA2 runs as one pickup arena, so the global
	// block is the whole file.
	cfg := fmt.Sprintf("maploop: q2dm1;\nvotetries: 3;\nrounds: %d;\nbots: %d;\nallowvotingbots: 1;\n",
		rounds, on)
	path := filepath.Join(d, "colosseum", "arena.cfg")
	os.Remove(path)
	if err := playtest.WriteFixture(path, []byte(cfg), 0o644); err != nil {
		return nil, err
	}
	srv := &playtest.Server{
		Binary: *q2, Dir: d, Game: "colosseum", Map: "q2dm1",
		Port: p, MaxClients: 12,
		Cvars: map[string]string{
			"g_ruleset": "arena", "cheats": "1", "skill": "1",
			"botfill": fill, "minimumplayers": "0", "bots_minplayers": "0",
			"admincode": "1234",
		},
		LogPath: filepath.Join(d, "server.log"),
	}
	return srv, srv.Start()
}

func join(name, side string, p int) (*playtest.Bot, error) {
	b := playtest.NewBot(name, "127.0.0.1", p)
	if err := b.Start(60 * time.Second); err != nil {
		return nil, err
	}
	if side == "" {
		return b, nil
	}
	if err := ra2.JoinTeam(b, ra2.PickupTeam(1, side)); err != nil {
		return b, fmt.Errorf("%s joining %s: %w", name, side, err)
	}
	return b, nil
}

// ---------------------------------------------------------------- reading it

type team struct {
	num, members, fighting, wins int
}

type client struct {
	name                     string
	bot                      bool
	arena, team, fight, dead int
}

// arena is one `sv arenadump`, arena 1's part of it.
type arena struct {
	state                                string
	round, rounds, waiting, active, here int
	teams                                map[string]team
	clients                              []client
}

func (a arena) side(s string) team {
	t, ok := a.teams[ra2.PickupTeam(1, s)]
	if !ok {
		return team{num: -2, members: -1}
	}
	return t
}

func (a arena) String() string {
	r, b := a.side("Red"), a.side("Blue")
	return fmt.Sprintf("%s round=%d/%d waiting=%d active=%d here=%d; Red members=%d wins=%d, Blue members=%d wins=%d",
		a.state, a.round, a.rounds, a.waiting, a.active, a.here, r.members, r.wins, b.members, b.wins)
}

func atoi(s string) int { n, _ := strconv.Atoi(s); return n }

func dump(srv *playtest.Server) arena {
	a := arena{state: "?", teams: map[string]team{}}
	mark := srv.Len()
	srv.Console("sv arenadump")
	if _, err := srv.WaitLogFrom(mark, `arena 1 +\S+ +pickup=`, 10*time.Second); err != nil {
		return a
	}
	time.Sleep(600 * time.Millisecond) // the team and client lines come after
	for _, l := range srv.LogFrom(mark) {
		l = strings.TrimSpace(l)
		if m := reArena.FindStringSubmatch(l); m != nil {
			a.state = m[1]
			a.round, a.rounds = atoi(m[2]), atoi(m[3])
			a.waiting, a.active, a.here = atoi(m[4]), atoi(m[5]), atoi(m[6])
		} else if m := reTeam.FindStringSubmatch(l); m != nil {
			a.teams[m[5]] = team{atoi(m[1]), atoi(m[2]), atoi(m[3]), atoi(m[4])}
		} else if m := reClient.FindStringSubmatch(l); m != nil {
			a.clients = append(a.clients, client{m[1], m[2] == "bot",
				atoi(m[3]), atoi(m[4]), atoi(m[5]), atoi(m[6])})
		}
	}
	return a
}

// fighting polls until arena 1 is fighting and cond holds of it.
func fighting(srv *playtest.Server, cond func(arena) bool, timeout time.Duration) (arena, error) {
	var a arena
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		a = dump(srv)
		if a.state == "fighting" && cond(a) {
			return a, nil
		}
		time.Sleep(time.Second)
	}
	return a, fmt.Errorf("arena 1 never reached the fight wanted; last: %s", a)
}

// alive says whether name is fighting in arena 1 and not dead.
func alive(a arena, name string) bool {
	for _, c := range a.clients {
		if c.name == name {
			return c.arena == 1 && c.fight == 1 && c.dead == 0
		}
	}
	return false
}

type outcome struct {
	at   time.Duration
	team int
	text string
}

func (o outcome) String() string { return fmt.Sprintf("+%.1fs %d %q", o.at.Seconds(), o.team, o.text) }

// observe stamps every round outcome arena 1 logs from mark on, for d.
func observe(srv *playtest.Server, mark int, d time.Duration) []outcome {
	var out []outcome
	t0 := time.Now()
	seen := mark
	for time.Since(t0) < d {
		time.Sleep(200 * time.Millisecond)
		ls := srv.LogFrom(seen)
		seen += len(ls)
		for _, l := range ls {
			if m := reOutcome.FindStringSubmatch(strings.TrimSpace(l)); m != nil {
				out = append(out, outcome{time.Since(t0), atoi(m[1]), m[2]})
			}
		}
	}
	return out
}

func list(outs []outcome) string {
	if len(outs) == 0 {
		return "none"
	}
	s := make([]string, len(outs))
	for i, o := range outs {
		s[i] = o.String()
	}
	return strings.Join(s, ", ")
}

// lastRecord is arena 1's last round record in the server's round log.
func lastRecord(dir string) (map[string]any, string) {
	var path string
	filepath.Walk(dir, func(p string, info os.FileInfo, err error) error {
		if err == nil && info.Name() == "ra2stats.jsonl" {
			path = p
		}
		return nil
	})
	if path == "" {
		return nil, "no ra2stats.jsonl under " + dir
	}
	f, err := os.Open(path)
	if err != nil {
		return nil, err.Error()
	}
	defer f.Close()
	var rec map[string]any
	n := 0
	sc := bufio.NewScanner(f)
	sc.Buffer(make([]byte, 1<<20), 1<<20)
	for sc.Scan() {
		var m map[string]any
		if json.Unmarshal(sc.Bytes(), &m) == nil && m["event"] == "round" && m["arena"] == float64(1) {
			rec = m
			n++
		}
	}
	if rec == nil {
		return nil, "no round record for arena 1 in " + path
	}
	return rec, fmt.Sprintf("%d record(s); the last: round=%v/%v forfeit=%v", n, rec["round"], rec["rounds"], rec["forfeit"])
}

// forfeited is the shape both one-sided departures must have: the first outcome
// after the departure is Blue's forfeit win, at the first round boundary --
// a fight_done(), three seconds of results -- and nothing comes after it.
func forfeited(tag string, outs []outcome, blue int, after arena, solo *playtest.Bot) {
	first := "none"
	if len(outs) > 0 {
		first = outs[0].String()
	}
	check(tag+"/the match ends at the next boundary",
		len(outs) > 0 && outs[0].team == blue &&
			strings.HasSuffix(outs[0].text, "#1 Pickup Blue has won the match by forfeit!") &&
			outs[0].at < 15*time.Second,
		"first outcome "+first)
	won := 0
	for _, o := range outs {
		if strings.Contains(o.text, "has won the round!") {
			won++
		}
	}
	check(tag+"/no round is handed out", won == 0 && len(outs) == 1,
		fmt.Sprintf("%d round win(s) among %s", won, list(outs)))
	told := false
	for _, c := range solo.Centers() {
		if strings.Contains(c, "has won the match by forfeit!") {
			told = true
		}
	}
	check(tag+"/solo is told", told,
		fmt.Sprintf("%d centerprint(s) received", len(solo.Centers())))
	check(tag+"/the arena waits for a side",
		after.state == "roundend" && after.active == 0 &&
			after.side("Red").members == 0 && after.side("Blue").members == 1,
		after.String())
}

// ---------------------------------------------------------------- the phases

func people(p int) error {
	fmt.Printf("\n##### people leave mid-match (port %d)\n", p)
	srv, err := boot("people", false, p)
	if err != nil {
		return err
	}
	defer srv.Stop()

	solo, err := join("solo", "Blue", p)
	if err != nil {
		return err
	}
	defer solo.Disconnect()
	solo.Cmd("god")
	var leavers []*playtest.Bot
	for _, c := range [][2]string{{"r1", "Red"}, {"b2", "Blue"}, {"r2", "Red"}} {
		b, err := join(c[0], c[1], p)
		if err != nil {
			return err
		}
		leavers = append(leavers, b)
	}

	before, err := fighting(srv, func(a arena) bool {
		return a.here == 4 && alive(a, "solo")
	}, 90*time.Second)
	if err != nil {
		return err
	}
	fmt.Printf("  ..     before: %s\n", before)

	mark := srv.Len()
	for _, b := range leavers {
		b.Disconnect()
	}
	outs := observe(srv, mark, *hold)
	after := dump(srv)
	fmt.Printf("  ..     after %v: %s\n", *hold, list(outs))

	forfeited("people", outs, before.side("Blue").num, after, solo)

	rec, detail := lastRecord(filepath.Join(*root, "people"))
	check("people/the round log says forfeit", rec != nil && rec["forfeit"] == true, detail)
	return nil
}

func bots(p int) error {
	fmt.Printf("\n##### the bots switched off mid-match, then on (port %d)\n", p)
	if *glad == "" {
		skip("bots", "no -gladdir")
		return nil
	}
	if _, err := os.Stat(filepath.Join(*glad, "assets", "maps", "q2dm1.aas")); err != nil {
		skip("bots", "no assets/maps/q2dm1.aas under "+*glad+" -- compose a GLADDIR with one")
		return nil
	}
	srv, err := boot("bots", true, p)
	if err != nil {
		return err
	}
	defer srv.Stop()

	solo, err := join("solo", "Blue", p)
	if err != nil {
		return err
	}
	defer solo.Disconnect()
	solo.Cmd("god")

	// Bots on both sides, and a round that has a Red one fighting in it: a
	// bot the fill seats mid-round waits for the next round's placement, and
	// solo in god mode ends no round, so the first may be all that comes.
	before, err := fighting(srv, func(a arena) bool {
		red := a.side("Red")
		if red.members < 2 || a.side("Blue").members < 2 || !alive(a, "solo") {
			return false
		}
		for _, c := range a.clients {
			if c.bot && c.arena == 1 && c.team == red.num && c.fight == 1 && c.dead == 0 {
				return true
			}
		}
		return false
	}, 240*time.Second)
	if err != nil {
		return err
	}
	fmt.Printf("  ..     before: %s\n", before)

	mark := srv.Len()
	if err := allowBots(solo, "YES"); err != nil {
		return err
	}
	outs := observe(srv, mark, *hold)
	after := dump(srv)
	fmt.Printf("  ..     after %v: %s\n", *hold, list(outs))

	swept := len(srv.GrepFrom(mark, `leaves arena 1: bots are switched off there`))
	left := 0
	for _, c := range after.clients {
		if c.bot {
			left++
		}
	}
	check("bots/the bots leave", swept > 0 && left == 0,
		fmt.Sprintf("%d swept, %d bot(s) on the server after", swept, left))
	forfeited("bots", outs, before.side("Blue").num, after, solo)

	// ...and back on.  ROUNDEND asks check_for_teams() every pass, so the next
	// match is the first moment both sides have somebody: the fill seats bots,
	// the pickup warmup counts down, and fill_arena starts it at round 1.
	mark = srv.Len()
	if err := allowBots(solo, "NO"); err != nil {
		return err
	}
	vs, err := srv.WaitLogFrom(mark, reVs.String(), 150*time.Second)
	next := dump(srv)
	red, blue := next.side("Red"), next.side("Blue")
	check("bots/back on: the next match starts",
		err == nil && next.round == 1 && red.members > 0 && blue.members > 0 &&
			red.wins == 0 && blue.wins == 0 &&
			(next.state == "countdown" || next.state == "fighting"),
		fmt.Sprintf("%q; %s", strings.TrimSpace(vs), next))
	return nil
}

// allowBots toggles the arena admin menu's Allow Bots row from the value it
// shows, and applies it.  The admin form is the one a scripted client can take
// alone -- a vote needs a second player to ratify it -- and the two converge
// on one settings write and the same sweep.
func allowBots(b *playtest.Bot, from string) error {
	b.Cmd("arenaadmin 1234 1")
	if err := b.WaitMenu(`(?i)arena admin menu`, 20*time.Second); err != nil {
		return fmt.Errorf("admin menu: %w", err)
	}
	if err := pick(b, `(?i)allow bots.*`+from); err != nil {
		return err
	}
	time.Sleep(300 * time.Millisecond)
	return pick(b, `(?i)^apply$`)
}

func pick(b *playtest.Bot, re string) error {
	rx := regexp.MustCompile(re)
	for steps := 0; steps < 80; steps++ {
		_, items := b.Menu()
		for _, it := range items {
			if it.Selected && rx.MatchString(it.Text) {
				return b.MenuUse(20 * time.Second)
			}
		}
		b.Cmd("invnext")
		time.Sleep(150 * time.Millisecond)
	}
	return fmt.Errorf("cursor never reached %q in menu %q", re, b.MenuTitle())
}

func empty(p int) error {
	fmt.Printf("\n##### both sides leave mid-match (port %d)\n", p)
	srv, err := boot("empty", false, p)
	if err != nil {
		return err
	}
	defer srv.Stop()

	// On the server from the start, on no team.  A pickup arena has no
	// audience but its two sides -- AddtoArena refuses one from the arena
	// menu -- so nobody can be told this match is over; what late can show is
	// what the next arrival finds.
	late, err := join("late", "", p)
	if err != nil {
		return err
	}
	defer late.Disconnect()
	r1, err := join("r1", "Red", p)
	if err != nil {
		return err
	}
	b1, err := join("b1", "Blue", p)
	if err != nil {
		return err
	}

	before, err := fighting(srv, func(a arena) bool {
		return a.here == 2 && alive(a, "r1") && alive(a, "b1")
	}, 90*time.Second)
	if err != nil {
		return err
	}
	fmt.Printf("  ..     before: %s\n", before)

	// Together: the results pause is three seconds, and both have to be gone
	// by its end for this to be the no-side case rather than the one-side one.
	mark := srv.Len()
	r1.Disconnect()
	b1.Disconnect()
	outs := observe(srv, mark, *hold)
	after := dump(srv)
	fmt.Printf("  ..     after %v: %s\n", *hold, list(outs))

	check("empty/the match ends once, with no winner",
		len(outs) == 1 && outs[0].team == -1 &&
			strings.HasSuffix(outs[0].text, "Not enough teams to go on") &&
			outs[0].at < 15*time.Second,
		list(outs))
	check("empty/the arena waits", after.state == "roundend" && after.active == 0 &&
		after.side("Red").members == 0 && after.side("Blue").members == 0,
		after.String())

	// A side with somebody on it and a side with nobody is not a match.  The
	// joiner is an observer until a placement puts its side in (menuAddtoTeam),
	// and the next placement RA2 had for it was the next tie round's, alone.
	mark = srv.Len()
	if err := ra2.JoinTeam(late, ra2.PickupTeam(1, "Red")); err != nil {
		return fmt.Errorf("late joining Red: %w", err)
	}
	wait := 20 * time.Second
	since := observe(srv, mark, wait)
	started := srv.GrepFrom(mark, reVs.String())
	again := dump(srv)
	check("empty/a late joiner waits for an opponent",
		late.Spectating() && again.state == "roundend" && len(since) == 0 && len(started) == 0,
		fmt.Sprintf("after %v: spectating=%v, %d outcome(s), %d match start(s); %s",
			wait, late.Spectating(), len(since), len(started), again))
	return nil
}

func main() {
	flag.Parse()
	if *q2 == "" || *lib == "" {
		fmt.Fprintln(os.Stderr, "usage: ra2forfeit -q2proded <bin> -lib <game.so> [-gladdir <dir>] [-ref <baseq2>]")
		os.Exit(2)
	}
	for i, ph := range []struct {
		name string
		run  func(int) error
	}{{"people", people}, {"bots", bots}, {"empty", empty}} {
		if !want(ph.name) {
			continue
		}
		if err := ph.run(*port + i); err != nil {
			fmt.Fprintf(os.Stderr, "phase %s could not run: %v\n", ph.name, err)
			os.Exit(2)
		}
	}
	fmt.Printf("\n%d check(s), %d failed, %d skipped\n", checks, failed, skips)
	if failed > 0 {
		os.Exit(1)
	}
}
