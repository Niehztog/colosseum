/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "g_local.h"
#include "g_ptrs.h"

#if USE_ZLIB
#include <zlib.h>
#else
#define gzopen(name, mode)          fopen(name, mode)
#define gzclose(file)               fclose(file)
#define gzwrite(file, buf, len)     fwrite(buf, 1, len, file)
#define gzread(file, buf, len)      fread(buf, 1, len, file)
#define gzbuffer(file, size)        (void)0
#define gzFile                      FILE *
#endif

typedef struct {
    fieldtype_t type;
#if USE_DEBUG
    char *name;
#endif
    unsigned ofs;
    unsigned size;
} save_field_t;

#if USE_DEBUG
#define _FA(type, name, size) { type, #name, _OFS(name), size }
#else
#define _FA(type, name, size) { type, _OFS(name), size }
#endif
#define _F(type, name) _FA(type, name, 1)
#define SZ(name, size) _FA(F_ZSTRING, name, size)
#define BA(name, size) _FA(F_BYTE, name, size)
#define B(name) BA(name, 1)
#define SA(name, size) _FA(F_SHORT, name, size)
#define S(name) SA(name, 1)
#define IA(name, size) _FA(F_INT, name, size)
#define I(name) IA(name, 1)
#define OA(name, size) _FA(F_BOOL, name, size)
#define O(name) OA(name, 1)
#define FA(name, size) _FA(F_FLOAT, name, size)
#define F(name) FA(name, 1)
#define L(name) _F(F_LSTRING, name)
#define V(name) _F(F_VECTOR, name)
#define T(name) _F(F_ITEM, name)
#define E(name) _F(F_EDICT, name)
#define P(name, type) _FA(F_POINTER, name, type)

