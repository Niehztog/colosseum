// ospmenu -- drive OSP Tourney's pop-up menu with real clients.
//
// The tourney menu is a LAYOUT (svc_layout), not the statusbar RA2 draws into,
// so everything here reads Bot.Layout(). Four questions, and the fourth is the
// one worth the harness:
//
//  1. does the menu open, and is the layout it sends WELL FORMED -- every token
//     complete, every quote closed (truncation is on a whole item, never
//     mid-token);
//  2. does the cursor move on invnext/invprev, which is also the check that the
//     rate-limited redraw still reaches the client at all;
//  3. is `inven` a toggle -- close, then reopen (a build where closing destroys
//     what reopening needs passes an "it opens" check);
//  4. CAN ONE CLIENT SEE ANOTHER CLIENT'S MENU STATE.  osp_menus.c stages every
//     menu's text into FILE-SCOPE GLOBALS from one client's `resp` immediately
//     before rendering, so whether the rows a client sees are its own depends
//     on the engine having taken a private copy. Two bots, one toggles its
//     per-client "Player ID" row, the other only moves its cursor -- and the
//     second must still see its OWN setting.
//
// Row 4 fails on a library without the per-client copy and passes with it,
// which is what makes it a test rather than a demonstration. Run it against
// both to see that.
package main

import (
	"flag"
	"fmt"
	"os"
	"regexp"
	"strings"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
)

var (
	q2proded = flag.String("q2proded", "", "dedicated server binary")
	ref      = flag.String("ref", "/usr/share/games/quake2/baseq2", "baseq2 paks")
	ctfref   = flag.String("ctf", "/usr/share/games/quake2/ctf", "threewave ctf paks")
	lib      = flag.String("lib", "", "game library under test")
	dir      = flag.String("dir", "/tmp/ospmenu-playtest", "scratch install dir")
	port     = flag.Int("port", 27990, "UDP port")
	ruleset  = flag.String("ruleset", "dm", "an OSP ruleset: dm, dmpro, tdm, duel")
	label    = flag.String("label", "", "tag for the output")
	verbose  = flag.Bool("v", false, "dump the layouts as they are read")
	game     = flag.String("game", "", "mod dir name; set it to drive a standalone OSP Tourney build instead of colosseum")
)

var failed bool

func check(name string, ok bool, note string, a ...any) bool {
	if len(a) > 0 {
		note = fmt.Sprintf(note, a...)
	}
	status := "FAIL"
	if ok {
		status = " ok "
	} else {
		failed = true
	}
	fmt.Printf("  [%s] %-34s %s\n", status, name, note)
	return ok
}

// ---------------------------------------------------------------- layout

// One drawn row: the text of a `string`/`string2` (or centred variant) token.
var reRow = regexp.MustCompile(`\bc?string2?\s+"([^"]*)"`)

func rows(layout string) []string {
	var out []string
	for _, m := range reRow.FindAllStringSubmatch(layout, -1) {
		out = append(out, m[1])
	}
	return out
}

// The cursor row is the one the engine prefixed with \x0d.
func cursor(layout string) (int, string) {
	for i, r := range rows(layout) {
		if strings.HasPrefix(r, "\x0d") {
			return i, strings.TrimPrefix(r, "\x0d")
		}
	}
	return -1, ""
}

// wellFormed consumes the whole layout as a sequence of complete statusbar
// tokens. Anything left over is a token the builder cut in half -- which is
// exactly what an unbounded snprintf that ran out of room leaves behind, and
// what the whole-item append exists to prevent.
var tokens = []*regexp.Regexp{
	regexp.MustCompile(`^\s*(?:xv|yv|xl|xr|yt|yb)\s+-?\d+`),
	regexp.MustCompile(`^\s*(?:picn|pic|stat_string)\s+\S+`),
	regexp.MustCompile(`^\s*c?string2?\s+"[^"]*"`),
	regexp.MustCompile(`^\s*(?:if|endif)\b`),
}

func wellFormed(layout string) (bool, string) {
	s := layout
	for strings.TrimSpace(s) != "" {
		matched := false
		for _, re := range tokens {
			if loc := re.FindStringIndex(s); loc != nil {
				s = s[loc[1]:]
				matched = true
				break
			}
		}
		if !matched {
			r := strings.TrimSpace(s)
			if len(r) > 48 {
				r = r[:48] + "..."
			}
			return false, fmt.Sprintf("stops parsing at %q", r)
		}
	}
	return true, fmt.Sprintf("%d rows, %d bytes, all tokens whole", len(rows(layout)), len(layout))
}

// ---------------------------------------------------------------- driving

