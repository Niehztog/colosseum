// ra2menuleak -- does a Rocket Arena respawn free the menu it throws away?
//
// PutClientInServer clears the client the way baseq2 does, by memset-ing the
// whole gclient_t and copying `pers` and `resp` back over the hole.  RA2 keeps
// its menu queue head -- and the two pointers into it -- OUTSIDE both of those,
// so a menu that is open when the memset runs is not closed, it is forgotten:
// the nodes stay allocated, still pointing back at a head that no longer knows
// about them, until the level ends and TAG_LEVEL takes them.
//
// And a menu is open on every one of those, because PutClientInServer ITSELF
// ends with move_to_arena(..., 1), which reopens the observer menu.  So each
// respawn forgets the menu the previous respawn opened.
//
// The invariant this asserts is not a number of bytes, which is a property of
// how wide the observer menu happens to be on this map.  It is that the count
// is FLAT: RA2 holds one menu per client, so N deaths must cost the same as
// one.  A build that forgets them instead grows by a menu per death, forever.
//
// q2pro counts every game-library allocation in `z_stats` under "game", which
// makes the block count readable from outside the library.  The same defect and
// the same shape of fix are in q2pro's own CTF: PutClientInServer there calls
// PMenu_Close() before the memset for exactly this reason.
//
// Exit 0 the count was flat, 1 it grew, 2 the scenario could not run.
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"time"

	"q2playtest/playtest"
	"q2playtest/ra2"
)

// the "game" row of z_stats: bytes, blocks, name
var reGameRow = regexp.MustCompile(`^\s*(\d+)\s+(\d+)\s+game\s*$`)

type usage struct {
	bytes  int
	blocks int
}

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "", "read-only reference install (RA2 paks + arena.cfg)")
	lib := flag.String("lib", "", "game library under test")
	dir := flag.String("dir", "/tmp/q2playtest/ra2menuleak", "scratch install dir")
	mapname := flag.String("map", "ra2map7", "map to test on")
	arena := flag.Int("arena", 6, "the map's pickup arena")
	respawns := flag.Int("respawns", 4, "respawns to measure over")
	port := flag.Int("port", 27960, "server port")
	label := flag.String("label", "", "label for the report")
	flag.Parse()

	if *q2 == "" || *ref == "" || *lib == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded, -ref and -lib")
		os.Exit(2)
	}
	if err := run(*q2, *ref, *lib, *dir, *mapname, *arena, *respawns, *port, *label); err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	if failed > 0 {
		os.Exit(1)
	}
}

var failed int

func check(name string, ok bool, detail string) {
	if ok {
		fmt.Printf("  PASS  %-30s %s\n", name, detail)
		return
	}
	failed++
	fmt.Printf("  FAIL  %-30s %s\n", name, detail)
}

