// ra2nullattacker -- does a damage event that carries NO attacker still take the
// server down?
//
// An edict_t *attacker of NULL is not a caller error in id's code: nothing ever
// assigns activator on a func_door, a func_clock that is not START_OFF never has
// one either, and both hand what they hold to G_UseTargets, which passes it on
// to every target it fires.  A target_explosion among those targets then uses it
// as the attacker of its radius damage.  baseq2 survives that by coincidence --
// its single read of attacker sits behind a DAMAGE_RADIUS test that
// T_RadiusDamage always passes -- and every read a mod adds in front of that is
// a new chance for the coincidence to stop holding.
//
// The producer here is the func_clock rather than the blocked door, because it
// fires on its own: a headless client cannot walk into a doorway, and the class
// of defect is the NULL, not which entity produced it.  The scenario writes the
// map's own entity string back out with a clock, a target_string for it to
// drive, and target_explosions wired to its pathtarget, and loads that as a
// map_override_path entity file -- so no map is edited and no BSP is built.
//
// Two phases, because the NULL reaches two different unguarded reads:
//
//	message  a target_explosion carrying a "message" reaches G_UseTargets's
//	         activator->svflags test.  No clients needed at all: an empty
//	         server dies by itself a few seconds into the map.
//	damage   an explosion on each of an arena's spawn pads, with two fighters
//	         standing on them, reaches T_Damage.  It is gated on the round
//	         actually being live, because a fighter is takedamage DAMAGE_NO
//	         until then and T_Damage returns before the read.
//
// The damage phase asserts more than survival: a fighter must actually LOSE
// HEALTH, because an explosion that reached nobody proves nothing about the
// read it never performed.
//
// Exit 0 both phases survived and the damage landed, 1 one did not, 2 the
// scenario could not run.
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	"q2playtest/playtest"
	"q2playtest/ra2"
)

var (
	binary  = flag.String("q2proded", "", "path to the q2proded binary")
	ref     = flag.String("ref", "", "read-only reference install holding the mod's paks and cfg")
	lib     = flag.String("lib", "", "game library to test")
	dir     = flag.String("dir", "/tmp/q2playtest/ra2nullattacker", "scratch install directory to build")
	gameDir = flag.String("game", "arena", "mod directory name")
	mapName = flag.String("map", "ra2map26", "map to run")
	arena   = flag.Int("arena", 1, "arena to fight in; must be a pickup arena")
	dmg     = flag.Int("dmg", 25, "target_explosion dmg; radius is dmg+40")
	period  = flag.Int("period", 3, "func_clock count, i.e. seconds between firings")
	watch   = flag.Duration("watch", 30*time.Second, "how long to watch each phase")
	port    = flag.Int("port", 27995, "server UDP port")
	ruleset = flag.String("ruleset", "arena", "g_ruleset for a library that serves several")
	label   = flag.String("label", "", "label for the report")
	phases  = flag.String("phases", "message,damage", "comma-separated phases to run")
)

var failed int

func check(name string, ok bool, detail string) {
	if ok {
		fmt.Printf("  PASS  %-28s %s\n", name, detail)
		return
	}
	failed++
	fmt.Printf("  FAIL  %-28s %s\n", name, detail)
}

func main() {
	flag.Parse()
	if *binary == "" || *ref == "" || *lib == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded, -ref and -lib")
		os.Exit(2)
	}
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	if failed > 0 {
		os.Exit(1)
	}
}

func run() error {
	if *label != "" {
		fmt.Printf("== %s ==\n", *label)
	}
	if err := playtest.Install(*dir, *gameDir, *ref, *lib); err != nil {
		return err
	}

	pads, err := playtest.MapEntities(filepath.Join(*ref, "pak*.pak"), *mapName, "info_player_deathmatch")
	if err != nil {
		return err
	}
	var origins [][3]float64
	for _, p := range pads {
		if p.Arena == *arena {
			origins = append(origins, p.Origin)
		}
	}
	if len(origins) == 0 {
		return fmt.Errorf("%s has no info_player_deathmatch in arena %d", *mapName, *arena)
	}

	if err := writeArenaCfg(); err != nil {
		return err
	}

	base, err := dumpEntityString()
	if err != nil {
		return err
	}
	fmt.Printf("  ..    %s entity string is %d bytes, arena %d has %d pads\n",
		*mapName, len(base), *arena, len(origins))

	for _, phase := range strings.Split(*phases, ",") {
		switch strings.TrimSpace(phase) {
		case "message":
			if err := messagePhase(base, origins[0]); err != nil {
				return err
			}
		case "damage":
			if err := damagePhase(base, origins); err != nil {
				return err
			}
		case "":
		default:
			return fmt.Errorf("unknown phase %q", phase)
		}
	}
	return nil
}

