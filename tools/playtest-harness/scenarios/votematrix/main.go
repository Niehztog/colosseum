// votematrix -- three vote systems, seven rulesets, and the head count every
// one of them divides by.
//
// Colosseum carries THREE voting systems, one per donor, sharing no code:
//
//	the OSP four  `vote <what> <value>` then `vote yes|no`.  A PERCENTAGE:
//	              `vote_yea * 100 / voters` against `vote_threshold` (51).
//	              osp_cmds.c, OSP_votePercent/OSP_checkVote.
//	ctf           an ELECTION.  `admin`, `warp` and the match request start
//	              one; `yes` counts toward `(players * electpercentage) / 100`,
//	              and the proposer may not vote.  g_ctf.c, CTFBeginElection.
//	arena         a per-arena settings PROPOSAL driven through the observer
//	              menu and carried when `yes - no >= voters / 3`, resolved
//	              thirty seconds later.  arena.c, start_voting/check_voting.
//	sp            none -- and that is the fourth claim, not the absence of one.
//
// WHY THE HEAD COUNT IS THE AXIS.  Every one of those three divides by a count
// of the people on the server, and each counts a DIFFERENT set: OSP counts
// connected clients under `dm`/`dmpro` and ENTERED ones under `tdm`/`duel`
// (`vote_countspectators` defaults differently for a teams ruleset), ctf counts
// every `inuse` client including the proposer who may not vote, and arena
// counts the clients in ONE arena.  A test with a single client cannot see any
// of that: one voter makes every threshold 100% and every divisor 1, which is
// the one head count at which all three systems agree and all three would still
// agree if the arithmetic were deleted.  So each ruleset is driven up a LADDER
// -- one connected player, then two, then three -- putting the same proposal at
// each rung and asserting what the system does with it.
//
// WHAT THE LADDER PINS, PER SYSTEM.  These are the donors' own answers, read
// out of the arithmetic and confirmed here against the running game:
//
//	              1 player            2 players           3 players
//	OSP           proposer alone      50% -- NOT enough,  33% (duel: 50%,
//	              is 100%, so the     a second yes        the third cannot
//	              vote carries on     carries it          enter), and two of
//	              the propose                             three carries it
//	ctf           REFUSED, "Not       needvotes 1, and    needvotes STILL 1:
//	              enough players      the proposer is     (3 * 66) / 100
//	              for election"       barred from it      truncates to 1
//	arena         1 yes >= 1/3,       1 yes >= 2/3 too,   2-1 >= 3/3 carries,
//	              carries             but 1-1 = 0 fails   1-1 = 0 does not
//
// The ctf column is the one worth reading twice: two players and three players
// need the SAME number of yeses, because `(count * electpercentage) / 100` is
// integer division and 66% of both 2 and 3 truncates to 1.  That is Threewave's
// own arithmetic, so it is asserted rather than corrected -- but it is asserted
// on the ANNOUNCED `Needed:` figure, which is what a change to it would move.
//
// FIVE THINGS THAT COST TIME HERE.
//
//  1. `vote_time` IS CLAMPED UP TO 30 AT INIT AND SETTABLE AFTERWARDS.
//     OSP_gameInit does `if (vote_time < 30) cvar_set("vote_time", "30")`, so
//     `+set vote_time 12` on the command line is silently 30 and the row that
//     waits for a vote to expire waits half a minute.  The clamp runs once, so
//     the scenario sets it from the server CONSOLE after the boot instead.
//
//  2. UNDER OSP A `no` DOES NOT FAIL A VOTE AT THESE COUNTS.  OSP_checkVote's
//     fail arm is `vote_nay * 100 / t >= vote_threshold`, where `t` is the YES
//     PERCENTAGE rather than the voter count -- so with one yes and one no of
//     two voters it reads 1 * 100 / 50 = 2, and 2 is not 51.  Only the clock
//     ends such a vote.  That is the target's own arithmetic and the row that
//     covers it says so; a scenario that expected "Vote failed" there would
//     report correct behaviour as broken.
//
//  3. A ROCKET ARENA VOTE NEEDS EVERYBODY IN ONE ARENA AND NO ROUND RUNNING.
//     `start_voting` counts clients by `resp.context`, so the three have to be
//     in the same arena; and it sets `proposetime = level.time + 30000` -- eight
//     hours -- when that arena is FIGHTING or in COUNTDOWN, so a round starting
//     means the vote never resolves and the row times out looking like a broken
//     tally.  Both are arranged by putting all three on ONE pickup team: an
//     arena with a single team fails `check_for_teams` at the end of every
//     warmup and goes straight back to it, so no round ever starts, and a
//     pickup side has room for everybody -- RA2 forces `playersperteam` to 128
//     on a pickup arena (arena.c, beside the two teams it creates there), so
//     `check_teams`, which ejects a team larger than that setting, ejects
//     nobody.
//
//  4. ROCKET ARENA ANSWERS A MENU WITH A MENU.  `menu_centerprint` does not
//     centerprint under `arena`: it builds a menu titled "Message" and pushes it
//     over whatever the client had open.  `start_voting` sends one to every
//     OTHER client in the arena the moment a proposal is put, so the second and
//     third voter are looking at that and not at the observer menu -- and
//     `inven` will not get them back, because it toggles the CURRENT menu and
//     the current menu is now the message.  It has to be dismissed through its
//     own "Continue" row.  Every refusal in the propose and vote paths arrives
//     the same way, which is also where their text can be read from.
//
//  5. ...AND IT DOES NOT STORE THE NUMBER THAT WAS TYPED.  The arena settings
//     menu takes a round count and writes `(it->num / 2) * 2 + 1` -- forced odd,
//     because a match is best-of-N and `wins > rounds / 2` is unreachable from
//     an even one.  So a proposal driven to 10 leaves the arena holding 11, and
//     the first version of this scenario reported that as a broken tally.  The
//     one-player cycle now drives an EVEN value ON PURPOSE and asserts the odd
//     result, so the rule is covered rather than stepped around.  Generally:
//     when a menu-driven setting comes back different from what was entered,
//     read the writer before believing the reader.
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
	"q2playtest/ra2"
)

var (
	q2proded = flag.String("q2proded", "", "dedicated server binary")
	ref      = flag.String("ref", "/usr/share/games/quake2/baseq2", "retail baseq2 paks")
	ctfdata  = flag.String("ctf", "/usr/share/games/quake2/ctf", "Threewave paks")
	lib      = flag.String("lib", "", "game library under test")
	dir      = flag.String("dir", "/tmp/q2playtest/votematrix", "scratch install root")
	port     = flag.Int("port", 27970, "first UDP port; each ruleset takes the next")
	rulesets = flag.String("rulesets", "dm,dmpro,tdm,duel,ctf,arena,sp",
		"which rulesets to drive, comma separated")
	keep = flag.Bool("keep", false, "keep the scratch dirs")
)

