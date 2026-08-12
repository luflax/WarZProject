# Tests.
#
# THE CONSTRAINT THAT SHAPES THIS FILE
#
# The product cross-compiles to PE32 and needs Wine to run. Wine is frequently
# unavailable -- `wine32:i386` pulls a dependency chain that does not resolve in many
# containers -- so a test suite that can only run under Wine gives no signal at all in
# the environment the port is actually developed in.
#
# So the tests are split by what they need in order to give a verdict:
#
#   warz_layout_checks  COMPILE-ONLY. All assertions are static_assert, so building the
#                       target IS the test. Needs the cross-compiler, needs no runtime
#                       whatsoever. This tier reports today.
#
#   warz_tests_host     Host-native ELF, built by configuring THIS DIRECTORY standalone
#                       with the host compiler. Carries the portable tests only -- the
#                       ones whose code under test has no Win32 dependency. Runs today.
#
#   warz_tests          The full suite, PE32, same toolchain as the shipping binaries.
#                       Registered with CTest and run through CMAKE_CROSSCOMPILING_EMULATOR
#                       (see cmake/toolchain-mingw-i686.cmake), so it runs the moment
#                       Wine is present and is skipped cleanly when it is not.
#
# The portable tests are compiled into BOTH warz_tests_host and warz_tests. That overlap
# is the point: the host build is what runs them, and the cross build is what proves the
# same source still compiles against the real Windows headers.

cmake_minimum_required(VERSION 3.25)

