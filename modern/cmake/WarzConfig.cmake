# Shared include paths and preprocessor configuration.
#
# This is the single source of truth for how a translation unit is compiled, and it is
# kept deliberately in step with tools/probe.sh -- the probe is what verifies the port
# compiles, so if the two drift apart the verification stops meaning anything.
#
# The layout mirrors the .vcxproj files' AdditionalIncludeDirectories and
# PreprocessorDefinitions. Where it does not, there is a comment saying why.

# ---------------------------------------------------------------------------
# warz_includes -- the include path every target shares
# ---------------------------------------------------------------------------

add_library(warz_includes INTERFACE)

target_include_directories(warz_includes INTERFACE
    ${WARZ_ROOT}/src/Eternity/Include
    ${WARZ_ROOT}/src/Eternity
    ${WARZ_ROOT}/src/GameEngine
    ${WARZ_ROOT}/src/EclipseStudio/Sources
    ${WARZ_ROOT}/src/ServerNetPackets

    # Third-party and shims. src/External itself is on the path because much of the
    # source includes through it by directory -- "NVApi/nvapi.h", "PhysX/include/...".
    ${WARZ_ROOT}/src/External
    ${WARZ_ROOT}/src/External/dxsdk/Include          # d3dx9.h    -> clean-room math
    ${WARZ_ROOT}/src/External/Scaleform3/Include     # GFx.h      -> no-op shim
    ${WARZ_ROOT}/src/External/ChilKat/Include        # CkHttp.h   -> no-op shim
    ${WARZ_ROOT}/src/External/Steam                  # steam_api  -> no-op shim
    ${WARZ_ROOT}/src/External/ts3_sdk_3/include      # TeamSpeak  -> no-op shim
    ${WARZ_ROOT}/src/External/GameBlocks             # FairFight  -> no-op shim
    ${WARZ_ROOT}/src/External/RakNet/Source
    ${WARZ_ROOT}/src/External/PhysX/physx-include
    ${WARZ_ROOT}/src/External/PhysX/pxshared-include
    ${WARZ_ROOT}/src/External/PhysX/compat           # Px3xCompat.h -- 3.x -> 4.1 bridge
    ${WARZ_ROOT}/src/External/Recast/Detour/Include
    ${WARZ_ROOT}/src/External/Recast/DetourCrowd/Include
    ${WARZ_ROOT}/src/External/Recast/Recast/Include
    ${WARZ_ROOT}/src/External/RmlUi/Include
)

# ---------------------------------------------------------------------------
# Preprocessor configuration
#
# warz_defines_common  -- everything
# warz_defines_client  -- the game client; NO WO_SERVER
# warz_defines_server  -- the three server binaries
#
# The client/server split is not cosmetic. WO_SERVER strips all rendering, turns off
# USE_R3D_MEMORY_ALLOCATOR and ENABLE_WEB_BROWSER, and removes the editor UI --
# UIimEdit.cpp defines eight imgui_DrawList symbols as client code and none as server
# code. Eternity and GameEngine are therefore built TWICE, once against each.
# ---------------------------------------------------------------------------

add_library(warz_defines_common INTERFACE)
target_compile_definitions(warz_defines_common INTERFACE
    WIN32
    _WINDOWS
    PX_PHYSX_STATIC_LIB     # no dllimport decoration; PhysX is linked statically
    # NDEBUG is not set here: CMake already puts -DNDEBUG in CMAKE_<LANG>_FLAGS_RELEASE,
    # RELWITHDEBINFO and MINSIZEREL, so adding it would only duplicate that -- and doing
    # it with $<NOT:$<CONFIG:Debug>> breaks outright when no build type is set.

    # Berkelium is abandoned and absent. r3dPCH.h only defaults this to 1 when it is
    # not already defined, and every call site is behind #if ENABLE_WEB_BROWSER.
    ENABLE_WEB_BROWSER=0
)
target_link_libraries(warz_defines_common INTERFACE warz_includes)

add_library(warz_defines_client INTERFACE)
target_link_libraries(warz_defines_client INTERFACE warz_defines_common)

add_library(warz_defines_server INTERFACE)
target_compile_definitions(warz_defines_server INTERFACE WO_SERVER)
target_link_libraries(warz_defines_server INTERFACE warz_defines_common)

# ---------------------------------------------------------------------------
# warz_add_dual_library -- build one source list twice, client and server
#
# Produces <name>_client and <name>_server static libraries. SOURCES is compiled into
# both; CLIENT_ONLY_SOURCES is added to the client variant alone, for files the server
# projects genuinely do not list -- obj_Vehicle.cpp and VehicleManager.cpp include the
# client weapon headers, which #error out under WO_SERVER.
#
# Used for Eternity and GameEngine, which sit below the client/server split.
# ---------------------------------------------------------------------------

function(warz_add_dual_library name)
    cmake_parse_arguments(ARG "" "" "SOURCES;CLIENT_ONLY_SOURCES" ${ARGN})

    foreach(variant client server)
        set(target ${name}_${variant})
        set(sources ${ARG_SOURCES})
        if("${variant}" STREQUAL "client")
            list(APPEND sources ${ARG_CLIENT_ONLY_SOURCES})
        endif()
        add_library(${target} STATIC ${sources})
        target_link_libraries(${target} PUBLIC
            warz_defines_${variant}
            warz_compiler_flags
        )
        set_target_properties(${target} PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
        )
    endforeach()
endfunction()
