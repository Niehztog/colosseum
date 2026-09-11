// Package ra2 holds the Rocket Arena 2 side of a playtest: how to get a client
// from "connected" to "standing in an arena".  The generic Quake II mechanics
// live in playtest; only the menu choreography is here, because RA2 has no
// console command for any of it.
package ra2

import (
	"fmt"
	"strings"
	"time"

	"q2playtest/playtest"
)

// Timeout is how long each menu step may take.
var Timeout = 30 * time.Second

// DismissMOTD clears the message of the day, which stands in front of the team
// menu when the server has a motd.txt.
func DismissMOTD(b *playtest.Bot) error {
	if err := b.WaitMenu(`.`, Timeout); err != nil {
		return err
	}
	if !strings.EqualFold(b.MenuTitle(), "Message of the Day") {
		return nil
	}
	return b.MenuPick(`Continue`, Timeout)
}

// NewTeamInArena creates a team of this client's own and takes it into the
// named arena.  arena is matched case-insensitively against the arena's menu
// row, which is the name its info_player_intermission carries.
//
// This is the ordinary join, and it does not work on a pickup arena: AddtoArena
// rejects one outright with "You must join a pickup team to enter that arena".
// Use JoinTeam with one of that arena's pickup teams instead.
func NewTeamInArena(b *playtest.Bot, arena string) error {
	if err := DismissMOTD(b); err != nil {
		return err
	}
	if err := b.WaitMenu(`(?i)choose your team`, Timeout); err != nil {
		return err
	}
	if err := b.MenuPick(`Start New Team`, Timeout); err != nil {
		return err
	}
	if err := b.WaitMenu(`(?i)choose your arena`, Timeout); err != nil {
		return err
	}
	return b.MenuPick(`(?i)`+arena, Timeout)
}

// JoinTeam joins an existing team from the team menu.  A pickup arena's teams
// are created by the mod at map load and are already in their arena, so joining
// one puts the client straight into that arena -- there is no arena menu step.
func JoinTeam(b *playtest.Bot, team string) error {
	if err := DismissMOTD(b); err != nil {
		return err
	}
	if err := b.WaitMenu(`(?i)choose your team`, Timeout); err != nil {
		return err
	}
	return b.MenuPick(`(?i)`+team, Timeout)
}

// PickupTeam names the pickup team the mod creates for arena n: it labels them
// "#<arena> Pickup Red" and "#<arena> Pickup Blue".
func PickupTeam(arena int, side string) string {
	return fmt.Sprintf(`#%d Pickup %s`, arena, side)
}
