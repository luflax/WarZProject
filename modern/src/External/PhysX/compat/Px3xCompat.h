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
    typedef PxQueryHitType        PxSceneQueryHitType;
    typedef PxQueryCache          PxSceneQueryCache;
}

// PxSceneQueryFlag::eIMPACT became PxHitFlag::ePOSITION in PhysX 4 (the hit
// "impact point" is now just "position"). Same meaning, new spelling.
//
// eDISTANCE is gone outright: PhysX 4 always fills in PxQueryHit::distance, so
// asking for it separately is meaningless. Folding it into ePOSITION preserves every
// call site's intent -- 19 of them pass eDISTANCE alone or OR'd with eIMPACT, and all
// of them then read both hit.position and hit.distance.
//
// These are MACROS ON ENUMERATOR NAMES, which is only safe because this header is
// included after PxPhysicsAPI.h has already parsed every PhysX header. The one other
// eDISTANCE in the SDK -- PxJointConcreteType::eDISTANCE in extensions/PxJoint.h --
// is therefore already compiled, and no game code names it. Do not include PhysX
// headers after this one.
#ifndef eIMPACT
  #define eIMPACT ePOSITION
#endif
#ifndef eDISTANCE
  #define eDISTANCE ePOSITION
#endif

// eINITIAL_OVERLAP / eINITIAL_OVERLAP_KEEP asked a sweep to report shapes that already
// overlap at the start pose. PhysX 4 does that unconditionally and offers only the
// opt-OUT, eASSUME_NO_INITIAL_OVERLAP -- so the 3.x flags have no counterpart and must
// resolve to something inert. ePOSITION is that: every call site that passes these also
// reads hit.position, so requesting it changes nothing.
#ifndef eINITIAL_OVERLAP
  #define eINITIAL_OVERLAP ePOSITION
#endif
#ifndef eINITIAL_OVERLAP_KEEP
  #define eINITIAL_OVERLAP_KEEP ePOSITION
#endif

namespace physx
{
    // Character-controller enums were renamed in PhysX 4:
    //   PxCCTNonWalkableMode -> PxControllerNonWalkableMode
    //   PxCCTInteractionMode -> replaced by PxControllerBehaviorFlag
    typedef PxControllerNonWalkableMode PxCCTNonWalkableMode;

    // PhysX 4 folded PxRigidDynamicFlag into PxRigidBodyFlag (and the setter with it:
    // setRigidDynamicFlag -> setRigidBodyFlag, which call sites now spell directly).
    typedef PxRigidBodyFlag  PxRigidDynamicFlag;
    typedef PxRigidBodyFlags PxRigidDynamicFlags;

    // -----------------------------------------------------------------------
    // Scene queries: PhysX 4 replaced the whole 3.x family
    //   raycastSingle / raycastMultiple / sweepSingle / sweepMultiple /
    //   overlapAny / overlapMultiple
    // with three methods -- raycast / sweep / overlap -- that write into a
    // PxHitCallback (PxRaycastBuffer, PxSweepBuffer, PxOverlapBuffer) owning the
    // touch array and the blocking-hit flag.
    //
    // There are ~35 call sites across the client, the server and GameEngine, all
    // spelled g_pPhysicsWorld->PhysXScene->xxx(...). Rather than rewrite each one,
    // PhysXScene is typed as PxScene3x*: a class that derives from PxScene and adds
    // the 3.x methods as ordinary non-virtual forwarders. It declares no data and no
    // virtuals, so a PxScene* from PxPhysics::createScene() is validly used through
    // it -- the object's layout and vtable are untouched. It stays abstract (PxScene's
    // pure virtuals are never overridden) and is only ever handled by pointer.
    //
    // The one behaviour note: 3.x returned the CLOSEST hit from sweepSingle, whereas
    // PhysX 4 reports the closest BLOCKING hit. With the default filter -- which is
    // what every call site here uses -- those are the same hit.
    // -----------------------------------------------------------------------
    class PxScene3x : public PxScene
    {
    public:
        bool raycastSingle(const PxVec3& origin, const PxVec3& unitDir, PxReal distance,
                           PxHitFlags outputFlags, PxRaycastHit& hit,
                           const PxQueryFilterData& filterData = PxQueryFilterData(),
                           PxQueryFilterCallback* filterCall = nullptr)
        {
            PxRaycastBuffer buf;
            if (!raycast(origin, unitDir, distance, buf, outputFlags, filterData, filterCall))
                return false;
            if (!buf.hasBlock)
                return false;
            hit = buf.block;
            return true;
        }

