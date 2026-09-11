// ospobsmodes -- sweep a tourney observer through every mode it has and ask,
// in each one, whether the VERTICAL axis still answers the mouse.
//
// "I can only look left and right" has exactly one mechanism behind it in this
// engine: PM_ClampAngles answers PMF_TIME_TELEPORT by writing
// viewangles[PITCH] = viewangles[ROLL] = 0 and letting YAW alone -- and the
// pm_time countdown that clears the flag sits BELOW Pmove()'s early return for
// PM_SPECTATOR, so an observer that is handed the flag keeps it for good.  Both
// halves of the engine agree: q2pro's client runs the same Pmove locally when
// prediction is on and copies pm.viewangles into cl.predicted_angles, so the
// pin is rendered as well as sent.
//
// So the sweep reports two things per mode: the flag word off the wire, and
// what the view actually does when the client is told to look down and then up.
// A mode where the mouse is not supposed to steer at all (chasecam, in-eyes,
// autocam -- the camera looks where the CAMERA looks) is reported rather than
// judged; the judgement is reserved for free-flight, which is the mode a
// player spends warmup in.
package main

import (
	"flag"
	"fmt"
	"math"
	"os"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
)

var pass, fail int

func ok(what, detail string)  { pass++; fmt.Printf("  [ ok ] %-46s %s\n", what, detail) }
func bad(what, detail string) { fail++; fmt.Printf("  [FAIL] %-46s %s\n", what, detail) }

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
	return fmt.Sprintf("pm_type %d", t)
}

// probe points the view down and then up and reports what came back, plus the
// pmove state that decides whether it could have.
func probe(b *playtest.Bot, label string, judge bool) {
	b.Look(-45, 70, 0)
	b.WaitFrames(15, 10*time.Second)
	d := b.ViewAngles()
	b.Look(35, 70, 0)
	b.WaitFrames(15, 10*time.Second)
	u := b.ViewAngles()
	b.Look(0, 0, 0)

	pm, fl, tm := b.PMType(), b.PMFlags(), b.PMTime()
	tele := fl&playtest.PMFTimeTeleport != 0
	moved := math.Abs(u[0]-d[0]) > 10

	detail := fmt.Sprintf("%s pm_flags 0x%02x pm_time %d | look -45 -> %.1f, "+
		"+35 -> %.1f (yaw %.1f -> %.1f)",
		pmName(pm), fl, tm, d[0], u[0], d[1], u[1])

	if !judge {
		fmt.Printf("  [ .. ] %-46s %s\n", label, detail)
		return
	}
	if moved && !tele {
		ok(label, detail)
	} else if tele {
		bad(label, detail+"  <-- PMF_TIME_TELEPORT pins PITCH and ROLL")
	} else {
		bad(label, detail+"  <-- the vertical axis did not move")
	}
}

func main() {
	q2 := flag.String("q2proded", "", "")
	lib := flag.String("lib", "", "")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "")
	dir := flag.String("dir", "/tmp/colosseum-ospobsmodes", "")
	port := flag.Int("port", 27983, "")
	rs := flag.String("ruleset", "tdm", "")
	mp := flag.String("map", "q2dm1", "")
	_ = flag.String("gladdir", "", "")
	flag.Parse()

	if err := colosseum.Install(*dir, *ref, *ctf, *lib); err != nil {
		panic(err)
	}
	srv := &playtest.Server{
		Binary: *q2, Dir: *dir, Game: "colosseum", Map: *mp, Port: *port,
		MaxClients: 8,
		Cvars: map[string]string{
			"g_ruleset": *rs, "bots": "0", "bots_minplayers": "0",
			"deathmatch": "1", "coop": "0", "flood_msgs": "0",
			"team_a_name": "Hometeam", "team_b_name": "Visitors",
		},
		LogPath: *dir + "/server.log",
	}
	if err := srv.Start(); err != nil {
		panic(err)
	}
	defer srv.Stop()

	// Somebody to watch: the cameras all refuse with "No clients to track"
	// when the game is empty.
	target := playtest.NewBot("target", "127.0.0.1", *port)
	if err := target.Start(30 * time.Second); err != nil {
		panic(err)
	}
	defer target.Disconnect()
	target.WaitFrames(20, 20*time.Second)
	target.Cmd("join Hometeam")
	target.WaitFrames(30, 20*time.Second)
	fmt.Printf("  ....  target is %s\n", pmName(target.PMType()))

	b := playtest.NewBot("watcher", "127.0.0.1", *port)
	if err := b.Start(30 * time.Second); err != nil {
		panic(err)
	}
	defer b.Disconnect()
	b.WaitFrames(30, 20*time.Second)

	// 1. free flight, straight off the connect.
	probe(b, "observer/free-fly, fresh connect", true)

	// 2. ...and after flying about for a while, which is the only way an
	//    observer can reach anything in the world that touches it.
	for i := 0; i < 6; i++ {
		b.Look(0, float64(i*60), 0)
		b.Walk(400, 0, 900*time.Millisecond)
	}
	b.Look(0, 0, 0)
	b.WaitFrames(10, 10*time.Second)
	probe(b, "observer/free-fly, after 6s of flight", true)

	// 3. the three cameras, each reached by its own command.
	for _, m := range []struct{ cmd, name string }{
		{"chasecam", "chasecam"},
		{"autocam", "autocam"},
		{"chasecam", "chasecam (again)"},
		{"observer", "free-fly (back out of the cameras)"},
	} {
		b.Cmd(m.cmd)
		b.WaitFrames(20, 15*time.Second)
		probe(b, "observer/"+m.name, m.cmd == "observer")
	}

	// 4. ...and the way a PLAYER gets back to watching: die, then `observer`.
	//    A client that leaves a body behind has been through respawn(), which is
	//    where the teleport hold is stamped.
	target.Cmd("kill")
	target.WaitFrames(20, 15*time.Second)
	fmt.Printf("  ....  target after `kill` is %s\n", pmName(target.PMType()))
	target.Cmd("observer")
	target.WaitFrames(30, 20*time.Second)
	probe(target, "player/after dying and going to observer", true)

	fmt.Printf("\n%d judged check(s), %d failed\n", pass+fail, fail)
	if fail > 0 {
		os.Exit(1)
	}
}
