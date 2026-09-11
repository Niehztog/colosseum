// ospthink -- does a HUMAN client's ClientThink run at all?
//
// p_client.c's ClientThink opens with
//
//	if (!(ent->flags & FL_BOTINPUT))
//	    return;
//
// where the Gladiator Bot SDK this came from has
//
//	if (ent->flags & FL_BOT)
//	    if (!(ent->flags & FL_BOTINPUT)) return;
//
// FL_BOTINPUT is set by bl_main.c around its own ClientThink calls and nowhere
// else, so on the flattened form the guard reads "return unless we are inside a
// bot's input" -- which is every real client, every frame.
//
// The witness has to be something ONLY ClientThink can produce.  Pmove is: the
// server runs it from there and nowhere else, so a client whose ClientThink
// returns early is never moved by the server at all -- it does not walk, and it
// does not even FALL.  q2dm1's spawn points are above the floor, so the
// z-drop on its own separates the two builds without needing steering.
package main

import (
	"flag"
	"fmt"
	"math"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"q2playtest/playtest"
)

var (
	engine = flag.String("engine", "q2pro", "q2pro | yq2")
	binary = flag.String("bin", "", "dedicated server binary")
	ref    = flag.String("ref", "../../../yquake2/release_", "reference install")
	lib    = flag.String("lib", "", "game library under test")
	dir    = flag.String("dir", "/tmp/ospthink", "scratch dir")
	port   = flag.Int("port", 27970, "UDP port")
	label  = flag.String("label", "run", "label")
)

func main() {
	flag.Parse()
	if *binary == "" {
		if *engine == "q2pro" {
			*binary = "../../../q2pro/builddir-test/q2proded"
		} else {
			*binary = "../../../yquake2/release/q2ded"
		}
	}
	d := filepath.Join(*dir, *engine+"-"+*label)
	os.RemoveAll(d)
	for _, sub := range []string{"baseq2", "tourney"} {
		os.MkdirAll(filepath.Join(d, sub), 0o755)
	}
	for _, p := range []string{"pak0.pak", "pak1.pak", "pak2.pak"} {
		os.Symlink(filepath.Join(*ref, "baseq2", p), filepath.Join(d, "baseq2", p))
	}
	ents, _ := os.ReadDir(filepath.Join(*ref, "tourney"))
	for _, e := range ents {
		n := e.Name()
		if strings.HasPrefix(n, "game") || strings.Contains(n, "oracle") ||
			strings.Contains(n, "v275-real") {
			continue
		}
		os.Symlink(filepath.Join(*ref, "tourney", n), filepath.Join(d, "tourney", n))
	}
	// q2pro looks for `game<cpu>`, and the caller has already built that
	// name, so the library is installed under the one it arrived with.
	// yquake2 names it `game.so` whatever the CPU, which is why the two
	// arms differ rather than sharing one spelling.
	name := filepath.Base(*lib)
	if *engine != "q2pro" {
		name = "game.so"
	}
	body, err := os.ReadFile(*lib)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(2)
	}
	os.WriteFile(filepath.Join(d, "tourney", name), body, 0o755)
	os.WriteFile(filepath.Join(d, "tourney", "maps.txt"), []byte("q2dm1\n"), 0o644)

	args := []string{"+set", "basedir", d, "+set", "game", "tourney",
		"+set", "dedicated", "1", "+set", "deathmatch", "1",
		"+set", "maxclients", "8", "+set", "timelimit", "0"}
	if *engine == "q2pro" {
		args = append(args, "+set", "homedir", d,
			"+set", "net_port", strconv.Itoa(*port), "+set", "sv_iplimit", "0")
	} else {
		args = append(args, "+set", "port", strconv.Itoa(*port))
	}
	args = append(args, "+map", "q2dm1")

	// Yamagi block-buffers stdout when it is not a tty, so without stdbuf
	// the console arrives only at exit and every wait below times out.
	bin, binArgs := *binary, args
	if _, err := exec.LookPath("stdbuf"); err == nil {
		bin, binArgs = "stdbuf", append([]string{"-oL", "-eL", *binary}, args...)
	}
	cmd := exec.Command(bin, binArgs...)
	cmd.Dir = d
	logf, _ := os.Create(filepath.Join(d, "server.log"))
	cmd.Stdout, cmd.Stderr = logf, logf
	if err := cmd.Start(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(2)
	}
	defer func() { cmd.Process.Kill(); cmd.Wait() }()
	time.Sleep(4 * time.Second)

	b := playtest.NewBot("walker", "127.0.0.1", *port)
	if err := b.Start(30 * time.Second); err != nil {
		fmt.Fprintln(os.Stderr, "client:", err)
		os.Exit(2)
	}
	for i := 0; i < 6 && b.Spectating(); i++ {
		b.Cmd("join")
		time.Sleep(1500 * time.Millisecond)
	}
	if b.Spectating() {
		fmt.Println("  could not enter the game")
		os.Exit(2)
	}

	// Let the spawn settle, then look for any server-side movement at all.
	time.Sleep(500 * time.Millisecond)
	p0 := b.Origin()
	f0 := b.Frame()
	for i := 0; i < 25; i++ {
		b.Walk(400, 0, 100*time.Millisecond)
	}
	time.Sleep(500 * time.Millisecond)
	p1 := b.Origin()
	f1 := b.Frame()

	moved := math.Abs(p1[0]-p0[0]) + math.Abs(p1[1]-p0[1]) + math.Abs(p1[2]-p0[2])
	fmt.Printf("== ospthink %s/%s ==\n", *engine, *label)
	fmt.Printf("   frames %d -> %d\n", f0, f1)
	fmt.Printf("   origin %.1f -> %.1f  (moved %.2f units)\n", p0, p1, moved)
	if moved > 1.0 {
		fmt.Println("  PASS  ClientThink runs for a human client (the server moved it)")
		return
	}
	fmt.Println("  FAIL  the server never moved this client: ClientThink returned early")
	os.Exit(1)
}
