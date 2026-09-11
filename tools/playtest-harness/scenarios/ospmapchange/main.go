// ospmapchange -- do OSP Tourney's four MANUAL map changes actually change the
// map to the one that was asked for?
//
// All four entry points were
// broken and none of them could be reached from a server console, which is why
// the defect shipped and why it could only be measured for half of the
// fix:
//
//	vote map <name>  + vote yes    a passed map vote
//	r_map <name>                   the referee client command
//	referee -> Change Map -> ...   the referee admin menu's map choice
//	vote config <name> + vote yes  a passed config vote (the one that WORKED)
//
// THE DEFECT.  `port_osp:g_main.c`'s `EndDMLevel` is ONE function holding
// `EnitityListClean()`, `endlvl_frame`, the highscore update and `NextMap()`.
// The OSP half was hoisted into `OSP_EndLevel` -- the ruleset's `EndLevel`
// dispatch row -- and left these four calling `EndDMLevel()` by name, which is
// now only baseq2's half.  `OSP_mapExists(.., true)` records the chosen map in
// `selected_map`/`next_map`; the only reader is `NextMap()`, and the only caller
// of that is `OSP_EndLevel`.  So the three `manual_map = 1` paths ended the
// level WITHOUT applying the map just chosen, and `EndDMLevel` fell through to
// the same map again.  The choice was not lost -- `selected_map` stayed set, so
// the next level end from the rules row picked it up ONE MAP LATE.
//
// THE WITNESS, and it is what makes these rows prove a mechanism rather than an
// outcome.  `"Next map: %s"` is printed at exactly one place in the whole tree:
// `osp_maps.c:166`, inside `NextMap()`'s `selected_map` arm.  Nothing else can
// emit it.  So each row asserts BOTH halves:
//
//	Next map: <target>       NextMap()'s selected arm ran  -> OSP_EndLevel was reached
//	SpawnServer: <target>    the level actually went there
//
// Against a library with the defect, a row reports no `Next map:` line at all
// and `SpawnServer:` naming the map it was already on.  That is a different
// observation from "nothing happened", which is why both are printed.
//
// THREE THINGS THAT COST TIME HERE.
//
//  1. A CONNECTED CLIENT HOLDS THE INTERMISSION OPEN, AND THE BUTTON IT WANTS
//     IS `BUTTON_ANY`.  `arena` and the OSP four get an "empty server,
//     go anyway" arm in BeginIntermission, but with a client on the server
//     `exitintermission` is only written by ClientThink on a button press --
//     and both the OSP arm and baseq2's test `ucmd->buttons & BUTTON_ANY`,
//     which is BIT(7), the "any key whatsoever" flag a real client ORs in
//     alongside whichever key is actually down.  `playtest.ButtonAttack` is
//     BIT(0), so pressing only that is INVISIBLE here: the press arrives, the
//     test is false, and the intermission sits there until the row times out.
//     That reads exactly like a map change that did not happen.  `buttonAny`
//     below is the missing bit; the harness exposes attack and use only.
//     Disconnecting instead of pressing does NOT work either: BeginIntermission
//     has already run by then and does not run twice.
//
//     The wait before a press counts is `nextlevel_click` (15s by default) or a
//     hardcoded 7s when `manual_map` is set, with `nextlevel_default` (45s)
//     ending it unattended.  Both are pinned low below -- they are not what is
//     under test, and fifteen seconds a row is fifteen seconds of nothing.
//
//  2. THE MAP LIST HAS TO CONTAIN THE TARGET AND NOT BE ABOUT TO PICK IT.
//     `r_map` and `vote map` both go through `OSP_mapExists`, so a map outside
//     `maps.txt` is refused before any of this is reached.  But the rotation's
//     own next entry is a false pass: it is what a BROKEN build would land on
//     one level later.  Every target below is chosen NOT to be the next entry.
//
//  3. `vote_enable_config` DEFAULTS TO 0 and is force-cleared to 0 by the
//     loader when no config list is found, so the config row needs a
//     `serverconfigs.txt`, a config file that exists, and the cvar set.  Its
//     point is that the config vote reads the map list belonging to the config
//     it just switched TO -- so the alternate config names a different
//     `map_file`, and the row asserts the new map comes from that list.
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