static const save_field_t entityfields[] = {
#define _OFS FOFS
    // R-SAVE-3.  content_flavour is latched at monster_start and decides which
    // frame tables a monster uses (R-CORE-11/11a), so it MUST persist: a monster
    // reloaded without it would silently change flavour mid-game, which is the
    // class of defect R-SAVE-3a exists to catch.  Type is int, macro is I().
    I(content_flavour),
    // R-SAVE-3: descriptors for every field the mission-pack merge added to a
    // persistent struct.  Found by tools/dsweep.py, which brace-matches the
    // struct bodies and self-tests -- auditsave.py misattributed spawn_temp_t
    // members to monsterinfo_t and would have sent this list wrong.
    //
    // gravityVector and the blindfire set below are R-SAVE-3a's named cases:
    // the mission-pack port lost exactly these to missing rows.
    I(orders),                          // XATRIX
    I(plat2flags),                      // ROGUE
    V(offset),                          // ROGUE
    V(gravityVector),                   // ROGUE -- R-SAVE-3a's named case
    I(hint_chain_id),                   // ROGUE
    F(lastMoveTime),                    // ROGUE
    E(bad_area),                        // ROGUE
    E(hint_chain),                      // ROGUE
    E(monster_hint_chain),              // ROGUE
    E(target_hint_chain),               // ROGUE
    // Entity references, all Ground Zero's.  dsweep classes these as
    // "pointer-only" on the grounds that id's savegame dumped pointers as raw
    // bit patterns so they never survived a reload anyway -- true, and not a
    // reason to leave them out: Q2PRO's F_EDICT machinery exists to relocate
    // them properly, so a descriptor is a strict improvement over the original.
    // Without them a reloaded game orphans the player's sphere and severs every
    // spawned monster from the carrier or widow that made it.
    E(monsterinfo.goal_hint),           // ROGUE
    E(monsterinfo.healer),              // ROGUE
    E(monsterinfo.last_player_enemy),   // ROGUE
    E(monsterinfo.commander),           // ROGUE
    I(monsterinfo.last_hint_framenum),  // ROGUE
    I(monsterinfo.medicTries),          // ROGUE
    F(monsterinfo.base_height),         // ROGUE
    I(monsterinfo.next_duck_framenum),  // ROGUE
    I(monsterinfo.duck_wait_framenum),  // ROGUE
    O(monsterinfo.blindfire),           // ROGUE -- R-SAVE-3a's named set
    F(monsterinfo.blind_fire_delay),    // ROGUE -- ditto
    V(monsterinfo.blind_fire_target),   // ROGUE -- ditto
    I(monsterinfo.monster_slots),       // ROGUE
    I(monsterinfo.monster_used),        // ROGUE
    I(monsterinfo.double_framenum),     // ROGUE
    V(s.origin),
    V(s.angles),
    V(s.old_origin),
    I(s.modelindex),
    I(s.modelindex2),
    I(s.modelindex3),
    I(s.modelindex4),
    I(s.frame),
    I(s.skinnum),
    I(s.effects),
    I(s.renderfx),
    I(s.solid),
    I(s.sound),
    I(s.event),

    // [...]

    I(svflags),
    V(mins),
    V(maxs),
    V(absmin),
    V(absmax),
    V(size),
    I(solid),
    I(clipmask),
    E(owner),

    I(movetype),
    I(flags),

    L(model),
    F(freetime),

    L(message),
    L(classname),
    I(spawnflags),

    I(timestamp),

    // ROGUE
    E(bad_area),
    // while the hint_path stuff could be reassembled on the fly, no reason to be different
    E(hint_chain),
    E(monster_hint_chain),
    E(target_hint_chain),
    //
    E(monsterinfo.goal_hint),
    E(monsterinfo.badMedic1),
    E(monsterinfo.badMedic2),
    E(monsterinfo.last_player_enemy),
    E(monsterinfo.commander),
    P(monsterinfo.blocked, P_monsterinfo_blocked),
    P(monsterinfo.duck, P_monsterinfo_duck),
    P(monsterinfo.unduck, P_monsterinfo_unduck),
    P(monsterinfo.sidestep, P_monsterinfo_sidestep),
    // ROGUE

    L(target),
    L(targetname),
    L(killtarget),
    L(team),
    L(pathtarget),
    L(deathtarget),
    L(combattarget),
    E(target_ent),

    F(speed),
    F(accel),
    F(decel),
    V(movedir),
    V(pos1),
    V(pos2),

    V(velocity),
    V(avelocity),
    I(mass),
    I(air_finished_framenum),
    F(gravity),

    // ROGUE
    // ROGUE

    E(goalentity),
    E(movetarget),
    F(yaw_speed),
    F(ideal_yaw),

    I(nextthink),
    P(prethink, P_prethink),
    P(think, P_think),
    P(blocked, P_blocked),
    P(touch, P_touch),
    P(use, P_use),
    P(pain, P_pain),
    P(die, P_die),
    // ROGUE
    // ROGUE

    I(touch_debounce_framenum),
    I(pain_debounce_framenum),
    I(damage_debounce_framenum),
    I(fly_sound_debounce_framenum),
    I(last_move_framenum),

    I(health),
    I(max_health),
    I(gib_health),
    I(deadflag),
    F(show_hostile),

    I(powerarmor_framenum),

    L(map),

    I(viewheight),
    I(takedamage),
    I(dmg),
    I(radius_dmg),
    F(dmg_radius),
    I(sounds),
    I(count),

    E(chain),
    E(enemy),
    E(oldenemy),
    E(activator),
    E(groundentity),
    I(groundentity_linkcount),
    E(teamchain),
    E(teammaster),

    E(mynoise),
    E(mynoise2),

    I(noise_index),
    I(noise_index2),
    F(volume),
    F(attenuation),

    F(wait),
    F(delay),
    F(random),

    I(last_sound_framenum),

    I(watertype),
    I(waterlevel),

    V(move_origin),
    V(move_angles),

    I(light_level),

    I(style),

    T(item),

    V(moveinfo.start_origin),
    V(moveinfo.start_angles),
    V(moveinfo.end_origin),
    V(moveinfo.end_angles),

    I(moveinfo.sound_start),
    I(moveinfo.sound_middle),
    I(moveinfo.sound_end),

    F(moveinfo.accel),
    F(moveinfo.speed),
    F(moveinfo.decel),
    F(moveinfo.distance),

    F(moveinfo.wait),

    I(moveinfo.state),
    V(moveinfo.dir),
    F(moveinfo.current_speed),
    F(moveinfo.move_speed),
    F(moveinfo.next_speed),
    F(moveinfo.remaining_distance),
    F(moveinfo.decel_distance),
    P(moveinfo.endfunc, P_moveinfo_endfunc),

    P(monsterinfo.currentmove, P_monsterinfo_currentmove),
    I(monsterinfo.aiflags),
    I(monsterinfo.double_framenum),   // ROGUE
    I(monsterinfo.nextframe),
    F(monsterinfo.scale),

    P(monsterinfo.stand, P_monsterinfo_stand),
    P(monsterinfo.idle, P_monsterinfo_idle),
    P(monsterinfo.search, P_monsterinfo_search),
    P(monsterinfo.walk, P_monsterinfo_walk),
    P(monsterinfo.run, P_monsterinfo_run),
    P(monsterinfo.dodge, P_monsterinfo_dodge),
    P(monsterinfo.attack, P_monsterinfo_attack),
    P(monsterinfo.melee, P_monsterinfo_melee),
    P(monsterinfo.sight, P_monsterinfo_sight),
    P(monsterinfo.checkattack, P_monsterinfo_checkattack),

    F(monsterinfo.pause_framenum),
    F(monsterinfo.attack_finished),

    V(monsterinfo.saved_goal),
    F(monsterinfo.search_framenum),
    F(monsterinfo.trail_framenum),
    V(monsterinfo.last_sighting),
    I(monsterinfo.attack_state),
    I(monsterinfo.lefty),
    F(monsterinfo.idle_framenum),
    I(monsterinfo.linkcount),

    I(monsterinfo.power_armor_type),
    I(monsterinfo.power_armor_power),

    {0}
#undef _OFS
};

