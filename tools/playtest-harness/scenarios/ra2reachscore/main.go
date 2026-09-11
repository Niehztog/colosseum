// ra2reachscore -- the scoreboard, the map change, and the twenty seconds in
// which a bot is a client that has never begun.
//
// THE WINDOW.  A .aas file written by bspc carries no REACHABILITY and no
// CLUSTERS lump, so the brain computes both at map load, `framereachability` at
// a time, over hundreds of frames.  BotStarted() holds a bot's ClientBegin back
// until the brain reports initialised, so for the length of that build the
// server holds clients that are `inuse`, have a gclient_t, and have not been
// through ClientBegin.
//
// THE MAP CHANGE is what makes the window dangerous.  `client_respawn_t`
// survives a level change and `resp.teamnum` is in it; `teams[]` does not --
// arena_init() TagMallocs a fresh one and only re-creates the pickup teams.  So
// on the second map a bot carries an index into an array whose slot is now
// NULL, and it carries it until its deferred ClientBegin runs InitClientResp.
//
// Anything that walks the client list during that window meets those bots.  The
// scoreboard walks the client list.
//
// Two phases, and the second is the one that matters:
//
//	same    one map, bots added, `score` pressed across the build
//	change  bots seated on map one, then map two with a stripped .aas, and a
//	        fresh client pressing `score` across ITS build
package main

import (
	"crypto/md5"
	"encoding/binary"
	"encoding/hex"
	"flag"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"
	"time"

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
	fmt.Printf("  [%s] %-26s %s\n", tag, name, detail)
}

func link(src, dst string) error {
	abs, err := filepath.Abs(src)
	if err != nil {
		return err
	}
	os.Remove(dst)
	return os.Symlink(abs, dst)
}

// writeAAS copies an .aas, optionally emptying its REACHABILITY (9) and
// CLUSTERS (13) lumps -- which is the shape bspc writes and the brain
// recomputes at load.
func writeAAS(src, dst string, strip bool) error {
	b, err := os.ReadFile(src)
	if err != nil {
		return err
	}
	if len(b) < 128 || string(b[:4]) != "EAAS" {
		return fmt.Errorf("%s is not an EAAS file", src)
	}
	out := append([]byte(nil), b...)
	if strip {
		for _, lump := range []int{9, 13} {
			binary.LittleEndian.PutUint32(out[8+lump*8+4:], 0)
		}
	}
	return os.WriteFile(dst, out, 0o644)
}

// botlibPaks is how many paks the BRAIN looks at, which is not how many the
// engine looks at: the botlib does its own file search, and `l_utils.c` builds
// "pak%d.pak" for 0..9 and stops.
const botlibPaks = 10

func pakDigest(path string) (int64, string, error) {
	f, err := os.Open(path)
	if err != nil {
		return 0, "", err
	}
	defer f.Close()
	st, err := f.Stat()
	if err != nil {
		return 0, "", err
	}
	h := md5.New()
	if _, err := io.Copy(h, f); err != nil {
		return 0, "", err
	}
	return st.Size(), hex.EncodeToString(h.Sum(nil)), nil
}

// brainCanSeeItsPak fails when the brain's asset pak is installed where the
// brain cannot reach it.
//
// install() numbers the paks it links and puts the brain's last, so the index
// depends on how many the reference install contributed -- and one -ref tree
// with the mod's paks already folded in is enough to push it to pak10, which
// the engine reads and the botlib never looks at.  What that costs is three
// lines of console naming neither the pak nor the limit:
//
//	Error: couldn't find weapons.c
//	Fatal: couldn't load the weapon config
//	gladiator.so not available
//
// The brain loaded, could not read its own configuration, unloaded itself, and
// the scenario then timed out on bots that were never going to exist.  The
// match is by CONTENT, because the file name is the thing that went wrong;
// size is compared first only because the retail paks dwarf this one.
func brainCanSeeItsPak(game, brainPak string) error {
	wantSize, wantSum, err := pakDigest(brainPak)
	if err != nil {
		return fmt.Errorf("cannot read the brain's asset pak %s: %w", brainPak, err)
	}
	for i := 0; i < botlibPaks; i++ {
		p := filepath.Join(game, fmt.Sprintf("pak%d.pak", i))
		if st, err := os.Stat(p); err != nil || st.Size() != wantSize {
			continue
		}
		if _, sum, err := pakDigest(p); err == nil && sum == wantSum {
			return nil
		}
	}
	where := "nowhere in the gamedir"
	paks, _ := filepath.Glob(filepath.Join(game, "pak*.pak"))
	for _, p := range paks {
		if st, err := os.Stat(p); err != nil || st.Size() != wantSize {
			continue
		}
		if _, sum, err := pakDigest(p); err == nil && sum == wantSum {
			where = filepath.Base(p) + ", which the engine reads and the brain does not"
			break
		}
	}
	return fmt.Errorf("the brain's asset pak is out of the brain's own reach: it is %s, "+
		"and the botlib searches pak0.pak..pak%d.pak only -- install fewer paks ahead "+
		"of it, which usually means a -ref tree that already carries the mod's paks "+
		"and counts them twice against -ra2ref", where, botlibPaks-1)
}

