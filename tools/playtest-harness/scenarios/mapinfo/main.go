// mapinfo lists a map's spawn points grouped by the arena they belong to,
// reading the BSP straight out of a pak.  Rocket Arena stamps each spawn point
// with an "arena" key; a mod that spreads several arenas across one map picks
// spawns per arena, so the per-arena counts are what bound its choices.
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"

	"github.com/packetflinger/libq2/bsp"
	"github.com/packetflinger/libq2/pak"
)

var (
	pakGlob = flag.String("paks", "", "glob of pak files to search")
	mapName = flag.String("map", "", "map name without extension; empty means every map")
	classes = flag.String("classes", "info_player_deathmatch,misc_teleporter_dest", "comma-separated spawn classnames")
	names   = flag.Bool("names", false, "also print each arena's name, which is what its menu row says")
)

func main() {
	flag.Parse()
	paks, err := filepath.Glob(*pakGlob)
	if err != nil || len(paks) == 0 {
		fmt.Fprintf(os.Stderr, "no paks matched %q\n", *pakGlob)
		os.Exit(1)
	}
	want := strings.Split(*classes, ",")

	for _, p := range paks {
		data, err := os.ReadFile(p)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			continue
		}
		archive, err := pak.Unmarshal(data)
		if err != nil {
			fmt.Fprintln(os.Stderr, err)
			continue
		}
		for _, f := range archive.GetFiles() {
			name := f.GetName()
			if !strings.HasSuffix(name, ".bsp") {
				continue
			}
			base := strings.TrimSuffix(filepath.Base(name), ".bsp")
			if *mapName != "" && base != *mapName {
				continue
			}
			report(base, f.GetData(), want)
		}
	}
}

func report(name string, data []byte, want []string) {
	tmp, err := os.CreateTemp("", "mapinfo-*.bsp")
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		return
	}
	defer os.Remove(tmp.Name())
	if _, err := tmp.Write(data); err != nil {
		fmt.Fprintln(os.Stderr, err)
		return
	}
	tmp.Close()

	b, err := bsp.OpenBSPFile(tmp.Name())
	if err != nil {
		fmt.Fprintln(os.Stderr, name, err)
		return
	}
	defer b.Close()

	// counts[class][arena]
	counts := map[string]map[int]int{}
	for _, e := range b.FetchEntities() {
		for _, c := range want {
			if e.Class != c {
				continue
			}
			arena, _ := strconv.Atoi(e.Values["arena"])
			if counts[c] == nil {
				counts[c] = map[int]int{}
			}
			counts[c][arena]++
		}
	}
	if *names {
		// getarenaname() reads the message of the info_player_intermission
		// belonging to that arena, and that string is the arena's menu row
		byArena := map[int]string{}
		for _, e := range b.FetchEntities() {
			if e.Class != "info_player_intermission" {
				continue
			}
			a, _ := strconv.Atoi(e.Values["arena"])
			if m := e.Values["message"]; m != "" {
				byArena[a] = m
			}
		}
		keys := []int{}
		for a := range byArena {
			keys = append(keys, a)
		}
		sort.Ints(keys)
		for _, a := range keys {
			fmt.Printf("%-10s arena %2d name %q\n", name, a, byArena[a])
		}
	}

	for _, c := range want {
		if counts[c] == nil {
			continue
		}
		arenas := []int{}
		for a := range counts[c] {
			arenas = append(arenas, a)
		}
		sort.Ints(arenas)
		var parts []string
		for _, a := range arenas {
			mark := ""
			if counts[c][a] == 1 {
				mark = "  <-- single spot"
			}
			parts = append(parts, fmt.Sprintf("arena %2d: %2d%s", a, counts[c][a], mark))
		}
		fmt.Printf("%-10s %-24s %s\n", name, c, strings.Join(parts, "  "))
	}
}
