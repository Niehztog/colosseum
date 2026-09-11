// ra2observer -- what a Rocket Arena client can see and do once it stops
// fighting.
//
// Four checks, all on a pickup arena, because a pickup arena is the one that
// has no `misc_teleporter_dest` waiting room: arena_init() marks it `active`
// for good, and move_to_arena() answers that by making every observer in it a
// free-flying MOVETYPE_NOCLIP body.  That is the state the checks are about.
//
//  1. score-while-fighting -- `score` must both send a layout AND set the
//     STAT_LAYOUTS bit that tells the client to draw it.  A mod with two
//     scoreboard fields (`showscores` and RA2's three-state `scoremode`) can
//     easily compose the board off one and gate the bit off the other, which
//     sends a scoreboard nobody sees.
//
//  2. score-while-observing -- the same, after a death.  RA2 reopens its
//     observer menu on every placement, so a `score` that is spent closing a
//     menu is a `score` an observer can never use.  RA2's own menu lives in
//     CS_STATUSBAR, not in the layout channel, so the two do not contend.
//
//  3. the observer's view -- PMF_TIME_TELEPORT must not be set on a client the
//     server is running as PM_SPECTATOR.  Pmove() clears that flag from the
//     `pm_time` countdown, and the countdown sits below the PM_SPECTATOR
//     early-out; PM_ClampAngles(), which runs above it, answers the flag by
//     pinning PITCH and ROLL to zero.  Set it on a spectator and the player can
//     turn left and right and never look up or down, for the rest of the map.
//
//  4. the observer's controls -- ATTACK must cycle the observer MODE, because
//     jump/crouch only cycles the tracked player inside the two camera modes.
//     Without the first, an observer is pinned in whichever mode placement left
//     it in and can never watch a team-mate.
//
// Exit 0 all checks passed, 1 a check failed, 2 the scenario could not run.
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
	"q2playtest/ra2"
)

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "", "read-only reference install (RA2 paks + arena.cfg)")
	lib := flag.String("lib", "", "game library under test")
	dir := flag.String("dir", "/tmp/q2playtest/ra2observer", "scratch install dir")
	mapname := flag.String("map", "ra2map7", "map to test on")
	arena := flag.Int("arena", 6, "the map's pickup arena")
	port := flag.Int("port", 27960, "server port")
	label := flag.String("label", "", "label for the report")
	flag.Parse()

	if *q2 == "" || *ref == "" || *lib == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded, -ref and -lib")
		os.Exit(2)
	}

	if err := run(*q2, *ref, *lib, *dir, *mapname, *arena, *port, *label); err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	if failed > 0 {
		os.Exit(1)
	}
}

var failed int

func check(name string, ok bool, detail string) {
	if ok {
		fmt.Printf("  PASS  %-28s %s\n", name, detail)
		return
	}
	failed++
	fmt.Printf("  FAIL  %-28s %s\n", name, detail)
}

