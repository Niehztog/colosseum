// Package playtest drives a real Quake II dedicated server with headless
// clients, so a game-library change can be checked against the running game
// instead of only by reading the source.
package playtest

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"
	"sync"
	"time"
)

// Server is a q2proded process running one mod.
type Server struct {
	// Binary is the q2proded executable.  Required.
	Binary string
	// Dir is the install root: it must contain a Game subdirectory holding
	// the mod's paks and its game library.  Required.
	Dir string
	// Game is the mod directory name, e.g. "arena".  Empty means baseq2.
	Game string
	// Map is the map to spawn.  Required.
	Map string
	// Port is the UDP port to listen on.
	Port int
	// MaxClients caps connected clients.  Defaults to 16.
	MaxClients int
	// Cvars are extra "name value" settings applied at startup.
	Cvars map[string]string
	// LogPath, if set, also writes the console to this file.
	LogPath string

	cmd   *exec.Cmd
	stdin io.WriteCloser
	logf  *os.File

	mu   sync.Mutex
	log  []string
	subs []chan string

	// done closes when the process has exited and its output is drained, and
	// exitErr is what Wait said.  A crash is otherwise invisible: q2proded
	// prints nothing on SIGSEGV or SIGFPE, so a dead server and a quiet one
	// read the same from its log.
	done    chan struct{}
	exitErr error
}

// Start launches the server and waits until it reports a spawned map.
func (s *Server) Start() error {
	if s.MaxClients == 0 {
		s.MaxClients = 16
	}
	if s.Port == 0 {
		s.Port = 27910
	}

	args := []string{
		"+set", "basedir", s.Dir,
		"+set", "homedir", s.Dir, // where q2pro looks for game<arch>.so
		"+set", "dedicated", "1",
		"+set", "deathmatch", "1",
		"+set", "maxclients", fmt.Sprint(s.MaxClients),
		"+set", "net_port", fmt.Sprint(s.Port),
		// several bots share 127.0.0.1, and q2pro's default allows three
		"+set", "sv_iplimit", "0",
	}
	if s.Game != "" {
		args = append(args, "+set", "game", s.Game)
	}
	for k, v := range s.Cvars {
		args = append(args, "+set", k, v)
	}
	// PLAYTEST_CVARS is "name=value ..." from the environment, set after the
	// scenario's own, so a wrapper can run any scenario under a setting the
	// scenario never declared -- Colosseum's tools/playtest.sh runs a bot
	// scenario on another botlib that way.
	for _, kv := range strings.Fields(os.Getenv("PLAYTEST_CVARS")) {
		if k, v, ok := strings.Cut(kv, "="); ok && k != "" {
			args = append(args, "+set", k, v)
		}
	}
	args = append(args, "+map", s.Map)

	// A port something else holds is a boot that can only fail, and it fails
	// as a 30-second "did not spawn" under a ruleset's name.  Said now, by
	// port.  (PortFree binds what q2proded binds; see port.go.)
	if !PortFree(s.Port) {
		return fmt.Errorf("UDP port %d is in use by another process -- a server "+
			"left over from an earlier run, or another game; pass a free -port", s.Port)
	}

	s.cmd = exec.Command(s.Binary, args...)
	OwnChild(s.cmd)
	s.cmd.Dir = s.Dir
	stdout, err := s.cmd.StdoutPipe()
	if err != nil {
		return err
	}
	s.cmd.Stderr = s.cmd.Stdout
	if s.stdin, err = s.cmd.StdinPipe(); err != nil {
		return err
	}
	if s.LogPath != "" {
		if s.logf, err = os.Create(s.LogPath); err != nil {
			return err
		}
	}
	if err := s.cmd.Start(); err != nil {
		return err
	}

	// Wait only after the pipe is drained: exec's contract is that Wait closes
	// the pipe, so calling it first loses the last lines -- which on a crash
	// are the only ones that say anything.
	s.done = make(chan struct{})
	go func() {
		s.drain(stdout)
		s.exitErr = s.cmd.Wait()
		close(s.done)
	}()

	// "SpawnServer: <map>" is printed once the map is up and the game
	// library's InitGame/SpawnEntities have run.
	if _, err := s.WaitLog(`SpawnServer: `+regexp.QuoteMeta(s.Map), 30*time.Second); err != nil {
		tail := s.tail(6)
		s.Stop()
		// THE SERVER'S OWN LAST WORDS, because without them this error names
		// only the line that did not arrive and every cause looks alike.  The
		// one that actually happens is a busy port, and the engine says so
		// exactly -- `UDP_OpenSocket: :27983: can't bind socket: Address
		// already in use` -- while the harness reported `timed out waiting for
		// console line "SpawnServer: q2dm1"` and a row named after a ruleset.
		// Diagnosing it meant knowing the scratch log existed and finding it
		// by hand, so the path is named here too: six lines is enough for the
		// bind failure and the FATAL under it, and the file has the rest.
		where := ""
		if s.LogPath != "" {
			where = fmt.Sprintf(", full log in %s", s.LogPath)
		}
		return fmt.Errorf("server did not spawn %s: %w%s; last output: %s",
			s.Map, err, where, strings.Join(tail, " | "))
	}
	return nil
}

