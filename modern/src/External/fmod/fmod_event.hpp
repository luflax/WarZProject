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
// Calling convention and file-system callback types
// ---------------------------------------------------------------------------

#ifndef F_CALLBACK
  #ifdef _WIN32
    #define F_CALLBACK __stdcall
    #define F_API      __stdcall
  #else
    #define F_CALLBACK
    #define F_API
  #endif
#endif

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
    FMOD_ERR_FILE_NOTFOUND,
    FMOD_ERR_FILE_BAD,
    FMOD_ERR_FILE_EOF,
    FMOD_ERR_NET_SOCKET_ERROR,
    FMOD_ERR_OUTPUT_CREATEBUFFER,
    FMOD_ERR_MEMORY,
    FMOD_ERR_INVALID_HANDLE_2,
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
    FMOD_EVENTPROPERTY_MODE,
};

// Bit flags, combined with | and assigned back, so a plain integer type rather
// than a scoped enum (which would need casts at every call site).
typedef unsigned int FMOD_EVENT_MODE;
#define FMOD_EVENT_DEFAULT      0x00000000u
#define FMOD_EVENT_NONBLOCKING  0x00000001u
#define FMOD_EVENT_3D           0x00000002u

enum FMOD_SPEAKERMODE
{
    FMOD_SPEAKERMODE_RAW = 0,
    FMOD_SPEAKERMODE_MONO,
    FMOD_SPEAKERMODE_STEREO,
    FMOD_SPEAKERMODE_QUAD,
    FMOD_SPEAKERMODE_SURROUND,
    FMOD_SPEAKERMODE_5POINT1,
    FMOD_SPEAKERMODE_7POINT1,
    FMOD_SPEAKERMODE_MAX,
};

typedef unsigned int FMOD_EVENT_STATE;
#define FMOD_EVENT_STATE_READY   0x00000001u
#define FMOD_EVENT_STATE_PLAYING 0x00000002u

struct FMOD_VECTOR
{
    float x, y, z;
};

using FMOD_MODE       = unsigned int;

// File-system override callbacks (SoundSys.cpp routes FMOD through r3dFile).
typedef FMOD_RESULT (F_CALLBACK *FMOD_FILE_OPENCALLBACK )(const char*, int, unsigned int*, void**, void**);
typedef FMOD_RESULT (F_CALLBACK *FMOD_FILE_CLOSECALLBACK)(void*, void*);
typedef FMOD_RESULT (F_CALLBACK *FMOD_FILE_READCALLBACK )(void*, void*, unsigned int, unsigned int*, void*);
typedef FMOD_RESULT (F_CALLBACK *FMOD_FILE_SEEKCALLBACK )(void*, unsigned int, void*);
using FMOD_INITFLAGS  = unsigned int;
using FMOD_EVENT_INITFLAGS = unsigned int;


// ---------------------------------------------------------------------------
// Remaining surface referenced by GameEngine/fmod/SoundSys.{h,cpp}
// ---------------------------------------------------------------------------

#define FMOD_VERSION 0x00044400

// Sound creation modes (bit flags on FMOD_MODE)
#define FMOD_3D                       0x00000010
#define FMOD_CREATECOMPRESSEDSAMPLE   0x00000200
#define FMOD_SOFTWARE                 0x00000040

// System init flags
#define FMOD_INIT_NORMAL              0x00000000
#define FMOD_INIT_ENABLE_PROFILE      0x00000010

// Event system flags
#define FMOD_EVENT_INFOONLY           0x00000004u
#define FMOD_EVENT_STATE_CHANNELSACTIVE 0x00000008u

typedef unsigned int FMOD_CAPS;
#define FMOD_CAPS_HARDWARE_EMULATED   0x00000002

enum FMOD_OUTPUTTYPE
{
    FMOD_OUTPUTTYPE_AUTOSELECT = 0,
    FMOD_OUTPUTTYPE_NOSOUND,
    FMOD_OUTPUTTYPE_MAX,
};

enum FMOD_DSP_RESAMPLER
{
    FMOD_DSP_RESAMPLER_NOINTERP = 0,
    FMOD_DSP_RESAMPLER_LINEAR,
    FMOD_DSP_RESAMPLER_MAX,
};