// writeArenaCfg makes the arena under test a PICKUP arena, which is the only
// kind whose teams exist before anybody has joined -- RA2 creates "#n Pickup
// Red"/"Blue" at map load for those and for no others, and a headless client
// has no console command to make a team of its own in a named arena.  `pickup`
// comes only from arena.cfg: a map with an `arena` worldspawn key defaults it
// to 0, so without this file the team menu has nothing to click.
func writeArenaCfg() error {
	cfg := fmt.Sprintf(`// written by scenarios/ra2nullattacker
rounds: 9;
health: 200;

%s {
	%d {
		pickup: 1;
	}
}
`, *mapName, *arena)
	path := filepath.Join(*dir, *gameDir, "arena.cfg")
	os.Remove(path) // Install may have symlinked the reference one; never write through it
	return os.WriteFile(path, []byte(cfg), 0o644)
}

// dumpEntityString boots the server once and asks the engine for the map's own
// entity string, which is the base every override is built from.  Reading it
// from the engine rather than from the pak keeps the override honest: it is the
// same text the server would have used.
func dumpEntityString() (string, error) {
	srv := newServer("")
	if err := srv.Start(); err != nil {
		return "", err
	}
	defer srv.Stop()

	if err := srv.Console("dumpents nullattacker"); err != nil {
		return "", err
	}
	if _, err := srv.WaitLog(`Dumped entity string`, 10*time.Second); err != nil {
		return "", err
	}
	b, err := os.ReadFile(filepath.Join(*dir, *gameDir, "entdumps", "nullattacker.ent"))
	if err != nil {
		return "", err
	}
	return strings.TrimRight(string(b), "\x00"), nil
}

// clock is the producer: TIMER_DOWN|MULTI_USE, so it counts `period` seconds
// down, fires its pathtarget through G_UseTargets with the activator it never
// had, resets and does it again.
func clock(at [3]float64) string {
	return fmt.Sprintf(`
{
"classname" "func_clock"
"spawnflags" "10"
"count" "%d"
"style" "0"
"target" "nullstr"
"pathtarget" "nullboom"
"origin" "%.0f %.0f %.0f"
}
{
"classname" "target_string"
"targetname" "nullstr"
"origin" "%.0f %.0f %.0f"
}
`, *period, at[0], at[1], at[2], at[0], at[1], at[2])
}

func explosion(at [3]float64, damage int, message string) string {
	s := fmt.Sprintf(`{
"classname" "target_explosion"
"targetname" "nullboom"
"dmg" "%d"
"origin" "%.0f %.0f %.0f"
`, damage, at[0], at[1], at[2])
	if message != "" {
		s += fmt.Sprintf("\"message\" \"%s\"\n", message)
	}
	return s + "}\n"
}

func writeOverride(ents string) (string, error) {
	sub := "nullents"
	d := filepath.Join(*dir, *gameDir, sub)
	if err := os.MkdirAll(d, 0o755); err != nil {
		return "", err
	}
	return sub, os.WriteFile(filepath.Join(d, *mapName+".ent"), []byte(ents), 0o644)
}

func newServer(entDir string) *playtest.Server {
	cvars := map[string]string{
		"g_ruleset":      *ruleset,
		"arenacfg":       "arena.cfg",
		"bots":           "0",
		"minimumplayers": "0",
		"admincode":      "0",
	}
	if entDir != "" {
		cvars["map_override_path"] = entDir
	}
	return &playtest.Server{
		Binary: *binary, Dir: *dir, Game: *gameDir, Map: *mapName,
		Port: *port, MaxClients: 16,
		LogPath: filepath.Join(*dir, "server.log"),
		Cvars:   cvars,
	}
}

// alive asks the server a question only a running server can answer.  A game
// library that dereferenced NULL takes the whole process with it, so this is
// the check the whole scenario turns on.
func alive(srv *playtest.Server) bool {
	if err := srv.Console("status"); err != nil {
		return false
	}
	_, err := srv.WaitLog(`Current map:`, 5*time.Second)
	return err == nil
}

