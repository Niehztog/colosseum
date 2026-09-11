# Developing Colosseum

Building the library, running its checks, and where things are. `README.md` is the other half: what a server operator needs and nothing else.

* [Getting the source](#getting-the-source)
* [What you need](#what-you-need)
* [Building](#building)
* [The old game API](#the-old-game-api)
* [Building in-tree under Q2PRO](#building-in-tree-under-q2pro)
* [Building the botlib](#building-the-botlib)
* [Warnings and hardening](#warnings-and-hardening)
* [The checks](#the-checks)
* [Source layout](#source-layout)
* [The rules this tree is built on](#the-rules-this-tree-is-built-on)

## Getting the source

The botlib - `gladiator.so`/`gladiator.dll`, the shared library that holds the bot AI - is a submodule, so clone recursively:

```sh
git clone --recurse-submodules https://github.com/Niehztog/colosseum.git
```

After a plain clone, `git submodule update --init` fetches it. It lands at `vendor/gladiator-bot-restored/` and is the source of the botlib, its assets and the map-prep tool - see [Building the botlib](#building-the-botlib). Nothing in the game library depends on it at compile time: the library builds, and the audits pass, in a tree where the submodule was never checked out.

## What you need

| for | what |
|---|---|
| the library | a C compiler, `python3`, `make` |
| the cross targets | `gcc-x86-64-linux-gnu`, `gcc-i686-linux-gnu`, `gcc-mingw-w64-i686`, `gcc-mingw-w64-x86-64` |
| the play-test harness | `go` - its dependencies are vendored, so it builds offline |
| the server-driven checks | a built `q2proded`, retail paks, and for the bot rows a built botlib plus an AAS file per map |
| the in-tree build | `meson` and `ninja` |

## Building

The build produces `game<cpu>.so` / `game<cpu>.dll` / `game<cpu>.dylib`, named the way the engine loads it (`game` + Q2PRO's remapped cpu family + the platform suffix).

```sh
make            # native, debug + release
make windows    # win32 + win64 PE, cross
make macos      # Mach-O dylib - macOS hosts only, not cross-buildable
make everything # all five cross-buildable targets, debug + release
make check      # the contract audits and the g_ptrs.c freshness check only
make help       # what each target is and whether this host can run it
```

Five targets are gating: ELF aarch64, ELF x86-64, ELF i386, win32 PE, win64 PE. A given host can usually only *execute* one of them; `make help` says which. **macOS is the exception to all of it**: Apple's SDK and linker are not redistributable, so `macos` cannot be cross-built and is exercised only where it runs - on the macOS runners of `.github/workflows/release.yml`. There is no 32-bit macOS target because Apple removed i386 from the OS and the toolchain.

`src/g_ptrs.c` is generated. Regenerate it with `cd src && python3 genptr.py $(PTR_SRC) > g_ptrs.c`, never by hand; `make check-ptrs` fails a stale one and runs as part of every build.

## The old game API

Colosseum targets Q2PRO's game ABI, `GAME_API_VERSION_NEW` (3302), and that is the default and the shipped configuration. `API=old` builds the same sources against the classic id ABI, `GAME_API_VERSION_OLD` (3) - `gclient_old_t`, `pmove_old_t`, 32 stat slots - which is what a Q2PRO built without `USE_NEW_GAME_API` wants, and what any 3.20-compatible engine loads. R1Q2 (`GAME_API_VERSION 3`, and its loader refuses every other value outright - the default build is turned away with `Game is API version 3302`) and Yamagi Quake II (`GAME_API_VERSION 3` likewise) both run this build: R1Q2 boots `dm`, `dmpro`, `tdm`, `duel` and `sp`/co-op and answers `sv ruleset`, `sv slots` and `sv extras`, Yamagi loads the library and brings up the map. Those runs are by hand and are not in the battery - the six server-driven scripts below drive Q2PRO, and nothing here regression-tests another engine. Q2PRO's own loader accepts either ABI, so both are real targets.

```sh
make API=old            # native pair, old ABI
make windows API=old    # composes with every target above
make oldapi             # shorthand for `native API=old`
make bothapis           # the same goal at both settings
```

`API=old` builds into `<dir>-oldapi`, and that separation is load-bearing rather than tidy: the switch changes **struct layouts**, so an object from one setting linked against an object from the other reads every field at the wrong offset *and links cleanly*. Header dependencies cannot catch it either - no header changed, so `make API=old` in a tree built as `new` would recompile nothing. The artifact **name** does not change, because Q2PRO looks for `game<cpu><suffix>` and nothing else. An engine with its own convention - Yamagi wants `game.so` - needs the file renamed to that.

**One behaviour differs, by design.** Under `ctf` the second powerup timer needs two stat slots and Threewave already uses 0..30 of the old 32, so Colosseum puts the pair at 32/33 - reachable only on the new ABI, and only for a client that negotiated protocol extensions. Under `API=old` it is dropped and `ctf` loses that display, exactly as upstream does. `sv slots` reports it by name, and its header line carries the api version for this reason:

    ruleset      ctf   (extensions on, api 3, so slots 0..31 are reachable)
      (dropped)  SID_TIMER2_ICON -- map says 32
      (dropped)  SID_TIMER2 -- map says 33

Note `extensions on` beside `0..31`: the two switches are independent, so a client can negotiate 64 slots from a library whose array holds 32. Every other ruleset stays inside 32 and is unaffected. Savegames are **not** portable between the two builds and are refused rather than misread (`SAVE_VERSION` already differs).

A second ABI also earns its keep as a check. `-Warray-bounds` at `API=old -O2` refused one line, and under it was a defect that the audits, the build configurations, the boot matrix and the play-test battery had all passed: the tourney runes had **never worked**, and any of the OSP four with `runes 1` died at map load with `ED_Alloc: no free edicts`. Everything that could have caught it asked the stat map, and the stat map was right.

## Building in-tree under Q2PRO

`meson.build` mirrors `q2pro/src/ctf/meson.build`: drop this tree at `q2pro/src/colosseum` and it builds through the same mission-pack mechanism, sharing `game_shared_src` and `engine_inc` rather than compiling its own copies of `shared.c` and `m_flash.c`.

**The standalone `Makefile` stays primary** - it is the one that covers all five build targets, both settings of the game ABI, and the contract audits. The two file lists must stay equal, and the header of `meson.build` carries the `diff` that proves it.

## Building the botlib

The botlib is the submodule at `vendor/gladiator-bot-restored/`, built separately and for the same platform as the game library - the loader says so when the bitness does not match:

```sh
cd vendor/gladiator-bot-restored
make botlib        # -> release/gladiator.so (.dll on Windows, .dylib on macOS)
```

**A release package carries this build already** (R-BUILD-11): every packaging job checks the submodule out, runs that same `make botlib` with the compiler it has just built the game library with -- and `YQ2_ARCH` beside it on the 32-bit Linux row, where the submodule would otherwise read the host's architecture off `uname -m` -- and `.github/package.sh` stages the result into the package's `colosseum/`, under the name the game `dlopen`s: `gladiator.dll` on Windows and `gladiator.so` everywhere else, macOS included, where the submodule emits a `.dylib` and the staged copy is renamed. Build it here to develop against it, or for a platform no package covers; an operator running a release does not have to.

Three more things come from that submodule, and `README.md` says where each one is installed: `assets/pak7.pak` (the botlib's weapon, item, sound and chat configs), `assets/bots.cfg` (the bot list) and `tools/vendor/bspc/` (the 1999 map-prep tool). **AAS files are per map**, and the release packages carry eight of them: `.github/aas.sh` fetches OSP Tourney DM's precomputed q2dm1..q2dm8 into `colosseum/maps/` before packaging, checksums them against `.github/aas.sha256`, and `package.sh` re-verifies the staged copies. That directory is gitignored - the AAS files are OSP's data and are not carried in the tree. Every other map needs one made; the README's [Bots](README.md#bots) section is the procedure.

The game<->botlib interface is `docs/botlib-contract.md`, at version 3, and `tools/botabi.py` checks it in `make check`: every slot, the preprocessor condition that selects `Trace`, and every struct size - the last by compiling a probe against the botlib's own headers rather than trusting a number in a document. It skips itself with a message when the submodule is not checked out - which the release workflow no longer does, so the contract is compared at release time against the pin the shipped botlib was built from.

## Warnings and hardening

Warnings are on and fatal: `-Wall -Wextra -Werror`, clean under both `gcc` and `clang`, with three suppressions for inherited code that has to stay byte-identical to upstream - `unused-parameter` (Quake II's callback signatures are fixed by their function-pointer types), `sign-compare` (all read, all benign) and `missing-field-initializers` (the `itemlist[]` sentinels). Each is documented in the `Makefile` with the count it silences. `_FORTIFY_SOURCE=2` is on for release builds under `gcc`; it cannot be used with `clang`, for a reason worth reading in the `Makefile` if you ever name a struct member `dprintf`.

The two PE artifacts are checked for what they **import** the moment they are linked (`tools/pedeps.sh`): a game DLL that needs a compiler runtime shipped beside it is not shippable, and both of ours wanted `libssp-0.dll` until the stack protector's runtime was linked in statically. That failure is invisible from a Linux host - the DLL links and every other check passes - so the check runs in the build rather than on request.

## The checks

The contract audits run as part of the build, not on request, and a finding fails it the way a warning does. Release packages are built by that same Makefile, so a finding stops a release too - and that is rehearsable without cutting one: `gh workflow run release.yml -f version=<name> --ref <ref>` runs every build and packaging job of `.github/workflows/release.yml` and leaves the seven packages as run artifacts, because the publish job is conditioned on the ref being a tag. Nothing is published and no tag is created; `gh run watch` follows it. Most of them ship a positive control that makes them fail, and each control is a run of its own. **`audit.py` prints the count when it finishes and that is the figure to quote** - the figures that used to be written down here had drifted by the time anything re-read them, so they are gone rather than corrected into the next stale pair. Section 9 of `SPECS.md` says what each check is for.

`make check` is static. Six scripts drive a real `q2proded`, and none of them is part of the build because each needs a built engine, retail paks and a minute or more. **Every one of them prints its own total when it finishes, and that is the figure to quote.**

```sh
tools/bootmatrix.sh   # every ruleset x xatrix x rogue boots and reports back
tools/smoke.sh        # one map per ruleset, with a savegame round trip
tools/playtest.sh     # the client-side battery, through headless clients
tools/botmatrix.sh    # 1, 16 and 32 bots per ruleset: spawned, played, removed, timed
tools/extras.sh       # the Gladiator extras, the shipped configs, an item respawn
tools/osprunes.sh     # do the OSP four's five runes actually grant and read?
```

And one that asserts nothing on purpose:

```sh
tools/watch.sh                     # four bots on q2dm1, and a person watching
tools/watch.sh doors -r sp -m base1
tools/watch.sh ctf-skin -r ctf -m q2ctf1
```

Every check here is a program looking at a program. `tools/watch.sh` puts q2pro's own client on your display against a server with bots in it and runs one of three drive scripts, each of which opens with what to look for. That is the only thing in this repository that can say whether what a player sees looks like a game.

**Every one takes `--control`, or ships its controls inline: a check that has never failed is not trusted.**

All of them read the same environment, all defaulted:

| variable | default | what |
|---|---|---|
| `Q2PRO_BUILD` | `../q2pro/builddir-native` | the directory holding `q2proded` |
| `Q2DATA` | `/usr/share/games/quake2/baseq2` | retail baseq2 paks |
| `CTFDATA` | `/usr/share/games/quake2/ctf` | Threewave paks |
| `XATRIXDATA`, `ROGUEDATA` | beside `Q2DATA` | mission-pack data, for the layer scenarios |
| `GLADDIR` | `vendor/gladiator-bot-restored`, else a sibling checkout | the botlib, its assets and its bot list |
| `LIB` | `release/game<cpu>.so` | the library under test |

`tools/playtest.sh` drives the Go harness in `tools/playtest-harness/`, which ships more scenarios than the six scripts run; `docs/playtest.md` lists them. One thing they need that this project does not carry is the Rocket Arena map pack: the `ra2map*` maps and the team skins are RA2's own distribution, and a scenario that wants one says so rather than failing silently. **OSP Tourney is not in that category**: it ships no assets at all, every path its code names is baseq2's, and the gamedir its scenarios run in - `colosseum`, the same one every other scenario uses - needs nothing in it but the library.

`tools/playtest.sh` cannot make a monster fight, so monster-AI changes are verified by donor comparison and must say so rather than implying a scenario covered them.

## Source layout

```
SPECS.md          the specification of record - read this first
src/              the game library; = q2pro/src/game, merged, plus donor subdirs
inc/              Q2PRO engine headers, vendored verbatim, never edited
tools/            the replay and audit toolchain
tools/playtest-harness/
                  the Go play-test harness the scripts drive, dependencies
                  vendored so it builds offline
tools/banners/    the banner renderer; it draws from the retail assets and
                  commits no image - the one the README shows lives in
                  .github/banner.png
vendor/gladiator-bot-restored/
                  the botlib, as a submodule: botlib source, assets, bspc
vendor/replay/    the replay bundles the divergence and coverage tools read
vendor/harness/   the replay harness state (rerere cache, tool inputs)
docs/             the cvar, command and entity inventories, the botlib contract,
                  the donor diff, the divergence matrix and the play-test
                  scenario list
colosseum/        the default gamedir config set that ships to operators
```

## The rules this tree is built on

| file | what it governs |
|---|---|
| `SPECS.md` | the specification of record; every requirement carries a stable ID |
| `docs/donor-fdiff.md` | the function-by-function behavioural diff against each donor |
| `CLAUDE.md` | how to decide a question the two above do not already answer |

A behaviour decision is stated in `SPECS.md` as the requirement it serves, and at the site it touches, in the same commit as the code.