enum FMOD_SOUND_FORMAT
{
    FMOD_SOUND_FORMAT_NONE = 0,
    FMOD_SOUND_FORMAT_PCM16,
    FMOD_SOUND_FORMAT_PCMFLOAT,
    FMOD_SOUND_FORMAT_MAX,
};

struct FMOD_REVERB_PROPERTIES
{
    int   Instance;
    int   Environment;
    float EnvDiffusion;
    int   Room;
    int   RoomHF;
    float DecayTime;
    unsigned int Flags;
};

// Reverb presets. Values are irrelevant while audio is shimmed out; only the
// names need to exist for ReverbZone.cpp / SoundSys.cpp to compile.
#define FMOD_PRESET_OFF               { 0,  0, 1.0f, -10000, 0, 1.0f, 0x3f }
#define FMOD_PRESET_GENERIC           FMOD_PRESET_OFF
#define FMOD_PRESET_PADDEDCELL        FMOD_PRESET_OFF
#define FMOD_PRESET_ROOM              FMOD_PRESET_OFF
#define FMOD_PRESET_BATHROOM          FMOD_PRESET_OFF
#define FMOD_PRESET_LIVINGROOM        FMOD_PRESET_OFF
#define FMOD_PRESET_STONEROOM         FMOD_PRESET_OFF
#define FMOD_PRESET_AUDITORIUM        FMOD_PRESET_OFF
#define FMOD_PRESET_CONCERTHALL       FMOD_PRESET_OFF
#define FMOD_PRESET_CAVE              FMOD_PRESET_OFF
#define FMOD_PRESET_ARENA             FMOD_PRESET_OFF
#define FMOD_PRESET_HANGAR            FMOD_PRESET_OFF
#define FMOD_PRESET_CARPETTEDHALLWAY  FMOD_PRESET_OFF
#define FMOD_PRESET_HALLWAY           FMOD_PRESET_OFF
#define FMOD_PRESET_STONECORRIDOR     FMOD_PRESET_OFF
#define FMOD_PRESET_ALLEY             FMOD_PRESET_OFF
#define FMOD_PRESET_FOREST            FMOD_PRESET_OFF
#define FMOD_PRESET_CITY              FMOD_PRESET_OFF
#define FMOD_PRESET_MOUNTAINS         FMOD_PRESET_OFF
#define FMOD_PRESET_QUARRY            FMOD_PRESET_OFF
#define FMOD_PRESET_PLAIN             FMOD_PRESET_OFF
#define FMOD_PRESET_PARKINGLOT        FMOD_PRESET_OFF
#define FMOD_PRESET_SEWERPIPE         FMOD_PRESET_OFF
#define FMOD_PRESET_UNDERWATER        FMOD_PRESET_OFF

struct FMOD_EVENT_LOADINFO
{
    unsigned int size;
    unsigned int encryptionKey;
    void*        loadFromMemory;
    unsigned int loadfrommemory_length;
};

struct FMOD_EVENT_PROJECTINFO
{
    char         name[256];
    unsigned int index;
};

struct FMOD_EVENT_INFO
{
    int          memoryused;
    unsigned int positionms;
    unsigned int lengthms;
    int          channelsplaying;
    unsigned int projectid;
};

// ---------------------------------------------------------------------------
// Interfaces
//
// All methods are inline no-ops returning failure. Kept as classes with virtuals
// so that pointer types, casts and any subclassing in game code still compile.
// ---------------------------------------------------------------------------

