// xatrixtune -- R-MP-25 to R-MP-29: do the eight settings this tree gives the
// Reckoning's trap and Ion Ripper, after openffa-xatrix, change what the running
// game does, under more than one ruleset?
//
// The trap and the Ion Ripper are shared code, so each setting claims every
// ruleset.  This boots one whole server per ruleset -- `dm`, OSP's
// free-for-all, and `ctf`, Threewave's -- and asks the WORLD, with both signs
// per setting: a value that must change something, and a control on the same
// server that must not.  The settings are plain cvars read where they act, so
// one server serves both signs.
//
//	held      R-MP-26  a trap held past its timer goes off in the hand.  The
//	                   thrower's own STAT_HEALTH is the receipt: the default
//	                   125 over 165 hurts, `sv_trap_held_damage 0` does not,
//	                   and `sv_trap_held_radius 1` leaves the thrower outside
//	                   its own blast.
//	ripper    R-MP-28  an ion fired straight down hits the floor, bounces, and
//	                   comes back up through its shooter.  `ripper_self 0`
//	                   lets it pass; 10 takes exactly 10; under Quad the shot
//	                   takes its full 120.
//	duration  R-MP-25  the server's own entity count (`entcount`), which sees a
//	                   trap wherever it lands: one thrown under
//	                   `sv_trap_duration 2` is gone four seconds later, one
//	                   under 8 is not, and is gone by ten.
//	shoot     R-MP-27  the thrower rails its own trap.  `sv_trap_health 0`
//	                   leaves it standing; `sv_trap_health 50` breaks it, and
//	                   the blast is credited to the thrower: a second client
//	                   beside it dies "was blown up by <thrower>'s trap".
//	quadpull  R-MP-29  a trap thrown at the thrower's feet pulls the thrower
//	                   in.  The receipt is the speed each of its thinks gives
//	                   the thrower: the first step, divided by the thinks
//	                   behind it, which the trap's own frame counts.  With
//	                   Quad, `sv_trap_quad_pull 4` gives at least twice what 1
//	                   gives; without Quad, 4 changes nothing.
//
// `-phases` runs a subset, by those names.
//
// A libq2 client cannot steer round geometry, so getting two clients into one
// place is a noclip flight, as in `ospff`.  Keeping clear of a trap is a long
// throw: released 2.5 seconds into its cook, a trap leaves at about 700 units a
// second and lands well outside the 256 units it pulls from.
package main

import (
	"flag"
	"fmt"
	"math"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
)

const (
	statHealth = 1
	statAmmo   = 3
	trapModel  = "models/weapons/z_trap/tris.md2"
	pullRange  = 256.0
)

var pass, fail int

// phases is the -phases set; every phase runs when it is empty.
var phases = map[string]bool{}

func want(phase string) bool { return len(phases) == 0 || phases[phase] }

func ok(what, detail string)  { pass++; fmt.Printf("  [ ok ] %-58s %s\n", what, detail) }
func bad(what, detail string) { fail++; fmt.Printf("  [FAIL] %-58s %s\n", what, detail) }
func skip(what, why string)   { fmt.Printf("  [skip] %-58s %s\n", what, why) }
func note(f string, a ...any) { fmt.Printf("  ....  "+f+"\n", a...) }

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

// lookAt points the VIEW at a spot, closing the loop on ps.viewangles because
// pmove adds delta_angles the client cannot see (see `ospff`).
func lookAt(b *playtest.Bot, from func() [3]float64, to [3]float64, tol float64) bool {
	wp, wy := aim(from(), to)
	return lookAngles(b, wp, wy, tol)
}

// railAt points the view so that a RAIL hits `to`.  The rail does not leave
// from the eye: weapon_railgun_fire projects its start 7 units to the right
// and viewheight-8 up from the origin (P_ProjectSource, `hand 0`), and traces
// along the view from there -- a line parallel to the eye's, 8 units below it
// and 7 to the side, which passes clean under an 8-unit trap aimed at from the
// eye.  The right vector depends on the yaw alone, so two passes settle it.
func railAt(b *playtest.Bot, to [3]float64, tol float64) bool {
	o := b.Origin()
	_, yaw := aim(b.Eye(), to)
	var wp, wy float64
	for i := 0; i < 3; i++ {
		s, c := math.Sincos(yaw * math.Pi / 180)
		muzzle := [3]float64{o[0] + 7*s, o[1] - 7*c, o[2] + 22 - 8}
		wp, wy = aim(muzzle, to)
		yaw = wy
	}
	return lookAngles(b, wp, wy, tol)
}

func lookAngles(b *playtest.Bot, wp, wy, tol float64) bool {
	rp, ry := wp, wy
	for i := 0; i < 10; i++ {
		b.Look(rp, ry, 0)
		b.WaitFrames(3, 5*time.Second)
		got := b.ViewAngles()
		ep := wp - got[0]
		ey := math.Mod(wy-got[1]+540, 360) - 180
		if math.Abs(ep) < tol && math.Abs(ey) < tol {
			return true
		}
		rp += ep
		ry += ey
	}
	return false
}

