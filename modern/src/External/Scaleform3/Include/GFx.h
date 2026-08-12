// COMPAT: Scaleform GFx -> RmlUi
//
// Replaces:  GFx.h from Scaleform GFx (Autodesk, discontinued 2018 -- cannot be
//            licensed at any price). See ../../../../DEPENDENCIES.md.
// Backed by: RmlUi (MIT). Movie -> Rml::Context + Rml::ElementDocument.
//
// The Scaleform TYPE NAMES are kept because 37 files across GameEngine and
// EclipseStudio reference them, and AI_Player.H holds a GFx::Value by value.
// GFx::Value stays a tagged variant -- it is a perfectly good interchange type and
// maps onto Rml::Variant.
//
// WHAT DOES NOT CARRY OVER: the content. Every screen is authored as .swf
// (data/menu/Frontend.swf and friends) and nothing imports Flash into RML. Screens
// must be re-authored; that is UI work, not porting work.
//
// Scope: enough of the type surface for headers that DECLARE Scaleform members to
// compile -- GameEngine/APIScaleformGfx.h, and AI_Player.H which holds a
// Scaleform::GFx::Value BY VALUE, so Value needs a real definition.
//
// GameEngine/APIScaleformGfx.cpp is NOT covered: it drives the full Loader / Movie /
// Renderer / FontProvider API and is a port target, not a shim target.
//
// Clean-room declarations derived from call sites. No code originates from the
// Scaleform SDK.

#pragma once

#include <cstring>

namespace Scaleform
{

class File;

namespace Render { namespace D3D9 { class Texture; } }

namespace GFx
{

class Movie;


// Held by value (AI_Player.H m_CharIcon), so this needs a definition rather than a
// forward declaration. Modelled as a tagged variant, which is what GFx::Value is.
class Value
{
public:
    enum ValueType
    {
        VT_Undefined = 0,
        VT_Null,
        VT_Boolean,
        VT_Int,
        VT_UInt,
        VT_Number,
        VT_String,
        VT_Object,
        VT_Array,
    };

    Value() : mType(VT_Undefined), mNumber(0.0), mString(nullptr) {}
    ~Value() = default;

    ValueType   GetType()   const { return mType; }
    bool        IsUndefined() const { return mType == VT_Undefined; }
    bool        IsObject()  const { return mType == VT_Object; }
    bool        IsArray()   const { return mType == VT_Array; }
    bool        IsString()  const { return mType == VT_String; }

    bool        GetBool()   const { return mNumber != 0.0; }
    int         GetInt()    const { return static_cast<int>(mNumber); }
    unsigned    GetUInt()   const { return static_cast<unsigned>(mNumber); }
    double      GetNumber() const { return mNumber; }
    const char* GetString() const { return mString ? mString : ""; }

    // The subset of a display object's transform that Scaleform exposed through
    // Value::GetDisplayInfo / SetDisplayInfo. RmlUi's equivalents are style properties
    // (`display`, `left`, `top`), set through Rml::Element -- so this is a plain
    // value type here, carrying the state until the RmlUi layer reads it.
    struct DisplayInfo
    {
        double x = 0.0, y = 0.0, z = 0.0;
        double rotation = 0.0;
        double xscale = 100.0, yscale = 100.0;
        double alpha = 100.0;
        bool   visible = true;

        void SetX(double v)        { x = v; }
        void SetY(double v)        { y = v; }
        void SetZ(double v)        { z = v; }
        void SetRotation(double v) { rotation = v; }
        void SetXScale(double v)   { xscale = v; }
        void SetYScale(double v)   { yscale = v; }
        void SetAlpha(double v)    { alpha = v; }
        void SetVisible(bool v)    { visible = v; }

        double GetX() const        { return x; }
        double GetY() const        { return y; }
        double GetZ() const        { return z; }
        double GetAlpha() const    { return alpha; }
        bool   GetVisible() const  { return visible; }
    };

    // Display state travels with the Value so a get/modify/set round trip preserves
    // what the caller wrote, rather than silently reverting.
    bool GetDisplayInfo(DisplayInfo* out) const { if (out) *out = mDisplay; return true; }
    bool SetDisplayInfo(const DisplayInfo& in)  { mDisplay = in; return true; }

