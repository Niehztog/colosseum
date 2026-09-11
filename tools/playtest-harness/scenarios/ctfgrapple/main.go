// ctfgrapple -- are the CTF grapple's assets registered at map load, or one
// configstring at a time in the middle of a round?
//
// weapon_grapple is the one item in the CTF itemlist that is "always owned,
// never in the world", so the only two callers of PrecacheItem() -- SpawnItem()
// and worldspawn's explicit PrecacheItem(FindItem("Blaster")) -- never reach
// it.  Its view model, HUD icon, pickup sound and grapple sounds are therefore
// registered lazily, at the moment the code first asks for an index: the view
// model when ChangeWeapon() runs, the icon when P_SetStats() puts the grapple
// in STAT_HELPICON, grhurt.wav inside CTFGrapplePull(), the hook model inside
// CTFFireGrapple().  PF_FindIndex() does not check the file exists and does not
// refuse a late call -- it just allocates the next slot and broadcasts a new
// configstring -- which is why this is a mid-round hitch and a blank HUD icon
// rather than a crash, and why nothing has caught it.
//
// The claim is about *when* a configstring exists, so the only witness is a
// client watching the configstring stream.  Four snapshots, one per phase:
//
//	load    everything the server sent at connect -- i.e. worldspawn's work
//	join    after "team red", so PutClientInServer/ChangeWeapon have run
//	select  after "use Grapple", so the grapple is the equipped weapon
//	fire    after latching BUTTON_ATTACK, so CTFGrappleFire() has run
//
// Three checks:
//
//  1. every grapple asset is already in the load snapshot.
//  2. no grapple asset first appears in join, select or fire.  This is the
//     half that fails on a build without the fix, and the list it prints
//     names which asset arrived in which phase.
//  3. with the grapple equipped, STAT_HELPICON indexes the configstring that
//     holds "w_grapple".  Self-calibrating: the images base is read off the
//     *blaster* icon during the join phase (weapon is the blaster there), so
//     the check is immune to the extended-configstring remap.  It is also the
//     proof that phases 3 and 4 really equipped and fired the grapple -- check
//     2 passing because nothing happened would fail here.
//
// Exit 0 all passed, 1 a check failed, 2 the scenario could not run.
package main

import (
	"flag"
	"fmt"
	"os"
	"sort"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
)

const (
	buttonAttack = 1
	// player_state->stats[] index: the icon of the weapon the client is
	// holding.  P_SetStats() fills it with gi.imageindex(weapon->icon), so on
	// a build that never precached the grapple this is the frame that creates
	// the w_grapple configstring.
	statHelpIcon = 11
)

// The nine indices weapon_grapple needs.  The first six are on the item row's
// .precaches list already; the last three are the ones only a running game
// asks for -- grhurt.wav from CTFGrapplePull(), the hook model from
// CTFFireGrapple(), and w_grapple from the HUD.
var assets = []string{
	"models/weapons/grapple/tris.md2",
	"weapons/grapple/grfire.wav",
	"weapons/grapple/grpull.wav",
	"weapons/grapple/grhang.wav",
	"weapons/grapple/grreset.wav",
	"weapons/grapple/grhit.wav",
	"weapons/grapple/grhurt.wav",
	"models/weapons/grapple/hook/tris.md2",
	"w_grapple",
}

var phases = []string{"load", "join", "select", "fire"}

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "retail baseq2 paks")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "Threewave CTF paks")
	lib := flag.String("lib", "", "game library under test")
	dir := flag.String("dir", "/tmp/q2playtest/ctfgrapple", "scratch install dir")
	mapname := flag.String("map", "q2ctf1", "map to test on")
	port := flag.Int("port", 27980, "server port")
	label := flag.String("label", "", "label for the report")
	flag.Parse()

	if *q2 == "" || *lib == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded and -lib")
		os.Exit(2)
	}
	bad, err := run(*q2, *ref, *ctf, *lib, *dir, *mapname, *port, *label)
	if err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	if bad {
		os.Exit(1)
	}
}

