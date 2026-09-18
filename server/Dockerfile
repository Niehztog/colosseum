# TWO TARGETS, ONE FILE, and the file lives in the COLOSSEUM repository, at
# server/.  It is the general half of what was a separate q2server-dockerfile
# repository until 2026-09-18: an engine, an admin wrapper and a game library,
# carrying no deployment's hostnames, ports, paths or passwords.  Those belong
# to whatever private repository drives this one -- docker-compose.example.yml
# beside this file is the shape of that.
#
#   --target server      what a live game server runs: q2pro's engine plus
#                        q2admin, and nothing else.  THE DEFAULT, so a bare
#                        `docker build .` gives you this.
#
#   --target colosseum   the game library those servers load, plus the bot
#                        library, its assets and bspc.  Nothing runs from it in
#                        production: scripts/deploy-colosseum-live.sh copies
#                        the artifacts out of it into the bind-mounted gamedir,
#                        and scripts/make-colosseum-aas.sh runs it as a
#                        throwaway server to compute navigation meshes.
#
# They share the build stage, so the engine is compiled ONCE for both.  Until
# 2026-09-18 the second lived in a separate Dockerfile-colosseum that cloned
# and built q2pro all over again from the same pinned commit.
#
# THE SERVER TARGET NEEDS NOTHING BUT THIS DIRECTORY.  The engine patches sit
# beside this file in q2pro/, so they arrive through the ordinary build context
# and a bare `docker build .` builds them in.  Only the colosseum target reaches
# for the game source, and it takes it as a NAMED BUILD CONTEXT -- `colosseum-src`,
# the repository root -- because BuildKit resolves a named context only for
# stages it actually builds.
#
# While this file lived in the other repository the patches had to arrive
# through that same named context, which made the LIVE SERVER image depend on a
# private clone being present.  Moving the file to sit beside the patches is
# what gives that back, and it is worth knowing it was ever lost: one copy of a
# patch, in the repository that owns it, is the constraint both layouts were
# solving for.
#
# Builds q2pro from source at a pinned commit, so the image is reproducible
# and the (safety-critical) ABI build flag is explicit rather than baked into
# a mystery binary. See README.md for the full setup story.
#
# Why q2pro and not q2repro (Paril's fork): q2repro was evaluated first and
# made to work, but its newer network protocol isn't understood by legacy
# clients (yquake2, the q2pro client). Plain q2pro speaks the classic
# protocol those clients expect.
#
# NOTE the upstream moved off skullernet/q2pro (which now 404s) to the q2pro
# organisation. q2pro's own version banner still prints the old URL.

# ---------------------------------------------------------------------------
# Build stage
# ---------------------------------------------------------------------------
FROM debian:trixie-slim AS build

# Pinned so rebuilds are deterministic. Bump deliberately, then re-test:
# an unpinned clone would look reproducible while silently drifting
# upstream on every rebuild, which is not something a live game server
# should do.
ARG Q2PRO_REPO=https://github.com/q2pro/q2pro
ARG Q2PRO_COMMIT=601a8df8433b0c50dbbe37c0716c3793fff140a7

# q2admin (the Q2Admin/anti-cheat wrapper game module) built the same way:
# pinned commit, cross-compiled to i386, verified before shipping.
#
# packetflinger/q2admin IS THE SOURCE AND THE ONLY ONE.  It is the actively
# maintained rewrite, and it is the one that understands q2pro's
# GMF_EXTRA_USERINFO/GMF_IPV6_ADDRESS_AWARE feature negotiation -- which this
# image needs both halves of, because colosseum declares both flags.  A wrapper
# that does not understand GMF_EXTRA_USERINFO kicks every connecting client
# ("doesn't have a valid IP address" -- not IP-specific, every client hits it),
# and one that does not understand GMF_IPV6_ADDRESS_AWARE heap-overflows a
# fixed 40-byte IP buffer via raw strcpy for any client arriving over IPv6.
#
# ONE PATCH IS CARRIED, in q2admin/ beside this file and checked by
# enginepatch.sh the same way the engine's is.  Upstream tip does not compile
# here: `c78b84c "Added q2a_printf for console output"` left an `edict_t *`
# being passed as a variadic FORMAT STRING in g_whois.c, which GCC 14 rejects
# outright -- so trixie refuses the file and the build stops.  The patch header
# has the detail; the fix is PR #32, still open, and when it merges the patch
# stops applying and this build fails until the pin moves, which is the
# behaviour wanted.
#
# CARRIED RATHER THAN FORKED, and that is a decision this file got wrong once.
# The same one line was a cherry-pick on a fork branch until 2026-09-22, and
# the pin went stale within days: the branch was rebased, the pinned SHA was no
# longer reachable from any branch, and `git clone` + `git checkout --detach`
# -- the two commands below -- failed with "reference is not a tree".  A fork
# pinned for one line is a fork somebody has to remember to unpin, and its SHA
# is only as durable as somebody else's branch.  A patch cannot rot silently:
# it either applies or fails the build, and enginepatch.sh asks that question
# before a build ever runs.
#
# TWO EARLIER FIXES OF OURS ARE UPSTREAM AND ARE RE-CHECKED AT EVERY BUMP,
# because both are load-bearing and neither is ours to defend any more:
# required_ui_keys[] must be {"name", "rate", "skin", NULL} with no "msg" (PR
# #27) -- upstream's "msg" requirement rejected every yquake2 connection, since
# yquake2 registers name/skin/rate/hand/fov/gender/password/spectator and not
# msg -- and every CA_WriteString call in g_cloud.c must pass `userinfo.raw`
# rather than the ~650-byte struct (PR #28), which otherwise segfaults the game
# server on the first real player connect.  Verified at this pin.
#
# PR #33 is why the pin is where it is: merged 2026-09-21 (6ed2a42, 8c36772,
# a0042b9), it fixes the three false-positive kicks that had `zbotdetect` and
# `timescaledetect` turned off in every gamedir -- q2admin reading its own
# incomplete bookkeeping as client misbehaviour while a client is connected but
# not yet `inuse`, which is the window colosseum's map-change-on-join lands in.
# Those are live-player kicks.
#
# WHAT IS NEW AND NOT FINISHED: an IP-context/VPN database (g_ip.c, g_vpn.c)
# built on sqlite3.  Its own commits say "WIP", "Incomplete" and "disabled for
# now in InitGame", and one VPN commit was reverted in the same series.  It is
# off at runtime, but it COMPILES AND LINKS unconditionally -- g_ip.o is in
# OBJS -- which is what drags sqlite3 into this build (see the apt lines and
# the LIBS override below).  That is the cost of tracking this project's main
# branch: it is a development branch, not a release, and a bump takes whatever
# half-finished work is sitting on it.
ARG Q2ADMIN_REPO=https://github.com/packetflinger/q2admin
ARG Q2ADMIN_COMMIT=15b7a7b5f727aa3de303ab9660d4f195aea4f506

