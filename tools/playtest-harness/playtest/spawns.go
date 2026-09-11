package playtest

import (
	"fmt"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"

	"github.com/packetflinger/libq2/bsp"
	"github.com/packetflinger/libq2/pak"
)

// Spawn is one spawn point read out of a map.
type Spawn struct {
	Class  string
	Arena  int
	Origin [3]float64
	// Name is a stable label, "<class abbreviation>#<n>", numbered in the
	// order the entity lump lists them within its arena.
	Name string
	// Message is the entity's message key, which for an
	// info_player_intermission is the arena's display name -- the text its
	// row in the mod's arena menu shows.
	Message string
}

// MapEntities returns every entity of the named classes in a map, read out of
// the paks matching pakGlob.  Knowing the real spawn points turns a measured
// position into the identity of the spot the game chose, which is a far
// sharper signal than a distance.
func MapEntities(pakGlob, mapName string, classes ...string) ([]Spawn, error) {
	paks, err := filepath.Glob(pakGlob)
	if err != nil {
		return nil, err
	}
	if len(paks) == 0 {
		return nil, fmt.Errorf("no paks matched %q", pakGlob)
	}
	want := map[string]bool{}
	for _, c := range classes {
		want[c] = true
	}

	for _, p := range paks {
		data, err := os.ReadFile(p)
		if err != nil {
			return nil, err
		}
		archive, err := pak.Unmarshal(data)
		if err != nil {
			return nil, err
		}
		for _, f := range archive.GetFiles() {
			if strings.TrimSuffix(filepath.Base(f.GetName()), ".bsp") != mapName {
				continue
			}
			if !strings.HasSuffix(f.GetName(), ".bsp") {
				continue
			}
			return entities(f.GetData(), want)
		}
	}
	return nil, fmt.Errorf("map %q not in %q", mapName, pakGlob)
}

func entities(data []byte, want map[string]bool) ([]Spawn, error) {
	// libq2 reads a BSP from a path, so stage the pak member on disk
	tmp, err := os.CreateTemp("", "playtest-*.bsp")
	if err != nil {
		return nil, err
	}
	defer os.Remove(tmp.Name())
	if _, err := tmp.Write(data); err != nil {
		return nil, err
	}
	tmp.Close()

	b, err := bsp.OpenBSPFile(tmp.Name())
	if err != nil {
		return nil, err
	}
	defer b.Close()

	var out []Spawn
	for _, e := range b.FetchEntities() {
		if !want[e.Class] {
			continue
		}
		arena, _ := strconv.Atoi(e.Values["arena"])
		var o [3]float64
		for i, f := range strings.Fields(e.Values["origin"]) {
			if i > 2 {
				break
			}
			o[i], _ = strconv.ParseFloat(f, 64)
		}
		out = append(out, Spawn{Class: e.Class, Arena: arena, Origin: o, Message: e.Values["message"]})
	}

	// number them per arena, in entity-lump order
	seen := map[int]int{}
	for i := range out {
		seen[out[i].Arena]++
		out[i].Name = fmt.Sprintf("%s#%d", abbrev(out[i].Class), seen[out[i].Arena])
	}
	sort.SliceStable(out, func(i, j int) bool { return out[i].Arena < out[j].Arena })
	return out, nil
}

// ArenaNumber resolves an arena's display name to its number, by the same
// route the mod does: the message of the info_player_intermission stamped with
// that arena.  Matching is case-insensitive on a substring.
func ArenaNumber(pakGlob, mapName, name string) (int, error) {
	ents, err := MapEntities(pakGlob, mapName, "info_player_intermission")
	if err != nil {
		return 0, err
	}
	want := strings.ToLower(name)
	for _, e := range ents {
		if strings.Contains(strings.ToLower(e.Message), want) {
			return e.Arena, nil
		}
	}
	var have []string
	for _, e := range ents {
		have = append(have, fmt.Sprintf("%d=%q", e.Arena, e.Message))
	}
	return 0, fmt.Errorf("no arena on %s named %q (have %s)", mapName, name, strings.Join(have, " "))
}

func abbrev(class string) string {
	switch class {
	case "info_player_deathmatch":
		return "dm"
	case "misc_teleporter_dest":
		return "tp"
	}
	return class
}

// Nearest returns the spawn point closest to pos among those in arena, and the
// distance to it.  move_to_arena traces the player down onto the floor, so a
// placed player sits near its spawn point rather than exactly on it.
func Nearest(spawns []Spawn, arena int, pos [3]float64) (Spawn, float64) {
	best := Spawn{Name: "?"}
	bestd := -1.0
	for _, s := range spawns {
		if s.Arena != arena {
			continue
		}
		d := Dist(s.Origin, pos)
		if bestd < 0 || d < bestd {
			best, bestd = s, d
		}
	}
	return best, bestd
}

// Dist is the distance between two points.
func Dist(a, b [3]float64) float64 {
	dx, dy, dz := a[0]-b[0], a[1]-b[1], a[2]-b[2]
	return sqrt(dx*dx + dy*dy + dz*dz)
}

func sqrt(x float64) float64 {
	if x <= 0 {
		return 0
	}
	// Newton's method keeps this file free of a math import for one call
	z := x
	for i := 0; i < 40; i++ {
		z -= (z*z - x) / (2 * z)
	}
	return z
}
