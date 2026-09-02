# Colosseum

One modern Quake II game library that unites baseq2, both mission packs,
Threewave CTF, Rocket Arena 2 and OSP Tourney DM on a single Q2PRO-derived code
base, and restores full Gladiator Bot command and botlib support on top of it.

One library, one venue, many rulesets, with Mr. Elusive's Gladiator bots as the
opponents.

## Colosseum is non-commercial

**This software may not be sold, in any form whatsoever.** It incorporates code
descended from the Gladiator Bot game source, whose licence forbids sale and
requires the attribution reproduced below. See `LICENSE` for the GPL-2 terms
covering the id-derived code, and `doc/provenance.md` for per-file licensing.

    This product incorporates source code from the Gladiator bot.
    The Gladiator bot is available at the Gladiator Bot page
    http://www.botepidemic.com/gladiator.

    This program is in NO way supported by MrElusive.

    This program may NOT be sold in ANY form whatsoever. If you have paid
    for this product, you should contact MrElusive immidiately via the
    Gladiator bot page or at MrElusive@botepidemic.com

(Reproduced verbatim from `gladq2_src/readme.txt` as R-LIC-2 requires,
misspelling included.)

The Rocket Arena 2 Bot Support Routines are © 1998 David Wright, non-commercial
use only. Colosseum does not carry that file — R-ARENA-1 replaces it with the
full Rocket Arena 2 — but the notice ships with any line of it that survives.

## Status

**Phase 8 of nine.** `SPECS.md` is the specification of record and every
requirement carries a stable ID that code, commits and review notes reference;
`doc/reconciliation.md` records every decision the merges needed and why.

What plays today, from one library, chosen by `g_ruleset`:

| ruleset | what it is |
|---|---|
| `dm` | OSP Tourney DM's RegularDM — baseq2 deathmatch, with the tourney match system over it |
| `dmpro` | OSP QualifierDM |
| `tdm` | OSP TeamPlay |
| `duel` | OSP 1-vs-1 |
| `ctf` | Threewave Capture The Flag 1.52 |
| `arena` | Rocket Arena 2 v2.25 |
| `sp` | the three campaigns — baseq2, The Reckoning, Ground Zero — single and co-op |

The first four are OSP Tourney DM v2.75's four structures of play. They were one
`tourney` ruleset selected by a second cvar, `match_mode`, until spec 1.36 made
them four values of `g_ruleset` and deleted the cvar (R-OSP-12). **`tourney` is
not a ruleset name**: it warns like any other unrecognised value, lists the seven
that are valid, and runs as `dm`.

`xatrix` and `rogue` are content layers, orthogonal to all seven and valid with
any of them.

The seven small Gladiator features are cvars rather than `#define`s (R-EXTRA):
the game log, client-lag simulation, two 1999 trigger entities, the rotating
button, visible weapons and the eye/chase observer. `sv extras` reports what is
in force. `colosseum/` in this repository is a default gamedir config set —
copy it beside the library and `exec configs/<ruleset>.cfg`.

**And bots.** `src/bot/` is the game side of Mr. Elusive's botlib: it loads the
brain dynamically, redirects twenty slots of the game import so the brain sees
everything the game does, spawns fake clients, drives them through the frame
loop and gives them the 1999 command set and menu. On `q2dm1` under `dm` they
load, spawn, navigate and fight.

They load, spawn, navigate, fight and chat in all six rulesets that accept them
— every one but `sp` (R-MODE-7): a CTF team from `botctfteam`, an arena's
waiting queue from the `arena` key, and a match under each of the OSP four,
readying themselves up per `bots_warmuptime`. With 32 of them on `q2dm1` the bot section of `G_RunFrame`
costs about 2.5 ms of a 100 ms frame (R-BOT-23, `sv botperf`).

The brain is a sibling repository, `gladiator-bot-restored`, built for whichever
platform the game targets. The interface between the two is
`doc/botlib-contract.md`, at version 3, and `tools/botabi.py` checks it in
`make check`: every slot, the preprocessor condition that selects `Trace`, and
every struct size — the last by compiling a probe against the brain's own
headers rather than trusting a number in a document.

## Building

