// SHIM: GameBlocks / FairFight anti-cheat client SDK
//
// Replaces:  GBClient/Inc/GBClient.h from the GameBlocks SDK (commercial; the product
//            became FairFight and is licensed per title). Absent from this drop --
//            server/GameBlocksSDK is gitignored. See ../../../../../DEPENDENCIES.md.
// Backed by: nothing. There is no permissive anti-cheat equivalent, and an anti-cheat
//            that a server operator can read the source of is not an anti-cheat.
// Status:    NO-OP. GBClient::Connect() reports a connection error and Connected()
//            returns false, so every guarded call site -- and they are all guarded,
//            `if(g_GameBlocks_Client && g_GameBlocks_Client->Connected())` -- is
//            skipped. The server runs with telemetry and cheat detection disabled.
//
// WHAT IS LOST: the server-side aimbot detector fed every player and zombie position
// each tick (ServerGameLogic.cpp), the weapon-cheat projectile-impact accounting, and
// the whole gameplay event stream (kills, chat, item pickups, god-mode attempts).
// Restoring any of it means either licensing FairFight or writing a replacement
// detector against these same call sites -- they are a reasonable seam for one.
//
// Clean-room declarations derived from call sites in server/src/WO_GameServer. No code
// originates from the GameBlocks SDK.

#pragma once

#include <cstdint>

namespace GameBlocks
{

// Opaque identifiers. The game constructs both from a uint32_t.
struct GBPublicPlayerId
{
    uint32_t id;
    GBPublicPlayerId(uint32_t v = 0) : id(v) {}
    operator uint32_t() const { return id; }
    // ServerGameLogic checks this before submitting a player to the aimbot detector.
    bool IsValid() const { return id != 0; }
};

typedef uint32_t GBPublicSourceId;

enum EGBClientError
{
    GBCLIENT_ERROR_OK = 0,
    GBCLIENT_ERROR_NOT_AVAILABLE,
    GBCLIENT_ERROR_CONNECT_FAILED,
};

// Passed to SetConfig. Only the commented-out branch of ServerGameLogic::Init builds
// one, but it has to be a complete type for that code to be revived.
struct ClientConfigData
{
    bool             gameblocksEnabled = false;
    const char*      serverAddress     = nullptr;
    int              serverPort        = 0;
    GBPublicSourceId sourceId          = 0;
    int              timeoutPeriod     = 0;
};

class GBClient
{
public:
    GBClient() = default;
    ~GBClient() = default;

    // ---- lifetime -------------------------------------------------------
    void SetConfig(const ClientConfigData&)                            {}
    EGBClientError Connect(const char* /*ip*/, int /*port*/, GBPublicSourceId /*src*/)
    { return GBCLIENT_ERROR_NOT_AVAILABLE; }
    EGBClientError ConnectUsingUrl(const char* /*url*/, int /*port*/, GBPublicSourceId /*src*/)
    { return GBCLIENT_ERROR_NOT_AVAILABLE; }
    void SetTimeoutPeriod(int)                                         {}
    void Close()                                                       {}
    void Tick()                                                        {}

    // Never connected -- this is the guard every call site checks first.
    bool Connected() const                                             { return false; }

    // ---- outgoing events ------------------------------------------------
    void PrepareEventForSending(const char* /*name*/, GBPublicSourceId /*src*/, GBPublicPlayerId /*player*/) {}
    void AddKeyValueInt(const char* /*key*/, int /*value*/)            {}
    void AddKeyValueFloat(const char* /*key*/, float /*value*/)        {}
    void AddKeyValueString(const char* /*key*/, const char* /*value*/) {}
    void AddKeyValueVector3D(const char* /*key*/, float /*x*/, float /*y*/, float /*z*/) {}
    void SendEvent()                                                   {}

    void Source_SetProperty(GBPublicSourceId, const char* /*key*/, const char* /*value*/) {}
    void Source_SetProperty(GBPublicSourceId, const char* /*key*/, int /*value*/)         {}

    // ---- incoming events ------------------------------------------------
    // The queue is always empty, so the read loop never runs.
    int  Incoming_EventCount() const                                   { return 0; }
    void Incoming_PrepareEventForReading()                             {}
    void Incoming_PopMessage()                                         {}
    const char*      Incoming_GetName() const                          { return ""; }
    GBPublicPlayerId Incoming_GetPlayerId() const                      { return GBPublicPlayerId(); }
    int  Incoming_GetNumPairs() const                                  { return 0; }
    bool Incoming_HasPairValueFloat(int /*index*/) const               { return false; }
    bool Incoming_GetPairValueFloat(float& out, int /*index*/) const   { out = 0.0f; return false; }
    bool Incoming_HasPairValueString(int /*index*/) const              { return false; }
    bool Incoming_GetPairValueString(char* out, int outSize, int /*index*/) const
    { if (out && outSize > 0) out[0] = 0; return false; }

    // ---- detectors ------------------------------------------------------
    void EnableAimBotDetector(int /*historyLen*/, float /*sampleRate*/)  {}
    void SetAimBotDetectorDebugPlayer(const char* /*playerId*/)          {}
    void AimBotDetector_BeginFrame()                                     {}
    void AimBotDetector_Add(GBPublicPlayerId, int /*weaponId*/, int /*team*/,
                            float /*px*/, float /*py*/, float /*pz*/,
                            float /*camX*/, float /*camY*/, float /*camZ*/,
                            float /*dirX*/, float /*dirY*/, float /*dirZ*/) {}
    void AimBotDetector_EndFrame()                                       {}

    void EnableWeaponCheatDetector(int /*windowSize*/)                   {}
    void WeaponCheatDetector_AddProjImpact(GBPublicPlayerId, int /*weaponId*/, bool /*hit*/,
                                           float /*distance*/, int /*bodyPart*/,
                                           int /*damage*/, int /*impactType*/) {}
};

} // namespace GameBlocks
