// COMPAT: Autodesk Navigation -> Recast & Detour.
//
// Kynapse nav profiles map onto dtQueryFilter (area costs + include/exclude flags).
// Only the super-zombie profile id is referenced by game code, and that lives on
// RecastNavMesh as m_NavProfileIdSuperZombie.
#pragma once
#include "AutodeskNavMesh.h"
