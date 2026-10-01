// hardening -- the crash and injection fixes, driven from a client.
//
// Each row is one of the defects a client, or a server's own configuration,
// could reach, and each is asked of the running game in the terms it failed
// in: a line a teammate was made to execute, a server that stopped answering,
// a counter that went back to zero.  Every row ends on Server.Alive -- the
// process has not exited AND answers a console command now -- because
// q2proded prints nothing when it dies, and "no crash in the log" is what a
// dead server's log says too.
//
//	C2  dm on an RA2 map: a teleporter pad with no target is FREED, with one
//	    line each, instead of left to G_Find(NULL, ...) the first time
//	    somebody steps on it; under arena the same pads are kept, because
//	    arena's own touch handler is what they are for (R-RA-14).
//	C3  tdm: a teamskin carrying a command separator is refused, and the
//	    teammate the donor stuffed it into receives nothing of it; a valid
//	    one still reaches them, which is the control; a team name keeps no
//	    separator (R-SEC-11).
//	C4  dmpro with referee_enable 1 and no referee_password: both referee
//	    paths answer "disabled" and the server lives -- the cvar was
//	    registered NULL, and `_is_referee` dereferenced it.
//	H10 a password-protected server: a bot the console adds still joins,
//	    while a person without the password is refused and one with it is
//	    let in (R-BOT-14).  Needs -gladdir.
//	H12 dm: the shots a player fired survive their death -- the accuracy
//	    record was cleared at every respawn (R-OSP-19).
//	H19 dmpro: a second wrong referee password inside the backoff is refused
//	    without being tried, and the right one is accepted after it
//	    (R-SEC-12).
//	H20 arena: a client cycling its skin allocates no image index -- every
//	    new skin name was one gi.imageindex, and a client could walk the
//	    server to the table's end (R-SEC-13).
//
// C1, C5, H9, H11, H14 and H18 are not rows here, and each for a reason
// stated where the fix is: they need a map position, a match in progress or a
// held item that a headless client cannot be steered to reliably.
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

var (
	q2proded = flag.String("q2proded", "", "dedicated server binary")
	ref      = flag.String("ref", "/usr/share/games/quake2/baseq2", "baseq2 paks")
	ctfref   = flag.String("ctf", "/usr/share/games/quake2/ctf", "threewave ctf paks")
	ra2ref   = flag.String("ra2ref", "", "Rocket Arena 2 paks, for C2; empty skips it")
	gladdir  = flag.String("gladdir", "", "the brain and its assets, for H10; empty skips it")
	lib      = flag.String("lib", "", "game library under test")
	dir      = flag.String("dir", "/tmp/q2playtest/hardening", "scratch install dir")
	port     = flag.Int("port", 27990, "first UDP port to try")
	only     = flag.String("only", "", "run only the rows whose name contains this")
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
	fmt.Printf("  %s %-54s %s\n", tag, name, note)
	return ok
}

// skip records a row that did not run.  It is not a pass.
func skip(name, why string) {
	skipped++
	fmt.Printf("  [skip] %-54s %s\n", name, why)
}

func want(row string) bool { return *only == "" || strings.Contains(row, *only) }

var portCursor int

func boot(ruleset, mapName string, extra map[string]string) *playtest.Server {
	cvars := map[string]string{
		"g_ruleset": ruleset, "deathmatch": "1", "coop": "0",
		"bots": "0", "flood_msgs": "0", "timelimit": "0", "fraglimit": "0",
	}
	for k, v := range extra {
		cvars[k] = v
	}
	p := playtest.NextPort(portCursor)
	portCursor = p + 1
	srv := &playtest.Server{
		Binary: *q2proded, Dir: *dir, Game: "colosseum", Map: mapName,
		Port: p, MaxClients: 8, Cvars: cvars,
		LogPath: filepath.Join(*dir, fmt.Sprintf("server-%s-%d.log", ruleset, p)),
	}
	if err := srv.Start(); err != nil {
		check(ruleset+"/boot", false, err.Error())
		return nil
	}
	time.Sleep(2 * time.Second)
	return srv
}

