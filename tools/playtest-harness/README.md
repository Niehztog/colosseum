# Play-test harness

The Go side of the play-test battery. `tools/playtest.sh` and `tools/osprunes.sh` drive it; they default `$HARNESS` to this directory, so neither needs anything installed outside the repository.

Each subdirectory of `scenarios/` is one `main` package: it starts `q2proded` against the built game library, connects one or more headless clients through [libq2](https://github.com/packetflinger/libq2), drives them, and asserts on what comes back over the wire -- prints, menus, configstrings, player origins. `playtest/`, `colosseum/` and `ra2/` are the shared helpers those scenarios use.

    cd tools/playtest-harness
    go run ./scenarios/botfill -h

`docs/playtest.md` lists the scenarios this tree ships and what they assert; the generic method is documented by the `q2-playtest` skill.

## The two local patches to the vendored libq2

`patches/libq2-v1.0.335-netchan.patch` changes libq2's `Bot` in two ways:

* **`NetMu`** guards the outgoing message and the sequence state. Three goroutines write them -- the receive loop, `Run`'s frame timer, and a scenario's `Cmd()` through `AddClientString` -- and unguarded, a string command could land in the middle of a usercmd.
* **`ReadErr()`** says why the receive loop stopped. It used to return without a word on a read error or a packet it could not parse, while `Run` went on sending usercmds, so the client stayed connected and heard nothing. Every `playtest.Bot` wait now names that reason when it times out.

`patches/libq2-v1.0.335-entities.patch` makes the frame's entity set honest, which `Bot.EntityModel` -- and so `ra2eyecam`, whose question is which entity a client is shown -- depends on:

* **A removed entity leaves the merged set.** `ParsePacketEntities` cloned every entity of the delta frame and then never deleted the one the server sent with the remove bit, so a player who left a client's view lingered there with whatever state it had when it left.
* **An entity entering a frame starts from its baseline**, as the protocol codes it, not from nothing. q2pro builds each client's baselines AT CONNECT from the entities live then (`SV_CreateBaselines`), so a player's baseline already says modelindex 255, the server leaves the field out, and an entity coming back into view read as modelindex 0. The baselines are kept in the frame history under `message.BaselineFrame`, which the history's pruning skips.

Both patches are applied to `vendor/`, in this order, and have to be re-applied after `go mod vendor`:

```sh
patch -d vendor/github.com/packetflinger/libq2 -p1 < patches/libq2-v1.0.335-netchan.patch
patch -d vendor/github.com/packetflinger/libq2 -p1 < patches/libq2-v1.0.335-entities.patch
```

`playtest.Bot.Cmd` names `NetMu` and `playtest.Bot`'s waits call `ReadErr`, so a refresh that loses the first patch does not compile. The second has no such tripwire: a refresh that loses it compiles, and `ra2eyecam`'s visibility checks fail on a correct build. The netchan patch is [libq2 PR #7](https://github.com/packetflinger/libq2/pull/7) plus the three-line notice it puts at the top of `bot.go`, which Apache-2.0 §4(b) asks of a changed file, and it goes away when a libq2 release carries that PR; the entities patch carries the same kind of notice on the three files it touches, and has no upstream PR yet.

## Why `vendor/` is committed

libq2 is pinned to an upstream release -- [packetflinger/libq2 `v1.0.335`](https://github.com/packetflinger/libq2/releases/tag/v1.0.335) -- which is the first one carrying the vanilla-handshake protocol fixes the harness depends on; they were a fork until [PR #6](https://github.com/packetflinger/libq2/pull/6) merged them. `vendor/` holds that release's code and `go build` and `go run` use it by default, so the harness builds with no network access and no second checkout. Refresh it with `go mod vendor` after changing the version in `go.mod`.

## Paths

Scenarios that compare Colosseum against a reference build (`ospfixes`, `ospreconnect`, `ospthink`) default to sibling checkouts of the repository -- `../../../q2pro`, `../../../yquake2` -- and every one of those defaults is a `-flag` you can override. `ra2botvote` finds its `.aas` by glob, or from `$RA2AAS`; `botfill` and `ospbotvote` take a `-aas` directory or file and fall back to `$Q2AAS` and the places a run of this tree leaves one.
