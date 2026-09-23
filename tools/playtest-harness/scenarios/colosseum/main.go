// colosseum -- drive every ruleset of a Colosseum build with real clients.
//
// One game library serves five rulesets, so this runs the same battery five
// times and asserts on what each ruleset does DIFFERENTLY from the others.
// Nothing here needs navigation: menus, client commands, userinfo updates and
// the two `sv` diagnostics reach most of the interesting code, and unlike
// walking they are deterministic.
package main

import (
	"flag"
	"fmt"
	"os"
	"regexp"
	"sort"
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
	lib      = flag.String("lib", "", "game library under test")
	dir      = flag.String("dir", "/tmp/colosseum-playtest", "scratch install dir")
	port     = flag.Int("port", 27980, "lowest UDP port to hand out")
	only     = flag.String("rulesets", "dm,dmpro,tdm,duel,ctf,arena,sp", "rulesets to run")
	keep     = flag.Bool("keep", false, "keep server logs on success")
	gladdir  = flag.String("gladdir", "", "gladiator-bot-restored checkout; enables the bot rows")
)

// portCursor is the next port nextPort() will consider.  Set from -port.
var portCursor int

// nextPort hands out a UDP port the machine is not already using.
//
// A PORT IS ASKED FOR, NOT COUNTED OUT, and the reason a free one has to be
// asked for is in playtest.NextPort.  What this spelling additionally removes
// is the arithmetic: every server here used to take a hand-written offset from
// -port, which admits two further mistakes, and the tree had both.  `extras`
// hands out eleven ports where main() reserved ten before `botRows`, and
// `port+2` was allocated twice inside `extras`.  Neither ever fired, because
// boot() stops its server before it returns -- so what they were was a latent
// second copy of the failure that did fire.  A cursor cannot make either.
func nextPort() int {
	p := playtest.NextPort(portCursor)
	if p == 0 {
		fmt.Printf("no free UDP port in %d..%d\n", portCursor, portCursor+1000)
		os.Exit(2)
	}
	portCursor = p + 1
	return p
}

type result struct {
	name, note string
	ok         bool
}

var results []result

func check(name string, ok bool, note string, a ...any) bool {
	if len(a) > 0 {
		note = fmt.Sprintf(note, a...)
	}
	results = append(results, result{name, note, ok})
	status := "FAIL"
	if ok {
		status = " ok "
	}
	fmt.Printf("  [%s] %-46s %s\n", status, name, note)
	return ok
}

func main() {
	flag.Parse()
	if *q2proded == "" || *ref == "" || *lib == "" {
		fmt.Println("need -q2proded, -ref and -lib")
		os.Exit(2)
	}
	portCursor = *port
	// `dm` first and unconditionally: it is the control for every other row.
	// Without a baseline "arena has 37 edicts" is a number, not a finding.
	var base colosseum.State
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
		st, err := run(rs, nextPort(), base)
		if err != nil {
			check(name+"/boot", false, err.Error())
			continue
		}
		if name == "dm" {
			base = st
		}
	}

	// Two more servers, each answering a question the five above cannot: a
	// modifier is a cvar, so the only way to learn whether a ruleset accepts
	// one is to ask for it.
	extras()

	// Bots across the rulesets, watched by a real client.  Skipped
	// rather than failed when -gladdir is absent, because the brain is a
	// sibling repository and not every run has one -- but the skip is
	// RECORDED, so "139 checks passed" cannot quietly mean "and eleven were
	// not run".
	botRows()

	bad := 0
	for _, r := range results {
		if !r.ok {
			bad++
		}
	}
	fmt.Printf("\n%d check(s), %d failed\n", len(results), bad)
	if bad > 0 {
		for _, r := range results {
			if !r.ok {
				fmt.Printf("  FAIL %s: %s\n", r.name, r.note)
			}
		}
		os.Exit(1)
	}
	if !*keep {
		os.RemoveAll(*dir)
	}
}

