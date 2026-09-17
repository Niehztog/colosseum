// ra2botkeep -- does the fill take apart a game it has no reason to touch?
//
// THE SUBJECT.  `botfill` under `arena` sizes each arena to the people in it,
// and an arena with nobody in it wants none -- which is how the arena the last
// person walked out of gives its bots back to be rebuilt where the people are.
// That rule reads a BOTS-ONLY arena the same way, because "nobody" in it means
// no human, and there the rule has no destination to justify it: the bots are
// not migrating anywhere, they are being evicted from a game they were playing.
//
// Reported from the live arena server on ra2map27, and reproduced here:
//
//   * ten bots were playing arena 8, the map's one pickup arena;
//   * one person joined arena 1, a ppt=1 arena, whose target is two;
//   * 32 frames apart, the removal arm took every bot in arena 8 -- nine of
//     them -- while arena 1, already at its target, asked for nothing;
//   * arena 8 spent the rest of the map printing `It was a tie!` with nobody
//     in it to fight.
//
// ...and then the second half, which is why the map never recovered.  One bot
// had followed the person into arena 1.  When they left, `RA_StagingArena`
// picked the LOWEST-numbered arena holding a bot -- arena 1, holding that
// stray -- so the staging target became the ppt=1 arena's two, and a server
// that had been running a ten-bot pickup game settled at two bots for the rest
// of the map.  Measured live: 2 bots, ppt=1, for over six hours.
//
// WHY IT NEEDS A RUNNING SERVER.  Both claims are about a number that is
// COMPUTED every 32 frames from the entity list -- `RA_BotFillTarget` against
// `RA_ArenaPlayers` -- and `sv arenadump`'s `want=`/`here=` pair is the only
// place either is written down.  Neither can be read off the source, and a
// count taken once cannot tell a server that settled from one still draining.
//
// THE MAP.  ra2map27, the one the report came from.  Arenas 1..7 are ppt=1
// (they name no `playersperteam`, so they inherit the default) and arena 8 is
// `pickup: 1` over ten spawn points.  The two targets therefore differ -- two
// against ten -- which is the whole mechanism: a small arena that a visitor
// fills completely, beside a big one that the fill then has no reason to feed.
//
// Exit 0 every check passed, 1 a check failed, 2 the scenario could not run.
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/packetflinger/libq2/pak"

	"q2playtest/colosseum"
	"q2playtest/playtest"
	"q2playtest/ra2"
)

var (
	q2proded = flag.String("q2proded", "", "path to q2proded")
	ref      = flag.String("ref", "/usr/share/games/quake2/baseq2", "retail baseq2 paks")
	ra2ref   = flag.String("ra2ref", os.Getenv("HOME")+"/q2-dev/yquake2/release_/arena",
		"RA2 install (pak0..2 and arena.cfg)")
	lib     = flag.String("lib", "", "game library under test")
	gladdir = flag.String("gladdir", "", "gladiator-bot-restored, for the brain")
	aas     = flag.String("aas", "", "ra2map27.aas; searched for if unset")
	dir     = flag.String("dir", "/tmp/q2playtest/ra2botkeep", "scratch install dir")
	port    = flag.Int("port", 27960, "server port")

	// The map's pickup arena and the ppt=1 arena a visitor lands in.  Named
	// rather than discovered so that a failure reads as "arena 8 lost its
	// bots" and not as a search that came back empty.
	bigArena   = 8
	smallArena = 1

	reWantHere = regexp.MustCompile(`want=(\d+) here=(\d+)`)

	failed, passed int
)

func check(name string, ok bool, format string, a ...any) {
	if ok {
		passed++
		fmt.Printf("  [ok  ] %s\n", name)
		return
	}
	failed++
	fmt.Printf("  [FAIL] %s -- %s\n", name, fmt.Sprintf(format, a...))
}

