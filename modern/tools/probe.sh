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
#   ./tools/probe.sh <dir-or-glob> [-v|--triage|--failed]
#
#     -v         print the first error per failing file
#     --triage   group failures by the file:line that ACTUALLY errored -- the
#                root-cause view. Fix N root causes per probe instead of one.
#     --failed   re-probe only the files that failed last run (cached in
#                .probe-failed). A full sweep is for checkpoints, not iteration.
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
VERBOSE=0; TRIAGE=0; ONLY_FAILED=0
case "${2:-}" in
  -v)       VERBOSE=1 ;;
  --triage) TRIAGE=1 ;;
  --failed) ONLY_FAILED=1 ;;
esac
# Per-target: a single shared cache replayed the wrong tree's failures.
CACHE=".probe-failed.$(echo "$TARGET" | tr "/" "_")"

INCLUDES="-Isrc/Eternity/Include -Isrc/Eternity -Isrc/GameEngine -Isrc/EclipseStudio/Sources -Isrc/External -Isrc/External/dxsdk/Include -Isrc/External/Scaleform3/Include -Isrc/External/ChilKat/Include -Isrc/External/Steam -Isrc/External/ts3_sdk_3/include -Isrc/External/RakNet/Source -Isrc/ServerNetPackets -Isrc/External/PhysX/physx-include -Isrc/External/PhysX/pxshared-include -Isrc/External/PhysX/compat -Isrc/External/Recast/Detour/Include -Isrc/External/Recast/DetourCrowd/Include -Isrc/External/Recast/Recast/Include -Isrc/External/RmlUi/Include"

# WO_SERVER strips rendering. PhysX 4.1 is now vendored (BSD-3), so DISABLE_PHYSX is
# no longer set -- the real SDK headers are used. PX_PHYSX_STATIC_LIB avoids dllimport
# decoration, which matters for a headers-only compile check.
# Defines depend on WHICH BINARY the file belongs to. EclipseStudio is the client
# and its headers hard-#error if WO_SERVER is set ("client weapon.h included in
# SERVER"); server/src is the server. Getting this wrong accounted for 22 of the
# first 94 EclipseStudio failures.
#
# GameEngine is shared, so its defines are chosen PER FILE rather than per tree: two of
# its sources (gameobjects/obj_Vehicle.cpp and VehicleManager.cpp) are listed only in
# Studio.vcxproj, never in WO_GameServer.vcxproj -- the server has its own obj_Vehicle
# under server/src/.../Vehicles/. They pull in client headers and must be probed as
# client code.
BASE_DEFINES="-DWIN32 -D_WINDOWS -DPX_PHYSX_STATIC_LIB -DNDEBUG"
CLIENT_ONLY_RE='GameEngine/gameobjects/(obj_Vehicle|VehicleManager)\.cpp$'

defines_for() {
  case "$1" in
    *EclipseStudio*) echo "$BASE_DEFINES -DVEHICLES_ENABLED"; return ;;
    *server*)        echo "$BASE_DEFINES -DWO_SERVER";        return ;;
  esac
  if [[ "$1" =~ $CLIENT_ONLY_RE ]]; then
    echo "$BASE_DEFINES -DVEHICLES_ENABLED"
  else
    echo "$BASE_DEFINES -DWO_SERVER"   # Eternity/GameEngine: smallest surface
  fi
}
# No -fpermissive: it masked ~70 real conformance errors behind one root cause
# (Tsg_stl/TString.h calling unqualified r3dTL::Max). Strict is the honest gate.
FLAGS="-std=c++20 -fsyntax-only -w -fms-extensions"

# Files present on disk but NOT listed in the original .vcxproj -- dead legacy that
# was never compiled. Verified by diffing Eternity.vcxproj's <ClCompile> entries
# against Source/: r3dObj_OLDRender.cpp is the only one in the engine.
# Kynapse wrapper implementations, superseded by ai/RecastNav/. Their headers remain
# as forwarding shims so call sites compile; the .cpp files have no counterpart.
EXCLUDE_RE='AutodeskNav/Autodesk.*\.cpp'

# Files the ORIGINAL Visual Studio projects never compiled -- dead legacy. Fixing
# one is pure waste, so they are excluded. Regenerate with tools/find_orphans.py.
if [[ -s .probe-orphans ]]; then
  ORPHAN_RE=$(paste -sd'|' .probe-orphans | sed 's/\./\\./g')
  EXCLUDE_RE="$EXCLUDE_RE|$ORPHAN_RE"
fi

if [[ $ONLY_FAILED == 1 && -s "$CACHE" ]]; then
  mapfile -t FILES < "$CACHE"
  echo "Re-probing ${#FILES[@]} previously-failing file(s) from $CACHE"
elif [[ -d "$TARGET" ]]; then
  mapfile -t FILES < <(find "$TARGET" -name '*.cpp' -o -name '*.CPP' | grep -Ev "$EXCLUDE_RE" | sort)
else
  mapfile -t FILES < <(compgen -G "$TARGET" | grep -Ev "$EXCLUDE_RE" || true)
fi

OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

export CXX FLAGS INCLUDES BASE_DEFINES CLIENT_ONLY_RE OUT
export -f defines_for
probe_one() {
  local f="$1"
  local tag; tag=$(echo "$f" | tr '/' '_')
  local defines; defines=$(defines_for "$f")
  if err=$($CXX $FLAGS $INCLUDES $defines "$f" 2>&1); then
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

# Cache the failure list so --failed can reuse it.
: > "$CACHE"
for r in "$OUT"/*; do
  [[ -f "$r" ]] || continue
  head -1 "$r" | grep -q '^FAIL' && head -1 "$r" | cut -d' ' -f2- >> "$CACHE"
done

if (( fail > 0 )); then
  echo
  if (( TRIAGE )); then
    # Group by the file:line that actually errored. One entry here is one fix,
    # however many TUs it takes down -- this is the view worth acting on.
    echo "ROOT CAUSES (files affected, location, message):"
    for r in "$OUT"/*; do
      [[ -f "$r" ]] || continue
      head -1 "$r" | grep -q '^FAIL' || continue
      sed -n '2p' "$r"
    done \
      | sed -E 's/^([^:]+:[0-9]+):[0-9]+: (fatal )?error: /\1 | /' \
      | sed -E "s/'[^']*'/X/g" \
      | sort | uniq -c | sort -rn | head -30
    echo
    echo "Fix the top entries first: each is ONE change."
  else
    echo "Distinct blockers (count, first error):"
    for k in "${!ERRKIND[@]}"; do printf '%4d  %s\n' "${ERRKIND[$k]}" "$k"; done | sort -rn | head -25
  fi
fi