// closeIn flies `b` at `to` on noclip until it is within `want` units, in bursts
// proportional to the distance left (see `ospff`).
func closeIn(b *playtest.Bot, to func() [3]float64, want float64) float64 {
	d := dist(b.Origin(), to())
	for i := 0; i < 24 && d > want; i++ {
		lookAt(b, b.Origin, to(), 2)
		hold := time.Duration(d/800*1000) * time.Millisecond
		if hold > 700*time.Millisecond {
			hold = 700 * time.Millisecond
		} else if hold < 60*time.Millisecond {
			hold = 60 * time.Millisecond
		}
		b.Walk(400, 0, hold)
		d = dist(b.Origin(), to())
	}
	return d
}

// modelsBase is CS_MODELS in the layout this client negotiated: model 1 is
// always the world, so its configstring sits one past the base.
func modelsBase(b *playtest.Bot) int {
	base := -1
	for i, s := range b.ConfigStrings() {
		d := playtest.Decode(s)
		if strings.HasPrefix(d, "maps/") && strings.HasSuffix(d, ".bsp") {
			if base < 0 || i-1 < base {
				base = i - 1
			}
		}
	}
	return base
}

// modelIndex is the modelindex the server gave `path`, or -1 before it has
// registered it -- the trap's model is registered by the first trap thrown.
func modelIndex(b *playtest.Bot, path string) int {
	base := modelsBase(b)
	if base < 0 {
		return -1
	}
	for i, s := range b.ConfigStrings() {
		if playtest.Decode(s) == path {
			return i - base
		}
	}
	return -1
}

// holding says whether the client's view weapon is the model named by `frag`.
func holding(b *playtest.Bot, frag string) bool {
	base := modelsBase(b)
	if base < 0 {
		return false
	}
	return strings.Contains(playtest.Decode(b.ConfigString(base+b.GunIndex())), frag)
}

// arm gives `b` a weapon and its ammunition and waits for it in hand.
func arm(b *playtest.Bot, frag string, cmds ...string) bool {
	for try := 0; try < 3; try++ {
		for _, c := range cmds {
			b.Cmd("%s", c)
		}
		b.WaitFrames(15, 10*time.Second)
		if holding(b, frag) {
			return true
		}
	}
	return false
}

var entRe = regexp.MustCompile(`(\d+) entites active`)

// entcount is the server's own count of entities in use.  It is the one view
// of a trap that does not depend on what any client can see.
func entcount(srv *playtest.Server, b *playtest.Bot) int {
	mark := srv.Len()
	b.Cmd("entcount")
	line, err := srv.WaitLogFrom(mark, entRe.String(), 5*time.Second)
	if err != nil {
		return -1
	}
	n, _ := strconv.Atoi(entRe.FindStringSubmatch(line)[1])
	return n
}

// alive respawns `b` if it is dead and reports whether it is standing again.
func alive(b *playtest.Bot) bool {
	for i := 0; i < 8; i++ {
		if b.PMType() == playtest.PMNormal && b.Stat(statHealth) > 0 {
			return true
		}
		b.Press(playtest.ButtonAttack, 150*time.Millisecond)
		b.WaitFrames(10, 5*time.Second)
	}
	return b.PMType() == playtest.PMNormal && b.Stat(statHealth) > 0
}

// enter puts a freshly connected client into play.  The OSP rulesets seat a
// newcomer as an observer until it says `join`; Threewave seats one in its join
// menu, and `team` is the command that menu runs.  (`ctf_forcejoin` only takes
// the other team's row out of that menu.)
func enter(b *playtest.Bot, rs colosseum.Ruleset, team string) {
	b.WaitFrames(10, 20*time.Second)
	if colosseum.IsOSP(rs.Name) {
		b.Cmd("join")
	} else if rs.Name == "ctf" {
		b.Cmd("team %s", team)
	}
	b.WaitFrames(20, 20*time.Second)
}

// respawn kills `b` and stands it up again at a spawn point: on the floor, out
// of noclip, its powerups gone.  It puts the blaster up first, because a death
// drops the weapon in hand and a dropped weapon frees itself thirty seconds
// later -- one entity off `entcount` in the middle of whatever is counting then.
func respawn(b *playtest.Bot) bool {
	b.Cmd("use blaster")
	b.WaitFrames(15, 10*time.Second)
	b.Cmd("kill")
	b.WaitFrames(20, 10*time.Second)
	if !alive(b) {
		return false
	}
	// Past any spawn protection before the next damage is measured.
	b.WaitFrames(40, 10*time.Second)
	return true
}

