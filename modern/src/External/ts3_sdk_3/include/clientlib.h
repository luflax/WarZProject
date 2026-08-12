// SHIM: TeamSpeak 3 Client SDK -- the clientlib entry points.
//
// Replaces:  clientlib.h from the TeamSpeak 3 SDK (commercial). See public_definitions.h
//            for the full rationale and ../../../../DEPENDENCIES.md for the audit.
// Status:    NO-OP. There is no voice chat.
//
// Contract (see ../../README.md): declare what is referenced, fail honestly, never
// fake success. Every entry point returns ERROR_not_implemented, so
// CTeamSpeakClient::Init() takes its TSHandleError path on the first call and the
// rest of the subsystem never engages. Getters that hand back an allocation instead
// return null through the out-parameter, so no caller frees a pointer it did not get.
//
// Clean-room declarations derived from the call sites in
// EclipseStudio/Sources/TeamSpeakClient.cpp. No code originates from the TeamSpeak SDK.

#pragma once

#include "public_definitions.h"
#include "public_errors.h"
#include "clientlib_publicdefinitions.h"

#include <cstddef>

// The SDK's callback table. Only the members TeamSpeakClient.cpp assigns are declared;
// it memsets the whole struct first, so unlisted callbacks would be null anyway.
struct ClientUIFunctions
{
    void (*onConnectStatusChangeEvent)(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber);
    void (*onNewChannelEvent)(uint64 serverConnectionHandlerID, uint64 channelID, uint64 channelParentID);
    void (*onNewChannelCreatedEvent)(uint64 serverConnectionHandlerID, uint64 channelID, uint64 channelParentID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier);
    void (*onDelChannelEvent)(uint64 serverConnectionHandlerID, uint64 channelID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier);
    void (*onClientMoveEvent)(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* moveMessage);
    void (*onClientMoveSubscriptionEvent)(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility);
    void (*onClientMoveTimeoutEvent)(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* timeoutMessage);
    void (*onClientKickFromChannelEvent)(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, const char* kickMessage);
    void (*onClientKickFromServerEvent)(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char* kickerName, const char* kickerUniqueIdentifier, const char* kickMessage);
    void (*onServerErrorEvent)(uint64 serverConnectionHandlerID, const char* errorMessage, unsigned int error, const char* returnCode, const char* extraMessage);
    void (*onTalkStatusChangeEvent)(uint64 serverConnectionHandlerID, int status, int isReceivedWhisper, anyID clientID);
    void (*onIgnoredWhisperEvent)(uint64 serverConnectionHandlerID, anyID clientID);
    void (*onEditMixedPlaybackVoiceDataEvent)(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask);
    void (*onEditCapturedVoiceDataEvent)(uint64 serverConnectionHandlerID, short* samples, int sampleCount, int channels, int* edited);
    void (*onCustom3dRolloffCalculationClientEvent)(uint64 serverConnectionHandlerID, anyID clientID, float distance, float* volume);
    void (*onUserLoggingMessageEvent)(const char* logMessage, int logLevel, const char* logChannel, uint64 logID, const char* logTime, const char* completeLogString);
    void (*onCustomPacketEncryptEvent)(char** dataToSend, unsigned int* sizeOfData);
    void (*onCustomPacketDecryptEvent)(char** dataReceived, unsigned int* dataReceivedSize);
    void (*onProvisioningSlotRequestResultEvent)(unsigned int error, uint64 requestHandle, const char* connectionKey);
};

struct ClientUIFunctionsRare;

// ---------------------------------------------------------------------------
// Every function below is a no-op. Definitions are inline so the shim needs no
// translation unit of its own.
// ---------------------------------------------------------------------------

// Library lifetime
inline unsigned int ts3client_initClientLib(const struct ClientUIFunctions*, const struct ClientUIFunctionsRare*,
                                           int /*usedLogTypes*/, const char* /*logFileFolder*/,
                                           const char* /*resourcesFolder*/)                     { return ERROR_not_implemented; }
inline unsigned int ts3client_destroyClientLib()                                                { return ERROR_not_implemented; }
inline unsigned int ts3client_getClientLibVersion(char** result)                                { if (result) *result = nullptr; return ERROR_not_implemented; }
inline unsigned int ts3client_createIdentity(char** result)                                     { if (result) *result = nullptr; return ERROR_not_implemented; }

// The SDK hands ownership of strings to the caller; nothing is ever handed out here,
// so this only has to tolerate being called with null.
inline void         ts3client_freeMemory(void*)                                                 {}
inline unsigned int ts3client_getErrorMessage(unsigned int /*errorCode*/, char** error)         { if (error) *error = nullptr; return ERROR_not_implemented; }

