// connectlog -- what the server's own console says about a session (R-LOG-1).
//
// THE SUBJECT.  A Quake II server's console is the only record a host keeps of
// who was on it.  Colosseum serves seven rulesets out of one library and, until
// R-LOG-1, three of them wrote three different lines there: the OSP four wrote
// tourney's "(name connected from <ip>)" AND the spine's bare "name connected"
// on the same stream, and `ctf`, `arena` and `sp` wrote only the bare one --
// no address, and nothing to tell a bot from a person.  A log reader written
// against the first format therefore counted zero players on an `arena`
// server, which is what this scenario exists to stop happening again.
//
// WHY IT NEEDS A RUNNING SERVER.  Every claim here is about a line the ENGINE
// printed after the game library handed it over, and two of them cannot be read
// from the source at all:
//
//   - the ADDRESS is the engine's.  q2pro force-sets userinfo `ip` in the
//     connect packet from the peer's real source address; a source reading can
//     say the field is copied, not that it holds 127.0.0.1 rather than an empty
//     string, "loopback", or a truncated "[".
//   - the COUNT is the whole point.  "One line per arrival" is a claim about
//     what a stream contains, and the defect being fixed was a duplicate.  A
//     grep for presence would have passed on the broken build.
//
// Exit 0 every check passed, 1 a check failed, 2 the scenario could not run.
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
	q2proded = flag.String("q2proded", "", "path to q2proded")
	ref      = flag.String("ref", "/usr/share/games/quake2/baseq2", "retail baseq2 paks")
	ctfref   = flag.String("ctf", "/usr/share/games/quake2/ctf", "threewave ctf paks")
	lib      = flag.String("lib", "", "game library under test")
	gladdir  = flag.String("gladdir", "", "gladiator-bot-restored, for the bot rows")
	dir      = flag.String("dir", "/tmp/q2playtest/connectlog", "scratch install dir")
	port0    = flag.Int("port", 27940, "first server port")
	only     = flag.String("rulesets", "dm,dmpro,tdm,duel,ctf,arena,sp", "rulesets to run")

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

	port := *port0
	for _, name := range strings.Split(*only, ",") {
		name = strings.TrimSpace(name)
		if name == "" {
			continue
		}
		rs, ok := colosseum.Find(name)
		if !ok {
			check(name, false, "not a Colosseum ruleset")
			continue
		}
		fmt.Printf("\n##### %s on %s (port %d)\n", rs.Name, rs.Map, port)
		if err := oneRuleset(rs, port); err != nil {
			fmt.Fprintf(os.Stderr, "ERROR: %s: %v\n", rs.Name, err)
			os.Exit(2)
		}
		port++
	}

	fmt.Printf("\n##### the address outlives a LEVEL CHANGE (port %d)\n", port)
	if err := mapChangeRow(port); err != nil {
		fmt.Fprintln(os.Stderr, "ERROR: level change:", err)
		os.Exit(2)
	}
	port++

	fmt.Printf("\n##### a drop the MOD starts, not the engine: referee kick (port %d)\n", port)
	if err := kickRow(port); err != nil {
		fmt.Fprintln(os.Stderr, "ERROR: kick:", err)
		os.Exit(2)
	}
	port++

	fmt.Printf("\n##### sp as a dedicated server actually runs it: coop (port %d)\n", port)
	if err := coopRow(port); err != nil {
		fmt.Fprintln(os.Stderr, "ERROR: coop:", err)
		os.Exit(2)
	}
	port++

	fmt.Printf("\n##### control: a REFUSED connection is not a session (port %d)\n", port)
	if err := controlRefused(port); err != nil {
		fmt.Fprintln(os.Stderr, "ERROR: control:", err)
		os.Exit(2)
	}

	fmt.Printf("\nconnectlog: %d check(s) passed, %d failed, %d skipped\n",
		passed, failed, skipped)
	if failed > 0 {
		os.Exit(1)
	}
}

// ---------------------------------------------------------------- the rows

