// ctfteams -- does a Capture The Flag bot know whose side it is on, and does
// the offhand hook have a client half?
//
// Two reports from one play test: "bots on the same team attack team mates,
// they seem to be unaware of team membership", and "the offhand grapple with
// bind e +hook is not implemented".  Both are about what the SERVER hands out
// rather than about what it computes, and both need a real client and the real
// brain to be observable at all.
//
// Three checks:
//
//  1. the team skin reaches the wire.  Threewave writes `name\model/ctf_r` into
//     the player configstring block, which is also the only currency the
//     Gladiator brain has for teams: `clientsettings[].skin` is what
//     BotSameTeam() and BotCTFTeam() compare, and the game fills it from the
//     client's USERINFO skin.  So the configstring passing is necessary and not
//     sufficient -- check 2 is the sufficient half.
//
//  2. the brain is told the team skin.  `sv ruleset`'s ctf botplace row counts
//     the bots whose `pers.userinfo` skin carries `ctf_`, which is the string
//     bl_main.c hands the brain and the only currency it has for teams.  This
//     is the sufficient half of check 1, and it fails where check 1 passes.
//
//     It is a diagnostic rather than the world, and the reason is worth
//     stating: under CTF the world barely answers.  T_Damage calls
//     CheckTeamDamage() -- which refuses a team-mate's damage before knockback
//     or an obituary -- for everything except DAMAGE_NO_PROTECTION, so a bot
//     firing at its own side hurts nobody and nothing is broadcast.  That is
//     exactly why the play test reported bots ATTACKING team-mates rather than
//     killing them.  The one thing that gets through is a TELEFRAG, and a
//     telefrag is a spawn collision rather than an act of aim, which Threewave
//     expects (player_die docks the attacker's frag for it) -- so check 3
//     counts telefrags separately and only a non-telefrag same-team kill is a
//     finding about the brain.
//
//  3. no bot kills its own side -- the guard described above.
//
//  4. the client is stuffed `alias +hook` and `alias -hook`.  `+hook` is a
//     console alias and no Quake II client ships one, so a server that never
//     stuffs it has a `bind e "+hook"` that expands to nothing.
//
// Exit 0 all passed, 1 a check failed, 2 the scenario could not run.
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
)

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "retail baseq2 paks")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "Threewave paks")
	lib := flag.String("lib", "", "game library under test")
	glad := flag.String("gladdir", "", "gladiator-bot-restored checkout (the brain)")
	dir := flag.String("dir", "/tmp/q2playtest/ctfteams", "scratch install dir")
	mapname := flag.String("map", "q2ctf1", "map to test on")
	nbots := flag.Int("bots", 4, "how many bots")
	secs := flag.Int("listen", 240, "seconds to watch the fight")
	port := flag.Int("port", 27990, "server port")
	label := flag.String("label", "", "label for the report")
	flag.Parse()

	if *q2 == "" || *lib == "" || *glad == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded, -lib and -gladdir")
		os.Exit(2)
	}
	bad, err := run(*q2, *ref, *ctf, *lib, *glad, *dir, *mapname, *nbots, *secs, *port, *label)
	if err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(2)
	}
	if bad > 0 {
		os.Exit(1)
	}
}

// The baseq2 obituary set, matched on the VERB so the roster does not have to
// be known in advance.  Threewave adds none of its own.
var reObit = regexp.MustCompile(`(?i) (was|tried to|ate|almost dodged|melted|blew|died|should have|saw|couldn't|does a back flip|suicides|cratered|got|rides|caught|didn't see|feels) `)

// "name\model/skin" -- the player configstring block.  The half after the last
// '/' is what a team mod substitutes.
var reSkin = regexp.MustCompile(`^(.+)\\(.*)$`)

func mark(ok bool) string {
	if ok {
		return "PASS"
	}
	return "FAIL"
}

