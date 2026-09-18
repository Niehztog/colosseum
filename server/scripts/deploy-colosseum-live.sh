#!/bin/bash
#
# deploy-colosseum-live.sh -- put colosseum behind q2admin in a LIVE gamedir.
#
# WHAT THIS REPLACES AND WHAT IT KEEPS.  A q2admin deployment loads q2admin as
# `gamei386.so` and the real mod behind it as `gamei386.real.so`.  Only the
# second one changes here: q2admin, the engine, anticheat, cvarban/cmdban and
# the q2admin-cloud link all stay exactly as they are.
#
#   arena   rocketarena2  -> colosseum, g_ruleset arena
#   xatrix  openffa       -> colosseum, g_ruleset dm + the xatrix content layer
#
# WHY A SCRIPT AND NOT A LIST OF COMMANDS.  A gamedir set up by hand is a
# gamedir nobody can rebuild -- when one is lost, the procedure has to be
# reconstructed from memory, and this file is that procedure instead.  It is
# also the only way the REHEARSAL can be trusted: point Q2_ROOT at a copy of the
# real tree, run this, and what is tested is what will be deployed rather than
# something resembling it.
#
#   Q2_ROOT=~/quake2-rehearsal ./deploy-colosseum-live.sh arena
#
# IDEMPOTENT.  Backups are taken once and never overwritten, the server1.cfg
# edits are matched before they are applied, and re-running installs the same
# files again rather than a second copy of them.
#
# THE BOT STACK IS FOUR THINGS AND ALL FOUR ARE INSTALLED HERE: the library
# itself, `gladiator.so`, `pak7.pak`, and a `.aas` navigation mesh per map.
# The meshes are NOT recomputed -- they are the ones the test rig already
# built, and they are copied rather than regenerated because computing them
# again is hours of work for a byte-identical result.
#
# AND THE MESHES NEED THE .bsp BESIDE THEM.  The botlib does its own file I/O
# and searches ONLY <basedir>/<gamedir>/maps/ -- not baseq2, and not the paks
# the engine would happily load the map from.  On xatrix almost every rotation
# map lives in baseq2 or inside a pak, so each one is installed loose into the
# gamedir.  Verified byte-identical to the source first; this changes which
# FILE the engine opens, and it must not change WHAT it opens.

set -euo pipefail

# WHERE THINGS ARE.  All four are the caller's to set; the defaults are only
# the conventional layout.
Q2_ROOT="${Q2_ROOT:-$HOME/quake2}"                          # the live game data
SERVER_ROOT="${SERVER_ROOT:-$HOME/quake2-colosseum-server}" # where the meshes were built
COLOSSEUM_SRC="${COLOSSEUM_SRC:-$HOME/projects/colosseum}"  # the clone, for configs/
IMAGE="${IMAGE:-colosseum}"                                 # --target colosseum, built

