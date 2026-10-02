#!/usr/bin/env python3
"""The game<->botlib ABI, compared against the brain's own header.

WHY THIS EXISTS.  `bsp_trace_t` is 88 bytes, so the
`Trace` slot always passes a hidden return buffer -- and on 32-bit that buffer
IS the first visible argument while on x86-64 and aarch64 it is a hidden
register.  The two spellings are therefore the same ABI at 32 bits and different
ABIs at 64, and `osp-tourney`'s botlib.h -- the one this tree takes -- carries
only the 32-bit one, because osp-tourney is a 32-bit port.

Taken verbatim on aarch64 that shifts every argument one place: `passent`
receives a truncated pointer, every trace returns an all-zero bsp_trace_t, the
brain believes it is wedged in solid everywhere, and the bots stand still.  It
compiles, it links, it loads the library, it spawns the clients, and it plays
exactly nothing.

WHAT THE CONTRACT OFFERS AND WHY NEITHER HELPS.  `BotVersion` is slot 0 of
`bot_export_t` precisely so the two sides can shake hands -- and it returns
"BotLib v0.96" on both sides of an ABI split, because the version is the
brain's, not the convention's.  Risk 6 offers a `Test()` round trip at load, and
`Test(int, char *, vec3_t, vec3_t)` passes no struct by value, so it exercises
none of the convention it is meant to prove.  A handshake that cannot fail is
not a handshake.

WHAT THIS CHECKS.  The ten `bot_import_t` slots against the brain's import
block (be_interface.h), and the twenty `bot_export_t` ones against the brain's
published game/botlib.h, which is the layout be_interface.c fills.  Both tables
are compared in ORDER, because a table of function pointers is indexed by
position and two swapped slots with the right names are every call to either
landing in the other.  Imports must agree on their parameter lists (four return
types differ harmlessly: the brain returns an `int` the game ignores); exports
on return types as well, because there the game reads what comes back.  And
for `Trace`, the preprocessor condition that selects between the two
spellings must agree character for character, because a 64-bit target inside
one list and outside the other is a crash.  The export half was described here
before it existed; it is the half that was missing.

AND WHAT IT CHECKS SECOND: STRUCT SIZE, MEASURED.  Every struct in the contract
crosses the boundary by pointer or by value, so both sides must agree on its
size and on every offset in it -- and two did not, each because a type NAME
means something different on the two sides:

  bsp_trace_t         80 here against 84 there.  osp-tourney's port swept
                      `qboolean` to `bool`, and C99's bool is one byte where
                      `enum { qfalse, qtrue }` is four, so every field from
                      `fraction` down was four bytes out and the brain read our
                      endpos[0] as the trace fraction.
  bot_updateclient_t  1292 here against 1228 there.  The array bound was
                      spelled MAX_STATS, and Q2PRO's is 64 under
                      USE_NEW_GAME_API where the brain's q_shared.h has 32.

So `src/bot/botlib.h` carries a `_Static_assert` per contract struct, and this
tool does not take those numbers on trust: it COMPILES a probe against the
brain's own headers and compares what the brain's compiler says.  A number
copied from a document goes stale; a number a compiler produced this minute does
not.

AND THIRD: THE STAGED WRITE IS THE ENGINE'S ENCODING.  The import redirection
(bl_redirgi.c) stages every gi.Write* and replays it at the flush, so what
reaches the wire is whatever the staging kept.  Gladiator's zeroed an
out-of-range WriteByte or WriteShort where the engine keeps the low byte and
the low sixteen bits, and target_laser_think's TE_LASER_SPARKS colour is
exactly such a byte -- `s.skinnum`, four palette indices in an int -- so with
bots loaded every laser's sparks went out as palette index 0.  This compiles
the tree's own Bot_WriteByte and Bot_WriteShort against a stub that records
what they stage and compares it with q2pro's MSG_WriteByte/MSG_WriteShort.

AND FOURTH: THE SECOND BOTLIB (R-BOT-31).  The Quake III botlib,
q3a_bot_backport_for_q2, implements the same two tables, under q2_ names --
its own Quake III headers already define a bot_input_t and a bot_settings_t
with other layouts -- in botlib/be_interface_q2.h.  The same three questions
are asked of it: both tables, in order, against ours (the prefix dropped
first; its export table goes on after Test with slots of its own, which a
game copying twenty never reads); the seven struct sizes, measured by a probe
compiled against ITS headers, where a wrong one is a compile error because the
header asserts them itself -- so a probe that will not build is a finding here,
not a skip; and the calling convention of GetBotAPI, which is a plain C
function on every target: a __stdcall one on 32-bit Windows, called the way
this game calls Gladiator's own gladiator.dll, returns with the stack four
bytes off.  Our side's pointer type is held to the same rule.  An absent
checkout is a skip and said so, like Gladiator's.

WHAT IT STILL CANNOT SEE.  Member ORDER at equal size -- two structs of 84 bytes
with two fields transposed pass -- and a slot the brain calls with the wrong
argument count inside itself.  Neither is a reason to
skip the two comparisons that do work.

USAGE
    tools/botabi.py [--ours src/bot/botlib.h] [--brain <gladiator-bot-restored>]
                    [--q3 <q3a_bot_backport_for_q2>]
    tools/botabi.py --selftest
"""
import argparse
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The one slot whose spelling is word-size dependent, and the condition that
# must be identical on both sides.
GUARDED_SLOT = 'Trace'


