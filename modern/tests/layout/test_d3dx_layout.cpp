// Layout of the D3DX compat types. static_assert only -- compiling this file IS the
// test, so it needs no runtime and reports in every environment.
//
// This file is compiled TWICE, and that is its whole purpose:
//
//   host-native  against tests/host/d3d9.h  -- the stand-in types
//   cross (PE32) against the real <d3d9.h>  -- what actually ships
//
// The same numbers are asserted in both. If the host stand-in ever drifts from the real
// D3D types, one of the two builds stops compiling, and the D3DX math tests -- which
// run host-native, because the PE32 suite needs Wine -- can no longer be passing against
// a fiction while the shipping layout says something else.
//
// The numbers themselves are from the DirectX 9 SDK. D3DXVECTOR3 derives from D3DVECTOR
// and D3DXMATRIX from D3DMATRIX, adding only member functions, so each derived type must
// come out exactly the size of its base -- that is the property that lets 864 uses of
// D3DXMATRIX be passed to an API expecting D3DMATRIX.

#include "d3dx9.h"

#include <cstddef>
#include <type_traits>

// ---------------------------------------------------------------------------
// The D3D base types
// ---------------------------------------------------------------------------

static_assert(sizeof(D3DVECTOR) == 12, "D3DVECTOR is three floats");
static_assert(offsetof(D3DVECTOR, x) == 0, "D3DVECTOR.x");
static_assert(offsetof(D3DVECTOR, y) == 4, "D3DVECTOR.y");
static_assert(offsetof(D3DVECTOR, z) == 8, "D3DVECTOR.z");

static_assert(sizeof(D3DMATRIX) == 64, "D3DMATRIX is 4x4 floats");
static_assert(offsetof(D3DMATRIX, _11) ==  0, "D3DMATRIX._11");
static_assert(offsetof(D3DMATRIX, _12) ==  4, "D3DMATRIX._12 -- row-major storage");
static_assert(offsetof(D3DMATRIX, _21) == 16, "D3DMATRIX._21");
static_assert(offsetof(D3DMATRIX, _41) == 48, "D3DMATRIX._41 -- translation row");
static_assert(offsetof(D3DMATRIX, _44) == 60, "D3DMATRIX._44");

// ---------------------------------------------------------------------------
// The D3DX types
//
// Size equality with the base is the load-bearing property. A D3DXMATRIX that grew a
// vptr -- one accidental `virtual` in the compat layer -- would be 68 or 72 bytes and
// every reinterpret_cast to D3DMATRIX in the engine would read garbage, silently.
// ---------------------------------------------------------------------------

static_assert(sizeof(D3DXVECTOR2) ==  8, "D3DXVECTOR2");
static_assert(sizeof(D3DXVECTOR3) == sizeof(D3DVECTOR), "D3DXVECTOR3 must not grow over D3DVECTOR");
static_assert(sizeof(D3DXVECTOR3) == 12, "D3DXVECTOR3");
static_assert(sizeof(D3DXVECTOR4) == 16, "D3DXVECTOR4");
static_assert(sizeof(D3DXQUATERNION) == 16, "D3DXQUATERNION");
static_assert(sizeof(D3DXPLANE) == 16, "D3DXPLANE");
static_assert(sizeof(D3DXMATRIX) == sizeof(D3DMATRIX), "D3DXMATRIX must not grow over D3DMATRIX");
static_assert(sizeof(D3DXMATRIX) == 64, "D3DXMATRIX");

static_assert(offsetof(D3DXVECTOR2, x) == 0, "D3DXVECTOR2.x");
static_assert(offsetof(D3DXVECTOR2, y) == 4, "D3DXVECTOR2.y");
static_assert(offsetof(D3DXVECTOR4, x) ==  0, "D3DXVECTOR4.x");
static_assert(offsetof(D3DXVECTOR4, w) == 12, "D3DXVECTOR4.w");
static_assert(offsetof(D3DXQUATERNION, x) ==  0, "D3DXQUATERNION.x");
static_assert(offsetof(D3DXQUATERNION, w) == 12, "D3DXQUATERNION.w -- scalar LAST, as D3DX has it");
static_assert(offsetof(D3DXPLANE, a) ==  0, "D3DXPLANE.a");
static_assert(offsetof(D3DXPLANE, d) == 12, "D3DXPLANE.d");