// messagePhase needs no clients: G_UseTargets reads activator->svflags before
// anything else it does with the message, and the explosion fires itself.
func messagePhase(base string, at [3]float64) error {
	entDir, err := writeOverride(base + clock(at) + explosion(at, 0, "null-attacker probe"))
	if err != nil {
		return err
	}
	srv := newServer(entDir)
	if err := srv.Start(); err != nil {
		check("message: server survived", false, fmt.Sprintf("did not even boot: %v", err))
		return nil
	}
	defer srv.Stop()
	if _, err := srv.WaitLog(`Loaded entity string`, 5*time.Second); err != nil {
		return fmt.Errorf("the entity override was not loaded: %w", err)
	}

	deadline := time.Now().Add(*watch)
	for time.Now().Before(deadline) {
		if !alive(srv) {
			check("message: server survived", false,
				fmt.Sprintf("dead after %.0fs, no clients connected -- G_UseTargets",
					(*watch-time.Until(deadline)).Seconds()))
			return nil
		}
		time.Sleep(2 * time.Second)
	}
	check("message: server survived", true,
		fmt.Sprintf("%.0fs of firings every %ds, empty server", watch.Seconds(), *period))
	return nil
}

// damagePhase puts an explosion on every pad of the arena, so wherever the round
// places the two fighters one of them is standing in one.
func damagePhase(base string, origins [][3]float64) error {
	ents := base + clock(origins[0])
	for _, o := range origins {
		ents += explosion(o, *dmg, "")
	}
	entDir, err := writeOverride(ents)
	if err != nil {
		return err
	}

	srv := newServer(entDir)
	if err := srv.Start(); err != nil {
		check("damage: server survived", false, fmt.Sprintf("did not even boot: %v", err))
		return nil
	}
	defer srv.Stop()

	var bots []*playtest.Bot
	for i, side := range []string{"Red", "Blue"} {
		b := playtest.NewBot(fmt.Sprintf("fighter%d", i+1), "127.0.0.1", *port)
		if err := b.Start(30 * time.Second); err != nil {
			check("damage: server survived", false,
				fmt.Sprintf("%s could not connect (%v); server alive: %v", b.Name, err, alive(srv)))
			return nil
		}
		bots = append(bots, b)
		if err := ra2.JoinTeam(b, ra2.PickupTeam(*arena, side)); err != nil {
			check("damage: server survived", false,
				fmt.Sprintf("%s could not join (%v); server alive: %v", b.Name, err, alive(srv)))
			return nil
		}
	}
	defer func() {
		for _, b := range bots {
			b.Disconnect()
		}
	}()

	// The round is live when the fighters stop being observers; only then is a
	// fighter takedamage DAMAGE_AIM and only then can the explosion reach the
	// read under test.
	live := false
	for deadline := time.Now().Add(60 * time.Second); time.Now().Before(deadline); {
		n := 0
		for _, b := range bots {
			if !b.Spectating() {
				n++
			}
		}
		if n == len(bots) {
			live = true
			break
		}
		if !alive(srv) {
			check("damage: server survived", false, "dead before the round went live")
			return nil
		}
		time.Sleep(time.Second)
	}
	if !live {
		return fmt.Errorf("the round never went live; see %s", filepath.Join(*dir, "server.log"))
	}
	fmt.Printf("  ..    round live, fighters at %v and %v\n", fmtPos(bots[0].Origin()), fmtPos(bots[1].Origin()))

	const statHealth = 1
	start := map[string]int{}
	for _, b := range bots {
		start[b.Name] = b.Stat(statHealth)
	}

	hurt := ""
	for deadline := time.Now().Add(*watch); time.Now().Before(deadline); {
		if !alive(srv) {
			check("damage: server survived", false,
				"dead during a live round -- T_Damage with no attacker")
			return nil
		}
		for _, b := range bots {
			if h := b.Stat(statHealth); h < start[b.Name] {
				hurt = fmt.Sprintf("%s %d -> %d health", b.Name, start[b.Name], h)
			}
		}
		if hurt != "" && time.Until(deadline) > 10*time.Second {
			// keep watching: the first hit proves the read happened, the rest
			// proves nothing downstream of it crashes either
			deadline = time.Now().Add(10 * time.Second)
		}
		time.Sleep(time.Second)
	}

	check("damage: server survived", true,
		fmt.Sprintf("%.0fs of a live round, explosions every %ds", watch.Seconds(), *period))
	check("damage: the damage landed", hurt != "",
		orElse(hurt, "no fighter ever lost health -- the explosion reached nobody, so nothing was proved"))
	return nil
}

func fmtPos(p [3]float64) string {
	return fmt.Sprintf("(%.0f %.0f %.0f)", p[0], p[1], p[2])
}

func orElse(s, alt string) string {
	if s == "" {
		return alt
	}
	return s
}
