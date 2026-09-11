package playtest

import (
	"fmt"
	"io"
	"log"
	"regexp"
	"strings"
	"sync"
	"time"

	"github.com/packetflinger/libq2/bot"
	"github.com/packetflinger/libq2/message"
	pl "github.com/packetflinger/libq2/player"
	pb "github.com/packetflinger/libq2/proto"
)

// CSStatusBar is CS_STATUSBAR, the configstring a mod's statusbar program (and
// so any menu drawn with one) arrives in -- and it arrives in MORE THAN ONE.
//
// A configstring on the wire holds at most MAX_QPATH bytes, so the server
// splits a statusbar program across consecutive slots: 5, 6, 7 ... up to
// CS_AIRACCEL.  Reading slot 5 alone gets the first 64 bytes and silently
// truncates everything after, which for a mod's menu means every row past the
// first two or three is invisible.  StatusBar() rejoins them.
const (
	CSStatusBar    = 5
	CSStatusBarEnd = 29 // CS_AIRACCEL: the first slot that is not the bar
	CSChunk        = 64 // MAX_QPATH; a shorter chunk is the last one
)

// Bot is one headless client.
type Bot struct {
	Name string

	b        *bot.Bot
	mu       sync.Mutex
	prints   []string
	centers  []string
	stuffs   []string
	cs       map[int]string
	layout   string
	origin   [3]float64
	frame    int
	spawned  chan struct{}
	once     sync.Once
	err      error
	pmtype   int
	pmflags  int
	pmtime   int
	view     [3]float64
	kick     [3]float64
	gunframe int
	gunhigh  int
	stats    map[int]int
	sounds   []Sound
}

// Playerstate stat slots that mean the same thing in every mod, and the
// STAT_LAYOUTS bits -- which are what decide whether a layout the server
// unicast is one the client draws at all.
const (
	StatLayouts      = 13
	LayoutsLayout    = 1 << 0
	LayoutsInventory = 1 << 1
)

// pmove flag bits (PMF_*).
const (
	PMFDucked        = 1 << 0
	PMFJumpHeld      = 1 << 1
	PMFOnGround      = 1 << 2
	PMFTimeWaterjump = 1 << 3
	PMFTimeLand      = 1 << 4
	PMFTimeTeleport  = 1 << 5
	PMFNoPrediction  = 1 << 6
)

// Quake II usercmd button bits.
const (
	ButtonAttack = 1 << 0
	ButtonUse    = 1 << 1
)

// Quake II pmove types, as the server reports them in the playerstate.  This
// is the authoritative way to tell an observer from a fighter: a mod that puts
// a spectator on MOVETYPE_NOCLIP shows up here as PMSpectator, whatever its
// own HUD happens to say.
const (
	PMNormal = iota
	PMSpectator
	PMDead
	PMGib
	PMFreeze
)

