//=========================================================================
//  Scaleform GFx -> RmlUi integration -- implementation
//  See RmlUiMovie.h.
//=========================================================================

#include "r3dPCH.h"
#include "r3d.h"

#include "RmlUiMovie.h"

#include "RmlUi/Core/Core.h"
#include "RmlUi/Core/Context.h"
#include "RmlUi/Core/ElementDocument.h"
#include "RmlUi/Core/RenderInterface.h"
#include "RmlUi/Core/SystemInterface.h"

//////////////////////////////////////////////////////////////////////////

namespace
{
    Rml::Context* gContext     = nullptr;
    bool          gInitialised = false;

    // ---------------------------------------------------------------------
    // SystemInterface: RmlUi asks the host for time and logging.
    // This one is complete -- it needs nothing from the renderer.
    // ---------------------------------------------------------------------
    class R3dSystemInterface : public Rml::SystemInterface
    {
    public:
        double GetElapsedTime() override
        {
            return (double)r3dGetTime();
        }

        bool LogMessage(Rml::Log::Type type, const Rml::String& message) override
        {
            const char* tag = "RmlUi";
            switch (type)
            {
            case Rml::Log::LT_ERROR:
            case Rml::Log::LT_ASSERT:   tag = "RmlUi ERROR";   break;
            case Rml::Log::LT_WARNING:  tag = "RmlUi WARNING"; break;
            default: break;
            }
            r3dOutToLog("%s: %s\n", tag, message.c_str());
            return true;
        }
    };

    // ---------------------------------------------------------------------
    // RenderInterface: SEAM.
    //
    // A working implementation issues r3dRenderer draw calls for RmlUi's
    // compiled geometry, manages texture handles through r3dTexture, and applies
    // scissor rectangles. That is real renderer work and belongs with the
    // renderer replacement, not with this port -- so the methods are stubbed and
    // the UI simply does not draw.
    // ---------------------------------------------------------------------
    class R3dRenderInterface : public Rml::RenderInterface
    {
    public:
        // RmlUi compiles geometry once and re-renders it, which suits r3d's
        // buffer model well -- CompileGeometry would build an r3dVertexBuffer /
        // r3dIndexBuffer pair and RenderGeometry would submit it.
        Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>,
                                                    Rml::Span<const int>) override
        {
            return 0;   // SEAM
        }

        void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f,
                            Rml::TextureHandle) override
        {
            // SEAM: submit the compiled buffers through r3dRenderer.
        }

        void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}

        Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions,
                                       const Rml::String& source) override
        {
            // SEAM: route through r3dTexture and the .wz archive filesystem.
            (void)source;
            dimensions = Rml::Vector2i(0, 0);
            return 0;
        }

        Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>,
                                           Rml::Vector2i) override
        {
            return 0;   // SEAM
        }

        void ReleaseTexture(Rml::TextureHandle) override {}

        void EnableScissorRegion(bool) override {}
        void SetScissorRegion(Rml::Rectanglei) override {}
    };

    R3dSystemInterface  gSystemInterface;
    R3dRenderInterface  gRenderInterface;
}

//////////////////////////////////////////////////////////////////////////

bool r3dRmlUiInitialise()
{
    if (gInitialised)
        return true;

    Rml::SetSystemInterface(&gSystemInterface);
    Rml::SetRenderInterface(&gRenderInterface);

    if (!Rml::Initialise())
    {
        r3dOutToLog("RmlUi: Rml::Initialise failed\n");
        return false;
    }

    int w = r3dRenderer ? r3dRenderer->ScreenW : 1280;
    int h = r3dRenderer ? r3dRenderer->ScreenH : 720;

    gContext = Rml::CreateContext("main", Rml::Vector2i(w, h));
    if (!gContext)
    {
        r3dOutToLog("RmlUi: CreateContext failed\n");
        Rml::Shutdown();
        return false;
    }

    gInitialised = true;
    r3dOutToLog("RmlUi: initialised at %dx%d (no screens authored yet)\n", w, h);
    return true;
}

void r3dRmlUiShutdown()
{
    if (!gInitialised)
        return;

    gContext = nullptr;   // owned by RmlUi; released by Shutdown()
    Rml::Shutdown();
    gInitialised = false;
}

Rml::Context* r3dRmlUiGetContext()
{
    return gContext;
}

void r3dRmlUiSetDimensions(int w, int h)
{
    if (gContext)
        gContext->SetDimensions(Rml::Vector2i(w, h));
}