func oneRuleset(rs colosseum.Ruleset, port int) error {
	srv, err := boot(rs.Name, rs.Map, port, 16, rs.Cvars)
	if err != nil {
		return err
	}
	defer srv.Stop()

	// ---- a person.
	//
	// "logtest" connects over a real UDP socket to 127.0.0.1, so the address
	// the engine hands the library is an NA_IP one with an ephemeral port on
	// it.  Asserting the exact string is asserting three things at once: that
	// the key was read at all, that it was read at CONNECT (where the engine
	// force-set it) and that the port was cut off the end of it.
	mark := srv.Len()
	b := playtest.NewBot("logtest", "127.0.0.1", port)
	if err := b.Start(30 * time.Second); err != nil {
		return err
	}
	if _, err := srv.WaitLog(`logtest connected`, 15*time.Second); err != nil {
		return fmt.Errorf("no connect line at all: %w", err)
	}
	time.Sleep(500 * time.Millisecond)

	rec := srv.GrepFrom(mark, `^\(logtest connected from 127\.0\.0\.1\)$`)
	check(rs.Name+"/connect record", len(rec) == 1,
		"want 1 line, got %d: %q", len(rec), rec)
	if len(rec) > 0 {
		fmt.Printf("         %s\n", rec[0])
	}

	// The double-print control, and the reason this is a COUNT and not a
	// grep: under the OSP four the bare line and the record both printed, and
	// a check that only asked whether the record was there passed on that
	// build.  Anything on this stream that says "connected" and is not the
	// record is a second line about one arrival.
	extra := other(srv.GrepFrom(mark, `logtest connected`), rec)
	check(rs.Name+"/connect prints once", len(extra) == 0,
		"%d other connect line(s): %q", len(extra), extra)

	// ---- and their departure.
	//
	// A MISSING record is a FAILED CHECK here and not a scenario error, which
	// matters for the A/B: on a library without R-LOG-1 there is no departure
	// record at all under the OSP four -- tourney announces "wimped out and
	// left" to the players and writes nothing to the log -- and a row that
	// aborted the run there would report the regression as a broken test.  So
	// the wait is for the line we want, and running out of patience is the
	// finding.
	mark = srv.Len()
	b.Disconnect()
	srv.WaitLog(`^\(logtest disconnected from `, 15*time.Second)
	time.Sleep(500 * time.Millisecond)

	rec = srv.GrepFrom(mark, `^\(logtest disconnected from 127\.0\.0\.1\)$`)
	check(rs.Name+"/disconnect record", len(rec) == 1,
		"want 1 line, got %d; the console said: %q", len(rec),
		srv.GrepFrom(mark, `logtest`))
	if len(rec) > 0 {
		fmt.Printf("         %s\n", rec[0])
	}

	// ---- and a bot, which is the field's other value and the half the
	// report was actually about: a log that cannot tell a bot from a person
	// cannot count either.
	if *gladdir == "" {
		skip(rs.Name+"/bot record", "no -gladdir, so no brain to add a bot with")
		skip(rs.Name+"/bot disconnect record", "no -gladdir")
		return nil
	}
	return botRows(srv, rs.Name)
}

var reBotConnect = regexp.MustCompile(`^\((.+) connected from SERVER_BOT\)$`)

func botRows(srv *playtest.Server, name string) error {
	mark := srv.Len()

	// `sp` is the one ruleset that takes no bots at all (N6), so its row is
	// the refusal rather than a skip: a bot that cannot exist cannot be
	// recorded, and a check that says so is worth more than one that says
	// nothing.
	//
	// TWO answers are accepted and both are correct, which is the point of
	// asking on a running server.  `AddRandomBot` prints "ruleset 'sp' does
	// not accept bots" -- and under `sp` that message is unreachable, because
	// `G_ResolveModifiers` turns the `bots` modifier off before the bot
	// layer's server commands are registered, so the engine answers `Unknown
	// server command "addrandom"` and the game library never sees it.  A row
	// written for the message alone reported a failure where the ruleset was
	// working harder than it claimed.
	if name == "sp" {
		srv.Console("sv addrandom")
		_, err := srv.WaitLog(
			`does not accept bots|Unknown server command "addrandom"`,
			15*time.Second)
		check("sp/takes no bots", err == nil,
			"neither a refusal nor an unknown command: %v", err)
		rec := srv.GrepFrom(mark, `connected from SERVER_BOT`)
		check("sp/and records no bot arrival", len(rec) == 0, "%q", rec)
		return nil
	}

	srv.Console("sv addrandom")
	if _, err := srv.WaitLog(`connected from SERVER_BOT`, 20*time.Second); err != nil {
		skip(name+"/bot record", "no bot appeared: "+err.Error())
		skip(name+"/bot disconnect record", "no bot appeared")
		return nil
	}
	time.Sleep(500 * time.Millisecond)

	rec := srv.GrepFrom(mark, `^\(.+ connected from SERVER_BOT\)$`)
	check(name+"/bot record", len(rec) == 1,
		"want 1 line, got %d: %q", len(rec), rec)
	if len(rec) == 0 {
		skip(name+"/bot disconnect record", "no bot record to name a bot")
		return nil
	}
	fmt.Printf("         %s\n", rec[0])
	bot := reBotConnect.FindStringSubmatch(rec[0])[1]

	// A bot's userinfo has no `ip` key at all, so an address derived from it
	// would be the empty string and the line would read "connected from )".
	check(name+"/bot is not a blank address", bot != "" && !strings.Contains(rec[0], "from )"),
		"%q", rec[0])

	mark = srv.Len()
	srv.Console("sv removebot")
	if _, err := srv.WaitLog(`disconnected from SERVER_BOT`, 20*time.Second); err != nil {
		check(name+"/bot disconnect record", false, "no line: %v", err)
		return nil
	}
	time.Sleep(500 * time.Millisecond)

	want := `^\(` + regexp.QuoteMeta(bot) + ` disconnected from SERVER_BOT\)$`
	rec = srv.GrepFrom(mark, want)
	check(name+"/bot disconnect record", len(rec) == 1,
		"want 1 line matching %s, got %d: %q", want, len(rec), rec)
	if len(rec) > 0 {
		fmt.Printf("         %s\n", rec[0])
	}
	return nil
}

