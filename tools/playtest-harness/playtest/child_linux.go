//go:build linux

package playtest

import (
	"os/exec"
	"syscall"
)

// OwnChild makes cmd's process die with the one that started it.  A scenario
// that is killed -- a Ctrl-C, a timeout, a panic -- otherwise leaves its
// q2proded running and holding the UDP port, and the NEXT run's boot fails
// on a busy port under the name of whatever row it was testing.  Linux kills
// the child when its parent goes (PR_SET_PDEATHSIG); elsewhere this is a
// no-op and Stop() is the only cleanup.
func OwnChild(cmd *exec.Cmd) {
	if cmd.SysProcAttr == nil {
		cmd.SysProcAttr = &syscall.SysProcAttr{}
	}
	cmd.SysProcAttr.Pdeathsig = syscall.SIGKILL
}
