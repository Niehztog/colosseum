// ospcamera -- tourney's chase camera has CONTROLS, and this is what they do.
//
// Tourney's observer is not baseq2's spectator.  baseq2 gives a spectator one
// key -- ATTACK toggles a fixed camera 30 units behind whoever it finds -- and
// the donor gives it a camera with a distance, a free-look, an in-eyes mode and
// a target cycle, all held on the observer's own edict: `speed` is the zoom
// (seeded from `camera_depth`), `osp_t018` the free-look yaw, `movedir` the pair
// UpdateChaseCam adds to the target's view angles.  The merge carried the
// entry command, the two cvars and the fields, and left out the input and the
// consumer, so every tourney chase camera sat at baseq2's fixed 30 units with
// no controls and no in-eyes mode.
//
// EVERY ONE OF THOSE IS VISIBLE FROM OUTSIDE, which is why this scenario can
// exist at all: the observer's own edict origin IS the camera, so the distance
// between the two clients' origins is the zoom; `ps.viewangles` is where the
// camera looks, so the free-look is a yaw that walks away from the target's own;
// and the mode changes announce themselves.
//
//	chasecam   camera_depth units behind the target, free-look and zoom live
//	in-eyes    twelve units IN FRONT of the eye, no free-look, no zoom
//	out        back to a free-flying observer
//
// Both signs throughout: a control client that stays a player, and a distance
// measured before and after every input.
package main

import (
	"flag"
	"fmt"
	"math"
	"os"
	"strings"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
)

var pass, fail int

func ok(what, detail string)  { pass++; fmt.Printf("  [ ok ] %-46s %s\n", what, detail) }
func bad(what, detail string) { fail++; fmt.Printf("  [FAIL] %-46s %s\n", what, detail) }

func dist(a, b [3]float64) float64 {
	dx, dy, dz := a[0]-b[0], a[1]-b[1], a[2]-b[2]
	return math.Sqrt(dx*dx + dy*dy + dz*dz)
}

// where prints the camera's offset from the target split into its horizontal and
// vertical parts, because the two are clamped by different things -- a wall
// shortens the horizontal reach and the floor lift raises the vertical one -- and
// a row that only prints the total cannot say which moved.
func where(a, b [3]float64) string {
	dx, dy := a[0]-b[0], a[1]-b[1]
	return fmt.Sprintf("xy %.0f, z %+.0f", math.Sqrt(dx*dx+dy*dy), a[2]-b[2])
}

