#!/usr/bin/env python3
"""A donor's own surface, used in a spine file, must be inside that donor's gate.

See `SPECS.md` section 7 (reconciliation rules) on the layer boundary.

WHY.  Every ruleset shares one library and one `gclient_t`, so a donor's fields
exist for every client and its functions link into every build.  Nothing stops a
spine file reading RA2's `resp.fightstate` under `sp`; the field is there, it
compiles, and it is **zero** -- which is `FIGHT_SPECTATING`, which is not
`FIGHT_ALIVE`, which makes the test false.  Six sites in the arena merge did
exactly that and each one deleted behaviour from the whole game rather than
adding any to arena.  None was a merge conflict and none was visible to a
compiler, an audit or a boot.

`gates.py` asks the neighbouring question -- "is a ruleset chosen by testing a
cvar?" -- and cannot see this one, because these sites test a *field*, not a
cvar, and the field is legitimately in the union.

WHAT COUNTS AS THE DONOR'S SURFACE.  Everything declared in `src/<donor>/*.h`
that is not also declared in `g_local.h` or defined in a spine file.  A helper
that moved into the shared tree on purpose -- `stuffcmd` did -- stops being the
donor's and stops being reported, automatically.

WHAT COUNTS AS A GATE.  `G_Ruleset() == RULESET_<DONOR>` on the occurrence's own
line, on the line that opens an enclosing block, or as the whole condition of a
braceless `if` immediately above.  A named predicate that is only true for one
ruleset counts too, by being listed in PREDICATES -- `G_IsObserver()` is not one
of those, deliberately: it is true under three.

WHAT IS NOT A FINDING.  Declarations (`g_local.h`), the savegame descriptor
tables and the dispatch tables: those are data naming a field, not behaviour
reading one.  Comments and string literals are stripped first, without which
this reports every prose mention of `teams`.

PACK-EXCLUSIVE ALIASES.  `content_flavour` is deliberately a server-wide
selection bit, not entity identity.  A pack-exclusive classname that reuses a
shared monster function must still receive its own donor behavior when that
layer is off.  This checks the two aliases where that distinction matters:
Medic Commander and Daedalus.  It also preserves the donor's Kamikaze guard
ordering, so the base Flyer arm cannot preempt the exclusive entity's move.

USAGE
    tools/donorgate.py [--tree src]
    tools/donorgate.py --selftest
"""
import argparse
import glob
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# donor subdirectory -> the ruleset whose gate its surface needs.
#
# A DONOR IS NOT A RULESET, and since the flattening it is not even one-to-one.
# `src/tourney/` is reachable from FOUR rulesets -- dm, dmpro, tdm, duel, which
# are OSP's four modes of play promoted to first-class values -- so
# the gate its surface needs is the family predicate, not a single equality.
# The donor identity is the stable half of that and is what this map is keyed
# on; how many rulesets dispatch into a donor is the half that moved.
DONORS = {
    'arena': 'RULESET_ARENA',
    'tourney': 'RULESET_OSP',
}

# Tests that are true for exactly one ruleset -- or, for RULESET_OSP, for
# exactly one donor's four -- and therefore gate as well as the explicit one
# does.  Kept short on purpose.  `menu_owner == MENU_ARENA` is the strongest of
# them: only arena's engine ever sets that owner, and the field is
# the single answer to "whose menu is open".
#
# G_IsOspRuleset() is the whole gate for tourney's surface, not a shortcut for
# one: there is no `RULESET_OSP` enum value, and a site that named all four by
# hand would be a list that the next ruleset gets left off.  OSP_IsMatch() and
# OSP_IsTeams() are strictly narrower -- each is true only under rulesets
# G_IsOspRuleset() is also true under -- so they gate too.
PREDICATES = {
    'RULESET_ARENA': ('MENU_ARENA',),
    'RULESET_OSP': ('MENU_TOURNEY', 'G_IsOspRuleset', 'OSP_IsMatch',
                    'OSP_IsTeams', 'RULESET_DM', 'RULESET_DMPRO',
                    'RULESET_TDM', 'RULESET_DUEL'),
}

# Fields that are the donor's but are declared in g_local.h, so the "declared in
# the donor's header" rule cannot find them.  One line per donor.
FIELDS = {
    'RULESET_ARENA': (
        'fightstate', 'teamnum', 'context', 'omode', 'lastomode',
        'omode_buttons', 'track_target', 'spawn_recheck', 'damagedealt',
        'zbotcount', 'zbotlastcheck', 'zbotscore', 'isbot', 'ra_voted',
        'ra_votes', 'scoremode', 'oldangles', 'spamcount', 'spamtime',
        'menuqueue', 'curmenulink', 'ra_menutime', 'menuusetime', 'menutext',
        'showmotd', 'teammember',
    ),
    'RULESET_TOURNEY': (),
}

# Files whose mention of a donor name is data or declaration, not behaviour.
EXEMPT_FILES = ('g_local.h', 'g_ptrs.c', 'g_ptrs.h')

# Line-level exemptions inside otherwise-live files: a descriptor row names a
# field so that the savegame writer can find it; it does not read it.
DESCRIPTOR = re.compile(r'^\s*[A-Z]\((?:resp\.|pers\.)?\w+\),\s*$')

# ...and a DESIGNATED INITIALISER row names a function so that a table can hold
# it: `.pickup = OSP_Pickup_Rune,` in itemlist[], `.CheckRules = OSP_CheckRules,`
# in a ruleset_ops_t.  The docstring has always said dispatch tables are data
# rather than behaviour; before the runes landed, DESCRIPTOR was the only line
# shape that said so, because CTF's techs are the one other case and CTF is not
# in DONORS.  A row like this is reached only through whatever spawns or invokes
# the table entry, and THAT is where the gate belongs -- for an item, the gate
# is whether the item is in the world at all.
INITIALISER = re.compile(r'^\s*\.\w+\s*=\s*[A-Za-z_]\w*,\s*$')

# Top-level only -- column 0.  Without that anchor this collects every STRUCT
# MEMBER in the donor's typedefs, and arena.h has members called `value`,
# `name` and `count`, which then match half the tree.
DECL = re.compile(r'^(?:extern\s+)?[A-Za-z_][\w \t*]*?\b([A-Za-z_]\w*)\s*[(;\[]', re.M)


def strip(t):
    t = re.sub(r'/\*.*?\*/', lambda m: re.sub(r'[^\n]', ' ', m.group(0)), t, flags=re.S)
    t = re.sub(r'//[^\n]*', '', t)
    return re.sub(r'"(\\.|[^"\\\n])*"', '""', t)


def read(p):
    return open(p, encoding='utf-8').read()


def span_mask(text):
    """Blank comments and literals without changing offsets for brace matching."""
    def blank(m):
        return re.sub(r'[^\n]', ' ', m.group(0))

    text = re.sub(r'/\*.*?\*/', blank, text, flags=re.S)
    text = re.sub(r'//[^\n]*', blank, text)
    text = re.sub(r'"(?:\\.|[^"\\\n])*"', blank, text)
    return re.sub(r"'(?:\\.|[^'\\\n])*'", blank, text)


def function_span(text, name):
    """Return the byte span of one simple C function definition, or None."""
    source = span_mask(text)
    m = re.search(r'^[A-Za-z_][\w \t*]*?\b%s\s*\([^;{}]*\)\s*\{' %
                  re.escape(name), source, re.M)
    if not m:
        return None
    start, depth = m.end() - 1, 0
    for end in range(start, len(source)):
        if source[end] == '{':
            depth += 1
        elif source[end] == '}':
            depth -= 1
            if depth == 0:
                return start, end + 1
    return None


