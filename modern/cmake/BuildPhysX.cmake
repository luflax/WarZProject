# PhysX 4.1 (BSD-3-Clause) built for i686-w64-mingw32.
#
# NVIDIA ships no such configuration. Their Windows build is MSVC-only -- the flags in
# physx/source/compiler/cmake/windows/CMakeLists.txt are /arch:SSE2 /d2Zi+ /GS- /GR-
# /fp:fast, and the harness above it aborts unless packman has fetched a CMakeModules
# package that is not in the repository. Their GCC build targets Linux. We need the
# intersection: GCC codegen, Windows platform sources.
#
# That intersection is real rather than invented -- PxPreprocessor.h resolves _WIN32 plus
# __GNUC__ to PX_WIN32 && PX_GCC without complaint -- but nobody upstream builds it, so
# this file is where that combination is pinned down.
#
# The compiler flags below are NVIDIA's own, taken from their two platform files and
# combined the same way the preprocessor combines the platform: the codegen half from
# linux/CMakeLists.txt (the GCC branch), the platform/define half from
# windows/CMakeLists.txt, in its release configuration.
#
# Source lists are NOT written here. They come from cmake/sources/PhysX_*.cmake, which
# tools/gen_physx_sources.py derives from NVIDIA's module files -- see the header comment
# there for why globbing this tree is wrong.

set(PHYSX_DIR "${WARZ_ROOT}/src/External/PhysX")

# NVIDIA's module split, kept as-is. Fifteen static libraries rather than one blob is
# what lets the dependency edges below be declared honestly, which in turn is what makes
# GNU ld's single left-to-right pass over the archives resolve.
set(WARZ_PHYSX_MODULES
    PhysXFoundation
    PhysXTask
    FastXml
    PhysXPvdSDK
    PhysXCommon
    LowLevel
    LowLevelAABB
    LowLevelDynamics
    SceneQuery
    SimulationController
    PhysX
    PhysXCooking
    PhysXExtensions
    PhysXCharacterKinematic
    PhysXVehicle
)

foreach(module ${WARZ_PHYSX_MODULES})
    include("${CMAKE_CURRENT_LIST_DIR}/sources/PhysX_${module}.cmake")
endforeach()

# ---------------------------------------------------------------------------
# Flags
#
# Two interface targets, and the split between them is the important part.
#
#   physx_abi   -- everything a CONSUMER must also see. Linked PUBLIC, so it reaches the
#                  game's own translation units, which include PhysX headers.
#   physx_flags -- everything internal to building PhysX. Linked PRIVATE, so it stops at
#                  the library boundary.
#
# Getting this wrong is not subtle in one direction and invisible in the other. Leaking
# the build flags outward fails loudly: -fno-exceptions and -fno-rtti reached the game
# and broke AsyncFuncs.cpp's try/catch and PostFX.h's typeid. Failing to propagate the
# ABI defines fails silently, which is worse -- DISABLE_CUDA_PHYSX decides
# PX_SUPPORT_GPU_PHYSX, which changes the body of the inline PxSceneDesc::isValid() and
# turns PX_PHYSX_GPU_API into __declspec(dllimport). Compile the library one way and the
# game the other and you have two definitions of one inline function; the linker keeps
# whichever it reaches first and reports nothing.
# ---------------------------------------------------------------------------

add_library(physx_abi INTERFACE)

# The three roots every module needs. Upstream gets these transitively: PhysXFoundation
# exports them as INTERFACE include directories -- including its own source tree, under a
# comment reading "FIXME: This is really terrible! Don't export src directories". The
# generator drops INTERFACE entries because they are install/export generator
# expressions with no in-tree meaning, so the same reach is restored here, once.
target_include_directories(physx_abi INTERFACE
    "${PHYSX_DIR}/physx-include"
    "${PHYSX_DIR}/pxshared-include"
    "${PHYSX_DIR}/physx-source/foundation/include")

