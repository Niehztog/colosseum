// botlibmenu -- can a player choose between the two botlibs, and a Quake III
// bot's skill, from the bot menu?
//
// R-BOT-34 is the player's half of offering two botlibs, and none of it is
// visible to `sv`: it is the menu.  With more than one botlib offered, the add
// list puts each one's bots under its name, in `botlibs`' order; a bot of the
// botlib that has skills is a SUBMENU holding Quake III's five, with the one it
// would get unasked marked, and picking one adds the bot at that skill; a
// Gladiator bot is added the way it always was, at once; and the bots page
// carries a `bot skill` row that cycles `botskill` through the five.  So this
// drives a real client through the menu with the inventory keys, and reads
// every step back from both sides -- the layout the client was sent, and what
// the server says it did.
//
// THE BOTLIB'S OWN WORD IS THE RECEIPT FOR THE SKILL.  `sv botlibdump` prints
// the skill the game pushed, which is the game's word; Quake III's botlib loads
// a character at skill 1, 4 or 5 directly and INTERPOLATES any other one from
// the 1 and the 4 -- so "loaded skill 1 from bots/sarge_c.c" in the log is the
// library itself saying it was asked for less than 4, and a bot added at the
// default 4 never prints it.
//
// Fixtures: a three-bot Quake III list and a two-bot Gladiator one, so the whole
// add list fits on one page and nothing here depends on scrolling; and both
// botlibs' meshes for the map -- Gladiator's from its distribution, the Quake
// III one made here with that botlib's own bspc, because none ships.  Under
// `ctf`: the OSP four route `menu` to tourney's own menu (botmenugate).
//
// ...which is the second server, under `dm` on the next port: there the bots
// are a page of the vote menu, and the same choice is a label, a picker and a
// vote (runOSP).
package main

import (
	"flag"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"time"

	"q2playtest/colosseum"
	"q2playtest/playtest"
)

var pass, fail int

func ok(what, detail string)  { pass++; fmt.Printf("  [ ok ] %-52s %s\n", what, detail) }
func bad(what, detail string) { fail++; fmt.Printf("  [FAIL] %-52s %s\n", what, detail) }

func check(cond bool, what, good, wrong string) bool {
	if cond {
		ok(what, good)
	} else {
		bad(what, wrong)
	}
	return cond
}

func die(format string, a ...any) {
	fmt.Printf("botlibmenu: "+format+"\n", a...)
	os.Exit(2)
}

// The two rosters.  The Quake III lines are the distribution's own; the
// Gladiator ones name characters that are in pak7.pak.
const q3roster = `"sv" "addbot"   "Sarge"   "sarge/default"   "bots/sarge_c.c"   "sarge"
"sv" "addbot"   "Grunt"   "grunt/default"   "bots/grunt_c.c"   "grunt"
"sv" "addbot"   "Xaero"   "xaero/default"   "bots/xaero_c.c"   "xaero"
`

const gladroster = `"sv" "addbot"   "Adrenaline Hunk"   "male/viper"      "bots/hunk_c.c"   "hunk"
"sv" "addbot"   "Zero"              "cyborg/tyr574"   "bots/zero_c.c"   "zero"
`

var (
	q3bots   = []string{"Sarge", "Grunt", "Xaero"}
	gladbots = []string{"Adrenaline Hunk", "Zero"}
	skills   = []string{"1 I Can Win", "2 Bring It On", "3 Hurt Me Plenty", "4 Hardcore", "5 Nightmare!"}
)

// ---------------------------------------------------------------- the menu

// The bot menu draws on the LAYOUT (p_menulib.c bot_DisplayMenu): every row is
// `xv 66 yv <y> string "<text>"`, the highlighted one has a character 13|128 at
// xv 50 on the same yv, and a row that opens a submenu has a plain 13 at
// xv 194.  Decode strips the high bit, so both arrive here as "\r".
type row struct {
	y    int
	text string
	sub  bool
}

