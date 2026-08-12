// SHIM: Scaleform GFx
//
// Replaces:  GFx.h from Scaleform GFx (Autodesk, discontinued 2018 -- cannot be
//            licensed at any price). See ../../../../DEPENDENCIES.md.
// Status:    TYPE SURFACE ONLY. No Flash renders.
// Later:     RmlUi (MIT). Every screen must be re-authored; nothing imports Flash.
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

class MovieDef;
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
};

class MovieDisplayHandle
{
public:
    MovieDisplayHandle() = default;
    bool NextCapture(void* = nullptr) { return false; }
};

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
};

class MovieDef
{
public:
    virtual ~MovieDef() = default;
};

} // namespace GFx
} // namespace Scaleform