func run(rs colosseum.Ruleset, port int, base colosseum.State) (colosseum.State, error) {
	d := fmt.Sprintf("%s/%s", *dir, rs.Name)
	if err := colosseum.Install(d, *ref, *ctfref, *lib); err != nil {
		return colosseum.State{}, err
	}
	cv := map[string]string{"g_ruleset": rs.Name, "cheats": "1"}
	for k, v := range rs.Cvars {
		cv[k] = v
	}
	srv := &playtest.Server{
		Binary: *q2proded, Dir: d, Game: "colosseum", Map: rs.Map,
		Port: port, MaxClients: 8, Cvars: cv, LogPath: d + "/server.log",
	}
	if err := srv.Start(); err != nil {
		return colosseum.State{}, err
	}
	defer srv.Stop()
	fmt.Printf("\n##### %s on %s (port %d)\n", rs.Name, rs.Map, port)
	serverBar := ""

	// ---- 1. the dispatch, read back from the running library
	srv.Console("sv ruleset")
	if _, err := srv.WaitLog(`^world `, 5*time.Second); err != nil {
		return colosseum.State{}, fmt.Errorf("sv ruleset never answered")
	}
	// The lines a scenario waits on are not the LAST lines of the block: the
	// ctf census and the "!!" complaints follow `world`, and the statusbar
	// text follows `statusbar`.  Parsing the moment the awaited line lands
	// reads a block that is still arriving, which is a flake, not a failure.
	settle()
	st, err := colosseum.ParseRuleset(strings.Join(srv.Log(), "\n"))
	if err != nil {
		return colosseum.State{}, err
	}
	check(rs.Name+"/dispatch", st.Ruleset == rs.Name, "library reports %q", st.Ruleset)
	check(rs.Name+"/no complaint", len(st.Complaints) == 0, strings.Join(st.Complaints, " | "))

	// The predicates are the ruleset gates; each is a claim about this
	// ruleset that another ruleset contradicts.
	switch rs.Name {
	case "sp":
		check(rs.Name+"/campaign", st.Campaign && st.Monsters && st.Saves,
			"campaign=%v monsters=%v saves=%v", st.Campaign, st.Monsters, st.Saves)
		check(rs.Name+"/monsters live", st.LiveMonsters > 0, "%d live monster(s)", st.LiveMonsters)
		check(rs.Name+"/no bots", !st.Bots, "bots=%v", st.Bots)
	case "ctf":
		// ctf is NOT a campaign, but it does permit monsters --
		// so deliberately, because Threewave's edits to the monster files have
		// never been compiled against live monster code anywhere else.
		check(rs.Name+"/not campaign", !st.Campaign, "campaign=%v", st.Campaign)
		check(rs.Name+"/monsters permitted", st.Monsters, "monsters=%v", st.Monsters)
		check(rs.Name+"/no monsters on a ctf map", st.LiveMonsters == 0, "%d live monster(s)", st.LiveMonsters)
		// Flags must be in the world and at their bases.  Techs must not be:
		// they are the `runes` modifier, which is off unless asked for, and
		// that is checked as its own case below.
		check(rs.Name+"/flags at base", strings.Contains(st.CTF, "flags base/base"), st.CTF)
		check(rs.Name+"/team spawns", !strings.Contains(st.CTF, " 0+") && !strings.Contains(st.CTF, "+0 "), st.CTF)
	default:
		check(rs.Name+"/not campaign", !st.Campaign && !st.Monsters, "campaign=%v monsters=%v", st.Campaign, st.Monsters)
		check(rs.Name+"/no live monsters", st.LiveMonsters == 0, "%d live monster(s)", st.LiveMonsters)
	}
	if rs.Name != "ctf" {
		check(rs.Name+"/no ctf content", st.CTF == "", st.CTF)
	}
	// A modifier is a cvar, not something a ruleset implies: none is asked for
	// here, so none may be on.  Whether the ruleset ACCEPTS one is checked
	// separately, by asking for it (modifierChecks).
	//
	// `runes` is the exception as of Colosseum 1.22, and it is the exception on
	// purpose: the modifier resolution makes a modifier
	// report what the ruleset's own switch says rather than what was asked for,
	// and under ctf that switch is DF_CTF_NO_TECH -- clear by default, so the
	// techs DO scatter and `runes=1` is now the truth about this server.  The
	// old assertion said all three must be false and was measuring the thing
	// was recorded as broken.  teamplay and hook still have no such switch and
	// still must be off unless asked for.
	check(rs.Name+"/no unrequested modifier", !st.Teamplay && !st.Hook,
		"teamplay=%v hook=%v", st.Teamplay, st.Hook)
	// ...and the derived one, checked as a derivation: true exactly where the
	// ruleset has techs or runes in play with nothing asked for.  ctf's techs
	// come from a dmflag that defaults to on; the OSP four's runes from
	// `runes_enable`, which defaults to 0; only arena refuses the modifier.
	check(rs.Name+"/runes reports its switch", st.Runes == (rs.Name == "ctf"),
		"runes=%v under %s", st.Runes, rs.Name)
	// The arena gate is visible as a number: arena frees every pickup at spawn.
	if rs.Name == "arena" && base.Edicts > 0 {
		check(rs.Name+"/items removed", st.Edicts < base.Edicts/2,
			"%d edicts against dm's %d on the same map", st.Edicts, base.Edicts)
	}

	// ---- 2. the stat map and the bar composed from it
	srv.Console("sv slots")
	if _, err := srv.WaitLog(`^statusbar `, 5*time.Second); err != nil {
		check(rs.Name+"/sv slots", false, "no reply")
	} else if sl, err := settled(colosseum.ParseSlots, srv); err != nil {
		check(rs.Name+"/sv slots", false, err.Error())
	} else {
		check(rs.Name+"/bar fits", !sl.Overflow && sl.BarLen > 0 && sl.BarLen < sl.BarMax,
			"%d of %d bytes, %d stats, top slot %d", sl.BarLen, sl.BarMax, sl.Mapped, sl.Top)
		// On a server without protocol extensions ctf's second powerup
		// timer resolves to nothing, by design, and `sv slots` reports it.
		// Every OTHER drop is a defect.
		wantDropped := colosseum.DroppedWithoutExtensions[rs.Name]
		if sl.Extended {
			wantDropped = nil
		}
		got := []string{}
		for n := range sl.Dropped {
			got = append(got, n)
		}
		sort.Strings(got)
		sort.Strings(wantDropped)
		check(rs.Name+"/drops are the documented ones", equal(got, wantDropped),
			"dropped %v, expected %v", got, wantDropped)

		exp := colosseum.Stats[rs.Name]
		check(rs.Name+"/own stats present", len(sl.Missing(exp.Want)) == 0, "missing %v", sl.Missing(exp.Want))
		check(rs.Name+"/no foreign stats", len(sl.Leaked(exp.Forbid)) == 0, "leaked %v", sl.Leaked(exp.Forbid))
		serverBar = sl.Bar
	}

	// ---- 3. a real client
	b := playtest.NewBot("probe", "127.0.0.1", port)
	if err := b.Start(30 * time.Second); err != nil {
		check(rs.Name+"/client connects", false, err.Error())
		return st, nil
	}
	defer b.Disconnect()
	check(rs.Name+"/client connects", true, "spawned in")

	if _, err := srv.WaitLog(`entered the game|probe`, 8*time.Second); err != nil {
		check(rs.Name+"/server sees client", false, "no connect print")
	} else {
		check(rs.Name+"/server sees client", true, "")
	}

	// The bar the client actually RECEIVED must be the bar the library
	// composed.  `sv slots` prints the latter, so the two can be compared
	// byte for byte -- which is a stronger claim than "it looks like a bar",
	// and the only one that catches a bar truncated on the way out.
	bar := playtest.Decode(b.StatusBar())
	check(rs.Name+"/client bar is the composed one",
		bar != "" && strings.TrimSpace(bar) == strings.TrimSpace(serverBar),
		"%d bytes received, %d composed", len(bar), len(strings.TrimSpace(serverBar)))

	// Say it twice if the first one is not echoed.  A `say` sent in the first
	// moments after the spawn can land before the server is ready to route it
	// back, which shows up as a one-in-N failure -- and a check that fails
	// occasionally is worse than no check, because it teaches its reader to
	// ignore it.  Two attempts and a wider window make the failure mean
	// something.
	echoed := false
	for attempt := 0; attempt < 2 && !echoed; attempt++ {
		b.Cmd("say colosseum-playtest")
		if _, err := b.WaitPrint(`colosseum-playtest`, 6*time.Second); err == nil {
			echoed = true
		}
	}
	if echoed {
		check(rs.Name+"/chat", true, "")
	} else {
		check(rs.Name+"/chat", false, "not echoed back after two attempts")
	}

	// `score` reaches each ruleset's own scoreboard writer, which is a
	// dispatch row and the largest per-ruleset string in the game.
	//
	// Two rulesets answer it differently and both are correct.  Under sp there
	// is no scoreboard at all -- baseq2 returns unless deathmatch or coop.
	//
	// Under arena the client is holding a menu and the board does NOT close it,
	// which is the one exemption rather than an exception to it.  Every
	// other menu engine here draws with svc_layout, so a board and a menu
	// contend for one channel; RA2's draws by overwriting CS_STATUSBAR for the
	// one client, so they are two channels and 1999 showed them together.  RA2's
	// own Cmd_Score_f has no menu test, and keeping one meant an arena observer
	// could never open the board at all -- move_to_arena() reopens the observer
	// menu on every placement, so `score` was always spent closing it.
	// Asserted in both channels at once, because that is the whole claim.
	if rs.Name == "sp" {
		b.Cmd("score")
		time.Sleep(1 * time.Second)
		check(rs.Name+"/no scoreboard in a campaign", b.Layout() == "", "%q", b.Layout())
	} else {
		// ONE press, and both channels read off it -- arena's `score` CYCLES
		// (board -> server-wide -> off, and for a client with no arena it
		// toggles the server-wide one), so spending a press on a separate
		// assertion turns the board back off before the board is asked for.
		before := b.MenuTitle()
		b.Cmd("score")
		if lay, err := b.WaitLayout(`.`, 5*time.Second); err != nil {
			check(rs.Name+"/scoreboard", false, "no layout arrived")
		} else {
			check(rs.Name+"/scoreboard", len(lay) > 0, "%d bytes", len(lay))
		}
		if rs.Name == "arena" {
			check(rs.Name+"/score leaves the menu up", b.MenuTitle() == before,
				"was %q, now %q", before, b.MenuTitle())
		}
	}

	// ---- 4. the ruleset's own paths
	switch {
	case rs.Name == "ctf":
		ctfChecks(srv, b, rs.Name)
	case rs.Name == "arena":
		arenaChecks(srv, b, rs.Name)
	case colosseum.IsOSP(rs.Name):
		tourneyChecks(srv, b, rs.Name)
	}

	// ---- 4b. The Gladiator observer.  Three implementations, one
	// per ruleset, and this is the dm/sp/ctf one -- so the check is BOTH that
	// it answers where it should and that it does not answer where another
	// donor owns the same verb.
	observerChecks(srv, b, rs.Name)

	// ---- 5. leaving is a code path too, and the one that crashes last
	b.Disconnect()
	time.Sleep(800 * time.Millisecond)
	srv.Console("sv ruleset")
	if _, err := srv.WaitLog(`^world `, 5*time.Second); err == nil {
		settle()
		if after, err := colosseum.ParseRuleset(strings.Join(srv.Log(), "\n")); err == nil {
			check(rs.Name+"/client leaves cleanly", after.Clients == 0, "%d client(s) still counted", after.Clients)
			// The techs are not in the world at spawn: CTFSetupTechSpawn puts
			// a thinker in that scatters them two seconds later, so the census
			// taken at boot reads zero and only a later one can see them.
			// This is that later one.
			if rs.Name == "ctf" {
				check(rs.Name+"/techs scattered", !strings.Contains(after.CTF, "0 tech"), after.CTF)
			}
		}
	}

	errs := srv.Grep(`ERROR|bad index|type mismatch|unknown pointer|SZ_GetSpace|overflow`)
	check(rs.Name+"/console clean", len(errs) == 0, strings.Join(errs, " | "))
	return st, nil
}

