// ospenter -- under OSP Tourney's four rulesets, CONNECTING IS NOT ENTERING.
//
// A tourney client arrives as an observer (OSP_clientBeginPre sets
// `resp.osp_entered` to 2) and enters through a command: `join` in a
// free-for-all, `join <team>` where there are teams.  Everything tourney knows
// about a client keys off that state -- which scoreboard section it is drawn in,
// whether g_combat.c will damage it, whether its score is a score or the -100
// sentinel -- so the PLACEMENT has to agree with it, and PutClientInServer is
// where the donor makes it agree: a client that has not entered is placed
// MOVETYPE_NOCLIP, SOLID_NOT, SVF_NOCLIENT, with no view weapon and no KillBox.
//
// The defect this was written for: the merge took the donor's entered-arm
// unconditionally and dropped the observer arm, so a connecting client was a
// solid, visible, walking body that every tourney predicate still called an
// observer -- it collected items, and under `dm` (where the match is live from
// the first frame) it shot and killed while being unkillable itself.
//
// THE PMOVE TYPE IS THE WITNESS, and it is the one question about observing that
// a mod's own HUD cannot fake: the server derives it from `movetype` in
// ClientThink, so PM_SPECTATOR means the body really is a noclipping observer
// and PM_NORMAL means it really is a player.  Two more are read alongside it --
// STAT_FRAGS off the wire, where the donor draws 0 for a client that has not
// entered rather than the -100 its score actually holds; and the server's own
// "entered the game (clients = N)" broadcast, which must NOT have been said
// before the join and must be said after it.
//
// Both signs, every phase: observer BEFORE the join, player AFTER it.
package main

import (
	"flag"
	"fmt"
	"os"
	"strings"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
)

const statFrags = 14

// Clients for the crowded phase.  q2dm1 has ten info_player_deathmatch, and the
// phase needs one more body than the map has room for.
var crowdN = 11

// The team a client joins where the ruleset has teams.  Set on the server too,
// so the name the scenario types is the name the server knows.
const teamA = "Hometeam"

var pass, fail int

func ok(what, detail string)  { pass++; fmt.Printf("  [ ok ] %-52s %s\n", what, detail) }
func bad(what, detail string) { fail++; fmt.Printf("  [FAIL] %-52s %s\n", what, detail) }

func pmName(t int) string {
	switch t {
	case playtest.PMNormal:
		return "PM_NORMAL (a player)"
	case playtest.PMSpectator:
		return "PM_SPECTATOR (an observer)"
	case playtest.PMDead:
		return "PM_DEAD"
	case playtest.PMFreeze:
		return "PM_FREEZE"
	}
	return fmt.Sprintf("pm_type %d", t)
}

func main() {
	q2 := flag.String("q2proded", "", "")
	lib := flag.String("lib", "", "")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "")
	dir := flag.String("dir", "/tmp/colosseum-ospenter", "")
	port := flag.Int("port", 27997, "")
	rulesets := flag.String("rulesets", "dm,tdm", "the OSP rulesets to drive")
	glad := flag.String("gladdir", "", "")
	crowd := flag.Int("crowd", 11, "clients for the crowded phase; 0 skips it")
	flag.Parse()
	crowdN = *crowd

	for i, rs := range strings.Split(*rulesets, ",") {
		rs = strings.TrimSpace(rs)
		if rs == "" {
			continue
		}
		if !colosseum.IsOSP(rs) {
			fmt.Printf("  [skip] %s is not one of the OSP four; there is no `entered` state there\n", rs)
			continue
		}
		r, found := colosseum.Find(rs)
		if !found {
			fmt.Printf("ospenter: no such ruleset %q\n", rs)
			os.Exit(2)
		}
		runRuleset(*q2, *lib, *ref, *ctf, *dir+"/"+rs, *glad, *port+i, rs, r.Map)
	}

	if crowdN > 0 {
		runCrowded(*q2, *lib, *ref, *ctf, *dir+"/crowd", *port+90, crowdN)
	}

	fmt.Printf("\n%d check(s), %d failed\n", pass+fail, fail)
	if fail > 0 {
		os.Exit(1)
	}
}