const (
	// The two team names tourney registers, which are `team_a_name` and
	// `team_b_name`'s defaults.  Entering a teams ruleset is `join <name>` --
	// OSP_teamjoin_cmd matches its argument against the team's netname, so a
	// number is not a team and `join 1` is silently nothing.
	teamA = "Hometeam"
	teamB = "Visitors"

	// The OSP vote subject.  `timelimit` is the cheapest one that leaves a
	// witness: OSP_timelimit_vote broadcasts "New timelimit: <n>" and sets the
	// cvar, so the value names WHICH row passed, and every value below is large
	// enough that no level ends while the scenario is running.
	voteBase = 20

	// The arena setting the proposal moves.  `rounds` is the one a player may
	// vote on by default (`allow_voting_rounds`) whose value is also reported
	// by a server diagnostic: `sv arenadump` prints `round=<n>/<rounds>`.
	// The stock value on an idmap is 9.
	arenaRoundsStart = 9

	// How long to give an arena vote to resolve.  check_voting fires when
	// `proposetime <= level.time` and start_voting sets that thirty seconds
	// out; there is no cvar to shorten it.
	arenaVoteWait = 40 * time.Second
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
	fmt.Printf("  %s %-58s %s\n", tag, name, note)
	return ok
}

func skip(name, why string) {
	total++
	skipped++
	fmt.Printf("  [skip] %-58s %s\n", name, why)
}

func report() {
	fmt.Printf("\n%d check(s), %d failed, %d skipped\n", total, failed, skipped)
}

// ------------------------------------------------------------------ plumbing

// boot installs a gamedir of its own and starts one server on it.
//
// A dir per ruleset rather than one shared one, because the arena phase writes
// an arena.cfg into the gamedir and the ctf phase wants the Threewave paks; a
// shared directory would make each phase depend on the order of the others.
// `prep` runs after the gamedir is built and BEFORE the map loads, which is the
// only window in which a fixture the map reads at load time -- arena.cfg is
// read by load_config from SpawnEntities -- can be put in place.
func boot(tag, rs, mapname string, p int, cvars map[string]string,
	prep func(game string) error) (*playtest.Server, string, error) {

	d := filepath.Join(*dir, tag)
	if err := colosseum.Install(d, *ref, *ctfdata, *lib); err != nil {
		return nil, "", err
	}
	game := filepath.Join(d, "colosseum")
	if prep != nil {
		if err := prep(game); err != nil {
			return nil, game, err
		}
	}

	cv := map[string]string{
		"g_ruleset":  rs,
		"deathmatch": "1",
		"coop":       "0",
		// Nothing may add a player: every voter this scenario counts is one it
		// connected itself, and a bot arriving mid-ladder changes the divisor
		// under a row that has already read it.
		"bots":            "0",
		"botfill":         "0",
		"bots_minplayers": "0",
		"bots_autoload":   "0",
		"minimumplayers":  "0",
		// No clock and no fraglimit: a level change would take the vote state
		// with it, and every level end in this scenario must be one it asked
		// for (none of them do).
		"timelimit":  "0",
		"fraglimit":  "0",
		"flood_msgs": "0",
		"dmflags":    "1024", // DF_FORCE_RESPAWN
	}
	for k, v := range cvars {
		cv[k] = v
	}

	srv := &playtest.Server{
		Binary: *q2proded, Dir: d, Game: "colosseum",
		Map: mapname, Port: p, MaxClients: 8,
		Cvars:   cv,
		LogPath: filepath.Join(d, "server.log"),
	}
	if err := srv.Start(); err != nil {
		return nil, game, err
	}
	time.Sleep(1500 * time.Millisecond)
	return srv, game, nil
}

// connect brings one headless client onto the server and leaves it wherever the
// ruleset puts a fresh arrival.  Under the OSP four and under arena that is an
// observer; entering is a separate act and has its own helper.
func connect(name string, p int) (*playtest.Bot, error) {
	b := playtest.NewBot(name, "127.0.0.1", p)
	if err := b.Start(30 * time.Second); err != nil {
		return nil, err
	}
	b.WaitFrames(10, 5*time.Second)
	return b, nil
}

// printsFrom returns this client's prints from index n onwards that match re,
// which is the only honest way to ask "did the server answer THIS command":
// every refusal in all three systems is a cprintf and most of them are worded
// the same in more than one place.
func printsFrom(b *playtest.Bot, n int, re string) []string {
	rx := regexp.MustCompile(re)
	pr := b.Prints()
	if n < 0 {
		n = 0
	}
	if n > len(pr) {
		n = len(pr)
	}
	var out []string
	for _, s := range pr[n:] {
		if rx.MatchString(s) {
			out = append(out, s)
		}
	}
	return out
}

// waitPrintFrom waits for a print matching re to arrive AFTER index n.
func waitPrintFrom(b *playtest.Bot, n int, re string, d time.Duration) (string, bool) {
	deadline := time.Now().Add(d)
	for {
		if m := printsFrom(b, n, re); len(m) > 0 {
			return strings.TrimSpace(m[len(m)-1]), true
		}
		if time.Now().After(deadline) {
			return "", false
		}
		time.Sleep(50 * time.Millisecond)
	}
}

// waitLogFrom is the same question asked of the server console, which is where
// every broadcast (gi.bprintf) and every diagnostic (gi.dprintf) lands.
func waitLogFrom(srv *playtest.Server, n int, re string, d time.Duration) (string, bool) {
	deadline := time.Now().Add(d)
	for {
		if m := srv.GrepFrom(n, re); len(m) > 0 {
			return strings.TrimSpace(m[len(m)-1]), true
		}
		if time.Now().After(deadline) {
			return "", false
		}
		time.Sleep(50 * time.Millisecond)
	}
}

// tail is what to print when a row failed: the last few things the client was
// told, which is where the reason is when there is one.
func tail(b *playtest.Bot, n int) string {
	pr := b.Prints()
	if n > len(pr) {
		n = len(pr)
	}
	pr = pr[n:]
	if len(pr) > 4 {
		pr = pr[len(pr)-4:]
	}
	for i := range pr {
		pr[i] = strings.TrimSpace(strings.ReplaceAll(pr[i], "\n", " / "))
	}
	if len(pr) == 0 {
		return "(the server said nothing)"
	}
	return strings.Join(pr, " | ")
}

// ------------------------------------------------------------- the OSP four

// ospEnter puts a client in the game and confirms it got there.
//
// Under the OSP four connecting is not entering, and `osp_entered` is what both
// OSP_votePercent's divisor and OSP_yes_cmd's observer gate read -- so a client
// that never entered is a client whose vote may be refused outright and which
// may or may not count toward the threshold, depending on the ruleset.  One
// `join` is not reliable (see scenarios/ospenter), so it is resent.
func ospEnter(srv *playtest.Server, b *playtest.Bot, rs, team string) bool {
	in := regexp.QuoteMeta(b.Name) + ` (entered the game|joined team)`
	for i := 0; i < 10; i++ {
		if len(srv.Grep(in)) > 0 || b.PMType() == playtest.PMNormal {
			b.WaitFrames(5, 3*time.Second)
			return true
		}
		if colosseum.IsTeams(rs) {
			b.Cmd("join %s", team)
		} else {
			b.Cmd("join")
		}
		b.WaitFrames(8, 4*time.Second)
	}
	return b.PMType() == playtest.PMNormal
}

