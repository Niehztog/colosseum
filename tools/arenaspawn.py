#!/usr/bin/env python3
"""Guard RA2 spawn-pad and arena-bound safety (R-146).

Arena spawn selection deliberately ranks candidates only by fighters in the
requested arena.  A second predicate must nevertheless keep a live,
collidable body already on a same-arena pad from being selected.  This source
audit checks that both selectors retain that predicate before their ranking,
and that their first-candidate saturated-map fallback remains intact.

Arena 0 is the lobby and 1..MAX_ARENAS are playable.  The second half checks
that storage includes ID 32, worldspawn counts fall back safely, external
arena ingress cannot index a malformed arena or team number, stale team-menu
rows do not transfer menu-owned text into replacement teams, and malformed
arena.cfg input fails rather than becoming partial configuration.

USAGE
    tools/arenaspawn.py [--tree src]
    tools/arenaspawn.py --selftest [--tree src]
"""
import argparse
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLEARANCE = 'ARENA_SPAWN_CLEARANCE'
HELPER = 'ArenaSpawnSpotClear'


def mask(text):
    """Blank comments and quoted literals while preserving offsets."""
    out = list(text)
    i = 0
    while i < len(text):
        if text.startswith('//', i):
            end = text.find('\n', i)
            end = len(text) if end < 0 else end
            out[i:end] = ' ' * (end - i)
            i = end
        elif text.startswith('/*', i):
            end = text.find('*/', i + 2)
            end = len(text) - 2 if end < 0 else end
            for n in range(i, end + 2):
                if text[n] != '\n':
                    out[n] = ' '
            i = end + 2
        elif text[i] in '"\'':
            quote = text[i]
            i += 1
            while i < len(text) and text[i] != quote:
                if text[i] == '\\':
                    out[i] = ' '
                    i += 1
                    if i == len(text):
                        break
                if text[i] != '\n':
                    out[i] = ' '
                i += 1
            i += 1
        else:
            i += 1
    return ''.join(out)


def function_span(text, name):
    """Return the definition span for one C function, with brace matching."""
    masked = mask(text)
    for match in re.finditer(r'\b%s\s*\(' % re.escape(name), masked):
        brace = masked.find('{', match.end())
        semicolon = masked.find(';', match.end())
        if brace < 0 or (semicolon >= 0 and semicolon < brace):
            continue
        depth = 0
        for end in range(brace, len(masked)):
            if masked[end] == '{':
                depth += 1
            elif masked[end] == '}':
                depth -= 1
                if depth == 0:
                    return match.start(), end + 1
    raise ValueError('cannot find definition of %s' % name)


def function(text, name):
    start, end = function_span(text, name)
    return mask(text[start:end])


def require(hits, condition, message):
    if not condition:
        hits.append(message)


def gate_position(body, name):
    gate = re.search(
        r'if\s*\(\s*!%s\s*\(\s*spot\s*,\s*arenanum\s*,\s*ignore\s*\)\s*\)'
        r'\s*continue\s*;' % HELPER, body)
    if not gate:
        return -1
    score = re.search(r'ArenaFightersRangeFromSpot\s*\(', body)
    if not score or gate.start() > score.start():
        return -1
    return gate.start()


def check(text):
    return check_all(text, None, None, None)


