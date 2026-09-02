# Verification to-do (before finishing)

> **All eight are discharged.** This is the original harness author's list,
> rescued with the rest of the scratchpad and kept as provenance. Every item
> became a requirement here — SPECS.md **R-VER-15**, closed in spec 1.14 — and
> six of the eight are now checks that run in `make check` rather than notes:
> item 1 is `auditems.py`, item 2 is `dsweep.py`, item 3 is the `check-ptrs`
> target, item 6 is `genptr.py`'s own macro pass, item 7 was confirmed against
> q2pro's `SAVE_VERSION`, item 8 by `clamp()` being gone from the tree. Item 4
> was closed by measurement (288 flash rows for 288 enumerators, sized by the
> enum) and item 5 by `meson.build` and the Makefile's per-target build dirs.
>
> Nothing below is open work. Read it as the record of what the replay could
> have got wrong, not as a to-do list.

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
