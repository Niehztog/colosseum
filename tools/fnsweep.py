#!/usr/bin/env python3
"""The function-level donor diff: does this tree carry each donor's behaviour?

R-VER-35, doc/donor-fdiff.md, doc/reconciliation.md R-195.

WHY.  `tools/divergence.py` states the merge load per FILE, in diff lines
(R-CORE-10).  A line count cannot answer the question a merge is actually judged
by -- *for each definition a donor touched, is that donor's behaviour here?* --
and every instrument that could was pointed somewhere narrower: `lostref.py`
asks it of the symbols a donor DELETED, `donorgate.py` of whether a donor's
surface is gated, `deadvalue.py` of the values a field can hold.  None of them
walks the donor's own feature set definition by definition.  This does.

METHOD.  R-PROV-3: `git diff baseq2 port_<donor>` in the vendored bundles IS
that donor's feature set, because both sides sit on the same spine tip.  For
every top-level definition a donor added, changed or removed against that spine,
three texts are extracted by brace matching over a comment/string-masked copy --
the spine's, the donor's, and this tree's -- and the tree's is found by NAME
across all of `src/`, so a definition that moved file (`stuffcmd` did) is still
compared against the right body.

    as_donor        the tree's text equals the donor's, modulo comments
    as_spine        the tree kept the BASE version: the donor's change is absent
    merged          neither, so the donor's delta is checked line by line
    absent          no definition of that name in src/ (a rename search runs)
    donor_deleted   the donor removed it; R-CORE-8 does not replay a deletion

THE FINDING, and why it is not "a line is missing".  A missing line is the
NORMAL case here and must not be a finding: R-MODE-5 puts a donor's concept in
the donor's own file and leaves one call at the site, so 1,168 of the donor
lines that are absent from their own site are elsewhere in the tree verbatim.
So every missing line is asked a second question, against the whole of `src/`:

    relocated   the exact line is somewhere else in the tree
    adapted     a >=0.80 match is somewhere else in the tree
    rephrased   every identifier in it exists in the tree, the line does not
    ABSENT      at least one identifier in it exists NOWHERE in the tree

`ABSENT` is the sharp one -- a field nothing reads, a constant nothing names, a
string no player can ever see -- and it is still not a finding on its own,
because a recorded decision removes identifiers by the hundred: 490 lines are
the libc replacements of R-SEC-1, 222 are `m_mode` comparisons that R-OSP-12
deleted with the cvar, 167 are the ngLog stack `osp_stats.c` replaced. CLUSTERS
below attributes each one to the decision that removed it, and **a line no
cluster claims and no EXEMPT row names is the finding**.

WHAT IS NOT A FINDING.  A definition inside a donor file the tree does not carry
at all (`gstats.c`, `q2log.c`, `nglog.c`, `ngmark.c`, `g_monsters.c`, the
container libraries): that is a file-level decision, recorded in
`doc/provenance.md` and R-114, and reporting it per definition would bury the
per-definition question under 500 rows of it.

USAGE
    tools/fnsweep.py                    # the report: matrix, attribution, findings
    tools/fnsweep.py --check            # audit mode: exit 1 on a line nothing explains
    tools/fnsweep.py --selftest         # the controls
    tools/fnsweep.py --json out.json    # every record, for triage
"""
import argparse
import collections
import difflib
import importlib
import json
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(REPO, 'tools')
sys.path.insert(0, TOOLS)

# The bundle refs and the cache builder are divergence.py's, imported rather
# than copied: the two tools must answer about the same donors or the file-level
# and definition-level numbers are not comparable (R-PROV-3, R-PROV-6).
divergence = importlib.import_module('divergence')

# label -> the subfolder(s) that donor's own files landed in (R-CORE-7).  Only a
# tie-break: a tree definition is chosen by body similarity first, because
# `M_walkmove` exists in both `src/m_move.c` and Rogue's `src/rogue/m_move2.c`
# and preferring the donor's own subfolder picked the wrong one and reported
# Rogue's change as missing when it was there.
SUBDIRS = {
    'ctf':    ['src/ctf'],
    'xatrix': ['src/xatrix'],
    'rogue':  ['src/rogue'],
    'ra2':    ['src/arena'],
    'osp':    ['src/tourney', 'src/bot'],
}

EXCLUDE = {'g_ptrs.c'}          # generated (R-SAVE-2), as in divergence.py


# ---------------------------------------------------------------------------
# The extractor.  Real brace matching, because the regex version of this
# question is what R-SAVE-3's `auditsave.py` got wrong (see audit.py's note),
# and because a trailing `//PGM` after a parameter list is enough to make a
# name-by-regex extractor merge two functions into one -- which it did, to
# `check_dodge`, until the mask was used for the decision as well as the scan.
# ---------------------------------------------------------------------------
FUNC_DECL = re.compile(r'\)\s*$')
TYPE_HEAD = re.compile(r'^(typedef|struct|union|enum)\b')
ASSIGN = re.compile(r'(?<![=!<>+\-*/%&|^])=(?!=)')


def mask(text):
    """Copy of `text` with comments and string/char literal bodies blanked to
    spaces, newlines preserved, so every offset stays valid in the original."""
    out = list(text)
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            while i < n and text[i] != '\n':
                out[i] = ' '
                i += 1
        elif c == '/' and i + 1 < n and text[i + 1] == '*':
            out[i] = out[i + 1] = ' '
            i += 2
            while i < n and not (text[i] == '*' and i + 1 < n and text[i + 1] == '/'):
                if text[i] != '\n':
                    out[i] = ' '
                i += 1
            if i < n:
                out[i] = ' '
                if i + 1 < n:
                    out[i + 1] = ' '
                i += 2
        elif c in '"\'':
            q = c
            i += 1
            while i < n:
                if text[i] == '\\':
                    out[i] = ' '
                    if i + 1 < n:
                        out[i + 1] = ' '
                    i += 2
                    continue
                if text[i] == q:
                    break
                if text[i] != '\n':
                    out[i] = ' '
                i += 1
            i += 1
        else:
            i += 1
    return ''.join(out)


