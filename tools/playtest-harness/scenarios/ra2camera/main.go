// ra2camera -- what a Rocket Arena client is given on spawn, what its observer
// camera does when the mouse moves, and whether a team-mate can hurt it.
//
// Three questions a source read cannot answer:
//
//  1. THE LOADOUT.  arena.cfg's `armor:`/`health:` are inherited global ->
//     map -> arena, and give_ammo() writes the armour as a body-armour COUNT.
//     What the client is actually handed is STAT_HEALTH and STAT_ARMOR, which
//     is what this reads.
//
//  2. THE OBSERVER CAMERA.  RA2's trackcam positions the camera from the
//     observer's OWN view angles and hands the difference to ent->velocity.
//     gi.Pmove then flies the camera along it -- ClientThink sets pm_type 3,
//     PM_GIB, and pmove returns early only for PM_FREEZE -- so that velocity is
//     the camera's motion, not a leftover nobody integrates.
//     SV_CalcViewOffset turns velocity into ps.kick_angles, and the client adds
//     kick_angles to the view before rendering -- so a camera with a standing
//     velocity renders with a tilted horizon.  The sweep turns the mouse
//     through a full circle and reports the worst kick seen.
//
//  3. FRIENDLY FIRE.  Two clients on the same RA2 team, one railgunning the
//     other, and the target's STAT_HEALTH either moves or it does not.  The
//     cross-team shot is the control: if that one does not land either, the
//     aim was wrong rather than the rules.
//
// Exit 0 all checks passed, 1 a check failed, 2 the scenario could not run.
package main

import (
	"flag"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"strings"
	"time"

	"q2playtest/playtest"
	"q2playtest/ra2"
)

// baseq2 stat slots, which Colosseum keeps for all five rulesets.
const (
	statHealth = 1
	statAmmo   = 3
	statArmor  = 5
)

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "", "read-only reference install (RA2 paks + arena.cfg)")
	lib := flag.String("lib", "", "game library under test")
	dir := flag.String("dir", "/tmp/q2playtest/ra2camera", "scratch install dir")
	mapname := flag.String("map", "ra2map9", "map to test on")
	arena := flag.Int("arena", 1, "the map's pickup arena")
	port := flag.Int("port", 27970, "server port")
	// 200/200 because that is what the arena.cfg under test grants: Colosseum's
	// copy sets `armor: 200; health: 200` in the header block, which is the
	// value every arena that names neither inherits.  Stock RA2 shipped
	// `armor: 100` and no `health:` at all -- pass -health 100 -armor 100 for a
	// tree carrying that file unmodified.
	flag.IntVar(&wantHealth, "health", 200, "the health this arena's config should grant")
	flag.IntVar(&wantArmor, "armor", 200, "the armor this arena's config should grant")
	cfg := flag.String("cfg", "", "arena.cfg to install over the reference one")
	label := flag.String("label", "", "label for the report")
	flag.Parse()

	if *q2 == "" || *ref == "" || *lib == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded, -ref and -lib")
		os.Exit(2)
	}
	if err := run(*q2, *ref, *lib, *dir, *mapname, *arena, *port, *label, *cfg); err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	if failed > 0 {
		os.Exit(1)
	}
}

var failed int
var wantHealth, wantArmor int

func check(name string, ok bool, detail string) {
	if ok {
		fmt.Printf("  PASS  %-30s %s\n", name, detail)
		return
	}
	failed++
	fmt.Printf("  FAIL  %-30s %s\n", name, detail)
}

func report(name, detail string) { fmt.Printf("  ..    %-30s %s\n", name, detail) }