var (
	q2proded = flag.String("q2proded", "", "dedicated server binary")
	ref      = flag.String("ref", "/usr/share/games/quake2/baseq2", "baseq2 paks")
	ctf      = flag.String("ctf", "", "threewave ctf paks (unused, accepted for the wrapper)")
	lib      = flag.String("lib", "", "game library under test")
	dir      = flag.String("dir", "/tmp/ospmapchange", "scratch install dir")
	port     = flag.Int("port", 27991, "UDP port")
	ruleset  = flag.String("ruleset", "dm", "which OSP ruleset (dm is free play: no ready gate)")
	keep     = flag.Bool("keep", false, "keep the scratch dir on success")
)

const (
	refPassword = "letmein"

	// shared.h: BUTTON_ANY is BIT(7), "any key whatsoever".  See note 1 in
	// the header -- this, and not ButtonAttack, is what an intermission tests.
	buttonAny = 1 << 7

	// The rotation, and the reason for its shape.  With `map_random 0` and
	// `map_once 0` the rules row walks it in order, so starting on q2dm1 the
	// NEXT entry is q2dm2 -- which makes q2dm2 the one target no row may use.
	// Everything below picks from further down the list.
	startMap = "q2dm1"
	nextMap  = "q2dm2" // what the rotation would pick on its own
	rmapWant = "q2dm5" // row 2: the referee command
	voteWant = "q2dm3" // row 3: a passed map vote
	menuWant = "q2dm6" // row 4: the admin menu

	// The alternate config, for row 5.  It names its own map_file, so a map
	// out of altMaps proves the vote read the NEW list.
	altConfig = "ospmapchange-alt.cfg"
	altMapsFn = "ospmapchange-alt-maps.txt"

	// A SECOND alternate config for row 6, and ITS FILENAME IS MIXED CASE ON
	// PURPOSE.  `OSP_configExists` matches a voted name case-insensitively and
	// the caller hands that name straight to `exec`, so a build that does not
	// normalise it execs the CLIENT's spelling rather than the operator's.
	//
	// An all-lower-case fixture cannot see this and the first version of this
	// row used one, passing against both builds: Q2PRO retries a mixed-case
	// path in lower case on non-Windows (`common/files.c`, PATH_MIXED_CASE), so
	// `exec OSPMAPCHANGE-ALT2.CFG` finds `ospmapchange-alt2.cfg` anyway.  That
	// retry is also what makes the mixed-case name the discriminator: the
	// engine can lower-case a request, it cannot re-capitalise one, so only the
	// name the operator actually listed is certain to resolve.
	alt2Config = "OspMapChange-Alt2.cfg"
	alt2MapsFn = "ospmapchange-alt2-maps.txt"
)

var (
	mapList = []string{startMap, nextMap, voteWant, rmapWant, menuWant}
	altMaps = []string{"q2dm7", "q2dm8"}
	// One entry, and it appears in neither of the other two lists: whichever
	// list row 6's map came from is then unambiguous.
	alt2Maps = []string{"q2dm4"}
)

var failed, total, skipped int

func check(name string, ok bool, note string, a ...any) bool {
	total++
	if len(a) > 0 {
		note = fmt.Sprintf(note, a...)
	}
	tag := "[ ok ]"
	if !ok {
		tag = "[FAIL]"
		failed++
	}
	fmt.Printf("  %s %-52s %s\n", tag, name, note)
	return ok
}

func skip(name, why string) {
	total++
	skipped++
	fmt.Printf("  [skip] %-52s %s\n", name, why)
}

func report() {
	fmt.Printf("\n%d check(s), %d failed, %d skipped\n", total, failed, skipped)
}

// fixtures writes the map list, the alternate config and its map list into the
// installed gamedir.  colosseum.Install symlinks paks in and leaves the rest of
// the directory to the caller, so these are real files and are ours to write.
func fixtures(game string) error {
	write := func(name string, lines []string) error {
		return os.WriteFile(filepath.Join(game, name),
			[]byte(strings.Join(lines, "\n")+"\n"), 0o644)
	}
	if err := write("maps.txt", mapList); err != nil {
		return err
	}
	if err := write(altMapsFn, altMaps); err != nil {
		return err
	}
	// A config the vote can switch to.  All it does is point map_file at the
	// other list -- which is the whole mechanism the config-vote row tests,
	// because OSP_exitLevel re-reads the list AFTER the exec has run.
	if err := write(altConfig, []string{
		"// written by scenarios/ospmapchange",
		"set map_file " + altMapsFn,
	}); err != nil {
		return err
	}
	if err := write(alt2MapsFn, alt2Maps); err != nil {
		return err
	}
	if err := write(alt2Config, []string{
		"// written by scenarios/ospmapchange",
		"set map_file " + alt2MapsFn,
	}); err != nil {
		return err
	}
	// serverconfigs.txt: "<file>\t<description>", and each file must exist or
	// the loader drops the row and clears vote_enable_config.
	return write("serverconfigs.txt", []string{
		altConfig + "\tAlternate map list",
		alt2Config + "\tSecond alternate map list",
	})
}

