// netlag -- what a client actually experiences on a RUNNING server, measured
// off the wire.
//
// Every other scenario here starts its own server.  This one points at an
// address, so it can be aimed at a live dedicated server and answer two
// questions that a subjective "it felt laggy" cannot settle on its own:
//
//  1. IS PREDICTION ON?  PMF_NO_PREDICTION in the playerstate is the only
//     witness.  A client carrying it renders the server's interpolated origin
//     instead of predicting its own, which is exactly one round trip of felt
//     delay on movement and jumping -- and nothing in the HUD says so.  The
//     watcher samples pm_type and pm_flags for the whole session and reports
//     every distinct combination it saw, with the share of samples that had
//     the bit set while the client was on PM_NORMAL.
//
//  2. DO SNAPSHOTS ARRIVE ON TIME?  A Quake II server frame is 100 ms.  What
//     a player feels as latency, when prediction IS on, is the spread of that
//     interval and the frames that never arrive -- not the mean.  So the gaps
//     between received frames are recorded individually and reported as a
//     distribution, together with how often the frame counter advanced by
//     more than one (a snapshot the client never got).
//
// Plus the end-to-end number the report is really about: how long from setting
// forwardmove to the origin moving, and from pressing ATTACK to the view
// weapon animating.  Both are quantised by the 10 Hz command rate and are only
// meaningful compared against another server measured the same way.
//
// Nothing here asserts.  It prints, because the question is "how do these two
// differ", not "is this one correct".
package main

import (
	"flag"
	"fmt"
	"math"
	"os"
	"sort"
	"strings"
	"sync"
	"time"

	"q2playtest/playtest"
	"q2playtest/ra2"
)

func pmName(t int) string {
	switch t {
	case playtest.PMNormal:
		return "PM_NORMAL"
	case playtest.PMSpectator:
		return "PM_SPECTATOR"
	case playtest.PMDead:
		return "PM_DEAD"
	case playtest.PMGib:
		return "PM_GIB"
	case playtest.PMFreeze:
		return "PM_FREEZE"
	}
	return fmt.Sprintf("pm_type(%d)", t)
}

// The seven pm_flags bits, named.  PMF_NO_PREDICTION is the one this scenario
// exists for; the rest are printed because a flag word is only readable whole.
func flagNames(f int) string {
	names := []struct {
		bit  int
		name string
	}{
		{1 << 0, "DUCKED"},
		{1 << 1, "JUMP_HELD"},
		{1 << 2, "ON_GROUND"},
		{1 << 3, "TIME_WATERJUMP"},
		{1 << 4, "TIME_LAND"},
		{1 << 5, "TIME_TELEPORT"},
		{1 << 6, "NO_PREDICTION"},
		{1 << 7, "TELEPORT_BIT"},
	}
	var on []string
	for _, n := range names {
		if f&n.bit != 0 {
			on = append(on, n.name)
		}
	}
	if len(on) == 0 {
		return "-"
	}
	return strings.Join(on, "|")
}

func pct(d []float64, p float64) float64 {
	if len(d) == 0 {
		return 0
	}
	s := append([]float64(nil), d...)
	sort.Float64s(s)
	i := int(p / 100 * float64(len(s)-1))
	return s[i]
}

func report(name string, d []float64, unit string) {
	if len(d) == 0 {
		fmt.Printf("    %-22s (no samples)\n", name)
		return
	}
	var sum float64
	max := d[0]
	for _, v := range d {
		sum += v
		if v > max {
			max = v
		}
	}
	mean := sum / float64(len(d))
	var va float64
	for _, v := range d {
		va += (v - mean) * (v - mean)
	}
	sd := math.Sqrt(va / float64(len(d)))
	fmt.Printf("    %-22s n=%-4d mean %6.1f %s  sd %5.1f  p50 %6.1f  p90 %6.1f  p99 %6.1f  max %7.1f\n",
		name, len(d), mean, unit, sd, pct(d, 50), pct(d, 90), pct(d, 99), max)
}

type watcher struct {
	mu      sync.Mutex
	seen    map[string]int
	normal  int
	nopred  int
	samples int
	// Every CHANGE of (pm_type, pm_flags), with the time it happened.  The
	// histogram says how much of the session was spent frozen; only the
	// transition log says whether that was one long stall or a stutter, and
	// what it lines up with.
	t0    time.Time
	trans []string
	last  string
}

