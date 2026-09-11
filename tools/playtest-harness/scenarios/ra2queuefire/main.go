// ra2queuefire -- can a Rocket Arena client waiting in the queue shoot the
// fighters it is standing among?
//
// ClientThink ends with an unguarded
//
//	if (client->latched_buttons & BUTTON_ATTACK)
//	    if (!client->weapon_thunk) { weapon_thunk = true; Think_Weapon(ent); }
//
// and ClientBeginServerFrame's companion guards only on `resp.spectator`, which
// is baseq2's spectator and not RA2's.  A queued client is an observer by
// `fightstate`, keeps the Blaster InitClientPersistant handed it, and the
// Blaster needs no ammo -- so on the source alone this reads as reachable, and
// the Gladiator tree guards both call sites for what looks like this reason.
//
// It is not reachable, and the reason is not at the call site.  RA2 v2.220 put
// its guard at the top of Weapon_Generic instead:
//
//	if (ent->client && ent->client->fightstate != FIGHT_ALIVE)
//	    return;
//
// with a second one on the fire condition itself (`ent->takedamage` and the
// arena in ASTATE_FIGHTING).  Every weapon a queued client can hold reaches its
// weaponthink through that function.
//
// Which of those two readings is true is a question about the running game, so
// this asks it there, in both signs:
//
//  1. a client that joins DURING a round waits in the arena as an observer, and
//     ATTACK reaches the game -- ChangeOMode says so out loud.
//  2. its ps.gunframe never leaves zero.  Only the weaponthink moves that: to
//     FRAME_FIRE_FIRST on a shot, round the idle loop otherwise.
//  3. a fighter's DOES move, on the same server in the same round.  Without
//     this the second check passes just as well on a server that never sends
//     gunframe to anyone.
//
// Exit 0 all checks passed, 1 a check failed, 2 the scenario could not run.
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"time"

	"q2playtest/playtest"
	"q2playtest/ra2"
)

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "", "read-only reference install (RA2 paks + arena.cfg)")
	lib := flag.String("lib", "", "game library under test")
	dir := flag.String("dir", "/tmp/q2playtest/ra2queuefire", "scratch install dir")
	mapname := flag.String("map", "ra2map7", "map to test on")
	arena := flag.Int("arena", 6, "the map's pickup arena")
	port := flag.Int("port", 27960, "server port")
	label := flag.String("label", "", "label for the report")
	flag.Parse()

	if *q2 == "" || *ref == "" || *lib == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded, -ref and -lib")
		os.Exit(2)
	}
	if err := run(*q2, *ref, *lib, *dir, *mapname, *arena, *port, *label); err != nil {
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
		fmt.Printf("  PASS  %-32s %s\n", name, detail)
		return
	}
	failed++
	fmt.Printf("  FAIL  %-32s %s\n", name, detail)
}

func run(q2, ref, lib, dir, mapname string, arena, port int, label string) error {
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

	fighter, err := seat("red1", "Red")
	if err != nil {
		return err
	}
	defer fighter.Disconnect()
	foe, err := seat("blue1", "Blue")
	if err != nil {
		return err
	}
	defer foe.Disconnect()

	if err := waitFor(60*time.Second, func() bool {
		return !fighter.Spectating() && !foe.Spectating()
	}); err != nil {
		return fmt.Errorf("round never started: %w", err)
	}
	fmt.Println("  ..    round is live")

	// The subject of the report: someone who arrives while the round is running
	// and has to wait it out standing in the arena with the fighters.
	queued, err := seat("queued", "Red")
	if err != nil {
		return err
	}
	defer queued.Disconnect()
	queued.WaitFrames(20, 10*time.Second)

	check("the queued client is observing", queued.Spectating(),
		fmt.Sprintf("pm_type=%d (%d=spectator)", queued.PMType(), playtest.PMSpectator))
	check("...and has not fired yet", queued.GunFrameHigh() == 0,
		fmt.Sprintf("gunframe high water %d", queued.GunFrameHigh()))

	// Hold ATTACK long enough to cover many server frames, several times over.
	// ChangeOMode is what RA2 spends an observer's ATTACK on, and it announces
	// itself -- which is how this tells "the weapon did not fire" apart from
	// "the button never arrived", the two readings a silent result has.
	for i := 0; i < 4; i++ {
		queued.Press(playtest.ButtonAttack, 500*time.Millisecond)
	}
	sw, swErr := queued.WaitPrint(`Switched Observer Mode to`, 5*time.Second)
	check("ATTACK reaches the game", swErr == nil, sw)

	queued.WaitFrames(20, 10*time.Second)
	check("queued client's weapon never thinks", queued.GunFrameHigh() == 0,
		fmt.Sprintf("gunframe high water %d after %d presses", queued.GunFrameHigh(), 4))

	// The other sign.  A fighter on the same server, in the same round, pressing
	// the same button: its weapon has to think, or check 2 is measuring a wire
	// that carries nothing rather than a gate that holds.
	for i := 0; i < 4 && fighter.GunFrameHigh() == 0; i++ {
		fighter.Press(playtest.ButtonAttack, 500*time.Millisecond)
	}
	fighter.WaitFrames(20, 10*time.Second)
	check("a fighter's weapon does think", fighter.GunFrameHigh() > 0,
		fmt.Sprintf("gunframe high water %d", fighter.GunFrameHigh()))

	check("server survived", len(srv.Grep(`Segmentation|assertion`)) == 0,
		"no crash in the console")
	return nil
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