    // Text-field accessors. Nothing is rendered while the UI is shimmed out, so the
    // text is simply retained.
    bool SetText(const char* v)     { mString = v; return true; }
    bool SetTextHTML(const char* v) { mString = v; return true; }
    const char* GetText() const     { return mString ? mString : ""; }

    void SetUndefined()          { mType = VT_Undefined; }
    void SetBoolean(bool v)      { mType = VT_Boolean; mNumber = v ? 1.0 : 0.0; }
    void SetInt(int v)           { mType = VT_Int;     mNumber = v; }
    void SetUInt(unsigned v)     { mType = VT_UInt;    mNumber = v; }
    void SetNumber(double v)     { mType = VT_Number;  mNumber = v; }
    void SetString(const char* v){ mType = VT_String;  mString = v; }

    // Object/array access -- all fail while the UI is shimmed out.
    bool GetMember(const char*, Value*) const { return false; }
    bool SetMember(const char*, const Value&) { return false; }
    bool Invoke(const char*, Value*, const Value*, unsigned) { return false; }
    bool Invoke(const char*) { return false; }
    unsigned GetArraySize() const { return 0; }
    bool GetElement(unsigned, Value*) const { return false; }

private:
    ValueType   mType;
    double      mNumber;
    const char* mString;
    DisplayInfo mDisplay;
};

class MovieDisplayHandle
{
public:
    MovieDisplayHandle() = default;
    bool NextCapture(void* = nullptr) { return false; }
};

// An ActionScript-callable native function. Scaleform's Movie::CreateFunction wrapped
// one of these into a GFx::Value that could be passed back into the movie; the RmlUi
// equivalent is an event listener bound to a document. One subclass survives --
// callbackEnterPassword in FrontEndWarZ.cpp -- and it needs Call() and Params to be
// real types for its override to compile.
class FunctionHandler
{
public:
    struct Params
    {
        Value*       pRetVal   = nullptr;
        Movie*       pMovie    = nullptr;
        Value*       pThis     = nullptr;
        const Value* pArgs     = nullptr;
        unsigned     argCount  = 0;
        void*        pUserData = nullptr;
    };

    virtual ~FunctionHandler() = default;
    virtual void Call(const Params& params) = 0;
};

// Movie wraps an RmlUi context + its loaded document. Declared opaquely here so
// this header does not drag RmlUi into every translation unit that merely names
// the type; the definition lives in GameEngine/RmlUiIntegration/RmlUiMovie.h.
class Movie
{
public:
    enum ScaleModeType
    {
        SM_NoScale = 0,
        SM_ShowAll,
        SM_ExactFit,
        SM_NoBorder,
    };

    virtual ~Movie() = default;

    // Scaleform drove the timeline itself; RmlUi has no timeline, so advancing is a
    // no-op and the frame rate is the nominal 30fps the .swf assets were authored at.
    // FrontEndWarZ's only use is "advance 5 frames so the fade-in shows", which has no
    // meaning without a timeline.
    void  Advance(float /*deltaTime*/, unsigned /*frameCatchUpCount*/ = 2) {}
    float GetFrameRate() const { return 30.0f; }

    // Resolved an ActionScript path such as "_root.Main" to a display object. With no
    // document loaded there is nothing to resolve, so the Value comes back undefined.
    bool GetVariable(Value* pval, const char* /*ppathToVar*/) const
    {
        if (pval) pval->SetUndefined();
        return false;
    }

    // Bound a native callback into ActionScript. Nothing to bind while the UI is
    // shimmed out; the Value is left undefined, which is what the caller passes on.
    bool CreateFunction(Value* pvalue, FunctionHandler* /*pfc*/, void* /*puserData*/ = nullptr)
    {
        if (pvalue) pvalue->SetUndefined();
        return false;
    }

    // Backing objects, filled in by the RmlUi integration layer.
    void* rmlContext  = nullptr;   // Rml::Context*
    void* rmlDocument = nullptr;   // Rml::ElementDocument*
};

// MovieDef was Scaleform's parsed-but-not-instantiated .swf. RmlUi has no direct
// equivalent -- documents are loaded straight into a context -- so this carries
// only the source path.
class MovieDef
{
public:
    virtual ~MovieDef() = default;
    char sourcePath[260] = {0};
};

} // namespace GFx
} // namespace Scaleform
