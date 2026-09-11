// ra2pickuptwins -- do two fighters placed at a PICKUP arena's round start end
// up standing inside each other?
//
// ra2twins asks this of the observer/farthest path.  A pickup arena is the
// other selector and the sharper case: SelectRandomArenaSpawnPoint gives the
// two sides alternate info_player_deathmatch indices and picks among them at
// random, so `ra2map26` arena 1 -- six pads, three per side, pickup: 1 -- has
// three clients on one side drawing from three pads.  Uniform draws collide
// 78% of the time.
//
// RA2's two answers both fail during a countdown: a fighter is takedamage
// DAMAGE_NO until ASTATE_FIGHTING, so KillBox takes its push branch rather
// than the telefrag, and that push adds ONE random vector to BOTH bodies --
// the pair drifts as a pair and the gap never changes.
//
// The measurement is per ROUND START, because that is when every fighter of a
// round is placed, and bots that do not shoot hold a round forever.  So a
// sample is: connect the clients, wait for the round to go live, look, then
// disconnect them all and let the arena empty.  Reported as "k of n samples
// had an overlapping pair", which is the honest shape for a probabilistic
// defect -- a single clean round proves nothing.
//
// Exit 0 no sample overlapped and every client could walk, 1 one did,
// 2 the scenario could not run.
package main

import (
	"flag"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"time"

	"q2playtest/playtest"
	"q2playtest/ra2"
)

var (
	binary  = flag.String("q2proded", "", "path to the q2proded binary")
	ref     = flag.String("ref", "", "read-only reference install holding the mod's paks and cfg")
	lib     = flag.String("lib", "", "game library to test")
	dir     = flag.String("dir", "/tmp/q2playtest/ra2pickuptwins", "scratch install directory to build")
	gameDir = flag.String("game", "arena", "mod directory name")
	mapName = flag.String("map", "ra2map26", "map to run")
	arena   = flag.Int("arena", 1, "arena number; must be a pickup arena in arena.cfg")
	clients = flag.Int("clients", 6, "clients to seat, alternating Red and Blue")
	samples = flag.Int("samples", 8, "round starts to sample")
	port    = flag.Int("port", 27990, "server UDP port")
	ruleset = flag.String("ruleset", "arena", "g_ruleset for a library that serves several")
	label   = flag.String("label", "", "label for the report")
)

// A player's bounding box is 32x32 wide and 56 tall.  Two boxes whose centres
// are closer than that in both axes are interpenetrating, not merely adjacent.
const (
	boxWide = 32.0
	boxTall = 56.0
	walkFor = 1500 * time.Millisecond
	moved   = 32.0
)

var failed int

func check(name string, ok bool, detail string) {
	if ok {
		fmt.Printf("  PASS  %-34s %s\n", name, detail)
		return
	}
	failed++
	fmt.Printf("  FAIL  %-34s %s\n", name, detail)
}

func main() {
	flag.Parse()
	if *binary == "" || *ref == "" || *lib == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded, -ref and -lib")
		os.Exit(2)
	}
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	if failed > 0 {
		os.Exit(1)
	}
}