def strip(t):
    t = re.sub(r'/\*.*?\*/', '', t, flags=re.S)
    t = re.sub(r'//[^\n]*', '', t)
    return t


def read(p):
    with open(p, encoding='utf-8', errors='replace') as f:
        return f.read()


# Two names for one thing, each verified by measurement rather than assumed:
#   bot_input_t / ea_state_t   36 bytes, member for member -- the brain's names
#                              are offset-derived, ours are the 1999 header's
#   qboolean / int             the same four bytes in this engine
ALIASES = {'ea_state_t': 'bot_input_t', 'qboolean': 'int', '_DWORD': 'int'}


def norm_type(s):
    """One parameter or return type, reduced to what a linker can tell apart.

    `const vec3_t`, `vec3_t` and `float *` are the same pointer; a PARAMETER
    NAME is not part of the ABI and the brain omits them, so it goes; and the
    two names for the input structure are folded together.  A difference this
    erases is a difference no call can get wrong.
    """
    s = re.sub(r'\b(?:const|__cdecl|q_gameabi|struct)\b', ' ', s)
    s = re.sub(r'\bfloat\s*\*', ' vec3_t ', s)
    s = re.sub(r'\bvec3_t\s*\*', ' vec3_t ', s)
    s = re.sub(r'\s*\*\s*', ' * ', s)
    s = re.sub(r'\s+', ' ', s).strip()
    if s in ('...', 'void', ''):
        return s
    # drop a trailing identifier that is a parameter NAME: it is the last word,
    # it is not a type keyword, and something is left in front of it.
    parts = s.split(' ')
    if len(parts) > 1 and re.fullmatch(r'[A-Za-z_]\w*', parts[-1]) \
            and parts[-1] not in ALIASES and not parts[-1].endswith('_t') \
            and parts[-1] not in ('int', 'char', 'float', 'void', 'short',
                                  'long', 'unsigned', 'byte', 'bool'):
        parts = parts[:-1]
    s = ' '.join(parts)
    for k, v in ALIASES.items():
        s = re.sub(r'\b%s\b' % k, v, s)
    return s.strip()


def norm(s):
    """A whole parameter list, or a return type, as a comparable string."""
    if '(' in s or ',' in s or ' ' in s or s.strip():
        return ', '.join(norm_type(p) for p in s.split(','))
    return norm_type(s)


# `ret (*Name)(args);` -- the shape every slot in both tables has.  The brain
# writes `ret (__cdecl *Name)(args);`, which the qualifier strip above removes.
SLOT = re.compile(r'([A-Za-z_][\w \t*]*?)\(\s*\*?\s*(?:__cdecl\s*\*?\s*)?'
                  r'([A-Za-z_]\w*)\s*\)\s*\(([^;]*)\)\s*;')


def slots(text, first, last):
    """-> {name: (ret, args)} for the block between two member names."""
    t = strip(text)
    i = t.find(first)
    if i < 0:
        return {}
    j = t.find(last, i)
    if j < 0:
        return {}
    j = t.find(';', j) + 1
    out = {}
    for m in SLOT.finditer(t[i - 200 if i > 200 else 0:j]):
        out[m.group(2)] = (norm(m.group(1)), norm(m.group(3)))
    return out


def guard_condition(text, slot):
    """The `#if` line immediately above the guarded slot, or '' when unguarded."""
    lines = strip(text).split('\n')
    for n, ln in enumerate(lines):
        if re.search(r'\(\s*\*\s*(?:__cdecl\s*\*?\s*)?%s\s*\)' % slot, ln) or \
           re.search(r'\*\s*%s\s*\)\s*\(' % slot, ln):
            for k in range(n - 1, max(-1, n - 4), -1):
                if lines[k].lstrip().startswith('#if'):
                    return re.sub(r'\s+', ' ', lines[k].strip())
            return ''
    return None


# `_Static_assert(sizeof(X) == N, ...)` -- the layout half of the contract.
ASSERT = re.compile(r"_Static_assert\s*\(\s*sizeof\s*\(\s*(\w+)\s*\)\s*=="
                    r"\s*(0[xX][0-9a-fA-F]+|\d+)")

PROBE_HEAD = '#include "q_shared.h"\n#include "botlib.h"\n#include <stdio.h>\nint main(void) {\n'
PROBE_TAIL = '    return 0;\n}\n'


