# The three server binaries.
#
# Ordered smallest first, which is also the order Milestone B links them in: each one
# adds dependency surface over the last, so the first link is small enough to debug.
#
#   SupervisorServer   10 sources  -- per-machine process babysitter. No PhysX.
#   MasterServer       11 + 3      -- server browser, matchmaking, sessions. No PhysX.
#   GameServer         53 + 28     -- one process per game instance. PhysX + Detour.
#
# All three reuse the engine, so they link Eternity_server / GameEngine_server -- the
# WO_SERVER build, which strips rendering. ServerGame.cpp stubs out every rendering
# global so the server links against the deferred-renderer headers without ever drawing.
#
# The "_SHARED" lists are sources OUTSIDE server/ that each project compiles ITSELF with
# WO_SERVER defined -- mostly EclipseStudio files reused server-side. They take
# different #ifdef branches than the client build, so they are genuinely compiled twice
# across the product and cannot be linked from the client's objects.

include(${WARZ_ROOT}/cmake/sources/SupervisorServer.cmake)
include(${WARZ_ROOT}/cmake/sources/SupervisorServer_shared.cmake)
include(${WARZ_ROOT}/cmake/sources/MasterServer.cmake)
include(${WARZ_ROOT}/cmake/sources/MasterServer_shared.cmake)
include(${WARZ_ROOT}/cmake/sources/WO_GameServer.cmake)
include(${WARZ_ROOT}/cmake/sources/WO_GameServer_shared.cmake)

# RakNet, pugixml and the Win32 import libraries are NOT listed here: Eternity declares
# them PUBLIC, so they arrive transitively and -- crucially -- in an order CMake works
# out from the dependency graph. Listing them here as well put them ahead of
# libEternity_server.a on the link line, which GNU ld resolves left to right, and every
# symbol Eternity referenced from them came out undefined.