func (w *watcher) run(b *playtest.Bot, stop <-chan struct{}) {
	w.seen = map[string]int{}
	w.t0 = time.Now()
	t := time.NewTicker(5 * time.Millisecond)
	defer t.Stop()
	for {
		select {
		case <-stop:
			return
		case <-t.C:
			ty, fl := b.PMType(), b.PMFlags()
			key := fmt.Sprintf("%-12s %s", pmName(ty), flagNames(fl))
			w.mu.Lock()
			w.samples++
			w.seen[key]++
			if key != w.last {
				w.trans = append(w.trans, fmt.Sprintf("%7.2fs  %s", time.Since(w.t0).Seconds(), key))
				w.last = key
			}
			if ty == playtest.PMNormal {
				w.normal++
				if fl&playtest.PMFNoPrediction != 0 {
					w.nopred++
				}
			}
			w.mu.Unlock()
		}
	}
}

func (w *watcher) print() {
	w.mu.Lock()
	defer w.mu.Unlock()
	fmt.Printf("  pmove states seen (%d samples at 5 ms):\n", w.samples)
	keys := make([]string, 0, len(w.seen))
	for k := range w.seen {
		keys = append(keys, k)
	}
	sort.Slice(keys, func(i, j int) bool { return w.seen[keys[i]] > w.seen[keys[j]] })
	for _, k := range keys {
		fmt.Printf("    %6.1f%%  %s\n", 100*float64(w.seen[k])/float64(w.samples), k)
	}
	if w.normal == 0 {
		fmt.Printf("  PMF_NO_PREDICTION while PM_NORMAL:  never on PM_NORMAL at all\n")
		return
	}
	fmt.Printf("  PMF_NO_PREDICTION while PM_NORMAL:  %d of %d samples (%.1f%%)\n",
		w.nopred, w.normal, 100*float64(w.nopred)/float64(w.normal))
	fmt.Printf("  pmove state changes (%d):\n", len(w.trans))
	for _, t := range w.trans {
		fmt.Printf("    %s\n", t)
	}
}

func moved(a, b [3]float64) bool {
	for i := 0; i < 3; i++ {
		if math.Abs(a[i]-b[i]) > 0.5 {
			return true
		}
	}
	return false
}

