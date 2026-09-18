# `server/` — running colosseum, and the engine it asks for

Colosseum is a game library. It runs on a stock Quake II engine, and this
directory is everything around that: an image that builds the engine and the
admin wrapper from pinned source, the engine patches colosseum would like but
does not need, and the tooling that installs a game library, a bot library and
its navigation meshes into a live gamedir.

**No deployment's configuration is in here** — no hostnames, ports, passwords,
rotations or host paths. Those belong to whoever runs a server; this half is
the part that is the same for everyone. Until 2026-09-18 the two lived together
in a `q2server-dockerfile` repository, which is where most of this came from.

| File | What it is |
|---|---|
| `Dockerfile` | Two targets: `server` (engine + q2admin, the default) and `colosseum` (the game library, bot library, assets, bspc) |
| `q2pro/*.patch`, `q2admin/*.patch` | The carried patches, each naming the upstream and commit it applies to |
| `enginepatch.sh` | Checks that they still apply, against the commit each one names |
| `filter-rcon-status.sh` | Drops the console noise that fires on a schedule and carries no signal |
| `docker-compose.example.yml` | The shape of a deployment, to be copied somewhere private and filled in |
| `scripts/deploy-colosseum-live.sh` | Installs the library, bot stack and meshes into a live gamedir |
| `scripts/setup-colosseum-serverdata.sh` | Builds a self-contained data tree to compute meshes in |
| `scripts/make-aas-geometry.sh` | Mesh step 1, off-host: bspc v1.4 over a rotation |
| `scripts/make-aas-reachability.py` | Mesh step 2, on a workstation: ~14–20 s/map |
| `scripts/make-colosseum-aas.sh` | Mesh step 2, on the game host: ~30–45 s/map, needs no Windows |
| `scripts/validate_override.py` | Checks a `.bsp.override` offline instead of on a live map load |
| `scripts/q2_test_client.py` | A real protocol-34 handshake, for exercising the connect path |

## The image

```sh
cd server
docker build -t q2pro-server .                       # engine + q2admin
docker build --target colosseum \
    --build-context colosseum-src=.. -t colosseum .  # the game library
```

Both targets share one build stage, so the engine is compiled once. The server
target needs nothing but this directory — the patches sit beside it in
`q2pro/`. Only the colosseum target reaches for the game source, and it takes
it as a named build context rather than a clone, because BuildKit resolves a
named context only for the stages it actually builds.

q2pro is cloned at a **pinned commit** (`Q2PRO_COMMIT`), and so are q2admin (`Q2ADMIN_COMMIT`) and
everything else the image builds. Bump a pin deliberately and re-test; an
unpinned clone looks reproducible while drifting upstream on every rebuild.
The game libraries are 32-bit, so the engine is cross-compiled to i386 — which
is what `PKG_CONFIG_LIBDIR` in the build stage is for: without it meson finds
the host's 64-bit zlib and the link fails with `libz.so: file in wrong format`.

The build fails rather than shipping a silently-wrong engine: not ELF32, a
`config.h` that disagrees with the requested `GAME_ABI_HACK`, a patch that did
not reach the binary, a TEXTREL in q2admin, a game library missing a cvar that
proves the right tree was built — each is a failed layer, not a warning.

### `GAME_ABI_HACK`

The one build arg that can ruin a server. It selects the i386 struct-return
calling convention for `gi.trace()`, it is a **whole-binary compile-time
switch**, and the engine and the game library it loads must agree:

| Value | For |
|---|---|
| `disabled` | any game library built with a modern gcc — colosseum included |
| `enabled` | old binaries expecting the callee-pops convention (a 2014 Rocket Arena 2 build wanted this) |

Getting it backwards drifts the stack 4 bytes after every `gi.trace()` and the
mod dereferences a bogus `trace.ent`: the server starts fine, then dies on the
first map, inside the *mod's* physics code, looking like a mod bug. One image
cannot serve both conventions — build a second image rather than flipping the
arg.

