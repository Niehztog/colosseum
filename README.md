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

**Phase 6 of nine.** `SPECS.md` is the specification of record and every
requirement carries a stable ID that code, commits and review notes reference;
`doc/reconciliation.md` records every decision the merges needed and why.

What plays today, from one library, chosen by `g_ruleset`:

| ruleset | what it is |
|---|---|
| `dm` | baseq2 deathmatch |
| `ctf` | Threewave Capture The Flag 1.52 |
| `arena` | Rocket Arena 2 v2.25 |
| `tourney` | OSP Tourney DM v2.75 |
| `sp` | the three campaigns — baseq2, The Reckoning, Ground Zero — single and co-op |

`xatrix` and `rogue` are content layers, orthogonal to all five and valid with
any of them.

**And bots.** `src/bot/` is the game side of Mr. Elusive's botlib: it loads the
brain dynamically, redirects twenty slots of the game import so the brain sees
everything the game does, spawns fake clients, drives them through the frame
loop and gives them the 1999 command set and menu. On `q2dm1` under `dm` they
load, spawn, navigate and fight.

On `q2dm1` under `dm` they load, spawn, navigate, fight and chat. Bots in `ctf`,
`arena` and `tourney` — team assignment, arena rosters, the tourney queue — are
Phase 7's; they do spawn and are removed cleanly in all four rulesets today.

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

Warnings are on and fatal: `-Wall -Wextra -Werror`, clean under both `gcc` and
`clang`, with three suppressions for inherited code that R-CORE-5 forbids
editing — `unused-parameter` (594 hits, Quake II's callback signatures are fixed
by their function-pointer types), `sign-compare` (28, all read, all benign) and
`missing-field-initializers` (2, the `itemlist[]` sentinels). Each is documented
in the `Makefile` with the count it silences. `_FORTIFY_SOURCE=2` is on for
release builds under `gcc`; it cannot be used with `clang`, for a reason worth
reading in the `Makefile` if you ever name a struct member `dprintf`.

The contract audits run as part of the build, not on request, and a finding
fails it the way a warning does. Seventeen of them, each with a positive control
that makes it fail — `tools/audit.py --help`, and §10 of `SPECS.md` for what
each one is for.

Requires: a C compiler, `python3`, and `make`. Cross targets additionally need
`gcc-x86-64-linux-gnu`, `gcc-i686-linux-gnu`, `gcc-mingw-w64-i686` and
`gcc-mingw-w64-x86-64`.

### Running the checks that need a server

`make check` is static. Four scripts drive a real `q2proded`, and none of them
is part of the build because each needs a built engine, retail paks and a
minute or more:

```sh
tools/bootmatrix.sh   # 20 rows: every ruleset x xatrix x rogue boots and reports back
tools/smoke.sh        # one map per ruleset, with a savegame round trip
tools/playtest.sh     # 139 assertions through headless clients (needs the q2-playtest skill)
tools/botmatrix.sh    # 1 and 16 bots per ruleset, spawned, played and removed
```

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

Colosseum loads into unmodified Q2PRO; it requires no engine patch.

## Layout

```
SPECS.md          the specification of record -- read this first
src/              the game library; = q2pro/src/game, merged, plus donor subdirs
inc/              Q2PRO engine headers, vendored verbatim, never edited
tools/            the replay and audit toolchain
vendor/replay/    the three replay bundles: the only copy of the port history
vendor/harness/   the rescued replay harness state (rerere cache, tool inputs)
doc/              provenance, reconciliation, coverage, regression
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
