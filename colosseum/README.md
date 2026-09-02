# `colosseum/` — the default gamedir config set (D6)

Copy the contents of this directory into the server's `colosseum` gamedir —
`<basedir>/colosseum/` or `<homedir>/colosseum/`, whichever the server was
started with — alongside the game library and the paks.  Nothing here is
required for the library to run: every value is a default the code already
carries, written down so an operator can see and change it.

| file | who reads it | format |
|---|---|---|
| `server.cfg` | the operator, by hand or from their own `autoexec.cfg` | console commands |
| `configs/dm.cfg` | ditto | console commands |
| `configs/dmpro.cfg` | ditto | console commands |
| `configs/tdm.cfg` | ditto | console commands |
| `configs/duel.cfg` | ditto | console commands |
| `configs/ctf.cfg` | ditto | console commands |
| `configs/arena.cfg` | ditto | console commands |
| `configs/sp.cfg` | ditto | console commands |
| `botcfg/bots.cfg` | the game library, at `InitGame`, via the `botfile` cvar | the 1999 bot roster format |
| `arena.cfg` | the game library, at every map load under `arena`, via the `arenacfg` cvar | **RA2's own file** — its brace/colon format, with two deviations its own header names |
| `motd.txt` | the game library, at every map load under `arena` | one line per menu row |

**There is no `default.cfg` here, and that is deliberate.** D6 asked for one,
but `default.cfg` is **id's own file**: it ships inside `pak0.pak` and holds all
68 key bindings a Quake II client starts with. q2pro execs `default.cfg` from
the gamedir before `config.cfg`, and a real file in the gamedir wins over a pak
entry — so a file of ours by that name leaves a client that joins with
`+set game colosseum` with **no bindings at all**. The shared defaults are
`server.cfg`.

**`arena.cfg` is Rocket Arena 2's own file** (1028 lines, CRLF as it shipped;
`.gitattributes` keeps git from rewriting it). Every one of its 171 per-arena
blocks is 1999's; the two deviations are in the header block that precedes
them and the header says so — the `armor:`/`health:` default, and
`armorprotect: 1` so that splash from your own launcher costs health and not
armour (the built-in default, 2, exempts a team-mate but leaves your own rocket
to eat it). It is the only file in this set that is a copy of somebody else's
rather than a written-out default, apart from `botcfg/bots.cfg`. What it carries
that a hand-written
config cannot: 28 per-map blocks with per-arena weapons, armour, round counts
and players-per-team — and **30 `pickup: 1` arenas**, which are what make
an arena a *pickup arena*. Those show as ` (PT)` in the "Choose Your Arena"
menu, and as two joinable teams, `#N Pickup Red` and `#N Pickup Blue`, in
"Choose your team". Before 1.27 this file was a 69-line example with none of
that, and on RA2's own maps the team menu offered nothing but *Start New Team*.

**Seven rulesets, and four of them are one code path.** `dm`, `dmpro`, `tdm`
and `duel` are OSP Tourney DM's four structures of play, which until spec 1.36
were selected by a second cvar — `match_mode` — *inside* a `tourney` ruleset.
They are `g_ruleset` values now, `match_mode` is gone, and `g_ruleset tourney`
is no longer a ruleset name at all — it warns like any other unrecognised value,
lists the seven that are valid, and runs as `dm`. `configs/tourney.cfg` is
replaced by the four files above.

Two things an existing config will notice. **`dm` reads `bots_minplayers`, not
`minimumplayers`** — under the OSP four the bot cvars are tourney's own
(R-OSP-11), and `dm` is one of them now; the shipped `configs/dm.cfg` sets it.
And **`teamplay` is refused** under all four with a message naming `tdm`, which
is where team play lives.