// ospVoters is OSP_votePercent's divisor, which is NOT the same count in every
// ruleset and is the reason this scenario asserts a number rather than merely
// "it passed".
//
// `vote_countspectators` is registered with a default of 1 for a free-for-all
// and 0 for a teams ruleset -- `if (!OSP_IsTeams())` in OSP_gameInit -- so
// dm and dmpro divide by everybody CONNECTED while tdm and duel divide by the
// people who actually entered.  The clamp to at least one is the target's, and
// it is what kept an empty server from dividing by zero.
func ospVoters(rs string, connected, entered int) int {
	n := connected
	if colosseum.IsTeams(rs) && entered > 0 {
		n = entered
	}
	if n < 1 {
		n = 1
	}
	return n
}

// ospPropose puts a timelimit proposal and reports what the server broadcast.
func ospPropose(srv *playtest.Server, b *playtest.Bot, minutes int) (mark, pmark int) {
	mark, pmark = srv.Len(), len(b.Prints())
	b.Cmd("vote timelimit %d", minutes)
	b.WaitFrames(6, 4*time.Second)
	return mark, pmark
}

// ospTally reads the live tally off the wire by asking for it: a bare `vote`
// while one is running prints the proposal and OSP_votePercent's two
// percentages to the client that asked.
//
// Read from an ENTERED client only.  The observer gate is at the TOP of
// OSP_vote_cmd, before the argc == 1 arm, so under tdm and duel an observer
// asking for the tally is refused the same way it would be refused a vote.
func ospTally(b *playtest.Bot) (percent int, raw string, ok bool) {
	n := len(b.Prints())
	b.Cmd("vote")
	line, got := waitPrintFrom(b, n, `% have accepted`, 5*time.Second)
	if !got {
		return 0, tail(b, n), false
	}
	m := rePercent.FindStringSubmatch(line)
	if m == nil {
		return 0, line, false
	}
	v, _ := strconv.Atoi(m[1])
	return v, strings.ReplaceAll(strings.TrimSpace(line), "\n", " / "), true
}

var rePercent = regexp.MustCompile(`(\d+)% have accepted`)