// NewBot builds a client that will connect to host:port as name.
func NewBot(name, host string, port int) *Bot {
	ui := pl.NewUserinfo()
	ui["name"] = name
	ui["skin"] = "male/grunt"
	ui["hand"] = "0"
	ui["rate"] = "25000"
	ui["msg"] = "1"
	ui["fov"] = "90"
	ui["spectator"] = "0"

	p := &Bot{Name: name, spawned: make(chan struct{}), cs: map[int]string{},
		stats: map[int]int{}}
	p.b = &bot.Bot{
		Net:     bot.Connection{Address: host, Port: port},
		User:    ui,
		Version: "q2playtest",
	}

	p.b.RegisterCallback(message.CallbackOnBegin, func(_ any, _ *message.Buffer) {
		p.once.Do(func() { close(p.spawned) })
	})
	p.b.RegisterCallback(message.SVCPrint, func(a any, _ *message.Buffer) {
		if pr, ok := a.(*pb.Print); ok {
			p.mu.Lock()
			p.prints = append(p.prints, Decode(pr.GetData()))
			p.mu.Unlock()
		}
	})
	p.b.RegisterCallback(message.SVCConfigString, func(a any, _ *message.Buffer) {
		cs, ok := a.(*pb.ConfigString)
		if !ok {
			return
		}
		p.mu.Lock()
		p.cs[int(cs.GetIndex())] = cs.GetData()
		p.mu.Unlock()
	})
	// A SOUND IS THE FIFTH CHANNEL, and for some mod behaviour it is the only
	// evidence there is.  A death scream, a pickup, an announcer cue: nothing
	// reaches a print, a layout, a centerprint or a stat.  Colosseum's tourney
	// gates BOTH of player_die's death sounds on `sync_stat != 2` so a match
	// does not begin on a chorus of screams -- the guard is a donor line whose
	// absence no static sweep can see, and this is what settles whether it is
	// there.
	//
	// The index is into the CS_SOUNDS block, whose base moves with the
	// configstring layout, so what is kept is the raw triple and the CHANNEL is
	// what a scenario asserts on: CHAN_VOICE from a client entity is a player
	// making a player noise, whatever the layout.
	p.b.RegisterCallback(message.SVCSound, func(a any, _ *message.Buffer) {
		sn, ok := a.(*pb.PackedSound)
		if !ok {
			return
		}
		p.mu.Lock()
		p.sounds = append(p.sounds, Sound{
			Index:   int(sn.GetIndex()),
			Entity:  int(sn.GetEntity()),
			Channel: int(sn.GetChannel()),
			Frame:   p.frame,
		})
		p.mu.Unlock()
	})
	// A stufftext is a console command the SERVER typed into this client, and
	// it is the only channel a mod has for anything that must live client-side
	// -- a `bind`, an `alias`, a `play`.  A feature whose whole client half is
	// an alias (Threewave's offhand hook: `alias +hook hookon`) is unreachable
	// if this never arrives, and no other channel can say so.
	p.b.RegisterCallback(message.SVCStuffText, func(a any, _ *message.Buffer) {
		if st, ok := a.(*pb.StuffText); ok {
			p.mu.Lock()
			p.stuffs = append(p.stuffs, Decode(st.GetData()))
			p.mu.Unlock()
		}
	})
	p.b.RegisterCallback(message.SVCLayout, func(a any, _ *message.Buffer) {
		if l, ok := a.(*pb.Layout); ok {
			p.mu.Lock()
			p.layout = l.GetData()
			p.mu.Unlock()
		}
	})
	p.b.RegisterCallback(message.SVCCenterPrint, func(a any, _ *message.Buffer) {
		if c, ok := a.(*pb.CenterPrint); ok {
			p.mu.Lock()
			p.centers = append(p.centers, Decode(c.GetData()))
			p.mu.Unlock()
		}
	})
	p.b.RegisterCallback(message.SVCFrame, func(a any, _ *message.Buffer) {
		fr, ok := a.(*pb.Frame)
		if !ok {
			return
		}
		ps := fr.GetPlayerState()
		if ps == nil || ps.GetMovestate() == nil {
			return
		}
		m := ps.GetMovestate()
		p.mu.Lock()
		p.frame = int(fr.GetNumber())
		p.pmtype = int(m.GetType())
		p.pmflags = int(m.GetFlags())
		p.pmtime = int(m.GetTime())
		// pmove origins are in eighths of a unit
		p.origin = [3]float64{
			float64(m.GetOriginX()) / 8,
			float64(m.GetOriginY()) / 8,
			float64(m.GetOriginZ()) / 8,
		}
		// ps.viewangles is a short per axis (SHORT2ANGLE) and ps.kick_angles
		// a signed char at quarter-degree resolution, which is the whole
		// dynamic range a mod's view kick has to live inside.
		p.view = [3]float64{
			float64(ps.GetViewAnglesX()) * (360.0 / 65536.0),
			float64(ps.GetViewAnglesY()) * (360.0 / 65536.0),
			float64(ps.GetViewAnglesZ()) * (360.0 / 65536.0),
		}
		p.kick = [3]float64{
			float64(ps.GetKickAnglesX()) * 0.25,
			float64(ps.GetKickAnglesY()) * 0.25,
			float64(ps.GetKickAnglesZ()) * 0.25,
		}
		// ps.gunframe is the one thing on the wire that says a weapon THOUGHT.
		// Only the weaponthink moves it -- through the activate frames, round
		// the idle loop, and to FRAME_FIRE_FIRST on a shot -- so a client whose
		// gunframe never leaves zero is one whose weaponthink returned early
		// every frame.  It is delta-compressed like everything else here, and
		// carried in the same block as the gun offsets, so a frame that did not
		// resend it decodes as zero; GunFrameHigh keeps the running maximum,
		// which is the half that survives that and the half a "did it fire"
		// question actually wants.
		p.gunframe = int(ps.GetGunFrame())
		if p.gunframe > p.gunhigh {
			p.gunhigh = p.gunframe
		}
		// stats[] is DELTA-COMPRESSED against the last acked frame, so a frame
		// carries only the slots that changed.  Merging rather than replacing is
		// what makes Stat(n) mean "the last value the server sent for n" instead
		// of "a value it happened to resend this frame" -- replacing would read
		// as 0 on every frame a stat held steady, which for a stat that is set
		// once on pickup is almost every frame.
		for k, v := range ps.GetStats() {
			p.stats[int(k)] = int(v)
		}
		p.mu.Unlock()
	})
	return p
}

