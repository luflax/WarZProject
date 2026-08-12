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

INCLUDES="-Isrc/Eternity/Include -Isrc/Eternity -Isrc/GameEngine -Isrc/EclipseStudio/Sources -Isrc/External -Isrc/External/dxsdk/Include -Isrc/External/Scaleform3/Include -Isrc/External/ChilKat/Include -Isrc/External/Steam -Isrc/External/ts3_sdk_3/include -Isrc/External/RakNet/Source -Isrc/ServerNetPackets -Isrc/External/PhysX/physx-include -Isrc/External/PhysX/pxshared-include -Isrc/External/PhysX/compat -Isrc/External/Recast/Detour/Include -Isrc/External/Recast/DetourCrowd/Include -Isrc/External/Recast/Recast/Include -Isrc/External/RmlUi/Include"
# Defines depend on WHICH BINARY the file belongs to. EclipseStudio is the client
# and its headers hard-#error if WO_SERVER is set ("client weapon.h included in
# SERVER"); server/src is the server. Getting this wrong accounted for 22 of the
# first 94 EclipseStudio failures.
# GameEngine is shared, so two of its sources are chosen per file: obj_Vehicle.cpp and
# VehicleManager.cpp are listed only in Studio.vcxproj and pull in client headers.
# Keep this in step with tools/probe.sh.
case "$FILE" in
  *EclipseStudio*)                            BINARY_DEFINES="-DVEHICLES_ENABLED" ;;
  *server*)                                   BINARY_DEFINES="-DWO_SERVER" ;;
  *GameEngine/gameobjects/obj_Vehicle.cpp|*GameEngine/gameobjects/VehicleManager.cpp)
                                              BINARY_DEFINES="-DVEHICLES_ENABLED" ;;
  *)                                          BINARY_DEFINES="-DWO_SERVER" ;;   # smallest surface
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