static const save_field_t levelfields[] = {
#define _OFS LLOFS
    E(disguise_violator),               // ROGUE
    I(disguise_violation_framenum),     // ROGUE
    I(framenum),
    F(time),

    SZ(level_name, MAX_QPATH),
    SZ(mapname, MAX_QPATH),
    SZ(nextmap, MAX_QPATH),

    I(intermission_framenum),
    L(changemap),
    I(exitintermission),
    V(intermission_origin),
    V(intermission_angle),

    E(sight_client),

    E(sight_entity),
    I(sight_entity_framenum),
    E(sound_entity),
    I(sound_entity_framenum),
    E(sound2_entity),
    I(sound2_entity_framenum),

    I(pic_health),

    I(total_secrets),
    I(found_secrets),

    I(total_goals),
    I(found_goals),

    I(total_monsters),
    I(killed_monsters),

    I(body_que),

    I(power_cubes),

    E(disguise_violator),    // ROGUE

    {0}

#undef _OFS
};

static const save_field_t clientfields[] = {
#define _OFS CLOFS
    I(ps.pmove.pm_type),

#if USE_NEW_GAME_API
    IA(ps.pmove.origin, 3),
    IA(ps.pmove.velocity, 3),
    S(ps.pmove.pm_flags),
    S(ps.pmove.pm_time),
#else
    SA(ps.pmove.origin, 3),
    SA(ps.pmove.velocity, 3),
    B(ps.pmove.pm_flags),
    B(ps.pmove.pm_time),
#endif
    S(ps.pmove.gravity),
    SA(ps.pmove.delta_angles, 3),

    V(ps.viewangles),
    V(ps.viewoffset),
    V(ps.kick_angles),

    V(ps.gunangles),
    V(ps.gunoffset),
    I(ps.gunindex),
    I(ps.gunframe),

    FA(ps.blend, 4),

    F(ps.fov),

    I(ps.rdflags),

    SA(ps.stats, MAX_STATS),

    SZ(pers.userinfo, MAX_INFO_STRING),
    SZ(pers.netname, 16),
    I(pers.hand),

    O(pers.connected),

    I(pers.health),
    I(pers.max_health),
    I(pers.savedFlags),

    I(pers.selected_item),
    IA(pers.inventory, MAX_ITEMS),

    I(pers.max_bullets),
    I(pers.max_shells),
    I(pers.max_rockets),
    I(pers.max_grenades),
    I(pers.max_cells),
    I(pers.max_slugs),
    // RAFAEL
    I(pers.max_magslug),
    I(pers.max_trap),

    T(pers.weapon),
    T(pers.lastweapon),

    I(pers.power_cubes),
    I(pers.score),

    I(pers.game_helpchanged),
    I(pers.helpchanged),

    O(pers.spectator),

    // CTF (R-CTF-5, R-SAVE-3a).  Savegames are sp-only (R-ENG-6) so none of
    // this can be reached today, but a descriptor that is absent because the
    // field "cannot be saved yet" is exactly the R-VER-15 item 2 defect --
    // xatrix's quadfire_framenum was lost that way.
    I(resp.ctf_team),
    I(resp.ctf_state),
    F(resp.ctf_lasthurtcarrier),
    F(resp.ctf_lastreturnedflag),
    F(resp.ctf_flagsince),
    F(resp.ctf_lastfraggedcarrier),
    O(resp.id_state),
    F(resp.lastidtime),
    O(resp.voted),
    O(resp.ready),
    O(resp.admin),
    // resp.ghost is a pointer into ctfgame.ghosts[], which is a static array in
    // g_ctf.c that no descriptor can reach and that a savegame has no copy of.
    // Deliberately not saved, and named here so dsweep.py's finding is answered
    // rather than merely absent.

    E(ctf_grapple),
    I(ctf_grapplestate),
    I(ctf_hookstate),
    F(ctf_grapplereleasetime),
    F(ctf_regentime),
    F(ctf_techsndtime),
    F(ctf_lasttechmsg),
    // menu_owner, ctf_menu, menutime and menudirty are not saved either: the
    // handle is TAG_LEVEL memory that SpawnEntities frees and g_spawn.c clears,
    // so a reloaded game has no menu open and that is the correct state.

    O(showscores),
    O(showinventory),
    O(showhelp),
    O(showhelpicon),

    I(ammo_index),

    T(newweapon),

    I(damage_armor),
    I(damage_parmor),
    I(damage_blood),
    I(damage_knockback),
    V(damage_from),

    F(killer_yaw),

    I(weaponstate),

    V(kick_angles),
    V(kick_origin),
    F(v_dmg_roll),
    F(v_dmg_pitch),
    F(v_dmg_time),
    F(fall_time),
    F(fall_value),
    F(damage_alpha),
    F(bonus_alpha),
    V(damage_blend),
    V(v_angle),
    F(bobtime),
    V(oldviewangles),
    V(oldvelocity),

    I(next_drown_framenum),
    I(old_waterlevel),
    I(breather_sound),

    I(machinegun_shots),

    I(anim_end),
    I(anim_priority),
    O(anim_duck),
    O(anim_run),

    // powerup timers
    I(quad_framenum),
    // ROGUE - powerup timers Ground Zero never described to the savegame code
    I(double_framenum),
    I(ir_framenum),
    I(nuke_framenum),
    I(tracker_pain_framenum),
    I(invincible_framenum),
    I(breather_framenum),
    I(enviro_framenum),

    O(grenade_blew_up),
    // RAFAEL
    E(owned_sphere),                    // ROGUE -- see the note in entityfields
    F(trap_time),                       // XATRIX -- the merge brought
                                        // quadfire_framenum and trap_blew_up
                                        // but not this one
    I(double_framenum),                 // ROGUE
    I(ir_framenum),                     // ROGUE
    I(nuke_framenum),                   // ROGUE
    I(tracker_pain_framenum),           // ROGUE
    I(pers.max_tesla),                  // ROGUE
    I(pers.max_prox),                   // ROGUE
    I(pers.max_mines),                  // ROGUE
    I(pers.max_flechettes),             // ROGUE
    I(quadfire_framenum),
    O(trap_blew_up),
    I(grenade_framenum),
    I(silencer_shots),
    I(weapon_sound),

    I(pickup_msg_framenum),

    E(owned_sphere),         // ROGUE

    {0}

#undef _OFS
};