// dump asks the server for a fresh arenadump and reads one arena's pair out of
// it.  Fresh every time: these are the numbers under test, so a cached line
// would be the scenario measuring its own history.
func dump(srv *playtest.Server, arena int) (want, here int) {
	mark := srv.Len()
	srv.Console("sv arenadump")
	deadline := time.Now().Add(10 * time.Second)
	pat := fmt.Sprintf(`arena %-2d .*here=`, arena)
	for time.Now().Before(deadline) {
		if ls := srv.GrepFrom(mark, pat); len(ls) > 0 {
			m := reWantHere.FindStringSubmatch(ls[len(ls)-1])
			if m != nil {
				want, _ = strconv.Atoi(m[1])
				here, _ = strconv.Atoi(m[2])
				return want, here
			}
		}
		time.Sleep(300 * time.Millisecond)
	}
	return -1, -1
}

// settle polls one arena until its head count stops moving, and reports the
// last value with the lowest and highest seen on the way.  The extremes are
// the point: the defect is a DRAIN, and an arena that was emptied and refilled
// between two samples looks identical to one that was never touched.
func settle(srv *playtest.Server, arena int, quiet int, limit time.Duration) (last, lo, hi int) {
	lo, hi, last = 1<<30, -1, -1
	same := 0
	deadline := time.Now().Add(limit)
	for time.Now().Before(deadline) {
		_, here := dump(srv, arena)
		if here < 0 {
			continue
		}
		if here == last {
			same++
		} else {
			same = 0
		}
		last = here
		if here < lo {
			lo = here
		}
		if here > hi {
			hi = here
		}
		if same >= quiet {
			break
		}
		time.Sleep(3 * time.Second)
	}
	return last, lo, hi
}

// findAAS locates a reachability file for the map.  Without one the brain
// answers "no AAS file available" and destroys every bot that wanted it, so a
// fill measured there is measuring the mesh.
func findAAS(explicit string) string {
	cands := []string{
		explicit,
		os.Getenv("RA2AAS"),
		"/tmp/gladcompose/assets/maps/ra2map27.aas",
		os.Getenv("HOME") + "/q2-dev/yquake2/release_/arena/maps/ra2map27.aas",
	}
	for _, c := range cands {
		if c == "" {
			continue
		}
		if st, err := os.Stat(c); err == nil && !st.IsDir() {
			return c
		}
		if ms, _ := filepath.Glob(c); len(ms) > 0 {
			return ms[0]
		}
	}
	return ""
}

// unpackBSP stages one map's .bsp on disk, out of whichever RA2 pak holds it.
//
// The ENGINE is happy to read it from inside the pak; the BRAIN is not.  The
// botlib does its own file I/O and Colosseum hands it the engine's filesystem
// only when the engine exports one -- `filesystem extension ABSENT` in the
// console otherwise, after which its reads fall back to stdio under
// basedir/gamedir and nothing inside a pak is visible to it.  So a bot on a
// pakked map answers `couldn't find the bsp file`, shuts the AAS down, and the
// fill has nobody to seat.  The deployed server has these extracted already;
// a scratch install has to do it here or measure an empty server.
func unpackBSP(gamedir, mapname string) error {
	paks, _ := filepath.Glob(filepath.Join(*ra2ref, "pak*.pak"))
	for _, p := range paks {
		data, err := os.ReadFile(p)
		if err != nil {
			continue
		}
		archive, err := pak.Unmarshal(data)
		if err != nil {
			continue
		}
		for _, f := range archive.GetFiles() {
			if !strings.EqualFold(filepath.Base(f.GetName()), mapname+".bsp") {
				continue
			}
			maps := filepath.Join(gamedir, "maps")
			if err := os.MkdirAll(maps, 0o755); err != nil {
				return err
			}
			return os.WriteFile(filepath.Join(maps, mapname+".bsp"),
				f.GetData(), 0o644)
		}
	}
	return fmt.Errorf("%s.bsp is in none of %s/pak*.pak", mapname, *ra2ref)
}

