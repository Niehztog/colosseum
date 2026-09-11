// ra2botchat -- how many times does one bot chat line reach a client?
//
// The dedicated console prints a chat line once per Cmd_Say_f call, so the
// console cannot tell "the game said it four times" from "the client was sent
// it four times".  Only a client can.  This connects one, lets the bots talk,
// and counts each distinct line it received.
//
// Exit 0 no line arrived more than once, 1 some line did, 2 could not run.
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
	"time"

	"q2playtest/playtest"
	"q2playtest/ra2"
)

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "", "read-only reference install (RA2 paks + arena.cfg)")
	lib := flag.String("lib", "", "game library under test")
	glad := flag.String("glad", "", "the bot brain (gladiator.so)")
	pak7 := flag.String("pak7", "", "the brain's asset pak")
	botcfg := flag.String("botcfg", "", "botcfg directory holding bots.cfg")
	aas := flag.String("aas", "", "a .aas for the map (copied, so it may be rewritten)")
	dir := flag.String("dir", "/tmp/q2playtest/ra2botchat", "scratch install dir")
	mapname := flag.String("map", "ra2map7", "map to test on")
	arena := flag.Int("arena", 6, "the map's pickup arena")
	port := flag.Int("port", 27978, "server port")
	secs := flag.Int("listen", 150, "seconds to listen for chat")
	label := flag.String("label", "", "label for the report")
	flag.Parse()

	if *q2 == "" || *ref == "" || *lib == "" || *glad == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded, -ref, -lib and -glad")
		os.Exit(2)
	}
	dup, err := run(*q2, *ref, *lib, *glad, *pak7, *botcfg, *aas, *dir, *mapname,
		*arena, *port, *secs, *label)
	if err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	if dup > 0 {
		os.Exit(1)
	}
}

func link(src, dst string) error {
	os.Remove(dst)
	if src == "" {
		return nil
	}
	return os.Symlink(src, dst)
}

func run(q2, ref, lib, glad, pak7, botcfg, aas, dir, mapname string,
	arena, port, secs int, label string) (int, error) {

	if label != "" {
		fmt.Printf("== %s ==\n", label)
	}
	os.RemoveAll(dir)
	if err := os.MkdirAll(filepath.Join(dir, "arena", "maps"), 0o755); err != nil {
		return 0, err
	}
	if err := playtest.Install(dir, "arena", ref, lib); err != nil {
		return 0, err
	}
	g := filepath.Join(dir, "arena")
	if err := link(glad, filepath.Join(g, filepath.Base(glad))); err != nil {
		return 0, err
	}
	if err := link(pak7, filepath.Join(g, "pak7.pak")); err != nil {
		return 0, err
	}
	if err := link(botcfg, filepath.Join(g, "botcfg")); err != nil {
		return 0, err
	}
	if aas != "" {
		// a copy: the brain writes reachability back into it
		b, err := os.ReadFile(aas)
		if err != nil {
			return 0, err
		}
		if err := os.WriteFile(filepath.Join(g, "maps", mapname+".aas"), b, 0o644); err != nil {
			return 0, err
		}
	}

	srv := &playtest.Server{
		Binary: q2, Dir: dir, Game: "arena", Map: mapname, Port: port,
		LogPath: filepath.Join(dir, "server.log"),
		Cvars: map[string]string{
			"g_ruleset": "arena", "arenacfg": "arena.cfg", "admincode": "0",
			"bots": "1", "minimumplayers": "4", "arena": "0",
		},
	}
	if err := srv.Start(); err != nil {
		return 0, err
	}
	defer srv.Stop()

	if _, err := srv.WaitLog(`AAS initialized`, 180*time.Second); err != nil {
		return 0, fmt.Errorf("brain never finished loading: %w", err)
	}

	b := playtest.NewBot("witness", "127.0.0.1", port)
	if err := b.Start(60 * time.Second); err != nil {
		return 0, err
	}
	defer b.Disconnect()
	if err := ra2.JoinTeam(b, ra2.PickupTeam(arena, "Red")); err != nil {
		return 0, err
	}
	fmt.Printf("  ..    listening for %ds\n", secs)
	time.Sleep(time.Duration(secs) * time.Second)

	// a chat line is "<name>: <text>"; obituaries and mod prints are not
	chat := regexp.MustCompile(`^[^:\n]{1,32}: .+`)
	counts := map[string]int{}
	var order []string
	for _, p := range b.Prints() {
		for _, l := range strings.Split(p, "\n") {
			l = strings.TrimSpace(playtest.Decode(l))
			if l == "" || !chat.MatchString(l) {
				continue
			}
			if counts[l] == 0 {
				order = append(order, l)
			}
			counts[l]++
		}
	}
	if len(order) == 0 {
		return 0, fmt.Errorf("the client heard no chat at all in %ds", secs)
	}
	sort.SliceStable(order, func(i, j int) bool { return counts[order[i]] > counts[order[j]] })

	dup := 0
	for _, l := range order {
		mark := "PASS"
		if counts[l] > 1 {
			mark = "FAIL"
			dup++
		}
		fmt.Printf("  %s  x%-3d %s\n", mark, counts[l], l)
	}
	fmt.Printf("  ---   %d distinct line(s), %d arrived more than once\n", len(order), dup)
	return dup, nil
}
