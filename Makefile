# Colosseum -- one Quake II game library, many rulesets.
#
# Shape inherited from osp-tourney/Makefile and rocketarena2/Makefile:
# the same debug/release split, the same $(BUILDDIR)/game$(CPU).$(SHLIBEXT)
# target, the same one-rule-per-TU tail.  Three deliberate departures:
#
#  1. WARNINGS ARE ON.  The donors carry -w because their trees still hold
#     pre-existing warnings.  That is not allowed here: -Wall -Wextra, clean, and
#     a new warning is not tolerated.  The build starts from a warning-clean
#     q2pro/src/game, so the debt is never taken on in the first place.
#
#  2. FIVE TARGETS, not two.  ELF aarch64 is the native one on the
#     reference machine; ELF x86-64, ELF i386, win32 PE and win64 PE
#     all cross-build from it.  Only the native one can be executed here, so
#     "built" and "run" are tracked separately.
#
#  3. THE LIBRARY NAME FOLLOWS THE ENGINE, NOT uname.  Q2PRO loads
#     game<CPUSTRING><LIBSUFFIX> (src/server/game.c:952) where CPUSTRING is
#     meson's cpu_family after q2pro/meson.build's cpuremap:
#
#         x86     -> "i386" on ELF, "x86" on PE
#         aarch64 -> "arm64"
#         x86_64  -> "x86_64"  (not remapped)
#
#     The donors compute the name from `uname -m` instead, which happens to be
#     right on x86-64 Linux and wrong everywhere else: on this host that pattern
#     emits gameaarch64.so while the engine looks for gamearm64.so.  The CPU_*
#     variables below implement the engine's mapping.
#
# This Makefile IS run, and every
# configuration it defines has been built and the native one smoke-tested
#.  The four cross targets are built, not run.

# ---------------------------------------------------------------- toolchains

CC_NATIVE   ?= gcc
CC_LINUX64  ?= x86_64-linux-gnu-gcc
CC_LINUX32  ?= i686-linux-gnu-gcc
CC_WIN32    ?= i686-w64-mingw32-gcc
CC_WIN64    ?= x86_64-w64-mingw32-gcc
# macOS builds ON macOS and is not a cross target: Apple's linker and SDK are
# not redistributable, so the host IS the toolchain.  `cc` is Apple clang there,
# which is also what CC_IS_CLANG below has to see for the fortify guard to fire
# -- so a macOS build sets both CC_NATIVE and CC_MACOS to the same compiler.
CC_MACOS    ?= cc

PYTHON      ?= python3

# Read by tools/mech.py, not by this file -- exported so that the reformat pass
# and the build agree on which astyle they mean (the version is pinned at 3.1,
# and the host's native one is that version and produces
# byte-identical output).  mech.py falls back to the same default on its own.
ASTYLE      ?= astyle
export ASTYLE

# Native CPUSTRING, via the engine's cpuremap.
NATIVE_MACHINE := $(shell uname -m)
CPU_NATIVE := $(shell echo $(NATIVE_MACHINE) | sed \
	-e 's/^aarch64$$/arm64/' \
	-e 's/^i[3-6]86$$/i386/' \
	-e 's/^armv.*/arm/')

CPU_LINUX64 = x86_64
CPU_LINUX32 = i386
# The Mach-O CPU is the host's, remapped the same way: macOS reports `arm64`
# already and `x86_64` unchanged, so CPU_NATIVE is right for both.
CPU_MACOS   = $(CPU_NATIVE)
CPU_WIN32   = x86
CPU_WIN64   = x86_64

# ---------------------------------------------------------------- game API
#
# Which Quake II game ABI the library exports.  Composes with every
# target above: `make API=old`, `make windows API=old`, `make everything API=old`.
#
#   new   GAME_API_VERSION_NEW (3302), gclient_new_t / pmove_new_t, 64 stats.
#         THE DEFAULT and the shipped configuration -- an unqualified
#         `make` is identical to what it was before this switch existed.
#   old   GAME_API_VERSION_OLD (3), gclient_old_t / pmove_old_t, 32 stats.  The
#         classic id ABI, for a 1997-vintage engine or any Q2PRO built without
#         USE_NEW_GAME_API.
#
# THE BUILD DIRECTORIES DIVERGE, and that is not tidiness.  The switch changes
# player_state_t, pmove_state_t and gclient_t -- struct layouts, not just a
# version number -- so an object from one setting linked against an object from
# the other reads every field at the wrong offset AND LINKS CLEANLY.  That is
# the failure the -MMD -MP note below describes, except that header
# dependencies cannot catch this one: no header changed, so `make API=old` in a
# tree built as `new` would recompile nothing at all.  Separate BUILDDIRs make
# the two physically incapable of meeting; -MMD -MP still covers everything
# within one of them.
#
# The name of the artifact does NOT change.  The engine looks for
# game<CPU><suffix> and nothing else, so both settings must produce that name --
# which is the other reason they cannot share a directory.
API ?= new

