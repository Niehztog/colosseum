// layeracc -- the loadout cvars, end to end: does a content-layer weapon reach tourney's
// accuracy report, and does a content layer's OWN map spawn its own content?
//
// Two phases, because the two halves of the content layers need different evidence.
//
//	spawn  boot a Reckoning and a Ground Zero map and census their weapons.
//	       "the union content is always spawnable" was verified from
//	       the itemlist before; this verifies it from a map that places them.
//	acc    give bots a layer weapon through the loadout cvars, let
//	       them fight, and read the accuracy report back off the wire.  The
//	       weapon comes from the LOADOUT rather than the floor, so the map can
//	       be q2dm1 -- which has the .aas the brain needs to navigate, and
//	       xdm1/rdm1 do not.
//
// The acc phase runs twice, and the second run is the control: the same server
// with weapon_have 0 must produce NO layer row, so a row in the first run is
// the cvar working rather than the report printing everything it has.
//
// `dm` is deliberate: RULESET_DM is OSP's RegularDM and sets sync_stat = 8, so a
// match is live from the first frame and OSP_accShot's `sync_stat <= 2` guard is
// open without anybody readying up.
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
)

var fails int

func check(name string, ok bool, detail string) {
	tag := " ok "
	if !ok {
		tag = "FAIL"
		fails++
	}
	fmt.Printf("  [%s] %-44s %s\n", tag, name, detail)
}

// EVERY BASEQ2 WEAPON IS SWITCHED OFF, through the allow_* family.
//
// Without this the accuracy phase was FLAKY and passed most of the time: q2dm1
// is covered in weapons, so a bot spawning with an ETF Rifle fires a few
// flechettes and then picks up a rocket launcher, and whether any ETF shot
// landed inside the sample window was luck.  Two runs in a row disagreed, which
// is the only reason it was found.
//
// With the floor stripped, the loadout is the only source of a weapon and the
// blaster the only alternative, so a bot that fights at all fights with the
// thing being measured.  It also makes this scenario exercise the inhibit it
// depends on: if allow_* stopped working the phase would go flaky again rather
// than silently pass.
var offBaseq2 = []string{
	"allow_shotgun", "allow_supershotgun", "allow_machinegun",
	"allow_chaingun", "allow_grenadelauncher", "allow_rocketlauncher",
	"allow_hyperblaster", "allow_railgun", "allow_bfg",
	"allow_ammo_grenades",
}

func cvars(m map[string]string) map[string]string {
	for _, c := range offBaseq2 {
		m[c] = "0"
	}
	return m
}

func link(src, dst string) error {
	abs, _ := filepath.Abs(src)
	os.Remove(dst)
	return os.Symlink(abs, dst)
}

