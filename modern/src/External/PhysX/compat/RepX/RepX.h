// COMPAT: PhysX 3.x "RepX/RepX.h"
//
// RepX was folded into the extensions library in PhysX 4. The 3.x entry points
// (RepXCollection, RepXIdToRepXObjectMap, instantiateCollection, ...) no longer
// exist; PxSerialization + PxRepXSerializer replace them.
//
// This forwards the headers that DO exist so include sites resolve. Call sites
// using the removed 3.x API still need porting -- see PhysXRepXHelpers.cpp.
#pragma once

#include "extensions/PxRepXSerializer.h"
#include "extensions/PxRepXSimpleType.h"
#include "extensions/PxSerialization.h"