func run() error {
	if *label != "" {
		fmt.Printf("== %s ==\n", *label)
	}
	if err := playtest.Install(*dir, *gameDir, *ref, *lib); err != nil {
		return err
	}
	srv := &playtest.Server{
		Binary: *binary, Dir: *dir, Game: *gameDir, Map: *mapName,
		Port: *port, MaxClients: 16,
		LogPath: filepath.Join(*dir, "server.log"),
		Cvars: map[string]string{
			"g_ruleset":      *ruleset,
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

	overlapped := 0
	readable := 0
	var worst []string

	for s := 1; s <= *samples; s++ {
		bots, err := seatRound(s)
		if err != nil {
			fmt.Printf("  ..    sample %d could not be seated: %v\n", s, err)
			release(bots)
			continue
		}

		pairs, live := inspect(bots)
		if live < 2 {
			fmt.Printf("  ..    sample %d: only %d live fighter(s), no reading\n", s, live)
			release(bots)
			continue
		}
		readable++

		if len(pairs) > 0 {
			overlapped++
			worst = append(worst, fmt.Sprintf("sample %d: %v", s, pairs))
			fmt.Printf("  ..    sample %d: %d live, %d OVERLAPPING pair(s) %v\n",
				s, live, len(pairs), pairs)
			// A sample that overlapped is the one worth asking to walk: that is
			// the reported symptom, "unable to move until hit with a weapon".
			walkAll(bots, s)
		} else {
			fmt.Printf("  ..    sample %d: %d live, every fighter on its own pad\n", s, live)
		}
		release(bots)

		// Let the arena notice it is empty before the next set arrives.
		time.Sleep(2 * time.Second)
	}

	if readable == 0 {
		return fmt.Errorf("no sample produced a reading; see %s",
			filepath.Join(*dir, "server.log"))
	}

	detail := fmt.Sprintf("%d of %d samples had an overlapping pair", overlapped, readable)
	if overlapped > 0 {
		detail += fmt.Sprintf(" -- %v", worst)
	}
	check("no round start overlaps two fighters", overlapped == 0, detail)
	return nil
}

// One sample's worth of clients, seated on the pickup teams of the arena under
// test and waited on until the round is actually live.  Red and Blue alternate,
// so an odd `clients` puts the extra one on Red -- which is deliberate: the
// collision this hunts is BETWEEN MEMBERS OF ONE SIDE, because that is the
// side whose parity-restricted index set is only half the pads.
func seatRound(sample int) ([]*playtest.Bot, error) {
	var bots []*playtest.Bot

	for i := 0; i < *clients; i++ {
		side := "Red"
		if i%2 == 1 {
			side = "Blue"
		}
		b := playtest.NewBot(fmt.Sprintf("s%dc%d", sample, i+1), "127.0.0.1", *port)
		if err := b.Start(30 * time.Second); err != nil {
			return bots, err
		}
		bots = append(bots, b)
		if err := ra2.JoinTeam(b, ra2.PickupTeam(*arena, side)); err != nil {
			return bots, fmt.Errorf("%s: %w", b.Name, err)
		}
	}

	// The round is live when the fighters stop being observers.  Not every
	// client of a pickup arena necessarily fights -- playersperteam and the
	// pad count both cap it -- so wait for a majority rather than for all.
	want := len(bots) / 2
	if want < 2 {
		want = 2
	}
	deadline := time.Now().Add(60 * time.Second)
	for time.Now().Before(deadline) {
		n := 0
		for _, b := range bots {
			if !b.Spectating() {
				n++
			}
		}
		if n >= want {
			for _, b := range bots {
				b.WaitFrames(10, 5*time.Second)
			}
			time.Sleep(time.Second)
			return bots, nil
		}
		time.Sleep(300 * time.Millisecond)
	}
	return bots, fmt.Errorf("round never went live")
}

// Overlapping pairs among the clients the game is running as live fighters.
// An observer is excluded: it is SOLID_NOT and noclipping, so two of them in
// one place is not the defect and counting them would be a false positive.
func inspect(bots []*playtest.Bot) ([]string, int) {
	var fighters []*playtest.Bot
	for _, b := range bots {
		if !b.Spectating() {
			fighters = append(fighters, b)
		}
	}

	for _, b := range fighters {
		o := b.Origin()
		fmt.Printf("  ..      %-8s at (%6.0f %6.0f %6.0f)\n", b.Name, o[0], o[1], o[2])
	}

	var pairs []string
	for i := 0; i < len(fighters); i++ {
		for j := i + 1; j < len(fighters); j++ {
			a, b := fighters[i].Origin(), fighters[j].Origin()
			if math.Abs(a[0]-b[0]) < boxWide && math.Abs(a[1]-b[1]) < boxWide &&
				math.Abs(a[2]-b[2]) < boxTall {
				pairs = append(pairs, fmt.Sprintf("%s/%s %.0f apart",
					fighters[i].Name, fighters[j].Name, dist(a, b)))
			}
		}
	}
	return pairs, len(fighters)
}

func walkAll(bots []*playtest.Bot, sample int) {
	for _, b := range bots {
		if b.Spectating() {
			continue
		}
		best := 0.0
		for _, yaw := range []float64{0, 180, 90} {
			from := b.Origin()
			b.Look(0, yaw, 0)
			b.Walk(400, 0, walkFor)
			if d := dist(from, b.Origin()); d > best {
				best = d
			}
			if best > moved {
				break
			}
		}
		check(fmt.Sprintf("sample %d %s can move", sample, b.Name), best > moved,
			fmt.Sprintf("best displacement over three directions = %.0f units", best))
	}
}

func release(bots []*playtest.Bot) {
	for _, b := range bots {
		b.Disconnect()
	}
	time.Sleep(500 * time.Millisecond)
}

func dist(a, b [3]float64) float64 {
	return math.Sqrt((a[0]-b[0])*(a[0]-b[0]) + (a[1]-b[1])*(a[1]-b[1]) + (a[2]-b[2])*(a[2]-b[2]))
}
