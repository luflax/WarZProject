// SHIM: TeamSpeak 3 Client SDK -- shared type definitions.
//
// Replaces:  public_definitions.h from the TeamSpeak 3 SDK (commercial; per-title
//            licence negotiated with TeamSpeak Systems GmbH). See ../../../../DEPENDENCIES.md.
// Backed by: nothing. Voice chat is absent in the port.
//
// There is no permissive drop-in for TeamSpeak. Replacing it means picking a
// different voice stack entirely (Opus + a self-hosted mixer is the obvious route),
// which is a feature project, not a porting task. Until then EclipseStudio's
// TeamSpeakClient.cpp compiles against these declarations and every entry point
// returns an error, so CTeamSpeakClient::Init() fails cleanly and the game runs mute.
//
// Clean-room declarations derived from the call sites in
// EclipseStudio/Sources/TeamSpeakClient.{h,cpp}. No code originates from the
// TeamSpeak SDK.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned short     anyID;
#ifndef _UINT64_DEFINED_TS3
#define _UINT64_DEFINED_TS3
typedef unsigned long long uint64;
#endif

// 3D positional audio takes these by pointer; the fields are read by the caller
// when it fills them in, so they need real members.
typedef struct
{
    float x;
    float y;
    float z;
} TS3_VECTOR;

// Connection status reported through onConnectStatusChangeEvent.
enum ConnectStatus
{
    STATUS_DISCONNECTED = 0,
    STATUS_CONNECTING,
    STATUS_CONNECTED,
    STATUS_CONNECTION_ESTABLISHING,
    STATUS_CONNECTION_ESTABLISHED,
};

// Talk status reported through onTalkStatusChangeEvent.
enum TalkStatus
{
    STATUS_NOT_TALKING = 0,
    STATUS_TALKING,
    STATUS_TALKING_WHILE_DISABLED,
};

// Client properties queried with ts3client_getClientVariableAsString and set with
// ts3client_setClientSelfVariableAsInt.
enum ClientProperties
{
    CLIENT_UNIQUE_IDENTIFIER = 0,
    CLIENT_NICKNAME,
    CLIENT_VERSION,
    CLIENT_PLATFORM,
    CLIENT_FLAG_TALKING,
    CLIENT_INPUT_MUTED,
    CLIENT_OUTPUT_MUTED,
    CLIENT_OUTPUTONLY_MUTED,
    CLIENT_INPUT_HARDWARE,
    CLIENT_OUTPUT_HARDWARE,
    CLIENT_INPUT_DEACTIVATED,
    CLIENT_IS_RECORDING,
    CLIENT_ENDMARKER,
};

enum InputDeactivationStatus
{
    INPUT_ACTIVE      = 0,
    INPUT_DEACTIVATED = 1,
};

enum LogTypes
{
    LogType_NONE        = 0x0000,
    LogType_FILE        = 0x0001,
    LogType_CONSOLE     = 0x0002,
    LogType_USERLOGGING = 0x0004,
    LogType_NO_NETLOGGING = 0x0008,
    LogType_DATABASE    = 0x0010,
};

enum LogLevel
{
    LogLevel_CRITICAL = 0,
    LogLevel_ERROR,
    LogLevel_WARNING,
    LogLevel_DEBUG,
    LogLevel_INFO,
    LogLevel_DEVEL,
};

enum Visibility
{
    ENTER_VISIBILITY = 0,
    RETAIN_VISIBILITY,
    LEAVE_VISIBILITY,
};

#ifdef __cplusplus
}
#endif
