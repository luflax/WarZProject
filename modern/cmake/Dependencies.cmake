# Dependency acquisition.
#
# Phase 1 replaces ONLY what blocks compilation. See ../DEPENDENCIES.md for the
# full audit and ../PHASE1-BUILD-PLAN.md for what is deliberately left shimmed.
#
# Everything fetched here is MIT / BSD / zlib / Apache-2.0. No commercial
# agreements, no copyleft.

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# --------------------------------------------------------------------------
# pugixml (MIT) — level/config XML.
#
# Referenced by the source as "../../External/pugiXML/src/pugixml.hpp" but NOT
# present in the original drop, so it must be vendored to compile at all.
# --------------------------------------------------------------------------
FetchContent_Declare(pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG        v1.14
    GIT_SHALLOW    TRUE
)

# --------------------------------------------------------------------------
# stb (public domain) — replaces libjpeg 6b (1998), whose .lib is absent.
# --------------------------------------------------------------------------
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(pugixml stb)

add_library(warz_stb INTERFACE)
target_include_directories(warz_stb INTERFACE ${stb_SOURCE_DIR})

# --------------------------------------------------------------------------
# Deliberately NOT fetched in phase 1
# --------------------------------------------------------------------------
#
#   PhysX 4.1 (BSD-3)  — largest sub-item; see PHASE1-BUILD-PLAN.md risk table.
#                        Start with the no-op shim, vendor once A/B are green.
#   libcurl            — optional early win, replaces Chilkat for backend HTTPS.
#   RmlUi              — Scaleform replacement. Phase 2+; requires re-authoring
#                        every screen, which is not a build-milestone concern.
#   miniaudio          — FMOD replacement. Phase 2+.
#   Recast & Detour    — Autodesk Navigation replacement. Compiled out for now
#                        via ENABLE_AUTODESK_NAVIGATION=0.
#   Jolt               — PhysX replacement. Performance phase, not build phase.
#   EnTT / Tracy       — ECS and profiler. Later phases.
#
# Adding any of these to phase 1 trades a reachable milestone for an open-ended
# migration. Resist it.