# WHAT A PARTICULAR SERVER ADDS, and none of it is written into this script.
# Which content layer, which rotation, which MOTD and what the server calls
# itself are that deployment's choices, so they arrive as files and values from
# whoever runs this -- keeping this repository free of any one server's config.
# All are optional; with none of them set this installs the library, the bot
# stack and the meshes, and writes a colosseum.cfg that is nothing but the
# ruleset and the bot fill.
#
#   CFG_EXTRA       a file appended verbatim to the generated colosseum.cfg.
#                   Where `set xatrix 1`, `set arenacfg "..."`,
#                   `set map_file "..."` and any other per-server cvar goes.
#   MOTD_SRC        a MOTD installed as <gamedir>/motd.txt, only if none is
#                   there already.  Checked against the reader's real limits
#                   before it is written -- see below.
#   MAPLIST_SRC     a rotation, one map name per line, filtered into
#                   <gamedir>/maps.txt.  Comments and blank lines are dropped
#                   and only the first field of each line is kept, so an
#                   openffa `mapcfg/maplist.txt` can be handed over as-is.
#   DEAD_MAPS       names to drop from that rotation.  Not cosmetic, and the
#                   REASON is the deployment's rather than this script's -- two
#                   are known to matter and they are not the same fault.  A map
#                   the botlib cannot mesh ends the bots for the WHOLE SESSION,
#                   because it disables itself on the first one it cannot load
#                   a mesh for and never retries.  A map the ENGINE cannot load
#                   ends the game outright: a Com_Error on spawn unloads the
#                   library and leaves the engine up and answering nothing.
#                   Either way the maps stay installed and can be loaded by
#                   hand; only the rotation drops them.
#   INSTALL_BSP=1   install the mesh source's loose .bsp into the gamedir.
#                   Needed wherever the rotation's maps live in baseq2 or inside
#                   a pak, because the botlib searches only the gamedir (below).
#   MAXCLIENTS_MIN  raise server1.cfg's `set maxclients` to at least this.  The
#                   fill only hands a seat back to a player already connected,
#                   so a maxclients at or below the fill's target lets the bots
#                   take every slot and an arrival is refused.
#   HOSTNAME_SUFFIX appended once to server1.cfg's `set hostname`.
CFG_EXTRA="${CFG_EXTRA:-}"
MOTD_SRC="${MOTD_SRC:-}"
MAPLIST_SRC="${MAPLIST_SRC:-}"
DEAD_MAPS="${DEAD_MAPS:-}"
INSTALL_BSP="${INSTALL_BSP:-0}"
MAXCLIENTS_MIN="${MAXCLIENTS_MIN:-0}"
HOSTNAME_SUFFIX="${HOSTNAME_SUFFIX:-}"

STAMP="$(date +%Y%m%d)"

# WRITE VIA RENAME, NEVER IN PLACE.  `install` and `cp` open the destination
# with O_TRUNC, which rewrites the very inode a RUNNING server has its game
# library and paks mapped from -- the deployment would be modifying the code
# under the live process minutes before it is restarted.  rename(2) within the
# directory swaps the name onto a NEW inode instead and leaves the old one
# alive for whoever still has it open, which is the whole point.
ainstall() {   # ainstall <mode> <src> <dst>
    install -m "$1" "$2" "$3.new-$$" && mv -f "$3.new-$$" "$3"
}

log()  { printf '==> %s\n' "$*"; }
info() { printf '    %s\n' "$*"; }
die()  { printf 'error: %s\n' "$*" >&2; exit 1; }

# THE GAMEDIR IS A PATH, THE RULESET IS A GAME.  They are separate arguments
# because they are separate things: the gamedir is what clients download content
# under and where this server's own configs live, and colosseum runs any of its
# seven rulesets in any of them.
[ $# -ge 1 ] && [ $# -le 2 ] || die "usage: $(basename "$0") <gamedir> [ruleset]"
GD="$1"
RULESET="${2:-}"
if [ -z "$RULESET" ]; then
    case "$GD" in
        arena)  RULESET=arena ;;   # the conventional pairing, nothing more
        xatrix) RULESET=dm ;;
        *) die "no default ruleset for gamedir '$GD' -- pass one: $(basename "$0") $GD <arena|ctf|dm|dmpro|duel|sp|tdm>" ;;
    esac
fi
[ -f "$COLOSSEUM_SRC/colosseum/configs/$RULESET.cfg" ] \
    || die "no such ruleset: $RULESET (looked for $COLOSSEUM_SRC/colosseum/configs/$RULESET.cfg)"

DIR="$Q2_ROOT/$GD"
SRC="$SERVER_ROOT/$GD"

# ------------------------------------------------------------------ preflight
[ -d "$DIR" ]                  || die "no gamedir at $DIR"
[ -f "$DIR/gamei386.so" ]      || die "$DIR/gamei386.so is missing -- that is q2admin, and this deployment goes BEHIND it"
[ -f "$DIR/server1.cfg" ]      || die "no server1.cfg in $DIR"
[ -d "$SRC/maps" ]             || die "no mesh source at $SRC/maps"
command -v docker >/dev/null   || die "docker not found"
docker image inspect "$IMAGE" >/dev/null 2>&1 || die "no such image: $IMAGE (build it with: docker build --target colosseum --build-context colosseum-src=$COLOSSEUM_SRC -t $IMAGE .)"

