# Verification to-do (before finishing)
- [ ] AUDIT itemlist[]: compare every shared item entry in each variant against q2pro's
      baseq2 entry field-by-field. A conflict resolution at "Convert itemlist[] to
      designated initializers" regenerated two entries from the variant side and dropped
      q2pro's railgun precache fix (caught + fixed at the next commit). Others may lurk.
- [ ] Save tables: every variant-added persistent field must appear in g_save.c
      (e.g. xatrix client quadfire_framenum is NOT in the client save table -> powerup
      is lost across save/load).
- [ ] g_ptrs.c must be regenerated per variant with genptr.py.
- [ ] m_flash.c / MZ2_* : mission packs need flash offsets beyond baseq2's table.
- [ ] Build integration: meson targets per game, unique output dirs.
- [ ] genptr.py: commented-out '&*_move_*' references pointing at non-existent symbols break pointer generation (q2pro hit this with flyer_move_attack1). Check rogue's m_flyer/m_carrier/m_widow comments when regenerating g_ptrs.c.
- [ ] SAVE_VERSION drifted in rogue (local transforms skipped q2pro's bumps); confirm final value matches q2pro's src/game/g_save.c.
- [ ] clamp() introduced mid-history no longer exists in current shared.h; verify final compile (rogue p_view.c gun-angle delta).
