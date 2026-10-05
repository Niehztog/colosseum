// ra2roundclock -- does Rocket Arena's round time limit end a fight, on time,
// and award it to the team that is ahead on health and armour?
//
// The port of packetflinger's roundtimelimit adds an arena.cfg key: a fight
// still going when the limit runs out is decided by the living players' health
// plus armour, the fighting stops, and the round ends exactly as it would on a
// wipe.  A clock counts down at the top right, in a per-client configstring
// that STAT_ROUNDTIME (slot 28) points at.
//
// Two arenas on one server: one with a limit and one without, so every check
// on the first has a negative twin on the second.
//
//	hud        STAT_ROUNDTIME names the configstring past RA2's own four, and
//	           the statusbar draws it
//	ticks      the clock is sent once a second, every second -- no second
//	           skipped, which arena_think's num_arenas * 2 frame cadence would
//	           skip on this eight-arena map
//	on time    "Time's up!" arrives the limit after "FIGHT!", and the clock
//	           read 0:00
//	decision   the round goes to the side with the larger health + armour,
//	           or is a tie when they are equal
//	frozen     a rocket after the whistle does no damage
//	reset      the next round's clock starts again from the limit
//	log        the round record says timeout, and carries the limit
//	no limit   the arena without one shows no clock and keeps fighting
//
// Exit 0 all checks passed, 1 a check failed, 2 the scenario could not run.
package main

import (
	"bufio"
	"encoding/json"
	"flag"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"time"

	"q2playtest/playtest"
	"q2playtest/ra2"
)

const (
	statHealth      = 1
	statAmmo        = 3
	statArmor       = 5
	statArenaStatus = 17 // RA2: game.csr.items + game.num_items
	statRoundTime   = 28 // the port: STAT_ROUNDTIME
)

var reClock = regexp.MustCompile(`^\s*(\d+):(\d\d)$`)

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "", "read-only reference install (RA2 paks)")
	lib := flag.String("lib", "", "game library under test")
	dir := flag.String("dir", "/tmp/q2playtest/ra2roundclock", "scratch install dir")
	mapname := flag.String("map", "ra2map9", "map to test on")
	timed := flag.Int("arena", 8, "arena given a roundtimelimit")
	free := flag.Int("free", 4, "arena given none")
	limit := flag.Int("limit", 25, "roundtimelimit for the timed arena, seconds")
	port := flag.Int("port", 27977, "server port")
	label := flag.String("label", "", "label for the report")
	flag.Parse()

	if *q2 == "" || *ref == "" || *lib == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded, -ref and -lib")
		os.Exit(2)
	}
	if err := run(*q2, *ref, *lib, *dir, *mapname, *timed, *free, *limit, *port, *label); err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	if failed > 0 {
		os.Exit(1)
	}
}

var failed int

func check(name string, ok bool, detail string) {
	if ok {
		fmt.Printf("  PASS  %-28s %s\n", name, detail)
		return
	}
	failed++
	fmt.Printf("  FAIL  %-28s %s\n", name, detail)
}

func report(name, detail string) { fmt.Printf("  ..    %-28s %s\n", name, detail) }

func waitFor(timeout time.Duration, cond func() bool) error {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if cond() {
			return nil
		}
		time.Sleep(100 * time.Millisecond)
	}
	return fmt.Errorf("timed out after %s", timeout)
}

func clockSecs(s string) int {
	m := reClock.FindStringSubmatch(s)
	if m == nil {
		return -1
	}
	min, _ := strconv.Atoi(m[1])
	sec, _ := strconv.Atoi(m[2])
	return min*60 + sec
}

// total is a fighter's health plus armour, as its own HUD reads them.
func total(b *playtest.Bot) int { return b.Stat(statHealth) + b.Stat(statArmor) }

// centersAfter is how many centerprints a client has had, for waiting on the
// NEXT one matching a pattern rather than an old one.
func waitCenterAfter(b *playtest.Bot, n int, re string, timeout time.Duration) (string, error) {
	rx := regexp.MustCompile(re)
	var got string
	err := waitFor(timeout, func() bool {
		c := b.Centers()
		for i := n; i < len(c); i++ {
			if rx.MatchString(c[i]) {
				got = c[i]
				return true
			}
		}
		return false
	})
	return got, err
}

// fireAt points the shooter at the target and fires one rocket.
func fireAt(shooter, target *playtest.Bot) {
	shooter.Look(0, 0, 0)
	shooter.WaitFrames(6, 3*time.Second)
	delta := shooter.ViewAngles()
	src, dst := shooter.Origin(), target.Origin()
	dx, dy, dz := dst[0]-src[0], dst[1]-src[1], (dst[2]-8)-(src[2]+22)
	yaw := math.Atan2(dy, dx) * 180 / math.Pi
	pitch := -math.Atan2(dz, math.Hypot(dx, dy)) * 180 / math.Pi
	shooter.Look(pitch-delta[0], yaw-delta[1], 0)
	shooter.WaitFrames(6, 3*time.Second)
	shooter.Press(playtest.ButtonAttack, 200*time.Millisecond)
}

