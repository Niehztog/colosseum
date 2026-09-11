// ospscore -- three claims about Colosseum's OSP Tourney integration that only
// the running game can settle.
//
//  1. resp.clientid COLLAPSES TO 0 for every client.  ClientBegin calls
//     OSP_clientBeginLevel() -- which calls OSP_giveClientID() -- and then
//     ClientBeginDeathmatch(), whose first statements are G_InitEdict() and an
//     UNCONDITIONAL InitClientResp().  clientid lives in client_respawn_t, so
//     the memset throws the id away one call later.  The donor guards that
//     InitClientResp with `if (!resp.osp_r210)` and gives the id from inside
//     ClientBegin's own recovered/not-recovered arms.
//     Read off the stats log, which stamps "id" into every event.
//
//  2. THE TEAM FRAG TOTALS ARE NEVER INCREMENTED.  osp_team_t.osp_m0f8 and its
//     four siblings are written in exactly two places in this tree, both of
//     which zero them.  The donor's ClientObituary is what raises them, and the
//     merged obituary replaced that with OSP_scoreChange(), which touches
//     resp.score alone.  Observable through Score_A/Score_B, the two serverinfo
//     cells OSP_updateTeamFrags publishes: a suicide moves the player's own
//     STAT_FRAGS and must move the team total with it.
//
//  3. `cmd _init_state` IS STUFFED EVERY FRAME.  The donor calls
//     OSP_speedDetect() from ClientBeginServerFrame behind four conditions, the
//     first of which is `resp.osp_r2b4 == level.framenum` -- a self-scheduling
//     ~20-second timer.  This tree calls it under `G_IsOspRuleset()` alone, so
//     every client is stuffed a console command ten times a second forever.
//     Count what arrives on the wire.
package main

import (
	"flag"
	"fmt"
	"os"
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
	lib      = flag.String("lib", "", "game library under test")
	dir      = flag.String("dir", "/tmp/ospscore", "scratch install dir")
	port     = flag.Int("port", 27994, "UDP port")
	mapname  = flag.String("map", "q2dm1", "map to run")
	keep     = flag.Bool("keep", false, "keep the server log on success")
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
	fmt.Printf("  %s %-46s %s\n", tag, name, note)
	return ok
}

func skip(name, why string) {
	total++
	skipped++
	fmt.Printf("  [skip] %-46s %s\n", name, why)
}

const (
	statHealth = 1
	statArmor  = 5
	statFrags  = 14 // shared.h STAT_FRAGS -- a universal slot, 0..15

	teamA, teamB   = "Hometeam", "Visitors"
	matchCountdown = 14
	dfForceRespawn = 1 << 10

	refPassword = "letmein"
)

