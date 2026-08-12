// SHIM: GameBlocks / FairFight -- the SDK's predefined event helpers.
// See GBClient.h for the rationale; all of these are no-ops.
//
// Clean-room declarations derived from call sites in
// server/src/WO_GameServer/Sources/ServerGameLogic.cpp and obj_ServerPlayer.cpp.

#pragma once

#include "GBClient.h"

// Reserved property key for "the map changed". A plain string in the real SDK.
#define GB_RESERVED_EVENT_SOURCEINFO_KEY_PROP_MAP_NEW "map_new"

namespace GameBlocks
{

inline void Event_PlayerJoin_Send(GBClient*, GBPublicSourceId, GBPublicPlayerId,
                                  const char* /*name*/, const char* /*ip*/) {}
inline void Event_PlayerLeave_Send(GBClient*, GBPublicSourceId, GBPublicPlayerId,
                                   const char* /*name*/) {}
inline void Event_PlayerCount_Send(GBClient*, GBPublicSourceId, int /*count*/) {}
inline void Event_MaxPlayerCount_Send(GBClient*, GBPublicSourceId, int /*count*/) {}

// Batched list events: Prepare, then Push per player, then Send.
inline void Event_PlayerList_Prepare(GBClient*, GBPublicSourceId) {}
inline void Event_PlayerList_Push(GBClient*, GBPublicPlayerId, const char* /*name*/,
                                  const char* /*ip*/, int /*team*/, int /*timeAlive*/,
                                  int /*score*/) {}
inline void Event_PlayerList_Send(GBClient*) {}

inline void Event_PlayerLocationList_Prepare(GBClient*, GBPublicSourceId) {}
inline void Event_PlayerLocationList_Push(GBClient*, GBPublicPlayerId,
                                          float /*px*/, float /*py*/, float /*pz*/,
                                          float /*dx*/, float /*dy*/, float /*dz*/,
                                          int /*vehicleType*/, int /*seat*/) {}
inline void Event_PlayerLocationList_Send(GBClient*) {}

inline void Event_AimBotDetect_Send(GBClient*, GBPublicSourceId,
                                    const char* /*detail*/, const char* /*detail2*/) {}
// The weapon-cheat variant names only the offending player; the aimbot one names both
// the shooter and the target.
inline void Event_WeaponCheatDetect_Send(GBClient*, GBPublicSourceId,
                                         const char* /*playerId*/) {}
inline void Event_PlayerScreenShotJpg_Send(GBClient*, GBPublicSourceId, GBPublicPlayerId,
                                           const void* /*jpegData*/, int /*size*/) {}

} // namespace GameBlocks