// throwLong throws a trap released 2.5 seconds into its cook, and returns once
// it has left the hand: STAT_AMMO, the trap count, drops by one.
func throwLong(b *playtest.Bot) bool {
	before := b.Stat(statAmmo)
	b.Hold(playtest.ButtonAttack)
	time.Sleep(3500 * time.Millisecond)
	b.Release(playtest.ButtonAttack)
	_, err := b.WaitStat(statAmmo, before-1, 3*time.Second)
	return err == nil
}

func main() {
	q2 := flag.String("q2proded", "", "the dedicated server")
	lib := flag.String("lib", "", "the game library under test")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "retail baseq2")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "Threewave's paks")
	xat := flag.String("xatrix", "/usr/share/games/quake2/xatrix", "the Reckoning's paks")
	dir := flag.String("dir", "/tmp/q2playtest/xatrixtune", "scratch")
	port := flag.Int("port", 27996, "first server port, one per ruleset")
	rulesets := flag.String("rulesets", "dm,ctf", "rulesets to run the battery under")
	only := flag.String("phases", "", "comma-separated phases to run (held, ripper, duration, "+
		"shoot, quadpull); all of them when empty")
	flag.Parse()
	for _, ph := range strings.Split(*only, ",") {
		if ph = strings.TrimSpace(ph); ph != "" {
			phases[ph] = true
		}
	}

	if _, err := os.Stat(filepath.Join(*xat, "pak0.pak")); err != nil {
		fmt.Printf("xatrixtune: no Reckoning pak at %s -- the trap and the Ion Ripper "+
			"need its models and sounds\n", *xat)
		os.Exit(2)
	}
	for i, name := range strings.Split(*rulesets, ",") {
		rs, found := colosseum.Find(strings.TrimSpace(name))
		if !found {
			fmt.Printf("xatrixtune: unknown ruleset %q\n", name)
			os.Exit(2)
		}
		run(*q2, *lib, *ref, *ctf, *xat, filepath.Join(*dir, rs.Name), *port+i, rs)
	}

	fmt.Printf("\n%d check(s), %d failed\n", pass+fail, fail)
	if fail > 0 {
		os.Exit(1)
	}
}

func run(q2, lib, ref, ctf, xat, dir string, port int, rs colosseum.Ruleset) {
	fmt.Printf("\n##### %s on %s (port %d)\n", rs.Name, rs.Map, port)

	if err := colosseum.Install(dir, ref, ctf, lib); err != nil {
		panic(err)
	}
	// pak10: baseq2 takes 0..3 and CTF 8..9 (as in `layeracc`).
	pak := filepath.Join(dir, "colosseum", "pak10.pak")
	os.Remove(pak)
	abs, _ := filepath.Abs(filepath.Join(xat, "pak0.pak"))
	if err := os.Symlink(abs, pak); err != nil {
		panic(err)
	}

	cvars := map[string]string{
		"g_ruleset": rs.Name, "xatrix": "1", "cheats": "1", "flood_msgs": "0",
		"timelimit": "0", "fraglimit": "0", "capturelimit": "0",
		"bots": "0", "bots_minplayers": "0", "minimumplayers": "0", "botfill": "0",
	}
	for k, v := range rs.Cvars {
		cvars[k] = v
	}
	srv := &playtest.Server{
		Binary: q2, Dir: dir, Game: "colosseum", Map: rs.Map, Port: port,
		MaxClients: 8, Cvars: cvars, LogPath: dir + "/server.log",
	}
	if err := srv.Start(); err != nil {
		panic(err)
	}
	defer srv.Stop()

	tag := rs.Name + "/"
	t := playtest.NewBot("tosser", "127.0.0.1", port)
	if err := t.Start(30 * time.Second); err != nil {
		panic(err)
	}
	defer t.Disconnect()
	enter(t, rs, "red")
	if !alive(t) {
		skip(tag+"everything", "the client never stood up in the game")
		return
	}
	t.WaitFrames(40, 10*time.Second) // past any spawn protection

	if want("held") {
		held(srv, t, tag)
	}
	if want("ripper") {
		ripper(srv, t, tag)
	}
	if want("duration") {
		if !respawn(t) {
			skip(tag+"duration", "the client did not come back after `kill`")
			return
		}
		duration(srv, t, tag)
	}
	if want("shoot") {
		shoot(srv, t, port, rs, tag)
	}
	if want("quadpull") {
		quadpull(srv, t, tag)
	}
}

