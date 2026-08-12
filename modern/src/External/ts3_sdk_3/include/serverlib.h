// SHIM: TeamSpeak 3 Server SDK -- the serverlib entry points.
//
// Replaces:  serverlib.h from the TeamSpeak 3 SDK (commercial). See
//            public_definitions.h for the rationale and ../../../../DEPENDENCIES.md
//            for the audit.
// Status:    NO-OP. The game server hosts no voice server.
//
// Contract (see ../../README.md): declare what is referenced, fail honestly, never
// fake success. ts3server_initServerLib() returns ERROR_not_implemented, so
// CTeamSpeakServer::Start() takes its error path on the first call and the subsystem
// never engages. Getters null their out-parameters so no caller frees a pointer it
// did not get.
//
// Clean-room declarations derived from the call sites in
// server/src/WO_GameServer/Sources/TeamSpeakServer.cpp. No code originates from the
// TeamSpeak SDK.

#pragma once

#include "public_definitions.h"
#include "public_errors.h"
#include "serverlib_publicdefinitions.h"

#include <cstddef>

// The SDK's callback table. Only the members TeamSpeakServer.cpp assigns are declared;
// it memsets the whole struct first, so unlisted callbacks would be null anyway.
struct ServerLibFunctions
{
    void (*onClientConnected)(uint64 serverID, anyID clientID, uint64 channelID, unsigned int* removeClientError);
    void (*onClientDisconnected)(uint64 serverID, anyID clientID, uint64 channelID);
    void (*onClientMoved)(uint64 serverID, anyID clientID, uint64 oldChannelID, uint64 newChannelID);
    void (*onChannelCreated)(uint64 serverID, anyID invokerClientID, uint64 channelID);
    void (*onChannelEdited)(uint64 serverID, anyID invokerClientID, uint64 channelID);
    void (*onChannelDeleted)(uint64 serverID, anyID invokerClientID, uint64 channelID);
    void (*onServerTextMessageEvent)(uint64 serverID, anyID invokerClientID, const char* textMessage);
    void (*onChannelTextMessageEvent)(uint64 serverID, anyID invokerClientID, uint64 targetChannelID, const char* textMessage);
    void (*onUserLoggingMessageEvent)(const char* logMessage, int logLevel, const char* logChannel, uint64 logID, const char* logTime, const char* completeLogString);
    void (*onClientStartTalkingEvent)(uint64 serverID, anyID clientID);
    void (*onClientStopTalkingEvent)(uint64 serverID, anyID clientID);
    void (*onAccountingErrorEvent)(uint64 serverID, unsigned int errorCode);
    void (*onCustomPacketEncryptEvent)(char** dataToSend, unsigned int* sizeOfData);
    void (*onCustomPacketDecryptEvent)(char** dataReceived, unsigned int* dataReceivedSize);
};

// ---------------------------------------------------------------------------
// Every function below is a no-op. Definitions are inline so the shim needs no
// translation unit of its own.
// ---------------------------------------------------------------------------

// Library lifetime
inline unsigned int ts3server_initServerLib(const struct ServerLibFunctions*, int /*usedLogTypes*/,
                                            const char* /*logFileFolder*/)              { return ERROR_not_implemented; }
inline unsigned int ts3server_destroyServerLib()                                        { return ERROR_not_implemented; }
inline unsigned int ts3server_getServerLibVersion(char** result)                        { if (result) *result = nullptr; return ERROR_not_implemented; }
inline void         ts3server_freeMemory(void*)                                         {}
inline unsigned int ts3server_getGlobalErrorMessage(unsigned int /*errorCode*/, char** error) { if (error) *error = nullptr; return ERROR_not_implemented; }

// Virtual servers
inline unsigned int ts3server_createVirtualServer(int /*port*/, const char* /*ip*/, const char* /*name*/,
                                                  const char* /*keyPair*/, int /*maxClients*/,
                                                  uint64* result)                       { if (result) *result = 0; return ERROR_not_implemented; }
inline unsigned int ts3server_stopVirtualServer(uint64)                                 { return ERROR_not_implemented; }
inline unsigned int ts3server_getVirtualServerList(uint64** result)                     { if (result) *result = nullptr; return ERROR_not_implemented; }
inline unsigned int ts3server_getVirtualServerKeyPair(uint64, char** result)            { if (result) *result = nullptr; return ERROR_not_implemented; }
inline unsigned int ts3server_setVirtualServerVariableAsInt(uint64, std::size_t /*flag*/, int /*value*/)             { return ERROR_not_implemented; }
inline unsigned int ts3server_setVirtualServerVariableAsString(uint64, std::size_t /*flag*/, const char* /*value*/)  { return ERROR_not_implemented; }
inline unsigned int ts3server_flushVirtualServerVariable(uint64)                        { return ERROR_not_implemented; }

// Channels
inline unsigned int ts3server_getChannelVariableAsString(uint64, uint64 /*channelID*/, std::size_t /*flag*/, char** result) { if (result) *result = nullptr; return ERROR_not_implemented; }
inline unsigned int ts3server_setChannelVariableAsInt(uint64, uint64 /*channelID*/, std::size_t /*flag*/, int /*value*/)            { return ERROR_not_implemented; }
inline unsigned int ts3server_setChannelVariableAsString(uint64, uint64 /*channelID*/, std::size_t /*flag*/, const char* /*value*/) { return ERROR_not_implemented; }
inline unsigned int ts3server_flushChannelVariable(uint64, uint64 /*channelID*/)        { return ERROR_not_implemented; }
inline unsigned int ts3server_flushChannelCreation(uint64, uint64 /*parentChannelID*/, uint64* result) { if (result) *result = 0; return ERROR_not_implemented; }

// Clients
inline unsigned int ts3server_getClientVariableAsString(uint64, anyID, std::size_t /*flag*/, char** result) { if (result) *result = nullptr; return ERROR_not_implemented; }
inline unsigned int ts3server_setClientVariableAsString(uint64, anyID, std::size_t /*flag*/, const char* /*value*/) { return ERROR_not_implemented; }
inline unsigned int ts3server_flushClientVariable(uint64, anyID)                        { return ERROR_not_implemented; }