static const save_field_t gamefields[] = {
#define _OFS GLOFS
    SZ(helpmessage1, 512),
    SZ(helpmessage2, 512),

    I(maxclients),
    I(maxentities),

    I(serverflags),

    I(num_items),

    O(autosaved),

    {0}
#undef _OFS
};

//=========================================================

static void write_data(void *buf, size_t len, gzFile f)
{
    if (gzwrite(f, buf, len) != len) {
        gzclose(f);
        gi.error("%s: couldn't write %zu bytes", __func__, len);
    }
}

static void write_short(gzFile f, int16_t v)
{
    v = LittleShort(v);
    write_data(&v, sizeof(v), f);
}

static void write_int(gzFile f, int32_t v)
{
    v = LittleLong(v);
    write_data(&v, sizeof(v), f);
}

static void write_float(gzFile f, float v)
{
    v = LittleFloat(v);
    write_data(&v, sizeof(v), f);
}

static void write_string(gzFile f, char *s)
{
    size_t len;

    if (!s) {
        write_int(f, -1);
        return;
    }

    len = strlen(s);
    if (len >= 65536) {
        gzclose(f);
        gi.error("%s: bad length", __func__);
    }
    write_int(f, len);
    write_data(s, len, f);
}

static void write_vector(gzFile f, vec_t *v)
{
    write_float(f, v[0]);
    write_float(f, v[1]);
    write_float(f, v[2]);
}

