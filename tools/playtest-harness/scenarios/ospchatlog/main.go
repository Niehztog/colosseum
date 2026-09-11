// ospchatlog -- do the three OSP stats-log events actually
// reach the file, and does the cvar that gates one of them still gate it?
//
// WHY THIS SCENARIO EXISTS.  `docs/donor-fdiff.md` sec 3.3 and 3.6 are three
// findings of one shape: a log function that EXISTS in this tree, HAS callers,
// and is not called from the site the donor calls it from.  No static check
// sees that -- every identifier is present, so no line is ABSENT and no
// exemption fires -- and no amount of reading osp_stats.c settles it either,
// because the function is fine.  Only the file the server writes does.
//
//	chat      the donor logs chat from Cmd_Say_f, which serves `say`.  This
//	          tree called OSP_Stats_Chat only from OSP_talkto_cmd, so
//	          `stats_logchat 1` recorded private messages and nothing else
//	          while the cvar's own documentation says "log chat lines".
//	drop      TossClientWeapon logs neither the quad DROPPED on death nor the
//	          one that expired on the death frame, and left resp.osp_r200 set.
//	shutdown  ShutdownGame has no counterpart for the donor's
//	          `if (!level.intermission_framenum) q2log_logAccuracy()`, so a
//	          `map` typed with a match still running ended the log without the
//	          accuracy table.  Every OSP_Stats_AccuracyAll() caller in
//	          osp_main.c is a MATCH-end condition, which is the case this one
//	          deliberately is not.
//
// EACH ROW IS ASSERTED IN BOTH SIGNS, because "the event is in the file" is
// satisfied by a build that logs unconditionally, and that is a different
// defect rather than the fix:
//
//	chat      a `say` on the logging server produces exactly one chat event
//	          carrying the text -- AND the same `say` on a control server with
//	          `stats_logchat 0` produces none.  The negative half is the cvar,
//	          which is the only thing standing between a stats file and every
//	          word said on the server.
//	drop      dying WITH an active quad under DF_QUAD_DROP writes item_drop
//	          "Quad" naming the edict the quad became -- AND dying without one
//	          writes no item_drop at all.
//	shutdown  `map` typed mid-match writes accuracy events, and writes them
//	          BEFORE the shutdown event and AFTER the last match_start with no
//	          match_end in between -- which is what distinguishes the new call
//	          from the six match-end callers that were always there.  Ordering
//	          is the whole assertion: an accuracy event on the wrong side of
//	          shutdown is the pre-fix build's six callers, not this one.
//
// WHAT THIS SCENARIO DELIBERATELY DOES NOT TEST is the second arm of `drop`,
// the expiry.  It is reachable only on the single frame where a quad runs out
// and its holder dies in the same frame -- p_view.c's per-frame reader takes
// every other frame, and player_die zeroes `quad_framenum` immediately after
// TossClientWeapon so no later frame can.  A one-frame race is not something a
// headless client can arrange, and a row that cannot fail is worse than no row.
// It is recorded here instead.
//
// TOURNEY MECHANICS THAT SHAPE THIS FILE, all of them from osprunes' notes and
// none of them obvious:
//
//   - NOTHING IS PICKABLE UNTIL A MATCH IS RUNNING.  Touch_Item returns early
//     while sync_stat < 4, and `give` reaches items THROUGH Touch_Item.  So the
//     quad rows need a real TeamPlay match: two clients, one per team, both
//     `ready`, then match_countdown seconds -- and osp_main.c clamps that cvar
//     UP to 14, so asking for less does not shorten the wait.
//   - THE MATCH START KILLS EVERYONE, and a libq2 bot cannot latch
//     BUTTON_ATTACK to get up again.  DF_FORCE_RESPAWN or every row below it
//     fails on `health < 1` inside Touch_Item and reads like a broken grant.
//   - FLOOD PROTECTION EATS CHAT past four messages in four seconds.
//     flood_msgs 0 really does disable it -- FloodProtect returns early on < 1.
package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"path/filepath"
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
	dir      = flag.String("dir", "/tmp/ospchatlog", "scratch install dir")
	port     = flag.Int("port", 27994, "UDP port for the logging server")
	ctlport  = flag.Int("ctlport", 27995, "UDP port for the stats_logchat 0 control")
	mapname  = flag.String("map", "q2dm1", "map to run")
	keep     = flag.Bool("keep", false, "keep the logs on success")

	// THE ACCURACY PHASE NEEDS A BRAIN, and no other phase does.  A libq2
	// client cannot fire -- BuildUserCommand emits an empty usercmd every
	// frame -- and OSP_Stats_Accuracy writes NOTHING for a player whose whole
	// per-weapon table is zero, so without bots the shutdown rows below would
	// pass or fail on an empty table either way.  Missing brain: skipped, with
	// the reason, rather than silently subtracted.
	glad = flag.String("gladdir", "", "gladiator-bot-restored, for the accuracy phase")

	accport = flag.Int("accport", 27996, "UDP port for the `dm`+bots accuracy server")
)

