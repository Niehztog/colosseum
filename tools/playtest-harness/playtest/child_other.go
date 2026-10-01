//go:build !linux

package playtest

import "os/exec"

// OwnChild is a no-op off Linux, which has no parent-death signal; see
// child_linux.go.
func OwnChild(cmd *exec.Cmd) {}
