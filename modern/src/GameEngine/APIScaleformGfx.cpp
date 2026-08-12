//=========================================================================
//  r3dScaleformMovie -- reimplemented on RmlUi
//
//  REPLACES: the Scaleform GFx implementation (Autodesk, discontinued 2018 and
//  unlicensable). The original is kept beside this file as
//  APIScaleformGfx.cpp.scaleform-orig for reference while screens are re-authored.
//
//  The public API is unchanged -- 37 files across GameEngine and EclipseStudio call
//  it -- but every method now maps onto RmlUi:
//
//      Scaleform                         RmlUi
//      ------------------------------    -------------------------------------
//      GFx::Loader + MovieDef + Movie    Rml::Context + Rml::ElementDocument
//      Movie::SetVariable("a.b.c", v)    GetElementById -> SetInnerRML / attribute
//      Movie::Invoke("m", args)          DispatchEvent(m, params) on the document
//      Movie::Advance + Display          Context::Update() + Context::Render()
//      GFx::Value                        tagged variant (see GFx.h shim)
//      FSCommandHandler                  event listener -> OnCommandCallback
//
//  WHAT IS DELIBERATELY NOT HERE
//
//  1. CONTENT. Every screen is authored as .swf (data/menu/Frontend.swf and
//     friends). Nothing imports Flash into RML, so documents fail to load until
//     each screen is re-authored. That is UI work, not porting work.
//
//  2. RENDERING. Rml::RenderInterface is stubbed in RmlUiIntegration/RmlUiMovie.cpp
//     pending an r3dRenderer-backed implementation.
//
//  Consequently the UI compiles, initialises, and draws nothing -- which is exactly
//  what PHASE1-BUILD-PLAN.md says Phase 1 leaves behind.
//=========================================================================

#include "r3dPCH.h"
#include "r3d.h"

#include "APIScaleformGfx.h"
#include "RmlUiIntegration/RmlUiMovie.h"

#include "RmlUi/Core/Core.h"
#include "RmlUi/Core/Context.h"
#include "RmlUi/Core/ElementDocument.h"
#include "RmlUi/Core/Element.h"
#include "RmlUi/Core/Types.h"

//////////////////////////////////////////////////////////////////////////
//
// UI timing counters for the profiler overlay.
//
// r3dProfilerRender.cpp declares these extern, prints them, and resets them each
// frame; the Scaleform implementation this file replaces was what defined and fed
// them. Defining them here keeps the overlay linking and keeps its numbers honest --
// the RmlUi path does not yet time itself, so they read zero, which is the truth
// rather than a fabricated figure.
//
// Feed them from Advance/Display and Invoke once the RmlUi render interface is backed
// by r3dRenderer; until then there is nothing to measure.
//
//////////////////////////////////////////////////////////////////////////

float g_ScaleFormCompositeInvoke   = 0.0f;
int   g_ScaleFormInvokeCount       = 0;
float g_ScaleFormUpdateAndDraw     = 0.0f;
int   g_ScaleFormUpdateAndDrawCount = 0;

//////////////////////////////////////////////////////////////////////////

namespace
{
    r3dScaleformMovie* gCurrentActiveMovie = NULL;
    char               gImageDirectory[MAX_PATH] = {0};

    float gUserXScale = 1.0f, gUserYScale = 1.0f;
    float gUserXOffset = 0.0f, gUserYOffset = 0.0f;

    // "a.b.c" addresses a nested Flash timeline object. RmlUi has no timeline, so
    // the trailing component is treated as an element id -- the closest useful
    // mapping, and the one screen authors will target.
    const char* LeafOf(const char* path)
    {
        if (!path) return "";
        const char* dot = strrchr(path, '.');
        return dot ? dot + 1 : path;
    }

    Rml::Element* FindElement(Rml::ElementDocument* doc, const char* path)
    {
        if (!doc || !path) return NULL;
        return doc->GetElementById(LeafOf(path));
    }
}

//////////////////////////////////////////////////////////////////////////
// Global lifecycle
//////////////////////////////////////////////////////////////////////////

void r3dScaleformGfxCreate()
{
    r3dRmlUiInitialise();
}

void r3dScaleformGfxDestroy()
{
    r3dRmlUiShutdown();
    gCurrentActiveMovie = NULL;
}

void r3dScaleformGfxSetImageDirectory(const char* dir)
{
    if (dir)
        r3dscpy(gImageDirectory, dir);
}

void r3dScaleformGfxSetFontLib(const char* /*swf*/)
{
    // Scaleform loaded fonts from a .swf font library. RmlUi resolves fonts through
    // Rml::LoadFontFace on .ttf/.otf, which is a content decision for the re-author.
}

