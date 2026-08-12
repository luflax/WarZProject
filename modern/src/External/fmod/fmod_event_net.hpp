// SHIM: FMOD Ex network/live-update API
//
// Replaces:  fmod_event_net.hpp (FMOD Ex live event auditioning)
// Status:    NO-OP.
// See ../../../../DEPENDENCIES.md -- miniaudio + Steam Audio replace FMOD.

#pragma once
#include "fmod_event.hpp"

namespace FMOD
{
inline FMOD_RESULT NetEventSystem_Init(EventSystem*)  { return FMOD_OK; }
inline FMOD_RESULT NetEventSystem_Update()            { return FMOD_OK; }
inline FMOD_RESULT NetEventSystem_Shutdown()          { return FMOD_OK; }
}
