// Include Game specific funtions/definitions
#include "r3dPCH.h"
#include "r3d.h"

#include "r3dNetwork.h"

//#include "GameCommon.h"
#include "ServerGameLogic.h"
// [PORT] Was "../../../server/src/WO_GameServer/Sources/ObjectsCode/obj_ServerPlayer.h",
// which does not resolve relative to THIS file -- it walks up to server/src/ and then
// looks for server/src/ again. It only ever worked by falling through to the include
// path, where -Isrc/EclipseStudio/Sources happened to make it land in the right place.
//
// That made the include silently sensitive to include-path ORDER: adding any -I whose
// third parent contains a server/src/ redirected it, and in this tree the directory it
// redirects to is the ORIGINAL unported source. The two copies of obj_ServerPlayer.h
// differ -- the ported one is where the MSVC-isms were fixed -- so the failure mode was
// a wall of errors that had already been fixed months ago, in a file the compiler was
// not actually reading.
#include "../ObjectsCode/obj_ServerPlayer.h"

extern bool r3dOutToLog(const char* Str, ...);