var failed, total, skipped int

// EVERY SERVER THIS SCENARIO STARTS, so report() can stop them.  `defer
// srv.Stop()` is not enough on its own: report() ends the process with
// os.Exit, which runs no deferred function, so a FAILING run used to leave
// q2proded holding the UDP port and the next run died on "Address already in
// use" -- a harness failure that reads exactly like a broken build.
var servers []*playtest.Server

func stopAll() {
	for _, s := range servers {
		s.Stop()
	}
}

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
	fmt.Printf("  %s %-46s %s\n", tag, name, note)
	return ok
}

func skip(name, why string) {
	total++
	skipped++
	fmt.Printf("  [skip] %-46s %s\n", name, why)
}

const (
	statHealth    = 1
	statArmor     = 5
	statTimerIcon = 9

	teamA, teamB = "Hometeam", "Visitors"

	matchCountdown = 14 // osp_main.c clamps UP to this; asking for less is asking for 14

	dfForceRespawn = 1 << 10 // DF_FORCE_RESPAWN
	dfQuadDrop     = 1 << 14 // DF_QUAD_DROP -- without it there is nothing to drop
)

// event is one line of the stats file.  Only the fields this scenario asserts
// on are named; the rest of the object is ignored, so a log that grows new keys
// does not break the parse.
type event struct {
	Event  string `json:"event"`
	Text   string `json:"text"`
	Item   string `json:"item"`
	Reason string `json:"reason"`
	// item_drop carries two entity numbers and the difference is the point:
	// `entity` is the edict the quad BECAME on the floor, `from` is
	// resp.osp_r200, the edict it was picked up from.  The second is the field
	// was left set, so joining it to the item_pickup that set it
	// is the assertion with content.
	Entity int `json:"entity"`
	From   int `json:"from"`
	Line   int `json:"-"` // position in the file: the ordering assertions need it
}

// readLog parses the stats file.  A malformed line is reported rather than
// skipped: this file is the thing under test, so "one line did not parse" is a
// finding and not noise.
func readLog(path string) ([]event, error) {
	b, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	var out []event
	for i, ln := range strings.Split(strings.TrimRight(string(b), "\n"), "\n") {
		if strings.TrimSpace(ln) == "" {
			continue
		}
		var e event
		if err := json.Unmarshal([]byte(ln), &e); err != nil {
			return out, fmt.Errorf("line %d does not parse as JSON: %q", i+1, ln)
		}
		e.Line = i + 1
		out = append(out, e)
	}
	return out, nil
}

func of(evs []event, kind string) []event {
	var out []event
	for _, e := range evs {
		if e.Event == kind {
			out = append(out, e)
		}
	}
	return out
}

