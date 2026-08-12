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

BASE_INCLUDES="-Isrc/Eternity/Include -Isrc/Eternity -Isrc/GameEngine -Isrc/EclipseStudio/Sources -Isrc/External -Isrc/External/dxsdk/Include -Isrc/External/Scaleform3/Include -Isrc/External/ChilKat/Include -Isrc/External/Steam -Isrc/External/ts3_sdk_3/include -Isrc/External/GameBlocks -Isrc/External/RakNet/Source -Isrc/ServerNetPackets -Isrc/External/PhysX/physx-include -Isrc/External/PhysX/pxshared-include -Isrc/External/PhysX/compat -Isrc/External/Recast/Detour/Include -Isrc/External/Recast/DetourCrowd/Include -Isrc/External/Recast/Recast/Include -Isrc/External/RmlUi/Include"

# Includes and defines are per-binary; keep this in step with tools/probe.sh, which
# carries the full explanation.
case "$FILE" in
  */WO_GameServer/*)    INCLUDES="$BASE_INCLUDES -Iserver/src/WO_GameServer/Sources" ;;
  */MasterServer/*)     INCLUDES="$BASE_INCLUDES -Iserver/src/MasterServer/Sources" ;;
  */SupervisorServer/*) INCLUDES="$BASE_INCLUDES -Iserver/src/SupervisorServer/Sources -Iserver/src/MasterServer/Sources" ;;
  *)                    INCLUDES="$BASE_INCLUDES" ;;
esac

case "$FILE" in
  *EclipseStudio*)      BINARY_DEFINES="" ;;
  */WO_GameServer/*)    BINARY_DEFINES="-DWO_SERVER -D_CRT_SECURE_NO_WARNINGS -DKY_BUILD_SHIPPING" ;;
  */MasterServer/*|*/SupervisorServer/*)
                        BINARY_DEFINES="-DWO_SERVER -DDISABLE_PHYSX" ;;
  *GameEngine/gameobjects/obj_Vehicle.cpp|*GameEngine/gameobjects/VehicleManager.cpp)
                        BINARY_DEFINES="" ;;   # listed only in Studio.vcxproj
  *)                    BINARY_DEFINES="-DWO_SERVER" ;;   # smallest surface
esac
DEFINES="-DWIN32 -D_WINDOWS $BINARY_DEFINES -DPX_PHYSX_STATIC_LIB -DNDEBUG"
FLAGS="-std=c++20 -fsyntax-only -w -fms-extensions"

out=$($CXX $FLAGS $INCLUDES $DEFINES "$FILE" 2>&1)
n=$(grep -c 'error:' <<<"$out")

if [[ "$FULL" == "-f" ]]; then
  echo "$out"
else
  grep -E 'error:' <<<"$out" | sed 's/.*error: //' | sort -u
fi
echo "--- $n error(s) in $(basename "$FILE")"