// The Gladiator observer's eight client commands, and the two facts
// about them that need a client -- that `observer` actually changes what the
// server sends this player, and that under `arena` and the OSP four the verb
// belongs to that ruleset's own observer instead.
//
// The pmove type is the honest question, as it is for arena: a mod's own HUD
// can say anything, and the movetype the server puts in the playerstate is what
// the client is actually being told.
func observerChecks(srv *playtest.Server, b *playtest.Bot, rs string) {
	// `dm` moved out of this set in spec 1.36: it is OSP's RegularDM now and
	// has osp_observe.c's observer, not the Gladiator one.  What is left is
	// `ctf` and `sp`, which is what G_GladiatorObserver() answers true for.
	glad := rs == "sp" || rs == "ctf"

	before := srv.Len()
	b.Cmd("observer")
	time.Sleep(1200 * time.Millisecond)
	// Under ctf the VERB is Threewave's -- it drops the flag, drops the tech and
	// resets the score, which the Gladiator toggle knows nothing about -- and
	// what it supplies there is the CAMERAS.  So the entry message
	// differs by ruleset, and the honest observation under ctf is the join menu
	// CTFObserver opens.
	entered := len(srv.GrepFrom(before, `entered observer mode`)) > 0
	if rs == "ctf" {
		entered = b.MenuTitle() != "" || b.Spectating()
	}

	if !glad {
		// arena's own observer is the implementation here and the OSP four's is
		// osp_observe.c; neither prints the Gladiator line, and neither may
		// report the command as unknown either -- arena has an `observer`
		// notion of its own and the OSP four answer with their own.
		check(rs+"/observer is not the Gladiator one", !entered,
			"no 'entered observer mode' under %s", rs)
		return
	}

	if !check(rs+"/observer entered", entered, strings.Join(srv.GrepFrom(before, `observer mode`), " | ")) {
		return
	}
	check(rs+"/observer is spectating", b.Spectating(), "pmtype=%d", b.PMType())

	// A PLAIN observer flies: no camera flag is set, so DoObserver leaves the
	// movement axes alone and puts the client on a spectator pmove.  A CAMERA
	// does not: it drives the view and zeroes forwardmove/sidemove/upmove,
	// because that is what a camera is.
	//
	// Both halves are asserted, and the second one is why: the first person to
	// watch this reported the controls dead for most of the run, and the honest
	// answer was "you were in a chase camera for ninety seconds" -- which is a
	// claim about design that nothing was checking. Now it is checked, so if a
	// future edit freezes a plain observer too, this says so.
	check(rs+"/plain observer can move", b.PMType() == playtest.PMSpectator,
		"pmtype=%d, want %d (PM_SPECTATOR -- movement is the client's)",
		b.PMType(), playtest.PMSpectator)

	// The seven camera verbs.  Each prints its own state, so "accepted" is a
	// real observation rather than the absence of a complaint -- and the
	// absence of a complaint is checked too, because a command that falls
	// through to the default arm is silent about it.
	before = srv.Len()
	for _, c := range []string{"autocam", "chasecam", "cyclecam", "camfixed", "camname", "observerhelp"} {
		b.Cmd(c)
		time.Sleep(250 * time.Millisecond)
	}
	time.Sleep(600 * time.Millisecond)
	check(rs+"/camera commands accepted",
		len(srv.GrepFrom(before, `Unknown command|unknown command`)) == 0,
		strings.Join(srv.GrepFrom(before, `nknown command`), " | "))

	// ...and a camera really does take the controls.  Only assertable with a
	// subject to follow: with nobody else in the map `Cam_Cycle` finds nothing,
	// the camera stays on the observer itself and DoObserver takes the plain
	// arm, which is correct and is not what this row is about.
	// How many players this client can see, counted off the wire: a non-empty
	// `playerskins` configstring is one, whichever base the server negotiated.
	others := 0
	for _, ext := range []bool{true, false} {
		base := colosseum.PlayerSkinBase(ext)
		n := 0
		for i := 0; i < 16; i++ {
			if b.ConfigString(base+i) != "" {
				n++
			}
		}
		if n > others {
			others = n
		}
	}
	if others > 1 {
		check(rs+"/a camera takes the controls", b.PMType() != playtest.PMSpectator,
			"pmtype=%d following one of %d players -- a camera drives the view",
			b.PMType(), others)
	} else {
		check(rs+"/a camera takes the controls", true,
			"SKIPPED: nobody to follow, so the camera stays on this client")
	}

	// ...and back out again.  Under ctf that is `team red`, not `observer`:
	// Threewave's front door is the join menu and CTFJoinTeam is what clears
	// the flag and the camera.
	before = srv.Len()
	if rs == "ctf" {
		b.Cmd("team red")
		time.Sleep(1500 * time.Millisecond)
		check(rs+"/joining a team ends the watch",
			len(srv.GrepFrom(before, `joined the .* team`)) > 0 && !b.Spectating(),
			"pmtype=%d", b.PMType())
		return
	}
	b.Cmd("observer")
	time.Sleep(1500 * time.Millisecond)
	{
		check(rs+"/observer left", len(srv.GrepFrom(before, `left observer mode`)) > 0,
			strings.Join(srv.GrepFrom(before, `observer mode`), " | "))
		check(rs+"/playing again", !b.Spectating(), "pmtype=%d", b.PMType())
	}
}

func equal(a, b []string) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}

// settle gives the rest of a multi-line console block time to arrive.
func settle() { time.Sleep(400 * time.Millisecond) }

func settled[T any](parse func(string) (T, error), srv *playtest.Server) (T, error) {
	settle()
	return parse(strings.Join(srv.Log(), "\n"))
}