func connect(srv *playtest.Server, name string) *playtest.Bot {
	b := playtest.NewBot(name, "127.0.0.1", srv.Port)
	if err := b.Start(30 * time.Second); err != nil {
		check(name+"/connects", false, err.Error())
		return nil
	}
	b.WaitFrames(10, 5*time.Second)
	return b
}

// printFrom waits for a print matching re that arrived after mark.
func printFrom(b *playtest.Bot, mark int, re string, d time.Duration) (string, bool) {
	rx := regexp.MustCompile(re)
	deadline := time.Now().Add(d)
	for time.Now().Before(deadline) {
		p := b.Prints()
		for i := mark; i < len(p); i++ {
			if rx.MatchString(p[i]) {
				return p[i], true
			}
		}
		time.Sleep(20 * time.Millisecond)
	}
	return "", false
}

func alive(row string, srv *playtest.Server) {
	err := srv.Alive(5 * time.Second)
	check(row+"/server still answers", err == nil, "%v", err)
}

// ---------------------------------------------------------------- C2 ------

func rowC2() {
	if !want("C2") {
		return
	}
	if *ra2ref == "" {
		skip("C2/teleporter pads", "no -ra2ref, so no RA2 map to load")
		return
	}
	for _, rs := range []string{"dm", "arena"} {
		srv := boot(rs, "ra2map1", nil)
		if srv == nil {
			continue
		}
		n := len(srv.Grep(`teleporter without a target\.`))
		if rs == "dm" {
			check("C2/dm frees the targetless pads, and says so", n > 0,
				"%d pad(s) freed", n)
		} else {
			check("C2/arena keeps them for its own touch", n == 0,
				"%d freed", n)
		}
		alive("C2/"+rs, srv)
		srv.Stop()
	}
}

// ---------------------------------------------------------------- C3 ------

func onTeam(b *playtest.Bot, team string) bool {
	for try := 0; try < 5; try++ {
		b.Cmd("team %s", team)
		time.Sleep(700 * time.Millisecond)
		mark := len(b.Prints())
		b.Cmd("team")
		if _, ok := printFrom(b, mark, `team "`+regexp.QuoteMeta(team)+`"`, 3*time.Second); ok {
			return true
		}
	}
	return false
}

func rowC3() {
	if !want("C3") {
		return
	}
	srv := boot("tdm", "q2dm1", map[string]string{"team_lockskin": "0"})
	if srv == nil {
		return
	}
	defer srv.Stop()
	a, b := connect(srv, "alpha"), connect(srv, "bravo")
	if a == nil || b == nil {
		return
	}
	defer a.Disconnect()
	defer b.Disconnect()
	if !check("C3/both clients on one team", onTeam(a, "Hometeam") && onTeam(b, "Hometeam"), "") {
		return
	}

	stuffs := len(b.Stuffs())
	mark := len(a.Prints())
	// No space in the payload: the server tokenises a client's command on
	// whitespace, so `male/grunt;zzinject` is ONE argument -- which is what
	// reached the teammate's console, unquoted, as a second command.
	a.Cmd("teamskin male/grunt;zzinject")
	_, refused := printFrom(a, mark, `A team skin is model/skin`, 5*time.Second)
	check("C3/a teamskin with a separator is refused", refused, "")
	time.Sleep(1 * time.Second)
	leaked := ""
	for _, s := range b.Stuffs()[stuffs:] {
		if strings.Contains(s, "zzinject") {
			leaked = s
		}
	}
	check("C3/the teammate is stuffed nothing of it", leaked == "", "%q", leaked)

	// The control: the stuff path is live, so its silence above means something.
	_, err := b.WaitStuff(`skin "male/major"`, 6*time.Second)
	if err != nil {
		a.Cmd("teamskin male/major")
		_, err = b.WaitStuff(`skin "male/major"`, 6*time.Second)
	}
	check("C3/control: a valid teamskin is stuffed to the teammate", err == nil, "%v", err)

	a.Cmd("teamname Red;say X")
	time.Sleep(800 * time.Millisecond)
	mark = len(a.Prints())
	a.Cmd("teamname")
	line, ok := printFrom(a, mark, `Current teamname:`, 5*time.Second)
	check("C3/a team name keeps no separator", ok && !strings.Contains(line, ";") &&
		strings.Contains(line, "Red"), "%q", strings.TrimSpace(line))
	alive("C3", srv)
}

