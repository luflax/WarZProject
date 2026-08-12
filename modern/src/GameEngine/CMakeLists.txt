# GameEngine -- object model, physics, terrain, AI, sound.
#
# Built TWICE, as GameEngine_client and GameEngine_server, for the same reason Eternity
# is. See cmake/WarzConfig.cmake.
#
# This tree has no .vcxproj of its own: the original listed its sources directly in
# Studio.vcxproj (89 entries) and WO_GameServer.vcxproj (47), recompiling them into
# every binary. Building them once per configuration is a deliberate change.
#
# The two variants do NOT compile the same files. 19 sources are common -- the object
# model, PhysX, terrain, AI and navigation. 25 more are client-only: everything that
# draws (SkyDome and its CloudPlane cluster, Sun, Apex, the RmlUi integration), sound,
# and the two vehicle sources that include client weapon headers which #error out under
# WO_SERVER. tools/gen_sources.py derives the split from what each .vcxproj lists.

include(${WARZ_ROOT}/cmake/sources/GameEngine.cmake)
include(${WARZ_ROOT}/cmake/sources/GameEngine_client_only.cmake)

warz_add_dual_library(GameEngine
    SOURCES             ${WARZ_GAMEENGINE_SOURCES}
    CLIENT_ONLY_SOURCES ${WARZ_GAMEENGINE_CLIENT_ONLY_SOURCES}
)

foreach(variant client server)
    target_link_libraries(GameEngine_${variant} PUBLIC Eternity_${variant})
endforeach()

# RmlUiIntegration/RmlUiMovie.cpp is client-only and includes RmlUi's headers, so the
# client variant must link the library -- not just for the symbols, but for the
# RMLUI_STATIC_LIB definition rmlui_core carries PUBLIC. Without it the headers decorate
# every Rml:: symbol with __declspec(dllimport), and the link fails on 26 mangled
# "_imp___ZN3Rml..." names that no static archive can ever provide.
if(WARZ_RMLUI_LIBS)
    target_link_libraries(GameEngine_client PUBLIC ${WARZ_RMLUI_LIBS})
endif()