// extras runs the servers that need a cvar the battery does not set.
func extras() {
	// 1. ctf WITH the runes modifier: the matrix accepts it here, and the
	//    consequence is techs in the world -- the same census that reported
	//    "0 tech(es)" with runes off.
	port := nextPort()
	fmt.Printf("\n##### ctf + runes (port %d)\n", port)
	if st, log, err := boot("ctf", "q2ctf1", port, map[string]string{
		"runes": "1", "hook": "1", "teamplay": "1",
	}); err != nil {
		check("ctf+runes/boot", false, err.Error())
	} else {
		check("ctf+runes/accepted", st.Runes && st.Hook && st.Teamplay,
			"runes=%v hook=%v teamplay=%v", st.Runes, st.Hook, st.Teamplay)
		// Techs are CTF's own content and spawn whether or not the modifier
		// was asked for -- Threewave gates them on DF_CTF_NO_TECH alone.  So
		// this is not evidence that `runes` did anything; it is evidence that
		// asking for it did not take the techs AWAY.
		check("ctf+runes/techs still there", st.CTF != "" && !strings.Contains(st.CTF, "0 tech"), st.CTF)
		check("ctf+runes/not refused", !strings.Contains(log, "does not accept modifier"), "")
	}

	// 2. dm WITH teamplay: the matrix forbids it, and that is one
	//    message naming ruleset and modifier, never a gi.error -- so the
	//    server must still be serving afterwards.
	//
	//    THIS ROW USED TO BE `dm + runes` AND THE GAME OUTGREW IT.  Under
	//    baseq2's dm the matrix refused runes; `dm` is OSP's RegularDM since
	//    spec 1.36 and runes are its own, so that pair is now an ACCEPTANCE and
	//    is checked as one below.  What the flattening refuses instead is
	//    `teamplay`, and for a reason worth having a row about: team play is a
	//    ruleset now (`tdm`, `duel`), so a modifier that also reached it would
	//    be a second selector, which this library does not have.
	port = nextPort()
	fmt.Printf("\n##### dm + teamplay, which the matrix forbids (port %d)\n", port)
	if st, log, err := boot("dm", "q2dm1", port, map[string]string{"teamplay": "1"}); err != nil {
		check("dm+teamplay/survives the refusal", false, err.Error())
	} else {
		check("dm+teamplay/survives the refusal", true, "server still answering")
		check("dm+teamplay/refused by name", strings.Contains(log, "does not accept modifier") &&
			strings.Contains(log, "teamplay"), firstMatch(log, "does not accept modifier"))
		check("dm+teamplay/modifier is off", !st.Teamplay, "teamplay=%v", st.Teamplay)
		// The refusal names where team play went, which is the half an operator
		// needs -- "not accepted" alone leaves them guessing.
		check("dm+teamplay/names the replacement",
			strings.Contains(log, "g_ruleset tdm"), firstMatch(log, "team play is a ruleset"))
	}

	// 2a. ...and the same modifier under `tdm`, where the answer is different:
	//     refused too, but because that ruleset IS team play rather than because
	//     it has none.  Two refusals with two reasons is the pair that shows the
	//     message is derived rather than fixed.
	port = nextPort()
	fmt.Printf("\n##### tdm + teamplay, refused for the other reason (port %d)\n", port)
	if _, log, err := boot("tdm", "q2dm1", port, map[string]string{"teamplay": "1"}); err != nil {
		check("tdm+teamplay/boot", false, err.Error())
	} else {
		check("tdm+teamplay/refused as redundant",
			strings.Contains(log, "IS team play"), firstMatch(log, "IS team play"))
	}

	// 2b. dm WITH runes, which the matrix now ACCEPTS.  The derivation runs
	//     here exactly as it does under ctf: the cvar is a request that can only
	//     turn something on, `runes_enable` is the switch it turns on, and the
	//     modifier then reports the switch.  Without this row the acceptance
	//     half of the matrix is untested under the four rulesets that gained it.
	port = nextPort()
	fmt.Printf("\n##### dm + runes, which the matrix now accepts (port %d)\n", port)
	if st, log, err := boot("dm", "q2dm1", port, map[string]string{"runes": "1"}); err != nil {
		check("dm+runes/boot", false, err.Error())
	} else {
		check("dm+runes/not refused",
			!strings.Contains(log, "does not accept modifier"),
			firstMatch(log, "does not accept modifier"))
		check("dm+runes/modifier is on", st.Runes, "runes=%v", st.Runes)
	}

	// 2b. The modifier resolution, as the pair of observations that can refute
	//     it.  A modifier now reports
	//     the ruleset's OWN switch, and the cvar is folded into that switch
	//     beforehand as a request that can only turn something on.  Under ctf
	//     the switch is DF_CTF_NO_TECH (524288), so:
	//       dmflags 524288, runes 0  ->  no techs, runes=false
	//       dmflags 524288, runes 1  ->  the flag is CLEARED, techs, runes=true
	//     The first row is the control: without it, "runes=true" under ctf is
	//     just the default and says nothing about the derivation.
	port = nextPort()
	fmt.Printf("\n##### ctf with techs turned off by dmflags (port %d)\n", port)
	if st, _, err := boot("ctf", "q2ctf1", port, map[string]string{
		"dmflags": "524288",
	}); err != nil {
		check("ctf-notech/boot", false, err.Error())
	} else {
		check("ctf-notech/no techs", strings.Contains(st.CTF, "0 tech"), st.CTF)
		check("ctf-notech/runes reports off", !st.Runes, "runes=%v", st.Runes)
	}
	port = nextPort()
	fmt.Printf("\n##### ctf, techs off by dmflags, runes 1 asked for (port %d)\n", port)
	if st, log, err := boot("ctf", "q2ctf1", port, map[string]string{
		"dmflags": "524288", "runes": "1",
	}); err != nil {
		check("ctf-notech-runes/boot", false, err.Error())
	} else {
		check("ctf-notech-runes/dmflag cleared",
			strings.Contains(log, "clears DF_CTF_NO_TECH"),
			firstMatch(log, "DF_CTF_NO_TECH"))
		check("ctf-notech-runes/techs came back",
			st.CTF != "" && !strings.Contains(st.CTF, "0 tech"), st.CTF)
		check("ctf-notech-runes/runes reports on", st.Runes, "runes=%v", st.Runes)
	}
	//     And the other half of the same decision: `bots` IS the switch, so
	//     asking for `bots 0` under a ruleset that accepts them turns the layer
	//     off and G_BotsAllowed() -- which is what `sv ruleset` prints -- says
	//     so.  The battery above already checks bots=true by default.
	port = nextPort()
	fmt.Printf("\n##### dm with the bot layer switched off (port %d)\n", port)
	if st, _, err := boot("dm", "q2dm1", port, map[string]string{
		"bots": "0",
	}); err != nil {
		check("dm-nobots/boot", false, err.Error())
	} else {
		check("dm-nobots/predicate is off", !st.Bots, "bots=%v", st.Bots)
	}

	// 3. The OSP four: each with its own banner, its own
	//    `match_type` serverinfo string and its own set of accepted client
	//    commands.  These were four values of `match_mode` on one `tourney`
	//    ruleset until spec 1.36 made them four rulesets.
	modes := []struct {
		rs, matchType, banner string
		accepts, rejects      []string
	}{
		{"dm", "RegularDM", "REGULAR DEATHMATCH",
			[]string{"highscores"}, []string{"queue", "captain"}},
		{"dmpro", "QualifierDM", "DM QUALIFIER",
			nil, []string{"queue", "captain"}},
		{"tdm", "TeamPlay", "DM TEAM-PLAY MODE",
			[]string{"captain", "lockteam"}, []string{"queue", "highscores"}},
		{"duel", "1-vs-1", "DM 1V1 MODE",
			[]string{"queue", "line", "order"}, []string{"captain", "highscores"}},
	}
	for _, m := range modes {
		modeCheck(nextPort(), m.rs, m.matchType, m.banner, m.accepts, m.rejects)
	}

	// The out-of-range arm is gone with the cvar it validated: a mode is
	// a `g_ruleset` value now and there is no number to sit outside a range.
	// Nothing replaces it here.  `g_ruleset tourney` is not a special case
	// either -- it is simply not a ruleset name, and the unknown-value
	// path is `bootmatrix.sh`'s `banana` control.  A row asserting that
	// `tourney` in particular falls back would be asserting it is still special.

	// 4. ctf WITH protocol extensions: the two dropped rows are dropped
	//    only because slots 32/33 do not exist without them.  Turn them on and
	//    the drop must disappear -- which is the control that proves the drop
	//    was the numbering and not a broken row.
	port = nextPort()
	fmt.Printf("\n##### ctf + protocol extensions (port %d)\n", port)
	if _, log, err := boot("ctf", "q2ctf1", port, map[string]string{
		"g_protocol_extensions": "1",
	}); err != nil {
		check("ctf+ext/boot", false, err.Error())
	} else if sl, err := colosseum.ParseSlots(log); err != nil {
		check("ctf+ext/sv slots", false, err.Error())
	} else {
		check("ctf+ext/extensions on", sl.Extended, "extensions=%v", sl.Extended)

		// WHICH OUTCOME IS CORRECT DEPENDS ON THE ABI, and getting that wrong
		// in either direction is worse than not checking.  ctf's timer pair is
		// at 32/33, which exists only when BOTH the wire carries 64 slots and
		// the library's player_state_t holds 64.  Extensions supply the first;
		// the game ABI supplies the second, and `make API=old` is a supported
		// configuration where it does not.
		//
		// So this row asserts the pair resolves on a 3302 library and asserts it
		// is DROPPED on a 3 library -- the drop is the designed degradation
		//Not a defect, and a check that failed there would be
		// reporting correct behaviour as broken. Before Slots.Api existed this
		// row could not tell the two apart and did exactly that.
		if sl.Reach() > 32 {
			check("ctf+ext/timer rows resolve", len(sl.Dropped) == 0 &&
				sl.ByName["SID_TIMER2"] > 0,
				"api %d, reach %d: dropped %v, SID_TIMER2 at %d",
				sl.Api, sl.Reach(), sl.Dropped, sl.ByName["SID_TIMER2"])
		} else {
			_, t2 := sl.Dropped["SID_TIMER2"]
			_, ti := sl.Dropped["SID_TIMER2_ICON"]
			check("ctf+ext/timer rows drop (old ABI)", t2 && ti,
				"api %d, reach %d: dropped %v -- 32/33 cannot exist here",
				sl.Api, sl.Reach(), sl.Dropped)
		}
	}
}