// held: R-MP-26, sv_trap_held_damage and sv_trap_held_radius.
func held(srv *playtest.Server, t *playtest.Bot, tag string) {
	if !arm(t, "v_trap", "give trap 10", "use trap") {
		skip(tag+"held: a trap in hand", "the view weapon never became the trap")
		return
	}
	cases := []struct {
		what     string
		dmg, rad string
		wantHurt bool
	}{
		{"held: the default 125 over 165 hurts the thrower", "125", "165", true},
		{"held: sv_trap_held_damage 0 hurts nobody", "0", "165", false},
		{"held: sv_trap_held_radius 1 leaves the thrower outside", "125", "1", false},
	}
	for _, c := range cases {
		srv.Console("set sv_trap_held_damage %s; set sv_trap_held_radius %s", c.dmg, c.rad)
		t.Cmd("give health 200")
		t.Cmd("use trap")
		t.WaitFrames(15, 10*time.Second)
		before, traps := t.Stat(statHealth), t.Stat(statAmmo)
		// The cook starts at gunframe 11, a second in, and runs 3.2 seconds:
		// held for five, the trap goes off in the hand.
		t.Hold(playtest.ButtonAttack)
		time.Sleep(5 * time.Second)
		t.Release(playtest.ButtonAttack)
		t.WaitFrames(15, 10*time.Second)
		after := t.Stat(statHealth)
		if t.Stat(statAmmo) != traps-1 {
			skip(tag+c.what, fmt.Sprintf("no trap left the hand (traps %d -> %d)",
				traps, t.Stat(statAmmo)))
			continue
		}
		detail := fmt.Sprintf("STAT_HEALTH %d -> %d", before, after)
		switch {
		case c.wantHurt && after < before:
			ok(tag+c.what, detail)
		case !c.wantHurt && after == before:
			ok(tag+c.what, detail)
		default:
			bad(tag+c.what, detail)
		}
	}
	srv.Console("set sv_trap_held_damage 125; set sv_trap_held_radius 165")
}

// ripper: R-MP-28, ripper_self.
func ripper(srv *playtest.Server, t *playtest.Bot, tag string) {
	if !arm(t, "v_boomer", "give ionripper", "give cells 200", "use ionripper") {
		skip(tag+"ripper: an Ion Ripper in hand", "the view weapon never became it")
		return
	}
	feet := t.Origin()
	feet[2] -= 400
	// pmove clamps pitch at 89, so straight down settles within 2 degrees.
	if !lookAt(t, t.Origin, feet, 2) {
		note("the view never settled straight down: %v", t.ViewAngles())
	}
	cases := []struct {
		what string
		self string
		quad bool
		want int
	}{
		{"ripper: ripper_self 0 -- the ion passes its shooter", "0", false, 0},
		{"ripper: ripper_self 10 -- the ion takes exactly 10", "10", false, 10},
		{"ripper: under Quad the ion takes its full 120", "10", true, 120},
	}
	for _, c := range cases {
		srv.Console("set ripper_self %s", c.self)
		if c.quad {
			t.Cmd("give quad damage")
			t.WaitFrames(5, 5*time.Second)
			t.Cmd("use quad damage")
		}
		t.Cmd("give health 200")
		t.WaitFrames(15, 10*time.Second)
		before, cells := t.Stat(statHealth), t.Stat(statAmmo)
		// A press can fail to reach the game at all: q2pro's sv_enforcetime
		// drops some of this client's moves (selfPull).  A press that spent
		// no cell fired nothing, so it is pressed again.
		for press := 0; press < 3 && t.Stat(statAmmo) >= cells; press++ {
			t.Press(playtest.ButtonAttack, 150*time.Millisecond)
			t.WaitFrames(25, 10*time.Second)
		}
		after := t.Stat(statHealth)
		if t.Stat(statAmmo) >= cells {
			skip(tag+c.what, fmt.Sprintf("three presses and the shot never left (cells %d -> %d)",
				cells, t.Stat(statAmmo)))
			continue
		}
		detail := fmt.Sprintf("STAT_HEALTH %d -> %d, wanted %d off", before, after, c.want)
		if before-after == c.want {
			ok(tag+c.what, detail)
		} else {
			bad(tag+c.what, detail)
		}
	}
	srv.Console("set ripper_self 0")
}

// duration: R-MP-25, sv_trap_duration.
func duration(srv *playtest.Server, t *playtest.Bot, tag string) {
	if !arm(t, "v_trap", "give trap 10", "use trap") {
		skip(tag+"duration: a trap in hand", "the view weapon never became the trap")
		return
	}
	cases := []struct {
		secs  string
		at    time.Duration
		alive bool
		what  string
	}{
		{"2", 4 * time.Second, false, "duration: under sv_trap_duration 2, gone at 4 s"},
		{"8", 4 * time.Second, true, "duration: under sv_trap_duration 8, there at 4 s"},
		{"8", 10 * time.Second, false, "duration: ...and gone at 10 s"},
	}
	var t0 time.Time
	var base int
	// On noclip for the whole phase: after each throw the thrower backs away
	// from the trap's pull, and on q2ctf1 a step back off a ledge followed by
	// noclip off was a fatal fall.  `shoot` stands it up again by respawning.
	t.Cmd("noclip")
	t.WaitFrames(5, 5*time.Second)
	for i, c := range cases {
		if i == 0 || c.secs != cases[i-1].secs {
			srv.Console("set sv_trap_duration %s", c.secs)
			t.Cmd("use trap")
			t.WaitFrames(15, 10*time.Second)
			base = entcount(srv, t)
			if !throwLong(t) {
				skip(tag+c.what, "no trap left the hand")
				return
			}
			t0 = time.Now()
			// Clear of the trap's pull whatever it landed on.
			t.Walk(-400, 0, 300*time.Millisecond)
			if n := entcount(srv, t); n != base+1 {
				skip(tag+c.what, fmt.Sprintf("the trap is not counted: %d entities "+
					"before the throw, %d after", base, n))
				return
			}
		}
		time.Sleep(time.Until(t0.Add(c.at)))
		n := entcount(srv, t)
		detail := fmt.Sprintf("%d entities, %d before the throw", n, base)
		if (n == base+1) == c.alive && (n == base || n == base+1) {
			ok(tag+c.what, detail)
		} else {
			bad(tag+c.what, detail)
		}
	}
	srv.Console("set sv_trap_duration 30")
}

