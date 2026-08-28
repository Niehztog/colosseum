# Staged exposure — the release process (R-SEC-2, §9 Phase 8)

> "Then a private server, then widened access — **there is no analytical release
> gate**." — SPECS.md §9, Phase 8

That sentence is a decision, not an omission, and §11 risk 18 records what it
costs: *"staged exposure means the first real players are the security test"*,
accepted deliberately. This file is what "staged" means in practice, so that the
stages are something a person can be at rather than something the project is
vaguely doing.

Nothing here is a promise that the software is safe. It is a description of how
many people can be hurt by it being wrong at each step, and of what has to hold
before that number goes up.

## What is already true before stage 0

These are checks, not intentions, and every one of them runs on demand today:

* ten build configurations under `-Werror` on gcc and clang, from `make clean`
* `make check` — 23 audits, every one with controls that make it fail
* `tools/bootmatrix.sh` 20 rows, `tools/smoke.sh`, `tools/playtest.sh` 187
  client-side checks, `tools/botmatrix.sh` 14 rows, `tools/extras.sh` 39
* R-SEC-1..9 discharged, with `bounded.py`, `noexec.py` and `externs.py`
  keeping three of them from coming back
* both R-SEC-2 ledgers entered item by item in `doc/regression.md`

**What none of that is.** No fuzzing of the network-facing parsers. No
adversarial client. No review by anybody who did not write it. The donors' own
authors say the same about their ports — RA2's post-port review "was not
exhaustive" by its author's account — and this project inherits that limit
rather than clearing it.

## Stage 0 — a private server, one operator

**Who can reach it:** the operator, on a LAN or behind a password.
**What it is for:** the failure modes that need a real client and real time, not
a scripted one — a match played to completion, a map rotation that runs
overnight, a savegame reloaded a week later.

**What the box needs before any of that**, because the first attempt at stage 0
spent its evening on two of these rather than on the game. Three of the four are
somewhere other than where the library goes:

| what | where | if it is missing |
|---|---|---|
| `game<cpu>.so` / `.dll` | `<homedir>/colosseum/` | `Failed to load game library` (R-BUILD-8) |
| the brain, `pak7.pak`, `botcfg/bots.cfg` | `<basedir>/colosseum/` | the brain unloads itself: *"couldn't load the weapon config"* |
| an `.aas` per map | `<basedir>/colosseum/maps/` | `no AAS file available`, then `gladiator.dll not available` and no bots |
| `cl_autopause 0` | the client, on a listen server | the console pauses the game (R-125) |

The brain reads its own files from `basedir` + `gamedir` rather than through the
engine, which is why its half of the install is the mirror image of the
library's. The `.aas` files ship with the brain (`assets/maps/`, 16 maps:
`q2dm1`–`q2dm8`, `q2ctf1`–`q2ctf8`); there is no auto-bspc and every other map
needs one made first (R-126). And a dedicated server needs none of the last row:
`cl_autopause` is the client's, and it matters only where the operator is playing
on the same process they are serving from.

Ready to leave stage 0 when, on that server:

- [ ] each of the five rulesets has been played to a natural end at least once
- [ ] `sv extras`, `sv ruleset`, `sv slots` and `sv botperf` have been read on a
      server that has been up for hours, not seconds
- [ ] the log the operator actually wants (`g_gamelog`, or `logfile 2` under
      arena) has been left on for a full session and read afterwards
- [ ] nothing in the console matches `ERROR|bad index|type mismatch|unknown
      pointer|SZ_GetSpace|overflow`
- [ ] the server has been stopped and started again without hand-editing
      anything in the gamedir

## Stage 1 — invited players, password on

**Who can reach it:** people the operator knows, by password.
**What it adds:** the first clients that are not the operator's. Everything that
takes *userinfo* from somebody else starts here — names, skins, the chat paths,
the vote quorum, the menus under contention.

Ready to leave stage 1 when:

- [ ] at least four different people have connected at once, with names and
      skins the operator did not choose
- [ ] somebody has tried the menus of every ruleset that has them
- [ ] a vote has been called, passed and failed
- [ ] a player has disconnected mid-match, mid-vote and mid-round, and the
      server was still serving afterwards
- [ ] the stats and log files have been read and are not full of one player

## Stage 2 — public, listed

**Who can reach it:** anybody.
**What it adds:** clients that are not co-operating, and the first traffic that
is not a game — port scans, malformed connects, unusual protocols.

Before stage 2:

- [ ] the server runs as an unprivileged user, in a directory it does not share
      with anything else
- [ ] `rcon_password` is set and is not a word
- [ ] `serveronlybotcmds` is 1 (its default) and `autolaunchbspc` is unset
- [ ] `sv_cheats` is 0
- [ ] `netlog` is empty — the name still resolves and does nothing (R-SEC-7),
      but an operator who set it expected something
- [ ] a crash leaves a core the operator can read, and the operator knows where

## What widening is not

**Widening is not a schedule.** A stage is left when its list is done, and a
stage can be re-entered: a defect found at stage 2 that needed a client to reach
sends the server back to stage 1 with the fix, not forward with a note.

**A stage is not evidence about the next one.** Stage 0 says nothing about
userinfo, because there is only one client and the operator wrote it. Stage 1
says nothing about hostile traffic. Each list is about what that stage can
actually see.

**And the honest summary of the whole thing:** the analytical work is done —
R-SEC-1..9, both R-SEC-2 ledgers, the audits with their controls. What is not
done, and cannot be done by this project alone, is anybody looking at it who
did not write it. Stage 1 is the first place that can happen.