def check_all(text, header, bot, misc, maploop=None, menus=None):
    hits = []
    try:
        helper = function(text, HELPER)
        random = function(text, 'SelectRandomArenaSpawnPoint')
        farthest = function(text, 'SelectFarthestArenaSpawnPoint')
    except ValueError as exc:
        return [str(exc)]

    compact = re.sub(r'\s+', ' ', helper)
    require(hits, re.search(r'#define\s+%s\s+50(?:\.0f?)?' % CLEARANCE, mask(text)),
            '%s must retain the 50-unit clearance' % CLEARANCE)
    require(hits, re.search(r'!ArenaLiveBody\s*\(\s*player\s*,\s*ignore\s*\)', compact),
            '%s must exclude the incoming entity and non-live clients' % HELPER)
    require(hits, re.search(r'player\s*->\s*solid\s*==\s*SOLID_NOT', compact),
            '%s must skip non-collidable clients' % HELPER)
    require(hits, re.search(
        r'player\s*->\s*client\s*->\s*resp\s*\.\s*context\s*!=\s*arenanum'
        r'\s*&&\s*idmap\s*==\s*false', compact),
            '%s must remain scoped to the requested arena' % HELPER)
    require(hits, re.search(
        r'VectorLength\s*\(\s*v\s*\)\s*<=\s*%s' % CLEARANCE, compact),
            '%s must reject bodies within its clearance' % HELPER)

    for name, body in (('SelectRandomArenaSpawnPoint', random),
                       ('SelectFarthestArenaSpawnPoint', farthest)):
        require(hits, gate_position(body, name) >= 0,
                '%s must reject an occupied pad before fighter ranking' % name)

    require(hits, re.search(
        r'ArenaFightersRangeFromSpot\s*\(\s*spot\s*,\s*arenanum\s*,\s*ignore\s*\)'
        r'\s*>\s*%s' % CLEARANCE, random),
            'SelectRandomArenaSpawnPoint must retain fighter-only ranking')
    require(hits, re.search(
        r'bestplayerdistance\s*=\s*ArenaFightersRangeFromSpot\s*\(\s*spot\s*,'
        r'\s*arenanum\s*,\s*ignore\s*\)', farthest),
            'SelectFarthestArenaSpawnPoint must retain fighter-only ranking')

    first = random.find('if (!first)')
    gate = gate_position(random, 'SelectRandomArenaSpawnPoint')
    clear = random.find('if (!clear)')
    require(hits, first >= 0 and first < gate and
            clear > gate and 'return clear ? clear : first;' in random,
            'SelectRandomArenaSpawnPoint must collide only on a saturated map')
    require(hits, 'return SelectRandomArenaSpawnPoint(' in farthest,
            'SelectFarthestArenaSpawnPoint must retain its saturated-map fallback')

    if header is None:
        return hits

    try:
        grants = function(text, 'RA_ArenaGrantsMask')
        add = function(text, 'AddtoArena')
        init = function(text, 'arena_init')
        teleport = function(misc, 'teleporter_touch')
        raw_token = function(maploop, 'read_raw_token')
        add_value = function(maploop, 'add_val')
        new_definition = function(maploop, 'new_def_item_checked')
        finish_block = function(maploop, 'finish_read_block')
        read_block = function(maploop, 'read_block')
        load_config = function(maploop, 'load_config')
        add_team = function(menus, 'menuAddtoTeam')
    except ValueError as exc:
        return hits + [str(exc)]

    compact_grants = re.sub(r'\s+', ' ', grants)
    compact_add = re.sub(r'\s+', ' ', add)
    compact_init = re.sub(r'\s+', ' ', init)
    compact_bot = re.sub(r'\s+', ' ', mask(bot))
    compact_teleport = re.sub(r'\s+', ' ', teleport)
    compact_raw_token = re.sub(r'\s+', ' ', raw_token)
    compact_add_value = re.sub(r'\s+', ' ', add_value)
    compact_new_definition = re.sub(r'\s+', ' ', new_definition)
    compact_finish_block = re.sub(r'\s+', ' ', finish_block)
    compact_read_block = re.sub(r'\s+', ' ', read_block)
    compact_load_config = re.sub(r'\s+', ' ', load_config)
    compact_add_team = re.sub(r'\s+', ' ', add_team)
    token_overflow = compact_raw_token.find('if (len + 1 >= out_size)')
    token_reject = compact_raw_token.find('return -1;', token_overflow)
    token_write = compact_raw_token.find('out[len++] = c;', token_overflow)

    require(hits, re.search(
        r'\barena_t\s+arenas\s*\[\s*MAX_ARENAS\s*\+\s*1\s*\]\s*;', mask(text)),
            'arena storage must provide lobby plus IDs 1..MAX_ARENAS')
    require(hits, re.search(
        r'\bextern\s+arena_t\s+arenas\s*\[\s*MAX_ARENAS\s*\+\s*1\s*\]\s*;',
        mask(header)),
            'arena storage declaration must provide lobby plus IDs 1..MAX_ARENAS')

    grants_guard = compact_grants.find('if (arenanum < 1 ||')
    grants_index = compact_grants.find('arenas[arenanum]')
    require(hits, grants_guard >= 0 and grants_guard < grants_index and
            'arenanum > num_arenas' in compact_grants and
            'arenanum > MAX_ARENAS' in compact_grants and
            'arenanum >= MAX_ARENAS' not in compact_grants,
            'RA_ArenaGrantsMask must honor configured IDs 1..MAX_ARENAS before indexing')

    require(hits, re.search(
        r'num_arenas\s*=\s*wsent->arena\s*;\s*if\s*\(\s*num_arenas\s*<\s*0\s*\|\|'
        r'\s*num_arenas\s*>\s*MAX_ARENAS\s*\).*?num_arenas\s*=\s*1\s*;.*?'
        r'idmap\s*=\s*true\s*;',
        compact_init),
            'arena_init must reject invalid worldspawn counts before setup')
    require(hits, re.search(
        r'else\s+if\s*\(\s*!num_arenas\s*\).*?num_arenas\s*=\s*1\s*;.*?'
        r'idmap\s*=\s*true\s*;',
        compact_init),
            'arena_init must map a zero worldspawn count to one id arena')

    add_guard = compact_add.find('if (arenanum < 1 ||')
    add_arena_index = compact_add.find('arenas[arenanum]')
    team_guard = compact_add.find('teamnum < 0 || teamnum >= MAX_TEAMS')
    team_index = compact_add.find('teams[teamnum]')
    require(hits, '!ent || !ent->client || !teams' in compact_add and
            add_guard >= 0 and add_guard < add_arena_index and
            'arenanum > num_arenas' in compact_add and
            'arenanum > MAX_ARENAS' in compact_add and
            team_guard >= 0 and team_guard < team_index and
            '!team || team->teamnum != teamnum' in compact_add,
            'AddtoArena must validate client, live team and arena bounds before indexing')

    require(hits, re.search(
        r'arena\s*<\s*0\s*\|\|\s*arena\s*>\s*num_arenas\s*\|\|\s*arena\s*>\s*'
        r'MAX_ARENAS', compact_bot) and
            'arena >= MAX_ARENAS' not in compact_bot,
            'bot arena userinfo must retain 0 and accept ID MAX_ARENAS')
    require(hits, re.search(
        r'self->arena\s*>\s*num_arenas\s*\|\|\s*self->arena\s*>\s*MAX_ARENAS',
        compact_teleport),
            'arena teleporter ingress must reject invalid positive arena IDs')
    require(hits, token_overflow >= 0 and token_reject >= token_overflow and
            token_reject < token_write,
            'arena.cfg raw tokens must reject overflow rather than split')
    require(hits, 'if (strlen(dest) + strlen(token) + 1 >= VAL_BLOCK_SIZE)' in
            compact_add_value and 'return false;' in compact_add_value and
            compact_read_block.count('if (!add_val(') == 2,
            'arena.cfg definition values must reject overflow rather than truncate')
    require(hits, 'if (count >= MAX_DEFS)' in compact_new_definition and
            'return false;' in compact_new_definition and
            compact_read_block.count('new_def_item_checked(') == 3,
            'arena.cfg definitions must not exceed their allocated block')
    require(hits, 'depth >= (int)q_countof(stack)' in compact_read_block and
            'return false;' in compact_read_block,
            'arena.cfg nesting must not exceed the parser stack')
    require(hits, 'if (mode || cur)' in compact_finish_block and
            'return false;' in compact_finish_block and
            compact_read_block.count(
                'finish_read_block(depth, mode, cur, count, result)') == 2,
            'arena.cfg EOF paths must reject unfinished definitions')
    require(hits, 'if (!read_config(fp))' in compact_load_config and
            'definition_blocks = NULL;' in compact_load_config and
            'num_definition_blocks = 0;' in compact_load_config,
            'arena.cfg parse failure must clear partial parser state')
    require(hits, re.search(
        r'len\s*=\s*strlen\s*\(\s*menuitem\s*->\s*text\s*\)\s*\+\s*1\s*;'
        r'\s*name\s*=\s*gi\s*\.\s*TagMalloc\s*\(\s*len\s*,\s*TAG_LEVEL\s*\)'
        r'\s*;', compact_add_team) and re.search(
            r'memcpy\s*\(\s*name\s*,\s*menuitem\s*->\s*text\s*,\s*len\s*\)'
            r'\s*;', compact_add_team),
            'menuAddtoTeam must copy its menu-owned team label before joining')
    require(hits, re.search(
        r'team\s*=\s*add_to_team\s*\(\s*ent\s*,\s*name\s*\)\s*;',
        compact_add_team) and not re.search(
            r'add_to_team\s*\(\s*ent\s*,\s*\(\s*\(\s*menuitem_t\s*\*\s*\)'
            r'\s*item\s*->\s*it\s*\)\s*->\s*text\s*\)', compact_add_team),
            'menuAddtoTeam must not pass a menu-owned label to add_to_team')
    require(hits, re.search(
        r'if\s*\(\s*!team\s*\)\s*\{\s*gi\s*\.\s*TagFree\s*\(\s*name\s*\)'
        r'\s*;', compact_add_team) and re.search(
            r'if\s*\(\s*team\s*->\s*name\s*!=\s*name\s*\)\s*'
            r'gi\s*\.\s*TagFree\s*\(\s*name\s*\)\s*;', compact_add_team),
            'menuAddtoTeam must retain its copy only when a new team owns it')
    return hits