// shoot: R-MP-27, sv_trap_health and the explosion pair.
func shoot(srv *playtest.Server, t *playtest.Bot, port int, rs colosseum.Ruleset, tag string) {
	v := playtest.NewBot("witness", "127.0.0.1", port)
	if err := v.Start(30 * time.Second); err != nil {
		skip(tag+"shoot", "the witness never connected")
		return
	}
	defer v.Disconnect()
	// Under `ctf` the witness is on the other team: the blast has to be a frag,
	// and Threewave takes friendly fire out of T_Damage.
	enter(v, rs, "blue")
	if !alive(v) {
		skip(tag+"shoot", "the witness never stood up in the game")
		return
	}

	// The blast is made big enough to reach the witness wherever the two end
	// up, and lethal, so the obituary is the receipt.
	srv.Console("set sv_trap_expl_damage 1000; set sv_trap_expl_radius 3000")
	rounds := []struct {
		health string
		what   string
	}{
		{"0", "shoot: sv_trap_health 0 -- the railed trap stands"},
		{"50", "shoot: sv_trap_health 50 -- the thrower's rail breaks it"},
	}
	for _, r := range rounds {
		verdict, judged := false, 0
		for attempt := 1; attempt <= 3 && !verdict; attempt++ {
			var tried bool
			verdict, tried = shootRound(srv, t, v, r.health, r.what, tag, attempt)
			if tried {
				judged++
			}
		}
		switch {
		case verdict:
		case judged > 0:
			// A rail that misses is the approach failing, so a miss is
			// retried.  A shootable trap that three whole attempts could not
			// break is the rule failing.
			bad(tag+r.what, fmt.Sprintf("%d attempt(s) of up to three rails, "+
				"and the trap was never broken", judged))
		default:
			skip(tag+r.what, "three attempts and none that could be judged (see the notes)")
		}
	}
	srv.Console("set sv_trap_health 0; set sv_trap_expl_damage 200; set sv_trap_expl_radius 300")
}

// settledTrap finds the thrower's trap in its own frame and waits until it
// lies still: a long throw bounces and slides, and a position read while it
// moved aims at where it was.  It answers the entity number, so the rest of
// the round watches THAT trap rather than counting entities -- under `ctf` the
// techs come and go on their own clock, and a count moves with them.
func settledTrap(t *playtest.Bot, m int, until time.Time) (int, [3]float64, bool) {
	var last [3]float64
	lastN := -1
	for time.Now().Before(until) {
		for _, n := range t.EntitiesWithModel(m) {
			if o, ok := t.EntityOrigin(n); ok {
				if n == lastN && dist(o, last) < 0.5 {
					return n, o, true
				}
				lastN, last = n, o
			}
		}
		time.Sleep(400 * time.Millisecond)
	}
	return -1, last, false
}

// present says whether entity n is still drawn with model m: a freed trap
// leaves the frame, and its slot may come back as something else entirely.
func present(t *playtest.Bot, n, m int) bool {
	got, ok := t.EntityModel(n)
	return ok && got == m
}