func ospPhase(rs string, p int) error {
	fmt.Printf("\n##### %s -- OSP Tourney: `vote <what>` and a percentage of the voters\n", rs)

	srv, _, err := boot("osp-"+rs, rs, "q2dm1", p, map[string]string{
		// Written down rather than inherited: every one of these is a default
		// this scenario's arithmetic depends on, and a row silently disabled by
		// an inherited config would look like a broken tally.
		"vote_enable":      "1",
		"vote_enable_time": "1",
		"vote_threshold":   "51",
		// `match_strictmode` off so an observer may still use the console, and
		// the ready nag left where it is -- nothing here readies up, so no
		// match starts and `sync_stat` stays in warmup for the whole ladder.
		"match_strictmode": "0",
	}, nil)
	if err != nil {
		return err
	}
	defer srv.Stop()

	// See note 1: the init clamp has already run, so this is the only place a
	// vote can be made to expire in less than half a minute.
	srv.Console("set vote_time 12")
	time.Sleep(400 * time.Millisecond)

	// The announced proposal time is the CLAMPED 30-or-more, not the 12 set
	// just above it: OSP_gameInit prints before this scenario can lower it, and
	// the row that waits for a vote to expire is what confirms the lower value
	// took.  See note 1.
	check(rs+"/voting is on at boot",
		len(srv.Grep(`Client voting enabled!`)) > 0,
		"%s (lowered to 12s from the console for the timeout row)",
		firstLine(srv, `Proposal time:|voting DISABLED`))

	// ---------------------------------------------------------- 1 player
	a, err := connect("alpha", p)
	if err != nil {
		return err
	}
	defer a.Disconnect()
	if !ospEnter(srv, a, rs, teamA) {
		return fmt.Errorf("%s: alpha never entered the game", rs)
	}

	mark, pmark := ospPropose(srv, a, voteBase)
	_, started := waitLogFrom(srv, mark, `Proposal: `, 5*time.Second)
	line, passed := waitLogFrom(srv, mark, `Vote passed!`, 5*time.Second)
	check(rs+"/1 player: the proposal is put", started,
		"%s", firstFrom(srv, mark, `Proposal: |disabled|Invalid`))
	check(rs+"/1 player: the proposer alone is 100%, so it carries", passed,
		"%s", pick(line, tail(a, pmark)))
	_, applied := waitLogFrom(srv, mark, `New timelimit: `+strconv.Itoa(voteBase), 5*time.Second)
	check(rs+"/1 player: ...and the timelimit really moved", applied,
		"%s", firstFrom(srv, mark, `New timelimit: `))

	// ---------------------------------------------------------- 2 players
	b, err := connect("bravo", p)
	if err != nil {
		return err
	}
	defer b.Disconnect()
	// THE OTHER TEAM, and under `duel` that is not a stylistic choice:
	// `team_maxplayers` is 1 there against 4 elsewhere, so a second client
	// asking for the team the first is on is refused and never enters.
	if !ospEnter(srv, b, rs, teamB) {
		return fmt.Errorf("%s: bravo never entered the game", rs)
	}

	want := 100 / ospVoters(rs, 2, 2)
	mark, pmark = ospPropose(srv, a, voteBase+1)
	_, early := waitLogFrom(srv, mark, `Vote passed!`, 2500*time.Millisecond)
	check(rs+"/2 players: one of two is not a majority", !early,
		"%d%% of %d voters, threshold 51", want, ospVoters(rs, 2, 2))
	pc, raw, got := ospTally(a)
	check(rs+"/2 players: the tally says so", got && pc == want,
		"wanted %d%%, %s", want, pick(raw, "no tally came back"))

	// The second yes, and the row that proves the first one was counted at all.
	mark2 := srv.Len()
	b.Cmd("vote yes")
	line, passed = waitLogFrom(srv, mark2, `Vote passed!`, 6*time.Second)
	check(rs+"/2 players: the second yes carries it", passed,
		"%s", pick(line, tail(b, pmark)))
	_, applied = waitLogFrom(srv, mark2, `New timelimit: `+strconv.Itoa(voteBase+1), 5*time.Second)
	check(rs+"/2 players: ...and the timelimit really moved", applied,
		"%s", firstFrom(srv, mark2, `New timelimit: `))

	// A `no`, and what it does NOT do -- see note 2.  The claim is asserted in
	// three parts because only the third is about the clock: the vote does not
	// pass, it does not fail on the nay either, and it is the timeout that ends
	// it with the setting untouched.
	mark, _ = ospPropose(srv, a, voteBase+2)
	nmark, npmark := srv.Len(), len(b.Prints())
	b.Cmd("vote no")
	b.WaitFrames(6, 4*time.Second)
	_, passedN := waitLogFrom(srv, nmark, `Vote passed!`, 2500*time.Millisecond)
	check(rs+"/2 players: a `no` does not carry the vote", !passedN,
		"%s", pick(firstFrom(srv, nmark, `Vote (passed|failed)`), "nothing was declared"))
	check(rs+"/2 players: ...and does not fail it either (nay*100/yes%)",
		len(srv.GrepFrom(nmark, `Vote failed: `)) == 0,
		"OSP_checkVote divides the nay tally by the YES PERCENTAGE: 1*100/50 = 2, not 51")
	line, timedout := waitLogFrom(srv, nmark, `Time up\. Vote failed\.`, 20*time.Second)
	check(rs+"/2 players: the clock is what ends it", timedout,
		"%s", pick(line, tail(b, npmark)))
	check(rs+"/2 players: ...with the timelimit untouched",
		len(srv.GrepFrom(mark, `New timelimit: `+strconv.Itoa(voteBase+2))) == 0,
		"nothing set timelimit to %d", voteBase+2)

	// ---------------------------------------------------------- 3 players
	c, err := connect("charlie", p)
	if err != nil {
		return err
	}
	defer c.Disconnect()
	// Back to the first team: under `tdm` it has room (4 per team) and under
	// `duel` both teams are now full, which is the point of the row below.
	entered3 := ospEnter(srv, c, rs, teamA)

	// Under `duel` the third client CANNOT enter: team_maxplayers is 1 there
	// (4 everywhere else), so both teams are full and the arrival stays an
	// observer.  That is not a failure to assert around -- it is the ruleset,
	// and it changes both halves of the arithmetic below.
	if rs == "duel" {
		check(rs+"/3 players: the third cannot enter a 1-vs-1", !entered3,
			"team_maxplayers is 1: %s", pmName(c.PMType()))
	} else {
		check(rs+"/3 players: the third enters like the others", entered3,
			"%s", pmName(c.PMType()))
	}

	active := 3
	if !entered3 {
		active = 2
	}
	voters := ospVoters(rs, 3, active)
	want = 100 / voters

	mark, pmark = ospPropose(srv, a, voteBase+3)
	_, early = waitLogFrom(srv, mark, `Vote passed!`, 2500*time.Millisecond)
	check(fmt.Sprintf("%s/3 players: one of %d voters is not a majority", rs, voters), !early,
		"%d%% of %d voters, threshold 51", want, voters)
	pc, raw, got = ospTally(a)
	check(rs+"/3 players: the threshold is read off the head count",
		got && pc == want,
		"wanted %d%% (%d voter(s)), %s", want, voters, pick(raw, "no tally came back"))

	// The proposer is already counted, which is the other half of "one of
	// three": a second yes from the same client must not move the tally.
	amark := len(a.Prints())
	a.Cmd("vote yes")
	line, refused := waitPrintFrom(a, amark, `already voted`, 4*time.Second)
	check(rs+"/3 players: the proposer's own yes is already counted", refused,
		"%s", pick(line, tail(a, amark)))

	mark2 = srv.Len()
	b.Cmd("vote yes")
	line, passed = waitLogFrom(srv, mark2, `Vote passed!`, 6*time.Second)
	check(fmt.Sprintf("%s/3 players: two of %d voters carries it", rs, voters), passed,
		"%d%% of %d voters, %s", 200/voters, voters, pick(line, tail(b, pmark)))
	_, applied = waitLogFrom(srv, mark2, `New timelimit: `+strconv.Itoa(voteBase+3), 5*time.Second)
	check(rs+"/3 players: ...and the timelimit really moved", applied,
		"%s", firstFrom(srv, mark2, `New timelimit: `))

	// The observer's own row, where there is an observer to ask.  With
	// `vote_countspectators` 0 and players in the game, OSP_vote_cmd refuses an
	// observer before it reads its arguments -- so the refusal is the same for
	// a vote and for a request to see the tally.
	if !entered3 {
		cmark := len(c.Prints())
		c.Cmd("vote yes")
		line, no := waitPrintFrom(c, cmark, `Observers cannot vote`, 4*time.Second)
		check(rs+"/3 players: an observer may not vote with players in the game", no,
			"%s", pick(line, tail(c, cmark)))
	} else {
		skip(rs+"/3 players: an observer may not vote with players in the game",
			"all three entered, so there is no observer here to refuse")
	}

	// ------------------------------------------------- the other two systems
	//
	// One library holds all three, so "this ruleset's vote works" is only half
	// the claim.  `admin` IS a command here -- osp_clientcmd.c routes it to the
	// referee login -- which makes it the sharper probe: it must reach THAT and
	// not Threewave's election.
	lmark := srv.Len()
	amark = len(a.Prints())
	a.Cmd("admin")
	a.WaitFrames(6, 4*time.Second)
	check(rs+"/no ctf election is reachable from here",
		len(srv.GrepFrom(lmark, `requested admin rights|Type YES or NO to vote`)) == 0 &&
			len(printsFrom(a, amark, `Not enough players for election`)) == 0,
		"`admin` is the referee login here: %s", tail(a, amark))

	return nil
}

func pmName(t int) string {
	switch t {
	case playtest.PMNormal:
		return "PM_NORMAL (a player)"
	case playtest.PMSpectator:
		return "PM_SPECTATOR (an observer)"
	}
	return fmt.Sprintf("pm_type %d", t)
}

// ------------------------------------------------------------------ ctf