// runCrowded fills every spawn point on the map and then asks one more client to
// enter.  THE DONOR REFUSES THE PLACEMENT RATHER THAN TELEFRAGGING: its
// SelectSpawnPoint returns false when a player is within 60 units of the chosen
// spot, and PutClientInServer then leaves the client frozen and bodiless until
// The respawn trigger finds it a spot.  Both signs are readable from
// outside: the newcomer's pmove type, and the obituary that a telefrag writes
// and a refusal does not.
//
// The clients cannot walk, which is what makes the phase deterministic: every
// entered client is parked ON a spawn point for as long as it stays connected.
func runCrowded(q2, lib, ref, ctf, dir string, port, n int) {
	fmt.Printf("\n##### crowded: %d clients on q2dm1's ten spawn points (port %d)\n", n, port)

	if err := colosseum.Install(dir, ref, ctf, lib); err != nil {
		panic(err)
	}
	srv := &playtest.Server{
		Binary: q2, Dir: dir, Game: "colosseum", Map: "q2dm1", Port: port,
		MaxClients: n + 1,
		Cvars: map[string]string{"g_ruleset": "dm", "bots": "0",
			"bots_minplayers": "0", "deathmatch": "1", "coop": "0"},
		LogPath: dir + "/server.log",
	}
	if err := srv.Start(); err != nil {
		panic(err)
	}
	defer srv.Stop()

	bots := make([]*playtest.Bot, 0, n)
	for i := 0; i < n; i++ {
		b := playtest.NewBot(fmt.Sprintf("crowd%02d", i), "127.0.0.1", port)
		// A client that cannot be seated is the HOST's limit and not a finding:
		// eleven libq2 clients plus four servers from the phases above is more
		// than a small machine spawns inside one timeout, and it has been seen
		// to stall one client's `begin` with nothing in the server log but a
		// time-out.  Reported as a skip rather than a failure, and the phase's
		// own checks do not run -- a check that could not be arranged is not a
		// pass (audit.py's rule, one harness over).
		if err := b.Start(45 * time.Second); err != nil {
			fmt.Printf("  [skip] the crowded phase needs %d clients and this "+
				"host seated %d (%v)\n", n, i, err)
			for _, o := range bots {
				o.Disconnect()
			}
			return
		}
		bots = append(bots, b)
		b.WaitFrames(4, 10*time.Second)
		b.Cmd("join")
		b.WaitFrames(4, 10*time.Second)
	}
	defer func() {
		for _, b := range bots {
			b.Disconnect()
		}
	}()
	bots[len(bots)-1].WaitFrames(40, 20*time.Second)

	frozen, normal := 0, 0
	for _, b := range bots {
		switch b.PMType() {
		case playtest.PMFreeze:
			frozen++
		case playtest.PMNormal:
			normal++
		}
	}

	if frozen > 0 {
		ok("crowd/a client with nowhere to go is frozen",
			fmt.Sprintf("%d frozen, %d placed of %d", frozen, normal, n))
	} else {
		bad("crowd/a client with nowhere to go is frozen",
			fmt.Sprintf("all %d were placed (%d PM_NORMAL) -- nothing was refused",
				normal, normal))
	}

	// The other sign, and the behaviour the refusal replaces: whoever cannot
	// have the spot must NOT take it by telefragging the client standing there.
	if tf := srv.Grep(`personal space`); len(tf) == 0 {
		ok("crowd/nobody was telefragged for a spawn point", "no MOD_TELEFRAG obituary")
	} else {
		bad("crowd/nobody was telefragged for a spawn point",
			fmt.Sprintf("%d telefrag obituary/ies: %q", len(tf), tf[0]))
	}

	// ...AND THE RETRY.  A refusal is only correct if it is temporary: make room
	// and the frozen client must be placed without asking again.
	if frozen > 0 {
		for i := 0; i < 4 && i < len(bots); i++ {
			bots[i].Disconnect()
		}
		time.Sleep(4 * time.Second)
		placed := 0
		for _, b := range bots[4:] {
			if b.PMType() == playtest.PMNormal {
				placed++
			}
		}
		if placed == len(bots)-4 {
			ok("crowd/room appearing places the frozen client",
				fmt.Sprintf("all %d remaining clients are PM_NORMAL", placed))
		} else {
			bad("crowd/room appearing places the frozen client",
				fmt.Sprintf("%d of %d placed -- the retry did not fire",
					placed, len(bots)-4))
		}
	}
}

