// ra2botvote -- does an arena's own `bots` switch actually keep bots out?
//
// Each RA2 arena has a `bots` setting (and an `allowvotingbots` key
// saying whether the people in it may vote on it).  The switch is a refusal in
// five separate places -- fill-arena selection, RA_AutoArena, RA_BotArenaOpen,
// RA_BotJoinArena, and the ruleset inference in CheckMinimumPlayers -- so the
// interesting question is not whether any one of them fires but whether the
// server, taken as a whole, ends up with bots in an arena that said no.
//
// Asserted in BOTH signs, because only the negative half fails on a broken
// build and only the positive half proves the test can see a bot at all:
//
//	A  bots: 1 (the default)  ->  the fill puts bots in the arena
//	B  bots: 0                ->  it puts none, and says why
//
// Phase B is the one that matters, and it is deliberately checked twice over:
// against the world (how many bot clients are actually on the server) and
// against the diagnostic (`sv ruleset`'s botfill line).  The skill's own advice
// is to prefer the world -- a mod's diagnostic reports what the mod believes --
// so the client census is the assertion and the botfill line is corroboration
// that the zero came from THIS feature rather than from the fill being off, a
// missing roster, or a map with nothing to count.
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

	"q2playtest/colosseum"
	"q2playtest/playtest"
	"q2playtest/ra2"
)

var failed int

func check(name string, ok bool, detail string) {
	tag := " ok "
	if !ok {
		tag = "FAIL"
		failed++
	}
	fmt.Printf("  [%s] %-38s %s\n", tag, name, detail)
}

// arenaCfg is a whole arena.cfg, written into the game dir for one phase.
// q2dm1 is an idmap -- RA2 runs it as a single pickup arena -- so the global
// block is the whole file and there are no per-arena blocks to write.
func arenaCfg(bots, allowVoting int) string {
	return fmt.Sprintf("maploop: q2dm1;\nvotetries: 3;\nbots: %d;\nallowvotingbots: %d;\n",
		bots, allowVoting)
}

// phase boots one arena server with the given arena.cfg and reports the bot
// census once the fill has had time to settle.
func phase(tag string, cfg string, port int, settle time.Duration,
	q2, ref, ctf, lib, glad, root string) (*playtest.Server, colosseum.BotCensus, int, error) {

	var census colosseum.BotCensus

	d := fmt.Sprintf("%s/%s", root, tag)
	if err := colosseum.Install(d, ref, ctf, lib); err != nil {
		return nil, census, 0, err
	}
	if err := colosseum.InstallBrain(d, glad); err != nil {
		return nil, census, 0, err
	}

	// Install symlinks the reference install's .cfg files in; this phase needs
	// its own, so the link (if any) is replaced by a real file.
	path := d + "/colosseum/arena.cfg"
	os.Remove(path)
	if err := os.WriteFile(path, []byte(cfg), 0o644); err != nil {
		return nil, census, 0, err
	}

	srv := &playtest.Server{
		Binary: q2, Dir: d, Game: "colosseum", Map: "q2dm1",
		Port: port, MaxClients: 12,
		Cvars: map[string]string{
			"g_ruleset": "arena", "cheats": "1", "skill": "1",
			// The fill under test, and the flat count silenced so that any bot
			// that appears got there through `botfill` and not through
			// `minimumplayers` -- which routes bots by a different path and
			// would make phase B ambiguous.
			"botfill":        "1",
			"minimumplayers": "0", "bots_minplayers": "0",
		},
		LogPath: d + "/server.log",
	}
	if err := srv.Start(); err != nil {
		return nil, census, 0, err
	}

	// A PERSON HAS TO BE IN THE ARENA, and that is the scheduler rather than a detail
	// of the harness: `botfill` fills the arenas people are in, so an arena
	// with nobody in it wants no bots and an empty SERVER never fills at all.
	// Without this client both phases would report zero bots -- the one that is
	// supposed to fill and the one that is supposed not to -- and stop telling
	// them apart.
	human := playtest.NewBot("watcher", "127.0.0.1", port)
	if err := human.Start(60 * time.Second); err != nil {
		return nil, census, 0, fmt.Errorf("client: %w", err)
	}
	if err := ra2.JoinTeam(human, ra2.PickupTeam(1, "Red")); err != nil {
		return nil, census, 0, fmt.Errorf("join: %w", err)
	}

	// Where the console stood once the PERSON was in.  Phase B counts join
	// lines to corroborate its census, and the client above writes one of its
	// own -- so the count has to start after it, or the corroboration becomes a
	// second reading of the fact that a human joined.
	mark := srv.Len()

	// The fill runs every 32 frames and a bot's ClientBegin is held back until
	// its library reports initialised -- on a first visit to a map that is the
	// whole reachability build.  Poll rather than sleep once, and keep asking
	// `sv ruleset` so the census line is fresh.
	deadline := time.Now().Add(settle)
	for time.Now().Before(deadline) {
		srv.Console("sv ruleset")
		time.Sleep(2 * time.Second)
		if b, err := colosseum.ParseBots(strings.Join(srv.Log(), "\n")); err == nil {
			census = b
			if b.Bots > 0 {
				break // phase A: as soon as the fill has visibly worked
			}
		}
	}

	// One last read, so a phase that never saw a bot still reports a real
	// census rather than a zero value that means "never parsed".
	srv.Console("sv ruleset")
	time.Sleep(1500 * time.Millisecond)
	if b, err := colosseum.ParseBots(strings.Join(srv.Log(), "\n")); err == nil {
		census = b
	}

	return srv, census, mark, nil
}