# q2admin must be the thing in front, not colosseum from an earlier run of this
# script: if gamei386.so were colosseum, the engine would load colosseum
# directly and q2admin would be gone from the stack without a word.  q2admin
# exports GetGameAPI only; colosseum exports GetGameAPIEx as well, so the
# presence of the second is the tell.
if readelf --dyn-syms -W "$DIR/gamei386.so" 2>/dev/null | grep -q ' GetGameAPIEx$'; then
    die "$DIR/gamei386.so exports GetGameAPIEx -- that is colosseum, not q2admin. Restore q2admin there first."
fi

log "$GD: colosseum -> $DIR (ruleset $RULESET)"

# ------------------------------------------------- artifacts out of the image
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
docker run --rm -v "$TMP:/out" --entrypoint sh "$IMAGE" -c \
    'cp /opt/colosseum/gamei386.so /opt/colosseum/gladiator.so /opt/colosseum/pak7.pak /opt/colosseum/bots.cfg /opt/colosseum/commit /out/' \
    || die "could not copy the artifacts out of $IMAGE"
COMMIT="$(cat "$TMP/commit")"
info "colosseum $COMMIT"

readelf -h "$TMP/gamei386.so" | grep -q 'ELF32' || die "the image's gamei386.so is not ELF32"
readelf --dyn-syms -W "$TMP/gamei386.so" | grep -q ' GetGameAPI$' || die "the image's gamei386.so does not export GetGameAPI"

# ------------------------------------------------------------------- backups
for f in gamei386.real.so server1.cfg; do
    [ -f "$DIR/$f" ] || continue
    b="$DIR/$f.bak-$STAMP-pre-colosseum"
    if [ -e "$b" ]; then
        info "backup already exists, keeping it: $(basename "$b")"
    else
        cp -p "$DIR/$f" "$b"; info "backed up $f -> $(basename "$b")"
    fi
done

# ------------------------------------------------------------ the game library
# INSTALLED AS gamei386.real.so, which is the name q2admin's own q2admin.cfg
# names in `gamelibrary`.  Writing it to gamei386.so instead would overwrite
# q2admin and silently drop the whole admin layer -- and it would look like it
# worked, because colosseum boots and the bots run.
ainstall 755 "$TMP/gamei386.so"  "$DIR/gamei386.real.so"
ainstall 755 "$TMP/gladiator.so" "$DIR/gladiator.so"
ainstall 644 "$TMP/pak7.pak"     "$DIR/pak7.pak"
mkdir -p "$DIR/botcfg"
ainstall 644 "$TMP/bots.cfg"  "$DIR/botcfg/bots.cfg"
echo "$COMMIT" > "$DIR/colosseum-commit.txt"
info "installed gamei386.real.so, gladiator.so, pak7.pak, botcfg/bots.cfg"

# BEHIND q2admin THE BOT LIST MUST BE A LOOSE FILE.  q2admin does not forward
# the engine's filesystem extension, so colosseum falls back to plain stdio,
# and that arm cannot see inside a pak.  botcfg/bots.cfg above is that loose
# file; pak7.pak still supplies the bot CHARACTERS, which the botlib reads
# through its own file I/O rather than the engine's.
grep -q 'addbot' "$DIR/botcfg/bots.cfg" || die "botcfg/bots.cfg has no addbot lines"

# --------------------------------------------------------- colosseum's configs
ainstall 644 "$COLOSSEUM_SRC/colosseum/server.cfg" "$DIR/server.cfg"
mkdir -p "$DIR/configs"
for c in "$COLOSSEUM_SRC/colosseum/configs/"*.cfg; do ainstall 644 "$c" "$DIR/configs/$(basename "$c")"; done
info "installed server.cfg and configs/ ($(ls "$DIR/configs" | wc -l) rulesets)"

