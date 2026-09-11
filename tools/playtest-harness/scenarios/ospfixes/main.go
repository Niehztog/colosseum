// ospfixes -- the battery for the OSP Tourney fixes ported from Colosseum.
//
// It runs against BOTH engine families from one binary, because the point of
// half these rows is that the repair is correct on an engine that predates
// Q2PRO's game ABI as well as on Q2PRO itself:
//
//	-engine q2pro   q2proded, library built with the default API=new
//	-engine yq2     Yamagi Quake II's q2ded, library built with API=old
//
// The two differ in three things and nothing else: the cvar that sets the
// listen port, whether `homedir` exists, and the filename the engine looks the
// game library up under.  Everything below the process launch -- the clients,
// the votes, the log and the stats file -- is the same test.
//
// Rows, and what each is a witness for:
//
//  1. CONFIG VOTE.  `OSP_loadMaps` prints `Loading maps from` exactly
//     once per call, and the `manual_map == 2` arm calls it.  An arm that
//     changes the level once prints it once; an arm that spins because the
//     engine refused its command and it never cleared the intermission flags
//     prints it every frame.  So the COUNT is the mechanism, not the outcome,
//     and the level change is asserted beside it.
//
//  2. team_maxplayers.  Re-getting a cvar with CVAR_NOSET is
//     permanent -- both engines OR new flags onto an existing cvar -- so the
//     test is simply whether the operator can still write it after a 1v1 map.
//
//  3. CONFIG NAME CASE.  A mixed-case config in serverconfigs.txt,
//     voted for in lower case.  The witness is the name the server ANNOUNCES
//     and, behind it, whether the config's own settings actually took.
//
//  4. INTERMISSION LENGTH (the timer-unit class).  `nextlevel_default` is
//     documented in seconds and was compared against a frame count, so it ran
//     at a tenth of its setting.  Measured as wall time between the timelimit
//     and the next map.
//
//  5. RUNE CACHE.  The stats log's `game_init` event carries
//     `"runes":<rune_stat>`, so the cache is readable from outside without a
//     diagnostic command.  A config vote that changes `runes_enable` must move
//     it.
//
//  6. QUAD EXPIRY.  Holding ONLY Invulnerability and dying must not
//     write an `item_expire` naming "Quad".//
//  NOT COVERED, and recorded rather than implied: the SECOND site, the
//  empty-server default-config reset arm in G_RunFrame.  Reaching it needs
//  `connected_clients - botglobals.numbots == 0` at the moment the arm is
//  evaluated, and `OSP_serverbotsRemove()` runs at the top of that same block
//  and takes numbots to zero while `connected_clients` keeps counting the bots
//  it just removed -- so any server with bots on it fails the gate.  Without
//  bots nothing sets `level.exitintermission` in the first place.  It also
//  needs `__current_config` to differ from "default", which `OSP_gameInit`
//  force-sets at osp_main.c:438, so only a passed config vote can arrange it.
//  The arm's repair is the same two lines as row 1's and is carried by
//  inspection; a scenario for it would have to drive that whole sequence.

package main

import (
	"bufio"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"

	"q2playtest/playtest"
)

var (
	engine = flag.String("engine", "q2pro", "q2pro | yq2")
	binary = flag.String("bin", "", "dedicated server binary")
	ref    = flag.String("ref", "../../../yquake2/release_", "read-only reference install")
	lib    = flag.String("lib", "", "game library to test")
	dir    = flag.String("dir", "", "scratch dir")
	port   = flag.Int("port", 27930, "base UDP port")
	label  = flag.String("label", "run", "label for output")
	only   = flag.String("only", "", "run only rows whose name contains this")
	keep   = flag.Bool("keep", false, "keep the scratch dir")
)

var pass, fail int

func ck(name string, ok bool, detail string) {
	if ok {
		pass++
		fmt.Printf("  PASS  %-46s %s\n", name, detail)
	} else {
		fail++
		fmt.Printf("  FAIL  %-46s %s\n", name, detail)
	}
}

// ---------------------------------------------------------------- server ---

// srv is a dedicated server of either family.  playtest.Server hardcodes
// q2pro's `net_port` and `homedir`, which Yamagi does not have, so this is its
// own small launcher rather than a fork of that one.
type srv struct {
	cmd   *exec.Cmd
	stdin io.WriteCloser
	mu    sync.Mutex
	lines []string
	dir   string
}

