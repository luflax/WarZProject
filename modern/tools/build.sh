#!/usr/bin/env bash
#
# Configure and build, reporting each binary's real status.
#
# Reports COMPILE FAILED and LINK FAILED separately, because they are different
# problems and conflating them is actively misleading: grepping the build log only for
# "undefined reference" reports a target with zero unresolved symbols when in truth it
# never got as far as the linker.
#
# Usage:
#   ./tools/build.sh                  # everything
#   ./tools/build.sh SupervisorServer MasterServer
#
set -uo pipefail

MODERN_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$MODERN_DIR"

BUILD_DIR=${BUILD_DIR:-build}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 4)}
TOOLCHAIN=${TOOLCHAIN:-cmake/toolchain-mingw-i686.cmake}

TARGETS=("$@")
if [[ ${#TARGETS[@]} -eq 0 ]]; then
  TARGETS=(SupervisorServer MasterServer GameServer WarZ)
fi

if [[ ! -d "$BUILD_DIR" ]]; then
  cmake -B "$BUILD_DIR" -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" >/dev/null || exit 1
fi

log_dir="$BUILD_DIR/.buildlogs"; mkdir -p "$log_dir"
rc_overall=0

for t in "${TARGETS[@]}"; do
  log="$log_dir/$t.log"
  cmake --build "$BUILD_DIR" --target "$t" -j"$JOBS" > "$log" 2>&1
  rc=$?

  exe=$(find "$BUILD_DIR" -name "$t.exe" -newermt '-1 day' 2>/dev/null | head -1)

  if [[ $rc -eq 0 && -n "$exe" ]]; then
    printf '  %-18s LINKED    %s bytes\n' "$t" "$(stat -c%s "$exe")"
    continue
  fi

  rc_overall=1

  # Distinguish "never reached the linker" from "linked and had unresolved symbols".
  #
  # Test for undefined references FIRST. The linker's own failure line is
  # "collect2: error: ld returned 1 exit status", which matches a naive grep for
  # 'error:' and would report a pure link failure as a compile failure.
  mapfile -t undef < <(grep -oE "undefined reference to \`[^']*'" "$log" \
                       | sed "s/undefined reference to .//;s/.$//" | sort -u)

  if [[ ${#undef[@]} -gt 0 ]]; then
    printf '  %-18s LINK FAILED     (%s undefined symbol(s))\n' "$t" "${#undef[@]}"
    printf '      %s\n' "${undef[@]:0:8}"
    [[ ${#undef[@]} -gt 8 ]] && echo "      ... and $(( ${#undef[@]} - 8 )) more"
  elif grep -qE '^[^:]+:[0-9]+:[0-9]+: error:' "$log"; then
    n=$(grep -cE '^[^:]+:[0-9]+:[0-9]+: error:' "$log")
    printf '  %-18s COMPILE FAILED  (%s error(s))\n' "$t" "$n"
    grep -m3 -E '^[^:]+:[0-9]+:[0-9]+: error:' "$log" | sed 's/^/      /'
  else
    printf '  %-18s FAILED          (see log)\n' "$t"
    grep -m3 -iE 'error|cannot find' "$log" | sed 's/^/      /'
  fi
  echo "      full log: $log"
done

exit $rc_overall
