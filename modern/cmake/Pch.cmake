# Precompiled headers.
#
# This is not a new idea for this codebase -- it is a restoration. The original Visual
# Studio projects all set <PrecompiledHeader>Use</PrecompiledHeader> against
# <PrecompiledHeaderFile>r3dPCH.h</PrecompiledHeaderFile>, and 417 of the 419 sources
# this port builds still open with #include "r3dPCH.h". Only the wiring was lost when the
# .vcxproj files were replaced with CMake.
#
# Why it matters here more than in most projects, measured on this tree with GCC 13
# targeting i686-w64-mingw32:
#
#   parsing r3dPCH.h, alone, nothing else       4.98 s
#   compiling a whole average TU                5.25 s
#
# The header is ~90% of the front-end cost of every translation unit, it is identical
# across all ~690 game TUs, and without this file it is paid ~690 times. An average TU
# pulls in 888 headers; 698 of them arrive through this one.
#
# Measured effect, and the objects were compared rather than assumed:
#
#   PFX_AnaglyphComposite.cpp   4.57 s -> 1.59 s     object byte-identical
#   Game.cpp                    8.06 s -> 3.37 s     object byte-identical
#   RenderDeffered.cpp         15.53 s -> 12.16 s    object byte-identical
#
# The saving is a flat ~3-4.7 s per TU -- the fixed header cost -- so it helps small
# files most. RenderDeffered.cpp benefits least because 12 of its 15 seconds are -O3
# codegen over 10k lines, which no amount of header caching touches.
#
# Byte-identical output is the point: this is a pure caching change with no effect on
# what is compiled. tools/buildtime.sh --verify checks that claim across all 1303
# objects rather than trusting the three above.

option(WARZ_PCH "Precompile r3dPCH.h for the engine and game targets" ON)

set(WARZ_PCH_HEADER "${WARZ_ROOT}/src/Eternity/Include/r3dPCH.h")

# The seven built C++ sources that do NOT open with #include "r3dPCH.h".
#
# CMake implements PCH by force-including the header with -include, which would push
# r3dPCH.h into these files whether they want it or not. They are excluded instead: the
# original build excluded them too, and r3dPCH.h is not an inert header -- it defines
# NOMINMAX and STRICT, selects a Windows version, and in debug configurations redefines
# `new`. Forcing that into files written to stand alone is risk with nothing to gain,
# since they were never paying the cost this file exists to remove.
#
# Derived by grepping the built source lists; tools/buildtime.sh --verify re-checks it,
# because a file that quietly grows an #include "r3dPCH.h" later should stop being an
# exception.
set(WARZ_PCH_EXCLUDED_SOURCES
    ${WARZ_ROOT}/server/src/WO_GameServer/Sources/PunkBuster/pbcl.cpp
    ${WARZ_ROOT}/server/src/WO_GameServer/Sources/PunkBuster/pbmd5.cpp
    ${WARZ_ROOT}/server/src/WO_GameServer/Sources/PunkBuster/pbsdk.cpp
    ${WARZ_ROOT}/server/src/WO_GameServer/Sources/PunkBuster/pbsv.cpp
    ${WARZ_ROOT}/src/Eternity/Source/AtlasComposer/RectPlacement.cpp
    ${WARZ_ROOT}/src/Eternity/Source/r3dVCacheAnalyze.cpp
    ${WARZ_ROOT}/src/Eternity/Source/r3dVCacheOptimize.cpp
)

if(WARZ_PCH)
    set(WARZ_PCH_STATUS "r3dPCH.h")
else()
    set(WARZ_PCH_STATUS "disabled (-DWARZ_PCH=OFF)")
endif()

# ---------------------------------------------------------------------------
# warz_enable_pch(<target>)
#
# One PCH per target, and that is not laziness -- r3dPCH.h reads WO_SERVER,
# DISABLE_PHYSX, FINAL_BUILD and ENABLE_WEB_BROWSER, so the client and server variants
# genuinely see different headers and cannot share a .gch. Eight targets, eight PCHs,
# about 80 s and 1.6 GB to build them.
#
# REUSE_FROM between SupervisorServer and MasterServer (both WO_SERVER + DISABLE_PHYSX)
# would save one of those, but their define sets are not currently identical and GCC's
# response to a mismatched PCH is to ignore it silently. Not worth guessing at for 200 MB.
# ---------------------------------------------------------------------------

function(warz_enable_pch target)
    if(NOT WARZ_PCH)
        return()
    endif()

    # The COMPILE_LANGUAGE guard is what keeps this off Eternity's eleven zlib .c files:
    # a C++ PCH forced into a C translation unit is a hard error, and Eternity is the one
    # target in the tree that compiles both languages.
    target_precompile_headers(${target} PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:${WARZ_PCH_HEADER}>")

    # TARGET_DIRECTORY rather than a bare call so this works from any scope -- the
    # servers are defined in server/src, Eternity and GameEngine come through
    # warz_add_dual_library in cmake/, and source properties are per-directory.
    set_source_files_properties(${WARZ_PCH_EXCLUDED_SOURCES}
        TARGET_DIRECTORY ${target}
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endfunction()