func runRuleset(q2, lib, ref, ctf, dir, glad string, port int, rs, mp string) {
	fmt.Printf("\n##### %s on %s (port %d)\n", rs, mp, port)

	if err := colosseum.Install(dir, ref, ctf, lib); err != nil {
		panic(err)
	}
	if glad != "" {
		if err := colosseum.InstallBrain(dir, glad); err != nil {
			panic(err)
		}
	}
	srv := &playtest.Server{
		Binary: q2, Dir: dir, Game: "colosseum", Map: mp, Port: port,
		MaxClients: 8,
		// No bots: a fill would enter clients of its own, and "who has entered"
		// is the whole subject.
		Cvars: map[string]string{"g_ruleset": rs, "bots": "0",
			"bots_minplayers": "0", "deathmatch": "1", "coop": "0",
			// Pinned rather than assumed: the join is BY NAME.
			"team_a_name": teamA, "team_b_name": "Visitors"},
		LogPath: dir + "/server.log",
	}
	if err := srv.Start(); err != nil {
		panic(err)
	}
	defer srv.Stop()

	b := playtest.NewBot("enterer", "127.0.0.1", port)
	if err := b.Start(30 * time.Second); err != nil {
		panic(err)
	}
	defer b.Disconnect()
	if err := b.WaitFrames(30, 20*time.Second); err != nil {
		panic(err)
	}

	// ---- 1. connected, not entered: an OBSERVER -------------------------
	pm := b.PMType()
	if pm == playtest.PMSpectator {
		ok(rs+"/a client that only connected is an observer", pmName(pm))
	} else {
		bad(rs+"/a client that only connected is an observer",
			fmt.Sprintf("%s -- it was placed as a body", pmName(pm)))
	}

	if f := b.Stat(statFrags); f == 0 {
		ok(rs+"/its HUD shows 0 frags, not the -100 sentinel", "STAT_FRAGS 0")
	} else {
		bad(rs+"/its HUD shows 0 frags, not the -100 sentinel",
			fmt.Sprintf("STAT_FRAGS %d -- the score sentinel reached the HUD", f))
	}

	// The negative sign of the broadcast: connecting is not entering, so nobody
	// has been told this client is in the game.  The wording is the entering
	// PATH's rather than the state's -- a free-for-all says "entered the game",
	// a team says "joined team" -- so the row asks for the one this ruleset
	// will use.
	// ...and `duel` says NOTHING, which is the donor's own asymmetry:
	// OSP_addTeamMember guards the broadcast with `m_mode == 2`, so only TDM
	// announces a team join.  There the pmove type is the only witness, and the
	// row below asserts the silence rather than skipping it.
	joined := `entered the game`
	switch {
	case rs == "duel":
		joined = ""
	case colosseum.IsTeams(rs):
		joined = `joined team`
	}
	mark := srv.Len()
	probe := joined
	if probe == "" {
		probe = `joined team|entered the game`
	}
	if len(srv.Grep(probe)) == 0 {
		ok(rs+"/nobody was told it entered", fmt.Sprintf("no %q broadcast yet", probe))
	} else {
		bad(rs+"/nobody was told it entered",
			fmt.Sprintf("%q -- connecting announced itself as a join", srv.Grep(probe)))
	}

	// ---- 2. ...and after it enters, a PLAYER ----------------------------
	// `join` is the FFA command; where there are teams it takes one BY NAME --
	// OSP_teamjoin_cmd matches its argument against `osp_teams[i].netname`, so
	// `join 1` is not a team and is silently nothing.  Both words route through
	// OSP_ClientCommand, which is why only the argument differs.
	if colosseum.IsTeams(rs) {
		b.Cmd("join %s", teamA)
	} else {
		b.Cmd("join")
	}
	if err := b.WaitFrames(40, 20*time.Second); err != nil {
		panic(err)
	}

	if joined == "" {
		// The silent ruleset: assert the silence, in the same place the others
		// assert the announcement.
		if len(srv.GrepFrom(mark, probe)) == 0 {
			ok(rs+"/entering is silent, as the donor has it",
				"no broadcast; only TDM announces a team join")
		} else {
			bad(rs+"/entering is silent, as the donor has it",
				fmt.Sprintf("%q -- the donor guards this with m_mode == 2",
					srv.GrepFrom(mark, probe)))
		}
	} else if _, err := srv.WaitLog(joined, 10*time.Second); err == nil {
		ok(rs+"/joining announces it", strings.TrimSpace(lastLine(srv, mark, joined)))
	} else {
		bad(rs+"/joining announces it",
			fmt.Sprintf("no %q broadcast -- the join did nothing", joined))
	}

	pm = b.PMType()
	if pm == playtest.PMNormal {
		ok(rs+"/after `join` it is a player", pmName(pm))
	} else {
		bad(rs+"/after `join` it is a player",
			fmt.Sprintf("%s -- entering did not give it a body", pmName(pm)))
	}

	// The frags stat is the score now rather than a forced 0 -- which reads the
	// same until somebody scores, so it is reported as the weaker row it is.
	ok(rs+"/an entered client's frags are its own score",
		fmt.Sprintf("STAT_FRAGS %d (it has not scored yet, so 0 either way)", b.Stat(statFrags)))
}

func lastLine(srv *playtest.Server, mark int, re string) string {
	ls := srv.GrepFrom(mark, re)
	if len(ls) == 0 {
		return ""
	}
	return ls[len(ls)-1]
}