// Start connects the bot and blocks until the server has spawned it in.
func (p *Bot) Start(timeout time.Duration) error {
	// libq2's bot logs the handshake to the standard logger; keep the harness
	// output readable
	log.SetOutput(io.Discard)
	go func() {
		if err := p.b.Run(); err != nil {
			p.mu.Lock()
			p.err = err
			p.mu.Unlock()
		}
	}()
	select {
	case <-p.spawned:
		return nil
	case <-time.After(timeout):
		p.mu.Lock()
		defer p.mu.Unlock()
		if p.err != nil {
			return fmt.Errorf("%s: %w", p.Name, p.err)
		}
		return fmt.Errorf("%s: never spawned in", p.Name)
	}
}

// Cmd sends a console command to the server, as if the player typed it.
func (p *Bot) Cmd(format string, a ...any) {
	p.b.AddClientString(format+"\n", a...)
}

// Origin is the bot's last known position.
func (p *Bot) Origin() [3]float64 {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.origin
}

// Frame is the last server frame the bot saw, which is how a scenario waits a
// fixed number of game frames rather than guessing at wall-clock sleeps.
func (p *Bot) Frame() int {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.frame
}

// WaitFrames blocks until n more server frames have arrived.
func (p *Bot) WaitFrames(n int, timeout time.Duration) error {
	target := p.Frame() + n
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if p.Frame() >= target {
			return nil
		}
		time.Sleep(20 * time.Millisecond)
	}
	return fmt.Errorf("%s: only reached frame %d, wanted %d", p.Name, p.Frame(), target)
}

// PMType is the pmove type the server last reported for this client.
func (p *Bot) PMType() int {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.pmtype
}

// PMFlags is the pmove flag word the server last reported (PMF_*).  It is the
// only witness to a mod that sets a timed pmove flag on a client whose pmove
// type never runs the countdown that clears it: PM_SPECTATOR and PM_FREEZE both
// return from Pmove() above the countdown, so PMF_TIME_TELEPORT left on one of
// them pins PITCH and ROLL to zero for good.
func (p *Bot) PMFlags() int {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.pmflags
}

// PMTime is the pmove timer byte that goes with PMFlags.
func (p *Bot) PMTime() int {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.pmtime
}

// GunFrame is the view-weapon animation frame the server last sent.
func (p *Bot) GunFrame() int {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.gunframe
}

// GunFrameHigh is the highest gunframe seen since connect.  Zero means the
// weaponthink has not run once -- which for a mod that gates firing on a
// player state (observing, dead, a round not started) is the check that the
// gate is really in the weapon path and not just in the HUD.
func (p *Bot) GunFrameHigh() int {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.gunhigh
}

// ViewAngles is playerstate.viewangles in degrees -- PITCH, YAW, ROLL.  For a
// client the server has on PM_FREEZE or PM_DEAD this is what the client
// actually renders with, because prediction is off and the interpolated server
// value is the only source there is.
func (p *Bot) ViewAngles() [3]float64 {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.view
}