// shootRound is one throw and up to three rails.  It answers whether it
// reported a verdict, and whether the rails were fired at all: an attempt whose
// trap landed out of sight or within pulling range was never tried, and a
// positive round whose rails all missed was tried and is retried -- the rule
// under test is what a HIT does.
func shootRound(srv *playtest.Server, t, v *playtest.Bot, health, what, tag string, attempt int) (verdict, tried bool) {
	// Both stand up fresh at a spawn point, on the floor and off noclip.
	if !respawn(t) || !respawn(v) {
		return false, false
	}
	// Twenty seconds: long enough that no trap expires while it is being
	// shot at, so a trap that leaves the frame was broken, not timed out.
	srv.Console("set sv_trap_health %s; set sv_trap_duration 20", health)

	// The witness stands beside the thrower, so the blast that reaches one
	// reaches the other.
	v.Cmd("noclip")
	if d := closeIn(v, t.Origin, 70); d > 120 {
		note("the witness got only to %.0f units", d)
		return false, false
	}
	if !arm(t, "v_trap", "give trap 10", "use trap") {
		return false, false
	}
	if !throwLong(t) {
		return false, false
	}
	thrown := time.Now()

	// Find the trap the thrower can see, and keep the round only if it landed
	// outside the pull: a trap that catches a client resets its clock and
	// proves nothing about being shot.
	time.Sleep(1500 * time.Millisecond)
	m := modelIndex(t, trapModel)
	n, trap, seen := settledTrap(t, m, thrown.Add(5*time.Second))
	if m < 0 || !seen {
		note("attempt %d: the thrower cannot see its trap at rest (model %d)", attempt, m)
		return false, false
	}

	if !arm(t, "v_rail", "give railgun", "give slugs 20", "use railgun") {
		return false, false
	}
	// From above.  A rail leaves 38 units over the floor, and at the distance a
	// long throw lands it grazes the floor all the way to an 8-unit trap: any
	// step or lip on the way stops it.  Risen on noclip, the thrower looks down
	// on the trap instead, and the witness comes up beside it.
	t.Cmd("noclip")
	lookAngles(t, -89, t.ViewAngles()[1], 3)
	t.Walk(400, 0, 150*time.Millisecond)
	if d := closeIn(v, t.Origin, 70); d > 120 {
		note("attempt %d: the witness got only to %.0f units of the risen thrower", attempt, d)
		return false, false
	}
	d := dist(t.Origin(), trap)
	if d < pullRange+60 || dist(v.Origin(), trap) < pullRange+60 {
		note("attempt %d: the trap is %.0f units away -- inside the pull", attempt, d)
		return false, false
	}
	// Armed by now: frame 4 comes four thinks after the trap comes to rest.
	time.Sleep(time.Until(thrown.Add(4 * time.Second)))
	if !present(t, n, m) {
		note("attempt %d: the trap left the frame before the first shot", attempt)
		return false, false
	}

	const obit = "witness was blown up by tosser's trap"
	mark := srv.Len()
	vBefore := v.Stat(statHealth)
	shots, presses, gone := 0, 0, false
	for shots < 3 && presses < 6 && !gone {
		target := trap
		target[2] += 4 // the middle of its 8-unit box
		if !railAt(t, target, 0.15) {
			note("attempt %d: the aim never settled within 0.15 degrees", attempt)
		}
		// A rail is a shot once it spends a slug.  A press that spent none
		// never reached the game (ripper), so it is pressed again, and a miss
		// is never confused with a trap that a hit would not break.
		slugs := t.Stat(statAmmo)
		t.Press(playtest.ButtonAttack, 150*time.Millisecond)
		presses++
		t.WaitFrames(12, 10*time.Second)
		if t.Stat(statAmmo) >= slugs {
			continue
		}
		shots++
		gone = !present(t, n, m)
		// A rail hands a trap it does not break a kick: aim at where it is now.
		if o, ok := t.EntityOrigin(n); ok && !gone {
			trap = o
		}
	}
	note("attempt %d: trap %d at %.0f units, %d rail(s) in %d press(es), gone: %v",
		attempt, n, d, shots, presses, gone)
	if shots == 0 {
		// Nothing was fired, so nothing was tested: the trap runs out, and
		// the round is tried again.
		time.Sleep(time.Until(thrown.Add(21 * time.Second)))
		return false, false
	}

	if health == "0" {
		_, missing := srv.WaitLogFrom(mark, regexp.QuoteMeta(obit), 2*time.Second)
		if !gone && missing != nil && v.Stat(statHealth) == vBefore {
			ok(tag+what, fmt.Sprintf("still there after %d rail(s), witness health %d -> %d",
				shots, vBefore, v.Stat(statHealth)))
		} else {
			bad(tag+what, fmt.Sprintf("gone %v after %d rail(s), witness health %d -> %d, "+
				"obituary seen: %v", gone, shots, vBefore, v.Stat(statHealth), missing == nil))
		}
		// The control trap runs out before the next round begins.
		time.Sleep(time.Until(thrown.Add(21 * time.Second)))
		return true, true
	}
	if !gone {
		// Missed, or not shootable: the caller retries, and judges three.
		time.Sleep(time.Until(thrown.Add(21 * time.Second)))
		return false, true
	}
	line, err := srv.WaitLogFrom(mark, regexp.QuoteMeta(obit), 5*time.Second)
	if err != nil && v.Stat(statHealth) == vBefore {
		// The blast never touched the witness.  T_RadiusDamage hurts only what
		// the trap can see, and a witness flown in on noclip can end up inside
		// a wall.  Nothing about the credit was tested, so the round is tried
		// again rather than judged.
		note("attempt %d: broken by rail %d, but the blast never reached the witness", attempt, shots)
		return false, false
	}
	ok(tag+what, fmt.Sprintf("broken by rail %d", shots))
	if err == nil {
		ok(tag+"shoot: the blast is the thrower's -- "+obit, strings.TrimSpace(line))
	} else {
		bad(tag+"shoot: the blast is the thrower's -- "+obit,
			fmt.Sprintf("no such obituary; witness health %d -> %d", vBefore, v.Stat(statHealth)))
	}
	return true, true
}

