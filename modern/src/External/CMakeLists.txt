# src/External — vendored libraries and dependency shims.
#
# This directory sits at the exact path the original source includes from, so most
# dependency substitution needs zero source edits. See README.md.

# --------------------------------------------------------------------------
# warz_shims — header-only interface target
#
# Every shim is headers-only, so this target just publishes include paths that
# resolve the absent SDKs. Shims that grow a .cpp get added to a real library here.
# --------------------------------------------------------------------------

add_library(warz_shims INTERFACE)

target_include_directories(warz_shims INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}                      # "External/<name>/..." relative forms
    ${CMAKE_CURRENT_SOURCE_DIR}/dxsdk/Include        # d3dx9.h  -> DirectXMath (FUNCTIONAL)
    ${CMAKE_CURRENT_SOURCE_DIR}/ChilKat/Include      # CkHttp.h etc. (no-op)
    ${CMAKE_CURRENT_SOURCE_DIR}/Scaleform3/Include   # GFx.h etc.    (no-op)
    ${CMAKE_CURRENT_SOURCE_DIR}/ts3_sdk_3/include    # ts3client_*   (no-op)
    ${CMAKE_CURRENT_SOURCE_DIR}/PhysX/include        # Px*           (no-op until PhysX 4.1)
    ${CMAKE_CURRENT_SOURCE_DIR}/CrashRpt/include     # CrashRpt.h    (no-op)
    ${CMAKE_CURRENT_SOURCE_DIR}/GameBlocks/include   # FairFight     (no-op)
)

# --------------------------------------------------------------------------
# RakNet — BSD-2. The one real dependency present in the original drop.
#
# Copied by tools/bootstrap.sh. Built from source rather than linked prebuilt,
# so it participates in the C++20 build.
# --------------------------------------------------------------------------

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/RakNet/Source")
    file(GLOB RAKNET_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/RakNet/Source/*.cpp"
    )

    add_library(RakNet STATIC ${RAKNET_SOURCES})

    target_include_directories(RakNet PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/RakNet/Source
    )

    # RakNet predates C++20 and is not ours to fix. Build it permissively and
    # keep the conformance work focused on the game code.
    set_target_properties(RakNet PROPERTIES CXX_STANDARD 17)

    if(MSVC)
        target_compile_options(RakNet PRIVATE /w)
        target_compile_definitions(RakNet PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(RakNet PRIVATE -w)
    endif()
else()
    message(WARNING
        "RakNet sources not found. Run tools/bootstrap.sh to copy them "
        "from the original tree.")
endif()

# --------------------------------------------------------------------------
# zlib — vendored in the original at src/Eternity/Source/ZLib/, so it is built
# as part of the Eternity target rather than here.
# --------------------------------------------------------------------------