func startServer(gdir string, p int, cvars map[string]string, mapname string) (*srv, error) {
	args := []string{"+set", "basedir", gdir, "+set", "game", "tourney",
		"+set", "dedicated", "1", "+set", "deathmatch", "1"}
	if *engine == "q2pro" {
		args = append(args, "+set", "homedir", gdir,
			"+set", "net_port", strconv.Itoa(p), "+set", "sv_iplimit", "0")
	} else {
		args = append(args, "+set", "port", strconv.Itoa(p))
	}
	for k, v := range cvars {
		args = append(args, "+set", k, v)
	}
	args = append(args, "+map", mapname)

	s := &srv{dir: gdir}
	// Yamagi block-buffers stdout when it is not a tty, so without stdbuf
	// the console arrives only at exit and every wait below times out.
	bin, binArgs := *binary, args
	if _, err := exec.LookPath("stdbuf"); err == nil {
		bin, binArgs = "stdbuf", append([]string{"-oL", "-eL", *binary}, args...)
	}
	s.cmd = exec.Command(bin, binArgs...)
	s.cmd.Dir = gdir
	out, err := s.cmd.StdoutPipe()
	if err != nil {
		return nil, err
	}
	s.cmd.Stderr = s.cmd.Stdout
	if s.stdin, err = s.cmd.StdinPipe(); err != nil {
		return nil, err
	}
	if err := s.cmd.Start(); err != nil {
		return nil, err
	}
	logf, _ := os.Create(filepath.Join(gdir, "server.log"))
	go func() {
		sc := bufio.NewScanner(out)
		sc.Buffer(make([]byte, 64*1024), 1024*1024)
		for sc.Scan() {
			l := sc.Text()
			s.mu.Lock()
			s.lines = append(s.lines, l)
			s.mu.Unlock()
			if logf != nil {
				fmt.Fprintln(logf, l)
			}
		}
	}()
	// Both engines print this line from SV_SpawnServer.
	if !s.waitLog(`SpawnServer: `+regexp.QuoteMeta(mapname), 30*time.Second) &&
		!s.waitLog(`Loading map: `+regexp.QuoteMeta(mapname), 5*time.Second) {
		s.stop()
		return nil, fmt.Errorf("server never spawned %s", mapname)
	}
	return s, nil
}

func (s *srv) stop() {
	if s.stdin != nil {
		s.stdin.Close()
	}
	if s.cmd != nil && s.cmd.Process != nil {
		s.cmd.Process.Kill()
		s.cmd.Wait()
	}
}

func (s *srv) console(f string, a ...any) {
	fmt.Fprintf(s.stdin, f+"\n", a...)
}

func (s *srv) log() []string {
	s.mu.Lock()
	defer s.mu.Unlock()
	return append([]string(nil), s.lines...)
}

func (s *srv) count(re string) int {
	r := regexp.MustCompile(re)
	n := 0
	for _, l := range s.log() {
		if r.MatchString(l) {
			n++
		}
	}
	return n
}

func (s *srv) grep(re string) []string {
	r := regexp.MustCompile(re)
	var out []string
	for _, l := range s.log() {
		if r.MatchString(l) {
			out = append(out, l)
		}
	}
	return out
}

func (s *srv) waitLog(re string, d time.Duration) bool {
	deadline := time.Now().Add(d)
	for time.Now().Before(deadline) {
		if len(s.grep(re)) > 0 {
			return true
		}
		time.Sleep(50 * time.Millisecond)
	}
	return false
}

// waitCount waits until `re` has been seen at least n times.
func (s *srv) waitCount(re string, n int, d time.Duration) bool {
	deadline := time.Now().Add(d)
	for time.Now().Before(deadline) {
		if s.count(re) >= n {
			return true
		}
		time.Sleep(50 * time.Millisecond)
	}
	return false
}

// ----------------------------------------------------------------- stage ---

