#ifndef __PHYSXWORLD_H__
#define __PHYSXWORLD_H__

#define PHYSX_USE_CCD 1

#ifndef WO_SERVER
#include "VehicleManager.h"
#endif
//////////////////////////////////////////////////////////////////////////

// This header assumes its includer has already pulled in the PhysX headers -- it names
// PxAllocatorCallback, PxFoundation and PVD::PvdConnection without including anything.
// PxPvdTransport is only reached through pvd/PxPvdTransport.h, which PhysXWorld.cpp
// includes but the header's other includers do not, so it is forward-declared rather
// than added to that assumption.
namespace physx { class PxPvdTransport; }

class MyPhysXAllocator : public PxAllocatorCallback
{
public:
	virtual void* allocate(size_t size, const char* typeName, const char* filename, int line);
	virtual void deallocate(void* ptr);
};

//////////////////////////////////////////////////////////////////////////

class VehicleManager;
class PhysXWorld
{
    bool    m_needFetchResults;
#ifndef FINAL_BUILD
	PVD::PvdConnection *debuggerConnection;
	// [PORT] PhysX 4 splits 3.x's single connection object in two: PxPvd owns the
	// instrumentation and PxPvdTransport owns the socket. The transport is created
	// by us and must be released by us, after the PxPvd that reads from it.
	physx::PxPvdTransport *debuggerTransport;
#endif

public:
	PxFoundation *PhysXFoundation;
	PxProfileZoneManager *PhysXProfileZoneMgr;
	physx::PxPhysics*	PhysXSDK;
	// [PORT] PxScene3x adds back the PhysX 3.x scene-query methods (raycastSingle,
	// sweepSingle, overlapAny, ...) that PhysX 4 removed. See External/PhysX/compat/Px3xCompat.h.
	PxScene3x*		PhysXScene;
	PxControllerManager* CharacterManager; // create all characters through it for proper physics behavior
	PxCooking* Cooking; // for cooking meshes for PhysX
	PxMaterial* defaultMaterial;
	PxMaterial*	noBounceMaterial;
	VehicleManager *m_VehicleManager;
#ifndef WO_SERVER
	PxObstacleContext*	m_PlayerObstaclesManager;
#ifdef VEHICLES_ENABLED
	PxObstacleContext* m_VehicleObstacleManager;
#endif
#endif
	/**
	* This critical section should be used to protect concurrent SDK operation during simulate() and fetchResults() calls. 
	* @see GetConcurrencyGuard().
	*/
	CRITICAL_SECTION concurrencyGuard;
	MyPhysXAllocator myPhysXAllocator;

public:
	PhysXWorld();
	~PhysXWorld();

	void Init();
	void Destroy();

	bool CookMesh(const r3dMesh* mesh, const char* save_as=NULL); // will save a cooked mesh as meshname.mpx
	PxTriangleMesh* getCookedMesh(const char* filename); // cached
	bool CookConvexMesh(const r3dMesh* mesh, const char* save_as=NULL); // will save a cooked mesh as meshname.cpx
	PxConvexMesh* getConvexMesh(const char* filename); // cached
	bool HasCookedMesh(const r3dMesh* mesh);
	bool HasConvexMesh(const r3dMesh* mesh);

    // fixed version of raycast single. PhysX was written by idiots and that shit returns collision with terrain even when ray is not even close to terrain.
    bool raycastSingle(const PxVec3& origin, const PxVec3& unitDir, const PxReal distance,
        PxSceneQueryFlags outputFlags,
        PxRaycastHit& hit,
        const PxSceneQueryFilterData& filterData = PxSceneQueryFilterData());

	void StartSimulation();
	void EndSimulation();

	void UpdateDebugBounds();
	void DrawDebug();

	/**
	* During multithread object creation/destruction and scene modification, client code should guard physX operations
	* using this CS. This is because during simulate() and fetchResults() calls no scene and object modification is allowed.
	*/
	CRITICAL_SECTION & GetConcurrencyGuard() { return concurrencyGuard; }

	/**	Client should use this add actor call, because of multithread concurrency issues. */
	void AddActor(PxActor &actor);
	void RemoveActor(PxActor &actor);

	MyPhysXAllocator & GetAllocator() { return myPhysXAllocator; }
	/**	Export whole scene into physx collection format. This function can be used for debug purposes, when you need to share scene with specific issue to other people/applications. */
#ifndef FINAL_BUILD
	bool ExportWholeScene(const char *filename) const;
#endif
};
extern PhysXWorld* g_pPhysicsWorld;

#endif //__PHYSXWORLD_H__