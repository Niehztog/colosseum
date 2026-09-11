// ra2waitroom -- where does a player killed MID-ROUND land, and can it move?
//
// This is the other half of ra2twins.  That scenario seats arrivals in an arena
// where nobody is fighting yet, which is the case RA2's two-pass spot measure
// was written for: no fighters to be farthest from, so the measure falls back to
// every live body and the arrivals repel each other.
//
// A round in progress is the opposite case, and it is the common one.  A killed
// fighter is respawned into its arena's WAITING ROOM -- move_to_arena(..., 1),
// which picks a misc_teleporter_dest -- while its opponents are still alive.  So
// the fighter pass succeeds, and it measures the waiting-room pads against the
// FIGHTERS, who are nowhere near them.  Every dead player is therefore handed
// the same pad: the one farthest from the fight.  Nothing else separates them --
// KillBox returns early for a FIGHT_SPECTATING client and check_telefrag skips
// one -- and an observer in NORMAL mode is SOLID_BBOX on MOVETYPE_WALK, so each
// one's pmove is allsolid inside the other and neither can move.
//
// Needs an arena that HAS a waiting room (a `misc_teleporter_dest` of its own)
// and more than one fighter per side, so that a death does not end the round.
// ra2map17 arena 4 ("Mad Mines") is both: 8 pads, 8 spawns, `pickup: 1`.
//
// Colosseum keeps `kill` under arena where RA2 makes it a no-op, which is what
// makes this measurable without aim or navigation.
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

var (
	binary  = flag.String("q2proded", "", "path to the q2proded binary")
	ref     = flag.String("ref", "", "read-only reference install holding the mod's paks and cfg")
	lib     = flag.String("lib", "", "game library to test")
	dir     = flag.String("dir", "/tmp/q2playtest/ra2waitroom", "scratch install directory to build")
	gameDir = flag.String("game", "arena", "mod directory name")
	mapName = flag.String("map", "ra2map17", "map to run")
	arena   = flag.Int("arena", 4, "pickup arena to fight in; must own a misc_teleporter_dest")
	perSide = flag.Int("perside", 3, "clients per pickup side")
	kills   = flag.Int("kills", 2, "clients per side to send to the waiting room")
	port    = flag.Int("port", 27984, "server UDP port")
	ruleset = flag.String("ruleset", "arena", "g_ruleset for a library that serves several")
	label   = flag.String("label", "", "label for the report")
)

// A player's bounding box is 32x32 wide and 56 tall.  Two boxes whose centres
// are closer than that in both axes are interpenetrating, not merely adjacent.
const (
	boxWide = 32.0
	boxTall = 56.0
	walkFor = 1500 * time.Millisecond
	moved   = 32.0 // a free client covers ~600 units in walkFor; 32 is generous
)

var failed int

func check(name string, ok bool, detail string) {
	if ok {
		fmt.Printf("  PASS  %-38s %s\n", name, detail)
		return
	}
	failed++
	fmt.Printf("  FAIL  %-38s %s\n", name, detail)
}

func note(format string, a ...any) { fmt.Printf("  ..    "+format+"\n", a...) }

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

// dumpRow is one `sv arenadump` client line.
type dumpRow struct {
	name    string
	fight   int
	solid   int
	dmg     int
	dead    int
	recheck int
	origin  [3]float64
}

var rowRe = regexp.MustCompile(
	`^client \d+\s+(\S+)\s+(?:bot|human)\s+arena=(\d+) team=(-?\d+) fight=(\d+) ` +
		`solid=(\d+) dmg=(\d+) dead=(\d+) hp=(-?\d+) svf=\S+ recheck=(-?\d+) ` +
		`at \((-?[\d.]+) (-?[\d.]+) (-?[\d.]+)\)`)

func arenadump(srv *playtest.Server) (map[string]dumpRow, []string, error) {
	mark := srv.Len()
	if err := srv.Console("sv arenadump"); err != nil {
		return nil, nil, err
	}
	// The `!!` complaints come after the last client row, so wait for a client
	// row to land and then let the rest of the block arrive.
	if _, err := srv.WaitLog(`^client \d+`, 10*time.Second); err != nil {
		return nil, nil, err
	}
	time.Sleep(600 * time.Millisecond)

	rows := map[string]dumpRow{}
	var bangs []string
	for _, l := range srv.GrepFrom(mark, `.`) {
		l = strings.TrimSpace(l)
		if strings.HasPrefix(l, "!!") {
			bangs = append(bangs, l)
			continue
		}
		m := rowRe.FindStringSubmatch(l)
		if m == nil {
			continue
		}
		atoi := func(s string) int { n, _ := strconv.Atoi(s); return n }
		atof := func(s string) float64 { f, _ := strconv.ParseFloat(s, 64); return f }
		rows[m[1]] = dumpRow{
			name: m[1], fight: atoi(m[4]), solid: atoi(m[5]), dmg: atoi(m[6]),
			dead: atoi(m[7]), recheck: atoi(m[9]),
			origin: [3]float64{atof(m[10]), atof(m[11]), atof(m[12])},
		}
	}
	return rows, bangs, nil
}