def without_span(text, span):
    """Blank a span without moving its line numbers."""
    start, end = span
    return text[:start] + re.sub(r'[^\n]', ' ', text[start:end]) + text[end:]


# These shared functions serve one regular baseq2 entity and one
# pack-exclusive alias. The former follows the layer selection; the latter must
# retain the only donor behavior it has regardless of that server-wide bit.
EXCLUSIVE_ALIASES = (
    ('m_medic.c', 'medic_IsCommander', 'medic_UsesRogueBehavior',
     'monster_medic_commander'),
    ('m_hover.c', 'hover_IsDaedalus', 'hover_UsesRogueBehavior',
     'monster_daedalus'),
)

ROGUE_FLAVOUR = re.compile(
    r'\bself\s*->\s*content_flavour\s*&\s*CONTENT_ROGUE\b')

ROGUE_IDENTITIES = (
    'monster_stalker',
    'monster_turret',
    'monster_daedalus',
    'monster_carrier',
    'monster_widow',
    'monster_widow2',
    'monster_medic_commander',
    'monster_kamikaze',
)


def exclusive_alias_violations(tree, override=None):
    """Check the full entity-versus-layer selection boundary."""
    out = []
    filename = 'g_monster.c'
    raw = (override or {}).get(filename, read(os.path.join(tree, filename)))
    behavior_span = function_span(raw, 'M_UsesRogueBehavior')
    if not behavior_span:
        out.append('  !! g_monster.c: `M_UsesRogueBehavior` is missing '
                   '(effective Rogue behavior)')
    else:
        behavior_text = raw[behavior_span[0]:behavior_span[1]]
        for pattern, explanation in (
            (r'\bif\s*\(\s*!\s*ent\s*\)', 'reject a NULL entity'),
            (r'\bent\s*->\s*content_flavour\s*&\s*CONTENT_ROGUE',
             'select the Rogue layer'),
            (r'\bif\s*\(\s*!\s*ent\s*->\s*classname\s*\)',
             'reject a NULL classname'),
        ):
            if not re.search(pattern, behavior_text):
                out.append('  !! g_monster.c:%d: `M_UsesRogueBehavior` must %s '
                           '(effective Rogue behavior)' %
                           (raw[:behavior_span[0]].count('\n') + 1, explanation))
        for rogue_classname in ROGUE_IDENTITIES:
            identity_rx = re.compile(
                r'!\s*strcmp\s*\(\s*ent\s*->\s*classname\s*,\s*"%s"\s*\)'
                % re.escape(rogue_classname))
            if not identity_rx.search(behavior_text):
                out.append('  !! g_monster.c:%d: `M_UsesRogueBehavior` must '
                           'recognize `%s` (effective Rogue behavior)' %
                           (raw[:behavior_span[0]].count('\n') + 1,
                            rogue_classname))
        found_identities = set(re.findall(
            r'!\s*strcmp\s*\(\s*ent\s*->\s*classname\s*,\s*"([^"]+)"\s*\)',
            behavior_text))
        for rogue_classname in sorted(found_identities - set(ROGUE_IDENTITIES)):
            out.append('  !! g_monster.c:%d: `M_UsesRogueBehavior` must not '
                       'treat `%s` as a Rogue-exclusive identity '
                       '(effective Rogue behavior)' %
                       (raw[:behavior_span[0]].count('\n') + 1,
                        rogue_classname))

    for filename, identity, behavior, classname in EXCLUSIVE_ALIASES:
        path = os.path.join(tree, filename)
        raw = (override or {}).get(filename, read(path))

        identity_span = function_span(raw, identity)
        if not identity_span:
            out.append('  !! %s: `%s` is missing (exclusive identity)' %
                       (filename, identity))
            continue
        identity_text = raw[identity_span[0]:identity_span[1]]
        identity_rx = re.compile(
            r'\breturn\s*!\s*strcmp\s*\(\s*self\s*->\s*classname\s*,\s*"%s"\s*\)\s*;'
            % re.escape(classname))
        if not identity_rx.search(identity_text):
            out.append('  !! %s:%d: `%s` does not identify `%s` by classname '
                       '(exclusive identity)' %
                       (filename, raw[:identity_span[0]].count('\n') + 1,
                        identity, classname))

        behavior_span = function_span(raw, behavior)
        if not behavior_span:
            out.append('  !! %s: `%s` is missing (effective Rogue behavior)' %
                       (filename, behavior))
            continue
        behavior_text = raw[behavior_span[0]:behavior_span[1]]
        behavior_rx = re.compile(r'\breturn\s+M_UsesRogueBehavior\s*'
                                 r'\(\s*self\s*\)\s*;')
        if not behavior_rx.search(behavior_text):
            out.append('  !! %s:%d: `%s` must delegate to '
                       '`M_UsesRogueBehavior` (effective Rogue behavior)' %
                       (filename, raw[:behavior_span[0]].count('\n') + 1,
                        behavior))

        # Every relevant branch in this shared family must use the effective
        # behavior predicate. Leaving one raw test here reproduces the silent
        # split where a Commander or Daedalus gets only part of its own setup.
        outside = strip(without_span(raw, behavior_span))
        for m in ROGUE_FLAVOUR.finditer(outside):
            out.append('  !! %s:%d: raw CONTENT_ROGUE selector bypasses `%s` '
                       '(exclusive identity)' %
                       (filename, outside[:m.start()].count('\n') + 1,
                        behavior))

    filename = 'm_flyer.c'
    raw = (override or {}).get(filename, read(os.path.join(tree, filename)))
    span = function_span(raw, 'flyer_attack')
    if not span:
        out.append('  !! m_flyer.c: `flyer_attack` is missing '
                   '(Kamikaze ordering)')
        return out
    attack = strip(raw[span[0]:span[1]])
    mass = re.search(r'\bif\s*\(\s*self\s*->\s*mass\s*>\s*50\s*\)', attack)
    base = re.search(
        r'\bif\s*\(\s*!\s*\(\s*self\s*->\s*content_flavour\s*&\s*CONTENT_ROGUE\s*\)\s*\)',
        attack)
    line = raw[:span[0]].count('\n') + 1
    if not mass:
        out.append('  !! m_flyer.c:%d: Kamikaze mass guard is missing from '
                   '`flyer_attack` (Kamikaze ordering)' % line)
    elif not base:
        out.append('  !! m_flyer.c:%d: base Flyer layer guard is missing from '
                   '`flyer_attack` (Kamikaze ordering)' % line)
    elif mass.start() > base.start():
        out.append('  !! m_flyer.c:%d: Kamikaze mass guard follows the base '
                   'Flyer layer guard (Kamikaze ordering)' % line)
    return out