// The mode banner on row 1 of whichever main menu the ruleset opens. NOT
// "OSP Tourney": the mod unicasts a four-line credits layout at connect whose
// first row is "OSP Tourney DM v(2.75)", so that substring finds the splash
// screen and reports it as a menu that opened.
const menuMark = `Regular DM Mode|Teamplay Mode|1v1 Mode`

// What the scenario sets referee_password to, and what it hands the `referee`
// client command to become one.
const refPassword = "letmein"

// `inven` is a TOGGLE, and that makes a naive retry loop actively harmful:
// send it, miss the redraw, send it again, and the second one CLOSES the menu
// the first one opened. The mod also debounces menu keys by two frames, which
// swallows sends and flips the parity the other way. So both helpers below
// LOOK FIRST and only press when the layout disagrees with what they want --
// a press is never issued against a state that is already correct.
//
// Getting this wrong does not fail loudly. It leaves the bot holding a layout
// from a moment when the menu WAS open while the server has since closed it,
// so the rows read fine and the next `invuse` falls through to item use. That
// looked exactly like a game that refused to open its admin menu.
func menuShowing(b *playtest.Bot) bool {
	return regexp.MustCompile(menuMark).MatchString(b.Layout())
}

func openMenu(b *playtest.Bot, d time.Duration) (string, error) {
	for deadline := time.Now().Add(d); ; {
		if menuShowing(b) {
			return b.Layout(), nil
		}
		if time.Now().After(deadline) {
			return "", fmt.Errorf("no tourney menu after %s", d)
		}
		b.Cmd("inven")
		time.Sleep(1200 * time.Millisecond)
	}
}

func closeMenu(b *playtest.Bot, d time.Duration) error {
	for deadline := time.Now().Add(d); ; {
		if !menuShowing(b) {
			return nil
		}
		if time.Now().After(deadline) {
			return fmt.Errorf("menu still open after %s", d)
		}
		b.Cmd("inven")
		time.Sleep(1200 * time.Millisecond)
	}
}

// settledCursor waits for the layout to stop changing, then reports which row
// the cursor is on.
//
// THE LAYOUT IN HAND IS NOT THE SERVER'S STATE. The redraw is rate limited --
// ClientEndServerFrame flushes at most five times a second -- so a bot that
// presses a cursor key and reads straight back is looking at the frame BEFORE
// its press. Acting on that is what makes a menu test lie: it reports the
// cursor on the row you wanted while the server has it a row or two further
// on, and the `invuse` that follows selects something else entirely. Settle
// first, every time.
func settledCursor(b *playtest.Bot) (string, bool) {
	last, stable := b.Layout(), 0
	for i := 0; i < 40; i++ {
		time.Sleep(100 * time.Millisecond)
		now := b.Layout()
		if now == last {
			if stable++; stable >= 3 { // ~300ms with nothing new
				_, txt := cursor(now)
				return txt, true
			}
			continue
		}
		last, stable = now, 0
	}
	_, txt := cursor(last)
	return txt, false
}

// pressCursor sends one cursor command and waits until the cursor is observed
// on a row OTHER than `from`. Settling alone is not enough here for the same
// reason it is not enough in waitCursor: a settled layout only proves nothing
// new has arrived recently, not that this particular press is reflected in it.
func pressCursor(b *playtest.Bot, cmd string, from int) (int, string) {
	b.Cmd(cmd)
	for j := 0; j < 8; j++ {
		settledCursor(b)
		if i, txt := cursor(b.Layout()); i >= 0 && i != from {
			return i, txt
		}
	}
	i, txt := cursor(b.Layout())
	return i, txt
}

// waitCursor presses `cmd` until the cursor lands on a row matching re, with
// EXACTLY ONE PRESS OUTSTANDING at a time.
//
// Settling is necessary and not sufficient. A loop that presses, settles, and
// presses again runs ahead of the server whenever a press is still queued: it
// sees a stable layout with the cursor on the row it wants, stops, and the
// press already in flight walks the cursor one row further before `invuse`
// arrives. That is not a flake -- it is deterministic and it silently selects
// the WRONG ROW, which is the worst way for a menu test to fail. So after each
// press, wait until the cursor is observed somewhere NEW before pressing again;
// then nothing is in flight when the caller acts.
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

// waitRow polls for the row matching re to stop reading `was`. The redraw is
// rate limited -- ClientThink flushes at most five times a second -- so a fixed
// sleep after a menu action is a flake generator: it fails when the flush lands
// on the wrong side of it and passes on the next run. Poll instead, and let the
// caller assert on what comes back.
func waitRow(b *playtest.Bot, re, was string, d time.Duration) string {
	deadline := time.Now().Add(d)
	for {
		got := rowText(b.Layout(), re)
		if got != "" && got != was {
			return got
		}
		if time.Now().After(deadline) {
			return got
		}
		time.Sleep(100 * time.Millisecond)
	}
}

