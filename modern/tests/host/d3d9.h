// HOST-BUILD ONLY. Enough of <d3d9.h> and the Win32 type vocabulary for the D3DX
// compat layer's MATH half to compile on a Linux host.
//
// WHY THIS EXISTS
//
// src/External/dxsdk/Include/d3dx9.h is the port's clean-room D3DX replacement, and its
// math is the highest-risk code in the tree: D3DXMATRIX appears 864 times, every
// object's world matrix is built through it, and unlike every other shim it fails
// SILENTLY -- it returns numbers rather than E_NOTIMPL. It has to be tested by RUNNING
// it, not by compiling it.
//
// The shipping target is PE32, which needs Wine to run, and Wine is not available in
// every container this port is developed in (wine32:i386 is frequently unresolvable).
// So the math tests are ALSO built for the host, where they can actually execute. That
// needs the handful of D3D types the math layer inherits from.
//
// WHAT KEEPS THIS HONEST
//
// This header could drift from the real d3d9.h and the tests would still pass against
// a fiction. Two things prevent that:
//
//   1. tests/layout/test_d3dx_layout.cpp asserts the size and field offsets of
//      D3DVECTOR, D3DMATRIX and every D3DX type built on them. It is compiled BOTH
//      host-native against this header AND cross against the real MinGW <d3d9.h>, and
//      it asserts the same numbers in both. A divergence is a compile error.
//   2. Nothing here is used by anything that ships. It is confined to tests/host/ and
//      is only ever on the include path of the host test build.
//
// Field layout below matches the DirectX 9 SDK's d3d9types.h exactly. Do not "tidy" it.

#ifndef WARZ_TEST_HOST_D3D9_H
#define WARZ_TEST_HOST_D3D9_H

#include <cstdint>

// ---------------------------------------------------------------------------
// Win32 scalar vocabulary
// ---------------------------------------------------------------------------

typedef float          FLOAT;
typedef int            BOOL;
typedef int            INT;
typedef unsigned int   UINT;
typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef uint32_t       DWORD;
typedef long           LONG;
typedef int32_t        HRESULT;

typedef char           CHAR;
typedef wchar_t        WCHAR;
typedef const char*    LPCSTR;
typedef char*          LPSTR;
typedef const wchar_t* LPCWSTR;
typedef wchar_t*       LPWSTR;
typedef void*          LPVOID;
typedef const void*    LPCVOID;
typedef void*          HWND;
typedef void*          HDC;

typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *LPRECT;

typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT, *LPPOINT;

// Font face-name buffer size, from wingdi.h. Only d3dx9core.h's D3DXFONT_DESC uses it,
// and that struct is part of the shim's stubbed half.
#ifndef LF_FACESIZE
#define LF_FACESIZE 32
#endif

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define S_OK          ((HRESULT)0)
#define E_FAIL        ((HRESULT)0x80004005L)
#define E_NOTIMPL     ((HRESULT)0x80004001L)
#define E_INVALIDARG  ((HRESULT)0x80070057L)
#define E_OUTOFMEMORY ((HRESULT)0x8007000EL)
#define D3D_OK        S_OK

#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr)    (((HRESULT)(hr)) <  0)

// Calling-convention and COM decoration. On the host these are all nothing: the tests
// call plain C++ functions, and nothing here crosses an ABI boundary.
#define WINAPI
#define STDMETHODCALLTYPE
#define __stdcall
#define STDMETHOD(m)          virtual HRESULT m
#define STDMETHOD_(t, m)      virtual t m
#define PURE                  = 0

// ---------------------------------------------------------------------------
// The three D3D types the D3DX math layer is built on
//
// D3DXVECTOR3 derives from D3DVECTOR and D3DXMATRIX from D3DMATRIX, so their layout
// is these definitions plus nothing -- both derived types add only member functions.
// ---------------------------------------------------------------------------

typedef struct _D3DVECTOR {
    float x;
    float y;
    float z;
} D3DVECTOR;