type page struct {
	rows   []row
	cursor int // the highlighted row's yv, -1 when no row is
}

var reString = regexp.MustCompile(`xv (-?\d+) yv (-?\d+) string "([^"]*)"`)

func parse(layout string) page {
	p := page{cursor: -1}
	subs := map[int]bool{}
	for _, m := range reString.FindAllStringSubmatch(layout, -1) {
		x, _ := strconv.Atoi(m[1])
		y, _ := strconv.Atoi(m[2])
		switch {
		case x == 66:
			p.rows = append(p.rows, row{y: y, text: strings.TrimRight(m[3], " ")})
		case x == 50 && m[3] == "\r":
			p.cursor = y
		case x == 194 && m[3] == "\r":
			subs[y] = true
		}
	}
	for i := range p.rows {
		p.rows[i].sub = subs[p.rows[i].y]
	}
	return p
}

func (p page) at() string {
	for _, r := range p.rows {
		if r.y == p.cursor {
			return r.text
		}
	}
	return ""
}

func (p page) find(text string) (row, bool) {
	for _, r := range p.rows {
		if r.text == text {
			return r, true
		}
	}
	return row{}, false
}

func (p page) texts() []string {
	var t []string
	for _, r := range p.rows {
		t = append(t, r.text)
	}
	return t
}

// settled is the layout once it has stopped changing: a redraw is one
// svc_layout per frame, and reading between two of them reads a page that is
// half the last one.
func settled(b *playtest.Bot) string {
	last := b.Layout()
	for i, stable := 0, 0; i < 40; i++ {
		time.Sleep(100 * time.Millisecond)
		now := b.Layout()
		if now == last {
			if stable++; stable >= 3 {
				return now
			}
			continue
		}
		last, stable = now, 0
	}
	return last
}

// waitPage waits for a page that has a row with this text.
func waitPage(b *playtest.Bot, text string, d time.Duration) (page, bool) {
	for deadline := time.Now().Add(d); ; {
		p := parse(settled(b))
		if _, found := p.find(text); found {
			return p, true
		}
		if time.Now().After(deadline) {
			return p, false
		}
	}
}

// moveTo presses `invnext` until the cursor is on the row with this text, with
// ONE press outstanding at a time: a loop that runs ahead of the server lands
// on the wrong row and selects it.
func moveTo(b *playtest.Bot, text string) bool {
	p := parse(settled(b))
	for tries := 0; tries < 24 && p.at() != text; tries++ {
		prev := p.cursor
		b.Cmd("invnext")
		for j := 0; j < 10; j++ {
			if p = parse(settled(b)); p.cursor != prev {
				break
			}
		}
	}
	return p.at() == text
}

// pick moves to a row and selects it: ONE `invuse`, because a page that has
// already opened has a cursor of its own and a second press selects on it.
func pick(b *playtest.Bot, text string) bool {
	if !moveTo(b, text) {
		return false
	}
	b.Cmd("invuse")
	return true
}

// ---------------------------------------------------------------- the server

// dump asks the server for one of its diagnostics and returns what it printed.
func dump(srv *playtest.Server, cmd, until string) []string {
	n := srv.Len()
	srv.Console("%s", cmd)
	srv.WaitLogFrom(n, until, 5*time.Second)
	time.Sleep(300 * time.Millisecond)
	return srv.GrepFrom(n, ".")
}

// clientLine is the library dump's line for a bot, and the library it was
// listed under.
func clientLine(lines []string, name string) (line, library string) {
	lib := ""
	for _, l := range lines {
		if strings.Contains(l, " botlib (") {
			lib = l
		}
		if strings.Contains(l, "client") && strings.Contains(l, ": "+name) {
			return strings.TrimSpace(l), lib
		}
	}
	return "", ""
}