def brain_sizes(brain_root, names):
    """Compile a probe against the brain's own headers -> {name: sizeof}.

    Returns None when the probe cannot be built, which is a SKIP and not a pass;
    the caller says so out loud.  Deliberately the brain's game/ headers and not
    ours, because the whole question is what the other side's compiler thinks.
    """
    import shutil
    import subprocess
    import tempfile
    inc = os.path.join(brain_root, "game")
    if not os.path.exists(os.path.join(inc, "botlib.h")):
        return None

    # The HOST compiler, and deliberately NOT $CC.  Reaching for $CC is the
    # reflex and it is wrong here: this probe is built to be RUN, and `make`
    # exports a different CC for each of the ten configurations -- so under the
    # win32 row the probe compiled fine, mingw wrote `probe.exe` where a
    # `probe` was asked for, and the tool died with FileNotFoundError instead of
    # measuring anything.  `make check` then failed the build on a broken tool
    # rather than on a finding, which is the build gate doing its job about the wrong
    # thing.  The sizes are word-size independent (every struct here is
    # pointer-free), so the host compiler is the right one to ask.
    cc = shutil.which("cc") or shutil.which("gcc")
    if not cc:
        return None
    body = "".join('    printf("%s %%zu\\n", sizeof(%s));\n' % (n, n)
                   for n in names)
    d = tempfile.mkdtemp()
    try:
        src = os.path.join(d, "probe.c")
        exe = os.path.join(d, "probe")
        with open(src, "w") as f:
            f.write(PROBE_HEAD + body + PROBE_TAIL)
        # Any failure at all is a SKIP that the caller reports, never an
        # exception: a probe that cannot be built or cannot be run says nothing
        # about the contract, and a tool that raises fails the build on itself.
        r = subprocess.run([cc, "-I", inc, "-std=gnu99", "-w", "-o", exe, src],
                           capture_output=True, text=True)
        if r.returncode or not os.path.exists(exe):
            return None
        r = subprocess.run([exe], capture_output=True, text=True)
        if r.returncode:
            return None
        out = {}
        for ln in r.stdout.split("\n"):
            f2 = ln.split()
            if len(f2) == 2 and f2[1].isdigit():
                out[f2[0]] = int(f2[1])
        return out
    except OSError:
        return None
    finally:
        shutil.rmtree(d, ignore_errors=True)


STAGE_PROBE = r"""
#include <stdio.h>
#include <stdint.h>
typedef enum { BW_CHAR, BW_BYTE, BW_SHORT } bot_writekind_t;
static void BotStageInt(bot_writekind_t kind, int v)
{
    printf("%d %d\n", (int)kind, v);
}
@FUNCS@
int main(void)
{
    static const int in[] = { @INPUTS@ };
    unsigned i;

    for (i = 0; i < sizeof(in) / sizeof(in[0]); i++) {
        Bot_WriteByte(in[i]);
        Bot_WriteShort(in[i]);
    }
    return 0;
}
"""

# In range, at both edges, and out of it both ways -- the last is red
# target_laser's `s.skinnum`, 0xf2f2f0f0.
STAGE_INPUTS = (0, 1, 127, 128, 255, 256, -1, -128, 32767, 32768, -32768,
                -32769, 70000, -70000, 0x7fffffff, -0x80000000,
                0xf2f2f0f0 - (1 << 32))


def c_function(text, name):
    """The full text of `static void <name>(...) { ... }`, brace-matched."""
    m = re.search(r'^static void %s\s*\([^)]*\)\s*\{' % re.escape(name),
                  text, re.M)
    if not m:
        return None
    depth, i = 0, m.end() - 1
    while i < len(text):
        if text[i] == '{':
            depth += 1
        elif text[i] == '}':
            depth -= 1
            if depth == 0:
                return text[m.start():i + 1]
        i += 1
    return None


def compare_staging(redir_text):
    """Findings for the staged value of Bot_WriteByte / Bot_WriteShort."""
    import shutil
    import subprocess
    import tempfile
    funcs = [c_function(redir_text, n) for n in ('Bot_WriteByte',
                                                 'Bot_WriteShort')]
    if not all(funcs):
        return ['bl_redirgi.c: Bot_WriteByte or Bot_WriteShort not found, so '
                'the staged-write check has nothing to compile']
    cc = shutil.which("cc") or shutil.which("gcc")
    if not cc:
        return ['SKIP: no host compiler for the staged-write probe']
    src_text = (STAGE_PROBE.replace('@FUNCS@', '\n'.join(funcs))
                .replace('@INPUTS@', ', '.join('(int)%dLL' % v
                                               for v in STAGE_INPUTS)))
    d = tempfile.mkdtemp()
    try:
        src = os.path.join(d, "stage.c")
        exe = os.path.join(d, "stage")
        with open(src, "w") as f:
            f.write(src_text)
        # A probe of OUR code that will not build is a finding, not a skip:
        # the functions grew a dependency the stub does not carry, and a
        # check that quietly stops running is the one that lets this back in.
        r = subprocess.run([cc, "-std=gnu99", "-fwrapv", "-w", "-o", exe, src],
                           capture_output=True, text=True)
        if r.returncode or not os.path.exists(exe):
            return ['the staged-write probe does not compile against its '
                    'stub: %s' % (r.stderr.strip().split('\n') or ['?'])[0]]
        r = subprocess.run([exe], capture_output=True, text=True)
        if r.returncode:
            return ['the staged-write probe exited %d' % r.returncode]
    except OSError as e:
        return ['SKIP: the staged-write probe could not run (%s)' % e]
    finally:
        shutil.rmtree(d, ignore_errors=True)

    got = [tuple(int(x) for x in ln.split()) for ln in r.stdout.split('\n')
           if ln.strip()]
    out = []
    if len(got) != 2 * len(STAGE_INPUTS):
        return ['the staged-write probe staged %d value(s) for %d write(s)'
                % (len(got), 2 * len(STAGE_INPUTS))]
    for k, c in enumerate(STAGE_INPUTS):
        for (kind, v), name, want in (
                (got[2 * k], 'Bot_WriteByte', c & 0xff),
                (got[2 * k + 1], 'Bot_WriteShort',
                 ((c & 0xffff) ^ 0x8000) - 0x8000)):
            if v != want:
                out.append('%s(%d) staged %d; the engine writes %d '
                           '(MSG_Write%s keeps the low %s)'
                           % (name, c, v, want, name[9:],
                              'byte' if name.endswith('Byte')
                              else 'sixteen bits'))
    return out


