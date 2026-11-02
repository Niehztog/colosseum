// bottag -- is every bot shown behind `botnametag`, under every ruleset family,
// and does everything that knows a bot by its OWN name still find it
// (R-BOT-37)?
//
// Players could not tell which of the others were bots, and the answer is a
// tag the GAME puts in front of every bot's name.  The bot keeps its own name
// in its userinfo, where bots.cfg and `addbot` put it; the name it is SHOWN by
// -- `pers.netname`, and from it the playerskins configstring that every
// scoreboard and HUD reads -- is the tag and then that name.  Those are two
// names for one bot, so the checks come in two halves: the tag reaches every
// place a player can see a name, and nothing that asks for a bot by its own
// name stopped finding it.
//
// Three servers, one per place a name is composed or published:
//
//	dm     OSP's RegularDM, so tourney's OSP_userinfoChanged composes the name.
//	       A person is not tagged; a bot is, on the wire and in the console's
//	       connect record; a second copy of one character is suffixed where the
//	       suffix can be SEEN, inside the ten characters the default tag leaves
//	       of "Bill Gates"; `removebot` finds a bot by its own name and by the
//	       name it is shown by; and a changed tag reaches a bot that is already
//	       in the game -- another tag, no tag, and one holding two of the
//	       characters no name may hold, which it loses.  `client_infochange 0`,
//	       because a change asked for inside a bot's first four seconds waits
//	       for its next spawn there (R-OSP-30), and that wait is tourney's
//	       rename rule rather than what is being tested.
//
//	       And the other side of the tag, which marks nothing if anybody can
//	       type it: a person who arrives as `[BOT]Faker` is shown as `Faker`,
//	       and so is the ENGINE's own copy of the name -- `status` prints it,
//	       and it is the copy a server browser is sent -- a rename to
//	       ` [bot]Liar` loses it the same way and is told so, a name that was
//	       only the tag is `unnamed`, and a person who chose `<B> Bob` before
//	       `<B> ` was the tag loses it when it becomes the tag.
//	ctf    the spine's ClientUserinfoChanged, and CTF's two name strings: the
//	       playerskins one CTFAssignSkin writes and the id view's.  The print
//	       that announces a bot carries the tag.  And the bot list, which knows
//	       a bot by its own name: with two bots in it and both in the game,
//	       `addrandom` must say the list is used up, rather than pick one of
//	       them again and be told the name is taken.
//	arena  the spine again, and arena's own id-view string, and a person
//	       arriving in the tag through the spine's half.
//
// Run it against a library from before the tag as the control: the eighteen
// rows that ask for a tag, or for a person to lose one, fail there, each on
// the name the library published, and the seven about people, `removebot`,
// the bot list and the empty tag -- which that library never had -- pass on
// both.
//
// Exit 0 every check passed, 1 a check failed, 2 the scenario could not run.
package main

import (
	"flag"
	"fmt"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
)

// MAX_CLIENTS is 256 in both configstring layouts, and the id-view names follow
// the player strings at +256 (CS_GENERAL).
const maxClients = 256

// The tag every bot is shown behind unless a server says otherwise, which is
// the default under test.
const defaultTag = "[BOT]"

var fails int

func check(name string, ok bool, detail string) {
	tag := " ok "
	if !ok {
		tag = "FAIL"
		fails++
	}
	fmt.Printf("  [%s] %-50s %s\n", tag, name, detail)
}

// layout is the first player configstring this client's server is using --
// the old one or the extended one -- found by where the client's own string
// landed, so neither number is assumed.
func layout(b *playtest.Bot, timeout time.Duration) (int, error) {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		cs := b.ConfigStrings()
		for _, base := range []int{colosseum.PlayerSkinBase(false), colosseum.PlayerSkinBase(true)} {
			for slot := 0; slot < maxClients; slot++ {
				if strings.HasPrefix(playtest.Decode(cs[base+slot]), b.Name+"\\") {
					return base, nil
				}
			}
		}
		time.Sleep(50 * time.Millisecond)
	}
	return 0, fmt.Errorf("%s's own player string never arrived", b.Name)
}

// shown is the name half of every player string a client holds, by slot.
func shown(b *playtest.Bot, base int) map[int]string {
	cs := b.ConfigStrings()
	out := map[int]string{}
	for slot := 0; slot < maxClients; slot++ {
		v := playtest.Decode(cs[base+slot])
		if v == "" {
			continue
		}
		if i := strings.Index(v, "\\"); i >= 0 {
			v = v[:i]
		}
		out[slot] = v
	}
	return out
}