// quadpull throws a trap at the thrower's own feet.  On a server of one the
// trap pulls its own thrower, because the Reckoning comments out the test that
// would spare a trap's owner, so nobody has to be flown into place.

// pullRun is what one such trap did to its thrower.
type pullRun struct {
	trap    [3]float64 // where the trap lay when it first pulled
	from    float64    // how far it lay from the thrower then, horizontally
	level   bool       // it lay on the thrower's own floor
	step    float64    // the thrower's first move under the pull, horizontally
	thinks  int        // how many of the trap's thinks had pulled by then
	counted bool       // the trap's own frame agrees with that count
	toward  float64    // the cosine between that move and the way to the trap
	died    bool
}

// perThink is the speed one think of the trap added to the thrower.
func (r pullRun) perThink() float64 { return r.step / (0.1 * float64(r.thinks)) }

func (r pullRun) String() string {
	return fmt.Sprintf("trap %.0f units off; first step %.1f units after %d think(s), "+
		"%.0f units/s a think; died %v",
		r.from, r.step, r.thinks, r.perThink(), r.died)
}

func hdist(a, b [3]float64) float64 { return math.Hypot(a[0]-b[0], a[1]-b[1]) }

// selfPull stands the thrower up afresh, with Quad if asked, and throws a trap
// straight down.  pmove clamps pitch at 89, so the throw still leaves at about
// 200 units a second along the floor, and the trap bounces to rest some 150
// units ahead: inside the 256 it pulls from, and far enough out that the pull
// shows before the catch.  It then watches the thrower frame by frame for five
// seconds, or until it dies.  It answers false when there was nothing to judge:
// no throw, or no first pull it could time.
//
// The first step has to be divided by the thinks behind it.  A think adds the
// pull to the thrower's velocity whether or not the thrower moves that frame.
// This client sends about two moves a frame, and q2pro's `sv_enforcetime`
// answers that by running none of its moves for the rest of a 1.6-second
// window: with it on, a first step carried up to seven pulls, and a Quad trap's
// seven threw the thrower across the room into a wall, which cut the step
// short.  So quadpull turns `sv_enforcetime` off, and the thinks are still
// counted.  The trap's frame steps from 0 to 1 on the think on which it first
// pulls, so the thinks behind a step made in frame F are F minus that frame.  A
// step is in proportion to the speed behind it -- the pull lifts the thrower,
// so its first move is airborne -- and the speed per think is what is
// compared, only ever against the same server's own control.
func selfPull(t *playtest.Bot, quad bool) (pullRun, bool) {
	var r pullRun
	if !respawn(t) {
		return r, false
	}
	if quad {
		t.Cmd("give quad damage")
		t.WaitFrames(5, 5*time.Second)
		t.Cmd("use quad damage")
	}
	if !arm(t, "v_trap", "give trap 10", "use trap") {
		return r, false
	}
	lookAngles(t, 89, t.ViewAngles()[1], 2)
	before := t.Stat(statAmmo)
	// Released at once, the trap is thrown when its cook starts, a second on.
	t.Press(playtest.ButtonAttack, 150*time.Millisecond)
	if _, err := t.WaitStat(statAmmo, before-1, 4*time.Second); err != nil {
		return r, false
	}
	m := modelIndex(t, trapModel)

	type sample struct {
		f    int
		o    [3]float64
		n    int // the trap's entity number, -1 while it is not in the frame
		tf   int // the trap's animation frame
		trap [3]float64
	}
	var ss []sample
	n := -1
	for until := time.Now().Add(5 * time.Second); time.Now().Before(until); {
		// Frame and origin are taken together, or not at all: a frame that
		// lands between the two reads would pair one frame's number with the
		// next one's position.
		f := t.Frame()
		o, h := t.Origin(), t.Stat(statHealth)
		if t.Frame() != f || (len(ss) > 0 && ss[len(ss)-1].f == f) {
			time.Sleep(10 * time.Millisecond)
			continue
		}
		x := sample{f: f, o: o, n: -1}
		if m < 0 {
			// The first trap of the level registers its model as it is
			// thrown, and the configstring can trail the ammo count.
			m = modelIndex(t, trapModel)
		}
		if n < 0 && m >= 0 {
			if ns := t.EntitiesWithModel(m); len(ns) > 0 {
				n = ns[0]
			}
		}
		if n >= 0 && present(t, n, m) {
			x.n = n
			x.trap, _ = t.EntityOrigin(n)
			x.tf, _ = t.EntityFrame(n)
		}
		ss = append(ss, x)
		if h <= 0 {
			r.died = true
			break
		}
	}

	// The first pull: the trap's frame steps from 0 to 1 between two
	// consecutive frames.  The first move: the first frame the thrower is
	// somewhere else.  Until the trap pulls, nothing moves it.
	pulled, first := -1, -1
	for i := 1; i < len(ss); i++ {
		a, b := ss[i-1], ss[i]
		if pulled < 0 && a.n >= 0 && b.n >= 0 && a.tf == 0 && b.tf >= 1 && b.f == a.f+1 {
			pulled = i
		}
		if hdist(a.o, b.o) > 1.5 {
			if b.f == a.f+1 {
				first = i
			}
			break
		}
	}
	if pulled < 0 || first < 0 || first <= pulled {
		return r, false
	}
	a, b := ss[first-1], ss[first]
	r.trap = ss[pulled].trap
	r.from = hdist(a.o, r.trap)
	r.level = math.Abs(r.trap[2]-(a.o[2]-24)) < 24
	r.step = hdist(a.o, b.o)
	r.thinks = b.f - ss[pulled].f
	// The trap steps its frame on every think up to 4, so in the frame before
	// the move it reads the thinks behind the move.  Not in the frame of the
	// move: a Quad trap's first pull carries the thrower almost onto it, the
	// think that follows the move catches it, and a catch sets frame 5.
	r.counted = (a.tf < 4 && r.thinks == a.tf) || (a.tf == 4 && r.thinks >= 4)
	dx, dy := b.o[0]-a.o[0], b.o[1]-a.o[1]
	tx, ty := r.trap[0]-a.o[0], r.trap[1]-a.o[1]
	r.toward = (dx*tx + dy*ty) / (math.Hypot(dx, dy) * math.Hypot(tx, ty))
	return r, true
}