def compare_sizes(ours_text, brain_root):
    """Our asserted sizes against what the brain's compiler measures."""
    want = {m.group(1): int(m.group(2), 0) for m in ASSERT.finditer(ours_text)}
    if not want:
        return ["no _Static_assert(sizeof(...)) in our botlib.h -- the layout "
                "half of the ABI contract is not being checked at all"]
    got = brain_sizes(brain_root, sorted(want))
    if got is None:
        return ["SKIP: could not build a probe against %s/game -- the layout "
                "half is unchecked this run" % brain_root]
    bad = []
    for name in sorted(want):
        if name not in got:
            bad.append("%s: the brain's headers do not declare it" % name)
        elif got[name] != want[name]:
            bad.append("%s: we assert %d, the brain's compiler says %d"
                       % (name, want[name], got[name]))
    return bad


def compare(ours_text, brain_text):
    bad = []

    a = slots(ours_text, 'BotInput', 'DebugLineShow')
    b = slots(brain_text, 'BotInput', 'DebugLineShow')
    if not a:
        return ['could not find bot_import_t in our botlib.h']
    if not b:
        return ['could not find the brain\'s botimport block']

    shared = sorted(set(a) & set(b))
    if len(shared) < 8:
        bad.append('only %d import slot(s) matched by name -- the parser is not '
                   'seeing one of the two tables' % len(shared))
    for name in shared:
        # The RETURN type differs harmlessly on four slots: the brain declares
        # Print, BotClientCommand, DebugLineDelete and DebugLineShow as `int`
        # where the 1999 game header says `void`, and a caller that ignores the
        # result cannot tell.  Arguments are what a mismatch corrupts.
        if a[name][1] != b[name][1]:
            bad.append('%s: arguments differ\n      ours : (%s)\n      brain: (%s)'
                       % (name, a[name][1], b[name][1]))

    # ...and the ORDER, which is the ABI of a table of function pointers: the
    # other side indexes it by position, so two slots swapped with the right
    # names and the right arguments is every call to either one landing in
    # the other.
    oa = [n for n in a if n in b]
    ob = [n for n in b if n in a]
    if oa != ob:
        bad.append('bot_import_t slots are in a different order\n'
                   '      ours : %s\n      brain: %s'
                   % (' '.join(oa), ' '.join(ob)))

    ga = guard_condition(ours_text, GUARDED_SLOT)
    gb = guard_condition(brain_text, GUARDED_SLOT)
    if ga is None or gb is None:
        bad.append('%s slot not found in %s' %
                   (GUARDED_SLOT, 'ours' if ga is None else 'the brain'))
    elif ga != gb:
        bad.append('%s is selected by a different condition\n'
                   '      ours : %s\n      brain: %s'
                   % (GUARDED_SLOT, ga or '(unguarded)', gb or '(unguarded)'))
    return bad


def compare_exports(ours_text, brain_pub_text):
    """bot_export_t against the brain's game/botlib.h, which is the layout
    the brain fills (be_interface.c assigns `botexport.<slot>` by name) and the
    game then reads by position.  Every slot, in order, with its return type
    as well as its arguments: here the GAME reads what the brain returns, so a
    `char *` against an `int` is a pointer truncated on every 64-bit target."""
    a = slots(ours_text, 'BotVersion', 'Test')
    b = slots(brain_pub_text, 'BotVersion', 'Test')
    if not a:
        return ['could not find bot_export_t in our botlib.h']
    if not b:
        return ["could not find bot_export_t in the brain's game/botlib.h"]
    bad = []
    if list(a) != list(b):
        bad.append('bot_export_t slots differ in name or order\n'
                   '      ours : %s\n      brain: %s'
                   % (' '.join(a), ' '.join(b)))
    for name in (n for n in a if n in b):
        if a[name] != b[name]:
            bad.append('%s: bot_export_t slot differs\n      ours : %s (%s)\n'
                       '      brain: %s (%s)' % (name, a[name][0], a[name][1],
                                                  b[name][0], b[name][1]))
    return bad


SELFTEST_EXPORTS = '''
typedef struct bot_export_s {
    char *(*BotVersion)(void);
    int (*BotSetupLibrary)(void);
    int (*BotLibVarSet)(char *var_name, char *value);
    int (*BotAI)(int client, float thinktime);
    int (*Test)(int parm0, char *parm1, vec3_t parm2, vec3_t parm3);
} bot_export_t;
'''


SELFTEST_OURS = '''
typedef struct bot_import_s {
    void (*BotInput)(int client, bot_input_t *bi);
    void (*BotClientCommand)(int client, char *str, ...);
    void (*Print)(int type, char *fmt, ...);
#if defined(__x86_64__) || defined(__aarch64__)
    bsp_trace_t *(*Trace)(bsp_trace_t *retbuf, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask);
#else
    bsp_trace_t (*Trace)(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask);
#endif
    int (*PointContents)(const vec3_t point);
    void *(*GetMemory)(int size);
    void (*FreeMemory)(void *ptr);
    int (*DebugLineCreate)(void);
    void (*DebugLineDelete)(int line);
    void (*DebugLineShow)(int line, vec3_t start, vec3_t end, int color);
} bot_import_t;
'''