typedef struct _D3DMATRIX {
    union {
        struct {
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
            float _41, _42, _43, _44;
        };
        float m[4][4];
    };
} D3DMATRIX;

typedef struct _D3DCOLORVALUE {
    float r;
    float g;
    float b;
    float a;
} D3DCOLORVALUE;

// D3DCOLOR is a packed ARGB DWORD, and D3DXCOLOR converts to and from it.
typedef DWORD D3DCOLOR;

#define D3DCOLOR_ARGB(a, r, g, b) \
    ((D3DCOLOR)((((a) & 0xff) << 24) | (((r) & 0xff) << 16) | (((g) & 0xff) << 8) | ((b) & 0xff)))
#define D3DCOLOR_RGBA(r, g, b, a) D3DCOLOR_ARGB(a, r, g, b)
#define D3DCOLOR_XRGB(r, g, b)    D3DCOLOR_ARGB(0xff, r, g, b)

// ---------------------------------------------------------------------------
// Format enum. Only the members the compat layer's math and its IMAGE_INFO struct
// name; the imaging half is not part of the host build.
// ---------------------------------------------------------------------------

typedef enum _D3DFORMAT {
    D3DFMT_UNKNOWN       = 0,
    D3DFMT_A8R8G8B8      = 21,
    D3DFMT_X8R8G8B8      = 22,
    D3DFMT_A8            = 28,
    D3DFMT_DXT1          = 0x31545844,
    D3DFMT_DXT3          = 0x33545844,
    D3DFMT_DXT5          = 0x35545844,
    D3DFMT_FORCE_DWORD   = 0x7fffffff
} D3DFORMAT;

// ---------------------------------------------------------------------------
// Device and resource interfaces
//
// Opaque. d3dx9core.h and the stubbed imaging half of d3dx9.h name these in signatures;
// nothing in the host build dereferences one. Declaring them as incomplete structs is
// deliberate -- it makes any accidental use in a host test a compile error rather than
// a test running against a fake device.
// ---------------------------------------------------------------------------

struct IDirect3DDevice9;
struct IDirect3DTexture9;
struct IDirect3DCubeTexture9;
struct IDirect3DVolumeTexture9;
struct IDirect3DSurface9;
struct IDirect3DBaseTexture9;
struct IDirect3DVertexBuffer9;
struct IDirect3DIndexBuffer9;

typedef IDirect3DDevice9*        LPDIRECT3DDEVICE9;
typedef IDirect3DTexture9*       LPDIRECT3DTEXTURE9;
typedef IDirect3DCubeTexture9*   LPDIRECT3DCUBETEXTURE9;
typedef IDirect3DVolumeTexture9* LPDIRECT3DVOLUMETEXTURE9;
typedef IDirect3DSurface9*       LPDIRECT3DSURFACE9;
typedef IDirect3DBaseTexture9*   LPDIRECT3DBASETEXTURE9;
typedef IDirect3DVertexBuffer9*  LPDIRECT3DVERTEXBUFFER9;
typedef IDirect3DIndexBuffer9*   LPDIRECT3DINDEXBUFFER9;

// D3DVIEWPORT9 is NOT opaque: D3DXVec3Project and D3DXVec3Unproject read all six
// fields, and both are math functions the host build tests. Layout from d3d9types.h.
typedef struct _D3DVIEWPORT9 {
    DWORD X;
    DWORD Y;
    DWORD Width;
    DWORD Height;
    float MinZ;
    float MaxZ;
} D3DVIEWPORT9;

typedef struct tagPALETTEENTRY {
    BYTE peRed;
    BYTE peGreen;
    BYTE peBlue;
    BYTE peFlags;
} PALETTEENTRY, *LPPALETTEENTRY;