// modeCheck covers one of the four OSP rulesets: the banner, the
// serverinfo string, and the commands that ruleset does and does not accept.
//
// The command half is the part worth having.  Every one of tourney's 63 client
// commands is in the same dispatcher under all four; what changes is the guard
// in front of it, and a guard that stopped guarding would look exactly like a
// working server until somebody typed `captain` in a duel.
func modeCheck(port int, mode, matchType, banner string, accepts, rejects []string) {
	fmt.Printf("\n##### %s -- %s (port %d)\n", mode, matchType, port)
	d := fmt.Sprintf("%s/mode-%s", *dir, mode)
	if err := colosseum.Install(d, *ref, *ctfref, *lib); err != nil {
		check("mode "+mode+"/install", false, err.Error())
		return
	}
	srv := &playtest.Server{
		Binary: *q2proded, Dir: d, Game: "colosseum", Map: "q2dm1", Port: port,
		MaxClients: 8, LogPath: d + "/server.log",
		Cvars: map[string]string{
			"g_ruleset": mode, "cheats": "1", "deathmatch": "1",
			"coop": "0",
		},
	}
	if err := srv.Start(); err != nil {
		check("mode "+mode+"/boot", false, err.Error())
		return
	}
	defer srv.Stop()
	settle()
	log := strings.Join(srv.Log(), "\n")

	check("mode "+mode+"/banner", strings.Contains(log, banner), firstMatch(log, "Mode:"))

	// `match_type` is serverinfo, so a client or a stats tool reads it -- which
	// is why it is checked from outside rather than from the console banner.
	srv.Console("serverinfo")
	srv.WaitLog(`match_type`, 5*time.Second)
	settle()
	info := strings.Join(srv.Log(), "\n")
	check("mode "+mode+"/match_type", strings.Contains(info, matchType),
		firstMatch(info, "match_type"))

	b := playtest.NewBot("mode"+mode, "127.0.0.1", port)
	if err := b.Start(30 * time.Second); err != nil {
		check("mode "+mode+"/client", false, err.Error())
		return
	}
	defer b.Disconnect()

	for _, c := range append(append([]string{}, accepts...), rejects...) {
		b.Cmd(c)
	}
	time.Sleep(1500 * time.Millisecond)
	unknown := srv.Grep(`Unknown command`)
	check("mode "+mode+"/no unknown command", len(unknown) == 0,
		strings.Join(unknown, " | "))

	// A command this mode does not have must be REFUSED, not silently ignored:
	// the mod tells the player which mode they are in.
	//
	// WAITED FOR, NOT SLEPT FOR.  The refusal is a print the server sends in
	// answer, so the question is "did one arrive", and a fixed window answers a
	// different one -- "did it arrive within 1.5s of a busy host" -- which is how
	// this row read "0 print(s) back" once on a CPU-starved machine and passed
	// 204 of 204 on the same library an hour later.  Same test as before, the
	// word "mode" or "not " in any print, asked until it is true or 8s pass.
	if len(rejects) > 0 {
		_, err := b.WaitPrint(`(?i)mode|not `, 8*time.Second)
		check("mode "+mode+"/refuses foreign commands", err == nil,
			"%d print(s) back", len(b.Prints()))
	}
}

// boot brings a server up, asks it both diagnostics, and returns the parsed
// ruleset state together with the whole console.
func boot(rs, mp string, port int, cv map[string]string) (colosseum.State, string, error) {
	d := fmt.Sprintf("%s/extra-%d", *dir, port)
	if err := colosseum.Install(d, *ref, *ctfref, *lib); err != nil {
		return colosseum.State{}, "", err
	}
	all := map[string]string{"g_ruleset": rs, "cheats": "1", "deathmatch": "1", "coop": "0"}
	for k, v := range cv {
		all[k] = v
	}
	srv := &playtest.Server{
		Binary: *q2proded, Dir: d, Game: "colosseum", Map: mp, Port: port,
		MaxClients: 8, Cvars: all, LogPath: d + "/server.log",
	}
	if err := srv.Start(); err != nil {
		return colosseum.State{}, "", err
	}
	defer srv.Stop()

	// Late enough for the entities that spawn on a think rather than at load:
	// CTF scatters its techs two seconds in, so a census taken at boot reports
	// a world that is still filling.
	time.Sleep(3 * time.Second)
	srv.Console("sv ruleset")
	if _, err := srv.WaitLog(`^world `, 5*time.Second); err != nil {
		return colosseum.State{}, strings.Join(srv.Log(), "\n"), fmt.Errorf("sv ruleset never answered")
	}
	settle()
	st, err := colosseum.ParseRuleset(strings.Join(srv.Log(), "\n"))
	if err != nil {
		return st, strings.Join(srv.Log(), "\n"), err
	}
	srv.Console("sv slots")
	srv.WaitLog(`^statusbar `, 5*time.Second)
	settle()
	return st, strings.Join(srv.Log(), "\n"), nil
}

func firstMatch(log, sub string) string {
	for _, l := range strings.Split(log, "\n") {
		if strings.Contains(l, sub) {
			return strings.TrimSpace(l)
		}
	}
	return ""
}

func ctfChecks(srv *playtest.Server, b *playtest.Bot, rs string) {
	// Half one: joining a team overwrites the player's skin with the
	// team skin, in the player configstring the whole server sees.
	base := colosseum.PlayerSkinBase(true)
	if b.ConfigString(base) == "" {
		base = colosseum.PlayerSkinBase(false)
	}
	before := b.ConfigString(base)
	b.Cmd("team red")
	joined, err := b.WaitConfigString(base, `ctf_r`, 6*time.Second)
	if !check(rs+"/join sets team skin", err == nil, "%q -> %q", colosseum.SkinOf(before), colosseum.SkinOf(joined)) {
		return
	}
	// Half two: a userinfo change must not lose it again.
	b.SetUserinfo("skin", "female/jezebel")
	time.Sleep(1200 * time.Millisecond)
	after := b.ConfigString(base)
	skin := colosseum.SkinOf(after)
	check(rs+"/team skin survives userinfo", strings.HasSuffix(skin, "ctf_r"),
		"%q -- model may change, team skin may not", skin)

	// The tech menu and the hook are CTF's, and both are client commands.
	b.Cmd("id")
	b.Cmd("hook")
	time.Sleep(500 * time.Millisecond)
	check(rs+"/ctf commands accepted", len(srv.Grep(`Unknown command`)) == 0, "")
}

