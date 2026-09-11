// botfill -- does the bot count follow the GAME rather than the server, and
// does the switch being off still mean the flat count?
//
// ONE cvar since spec 1.36.  It was `ra_botfill`, `ctf_botfill`
// and `dm_botfill` -- three names for one concept, spelled
// per ruleset, under a rule that never covered them: that rule
// governs cvars a DONOR named and all three were Colosseum's own.  The switch
// is `botfill` now and what stays per ruleset is the TARGET.
//
// None of it can be checked from the source, which is why this exists:
// the target is COMPUTED every tick -- from the map's spawn pools, the arena's
// declared size, or `team_maxplayers` -- and `sv ruleset`'s `botfill` row is
// the only place it is written down.
//
// Five servers, because the interesting claims are different:
//
//   A  dm / q2dm1 / maxclients 12 / botfill 1
//      The arithmetic AND the fill reaching it: q2dm1 carries 10
//      `info_player_deathmatch`, the random selector refuses the two nearest a
//      player, so the pool is 8 -- and the server must settle at 8 players and
//      STAY there, sampled across several 32-frame fill ticks.  Then two people
//      arrive and the removal arm has to give the seats back.
//
//   B  dm / q2dm1 / maxclients 12 / botfill 0 / bots_minplayers 4
//      The control.  Off must mean off: the row says `bots_minplayers 4 is the
//      target` and the count settles at 4, not at 8.  Without this row, a fill
//      that ignored its own switch would pass every check in A.  The cvar is
//      `bots_minplayers` and not `minimumplayers` because `dm` is OSP's
//      RegularDM since 1.36, and the OSP four use tourney's bot cvars
// -- which is this scenario's only migration.
//
//   C  ctf / q2ctf1 / maxclients 4 / botfill 1
//      The arithmetic on a BIG map, cheaply.  q2ctf1's three pools are 17
//      shared and 12/14 per base, so `seats` is 2*min(17/2,12,14) = 16 -- and
//      `want` is 4, because `maxclients` is the ceiling and it is latched at 4.
//      Both numbers are printed, which is the only way to tell a clamp from a
//      miscalculation.
//
//   D  ctf / q2ctf4 / maxclients 12 / botfill 1
//      The case where the SHARED pool binds rather than a base: q2ctf4 has 7
//      shared spawn points against bases of 12 and 10, so seats is 4 on a map
//      whose bases would claim 8v8.  Cheap enough to also check what the target
//      being even is FOR -- the two sides equal, and still equal after two
//      people arrive and two bots are removed to seat them.
//
//   E  tdm / q2dm1 / maxclients 12 / botfill 1 / team_maxplayers 3
//      The half that was deferred and the flattening closed.
//      `tdm` and `duel` DECLARE a capacity, so their target is
//      `2 * team_maxplayers` and not a count read off the map -- six here, on a
//      map whose spawn pool would say eight.  That difference is the whole
//      point: it is the one row where the map and the ruleset disagree and the
//      ruleset has to win.
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

	"q2playtest/colosseum"
	"q2playtest/playtest"
)

// The two rows this scenario is about, out of `sv ruleset`:
//
//	botfill      dm want=8 from seats=8, spawns=8
//	botfill      ctf want=4 from seats=16, shared=17 base=12+14
//	botfill      tdm want=6 from 2 * team_maxplayers 3
//	botfill      off -- bots_minplayers 4 is the target
//	bots         8 bot(s) of 8 client(s) in 12 slot(s), at 0,1,2,3,4,5,6,7
//	botplace     ctf red=2 blue=2 noteam=0 teamskin=4, FL_BOTCLIENT=4
//
// ONE shape, where there were three.  Four arms printing four layouts is how a
// reader of this file ends up maintaining three regexes and a gap between them,
// which is exactly what happened before 1.36 unified the line.
var (
	reFillOn  = regexp.MustCompile(`^botfill\s+(\S+)\s+want=(\d+)\s+from\s+(.*)$`)
	reFillOff = regexp.MustCompile(`^botfill\s+off\s+--\s+(\S+)\s+(\d+)\s+is the target`)
	reBots    = regexp.MustCompile(`^bots\s+(\d+)\s+bot\(s\)\s+of\s+(\d+)\s+client\(s\)`)
	rePlace   = regexp.MustCompile(`^botplace\s+ctf\s+red=(\d+)\s+blue=(\d+)\s+noteam=(\d+)`)
	reSeats   = regexp.MustCompile(`seats=(\d+)`)
)