func run(q2, ref, lib, dir, mapname string, arena, respawns, port int, label string) error {
	if label != "" {
		fmt.Printf("== %s ==\n", label)
	}
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return err
	}
	if err := playtest.Install(dir, "arena", ref, lib); err != nil {
		return err
	}

	srv := &playtest.Server{
		Binary: q2, Dir: dir, Game: "arena", Map: mapname, Port: port,
		LogPath: filepath.Join(dir, "server.log"),
		Cvars: map[string]string{
			"g_ruleset":      "arena",
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

	// Two fighters: the subject that respawns, and one on the other side so the
	// arena has a round to be in.  They stay on opposite teams so neither side
	// is ever wiped and no round end mixes its own placements into the samples.
	seat := func(name, side string) (*playtest.Bot, error) {
		b := playtest.NewBot(name, "127.0.0.1", port)
		if err := b.Start(30 * time.Second); err != nil {
			return nil, err
		}
		if err := ra2.JoinTeam(b, ra2.PickupTeam(arena, side)); err != nil {
			return nil, fmt.Errorf("%s: %w", name, err)
		}
		return b, nil
	}
	subject, err := seat("red1", "Red")
	if err != nil {
		return err
	}
	defer subject.Disconnect()
	other, err := seat("blue1", "Blue")
	if err != nil {
		return err
	}
	defer other.Disconnect()

	if err := waitFor(60*time.Second, func() bool {
		return !subject.Spectating() && !other.Spectating()
	}); err != nil {
		return fmt.Errorf("round never started: %w", err)
	}
	fmt.Println("  ..    round is live, both are fighters")

	// The driver.  RA2 dispatches no `kill` -- Cmd_Kill_f is q_unused -- and a
	// bot cannot shoot its way to a death, so the respawn is reached the other
	// way PutClientInServer is: `spectator 1` in the userinfo makes
	// pers.spectator disagree with resp.spectator, and ClientBeginServerFrame
	// answers that with spectator_respawn() five seconds later.  It is the same
	// PutClientInServer a death reaches through respawn(), and it lands with the
	// observer menu open because PutClientInServer's own last statement --
	// move_to_arena(..., 1) -- is what opens it.
	//
	// A userinfo update is one unreliable-channel message and the gate it opens
	// is only re-read once every five seconds, so resend until the server says
	// it landed.  Retrying the trigger cannot make a leaking build look clean:
	// the count below is a separate observation and holds however many attempts
	// it took to get there.
	var spectErr error
	for i := 0; i < 5; i++ {
		subject.SetUserinfo("spectator", "1")
		if _, spectErr = subject.WaitPrint(`moved to the sidelines`, 12*time.Second); spectErr == nil {
			break
		}
	}
	if spectErr != nil {
		return fmt.Errorf("spectator_respawn never ran: %w", spectErr)
	}
	// let the placement and the menu it opens settle
	subject.WaitFrames(20, 10*time.Second)

	base, err := gameUsage(srv)
	if err != nil {
		return err
	}
	fmt.Printf("  ..    after the first respawn: %d blocks, %d bytes\n", base.blocks, base.bytes)

	// Every further respawn opens one observer menu and lets go of the one
	// before it, so the count is flat on a build that frees what it drops.
	//
	// THE DRIVER HAS TO KEEP ASKING.  An earlier version of this waited for the
	// respawn to repeat on its own, on the reading that spectator_respawn runs
	// for as long as pers.spectator disagrees with resp.spectator -- it does
	// not: PutClientInServer sets `resp.spectator = pers.spectator` on both of
	// its arms, so one userinfo change buys exactly one respawn and the wait
	// timed out on every build.  So the toggle is driven, both ways, and both
	// ways are respawns: `spectator 0` is the one that lands with a menu open,
	// because PutClientInServer returns early for a spectator and never reaches
	// the move_to_arena that opens it.
	var last usage
	toggle := func(i int, value, want string) error {
		seen := len(subject.Prints())
		for try := 0; try < 5; try++ {
			subject.SetUserinfo("spectator", value)
			if err := waitForPrint(subject, seen, want, 12*time.Second); err == nil {
				return nil
			}
		}
		return fmt.Errorf("respawn %d: no %q after 5 attempts", i, want)
	}
	for i := 1; i <= respawns; i++ {
		if err := toggle(i, "0", `joined the game`); err != nil {
			return err
		}
		subject.WaitFrames(20, 10*time.Second)
		if err := toggle(i, "1", `moved to the sidelines`); err != nil {
			return err
		}
		subject.WaitFrames(20, 10*time.Second)
		u, err := gameUsage(srv)
		if err != nil {
			return err
		}
		last = u
		fmt.Printf("  ..    respawn pair %d: %+d blocks, %+d bytes against the first\n",
			i, u.blocks-base.blocks, u.bytes-base.bytes)
	}

	check("respawns do not grow the heap", last.blocks <= base.blocks,
		fmt.Sprintf("%d blocks after %d further respawns, %d after the first (%+d)",
			last.blocks, respawns, base.blocks, last.blocks-base.blocks))

	// Back into the arena for the menu checks, and then let it settle: the
	// driver stops on its own now (one userinfo change is one respawn), so what
	// this waits for is the last respawn's own statusbar repaint landing before
	// anything reads the bar.
	if err := toggle(0, "0", `joined the game`); err != nil {
		return err
	}
	subject.WaitFrames(80, 20*time.Second)

	// ...and the menu still has to WORK afterwards, in both directions: a fix
	// that freed a menu the client was still using would pass the count and
	// break the game.
	//
	// WHICH SIDE OF THE TOGGLE THE CLIENT IS ON HERE IS NOT KNOWN, and asserting
	// a fixed open/close/open from an assumed state is a claim about everything
	// that ran before it rather than about `inven`.  Whether a respawned arena
	// client is holding a menu depends on the path PutClientInServer took --
	// `init_player` builds one, `reinit_player` does not, and move_to_arena only
	// reopens the observer menu for a client coming from arena 0 -- and it moved
	// again the day a motd.txt put one more menu in the queue.  So the state is
	// established first and the TOGGLE is what is asserted: whatever is on
	// screen, `inven` must change it and change it back.
	if subject.MenuTitle() == "" {
		subject.Cmd("inven")
		subject.WaitFrames(10, 5*time.Second)
	}
	open, openTitle := subject.StatusBar(), subject.MenuTitle()
	check("a menu can be opened at all", openTitle != "",
		fmt.Sprintf("%d chars, title %q", len(open), openTitle))

	subject.Cmd("inven")
	subject.WaitFrames(10, 5*time.Second)
	closed := subject.StatusBar()

	subject.Cmd("inven")
	subject.WaitFrames(10, 5*time.Second)
	again, againTitle := subject.StatusBar(), subject.MenuTitle()

	check("menu still reopens to the same thing", open != "" && open == again,
		fmt.Sprintf("%d chars %q, %d chars closed, %d chars %q reopened",
			len(open), openTitle, len(closed), len(again), againTitle))
	check("menu still closes", closed != open, trunc(playtest.Decode(closed)))

	check("server survived", len(srv.Grep(`Segmentation|assertion|Z_Free|bad magic`)) == 0,
		"no crash or heap complaint in the console")
	return nil
}

// waitForPrint waits for a print that arrives AFTER the first `from` the bot
// had already collected, so a line matched a moment ago is not matched again.
func waitForPrint(b *playtest.Bot, from int, re string, timeout time.Duration) error {
	rx := regexp.MustCompile(re)
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		lines := b.Prints()
		for _, l := range lines[min(from, len(lines)):] {
			if rx.MatchString(l) {
				return nil
			}
		}
		time.Sleep(250 * time.Millisecond)
	}
	return fmt.Errorf("no %q in %s", re, timeout)
}

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}