// proposeRows opens one client's "Change Arena Settings" menu -- the propose
// form, mode 1, the one an ordinary player reaches without the admin code --
// and returns the rows it draws.
//
// This is the half the server console cannot answer.  `allowvotingbots`
// decides whether the "Allow Bots" row is OFFERED to a player, and the only
// witness to that is a client's statusbar, because an RA2 menu IS the statusbar.
// It returns the rows of the FIRST page separately from the union of every page,
// because where the row sits is part of the feature: menu.c's window is 18 rows
// and a setting parked behind "(More)" is one most players never find.
func proposeRows(tag, cfg string, port int, q2, ref, ctf, lib, glad, root string,
	extra map[string]string) ([]string, []string, error) {
	d := fmt.Sprintf("%s/%s", root, tag)
	if err := colosseum.Install(d, ref, ctf, lib); err != nil {
		return nil, nil, err
	}
	if err := colosseum.InstallBrain(d, glad); err != nil {
		return nil, nil, err
	}
	path := d + "/colosseum/arena.cfg"
	os.Remove(path)
	if err := os.WriteFile(path, []byte(cfg), 0o644); err != nil {
		return nil, nil, err
	}

	srv := &playtest.Server{
		Binary: q2, Dir: d, Game: "colosseum", Map: "q2dm1",
		Port: port, MaxClients: 12,
		Cvars: map[string]string{
			"g_ruleset": "arena", "cheats": "1",
			// The fill is OFF for these two phases.  A bot arriving mid-menu
			// can start a round and move this client out of FIGHT_SPECTATING,
			// and show_observer_menu is what draws the rows being counted.
			"botfill":        "0",
			"minimumplayers": "0", "bots_minplayers": "0",
		},
		LogPath: d + "/server.log",
	}
	// The phase's own cvars, applied over the block above so a phase can set
	// the thing under test without restating the four that make it readable.
	for k, v := range extra {
		srv.Cvars[k] = v
	}
	if err := srv.Start(); err != nil {
		return nil, nil, err
	}
	defer srv.Stop()

	b := playtest.NewBot("voter", "127.0.0.1", port)
	if err := b.Start(30 * time.Second); err != nil {
		return nil, nil, err
	}
	defer b.Disconnect()

	// q2dm1 is an idmap: RA2 runs it as one pickup arena, whose two teams the
	// mod makes at map load.  A pickup arena refuses the ordinary arena menu,
	// so the client joins a team that is already in it.
	if err := ra2.JoinTeam(b, ra2.PickupTeam(1, "Red")); err != nil {
		return nil, nil, fmt.Errorf("join: %w", err)
	}
	b.WaitFrames(10, 5*time.Second)

	// TAB.  RA2 has no console command for any of this.
	b.Cmd("inven")
	if err := b.WaitMenu(`(?i)observer options`, 20*time.Second); err != nil {
		return nil, nil, fmt.Errorf("observer menu: %w", err)
	}
	if err := b.MenuPick(`(?i)change arena settings`, 20*time.Second); err != nil {
		return nil, nil, fmt.Errorf("propose: %w", err)
	}
	if err := b.WaitMenu(`(?i)arena admin menu`, 20*time.Second); err != nil {
		return nil, nil, fmt.Errorf("admin menu: %w", err)
	}

	// THE MENU SCROLLS, AND READING IT ONCE READS ONE PAGE OF IT.
	//
	// menu.c draws a window of MAXMENUITEMS (18) rows anchored on the cursor and
	// marks the rest "(More)".  This propose menu is 21 rows before the vote adds
	// anything -- 18 settings plus a spacer, Propose and Cancel -- so page one is
	// already full and everything after Damage Scoring is below the fold.  A test
	// that read the first page and concluded "no Allow Bots row" would report a
	// working feature as broken, and did, which is why this walks the cursor down
	// and takes the union of what it sees.
	seen := map[string]bool{}
	var rows []string
	add := func() []string {
		_, items := b.Menu()
		page := make([]string, 0, len(items))
		for _, it := range items {
			if it.Text == "" {
				continue
			}
			page = append(page, it.Text)
			if !seen[it.Text] {
				seen[it.Text] = true
				rows = append(rows, it.Text)
			}
		}
		return page
	}
	first := add()
	for i := 0; i < 40; i++ {
		b.Cmd("invnext")
		time.Sleep(120 * time.Millisecond)
		add()
	}
	return first, rows, nil
}

