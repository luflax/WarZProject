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
    ${CMAKE_CURRENT_SOURCE_DIR}/ChilKat/Include      # CkHttp.h etc. -> WinHTTP (FUNCTIONAL)
    ${CMAKE_CURRENT_SOURCE_DIR}/Scaleform3/Include   # GFx.h etc.    (no-op)
    ${CMAKE_CURRENT_SOURCE_DIR}/ts3_sdk_3/include    # ts3client_*   (no-op)
    ${CMAKE_CURRENT_SOURCE_DIR}/PhysX/include        # Px*           (superseded: PhysX 4.1 is built from source)
    ${CMAKE_CURRENT_SOURCE_DIR}/CrashRpt/include     # CrashRpt.h    -> dbghelp (FUNCTIONAL)
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
        # -fpermissive is what "build it permissively" above actually requires, and it
        # is deliberately scoped to THIS target. RakNet contains constructs no standard
        # C++ accepts -- ReplicaManager3.cpp:122 compares a pointer to false -- and
        # fixing an unmaintained third-party library is not this port's job.
        #
        # The game code is still compiled with NO -fpermissive. That distinction is the
        # whole point: the flag masked roughly seventy real conformance errors in our
        # own sources when it was applied globally.
        target_compile_options(RakNet PRIVATE -w -fpermissive)
    endif()

    if(WIN32)
        # DR_SHA1.h tests _MSC_VER to decide whether <tchar.h> exists, and when it does
        # not, defines its own "#define TCHAR char". Under MinGW both premises are
        # wrong: _MSC_VER is undefined but tchar.h is present and the Windows headers
        # define TCHAR themselves -- so winnt.h's "typedef char TCHAR, *PTCHAR;"
        # expands to "typedef char char, *PTCHAR;" and fails.
        #
        # SHA1_HAS_TCHAR is RakNet's own escape hatch for exactly this, so no source
        # change is needed.
        target_compile_definitions(RakNet PRIVATE SHA1_HAS_TCHAR)
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
