// ra2eyecam -- does Rocket Arena's "In Eyes" observer camera look through the
// target's eyes?
//
// RA2's eyecam_think() parks the observer 20 units in front of the target's
// face and 22 up, from the observer's own ClientThink.  The port of
// packetflinger's in-eyes camera copies the target's finished view at the end
// of the frame instead -- eye position, angles, gun -- and names the target in
// gclient_t::clientNum, which the server reads because the game advertises
// GMF_CLIENTNUM, so that the target's own model is hidden from the camera.
//
// Everything is read off the wire, from three clients' playerstates and the
// observer's entity list:
//
//	feature    g_features carries GMF_CLIENTNUM
//	eye        the observer renders from the target's eye
//	angles     the observer's view follows the target's as the target turns
//	gun        the observer's view weapon follows the target's weapon switch
//	hidden     the target's entity reaches the observer with modelindex 0,
//	           and the other fighter's does not
//	players    the fighters still see each other: clientNum is reset to the
//	           client's own number for everybody who is not looking through
//	           somebody else
//	retarget   switching target moves the eye and the hiding to the new one
//	leave      leaving the camera un-hides the last target
//
// Exit 0 all checks passed, 1 a check failed, 2 the scenario could not run.
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

	"q2playtest/playtest"
	"q2playtest/ra2"
)

const csPlayerSkinsOld = 1312 // CS_PLAYERSKINS without protocol extensions

var (
	reMode  = regexp.MustCompile(`Switched Observer Mode to: (.+)`)
	reTrack = regexp.MustCompile(`Tracking (.+)`)
	reFeat  = regexp.MustCompile(`"g_features" is "(\d+)"`)
)

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "", "read-only reference install (RA2 paks)")
	lib := flag.String("lib", "", "game library under test")
	dir := flag.String("dir", "/tmp/q2playtest/ra2eyecam", "scratch install dir")
	mapname := flag.String("map", "ra2map9", "map to test on")
	arena := flag.Int("arena", 8, "arena to make a pickup arena and fight in")
	port := flag.Int("port", 27975, "server port")
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
		fmt.Printf("  PASS  %-26s %s\n", name, detail)
		return
	}
	failed++
	fmt.Printf("  FAIL  %-26s %s\n", name, detail)
}

func report(name, detail string) { fmt.Printf("  ..    %-26s %s\n", name, detail) }

func dist(a, b [3]float64) float64 {
	return math.Sqrt((a[0]-b[0])*(a[0]-b[0]) + (a[1]-b[1])*(a[1]-b[1]) + (a[2]-b[2])*(a[2]-b[2]))
}