AI_REQUIREMENTS = (
    ('g_ai.c', 'AI_SetSightClient',
     ('level.sight_client = NextSightClient',
      'level.rogue_sight_client = NextSightClient',
      'FL_NOTARGET | FL_DISGUISED')),
    ('g_ai.c', 'ai_stand',
     ('M_UsesRogueBehavior(self)', '15 + random() * 15',
      '(1 + random()) * 15')),
    ('g_ai.c', 'ai_walk',
     ('M_UsesRogueBehavior(self)', '15 + random() * 15',
      '(1 + random()) * 15')),
    ('g_ai.c', 'ai_charge', ('if (!M_UsesRogueBehavior(self))',)),
    ('g_ai.c', 'ai_turn', ('M_ChangeYaw(self);',)),
    ('g_ai.c', 'visible', ('M_UsesRogueBehavior(self)',)),
    ('g_ai.c', 'FoundTarget', ('M_UsesRogueBehavior(self)',)),
    ('g_ai.c', 'FindTarget',
     ('rogue = M_UsesRogueBehavior(self)',
      'rogue ? level.rogue_sight_client : level.sight_client',
      'level.disguise_violator',
      'client->enemy->flags & FL_DISGUISED',
      # Gladiator's guard is outside its ROGUE fence, so it belongs to
      # both arms.  Splitting it bought nothing -- the base donor's bare deref
      # can only differ from the guarded form by crashing.
      'client->owner && (client->owner->flags & FL_NOTARGET)')),
    ('g_ai.c', 'M_CheckAttack', ('return bq2_M_CheckAttack(self);',)),
    ('g_ai.c', 'ai_checkattack', ('return bq2_ai_checkattack(self, dist);',)),
    ('g_ai.c', 'ai_run', ('bq2_ai_run(self, dist);',)),
    ('g_monster.c', 'monster_use', ('M_UsesRogueBehavior(self)',)),
    ('g_monster.c', 'monster_triggered_spawn',
     ('M_UsesRogueBehavior(self)', 'FL_DISGUISED')),
    ('g_monster.c', 'stationarymonster_triggered_spawn',
     ('M_UsesRogueBehavior(self)', 'FL_DISGUISED')),
    ('g_combat.c', 'Killed',
     ('M_UsesRogueBehavior(targ)', 'targ->enemy->owner == targ',
      'targ->owner = attacker')),
    ('g_combat.c', 'M_ReactToDamage',
     ('rogue_behavior = M_UsesRogueBehavior(targ)', 'AI_IGNORE_SHOTS',
      '"monster_tank"', '"monster_supertank"', '"monster_makron"',
      '"monster_jorg"')),
    ('g_misc.c', 'SP_misc_explobox',
     ('self->think = barrel_start;', 'self->think = M_droptofloor;')),
    ('g_misc.c', 'misc_deadsoldier_die', ('self->health > -30',
                                           'self->health > -80')),
    # The sight cache is split in two; this is its only other
    # reader, and a Rogue-effective Makron must not read the base cache,
    # which still reports a disguised player.
    ('m_boss32.c', 'MakronSpawn',
     ('M_UsesRogueBehavior(self) ? level.rogue_sight_client',
      ': level.sight_client')),
)

SAVE_FORMAT_REQUIREMENTS = (
    ('g_save.c', 'E(rogue_sight_client)'),
    ('g_save.c', '#define SAVE_VERSION    0x101'),
    ('g_save.c', '#define SAVE_VERSION    9'),
)


def shared_behavior_violations(tree, override=None):
    """Require both donor arms where one shared implementation serves both."""
    out = []
    for filename, function, requirements in AI_REQUIREMENTS:
        raw = (override or {}).get(filename, read(os.path.join(tree, filename)))
        span = function_span(raw, function)
        if not span:
            out.append('  !! %s: `%s` is missing (shared behavior)' %
                       (filename, function))
            continue
        body = raw[span[0]:span[1]]
        for required in requirements:
            if required not in body:
                out.append('  !! %s:%d: `%s` must retain `%s` '
                           '(shared behavior)' %
                           (filename, raw[:span[0]].count('\n') + 1,
                            function, required))
        if filename == 'g_ai.c' and function == 'ai_charge':
            stale_guard = body.find('if (!self->enemy || !self->enemy->inuse)')
            selector = body.find('if (!M_UsesRogueBehavior(self))')
            if stale_guard < 0 or stale_guard > selector:
                out.append('  !! g_ai.c:%d: `ai_charge` must reject a stale '
                           'Tesla enemy before selecting an actor arm '
                           '(shared Tesla state)' %
                           (raw[:span[0]].count('\n') + 1))
        if filename == 'g_ai.c' and function == 'FindTarget':
            # The guarded form above can coexist with a second, bare one --
            # which is exactly the shape that was removed -- so require that
            # EVERY read of client->owner->flags is a guarded read.
            reads = len(re.findall(r'client->owner->flags', body))
            guarded = len(re.findall(
                r'client->owner\s*&&\s*\(\s*client->owner->flags', body))
            if reads != guarded:
                out.append('  !! g_ai.c:%d: `FindTarget` dereferences '
                           '`client->owner` without Gladiator\'s NULL guard '
                           '(shared noise owner)' %
                           (raw[:span[0]].count('\n') + 1))
        if filename == 'g_ai.c' and function == 'ai_turn':
            # Gladiator has no #ifdef ROGUE here, so the yaw is unconditional
            # and ANY guard on it is the violation -- not just one written
            # with M_UsesRogueBehavior.  The arm actually rejected is
            # port_rogue's own `AI_MANUAL_STEERING` test, which names no
            # selector token at all; keying on one let that exact line back
            # in with the audit silent.  So this asks the structural
            # question: is there a condition between the FindTarget return
            # and the yaw?
            tail = body[body.find('if (FindTarget(self))'):]
            cut = tail.find('return;')
            yaw = tail.find('M_ChangeYaw(self);')
            if cut < 0 or yaw < 0 or 'if (' in tail[cut + len('return;'):yaw]:
                out.append('  !! g_ai.c:%d: `ai_turn` must retain Gladiator\'s '
                           'unconditional `M_ChangeYaw` (shared yaw owner)' %
                           (raw[:span[0]].count('\n') + 1))
    for filename, required in SAVE_FORMAT_REQUIREMENTS:
        raw = (override or {}).get(filename, read(os.path.join(tree, filename)))
        if required not in raw:
            out.append('  !! %s: sight-cache serialization must retain `%s`'
                       % (filename, required))
    return out


def dynamic_flavour_violations(tree, override=None):
    """A direct dynamic monster spawn must retain its parent's resolved layer."""
    out = []
    filename = 'm_boss32.c'
    raw = (override or {}).get(filename, read(os.path.join(tree, filename)))

    toss_span = function_span(raw, 'MakronToss')
    if not toss_span:
        out.append('  !! m_boss32.c: `MakronToss` is missing '
                   '(dynamic monster flavor)')
        return out
    toss = raw[toss_span[0]:toss_span[1]]
    required = ('ent = G_Spawn();',
                'ent->content_flavour = self->content_flavour;',
                'ent->think = MakronSpawn;')
    for text in required:
        if text not in toss:
            out.append('  !! m_boss32.c:%d: `MakronToss` must retain `%s` '
                       '(dynamic monster flavor)' %
                       (raw[:toss_span[0]].count('\n') + 1, text))
    if all(text in toss for text in required):
        if not (toss.find(required[0]) < toss.find(required[1]) <
                toss.find(required[2])):
            out.append('  !! m_boss32.c:%d: `MakronToss` must copy the '
                       'parent flavor after G_Spawn and before MakronSpawn '
                       '(dynamic monster flavor)' %
                       (raw[:toss_span[0]].count('\n') + 1))

    spawn_span = function_span(raw, 'MakronSpawn')
    if not spawn_span or 'SP_monster_makron(self);' not in \
            raw[spawn_span[0]:spawn_span[1]]:
        out.append('  !! m_boss32.c: delayed Makron spawn must retain its '
                   'direct `SP_monster_makron` initialization '
                   '(dynamic monster flavor)')

    helper = (override or {}).get(
        'g_spawn.c', read(os.path.join(tree, 'g_spawn.c')))
    helper_span = function_span(helper, 'CreateMonster')
    if not helper_span or 'ED_CallSpawn(newEnt);' not in \
            helper[helper_span[0]:helper_span[1]]:
        out.append('  !! g_spawn.c: `CreateMonster` must latch dynamic '
                   'monster flavor through `ED_CallSpawn` '
                   '(dynamic monster flavor)')
    return out


