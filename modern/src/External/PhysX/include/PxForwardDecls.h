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

namespace physx
{
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
}

using namespace physx;