type row struct {
	on       bool
	ruleset  string
	want     int
	seats    int
	detail   string // `spawns=8` or `shared=17 base=12+14`
	offCvar  string
	offValue int

	bots, clients   int
	red, blue, noteam int
	haveBots        bool
	havePlace       bool
}

// One `sv ruleset` block's bot rows.  Read from the LAST `ruleset ` line in the
// log so a scenario can ask again.
func read(srv *playtest.Server) (row, error) {
	n := srv.Len()
	srv.Console("sv ruleset")
	if _, err := srv.WaitLog("world        frame", 10*time.Second); err != nil {
		return row{}, fmt.Errorf("sv ruleset never answered: %w", err)
	}
	// The bot rows come AFTER `world`, so the awaited line is not the last one.
	time.Sleep(500 * time.Millisecond)

	var r row
	for _, l := range srv.GrepFrom(n, `.`) {
		l = strings.TrimSpace(playtest.Decode(l))
		if m := reFillOn.FindStringSubmatch(l); m != nil {
			r.on, r.ruleset, r.detail = true, m[1], m[3]
			r.want, _ = strconv.Atoi(m[2])
			// `seats=` is the map's own number, printed beside the clamped one
			// so a clamp can be told from a miscalculation.  The two rulesets
			// that read a declared capacity instead have no `seats=`.
			if sm := reSeats.FindStringSubmatch(m[3]); sm != nil {
				r.seats, _ = strconv.Atoi(sm[1])
			}
			continue
		}
		if m := reFillOff.FindStringSubmatch(l); m != nil {
			r.on, r.offCvar = false, m[1]
			r.offValue, _ = strconv.Atoi(m[2])
			continue
		}
		if m := reBots.FindStringSubmatch(l); m != nil {
			r.haveBots = true
			r.bots, _ = strconv.Atoi(m[1])
			r.clients, _ = strconv.Atoi(m[2])
			continue
		}
		if m := rePlace.FindStringSubmatch(l); m != nil {
			r.havePlace = true
			r.red, _ = strconv.Atoi(m[1])
			r.blue, _ = strconv.Atoi(m[2])
			r.noteam, _ = strconv.Atoi(m[3])
		}
	}
	if !r.haveBots {
		return r, fmt.Errorf("`sv ruleset` printed no `bots` row -- is `bots 1`?")
	}
	return r, nil
}

// Wait for the client count to stop moving.  A fill tick is every 32 frames and
// a bot's ClientBegin waits for its library to report initialised, so "settled"
// is the only honest way to sample this -- and holding still across several
// ticks is itself half of what the two fills claim (the add and the remove
// arm settle on ONE number).
// `placed` additionally requires every bot to be ON A SIDE, and under ctf it is
// not optional.  `clients` counts a client that has CONNECTED, and a bot's
// ClientBegin is held back until its library reports initialised (twenty seconds
// of frames on a cold map), so a ctf server reaches its client count with every
// bot still CTF_NOTEAM -- the team is assigned inside CTFStartClient, which
// runs from the deferred begin.  A "the sides are equal" check that samples
// there reads red=0 blue=0 and passes on nothing, which is what the first run of
// this scenario did.
func settle(srv *playtest.Server, want int, ticks int, placed bool, d time.Duration) ([]row, error) {
	var hist []row
	deadline := time.Now().Add(d)
	stable := 0
	for time.Now().Before(deadline) {
		r, err := read(srv)
		if err != nil {
			return hist, err
		}
		hist = append(hist, r)
		ok := r.clients == want
		if placed && r.red+r.blue != r.bots {
			ok = false
		}
		if ok {
			stable++
			if stable >= ticks {
				return hist, nil
			}
		} else {
			stable = 0
		}
		time.Sleep(4 * time.Second)
	}
	last := hist[len(hist)-1]
	if placed && last.clients == want {
		return hist, fmt.Errorf("settled at %d client(s) but %d of %d bot(s) never "+
			"reached a side (red=%d blue=%d noteam=%d)", want,
			last.bots-last.red-last.blue, last.bots, last.red, last.blue, last.noteam)
	}
	return hist, fmt.Errorf("never settled at %d client(s) -- last was %d", want,
		last.clients)
}

