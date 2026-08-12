#!/usr/bin/env bash
#
# Measure what this build actually costs, so that claims about it stay checkable.
#
# Four numbers, because they are four different problems with four different fixes:
#
#   null build     -- how long the build system takes to decide there is nothing to do.
#                     Fixed by the generator (Ninja vs make), nothing else.
#   one .cpp       -- the floor for any edit. Fixed by precompiled headers.
#   one header     -- fan-out. Fixed only by header hygiene; PCH scales it but does not
#                     change the shape.
#   cold build     -- branch switches and flag changes. Fixed by ccache.
#
# Baseline on 4 cores, before any of this work, for comparison:
#
#   null 2.3 s | one .cpp 6.3 s | P2PMessages.h 1m42s | cold 18.9 min
#
# The per-edit timings run with CCACHE_DISABLE=1 on purpose. Touching a file without
# changing it is a guaranteed ccache hit, which would report a tenth of a second and
# mean nothing -- a real edit changes content and always misses. The cache is measured
# where it genuinely applies, on the cold rebuild.
#
# Usage:
#   ./tools/buildtime.sh                    # the three fast measurements
#   ./tools/buildtime.sh --cold             # ...and a from-scratch build (~10-20 min)
#   ./tools/buildtime.sh --verify           # PCH changes no object file
#
set -uo pipefail

MODERN_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$MODERN_DIR"

BUILD_DIR=${BUILD_DIR:-build}
REF_DIR=${REF_DIR:-build-reference}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 4)}
TOOLCHAIN=${TOOLCHAIN:-cmake/toolchain-mingw-i686.cmake}

TU=src/EclipseStudio/Sources/RENDERING/Deffered/PFX_AnaglyphComposite.cpp
HDR=src/EclipseStudio/Sources/multiplayer/P2PMessages.h
PCH_HDR=src/Eternity/Include/r3dPCH.h

DO_COLD=0
DO_VERIFY=0
for arg in "$@"; do
  case "$arg" in
    --cold)   DO_COLD=1 ;;
    --verify) DO_VERIFY=1 ;;
    *) echo "unknown option: $arg" >&2; exit 2 ;;
  esac
done

# bc's `scale` governs division only -- a subtraction keeps the operands' precision, so
# "scale=1; $e - $s" on two nanosecond timestamps prints nine decimal places. Round in
# printf instead.
secs() { local s=$1 e=$2; printf '%.1f' "$(echo "$e - $s" | bc)"; }
fmt()  { printf '%dm%02ds' $(( ${1%.*} / 60 )) $(( ${1%.*} % 60 )); }