DMGAME_CALLBACKS = (
    ('g_main.c', 'InitGame', 'InitGameRules'),
    ('g_main.c', 'CheckDMRules', 'DMGame.CheckDMRules'),
    ('g_spawn.c', 'SpawnEntities', 'DMGame.PostInitSetup'),
    ('g_combat.c', 'T_Damage', 'DMGame.ChangeDamage'),
    ('g_combat.c', 'T_Damage', 'DMGame.ChangeKnockback'),
    ('p_view.c', 'G_SetClientEffects', 'DMGame.PlayerEffects'),
    ('p_hud.c', 'DeathmatchScoreboardMessage', 'DMGame.DogTag'),
    ('p_client.c', 'ClientObituary', 'DMGame.Score'),
    ('p_client.c', 'player_die', 'DMGame.PlayerDeath'),
    ('p_client.c', 'ClientBeginDeathmatch', 'DMGame.ClientBegin'),
    ('p_client.c', 'ClientDisconnect', 'DMGame.PlayerDisconnect'),
    ('rogue/g_newdm.c', 'InitGameRules', 'DMGame.GameInit'),
)


# The DMGame TABLE is not the only way into a Rogue DM game: Tag's
# token and Deathball's goal reach the world through the spawn table and the
# item list.  Each file gets one predicate that asks the layer question, and
# every world-facing entry point asks it -- otherwise `gamerules 2` under an
# OSP ruleset spawns a token nothing scores, and `gamerules 3` spawns a
# dball goal that InitGameRules() no longer resets away.
ROGUE_DMGAME_ENTITIES = (
    ('rogue/dm_tag.c', 'Tag_Active', 'RDM_TAG',
     ('Tag_PickupToken', 'Tag_DropToken', 'SP_dm_tag_token')),
    ('rogue/dm_ball.c', 'DBall_Active', 'RDM_DEATHBALL',
     ('SP_dm_dball_ball', 'SP_dm_dball_team1_start',
      'SP_dm_dball_team2_start', 'SP_dm_dball_ball_start',
      'SP_dm_dball_speed_change', 'SP_dm_dball_goal')),
)


def rogue_dmgame_entity_violations(tree, override=None):
    """A Rogue DM game's own entities and items ask the layer question too."""
    out = []
    for filename, predicate, mode, entries in ROGUE_DMGAME_ENTITIES:
        raw = (override or {}).get(filename, read(os.path.join(tree, filename)))
        span = function_span(raw, predicate)
        if not span:
            out.append('  !! %s: `%s` is missing (Rogue DM entity)'
                       % (filename, predicate))
            continue
        body = strip(raw[span[0]:span[1]])
        for required in ('G_UsesRogueGameRules()', mode):
            if required not in body:
                out.append('  !! %s:%d: `%s` must name `%s` '
                           '(Rogue DM entity)' %
                           (filename, raw[:span[0]].count('\n') + 1,
                            predicate, required))
        for entry in entries:
            entry_span = function_span(raw, entry)
            if not entry_span:
                out.append('  !! %s: `%s` is missing (Rogue DM entity)'
                           % (filename, entry))
                continue
            entry_body = strip(raw[entry_span[0]:entry_span[1]])
            if predicate + '()' not in entry_body:
                out.append('  !! %s:%d: `%s` reaches the world without `%s` '
                           '(Rogue DM entity)' %
                           (filename, raw[:entry_span[0]].count('\n') + 1,
                            entry, predicate))
        # The raw comparison is what the predicate replaced; a second one is a
        # second authority, which is the defect rather than a duplicate.
        outside = strip(raw[:span[0]]) + strip(raw[span[1]:])
        if re.search(r'gamerules\s*->\s*value', outside):
            out.append('  !! %s: `gamerules->value` is read outside `%s` '
                       '(Rogue DM entity)' % (filename, predicate))
    return out


def enclosed_by_rogue_gamerule_gate(body, offset):
    """Whether the source offset is inside an explicit DMGame mode gate."""
    source = span_mask(body)
    stack = []

    for index, char in enumerate(source[:offset]):
        if char == '{':
            boundary = max(source.rfind(';', 0, index),
                           source.rfind('{', 0, index),
                           source.rfind('}', 0, index)) + 1
            opener = source[boundary:index]
            stack.append(bool(re.search(
                r'\bif\s*\([^{}]*\bG_UsesRogueGameRules\s*\(\s*\)'
                r'[^{}]*\)\s*$', opener, re.S)))
        elif char == '}' and stack:
            stack.pop()
    return any(stack)


def rogue_gamerule_violations(tree, override=None):
    """DMGame is a global ruleset protocol, never an OSP side channel."""
    out = []
    filename = 'g_ruleset.c'
    raw = (override or {}).get(filename, read(os.path.join(tree, filename)))
    span = function_span(raw, 'G_UsesRogueGameRules')
    if not span:
        out.append('  !! g_ruleset.c: `G_UsesRogueGameRules` is missing '
                   '(DMGame boundary)')
    else:
        body = strip(raw[span[0]:span[1]])
        for required in ('gamerules && gamerules->value', 'RULESET_CTF',
                         'RULESET_ARENA'):
            if required not in body:
                out.append('  !! g_ruleset.c:%d: `G_UsesRogueGameRules` must '
                           'name `%s` (DMGame boundary)' %
                           (raw[:span[0]].count('\n') + 1, required))

    for filename, function, callback in DMGAME_CALLBACKS:
        raw = (override or {}).get(filename, read(os.path.join(tree, filename)))
        span = function_span(raw, function)
        if not span:
            out.append('  !! %s: `%s` is missing (DMGame boundary)' %
                       (filename, function))
            continue
        body = strip(raw[span[0]:span[1]])
        if callback not in body:
            out.append('  !! %s:%d: `%s` no longer owns `%s` '
                       '(DMGame boundary)' %
                       (filename, raw[:span[0]].count('\n') + 1,
                        function, callback))
        elif 'G_UsesRogueGameRules()' not in body:
            out.append('  !! %s:%d: `%s` reaches `%s` outside '
                       '`G_UsesRogueGameRules` (DMGame boundary)' %
                       (filename, raw[:span[0]].count('\n') + 1,
                        function, callback))
        else:
            source = span_mask(body)
            for occurrence in re.finditer(re.escape(callback) + r'\s*\(', source):
                before = source[:occurrence.start()]
                if filename == 'rogue/g_newdm.c':
                    guarded = bool(re.search(
                        r'\bif\s*\(\s*!\s*G_UsesRogueGameRules\s*\(\s*\)\s*\)'
                        r'\s*return\s*;', before, re.S))
                else:
                    guarded = enclosed_by_rogue_gamerule_gate(
                        body, occurrence.start())
                if not guarded:
                    out.append('  !! %s:%d: `%s` reaches `%s` outside the '
                               'lexical `G_UsesRogueGameRules` gate '
                               '(DMGame boundary)' %
                               (filename, raw[:span[0]].count('\n') + 1,
                                function, callback))
                    break
    return out


