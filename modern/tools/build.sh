#!/usr/bin/env bash
#
# Configure and build, reporting each binary's real status.
#
# Reports COMPILE FAILED and LINK FAILED separately, because they are different
# problems and conflating them is actively misleading: grepping the build log only for
# "undefined reference" reports a target with zero unresolved symbols when in truth it
# never got as far as the linker.
#
# Targets are built in ONE invocation so the four link steps and everything still
# compiling can overlap -- building them one at a time left three of four cores idle
# through each serial link tail. Per-target diagnosis is still available and still
# exact: on failure the script re-runs the targets individually, which costs nothing in
# the common case because a failing build is not the common case (and because ccache
# makes the second pass nearly free).
#
# Usage:
#   ./tools/build.sh                  # everything
#   ./tools/build.sh SupervisorServer MasterServer
#
# Environment:
#   BUILD_DIR   build directory              (default: build)
#   JOBS        parallelism                  (default: nproc)
#   GENERATOR   CMake generator              (default: Ninja, falls back to make)
#   TOOLCHAIN   toolchain file               (default: cmake/toolchain-mingw-i686.cmake)
#
set -uo pipefail

MODERN_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$MODERN_DIR"

BUILD_DIR=${BUILD_DIR:-build}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 4)}
TOOLCHAIN=${TOOLCHAIN:-cmake/toolchain-mingw-i686.cmake}

# Ninja when it is present: it schedules across targets instead of recursing per
# directory, and its no-op build measures 0.06 s against make's 2.3 s of stat-ing 1303
# objects. Not required -- the build works under make, and tools/buildtime.sh checks it.
if [[ -z ${GENERATOR:-} ]]; then
  if command -v ninja >/dev/null 2>&1; then GENERATOR="Ninja"; else GENERATOR="Unix Makefiles"; fi
fi

TARGETS=("$@")
if [[ ${#TARGETS[@]} -eq 0 ]]; then
  TARGETS=(SupervisorServer MasterServer GameServer WarZ)
fi

if [[ ! -d "$BUILD_DIR" ]]; then
  cmake -B "$BUILD_DIR" -G "$GENERATOR" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" >/dev/null || exit 1
fi

log_dir="$BUILD_DIR/.buildlogs"; mkdir -p "$log_dir"

# Report one target's outcome from its log. Returns 0 if it produced a binary.
report() {
  local t=$1 rc=$2 log=$3 exe
  exe=$(find "$BUILD_DIR" -name "$t.exe" -newermt '-1 day' 2>/dev/null | head -1)

  if [[ $rc -eq 0 && -n "$exe" ]]; then
    printf '  %-18s LINKED    %s bytes\n' "$t" "$(stat -c%s "$exe")"
    return 0
  fi

  # Distinguish "never reached the linker" from "linked and had unresolved symbols".
  #
  # Test for undefined references FIRST. The linker's own failure line is
  # "collect2: error: ld returned 1 exit status", which matches a naive grep for
  # 'error:' and would report a pure link failure as a compile failure.
  local undef
  mapfile -t undef < <(grep -oE "undefined reference to \`[^']*'" "$log" \
                       | sed "s/undefined reference to .//;s/.$//" | sort -u)

  if [[ ${#undef[@]} -gt 0 ]]; then
    printf '  %-18s LINK FAILED     (%s undefined symbol(s))\n' "$t" "${#undef[@]}"
    printf '      %s\n' "${undef[@]:0:8}"
    [[ ${#undef[@]} -gt 8 ]] && echo "      ... and $(( ${#undef[@]} - 8 )) more"
  elif grep -qE '^[^:]+:[0-9]+:[0-9]+: error:' "$log"; then
    local n
    n=$(grep -cE '^[^:]+:[0-9]+:[0-9]+: error:' "$log")
    printf '  %-18s COMPILE FAILED  (%s error(s))\n' "$t" "$n"
    grep -m3 -E '^[^:]+:[0-9]+:[0-9]+: error:' "$log" | sed 's/^/      /'
  else
    printf '  %-18s FAILED          (see log)\n' "$t"
    grep -m3 -iE 'error|cannot find' "$log" | sed 's/^/      /'
  fi
  echo "      full log: $log"
  return 1
}

# Fast path: everything at once.
all_log="$log_dir/all.log"
cmake --build "$BUILD_DIR" --target "${TARGETS[@]}" -j"$JOBS" > "$all_log" 2>&1
rc=$?

if [[ $rc -eq 0 ]]; then
  rc_overall=0
  for t in "${TARGETS[@]}"; do
    report "$t" 0 "$all_log" || rc_overall=1
  done
  exit $rc_overall
fi

# Something broke. Re-run per target so each failure is attributed to the target that
# owns it rather than to whichever one happened to be in the log first.
echo "  (build failed -- re-running per target to attribute the failure)"
rc_overall=0
for t in "${TARGETS[@]}"; do
  log="$log_dir/$t.log"
  cmake --build "$BUILD_DIR" --target "$t" -j"$JOBS" > "$log" 2>&1
  report "$t" $? "$log" || rc_overall=1
done

exit $rc_overall