// makeMesh makes the Quake III mesh for a map with that botlib's bspc, reading
// the map out of whichever pak has it -- bspc takes `<pak>/maps/<map>.bsp`.
func makeMesh(bspc, mapname, work string, sources []string) (string, error) {
	if err := os.MkdirAll(work, 0o755); err != nil {
		return "", err
	}
	out := filepath.Join(work, mapname+".aas")
	os.Remove(out)
	var tried []string
	for _, src := range sources {
		bsp := src + "/maps/" + mapname + ".bsp"
		cmd := exec.Command(bspc, "-bsp2aas", bsp, "-output", work)
		cmd.Dir = work
		log, _ := cmd.CombinedOutput()
		os.WriteFile(filepath.Join(work, "bspc-out.log"), log, 0o644)
		if st, err := os.Stat(out); err == nil && st.Size() > 0 {
			return out, nil
		}
		tried = append(tried, bsp)
	}
	return "", fmt.Errorf("bspc made no %s.aas from any of %v", mapname, tried)
}

// The inputs every server here is built from.
var cfg struct {
	q2, lib, ref, ctf, glad, q3dir, q3bot, bspc string
}

// install lays out one server's gamedir: both botlibs, the two short bot lists,
// and both botlibs' meshes for mapname -- Gladiator's from its distribution,
// the Quake III one `mesh`, or made here when that is "".
func install(dir, mapname, mesh string) error {
	if err := colosseum.Install(dir, cfg.ref, cfg.ctf, cfg.lib); err != nil {
		return err
	}
	if err := colosseum.InstallBrain(dir, cfg.glad); err != nil {
		return err
	}
	game := filepath.Join(dir, "colosseum")
	if err := playtest.WriteFixture(filepath.Join(game, "botcfg", "bots.cfg"),
		[]byte(gladroster), 0o644); err != nil {
		return err
	}
	if _, err := os.Stat(filepath.Join(game, "maps", mapname+".aas")); err != nil {
		return fmt.Errorf("no Gladiator mesh for %s in %s/assets/maps", mapname, cfg.glad)
	}
	if err := colosseum.InstallQ3(dir, cfg.q3bot, cfg.q3dir, []byte(q3roster)); err != nil {
		return err
	}
	if mesh == "" {
		var sources []string
		for _, d := range []string{cfg.ctf, cfg.ref} {
			if _, err := os.Stat(filepath.Join(d, "maps", mapname+".bsp")); err == nil {
				sources = append(sources, d)
			}
			paks, _ := filepath.Glob(filepath.Join(d, "pak*.pak"))
			sources = append(sources, paks...)
		}
		start := time.Now()
		var err error
		if mesh, err = makeMesh(cfg.bspc, mapname, filepath.Join(dir, "bspc"), sources); err != nil {
			return err
		}
		fmt.Printf("=== Quake III mesh for %s made with %s in %s\n", mapname, cfg.bspc,
			time.Since(start).Round(time.Second))
	}
	return playtest.CopyFixture(mesh, filepath.Join(game, "q3bot", "maps", mapname+".aas"))
}