## The carried patches

**Every upstream this image builds is the real one**, at a pinned commit:
`q2pro/q2pro` for the engine and `packetflinger/q2admin` for the admin wrapper.
Neither is forked. Where something is needed that upstream does not have, it is
a patch in a directory named for that upstream, and each patch documents itself
in its own header — what it fixes, what it does not touch, how it was verified —
and names the commit it applies to.

| Patch | Upstream | What it is for |
|---|---|---|
| `q2pro/0001-status-report-game-created-bots.patch` | q2pro `601a8df8` | bots counted as players in the server browser and in `rcon status` |
| `q2admin/0001-whois-reload-message-to-the-client.patch` | q2admin `15b7a7b5` | upstream tip does not compile: an `edict_t *` reaches a variadic format argument in `g_whois.c`, which GCC 14 rejects |

The two are different in kind, and the table's last column says which is which.
The engine one buys something optional — an engine without it runs colosseum
correctly, the bots just stay invisible to the outside. The q2admin one is not
optional: without it this image does not build at all.

```sh
./enginepatch.sh          # Q2PRO_SRC=../q2pro, Q2ADMIN_SRC=../q2admin
```

It reads the upstream name and the commit out of each patch's own header, picks
the clone that upstream lives in, and tests the patch with `git apply --check`
in a throwaway worktree at that commit — so two patches naming two upstreams
both get an honest answer and the caller's working tree is untouched. A header
naming an upstream with no clone configured is a failure and not a skip.
**Bumping `Q2PRO_COMMIT` or `Q2ADMIN_COMMIT` means refreshing that upstream's
patches in the same commit** — a patch that no longer applies fails the build
rather than being skipped.

**A patch is the shape this takes because a fork is not durable.** The q2admin
line was a cherry-pick on a fork branch until 2026-09-22, and the pin went
stale within days: the branch was rebased, the pinned SHA stopped being
reachable from any branch, and the Dockerfile's own `git clone` plus
`git checkout --detach` failed with "reference is not a tree". A patch either
applies or fails the build, and `enginepatch.sh` asks before a build runs.

### Why an engine patch at all

A Gladiator-derived bot — colosseum's, and every Quake II mod's since
Mr. Elusive's 1999 game source — is a **game** client, not an **engine**
client. `G_SpawnClient` hands it one of the game's client edicts and its
`ClientConnect` runs, but nothing ever connects, so the engine holds no
`client_t` for that slot. Both places the engine reports players from walk its
own client list, which only `SV_DirectConnect` appends to, so a server with
eight bots playing advertises itself as empty.

That is not a q2pro defect. id's 1997 server is identical on the point —
`SV_StatusString` iterates `svs.clients` and requires `cs_connected` or
`cs_spawned`, and `SV_DirectConnect` is the only way into that state — and so
are yquake2 and q2repro. All four `#define SVF_BOT` and none of them reads it.
Quake III solved it from the other side, in the engine: `SV_BotAllocateClient`
takes a real `client_t` and marks it `NA_BOT`, which is a service Q2's
`game_import_t` has no equivalent of. So the choice is an engine patch or
nothing, and this directory is where the engine patches live.

The patch feeds those bots to the UDP status reply, the `N/M` count in `info`,
and `rcon status` — that last one being what a master-server bot polls, so it
is the one that decides what a public listing shows. `set sv_status_show_bots 0`
reverts every reply to stock output without a restart.

## Installing into a gamedir

Two things have to physically sit in the bind-mounted game data directory
rather than in the image, because they are `dlopen`ed from there: q2admin, and
the game library behind it. An entrypoint that copied them in on every start
would silently overwrite a deliberate version choice, so both are install-once
operations. Back up first.

**q2admin**, once per gamedir:

```sh
docker create --name q2admin-extract q2pro-server
docker cp q2admin-extract:/opt/q2admin/gamei386.so <gamedata>/<gamedir>/
docker rm q2admin-extract
```

**colosseum and its bot stack**, which is what `deploy-colosseum-live.sh` is
for:

```sh
Q2_ROOT=~/quake2 SERVER_ROOT=~/quake2-colosseum-server \
    ./scripts/deploy-colosseum-live.sh <gamedir> [ruleset]
```

It goes **behind q2admin**: q2admin stays `gamei386.so` and colosseum is
installed as `gamei386.real.so`, the name q2admin's own `gamelibrary` directive
points at. The script refuses to run if it finds colosseum already in front.
It is idempotent, takes one backup per gamedir (which is also the rollback),
and writes every file through a temp name and `mv` — `install` and `cp` open
the destination `O_TRUNC` and would rewrite the very inode a running server has
its library mapped from. It is parameterised by `Q2_ROOT`, so it can be
rehearsed against a copy of the live tree before it touches the real one.

What it installs is four things, and all four are needed for bots: the library,
`gladiator.so`, `pak7.pak`, and one `.aas` navigation mesh per map. Behind
q2admin the bot list additionally has to be a **loose file**
(`botcfg/bots.cfg`) — q2admin does not forward the engine's filesystem
extension, so colosseum falls back to plain stdio, and that arm cannot see
inside a pak.

Everything server-specific is an input rather than something the script knows:

| Variable | What it supplies |
|---|---|
| `CFG_EXTRA` | a file appended verbatim to the generated `colosseum.cfg` — content layers, rotation file names, arena definitions |
| `MOTD_SRC` | a MOTD, installed only if none is there, checked against the reader's real limits first |
| `MAPLIST_SRC` | a rotation, filtered into `maps.txt` |
| `DEAD_MAPS` | names to drop from it |
| `INSTALL_BSP` | install the mesh source's loose `.bsp` into the gamedir |
| `MAXCLIENTS_MIN` | raise `maxclients` to leave the bot fill headroom |
| `HOSTNAME_SUFFIX` | appended once to `set hostname` |

The ruleset is the second argument, and it decides which of **two differently
named flat bot counts** is written: `minimumplayers` under `arena` and `ctf`,
`bots_minplayers` under the OSP rulesets. Both are registered under every
ruleset, so the wrong one is accepted at the console and then silently ignored.

A MOTD is checked at **nine rows of thirty-two columns**, which is what OSP's
reader takes — `char[9][33]`, anything wider cut at 32 with the rest of that
line discarded, in the game, where nobody testing the deployment sees it.

## Navigation meshes

Bots need one `.aas` mesh per map and there is no way around it: only eight
precomputed meshes have ever been distributed (q2dm1–q2dm8, shipped by OSP
Tourney DM in 1999). The `autolaunchbspc` libvar looks like the answer and is
not — that path is Windows-only, wants a `winbspc.exe` in the gamedir, and the
rebuilt botlib's `SpawnProcess` is an empty stub.

It is two passes:

1. **Geometry** — `bspc -bsp2aas`. Use **bspc.exe v1.4, never the bundled Linux
   `bspc-linux-x86` v1.2.** The submodule ships both and its own README notes
   the Linux one is *older* (v1.2 of 1999-05-20 against v1.4 of 1999-07-18).
   v1.2 fails `FloodEntities` with `**** leaked ****` and writes no `.aas` at
   all on most maps — 9 of 28 in one rotation here — and nothing rescues it:
   not `-nocsg`, `-noliquids`, `-freetree`, `-nobrushmerge` or `-breath`, not
   reading the `.bsp` out of the pak, not `bsp2map` + `map2aas`, not an older
   glibc. It is not the maps either: v1.2 leaks on `q2dm1` and `q2dm2`, two of
   the maps it shipped meshes for in 1999, which v1.4 meshes in about a second.
   v1.4 is a Win32 binary, so this pass runs off-host under WSL interop or
   Wine: `make-aas-geometry.sh`.