// mapChangeRow is the row that answers for where the address LIVES.
//
// ClientConnect is not called again on a level change, so the only thing
// keeping the address is that `game.clients` -- and therefore `pers` -- is
// allocated once in InitGame and never touched by one.  Tourney did not rely on
// that: `osp_e37c` was on the EDICT, every client edict is memset in
// SpawnEntities, and the donor copies four fields out and back around that
// memset to save them.  Moving the address into `pers` deletes that copy, and
// this is what says the deletion was safe rather than merely tidy.
//
// `gamemap`, not `map`: Q2PRO ignores a `map` issued while a game is running,
// and `gamemap` is the change that KEEPS the connection -- the server stuffs
// `changing` and `reconnect` and the client answers with the spawn handshake.
// The frame counter moving afterwards is what says it did.
func mapChangeRow(port int) error {
	srv, err := boot("dm", "q2dm1", port, 8,
		map[string]string{"deathmatch": "1", "coop": "0"})
	if err != nil {
		return err
	}
	defer srv.Stop()

	b := playtest.NewBot("traveller", "127.0.0.1", port)
	if err := b.Start(30 * time.Second); err != nil {
		return err
	}
	if _, err := srv.WaitLog(`traveller connected`, 15*time.Second); err != nil {
		return err
	}

	mark := srv.Len()
	srv.Console("gamemap q2dm5")
	if _, err := srv.WaitLog(`SpawnServer: q2dm5`, 30*time.Second); err != nil {
		return fmt.Errorf("the server never reached q2dm5: %w", err)
	}
	time.Sleep(2 * time.Second)

	// The row is only about anything if the client is still there.
	if err := b.WaitFrames(20, 20*time.Second); err != nil {
		return fmt.Errorf("the client did not survive the level change: %w", err)
	}

	mark = srv.Len()
	b.Disconnect()
	srv.WaitLog(`^\(traveller disconnected from `, 15*time.Second)
	time.Sleep(500 * time.Millisecond)

	rec := srv.GrepFrom(mark, `^\(traveller disconnected from 127\.0\.0\.1\)$`)
	check("mapchange/address survives a level change", len(rec) == 1,
		"want 1 line, got %d; the console said: %q", len(rec),
		srv.GrepFrom(mark, `traveller`))
	if len(rec) > 0 {
		fmt.Printf("         %s\n", rec[0])
	}
	return nil
}