func arenaChecks(srv *playtest.Server, b *playtest.Bot, rs string) {
	// A fresh arena client is an OBSERVER, not a fighter: RA2 puts everyone in
	// the audience until they join an arena.  The pmove type is the honest
	// question -- a mod's own HUD can say anything.
	check(rs+"/new client observes", b.Spectating(), "pmtype=%d", b.PMType())

	// The join menu is arena's front door.  It is drawn by overwriting this
	// client's statusbar, so the harness can read it -- and `inven` is what
	// toggles it, which is worth checking in both directions: closing a menu
	// must hide it, not destroy it, or nothing can bring it back.
	//
	// The state on ENTRY is not known and must not be assumed.  A client
	// connects already holding the menu, and `score` deliberately leaves it up
	// under arena, so a fixed open/close/open sequence reads the parity of
	// everything that ran before it rather than the toggle.  Drive it to a known
	// state first; that is also a check, because the close is half the claim.
	if b.MenuTitle() != "" {
		b.Cmd("inven")
		time.Sleep(700 * time.Millisecond)
	}
	check(rs+"/inven closes the menu it arrived with", b.MenuTitle() == "", "%q", b.MenuTitle())

	b.Cmd("inven")
	if err := b.WaitMenu(`(?i)arena|team|rocket`, 6*time.Second); err != nil {
		check(rs+"/inven reopens the menu", false, err.Error())
		return
	}
	title, items := b.Menu()
	check(rs+"/inven reopens the menu", len(items) > 0, "%q, %d row(s)", title, len(items))

	b.Cmd("inven")
	time.Sleep(700 * time.Millisecond)
	check(rs+"/inven closes it again", b.MenuTitle() == "", "%q", b.MenuTitle())

	b.Cmd("inven")
	time.Sleep(700 * time.Millisecond)
	check(rs+"/and opens it a third time", b.MenuTitle() != "", "%q", b.MenuTitle())

	// Leaving a team must hand the menu BACK.  init_player() rebuilds the team
	// list, and until Colosseum 1.28 UseMenu() then unlinked it again on its way
	// out -- the menu it destroyed was the one the callback had just built, so a
	// player who pressed Leave Team was left in arena 0 with no menu, unable to
	// rejoin or spawn, and `inven` could not help because its reopen tests the
	// pointer that had been nulled.
	if err := b.MenuPick(`Start New Team`, 20*time.Second); err != nil {
		check(rs+"/joined a team to leave", false, err.Error())
		return
	}
	time.Sleep(1500 * time.Millisecond)
	if err := b.MenuPick(`Leave Team`, 20*time.Second); err != nil {
		check(rs+"/Leave Team is offered", false, err.Error())
		return
	}
	time.Sleep(2 * time.Second)
	check(rs+"/leaving hands the menu back", b.MenuTitle() != "", "%q", b.MenuTitle())

	b.Cmd("inven")
	time.Sleep(700 * time.Millisecond)
	b.Cmd("inven")
	time.Sleep(700 * time.Millisecond)
	check(rs+"/and inven still toggles it", b.MenuTitle() != "", "%q", b.MenuTitle())
}

func tourneyChecks(srv *playtest.Server, b *playtest.Bot, rs string) {
	// Tourney's four statusbars are chosen by m_mode; whichever one this is,
	// the client must have one and it must be tourney's.
	check(rs+"/client has a bar", len(b.StatusBar()) > 0, "%d bytes", len(b.StatusBar()))

	// The mod's own client commands must reach OSP_ClientCommand rather than
	// falling through to "unknown command".
	for _, c := range []string{"settings", "players", "matchinfo", "stats"} {
		b.Cmd(c)
	}
	time.Sleep(800 * time.Millisecond)
	check(rs+"/mod commands accepted", len(srv.Grep(`Unknown command`)) == 0,
		strings.Join(srv.Grep(`Unknown command`), " | "))

	// R-OSP-14 -- WHICH SCREEN THE MOTD WINDOW DRAWS.
	//
	// OSP_setStats shows page 2, the MOTD, for the first ten seconds after
	// resp.osp_r0ac is stamped -- which is ClientBegin -- and re-arms it on
	// every frame the client has nothing up, which is why the board cannot be
	// dismissed inside the window.  That part is the donor's.  What it DRAWS is
	// the claim: the donor's name for its own page dispatcher is baseq2's board
	// here, so the row is what has to answer, and nothing but a client can say
	// which screen arrived.
	//
	// A CONNECTING client is the only one that can be asked: the bot this
	// battery has been driving is long past its ten seconds.  The first layout
	// it receives is the window's, and the MOTD is identified by the mod's own
	// credit lines, which every MOTD carries whether or not the server set one.
	//
	// `dm` ALONE, and that is a scope rather than a doubt.  The window is
	// OSP_setStats' and is the same code for all four, but its one suppressor
	// is `menu_owner == MENU_TOURNEY`, and what a client is shown on connecting
	// is not the same question under a mode with teams.  A row that asserts a
	// layout where a menu is legitimately holding the channel would be a row
	// that fails for the wrong reason, and skipping it there reads as a pass.
	//
	// The one way this can pass over a tree that has it wrong: OSP_setStats and
	// the 32-frame redraw both run inside ClientEndServerFrame, so on 1 frame in
	// 32 the first layout is overwritten by the gated redraw before the client
	// sees it.  It cannot fail over a correct one, which is the direction that
	// matters.
	if rs != "dm" {
		return
	}
	nb := playtest.NewBot(rs+"-motd", "127.0.0.1", srv.Port)
	if err := nb.Start(30 * time.Second); err != nil {
		check(rs+"/MOTD window: a client connects", false, "%v", err)
		return
	}
	defer nb.Disconnect()
	lay, err := nb.WaitLayout(`\S`, 8*time.Second)
	if err != nil {
		check(rs+"/MOTD window draws OSP's page", false,
			"no layout at all in the ten-second window")
		return
	}
	check(rs+"/MOTD window draws OSP's page, not baseq2's board",
		strings.Contains(lay, "OrangeSmoothie"), "%.70q", lay)
}

// ---------------------------------------------------------------- bots
//
// These rows add, over tools/botmatrix.sh, a PERSON --
// or the nearest a script gets to one.  botmatrix.sh boots a server, adds bots
// and reads two `sv` dumps: it can say a bot exists and that removing it
// returns the slot.  It cannot say that a client connecting to that server sees
// the bots, that the bot wears its team's skin on the wire, that the scoreboard
// has the bot's name on it, or that a bot gets out of the way when a human
// wants its slot.  Every one of those needs a client, and every one of them is
// what "bots play in ctf" actually means.
func botRows() {
	if *gladdir == "" {
		check("bots/skipped", true, "no -gladdir, so the bot rows did not run")
		return
	}

	// Both halves.  The requirement is "fill every client slot with
	// bots, connect a human, and verify a bot RELOCATES rather than the
	// connection being refused", and measuring it shows those are two
	// different servers rather than one:
	//
	//   * a bot moves when the slot the engine hands the human is a bot's AND
	//     another slot is free.  Bots take the HIGHEST free slot
	//     and the engine hands a human the lowest free one, so they only meet
	//     after a high-slot bot has left -- which is what the first row
	//     arranges, by name, out of `sv clientdump`.
	//   * with every slot taken there is nowhere to move to, and the
	//     own answer is to REFUSE the human rather than steal the slot: the
	//     brain would otherwise be talking about a client that is now somebody
	//     else.  That is the second row, and "Server is full" is the rejmsg.
	slotRows()

	// The SIGFPE row, live.  OSP_votePercent divides by a head count
	// that reaches zero the moment the last human leaves a server that still
	// has bots on it, and it is called EVERY FRAME while a vote is running --
	// so the crash needs a vote in progress and nobody left to count.  A
	// client is the only thing that can start one.
	voteRow(nextPort())

	// And the three rulesets, each with a client watching.
	for _, rs := range []struct {
		name, mp string
		cv       map[string]string
	}{
		{"ctf", "q2ctf1", nil},
		{"arena", "q2dm1", nil},
		// match_countdown is clamped to a floor of 14 and defaults to 30, so
		// the row otherwise spends up to half a minute of its window watching
		// a countdown rather than a fight.
		{"tdm", "q2dm1", map[string]string{
			"match_countdown": "14",
		}},
	} {
		botsIn(rs.name, rs.mp, nextPort(), rs.cv)
	}
}

// startBotServer installs a gamedir with the brain in it and boots one.
func startBotServer(tag, rs, mp string, port, maxclients int, cv map[string]string) (*playtest.Server, error) {
	d := fmt.Sprintf("%s/%s", *dir, tag)
	if err := colosseum.Install(d, *ref, *ctfref, *lib); err != nil {
		return nil, err
	}
	if err := colosseum.InstallBrain(d, *gladdir); err != nil {
		return nil, err
	}
	c := map[string]string{
		"g_ruleset": rs, "cheats": "1", "skill": "1",
		// Both spellings, because which one is authoritative is the ruleset's
		// and these rows set up their rosters by hand.
		"minimumplayers": "0", "bots_minplayers": "0",
	}
	for k, v := range cv {
		c[k] = v
	}
	srv := &playtest.Server{
		Binary: *q2proded, Dir: d, Game: "colosseum", Map: mp,
		Port: port, MaxClients: maxclients, Cvars: c, LogPath: d + "/server.log",
	}
	if err := srv.Start(); err != nil {
		return nil, err
	}
	return srv, nil
}

