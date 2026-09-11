package colosseum

import "time"

// Killable is the part of a client KillAndConfirm needs: send it a console
// command, and let a frame go by.  An interface rather than *playtest.Bot so
// that this package keeps depending on nothing but the standard library.
type Killable interface {
	Cmd(format string, a ...any)
	WaitFrames(n int, timeout time.Duration) error
}

// Cmd_Kill_f refuses a suicide for five seconds after a respawn --
// `(level.framenum - respawn_framenum) < 5 * BASE_FRAMERATE` -- and says
// nothing when it does.  At ten frames a second that is fifty frames of
// silence, so a scenario that sends one `kill` and reads on has no way to
// tell "the player died" from "the server ignored me".
const (
	killRefusal = 5 * time.Second // the guard in Cmd_Kill_f
	killMargin  = 3 * time.Second // for a refusal that began just before we did
	killRetry   = 10              // frames between attempts, i.e. one second
)

// KillAndConfirm sends `kill` until the server actually takes it, and reports
// whether a death was ever observed.
//
// Every caller wants the same thing and none of them can get it from `kill`
// alone: a match start respawns everybody, so a `kill` sent just after one
// lands inside the refusal window more often than not, and a check written on
// top of an unconfirmed death is satisfied by a death that never happened.
// Retrying across the window turns the refusal into a delay instead.
//
// Retries are a second apart rather than as fast as the frames allow: nothing
// in the game library rate-limits `kill` -- FloodProtect guards only chat --
// but the engine does rate-limit client commands, and a helper that trips it
// would fail the scenario for a reason that has nothing to do with the scenario.
//
// alive reports whether the client still has a body, in practice
// `func() bool { return b.Stat(statHealth) > 0 }`.  It is polled every frame
// rather than once per attempt, because the client respawns on its own and a
// death seen late is indistinguishable from one that never happened.
//
// A client that is already bodiless counts as confirmed: there is nothing left
// to kill, and reporting failure there would fail the caller for the very
// state it was asking for.
func KillAndConfirm(p Killable, alive func() bool) bool {
	if !alive() {
		return true
	}
	deadline := time.Now().Add(killRefusal + killMargin)
	for {
		p.Cmd("kill")
		for i := 0; i < killRetry; i++ {
			if err := p.WaitFrames(1, 2*time.Second); err != nil {
				return false
			}
			if !alive() {
				return true
			}
		}
		if time.Now().After(deadline) {
			return false
		}
	}
}
