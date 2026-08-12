#!/usr/bin/env bash
#
# Copy source from the original tree into modern/, preserving the directory layout
# exactly so the codebase's deep relative includes keep resolving.
#
# Idempotent and re-runnable. NEVER writes to the original tree.
#
# Usage:  ./modern/tools/bootstrap.sh [--dry-run]
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODERN_DIR="$(dirname "$SCRIPT_DIR")"
ROOT_DIR="$(dirname "$MODERN_DIR")"

DRY_RUN=0
[[ "${1:-}" == "--dry-run" ]] && DRY_RUN=1

# Source trees copied verbatim. Layout is preserved on purpose: thousands of
# relative includes such as "../../External/fmod/fmod_event.hpp" depend on it.
TREES=(
  "src/Eternity"
  "src/GameEngine"
  "src/EclipseStudio"
  "src/ServerNetPackets"
  "server/src"
)

# RakNet is the one real dependency present in the original drop (BSD-2).
# Everything else under src/External/ is shimmed by hand and must not be clobbered.
VENDORED=(
  "src/External/RakNet"
)

# Prebuilt binaries, per-project build files, and art assets are excluded.
# CMake replaces every .vcxproj/.vcproj/.sln.
EXCLUDES=(
  --exclude='*.lib'   --exclude='*.dll'   --exclude='*.exe'   --exclude='*.pdb'
  --exclude='*.obj'   --exclude='*.ilk'   --exclude='*.exp'
  --exclude='*.vcproj*' --exclude='*.vcxproj*' --exclude='*.sln' --exclude='*.user'
  --exclude='*.ncb'   --exclude='*.suo'   --exclude='*.sdf'
  --exclude='*.png'   --exclude='*.ico'   --exclude='*.swf'   --exclude='*.jpg'
  --exclude='Debug'   --exclude='Release' --exclude='Final'  --exclude='.vs'
)

echo "Original tree : $ROOT_DIR"
echo "Target tree   : $MODERN_DIR"
[[ $DRY_RUN == 1 ]] && echo ">>> DRY RUN — nothing will be written"
echo

copy_tree() {
  local rel="$1"
  local src="$ROOT_DIR/$rel"
  local dst="$MODERN_DIR/$rel"

  if [[ ! -d "$src" ]]; then
    echo "  !! missing, skipped: $rel"
    return
  fi

  local n
  n=$(find "$src" -type f | wc -l)
  echo "  -> $rel ($n files)"

  [[ $DRY_RUN == 1 ]] && return

  # Wipe the destination first so the copy is idempotent and stale files from a
  # previous run cannot linger. Safe because every path in TREES/VENDORED is a
  # pure copy target -- the hand-written shims live elsewhere under src/External/
  # and are never touched.
  rm -rf "$dst"
  mkdir -p "$dst"

  # tar rather than rsync: universally available, and preserves the directory
  # layout the relative includes depend on.
  tar -cf - -C "$src" "${EXCLUDES[@]}" . | tar -xf - -C "$dst"
}

echo "Copying source trees:"
for t in "${TREES[@]}"; do copy_tree "$t"; done

echo
echo "Copying vendored third-party (permissive licences only):"
for v in "${VENDORED[@]}"; do copy_tree "$v"; done

echo
if [[ $DRY_RUN == 1 ]]; then
  echo "Dry run complete."
else
  echo "Bootstrap complete."
  echo "Shims under modern/src/External/ were left untouched."
  echo
  echo "Next:  cmake -S modern -B modern/build -DCMAKE_BUILD_TYPE=Debug"
fi