func main() {
	// Spelled flag.String("name", ...) and never StringVar: tools/playtest.sh
	// decides which of its inputs to pass by grepping for that shape.
	q2 := flag.String("q2proded", "", "")
	lib := flag.String("lib", "", "")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "")
	dir := flag.String("dir", "/tmp/colosseum-botlibmenu", "")
	glad := flag.String("gladdir", "", "")
	q3dir := flag.String("q3dir", "", "a q3a_bot_backport_for_q2 checkout, for its assets/botfiles")
	q3bot := flag.String("q3bot", "", "the Quake III botlib (default: q3bot.so beside -lib, where the Makefile builds it)")
	bspc := flag.String("bspc", "", "its bspc (default: q3bot/bspc beside -lib)")
	q3aas := flag.String("q3aas", "", "a Quake III mesh for -map (default: made with -bspc)")
	mp := flag.String("map", "q2ctf1", "")
	port := flag.Int("port", 27996, "the first of two ports; the OSP server is on the second")
	rcon := flag.String("rcon", "secretpw", "")
	flag.Parse()
	cfg.q2, cfg.lib, cfg.ref, cfg.ctf = *q2, *lib, *ref, *ctf
	cfg.glad, cfg.q3dir, cfg.q3bot, cfg.bspc = *glad, *q3dir, *q3bot, *bspc

	if cfg.glad == "" {
		die("no Gladiator botlib (-gladdir): this is a test of choosing between two")
	}
	if cfg.q3dir == "" {
		die("no Quake III botlib data (-q3dir): a q3a_bot_backport_for_q2 checkout")
	}
	if cfg.q3bot == "" {
		cfg.q3bot = filepath.Join(filepath.Dir(cfg.lib), "q3bot.so")
	}
	if cfg.bspc == "" {
		cfg.bspc = filepath.Join(filepath.Dir(cfg.lib), "q3bot", "bspc")
	}

	if err := install(*dir, *mp, *q3aas); err != nil {
		die("%v", err)
	}
	srv := &playtest.Server{
		Binary: cfg.q2, Dir: *dir, Game: "colosseum", Map: *mp, Port: *port,
		MaxClients: 8, LogPath: *dir + "/server.log",
		Cvars: map[string]string{
			"g_ruleset": "ctf", "bots": "1", "rcon_password": *rcon,
			// read when the menu tree is built, which is once per game
			"botlibs": "q3,gladiator",
		},
	}
	if err := srv.Start(); err != nil {
		die("%v", err)
	}
	b := playtest.NewBot("menutester", "127.0.0.1", *port)
	if err := b.Start(30 * time.Second); err != nil {
		srv.Stop()
		die("%v", err)
	}
	time.Sleep(2 * time.Second)

	fmt.Printf("=== %s, botlibs \"q3,gladiator\"\n", *mp)
	run(srv, b, *mp, *rcon)
	b.Disconnect()
	srv.Stop()

	fmt.Printf("=== q2dm1 under dm, the OSP vote menu, botlibs \"q3,gladiator\"\n")
	runOSP(filepath.Join(*dir, "osp"), *port+1)

	fmt.Printf("\n%d check(s), %d failed\n", pass+fail, fail)
	if fail > 0 {
		os.Exit(1)
	}
}

// skillRow is the bots page's `bot skill` row, whatever value it shows.
func skillRow(p page) string {
	for _, t := range p.texts() {
		if strings.HasPrefix(t, "bot skill") {
			return t
		}
	}
	return ""
}