// stage builds a throwaway game dir: the reference install's paks and configs
// by symlink, the library under test copied in under the name this engine
// looks for.  Nothing is ever written into the reference.
func stage(d string, extra map[string]string) error {
	os.RemoveAll(d)
	for _, sub := range []string{"baseq2", "tourney"} {
		if err := os.MkdirAll(filepath.Join(d, sub), 0o755); err != nil {
			return err
		}
	}
	for _, p := range []string{"pak0.pak", "pak1.pak", "pak2.pak"} {
		os.Symlink(filepath.Join(*ref, "baseq2", p), filepath.Join(d, "baseq2", p))
	}
	ents, err := os.ReadDir(filepath.Join(*ref, "tourney"))
	if err != nil {
		return err
	}
	for _, e := range ents {
		n := e.Name()
		if strings.HasPrefix(n, "game") || strings.Contains(n, "oracle") ||
			strings.Contains(n, "v275-real") {
			continue
		}
		os.Symlink(filepath.Join(*ref, "tourney", n), filepath.Join(d, "tourney", n))
	}
	// q2pro looks for `game<cpu>`, and the caller has already built that
	// name, so the library is installed under the one it arrived with.
	// yquake2 names it `game.so` whatever the CPU, which is why the two
	// arms differ rather than sharing one spelling.
	name := filepath.Base(*lib)
	if *engine != "q2pro" {
		name = "game.so"
	}
	src, err := os.ReadFile(*lib)
	if err != nil {
		return err
	}
	if err := os.WriteFile(filepath.Join(d, "tourney", name), src, 0o755); err != nil {
		return err
	}
	// Files the test writes itself replace the symlink.
	for n, body := range extra {
		p := filepath.Join(d, "tourney", n)
		os.Remove(p)
		if err := os.WriteFile(p, []byte(body), 0o644); err != nil {
			return err
		}
	}
	return nil
}

// --------------------------------------------------------------- stats -----

type statEvent map[string]any

func statsEvents(d string) []statEvent {
	f, err := os.Open(filepath.Join(d, "tourney", "osptourney.jsonl"))
	if err != nil {
		return nil
	}
	defer f.Close()
	var out []statEvent
	sc := bufio.NewScanner(f)
	sc.Buffer(make([]byte, 1<<20), 1<<20)
	for sc.Scan() {
		var e statEvent
		if json.Unmarshal(sc.Bytes(), &e) == nil {
			out = append(out, e)
		}
	}
	return out
}

func lastEvent(d, kind string) statEvent {
	var last statEvent
	for _, e := range statsEvents(d) {
		if e["event"] == kind {
			last = e
		}
	}
	return last
}

// ------------------------------------------------------------- choreography ---

// enter puts a client in the game.  A tourney client CONNECTS as an observer
// and enters through a command, so without this `give` is silently refused and
// the client has no body -- which reads exactly like a broken grant.
func enter(s *srv, b *playtest.Bot) bool {
	for i := 0; i < 6; i++ {
		b.Cmd("join")
		deadline := time.Now().Add(2 * time.Second)
		for time.Now().Before(deadline) {
			if !b.Spectating() {
				return true
			}
			time.Sleep(100 * time.Millisecond)
		}
	}
	return !b.Spectating()
}

// voteConfig proposes a config change and waits for the server to announce it.
// One send is not reliably enough: the command can arrive before the client is
// fully in, and OSP_vote_cmd has a two-frame debounce of its own, so a dropped
// proposal is silent on both sides.  Retrying cannot turn a broken build green
// -- a build that refuses the vote refuses every one of them.
func voteConfig(s *srv, b *playtest.Bot, name string) bool {
	for i := 0; i < 5; i++ {
		b.Cmd("vote config %s", name)
		if s.waitLog(`has initiated a vote`, 3*time.Second) {
			return s.waitLog(`Vote passed`, 10*time.Second)
		}
		time.Sleep(1500 * time.Millisecond)
	}
	return false
}

// cvarIs asks the server console for a cvar and returns true when it reads back
// as `want`.  Both engines answer a bare cvar name with
// `"name" is "value"  default: "..."`, so the value is matched inside quotes
// rather than by an `=` that neither prints.
func cvarIs(s *srv, name, want string) (bool, string) {
	before := len(s.grep(`"` + name + `" is `))
	s.console("%s", name)
	deadline := time.Now().Add(4 * time.Second)
	for time.Now().Before(deadline) {
		got := s.grep(`"` + name + `" is `)
		if len(got) > before {
			line := got[len(got)-1]
			return strings.Contains(line, `is "`+want+`"`), strings.TrimSpace(line)
		}
		time.Sleep(100 * time.Millisecond)
	}
	return false, "(no answer from the console)"
}

