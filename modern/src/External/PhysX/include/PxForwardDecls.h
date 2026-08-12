// SHIM: PhysX forward declarations for DISABLE_PHYSX builds
//
// Replaces:  the type surface of PxPhysicsAPI.h
// Status:    FORWARD DECLARATIONS ONLY. No PhysX behaviour.
// Later:     PhysX 4.1 (BSD-3) behind a 3.x compat header, then Jolt (MIT).
//            See DEPENDENCIES.md.
//
// r3dPCH.h skips PxPhysicsAPI.h when DISABLE_PHYSX is set, but headers such as
// GameEngine/gameobjects/PhysObj.h still name PhysX types unconditionally --
// e.g. "virtual PxActor* getPhysicsActor() = 0;" at PhysObj.h:188. Pointers and
// references to an incomplete type are legal, so forward declarations are enough
// to compile the engine without the SDK.

#pragma once

#include <cstddef>

namespace physx
{
    // --- scalar typedefs -------------------------------------------------
    typedef unsigned char      PxU8;
    typedef signed char        PxI8;
    typedef unsigned short     PxU16;
    typedef signed short       PxI16;
    typedef unsigned int       PxU32;
    typedef signed int         PxI32;
    typedef unsigned long long PxU64;
    typedef signed long long   PxI64;
    typedef float              PxReal;
    typedef float              PxF32;
    typedef double             PxF64;

    // --- types that must be COMPLETE, not merely declared ----------------
    //
    // PxAllocatorCallback is used as a BASE CLASS (PhysXWorld.h:11), and
    // PxHeightFieldGeometry is returned BY VALUE (ITerrain.h:73). Both need a
    // definition; forward declarations are not enough. Layout does not matter
    // here -- nothing is passed to a real PhysX build in this configuration.

    class PxAllocatorCallback
    {
    public:
        virtual ~PxAllocatorCallback() {}
        virtual void* allocate(size_t size, const char* typeName,
                               const char* filename, int line) = 0;
        virtual void  deallocate(void* ptr) = 0;
    };

    class PxErrorCallback
    {
    public:
        virtual ~PxErrorCallback() {}
        virtual void reportError(int code, const char* message,
                                 const char* file, int line) = 0;
    };

    struct PxVec3
    {
        PxReal x, y, z;
        PxVec3() : x(0), y(0), z(0) {}
        PxVec3(PxReal x_, PxReal y_, PxReal z_) : x(x_), y(y_), z(z_) {}
    };

    struct PxHeightFieldDesc
    {
        PxU32  nbRows;
        PxU32  nbColumns;
        PxReal thickness;
        PxReal convexEdgeThreshold;
        PxU32  flags;
        void*  samples;

        PxHeightFieldDesc()
            : nbRows(0), nbColumns(0), thickness(0),
              convexEdgeThreshold(0), flags(0), samples(nullptr) {}
    };

    struct PxHeightFieldGeometry
    {
        void*  heightField;
        PxReal heightScale;
        PxReal rowScale;
        PxReal columnScale;

        PxHeightFieldGeometry()
            : heightField(nullptr), heightScale(1), rowScale(1), columnScale(1) {}
    };

    class PxActor;
    class PxRigidActor;
    class PxRigidBody;
    class PxRigidDynamic;
    class PxRigidStatic;
    class PxShape;
    class PxScene;
    class PxPhysics;
    class PxFoundation;
    class PxMaterial;
    class PxGeometry;
    class PxTriangleMesh;
    class PxConvexMesh;
    class PxHeightField;
    class PxController;
    class PxControllerManager;
    class PxCooking;
    class PxJoint;
    class PxAggregate;
    class PxObstacleContext;
    class PxProfileZoneManager;

    struct PxContactSet;
    struct PxRaycastHit;
    struct PxFilterData;
    struct PxCapsuleObstacle;

    struct PxPairFlag { enum Enum { eSOLVE_CONTACT = 0 }; };

    // Query types appear by value in signatures (PhysXWorld.h:67-69), so they
    // need definitions rather than forward declarations.
    typedef PxU32 PxSceneQueryFlags;

    struct PxQueryFilterData
    {
        PxU32 data0, data1, data2, data3;
        PxU32 flags;
        PxQueryFilterData() : data0(0), data1(0), data2(0), data3(0), flags(0) {}
    };
    typedef PxQueryFilterData PxSceneQueryFilterData;

    struct PxRaycastHit
    {
        PxVec3 position;
        PxVec3 normal;
        PxReal distance;
        PxU32  faceIndex;
        void*  shape;
        void*  actor;

        PxRaycastHit()
            : distance(0), faceIndex(0), shape(nullptr), actor(nullptr) {}
    };
    typedef PxRaycastHit PxSceneQueryHit;
}

// PhysX Visual Debugger connection type, referenced by PhysXWorld.h:25 in
// non-FINAL_BUILD configurations.
namespace PVD
{
    class PvdConnection;
}

using namespace physx;
