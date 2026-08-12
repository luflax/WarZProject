# MinGW-w64 i686 cross-toolchain.
#
# i686, not x86_64: the original product is a 32-bit Win32 build, and matching it keeps
# the type sizes, calling conventions and inline-assembly assumptions honest. Several
# places in the codebase assume sizeof(void*) == 4.
#
# Usage:
#   cmake --preset default          # preferred -- see CMakePresets.json
#   cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-i686.cmake

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

set(TOOLCHAIN_PREFIX i686-w64-mingw32)

set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER  ${TOOLCHAIN_PREFIX}-windres)
set(CMAKE_AR           ${TOOLCHAIN_PREFIX}-ar)
set(CMAKE_RANLIB       ${TOOLCHAIN_PREFIX}-ranlib)

set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})

# Look for programs on the host, but headers and libraries only in the target sysroot.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Run PE32 output through Wine. This is what lets `ctest` execute the cross-compiled
# test binaries without every test command having to spell out the emulator.
#
# Wine is NOT a build requirement: it is looked up rather than assumed, and when it is
# absent the variable stays empty, the cross-compiled suite simply cannot run, and the
# compile-only test tier (tests/layout) still reports. See tests/CMakeLists.txt for how
# the tiers are split, and MILESTONE-C-PREWORK.md §1.2 for why `wine32:i386` is
# frequently unresolvable in the containers this port is developed in.
find_program(WARZ_WINE wine)
if(WARZ_WINE)
    set(CMAKE_CROSSCOMPILING_EMULATOR ${WARZ_WINE})
endif()