// run is every check, in the order a player meets them.  It returns early
// when a page that the rest depends on never opened: what follows would only
// report the same failure again under other names.
func run(srv *playtest.Server, b *playtest.Bot, mp, rcon string) {
	// ---- the server's half: both botlibs offered, both able to play ------
	all := strings.Join(dump(srv, "sv botlibs", `^botskill`), "\n")
	check(regexp.MustCompile(`bot list q3bot/bots\.cfg: 3 bots`).MatchString(all),
		"server/the Quake III bot list is its own", "q3bot/bots.cfg: 3 bots",
		fmt.Sprintf("no Quake III list of 3 in %q", all))
	check(regexp.MustCompile(`bot list \S*bots\.cfg: 2 bots`).MatchString(all),
		"server/the Gladiator bot list is its own", "2 bots",
		fmt.Sprintf("no Gladiator list of 2 in %q", all))
	playable := regexp.MustCompile(`can play this map`).FindAllString(all, -1)
	check(len(playable) == 2 && !strings.Contains(all, "cannot play"),
		"server/both botlibs can play "+mp, "two \"can play this map\"",
		fmt.Sprintf("%d of 2: %q", len(playable), all))

	// ---- the bots page: the skill row -------------------------------------
	b.Cmd("menu %s", rcon)
	if _, up := waitPage(b, "Bots", 6*time.Second); !up {
		bad("menu/the bot menu opens", fmt.Sprintf("layout=%.120q", b.Layout()))
		return
	}
	ok("menu/the bot menu opens", "rcon password accepted")
	pick(b, "Bots")
	p, up := waitPage(b, "add bot", 6*time.Second)
	if !check(up, "menu/the bots page opens", "", fmt.Sprintf("rows %q", p.texts())) {
		return
	}
	check(regexp.MustCompile(`^bot skill\s+4 Hardcore$`).MatchString(skillRow(p)),
		"bots/a `bot skill` row, at the default", fmt.Sprintf("%q", skillRow(p)),
		fmt.Sprintf("rows %q", p.texts()))

	// ---- the add list: one heading per botlib, in `botlibs`' order --------
	pick(b, "add bot")
	p, up = waitPage(b, "- Quake III Arena -", 6*time.Second)
	if !check(up, "add/the add list has a Quake III heading", "",
		fmt.Sprintf("rows %q", p.texts())) {
		return
	}
	index := map[string]int{}
	for i, r := range p.rows {
		index[r.text] = i
	}
	q3h, gh := index["- Quake III Arena -"], -1
	if i, found := index["- Gladiator -"]; found {
		gh = i
	}
	check(gh > q3h, "add/...then a Gladiator heading after it",
		fmt.Sprintf("rows %d and %d", q3h, gh), fmt.Sprintf("rows %q", p.texts()))
	under := func(name string) string {
		i, found := index[name]
		switch {
		case !found:
			return "missing"
		case i > q3h && (gh < 0 || i < gh):
			return "q3"
		case gh >= 0 && i > gh:
			return "gladiator"
		}
		return "elsewhere"
	}
	for _, n := range q3bots {
		r, _ := p.find(n)
		check(under(n) == "q3" && r.sub, "add/"+n+" is under Quake III, a submenu",
			"", fmt.Sprintf("%s, submenu=%v", under(n), r.sub))
	}
	for _, n := range gladbots {
		r, _ := p.find(n)
		check(under(n) == "gladiator" && !r.sub, "add/"+n+" is under Gladiator, a row",
			"", fmt.Sprintf("%s, submenu=%v", under(n), r.sub))
	}

	// ---- a Quake III bot: its skill submenu -------------------------------
	// `2 Bring It On` is a row on that page and on no other, and is not the
	// marked one, so it says the page is up without assuming the mark.
	pick(b, "Sarge")
	p, up = waitPage(b, "2 Bring It On", 6*time.Second)
	if !check(up, "skill/Sarge opens a skill submenu", "",
		fmt.Sprintf("rows %q", p.texts())) {
		return
	}
	var got []string
	marked := ""
	for _, t := range p.texts() {
		s := strings.TrimSuffix(t, " *")
		for _, want := range skills {
			if s == want {
				got = append(got, s)
				if s != t {
					marked += s
				}
			}
		}
	}
	check(strings.Join(got, "|") == strings.Join(skills, "|"),
		"skill/its five skills, in order", strings.Join(got, ", "),
		fmt.Sprintf("rows %q", p.texts()))
	check(marked == "4 Hardcore", "skill/`botskill`'s is the one marked",
		fmt.Sprintf("%q", marked), fmt.Sprintf("marked %q", marked))
	_, named := p.find("Sarge")
	check(named, "skill/the bot's name heads it", "\"Sarge\"", fmt.Sprintf("rows %q", p.texts()))

	n := srv.Len()
	pick(b, "2 Bring It On")
	_, err := srv.WaitLogFrom(n, `Sarge entered the game`, 15*time.Second)
	check(err == nil, "skill/picking `2 Bring It On` adds Sarge", "entered the game",
		"Sarge never entered the game")
	p, up = waitPage(b, "- Quake III Arena -", 6*time.Second)
	_, still := p.find("Sarge")
	check(up && !still, "skill/...and goes back to a list without him", "",
		fmt.Sprintf("rows %q", p.texts()))
	line, library := clientLine(dump(srv, "sv botlibdump", `client`), "Sarge")
	check(strings.HasSuffix(line, "Sarge, skill 2") && strings.Contains(library, "the q3 botlib"),
		"skill/the game set him up at 2, on the Quake III botlib",
		fmt.Sprintf("%q", line), fmt.Sprintf("%q under %q", line, library))
	check(len(srv.GrepFrom(n, `loaded skill 1 from bots/sarge_c\.c`)) > 0,
		"skill/...and the botlib interpolated below 4",
		"\"loaded skill 1 from bots/sarge_c.c\"",
		"the botlib never loaded skill 1 -- it was not asked for less than 4")

	// ---- a Gladiator bot: added at once -----------------------------------
	n = srv.Len()
	pick(b, "Zero")
	_, err = srv.WaitLogFrom(n, `Zero entered the game`, 15*time.Second)
	check(err == nil, "glad/picking Zero adds him at once", "entered the game",
		"Zero never entered the game")
	p, _ = waitPage(b, "- Gladiator -", 6*time.Second)
	_, still = p.find("Zero")
	check(!still, "glad/...and the list no longer offers him", "",
		fmt.Sprintf("rows %q", p.texts()))
	line, library = clientLine(dump(srv, "sv botlibdump", `client`), "Zero")
	check(line != "" && !strings.Contains(line, "skill") &&
		strings.Contains(library, "the gladiator botlib"),
		"glad/he is on the Gladiator botlib, with no skill of his own",
		fmt.Sprintf("%q", line), fmt.Sprintf("%q under %q", line, library))

	// ---- the skill row cycles `botskill` through the five, and wraps ------
	pick(b, "back")
	if p, up = waitPage(b, "add bot", 6*time.Second); !up {
		bad("cycle/back to the bots page", fmt.Sprintf("rows %q", p.texts()))
		return
	}
	for _, want := range []string{"5 Nightmare!", "1 I Can Win"} {
		pick(b, skillRow(parse(settled(b))))
		row := ""
		for deadline := time.Now().Add(5 * time.Second); time.Now().Before(deadline); {
			if row = skillRow(parse(settled(b))); strings.HasSuffix(row, want) {
				break
			}
		}
		sv := regexp.MustCompile(`botskill.*`).FindString(
			strings.Join(dump(srv, "sv botlibs", `^botskill`), "\n"))
		num := want[:1]
		check(strings.HasSuffix(row, want) && strings.HasPrefix(sv, "botskill   "+num+" "),
			"cycle/the row moves `botskill` to "+num, fmt.Sprintf("%q; %q", row, sv),
			fmt.Sprintf("row %q; server %q", row, sv))
	}
}

