// SHIM: TeamSpeak 3 Server SDK -- serverlib-only definitions.
//
// Replaces:  serverlib_publicdefinitions.h from the TeamSpeak 3 SDK (commercial).
// See public_definitions.h for the full rationale.

#pragma once

#include "public_definitions.h"

#ifdef __cplusplus
extern "C" {
#endif

// Virtual-server properties, set with ts3server_setVirtualServerVariableAs*.
enum VirtualServerProperties
{
    VIRTUALSERVER_UNIQUE_IDENTIFIER = 0,
    VIRTUALSERVER_NAME,
    VIRTUALSERVER_WELCOMEMESSAGE,
    VIRTUALSERVER_PLATFORM,
    VIRTUALSERVER_VERSION,
    VIRTUALSERVER_MAXCLIENTS,
    VIRTUALSERVER_PASSWORD,
    VIRTUALSERVER_CLIENTS_ONLINE,
    VIRTUALSERVER_CHANNELS_ONLINE,
    VIRTUALSERVER_CREATED,
    VIRTUALSERVER_UPTIME,
    VIRTUALSERVER_ENDMARKER,
};

// Channel properties, set with ts3server_setChannelVariableAs*.
enum ChannelProperties
{
    CHANNEL_NAME = 0,
    CHANNEL_TOPIC,
    CHANNEL_DESCRIPTION,
    CHANNEL_PASSWORD,
    CHANNEL_CODEC,
    CHANNEL_CODEC_QUALITY,
    CHANNEL_MAXCLIENTS,
    CHANNEL_MAXFAMILYCLIENTS,
    CHANNEL_ORDER,
    CHANNEL_FLAG_PERMANENT,
    CHANNEL_FLAG_SEMI_PERMANENT,
    CHANNEL_FLAG_DEFAULT,
    CHANNEL_FLAG_PASSWORD,
    CHANNEL_ENDMARKER,
};

#ifdef __cplusplus
}
#endif