// kickRow asks the one question a client's own quit cannot: what does the
// record do when the MOD tears the client down rather than the engine?
//
// Tourney's referee kick, its ban, its vote-kick and both cheat detectors all
// write `svc_disconnect` to the client and then call `ClientDisconnect`
// themselves.  The engine's own `SV_DropClient` follows when the connection
// actually goes, and its "called recursively?" guard is on the ENGINE's client
// state, which the mod's call did not touch -- so the game library's
// ClientDisconnect can run twice for one departure.  One record per departure
// is the requirement (R-LOG-1), so this counts them.
//
// It is here rather than in the per-ruleset sweep because only the OSP four
// have a referee.
//
// IT HAS ALREADY CAUGHT ONE (R-LOG-7).  Before the guard in ClientDisconnect
// this row read:
//
//	[FAIL] kick/one departure record -- 2 record(s):
//	       ["(kickme disconnected from 127.0.0.1)" "( disconnected from 127.0.0.1)"]
//
// The empty name is OSP_clientLeft having cleared `pers.netname` on the first
// pass -- so the second pass was not only a duplicate line, it was a second
// teardown of a client that had already left.
func kickRow(port int) error {
	srv, err := boot("dm", "q2dm1", port, 8, map[string]string{
		"deathmatch": "1", "coop": "0",
		"referee_enable": "1", "referee_password": "letmein",
	})
	if err != nil {
		return err
	}
	defer srv.Stop()

	ref := playtest.NewBot("theref", "127.0.0.1", port)
	if err := ref.Start(30 * time.Second); err != nil {
		return err
	}
	victim := playtest.NewBot("kickme", "127.0.0.1", port)
	if err := victim.Start(30 * time.Second); err != nil {
		return err
	}
	if _, err := srv.WaitLog(`kickme connected`, 15*time.Second); err != nil {
		return err
	}

	ref.Cmd("referee letmein")
	if _, err := srv.WaitLog(`now has referee status`, 15*time.Second); err != nil {
		return fmt.Errorf("the referee never got in: %w", err)
	}

	mark := srv.Len()
	ref.Cmd("r_kick kickme")
	if _, err := srv.WaitLog(`kickme has been kicked`, 15*time.Second); err != nil {
		return fmt.Errorf("the kick never happened: %w", err)
	}
	// The engine's own drop follows the mod's, whenever the socket notices.
	// Long enough for both to have happened, so a count of one is a count and
	// not a race.
	time.Sleep(6 * time.Second)

	rec := srv.GrepFrom(mark, `disconnected from`)
	check("kick/one departure record", len(rec) == 1,
		"%d record(s): %q", len(rec), rec)
	for _, l := range rec {
		fmt.Printf("         %s\n", l)
	}
	return nil
}

// coopRow is the `sp` row a dedicated server actually runs, and it is its own
// phase because it takes a DIFFERENT path through PutClientInServer.
//
// The deathmatch arm re-runs InitClientPersistant on every spawn and that
// function preserves the address across its own wipe; the coop arm instead
// assigns the whole of `pers` from `resp.coop_respawn`, a snapshot
// InitClientResp took before ClientConnect had latched anything.  So the
// address is present at connect, gone after the first spawn, and the departure
// record reads "(name disconnected from )" -- which is exactly the failure the
// pair exists to prevent, on the one configuration a dedicated `sp` server
// uses (README: single player needs a client, so it runs as co-op).
func coopRow(port int) error {
	srv, err := boot("sp", "base1", port, 4,
		map[string]string{"deathmatch": "0", "coop": "1"})
	if err != nil {
		return err
	}
	defer srv.Stop()

	mark := srv.Len()
	b := playtest.NewBot("coopest", "127.0.0.1", port)
	if err := b.Start(30 * time.Second); err != nil {
		return err
	}
	if _, err := srv.WaitLog(`coopest connected`, 15*time.Second); err != nil {
		return fmt.Errorf("no connect line at all: %w", err)
	}
	rec := srv.GrepFrom(mark, `^\(coopest connected from 127\.0\.0\.1\)$`)
	check("coop/connect record", len(rec) == 1,
		"want 1 line, got %d: %q", len(rec), rec)

	// The client has to have SPAWNED for this row to be about anything: the
	// assignment that loses the address is in PutClientInServer.
	if err := b.WaitFrames(20, 15*time.Second); err != nil {
		return fmt.Errorf("client never got frames: %w", err)
	}

	mark = srv.Len()
	b.Disconnect()
	srv.WaitLog(`^\(coopest disconnected from `, 15*time.Second)
	time.Sleep(500 * time.Millisecond)

	rec = srv.GrepFrom(mark, `^\(coopest disconnected from 127\.0\.0\.1\)$`)
	check("coop/address survives the first spawn", len(rec) == 1,
		"want 1 line, got %d; the console said: %q", len(rec),
		srv.GrepFrom(mark, `coopest`))
	if len(rec) > 0 {
		fmt.Printf("         %s\n", rec[0])
	}
	return nil
}