// ------------------------------------------------------------------ rows ---

func want(name string) bool { return *only == "" || strings.Contains(name, *only) }

// row1.  A passed config vote changes the level exactly once.
func row1(base string, p int) {
	if !want("configvote") {
		return
	}
	d := base + "-r1"
	// A config that is unmistakably applied: it moves a cvar nothing else does.
	must(stage(d, map[string]string{
		"maps.txt":          "q2dm1\nq2dm5\nq2dm7\n",
		"serverconfigs.txt": "pt_a.cfg\npt_b.cfg\n",
		"pt_a.cfg":          "set fraglimit 41\n",
		"pt_b.cfg":          "set fraglimit 42\n",
	}))
	s, err := startServer(d, p, map[string]string{
		"maxclients":         "8",
		"__current_config":   "pt_a.cfg",
		"vote_enable_config": "1",
		"vote_threshold":     "51",
		"timelimit":          "0",
		"fraglimit":          "41",
	}, "q2dm1")
	if err != nil {
		ck("configvote/boot", false, err.Error())
		return
	}
	defer s.stop()

	b := playtest.NewBot("voter", "127.0.0.1", p)
	if err := b.Start(30 * time.Second); err != nil {
		ck("configvote/client-in", false, err.Error())
		return
	}
	time.Sleep(1500 * time.Millisecond)

	baseMaps := s.count(`Loading maps from`)
	baseSpawn := s.count(`SpawnServer: |Loading map: `)

	ck("configvote/client-entered", enter(s, b), "the client is a player, not an observer")

	if !voteConfig(s, b, "pt_b.cfg") ||
		!s.waitLog(`Changing to config: pt_b\.cfg`, 10*time.Second) {
		ck("configvote/vote-passed", false,
			fmt.Sprintf("no 'Changing to config' line; prints=%v", tail(b.Prints(), 6)))
		return
	}
	ck("configvote/vote-passed", true, "server announced the config change")

	// The level must change, once, within a few seconds.
	// 40 s, because with the timer units repaired the manual-map intermission
	// is a real 15 seconds rather than 15 frames.
	changed := s.waitCount(`SpawnServer: |Loading map: `, baseSpawn+1, 40*time.Second)
	time.Sleep(6 * time.Second) // give a spinning arm time to be obvious

	// Is the server still running frames at all?  A wedged intermission looks
	// the same as a dead process from the log alone, and they are different
	// findings, so ask.
	s.console("status")
	alive := s.waitLog(`map *:|num +score|Connected clients`, 5*time.Second)
	ck("configvote/server-alive", alive, "server still answers its console")

	nMaps := s.count(`Loading maps from`) - baseMaps
	nSpawn := s.count(`SpawnServer: |Loading map: `) - baseSpawn
	refused := s.count(`Using 'map' will cause full server restart`)

	ck("configvote/level-changed", changed,
		fmt.Sprintf("map loads after the vote: %d", nSpawn))
	// One re-read for the arm itself, one for the new map's SpawnEntities.
	ck("configvote/no-intermission-spin", nMaps <= 3,
		fmt.Sprintf("'Loading maps from' after the vote: %d (a spin gives hundreds)", nMaps))
	ck("configvote/engine-did-not-refuse", refused == 0,
		fmt.Sprintf("refusals: %d", refused))

	// *** AND THE CLIENTS MUST STILL BE THERE. ***
	//
	// This is the half that matters on an OLD engine, where `map` is not
	// refused at all: Yamagi, q2ded and r1q2 honour it as a FULL SERVER
	// RESTART, which tears the server down and drops every connected player.
	// The level changes, so a row that only counts map loads passes -- and a
	// tournament has just lost both duellists to a config vote.  `gamemap` is
	// the command that changes level without disturbing anybody, which is what
	// ExitLevel() has always used.
	// Asked of the SERVER's own client list rather than of the client's frame
	// counter.  A `gamemap` makes both engines stuff `reconnect` at every
	// client, and a headless client that does not implement that handshake
	// stops receiving frames while still being connected -- so the client-side
	// reading says "dropped" for a server that kept it.  `status` is the
	// authoritative answer and it is the same question on both engines.
	before := len(s.grep(`^ *\d+ +-?\d+ +\S+ +` + regexp.QuoteMeta("voter")))
	s.console("status")
	time.Sleep(2 * time.Second)
	rows := s.grep(`^ *\d+ +-?\d+ +\S+ +` + regexp.QuoteMeta("voter"))
	ck("configvote/clients-survived", len(rows) > before,
		fmt.Sprintf("client rows in `status` after the change: %d", len(rows)-before))
}