func boot(mesh string) (*playtest.Server, error) {
	os.RemoveAll(*dir)
	// The RA2 paks ride in on the `ctf` argument, which links every pak*.pak it
	// is given above baseq2 -- which is where the ra2map*.bsp have to be.
	if err := colosseum.Install(*dir, *ref, *ra2ref, *lib); err != nil {
		return nil, err
	}
	if err := colosseum.InstallBrain(*dir, *gladdir); err != nil {
		return nil, err
	}
	// The authentic 1999 arena.cfg, unedited: this scenario is about the fill,
	// and both the pickup arena and the ppt=1 ones are already in it.
	cfg, err := os.ReadFile(*ra2ref + "/arena.cfg")
	if err != nil {
		return nil, fmt.Errorf("arena.cfg: %w", err)
	}
	os.Remove(*dir + "/colosseum/arena.cfg")
	if err := os.WriteFile(*dir+"/colosseum/arena.cfg", cfg, 0o644); err != nil {
		return nil, err
	}
	src, err := os.ReadFile(mesh)
	if err != nil {
		return nil, fmt.Errorf("mesh %s: %w", mesh, err)
	}
	if err := os.MkdirAll(*dir+"/colosseum/maps", 0o755); err != nil {
		return nil, err
	}
	if err := os.WriteFile(*dir+"/colosseum/maps/ra2map27.aas", src, 0o644); err != nil {
		return nil, err
	}
	if err := unpackBSP(*dir+"/colosseum", "ra2map27"); err != nil {
		return nil, err
	}

	srv := &playtest.Server{
		Binary: *q2proded, Dir: *dir, Game: "colosseum", Map: "ra2map27",
		// Tight on purpose.  Every bot is a library load and this run has to
		// fill an arena, let a person come and go, and watch it settle twice;
		// the claim is made with a handful of bots as well as with ten.
		Port: *port, MaxClients: 8,
		Cvars: map[string]string{
			"g_ruleset": "arena", "cheats": "1", "skill": "1",
			// Both flat-count names are zeroed, so that a fill target arriving
			// from either of them would be no evidence.
			"botfill": "1", "minimumplayers": "0", "bots_minplayers": "0",
		},
		LogPath: *dir + "/server.log",
	}
	if err := srv.Start(); err != nil {
		return nil, err
	}
	return srv, nil
}

