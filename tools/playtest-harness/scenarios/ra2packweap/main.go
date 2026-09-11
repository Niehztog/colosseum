// ra2packweap -- the pack weapons' menu half: does the arena settings menu offer the
// mission packs' six weapons exactly when their content layer is switched on?
//
// The server console cannot answer this.  An RA2 menu IS the client's
// statusbar, so the only witness to which rows a player is offered is a client
// reading its own bar -- the same reason the `allowvotingbots` row needed
// a scenario rather than a `sv` command.
//
// Five phases, each a whole server, and every one asserts a DIFFERENCE:
//
//	neither    xatrix 0 rogue 0  -- none of the six
//	xatrix     xatrix 1 rogue 0  -- Ionripper and Phalanx, and only those two
//	rogue      xatrix 0 rogue 1  -- the four Ground Zero rows, and only those
//	both       xatrix 1 rogue 1  -- all six
//	novote     both layers on, allowvotingpackweapons 0 -- none, for a PLAYER
//
// The last one is the control that separates "the layer is on" from "the arena
// lets a player vote on it": without it, every row appearing could be the gate
// never having been consulted.
//
// The menu SCROLLS -- menu.c draws 18 rows and this one is longer -- so the
// cursor is walked and the union of the pages taken.  A first-page read would
// report a present row as missing, which is the mistake that scenario
// records making.
package main

import (
	"flag"
	"fmt"
	"os"
	"sort"
	"strings"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
	"q2playtest/ra2"
)

var fails int

func check(name string, ok bool, detail string) {
	tag := " ok "
	if !ok {
		tag = "FAIL"
		fails++
	}
	fmt.Printf("  [%s] %-40s %s\n", tag, name, detail)
}

// The six rows, by the label ra_pack_weapons[] gives them, trimmed.
var packRows = []string{
	"Allow Ionripper:", "Allow Phalanx:",
	"Allow ETF Rifle:", "Allow Prox Launcher:",
	"Allow Plasma Beam:", "Allow Chainfist:",
}

var xatrixRows = map[string]bool{"Allow Ionripper:": true, "Allow Phalanx:": true}

// rows opens one client's "Change Arena Settings" menu and returns every row it
// draws across all pages.
func rows(tag, cfg string, cv map[string]string, port int,
	q2, ref, ctf, lib, glad, root string) ([]string, error) {
	d := fmt.Sprintf("%s/%s", root, tag)
	if err := colosseum.Install(d, ref, ctf, lib); err != nil {
		return nil, err
	}
	if err := colosseum.InstallBrain(d, glad); err != nil {
		return nil, err
	}
	path := d + "/colosseum/arena.cfg"
	os.Remove(path)
	if err := os.WriteFile(path, []byte(cfg), 0o644); err != nil {
		return nil, err
	}

	c := map[string]string{
		"g_ruleset": "arena", "cheats": "1",
		// The fill is off: a bot arriving mid-menu can start a round and move
		// this client out of FIGHT_SPECTATING, which is what draws the rows.
		"botfill": "0", "minimumplayers": "0", "bots_minplayers": "0",
	}
	for k, v := range cv {
		c[k] = v
	}

	srv := &playtest.Server{
		Binary: q2, Dir: d, Game: "colosseum", Map: "q2dm1",
		Port: port, MaxClients: 12, Cvars: c, LogPath: d + "/server.log",
	}
	if err := srv.Start(); err != nil {
		return nil, err
	}
	defer srv.Stop()

	b := playtest.NewBot("voter", "127.0.0.1", port)
	if err := b.Start(30 * time.Second); err != nil {
		return nil, err
	}
	defer b.Disconnect()

	// q2dm1 is an idmap: one pickup arena, whose two teams the mod makes at map
	// load, and a pickup arena refuses the ordinary arena menu -- so the client
	// joins a team already in it.
	if err := ra2.JoinTeam(b, ra2.PickupTeam(1, "Red")); err != nil {
		return nil, fmt.Errorf("join: %w", err)
	}
	b.WaitFrames(10, 5*time.Second)

	b.Cmd("inven") // TAB; RA2 has no console command for any of this
	if err := b.WaitMenu(`(?i)observer options`, 20*time.Second); err != nil {
		return nil, fmt.Errorf("observer menu: %w", err)
	}
	if err := b.MenuPick(`(?i)change arena settings`, 20*time.Second); err != nil {
		return nil, fmt.Errorf("propose: %w", err)
	}
	if err := b.WaitMenu(`(?i)arena admin menu`, 20*time.Second); err != nil {
		return nil, fmt.Errorf("admin menu: %w", err)
	}

	seen := map[string]bool{}
	var out []string
	add := func() {
		_, items := b.Menu()
		for _, it := range items {
			t := strings.TrimSpace(it.Text)
			if t == "" || seen[t] {
				continue
			}
			seen[t] = true
			out = append(out, t)
		}
	}
	add()
	for i := 0; i < 40; i++ {
		b.Cmd("invnext")
		time.Sleep(120 * time.Millisecond)
		add()
	}
	sort.Strings(out)
	return out, nil
}