def _line_of(text, off):
    return text.count('\n', 0, off) + 1


def _func_name(decl):
    """Name of the function whose declarator is `decl`.  The parameter list is
    the LAST balanced paren group, so walk back from its `)` to its `(`."""
    d = decl.rstrip()
    if not d.endswith(')'):
        return None
    depth, i = 0, len(d) - 1
    while i >= 0:
        if d[i] == ')':
            depth += 1
        elif d[i] == '(':
            depth -= 1
            if depth == 0:
                break
        i -= 1
    if i < 0:
        return None
    head = d[:i].rstrip()
    if head.endswith(')'):                          # `void (*f)(int)`
        inner = re.search(r'\(\s*\*+\s*([A-Za-z_]\w*)\s*\)$', head)
        return inner.group(1) if inner else None
    m = re.search(r'([A-Za-z_]\w*)\s*$', head)
    return m.group(1) if m else None


def _obj_name(flat):
    m = re.search(r'\}\s*\**\s*([A-Za-z_]\w*)\s*(?:\[[^\]]*\])*\s*(?:=|;)', flat)
    if m:
        return m.group(1)
    if TYPE_HEAD.match(flat):
        t = re.match(r'^(?:typedef\s+)?(?:struct|union|enum)\s+([A-Za-z_]\w*)', flat)
        if t:
            return t.group(1)
    if ASSIGN.search(flat):
        lhs = re.sub(r'\[[^\]]*\]', '', ASSIGN.split(flat)[0]).strip()
        m = re.search(r'([A-Za-z_]\w*)\s*$', lhs)
        if m:
            return m.group(1)
    m = re.search(r'([A-Za-z_]\w*)\s*(?:\[[^\]]*\])*\s*;', flat)
    return m.group(1) if m else None


def _is_definition(flat):
    """A `;`-terminated chunk that DEFINES something.  Prototypes, externs and
    static asserts are declarations and carry no behaviour."""
    if re.match(r'^(extern|_Static_assert|static_assert)\b', flat):
        return False
    return '{' in flat or bool(ASSIGN.search(flat))


def defs(text, want_macros=True):
    """Every top-level definition: name, kind, decl, body, text, line, endline,
    and the enclosing `#if` stack."""
    m = mask(text)
    n = len(m)
    out, cond = [], []
    i = chunk_start = 0
    while i < n:
        c = m[i]
        if c == '#' and m[m.rfind('\n', 0, i) + 1:i].strip() == '':
            start = i
            while i < n:
                nl = m.find('\n', i)
                if nl == -1:
                    i = n
                    break
                if text[nl - 1] != '\\':
                    i = nl + 1
                    break
                i = nl + 1
            flat = ' '.join(text[start:i].split())
            d = re.match(r'#\s*(\w+)', flat)
            key = d.group(1) if d else ''
            if key in ('if', 'ifdef', 'ifndef'):
                cond.append(flat)
            elif key in ('elif', 'else'):
                if cond:
                    cond[-1] = flat
            elif key == 'endif':
                if cond:
                    cond.pop()
            elif key == 'define' and want_macros:
                nm = re.match(r'#\s*define\s+([A-Za-z_]\w*)', flat)
                if nm and ('(' in flat or '\\' in text[start:i]):
                    out.append({'name': nm.group(1), 'kind': 'macro',
                                'decl': flat[:200], 'body': text[start:i],
                                'text': text[start:i],
                                'line': _line_of(text, start),
                                'endline': _line_of(text, max(start, i - 1)),
                                'cond': list(cond)})
            chunk_start = i
            continue
        if c == ';':
            flat = ' '.join(m[chunk_start:i + 1].split())
            if _is_definition(flat):
                chunk = text[chunk_start:i + 1]
                out.append({
                    'name': _obj_name(flat) or '<anon:%d>' % _line_of(text, chunk_start),
                    'kind': ('type' if ('{' in flat and TYPE_HEAD.match(flat)
                                        and not ASSIGN.search(flat.split('{')[0]))
                             else 'data'),
                    'decl': flat[:300], 'body': chunk, 'text': chunk,
                    'line': _line_of(text, chunk_start + len(chunk) - len(chunk.lstrip())),
                    'endline': _line_of(text, i), 'cond': list(cond)})
            i += 1
            chunk_start = i
            continue
        if c == '{':
            depth, j = 0, i
            while j < n:
                if m[j] == '{':
                    depth += 1
                elif m[j] == '}':
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            if j >= n:
                break
            body_end = j + 1
            # The MASKED text decides the name: a trailing `//PGM` after the
            # closing paren is not syntax, and reading it as syntax merges the
            # function into its predecessor.
            decl = ' '.join(m[chunk_start:i].split())
            name = None
            if FUNC_DECL.search(decl) and '(' in decl:
                name = _func_name(decl)
                if name in ('__attribute__', '__declspec'):
                    name = None
            if name:
                head = text[chunk_start:i]
                out.append({'name': name, 'kind': 'func', 'decl': decl,
                            'body': text[i:body_end],
                            'text': text[chunk_start:body_end],
                            'line': _line_of(text, chunk_start + len(head)
                                             - len(head.lstrip())),
                            'endline': _line_of(text, body_end),
                            'cond': list(cond)})
                i = chunk_start = body_end
            else:
                i = body_end            # a type or data chunk: run on to its `;`
            continue
        i += 1
    return out


