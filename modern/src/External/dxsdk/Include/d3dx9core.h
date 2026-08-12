// COMPAT LAYER: D3DX9 core interfaces
//
// Replaces:  d3dx9core.h from the DirectX SDK (June 2010)
// Status:    STUBS. Declares the COM interface types so the codebase compiles;
//            creation functions all fail.
//
// r3dPCH.h:78 includes <d3dx9core.h> BEFORE <d3dx9.h>, so this header must stand on
// its own AND must shadow MinGW's own d3dx9core.h -- otherwise MinGW's d3dx9math.h
// gets pulled in and redefines D3DXMATRIX against our compat layer.
//
// Clean-room declarations derived from call sites. No code originates from the
// DirectX SDK.

#ifndef __WARZ_COMPAT_D3DX9CORE_H
#define __WARZ_COMPAT_D3DX9CORE_H

#include <d3d9.h>
#include <unknwn.h>

// ===========================================================================
// ID3DXBuffer -- opaque byte blob, returned by shader compilation and image save
// ===========================================================================

struct ID3DXBuffer : public IUnknown
{
    virtual LPVOID STDMETHODCALLTYPE GetBufferPointer() = 0;
    virtual DWORD  STDMETHODCALLTYPE GetBufferSize()    = 0;
};
using LPD3DXBUFFER = ID3DXBuffer*;

// ===========================================================================
// ID3DXInclude -- shader #include resolution callback
// ===========================================================================

struct ID3DXInclude
{
    virtual HRESULT STDMETHODCALLTYPE Open(int includeType, LPCSTR fileName,
                                           LPCVOID parentData, LPCVOID* data, UINT* bytes) = 0;
    virtual HRESULT STDMETHODCALLTYPE Close(LPCVOID data) = 0;
};
using LPD3DXINCLUDE = ID3DXInclude*;

// ===========================================================================
// ID3DXConstantTable -- shader constant reflection
// ===========================================================================

using D3DXHANDLE = LPCSTR;

struct ID3DXConstantTable : public IUnknown
{
    virtual LPVOID    STDMETHODCALLTYPE GetBufferPointer() = 0;
    virtual DWORD     STDMETHODCALLTYPE GetBufferSize()    = 0;
    virtual D3DXHANDLE STDMETHODCALLTYPE GetConstantByName(D3DXHANDLE constant, LPCSTR name) = 0;
    virtual HRESULT   STDMETHODCALLTYPE SetFloat(LPDIRECT3DDEVICE9 dev, D3DXHANDLE c, FLOAT f) = 0;
    virtual HRESULT   STDMETHODCALLTYPE SetFloatArray(LPDIRECT3DDEVICE9 dev, D3DXHANDLE c,
                                                      const FLOAT* f, UINT count) = 0;
};
using LPD3DXCONSTANTTABLE = ID3DXConstantTable*;

// ===========================================================================
// ID3DXFont / ID3DXSprite
//
// Used by src/Eternity/Source/d3dFont.cpp (CD3DFont). Stubbed: no text renders.
// ===========================================================================

struct ID3DXSprite;

struct ID3DXFont : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE OnLostDevice()  = 0;
    virtual HRESULT STDMETHODCALLTYPE OnResetDevice() = 0;
    virtual INT     STDMETHODCALLTYPE DrawTextA(ID3DXSprite* sprite, LPCSTR text, INT count,
                                                LPRECT rect, DWORD format, D3DCOLOR color) = 0;
};
using LPD3DXFONT = ID3DXFont*;

struct ID3DXSprite : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE Begin(DWORD flags) = 0;
    virtual HRESULT STDMETHODCALLTYPE End()              = 0;
    virtual HRESULT STDMETHODCALLTYPE OnLostDevice()     = 0;
    virtual HRESULT STDMETHODCALLTYPE OnResetDevice()    = 0;
};
using LPD3DXSPRITE = ID3DXSprite*;

struct D3DXFONT_DESCA
{
    INT     Height;
    UINT    Width;
    UINT    Weight;
    UINT    MipLevels;
    BOOL    Italic;
    BYTE    CharSet;
    BYTE    OutputPrecision;
    BYTE    Quality;
    BYTE    PitchAndFamily;
    CHAR    FaceName[LF_FACESIZE];
};
using D3DXFONT_DESC = D3DXFONT_DESCA;

inline HRESULT D3DXCreateFontA(LPDIRECT3DDEVICE9, INT, UINT, UINT, UINT, BOOL, DWORD, DWORD,
                               DWORD, DWORD, LPCSTR, LPD3DXFONT* font)
{
    if (font) *font = nullptr;
    return E_NOTIMPL;
}
#define D3DXCreateFont D3DXCreateFontA

inline HRESULT D3DXCreateSprite(LPDIRECT3DDEVICE9, LPD3DXSPRITE* sprite)
{
    if (sprite) *sprite = nullptr;
    return E_NOTIMPL;
}

// ===========================================================================
// Render-to-environment-map helper (obj_Depot / EnvmapGen use it)
// ===========================================================================

struct ID3DXRenderToEnvMap : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE OnLostDevice()  = 0;
    virtual HRESULT STDMETHODCALLTYPE OnResetDevice() = 0;
};
using LPD3DXRenderToEnvMap = ID3DXRenderToEnvMap*;

inline HRESULT D3DXCreateRenderToEnvMap(LPDIRECT3DDEVICE9, UINT, D3DFORMAT, BOOL, D3DFORMAT,
                                        LPD3DXRenderToEnvMap* out)
{
    if (out) *out = nullptr;
    return E_NOTIMPL;
}

#endif // __WARZ_COMPAT_D3DX9CORE_H
