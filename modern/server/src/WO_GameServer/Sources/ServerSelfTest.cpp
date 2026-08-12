// [PORT] New file. Milestone C, decomposed into stages that can be run one at a time.
//
// WHY
//
// PHASE1-BUILD-PLAN.md:170 states Milestone C's server criterion as one sentence --
// "boots, parses config, loads a level via LoadLevel_Objects, enters the PlayGameServer
// tick loop, accepts a socket connection" -- and that bundles four independent claims
// into a single bit. When it fails, which it will the first time it is run, "the server
// did not boot" says nothing about which of the four broke.
//
// Worse, the real boot path cannot reach those steps at all in a development
// environment. gameServerLoop (ServerMain.cpp:95) connects to a SupervisorServer, then
// calls ApiGetShopData, downloadGameRewards and downloadLootBoxData -- the last two
// call r3dError, which is fatal, if the backend does not answer. So the server dies
// roughly 60% of the way through its own boot function, before PlayGameServer is ever
// entered, unless a MasterServer, a SupervisorServer, IIS and MSSQL are all up.
// MILESTONE-C-PREWORK.md 1.4 and 1.5 are about exactly that.
//
// The observation this file is built on: EVERY external dependency lives in the outer
// shell. PlayGameServer's own initialisation -- world, physics, level, collections --
// needs no backend, no master server and no socket. So the stages below drive those
// subsystems directly, in the same order the real boot does, and each one either passes
// or names the subsystem that failed.
//
// WHAT THIS IS NOT
//
// Not a substitute for running the real thing. A stage that passes says its subsystem
// initialises and ticks, not that the game is playable. The point is that when the real
// boot is finally attempted, everything below it has already been eliminated.
//
// USAGE
//
//     GameServer.exe --selftest=<stage> [--level=<dir>] [--ticks=<n>]
//
//     config   process starts, log opens, console vars register
//     world    + ObjectManager and PhysX initialise, tick, and tear down
//     level    + a level loads from --level and its objects are counted
//     all      every stage above, in order
//
// Exit code is 0 on success and non-zero on the first stage that fails, so a shell or
// CTest can consume it directly.

#include "r3dPCH.h"
#include "r3d.h"

#include "ServerSelfTest.h"

#include "gameobjects/ObjManag.h"
#include "../../GameEngine/gameobjects/PhysXWorld.h"
#include "../EclipseStudio/Sources/GameLevel.h"
#include "../EclipseStudio/Sources/Editors/CollectionsManager.h"

#include <cstring>
#include <cstdlib>

extern int LoadLevel_Objects(float BarRange);

// Declared at file scope, NOT inside the anonymous namespace below: a declaration in an
// unnamed namespace has internal linkage, so it would resolve to a local symbol that
// nothing defines rather than to the engine's RegisterAllVars (SF/Console).
extern void RegisterAllVars();

