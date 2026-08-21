# OSP Tourney DM

OSP Tourney DM is a tournament/deathmatch mod for Quake II, released by
Orange Smoothie Productions between 1999 and 2000. Its source code was
never published by the original authors; this repository is an independent
reconstruction of it, built to compile and link into a working Quake II
game library that a Quake II server can load in place of the original
binary. It targets the behaviour of the v2.75 release.

## Authenticity

Every reconstructed function is built with the same compilers the original
release used, and checked instruction-for-instruction against the shipped
v2.75 binaries:

- **Linux** (gcc 2.7.2.3): all 1051 exported functions present, 1044 of
  them (99%) byte-identical machine code; the rest differ only in register
  allocation. Nothing missing, nothing invented.
- **Windows** (Visual C++ 6.0 SP3): all 1082 functions located, 1050 of
  them (97%) byte-identical; the remainder differ only in register
  allocation or relocated addresses.

Global variables, data tables, string literals and struct layouts match
both binaries byte-for-byte as well.

## Building

```
make
```

Produces a debug and a release build, under `debug/` and `release/`
respectively, named `game<arch>.so` for the host architecture. Requires
GCC or Clang on a GNU/Linux system; no dependencies beyond a standard C
toolchain. Drop the resulting shared library into a Quake II `baseq2`-style
game directory as `game.so` to run it with a Quake II 3.20-compatible
engine, such as Yamagi Quake II.

## Contents

- The mod's source: a flat set of `.c`/`.h` files plus the `Makefile` that
  builds them.
- `docs/` — the mod's own shipped documentation: README, changelog,
  server-settings and console-command reference.
- `configs/` — the mod's own shipped example server configurations:
  deathmatch/team/1v1/instagib rule sets, MOTD text, bot loadouts, map
  rotation lists, and a player allow/ban list template.

## Provenance

Reconstructed on top of id Software's Quake II 3.20 game SDK, id's Quake II
CTF 1.02 SDK (the grapple hook and menu system), Mr. Elusive's Gladiator
Bot Q2 game SDK (the bot integration layer), and the RSA Data Security MD5
reference implementation (`md5c.c`, used for log verification). Each of
those components carries its own original license terms.

## Disclaimer

Not affiliated with id Software or Orange Smoothie Productions. Provided
for compatibility and preservation purposes.
