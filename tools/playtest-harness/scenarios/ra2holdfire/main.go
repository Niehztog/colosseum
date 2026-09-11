// ra2holdfire -- two questions about a Rocket Arena bot, asked of the running
// game because neither can be answered from the source.
//
//  1. Does a bot fire during the round COUNTDOWN?  RA2 grants damage only when
//     the countdown reaches zero, and hands a fighter its ammo once per round,
//     so a shot before the bell is spent ammo and nothing else.  The measurement
//     is the ammo the bot still has on the first frame of the fight: give_ammo()
//     handed out a figure the arena config knows, and only firing takes it away.
//
//  2. Which weapon does it choose, and does the inventory it chose from say
//     anything true?  `sv botinv` prints two lines per bot -- what the BRAIN
//     reads at its own slot numbers, and what the client actually holds -- so a
//     translation defect between the two index spaces is one dump, not an
//     inference.
//
// Neither needs the bot to walk anywhere, but both need the real brain, so
// -gladdir is required.
//
// Exit 0 both checks passed, 1 one failed, 2 the scenario could not run.
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
	"q2playtest/ra2"
)

// `sv botinv`'s three-line block per bot.
//
//	3: Quad Bitch       arena 1 countdown  hold=1 weapon Machinegun
//	   brain    blaster 1 shotgun 1 ... grapple 0
//	   brain    shells 100 bullets 200 ...
//	   game     shells 100 bullets 200 ...
var (
	reHead = regexp.MustCompile(`^\s*(\d+):\s+(\S.*?)\s+arena\s+(\d+)\s+(\w+)\s+hold=(\d)\s+weapon\s+(.*?)\s+fire asked (\d+) dropped (\d+)$`)
	rePair = regexp.MustCompile(`(\w+) (-?\d+)`)
)

type sample struct {
	name    string
	arena   int
	state   string
	hold    bool
	weapon  string
	asked   int // attack the brain wanted while the arena was not fighting
	dropped int // ...and how much of it the gate took away
	brain   map[string]int
	game    map[string]int
}

func pairs(line string) map[string]int {
	out := map[string]int{}
	for _, m := range rePair.FindAllStringSubmatch(line, -1) {
		n, _ := strconv.Atoi(m[2])
		out[m[1]] = n
	}
	return out
}

// One `sv botinv` block per bot out of the console lines that arrived after the
// command.  The SECOND `brain` line is the ammo one -- the first is weapons.
func parseDump(lines []string) []sample {
	var out []sample
	for i := 0; i < len(lines); i++ {
		m := reHead.FindStringSubmatch(playtest.Decode(lines[i]))
		if m == nil {
			continue
		}
		s := sample{name: strings.TrimSpace(m[2]), state: m[4],
			hold: m[5] == "1", weapon: strings.TrimSpace(m[6])}
		s.arena, _ = strconv.Atoi(m[3])
		s.asked, _ = strconv.Atoi(m[7])
		s.dropped, _ = strconv.Atoi(m[8])
		nbrain := 0
		for j := i + 1; j < len(lines) && j <= i+3; j++ {
			l := strings.TrimSpace(playtest.Decode(lines[j]))
			switch {
			case strings.HasPrefix(l, "brain"):
				nbrain++
				if nbrain == 2 {
					s.brain = pairs(strings.TrimPrefix(l, "brain"))
				}
			case strings.HasPrefix(l, "game"):
				s.game = pairs(strings.TrimPrefix(l, "game"))
			}
		}
		out = append(out, s)
	}
	return out
}

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "retail baseq2 paks")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "Threewave paks")
	lib := flag.String("lib", "", "game library under test")
	glad := flag.String("gladdir", "", "gladiator-bot-restored checkout (the brain)")
	dir := flag.String("dir", "/tmp/q2playtest/ra2holdfire", "scratch install dir")
	mapname := flag.String("map", "q2dm1", "map to test on")
	arena := flag.Int("arena", 1, "pickup arena to join")
	nbots := flag.Int("bots", 3, "how many bots")
	secs := flag.Int("listen", 150, "seconds to watch")
	port := flag.Int("port", 27990, "server port")
	label := flag.String("label", "", "label for the report")
	flag.Parse()

	if *q2 == "" || *lib == "" || *glad == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded, -lib and -gladdir")
		os.Exit(2)
	}
	bad, err := run(*q2, *ref, *ctf, *lib, *glad, *dir, *mapname, *arena, *nbots,
		*secs, *port, *label)
	if err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	if bad > 0 {
		os.Exit(1)
	}
}

