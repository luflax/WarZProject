#!/usr/bin/env bash
#
# Codegen probe -- like probe.sh, but emits real object files instead of
# syntax-checking.
#
# probe.sh answers "does this parse?". This answers "does this produce code?", which is
# the question Milestone B cares about, and it leaves .o files behind for linkcheck.sh
# to analyse.
#
# The one flag that matters is -msse2. Bare i686 has no SSE, so the engine's
# _mm_cvtss_si32 / _mm_set_ss calls fail to inline and codegen dies -- five files in
# Eternity alone. The original 2013 build assumed SSE2, so this restores the real
# target rather than relaxing anything.
#
# Usage:
#   ./tools/codegen.sh <dir|@listfile> [tag]
#
# Env:
#   CXX=<compiler>    default i686-w64-mingw32-g++
#   JOBS=<n>          default nproc
#   OBJDIR=<path>     default .objs
#   CONFIG=server|client
#                     Which configuration to build Eternity/GameEngine in. See below.
#   FORCE_BINARY=...  as probe.sh
#
# CONFIG exists because Eternity and GameEngine are compiled TWICE across the product.
# probe.sh gives them -DWO_SERVER (the smallest surface that syntax-checks), but the
# client links them without it, and WO_SERVER #ifdefs out code the client calls --
# UIimEdit.cpp defines 8 imgui_DrawList symbols as client code and 0 as server code.
# A link needs each library built in the configuration its consumer actually uses.
#
set -uo pipefail

MODERN_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$MODERN_DIR"

CXX=${CXX:-i686-w64-mingw32-g++}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 4)}
OBJDIR=${OBJDIR:-.objs}
CONFIG=${CONFIG:-server}
TARGET="${1:?usage: codegen.sh <dir|@listfile> [tag]}"
TAG="${2:-$(echo "$TARGET" | tr '/@' '__')}"

# Reuse probe.sh's include and define tables verbatim rather than copying them, so the
# two tools cannot drift apart.
eval "$(sed -n '/^BASE_INCLUDES=/,/^}$/p' tools/probe.sh)"
eval "$(sed -n '/^BASE_DEFINES=/,/^}$/p'  tools/probe.sh)"

# Same dead-file exclusions as probe.sh.
EXCLUDE_RE='AutodeskNav/Autodesk.*\.cpp'
if [[ -s .probe-orphans ]]; then
  EXCLUDE_RE="$EXCLUDE_RE|$(paste -sd'|' .probe-orphans | sed 's/\./\\./g')"
fi

if [[ "$TARGET" == @* ]]; then
  mapfile -t FILES < "${TARGET#@}"
elif [[ -d "$TARGET" ]]; then
  mapfile -t FILES < <(find "$TARGET" -name '*.cpp' -o -name '*.CPP' | grep -Ev "$EXCLUDE_RE" | sort)
else
  mapfile -t FILES < <(compgen -G "$TARGET" | grep -Ev "$EXCLUDE_RE" || true)
fi

OUT="$OBJDIR/$TAG"
rm -rf "$OUT"; mkdir -p "$OUT"

compile_one() {
  local f="$1"
  local o="$OUT/$(echo "$f" | tr '/' '_').o"
  local defs
  if [[ "$CONFIG" == client ]]; then
    # No WO_SERVER: the client configuration of Eternity/GameEngine.
    defs="$BASE_DEFINES"
  else
    defs="$(defines_for "$f")"
  fi
  if $CXX -std=c++20 -w -fms-extensions -msse2 \
       $defs $(includes_for "$f") -c "$f" -o "$o" 2> "$o.log"
  then echo "PASS $f"; else echo "FAIL $f"; fi
}
export -f compile_one defines_for includes_for
export CXX OUT CONFIG BASE_INCLUDES BASE_DEFINES CLIENT_ONLY_RE
export FORCE_BINARY="${FORCE_BINARY:-}"

printf '%s\n' "${FILES[@]}" \
  | xargs -P "$JOBS" -I{} bash -c 'compile_one "$@"' _ {} > "$OUT/results.txt" 2>&1

pass=$(grep -c '^PASS' "$OUT/results.txt")
fail=$(grep -c '^FAIL' "$OUT/results.txt")
echo "=== [$TAG, $CONFIG] $pass pass / $fail fail (of $((pass+fail))) -> $OUT ==="
grep '^FAIL' "$OUT/results.txt" | sed 's/^FAIL /  /'
