package playtest

import (
	"fmt"
	"net"
)

// PortFree reports whether a dedicated server could bind p right now.
//
// It binds what q2proded binds -- a UDP socket on 0.0.0.0, and Go sets no
// SO_REUSEADDR on a packet conn, which q2pro does not either -- so a port that
// answers here is a port the server can have.  Anything weaker answers a
// different question: a connect-style probe, or a bind on 127.0.0.1 alone,
// both succeed against a wildcard socket that will refuse the server.
func PortFree(p int) bool {
	c, err := net.ListenPacket("udp4", fmt.Sprintf("0.0.0.0:%d", p))
	if err != nil {
		return false
	}
	c.Close()
	return true
}

// NextPort returns the first port at or above `from` that PortFree accepts,
// or 0 when a thousand consecutive ports are all taken.
//
// WHY A SCENARIO ASKS RATHER THAN COUNTS.  Every scenario here names a default
// port, they are all in the 279xx band, and the band is not ours: it is
// Quake II's registered range and anything else on the machine speaking that
// protocol will be sitting in it.  When the port is taken, q2proded prints
//
//	UDP_OpenSocket: :27983: can't bind socket: Address already in use
//	FATAL: Couldn't open dedicated server UDP port
//
// exits, and never reaches `SpawnServer` -- so the row fails as a boot
// timeout, under the name of whatever ruleset it happened to be testing.
// Measured: a Daikatana dedicated server holding 27983 and 27993 cost the
// `colosseum` battery two rows in the default run and four different ones
// under `-r tdm`, on a library that passed all 235 checks the moment it was
// given a free range.  `tools/botmatrix.sh` has never had the problem because
// it asks the engine for a port instead -- `+set net_port 0`.
//
// A probe can lose a race with a process that binds between the answer and the
// server's own bind.  It cannot lose to a socket that was already there, which
// is every collision seen.  A flag default becomes where the search starts
// rather than the port that is used.
func NextPort(from int) int {
	for p := from; p < from+1000; p++ {
		if PortFree(p) {
			return p
		}
	}
	return 0
}