ifeq ($(API),new)
API_CFLAGS =
API_SUFFIX =
else
ifeq ($(API),old)
API_CFLAGS = -DUSE_NEW_GAME_API=0
API_SUFFIX = -oldapi
else
$(error API must be 'new' (default, GAME_API_VERSION 3302) or 'old' \
(GAME_API_VERSION 3), not '$(API)')
endif
endif

# ---------------------------------------------------------------- flags

INCLUDES = -I. -Iinc -Isrc

# The workspace standard for this era of code.  -fvisibility=hidden
# keeps everything but GetGameAPI/GetGameAPIEx out of the dynamic symbol table.
# -MMD -MP: HEADER DEPENDENCIES, and this is not a nicety.  Without them a
# change to g_local.h rebuilds nothing, so the next `make` links objects
# compiled against DIFFERENT struct layouts -- and it links cleanly.  The merge
# hit exactly that: adding level_locals_t.forcemap and six gclient_t fields
# produced a library that read level.sight_client at the old offset and
# segfaulted on the first frame, while `make` reported nothing to do.  The
# symptom looked like memory corruption in game code and cost an hour of
# bisecting before the build was suspected.
BASE_CFLAGS = -DHAVE_CONFIG_H $(INCLUDES) -std=gnu99 -MMD -MP \
	-fno-strict-aliasing -fwrapv -fvisibility=hidden $(API_CFLAGS)

# -Wall -Wextra, no casts to silence the
# __attribute__((format)) annotations Q2PRO puts on the game import table, and
# -Werror so that "new warnings are not tolerated" is mechanical rather than
# aspirational.
#
# Three suppressions, all measured, all forced by keeping src/ byte-identical.
#
# The build wants -Wall -Wextra clean; the merge wants src/ byte-identical to
# q2pro/src/game.  On inherited code those two conflict, and byte-identity wins
# first -- the starting point is an *unmodified* baseq2 library, so a warning
# in inherited code must be suppressed, not edited away.  Measured on
# the pristine spine: 622 warnings, all of them from -Wextra, none from -Wall.
#
#   -Wno-unused-parameter   594 of the 622.  Structural, not sloppy: Quake II's
#                           callbacks have signatures fixed by their function-
#                           pointer types (touch takes plane and surf, die takes
#                           inflictor and point), so most implementations ignore
#                           some parameters.  Silencing them individually would
#                           mean ~600 (void) casts in code that has to
#                           stay byte-identical.
#   -Wno-sign-compare       the other 28.  Each was read: all are int-versus-
#                           unsigned in a bounded loop or a guarded check, and
#                           g_save.c:678 is the interesting one -- it tests
#                           `len < 0` BEFORE the unsigned comparison, which is
#                           correct defensive code that -Wsign-compare cannot
#                           see.  Zero real defects.
#
# Note upstream never hit any of them: q2pro builds with -Wall and a curated
# set, NOT -Wextra.  Keeping -Wextra minus these three still buys -Wtype-limits
# and -Wempty-body, which is the reason to keep it at all -- and they apply to
# the code Colosseum writes.  A phase that adds new code should re-check whether
# any of the three can be narrowed to the inherited translation units; the third
# below is the closest to it, being two sites in one file.
#   -Wno-missing-field-initializers
#                           clang only, and exactly two sites: the `{ NULL }`
#                           sentinels in itemlist[] -- the index-0 placeholder
#                           ("leave index 0 alone") and the end-of-list marker.
#                           Both are idiomatic zero-init and both are
#                           load-bearing: the terminator is what the item loop
#                           and auditems.py walk to.  gcc does not warn; clang
#                           does, and both have to be clean, so
#                           the flag is set for both rather than diverging the
#                           two compilers' flags.
WARN_CFLAGS = -Wall -Wextra -Werror \
	-Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers

# q2pro's own curated set, adopted as-is: warning policy is not a
# donor's feature, so the base tree's choice wins.  From q2pro/meson.build's
# test_args, filtered there through cc.get_supported_arguments().
WARN_CFLAGS += -Werror=vla -Wformat-security -Wpointer-arith \
	-Wstrict-prototypes

# -fstack-protector-strong applies to every
# configuration.  _FORTIFY_SOURCE is release-only, and that is not the
# "disabled to make something compile" that would be an evasion: glibc's fortified
# headers require optimisation, so at -O0 the define is inert and only produces
# a "requires compiling with optimization" warning -- which the build would
# then fail the build on.  Release is where it has meaning and there it is on.
HARDEN_CFLAGS         = -fstack-protector-strong
HARDEN_RELEASE_CFLAGS = -D_FORTIFY_SOURCE=2

