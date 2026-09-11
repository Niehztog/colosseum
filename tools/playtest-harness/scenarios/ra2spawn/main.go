// ra2spawn checks whether Rocket Arena places the fighters of a round away
// from each other, or away from the audience watching them.
//
// SelectFarthestArenaSpawnPoint puts each arriving fighter on the spawn point
// farthest from "any player".  Observers are alive and noclipping, and RA2
// parks them on the arena's own spawn points, so ranging that counts them
// picks spawns by where the audience stood.
//
// Two checks, both invariants rather than judgement calls.
//
//  1. Observers are not players to avoid.  Every observer seated under the same
//     conditions is ranged against the same set, so they all land on the same
//     spawn point.  Ranging that counts observers makes each one avoid the last,
//     spreading them over distinct spots.
//
//  2. Fighters are placed apart.  When a round starts the mod places the two
//     fighters one after the other: the first has nobody to avoid, so the second
//     must land on whichever spawn point is farthest from the first.
//
// Check 1 is the reliable detector.  Check 2 is necessary but not always
// violated: whether counting the audience changes the outcome depends on where
// the audience happens to be standing, so it can pass by coincidence.
//
// Seating the audience takes a detour, because a round starts the moment two
// teams are queued.  Two starter clients open a round, the audience joins
// behind them as observers, and then the starters disconnect -- which ends the
// round and starts a fresh one with the audience already standing on the
// arena's spawn points, which is the situation under test.
//
// Pick an arena with several info_player_deathmatch and no
// misc_teleporter_dest, so observers and fighters share one spot set; the
// mapinfo scenario lists them.
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"time"

	"q2playtest/playtest"
	"q2playtest/ra2"
)

var (
	binary    = flag.String("q2proded", "", "path to the q2proded binary")
	ref       = flag.String("ref", "", "read-only reference install holding the mod's paks and cfg")
	lib       = flag.String("lib", "", "game library to test")
	dir       = flag.String("dir", "", "scratch install directory to build")
	gameDir   = flag.String("game", "arena", "mod directory name")
	mapName   = flag.String("map", "ra2map19", "map to run")
	arenaName = flag.String("arena", "Diet Smack", "display name of the arena to test")
	audience  = flag.Int("audience", 3, "observers to seat in the arena before the round")
	port      = flag.Int("port", 27960, "server UDP port")
	label     = flag.String("label", "", "name for this run in the output")
	ruleset   = flag.String("ruleset", "arena", "g_ruleset for a library that serves several; ignored by one that serves only RA2")
	settle    = flag.Duration("settle", 8*time.Second, "how long to let a round start before sampling")
)

const joinTimeout = 30 * time.Second

func main() {
	flag.Parse()
	for name, v := range map[string]*string{"q2proded": binary, "ref": ref, "lib": lib, "dir": dir} {
		if *v == "" {
			fmt.Fprintf(os.Stderr, "-%s is required\n", name)
			os.Exit(2)
		}
	}
	ok, err := run()
	if err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	if !ok {
		os.Exit(1)
	}
}

