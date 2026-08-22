# Colosseum -- one Quake II game library, many rulesets.
#
# Shape inherited from osp-tourney/Makefile and rocketarena2-public/Makefile:
# the same debug/release split, the same $(BUILDDIR)/game$(CPU).$(SHLIBEXT)
# target, the same one-rule-per-TU tail.  Three deliberate departures:
#
#  1. WARNINGS ARE ON.  The donors carry -w because their trees still hold
#     pre-existing warnings.  R-BUILD-2 forbids that: -Wall -Wextra, clean, and
#     a new warning is not tolerated.  Phase 0 starts from a warning-clean
#     q2pro/src/game, so the debt is never taken on in the first place.
#
#  2. FIVE TARGETS, not two (R-BUILD-5).  ELF aarch64 is the native one on the
#     reference machine (R-BUILD-6); ELF x86-64, ELF i386, win32 PE and win64 PE
#     all cross-build from it.  Only the native one can be executed here, so
#     "built" and "run" are tracked separately -- see doc/regression.md.
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
# R-VER-9 was amended in SPECS.md 1.5: this Makefile IS run, and every
# configuration it defines has been built and the native one smoke-tested
# (R-VER-17).  The four cross targets are built, not run.

# ---------------------------------------------------------------- toolchains

CC_NATIVE   ?= gcc
CC_LINUX64  ?= x86_64-linux-gnu-gcc
CC_LINUX32  ?= i686-linux-gnu-gcc
CC_WIN32    ?= i686-w64-mingw32-gcc
CC_WIN64    ?= x86_64-w64-mingw32-gcc

PYTHON      ?= python3
ASTYLE      ?= astyle

# Native CPUSTRING, via the engine's cpuremap.
NATIVE_MACHINE := $(shell uname -m)
CPU_NATIVE := $(shell echo $(NATIVE_MACHINE) | sed \
	-e 's/^aarch64$$/arm64/' \
	-e 's/^i[3-6]86$$/i386/' \
	-e 's/^armv.*/arm/')

CPU_LINUX64 = x86_64
CPU_LINUX32 = i386
CPU_WIN32   = x86
CPU_WIN64   = x86_64

# ---------------------------------------------------------------- flags

INCLUDES = -I. -Iinc -Isrc

# CLAUDE.md / the workspace standard for this era of code.  -fvisibility=hidden
# keeps everything but GetGameAPI/GetGameAPIEx out of the dynamic symbol table.
# -MMD -MP: HEADER DEPENDENCIES, and this is not a nicety.  Without them a
# change to g_local.h rebuilds nothing, so the next `make` links objects
# compiled against DIFFERENT struct layouts -- and it links cleanly.  Phase 3
# hit exactly that: adding level_locals_t.forcemap and six gclient_t fields
# produced a library that read level.sight_client at the old offset and
# segfaulted on the first frame, while `make` reported nothing to do.  The
# symptom looked like memory corruption in game code and cost an hour of
# bisecting before the build was suspected (doc/reconciliation.md R-48).
BASE_CFLAGS = -DHAVE_CONFIG_H $(INCLUDES) -std=gnu99 -MMD -MP \
	-fno-strict-aliasing -fwrapv -fvisibility=hidden

