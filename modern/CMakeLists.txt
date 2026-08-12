#
# Infestation: Survivor Stories — C++20 port
#
# Replaces the original Visual Studio solutions (Eternity, WarZ, WarZ_Server) with one
# CMake tree. Milestone B's goal is a clean COMPILE and LINK, not a running game --
# see ../MILESTONE-B-PLAN.md.
#
# Cross-compile with:
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-i686.cmake
#   cmake --build build -j
#
cmake_minimum_required(VERSION 3.25)

project(InfestationModern
    VERSION 0.2.0
    DESCRIPTION "Infestation: Survivor Stories — C++20 port"
    # C as well as C++: zlib ships inside src/Eternity/Source/ZLib as eleven .c files,
    # and Eternity.vcxproj compiles them into the engine rather than linking a
    # separate library.
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Release by default. The original's shipping configuration is 'Final', which strips the
# editors -- that is a separate configuration this port has not verified yet, so it is
# reachable but not the default.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(WARZ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${WARZ_ROOT}/cmake")

# compile_commands.json: what clangd, clang-tidy and include-what-you-use all read, and
# what tools/header_cost.py uses to price a header. Free to produce, so always on.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --------------------------------------------------------------------------
# Options
# --------------------------------------------------------------------------

option(WARZ_BUILD_CLIENT       "Build the game client"                     ON)
option(WARZ_BUILD_SERVER       "Build the three server binaries"           ON)
option(WARZ_BUILD_TESTS        "Build the test suites"                     ON)
option(WARZ_WARNINGS_AS_ERRORS "Treat warnings as errors (not yet)"        OFF)

# --------------------------------------------------------------------------
# Guards
# --------------------------------------------------------------------------

if(NOT EXISTS "${WARZ_ROOT}/src/Eternity/Include/r3d.h")
    message(FATAL_ERROR
        "Source tree not bootstrapped.\n"
        "Run:  ./tools/bootstrap.sh\n"
        "This copies source from the original tree without modifying it.")
endif()

if(NOT EXISTS "${WARZ_ROOT}/cmake/sources/Eternity.cmake")
    message(FATAL_ERROR
        "Source lists missing.\n"
        "Run:  ./tools/gen_sources.py\n"
        "They are derived from the original .vcxproj files rather than globbed, "
        "because sixteen files on disk are orphans the original never compiled.")
endif()

# --------------------------------------------------------------------------
# Configuration
# --------------------------------------------------------------------------

include(Speed)             # ccache, when it is installed
include(CompilerFlags)     # warz_compiler_flags, warz_final_config
include(Pch)               # warz_enable_pch -- must precede any target that calls it
include(WarzConfig)        # warz_includes, warz_defines_*, warz_add_dual_library
include(Dependencies)      # vendored libraries -> WARZ_*_LIBS

# --------------------------------------------------------------------------
# Targets -- mirrors the original layering:
#
#     Eternity -> GameEngine -> EclipseStudio / server
#
# Eternity and GameEngine are each built TWICE, _client and _server, because the client
# links them without WO_SERVER and the servers with it, and the two take substantially
# different #ifdef branches.
# --------------------------------------------------------------------------

add_subdirectory(src/External)      # RakNet + the header-only shims
add_subdirectory(src/Eternity)      # Eternity_client   / Eternity_server
add_subdirectory(src/GameEngine)    # GameEngine_client / GameEngine_server

if(WARZ_BUILD_CLIENT)
    add_subdirectory(src/EclipseStudio)   # WarZ.exe
endif()

if(WARZ_BUILD_SERVER)
    add_subdirectory(server/src)          # SupervisorServer / MasterServer / GameServer
endif()

# Tests. Two tiers here -- a compile-only one that needs no runtime at all, and a
# PE32 one that runs under Wine when it is available. A third, host-native tier is
# configured separately (`cmake -B build-host -S tests`) because it needs the host
# compiler rather than the cross-toolchain. tests/CMakeLists.txt explains the split.
if(WARZ_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --------------------------------------------------------------------------
# Summary
# --------------------------------------------------------------------------

message(STATUS "")
message(STATUS "Infestation (C++20 port) ${PROJECT_VERSION}")
message(STATUS "  C++ standard : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler     : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Generator    : ${CMAKE_GENERATOR}")
message(STATUS "  Build type   : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Client       : ${WARZ_BUILD_CLIENT}")
message(STATUS "  Server       : ${WARZ_BUILD_SERVER}")
message(STATUS "  ccache       : ${WARZ_CCACHE_STATUS}")
message(STATUS "  PCH          : ${WARZ_PCH_STATUS}")
message(STATUS "  PhysX libs   : ${WARZ_PHYSX_LIBS}")
message(STATUS "  RmlUi libs   : ${WARZ_RMLUI_LIBS}")
message(STATUS "  Recast libs  : ${WARZ_RECAST_LIBS}")
message(STATUS "")
