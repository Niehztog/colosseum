# Colosseum, built in place under q2pro (R-BUILD-3).
#
# Mirrors q2pro/src/ctf/meson.build: drop this tree at q2pro/src/colosseum and
# it builds through the same mission-pack mechanism, sharing game_shared_src and
# engine_inc rather than compiling its own copies of shared.c and m_flash.c.
# The standalone Makefile stays primary -- it is the one that covers all five
# targets of R-BUILD-5, both settings of R-ENG-1a, and the R-TOOL-3 audits.
#
# THE FILE LIST BELOW IS THE MAKEFILE'S GAME_SRC, IN ITS ORDER.  It fell 43
# files behind between Phase 2 and Phase 8 -- every donor subdirectory added
# after xatrix and rogue was appended to the Makefile and not to this file, so
# an in-tree build produced a library with no CTF, no arena, no tourney and no
# bots.  Two omissions are deliberate and both are stated where they apply:
# src/rogue/m_move2.c below, and src/shared/*.c, which in-tree come from
# game_shared_src (the same two files from q2pro/src/shared; the vendored copies
# under src/shared/ exist for the standalone build, which has no q2pro around
# it).  Keep the two lists equal -- the grep anchors on a list entry's leading
# two spaces so that this comment cannot match itself, and git's pathspec `*`
# crosses `/`, so `src/*.c` is the whole tree and `src/**/*.c` would be only its
# subdirectories:
#
#     diff <(sed -n "s/^  '\\(src\\/.*\\.c\\)',$/\\1/p" meson.build | sort) \\
#          <(git ls-files 'src/*.c' | grep -vE 'rogue/m_move2|src/shared/' | sort)

colosseum_src = files(
  'src/g_ai.c',
  'src/g_chase.c',
  'src/g_cmds.c',
  'src/g_combat.c',
  'src/g_func.c',
  'src/g_items.c',
  'src/g_main.c',
  'src/g_misc.c',
  'src/g_monster.c',
  'src/g_phys.c',
  'src/g_ptrs.c',
  'src/g_save.c',
  'src/g_spawn.c',
  'src/g_svcmds.c',
  'src/g_ruleset.c',
  'src/g_stats.c',
  'src/g_fs.c',
  'src/g_log.c',
  'src/g_target.c',
  'src/g_trigger.c',
  'src/g_turret.c',
  'src/g_utils.c',
  'src/g_weapon.c',
  'src/m_actor.c',
  'src/m_berserk.c',
  'src/m_boss2.c',
  'src/m_boss3.c',
  'src/m_boss31.c',
  'src/m_boss32.c',
  'src/m_brain.c',
  'src/m_chick.c',
  'src/m_flipper.c',
  'src/m_float.c',
  'src/m_flyer.c',
  'src/m_gladiator.c',
  'src/m_gunner.c',
  'src/m_hover.c',
  'src/m_infantry.c',
  'src/m_insane.c',
  'src/m_medic.c',
  'src/m_move.c',
  'src/m_mutant.c',
  'src/m_parasite.c',
  'src/m_soldier.c',
  'src/m_supertank.c',
  'src/m_tank.c',
  'src/p_client.c',
  'src/p_hud.c',
  'src/p_lag.c',
  'src/p_trail.c',
  'src/p_view.c',
  'src/p_weapon.c',

  # CTF -- Threewave Capture The Flag 1.52 (R-CTF-1..7)
  'src/ctf/g_ctf.c',
  'src/ctf/p_menu.c',

  # XATRIX -- the four Reckoning-only monsters (R-SP-5)
  'src/xatrix/m_boss5.c',
  'src/xatrix/m_fixbot.c',
  'src/xatrix/m_gekk.c',
  'src/xatrix/m_gladb.c',

  # ROGUE -- Ground Zero.  m_move2.c is deliberately NOT here: R-MP-5 ships it
  # as reference and unbuilt, because it redefines all nine of m_move.c's
  # functions with external linkage.
  'src/rogue/dm_ball.c',
  'src/rogue/dm_tag.c',
  'src/rogue/g_newai.c',
  'src/rogue/g_newdm.c',
  'src/rogue/g_newfnc.c',
  'src/rogue/g_newtarg.c',
  'src/rogue/g_newtrig.c',
  'src/rogue/g_newweap.c',
  'src/rogue/g_sphere.c',
  'src/rogue/m_carrier.c',
  'src/rogue/m_stalker.c',
  'src/rogue/m_turret.c',
  'src/rogue/m_widow.c',
  'src/rogue/m_widow2.c',

  # ARENA -- Rocket Arena 2 v2.25 (R-RA-1..6)
  'src/arena/arena.c',
  'src/arena/gslog.c',
  'src/arena/maploop.c',
  'src/arena/menu.c',
  'src/arena/ra2menus.c',
  'src/arena/ra2stats.c',

  # BOT -- the Gladiator Bot SDK glue (R-BOT-1..30)
  'src/bot/bl_botcfg.c',
  'src/bot/bl_cmd.c',
  'src/bot/bl_debug.c',
  'src/bot/bl_main.c',
  'src/bot/bl_redirgi.c',
  'src/bot/bl_spawn.c',
  'src/bot/p_botmenu.c',
  'src/bot/p_menulib.c',
  'src/bot/p_observer.c',

  # TOURNEY -- OSP Tourney DM v2.75 (R-OSP-1..13)
  'src/tourney/osp_acc.c',
  'src/tourney/osp_clientcmd.c',
  'src/tourney/osp_cmds.c',
  'src/tourney/osp_config.c',
  'src/tourney/osp_detect.c',
  'src/tourney/osp_display.c',
  'src/tourney/osp_hiscore.c',
  'src/tourney/osp_hook.c',
  'src/tourney/osp_main.c',
  'src/tourney/osp_maps.c',
  'src/tourney/osp_menus.c',
  'src/tourney/osp_observe.c',
  'src/tourney/osp_players.c',
  'src/tourney/osp_plist.c',
  'src/tourney/osp_runes.c',
  'src/tourney/osp_stats.c',
  'src/tourney/osp_teams.c',
  'src/tourney/p_camera.c',
  'src/tourney/p_menu.c',
  'src/tourney/sl_write.c',
  'src/tourney/stdlog.c',
)

# `src/` on the include path, which q2pro/src/ctf does not need and this tree
# does: the donor subdirectories address their headers from the top of the game
# tree -- src/bot/bl_main.c includes "g_local.h", src/g_main.c includes
# "bot/bl_main.h" -- so a compile that only has the including file's own
# directory resolves the second and not the first.  The standalone Makefile
# spells the same thing -Isrc.
colosseum_inc = include_directories('src')

shared_library('game' + cpu, colosseum_src + game_shared_src,
  name_prefix:           '',
  dependencies:          game_deps,
  include_directories:   [engine_inc, colosseum_inc],
  gnu_symbol_visibility: 'hidden',
  link_args:             dll_link_args,
  install:               system_wide,
  install_dir:           libdir / 'colosseum',
  override_options:      get_option('game-build-options'),
)