# R-BUILD-2 / R-SEC-9.  -Wall -Wextra, no casts to silence the
# __attribute__((format)) annotations Q2PRO puts on the game import table, and
# -Werror so that "new warnings are not tolerated" is mechanical rather than
# aspirational.
#
# TWO SUPPRESSIONS, both measured, both forced by R-CORE-5.
#
# R-BUILD-2 wants -Wall -Wextra clean; R-CORE-5 wants src/ byte-identical to
# q2pro/src/game.  On inherited code those two conflict, and R-CORE-5 wins in
# Phase 0 -- its exit criterion is an *unmodified* baseq2 library, so a warning
# in inherited code must be suppressed, not edited away.  Measured 2026-08-21 on
# the pristine spine: 622 warnings, all of them from -Wextra, none from -Wall.
#
#   -Wno-unused-parameter   594 of the 622.  Structural, not sloppy: Quake II's
#                           callbacks have signatures fixed by their function-
#                           pointer types (touch takes plane and surf, die takes
#                           inflictor and point), so most implementations ignore
#                           some parameters.  Silencing them individually would
#                           mean ~600 (void) casts in code R-CORE-5 requires to
#                           stay byte-identical.
#   -Wno-sign-compare       the other 28.  Each was read: all are int-versus-
#                           unsigned in a bounded loop or a guarded check, and
#                           g_save.c:678 is the interesting one -- it tests
#                           `len < 0` BEFORE the unsigned comparison, which is
#                           correct defensive code that -Wsign-compare cannot
#                           see.  Zero real defects.
#
# Note upstream never hit either: q2pro builds with -Wall and a curated set,
# NOT -Wextra.  Keeping -Wextra minus these two still buys
# -Wmissing-field-initializers, -Wtype-limits and -Wempty-body, which is the
# reason to keep it at all -- and they apply to the code Colosseum writes.
# A phase that adds new code should re-check whether either suppression can be
# narrowed to the inherited translation units.
#   -Wno-missing-field-initializers
#                           clang only, and exactly two sites: the `{ NULL }`
#                           sentinels in itemlist[] -- the index-0 placeholder
#                           ("leave index 0 alone") and the end-of-list marker.
#                           Both are idiomatic zero-init and both are
#                           load-bearing: the terminator is what the item loop
#                           and auditems.py walk to.  gcc does not warn; clang
#                           does, and CLAUDE.md requires both to be clean, so
#                           the flag is set for both rather than diverging the
#                           two compilers' flags.
WARN_CFLAGS = -Wall -Wextra -Werror \
	-Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers

# q2pro's own curated set, adopted under §7 rule 1: warning policy is not a
# donor's feature, so the base tree's choice wins.  From q2pro/meson.build's
# test_args, filtered there through cc.get_supported_arguments().
WARN_CFLAGS += -Werror=vla -Wformat-security -Wpointer-arith \
	-Wstrict-prototypes

# R-BUILD-2 / R-SEC-6.  -fstack-protector-strong applies to every
# configuration.  _FORTIFY_SOURCE is release-only, and that is not the
# "disabled to make something compile" that R-SEC-6 forbids: glibc's fortified
# headers require optimisation, so at -O0 the define is inert and only produces
# a "requires compiling with optimization" warning -- which R-BUILD-2 would
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
# Isolated 2026-08-21: it needs clang AND -O2 AND _FORTIFY_SOURCE together; any
# two are fine.  Neither side can move -- R-CORE-9 forbids editing the vendored
# header that names the member, and R-CORE-5 forbids parenthesising the call
# sites as `(gi.dprintf)(...)` to suppress macro expansion.
#
# So fortify applies to gcc, which is the compiler the shipped artifact is built
# with, and clang remains a full second opinion on warnings at every level plus
# -O2 without fortify.  This is not the evasion R-SEC-6 forbids: the hardened
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

# ELF: -ldl for the botlib dlopen path (R-BOT-3), stricmp is not in glibc.
ELF_CFLAGS      = -Dstricmp=strcasecmp
ELF_LDFLAGS     = -ldl -lm
ELF_SHLIBCFLAGS = -fPIC
ELF_SHLIBLDFLAGS = -shared -Wl,--no-undefined

# PE: stricmp is native to the Windows CRT, nothing dlopen's, and
# __USE_MINGW_ANSI_STDIO makes MinGW's printf take the C99 length modifiers the
# format attributes are checked against.
PE_CFLAGS       = -D__USE_MINGW_ANSI_STDIO=1
PE_LDFLAGS      = -lm -static-libgcc
PE_SHLIBCFLAGS  =
PE_SHLIBLDFLAGS = -shared

# ---------------------------------------------------------------- sources
#
# src/ is q2pro/src/game file for file (R-CORE-5); src/shared/ is vendored from
# q2pro/src/shared.  Donor subdirectories (ctf/ xatrix/ rogue/ arena/ tourney/
# bot/) arrive in Phases 2-6 and are appended here as they land -- listed
# explicitly rather than wildcarded so that a file appearing in the tree
# without a spec row cannot be silently linked.

