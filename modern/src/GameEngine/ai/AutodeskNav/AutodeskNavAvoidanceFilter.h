// COMPAT: Autodesk Navigation -> Recast & Detour.
//
// There is no counterpart to this file. DetourCrowd performs local avoidance as
// part of its update, so the separate avoidance filter disappears entirely; the
// results are read back off the crowd agent in RecastNavAgent::Update.
#pragma once
#include "AutodeskNavMesh.h"