func main() {
	flag.Parse()
	if *q2proded == "" || *lib == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded and -lib")
		os.Exit(2)
	}
	if err := colosseum.Install(*dir, *ref, *ctfref, *lib); err != nil {
		fmt.Fprintln(os.Stderr, "install:", err)
		os.Exit(2)
	}
	statsPath := *dir + "/colosseum/ospscore.jsonl"
	os.Remove(statsPath)

	srv := &playtest.Server{
		Binary: *q2proded, Dir: *dir, Game: "colosseum",
		Map: *mapname, Port: *port, MaxClients: 8,
		Cvars: map[string]string{
			"g_ruleset":        "tdm",
			"cheats":           "1",
			"deathmatch":       "1",
			"coop":             "0",
			"match_countdown":  fmt.Sprint(matchCountdown),
			"bots":             "0",
			"flood_msgs":       "0",
			"statsfile":        "1",
			"statsname":        "ospscore.jsonl",
			"dmflags":          fmt.Sprint(dfForceRespawn),
			"referee_enable":   "1",
			"referee_password": refPassword,
		},
		LogPath: *dir + "/server.log",
	}
	if err := srv.Start(); err != nil {
		fmt.Fprintln(os.Stderr, "server:", err)
		os.Exit(2)
	}
	defer srv.Stop()
	time.Sleep(3 * time.Second)

	fmt.Printf("\n##### the server survived loading the map\n")
	if boom := srv.Grep(`Game Error|ERROR:`); len(boom) > 0 {
		check("boot/no game error", false, strings.TrimSpace(boom[0]))
		report()
		return
	}
	check("boot/no game error", true, "")

	// ------------------------------------------------------------ three clients
	fmt.Printf("\n##### three clients connect and enter the game\n")
	bots := []*playtest.Bot{}
	for i, n := range []string{"alpha", "bravo", "charlie"} {
		b := playtest.NewBot(n, "127.0.0.1", *port)
		if err := b.Start(30 * time.Second); err != nil {
			fmt.Fprintf(os.Stderr, "bot %d: %v\n", i, err)
			os.Exit(2)
		}
		defer b.Disconnect()
		bots = append(bots, b)
	}
	bots[0].WaitFrames(10, 5*time.Second)

	// Entering is what emits the stats "connect"/"enter" records that carry the
	// id.  Two on one team and one on the other, so the match below can start.
	joined := 0
	for i, b := range bots {
		name := teamA
		if i == 2 {
			name = teamB
		}
		if joinTeam(b, name) {
			joined++
		}
	}
	check("setup/all three entered the game", joined == 3, "%d of 3", joined)

	// ------------------------------------------------------------ 1. clientid
	fmt.Printf("\n##### 1. does every client get its OWN resp.clientid?\n")
	time.Sleep(1500 * time.Millisecond)
	ids, names := statsIDs(statsPath)
	if len(ids) == 0 {
		skip("clientid/distinct per client", "no stats records at "+statsPath)
	} else {
		distinct := map[int]bool{}
		for _, v := range ids {
			distinct[v] = true
		}
		check("clientid/distinct per client", len(distinct) >= 3,
			"%d record(s) over %d client name(s), ids seen: %s",
			len(ids), len(names), keysOf(distinct))
		allZero := len(distinct) == 1 && distinct[0]
		note := "ids differ"
		if allZero {
			note = "every stats record says \"id\":0"
		}
		check("clientid/not all zero", !allZero, "%s", note)
	}

	// ------------------------------------------------------------ 4. the menu
	//
	// The donor's ENTRY POINT into every OSP menu is Cmd_Inven_f, which it
	// rewrites to `OSP_teamMenu(ent)` / `OSP_DMMenu(ent)`.  osp_clientcmd.c
	// still routes `menu`, `ctfmenu` and `inven` there, but the merged
	// Cmd_Inven_f has arms for ctf and arena and none for tourney, so the
	// command falls through to baseq2's inventory.
	//
	// osp_PMenu_Update draws with svc_layout and its first token is
	// "picn inventory", which is what a menu on the wire looks like; the
	// vanilla inventory is svc_inventory and produces no layout at all.
	fmt.Printf("\n##### 4. does `menu` open the tourney menu?\n")
	m := bots[1]
	m.Cmd("menu")
	time.Sleep(1200 * time.Millisecond)
	lay := m.Layout()
	check("menu/`menu` draws the tourney menu",
		strings.Contains(lay, "picn inventory"),
		"layout=%q", trunc(lay, 60))

	// ------------------------------------------------------- 5. the menu toggle
	//
	// osp_PMenu_Close() does not clear gclient_t.menu_owner; only G_MenuClose()
	// does, and osp_menus.c calls the former directly in 38 places.  ctf's
	// counterpart clears it in both paths and says why.  The visible consequence
	// is a menu that opens once: every OSP_*Menu() entry point is written
	// `if (menu_owner == MENU_TOURNEY) close; else open`, so a stale owner turns
	// the next press into a second close.
	fmt.Printf("\n##### 5. does the referee menu open, close and open again?\n")
	sample := func(cmd string) bool {
		m.Cmd("%s", cmd)
		time.Sleep(1400 * time.Millisecond)
		return strings.Contains(m.Layout(), "picn inventory")
	}
	// Check 4 may have left a menu up.  Start from a known state: `referee
	// <password>` grants the status and then TOGGLES the admin menu, so the
	// first sample's meaning depends on what was on screen before it.
	if strings.Contains(m.Layout(), "picn inventory") {
		m.Cmd("menu")
		time.Sleep(1200 * time.Millisecond)
	}
	up1 := sample("referee " + refPassword)
	check("menu/referee menu opens", up1, "layout=%q", trunc(m.Layout(), 60))
	down := !sample("referee")
	check("menu/...and closes", down, "layout=%q", trunc(m.Layout(), 60))
	if !up1 || !down {
		skip("menu/...and opens again", "the first open/close did not work")
	} else {
		up2 := sample("referee")
		check("menu/...and opens again", up2, "layout=%q", trunc(m.Layout(), 60))
	}

	// ------------------------------------------------------- 3. stufftext storm
	//
	// Counted BEFORE the match starts, because the storm is not conditional on
	// one: OSP_speedDetect is reached from ClientBeginServerFrame on every frame
	// of every client under an OSP ruleset.
	fmt.Printf("\n##### 3. how often is `cmd _init_state` stuffed at one client?\n")
	before := countStuff(bots[0], "_init_state")
	time.Sleep(3 * time.Second)
	after := countStuff(bots[0], "_init_state")
	got := after - before
	// The donor re-arms itself with `level.framenum + rand(0..30) + 200`, i.e.
	// once every 20 to 23 seconds, so three seconds may legitimately carry one.
	// Ten a second is 30.
	check("speeddetect/is rate-limited", got <= 2,
		"%d `cmd _init_state` stufftexts in 3s (donor: at most 1 per ~20s)", got)

	// ---------------------------------------------------------- 2. team totals
	fmt.Printf("\n##### 2. does a frag change move the TEAM total?\n")
	for _, b := range bots {
		b.Cmd("ready")
	}
	live := false
	deadline := time.Now().Add(time.Duration(matchCountdown+20) * time.Second)
	for time.Now().Before(deadline) && !live {
		bots[0].Cmd("give Body Armor")
		time.Sleep(800 * time.Millisecond)
		live = bots[0].Stat(statArmor) > 0
	}
	check("match/is running (items pickable)", live,
		"stats[%d]=%d", statArmor, bots[0].Stat(statArmor))
	if !live {
		skip("teamfrags/team total follows the player score", "no match")
		report()
		return
	}

	fmt.Printf("       Score_A/Score_B before: %q / %q\n",
		serverinfo(srv, "Score_A"), serverinfo(srv, "Score_B"))
	scoreBefore := bots[0].Stat(statFrags)
	aBefore, bBefore := serverinfo(srv, "Score_A"), serverinfo(srv, "Score_B")

	// Two suicides.  Each one is `resp.score--` plus, in the donor,
	// osp_teams[team].osp_m0f8-- and osp_m108++.
	// Cmd_Kill_f refuses within 5 * BASE_FRAMERATE of the last respawn, so the
	// wait is longer than the guard rather than long enough to look patient.
	for i := 0; i < 2; i++ {
		bots[0].Cmd("kill")
		time.Sleep(6500 * time.Millisecond)
	}
	time.Sleep(1500 * time.Millisecond)

	scoreAfter := bots[0].Stat(statFrags)
	aAfter, bAfter := serverinfo(srv, "Score_A"), serverinfo(srv, "Score_B")

	moved := check("teamfrags/the player's own score moved",
		scoreAfter < scoreBefore, "STAT_FRAGS %d -> %d", scoreBefore, scoreAfter)
	if !moved {
		skip("teamfrags/team total followed it",
			"the player's own score did not move either, so nothing is proven")
	} else {
		check("teamfrags/team total followed it",
			aAfter != aBefore || bAfter != bBefore,
			"Score_A %q -> %q, Score_B %q -> %q", aBefore, aAfter, bBefore, bAfter)
	}

	report()
}