func angleDiff(a, b float64) float64 {
	d := math.Mod(a-b, 360)
	if d > 180 {
		d -= 360
	}
	if d < -180 {
		d += 360
	}
	return math.Abs(d)
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

// slotOf finds a player's client slot from the playerskins configstrings any
// client receives: CS_PLAYERSKINS + slot holds "name\model/skin".
func slotOf(watcher *playtest.Bot, name string) int {
	for n, v := range watcher.ConfigStrings() {
		if n >= csPlayerSkinsOld && n < csPlayerSkinsOld+256 &&
			strings.HasPrefix(v, name+`\`) {
			return n - csPlayerSkinsOld
		}
	}
	return -1
}

// modelOf describes what the watcher sees of a player's entity.
func modelOf(watcher *playtest.Bot, slot int) (int, bool, string) {
	m, ok := watcher.EntityModel(slot + 1)
	if !ok {
		return 0, false, fmt.Sprintf("entity %d not in the frame", slot+1)
	}
	return m, true, fmt.Sprintf("entity %d modelindex %d", slot+1, m)
}

// observerState reads the camera's mode and target out of its prints, in the
// order they arrived: a target only counts while the camera is on one.
func observerState(cam *playtest.Bot) (mode, target string) {
	for _, l := range cam.Prints() {
		d := playtest.Decode(l)
		if m := reMode.FindStringSubmatch(d); m != nil {
			mode = strings.TrimSpace(m[1])
			if mode != "Trackcam" && mode != "In Eyes" {
				target = ""
			}
		}
		if m := reTrack.FindStringSubmatch(d); m != nil {
			target = strings.TrimSpace(m[1])
		}
	}
	return mode, target
}

// worstEye samples the eye distance between camera and target a few times.
func worstEye(cam, target *playtest.Bot) float64 {
	worst := 0.0
	for i := 0; i < 5; i++ {
		cam.WaitFrames(3, 2*time.Second)
		if d := dist(cam.Eye(), target.Eye()); d > worst {
			worst = d
		}
	}
	return worst
}

func run(q2, ref, lib, dir, mapname string, arena, port int, label string) error {
	if label != "" {
		fmt.Printf("== %s ==\n", label)
	}
	os.RemoveAll(dir)
	if err := playtest.Install(dir, "arena", ref, lib); err != nil {
		return err
	}
	cfg := fmt.Sprintf("%s {\n\t%d {\n\t\tpickup: 1;\n\t\trounds: 9;\n\t}\n}\n", mapname, arena)
	if err := playtest.WriteFixture(filepath.Join(dir, "arena", "arena.cfg"), []byte(cfg), 0o644); err != nil {
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

	// ---- 0. the feature bit -------------------------------------------------
	mark, err := srv.Ask("g_features", `"g_features" is`, 5*time.Second)
	if err != nil {
		return fmt.Errorf("g_features: %w", err)
	}
	feat := -1
	for _, l := range srv.LogFrom(mark) {
		if m := reFeat.FindStringSubmatch(l); m != nil {
			feat, _ = strconv.Atoi(m[1])
		}
	}
	check("GMF_CLIENTNUM advertised", feat >= 0 && feat&1 != 0,
		fmt.Sprintf("g_features = %d", feat))

	// ---- seat two fighters and wait for the round ---------------------------
	red := playtest.NewBot("red1", "127.0.0.1", port)
	blue := playtest.NewBot("blue1", "127.0.0.1", port)
	for _, s := range []struct {
		b    *playtest.Bot
		side string
	}{{red, "Red"}, {blue, "Blue"}} {
		if err := s.b.Start(30 * time.Second); err != nil {
			return err
		}
		defer s.b.Disconnect()
		if err := ra2.JoinTeam(s.b, ra2.PickupTeam(arena, s.side)); err != nil {
			return fmt.Errorf("%s: %w", s.b.Name, err)
		}
	}
	if err := waitFor(90*time.Second, func() bool { return !red.Spectating() && !blue.Spectating() }); err != nil {
		return fmt.Errorf("the fighters were never placed: %w", err)
	}
	if _, err := red.WaitCenter(`FIGHT`, 40*time.Second); err != nil {
		return fmt.Errorf("the round never started: %w", err)
	}
	red.WaitFrames(5, 5*time.Second)
	fmt.Println("  ..    red1 and blue1 are fighting")

	// ---- seat the camera behind the live round ------------------------------
	cam := playtest.NewBot("watcher", "127.0.0.1", port)
	if err := cam.Start(30 * time.Second); err != nil {
		return err
	}
	defer cam.Disconnect()
	if err := ra2.JoinTeam(cam, ra2.PickupTeam(arena, "Red")); err != nil {
		return fmt.Errorf("watcher: %w", err)
	}
	// An arena with no observer spawns is "active" and seats an observer free
	// flying (PM_SPECTATOR); one with them seats it walking, on PM_NORMAL.
	// Either way the team announcement is the seating, and the mode cycle
	// below reaches In Eyes from both.
	if _, err := cam.WaitPrint(`watcher has been added to team`, 20*time.Second); err != nil {
		return fmt.Errorf("the watcher never joined: %w", err)
	}
	cam.WaitFrames(20, 5*time.Second)

	ownGun := cam.GunIndex() // the observer's own view weapon, before In Eyes
	mode, tname := observerState(cam)
	for i := 0; i < 8 && !(mode == "In Eyes" && tname != ""); i++ {
		cam.Press(playtest.ButtonAttack, 250*time.Millisecond)
		time.Sleep(800 * time.Millisecond)
		mode, tname = observerState(cam)
	}
	if mode != "In Eyes" || tname == "" {
		return fmt.Errorf("could not reach In Eyes with a target (mode %q, target %q)", mode, tname)
	}
	cam.WaitFrames(10, 5*time.Second)

	slots := map[string]int{}
	for _, b := range []*playtest.Bot{red, blue} {
		slots[b.Name] = slotOf(cam, b.Name)
		if slots[b.Name] < 0 {
			return fmt.Errorf("no playerskins configstring names %s", b.Name)
		}
	}
	target, other := red, blue
	if tname == blue.Name {
		target, other = blue, red
	}
	report("observer", fmt.Sprintf("In Eyes on %s (slot %d), pm_type %d; other fighter %s (slot %d)",
		target.Name, slots[target.Name], cam.PMType(), other.Name, slots[other.Name]))

	// ---- 1. the eye ---------------------------------------------------------
	e := worstEye(cam, target)
	ce, te := cam.Eye(), target.Eye()
	check("camera is in the eyes", e < 1.0,
		fmt.Sprintf("worst %.1f units; camera eye %.1f %.1f %.1f, %s's %.1f %.1f %.1f",
			e, ce[0], ce[1], ce[2], target.Name, te[0], te[1], te[2]))

	// ---- 2. the view follows the target turning -----------------------------
	worstAng, lastYaw, turned := 0.0, math.NaN(), 0
	for _, yaw := range []float64{0, 90, 180, 270, 45} {
		target.Look(-10, yaw, 0)
		target.WaitFrames(8, 4*time.Second)
		cam.WaitFrames(3, 2*time.Second)
		tv, cv := target.ViewAngles(), cam.ViewAngles()
		if !math.IsNaN(lastYaw) && angleDiff(tv[1], lastYaw) > 10 {
			turned++
		}
		lastYaw = tv[1]
		for i := 0; i < 2; i++ {
			if d := angleDiff(tv[i], cv[i]); d > worstAng {
				worstAng = d
			}
		}
	}
	check("view follows the target", turned >= 3 && worstAng < 2.0,
		fmt.Sprintf("target turned %d times, worst pitch/yaw difference %.2f deg", turned, worstAng))

	// ---- 3. the gun follows the target's weapon switch ----------------------
	type gunPair struct{ t, c int }
	var guns []gunPair
	for _, w := range []string{"Railgun", "Rocket Launcher"} {
		target.Cmd("use %s", w)
		target.WaitFrames(15, 5*time.Second)
		cam.WaitFrames(3, 2*time.Second)
		guns = append(guns, gunPair{target.GunIndex(), cam.GunIndex()})
	}
	switched := guns[0].t != guns[1].t && guns[0].t != 0 && guns[1].t != 0
	follows := guns[0].c == guns[0].t && guns[1].c == guns[1].t
	check("gun follows the target", switched && follows,
		fmt.Sprintf("railgun: target %d camera %d; rocket launcher: target %d camera %d",
			guns[0].t, guns[0].c, guns[1].t, guns[1].c))

	// ---- 4. the target's own model is hidden from the camera ----------------
	tm, tok, tdesc := modelOf(cam, slots[target.Name])
	check("target hidden from camera", tok && tm == 0, tdesc)
	om, ook, odesc := modelOf(cam, slots[other.Name])
	if ook {
		check("other fighter still drawn", om != 0, odesc)
	} else {
		report("other fighter", odesc+" (out of view, not judged)")
	}

	// ---- 5. the fighters still see each other -------------------------------
	for _, pair := range [][2]*playtest.Bot{{red, blue}, {blue, red}} {
		m, ok, desc := modelOf(pair[0], slots[pair[1].Name])
		if ok {
			check(pair[0].Name+" sees "+pair[1].Name, m != 0, desc)
		} else {
			report(pair[0].Name+" sees "+pair[1].Name, desc+" (out of view, not judged)")
		}
	}

	// ---- 6. switching target moves the eye and the hiding -------------------
	before := len(cam.Prints())
	for i := 0; i < 4; i++ {
		cam.Nudge(200, 300*time.Millisecond)
		time.Sleep(700 * time.Millisecond)
		if _, n := observerState(cam); n == other.Name && len(cam.Prints()) > before {
			break
		}
	}
	if _, n := observerState(cam); n != other.Name {
		return fmt.Errorf("could not switch the camera to %s (tracking %q)", other.Name, n)
	}
	cam.WaitFrames(10, 5*time.Second)
	e2 := worstEye(cam, other)
	nm, nok, ndesc := modelOf(cam, slots[other.Name])
	pm, pok, pdesc := modelOf(cam, slots[target.Name])
	check("retarget: eyes", e2 < 1.0, fmt.Sprintf("worst %.1f units from %s's eye", e2, other.Name))
	check("retarget: new target hidden", nok && nm == 0, ndesc)
	if pok {
		check("retarget: old target drawn", pm != 0, pdesc)
	} else {
		report("retarget: old target", pdesc+" (out of view, not judged)")
	}

	// ---- 7. leaving the camera un-hides the target --------------------------
	//
	// In Eyes -> Normal (which move_to_arena turns into Free Flying, parked on
	// a spawn point) -> Trackcam.  The trackcam sits 150 units behind its
	// target looking at it, so the target is certainly in view there, and a
	// clientNum left pointing at it would hide it.
	var lname string
	for i := 0; i < 6; i++ {
		cam.Press(playtest.ButtonAttack, 250*time.Millisecond)
		time.Sleep(800 * time.Millisecond)
		if mode, lname = observerState(cam); mode == "Trackcam" && lname != "" {
			break
		}
	}
	if mode != "Trackcam" || lname == "" {
		return fmt.Errorf("could not reach the trackcam after In Eyes (mode %q, target %q)", mode, lname)
	}
	cam.WaitFrames(15, 5*time.Second)
	lm, lok, ldesc := modelOf(cam, slots[lname])
	check("leave: target drawn again", lok && lm != 0,
		fmt.Sprintf("Trackcam on %s: %s", lname, ldesc))

	// The view weapon is the observer's own again, and it does not think:
	// eyecam_SetView hands it back as WEAPON_ACTIVATING, and Weapon_Generic's
	// FIGHT_ALIVE guard is what must keep that from animating.
	still := true
	for i := 0; i < 10; i++ {
		if cam.GunFrame() != 0 {
			still = false
		}
		cam.WaitFrames(1, time.Second)
	}
	check("leave: own gun back, still", cam.GunIndex() == ownGun && still,
		fmt.Sprintf("gunindex %d (own %d), gunframe stayed 0: %v", cam.GunIndex(), ownGun, still))

	if err := srv.Alive(5 * time.Second); err != nil {
		check("server survived", false, err.Error())
	}
	return nil
}