func ctfPhase(p int) error {
	fmt.Printf("\n##### ctf -- Threewave: an election, and a quorum that truncates\n")

	srv, _, err := boot("ctf", "ctf", "q2ctf1", p, map[string]string{
		"electpercentage": "66", // the shipped default, written down
		"allow_admin":     "1",
		"competition":     "0",
	}, nil)
	if err != nil {
		return err
	}
	defer srv.Stop()

	// ---------------------------------------------------------- 1 player
	a, err := connect("alpha", p)
	if err != nil {
		return err
	}
	defer a.Disconnect()

	amark := len(a.Prints())
	a.Cmd("admin")
	line, refused := waitPrintFrom(a, amark, `Not enough players for election`, 5*time.Second)
	check("ctf/1 player: one player is not an electorate", refused,
		"%s", pick(line, tail(a, amark)))

	// ...and the refusal left no election behind, which the count alone does
	// not prove: CTFBeginElection writes `ctfgame.election` BEFORE it counts in
	// the donor's other arms, so "nothing started" is its own question.
	amark = len(a.Prints())
	a.Cmd("yes")
	line, none := waitPrintFrom(a, amark, `No election is in progress`, 4*time.Second)
	check("ctf/1 player: ...and no election was left behind", none,
		"%s", pick(line, tail(a, amark)))

	// ---------------------------------------------------------- 2 players
	b, err := connect("bravo", p)
	if err != nil {
		return err
	}
	defer b.Disconnect()

	mark := srv.Len()
	amark = len(a.Prints())
	a.Cmd("admin")
	line, began := waitLogFrom(srv, mark, `has requested admin rights`, 5*time.Second)
	check("ctf/2 players: two players make an electorate", began,
		"%s", pick(line, tail(a, amark)))
	need, needline, gotNeed := ctfNeeded(srv, mark)
	check("ctf/2 players: the quorum is (2 * 66) / 100 = 1", gotNeed && need == 1,
		"%s", pick(needline, "no `Votes:/Needed:` broadcast"))

	// The proposer is the TARGET of an admin election, and Threewave bars the
	// target from voting -- which is why a two-player election needs the OTHER
	// player and not merely one yes from anybody.
	amark = len(a.Prints())
	a.Cmd("yes")
	line, self := waitPrintFrom(a, amark, `can't vote for yourself`, 4*time.Second)
	check("ctf/2 players: the proposer may not vote for itself", self,
		"%s", pick(line, tail(a, amark)))

	mark = srv.Len()
	b.Cmd("yes")
	line, won := waitLogFrom(srv, mark, `alpha has become an admin`, 6*time.Second)
	check("ctf/2 players: the other player's yes carries it", won,
		"%s", pick(line, tail(b, len(b.Prints()))))

	// ---------------------------------------------------------- 3 players
	c, err := connect("charlie", p)
	if err != nil {
		return err
	}
	defer c.Disconnect()

	mark = srv.Len()
	cmark := len(c.Prints())
	c.Cmd("admin")
	_, began = waitLogFrom(srv, mark, `charlie has requested admin rights`, 5*time.Second)
	check("ctf/3 players: a third player may still stand", began,
		"%s", pick(firstFrom(srv, mark, `requested admin rights`), tail(c, cmark)))
	need, needline, gotNeed = ctfNeeded(srv, mark)
	check("ctf/3 players: the quorum is STILL 1 -- (3 * 66) / 100 truncates",
		gotNeed && need == 1,
		"%s", pick(needline, "no `Votes:/Needed:` broadcast"))

	// A second election while one runs is refused, which is the state half of
	// the system: `ctfgame.election` is a single slot for the whole server.
	bmark := len(b.Prints())
	b.Cmd("admin")
	line, busy := waitPrintFrom(b, bmark, `Election already in progress`, 4*time.Second)
	check("ctf/3 players: a second election is refused while one runs", busy,
		"%s", pick(line, tail(b, bmark)))

	mark = srv.Len()
	a.Cmd("yes")
	line, won = waitLogFrom(srv, mark, `charlie has become an admin`, 6*time.Second)
	check("ctf/3 players: one yes of three carries it", won,
		"%s", pick(line, tail(a, len(a.Prints()))))

	// The negative sign, and the only row here that needs the clock: a `no` is
	// not a veto -- CTFVoteNo counts nothing at all, it only spends the voter
	// -- so an election nobody accepts ends by TIMING OUT after its twenty
	// seconds, and the request it carried does not happen.
	mark = srv.Len()
	bmark = len(b.Prints())
	b.Cmd("admin")
	if _, ok := waitLogFrom(srv, mark, `bravo has requested admin rights`, 5*time.Second); !ok {
		skip("ctf/3 players: an election nobody accepts times out",
			"the election never started: "+tail(b, bmark))
	} else {
		a.Cmd("no")
		c.Cmd("no")
		line, out := waitLogFrom(srv, mark, `Election timed out and has been cancelled`, 30*time.Second)
		check("ctf/3 players: two `no`s leave it to time out", out,
			"%s", pick(line, "no cancellation in 30s"))
		check("ctf/3 players: ...and the request did not happen",
			len(srv.GrepFrom(mark, `bravo has become an admin`)) == 0,
			"nothing granted bravo admin")
	}

	// ------------------------------------------------- the other two systems
	lmark := srv.Len()
	amark = len(a.Prints())
	a.Cmd("vote timelimit 99")
	a.WaitFrames(6, 4*time.Second)
	check("ctf/no OSP vote is reachable from here",
		len(srv.GrepFrom(lmark, `Vote passed!|New timelimit: 99|Proposal: `)) == 0,
		"`vote` is not a ctf command; it reached the chat fallback: %s", tail(a, amark))

	return nil
}

var reNeeded = regexp.MustCompile(`Needed: (\d+)`)

// ctfNeeded reads the quorum out of the broadcast that announces it.
//
// The ANNOUNCED figure rather than a recomputed one: `needvotes` is what
// CTFVoteYes compares against with `==`, and the broadcast is the only place
// the server states it, so a change to the formula moves this line and nothing
// else a client can see.
func ctfNeeded(srv *playtest.Server, mark int) (int, string, bool) {
	line, ok := waitLogFrom(srv, mark, `Votes: \d+  Needed: \d+`, 6*time.Second)
	if !ok {
		return 0, "", false
	}
	m := reNeeded.FindStringSubmatch(line)
	if m == nil {
		return 0, line, false
	}
	n, _ := strconv.Atoi(m[1])
	return n, strings.TrimSpace(line), true
}

// ------------------------------------------------------------------ arena

// arenaCfg is the whole arena.cfg this phase runs on.  q2dm1 is an idmap, so
// Rocket Arena runs it as a single PICKUP arena and the global block is the
// entire file.
//
// `votetries` is raised from its default of 3 because the proposer here puts
// four proposals and only a PASSED one refunds the count (check_voting resets
// `ra_votes` for the arena), and `allowvotingrounds` is written down rather
// than inherited because it is the row the proposal moves: with it off the
// propose form has no Rounds row at all and every cycle would fail looking for
// it.  `rounds` is the donor's own idmap value, pinned so the ladder's
// arithmetic does not depend on the reference install's arena.cfg.
const arenaCfg = `maploop: q2dm1;
votetries: 6;
rounds: 9;
allowvotingrounds: 1;
`