// ---------------------------------------------------------------- the OSP four
//
// Under dm, dmpro, tdm and duel `menu` is tourney's own, and its bots are a page
// of the VOTE menu (osp_menus.c).  Its labels named Gladiator's bots, so with
// two botlibs offered they say "Bots", and the picker puts a bot's botlib's two
// letters in front of its name -- "GB|", which the page's placeholder row always
// carried, or "Q3|".  A vote for a picked bot is a vote for an index into the bot
// list, so the proposal names the bot and its botlib too, and the bot it adds
// runs on its own botlib.  One entered person is every voter -- bots are not
// counted -- so a proposal passes on the spot.

// The tourney menu is a layout too, drawn by osp_PMenu: one string per row,
// and the highlighted one starts with a 13.  (ospbotvote's helpers.)
var reOSPRow = regexp.MustCompile(`\bc?string2?\s+"([^"]*)"`)

func ospRows(layout string) []string {
	var out []string
	for _, m := range reOSPRow.FindAllStringSubmatch(layout, -1) {
		out = append(out, strings.TrimSpace(strings.TrimPrefix(m[1], "\x0d")))
	}
	return out
}

func ospCursor(layout string) string {
	for _, m := range reOSPRow.FindAllStringSubmatch(layout, -1) {
		if strings.HasPrefix(m[1], "\x0d") {
			return strings.TrimSpace(strings.TrimPrefix(m[1], "\x0d"))
		}
	}
	return ""
}

