// ospbotvote -- can the people playing vote the FILL's bots out, under the
// OSP four (R-OSP-16)?
//
// `arena` has had this since its per-arena `bots` switch became votable, and
// `scenarios/ra2botvote` is that half.  The OSP four had the rows -- `vote
// rembot <n>` at the console, "Remove random bots" in the bot menu -- and could
// not reach a single bot with either of them on a server that runs any, for two
// independent reasons.  Both are here, in both signs, because either one alone
// leaves the feature dead:
//
//	the CAP counted `bots_votedin`, the bots a VOTE had added, which is 0
//	on every server whose bots came from `botfill` or `bots_minplayers`
//
//	the REMOVAL did not hold: CheckMinimumPlayers compares the census
//	against the target every 32 frames, so what the vote took out came
//	back while the passing vote was still on the screen
//
// Four phases, and none of them is a demonstration -- each asserts a number
// that is different on a library without the change:
//
//	A  dm / q2dm1 / maxclients 12 / botfill 1 / vote_enable_bots 1
//	   The console form and the claim itself.  The fill settles at 8, one
//	   person joins, `vote rembot 3` passes -- and the server holds at FIVE
//	   across several fill ticks.  Without the target cut it returns to 8,
//	   which is what this phase is here to catch.  Then the cap is asked for
//	   in its own words (`vote rembot 6` with five bots left refuses and says
//	   five), and the rest are voted out, because reaching NONE is the parity
//	   with an arena that has voted its switch off.
//
//	B  dm / q2dm1 / maxclients 12 / botfill 1 -- THE MENU, which is where a
//	   player actually does this.  The "Remove random bots" row is stepped
//	   with ENTER and must leave 0 on a server whose bots are all the fill's;
//	   the old clamp was `bots_votedin` and could not.  Its control is the
//	   same server with `vote_enable_bots 0`, where the voting menu's bot row
//	   reads [LOCKED] instead of offering the page at all.
//
//	C  dm / q2dm1 / botfill 0 / bots_minplayers 4 -- the OTHER arm.  The flat
//	   count re-adds exactly as the fill does, and it is a cvar rather than a
//	   computed number, so it takes the cut in a different place in the
//	   source.  Two bots voted out of four, and it holds at two.
//
//	D  dm / q2dm1 / botfill 1 / vote_enable_bots 0 -- the gate, at the
//	   console.  The vote is refused by name and the bot count does not move.
//
// `sv ruleset`'s `botfill` row is read alongside the census in every phase,
// because the census says the server did something and the row says it was
// THIS feature: `want=` comes down by the vote's number and the row ends in
// `, N voted out` while it is holding.
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

// ---------------------------------------------------------------- reporting

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
		fmt.Printf("  [%s] %-52s", mark, c.name)
		if c.note != "" {
			fmt.Printf("  -- %s", c.note)
		}
		fmt.Println()
	}
	return bad
}

// ---------------------------------------------------------------- sv ruleset

// The three rows this scenario reads, out of one `sv ruleset`:
//
//	botfill      dm want=5 from seats=8, spawns=8, 3 voted out
//	botfill      off -- bots_minplayers 4 is the target, 2 voted out
//	bots         5 bot(s) of 6 client(s) in 12 slot(s), at 0,1,2,3,4
//
// The `, N voted out` clause is appended rather than spliced in, so these are
// the same two expressions `scenarios/botfill` anchors on.
var (
	reFillOn  = regexp.MustCompile(`^botfill\s+(\S+)\s+want=(\d+)\s+from\s+(.*)$`)
	reFillOff = regexp.MustCompile(`^botfill\s+off\s+--\s+(\S+)\s+(\d+)\s+is the target(.*)$`)
	reBots    = regexp.MustCompile(`^bots\s+(\d+)\s+bot\(s\)\s+of\s+(\d+)\s+client\(s\)`)
	reVoted   = regexp.MustCompile(`,\s*(\d+)\s+voted out`)
)

type row struct {
	on       bool
	ruleset  string
	want     int
	detail   string
	offCvar  string
	offValue int
	votedOut int

	bots, clients int
	haveBots      bool
}

