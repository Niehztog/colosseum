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
	args = append(args, "+map", s.Map)

	s.cmd = exec.Command(s.Binary, args...)
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

	go s.drain(stdout)

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

// WaitLog blocks until a console line matches re, and returns it.  Lines
// already logged count, so a race against a fast server cannot lose the match.
func (s *Server) WaitLog(re string, timeout time.Duration) (string, error) {
	rx := regexp.MustCompile(re)
	ch := make(chan string, 256)

	s.mu.Lock()
	for _, l := range s.log {
		if rx.MatchString(l) {
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

	deadline := time.After(timeout)
	for {
		select {
		case l := <-ch:
			if rx.MatchString(l) {
				return l, nil
			}
		case <-deadline:
			return "", fmt.Errorf("timed out waiting for console line %q", re)
		}
	}
}

// Stop shuts the server down.
func (s *Server) Stop() {
	if s.cmd == nil || s.cmd.Process == nil {
		return
	}
	s.Console("quit")
	done := make(chan struct{})
	go func() { s.cmd.Wait(); close(done) }()
	select {
	case <-done:
	case <-time.After(3 * time.Second):
		s.cmd.Process.Kill()
		<-done
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