def osp_hook_violations(tree, override=None):
    """Keep OSP's live hook authority coherent across its config lifecycle."""
    out = []
    int_hook_value = re.compile(
        r'\(\s*int\s*\)\s*hook_enable\s*->\s*value')

    requirements = (
        ('g_ruleset.c', 'G_ModifierEnabled',
         ('hook_enable && (int)hook_enable->value != 0',)),
        ('g_ruleset.c', 'G_ApplyOspHookRequest',
         ('if (G_IsOspRuleset() && hook_enable)',
          'if (request->value && !(int)hook_enable->value)',
          'g_modifier[MOD_HOOK] = (int)hook_enable->value != 0;')),
        ('g_ruleset.c', 'G_QueueOspHookRequest',
         ('if (G_IsOspRuleset())', 'g_osp_hook_request_queued = true;',)),
        ('g_ruleset.c', 'G_ApplyQueuedOspHookRequest',
         ('g_osp_hook_request_queued = false;',
          'G_ApplyOspHookRequest();')),
        ('g_ruleset.c', 'G_InitRuleset',
         ('g_osp_hook_request_queued = false;',)),
        ('bot/bl_main.c', 'BotTourneyHook',
         ('G_IsOspRuleset() && hook_enable',
          '(int)hook_enable->value != 0',)),
        ('tourney/osp_cmds.c', 'OSP_config_vote',
         ('G_QueueOspHookRequest();',)),
        ('tourney/osp_main.c', 'OSP_endClean',
         ('G_ApplyOspHookRequest();', 'OSP_setFeatures();')),
        ('tourney/osp_main.c', 'OSP_exitLevel',
         ('G_QueueOspHookRequest();',)),
        ('g_spawn.c', 'SpawnEntities',
         ('G_ApplyQueuedOspHookRequest()', 'OSP_setFeatures();')),
    )

    for filename, function, required in requirements:
        path = os.path.join(tree, filename)
        raw = (override or {}).get(os.path.basename(filename), read(path))
        span = function_span(raw, function)
        if not span:
            out.append('  !! %s: `%s` is missing (OSP hook lifecycle)' %
                       (filename, function))
            continue
        body = raw[span[0]:span[1]]
        for text in required:
            if text not in body:
                out.append('  !! %s:%d: `%s` must retain `%s` '
                           '(OSP hook lifecycle)' %
                           (filename, raw[:span[0]].count('\n') + 1,
                            function, text))

    ordered = (
        ('g_main.c', 'InitGame', 'G_ResolveModifiers();', 'OSP_setFeatures();'),
        ('tourney/osp_cmds.c', 'OSP_config_vote', 'G_QueueOspHookRequest();',
         'gi.AddCommandString(cmd);'),
        # The config name is quoted; the literal is the anchor, so it moves
        # with the code it anchors to.
        ('tourney/osp_main.c', 'OSP_exitLevel', 'G_QueueOspHookRequest();',
         'gi.AddCommandString(va("exec \\"%s\\"\\n", vote_config_defaultname->string));'),
        ('g_spawn.c', 'SpawnEntities', 'G_ApplyQueuedOspHookRequest()',
         'OSP_setFeatures();'),
    )
    for filename, function, first, second in ordered:
        raw = (override or {}).get(
            os.path.basename(filename), read(os.path.join(tree, filename)))
        span = function_span(raw, function)
        if not span:
            continue
        body = raw[span[0]:span[1]]
        # A MISSING ANCHOR IS ITS OWN FINDING, not an ordering complaint.
        # `str.find` returns -1, so the old test -- `find(first) >= find(second)`
        # -- reported "must apply X before Y" when Y was simply not there any
        # more, which sends the reader looking for a reordering that never
        # happened (it cost one build cycle when the `exec` was quoted
        # argument).  Worse in the other direction: a MISSING `first` made the
        # comparison -1 >= n, which is false, so the rule passed with the hook
        # request gone.  `requirements` above covers presence for the two OSP
        # pairs and for SpawnEntities, but not for InitGame's -- so the test
        # states both halves rather than relying on another rule to hold.
        a, b = body.find(first), body.find(second)
        if a < 0 or b < 0:
            out.append('  !! %s:%d: `%s` no longer contains `%s` -- the '
                       'ordering rule has lost an anchor and cannot answer '
                       '(OSP hook lifecycle)' %
                       (filename, raw[:span[0]].count('\n') + 1, function,
                        first if a < 0 else second))
        elif a >= b:
            out.append('  !! %s:%d: `%s` must apply `%s` before `%s` '
                       '(OSP hook lifecycle)' %
                       (filename, raw[:span[0]].count('\n') + 1,
                        function, first, second))

    hook_sources = [os.path.join(tree, 'g_ruleset.c'),
                    os.path.join(tree, 'bot', 'bl_main.c')]
    hook_sources.extend(sorted(glob.glob(os.path.join(tree, 'tourney', '*.c'))))
    for path in hook_sources:
        filename = os.path.basename(path)
        raw = (override or {}).get(filename, read(path))
        for line_number, line in enumerate(strip(raw).splitlines(), 1):
            if 'hook_enable->value' in line and not int_hook_value.search(line):
                out.append('  !! %s:%d: `hook_enable` must use OSP integer '
                           'semantics (OSP hook lifecycle)' %
                           (os.path.relpath(path, tree), line_number))
    return out


def osp_config_violations(tree, override=None):
    """Keep cvar-derived OSP rune state coherent across config execution."""
    out = []
    requirements = (
        ('tourney/osp_main.c', 'OSP_SyncRuneState',
         ('rune_stat = (int)runes_enable->value;',
          'if (rune_stat > 0x1f)',
          'rune_stat = 0x1f;')),
        ('tourney/osp_main.c', 'OSP_gameInit',
         ('OSP_SyncRuneState();',)),
        ('tourney/osp_main.c', 'OSP_endClean',
         ('OSP_SyncRuneState();',)),
        ('g_ruleset.c', 'G_ResolveModifiers',
         ('gi.cvar_set("runes_enable", "31");',
          'OSP_SyncRuneState();')),
        ('g_spawn.c', 'SpawnEntities',
         ('G_ApplyQueuedOspHookRequest()',
          'OSP_SyncRuneState();',
          'OSP_setFeatures();',
          # The donor's guard is integer (port_osp:g_spawn.c:761) and
          # has to match the cast rune_stat is derived with, or a fractional
          # runes_enable schedules a spawner with no rune type enabled.
          'if (runes_enable && (int)runes_enable->value)',
          'OSP_setupRuneSpawn(0);')),
    )

    for filename, function, required in requirements:
        path = os.path.join(tree, filename)
        raw = (override or {}).get(os.path.basename(filename), read(path))
        span = function_span(raw, function)
        if not span:
            out.append('  !! %s: `%s` is missing (OSP config lifecycle)' %
                       (filename, function))
            continue
        body = raw[span[0]:span[1]]
        for text in required:
            if text not in body:
                out.append('  !! %s:%d: `%s` must retain `%s` '
                           '(OSP config lifecycle)' %
                           (filename, raw[:span[0]].count('\n') + 1,
                            function, text))

    raw = (override or {}).get('g_spawn.c',
                                read(os.path.join(tree, 'g_spawn.c')))
    span = function_span(raw, 'SpawnEntities')
    if span:
        body = raw[span[0]:span[1]]
        queued_sync = re.search(
            r'if\s*\(\s*G_ApplyQueuedOspHookRequest\s*\(\s*\)\s*\)\s*\{\s*'
            r'OSP_SyncRuneState\s*\(\s*\)\s*;\s*'
            r'OSP_setFeatures\s*\(\s*\)\s*;', body, re.S)
        if not queued_sync:
            out.append('  !! g_spawn.c:%d: `SpawnEntities` must synchronize '
                       'rune state only in the queued configuration transition '
                       '(OSP config lifecycle)' %
                       (raw[:span[0]].count('\n') + 1))
        else:
            setup = body.find('OSP_setupRuneSpawn(0);')
            if setup < queued_sync.end():
                out.append('  !! g_spawn.c:%d: `SpawnEntities` must synchronize '
                           'rune state and features before scheduling runes '
                           '(OSP config lifecycle)' %
                           (raw[:span[0]].count('\n') + 1))
    return out


