#!/usr/bin/env bash
#
# Compile-probe driver for the C++20 port.
#
# Syntax-checks translation units with MinGW-GCC and tallies pass/fail, so shim and
# conformance work is driven by real compiler errors rather than guesswork. This is
# NOT the build -- see CMakeLists.txt. It is the phase 1 feedback loop.
#
# Runs jobs in parallel (defaults to nproc); a serial pass over the engine takes
# minutes, which is too slow to iterate against.
#
# Usage:
#   ./tools/probe.sh <dir-or-glob> [-v]     # -v prints the first error per file
#
# Env:
#   CXX=<compiler>   default i686-w64-mingw32-g++
#   JOBS=<n>         default nproc
#
set -uo pipefail

MODERN_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$MODERN_DIR"

CXX=${CXX:-i686-w64-mingw32-g++}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 4)}
TARGET="${1:-src/Eternity/Source}"
VERBOSE=0
[[ "${2:-}" == "-v" ]] && VERBOSE=1

INCLUDES="-Isrc/Eternity/Include -Isrc/Eternity -Isrc/GameEngine -Isrc/EclipseStudio/Sources -Isrc/External -Isrc/External/dxsdk/Include -Isrc/External/Scaleform3/Include -Isrc/External/RakNet/Source -Isrc/ServerNetPackets -Isrc/External/PhysX/physx-include -Isrc/External/PhysX/pxshared-include -Isrc/External/PhysX/compat"

# WO_SERVER strips rendering. PhysX 4.1 is now vendored (BSD-3), so DISABLE_PHYSX is
# no longer set -- the real SDK headers are used. PX_PHYSX_STATIC_LIB avoids dllimport
# decoration, which matters for a headers-only compile check.
DEFINES="-DWIN32 -D_WINDOWS -DWO_SERVER -DPX_PHYSX_STATIC_LIB -DNDEBUG"
# No -fpermissive: it masked ~70 real conformance errors behind one root cause
# (Tsg_stl/TString.h calling unqualified r3dTL::Max). Strict is the honest gate.
FLAGS="-std=c++20 -fsyntax-only -w -fms-extensions"

# Files present on disk but NOT listed in the original .vcxproj -- dead legacy that
# was never compiled. Verified by diffing Eternity.vcxproj's <ClCompile> entries
# against Source/: r3dObj_OLDRender.cpp is the only one in the engine.
EXCLUDE_RE='r3dObj_OLDRender\.cpp'

if [[ -d "$TARGET" ]]; then
  mapfile -t FILES < <(find "$TARGET" -name '*.cpp' -o -name '*.CPP' | grep -Ev "$EXCLUDE_RE" | sort)
else
  mapfile -t FILES < <(compgen -G "$TARGET" | grep -Ev "$EXCLUDE_RE" || true)
fi

OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

export CXX FLAGS INCLUDES DEFINES OUT
probe_one() {
  local f="$1"
  local tag; tag=$(echo "$f" | tr '/' '_')
  if err=$($CXX $FLAGS $INCLUDES $DEFINES "$f" 2>&1); then
    echo "PASS $f" > "$OUT/$tag"
  else
    { echo "FAIL $f"; grep -m1 -E "error:|fatal error:" <<<"$err"; } > "$OUT/$tag"
  fi
}
export -f probe_one

echo "Probing ${#FILES[@]} translation units ($JOBS jobs, $(basename "$CXX")) ..."
printf '%s\n' "${FILES[@]}" | xargs -P "$JOBS" -I{} bash -c 'probe_one "$@"' _ {}

pass=0; fail=0
declare -A ERRKIND
for r in "$OUT"/*; do
  [[ -f "$r" ]] || continue
  if head -1 "$r" | grep -q '^PASS'; then
    ((pass++))
  else
    ((fail++))
    first=$(sed -n '2p' "$r")
    kind=$(sed -E 's/.*(fatal error|error): //' <<<"$first" | cut -c1-72)
    ERRKIND["$kind"]=$(( ${ERRKIND["$kind"]:-0} + 1 ))
    [[ $VERBOSE == 1 ]] && printf '  FAIL %-46s %s\n' "$(basename "$(head -1 "$r" | cut -d' ' -f2)")" "$first"
  fi
done

echo
echo "======================================================"
printf "  pass: %3d   fail: %3d   total: %3d  (%d%% passing)\n" \
       "$pass" "$fail" "${#FILES[@]}" $(( ${#FILES[@]} ? pass*100/${#FILES[@]} : 0 ))
echo "======================================================"

if (( fail > 0 )); then
  echo
  echo "Distinct blockers (count, first error):"
  for k in "${!ERRKIND[@]}"; do printf '%4d  %s\n' "${ERRKIND[$k]}" "$k"; done | sort -rn | head -25
fi