// ---------------------------------------------------------------- C4 ------

func rowC4() {
	if !want("C4") {
		return
	}
	srv := boot("dmpro", "q2dm1", map[string]string{"referee_enable": "1"})
	if srv == nil {
		return
	}
	defer srv.Stop()
	a := connect(srv, "charlie")
	if a == nil {
		return
	}
	defer a.Disconnect()
	mark := len(a.Prints())
	a.Cmd("referee x")
	_, ok := printFrom(a, mark, `Referee mode is disabled on this server`, 5*time.Second)
	check("C4/referee with no password configured is refused", ok, "")
	mark = len(a.Prints())
	a.Cmd("_is_referee 1 x")
	_, ok = printFrom(a, mark, `Referee status disabled`, 5*time.Second)
	check("C4/_is_referee answers \"disabled\"", ok, "")
	alive("C4", srv)
}

// ---------------------------------------------------------------- H10 -----

func rowH10() {
	if !want("H10") {
		return
	}
	if *gladdir == "" {
		skip("H10/bots on a password server", "no -gladdir, so there is no brain")
		return
	}
	srv := boot("dm", "q2dm1", map[string]string{"password": "secret", "bots": "1",
		"bots_minplayers": "0", "minimumplayers": "0"})
	if srv == nil {
		return
	}
	defer srv.Stop()
	srv.Console(`sv addbot "Trash" "cyborg/ps9000" "bots/trash_c.c" "trash"`)
	bots := 0
	for i := 0; i < 20 && bots < 1; i++ {
		time.Sleep(1 * time.Second)
		mark, err := srv.Ask("sv ruleset", `^bots `, 5*time.Second)
		if err != nil {
			break
		}
		time.Sleep(400 * time.Millisecond)
		if c, err := colosseum.ParseBots(strings.Join(srv.LogFrom(mark), "\n")); err == nil {
			bots = c.Bots
		}
	}
	check("H10/a bot joins a password-protected server", bots >= 1, "%d bot(s)", bots)

	h := playtest.NewBot("nopass", "127.0.0.1", srv.Port)
	err := h.Start(12 * time.Second)
	check("H10/a person without the password is refused", err != nil, "%v", err)
	if err == nil {
		h.Disconnect()
	}
	g := playtest.NewBot("withpass", "127.0.0.1", srv.Port)
	g.ConnectKey("password", "secret")
	err = g.Start(20 * time.Second)
	check("H10/control: one with the password is let in", err == nil, "%v", err)
	if err == nil {
		g.Disconnect()
	}
	alive("H10", srv)
}

// ---------------------------------------------------------------- H12 -----

var shotsRe = regexp.MustCompile(`\((\d+)/(\d+) hits\)`)

func shots(b *playtest.Bot) (int, string) {
	mark := len(b.Prints())
	b.Cmd("accuracy")
	line, ok := printFrom(b, mark, `hits\)|didn't shoot a thing`, 5*time.Second)
	if !ok {
		return -1, "(no accuracy report)"
	}
	if m := shotsRe.FindStringSubmatch(line); m != nil {
		n, _ := strconv.Atoi(m[2])
		return n, strings.TrimSpace(line)
	}
	return 0, strings.TrimSpace(line)
}

func rowH12() {
	if !want("H12") {
		return
	}
	srv := boot("dm", "q2dm1", nil)
	if srv == nil {
		return
	}
	defer srv.Stop()
	a := connect(srv, "delta")
	if a == nil {
		return
	}
	defer a.Disconnect()
	for i := 0; i < 6 && a.Spectating(); i++ {
		a.Cmd("join")
		time.Sleep(1500 * time.Millisecond)
	}
	if !check("H12/the client is in the game", !a.Spectating(), "") {
		return
	}
	a.Press(playtest.ButtonAttack, 2500*time.Millisecond)
	time.Sleep(500 * time.Millisecond)
	before, l1 := shots(a)
	if !check("H12/shots are counted", before > 0, "%s", l1) {
		return
	}
	a.Cmd("kill")
	time.Sleep(2 * time.Second)
	after, l2 := shots(a)
	check("H12/the shots survive the death", after >= before,
		"%d before, %d after: %s", before, after, l2)
	alive("H12", srv)
}

