#!/usr/bin/env bash
#
# Syntax-check a single translation unit with the same flags probe.sh uses.
# Faster feedback than a full probe when porting one file at a time.
#
#   ./tools/check.sh src/GameEngine/gameobjects/PhysObj.cpp        # unique errors
#   ./tools/check.sh src/GameEngine/gameobjects/PhysObj.cpp -f     # full output
#
set -uo pipefail
cd "$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

CXX=${CXX:-i686-w64-mingw32-g++}
FILE="${1:?usage: check.sh <file.cpp> [-f]}"
FULL="${2:-}"

INCLUDES="-Isrc/Eternity/Include -Isrc/Eternity -Isrc/GameEngine -Isrc/EclipseStudio/Sources -Isrc/External -Isrc/External/dxsdk/Include -Isrc/External/Scaleform3/Include -Isrc/External/RakNet/Source -Isrc/ServerNetPackets -Isrc/External/PhysX/physx-include -Isrc/External/PhysX/pxshared-include -Isrc/External/PhysX/compat -Isrc/External/Recast/Detour/Include -Isrc/External/Recast/DetourCrowd/Include -Isrc/External/Recast/Recast/Include -Isrc/External/RmlUi/Include"
DEFINES="-DWIN32 -D_WINDOWS -DWO_SERVER -DPX_PHYSX_STATIC_LIB -DNDEBUG"
FLAGS="-std=c++20 -fsyntax-only -w -fms-extensions"

out=$($CXX $FLAGS $INCLUDES $DEFINES "$FILE" 2>&1)
n=$(grep -c 'error:' <<<"$out")

if [[ "$FULL" == "-f" ]]; then
  echo "$out"
else
  grep -E 'error:' <<<"$out" | sed 's/.*error: //' | sort -u
fi
echo "--- $n error(s) in $(basename "$FILE")"
