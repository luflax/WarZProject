# Compiler configuration for the C++20 port.
#
# On MSVC the critical flags are /permissive-, /Zc:__cplusplus and /Zc:preprocessor.
# Without them you are not really compiling C++20 -- MSVC keeps accepting the
# pre-standard constructs this codebase is full of, and the conformance debt stays
# hidden until a Clang or GCC build is attempted.
#
# GCC/Clang is the toolchain that actually drives this port (MinGW-w64 i686), and it is
# deliberately run WITHOUT -fpermissive: that flag masked roughly seventy real
# conformance errors behind a single root cause.

add_library(warz_compiler_flags INTERFACE)

if(MSVC)
    target_compile_options(warz_compiler_flags INTERFACE
        /permissive-          # ISO conformance -- surfaces the real work
        /Zc:__cplusplus       # report the true __cplusplus value
        /Zc:preprocessor      # conforming preprocessor
        /Zc:inline
        /Zc:throwingNew
        /MP                   # parallel compilation
        /bigobj               # AI_Player.CPP and RenderDeffered.cpp need this
        /arch:SSE2
        /W3
        /utf-8
    )

    # Noise suppression for a 2013 codebase. Revisit later -- these are silenced to keep
    # the signal readable, not because they are benign.
    target_compile_options(warz_compiler_flags INTERFACE
        /wd4244    # conversion, possible loss of data
        /wd4267    # size_t -> smaller type
        /wd4305    # truncation double -> float
        /wd4996    # deprecated CRT
    )

    target_compile_definitions(warz_compiler_flags INTERFACE
        _WIN32_WINNT=0x0601   # Windows 7 -- matches the compatibility target
    )
else()
    target_compile_options(warz_compiler_flags INTERFACE
        # -fms-extensions: the codebase uses MSVC-isms the standard does not cover,
        # notably anonymous structs inside unions.
        $<$<COMPILE_LANGUAGE:CXX>:-fms-extensions>

        # -msse2: bare i686 has no SSE, so the engine's _mm_cvtss_si32 / _mm_set_ss
        # calls fail to inline with "target specific option mismatch". It is invisible
        # under -fsyntax-only and breaks five files the moment real codegen is on. The
        # original 2013 build assumed SSE2, so this restores the true target rather
        # than relaxing anything.
        -msse2

        -fno-strict-aliasing   # the codebase type-puns freely

        # -w, matching tools/probe.sh. The gate for this port is ERRORS -- strict ISO
        # C++20 with no -fpermissive -- not warnings. A 2013 codebase produces thousands
        # of -Wall diagnostics (reorder, unused-local-typedefs from the COMPILE_ASSERT
        # macro, and so on), and leaving them on buries real errors in scrollback: the
        # first failing build here printed several hundred lines of warnings around a
        # single error. Turning warnings back on and working through them is worthwhile,
        # but it is its own task, not a prerequisite for linking.
        -w
    )

    target_compile_definitions(warz_compiler_flags INTERFACE
        _WIN32_WINNT=0x0601
    )
endif()

if(WARZ_WARNINGS_AS_ERRORS)
    if(MSVC)
        target_compile_options(warz_compiler_flags INTERFACE /WX)
    else()
        target_compile_options(warz_compiler_flags INTERFACE -Werror)
    endif()
endif()

# The original 'Final' configuration: strips editors and debug UI, enables the
# single-instance guard, caps FPS. Preserved so the shipping build stays reachable --
# but note it is a SEPARATE configuration that this port has not yet verified.
set(CMAKE_CXX_FLAGS_FINAL "${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_FINAL "${CMAKE_C_FLAGS_RELEASE}" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_FINAL "${CMAKE_EXE_LINKER_FLAGS_RELEASE}" CACHE STRING "" FORCE)

add_library(warz_final_config INTERFACE)
target_compile_definitions(warz_final_config INTERFACE
    $<$<CONFIG:Final>:FINAL_BUILD>
)