SELFTEST_BRAIN = SELFTEST_OURS.replace('bot_import_s', 'botimport_block_s') \
                              .replace('bot_import_t;', 'botimport_block_t;')


Q3_HEADER = os.path.join('botlib', 'be_interface_q2.h')

# Our name for a contract struct -> the Quake III botlib's.  `bsp_trace_t`,
# `bsp_surface_t` and `qboolean` are spelled alike on both sides.
Q3_TYPES = {'bot_settings_t': 'q2_bot_settings_t',
            'bot_clientsettings_t': 'q2_bot_clientsettings_t',
            'bot_input_t': 'q2_bot_input_t',
            'bot_updateclient_t': 'q2_bot_updateclient_t',
            'bot_updateentity_t': 'q2_bot_updateentity_t'}

Q3_PROBE_HEAD = ('#include "q_shared.h"\n#include "botlib.h"\n'
                 '#include "be_ai_chat.h"\n#include "be_interface_q2.h"\n'
                 '#include <stdio.h>\nint main(void) {\n')


def find_q3():
    """Where the Quake III botlib is: $Q3DIR, the submodule, a sibling."""
    env = os.environ.get('Q3DIR')
    if env:
        return env
    for d in (os.path.join(REPO, 'vendor', 'q3a_bot_backport_for_q2'),
              os.path.join(REPO, '..', 'q3a_bot_backport_for_q2')):
        if os.path.exists(os.path.join(d, Q3_HEADER)):
            return d
    return os.path.join(REPO, 'vendor', 'q3a_bot_backport_for_q2')


def unprefix(text):
    """The Quake III botlib's contract types without their q2_ prefix."""
    return re.sub(r'\bq2_(bot_\w+_t)\b', r'\1', text)


def q3_tables(hdr):
    """Its two tables, each cut out whole and unprefixed: (imports, exports).
    Cut, because slots() reads from a little before the first slot it is
    asked for, and in this header the import table ends right where the export
    table starts."""
    out = []
    for first, last in (('typedef struct q2_bot_import_s', '} q2_bot_import_t;'),
                        ('typedef struct q2_bot_export_s', '} q2_bot_export_t;')):
        i = hdr.find(first)
        j = hdr.find(last, i) if i >= 0 else -1
        out.append(unprefix(hdr[i:j + len(last)]) if i >= 0 and j >= 0 else '')
    return out[0], out[1]


def q3_sizes(root, names):
    """-> ({name: sizeof}, None), or (None, why) when the probe fails.

    Compiled with the host compiler for the reason brain_sizes() gives.  The
    header asserts the sizes itself, so a probe that does not compile is
    reported with the compiler's own first error -- usually the assert that
    failed -- rather than skipped.
    """
    import shutil
    import subprocess
    import tempfile
    cc = shutil.which("cc") or shutil.which("gcc")
    if not cc:
        return None, "SKIP: no host compiler for the Quake III probe"
    body = "".join('    printf("%s %%zu\\n", sizeof(%s));\n'
                   % (n, Q3_TYPES.get(n, n)) for n in names)
    d = tempfile.mkdtemp()
    try:
        src = os.path.join(d, "probe.c")
        exe = os.path.join(d, "probe")
        with open(src, "w") as f:
            f.write(Q3_PROBE_HEAD + body + PROBE_TAIL)
        # On macOS the header is told it is macOS, as the botlib's own
        # Makefile tells it (-DMACOS_X on Darwin): its q_shared.h recognises
        # id's MACOS_X token rather than __APPLE__, and without it defines no
        # ID_INLINE and the probe does not compile there.
        plat = ["-DMACOS_X"] if sys.platform == "darwin" else []
        r = subprocess.run([cc, "-I", os.path.join(root, "game_q3"),
                            "-I", os.path.join(root, "botlib")] + plat +
                           ["-std=gnu99", "-w", "-o", exe, src],
                           capture_output=True, text=True)
        if r.returncode or not os.path.exists(exe):
            first = [ln for ln in r.stderr.split('\n') if 'error' in ln]
            return None, ("the probe against %s does not compile: %s"
                          % (Q3_HEADER, (first or ['?'])[0].strip()))
        r = subprocess.run([exe], capture_output=True, text=True)
        if r.returncode:
            return None, "the Quake III probe exited %d" % r.returncode
        out = {}
        for ln in r.stdout.split("\n"):
            f2 = ln.split()
            if len(f2) == 2 and f2[1].isdigit():
                out[f2[0]] = int(f2[1])
        return out, None
    except OSError as e:
        return None, "SKIP: the Quake III probe could not run (%s)" % e
    finally:
        shutil.rmtree(d, ignore_errors=True)


def stdcall_findings(header_text, where):
    """GetBotAPI as a plain C function, and every pointer to it."""
    bad = []
    # Preprocessor lines out first, so a finding quotes the declaration and
    # not the export macro's #if block in front of it.
    t = '\n'.join(ln for ln in strip(header_text).split('\n')
                  if not ln.lstrip().startswith('#'))
    decl = re.findall(r'([^;{}]*\bGetBotAPI\s*\)?\s*\([^;{]*)', t)
    if not decl:
        bad.append('%s: no GetBotAPI declaration found' % where)
    for d in decl:
        if re.search(r'\b(__stdcall|WINAPI|CALLBACK|APIENTRY)\b', d):
            bad.append('%s: GetBotAPI is declared %s -- a game calls it as a '
                       'plain C function, as the real gladiator.dll answers'
                       % (where, re.sub(r'\s+', ' ', d.strip())))
    return bad