# ---------------------------------------------------------------------------
# --verify: neither the precompiled header nor ccache may change a single byte of output.
#
# This is the whole safety argument for both. They are caching mechanisms; if either
# changes what is compiled, something is wrong -- a macro from r3dPCH.h reaching a file
# that previously did not see it, most likely.
#
# It is not a hypothetical. Setting CCACHE_BASEDIR, which is the standard advice for
# sharing a cache between checkouts, rewrote absolute paths to relative ones on the way
# to the compiler and changed __FILE__ in 621 of 1303 objects. Correct output, wrong
# principle, and caught only by running this. See cmake/Speed.cmake.
# ---------------------------------------------------------------------------
if [[ $DO_VERIFY -eq 1 ]]; then
  echo "=== VERIFY: precompiled headers change no output ==="

  if [[ ! -d "$REF_DIR" ]]; then
    echo "  building reference tree (PCH and ccache both off) in $REF_DIR ..."
    cmake -B "$REF_DIR" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
          -DWARZ_PCH=OFF -DWARZ_CCACHE=OFF >/dev/null || exit 1
    cmake --build "$REF_DIR" -j"$JOBS" >/dev/null 2>&1
  fi
  [[ -d "$BUILD_DIR" ]] || { echo "  no $BUILD_DIR -- run tools/build.sh first"; exit 1; }

  # The translation units that expand __TIME__ differ between any two builds started in
  # different seconds, and that is not evidence of anything. There are exactly three:
  #
  #   Main.cpp      -- the only TU that includes src/Eternity/SF/Version.h
  #   VersionNo.cpp -- once for SupervisorServer, once for MasterServer
  #
  # Everything else must match byte for byte.
  declare -a TIME_MACRO_TUS=(/Main.cpp.obj /VersionNo.cpp.obj)

  same=0; diff=0; only=0
  while read -r obj; do
    rel=${obj#"$REF_DIR"/}
    other="$BUILD_DIR/$rel"
    if [[ ! -f "$other" ]]; then only=$((only+1)); continue; fi
    skip=0
    for pat in "${TIME_MACRO_TUS[@]}"; do [[ "$rel" == *"$pat"* ]] && skip=1; done
    [[ $skip -eq 1 ]] && continue
    if cmp -s "$obj" "$other"; then same=$((same+1)); else
      diff=$((diff+1))
      [[ $diff -le 5 ]] && echo "    DIFFERS: $rel"
    fi
  done < <(find "$REF_DIR" -name '*.obj' -o -name '*.o' | sort)

  printf '  %d identical, %d differing, %d only in the reference tree\n' "$same" "$diff" "$only"

  # A source that grows an #include "r3dPCH.h" later should stop being an exception, and
  # one that loses it should become one. Neither is visible without checking.
  echo "  PCH exclusion list:"
  expected=$(grep -oE '\$\{WARZ_ROOT\}/[^ ]+\.cpp' cmake/Pch.cmake | sed 's|${WARZ_ROOT}/||' | sort)
  actual=$(cat cmake/sources/*.cmake \
           | grep -oE '\$\{WARZ_ROOT\}/[^ ]+\.cpp' | sed 's|${WARZ_ROOT}/||' | sort -u \
           | while read -r f; do grep -qE '#include[[:space:]]*"r3dPCH\.h"' "$f" || echo "$f"; done | sort)
  if [[ "$expected" == "$actual" ]]; then
    echo "    matches the $(echo "$actual" | wc -l) sources that lack #include \"r3dPCH.h\""
  else
    echo "    STALE -- cmake/Pch.cmake disagrees with the tree:"
    diff <(echo "$expected") <(echo "$actual") | sed 's/^/      /'
  fi

  [[ $diff -eq 0 ]] || exit 1
  echo
fi

# ---------------------------------------------------------------------------
# Timings
# ---------------------------------------------------------------------------
echo "=== TIMINGS  (jobs: $JOBS, build dir: $BUILD_DIR) ==="

if [[ $DO_COLD -eq 1 ]]; then
  rm -rf "$BUILD_DIR"
  s=$(date +%s)
  cmake -B "$BUILD_DIR" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" >/dev/null || exit 1
  CCACHE_DISABLE=1 cmake --build "$BUILD_DIR" -j"$JOBS" >/dev/null 2>&1
  e=$(date +%s); cold=$(secs "$s" "$e")

  # Same build again from scratch, this time allowed to use the cache it just filled.
  # This is the number that matters for branch switches, which is what a clean build
  # actually is most of the time.
  rm -rf "$BUILD_DIR"
  s=$(date +%s)
  cmake -B "$BUILD_DIR" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" >/dev/null || exit 1
  cmake --build "$BUILD_DIR" -j"$JOBS" >/dev/null 2>&1
  e=$(date +%s); warm=$(secs "$s" "$e")

  printf '  %-34s %s\n' "cold build (no cache)" "$(fmt "$cold")"
  printf '  %-34s %s\n' "cold build (warm ccache)" "$(fmt "$warm")"
fi

[[ -d "$BUILD_DIR" ]] || { echo "  no $BUILD_DIR -- run tools/build.sh first"; exit 1; }

# Make sure the tree is fully built, or the first measurement below inherits the
# leftovers of whatever was pending and reports them as its own cost.
cmake --build "$BUILD_DIR" -j"$JOBS" >/dev/null 2>&1

s=$(date +%s.%N); cmake --build "$BUILD_DIR" -j"$JOBS" >/dev/null 2>&1; e=$(date +%s.%N)
printf '  %-34s %ss\n' "null build" "$(secs "$s" "$e")"

# Sub-second resolution here specifically: post-PCH this is a couple of seconds, and
# whole seconds cannot tell 1.4 from 2.4 -- which is the difference between "PCH is
# working" and "PCH is not working".
touch "$TU"
s=$(date +%s.%N); CCACHE_DISABLE=1 cmake --build "$BUILD_DIR" --target WarZ -j"$JOBS" >/dev/null 2>&1; e=$(date +%s.%N)
printf '  %-34s %ss\n' "one .cpp + relink" "$(secs "$s" "$e")"

touch "$HDR"
s=$(date +%s); CCACHE_DISABLE=1 cmake --build "$BUILD_DIR" --target WarZ -j"$JOBS" >/dev/null 2>&1; e=$(date +%s)
printf '  %-34s %s\n' "$(basename "$HDR") (fan-out)" "$(fmt "$(secs "$s" "$e")")"

touch "$PCH_HDR"
s=$(date +%s); CCACHE_DISABLE=1 cmake --build "$BUILD_DIR" --target WarZ -j"$JOBS" >/dev/null 2>&1; e=$(date +%s)
printf '  %-34s %s\n' "$(basename "$PCH_HDR") (rebuilds the PCH)" "$(fmt "$(secs "$s" "$e")")"

# ---------------------------------------------------------------------------
# Health checks that only mean something right after a build
# ---------------------------------------------------------------------------
echo
echo "=== HEALTH ==="
logs="$BUILD_DIR/.buildlogs"
if [[ -d "$logs" ]]; then
  printf '  %-34s %s\n' "invalid-pch warnings" "$(grep -rc 'invalid-pch' "$logs" 2>/dev/null | awk -F: '{s+=$2} END{print s+0}')"
  printf '  %-34s %s\n' "dropped attributes" "$(grep -rc 'attribute directive ignored' "$logs" 2>/dev/null | awk -F: '{s+=$2} END{print s+0}')"
fi
printf '  %-34s %s\n' "precompiled headers built" "$(find "$BUILD_DIR" -name '*.gch' | wc -l)"
printf '  %-34s %s\n' "PCH disk cost" "$(find "$BUILD_DIR" -name '*.gch' -exec du -ch {} + 2>/dev/null | tail -1 | cut -f1)"
if command -v ccache >/dev/null 2>&1; then
  # ccache 4.x prints "  Hits:  12 / 1317 ( 0.91%)" under "Cacheable calls"; the older
  # "cache hit rate" line is gone, which is why this greps for Hits rather than a rate.
  printf '  %-34s %s\n' "ccache hits" "$(ccache -s 2>/dev/null | grep -m1 'Hits:' | sed 's/^ *Hits: *//')"
fi