static void write_index(gzFile f, void *p, size_t size, const void *start, int max_index)
{
    uintptr_t diff;

    if (!p) {
        write_int(f, -1);
        return;
    }

    diff = (uintptr_t)p - (uintptr_t)start;
    if (diff > max_index * size) {
        gzclose(f);
        gi.error("%s: pointer out of range: %p", __func__, p);
    }
    if (diff % size) {
        gzclose(f);
        gi.error("%s: misaligned pointer: %p", __func__, p);
    }
    write_int(f, (int)(diff / size));
}

static void write_pointer(gzFile f, void *p, ptr_type_t type)
{
    const save_ptr_t *ptr;
    int i;

    if (!p) {
        write_int(f, -1);
        return;
    }

    for (i = 0, ptr = save_ptrs; i < num_save_ptrs; i++, ptr++) {
        if (ptr->type == type && ptr->ptr == p) {
            write_int(f, i);
            return;
        }
    }

    gzclose(f);
    gi.error("%s: unknown pointer: %p", __func__, p);
}

static void write_field(gzFile f, const save_field_t *field, void *base)
{
    void *p = (byte *)base + field->ofs;
    int i;

    switch (field->type) {
    case F_BYTE:
        write_data(p, field->size, f);
        break;
    case F_SHORT:
        for (i = 0; i < field->size; i++) {
            write_short(f, ((short *)p)[i]);
        }
        break;
    case F_INT:
        for (i = 0; i < field->size; i++) {
            write_int(f, ((int *)p)[i]);
        }
        break;
    case F_BOOL:
        for (i = 0; i < field->size; i++) {
            write_int(f, ((bool *)p)[i]);
        }
        break;
    case F_FLOAT:
        for (i = 0; i < field->size; i++) {
            write_float(f, ((float *)p)[i]);
        }
        break;
    case F_VECTOR:
        write_vector(f, (vec_t *)p);
        break;

    case F_ZSTRING:
        write_string(f, (char *)p);
        break;
    case F_LSTRING:
        write_string(f, *(char **)p);
        break;

    case F_EDICT:
        write_index(f, *(void **)p, sizeof(edict_t), g_edicts, game.maxentities - 1);
        break;
    case F_CLIENT:
        write_index(f, *(void **)p, sizeof(gclient_t), game.clients, game.maxclients - 1);
        break;
    case F_ITEM:
        write_index(f, *(void **)p, sizeof(gitem_t), itemlist, game.num_items - 1);
        break;

    case F_POINTER:
        write_pointer(f, *(void **)p, field->size);
        break;

    default:
        gi.error("%s: unknown field type", __func__);
    }
}

