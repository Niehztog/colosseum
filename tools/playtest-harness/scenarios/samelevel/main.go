// samelevel -- does `dmflags` "same map" outrank the ruleset's own rotation?
//
// THE SUBJECT.  Two things want to choose the next map when a level ends, and
// both donors put them in a fixed order.  `port_ra2:g_main.c`'s `EndDMLevel`:
//
//     if ((int)dmflags->value & DF_SAME_LEVEL) {
//         BeginIntermission(CreateTargetChangeLevel(level.mapname));
//         return;
//     }
//     n = get_next_map(level.mapname);      // maploop.c, arena.cfg's `maploop:`
//
// and `port_osp:g_main.c`'s, with one extra clause -- `manual_map != 1`, so a
// map somebody typed is not overridden by a flag -- ahead of `NextMap()`.
//
// Colosseum hoists each rotation into its ruleset's `EndLevel` dispatch row, so
// that no two rotations can ever both run (R-OSP-9's double-rotation case).
// That puts the flag and the rotation in two different functions, and the order
// between them becomes the row's to keep: `RA_EndLevel` and `OSP_EndLevel` ask
// the flag and skip their loop on it, leaving `EndDMLevel()` to give the answer
// (R-RA-12).  `tools/dispatch.py` question E is the source-level check on that
// shape; this is the behavioural one.
//
// FOUR ARMS, TWO PER RULESET, AND BOTH ARE LOAD-BEARING.
//
//   control, `dmflags 0`.  The level end follows the rotation to the map the
//   rotation names.  Without it, "the map did not change" is equally true of a
//   server whose rotation was never read at all -- a config file in the wrong
//   gamedir, a misspelt key -- and the subject would pass for the wrong reason.
//
//   subject, `dmflags 32`.  Same server, same rotation, one bit different: the
//   level end comes back to the map it started on.
//
// The rotation is `q2dm1 -> q2dm3` ON PURPOSE.  q2dm1 ships a
// `target_changelevel` to q2dm2, which is what `EndDMLevel` falls through to
// when nothing else chooses -- so q2dm2 is the "nobody decided" answer and no
// arm may land on it.  q2dm3 can only come from the rotation and q2dm1 can only
// come from the flag.
//
// NO CLIENTS, deliberately.  Both rulesets end an intermission at once when it
// moved nobody into it, so an empty server changes map on the frame after the
// limit trips and no arm waits out a timer that `scenarios/nextlevel` already
// checks.  It also keeps the brain out of a scenario that has nothing to do
// with bots.
//
// Exit 0 every check passed, 1 a check failed, 2 the scenario could not run.
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
)

var (
	q2proded = flag.String("q2proded", "", "path to q2proded")
	ref      = flag.String("ref", "/usr/share/games/quake2/baseq2", "retail baseq2 paks")
	lib      = flag.String("lib", "", "game library under test")
	dir      = flag.String("dir", "/tmp/q2playtest/samelevel", "scratch install dir")
	port0    = flag.Int("port", 27970, "first server port")

	failed, passed int
)