// lastLineOf is the position of the last event of a kind, or -1.
func lastLineOf(evs []event, kind string) int {
	last := -1
	for _, e := range evs {
		if e.Event == kind {
			last = e.Line
		}
	}
	return last
}

func statsPath(root string) string {
	// G_FsGamePath composes <homedir>/<gamedir>/<statsname>, homedir is the
	// scratch dir (playtest.Server passes it) and statsname defaults to
	// osptourney.jsonl under every OSP ruleset (g_ruleset.c:319).
	return filepath.Join(root, "colosseum", "osptourney.jsonl")
}

// boot brings up one server.  Both servers in this scenario are the same except
// for stats_logchat and the port, which is exactly what makes the control a
// control.
func boot(root string, p int, logchat string) (*playtest.Server, error) {
	srv := &playtest.Server{
		Binary: *q2proded, Dir: root, Game: "colosseum",
		Map: *mapname, Port: p, MaxClients: 8,
		Cvars: map[string]string{
			"g_ruleset":       "tdm",
			"cheats":          "1",
			"deathmatch":      "1",
			"coop":            "0",
			"bots":            "0",
			"match_countdown": fmt.Sprint(matchCountdown),
			"flood_msgs":      "0",
			"statsfile":       "1",
			"stats_logchat":   logchat,
			"dmflags":         fmt.Sprint(dfForceRespawn | dfQuadDrop),
			// WITHOUT THIS `map` DOES NOTHING.  q2pro answers a console `map`
			// with "Using 'map' will cause full server restart.  Use 'gamemap'
			// ..." and changes nothing, so the shutdown rows below fail with
			// "no shutdown event" and look like the fix rather than the
			// harness.  `map` and not `gamemap` because `map` is the donor's
			// own case: port_osp:g_main.c special-cases gi.argv(0) == "map".
			"sv_allow_map": "1",
		},
		LogPath: root + "/server.log",
	}
	return srv, srv.Start()
}