// row2.  team_maxplayers survives a 1v1 map as a writable cvar.
func row2(base string, p int) {
	if !want("maxplayers") {
		return
	}
	d := base + "-r2"
	must(stage(d, map[string]string{"maps.txt": "q2dm1\nq2dm5\n"}))
	s, err := startServer(d, p, map[string]string{
		"maxclients":      "8",
		"match_mode":      "3", // 1v1: the arm that re-got the cvar NOSET
		"team_maxplayers": "4",
		"timelimit":       "0",
	}, "q2dm1")
	if err != nil {
		ck("maxplayers/boot", false, err.Error())
		return
	}
	defer s.stop()
	time.Sleep(2 * time.Second)

	ck("maxplayers/1v1-forced-to-1",
		len(s.grep(`1V1 Mode: setting teams' maxplayers to 1`)) > 0,
		"the 1v1 clamp ran")

	// The operator's own write, from the console the engine trusts most.
	s.console("set team_maxplayers 4")
	time.Sleep(700 * time.Millisecond)

	// Scoped to this cvar's own name: Yamagi prints "basedir is write
	// protected." at every boot, and an unscoped count reads that as a refusal
	// of the write under test.
	protected := s.count(`team_maxplayers (is write protected|may be set from command line only)`)
	ok, line := cvarIs(s, "team_maxplayers", "4")
	ck("maxplayers/operator-write-not-refused", protected == 0,
		fmt.Sprintf("engine refusals: %d", protected))
	ck("maxplayers/value-took", ok, line)
}

// row3.  A mixed-case config voted for in lower case is applied.
func row3(base string, p int) {
	if !want("configcase") {
		return
	}
	d := base + "-r3"
	must(stage(d, map[string]string{
		"maps.txt": "q2dm1\nq2dm5\n",
		// The operator's list names a MIXED-CASE file, which is the case no
		// engine's lower-case retry can rescue.
		"serverconfigs.txt": "PtMixedCase.cfg\n",
		"PtMixedCase.cfg":   "set fraglimit 77\n",
	}))
	s, err := startServer(d, p, map[string]string{
		"maxclients":         "8",
		"__current_config":   "default",
		"vote_enable_config": "1",
		"timelimit":          "0",
		"fraglimit":          "10",
	}, "q2dm1")
	if err != nil {
		ck("configcase/boot", false, err.Error())
		return
	}
	defer s.stop()

	b := playtest.NewBot("caser", "127.0.0.1", p)
	if err := b.Start(30 * time.Second); err != nil {
		ck("configcase/client-in", false, err.Error())
		return
	}
	time.Sleep(1500 * time.Millisecond)

	enter(s, b)
	// All lower case, as a player would type it.
	if !voteConfig(s, b, "ptmixedcase.cfg") ||
		!s.waitLog(`Changing to config:`, 10*time.Second) {
		ck("configcase/vote-passed", false, "no config change announced")
		return
	}
	announced := s.grep(`Changing to config:`)
	line := announced[len(announced)-1]
	ck("configcase/announced-operator-spelling",
		strings.Contains(line, "PtMixedCase.cfg"),
		strings.TrimSpace(line))

	// ...and the settings behind the name actually took.
	time.Sleep(3 * time.Second)
	ok3, fl := cvarIs(s, "fraglimit", "77")
	ck("configcase/config-was-applied", ok3, fl)
}

