// ra2prefire -- can a PERSON shoot during a Rocket Arena round countdown?
//
// RA2 grants damage exactly once per round: arena_think() calls
// set_damage(arena, DAMAGE_AIM) on the frame the countdown reaches zero, so a
// fighter standing in the countdown is `takedamage DAMAGE_NO` for the whole of
// it.  A shot fired before the bell cannot land -- and the ammo it spends is the
// round's, handed out once by give_ammo() and not replaced until the next round.
//
// The bot side of this is gated in BotExecuteInput and measured by
// `scenarios/ra2holdfire`.  The PERSON side stayed open for two
// increments because the Gladiator donor's own answer cannot be taken verbatim:
// it guards `Think_Weapon` wholesale, and one of its two call sites runs every
// frame and IS the weapon state machine, so the guard would freeze weapon
// SELECTION for the length of the countdown -- and choosing a weapon during the
// countdown is how a Rocket Arena round is prepared.
//
// So the gate under test is at the BUTTON, and that makes the measurement a
// question about ammo rather than about ps.gunframe: the think still runs, so
// the weapon still animates and the gunframe still moves.  Only firing takes
// ammo away.
//
// Asserted in BOTH signs, because "ammo did not move" is also what a server that
// never gave any ammo looks like:
//
//  1. a fighter placed for the countdown holds ATTACK and its ammo does not move
//  2. the same fighter, same weapon, same round, after the bell: it does
//  3. and weapon SELECTION works during the countdown -- `use` reaches
//     ChangeWeapon, which is the half the donor's fix would have broken
//
// Exit 0 all checks passed, 1 a check failed, 2 the scenario could not run.
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"time"

	"q2playtest/playtest"
	"q2playtest/ra2"
)

// Quake II's own stat slots.  STAT_AMMO is 3; it is id's, not a mod's, so it is
// safe to name here (a mod's own slots never are -- read those from its
// diagnostic).
const statAmmo = 3

var failed int

func check(name string, ok bool, format string, a ...any) {
	detail := fmt.Sprintf(format, a...)
	if ok {
		fmt.Printf("  PASS  %-38s %s\n", name, detail)
		return
	}
	failed++
	fmt.Printf("  FAIL  %-38s %s\n", name, detail)
}

func waitFor(d time.Duration, cond func() bool) error {
	deadline := time.Now().Add(d)
	for time.Now().Before(deadline) {
		if cond() {
			return nil
		}
		time.Sleep(100 * time.Millisecond)
	}
	return fmt.Errorf("timed out after %s", d)
}

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "", "read-only reference install (RA2 paks + arena.cfg)")
	lib := flag.String("lib", "", "game library under test")
	dir := flag.String("dir", "/tmp/q2playtest/ra2prefire", "scratch install dir")
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

var reState = regexp.MustCompile(`^arena (\d+)\s+(\w+)`)

// The arena's round-machine state, read out of `sv arenadump`.
func arenaState(srv *playtest.Server, arena int) string {
	srv.Console("sv arenadump")
	time.Sleep(400 * time.Millisecond)
	state := ""
	for _, line := range srv.Log() {
		m := reState.FindStringSubmatch(strings.TrimSpace(line))
		if m != nil && m[1] == fmt.Sprint(arena) {
			state = m[2] // the LAST one printed is the current one
		}
	}
	return state
}

// Hold ATTACK for a while, in short presses, and report how much ammo went.
func burn(b *playtest.Bot, presses int) (before, after int) {
	before = b.Stat(statAmmo)
	for i := 0; i < presses; i++ {
		b.Press(playtest.ButtonAttack, 400*time.Millisecond)
		time.Sleep(150 * time.Millisecond)
	}
	time.Sleep(400 * time.Millisecond)
	return before, b.Stat(statAmmo)
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
			"botfill":        "0",
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

	red, err := seat("red1", "Red")
	if err != nil {
		return err
	}
	defer red.Disconnect()
	blue, err := seat("blue1", "Blue")
	if err != nil {
		return err
	}
	defer blue.Disconnect()

	// Placement into the round is the start of the COUNTDOWN, not of the fight:
	// fill_arena() runs when the warmup reaches zero and the five-second round
	// countdown follows it.  pm_type is the honest signal that placement has
	// happened (the HUD is not).
	if err := waitFor(90*time.Second, func() bool {
		return !red.Spectating() && !blue.Spectating()
	}); err != nil {
		return fmt.Errorf("nobody was ever placed as a fighter: %w", err)
	}
	state := arenaState(srv, arena)
	fmt.Printf("  ..    placed as fighters, arena %d is %q\n", arena, state)
	check("placed during a countdown, not a fight", state == "countdown",
		"arena %d state %q", arena, state)

	// ---- 1. the countdown
	ammo0 := red.Stat(statAmmo)
	check("the round handed out ammo", ammo0 > 0,
		"STAT_AMMO %d -- without this the next row proves nothing", ammo0)

	before, after := burn(red, 4)
	stillCounting := arenaState(srv, arena) == "countdown"
	check("ATTACK in the countdown spends nothing", before == after,
		"STAT_AMMO %d -> %d (arena still counting: %v)", before, after, stillCounting)

	// ---- 3. ...and the weapon can still be CHANGED while it is held back.
	// This is the half the donor's Think_Weapon guard would have broken, so it
	// is checked here rather than left to be assumed.
	red.Cmd("use Shotgun")
	time.Sleep(1200 * time.Millisecond)
	shotgunAmmo := red.Stat(statAmmo)
	check("weapon selection still works in a countdown", shotgunAmmo != after,
		"STAT_AMMO %d -> %d across `use Shotgun` (a different ammo counter)",
		after, shotgunAmmo)

	// ---- 2. the fight
	if err := waitFor(60*time.Second, func() bool {
		return arenaState(srv, arena) == "fighting"
	}); err != nil {
		return fmt.Errorf("the round never started fighting: %w", err)
	}
	fmt.Println("  ..    the bell has rung")

	before, after = burn(red, 4)
	check("ATTACK after the bell does spend", after < before,
		"STAT_AMMO %d -> %d", before, after)

	return nil
}