// Server connection handlers
inline unsigned int ts3client_spawnNewServerConnectionHandler(int /*port*/, uint64* result)     { if (result) *result = 0; return ERROR_not_implemented; }
inline unsigned int ts3client_destroyServerConnectionHandler(uint64)                            { return ERROR_not_implemented; }
inline unsigned int ts3client_startConnection(uint64, const char* /*identity*/, const char* /*ip*/,
                                              unsigned int /*port*/, const char* /*nickname*/,
                                              const char** /*defaultChannelArray*/,
                                              const char* /*defaultChannelPassword*/,
                                              const char* /*serverPassword*/)                   { return ERROR_not_implemented; }
inline unsigned int ts3client_stopConnection(uint64, const char* /*quitMessage*/)               { return ERROR_not_implemented; }
inline unsigned int ts3client_getClientID(uint64, anyID* result)                                { if (result) *result = 0; return ERROR_not_implemented; }

// Sound modes and devices
inline unsigned int ts3client_getPlaybackModeList(char*** result)                               { if (result) *result = nullptr; return ERROR_not_implemented; }
inline unsigned int ts3client_getCaptureModeList(char*** result)                                { if (result) *result = nullptr; return ERROR_not_implemented; }
inline unsigned int ts3client_getDefaultPlayBackMode(char** result)                             { if (result) *result = nullptr; return ERROR_not_implemented; }
inline unsigned int ts3client_getDefaultCaptureMode(char** result)                              { if (result) *result = nullptr; return ERROR_not_implemented; }
inline unsigned int ts3client_getPlaybackDeviceList(const char* /*modeID*/, char**** result)    { if (result) *result = nullptr; return ERROR_not_implemented; }
inline unsigned int ts3client_getCaptureDeviceList(const char* /*modeID*/, char**** result)     { if (result) *result = nullptr; return ERROR_not_implemented; }
inline unsigned int ts3client_getDefaultPlaybackDevice(const char* /*modeID*/, char*** result)  { if (result) *result = nullptr; return ERROR_not_implemented; }
inline unsigned int ts3client_getDefaultCaptureDevice(const char* /*modeID*/, char*** result)   { if (result) *result = nullptr; return ERROR_not_implemented; }
inline unsigned int ts3client_openPlaybackDevice(uint64, const char* /*modeID*/, const char* /*device*/) { return ERROR_not_implemented; }
inline unsigned int ts3client_openCaptureDevice(uint64, const char* /*modeID*/, const char* /*device*/)  { return ERROR_not_implemented; }
inline unsigned int ts3client_closePlaybackDevice(uint64)                                       { return ERROR_not_implemented; }
inline unsigned int ts3client_closeCaptureDevice(uint64)                                        { return ERROR_not_implemented; }

// Client variables
inline unsigned int ts3client_getClientVariableAsString(uint64, anyID, std::size_t /*flag*/, char** result) { if (result) *result = nullptr; return ERROR_not_implemented; }
inline unsigned int ts3client_setClientSelfVariableAsInt(uint64, std::size_t /*flag*/, int /*value*/)       { return ERROR_not_implemented; }
inline unsigned int ts3client_flushClientSelfUpdates(uint64, const char* /*returnCode*/)                    { return ERROR_not_implemented; }

// Codec / preprocessor / playback configuration
inline unsigned int ts3client_getEncodeConfigValue(uint64, const char* /*ident*/, char** result)            { if (result) *result = nullptr; return ERROR_not_implemented; }
inline unsigned int ts3client_setPreProcessorConfigValue(uint64, const char* /*ident*/, const char* /*value*/) { return ERROR_not_implemented; }
inline unsigned int ts3client_setPlaybackConfigValue(uint64, const char* /*ident*/, const char* /*value*/)     { return ERROR_not_implemented; }
inline unsigned int ts3client_getPlaybackConfigValueAsFloat(uint64, const char* /*ident*/, float* result)      { if (result) *result = 0.0f; return ERROR_not_implemented; }

// 3D positional audio
inline unsigned int ts3client_systemset3DListenerAttributes(uint64, const TS3_VECTOR* /*position*/,
                                                            const TS3_VECTOR* /*forward*/,
                                                            const TS3_VECTOR* /*up*/)           { return ERROR_not_implemented; }
inline unsigned int ts3client_channelset3DAttributes(uint64, anyID /*clientID*/, const TS3_VECTOR* /*position*/) { return ERROR_not_implemented; }

// Muting
inline unsigned int ts3client_requestMuteClients(uint64, const anyID* /*clientIDArray*/, const char* /*returnCode*/)   { return ERROR_not_implemented; }
inline unsigned int ts3client_requestUnmuteClients(uint64, const anyID* /*clientIDArray*/, const char* /*returnCode*/) { return ERROR_not_implemented; }
