// SHIM: FMOD Ex
//
// Replaces:  FMOD Ex (Firelight Technologies) — commercial licence, and FMOD Ex
//            itself is end-of-life. See ../../../../DEPENDENCIES.md.
// Status:    NO-OP. The game will be silent.
// Later:     miniaudio (MIT-0) + Steam Audio (Apache-2.0). GameEngine/fmod/SoundSys.h
//            already isolates FMOD, so the eventual swap is contained.
//
// Clean-room declarations derived from call sites in GameEngine/fmod/SoundSys.{h,cpp}.
// No code originates from the FMOD SDK.
//
// Contract (see ../README.md): declare what is referenced, fail honestly at runtime,
// never fake success. Every function here returns an error or a null handle so calling
// code takes its failure path rather than proceeding into a half-initialised state.
//
// COVERAGE IS INCOMPLETE BY DESIGN. Add symbols as the compiler demands them.

#pragma once

// ---------------------------------------------------------------------------
// Result codes
// ---------------------------------------------------------------------------

enum FMOD_RESULT
{
    FMOD_OK = 0,
    FMOD_ERR_INITIALIZATION,
    FMOD_ERR_INTERNAL,
    FMOD_ERR_INVALID_HANDLE,
    FMOD_ERR_INVALID_PARAM,
    FMOD_ERR_EVENT_NOTFOUND,
    FMOD_ERR_UNIMPLEMENTED,
};

// ---------------------------------------------------------------------------
// Enums and POD types
// ---------------------------------------------------------------------------

enum FMOD_EVENT_PROPERTY
{
    FMOD_EVENTPROPERTY_VOLUME = 0,
    FMOD_EVENTPROPERTY_PITCH,
    FMOD_EVENTPROPERTY_3D_MINDISTANCE,
    FMOD_EVENTPROPERTY_3D_MAXDISTANCE,
};

enum FMOD_EVENT_MODE
{
    FMOD_EVENT_DEFAULT      = 0x00000000,
    FMOD_EVENT_NONBLOCKING  = 0x00000001,
    FMOD_EVENT_3D          = 0x00000002,
};

enum FMOD_EVENT_STATE
{
    FMOD_EVENT_STATE_READY   = 0x00000001,
    FMOD_EVENT_STATE_PLAYING = 0x00000002,
};

struct FMOD_VECTOR
{
    float x, y, z;
};

using FMOD_MODE       = unsigned int;
using FMOD_INITFLAGS  = unsigned int;
using FMOD_EVENT_INITFLAGS = unsigned int;

// ---------------------------------------------------------------------------
// Interfaces
//
// All methods are inline no-ops returning failure. Kept as classes with virtuals
// so that pointer types, casts and any subclassing in game code still compile.
// ---------------------------------------------------------------------------

namespace FMOD
{

class Sound
{
public:
    virtual ~Sound() = default;
    virtual FMOD_RESULT release() { return FMOD_OK; }
};

class Channel
{
public:
    virtual ~Channel() = default;
    virtual FMOD_RESULT stop()                   { return FMOD_OK; }
    virtual FMOD_RESULT setVolume(float)         { return FMOD_OK; }
    virtual FMOD_RESULT setPaused(bool)          { return FMOD_OK; }
    virtual FMOD_RESULT isPlaying(bool* p)       { if (p) *p = false; return FMOD_OK; }
};

class Event
{
public:
    virtual ~Event() = default;
    virtual FMOD_RESULT start()                          { return FMOD_OK; }
    virtual FMOD_RESULT stop(bool = false)               { return FMOD_OK; }
    virtual FMOD_RESULT release(bool = true, bool = false){ return FMOD_OK; }
    virtual FMOD_RESULT setVolume(float)                 { return FMOD_OK; }
    virtual FMOD_RESULT set3DAttributes(const FMOD_VECTOR*, const FMOD_VECTOR*, const FMOD_VECTOR* = nullptr) { return FMOD_OK; }
    virtual FMOD_RESULT getState(FMOD_EVENT_STATE* s)    { if (s) *s = FMOD_EVENT_STATE_READY; return FMOD_OK; }
    virtual FMOD_RESULT setPropertyByIndex(int, void*, bool = false) { return FMOD_OK; }
    virtual FMOD_RESULT getPropertyByIndex(int, void*)   { return FMOD_ERR_UNIMPLEMENTED; }
};

class EventReverb
{
public:
    virtual ~EventReverb() = default;
    virtual FMOD_RESULT release()             { return FMOD_OK; }
    virtual FMOD_RESULT setActive(bool)       { return FMOD_OK; }
    virtual FMOD_RESULT set3DAttributes(const FMOD_VECTOR*, float, float) { return FMOD_OK; }
};

class EventGroup
{
public:
    virtual ~EventGroup() = default;
    virtual FMOD_RESULT getEvent(const char*, FMOD_EVENT_MODE, Event** e)
    {
        if (e) *e = nullptr;
        return FMOD_ERR_EVENT_NOTFOUND;
    }
};

class EventProject
{
public:
    virtual ~EventProject() = default;
    virtual FMOD_RESULT getGroup(const char*, bool, EventGroup** g)
    {
        if (g) *g = nullptr;
        return FMOD_ERR_EVENT_NOTFOUND;
    }
};

class EventSystem
{
public:
    virtual ~EventSystem() = default;

    virtual FMOD_RESULT init(int, FMOD_INITFLAGS, void*, FMOD_EVENT_INITFLAGS = 0) { return FMOD_OK; }
    virtual FMOD_RESULT release()                        { return FMOD_OK; }
    virtual FMOD_RESULT update()                         { return FMOD_OK; }
    virtual FMOD_RESULT setMediaPath(const char*)        { return FMOD_OK; }
    virtual FMOD_RESULT load(const char*, void*, EventProject** p)
    {
        if (p) *p = nullptr;
        return FMOD_ERR_INITIALIZATION;
    }
    virtual FMOD_RESULT getEvent(const char*, FMOD_EVENT_MODE, Event** e)
    {
        if (e) *e = nullptr;
        return FMOD_ERR_EVENT_NOTFOUND;
    }
    virtual FMOD_RESULT getEventBySystemID(unsigned int, FMOD_EVENT_MODE, Event** e)
    {
        if (e) *e = nullptr;
        return FMOD_ERR_EVENT_NOTFOUND;
    }
    virtual FMOD_RESULT set3DListenerAttributes(int, const FMOD_VECTOR*, const FMOD_VECTOR*,
                                                const FMOD_VECTOR*, const FMOD_VECTOR*) { return FMOD_OK; }
    virtual FMOD_RESULT createReverb(EventReverb** r)
    {
        if (r) *r = nullptr;
        return FMOD_ERR_UNIMPLEMENTED;
    }
};

} // namespace FMOD

// ---------------------------------------------------------------------------
// C entry points
// ---------------------------------------------------------------------------

inline FMOD_RESULT FMOD_EventSystem_Create(FMOD::EventSystem** system)
{
    // Returning OK with a null system would let SoundSys::Valid() dereference it.
    // Fail honestly instead.
    if (system) *system = nullptr;
    return FMOD_ERR_INITIALIZATION;
}