func run(q2, ref, ctf, lib, dir, mapname string, port int, label string) (bool, error) {
	if err := colosseum.Install(dir, ref, ctf, lib); err != nil {
		return false, err
	}
	srv := &playtest.Server{
		Binary: q2, Dir: dir, Game: "colosseum", Map: mapname, Port: port,
		MaxClients: 8,
		// Extensions stay OFF, which is the default and the only setting a
		// vanilla-protocol client can connect under: with svs.csr.extended the
		// server rejects anything below PROTOCOL_VERSION_Q2PRO_EXTENDED_LIMITS.
		// That in turn means the library must not write CS_GENERAL through the
		// compile-time macro -- see the note in the report.
		// `g_ruleset` because this library serves five rulesets and picks from
		// this cvar; without it the server boots `dm`, where `team red` reaches
		// no CTF join and every client stays a deathmatcher.
		Cvars:   map[string]string{"g_ruleset": "ctf", "cheats": "1", "dmflags": "1024"},
		LogPath: dir + "/server.log",
	}
	if err := srv.Start(); err != nil {
		return false, err
	}
	defer srv.Stop()

	b := playtest.NewBot("hooker", "127.0.0.1", port)
	if err := b.Start(30 * time.Second); err != nil {
		return false, err
	}
	defer b.Disconnect()

	// phase "load": everything the server had to say before this client did
	// anything at all.  That set is exactly what worldspawn registered.
	time.Sleep(2 * time.Second)
	snaps := map[string]map[int]string{"load": b.ConfigStrings()}

	// phase "join": a team, so the client gets a body, an inventory holding
	// the grapple, and a ChangeWeapon() to the blaster.
	// One "team" command is not reliably enough -- the client command can land
	// in the same frame as ClientBegin and be dropped -- so resend.  This is a
	// transient being retried, not a result being retried: a join that is
	// refused is refused every time.
	joined := false
	for i := 0; i < 5 && !joined; i++ {
		b.Cmd("team red")
		if _, err := b.WaitPrint(`(?i)joined the red team`, 4*time.Second); err == nil {
			joined = true
		}
	}
	if !joined {
		return false, fmt.Errorf("never joined a team after 5 attempts")
	}
	b.WaitFrames(20, 10*time.Second)
	if b.Spectating() {
		return false, fmt.Errorf("still a spectator after joining red")
	}
	// P_SetStats() only puts the held weapon's icon in STAT_HELPICON for a
	// centre-handed client (or one with fov > 91); the default right-handed
	// client gets 0 there.  So ask for centre-handed, which is what makes the
	// weapon icon observable at all.
	b.SetUserinfo("hand", "2")
	b.WaitFrames(20, 10*time.Second)

	// The blaster is the weapon here, so STAT_HELPICON is w_blaster's image
	// index -- which is how the images base is recovered without hardcoding
	// either configstring layout.
	blasterIcon := b.Stat(statHelpIcon)
	snaps["join"] = b.ConfigStrings()
	base, err := imagesBase(snaps["join"], "w_blaster", blasterIcon)
	if err != nil {
		return false, err
	}

	// phase "select": the grapple becomes the equipped weapon, which is what
	// asks for its view model and its HUD icon.
	b.Cmd("use Grapple")
	b.WaitFrames(20, 10*time.Second)
	grappleIcon := b.Stat(statHelpIcon)
	snaps["select"] = b.ConfigStrings()

	// phase "fire": latch BUTTON_ATTACK a few times.  CTFWeapon_Grapple only
	// fires on frame 6 of its activation sequence, so one press can land in
	// the wrong frame; three is enough and is still deterministic.
	for i := 0; i < 3; i++ {
		b.Press(buttonAttack, 400*time.Millisecond)
		b.WaitFrames(10, 5*time.Second)
	}
	b.WaitFrames(20, 10*time.Second)
	snaps["fire"] = b.ConfigStrings()

	// ---- report ----
	if label != "" {
		fmt.Printf("== %s ==\n", label)
	}
	fmt.Printf("%-40s %-8s %s\n", "asset", "phase", "configstring")
	first := map[string]string{}
	late := []string{}
	missing := []string{}
	for _, a := range assets {
		ph, idx := firstSeen(snaps, a)
		first[a] = ph
		switch ph {
		case "":
			missing = append(missing, a)
			fmt.Printf("%-40s %-8s %s\n", a, "never", "-")
		case "load":
			fmt.Printf("%-40s %-8s %d\n", a, ph, idx)
		default:
			late = append(late, fmt.Sprintf("%s (%s)", a, ph))
			fmt.Printf("%-40s %-8s %d   <-- late\n", a, ph, idx)
		}
	}
	fmt.Println()

	bad := false
	fail := func(f string, a ...any) {
		bad = true
		fmt.Printf("FAIL  "+f+"\n", a...)
	}

	// check 1
	if len(missing) == 0 && len(late) == 0 {
		fmt.Printf("PASS  all %d grapple assets registered at map load\n", len(assets))
	} else {
		fail("%d of %d grapple assets not registered at map load", len(missing)+len(late), len(assets))
		for _, a := range missing {
			fmt.Printf("      never registered at all: %s\n", a)
		}
	}

	// check 2
	if len(late) == 0 {
		fmt.Println("PASS  no grapple configstring was created mid-round")
	} else {
		fail("%d grapple configstring(s) created mid-round:", len(late))
		for _, a := range late {
			fmt.Printf("      %s\n", a)
		}
	}

	// check 3
	switch {
	case grappleIcon == 0:
		fail("STAT_HELPICON is 0 with the grapple equipped -- no HUD icon")
	case grappleIcon == blasterIcon:
		fail("STAT_HELPICON did not change from the blaster (%d) -- the grapple was never equipped", blasterIcon)
	default:
		got := snaps["fire"][base+grappleIcon]
		if got != "w_grapple" {
			fail("STAT_HELPICON is %d, and configstring %d is %q, not \"w_grapple\"",
				grappleIcon, base+grappleIcon, got)
		} else {
			fmt.Printf("PASS  grapple equipped: STAT_HELPICON %d -> configstring %d = %q\n",
				grappleIcon, base+grappleIcon, got)
		}
	}
	return bad, nil
}

// firstSeen returns the earliest phase whose snapshot holds name, and the
// configstring index it sits at.
func firstSeen(snaps map[string]map[int]string, name string) (string, int) {
	for _, ph := range phases {
		cs, ok := snaps[ph]
		if !ok {
			continue
		}
		idx := make([]int, 0, len(cs))
		for k := range cs {
			idx = append(idx, k)
		}
		sort.Ints(idx)
		for _, k := range idx {
			if playtest.Decode(cs[k]) == name {
				return ph, k
			}
		}
	}
	return "", 0
}

// imagesBase recovers CS_IMAGES from one image whose index the server has
// already told the client, so neither configstring layout is hardcoded.
func imagesBase(cs map[int]string, name string, index int) (int, error) {
	if index == 0 {
		return 0, fmt.Errorf("%s has no index in STAT_HELPICON -- cannot calibrate", name)
	}
	for k, v := range cs {
		if playtest.Decode(v) == name {
			return k - index, nil
		}
	}
	return 0, fmt.Errorf("%q is not in the configstrings at all", name)
}