// pickScrolling activates a menu row that may be below the fold.
//
// playtest.MenuPick fast-fails when the row it wants is not on the page it can
// see, which is right for a menu that fits and wrong for one that scrolls: the
// arena settings menu is 21 rows in its propose form and 22 in the admin form,
// against menu.c's 18-row window.  This steps the cursor -- which is what moves
// the window -- until the wanted row is the selected one, and only then uses it.
func pickScrolling(b *playtest.Bot, re string, timeout time.Duration) error {
	rx := regexp.MustCompile(re)
	deadline := time.Now().Add(timeout)

	for steps := 0; time.Now().Before(deadline) && steps < 80; steps++ {
		_, items := b.Menu()
		for _, it := range items {
			if it.Selected && rx.MatchString(it.Text) {
				return b.MenuUse(timeout)
			}
		}
		b.Cmd("invnext")
		time.Sleep(150 * time.Millisecond)
	}
	return fmt.Errorf("cursor never reached %q in menu %q", re, b.MenuTitle())
}

// arenaHere reads `here=N` for one arena out of the last `sv arenadump`.  That
// number is RA_ArenaPlayers() -- who is actually on a team in that arena -- so
// it is the world's answer to "are the bots still in there", not the bot
// layer's opinion of its own fill.
func arenaHere(srv *playtest.Server, arena int) int {
	ls := srv.Grep(fmt.Sprintf(`arena %-2d .*here=`, arena))
	if len(ls) == 0 {
		return -1
	}
	m := reHere.FindStringSubmatch(ls[len(ls)-1])
	if m == nil {
		return -1
	}
	n, _ := strconv.Atoi(m[1])
	return n
}

var reHere = regexp.MustCompile(`here=(\d+)`)

// aasPath is the reachability file the crowded phase copies in, resolved once
// in main from -aas or by looking where the tree's own runs leave them.
var aasPath string

// findAAS returns the first readable ra2map9.aas among the candidates.
func findAAS(explicit string) string {
	if explicit != "" {
		return explicit
	}
	// Written beside the map by the brain on a previous run.  Globbed because
	// the working directory it was produced under is not stable; $RA2AAS names
	// it outright.
	for _, pat := range []string{
		os.Getenv("RA2AAS"),
		"../../../yquake2/release_/arena/maps/ra2map9.aas",
		"/tmp/q2playtest/*/colosseum/maps/ra2map9.aas",
	} {
		if pat == "" {
			continue
		}
		ms, _ := filepath.Glob(pat)
		for _, m := range ms {
			if fi, err := os.Stat(m); err == nil && fi.Size() > 0 {
				return m
			}
		}
	}
	return ""
}

func hasRow(rows []string, sub string) bool {
	for _, r := range rows {
		if strings.Contains(strings.ToLower(r), strings.ToLower(sub)) {
			return true
		}
	}
	return false
}