void r3dScaleformReset()
{
    const int w = r3dRenderer ? r3dRenderer->ScreenW : 1280;
    const int h = r3dRenderer ? r3dRenderer->ScreenH : 720;
    r3dRmlUiSetDimensions(w, h);
}

void r3dScaleformSetUserMatrix(float xScale, float yScale, float xOffset, float yOffset)
{
    // Scaleform applied a user matrix on top of the movie's own transform. RmlUi has
    // no global equivalent; the values are retained so callers keep working and a
    // future render interface can apply them.
    gUserXScale  = xScale;
    gUserYScale  = yScale;
    gUserXOffset = xOffset;
    gUserYOffset = yOffset;
}

void r3dSetCurrentActiveMovie(r3dScaleformMovie* pMovie)
{
    gCurrentActiveMovie = pMovie;
}

void r3dScaleformForceReTranslation()
{
    // Scaleform re-ran its translator over every loaded movie. With RmlUi the
    // localisation pass belongs to whatever populates the documents, so the hook is
    // kept and does nothing until screens exist.
}

bool r3dScaleformGfxWinProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // SEAM: forward mouse/key messages into Rml::Context::ProcessMouseMove /
    // ProcessKeyDown etc. Input routing is meaningless without screens, and wiring
    // it now would swallow events the rest of the game still handles.
    (void)uMsg; (void)wParam; (void)lParam;
    return false;
}

//////////////////////////////////////////////////////////////////////////
// r3dScaleformMovie
//////////////////////////////////////////////////////////////////////////

r3dScaleformMovie::r3dScaleformMovie()
    : pMovieDef(NULL)
    , pMovie(NULL)
    , viewX(0), viewY(0), viewW(0), viewH(0)
    , movieW(0), movieH(0)
    , ConvertMouseCoords(0)
    , timeForNextUpdate(0.0f)
    , timePrevUpdate(0.0f)
    , byteSize(0)
    , NumGfxEvents(0)
{
}

r3dScaleformMovie::~r3dScaleformMovie()
{
    Unload();
}

bool r3dScaleformMovie::Load(const char* fname, bool /*set_keyboard_focus*/)
{
    Unload();

    Rml::Context* ctx = r3dRmlUiGetContext();
    if (!ctx)
        return false;

    pMovieDef = new Scaleform::GFx::MovieDef();
    r3dscpy(pMovieDef->sourcePath, fname ? fname : "");

    // Screens are still .swf; RmlUi cannot load them. LoadDocument fails cleanly and
    // the caller takes its error path, which is the honest behaviour until each
    // screen is re-authored as .rml.
    Rml::ElementDocument* doc = ctx->LoadDocument(fname ? fname : "");
    if (!doc)
    {
        r3dOutToLog("RmlUi: cannot load '%s' -- screens must be re-authored as .rml\n",
                    fname ? fname : "(null)");
        delete pMovieDef;
        pMovieDef = NULL;
        return false;
    }

    pMovie = new Scaleform::GFx::Movie();
    pMovie->rmlContext  = ctx;
    pMovie->rmlDocument = doc;

    doc->Show();

    const Rml::Vector2i dims = ctx->GetDimensions();
    movieW = dims.x;
    movieH = dims.y;

    return true;
}

void r3dScaleformMovie::Unload()
{
    if (pMovie)
    {
        Rml::ElementDocument* doc = static_cast<Rml::ElementDocument*>(pMovie->rmlDocument);
        if (doc)
            doc->Close();
        delete pMovie;
        pMovie = NULL;
    }
    delete pMovieDef;
    pMovieDef = NULL;

    NumGfxEvents = 0;
}

r3dScaleformMovie* r3dScaleformMovie::SetKeyboardCapture()
{
    r3dScaleformMovie* prev = gCurrentActiveMovie;
    gCurrentActiveMovie = this;
    return prev;
}

//////////////////////////////////////////////////////////////////////////
// Viewport
//////////////////////////////////////////////////////////////////////////

void r3dScaleformMovie::GetViewport(int* x, int* y, int* w, int* h) const
{
    if (x) *x = viewX;
    if (y) *y = viewY;
    if (w) *w = viewW;
    if (h) *h = viewH;
}

