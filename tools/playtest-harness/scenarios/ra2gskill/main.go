// ra2gskill -- does Rocket Arena's stdlog still record DEATHS?
//
// `scenarios/ra2gslog` covers the five lines a bot can produce by connecting
// and leaving: StdLog, PatchName, MAP, GameStart, PlayerConnect, PlayerLeft.
// It cannot reach the sixth, which is the one that matters most -- GSLogDeath
// and the GSdodeathlog it calls are the only part of gslog.c a round actually
// exercises, and the only part that ever had a second consumer behind it.
//
// A bot cannot walk and cannot aim by eye, but it does not have to: Look()
// points the usercmd angles wherever a scenario says, and Origin() reports
// where the server put every client.  A rocket at one's own feet is therefore a
// deterministic suicide, which reaches GSLogDeath's attacker == self arm and
// the GSdodeathlog behind it.
//
// The other arm -- attacker != self, logged as Kill -- cannot be driven this
// way, and the reason is worth writing down: RA2 places every arrival as far
// from every other player as the arena allows, teammates included.  Measured
// on ra2map7 arena 6, a pickup arena: 771 units between the two SIDES and 2315
// between two clients on the SAME side.  A rocket fired across either lands in
// the geometry between them -- at 767 units the shooter splashed itself and
// logged a second Suicide.  Seating them together is not a thing a scenario can
// ask for, so the aimed shot is attempted and REPORTED rather than asserted.
// Both arms call the same GSdodeathlog, so the suicide rows are what carry the
// verdict.
//
// Exit 0 a death was logged, 1 a death happened and was not logged, 2 the
// scenario could not run.
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

	"q2playtest/playtest"
	"q2playtest/ra2"
)

// Quake II's own slots, not a mod's.
const (
	statHealth = 1
	statAmmo   = 3
)

var failed int

func check(name string, ok bool, format string, a ...any) {
	detail := fmt.Sprintf(format, a...)
	if ok {
		fmt.Printf("  PASS  %-40s %s\n", name, detail)
		return
	}
	failed++
	fmt.Printf("  FAIL  %-40s %s\n", name, detail)
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

// aim points `from` at `at` the way a mouse would.  Quake II's pitch is
// POSITIVE downward, which is why the z term is negated.
func aim(b *playtest.Bot, from, at [3]float64) (pitch, yaw float64) {
	dx, dy, dz := at[0]-from[0], at[1]-from[1], at[2]-from[2]
	yaw = math.Atan2(dy, dx) * 180 / math.Pi
	pitch = -math.Atan2(dz, math.Hypot(dx, dy)) * 180 / math.Pi
	b.Look(pitch, yaw, 0)
	time.Sleep(300 * time.Millisecond)
	return pitch, yaw
}

// shoot holds ATTACK until `done` says stop or the presses run out, and
// reports how many it took and how much ammo went.
func shoot(b *playtest.Bot, presses int, done func() bool) (fired, ammo0, ammo1 int) {
	ammo0 = b.Stat(statAmmo)
	for i := 0; i < presses; i++ {
		if done() {
			break
		}
		b.Press(playtest.ButtonAttack, 250*time.Millisecond)
		fired++
		time.Sleep(250 * time.Millisecond)
	}
	time.Sleep(600 * time.Millisecond)
	return fired, ammo0, b.Stat(statAmmo)
}

// waitNewCenter waits for a centerprint matching re that arrived AFTER `seen`
// prints.  Centers() accumulates, so WaitCenter on a second round matches the
// first round's bell instantly and hands back a fight that is already over.
func waitNewCenter(b *playtest.Bot, re string, seen int, d time.Duration) (string, int, error) {
	rx := regexp.MustCompile(re)
	deadline := time.Now().Add(d)
	for time.Now().Before(deadline) {
		c := b.Centers()
		for i := seen; i < len(c); i++ {
			if rx.MatchString(c[i]) {
				return c[i], len(c), nil
			}
		}
		time.Sleep(100 * time.Millisecond)
	}
	return "", seen, fmt.Errorf("no new %q within %s", re, d)
}

func grep(path, needle string) bool {
	raw, err := os.ReadFile(path)
	return err == nil && strings.Contains(string(raw), needle)
}

func dist(a, b [3]float64) float64 {
	return math.Sqrt((a[0]-b[0])*(a[0]-b[0]) + (a[1]-b[1])*(a[1]-b[1]) + (a[2]-b[2])*(a[2]-b[2]))
}

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "", "read-only reference install")
	lib := flag.String("lib", "", "game library under test")
	dir := flag.String("dir", "/tmp/q2playtest/ra2gskill", "scratch install dir")
	mapname := flag.String("map", "ra2map7", "map to test on")
	arena := flag.Int("arena", 6, "the map's pickup arena")
	weapon := flag.String("weapon", "Rocket Launcher", "splash weapon to suicide with")
	port := flag.Int("port", 27980, "server port")
	label := flag.String("label", "", "label for the report")
	flag.Parse()

	if *q2 == "" || *ref == "" || *lib == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded, -ref and -lib")
		os.Exit(2)
	}
	if err := run(*q2, *ref, *lib, *dir, *mapname, *weapon, *arena, *port, *label); err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	if failed > 0 {
		os.Exit(1)
	}
}