def donor_surface(tree, donor):
    """Names the donor's headers declare and the shared tree does not own."""
    names = set()
    for h in sorted(glob.glob(os.path.join(tree, donor, '*.h'))):
        for m in DECL.finditer(strip(read(h))):
            if len(m.group(1)) >= 4:
                names.add(m.group(1))
    # Anything g_local.h also declares, or a spine .c defines, is shared.  So is
    # every MEMBER of a struct g_local.h defines: `max_health` is baseq2's field
    # on edict_t, and a donor header that names it in a prototype does not make
    # it the donor's.  Without this the tool reports 111 findings, all of them
    # baseq2 reading its own fields.
    shared = set()
    local = strip(read(os.path.join(tree, 'g_local.h')))
    for m in DECL.finditer(local):
        shared.add(m.group(1))
    for m in re.finditer(r'^\s+[A-Za-z_][\w \t*]*?\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*;',
                         local, re.M):
        shared.add(m.group(1))
    for c in sorted(glob.glob(os.path.join(tree, '*.c'))):
        body = strip(read(c))
        # functions the spine defines
        for m in re.finditer(r'^[A-Za-z_][\w \t*]*?\b([A-Za-z_]\w*)\s*\([^;]*$', body, re.M):
            shared.add(m.group(1))
        # and OBJECTS it defines.  baseq2's three `static const gitem_armor_t`
        # tables are the case: tourney declares them extern and non-const
        # because it rewrites them from cvars, which makes them a merge decision
        # in g_items.c -- not a donor-private symbol leaking into the spine.
        for m in re.finditer(r'^(?:static\s+)?(?:const\s+)?[A-Za-z_][\w \t*]*?'
                             r'\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*=', body, re.M):
            shared.add(m.group(1))
        # ...including TENTATIVE definitions, which have no initialiser.  Once a
        # spine file defines `int match_paused;` the symbol is the tree's, and
        # the line that defines it is not a read under the wrong ruleset.
        for m in re.finditer(r'^(?:static\s+)?(?:const\s+)?[A-Za-z_][\w \t*]+'
                             r'\b([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*;', body, re.M):
            shared.add(m.group(1))
    return names - shared


def group(lines):
    """line index -> (first, last) of the statement it belongs to.

    Grouped FORWARD by parenthesis balance rather than guessed backwards: a
    condition wrapped over three lines is one test, and the ruleset half of it is
    usually on the first of them, so a per-line check reports the second and
    third as ungated.  Backward guessing gets the common shapes and misses a
    ternary whose branches each balance on their own -- which is exactly the
    shape p_view.c's falling damage has.
    """
    out, i, n = {}, 0, len(lines)
    while i < n:
        bal, j = 0, i
        while j < n:
            bal += lines[j].count('(') - lines[j].count(')')
            if bal <= 0:
                break
            j += 1
        for k in range(i, min(j, n - 1) + 1):
            out[k] = (i, min(j, n - 1))
        i = min(j, n - 1) + 1
    return out


COND = re.compile(r'^\s*(?:\}\s*)?(?:else\s+)?if\s*\(.*\)\s*$')
CASE = re.compile(r'^\s*(?:case\s+\w+|default)\s*:')

# `} else if (G_Ruleset() == RULESET_TOURNEY) {` opens the block its body is in,
# but it closes one too, so the outward walk's brace arithmetic nets to zero and
# sails straight past it -- the gate is right there on the line and the tool
# reports the body as ungated.  An else-arm opener is recognised explicitly.
ELSEARM = re.compile(r'^\s*\}\s*else\b.*\{\s*$')


def gate_lines(lines, groups, ruleset, i):
    """True if line i is inside a gate for `ruleset`."""
    # RULESET_OSP is the family, not an enum value, so it contributes no
    # literal name of its own -- everything that gates it is in PREDICATES.
    pats = [] if ruleset == 'RULESET_OSP' else \
        [r'RULESET_' + ruleset.split('_', 1)[1]]
    for p in PREDICATES.get(ruleset, ()):
        pats.append(re.escape(p))
    rx = re.compile('|'.join(pats))

    def stmt(k):
        a, b = groups.get(k, (k, k))
        return ' '.join(lines[a:b + 1])

    start = groups.get(i, (i, i))[0]
    if rx.search(stmt(i)):
        return True

    # A braceless `if (...)` above this statement, skipping blank lines and the
    # comment lines strip() has emptied.
    k = start - 1
    while k >= 0 and not lines[k].strip():
        k -= 1
    if k >= 0:
        cond = stmt(k)
        if COND.match(cond.strip()) and rx.search(cond):
            return True

    # Walk outward through enclosing blocks, and check the `case` arm we are in.
    depth, seen_case = 0, False
    j = start - 1
    while j >= 0:
        line = lines[j]
        if depth == 0 and ELSEARM.match(line) and rx.search(stmt(j)):
            return True
        if depth == 0 and not seen_case and CASE.match(line):
            seen_case = True
            if rx.search(line):
                return True
        depth += line.count('}') - line.count('{')
        if depth < 0:
            if rx.search(stmt(j)):
                return True
            depth, seen_case = 0, False
        j -= 1
    return False


def run(tree, override=None):
    out, bad = [], 0
    checked = 0
    for donor, ruleset in DONORS.items():
        if not os.path.isdir(os.path.join(tree, donor)):
            continue
        surface = donor_surface(tree, donor) | set(FIELDS.get(ruleset, ()))
        rx = re.compile(r'\b(%s)\b' % '|'.join(sorted(re.escape(s) for s in surface)))
        for f in sorted(glob.glob(os.path.join(tree, '*.c')) +
                        glob.glob(os.path.join(tree, '*.h'))):
            name = os.path.basename(f)
            if name in EXEMPT_FILES:
                continue
            raw = (override or {}).get(name) or read(f)
            lines = strip(raw).split('\n')
            groups = group(lines)
            for i, line in enumerate(lines):
                m = rx.search(line)
                if not m or DESCRIPTOR.match(line) or INITIALISER.match(line):
                    continue
                checked += 1
                if not gate_lines(lines, groups, ruleset, i):
                    out.append('  !! %s:%d: `%s` is %s\'s and is not inside a '
                               '%s gate: %s'
                               % (name, i + 1, m.group(1), donor, ruleset,
                                  line.strip()[:60]))
                    bad += 1
    # The stray-extern shape, which is not a gate question but is the same input: a
    # donor's own object re-declared `extern` somewhere other than the header
    # that defines it.  The bug it is named for is `extern int botglobals;` in
    # the donor's g_spawn.c against a `bot_globals_t botglobals;` elsewhere --
    # the linker resolved a four-byte int over the first member of a struct and
    # zeroed numbits, and nothing warned.  A local extern of a donor object is
    # never right here: every one of them has a header.
    redecl = 0
    for donor in DONORS:
        if not os.path.isdir(os.path.join(tree, donor)):
            continue
        objs = set()
        for h in sorted(glob.glob(os.path.join(tree, donor, '*.h'))):
            for m in re.finditer(r'^extern\s+[A-Za-z_][\w \t*]*?\b(\w+)\s*[;\[]',
                                 strip(read(h)), re.M):
                objs.add(m.group(1))
        if not objs:
            continue
        for f in sorted(glob.glob(os.path.join(tree, '*.c')) +
                        glob.glob(os.path.join(tree, '*.h')) +
                        glob.glob(os.path.join(tree, donor, '*.c'))):
            name = os.path.relpath(f, tree)
            raw = (override or {}).get(os.path.basename(f)) or read(f)
            for i, line in enumerate(strip(raw).split('\n')):
                m = re.match(r'^extern\s+[A-Za-z_][\w \t*]*?\b(\w+)\s*[;\[]', line)
                if m and m.group(1) in objs:
                    out.append('  !! %s:%d: `%s` is %s\'s and is re-declared '
                               'extern outside its own header: %s'
                               % (name, i + 1, m.group(1), donor,
                                  line.strip()[:60]))
                    redecl += 1
    bad += redecl
    exclusive = exclusive_alias_violations(tree, override)
    shared = shared_behavior_violations(tree, override)
    dynamic_flavour = dynamic_flavour_violations(tree, override)
    dmgame = rogue_gamerule_violations(tree, override)
    dmgame += rogue_dmgame_entity_violations(tree, override)
    osp_hook = osp_hook_violations(tree, override)
    osp_config = osp_config_violations(tree, override)
    out.extend(exclusive)
    out.extend(shared)
    out.extend(dynamic_flavour)
    out.extend(dmgame)
    out.extend(osp_hook)
    out.extend(osp_config)
    bad += len(exclusive) + len(shared) + len(dynamic_flavour) + len(dmgame) + \
           len(osp_hook) + len(osp_config)

    out.insert(0, 'donorgate.py: %d donor-surface use(s) in spine files; '
                  '%d ungated; %d stray extern(s); %d exclusive-alias; '
                  '%d shared-behavior; %d dynamic-flavor; %d DMGame-boundary; %d OSP-hook '
                  'and %d OSP-config violation(s)' %
                  (checked, bad - redecl - len(exclusive) - len(shared) -
                   len(dynamic_flavour) - len(dmgame) - len(osp_hook) -
                   len(osp_config),
                   redecl, len(exclusive), len(shared), len(dynamic_flavour),
                   len(dmgame), len(osp_hook), len(osp_config)))
    if not bad:
        out.append('  every donor-private field and function in a shared file '
                   'is inside its own ruleset\'s gate; every pack-exclusive '
                   'alias selects its own behavior')
    return out