func run(q2, ref, lib, dir, mapname string, arena, port int, label string) error {
	if label != "" {
		fmt.Printf("== %s ==\n", label)
	}
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return err
	}
	if err := playtest.Install(dir, "arena", ref, lib); err != nil {
		return err
	}

	srv := &playtest.Server{
		Binary: q2, Dir: dir, Game: "arena", Map: mapname, Port: port,
		LogPath: filepath.Join(dir, "server.log"),
		Cvars: map[string]string{
			"g_ruleset":      "arena",
			"arenacfg":       "arena.cfg",
			"bots":           "0",
			"minimumplayers": "0",
			"admincode":      "0",
		},
	}
	if err := srv.Start(); err != nil {
		return err
	}
	defer srv.Stop()

	// Two on red, one on blue: the round has to survive red1's suicide, or the
	// round-end placement runs SetObserverMode() again and clears the very flag
	// check 3 is looking for.
	seats := []struct {
		name string
		side string
	}{
		{"red1", "Red"}, {"red2", "Red"}, {"blue1", "Blue"},
	}
	var bots []*playtest.Bot
	for _, s := range seats {
		b := playtest.NewBot(s.name, "127.0.0.1", port)
		if err := b.Start(30 * time.Second); err != nil {
			return err
		}
		defer b.Disconnect()
		if err := ra2.JoinTeam(b, ra2.PickupTeam(arena, s.side)); err != nil {
			return fmt.Errorf("%s: %w", s.name, err)
		}
		bots = append(bots, b)
	}
	subject := bots[0]

	// warmup (15s) then countdown (5s) before anyone is a fighter
	if err := waitFor(60*time.Second, func() bool {
		for _, b := range bots {
			if b.Spectating() {
				return false
			}
		}
		return true
	}); err != nil {
		return fmt.Errorf("round never started: %w", err)
	}
	fmt.Println("  ..    round is live, all three are fighters")

	// ---- 1. score while fighting -------------------------------------------
	subject.Cmd("score")
	subject.WaitFrames(10, 5*time.Second)
	lay, layErr := subject.WaitLayout(`.`, 3*time.Second)
	bit := subject.Stat(playtest.StatLayouts) & playtest.LayoutsLayout
	check("score/fighting: layout sent", layErr == nil, trunc(lay))
	check("score/fighting: layout drawn", bit != 0,
		fmt.Sprintf("STAT_LAYOUTS=%d", subject.Stat(playtest.StatLayouts)))

	// ---- 2 and 3 both need the subject dead and observing -------------------
	subject.Cmd("kill")
	if err := waitFor(20*time.Second, subject.Spectating); err != nil {
		return fmt.Errorf("subject never became an observer: %w", err)
	}
	// let the placement settle, and give the pm_time countdown far longer than
	// the 112ms it would need if it were running at all
	subject.WaitFrames(20, 10*time.Second)

	fl := subject.PMFlags()
	check("observer: view not pinned", fl&playtest.PMFTimeTeleport == 0,
		fmt.Sprintf("pm_type=%d pm_flags=0x%02x pm_time=%d", subject.PMType(), fl, subject.PMTime()))

	// The observer menu is up -- move_to_arena() reopens it on every placement.
	subject.Cmd("score")
	subject.WaitFrames(10, 5*time.Second)
	bit = subject.Stat(playtest.StatLayouts) & playtest.LayoutsLayout
	check("score/observing: layout drawn", bit != 0,
		fmt.Sprintf("STAT_LAYOUTS=%d", subject.Stat(playtest.StatLayouts)))

	// ---- 4. the observer can change what it is watching --------------------
	//
	// RA2 has two observer inputs: ATTACK cycles the MODE (normal, free-flying,
	// trackcam, eyecam) and jump/crouch cycles WHO, but only inside the two
	// camera modes.  Without the first there is no way to reach the second, so
	// an observer can never watch a team-mate.  ChangeOMode announces itself,
	// and track_change announces the subject, so both halves are readable.
	subject.Press(playtest.ButtonAttack, 400*time.Millisecond)
	sw, swErr := subject.WaitPrint(`Switched Observer Mode to`, 5*time.Second)
	check("observer: mode cycles", swErr == nil, sw)

	// Free-flying -> trackcam is one press; SetObserverMode calls track_next
	// itself on the way in, so the subject is already named.
	for i := 0; i < 3 && swErr == nil; i++ {
		if _, err := subject.WaitPrint(`Tracking `, 2*time.Second); err == nil {
			break
		}
		subject.Press(playtest.ButtonAttack, 400*time.Millisecond)
	}
	tr, trErr := subject.WaitPrint(`Tracking `, 3*time.Second)
	check("observer: can watch a player", trErr == nil, tr)
	// the other two must still be fighting -- otherwise check 3 sampled the
	// round-end placement rather than the death placement
	stillFighting := !bots[1].Spectating() || !bots[2].Spectating()
	check("round outlived the suicide", stillFighting,
		fmt.Sprintf("red2 spectating=%v blue1 spectating=%v",
			bots[1].Spectating(), bots[2].Spectating()))

	// ---- 5. a death does not litter the arena with a weapon -----------------
	//
	// RA2 does not drop the dead player's weapon: its own TossClientWeapon is
	// `static q_unused` and the reconstruction's note against the shipped DLL
	// is "no real counterpart -- confirmed dead code".  An arena frees every
	// pickup item on the map, so a dropped weapon is an item in a ruleset that
	// has none.  Nothing on the wire names an item, so this is the edict count
	// either side of a death: the number to compare is the same scenario's
	// number on another build.
	before := census(srv)

	// ---- 6. wiping a team ENDS the round ------------------------------------
	//
	// Blue is one client; killing it leaves red2 as the only fighter and
	// fight_done() must say so.  It reads `takedamage == DAMAGE_AIM &&
	// deadflag == DEAD_NO` as "still in the fight", and PutClientInServer
	// clears deadflag and hands out takedamage on every respawn -- so a client
	// respawned as an observer counts as a living fighter on a team it is no
	// longer playing for, and the round sits in ASTATE_FIGHTING for the rest of
	// the map.  RA2 spawns an arena client DAMAGE_NO for exactly this reason.
	//
	// The end is announced twice: a centerprint with the result and a print
	// with the running score.  Either is proof the state machine moved.
	if !bots[2].Spectating() {
		bots[2].Cmd("kill")
	}
	won, wonErr := bots[1].WaitCenter(`won the (round|match)`, 30*time.Second)
	check("wiping a team ends the round", wonErr == nil, strings.TrimSpace(won))

	after := census(srv)
	// The first sample is taken after check 2's suicide and before this one's,
	// so it already carries whatever that death left in the world.  A dropped
	// weapon is one edict: the number to compare is another build's, at the
	// same point, with the same three clients on the same map.
	fmt.Printf("  ..    edicts in use %d after one death, %d after the wipe\n",
		before, after)

	return nil
}

// census asks the mod's own diagnostic for the edict count and waits for the
// block to finish printing -- the world line is not the last one.
func census(srv *playtest.Server) int {
	n := srv.Len()
	if err := srv.Console("sv ruleset"); err != nil {
		return -1
	}
	if _, err := srv.WaitLog(`^world\s+frame`, 5*time.Second); err != nil {
		return -1
	}
	time.Sleep(400 * time.Millisecond)
	st, err := colosseum.ParseRuleset(strings.Join(srv.Log()[n:], "\n"))
	if err != nil {
		return -1
	}
	return st.Edicts
}

func trunc(s string) string {
	s = playtest.Decode(s)
	if len(s) > 60 {
		return s[:60] + "..."
	}
	return s
}

func waitFor(timeout time.Duration, cond func() bool) error {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if cond() {
			return nil
		}
		time.Sleep(100 * time.Millisecond)
	}
	return fmt.Errorf("timed out after %s", timeout)
}
