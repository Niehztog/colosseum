// osprunes -- does holding a rune do what the rune says, and does holding
// something ELSE not?
//
// WHY THIS SCENARIO EXISTS.  The five OSP Tourney runes are carried in
// playerstate stats, one slot each, and Colosseum resolves those slots through a
// per-ruleset map rather than a #define.  That map was correct, and
// every check that read it passed -- the boot matrix, the 81-row battery, the
// build audits, `sv slots` itself -- while the feature did nothing at all,
// because the code around the map disagreed with it in two directions at once
// (the rune slot map):
//
//	the WRITE   OSP_Pickup_Rune called G_SetStat(other, item->quantity, 1), and
//	            `quantity` still held the donor's literal 22 where the id had
//	            become an enum member worth 28.  statslot_t 22 is another
//	            ruleset's stat, unmapped in the OSP column, so the grant was a
//	            silent no-op: picking up a rune granted nothing.
//	the READ    the gameplay code indexed ps.stats[] with the id instead of the
//	            slot, reading ordinals 28..32 where the runes lived at 22..26.
//	            Ordinals 29 and 30 are the OSP column's own second-powerup pair,
//	            so a player holding an invulnerability read as holding the
//	            STRENGTH and HASTE runes -- doubled damage, haste fire rate.
//
// THE LESSON THIS SCENARIO ENCODES.  "Ask the world, not the diagnostic" is in
// this skill's notes already; this is the sharpest instance of it so far,
// because the diagnostic was not merely uninformative, it was *correct*.
// `sv slots` reported 22..26 and the map really did say 22..26.  What no
// diagnostic could report is that nothing ever wrote there.  Only the wire
// settles that, which is what Bot.Stat exists for.
//
// So the assertions come in opposite signs, and the negative one is the half no
// amount of asking the mod could have produced:
//
//	POSITIVE  give a rune -> the slot the mod's own map names for that rune
//	          carries a value, no other rune's slot moves, and %r names it.
//	NEGATIVE  hold two powerups and NO rune -> every rune slot stays zero and
//	          %r says "no runes".  This is the cross-talk check.  It failed
//	          before the fix and it is what "the runes work" actually means.
//
// Slot numbers are never hardcoded: they come out of `sv slots` BY NAME, so the
// scenario keeps working if the map renumbers and fails if the map and the wire
// disagree -- which is the point.
//
// ---------------------------------------------------------------------------
// TWO THINGS ABOUT TOURNEY THAT SHAPE THE WHOLE SCENARIO, both learned the hard
// way and both worth knowing before writing any OSP-ruleset test:
//
//  1. %r EXPANDS ONLY UNDER `tdm`.  osp_clientcmd.c routes say_team to
//     OSP_sayteam_cmd for TeamPlay and to plain Cmd_Say_f otherwise, and only
//     the former walks the % escapes.  Under any other mode this scenario would
//     assert on an unexpanded literal "%r" and pass for the wrong reason.
//
//  2. NOTHING IS PICKABLE UNTIL THE MATCH IS RUNNING.  Touch_Item returns early
//     for `G_Ruleset() == RULESET_TOURNEY && sync_stat < 4`, and `give` reaches
//     items THROUGH Touch_Item -- it spawns the entity and touches the player
//     with it.  So `give Resist_Rune` during warmup is a silent no-op, and the
//     first version of this scenario read that as the bug it was hunting.
//     sync_stat is 8 (free play) only under `dm` -- which is exactly the
//     mode where %r does not expand.  The two requirements are mutually
//     exclusive during warmup, so the scenario starts a REAL match: two clients,
//     one per team, both ready, then match_countdown seconds.
//
// A precondition row asserts items are pickable at all (`give Body Armor`, a
// stat every ruleset has) before any rune row runs.  Without it a broken grant
// and an unstarted match look identical, and the scenario would report the wrong
// defect -- which it did, once.
//
// The chat goes to every client whose resp.team matches the sender's, and that
// includes the sender, so %r is read back off the sender's own connection.
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
	dir      = flag.String("dir", "/tmp/osprunes", "scratch install dir")
	port     = flag.Int("port", 27990, "UDP port")
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
	fmt.Printf("  %s %-44s %s\n", tag, name, note)
	return ok
}

// skip records a row that could not run.  Recorded rather than silently
// subtracted, so "12 checks passed" cannot quietly mean "and eight never ran".
func skip(name, why string) {
	total++
	skipped++
	fmt.Printf("  [skip] %-44s %s\n", name, why)
}