# ---------------------------------------------------------------------------
# Standalone mode -- the host build
#
# `cmake -B build-host -S tests` lands here with no parent project. Everything the
# portable tests need is in this directory plus src/External/dxsdk, so there is no
# dependency on the main tree's configuration having succeeded.
# ---------------------------------------------------------------------------

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(WarzTestsHost LANGUAGES CXX)
    set(WARZ_TESTS_STANDALONE ON)
    set(WARZ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
    enable_testing()
else()
    set(WARZ_TESTS_STANDALONE OFF)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Source groups
# ---------------------------------------------------------------------------

# The harness itself.
set(WARZ_TEST_FRAMEWORK
    framework/warz_test.cpp
    framework/test_framework.cpp
)

# Tests whose code under test has no Win32 dependency, so they build for the host as
# well as the target. Keep this list honest: a test that needs windows.h belongs below.
set(WARZ_TEST_PORTABLE
    math/test_d3dx_math.cpp
)

# static_assert-only translation units. These are compiled into warz_layout_checks,
# where building them is the whole test, and NOT into the running suites -- there would
# be nothing for the runner to execute.
#
# test_d3dx_layout.cpp is in BOTH portable and layout terms: it is compiled host-native
# against tests/host/d3d9.h and cross against the real one, asserting the same numbers.
# That is what stops the host shim from drifting into a comfortable fiction.
set(WARZ_TEST_LAYOUT_PORTABLE
    layout/test_d3dx_layout.cpp
)

# ---------------------------------------------------------------------------
# Host build
# ---------------------------------------------------------------------------

if(WARZ_TESTS_STANDALONE)
    if(CMAKE_CROSSCOMPILING)
        message(FATAL_ERROR
            "tests/ configured standalone but with a cross-toolchain.\n"
            "Standalone mode is the HOST build -- configure it without a toolchain file:\n"
            "    cmake -B build-host -S tests && cmake --build build-host\n"
            "The cross-compiled suite is built from the main tree instead.")
    endif()

    add_executable(warz_tests_host
        ${WARZ_TEST_FRAMEWORK}
        ${WARZ_TEST_PORTABLE}
        ${WARZ_TEST_LAYOUT_PORTABLE}
    )
    target_include_directories(warz_tests_host PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/framework
        ${CMAKE_CURRENT_SOURCE_DIR}/host          # d3d9.h / unknwn.h stand-ins
        ${WARZ_ROOT}/src/External/dxsdk/Include   # the compat layer under test
    )
    # Marks the host build inside the sources, so a test can assert on the difference
    # rather than silently skipping.
    target_compile_definitions(warz_tests_host PRIVATE WARZ_TEST_HOST_BUILD=1)

    add_test(NAME warz_tests_host COMMAND warz_tests_host)

    message(STATUS "")
    message(STATUS "Warz tests -- HOST build")
    message(STATUS "  Compiler : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    message(STATUS "  Runs     : natively, no Wine needed")
    message(STATUS "  Covers   : portable tests only (D3DX math, harness self-test)")
    message(STATUS "")

    return()
endif()

# ---------------------------------------------------------------------------
# Cross build -- reached via add_subdirectory() from the main tree
# ---------------------------------------------------------------------------

# Compile-only tier. An OBJECT library rather than an executable: there is no main() to
# link and nothing to run, and asking CMake to produce an executable from a translation
# unit of pure static_assert only invites a linker error that says nothing.
#
# `cmake --build build --target warz_layout_checks` is a complete test run for this tier.
add_library(warz_layout_checks OBJECT
    ${WARZ_TEST_LAYOUT_PORTABLE}
    layout/test_packet_layout.cpp
)
target_include_directories(warz_layout_checks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/framework)
target_link_libraries(warz_layout_checks PRIVATE
    warz_defines_server      # WO_SERVER: the packet structs are shared, and the server
                             # build is the one whose layout has to match the client's
    warz_compiler_flags
)

# CTest cannot express "building this target is the test", so drive the build from a
# test command. Failure to compile is failure to pass, which is exactly right.
add_test(NAME layout_checks
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target warz_layout_checks
)
set_tests_properties(layout_checks PROPERTIES LABELS "compile-only")

# The running suite. CMAKE_CROSSCOMPILING_EMULATOR carries `wine` in from the toolchain
# file, so add_test() below produces `wine warz_tests` automatically.
add_executable(warz_tests
    ${WARZ_TEST_FRAMEWORK}
    ${WARZ_TEST_PORTABLE}
)
target_include_directories(warz_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/framework
)
target_link_libraries(warz_tests PRIVATE
    warz_defines_server
    warz_compiler_flags
)

# ---------------------------------------------------------------------------
# Can the emulator actually run what we just built?
#
# `wine` being installed does not mean 32-bit PE32 works: on a 64-bit host that needs
# wine32:i386, whose dependency chain frequently does not resolve, and Wine then fails
# at LOAD time with "failed to load ntdll.dll" rather than refusing up front. Finding
# that out once, here, is better than every test reporting a failure that is really an
# environment gap -- which is what MILESTONE-C-PREWORK.md §1.2 asks to have confirmed.
#
# try_run routes through CMAKE_CROSSCOMPILING_EMULATOR, so this is a genuine end-to-end
# check of compile + link + execute for the real target.
# ---------------------------------------------------------------------------

if(NOT DEFINED WARZ_EMULATOR_WORKS)
    if(CMAKE_CROSSCOMPILING_EMULATOR)
        set(_probe "${CMAKE_CURRENT_BINARY_DIR}/emulator_probe.cpp")
        file(WRITE "${_probe}" "int main() { return 42; }\n")

        try_run(_probe_run _probe_compile
            "${CMAKE_CURRENT_BINARY_DIR}/emulator_probe"
            "${_probe}")

        if(_probe_compile AND _probe_run STREQUAL "42")
            set(WARZ_EMULATOR_WORKS ON  CACHE INTERNAL "emulator can run target binaries")
        else()
            set(WARZ_EMULATOR_WORKS OFF CACHE INTERNAL "emulator cannot run target binaries")
        endif()
    else()
        set(WARZ_EMULATOR_WORKS OFF CACHE INTERNAL "no emulator configured")
    endif()
endif()

add_test(NAME warz_tests COMMAND warz_tests)

if(NOT WARZ_EMULATOR_WORKS)
    # DISABLED, not failing. ctest reports it as "Disabled" and does not count it
    # against the run, which keeps a red suite meaning "something is broken" rather than
    # "this container has no Wine". The binary is still BUILT -- compiling and linking
    # the suite for the real target is signal in its own right.
    set_tests_properties(warz_tests PROPERTIES DISABLED TRUE)
endif()

if(WARZ_EMULATOR_WORKS)
    set(_emu_status "${CMAKE_CROSSCOMPILING_EMULATOR} (verified: runs PE32)")
elseif(CMAKE_CROSSCOMPILING_EMULATOR)
    set(_emu_status "${CMAKE_CROSSCOMPILING_EMULATOR} FOUND BUT CANNOT RUN PE32 -- warz_tests disabled")
else()
    set(_emu_status "none -- warz_tests disabled")
endif()

message(STATUS "")
message(STATUS "Warz tests -- CROSS build")
message(STATUS "  Emulator          : ${_emu_status}")
message(STATUS "  Compile-only tier : warz_layout_checks  (needs no runtime)")
message(STATUS "  Running tier      : warz_tests          (needs a working emulator)")
message(STATUS "  Host-native tier  : cmake -B build-host -S tests")
message(STATUS "")