// gameUsage reads the "game" row out of q2pro's z_stats.  Every allocation the
// game library makes through gi.TagMalloc is tagged above TAG_MAX and counted
// there, whatever tag the library itself passed.
func gameUsage(srv *playtest.Server) (usage, error) {
	n := srv.Len()
	if err := srv.Console("z_stats"); err != nil {
		return usage{}, err
	}
	if _, err := srv.WaitLog(`^\s*\d+\s+\d+ total\s*$`, 5*time.Second); err != nil {
		return usage{}, fmt.Errorf("z_stats did not answer: %w", err)
	}
	time.Sleep(300 * time.Millisecond)
	for _, line := range srv.Log()[n:] {
		if m := reGameRow.FindStringSubmatch(line); m != nil {
			b, _ := strconv.Atoi(m[1])
			c, _ := strconv.Atoi(m[2])
			return usage{bytes: b, blocks: c}, nil
		}
	}
	return usage{}, fmt.Errorf("no game row in z_stats")
}

func waitFor(timeout time.Duration, ok func() bool) error {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if ok() {
			return nil
		}
		time.Sleep(250 * time.Millisecond)
	}
	return fmt.Errorf("timed out after %s", timeout)
}

func trunc(s string) string {
	if len(s) > 70 {
		return s[:70] + "..."
	}
	return s
}
