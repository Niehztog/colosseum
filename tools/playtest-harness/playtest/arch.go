package playtest

import "runtime"

// hostArch is q2pro's CPUSTRING for this host: the game library must be named
// game<CPUSTRING>.so or the server will not find it.  These are meson.build's
// own cpu_family() names and its cpuremap, not Go's GOARCH spellings -- on
// aarch64 the server looks for gamearm64.so, and on 32-bit x86 gamei386.so.
//
// If a host ever disagrees, the server says so: the startup log prints
// "Can't access <dir>/game<arch>.so" with the exact name it wanted.
func hostArch() string {
	switch runtime.GOARCH {
	case "amd64":
		return "x86_64"
	case "arm64":
		return "arm64"
	case "386":
		return "i386"
	case "arm":
		return "arm"
	default:
		return runtime.GOARCH
	}
}
