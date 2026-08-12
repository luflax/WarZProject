# Compiler caching.
#
# ccache is OPTIONAL. When it is not installed this file is a no-op and the build
# behaves exactly as it did before -- nothing here is a dependency, and tools/build.sh
# must keep working on a machine that has never heard of ccache.
#
# What it buys: a clean build is ~19 minutes before any of this work and 8m23s with
# precompiled headers, and that price is paid not only for the first build but for every
# branch switch and every compile-flag change. With a warm cache the same clean build
# measures 13 seconds at 1311/1311 direct hits, because the compiler is never invoked at
# all -- only the 20 archives and 4 executables are actually produced.
#
# The configuration below is passed through `cmake -E env` rather than written to the
# user's ~/.config/ccache/ccache.conf. That keeps it with the project instead of in
# global state on the machine, and means two checkouts configured differently do not
# fight over one config file.

option(WARZ_CCACHE "Use ccache when it is available" ON)

set(WARZ_CCACHE_DIR "" CACHE STRING
    "ccache directory. Empty uses ccache's own default (~/.cache/ccache), which is \
already outside the build tree and therefore survives 'rm -rf build'.")

set(WARZ_CCACHE_MAXSIZE "10G" CACHE STRING
    "ccache size limit. 1303 objects per configuration, and this port has several.")

# Reported by the summary block at the end of the top-level CMakeLists.txt rather than
# printed here, so the configure output stays in one readable block.
set(WARZ_CCACHE_STATUS "disabled (-DWARZ_CCACHE=OFF)")

if(NOT WARZ_CCACHE)
    return()
endif()

find_program(WARZ_CCACHE_PROGRAM ccache)

set(WARZ_CCACHE_STATUS "NOT FOUND -- builds will not be cached")

if(NOT WARZ_CCACHE_PROGRAM)
    return()
endif()

set(_warz_ccache_env
    # REQUIRED for precompiled headers to cache at all -- without pch_defines, ccache
    # refuses to touch any compilation that uses a .gch and every PCH build is a miss.
    #
    # time_macros is the other half: three files in this tree expand __DATE__/__TIME__
    # (src/Eternity/SF/Version.h and the two VersionNo.cpp), and without this they can
    # never hit. The cost is real and worth stating plainly -- a cached object may carry
    # the timestamp of when it was FIRST compiled, not of this build. That is also why
    # those three translation units are the documented exception to the byte-identity
    # check in tools/buildtime.sh.
    "CCACHE_SLOPPINESS=pch_defines,time_macros"

    "CCACHE_MAXSIZE=${WARZ_CCACHE_MAXSIZE}"

    # NOT set: CCACHE_BASEDIR.
    #
    # It is the obvious thing to want -- it rewrites absolute paths under a given root to
    # relative ones before hashing, so a second clone in a different directory reuses the
    # cache instead of recompiling 1303 objects. It was set here at first.
    #
    # The catch is that the rewrite reaches the compiler, not just the hash. __FILE__ then
    # expands to "../src/Eternity/Source/r3dLight.cpp" instead of the absolute path, which
    # this codebase bakes into every r3d_assert and a great many log calls. Measured: it
    # changed 621 of 1303 object files and took 27 KB off WarZ.exe.
    #
    # Correct output either way, but it means installing a *cache* changes the binary, and
    # two people debugging the same crash would read different paths out of the same
    # assert depending on whether they happen to have ccache. A cache that alters its
    # program's output is not a cache. The cross-directory hits are not worth that, and
    # dropping it is also what lets tools/buildtime.sh --verify mean anything: with this
    # unset, ccache on and ccache off produce byte-identical trees.
)

if(WARZ_CCACHE_DIR)
    list(APPEND _warz_ccache_env "CCACHE_DIR=${WARZ_CCACHE_DIR}")
endif()

set(_warz_launcher "${CMAKE_COMMAND}" -E env ${_warz_ccache_env} "${WARZ_CCACHE_PROGRAM}")

# Applies to every target in the tree, including the vendored PhysX/RmlUi/Recast builds
# -- 588 of the 1303 objects, and the ones with the least reason ever to be recompiled.
set(CMAKE_C_COMPILER_LAUNCHER   ${_warz_launcher})
set(CMAKE_CXX_COMPILER_LAUNCHER ${_warz_launcher})

set(WARZ_CCACHE_STATUS "${WARZ_CCACHE_PROGRAM}")