// botfillLine returns the last `botfill` line `sv ruleset` printed.
func botfillLine(srv *playtest.Server) string {
	ls := srv.Grep(`botfill\s`)
	if len(ls) == 0 {
		return ""
	}
	return strings.TrimSpace(ls[len(ls)-1])
}

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "retail baseq2 paks")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "Threewave paks")
	lib := flag.String("lib", "", "game library under test")
	glad := flag.String("gladdir", "", "gladiator-bot-restored checkout (the brain)")
	// The RA2 retail install -- pak0..2 hold the ra2map*.bsp.  Only the crowded
	// phase needs it; without it that phase is skipped and says so, rather than
	// being silently subtracted.
	ra2ref := flag.String("ra2ref", os.Getenv("HOME")+"/q2-dev/yquake2/release_/arena",
		"RA2 install (pak0..2 and arena.cfg)")
	aas := flag.String("aas", "", "ra2map9.aas for the crowded phase; searched for if unset")
	root := flag.String("dir", "/tmp/q2playtest/ra2botvote", "scratch install root")
	port := flag.Int("port", 27971, "first server port")
	settle := flag.Int("settle", 90, "seconds to let a fill settle")
	flag.Parse()

	if *q2 == "" || *lib == "" || *glad == "" {
		fmt.Fprintln(os.Stderr,
			"usage: ra2botvote -q2proded <bin> -lib <game.so> -gladdir <checkout>")
		os.Exit(2)
	}

	aasPath = findAAS(*aas)
	d := time.Duration(*settle) * time.Second

	// ---------------------------------------------------------------- A
	fmt.Printf("\n##### arena, bots: 1 -- the control (port %d)\n", *port)
	srvA, a, _, err := phase("botson", arenaCfg(1, 0), *port, d,
		*q2, *ref, *ctf, *lib, *glad, *root)
	if err != nil {
		fmt.Fprintf(os.Stderr, "phase A could not run: %v\n", err)
		os.Exit(2)
	}
	lineA := botfillLine(srvA)
	srvA.Stop()

	check("bots-on/the fill seated bots", a.Bots > 0,
		fmt.Sprintf("%d bot(s), %d client(s), slots %v", a.Bots, a.Clients, a.Slots))
	check("bots-on/botfill names an arena",
		strings.Contains(lineA, "arena 1") &&
			!strings.Contains(lineA, "bots switched off"),
		lineA)

	// ---------------------------------------------------------------- B
	fmt.Printf("\n##### arena, bots: 0 -- the arena refuses (port %d)\n", *port+1)
	srvB, b, markB, err := phase("botsoff", arenaCfg(0, 0), *port+1, d,
		*q2, *ref, *ctf, *lib, *glad, *root)
	if err != nil {
		fmt.Fprintf(os.Stderr, "phase B could not run: %v\n", err)
		os.Exit(2)
	}
	lineB := botfillLine(srvB)
	joined := srvB.GrepFrom(markB, `joined the .* team|entered the game`)
	srvB.Stop()

	// The world: no bot is on the server.  This is the assertion; everything
	// below it is corroboration.
	check("bots-off/no bot was seated", b.Bots == 0,
		fmt.Sprintf("%d bot(s), %d client(s), slots %v", b.Bots, b.Clients, b.Slots))
	check("bots-off/nobody joined an arena team", len(joined) == 0,
		fmt.Sprintf("%d join line(s) in the console", len(joined)))
	// The diagnostic: and the zero is THIS feature's, not the fill being off.
	check("bots-off/botfill says which zero this is",
		strings.Contains(lineB, "bots switched off") &&
			strings.Contains(lineB, "want=0"),
		lineB)

	// ---------------------------------------------------------------- C
	fmt.Printf("\n##### allowvotingbots: 1 -- the row is offered (port %d)\n", *port+2)
	firstC, rowsC, err := proposeRows("voteon", arenaCfg(1, 1), *port+2,
		*q2, *ref, *ctf, *lib, *glad, *root, nil)
	if err != nil {
		fmt.Fprintf(os.Stderr, "phase C could not run: %v\n", err)
		os.Exit(2)
	}
	check("vote-on/the propose menu opened", hasRow(rowsC, "rounds"),
		fmt.Sprintf("%d row(s): %v", len(rowsC), rowsC))
	check("vote-on/Allow Bots is offered", hasRow(rowsC, "allow bots"),
		fmt.Sprintf("%d row(s)", len(rowsC)))
	// Where it sits, not just that it exists: a row behind "(More)" is a row
	// most players never scroll to, which is why it was moved up beside Falling
	// Damage rather than left at the end of the list.
	check("vote-on/...on the first page", hasRow(firstC, "allow bots"),
		fmt.Sprintf("page 1 is %d row(s), ending %q", len(firstC), firstC[len(firstC)-1]))

	// ---------------------------------------------------------------- D
	fmt.Printf("\n##### allowvotingbots: 0 -- the row is withheld (port %d)\n", *port+3)
	_, rowsD, err := proposeRows("voteoff", arenaCfg(1, 0), *port+3,
		*q2, *ref, *ctf, *lib, *glad, *root, nil)
	if err != nil {
		fmt.Fprintf(os.Stderr, "phase D could not run: %v\n", err)
		os.Exit(2)
	}
	// The positive control matters more here than in C: a menu that failed to
	// open draws no rows at all, and "no Allow Bots row" would pass on it.
	check("vote-off/the propose menu opened", hasRow(rowsD, "rounds"),
		fmt.Sprintf("%d row(s): %v", len(rowsD), rowsD))
	check("vote-off/Allow Bots is withheld", !hasRow(rowsD, "allow bots"),
		fmt.Sprintf("%d row(s)", len(rowsD)))

	// ---------------------------------------------------------------- E
	//
	// The eviction path, which none of the phases above reach: the bots that
	// were ALREADY in the arena when the switch moved.  Driven through the
	// admin form of the same menu (`arenaadmin <code> <arena>`, mode 0) because
	// that is the one route a scripted client can take to flip the setting on a
	// running server -- the vote form needs a second player to ratify it, and
	// the two paths converge on the same settings write and the same sweep.
	fmt.Printf("\n##### bots switched off under a running fill (port %d)\n", *port+4)
	e, err := evict(*port+4, *q2, *ref, *ctf, *lib, *glad, *root)
	if err != nil {
		fmt.Fprintf(os.Stderr, "phase E could not run: %v\n", err)
		os.Exit(2)
	}
	// The positive control: the fill really had put bots in there, so "none
	// afterwards" is a change and not a server that never filled.
	check("evict/bots were in the arena", e.before > 1,
		fmt.Sprintf("here=%d before (the client plus %d bot(s))", e.before, e.before-1))
	check("evict/only the client is left", e.after == 1,
		fmt.Sprintf("here=%d after", e.after))
	// AND THEY LEFT THE SERVER, not just the arena.  q2dm1 is one arena, so
	// there is nowhere for a bot voted out of it to go -- and a bot parked in
	// arena 0 would hold a client slot for the rest of the map with nothing
	// able to collect it, which is the state this half exists to
	// prevent.  `bots` here is the server-wide FL_BOT count, not the arena's.
	check("evict/no bot is left on the server", e.botsAfter == 0,
		fmt.Sprintf("%d bot(s) connected, %d before", e.botsAfter, e.botsBefore))
	check("evict/the sweep said so", e.moved > 0,
		fmt.Sprintf("%d 'leaves arena' line(s) in the console", e.moved))

	// ---------------------------------------------------------------- F
	//
	// R-RA-13: the same arena.cfg as phase D -- which withholds the row -- plus
	// the server cvar.  If the row is offered here, the cvar reached past all
	// three of the file's layers, which is the whole requirement. D is this
	// phase's control and shares its config exactly, so a difference between
	// them can only be the cvar.
	fmt.Printf("\n##### allowvotingbots: 0 + ra_allowvotingbots 1 (port %d)\n", *port+5)
	firstF, rowsF, err := proposeRows("votecvar", arenaCfg(1, 0), *port+5,
		*q2, *ref, *ctf, *lib, *glad, *root,
		map[string]string{"ra_allowvotingbots": "1"})
	if err != nil {
		fmt.Fprintf(os.Stderr, "phase F could not run: %v\n", err)
		os.Exit(2)
	}
	check("vote-cvar/the propose menu opened", hasRow(rowsF, "rounds"),
		fmt.Sprintf("%d row(s)", len(rowsF)))
	check("vote-cvar/Allow Bots is offered despite the file",
		hasRow(rowsF, "allow bots"), fmt.Sprintf("%d row(s)", len(rowsF)))
	check("vote-cvar/...on the first page", hasRow(firstF, "allow bots"),
		fmt.Sprintf("page 1 is %d row(s)", len(firstF)))

	// ---------------------------------------------------------------- F
	//
	// The scheduler proper, and the only phase that needs a map with more than one
	// arena on it.  Everything above runs on q2dm1, where "the fill serves one
	// arena" and "the fill serves every arena" are the same sentence.
	fmt.Printf("\n##### two crowded arenas, one fill (port %d)\n", *port+5)
	if _, err := os.Stat(*ra2ref + "/arena.cfg"); err != nil || aasPath == "" {
		why := "no ra2map9.aas -- run with -aas"
		if err != nil {
			why = "no RA2 install -- run with -ra2ref"
		}
		check("crowded/skipped", true, why)
	} else {
		a1, a2, err := crowded(*port+5, *q2, *ref, *ra2ref, *lib, *glad, *root)
		if err != nil {
			fmt.Fprintf(os.Stderr, "phase F could not run: %v\n", err)
			os.Exit(2)
		}
		// The control: arena 1 is what the old "follow the people" answer
		// filled, and it must still fill, or this measures nothing.
		check("crowded/arena 1 was filled", a1 > 1,
			fmt.Sprintf("here=%d (10 spawn points)", a1))
		// The claim: so was the other one.
		check("crowded/arena 2 was filled too", a2 > 1,
			fmt.Sprintf("here=%d (12 spawn points)", a2))

		// ------------------------------------------------------------ G
		//
		// The dynamic half: the fill has to TRACK the people, not just find
		// them once.  An arena the last person walked out of should end up
		// with no bots in it at all, and the one they walked into should fill.
		fmt.Printf("\n##### the people move, the bots follow (port %d)\n", *port+6)
		f, err := follows(*port+6, *q2, *ref, *ra2ref, *lib, *glad, *root)
		if err != nil {
			fmt.Fprintf(os.Stderr, "phase G could not run: %v\n", err)
			os.Exit(2)
		}
		check("follow/arena 1 was filled first", f.a1Before > 2,
			fmt.Sprintf("here=%d, arena 2 had %d", f.a1Before, f.a2Before))
		check("follow/the abandoned arena emptied", f.a1After == 0,
			fmt.Sprintf("here=%d after everyone left it", f.a1After))
		check("follow/the new arena filled", f.a2After > 1,
			fmt.Sprintf("here=%d", f.a2After))
	}

	fmt.Printf("\n%d check(s), %d failed\n", 19, failed)
	if failed > 0 {
		os.Exit(1)
	}
}