// KickAngles is playerstate.kick_angles in degrees.  The client ADDS these to
// the view angles before rendering, so a mod that leaves a kick on a camera --
// weapon kick, damage kick, or the run/bob terms SV_CalcViewOffset computes
// from ent->velocity -- tilts the horizon by exactly this much.
func (p *Bot) KickAngles() [3]float64 {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.kick
}

// Look points the client's usercmd angles at pitch/yaw/roll (degrees), the way
// a mouse would.  The angles stay where they are put until the next call, which
// is what lets a scenario sweep a view across a mod's camera code.
func (p *Bot) Look(pitch, yaw, roll float64) {
	p.b.MoveMu.Lock()
	p.b.Move.Angles = [3]int16{angleShort(pitch), angleShort(yaw), angleShort(roll)}
	p.b.MoveMu.Unlock()
}

func angleShort(deg float64) int16 {
	return int16(int32(deg*(65536.0/360.0)) & 65535)
}

// Spectating reports whether the server has this client on a spectator pmove,
// which is what a mod's observer/free-flying mode comes out as.
func (p *Bot) Spectating() bool {
	return p.PMType() == PMSpectator
}

// Stat is the value the server last sent in playerstate.stats[n], or 0 if it
// has never sent one.
//
// This is the only channel that settles what a mod's stat map actually DID.  A
// mod's own diagnostic reports the map it believes in, and a HUD that looks
// right proves the bar and the writes agree with each other -- neither shows
// which slot a value landed in.  Reading the number off the wire does, which is
// how the tourney rune bug was pinned down: `sv slots` said the runes were at
// 22..26 and it was telling the truth, while the pickup wrote nothing at all
// and the gameplay code read 28..32.
//
// Slot numbers are the MOD's, per its own map -- there is no portable meaning
// above 15 -- so a scenario should read them from the mod's diagnostic rather
// than hardcoding them. colosseum.Slots.ByName does that.
func (p *Bot) Stat(n int) int {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.stats[n]
}

// Stats is a copy of every slot the server has sent so far, for a scenario that
// wants to assert on the shape of the whole array (for example that nothing
// outside the mod's declared slots is ever written).
func (p *Bot) Stats() map[int]int {
	p.mu.Lock()
	defer p.mu.Unlock()
	out := make(map[int]int, len(p.stats))
	for k, v := range p.stats {
		out[k] = v
	}
	return out
}

// WaitStat blocks until stats[n] == want, which is the honest way to wait on a
// pickup: the grant happens in the game frame after the command, and the value
// reaches this client one frame later still.
func (p *Bot) WaitStat(n, want int, timeout time.Duration) (int, error) {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if got := p.Stat(n); got == want {
			return got, nil
		}
		time.Sleep(20 * time.Millisecond)
	}
	return p.Stat(n), fmt.Errorf("%s: stats[%d] is %d, wanted %d",
		p.Name, n, p.Stat(n), want)
}

// Press holds the given BUTTON_* bits down for `hold`, then releases them, and
// waits a beat so the release reaches the server.  Mods latch buttons on the
// EDGE (`latched_buttons |= buttons & ~oldbuttons`), so a button that is never
// released fires exactly once and a button that is never pressed for a whole
// frame may not fire at all.
func (p *Bot) Press(buttons byte, hold time.Duration) {
	p.b.MoveMu.Lock()
	p.b.Move.Buttons |= buttons
	p.b.MoveMu.Unlock()
	time.Sleep(hold)
	p.b.MoveMu.Lock()
	p.b.Move.Buttons &^= buttons
	p.b.MoveMu.Unlock()
	time.Sleep(hold)
}

// Nudge holds upmove (jump when positive, crouch when negative) the same way.
// It is how an RA2 observer cycles who it is watching.
func (p *Bot) Nudge(up int16, hold time.Duration) {
	p.b.MoveMu.Lock()
	p.b.Move.UpMove = up
	p.b.MoveMu.Unlock()
	time.Sleep(hold)
	p.b.MoveMu.Lock()
	p.b.Move.UpMove = 0
	p.b.MoveMu.Unlock()
	time.Sleep(hold)
}