GAME_SRC = \
	g_ai.c g_chase.c g_cmds.c g_combat.c g_func.c g_items.c g_main.c \
	g_misc.c g_monster.c g_phys.c g_ptrs.c g_save.c g_spawn.c g_svcmds.c \
	g_ruleset.c g_stats.c \
	g_target.c g_trigger.c g_turret.c g_utils.c g_weapon.c \
	m_actor.c m_berserk.c m_boss2.c m_boss3.c m_boss31.c m_boss32.c \
	m_brain.c m_chick.c m_flipper.c m_float.c m_flyer.c m_gladiator.c \
	m_gunner.c m_hover.c m_infantry.c m_insane.c m_medic.c m_move.c \
	m_mutant.c m_parasite.c m_soldier.c m_supertank.c m_tank.c \
	p_client.c p_hud.c p_trail.c p_view.c p_weapon.c \
	shared/m_flash.c shared/shared.c \
	\
	ctf/g_ctf.c ctf/p_menu.c \
	\
	xatrix/m_boss5.c xatrix/m_fixbot.c xatrix/m_gekk.c xatrix/m_gladb.c \
	\
	rogue/dm_ball.c rogue/dm_tag.c rogue/g_newai.c rogue/g_newdm.c \
	rogue/g_newfnc.c rogue/g_newtarg.c rogue/g_newtrig.c rogue/g_newweap.c \
	rogue/g_sphere.c rogue/m_carrier.c rogue/m_stalker.c rogue/m_turret.c \
	rogue/m_widow.c rogue/m_widow2.c

# Every .c that genptr.py must scan.  The generated g_ptrs.c is not an input to
# itself.
PTR_SRC = $(filter-out g_ptrs.c,$(filter %.c,$(GAME_SRC)))

OBJS = $(addprefix $(BUILDDIR)/,$(GAME_SRC:.c=.o))

TARGET = $(BUILDDIR)/game$(CPU).$(SHLIBEXT)

# ---------------------------------------------------------------- goals

.PHONY: all everything native linux64 linux32 win32 win64 windows \
	check check-ptrs check-audits clean distclean help

all: native

native:
	$(MAKE) _build BUILDDIR=debug          CC=$(CC_NATIVE)  CPU=$(CPU_NATIVE) \
		SHLIBEXT=so  CFLAGS="$(DEBUG_CFLAGS) $(ELF_CFLAGS)"   KIND=ELF
	$(MAKE) _build BUILDDIR=release        CC=$(CC_NATIVE)  CPU=$(CPU_NATIVE) \
		SHLIBEXT=so  CFLAGS="$(NATIVE_RELEASE_CFLAGS) $(ELF_CFLAGS)" KIND=ELF

linux64:
	$(MAKE) _build BUILDDIR=debug-linux64   CC=$(CC_LINUX64) CPU=$(CPU_LINUX64) \
		SHLIBEXT=so  CFLAGS="$(DEBUG_CFLAGS) $(ELF_CFLAGS)"   KIND=ELF
	$(MAKE) _build BUILDDIR=release-linux64 CC=$(CC_LINUX64) CPU=$(CPU_LINUX64) \
		SHLIBEXT=so  CFLAGS="$(RELEASE_CFLAGS) $(ELF_CFLAGS)" KIND=ELF

linux32:
	$(MAKE) _build BUILDDIR=debug-linux32   CC=$(CC_LINUX32) CPU=$(CPU_LINUX32) \
		SHLIBEXT=so  CFLAGS="$(DEBUG_CFLAGS) $(ELF_CFLAGS)"   KIND=ELF
	$(MAKE) _build BUILDDIR=release-linux32 CC=$(CC_LINUX32) CPU=$(CPU_LINUX32) \
		SHLIBEXT=so  CFLAGS="$(RELEASE_CFLAGS) $(ELF_CFLAGS)" KIND=ELF

win32:
	$(MAKE) _build BUILDDIR=debug-win32     CC=$(CC_WIN32)   CPU=$(CPU_WIN32) \
		SHLIBEXT=dll CFLAGS="$(DEBUG_CFLAGS) $(PE_CFLAGS)"    KIND=PE
	$(MAKE) _build BUILDDIR=release-win32   CC=$(CC_WIN32)   CPU=$(CPU_WIN32) \
		SHLIBEXT=dll CFLAGS="$(RELEASE_CFLAGS) $(PE_CFLAGS)"  KIND=PE

win64:
	$(MAKE) _build BUILDDIR=debug-win64     CC=$(CC_WIN64)   CPU=$(CPU_WIN64) \
		SHLIBEXT=dll CFLAGS="$(DEBUG_CFLAGS) $(PE_CFLAGS)"    KIND=PE
	$(MAKE) _build BUILDDIR=release-win64   CC=$(CC_WIN64)   CPU=$(CPU_WIN64) \
		SHLIBEXT=dll CFLAGS="$(RELEASE_CFLAGS) $(PE_CFLAGS)"  KIND=PE