// spawned reports every "SpawnServer: <map>" logged since mark.
func spawned(srv *playtest.Server, mark int) []string {
	var out []string
	re := regexp.MustCompile(`SpawnServer: (\S+)`)
	for _, ln := range srv.GrepFrom(mark, `SpawnServer: `) {
		if m := re.FindStringSubmatch(ln); m != nil {
			out = append(out, m[1])
		}
	}
	return out
}

// nextMapLines reports every "Next map: <map>" logged since mark.  One site in
// the tree prints this and it is inside NextMap()'s selected_map arm.
func nextMapLines(srv *playtest.Server, mark int) []string {
	var out []string
	re := regexp.MustCompile(`Next map: (\S+)`)
	for _, ln := range srv.GrepFrom(mark, `Next map: `) {
		if m := re.FindStringSubmatch(ln); m != nil {
			out = append(out, m[1])
		}
	}
	return out
}

// endLevel runs `trigger`, then holds BUTTON_ATTACK until the server spawns a
// map or the deadline passes, and reports which map and whether NextMap() said
// so first.
//
// The press is not optional and not a nicety: with a client connected, nothing
// else writes level.exitintermission, so a row without it times out on
// a correct build.
func endLevel(srv *playtest.Server, b *playtest.Bot, trigger func(), d time.Duration) (got string, witness string) {
	mark := srv.Len()
	trigger()
	deadline := time.Now().Add(d)
	for time.Now().Before(deadline) {
		if maps := spawned(srv, mark); len(maps) > 0 {
			// Give the console a beat to finish the block, then read the
			// witness from the same window.
			time.Sleep(300 * time.Millisecond)
			if w := nextMapLines(srv, mark); len(w) > 0 {
				witness = w[len(w)-1]
			}
			return maps[len(maps)-1], witness
		}
		if b != nil {
			b.Press(playtest.ButtonAttack|buttonAny, 350*time.Millisecond)
		} else {
			time.Sleep(350 * time.Millisecond)
		}
	}
	if w := nextMapLines(srv, mark); len(w) > 0 {
		witness = w[len(w)-1]
	}
	// A row that timed out is the one case where "what happened instead" is
	// the whole diagnosis, and neither channel alone carries it: a command the
	// mod REFUSED says so to the client, and a command it accepted but could
	// not finish leaves its trail on the console.  Print both rather than
	// leaving the next reader to re-run with a debugger.
	fmt.Printf("  --- no map change in %s; the console said:\n", d)
	log := srv.Log()
	if len(log) > mark {
		for _, ln := range log[mark:] {
			if strings.TrimSpace(ln) != "" {
				fmt.Printf("      %s\n", strings.TrimRight(ln, "\r\n"))
			}
		}
	} else {
		fmt.Printf("      (nothing at all)\n")
	}
	if b != nil {
		fmt.Printf("  --- and the client was told:\n")
		pr := b.Prints()
		if len(pr) > 6 {
			pr = pr[len(pr)-6:]
		}
		for _, x := range pr {
			fmt.Printf("      %s\n", strings.TrimSpace(x))
		}
	}
	return "", witness
}

// reset puts the server back on a known map through the SERVER console, so no
// row inherits where the previous one landed.
//
// Without it a broken build cascades: row 2 goes to the wrong map, row 3 starts
// from there, and by row 4 the server is on a map that is not in maps.txt at
// all -- so `NextMap()`'s "where are we now?" loop finds nothing, the admin
// menu's selection index means something else, and rows fail for reasons that
// have nothing to do with their own claim.  An A/B is only readable if each row
// answers one question.
func reset(srv *playtest.Server, mapname string) error {
	mark := srv.Len()
	// `gamemap`, not `map`: Q2PRO ignores a `map` issued while a game is
	// running unless sv_allow_map is set (the same trap exists in the mod).
	srv.Console("gamemap %s", mapname)
	deadline := time.Now().Add(30 * time.Second)
	for time.Now().Before(deadline) {
		for _, m := range spawned(srv, mark) {
			if m == mapname {
				time.Sleep(1500 * time.Millisecond)
				return nil
			}
		}
		time.Sleep(300 * time.Millisecond)
	}
	return fmt.Errorf("server never went back to %s", mapname)
}

