// ra2join is a smoke test: seat N clients in one arena and check that every one
// of them ends up somewhere sane.
//
// It is deliberately undemanding about *which* spot each client gets, so it
// works on a pickup arena ("idarena") too, where the mod picks spawns by side
// with SelectRandomArenaSpawnPoint rather than by distance.  That path indexes
// a spot list by a computed selection, so what this checks is that everybody
// lands on a real spawn point belonging to the right arena and the server is
// still alive afterwards.
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"strings"
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
	mapName   = flag.String("map", "ra2map9", "map to run")
	arenaName = flag.String("arena", "Medieval", "display name of the arena to join")
	clients   = flag.Int("clients", 4, "clients to seat")
	teamRows  = flag.String("teams", "", "comma-separated team menu rows to join round-robin; "+
		"empty means each client starts its own team and picks the arena. A pickup arena "+
		"must be entered this way, e.g. -teams '#1 Pickup Red,#1 Pickup Blue'")
	port   = flag.Int("port", 27960, "server UDP port")
	ruleset = flag.String("ruleset", "arena", "g_ruleset for a library that serves several; ignored by one that serves only RA2")
	settle = flag.Duration("settle", 8*time.Second, "how long to let the arena settle")
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
	spawns, err := playtest.MapEntities(pakGlob, *mapName, "info_player_deathmatch", "misc_teleporter_dest")
	if err != nil {
		return false, err
	}

	if err := playtest.Install(*dir, *gameDir, *ref, *lib); err != nil {
		return false, err
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

	fmt.Printf("=== %s: %s arena %d %q, %d clients ===\n",
		filepath.Base(*lib), *mapName, arena, *arenaName, *clients)

	var bots []*playtest.Bot
	for i := 0; i < *clients; i++ {
		b := playtest.NewBot(fmt.Sprintf("client%d", i+1), "127.0.0.1", *port)
		if err := b.Start(joinTimeout); err != nil {
			return false, err
		}
		if err := join(b, i); err != nil {
			return false, err
		}
		bots = append(bots, b)
		b.WaitFrames(10, 5*time.Second)
	}
	time.Sleep(*settle)

	pass := true
	for _, b := range bots {
		pos := b.Origin()
		spot, off := playtest.Nearest(spawns, arena, pos)
		role := "fighter"
		if b.Spectating() {
			role = "observer"
		}
		// 128 units covers the drop onto the floor plus a spectator drifting a
		// little after being placed
		ok := pos != [3]float64{} && off < 128
		if !ok {
			pass = false
		}
		fmt.Printf("    %-8s %-8s at (%6.0f %6.0f %5.0f)  nearest %s of arena %d, %.0f away  %s\n",
			b.Name, role, pos[0], pos[1], pos[2], spot.Name, arena, off, mark(ok))
	}

	// the server must still be up: a bad spawn selection in this mod indexes
	// past the end of the spot list and dereferences NULL, which shows up here
	// as a dead server rather than as a bad position
	if _, err := srv.WaitLog(`.`, 500*time.Millisecond); err != nil {
		// no new output is fine; a crash is not
	}
	if err := srv.Console("status"); err != nil {
		fmt.Printf("    FAIL: server is gone (%v)\n", err)
		return false, nil
	}
	if _, err := srv.WaitLog(`(?i)map\s*:|players|num score`, 5*time.Second); err != nil {
		fmt.Printf("    FAIL: server did not answer a console command -- likely crashed\n")
		return false, nil
	}
	fmt.Printf("    server still answering the console\n")

	if pass {
		fmt.Printf("    PASS\n")
	} else {
		fmt.Printf("    FAIL: a client is not on any spawn point of arena %d\n", arena)
	}
	return pass, nil
}

func join(b *playtest.Bot, i int) error {
	rows := splitRows(*teamRows)
	if len(rows) == 0 {
		return ra2.NewTeamInArena(b, *arenaName)
	}
	return ra2.JoinTeam(b, rows[i%len(rows)])
}

func splitRows(s string) []string {
	var out []string
	for _, r := range strings.Split(s, ",") {
		if r = strings.TrimSpace(r); r != "" {
			out = append(out, r)
		}
	}
	return out
}

func mark(ok bool) string {
	if ok {
		return "ok"
	}
	return "<-- BAD"
}