windows: win32 win64

# R-BUILD-1 / R-VER-8: every gating target of R-BUILD-5, debug and release.
everything: native linux64 linux32 win32 win64

# ---------------------------------------------------------------- build

# R-TOOL-3: the audits run in the build, not on request, and a finding fails it
# the way a warning does.  They run once per invocation, before compiling.
_build: check
	@mkdir -p $(BUILDDIR)/shared $(BUILDDIR)/xatrix $(BUILDDIR)/rogue \
		$(BUILDDIR)/ctf $(BUILDDIR)/arena $(BUILDDIR)/tourney $(BUILDDIR)/bot
	$(MAKE) $(TARGET) BUILDDIR=$(BUILDDIR) CC=$(CC) CPU=$(CPU) \
		SHLIBEXT=$(SHLIBEXT) CFLAGS="$(CFLAGS)" KIND=$(KIND)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $($(KIND)_SHLIBLDFLAGS) -o $@ $(OBJS) $($(KIND)_LDFLAGS)

$(BUILDDIR)/%.o: src/%.c
	$(CC) $(CFLAGS) $($(KIND)_SHLIBCFLAGS) -DCPUSTRING='"$(CPU)"' -o $@ -c $<

# The generated .d files.  Absent on a clean tree, which is why this is a
# hyphenated include: the first build has nothing to read and needs nothing.
-include $(OBJS:.o=.d)

# ---------------------------------------------------------------- checks

check: check-ptrs check-audits

# R-SAVE-2 / R-BUILD-4: genptr.py runs over the whole tree as a build step and a
# stale g_ptrs.c is a build failure, not a warning.  The committed file must be
# exactly what the generator produces from the current sources.
check-ptrs:
	@cd src && $(PYTHON) genptr.py $(PTR_SRC) > .g_ptrs.gen \
		&& if cmp -s g_ptrs.c .g_ptrs.gen; then \
			rm -f .g_ptrs.gen; \
		else \
			echo "*** g_ptrs.c is stale (R-SAVE-2).  Regenerate it:"; \
			echo "***   cd src && python3 genptr.py $(PTR_SRC) > g_ptrs.c"; \
			diff -u g_ptrs.c .g_ptrs.gen | head -40; \
			rm -f .g_ptrs.gen; \
			exit 1; \
		fi

# R-TOOL-3 / R-VER-11: the four contract audits.  A finding fails the build.
# The four tools are reports and exit 0 whatever they find; tools/audit.py is
# the driver that turns a finding into a non-zero exit, which is what R-TOOL-3
# actually asks for.  It also labels the two comparative audits "not applicable"
# rather than "clean" while no donor is in the tree.
# R-33 / R-VER-15 item 1: `auditems` compares the merged itemlist against
# **q2pro's baseq2 entries**, not against a donor, because the regression it
# names is a baseq2 field lost in a merge.  Passed here rather than left to a
# human, because from Phase 2 on the tree HAS donors in it and "not applicable"
# would be a vacuous pass.  A missing reference tree is skipped and said so.
BASEQ2_REF ?= ../q2pro/src/game

check-audits:
	@$(PYTHON) tools/audit.py --tree src --donor baseq2=$(BASEQ2_REF)

# ---------------------------------------------------------------- housekeeping

clean:
	-rm -rf debug release debug-linux64 release-linux64 \
		debug-linux32 release-linux32 debug-win32 release-win32 \
		debug-win64 release-win64

distclean: clean
	-rm -f src/.g_ptrs.gen

help:
	@echo "Colosseum -- targets (R-BUILD-5):"
	@echo "  native      ELF $(CPU_NATIVE) (this host), debug + release   [gating, runnable here]"
	@echo "  linux64     ELF x86_64, cross                               [gating, not runnable here]"
	@echo "  linux32     ELF i386, cross                                 [gating, not runnable here]"
	@echo "  win32       win32 PE, cross                                 [gating, not runnable here]"
	@echo "  win64       win64 PE, cross                                 [gating, not runnable here]"
	@echo "  windows     win32 + win64"
	@echo "  everything  all five, debug + release"
	@echo "  check       the audits and the g_ptrs.c freshness check only"
	@echo
	@echo "This host builds all five and can execute only 'native' (R-BUILD-6)."
