// ospff -- does `team_hurtteam` actually stop a shot, or only describe itself?
//
// OSP Tourney decides friendly fire PER TEAM rather than from dmflags, because
// a referee changes it mid-match: the cvar only SEEDS `osp_teams[n]` once, in
// OSP_gameInit, and g_combat.c asks OSP_teamFriendlyFire(team) on the T_Damage
// path.  A switch like that has two ways to be broken and only one of them is
// visible from the console -- the seed can fail to arrive, or it can arrive and
// nothing can read it.  Colosseum has shipped the second kind before (the
// `runes` modifier gated nothing while every diagnostic reported it), so this
// asks the WORLD: a live rocket, two teammates standing together, and the
// victim's STAT_HEALTH off the wire.
//
// Both signs, on two whole servers: `team_hurtteam 0` must leave the teammate
// untouched and `team_hurtteam 1` must hurt them.  Without the second server the
// first proves only that something -- a missed shot, a dead rocket, a client
// out of range -- happened to do no damage.
//
// Getting two clients into one place is the awkward part and `noclip` is the
// answer: a libq2 client cannot steer round map geometry, but with cheats on
// there is no geometry to steer round, and PM_FlyMove takes the whole view
// vector so one Look() plus one Walk() flies a straight line to the target.
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

const statHealth = 1

var pass, fail int

func ok(what, detail string)  { pass++; fmt.Printf("  [ ok ] %-52s %s\n", what, detail) }
func bad(what, detail string) { fail++; fmt.Printf("  [FAIL] %-52s %s\n", what, detail) }
func skip(what, why string)   { fmt.Printf("  [skip] %-52s %s\n", what, why) }

func dist(a, b [3]float64) float64 {
	return math.Sqrt((a[0]-b[0])*(a[0]-b[0]) + (a[1]-b[1])*(a[1]-b[1]) +
		(a[2]-b[2])*(a[2]-b[2]))
}

// aim points `from` at `to` the way vectoangles does.
func aim(from, to [3]float64) (pitch, yaw float64) {
	dx, dy, dz := to[0]-from[0], to[1]-from[1], to[2]-from[2]
	yaw = math.Atan2(dy, dx) * 180 / math.Pi
	pitch = -math.Atan2(dz, math.Hypot(dx, dy)) * 180 / math.Pi
	return
}

// lookAt points the client's VIEW at a spot, which is not the same as asking
// for the angle: pmove reads `cmd.angles + delta_angles`, and PutClientInServer
// seeds delta_angles with whatever the spawn point faced minus whatever the
// client last sent.  Asking for a bearing therefore applies that bearing plus
// an offset nobody outside the server knows -- which is the "yaw calibration
// differs per spawn point" trap in the skill notes.  So the request is a closed
// loop: ask, read `ps.viewangles` back, and correct by the error.
func lookAt(b *playtest.Bot, to [3]float64) {
	wp, wy := aim(b.Origin(), to)
	rp, ry := wp, wy
	for i := 0; i < 4; i++ {
		b.Look(rp, ry, 0)
		b.WaitFrames(3, 5*time.Second)
		got := b.ViewAngles()
		ep := wp - got[0]
		ey := math.Mod(wy-got[1]+540, 360) - 180
		if math.Abs(ep) < 2 && math.Abs(ey) < 2 {
			return
		}
		rp += ep
		ry += ey
	}
}

// closeIn flies `b` at `to` on noclip until it is within `want` units.  The
// burst is PROPORTIONAL to the distance left, because a fixed one overshoots:
// PM_FlyMove at forwardmove 400 covers roughly 800 units a second, so a
// half-second burst from 200 units away ends up 200 units past the target and
// the next burst flies back over it again.  Two thousand units of q2dm1 is four
// bursts down to arm's length this way.
func closeIn(b *playtest.Bot, to func() [3]float64, want float64) float64 {
	b.Cmd("noclip")
	defer b.Cmd("noclip")
	d := dist(b.Origin(), to())
	best := d
	for i := 0; i < 24 && d > want; i++ {
		lookAt(b, to())
		hold := time.Duration(d/800*1000) * time.Millisecond
		if hold > 700*time.Millisecond {
			hold = 700 * time.Millisecond
		} else if hold < 60*time.Millisecond {
			hold = 60 * time.Millisecond
		}
		b.Walk(400, 0, hold)
		d = dist(b.Origin(), to())
		if d < best {
			best = d
		}
	}
	return d
}

func main() {
	q2 := flag.String("q2proded", "", "")
	lib := flag.String("lib", "", "")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "")
	dir := flag.String("dir", "/tmp/colosseum-ospff", "")
	port := flag.Int("port", 27985, "")
	_ = flag.String("gladdir", "", "")
	flag.Parse()

	run(*q2, *lib, *ref, *ctf, *dir+"/off", *port, 0)
	run(*q2, *lib, *ref, *ctf, *dir+"/on", *port+1, 1)

	fmt.Printf("\n%d check(s), %d failed\n", pass+fail, fail)
	if fail > 0 {
		os.Exit(1)
	}
}

