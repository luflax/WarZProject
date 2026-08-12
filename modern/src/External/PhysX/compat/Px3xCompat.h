// PhysX 3.x -> 4.1 source compatibility layer
//
// The codebase targets PhysX 3.x; the vendored SDK is 4.1 (the earliest BSD-3
// release, and the closest API to 3.x). This header reinstates the 3.x spellings
// that 4.1 renamed or dropped, so call sites compile unchanged.
//
// Included from r3dPCH.h immediately after PxPhysicsAPI.h.
//
// Anything that cannot be aliased -- because the 4.1 replacement has genuinely
// different semantics -- is listed at the bottom and must be ported by hand.

#pragma once

#include "PxPhysicsAPI.h"
#include "pvd/PxPvd.h"
#include "pvd/PxPvdTransport.h"

// ---------------------------------------------------------------------------
// PVD: rewritten in PhysX 4. 3.x had namespace PVD + PvdConnection reached via
// PxPhysics::getPvdConnectionManager(); 4.1 has PxPvd + PxPvdTransport.
// ---------------------------------------------------------------------------

namespace PVD
{
    typedef physx::PxPvd PvdConnection;
}

namespace physx
{
    // Removed in PhysX 4 -- the profile zone manager is gone entirely. Declared
    // (never defined) so pointer members keep compiling; nothing dereferences it.
    class PxProfileZoneManager;

    // ---------------------------------------------------------------------
    // Scene query types were renamed PxSceneQuery* -> PxQuery* in PhysX 4.
    // ---------------------------------------------------------------------
    typedef PxQueryFilterData  PxSceneQueryFilterData;
    typedef PxQueryFlag        PxSceneQueryFilterFlag;
    typedef PxQueryFlags       PxSceneQueryFilterFlags;
    typedef PxQueryHit         PxSceneQueryHit;
    typedef PxHitFlag          PxSceneQueryFlag;
    typedef PxHitFlags         PxSceneQueryFlags;
    typedef PxQueryFilterCallback PxSceneQueryFilterCallback;
}

// PxSceneQueryFlag::eIMPACT became PxHitFlag::ePOSITION in PhysX 4 (the hit
// "impact point" is now just "position"). Same meaning, new spelling.
#ifndef eIMPACT
  #define eIMPACT ePOSITION
#endif
#ifndef eNORMAL
  #define eNORMAL eNORMAL
#endif

namespace physx
{
    // Character-controller enums were renamed in PhysX 4:
    //   PxCCTNonWalkableMode -> PxControllerNonWalkableMode
    //   PxCCTInteractionMode -> replaced by PxControllerBehaviorFlag
    typedef PxControllerNonWalkableMode PxCCTNonWalkableMode;
}

// ---------------------------------------------------------------------------
// NOT aliasable -- needs hand porting:
//
//   PxVisualDebuggerExt::createConnection(...)   PhysXWorld.cpp
//       4.1: PxPvd::connect(PxPvdTransport&, PxPvdInstrumentationFlags)
//
//   RepXCollection / instantiateCollection(...)  PhysXRepXHelpers.cpp
//       4.1: PxSerialization + PxRepXSerializer
//
//   PxVehicleGearsData::mNumRatios               VehicleDescriptor.cpp
//       4.1: mNbRatios
// ---------------------------------------------------------------------------
