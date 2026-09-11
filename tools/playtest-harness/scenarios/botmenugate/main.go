// botmenugate -- who may open the bot menu, and who may not?
//
// The bot menu gates `menu` on the RCON PASSWORD rather than on
// `serveronlybotcmds`, which is the 1999 shape: the password is typed as
// argument 1 of the client command and compared with strcmp.  That gate decides
// who may add and remove bots, because the menu's own rows reach the bot
// commands through BotServerCommand -- i.e. as CONSOLE commands -- so anybody
// who gets the menu open has bot management whatever `serveronlybotcmds` says.
//
// Three things it asserts, each in both signs:
//
//  1. A SERVER THAT NEVER SET A PASSWORD REFUSES EVERYBODY.  An unset
//     `rcon_password` is the empty string, and `menu ""` arrives as argc 2 with
//     an empty argv(1) -- q2pro's Cmd_TokenizeString registers the argument
//     before it parses the quotes -- so the donor's bare strcmp MATCHED and two
//     quote marks were a way in for any client on the server.  Measured that
//     way before the fix; the `listkeys ""` probe below is what proves the empty
//     argument reaches the library at all.
//  2. A SERVER THAT SET ONE HONOURS IT, in all four directions: no argument, an
//     empty argument, a wrong password, the right password.
//  3. THE HOST EXEMPTION IS NOT READ LIVE.  It is `pers.listenhost`, latched in
//     ClientConnect from the userinfo the ENGINE force-set.  A client that
//     pushes `ip=loopback` into its own userinfo afterwards -- one `setu` away
//     for anybody -- must still be refused.
//
// WHAT THIS SCENARIO CANNOT SEE: the host of a listen server, which is what the
// exemption is for.  A dedicated server has no local client, and a libq2 client
// is a real UDP peer whose address is 127.0.0.1 and never NA_LOOPBACK.  That arm
// needs the q2pro CLIENT hosting a map; Colosseum's tools/playtest.sh documents
// the listen-server drive that covers it.  The 127.0.0.1 client here is the
// control for it: a client on the same machine is still a client.
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

var pass, fail int

func ok(what, detail string)  { pass++; fmt.Printf("  [ ok ] %-52s %s\n", what, detail) }
func bad(what, detail string) { fail++; fmt.Printf("  [FAIL] %-52s %s\n", what, detail) }

// waitAfter waits for a print matching re that arrived AFTER mark.  Bot.Prints
// is the whole history and Bot.WaitPrint scans all of it, so a check written on
// WaitPrint would be satisfied by the refusal three checks ago -- which is how a
// gate that had stopped refusing anything would still pass.
func waitAfter(b *playtest.Bot, mark int, re string, d time.Duration) (string, bool) {
	rx := regexp.MustCompile(re)
	deadline := time.Now().Add(d)
	for {
		all := b.Prints()
		if mark > len(all) {
			mark = len(all)
		}
		for _, s := range all[mark:] {
			if rx.MatchString(s) {
				return s, true
			}
		}
		if !time.Now().Before(deadline) {
			return "", false
		}
		time.Sleep(25 * time.Millisecond)
	}
}

var refDir, ctfDir string

type server struct {
	srv *playtest.Server
	b   *playtest.Bot
	// The layout as it was before anything was typed.  `arena` has its own join
	// menu up on spawn, so the question is never "is a menu up" but "did the BOT
	// menu appear since the baseline" -- reading the wrong channel is what made
	// this scenario's first three runs report a defect that was not there.  THE
	// BOT MENU DRAWS ON THE LAYOUT, NOT THE STATUSBAR.
	baseline string
}

func (s *server) botMenuUp() bool {
	l := s.b.Layout()
	return l != s.baseline && strings.Contains(strings.ToLower(l), "bot")
}

func (s *server) mark() int { return len(s.b.Prints()) }

func (s *server) stop() {
	s.b.Disconnect()
	s.srv.Stop()
}

func boot(q2, lib, dir, rs, mp, glad string, port int, rcon string) *server {
	if err := colosseum.Install(dir, refDir, ctfDir, lib); err != nil {
		panic(err)
	}
	if glad != "" {
		if err := colosseum.InstallBrain(dir, glad); err != nil {
			panic(err)
		}
	}
	cv := map[string]string{"g_ruleset": rs, "bots": "1"}
	if rcon != "" {
		cv["rcon_password"] = rcon
	}
	srv := &playtest.Server{
		Binary: q2, Dir: dir, Game: "colosseum", Map: mp, Port: port,
		MaxClients: 8, Cvars: cv, LogPath: dir + "/server.log",
	}
	if err := srv.Start(); err != nil {
		panic(err)
	}
	b := playtest.NewBot("gatetester", "127.0.0.1", port)
	if err := b.Start(30 * time.Second); err != nil {
		srv.Stop()
		panic(err)
	}
	time.Sleep(2 * time.Second)
	return &server{srv: srv, b: b, baseline: b.Layout()}
}