// slotOf is the slot whose player string names exactly `name`, or -1.
func slotOf(b *playtest.Bot, base int, name string) int {
	for slot, n := range shown(b, base) {
		if n == name {
			return slot
		}
	}
	return -1
}

func waitSlot(b *playtest.Bot, base int, name string, timeout time.Duration) int {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if s := slotOf(b, base, name); s >= 0 {
			return s
		}
		time.Sleep(50 * time.Millisecond)
	}
	return -1
}

// names prints what a client holds, for a check's detail column.
func names(b *playtest.Bot, base int) string {
	var l []string
	for slot, n := range shown(b, base) {
		l = append(l, fmt.Sprintf("%d=%q", slot, n))
	}
	if len(l) == 0 {
		return "no player strings"
	}
	return strings.Join(l, " ")
}

type env struct {
	q2, ref, ctf, lib, glad, root string
	port                          int
}

func (e env) boot(label, ruleset, mapname string, port int, cvars map[string]string,
	roster []byte) (*playtest.Server, error) {
	dir := filepath.Join(e.root, label)
	os.RemoveAll(dir)
	if err := colosseum.Install(dir, e.ref, e.ctf, e.lib); err != nil {
		return nil, err
	}
	if err := colosseum.InstallBrain(dir, e.glad); err != nil {
		return nil, err
	}
	if _, err := os.Stat(filepath.Join(dir, "colosseum", "maps", mapname+".aas")); err != nil {
		return nil, fmt.Errorf("no navigation mesh for %s under %s/assets/maps -- "+
			"the brain refuses every bot without one", mapname, e.glad)
	}
	if roster != nil {
		if err := playtest.WriteFixture(filepath.Join(dir, "colosseum", "botcfg", "bots.cfg"),
			roster, 0o644); err != nil {
			return nil, err
		}
	}
	// No fill and no flat count, under either family's name: the only bots
	// are the ones a check adds.
	cv := map[string]string{
		"g_ruleset": ruleset, "deathmatch": "1", "coop": "0",
		"botfill": "0", "minimumplayers": "0", "bots_minplayers": "0",
		"bots_autoload": "0",
	}
	for k, v := range cvars {
		cv[k] = v
	}
	srv := &playtest.Server{
		Binary: e.q2, Dir: dir, Game: "colosseum", Map: mapname, Port: port,
		MaxClients: 8, LogPath: filepath.Join(dir, "server.log"), Cvars: cv,
	}
	if err := srv.Start(); err != nil {
		return nil, err
	}
	return srv, nil
}

// watch connects the person every check reads the wire through.
func watch(port int) (*playtest.Bot, int, error) {
	w := playtest.NewBot("watcher", "127.0.0.1", port)
	if err := w.Start(30 * time.Second); err != nil {
		return nil, 0, err
	}
	base, err := layout(w, 10*time.Second)
	if err != nil {
		w.Disconnect()
		return nil, 0, err
	}
	return w, base, nil
}

// person is the row every server opens with: the person's own name carries no
// tag, because only a bot's is composed with one.
func person(label string, w *playtest.Bot, base int) {
	check(label+"/a person is not tagged",
		slotOf(w, base, "watcher") >= 0 && slotOf(w, base, defaultTag+"watcher") < 0,
		names(w, base))
}

// arrive connects another person, under the name given.
func arrive(port int, name string) (*playtest.Bot, error) {
	p := playtest.NewBot(name, "127.0.0.1", port)
	if err := p.Start(30 * time.Second); err != nil {
		return nil, err
	}
	return p, nil
}

// impostor is the row for a person who arrives wearing the tag: shown by the
// name without it, and no player string under the tagged one.
func impostor(label string, w *playtest.Bot, base int, slot int) {
	check(label+"/a person who arrives with the tag loses it",
		slot >= 0 && slotOf(w, base, defaultTag+"Faker") < 0, names(w, base))
}