// Universal stat slots -- shared.h's own enum, the same in every ruleset and
// every ABI, because slots 0..15 are unclaimable.  These are the
// only stat numbers this file is allowed to write down.
const (
	statHealth    = 1
	statArmor     = 5
	statTimerIcon = 9 // the FIRST powerup timer

	teamA, teamB = "Hometeam", "Visitors" // team_a_name / team_b_name defaults

	// osp_main.c clamps match_countdown UP to 14, so asking for less is asking
	// for 14; the wait below is derived from this rather than guessed.
	matchCountdown = 14

	dfForceRespawn = 1 << 10 // DF_FORCE_RESPAWN, shared.h
)

// The five runes as the game names them: the item to `give`, the stat the map
// must place it in, and the phrase %r must produce.  The stat NAME is the key;
// the number comes from the server.
var runes = []struct {
	item, stat, says string
}{
	{"Resist_Rune", "SID_OSP_RUNE_RESIST", "the RESIST rune"},
	{"Strength_Rune", "SID_OSP_RUNE_STRENGTH", "the STRENGTH rune"},
	{"Haste_Rune", "SID_OSP_RUNE_HASTE", "the HASTE rune"},
	{"Regen_Rune", "SID_OSP_RUNE_REGEN", "the REGEN rune"},
	{"Vampire_Rune", "SID_OSP_RUNE_VAMPIRE", "the VAMPIRE rune"},
}

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

	srv := &playtest.Server{
		Binary: *q2proded, Dir: *dir, Game: "colosseum",
		Map: *mapname, Port: *port, MaxClients: 8,
		Cvars: map[string]string{
			// `tdm` -- OSP's TeamPlay, which spec 1.36 promoted from
			// `match_mode 2` to a ruleset.  It is the only one of
			// the four that expands %r, for the reason noted above.
			"g_ruleset":       "tdm",
			"runes":           "1", // resolves to rune_stat 0x1f, all five in play
			"cheats":          "1",
			"deathmatch":      "1",
			"coop":            "0",
			"match_countdown": fmt.Sprint(matchCountdown),
			"bots":            "0",
			// FLOOD PROTECTION EATS say_team.  Defaults are 4 messages per 4
			// seconds then a 10-second silence, and this scenario sends seven --
			// one per rune plus %r and %t at the end.  Past the fourth the server
			// answers "You can't talk for N more seconds" instead of the chat
			// line, so rows fail intermittently depending on how fast the give
			// loop ran.  It cost one confusing run where three rows failed and
			// the same build passed on retry.  Zero disables it.
			"flood_msgs": "0",
			// DF_FORCE_RESPAWN (bit 10).  THE MATCH START KILLS EVERYONE:
			// health goes 100 -> 0 on the frame OSP announces "Match has
			// started!", and a dead client stays dead until it latches
			// BUTTON_ATTACK -- which a libq2 bot cannot send, because
			// BuildUserCommand emits an empty usercmd every frame.  Without this
			// flag every row below fails on `health < 1` inside Touch_Item and
			// reads exactly like a broken grant, which cost an hour once.
			"dmflags": fmt.Sprint(dfForceRespawn),
		},
		LogPath: *dir + "/server.log",
	}
	if err := srv.Start(); err != nil {
		fmt.Fprintln(os.Stderr, "server:", err)
		os.Exit(2)
	}
	defer srv.Stop()
	time.Sleep(3 * time.Second)

	// DID IT EVEN BOOT?  Reported as a check rather than an early exit with
	// "sv slots never answered", because on the pre-fix library this is the
	// finding: an OSP ruleset with `runes 1` died at map load with
	//
	//     ERROR: Game Error: ED_Alloc: no free edicts
	//
	// OSP_spawnRuneAt incremented r_count[-6] instead of r_count[0..4], so
	// OSP_checkMinRunes never saw its own count rise, kept deciding it needed
	// another rune, and the two tail-called each other until the edict pool ran
	// out.  A scenario whose only symptom for that is "the diagnostic did not
	// answer" is hiding the most important thing it found.
	fmt.Printf("\n##### the server survived loading the map\n")
	if boom := srv.Grep(`Game Error|ERROR:`); len(boom) > 0 {
		check("boot/no game error", false, strings.TrimSpace(boom[0]))
		report()
		return
	}
	check("boot/no game error", true, "")

	// The slot map, from the server, by name.  Every rune slot used below came
	// from here.
	srv.Console("sv slots")
	if _, err := srv.WaitLog(`^statusbar `, 5*time.Second); err != nil {
		check("boot/sv slots answers", false,
			"no `sv slots` block; last log line: %s", lastLog(srv))
		report()
		return
	}
	time.Sleep(400 * time.Millisecond)
	sl, err := colosseum.ParseSlots(strings.Join(srv.Log(), "\n"))
	if err != nil {
		fmt.Fprintln(os.Stderr, "sv slots:", err)
		os.Exit(2)
	}

	fmt.Printf("\n##### the map the server reports (ruleset %s, api %d, reach %d)\n",
		sl.Ruleset, sl.Api, sl.Reach())
	for _, r := range runes {
		slot, ok := sl.ByName[r.stat]
		check("map/"+r.stat, ok && slot >= 16, "slot %d", slot)
	}

	// ---------------------------------------------------------------- a match
	fmt.Printf("\n##### start a TeamPlay match (two clients, one per team, both ready)\n")
	a := playtest.NewBot("runetester", "127.0.0.1", *port)
	if err := a.Start(30 * time.Second); err != nil {
		fmt.Fprintln(os.Stderr, "bot a:", err)
		os.Exit(2)
	}
	defer a.Disconnect()
	b := playtest.NewBot("sparring", "127.0.0.1", *port)
	if err := b.Start(30 * time.Second); err != nil {
		fmt.Fprintln(os.Stderr, "bot b:", err)
		os.Exit(2)
	}
	defer b.Disconnect()
	a.WaitFrames(10, 5*time.Second)

	// RETRIED, because one `team` command is not reliably enough.  Observed:
	// bot b became captain of Visitors while bot a's join produced "You aren't
	// currently on any team" -- and a team with nobody on it never readies, so
	// the match never starts and all twenty rows below fail for a reason that has
	// nothing to do with runes.  Confirm with the mod, resend if it did not take.
	joinedA, joinedB := joinTeam(a, teamA), joinTeam(b, teamB)
	check("match/both joined a team", joinedA && joinedB,
		"%q | %q", lastTeamLine(a), lastTeamLine(b))

	a.Cmd("ready")
	b.Cmd("ready")

	// The countdown, plus slack for the frame the match actually starts on.
	// Polled with a real item rather than slept blind, so a server that starts
	// sooner does not cost the whole wait and one that never starts says so.
	live := false
	deadline := time.Now().Add(time.Duration(matchCountdown+20) * time.Second)
	for time.Now().Before(deadline) && !live {
		a.Cmd("give Body Armor")
		time.Sleep(800 * time.Millisecond)
		live = a.Stat(statArmor) > 0
	}
	// Reported separately from the grant, because "dead" and "the grant is
	// broken" are the two explanations for an unchanged armor stat and only one
	// of them is about the code under test.
	check("match/client is alive", a.Stat(statHealth) > 0,
		"stats[%d]=%d (STAT_HEALTH)", statHealth, a.Stat(statHealth))
	// THE PRECONDITION.  Every rune row below is meaningless if items are not
	// pickable, and a refused grant is indistinguishable from a broken one.
	check("match/items are pickable (sync_stat >= 4)", live,
		"stats[%d]=%d after ready + up to %ds", statArmor, a.Stat(statArmor), matchCountdown+20)

	if !live {
		for _, r := range runes {
			skip("give/"+r.item, "no match, so nothing is pickable")
		}
		skip("crosstalk/%r", "no match, so no powerups either")
		report()
		return
	}

	// ---------------------------------------------------------------- positive
	//
	// One rune at a time: OSP_Pickup_Rune refuses a second while one is held, so
	// each round drops the last before the next.
	fmt.Printf("\n##### give a rune -> the mod's own slot carries it, and %%r names it\n")
	for _, r := range runes {
		slot, ok := sl.ByName[r.stat]
		if !ok {
			skip("give/"+r.item, r.stat+" is not in this ruleset's map")
			continue
		}

		got, err := giveRune(a, r.item, slot)
		// The grant is what the broken write could not do at all: it wrote
		// statslot_t 22, which in the OSP column resolves to nothing.
		check("give/"+r.item+" sets its slot", err == nil,
			"stats[%d]=%d%s", slot, got, errNote(err))

		// ...and nothing else moved.  A write that landed on some OTHER rune's
		// slot would satisfy the row above for that rune, so the negative half
		// belongs here and not only in the cross-talk block.
		stray := ""
		for _, o := range runes {
			if o.stat == r.stat {
				continue
			}
			if os, ok := sl.ByName[o.stat]; ok && a.Stat(os) != 0 {
				stray += fmt.Sprintf(" %s=%d", o.stat, a.Stat(os))
			}
		}
		check("give/"+r.item+" moves no other rune", stray == "",
			"other rune slots%s", orNone(stray))

		// The READ path, in the mod's own words: sayteam_runes tests all five in
		// order and names the first it finds.
		said := sayTeam(a, "%r")
		check("say/"+r.item+" %r names it",
			strings.Contains(said, r.says), "%q", said)

		// SHED IT BY DYING, not by dropping.  `drop` leaves the rune at the
		// player's feet, drop_make_touchable re-arms Touch_Item about a second
		// later, and a client that cannot walk is still standing on it -- so it
		// picks its own rune straight back up.  That showed up as the NEXT rune
		// failing to grant (OSP_Pickup_Rune correctly refuses a second) with
		// "other rune slots SID_OSP_RUNE_RESIST=1" beside it, three rows in a
		// row, on one run in three.  `kill` runs OSP_deadDropRune at the corpse
		// and respawns the client at a spawn point instead.  Cmd_Kill_f ignores a
		// suicide within 5s of a respawn, which is why the wait is generous.
		shed(a, slot)
	}

	// ---------------------------------------------------------------- negative
	//
	// THE CROSS-TALK CHECK, and the reason this scenario is worth its length.
	// The broken reads landed on ordinals 28..32; 29 and 30 are the OSP column's own
	// SID_TIMER2_ICON and SID_TIMER2, the second powerup timer.  A player with
	// two powerups and no rune therefore answered %r with "the STRENGTH rune"
	// and took doubled damage from OSP_runesApplyStrength.
	//
	// TWO powerups, because it is the SECOND timer that shares those ordinals:
	// STAT_TIMER holds the first and SID_TIMER2 the second, so one powerup never
	// reaches the slots that were misread.
	fmt.Printf("\n##### hold two powerups and NO rune -> %%r says so\n")
	// Retried for the same reason the team join is: a give that lands on the
	// frame the client is dead is silently refused by Touch_Item, and then the
	// cross-talk rows below are vacuous rather than wrong.
	for try := 0; try < 6 && a.Stat(statTimerIcon) == 0; try++ {
		for _, c := range []string{
			"give Quad Damage", "give Invulnerability",
			"use Quad Damage", "use Invulnerability",
		} {
			a.Cmd(c)
			time.Sleep(250 * time.Millisecond)
		}
		a.WaitFrames(10, 4*time.Second)
	}

	// Preconditions again, and again because a vacuous pass is the failure mode
	// here: with no second timer running, "no runes" would pass on the broken
	// build too, and the row would be testing nothing.
	check("powerups/first timer is running", a.Stat(statTimerIcon) != 0,
		"stats[%d]=%d (STAT_TIMER_ICON)", statTimerIcon, a.Stat(statTimerIcon))

	t2, hasT2 := sl.ByName["SID_TIMER2"]
	secondLive := false
	if !hasT2 {
		// Legal in general -- on an API=old build ctf drops this pair -- but
		// in the OSP column it is at 30, so absence here is news rather than noise.
		skip("powerups/second timer is running",
			"SID_TIMER2 is not mapped in this ruleset -- nothing to cross-talk with")
	} else {
		v, err := waitNonZero(a, t2, 5*time.Second)
		secondLive = err == nil
		check("powerups/second timer is running", secondLive,
			"stats[%d]=%d (SID_TIMER2)%s", t2, v, errNote(err))
	}

	for _, r := range runes {
		slot, ok := sl.ByName[r.stat]
		if !ok {
			continue
		}
		check("crosstalk/"+r.stat+" stays zero", a.Stat(slot) == 0,
			"stats[%d]=%d", slot, a.Stat(slot))
	}

	said := sayTeam(a, "%r")
	if secondLive {
		check("crosstalk/%r says no runes", strings.Contains(said, "no runes"),
			"%q", said)
	} else {
		skip("crosstalk/%r says no runes",
			fmt.Sprintf("second timer never ran, so %q proves nothing", said))
	}

	// %t is the same body behind a second escape -- two case labels, one
	// implementation -- so it is checked rather than assumed.
	said = sayTeam(a, "%t")
	check("crosstalk/%t agrees with %r", strings.Contains(said, "no runes"),
		"%q", said)

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

