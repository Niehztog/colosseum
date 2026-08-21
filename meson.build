# Colosseum, built in place under q2pro (R-BUILD-3).
#
# Mirrors q2pro/src/ctf/meson.build: drop this tree at q2pro/src/colosseum and
# it builds through the same mission-pack mechanism, sharing game_shared_src and
# engine_inc rather than compiling its own copies of shared.c and m_flash.c.
# The standalone Makefile stays primary -- it is the one that covers all five
# targets of R-BUILD-5 and runs the R-TOOL-3 audits.
#
# Deliberately not listed here: src/shared/m_flash.c and src/shared/shared.c.
# In-tree they come from game_shared_src, which is the same two files from
# q2pro/src/shared; the vendored copies under src/shared/ exist for the
# standalone build, which has no q2pro around it.

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
  'src/g_ruleset.c',
  'src/g_save.c',
  'src/g_spawn.c',
  'src/g_svcmds.c',
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
  'src/p_trail.c',
  'src/p_view.c',
  'src/p_weapon.c',
)

shared_library('game' + cpu, colosseum_src + game_shared_src,
  name_prefix:           '',
  dependencies:          game_deps,
  include_directories:   engine_inc,
  gnu_symbol_visibility: 'hidden',
  link_args:             dll_link_args,
  install:               system_wide,
  install_dir:           libdir / 'colosseum',
  override_options:      get_option('game-build-options'),
)