func arenaPhase(p int) error {
	fmt.Printf("\n##### arena -- Rocket Arena: a proposal per arena, carried by yes - no >= voters/3\n")

	srv, _, err := boot("arena", "arena", "q2dm1", p, nil, func(game string) error {
		path := filepath.Join(game, "arena.cfg")
		// colosseum.Install symlinks the reference install's .cfg files in, so
		// the link (if there is one) is replaced by a real file.
		os.Remove(path)
		return os.WriteFile(path, []byte(arenaCfg), 0o644)
	})
	if err != nil {
		return err
	}
	defer srv.Stop()

	rounds, ok := arenaRounds(srv)
	if !check("arena/the scenario's arena.cfg was read", ok && rounds == arenaRoundsStart,
		"rounds=%d, wanted %d (`sv arenadump`)", rounds, arenaRoundsStart) {
		return fmt.Errorf("arena.cfg did not take: rounds=%d", rounds)
	}

	// ---------------------------------------------------------- 1 player
	a, err := connect("alpha", p)
	if err != nil {
		return err
	}
	defer a.Disconnect()
	if err := ra2.JoinTeam(a, ra2.PickupTeam(1, "Red")); err != nil {
		return fmt.Errorf("arena: alpha could not join: %w", err)
	}
	a.WaitFrames(10, 5*time.Second)

	// The target is EVEN, and that is the row's second claim: the arena will
	// hold 11 rather than the 10 the menu was driven to.  See raOddRounds.
	if err := arenaCycle(srv, a, nil, 1,
		"1 player: a lone voter carries its own proposal (even -> odd)",
		arenaRoundsStart+1, true); err != nil {
		return err
	}

	// ---------------------------------------------------------- 2 players
	b, err := connect("bravo", p)
	if err != nil {
		return err
	}
	defer b.Disconnect()
	if err := ra2.JoinTeam(b, ra2.PickupTeam(1, "Red")); err != nil {
		return fmt.Errorf("arena: bravo could not join: %w", err)
	}
	b.WaitFrames(10, 5*time.Second)

	// Both on ONE team on purpose: an arena with a single team never leaves
	// warmup, and a vote proposed in a FIGHTING arena is parked for 30000
	// seconds.  See note 3.
	if err := arenaCycle(srv, a, []voter{{b, abstain}}, 2,
		"2 players: one yes of two still clears voters/3", arenaRoundsStart+4, true); err != nil {
		return err
	}
	if err := arenaCycle(srv, a, []voter{{b, votesNo}}, 2,
		"2 players: ...but one `no` cancels it out and it fails", arenaRoundsStart+6, false); err != nil {
		return err
	}

	// ---------------------------------------------------------- 3 players
	c, err := connect("charlie", p)
	if err != nil {
		return err
	}
	defer c.Disconnect()
	if err := ra2.JoinTeam(c, ra2.PickupTeam(1, "Red")); err != nil {
		return fmt.Errorf("arena: charlie could not join: %w", err)
	}
	c.WaitFrames(10, 5*time.Second)

	// Three voters put the bar at exactly one clear vote: 2 - 1 >= 3/3 carries,
	// and the same proposal with only the proposer behind it (1 - 1 = 0) does
	// not -- which the two-player row above has already shown in the other
	// sign.
	if err := arenaCycle(srv, a, []voter{{b, votesYes}, {c, votesNo}}, 3,
		"3 players: two yes against one no carries it", arenaRoundsStart+6, true); err != nil {
		return err
	}

	// ------------------------------------------------- the other two systems
	lmark := srv.Len()
	amark := len(a.Prints())
	a.Cmd("vote timelimit 99")
	a.Cmd("yes")
	a.WaitFrames(6, 4*time.Second)
	check("arena/neither of the other two vote systems answers here",
		len(srv.GrepFrom(lmark, `Vote passed!|New timelimit: 99|Type YES or NO to vote`)) == 0,
		"both reached the chat fallback: %s", tail(a, amark))

	return nil
}

// ballot is how one of the other clients in the arena answers -- and `abstain`
// is a real third state rather than the absence of a row: an abstainer is still
// counted in `votetries`, so it raises the bar the proposal has to clear
// without ever adding to either tally.  It also still gets the proposal notice,
// which has to be cleared or it is still on that client's screen at the next
// cycle.
type ballot int

const (
	abstain ballot = iota
	votesYes
	votesNo
)

type voter struct {
	bot *playtest.Bot
	how ballot
}

// arenaCycle runs one whole proposal: the proposer walks the observer menu,
// bumps `Rounds` to want, proposes, the others vote, and the cycle waits for
// check_voting to declare it thirty seconds later.
//
// It asserts three things, and the third is the one the other two cannot fake:
// the voter count the server announced, the verdict it declared, and whether
// the arena's LIVE setting moved.  A tally that reads right and a memcpy that
// never happened are the same observation without that last one.
func arenaCycle(srv *playtest.Server, proposer *playtest.Bot, others []voter,
	voters int, name string, want int, shouldPass bool) error {

	before, _ := arenaRounds(srv)
	mark := srv.Len()

	if err := arenaProposeRounds(proposer, want); err != nil {
		return fmt.Errorf("%s: %w", name, err)
	}

	line, started := waitLogFrom(srv, mark, `Starting Voting in Arena \d+ with \d+ voters`, 10*time.Second)
	if !check("arena/"+name+" (the vote started)", started, "%s", pick(line, "no `Starting Voting` line")) {
		return nil
	}
	n := 0
	if m := reVoters.FindStringSubmatch(line); m != nil {
		n, _ = strconv.Atoi(m[1])
	}
	check("arena/"+name+" (the electorate is the arena)", n == voters,
		"%s -- wanted %d", strings.TrimSpace(line), voters)

	// start_voting pushes the "Settings changes have been proposed" notice as a
	// MENU onto every other client in the arena (note 4), so clear it before
	// anybody tries to drive their own -- including the clients that abstain,
	// whose notice would otherwise still be on screen at the next cycle.
	for _, v := range others {
		dismissMessage(v.bot)
	}
	for _, v := range others {
		if v.how == abstain {
			continue
		}
		if err := arenaVote(v.bot, v.how == votesYes); err != nil {
			return fmt.Errorf("%s: %s: %w", name, v.bot.Name, err)
		}
	}

	verdict := `Changes Passed!`
	if !shouldPass {
		verdict = `Changes Failed!`
	}
	line, decided := waitLogFrom(srv, mark, `Changes (Passed|Failed)!`, arenaVoteWait)
	check("arena/"+name, decided && strings.Contains(line, verdict),
		"%s -- wanted %q", pick(strings.TrimSpace(line), "nothing was declared"), verdict)

	// ...and the world.  `sv arenadump` prints `round=<n>/<rounds>`, which is
	// the LIVE setting rather than the proposal, so it moves only if
	// check_voting copied `proposed` over it.
	time.Sleep(700 * time.Millisecond)
	after, _ := arenaRounds(srv)
	wantAfter := before
	if shouldPass {
		wantAfter = raOddRounds(want)
	}
	check("arena/"+name+" (the arena's own setting)", after == wantAfter,
		"rounds %d -> %d, wanted %d", before, after, wantAfter)
	return nil
}

var reVoters = regexp.MustCompile(`with (\d+) voters`)

// raOddRounds is menuApplyArenaAdmin's own rule for the Rounds row:
//
//	settings[1] = (it->num / 2) * 2 + 1;
//
// A round count is forced ODD on the way in, because a match is best-of-N and
// arena_think declares a winner at `wins > rounds / 2` -- an even count can end
// level.  So the menu can be driven to 10 and the arena will hold 11, and a
// scenario that asserted the number it typed would report the donor's rule as a
// defect (which is exactly what the first version of this one did).  The
// one-player cycle drives an EVEN value on purpose, so the rule is asserted
// rather than merely stepped around.
func raOddRounds(n int) int { return (n/2)*2 + 1 }

