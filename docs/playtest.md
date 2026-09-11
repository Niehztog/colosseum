# Play-testing Colosseum

The generic method -- how the harness works, how to write a scenario, and the protocol-level traps that cost an hour each -- is documented by the `q2-playtest` skill. **This file is the half that is Colosseum's**: the scenarios this tree ships, the two `sv` diagnostics they read, and the findings that came out of pointing the harness at this merge.

The harness itself is `tools/playtest-harness`, with its dependencies vendored; `tools/playtest.sh` and `tools/osprunes.sh` are the versioned wrappers, and both default `$HARNESS` to it.

## The scenarios this tree ships

Forty-seven, each one a `main` package under `tools/playtest-harness/scenarios/`. Four are generic diagnostics rather than checks: `mapinfo` lists a map's spawn points grouped by the arena key they carry, read straight out of a pak; `csdump` connects once and prints every configstring the server sent, so the numbering itself can be read; and `netlag` points at a server that is ALREADY RUNNING -- `-host`/`-port`, starting nothing -- and reports what a client there experiences. It is how "it felt laggy on that one" becomes a number: the pm_type and pm_flags the server holds the client on, the distribution of snapshot arrivals against the 100 ms server frame, and the delay from an input to its effect.  `liveprobe` points at a running server the same way and takes ONE reading -- `pm_type`, the configstrings, the playerstate stats and the layout -- which is how "the server is up but nothing is happening" becomes `pm_type = 4 (FREEZE)`, the intermission, with the map name it has been stuck on.  It sends no button, deliberately: under every ruleset but tourney and `arena` a press is what ENDS an intermission, so a probe that pressed one would destroy the state it was sent to look at.

**The battery, and the two content layers:**

| Scenario | What it asserts |
|---|---|
| `scenarios/colosseum` | The ruleset battery: every ruleset, plus three control servers, exit 0 or 1. It prints its own total when it finishes, and that is the figure to quote |
| `scenarios/connectlog` | What the server's own console says about a session (R-LOG-1): one `(name connected from <ip>)` per arrival and one matching line per departure, under all seven rulesets, with the client's real address and `SERVER_BOT` where a bot's would go. It COUNTS rather than greps -- the defect it closes was a duplicate -- and its extra rows are the three places the address can be lost that a per-ruleset sweep does not reach: a coop spawn, a level change, and a drop the mod starts rather than the engine |
| `scenarios/layeracc` | Do the loadout cvars reach tourney's accuracy report, and does a content layer's OWN map spawn its own content? Two phases, because the two halves of the layers need different evidence |
| `scenarios/botfill` | Does the bot count follow the GAME rather than the server, and does the switch being off still mean the flat count? |
| `scenarios/botmenugate` | Who may open the bot menu, and who may not? The menu gates `menu` on the rcon password rather than on `serveronlybotcmds`, which is the 1999 shape |
| `scenarios/votematrix` | THREE vote systems, seven rulesets, and the head count every one of them divides by. Each ruleset is driven up a ladder -- one connected player, then two, then three -- putting the same proposal at each rung. 117 checks |

**Threewave CTF:**

| Scenario | What it asserts |
|---|---|
| `scenarios/ctfidview` | Does a CTF library address configstrings through the layout in force, or through the compile-time macro? Ten checks over three bases -- general, and the statusbar tail the match timer and the team warning borrow -- calibrated off the wire, with a 4-v-2 and a `competition 2` control server. Catches a server that dies on the first connect and prose written into the model table |
| `scenarios/ctfgrapple` | Is an "always owned, never in the world" item precached at map load, or one configstring at a time mid-round? Snapshots the configstring set per phase (load / join / select / fire) and names which asset arrived when |
| `scenarios/ctfteams` | Does a CTF bot know whose side it is on, and does the offhand hook have a client half? Both are about what the SERVER hands out |

**OSP Tourney DM:**