// Walk holds forwardmove/sidemove down for `hold`, then releases them and waits
// a beat, the way Nudge holds upmove.  400 is what a real client sends for a
// held movement key.  It is how you tell a player who is merely standing on
// another one from a player who is trapped inside them: the trapped one's
// pmove finds itself allsolid every frame and the origin never changes.
func (p *Bot) Walk(forward, side int16, hold time.Duration) {
	p.b.MoveMu.Lock()
	p.b.Move.ForwardMove = forward
	p.b.Move.SideMove = side
	p.b.MoveMu.Unlock()
	time.Sleep(hold)
	p.b.MoveMu.Lock()
	p.b.Move.ForwardMove = 0
	p.b.Move.SideMove = 0
	p.b.MoveMu.Unlock()
	time.Sleep(200 * time.Millisecond)
}

// Disconnect drops the client the way a real one leaves, so the game sees a
// disconnect rather than a timeout.
func (p *Bot) Disconnect() {
	p.Cmd("disconnect")
	time.Sleep(300 * time.Millisecond)
	if p.b.Net.Conn != nil {
		p.b.Net.Conn.Close()
	}
}

// Prints returns every server print this client received.
func (p *Bot) Prints() []string {
	p.mu.Lock()
	defer p.mu.Unlock()
	return append([]string(nil), p.prints...)
}

// WaitPrint blocks until one of this client's prints matches re.
func (p *Bot) WaitPrint(re string, timeout time.Duration) (string, error) {
	rx := regexp.MustCompile(re)
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		for _, s := range p.Prints() {
			if rx.MatchString(s) {
				return s, nil
			}
		}
		time.Sleep(20 * time.Millisecond)
	}
	return "", fmt.Errorf("%s: no print matched %q", p.Name, re)
}

// StatusBar is the statusbar program last sent to this client, rejoined from
// the configstring slots the server split it across.
//
// The join stops at the first chunk shorter than MAX_QPATH, because that chunk
// carries the string's terminator: a mod that replaces a long bar with a short
// one leaves the old tail behind in the later slots, and reading past the
// terminator would resurrect it.
func (p *Bot) StatusBar() string {
	p.mu.Lock()
	defer p.mu.Unlock()
	var b strings.Builder
	for i := CSStatusBar; i < CSStatusBarEnd; i++ {
		c, ok := p.cs[i]
		if !ok {
			break
		}
		b.WriteString(c)
		if len(c) < CSChunk {
			break
		}
	}
	return b.String()
}

// Decode strips Quake II's high-bit "green text" encoding so a string compares
// as plain ASCII.  Mods set the high bit to draw alternate glyphs, which is how
// an unselected menu row differs from the selected one on the wire.
//
// It masks runes, not bytes.  libq2 reads a message string one byte at a time
// through string(byte), which Go treats as a rune -- so every byte over 0x7f
// arrives as a two-byte UTF-8 sequence.  Masking the bytes of that turns "i"
// with the high bit set into "C" plus a stray, which is what a wrongly-decoded
// menu row looks like.
func Decode(s string) string {
	out := make([]byte, 0, len(s))
	for _, r := range s {
		out = append(out, byte(r&0x7f))
	}
	return string(out)
}

// Sound is one svc_sound this client received.  Channel is the Q2 channel
// (CHAN_AUTO 0, CHAN_WEAPON 1, CHAN_VOICE 2, CHAN_ITEM 3, CHAN_BODY 4) and
// Entity is the edict that made it -- 1..maxclients is a player.
type Sound struct {
	Index   int
	Entity  int
	Channel int
	Frame   int // the server frame this client was on when it arrived
}

// Sounds returns every sound this client has been sent.
func (p *Bot) Sounds() []Sound {
	p.mu.Lock()
	defer p.mu.Unlock()
	return append([]Sound(nil), p.sounds...)
}

// VoiceSounds counts the sounds on CHAN_VOICE made by a client entity -- the
// channel player pain and death noises use.
func (p *Bot) VoiceSounds() int {
	n := 0
	for _, s := range p.Sounds() {
		if s.Channel == 2 && s.Entity >= 1 && s.Entity <= 64 {
			n++
		}
	}
	return n
}