// tail returns the last n console lines the server produced.
func (s *Server) tail(n int) []string {
	s.mu.Lock()
	defer s.mu.Unlock()
	if len(s.log) < n {
		n = len(s.log)
	}
	if n == 0 {
		return []string{"(the server printed nothing)"}
	}
	return append([]string(nil), s.log[len(s.log)-n:]...)
}

func (s *Server) drain(r io.Reader) {
	sc := bufio.NewScanner(r)
	sc.Buffer(make([]byte, 64*1024), 1024*1024)
	for sc.Scan() {
		line := sc.Text()
		s.mu.Lock()
		s.log = append(s.log, line)
		for _, c := range s.subs {
			select {
			case c <- line:
			default:
			}
		}
		s.mu.Unlock()
		if s.logf != nil {
			fmt.Fprintln(s.logf, line)
		}
	}
}

// Console sends a command to the server console, as if typed by the operator.
func (s *Server) Console(format string, a ...any) error {
	_, err := fmt.Fprintf(s.stdin, format+"\n", a...)
	return err
}

// Log returns every console line seen so far.
func (s *Server) Log() []string {
	s.mu.Lock()
	defer s.mu.Unlock()
	return append([]string(nil), s.log...)
}

// Grep returns the console lines matching re.
func (s *Server) Grep(re string) []string {
	rx := regexp.MustCompile(re)
	var out []string
	for _, l := range s.Log() {
		if rx.MatchString(l) {
			out = append(out, l)
		}
	}
	return out
}

// Len is how many console lines have been seen.  Paired with GrepFrom it turns
// "does the log contain X" into "did X arrive AFTER this point", which is the
// question a check about a command's effect is actually asking -- a server that
// printed the line ten seconds ago answers the first question and not the
// second.
func (s *Server) Len() int {
	s.mu.Lock()
	defer s.mu.Unlock()
	return len(s.log)
}

// GrepFrom returns the console lines from index n onwards that match re.
func (s *Server) GrepFrom(n int, re string) []string {
	rx := regexp.MustCompile(re)
	log := s.Log()
	if n < 0 {
		n = 0
	}
	if n > len(log) {
		n = len(log)
	}
	var out []string
	for _, l := range log[n:] {
		if rx.MatchString(l) {
			out = append(out, l)
		}
	}
	return out
}

// Exited reports whether the server process has ended, and how.  A row that
// ends with "the server is still answering" asks this as well as the console,
// because the console of a crashed server is not an answer.
func (s *Server) Exited() (bool, error) {
	if s.done == nil {
		return false, nil
	}
	select {
	case <-s.done:
		return true, s.exitErr
	default:
		return false, nil
	}
}

// WaitLog blocks until a console line matches re, and returns it.  Lines
// already logged count, so a race against a fast server cannot lose the match
// -- and so does every line from before the event being waited for, which is
// the wrong answer to "did it happen AFTER this?".  That question is
// WaitLogFrom's, with a mark from Len().
func (s *Server) WaitLog(re string, timeout time.Duration) (string, error) {
	return s.WaitLogFrom(0, re, timeout)
}