func run(q2, ref, lib, dir, mapname string, timed, free, limit, port int, label string) error {
	if label != "" {
		fmt.Printf("== %s ==\n", label)
	}
	os.RemoveAll(dir)
	if err := playtest.Install(dir, "arena", ref, lib); err != nil {
		return err
	}
	cfg := fmt.Sprintf("%s {\n\t%d {\n\t\tpickup: 1;\n\t\trounds: 9;\n\t\troundtimelimit: %d;\n\t}\n"+
		"\t%d {\n\t\tpickup: 1;\n\t\trounds: 9;\n\t}\n}\n", mapname, timed, limit, free)
	if err := playtest.WriteFixture(filepath.Join(dir, "arena", "arena.cfg"), []byte(cfg), 0o644); err != nil {
		return err
	}

	srv := &playtest.Server{
		Binary: q2, Dir: dir, Game: "arena", Map: mapname, Port: port,
		LogPath: filepath.Join(dir, "server.log"),
		Cvars: map[string]string{
			"g_ruleset":      "arena",
			"arenacfg":       "arena.cfg",
			"bots":           "0",
			"minimumplayers": "0",
			"admincode":      "0",
		},
	}
	if err := srv.Start(); err != nil {
		return err
	}
	defer srv.Stop()

	type seat struct {
		b     *playtest.Bot
		arena int
		side  string
	}
	red := playtest.NewBot("red1", "127.0.0.1", port)
	blue := playtest.NewBot("blue1", "127.0.0.1", port)
	red2 := playtest.NewBot("red2", "127.0.0.1", port)
	blue2 := playtest.NewBot("blue2", "127.0.0.1", port)
	for _, s := range []seat{{red, timed, "Red"}, {blue, timed, "Blue"}, {red2, free, "Red"}, {blue2, free, "Blue"}} {
		if err := s.b.Start(30 * time.Second); err != nil {
			return err
		}
		defer s.b.Disconnect()
		if err := ra2.JoinTeam(s.b, ra2.PickupTeam(s.arena, s.side)); err != nil {
			return fmt.Errorf("%s: %w", s.b.Name, err)
		}
	}
	if _, err := red.WaitCenter(`FIGHT`, 90*time.Second); err != nil {
		return fmt.Errorf("arena %d never started a round: %w", timed, err)
	}
	fight := time.Now()
	if _, err := red2.WaitCenter(`FIGHT`, 30*time.Second); err != nil {
		return fmt.Errorf("arena %d never started a round: %w", free, err)
	}
	red.WaitFrames(5, 5*time.Second)
	fmt.Printf("  ..    arena %d (roundtimelimit %d) and arena %d (none) are both fighting\n", timed, limit, free)

	// ---- 1. the HUD ---------------------------------------------------------
	cs := red.Stat(statRoundTime)
	base := red.Stat(statArenaStatus)
	check("STAT_ROUNDTIME names the clock", cs != 0 && base != 0 && cs == base+4,
		fmt.Sprintf("stat %d = %d, RA2's arena status is at %d", statRoundTime, cs, base))
	bar := red.StatusBar()
	check("statusbar draws the clock", strings.Contains(bar, "if 28 xr -42 yt 28 stat_string 28"),
		fmt.Sprintf("%d bytes of statusbar", len(bar)))
	check("no-limit arena: no clock", red2.Stat(statRoundTime) == 0,
		fmt.Sprintf("red2's stat %d = %d", statRoundTime, red2.Stat(statRoundTime)))

	// ---- 2. a hit, so that the decision has something to decide -------------
	red.Cmd("use Rocket Launcher")
	blue.Cmd("use Rocket Launcher")
	blue.WaitFrames(10, 4*time.Second)
	// The rocket either reaches red1 or bursts on the cover in front of blue1;
	// under armorprotect 2 that splash costs blue1 its own armour.  Either
	// way the two totals part, which is all the decision needs -- and it
	// shows a rocket CAN do damage, which the after-the-whistle shot needs.
	r0, b0 := total(red), total(blue)
	hit := false
	for i := 0; i < 6 && !hit && time.Since(fight) < time.Duration(limit-6)*time.Second; i++ {
		fireAt(blue, red)
		red.WaitFrames(15, 5*time.Second)
		hit = total(red) < r0 || total(blue) < b0
	}
	report("blue1 fires on red1", fmt.Sprintf("red1 %d -> %d, blue1 %d -> %d (health + armour)",
		r0, total(red), b0, total(blue)))

	// ---- 3. time's up ----------------------------------------------------------
	_, terr := red.WaitCenter(`Time's up`, time.Duration(limit+8)*time.Second-time.Since(fight))
	elapsed := time.Since(fight).Seconds()
	if terr != nil {
		check("the clock ends the fight", false, fmt.Sprintf("no \"Time's up!\" %.1f s after FIGHT!", elapsed))
	} else {
		check("the clock ends the fight", math.Abs(elapsed-float64(limit)) <= 1.0,
			fmt.Sprintf("\"Time's up!\" %.1f s after FIGHT!, limit %d", elapsed, limit))
	}
	// the totals the decision was made on: damage is off from here on
	rt, bt := total(red), total(blue)

	hist := red.ConfigStringHistory(cs)
	var secs []int
	for _, v := range hist {
		secs = append(secs, clockSecs(v))
	}
	consecutive := len(secs) > 1 && secs[0] == limit && secs[len(secs)-1] == 0
	for i := 1; i < len(secs) && consecutive; i++ {
		if secs[i] != secs[i-1]-1 {
			consecutive = false
		}
	}
	shown := hist
	if len(shown) > 6 {
		shown = append(append([]string{}, hist[:3]...), append([]string{"..."}, hist[len(hist)-3:]...)...)
	}
	check("clock ticks every second", terr == nil && consecutive,
		fmt.Sprintf("%d updates %q", len(hist), shown))

	// ---- 4. frozen: a rocket after the whistle does nothing -----------------
	if terr == nil && hit {
		fireAt(blue, red)
		red.WaitFrames(12, 4*time.Second)
		check("no damage after time's up", total(red) == rt && total(blue) == bt,
			fmt.Sprintf("the same shot: red1 %d -> %d, blue1 %d -> %d", rt, total(red), bt, total(blue)))
	} else if terr == nil {
		report("no damage after time's up", "not judged: no rocket did damage before the whistle either")
	}

	// ---- 5. the decision ------------------------------------------------------
	want := "It was a tie"
	switch {
	case bt > rt:
		want = ra2.PickupTeam(timed, "Blue") + " has won the round"
	case rt > bt:
		want = ra2.PickupTeam(timed, "Red") + " has won the round"
	}
	if terr == nil {
		got, err := red.WaitCenter(`has won the round|It was a tie|has won the match`, 15*time.Second)
		check("decided on health + armour", err == nil && strings.Contains(got, want),
			fmt.Sprintf("red %d vs blue %d: got %q, want %q", rt, bt, got, want))
		if rt == bt {
			report("note", "the totals were equal, so the decision under test was a tie")
		}
	}

	// ---- 6. no-limit arena kept fighting ------------------------------------
	gotUp := false
	for _, c := range red2.Centers() {
		if strings.Contains(c, "Time's up") {
			gotUp = true
		}
	}
	check("no-limit arena kept fighting", !gotUp && !red2.Spectating() && !blue2.Spectating() &&
		red2.Stat(statRoundTime) == 0,
		fmt.Sprintf("time's up seen %v, red2 pm_type %d, blue2 pm_type %d, stat %d = %d",
			gotUp, red2.PMType(), blue2.PMType(), statRoundTime, red2.Stat(statRoundTime)))

	// ---- 7. the next round's clock starts over ------------------------------
	if terr == nil {
		n := len(red.Centers())
		if _, err := waitCenterAfter(red, n, `FIGHT`, 60*time.Second); err != nil {
			check("next round's clock resets", false, "no second FIGHT!: "+err.Error())
		} else {
			red.WaitFrames(5, 3*time.Second)
			h := red.ConfigStringHistory(cs)
			first := ""
			if len(h) > len(hist) {
				first = h[len(hist)]
			}
			check("next round's clock resets", clockSecs(first) == limit,
				fmt.Sprintf("first update of round 2 is %q", first))
		}
	}

	// ---- 8. the round log -----------------------------------------------------
	if terr == nil {
		var path string
		filepath.Walk(dir, func(p string, info os.FileInfo, err error) error {
			if err == nil && info.Name() == "ra2stats.jsonl" {
				path = p
			}
			return nil
		})
		var rec map[string]any
		if f, err := os.Open(path); err == nil {
			sc := bufio.NewScanner(f)
			sc.Buffer(make([]byte, 1<<20), 1<<20)
			for sc.Scan() {
				var m map[string]any
				if json.Unmarshal(sc.Bytes(), &m) == nil && m["event"] == "round" &&
					m["arena"] == float64(timed) && rec == nil {
					rec = m
				}
			}
			f.Close()
		}
		ok := false
		detail := "no round record for arena " + strconv.Itoa(timed) + " in " + path
		if rec != nil {
			st, _ := rec["settings"].(map[string]any)
			ok = rec["timeout"] == true && st["roundtimelimit"] == float64(limit)
			detail = fmt.Sprintf("timeout=%v roundtimelimit=%v duration=%v", rec["timeout"], st["roundtimelimit"], rec["duration"])
		}
		check("round log says timeout", ok, detail)
	}

	if err := srv.Alive(5 * time.Second); err != nil {
		check("server survived", false, err.Error())
	}
	return nil
}