The build produces `game<cpu>.so` / `game<cpu>.dll`, named the way the engine
loads it (`game` + Q2PRO's remapped cpu family + the platform suffix).

```sh
make            # native, debug + release
make windows    # win32 + win64 PE, cross
make everything # all five targets of R-BUILD-5, debug + release
make check      # the contract audits and the g_ptrs.c freshness check only
make help       # what each target is and whether this host can run it
```

Five targets are gating: ELF aarch64, ELF x86-64, ELF i386, win32 PE, win64 PE.
A given host can usually only *execute* one of them; `make help` says which.

### The old game API

Colosseum targets Q2PRO's game ABI, `GAME_API_VERSION_NEW` (3302), and that is
the default and the shipped configuration. `API=old` builds the same sources
against the classic id ABI, `GAME_API_VERSION_OLD` (3) — `gclient_old_t`,
`pmove_old_t`, 32 stat slots — for a 1997-vintage engine or a Q2PRO built without
`USE_NEW_GAME_API`. Q2PRO's loader accepts either, so both are real targets.

```sh
make API=old            # native pair, old ABI
make windows API=old    # composes with every target above
make oldapi             # shorthand for `native API=old`
make bothapis           # the same goal at both settings
```

`API=old` builds into `<dir>-oldapi`, and that separation is load-bearing rather
than tidy: the switch changes **struct layouts**, so an object from one setting
linked against an object from the other reads every field at the wrong offset
*and links cleanly*. Header dependencies cannot catch it either — no header
changed, so `make API=old` in a tree built as `new` would recompile nothing. The
artifact **name** does not change, because the engine looks for
`game<cpu><suffix>` and nothing else.

**One behaviour differs, by design.** Under `ctf` the second powerup timer needs
two stat slots and Threewave already uses 0..30 of the old 32, so Colosseum puts
the pair at 32/33 — reachable only on the new ABI, and only for a client that
negotiated protocol extensions. Under `API=old` it is dropped and `ctf` loses that
display, exactly as upstream does. `sv slots` reports it by name, and its header
line carries the api version for this reason:

    ruleset      ctf   (extensions on, api 3, so slots 0..31 are reachable)
      (dropped)  SID_TIMER2_ICON -- map says 32
      (dropped)  SID_TIMER2 -- map says 33

Note `extensions on` beside `0..31`: the two switches are independent, so a
client can negotiate 64 slots from a library whose array holds 32. Every other
ruleset stays inside 32 and is unaffected. Savegames are **not** portable between
the two builds and are refused rather than misread (`SAVE_VERSION` already
differs). See R-ENG-1a and R-OSP-7 clause 6.

A second ABI also earns its keep as a check. `-Warray-bounds` at `API=old -O2`
refused one line, and under it was a defect that five phases of audits, ten build
configurations, a twenty-row boot matrix and the play-test battery had all
passed: the tourney runes had **never worked**, and any of the OSP four with
`runes 1` died at map load with `ED_Alloc: no free edicts`. Everything that could
have caught it asked the stat map, and the stat map was right (R-132).

The two PE artifacts are checked for what they **import** the moment they are
linked (`tools/pedeps.sh`, R-SEC-6a): a game DLL that needs a compiler runtime
shipped beside it is not shippable, and both of ours wanted `libssp-0.dll` until
the stack protector's runtime was linked in statically. That failure is invisible
from a Linux host — the DLL links and every other check passes — so the check
runs in the build rather than on request.

Warnings are on and fatal: `-Wall -Wextra -Werror`, clean under both `gcc` and
`clang`, with three suppressions for inherited code that R-CORE-5 forbids
editing — `unused-parameter` (594 hits, Quake II's callback signatures are fixed
by their function-pointer types), `sign-compare` (28, all read, all benign) and
`missing-field-initializers` (2, the `itemlist[]` sentinels). Each is documented
in the `Makefile` with the count it silences. `_FORTIFY_SOURCE=2` is on for
release builds under `gcc`; it cannot be used with `clang`, for a reason worth
reading in the `Makefile` if you ever name a struct member `dprintf`.

The contract audits run as part of the build, not on request, and a finding
fails it the way a warning does. Most of them ship a positive control that makes
them fail, and each control is a run of its own. **`audit.py` prints the count
when it finishes and that is the figure to quote** — the two written down here
had drifted by the time anything re-read them, so they are gone rather than
corrected into the next stale pair. §10 of `SPECS.md` says what each check is
for.

Requires: a C compiler, `python3`, and `make`. Cross targets additionally need
`gcc-x86-64-linux-gnu`, `gcc-i686-linux-gnu`, `gcc-mingw-w64-i686` and
`gcc-mingw-w64-x86-64`.

### Running the checks that need a server

`make check` is static. Six scripts drive a real `q2proded`, and none of them is
part of the build because each needs a built engine, retail paks and a minute or
more. **Every one of them prints its own total when it finishes, and that is the
figure to quote.** The ones that used to be written down here had all drifted —
the boot matrix by a whole ruleset selector, and the client battery by three
different numbers in three different files — so they are gone from the comments
below rather than corrected into the next stale set:

```sh
tools/bootmatrix.sh   # 28 rows: every ruleset x xatrix x rogue boots and reports back
tools/smoke.sh        # one map per ruleset, with a savegame round trip
tools/playtest.sh     # the client-side battery, through headless clients (needs the q2-playtest skill)
tools/botmatrix.sh    # 1, 16 and 32 bots per ruleset: spawned, played, removed, and timed
tools/extras.sh       # the R-EXTRA features, the shipped configs, an item respawn
tools/osprunes.sh     # do the OSP four's five runes actually grant and read? (R-132)
```

And one that asserts nothing on purpose:

```sh
tools/watch.sh                     # four bots on q2dm1, and a person watching
tools/watch.sh doors -r sp -m base1
tools/watch.sh ctf-skin -r ctf -m q2ctf1
```

Every check here is a program looking at a program. `tools/watch.sh` puts
q2pro's own client on your display against a server with bots in it and runs one
of three drive scripts, each of which opens with what to look for. That is the
only thing in this repository that can say whether what a player sees looks like
a game.

Every one takes `--control`, or ships its controls inline: a check that has
never failed is not trusted (§10, R-VER-9). `tools/botmatrix.sh` additionally
needs a brain — `gladiator-bot-restored/botlib` built for this platform, its
`bots.cfg`, and an `.aas` per map — and `GLADDIR` says where to find it.

## Installing

The library goes in your **home** directory, not beside the assets:

```
<homedir>/colosseum/game<cpu>.so     e.g. ~/.q2pro/colosseum/gamearm64.so
```

Then `q2proded +set game colosseum +map q2dm1`.

This trips people up, so it is worth stating plainly: Q2PRO searches `homedir`
and its compiled-in `libdir` for a game library, and **never `basedir`** — while
the *filesystem* search path does include `basedir/colosseum`, so assets placed
there are found and a library placed there is not. Putting it beside the assets
gives `Failed to load game library` and four `Can't access` lines, none of which
names the directory you used. See R-BUILD-8.

### And the bots, which install the other way round

The brain is not part of the library and does not live where the library lives.
`BotUseLibrary` calls `dlopen`/`LoadLibrary` on an OS path, and the botlib does
its own file I/O from `basedir` + `gamedir` rather than through the engine — so
everything the bots need goes **beside the assets**, in `basedir`:

```
<basedir>/colosseum/gladiator.so      the brain, built for this platform
<basedir>/colosseum/pak7.pak          its weapon, item, sound and chat configs
<basedir>/colosseum/botcfg/bots.cfg   the roster
<basedir>/colosseum/maps/q2dm1.aas    one navigation mesh PER MAP
```

All four come from `gladiator-bot-restored` (`release/`, `assets/`); `GLADDIR`
is where the harness scripts look for them. Miss `pak7.pak` and the brain loads,
says *"couldn't load the weapon config"* and unloads itself. Miss the `.aas` and
it loads, refuses the map with **`no AAS file available`**, and every bot that
wanted it is destroyed — which reads on the console as `gladiator.so not
available` and no bots.

**There is no auto-bspc.** The `autolaunchbspc` libvar is off by default
(R-SEC-7) and cannot work here even when set: the branch is Windows-only, it
spawns a `winbspc.exe` that has to be in the gamedir, and the reconstructed
brain's `SpawnProcess` is a stub. The assets ship 16 finished meshes — `q2dm1`–
`q2dm8` and `q2ctf1`–`q2ctf8`. Any other map, `base1` included, needs one made
first, and that is two steps: `bspc -bsp2aas` for the geometry, then one load of
the map with the brain, which computes reachability and clustering itself and
writes the finished file (minutes, not seconds). `gladiator-bot-restored/tools/`
has both halves as scripts.

**One client setting, if you play on a listen server.** q2pro pauses the game
whenever the console or a menu is up and exactly one client is connected — it
does not look at `deathmatch`, and bots are fake clients that never enter the
engine's client list, so a server full of them still counts as one. `cl_autopause
0` turns that off. See `doc/reconciliation.md` R-125.

Colosseum loads into unmodified Q2PRO; it requires no engine patch.

## Layout

```
SPECS.md          the specification of record -- read this first
src/              the game library; = q2pro/src/game, merged, plus donor subdirs
inc/              Q2PRO engine headers, vendored verbatim, never edited
tools/            the replay and audit toolchain
vendor/replay/    the three replay bundles: the only copy of the port history
vendor/harness/   the rescued replay harness state (rerere cache, tool inputs)
doc/              provenance, reconciliation, the donor diff, coverage, regression
```

## Credits

* **id Software** — Quake II, and the mission-pack sources
* **Mr. Elusive** (Jean-Paul van Waveren) — the Gladiator Bot and its botlib,
  the bot brain Colosseum is the game side of
* **Zoid** — Threewave Capture The Flag
* **David Wright** — Rocket Arena 2
* **OSP** — Tourney DM
* **skuller and the Q2PRO contributors** — the modern engine and game API this
  is built on

Colosseum replaces none of them, and is not endorsed by any of them.
