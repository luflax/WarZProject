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
        dbghelp                     # StackWalker.cpp, MiniDumpWriteDump
        pdh                         # HWInfo.cpp performance counters
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
    # zlib is not ours to modernise; keep the port's conformance signal focused on the
    # game code.
    set_source_files_properties(
        ${WARZ_ETERNITY_SOURCES}
        DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON
    )
endforeach()
