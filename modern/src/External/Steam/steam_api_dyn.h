// SHIM: Steamworks (dynamically loaded)
//
// Replaces:  steam_api_dyn.h. Steamworks is free but proprietary and requires a
//            Steamworks agreement -- optional, per ../../../DEPENDENCIES.md.
// Status:    NO-OP. Not running under Steam.
//
// The original already loaded Steam dynamically, so a stub that reports "no Steam"
// is exactly the path the code takes when the DLL is absent.
#pragma once

#include <cstdint>

typedef uint64_t uint64_steamid;
typedef uint32_t uint32;
typedef uint64_t uint64_steam;

struct CSteamID
{
    uint64_t m_id = 0;
    uint64_t ConvertToUint64() const { return m_id; }
    bool     IsValid()         const { return m_id != 0; }
};

typedef uint32 HAuthTicket;

// The interface accessors below hand back a pointer to a single static instance whose
// every method reports failure. SteamHelper checks BLoggedOn() first and bails, so the
// rest is never reached -- but it has to exist for the file to compile.
class ISteamUser
{
public:
    bool     BLoggedOn()  { return false; }
    CSteamID GetSteamID() { return CSteamID(); }
    HAuthTicket GetAuthSessionTicket(void* /*pTicket*/, int /*cbMaxTicket*/, uint32* pcbTicket)
    {
        if (pcbTicket) *pcbTicket = 0;
        return 0;
    }
};

class ISteamUtils
{
public:
    const char* GetIPCountry() { return ""; }
    uint32      GetAppID()     { return 0; }
};

inline ISteamUser*  SteamUser()  { static ISteamUser  s; return &s; }
inline ISteamUtils* SteamUtils() { static ISteamUtils s; return &s; }

// ---------------------------------------------------------------------------
// Callbacks
//
// Steamworks delivers asynchronous results through CCallback<T, P, false>, declared
// with the STEAM_CALLBACK macro. Nothing dispatches here -- SteamAPI_RunCallbacks is
// a no-op -- so the registration object exists purely so the declarations compile.
// ---------------------------------------------------------------------------

// Result of a Steam Wallet micro-transaction authorization prompt.
struct MicroTxnAuthorizationResponse_t
{
    uint32   m_unAppID     = 0;
    uint64_t m_ulOrderID   = 0;
    uint8_t  m_bAuthorized = 0;
};

template <class T, class P>
class CCallback
{
public:
    CCallback() = default;
    CCallback(T* /*obj*/, void (T::*/*func*/)(P*)) {}
    void Register(T* /*obj*/, void (T::*/*func*/)(P*)) {}
    void Unregister() {}
};

// Declares the handler method plus the registration member, matching the shape the
// real macro produces so both the declaration and the out-of-line definition compile.
#define STEAM_CALLBACK(thisclass, func, param, var) \
    void func(param* pParam);                       \
    CCallback<thisclass, param> var

// Returns the count of entry points resolved -- 0 means Steam is unavailable, which
// is the branch SteamHelper::Init() takes.
inline int      SteamAPI_LoadDynamic() { return 0; }
inline void     SteamAPI_UnloadDynamic() {}

inline bool     SteamAPI_Init()        { return false; }
inline void     SteamAPI_Shutdown()    {}
inline void     SteamAPI_RunCallbacks(){}
inline bool     SteamAPI_IsSteamRunning() { return false; }
inline bool     SteamAPI_LoadLibrary(const char* = nullptr) { return false; }
inline void     SteamAPI_UnloadLibrary() {}
inline bool     SteamAPI_RestartAppIfNecessary(unsigned int) { return false; }