| Scenario | What it asserts |
|---|---|
| `scenarios/osprunes` | Do OSP Tourney's five runes actually grant and read the slots the map names? Reads stat VALUES off the wire, in both signs |
| `scenarios/ospmapchange` | Do OSP Tourney's four MANUAL map changes load the map that was asked for -- a passed `vote map`, a referee `r_map`, the admin menu's map choice, a passed `vote config`? Writes its own `maps.txt`/`serverconfigs.txt` fixtures, resets the level between rows so none inherits the last one's landing place, and asserts the MECHANISM as well as the outcome off a console line only one code path can print |
| `scenarios/ospfixes` | The battery for the tourney fixes, run against BOTH engine families from one binary: half the rows are only interesting if the repair also holds on an engine that predates Q2PRO's game ABI |
| `scenarios/ospenter` | Under the OSP four, CONNECTING IS NOT ENTERING. A client arrives as an observer and enters through a command; everything tourney knows about a client keys off that state |
| `scenarios/ospwarmup` | What a tourney OBSERVER may do, and what a match START leaves behind |
| `scenarios/ospobsmodes` | Sweep an observer through every mode it has and ask, in each one, whether the VERTICAL axis still answers the mouse |
| `scenarios/ospcamera` | The chase camera has CONTROLS -- distance, free-look, in-eyes, target cycle -- and this is what they do |
| `scenarios/ospclock` | Is the match clock on screen during a running match? A missing HUD widget has four independent places it can be missing from |
| `scenarios/ospmenu` | Drive the pop-up menu with real clients. The tourney menu is a LAYOUT, not the statusbar RA2 draws into |
| `scenarios/ospff` | Does `team_hurtteam` actually stop a shot, or only describe itself? Friendly fire is decided per team, not from dmflags |
| `scenarios/ospscore` | Three claims about the tourney integration that only the running game can settle, starting with whether `resp.clientid` collapses to 0 for every client |
| `scenarios/ospchatlog` | Do the three stats-log events reach the file, and does the cvar that gates one of them still gate it? A log function with callers that no donor site calls is what this hunts |
| `scenarios/ospthink` | Does a HUMAN client's `ClientThink` run at all? |
| `scenarios/ospreconnect` | Does a headless client survive a level change? A level change is not a disconnect: the server stuffs `changing` and `reconnect` and asks for the spawn handshake again |

**Rocket Arena 2:**

| Scenario | What it asserts |
|---|---|
| `scenarios/ra2spawn` | Round spawns are chosen by where the *fighters* are, not the audience |
| `scenarios/ra2join` | Smoke test: seat N clients in an arena, check they all land on real spawn points and the server survives |
| `scenarios/ra2twins` | Do two arrivals land on the same spot, and can they walk out of each other? Reports overlapping pairs, then asks every client to move |
| `scenarios/ra2pickuptwins` | The same question on a PICKUP arena, which is the other selector and the sharper case: spawns are chosen by side rather than by distance |
| `scenarios/ra2waitroom` | Where does a player killed MID-ROUND land, and can it move? The other half of `ra2twins` |
| `scenarios/ra2menuleak` | Does a respawn free the menu it throws away? Drives respawns through `spectator 1` (RA2 dispatches no `kill`) and reads the live allocation count out of Q2PRO's `z_stats`, then checks the menu still opens and closes |
| `scenarios/ra2queuefire` | Can a client waiting out a round shoot the fighters it stands among? Asserts in both signs off `ps.gunframe`: an observer's never leaves zero, a fighter's does |
| `scenarios/ra2prefire` | Can a PERSON shoot during the round countdown? Damage is granted exactly once per round, on the frame the countdown reaches zero |
| `scenarios/ra2holdfire` | The same question asked of a BOT, plus whether a shot before the bell is spent ammo and nothing else |
| `scenarios/ra2teamfire` | Can a bot kill somebody on its own side? Needs the real botlib in the game, because a headless client cannot decide to shoot a team-mate |
| `scenarios/ra2observer` | What a client can see and do once it stops fighting, on a pickup arena -- the one kind with no waiting room to be sent to |
| `scenarios/ra2camera` | What a client is given on spawn, what its observer camera does when the mouse moves, and whether a team-mate can hurt it |
| `scenarios/ra2trackcam` | Does the follow camera actually follow? It positions itself 150 units behind the tracked player by writing `velocity`, and nothing in the game integrates that |
| `scenarios/ra2packweap` | Does the arena settings menu offer the mission packs' six weapons exactly when their content layer is switched on? An RA2 menu IS the client's statusbar, so only a client can witness the rows |
| `scenarios/ra2botvote` | Does an arena's own `bots` switch actually keep bots out? The refusal lives in five separate places |
| `scenarios/nextlevel` | Does the level end when there is nobody to press a key (R-RA-10, R-OSP-15, R-CTF-9)? Three phases: `arena` with only bots, which never send BUTTON_ANY; `dm` where the last client quits while the scoreboard is up, after which no ClientThink runs at all; and `ctf`, the same hole reached by a third ruleset. All three fail on the library without the fix |
| `scenarios/ra2botchat` | How many times does one bot chat line reach a client? The dedicated console cannot tell four sends from one |
| `scenarios/ra2gslog` | Does the stdlog record what it claims to? `gslog.c` has six entry points and a dropped call site leaves a function that compiles, links and never runs |
| `scenarios/ra2gskill` | The sixth entry point `ra2gslog` cannot reach: does a round still log DEATHS? |
| `scenarios/ra2reachscore` | The scoreboard, the map change, and the twenty seconds in which a bot is a client that has never begun -- the window the botlib spends computing reachability |