**`configs/arena.cfg` sets `botfill 1` rather than a flat `minimumplayers`.**
The bot count is taken from the arena the bots are being fed into: its
`playersperteam` where this file carries one, and the count of that arena's own
`info_player_deathmatch` entities where it does not. The second case is every
**pickup** arena, because `arena_init()` replaces `playersperteam` with 128
there — RA2 wants a pickup team unbounded — and all 30 of RA2's pickup arenas
leave the key unset anyway. Spawn points are *not* used for the other kind and
that is deliberate: RA2's mappers used them for variety, so `ra2map8` arena 3
has thirteen of them and is declared 1v1. `set botfill 0` restores 1999's
flat count, and `set minimumplayers 4` beside it is what this file used to say.
Two things to know: **`maxclients` is latched and defaults to 4**, so the fill
cannot exceed 2v2 until it is raised; and with it raised, bots will take every
slot on an empty server, which is `minimumplayers`' own behaviour reached more
easily. `sv arenadump` prints `want=` and `here=` per arena, and `sv ruleset`
prints the target in force.

Two consequences worth knowing:

* **Its `maploop` names RA2's own 28 maps**, not stock ones. Nothing under
  `arena` sets a `timelimit`, so the rotation is reached only by a vote or an
  explicit map change — but an operator who runs the arena ruleset on stock maps
  *and* sets a timelimit should replace that one line.
* **On a stock map you get pickup teams anyway.** A map with no `arena` key in
  its worldspawn is an "idmap": one arena covering the whole level, and `pickup`
  defaults to 1 there, so `#1 Pickup Red`/`#1 Pickup Blue` exist whatever this
  file says. The per-arena settings only mean something on a map built with
  arenas in it.

**`configs/ctf.cfg` and the four OSP files carry the same switch, off.**
There is **one** `botfill` cvar for every ruleset (R-RA-7, R-CTF-8, R-DM-1); it
was three names — `ra_botfill`, `ctf_botfill`, `dm_botfill` — until spec 1.36
unified them, and what stays per ruleset is the *target*, not the switch. It
defaults to `0`, because a flat count is a perfectly good answer for one map and
one game — it is just a number an operator has to re-guess every map change.
`1` reads the target off the game instead. Under `dm` and `dmpro`
that is the shared spawn pool, which is two short of the map's count because
`SelectRandomDeathmatchSpawnPoint` refuses the two spots nearest a player: 8, 5,
5, 9, 7, 6, 4, 4 across the eight `q2dm` maps, rounded down to even under
`teamplay`. Under `ctf` it is twice the smaller of a base and half that shared
pool, because a CTF client spawns at its base once and in the shared pool for
ever after: 16, 12, 14, 4, 20, 14, 14, 16 across the eight Threewave maps. Base
spawn points alone would say 28 for `q2ctf1`, which is played 8v8 — the same
variety-not-capacity trap `arena.cfg`'s note above records. Under `tdm` and
`duel` there is nothing to read off the map, because those two **declare** a
capacity: the target is `2 * team_maxplayers`, which `duel` forces to 1, so it
is exactly 2 there. `maxclients` is latched at 4 by default, so none of this
does anything visible until it is raised, and `sv ruleset` prints the target in
force and where the number came from.

**Two files called `arena.cfg`, and they are not the same file.**
`configs/arena.cfg` is a console script for the `arena` RULESET — `set`
commands the operator execs.  `arena.cfg` at the top level is Rocket Arena's
own **arena definition** file, in its own brace-and-colon format, and it is
read by `load_config()` in `src/arena/maploop.c` at every map load.  The
collision is the donors': RA2 named its data file after the game type, and the
Quake II convention names console scripts the same way.  The paths keep them
apart and the `arenacfg` cvar can move the data one if an operator wants.

**Nothing here binds a key or sets a video mode**, and that is deliberate: those
belong to a player's `config.cfg`, not to a server's gamedir. `tools/watch.sh`
sets up a watcher's keyboard from `tools/drive/watch-keys.cfg`, which is not
part of this set for the same reason.

## Using it

```
# in your server.cfg, before the first map
exec server.cfg
exec configs/tdm.cfg          // or dm / dmpro / duel / ctf / arena / sp
map q2dm1
```

The per-ruleset configs each `set g_ruleset` themselves, so exec'ing one is
enough to choose the ruleset.  `g_ruleset` is latched: it takes effect at the
next map load, which is why the exec goes before `map`.