# THE important knob. Controls the i386 struct-return calling convention
# q2pro uses for gi.trace() (it applies
# __attribute__((callee_pop_aggregate_return(0))) plus -mstackrealign).
#
# DISABLED FOR BOTH SERVERS as of 2026-09-18, because both now run colosseum,
# which this project builds with a modern gcc and which therefore expects the
# modern caller-pops convention. Enabling it against such a build drifts the
# stack 4 bytes after every gi.trace() and the mod segfaults dereferencing a
# bogus trace.ent, usually inside its own SV_PushEntity - confirmed via gdb on
# a real core dump on 2026-08-19, when an engine built with 'enabled' by
# mistake met a freshly-compiled game library.
#
# It only ever mattered for ancient binaries: arena ran a 2014
# gamei386.real.so until 2026-08-19 that genuinely needed 'enabled', while
# xatrix's 2017 build needed it off - which is the whole reason there is one
# image per gamedir. With both on colosseum the two images now differ only in
# runtime env and could be collapsed into one; left alone because that means
# recreating both live containers for no functional gain.
#
# UNRELATED to USE_NEW_GAME_API, which q2proded always gets unconditionally:
# q2pro's meson.build hardcodes -DUSE_SERVER=1 for the server target and
# shared.h defines USE_NEW_GAME_API as (USE_CLIENT || USE_SERVER) whenever the
# game DLL does not override it itself. Read out of q2pro's
# meson.build/meson_options.txt/shared.h/game.h directly, not assumed.
#
# It is a whole-binary compile-time switch, so one engine binary cannot serve
# two mods of different vintages. Getting it backwards produces a server that
# starts fine and then dies on the first map.
ARG GAME_ABI_HACK=disabled

