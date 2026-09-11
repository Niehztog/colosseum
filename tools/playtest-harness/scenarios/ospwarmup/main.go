// ospwarmup -- what a tourney OBSERVER may do, and what a match START leaves
// behind.  Four subjects, all reported from a playtest of `tdm`:
//
//   1. an observer can only look LEFT AND RIGHT.  PM_ClampAngles answers
//      PMF_TIME_TELEPORT by pinning PITCH and ROLL to zero and letting only YAW
//      follow the mouse -- and the countdown that clears the flag sits BELOW
//      Pmove()'s `if (pm_type == PM_SPECTATOR) { PM_FlyMove(); return; }`, so
//      the flag stamped on an observer is stamped there for good.  Two witnesses,
//      because either alone can be argued with: the flag word off the wire, and
//      the view pitch the client is actually rendering after being told to look
//      down.
//   2. an observer can SHOOT.  `ps.gunframe` is moved only by the weaponthink,
//      so an observer whose gunframe never leaves zero is one whose weapon
//      really is gated -- asserted in both signs against an entered player on
//      the same server.
//   3. the match COUNTDOWN has no clock.  Tourney draws it out of
//      SID_OSP_MATCHSTATE, which holds a configstring INDEX rather than a
//      value, so the check reads the stat and then reads the configstring it
//      points at and watches it count down.
//   4. the match START leaves bodies.  OSP kills everybody into a fresh spawn
//      at the end of the countdown; the donor's Cmd_Kill_f respawns them in the
//      same call, so the sweep that runs immediately after can park the corpses
//      it made.  A client left DEAD at "Match has started!" is the visible half
//      of a kill that did not respawn, and dropped items are the other half.
package main

import (
	"flag"
	"fmt"
	"math"
	"os"
	"regexp"
	"strconv"
	"strings"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
)

const (
	statFrags      = 14
	statMatchState = 17 // SID_OSP_MATCHSTATE in the OSP column (g_stats.h)
)

var pass, fail int

func ok(what, detail string)  { pass++; fmt.Printf("  [ ok ] %-56s %s\n", what, detail) }
func bad(what, detail string) { fail++; fmt.Printf("  [FAIL] %-56s %s\n", what, detail) }

func pmName(t int) string {
	switch t {
	case playtest.PMNormal:
		return "PM_NORMAL"
	case playtest.PMSpectator:
		return "PM_SPECTATOR"
	case playtest.PMDead:
		return "PM_DEAD"
	case playtest.PMFreeze:
		return "PM_FREEZE"
	case playtest.PMGib:
		return "PM_GIB"
	}
	return fmt.Sprintf("pm_type %d", t)
}

var reEdicts = regexp.MustCompile(`(\d+) edicts in use, (\d+) clients`)

// props is the census with the CLIENTS taken out of it: an edict per connected
// player is not litter, and counting them makes every phase look like it left
// something behind.  What is left moves only when something is spawned into the
// world or freed out of it -- a dropped weapon, a gib, a rune.
func props(srv *playtest.Server) int {
	mark := srv.Len()
	srv.Console("sv ruleset")
	srv.WaitLog(`edicts in use`, 5*time.Second)
	time.Sleep(400 * time.Millisecond)
	for _, l := range srv.GrepFrom(mark, `edicts in use`) {
		if m := reEdicts.FindStringSubmatch(l); m != nil {
			n, _ := strconv.Atoi(m[1])
			c, _ := strconv.Atoi(m[2])
			return n - c
		}
	}
	return -1
}

func main() {
	q2 := flag.String("q2proded", "", "")
	lib := flag.String("lib", "", "")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "")
	dir := flag.String("dir", "/tmp/colosseum-ospwarmup", "")
	port := flag.Int("port", 27981, "")
	rs := flag.String("ruleset", "tdm", "which OSP ruleset to drive")
	glad := flag.String("gladdir", "", "")
	flag.Parse()

	if !colosseum.IsOSP(*rs) {
		fmt.Printf("ospwarmup: %s is not one of the OSP four\n", *rs)
		os.Exit(2)
	}
	run(*q2, *lib, *ref, *ctf, *dir, *glad, *port, *rs)

	fmt.Printf("\n%d check(s), %d failed\n", pass+fail, fail)
	if fail > 0 {
		os.Exit(1)
	}
}