type check struct {
	name string
	ok   bool
	note string
}

func report(cs []check) int {
	bad := 0
	for _, c := range cs {
		mark := "ok  "
		if !c.ok {
			mark, bad = "FAIL", bad+1
		}
		fmt.Printf("  [%s] %s", mark, c.name)
		if c.note != "" {
			fmt.Printf("  -- %s", c.note)
		}
		fmt.Println()
	}
	return bad
}

type phase struct {
	label      string
	ruleset    string
	mapname    string
	maxclients int
	cvars      map[string]string
}

func boot(q2, ref, ctf, lib, glad, dir string, port int, p phase) (*playtest.Server, error) {
	os.RemoveAll(dir)
	if err := colosseum.Install(dir, ref, ctf, lib); err != nil {
		return nil, err
	}
	if err := colosseum.InstallBrain(dir, glad); err != nil {
		return nil, err
	}
	cv := map[string]string{
		"g_ruleset": p.ruleset, "skill": "1", "admincode": "0",
		// Both cvars exist under every ruleset and only one is the
		// authority, so BOTH are set on every server: a row that passed because
		// the other name happened to hold the right number would be no evidence.
		"minimumplayers": "0", "bots_minplayers": "0",
		// One switch since spec 1.36.  Setting the three retired names as well
		// would be worse than useless: they are not registered, so a server
		// that started obeying one again would look identical from here.
		"botfill": "0",
	}
	for k, v := range p.cvars {
		cv[k] = v
	}
	srv := &playtest.Server{
		Binary: q2, Dir: dir, Game: "colosseum", Map: p.mapname, Port: port,
		MaxClients: p.maxclients, LogPath: filepath.Join(dir, "server.log"),
		Cvars:      cv,
	}
	if err := srv.Start(); err != nil {
		return nil, err
	}
	return srv, nil
}

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "retail baseq2 paks")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "Threewave paks")
	lib := flag.String("lib", "", "game library under test")
	glad := flag.String("gladdir", "", "gladiator-bot-restored checkout (the brain)")
	dir := flag.String("dir", "/tmp/q2playtest/botfill", "scratch install root")
	port := flag.Int("port", 27994, "first server port")
	secs := flag.Int("settle", 180, "seconds to wait for a fill to settle")
	only := flag.String("only", "", "run only these phases, e.g. A,C")
	flag.Parse()

	if *q2 == "" || *lib == "" || *glad == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded, -lib and -gladdir")
		os.Exit(2)
	}
	bad, err := run(*q2, *ref, *ctf, *lib, *glad, *dir, *port, *secs, *only)
	if err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	fmt.Printf("\nbotfill: %d check(s) failed\n", bad)
	if bad > 0 {
		os.Exit(1)
	}
}

func want(p string, only string) bool {
	return only == "" || strings.Contains(only, p)
}