// listed asks the ENGINE what it calls a client: `status` prints its own copy
// of every connected client's name, the copy the reply to a server browser is
// made from, and the game reaches that copy only through the userinfo it is
// handed.  True when a line names `name` and none names `not`.
func listed(srv *playtest.Server, name, not string) (bool, string) {
	mark := srv.Len()
	srv.Console("status")
	_, err := srv.WaitLogFrom(mark, `\s`+regexp.QuoteMeta(name)+`\s`, 5*time.Second)
	time.Sleep(300 * time.Millisecond)
	rows := srv.GrepFrom(mark, regexp.QuoteMeta(name))
	bad := srv.GrepFrom(mark, regexp.QuoteMeta(not))
	return err == nil && len(bad) == 0, strings.Join(append(rows, bad...), " | ")
}

// addbot adds one bot by its own name and returns the name the server says it
// connected under -- its console record, "(<name> connected from SERVER_BOT)",
// which every ruleset prints once per bot (R-LOG-1) -- and the slot whose
// player string a client was sent under that name.  Whatever the name is, so a
// library without the tag fails the row that asks for one on the name it did
// publish rather than by timing out.  Not by watching for a NEW player string:
// under the OSP four a bot that leaves keeps its string until the slot is used
// again, so the next bot can arrive in a slot that never looked empty.
func addbot(srv *playtest.Server, w *playtest.Bot, base int, own string) (int, string) {
	mark := srv.Len()
	srv.Console(`sv addbot "%s"`, own)
	line, err := srv.WaitLogFrom(mark, `^\(.+ connected from SERVER_BOT\)$`, 60*time.Second)
	if err != nil {
		return -1, ""
	}
	name := reConnect.FindStringSubmatch(line)[1]
	return waitSlot(w, base, name, 10*time.Second), name
}

var (
	reConnect = regexp.MustCompile(`^\((.+) connected from SERVER_BOT\)$`)
)

// removebot removes a bot by the name given and reports whether the bot that
// left was `left` and no other: the console's departure record, which is the
// one place a removal is written down on every ruleset.
func removebot(srv *playtest.Server, by, left string) (bool, string) {
	mark := srv.Len()
	srv.Console(`sv removebot "%s"`, by)
	_, err := srv.WaitLogFrom(mark, `^\(`+regexp.QuoteMeta(left)+` disconnected from SERVER_BOT\)$`,
		10*time.Second)
	gone := srv.GrepFrom(mark, `disconnected from SERVER_BOT\)$`)
	return err == nil && len(gone) == 1, strings.Join(gone, " ")
}

// idView waits for the id view's name string of a slot to read `want`, and
// returns what it reads.  CS_GENERAL + slot, written beside the player string.
func idView(w *playtest.Bot, base, slot int, want string) string {
	got := ""
	deadline := time.Now().Add(10 * time.Second)
	for slot >= 0 && time.Now().Before(deadline) {
		if got = playtest.Decode(w.ConfigString(base + maxClients + slot)); got == want {
			break
		}
		time.Sleep(50 * time.Millisecond)
	}
	return got
}

// setTag sets `botnametag` and reads the value back, so a row about what the
// tag DID cannot be mistaken for one about what the console parsed.
func setTag(srv *playtest.Server, value string) error {
	mark := srv.Len()
	srv.Console(`set botnametag "%s"`, value)
	srv.Console(`botnametag`)
	_, err := srv.WaitLogFrom(mark, `"botnametag" is "`+regexp.QuoteMeta(value)+`"`, 5*time.Second)
	return err
}