2. **Reachability and clustering** — one load of each map with the botlib in
   the game, which rewrites the mesh in place. `make-aas-reachability.py`
   drives a local win32 `q2proded.exe` (~14–20 s/map); `make-colosseum-aas.sh`
   drives a throwaway container on the game host over rcon (~30–45 s/map, and
   needs no Windows machine). They produce the same file.

Four things bite here, all of them learned the hard way:

- **Never ship a geometry-only mesh.** One with an empty reachability lump
  loads happily and leaves the bots with no navigation data — worse than the
  clean failure of no mesh at all. The deploy script refuses them.
- **The finished mesh is *smaller* than bspc's output.** bspc emits every area
  it found; the botlib's pass drops what it cannot reach. A completion check
  that waits for the file to grow waits for ever.
- **The botlib disables itself for the whole session** after the first map it
  cannot load a mesh for — `no AAS file available`, `AAS shutdown` — and never
  retries. Boot a mesh generator onto a map that already has one, restart after
  any failure, and keep unmeshable maps out of a live rotation entirely.
- **The botlib reads the raw `.bsp` itself**, through its own file search of
  `<basedir>/<gamedir>/`, not baseq2 and not the engine's loaded entity string.
  So every rotation map's `.bsp` must be in the *gamedir's* `maps/`, and
  `.bsp.override` files play no part: drive maps by their resolved name and
  name the mesh for that.

The mesh rig — `setup-colosseum-serverdata.sh` and the two step-2 scripts — is
built around two gamedirs called `arena` and `xatrix`, which is the shape it
was developed against rather than a constraint of anything downstream. Another
deployment's gamedirs would need that structure adjusted;
`deploy-colosseum-live.sh` itself takes any gamedir and any ruleset.

## Gotchas worth keeping

- **Don't strip the `script`/`stty -onlcr` wrapper or `init: true`.** `script`
  allocates a PTY so the game's libc line-buffers stdout — otherwise
  `docker logs` arrives in delayed batches. `stty -onlcr` stops that PTY
  turning `\n` into `\r\n`, which breaks any `$`-anchored log filter
  downstream. `init: true` gives the container a real init so SIGTERM reaches
  the game instead of the shell running the pipeline.
- **`homedir` is set to the same path as `basedir`/`libdir`.** q2pro defaults
  it to `~/.q2pro` and checks that first for several things. For the game
  library it falls back; for file reads the game library itself makes, it does
  not — it fails outright. That silently emptied a mod's map list and left a
  server replaying one map after every timelimit.
- **`net_port`, not `port`, controls the listen socket.** q2pro binds
  `PORT_SERVER` (27910) regardless of `port`. Both are set; `port` is still
  needed because the game library reads it.
- **`map_override_path maps`** is required for q2pro to honour
  `.bsp.override`/`.ent` entity overrides. Without it a map that depends on one
  does not resolve at all.
- **Editing a bind-mounted `.cfg` and running `docker compose up -d` does
  nothing.** Compose sees no image change, reports "Running", and the old cvar
  values stay live in memory. Use `docker compose restart`.
- **Use `docker compose` (space), not `docker-compose` (hyphen).** The legacy
  v1 tool crashes recreating containers on modern Docker Engine — and gets far
  enough to stop and rename the old containers before crashing.
- Some legacy admin directives (`addcvarban`, `addcommandban`,
  `sv_max_packetdup`, …) log as `Unknown command`. Harmless — extensions q2pro
  does not implement.

## Checking a server is up

```python
import socket
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.settimeout(3)
s.sendto(b"\xff\xff\xff\xffstatus", ("<host>", 27910))
print(s.recvfrom(4096)[0].decode(errors="replace"))
```

With `0001` applied, bots appear in that reply. Without it they do not, however
many are playing.