static void write_fields(gzFile f, const save_field_t *fields, void *base)
{
    const save_field_t *field;

    for (field = fields; field->type; field++) {
        write_field(f, field, base);
    }
}

static void read_data(void *buf, size_t len, gzFile f)
{
    if (gzread(f, buf, len) != len) {
        gzclose(f);
        gi.error("%s: couldn't read %zu bytes", __func__, len);
    }
}

static int read_short(gzFile f)
{
    int16_t v;

    read_data(&v, sizeof(v), f);
    v = LittleShort(v);

    return v;
}

static int read_int(gzFile f)
{
    int32_t v;

    read_data(&v, sizeof(v), f);
    v = LittleLong(v);

    return v;
}

static float read_float(gzFile f)
{
    float v;

    read_data(&v, sizeof(v), f);
    v = LittleFloat(v);

    return v;
}

static char *read_string(gzFile f)
{
    int len;
    char *s;

    len = read_int(f);
    if (len == -1) {
        return NULL;
    }

    if (len < 0 || len >= 65536) {
        gzclose(f);
        gi.error("%s: bad length", __func__);
    }

    s = gi.TagMalloc(len + 1, TAG_LEVEL);
    read_data(s, len, f);
    s[len] = 0;

    return s;
}

static void read_zstring(gzFile f, char *s, size_t size)
{
    int len;

    len = read_int(f);
    if (len < 0 || len >= size) {
        gzclose(f);
        gi.error("%s: bad length", __func__);
    }

    read_data(s, len, f);
    s[len] = 0;
}

static void read_vector(gzFile f, vec_t *v)
{
    v[0] = read_float(f);
    v[1] = read_float(f);
    v[2] = read_float(f);
}

static void *read_index(gzFile f, size_t size, const void *start, int max_index)
{
    int index;
    byte *p;

    index = read_int(f);
    if (index == -1) {
        return NULL;
    }

    if (index < 0 || index > max_index) {
        gzclose(f);
        gi.error("%s: bad index", __func__);
    }

    p = (byte *)start + index * size;
    return p;
}

static void *read_pointer(gzFile f, ptr_type_t type)
{
    int index;
    const save_ptr_t *ptr;

    index = read_int(f);
    if (index == -1) {
        return NULL;
    }

    if (index < 0 || index >= num_save_ptrs) {
        gzclose(f);
        gi.error("%s: bad index", __func__);
    }

    ptr = &save_ptrs[index];
    if (ptr->type != type) {
        gzclose(f);
        gi.error("%s: type mismatch", __func__);
    }

    return (void *)ptr->ptr;
}