// rowText returns the first drawn row matching re, cursor marker stripped.
func rowText(layout, re string) string {
	rx := regexp.MustCompile(re)
	for _, r := range rows(layout) {
		r = strings.TrimPrefix(r, "\x0d")
		if rx.MatchString(r) {
			return r
		}
	}
	return ""
}

func main() {
	flag.Parse()
	if *q2proded == "" || *lib == "" {
		fmt.Println("need -q2proded and -lib")
		os.Exit(2)
	}
	d := *dir
	// allow_id 2 and 3 are the server-locked settings, which null the Player ID
	// row's SelectFunc -- and check 4 needs a row it can pick. 1 leaves it to
	// the client.
	cv := map[string]string{
		"cheats": "1", "allow_id": "1", "deathmatch": "1",
		// BOTH are needed: OSP_referee_cmd refuses with "Referee mode is
		// disabled on this server" before it ever looks at the password.
		"referee_enable":   "1",
		"referee_password": refPassword,
	}
	gamedir, mapname := "colosseum", "q2dm1"

	if *game != "" {
		// Standalone OSP Tourney (the donor this engine came from): one mod,
		// no ruleset dispatch, and `match_mode` where colosseum has g_ruleset.
		// 0 is free play, which is the mode whose main menu is RegDM_Menu.
		gamedir = *game
		cv["match_mode"] = "0"
		if err := playtest.Install(d, gamedir, *ref, *lib); err != nil {
			fmt.Println("install:", err)
			os.Exit(2)
		}
	} else {
		rs, ok := colosseum.Find(*ruleset)
		if !ok || !colosseum.IsOSP(*ruleset) {
			fmt.Printf("%s is not an OSP ruleset\n", *ruleset)
			os.Exit(2)
		}
		mapname = rs.Map
		cv["g_ruleset"] = rs.Name
		for k, v := range rs.Cvars {
			cv[k] = v
		}
		if err := colosseum.Install(d, *ref, *ctfref, *lib); err != nil {
			fmt.Println("install:", err)
			os.Exit(2)
		}
	}
	srv := &playtest.Server{
		Binary: *q2proded, Dir: d, Game: gamedir, Map: mapname,
		Port: *port, MaxClients: 8, Cvars: cv, LogPath: d + "/server.log",
	}
	if err := srv.Start(); err != nil {
		fmt.Println("server:", err)
		os.Exit(2)
	}
	defer srv.Stop()

	tag := *label
	if tag == "" {
		tag = *lib
	}
	fmt.Printf("\n##### osp menu on %s / %s   (%s)\n", gamedir, mapname, tag)

	a := playtest.NewBot("alpha", "127.0.0.1", *port)
	if err := a.Start(30 * time.Second); err != nil {
		fmt.Println("alpha:", err)
		os.Exit(2)
	}
	defer a.Disconnect()
	b := playtest.NewBot("bravo", "127.0.0.1", *port)
	if err := b.Start(30 * time.Second); err != nil {
		fmt.Println("bravo:", err)
		os.Exit(2)
	}
	defer b.Disconnect()
	a.WaitFrames(10, 5*time.Second)

	// ---- 1. the menu opens, and what it sent is a complete layout
	la, err := openMenu(a, 8*time.Second)
	if !check("menu/opens", err == nil, "%v", errOr(err, "alpha has a tourney menu")) {
		report()
		return
	}
	if *verbose {
		fmt.Printf("  --- alpha layout (%d bytes) ---\n  %s\n  --- rows: %q\n",
			len(la), strings.ReplaceAll(la, "\x0d", "<>"), rows(la))
	}
	okForm, note := wellFormed(la)
	check("menu/wellformed", okForm, "%s", note)
	check("menu/quotes", strings.Count(la, `"`)%2 == 0,
		"%d quote characters", strings.Count(la, `"`))

	// ---- 2. the cursor moves
	settledCursor(a)
	i0, t0 := cursor(a.Layout())
	i1, t1 := pressCursor(a, "invnext", i0)
	check("menu/cursor-next", i0 >= 0 && i1 >= 0 && i0 != i1,
		"row %d %q -> row %d %q", i0, t0, i1, t1)
	i2, _ := pressCursor(a, "invprev", i1)
	check("menu/cursor-prev", i2 == i0, "back to row %d (from %d)", i2, i0)

	// ---- 3. inven is a toggle, in both directions
	err = closeMenu(a, 6*time.Second)
	check("menu/closes", err == nil && len(rows(a.Layout())) <= 1,
		"%v, %d rows drawn", errOr(err, "closed"), len(rows(a.Layout())))
	_, err = openMenu(a, 8*time.Second)
	check("menu/reopens", err == nil, "%v", errOr(err, "menu came back"))

	// ---- 4. two clients, two menus, one set of globals
	if _, err := openMenu(b, 8*time.Second); err != nil {
		check("menu/two-clients", false, "bravo has no menu: %v", err)
		report()
		return
	}
	// Alpha picks its own Player ID row and toggles it. That row is per client
	// (resp.osp_r204) and its text is staged into a file-scope static.
	if _, ok := waitCursor(a, "invnext", `Player ID`, 24); !ok {
		check("menu/two-clients", false, "alpha could not reach the Player ID row")
		report()
		return
	}
	aBefore := rowText(a.Layout(), `Player ID`)
	bBefore := rowText(b.Layout(), `Player ID`)
	a.Cmd("invuse")
	mine := waitRow(a, `Player ID`, aBefore, 3*time.Second)
	if *verbose {
		fmt.Printf("  --- alpha after invuse (%d bytes) ---\n  %s\n",
			len(a.Layout()), strings.ReplaceAll(a.Layout(), "\x0d", "<>"))
	}
	check("menu/own-toggle", mine != "" && mine != aBefore,
		"alpha %q -> %q", aBefore, mine)

	// Bravo did not toggle anything. It only moves its cursor -- which is the
	// path that re-renders without restaging, and therefore the path that read
	// the globals alpha has just written.
	// Prove bravo actually REDREW before believing what its rows say. A client
	// that never got a new layout still shows the right text, and an isolation
	// check that cannot tell those apart passes on a broken build whenever the
	// redraw is late.
	bCur, _ := cursor(b.Layout())
	b.Cmd("invnext")
	moved := false
	for deadline := time.Now().Add(3 * time.Second); time.Now().Before(deadline); {
		if i, _ := cursor(b.Layout()); i >= 0 && i != bCur {
			moved = true
			break
		}
		time.Sleep(100 * time.Millisecond)
	}
	check("menu/bravo-redrew", moved, "cursor left row %d", bCur)
	theirs := rowText(b.Layout(), `Player ID`)
	check("menu/isolation", moved && theirs != "" && theirs == bBefore,
		"bravo sees %q (its own is %q, alpha's is now %q)", theirs, bBefore, mine)

	// ---- 5. the admin tree is referee-only, asserted in BOTH signs.
	//
	// The negative half alone is worth little: the "*Admin Menu" row is blank
	// for a non-referee, so a test that only checks bravo cannot see it passes
	// on a build that refuses everybody. The positive half -- a real referee
	// still gets in -- is what fails if the gate is too tight.
	check("admin/hidden-from-player", rowText(b.Layout(), `Admin Menu`) == "",
		"bravo's admin row is %q", rowText(b.Layout(), `Admin Menu`))

	a.Cmd("referee " + refPassword)
	if _, err := a.WaitPrint(`referee status`, 4*time.Second); err != nil {
		check("admin/becomes-referee", false,
			"no referee grant; last prints: %q", tailPrints(a, 3))
		report()
		return
	}
	check("admin/becomes-referee", true, "alpha took referee status")
	// Close and reopen, so the row is restaged from the new referee status.
	if err := closeMenu(a, 6*time.Second); err != nil {
		check("admin/shown-to-referee", false, "%v", err)
		report()
		return
	}
	if _, err := openMenu(a, 8*time.Second); err != nil {
		check("admin/shown-to-referee", false, "alpha lost its menu: %v", err)
		report()
		return
	}
	adminRow := rowText(a.Layout(), `Admin Menu`)
	check("admin/shown-to-referee", adminRow != "", "alpha's admin row is %q", adminRow)

	if _, ok := waitCursor(a, "invnext", `Admin Menu`, 24); !ok {
		check("admin/referee-opens", false, "referee could not reach the admin row")
		report()
		return
	}
	cur0, curTxt := cursor(a.Layout())
	a.Cmd("invuse")
	_, err = a.WaitLayout(`Main Admin Menu`, 4*time.Second)
	if err != nil {
		fmt.Printf("  --- cursor was row %d %q; layout after invuse:\n  %s\n",
			cur0, curTxt, strings.ReplaceAll(a.Layout(), "\x0d", "<>"))
		fmt.Printf("  --- alpha prints: %q\n", tailPrints(a, 4))
	}
	check("admin/referee-opens", err == nil, "%v", errOr(err, "referee is in the admin menu"))

	report()
}

func tailPrints(b *playtest.Bot, n int) []string {
	p := b.Prints()
	if len(p) > n {
		p = p[len(p)-n:]
	}
	return p
}

func errOr(err error, ok string) any {
	if err != nil {
		return err
	}
	return ok
}

func report() {
	if failed {
		fmt.Println("\nFAILED")
		os.Exit(1)
	}
	fmt.Println("\nall checks passed")
}
