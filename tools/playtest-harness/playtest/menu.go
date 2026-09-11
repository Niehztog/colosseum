package playtest

import (
	"fmt"
	"regexp"
	"strings"
	"time"
)

// MenuItem is one row of a mod-drawn menu.
type MenuItem struct {
	Text     string
	Selected bool
}

// Menu decodes the menu currently on this client's screen out of the statusbar
// program the server sent.
//
// The wire format is a statusbar layout: quoted strings introduced by string /
// string2 / cstring / cstring2 tokens.  A menu row is a string2 whose text
// begins with "\r" when it is the selected row and " " when it is not -- that
// is the only thing separating a row from the layout's own decorations, so it
// is what the parser keys on.
func (p *Bot) Menu() (title string, items []MenuItem) {
	toks := tokenizeStatusBar(p.StatusBar())
	for _, t := range toks {
		switch t.kind {
		case "cstring2", "cstring":
			if title == "" {
				title = strings.TrimSpace(Decode(t.text))
			}
		case "string2", "string":
			if t.text == "" {
				continue
			}
			switch t.text[0] {
			case '\r':
				items = append(items, MenuItem{Text: strings.TrimSpace(Decode(t.text[1:])), Selected: true})
			case ' ':
				items = append(items, MenuItem{Text: strings.TrimSpace(Decode(t.text[1:]))})
			}
		}
	}
	return title, items
}

// MenuTitle is the title of the menu on screen, or "" if there is none.
func (p *Bot) MenuTitle() string {
	t, _ := p.Menu()
	return t
}

// WaitMenu blocks until a menu whose title matches re is on screen.
func (p *Bot) WaitMenu(re string, timeout time.Duration) error {
	rx := regexp.MustCompile(re)
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if t := p.MenuTitle(); t != "" && rx.MatchString(t) {
			return nil
		}
		time.Sleep(30 * time.Millisecond)
	}
	return fmt.Errorf("%s: no menu titled %q (on screen: %q)", p.Name, re, p.MenuTitle())
}

// MenuPick moves the cursor onto the first row matching re and activates it.
//
// It steps with invnext and re-reads the menu after each step rather than
// counting rows: the mod's own cursor movement skips rows that have no callback
// (blank spacers, plain labels), so a row index computed from what is drawn
// does not match where the cursor will land.
func (p *Bot) MenuPick(re string, timeout time.Duration) error {
	rx := regexp.MustCompile(re)
	deadline := time.Now().Add(timeout)

	for steps := 0; time.Now().Before(deadline); steps++ {
		_, items := p.Menu()
		if len(items) == 0 {
			time.Sleep(50 * time.Millisecond)
			continue
		}
		for _, it := range items {
			if it.Selected && rx.MatchString(it.Text) {
				return p.MenuUse(timeout)
			}
		}
		// is the row even here?
		found := false
		for _, it := range items {
			if rx.MatchString(it.Text) {
				found = true
				break
			}
		}
		if !found {
			return fmt.Errorf("%s: menu %q has no row matching %q (rows: %s)",
				p.Name, p.MenuTitle(), re, strings.Join(rowText(items), " | "))
		}
		if steps > 4*len(items)+8 {
			return fmt.Errorf("%s: cursor never reached %q in menu %q", p.Name, re, p.MenuTitle())
		}

		before := p.StatusBar()
		p.Cmd("invnext")
		p.waitBarChange(before, 2*time.Second)
	}
	return fmt.Errorf("%s: timed out picking %q", p.Name, re)
}

// MenuUse activates the highlighted row.
//
// A mod may throttle menu activations and silently drop the ones that arrive
// too soon -- Rocket Arena ignores an invuse within five frames of the last
// one, which is enough to swallow every step of a scripted join.  So confirm
// the screen actually changed and send again if it did not, rather than
// assuming the command took effect.
func (p *Bot) MenuUse(timeout time.Duration) error {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		before := p.StatusBar()
		p.Cmd("invuse")
		if p.waitBarChange(before, 1500*time.Millisecond) {
			return nil
		}
	}
	return fmt.Errorf("%s: menu %q did not respond to invuse", p.Name, p.MenuTitle())
}

// waitBarChange blocks until the statusbar differs from before, so the next
// step reads the menu as the server redrew it rather than as it was.  Reports
// whether it changed.
func (p *Bot) waitBarChange(before string, timeout time.Duration) bool {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if p.StatusBar() != before {
			return true
		}
		time.Sleep(20 * time.Millisecond)
	}
	return false
}

func rowText(items []MenuItem) []string {
	out := make([]string, 0, len(items))
	for _, it := range items {
		out = append(out, it.Text)
	}
	return out
}

type sbToken struct {
	kind string
	text string
}

// tokenizeStatusBar pulls the quoted operands out of a statusbar program,
// tagged with the token that introduced them.
func tokenizeStatusBar(s string) []sbToken {
	var out []sbToken
	i := 0
	for i < len(s) {
		// next word
		for i < len(s) && s[i] == ' ' {
			i++
		}
		j := i
		for j < len(s) && s[j] != ' ' {
			j++
		}
		word := s[i:j]
		i = j

		switch word {
		case "string", "string2", "cstring", "cstring2":
		default:
			continue
		}
		// its operand is the next quoted run
		for i < len(s) && s[i] != '"' {
			i++
		}
		if i >= len(s) {
			return out
		}
		i++ // opening quote
		k := strings.IndexByte(s[i:], '"')
		if k < 0 {
			return out
		}
		out = append(out, sbToken{kind: word, text: s[i : i+k]})
		i += k + 1
	}
	return out
}
