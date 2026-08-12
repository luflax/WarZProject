// SHIM: FMOD Ex error strings
//
// Replaces:  fmod_errors.h
// Status:    NO-OP. Used by SND_ERR_CHK in GameEngine/fmod/SoundSys.h.

#pragma once
#include "fmod_event.hpp"

inline const char* FMOD_ErrorString(FMOD_RESULT result)
{
    switch (result)
    {
    case FMOD_OK:                   return "No errors.";
    case FMOD_ERR_INITIALIZATION:   return "FMOD is shimmed out; audio is disabled.";
    case FMOD_ERR_INVALID_HANDLE:   return "Invalid handle.";
    case FMOD_ERR_INVALID_PARAM:    return "Invalid parameter.";
    case FMOD_ERR_EVENT_NOTFOUND:   return "Event not found.";
    case FMOD_ERR_UNIMPLEMENTED:    return "Not implemented in the FMOD shim.";
    default:                        return "Unknown error (FMOD shim).";
    }
}