// One `sv ruleset` block's bot rows, read from the console.
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
			if v := reVoted.FindStringSubmatch(m[3]); v != nil {
				r.votedOut, _ = strconv.Atoi(v[1])
			}
			continue
		}
		if m := reFillOff.FindStringSubmatch(l); m != nil {
			r.on, r.offCvar = false, m[1]
			r.offValue, _ = strconv.Atoi(m[2])
			if v := reVoted.FindStringSubmatch(m[3]); v != nil {
				r.votedOut, _ = strconv.Atoi(v[1])
			}
			continue
		}
		if m := reBots.FindStringSubmatch(l); m != nil {
			r.haveBots = true
			r.bots, _ = strconv.Atoi(m[1])
			r.clients, _ = strconv.Atoi(m[2])
		}
	}
	if !r.haveBots {
		return r, fmt.Errorf("`sv ruleset` printed no `bots` row -- is `bots 1`?")
	}
	return r, nil
}

// settle waits for the bot count to reach `bots` and then to HOLD it for
// `ticks` further samples.
//
// Holding is the whole assertion here and not a nicety: a vote that removes
// bots and a fill that seats them again both pass a check that samples once,
// and the interval between them is the 32-frame fill tick.  Sampling at four
// seconds means every sample crosses at least one.
func settle(srv *playtest.Server, bots, ticks int, d time.Duration) ([]row, error) {
	var hist []row
	deadline := time.Now().Add(d)
	stable := 0
	for time.Now().Before(deadline) {
		r, err := read(srv)
		if err != nil {
			return hist, err
		}
		hist = append(hist, r)
		if r.bots == bots {
			if stable++; stable >= ticks {
				return hist, nil
			}
		} else {
			stable = 0
		}
		time.Sleep(4 * time.Second)
	}
	if len(hist) == 0 {
		return hist, fmt.Errorf("no sample at all")
	}
	return hist, fmt.Errorf("never held at %d bot(s) for %d sample(s) -- saw %s",
		bots, ticks, counts(hist))
}

func counts(hist []row) string {
	var b []string
	for _, h := range hist {
		b = append(b, strconv.Itoa(h.bots))
	}
	return strings.Join(b, ",")
}

// ---------------------------------------------------------------- the server

type phase struct {
	ruleset    string
	mapname    string
	maxclients int
	cvars      map[string]string
}