def mutate_function(text, name, old, new):
    start, end = function_span(text, name)
    part = text[start:end]
    if old not in part:
        raise ValueError('control cannot find mutation in %s' % name)
    return text[:start] + part.replace(old, new, 1) + text[end:]


def selftest(text, header, bot, misc, maploop, menus):
    failures = check_all(text, header, bot, misc, maploop, menus)
    if failures:
        print('!! control clean source unexpectedly fails: %s' % '; '.join(failures))
        return 1

    mutations = (
        ('non-collidable clients reserve pads', 'ArenaSpawnSpotClear',
         'player->solid == SOLID_NOT', 'false'),
        ('reservations cross arena boundaries', 'ArenaSpawnSpotClear',
         'player->client->resp.context != arenanum && idmap == false', 'false'),
        ('random selector loses occupied-pad gate', 'SelectRandomArenaSpawnPoint',
         'if (!ArenaSpawnSpotClear(spot, arenanum, ignore))', 'if (false)'),
        ('farthest selector loses occupied-pad gate', 'SelectFarthestArenaSpawnPoint',
         'if (!ArenaSpawnSpotClear(spot, arenanum, ignore))', 'if (false)'),
        ('clear fallback is not recorded', 'SelectRandomArenaSpawnPoint',
         'if (!clear)', 'if (false)'),
        ('random selector loses saturation fallback', 'SelectRandomArenaSpawnPoint',
         'return clear ? clear : first;', 'return NULL;'),
    )
    for label, name, old, new in mutations:
        mutated = mutate_function(text, name, old, new)
        if check_all(mutated, header, bot, misc, maploop, menus):
            print('ok  control "%s" fires' % label)
        else:
            print('!! control "%s" did NOT fire' % label)
            return 1

    controls = (
        ('arena storage loses ID 32',
         text.replace('arenas[MAX_ARENAS + 1]', 'arenas[MAX_ARENAS]', 1),
         header, bot, misc, maploop),
        ('arena declaration loses ID 32',
         text, header.replace('arenas[MAX_ARENAS + 1]', 'arenas[MAX_ARENAS]', 1),
         bot, misc, maploop),
        ('world count rejects ID 32',
         mutate_function(text, 'arena_init', 'num_arenas > MAX_ARENAS',
                         'num_arenas >= MAX_ARENAS'),
         header, bot, misc, maploop),
        ('grant mask rejects ID 32',
         mutate_function(text, 'RA_ArenaGrantsMask', 'arenanum > MAX_ARENAS',
                         'arenanum >= MAX_ARENAS'),
         header, bot, misc, maploop),
        ('grant mask accepts an unconfigured arena',
         mutate_function(text, 'RA_ArenaGrantsMask', ' || arenanum > num_arenas', ''),
         header, bot, misc, maploop),
        ('AddtoArena loses its max-ID guard',
         mutate_function(text, 'AddtoArena', ' || arenanum > MAX_ARENAS', ''),
         header, bot, misc, maploop),
        ('AddtoArena loses its team bound',
         mutate_function(text, 'AddtoArena', 'teamnum >= MAX_TEAMS',
                         'teamnum > MAX_TEAMS'),
         header, bot, misc, maploop),
        ('bot userinfo rejects ID 32',
         text, header,
         bot.replace('arena > MAX_ARENAS', 'arena >= MAX_ARENAS', 1), misc, maploop),
        ('teleporter ingress admits an invalid ID',
         text, header, bot,
         mutate_function(misc, 'teleporter_touch', 'self->arena > MAX_ARENAS',
                         'self->arena >= MAX_ARENAS'), maploop),
        ('overlong config token is accepted',
         text, header, bot, misc,
         mutate_function(maploop, 'read_raw_token', 'return -1;', 'return 1;')),
        ('unfinished config definition is accepted',
         text, header, bot, misc,
         mutate_function(maploop, 'finish_read_block', 'mode || cur', 'false')),
        ('overlong config value is truncated',
         text, header, bot, misc,
         mutate_function(maploop, 'add_val', 'return false;', 'return true;')),
        ('config definition block overflows',
         text, header, bot, misc,
         mutate_function(maploop, 'new_def_item_checked', 'count >= MAX_DEFS',
                         'count > MAX_DEFS')),
        ('config nesting overflows the parser stack',
         text, header, bot, misc,
         mutate_function(maploop, 'read_block', 'depth >= (int)q_countof(stack)',
                         'depth > (int)q_countof(stack)')),
        ('config parse failure is ignored',
         text, header, bot, misc,
         mutate_function(maploop, 'load_config', 'if (!read_config(fp))', 'if (false)')),
    )
    for label, arena_text, header_text, bot_text, misc_text, maploop_text in controls:
        if check_all(arena_text, header_text, bot_text, misc_text, maploop_text,
                     menus):
            print('ok  control "%s" fires' % label)
        else:
            print('!! control "%s" did NOT fire' % label)
            return 1

    menu_controls = (
        ('team menu transfers menu-owned text',
         mutate_function(menus, 'menuAddtoTeam',
                         'team = add_to_team(ent, name);',
                         'team = add_to_team(ent, ((menuitem_t *)item->it)->text);')),
        ('team menu frees its replacement name',
         mutate_function(menus, 'menuAddtoTeam',
                         'if (team->name != name)\n        gi.TagFree(name);',
                         'if (team->name == name)\n        gi.TagFree(name);')),
    )
    for label, menu_text in menu_controls:
        if check_all(text, header, bot, misc, maploop, menu_text):
            print('ok  control "%s" fires' % label)
        else:
            print('!! control "%s" did NOT fire' % label)
            return 1
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tree', default=os.path.join(REPO, 'src'))
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    tree = os.path.abspath(a.tree)
    paths = {
        'arena': os.path.join(tree, 'arena', 'arena.c'),
        'header': os.path.join(tree, 'arena', 'arena.h'),
        'bot': os.path.join(tree, 'bot', 'bl_spawn.c'),
        'misc': os.path.join(tree, 'g_misc.c'),
        'maploop': os.path.join(tree, 'arena', 'maploop.c'),
        'menus': os.path.join(tree, 'arena', 'ra2menus.c'),
    }
    try:
        texts = {}
        for name, path in paths.items():
            with open(path, encoding='utf-8') as fh:
                texts[name] = fh.read()
    except OSError as exc:
        print('!! arenaspawn.py: %s' % exc)
        return 1

    if a.selftest:
        return selftest(texts['arena'], texts['header'], texts['bot'], texts['misc'],
                        texts['maploop'], texts['menus'])
    hits = check_all(texts['arena'], texts['header'], texts['bot'], texts['misc'],
                     texts['maploop'], texts['menus'])
    for hit in hits:
        print('!! arenaspawn.py: %s' % hit)
    print('arenaspawn.py: %s' % ('clean' if not hits else '%d finding(s)' % len(hits)))
    return 1 if hits else 0


if __name__ == '__main__':
    sys.exit(main())