func ospRow(layout, re string) string {
	rx := regexp.MustCompile(re)
	for _, r := range ospRows(layout) {
		if rx.MatchString(r) {
			return r
		}
	}
	return ""
}

// `inven` is a toggle: look first, press only when the menu is not up.
func ospOpen(b *playtest.Bot) bool {
	rx := regexp.MustCompile(`Regular DM Mode|Teamplay Mode|1v1 Mode`)
	for i := 0; i < 8; i++ {
		if rx.MatchString(settled(b)) {
			return true
		}
		b.Cmd("inven")
		time.Sleep(1200 * time.Millisecond)
	}
	return false
}

// ospMove presses `invnext` until the cursor is on a row matching re, one press
// outstanding at a time.
func ospMove(b *playtest.Bot, re string) bool {
	rx := regexp.MustCompile(re)
	cur := ospCursor(settled(b))
	for i := 0; i < 24 && !rx.MatchString(cur); i++ {
		prev := cur
		b.Cmd("invnext")
		for j := 0; j < 8; j++ {
			if cur = ospCursor(settled(b)); cur != prev {
				break
			}
		}
	}
	return rx.MatchString(cur)
}

// ospPick selects the row the cursor is on and waits for a page with a row
// matching re: one press and a long look, because a page that has opened has a
// cursor of its own and a second press would select on it.
func ospPick(b *playtest.Bot, re string) (string, bool) {
	b.Cmd("invuse")
	for deadline := time.Now().Add(6 * time.Second); time.Now().Before(deadline); {
		if l := settled(b); ospRow(l, re) != "" {
			return l, true
		}
	}
	return b.Layout(), false
}

// ospBotPage is the main menu -> Voting Menu -> the bot page, and what the
// voting menu's bot row said on the way.
func ospBotPage(b *playtest.Bot) (botrow string, page string, ok bool) {
	if !ospOpen(b) || !ospMove(b, `Voting Menu`) {
		return "", b.Layout(), false
	}
	l, up := ospPick(b, `\[ Voting Menu \]`)
	if !up {
		return "", l, false
	}
	botrow = ospRow(l, `Bots`)
	if !ospMove(b, `Bots`) {
		return botrow, b.Layout(), false
	}
	page, ok = ospPick(b, `Bots Menu`)
	return botrow, page, ok
}

// ospPicked presses "Add specific bot" once and returns the picker's row once
// it has changed from `before`.
// The picker's row: nothing picked, nothing to pick, or a bot -- with its
// botlib's two letters in front while two botlibs are offered.
const ospPicker = `^(\[SELECT\]|\[NONE AVAILABLE\]|[A-Z0-9]{2}\|.*)$`

func ospPicked(b *playtest.Bot, before string) string {
	if !ospMove(b, `Add specific bot`) {
		return ""
	}
	b.Cmd("invuse")
	for i := 0; i < 30; i++ {
		if r := ospRow(settled(b), ospPicker); r != "" && r != before {
			return r
		}
	}
	return ospRow(b.Layout(), ospPicker)
}

// enter is `join`, resent until the server says so: under the OSP four a client
// arrives as an observer, and an observer's vote is not a voter's.
func enter(srv *playtest.Server, b *playtest.Bot) bool {
	in := regexp.QuoteMeta(b.Name) + ` (entered the game|joined team)`
	for i := 0; i < 10; i++ {
		if len(srv.Grep(in)) > 0 || b.PMType() == playtest.PMNormal {
			b.WaitFrames(5, 3*time.Second)
			return true
		}
		b.Cmd("join")
		b.WaitFrames(8, 4*time.Second)
	}
	return b.PMType() == playtest.PMNormal
}