void r3dScaleformMovie::SetViewportTemp(int x, int y, int w, int h,
                                        Scaleform::GFx::Movie::ScaleModeType /*scaleType*/,
                                        int NeedConvertMouseCoords)
{
    viewX = x; viewY = y; viewW = w; viewH = h;
    ConvertMouseCoords = NeedConvertMouseCoords;

    // RmlUi scales a context by its dimensions rather than a scale mode; the mode is
    // therefore expressed by what the caller passes as w/h.
    if (pMovie && pMovie->rmlContext && w > 0 && h > 0)
        static_cast<Rml::Context*>(pMovie->rmlContext)->SetDimensions(Rml::Vector2i(w, h));
}

void r3dScaleformMovie::SetBackBufferViewport(Scaleform::GFx::Movie::ScaleModeType scaleType)
{
    const int w = r3dRenderer ? r3dRenderer->ScreenW : 1280;
    const int h = r3dRenderer ? r3dRenderer->ScreenH : 720;
    SetViewportTemp(0, 0, w, h, scaleType, 1);
}

void r3dScaleformMovie::SetCurentRTViewport(Scaleform::GFx::Movie::ScaleModeType scaleType)
{
    SetBackBufferViewport(scaleType);
}

//////////////////////////////////////////////////////////////////////////
// Variables
//////////////////////////////////////////////////////////////////////////

void r3dScaleformMovie::SetVariable(const char* Var, const char* Value)
{
    if (!pMovie) return;
    if (Rml::Element* e = FindElement(static_cast<Rml::ElementDocument*>(pMovie->rmlDocument), Var))
        e->SetInnerRML(Value ? Value : "");
}

void r3dScaleformMovie::SetVariable(const char* Var, const wchar_t* Value)
{
    if (!Value) { SetVariable(Var, ""); return; }

    char buf[1024] = {0};
    WideCharToMultiByte(CP_UTF8, 0, Value, -1, buf, sizeof(buf) - 1, NULL, NULL);
    SetVariable(Var, buf);
}

void r3dScaleformMovie::SetVariable(const char* Var, const float Value)
{
    char buf[64];
    sprintf(buf, "%g", Value);
    SetVariable(Var, buf);
}

void r3dScaleformMovie::SetVariable(const char* Var, const int Value)
{
    char buf[64];
    sprintf(buf, "%d", Value);
    SetVariable(Var, buf);
}

void r3dScaleformMovie::SetVariableArrayElement(const char* /*VarPath*/, unsigned int /*index*/,
                                                unsigned int /*count*/, const void* /*data*/)
{
    // Scaleform wrote directly into an ActionScript array. The RmlUi equivalent is a
    // data model bound to a container element, which only exists once the screen is
    // authored -- there is nothing meaningful to write to yet.
}

//////////////////////////////////////////////////////////////////////////
// Invocation
//////////////////////////////////////////////////////////////////////////

void r3dScaleformMovie::Invoke(const char* Var, const char* Value)
{
    if (!pMovie) return;
    Rml::ElementDocument* doc = static_cast<Rml::ElementDocument*>(pMovie->rmlDocument);
    if (!doc) return;

    Rml::Dictionary params;
    params["value"] = Rml::Variant(Rml::String(Value ? Value : ""));
    doc->DispatchEvent(LeafOf(Var), params);
}

void r3dScaleformMovie::Invoke(const char* Var, const wchar_t* Value)
{
    char buf[1024] = {0};
    if (Value)
        WideCharToMultiByte(CP_UTF8, 0, Value, -1, buf, sizeof(buf) - 1, NULL, NULL);
    Invoke(Var, buf);
}

void r3dScaleformMovie::Invoke(const char* Var, float Value)
{
    if (!pMovie) return;
    Rml::ElementDocument* doc = static_cast<Rml::ElementDocument*>(pMovie->rmlDocument);
    if (!doc) return;

    Rml::Dictionary params;
    params["value"] = Rml::Variant(Value);
    doc->DispatchEvent(LeafOf(Var), params);
}

void r3dScaleformMovie::Invoke(const char* pmethodName, const Scaleform::GFx::Value* pargs, uint32_t numArgs)
{
    if (!pMovie) return;
    Rml::ElementDocument* doc = static_cast<Rml::ElementDocument*>(pMovie->rmlDocument);
    if (!doc) return;

    Rml::Dictionary params;
    for (uint32_t i = 0; i < numArgs; ++i)
    {
        char key[32];
        sprintf(key, "arg%u", i);

        const Scaleform::GFx::Value& v = pargs[i];
        switch (v.GetType())
        {
        case Scaleform::GFx::Value::VT_String:
            params[key] = Rml::Variant(Rml::String(v.GetString()));
            break;
        case Scaleform::GFx::Value::VT_Boolean:
            params[key] = Rml::Variant(v.GetBool());
            break;
        case Scaleform::GFx::Value::VT_Int:
        case Scaleform::GFx::Value::VT_UInt:
            params[key] = Rml::Variant((int)v.GetInt());
            break;
        default:
            params[key] = Rml::Variant((float)v.GetNumber());
            break;
        }
    }
    doc->DispatchEvent(LeafOf(pmethodName), params);
}

