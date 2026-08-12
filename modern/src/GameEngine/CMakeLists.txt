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

# GameEngine is where PhysX actually lives -- PhysXWorld, VehicleManager,
# VehicleDescriptor -- so both variants compile PxPhysicsAPI.h and must see the same
# configuration the library was built with. physx_abi carries exactly that, and nothing
# else; the build flags stay on PhysX's side of the boundary. See cmake/BuildPhysX.cmake.
#
# This is not about symbols. Both binaries that need PhysX code name the libraries
# themselves (EclipseStudio and GameServer), and the tree linked without this. It is
# about DISABLE_CUDA_PHYSX, which decides PX_SUPPORT_GPU_PHYSX, which changes the body of
# the inline PxSceneDesc::isValid() and the decoration on every GPU entry point. Compiled
# one way here and another way in the library, those are two definitions of one inline
# function -- the linker keeps whichever it meets first and says nothing.
if(TARGET physx_abi)
    foreach(variant client server)
        target_link_libraries(GameEngine_${variant} PUBLIC physx_abi)
    endforeach()
endif()

# RmlUiIntegration/RmlUiMovie.cpp is client-only and includes RmlUi's headers, so the
# client variant must link the library -- not just for the symbols, but for the
# RMLUI_STATIC_LIB definition rmlui_core carries PUBLIC. Without it the headers decorate
# every Rml:: symbol with __declspec(dllimport), and the link fails on 26 mangled
# "_imp___ZN3Rml..." names that no static archive can ever provide.
if(WARZ_RMLUI_LIBS)
    target_link_libraries(GameEngine_client PUBLIC ${WARZ_RMLUI_LIBS})
endif()