func runOSP(dir string, port int) {
	const mp = "q2dm1"
	if err := install(dir, mp, ""); err != nil {
		bad("osp/the fixture", err.Error())
		return
	}
	srv := &playtest.Server{
		Binary: cfg.q2, Dir: dir, Game: "colosseum", Map: mp, Port: port,
		MaxClients: 8, LogPath: dir + "/server.log",
		Cvars: map[string]string{
			"g_ruleset": "dm", "bots": "1", "botlibs": "q3,gladiator",
			"vote_enable": "1", "vote_enable_bots": "1", "vote_threshold": "51",
			"vote_bots_max": "8", "match_strictmode": "0", "botfill": "0",
			"minimumplayers": "0", "bots_minplayers": "0",
		},
	}
	if err := srv.Start(); err != nil {
		bad("osp/the server starts", err.Error())
		return
	}
	defer srv.Stop()
	b := playtest.NewBot("osptester", "127.0.0.1", port)
	if err := b.Start(30 * time.Second); err != nil {
		bad("osp/a client connects", err.Error())
		return
	}
	defer b.Disconnect()
	if !check(enter(srv, b), "osp/the client enters the game", "", "it stayed an observer") {
		return
	}

	botrow, l, up := ospBotPage(b)
	check(botrow == "Bots...", "osp/the voting menu's row says Bots",
		fmt.Sprintf("%q", botrow), fmt.Sprintf("%q -- with two botlibs it names neither", botrow))
	title := ospRow(l, `Bots Menu`)
	if !check(up, "osp/...and opens the bot page", "", fmt.Sprintf("rows %q", ospRows(l))) {
		return
	}
	check(title == "[ Bots Menu ]", "osp/its title is [ Bots Menu ]", fmt.Sprintf("%q", title),
		fmt.Sprintf("%q", title))
	check(ospRow(l, `Gladiator`) == "", "osp/no label on it names Gladiator", "",
		fmt.Sprintf("%q", ospRow(l, `Gladiator`)))

	// The list is in `botlibs`' order, so its first bot is a Quake III one.
	picked := ospPicked(b, ospRow(l, ospPicker))
	if !check(strings.HasPrefix(picked, "Q3|"), "osp/the picker's first bot is Q3|<name>",
		fmt.Sprintf("%q", picked), fmt.Sprintf("%q", picked)) {
		return
	}
	name := strings.TrimPrefix(picked, "Q3|")

	n := srv.Len()
	if !check(ospMove(b, `Propose Change`), "osp/the cursor reaches Propose Change", "",
		fmt.Sprintf("cursor on %q", ospCursor(b.Layout()))) {
		return
	}
	b.Cmd("invuse")
	_, err := srv.WaitLogFrom(n, regexp.QuoteMeta(name)+` entered the game`, 20*time.Second)
	prop := ""
	if ps := srv.GrepFrom(n, `Proposal: `); len(ps) > 0 {
		prop = strings.TrimSpace(ps[0])
	}
	check(strings.HasSuffix(prop, "Proposal: Add Q3|"+name+"."),
		"osp/the proposal names the bot and its botlib", fmt.Sprintf("%q", prop),
		fmt.Sprintf("%q", prop))
	check(err == nil, "osp/the vote passes and adds "+name, "entered the game",
		name+" never entered the game")
	line, library := clientLine(dump(srv, "sv botlibdump", `client`), name)
	check(strings.Contains(library, "the q3 botlib"), "osp/...on the Quake III botlib",
		fmt.Sprintf("%q", line), fmt.Sprintf("%q under %q", line, library))

	// ...and a Gladiator bot is GB|<name> in the same picker.
	_, l, up = ospBotPage(b)
	gb := ospRow(l, ospPicker)
	for i := 0; up && i < 8 && !strings.HasPrefix(gb, "GB|"); i++ {
		gb = ospPicked(b, gb)
	}
	check(strings.HasPrefix(gb, "GB|"), "osp/a Gladiator bot is GB|<name>",
		fmt.Sprintf("%q", gb), fmt.Sprintf("last %q", gb))
}