func main() {
	q2 := flag.String("q2proded", "", "")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "")
	xat := flag.String("xatrix", os.Getenv("HOME")+"/q2-dev/yquake2/release_/xatrix", "")
	rog := flag.String("rogue", os.Getenv("HOME")+"/q2-dev/yquake2/release_/rogue", "")
	glad := flag.String("gladdir", os.Getenv("HOME")+"/q2-dev/gladiator-bot-restored", "")
	lib := flag.String("lib", "", "")
	dir := flag.String("dir", "/tmp/q2playtest/layeracc", "")
	port := flag.Int("port", 27950, "")
	flag.Parse()

	// The house rule for a missing prerequisite: skip the phase, name what was
	// missing, and never subtract it silently.
	have := func(p, what string) bool {
		if _, err := os.Stat(p); err != nil {
			check("skipped/"+what, true, "no "+p)
			return false
		}
		return true
	}
	haveXat := have(filepath.Join(*xat, "pak0.pak"), "xatrix")
	haveRog := have(filepath.Join(*rog, "pak0.pak"), "rogue")
	haveGlad := *glad != "" && have(filepath.Join(*glad, "release", "gladiator.so"), "brain")

	install := func(d string, brain bool) error {
		if err := colosseum.Install(d, *ref, *ctf, *lib); err != nil {
			return err
		}
		game := filepath.Join(d, "colosseum")
		// pak10/pak11: baseq2 takes 0..3 and CTF 8..9, so these are free.
		if haveXat {
			if err := link(filepath.Join(*xat, "pak0.pak"), filepath.Join(game, "pak10.pak")); err != nil {
				return err
			}
		}
		if haveRog {
			if err := link(filepath.Join(*rog, "pak0.pak"), filepath.Join(game, "pak11.pak")); err != nil {
				return err
			}
		}
		if brain {
			return colosseum.InstallBrain(d, *glad)
		}
		return nil
	}

	// ---------------------------------------------------------------- spawn
	for _, m := range []struct {
		name, mp, want string
		ok             bool
	}{
		{"xatrix", "xdm1", "weapon_", haveXat},
		{"rogue", "rdm1", "weapon_", haveRog},
	} {
		if !m.ok {
			continue
		}
		d := fmt.Sprintf("%s/spawn-%s", *dir, m.name)
		if err := install(d, false); err != nil {
			fmt.Fprintln(os.Stderr, "ERROR:", err)
			os.Exit(2)
		}
		srv := &playtest.Server{
			Binary: *q2, Dir: d, Game: "colosseum", Map: m.mp, Port: *port,
			MaxClients: 8,
			Cvars: map[string]string{"g_ruleset": "dm", "cheats": "1",
				"xatrix": "1", "rogue": "1"},
			LogPath: d + "/server.log",
		}
		if err := srv.Start(); err != nil {
			check("spawn/"+m.name+" boots", false, err.Error())
			continue
		}
		time.Sleep(2 * time.Second)
		srv.Console("sv census weapon_")
		srv.Console("sv census item_")
		srv.Console("sv census ammo_")
		time.Sleep(1500 * time.Millisecond)
		log := strings.Join(srv.Log(), "\n")
		srv.Stop()

		re := regexp.MustCompile(`census (\S+): (\d+) spawned, (\d+) in world`)
		got := map[string]string{}
		for _, mm := range re.FindAllStringSubmatch(log, -1) {
			got[mm[1]] = mm[2] + " spawned, " + mm[3] + " in world"
		}
		bad := strings.Contains(log, "ERROR")
		check("spawn/"+m.name+" boots "+m.mp, !bad && got["weapon_"] != "",
			fmt.Sprintf("weapon_ %s | item_ %s | ammo_ %s",
				got["weapon_"], got["item_"], got["ammo_"]))
	}

	// ---------------------------------------------------------------- acc
	// weapon_have / weapon_initial 0x400 is the ETF Rifle bit, and
	// start_flechettes is the cvar for its ammo -- so one row proves the
	// bit, the ammo cvar, the OSP_accShot call and the new column together.
	for _, phase := range []struct {
		tag  string
		have string
		want bool
	}{
		{"etf", "1024", true},
		{"control", "0", false},
	} {
		// The accuracy phases need bots: a headless client cannot walk, so
		// nothing else on this harness will fire a weapon at another player.
		if !haveGlad {
			continue
		}
		d := fmt.Sprintf("%s/acc-%s", *dir, phase.tag)
		if err := install(d, true); err != nil {
			check("acc/"+phase.tag+" install", false, err.Error())
			continue
		}
		p := *port + 1
		srv := &playtest.Server{
			Binary: *q2, Dir: d, Game: "colosseum", Map: "q2dm1", Port: p,
			MaxClients: 12,
			Cvars: cvars(map[string]string{
				"g_ruleset": "dm", "cheats": "1", "skill": "3",
				"minimumplayers": "0", "bots_minplayers": "0",
				"weapon_have": phase.have, "weapon_initial": phase.have,
				"start_flechettes": "200",
			}),
			LogPath: d + "/server.log",
		}
		if err := srv.Start(); err != nil {
			check("acc/"+phase.tag+" boots", false, err.Error())
			continue
		}

		for i := 0; i < 4; i++ {
			srv.Console("sv addrandom")
			time.Sleep(400 * time.Millisecond)
		}

		watcher := playtest.NewBot("watcher", "127.0.0.1", p)
		if err := watcher.Start(30 * time.Second); err != nil {
			check("acc/"+phase.tag+" watcher", false, err.Error())
			srv.Stop()
			continue
		}

		// NO OBITUARY CHECK.  It used to be one, as a precondition, and it was
		// the second flake in this phase: with the baseq2 weapons stripped off
		// the floor the control's bots carry nothing but a blaster, and a blaster
		// fight does not reliably produce a kill inside a fixed window.  A kill
		// was never the evidence anyway -- OSP_accShot counts SHOTS -- so the
		// polled assertions below carry the phase and their own deadline, and
		// `any` is what tells a quiet server apart from a broken one.
		srv.WaitLog(`(?i)(killed|blasted|railed|was|tried)`, 30*time.Second)
		time.Sleep(10 * time.Second)

		// Which bots are on?  Read the names off the playerskins configstrings.
		names := []string{}
		cs := watcher.ConfigStrings()
		for k, v := range cs {
			s := playtest.Decode(v)
			if i := strings.IndexByte(s, '\\'); i > 0 && k > 1000 {
				if n := s[:i]; n != "" && n != "watcher" {
					names = append(names, n)
				}
			}
		}

		// POLLED, not sampled.  A bot has to choose to shoot, and asking once
		// makes this a question about timing rather than about the code.  The
		// loop ends the moment what it wants is there, so a healthy run is as
		// quick as the old one and only a broken one waits.
		// ACCUMULATED, not overwritten -- one sample must not be able to erase
		// what an earlier one proved.  `Prints()` is read fresh per sample and a
		// 900ms window can catch a report's rows without its header, so
		// assigning `table = out` let a partial capture blank a good one: that
		// is what failed one run in five as BOTH "no accuracy table came back"
		// and "acc/report reached a client", from a single lost header.  The
		// question these checks ask is "did the report ever show this", so the
		// answer has to be sticky the way `found` already was.
		found, any := false, false
		var seen strings.Builder
		// 150s, not 90.  The loop returns the moment it has what it wants, so a
		// healthy run is unaffected and only a slow one uses the extra time --
		// which is the right trade for a check whose subject is "does a bot
		// ever fire this weapon" rather than "does it fire within 90 seconds".
		deadline := time.Now().Add(150 * time.Second)
		for time.Now().Before(deadline) {
			for _, n := range names {
				watcher.Cmd("accuracy %s", n)
				time.Sleep(900 * time.Millisecond)
				out := strings.Join(watcher.Prints(), "\n")
				seen.WriteString(out)
				seen.WriteByte('\n')
				if strings.Contains(out, "ETF Rifle") {
					found = true
				}
				// Any weapon row at all: the control needs this to tell "no ETF
				// row" apart from "the report never ran".
				for _, w := range []string{"Blaster ", "Shotgun ", "Machinegun",
					"Railgun ", "R.Launcher", "Chaingun ", "H.Blaster"} {
					if strings.Contains(out, w) {
						any = true
					}
				}
			}
			// The control forbids what the other phase waits for, so it stops
			// as soon as it has seen the report work at all.
			if found || (!phase.want && any) {
				break
			}
		}
		srv.Stop()

		// Everything the report printed across the whole poll, so a lost header
		// in the last sample cannot make an earlier success unprovable.
		table := seen.String()
		if strings.TrimSpace(table) == "" {
			table = ""
		}

		if table != "" {
			fmt.Printf("  ---- %s: full accuracy table ----\n", phase.tag)
			for _, l := range strings.Split(table, "\n") {
				if l = strings.TrimRight(l, "\r"); strings.TrimSpace(l) != "" {
					fmt.Printf("       %s\n", l)
				}
			}
		}
		if phase.want {
			detail := firstETF(table)
			if !found {
				// Which of the two failures this is, said out loud: a report
				// that ran and saw no ETF shot is a different bug from a server
				// where nothing happened at all.
				if any {
					detail += " | the report ran (a baseq2 row came back), so " +
						"no ETF shot was counted"
				} else {
					detail += " | no weapon row of any kind came back, so " +
						"nothing was fighting"
				}
			}
			check("acc/etf row present", found, detail)
			check("acc/report reached a client", strings.Contains(table, "Accuracy info"),
				fmt.Sprintf("%d bot name(s) queried", len(names)))
		} else {
			// Two halves, because "no ETF row" on its own also passes on a
			// server where nothing happened at all.
			check("acc/control: no ETF row with weapon_have 0", !found,
				"report printed, no layer row")
			check("acc/control: the report could have shown one", any,
				fmt.Sprintf("a baseq2 weapon row came back (%d name(s) queried)",
					len(names)))
		}
	}

	fmt.Printf("\n%d check(s) failed\n", fails)
	if fails > 0 {
		os.Exit(1)
	}
}

func firstETF(table string) string {
	for _, l := range strings.Split(table, "\n") {
		if strings.Contains(l, "ETF Rifle") {
			return strings.TrimSpace(l)
		}
	}
	if table == "" {
		return "no accuracy table came back"
	}
	return "no ETF Rifle row in: " + strings.Join(strings.Fields(table), " ")
}
