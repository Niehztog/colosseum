# OSP Tourney DM -- native Makefile.
#
# Same variables, same debug/release split, same
# $(BUILDDIR)/game$(ARCH).$(SHLIBEXT) target as id's Quake II 3.20 game SDK
# Makefile, which this mod was built from. `make` alone builds debug only;
# `make all` builds both. GAME_OBJS is listed in the link order of the
# original v2.75 release.

BUILD_DEBUG_DIR=debug
BUILD_RELEASE_DIR=release

ARCH:=$(shell uname -m | sed -e 's/i.86/i386/' -e 's/^armv.*/arm/')

CC=gcc
BASE_CFLAGS=-Dstricmp=strcasecmp

# Matches the original release's compiler flags.
RELEASE_CFLAGS=$(BASE_CFLAGS) $(MODERN_CFLAGS) -O3
DEBUG_CFLAGS=$(BASE_CFLAGS) $(MODERN_CFLAGS) -g

# Not in the 1999 Makefile; required to build 1999 C with a current gcc.
#  -std=gnu99                    the tree predates C99-by-default diagnostics
#  -fno-strict-aliasing -fwrapv  the workspace standard for this era of code
#  -fno-stack-protector,
#  -D_FORTIFY_SOURCE=0           this tree DELIBERATELY reproduces the mod's own
#                                buffer overruns (OSP_defaultteam_cmd strncpy's
#                                with a 128 limit into 80 bytes;
#                                OSP_startDemos writes name[2][16] at index 2).
#                                They are faithful, not defects, and modern
#                                glibc aborts on the first one during map spawn.
MODERN_CFLAGS=-std=gnu99 -fno-strict-aliasing -fwrapv -w \
	-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fno-stack-protector

LDFLAGS=-ldl -lm

SHLIBEXT=so

SHLIBCFLAGS=-fPIC
SHLIBLDFLAGS=-shared

DO_CC=$(CC) $(CFLAGS) -o $@ -c $<
DO_SHLIB_CC=$(CC) $(CFLAGS) $(SHLIBCFLAGS) -o $@ -c $<

TARGETS=$(BUILDDIR)/game$(ARCH).$(SHLIBEXT)

build_debug:
	@-mkdir $(BUILD_DEBUG_DIR)
	$(MAKE) targets BUILDDIR=$(BUILD_DEBUG_DIR) CFLAGS="$(DEBUG_CFLAGS)"

build_release:
	@-mkdir $(BUILD_RELEASE_DIR)
	$(MAKE) targets BUILDDIR=$(BUILD_RELEASE_DIR) CFLAGS="$(RELEASE_CFLAGS)"

all: build_debug build_release

targets: $(TARGETS)

GAME_OBJS = \
	$(BUILDDIR)/g_ai.o \
	$(BUILDDIR)/g_cmds.o \
	$(BUILDDIR)/g_combat.o \
	$(BUILDDIR)/g_func.o \
	$(BUILDDIR)/g_items.o \
	$(BUILDDIR)/g_main.o \
	$(BUILDDIR)/g_monsters.o \
	$(BUILDDIR)/g_misc.o \
	$(BUILDDIR)/g_monster.o \
	$(BUILDDIR)/g_phys.o \
	$(BUILDDIR)/g_save.o \
	$(BUILDDIR)/g_spawn.o \
	$(BUILDDIR)/g_svcmds.o \
	$(BUILDDIR)/g_target.o \
	$(BUILDDIR)/g_trigger.o \
	$(BUILDDIR)/g_turret.o \
	$(BUILDDIR)/g_utils.o \
	$(BUILDDIR)/g_weapon.o \
	$(BUILDDIR)/m_move.o \
	$(BUILDDIR)/p_camera.o \
	$(BUILDDIR)/p_client.o \
	$(BUILDDIR)/p_hud.o \
	$(BUILDDIR)/p_trail.o \
	$(BUILDDIR)/p_view.o \
	$(BUILDDIR)/p_weapon.o \
	$(BUILDDIR)/q_shared.o \
	$(BUILDDIR)/osp_config.o \
	$(BUILDDIR)/osp_main.o \
	$(BUILDDIR)/osp_display.o \
	$(BUILDDIR)/osp_observe.o \
	$(BUILDDIR)/g_chase.o \
	$(BUILDDIR)/osp_cmds.o \
	$(BUILDDIR)/osp_hook.o \
	$(BUILDDIR)/osp_hiscore.o \
	$(BUILDDIR)/osp_menus.o \
	$(BUILDDIR)/osp_runes.o \
	$(BUILDDIR)/osp_teams.o \
	$(BUILDDIR)/osp_players.o \
	$(BUILDDIR)/osp_plist.o \
	$(BUILDDIR)/osp_maps.o \
	$(BUILDDIR)/nglog.o \
	$(BUILDDIR)/ngmark.o \
	$(BUILDDIR)/md5c.o \
	$(BUILDDIR)/osp_detect.o \
	$(BUILDDIR)/q2log.o \
	$(BUILDDIR)/stdlog.o \
	$(BUILDDIR)/sl_write.o \
	$(BUILDDIR)/p_menu.o \
	$(BUILDDIR)/bl_botcfg.o \
	$(BUILDDIR)/bl_cmd.o \
	$(BUILDDIR)/bl_debug.o \
	$(BUILDDIR)/bl_main.o \
	$(BUILDDIR)/bl_redirgi.o \
	$(BUILDDIR)/bl_spawn.o