// arenaProposeRounds walks the propose form and puts a `Rounds` change.
//
// Rocket Arena has no console command for any of this -- the menu IS the
// feature -- and an RA2 menu is drawn into the client's STATUSBAR, so the only
// witness at every step is the client's own bar.
func arenaProposeRounds(b *playtest.Bot, want int) error {
	if err := openFromObserver(b, `(?i)change arena settings`, `(?i)arena admin menu`); err != nil {
		return fmt.Errorf("propose menu: %w", err)
	}
	dbg(b, "form open")
	if err := cursorTo(b, `(?i)^rounds:`, 20*time.Second); err != nil {
		return fmt.Errorf("rounds row: %w", err)
	}
	if err := bumpRow(b, `(?i)^rounds:`, want); err != nil {
		return err
	}
	dbg(b, "after bump")
	if err := cursorTo(b, `(?i)^propose$`, 20*time.Second); err != nil {
		return fmt.Errorf("propose row: %w", err)
	}
	dbg(b, "on Propose")
	err := b.MenuUse(8 * time.Second)
	time.Sleep(500 * time.Millisecond)
	dbg(b, "after Propose")
	return err
}

// bumpRow presses the selected settings row until the number it DRAWS reads
// `want`, confirming each press before sending the next one.
//
// Neither half of that is optional, and the first version of this scenario got
// both wrong in a way that passed its own check and proposed the wrong value.
// UseMenu drops an `invuse` that arrives within five frames of the last one and
// says nothing, so a fixed number of presses undershoots; and the statusbar the
// value is read back from arrives a frame or two later, so a loop that re-reads
// immediately sees the value BEFORE its own press and presses again -- which
// overshoots, and then reads a stale bar that agrees with it.  So: press once,
// wait for the drawn number to actually move, and confirm the final value twice.
func bumpRow(b *playtest.Bot, re string, want int) error {
	for i := 0; i < 40; i++ {
		got, ok := rowValue(b, re)
		if !ok {
			time.Sleep(200 * time.Millisecond)
			continue
		}
		if got > want {
			return fmt.Errorf("%s overshot: the row reads %d, wanted %d", re, got, want)
		}
		if got == want {
			// Settled rather than in flight: a value read between a press and
			// its redraw is the previous one, and agreeing with it by accident
			// is exactly the failure this function exists to stop.
			time.Sleep(500 * time.Millisecond)
			if again, ok := rowValue(b, re); ok && again == want {
				return nil
			}
			continue
		}
		b.Cmd("invuse")
		deadline := time.Now().Add(2500 * time.Millisecond)
		for time.Now().Before(deadline) {
			if v, ok := rowValue(b, re); ok && v != got {
				break
			}
			time.Sleep(60 * time.Millisecond)
		}
	}
	got, _ := rowValue(b, re)
	return fmt.Errorf("%s never reached %d (reads %d)", re, want, got)
}

// dismissMessage clears a "Message" menu if one is on screen and returns what
// it said -- see note 4.  Rocket Arena's `menu_centerprint` is a menu, not a
// centerprint, so this is both how a client gets back to the observer menu and
// the only place a refusal from the propose or vote path can be read.
func dismissMessage(b *playtest.Bot) string {
	var said string
	for i := 0; i < 4; i++ {
		if !reMessageMenu.MatchString(b.MenuTitle()) {
			return said
		}
		_, items := b.Menu()
		var text []string
		for _, it := range items {
			if t := strings.TrimSpace(strings.Trim(it.Text, "-")); t != "" &&
				!strings.EqualFold(t, "Continue") {
				text = append(text, t)
			}
		}
		if len(text) > 0 {
			said = strings.Join(text, " ")
		}
		if err := cursorTo(b, `(?i)continue`, 6*time.Second); err != nil {
			return said
		}
		if err := b.MenuUse(6 * time.Second); err != nil {
			return said
		}
		time.Sleep(400 * time.Millisecond)
	}
	return said
}

var reMessageMenu = regexp.MustCompile(`(?i)^message$`)

// arenaVote answers a running proposal yes or no.
func arenaVote(b *playtest.Bot, yes bool) error {
	if err := openFromObserver(b, `(?i)vote on changes`, `(?i)proposed changes`); err != nil {
		return fmt.Errorf("vote menu: %w", err)
	}
	row := `(?i)^vote\s+yes$`
	if !yes {
		row = `(?i)^vote\s+no$`
	}
	if err := cursorTo(b, row, 20*time.Second); err != nil {
		return fmt.Errorf("%q: %w", row, err)
	}
	return b.MenuUse(8 * time.Second)
}

// openFromObserver toggles the observer menu up and activates one of its rows,
// retrying the WHOLE walk rather than any single step.
//
// `inven` is a toggle and a closed RA2 menu can leave its last frame painted on
// the statusbar, so "the bar says Observer Options" and "the menu is open" are
// not the same fact -- the only reliable confirmation is that the row actually
// opened the submenu it names.
func openFromObserver(b *playtest.Bot, row, want string) error {
	var last error
	for try := 0; try < 6; try++ {
		if said := dismissMessage(b); said != "" && try > 0 {
			last = fmt.Errorf("the arena answered with a message: %q", said)
		}
		if err := b.WaitMenu(`(?i)observer options`, 2*time.Second); err != nil {
			b.Cmd("inven")
			if err := b.WaitMenu(`(?i)observer options`, 6*time.Second); err != nil {
				last = err
				continue
			}
		}
		if err := cursorTo(b, row, 10*time.Second); err != nil {
			last = err
			b.Cmd("inven")
			time.Sleep(400 * time.Millisecond)
			continue
		}
		if err := b.MenuUse(6 * time.Second); err != nil {
			last = err
			continue
		}
		if err := b.WaitMenu(want, 6*time.Second); err == nil {
			return nil
		} else {
			last = err
		}
		b.Cmd("inven")
		time.Sleep(400 * time.Millisecond)
	}
	return last
}

// cursorTo steps the menu cursor onto the first row matching re WITHOUT
// activating it.
//
// playtest.MenuPick cannot be used for the settings form: it fast-fails when
// the row it wants is not on the page it can see, which is right for a menu
// that fits and wrong for one that scrolls -- menu.c draws a window of 18 rows
// and the propose form is longer.  Stepping the cursor is what moves the
// window.
func cursorTo(b *playtest.Bot, re string, timeout time.Duration) error {
	rx := regexp.MustCompile(re)
	deadline := time.Now().Add(timeout)

	for steps := 0; time.Now().Before(deadline) && steps < 80; steps++ {
		_, items := b.Menu()
		for _, it := range items {
			if it.Selected && rx.MatchString(it.Text) {
				return nil
			}
		}
		before := b.StatusBar()
		b.Cmd("invnext")
		for i := 0; i < 20 && b.StatusBar() == before; i++ {
			time.Sleep(40 * time.Millisecond)
		}
	}
	_, items := b.Menu()
	return fmt.Errorf("cursor never reached %q in menu %q (rows: %s)",
		re, b.MenuTitle(), strings.Join(rowsOf(items), " | "))
}

