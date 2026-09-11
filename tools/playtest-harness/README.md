# Play-test harness

The Go side of the play-test battery. `tools/playtest.sh` and `tools/osprunes.sh` drive it; they default `$HARNESS` to this directory, so neither needs anything installed outside the repository.

Each subdirectory of `scenarios/` is one `main` package: it starts `q2proded` against the built game library, connects one or more headless clients through [libq2](https://github.com/packetflinger/libq2), drives them, and asserts on what comes back over the wire -- prints, menus, configstrings, player origins. `playtest/`, `colosseum/` and `ra2/` are the shared helpers those scenarios use.

    cd tools/playtest-harness
    go run ./scenarios/botfill -h

`docs/playtest.md` lists the scenarios this tree ships and what they assert; the generic method is documented by the `q2-playtest` skill.

## Why `vendor/` is committed

libq2 is pinned to an upstream release -- [packetflinger/libq2 `v1.0.335`](https://github.com/packetflinger/libq2/releases/tag/v1.0.335) -- which is the first one carrying the vanilla-handshake protocol fixes the harness depends on; they were a fork until [PR #6](https://github.com/packetflinger/libq2/pull/6) merged them. `vendor/` holds that release's code and `go build` and `go run` use it by default, so the harness builds with no network access and no second checkout. Refresh it with `go mod vendor` after changing the version in `go.mod`.

## Paths

Scenarios that compare Colosseum against a reference build (`ospfixes`, `ospreconnect`, `ospthink`) default to sibling checkouts of the repository -- `../../../q2pro`, `../../../yquake2` -- and every one of those defaults is a `-flag` you can override. `ra2botvote` finds its `.aas` by glob, or from `$RA2AAS`.