func boot(q2, ref, ctf, lib, glad, aas, dir string, port int, p phase) (*playtest.Server, error) {
	os.RemoveAll(dir)
	if err := colosseum.Install(dir, ref, ctf, lib); err != nil {
		return nil, err
	}
	if err := colosseum.InstallBrain(dir, glad); err != nil {
		return nil, err
	}
	// Without the map's reachability mesh the brain answers "no AAS file
	// available" and destroys every bot that wanted it, so a fill measured
	// there is measuring the mesh.  Resolved once in run(), copied in here.
	if aas != "" {
		if src, err := os.ReadFile(aas); err == nil {
			maps := filepath.Join(dir, "colosseum", "maps")
			if err := os.MkdirAll(maps, 0o755); err != nil {
				return nil, err
			}
			if err := os.WriteFile(filepath.Join(maps, p.mapname+".aas"),
				src, 0o644); err != nil {
				return nil, err
			}
		}
	}
	cv := map[string]string{
		"g_ruleset": p.ruleset, "skill": "1", "admincode": "0",
		// Both names exist under every ruleset and only one is the authority,
		// so both are written: a row that passed because the other name held
		// the right number would be no evidence.
		"minimumplayers": "0", "bots_minplayers": "0",
		"botfill": "0",
		// Every one of these is a default this scenario's arithmetic depends
		// on.  Written down rather than inherited, so a row disabled somewhere
		// else does not read as a broken vote.
		"vote_enable": "1", "vote_enable_bots": "1", "vote_threshold": "51",
		"vote_bots_max": "8",
		// An observer may still use the console, and nothing here readies up.
		"match_strictmode": "0",
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

// A CLIENT UNDER AN OSP RULESET IS AN OBSERVER UNTIL IT ENTERS, and an
// observer's vote is refused outright wherever `vote_countspectators` is off.
// `join` is the command a person presses under `dm`; one is not reliable (see
// scenarios/ospenter), so it is resent until the server says so.
func enter(srv *playtest.Server, b *playtest.Bot) bool {
	in := regexp.QuoteMeta(b.Name) + ` (entered the game|joined team)`
	for i := 0; i < 10; i++ {
		if len(srv.Grep(in)) > 0 || b.PMType() == playtest.PMNormal {
			b.WaitFrames(5, 3*time.Second)
			return true
		}
		b.Cmd("join")
		b.WaitFrames(8, 4*time.Second)
	}
	return b.PMType() == playtest.PMNormal
}

// One person, connected and entered, on a server that has already filled.
func person(srv *playtest.Server, name string, port int) (*playtest.Bot, error) {
	b := playtest.NewBot(name, "127.0.0.1", port)
	if err := b.Start(60 * time.Second); err != nil {
		return nil, err
	}
	if !enter(srv, b) {
		b.Disconnect()
		return nil, fmt.Errorf("%s never entered the game", name)
	}
	return b, nil
}

// vote puts one proposal and reports what the server broadcast about it.  With
// one entered human the proposer is 100% of the voters -- bots are subtracted
// from OSP_votePercent's divisor -- so a vote that is going to pass passes on
// the spot, and one that does not is a refusal this returns the text of.
func vote(srv *playtest.Server, b *playtest.Bot, form string, a ...any) (log []string, mine []string) {
	n, pn := srv.Len(), len(b.Prints())
	b.Cmd("vote "+form, a...)
	b.WaitFrames(8, 5*time.Second)
	time.Sleep(1500 * time.Millisecond)
	return srv.GrepFrom(n, `.`), b.Prints()[pn:]
}

func joined(lines []string, re string) string {
	rx := regexp.MustCompile(re)
	for _, l := range lines {
		l = strings.TrimSpace(playtest.Decode(l))
		if rx.MatchString(l) {
			return l
		}
	}
	return ""
}

// ---------------------------------------------------------------- the menu

// The tourney menu is a LAYOUT, not the statusbar RA2 draws into.
var reRow = regexp.MustCompile(`\bc?string2?\s+"([^"]*)"`)

func rows(layout string) []string {
	var out []string
	for _, m := range reRow.FindAllStringSubmatch(layout, -1) {
		out = append(out, playtest.Decode(strings.TrimPrefix(m[1], "\x0d")))
	}
	return out
}

func cursor(layout string) (int, string) {
	for i, r := range reRow.FindAllStringSubmatch(layout, -1) {
		if strings.HasPrefix(r[1], "\x0d") {
			return i, playtest.Decode(strings.TrimPrefix(r[1], "\x0d"))
		}
	}
	return -1, ""
}

func rowText(layout, re string) string {
	rx := regexp.MustCompile(re)
	for _, r := range rows(layout) {
		if rx.MatchString(r) {
			return strings.TrimSpace(r)
		}
	}
	return ""
}

const menuMark = `Regular DM Mode|Teamplay Mode|1v1 Mode`

// THE LAYOUT IN HAND IS NOT THE SERVER'S STATE: the redraw is rate limited, so
// a client that presses and reads straight back is looking at the frame before
// its press.  Settle first, every time.
func settled(b *playtest.Bot) string {
	last, stable := b.Layout(), 0
	for i := 0; i < 40; i++ {
		time.Sleep(100 * time.Millisecond)
		now := b.Layout()
		if now == last {
			if stable++; stable >= 3 {
				return now
			}
			continue
		}
		last, stable = now, 0
	}
	return last
}

// `inven` is a TOGGLE, so a retry loop that presses blind closes what the
// previous press opened.  Look first, press only when the layout disagrees.
func openMenu(b *playtest.Bot, d time.Duration) (string, error) {
	rx := regexp.MustCompile(menuMark)
	for deadline := time.Now().Add(d); ; {
		if l := b.Layout(); rx.MatchString(l) {
			return l, nil
		}
		if time.Now().After(deadline) {
			return "", fmt.Errorf("no tourney menu after %s", d)
		}
		b.Cmd("inven")
		time.Sleep(1200 * time.Millisecond)
	}
}

// waitCursor presses `cmd` until the cursor lands on a row matching re, with
// EXACTLY ONE PRESS OUTSTANDING at a time -- a loop that runs ahead of the
// server selects the wrong row, deterministically and silently.
func waitCursor(b *playtest.Bot, cmd, re string, tries int) (string, bool) {
	rx := regexp.MustCompile(re)
	_, txt := cursor(settled(b))
	for i := 0; i < tries && !rx.MatchString(txt); i++ {
		prev := txt
		b.Cmd(cmd)
		for j := 0; j < 8; j++ {
			if _, t := cursor(settled(b)); t != prev {
				txt = t
				break
			}
		}
	}
	return txt, rx.MatchString(txt)
}

// Select the row the cursor is on and wait for the page it opens.
//
// ONE press, then a long poll, and at most one retry -- not a press-and-look
// loop.  A page that has already opened has a cursor of its own, so a second
// `invuse` sent while the first is still in flight selects a row on the NEW
// page: from the voting menu that stages a map vote, and the scenario then
// reports "the bot page did not open" about a server that opened it.
func pick(b *playtest.Bot, title string, d time.Duration) (string, bool) {
	rx := regexp.MustCompile(title)
	for try := 0; try < 2; try++ {
		b.Cmd("invuse")
		deadline := time.Now().Add(d / 2)
		for time.Now().Before(deadline) {
			if l := settled(b); rx.MatchString(strings.Join(rows(l), "\n")) {
				return l, true
			}
			time.Sleep(300 * time.Millisecond)
		}
	}
	return b.Layout(), false
}

// ---------------------------------------------------------------- phases

func want(p, only string) bool { return only == "" || strings.Contains(only, p) }

// The reachability mesh for one map, from -aas, from $Q2AAS, or from the two
// places this tree's own runs leave them.  "" means there is none, which the
// caller reports rather than works around.
func findAAS(explicit, mapname string) string {
	cand := []string{explicit, os.Getenv("Q2AAS"), "/tmp/osp-aas",
		"colosseum/maps", "/tmp/q2playtest/*/colosseum/maps"}
	for _, c := range cand {
		if c == "" {
			continue
		}
		ms, _ := filepath.Glob(c)
		for _, m := range ms {
			if fi, err := os.Stat(m); err == nil && fi.IsDir() {
				m = filepath.Join(m, mapname+".aas")
			}
			if fi, err := os.Stat(m); err == nil && !fi.IsDir() && fi.Size() > 0 {
				return m
			}
		}
	}
	return ""
}

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "retail baseq2 paks")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "Threewave paks")
	lib := flag.String("lib", "", "game library under test")
	glad := flag.String("gladdir", "", "gladiator-bot-restored checkout (the brain)")
	aas := flag.String("aas", "", "directory holding q2dm1.aas, or the file; searched for if unset")
	dir := flag.String("dir", "/tmp/q2playtest/ospbotvote", "scratch install root")
	port := flag.Int("port", 27984, "first server port")
	secs := flag.Int("settle", 180, "seconds to wait for a fill to settle")
	only := flag.String("only", "", "run only these phases, e.g. A,C")
	flag.Parse()

	if *q2 == "" || *lib == "" || *glad == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded, -lib and -gladdir")
		os.Exit(2)
	}
	bad, err := run(*q2, *ref, *ctf, *lib, *glad, *aas, *dir, *port, *secs, *only)
	if err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	fmt.Printf("\nospbotvote: %d check(s) failed\n", bad)
	if bad > 0 {
		os.Exit(1)
	}
}

