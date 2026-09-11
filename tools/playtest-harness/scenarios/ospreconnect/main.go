// ospreconnect -- does a headless client survive a level change?
//
// A level change is not a disconnect.  The server stuffs `changing` and then
// `reconnect` at every client, and `reconnect` asks for the SPAWN handshake
// again -- id's CL_Reconnect_f answers it with the string command `new`, which
// is what makes the server re-send serverdata, configstrings and baselines for
// the new level.  A client that does not answer stays CONNECTED and receives
// nothing further: it still appears in `status`, its frame counter stops, and
// every command it sends is for a level the server has left.
//
// That failure is worth a scenario of its own because of how it reads from
// outside: it is indistinguishable from a mod that has stopped talking to the
// client, and it silently caps every other scenario at one level change.
//
// THE WITNESS IS MOVEMENT, on both sides of the change.  Only the server can
// move a client, and only through ClientThink, so a client that walks after the
// change is one whose handshake completed, whose baselines are current and
// whose usercmds are being accepted.  Frame advance alone is weaker -- it can
// survive a partial handshake -- so it is reported beside the walk rather than
// instead of it.
//
// Asserted in both signs by construction: the same walk is measured BEFORE the
// change, so "it did not move afterwards" cannot be confused with a client that
// could never move at all.
package main

import (
	"bufio"
	"flag"
	"fmt"
	"io"
	"math"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"

	"q2playtest/playtest"
)

var (
	engine = flag.String("engine", "q2pro", "q2pro | yq2")
	binary = flag.String("bin", "", "dedicated server binary")
	ref    = flag.String("ref", "../../../yquake2/release_", "reference install")
	lib    = flag.String("lib", "", "game library under test")
	dir    = flag.String("dir", "/tmp/ospreconnect", "scratch dir")
	port   = flag.Int("port", 27995, "UDP port")
	label  = flag.String("label", "run", "label")
)

var pass, fail int

func ck(name string, ok bool, detail string) {
	if ok {
		pass++
		fmt.Printf("  PASS  %-34s %s\n", name, detail)
	} else {
		fail++
		fmt.Printf("  FAIL  %-34s %s\n", name, detail)
	}
}

type srv struct {
	cmd   *exec.Cmd
	stdin io.WriteCloser
	mu    sync.Mutex
	lines []string
}

func (s *srv) console(f string, a ...any) { fmt.Fprintf(s.stdin, f+"\n", a...) }

func (s *srv) count(re string) int {
	r := regexp.MustCompile(re)
	s.mu.Lock()
	defer s.mu.Unlock()
	n := 0
	for _, l := range s.lines {
		if r.MatchString(l) {
			n++
		}
	}
	return n
}

func (s *srv) waitCount(re string, n int, d time.Duration) bool {
	deadline := time.Now().Add(d)
	for time.Now().Before(deadline) {
		if s.count(re) >= n {
			return true
		}
		time.Sleep(50 * time.Millisecond)
	}
	return false
}

// walk asks the server to move the client and reports how far it got.
func walk(b *playtest.Bot) float64 {
	time.Sleep(500 * time.Millisecond)
	p0 := b.Origin()
	for i := 0; i < 20; i++ {
		b.Walk(400, 0, 100*time.Millisecond)
	}
	time.Sleep(400 * time.Millisecond)
	p1 := b.Origin()
	return math.Abs(p1[0]-p0[0]) + math.Abs(p1[1]-p0[1]) + math.Abs(p1[2]-p0[2])
}