Two helper packages hold the knowledge those scenarios share:

| Path | What |
|---|---|
| `colosseum/ruleset.go` | The ruleset dispatch: pick a ruleset, read `sv ruleset` / `sv slots` back |
| `colosseum/kill.go` | `KillAndConfirm`: `kill` is silently refused for five seconds after a respawn, so a single one is not a death |
| `ra2/join.go` | Rocket Arena's own menu choreography (there is no console command for joining). `ra2.PickupTeam` joins `#<n> Pickup Red` / `#<n> Pickup Blue`, which is the only way into a pickup arena -- `AddtoArena` refuses one from the arena menu with "You must join a pickup team to enter that arena", and it is also the only way to exercise the side-based `SelectRandomArenaSpawnPoint` path |

## Running the battery

The battery takes retail paks rather than a mod install, and defaults to the system ones:

```bash
cd tools/playtest-harness
Q2=<q2pro>/builddir-native/q2proded

go run ./scenarios/colosseum -q2proded $Q2 \
    -lib <colosseum>/release/game<cpu>.so                 # every ruleset
go run ./scenarios/colosseum -q2proded $Q2 -lib ... -rulesets arena -keep
```

## Testing one binary that serves several rulesets

Colosseum is one game library with seven rulesets -- `dm`, `dmpro`, `tdm`, `duel`, `ctf`, `arena`, `sp` -- chosen at load by `+set g_ruleset <name>` and latched for the life of the map. A test written for one of them compiles and runs under all of them, so the same binary has to be tested once per ruleset. Two `sv` commands make the dispatch readable from outside the process:

```
sv ruleset     the resolved ruleset, the modifier matrix, the content layers,
               level.framenum, and an entity/monster/corpse/gib census
sv slots       the active ruleset's stat-slot map and the composed statusbar,
               with every row resolution dropped and why
```

`sv slots` also reports the game ABI the library was built against, and `Slots.Api` / `Slots.Reach()` parse it. That matters because how many stat slots exist is **two** independent facts, not one: the wire carries 32..63 only to a client that negotiated protocol extensions, and the array only holds them on a library built against the new game API (`make API=old` is a supported configuration where it does not). `Reach()` is the lower of the two. A check written as "extensions are on, therefore slot 33 is live" is true on one ABI and reports correct behaviour as broken on the other -- which the ctf timer-pair row did until it learned to ask.

Both print to the server console, so `Server.Console` plus `Server.WaitLog` is the whole recipe. Assert on those rather than on the HUD: a HUD that looks right proves nothing about which slot a stat landed in.

**Cross-ruleset leakage is the failure worth hunting.** Every donor's fields are in one merged `gclient_t`, so a test written for one ruleset compiles and runs under all of them. The instructive assertion is negative: boot `dm` and check the thing only `arena` should do did *not* happen. The boot matrix does this by comparing entity counts -- `arena` frees every pickup item, so `q2dm1` is 37 edicts there against 120 under `dm`, and that difference is a one-line check that the gate is live.

`colosseum/ruleset.go` holds the durable half: the rulesets and the map each can hold a match on, parsers for both `sv` commands, and a `Stats` table of what each ruleset's slot map must contain **and must not**. The Forbid half is the point -- "ctf maps `SID_CTF_TEAM1_PIC`" is a fact about the table, which has a ctf column either way; "dm does not" is the half that fails when a gate goes missing.

**Give the diagnostic time to finish printing.** Both commands print a block, and the line a scenario waits on is not the last one: the ctf census and the `!!` complaints come after `world`, and the statusbar text comes after `statusbar`. Parsing the instant the awaited line lands reads a half-arrived block, which is a flake rather than a failure. Wait, then sleep ~400ms, then parse.

**Some entities are not there at boot.** CTF scatters its techs from a thinker two seconds into the level, so a census taken the moment the map is up reports `0 tech(es)` and is not wrong -- it is early. Ask again later.

**Compare the client's statusbar with the one the server says it composed.** `sv slots` prints the composed bar; the client receives it in `CS_STATUSBAR`. Byte-identical is a much stronger claim than "it looks like a bar", and it is the only check that catches a bar truncated on the way out.

## What the harness found here

**Three donors hardcoded a configstring index.** Without protocol extensions `CS_PLAYERSKINS` is 1312 and `CS_GENERAL` is 1568; with them they are 12862 and
13118. Watching index 1312 while a client changes skin tests the team-skin path end to end -- and found three donors addressing it by literal, one of them in hex.