def strip_comments(text):
    """Comments blanked; string literals KEPT -- a literal is behaviour, it is
    what the player reads."""
    m = mask(text)
    out, inq = [], None
    i, n = 0, len(text)
    while i < n:
        if inq is None and text[i] in '"\'' and m[i] == text[i]:
            inq = text[i]
            out.append(text[i])
        elif inq is not None:
            out.append(text[i])
            if text[i] == '\\':
                i += 1
                if i < n:
                    out.append(text[i])
            elif text[i] == inq:
                inq = None
        else:
            out.append(m[i] if text[i] != m[i] and m[i] == ' ' else text[i])
        i += 1
    return ''.join(out)


def norm_lines(text):
    return [' '.join(l.split()) for l in strip_comments(text).split('\n')
            if l.strip()]


def norm_key(text):
    return '\x00'.join(norm_lines(text))


# ---------------------------------------------------------------------------
# Attribution.  Every ABSENT line belongs to a decision; the findings row is
# FIRST so no later pattern can swallow one.
# ---------------------------------------------------------------------------
CLUSTERS = [
    ('findings (R-195)',
     r'^(osp_r008|SVF_PROJECTILE|hooked|item_spehre_defender)$'),
    ('unsafe libc/raw scanner replaced (R-SEC-1, R-VER-30)',
     r'^(sprintf|strcat|strncpy|strncat|vsprintf|fscanf|stricmp|strcasecmp)$'),
    ('match_mode deleted, four values of g_ruleset instead (R-OSP-12)',
     r'^(m_mode|MODE_TEAM)$'),
    ('the ngLog / ngWorldStats stack, replaced by osp_stats.c (R-OSP-1)',
     r'^(q2log_|nglog_|ngLog_|ngStats$|ngmark|sl_nglog|__nglog|ngloglog|'
     r'ngWorldStats|ngworldstats|_ngws_client_id$|browser$|OSP_ngStatsView$|'
     r'__dummy_nglog_name$|Quake2$|com$)'),
    ('STAT_* macros replaced by the SID_* map (R-OSP-7a)', r'^(STAT_|SID_)'),
    ('symbol collisions prefixed (sec 7 rule 4)',
     r'^(fire_heat|monster_fire_heat|CheckFlood|TECH[1-5]_INDEX|'
     r'MAX_(MODEL|SOUND|IMAGE)INDEXES|avertexnormals|NUMVERTEXNORMALS|maxdot|'
     r'maxdotindex|writebyte|readbyte|ReadByte|ReadShort|bot_networkmessage_s|'
     r'GAMEVERSION|BOT_IMPORT|grapple_state_t|jacketarmor_info|spawn_funcs|'
     r'SP_none|gladi386|RA2_NumArenas|BotDebugCmd|OSP_PrecacheCTFRunes|'
     r'Cmd_ServerCommand|q2log_stdlog)'),
    ('trigger_push bit collision resolved by targetname (R-27)',
     r'^(targeted)$'),
    ('verified present under another name (doc/donor-fdiff.md sec 4.4)',
     r'^(allow_|supershotgun|rocketlauncher|grenadelauncher|railgun|_default_|'
     r'_is_referee|_init_state|_client_hud|'
     r'checkvwepmodel|mylcase|hookbutton|worldlog_file|'
     r'FL_OSP_(NOCMD|BOT)|framecount|timeframes|starttime|'
     r'debug_(ainet|goalai|moveai|weapai)|ra_time)$'),
    ("RA2's GameSpy / remote stats (R-114, R-RA-1a, R-SEC-7)",
     r'^(statsptr|bopfuncs|bucketfuncs|BUCKET_|BOP_|Array|Bucket|Table|'
     r'Do(Get|Set|Find|Send|Escape|Lower)$|bint$|bfloat$|hashFn$|freefn$|'
     r'cmpFn$|mylsearch$|g_crc32$|xcode_buf$|current_time$|'
     r'Socket(StartUp|ShutDown)$|WSA|MAKEWORD$|FD_|net_|GS[A-Z]|gcd_|Internal|'
     r'GenerateAuth$|GetChallenge$|create_challenge$|CheckDiskFile$|DiskWrite$|'
     r'get_sockaddrin$|value_for_key$|NetShutdown$|SendGameSnapShot$|'
     r'SendChallengeResponse$|CreateBucketSnapShot$|DumpBucketSet$|'
     r'NewBucketSet$|FreeBucketSet$|IsStatsConnected$|InitStatsConnection$|'
     r'CloseStatsConnection$|NewStatsPlayer$|NewGame$|FreeGame$|NewPlayer$|'
     r'NewTeam$|RemovePlayer$|ValidatePlayer$|Get(Team|Player)Index$|'
     r'(Server|Team|Player)Op|set_server_bucket_info$|SETSTR$|SETINT$|'
     r'publicserver$|statsdone$|_strdup$|ra2$|z3312$)'),
    ('locals and goto labels -- no behaviour',
     r'^(kill_done|plain_death|clear_args|fraglimit_check|'
     r'format_high_scores_rows|rune_item_found|keep|suff|nrand|count[0-9]|'
     r'lastprio|osp_dead|protratio|when|damagescale|didskin|maxfps_command|'
     r'intermission_command|inactive_seconds|tents|guys|much|tb|clock_t|'
     r'vid_restart|ospdm|Elusive|Supported|disconnect)$'),
    ('donor build switches resolved statically (R-CORE-3)',
     r'^(TOURNEY|ROCKETARENA|BOT_DEBUG|ZOID|XATRIX|ROGUE|CH|WIN32|__LCC__|'
     r'KILL_DISRUPTOR|max_rounds|AMMO_DISRUPTOR)$'),
    ('the menu engines renamed (R-MENU-1, sec 7 rule 4)',
     r'^(showmenu|inmenu|clear_menus|PMenu_|pmenu|PMENU_ALIGN|DisplaySimpMenu$|'
     r'MySelect|PrintMenu)'),
    ('donor file I/O replaced by g_fs.c (R-BOT-8, R-BOT-26)',
     r'^(MAX_PATH|globbuf|filespec|fileinfo|_find(first|next)|globfree|glob|'
     r'ConvertPath|LoadLibrary|HANDLE|_finddata_t|gl_path[cv]|access|ungetc|'
     r'feof|getcwd|CreateProcess|system|rename|remove|fseek|ftell|ferror|exit|'
     r'perror|printf|assert|ftime|timeb|srand|GetTickCount|gettimeofday|Sleep|'
     r'usleep|socket|connect|send|recv|close|closesocket|inet_addr|'
     r'gethostbyname|gethostname|htons|ntohs|memmove|bsearch|isascii|tolower|'
     r'strrchr)$'),
    ("the reconstructions' placeholder field names",
     r'^(_arena_unidentified|_unidentified|_edict_unidentified|x[0-9a-f]{2,3}$|'
     r'osp_[a-z0-9]{4}$|op$|fn$|p$|t$|q$)'),
    ('MD5 (not carried)',
     r'^(MD5|ROTATE_LEFT$|Encode$|Decode$|FF$|GG$|HH$|II$|[GH]$)'),
    ('statusbar literals replaced by the composed bar (R-OSP-7a)',
     r'^(ctf_statusbar|dm_statusbar|single_statusbar|team_statusbar)'),
]
CLUSTERS = [(n, re.compile(p)) for n, p in CLUSTERS]
FINDINGS_CLUSTER = CLUSTERS[0][0]