# ...and _FORTIFY_SOURCE is gcc-only, which is a glibc/clang interaction rather
# than a policy choice.  glibc's bits/stdio2.h guards its fortified dprintf on
# `#ifdef __va_arg_pack`:
#
#     #ifdef __va_arg_pack
#     __fortify_function int dprintf (int __fd, const char *__fmt, ...)
#     #elif !defined __cplusplus
#     # define dprintf(fd, ...) __dprintf_chk (fd, __USE_FORTIFY_LEVEL-1, ...)
#     #endif
#
# gcc provides __va_arg_pack and gets a harmless function *declaration*.  clang
# does not, so it gets a function-like *macro* named `dprintf` -- and
# game_import_t has a member called `dprintf`, so every `gi.dprintf(...)` in the
# tree expands to `gi.__dprintf_chk(...)`:
#
#     src/g_ai.c:338:12: error: no member named '__dprintf_chk' in 'game_import_t'
#
# Isolated: it needs clang AND -O2 AND _FORTIFY_SOURCE together; any
# two are fine.  Neither side can move -- the vendored header that names the
# member may not be edited, and parenthesising the call is not allowed
# sites as `(gi.dprintf)(...)` to suppress macro expansion.
#
# So fortify applies to gcc, which is the compiler the shipped artifact is built
# with, and clang remains a full second opinion on warnings at every level plus
# -O2 without fortify.  This is not an evasion: the hardened
# configuration is not weakened, it is that clang + fortify + this ABI cannot
# coexist at all.
CC_IS_CLANG = $(findstring clang,$(shell $(CC_NATIVE) --version 2>/dev/null | head -1))
NATIVE_HARDEN_RELEASE = $(if $(CC_IS_CLANG),,$(HARDEN_RELEASE_CFLAGS))

RELEASE_CFLAGS = $(BASE_CFLAGS) $(WARN_CFLAGS) $(HARDEN_CFLAGS) \
	$(HARDEN_RELEASE_CFLAGS) -O2
DEBUG_CFLAGS   = $(BASE_CFLAGS) $(WARN_CFLAGS) $(HARDEN_CFLAGS) -g -O0

# The native release differs from the cross releases only in the fortify guard
# above; the cross compilers are all gcc, so they always take it.
NATIVE_RELEASE_CFLAGS = $(BASE_CFLAGS) $(WARN_CFLAGS) $(HARDEN_CFLAGS) \
	$(NATIVE_HARDEN_RELEASE) -O2

# ELF: -ldl for the botlib dlopen path, stricmp is not in glibc.
ELF_CFLAGS      = -Dstricmp=strcasecmp
ELF_LDFLAGS     = -ldl -lm
ELF_SHLIBCFLAGS = -fPIC
ELF_SHLIBLDFLAGS = -shared -Wl,--no-undefined

# PE: stricmp is native to the Windows CRT, nothing dlopen's, and
# __USE_MINGW_ANSI_STDIO makes MinGW's printf take the C99 length modifiers the
# format attributes are checked against.
PE_CFLAGS       = -D__USE_MINGW_ANSI_STDIO=1
# No -lws2_32 any more: src/arena/gslog.c's UDP event forwarding was the tree's
# only socket user and it is not allowed, so nothing links winsock.
#
# -Bstatic -lssp: the stack protector's runtime, linked IN rather than imported.
# `-fstack-protector-strong` makes gcc add an implicit `-lssp`, and on mingw the
# link prefers `libssp.dll.a` over `libssp.a` -- so the shipped DLL declared
# `libssp-0.dll` as an import and would not load on a machine without it.  A
# game DLL that needs a compiler runtime beside it is not shippable, and the
# answer is not to drop the hardening: the static
# archive is right there in the toolchain.  -Bdynamic is restored immediately so
# nothing after this line is affected.
PE_LDFLAGS      = -Wl,-Bstatic -lssp -Wl,-Bdynamic -lm -static-libgcc
PE_SHLIBCFLAGS  =
PE_SHLIBLDFLAGS = -shared

# Mach-O: `stricmp` is absent as it is on ELF, but the other two ELF answers are
# wrong here.  There is no `-ldl`: dlopen lives in libSystem and asking for the
# library fails the link.  And `--no-undefined` is GNU ld's spelling; Apple's
# is `-undefined error`, which is the default anyway and is passed so that a
# toolchain change cannot quietly make it a warning.  `-install_name` is set
# because the engine dlopens the file by path and the default would otherwise
# bake this machine's build directory into a shipped artifact.
MACHO_CFLAGS      = -Dstricmp=strcasecmp
MACHO_LDFLAGS     = -lm
MACHO_SHLIBCFLAGS = -fPIC
MACHO_SHLIBLDFLAGS = -dynamiclib -Wl,-undefined,error \
	-Wl,-install_name,@rpath/game$(CPU).dylib