// controlRefused is the check on the check: something that MUST NOT produce a
// record, so a row that only ever looks for one cannot pass by accident.
//
// A client that fails the server password is refused inside ClientConnect --
// it returns false and no session begins -- and the record is written at the
// END of that function, after every refusal.  A latch or a print moved above
// them would log an arrival that never happened, and a per-ruleset sweep of
// successful connections could not tell.
//
// *The control this replaces could not be arranged at all, and that is worth
// recording rather than quietly dropping.*  R-LOG-1 gates both records on
// `game.maxclients > 1`, and a DEDICATED server can never be under it:
// `SV_InitGame` forces `deathmatch 1` when `coop` is 0 on a dedicated server,
// then raises `maxclients` to 8 (or to 4 under coop) whenever it is 1.  The
// gate is reachable only on a LISTEN server in single player, which this
// harness has no way to drive, so a check written for it measured a
// `maxclients 8` server and reported the gate broken.
func controlRefused(port int) error {
	srv, err := boot("dm", "q2dm1", port, 8, map[string]string{
		"deathmatch": "1", "coop": "0", "password": "letmein",
	})
	if err != nil {
		return err
	}
	defer srv.Stop()

	mark := srv.Len()
	// No password: the engine relays `rejmsg` and the connection ends here.
	b := playtest.NewBot("nopass", "127.0.0.1", port)
	err = b.Start(15 * time.Second)
	check("control/the refused client does not get in", err != nil,
		"the server accepted a client with no password")
	if err == nil {
		b.Disconnect()
		time.Sleep(500 * time.Millisecond)
	}

	rec := srv.GrepFrom(mark, `nopass (connected|disconnected) from`)
	check("control/and is not recorded as a session", len(rec) == 0,
		"%d line(s): %q", len(rec), rec)

	// ...and the same server still records a client that DOES get in, so the
	// row above is the password working rather than the record being gone.
	mark = srv.Len()
	ok := playtest.NewBot("haspass", "127.0.0.1", port)
	ok.ConnectKey("password", "letmein")
	if err := ok.Start(30 * time.Second); err != nil {
		return fmt.Errorf("the client WITH the password could not get in: %w", err)
	}
	srv.WaitLog(`^\(haspass connected from `, 15*time.Second)
	time.Sleep(500 * time.Millisecond)
	rec = srv.GrepFrom(mark, `^\(haspass connected from 127\.0\.0\.1\)$`)
	check("control/and the accepted one is", len(rec) == 1,
		"want 1 line, got %d: %q", len(rec), srv.GrepFrom(mark, `haspass`))
	if len(rec) > 0 {
		fmt.Printf("         %s\n", rec[0])
	}
	ok.Disconnect()
	return nil
}

// ---------------------------------------------------------------- plumbing

func boot(rs, mp string, port, maxclients int, cv map[string]string) (*playtest.Server, error) {
	d := filepath.Join(*dir, rs+fmt.Sprint(port))
	if err := os.MkdirAll(d, 0o755); err != nil {
		return nil, err
	}
	if err := colosseum.Install(d, *ref, *ctfref, *lib); err != nil {
		return nil, err
	}
	if *gladdir != "" {
		if err := colosseum.InstallBrain(d, *gladdir); err != nil {
			return nil, err
		}
	}
	c := map[string]string{
		"g_ruleset": rs, "cheats": "1",
		// No fill: every client on these servers is one this scenario put
		// there, so a count of lines is a count of arrivals.
		"minimumplayers": "0", "bots_minplayers": "0", "botfill": "0",
	}
	for k, v := range cv {
		c[k] = v
	}
	srv := &playtest.Server{
		Binary: *q2proded, Dir: d, Game: "colosseum", Map: mp, Port: port,
		MaxClients: maxclients, Cvars: c,
		LogPath: filepath.Join(d, "server.log"),
	}
	if err := srv.Start(); err != nil {
		return nil, err
	}
	return srv, nil
}

// other returns the lines of all that are not in want.
func other(all, want []string) []string {
	seen := map[string]int{}
	for _, l := range want {
		seen[l]++
	}
	var out []string
	for _, l := range all {
		if seen[l] > 0 {
			seen[l]--
			continue
		}
		out = append(out, l)
	}
	return out
}