# What is left of R-195 inside this check's own space, each with the row that
# records it.  An entry whose identifier no longer appears in any donor's
# missing set is STALE and is reported: an exemption that has stopped applying
# is how a check rots (deadvalue.py's controls make the same point).
#
# ONE HISTORICAL ENTRY REMAINS HERE AFTER R-197. R-196 ported R-195.7's
# first two -- `SVF_PROJECTILE` on CTF's blaster bolts and tourney's
# `MOD_GRAPPLE` obituary -- and the build went red on the next `make`, naming
# both exemptions as no longer matching a donor line.  That is the stale-
# exemption rule doing the job it was written for, on its first real occasion:
# the fix and the exemption have to be retired together or the check rots
# silently.
EXEMPT = {
    'osp_r008':       'R-195.4 -- tourney\'s second ZBot heuristic ("cr") and '
                      'the userinfo flag that fed it',
    'item_spehre_defender':
                      'R-195.7 -- id\'s typo, which this tree spells correctly, '
                      'so its substitution fires where the donor\'s never did',
}

# AND THE R-195 FINDINGS THIS CHECK CANNOT SEE, which is worth stating in the
# tool rather than only in the report: every identifier they turn on EXISTS in
# this tree, so no line of theirs is ever ABSENT and no exemption would ever
# fire.  They were found by reading the sweep -- the `as_spine` column, the
# `missing_added` lists and the call delta -- not by this rule.
#
#   R-195.1  WITHDRAWN, and kept here because the withdrawal is the lesson.
#            The sweep was right that RA2's four `p_weapon.c` gate lines are
#            absent; the conclusion drawn from that -- that a person can
#            pre-fire an arena countdown -- was wrong, because R-151 answers the
#            same question one level up, at the latch in `ClientLagThink`.  What
#            made it believable was `doc/reconciliation.md` R-149 still reading
#            "Open, deliberately" two entries before the one that closed it.  An
#            absence is a finding only once you know what else could be
#            answering the question, and no line-level rule can know that.
#   R-195.2  CLOSED BY R-197. RA2's chat-spam counter now runs after
#            FloodProtect(), as the donor does. It remains outside this check's
#            ABSENT-line space because every identifier involved exists here.
#
# CLOSED BY R-196, and listed because a reader of this file should be able to
# tell "this check never saw it" from "this check still cannot see it":
#
#   R-195.3  the chat log now has its Cmd_Say_f call (g_cmds.c), measured on the
#            wire by scenarios/ospchatlog.
#   R-195.5  the spawn exclusions: `ent` is threaded through
#            PlayersRangeFromSpot and both selectors.
#   R-195.6  TossClientWeapon's two quad events and ShutdownGame's accuracy
#            dump, both measured by the same scenario.
#
# ...and R-196 added one finding OF ITS OWN that no rule in this file could ever
# have raised, because it is a MISSING GUARD around a line that is present: the
# donor writes `if (sync_stat != 2)` in front of both of player_die's death
# sounds so a tourney match does not start on a chorus of screams.  A gate that
# is absent is invisible to a rule about lines and to a rule about calls alike
# (doc/donor-fdiff.md sec 6 item 1), and it was found by playing the game.
NOT_MECHANISED = ('R-195.1',)

# A donor file the tree carries no file of that name for.  A file-level decision
# (doc/provenance.md, R-114, and g_monsters.c under R-CORE-8), so its
# definitions are counted and not reported.
KEYWORDS = set('''if else for while do switch case default break continue return
goto sizeof typedef struct union enum static const extern register volatile
signed unsigned void char short int long float double bool true false NULL
inline restrict _Static_assert q_unused q_noreturn'''.split())


def attribute(idents):
    for name, pat in CLUSTERS:
        for u in idents:
            if pat.search(u):
                return name
    return None


# ---------------------------------------------------------------------------
def ref_spine():
    return divergence.ref(divergence.SPINE_BUNDLE, divergence.SPINE_REF)


def git(*args):
    # See divergence.git(): this cache is deliberately bare.
    r = subprocess.run(['git', f'--git-dir={divergence.CACHE}'] + list(args),
                       capture_output=True, text=True, errors='replace')
    return r.stdout if r.returncode == 0 else None


