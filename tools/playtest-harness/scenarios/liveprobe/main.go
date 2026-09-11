// liveprobe -- read-only observation of a RUNNING server.  Connects one libq2
// client, reads the playerstate and configstrings, prints them, disconnects.
// It never sends a button, because on a non-OSP ruleset a button press during
// intermission is exactly what ends the intermission (p_client.c:3462) -- the
// thing under investigation.
package main

import (
	"flag"
	"fmt"
	"os"
	"sort"
	"time"

	"q2playtest/playtest"
)

var (
	host  = flag.String("host", "127.0.0.1", "server address")
	port  = flag.Int("port", 27910, "server port")
	name  = flag.String("name", "probe", "client name")
	watch = flag.Duration("watch", 12*time.Second, "how long to observe")
)

var pmNames = map[int]string{0: "NORMAL", 1: "SPECTATOR", 2: "DEAD", 3: "GIB", 4: "FREEZE"}

func main() {
	flag.Parse()
	p := playtest.NewBot(*name, *host, *port)
	if err := p.Start(25 * time.Second); err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	defer p.Disconnect()

	f0 := p.Frame()
	o0 := p.Origin()
	time.Sleep(*watch)
	f1 := p.Frame()
	o1 := p.Origin()

	pm := p.PMType()
	fmt.Printf("pm_type      = %d (%s)\n", pm, pmNames[pm])
	fmt.Printf("frames       = %d -> %d  (%d over %s; server is %srunning)\n",
		f0, f1, f1-f0, *watch, map[bool]string{true: "", false: "NOT "}[f1 > f0])
	fmt.Printf("origin       = %.0f -> %.0f\n", o0, o1)
	fmt.Printf("spectating   = %v\n", p.Spectating())

	cs := p.ConfigStrings()
	keys := make([]int, 0, len(cs))
	for k := range cs {
		keys = append(keys, k)
	}
	sort.Ints(keys)
	fmt.Println("--- configstrings of interest ---")
	for _, k := range keys {
		if k <= 4 || k == 30 || k == 31 || k == 33 || (k >= 1312 && k < 1312+16) {
			fmt.Printf("  cs[%d] = %q\n", k, cs[k])
		}
	}

	fmt.Println("--- playerstate stats ---")
	st := p.Stats()
	sk := make([]int, 0, len(st))
	for k := range st {
		sk = append(sk, k)
	}
	sort.Ints(sk)
	for _, k := range sk {
		fmt.Printf("  stat[%d] = %d\n", k, st[k])
	}

	if l := p.Layout(); l != "" {
		fmt.Printf("--- layout (%d bytes) ---\n%s\n", len(l), l)
	} else {
		fmt.Println("--- layout: (none) ---")
	}
	fmt.Println("--- statusbar centerprints/prints ---")
	for _, s := range p.Centers() {
		fmt.Printf("  CENTER: %q\n", s)
	}
	for _, s := range p.Prints() {
		fmt.Printf("  PRINT:  %q\n", s)
	}
}