func main() {
	flag.Parse()
	if *q2proded == "" || *lib == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded and -lib")
		os.Exit(2)
	}

	logDir := *dir + "/log"
	ctlDir := *dir + "/ctl"
	for _, d := range []string{logDir, ctlDir} {
		if err := colosseum.Install(d, *ref, *ctfref, *lib); err != nil {
			fmt.Fprintln(os.Stderr, "install:", err)
			os.Exit(2)
		}
		// The stats log is opened with "a".  A file left by an earlier run
		// would make every "is the event in the file" row pass on its own
		// history, which is the most embarrassing way for a scenario to lie.
		os.Remove(statsPath(d))
	}

	srv, err := boot(logDir, *port, "1")
	if err != nil {
		fmt.Fprintln(os.Stderr, "server:", err)
		os.Exit(2)
	}
	servers = append(servers, srv)
	ctl, err := boot(ctlDir, *ctlport, "0")
	if err != nil {
		fmt.Fprintln(os.Stderr, "control server:", err)
		os.Exit(2)
	}
	servers = append(servers, ctl)
	time.Sleep(3 * time.Second)

	fmt.Printf("\n##### both servers survived loading the map\n")
	for _, s := range []struct {
		name string
		srv  *playtest.Server
	}{{"logging", srv}, {"control", ctl}} {
		if boom := s.srv.Grep(`Game Error|ERROR:`); len(boom) > 0 {
			check("boot/"+s.name+" no game error", false, strings.TrimSpace(boom[0]))
			report()
			return
		}
		check("boot/"+s.name+" no game error", true, "")
	}
	// The log has to EXIST before anything is asserted about its contents:
	// statsfile off, or a path g_fs.c refused, and every row below would fail
	// for a reason that has nothing to do with the three call sites.
	if _, err := os.Stat(statsPath(logDir)); err != nil {
		check("boot/stats log was opened", false, "%v -- is statsfile on?", err)
		report()
		return
	}
	check("boot/stats log was opened", true, statsPath(logDir))

	// ------------------------------------------------------------------ chat
	//
	// Before the match, deliberately: `say` needs no match and this proves the
	// call site is not accidentally behind one.
	fmt.Printf("\n##### `say` reaches the log, and stats_logchat still gates it\n")

	const token = "sweep-r1953-marker"

	a := playtest.NewBot("chatter", "127.0.0.1", *port)
	if err := a.Start(30 * time.Second); err != nil {
		fmt.Fprintln(os.Stderr, "bot a:", err)
		os.Exit(2)
	}
	defer a.Disconnect()
	c := playtest.NewBot("chatter", "127.0.0.1", *ctlport)
	if err := c.Start(30 * time.Second); err != nil {
		fmt.Fprintln(os.Stderr, "control bot:", err)
		os.Exit(2)
	}
	defer c.Disconnect()
	a.WaitFrames(10, 5*time.Second)
	c.WaitFrames(10, 5*time.Second)

	a.Cmd("say %s", token)
	c.Cmd("say %s", token)
	time.Sleep(1500 * time.Millisecond)

	evs, err := readLog(statsPath(logDir))
	if err != nil {
		check("chat/the log parses", false, "%v", err)
		report()
		return
	}
	chats := of(evs, "chat")
	var carried int
	for _, e := range chats {
		if strings.Contains(e.Text, token) {
			carried++
		}
	}
	// POSITIVE.  Exactly one, not "at least one": Cmd_Say_f is reached by
	// several commands and a second call site would double every line.
	check("chat/one chat event carries the say", carried == 1,
		"%d chat event(s), %d carrying %q", len(chats), carried, token)
	if len(chats) > 0 {
		check("chat/it is the prefixed line the player saw",
			strings.Contains(chats[len(chats)-1].Text, a.Name+":"),
			"%q", chats[len(chats)-1].Text)
	} else {
		skip("chat/it is the prefixed line the player saw", "no chat event to look at")
	}

	// NEGATIVE.  The same say on a server with the cvar off.
	ctlEvs, err := readLog(statsPath(ctlDir))
	if err != nil && !os.IsNotExist(err) {
		check("chat/control log parses", false, "%v", err)
	}
	ctlChats := of(ctlEvs, "chat")
	check("chat/stats_logchat 0 logs nothing", len(ctlChats) == 0,
		"%d chat event(s) on the control server", len(ctlChats))
	// ...and the control server is logging at all, or the row above is vacuous:
	// a control whose stats file was never opened would pass it for free.
	check("chat/control is otherwise logging", len(ctlEvs) > 0,
		"%d event(s) of other kinds", len(ctlEvs))

	// ----------------------------------------------------------------- match
	fmt.Printf("\n##### a TeamPlay match, so that anything is pickable at all\n")
	b := playtest.NewBot("sparring", "127.0.0.1", *port)
	if err := b.Start(30 * time.Second); err != nil {
		fmt.Fprintln(os.Stderr, "bot b:", err)
		os.Exit(2)
	}
	defer b.Disconnect()
	b.WaitFrames(10, 5*time.Second)

	joinedA, joinedB := joinTeam(a, teamA), joinTeam(b, teamB)
	check("match/both joined a team", joinedA && joinedB,
		"%q | %q", lastTeamLine(a), lastTeamLine(b))
	a.Cmd("ready")
	b.Cmd("ready")

	live := false
	deadline := time.Now().Add(time.Duration(matchCountdown+20) * time.Second)
	for time.Now().Before(deadline) && !live {
		a.Cmd("give Body Armor")
		time.Sleep(800 * time.Millisecond)
		live = a.Stat(statArmor) > 0
	}
	check("match/client is alive", a.Stat(statHealth) > 0,
		"stats[%d]=%d (STAT_HEALTH)", statHealth, a.Stat(statHealth))
	check("match/items are pickable (sync_stat >= 4)", live,
		"stats[%d]=%d after ready + up to %ds", statArmor, a.Stat(statArmor),
		matchCountdown+20)

	if !live {
		for _, n := range []string{
			"drop/no quad, no item_drop", "drop/dying with a quad logs item_drop",
			"drop/it closes the pickup that set osp_r200",
		} {
			skip(n, "no match, so nothing is pickable")
		}
		report()
		return
	}

	// ------------------------------------------------------------------ drop
	fmt.Printf("\n##### a quad DROPPED on death is an item_drop, at the death\n")

	// NEGATIVE FIRST, and on purpose: die with no quad and establish that the
	// death itself does not produce an item_drop.  Run after the positive it
	// would be indistinguishable from a stale event.
	bareDeath := colosseum.KillAndConfirm(b, func() bool { return b.Stat(statHealth) > 0 })
	// THE ROW THAT KEEPS THE NEXT ONE HONEST.  Cmd_Kill_f refuses a suicide
	// within five seconds of a respawn and says nothing, and the match start
	// respawns everybody -- so this `kill` arrives inside the refusal window
	// more often than not.  Without confirming the death, "no item_drop after a
	// bare death" is satisfied by a death that never happened.
	check("drop/the bare death actually happened", bareDeath,
		"the server confirmed a death for %s", b.Name)
	evs, _ = readLog(statsPath(logDir))
	before := len(quadDrops(evs))
	if !bareDeath {
		skip("drop/no quad, no item_drop", "the suicide was refused")
	} else {
		check("drop/no quad, no item_drop", before == 0,
			"%d item_drop \"Quad\" event(s) after a bare death", before)
	}

	// POSITIVE.  give, then USE -- the pickup only puts it in the inventory,
	// and TossClientWeapon drops it only while quad_framenum is more than ten
	// frames ahead, i.e. while it is actually running.
	granted := false
	for try := 0; try < 6 && !granted; try++ {
		if a.Stat(statHealth) <= 0 {
			time.Sleep(1 * time.Second)
			continue
		}
		a.Cmd("give Quad Damage")
		time.Sleep(300 * time.Millisecond)
		a.Cmd("use Quad Damage")
		if _, err := a.WaitStat(statTimerIcon, -1, 2*time.Second); err == nil ||
			a.Stat(statTimerIcon) != 0 {
			granted = a.Stat(statTimerIcon) != 0
		}
	}
	// Reported on its own, because "the quad never started" and "the drop was
	// not logged" are different observations and only the second is a finding.
	if !check("drop/the quad is running", granted,
		"stats[%d]=%d (STAT_TIMER_ICON)", statTimerIcon, a.Stat(statTimerIcon)) {
		skip("drop/dying with a quad logs item_drop", "no quad was ever active")
		skip("drop/it closes the pickup that set osp_r200", "no quad was ever active")
	} else {
		if !colosseum.KillAndConfirm(a, func() bool { return a.Stat(statHealth) > 0 }) {
			skip("drop/dying with a quad logs item_drop", "the suicide was refused")
			skip("drop/it closes the pickup that set osp_r200", "the suicide was refused")
			report()
			return
		}
		evs, _ = readLog(statsPath(logDir))
		drops := quadDrops(evs)
		check("drop/dying with a quad logs item_drop", len(drops) == before+1,
			"%d item_drop \"Quad\" event(s), was %d", len(drops), before)
		if len(drops) > 0 {
			d := drops[len(drops)-1]
			// THE ROW THAT PROVES resp.osp_r200 WAS STILL SET.  `from` is that
			// field, and the item_pickup event that set it named the same
			// edict -- so this joins the two halves of one quad's record.  A
			// zero here is the pre-fix state arriving by another route: the
			// field cleared, or never read, and the drop unattributable.
			var pickedFrom int
			for _, e := range of(evs, "item_pickup") {
				if e.Item == "Quad" {
					pickedFrom = e.Entity
				}
			}
			check("drop/it closes the pickup that set osp_r200",
				pickedFrom != 0 && d.From == pickedFrom,
				"from=%d, item_pickup entity=%d, dropped edict=%d",
				d.From, pickedFrom, d.Entity)
		} else {
			skip("drop/it closes the pickup that set osp_r200", "no item_drop to look at")
		}
	}

	// -------------------------------------------------------------- shutdown
	//
	// A THIRD SERVER, and it has to be one: this row needs a client with a
	// non-empty accuracy table, and only a bot can produce one.  It runs `dm`
	// -- also one of the OSP four, so G_IsOspRuleset() is true and the same
	// ShutdownGame arm runs -- because RULESET_DM sets sync_stat = 8 and the
	// match is live from the first frame, with no readying up and no
	// countdown.  Under `tdm` the bots would have to join teams and ready.
	fmt.Printf("\n##### `map` mid-match writes the accuracy table first\n")
	if *glad == "" {
		for _, n := range []string{
			"shutdown/the game logged its end", "shutdown/bots produced an accuracy table",
			"shutdown/accuracy is written", "shutdown/it comes before the shutdown event",
			"shutdown/it is not the match-end dump",
		} {
			skip(n, "no -gladdir, so nothing on this harness can fire a weapon")
		}
		report()
		return
	}
	accDir := *dir + "/acc"
	if err := colosseum.Install(accDir, *ref, *ctfref, *lib); err != nil {
		fmt.Fprintln(os.Stderr, "acc install:", err)
		os.Exit(2)
	}
	if err := colosseum.InstallBrain(accDir, *glad); err != nil {
		for _, n := range []string{
			"shutdown/the game logged its end", "shutdown/bots produced an accuracy table",
			"shutdown/accuracy is written", "shutdown/it comes before the shutdown event",
			"shutdown/it is not the match-end dump",
		} {
			skip(n, fmt.Sprintf("no brain: %v", err))
		}
		report()
		return
	}
	os.Remove(statsPath(accDir))

	acc := &playtest.Server{
		Binary: *q2proded, Dir: accDir, Game: "colosseum",
		Map: "q2dm1", Port: *accport, MaxClients: 12,
		Cvars: map[string]string{
			"g_ruleset": "dm", "cheats": "1", "skill": "3",
			"deathmatch": "1", "coop": "0",
			"minimumplayers": "0", "bots_minplayers": "0",
			"statsfile": "1", "stats_logchat": "0",
			"sv_allow_map": "1",
		},
		LogPath: accDir + "/server.log",
	}
	if err := acc.Start(); err != nil {
		fmt.Fprintln(os.Stderr, "accuracy server:", err)
		os.Exit(2)
	}
	servers = append(servers, acc)
	time.Sleep(3 * time.Second)
	for i := 0; i < 4; i++ {
		acc.Console("sv addrandom")
		time.Sleep(400 * time.Millisecond)
	}
	// Let them fight.  Polled on the obituaries rather than slept blind, and
	// then given a further stretch, because the table needs a HIT and not just
	// a shot -- OSP_Stats_Accuracy's `any` test walks shots/taken/given.
	acc.WaitLog(`(?i)(killed|blasted|railed|was |tried)`, 60*time.Second)
	time.Sleep(15 * time.Second)

	accEvs, err := readLog(statsPath(accDir))
	if err != nil {
		check("shutdown/the accuracy server's log parses", false, "%v", err)
		report()
		return
	}
	startLine := lastLineOf(accEvs, "match_start")
	endBefore := lastLineOf(accEvs, "match_end")
	accBefore := len(of(accEvs, "accuracy"))
	killsBefore := len(of(accEvs, "kill"))

	// THE PRECONDITION, and the row the previous version of this scenario was
	// missing: without a kill nothing has an accuracy table, and "no accuracy
	// event" would then be a fact about the bots rather than about the code.
	check("shutdown/bots produced an accuracy table", killsBefore > 0,
		"%d kill event(s) before the map change", killsBefore)

	acc.Console("map q2dm1")
	time.Sleep(8 * time.Second)

	accEvs, err = readLog(statsPath(accDir))
	if err != nil {
		check("shutdown/the accuracy server's log parses", false, "%v", err)
		report()
		return
	}
	shut := of(accEvs, "shutdown")
	if len(shut) == 0 {
		check("shutdown/the game logged its end", false,
			"no shutdown event -- did `map` reach the server?")
		for _, n := range []string{
			"shutdown/accuracy is written", "shutdown/it comes before the shutdown event",
			"shutdown/it is not the match-end dump",
		} {
			skip(n, "no shutdown event to order against")
		}
		report()
		return
	}
	sd := shut[len(shut)-1]
	check("shutdown/the game logged its end", sd.Reason == "map",
		"reason=%q at line %d", sd.Reason, sd.Line)

	accs := of(accEvs, "accuracy")
	check("shutdown/accuracy is written", len(accs) > accBefore,
		"%d accuracy event(s), was %d", len(accs), accBefore)

	var inWindow []event
	for _, e := range accs {
		if e.Line > startLine && e.Line < sd.Line {
			inWindow = append(inWindow, e)
		}
	}
	check("shutdown/it comes before the shutdown event", len(inWindow) > 0,
		"%d accuracy event(s) between line %d and shutdown (line %d)",
		len(inWindow), startLine, sd.Line)

	// THE ROW THAT SEPARATES THIS CALL FROM THE SIX THAT WERE ALREADY THERE.
	// All six fire at match end.  If a match_end landed in the window the
	// accuracy events in it are theirs and this scenario has proved nothing --
	// so that is a skip with the reason, not a pass.
	endAfter := lastLineOf(accEvs, "match_end")
	if endAfter != endBefore && endAfter > startLine && endAfter < sd.Line {
		skip("shutdown/it is not the match-end dump",
			fmt.Sprintf("a match_end landed at line %d inside the window", endAfter))
	} else {
		check("shutdown/it is not the match-end dump", len(inWindow) > 0,
			"no match_end between line %d and %d", startLine, sd.Line)
	}

	report()
}