def blob(ref, path):
    return git('show', '%s:%s' % (ref, path))


def interesting(path):
    return (path.endswith(('.c', '.h')) and not divergence.SPILL.search(path)
            and os.path.basename(path) not in EXCLUDE
            and not path.startswith('shared/'))


def classify_files(ref):
    mod, add = [], []
    for row in (git('diff', '--name-status', ref_spine(), ref) or '').splitlines():
        p = row.split('\t')
        if len(p) < 2 or not interesting(p[-1]):
            continue
        if p[0][:1] == 'M':
            mod.append(p[-1])
        elif p[0][:1] == 'A':
            add.append(p[-1])
    return mod, add


def index(text):
    out = {}
    for d in defs(text):
        out.setdefault(d['name'], []).append(d)
    return out


def load_tree(tree):
    by_name, files = {}, {}
    for root, _dirs, fs in os.walk(tree):
        for f in sorted(fs):
            if not f.endswith(('.c', '.h')) or f in EXCLUDE:
                continue
            p = os.path.join(root, f)
            rel = os.path.relpath(p, REPO)
            text = open(p, encoding='latin-1').read()
            ds = defs(text)
            files[rel] = text
            for d in ds:
                d['path'] = rel
                by_name.setdefault(d['name'], []).append(d)
    return by_name, files


def corpus(files):
    """The whole tree as four sets: normalised lines, identifiers, the calls it
    makes anywhere, and the lines bucketed by first identifier so the fuzzy pass
    is not quadratic."""
    lines, idents, made = set(), set(), set()
    for text in files.values():
        s = strip_comments(text)
        for l in s.split('\n'):
            t = ' '.join(l.split())
            if t:
                lines.add(t)
        idents.update(re.findall(r'[A-Za-z_]\w*', s))
        made.update(CALL.findall(s))
    buckets = collections.defaultdict(list)
    for l in lines:
        m = re.search(r'[A-Za-z_]\w*', l)
        buckets[m.group(0) if m else ''].append(l)
    return lines, idents, buckets, made


CALL = re.compile(r'\b([A-Za-z_]\w*)\s*\(')
CALL_KW = KEYWORDS | set('''defined offsetof q_offsetof q_countof va_start
va_end va_arg'''.split())


def calls(d):
    """Every call made by a definition, with its count."""
    if d is None:
        return {}
    out = {}
    body = strip_comments(d['body'] if d['kind'] == 'func' else d['text'])
    for m in CALL.finditer(body):
        n = m.group(1)
        if n in CALL_KW or n == d['name']:
            continue
        out[n] = out.get(n, 0) + 1
    return out


def nl(d):
    return norm_lines(d['text']) if d else []


def delta(spine, donor):
    a, b = nl(spine), nl(donor)
    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    added, removed = [], []
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag in ('insert', 'replace'):
            added += b[j1:j2]
        if tag in ('delete', 'replace'):
            removed += a[i1:i2]
    return added, removed


TRIVIAL = re.compile(r'^[\{\}\(\);,]*$|^(return;|break;|continue;|else|'
                     r'\} else \{|default:)$')


def best_tree(name, donor_d, by_name, prefer, origin, path):
    """Body similarity decides; file locality only breaks ties."""
    cands = by_name.get(name, [])
    if not cands:
        return None
    if len(cands) == 1:
        return cands[0]
    dl = nl(donor_d)
    base = os.path.basename(path)

    def score(c):
        r = difflib.SequenceMatcher(None, dl, nl(c), autojunk=False).ratio() if dl else 0.0
        same = 1 if os.path.basename(c['path']) == base else 0
        pref = 1 if any(c['path'].startswith(p) for p in prefer) else 0
        return (round(r, 3), same if origin == 'shared' else pref, same + pref)

    return max(cands, key=score)


def call_delta(rec, spine_d, donor_d, tree_d):
    """Which calls did the donor ADD that this tree does not make here?

    Line matching answers "is the donor's text here"; this answers "is the
    donor's ACT here", which is the sharper question and the one that found
    R-195.3 and R-195.6 -- a log event whose function exists, has callers, and
    is not called from the site the donor calls it from.  `lostref.py` asks this
    of the symbols a donor DELETED; this is the other half.
    """
    dc, sc, tc = calls(donor_d), calls(spine_d), calls(tree_d)
    miss = {}
    for n, cnt in dc.items():
        if cnt <= sc.get(n, 0):
            continue                      # not the donor's own addition
        if tc.get(n, 0) < cnt:
            miss[n] = tc.get(n, 0)
    if miss:
        rec['missing_calls'] = miss