// row4 -- the timer-unit class, measured as wall time.
func row4(base string, p int) {
	if !want("intermission") {
		return
	}
	d := base + "-r4"
	must(stage(d, map[string]string{"maps.txt": "q2dm1\nq2dm5\nq2dm7\n"}))
	s, err := startServer(d, p, map[string]string{
		"maxclients":        "8",
		"timelimit":         "0.2", // ~12 s
		"nextlevel_default": "20",  // SECONDS: the intermission's own length
		"nextlevel_click":   "0",
	}, "q2dm1")
	if err != nil {
		ck("intermission/boot", false, err.Error())
		return
	}
	defer s.stop()

	b := playtest.NewBot("waiter", "127.0.0.1", p)
	if err := b.Start(30 * time.Second); err != nil {
		ck("intermission/client-in", false, err.Error())
		return
	}
	// Both anchors are COUNTED, not waited for.  The level rotates on its own
	// clock, so by the time the client is in, a "Timelimit hit" may already be
	// in the log -- and waiting on the pattern then returns instantly, timing
	// the NEXT full timelimit cycle instead of one intermission.  That reads as
	// a comfortable pass on a broken build.
	baseSpawn := s.count(`SpawnServer: |Loading map: `)
	baseTL := s.count(`Timelimit hit`)

	if !s.waitCount(`Timelimit hit`, baseTL+1, 60*time.Second) {
		ck("intermission/timelimit", false, "the level never ended")
		return
	}
	t0 := time.Now()
	if !s.waitCount(`SpawnServer: |Loading map: `, baseSpawn+1, 60*time.Second) {
		ck("intermission/level-changed", false, "no map change after the intermission")
		return
	}
	d4 := time.Since(t0)
	// 20 s asked for.  Unscaled it is 20 FRAMES, i.e. 2 s.  The midpoint is
	// far outside either engine's map-load time, which is what the gap has to
	// clear to be a measurement rather than a coin toss.
	ck("intermission/length-is-seconds-not-frames", d4 > 10*time.Second,
		fmt.Sprintf("nextlevel_default 20 -> intermission lasted %.1fs", d4.Seconds()))
}

// row5.  A config vote that changes runes_enable moves rune_stat.
func row5(base string, p int) {
	if !want("runecache") {
		return
	}
	d := base + "-r5"
	must(stage(d, map[string]string{
		"maps.txt":          "q2dm1\nq2dm5\n",
		"serverconfigs.txt": "pt_runes.cfg\n",
		"pt_runes.cfg":      "set runes_enable 31\n",
	}))
	s, err := startServer(d, p, map[string]string{
		"maxclients":         "8",
		"__current_config":   "default",
		"vote_enable_config": "1",
		"runes_enable":       "0", // the cache starts empty
		"timelimit":          "0",
	}, "q2dm1")
	if err != nil {
		ck("runecache/boot", false, err.Error())
		return
	}
	defer s.stop()

	b := playtest.NewBot("runer", "127.0.0.1", p)
	if err := b.Start(30 * time.Second); err != nil {
		ck("runecache/client-in", false, err.Error())
		return
	}
	time.Sleep(1500 * time.Millisecond)

	if g := lastEvent(d, "game_init"); g != nil {
		ck("runecache/starts-at-zero", num(g["runes"]) == 0,
			fmt.Sprintf(`game_init "runes":%d`, num(g["runes"])))
	} else {
		ck("runecache/starts-at-zero", false, "no game_init event in the stats log")
	}

	enter(s, b)
	if !voteConfig(s, b, "pt_runes.cfg") ||
		!s.waitLog(`Changing to config: pt_runes\.cfg`, 10*time.Second) {
		ck("runecache/vote-passed", false, "no config change announced")
		return
	}
	// Wait for a NEW map load, counted from a baseline: the boot's own line
	// matches this pattern too, so waiting on the pattern alone returns
	// instantly and reads the OLD game_init back.  And the wait has to outlast
	// the intermission, which with the timer units repaired is a real 15
	// seconds on the manual-map path rather than 1.5.
	baseSpawn := s.count(`SpawnServer: |Loading map: `)
	if !s.waitCount(`SpawnServer: |Loading map: `, baseSpawn+1, 60*time.Second) {
		ck("runecache/level-changed", false, "no map change")
		return
	}
	ck("runecache/level-changed", true, "the config change reached a new map")
	time.Sleep(3 * time.Second)

	g := lastEvent(d, "game_init")
	if g == nil {
		ck("runecache/refreshed-after-config", false, "no game_init after the change")
		return
	}
	ck("runecache/refreshed-after-config", num(g["runes"]) == 31,
		fmt.Sprintf(`runes_enable 31 -> game_init "runes":%d`, num(g["runes"])))
}