# ---------------------------------------------------------------- sources
#
# src/ is q2pro/src/game file for file; src/shared/ is vendored from
# q2pro/src/shared.  Donor subdirectories (ctf/ xatrix/ rogue/ arena/ tourney/
# bot/) arrive in Phases 2-6 and are appended here as they land -- listed
# explicitly rather than wildcarded so that a file appearing in the tree
# without a spec row cannot be silently linked.


# src/tourney/ -- OSP Tourney DM, linked as of spec 1.20.  It is
# still its own variable because genptr.py and the spec both want to name the
# donor's translation units as a set.
#
# It was imported-but-unlinked for one increment:
# the 21 units referenced `match_paused`, `pause_time`, `endlvl_frame`, three
# armor tables, four statusbar literals and two static g_cmds.c helpers, every
# one of them defined by a shared file whose merge had not landed.  `make
# check-tourney` compiled them -fsyntax-only in the meantime.  When the merges
# landed the target went away rather than becoming a permanent second build,
# which is what was asked for.
TOURNEY_SRC = \
	tourney/osp_acc.c tourney/osp_clientcmd.c \
	tourney/osp_cmds.c \
	tourney/osp_config.c \
	tourney/osp_detect.c tourney/osp_display.c tourney/osp_hiscore.c \
	tourney/osp_hook.c tourney/osp_main.c tourney/osp_maps.c \
	tourney/osp_menus.c tourney/osp_observe.c tourney/osp_players.c \
	tourney/osp_plist.c tourney/osp_runes.c tourney/osp_stats.c \
	tourney/osp_teams.c tourney/p_camera.c tourney/p_menu.c \
	tourney/sl_write.c tourney/stdlog.c

# src/bot/ -- the Gladiator Bot SDK glue, linked as of spec 1.22.
# Six files are osp-tourney's already-ported bl_*.c and two are the Gladiator
# reconstruction's menu, which osp-tourney cannot supply because it moved its
# bot menu onto id's PMenu.
#
# `tourney/osp_botseam.c` is GONE, not edited: it defined BotServerCommand,
# BotDestroy, AddRandomBot, CheckForNewBotFile, botglobals, botlist and
# old_botcount as no-ops behind !G_BotsAllowed(), and bl_redirgi.c, bl_spawn.c
# and bl_botcfg.c define all seven for real.  The early-out is what kept the
# swap honest.
BOT_SRC = \
	bot/bl_botcfg.c bot/bl_cmd.c bot/bl_debug.c bot/bl_main.c \
	bot/bl_redirgi.c bot/bl_spawn.c \
	bot/p_botmenu.c bot/p_menulib.c bot/p_observer.c

GAME_SRC = \
	g_ai.c g_chase.c g_cmds.c g_combat.c g_func.c g_items.c g_main.c \
	g_misc.c g_monster.c g_phys.c g_ptrs.c g_save.c g_spawn.c g_svcmds.c \
	g_ruleset.c g_stats.c g_fs.c g_log.c \
	g_target.c g_trigger.c g_turret.c g_utils.c g_weapon.c \
	m_actor.c m_berserk.c m_boss2.c m_boss3.c m_boss31.c m_boss32.c \
	m_brain.c m_chick.c m_flipper.c m_float.c m_flyer.c m_gladiator.c \
	m_gunner.c m_hover.c m_infantry.c m_insane.c m_medic.c m_move.c \
	m_mutant.c m_parasite.c m_soldier.c m_supertank.c m_tank.c \
	p_client.c p_hud.c p_lag.c p_trail.c p_view.c p_weapon.c \
	shared/m_flash.c shared/shared.c \
	\
	ctf/g_ctf.c ctf/p_menu.c \
	\
	xatrix/m_boss5.c xatrix/m_fixbot.c xatrix/m_gekk.c xatrix/m_gladb.c \
	\
	rogue/dm_ball.c rogue/dm_tag.c rogue/g_newai.c rogue/g_newdm.c \
	rogue/g_newfnc.c rogue/g_newtarg.c rogue/g_newtrig.c rogue/g_newweap.c \
	rogue/g_sphere.c rogue/m_carrier.c rogue/m_stalker.c rogue/m_turret.c \
	rogue/m_widow.c rogue/m_widow2.c \
	\
	arena/arena.c arena/gslog.c arena/maploop.c arena/menu.c \
	arena/ra2menus.c arena/ra2stats.c \
	\
	$(BOT_SRC) \
	\
	$(TOURNEY_SRC)