target_compile_definitions(physx_abi INTERFACE
    # PVD is ON. It was 0 while PhysXWorld::Init left the debugger disconnected;
    # Init now creates the PxPvd ahead of PxCreatePhysics and threads it through, so
    # the instrumentation this define guards is what makes that connection carry data
    # rather than an empty stream. It costs nothing when nothing is listening --
    # PxPvdImpl short-circuits on a transport that never connected.
    #
    # This and PX_NVTX are strictly internal: neither appears in any header this target
    # exports, only in physx-source/*.cpp (44 files for PVD, verified by grep). They sit
    # here rather than in physx_flags because the exported include list is not purely
    # public -- it carries physx-source/foundation/include -- and a define that could
    # change a shared header's meaning is safer identical on both sides than merely
    # believed not to matter.
    PX_SUPPORT_PVD=1
    PX_NVTX=0

    # Static libraries: suppresses the __declspec(dllimport) decoration on every PhysX
    # API declaration. The game's own TUs must agree, and do -- warz_defines_common in
    # WarzConfig.cmake sets the same macro. Disagreement here is not a compile error, it
    # is a link error naming _imp__ symbols.
    PX_PHYSX_STATIC_LIB

    # No CUDA toolkit here, and no PhysXGpu DLL to delay-load. This one reaches consumers
    # through PX_SUPPORT_GPU_PHYSX, hence its place on this side of the split.
    DISABLE_CUDA_PHYSX
)

add_library(physx_flags INTERFACE)

target_compile_definitions(physx_flags INTERFACE
    # windows/CMakeLists.txt, PHYSX_WINDOWS_COMPILE_DEFS. Internal: these only silence
    # MSVC CRT deprecation warnings and select Windows headers.
    WIN32
    _CRT_SECURE_NO_DEPRECATE
    _CRT_NONSTDC_NO_DEPRECATE
    _WINSOCK_DEPRECATED_NO_WARNINGS
)

if(NOT MSVC)
    target_compile_options(physx_flags INTERFACE
        # linux/CMakeLists.txt, the "GNU" branch, verbatim.
        -fno-rtti
        -fno-exceptions
        -ffunction-sections
        -fdata-sections
        -fno-strict-aliasing        # note: their Clang branch uses -fstrict-aliasing

        # No -ffast-math, even though windows/CMakeLists.txt passes /fp:fast. That is
        # deliberate and matches upstream: NVIDIA's own GCC flag list has no fast-math
        # equivalent, and GCC's version of it is considerably more aggressive than
        # MSVC's -- it implies -ffinite-math-only, which would let the compiler assume
        # away the NaN and infinity checks PhysX writes on purpose.

        # windows/CMakeLists.txt uses /arch:SSE2 for the 32-bit configuration. Bare i686
        # has no SSE at all, and PsWindowsInlineAoS.h is written in SSE intrinsics.
        -msse2

        # PhysX builds -Werror against its own supported compilers. This is not one of
        # them, and its warnings are not ours to fix.
        -w

        # ...with one exception, re-enabled after -w because GCC applies these in order.
        #
        # "attribute directive ignored" is how GCC reports that it silently dropped a
        # __declspec it does not implement. PhysX is full of __declspec(align(N)) on the
        # Windows path, and dropping those changes structure layout and misaligns SSE
        # loads -- a runtime fault, not a compile error. It cost real time to find once
        # (see the PX_ALIGN note in PxPreprocessor.h); it should not be able to hide
        # again.
        -Wattributes
    )
endif()

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------

foreach(module ${WARZ_PHYSX_MODULES})
    string(TOUPPER ${module} MOD)
    add_library(${module} STATIC ${WARZ_${MOD}_SOURCES})
    target_include_directories(${module} PRIVATE ${WARZ_${MOD}_INCLUDES})
    target_link_libraries(${module} PUBLIC physx_abi PRIVATE physx_flags)
    set_target_properties(${module} PROPERTIES
        CXX_STANDARD 17             # not the project's 20 -- see note below
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)
endforeach()

# On the C++ standard: PhysX 4.1 is not C++20 code and cannot be compiled as C++20.
# C++20 (P1787) made it ill-formed to name a constructor with template arguments --
# "ZoneImpl<TNameProvider>(const ZoneImpl<TNameProvider>&)" -- which is precisely what
# PhysX's own PX_NOCOPY macro generates every time it is applied to a class template,
# and what several PVD type-map macros generate directly. GCC enforces the rule from
# -std=c++20 on. Fixing it would mean editing the vendored tree in dozens of places to
# no benefit.
#
# Building a library at a different standard than its consumers is normally worth
# worrying about: PhysX headers are included by the game's own translation units, which
# are C++20, so any inline function or class template whose *definition* differs between
# the two standards would be an ODR violation the linker cannot detect -- it keeps one
# definition and silently discards the other. That is not a risk here, and the reason is
# checkable rather than assumed: no header under physx-include/ or pxshared-include/
# tests __cplusplus at all, so the token stream the two sides see is identical. Only a
# language rule could separate them, and every C++20 rule change that could is about
# declarations PhysX does not write.
#
# NVIDIA builds at C++11; 17 rather than 11 only because it is the nearest standard to
# the rest of this tree that still predates the constructor-naming change.

