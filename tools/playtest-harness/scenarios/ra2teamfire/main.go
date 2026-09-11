// ra2teamfire -- can a Rocket Arena bot kill somebody on its own side?
//
// The report this answers is a play test's: "bots on my team were able to
// injure and kill me".  Everything about that sentence needs the real brain to
// be in the game -- a headless client cannot decide to shoot a team-mate -- so
// this seats one client on a pickup team, fills the arena with bots, and reads
// the two things the server tells everybody: who joined which team, and who
// killed whom.
//
// The assertion is that those two never cross.  RA2 has no friendly fire: its
// `healthprotect` defaults to 1, which means nobody on your side takes health
// off you, so an obituary naming two members of one team is a defect wherever
// it comes from -- the damage rules, or a brain that thinks a team-mate is a
// target.
//
// Exit 0 no same-team kill, 1 at least one, 2 the scenario could not run.
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
	"q2playtest/ra2"
)

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "retail baseq2 paks")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "Threewave paks")
	lib := flag.String("lib", "", "game library under test")
	glad := flag.String("gladdir", "", "gladiator-bot-restored checkout (the brain)")
	dir := flag.String("dir", "/tmp/q2playtest/ra2teamfire", "scratch install dir")
	mapname := flag.String("map", "q2dm1", "map to test on")
	arena := flag.Int("arena", 1, "the pickup arena to join")
	nbots := flag.Int("bots", 4, "how many bots")
	secs := flag.Int("listen", 240, "seconds to watch the fight")
	port := flag.Int("port", 27980, "server port")
	label := flag.String("label", "", "label for the report")
	flag.Parse()

	if *q2 == "" || *lib == "" || *glad == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded, -lib and -gladdir")
		os.Exit(2)
	}
	bad, err := run(*q2, *ref, *ctf, *lib, *glad, *dir, *mapname, *arena, *nbots,
		*secs, *port, *label)
	if err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	if bad > 0 {
		os.Exit(1)
	}
}

// "Bob has been added to team 3 (#1 Pickup Red)" -- add_to_team's own print,
// which is how a client learns the roster.  There is no other channel: RA2's
// scoreboard is a layout of names and the team menu is a statusbar.
var reJoin = regexp.MustCompile(`^(.+?) has been added to team \d+ \((.+)\)`)
var reLeave = regexp.MustCompile(`^(.+?) has been removed from team \d+ \(`)

// The baseq2 obituary set, matched on the VERB so the roster does not have to
// be known in advance.
var reObit = regexp.MustCompile(`(?i) (was|tried to|ate|almost dodged|melted|blew|died|should have|saw|couldn't|does a back flip|suicides|cratered|got|rides|caught|didn't see|feels) `)

func run(q2, ref, ctf, lib, glad, dir, mapname string, arena, nbots, secs, port int,
	label string) (int, error) {

	if label != "" {
		fmt.Printf("== %s ==\n", label)
	}
	os.RemoveAll(dir)
	if err := colosseum.Install(dir, ref, ctf, lib); err != nil {
		return 0, err
	}
	if err := colosseum.InstallBrain(dir, glad); err != nil {
		return 0, err
	}

	srv := &playtest.Server{
		Binary: q2, Dir: dir, Game: "colosseum", Map: mapname, Port: port,
		MaxClients: 12, LogPath: filepath.Join(dir, "server.log"),
		Cvars: map[string]string{
			"g_ruleset": "arena", "skill": "1", "admincode": "0",
			"minimumplayers": "0", "bots_minplayers": "0", "arena": "0",
		},
	}
	if err := srv.Start(); err != nil {
		return 0, err
	}
	defer srv.Stop()

	// The witness joins FIRST, so that it is a body in the arena for the whole
	// fight rather than an audience that arrived after the teams were made --
	// the report is about a bot shooting the person standing next to it.
	b := playtest.NewBot("witness", "127.0.0.1", port)
	if err := b.Start(60 * time.Second); err != nil {
		return 0, err
	}
	defer b.Disconnect()
	if err := ra2.JoinTeam(b, ra2.PickupTeam(arena, "Red")); err != nil {
		return 0, fmt.Errorf("witness could not join: %w", err)
	}

	for i := 0; i < nbots; i++ {
		srv.Console("sv addrandom")
		time.Sleep(400 * time.Millisecond)
	}
	fmt.Printf("  ..    witness is on %s with %d bot(s); watching for %ds\n",
		ra2.PickupTeam(arena, "Red"), nbots, secs)
	time.Sleep(time.Duration(secs) * time.Second)

	// ---- roster and kills, in the order the server said them ---------------
	//
	// Read off the server console rather than out of the client, because RA2
	// tears the teams down and rebuilds them between rounds: who was on whose
	// side is a fact with a TIME, and only a sequential read has that.  The
	// client is in the game to be shot at, not to be the instrument.
	team := map[string]string{}
	seen := map[string]string{}
	kills, sameTeam := 0, 0
	var report []string

	names := func() []string {
		out := make([]string, 0, len(team))
		for n := range team {
			out = append(out, n)
		}
		// longest first, so one player's name is not found inside another's
		sort.Slice(out, func(i, j int) bool { return len(out[i]) > len(out[j]) })
		return out
	}

	for _, l := range srv.Log() {
		l = strings.TrimSpace(playtest.Decode(l))
		if m := reJoin.FindStringSubmatch(l); m != nil {
			team[m[1]] = m[2]
			seen[m[1]] = m[2]
			continue
		}
		if m := reLeave.FindStringSubmatch(l); m != nil {
			delete(team, m[1])
			continue
		}
		if !reObit.MatchString(l) {
			continue
		}
		var involved []string
		rest := l
		for _, n := range names() {
			if strings.Contains(rest, n) {
				involved = append(involved, n)
				rest = strings.Replace(rest, n, "", 1)
			}
		}
		if len(involved) < 2 {
			continue // a suicide, or somebody who is between teams
		}
		kills++
		one := true
		for _, n := range involved[1:] {
			if team[n] != team[involved[0]] {
				one = false
			}
		}
		if one {
			sameTeam++
			report = append(report, fmt.Sprintf("  FAIL  same-team kill            %s   [%s]", l, team[involved[0]]))
		}
	}

	fmt.Printf("  ..    roster seen (%d):\n", len(seen))
	for n, t := range seen {
		fmt.Printf("  ..      %-20s %s\n", n, t)
	}
	if len(seen) < 2 {
		return 0, fmt.Errorf("the server never announced a roster; nothing to check")
	}
	for _, r := range report {
		fmt.Println(r)
	}

	fmt.Printf("  ..    %d kill(s) between two named players\n", kills)
	if kills == 0 {
		return 0, fmt.Errorf("nobody killed anybody in %ds -- the check never ran", secs)
	}
	if sameTeam == 0 {
		fmt.Printf("  PASS  no bot killed its own side  %d cross-team kill(s), 0 same-team\n", kills)
	}
	return sameTeam, nil
}
