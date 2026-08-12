// SHIM: Steamworks (dynamically loaded) -- implementation side.
//
// The original steam_api_dyn.cpp resolved every Steamworks entry point through
// GetProcAddress at runtime, which is why SteamHelper.cpp #includes the .cpp rather
// than linking it: the loader and the declarations had to share one translation unit.
//
// Nothing to implement here. Every function in steam_api_dyn.h is an inline stub that
// reports "Steam is not running", which is the same path the original took when
// steam_api.dll was absent -- so CSteamHelper::Init() sets IS_ENABLED = false and the
// game runs standalone.
//
// This file exists only because SteamHelper.cpp includes it by name.