func install(dir, baseq2, ra2dir, glad, lib string) (string, error) {
	game := filepath.Join(dir, "colosseum")
	for _, d := range []string{game, filepath.Join(dir, "baseq2"),
		filepath.Join(game, "maps"), filepath.Join(game, "botcfg")} {
		if err := os.MkdirAll(d, 0o755); err != nil {
			return game, err
		}
	}
	n := 0
	add := func(pattern string) error {
		paks, _ := filepath.Glob(pattern)
		for _, p := range paks {
			if err := link(p, filepath.Join(game, fmt.Sprintf("pak%d.pak", n))); err != nil {
				return err
			}
			n++
		}
		return nil
	}
	for _, p := range []string{
		filepath.Join(baseq2, "pak*.pak"),
		filepath.Join(ra2dir, "pak*.pak"),
		filepath.Join(glad, "assets", "pak7.pak"),
	} {
		if err := add(p); err != nil {
			return game, err
		}
	}
	for _, f := range []string{"arena.cfg", "motd.txt"} {
		src, err := os.ReadFile(filepath.Join(ra2dir, f))
		if err != nil {
			continue
		}
		if err := os.WriteFile(filepath.Join(game, f), src, 0o644); err != nil {
			return game, err
		}
	}
	cfg, err := os.ReadFile(filepath.Join(glad, "assets", "bots.cfg"))
	if err != nil {
		return game, err
	}
	if err := os.WriteFile(filepath.Join(game, "botcfg", "bots.cfg"), cfg, 0o644); err != nil {
		return game, err
	}
	if err := link(filepath.Join(glad, "release", "gladiator.so"),
		filepath.Join(game, "gladiator.so")); err != nil {
		return game, err
	}
	if err := brainCanSeeItsPak(game, filepath.Join(glad, "assets", "pak7.pak")); err != nil {
		return game, err
	}
	// The engine looks for `game<cpu>` and nothing else, and the caller has
	// already built that name -- so install it under the one it arrived with
	// rather than under this machine's.
	return game, link(lib, filepath.Join(game, filepath.Base(lib)))
}

// waitFrom is WaitLog restricted to lines that arrived after `mark`.  The
// second map replays every line the first one printed -- "AAS initialized"
// included -- so a whole-log wait answers with the PREVIOUS map's build.
func waitFrom(srv *playtest.Server, mark int, re string, timeout time.Duration) error {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if len(srv.GrepFrom(mark, re)) > 0 {
			return nil
		}
		time.Sleep(100 * time.Millisecond)
	}
	return fmt.Errorf("timed out waiting for console line %q", re)
}

// hammer presses `score` until the brain finishes, and reports how many times.
func hammer(b *playtest.Bot, srv *playtest.Server, mark int, limit time.Duration) (int, bool) {
	done := make(chan error, 1)
	go func() { done <- waitFrom(srv, mark, `AAS initialized`, limit) }()
	presses := 0
	deadline := time.Now().Add(limit)
	for time.Now().Before(deadline) {
		select {
		case err := <-done:
			return presses, err == nil
		default:
		}
		b.Cmd("score")
		presses++
		time.Sleep(50 * time.Millisecond)
	}
	return presses, false
}

// responsive is the witness that the server is still there: a dead q2proded
// answers no console command.
func responsive(srv *playtest.Server) bool {
	srv.Console("sv ruleset")
	_, err := srv.WaitLog(`^world `, 15*time.Second)
	return err == nil
}