func run() (bool, error) {
	pakGlob := filepath.Join(*ref, "pak*.pak")
	arena, err := playtest.ArenaNumber(pakGlob, *mapName, *arenaName)
	if err != nil {
		return false, err
	}
	spawns, err := playtest.MapEntities(pakGlob, *mapName, "info_player_deathmatch")
	if err != nil {
		return false, err
	}
	var spots []playtest.Spawn
	for _, s := range spawns {
		if s.Arena == arena {
			spots = append(spots, s)
		}
	}

	if err := playtest.Install(*dir, *gameDir, *ref, *lib); err != nil {
		return false, fmt.Errorf("install: %w", err)
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
		return false, err
	}
	defer srv.Stop()

	name := *label
	if name == "" {
		name = filepath.Base(*lib)
	}
	fmt.Printf("=== %s ===\n", name)
	fmt.Printf("    %s arena %d %q, %d spawn points, audience %d\n",
		*mapName, arena, *arenaName, len(spots), *audience)

	// two starters open a round so that everyone after them arrives as an
	// observer
	var starters []*playtest.Bot
	for i := 0; i < 2; i++ {
		b, err := seat(fmt.Sprintf("starter%d", i+1), *port, *arenaName)
		if err != nil {
			return false, err
		}
		starters = append(starters, b)
	}
	time.Sleep(*settle)

	var crowd []*playtest.Bot
	for i := 0; i < *audience; i++ {
		b, err := seat(fmt.Sprintf("watcher%d", i+1), *port, *arenaName)
		if err != nil {
			return false, err
		}
		crowd = append(crowd, b)
	}
	time.Sleep(*settle)

	seatedOn := map[string]int{}
	for _, b := range crowd {
		if !b.Spectating() {
			fmt.Printf("    note: %s is not spectating (pm_type %d); the arena may have started a round early\n",
				b.Name, b.PMType())
		}
		spot, _ := playtest.Nearest(spots, arena, b.Origin())
		seatedOn[spot.Name]++
		fmt.Printf("    watching %-9s at %s on %s\n", b.Name, fmtv(b.Origin()), spotName(spots, b.Origin()))
	}

	// check 1
	check1 := len(seatedOn) <= 1
	if len(crowd) < 2 {
		fmt.Printf("    CHECK1 skipped: needs at least 2 observers\n")
		check1 = true
	} else if check1 {
		fmt.Printf("    CHECK1 pass: all %d observers were placed on one spot -- they do not repel each other\n", len(crowd))
	} else {
		fmt.Printf("    CHECK1 FAIL: %d observers spread over %d spots %s -- each is avoiding the last, so observers count as players\n",
			len(crowd), len(seatedOn), sortedKeys(seatedOn))
	}

	// end the round: with a team emptied the arena rolls into the next one,
	// and the audience's teams are next in the queue
	fmt.Printf("    -- starters leave, next round begins with the audience seated --\n")
	for _, b := range starters {
		b.Disconnect()
	}
	time.Sleep(*settle + 6*time.Second)

	var fighters, observers []*playtest.Bot
	for _, b := range crowd {
		if b.Spectating() {
			observers = append(observers, b)
		} else {
			fighters = append(fighters, b)
		}
	}
	sort.Slice(fighters, func(i, j int) bool { return fighters[i].Name < fighters[j].Name })

	for _, b := range observers {
		fmt.Printf("    observer %-9s at %s on %s\n", b.Name, fmtv(b.Origin()), spotName(spots, b.Origin()))
	}
	for _, b := range fighters {
		fmt.Printf("    FIGHTER  %-9s at %s on %s\n", b.Name, fmtv(b.Origin()), spotName(spots, b.Origin()))
	}
	if len(fighters) != 2 {
		return false, fmt.Errorf("expected 2 fighters in the new round, got %d", len(fighters))
	}

	// the invariant
	a, b := fighters[0].Origin(), fighters[1].Origin()
	sep := playtest.Dist(a, b)
	fromA, spotA := farthest(spots, a)
	fromB, spotB := farthest(spots, b)
	tol := 40.0 // the placement traces down onto the floor

	fmt.Printf("    separation %.0f;  farthest spot from %s is %s at %.0f, from %s is %s at %.0f\n",
		sep, fighters[0].Name, spotA, fromA, fighters[1].Name, spotB, fromB)

	check2 := sep+tol >= fromA || sep+tol >= fromB
	if check2 {
		fmt.Printf("    CHECK2 pass: a fighter is on the spawn point farthest from the other\n")
	} else {
		fmt.Printf("    CHECK2 FAIL: neither fighter is on the spawn point farthest from the other -- "+
			"%.0f apart where %.0f was available, so the audience decided this\n", sep, max(fromA, fromB))
	}

	if check1 && check2 {
		fmt.Printf("    PASS\n")
	} else {
		fmt.Printf("    FAIL\n")
	}
	return check1 && check2, nil
}

func sortedKeys(m map[string]int) []string {
	out := make([]string, 0, len(m))
	for k := range m {
		out = append(out, k)
	}
	sort.Strings(out)
	return out
}

func seat(name string, port int, arena string) (*playtest.Bot, error) {
	b := playtest.NewBot(name, "127.0.0.1", port)
	if err := b.Start(joinTimeout); err != nil {
		return nil, err
	}
	if err := ra2.NewTeamInArena(b, arena); err != nil {
		return nil, err
	}
	b.WaitFrames(10, 5*time.Second)
	return b, nil
}

// farthest returns the distance to the arena spawn point farthest from pos,
// and its name -- the best a "place them apart" rule could have done.
func farthest(spots []playtest.Spawn, pos [3]float64) (float64, string) {
	best, name := 0.0, "?"
	for _, s := range spots {
		if d := playtest.Dist(s.Origin, pos); d > best {
			best, name = d, s.Name
		}
	}
	return best, name
}

func spotName(spots []playtest.Spawn, pos [3]float64) string {
	best, name, d := 1e18, "?", 0.0
	for _, s := range spots {
		if dd := playtest.Dist(s.Origin, pos); dd < best {
			best, name, d = dd, s.Name, dd
		}
	}
	return fmt.Sprintf("%s(+%.0f)", name, d)
}

func max(a, b float64) float64 {
	if a > b {
		return a
	}
	return b
}

func fmtv(v [3]float64) string {
	return fmt.Sprintf("(%6.0f %6.0f %5.0f)", v[0], v[1], v[2])
}
