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
warz_add_server(GameServer
    SOURCES  ${WARZ_WO_GAMESERVER_SOURCES} ${WARZ_WO_GAMESERVER_SHARED_SOURCES}
    DEFINES  _CRT_SECURE_NO_WARNINGS KY_BUILD_SHIPPING
    INCLUDES ${WARZ_ROOT}/server/src/WO_GameServer/Sources
    LIBS     ${WARZ_RECAST_LIBS} ${WARZ_PHYSX_LIBS}
)