// crowded is the scheduler's own phase, and it needs a REAL RA2 map: two arenas, both
// with people in them, both short of bots.
//
// ra2map9 is the one the tree's own comments already measure against.  Its
// arenas 1 and 2 are `pickup: 1` -- so a bot can join one without inventing a
// team -- and they declare DIFFERENT capacities, 10 spawn points against 12, so
// the two fill targets are not the same number and a fill that served only one
// of them cannot be mistaken for one that served both.
//
// The discriminator is arena 2.  "Follow the people" resolves to the
// LOWEST-numbered populated arena, so before the scheduler every bot the fill
// added walked into arena 1 however short arena 2 was, and arena 2 sat at its
// one human for the whole map.  Both arenas holding bots is the claim.
func crowded(port int, q2, baseq2, ra2ref, lib, glad, root string) (int, int, error) {
	d := fmt.Sprintf("%s/crowded", root)

	// The RA2 paks ride in on the `ctf` argument, which links every pak*.pak it
	// is given as pak8 upward -- above baseq2, which is where the ra2map*.bsp
	// have to be to win.  The gamedir is still called "colosseum" so that
	// InstallBrain and the rest of the harness find it.
	if err := colosseum.Install(d, baseq2, ra2ref, lib); err != nil {
		return 0, 0, err
	}
	if err := colosseum.InstallBrain(d, glad); err != nil {
		return 0, 0, err
	}

	// The authentic 1999 arena.cfg, unedited: this phase is about the fill, and
	// `bots` defaults to 1 in code, so there is nothing it needs to say.
	cfg, err := os.ReadFile(ra2ref + "/arena.cfg")
	if err != nil {
		return 0, 0, fmt.Errorf("arena.cfg: %w", err)
	}
	os.Remove(d + "/colosseum/arena.cfg")
	if err := os.WriteFile(d+"/colosseum/arena.cfg", cfg, 0o644); err != nil {
		return 0, 0, err
	}

	// A bot with no reachability file never finishes initialising, so it stays
	// `pending` forever and the fill stalls -- which would fail this phase for a
	// reason that has nothing to do with it.
	if src, err := os.ReadFile(aasPath); err == nil {
		os.MkdirAll(d+"/colosseum/maps", 0o755)
		os.WriteFile(d+"/colosseum/maps/ra2map9.aas", src, 0o644)
	} else {
		return 0, 0, fmt.Errorf("no ra2map9.aas at %s: %w", aasPath, err)
	}

	srv := &playtest.Server{
		Binary: q2, Dir: d, Game: "colosseum", Map: "ra2map9",
		Port: port, MaxClients: 12,
		Cvars: map[string]string{
			"g_ruleset": "arena", "cheats": "1", "skill": "1",
			"botfill": "1", "minimumplayers": "0", "bots_minplayers": "0",
			// `arena` IS DELIBERATELY NOT SET.  It used to default to "1",
			// which RA_BotFillArena reads as an operator naming arena 1 and
			// obeys literally -- so the scheduler was never consulted on a
			// server that had not exec'd configs/arena.cfg, and this phase had
			// to set it by hand to test anything.  The default is "0" now, and
			// leaving it out is what checks that: if any of the four readers of
			// that cvar goes back to "1", whichever runs first creates it as 1
			// and arena 2 goes empty again.
		},
		LogPath: d + "/server.log",
	}
	if err := srv.Start(); err != nil {
		return 0, 0, err
	}
	defer srv.Stop()

	// One person in each of the two pickup arenas.
	for i, side := range []int{1, 2} {
		b := playtest.NewBot(fmt.Sprintf("human%d", i+1), "127.0.0.1", port)
		if err := b.Start(60 * time.Second); err != nil {
			return 0, 0, fmt.Errorf("client %d: %w", i+1, err)
		}
		defer b.Disconnect()
		if err := ra2.JoinTeam(b, ra2.PickupTeam(side, "Red")); err != nil {
			return 0, 0, fmt.Errorf("client %d join arena %d: %w", i+1, side, err)
		}
		b.WaitFrames(10, 5*time.Second)
	}

	// Let the fill work both arenas.  One bot per 32-frame tick and an AAS load
	// per bot on top, so this is the slowest phase by a distance.
	var a1, a2 int
	deadline := time.Now().Add(240 * time.Second)
	for time.Now().Before(deadline) {
		srv.Console("sv arenadump")
		time.Sleep(3 * time.Second)
		if n := arenaHere(srv, 1); n > a1 {
			a1 = n
		}
		if n := arenaHere(srv, 2); n > a2 {
			a2 = n
		}
		if a1 > 1 && a2 > 1 {
			break // both arenas served: the claim is settled
		}
	}

	return a1, a2, nil
}