func main() {
	q2 := flag.String("q2proded", "", "")
	lib := flag.String("lib", "", "")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "")
	dir := flag.String("dir", "/tmp/colosseum-ospcamera", "")
	port := flag.Int("port", 27999, "")
	rs := flag.String("ruleset", "dm", "an OSP ruleset")
	// 20 rather than the shipped 60, and the reason is a property of the camera
	// rather than of the harness: the placement is traced against the world, so
	// the distance is min(zoom, free space behind the target) and a zoom above
	// that space is invisible.  Measured on q2dm1 -- `camera_depth 100` puts the
	// camera at xy 38, z +32 against a wall and fifteen units of zoom change it
	// not at all, while at 20 the same input reads 34 -> 30 -> 34.  A row that
	// wanted the shipped default would be asserting the map.
	depth := flag.Float64("depth", 20, "camera_depth to set and expect")
	glad := flag.String("gladdir", "", "")
	flag.Parse()

	if !colosseum.IsOSP(*rs) {
		fmt.Printf("  [skip] %s has no tourney camera\n", *rs)
		return
	}
	if err := colosseum.Install(*dir, *ref, *ctf, *lib); err != nil {
		panic(err)
	}
	if *glad != "" {
		colosseum.InstallBrain(*dir, *glad)
	}

	srv := &playtest.Server{
		Binary: *q2, Dir: *dir, Game: "colosseum", Map: "q2dm1", Port: *port,
		MaxClients: 8,
		Cvars: map[string]string{"g_ruleset": *rs, "bots": "0",
			"bots_minplayers": "0", "deathmatch": "1", "coop": "0",
			"camera_depth": fmt.Sprintf("%g", *depth), "camera_pitch": "15",
			"team_a_name": "Hometeam", "team_b_name": "Visitors"},
		LogPath: *dir + "/server.log",
	}
	if err := srv.Start(); err != nil {
		panic(err)
	}
	defer srv.Stop()

	// The subject: a client that plays, so there is something to watch.
	player := playtest.NewBot("target", "127.0.0.1", *port)
	if err := player.Start(30 * time.Second); err != nil {
		panic(err)
	}
	defer player.Disconnect()
	player.WaitFrames(20, 15*time.Second)
	// A team ruleset joins BY NAME (ospenter's note): `join` alone is not a
	// team, and the target would stay an observer with nothing to watch.
	if colosseum.IsTeams(*rs) {
		player.Cmd("join Hometeam")
	} else {
		player.Cmd("join")
	}
	player.WaitFrames(20, 15*time.Second)
	if player.PMType() != playtest.PMNormal {
		bad("setup/the target is a player", "it never got a body")
		report()
		return
	}
	ok("setup/the target is a player", "PM_NORMAL, standing on a spawn point")

	// The observer.
	obs := playtest.NewBot("watcher", "127.0.0.1", *port)
	if err := obs.Start(30 * time.Second); err != nil {
		panic(err)
	}
	defer obs.Disconnect()
	obs.WaitFrames(20, 15*time.Second)

	obs.Cmd("chasecam")
	obs.WaitFrames(20, 15*time.Second)

	if obs.PMType() == playtest.PMFreeze {
		ok("chase/the camera is frozen", "PM_FREEZE, which is what a camera is")
	} else {
		bad("chase/the camera is frozen", fmt.Sprintf("pm_type %d", obs.PMType()))
	}

	// Who it is watching, read off the layout g_chase.c unicasts.
	if l, err := obs.WaitLayout("Chasing", 8*time.Second); err == nil &&
		strings.Contains(l, "target") {
		ok("chase/it says who it is watching", strings.TrimSpace(l))
	} else {
		bad("chase/it says who it is watching",
			fmt.Sprintf("layout=%.60q", obs.Layout()))
	}

	// ---- the zoom -------------------------------------------------------
	// The camera is placed camera_depth units back along the target's view and
	// then traced against the world, so a wall behind the target shortens it:
	// the row asserts the ORDER of the three distances rather than the number,
	// and prints all three.
	d0 := dist(obs.Origin(), player.Origin())
	if d0 > 8 {
		ok("chase/the camera stands off the target",
			fmt.Sprintf("%.0f units back (camera_depth %g, traced)", d0, *depth))
	} else {
		bad("chase/the camera stands off the target",
			fmt.Sprintf("%.0f units -- baseq2's fixed 30 would be 30, in-eyes 12", d0))
	}

	// ZOOM IN FIRST, and the order is the whole reason this row is reliable.
	// The camera's placement is `camera_depth` units back along the target's
	// view TRACED against the world, so the distance is min(zoom, whatever is
	// behind the target): zooming out gains nothing when the target spawned in
	// a corner, which is real behaviour and was measured as 55 -> 55 on one
	// spawn and 67 -> 74 on another.  Zooming IN is never blocked, so it is
	// asserted first and the zoom-out is asserted as a recovery from it.
	obs.Walk(400, 0, 2500*time.Millisecond) // forward = zoom in, to the eye
	obs.WaitFrames(10, 8*time.Second)
	d1 := dist(obs.Origin(), player.Origin())
	if d1 < d0-1 {
		ok("chase/forward zooms the camera in",
			fmt.Sprintf("%.0f -> %.0f units", d0, d1))
	} else {
		bad("chase/forward zooms the camera in",
			fmt.Sprintf("%.0f -> %.0f units (%s -> %s) -- forwardmove reached nothing",
				d0, d1, where(obs.Origin(), player.Origin()), where(obs.Origin(), player.Origin())))
	}

	obs.Walk(-400, 0, 2000*time.Millisecond) // back = zoom out again
	obs.WaitFrames(10, 8*time.Second)
	d2 := dist(obs.Origin(), player.Origin())
	if d2 > d1+1 {
		ok("chase/back zooms it out again", fmt.Sprintf("%.0f -> %.0f units", d1, d2))
	} else {
		bad("chase/back zooms it out again",
			fmt.Sprintf("%.0f -> %.0f units", d1, d2))
	}

	// ---- the free-look --------------------------------------------------
	// The camera's view angles are the target's plus an offset the observer
	// accumulates 4 degrees at a time, so the two yaws come apart.  Measured
	// against the TARGET's yaw rather than against a constant, because the
	// target is a real client whose own view is whatever it spawned facing.
	y0 := math.Abs(obs.ViewAngles()[1] - player.ViewAngles()[1])
	obs.Walk(0, 400, 1200*time.Millisecond)
	obs.WaitFrames(10, 8*time.Second)
	y1 := math.Abs(obs.ViewAngles()[1] - player.ViewAngles()[1])
	if y1 > y0+2 {
		ok("chase/strafe free-looks the camera",
			fmt.Sprintf("yaw offset %.0f -> %.0f degrees from the target's", y0, y1))
	} else {
		bad("chase/strafe free-looks the camera",
			fmt.Sprintf("yaw offset %.0f -> %.0f -- sidemove reached nothing", y0, y1))
	}

	// ---- in-eyes --------------------------------------------------------
	// A PRESS CAN MISS A FRAME, so the row presses until the mode changes and
	// reports how many it took.  The mode cycle needs the button's transition to
	// land inside a ClientThink that is not inside the donor's two-frame rate
	// limit (`osp_r010`), and a libq2 client holding a button for 700 ms does
	// not always put the transition where a real player's would be.  MEASURED
	// AND STABLE: the press that follows a movement input costs two, the one
	// after it costs one -- so the count is reported rather than hidden, and
	// three every time would be a defect this row is then reporting.
	presses, got := 0, false
	for presses < 3 && !got {
		mark := len(obs.Prints())
		obs.Press(playtest.ButtonAttack, 700*time.Millisecond)
		obs.WaitFrames(15, 10*time.Second)
		presses++
		for _, s := range obs.Prints()[min(mark, len(obs.Prints())):] {
			if strings.Contains(s, "IN-EYES") {
				got = true
			}
		}
	}
	if got {
		ok("in-eyes/attack changes mode",
			fmt.Sprintf("\"Changing to IN-EYES mode.\" after %d press(es)", presses))
	} else {
		bad("in-eyes/attack changes mode",
			"no IN-EYES print after 3 presses -- the cycle is not wired")
	}

	// Twelve units in FRONT of the eye, and the eye is 22 above the origin the
	// distance is measured from, so ~25 is the number to expect and anything
	// near the chase camera's stand-off is the failure.
	d3 := dist(obs.Origin(), player.Origin())
	if d3 < 40 {
		ok("in-eyes/the camera is at the target's eyes",
			fmt.Sprintf("%.0f units, against %.0f in chasecam", d3, d2))
	} else {
		bad("in-eyes/the camera is at the target's eyes",
			fmt.Sprintf("%.0f units -- still standing off like a chase camera", d3))
	}

	// ...and in-eyes has no free-look: the offset is dropped, so the two views
	// agree again.
	y2 := math.Abs(obs.ViewAngles()[1] - player.ViewAngles()[1])
	if y2 < 2 {
		ok("in-eyes/the free-look offset is dropped",
			fmt.Sprintf("yaw offset %.0f degrees, against %.0f in chasecam", y2, y1))
	} else {
		bad("in-eyes/the free-look offset is dropped",
			fmt.Sprintf("yaw offset %.0f degrees", y2))
	}

	// ---- and out --------------------------------------------------------
	presses, left := 0, false
	for presses < 3 && !left {
		mark := len(obs.Prints())
		obs.Press(playtest.ButtonAttack, 700*time.Millisecond)
		obs.WaitFrames(15, 10*time.Second)
		presses++
		for _, s := range obs.Prints()[min(mark, len(obs.Prints())):] {
			if strings.Contains(s, "OBSERVER") {
				left = true
			}
		}
	}
	if left && obs.PMType() == playtest.PMSpectator {
		ok("out/attack again leaves the camera",
			fmt.Sprintf("\"Changing to OBSERVER mode.\" and PM_SPECTATOR, after %d press(es)",
				presses))
	} else {
		bad("out/attack again leaves the camera",
			fmt.Sprintf("print=%v pm_type=%d after %d press(es)", left, obs.PMType(), presses))
	}

	report()
}

func report() {
	fmt.Printf("\n%d check(s), %d failed\n", pass+fail, fail)
	if fail > 0 {
		os.Exit(1)
	}
}

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}