func run(q2, ref, ctf, lib, glad, aasarg, root string, port, secs int, only string) (int, error) {
	settleFor := time.Duration(secs) * time.Second
	mesh := findAAS(aasarg, "q2dm1")
	if mesh == "" {
		return 0, fmt.Errorf("no q2dm1.aas found -- pass -aas <dir>; " +
			"a brain with no mesh destroys its bots and every phase here " +
			"would measure that instead")
	}
	bad := 0

	// ---- A: the console form, and that the removal HOLDS ------------------
	if want("A", only) {
		fmt.Println("== A. dm / q2dm1 / maxclients 12 / botfill 1 -- `vote rembot` reaches the fill's bots ==")
		srv, err := boot(q2, ref, ctf, lib, glad, mesh, filepath.Join(root, "a"), port,
			phase{ruleset: "dm", mapname: "q2dm1", maxclients: 12,
				cvars: map[string]string{"botfill": "1"}})
		if err != nil {
			return bad, err
		}
		var cs []check
		hist, serr := settle(srv, 8, 2, settleFor)
		if len(hist) == 0 {
			srv.Stop()
			return bad, fmt.Errorf("A: no sample at all: %v", serr)
		}
		first := hist[len(hist)-1]
		cs = append(cs, check{"the fill seats the map's own number first",
			serr == nil && first.want == 8,
			fmt.Sprintf("bots=%d want=%d from %q", first.bots, first.want, first.detail)})

		if serr == nil {
			p, perr := person(srv, "voter", port)
			if perr != nil {
				cs = append(cs, check{"a person can join the filled server", false, perr.Error()})
			} else {
				// The bots stay at 8 beside the person: the OSP arms add up to
				// `want` and remove down to `want + 1`.
				log, mine := vote(srv, p, "rembot 3")
				passed := joined(log, `Vote passed!`) != ""
				cs = append(cs, check{"the vote is put and carries", passed,
					pickText(joined(log, `Vote (passed|failed)|Proposal: `),
						joined(mine, `.`))})
				cs = append(cs, check{"...and the server says it removed three",
					joined(log, `3 bots removed!`) != "",
					pickText(joined(log, `bots? removed|no more bots`), "nothing was said")})

				// THE CLAIM.  Five, and five on the next tick, and the one
				// after that: without the target cut the fill reads the server
				// as short and is back at eight inside a few seconds.
				h2, e2 := settle(srv, 5, 3, settleFor)
				l2 := h2[len(h2)-1]
				cs = append(cs, check{"five bots are left and they STAY left",
					e2 == nil,
					fmt.Sprintf("bots over %d sample(s): %s", len(h2), counts(h2))})
				cs = append(cs, check{"...the target itself came down",
					l2.want == 5 && l2.votedOut == 3,
					fmt.Sprintf("want=%d votedout=%d from %q", l2.want, l2.votedOut, l2.detail)})

				// The cap, in its own words: five bots left, six refused, and
				// the number in the refusal is the census rather than
				// `bots_votedin` (which is 0 on this server and always was).
				_, mine2 := vote(srv, p, "rembot 6")
				cs = append(cs, check{"the cap is the bots that are there",
					joined(mine2, `You can remove only 5 more bots`) != "",
					pickText(joined(mine2, `You can remove|Sorry|disabled`), "nothing was printed")})

				// ...and it can reach NONE, which is the parity with an arena
				// that has voted its own switch off.
				log3, _ := vote(srv, p, "rembot 5")
				h3, e3 := settle(srv, 0, 3, settleFor)
				l3 := h3[len(h3)-1]
				cs = append(cs,
					check{"the last five can go too", joined(log3, `Vote passed!`) != "" && e3 == nil,
						fmt.Sprintf("bots over %d sample(s): %s", len(h3), counts(h3))},
					check{"...and the target is zero, not a fallback to the flat count",
						l3.want == 0 && l3.votedOut == 8,
						fmt.Sprintf("want=%d votedout=%d", l3.want, l3.votedOut)})
				p.Disconnect()
			}
		}
		bad += report(cs)
		srv.Stop()
		port++
	}

	// ---- B: the menu, which is where a player does this -------------------
	if want("B", only) {
		fmt.Println("\n== B. dm / q2dm1 / maxclients 12 / botfill 1 -- the bot menu's own row ==")
		srv, err := boot(q2, ref, ctf, lib, glad, mesh, filepath.Join(root, "b"), port,
			phase{ruleset: "dm", mapname: "q2dm1", maxclients: 12,
				cvars: map[string]string{"botfill": "1"}})
		if err != nil {
			return bad, err
		}
		var cs []check
		hist, serr := settle(srv, 8, 2, settleFor)
		if serr != nil {
			cs = append(cs, check{"the fill seats eight before the menu is read", false,
				fmt.Sprintf("%v", serr)})
			bad += report(cs)
			srv.Stop()
			port++
		} else {
			_ = hist
			p, perr := person(srv, "menuer", port)
			if perr != nil {
				cs = append(cs, check{"a person can join the filled server", false, perr.Error()})
			} else {
				if _, err := openMenu(p, 12*time.Second); err != nil {
					cs = append(cs, check{"the tourney menu opens", false, err.Error()})
				} else {
					// main menu -> Voting Menu -> Gladiator Bots...
					_, ok := waitCursor(p, "invnext", `Voting Menu`, 24)
					cs = append(cs, check{"the voting menu is offered", ok,
						rowText(p.Layout(), `Voting Menu`)})
					if ok {
						l, opened := pick(p, `\[ Voting Menu \]`, 8*time.Second)
						cs = append(cs, check{"...and opens", opened,
							pickText(rowText(l, `Gladiator Bots`), "no bot row drawn")})
						botrow := rowText(l, `Gladiator Bots`)
						cs = append(cs, check{"the bot page is unlocked by vote_enable_bots 1",
							strings.Contains(botrow, "..."), pickText(botrow, "no row")})

						if opened && strings.Contains(botrow, "...") {
							_, ok = waitCursor(p, "invnext", `Gladiator Bots`, 24)
							if !ok {
								cs = append(cs, check{"the cursor reaches the bot row", false,
									rowText(p.Layout(), `Gladiator Bots`)})
							} else {
								l, opened = pick(p, `Gladiator Bots Menu`, 8*time.Second)
								cs = append(cs, check{"the bot menu opens", opened,
									pickText(rowText(l, `Total active bots`), "no page")})
								cs = append(cs, check{"...and it can see the fill's bots",
									rowText(l, `Total active bots: 8`) != "",
									pickText(rowText(l, `Total active bots`), "no count row")})

								// THE MENU CLAIM.  ENTER steps "Remove random
								// bots" and the value must leave 0: the clamp
								// was `bots_votedin`, which is 0 here, so the
								// row could not be moved at all and the page
								// was decoration on the one server that has
								// bots to remove.
								_, ok = waitCursor(p, "invnext", `Remove random bots`, 24)
								if !ok {
									cs = append(cs, check{"the cursor reaches the remove row", false,
										rowText(p.Layout(), `Remove random bots`)})
								} else {
									before := rowText(p.Layout(), `Remove random bots`)
									p.Cmd("invuse")
									time.Sleep(1500 * time.Millisecond)
									after := rowText(settled(p), `Remove random bots`)
									cs = append(cs, check{"the remove row steps off zero",
										after != "" && !strings.HasSuffix(after, ": 0"),
										fmt.Sprintf("%q -> %q", before, after)})
								}
							}
						}
					}
				}
				p.Disconnect()
			}

			// The control, on the same server: the operator's gate closes and
			// the page stops being offered at all.
			srv.Console("set vote_enable_bots 0")
			time.Sleep(600 * time.Millisecond)
			p2, perr2 := person(srv, "locked", port)
			if perr2 != nil {
				cs = append(cs, check{"a second person can join", false, perr2.Error()})
			} else {
				if _, err := openMenu(p2, 12*time.Second); err != nil {
					cs = append(cs, check{"the menu opens for the control", false, err.Error()})
				} else if _, ok := waitCursor(p2, "invnext", `Voting Menu`, 24); ok {
					l, _ := pick(p2, `\[ Voting Menu \]`, 8*time.Second)
					botrow := rowText(l, `Gladiator Bots`)
					cs = append(cs, check{"vote_enable_bots 0 locks the page",
						strings.Contains(botrow, "[LOCKED]"), pickText(botrow, "no row drawn")})
				}
				p2.Disconnect()
			}
			bad += report(cs)
			srv.Stop()
			port++
		}
	}

	// ---- C: the other arm -- a flat count re-adds exactly the same way ----
	if want("C", only) {
		fmt.Println("\n== C. dm / q2dm1 / botfill 0 / bots_minplayers 4 -- the flat count takes the cut too ==")
		srv, err := boot(q2, ref, ctf, lib, glad, mesh, filepath.Join(root, "c"), port,
			phase{ruleset: "dm", mapname: "q2dm1", maxclients: 12,
				cvars: map[string]string{"botfill": "0", "bots_minplayers": "4"}})
		if err != nil {
			return bad, err
		}
		var cs []check
		hist, serr := settle(srv, 4, 2, settleFor)
		if len(hist) == 0 {
			srv.Stop()
			return bad, fmt.Errorf("C: no sample at all: %v", serr)
		}
		last := hist[len(hist)-1]
		cs = append(cs, check{"the flat count seats four, with the switch off",
			serr == nil && !last.on && last.offCvar == "bots_minplayers" && last.offValue == 4,
			fmt.Sprintf("bots=%d, row: off=%v %s %d", last.bots, !last.on, last.offCvar, last.offValue)})

		if serr == nil {
			p, perr := person(srv, "voter", port)
			if perr != nil {
				cs = append(cs, check{"a person can join", false, perr.Error()})
			} else {
				log, _ := vote(srv, p, "rembot 2")
				cs = append(cs, check{"the vote carries", joined(log, `Vote passed!`) != "",
					pickText(joined(log, `Vote (passed|failed)|Proposal: `), "nothing was declared")})
				h2, e2 := settle(srv, 2, 3, settleFor)
				l2 := h2[len(h2)-1]
				cs = append(cs,
					check{"two bots are left and they STAY left", e2 == nil,
						fmt.Sprintf("bots over %d sample(s): %s", len(h2), counts(h2))},
					check{"...and the off row says what it took off",
						l2.votedOut == 2 && l2.offValue == 4,
						fmt.Sprintf("%s %d is the target, votedout=%d", l2.offCvar, l2.offValue, l2.votedOut)})
				p.Disconnect()
			}
		}
		bad += report(cs)
		srv.Stop()
		port++
	}

	// ---- D: the gate, at the console --------------------------------------
	if want("D", only) {
		fmt.Println("\n== D. dm / q2dm1 / botfill 1 / vote_enable_bots 0 -- the gate (control) ==")
		srv, err := boot(q2, ref, ctf, lib, glad, mesh, filepath.Join(root, "d"), port,
			phase{ruleset: "dm", mapname: "q2dm1", maxclients: 12,
				cvars: map[string]string{"botfill": "1", "vote_enable_bots": "0"}})
		if err != nil {
			return bad, err
		}
		var cs []check
		hist, serr := settle(srv, 8, 2, settleFor)
		if len(hist) == 0 {
			srv.Stop()
			return bad, fmt.Errorf("D: no sample at all: %v", serr)
		}
		if serr != nil {
			cs = append(cs, check{"the fill seats eight", false, counts(hist)})
		} else {
			p, perr := person(srv, "voter", port)
			if perr != nil {
				cs = append(cs, check{"a person can join", false, perr.Error()})
			} else {
				_, mine := vote(srv, p, "rembot 3")
				cs = append(cs, check{"the vote is refused by name",
					joined(mine, `Bot voting is currently disabled`) != "",
					pickText(joined(mine, `disabled|You can remove|Vote`), "nothing was printed")})
				h2, e2 := settle(srv, 8, 2, settleFor)
				cs = append(cs, check{"...and the bots are all still there", e2 == nil,
					fmt.Sprintf("bots over %d sample(s): %s", len(h2), counts(h2))})
				p.Disconnect()
			}
		}
		bad += report(cs)
		srv.Stop()
	}

	return bad, nil
}

func pickText(a, b string) string {
	if strings.TrimSpace(a) != "" {
		return strings.TrimSpace(a)
	}
	return strings.TrimSpace(b)
}
