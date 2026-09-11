// csdump -- connect once and print every configstring the server sent, so the
// numbering itself can be read.  Which index a skin or a statusbar arrives at
// is the evidence for which configstring layout the server negotiated.
package main

import (
	"flag"
	"fmt"
	"sort"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
)

func main() {
	q2 := flag.String("q2proded", "", "")
	lib := flag.String("lib", "", "")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "")
	dir := flag.String("dir", "/tmp/colosseum-csdump", "")
	rs := flag.String("ruleset", "dm", "")
	mp := flag.String("map", "q2dm1", "")
	port := flag.Int("port", 27995, "")
	flag.Parse()

	if err := colosseum.Install(*dir, *ref, *ctf, *lib); err != nil {
		panic(err)
	}
	srv := &playtest.Server{
		Binary: *q2, Dir: *dir, Game: "colosseum", Map: *mp, Port: *port,
		MaxClients: 8, Cvars: map[string]string{"g_ruleset": *rs, "cheats": "1"},
		LogPath: *dir + "/server.log",
	}
	if err := srv.Start(); err != nil {
		panic(err)
	}
	defer srv.Stop()

	b := playtest.NewBot("dump", "127.0.0.1", *port)
	if err := b.Start(30 * time.Second); err != nil {
		panic(err)
	}
	defer b.Disconnect()
	time.Sleep(2 * time.Second)

	cs := b.ConfigStrings()
	keys := make([]int, 0, len(cs))
	for k := range cs {
		keys = append(keys, k)
	}
	sort.Ints(keys)
	for _, k := range keys {
		v := playtest.Decode(cs[k])
		if len(v) > 110 {
			v = v[:110] + fmt.Sprintf("... (%d bytes)", len(cs[k]))
		}
		fmt.Printf("%6d  %q\n", k, v)
	}
	fmt.Printf("\n%d configstrings\n", len(cs))
}