def pointer_findings(text, where):
    """Our pointer to GetBotAPI, which a cast makes the call: plain C too."""
    t = strip(text)
    m = re.search(r'typedef[^;]*\(\s*[A-Za-z_ \t]*\*\s*PFNGetBotAPI\s*\)[^;]*;', t)
    if not m:
        return ['%s: no PFNGetBotAPI typedef found' % where]
    if re.search(r'\b(__stdcall|WINAPI|CALLBACK|APIENTRY)\b', m.group(0)):
        return ['%s: PFNGetBotAPI is %s -- both botlibs export a plain C '
                'function' % (where, re.sub(r'\s+', ' ', m.group(0)))]
    return []


def compare_q3(ours_text, q3_root, hdr=None, sizes=True):
    """The Quake III botlib against our header: tables, sizes, convention.

    `hdr` is its header's text, read from q3_root when not given; the selftest
    passes a mutated one.  The sizes are a probe compiled against the files in
    q3_root, so a mutated text is compared with `sizes` off.
    """
    if hdr is None:
        hdr = read(os.path.join(q3_root, Q3_HEADER))
    imports, exports = q3_tables(hdr)
    bad = [b.replace('brain', 'Quake III botlib')
           for b in compare(ours_text, imports) + compare_exports(ours_text, exports)]
    bad += stdcall_findings(hdr, Q3_HEADER)
    if not sizes:
        return bad
    want = {m.group(1): int(m.group(2), 0) for m in ASSERT.finditer(ours_text)}
    got, why = q3_sizes(q3_root, sorted(want))
    if why:
        bad.append(why)
        return bad
    for name in sorted(want):
        if name not in got:
            bad.append("%s: the Quake III botlib's headers do not declare it" % name)
        elif got[name] != want[name]:
            bad.append("%s: we assert %d, the Quake III botlib's compiler says %d"
                       % (name, want[name], got[name]))
    return bad


def find_brain():
    """Where the brain is.

    `vendor/gladiator-bot-restored` is the submodule and the documented place.
    A sibling checkout beside the repository is still accepted, because that is
    where it lived before the submodule existed and a working tree should not
    stop working for a move.  $GLADDIR overrides both.
    """
    env = os.environ.get('GLADDIR')
    if env:
        return env
    for d in (os.path.join(REPO, 'vendor', 'gladiator-bot-restored'),
              os.path.join(REPO, '..', 'gladiator-bot-restored')):
        if os.path.isdir(os.path.join(d, 'botlib')):
            return d
    return os.path.join(REPO, 'vendor', 'gladiator-bot-restored')


