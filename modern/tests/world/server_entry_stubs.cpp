// Stands in for ServerMain.cpp in the test binary.
//
// The engine-linked tests compile the GameServer's own sources, minus ServerMain.cpp --
// which is excluded because it defines main(), and the test harness brings its own
// (tests/framework/warz_test.cpp). ServerMain.cpp is not only an entry point though: it
// also owns a handful of globals and the game:: entry stubs that the rest of the server
// links against. Those are re-provided here.
//
// Everything below is deliberately inert. This file exists so the test binary LINKS,
// not so it can boot a server -- the boot path is what MILESTONE-C-PREWORK.md §1.4 and
// §1.5 are about, and it needs a MasterServer, a SupervisorServer and a backend that
// no unit test should be standing up.

#include "r3dPCH.h"
#include "r3d.h"

// ---------------------------------------------------------------------------
// game:: entry points
//
// ServerMain.cpp defines these as r3dError("not a gfx app") -- the server links against
// the client's headers and has to satisfy the symbols without ever drawing. Keeping
// them fatal here is deliberate: if a test ever reaches one, that is a real finding
// about what the code under test is doing, not something to swallow.
// ---------------------------------------------------------------------------

void game::Shutdown(void) { r3dError("game::Shutdown called from a test"); }
void game::MainLoop(void) { r3dError("game::MainLoop called from a test"); }
void game::Init(void)     { r3dError("game::Init called from a test"); }
void game::PreInit(void)  { r3dError("game::PreInit called from a test"); }

// ---------------------------------------------------------------------------
// Configuration globals owned by ServerMain.cpp
//
// Non-static there, so other translation units reference them directly. The values are
// the ones ServerMain.cpp initialises them to before ParseArgs runs, which is the state
// a test should see: unconfigured.
// ---------------------------------------------------------------------------

int     cfg_hostPort   = 0;
__int64 cfg_sessionId  = 0;
int     cfg_uploadLogs = 0;

// Russian-client flag, read by the localisation paths.
int     RUS_CLIENT     = 0;

// ---------------------------------------------------------------------------
// gameServerLoop
//
// The real one connects to the SupervisorServer, fetches shop data, game rewards and
// lootbox data from the backend, and only then calls PlayGameServer. Three of those
// call r3dError on failure and the backend is not reachable from a test, which is
// exactly why the tests drive the subsystems below this function instead of calling it.
// ---------------------------------------------------------------------------

void gameServerLoop()
{
    r3dError("gameServerLoop called from a test -- drive the subsystem directly instead");
}
