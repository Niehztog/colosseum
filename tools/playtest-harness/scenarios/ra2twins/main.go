// ra2twins -- do two clients arriving in the same place end up inside each
// other, and can they walk out again?
//
// Rocket Arena places every arriving client itself, picking the spot farthest
// from the players it counts.  Which players it counts is the whole question:
// a picker that measures against nobody has nothing to be farthest from, falls
// through to a random spot, and random spots collide.  Two clients on one spot
// is not cosmetic -- an RA2 observer in NORMAL mode is SOLID_BBOX on
// MOVETYPE_WALK, so each one's pmove is allsolid inside the other and neither
// can move.  RA2's own separator cannot help: KillBox returns early for a
// FIGHT_SPECTATING client and check_telefrag skips one, so nothing shoves them
// apart and no telefrag fires.
//
// So this seats N clients, reports who overlaps whom, and then asks every one
// of them to walk.  A client who cannot move in any of three directions is
// trapped, and that is the failure this scenario exists to catch.
//
// Exit 0 all checks passed, 1 a check failed, 2 the scenario could not run.
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
	binary    = flag.String("q2proded", "", "path to the q2proded binary")
	ref       = flag.String("ref", "", "read-only reference install holding the mod's paks and cfg")
	lib       = flag.String("lib", "", "game library to test")
	dir       = flag.String("dir", "/tmp/q2playtest/ra2twins", "scratch install directory to build")
	gameDir   = flag.String("game", "arena", "mod directory name")
	mapName   = flag.String("map", "ra2map9", "map to run")
	arenaName = flag.String("arena", "Medieval", "display name of the arena to join")
	clients   = flag.Int("clients", 4, "clients to seat")
	port      = flag.Int("port", 27980, "server UDP port")
	ruleset   = flag.String("ruleset", "arena", "g_ruleset for a library that serves several; ignored by one that serves only RA2")
	label     = flag.String("label", "", "label for the report")
)

// A player's bounding box is 32x32 wide and 56 tall.  Two boxes whose centres
// are closer than that in both axes are interpenetrating, not merely adjacent.
const (
	boxWide = 32.0
	boxTall = 56.0
	walkFor = 1500 * time.Millisecond
	moved   = 32.0 // a free client covers ~600 units in walkFor; 32 is generous
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
		// A library that serves ONE mod is selected by its gamedir; one that
		// serves several is not.  Colosseum picks its ruleset from `g_ruleset`
		// and boots `dm` without it -- where there is no arena, no team menu and
		// nothing for the choreography below to click, which reads from here as
		// "the client never got a menu".  Harmless against a standalone RA2
		// library, which has no such cvar to read.
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

	var bots []*playtest.Bot
	for i := 0; i < *clients; i++ {
		b := playtest.NewBot(fmt.Sprintf("client%d", i+1), "127.0.0.1", *port)
		if err := b.Start(30 * time.Second); err != nil {
			return err
		}
		defer b.Disconnect()
		if err := ra2.NewTeamInArena(b, *arenaName); err != nil {
			return fmt.Errorf("%s: %w", b.Name, err)
		}
		bots = append(bots, b)
		b.WaitFrames(10, 5*time.Second)
	}
	time.Sleep(time.Second)

	// ---- 1. who is standing inside whom -------------------------------------
	for _, b := range bots {
		o := b.Origin()
		fmt.Printf("  ..    %-8s at (%6.0f %6.0f %6.0f)\n", b.Name, o[0], o[1], o[2])
	}
	var pairs []string
	for i := 0; i < len(bots); i++ {
		for j := i + 1; j < len(bots); j++ {
			a, b := bots[i].Origin(), bots[j].Origin()
			if math.Abs(a[0]-b[0]) < boxWide && math.Abs(a[1]-b[1]) < boxWide &&
				math.Abs(a[2]-b[2]) < boxTall {
				pairs = append(pairs, fmt.Sprintf("%s/%s %.0f apart",
					bots[i].Name, bots[j].Name, dist(a, b)))
			}
		}
	}
	detail := "every client got its own spot"
	if len(pairs) > 0 {
		detail = fmt.Sprintf("%d overlapping pair(s): %v", len(pairs), pairs)
	}
	check("no two clients share a spot", len(pairs) == 0, detail)

	// ---- 2. can each of them walk out ---------------------------------------
	// Three directions, because a client placed facing a wall is not trapped.
	for _, b := range bots {
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
		check(b.Name+" can move", best > moved,
			fmt.Sprintf("best displacement over three directions = %.0f units", best))
	}
	return nil
}

func dist(a, b [3]float64) float64 {
	return math.Sqrt((a[0]-b[0])*(a[0]-b[0]) + (a[1]-b[1])*(a[1]-b[1]) + (a[2]-b[2])*(a[2]-b[2]))
}
