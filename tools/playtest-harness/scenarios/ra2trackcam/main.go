// ra2trackcam -- does Rocket Arena's follow camera actually follow?
//
// RA2's trackcam positions itself 150 units behind the tracked player, along the
// OBSERVER's own view angles, and moves by handing the difference to
// `ent->velocity`.  Nothing in the game integrates that: `SV_Physics_Noclip` is
// never reached for a client edict, because G_RunFrame runs clients through
// ClientBeginServerFrame and continues.  The integrator is `gi.Pmove`, called
// from ClientThink -- and pmove returns early, `// no movement at all`, for
// exactly one pm_type: PM_FREEZE.  So the pm_type this branch chooses decides
// whether the camera moves at all, and the two candidates differ by one:
//
//	PM_GIB    (3) narrows the bounding box and falls through friction, air
//	              move and step-slide, so the velocity is covered -- the donor's
//	              literal `3`
//	PM_FREEZE (4) returns before any of it, so the velocity is set every frame
//	              and consumed by nobody
//
// Under PM_FREEZE track_think's clear-path branch cannot move the camera, and
// its blocked branch -- which teleports and zeroes the velocity -- becomes the
// only thing that ever does: the camera does not follow, it snaps when something
// gets in the way.
//
// Two readings, both off the wire and neither needing a source read:
//
//	pm_type    the playerstate's own, which says which branch is in force
//	motion     the observer's origin, sampled while the tracked player moves
//
// Exit 0 both pass, 1 one failed, 2 the scenario could not run.
package main

import (
	"flag"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
	"q2playtest/ra2"
)

var reMode = regexp.MustCompile(`Switched Observer Mode to: (.+)`)
var reTrack = regexp.MustCompile(`Tracking (.+)`)

const trackcam = "Trackcam"

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "retail baseq2 paks")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "Threewave paks")
	lib := flag.String("lib", "", "game library under test")
	glad := flag.String("gladdir", "", "gladiator-bot-restored checkout (the brain)")
	dir := flag.String("dir", "/tmp/q2playtest/ra2trackcam", "scratch install dir")
	mapname := flag.String("map", "q2dm1", "map to test on")
	arena := flag.Int("arena", 1, "pickup arena to join")
	nbots := flag.Int("bots", 3, "how many bots (the camera needs somebody to follow)")
	port := flag.Int("port", 28040, "server port -- clear of the colosseum battery, which uses 27981..28010")
	label := flag.String("label", "", "label for the report")
	flag.Parse()

	if *q2 == "" || *lib == "" || *glad == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded, -lib and -gladdir")
		os.Exit(2)
	}
	bad, err := run(*q2, *ref, *ctf, *lib, *glad, *dir, *mapname, *arena, *nbots, *port, *label)
	if err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	if bad > 0 {
		os.Exit(1)
	}
}

func dist(a, b [3]float64) float64 {
	return math.Sqrt((a[0]-b[0])*(a[0]-b[0]) + (a[1]-b[1])*(a[1]-b[1]) + (a[2]-b[2])*(a[2]-b[2]))
}