# Each control removes a real gate this merge added.
SELFTESTS = [
    ('landing sound', 'g_phys.c',
     ('if (G_Ruleset() != RULESET_ARENA || !ent->client ||\n'
      '            ent->client->resp.fightstate == FIGHT_ALIVE)',
      'if (!ent->client || ent->client->resp.fightstate == FIGHT_ALIVE)')),
    ('killbox', 'g_utils.c',
     ('    if (G_Ruleset() == RULESET_ARENA) {\n'
      '        // RA2 telefrags only between fighting players.',
      '    if (1) {\n'
      '        // RA2 telefrags only between fighting players.')),
    # The donor's own bug, in its shape: a local `extern` of a donor object where
    # the donor's own header already declares it.  `m_mode` is tourney's match
    # mode and an `extern int` of it in a shared file is exactly what
    # `extern int botglobals;` was.
    ('stray extern', 'g_spawn.c',
     ('#include "tourney/osp_hooks.h"',
      '#include "tourney/osp_hooks.h"\nextern int sync_stat;')),
    # An else-arm gate is a gate.  Removing the ruleset test from the `} else
    # if (...) {` that opens the block must be reported -- before this control
    # existed the tool could not see that line at all.
    ('else-arm gate', 'g_items.c',
     ('} else if (G_IsOspRuleset()) {',
      '} else if (master->item) {')),
    # The initialiser exemption must not swallow a real call.  `.pickup =`
    # rows are data; `OSP_Pickup_Rune(ent, other);` in a function body is not,
    # and turning one into the other has to be reported.
    ('rune pickup call', 'g_items.c',
     ('    taken = ent->item->pickup(ent, other);',
      '    taken = OSP_Pickup_Rune(ent, other);')),
    ('weapon think', 'p_weapon.c',
     ('if (G_Ruleset() == RULESET_ARENA && ent->client &&\n'
      '        ent->client->resp.fightstate != FIGHT_ALIVE)',
      'if (ent->client &&\n'
      '        ent->client->resp.fightstate != FIGHT_ALIVE)')),
]

IDENTITY_SELFTESTS = [
    ('Medic Commander identity', 'g_monster.c',
     '"monster_medic_commander"', '"monster_missing_commander"'),
    ('Daedalus identity', 'g_monster.c',
     '"monster_daedalus"', '"monster_missing_daedalus"'),
    ('Rogue identity overmatch', 'g_monster.c',
     '"monster_kamikaze");',
     '"monster_kamikaze") || !strcmp(ent->classname, "monster_not_rogue");'),
    ('Medic generic selector', 'm_medic.c',
     'return M_UsesRogueBehavior(self);', 'return false;'),
    ('Daedalus generic selector', 'm_hover.c',
     'return M_UsesRogueBehavior(self);', 'return false;'),
    ('Kamikaze guard order', 'm_flyer.c',
     ('void flyer_attack(edict_t *self)\n'
      '{\n'
      '    if (self->mass > 50) {'),
     ('void flyer_attack(edict_t *self)\n'
      '{\n'
      '    if (self->mass > 500) {')),
]

SHARED_BEHAVIOR_SELFTESTS = [
    ('Rogue sight cache', 'g_ai.c',
     'level.rogue_sight_client = NextSightClient',
     'level.rogue_sight_client = NULL; //'),
    ('Rogue sight cache serialization', 'g_save.c',
     'E(rogue_sight_client)', 'E(sight_client)'),
    ('Rogue sight cache save version', 'g_save.c',
     '#define SAVE_VERSION    0x101', '#define SAVE_VERSION    0x100'),
    ('base idle interval', 'g_ai.c',
     '(1 + random()) * 15', '(1 + random() * 15)'),
    ('base checkattack arm', 'g_ai.c',
     'return bq2_ai_checkattack(self, dist);', 'return false;'),
    ('base ai_charge stale Tesla guard', 'g_ai.c',
     ('    // Tesla targeting is relationship state shared by both behavior arms.\n'
      '    if (!self->enemy || !self->enemy->inuse)'),
     ('    // Tesla targeting is relationship state shared by both behavior arms.\n'
      '    if (false)')),
    # The mutation is port_rogue's literal line, because that is the arm
    # rejected and the one a future edit would reach for.
    ('base ai_turn yaw', 'g_ai.c',
     ('    if (FindTarget(self))\n'
      '        return;\n'
      '\n'
      '    M_ChangeYaw(self);\n'
      '}\n'),
     ('    if (FindTarget(self))\n'
      '        return;\n'
      '\n'
      '    if (!(self->monsterinfo.aiflags & AI_MANUAL_STEERING))\n'
      '        M_ChangeYaw(self);\n'
      '}\n')),
    # ...and the runtime-selector shape stays covered too.
    ('base ai_turn yaw selector', 'g_ai.c',
     ('    if (FindTarget(self))\n'
      '        return;\n'
      '\n'
      '    M_ChangeYaw(self);\n'
      '}\n'),
     ('    if (FindTarget(self))\n'
      '        return;\n'
      '\n'
      '    if (M_UsesRogueBehavior(self))\n'
      '        M_ChangeYaw(self);\n'
      '}\n')),
]

SHARED_BEHAVIOR_SELFTESTS += [
    # The mutation is the base donor's literal bare deref -- the arm
    # the split had restored -- not a deletion of the whole branch.
    ('base noise owner NULL guard', 'g_ai.c',
     'if (client->owner && (client->owner->flags & FL_NOTARGET))',
     'if (client->owner->flags & FL_NOTARGET)'),
    ('Makron rogue sight cache', 'm_boss32.c',
     'M_UsesRogueBehavior(self) ? level.rogue_sight_client',
     'false ? level.rogue_sight_client'),
]

