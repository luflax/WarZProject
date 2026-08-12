# Compiler configuration for the C++20 port.
#
# The critical MSVC flags are /permissive-, /Zc:__cplusplus and /Zc:preprocessor.
# Without them you are not really compiling C++20 — MSVC keeps accepting the
# pre-standard constructs this codebase is full of, and the conformance debt
# stays hidden until a Clang or GCC build is attempted.

add_library(warz_compiler_flags INTERFACE)

if(MSVC)
    target_compile_options(warz_compiler_flags INTERFACE
        /permissive-          # ISO conformance — surfaces the real work
        /Zc:__cplusplus       # report the true __cplusplus value
        /Zc:preprocessor      # conforming preprocessor
        /Zc:inline
        /Zc:throwingNew
        /MP                   # parallel compilation
        /bigobj               # AI_Player.CPP and RenderDeffered.cpp need this
        /W3
        /utf-8
    )

    # Noise suppression for a 2013 codebase. Revisit in phase 2 — these are
    # silenced to keep the phase 1 signal readable, not because they are benign.
    target_compile_options(warz_compiler_flags INTERFACE
        /wd4244    # conversion, possible loss of data
        /wd4267    # size_t -> smaller type
        /wd4305    # truncation double -> float
        /wd4996    # deprecated CRT
    )

    target_compile_definitions(warz_compiler_flags INTERFACE
        _WIN32_WINNT=0x0601   # Windows 7 — matches the compatibility target
    )
else()
    # Non-MSVC is a server-only target for now. The client is deeply Win32-bound;
    # making it portable is well outside phase 1.
    target_compile_options(warz_compiler_flags INTERFACE
        -Wall
        -Wno-unused-variable
        -Wno-unused-but-set-variable
        -Wno-multichar
        -fno-strict-aliasing   # the codebase type-puns freely
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
# single-instance guard, caps FPS. Preserved so the shipping build stays reachable.
set(CMAKE_CXX_FLAGS_FINAL "${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_FINAL "${CMAKE_EXE_LINKER_FLAGS_RELEASE}" CACHE STRING "" FORCE)

add_library(warz_final_config INTERFACE)
target_compile_definitions(warz_final_config INTERFACE
    $<$<CONFIG:Final>:FINAL_BUILD>
)