// WaitLogFrom is WaitLog counting only the lines from index mark onwards.  It
// returns at once, with an error, if the server exits first: a dead server
// never prints the line, and waiting out the timeout only to report "timed
// out" names the wrong cause.
func (s *Server) WaitLogFrom(mark int, re string, timeout time.Duration) (string, error) {
	rx := regexp.MustCompile(re)
	ch := make(chan string, 256)

	s.mu.Lock()
	if mark < 0 {
		mark = 0
	}
	for i := mark; i < len(s.log); i++ {
		if rx.MatchString(s.log[i]) {
			l := s.log[i]
			s.mu.Unlock()
			return l, nil
		}
	}
	s.subs = append(s.subs, ch)
	s.mu.Unlock()

	defer func() {
		s.mu.Lock()
		for i, c := range s.subs {
			if c == ch {
				s.subs = append(s.subs[:i], s.subs[i+1:]...)
				break
			}
		}
		s.mu.Unlock()
	}()

	var exited <-chan struct{}
	if s.done != nil {
		exited = s.done
	}
	deadline := time.After(timeout)
	for {
		select {
		case l := <-ch:
			if rx.MatchString(l) {
				return l, nil
			}
		case <-exited:
			// Drained before done closed, so a matching last line is in the
			// log even if it never reached the channel.
			for _, l := range s.LogFrom(mark) {
				if rx.MatchString(l) {
					return l, nil
				}
			}
			return "", fmt.Errorf("the server exited (%v) before printing %q",
				s.exitErr, re)
		case <-deadline:
			return "", fmt.Errorf("timed out waiting for console line %q", re)
		}
	}
}

// Alive asks whether the server is running and answering NOW: the process has
// not exited, and `status` gets a fresh reply.  It is the check a "the server
// survived" row means, and grepping the console for "Segmentation" is not --
// q2proded prints nothing when it dies.
func (s *Server) Alive(timeout time.Duration) error {
	_, err := s.Ask("status", `^Current map: `, timeout)
	return err
}

// LogFrom returns the console lines from index mark onwards.
func (s *Server) LogFrom(mark int) []string {
	log := s.Log()
	if mark < 0 {
		mark = 0
	}
	if mark > len(log) {
		mark = len(log)
	}
	return log[mark:]
}

// Ask sends a console command and waits for a reply line matching re that
// arrived AFTER it was sent.  It returns the index the reply starts from, so
// the caller parses the answer to THIS question with LogFrom and not the
// first answer the log happens to hold.  The server having exited is an
// error, never an answer.
func (s *Server) Ask(cmd, re string, timeout time.Duration) (int, error) {
	if dead, err := s.Exited(); dead {
		return 0, fmt.Errorf("the server has exited (%v)", err)
	}
	mark := s.Len()
	if err := s.Console("%s", cmd); err != nil {
		return mark, err
	}
	_, err := s.WaitLogFrom(mark, re, timeout)
	return mark, err
}

// Stop shuts the server down.
func (s *Server) Stop() {
	if s.cmd == nil || s.cmd.Process == nil {
		return
	}
	s.Console("quit")
	select {
	case <-s.done:
	case <-time.After(3 * time.Second):
		s.cmd.Process.Kill()
		<-s.done
	}
	if s.logf != nil {
		s.logf.Close()
	}
}

// Install prepares a throwaway install directory: it links the mod's paks and
// config in from a read-only reference install and links the game library the
// server should load, named as q2pro expects for this host.
//
// ref is the directory holding the original paks; lib is the built game
// library.  Nothing is written into ref.
func Install(dir, game, ref, lib string) error {
	gamedir := filepath.Join(dir, game)
	if err := os.MkdirAll(gamedir, 0o755); err != nil {
		return err
	}
	// baseq2 has to exist even when everything lives in the mod dir
	if err := os.MkdirAll(filepath.Join(dir, "baseq2"), 0o755); err != nil {
		return err
	}

	entries, err := os.ReadDir(ref)
	if err != nil {
		return err
	}
	for _, e := range entries {
		name := e.Name()
		ext := strings.ToLower(filepath.Ext(name))
		if ext != ".pak" && ext != ".pkz" && ext != ".cfg" && ext != ".txt" {
			continue
		}
		dst := filepath.Join(gamedir, name)
		os.Remove(dst)
		src, err := filepath.Abs(filepath.Join(ref, name))
		if err != nil {
			return err
		}
		if err := os.Symlink(src, dst); err != nil {
			return err
		}
	}

	if lib != "" {
		abs, err := filepath.Abs(lib)
		if err != nil {
			return err
		}
		dst := filepath.Join(gamedir, "game"+hostArch()+".so")
		os.Remove(dst)
		if err := os.Symlink(abs, dst); err != nil {
			return err
		}
	}
	return nil
}