func run(q2, ref, lib, dir, mapname string, arena, port int, label, cfg string) error {
	if label != "" {
		fmt.Printf("== %s ==\n", label)
	}
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return err
	}
	if err := playtest.Install(dir, "arena", ref, lib); err != nil {
		return err
	}
	// The reference install carries RA2's own arena.cfg.  A mod that ships its
	// own has to be tested against THAT one, or the loadout check reads the
	// donor's numbers and calls them the mod's.
	if cfg != "" {
		b, err := os.ReadFile(cfg)
		if err != nil {
			return err
		}
		if err := os.WriteFile(filepath.Join(dir, "arena", "arena.cfg"), b, 0o644); err != nil {
			return err
		}
		fmt.Printf("  ..    arena.cfg from %s\n", cfg)
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

	seats := []struct{ name, side string }{
		{"red1", "Red"}, {"red2", "Red"}, {"blue1", "Blue"},
	}
	var bots []*playtest.Bot
	for _, s := range seats {
		b := playtest.NewBot(s.name, "127.0.0.1", port)
		if err := b.Start(30 * time.Second); err != nil {
			return err
		}
		defer b.Disconnect()
		if err := ra2.JoinTeam(b, ra2.PickupTeam(arena, s.side)); err != nil {
			return fmt.Errorf("%s: %w", s.name, err)
		}
		bots = append(bots, b)
	}
	red1, red2, blue1 := bots[0], bots[1], bots[2]

	if err := waitFor(90*time.Second, func() bool {
		for _, b := range bots {
			if b.Spectating() {
				return false
			}
		}
		return true
	}); err != nil {
		return fmt.Errorf("round never started: %w", err)
	}
	fmt.Println("  ..    fighters placed; waiting out the countdown")
	// The countdown is 5 seconds and it is drawn with configstrings, not
	// prints, so there is nothing on the wire to wait for.  Damage is switched
	// on (set_damage(arena, DAMAGE_AIM)) only when it reaches zero -- shoot
	// before that and every check below reads a miss that never had a chance.
	time.Sleep(9 * time.Second)
	red1.WaitFrames(5, 5*time.Second)
	fmt.Println("  ..    round is live, all three are fighters")

	// ---- 1. the loadout ----------------------------------------------------
	for _, b := range bots {
		report("loadout "+b.Name,
			fmt.Sprintf("health=%d armor=%d", b.Stat(statHealth), b.Stat(statArmor)))
	}
	check("spawn health is the config's", red1.Stat(statHealth) == wantHealth,
		fmt.Sprintf("STAT_HEALTH=%d, arena.cfg says health: %d", red1.Stat(statHealth), wantHealth))
	check("spawn armor is the config's", red1.Stat(statArmor) == wantArmor,
		fmt.Sprintf("STAT_ARMOR=%d, arena.cfg says armor: %d", red1.Stat(statArmor), wantArmor))

	// ---- 3. friendly fire, while everyone is still alive --------------------
	//
	// Done before the camera sweep, which needs the round still running.
	//
	// RA2's two switches.  `healthprotect 1` means a same-team attacker -- and
	// OnSameTeam(x,x) is true, so that includes yourself -- takes no health off
	// you at all.  `armorprotect` is the one that VARIES between configs: 2
	// exempts a team-mate and leaves your own splash to eat your armour, 1
	// exempts the shooter as well.  Both are per-arena settings out of
	// arena.cfg, so this scenario must not assume either.
	//
	// So one rocket at red2's own feet, twenty units from red1, answers what is
	// invariant across both settings:
	//
	//    red1 (the team-mate) neither health nor armour moves
	//    red2 (the attacker)  health does not move
	//
	// THE WITNESS THAT THE SHOT LANDED IS THE KNOCKBACK, not the attacker's own
	// armour.  It used to be the armour, and under `armorprotect 1` that reads
	// as "the rocket never went off" on a build where it went off perfectly --
	// which is the row failing for the opposite of its own reason.  T_Damage
	// applies knockback before either protect arm returns, so a rocket at your
	// own feet moves you whichever value is in force.  That is a rocket jump,
	// and being able to make one for free is the point of `armorprotect 1`.
	for _, b := range bots {
		o := b.Origin()
		report("origin "+b.Name, fmt.Sprintf("%.0f %.0f %.0f", o[0], o[1], o[2]))
	}

	// the cross-team shot first, while the round is certainly still live
	xHit := shootAt(blue1, red1, "Railgun")
	report("enemy shot", "blue1 -> red1: "+xHit.String())

	self := snap(red2)
	mate := snap(red1)
	selfWas := red2.Origin()
	ffHit := shootAt(red2, red2, "")
	report("own-feet rocket", "red2: "+ffHit.String())
	report("attacker after", "red2: "+delta(self, snap(red2)))
	report("team-mate after", "red1: "+delta(mate, snap(red1)))
	selfNow, mateNow := snap(red2), snap(red1)

	selfIs := red2.Origin()
	kick := math.Sqrt((selfIs[0]-selfWas[0])*(selfIs[0]-selfWas[0]) +
		(selfIs[1]-selfWas[1])*(selfIs[1]-selfWas[1]) +
		(selfIs[2]-selfWas[2])*(selfIs[2]-selfWas[2]))
	check("the shot landed (the attacker was knocked back)", kick > 1,
		fmt.Sprintf("red2 moved %.0f units, armor %d -> %d (armorprotect 1 keeps it, 2 does not)",
			kick, self[1], selfNow[1]))
	check("a team-mate's splash costs no armour", mateNow[1] >= mate[1],
		fmt.Sprintf("red1 armor %d -> %d", mate[1], mateNow[1]))
	check("a team-mate's splash costs no health", mateNow[0] >= mate[0],
		fmt.Sprintf("red1 health %d -> %d", mate[0], mateNow[0]))
	check("your own splash costs no health", selfNow[0] >= self[0],
		fmt.Sprintf("red2 health %d -> %d", self[0], selfNow[0]))

	// ---- 2. the observer camera --------------------------------------------
	//
	// RA2 gives a fighter no way out of a round on demand: ClientCommand
	// dispatches "kill" to nothing at all and leaves Cmd_Kill_f unreferenced, so
	// a fighter stays one until somebody kills it or the round ends.  A client
	// that joins a team whose arena is already mid-round IS seated as an observer
	// though -- menuAddtoTeam sets FIGHT_SPECTATING and move_to_arena parks it in
	// the arena to watch the round out -- so the camera under test is a fourth
	// client, seated behind the live round.
	cam := playtest.NewBot("watcher", "127.0.0.1", port)
	if err := cam.Start(30 * time.Second); err != nil {
		return err
	}
	defer cam.Disconnect()
	if err := ra2.JoinTeam(cam, ra2.PickupTeam(arena, "Red")); err != nil {
		return fmt.Errorf("watcher: %w", err)
	}
	if err := waitFor(20*time.Second, cam.Spectating); err != nil {
		return fmt.Errorf("watcher never became an observer: %w", err)
	}
	cam.WaitFrames(20, 10*time.Second)

	// free-flying -> trackcam
	tracking := false
	for i := 0; i < 4 && !tracking; i++ {
		cam.Press(playtest.ButtonAttack, 400*time.Millisecond)
		if _, err := cam.WaitPrint(`Tracking `, 2*time.Second); err == nil {
			tracking = true
		}
	}
	// whoever the camera ended up tracking -- it is meant to sit 150 units
	// behind them, so their origin is the yardstick for how far it lags
	subject := red2
	if pr, err := cam.WaitPrint(`Tracking `, time.Second); err == nil {
		for _, b := range bots {
			if strings.Contains(pr, b.Name) {
				subject = b
			}
		}
	}
	if !tracking {
		return fmt.Errorf("the watcher never reached the trackcam")
	}
	cam.WaitFrames(10, 5*time.Second)
	report("observer", fmt.Sprintf("tracking %s, pm_type=%d (3 = PM_GIB, which pmove still runs physics on)",
		subject.Name, cam.PMType()))

	// Sweep the mouse through a full circle in 24 steps, letting the camera
	// settle at each one, and keep the worst kick and the worst roll.
	var worstRoll, worstPitch, worstRange float64
	var moved float64
	prev := cam.Origin()
	for step := 0; step < 24; step++ {
		cam.Look(0, float64(step)*15, 0)
		cam.WaitFrames(4, 3*time.Second)
		k := cam.KickAngles()
		if math.Abs(k[2]) > math.Abs(worstRoll) {
			worstRoll = k[2]
		}
		if math.Abs(k[0]) > math.Abs(worstPitch) {
			worstPitch = k[0]
		}
		o := cam.Origin()
		moved += dist(prev, o)
		prev = o
		if d := dist(o, subject.Origin()); d > worstRange {
			worstRange = d
		}
	}
	v := cam.ViewAngles()
	report("worst camera range", fmt.Sprintf("%.0f units from %s (the camera wants 150)", worstRange, subject.Name))
	report("camera travelled", fmt.Sprintf("%.0f units over the sweep", moved))
	report("view at end", fmt.Sprintf("pitch=%.1f yaw=%.1f roll=%.1f", v[0], v[1], v[2]))
	check("trackcam does not tilt the horizon", math.Abs(worstRoll) < 1.0,
		fmt.Sprintf("worst kick_angles ROLL over a 360 sweep = %.2f deg", worstRoll))
	check("trackcam does not pitch the view", math.Abs(worstPitch) < 1.0,
		fmt.Sprintf("worst kick_angles PITCH over a 360 sweep = %.2f deg", worstPitch))

	return nil
}