namespace {

// Stage results are reported in one fixed format so a harness can grep them and a human
// can read them. The subsystem name is always the first word after the verdict.
void stage_pass(const char* stage, const char* detail)
{
    r3dOutToLog("SELFTEST PASS %s%s%s\n", stage, detail ? " -- " : "", detail ? detail : "");
}

void stage_fail(const char* stage, const char* detail)
{
    r3dOutToLog("SELFTEST FAIL %s%s%s\n", stage, detail ? " -- " : "", detail ? detail : "");
}

// ---------------------------------------------------------------------------
// C0 -- the process runs at all
// ---------------------------------------------------------------------------
bool stage_config()
{
    // RegisterAllVars populates the console-variable table that most of the engine
    // reads its configuration out of. It runs before anything else in the real boot
    // (ServerMain.cpp, just before gameServerLoop), and a failure here is a static
    // initialisation problem -- the kind that is invisible until something reads a var
    // that was never registered.
    RegisterAllVars();

    stage_pass("config", "log open, console vars registered");
    return true;
}

// ---------------------------------------------------------------------------
// C1 -- world and physics come up, tick, and go down again
// ---------------------------------------------------------------------------
bool stage_world(int ticks)
{
    GameWorld_Create();
    GameWorld().Init(OBJECTMANAGER_MAXOBJECTS, OBJECTMANAGER_MAXSTATICOBJECTS);

    if(!GameWorld().bInited)
    {
        stage_fail("world", "ObjectManager did not initialise");
        return false;
    }

    // PhysX 4.1, vendored and built from source for MinGW-i686 -- a configuration
    // NVIDIA does not ship. This is the first time in the port that it is asked to
    // actually create a scene rather than merely link.
    r3d_assert(g_pPhysicsWorld == NULL);
    g_pPhysicsWorld = new PhysXWorld;
    g_pPhysicsWorld->Init();

    // Tick an empty world. Empty is deliberate: this stage is about the loop itself --
    // frame timing, the ObjectManager's per-frame walk, PhysX's simulate/fetch pair --
    // with nothing in it that could fail for its own reasons.
    r3dResetFrameTime();
    for(int i = 0; i < ticks; i++)
    {
        r3dEndFrame();
        r3dStartFrame();
        GameWorld().Update();
        GameWorld().EndFrame();
    }

    GameWorld().Destroy();
    GameWorld_Destroy();

    SAFE_DELETE(g_pPhysicsWorld);

    char detail[128];
    sprintf(detail, "%d ticks, physics up and down cleanly", ticks);
    stage_pass("world", detail);
    return true;
}

// ---------------------------------------------------------------------------
// C2 -- a level loads
// ---------------------------------------------------------------------------
bool stage_level(const char* levelDir, int ticks)
{
    if(levelDir == NULL || levelDir[0] == 0)
    {
        stage_fail("level", "no --level=<dir> given");
        return false;
    }

    GameWorld_Create();
    GameWorld().Init(OBJECTMANAGER_MAXOBJECTS, OBJECTMANAGER_MAXSTATICOBJECTS);

    r3d_assert(g_pPhysicsWorld == NULL);
    g_pPhysicsWorld = new PhysXWorld;
    g_pPhysicsWorld->Init();

    r3dGameLevel::SetHomeDir(levelDir);

    LoadLevel_Objects(1.0f);
    gCollectionsManager.Init(0, 1);

    const int numObjects = GameWorld().GetNumObjects();
    const int numStatic  = GameWorld().GetStaticObjectCount();

    // An empty world is the failure this stage exists to catch. It is what a level path
    // that does not resolve looks like, and it is ALSO what a class table that failed to
    // link looks like -- objects are constructed from a className string, so a dropped
    // static initialiser produces a silent, empty, entirely successful-looking load.
    if(numObjects == 0 && numStatic == 0)
    {
        stage_fail("level", "loaded zero objects -- bad level path, or the class table is empty");
        GameWorld().Destroy();
        GameWorld_Destroy();
        SAFE_DELETE(g_pPhysicsWorld);
        return false;
    }

    r3dResetFrameTime();
    for(int i = 0; i < ticks; i++)
    {
        r3dEndFrame();
        r3dStartFrame();
        GameWorld().Update();
        GameWorld().EndFrame();
    }

    GameWorld().Destroy();
    GameWorld_Destroy();
    SAFE_DELETE(g_pPhysicsWorld);

    char detail[256];
    sprintf(detail, "%d objects + %d static from '%s', %d ticks",
            numObjects, numStatic, levelDir, ticks);
    stage_pass("level", detail);
    return true;
}

} // namespace

// ---------------------------------------------------------------------------

bool SelfTest_ParseArgs(int argc, char* argv[], SelfTestOptions* out)
{
    out->stage    = NULL;
    out->levelDir = NULL;
    out->ticks    = 100;

    for(int i = 1; i < argc; i++)
    {
        if(strncmp(argv[i], "--selftest=", 11) == 0)
            out->stage = argv[i] + 11;
        else if(strncmp(argv[i], "--level=", 8) == 0)
            out->levelDir = argv[i] + 8;
        else if(strncmp(argv[i], "--ticks=", 8) == 0)
            out->ticks = atoi(argv[i] + 8);
    }

    return out->stage != NULL;
}

int SelfTest_Run(const SelfTestOptions& opt)
{
    r3dOutToLog("SELFTEST starting stage '%s'\n", opt.stage);

    const bool all = strcmp(opt.stage, "all") == 0;
    bool ok = true;

    // Ordered deliberately: each stage depends on everything before it, so the FIRST
    // failure is the informative one and there is no value in continuing past it.
    if(ok && (all || strcmp(opt.stage, "config") == 0))
        ok = stage_config();

    if(ok && (all || strcmp(opt.stage, "world") == 0))
        ok = stage_world(opt.ticks);

    if(ok && (all || strcmp(opt.stage, "level") == 0))
        ok = stage_level(opt.levelDir, opt.ticks);

    if(!all
       && strcmp(opt.stage, "config") != 0
       && strcmp(opt.stage, "world")  != 0
       && strcmp(opt.stage, "level")  != 0)
    {
        r3dOutToLog("SELFTEST FAIL unknown stage '%s' "
                    "(expected: config, world, level, all)\n", opt.stage);
        return 2;
    }

    r3dOutToLog("SELFTEST %s\n", ok ? "PASSED" : "FAILED");
    return ok ? 0 : 1;
}
