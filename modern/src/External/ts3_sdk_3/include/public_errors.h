// SHIM: TeamSpeak 3 Client SDK -- error codes. See public_definitions.h for why.
//
// Only the codes the game actually compares against are defined. ERROR_not_implemented
// is what every shimmed entry point returns, so TeamSpeakClient's own error handling
// reports a clear reason rather than a silent failure.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

enum Ts3ErrorType
{
    ERROR_ok                = 0x0000,
    ERROR_undefined         = 0x0001,
    ERROR_not_implemented   = 0x0002,
    ERROR_ok_no_update      = 0x0003,
    ERROR_dont_notify       = 0x0004,
    ERROR_lib_time_limit_reached = 0x0005,

    ERROR_failed_connection_initialisation = 0x0403,
    ERROR_client_invalid_id = 0x0508,
};

#ifdef __cplusplus
}
#endif