func report() {
	fmt.Printf("\n%d check(s), %d failed, %d skipped\n", total, failed, skipped)
	if failed > 0 {
		fmt.Println("  server log:", *dir+"/server.log")
		os.Exit(1)
	}
	if !*keep {
		os.Remove(*dir + "/server.log")
	}
}

func trunc(s string, n int) string {
	s = strings.ReplaceAll(s, "\n", " ")
	if len(s) > n {
		return s[:n] + "..."
	}
	return s
}

func keysOf(m map[int]bool) string {
	out := []string{}
	for k := range m {
		out = append(out, strconv.Itoa(k))
	}
	return strings.Join(out, ",")
}

func countStuff(b *playtest.Bot, sub string) int {
	n := 0
	for _, s := range b.Stuffs() {
		if strings.Contains(s, sub) {
			n++
		}
	}
	return n
}

var idRe = regexp.MustCompile(`"id":(-?\d+)`)
var nameRe = regexp.MustCompile(`"name":"([^"]*)"`)

// statsIDs reads every "id" the stats log stamped on a player record, and the
// set of names those records carry.  Three named clients producing three
// records whose ids are all 0 is the finding.
func statsIDs(path string) ([]int, map[string]bool) {
	raw, err := os.ReadFile(path)
	if err != nil {
		return nil, nil
	}
	ids := []int{}
	names := map[string]bool{}
	for _, line := range strings.Split(string(raw), "\n") {
		m := idRe.FindStringSubmatch(line)
		nm := nameRe.FindStringSubmatch(line)
		if m == nil || nm == nil {
			continue
		}
		if nm[1] == "" {
			continue
		}
		v, _ := strconv.Atoi(m[1])
		ids = append(ids, v)
		names[nm[1]] = true
	}
	return ids, names
}

var cvarRe = regexp.MustCompile(`"([^"]+)" is "([^"]*)"`)

// serverinfo asks the console for one cvar and returns its value.  Quake II's
// console prints `"name" is "value"` for a bare cvar name.
func serverinfo(srv *playtest.Server, name string) string {
	before := len(srv.Log())
	srv.Console(name)
	deadline := time.Now().Add(3 * time.Second)
	for time.Now().Before(deadline) {
		lines := srv.Log()
		for i := len(lines) - 1; i >= before && i >= 0; i-- {
			m := cvarRe.FindStringSubmatch(lines[i])
			if m != nil && m[1] == name {
				return m[2]
			}
		}
		time.Sleep(20 * time.Millisecond)
	}
	return "<no answer>"
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
	deadline := time.Now().Add(3 * time.Second)
	for time.Now().Before(deadline) {
		p := b.Prints()
		for i := len(p) - 1; i >= before; i-- {
			if strings.Contains(p[i], name) {
				return true
			}
		}
		time.Sleep(20 * time.Millisecond)
	}
	return false
}