func run(q2, ref, ctf, lib, glad, dir, mapname string, nbots, secs, port int,
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
			"g_ruleset": "ctf", "skill": "1",
			"minimumplayers": "0", "bots_minplayers": "0", "botctfteam": "0",
			"ctf_hook": "1",
		},
	}
	if err := srv.Start(); err != nil {
		return 0, err
	}
	defer srv.Stop()

	// The witness joins first and stays for the whole fight, so that it is a
	// body on a team rather than an audience that arrived afterwards.
	b := playtest.NewBot("witness", "127.0.0.1", port)
	if err := b.Start(60 * time.Second); err != nil {
		return 0, err
	}
	defer b.Disconnect()

	fails := 0

	// ---- 3. the hook's client half ----------------------------------------
	//
	// Checked first because it is stuffed at ClientBegin and needs no bots.
	plus, errPlus := b.WaitStuff(`alias \+hook`, 10*time.Second)
	minus, errMinus := b.WaitStuff(`alias -hook`, 10*time.Second)
	ok := errPlus == nil && errMinus == nil
	if !ok {
		fails++
	}
	fmt.Printf("  %s  offhand hook aliases stuffed   %q / %q\n",
		mark(ok), strings.TrimSpace(plus), strings.TrimSpace(minus))

	for i := 0; i < nbots; i++ {
		srv.Console("sv addrandom")
		time.Sleep(600 * time.Millisecond)
	}
	// give every bot time to connect, load and be assigned a team
	time.Sleep(20 * time.Second)

	// ---- 1. the team skin on the wire -------------------------------------
	//
	// Read from the configstrings rather than from a fixed CS_PLAYERSKINS
	// index: which block the server used depends on whether protocol
	// extensions were negotiated, and the shape of the value identifies it.
	team := map[string]string{}
	for _, v := range b.ConfigStrings() {
		m := reSkin.FindStringSubmatch(playtest.Decode(v))
		if m == nil || m[1] == "" {
			continue
		}
		switch {
		case strings.Contains(m[2], "ctf_r"):
			team[m[1]] = "RED"
		case strings.Contains(m[2], "ctf_b"):
			team[m[1]] = "BLUE"
		}
	}
	ok = len(team) >= nbots
	if !ok {
		fails++
	}
	fmt.Printf("  %s  team skins on the wire         %d of %d bot(s) wear ctf_r/ctf_b\n",
		mark(ok), len(team), nbots)
	names := make([]string, 0, len(team))
	for n := range team {
		names = append(names, n)
	}
	sort.Slice(names, func(i, j int) bool { return len(names[i]) > len(names[j]) })
	for _, n := range names {
		fmt.Printf("  ..      %-20s %s\n", n, team[n])
	}
	if len(team) < 2 {
		return fails, fmt.Errorf("fewer than two clients are on a team; nothing to check")
	}

	// ---- 2. what the brain was told ---------------------------------------
	srv.Console("sv ruleset")
	line, err := srv.WaitLog(`botplace\s+ctf`, 10*time.Second)
	if err != nil {
		return fails, fmt.Errorf("sv ruleset printed no ctf botplace row: %w", err)
	}
	time.Sleep(400 * time.Millisecond)
	m := regexp.MustCompile(`teamskin=(\d+)`).FindStringSubmatch(line)
	got := -1
	if m != nil {
		fmt.Sscanf(m[1], "%d", &got)
	}
	ok = got == nbots
	if !ok {
		fails++
	}
	fmt.Printf("  %s  team skin reaches the brain     %s\n", mark(ok), strings.TrimSpace(line))

	fmt.Printf("  ..    watching %d bot(s) for %ds\n", nbots, secs)
	time.Sleep(time.Duration(secs) * time.Second)

	// ---- 2. no bot kills its own side -------------------------------------
	kills, sameTeam, sameTeamFrag := 0, 0, 0
	var report []string
	// the MOD_TELEFRAG obituary; a spawn collision, not the brain choosing a
	// target, and Threewave has a rule for it in player_die
	reTelefrag := regexp.MustCompile(`tried to invade .*personal space`)
	for _, l := range srv.Log() {
		l = strings.TrimSpace(playtest.Decode(l))
		if !reObit.MatchString(l) {
			continue
		}
		var involved []string
		rest := l
		for _, n := range names {
			if strings.Contains(rest, n) {
				involved = append(involved, n)
				rest = strings.Replace(rest, n, "", 1)
			}
		}
		if len(involved) < 2 {
			continue // a suicide, or somebody not on a team
		}
		kills++
		one := true
		for _, n := range involved[1:] {
			if team[n] != team[involved[0]] {
				one = false
			}
		}
		if one {
			if reTelefrag.MatchString(l) {
				sameTeamFrag++
				report = append(report, fmt.Sprintf("  ..    same-team TELEFRAG (a spawn collision, not the brain)   %s   [%s]", l, team[involved[0]]))
				continue
			}
			sameTeam++
			report = append(report, fmt.Sprintf("  FAIL  same-team kill   %s   [%s]", l, team[involved[0]]))
		}
	}
	for _, r := range report {
		fmt.Println(r)
	}
	if kills == 0 {
		return fails, fmt.Errorf("nobody killed anybody in %ds -- the team-fire check never ran", secs)
	}
	if sameTeam > 0 {
		fails++
	}
	fmt.Printf("  %s  no bot shot its own side       %d kill(s) between two named players, "+
		"%d same-team, %d of those telefrags (CheckTeamDamage blocks everything else)\n",
		mark(sameTeam == 0), kills, sameTeam+sameTeamFrag, sameTeamFrag)

	return fails, nil
}