func main() {
	q2 := flag.String("q2proded", "", "")
	lib := flag.String("lib", "", "")
	ref := flag.String("ref", "/usr/share/games/quake2/baseq2", "")
	ctf := flag.String("ctf", "/usr/share/games/quake2/ctf", "")
	dir := flag.String("dir", "/tmp/colosseum-botmenugate", "")
	port := flag.Int("port", 27993, "first of two ports; the second is +1")
	rcon := flag.String("rcon", "secretpw", "rcon_password for the second server")
	rs := flag.String("ruleset", "ctf", "")
	glad := flag.String("gladdir", "", "")
	mp := flag.String("map", "", "default: the ruleset's own map")
	flag.Parse()
	refDir, ctfDir = *ref, *ctf

	r, found := colosseum.Find(*rs)
	if !found {
		fmt.Printf("botmenugate: no such ruleset %q\n", *rs)
		os.Exit(2)
	}
	m := *mp
	if m == "" {
		m = r.Map
	}

	// UNDER THE OSP FOUR THERE IS NOTHING HERE TO TEST.  g_cmds.c consults
	// OSP_ClientCommand first and osp_clientcmd.c routes `menu` to Cmd_Inven_f,
	// so bot_MenuToggle is unreachable from a client under dm/dmpro/tdm/duel.
	// Say so rather than reporting failures that mean "the command went
	// somewhere else".
	if colosseum.IsOSP(*rs) {
		fmt.Printf("  [skip] %s routes `menu` to OSP's inventory before BotCmd;"+
			" bot_MenuToggle is unreachable from a client here\n", *rs)
		fmt.Printf("\n0 check(s), 0 failed\n")
		return
	}

	// ---- A. a server that never set an rcon password ---------------------
	fmt.Printf("=== server A: rcon_password unset, g_ruleset %s, %s\n", *rs, m)
	a := boot(*q2, *lib, *dir+"/a", *rs, m, *glad, *port, "")
	defer a.stop()

	for _, l := range a.srv.Grep("ruleset ") {
		fmt.Printf("       | %s\n", l)
	}

	// The console is not a client, so the console cannot open a menu at all.
	// It is here because it is the other half of "the host had no way in": what
	// a listen server's host holds is CONSOLE authority, and this is what the
	// console gets.
	n := a.srv.Len()
	a.srv.Console("sv menu")
	time.Sleep(1 * time.Second)
	if len(a.srv.GrepFrom(n, "only clients can open the menu")) > 0 {
		ok("console/`sv menu` refuses the console itself", "\"only clients can open the menu\"")
	} else {
		bad("console/`sv menu` refuses the console itself",
			fmt.Sprintf("expected that print; got %q", a.srv.GrepFrom(n, ".")))
	}

	// (arena only) WHAT DOES THE SERVER ACTUALLY RECEIVE?  `listkeys` loops
	// `for i = 1; i < gi.argc(); i++` and prints "Block not found: <argv(i)>",
	// so with argc==1 it prints the whole key list instead.  That distinguishes
	// "the quotes produced an empty argv[1]" from "the quotes produced no token
	// at all", which is the entire reason check A2 has to exist.
	if *rs == "arena" {
		mk := a.mark()
		a.b.Cmd(`listkeys ""`)
		if l, got := waitAfter(a.b, mk, "Block not found", 5*time.Second); got {
			ok("A/probe: `listkeys \"\"` yields an empty argv[1]",
				fmt.Sprintf("%q -- argc==2, argv(1)==\"\"", strings.TrimSpace(l)))
		} else {
			bad("A/probe: `listkeys \"\"` yields an empty argv[1]",
				"no \"Block not found\" -- argc==1, the quotes produced NO token")
		}
	}

	// A1: the honest form.  The refusal must name the missing password rather
	// than ask for one the server has not got.
	mk := a.mark()
	a.b.Cmd("menu")
	if l, got := waitAfter(a.b, mk, "has not set one", 5*time.Second); got {
		ok("A/bare `menu` says the server set no password", fmt.Sprintf("%q", strings.TrimSpace(l)))
	} else if _, got := waitAfter(a.b, mk, "need rcon password", 1*time.Second); got {
		bad("A/bare `menu` says the server set no password",
			"refused with \"need rcon password\" -- asking for a password that does not exist")
	} else {
		bad("A/bare `menu` says the server set no password", "no refusal at all -- the gate is off")
	}

	// A2: THE HOLE.  Two quote marks used to open the menu here.
	mk = a.mark()
	a.b.Cmd(`menu ""`)
	time.Sleep(3 * time.Second)
	opened := a.botMenuUp()
	_, refused := waitAfter(a.b, mk, "has not set one|need rcon password", 1*time.Second)
	switch {
	case opened:
		bad("A/`menu \"\"` is refused", "THE BOT MENU OPENED on an empty password")
	case refused:
		ok("A/`menu \"\"` is refused", "refused, and no menu on the layout")
	default:
		bad("A/`menu \"\"` is refused", "no menu and no refusal -- inconclusive, see server.log")
	}

	// A3: a wrong password is refused too, so A1/A2 are not "everything is
	// refused because the command never arrived".
	mk = a.mark()
	a.b.Cmd("menu wrongpassword")
	if _, got := waitAfter(a.b, mk, "has not set one|need rcon password", 5*time.Second); got {
		ok("A/`menu wrongpassword` is refused", "refused")
	} else {
		bad("A/`menu wrongpassword` is refused", "no refusal print")
	}

	// A4: THE SPOOF.  `ip` is the engine's only in the connect packet; a full
	// userinfo update replaces the whole string and a delta writes any key at
	// all, unfiltered.  So a client can claim to be the loopback client, and the
	// exemption has to be reading the value latched at connect instead.
	a.b.SetUserinfo("ip", "loopback")
	time.Sleep(2 * time.Second)
	mk = a.mark()
	a.b.Cmd("menu")
	time.Sleep(2 * time.Second)
	_, refused = waitAfter(a.b, mk, "has not set one|need rcon password", 3*time.Second)
	if refused && !a.botMenuUp() {
		ok("A/a client claiming `ip=loopback` is still refused", "refused; the latch is not read live")
	} else {
		bad("A/a client claiming `ip=loopback` is still refused",
			"THE HOST EXEMPTION IS SPOOFABLE from any client's userinfo")
	}

	if a.botMenuUp() {
		bad("A/nothing opened the menu on server A", "the bot menu is on the layout")
	} else {
		ok("A/nothing opened the menu on server A", "no bot menu on the layout")
	}

	// ---- B. a server that did set one ------------------------------------
	fmt.Printf("=== server B: rcon_password %q\n", *rcon)
	b := boot(*q2, *lib, *dir+"/b", *rs, m, *glad, *port+1, *rcon)
	defer b.stop()

	for _, c := range []struct{ cmd, what string }{
		{"menu", "B/bare `menu` is refused"},
		{`menu ""`, "B/`menu \"\"` is refused"},
		{"menu wrongpassword", "B/`menu wrongpassword` is refused"},
	} {
		mk := b.mark()
		b.b.Cmd("%s", c.cmd)
		if l, got := waitAfter(b.b, mk, "need rcon password", 5*time.Second); got {
			ok(c.what, fmt.Sprintf("%q", strings.TrimSpace(l)))
		} else {
			bad(c.what, "no \"need rcon password\" refusal")
		}
	}
	if b.botMenuUp() {
		bad("B/none of the three opened it", "the bot menu is on the layout")
	} else {
		ok("B/none of the three opened it", "no bot menu on the layout")
	}

	// B4: the positive.  Without it, every "refused" above could equally mean
	// the command never reached bot_MenuToggle.
	b.b.Cmd("menu %s", *rcon)
	time.Sleep(3 * time.Second)
	if b.botMenuUp() {
		ok("B/the right password opens it", "the bot menu is on the layout")
	} else {
		bad("B/the right password opens it",
			fmt.Sprintf("nothing appeared; layout=%.90q", b.b.Layout()))
	}

	// B5: and the toggle closes it with NO password, because the menu_owner test
	// runs before the gate -- the gate is on opening, not on closing.
	b.b.Cmd("menu")
	time.Sleep(3 * time.Second)
	if !b.botMenuUp() {
		ok("B/bare `menu` closes what it may not open", "the layout is back to the baseline")
	} else {
		bad("B/bare `menu` closes what it may not open", "the menu is still up")
	}

	fmt.Printf("  [skip] the listen server's host is not observable from a dedicated\n" +
		"         server: this client is a real UDP peer at 127.0.0.1, never\n" +
		"         NA_LOOPBACK.  See tools/playtest.sh's botmenugate notes.\n")

	fmt.Printf("\n%d check(s), %d failed\n", pass+fail, fail)
	if fail > 0 {
		os.Exit(1)
	}
}
