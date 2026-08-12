// SHIM: TeamSpeak 3 Client SDK -- clientlib-only definitions.
// See public_definitions.h for why this exists.

#pragma once

#include "public_definitions.h"

#ifdef __cplusplus
extern "C" {
#endif

enum LocalTestMode
{
    TEST_MODE_OFF = 0,
    TEST_MODE_VOICE_LOCAL_ONLY,
    TEST_MODE_VOICE_LOCAL_AND_SERVER,
};

#ifdef __cplusplus
}
#endif