# Every .c that genptr.py must scan.  The generated g_ptrs.c is not an input to
# itself.
PTR_SRC = $(filter-out g_ptrs.c,$(filter %.c,$(GAME_SRC)))

OBJS = $(addprefix $(BUILDDIR)/,$(GAME_SRC:.c=.o))

TARGET = $(BUILDDIR)/game$(CPU).$(SHLIBEXT)

# ---------------------------------------------------------------- the botlib
#
# THE BOTLIB IS BUILT HERE, for the same target and into the same directory as
# the library.  It is still a separate repository and a separate build --
# nothing in `src/` includes a line of it, and `docs/botlib-contract.md` is the
# only thing that crosses -- and what changed is only who runs that build.
# Leaving it to the caller meant `make win32` produced half of what an engine
# needs to load this mod with bots, and the other half came from a command the
# build itself never named.
#
# AN ABSENT SUBMODULE IS A SKIP, NOT A FAILURE, and that is load-bearing rather
# than polite.  `ci.yml`'s server-checks job checks this tree out WITHOUT
# submodules and then runs `make native`, and DEVELOPMENT.md states the property
# that rests on: the library builds, and the audits pass, in a tree where the
# submodule was never checked out.  A missing `$(GLADDIR)/Makefile` therefore
# prints one line and succeeds.
#
# THE SUBMODULE HAS ONE BUILD DIRECTORY AND ONE OUTPUT PATH whatever it is
# building -- objects in its own `build/`, the artifact at
# `release/gladiator.<ext>`, and no target named anywhere in either.  So a win32
# build followed by a win64 build in the same tree hands the second link the
# first one's objects, which is measured rather than theoretical:
# `build/botlib_debug.o: file format not recognized`.  The stamp records what
# that tree currently holds and cleans when it is about to change, which is also
# what keeps an unchanged target from paying for a rebuild.
#
# ...and the lock is that same collision under `-j`.  `everything` names five
# targets as prerequisites and make runs prerequisites in parallel, so five
# compilers would drive that one directory at once.  The mutex is `mkdir`,
# atomic on every filesystem this builds on, and a trap releases it rather than
# the recipe reaching its end.  Serialised this way, two targets alternating
# still rebuild the botlib each time they trade places: slower than it looks
# written down, and correct, which is the right way round.
#
# THE PUBLISHED NAME IS THE ONE THE GAME dlopens, which is not always the one
# the submodule emits: `BotDefaultLibrary()` asks for `gladiator.dll` on Windows
# and `gladiator.so` everywhere else INCLUDING macOS, where the submodule
# produces a `.dylib`.  `.github/package.sh` renames it for that same reason.
#
# `GLAD_SERVERFIX=1` IS THE DEFAULT HERE, and it is the far side of the
# submodule's own default, for the reason R-BUILD-11 gives: that tree is a
# reconstruction of the 1999 binaries, so a plain `make` there reproduces them
# bugs and all -- the build its two ASM-matching oracles measure, and the right
# default in that repository.  A library built to be played wants the other one.
GLADDIR        ?= vendor/gladiator-bot-restored
GLAD_SERVERFIX ?= 1

# The lock lives in THIS tree and not in the submodule's, and both halves of
# that matter.  The submodule is a git repository of its own, so a directory
# left in its working tree is reported here as `modified:
# vendor/gladiator-bot-restored (untracked content)` -- a tree that looks
# changed when nothing has been.  And the two directories inside it that are
# already ignored, `build/` and `release/`, are exactly the two its own `clean`
# removes, which this step runs WHILE HOLDING the lock.
BOTLIB_LOCK = .botlib-lock

# The submodule is built with those variables CLEARED, and that is correctness
# rather than hygiene.  `_build` receives `CFLAGS` on make's command line, so it
# travels on in `MAKEFLAGS` and beats an ordinary assignment in every sub-make
# below it -- the submodule's own included.  That tree is a reconstruction of
# 1999 code and does not compile under this one's warning set: the first file it
# reaches stops on ten `-Werror=strict-prototypes` errors for declarations like
# `int AAS_Initialized();`, which are the originals' own and are not this
# project's to correct.  This is also why the packaging workflow could run that
# build as a separate step and never see it.  `CC` is cleared with them and
# handed straight back, because choosing it is the whole point of the step.
BOTLIB_MAKE = env -u CFLAGS -u CC -u MAKEFLAGS $(MAKE)

