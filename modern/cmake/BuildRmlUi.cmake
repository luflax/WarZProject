# Build RmlUi Core directly, bypassing RmlUi's own build system.
#
# Why not add_subdirectory(RmlUi/Source): that CMakeLists calls helper functions --
# set_common_target_options() and friends -- that RmlUi defines in its ROOT
# CMakeLists.txt and CMake/ modules, neither of which was vendored here. Only Include/
# and Source/ were copied. Vendoring RmlUi's whole build system to get one static
# library is more moving parts than the library itself.
#
# Core is self-contained: the only external dependency in the whole tree is FreeType,
# and it is confined to Source/Core/FontEngineDefault/FreeTypeInterface.cpp. Omitting
# that directory and leaving RMLUI_FONT_ENGINE_FREETYPE undefined gives a Core that
# builds and links with no third-party dependency at all -- it lays out and renders
# everything except text, which is a Milestone C concern, not a Milestone B one.

set(RMLUI_DIR "${WARZ_ROOT}/src/External/RmlUi")

file(GLOB RMLUI_CORE_SOURCES CONFIGURE_DEPENDS
    "${RMLUI_DIR}/Source/Core/*.cpp"
    "${RMLUI_DIR}/Source/Core/Elements/*.cpp"
    "${RMLUI_DIR}/Source/Core/Layout/*.cpp"
)

find_package(Freetype QUIET)

if(FREETYPE_FOUND)
    file(GLOB RMLUI_FONT_SOURCES CONFIGURE_DEPENDS
        "${RMLUI_DIR}/Source/Core/FontEngineDefault/*.cpp")
    list(APPEND RMLUI_CORE_SOURCES ${RMLUI_FONT_SOURCES})
endif()

add_library(rmlui_core STATIC ${RMLUI_CORE_SOURCES})

target_include_directories(rmlui_core PUBLIC "${RMLUI_DIR}/Include")

# RmlUi targets C++17 and is not ours to modernise; building it at its own standard
# keeps this port's conformance signal focused on the game code.
set_target_properties(rmlui_core PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

if(FREETYPE_FOUND)
    target_compile_definitions(rmlui_core PRIVATE RMLUI_FONT_ENGINE_FREETYPE)
    target_link_libraries(rmlui_core PRIVATE Freetype::Freetype)
    message(STATUS "RmlUi: building with the FreeType font engine")
else()
    message(STATUS "RmlUi: no font engine (FreeType absent for the target). "
                   "The UI lays out but renders no text.")
endif()

# RMLUI_STATIC_LIB stops the headers decorating symbols with __declspec(dllimport),
# which would otherwise leave every Rml:: symbol unresolved against a static library.
target_compile_definitions(rmlui_core PUBLIC RMLUI_STATIC_LIB)

if(MSVC)
    target_compile_options(rmlui_core PRIVATE /w)
else()
    target_compile_options(rmlui_core PRIVATE -w)
endif()

set(WARZ_RMLUI_LIBS rmlui_core CACHE INTERNAL "")