// enter connects one client and puts it in the game, and CONFIRMS it got in.
//
// Under an OSP ruleset a fresh client is an OBSERVER, and `osp_entered` is what
// OSP_yes_cmd and OSP_votePercent both read -- so a bot that never joined is a
// bot whose vote is refused with "Observers cannot vote with active players in
// the game".
//
// One `join` is not reliable, and the reason is the level change every row
// before this one performed: a client that connects while the server is still
// settling a new map issues its `join` into a game that is not ready for it,
// the command is dropped, and NOTHING SAYS SO.  The first version of this
// scenario sent one `join` and three of its five bots never entered -- one row
// failed and passed on a re-run, which is the flake that teaches people to
// re-run instead of to read.  So: resend until the server says the client is
// in, and treat running out of tries as a scenario error rather than as a
// finding about the game.
func enter(srv *playtest.Server, name string, port int) (*playtest.Bot, error) {
	b := playtest.NewBot(name, "127.0.0.1", port)
	if err := b.Start(30 * time.Second); err != nil {
		return nil, err
	}
	in := regexp.MustCompile(regexp.QuoteMeta(name) + ` entered the game`)
	for i := 0; i < 12; i++ {
		if len(srv.Grep(in.String())) > 0 {
			b.WaitFrames(5, 3*time.Second)
			return b, nil
		}
		b.Cmd("join")
		b.WaitFrames(5, 3*time.Second)
	}
	b.Disconnect()
	return nil, fmt.Errorf("%s never entered the game", name)
}

// ------------------------------------------------------------------ the menu
//
// OSP draws its pop-up menu as a LAYOUT, not as a statusbar, so none of
// Bot.Menu/MenuPick/MenuUse applies -- those read the statusbar.  These four
// helpers are the same ones scenarios/ospmenu carries, and the reasoning behind
// them is written up there: the redraw is rate limited, so a bot that presses a
// cursor key and reads straight back is looking at the frame BEFORE its press,
// and a loop that presses again on a settled layout runs ahead of the server
// and silently selects the WRONG ROW.

var reRow = regexp.MustCompile(`c?string2?\s+"([^"]*)"`)

func rows(layout string) []string {
	var out []string
	for _, m := range reRow.FindAllStringSubmatch(layout, -1) {
		out = append(out, m[1])
	}
	return out
}

func cursor(layout string) (int, string) {
	for i, r := range rows(layout) {
		if strings.HasPrefix(r, "\x0d") {
			return i, strings.TrimPrefix(r, "\x0d")
		}
	}
	return -1, ""
}

func rowText(layout, re string) string {
	rx := regexp.MustCompile(re)
	for _, r := range rows(layout) {
		r = strings.TrimPrefix(r, "\x0d")
		if rx.MatchString(playtest.Decode(r)) {
			return strings.TrimSpace(playtest.Decode(r))
		}
	}
	return ""
}

func settledCursor(b *playtest.Bot) (string, bool) {
	last, stable := b.Layout(), 0
	for i := 0; i < 40; i++ {
		time.Sleep(100 * time.Millisecond)
		now := b.Layout()
		if now == last {
			if stable++; stable >= 3 {
				_, txt := cursor(now)
				return playtest.Decode(txt), true
			}
			continue
		}
		last, stable = now, 0
	}
	_, txt := cursor(last)
	return playtest.Decode(txt), false
}

// waitCursor presses `cmd` until the cursor lands on a row matching re, with
// exactly one press outstanding at a time.
func waitCursor(b *playtest.Bot, cmd, re string, tries int) (string, bool) {
	rx := regexp.MustCompile(re)
	txt, _ := settledCursor(b)
	for i := 0; i < tries && !rx.MatchString(txt); i++ {
		prev := txt
		b.Cmd(cmd)
		for j := 0; j < 8; j++ {
			if t, ok := settledCursor(b); ok && t != prev {
				txt = t
				break
			}
		}
	}
	return txt, rx.MatchString(txt)
}

func use(b *playtest.Bot) {
	b.Cmd("invuse")
	settledCursor(b)
}

// choiceRow reads the map name the selection menu is currently showing --
// AdminSelect_Menu row 6, which is `[ SELECT ]` until a real map is picked.
func choiceRow(b *playtest.Bot) string {
	return rowText(b.Layout(), `^(q2dm\w+|\[ SELECT \])`)
}