// addBots adds n bots and waits until the library reports all of them.
func addBots(srv *playtest.Server, n int) error {
	for i := 0; i < n; i++ {
		srv.Console("sv addrandom")
		time.Sleep(300 * time.Millisecond)
	}
	deadline := time.Now().Add(20 * time.Second)
	for time.Now().Before(deadline) {
		srv.Console("sv ruleset")
		time.Sleep(500 * time.Millisecond)
		if b, err := colosseum.ParseBots(strings.Join(srv.Log(), "\n")); err == nil && b.Bots >= n {
			return nil
		}
	}
	return fmt.Errorf("only %d of %d bots appeared", botCount(srv), n)
}

func botCount(srv *playtest.Server) int {
	b, err := colosseum.ParseBots(strings.Join(srv.Log(), "\n"))
	if err != nil {
		return -1
	}
	return b.Bots
}

// clientDump returns slot -> name for every occupied slot, out of `sv clientdump`.
//
//	16: Byte             .../gladiator.so
//	17: player           human
//	 3: -
var reDumpRow = regexp.MustCompile(`(?m)^ *(\d+): (.*)$`)

func clientDump(srv *playtest.Server) map[int]string {
	// The LAST dump only: a scenario asks more than once.
	log := strings.Join(srv.Log(), "\n")
	i := strings.LastIndex(log, " bots, ")
	if i < 0 {
		return nil
	}
	j := strings.LastIndex(log[:i], "\n  0: ")
	if j < 0 {
		j = 0
	}
	out := map[int]string{}
	for _, m := range reDumpRow.FindAllStringSubmatch(log[j:i], -1) {
		n, _ := strconv.Atoi(m[1])
		row := strings.TrimSpace(m[2])
		if row == "-" {
			continue
		}
		// The name is fixed-width padded to 16 and the library path follows.
		name := row
		if len(row) > 16 {
			name = strings.TrimSpace(row[:16])
		}
		out[n] = name
	}
	return out
}

func slotRows() {
	const max = 4
	port := nextPort()
	fmt.Printf("\n##### a bot gets out of a human's way (port %d)\n", port)

	srv, err := startBotServer("slots", "dm", "q2dm1", port, max, nil)
	if err != nil {
		check("slots/boot", false, err.Error())
		return
	}
	defer srv.Stop()

	if err := addBots(srv, max); err != nil {
		check("slots/fill", false, err.Error())
		return
	}
	srv.Console("sv clientdump")
	time.Sleep(600 * time.Millisecond)
	dump := clientDump(srv)
	top := dump[max-1]
	if !check("slots/filled", len(dump) == max && top != "",
		"%d slot(s) occupied, top is %q", len(dump), top) {
		return
	}

	// Free the TOP slot, so the bot the engine will collide with is in slot 0
	// and there is somewhere for it to go.
	// Quoted: the roster's names have spaces in them ("Java Man", "Steroid
	// Stud"), and gi.argv would otherwise hand BotRemoveDeathmatch the first
	// word, match nothing, and remove nothing.
	srv.Console("sv removebot \"%s\"", top)
	time.Sleep(800 * time.Millisecond)
	srv.Console("sv ruleset")
	time.Sleep(600 * time.Millisecond)
	b, _ := colosseum.ParseBots(strings.Join(srv.Log(), "\n"))
	if !check("slots/one removed", b.Bots == max-1, "%d bot(s) left", b.Bots) {
		return
	}
	occupiedLow := false
	for _, s := range b.Slots {
		if s == 0 {
			occupiedLow = true
		}
	}
	check("slots/bot holds slot 0", occupiedLow, "bots at %v", b.Slots)

	p := playtest.NewBot("human", "127.0.0.1", port)
	if err := p.Start(15 * time.Second); err != nil {
		check("slots/human connects", false, err.Error())
		return
	}
	check("slots/human connects", true, "connected to a server whose slot 0 held a bot")
	p.WaitFrames(20, 10*time.Second)

	srv.Console("sv ruleset")
	srv.Console("sv clientdump")
	time.Sleep(800 * time.Millisecond)
	b2, _ := colosseum.ParseBots(strings.Join(srv.Log(), "\n"))
	// The bot count is unchanged: it MOVED, it did not die.  The whole
	// point is that the bot survives the move, and the donor's memcpy left the
	// new slot's gclient_t unfilled.
	check("slots/bot relocated, not destroyed", b2.Bots == max-1,
		"%d bot(s) after the human connected, at %v", b2.Bots, b2.Slots)
	check("slots/human is counted", b2.Clients == max, "%d client(s)", b2.Clients)
	moved := true
	for _, s := range b2.Slots {
		if s == 0 {
			moved = false
		}
	}
	check("slots/slot 0 is the human's now", moved, "bots at %v", b2.Slots)

	// The relocated bot must still be a bot to the brain as well as to the
	// game: FL_BOT and FL_BOTCLIENT are two different bits and a
	// move that carried one and not the other would look right here and be
	// wrong everywhere else.
	check("slots/both bot bits survived the move",
		b2.Fields["FL_BOTCLIENT"] == b2.Bots,
		"FL_BOT=%d FL_BOTCLIENT=%d", b2.Bots, b2.Fields["FL_BOTCLIENT"])
	p.Disconnect()

	// Second half: every slot a bot, and the human is refused BY NAME.
	full := nextPort()
	fmt.Printf("\n##### a full server refuses rather than steals (port %d)\n", full)
	srv2, err := startBotServer("slotsfull", "dm", "q2dm1", full, max, nil)
	if err != nil {
		check("slotsfull/boot", false, err.Error())
		return
	}
	defer srv2.Stop()
	if err := addBots(srv2, max); err != nil {
		check("slotsfull/fill", false, err.Error())
		return
	}
	p2 := playtest.NewBot("human2", "127.0.0.1", full)
	err = p2.Start(12 * time.Second)
	check("slotsfull/human refused", err != nil,
		"connect returned %v", err)
	srv2.Console("sv ruleset")
	time.Sleep(600 * time.Millisecond)
	b3, _ := colosseum.ParseBots(strings.Join(srv2.Log(), "\n"))
	check("slotsfull/no bot was stolen", b3.Bots == max,
		"%d bot(s) still there", b3.Bots)
	if err == nil {
		p2.Disconnect()
	}
}