$(BUILDDIR)/game$(ARCH).$(SHLIBEXT) : $(GAME_OBJS)
	$(CC) $(CFLAGS) $(SHLIBLDFLAGS) -o $@ $(GAME_OBJS) $(LDFLAGS)

$(BUILDDIR)/g_ai.o :         g_ai.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_cmds.o :       g_cmds.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_combat.o :     g_combat.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_func.o :       g_func.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_items.o :      g_items.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_main.o :       g_main.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_monsters.o :   g_monsters.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_misc.o :       g_misc.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_monster.o :    g_monster.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_phys.o :       g_phys.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_save.o :       g_save.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_spawn.o :      g_spawn.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_svcmds.o :     g_svcmds.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_target.o :     g_target.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_trigger.o :    g_trigger.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_turret.o :     g_turret.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_utils.o :      g_utils.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_weapon.o :     g_weapon.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/m_move.o :       m_move.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/p_camera.o :     p_camera.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/p_client.o :     p_client.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/p_hud.o :        p_hud.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/p_trail.o :      p_trail.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/p_view.o :       p_view.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/p_weapon.o :     p_weapon.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/q_shared.o :     q_shared.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/osp_config.o :   osp_config.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/osp_display.o :  osp_display.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/osp_main.o :     osp_main.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/osp_observe.o :  osp_observe.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/g_chase.o :      g_chase.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/osp_cmds.o :     osp_cmds.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/osp_hook.o :     osp_hook.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/osp_hiscore.o :  osp_hiscore.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/osp_menus.o :    osp_menus.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/osp_runes.o :    osp_runes.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/osp_teams.o :    osp_teams.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/osp_players.o :  osp_players.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/osp_plist.o :    osp_plist.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/osp_maps.o :     osp_maps.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/ngmark.o :       ngmark.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/nglog.o :        nglog.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/md5c.o :         md5c.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/osp_detect.o :   osp_detect.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/q2log.o :        q2log.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/sl_write.o :     sl_write.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/stdlog.o :       stdlog.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/p_menu.o :       p_menu.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/bl_botcfg.o :    bl_botcfg.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/bl_cmd.o :       bl_cmd.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/bl_debug.o :     bl_debug.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/bl_main.o :      bl_main.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/bl_redirgi.o :   bl_redirgi.c
	$(DO_SHLIB_CC)

$(BUILDDIR)/bl_spawn.o :     bl_spawn.c
	$(DO_SHLIB_CC)

#####

clean: clean-debug clean-release

clean-debug:
	$(MAKE) clean2 BUILDDIR=$(BUILD_DEBUG_DIR) CFLAGS="$(DEBUG_CFLAGS)"

clean-release:
	$(MAKE) clean2 BUILDDIR=$(BUILD_RELEASE_DIR) CFLAGS="$(RELEASE_CFLAGS)"

clean2:
	-rm -f $(GAME_OBJS) $(TARGETS)