// ConfigString returns configstring n as this client last received it.
//
// Every configstring is kept, not just the statusbar, because on a mod that
// remaps the configstring space the INDEX is itself the evidence: a skin
// arriving at 1312 says the server is on the old layout, at 12862 the extended
// one, and a mod that hardcodes a compile-time CS_ constant writes to the
// wrong one of those two.
func (p *Bot) ConfigString(n int) string {
	p.mu.Lock()
	defer p.mu.Unlock()
	return p.cs[n]
}

// ConfigStrings returns a copy of every configstring seen so far.
func (p *Bot) ConfigStrings() map[int]string {
	p.mu.Lock()
	defer p.mu.Unlock()
	out := make(map[int]string, len(p.cs))
	for k, v := range p.cs {
		out[k] = v
	}
	return out
}

// WaitConfigString blocks until configstring n matches re.
func (p *Bot) WaitConfigString(n int, re string, timeout time.Duration) (string, error) {
	rx := regexp.MustCompile(re)
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if s := p.ConfigString(n); rx.MatchString(s) {
			return s, nil
		}
		time.Sleep(20 * time.Millisecond)
	}
	return "", fmt.Errorf("%s: configstring %d never matched %q (is %q)", p.Name, n, re, p.ConfigString(n))
}

// PlayerSkin returns this client's own entry in the player configstring block,
// which is "name\\model/skin" -- the half after the backslash is what a team
// mod overwrites when the player joins a team.  base is the first player
// configstring, which differs between the old and extended layouts, so the
// caller passes the one its server is using.
func (p *Bot) PlayerSkin(base, slot int) string {
	return p.ConfigString(base + slot)
}

// Stuffs returns every console command the server has stuffed into this client.
func (p *Bot) Stuffs() []string {
	p.mu.Lock()
	defer p.mu.Unlock()
	return append([]string(nil), p.stuffs...)
}

// WaitStuff blocks until a stuffed command matches re, and returns it.
func (p *Bot) WaitStuff(re string, timeout time.Duration) (string, error) {
	rx := regexp.MustCompile(re)
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		for _, s := range p.Stuffs() {
			if rx.MatchString(s) {
				return s, nil
			}
		}
		time.Sleep(20 * time.Millisecond)
	}
	return "", fmt.Errorf("%s: no stuffed command matched %q", p.Name, re)
}

// Layout is the last scoreboard/layout string the server sent this client.
func (p *Bot) Layout() string {
	p.mu.Lock()
	defer p.mu.Unlock()
	return Decode(p.layout)
}

// WaitLayout blocks until the layout matches re, and returns it.
func (p *Bot) WaitLayout(re string, timeout time.Duration) (string, error) {
	rx := regexp.MustCompile(re)
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		if s := p.Layout(); rx.MatchString(s) {
			return s, nil
		}
		time.Sleep(20 * time.Millisecond)
	}
	return "", fmt.Errorf("%s: no layout matched %q", p.Name, re)
}

// Centers returns every centerprint this client received.
func (p *Bot) Centers() []string {
	p.mu.Lock()
	defer p.mu.Unlock()
	return append([]string(nil), p.centers...)
}

// WaitCenter blocks until one of this client's centerprints matches re.
func (p *Bot) WaitCenter(re string, timeout time.Duration) (string, error) {
	rx := regexp.MustCompile(re)
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		for _, s := range p.Centers() {
			if rx.MatchString(s) {
				return s, nil
			}
		}
		time.Sleep(20 * time.Millisecond)
	}
	return "", fmt.Errorf("%s: no centerprint matched %q", p.Name, re)
}

// ConnectKey sets a userinfo key BEFORE Start, so that it travels in the
// CONNECT packet.  SetUserinfo below is the after-connect form and pushes an
// update, which is too late for anything ClientConnect itself reads -- the
// server `password`, `spectator`, the OSP player list's name check -- because
// by then the connection has already been accepted or refused.
func (p *Bot) ConnectKey(key, value string) {
	p.b.User[key] = value
}

// SetUserinfo changes one userinfo key and pushes the whole userinfo to the
// server, which is what a real client does on `name foo` or `skin male/x`.
// Without the push the map changes locally and the mod's
// ClientUserinfoChanged never runs.
func (p *Bot) SetUserinfo(key, value string) {
	p.b.User[key] = value
	p.b.SendUserinfo()
}