// snap is a client's {health, armor} at this instant.
func snap(b *playtest.Bot) [2]int {
	return [2]int{b.Stat(statHealth), b.Stat(statArmor)}
}

func delta(before, after [2]int) string {
	return fmt.Sprintf("health %d -> %d, armor %d -> %d",
		before[0], after[0], before[1], after[1])
}

type shot struct {
	hpBefore, hpAfter     int
	apBefore, apAfter     int
	ammoBefore, ammoAfter int
	rng                   float64
	aimed, wanted         [3]float64
}

func (s shot) hurt() bool  { return s.hpAfter < s.hpBefore || s.apAfter < s.apBefore }
func (s shot) fired() bool { return s.ammoAfter < s.ammoBefore }
func (s shot) String() string {
	return fmt.Sprintf("health %d->%d armor %d->%d ammo %d->%d range %.0f aim(p,y) want %.1f,%.1f got %.1f,%.1f",
		s.hpBefore, s.hpAfter, s.apBefore, s.apAfter, s.ammoBefore, s.ammoAfter,
		s.rng, s.wanted[0], s.wanted[1], s.aimed[0], s.aimed[1])
}

// shootAt points the shooter at the target and fires three rockets -- the
// rocket launcher is what give_ammo() selects on an arena spawn, and its splash
// forgives the aim in a way a railgun does not.  It reports what the target's
// health and armour did, and the shooter's ammo, which is the witness that the
// weapon fired at all.
func shootAt(shooter, target *playtest.Bot, weapon string) shot {
	if weapon != "" {
		shooter.Cmd("use %s", weapon)
		shooter.WaitFrames(8, 4*time.Second)
	}
	// ps.viewangles = usercmd.angles + delta_angles, and delta_angles is
	// whatever the spawn left behind.  Measure it once, then command through it.
	shooter.Look(0, 0, 0)
	shooter.WaitFrames(6, 3*time.Second)
	delta := shooter.ViewAngles()

	src, dst := shooter.Origin(), target.Origin()
	yaw, pitch := 0.0, 89.0 // straight down, for a shot at your own feet
	if shooter != target {
		dx, dy, dz := dst[0]-src[0], dst[1]-src[1], (dst[2]+4)-(src[2]+22)
		yaw = math.Atan2(dy, dx) * 180 / math.Pi
		pitch = -math.Atan2(dz, math.Hypot(dx, dy)) * 180 / math.Pi
	}
	dx, dy, dz := dst[0]-src[0], dst[1]-src[1], dst[2]-src[2]

	s := shot{hpBefore: target.Stat(statHealth), apBefore: target.Stat(statArmor),
		ammoBefore: shooter.Stat(statAmmo), rng: math.Sqrt(dx*dx + dy*dy + dz*dz)}

	shooter.Look(pitch-delta[0], yaw-delta[1], 0)
	shooter.WaitFrames(8, 4*time.Second)
	s.aimed = shooter.ViewAngles()
	s.wanted = [3]float64{pitch, yaw, 0}
	shots := 3
	if shooter == target {
		shots = 1 // one is enough at point blank, and three would be a suicide
	}
	for i := 0; i < shots; i++ {
		shooter.Press(playtest.ButtonAttack, 300*time.Millisecond)
		shooter.WaitFrames(12, 4*time.Second)
	}
	target.WaitFrames(5, 3*time.Second)
	s.hpAfter, s.apAfter = target.Stat(statHealth), target.Stat(statArmor)
	s.ammoAfter = shooter.Stat(statAmmo)
	return s
}

func dist(a, b [3]float64) float64 {
	return math.Sqrt((a[0]-b[0])*(a[0]-b[0]) + (a[1]-b[1])*(a[1]-b[1]) + (a[2]-b[2])*(a[2]-b[2]))
}

func waitFor(timeout time.Duration, cond func() bool) error {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if cond() {
			return nil
		}
		time.Sleep(100 * time.Millisecond)
	}
	return fmt.Errorf("timed out after %s", timeout)
}