func run(q2, ref, ctf, lib, glad, dir, mapname string, arena, nbots, secs, port int,
	label string) (int, error) {

	if label != "" {
		fmt.Printf("== %s ==\n", label)
	}
	os.RemoveAll(dir)
	if err := colosseum.Install(dir, ref, ctf, lib); err != nil {
		return 0, err
	}
	if err := colosseum.InstallBrain(dir, glad); err != nil {
		return 0, err
	}

	srv := &playtest.Server{
		Binary: q2, Dir: dir, Game: "colosseum", Map: mapname, Port: port,
		MaxClients: 12, LogPath: filepath.Join(dir, "server.log"),
		Cvars: map[string]string{
			"g_ruleset": "arena", "skill": "1", "admincode": "0",
			"minimumplayers": "0", "bots_minplayers": "0", "arena": "0",
		},
	}
	if err := srv.Start(); err != nil {
		return 0, err
	}
	defer srv.Stop()

	b := playtest.NewBot("witness", "127.0.0.1", port)
	if err := b.Start(60 * time.Second); err != nil {
		return 0, err
	}
	defer b.Disconnect()
	if err := ra2.JoinTeam(b, ra2.PickupTeam(arena, "Red")); err != nil {
		return 0, fmt.Errorf("witness could not join: %w", err)
	}
	for i := 0; i < nbots; i++ {
		srv.Console("sv addrandom")
		time.Sleep(400 * time.Millisecond)
	}
	fmt.Printf("  ..    witness on %s, %d bot(s), sampling for %ds\n",
		ra2.PickupTeam(arena, "Red"), nbots, secs)

	// ---- sample -----------------------------------------------------------
	var all []sample
	deadline := time.Now().Add(time.Duration(secs) * time.Second)
	for time.Now().Before(deadline) {
		n := srv.Len()
		srv.Console("sv botinv")
		time.Sleep(500 * time.Millisecond)
		all = append(all, parseDump(srv.GrepFrom(n, `.`))...)
	}

	// ---- report -----------------------------------------------------------
	states := map[string]int{}
	weapons := map[string]int{}
	firstFight := map[string]sample{} // per bot, the earliest `fighting` sample
	leanest := map[string]sample{}    // per bot, the worst COUNTDOWN sample it appeared in
	worstHold := []sample{}           // the gate read back: hold must never be set while fighting
	for _, s := range all {
		states[s.state]++
		switch s.state {
		case "fighting":
			weapons[s.weapon]++
			if _, seen := firstFight[s.name]; !seen {
				firstFight[s.name] = s
			}
			if s.hold {
				worstHold = append(worstHold, s)
			}
		case "countdown":
			// A countdown sample is the honest place to ask what firing cost.
			// The arena is filled at the START of the countdown, so any figure
			// below give_ammo's was spent inside it.  The first FIGHTING sample
			// cannot answer this: half a second of legitimate shooting fits
			// inside one sampling interval.
			cur, seen := leanest[s.name]
			if !seen || total(s.game) < total(cur.game) {
				leanest[s.name] = s
			}
		}
	}
	fmt.Printf("  ..    %d bot sample(s); states:", len(all))
	for _, k := range keys(states) {
		fmt.Printf(" %s=%d", k, states[k])
	}
	fmt.Println()

	if len(all) == 0 {
		return 0, fmt.Errorf("no bot ever appeared in `sv botinv` -- nothing to check")
	}
	if states["fighting"] == 0 {
		return 0, fmt.Errorf("no round was ever fought in %ds -- the checks never ran", secs)
	}

	fmt.Println("  ..    weapon chosen while fighting:")
	for _, k := range keys(weapons) {
		fmt.Printf("  ..      %-18s %4d sample(s)\n", k, weapons[k])
	}

	fmt.Println("  ..    the first frame of each bot's first fight:")
	bad := 0
	for _, name := range keysOf(firstFight) {
		s := firstFight[name]
		fmt.Printf("  ..      %-18s brain bullets %-4d rockets %-4d cells %-4d slugs %-4d\n",
			name, s.brain["bullets"], s.brain["rockets"], s.brain["cells"], s.brain["slugs"])
		fmt.Printf("  ..      %-18s game  bullets %-4d rockets %-4d cells %-4d slugs %-4d\n",
			"", s.game["bullets"], s.game["rockets"], s.game["cells"], s.game["slugs"])
	}

	// CHECK 1.  The brain's slots must agree with the client's own
	// inventory.  This is the whole of the translation: one wrong row and a
	// weapon's weight is zeroed for the rest of the level.
	for _, name := range keysOf(firstFight) {
		s := firstFight[name]
		for _, k := range []string{"shells", "bullets", "cells", "rockets", "slugs", "grenades"} {
			if s.brain[k] != s.game[k] {
				fmt.Printf("  FAIL  brain reads the wrong slot   %s: %s brain=%d game=%d\n",
					name, k, s.brain[k], s.game[k])
				bad++
			}
		}
	}
	if bad == 0 {
		fmt.Println("  PASS  brain inventory == client inventory  for every bot, every ammo type")
	}

	// CHECK 2.  Nothing may be spent inside a countdown at all.
	want := map[string]int{"bullets": 200, "rockets": 50, "cells": 150, "slugs": 50}
	spent := 0
	if len(leanest) == 0 {
		fmt.Println("  ..    no countdown was ever sampled -- check 2 did not run")
	} else {
		fmt.Println("  ..    the leanest countdown sample per bot:")
	}
	for _, name := range keysOf(leanest) {
		s := leanest[name]
		fmt.Printf("  ..      %-18s game  bullets %-4d rockets %-4d cells %-4d slugs %-4d\n",
			name, s.game["bullets"], s.game["rockets"], s.game["cells"], s.game["slugs"])
		for _, k := range keys(want) {
			if s.game[k] < want[k] {
				fmt.Printf("  FAIL  ammo spent in a countdown   %s: %s %d of %d left\n",
					name, k, s.game[k], want[k])
				spent++
			}
		}
	}
	if spent == 0 && len(leanest) > 0 {
		fmt.Println("  PASS  no bot spent ammo in a countdown  full load in every countdown sample")
	}
	// CHECK 3, and the one that does not depend on two bots finding
	// each other inside a five-second countdown: the brain's OWN request to
	// fire, counted only while the arena was not being fought, against how much
	// of it the gate took away.  Equal is the whole of the fix; a gap is a shot
	// that left during a countdown.
	last := map[string]sample{}
	for _, s := range all {
		last[s.name] = s
	}
	fmt.Println("  ..    attack the brain asked for outside a fight, and what the gate did:")
	leaked := 0
	for _, name := range keysOf(last) {
		s := last[name]
		fmt.Printf("  ..      %-18s asked %-5d dropped %-5d\n", name, s.asked, s.dropped)
		if s.asked > s.dropped {
			leaked += s.asked - s.dropped
		}
	}
	if leaked > 0 {
		fmt.Printf("  FAIL  shots left during a countdown   %d frame(s) of attack were not held\n", leaked)
	} else {
		fmt.Println("  PASS  every out-of-fight attack was held  asked == dropped for every bot")
	}
	if leaked > 0 {
		bad++
	}

	if len(worstHold) > 0 {
		fmt.Printf("  ..    %d sample(s) reported hold=1 while fighting -- the gate is inverted\n",
			len(worstHold))
		bad++
	}
	return bad + spent, nil
}

// the four ammo types the arena config names, added up -- a single number to
// order two countdown samples by "which one had fired more".
func total(m map[string]int) int {
	return m["bullets"] + m["rockets"] + m["cells"] + m["slugs"] + m["shells"]
}

func keys(m map[string]int) []string {
	out := make([]string, 0, len(m))
	for k := range m {
		out = append(out, k)
	}
	sort.Strings(out)
	return out
}

func keysOf(m map[string]sample) []string {
	out := make([]string, 0, len(m))
	for k := range m {
		out = append(out, k)
	}
	sort.Strings(out)
	return out
}