// The rotation the scenario writes, and the three maps it tells apart.
const (
	start  = "q2dm1" // where every arm begins
	looped = "q2dm3" // only the rotation names this
	fallth = "q2dm2" // only q2dm1's own target_changelevel names this
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

func die(format string, a ...any) {
	fmt.Fprintf(os.Stderr, "ERROR: "+format+"\n", a...)
	os.Exit(2)
}

// A ruleset, its rotation, and how to tell the server read it.
type kind struct {
	label   string
	ruleset string
	// rotation is the file the ruleset's own map list lives in, relative to
	// the gamedir, and its contents.  Named per ruleset because that IS the
	// difference between the two: `arena` reads arena.cfg's `maploop:` key,
	// tourney reads one map per line out of `map_file`.
	file, body string
	// read is what the server prints once it has the list.  An arm that
	// cannot see this line is measuring a server with no rotation.
	read string
	// extra cvars this ruleset's rotation needs to be deterministic.
	cvars map[string]string
}

var kinds = []kind{
	{
		label: "arena", ruleset: "arena",
		file: "arena.cfg", body: "maploop: " + start + " " + looped + ";\n",
		read: `Map loop read`,
		// Every other per-arena setting stays at its built-in default, which
		// is what a `find_key` miss gives: the file has to exist and parse, it
		// does not have to be RA2's.
		cvars: map[string]string{},
	},
	{
		label: "tourney", ruleset: "dm",
		file: "maps.txt", body: start + "\n" + looped + "\n",
		read: `Loading maps from`,
		cvars: map[string]string{
			// `map_random 0` and `map_once 1` walk the list in order from the
			// map just played, which is the only setting of the three under
			// which "the next map" has one answer.  `vote_config_default 0`
			// keeps OSP_exitLevel's empty-server arm out of it -- that one
			// reloads the map to apply a config and would look like the flag.
			"map_queue": "1", "map_random": "0", "map_once": "1",
			"map_debug": "0", "vote_config_default": "0",
		},
	},
}

// boot installs a scratch server of one kind, with its rotation written in.
//
// `timelimit 1` is the shortest the engine takes and is the whole clock here;
// `fraglimit 0` because an empty server scores nothing anyway, and saying so
// keeps the arm honest about which limit it measured.
func boot(k kind, sub string, port int, dmflags string) (*playtest.Server, error) {
	d := filepath.Join(*dir, sub)
	os.RemoveAll(d)
	if err := colosseum.Install(d, *ref, "", *lib); err != nil {
		return nil, err
	}
	if err := os.WriteFile(filepath.Join(d, "colosseum", k.file),
		[]byte(k.body), 0o644); err != nil {
		return nil, err
	}
	cv := map[string]string{
		"g_ruleset": k.ruleset, "skill": "1", "admincode": "0",
		"timelimit": "1", "fraglimit": "0", "dmflags": dmflags,
		"botfill": "0", "minimumplayers": "0", "bots_minplayers": "0",
		"sv_maplist": "",
	}
	for n, v := range k.cvars {
		cv[n] = v
	}
	srv := &playtest.Server{
		Binary: *q2proded, Dir: d, Game: "colosseum", Map: start,
		Port: port, MaxClients: 8, LogPath: d + "/server.log",
		Cvars: cv,
	}
	if err := srv.Start(); err != nil {
		return nil, err
	}
	return srv, nil
}

// nextMap returns the map name of the first server spawn after mark.
//
// After MARK rather than anywhere in the log: `SpawnServer: q2dm1` is in every
// one of these logs from the first second, so a scenario that greps the whole
// file cannot tell "came back to q2dm1" from "never left it" -- which is the
// exact confusion the subject arms exist to resolve.
func nextMap(srv *playtest.Server, mark int, d time.Duration) string {
	re := regexp.MustCompile(`SpawnServer: (\S+)`)
	deadline := time.Now().Add(d)
	for time.Now().Before(deadline) {
		for _, l := range srv.GrepFrom(mark, `SpawnServer: `) {
			if m := re.FindStringSubmatch(l); m != nil {
				return m[1]
			}
		}
		time.Sleep(500 * time.Millisecond)
	}
	return ""
}

// arm runs one server to its timelimit and reports the map it went to.
func arm(k kind, what, sub string, port int, dmflags, want string) {
	name := fmt.Sprintf("%s/%s", k.label, what)
	fmt.Printf("\n== %s ==\n", name)
	srv, err := boot(k, sub, port, dmflags)
	if err != nil {
		check(name, false, "could not boot: %v", err)
		return
	}
	defer srv.Stop()

	if _, err := srv.WaitLog(k.read, 30*time.Second); err != nil {
		check(name, false,
			"the server never read a map rotation out of %s, so this arm "+
				"would have measured a server that has no rotation at all: %v",
			k.file, err)
		return
	}

	mark := srv.Len()
	if _, err := srv.WaitLog(`Timelimit hit`, 150*time.Second); err != nil {
		check(name, false, "the timelimit never arrived: %v", err)
		return
	}

	// An empty intermission ends on the frame it begins, so 40s is the map
	// load and not a timer.
	got := nextMap(srv, mark, 40*time.Second)
	switch got {
	case want:
		check(name, true, "")
	case "":
		check(name, false,
			"no map loaded in the 40s after the level ended (log lines since: %d)",
			srv.Len()-mark)
	case fallth:
		check(name, false,
			"went to %s, which is %s's own target_changelevel -- neither the "+
				"rotation nor the flag decided this level end", got, start)
	default:
		check(name, false, "went to %s, wanted %s", got, want)
	}
}

func main() {
	flag.Parse()
	if *q2proded == "" || *lib == "" {
		die("need -q2proded and -lib")
	}
	if err := os.MkdirAll(*dir, 0o755); err != nil {
		die("%v", err)
	}

	port := *port0
	for _, k := range kinds {
		arm(k, "control/the rotation chooses the next map",
			k.label+"-loop", port, "0", looped)
		port++
		arm(k, "subject/DF_SAME_LEVEL outranks the rotation",
			k.label+"-same", port, "32", start)
		port++
	}

	fmt.Printf("\n%d check(s) passed, %d failed\n", passed, failed)
	if failed > 0 {
		os.Exit(1)
	}
	os.Exit(0)
}