// ---------------------------------------------------------------- H19 -----

func rowH19() {
	if !want("H19") {
		return
	}
	srv := boot("dmpro", "q2dm1", map[string]string{"referee_enable": "1",
		"referee_password": "pw"})
	if srv == nil {
		return
	}
	defer srv.Stop()
	a := connect(srv, "echo")
	if a == nil {
		return
	}
	defer a.Disconnect()
	mark := len(a.Prints())
	a.Cmd("referee nope")
	_, ok := printFrom(a, mark, `Password incorrect`, 5*time.Second)
	check("H19/a wrong password is refused", ok, "")
	mark = len(a.Prints())
	a.Cmd("referee pw")
	_, ok = printFrom(a, mark, `Wait a moment before trying again`, 5*time.Second)
	check("H19/inside the backoff even the right one is not tried", ok, "")
	time.Sleep(3 * time.Second)
	mark = len(a.Prints())
	a.Cmd("referee pw")
	_, ok = printFrom(a, mark, `now has referee status`, 5*time.Second)
	check("H19/control: after it, the right one is accepted", ok, "")
	alive("H19", srv)
}

// ---------------------------------------------------------------- H20 -----

func imageEntries(srv *playtest.Server) int {
	mark, err := srv.Ask("sv imageindex", `^ *\d+: `, 5*time.Second)
	if err != nil {
		return -1
	}
	time.Sleep(600 * time.Millisecond)
	n := 0
	for _, l := range srv.LogFrom(mark) {
		if regexp.MustCompile(`^ *\d+: \S`).MatchString(l) {
			n++
		}
	}
	return n
}

func rowH20() {
	if !want("H20") {
		return
	}
	// `bots 1` although no bot is added: `sv imageindex` is the bot layer's,
	// and it is the one window onto the image table from outside.
	srv := boot("arena", "q2dm1", map[string]string{"bots": "1",
		"minimumplayers": "0", "bots_minplayers": "0"})
	if srv == nil {
		return
	}
	defer srv.Stop()
	a := connect(srv, "foxtrot")
	if a == nil {
		return
	}
	defer a.Disconnect()
	time.Sleep(1 * time.Second)
	before := imageEntries(srv)
	for i := 0; i < 120; i++ {
		a.SetUserinfo("skin", fmt.Sprintf("male/zz%03d", i))
		time.Sleep(60 * time.Millisecond)
	}
	time.Sleep(2 * time.Second)
	after := imageEntries(srv)
	check("H20/cycling skins allocates no image index", before > 0 && after == before,
		"%d image(s) before, %d after 120 skins", before, after)
	alive("H20", srv)
}

func main() {
	flag.Parse()
	if *q2proded == "" || *lib == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded and -lib")
		os.Exit(2)
	}
	portCursor = *port
	os.RemoveAll(*dir)
	if err := colosseum.Install(*dir, *ref, *ctfref, *lib); err != nil {
		fmt.Fprintln(os.Stderr, "install:", err)
		os.Exit(2)
	}
	if *ra2ref != "" {
		paks, _ := filepath.Glob(filepath.Join(*ra2ref, "pak*.pak"))
		for i, p := range paks {
			if err := os.Symlink(p, filepath.Join(*dir, "colosseum", fmt.Sprintf("pak%d.pak", 20+i))); err != nil {
				fmt.Fprintln(os.Stderr, "ra2 paks:", err)
				os.Exit(2)
			}
		}
	}
	if *gladdir != "" {
		if err := colosseum.InstallBrain(*dir, *gladdir); err != nil {
			fmt.Fprintln(os.Stderr, "brain:", err)
			os.Exit(2)
		}
	}

	rowC2()
	rowC3()
	rowC4()
	rowH10()
	rowH12()
	rowH19()
	rowH20()

	fmt.Printf("\n%d check(s), %d failed, %d skipped\n", total, failed, skipped)
	if failed > 0 {
		fmt.Printf("server logs under %s\n", *dir)
		os.Exit(1)
	}
}