// followResult is phase G: what happened to the bots when the last person in
// an arena left it for another one.
type followResult struct {
	a1Before, a2Before int
	a1After, a2After   int
}

// follows is the DYNAMIC half of the scheduler.  Filling two crowded arenas is one
// claim; tracking the people as they move between arenas is the other, and it
// is the one an operator actually watches happen.
//
// The move is done as a leave and a join rather than by walking one client
// through the menus: RA2 has no console command for switching arenas, and what
// is being measured -- "the last person in arena 1 is gone, and there is now a
// person in arena 2" -- is the same state either way.
func follows(port int, q2, baseq2, ra2ref, lib, glad, root string) (followResult, error) {
	var r followResult

	d := fmt.Sprintf("%s/follows", root)
	if err := colosseum.Install(d, baseq2, ra2ref, lib); err != nil {
		return r, err
	}
	if err := colosseum.InstallBrain(d, glad); err != nil {
		return r, err
	}
	cfg, err := os.ReadFile(ra2ref + "/arena.cfg")
	if err != nil {
		return r, fmt.Errorf("arena.cfg: %w", err)
	}
	os.Remove(d + "/colosseum/arena.cfg")
	if err := os.WriteFile(d+"/colosseum/arena.cfg", cfg, 0o644); err != nil {
		return r, err
	}
	src, err := os.ReadFile(aasPath)
	if err != nil {
		return r, fmt.Errorf("no ra2map9.aas at %s: %w", aasPath, err)
	}
	os.MkdirAll(d+"/colosseum/maps", 0o755)
	os.WriteFile(d+"/colosseum/maps/ra2map9.aas", src, 0o644)

	srv := &playtest.Server{
		Binary: q2, Dir: d, Game: "colosseum", Map: "ra2map9",
		// Deliberately tight.  Every bot is a library load, and this phase has
		// to fill an arena, empty it again and fill another -- so the point is
		// made with five bots rather than eleven.
		Port: port, MaxClients: 6,
		Cvars: map[string]string{
			"g_ruleset": "arena", "cheats": "1", "skill": "1",
			"botfill": "1", "minimumplayers": "0", "bots_minplayers": "0",
		},
		LogPath: d + "/server.log",
	}
	if err := srv.Start(); err != nil {
		return r, err
	}
	defer srv.Stop()

	// A person in arena 1, and nobody in arena 2.
	a := playtest.NewBot("first", "127.0.0.1", port)
	if err := a.Start(60 * time.Second); err != nil {
		return r, fmt.Errorf("first client: %w", err)
	}
	if err := ra2.JoinTeam(a, ra2.PickupTeam(1, "Red")); err != nil {
		return r, fmt.Errorf("first join: %w", err)
	}

	deadline := time.Now().Add(180 * time.Second)
	for time.Now().Before(deadline) {
		srv.Console("sv arenadump")
		time.Sleep(3 * time.Second)
		if n := arenaHere(srv, 1); n > r.a1Before {
			r.a1Before = n
		}
		if r.a1Before > 2 { // the person and at least two bots
			break
		}
	}
	r.a2Before = arenaHere(srv, 2)

	// The person leaves for arena 2.
	a.Disconnect()
	time.Sleep(2 * time.Second)

	b := playtest.NewBot("second", "127.0.0.1", port)
	if err := b.Start(60 * time.Second); err != nil {
		return r, fmt.Errorf("second client: %w", err)
	}
	defer b.Disconnect()
	if err := ra2.JoinTeam(b, ra2.PickupTeam(2, "Red")); err != nil {
		return r, fmt.Errorf("second join: %w", err)
	}

	// Arena 1 drains one bot a tick and arena 2 fills one a tick, and every
	// arrival is a library load, so this is the slowest wait in the scenario.
	r.a1After, r.a2After = 99, 0
	deadline = time.Now().Add(240 * time.Second)
	for time.Now().Before(deadline) {
		srv.Console("sv arenadump")
		time.Sleep(3 * time.Second)
		if n := arenaHere(srv, 1); n >= 0 && n < r.a1After {
			r.a1After = n
		}
		if n := arenaHere(srv, 2); n > r.a2After {
			r.a2After = n
		}
		if r.a1After == 0 && r.a2After > 1 {
			break
		}
	}

	return r, nil
}