func run(q2, ref, ctf, lib, glad, root string, port, secs int, only string) (int, error) {
	settleFor := time.Duration(secs) * time.Second
	bad := 0

	// ---- A: dm, the fill reaching the map's own number --------------------
	if want("A", only) {
		fmt.Println("== A. dm / q2dm1 / maxclients 12 / botfill 1 ==")
		srv, err := boot(q2, ref, ctf, lib, glad, filepath.Join(root, "a"), port,
			phase{ruleset: "dm", mapname: "q2dm1", maxclients: 12,
				cvars: map[string]string{"botfill": "1"}})
		if err != nil {
			return bad, err
		}
		hist, serr := settle(srv, 8, 3, false, settleFor)
		var cs []check
		if len(hist) == 0 {
			srv.Stop()
			return bad, fmt.Errorf("A: no sample at all: %v", serr)
		}
		last := hist[len(hist)-1]
		cs = append(cs,
			check{"dm/the botfill row is present and on", last.on && last.ruleset == "dm",
				fmt.Sprintf("row=%v ruleset=%q", last.on, last.ruleset)},
			check{"dm/seats is the map's pool, not its count", last.seats == 8,
				fmt.Sprintf("seats=%d, want 8 (q2dm1 carries 10, the selector refuses 2)", last.seats)},
			check{"dm/spawns is reported", strings.Contains(last.detail, "spawns=8"),
				fmt.Sprintf("detail=%q", last.detail)},
			check{"dm/want is seats, under a maxclients of 12", last.want == 8,
				fmt.Sprintf("want=%d", last.want)},
			check{"dm/the server actually filled to it", serr == nil,
				fmt.Sprintf("clients=%d bots=%d over %d sample(s)", last.clients, last.bots, len(hist))},
		)
		// Held still: the add and the remove arm must agree on one number.
		if serr == nil {
			same := true
			for _, h := range hist[len(hist)-3:] {
				if h.clients != 8 {
					same = false
				}
			}
			cs = append(cs, check{"dm/it holds still across three fill ticks", same,
				counts(hist)})
		}
		// ...and gives seats back when people arrive.
		//
		// *** A CLIENT UNDER AN OSP RULESET IS AN OBSERVER UNTIL IT ENTERS. ***
		// The same finding the ctf phase below records, one ruleset over and for
		// a different donor's reason: `dm` is OSP's RegularDM since spec 1.36, a
		// client connects as an observer and enters by pressing a key, and
		// BotCountsAsPlayer asks for `resp.osp_entered == ENTERED_ENTERED`
		//So two clients that connect and sit there do not raise
		// the census and the fill is right not to remove anybody -- this row
		// asserted otherwise on its first run after the flattening and was
		// wrong, not the game.  `join` is OSP's own command, and under `dm` and
		// `dmpro` it is the one a person presses (osp_clientcmd.c gates it on
		// !OSP_IsTeams()).
		if serr == nil {
			var bots []*playtest.Bot
			joinerr := ""
			for i := 0; i < 2; i++ {
				b := playtest.NewBot(fmt.Sprintf("person%d", i), "127.0.0.1", port)
				if err := b.Start(60 * time.Second); err != nil {
					joinerr = err.Error()
					break
				}
				b.Cmd("join")
				bots = append(bots, b)
			}
			if joinerr != "" {
				cs = append(cs, check{"dm/two people can join a filled server", false, joinerr})
			} else {
				// 9, not 8: the OSP arms settle at want+1 once people are on
				// the server -- see the check below.  `settle` waits for the
				// count it is given, so asking for 8 here made the row fail
				// with the right numbers in its own message.
				h2, e2 := settle(srv, 9, 2, false, settleFor)
				l2 := h2[len(h2)-1]
				// *** AND THE OSP ARMS SETTLE ONE PLAYER ABOVE THE TARGET. ***
				// Two people cost ONE bot here, not two, and that is tourney's
				// own arithmetic rather than a rounding error: its removal test
				// is `(numplayers - bots_votedin - 1) > want` where every other
				// ruleset's is `numplayers > want`, so the two arms add up to
				// `want` and remove down to `want + 1` -- which bl_spawn.c's own
				// comment says in as many words.  Tourney preserves its own
				// bot contract rather than redesigning it, and `dm` is inside
				// that contract since spec 1.36.
				//
				// So: 8 bots, two people join, one bot leaves, and it holds at 7
				// bots and 9 clients.  `ctf` below is the control for this very
				// asymmetry -- it uses the other arm and does give both seats
				// back, 2 bots and 4 clients.
				cs = append(cs,
					check{"dm/two people arriving cost one bot (the OSP arm)",
						e2 == nil && l2.bots == 7 && l2.clients == 9,
						fmt.Sprintf("bots=%d clients=%d (want 7 and 9: OSP removes down to want+1)",
							l2.bots, l2.clients)})
			}
			for _, b := range bots {
				b.Disconnect()
			}
		}
		bad += report(cs)
		srv.Stop()
		port++
	}

	// ---- B: the control.  Off means off. ----------------------------------
	if want("B", only) {
		fmt.Println("\n== B. dm / q2dm1 / maxclients 12 / botfill 0 / bots_minplayers 4 (control) ==")
		srv, err := boot(q2, ref, ctf, lib, glad, filepath.Join(root, "b"), port,
			phase{ruleset: "dm", mapname: "q2dm1", maxclients: 12,
				cvars: map[string]string{"botfill": "0", "bots_minplayers": "4"}})
		if err != nil {
			return bad, err
		}
		hist, serr := settle(srv, 4, 3, false, settleFor)
		last := hist[len(hist)-1]
		bad += report([]check{
			{"dm-off/the row says off and names the flat cvar",
				!last.on && last.offCvar == "bots_minplayers" && last.offValue == 4,
				fmt.Sprintf("on=%v cvar=%q value=%d", last.on, last.offCvar, last.offValue)},
			{"dm-off/the count is the flat one, not the map's", serr == nil && last.clients == 4,
				fmt.Sprintf("clients=%d bots=%d, want 4 (the map's pool is 8)", last.clients, last.bots)},
		})
		srv.Stop()
		port++
	}

	// ---- C: ctf's arithmetic on a big map, and the maxclients clamp -------
	if want("C", only) {
		fmt.Println("\n== C. ctf / q2ctf1 / maxclients 4 / botfill 1 ==")
		srv, err := boot(q2, ref, ctf, lib, glad, filepath.Join(root, "c"), port,
			phase{ruleset: "ctf", mapname: "q2ctf1", maxclients: 4,
				cvars: map[string]string{"botfill": "1"}})
		if err != nil {
			return bad, err
		}
		hist, serr := settle(srv, 4, 2, true, settleFor)
		last := hist[len(hist)-1]
		bad += report([]check{
			{"ctf/the botfill row is present and on", last.on && last.ruleset == "ctf",
				fmt.Sprintf("row=%v ruleset=%q", last.on, last.ruleset)},
			{"ctf/seats is 2*min(shared/2, base1, base2)", last.seats == 16,
				fmt.Sprintf("seats=%d, want 16", last.seats)},
			{"ctf/the three pools are reported",
				strings.Contains(last.detail, "shared=17 base=12+14"),
				fmt.Sprintf("detail=%q, want \"shared=17 base=12+14\"", last.detail)},
			{"ctf/want is clamped to maxclients, and says so separately", last.want == 4,
				fmt.Sprintf("want=%d of seats=%d", last.want, last.seats)},
			{"ctf/the server filled to the clamped target", serr == nil && last.clients == 4,
				fmt.Sprintf("clients=%d bots=%d", last.clients, last.bots)},
		})
		srv.Stop()
		port++
	}

	// ---- D: the shared pool binding, and what an even target is FOR -------
	if want("D", only) {
		fmt.Println("\n== D. ctf / q2ctf4 / maxclients 12 / botfill 1 ==")
		srv, err := boot(q2, ref, ctf, lib, glad, filepath.Join(root, "d"), port,
			phase{ruleset: "ctf", mapname: "q2ctf4", maxclients: 12,
				cvars: map[string]string{"botfill": "1"}})
		if err != nil {
			return bad, err
		}
		hist, serr := settle(srv, 4, 3, true, settleFor)
		var cs []check
		last := hist[len(hist)-1]
		cs = append(cs,
			check{"ctf-small/the shared pool binds, not a base", last.seats == 4,
				fmt.Sprintf("seats=%d, want 4 -- bases of 10 and 8 would claim 8v8",
					last.seats)},
			check{"ctf-small/the three pools are reported",
				strings.Contains(last.detail, "shared=5 base=10+8"),
				fmt.Sprintf("detail=%q, want \"shared=5 base=10+8\"", last.detail)},
			check{"ctf-small/the server filled to it", serr == nil && last.clients == 4,
				fmt.Sprintf("clients=%d bots=%d", last.clients, last.bots)},
			// Both halves, because red==blue is true of an empty board: every
			// bot has to be ON a side before "the sides are equal" means
			// anything at all.
			check{"ctf-small/every bot reached a side",
				last.havePlace && last.bots > 0 && last.red+last.blue == last.bots,
				fmt.Sprintf("red=%d blue=%d noteam=%d of %d bot(s)",
					last.red, last.blue, last.noteam, last.bots)},
			check{"ctf-small/...and the two sides are equal",
				last.havePlace && last.red == last.blue && last.red > 0,
				fmt.Sprintf("red=%d blue=%d", last.red, last.blue)},
		)
		if serr == nil {
			// *** A CTF CLIENT THAT HAS NOT PICKED A TEAM IS AN OBSERVER, AND
			// AN OBSERVER IS NOT A PLAYER. ***  G_IsObserver() is
			// `ctf_team == CTF_NOTEAM` under ctf and BotCountsAsPlayer excludes
			// it, which is the Gladiator SDK's own exclusion -- so two
			// clients that connect and sit in the join menu do not raise the
			// census and the fill is right not to remove anybody.  The first run
			// of this scenario asserted otherwise and was wrong, not the game.
			// `team red` / `team blue` is Threewave's own command.
			var bots []*playtest.Bot
			joinerr := ""
			for i := 0; i < 2; i++ {
				b := playtest.NewBot(fmt.Sprintf("person%d", i), "127.0.0.1", port)
				if err := b.Start(60 * time.Second); err != nil {
					joinerr = err.Error()
					break
				}
				b.Cmd(fmt.Sprintf("team %s", []string{"red", "blue"}[i]))
				bots = append(bots, b)
			}
			if joinerr != "" {
				cs = append(cs, check{"ctf-small/two people can join", false, joinerr})
			} else {
				h2, e2 := settle(srv, 4, 2, false, settleFor)
				l2 := h2[len(h2)-1]
				cs = append(cs,
					check{"ctf-small/two people arriving cost two bots",
						e2 == nil && l2.bots == 2 && l2.clients == 4,
						fmt.Sprintf("bots=%d clients=%d (want 2 and 4)", l2.bots, l2.clients)},
					// The removal is what CTFBotFillName exists for: `removebot`
					// with no name takes the lowest client slot, which is a bot
					// on whichever side connected first.
					check{"ctf-small/...and the sides are still level",
						l2.havePlace && abs(l2.red-l2.blue) <= 1,
						fmt.Sprintf("red=%d blue=%d", l2.red, l2.blue)})
			}
			for _, b := range bots {
				b.Disconnect()
			}
		}
		bad += report(cs)
		srv.Stop()
	}

	// ---- E: the ruleset that DECLARES its capacity ------------------------
	//
	// It was recorded that `team_maxplayers` is a capacity
	// tourney already declares -- exactly what ctf and dm lacked and had to read
	// off the map -- and then declined to use it, because tourney preserves
	// tourney's bot contract rather than redesigning it.  Spec 1.36 closed that:
	// once `match_mode 0` became `dm`, the same ruleset was both the rule's
	// subject and its exception.
	//
	// So `tdm` fills to `2 * team_maxplayers`, and this row is worth having
	// because it is the one place the map and the ruleset DISAGREE: q2dm1's
	// pool says 8, `team_maxplayers 3` says 6, and the ruleset has to win.  A
	// fill that quietly fell back to the map-sized answer would settle at 8 and
	// look perfectly healthy.
	if want("E", only) {
		fmt.Println("\n== E. tdm / q2dm1 / maxclients 12 / botfill 1 / team_maxplayers 3 ==")
		srv, err := boot(q2, ref, ctf, lib, glad, filepath.Join(root, "e"), port,
			phase{ruleset: "tdm", mapname: "q2dm1", maxclients: 12,
				cvars: map[string]string{
					"botfill": "1", "team_maxplayers": "3",
					"match_countdown": "14",
				}})
		if err != nil {
			return bad, err
		}
		hist, serr := settle(srv, 6, 2, false, settleFor)
		last := hist[len(hist)-1]
		bad += report([]check{
			{"tdm/the row is present, on, and names this ruleset",
				last.on && last.ruleset == "tdm",
				fmt.Sprintf("on=%v ruleset=%q", last.on, last.ruleset)},
			{"tdm/the target is 2 * team_maxplayers", last.want == 6,
				fmt.Sprintf("want=%d, expected 6 from team_maxplayers 3", last.want)},
			{"tdm/...and NOT the map's spawn pool", last.seats == 0 &&
				strings.Contains(last.detail, "team_maxplayers"),
				fmt.Sprintf("seats=%d detail=%q -- q2dm1's pool is 8", last.seats, last.detail)},
			{"tdm/the fill reaches it", serr == nil && last.clients == 6,
				fmt.Sprintf("clients=%d bots=%d, want 6", last.clients, last.bots)},
		})
		srv.Stop()
	}

	return bad, nil
}

func abs(n int) int {
	if n < 0 {
		return -n
	}
	return n
}

func counts(hist []row) string {
	var b strings.Builder
	b.WriteString("clients:")
	for _, h := range hist {
		fmt.Fprintf(&b, " %d", h.clients)
	}
	return b.String()
}