def sweep(tree, fuzzy=True):
    """Every record, plus the corpus the classification needs."""
    divergence.build_cache()
    by_name, files = load_tree(tree)
    lines, idents, buckets, made = corpus(files)
    tree_names = set(os.path.basename(p) for p in files)
    records = []
    for label, bundle, refname in divergence.DONORS:
        ref = divergence.ref(bundle, refname)
        prefer = SUBDIRS[label]
        mod, add = classify_files(ref)
        for path in mod + add:
            shared = path in mod
            st = blob(ref_spine(), path) if shared else None
            dt = blob(ref, path)
            if dt is None:
                continue
            si = index(st) if st else {}
            di = index(dt)
            names = sorted(set(si) | set(di)) if shared else sorted(di)
            for name in names:
                if name.startswith('<anon'):
                    continue
                sds, dds = si.get(name, []), di.get(name, [])
                for k in range(max(len(sds), len(dds))):
                    sd = sds[k] if k < len(sds) else None
                    dd = dds[k] if k < len(dds) else None
                    if sd and dd and norm_key(sd['text']) == norm_key(dd['text']):
                        continue
                    rec = {'donor': label, 'file': path, 'name': name,
                           'kind': (dd or sd)['kind'],
                           'origin': 'shared' if shared else 'donorfile',
                           'carried': os.path.basename(path) in tree_names}
                    if dd is None:
                        rec['state'] = 'donor_deleted'
                        rec['tree_has'] = bool(by_name.get(name))
                        records.append(rec)
                        continue
                    td = best_tree(name, dd, by_name, prefer,
                                   rec['origin'], path)
                    rec['tree_path'] = td['path'] if td else None
                    if td is None:
                        rec['state'] = 'absent'
                        records.append(rec)
                        continue
                    kd, kt = norm_key(dd['text']), norm_key(td['text'])
                    if kt == kd:
                        rec['state'] = 'as_donor'
                        records.append(rec)
                        continue
                    if sd is not None and kt == norm_key(sd['text']):
                        rec['state'] = 'as_spine'
                        call_delta(rec, sd, dd, td)
                        records.append(rec)
                        continue
                    rec['state'] = 'merged'
                    added, removed = delta(sd, dd)
                    pool = nl(td)
                    pool_set = set(pool)
                    miss = []
                    for l in added:
                        if TRIVIAL.match(l) or l in pool_set:
                            continue
                        if difflib.get_close_matches(l, pool, n=1, cutoff=0.80):
                            continue
                        miss.append(l)
                    rec['donor_added'] = sum(1 for l in added if not TRIVIAL.match(l))
                    rec['missing_added'] = miss
                    rec['kept_removed'] = [l for l in removed
                                           if not TRIVIAL.match(l) and l in pool_set]
                    rec['sim_donor'] = round(difflib.SequenceMatcher(
                        None, nl(dd), pool, autojunk=False).ratio(), 3)
                    call_delta(rec, sd, dd, td)
                    records.append(rec)
    classify(records, lines, idents, buckets, fuzzy)
    for rec in records:
        for n in (rec.get('missing_calls') or {}):
            rec.setdefault('calls_elsewhere', {})[n] = n in made
    return records, (lines, idents, buckets)


def classify(records, lines, idents, buckets, fuzzy=True):
    """Level every missing line against the whole tree, and attribute the
    ABSENT ones to the decision that removed them."""
    for rec in records:
        levels = collections.Counter()
        absent = []
        for l in rec.get('missing_added') or []:
            if l in lines:
                levels['relocated'] += 1
                continue
            ids = [i for i in re.findall(r'[A-Za-z_]\w*', l) if i not in KEYWORDS]
            unknown = sorted({i for i in ids if i not in idents})
            if unknown:
                levels['ABSENT'] += 1
                absent.append({'line': l, 'unknown': unknown,
                               'cluster': attribute(unknown)})
                continue
            if fuzzy:
                key = ids[0] if ids else ''
                if difflib.get_close_matches(l, buckets.get(key, ()), n=1,
                                             cutoff=0.80):
                    levels['adapted'] += 1
                    continue
            levels['rephrased'] += 1
        rec['miss_levels'] = dict(levels)
        rec['miss_absent'] = absent


def findings(records):
    """A missing donor line that no cluster claims, or that a cluster sends to
    the findings row without an EXEMPT entry."""
    out = []
    for rec in records:
        if not rec.get('carried', True):
            continue                     # a file-level decision, see the header
        for d in rec.get('miss_absent') or []:
            if d['cluster'] is None:
                out.append((rec, d, 'no decision in this tree explains it'))
            elif d['cluster'] == FINDINGS_CLUSTER:
                named = [u for u in d['unknown'] if u in EXEMPT]
                if not named:
                    out.append((rec, d, 'reads as a finding and is not exempt'))
    return out


def stale_exemptions(records):
    seen = set()
    for rec in records:
        for d in rec.get('miss_absent') or []:
            seen.update(d['unknown'])
    return sorted(k for k in EXEMPT if k not in seen)