static void read_field(gzFile f, const save_field_t *field, void *base)
{
    void *p = (byte *)base + field->ofs;
    int i;

    switch (field->type) {
    case F_BYTE:
        read_data(p, field->size, f);
        break;
    case F_SHORT:
        for (i = 0; i < field->size; i++) {
            ((short *)p)[i] = read_short(f);
        }
        break;
    case F_INT:
        for (i = 0; i < field->size; i++) {
            ((int *)p)[i] = read_int(f);
        }
        break;
    case F_BOOL:
        for (i = 0; i < field->size; i++) {
            ((bool *)p)[i] = read_int(f);
        }
        break;
    case F_FLOAT:
        for (i = 0; i < field->size; i++) {
            ((float *)p)[i] = read_float(f);
        }
        break;
    case F_VECTOR:
        read_vector(f, (vec_t *)p);
        break;

    case F_LSTRING:
        *(char **)p = read_string(f);
        break;
    case F_ZSTRING:
        read_zstring(f, (char *)p, field->size);
        break;

    case F_EDICT:
        *(edict_t **)p = read_index(f, sizeof(edict_t), g_edicts, game.maxentities - 1);
        break;
    case F_CLIENT:
        *(gclient_t **)p = read_index(f, sizeof(gclient_t), game.clients, game.maxclients - 1);
        break;
    case F_ITEM:
        *(const gitem_t **)p = read_index(f, sizeof(gitem_t), itemlist, game.num_items - 1);
        break;

    case F_POINTER:
        *(void **)p = read_pointer(f, field->size);
        break;

    default:
        gi.error("%s: unknown field type", __func__);
    }
}

static void read_fields(gzFile f, const save_field_t *fields, void *base)
{
    const save_field_t *field;

    for (field = fields; field->type; field++) {
        read_field(f, field, base);
    }
}

//=========================================================

#define SAVE_MAGIC1     MakeLittleLong('S','S','V','1')
#define SAVE_MAGIC2     MakeLittleLong('S','A','V','1')
#if USE_NEW_GAME_API
#define SAVE_VERSION    0x100
#else
#define SAVE_VERSION    8
#endif

static void check_gzip(int magic)
{
#if !USE_ZLIB
    if ((magic & 0xe0ffffff) == 0x00088b1f)
        gi.error("Savegame is compressed, but no gzip support linked in");
#endif
}

/*
============
WriteGame

This will be called whenever the game goes to a new level,
and when the user explicitly saves the game.

Game information include cross level data, like multi level
triggers, help computer info, and all client states.

A single player death will automatically restore from the
last save position.
============
*/
void WriteGame(const char *filename, qboolean autosave)
{
    gzFile  f;
    int     i;

    if (!autosave)
        SaveClientData();

    f = gzopen(filename, "wb");
    if (!f)
        gi.error("Couldn't open %s", filename);

    write_int(f, SAVE_MAGIC1);
    write_int(f, SAVE_VERSION);

    game.autosaved = autosave;
    write_fields(f, gamefields, &game);
    game.autosaved = false;

    for (i = 0; i < game.maxclients; i++) {
        write_fields(f, clientfields, &game.clients[i]);
    }

    if (gzclose(f))
        gi.error("Couldn't write %s", filename);
}

void ReadGame(const char *filename)
{
    gzFile  f;
    int     i;

    gi.FreeTags(TAG_GAME);

    f = gzopen(filename, "rb");
    if (!f)
        gi.error("Couldn't open %s", filename);

    gzbuffer(f, 65536);

    i = read_int(f);
    if (i != SAVE_MAGIC1) {
        gzclose(f);
        check_gzip(i);
        gi.error("Not a Q2PRO save game");
    }

    i = read_int(f);
    if (i != SAVE_VERSION) {
        gzclose(f);
        gi.error("Savegame from different version (got %d, expected %d)", i, SAVE_VERSION);
    }

    read_fields(f, gamefields, &game);

    // should agree with server's version
    // ...and the same defect in the savegame's own consistency check, where the
    // comparison had become a tautology and could never fire (R-47).
    if (game.maxclients != (int)maxclients->value) {
        gzclose(f);
        gi.error("Savegame has bad maxclients");
    }
    if (game.maxentities <= game.maxclients || game.maxentities > game.csr.max_edicts) {
        gzclose(f);
        gi.error("Savegame has bad maxentities");
    }

    g_edicts = gi.TagMalloc(game.maxentities * sizeof(g_edicts[0]), TAG_GAME);
    globals.edicts = g_edicts;
    globals.max_edicts = game.maxentities;

    game.clients = gi.TagMalloc(game.maxclients * sizeof(game.clients[0]), TAG_GAME);
    for (i = 0; i < game.maxclients; i++) {
        read_fields(f, clientfields, &game.clients[i]);
    }

    gzclose(f);
}