DYNAMIC_FLAVOUR_SELFTESTS = [
    ('Jorg Makron flavor inheritance', 'm_boss32.c',
     'ent->content_flavour = self->content_flavour;',
     'ent->content_flavour = 0;'),
]

DMGAME_SELFTESTS = [
    ('DMGame effects mode gate', 'p_view.c',
     'if (G_UsesRogueGameRules()) {',
     'if (gamerules && gamerules->value) {'),
    # Each mutation is the donor's own raw comparison, which is what
    # the entry points read before the predicate existed.
    ('Tag token entity gate', 'rogue/dm_tag.c',
     '    if (!Tag_Active()) {\n        G_FreeEdict(self);',
     '    if (gamerules && (gamerules->value != 2)) {\n        G_FreeEdict(self);'),
    ('Tag token pickup gate', 'rogue/dm_tag.c',
     '    if (!Tag_Active())\n        return false;',
     '    if (gamerules && (gamerules->value != 2))\n        return false;'),
    ('Deathball entity gate', 'rogue/dm_ball.c',
     '    if (!DBall_Active()) {',
     '    if (gamerules && (gamerules->value != RDM_DEATHBALL)) {'),
]

OSP_HOOK_SELFTESTS = [
    ('OSP hook integer predicate', 'bot/bl_main.c',
     '(int)hook_enable->value != 0', 'hook_enable->value != 0'),
    ('OSP config reapplication', 'tourney/osp_cmds.c',
     'G_QueueOspHookRequest();', '/* no queued hook request */'),
    ('OSP feature resolution order', 'g_main.c',
     ('G_ResolveModifiers();\n'
      '    if (G_IsOspRuleset())\n'
      '        OSP_setFeatures();'),
     ('if (G_IsOspRuleset())\n'
      '        OSP_setFeatures();\n'
      '    G_ResolveModifiers();')),
]

OSP_CONFIG_SELFTESTS = [
    ('OSP rune spawn integer guard', 'g_spawn.c',
     'if (runes_enable && (int)runes_enable->value)',
     'if (runes_enable && runes_enable->value)'),
    ('OSP config rune synchronization', 'g_spawn.c',
     ('        if (G_ApplyQueuedOspHookRequest()) {\n'
      '            OSP_SyncRuneState();\n'
      '            OSP_setFeatures();\n'
      '        }'),
     ('        if (G_ApplyQueuedOspHookRequest()) {\n'
      '            OSP_setFeatures();\n'
      '        }')),
    ('OSP rune synchronization mask', 'tourney/osp_main.c',
     'if (rune_stat > 0x1f)',
     'if (rune_stat > 0x3f)'),
]


def selftest(tree):
    bad = 0
    for name, fname, (orig, rev) in SELFTESTS:
        text = read(os.path.join(tree, fname))
        if orig not in text:
            print('  !! control "%s": its own mutation no longer applies -- '
                  'the control has rotted' % name)
            bad += 1
            continue
        lines = run(tree, {fname: text.replace(orig, rev, 1)})
        if any('!!' in l and fname in l for l in lines):
            print('  ok  control "%s" fires: an ungated use in %s' % (name, fname))
        else:
            print('  !! control "%s" did NOT fire; the check is asleep' % name)
            bad += 1
    for name, fname, orig, rev in IDENTITY_SELFTESTS:
        text = read(os.path.join(tree, fname))
        if orig not in text:
            print('  !! control "%s": its own mutation no longer applies -- '
                  'the control has rotted' % name)
            bad += 1
            continue
        lines = run(tree, {fname: text.replace(orig, rev, 1)})
        if any('exclusive identity' in line or 'Kamikaze ordering' in line or
               'effective Rogue behavior' in line or 'shared behavior' in line or
               'sight-cache serialization' in line for line in lines):
            print('  ok  control "%s" fires: exclusive alias behavior changes '
                  'in %s' % (name, fname))
        else:
            print('  !! control "%s" did NOT fire; the check is asleep' % name)
            bad += 1
    for name, fname, orig, rev in SHARED_BEHAVIOR_SELFTESTS:
        text = read(os.path.join(tree, fname))
        if orig not in text:
            print('  !! control "%s": its own mutation no longer applies -- '
                  'the control has rotted' % name)
            bad += 1
            continue
        lines = run(tree, {fname: text.replace(orig, rev, 1)})
        if any('shared behavior' in line or
               'sight-cache serialization' in line or
               'shared yaw owner' in line or 'shared Tesla state' in line or
               'shared noise owner' in line for line in lines):
            print('  ok  control "%s" fires: shared donor arm changes in %s' %
                  (name, fname))
        else:
            print('  !! control "%s" did NOT fire; the check is asleep' % name)
            bad += 1
    for name, fname, orig, rev in DYNAMIC_FLAVOUR_SELFTESTS:
        text = read(os.path.join(tree, fname))
        if orig not in text:
            print('  !! control "%s": its own mutation no longer applies -- '
                  'the control has rotted' % name)
            bad += 1
            continue
        lines = run(tree, {fname: text.replace(orig, rev, 1)})
        if any('dynamic monster flavor' in line for line in lines):
            print('  ok  control "%s" fires: dynamic monster flavor changes '
                  'in %s' % (name, fname))
        else:
            print('  !! control "%s" did NOT fire; the check is asleep' % name)
            bad += 1
    for name, fname, orig, rev in DMGAME_SELFTESTS:
        text = read(os.path.join(tree, fname))
        if orig not in text:
            print('  !! control "%s": its own mutation no longer applies -- '
                  'the control has rotted' % name)
            bad += 1
            continue
        lines = run(tree, {fname: text.replace(orig, rev, 1)})
        if any('DMGame boundary' in line or
               'Rogue DM entity' in line for line in lines):
            print('  ok  control "%s" fires: DMGame escapes its mode gate in %s' %
                  (name, fname))
        else:
            print('  !! control "%s" did NOT fire; the check is asleep' % name)
            bad += 1
    for name, fname, orig, rev in OSP_HOOK_SELFTESTS:
        text = read(os.path.join(tree, fname))
        if orig not in text:
            print('  !! control "%s": its own mutation no longer applies -- '
                  'the control has rotted' % name)
            bad += 1
            continue
        lines = run(tree, {os.path.basename(fname): text.replace(orig, rev, 1)})
        if any('OSP hook lifecycle' in line for line in lines):
            print('  ok  control "%s" fires: OSP hook lifecycle changes in %s' %
                  (name, fname))
        else:
            print('  !! control "%s" did NOT fire; the check is asleep' % name)
            bad += 1
    for name, fname, orig, rev in OSP_CONFIG_SELFTESTS:
        text = read(os.path.join(tree, fname))
        if orig not in text:
            print('  !! control "%s": its own mutation no longer applies -- '
                  'the control has rotted' % name)
            bad += 1
            continue
        lines = run(tree, {os.path.basename(fname): text.replace(orig, rev, 1)})
        if any('OSP config lifecycle' in line for line in lines):
            print('  ok  control "%s" fires: OSP config lifecycle changes in %s' %
                  (name, fname))
        else:
            print('  !! control "%s" did NOT fire; the check is asleep' % name)
            bad += 1
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tree', default=os.path.join(REPO, 'src'))
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    tree = os.path.abspath(a.tree)

    if a.selftest:
        return 1 if selftest(tree) else 0

    lines = run(tree)
    for l in lines:
        print(l)
    return 1 if any('!!' in l for l in lines) else 0


if __name__ == '__main__':
    sys.exit(main())