func enter(b *playtest.Bot) {
	for i := 0; i < 6 && b.Spectating(); i++ {
		b.Cmd("join")
		time.Sleep(1200 * time.Millisecond)
	}
}

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
	os.WriteFile(filepath.Join(d, "tourney", "maps.txt"), []byte("q2dm1\nq2dm5\n"), 0o644)

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

	// Yamagi block-buffers stdout when it is not a tty.
	bin, binArgs := *binary, args
	if _, err := exec.LookPath("stdbuf"); err == nil {
		bin, binArgs = "stdbuf", append([]string{"-oL", "-eL", *binary}, args...)
	}
	s := &srv{}
	s.cmd = exec.Command(bin, binArgs...)
	s.cmd.Dir = d
	out, _ := s.cmd.StdoutPipe()
	s.cmd.Stderr = s.cmd.Stdout
	s.stdin, _ = s.cmd.StdinPipe()
	if err := s.cmd.Start(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(2)
	}
	defer func() { s.cmd.Process.Kill(); s.cmd.Wait() }()
	logf, _ := os.Create(filepath.Join(d, "server.log"))
	go func() {
		sc := bufio.NewScanner(out)
		sc.Buffer(make([]byte, 64*1024), 1024*1024)
		for sc.Scan() {
			l := sc.Text()
			s.mu.Lock()
			s.lines = append(s.lines, l)
			s.mu.Unlock()
			fmt.Fprintln(logf, l)
		}
	}()
	s.waitCount(`SpawnServer: |Loading map: `, 1, 30*time.Second)
	time.Sleep(2 * time.Second)

	fmt.Printf("== ospreconnect %s/%s ==\n", *engine, *label)

	b := playtest.NewBot("survivor", "127.0.0.1", *port)
	if err := b.Start(30 * time.Second); err != nil {
		fmt.Fprintln(os.Stderr, "client:", err)
		os.Exit(2)
	}
	enter(b)

	// The control: this client can be moved BEFORE the change.
	d0 := walk(b)
	ck("before/client-walks", d0 > 1.0, fmt.Sprintf("moved %.0f units", d0))
	f0 := b.Frame()

	// A level change that keeps the connection: `gamemap`, not `map`.
	baseSpawn := s.count(`SpawnServer: |Loading map: `)
	s.console("gamemap q2dm5")
	if !s.waitCount(`SpawnServer: |Loading map: `, baseSpawn+1, 30*time.Second) {
		ck("change/level-changed", false, "the server never changed level")
		os.Exit(1)
	}
	ck("change/level-changed", true, "server is on the next map")
	time.Sleep(4 * time.Second)

	// Still in the server's own client list: this separates "dropped" from
	// "connected but deaf", which are different bugs with the same symptom.
	s.console("status")
	stillListed := s.waitCount(`survivor`, 2, 5*time.Second)
	ck("after/still-connected", stillListed, "client still in `status`")

	// Measured WITHIN the new level, not across the change: a server restarts
	// its frame numbering per level, so the new level's early frames are
	// numerically BELOW the old level's last one and a straight comparison
	// reports a working client as broken.  Wait for the handshake to land
	// first -- until it does, FrameNum still holds the old level's last value,
	// and sampling then times the wait rather than the client.
	t0 := time.Now()
	landed := false
	for time.Since(t0) < 20*time.Second {
		if b.Frame() != f0 {
			landed = true
			break
		}
		time.Sleep(100 * time.Millisecond)
	}
	ck("after/handshake-landed", landed,
		fmt.Sprintf("new frames after %.1fs", time.Since(t0).Seconds()))

	f1 := b.Frame()
	time.Sleep(2 * time.Second)
	f2 := b.Frame()
	ck("after/frames-advance", f2 > f1,
		fmt.Sprintf("client frame %d -> %d on the new level (was %d on the old)", f1, f2, f0))

	// The real question.  A client that walks here re-did the handshake,
	// re-received its baselines and is having its usercmds accepted again.
	enter(b)
	d1 := walk(b)
	ck("after/client-walks", d1 > 1.0, fmt.Sprintf("moved %.0f units", d1))

	fmt.Printf("---- %s/%s: %d passed, %d failed ----\n", *engine, *label, pass, fail)
	if fail > 0 {
		os.Exit(1)
	}
}