// quadpull: R-MP-29, sv_trap_quad_pull.  The receipt is the speed each of the
// trap's thinks gave the thrower: the Reckoning's 250, and four times that from
// a Quad trap under `sv_trap_quad_pull 4`.  The rounds are judged against the
// first one, measured on the same server.
func quadpull(srv *playtest.Server, t *playtest.Bot, tag string) {
	// Six seconds: a trap that came to rest out of reach is gone before the
	// next attempt stands the thrower up again.  sv_enforcetime: selfPull.
	srv.Console("set sv_trap_duration 6; set sv_enforcetime 0")
	rounds := []struct {
		pull string
		quad bool
		what string
	}{
		{"1", true, "quadpull: sv_trap_quad_pull 1 -- a Quad trap pulls as before"},
		{"4", true, "quadpull: sv_trap_quad_pull 4 -- a Quad trap pulls 4x as hard"},
		{"4", false, "quadpull: sv_trap_quad_pull 4 -- no Quad, no change"},
	}
	base := 0.0
	for _, c := range rounds {
		srv.Console("set sv_trap_quad_pull %s", c.pull)
		// Two clean pulls, and the slower of them is judged: now and then two
		// of the client's moves land in one frame and a step reads double,
		// and the slower of two is never that one.
		var runs []pullRun
		for attempt := 1; attempt <= 6 && len(runs) < 2; attempt++ {
			r, timed := selfPull(t, c.quad)
			switch {
			case !timed:
				note("attempt %d: no first pull to time", attempt)
			case !r.level || r.from < 48 || r.toward < 0.8 || r.step > r.from+32 || !r.counted:
				// A step that carries the thrower past the trap may have
				// ended against whatever lies beyond it.
				note("attempt %d: not a clean pull: %v, level %v, toward %.2f, counted %v",
					attempt, r, r.level, r.toward, r.counted)
			default:
				runs = append(runs, r)
			}
		}
		if len(runs) == 0 {
			skip(tag+c.what, "six attempts and no first pull it could time")
			continue
		}
		r := runs[0]
		if len(runs) > 1 && runs[1].perThink() < r.perThink() {
			r = runs[1]
		}
		v := r.perThink()
		switch {
		case c.pull == "1":
			// The Reckoning's 250 reads about 370 here, because the client's
			// two moves a frame both run; a quadrupled pull reads about 1,500.
			base = v
			if v >= 150 && v <= 750 {
				ok(tag+c.what, r.String())
			} else {
				bad(tag+c.what, r.String()+"; wanted 150 to 750")
			}
		case base == 0:
			skip(tag+c.what, "no pull under sv_trap_quad_pull 1 to compare with: "+r.String())
		case c.quad && v >= 2*base:
			ok(tag+c.what, fmt.Sprintf("%v; %.1fx the pull under 1", r, v/base))
		case !c.quad && v >= 0.6*base && v <= 1.6*base:
			ok(tag+c.what, fmt.Sprintf("%v; %.1fx the pull under 1", r, v/base))
		default:
			bad(tag+c.what, fmt.Sprintf("%v; %.1fx the pull under 1", r, v/base))
		}
	}
	srv.Console("set sv_trap_quad_pull 1; set sv_trap_duration 30; set sv_enforcetime 1")
}