func run() error {
	if *label != "" {
		fmt.Printf("== %s ==\n", *label)
	}
	pads, err := playtest.MapEntities(
		filepath.Join(*ref, "pak*.pak"), *mapName, "misc_teleporter_dest")
	if err != nil {
		return err
	}
	n := 0
	for _, p := range pads {
		if p.Arena == *arena {
			n++
		}
	}
	if n == 0 {
		return fmt.Errorf("arena %d of %s has no misc_teleporter_dest: it has no waiting room, so observers in it are free-flying and cannot collide", *arena, *mapName)
	}
	note("arena %d owns %d waiting-room pad(s) in %s", *arena, n, *mapName)

	if err := playtest.Install(*dir, *gameDir, *ref, *lib); err != nil {
		return err
	}
	srv := &playtest.Server{
		Binary: *binary, Dir: *dir, Game: *gameDir, Map: *mapName,
		Port: *port, MaxClients: 16,
		LogPath: filepath.Join(*dir, "server.log"),
		Cvars: map[string]string{
			"g_ruleset":      *ruleset,
			"arenacfg":       "arena.cfg",
			"bots":           "0",
			"botfill":        "0",
			"minimumplayers": "0",
			"admincode":      "0",
		},
	}
	if err := srv.Start(); err != nil {
		return err
	}
	defer srv.Stop()

	// ---- seat both pickup sides ---------------------------------------------
	type client struct {
		bot  *playtest.Bot
		side string
	}
	var all []client
	for i := 0; i < *perSide*2; i++ {
		side := "Red"
		if i%2 == 1 {
			side = "Blue"
		}
		b := playtest.NewBot(fmt.Sprintf("%s%d", strings.ToLower(side), i/2+1),
			"127.0.0.1", *port)
		if err := b.Start(30 * time.Second); err != nil {
			return err
		}
		defer b.Disconnect()
		if err := ra2.JoinTeam(b, ra2.PickupTeam(*arena, side)); err != nil {
			return fmt.Errorf("%s: %w", b.Name, err)
		}
		b.WaitFrames(10, 5*time.Second)
		all = append(all, client{b, side})
	}

	// The round needs both sides seated and then a countdown.  FIGHT! is a
	// centerprint, which is the honest signal that placement as a fighter has
	// happened -- the HUD is not.
	if _, err := all[0].bot.WaitCenter(`(?i)fight`, 40*time.Second); err != nil {
		note("no FIGHT! centerprint arrived (%v) -- continuing, the dump says the state", err)
	}
	time.Sleep(1500 * time.Millisecond)

	rows, _, err := arenadump(srv)
	if err != nil {
		return err
	}
	fighting := 0
	for _, c := range all {
		r := rows[c.bot.Name]
		if r.fight == 1 {
			fighting++
		}
	}
	check("both sides are fighting", fighting == len(all),
		fmt.Sprintf("%d of %d clients have fight=1", fighting, len(all)))

	// ---- send some of them to the waiting room ------------------------------
	// Cmd_Kill_f refuses within five seconds of a respawn, and leaving one
	// fighter per side alive is what keeps fight_done() from ending the round.
	time.Sleep(6 * time.Second)

	var dead []client
	for _, side := range []string{"Red", "Blue"} {
		k := 0
		for _, c := range all {
			if c.side != side || k >= *kills {
				continue
			}
			c.bot.Cmd("kill")
			dead = append(dead, c)
			k++
			time.Sleep(1200 * time.Millisecond) // let each placement finish
		}
	}
	time.Sleep(2 * time.Second)

	rows, bangs, err := arenadump(srv)
	if err != nil {
		return err
	}
	for _, c := range all {
		r := rows[c.bot.Name]
		spot, off := playtest.Nearest(pads, *arena, r.origin)
		where := fmt.Sprintf("%s +%.0f", spot.Name, off)
		if r.fight == 1 {
			where = "(fighting)"
		}
		note("%-7s fight=%d solid=%d dmg=%d dead=%d recheck=%-6d at (%6.0f %6.0f %6.0f) %s",
			r.name, r.fight, r.solid, r.dmg, r.dead, r.recheck,
			r.origin[0], r.origin[1], r.origin[2], where)
	}
	for _, b := range bangs {
		note("arenadump: %s", b)
	}

	// ---- 0. the control, and it is the same function ------------------------
	// Three clients that never join anything stay in arena 0, the lobby.  The
	// lobby is measured by the same ArenaFightersRangeFromSpot, but nobody is
	// ever FIGHT_ALIVE there, so its fighter pass counts nobody and the measure
	// falls through to every live body -- which is the fix, and it repels.
	// Same map, same server, same function: it spreads arrivals here and stacks
	// them in the waiting room above.  That is what makes the failure specific
	// to "a round is in progress" rather than to observer placement in general.
	var lobby []*playtest.Bot
	for i := 0; i < 3; i++ {
		b := playtest.NewBot(fmt.Sprintf("lobby%d", i+1), "127.0.0.1", *port)
		if err := b.Start(30 * time.Second); err != nil {
			return err
		}
		defer b.Disconnect()
		if err := ra2.DismissMOTD(b); err != nil {
			return fmt.Errorf("%s: %w", b.Name, err)
		}
		b.WaitFrames(10, 5*time.Second)
		lobby = append(lobby, b)
		time.Sleep(700 * time.Millisecond)
	}
	time.Sleep(time.Second)

	lrows, _, err := arenadump(srv)
	if err != nil {
		return err
	}
	var lpairs []string
	for i := 0; i < len(lobby); i++ {
		spot, off := playtest.Nearest(pads, 0, lrows[lobby[i].Name].origin)
		o := lrows[lobby[i].Name].origin
		note("%-7s fight=%d solid=%d recheck=%-6d at (%6.0f %6.0f %6.0f) %s +%.0f",
			lobby[i].Name, lrows[lobby[i].Name].fight, lrows[lobby[i].Name].solid,
			lrows[lobby[i].Name].recheck, o[0], o[1], o[2], spot.Name, off)
		for j := i + 1; j < len(lobby); j++ {
			a, b := lrows[lobby[i].Name].origin, lrows[lobby[j].Name].origin
			if math.Abs(a[0]-b[0]) < boxWide && math.Abs(a[1]-b[1]) < boxWide &&
				math.Abs(a[2]-b[2]) < boxTall {
				lpairs = append(lpairs, fmt.Sprintf("%s/%s", lobby[i].Name, lobby[j].Name))
			}
		}
	}
	check("lobby arrivals repel (control)", len(lpairs) == 0,
		fmt.Sprintf("%d overlapping pair(s) among %d lobby clients %v",
			len(lpairs), len(lobby), lpairs))

	// ---- 1. did the killed clients reach the waiting room at all ------------
	waiting := 0
	for _, c := range dead {
		if rows[c.bot.Name].fight == 0 {
			waiting++
		}
	}
	check("killed clients are observing", waiting == len(dead),
		fmt.Sprintf("%d of %d killed clients have fight=0", waiting, len(dead)))

	// ---- 2. do they share a pad --------------------------------------------
	var pairs []string
	for i := 0; i < len(dead); i++ {
		for j := i + 1; j < len(dead); j++ {
			a, b := rows[dead[i].bot.Name].origin, rows[dead[j].bot.Name].origin
			if math.Abs(a[0]-b[0]) < boxWide && math.Abs(a[1]-b[1]) < boxWide &&
				math.Abs(a[2]-b[2]) < boxTall {
				pairs = append(pairs, fmt.Sprintf("%s/%s %.0f apart",
					dead[i].bot.Name, dead[j].bot.Name, playtest.Dist(a, b)))
			}
		}
	}
	detail := "every observer got its own pad"
	if len(pairs) > 0 {
		detail = fmt.Sprintf("%d overlapping pair(s): %v", len(pairs), pairs)
	}
	check("no two observers share a pad", len(pairs) == 0, detail)

	// ---- 3. the control: the fighters still in the arena are not stacked ----
	// The fighter side of this was fixed earlier.  Asserting it here is what makes
	// check 2's failure specific to the observer path rather than to spawning
	// in general.
	var fpairs []string
	var live []client
	for _, c := range all {
		if rows[c.bot.Name].fight == 1 {
			live = append(live, c)
		}
	}
	for i := 0; i < len(live); i++ {
		for j := i + 1; j < len(live); j++ {
			a, b := rows[live[i].bot.Name].origin, rows[live[j].bot.Name].origin
			if math.Abs(a[0]-b[0]) < boxWide && math.Abs(a[1]-b[1]) < boxWide &&
				math.Abs(a[2]-b[2]) < boxTall {
				fpairs = append(fpairs, fmt.Sprintf("%s/%s", live[i].bot.Name, live[j].bot.Name))
			}
		}
	}
	check("fighters are not stacked (control)", len(fpairs) == 0,
		fmt.Sprintf("%d fighter(s) left, %d overlapping pair(s) %v", len(live), len(fpairs), fpairs))

	// ---- 4. can each observer walk out -------------------------------------
	// Three directions, because a client placed facing a wall is not trapped.
	for _, c := range dead {
		best := 0.0
		for _, yaw := range []float64{0, 180, 90} {
			from := c.bot.Origin()
			c.bot.Look(0, yaw, 0)
			c.bot.Walk(400, 0, walkFor)
			if d := playtest.Dist(from, c.bot.Origin()); d > best {
				best = d
			}
			if best > moved {
				break
			}
		}
		check(c.bot.Name+" can move", best > moved,
			fmt.Sprintf("best displacement over three directions = %.0f units", best))
	}
	return nil
}