# ---------------------------------------------------------------------------
def report(all_records, out=sys.stdout):
    labels = [d[0] for d in divergence.DONORS]
    w = out.write
    # The matrix is over the definitions in files this tree CARRIES.  A donor
    # file it carries no file of that name for is a file-level decision
    # (doc/provenance.md, R-114, R-CORE-8) and its definitions would otherwise
    # arrive as several hundred `absent` rows that say one thing once.
    records = [r for r in all_records if r.get('carried', True)]
    dropped = collections.Counter()
    for r in all_records:
        if not r.get('carried', True):
            dropped[r['donor']] += 1
    st = collections.defaultdict(collections.Counter)
    lv = collections.defaultdict(collections.Counter)
    for r in records:
        st[(r['donor'], r['origin'])][r['state']] += 1
        lv[r['donor']].update(r.get('miss_levels') or {})

    for origin, title in (('shared', 'shared spine files -- the n-way merges'),
                          ('donorfile', "the donors' own files")):
        w('\n%s\n' % title)
        w('%-8s %6s %9s %8s %8s %8s %8s %8s %8s\n' %
          ('donor', 'defs', 'as_donor', 'merged', 'partial', 'as_spine',
           'absent', 'deleted', 'med sim'))
        for l in labels:
            c = st[(l, origin)]
            mg = [r for r in records if r['donor'] == l and r['origin'] == origin
                  and r['state'] == 'merged']
            part = sum(1 for r in mg if r['missing_added'] or r['kept_removed'])
            sims = sorted(r['sim_donor'] for r in mg if 'sim_donor' in r)
            med = sims[len(sims) // 2] if sims else 0.0
            w('%-8s %6d %9d %8d %8d %8d %8d %8d %8.2f\n' %
              (l, sum(c.values()), c['as_donor'], c['merged'], part,
               c['as_spine'], c['absent'], c['donor_deleted'], med))

    w('\nwhere the donors\' lines went\n')
    w('%-8s %8s %10s %8s %10s %8s\n' %
      ('donor', 'missing', 'relocated', 'adapted', 'rephrased', 'ABSENT'))
    tot = collections.Counter()
    for l in labels:
        c = lv[l]
        tot.update(c)
        w('%-8s %8d %10d %8d %10d %8d\n' %
          (l, sum(c.values()), c['relocated'], c['adapted'], c['rephrased'],
           c['ABSENT']))
    w('%-8s %8d %10d %8d %10d %8d\n' %
      ('all', sum(tot.values()), tot['relocated'], tot['adapted'],
       tot['rephrased'], tot['ABSENT']))

    w('\nthe ABSENT lines, attributed to the decision that removed them\n')
    tally = collections.Counter()
    exempt_sites = collections.defaultdict(list)
    for r in all_records:
        for d in r.get('miss_absent') or []:
            tally[d['cluster'] or '*** UNATTRIBUTED ***'] += 1
            if d['cluster'] == FINDINGS_CLUSTER:
                for u in d['unknown']:
                    if u in EXEMPT:
                        exempt_sites[u].append('%s %s:%s' % (r['donor'], r['file'],
                                                             r['name']))
    for name, _ in CLUSTERS:
        if tally[name]:
            w('  %6d  %s\n' % (tally[name], name))
    if tally['*** UNATTRIBUTED ***']:
        w('  %6d  *** UNATTRIBUTED ***\n' % tally['*** UNATTRIBUTED ***'])

    w('\nthe call delta -- calls the donor ADDS that this tree does not make '
      'at that site\n')
    w('%-8s %10s %8s %12s %10s\n' %
      ('donor', 'defs', 'calls', 'made elsewhere', 'made nowhere'))
    cd = collections.defaultdict(lambda: [0, 0, 0, 0])
    for r in all_records:
        mc = r.get('missing_calls') or {}
        if not mc:
            continue
        cd[r['donor']][0] += 1
        for n in mc:
            cd[r['donor']][1] += 1
            if (r.get('calls_elsewhere') or {}).get(n):
                cd[r['donor']][2] += 1
            else:
                cd[r['donor']][3] += 1
    for l in labels:
        v = cd[l]
        w('%-8s %10d %8d %12d %10d\n' % (l, v[0], v[1], v[2], v[3]))
    w('%-8s %10d %8d %12d %10d\n' %
      ('all', *[sum(cd[l][i] for l in labels) for i in range(4)]))

    if dropped:
        w('\nnot counted above -- definitions in donor files this tree carries '
          'no file of that name\n')
        for l in labels:
            if dropped[l]:
                w('  %-8s %d\n' % (l, dropped[l]))

    f = findings(all_records)
    w('\n%d definition-level finding(s), %d exempt identifier(s), '
      '%d recorded findings outside this check\'s space '
      '(doc/reconciliation.md R-195)\n'
      % (len(f), len(EXEMPT), len(NOT_MECHANISED)))
    for rec, d, why in f:
        w('  !! %s %s:%s -- %s\n' % (rec['donor'], rec['file'], rec['name'], why))
        w('       %s\n' % d['line'][:120])
    for u in sorted(exempt_sites):
        w('  -- %-16s %d line(s): %s\n'
          % (u, len(exempt_sites[u]), '; '.join(sorted(set(exempt_sites[u])))))
        w('       %s\n' % EXEMPT[u])
    w('  -- outside this check\'s space, recorded in R-195: %s\n'
      % ', '.join(NOT_MECHANISED))
    w('\nfnsweep.py: %d definitions touched by at least one donor, %d of them '
      'in files this tree carries\n' % (len(all_records), len(records)))


def check(records):
    hits = []
    for rec, d, why in findings(records):
        hits.append('  !! %s %s:%s -- %s: %s'
                    % (rec['donor'], rec['file'], rec['name'], why,
                       d['line'][:100]))
    for k in stale_exemptions(records):
        hits.append('  !! exemption "%s" no longer applies -- it names no '
                    'donor line any more (R-195)' % k)
    return hits


# ---------------------------------------------------------------------------
# The controls.  R-VER-9 clause 2: a check that has never failed is not
# trusted, so they run in the build beside the check.
#
# The expensive half of this tool is donor extraction and it is tree-
# independent, so the controls mutate the CORPUS -- the set of identifiers the
# tree contains -- which is exactly the input the finding is decided from, and
# they reuse one sweep instead of paying for four.
CORPUS_CONTROLS = [
    # (identifier to remove from the tree's corpus, the donor whose line must
    #  then be reported).  One per donor that has a specific enough one: a
    #  mutation on `ent` would fire hundreds of times and test nothing.
    ('send_sound_to_arena', 'ra2'),     # RA2's arena announcer, in RA_Obituary
    ('max_flechettes', 'rogue'),        # Ground Zero's ammo ceiling, Pickup_Pack
    ('sync_stat', 'osp'),               # tourney's match state, tested everywhere
]


def _echo(hit):
    """A control's own output may not carry the finding marker: audit.py greps
    for `!!` and would report a failure on the line that says the control
    worked.  Its header warns about exactly this shape."""
    return hit.replace('!!', '->').strip()


def selftest(tree):
    bad = 0

    # 1. the extractor, on the shape that broke it: a trailing line comment
    #    after the parameter list, and a commented-out prototype above it.
    tu = ('void fire_hit(edict_t *self)\n{\n    x = 1;\n}\n\n'
          '/*\n=================\ncheck_dodge\n=================\n*/\n'
          '//static void check_dodge (edict_t *self, int speed)\n'
          'void check_dodge(edict_t *self, int speed)        //PGM\n'
          '{\n    y = 2;\n}\n')
    names = [d['name'] for d in defs(tu)]
    if names == ['fire_hit', 'check_dodge']:
        print('  ok  control "extractor: trailing comment" fires: %s' % names)
    else:
        print('  !! control "extractor: trailing comment" did NOT fire; got %s'
              % names)
        bad += 1

    # 2. A struct return type starts with TYPE_HEAD too, but its trailing
    # declarator makes it a function.  Before the function check ran first,
    # this swallowed both bodies through the translation-unit tail.
    tu = ('struct sockaddr_in net_name_to_address(char *name)\n'
          '{\n    return result;\n}\n\n'
          'void address_done(void)\n'
          '{\n    done = 1;\n}\n')
    names = [d['name'] for d in defs(tu)]
    if names == ['net_name_to_address', 'address_done']:
        print('  ok  control "extractor: struct return" fires: %s' % names)
    else:
        print('  !! control "extractor: struct return" did NOT fire; got %s'
              % names)
        bad += 1

    # 3. An attribute on a struct definition is not a function whose name is
    # the attribute macro.  The struct-return exception above must stay narrow.
    tu = ('struct packed_state __attribute__((packed))\n'
          '{\n    int value;\n};\n\n'
          'struct sockaddr_in net_name_to_address(char *name)\n'
          '{\n    return result;\n}\n')
    names = [d['name'] for d in defs(tu)]
    if names == ['packed_state', 'net_name_to_address']:
        print('  ok  control "extractor: struct attribute" fires: %s' % names)
    else:
        print('  !! control "extractor: struct attribute" did NOT fire; got %s'
              % names)
        bad += 1

    # 4. the extractor against the real tree: a count that collapses, or a
    #    crop of anonymous names, means it has stopped reading the tree.
    by_name, files = load_tree(tree)
    total = sum(len(v) for v in by_name.values())
    anon = sum(len(v) for k, v in by_name.items() if k.startswith('<anon'))
    if total > 3500 and anon <= 8:
        print('  ok  control "extractor: %s" fires: %d definitions, %d anonymous'
              % (os.path.relpath(tree, REPO), total, anon))
    else:
        print('  !! control "extractor: real tree" did NOT fire: %d definitions, '
              '%d anonymous -- the extractor has stopped reading the tree'
              % (total, anon))
        bad += 1

    # 5. every exempt identifier must attribute to the FINDINGS row, or a
    #    later cluster is swallowing a finding.
    for k in sorted(EXEMPT):
        if attribute([k]) != FINDINGS_CLUSTER:
            print('  !! control "exempt %s is a finding" did NOT fire: it '
                  'attributes to "%s"' % (k, attribute([k])))
            bad += 1
    else:
        print('  ok  control "the exempt identifiers attribute to the findings '
              'row" fires: %d of %d' % (len(EXEMPT), len(EXEMPT)))

    # 6. one sweep, then the corpus mutations.
    records, (lines, idents, buckets) = sweep(tree, fuzzy=False)
    base = check(records)
    if base:
        print('  !! control "the clean tree is clean" did NOT fire: %d finding(s) '
              'with every exemption in place' % len(base))
        for h in base:
            print('    ' + _echo(h))
        bad += 1
    else:
        print('  ok  control "the clean tree is clean" fires: 0 findings')

    for ident, donor in CORPUS_CONTROLS:
        if ident not in idents:
            print('  !! control "corpus drop: %s": its own mutation no longer '
                  'applies -- the control has rotted' % ident)
            bad += 1
            continue
        classify(records, lines, idents - {ident}, buckets, fuzzy=False)
        hits = [h for h in check(records) if donor in h and ident in h]
        if hits:
            print('  ok  control "corpus drop: %s" fires: %s'
                  % (ident, _echo(hits[0])[:96]))
        else:
            print('  !! control "corpus drop: %s" did NOT fire; the check is '
                  'asleep' % ident)
            bad += 1

    # 7. a stale exemption is a finding.
    classify(records, lines, idents, buckets, fuzzy=False)
    EXEMPT['no_such_identifier_at_all'] = 'the control'
    try:
        if any('no_such_identifier_at_all' in h for h in check(records)):
            print('  ok  control "a stale exemption is reported" fires')
        else:
            print('  !! control "a stale exemption is reported" did NOT fire')
            bad += 1
    finally:
        del EXEMPT['no_such_identifier_at_all']
    return bad


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--tree', default=os.path.join(REPO, 'src'))
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--json')
    ap.add_argument('--no-fuzzy', action='store_true',
                    help='skip the >=0.80 pass: adapted lines read as rephrased')
    a = ap.parse_args()
    tree = os.path.abspath(a.tree)

    for _label, bundle, _r in divergence.DONORS:
        p = os.path.join(divergence.BUNDLES, bundle + '.bundle')
        if not os.path.exists(p):
            # The bundles are vendored (R-PROV-1), so this is a partial checkout
            # rather than a state to pass silently.  A skip, and it says so.
            print('fnsweep.py: SKIP -- %s is not present, so R-PROV-3\'s donor '
                  'diffs cannot be taken' % os.path.relpath(p, REPO))
            return 0

    if a.selftest:
        return 1 if selftest(tree) else 0

    records, _c = sweep(tree, fuzzy=not (a.no_fuzzy or a.check))
    if a.json:
        with open(a.json, 'w') as fh:
            json.dump(records, fh, indent=1)
        print('wrote %s (%d records)' % (a.json, len(records)))
    if a.check:
        hits = check(records)
        for h in hits:
            print(h)
        print('fnsweep.py: %d definitions, %d finding(s) beyond the %d exempt '
              'identifier(s) of R-195, whose other %d findings are outside this '
              'check\'s space (see NOT_MECHANISED)'
              % (len(records), len(hits), len(EXEMPT), len(NOT_MECHANISED)))
        return 1 if hits else 0
    report(records)
    return 0


if __name__ == '__main__':
    sys.exit(main())