**A mode cvar with no range check is a server that lies about itself.** The sharpest thing the battery found was not a crash: `match_mode 4` on a tourney server announced the 1-vs-1 banner and put `1-vs-1` in `match_type` serverinfo -- which is what a client and a server browser read -- while running none of the duel code. The check that catches it is three assertions per mode (banner, serverinfo string, which commands that mode accepts and refuses) plus one boot per out-of-range value. Generalised: for any mod with a mode selector, test the values outside the range, not only the ones inside it.

**A declared feature switch that gates nothing.** The `runes` modifier is reported by a diagnostic, refused where the matrix forbids it, and consulted by no gameplay code at all -- both would-be consumers read their own cvar instead. A modifier that only prints itself passes every test that asks the diagnostic, so ask the *world* instead: are the techs there, are the runes there.

**...and the sharpest form of that is a diagnostic that is CORRECT and still tells you nothing.** The tourney runes live in five stat slots resolved through a per-ruleset map. `sv slots` reported them at 22..26 and the map really did say 22..26 -- the diagnostic was not vague or stale, it was right. What it could not report is that *nothing ever wrote there*: the pickup passed a slot number where a logical id belonged, so it set a stat no ruleset had, and the gameplay code indexed the array with the id instead of the slot, so it read five slots the map had given to something else. Two of those belonged to the second powerup timer, so holding a pent read as holding two runes. Five phases of audits, a twenty-row boot matrix and the whole client battery all passed, because every one of them asked the map and the map was fine. `Bot.Stat(n)` is the answer: take the slot number from the mod's diagnostic and read its VALUE off the wire. `scenarios/osprunes` is the worked example, and it asserts in both signs -- give a rune and see its own slot move, then hold two powerups with no rune and see all five stay zero. The negative half is the one that fails on a broken build.

**A donor writing the same table at fifteen call sites.** The accuracy report was collected by fifteen inline copies of a five-line block across two shared files, each with its own copy of the guard and three without one -- and three of the fifteen double-counted. When a port carries that shape forward it carries the inconsistencies too; when it reduces the shape to one function at the choke point they were all feeding, the inconsistencies show up as differences to decide.

**A round only ends when a team is wiped.** Bots that do not shoot will hold one round forever, so a scenario gets exactly one placement per round. To force a fresh placement, disconnect a fighter: the emptied team ends the round and the next one starts with whoever else is queued -- which is how `ra2spawn` gets an audience seated *before* the fighters are placed.

**"Farthest from any player" is a minimum over players, so distant players cannot change it.** Seating decoys in a different arena to prove a server-wide scan is wrong does not work: they are never the nearest player to any candidate spot, so the minimum is unaffected. Only players standing in or near the arena under test move the result.

**A threshold is a claim about a head count, and one client cannot test it.**
Colosseum carries three vote systems -- tourney's `vote <what>` percentage,
Threewave's election, and Rocket Arena's per-arena proposal -- and every one of
them divides by a count of the people on the server. One connected client makes
every divisor 1 and every threshold 100%, which is the one head count at which
all three agree *and at which they would still agree if the arithmetic were
deleted*. `scenarios/votematrix` is the shape that answers it: the same proposal
put at one, two and three connected players, per ruleset. What that turned up
was not a defect but three donor rules a single-count test cannot see -- each
system counts a different set (tourney counts connected clients under `dm` and
`dmpro` and ENTERED ones under `tdm` and `duel`, because `vote_countspectators`
is registered with a different default for a teams ruleset; Threewave counts
every client including the proposer, who may not vote; Rocket Arena counts one
arena), `(count * electpercentage) / 100` truncates so two players and three
players need the same single yes, and OSP's fail arm divides the nay tally by
the YES PERCENTAGE rather than by the voter count, so at these counts a `no`
cannot fail a vote and only the clock ends it.

**...and the trap in writing that test is asserting the number you typed.**
Rocket Arena's settings menu takes a round count and stores
`(it->num / 2) * 2 + 1` -- forced odd, because a match is best-of-N and
`wins > rounds / 2` cannot be reached from an even one. A vote driven to 10
therefore leaves the arena holding 11, and the first version of the scenario
reported that as a broken tally. The same row now drives an EVEN value on
purpose so the rule is asserted rather than stepped around. Generalised: when a
menu-driven setting comes back different from what was entered, read the writer
before believing the reader.

**Under Threewave CTF a userinfo skin change keeps the team half.** Connect, join a team, then set `User["skin"]` and `SendUserinfo()`, and watch the playerskins configstring: the *model* half changes and the skin half stays `ctf_r`/`ctf_b`. If the whole string changes, the team skin has been lost.