// rowValue reads the number a settings row is currently drawing.  A drawn row
// is its label, then its value, then its `num` -- so the trailing digits are
// the setting.
func rowValue(b *playtest.Bot, re string) (int, bool) {
	rx := regexp.MustCompile(re)
	_, items := b.Menu()
	for _, it := range items {
		if !rx.MatchString(it.Text) {
			continue
		}
		m := reTrailingNum.FindStringSubmatch(it.Text)
		if m == nil {
			return 0, false
		}
		n, err := strconv.Atoi(m[1])
		return n, err == nil
	}
	return 0, false
}

var reTrailingNum = regexp.MustCompile(`(\d+)\s*$`)

// dbg prints the arena menu a client is looking at, under `VOTEDEBUG=1`.
//
// It earns its place: an RA2 menu IS the client's statusbar, so there is no
// server-side way to see what the scenario is driving, and every failure in the
// propose path reads as "the vote proposed the wrong value" until the rows are
// on screen.  That is how the odd-rounds rule was found.
func dbg(b *playtest.Bot, where string) {
	if os.Getenv("VOTEDEBUG") == "" {
		return
	}
	v, ok := rowValue(b, `(?i)^rounds:`)
	_, items := b.Menu()
	fmt.Printf("    ~~ %-14s %-8s menu=%q rounds=%d(%v) rows=%s\n",
		b.Name, where, b.MenuTitle(), v, ok, strings.Join(rowsOf(items), "|"))
}

func rowsOf(items []playtest.MenuItem) []string {
	out := make([]string, 0, len(items))
	for _, it := range items {
		out = append(out, it.Text)
	}
	return out
}

var reRound = regexp.MustCompile(`round=(\d+)/(\d+)`)

// arenaRounds reads arena 1's LIVE `rounds` setting off `sv arenadump`, from
// the block this call asked for rather than from any earlier one.
func arenaRounds(srv *playtest.Server) (int, bool) {
	mark := srv.Len()
	srv.Console("sv arenadump")
	line, ok := waitLogFrom(srv, mark, `arena 1\s`, 5*time.Second)
	if !ok {
		return 0, false
	}
	m := reRound.FindStringSubmatch(line)
	if m == nil {
		return 0, false
	}
	n, err := strconv.Atoi(m[2])
	return n, err == nil
}

// ------------------------------------------------------------------ sp

// spPhase is the fourth claim, and it is a claim rather than a gap: one library
// holds all three vote systems and every one of them is reachable by a client
// command, so "sp has no voting" is a thing the dispatch has to keep true.
//
// The witness is the CHAT FALLBACK rather than silence.  ClientCommand's last
// arm hands anything it did not recognise to Cmd_Say_f with arg0 set, so a
// command that reached no handler comes back as the client's own words --
// which tells "no handler claimed it" apart from "a handler claimed it and did
// nothing", and those need different fixes.
func spPhase(p int) error {
	fmt.Printf("\n##### sp -- the fourth claim: no vote system answers here\n")

	srv, _, err := boot("sp", "sp", "base1", p, map[string]string{
		"deathmatch": "0",
		"coop":       "0",
	}, nil)
	if err != nil {
		return err
	}
	defer srv.Stop()

	a, err := connect("alpha", p)
	if err != nil {
		return err
	}
	defer a.Disconnect()

	for _, probe := range []struct{ name, cmd, forbid string }{
		{"the OSP vote command", "vote timelimit 99",
			`Vote passed!|New timelimit: 99|Proposal: |Voting disabled on this server`},
		{"a ctf election", "admin",
			`requested admin rights|Type YES or NO to vote|Not enough players for election`},
		{"a ctf ballot", "yes",
			`No election is in progress|You already voted`},
	} {
		mark := srv.Len()
		amark := len(a.Prints())
		a.Cmd("%s", probe.cmd)
		a.WaitFrames(8, 5*time.Second)
		check("sp/"+probe.name+" does not answer",
			len(srv.GrepFrom(mark, probe.forbid)) == 0 &&
				len(printsFrom(a, amark, probe.forbid)) == 0,
			"%s", tail(a, amark))
		// ...and it went to chat, which is where an unclaimed command belongs.
		check("sp/`"+probe.cmd+"` reached the chat fallback",
			len(printsFrom(a, amark, `alpha: `+regexp.QuoteMeta(strings.Fields(probe.cmd)[0]))) > 0,
			"%s", tail(a, amark))
	}
	return nil
}

// ------------------------------------------------------------------ helpers

func firstLine(srv *playtest.Server, re string) string {
	if ls := srv.Grep(re); len(ls) > 0 {
		return strings.TrimSpace(ls[len(ls)-1])
	}
	return "(nothing on the console)"
}

func firstFrom(srv *playtest.Server, mark int, re string) string {
	if ls := srv.GrepFrom(mark, re); len(ls) > 0 {
		return strings.TrimSpace(ls[len(ls)-1])
	}
	return "(nothing on the console)"
}

func pick(a, b string) string {
	if strings.TrimSpace(a) != "" {
		return strings.TrimSpace(a)
	}
	return b
}

// main is a wrapper and run() holds the body, because os.Exit does not run
// deferred calls -- and a scenario that exits with a server up leaves q2proded
// holding the UDP port, so the next run fails to bind and reports "server did
// not spawn", which looks nothing like the cause.
func main() {
	os.Exit(run())
}

func run() int {
	flag.Parse()
	if *q2proded == "" || *lib == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded and -lib")
		return 2
	}

	var list []string
	for _, r := range strings.Split(*rulesets, ",") {
		r = strings.TrimSpace(r)
		if r == "" {
			continue
		}
		if _, ok := colosseum.Find(r); !ok {
			fmt.Fprintf(os.Stderr, "unknown ruleset %q\n", r)
			return 2
		}
		list = append(list, r)
	}
	if len(list) == 0 {
		fmt.Fprintln(os.Stderr, "no rulesets selected")
		return 2
	}

	for i, rs := range list {
		p := *port + i*10
		var err error
		switch {
		case colosseum.IsOSP(rs):
			err = ospPhase(rs, p)
		case rs == "ctf":
			err = ctfPhase(p)
		case rs == "arena":
			err = arenaPhase(p)
		case rs == "sp":
			err = spPhase(p)
		default:
			skip(rs, "no vote system and no ruleset row for it")
		}
		if err != nil {
			fmt.Fprintf(os.Stderr, "\nERROR (%s): %v\n", rs, err)
			report()
			return 2
		}
	}

	report()
	if failed > 0 {
		return 1
	}
	if !*keep {
		os.RemoveAll(*dir)
	}
	return 0
}