// Alignment: 4, not 16. D3DX made no SIMD alignment promise, and the engine embeds
// these in #pragma pack(1) structures and file-format records. A compat layer that
// "helpfully" over-aligned them would change the size of everything containing one.
static_assert(alignof(D3DXVECTOR3) == 4, "D3DXVECTOR3 alignment");
static_assert(alignof(D3DXMATRIX)  == 4, "D3DXMATRIX alignment");

// ---------------------------------------------------------------------------
// Bit-copyability
//
// The engine memcpy()s these, writes them to disk and reads them back, and casts
// D3DXMATRIX* to float*. All of that requires trivial copy and standard layout.
// ---------------------------------------------------------------------------

static_assert(std::is_trivially_copyable_v<D3DXVECTOR3>, "D3DXVECTOR3 must be memcpy-able");
static_assert(std::is_trivially_copyable_v<D3DXMATRIX>,  "D3DXMATRIX must be memcpy-able");
static_assert(std::is_trivially_copyable_v<D3DXQUATERNION>, "D3DXQUATERNION must be memcpy-able");

// D3DXVECTOR3 and D3DXMATRIX inherit data members from D3DVECTOR / D3DMATRIX and add
// none of their own, which keeps them standard-layout despite the inheritance. Adding
// a single data member to the derived type would break this -- and break every
// reinterpret_cast between the pair.
static_assert(std::is_standard_layout_v<D3DXVECTOR3>, "D3DXVECTOR3 must stay standard-layout");
static_assert(std::is_standard_layout_v<D3DXMATRIX>,  "D3DXMATRIX must stay standard-layout");

// The casts the engine actually performs.
static_assert(sizeof(D3DXMATRIX) == 16 * sizeof(float), "D3DXMATRIX -> float[16]");
static_assert(sizeof(D3DXVECTOR3) == 3 * sizeof(float), "D3DXVECTOR3 -> float[3]");

// ---------------------------------------------------------------------------
// Scalar vocabulary
//
// The host stand-in declares these itself; the cross build gets them from the Windows
// SDK. Asserting them here is what makes the two agree by construction rather than by
// hope -- DWORD in particular is 32-bit on Win32 but `unsigned long` is 64-bit on a
// Linux host, which is exactly the kind of drift that would otherwise go unnoticed.
// ---------------------------------------------------------------------------

static_assert(sizeof(DWORD) == 4, "DWORD is 32-bit");
static_assert(sizeof(UINT)  == 4, "UINT is 32-bit");
static_assert(sizeof(FLOAT) == 4, "FLOAT is float32");
static_assert(sizeof(WORD)  == 2, "WORD is 16-bit");
static_assert(sizeof(BYTE)  == 1, "BYTE is 8-bit");
static_assert(sizeof(HRESULT) == 4, "HRESULT is 32-bit");

// D3DVIEWPORT9 is read field-by-field by D3DXVec3Project / D3DXVec3Unproject, which the
// host build tests by running them, so its layout has to match on both sides.
static_assert(sizeof(D3DVIEWPORT9) == 24, "D3DVIEWPORT9");
static_assert(offsetof(D3DVIEWPORT9, X)      ==  0, "D3DVIEWPORT9.X");
static_assert(offsetof(D3DVIEWPORT9, Width)  ==  8, "D3DVIEWPORT9.Width");
static_assert(offsetof(D3DVIEWPORT9, MinZ)   == 16, "D3DVIEWPORT9.MinZ");
static_assert(offsetof(D3DVIEWPORT9, MaxZ)   == 20, "D3DVIEWPORT9.MaxZ");
