//=========================================================================
//	Module: VehicleManager.h
//	Copyright (C) 2012.
//=========================================================================

#pragma once

#ifdef VEHICLES_ENABLED

#include "PxBatchQuery.h"
#include "vehicle/PxVehicleDrive.h"
#include "vehicle/PxVehicleUtilSetup.h"

#include "VehicleDescriptor.h"

//////////////////////////////////////////////////////////////////////////

const uint32_t VEHICLE_DRIVABLE_SURFACE = 0xffff0000;
const uint32_t VEHICLE_NONDRIVABLE_SURFACE = 0xffff;

//Set up query filter data so that vehicles can drive on shapes with this filter data.
//Note that we have reserved word3 of the PxFilterData for vehicle raycast query filtering.
void VehicleSetupDrivableShapeQueryFilterData(PxFilterData &qryFilterData);

//Set up query filter data so that vehicles cannot drive on shapes with this filter data.
//Note that we have reserved word3 of the PxFilterData for vehicle raycast query filtering.
void VehicleSetupNonDrivableShapeQueryFilterData(PxFilterData &qryFilterData);

//////////////////////////////////////////////////////////////////////////

class VehicleManager;
class obj_Vehicle;
class PhysicsCallbackObject;
struct PhysicsObjectConfig;
//////////////////////////////////////////////////////////////////////////

class VehicleCameraController
{
	PxVec3 mCameraPos;
	PxVec3 mCameraTargetPos;
	PxVec3 mLastCarPos;
	PxVec3 mLastCarVelocity;
	bool mCameraInit;
	PxTransform mLastFocusVehTransform;
	float mCameraRotateAngleY;
	float mCameraRotateAngleZ;

	typedef r3dTL::TFixedArray<float, 10> FloatHistory;
	FloatHistory mAccelZHistory;
	FloatHistory mAccelXHistory;
	int mCounter;

	PxRigidDynamic *actor;

	void ResetHistory(FloatHistory &fh, float v);
	float HistoryAverage(const FloatHistory &fh) const;
	void CheckCameraCollision(r3dPoint3D& camPos, r3dPoint3D target, PxVec3 direction, float distance);

	bool isRearView;
	PxVec3 rearViewZ;
	PxVec3 rearViewX;
	PxVec3 frontViewZ;
	PxVec3 frontViewX;
public:
	VehicleCameraController();

	/** Update camera params for current driven vehicle */
	void Update(float dtime);
	void TankUpdate(float dtime);

	/**	Configure camera position and orientation for current driven vehicle. */
	bool ConfigureCamera(r3dCamera &cam);
	void SetDrivenVehicle(PxRigidDynamic *a);
	void SilentlyFollowPlayer(r3dPoint3D position);

	void SetRearView(bool enable);
};

//////////////////////////////////////////////////////////////////////////

class VehicleManager
{
	r3dTL::TArray<VehicleDescriptor*> vehicles;
	r3dTL::TArray<PxVehicleWheels*> physxVehs;
	r3dgameVector(obj_Vehicle*) physxVehicles;

	//	One result for each wheel in each car.
	r3dTL::TArray<PxRaycastQueryResult> batchQueryResults;
	//	One hit for each wheel in each car.
	r3dTL::TArray<PxRaycastHit> batchHits;

	//	[PORT] PhysX 3.x let you read per-wheel suspension jounce and steer angle back
	//	off PxVehicleWheelsDynData. PhysX 4 removed those accessors: the values are now
	//	produced by PxVehicleUpdates into a caller-supplied PxVehicleWheelQueryResult,
	//	one per vehicle, each pointing at a slice of a flat per-wheel array. obj_Vehicle
	//	reads them back through GetWheelQueryResults() when it builds its move packet.
	r3dTL::TArray<PxWheelQueryResult>        wheelQueryStorage;
	r3dTL::TArray<PxVehicleWheelQueryResult> wheelQueryResults;

	//	Suspension raycasts batched query
	physx::PxBatchQuery * batchSuspensionRaycasts;

	//	Tire-surface friction table;
	//PxVehicleDrivableSurfaceToTireFrictionPairs *frictionData;

	void ConfigureSuspensionRaycasts(const VehicleDescriptor &car);
	void ClearSuspensionRaycatsQuery();

	physx::PxBatchQuery * SetUpBatchedSceneQuery();
	void IssueSuspensionRaycasts();

	PxVehicleDrivableSurfaceToTireFrictionPairs *surfaceTypePairs;

	bool mAtRestUnderBraking;
	float mTimeElapsedSinceAtRestUnderBraking;
	bool mInReverseMode;

	void DoUserCarControl(float timeStep);
	bool ProcessAutoReverse(float timestep);

	VehicleDescriptor *drivableCar;
	
	PxVec3 gravity;

	int activeVehicles;
	
	bool IsDrivableCarUsed();
public:
	/**
	* Per-wheel simulation output for a vehicle, as of the last PxVehicleUpdates.
	* Returns NULL when the vehicle is not currently being simulated.
	*/
	const PxVehicleWheelQueryResult* GetWheelQueryResults(const PxVehicleWheels* veh) const;

	bool hasDrivableCar;
	bool clearInputData;

	VehicleCameraController cameraContoller;

	bool SetActiveVehicle(VehicleDescriptor* vd);
	bool RemoveActiveVehicle(VehicleDescriptor* vd);

	bool IsInReverse();
	void SetHasDrivableCar(bool value);
	void ClearControlData();

	void UpdateFrictionValues(int vehicleType, float frictionValue);
public:
	VehicleManager();
	~VehicleManager();

	bool vehiclesLoaded;

	void Update(float timeStep);
	VehicleDescriptor * CreateVehicle(const r3dMesh *m, PhysicsCallbackObject* physCallbackObj, const PhysicsObjectConfig& PhysicsConfig);

	PxVehicleDrive4WRawInputData carControlData;
	PxVehicleDriveTankRawInputData tankControlData;

	void DeleteCar(VehicleDescriptor *car);
	void DriveCar(VehicleDescriptor *car);
	const VehicleDescriptor * GetDrivenCar() const { return drivableCar; }
	void UpdateInput();
	obj_Vehicle* getRealDrivenVehicle();
	/** Update vehicle poses from physx. */
	void UpdateVehiclePoses();
	bool ConfigureCamera(r3dCamera &cam);

	PxVehicleWheels* GetVehicleWheelByIndex(uint32_t index);

	obj_Vehicle* GetVehicleById(DWORD vehicleId);

	void UpdateCamera(float timeStep);

	PxF32 VehicleForwardSpeed;
	PxF32 VehicleSidewaySpeed;
};

//////////////////////////////////////////////////////////////////////////

#endif // VEHICLES_ENABLED