// Vertex declarations. D3DXDeclaratorFromFVF and D3DXGetDeclVertexSize read these, and
// the latter is pure arithmetic over the type enum -- so it is testable and the enum
// values matter. Values from d3d9types.h.
typedef enum _D3DDECLTYPE {
    D3DDECLTYPE_FLOAT1    =  0,
    D3DDECLTYPE_FLOAT2    =  1,
    D3DDECLTYPE_FLOAT3    =  2,
    D3DDECLTYPE_FLOAT4    =  3,
    D3DDECLTYPE_D3DCOLOR  =  4,
    D3DDECLTYPE_UBYTE4    =  5,
    D3DDECLTYPE_SHORT2    =  6,
    D3DDECLTYPE_SHORT4    =  7,
    D3DDECLTYPE_UBYTE4N   =  8,
    D3DDECLTYPE_SHORT2N   =  9,
    D3DDECLTYPE_SHORT4N   = 10,
    D3DDECLTYPE_USHORT2N  = 11,
    D3DDECLTYPE_USHORT4N  = 12,
    D3DDECLTYPE_UDEC3     = 13,
    D3DDECLTYPE_DEC3N     = 14,
    D3DDECLTYPE_FLOAT16_2 = 15,
    D3DDECLTYPE_FLOAT16_4 = 16,
    D3DDECLTYPE_UNUSED    = 17
} D3DDECLTYPE;

typedef enum _D3DDECLMETHOD {
    D3DDECLMETHOD_DEFAULT = 0
} D3DDECLMETHOD;

typedef enum _D3DDECLUSAGE {
    D3DDECLUSAGE_POSITION     =  0,
    D3DDECLUSAGE_BLENDWEIGHT  =  1,
    D3DDECLUSAGE_BLENDINDICES =  2,
    D3DDECLUSAGE_NORMAL       =  3,
    D3DDECLUSAGE_PSIZE        =  4,
    D3DDECLUSAGE_TEXCOORD     =  5,
    D3DDECLUSAGE_TANGENT      =  6,
    D3DDECLUSAGE_BINORMAL     =  7,
    D3DDECLUSAGE_TESSFACTOR   =  8,
    D3DDECLUSAGE_POSITIONT    =  9,
    D3DDECLUSAGE_COLOR        = 10,
    D3DDECLUSAGE_FOG          = 11,
    D3DDECLUSAGE_DEPTH        = 12,
    D3DDECLUSAGE_SAMPLE       = 13
} D3DDECLUSAGE;

typedef struct _D3DVERTEXELEMENT9 {
    WORD Stream;
    WORD Offset;
    BYTE Type;
    BYTE Method;
    BYTE Usage;
    BYTE UsageIndex;
} D3DVERTEXELEMENT9;

#define MAXD3DDECLLENGTH 64

#define D3DDECL_END() { 0xFF, 0, D3DDECLTYPE_UNUSED, 0, 0, 0 }

typedef enum _D3DCUBEMAP_FACES {
    D3DCUBEMAP_FACE_POSITIVE_X = 0,
    D3DCUBEMAP_FACE_NEGATIVE_X = 1,
    D3DCUBEMAP_FACE_POSITIVE_Y = 2,
    D3DCUBEMAP_FACE_NEGATIVE_Y = 3,
    D3DCUBEMAP_FACE_POSITIVE_Z = 4,
    D3DCUBEMAP_FACE_NEGATIVE_Z = 5,
    D3DCUBEMAP_FACE_FORCE_DWORD = 0x7fffffff
} D3DCUBEMAP_FACES;

typedef struct _D3DRECT {
    LONG x1, y1, x2, y2;
} D3DRECT;

typedef enum _D3DPOOL {
    D3DPOOL_DEFAULT     = 0,
    D3DPOOL_MANAGED     = 1,
    D3DPOOL_SYSTEMMEM   = 2,
    D3DPOOL_SCRATCH     = 3,
    D3DPOOL_FORCE_DWORD = 0x7fffffff
} D3DPOOL;

typedef enum _D3DRESOURCETYPE {
    D3DRTYPE_SURFACE        = 1,
    D3DRTYPE_TEXTURE        = 3,
    D3DRTYPE_VOLUMETEXTURE  = 4,
    D3DRTYPE_CUBETEXTURE    = 5,
    D3DRTYPE_FORCE_DWORD    = 0x7fffffff
} D3DRESOURCETYPE;

#endif // WARZ_TEST_HOST_D3D9_H