func run(q2, ref, ctf, lib, glad, dir, mapname string, arena, nbots, port int, label string) (int, error) {
	if label != "" {
		fmt.Printf("== %s ==\n", label)
	}
	os.RemoveAll(dir)
	if err := colosseum.Install(dir, ref, ctf, lib); err != nil {
		return 0, err
	}
	if err := colosseum.InstallBrain(dir, glad); err != nil {
		return 0, err
	}
	srv := &playtest.Server{
		Binary: q2, Dir: dir, Game: "colosseum", Map: mapname, Port: port,
		MaxClients: 12, LogPath: filepath.Join(dir, "server.log"),
		Cvars: map[string]string{
			"g_ruleset": "arena", "skill": "1", "admincode": "0",
			"minimumplayers": "0", "bots_minplayers": "0", "arena": "0",
		},
	}
	if err := srv.Start(); err != nil {
		return 0, err
	}
	defer srv.Stop()

	// TWO clients on one pickup team, because ChangeOMode returns early for
	// anything but a spectator: with playersperteam 1 the first fights and the
	// second waits, and the one that waits is the one with a camera.  The
	// fighter is not decoration either -- the trackcam only holds a target that
	// is FIGHT_ALIVE.
	fighter := playtest.NewBot("fighter", "127.0.0.1", port)
	if err := fighter.Start(60 * time.Second); err != nil {
		return 0, err
	}
	defer fighter.Disconnect()
	if err := ra2.JoinTeam(fighter, ra2.PickupTeam(arena, "Red")); err != nil {
		return 0, fmt.Errorf("fighter could not join: %w", err)
	}
	b := playtest.NewBot("watcher", "127.0.0.1", port)
	if err := b.Start(60 * time.Second); err != nil {
		return 0, err
	}
	defer b.Disconnect()
	if err := ra2.JoinTeam(b, ra2.PickupTeam(arena, "Red")); err != nil {
		return 0, fmt.Errorf("watcher could not join: %w", err)
	}
	for i := 0; i < nbots; i++ {
		srv.Console("sv addrandom")
		time.Sleep(400 * time.Millisecond)
	}
	// A round has to be running: the trackcam only holds a target that is
	// FIGHT_ALIVE, and track_next drops it the moment nobody is.
	fmt.Printf("  ..    two clients on %s, %d bot(s); waiting for a round\n",
		ra2.PickupTeam(arena, "Red"), nbots)

	// ChangeOMode returns early for anything but a spectator, and `playersperteam`
	// on this arena seats both clients as fighters -- so the watcher becomes an
	// observer the way RA2 makes one: it gets killed.  A client that cannot move
	// or shoot does not wait long with bots in the arena, and PutClientInServer's
	// arena tail turns the corpse into a spectator (reinit_player), which is what
	// pm_type reports here.
	deadline := time.Now().Add(120 * time.Second)
	for time.Now().Before(deadline) {
		if b.PMType() != playtest.PMNormal {
			break
		}
		time.Sleep(2 * time.Second)
	}
	if b.PMType() == playtest.PMNormal {
		return 0, fmt.Errorf("the watcher was never taken out of the fight; "+
			"pm_type is still %d", b.PMType())
	}
	fmt.Printf("  ..    watcher is out of the fight, pm_type %d\n", b.PMType())

	// ---- reach the trackcam ------------------------------------------------
	//
	// ATTACK is RA2's observer-mode key and the modes cycle Normal -> Free
	// Flying -> Trackcam -> In Eyes; the server names each one as it switches,
	// which is what this reads rather than guessing at a count.
	// Two conditions, not one.  Being IN the trackcam is not enough: the branch
	// under test needs `resp.track_target`, and track_change only sets one when
	// somebody in this arena is FIGHT_ALIVE -- so between rounds the mode is
	// Trackcam with no target, and ClientThink leaves pm_type at PM_SPECTATOR
	// from MOVETYPE_NOCLIP.  The server announces the target ("Tracking <name>"),
	// so both are read rather than assumed, and the cycle is retried across
	// rounds until they hold together.
	// THREE conditions, and the third is why: the reading has to be taken while
	// the state under test is in force.  The watcher is put back in the fight
	// every round, and a live fighter reports pm_type PM_NORMAL and stands still
	// -- which reads exactly like a frozen camera and is nothing of the kind.
	// Two runs were thrown away learning that, so the camera pm_type is a
	// CONDITION of the acquisition, not something sampled after it.
	mode, target, pm := "", "", -1
	camera := func(t int) bool { return t == playtest.PMGib || t == playtest.PMFreeze }
	deadline = time.Now().Add(240 * time.Second)
	for time.Now().Before(deadline) && !(mode == trackcam && target != "" && camera(pm)) {
		// The watcher is put back in the fight every round, and ChangeOMode is a
		// no-op for a fighter -- so wait each time rather than pressing into a
		// body that cannot switch.
		if b.PMType() == playtest.PMNormal {
			mode, target = "", ""
			time.Sleep(2 * time.Second)
			continue
		}
		b.Press(playtest.ButtonAttack, 250*time.Millisecond)
		time.Sleep(600 * time.Millisecond)
		for _, l := range b.Prints() {
			d := playtest.Decode(l)
			if m := reMode.FindStringSubmatch(d); m != nil {
				mode = strings.TrimSpace(m[1])
				if mode != trackcam {
					target = "" // a target only counts while the camera is on
				}
			}
			if m := reTrack.FindStringSubmatch(d); m != nil {
				target = strings.TrimSpace(m[1])
			}
		}
		pm = b.PMType()
	}
	if mode != trackcam || target == "" || !camera(pm) {
		return 0, fmt.Errorf("could not get a tracking trackcam in a camera pm_type "+
			"(mode %q, target %q, pm_type %d)", mode, target, pm)
	}
	fmt.Printf("  ..    observer mode is %q, tracking %q\n", mode, target)

	bad := 0

	// ---- 1. which branch of pmove is in force ------------------------------
	if pm == playtest.PMGib {
		fmt.Printf("  PASS  %-28s pm_type %d (PM_GIB) -- pmove integrates\n", "camera has an integrator", pm)
	} else {
		fmt.Printf("  FAIL  %-28s pm_type %d", "camera has an integrator", pm)
		if pm == playtest.PMFreeze {
			fmt.Printf(" (PM_FREEZE) -- pmove returns before any movement\n")
		} else {
			fmt.Printf(" -- expected %d (PM_GIB)\n", playtest.PMGib)
		}
		bad++
	}

	// ---- 2. does it move ---------------------------------------------------
	var samples [][3]float64
	lapsed := false
	for i := 0; i < 16; i++ {
		if !camera(b.PMType()) {
			lapsed = true
			break
		}
		samples = append(samples, b.Origin())
		time.Sleep(500 * time.Millisecond)
	}
	if lapsed {
		fmt.Printf("  ..    %-28s the round ended mid-sample after %d of 16 -- "+
			"travel below is short\n", "camera travel", len(samples))
	}
	if len(samples) < 2 {
		fmt.Printf("  ..    %-28s not sampled\n", "camera travel")
		return bad, nil
	}
	total, moved, worst := 0.0, 0, 0.0
	for i := 1; i < len(samples); i++ {
		d := dist(samples[i-1], samples[i])
		total += d
		if d > 0.5 {
			moved++
		}
		if d > worst {
			worst = d
		}
	}
	// REPORTED, NOT ASSERTED, and the first version of this got it wrong.
	// "Does the camera move" does not separate the two branches, because
	// track_think's blocked branch TELEPORTS -- and following a bot around
	// q2dm1 the line to the goal is blocked often, so a camera with no
	// integrator still covers ground, in jumps.  Measured on the pre-fix
	// library: 1837 units with 9 of 15 intervals moving.  What separates them
	// is the SHAPE: an integrated camera is fed a velocity every think and so
	// moves on nearly every interval, where a teleporting one moves on some and
	// sits still on the rest.  Both numbers are printed so the two builds can be
	// read side by side; the assertion is the pm_type above, which is the fact
	// rather than its consequence.
	fmt.Printf("  ..    %-28s %.0f units, %d of %d intervals moved, worst single %.0f\n",
		"camera travel", total, moved, len(samples)-1, worst)
	return bad, nil
}