# ------------------------------------------------------------------- the meshes
mkdir -p "$DIR/maps"
n=0
for m in "$SRC/maps"/*.aas; do
    [ -e "$m" ] || continue
    ainstall 644 "$m" "$DIR/maps/$(basename "$m")"; n=$((n+1))
done
info "installed $n navigation mesh(es)"

# A mesh whose reachability lump is empty is bspc's raw geometry output, and it
# is WORSE than no mesh at all: it loads happily and leaves the bots with no
# navigation data, where a missing file at least fails loudly.
python3 - "$DIR/maps" <<'PY'
import struct, sys, os, glob
bad = []
for f in sorted(glob.glob(os.path.join(sys.argv[1], '*.aas'))):
    d = open(f, 'rb').read(120)
    if len(d) < 120 or struct.unpack_from('<i', d, 0)[0] != 0x53414145:
        bad.append(os.path.basename(f) + ':not-an-aas'); continue
    # lump 9 is reachability; bspc leaves it empty and the botlib fills it in.
    if struct.unpack_from('<ii', d, 8 + 9 * 8)[1] == 0:
        bad.append(os.path.basename(f) + ':geometry-only')
if bad:
    print('error: unusable mesh(es): ' + ', '.join(bad), file=sys.stderr)
    sys.exit(1)
PY
info "every mesh carries a reachability lump"

# --------------------------------------------------- the rotation's own maps
# THE BOTLIB SEARCHES ONLY <basedir>/<gamedir>/maps/, through its own file I/O,
# so a map the engine happily loads out of baseq2 or a pak has no mesh as far as
# the bots are concerned.  INSTALL_BSP=1 puts the mesh source's copies beside
# them.  cmp first: this changes which FILE the engine opens and must not change
# WHAT it opens, so an identical one already in place is left alone.
if [ "$INSTALL_BSP" = 1 ]; then
    copied=0; present=0
    for b in "$SRC/maps"/*.bsp; do
        [ -e "$b" ] || continue
        t="$DIR/maps/$(basename "$b")"
        if [ -f "$t" ] && cmp -s "$b" "$t"; then present=$((present+1)); continue; fi
        ainstall 644 "$b" "$t"; copied=$((copied+1))
    done
    info "rotation .bsp: $copied installed, $present already identical"
fi

# ------------------------------------------------------------- the rotation
# WRITTEN TO maps.txt, which under the OSP rulesets is what `map_file` names --
# it takes precedence over sv_maplist and has no length limit to hit at several
# hundred names, where sv_maplist does.  Under arena the rotation is the arena
# config's own maploop and MAPLIST_SRC is simply left unset.
if [ -n "$MAPLIST_SRC" ]; then
    [ -f "$MAPLIST_SRC" ] || die "no rotation at $MAPLIST_SRC"
    dropped=0
    {
        echo "# The rotation, from $(basename "$MAPLIST_SRC").  Written by"
        echo "# scripts/deploy-colosseum-live.sh; read through the map_file cvar."
        if [ -n "$DEAD_MAPS" ]; then
            echo "#"
            echo "# Deliberately absent from the rotation -- see DEAD_MAPS in the"
            echo "# deployment that called this script for why each one is here:"
            echo "#   $DEAD_MAPS"
        fi
        grep -vE '^\s*(#|$)' "$MAPLIST_SRC" | awk '{print $1}' | while read -r m; do
            case " $DEAD_MAPS " in *" $m "*) continue ;; esac
            echo "$m"
        done
    } > "$DIR/maps.txt"
    for m in $DEAD_MAPS; do
        grep -qxF "$m" "$MAPLIST_SRC" 2>/dev/null && dropped=$((dropped+1))
    done
    info "maps.txt: $(grep -cvE '^\s*(#|$)' "$DIR/maps.txt") entries ($dropped dropped)"
fi

# ------------------------------------------------------------------ the MOTD
# NINE ROWS OF THIRTY-TWO COLUMNS.  Under the OSP rulesets `motd_file` is read
# into char[9][33], and anything wider is cut at 32 with the rest of that line
# discarded -- silently, in the game, where nobody testing the deployment sees
# it.  The arena ruleset's own MOTD reader has neither limit, so a file written
# for one is not necessarily valid for the other; checking always is cheaper
# than remembering which reader this ruleset uses.  Written only if absent, so a
# hand-edited one is never clobbered.
if [ -n "$MOTD_SRC" ]; then
    [ -f "$MOTD_SRC" ] || die "no MOTD at $MOTD_SRC"
    if [ -f "$DIR/motd.txt" ]; then
        info "motd.txt already present, keeping it"
    else
        long=$(awk 'length($0)>32' "$MOTD_SRC" | wc -l)
        rows=$(grep -c '' "$MOTD_SRC")
        [ "$long" -eq 0 ] || die "$MOTD_SRC has $long row(s) wider than 32 columns"
        [ "$rows" -le 9 ] || die "$MOTD_SRC has $rows rows, the reader takes 9"
        ainstall 644 "$MOTD_SRC" "$DIR/motd.txt"
        info "wrote motd.txt ($rows rows, all within 32 columns)"
    fi
fi

# -------------------------------------------------------------- colosseum.cfg
# EXEC'd EARLY, from server1.cfg, and that is deliberate.  g_ruleset is LATCHED:
# it takes effect at the next map load, and server1.cfg loads the first map
# itself -- so a ruleset set from the override cfg the CMD execs afterwards
# would leave the server running its FIRST map under the wrong ruleset.  Early
# also means this server's own values, further down server1.cfg, still win over
# the ruleset defaults exec'd here.
cat > "$DIR/colosseum.cfg" <<CFG
// colosseum.cfg -- what makes this a colosseum server.  Written by
// scripts/deploy-colosseum-live.sh; exec'd from server1.cfg right after
// master.cfg, BEFORE this server's own gameplay values, which therefore win.
//
// colosseum $COMMIT

// Sets g_ruleset, the ruleset's own defaults, and botfill.  configs/$RULESET.cfg
// execs server.cfg first, which is colosseum's shared default surface.
exec configs/$RULESET.cfg
CFG

# THIS SERVER'S OWN CVARS, and they come from the deployment rather than from
# here: which content layer, which rotation file, which arena definitions.  A
# file, appended verbatim, so this script never has to know what is in it.
if [ -n "$CFG_EXTRA" ]; then
    [ -f "$CFG_EXTRA" ] || die "no such file: $CFG_EXTRA"
    printf '\n' >> "$DIR/colosseum.cfg"
    cat "$CFG_EXTRA" >> "$DIR/colosseum.cfg"
    info "appended $(basename "$CFG_EXTRA")"
fi

# THE FLAT BOT COUNT GOES BY TWO NAMES AND THIS IS THE THING TO GET RIGHT.
# `minimumplayers` is RA2's and is authoritative under arena and ctf;
# `bots_minplayers` is OSP's, under the four OSP rulesets.  BOTH are registered
# under EVERY ruleset, so the wrong one is accepted at the console and then
# silently ignored -- which is why it is chosen from the ruleset here rather
# than written down once per server and copied.
#
# It is zeroed because it and botfill are ALTERNATIVES, not companions:
# CheckMinimumPlayers takes the fill's computed target INSTEAD of the flat one.
case "$RULESET" in
    arena|ctf) FLAT=minimumplayers ;;
    sp)        FLAT= ;;               # nothing to fill in single player
    *)         FLAT=bots_minplayers ;;
esac
if [ -n "$FLAT" ]; then
    cat >> "$DIR/colosseum.cfg" <<CFG

// THE BOT FILL, ON, with the flat 1999 count zeroed beside it -- they are
// alternatives, not companions.  NOTE THE NAME: $FLAT is the authoritative one
// under $RULESET; the other is registered too and would be accepted here and
// then silently ignored.
set botfill 1
set $FLAT 0
CFG
    if [ "$RULESET" = arena ]; then
        cat >> "$DIR/colosseum.cfg" <<'CFG'

// Under arena the fill needs colosseum's "An empty arena server fills itself"
// or later, without which botfill leaves an empty arena server empty.
CFG
    fi
fi
info "wrote colosseum.cfg (ruleset $RULESET, flat count ${FLAT:-none})"

# ------------------------------------------------------------- server1.cfg edits
# IN PYTHON, BECAUSE THESE FILES ARE CRLF.  `sed -i 's|^exec master.cfg$|...|'`
# matches nothing on a line that really ends `master.cfg\r`, and sed reports no
# error when a substitution does not fire -- so the first version of this script
# announced a hostname change and an exec insertion it had not made, and only a
# read-back of the file caught it.  Each edit below is matched, applied and then
# verified, and the file keeps whatever line ending it arrived with.
CFGF="$DIR/server1.cfg"
python3 - "$CFGF" "$HOSTNAME_SUFFIX" "$MAXCLIENTS_MIN" <<'EDITS'
import re, sys

path, suffix, maxclients_min = sys.argv[1], sys.argv[2], int(sys.argv[3])
raw = open(path, 'rb').read().decode('latin-1')
nl = '\r\n' if '\r\n' in raw else '\n'
lines = raw.split(nl)
changed = []

# 1. exec colosseum.cfg, immediately after master.cfg.
if any(l.strip() == 'exec colosseum.cfg' for l in lines):
    print('    server1.cfg: exec colosseum.cfg already present')
else:
    for i, l in enumerate(lines):
        if l.strip() == 'exec master.cfg':
            lines[i:i+1] = [
                l, '',
                '// Colosseum: ruleset, content layer and bots.  Must come before the',
                '// `map` line below -- g_ruleset is latched and applies at the next',
                '// map load, and server1.cfg loads the first map itself.',
                'exec colosseum.cfg',
            ]
            changed.append('inserted exec colosseum.cfg after exec master.cfg')
            break
    else:
        sys.exit('error: server1.cfg has no "exec master.cfg" line to anchor to')

# 2. the hostname suffix, once.  Skipped entirely when the deployment set none.
for i, l in (enumerate(lines) if suffix else []):
    m = re.match(r'^(set\s+hostname\s+")(.*)(".*)$', l)
    if not m:
        continue
    head, name, tail = m.groups()
    if name.endswith(suffix):
        print('    hostname already suffixed: %s' % name)
    else:
        lines[i] = head + name + suffix + tail
        changed.append('hostname: %s%s' % (name, suffix))
    break
else:
    if suffix:
        sys.exit('error: server1.cfg has no "set hostname" line')

# 3. maxclients, raised to MAXCLIENTS_MIN if it is below it.  It has to stay
# ABOVE the fill's target: the fill only hands a seat back to a player who is
# already on the server, so at or below the target the bots take every slot and
# an arrival is refused.  Under dm that target is the MAP's spawn count, which
# is a property of the rotation and therefore the deployment's number to pick.
if maxclients_min:
    for i, l in enumerate(lines):
        m = re.match(r'^(set\s+maxclients\s+"?)(\d+)("?.*)$', l)
        if not m:
            continue
        head, val, tail = m.groups()
        if int(val) >= maxclients_min:
            print('    maxclients already %s' % val)
        else:
            lines[i] = head + str(maxclients_min) + tail
            changed.append('maxclients: %s -> %d (bot fill headroom)' % (val, maxclients_min))
        break
    else:
        sys.exit('error: server1.cfg has no "set maxclients" line')

if changed:
    open(path, 'wb').write(nl.join(lines).encode('latin-1'))
    for c in changed:
        print('    ' + c)

# Read back rather than trust the write.
back = open(path, 'rb').read().decode('latin-1')
assert 'exec colosseum.cfg' in back, 'exec colosseum.cfg did not survive the write'
if suffix:
    assert suffix in back, 'the hostname suffix did not survive the write'
if maxclients_min:
    m = re.search(r'^set\s+maxclients\s+"?(\d+)', back, re.M)
    assert m and int(m.group(1)) >= maxclients_min, 'maxclients did not survive the write'
EDITS

# ---------------------------------------------------------------------- report
echo
log "$GD ready"
info "library   $(readelf -h "$DIR/gamei386.real.so" | sed -n 's/.*Class:  *//p') $(du -h "$DIR/gamei386.real.so" | cut -f1)  colosseum $COMMIT"
info "meshes    $(ls "$DIR/maps"/*.aas 2>/dev/null | wc -l)"
info "maps      $(ls "$DIR/maps"/*.bsp 2>/dev/null | wc -l) loose .bsp"
info "hostname  $(tr -d '\r' < "$CFGF" | sed -n 's/^set hostname "\(.*\)".*/\1/p' | head -1)"
echo
info 'Restart the server to pick this up: bind-mounted files changed, not the'
info 'image, so a rebuild is not what is needed -- and "docker compose up -d"'
info 'sees no image change, reports "Running", and leaves the old library live.' 
