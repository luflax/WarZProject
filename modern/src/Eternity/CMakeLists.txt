# Eternity -- the r3d engine core.
#
# Built TWICE, as Eternity_client and Eternity_server. See cmake/WarzConfig.cmake for
# why: WO_SERVER is not a small difference in this tree.
#
# The source list comes from Eternity.vcxproj via tools/gen_sources.py, not from a
# glob -- r3dObj_OLDRender.cpp is on disk but in no project, and building it is waste.
# It includes SF/ and UndoHistory/, which sit outside Source/ and were missed when the
# compile probe was pointed at Source/ alone.

include(${WARZ_ROOT}/cmake/sources/Eternity.cmake)

warz_add_dual_library(Eternity SOURCES ${WARZ_ETERNITY_SOURCES})

# The compat layer -- shim APIs that grew real implementations.
#
# These live under src/External/compat but are compiled INTO Eternity rather than into
# a library of their own, and that placement is deliberate. Eternity sits below
# everything that calls them (r3dTex.cpp and VShader.cpp for D3DX, r3dDebug.cpp for
# CrashRpt, EclipseStudio and all three servers for Chilkat), so it is the one target
# that can both reach the engine's log and be reached by every caller. A separate
# static library would have to either duplicate r3dOutToLog or form a cycle with
# Eternity, and GNU ld does not revisit an archive.
#
# They are not in cmake/sources/Eternity.cmake because that file is generated from
# Eternity.vcxproj by tools/gen_sources.py and would drop them on the next run.
set(WARZ_COMPAT_SOURCES
    ${WARZ_ROOT}/src/External/compat/CrashReport.cpp        # -> dbghelp minidumps
    ${WARZ_ROOT}/src/External/compat/ChilkatHttp.cpp        # -> WinHTTP + zlib + base64
    ${WARZ_ROOT}/src/External/compat/D3DXImage.cpp          # -> DDS loading
    ${WARZ_ROOT}/src/External/compat/D3DXShaderCompile.cpp  # -> d3dcompiler_NN.dll
)

foreach(variant client server)
    target_sources(Eternity_${variant} PRIVATE ${WARZ_COMPAT_SOURCES})
    target_include_directories(Eternity_${variant} PRIVATE
        ${WARZ_ROOT}/src/External/compat)
endforeach()

# Eternity's OWN dependencies, declared here rather than on each executable.
#
# This is not bookkeeping -- it is what makes the link work. GNU ld resolves static
# archives left to right and never revisits one, so a library must appear AFTER
# everything that references it. When the executables listed RakNet and pugixml
# themselves, CMake had no reason to know Eternity needed them too and emitted
# "libRakNet.a libpugixml.a ... libEternity_server.a", leaving every RakNet, pugixml
# and Direct3D symbol Eternity references undefined. Declaring them PUBLIC here lets
# CMake order the whole graph correctly on its own.
foreach(variant client server)
    target_link_libraries(Eternity_${variant} PUBLIC
        RakNet                      # r3dNetwork.cpp
        ${WARZ_PUGIXML_LIB}         # r3dIniFile / config parsing

        # Win32. d3d9/dinput8/dxguid are needed even by the server build: WO_SERVER
        # strips the drawing, not the renderer headers and their device plumbing.
        d3d9 dinput8 dxguid xinput
        winmm ws2_32 shlwapi crypt32 rpcrt4 version psapi
        dbghelp                     # StackWalker.cpp, MiniDumpWriteDump, compat/CrashReport.cpp
        pdh                         # HWInfo.cpp performance counters
        winhttp                     # compat/ChilkatHttp.cpp -- the backend transport
        iphlpapi dnsapi
        odbc32 odbccp32 wbemuuid
    )
endforeach()

# zlib ships inside this tree as C, at Source/ZLib/src/*.c, and Eternity.vcxproj
# compiles it as part of the library rather than linking a separate one. Eleven of the
# 102 sources above are those .c files, which is why this project enables C as well as
# C++ -- see the top-level project() call.
foreach(variant client server)
    target_include_directories(Eternity_${variant} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/ZLib/src
    )
endforeach()

# Those eleven .c files are also why this target needs care with precompiled headers: a
# C++ PCH forced into a C translation unit is a hard error, and Eternity is the only
# target in the tree that compiles both languages.
#
# That is handled in cmake/Pch.cmake, by guarding the header with
# $<$<COMPILE_LANGUAGE:CXX>:...> so it never reaches a C source. This block used to set
# SKIP_PRECOMPILE_HEADERS on ${WARZ_ETERNITY_SOURCES} -- ALL 98 of them, C and C++
# alike -- under a comment about zlib. It was inert while no PCH existed, and would have
# silently switched precompiled headers off for the whole engine the moment one did.