def selftest():
    """Three mutations of a header that agrees with itself.

    A check that has never failed is not trusted.  Mutation 1 is the original
    defect exactly -- the 32-bit spelling on both arms.
    """
    cases = [
        ('unmutated', SELFTEST_OURS, SELFTEST_BRAIN, 0),
        # Our side loses the 64-bit arm.
        ('Trace unguarded on our side', re.sub(
            r'#if defined\(__x86_64__\).*?#endif\n',
            '    bsp_trace_t (*Trace)(vec3_t start, vec3_t mins, vec3_t maxs, '
            'vec3_t end, int passent, int contentmask);\n',
            SELFTEST_OURS, flags=re.S), SELFTEST_BRAIN, 1),
        # a condition that is not the brain's, which is the same crash on any
        # 64-bit target inside one list and outside the other
        ('Trace guarded by a different condition',
         SELFTEST_OURS.replace('#if defined(__x86_64__) || defined(__aarch64__)',
                               '#if __SIZEOF_POINTER__ == 8'),
         SELFTEST_BRAIN, 1),
        # an argument dropped from a slot nobody would look at twice
        ('BotAddSound-shaped argument drift',
         SELFTEST_OURS.replace('void (*FreeMemory)(void *ptr);',
                               'void (*FreeMemory)(void *ptr, int tag);'),
         SELFTEST_BRAIN, 1),
    ]
    # The order, which is the ABI: two import slots swapped.
    swapped = SELFTEST_OURS.replace(
        '    int (*DebugLineCreate)(void);\n    void (*DebugLineDelete)(int line);\n',
        '    void (*DebugLineDelete)(int line);\n    int (*DebugLineCreate)(void);\n')
    cases.append(('import slots swapped', swapped, SELFTEST_BRAIN, 1))
    bad = 0
    for label, ours, brain, want in cases:
        got = compare(ours, brain)
        fired = 1 if got else 0
        ok = fired == want
        print('  %-42s %s' % (label, 'ok' if ok else 'DID NOT FIRE'
                              if want else 'FIRED ON CLEAN INPUT'))
        for g in got:
            print('      ' + g.replace('\n', '\n  '))
        if not ok:
            bad += 1

    # ...and three for the EXPORT table, which was never compared at all.
    n_exports = 0
    for label, ours, want in (
            ('exports unmutated', SELFTEST_EXPORTS, 0),
            ('export slots swapped', SELFTEST_EXPORTS.replace(
                '    int (*BotSetupLibrary)(void);\n    int (*BotLibVarSet)(char *var_name, char *value);\n',
                '    int (*BotLibVarSet)(char *var_name, char *value);\n    int (*BotSetupLibrary)(void);\n'), 1),
            ('export return type drift', SELFTEST_EXPORTS.replace(
                'char *(*BotVersion)(void);', 'int (*BotVersion)(void);'), 1)):
        n_exports += 1
        got = compare_exports(ours, SELFTEST_EXPORTS)
        fired = 1 if got else 0
        ok = fired == want
        print('  %-42s %s' % (label, 'ok' if ok else 'DID NOT FIRE'
                              if want else 'FIRED ON CLEAN INPUT'))
        for g in got:
            print('      ' + g.replace('\n', '\n  '))
        if not ok:
            bad += 1

    # ...and two for the LAYOUT half, against the real brain, because the
    # numbers those asserts carry are the whole point of them.  Mutation 1 is
    # The second defect exactly: bsp_trace_t at 80 bytes, which is what osp-tourney's
    # `qboolean` -> `bool` sweep produced.  Skipped, out loud, when the brain
    # is not beside the repository -- a control that quietly does not run is
    # worse than no control.
    n = len(cases) + n_exports
    real = os.path.join(REPO, 'src/bot/botlib.h')
    root = find_brain()
    if os.path.exists(real) and brain_sizes(root, ['bsp_trace_t']):
        text = read(real)
        n += 2
        for label, mutated, want in (
                ('sizes unmutated', text, 0),
                ('bsp_trace_t asserted at 80',
                 text.replace('sizeof(bsp_trace_t)          == 84',
                              'sizeof(bsp_trace_t)          == 80'), 1)):
            got = [g for g in compare_sizes(mutated, root)
                   if not g.startswith('SKIP: ')]
            fired = 1 if got else 0
            ok = fired == want
            print('  %-42s %s' % (label, 'ok' if ok else 'DID NOT FIRE'
                                  if want else 'FIRED ON CLEAN INPUT'))
            for g in got:
                print('      ' + g)
            if not ok:
                bad += 1
    else:
        print('  %-42s %s' % ('the two size controls', 'SKIPPED: no brain'))

    # ...and two for the staged writes: the tree's own text is clean, and
    # Gladiator's zeroing of an out-of-range byte -- the defect -- is reported.
    redir = read(os.path.join(REPO, 'src/bot/bl_redirgi.c'))
    zeroed = redir.replace('BotStageInt(BW_BYTE, c & 0xff);',
                           'BotStageInt(BW_BYTE, (c < 0 || c > 255) ? 0 : c);')
    for label, text, want in (('staged writes unmutated', redir, 0),
                              ("Gladiator's zeroed byte", zeroed, 1)):
        n += 1
        if want and zeroed == redir:
            print('  %-42s %s' % (label, 'ROTTED: the mutation no longer '
                                  'applies'))
            bad += 1
            continue
        got = [g for g in compare_staging(text) if not g.startswith('SKIP: ')]
        fired = 1 if got else 0
        ok = fired == want
        print('  %-42s %s' % (label, 'ok' if ok else 'DID NOT FIRE'
                              if want else 'FIRED ON CLEAN INPUT'))
        for g in got[:3]:
            print('      ' + g)
        if not ok:
            bad += 1

    # ...and four for the Quake III botlib: the convention, which is the
    # defect it shipped with, and three against the real checkout -- clean,
    # an export slot swapped in its header, and a size we assert that its
    # compiler does not agree with.  Skipped, out loud, without a checkout.
    for label, text, want in (
            ('GetBotAPI plain C', 'q2_bot_export_t *GetBotAPI(q2_bot_import_t *import);', 0),
            ('GetBotAPI __stdcall', '__declspec(dllexport) q2_bot_export_t * __stdcall '
             'GetBotAPI(q2_bot_import_t *import);', 1)):
        n += 1
        got = stdcall_findings(text, 'selftest')
        fired = 1 if got else 0
        ok = fired == want
        print('  %-42s %s' % (label, 'ok' if ok else 'DID NOT FIRE'
                              if want else 'FIRED ON CLEAN INPUT'))
        if not ok:
            bad += 1
    for label, text, want in (
            ('PFNGetBotAPI plain C',
             'typedef bot_export_t *(*PFNGetBotAPI)(bot_import_t *import);', 0),
            ('PFNGetBotAPI WINAPI',
             'typedef bot_export_t *(WINAPI *PFNGetBotAPI)(bot_import_t *import);', 1)):
        n += 1
        got = pointer_findings(text, 'selftest')
        fired = 1 if got else 0
        ok = fired == want
        print('  %-42s %s' % (label, 'ok' if ok else 'DID NOT FIRE'
                              if want else 'FIRED ON CLEAN INPUT'))
        if not ok:
            bad += 1
    q3root = find_q3()
    if os.path.exists(real) and os.path.exists(os.path.join(q3root, Q3_HEADER)):
        text = read(real)
        hdr = read(os.path.join(q3root, Q3_HEADER))
        swapped_hdr = hdr.replace(
            '    int  (*BotSetupLibrary)(void);\n    int  (*BotShutdownLibrary)(void);\n',
            '    int  (*BotShutdownLibrary)(void);\n    int  (*BotSetupLibrary)(void);\n')
        stdcall_hdr = hdr.replace('q2_bot_export_t *GetBotAPI(',
                                  'q2_bot_export_t * __stdcall GetBotAPI(')
        cases_q3 = (('Quake III botlib unmutated', text, hdr, 0),
                    ('Quake III export slots swapped', text, swapped_hdr, 1),
                    ('Quake III GetBotAPI __stdcall', text, stdcall_hdr, 1),
                    ('Quake III size disagreement', text.replace(
                        'sizeof(bot_input_t)          == 36',
                        'sizeof(bot_input_t)          == 40'), hdr, 1))
        for label, ours_t, hdr_t, want in cases_q3:
            n += 1
            if want and (ours_t == text and hdr_t == hdr):
                print('  %-42s %s' % (label, 'ROTTED: the mutation no longer applies'))
                bad += 1
                continue
            # The real check, as main() runs it; a SKIP is not a finding.
            got = [g for g in compare_q3(ours_t, q3root, hdr_t, sizes=hdr_t == hdr)
                   if not g.startswith('SKIP: ')]
            fired = 1 if got else 0
            ok = fired == want
            print('  %-42s %s' % (label, 'ok' if ok else 'DID NOT FIRE'
                                  if want else 'FIRED ON CLEAN INPUT'))
            for g in got[:3]:
                print('      ' + g.replace('\n', '\n  '))
            if not ok:
                bad += 1
    else:
        print('  %-42s %s' % ('the Quake III botlib controls',
                              'SKIPPED: no checkout'))

    print('botabi.py --selftest: %d control(s), %d wrong' % (n, bad))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ours', default=os.path.join(REPO, 'src/bot/botlib.h'))
    ap.add_argument('--brain', default=find_brain())
    ap.add_argument('--q3', default=find_q3())
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()

    if a.selftest:
        return selftest()

    # Needs no brain: it is our own redirection measured against the engine.
    redir = os.path.join(REPO, 'src/bot/bl_redirgi.c')
    staging = compare_staging(read(redir))
    for x in staging:
        print('  %s %s' % ('--' if x.startswith('SKIP: ') else '!!',
                           x[6:] if x.startswith('SKIP: ') else x))
    staged_bad = [x for x in staging if not x.startswith('SKIP: ')]
    if not staging:
        print('botabi.py: Bot_WriteByte and Bot_WriteShort stage what the '
              'engine would write, for %d value(s)' % len(STAGE_INPUTS))

    brain_hdr = os.path.join(a.brain, 'botlib', 'be_interface.h')
    if not os.path.exists(brain_hdr):
        # A checked-out brain is a convenience, not a dependency -- the same
        # rule audit.py applies to q2pro.  Absent means skipped and said so,
        # never a silent pass.
        # The SKIP form is the one tools/audit.py lists as "not applicable";
        # this prose used to exit 0 and be counted as a clean comparison.
        print('botabi.py: SKIP -- %s is absent, so the contract has nothing '
              'to be compared against (git submodule update --init '
              'vendor/gladiator-bot-restored); the staged-write half ran'
              % brain_hdr)
        return 1 if staged_bad else 0
    if not os.path.exists(a.ours):
        print('botabi.py: %s absent' % a.ours)
        return 1

    ours = read(a.ours)
    bad = compare(ours, read(brain_hdr))
    pub_hdr = os.path.join(a.brain, 'game', 'botlib.h')
    if os.path.exists(pub_hdr):
        bad += compare_exports(ours, read(pub_hdr))
    else:
        bad.append('%s absent: the export table has nothing to be compared '
                   'against' % pub_hdr)
    sizes = compare_sizes(ours, a.brain)
    skips = [x for x in sizes if x.startswith('SKIP: ')]
    bad += [x for x in sizes if not x.startswith('SKIP: ')]

    print('botabi.py: %s vs %s' % (os.path.relpath(a.ours, REPO), brain_hdr))
    for b in bad:
        print('  !! ' + b)
    for sk in skips:
        print('  -- ' + sk[6:])

    # The second botlib (R-BOT-31), and our own pointer to GetBotAPI, which
    # has to be plain C for both.
    q3bad = pointer_findings(read(os.path.join(REPO, 'src/bot/bl_main.c')),
                             'src/bot/bl_main.c')
    q3hdr = os.path.join(a.q3, Q3_HEADER)
    if os.path.exists(q3hdr):
        q3bad += compare_q3(ours, a.q3)
        print('botabi.py: %s vs %s' % (os.path.relpath(a.ours, REPO), q3hdr))
    else:
        print('botabi.py: SKIP -- %s is absent, so the Quake III botlib has '
              'nothing to be compared against (git submodule update --init '
              'vendor/q3a_bot_backport_for_q2)' % q3hdr)
    q3skips = [x for x in q3bad if x.startswith('SKIP: ')]
    q3bad = [x for x in q3bad if not x.startswith('SKIP: ')]
    for b in q3bad:
        print('  !! ' + b)
    for sk in q3skips:
        print('  -- ' + sk[6:])
    bad += q3bad
    if bad or staged_bad:
        return 1
    print('  the two sides agree on every slot both tables declare and on their '
          'order, on the condition that selects Trace%s'
          % ('' if skips else ', and on the size of every contract struct'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