func run(q2, lib, ref, ctf, dir, glad string, port int, rs string) {
	fmt.Printf("\n##### %s on q2dm1 (port %d)\n", rs, port)

	if err := colosseum.Install(dir, ref, ctf, lib); err != nil {
		panic(err)
	}
	srv := &playtest.Server{
		Binary: q2, Dir: dir, Game: "colosseum", Map: "q2dm1", Port: port,
		MaxClients: 8,
		Cvars: map[string]string{
			"g_ruleset": rs, "bots": "0", "bots_minplayers": "0",
			"deathmatch": "1", "coop": "0",
			"team_a_name": "Hometeam", "team_b_name": "Visitors",
			// The shortest countdown osp_main.c will accept -- it clamps the
			// cvar UP to 14, so asking for less does not make the wait shorter.
			"match_countdown": "14",
			"flood_msgs":      "0",
		},
		LogPath: dir + "/server.log",
	}
	if err := srv.Start(); err != nil {
		panic(err)
	}
	defer srv.Stop()

	base := props(srv)
	fmt.Printf("  ....  baseline: %d non-client edicts before anyone connects\n", base)

	// ---------------------------------------------------------------- 1 & 2
	// The observer, and a player on the same server as its control.
	watch := playtest.NewBot("watcher", "127.0.0.1", port)
	if err := watch.Start(30 * time.Second); err != nil {
		panic(err)
	}
	defer watch.Disconnect()
	watch.WaitFrames(30, 20*time.Second)

	if pm := watch.PMType(); pm == playtest.PMSpectator {
		ok("observer/is really an observer", pmName(pm))
	} else {
		bad("observer/is really an observer", pmName(pm)+" -- it was placed as a body")
	}

	// -- the pitch pin, witness one: the flag word itself.
	fl, tm := watch.PMFlags(), watch.PMTime()
	if fl&playtest.PMFTimeTeleport == 0 {
		ok("observer/no PMF_TIME_TELEPORT on a PM_SPECTATOR",
			fmt.Sprintf("pm_flags 0x%02x pm_time %d", fl, tm))
	} else {
		bad("observer/no PMF_TIME_TELEPORT on a PM_SPECTATOR",
			fmt.Sprintf("pm_flags 0x%02x pm_time %d -- PM_ClampAngles pins PITCH "+
				"and ROLL, and the countdown that clears it is below "+
				"Pmove()'s PM_SPECTATOR return", fl, tm))
	}

	// -- witness two: point the view down and read back what it renders.
	watch.Look(-45, 30, 0)
	watch.WaitFrames(15, 10*time.Second)
	down := watch.ViewAngles()
	watch.Look(35, 30, 0)
	watch.WaitFrames(15, 10*time.Second)
	up := watch.ViewAngles()

	if math.Abs(down[0]-(-45)) < 6 && math.Abs(up[0]-35) < 6 {
		ok("observer/the view pitch follows the mouse",
			fmt.Sprintf("look -45 -> %.1f, look +35 -> %.1f", down[0], up[0]))
	} else {
		bad("observer/the view pitch follows the mouse",
			fmt.Sprintf("look -45 -> %.1f, look +35 -> %.1f (yaw %.1f/%.1f) "+
				"-- the vertical axis is frozen", down[0], up[0], down[1], up[1]))
	}
	watch.Look(0, 0, 0)

	// -- does an observer's weapon THINK?  Asked WITHOUT pressing anything,
	// because ATTACK is not a trigger for a tourney observer: the first press
	// opens the team menu and the second picks a row off it, so a scenario that
	// holds fire here joins a team and then measures a player.  It does not
	// need the button: Weapon_Generic advances ps.gunframe round its idle loop
	// on every think, so a gunframe that has moved at all is a weaponthink that
	// ran.
	watch.WaitFrames(40, 20*time.Second)
	if g := watch.GunFrameHigh(); g == 0 {
		ok("observer/its weapon never thinks", "gunframe high-water 0 over 40 frames")
	} else {
		bad("observer/its weapon never thinks",
			fmt.Sprintf("gunframe reached %d -- an observer ran the weaponthink", g))
	}

	// -- and the human's own route in, which is the one the report came from:
	// ATTACK opens the team menu, ATTACK again picks the row under the cursor.
	// Whatever that lands on, the client must not be a body with a live weapon
	// while the game still calls it an observer -- so both halves are read
	// after the two presses.
	before := watch.PMType()
	watch.Press(playtest.ButtonAttack, 600*time.Millisecond)
	watch.WaitFrames(10, 10*time.Second)
	menuUp := strings.Contains(watch.Layout(), "Team") ||
		strings.Contains(playtest.Decode(watch.StatusBar()), "Team")
	watch.Press(playtest.ButtonAttack, 600*time.Millisecond)
	watch.WaitFrames(20, 15*time.Second)
	after2 := watch.PMType()
	fmt.Printf("  ....  ATTACK x2 as an observer: %s -> %s (menu seen: %v)\n",
		pmName(before), pmName(after2), menuUp)

	watch.Look(-45, 60, 0)
	watch.WaitFrames(15, 10*time.Second)
	d2 := watch.ViewAngles()
	watch.Look(35, 60, 0)
	watch.WaitFrames(15, 10*time.Second)
	u2 := watch.ViewAngles()
	fl2 := watch.PMFlags()
	if math.Abs(d2[0]-(-45)) < 6 && math.Abs(u2[0]-35) < 6 {
		ok("menu/the view pitch still follows the mouse after the menu",
			fmt.Sprintf("%s, pm_flags 0x%02x, look -45 -> %.1f, +35 -> %.1f",
				pmName(after2), fl2, d2[0], u2[0]))
	} else {
		bad("menu/the view pitch still follows the mouse after the menu",
			fmt.Sprintf("%s, pm_flags 0x%02x, look -45 -> %.1f, +35 -> %.1f "+
				"-- the vertical axis is frozen", pmName(after2), fl2, d2[0], u2[0]))
	}
	watch.Look(0, 0, 0)

	// Whatever the menu did, put this client back where the rest of the
	// scenario needs it: an observer that is not counted among the players who
	// have to ready up.
	watch.Cmd("observer")
	watch.WaitFrames(20, 15*time.Second)

	// ---------------------------------------------------------------- teams
	red := playtest.NewBot("red", "127.0.0.1", port)
	if err := red.Start(30 * time.Second); err != nil {
		panic(err)
	}
	defer red.Disconnect()
	red.WaitFrames(20, 20*time.Second)
	red.Cmd("join Hometeam")
	red.WaitFrames(30, 20*time.Second)

	blue := playtest.NewBot("blue", "127.0.0.1", port)
	if err := blue.Start(30 * time.Second); err != nil {
		panic(err)
	}
	defer blue.Disconnect()
	blue.WaitFrames(20, 20*time.Second)
	blue.Cmd("join Visitors")
	blue.WaitFrames(30, 20*time.Second)

	if pm := red.PMType(); pm == playtest.PMNormal {
		ok("player/a client that joined a team is a body", pmName(pm))
	} else {
		bad("player/a client that joined a team is a body", pmName(pm))
	}

	// The control for the row above: the SAME server, a client that HAS
	// entered, holding the same button.  Without it "gunframe stayed 0" is
	// equally consistent with a wire that never carries a gunframe.
	red.Press(playtest.ButtonAttack, 700*time.Millisecond)
	red.Press(playtest.ButtonAttack, 700*time.Millisecond)
	red.WaitFrames(10, 10*time.Second)
	if g := red.GunFrameHigh(); g > 0 {
		ok("player/control: an entered player's weapon DOES think",
			fmt.Sprintf("gunframe reached %d", g))
	} else {
		bad("player/control: an entered player's weapon DOES think",
			"gunframe stayed 0 -- the observer row above proves nothing")
	}

	// ---------------------------------------------------------------- 3
	// Ready up and watch the countdown clock.
	mark := srv.Len()
	red.Cmd("ready")
	blue.Cmd("ready")
	if _, err := srv.WaitLog(`countdown starts`, 15*time.Second); err != nil {
		bad("countdown/the match countdown starts", "no `countdown starts!` broadcast")
		return
	}
	ok("countdown/the match countdown starts", strings.TrimSpace(last(srv.GrepFrom(mark, `countdown starts`))))

	red.WaitFrames(10, 10*time.Second)
	cs := red.Stat(statMatchState)
	if cs > 0 {
		ok("countdown/SID_OSP_MATCHSTATE points at a configstring",
			fmt.Sprintf("stat %d = configstring %d", statMatchState, cs))
	} else {
		bad("countdown/SID_OSP_MATCHSTATE points at a configstring",
			fmt.Sprintf("stat %d = %d -- the HUD has no clock cell to draw",
				statMatchState, cs))
	}

	// ...and the cell it points at must actually be counting.
	if cs > 0 {
		first := strings.TrimSpace(red.ConfigString(cs))
		red.WaitFrames(25, 15*time.Second)
		second := strings.TrimSpace(red.ConfigString(cs))
		if first != "" && second != "" && first != second {
			ok("countdown/the clock counts down",
				fmt.Sprintf("configstring %d: %q -> %q", cs, first, second))
		} else {
			bad("countdown/the clock counts down",
				fmt.Sprintf("configstring %d: %q -> %q", cs, first, second))
		}
	}

	// ...and the OBSERVER's own HUD, which is the seat the report came from.
	// The donor points every client's clock cell at the same configstring in
	// OSP_CheckReady, players and watchers alike -- an observer is meant to be
	// able to follow a match, and a clock it cannot see is the first
	// thing missing from that.
	ocs := watch.Stat(statMatchState)
	if ocs > 0 {
		ok("countdown/an observer gets the clock too",
			fmt.Sprintf("stat %d = configstring %d, %q",
				statMatchState, ocs, strings.TrimSpace(watch.ConfigString(ocs))))
	} else {
		bad("countdown/an observer gets the clock too",
			fmt.Sprintf("stat %d = %d -- the watcher's HUD has no clock cell",
				statMatchState, ocs))
	}

	// The bar has to CARRY the cell as well as the stat having a value: the
	// statusbar is composed per ruleset and the "Time" panel is one `if
	// stat 17` block inside it.
	sb := playtest.Decode(red.StatusBar())
	if strings.Contains(sb, "Time") {
		ok("countdown/the composed statusbar draws a Time panel",
			fmt.Sprintf("%d bytes of bar, `Time` present", len(sb)))
	} else {
		bad("countdown/the composed statusbar draws a Time panel",
			fmt.Sprintf("%d bytes of bar and no `Time` cell", len(sb)))
	}

	// ---------------------------------------------------------------- 4
	mark = srv.Len()
	if _, err := srv.WaitLog(`Match has started`, 40*time.Second); err != nil {
		bad("start/the match starts", "no `Match has started!` broadcast")
		return
	}
	ok("start/the match starts", "Match has started!")

	// Give the kill and whatever follows it a full second of frames.
	red.WaitFrames(20, 15*time.Second)

	rpm, bpm := red.PMType(), blue.PMType()
	if rpm != playtest.PMDead && bpm != playtest.PMDead {
		ok("start/nobody is left dead by the match-start kill",
			fmt.Sprintf("red %s, blue %s", pmName(rpm), pmName(bpm)))
	} else {
		bad("start/nobody is left dead by the match-start kill",
			fmt.Sprintf("red %s, blue %s -- Cmd_Kill_f did not respawn them",
				pmName(rpm), pmName(bpm)))
	}

	if h := red.Stat(1); h > 0 {
		ok("start/a player starts the match alive", fmt.Sprintf("STAT_HEALTH %d", h))
	} else {
		bad("start/a player starts the match alive",
			fmt.Sprintf("STAT_HEALTH %d", h))
	}

	// Dropped weapons are NEW edicts and survive 30 seconds, so the census is
	// the witness: three connected clients and no litter is the baseline plus
	// nothing.  (Bodyques are pre-allocated at level load and do not move it,
	// which is why the death count is read off pm_type above instead.)
	after := props(srv)
	if after <= base {
		ok("start/the match-start kill dropped nothing",
			fmt.Sprintf("%d non-client edicts, baseline %d", after, base))
	} else {
		bad("start/the match-start kill dropped nothing",
			fmt.Sprintf("%d non-client edicts against a %d baseline -- %d left in the world",
				after, base, after-base))
	}

	// ...and the obituaries the kill wrote, which a player reads as "everybody
	// died" in the console.
	if ob := srv.GrepFrom(mark, `killed himself|suicide|cratered|tried to put the pin back`); len(ob) > 0 {
		bad("start/the match-start kill is silent",
			fmt.Sprintf("%d obituary/ies, e.g. %q", len(ob), strings.TrimSpace(ob[0])))
	} else {
		ok("start/the match-start kill is silent", "no obituary broadcast")
	}
}

func last(ls []string) string {
	if len(ls) == 0 {
		return ""
	}
	return ls[len(ls)-1]
}