func (e env) dm() error {
	fmt.Println("== dm (OSP_userinfoChanged) ==")
	srv, err := e.boot("dm", "dm", "q2dm1", e.port,
		map[string]string{"client_infochange": "0"}, nil)
	if err != nil {
		return err
	}
	defer srv.Stop()
	w, base, err := watch(e.port)
	if err != nil {
		return err
	}
	defer w.Disconnect()
	person("dm", w, base)

	// Nobody but a bot may wear the tag.  One person arrives in it, which
	// tourney's player list has to judge as the name it will be, and another
	// arrives in the tag this server is about to change to.
	f, err := arrive(e.port, "[BOT]Faker")
	if err != nil {
		return err
	}
	defer f.Disconnect()
	fslot := waitSlot(w, base, "Faker", 10*time.Second)
	impostor("dm", w, base, fslot)
	ok, rows := listed(srv, "Faker", "[BOT]Faker")
	check("dm/...and the engine lists them without it too", ok, rows)
	f.SetUserinfo("name", " [bot]Liar")
	got := waitSlot(w, base, "Liar", 10*time.Second)
	_, perr := f.WaitPrint(`kept for bots; yours is "Liar"`, 10*time.Second)
	check("dm/a rename into the tag loses it, and says so",
		fslot >= 0 && got == fslot && perr == nil, names(w, base)+" "+errText(perr))
	ok, rows = listed(srv, "Liar", "[bot]Liar")
	check("dm/...and the engine's copy loses it with the rename", ok, rows)
	f.SetUserinfo("name", "[BOT]")
	got = waitSlot(w, base, "unnamed", 10*time.Second)
	check("dm/a name that was only the tag is unnamed", fslot >= 0 && got == fslot,
		names(w, base))
	bob, err := arrive(e.port, "<B> Bob")
	if err != nil {
		return err
	}
	defer bob.Disconnect()
	bslot := waitSlot(w, base, "<B> Bob", 10*time.Second)

	// "Bill Gates" is ten characters, which is exactly what the default tag
	// leaves of fifteen: the whole name, and no room after it.
	slot, name := addbot(srv, w, base, "Bill Gates")
	check("dm/a bot is shown behind the tag", slot >= 0 && name == "[BOT]Bill Gates",
		fmt.Sprintf("slot %d is %q", slot, name))
	_, err = srv.WaitLog(`^\(\[BOT\]Bill Gates connected from SERVER_BOT\)`, 5*time.Second)
	check("dm/the console's connect record names the tag", err == nil, errText(err))

	// A second copy of the character.  Tourney seats it under a suffix, and the
	// suffix has to land where it can be seen: on the tenth character, since
	// the name already fills the ten the tag leaves.
	slot2, name2 := addbot(srv, w, base, "Bill Gates")
	check("dm/a second copy's suffix can be seen", slot2 >= 0 && name2 == "[BOT]Bill Gate0",
		fmt.Sprintf("slot %d is %q", slot2, name2))

	// The bot list and an operator know a bot by its own name, the bot menu
	// by the one it is shown by.
	ok, gone := removebot(srv, "Bill Gates", name)
	check("dm/removebot finds a bot by its own name", slot >= 0 && ok, gone)
	ok, gone = removebot(srv, name2, name2)
	check("dm/removebot finds a bot by the name it is shown by", slot2 >= 0 && ok,
		fmt.Sprintf("removebot %q: %s", name2, gone))

	// A change of tag reaches a bot that is already in the game.
	slot, name = addbot(srv, w, base, "Laura Craft")
	check("dm/the default tag cuts a long name short", slot >= 0 && name == "[BOT]Laura Craf",
		fmt.Sprintf("slot %d is %q", slot, name))
	for i, c := range []struct{ name, tag, want string }{
		{"dm/a changed tag reaches a bot in the game", "<B> ", "<B> Laura Craft"},
		{"dm/an empty tag is no tag", "", "Laura Craft"},
		// `\` would end the name inside the player string, and `;` is kept
		// out of every name a person can choose.  A `"` cannot be typed at a
		// console, so this pair stands for the three.
		{"dm/the tag loses the characters no name holds", `[\;BOT]`, "[BOT]Laura Craf"},
	} {
		if err := setTag(srv, c.tag); err != nil {
			check(c.name, false, "the console did not set it: "+err.Error())
			continue
		}
		got := waitSlot(w, base, c.want, 5*time.Second)
		check(c.name, slot >= 0 && got == slot, names(w, base))
		// ...and the first change put on a person the tag they had chosen
		// before it was the tag.
		if i == 0 {
			got = waitSlot(w, base, "Bob", 5*time.Second)
			check("dm/a changed tag comes off a person who had it",
				bslot >= 0 && got == bslot, names(w, base))
		}
	}
	return nil
}