        PxI32 raycastMultiple(const PxVec3& origin, const PxVec3& unitDir, PxReal distance,
                              PxHitFlags outputFlags, PxRaycastHit* hitBuffer, PxU32 hitBufferSize,
                              bool& blockingHit,
                              const PxQueryFilterData& filterData = PxQueryFilterData(),
                              PxQueryFilterCallback* filterCall = nullptr)
        {
            PxRaycastBuffer buf(hitBuffer, hitBufferSize);
            raycast(origin, unitDir, distance, buf, outputFlags, filterData, filterCall);
            blockingHit = buf.hasBlock;
            return (PxI32)buf.getNbTouches();
        }

        bool sweepSingle(const PxGeometry& geometry, const PxTransform& pose, const PxVec3& unitDir,
                         PxReal distance, PxHitFlags outputFlags, PxSweepHit& hit,
                         const PxQueryFilterData& filterData = PxQueryFilterData(),
                         PxQueryFilterCallback* filterCall = nullptr)
        {
            PxSweepBuffer buf;
            if (!sweep(geometry, pose, unitDir, distance, buf, outputFlags, filterData, filterCall))
                return false;
            if (!buf.hasBlock)
                return false;
            hit = buf.block;
            return true;
        }

        PxI32 sweepMultiple(const PxGeometry& geometry, const PxTransform& pose, const PxVec3& unitDir,
                            PxReal distance, PxHitFlags outputFlags, PxSweepHit* hitBuffer,
                            PxU32 hitBufferSize, bool& blockingHit,
                            const PxQueryFilterData& filterData = PxQueryFilterData(),
                            PxQueryFilterCallback* filterCall = nullptr)
        {
            PxSweepBuffer buf(hitBuffer, hitBufferSize);
            sweep(geometry, pose, unitDir, distance, buf, outputFlags, filterData, filterCall);
            blockingHit = buf.hasBlock;
            return (PxI32)buf.getNbTouches();
        }

        // 3.x handed back the overlapping PxShape*; PhysX 4 reports PxOverlapHit, which
        // carries both the shape and its actor.
        bool overlapAny(const PxGeometry& geometry, const PxTransform& pose, PxShape*& hit,
                        const PxQueryFilterData& filterData = PxQueryFilterData(),
                        PxQueryFilterCallback* filterCall = nullptr)
        {
            PxOverlapBuffer buf;
            // eANY_HIT makes the traversal stop at the first hit and deliver it as the
            // block -- which is what "any" meant in 3.x.
            PxQueryFilterData fd(filterData);
            fd.flags |= PxQueryFlag::eANY_HIT;
            if (!overlap(geometry, pose, buf, fd, filterCall) || !buf.hasBlock)
            {
                hit = nullptr;
                return false;
            }
            hit = buf.block.shape;
            return true;
        }

        bool overlapAny(const PxGeometry& geometry, const PxTransform& pose, PxOverlapHit& hit,
                        const PxQueryFilterData& filterData = PxQueryFilterData(),
                        PxQueryFilterCallback* filterCall = nullptr)
        {
            PxOverlapBuffer buf;
            PxQueryFilterData fd(filterData);
            fd.flags |= PxQueryFlag::eANY_HIT;
            if (!overlap(geometry, pose, buf, fd, filterCall) || !buf.hasBlock)
                return false;
            hit = buf.block;
            return true;
        }

        PxI32 overlapMultiple(const PxGeometry& geometry, const PxTransform& pose,
                              PxShape** hitBuffer, PxU32 hitBufferSize,
                              const PxQueryFilterData& filterData = PxQueryFilterData(),
                              PxQueryFilterCallback* filterCall = nullptr)
        {
            if (hitBufferSize == 0)
                return 0;

            // Bounded so the staging array stays on the stack; every call site asks
            // for 64 or fewer.
            const PxU32 cap = hitBufferSize < 256 ? hitBufferSize : 256;
            PxOverlapHit touches[256];
            PxOverlapBuffer buf(touches, cap);
            overlap(geometry, pose, buf, filterData, filterCall);

            const PxU32 n = buf.getNbTouches();
            for (PxU32 i = 0; i < n; ++i)
                hitBuffer[i] = touches[i].shape;
            return (PxI32)n;
        }

        PxI32 overlapMultiple(const PxGeometry& geometry, const PxTransform& pose,
                              PxOverlapHit* hitBuffer, PxU32 hitBufferSize,
                              const PxQueryFilterData& filterData = PxQueryFilterData(),
                              PxQueryFilterCallback* filterCall = nullptr)
        {
            PxOverlapBuffer buf(hitBuffer, hitBufferSize);
            overlap(geometry, pose, buf, filterData, filterCall);
            return (PxI32)buf.getNbTouches();
        }
    };

    // PxPhysics::createScene() returns PxScene*; this is the one place the pointer is
    // adopted into the compat type.
    inline PxScene3x* PxAsScene3x(PxScene* s) { return static_cast<PxScene3x*>(s); }
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