func run(q2, lib, ref, ctf, dir string, port, ff int) {
	fmt.Printf("\n##### tdm, team_hurtteam %d (port %d)\n", ff, port)

	if err := colosseum.Install(dir, ref, ctf, lib); err != nil {
		panic(err)
	}
	srv := &playtest.Server{
		Binary: q2, Dir: dir, Game: "colosseum", Map: "q2dm1", Port: port,
		MaxClients: 8,
		Cvars: map[string]string{
			"g_ruleset": "tdm", "bots": "0", "bots_minplayers": "0",
			"deathmatch": "1", "coop": "0", "flood_msgs": "0",
			"team_a_name": "Hometeam", "team_b_name": "Visitors",
			"team_hurtteam": fmt.Sprint(ff), "team_hurtself": "1",
			"match_countdown": "14", "cheats": "1",
			// Nothing after the countdown works if the match-start kill leaves
			// a client dead and waiting for a fire button it cannot press.
			"dmflags": "1024",
		},
		LogPath: dir + "/server.log",
	}
	if err := srv.Start(); err != nil {
		panic(err)
	}
	defer srv.Stop()

	tag := fmt.Sprintf("hurtteam=%d/", ff)

	shooter := playtest.NewBot("shooter", "127.0.0.1", port)
	mate := playtest.NewBot("mate", "127.0.0.1", port)
	foe := playtest.NewBot("foe", "127.0.0.1", port)
	for _, b := range []*playtest.Bot{shooter, mate, foe} {
		if err := b.Start(30 * time.Second); err != nil {
			panic(err)
		}
		defer b.Disconnect()
		b.WaitFrames(15, 20*time.Second)
	}
	shooter.Cmd("join Hometeam")
	mate.Cmd("join Hometeam")
	foe.Cmd("join Visitors")
	shooter.WaitFrames(30, 20*time.Second)

	// The match has to be RUNNING before anything else: Touch_Item returns
	// early while `sync_stat < 4`, so `give` is silently refused in warmup.
	for _, b := range []*playtest.Bot{shooter, mate, foe} {
		b.Cmd("ready")
	}
	if _, err := srv.WaitLog(`Match has started`, 45*time.Second); err != nil {
		skip(tag+"a live match", "the countdown never finished")
		return
	}
	shooter.WaitFrames(30, 20*time.Second)

	// The precondition, so that a refused grant and a broken grant are not the
	// same observation.
	shooter.Cmd("give rocket launcher")
	shooter.Cmd("give rockets")
	shooter.WaitFrames(10, 10*time.Second)
	shooter.Cmd("use rocket launcher")
	shooter.WaitFrames(10, 10*time.Second)
	if g := shooter.GunFrameHigh(); g == 0 {
		skip(tag+"the shooter has a live weapon", "gunframe never left zero")
		return
	}

	// The shot: fired STRAIGHT DOWN at the shooter's own feet rather than at
	// the teammate.  A rocket travels 24 units, hits the floor and splashes
	// everything inside 120 -- so the test needs the two clients to be NEAR each
	// other and needs no aim at all, and the shooter's own health becomes the
	// receipt that the rocket really went off (team_hurtself is 1 on both
	// servers).  Aiming at the other player instead makes a missed shot and a
	// gated shot the same observation.
	//
	// Up to three attempts, because the approach is the flaky part and not the
	// rule under test: a burst that overshoots is retried, a rocket that does no
	// self damage is retried, and an attempt that never gets close enough is
	// reported as a skip rather than counted as a pass.
	for attempt := 1; attempt <= 3; attempt++ {
		fmt.Printf("  ....  attempt %d: shooter %v mate %v\n",
			attempt, shooter.Origin(), mate.Origin())
		d := closeIn(shooter, mate.Origin, 70)
		if d > 110 {
			fmt.Printf("  ....  closed only to %.0f units\n", d)
			continue
		}

		feet := shooter.Origin()
		feet[2] -= 200
		lookAt(shooter, feet)
		shooter.WaitFrames(6, 10*time.Second)

		before, selfBefore := mate.Stat(statHealth), shooter.Stat(statHealth)
		shooter.Press(playtest.ButtonAttack, 400*time.Millisecond)
		shooter.WaitFrames(25, 15*time.Second)
		after, selfAfter := mate.Stat(statHealth), shooter.Stat(statHealth)

		if selfAfter >= selfBefore {
			fmt.Printf("  ....  no self splash (%d -> %d) at %.0f units; retrying\n",
				selfBefore, selfAfter, d)
			continue
		}
		ok(tag+"the rocket went off",
			fmt.Sprintf("the shooter took %d of its own splash at %.0f units",
				selfBefore-selfAfter, d))

		if ff == 0 {
			if after >= before {
				ok("hurtteam=0/a teammate in the splash is unhurt",
					fmt.Sprintf("STAT_HEALTH %d -> %d", before, after))
			} else {
				bad("hurtteam=0/a teammate in the splash is unhurt",
					fmt.Sprintf("STAT_HEALTH %d -> %d, %d damage -- the switch "+
						"gates nothing", before, after, before-after))
			}
		} else {
			if after < before {
				ok("hurtteam=1/control: the same shot DOES hurt them",
					fmt.Sprintf("STAT_HEALTH %d -> %d, %d damage",
						before, after, before-after))
			} else {
				bad("hurtteam=1/control: the same shot DOES hurt them",
					fmt.Sprintf("STAT_HEALTH %d -> %d -- the zero-damage row "+
						"above proves nothing", before, after))
			}
		}
		return
	}
	skip(tag+"a rocket landed between two teammates",
		"three attempts and the two clients never ended up in one place")
}