// botsIn is the author's play test, mechanised: a client connects to a server
// that has bots in it and is asked what it can see.
func botsIn(rs, mp string, port int, cv map[string]string) {
	const n = 4
	fmt.Printf("\n##### %s with %d bots, watched by a client (port %d)\n", rs, n, port)

	srv, err := startBotServer("bots-"+rs, rs, mp, port, 12, cv)
	if err != nil {
		check(rs+"+bots/boot", false, err.Error())
		return
	}
	defer srv.Stop()
	if err := addBots(srv, n); err != nil {
		check(rs+"+bots/spawn", false, err.Error())
		return
	}

	// Counted BEFORE the client connects.  The bots have been in the game
	// since addBots returned and may already have killed each other, and
	// WaitLog scans the backlog -- so waiting for "an obituary" matched a line
	// from before the client existed and then asked the client why it had not
	// seen it.  The question is whether a kill happens WHILE somebody is
	// watching, so the count has to move.
	obitsBefore := len(srv.Grep(obitRE))

	p := playtest.NewBot("watcher", "127.0.0.1", port)
	if err := p.Start(15 * time.Second); err != nil {
		check(rs+"+bots/client connects", false, err.Error())
		return
	}
	defer p.Disconnect()

	// Wait for the FIGHTING rather than for a fixed number of frames.  Under
	// arena a round does not begin until two teams are populated and the
	// countdown has run, and under a match ruleset it waits for the ready-up,
	// so "30 seconds" is a number about this machine and not about the game.
	// Waiting on the server's own obituary also separates the two things that
	// can go wrong: the bots did not fight, or they fought and the client was
	// not told.
	// The window is the ruleset's setup cost plus room for a fight, and the
	// ones with a match machine need both.  Under `arena` a round waits for two
	// populated teams and a countdown, and the machine re-runs that between
	// rounds; under `dmpro`, `tdm` and `duel` the match waits for the ready-up
	// and then `match_countdown` seconds, during which nobody can be hurt.
	// `ctf` and `dm` are fighting from the first frame a bot has a weapon --
	// `dm` is OSP's RegularDM, which has no ready gate at all.
	wait := 150 * time.Second
	if rs == "arena" || rs == "dmpro" || rs == "tdm" || rs == "duel" {
		wait = 300 * time.Second
	}
	started := time.Now()
	fought := false
	for time.Since(started) < wait {
		if len(srv.Grep(obitRE)) > obitsBefore {
			fought = true
			break
		}
		time.Sleep(2 * time.Second)
	}
	check(rs+"+bots/they fight", fought,
		"a NEW obituary reached the server console after %ds",
		int(time.Since(started).Seconds()))
	p.WaitFrames(20, 15*time.Second)

	srv.Console("sv ruleset")
	time.Sleep(800 * time.Millisecond)
	b, err := colosseum.ParseBots(strings.Join(srv.Log(), "\n"))
	if err != nil {
		check(rs+"+bots/census", false, err.Error())
		return
	}
	check(rs+"+bots/still there with a human watching", b.Bots == n,
		"%d bot(s), %d client(s)", b.Bots, b.Clients)

	// What the WIRE says about each bot.  A bot that exists only in the game's
	// own bookkeeping has no name in the playerskins table, so a client cannot
	// draw it and cannot see it on a scoreboard.
	base := colosseum.PlayerSkinBase(strings.Contains(strings.Join(srv.Log(), "\n"), "extensions"))
	named, teamskinned := 0, 0
	for _, slot := range b.Slots {
		cs := p.ConfigString(base + slot)
		if cs == "" {
			continue
		}
		named++
		skin := colosseum.SkinOf(cs)
		if strings.HasSuffix(skin, "ctf_r") || strings.HasSuffix(skin, "ctf_b") {
			teamskinned++
		}
	}
	check(rs+"+bots/client can see every bot", named == n,
		"%d of %d bots have a playerskins entry", named, n)

	switch rs {
	case "ctf":
		// Forced onto a team, balanced, none left in the audience.
		check("ctf+bots/on a team", b.Fields["noteam"] == 0 &&
			b.Fields["red"]+b.Fields["blue"] == n, b.Place)
		check("ctf+bots/teams are balanced", b.Fields["red"] == b.Fields["blue"], b.Place)
		// The v0.95 half, observable for the first time: under ctf the
		// skin is the TEAM's, and ClientUserinfoChanged used to overwrite it.
		// This reads it off the wire rather than out of the source.
		check("ctf+bots/wear the team skin", teamskinned == n,
			"%d of %d bots are in ctf_r/ctf_b", teamskinned, n)
	case "arena":
		// In the selected arena, on a team of its own or on a
		// pickup team, and not standing outside in arena 0.
		check("arena+bots/in an arena", b.Fields["in-arena"] == n, b.Place)
		check("arena+bots/on a team", b.Fields["on-team"] == n, b.Place)
	case "dm", "dmpro", "tdm", "duel":
		// Entered, counted, readied up, and split across the two
		// teams -- which is the whole of "bots join teams and are counted".
		check(rs+"+bots/entered", b.Fields["entered"] == n, b.Place)
		check(rs+"+bots/readied up", b.Fields["ready"] == n, b.Place)
		check(rs+"+bots/two teams", b.Fields["team0"] == b.Fields["team1"] &&
			b.Fields["team0"] > 0, b.Place)
	}

	// And the thing a person watching would actually notice: the client is
	// told about it.  Obituaries are gi.bprintf, so every connected client
	// gets them -- including one that is only watching, which is the case
	// this row exists to cover.
	//
	// WAITED FOR, not counted once.  The server's first obituary may already
	// have been printed when this client connected, and the print that reaches
	// the client arrives on its own schedule -- so a single count taken twenty
	// frames after the server saw a kill failed about one run in five, which is
	// the kind of check that teaches its reader to ignore it.  The window here
	// is for the NEXT kill among four fighting bots.
	kills := 0
	for deadline := time.Now().Add(90 * time.Second); time.Now().Before(deadline); {
		kills = 0
		for _, s := range p.Prints() {
			if reObit.MatchString(s) {
				kills++
			}
		}
		if kills > 0 {
			break
		}
		time.Sleep(2 * time.Second)
	}
	check(rs+"+bots/the client sees them fight", !fought || kills > 0,
		"%d obituary/ies reached the client", kills)
}

// The server-side form of the same pattern, as a WaitLog regex.  It is anchored
// to the start of a line because the console prints obituaries there and the
// bots' own chat -- "Trash: this place blows, dudes." -- is full of the same
// verbs.
const obitRE = `(?m)^[^:]+ (was|tried to|ate|almost dodged|melted|blew|died|should have|saw|couldn't|does a back flip|suicides|cratered|got|rides) `

// The baseq2 obituary set, plus the three CTF and arena add.  Matching the VERB
// rather than a name keeps this independent of which bots the roster picked.
var reObit = regexp.MustCompile(`(?i) (was|tried to|ate|almost dodged|melted|blew|died|should have|saw|couldn't|does a back flip|suicides|cratered|got|rides) `)

// voteRow is the "SIGFPE when the last human leaves a server with bots" case.
//
// The port fixed it by clamping the divisor, and a clamp is exactly the kind of
// fix that cannot be seen in the source once it is there: the line reads as if
// it were always safe.  This arranges the state that used to divide by zero and
// then asks whether the server is still answering.
func voteRow(port int) {
	fmt.Printf("\n##### a vote outliving the last human (port %d)\n", port)

	srv, err := startBotServer("vote", "tdm", "q2dm1", port, 8, map[string]string{
		// Long enough that the vote is still running after the client has gone.
		"vote_time": "60", "vote_enable": "1", "vote_enable_time": "1",
	})
	if err != nil {
		check("vote/boot", false, err.Error())
		return
	}
	defer srv.Stop()
	if err := addBots(srv, 3); err != nil {
		check("vote/bots", false, err.Error())
		return
	}

	p := playtest.NewBot("voter", "127.0.0.1", port)
	if err := p.Start(15 * time.Second); err != nil {
		check("vote/client connects", false, err.Error())
		return
	}
	p.WaitFrames(30, 15*time.Second)
	// Entering the game first: OSP_vote_cmd is refused to a client that has
	// not, and a refused vote leaves vote_inprogress at 0 -- which would make
	// this row pass without ever arranging the state it is about.
	p.Cmd("join")
	p.WaitFrames(20, 10*time.Second)
	p.Cmd("vote timelimit 15")
	if _, err := srv.WaitLog(`has initiated a vote!`, 12*time.Second); err != nil {
		check("vote/started", false, "no vote was started, so the row proves nothing")
		p.Disconnect()
		return
	}
	check("vote/started", true, "a vote is running")

	// Now take the only human away and let the per-frame division run.
	p.Disconnect()
	time.Sleep(6 * time.Second)
	srv.Console("sv ruleset")
	if _, err := srv.WaitLog(`^bots `, 10*time.Second); err != nil {
		check("vote/server survived the last human leaving", false,
			"the server stopped answering: %v", err)
		return
	}
	b, _ := colosseum.ParseBots(strings.Join(srv.Log(), "\n"))
	check("vote/server survived the last human leaving", true,
		"still answering with %d bot(s) and %d client(s)", b.Bots, b.Clients)
}