// quadDrops is every item_drop naming the quad.
func quadDrops(evs []event) []event {
	var out []event
	for _, e := range of(evs, "item_drop") {
		if e.Item == "Quad" {
			out = append(out, e)
		}
	}
	return out
}

func joinTeam(b *playtest.Bot, name string) bool {
	for try := 0; try < 5; try++ {
		b.Cmd("team %s", name)
		time.Sleep(600 * time.Millisecond)
		if onTeam(b, name) {
			return true
		}
	}
	return false
}

func onTeam(b *playtest.Bot, name string) bool {
	before := len(b.Prints())
	b.Cmd("team")
	deadline := time.Now().Add(2 * time.Second)
	for time.Now().Before(deadline) {
		p := b.Prints()
		for i := len(p) - 1; i >= before; i-- {
			if strings.Contains(p[i], `team "`+name+`"`) {
				return true
			}
		}
		time.Sleep(20 * time.Millisecond)
	}
	return false
}

func lastTeamLine(b *playtest.Bot) string {
	p := b.Prints()
	for i := len(p) - 1; i >= 0; i-- {
		if strings.Contains(p[i], "team") {
			return strings.TrimSpace(p[i])
		}
	}
	return "<nothing about a team>"
}

func report() {
	stopAll()
	fmt.Printf("\n%d check(s), %d failed, %d skipped\n", total, failed, skipped)
	if failed > 0 {
		fmt.Println("  server log:  ", *dir+"/log/server.log")
		fmt.Println("  stats log:   ", statsPath(*dir+"/log"))
		fmt.Println("  control log: ", statsPath(*dir+"/ctl"))
		os.Exit(1)
	}
	if !*keep {
		os.Remove(*dir + "/log/server.log")
		os.Remove(*dir + "/ctl/server.log")
	}
}
