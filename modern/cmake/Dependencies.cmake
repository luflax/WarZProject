# Dependency acquisition and the vendored-library targets.
#
# Everything here is MIT / BSD / zlib / Apache-2.0 / FTL. No commercial agreements, no
# copyleft. See ../../DEPENDENCIES.md for the full audit of what was replaced and why.
#
# Where a dependency was replaced by a NO-OP SHIM rather than a real library -- Chilkat,
# FMOD, TeamSpeak, Steam, GameBlocks, NVAPI, atimgpud, D3DX's non-math half -- there is
# nothing to build: every shim is header-only and fully inline, and contributes zero
# unresolved symbols at link time. Those live under src/External/ and are on the include
# path via warz_includes.

include(FetchContent)

# ---------------------------------------------------------------------------
# pugixml (MIT) -- level and config XML.
#
# Referenced by the source as "../../External/pugiXML/src/pugixml.hpp". Vendored in
# modern/src/External/pugiXML; if it is absent, fetch it.
# ---------------------------------------------------------------------------

set(WARZ_PUGIXML_LIB "" CACHE INTERNAL "")

if(EXISTS "${WARZ_ROOT}/src/External/pugiXML/src/pugixml.cpp")
    add_library(pugixml STATIC "${WARZ_ROOT}/src/External/pugiXML/src/pugixml.cpp")
    target_include_directories(pugixml PUBLIC "${WARZ_ROOT}/src/External/pugiXML/src")
    set_target_properties(pugixml PROPERTIES CXX_STANDARD 17)
    set(WARZ_PUGIXML_LIB pugixml CACHE INTERNAL "")
else()
    FetchContent_Declare(pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG        v1.14
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(pugixml)
    set(WARZ_PUGIXML_LIB pugixml CACHE INTERNAL "")
endif()

# ---------------------------------------------------------------------------
# Recast & Detour (zlib) -- replaces Autodesk Navigation (Kynapse), discontinued.
#
# Vendored with its own CMakeLists per component.
# ---------------------------------------------------------------------------

set(WARZ_RECAST_LIBS "" CACHE INTERNAL "")

if(EXISTS "${WARZ_ROOT}/src/External/Recast/Detour/CMakeLists.txt")
    # Only Recast/ and Detour/ were vendored, not the root CMakeLists that normally
    # declares these options. Their per-component files reference them inside generator
    # expressions -- "$<$<NOT:${RECASTNAVIGATION_ENABLE_ASSERTS}>:RC_DISABLE_ASSERTS>" --
    # which expands to a malformed "$<NOT:>" if the variable is undefined. Declare them
    # here so the expressions resolve.
    # The values must be the literals 0 and 1, not OFF/ON: $<NOT:...> rejects anything
    # else, so "$<NOT:OFF>" is as much an error as "$<NOT:>".
    set(RECASTNAVIGATION_ENABLE_ASSERTS         0)  # off in a Release-style build
    set(RECASTNAVIGATION_DT_POLYREF             0)  # 32-bit dtPolyRef
    set(RECASTNAVIGATION_DT_VIRTUAL_QUERYFILTER 1)  # obj_Zombie subclasses dtQueryFilter

    foreach(component Recast Detour DetourCrowd DetourTileCache DebugUtils)
        if(EXISTS "${WARZ_ROOT}/src/External/Recast/${component}/CMakeLists.txt")
            add_subdirectory("${WARZ_ROOT}/src/External/Recast/${component}"
                             "${CMAKE_BINARY_DIR}/External/Recast/${component}")
        endif()
    endforeach()
    set(WARZ_RECAST_LIBS Detour DetourCrowd Recast CACHE INTERNAL "")

    foreach(t ${WARZ_RECAST_LIBS} DetourTileCache DebugUtils)
        if(TARGET ${t})
            # RecastAlloc.h does "#include <stdint.h>" and then uses INTPTR_MAX, but
            # MinGW-w64's stdint.h hides the limit macros from C++ unless
            # __STDC_LIMIT_MACROS is set (the C99 rule that <cstdint> later dropped).
            # Recast's own build defines it; ours has to as well.
            target_compile_definitions(${t} PRIVATE __STDC_LIMIT_MACROS __STDC_CONSTANT_MACROS)
            if(NOT MSVC)
                target_compile_options(${t} PRIVATE -w)
            endif()
        endif()
    endforeach()
endif()

# ---------------------------------------------------------------------------
# RmlUi (MIT) -- replaces Scaleform GFx, which Autodesk discontinued in 2018 and
# which cannot be licensed at any price.
#
# RmlUi's default font engine needs FreeType (FTL -- BSD-style with attribution, so
# permissible). FreeType is NOT in the MinGW sysroot, so unless it has been built for
# the target, configure RmlUi without a font engine: the library still links and the UI
# still lays out, it simply renders no text. That is a Milestone C concern, not B.
# ---------------------------------------------------------------------------

set(WARZ_RMLUI_LIBS "" CACHE INTERNAL "")

if(EXISTS "${WARZ_ROOT}/src/External/RmlUi/Source/Core")
    include(BuildRmlUi)
endif()

# ---------------------------------------------------------------------------
# PhysX 4.1 (BSD-3) -- interim replacement for PhysX 3.x. Jolt (MIT) is the eventual
# target; see ../../PERFORMANCE-OPTIMIZATION-PLAN.md.
#
# Built from source by BuildPhysX.cmake rather than by NVIDIA's own harness, which
# cannot run here: it targets MSVC and depends on a packman-fetched CMakeModules package
# that is not in the repository. That file is where the MinGW-i686 configuration lives.
# ---------------------------------------------------------------------------

set(WARZ_PHYSX_LIBS "" CACHE INTERNAL "")

if(EXISTS "${WARZ_ROOT}/src/External/PhysX/physx-source")
    include(BuildPhysX)
else()
    message(STATUS "PhysX: headers only, no sources -- GameServer and WarZ.exe will not "
                   "link until these are vendored. SupervisorServer and MasterServer "
                   "build with DISABLE_PHYSX and are unaffected.")
endif()

# ---------------------------------------------------------------------------
# stb (public domain) -- replaces libjpeg 6b (1998), whose .lib is absent.
# ---------------------------------------------------------------------------

add_library(warz_stb INTERFACE)
if(EXISTS "${WARZ_ROOT}/src/External/stb")
    target_include_directories(warz_stb INTERFACE "${WARZ_ROOT}/src/External/stb")
endif()

# ---------------------------------------------------------------------------
# Deliberately NOT fetched
# ---------------------------------------------------------------------------
#
#   libcurl     replaces Chilkat for backend HTTPS. Shimmed for now; the shim fails
#               honestly rather than faking a response, so nothing silently "works".
#   miniaudio   replaces FMOD Ex. Shimmed; the game is silent.
#   Opus        replaces the TeamSpeak SDK for voice. Shimmed; there is no voice chat.
#   Jolt        replaces PhysX entirely. Performance phase, not the build phase.
#   EnTT/Tracy  ECS and profiler. Later phases.
#
# Each of these is a feature project, not a porting task. Pulling one into Milestone B
# trades a reachable milestone for an open-ended migration.