func main() {
	host := flag.String("host", "127.0.0.1", "server address")
	port := flag.Int("port", 27910, "server port")
	label := flag.String("label", "", "name for this run in the output")
	name := flag.String("name", "latprobe", "client name")
	// Under the OSP four -- which `dm` is -- connecting is not entering: the
	// client arrives as an observer and enters with `join`.  Under arena it is
	// the team menu instead.  One flag for the first, -arena for the second.
	join := flag.String("join", "", "console command(s) to enter the game, comma separated (e.g. 'join')")
	arena := flag.Bool("arena", false, "walk the RA2 menus into a team")
	team := flag.String("team", "", "existing team to join, e.g. '#1 Pickup Red'")
	newteam := flag.String("newteam", "", "arena to start a new team in")
	secs := flag.Int("secs", 30, "seconds of frame-arrival sampling")
	reps := flag.Int("reps", 8, "input-latency repetitions")
	wait := flag.Int("wait", 30, "seconds to wait for the server to put us on PM_NORMAL")
	// BUTTON_ANY is bit 7 and a REAL client sets it for any key pressed.  The
	// bot layer never does -- BotExecuteInput sets only ATTACK and USE -- and
	// baseq2's intermission ends on `ucmd->buttons & BUTTON_ANY` and nothing
	// else.  So this is the one input that tells a server stuck at an
	// intermission apart from one that is merely idle.
	anykey := flag.Int("anykey", 0, "hold BUTTON_ANY for this many seconds after connecting")
	flag.Parse()

	if *label == "" {
		*label = fmt.Sprintf("%s:%d", *host, *port)
	}

	b := playtest.NewBot(*name, *host, *port)
	if err := b.Start(40 * time.Second); err != nil {
		fmt.Printf("FATAL %s: %v\n", *label, err)
		os.Exit(2)
	}
	defer b.Disconnect()

	stop := make(chan struct{})
	w := &watcher{}
	go w.run(b, stop)
	defer close(stop)

	fmt.Printf("\n================ %s ================\n", *label)

	// Let the connect settle before anything is asked of the state.
	time.Sleep(6 * time.Second)

	for _, c := range strings.Split(*join, ",") {
		if c = strings.TrimSpace(c); c != "" {
			fmt.Printf("  sending %q\n", c)
			b.Cmd("%s", c)
			time.Sleep(1500 * time.Millisecond)
		}
	}

	if *anykey > 0 {
		fmt.Printf("  pm_type before BUTTON_ANY: %s (flags %s)\n",
			pmName(b.PMType()), flagNames(b.PMFlags()))
		b.Press(1<<7, time.Duration(*anykey)*time.Second)
		fmt.Printf("  pm_type after  BUTTON_ANY: %s (flags %s)\n",
			pmName(b.PMType()), flagNames(b.PMFlags()))
	}

	if *arena {
		// The MOTD stands in front of the team list, so it has to go before the
		// list can be read -- and the list is what names the arenas that
		// actually have somebody in them.
		if err := ra2.DismissMOTD(b); err != nil {
			fmt.Printf("  motd: %v\n", err)
		}
		if title, items := b.Menu(); title != "" {
			fmt.Printf("  menu: %q\n", title)
			for _, it := range items {
				fmt.Printf("      - %s\n", it.Text)
			}
		}
		var err error
		switch {
		case *team != "":
			err = ra2.JoinTeam(b, *team)
		case *newteam != "":
			err = ra2.NewTeamInArena(b, *newteam)
		default:
			err = ra2.DismissMOTD(b)
		}
		if err != nil {
			fmt.Printf("  join: %v (continuing as whatever the server left us)\n", err)
		}
	}

	// Wait for the server to put us on a walking pmove, but do not insist:
	// under arena a client between rounds is legitimately an observer, and
	// that is itself an answer.
	deadline := time.Now().Add(time.Duration(*wait) * time.Second)
	for time.Now().Before(deadline) && b.PMType() != playtest.PMNormal {
		time.Sleep(250 * time.Millisecond)
	}

	fmt.Printf("  pm_type=%s pm_flags=0x%02x (%s) pm_time=%d origin=%.0f\n",
		pmName(b.PMType()), b.PMFlags(), flagNames(b.PMFlags()), b.PMTime(), b.Origin())
	fmt.Printf("  health=%d frame=%d\n", b.Stat(1), b.Frame())
	// What the server SAID, which is the only way to tell a join that was
	// refused from one that was never attempted.
	if pr := b.Prints(); len(pr) > 0 {
		fmt.Printf("  server prints (last %d):\n", min(12, len(pr)))
		for _, l := range pr[max(0, len(pr)-12):] {
			fmt.Printf("      | %s\n", strings.TrimRight(l, "\n"))
		}
	}
	if t := b.MenuTitle(); t != "" {
		fmt.Printf("  menu on screen: %q\n", t)
	}

	// ---------------------------------------------------------------- frames
	// Poll the frame counter far faster than it can move, so the recorded
	// arrival time is the packet's and not the poll's.
	fmt.Printf("  sampling frame arrivals for %d s...\n", *secs)
	var gaps []float64
	var atFrame []int
	var skips int
	lastF := b.Frame()
	lastT := time.Now()
	end := time.Now().Add(time.Duration(*secs) * time.Second)
	for time.Now().Before(end) {
		f := b.Frame()
		if f > lastF {
			now := time.Now()
			adv := f - lastF
			gaps = append(gaps, float64(now.Sub(lastT).Microseconds())/1000.0/float64(adv))
			atFrame = append(atFrame, f)
			if adv > 1 {
				skips += adv - 1
			}
			lastF, lastT = f, now
		}
		time.Sleep(time.Millisecond)
	}

	// ---------------------------------------------------- input -> movement
	var mv []float64
	var mvSkipped int
	for i := 0; i < *reps*12 && len(mv) < *reps; i++ {
		if b.PMType() != playtest.PMNormal {
			mvSkipped++
			time.Sleep(500 * time.Millisecond)
			continue
		}
		{
			start := b.Origin()
			// Begin on a fresh frame so every repetition starts at the same
			// phase of the server's 100 ms tick.
			f := b.Frame()
			for b.Frame() == f {
				time.Sleep(time.Millisecond)
			}
			// Walk() holds the key down and then releases it, so it has to run
			// beside the poll rather than before it.  The clock starts first:
			// the goroutine's own scheduling delay is then counted against the
			// measurement instead of hidden inside it.
			t0 := time.Now()
			go b.Walk(400, 0, 500*time.Millisecond)
			d := time.Duration(0)
			for time.Since(t0) < 2*time.Second {
				if moved(start, b.Origin()) {
					d = time.Since(t0)
					break
				}
				time.Sleep(time.Millisecond)
			}
			if d > 0 {
				mv = append(mv, float64(d.Microseconds())/1000.0)
			}
			time.Sleep(1200 * time.Millisecond) // outlast the Walk and settle
		}
	}

	// ------------------------------------------------------ attack -> weapon
	var fire []float64
	var fireSkipped int
	for i := 0; i < *reps*12 && len(fire) < *reps; i++ {
		if b.PMType() != playtest.PMNormal {
			fireSkipped++
			time.Sleep(500 * time.Millisecond)
			continue
		}
		g := b.GunFrame()
		f := b.Frame()
		for b.Frame() == f {
			time.Sleep(time.Millisecond)
		}
		t0 := time.Now()
		go b.Press(playtest.ButtonAttack, 300*time.Millisecond)
		d := time.Duration(0)
		for time.Since(t0) < 2*time.Second {
			if b.GunFrame() != g {
				d = time.Since(t0)
				break
			}
			time.Sleep(time.Millisecond)
		}
		if d > 0 {
			fire = append(fire, float64(d.Microseconds())/1000.0)
		}
		time.Sleep(900 * time.Millisecond) // outlast the Press
	}

	fmt.Printf("\n  --- %s ---\n", *label)
	if pr := b.Prints(); len(pr) > 0 {
		fmt.Printf("  server prints (%d):\n", len(pr))
		for _, l := range pr {
			fmt.Printf("      | %s\n", strings.TrimRight(l, "\n"))
		}
	}
	w.print()
	fmt.Printf("  frame arrivals (a server frame is 100.0 ms):\n")
	report("inter-arrival", gaps, "ms")
	fmt.Printf("    %-22s %d snapshot(s) the client never received\n", "skipped frames", skips)
	over := func(ms float64) int {
		n := 0
		for _, g := range gaps {
			if g > ms {
				n++
			}
		}
		return n
	}
	fmt.Printf("    %-22s >120ms: %d   >150ms: %d   >200ms: %d   (of %d)\n",
		"late snapshots", over(120), over(150), over(200), len(gaps))
	// Worst ten, with the frame each landed on.  A hitch driven by work the
	// game gates on `level.framenum & 31` repeats on one residue; one driven by
	// a round restart does not repeat at all.
	type ga struct {
		g float64
		f int
	}
	all := make([]ga, len(gaps))
	for i := range gaps {
		all[i] = ga{gaps[i], atFrame[i]}
	}
	sort.Slice(all, func(i, j int) bool { return all[i].g > all[j].g })
	fmt.Printf("    worst 10 gaps (frame, frame%%32):\n      ")
	for i := 0; i < 10 && i < len(all); i++ {
		fmt.Printf("%.0fms@%d(%d)  ", all[i].g, all[i].f, all[i].f%32)
	}
	fmt.Println()
	var bmean [32]float64
	var bmax [32]float64
	var bn [32]int
	for i := range gaps {
		r := atFrame[i] % 32
		bmean[r] += gaps[i]
		bn[r]++
		if gaps[i] > bmax[r] {
			bmax[r] = gaps[i]
		}
	}
	worstR, worstV := 0, 0.0
	for r := 0; r < 32; r++ {
		if bn[r] > 0 && bmean[r]/float64(bn[r]) > worstV {
			worstV, worstR = bmean[r]/float64(bn[r]), r
		}
	}
	fmt.Printf("    %-22s residue %d is slowest: mean %.1f ms, max %.1f ms (n=%d)\n",
		"frame%32 phase", worstR, worstV, bmax[worstR], bn[worstR])
	fmt.Printf("  end to end:\n")
	report("forwardmove->moved", mv, "ms")
	report("attack->gunframe", fire, "ms")
	fmt.Printf("    %-22s %d movement / %d attack attempt(s) skipped -- not on PM_NORMAL\n",
		"alive-only gating", mvSkipped, fireSkipped)
	fmt.Println()
}