// cycleChoice activates the "Select new map to load:" row until row 6 reads
// `want`, CONFIRMING each activation before sending the next one.
//
// A bare loop of `invuse` does not work and does not look broken: menu
// activations are throttled -- an `invuse` within five frames of the last one
// is dropped in silence -- so a loop that presses as fast as the layout settles
// (~300ms) loses roughly every other press and stops one or two maps short of
// the target.  It is also not a flake: where it stops depends on which map the
// index happened to start from, so it passes from one starting map and fails
// from another, which is how this row passed before the rows were made
// independent of each other.
func cycleChoice(b *playtest.Bot, want string, maps int) (string, bool) {
	got := choiceRow(b)
	// Each map plus the [ SELECT} sentinel, twice round: enough to reach any
	// entry from any start, and bounded so a wrong build runs out rather than
	// spinning.
	for i := 0; i < 2*(maps+1) && got != want; i++ {
		prev := got
		b.Cmd("invuse")
		for j := 0; j < 12; j++ { // ~1.8s, comfortably past the 5-frame gate
			time.Sleep(150 * time.Millisecond)
			if now := choiceRow(b); now != "" && now != prev {
				got = now
				break
			}
		}
		if got == prev { // the press was swallowed -- say so rather than looping
			b.Cmd("invuse")
			time.Sleep(700 * time.Millisecond)
			got = choiceRow(b)
		}
	}
	return got, got == want
}

// main is a wrapper and run() holds the body, because os.Exit does NOT run
// deferred calls.  A scenario that exits from inside a row while its server
// is up leaves q2proded holding the UDP port, and the NEXT run fails to bind
// and reports "server did not spawn" -- which looks nothing like the cause.
func main() {
	os.Exit(run())
}