// row6.  Invulnerability alone must not write a "Quad" expiry.
func row6(base string, p int) {
	if !want("quadexpiry") {
		return
	}
	d := base + "-r6"
	must(stage(d, map[string]string{"maps.txt": "q2dm1\n"}))
	s, err := startServer(d, p, map[string]string{
		"maxclients": "8",
		"cheats":     "1", // the cvar the game reads is `cheats`, not sv_cheats
		"timelimit":  "0",
		// DF_FORCE_RESPAWN, so a headless client that cannot press fire still
		// respawns; DF_QUAD_DROP deliberately NOT set, because the arm under
		// test is the one that runs when the quad is not dropped.
		"dmflags":              "1024",
		"client_deathweapdrop": "1", // gates TossClientWeapon at all
	}, "q2dm1")
	if err != nil {
		ck("quadexpiry/boot", false, err.Error())
		return
	}
	defer s.stop()

	b := playtest.NewBot("penter", "127.0.0.1", p)
	if err := b.Start(30 * time.Second); err != nil {
		ck("quadexpiry/client-in", false, err.Error())
		return
	}
	time.Sleep(2 * time.Second)
	ck("quadexpiry/client-entered", enter(s, b), "the client is a player, not an observer")
	time.Sleep(1 * time.Second)

	// Precondition: pickups work at all on this server.  Without it a refused
	// grant and a broken grant read the same.
	b.Cmd("give Body Armor")
	time.Sleep(2 * time.Second)
	gotArmor := false
	for _, e := range statsEvents(d) {
		if e["event"] == "item_pickup" && strings.Contains(fmt.Sprint(e["item"]), "Armor") {
			gotArmor = true
		}
	}
	ck("quadexpiry/pickups-work", gotArmor, "an item_pickup reached the stats log")

	before := countExpire(d, "Quad")
	b.Cmd("give Invulnerability")
	time.Sleep(1500 * time.Millisecond)
	// The pickup itself must be in the log, or the row proves nothing.
	gotPent := false
	for _, e := range statsEvents(d) {
		if e["event"] == "item_pickup" && strings.Contains(fmt.Sprint(e["item"]), "Invulnerability") {
			gotPent = true
		}
	}
	ck("quadexpiry/invulnerability-held", gotPent, "item_pickup Invulnerability in the log")

	time.Sleep(6 * time.Second) // let it run out, then die holding nothing else
	b.Cmd("kill")
	time.Sleep(3 * time.Second)

	after := countExpire(d, "Quad")
	ck("quadexpiry/no-phantom-quad-expiry", after == before,
		fmt.Sprintf(`item_expire "Quad" events: %d before, %d after`, before, after))
}

func countExpire(d, item string) int {
	n := 0
	for _, e := range statsEvents(d) {
		if e["event"] == "item_expire" && fmt.Sprint(e["item"]) == item {
			n++
		}
	}
	return n
}

func num(v any) int {
	if f, ok := v.(float64); ok {
		return int(f)
	}
	return -1
}

func tail(s []string, n int) []string {
	if len(s) <= n {
		return s
	}
	return s[len(s)-n:]
}

func must(err error) {
	if err != nil {
		fmt.Fprintln(os.Stderr, "setup:", err)
		os.Exit(2)
	}
}

func main() {
	flag.Parse()
	if *binary == "" {
		if *engine == "q2pro" {
			*binary = "../../../q2pro/builddir-test/q2proded"
		} else {
			*binary = "../../../yquake2/release/q2ded"
		}
	}
	if *lib == "" {
		fmt.Fprintln(os.Stderr, "-lib is required")
		os.Exit(2)
	}
	if *dir == "" {
		*dir = filepath.Join("/tmp/ospfixes", *engine+"-"+*label)
	}
	fmt.Printf("== ospfixes: %s on %s ==\n   lib %s\n", *label, *engine, *lib)

	row1(*dir, *port+0)
	row2(*dir, *port+1)
	row3(*dir, *port+2)
	row4(*dir, *port+3)
	row5(*dir, *port+4)
	row6(*dir, *port+5)

	fmt.Printf("---- %s/%s: %d passed, %d failed ----\n", *engine, *label, pass, fail)
	if !*keep {
		// leave the dirs: the logs are the evidence
	}
	if fail > 0 {
		os.Exit(1)
	}
}
