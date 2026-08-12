#
# Infestation: Survivor Stories — C++20 port
#
# Replaces six Visual Studio solutions (Eternity, WarZ, WarZ_Server, RSBuild,
# RSUpdate, backend) with a single CMake tree.
#
# Phase 1 goal is a clean COMPILE and LINK, not a running game. See
# ../PHASE1-BUILD-PLAN.md for scope and explicit non-goals.
#
cmake_minimum_required(VERSION 3.25)

project(InfestationModern
    VERSION 0.1.0
    DESCRIPTION "Infestation: Survivor Stories — C++20 port"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# --------------------------------------------------------------------------
# Options
# --------------------------------------------------------------------------

option(WARZ_BUILD_CLIENT  "Build the game client"          ON)
option(WARZ_BUILD_SERVER  "Build the three server binaries" ON)
option(WARZ_USE_SHIMS     "Use no-op shims for absent commercial dependencies" ON)
option(WARZ_WARNINGS_AS_ERRORS "Treat warnings as errors (NOT for phase 1)" OFF)

# --------------------------------------------------------------------------
# Guard: the tree must be bootstrapped first
# --------------------------------------------------------------------------

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/Eternity/Include/r3d.h")
    message(FATAL_ERROR
        "Source tree not bootstrapped.\n"
        "Run:  ./modern/tools/bootstrap.sh\n"
        "This copies source from the original tree without modifying it.")
endif()

# --------------------------------------------------------------------------
# Compiler configuration
# --------------------------------------------------------------------------

include(CompilerFlags)

# --------------------------------------------------------------------------
# Feature flags
#
# These switches already exist in the original source (r3dPCH.h). Phase 1 sets
# them to compile out subsystems whose SDKs are absent and which are not needed
# for a build milestone. See PHASE1-BUILD-PLAN.md section 3.
# --------------------------------------------------------------------------

add_library(warz_config INTERFACE)

target_compile_definitions(warz_config INTERFACE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    _SCL_SECURE_NO_WARNINGS

    # Subsystems disabled for the build milestone
    ENABLE_AUTODESK_NAVIGATION=0    # Kynapse — discontinued, no SDK
    ENABLE_WEB_BROWSER=0            # Berkelium — abandoned
    APEX_ENABLED=0                  # already 0 upstream

    # __WITH_PB__      intentionally undefined — PunkBuster
    # USE_VMPROTECT    intentionally undefined — VMProtect
    # ENABLE_GAMEBLOCKS see note in ServerGameLogic.h:9; made conditional in phase 1
)

# --------------------------------------------------------------------------
# Dependencies
# --------------------------------------------------------------------------

include(Dependencies)

# --------------------------------------------------------------------------
# Targets
#
# Mirrors the original layering:  Eternity -> GameEngine -> EclipseStudio / server
# --------------------------------------------------------------------------

add_subdirectory(src/External)      # real vendored libs + shims
add_subdirectory(src/Eternity)      # r3dLib      (static)
add_subdirectory(src/GameEngine)    # GameEngine  (static)

if(WARZ_BUILD_CLIENT)
    add_subdirectory(src/EclipseStudio)   # WarZ.exe
endif()

if(WARZ_BUILD_SERVER)
    add_subdirectory(server/src)          # GameServer / MasterServer / SupervisorServer
endif()

# --------------------------------------------------------------------------
# Summary
# --------------------------------------------------------------------------

message(STATUS "")
message(STATUS "Infestation (C++20 port) ${PROJECT_VERSION}")
message(STATUS "  C++ standard   : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler       : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Client         : ${WARZ_BUILD_CLIENT}")
message(STATUS "  Server         : ${WARZ_BUILD_SERVER}")
message(STATUS "  Dependency shims: ${WARZ_USE_SHIMS}")
message(STATUS "")
