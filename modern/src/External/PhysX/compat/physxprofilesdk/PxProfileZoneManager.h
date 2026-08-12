// COMPAT: PhysX 3.x "physxprofilesdk/PxProfileZoneManager.h"
//
// The profile SDK was removed outright in PhysX 4. Only the type name is needed so
// pointer members and parameters compile; nothing is dereferenced.
#pragma once
namespace physx { class PxProfileZoneManager; }