# q2pro is pure C (no C++), needs meson >= 0.59 - trixie's packaged meson is
# new enough, so no pip needed - and for a dedicated server only zlib.
# gcc-multilib + libc6-dev-i386 provide the 32-bit toolchain.
RUN echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections && \
    dpkg --add-architecture i386 && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        git ca-certificates meson ninja-build pkg-config make \
        gcc gcc-multilib libc6-dev-i386 zlib1g-dev:i386 libssl-dev:i386 \
        libsqlite3-dev:i386 \
        && \
    rm -rf /var/lib/apt/lists/*

# The game DLLs are 32-bit, so the engine must be too.
RUN printf '%s\n' \
    '[binaries]' \
    "c = 'gcc'" \
    "ar = 'ar'" \
    "strip = 'strip'" \
    "pkg-config = 'pkg-config'" \
    '' \
    '[built-in options]' \
    "c_args = ['-m32']" \
    "c_link_args = ['-m32']" \
    '' \
    '[host_machine]' \
    "system = 'linux'" \
    "cpu_family = 'x86'" \
    "cpu = 'i686'" \
    "endian = 'little'" \
    > /i386-linux.txt

# PKG_CONFIG_LIBDIR pins pkg-config to the i386 .pc files. Without it meson
# finds the host's 64-bit zlib and the link fails with
# "libz.so: file in wrong format".
#
# -Danticheat-server=true compiles in src/server/ac.c, q2pro's r1ch.net
# anticheat client. Defaults to false upstream - without this flag, every
# sv_anticheat_* cvar in anticheat.cfg (including sv_anticheat_required) is
# just an unregistered loose cvar with zero effect, silently, since `set`
# never errors on an unknown name. Confirmed via byte-grep of a build
# without this flag: zero occurrences of "ANTICHEAT", "anticheat.r1ch.net",
# or any sv_anticheat_* name anywhere in the resulting q2proded binary.

# THE ENGINE PATCHES, applied in filename order straight after the checkout.
# They sit in q2pro/ beside this file, which is also where they are documented
# and where enginepatch.sh checks them against the commit each one names.
#
# 0001 makes the engine report the game library's bots. A Gladiator-derived
# bot holds no client_t, so a stock engine reports a server with eight bots
# playing as empty -- in the browser and in the `rcon status` WallFly polls
# (see filter-rcon-status.sh). That is true of every Q2 engine including id's
# own; the patch header has the detail.  `set sv_status_show_bots 0` reverts
# every reply to stock output without a restart.
#
# A patch that no longer applies fails the build rather than being skipped,
# which is what pins Q2PRO_COMMIT and q2pro/ together: bump the pin and refresh
# the patches in the same commit.  Nothing here requires a patch -- an empty
# q2pro/ builds a stock engine, and the loop below is a no-op.
COPY q2pro/ /patches/

RUN git clone "$Q2PRO_REPO" /src && \
    cd /src && \
    git checkout --detach "$Q2PRO_COMMIT" && \
    for p in /patches/*.patch; do \
        [ -e "$p" ] || continue; \
        echo "applying $(basename "$p")"; \
        git apply --whitespace=nowarn "$p" || exit 1; \
    done && \
    PKG_CONFIG_LIBDIR=/usr/lib/i386-linux-gnu/pkgconfig PKG_CONFIG_PATH= \
      meson setup build-i386 \
        --cross-file /i386-linux.txt \
        -Dgame-abi-hack="$GAME_ABI_HACK" \
        -Dclient-ui=false \
        -Dclient-gtv=false \
        -Danticheat-server=true && \
    ninja -C build-i386 q2proded && \
    install -Dm755 build-i386/q2proded /out/q2proded

# Fail the build rather than ship a silently-wrong engine.
RUN set -eu; \
    readelf -h /out/q2proded | grep -q 'ELF32' || { echo 'q2proded is not ELF32'; exit 1; }; \
    if [ "$GAME_ABI_HACK" = 'enabled' ]; then \
        grep -q 'USE_GAME_ABI_HACK 1' /src/build-i386/config.h \
            || { echo 'game-abi-hack requested but not enabled in config.h'; exit 1; }; \
    else \
        grep -q 'USE_GAME_ABI_HACK 1' /src/build-i386/config.h \
            && { echo 'game-abi-hack should be off but is on'; exit 1; } || true; \
    fi; \
    grep -qa 'sv_status_show_bots' /out/q2proded \
        || { echo 'bot-status patch did not reach the binary'; exit 1; }; \
    echo "q2proded OK (ELF32, game-abi-hack=$GAME_ABI_HACK, bot-status patched)"

# q2admin's own Makefile defaults to its vendored, prebuilt i386 static libs
# (deps/i386/{curl,zlib,openssl}) - convenient, but its openssl archive links
# in one object (libcrypto-lib-v3_san.o) that isn't fully position-independent,
# producing a shared object with a TEXTREL (confirmed via readelf; it still
# loaded and ran fine in testing, but that's a real hardening regression, not
# something to ship deliberately). zlib's vendored archive has the same
# problem (masked the first time around: once a shared object already has one
# non-PIC relocation, the linker doesn't necessarily warn about every other
# one too). Fix: link openssl and zlib dynamically against Debian's own i386
# packages instead - shared libraries are position-independent by
# construction, so this sidesteps the problem entirely rather than papering
# over it, and matches how q2proded itself is linked (dynamic against system
# libc/zlib, nothing vendored). Only curl stays on the vendored static
# archive: it never showed a TEXTREL, and q2admin's Makefile only exposes
# INCLUDES/LIBS as a pair (?= , so both are overridden together below) - no
# need to touch what already links clean.
# CPU=i386 only controls the output filename (game$(CPU)-q2admin-r$(VER).so
# by default) - it does not select a 32-bit target on its own, so CC is
# overridden to force -m32 explicitly, the same reason q2pro gets a full
# cross-file above rather than relying on CPU alone. TARGET is forced to a
# fixed name instead of the default so the runtime stage's COPY doesn't need
# to know the current commit's revision number.
#
# THE q2admin PATCHES, applied in filename order straight after the checkout,
# exactly as the engine's are.  Same rule too: a patch that no longer applies
# fails the build rather than being skipped, which is what pins Q2ADMIN_COMMIT
# and q2admin/ together -- bump the pin and refresh the patches in the same
# commit.  An empty q2admin/ builds stock and the loop is a no-op.
COPY q2admin/ /patches-q2admin/

RUN git clone "$Q2ADMIN_REPO" /src-q2admin && \
    cd /src-q2admin && \
    git checkout --detach "$Q2ADMIN_COMMIT" && \
    for p in /patches-q2admin/*.patch; do \
        [ -e "$p" ] || continue; \
        echo "applying $(basename "$p")"; \
        git apply --whitespace=nowarn "$p" || exit 1; \
    done && \
    make CPU=i386 CC="gcc -m32" TARGET=gamei386.so \
        INCLUDES="-Ideps/i386/curl/include" \
        LIBS="deps/i386/curl/lib/libcurl.a -lz -lssl -lcrypto -lsqlite3 -lpthread -ldl" && \
    install -Dm755 gamei386.so /out/gamei386.so

# Fail the build rather than ship a silently-wrong or non-hardened wrapper.
RUN set -eu; \
    readelf -h /out/gamei386.so | grep -q 'ELF32' || { echo 'q2admin gamei386.so is not ELF32'; exit 1; }; \
    readelf -d /out/gamei386.so | grep -q TEXTREL \
        && { echo 'q2admin gamei386.so has a TEXTREL - dynamic linking of openssl/zlib must have regressed'; exit 1; } || true; \
    readelf -d /out/gamei386.so | grep -q 'libsqlite3\.so' \
        || { echo 'q2admin gamei386.so is not linked against libsqlite3 - g_ip.c needs it, and there is no vendored i386 sqlite to fall back on'; exit 1; }; \
    echo "q2admin gamei386.so OK (ELF32, no TEXTREL, sqlite3 linked)"

# ---------------------------------------------------------------------------
# The colosseum game library                                 (--target colosseum)
# ---------------------------------------------------------------------------
# FROM build, so the engine and the toolchain above are REUSED rather than
# compiled a second time.  Until 2026-09-18 this lived in its own
# Dockerfile-colosseum which cloned and built q2pro all over again, from the
# same repo at the same pinned commit, to get an engine for the mesh harness.
#
# THE SOURCE ARRIVES AS A NAMED BUILD CONTEXT, not a `git clone`, because the
# colosseum repository is PRIVATE and the pinned clones above cannot
# authenticate:
#
#   cd server
#   docker build --target colosseum --build-context colosseum-src=.. \
#       -t colosseum .
#
# THE CONTEXT IS THIS TARGET'S ALONE.  BuildKit resolves a named context only
# when a stage being built references it, so `--target server` builds with no
# colosseum source present and no extra flag -- verified, not assumed.  That
# held while these two Dockerfiles were first merged, was lost for one commit
# when the engine patches had to travel through this same context, and came
# back when the file moved into the repository the patches live in.
#
# Omitting the flag on this target fails with `failed to resolve source
# metadata for docker.io/library/colosseum-src`, which reads like a missing
# image rather than a missing flag.  It means the --build-context above.
FROM build AS colosseum-build

# python3 for colosseum's own contract audits, which its Makefile runs as part
# of the build; the shim for its CC_LINUX32.  Installed HERE rather than in the
# shared stage above so that stage stays byte-identical for the server target.
RUN apt-get update && \
    apt-get install -y --no-install-recommends python3 && \
    rm -rf /var/lib/apt/lists/*

# A one-line i686-linux-gnu-gcc, so colosseum's Makefile finds the compiler it
# names without being told about it.  `make linux32 CC_LINUX32="gcc -m32"` is
# the obvious alternative and DOES NOT WORK: the linux32 recipe passes the
# value on to a recursive make unquoted, as `CC=gcc -m32`, where make parses
# -m32 as its own option and dies with `invalid option -- '3'`.  A shim keeps
# the compiler a single word, which is what that recipe requires.
RUN printf '%s\n' '#!/bin/sh' 'exec gcc -m32 "$@"' > /usr/local/bin/i686-linux-gnu-gcc && \
    chmod 755 /usr/local/bin/i686-linux-gnu-gcc && \
    i686-linux-gnu-gcc --version | head -1

# ---------------------------------------------------------------------------
# The game library
# ---------------------------------------------------------------------------
# The whole clone, `.git` included, so the build can record which commit it is.
#
# --from=colosseum-src, NOT the build context: this file's context is server/,
# one directory down from the source, and a bare `COPY .` here silently copies
# THAT in and then fails several layers later inside `make linux32` -- which is
# exactly what it did the first time these two Dockerfiles were merged.
COPY --from=colosseum-src . /src-colosseum

# `make linux32` builds debug AND release into separate directories; release is
# the one installed.  The contract audits and the g_ptrs.c freshness check run
# as part of the build (not on request), so a finding fails this layer the way
# a compiler warning does -- warnings are -Werror in that Makefile.
#
# The GAME LIBRARY needs no submodule: the botlib is one, and nothing here
# links against it at compile time.  The stage further down does need it
# checked out in the context, because it builds it -- `git submodule update
# --init` in the clone, not a flag here; a build context carries whatever the
# working tree has and no more.
RUN set -eu; \
    mkdir -p /out-colosseum; \
    cd /src-colosseum; \
    git rev-parse HEAD > /out-colosseum/commit 2>/dev/null \
        || echo 'unknown (context had no .git)' > /out-colosseum/commit; \
    echo "building colosseum $(cat /out-colosseum/commit)"; \
    make linux32; \
    install -Dm755 release-linux32/gamei386.so /out-colosseum/gamei386.so

RUN set -eu; \
    readelf -h /out-colosseum/gamei386.so | grep -q 'ELF32' \
        || { echo 'colosseum gamei386.so is not ELF32'; exit 1; }; \
    readelf --dyn-syms -W /out-colosseum/gamei386.so | grep -q ' GetGameAPI$' \
        || { echo 'colosseum gamei386.so does not export GetGameAPI'; exit 1; }; \
    strings /out-colosseum/gamei386.so | grep -q '^g_ruleset$' \
        || { echo 'colosseum gamei386.so has no g_ruleset cvar - wrong tree built?'; exit 1; }; \
    # One distinctive cvar per donor, rather than the seven ruleset NAMES: four
    # of those are under four characters and `strings` has a four-character
    # minimum, so `dm`, `sp`, `tdm` and `ctf` can never appear in its output
    # however complete the binary is.  These stand in for the same thing --
    # that every merged donor's code is actually linked in: arenacfg is RA2's,
    # bots_minplayers is OSP tourney's and the bot layer's, capturelimit is
    # Threewave's, g_ruleset is the resolution layer's.
    #
    # The two CONTENT LAYER cvars, `xatrix` and `rogue`, are deliberately not
    # in this list: neither appears as a standalone string in the binary (the
    # compiler pools them into longer literals), so asserting on them fails on
    # a perfectly good library.  `sv ruleset` reports the layers at runtime and
    # the boot test is what checks them.
    for sym in g_ruleset arenacfg bots_minplayers capturelimit minimumplayers botfill; do \
        strings /out-colosseum/gamei386.so | grep -qx "$sym" \
            || { echo "cvar '$sym' missing from the binary - a donor did not link in?"; exit 1; }; \
    done; \
    echo '--- shared libraries it needs ---'; \
    readelf -d /out-colosseum/gamei386.so | grep NEEDED; \
    echo "colosseum gamei386.so OK (ELF32, exports GetGameAPI, every donor linked in)"

# ---------------------------------------------------------------------------
# The botlib, its assets, and the map-prep tool
# ---------------------------------------------------------------------------
# The bot AI is NOT part of the game library: it is a separate shared object,
# `gladiator.so`, dlopen'd by name out of the gamedir and entered through
# GetBotAPI.  It comes from colosseum's own submodule, so it is pinned by the
# colosseum checkout rather than by an ARG here, and it must be built for the
# same platform as the game library -- the loader says so when the bitness does
# not match.
#
# YQ2_ARCH=i386 is not optional.  Left alone, that submodule reads the
# architecture off `uname -m`, which on this builder is x86_64, and it would
# emit a 64-bit object the 32-bit game library cannot load.
#
# CC is the shim rather than `gcc -m32` for the same reason the game library
# uses it: `botlib:` re-enters make, and a CC carrying a space does not survive
# that intact.  GLAD_SERVERFIX survives that re-entry unaided -- a variable set
# on make's command line is handed to sub-makes through MAKEFLAGS.
#
# GLAD_SERVERFIX=1 is not optional for a SERVER.  That submodule is a
# reconstruction of the 1999 binaries, so its DEFAULT build reproduces them
# bugs and all -- that is the build its two ASM-matching oracles measure, and
# it is the right default there.  The gate is how the original bugs that are
# fatal or simply wrong on a live server are built out, each replaced by
# Quake III's own later fix of the same code rather than by anything invented
# (Mr. Elusive wrote both bot libraries).  Four of the five reach THIS object:
#
#   aas-linkstack-overflow       AAS_AASLinkEntity's BSP traversal queue is a bare
#                                int[64] with no bound on the push, and linkstack[64]
#                                is exactly the return-address slot.  Stock maps peak
#                                at 40; densely subdivided custom maps do exceed 64.
#   route-start-areatraveltimes  AAS_UpdateAreaRoutingCache seeds the start node with
#                                reachability row 0 of the goal area, which an area
#                                with no reachabilities does not have: the row array
#                                is zero-length, `[0]` reads past the blob, and where
#                                that read is a 0 the link loop dereferences NULL.
#   bot-item-stale-iteminfo      BotUpdateEntityItems re-links a level item through a
#                                stale index, so a bot navigates to one item using
#                                another item's bounding box.
#   script-lu-suffix-guard       PS_ReadNumber's l/u numeric-suffix guard, an operator
#                                precedence slip in the original.
#
# The fifth (ctf-grapple-stale-pointer, in G_FreeEdict) is game.so's, which
# this stage does not build.  The submodule's own release workflow sets the same
# flag for the same reason: a release is for playing, not for measuring.
RUN set -eu; \
    cd /src-colosseum/vendor/gladiator-bot-restored; \
    [ -f Makefile ] || { echo 'the gladiator-bot-restored submodule is not checked out in the build context - run: git -C <clone> submodule update --init'; exit 1; }; \
    make botlib GLAD_SERVERFIX=1 CC=i686-linux-gnu-gcc YQ2_ARCH=i386; \
    install -Dm755 release/gladiator.so /out-colosseum/gladiator.so; \
    install -Dm644 assets/pak7.pak     /out-colosseum/pak7.pak; \
    install -Dm644 assets/bots.cfg     /out-colosseum/bots.cfg; \
    install -Dm755 tools/vendor/bspc/bspc-linux-x86 /out-colosseum/bspc

# The GLAD_SERVERFIX check is that one string.  It is the PRT_ERROR text of the
# overflow guard Q3 added, so it exists in the preprocessed source only on the
# gated arm: no gate, no string.  Checking the flag any other way would only
# prove it was typed on the command line, not that it reached the object -- and
# a silently faithful botlib is exactly the failure this has to catch, because
# it runs perfectly until a map subdivides densely enough to smash the frame.
RUN set -eu; \
    readelf -h /out-colosseum/gladiator.so | grep -q 'ELF32' \
        || { echo 'gladiator.so is not ELF32 - YQ2_ARCH did not take'; exit 1; }; \
    readelf --dyn-syms -W /out-colosseum/gladiator.so | grep -q ' GetBotAPI$' \
        || { echo 'gladiator.so does not export GetBotAPI - the game could dlopen it and find nothing'; exit 1; }; \
    strings -a /out-colosseum/gladiator.so | grep -q 'AAS_LinkEntity: stack overflow' \
        || { echo 'gladiator.so carries no GLAD_SERVERFIX code - the faithful 1999 bugs are built in'; exit 1; }; \
    readelf -h /out-colosseum/bspc | grep -q 'ELF32' || { echo 'bspc is not ELF32'; exit 1; }; \
    echo "botlib OK (ELF32, exports GetBotAPI, GLAD_SERVERFIX in); pak7.pak $(stat -c%s /out-colosseum/pak7.pak) bytes, bots.cfg $(grep -c . /out-colosseum/bots.cfg) lines, bspc staged"

# ---------------------------------------------------------------------------
# The colosseum runtime                                      (--target colosseum)
# ---------------------------------------------------------------------------
# NOT a production server: nothing runs from this image on the live host. It is
# how the game library REACHES production -- scripts/deploy-colosseum-live.sh
# copies the four artifacts below out of it and installs them into the
# bind-mounted gamedir, because q2admin dlopens <gamedir>/gamei386.real.so and
# baking a game library into the server image would silently overwrite the
# operator's deliberate version choice on every container start.
#
# It is also the throwaway server scripts/make-colosseum-aas.sh runs to compute
# navigation meshes, which is why it carries an engine and a CMD at all.
FROM debian:trixie-slim AS colosseum

# Which gamedir under the bind-mounted /opt/quake2 this container runs, which
# config it execs, and where it starts.  Every one of these is set per service
# by the caller at `docker run` time; the defaults here are the arena one.
#
# THE GAMEDIR NAMES ARE THE LIVE ONES -- `arena` and `xatrix`, not the
# `colosseum` colosseum's own install instructions assume.  The name is not
# private to the server: it is the path clients download content under and what
# the existing per-server configs and custom maps sit in, so an eventual
# migration that renamed it would be a content migration rather than a library
# swap.  Isolation comes from the BASEDIR instead: the caller mounts a
# self-contained tree with its own baseq2 here -- scripts/setup-colosseum-serverdata.sh
# builds one -- rather than the live game data.
ENV Q2_GAMEDIR="arena"
ENV Q2_IP="localhost"
ENV Q2_PORT="27920"
ENV Q2_SERVER_CFG="test-server.cfg"
ENV Q2_MAP="ra2map1"

# Same as the live images: the host runs Europe/Berlin and a container left on
# UTC shows the wrong time in-game (xatrix's func_clock is how that was found).
ENV TZ=Europe/Berlin

RUN echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections && \
    dpkg --add-architecture i386 && \
    apt-get update && \
    apt-get install -y --no-install-recommends libc6:i386 zlib1g:i386 tzdata && \
    ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && \
    echo $TZ > /etc/timezone && \
    dpkg-reconfigure -f noninteractive tzdata && \
    apt-get -y autoclean && \
    apt-get -y autoremove && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* /usr/share/locale/* \
           /var/cache/debconf/*-old /usr/share/doc/*

# UID 1000 matches the host's real `nils` login, so the process can write to its
# bind-mounted gamedir (the console log, and the library install below).
RUN useradd -r -u 1000 -U -s /sbin/nologin -M quake2
RUN mkdir -p /opt/quake2 && chown quake2:quake2 /opt/quake2

COPY --from=build /out/q2proded       /opt/q2pro/q2proded
COPY --from=colosseum-build /out-colosseum/gamei386.so    /opt/colosseum/gamei386.so
COPY --from=colosseum-build /out-colosseum/commit /opt/colosseum/commit
# The bot stack.  gladiator.so, pak7.pak and bots.cfg are installed into the
# gamedir on start exactly like the game library is; bspc is a tool, kept here
# so `scripts/make-colosseum-aas.sh` can reach the 1999 map-prep binary without
# anything being installed on the host.
COPY --from=colosseum-build /out-colosseum/gladiator.so   /opt/colosseum/gladiator.so
COPY --from=colosseum-build /out-colosseum/pak7.pak       /opt/colosseum/pak7.pak
COPY --from=colosseum-build /out-colosseum/bots.cfg       /opt/colosseum/bots.cfg
COPY --from=colosseum-build /out-colosseum/bspc           /opt/colosseum/bspc

USER quake2
WORKDIR /opt/quake2

# THE LIBRARY IS INSTALLED INTO THE GAMEDIR AT EVERY START, and that is the one
# real departure from the production Dockerfile.  There, the game library lives
# in the bind-mounted gamedir and has to be re-extracted from a freshly built
# image BY HAND after every rebuild -- `docker compose restart` reuses whatever
# .so is already on disk and does not know the image changed.  Forgetting that
# step crash-looped both live servers once already.  Copying it in on start
# removes the step and the failure mode with it: the running library is always
# the one in the image that started the container.  It is safe here because
# these gamedirs are this test rig's own and nothing else writes them.
#
# No filter-rcon-status.sh: these servers are NOT public, so the WallFly master
# -server bot never polls them and there is no rcon status spam to filter.  The
# per-match timer lines it also drops are worth keeping on a test server.
#
# `map_override_path maps` is what makes `<gamedir>/maps/<name>.bsp.override`
# work, and the live xatrix server's whole map rotation depends on it: those
# files carry an OVERRIDE_NAME field, so a virtual map name redirects to a
# different .bsp and supplies a replacement entity string --
# `x1492annodomini` loads `maps/1492annodomini.bsp` with xatrix's entities in
# it. Without this cvar those names do not resolve to anything at all. It is
# q2pro's own feature (CM_LoadOverride), not a game-library one, so it works
# the same under colosseum as under openffa, and it is inert in a gamedir that
# has no override files -- which is why it is set here for both services
# rather than per service. The production Dockerfile sets it too.
#
# `maps/` is created because that is where the botlib looks for its .aas
# navigation meshes -- through its OWN file search, not the engine's, so a mesh
# inside a pak would not be found.  One per map, made by
# scripts/make-colosseum-aas.sh; without one the botlib loads, refuses the map
# with "no AAS file available" and destroys every bot that wanted it, which
# reads on the console as no bots at all.
# pak7.pak keeps THAT NUMBER whatever else is installed -- it is the one pak in
# colosseum's numbering that is never renumbered, and neither of these gamedirs
# reaches 7 on its own (arena stops at 2, xatrix at 0), so there is no clash.
CMD install -m 755 /opt/colosseum/gamei386.so /opt/quake2/$Q2_GAMEDIR/gamei386.so && \
    install -m 755 /opt/colosseum/gladiator.so /opt/quake2/$Q2_GAMEDIR/gladiator.so && \
    install -m 644 /opt/colosseum/pak7.pak /opt/quake2/$Q2_GAMEDIR/pak7.pak && \
    install -m 644 -D /opt/colosseum/bots.cfg /opt/quake2/$Q2_GAMEDIR/botcfg/bots.cfg && \
    mkdir -p /opt/quake2/$Q2_GAMEDIR/logs /opt/quake2/$Q2_GAMEDIR/maps && \
    echo "colosseum $(cat /opt/colosseum/commit) -> $Q2_GAMEDIR" && \
    script -qefc "stty -onlcr; /opt/q2pro/q2proded \
        +set basedir /opt/quake2 +set libdir /opt/quake2 +set homedir /opt/quake2 \
        +set dedicated 1 +set public 0 \
        +set game $Q2_GAMEDIR \
        +set ip $Q2_IP +set port $Q2_PORT +set net_port $Q2_PORT \
        +set map_override_path maps \
        +exec $Q2_SERVER_CFG +map $Q2_MAP" /dev/null 2>&1 \
    | tee -a /opt/quake2/$Q2_GAMEDIR/logs/console.log

# ---------------------------------------------------------------------------
# The server runtime                             (--target server, the default)
# ---------------------------------------------------------------------------
# This is what the live containers run: the engine, q2admin, and nothing else.
# The game library they load is NOT in here -- see the colosseum stage above.
FROM debian:trixie-slim AS server

ENV Q2_GAMEDIR="arena"
ENV Q2_IP="localhost"
ENV Q2_PORT="27910"
ENV Q2_OVERRIDE_CFG="q2pro-override-arena.cfg"

# Host is Europe/Berlin; without this the container defaults to UTC. Real
# symptom found 2026-08-06: xatrix/openffa's func_clock entity, in its
# default "time of day" mode, calls time(NULL)/localtime() directly and
# displayed exactly 2 hours behind host time (CEST is UTC+2) - the game
# process has no other source of wall-clock time than the container's own.
# tzdata provides the zoneinfo database; the explicit symlink + timezone
# file + dpkg-reconfigure make it authoritative rather than relying on
# tzdata's postinst alone to notice the TZ env var.
ENV TZ=Europe/Berlin

# 32-bit runtime libs for the engine and the game DLLs. libssl3t64 is for
# q2admin's dynamically-linked openssl (see the build stage - avoids a
# TEXTREL that vendoring a static openssl produced). util-linux (script,
# stty) is already in the base image and is needed by the CMD below.
RUN echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections && \
    dpkg --add-architecture i386 && \
    apt-get update && \
    apt-get install -y --no-install-recommends libc6:i386 zlib1g:i386 libssl3t64:i386 libsqlite3-0:i386 tzdata && \
    ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && \
    echo $TZ > /etc/timezone && \
    dpkg-reconfigure -f noninteractive tzdata && \
    apt-get -y autoclean && \
    apt-get -y autoremove && \
    rm -rf \
        /var/lib/apt/lists/* \
        /tmp/* \
        /var/tmp/* \
        /usr/share/locale/* \
        /var/cache/debconf/*-old \
        /usr/share/doc/*

# UID 1000 matches the host's real "nils" login so the server can write to
# its bind-mounted game data.
RUN useradd -r -u 1000 -U -s /sbin/nologin -M quake2

RUN mkdir -p /opt/quake2 && chown quake2:quake2 /opt/quake2

# q2admin lands at /opt/q2admin rather than in a gamedir: unlike q2proded it
# has to sit *inside* the bind-mounted gamedir at runtime (the engine loads
# "<gamedir>/gamei386.so"), so the image cannot deliver it there directly, and
# copying over the operator's file from an entrypoint on every start would
# silently overwrite a deliberate version choice. Install it into a gamedir
# once:
#   docker create --name q2admin-extract <image> && \
#   docker cp q2admin-extract:/opt/q2admin/gamei386.so <gamedata>/<gamedir>/ && \
#   docker rm q2admin-extract
#
# The game library q2admin then loads as "gamei386.real.so" is NOT built here.
# Both servers run colosseum, which the --target colosseum stage above builds
# (from a named build context, because that repo is private and the pinned
# clones here cannot authenticate) and scripts/deploy-colosseum-live.sh
# installs. openffa-xatrix
# and rocketarena2 were built here until 2026-09-18 and are gone with them.
COPY --from=build /out/q2proded /opt/q2pro/q2proded
COPY --from=build /out/gamei386.so /opt/q2admin/gamei386.so
COPY filter-rcon-status.sh /opt/filter-rcon-status.sh

USER quake2

WORKDIR /opt/quake2

# The engine lives in the image; only game DATA is bind-mounted at
# /opt/quake2. basedir/libdir/homedir are all set explicitly to /opt/quake2
# because q2pro otherwise defaults homedir to ~/.q2pro and checks it before
# basedir/libdir for several lookups. For the game library, that's harmless -
# "Can't access /home/quake2/.q2pro/<gamedir>/gamei386.so", immediately
# followed by a fallback that finds it under libdir. But file reads the game
# DLL itself makes (e.g. xatrix/openffa's own map-rotation code loading
# "mapcfg/maplist.txt" for its random map cycle) only try the homedir path,
# with no fallback - they fail outright ("Couldn't load
# '/home/quake2/.q2pro/xatrix/mapcfg/maplist.txt'"), silently leaving the
# mod's map list empty. Symptom: the server never rotates and instead
# restarts the same map after every timelimit. Pointing homedir at the same
# real directory as basedir/libdir closes the gap for every such lookup, not
# just this one.
#
# net_port, not the classic "port" cvar, controls q2pro's actual listen socket
# (it binds PORT_SERVER=27910 regardless of what "port" says), so both are
# set. "port" is still needed because the game DLL reads it.
#
# map_override_path enables q2pro's .bsp.override / .ent entity-string
# overrides, which several xatrix maps rely on. q2pro requires this cvar to be
# set; without it such a map fails to load entirely.
#
# script -qefc allocates a PTY so the game's libc line-buffers stdout and
# "docker logs" is live rather than arriving in delayed batches; stty -onlcr
# stops the PTY translating \n to \r\n (which would otherwise break the
# rcon filter's line matching).
#
# The final `tee -a` persists that same filtered stream into the
# bind-mounted gamedir (/opt/quake2/$Q2_GAMEDIR -> the host's <gamedir>),
# independent of Docker's own container-scoped json-file driver. That
# driver's history is lost whenever the CONTAINER is recreated (not just
# restarted) - which happens on every image rebuild/`docker compose up -d`,
# and both arena and xatrix have been recreated several times already, most
# recently 2026-08-19; nothing from before that survived in `docker logs`.
# tee reads after filter-rcon-status.sh, not before, so the persisted file
# carries the same WallFly/timer/spawn-count noise already stripped rather
# than a second, unfiltered copy of it - `docker logs` and this file show
# identical content, one just outlives the container. mkdir -p first since
# tee won't create the parent directory itself, and a fresh bind-mount
# wouldn't have logs/ yet. Rotation is left to the operator, outside the
# container - `tee` has no size cap of its own and this CMD's shell has no
# cheap way to enforce one mid-stream. logrotate with `copytruncate` is the
# working answer, because tee holds the file open for the container's life.
CMD mkdir -p /opt/quake2/$Q2_GAMEDIR/logs && script -qefc "stty -onlcr; /opt/q2pro/q2proded +set basedir /opt/quake2 +set libdir /opt/quake2 +set homedir /opt/quake2 +set dedicated 1 +set game $Q2_GAMEDIR +set ip $Q2_IP +set port $Q2_PORT +set net_port $Q2_PORT +set map_override_path maps +exec server1.cfg +exec $Q2_OVERRIDE_CFG" /dev/null 2>&1 | /bin/sh /opt/filter-rcon-status.sh | tee -a /opt/quake2/$Q2_GAMEDIR/logs/console.log

# ...and "the process is alive" is not "the server is serving".
#
# `gi.error()` ends the GAME, not the engine. q2proded prints ShutdownGame,
# drops `sv_running` and keeps running: the container stays up, RestartCount
# stays 0, `restart: always` never fires, `docker ps` reads `Up N hours` -- and
# the server answers no out-of-band status at all, so it is gone from every
# browser. Measured 2026-09-18: 25 minutes of silence that looked healthy from
# outside, after an arena round killed the game library (SPECS R-RA-9).
#
# So the probe is the thing a browser asks: a real UDP status query on the
# port this container serves. It is the only check that distinguishes the two,
# because every cheaper one -- the PID, the port being bound, the log file
# growing -- is still true of a dead game.
#
# bash's /dev/udp rather than a netcat: it connects the socket, so the reply
# comes back on the same descriptor, and it needs no package the runtime image
# does not already have. A live server answers `\xff\xff\xff\xffprint\n` plus
# the serverinfo string, which is far more than the 64 bytes read here; a dead
# one answers nothing and `head` blocks until --timeout ends it.
#
# WHAT THIS DOES NOT DO. Docker's restart policy does not act on health --
# `restart: always` restarts a container that EXITS, and this one does not.
# The healthcheck makes the failure visible (`docker ps` says `unhealthy`, and
# `docker inspect` carries the failing output); acting on it is the operator's,
# with a watcher or an orchestrator that understands health. The compose
# example says so beside `restart: always`.
HEALTHCHECK --interval=30s --timeout=5s --start-period=60s --retries=3 \
    CMD bash -c 'exec 3<>/dev/udp/127.0.0.1/${Q2_PORT} && printf "\377\377\377\377status\n" >&3 && head -c 64 <&3 | grep -q print'
