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

**Phase 0 of nine.** The specification is complete and settled;
`SPECS.md` is the specification of record and every requirement carries a stable
ID that code, commits and review notes reference.

What exists today:

* `src/` — the pristine Q2PRO baseq2 game library, 72 files, byte-identical to
  the pinned upstream tree except the regenerated `g_ptrs.c`
* `inc/` — Q2PRO's engine headers, vendored verbatim
* a five-target build (`Makefile`) and an in-tree Q2PRO build (`meson.build`)
* `tools/` — the replay and audit toolchain, wired into the build so a contract
  violation fails it
* `vendor/` — the replay bundles and the rescued harness state that make the
  provenance auditable
* `doc/` — provenance, the reconciliation ledger, and the replay-coverage ledger

No donor has been merged yet. There are no bots yet. It builds a game library
that plays baseq2.

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

The four contract audits run as part of the build, not on request, and a finding
fails it the way a warning does.

Requires: a C compiler, `python3`, and `make`. Cross targets additionally need
`gcc-x86-64-linux-gnu`, `gcc-i686-linux-gnu`, `gcc-mingw-w64-i686` and
`gcc-mingw-w64-x86-64`.

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
