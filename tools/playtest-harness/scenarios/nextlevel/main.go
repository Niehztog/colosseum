// nextlevel -- does the level end when there is nobody to press a key?
//
// THE SUBJECT.  baseq2 ends an intermission one way: five seconds, then the
// first BUTTON_ANY from a connected client (p_client.c).  Tourney brought two
// timers with it -- `nextlevel_click`, how long a press is ignored for, and
// `nextlevel_default`, which ends the intermission with no press at all -- and
// R-RA-10 gives `arena` the SECOND of them, because a server whose clients are
// all bots has nobody who will ever press anything: the bot input path sets
// BUTTON_ATTACK and BUTTON_USE and never BUTTON_ANY (bl_main.c).  The press
// half stays tourney's; `arena` keeps RA2's own five seconds (R-RA-10a).  Every
// phase below is about the CLOCK, which is why none of them had to change when
// that scope narrowed.
//
// Measured on the live arena server before the fix: 23.8 hours of a 26.9-hour
// uptime frozen on an intermission scoreboard.  Eleven map loads in all of it,
// one a container restart and every other one within half a minute of a person
// being there to press a key -- none at all with nobody there.  The longest
// single stall was 6h04m and was still running when it was found.
//
// THREE PHASES.  The first two are two ways to have nobody, which fail
// differently; the third is a second ruleset reaching the first way.
//
//   A  arena / bots only.  Clients exist and think every frame; not one of
//      them will ever send BUTTON_ANY.  This is what the report hit.
//
//   B  dm (tourney's RegularDM) / the last client leaves DURING the
//      intermission.  Tourney already had the lazy timer, and BeginIntermission
//      already skips the intermission outright when no human is on the server
//      -- but that is decided once, at the level end, so a client who quits on
//      the scoreboard is not in it.  After they go there is no ClientThink at
//      all, so a timer living there cannot run: the fix is that it is a FRAME's
//      work now (G_CheckIntermissionExit), not a client's.  Without a client
//      the old code had no way to end the level either.
//
//   C  ctf / bots only.  The same shape as A in a ruleset that never had
//      either timer (R-CTF-9).  `ctf` takes the LAZY one only and keeps
//      baseq2's five-second press delay -- as `arena` does -- so this phase is
//      about the clock and says nothing about the press.
//
// WHY IT NEEDS A RUNNING SERVER.  The claim is that a map change happens with
// no input, which is a claim about a clock nothing is driving.  Reading the
// source can say the timer is wired; only running it can say something reaches
// it on a server with no clients thinking.
//
// Exit 0 every check passed, 1 a check failed, 2 the scenario could not run.
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
)

