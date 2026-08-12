// COMPAT: Autodesk Navigation -> Recast & Detour.
//
// Kynapse required "bridge" objects to expose engine collision/rendering to its
// generator and visual debugger. Recast consumes raw triangles at cook time and
// Detour has no debug server, so none of it has a counterpart.
#pragma once
#include "AutodeskNavMesh.h"
