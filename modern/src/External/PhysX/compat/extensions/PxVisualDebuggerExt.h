// COMPAT: PhysX 3.x "extensions/PxVisualDebuggerExt.h"
//
// The 3.x PVD API (namespace PVD, PvdConnection, PxVisualDebuggerExt::createConnection,
// PxPhysics::getPvdConnectionManager) was replaced wholesale in PhysX 4 by PxPvd +
// PxPvdTransport.
//
// The old names are reproduced here as thin aliases so headers that merely DECLARE a
// connection pointer (e.g. PhysXWorld.h:25 "PVD::PvdConnection* debuggerConnection")
// compile unchanged. Code that actually opens a connection must be ported to PxPvd --
// PhysXWorld.cpp:488 is the only such site.
#pragma once

#include "pvd/PxPvd.h"
#include "pvd/PxPvdTransport.h"

namespace PVD
{
    // 3.x called it PvdConnection; 4.1 calls it PxPvd.
    typedef physx::PxPvd PvdConnection;
}

namespace physx
{
    // 3.x connection flags lived here; 4.1 uses PxPvdInstrumentationFlag.
    struct PxVisualDebuggerExt
    {
        static PxPvd* createConnection(void* /*manager*/, const char* /*host*/,
                                       int /*port*/, unsigned int /*timeoutMs*/,
                                       PxPvdInstrumentationFlags /*flags*/)
        {
            // Not portable to 4.1 without a PxPvdTransport; PVD is a debug-only
            // feature and is disabled in this configuration.
            return nullptr;
        }
    };

    typedef PxPvdInstrumentationFlags PxVisualDebuggerConnectionFlags;
}