func main() {
	q2 := flag.String("q2proded", "", "")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "")
	glad := flag.String("gladdir", "", "")
	lib := flag.String("lib", "", "")
	dir := flag.String("dir", "/tmp/q2playtest/ra2packweap", "")
	port := flag.Int("port", 27930, "")
	flag.Parse()

	if *glad == "" {
		check("skipped", true, "no -gladdir, and InstallBrain needs it")
		fmt.Printf("\n%d check(s) failed\n", fails)
		return
	}

	base := "maploop: q2dm1;\nweapons: 2 3 4 5 6 7 8 9;\n"

	for _, ph := range []struct {
		tag   string
		cfg   string
		cv    map[string]string
		want  func(string) bool
		label string
	}{
		{"neither", base, map[string]string{"xatrix": "0", "rogue": "0"},
			func(string) bool { return false }, "no pack row"},
		{"xatrix", base, map[string]string{"xatrix": "1", "rogue": "0"},
			func(r string) bool { return xatrixRows[r] }, "the two Reckoning rows"},
		{"rogue", base, map[string]string{"xatrix": "0", "rogue": "1"},
			func(r string) bool { return !xatrixRows[r] }, "the four Ground Zero rows"},
		{"both", base, map[string]string{"xatrix": "1", "rogue": "1"},
			func(string) bool { return true }, "all six"},
		{"novote", base + "allowvotingpackweapons: 0;\n",
			map[string]string{"xatrix": "1", "rogue": "1"},
			func(string) bool { return false }, "no pack row for a player"},
	} {
		got, err := rows(ph.tag, ph.cfg, ph.cv, *port, *q2, *ref, *ctf, *lib, *glad, *dir)
		*port++
		if err != nil {
			check("menu/"+ph.tag, false, err.Error())
			continue
		}
		// A drawn row is its LABEL plus its value -- "Allow BFG10K:          NO"
		// -- so a row is matched by prefix.  Comparing whole strings reported
		// every row missing, which is how this was found.
		have := func(label string) bool {
			for _, r := range got {
				if strings.HasPrefix(r, label) {
					return true
				}
			}
			return false
		}
		// The donor's own nine must be there in every phase, which is what says
		// the menu was read at all rather than coming back empty.
		if os.Getenv("DUMPROWS") != "" {
			fmt.Printf("  ---- %s rows ----\n", ph.tag)
			for _, r := range got {
				fmt.Printf("       %q\n", r)
			}
		}
		if !have("Allow BFG10K:") || !have("Allow Railgun:") {
			check("menu/"+ph.tag, false,
				fmt.Sprintf("the donor's rows are missing too; read %d row(s)", len(got)))
			continue
		}

		var wrong []string
		var found []string
		for _, r := range packRows {
			if have(r) {
				found = append(found, strings.TrimSuffix(r, ":"))
			}
			if have(r) != ph.want(r) {
				wrong = append(wrong, r)
			}
		}
		detail := ph.label + " -> " + strings.Join(found, ", ")
		if len(found) == 0 {
			detail = ph.label + " -> none"
		}
		if len(wrong) > 0 {
			detail += " | wrong: " + strings.Join(wrong, ", ")
		}
		check("menu/"+ph.tag, len(wrong) == 0, detail)
	}

	fmt.Printf("\n%d check(s) failed\n", fails)
	if fails > 0 {
		os.Exit(1)
	}
}