// sayTeam sends one say_team and returns the chat line it produced.
//
// Matched on the mod's "(name): text" chat format rather than on the escape's
// output, so a server that expanded nothing still returns a line and the
// assertion fails on the CONTENT instead of timing out with no evidence.
func saidOnce(b *playtest.Bot, escape string) string {
	// Let the frame that granted or dropped the rune land first: the escape is
	// expanded when the command is processed, so a say_team racing a pickup can
	// read the state from before it.  And the wait is generous -- a chat line
	// that has not arrived in a second is not evidence of anything except a busy
	// server, and a short timeout here produced "<no chat line came back>" on
	// roughly one row in fifteen.
	time.Sleep(300 * time.Millisecond)
	before := len(b.Prints())
	b.Cmd("say_team %s", escape)
	re := regexp.MustCompile(`\(` + regexp.QuoteMeta(b.Name) + `\):`)
	deadline := time.Now().Add(8 * time.Second)
	for time.Now().Before(deadline) {
		p := b.Prints()
		for i := len(p) - 1; i >= before; i-- {
			if re.MatchString(p[i]) {
				return strings.TrimSpace(p[i])
			}
		}
		time.Sleep(20 * time.Millisecond)
	}
	return ""
}

// sayTeam sends the escape and returns the chat line, retrying when nothing came
// back at all.  An empty result is "the server did not answer", which is a
// different thing from "the server answered with the wrong rune" -- only the
// second is a finding, so the first is retried rather than reported.
func sayTeam(b *playtest.Bot, escape string) string {
	for try := 0; try < 3; try++ {
		if line := saidOnce(b, escape); line != "" {
			return line
		}
	}
	return "<no chat line came back>"
}