function(warz_add_server name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEFINES;INCLUDES;LIBS" ${ARGN})

    add_executable(${name} ${ARG_SOURCES})
    set_target_properties(${name} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        OUTPUT_NAME ${name}
    )
    warz_configure_server_target(${name} "${ARG_DEFINES}" "${ARG_INCLUDES}" "${ARG_LIBS}")
endfunction()

# The compile/link configuration shared by a server binary and by anything else built
# from the same sources -- currently the GameServer test harness. Split out of
# warz_add_server so the two cannot drift: a test compiled with different defines than
# the binary it is testing is testing a different program.
function(warz_configure_server_target name ARG_DEFINES ARG_INCLUDES ARG_LIBS)
    # Each server project puts its OWN Sources/ on the include path (".\Sources" in the
    # vcxproj), so "ServerGameLogic.h" and friends resolve unqualified.
    target_include_directories(${name} PRIVATE ${ARG_INCLUDES})
    target_compile_definitions(${name} PRIVATE ${ARG_DEFINES})
    target_link_libraries(${name} PRIVATE
        GameEngine_server          # brings Eternity_server transitively
        warz_defines_server
        warz_compiler_flags
        ${ARG_LIBS}
    )

    # Each server gets its OWN precompiled header rather than sharing one. r3dPCH.h
    # branches on DISABLE_PHYSX, which Supervisor and Master define and GameServer does
    # not -- so their r3dPCH.h is a genuinely different header, one pulling in
    # PxForwardDecls.h and the other the whole PxPhysicsAPI.h. See cmake/Pch.cmake.
    warz_enable_pch(${name})
endfunction()

# ---------------------------------------------------------------------------
# SupervisorServer -- the smallest link in the product, and the first target.
# ---------------------------------------------------------------------------
warz_add_server(SupervisorServer
    SOURCES  ${WARZ_SUPERVISORSERVER_SOURCES} ${WARZ_SUPERVISORSERVER_SHARED_SOURCES}
    DEFINES  DISABLE_PHYSX
    INCLUDES ${WARZ_ROOT}/server/src/SupervisorServer/Sources
             ${WARZ_ROOT}/server/src/MasterServer/Sources
)

# ---------------------------------------------------------------------------
# MasterServer
# ---------------------------------------------------------------------------
warz_add_server(MasterServer
    SOURCES  ${WARZ_MASTERSERVER_SOURCES} ${WARZ_MASTERSERVER_SHARED_SOURCES}
    DEFINES  DISABLE_PHYSX
    INCLUDES ${WARZ_ROOT}/server/src/MasterServer/Sources
)

# ---------------------------------------------------------------------------
# GameServer -- the only server that needs PhysX and navigation.
#
# KY_BUILD_SHIPPING is a leftover of the Kynapse integration that the source still
# tests; Recast/Detour replaced Kynapse but the guard remains.
# ---------------------------------------------------------------------------
# Sources added by the PORT rather than inherited from the .vcxproj. They are listed
# here and not in cmake/sources/WO_GameServer.cmake because that file is GENERATED from
# the original project file by tools/gen_sources.py -- anything added to it is lost the
# next time it is regenerated.
set(WARZ_WO_GAMESERVER_PORT_SOURCES
    ${WARZ_ROOT}/server/src/WO_GameServer/Sources/ServerSelfTest.cpp
)

warz_add_server(GameServer
    SOURCES  ${WARZ_WO_GAMESERVER_SOURCES} ${WARZ_WO_GAMESERVER_SHARED_SOURCES}
             ${WARZ_WO_GAMESERVER_PORT_SOURCES}
    DEFINES  _CRT_SECURE_NO_WARNINGS KY_BUILD_SHIPPING
    INCLUDES ${WARZ_ROOT}/server/src/WO_GameServer/Sources
    LIBS     ${WARZ_RECAST_LIBS} ${WARZ_PHYSX_LIBS}
)

# ---------------------------------------------------------------------------
# The GameServer's sources, minus its entry point, exported for the test harness.
#
# WHY A VARIABLE AND NOT A STATIC LIBRARY
#
# The obvious shape is a GameServerLib that both GameServer.exe and the tests link.
# It is the wrong one here: this codebase registers its object classes from static
# initialisers (AUTOREGISTER_CLASS, AObject.h:22-49), and a linker pulls an object file
# out of an archive only when something already references a symbol in it. Nothing
# references obj_ServerZombie by name -- LevelData.xml names it as a STRING and the
# class table resolves it at load. So an archive link silently drops most of the game's
# classes and the world comes up empty, with no diagnostic anywhere.
#
# --whole-archive would solve that, and is what a library version of this would need.
# Compiling the sources into each target directly sidesteps the question entirely, and
# costs only build time -- which ccache already absorbs, since the two targets compile
# the same files with the same flags.
#
# ServerMain.cpp is excluded because it defines main(). The test harness brings its own
# entry point and stubs the handful of globals ServerMain.cpp uniquely owns; see
# tests/world/server_entry_stubs.cpp.
# ---------------------------------------------------------------------------
set(_gameserver_entry ${WARZ_ROOT}/server/src/WO_GameServer/Sources/ServerMain.cpp)

set(_gameserver_testable
    ${WARZ_WO_GAMESERVER_SOURCES}
    ${WARZ_WO_GAMESERVER_SHARED_SOURCES}
    ${WARZ_WO_GAMESERVER_PORT_SOURCES}
)
list(REMOVE_ITEM _gameserver_testable ${_gameserver_entry})

# REMOVE_ITEM matches by exact string, so a path that stopped being spelled the way
# gen_sources.py spells it would silently remove nothing -- and the failure would be a
# duplicate-symbol link error 100 files later. Fail here instead, where the cause is
# still visible.
list(LENGTH WARZ_WO_GAMESERVER_SOURCES _n_own)
list(LENGTH WARZ_WO_GAMESERVER_SHARED_SOURCES _n_shared)
list(LENGTH WARZ_WO_GAMESERVER_PORT_SOURCES _n_port)
list(LENGTH _gameserver_testable _n_testable)
math(EXPR _n_expected "${_n_own} + ${_n_shared} + ${_n_port} - 1")
if(NOT _n_testable EQUAL _n_expected)
    message(FATAL_ERROR
        "Failed to exclude ServerMain.cpp from the testable GameServer sources.\n"
        "Expected ${_n_expected} sources, got ${_n_testable}.\n"
        "The entry-point path below must match cmake/sources/WO_GameServer.cmake exactly:\n"
        "  ${_gameserver_entry}")
endif()

set(WARZ_GAMESERVER_TESTABLE_SOURCES ${_gameserver_testable} PARENT_SCOPE)