# The submodule reads the target architecture off `uname -m` unless the compiler
# names one.  A mingw triple decides it there -- its own Makefile keys on
# `x86_64-w64-mingw32` and `i686-w64-mingw32` -- so the two PE rows need nothing
# from here; the two ELF cross rows would otherwise take the HOST's, which is
# aarch64 on the reference machine and x86_64 on a runner, neither of them the
# target.
BOTLIB_ARCH = $(if $(findstring i686-linux-gnu,$(CC)),YQ2_ARCH=i386)\
$(if $(findstring x86_64-linux-gnu,$(CC)),YQ2_ARCH=x86_64)

# ---------------------------------------------------------------- goals

.PHONY: all everything native linux64 linux32 win32 win64 windows macos \
	oldapi bothapis check check-ptrs check-audits clean distclean help

all: native

native:
	$(MAKE) _build BUILDDIR=debug$(API_SUFFIX)          CC=$(CC_NATIVE)  CPU=$(CPU_NATIVE) \
		SHLIBEXT=so  CFLAGS="$(DEBUG_CFLAGS) $(ELF_CFLAGS)"   KIND=ELF
	$(MAKE) _build BUILDDIR=release$(API_SUFFIX)        CC=$(CC_NATIVE)  CPU=$(CPU_NATIVE) \
		SHLIBEXT=so  CFLAGS="$(NATIVE_RELEASE_CFLAGS) $(ELF_CFLAGS)" KIND=ELF

linux64:
	$(MAKE) _build BUILDDIR=debug-linux64$(API_SUFFIX)   CC=$(CC_LINUX64) CPU=$(CPU_LINUX64) \
		SHLIBEXT=so  CFLAGS="$(DEBUG_CFLAGS) $(ELF_CFLAGS)"   KIND=ELF
	$(MAKE) _build BUILDDIR=release-linux64$(API_SUFFIX) CC=$(CC_LINUX64) CPU=$(CPU_LINUX64) \
		SHLIBEXT=so  CFLAGS="$(RELEASE_CFLAGS) $(ELF_CFLAGS)" KIND=ELF

linux32:
	$(MAKE) _build BUILDDIR=debug-linux32$(API_SUFFIX)   CC=$(CC_LINUX32) CPU=$(CPU_LINUX32) \
		SHLIBEXT=so  CFLAGS="$(DEBUG_CFLAGS) $(ELF_CFLAGS)"   KIND=ELF
	$(MAKE) _build BUILDDIR=release-linux32$(API_SUFFIX) CC=$(CC_LINUX32) CPU=$(CPU_LINUX32) \
		SHLIBEXT=so  CFLAGS="$(RELEASE_CFLAGS) $(ELF_CFLAGS)" KIND=ELF

win32:
	$(MAKE) _build BUILDDIR=debug-win32$(API_SUFFIX)     CC=$(CC_WIN32)   CPU=$(CPU_WIN32) \
		SHLIBEXT=dll CFLAGS="$(DEBUG_CFLAGS) $(PE_CFLAGS)"    KIND=PE
	$(MAKE) _build BUILDDIR=release-win32$(API_SUFFIX)   CC=$(CC_WIN32)   CPU=$(CPU_WIN32) \
		SHLIBEXT=dll CFLAGS="$(RELEASE_CFLAGS) $(PE_CFLAGS)"  KIND=PE

win64:
	$(MAKE) _build BUILDDIR=debug-win64$(API_SUFFIX)     CC=$(CC_WIN64)   CPU=$(CPU_WIN64) \
		SHLIBEXT=dll CFLAGS="$(DEBUG_CFLAGS) $(PE_CFLAGS)"    KIND=PE
	$(MAKE) _build BUILDDIR=release-win64$(API_SUFFIX)   CC=$(CC_WIN64)   CPU=$(CPU_WIN64) \
		SHLIBEXT=dll CFLAGS="$(RELEASE_CFLAGS) $(PE_CFLAGS)"  KIND=PE

# Not part of `everything`: it cannot run anywhere but on macOS, and a goal that
# is unbuildable on the machine most of this is developed on does not belong in
# the all-targets rule.  CI builds it on a macOS runner, once per architecture.
macos:
	$(MAKE) _build BUILDDIR=debug-macos$(API_SUFFIX)   CC=$(CC_MACOS) CPU=$(CPU_MACOS) \
		SHLIBEXT=dylib CFLAGS="$(DEBUG_CFLAGS) $(MACHO_CFLAGS)" KIND=MACHO
	$(MAKE) _build BUILDDIR=release-macos$(API_SUFFIX) CC=$(CC_MACOS) CPU=$(CPU_MACOS) \
		SHLIBEXT=dylib CFLAGS="$(NATIVE_RELEASE_CFLAGS) $(MACHO_CFLAGS)" KIND=MACHO

windows: win32 win64

# Every gating target, debug and release.
everything: native linux64 linux32 win32 win64