func run() int {
	flag.Parse()
	if *q2proded == "" || *lib == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded and -lib")
		return 2
	}
	if !colosseum.IsOSP(*ruleset) {
		fmt.Fprintf(os.Stderr, "%s is not an OSP ruleset; the four map-change "+
			"entry points are tourney's\n", *ruleset)
		return 2
	}
	if err := colosseum.Install(*dir, *ref, *ctf, *lib); err != nil {
		fmt.Fprintln(os.Stderr, "install:", err)
		return 2
	}
	game := filepath.Join(*dir, "colosseum")
	if err := fixtures(game); err != nil {
		fmt.Fprintln(os.Stderr, "fixtures:", err)
		return 2
	}

	srv := &playtest.Server{
		Binary: *q2proded, Dir: *dir, Game: "colosseum",
		Map: startMap, Port: *port, MaxClients: 8,
		Cvars: map[string]string{
			"g_ruleset":  *ruleset,
			"deathmatch": "1",
			"coop":       "0",
			"cheats":     "1",
			// No clock and no fraglimit: every level change below must be one
			// this scenario asked for.  Row 1 turns the clock on for exactly
			// one rotation and turns it off again.
			"timelimit": "0",
			"fraglimit": "0",
			// Nothing may add a player.  A bot on the server changes the vote
			// arithmetic and the intermission's client count.
			"bots":            "0",
			"bots_minplayers": "0",
			"minimumplayers":  "0",
			"botfill":         "0",
			// The rotation walks maps.txt in order, so "the next entry" is a
			// fact this scenario can rely on rather than a coin toss.
			"map_queue":  "1",
			"map_random": "0",
			"map_once":   "0",
			// Voting.  vote_enable_config defaults to 0; the other two default
			// on and are written down so the row cannot be silently disabled by
			// an inherited config.
			"vote_enable":          "1",
			"vote_enable_map":      "1",
			"vote_enable_config":   "1",
			"vote_threshold":       "51",
			"vote_time":            "45",
			"vote_countspectators": "1",
			"vote_config_default":  "0",
			"referee_enable":       "1",
			"referee_password":     refPassword,
			"flood_msgs":           "0",
			// The intermission's own two timers, pinned so a row waits five
			// seconds rather than fifteen.  Not under test; see note 1.
			"nextlevel_click":   "5",
			"nextlevel_default": "30",
			"dmflags":           "1024", // DF_FORCE_RESPAWN
		},
		LogPath: filepath.Join(*dir, "server.log"),
	}
	if err := srv.Start(); err != nil {
		fmt.Fprintln(os.Stderr, "server:", err)
		return 2
	}
	defer srv.Stop()
	time.Sleep(2 * time.Second)

	// ------------------------------------------------------ 0. preconditions
	//
	// Without these three, every row below fails for a reason that has nothing
	// to do with the dispatch -- and a refused command and a broken command
	// look identical from outside.
	fmt.Printf("\n##### 0. preconditions\n")
	if boom := srv.Grep(`Game Error|ERROR:`); len(boom) > 0 {
		check("boot/no game error", false, strings.TrimSpace(boom[0]))
		report()
		return 1
	}
	check("boot/no game error", true, "")
	check("boot/the OSP map list loaded",
		len(srv.Grep(`Loading maps from`)) > 0,
		"maps.txt: %s", strings.Join(mapList, " "))
	cfgFound := srv.Grep(`server configs found`)
	haveConfigs := len(cfgFound) > 0 && !strings.HasPrefix(strings.TrimSpace(cfgFound[0]), "No ")
	check("boot/the alternate server config loaded", haveConfigs,
		"%s", strings.TrimSpace(strings.Join(cfgFound, "; ")))

	// --------------------------------------- 1. the positive control: the rules row
	//
	// The rules row was never broken -- it has always reached OSP_EndLevel
	// through G_EndLevel() -- so it is the control that says NextMap() and
	// maps.txt work on THIS server.  Without it, a failing vote row could
	// equally be a map list that was never read, and the two need different
	// fixes.
	fmt.Printf("\n##### 1. control: does the rules row still walk maps.txt?\n")
	ctl, err := enter(srv, "control", *port)
	if err != nil {
		fmt.Fprintln(os.Stderr, "bot:", err)
		return 2
	}
	got, _ := endLevel(srv, ctl, func() {
		// 0.1 minutes is six seconds; level.time restarts at zero on the new
		// map, so there is room to turn it off again before it fires twice.
		srv.Console("set timelimit 0.1")
	}, 40*time.Second)
	srv.Console("set timelimit 0")
	check("control/timelimit rotates to the next entry", got == nextMap,
		"went to %q, maps.txt has %s after %s", got, nextMap, startMap)
	ctl.Disconnect()
	if got != nextMap {
		// Everything below asserts "not the rotation's own answer", and that
		// assertion is meaningless if the rotation is not working.
		fmt.Println("  --- the control failed; the rows below cannot be interpreted")
		report()
		return 1
	}
	cur := nextMap
	time.Sleep(3 * time.Second)

	// ------------------------------------------------- 2. the referee command
	fmt.Printf("\n##### 2. r_map <name> -- the referee client command\n")
	if err := reset(srv, startMap); err != nil {
		fmt.Fprintln(os.Stderr, "reset:", err)
		return 2
	}
	cur = startMap
	b, err := enter(srv, "referee", *port)
	if err != nil {
		fmt.Fprintln(os.Stderr, "bot:", err)
		return 2
	}
	// referee_enable AND referee_password are both required: OSP_referee_cmd
	// refuses with "Referee mode is disabled on this server" before it looks at
	// the password.  Granting it also toggles the admin menu open, which is
	// harmless for a client command.
	b.Cmd("referee %s", refPassword)
	time.Sleep(1200 * time.Millisecond)
	gotRef := check("r_map/referee status granted",
		len(mustPrint(b, `(?i)referee`)) > 0, "%s", firstPrint(b, `(?i)referee`))
	if !gotRef {
		skip("r_map/loads the named map", "referee status was not granted")
		skip("r_map/NextMap() ran (the `Next map:` witness)", "referee status was not granted")
		b.Disconnect()
	} else {
		got, wit := endLevel(srv, b, func() { b.Cmd("r_map %s", rmapWant) }, 40*time.Second)
		check("r_map/loads the named map", got == rmapWant,
			"asked %s, got %q (a broken build stays on %s; the rotation's own "+
				"answer would be the entry after it)", rmapWant, got, cur)
		check("r_map/NextMap() ran (the `Next map:` witness)", wit == rmapWant,
			"witness=%q -- printed only by osp_maps.c's selected_map arm", wit)
		b.Disconnect()
		if got != "" {
			cur = got
		}
	}
	time.Sleep(3 * time.Second)

	// ----------------------------------------------------- 3. a passed map vote
	fmt.Printf("\n##### 3. vote map <name> + vote yes -- a passed map vote\n")
	if err := reset(srv, startMap); err != nil {
		fmt.Fprintln(os.Stderr, "reset:", err)
		return 2
	}
	cur = startMap
	b, err = enter(srv, "voter", *port)
	if err != nil {
		fmt.Fprintln(os.Stderr, "bot:", err)
		return 2
	}
	// BOTH COMMANDS ARE INSIDE THE WINDOW, and that is not tidiness.
	// `OSP_checkVote()` is called from the vote's START as well as from `vote
	// yes`, so with one client on the server the proposal can carry
	// immediately -- and a window opened after it would see the SpawnServer
	// (the map load takes a moment) while the `Next map:` witness had already
	// scrolled past.  The row would then report the outcome correctly and the
	// mechanism as absent, which is the worst of both.
	got, wit := endLevel(srv, b, func() {
		b.Cmd("vote map %s", voteWant)
		time.Sleep(1200 * time.Millisecond)
		b.Cmd("vote yes")
	}, 40*time.Second)
	{
		started := len(mustPrint(b, `(?i)vote|Usage`)) > 0
		check("vote map/the vote was accepted", started,
			"%s", firstPrint(b, `(?i)vote|Usage|not available`))
		check("vote map/loads the voted map", got == voteWant,
			"voted %s, got %q (a broken build stays on %s)", voteWant, got, cur)
		check("vote map/NextMap() ran (the `Next map:` witness)", wit == voteWant,
			"witness=%q", wit)
		b.Disconnect()
		if got != "" {
			cur = got
		}
	}
	time.Sleep(3 * time.Second)

	// ------------------------------------------------ 4. the admin menu's choice
	//
	// referee <pw> opens AdminMain_Menu.  "Change Map" opens AdminSelect_Menu
	// in its map mode, where row 4 ("Select new map to load:") advances the
	// choice one map per activation and row 6 shows it -- `[ SELECT ]` until a
	// real map is picked, which is also when row 11 ("Load selected map") gains
	// its SelectFunc.  Row 11 is OSP_mapAdminChoose, the site under test.
	fmt.Printf("\n##### 4. referee admin menu -> Change Map -> Load selected map\n")
	if err := reset(srv, startMap); err != nil {
		fmt.Fprintln(os.Stderr, "reset:", err)
		return 2
	}
	cur = startMap
	b, err = enter(srv, "admin", *port)
	if err != nil {
		fmt.Fprintln(os.Stderr, "bot:", err)
		return 2
	}
	b.Cmd("referee %s", refPassword)
	time.Sleep(3 * time.Second)
	if _, ok := waitCursor(b, "invnext", `Change Map`, 24); !ok {
		check("admin menu/reaches the Change Map row", false,
			"cursor stopped on %q", firstOf(settledCursor(b)))
		skip("admin menu/selects "+menuWant, "the Change Map row was never reached")
		skip("admin menu/loads the selected map", "the Change Map row was never reached")
		skip("admin menu/NextMap() ran (the `Next map:` witness)", "the Change Map row was never reached")
		b.Disconnect()
	} else {
		check("admin menu/reaches the Change Map row", true, "")
		use(b)
		_, inMap := waitCursor(b, "invnext", `Select new map to load`, 24)
		check("admin menu/the map selection menu opened", inMap,
			"title=%q", rowText(b.Layout(), `Selection Menu|Admin Menu`))
		// Advance the choice until row 6 reads the target.  map_size is
		// len(mapList), and the cycle passes through `[ SELECT ]`, so two full
		// laps is a generous bound and a wrong build runs out rather than
		// looping for ever.
		picked, okPick := "", false
		if inMap {
			picked, okPick = cycleChoice(b, menuWant, len(mapList))
		}
		if !check("admin menu/selects "+menuWant, okPick,
			"row 6 reads %q", picked) {
			skip("admin menu/loads the selected map", "the map was never selected")
			skip("admin menu/NextMap() ran (the `Next map:` witness)", "the map was never selected")
			b.Disconnect()
		} else {
			_, onLoad := waitCursor(b, "invnext", `Load selected map`, 24)
			check("admin menu/reaches the Load row", onLoad,
				"cursor on %q", firstOf(settledCursor(b)))
			got, wit := endLevel(srv, b, func() { b.Cmd("invuse") }, 40*time.Second)
			check("admin menu/loads the selected map", got == menuWant,
				"selected %s, got %q (a broken build stays on %s)", menuWant, got, cur)
			check("admin menu/NextMap() ran (the `Next map:` witness)", wit == menuWant,
				"witness=%q", wit)
			b.Disconnect()
			if got != "" {
				cur = got
			}
		}
	}
	time.Sleep(3 * time.Second)

	// ------------------------------------------------- 5. a passed config vote
	//
	// LAST, because it replaces the map list every row above depends on.
	//
	// This is the one of the four that kept working, and it worked by a
	// different route: OSP_config_vote sets manual_map = 2, and OSP_exitLevel
	// has its own arm for that value which re-reads the list and issues the map
	// itself.  The row is here as a REGRESSION check -- the fix routes this site
	// through OSP_EndLevel as well, so NextMap() now runs once against the OLD
	// list before exitLevel overrides it from the NEW one, and the answer that
	// reaches the server must still come from the new list.
	fmt.Printf("\n##### 5. vote config + vote yes -- regression: still reads the NEW list\n")
	if err := reset(srv, startMap); err != nil {
		fmt.Fprintln(os.Stderr, "reset:", err)
		return 2
	}
	if !haveConfigs {
		skip("vote config/loads a map from the new config's list",
			"no server configs were loaded, so the vote cannot be started")
	} else {
		b, err = enter(srv, "cfgvoter", *port)
		if err != nil {
			fmt.Fprintln(os.Stderr, "bot:", err)
			return 2
		}
		got, _ := endLevel(srv, b, func() {
			b.Cmd("vote config %s", altConfig)
			time.Sleep(1200 * time.Millisecond)
			b.Cmd("vote yes")
		}, 40*time.Second)
		fromAlt := false
		for _, m := range altMaps {
			if got == m {
				fromAlt = true
			}
		}
		check("vote config/loads a map from the new config's list", fromAlt,
			"got %q, %s names map_file %s (%s)", got, altConfig, altMapsFn,
			strings.Join(altMaps, " "))
		b.Disconnect()
	}

	// ------------------------------- 6. a config vote whose CASE does not match
	//
	// LAST, and it depends on row 5 having run: the server is on the alternate
	// map list by now, so a map out of alt2Maps can only mean this vote's own
	// config was actually exec'd.  A build that hands the client's spelling to
	// `exec` leaves the list where row 5 put it and the next map comes from
	// THERE -- a level change that happened, with the configuration silently
	// not applied, which is the whole reason this row reads the map list rather
	// than watching for a map change.
	fmt.Printf("\n##### 6. vote config, WRONG CASE -- is the name normalised?\n")
	if !haveConfigs {
		skip("vote config/a wrong-cased name still execs the right file",
			"no server configs were loaded")
	} else {
		if err := reset(srv, startMap); err != nil {
			fmt.Fprintln(os.Stderr, "reset:", err)
			return 2
		}
		b, err = enter(srv, "casevoter", *port)
		if err != nil {
			fmt.Fprintln(os.Stderr, "bot:", err)
			return 2
		}
		// Lower-cased, not upper: the engine's own retry already lower-cases a
		// request, so asking in lower case for a mixed-case file is the one
		// spelling nothing but the mod's normalisation can rescue.
		shouted := strings.ToLower(alt2Config)
		got, _ := endLevel(srv, b, func() {
			b.Cmd("vote config %s", shouted)
			time.Sleep(1200 * time.Millisecond)
			b.Cmd("vote yes")
		}, 40*time.Second)
		fromAlt2 := false
		for _, m := range alt2Maps {
			if got == m {
				fromAlt2 = true
			}
		}
		check("vote config/a wrong-cased name still execs the right file",
			fromAlt2, "voted %q for the listed %q; got %q -- it names map_file "+
				"%s (%s), and a build that execs the client's spelling leaves "+
				"the list at %s (%s)",
			shouted, alt2Config, got, alt2MapsFn,
			strings.Join(alt2Maps, " "), altMapsFn, strings.Join(altMaps, " "))
		b.Disconnect()
	}

	report()
	if failed == 0 && !*keep {
		os.Remove(srv.LogPath)
	}
	if failed > 0 {
		fmt.Printf("server console: %s\n", srv.LogPath)
		return 1
	}
	return 0
}

// mustPrint returns the client prints matching re.  A refused command and a
// broken command are different observations and only the second is a finding,
// so every row that can be refused looks at what the server said.
func mustPrint(b *playtest.Bot, re string) []string {
	rx := regexp.MustCompile(re)
	var out []string
	for _, p := range b.Prints() {
		if rx.MatchString(p) {
			out = append(out, p)
		}
	}
	return out
}

func firstPrint(b *playtest.Bot, re string) string {
	if p := mustPrint(b, re); len(p) > 0 {
		return strings.TrimSpace(p[len(p)-1])
	}
	return "(no answer from the server)"
}

func firstOf(s string, _ bool) string { return s }