func run() (int, error) {
	mesh := findAAS(*aas)
	if mesh == "" {
		fmt.Println("  [skip] no ra2map27.aas -- pass -aas <file> or set $RA2AAS.")
		return 2, nil
	}
	if _, err := os.Stat(*ra2ref + "/arena.cfg"); err != nil {
		fmt.Println("  [skip] no RA2 install -- pass -ra2ref <dir>.")
		return 2, nil
	}

	srv, err := boot(mesh)
	if err != nil {
		return 2, err
	}
	defer srv.Stop()

	// The person connects BEFORE the fill runs and sits in the lobby.  A
	// client that has picked no team is in no arena -- RA_ArenaPlayers counts
	// team members -- so it is invisible to both the census and the fill, and
	// the staging arena still reads as the only one with anybody in it.  What
	// it does do is hold a slot: with `maxclients` this tight the fill takes
	// every free one within a tick or two, and a person arriving afterwards is
	// refused by a full server.
	person := playtest.NewBot("visitor", "127.0.0.1", *port)
	if err := person.Start(90 * time.Second); err != nil {
		return 2, fmt.Errorf("visitor: %w", err)
	}

	fmt.Println("\n== 1. nobody playing: the fill stages the bots in the pickup arena ==")
	bigWant, _ := dump(srv, bigArena)
	big0, _, _ := settle(srv, bigArena, 2, 300*time.Second)
	small0, _, _ := settle(srv, smallArena, 1, 30*time.Second)
	check("the pickup arena is the staging arena and it filled",
		big0 >= 2,
		"arena %d here=%d want=%d -- the fill seated nobody, so nothing below measures the fill",
		bigArena, big0, bigWant)
	check("...and the ppt=1 arena is empty",
		small0 == 0,
		"arena %d here=%d, want 0", smallArena, small0)
	if big0 < 2 {
		return 1, nil
	}

	fmt.Printf("\n== 2. one person joins arena %d, the ppt=1 one ==\n", smallArena)
	// Picked off the live menu rather than by name: the row carries the
	// arena's name out of the map, and "(PT)" is what marks the pickup one.
	// The first row without it is arena 1, which is the arena under test.
	if err := ra2.DismissMOTD(person); err != nil {
		return 2, fmt.Errorf("motd: %w", err)
	}
	if err := person.WaitMenu(`(?i)choose your team`, 30*time.Second); err != nil {
		return 2, fmt.Errorf("team menu: %w", err)
	}
	if err := person.MenuPick(`Start New Team`, 30*time.Second); err != nil {
		return 2, fmt.Errorf("new team: %w", err)
	}
	if err := person.WaitMenu(`(?i)choose your arena`, 30*time.Second); err != nil {
		return 2, fmt.Errorf("arena menu: %w", err)
	}
	_, rows := person.Menu()
	target := ""
	for _, it := range rows {
		t := strings.TrimSpace(it.Text)
		if t == "" || strings.Contains(t, "(PT)") || strings.EqualFold(t, "Leave Team") {
			continue
		}
		target = t
		break
	}
	if target == "" {
		return 2, fmt.Errorf("no non-pickup arena row in the arena menu")
	}
	if err := person.MenuPick(`(?i)^`+regexp.QuoteMeta(target), 30*time.Second); err != nil {
		return 2, fmt.Errorf("join %q: %w", target, err)
	}
	person.WaitFrames(20, 10*time.Second)

	// The window that matters.  A bot may legitimately MIGRATE into the
	// person's arena -- that arena is short by one and giving somebody an
	// opponent is what the fill is for -- so one bot leaving the pickup arena
	// is the feature.  What is under test is the other eight.
	bigAfter, bigLow, _ := settle(srv, bigArena, 3, 240*time.Second)
	smallAfter, _, _ := settle(srv, smallArena, 2, 60*time.Second)
	fmt.Printf("  (arena %d: %d -> %d, low %d; arena %d: %d)\n",
		bigArena, big0, bigAfter, bigLow, smallArena, smallAfter)
	check("the pickup arena keeps its game when somebody joins another arena",
		bigLow >= big0-1,
		"arena %d fell from %d to %d (settled %d): a person in arena %d cost it "+
			"more than the one bot that could have gone to them",
		bigArena, big0, bigLow, bigAfter, smallArena)
	check("...and the person is not left alone in theirs",
		smallAfter >= 2,
		"arena %d here=%d -- the person has no opponent", smallArena, smallAfter)

	fmt.Println("\n== 3. ...and leaves again ==")
	person.Disconnect()
	time.Sleep(5 * time.Second)

	bigEnd, _, bigHigh := settle(srv, bigArena, 3, 300*time.Second)
	smallEnd, _, _ := settle(srv, smallArena, 2, 60*time.Second)
	fmt.Printf("  (arena %d: %d, high %d; arena %d: %d)\n",
		bigArena, bigEnd, bigHigh, smallArena, smallEnd)
	// The ratchet.  With the staging arena chosen by lowest number, the stray
	// left behind in the ppt=1 arena becomes the whole server: its target is
	// two, the pickup arena's target drops to zero, and the map never runs a
	// full game again.
	check("the bots go back to the game, not to the arena the visitor left behind",
		bigEnd >= big0-1,
		"arena %d settled at %d against %d before the visit -- the server "+
			"re-formed around the stray in arena %d instead",
		bigArena, bigEnd, big0, smallArena)
	check("...and nothing is left stranded in the ppt=1 arena",
		smallEnd == 0,
		"arena %d still holds %d, with nobody in it to play", smallArena, smallEnd)

	if failed > 0 {
		return 1, nil
	}
	return 0, nil
}

func main() {
	flag.Parse()
	if *q2proded == "" || *lib == "" || *gladdir == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded, -lib and -gladdir")
		os.Exit(2)
	}
	code, err := run()
	if err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	fmt.Printf("\n%d check(s) passed, %d failed\n", passed, failed)
	os.Exit(code)
}