# Spelled as a target because `API=old` on a command line is easy to
# forget and easy to misread in a build log.  The recursion re-enters this file
# with API set, so it composes the same way the variable does: `make oldapi`
# is the native pair, `make oldapi GOAL=everything` is all five.
GOAL ?= native

oldapi:
	$(MAKE) $(GOAL) API=old

# Both settings of the game ABI, which is what "the old API still builds" has to
# mean to be worth claiming.  Serial rather than a prerequisite list: the two
# recursions must not interleave their `check` runs (see check-ptrs).
bothapis:
	$(MAKE) $(GOAL) API=new
	$(MAKE) $(GOAL) API=old

# ---------------------------------------------------------------- build

# The audits run in the build, not on request, and a finding fails it
# the way a warning does.  They run once per invocation, before compiling.
_build: check
	@mkdir -p $(BUILDDIR)/shared $(BUILDDIR)/xatrix $(BUILDDIR)/rogue \
		$(BUILDDIR)/ctf $(BUILDDIR)/arena $(BUILDDIR)/tourney $(BUILDDIR)/bot
	$(MAKE) $(TARGET) BUILDDIR=$(BUILDDIR) CC=$(CC) CPU=$(CPU) \
		SHLIBEXT=$(SHLIBEXT) CFLAGS="$(CFLAGS)" KIND=$(KIND)
	@if [ ! -f $(GLADDIR)/Makefile ]; then \
		echo "botlib: $(GLADDIR) is not checked out -- no botlib built (DEVELOPMENT.md)"; \
	else \
		set -e; \
		case $(KIND) in \
		PE)    built=gladiator.dll;   name=gladiator.dll ;; \
		MACHO) built=gladiator.dylib; name=gladiator.so  ;; \
		*)     built=gladiator.so;    name=gladiator.so  ;; \
		esac; \
		lock=$(BOTLIB_LOCK); waited=0; \
		while ! mkdir $$lock 2>/dev/null; do \
			waited=$$((waited + 1)); \
			if [ $$waited -gt 600 ]; then \
				echo "botlib: $$lock has been held for ten minutes." >&2; \
				echo "  If no build owns it, remove it and run this again." >&2; \
				exit 1; \
			fi; \
			sleep 1; \
		done; \
		trap 'rmdir $$lock 2>/dev/null || true' EXIT INT TERM; \
		want="$(CC) GLAD_SERVERFIX=$(GLAD_SERVERFIX) $(BOTLIB_ARCH)"; \
		stamp=$(GLADDIR)/release/.built-for; \
		[ -f $$stamp ] && [ "$$(cat $$stamp)" = "$$want" ] \
			|| $(BOTLIB_MAKE) -C $(GLADDIR) clean; \
		$(BOTLIB_MAKE) -C $(GLADDIR) botlib GLAD_SERVERFIX=$(GLAD_SERVERFIX) \
			CC=$(CC) $(BOTLIB_ARCH); \
		printf '%s\n' "$$want" > $$stamp; \
		cp $(GLADDIR)/release/$$built $(BUILDDIR)/$$name; \
		[ $(KIND) != PE ] || $(SHELL) tools/pedeps.sh $(BUILDDIR)/$$name; \
	fi

# A PE artifact is checked for what it IMPORTS the moment it is
# linked.  The failure it catches is invisible from a Linux host -- the DLL
# links, the build is clean, and nothing here can load it to find out that it
# wanted `libssp-0.dll` beside it.  In the build rather than on request, for the
# same reason the audits are.
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $($(KIND)_SHLIBLDFLAGS) -o $@ $(OBJS) $($(KIND)_LDFLAGS)
ifeq ($(KIND),PE)
	@$(SHELL) tools/pedeps.sh $@
endif

$(BUILDDIR)/%.o: src/%.c
	$(CC) $(CFLAGS) $($(KIND)_SHLIBCFLAGS) -DCPUSTRING='"$(CPU)"' -o $@ -c $<

# The generated .d files.  Absent on a clean tree, which is why this is a
# hyphenated include: the first build has nothing to read and needs nothing.
-include $(OBJS:.o=.d)

# ---------------------------------------------------------------- checks

check: check-ptrs check-audits