// joinTeam sends `team <name>` until the mod agrees the client is on it.
// giveRune asks for the rune until the slot carries it, and reports the last
// attempt if it never does.
//
// RETRYING IS NOT PAPERING OVER THE BUG IT HUNTS.  A grant that writes the wrong
// stat writes the wrong stat every time, so no number of attempts turns the
// broken build green -- which is checked: on the pre-fix library this loop
// exhausts and the row fails.  What the retry removes is the transient refusal:
// Touch_Item bails on `health < 1`, and a client can be briefly dead again after
// the previous rune was shed, or from respawn telefragging on a small map.  A
// single-shot give failed roughly one row in fifteen that way.
func giveRune(b *playtest.Bot, item string, slot int) (int, error) {
	var err error
	var got int
	for try := 0; try < 6; try++ {
		if b.Stat(statHealth) <= 0 {
			time.Sleep(1 * time.Second)
			continue
		}
		b.Cmd("give %s", item)
		if got, err = b.WaitStat(slot, 1, 2*time.Second); err == nil {
			return got, nil
		}
	}
	return got, err
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

// shed gets rid of the rune the client is holding by dying with it, and waits
// for the client to be alive and rune-less again.  Both halves matter: alive,
// or the next `give` is refused on health; rune-less, or the next pickup is
// refused because one rune is already held.
func shed(b *playtest.Bot, slot int) {
	deadline := time.Now().Add(20 * time.Second)
	for time.Now().Before(deadline) {
		b.Cmd("kill")
		time.Sleep(1500 * time.Millisecond)
		if b.Stat(slot) == 0 && b.Stat(statHealth) > 0 {
			return
		}
	}
}

// onTeam asks the mod, because team membership has no playerstate spelling:
// `team` with no argument prints the caller's own team.
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

func waitNonZero(b *playtest.Bot, slot int, timeout time.Duration) (int, error) {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if v := b.Stat(slot); v != 0 {
			return v, nil
		}
		time.Sleep(20 * time.Millisecond)
	}
	return b.Stat(slot), fmt.Errorf("stats[%d] never became non-zero", slot)
}

func lastLog(srv *playtest.Server) string {
	l := srv.Log()
	if len(l) == 0 {
		return "<empty>"
	}
	return strings.TrimSpace(l[len(l)-1])
}

func errNote(err error) string {
	if err == nil {
		return ""
	}
	return " -- " + err.Error()
}

func orNone(s string) string {
	if s == "" {
		return " all zero"
	}
	return s
}