func run(q2, ref, lib, dir, mapname, weapon string, arena, port int, label string) error {
	if label != "" {
		fmt.Printf("== %s ==\n", label)
	}
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return err
	}
	if err := playtest.Install(dir, "arena", ref, lib); err != nil {
		return err
	}
	logPath := filepath.Join(dir, "arena", "stdlog.log")
	os.Remove(logPath)

	// RA2 routes a self-inflicted rocket through the same OnSameTeam() arm as
	// friendly fire, so `healthprotect 1` -- what every arena in the shipped
	// arena.cfg inherits -- returns before the damage is taken and a rocket at
	// one's own feet costs nothing.  Ammo still goes, which is what makes the
	// stock config read as "the shot fired and did nothing".  This config turns
	// the protection off and strips the armour so a couple of rockets are
	// lethal; it is the arena settings under test, not the log.
	// The arena has to stay a PICKUP arena (`pickup: 1`) or there are no
	// `#6 Pickup Red`/`Blue` teams to join, so this derives its config from the
	// shipped one and rewrites a single arena block rather than inventing a
	// file.  Stripping the armour and dropping health makes two rockets lethal.
	cfgName := "gskill.cfg"
	refCfg, err := os.ReadFile(filepath.Join(ref, "arena.cfg"))
	if err != nil {
		return err
	}
	block := fmt.Sprintf("\t%d {", arena)
	i := strings.Index(string(refCfg), fmt.Sprintf("%s {", mapname))
	if i < 0 {
		return fmt.Errorf("%s has no %s block", "arena.cfg", mapname)
	}
	j := strings.Index(string(refCfg)[i:], block)
	if j < 0 {
		return fmt.Errorf("%s has no arena %d block", mapname, arena)
	}
	j += i
	end := strings.Index(string(refCfg)[j:], "\n\t}")
	if end < 0 {
		return fmt.Errorf("arena %d block is unterminated", arena)
	}
	end += j + len("\n\t}")
	patched := string(refCfg)[:j] + fmt.Sprintf(`	%d {
		weapons: 2 3 4 5 6 7 8 9 0;
		armor: 0;
		health: 100;
		healthprotect: 0;
		armorprotect: 0;
		fallingdamage: 1;
		pickup: 1;
		rounds: 9;
	}`, arena) + string(refCfg)[end:]
	if err := os.WriteFile(filepath.Join(dir, "arena", cfgName), []byte(patched), 0o644); err != nil {
		return err
	}
	fmt.Printf("  ..    %s: %s arena %d rewritten with healthprotect 0, armor 0, health 100\n",
		cfgName, mapname, arena)

	srv := &playtest.Server{
		Binary: q2, Dir: dir, Game: "arena", Map: mapname, Port: port,
		LogPath: filepath.Join(dir, "server.log"),
		Cvars: map[string]string{
			"g_ruleset": "arena", "arenacfg": cfgName,
			"bots": "0", "botfill": "0", "minimumplayers": "0", "admincode": "0",
			"logfile": "2", "logname": "stdlog.log",
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

	red, err := seat("redfrag", "Red")
	if err != nil {
		return err
	}
	defer red.Disconnect()
	blue, err := seat("bluefrag", "Blue")
	if err != nil {
		return err
	}
	defer blue.Disconnect()
	// A third client on RED's side.  An idarena alternates its spawn points
	// between the two sides, so teammates are placed near each other while the
	// two sides are put as far apart as the arena allows -- which is why an
	// aimed shot across the arena hits a wall and one at a teammate does not.
	// `healthprotect 0` in the config above is what lets it land.
	mate, err := seat("redmate", "Red")
	if err != nil {
		return err
	}
	defer mate.Disconnect()

	if err := waitFor(90*time.Second, func() bool {
		return !red.Spectating() && !blue.Spectating() && !mate.Spectating()
	}); err != nil {
		return fmt.Errorf("nobody was ever placed as a fighter: %w", err)
	}

	// Damage is only granted after the bell -- ra2prefire establishes that the
	// countdown eats ATTACK entirely, so a shot fired before it proves nothing.
	// The bell is a centerprint, which is the channel that says so from the
	// client's side without asking the mod what it believes.
	if _, _, err := waitNewCenter(red, `FIGHT`, 0, 90*time.Second); err != nil {
		return fmt.Errorf("the round never rang the bell: %w", err)
	}
	time.Sleep(500 * time.Millisecond)
	fmt.Printf("  ..    bell rang; redfrag health %d armor %d\n",
		red.Stat(statHealth), red.Stat(5))

	// ---- 1. an aimed rocket at a TEAMMATE.  GSLogDeath's attacker != self arm,
	// so the line reads Kill.  It goes first because it needs both Red
	// fighters alive: RA2 puts the two SIDES as far apart as the arena allows,
	// and a rocket fired across that distance hits the geometry in between
	// (measured: 767 units on this arena, and the shooter splashed itself).
	// Same-side teammates are placed together, which is a shot that connects.
	mate.Cmd("use " + weapon)
	time.Sleep(500 * time.Millisecond)
	check("the round handed out ammo for "+weapon, mate.Stat(statAmmo) > 0,
		"STAT_AMMO %d -- without this the rows below prove nothing", mate.Stat(statAmmo))

	fmt.Printf("  ..    redmate -> redfrag is %.0f units, redmate -> bluefrag %.0f\n",
		dist(mate.Origin(), red.Origin()), dist(mate.Origin(), blue.Origin()))
	for i := 0; i < 14 && !mate.Spectating() && !red.Spectating(); i++ {
		pi, ya := aim(mate, mate.Origin(), red.Origin())
		mate.Press(playtest.ButtonAttack, 250*time.Millisecond)
		time.Sleep(250 * time.Millisecond)
		if i == 0 {
			fmt.Printf("  ..    redmate aiming pitch %.1f yaw %.1f\n", pi, ya)
		}
	}
	time.Sleep(1500 * time.Millisecond)
	if red.Spectating() && grep(logPath, "\tKill\t") {
		fmt.Printf("  PASS  %-40s the attacker != self arm logged a Kill\n",
			"an aimed rocket connected")
	} else {
		fmt.Printf("  ..    the aimed rocket did not connect (redfrag pm_type %d) -- see the\n", red.PMType())
		fmt.Printf("  ..    header: RA2 spreads every arrival apart, so no pairing a scenario\n")
		fmt.Printf("  ..    can arrange has a clear line of fire.  Reported, not asserted.\n")
	}

	// ---- 2. a rocket at one's own feet.  The other arm: attacker == self and
	// the weapon is one of the three GSLogDeath names, so the line carries the
	// weapon.  A dead fighter is off PM_NORMAL within a frame or two and stats
	// are delta-compressed, so STAT_HEALTH is back at full by the time it can
	// be sampled -- pm_type is the signal, not health.
	survivor, who := mate, "redmate"
	if survivor.Spectating() {
		survivor, who = blue, "bluefrag"
	}
	survivor.Cmd("use " + weapon)
	time.Sleep(400 * time.Millisecond)
	h0 := survivor.Stat(statHealth)
	survivor.Look(85, 0, 0) // straight down
	time.Sleep(300 * time.Millisecond)
	fired, a0, a1 := shoot(survivor, 12, survivor.Spectating)
	fmt.Printf("  ..    %s fired %d down: health %d -> %d, ammo %d -> %d\n",
		who, fired, h0, survivor.Stat(statHealth), a0, a1)
	check("the self-rocket spent ammo", a1 < a0, "STAT_AMMO %d -> %d", a0, a1)
	check("a self-rocket took its owner off the fight", survivor.Spectating(),
		"%s pm_type %d (PMSpectator is %d)", who, survivor.PMType(), playtest.PMSpectator)
	check("...and the log wrote a Suicide for it", grep(logPath, "\tSuicide\t"),
		"looking for a Suicide line -- GSLogDeath's attacker == self arm")

	// ---- 3. what the log says.
	time.Sleep(2 * time.Second)
	raw, err := os.ReadFile(logPath)
	if err != nil {
		return fmt.Errorf("no stdlog at %s: %w", logPath, err)
	}
	text := string(raw)

	deaths := 0
	for _, l := range strings.Split(text, "\n") {
		if strings.Contains(l, "\tSuicide\t") || strings.Contains(l, "\tKill\t") {
			deaths++
		}
	}
	check("a death reached GSdodeathlog", deaths > 0,
		"%d Kill/Suicide line(s) in stdlog.log", deaths)
	check("the log still has its header and map lines",
		strings.Contains(text, "StdLog\t1.22") && strings.Contains(text, "\tMAP\t"),
		"header + MAP present")

	fmt.Println("  --- stdlog.log ---")
	for _, l := range strings.Split(strings.TrimRight(text, "\n"), "\n") {
		fmt.Printf("      %s\n", strings.ReplaceAll(l, "\t", " | "))
	}
	return nil
}