# The only two module-private defines in the whole set. Everything else NVIDIA passes
# per-module is either the shared Windows base above, or a lib-type export macro that
# applies to SHARED builds only. PX_COOKING in particular is not optional: MeshCleaner.h
# opens with "#ifndef PX_COOKING / #error Do not include anymore!".
target_compile_definitions(PhysXCooking PRIVATE PX_COOKING)
target_compile_definitions(PhysXTask    PRIVATE _LIB)

# Dependency edges.
#
# These are ours, not NVIDIA's. Their windows/*.cmake populates the linked-library lists
# only inside "IF(NOT PX_GENERATE_STATIC_LIBRARIES)" -- a static build leaves the whole
# question to whoever links the executable, which is us. The edges below were derived
# from what actually came out undefined.
#
# PUBLIC so that a consumer naming only PhysXVehicle still gets the transitive archives,
# in an order CMake computes -- the same fix that took SupervisorServer from 62 undefined
# symbols to 3 in B3.
# Winsock, for PhysXFoundation.
#
# PsWindowsSocket.cpp is compiled unconditionally but its body is behind
# PX_SUPPORT_PVD -- with PVD off it is an empty translation unit and nothing here was
# needed. Turning PVD on makes the socket real, and with it WSASendDisconnect,
# WSAGetLastError and gethostbyaddr.
#
# Eternity already lists ws2_32, but that does not help: GNU ld walks the link line
# left to right and never revisits an archive, and PhysXFoundation lands to the RIGHT
# of Eternity's -lws2_32. Declaring the edge here is what lets CMake put ws2_32 after
# the archive that needs it. Same failure mode, and same fix, as the PUBLIC edges below.
target_link_libraries(PhysXFoundation       PUBLIC ws2_32)

target_link_libraries(PhysXTask             PUBLIC PhysXFoundation)
target_link_libraries(FastXml               PUBLIC PhysXFoundation)
target_link_libraries(PhysXPvdSDK           PUBLIC PhysXFoundation)
target_link_libraries(PhysXCommon           PUBLIC PhysXFoundation)
target_link_libraries(LowLevel              PUBLIC PhysXCommon PhysXTask)
target_link_libraries(LowLevelAABB          PUBLIC PhysXCommon)
target_link_libraries(LowLevelDynamics      PUBLIC PhysXCommon)
target_link_libraries(SceneQuery            PUBLIC PhysXCommon)
# Sc::Scene drives the broad phase and the dynamics context directly, so
# SimulationController needs LowLevelAABB (Bp::AABBManager, Bp::BroadPhase::create) and
# LowLevelDynamics (Dy::createTGSDynamicsContext) as well as LowLevel. NVIDIA declares
# none of these for a static build -- their windows/*.cmake sets the linked-library lists
# only in the SHARED branch, because a static build defers the whole question to whoever
# links the executable. Here that whoever is us.
target_link_libraries(SimulationController  PUBLIC PhysXCommon LowLevel LowLevelAABB
                                                   LowLevelDynamics)
target_link_libraries(PhysX                 PUBLIC LowLevel LowLevelAABB LowLevelDynamics
                                                   SceneQuery SimulationController
                                                   PhysXTask PhysXCommon PhysXPvdSDK)
target_link_libraries(PhysXCooking          PUBLIC PhysXCommon)
target_link_libraries(PhysXExtensions       PUBLIC PhysX PhysXCooking PhysXPvdSDK FastXml)
target_link_libraries(PhysXCharacterKinematic PUBLIC PhysXExtensions)
target_link_libraries(PhysXVehicle          PUBLIC PhysXExtensions)

# The four the game actually names. Everything else arrives transitively.
set(WARZ_PHYSX_LIBS
    PhysXVehicle
    PhysXCharacterKinematic
    PhysXExtensions
    PhysXCooking
    CACHE INTERNAL "")
