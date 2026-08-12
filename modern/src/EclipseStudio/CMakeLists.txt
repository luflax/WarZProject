# EclipseStudio -- the game client AND every editor, in one executable.
#
# The level, particle, physics and character editors are #ifndef FINAL_BUILD branches in
# this same binary, which is why 209 sources produce one exe.
#
# Source list from Studio.vcxproj via tools/gen_sources.py. Fifteen files on disk are
# orphans that no project ever compiled -- the whole Mech/ and http/ directories among
# them -- and a glob would drag them in.

include(${WARZ_ROOT}/cmake/sources/EclipseStudio.cmake)

add_executable(WarZ WIN32 ${WARZ_ECLIPSESTUDIO_SOURCES})

set_target_properties(WarZ PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    OUTPUT_NAME "WarZ"
)

target_link_libraries(WarZ PRIVATE
    GameEngine_client          # brings Eternity_client transitively
    warz_defines_client
    warz_compiler_flags
    warz_final_config

    # Client-only third-party. RakNet, pugixml and the Win32 import libraries come
    # transitively from Eternity, which declares them PUBLIC -- see its CMakeLists for
    # why repeating them here breaks the link order.
    ${WARZ_RECAST_LIBS}     # Detour/DetourCrowd -- replaces Kynapse
    ${WARZ_RMLUI_LIBS}      # replaces Scaleform GFx
    ${WARZ_PHYSX_LIBS}
)