// evictResult is what phase E measures either side of the switch moving.
type evictResult struct {
	before, after         int // RA_ArenaPlayers for arena 1, from `sv arenadump`
	botsBefore, botsAfter int // FL_BOT clients on the whole server
	moved, removed        int // what the sweep's own lines say it did
}

// evict fills an arena with bots, turns its `bots` switch off through the admin
// menu, and measures what became of them.
func evict(port int, q2, ref, ctf, lib, glad, root string) (evictResult, error) {
	var r evictResult
	d := fmt.Sprintf("%s/evict", root)
	if err := colosseum.Install(d, ref, ctf, lib); err != nil {
		return r, err
	}
	if err := colosseum.InstallBrain(d, glad); err != nil {
		return r, err
	}
	path := d + "/colosseum/arena.cfg"
	os.Remove(path)
	if err := os.WriteFile(path, []byte(arenaCfg(1, 0)), 0o644); err != nil {
		return r, err
	}

	srv := &playtest.Server{
		Binary: q2, Dir: d, Game: "colosseum", Map: "q2dm1",
		Port: port, MaxClients: 12,
		Cvars: map[string]string{
			"g_ruleset": "arena", "cheats": "1", "skill": "1",
			"botfill": "1", "minimumplayers": "0", "bots_minplayers": "0",
			"admincode": "1234",
		},
		LogPath: d + "/server.log",
	}
	if err := srv.Start(); err != nil {
		return r, err
	}
	defer srv.Stop()

	b := playtest.NewBot("admin", "127.0.0.1", port)
	if err := b.Start(30 * time.Second); err != nil {
		return r, err
	}
	defer b.Disconnect()

	if err := ra2.JoinTeam(b, ra2.PickupTeam(1, "Red")); err != nil {
		return r, fmt.Errorf("join: %w", err)
	}

	// Let the fill work.  `here` counts everyone on a team in arena 1, so it
	// starts at 1 -- this client -- and rises as bots are seated.
	r.before = 1
	deadline := time.Now().Add(120 * time.Second)
	for time.Now().Before(deadline) {
		srv.Console("sv arenadump")
		time.Sleep(2 * time.Second)
		if n := arenaHere(srv, 1); n > r.before {
			r.before = n
		}
		if r.before > 1 {
			break
		}
	}

	// Re-read immediately before the toggle rather than trusting the sample the
	// loop broke on: the loop stops at the FIRST bot it sees and the fill keeps
	// working, so that first number understates what is actually in the arena
	// when the switch moves -- and it is the "after" it has to be compared with.
	srv.Console("sv arenadump")
	srv.Console("sv ruleset")
	time.Sleep(2 * time.Second)
	if n := arenaHere(srv, 1); n > 0 {
		r.before = n
	}
	if c, err := colosseum.ParseBots(strings.Join(srv.Log(), "\n")); err == nil {
		r.botsBefore = c.Bots
	}

	mark := srv.Len()

	// The admin form of the settings menu for this arena.
	b.Cmd("arenaadmin 1234 1")
	if err := b.WaitMenu(`(?i)arena admin menu`, 20*time.Second); err != nil {
		return r, fmt.Errorf("admin menu: %w", err)
	}
	// Activating a yes/no row toggles it; the row starts YES here.
	if err := pickScrolling(b, `(?i)allow bots.*YES`, 30*time.Second); err != nil {
		return r, fmt.Errorf("toggle: %w", err)
	}
	time.Sleep(500 * time.Millisecond)
	if err := pickScrolling(b, `(?i)^apply$`, 30*time.Second); err != nil {
		return r, fmt.Errorf("apply: %w", err)
	}

	// The sweep runs on the 32-frame tick, and a bot it removes goes through a
	// full client disconnect.  Give it several ticks.
	time.Sleep(8 * time.Second)
	srv.Console("sv arenadump")
	srv.Console("sv ruleset")
	time.Sleep(2 * time.Second)
	r.after = arenaHere(srv, 1)
	if c, err := colosseum.ParseBots(strings.Join(srv.Log(), "\n")); err == nil {
		r.botsAfter = c.Bots
	}

	// The sweep says which of the two things it did to each bot, and on a
	// one-arena map every one of them should be the second.
	r.moved = len(srv.GrepFrom(mark, `leaves arena \d+: bots are switched off`))
	r.removed = len(srv.GrepFrom(mark, `leaves the server: no arena will have bots`))
	return r, nil
}