//==========================================================

/*
=================
WriteLevel

=================
*/
void WriteLevel(const char *filename)
{
    int     i;
    edict_t *ent;
    gzFile  f;

    f = gzopen(filename, "wb");
    if (!f)
        gi.error("Couldn't open %s", filename);

    write_int(f, SAVE_MAGIC2);
    write_int(f, SAVE_VERSION);

    // write out level_locals_t
    write_fields(f, levelfields, &level);

    // write out all the entities
    for (i = 0; i < globals.num_edicts; i++) {
        ent = &g_edicts[i];
        if (!ent->inuse)
            continue;
        write_int(f, i);
        write_fields(f, entityfields, ent);
    }
    write_int(f, -1);

    if (gzclose(f))
        gi.error("Couldn't write %s", filename);
}

/*
=================
ReadLevel

SpawnEntities will already have been called on the
level the same way it was when the level was saved.

That is necessary to get the baselines
set up identically.

The server will have cleared all of the world links before
calling ReadLevel.

No clients are connected yet.
=================
*/
void ReadLevel(const char *filename)
{
    int     entnum;
    gzFile  f;
    int     i;
    edict_t *ent;

    // free any dynamic memory allocated by loading the level
    // base state
    gi.FreeTags(TAG_LEVEL);

    f = gzopen(filename, "rb");
    if (!f)
        gi.error("Couldn't open %s", filename);

    gzbuffer(f, 65536);

    // wipe all the entities
    memset(g_edicts, 0, game.maxentities * sizeof(g_edicts[0]));
    globals.num_edicts = game.maxclients + 1;

    i = read_int(f);
    if (i != SAVE_MAGIC2) {
        gzclose(f);
        check_gzip(i);
        gi.error("Not a Q2PRO save game");
    }

    i = read_int(f);
    if (i != SAVE_VERSION) {
        gzclose(f);
        gi.error("Savegame from different version (got %d, expected %d)", i, SAVE_VERSION);
    }

    // load the level locals
    read_fields(f, levelfields, &level);

    // load all the entities
    while (1) {
        entnum = read_int(f);
        if (entnum == -1)
            break;
        if (entnum < 0 || entnum >= game.maxentities) {
            gzclose(f);
            gi.error("%s: bad entity number", __func__);
        }
        if (entnum >= globals.num_edicts)
            globals.num_edicts = entnum + 1;

        ent = &g_edicts[entnum];
        read_fields(f, entityfields, ent);
        ent->inuse = true;
        ent->s.number = entnum;

        // let the server rebuild world links for this ent
        memset(&ent->area, 0, sizeof(ent->area));
        gi.linkentity(ent);
    }

    gzclose(f);

    // PMM - rebuild the hint path chains
//  InitHintPaths();
    // pmm

    // mark all clients as unconnected
    for (i = 0; i < game.maxclients; i++) {
        ent = &g_edicts[i + 1];
        ent->client = game.clients + i;
        ent->client->pers.connected = false;
    }

    // do any load time things at this point
    for (i = 0; i < globals.num_edicts; i++) {
        ent = &g_edicts[i];

        if (!ent->inuse)
            continue;

        // fire any cross-level triggers
        if (ent->classname)
            if (strcmp(ent->classname, "target_crosslevel_target") == 0)
                ent->nextthink = level.framenum + ent->delay * BASE_FRAMERATE;

        if (ent->think == func_clock_think || ent->use == func_clock_use) {
            char *msg = ent->message;
            ent->message = gi.TagMalloc(CLOCK_MESSAGE_SIZE, TAG_LEVEL);
            if (msg) {
                Q_strlcpy(ent->message, msg, CLOCK_MESSAGE_SIZE);
                gi.TagFree(msg);
            }
        }
    }

    // refresh global precache indices
    G_RefreshPrecaches();
}