func main() {
	q2 := flag.String("q2proded", "", "")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "")
	ra2dir := flag.String("ra2ref", os.Getenv("HOME")+"/q2-dev/yquake2/release_/arena", "")
	glad := flag.String("gladdir", os.Getenv("HOME")+"/q2-dev/gladiator-bot-restored", "")
	lib := flag.String("lib", "", "")
	aas := flag.String("aas", "", "a COMPLETE .aas; map two gets a copy with its reachability lump emptied")
	map1 := flag.String("map1", "ra2map11", "")
	map2 := flag.String("map2", "ra2map12", "")
	dir := flag.String("dir", "/tmp/q2playtest/ra2reachscore", "")
	port := flag.Int("port", 27975, "")
	bots := flag.Int("bots", 6, "")
	flag.Parse()

	game, err := install(*dir, *ref, *ra2dir, *glad, *lib)
	if err != nil {
		check("install", false, err.Error())
		os.Exit(2)
	}
	// map one starts FAST (its .aas is complete) so the bots reach their teams;
	// map two starts SLOW, which is the window under test.
	if err := writeAAS(*aas, filepath.Join(game, "maps", *map1+".aas"), false); err != nil {
		check("install", false, err.Error())
		os.Exit(2)
	}
	if err := writeAAS(*aas, filepath.Join(game, "maps", *map2+".aas"), true); err != nil {
		check("install", false, err.Error())
		os.Exit(2)
	}

	srv := &playtest.Server{
		Binary: *q2, Dir: *dir, Game: "colosseum", Map: *map1,
		Port: *port, MaxClients: 16,
		Cvars: map[string]string{
			"g_ruleset": "arena", "deathmatch": "1", "coop": "0",
			"cheats": "1", "botfill": "0", "minimumplayers": "0",
		},
		LogPath: *dir + "/server.log",
	}
	if err := srv.Start(); err != nil {
		check("server", false, err.Error())
		os.Exit(2)
	}
	defer srv.Stop()

	// ---------------------------------------------------------------- phase 1
	b := playtest.NewBot("watcher", "127.0.0.1", *port)
	if err := b.Start(30 * time.Second); err != nil {
		check("connect", false, err.Error())
		os.Exit(2)
	}
	if err := ra2.NewTeamInArena(b, "."); err != nil {
		check("join", false, err.Error())
	}
	b.WaitFrames(10, 5*time.Second)

	mark1 := srv.Len()
	srv.Console("sv addrandom %d", *bots)
	// map one's .aas is complete, so there is no build to wait on; wait for the
	// bots to land on teams instead, which is the state phase 2 needs them in.
	if err := waitFrom(srv, mark1, `(has been added to team|has created team number)`, 90*time.Second); err != nil {
		check("seeded", false, "no bot ever joined a team on "+*map1)
	} else {
		time.Sleep(6 * time.Second)
		teams := len(srv.GrepFrom(mark1, `(has been added to team|has created team number)`))
		check("seeded", teams > 0,
			fmt.Sprintf("%d bot team joins on %s", teams, *map1))
	}
	b.Disconnect()

	// ---------------------------------------------------------------- phase 2
	mark := srv.Len()
	// `gamemap`, not `map`: q2pro refuses `map` on a running server ("will cause
	// full server restart"), and a restart is not the case under test -- the
	// window needs clients that CARRY resp across the level change, which is
	// what a rotation does.
	srv.Console("gamemap %s", *map2)
	if err := waitFrom(srv, mark, `calculating reachability`, 60*time.Second); err != nil {
		check("window", false, "no reachability build on "+*map2+": "+err.Error())
		fmt.Printf("\n%d check(s) failed\n", fails)
		os.Exit(1)
	}
	check("window", true, *map2+" is building reachabilities")

	// A client that arrives DURING the build.  Its own ClientBegin runs at once
	// -- only bots are deferred -- so it is an arena-0 observer, and `score`
	// there is the server-wide board, which walks every client on the server.
	c := playtest.NewBot("presser", "127.0.0.1", *port)
	if err := c.Start(60 * time.Second); err != nil {
		check("connect/2", false, err.Error())
		if !responsive(srv) {
			check("responsive", false, "server gone before the board was opened")
		}
		fmt.Printf("\n%d check(s) failed\n", fails)
		os.Exit(1)
	}
	check("connect/2", true, "a client joined during the build")

	presses, finished := hammer(c, srv, mark, 240*time.Second)
	fmt.Printf("  ---- %d score presses, build finished=%v ----\n", presses, finished)

	if !responsive(srv) {
		check("survives", false,
			fmt.Sprintf("server stopped answering after %d score presses during %s's build",
				presses, *map2))
	} else {
		check("survives", true, "server answered `sv ruleset` after the build")
	}

	// ...and the fix must not stop the bots joining.  arena_init() clears
	// `resp.teamnum` for every client, which is only safe because ClientBegin
	// resets it anyway; if that reasoning were wrong the bots would arrive on
	// map two with no team and never get one.
	seated := len(srv.GrepFrom(mark, `(has been added to team|has created team number)`))
	check("bots reseat", seated > 0,
		fmt.Sprintf("%d bot team joins on %s after the build", seated, *map2))

	log := strings.Join(srv.Log()[min(mark, len(srv.Log())):], "\n")
	for _, bad := range []string{"SIGSEGV", "Segmentation", "ERROR:", "FATAL"} {
		if strings.Contains(log, bad) {
			check("console clean", false, "server log contains "+bad)
		}
	}

	fmt.Printf("\n%d check(s) failed\n", fails)
	if fails > 0 {
		os.Exit(1)
	}
}

func min(a, b int) int {
	if a < b {
		return a
	}
	return b
}
