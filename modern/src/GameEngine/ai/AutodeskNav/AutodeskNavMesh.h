//=========================================================================
//  COMPAT: Autodesk Navigation -> Recast & Detour
//
//  Autodesk discontinued Gameware Navigation (Kynapse); it cannot be licensed.
//  The navigation layer now runs on Recast & Detour (zlib) -- see
//  ../RecastNav/RecastNavMesh.h.
//
//  This header keeps the old include path and the old type names working, so
//  AI_Brain, AI_Tactics, obj_Zombie, sobj_Zombie and ZombieNavAgent compile
//  unchanged. Renaming the call sites is a follow-up, not a porting concern.
//=========================================================================

#pragma once

#include "../RecastNav/RecastNavMesh.h"

typedef RecastNavMesh  AutodeskNavMesh;
typedef RecastNavAgent AutodeskNavAgent;

// The old global was gAutodeskNavMesh; it is now gRecastNavMesh.
#define gAutodeskNavMesh gRecastNavMesh