var (
	q2proded = flag.String("q2proded", "", "path to q2proded")
	ref      = flag.String("ref", "/usr/share/games/quake2/baseq2", "retail baseq2 paks")
	ctfref   = flag.String("ctf", "/usr/share/games/quake2/ctf", "threewave ctf paks")
	lib      = flag.String("lib", "", "game library under test")
	gladdir  = flag.String("gladdir", "", "gladiator-bot-restored, for the bot phase")
	aas      = flag.String("aas", "", "q2dm1.aas; searched for if unset")
	dir      = flag.String("dir", "/tmp/q2playtest/nextlevel", "scratch install dir")
	port0    = flag.Int("port", 27950, "first server port")

	failed, passed, skipped int
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

func skip(name, why string) {
	skipped++
	fmt.Printf("  [skip] %s -- %s\n", name, why)
}

func findAAS(explicit string) string {
	for _, c := range []string{
		explicit,
		os.Getenv("Q2AAS"),
		"/tmp/gladcompose/assets/maps/q2dm1.aas",
		os.Getenv("HOME") + "/q2-dev/yquake2/release_/baseq2/maps/q2dm1.aas",
	} {
		if c == "" {
			continue
		}
		if st, err := os.Stat(c); err == nil && !st.IsDir() {
			return c
		}
	}
	return ""
}

// boot installs a scratch server.  `timelimit 1` is the shortest the engine
// takes, and `nextlevel_default 5` shortens only the part under test -- the
// wait AFTER the level has ended -- so a pass is 5 seconds of evidence rather
// than the default 45 of waiting.
func boot(sub, ruleset, mesh string, port int, extra map[string]string) (*playtest.Server, error) {
	d := filepath.Join(*dir, sub)
	os.RemoveAll(d)
	if err := colosseum.Install(d, *ref, *ctfref, *lib); err != nil {
		return nil, err
	}
	if mesh != "" {
		if err := colosseum.InstallBrain(d, *gladdir); err != nil {
			return nil, err
		}
		src, err := os.ReadFile(mesh)
		if err != nil {
			return nil, err
		}
		if err := os.MkdirAll(d+"/colosseum/maps", 0o755); err != nil {
			return nil, err
		}
		if err := os.WriteFile(d+"/colosseum/maps/q2dm1.aas", src, 0o644); err != nil {
			return nil, err
		}
	}
	cv := map[string]string{
		"g_ruleset": ruleset, "skill": "1", "admincode": "0",
		"timelimit": "1", "fraglimit": "0",
		"nextlevel_default": "5", "nextlevel_click": "3",
		"botfill": "0", "minimumplayers": "0", "bots_minplayers": "0",
	}
	for k, v := range extra {
		cv[k] = v
	}
	srv := &playtest.Server{
		Binary: *q2proded, Dir: d, Game: "colosseum", Map: "q2dm1",
		Port: port, MaxClients: 8, LogPath: d + "/server.log",
		Cvars: cv,
	}
	if err := srv.Start(); err != nil {
		return nil, err
	}
	return srv, nil
}

// waitFrom blocks until a line matching re appears AFTER mark.
//
// Server.WaitLog scans the whole log before it subscribes, which for a MAP
// CHANGE answers the wrong question: `Map Loading` is in every log from the
// first second, so a wedged server passes on the load it did at boot.  That is
// not a hypothetical -- it is what this scenario did until the control run on
// the unfixed library came back green.
func waitFrom(srv *playtest.Server, mark int, re string, d time.Duration) bool {
	deadline := time.Now().Add(d)
	for time.Now().Before(deadline) {
		if len(srv.GrepFrom(mark, re)) > 0 {
			return true
		}
		time.Sleep(500 * time.Millisecond)
	}
	return false
}

// waitLevelEnd blocks until the server says the level is over.  Under `arena`
// that line is BeginIntermission's own client count; everywhere else it is the
// engine's timelimit announcement.
func waitLevelEnd(srv *playtest.Server, re string, d time.Duration) bool {
	_, err := srv.WaitLog(re, d)
	return err == nil
}

func phaseA(port int) {
	fmt.Println("\n== A. arena, bots only: nobody who can press anything ==")
	mesh := findAAS(*aas)
	if mesh == "" {
		skip("arena/the level ends with only bots on the server",
			"no q2dm1.aas -- pass -aas <file> or set $Q2AAS; without a mesh the "+
				"brain destroys every bot and the server is merely EMPTY, which "+
				"BeginIntermission already handled")
		return
	}
	srv, err := boot("arena", "arena", mesh, port, map[string]string{"botfill": "1"})
	if err != nil {
		skip("arena/the level ends with only bots on the server", err.Error())
		return
	}
	defer srv.Stop()

	// The bots have to be ON before the level ends, or this measures an empty
	// server -- which is the case that already worked.
	if _, err := srv.WaitLog(`entered the game`, 240*time.Second); err != nil {
		skip("arena/the level ends with only bots on the server",
			"no bot ever entered the game: "+err.Error())
		return
	}

	if !waitLevelEnd(srv, `clients on level change|Timelimit hit`, 150*time.Second) {
		check("arena/the level ends with only bots on the server", false,
			"the timelimit never arrived")
		return
	}
	mark := srv.Len()
	// `nextlevel_default 5`, plus a frame or two and the map load itself.
	ok := waitFrom(srv, mark, `SpawnServer`, 40*time.Second)
	check("arena/the map changes with nobody to press a key",
		ok,
		"no map load in the 40s after the level ended -- the server is wedged on "+
			"the intermission, which is the reported defect (log lines since: %d)",
		srv.Len()-mark)

	// ...and it must be the TIMER that did it, not the level being skipped.
	// BeginIntermission prints this line only when it moved somebody into an
	// intermission, so its presence says the intermission really happened.
	check("arena/...and it was an intermission, not a skipped level",
		len(srv.Grep(`clients on level change`)) > 0,
		"no intermission was entered at all, so the timer is not what this "+
			"measured")
}

func phaseB(port int) {
	fmt.Println("\n== B. dm, last client quits ON the scoreboard ==")
	srv, err := boot("dm", "dm", "", port, nil)
	if err != nil {
		skip("dm/the level ends after the last client quits in the intermission",
			err.Error())
		return
	}
	defer srv.Stop()

	person := playtest.NewBot("quitter", "127.0.0.1", port)
	if err := person.Start(60 * time.Second); err != nil {
		skip("dm/the level ends after the last client quits in the intermission",
			err.Error())
		return
	}

	if !waitLevelEnd(srv, `Timelimit hit`, 150*time.Second) {
		check("dm/the level ends after the last client quits in the intermission",
			false, "the timelimit never arrived")
		person.Disconnect()
		return
	}
	// Out at once, while the scoreboard is still up.  BeginIntermission has
	// already run and already decided there was somebody here, so its own
	// empty-server arm cannot help from this point on.
	person.Disconnect()
	mark := srv.Len()

	ok := waitFrom(srv, mark, `SpawnServer`, 40*time.Second)
	check("dm/the level ends after the last client quits in the intermission",
		ok,
		"no map load in the 40s after the only client left the intermission: "+
			"with no client there is no ClientThink, so a timer that lives there "+
			"never runs (log lines since: %d)", srv.Len()-mark)
}

// phaseC is A's claim under `ctf`.  Separate rather than a loop over two
// rulesets, because the two are not the same requirement -- R-RA-10 gave
// `arena` tourney's whole pair and R-CTF-9 gave `ctf` only the clock -- and a
// parameterised phase would hide that.
func phaseC(port int) {
	fmt.Println("\n== C. ctf, bots only: the same hole, one ruleset over ==")
	mesh := findAAS(*aas)
	if mesh == "" {
		skip("ctf/the level ends with only bots on the server",
			"no q2dm1.aas -- pass -aas <file> or set $Q2AAS")
		return
	}
	// q2dm1 under `ctf`: the bots need somewhere to stand and the fill needs a
	// target, and the map's spawn pool is what gives it one.  `capturelimit 0`
	// so that nothing but the timelimit can end the level -- a capture would
	// end it too, and this phase has to know which limit it measured.
	srv, err := boot("ctf", "ctf", mesh, port, map[string]string{
		"botfill": "1", "capturelimit": "0",
	})
	if err != nil {
		skip("ctf/the level ends with only bots on the server", err.Error())
		return
	}
	defer srv.Stop()

	if _, err := srv.WaitLog(`entered the game`, 240*time.Second); err != nil {
		skip("ctf/the level ends with only bots on the server",
			"no bot ever entered the game: "+err.Error())
		return
	}
	if !waitLevelEnd(srv, `Timelimit hit`, 150*time.Second) {
		check("ctf/the level ends with only bots on the server", false,
			"the timelimit never arrived")
		return
	}
	mark := srv.Len()
	ok := waitFrom(srv, mark, `SpawnServer`, 40*time.Second)
	check("ctf/the map changes with nobody to press a key",
		ok,
		"no map load in the 40s after the level ended -- a bot-filled ctf "+
			"server is wedged on the intermission (log lines since: %d)",
		srv.Len()-mark)
}

func main() {
	flag.Parse()
	if *q2proded == "" || *lib == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded and -lib")
		os.Exit(2)
	}
	if err := os.MkdirAll(*dir, 0o755); err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}

	phaseA(*port0)
	phaseB(*port0 + 1)
	phaseC(*port0 + 2)

	fmt.Printf("\n%d check(s) passed, %d failed, %d skipped\n", passed, failed, skipped)
	if failed > 0 {
		os.Exit(1)
	}
	os.Exit(0)
}