namespace FMOD
{

class EventGroup;
class EventProject;
class Event;
class Sound;


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

class EventParameter
{
public:
    virtual ~EventParameter() = default;
    virtual FMOD_RESULT setValue(float)      { return FMOD_OK; }
    virtual FMOD_RESULT getValue(float* v)   { if (v) *v = 0.0f; return FMOD_OK; }
    virtual FMOD_RESULT keyOff() { return FMOD_OK; }
    virtual FMOD_RESULT getRange(float* lo, float* hi)
    { if (lo) *lo = 0.0f; if (hi) *hi = 1.0f; return FMOD_OK; }
};

class Event
{
public:
    virtual ~Event() = default;
    virtual FMOD_RESULT setPaused(bool)              { return FMOD_OK; }
    virtual FMOD_RESULT getPaused(bool* p)           { if (p) *p = false; return FMOD_OK; }
    virtual FMOD_RESULT getInfo(int*, char**, FMOD_EVENT_INFO*) { return FMOD_OK; }
    virtual FMOD_RESULT getParameter(const char*, EventParameter** p)
    { if (p) *p = nullptr; return FMOD_ERR_EVENT_NOTFOUND; }
    virtual FMOD_RESULT getParameterByIndex(int, EventParameter** p)
    { if (p) *p = nullptr; return FMOD_ERR_EVENT_NOTFOUND; }
    virtual FMOD_RESULT getParentGroup(EventGroup** g)
    { if (g) *g = nullptr; return FMOD_ERR_EVENT_NOTFOUND; }
    virtual FMOD_RESULT start()                          { return FMOD_OK; }
    virtual FMOD_RESULT stop(bool = false)               { return FMOD_OK; }
    virtual FMOD_RESULT release(bool = true, bool = false){ return FMOD_OK; }
    virtual FMOD_RESULT setVolume(float)                 { return FMOD_OK; }
    virtual FMOD_RESULT set3DAttributes(const FMOD_VECTOR*, const FMOD_VECTOR*, const FMOD_VECTOR* = nullptr) { return FMOD_OK; }
    virtual FMOD_RESULT getState(FMOD_EVENT_STATE* s)    { if (s) *s = FMOD_EVENT_STATE_READY; return FMOD_OK; }
    virtual FMOD_RESULT setPropertyByIndex(FMOD_EVENT_PROPERTY, void*, bool = false) { return FMOD_OK; }
    virtual FMOD_RESULT getPropertyByIndex(FMOD_EVENT_PROPERTY, void*, bool = false) { return FMOD_ERR_UNIMPLEMENTED; }
};

class EventReverb
{
public:
    virtual ~EventReverb() = default;
    virtual FMOD_RESULT release()             { return FMOD_OK; }
    virtual FMOD_RESULT setActive(bool)       { return FMOD_OK; }
    virtual FMOD_RESULT set3DAttributes(const FMOD_VECTOR*, float, float) { return FMOD_OK; }
    virtual FMOD_RESULT setProperties(const FMOD_REVERB_PROPERTIES*) { return FMOD_OK; }
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
    virtual FMOD_RESULT freeEventData(Event* = nullptr, bool = true) { return FMOD_OK; }
    virtual FMOD_RESULT getInfo(int* idx, char** name)
    { if (idx) *idx = 0; if (name) *name = nullptr; return FMOD_OK; }
    virtual FMOD_RESULT getParentGroup(EventGroup** g)
    { if (g) *g = nullptr; return FMOD_ERR_EVENT_NOTFOUND; }
    virtual FMOD_RESULT getParentProject(EventProject** p)
    { if (p) *p = nullptr; return FMOD_ERR_EVENT_NOTFOUND; }
};

class EventCategory
{
public:
    virtual ~EventCategory() = default;
    virtual FMOD_RESULT setVolume(float) { return FMOD_OK; }
    virtual FMOD_RESULT setPaused(bool)  { return FMOD_OK; }
    virtual FMOD_RESULT stopAllEvents()  { return FMOD_OK; }
};

class EventProject
{
public:
    virtual ~EventProject() = default;
    virtual FMOD_RESULT release() { return FMOD_OK; }
    virtual FMOD_RESULT getInfo(FMOD_EVENT_PROJECTINFO* info)
    { if (info) { info->name[0] = 0; info->index = 0; } return FMOD_OK; }
    virtual FMOD_RESULT getEventByProjectID(unsigned int, FMOD_EVENT_MODE, Event** e)
    { if (e) *e = nullptr; return FMOD_ERR_EVENT_NOTFOUND; }
    virtual FMOD_RESULT getGroup(const char*, bool, EventGroup** g)
    {
        if (g) *g = nullptr;
        return FMOD_ERR_EVENT_NOTFOUND;
    }
};

// Low-level system, reached via EventSystem::getSystemObject().
class System
{
public:
    virtual ~System() = default;
    virtual FMOD_RESULT setOutput(FMOD_OUTPUTTYPE)                    { return FMOD_OK; }
    virtual FMOD_RESULT setSoftwareFormat(int, FMOD_SOUND_FORMAT, int, int,
                                          FMOD_DSP_RESAMPLER)         { return FMOD_OK; }
    virtual FMOD_RESULT setSoftwareChannels(int)                      { return FMOD_OK; }
    virtual FMOD_RESULT setFileSystem(FMOD_FILE_OPENCALLBACK, FMOD_FILE_CLOSECALLBACK,
                                      FMOD_FILE_READCALLBACK, FMOD_FILE_SEEKCALLBACK,
                                      void*, void*, int)              { return FMOD_OK; }
    virtual FMOD_RESULT getDriverCaps(int, FMOD_CAPS* caps, int*, FMOD_SPEAKERMODE* mode)
    { if (caps) *caps = 0; if (mode) *mode = FMOD_SPEAKERMODE_STEREO; return FMOD_OK; }
    virtual FMOD_RESULT setSpeakerMode(FMOD_SPEAKERMODE)              { return FMOD_OK; }
    virtual FMOD_RESULT getVersion(unsigned int* v)                   { if (v) *v = FMOD_VERSION; return FMOD_OK; }
    virtual FMOD_RESULT createSound(const char*, FMOD_MODE, void*, Sound** s)
    { if (s) *s = nullptr; return FMOD_ERR_INITIALIZATION; }
    virtual FMOD_RESULT set3DSettings(float, float, float)            { return FMOD_OK; }
    virtual FMOD_RESULT setReverbProperties(const FMOD_REVERB_PROPERTIES*) { return FMOD_OK; }
    virtual FMOD_RESULT setReverbAmbientProperties(FMOD_REVERB_PROPERTIES*) { return FMOD_OK; }
    virtual FMOD_RESULT getNumDrivers(int* n)        { if (n) *n = 0; return FMOD_OK; }
    virtual FMOD_RESULT getDriverInfo(int, char* name, int len, void*)
    { if (name && len > 0) name[0] = 0; return FMOD_OK; }
    virtual FMOD_RESULT setDSPBufferSize(unsigned int, int) { return FMOD_OK; }
};

class EventSystem
{
public:
    virtual FMOD_RESULT getSystemObject(System** sys) { if (sys) *sys = nullptr; return FMOD_ERR_INITIALIZATION; }
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
    virtual FMOD_RESULT get3DListenerAttributes(int, FMOD_VECTOR*, FMOD_VECTOR*,
                                                FMOD_VECTOR*, FMOD_VECTOR*) { return FMOD_OK; }
    virtual FMOD_RESULT set3DNumListeners(int)                     { return FMOD_OK; }
    virtual FMOD_RESULT getCategory(const char*, EventCategory** c)
    { if (c) *c = nullptr; return FMOD_ERR_EVENT_NOTFOUND; }
    virtual FMOD_RESULT getNumEvents(int* n)                       { if (n) *n = 0; return FMOD_OK; }
    virtual FMOD_RESULT getReverbPreset(const char*, FMOD_REVERB_PROPERTIES*, int*)
    { return FMOD_ERR_EVENT_NOTFOUND; }
    virtual FMOD_RESULT preloadFSB(const char*, int, Sound* = nullptr) { return FMOD_OK; }
    virtual FMOD_RESULT unloadFSB(const char*, int)                { return FMOD_OK; }
};

} // namespace FMOD

// ---------------------------------------------------------------------------
// C entry points
// ---------------------------------------------------------------------------

namespace FMOD
{

class EventGroup;
class EventProject;
class Event;
class Sound;

// SoundSys.cpp calls FMOD::EventSystem_Create(...).
inline FMOD_RESULT EventSystem_Create(EventSystem** system)
{
    if (system) *system = nullptr;
    return FMOD_ERR_INITIALIZATION;
}
}

inline FMOD_RESULT FMOD_EventSystem_Create(FMOD::EventSystem** system)
{
    // Returning OK with a null system would let SoundSys::Valid() dereference it.
    // Fail honestly instead.
    if (system) *system = nullptr;
    return FMOD_ERR_INITIALIZATION;
}
