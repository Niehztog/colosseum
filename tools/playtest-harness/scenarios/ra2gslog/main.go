// ra2gslog -- does Rocket Arena's stdlog record what it claims to?
//
// `logfile 2` turns RA2's own round log on, and gslog.c has six entry points:
// GameStart, MAP, PlayerConnect, Kill/Suicide, PlayerLeft, GameEnd.  A file
// that is complete and correctly ported can still be half wired -- a call site
// dropped in a merge leaves the function compiling, linking and never running,
// and nothing but the log's own content can tell.
//
// Exit 0 all lines present, 1 a line missing, 2 the scenario could not run.
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

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "", "read-only reference install (RA2 paks + arena.cfg)")
	lib := flag.String("lib", "", "game library under test")
	dir := flag.String("dir", "/tmp/q2playtest/ra2gslog", "scratch install dir")
	mapname := flag.String("map", "ra2map7", "map to test on")
	arena := flag.Int("arena", 6, "the map's pickup arena")
	port := flag.Int("port", 27968, "server port")
	label := flag.String("label", "", "label for the report")
	flag.Parse()

	if *q2 == "" || *ref == "" || *lib == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded, -ref and -lib")
		os.Exit(2)
	}
	bad, err := run(*q2, *ref, *lib, *dir, *mapname, *arena, *port, *label)
	if err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	if bad > 0 {
		os.Exit(1)
	}
}

func run(q2, ref, lib, dir, mapname string, arena, port int, label string) (int, error) {
	if label != "" {
		fmt.Printf("== %s ==\n", label)
	}
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return 0, err
	}
	if err := playtest.Install(dir, "arena", ref, lib); err != nil {
		return 0, err
	}
	log := filepath.Join(dir, "arena", "stdlog.log")
	os.Remove(log)

	srv := &playtest.Server{
		Binary: q2, Dir: dir, Game: "arena", Map: mapname, Port: port,
		LogPath: filepath.Join(dir, "server.log"),
		Cvars: map[string]string{
			"g_ruleset": "arena", "arenacfg": "arena.cfg", "bots": "0",
			"minimumplayers": "0", "admincode": "0",
			"logfile": "2", "logname": "stdlog.log",
		},
	}
	if err := srv.Start(); err != nil {
		return 0, err
	}
	defer srv.Stop()

	b := playtest.NewBot("logger", "127.0.0.1", port)
	if err := b.Start(30 * time.Second); err != nil {
		return 0, err
	}
	if err := ra2.JoinTeam(b, ra2.PickupTeam(arena, "Red")); err != nil {
		return 0, err
	}
	b.WaitFrames(20, 10*time.Second)
	b.Disconnect()
	time.Sleep(1500 * time.Millisecond)

	raw, err := os.ReadFile(log)
	if err != nil {
		return 0, fmt.Errorf("no stdlog at %s: %w", log, err)
	}
	text := string(raw)

	want := []struct{ what, needle string }{
		{"MAP line", "\tMAP\t"},
		{"GameStart", "GameStart"},
		{"PlayerConnect", "PlayerConnect\tlogger"},
		{"PlayerLeft", "PlayerLeft\tlogger"},
	}
	bad := 0
	for _, w := range want {
		if strings.Contains(text, w.needle) {
			fmt.Printf("  PASS  %-16s\n", w.what)
		} else {
			bad++
			fmt.Printf("  FAIL  %-16s (no %q in the log)\n", w.what, w.needle)
		}
	}
	fmt.Println("  --- stdlog.log ---")
	for _, l := range strings.Split(strings.TrimRight(text, "\n"), "\n") {
		fmt.Printf("      %s\n", strings.ReplaceAll(l, "\t", " | "))
	}
	return bad, nil
}