func (e env) ctfPhase() error {
	fmt.Println("== ctf (ClientUserinfoChanged, CTFAssignSkin, the id view) ==")
	// Two bots in the list, so that the list can be used up.
	roster := []byte(`"sv" "addbot"   "Bill Gates"        "male/sniper"       "bots/bill_c.c"   "bill"
"sv" "addbot"   "Laura Craft"       "female/ensign"     "bots/laura_c.c"  "laura"
`)
	srv, err := e.boot("ctf", "ctf", "q2ctf1", e.port+1, nil, roster)
	if err != nil {
		return err
	}
	defer srv.Stop()
	w, base, err := watch(e.port + 1)
	if err != nil {
		return err
	}
	defer w.Disconnect()
	person("ctf", w, base)

	slot, name := addbot(srv, w, base, "Bill Gates")
	check("ctf/a bot is shown behind the tag", slot >= 0 && name == "[BOT]Bill Gates",
		fmt.Sprintf("slot %d is %q", slot, name))
	got := idView(w, base, slot, "[BOT]Bill Gates")
	check("ctf/the id view's name string has the tag", got == "[BOT]Bill Gates",
		fmt.Sprintf("general + %d = %q", slot, got))
	// The arrival print comes when the brain has the bot ready, which on a
	// mesh's first load is its whole reachability build.
	said, err := w.WaitPrint(`Bill Gates entered the game`, 90*time.Second)
	check("ctf/the print announcing a bot has the tag",
		err == nil && strings.HasPrefix(said, "[BOT]Bill Gates entered"),
		strings.TrimSpace(said)+errText(err))

	// The list knows its bots by their own names.  Both in the game, nothing
	// left to add: the roster walk has to see that, which it cannot do by
	// comparing the list's "Laura Craft" with the "[BOT]Laura Craf" shown.
	slot2, _ := addbot(srv, w, base, "Laura Craft")
	mark := srv.Len()
	srv.Console(`sv addrandom`)
	used, err := srv.WaitLogFrom(mark, `Every configured bot is already in the game|already used`, 10*time.Second)
	check("ctf/the bot list sees its bots in the game",
		slot2 >= 0 && err == nil && strings.Contains(used, "Every configured bot"),
		strings.TrimSpace(used)+errText(err))
	return nil
}

func (e env) arena() error {
	fmt.Println("== arena (ClientUserinfoChanged, arena's id view) ==")
	srv, err := e.boot("arena", "arena", "q2dm1", e.port+2, nil, nil)
	if err != nil {
		return err
	}
	defer srv.Stop()
	w, base, err := watch(e.port + 2)
	if err != nil {
		return err
	}
	defer w.Disconnect()
	person("arena", w, base)
	f, err := arrive(e.port+2, "[BOT]Faker")
	if err != nil {
		return err
	}
	defer f.Disconnect()
	impostor("arena", w, base, waitSlot(w, base, "Faker", 10*time.Second))

	slot, name := addbot(srv, w, base, "Bill Gates")
	check("arena/a bot is shown behind the tag", slot >= 0 && name == "[BOT]Bill Gates",
		fmt.Sprintf("slot %d is %q", slot, name))
	got := idView(w, base, slot, "[BOT]Bill Gates")
	check("arena/the id view's name string has the tag", got == "[BOT]Bill Gates",
		fmt.Sprintf("general + %d = %q", slot, got))
	return nil
}

func errText(err error) string {
	if err == nil {
		return ""
	}
	return err.Error()
}

func main() {
	q2 := flag.String("q2proded", "", "path to q2proded")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "retail baseq2 paks")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "Threewave paks, for q2ctf1")
	lib := flag.String("lib", "", "game library under test")
	glad := flag.String("gladdir", "", "the brain's checkout: its pak7, bots.cfg and assets/maps meshes")
	dir := flag.String("dir", "/tmp/q2playtest/bottag", "scratch install root")
	port := flag.Int("port", 28070, "first server port; the three use it and the next two")
	only := flag.String("only", "", "run only these servers, e.g. dm,ctf")
	flag.Parse()

	if *q2 == "" || *lib == "" || *glad == "" {
		fmt.Fprintln(os.Stderr, "need -q2proded, -lib and -gladdir")
		os.Exit(2)
	}
	e := env{q2: *q2, ref: *ref, ctf: *ctf, lib: *lib, glad: *glad, root: *dir, port: *port}
	for _, p := range []struct {
		name string
		run  func() error
	}{{"dm", e.dm}, {"ctf", e.ctfPhase}, {"arena", e.arena}} {
		if *only != "" && !strings.Contains(","+*only+",", ","+p.name+",") {
			continue
		}
		if err := p.run(); err != nil {
			fmt.Fprintf(os.Stderr, "ERROR: %s: %v\n", p.name, err)
			os.Exit(2)
		}
	}
	fmt.Printf("\nbottag: %d check(s) failed\n", fails)
	if fails > 0 {
		os.Exit(1)
	}
}