void r3dScaleformMovie::Invoke(const char* pmethodName, Scaleform::GFx::Value* presult,
                               const Scaleform::GFx::Value* pargs, uint32_t numArgs)
{
    Invoke(pmethodName, pargs, numArgs);

    // RmlUi events do not return values; a screen that needs to report back does so
    // through a data model or a follow-up FSCommand-style event.
    if (presult)
        presult->SetUndefined();
}

//////////////////////////////////////////////////////////////////////////
// Update / draw
//////////////////////////////////////////////////////////////////////////

void r3dScaleformMovie::UpdateAndDraw(bool skipDraw)
{
    if (!pMovie || !pMovie->rmlContext)
        return;

    Rml::Context* ctx = static_cast<Rml::Context*>(pMovie->rmlContext);

    ctx->Update();
    timePrevUpdate = r3dGetTime();

    if (!skipDraw)
    {
        // Draws nothing until Rml::RenderInterface is implemented against
        // r3dRenderer -- see RmlUiIntegration/RmlUiMovie.cpp.
        ctx->Render();
    }
}

//////////////////////////////////////////////////////////////////////////
// Render-target binding
//////////////////////////////////////////////////////////////////////////

Scaleform::Render::D3D9::Texture* r3dScaleformMovie::BoundRTToImage(const char* /*resName*/,
                                                                    LPDIRECT3DTEXTURE9 /*pRenderTarget*/,
                                                                    int /*RTWidth*/, int /*RTHeight*/)
{
    // Scaleform could bind a live D3D9 render target as a movie image (used for the
    // in-game weapon preview). The RmlUi equivalent is a texture handle produced by
    // the render interface, so this waits on that.
    return NULL;
}

void r3dScaleformMovie::UpdateTextureMatrices(const char* /*resName*/, int /*RTWidth*/, int /*RTHeight*/)
{
}

//////////////////////////////////////////////////////////////////////////
// Event handlers
//////////////////////////////////////////////////////////////////////////

BOOL r3dScaleformMovie::RegisterEventHandler(const char* EventString, void* data, fn_gfxEventHandler1 Fnc)
{
    if (NumGfxEvents >= 256) return FALSE;

    gfxEvent& e = EventHandlers[NumGfxEvents++];
    e.EventName = EventString;
    e.data      = data;
    e.fnc1      = Fnc;
    e.fnc2      = NULL;
    return TRUE;
}

BOOL r3dScaleformMovie::RegisterEventHandler(const char* EventString, void* data, fn_gfxEventHandler2 Fnc)
{
    if (NumGfxEvents >= 256) return FALSE;

    gfxEvent& e = EventHandlers[NumGfxEvents++];
    e.EventName = EventString;
    e.data      = data;
    e.fnc1      = NULL;
    e.fnc2      = Fnc;
    return TRUE;
}

BOOL r3dScaleformMovie::RegisterEventHandler(const char* EventString, sGFxEICallback* eiCallback)
{
    if (NumGfxEvents >= 256) return FALSE;

    gfxEvent& e = EventHandlers[NumGfxEvents++];
    e.EventName  = EventString;
    e.eiCallback = eiCallback;
    e.fnc1       = NULL;
    e.fnc2       = NULL;
    return TRUE;
}

void r3dScaleformMovie::OnCommandCallback(const char* command, const char* arg)
{
    for (int i = 0; i < NumGfxEvents; ++i)
    {
        gfxEvent& e = EventHandlers[i];
        if (e.EventName != command)
            continue;

        if (e.fnc1)
            e.fnc1(e.data, this, arg);
        else if (e.eiCallback)
            e.eiCallback->Execute(this, NULL, 0);
        return;
    }
}

void r3dScaleformMovie::OnCommandCallback(const char* methodName,
                                          const Scaleform::GFx::Value* args, uint32_t argCount)
{
    for (int i = 0; i < NumGfxEvents; ++i)
    {
        gfxEvent& e = EventHandlers[i];
        if (e.EventName != methodName)
            continue;

        if (e.fnc2)
            e.fnc2(e.data, this, args, argCount);
        else if (e.eiCallback)
            e.eiCallback->Execute(this, args, argCount);
        return;
    }
}