# Genptr.py runs over the whole tree as a build step and a
# stale g_ptrs.c is a build failure, not a warning.  The committed file must be
# exactly what the generator produces from the current sources.
# The scratch file carries the shell's PID, because `everything` builds five
# configurations and each one depends on `check`: under `make -j` all five run
# this recipe at once, into one file, and read back whatever the last writer
# left.  It reported g_ptrs.c stale against a diff that showed no differences,
# which is the signature of the race rather than of a finding.
check-ptrs:
	@cd src && $(PYTHON) genptr.py $(PTR_SRC) > .g_ptrs.gen.$$$$ \
		&& if cmp -s g_ptrs.c .g_ptrs.gen.$$$$; then \
			rm -f .g_ptrs.gen.$$$$; \
		else \
			echo "*** g_ptrs.c is stale.  Regenerate it:"; \
			echo "***   cd src && python3 genptr.py $(PTR_SRC) > g_ptrs.c"; \
			diff -u g_ptrs.c .g_ptrs.gen.$$$$ | head -40; \
			rm -f .g_ptrs.gen.$$$$; \
			exit 1; \
		fi

# The four contract audits.  A finding fails the build.
# The four tools are reports and exit 0 whatever they find; tools/audit.py is
# the driver that turns a finding into a non-zero exit, which is what the gate
# actually asks for.  It also labels the two comparative audits "not applicable"
# rather than "clean" while no donor is in the tree.
# `auditems` compares the merged itemlist against
# **q2pro's baseq2 entries**, not against a donor, because the regression it
# names is a baseq2 field lost in a merge.  Passed here rather than left to a
# human, because the tree has donors in it and "not applicable"
# would be a vacuous pass.  A missing reference tree is skipped and said so.
BASEQ2_REF ?= ../q2pro/src/game

check-audits:
	@$(PYTHON) tools/audit.py --tree src --donor baseq2=$(BASEQ2_REF)

# ---------------------------------------------------------------- housekeeping

# Both API settings' directories, unconditionally -- `make clean` must not
# depend on which API the caller happened to name, or the stale half survives a
# clean and gets linked later.
BUILDDIRS = debug release debug-linux64 release-linux64 \
	debug-linux32 release-linux32 debug-win32 release-win32 \
	debug-win64 release-win64 debug-macos release-macos

clean:
	-rm -rf $(BUILDDIRS) $(addsuffix -oldapi,$(BUILDDIRS))

# ...and the two derived trees `clean` leaves alone.  `src/.g_ptrs.gen*` is
# GLOBBED: check-ptrs writes the scratch file with the shell's PID appended (see
# its recipe), so the bare name it used before the -j race fix is a file nothing
# has created since.  `build/` is tools/divergence.py's and tools/coverage.py's
# cache of vendor/replay/ -- derived, and .gitignore says so.
distclean: clean
	-rm -f src/.g_ptrs.gen*
	-rm -rf build
	-find . -name __pycache__ -type d -prune -exec rm -rf {} +

help:
	@echo "Colosseum -- targets:"
	@echo "  native      ELF $(CPU_NATIVE) (this host), debug + release   [gating, runnable here]"
	@echo "  linux64     ELF x86_64, cross                               [gating, not runnable here]"
	@echo "  linux32     ELF i386, cross                                 [gating, not runnable here]"
	@echo "  win32       win32 PE, cross                                 [gating, not runnable here]"
	@echo "  win64       win64 PE, cross                                 [gating, not runnable here]"
	@echo "  macos       Mach-O $(CPU_NATIVE) dylib, macOS hosts only       [not cross-buildable]"
	@echo "  windows     win32 + win64"
	@echo "  everything  all five, debug + release"
	@echo "  check       the audits and the g_ptrs.c freshness check only"
	@echo
	@echo "Every target above also builds the botlib out of $(GLADDIR)"
	@echo "for that same target and puts it beside the library it just built,"
	@echo "under the name the game dlopens: gladiator.dll on Windows,"
	@echo "gladiator.so everywhere else.  A tree whose submodule is not checked"
	@echo "out says so and builds the library anyway."
	@echo "  GLAD_SERVERFIX=0  the submodule's own default -- the 1999 bugs kept"
	@echo "  GLADDIR=<path>    build the botlib from another checkout"
	@echo
	@echo "This host builds all five and can execute only 'native'."
	@echo
	@echo "Game ABI -- API=new is the default and the shipped one:"
	@echo "  API=new     GAME_API_VERSION 3302, gclient_new_t/pmove_new_t, 64 stats"
	@echo "  API=old     GAME_API_VERSION 3, gclient_old_t/pmove_old_t, 32 stats"
	@echo "  oldapi      shorthand for 'native API=old'; GOAL=<target> to widen it"
	@echo "  bothapis    the named GOAL (default native) at both settings"
	@echo
	@echo "API=old builds into <dir>-oldapi, because the two settings differ in"
	@echo "STRUCT LAYOUT and mixed objects would link cleanly and run wrong."
	@echo "Under API=old, ctf drops the second powerup timer: its slots are"
	@echo "32/33 and the old player_state_t holds 32."
	@echo "Now: API=$(API), so BUILDDIRs are '<dir>$(API_SUFFIX)'."
