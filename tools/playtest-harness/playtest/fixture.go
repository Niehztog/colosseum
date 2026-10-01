package playtest

import (
	"io"
	"os"
	"path/filepath"
)

// WriteFixture writes data to path in a scratch install, replacing whatever
// was there -- and a symlink is REPLACED, not written through.  Install links
// the reference install's .cfg and .txt files into the gamedir and
// InstallBrain its meshes, so os.WriteFile on one of those names wrote the
// scenario's arena.cfg or stripped .aas into the reference install itself.
// Unlinking first is what makes the fixture the only thing that changes.
func WriteFixture(path string, data []byte, perm os.FileMode) error {
	if err := os.Remove(path); err != nil && !os.IsNotExist(err) {
		return err
	}
	return os.WriteFile(path, data, perm)
}

// Brain is the gladiator.so a fixture should install for gladdir: the one the
// running script resolved, when it resolved one (COLOSSEUM_BRAIN), and the
// checkout's own release/ otherwise.  The script's is the one built beside
// the game library under test -- the Makefile copies it there -- while the
// checkout's release/ holds whatever target was built LAST, and is wiped and
// rebuilt on every target switch.
func Brain(gladdir string) string {
	if b := os.Getenv("COLOSSEUM_BRAIN"); b != "" {
		return b
	}
	return filepath.Join(gladdir, "release", "gladiator.so")
}

// CopyFixture puts a private copy of src at dst, for a file something in the
// server will WRITE.  The brain rewrites a mesh it has computed reachability
// for (AAS_WriteAASFile opens it "wb"), which through a symlink -- or a
// hard link, which shares the inode -- rewrites the shipped mesh.
func CopyFixture(src, dst string) error {
	in, err := os.Open(src)
	if err != nil {
		return err
	}
	defer in.Close()
	if err := os.Remove(dst); err != nil && !os.IsNotExist(err) {
		return err
	}
	out, err := os.OpenFile(dst, os.O_WRONLY|os.O_CREATE|os.O_EXCL, 0o644)
	if err != nil {
		return err
	}
	if _, err := io.Copy(out, in); err != nil {
		out.Close()
		return err
	}
	return out.Close()
}
