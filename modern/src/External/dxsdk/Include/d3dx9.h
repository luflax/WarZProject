// COMPAT LAYER: D3DX9
//
// Replaces:  D3DX9 from the DirectX SDK (June 2010)
// Why:       D3DX was REMOVED from the Windows SDK. The last shipping version was the
//            June 2010 DXSDK, which is deprecated and unavailable on modern toolchains.
// Status:    MATH IS FUNCTIONAL. Texture/shader/font utilities are stubs.
//
// D3DXMATRIX appears 864 times in this codebase and D3DXVECTOR3/4 roughly 1000 more.
// GameObject::UpdateTransform (GameObj.h:305-318) builds every object's world matrix
// through D3DXMatrixTranslation / D3DXMatrixScaling and operator*, so the math here
// must preserve D3DX semantics exactly or nothing transforms correctly.
//
// CONVENTION: row-major storage, row-vector convention (v' = v * M), left-handed.
// This matches D3DX exactly. Do not "fix" it to column-major.
//
// IMPLEMENTATION: deliberately scalar and dependency-free.
//   - DirectXMath is MSVC/clang-oriented and does not build cleanly under MinGW-GCC,
//     which is the cross-compilation toolchain used to drive this port.
//   - A compat shim that only builds on one compiler defeats its own purpose.
//   - SIMD belongs in the eventual math-library rewrite, not in a compatibility layer.
//
// This is a clean-room reimplementation derived from call sites in this codebase and
// from publicly documented D3DX semantics. No code originates from the DirectX SDK.
//
// COVERAGE IS INCOMPLETE BY DESIGN. Add symbols as the compiler demands them --
// see ../../README.md, "How these get built out".

#ifndef __WARZ_COMPAT_D3DX9_H
#define __WARZ_COMPAT_D3DX9_H

#include <d3d9.h>
#include "d3dx9core.h"   // COM interface types (ID3DXBuffer, ID3DXInclude, ...)
#include <cmath>
#include <cstring>
#include <utility>   // std::declval

// ===========================================================================
// Constants
// ===========================================================================

#ifndef D3DX_PI
#define D3DX_PI    (3.14159265358979323846f)
#endif

#ifndef D3DX_16F_DIG
#define D3DX_16F_DIG 3
#endif

#define D3DXToRadian(deg) ((deg) * (D3DX_PI / 180.0f))
#define D3DXToDegree(rad) ((rad) * (180.0f / D3DX_PI))

#define D3DX_DEFAULT            ((UINT)-1)
#define D3DX_DEFAULT_NONPOW2    ((UINT)-2)

#define D3DX_FILTER_NONE        (1 << 0)
#define D3DX_FILTER_POINT       (2 << 0)
#define D3DX_FILTER_LINEAR      (3 << 0)
#define D3DX_FILTER_TRIANGLE    (4 << 0)
#define D3DX_FILTER_BOX         (5 << 0)

#define D3DXERR_INVALIDDATA     MAKE_HRESULT(1, 0x876, 2900)

// "Take this from the source file" sentinels used by the D3DX*FromFile* helpers.
// Distinct from D3DX_DEFAULT: -3 rather than -1.
#define D3DX_FROM_FILE          ((UINT)-3)
#define D3DFMT_FROM_FILE        ((D3DFORMAT)-3)

#define D3DXSHADER_DEBUG                (1 << 0)
#define D3DXSHADER_SKIPOPTIMIZATION     (1 << 2)

// ===========================================================================
// Vector types
// ===========================================================================

struct D3DXVECTOR2
{
    float x, y;

    D3DXVECTOR2() = default;
    D3DXVECTOR2(float x_, float y_) : x(x_), y(y_) {}
    D3DXVECTOR2(const float* p) : x(p[0]), y(p[1]) {}

    operator float*()             { return &x; }
    operator const float*() const { return &x; }

    D3DXVECTOR2 operator+(const D3DXVECTOR2& v) const { return D3DXVECTOR2(x + v.x, y + v.y); }
    D3DXVECTOR2 operator-(const D3DXVECTOR2& v) const { return D3DXVECTOR2(x - v.x, y - v.y); }
    D3DXVECTOR2 operator*(float s) const              { return D3DXVECTOR2(x * s, y * s); }
};

// Derives from D3DVECTOR exactly as the real D3DX does, so D3DXVECTOR3 converts
// to D3DVECTOR and the float* constructor is implicit (r3dPoint3D relies on it).
struct D3DXVECTOR3 : public D3DVECTOR
{
    D3DXVECTOR3() = default;
    D3DXVECTOR3(float x_, float y_, float z_) { x = x_; y = y_; z = z_; }
    D3DXVECTOR3(const float* p) { x = p[0]; y = p[1]; z = p[2]; }
    D3DXVECTOR3(const D3DVECTOR& v) { x = v.x; y = v.y; z = v.z; }

    // Accepts any type with operator const float*() -- e.g. r3dPoint3D -- as a
    // single user-defined conversion.
    template <class T, class = decltype(static_cast<const float*>(
                                            std::declval<const T&>()))>
    D3DXVECTOR3(const T& v)
    {
        const float* p = static_cast<const float*>(v);
        x = p[0]; y = p[1]; z = p[2];
    }

    operator float*()             { return &x; }
    operator const float*() const { return &x; }

    D3DXVECTOR3 operator+(const D3DXVECTOR3& v) const { return D3DXVECTOR3(x + v.x, y + v.y, z + v.z); }
    D3DXVECTOR3 operator-(const D3DXVECTOR3& v) const { return D3DXVECTOR3(x - v.x, y - v.y, z - v.z); }
    D3DXVECTOR3 operator*(float s) const              { return D3DXVECTOR3(x * s, y * s, z * s); }
    D3DXVECTOR3 operator/(float s) const              { return D3DXVECTOR3(x / s, y / s, z / s); }
    D3DXVECTOR3 operator-() const                     { return D3DXVECTOR3(-x, -y, -z); }

    D3DXVECTOR3& operator+=(const D3DXVECTOR3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    D3DXVECTOR3& operator-=(const D3DXVECTOR3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    D3DXVECTOR3& operator*=(float s)              { x *= s; y *= s; z *= s; return *this; }
};

// D3DX spelled this both ways in different headers; the codebase uses both.
using D3DXVector3 = D3DXVECTOR3;

struct D3DXVECTOR4
{
    float x, y, z, w;

    D3DXVECTOR4() = default;
    D3DXVECTOR4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    D3DXVECTOR4(const float* p) : x(p[0]), y(p[1]), z(p[2]), w(p[3]) {}

    operator float*()             { return &x; }
    operator const float*() const { return &x; }

    D3DXVECTOR4 operator+(const D3DXVECTOR4& v) const { return D3DXVECTOR4(x + v.x, y + v.y, z + v.z, w + v.w); }
    D3DXVECTOR4 operator-(const D3DXVECTOR4& v) const { return D3DXVECTOR4(x - v.x, y - v.y, z - v.z, w - v.w); }
    D3DXVECTOR4 operator*(float s) const              { return D3DXVECTOR4(x * s, y * s, z * s, w * s); }
    D3DXVECTOR4& operator*=(float s) { x *= s; y *= s; z *= s; w *= s; return *this; }
    D3DXVECTOR4& operator+=(const D3DXVECTOR4& v) { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
    D3DXVECTOR4& operator-=(const D3DXVECTOR4& v) { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }
};

struct D3DXQUATERNION
{
    float x, y, z, w;

    D3DXQUATERNION() = default;
    D3DXQUATERNION(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

    // Hamilton product, D3DX argument order (this * rhs).
    D3DXQUATERNION operator*(const D3DXQUATERNION& q) const
    {
        return D3DXQUATERNION(
            w*q.x + x*q.w + y*q.z - z*q.y,
            w*q.y - x*q.z + y*q.w + z*q.x,
            w*q.z + x*q.y - y*q.x + z*q.w,
            w*q.w - x*q.x - y*q.y - z*q.z);
    }
    D3DXQUATERNION& operator*=(const D3DXQUATERNION& q) { *this = *this * q; return *this; }
    D3DXQUATERNION operator+(const D3DXQUATERNION& q) const
    { return D3DXQUATERNION(x+q.x, y+q.y, z+q.z, w+q.w); }
    D3DXQUATERNION operator*(float s) const
    { return D3DXQUATERNION(x*s, y*s, z*s, w*s); }

    operator float*()             { return &x; }
    operator const float*() const { return &x; }
};

struct D3DXPLANE
{
    float a, b, c, d;

    D3DXPLANE() = default;
    D3DXPLANE(float a_, float b_, float c_, float d_) : a(a_), b(b_), c(c_), d(d_) {}

    operator float*()             { return &a; }
    operator const float*() const { return &a; }
};

// ===========================================================================
// D3DXMATRIX
//
// Exposes BOTH m[i][j] and named _11.._44 through an anonymous union, because the
// codebase uses both forms -- see ShadowExtrusionData::Displace (GameObj.h:588-590),
// which pokes ToExtrusionBox.m[3][0] directly.
// ===========================================================================

// Derives from D3DMATRIX exactly as the real D3DX does. D3DMATRIX already supplies
// the _11.._44 / m[4][4] anonymous union, and inheriting it is what allows a
// D3DXMATRIX* to be passed to D3D9 entry points taking a const D3DMATRIX*.
struct D3DXMATRIX : public D3DMATRIX
{
    D3DXMATRIX() = default;
    D3DXMATRIX(const D3DMATRIX& o) : D3DMATRIX(o) {}

    D3DXMATRIX(float m11, float m12, float m13, float m14,
               float m21, float m22, float m23, float m24,
               float m31, float m32, float m33, float m34,
               float m41, float m42, float m43, float m44)
    {
        _11 = m11; _12 = m12; _13 = m13; _14 = m14;
        _21 = m21; _22 = m22; _23 = m23; _24 = m24;
        _31 = m31; _32 = m32; _33 = m33; _34 = m34;
        _41 = m41; _42 = m42; _43 = m43; _44 = m44;
    }

    explicit D3DXMATRIX(const float* p) { std::memcpy(m, p, sizeof(m)); }

    float&       operator()(unsigned r, unsigned c)       { return m[r][c]; }
    float const& operator()(unsigned r, unsigned c) const { return m[r][c]; }

    operator float*()             { return &_11; }
    operator const float*() const { return &_11; }

    D3DXMATRIX operator*(const D3DXMATRIX& rhs) const
    {
        D3DXMATRIX out;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                out.m[i][j] = m[i][0] * rhs.m[0][j]
                            + m[i][1] * rhs.m[1][j]
                            + m[i][2] * rhs.m[2][j]
                            + m[i][3] * rhs.m[3][j];
        return out;
    }

    D3DXMATRIX& operator*=(const D3DXMATRIX& rhs) { *this = *this * rhs; return *this; }

    D3DXMATRIX operator+(const D3DXMATRIX& rhs) const
    {
        D3DXMATRIX out;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                out.m[i][j] = m[i][j] + rhs.m[i][j];
        return out;
    }

    D3DXMATRIX operator*(float s) const
    {
        D3DXMATRIX out;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                out.m[i][j] = m[i][j] * s;
        return out;
    }
};

// 16-byte-aligned variant. Alignment only; layout is identical.
struct alignas(16) D3DXMATRIXA16 : public D3DXMATRIX
{
    D3DXMATRIXA16() = default;
    D3DXMATRIXA16(const D3DXMATRIX& o) : D3DXMATRIX(o) {}
};

// ===========================================================================
// Matrix construction
// ===========================================================================

inline D3DXMATRIX* D3DXMatrixIdentity(D3DXMATRIX* o)
{
    *o = D3DXMATRIX(1,0,0,0,  0,1,0,0,  0,0,1,0,  0,0,0,1);
    return o;
}

inline D3DXMATRIX* D3DXMatrixTranslation(D3DXMATRIX* o, float x, float y, float z)
{
    *o = D3DXMATRIX(1,0,0,0,  0,1,0,0,  0,0,1,0,  x,y,z,1);
    return o;
}

inline D3DXMATRIX* D3DXMatrixScaling(D3DXMATRIX* o, float sx, float sy, float sz)
{
    *o = D3DXMATRIX(sx,0,0,0,  0,sy,0,0,  0,0,sz,0,  0,0,0,1);
    return o;
}

inline D3DXMATRIX* D3DXMatrixRotationX(D3DXMATRIX* o, float a)
{
    const float s = std::sin(a), c = std::cos(a);
    *o = D3DXMATRIX(1,0,0,0,  0,c,s,0,  0,-s,c,0,  0,0,0,1);
    return o;
}

inline D3DXMATRIX* D3DXMatrixRotationY(D3DXMATRIX* o, float a)
{
    const float s = std::sin(a), c = std::cos(a);
    *o = D3DXMATRIX(c,0,-s,0,  0,1,0,0,  s,0,c,0,  0,0,0,1);
    return o;
}

inline D3DXMATRIX* D3DXMatrixRotationZ(D3DXMATRIX* o, float a)
{
    const float s = std::sin(a), c = std::cos(a);
    *o = D3DXMATRIX(c,s,0,0,  -s,c,0,0,  0,0,1,0,  0,0,0,1);
    return o;
}

inline D3DXMATRIX* D3DXMatrixRotationAxis(D3DXMATRIX* o, const D3DXVECTOR3* axis, float angle)
{
    const float len = std::sqrt(axis->x*axis->x + axis->y*axis->y + axis->z*axis->z);
    const float ax = axis->x/len, ay = axis->y/len, az = axis->z/len;
    const float s = std::sin(angle), c = std::cos(angle), t = 1.0f - c;

    *o = D3DXMATRIX(
        t*ax*ax + c,      t*ax*ay + s*az,  t*ax*az - s*ay,  0,
        t*ax*ay - s*az,   t*ay*ay + c,     t*ay*az + s*ax,  0,
        t*ax*az + s*ay,   t*ay*az - s*ax,  t*az*az + c,     0,
        0, 0, 0, 1);
    return o;
}

inline D3DXMATRIX* D3DXMatrixRotationYawPitchRoll(D3DXMATRIX* o, float yaw, float pitch, float roll)
{
    D3DXMATRIX mx, my, mz;
    D3DXMatrixRotationZ(&mz, roll);
    D3DXMatrixRotationX(&mx, pitch);
    D3DXMatrixRotationY(&my, yaw);
    *o = mz * mx * my;     // D3DX order: roll, then pitch, then yaw
    return o;
}

inline D3DXMATRIX* D3DXMatrixRotationQuaternion(D3DXMATRIX* o, const D3DXQUATERNION* q)
{
    const float xx = q->x*q->x, yy = q->y*q->y, zz = q->z*q->z;
    const float xy = q->x*q->y, xz = q->x*q->z, yz = q->y*q->z;
    const float wx = q->w*q->x, wy = q->w*q->y, wz = q->w*q->z;

    *o = D3DXMATRIX(
        1-2*(yy+zz),  2*(xy+wz),    2*(xz-wy),    0,
        2*(xy-wz),    1-2*(xx+zz),  2*(yz+wx),    0,
        2*(xz+wy),    2*(yz-wx),    1-2*(xx+yy),  0,
        0, 0, 0, 1);
    return o;
}

// ===========================================================================
// Matrix operations
// ===========================================================================

inline D3DXMATRIX* D3DXMatrixMultiply(D3DXMATRIX* o, const D3DXMATRIX* a, const D3DXMATRIX* b)
{
    *o = (*a) * (*b);
    return o;
}

inline D3DXMATRIX* D3DXMatrixTranspose(D3DXMATRIX* o, const D3DXMATRIX* in)
{
    D3DXMATRIX t;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            t.m[i][j] = in->m[j][i];
    *o = t;
    return o;
}

inline D3DXMATRIX* D3DXMatrixMultiplyTranspose(D3DXMATRIX* o, const D3DXMATRIX* a, const D3DXMATRIX* b)
{
    const D3DXMATRIX p = (*a) * (*b);
    return D3DXMatrixTranspose(o, &p);
}

// Cofactor expansion. Returns nullptr on a singular matrix, matching D3DX.
inline D3DXMATRIX* D3DXMatrixInverse(D3DXMATRIX* o, float* determinant, const D3DXMATRIX* in)
{
    const float* s = &in->_11;
    float inv[16];

    inv[0]  =  s[5]*s[10]*s[15] - s[5]*s[11]*s[14] - s[9]*s[6]*s[15] + s[9]*s[7]*s[14] + s[13]*s[6]*s[11] - s[13]*s[7]*s[10];
    inv[4]  = -s[4]*s[10]*s[15] + s[4]*s[11]*s[14] + s[8]*s[6]*s[15] - s[8]*s[7]*s[14] - s[12]*s[6]*s[11] + s[12]*s[7]*s[10];
    inv[8]  =  s[4]*s[9]*s[15]  - s[4]*s[11]*s[13] - s[8]*s[5]*s[15] + s[8]*s[7]*s[13] + s[12]*s[5]*s[11] - s[12]*s[7]*s[9];
    inv[12] = -s[4]*s[9]*s[14]  + s[4]*s[10]*s[13] + s[8]*s[5]*s[14] - s[8]*s[6]*s[13] - s[12]*s[5]*s[10] + s[12]*s[6]*s[9];

    inv[1]  = -s[1]*s[10]*s[15] + s[1]*s[11]*s[14] + s[9]*s[2]*s[15] - s[9]*s[3]*s[14] - s[13]*s[2]*s[11] + s[13]*s[3]*s[10];
    inv[5]  =  s[0]*s[10]*s[15] - s[0]*s[11]*s[14] - s[8]*s[2]*s[15] + s[8]*s[3]*s[14] + s[12]*s[2]*s[11] - s[12]*s[3]*s[10];
    inv[9]  = -s[0]*s[9]*s[15]  + s[0]*s[11]*s[13] + s[8]*s[1]*s[15] - s[8]*s[3]*s[13] - s[12]*s[1]*s[11] + s[12]*s[3]*s[9];
    inv[13] =  s[0]*s[9]*s[14]  - s[0]*s[10]*s[13] - s[8]*s[1]*s[14] + s[8]*s[2]*s[13] + s[12]*s[1]*s[10] - s[12]*s[2]*s[9];

    inv[2]  =  s[1]*s[6]*s[15]  - s[1]*s[7]*s[14]  - s[5]*s[2]*s[15] + s[5]*s[3]*s[14] + s[13]*s[2]*s[7]  - s[13]*s[3]*s[6];
    inv[6]  = -s[0]*s[6]*s[15]  + s[0]*s[7]*s[14]  + s[4]*s[2]*s[15] - s[4]*s[3]*s[14] - s[12]*s[2]*s[7]  + s[12]*s[3]*s[6];
    inv[10] =  s[0]*s[5]*s[15]  - s[0]*s[7]*s[13]  - s[4]*s[1]*s[15] + s[4]*s[3]*s[13] + s[12]*s[1]*s[7]  - s[12]*s[3]*s[5];
    inv[14] = -s[0]*s[5]*s[14]  + s[0]*s[6]*s[13]  + s[4]*s[1]*s[14] - s[4]*s[2]*s[13] - s[12]*s[1]*s[6]  + s[12]*s[2]*s[5];

    inv[3]  = -s[1]*s[6]*s[11]  + s[1]*s[7]*s[10]  + s[5]*s[2]*s[11] - s[5]*s[3]*s[10] - s[9]*s[2]*s[7]   + s[9]*s[3]*s[6];
    inv[7]  =  s[0]*s[6]*s[11]  - s[0]*s[7]*s[10]  - s[4]*s[2]*s[11] + s[4]*s[3]*s[10] + s[8]*s[2]*s[7]   - s[8]*s[3]*s[6];
    inv[11] = -s[0]*s[5]*s[11]  + s[0]*s[7]*s[9]   + s[4]*s[1]*s[11] - s[4]*s[3]*s[9]  - s[8]*s[1]*s[7]   + s[8]*s[3]*s[5];
    inv[15] =  s[0]*s[5]*s[10]  - s[0]*s[6]*s[9]   - s[4]*s[1]*s[10] + s[4]*s[2]*s[9]  + s[8]*s[1]*s[6]   - s[8]*s[2]*s[5];

    const float det = s[0]*inv[0] + s[1]*inv[4] + s[2]*inv[8] + s[3]*inv[12];
    if (determinant) *determinant = det;
    if (det == 0.0f) return nullptr;

    const float invDet = 1.0f / det;
    for (int i = 0; i < 16; ++i)
        (&o->_11)[i] = inv[i] * invDet;
    return o;
}

inline BOOL D3DXMatrixDecompose(D3DXVECTOR3* outScale, D3DXQUATERNION* outRot,
                                D3DXVECTOR3* outTrans, const D3DXMATRIX* in)
{
    if (outTrans) *outTrans = D3DXVECTOR3(in->_41, in->_42, in->_43);

    const float sx = std::sqrt(in->_11*in->_11 + in->_12*in->_12 + in->_13*in->_13);
    const float sy = std::sqrt(in->_21*in->_21 + in->_22*in->_22 + in->_23*in->_23);
    const float sz = std::sqrt(in->_31*in->_31 + in->_32*in->_32 + in->_33*in->_33);
    if (outScale) *outScale = D3DXVECTOR3(sx, sy, sz);

    if (sx == 0.0f || sy == 0.0f || sz == 0.0f) return FALSE;

    if (outRot)
    {
        const float r[3][3] = {
            { in->_11/sx, in->_12/sx, in->_13/sx },
            { in->_21/sy, in->_22/sy, in->_23/sy },
            { in->_31/sz, in->_32/sz, in->_33/sz },
        };
        const float trace = r[0][0] + r[1][1] + r[2][2];
        if (trace > 0.0f)
        {
            const float k = std::sqrt(trace + 1.0f) * 2.0f;
            *outRot = D3DXQUATERNION((r[1][2]-r[2][1])/k, (r[2][0]-r[0][2])/k, (r[0][1]-r[1][0])/k, 0.25f*k);
        }
        else if (r[0][0] > r[1][1] && r[0][0] > r[2][2])
        {
            const float k = std::sqrt(1.0f + r[0][0] - r[1][1] - r[2][2]) * 2.0f;
            *outRot = D3DXQUATERNION(0.25f*k, (r[1][0]+r[0][1])/k, (r[2][0]+r[0][2])/k, (r[1][2]-r[2][1])/k);
        }
        else if (r[1][1] > r[2][2])
        {
            const float k = std::sqrt(1.0f + r[1][1] - r[0][0] - r[2][2]) * 2.0f;
            *outRot = D3DXQUATERNION((r[1][0]+r[0][1])/k, 0.25f*k, (r[2][1]+r[1][2])/k, (r[2][0]-r[0][2])/k);
        }
        else
        {
            const float k = std::sqrt(1.0f + r[2][2] - r[0][0] - r[1][1]) * 2.0f;
            *outRot = D3DXQUATERNION((r[2][0]+r[0][2])/k, (r[2][1]+r[1][2])/k, 0.25f*k, (r[0][1]-r[1][0])/k);
        }
    }
    return TRUE;
}

// ===========================================================================
// View / projection
// ===========================================================================

inline D3DXMATRIX* D3DXMatrixLookAtLH(D3DXMATRIX* o, const D3DXVECTOR3* eye,
                                      const D3DXVECTOR3* at, const D3DXVECTOR3* up)
{
    D3DXVECTOR3 z = *at - *eye;
    float l = std::sqrt(z.x*z.x + z.y*z.y + z.z*z.z);
    z = z / l;

    D3DXVECTOR3 x(up->y*z.z - up->z*z.y, up->z*z.x - up->x*z.z, up->x*z.y - up->y*z.x);
    l = std::sqrt(x.x*x.x + x.y*x.y + x.z*x.z);
    x = x / l;

    const D3DXVECTOR3 y(z.y*x.z - z.z*x.y, z.z*x.x - z.x*x.z, z.x*x.y - z.y*x.x);

    const float dx = -(x.x*eye->x + x.y*eye->y + x.z*eye->z);
    const float dy = -(y.x*eye->x + y.y*eye->y + y.z*eye->z);
    const float dz = -(z.x*eye->x + z.y*eye->y + z.z*eye->z);

    *o = D3DXMATRIX(x.x, y.x, z.x, 0,
                    x.y, y.y, z.y, 0,
                    x.z, y.z, z.z, 0,
                    dx,  dy,  dz,  1);
    return o;
}

inline D3DXMATRIX* D3DXMatrixLookAtRH(D3DXMATRIX* o, const D3DXVECTOR3* eye,
                                      const D3DXVECTOR3* at, const D3DXVECTOR3* up)
{
    const D3DXVECTOR3 flipped(2.0f*eye->x - at->x, 2.0f*eye->y - at->y, 2.0f*eye->z - at->z);
    return D3DXMatrixLookAtLH(o, eye, &flipped, up);
}

inline D3DXMATRIX* D3DXMatrixPerspectiveFovLH(D3DXMATRIX* o, float fovy, float aspect, float zn, float zf)
{
    const float yScale = 1.0f / std::tan(fovy * 0.5f);
    const float xScale = yScale / aspect;
    *o = D3DXMATRIX(xScale, 0, 0, 0,
                    0, yScale, 0, 0,
                    0, 0, zf/(zf-zn), 1,
                    0, 0, -zn*zf/(zf-zn), 0);
    return o;
}

inline D3DXMATRIX* D3DXMatrixPerspectiveOffCenterLH(D3DXMATRIX* o, float l, float r,
                                                    float b, float t, float zn, float zf)
{
    *o = D3DXMATRIX(2*zn/(r-l), 0, 0, 0,
                    0, 2*zn/(t-b), 0, 0,
                    (l+r)/(l-r), (t+b)/(b-t), zf/(zf-zn), 1,
                    0, 0, zn*zf/(zn-zf), 0);
    return o;
}

inline D3DXMATRIX* D3DXMatrixOrthoLH(D3DXMATRIX* o, float w, float h, float zn, float zf)
{
    *o = D3DXMATRIX(2/w, 0, 0, 0,
                    0, 2/h, 0, 0,
                    0, 0, 1/(zf-zn), 0,
                    0, 0, zn/(zn-zf), 1);
    return o;
}

inline D3DXMATRIX* D3DXMatrixOrthoOffCenterLH(D3DXMATRIX* o, float l, float r,
                                              float b, float t, float zn, float zf)
{
    *o = D3DXMATRIX(2/(r-l), 0, 0, 0,
                    0, 2/(t-b), 0, 0,
                    0, 0, 1/(zf-zn), 0,
                    (l+r)/(l-r), (t+b)/(b-t), zn/(zn-zf), 1);
    return o;
}

// ===========================================================================
// Vector operations
// ===========================================================================

inline D3DXVECTOR2* D3DXVec2Add(D3DXVECTOR2* o, const D3DXVECTOR2* a, const D3DXVECTOR2* b)
{ *o = D3DXVECTOR2(a->x+b->x, a->y+b->y); return o; }

inline D3DXVECTOR2* D3DXVec2Scale(D3DXVECTOR2* o, const D3DXVECTOR2* v, float s)
{ *o = D3DXVECTOR2(v->x*s, v->y*s); return o; }

inline D3DXVECTOR2* D3DXVec2Normalize(D3DXVECTOR2* o, const D3DXVECTOR2* v)
{
    const float l = std::sqrt(v->x*v->x + v->y*v->y);
    *o = (l > 0.0f) ? D3DXVECTOR2(v->x/l, v->y/l) : D3DXVECTOR2(0,0);
    return o;
}

inline D3DXVECTOR3* D3DXVec3Add(D3DXVECTOR3* o, const D3DXVECTOR3* a, const D3DXVECTOR3* b)
{ *o = D3DXVECTOR3(a->x+b->x, a->y+b->y, a->z+b->z); return o; }

inline D3DXVECTOR3* D3DXVec3Subtract(D3DXVECTOR3* o, const D3DXVECTOR3* a, const D3DXVECTOR3* b)
{ *o = D3DXVECTOR3(a->x-b->x, a->y-b->y, a->z-b->z); return o; }

inline D3DXVECTOR3* D3DXVec3Scale(D3DXVECTOR3* o, const D3DXVECTOR3* v, float s)
{ *o = D3DXVECTOR3(v->x*s, v->y*s, v->z*s); return o; }

inline float D3DXVec3Dot(const D3DXVECTOR3* a, const D3DXVECTOR3* b)
{ return a->x*b->x + a->y*b->y + a->z*b->z; }

inline float D3DXVec3Length(const D3DXVECTOR3* v)
{ return std::sqrt(v->x*v->x + v->y*v->y + v->z*v->z); }

inline D3DXVECTOR3* D3DXVec3Cross(D3DXVECTOR3* o, const D3DXVECTOR3* a, const D3DXVECTOR3* b)
{
    *o = D3DXVECTOR3(a->y*b->z - a->z*b->y,
                     a->z*b->x - a->x*b->z,
                     a->x*b->y - a->y*b->x);
    return o;
}

inline D3DXVECTOR3* D3DXVec3Normalize(D3DXVECTOR3* o, const D3DXVECTOR3* v)
{
    const float l = D3DXVec3Length(v);
    *o = (l > 0.0f) ? D3DXVECTOR3(v->x/l, v->y/l, v->z/l) : D3DXVECTOR3(0,0,0);
    return o;
}

inline D3DXVECTOR3* D3DXVec3Minimize(D3DXVECTOR3* o, const D3DXVECTOR3* a, const D3DXVECTOR3* b)
{
    *o = D3DXVECTOR3(a->x < b->x ? a->x : b->x,
                     a->y < b->y ? a->y : b->y,
                     a->z < b->z ? a->z : b->z);
    return o;
}

inline D3DXVECTOR3* D3DXVec3Maximize(D3DXVECTOR3* o, const D3DXVECTOR3* a, const D3DXVECTOR3* b)
{
    *o = D3DXVECTOR3(a->x > b->x ? a->x : b->x,
                     a->y > b->y ? a->y : b->y,
                     a->z > b->z ? a->z : b->z);
    return o;
}

inline D3DXVECTOR4* D3DXVec3Transform(D3DXVECTOR4* o, const D3DXVECTOR3* v, const D3DXMATRIX* mx)
{
    *o = D3DXVECTOR4(
        v->x*mx->_11 + v->y*mx->_21 + v->z*mx->_31 + mx->_41,
        v->x*mx->_12 + v->y*mx->_22 + v->z*mx->_32 + mx->_42,
        v->x*mx->_13 + v->y*mx->_23 + v->z*mx->_33 + mx->_43,
        v->x*mx->_14 + v->y*mx->_24 + v->z*mx->_34 + mx->_44);
    return o;
}

inline D3DXVECTOR3* D3DXVec3TransformCoord(D3DXVECTOR3* o, const D3DXVECTOR3* v, const D3DXMATRIX* mx)
{
    D3DXVECTOR4 t;
    D3DXVec3Transform(&t, v, mx);
    const float iw = (t.w != 0.0f) ? 1.0f / t.w : 1.0f;
    *o = D3DXVECTOR3(t.x*iw, t.y*iw, t.z*iw);
    return o;
}

inline D3DXVECTOR3* D3DXVec3TransformNormal(D3DXVECTOR3* o, const D3DXVECTOR3* v, const D3DXMATRIX* mx)
{
    *o = D3DXVECTOR3(
        v->x*mx->_11 + v->y*mx->_21 + v->z*mx->_31,
        v->x*mx->_12 + v->y*mx->_22 + v->z*mx->_32,
        v->x*mx->_13 + v->y*mx->_23 + v->z*mx->_33);
    return o;
}

inline D3DXVECTOR4* D3DXVec3TransformArray(D3DXVECTOR4* o, UINT ostride, const D3DXVECTOR3* v,
                                           UINT vstride, const D3DXMATRIX* mx, UINT n)
{
    for (UINT i = 0; i < n; ++i)
        D3DXVec3Transform(reinterpret_cast<D3DXVECTOR4*>(reinterpret_cast<BYTE*>(o) + i*ostride),
                          reinterpret_cast<const D3DXVECTOR3*>(reinterpret_cast<const BYTE*>(v) + i*vstride), mx);
    return o;
}

inline D3DXVECTOR3* D3DXVec3TransformCoordArray(D3DXVECTOR3* o, UINT ostride, const D3DXVECTOR3* v,
                                                UINT vstride, const D3DXMATRIX* mx, UINT n)
{
    for (UINT i = 0; i < n; ++i)
        D3DXVec3TransformCoord(reinterpret_cast<D3DXVECTOR3*>(reinterpret_cast<BYTE*>(o) + i*ostride),
                               reinterpret_cast<const D3DXVECTOR3*>(reinterpret_cast<const BYTE*>(v) + i*vstride), mx);
    return o;
}

inline D3DXVECTOR3* D3DXVec3Project(D3DXVECTOR3* o, const D3DXVECTOR3* v, const D3DVIEWPORT9* vp,
                                    const D3DXMATRIX* proj, const D3DXMATRIX* view, const D3DXMATRIX* world)
{
    D3DXMATRIX m = (*world) * (*view) * (*proj);
    D3DXVECTOR3 t;
    D3DXVec3TransformCoord(&t, v, &m);
    o->x = vp->X + (1.0f + t.x) * vp->Width  * 0.5f;
    o->y = vp->Y + (1.0f - t.y) * vp->Height * 0.5f;
    o->z = vp->MinZ + t.z * (vp->MaxZ - vp->MinZ);
    return o;
}

inline float D3DXVec4Dot(const D3DXVECTOR4* a, const D3DXVECTOR4* b)
{ return a->x*b->x + a->y*b->y + a->z*b->z + a->w*b->w; }

inline D3DXVECTOR4* D3DXVec4Scale(D3DXVECTOR4* o, const D3DXVECTOR4* v, float s)
{ *o = D3DXVECTOR4(v->x*s, v->y*s, v->z*s, v->w*s); return o; }

inline D3DXVECTOR4* D3DXVec4Lerp(D3DXVECTOR4* o, const D3DXVECTOR4* a, const D3DXVECTOR4* b, float t)
{
    *o = D3DXVECTOR4(a->x + t*(b->x-a->x), a->y + t*(b->y-a->y),
                     a->z + t*(b->z-a->z), a->w + t*(b->w-a->w));
    return o;
}

inline D3DXVECTOR4* D3DXVec4Normalize(D3DXVECTOR4* o, const D3DXVECTOR4* v)
{
    const float l = std::sqrt(v->x*v->x + v->y*v->y + v->z*v->z + v->w*v->w);
    *o = (l > 0.0f) ? D3DXVECTOR4(v->x/l, v->y/l, v->z/l, v->w/l) : D3DXVECTOR4(0,0,0,0);
    return o;
}

inline D3DXVECTOR4* D3DXVec4Transform(D3DXVECTOR4* o, const D3DXVECTOR4* v, const D3DXMATRIX* mx)
{
    *o = D3DXVECTOR4(
        v->x*mx->_11 + v->y*mx->_21 + v->z*mx->_31 + v->w*mx->_41,
        v->x*mx->_12 + v->y*mx->_22 + v->z*mx->_32 + v->w*mx->_42,
        v->x*mx->_13 + v->y*mx->_23 + v->z*mx->_33 + v->w*mx->_43,
        v->x*mx->_14 + v->y*mx->_24 + v->z*mx->_34 + v->w*mx->_44);
    return o;
}

// ===========================================================================
// Quaternion
// ===========================================================================

inline D3DXQUATERNION* D3DXQuaternionRotationYawPitchRoll(D3DXQUATERNION* o, float yaw, float pitch, float roll)
{
    const float hy = yaw*0.5f, hp = pitch*0.5f, hr = roll*0.5f;
    const float sy = std::sin(hy), cy = std::cos(hy);
    const float sp = std::sin(hp), cp = std::cos(hp);
    const float sr = std::sin(hr), cr = std::cos(hr);

    *o = D3DXQUATERNION(cy*sp*cr + sy*cp*sr,
                        sy*cp*cr - cy*sp*sr,
                        cy*cp*sr - sy*sp*cr,
                        cy*cp*cr + sy*sp*sr);
    return o;
}

inline D3DXQUATERNION* D3DXQuaternionRotationMatrix(D3DXQUATERNION* o, const D3DXMATRIX* m)
{
    const float trace = m->_11 + m->_22 + m->_33;
    if (trace > 0.0f)
    {
        const float k = std::sqrt(trace + 1.0f) * 2.0f;
        *o = D3DXQUATERNION((m->_23 - m->_32)/k, (m->_31 - m->_13)/k, (m->_12 - m->_21)/k, 0.25f*k);
    }
    else if (m->_11 > m->_22 && m->_11 > m->_33)
    {
        const float k = std::sqrt(1.0f + m->_11 - m->_22 - m->_33) * 2.0f;
        *o = D3DXQUATERNION(0.25f*k, (m->_21 + m->_12)/k, (m->_31 + m->_13)/k, (m->_23 - m->_32)/k);
    }
    else if (m->_22 > m->_33)
    {
        const float k = std::sqrt(1.0f + m->_22 - m->_11 - m->_33) * 2.0f;
        *o = D3DXQUATERNION((m->_21 + m->_12)/k, 0.25f*k, (m->_32 + m->_23)/k, (m->_31 - m->_13)/k);
    }
    else
    {
        const float k = std::sqrt(1.0f + m->_33 - m->_11 - m->_22) * 2.0f;
        *o = D3DXQUATERNION((m->_31 + m->_13)/k, (m->_32 + m->_23)/k, 0.25f*k, (m->_12 - m->_21)/k);
    }
    return o;
}

inline D3DXQUATERNION* D3DXQuaternionNormalize(D3DXQUATERNION* o, const D3DXQUATERNION* q)
{
    const float l = std::sqrt(q->x*q->x + q->y*q->y + q->z*q->z + q->w*q->w);
    *o = (l > 0.0f) ? D3DXQUATERNION(q->x/l, q->y/l, q->z/l, q->w/l) : D3DXQUATERNION(0,0,0,1);
    return o;
}

inline D3DXQUATERNION* D3DXQuaternionInverse(D3DXQUATERNION* o, const D3DXQUATERNION* q)
{
    const float n = q->x*q->x + q->y*q->y + q->z*q->z + q->w*q->w;
    if (n == 0.0f) { *o = D3DXQUATERNION(0,0,0,1); return o; }
    const float i = 1.0f / n;
    *o = D3DXQUATERNION(-q->x*i, -q->y*i, -q->z*i, q->w*i);
    return o;
}

inline D3DXQUATERNION* D3DXQuaternionSlerp(D3DXQUATERNION* o, const D3DXQUATERNION* a,
                                           const D3DXQUATERNION* b, float t)
{
    float dot = a->x*b->x + a->y*b->y + a->z*b->z + a->w*b->w;
    float sign = 1.0f;
    if (dot < 0.0f) { dot = -dot; sign = -1.0f; }

    float ka, kb;
    if (dot > 0.9995f)
    {
        ka = 1.0f - t;
        kb = t * sign;
    }
    else
    {
        const float theta = std::acos(dot);
        const float st = std::sin(theta);
        ka = std::sin((1.0f - t) * theta) / st;
        kb = std::sin(t * theta) / st * sign;
    }
    *o = D3DXQUATERNION(ka*a->x + kb*b->x, ka*a->y + kb*b->y,
                        ka*a->z + kb*b->z, ka*a->w + kb*b->w);
    return o;
}

// ===========================================================================
// Plane
// ===========================================================================

inline float D3DXPlaneDotCoord(const D3DXPLANE* p, const D3DXVECTOR3* v)
{ return p->a*v->x + p->b*v->y + p->c*v->z + p->d; }

inline float D3DXPlaneDotNormal(const D3DXPLANE* p, const D3DXVECTOR3* v)
{ return p->a*v->x + p->b*v->y + p->c*v->z; }

inline D3DXPLANE* D3DXPlaneFromPointNormal(D3DXPLANE* o, const D3DXVECTOR3* pt, const D3DXVECTOR3* n)
{
    *o = D3DXPLANE(n->x, n->y, n->z, -(n->x*pt->x + n->y*pt->y + n->z*pt->z));
    return o;
}

inline D3DXPLANE* D3DXPlaneFromPoints(D3DXPLANE* o, const D3DXVECTOR3* p1,
                                      const D3DXVECTOR3* p2, const D3DXVECTOR3* p3)
{
    const D3DXVECTOR3 e1 = *p2 - *p1;
    const D3DXVECTOR3 e2 = *p3 - *p1;
    D3DXVECTOR3 n;
    D3DXVec3Cross(&n, &e1, &e2);
    D3DXVec3Normalize(&n, &n);
    return D3DXPlaneFromPointNormal(o, p1, &n);
}

inline D3DXPLANE* D3DXPlaneTransform(D3DXPLANE* o, const D3DXPLANE* p, const D3DXMATRIX* mx)
{
    *o = D3DXPLANE(
        p->a*mx->_11 + p->b*mx->_21 + p->c*mx->_31 + p->d*mx->_41,
        p->a*mx->_12 + p->b*mx->_22 + p->c*mx->_32 + p->d*mx->_42,
        p->a*mx->_13 + p->b*mx->_23 + p->c*mx->_33 + p->d*mx->_43,
        p->a*mx->_14 + p->b*mx->_24 + p->c*mx->_34 + p->d*mx->_44);
    return o;
}

inline D3DXVECTOR3* D3DXPlaneIntersectLine(D3DXVECTOR3* o, const D3DXPLANE* p,
                                           const D3DXVECTOR3* v1, const D3DXVECTOR3* v2)
{
    const D3DXVECTOR3 dir = *v2 - *v1;
    const float den = p->a*dir.x + p->b*dir.y + p->c*dir.z;
    if (den == 0.0f) return nullptr;
    const float t = -(p->a*v1->x + p->b*v1->y + p->c*v1->z + p->d) / den;
    *o = D3DXVECTOR3(v1->x + dir.x*t, v1->y + dir.y*t, v1->z + dir.z*t);
    return o;
}

// ===========================================================================
// Image / texture / shader utilities
//
// STUBS. These wrapped D3DX's texture loading, format conversion and shader
// compilation, all of which need real replacements (stb_image + a modern shader
// compiler). They are declared so the codebase links; every one returns failure so
// callers take their error path rather than proceeding with garbage.
//
// See ../../../../DEPENDENCIES.md: stb_image is the chosen replacement for image
// loading, DirectXShaderCompiler/slang for shaders.
// ===========================================================================

enum D3DXIMAGE_FILEFORMAT
{
    D3DXIFF_BMP = 0,
    D3DXIFF_JPG = 1,
    D3DXIFF_TGA = 2,
    D3DXIFF_PNG = 3,
    D3DXIFF_DDS = 4,
    D3DXIFF_FORCE_DWORD = 0x7fffffff
};

struct D3DXIMAGE_INFO
{
    UINT                 Width;
    UINT                 Height;
    UINT                 Depth;
    UINT                 MipLevels;
    D3DFORMAT            Format;
    D3DRESOURCETYPE      ResourceType;
    D3DXIMAGE_FILEFORMAT ImageFileFormat;
};

struct D3DXMACRO
{
    LPCSTR Name;
    LPCSTR Definition;
};

// --- image info -----------------------------------------------------------

inline HRESULT D3DXGetImageInfoFromFileA(LPCSTR, D3DXIMAGE_INFO*)                 { return E_NOTIMPL; }
inline HRESULT D3DXGetImageInfoFromFileInMemory(LPCVOID, UINT, D3DXIMAGE_INFO*)   { return E_NOTIMPL; }
#define D3DXGetImageInfoFromFile D3DXGetImageInfoFromFileA

// --- texture creation -----------------------------------------------------

inline HRESULT D3DXCreateTextureFromFileA(LPDIRECT3DDEVICE9, LPCSTR, LPDIRECT3DTEXTURE9* t)
{ if (t) *t = nullptr; return E_NOTIMPL; }
#define D3DXCreateTextureFromFile D3DXCreateTextureFromFileA

inline HRESULT D3DXCreateTextureFromFileExA(LPDIRECT3DDEVICE9, LPCSTR, UINT, UINT, UINT, DWORD,
                                            D3DFORMAT, D3DPOOL, DWORD, DWORD, D3DCOLOR,
                                            D3DXIMAGE_INFO*, PALETTEENTRY*, LPDIRECT3DTEXTURE9* t)
{ if (t) *t = nullptr; return E_NOTIMPL; }
#define D3DXCreateTextureFromFileEx D3DXCreateTextureFromFileExA

inline HRESULT D3DXCreateTextureFromFileInMemoryEx(LPDIRECT3DDEVICE9, LPCVOID, UINT, UINT, UINT, UINT,
                                                   DWORD, D3DFORMAT, D3DPOOL, DWORD, DWORD, D3DCOLOR,
                                                   D3DXIMAGE_INFO*, PALETTEENTRY*, LPDIRECT3DTEXTURE9* t)
{ if (t) *t = nullptr; return E_NOTIMPL; }

inline HRESULT D3DXCreateCubeTextureFromFileInMemoryEx(LPDIRECT3DDEVICE9, LPCVOID, UINT, UINT, UINT,
                                                       DWORD, D3DFORMAT, D3DPOOL, DWORD, DWORD, D3DCOLOR,
                                                       D3DXIMAGE_INFO*, PALETTEENTRY*, LPDIRECT3DCUBETEXTURE9* t)
{ if (t) *t = nullptr; return E_NOTIMPL; }

inline HRESULT D3DXCreateVolumeTextureFromFileInMemoryEx(LPDIRECT3DDEVICE9, LPCVOID, UINT, UINT, UINT, UINT,
                                                         UINT, DWORD, D3DFORMAT, D3DPOOL, DWORD, DWORD, D3DCOLOR,
                                                         D3DXIMAGE_INFO*, PALETTEENTRY*, LPDIRECT3DVOLUMETEXTURE9* t)
{ if (t) *t = nullptr; return E_NOTIMPL; }

inline HRESULT D3DXCreateBox(LPDIRECT3DDEVICE9, FLOAT, FLOAT, FLOAT,
                             LPD3DXMESH* mesh, LPD3DXBUFFER* adjacency)
{ if (mesh) *mesh = nullptr; if (adjacency) *adjacency = nullptr; return E_NOTIMPL; }

inline HRESULT D3DXCreateCubeTexture(LPDIRECT3DDEVICE9, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
                                     LPDIRECT3DCUBETEXTURE9* t)
{ if (t) *t = nullptr; return E_NOTIMPL; }

// --- save -----------------------------------------------------------------

inline HRESULT D3DXSaveTextureToFileA(LPCSTR, D3DXIMAGE_FILEFORMAT, LPDIRECT3DBASETEXTURE9, const PALETTEENTRY*)
{ return E_NOTIMPL; }
#define D3DXSaveTextureToFile D3DXSaveTextureToFileA

inline HRESULT D3DXSaveTextureToFileInMemory(LPD3DXBUFFER* buf, D3DXIMAGE_FILEFORMAT,
                                             LPDIRECT3DBASETEXTURE9, const PALETTEENTRY*)
{ if (buf) *buf = nullptr; return E_NOTIMPL; }

inline HRESULT D3DXSaveSurfaceToFileInMemory(LPD3DXBUFFER* buf, D3DXIMAGE_FILEFORMAT,
                                             LPDIRECT3DSURFACE9, const PALETTEENTRY*, const RECT*)
{ if (buf) *buf = nullptr; return E_NOTIMPL; }

inline HRESULT D3DXSaveSurfaceToFileA(LPCSTR, D3DXIMAGE_FILEFORMAT, LPDIRECT3DSURFACE9,
                                      const PALETTEENTRY*, const RECT*)
{ return E_NOTIMPL; }
#define D3DXSaveSurfaceToFile D3DXSaveSurfaceToFileA

// --- surface --------------------------------------------------------------

inline HRESULT D3DXLoadSurfaceFromSurface(LPDIRECT3DSURFACE9, const PALETTEENTRY*, const RECT*,
                                          LPDIRECT3DSURFACE9, const PALETTEENTRY*, const RECT*,
                                          DWORD, D3DCOLOR)
{ return E_NOTIMPL; }

inline HRESULT D3DXLoadSurfaceFromFileInMemory(LPDIRECT3DSURFACE9, const PALETTEENTRY*, const RECT*,
                                               LPCVOID, UINT, const RECT*, DWORD, D3DCOLOR,
                                               D3DXIMAGE_INFO*)
{ return E_NOTIMPL; }

// --- vertex declaration ---------------------------------------------------

inline UINT D3DXGetDeclVertexSize(const D3DVERTEXELEMENT9* decl, DWORD stream)
{
    // Real implementation: walks the declaration and returns the largest
    // offset+size for the given stream. Needed by mesh setup, so implement
    // rather than stub.
    if (!decl) return 0;

    UINT size = 0;
    for (const D3DVERTEXELEMENT9* e = decl; e->Stream != 0xFF; ++e)
    {
        if (e->Stream != stream) continue;

        UINT typeSize = 0;
        switch (e->Type)
        {
        case D3DDECLTYPE_FLOAT1:    typeSize = 4;  break;
        case D3DDECLTYPE_FLOAT2:    typeSize = 8;  break;
        case D3DDECLTYPE_FLOAT3:    typeSize = 12; break;
        case D3DDECLTYPE_FLOAT4:    typeSize = 16; break;
        case D3DDECLTYPE_D3DCOLOR:  typeSize = 4;  break;
        case D3DDECLTYPE_UBYTE4:    typeSize = 4;  break;
        case D3DDECLTYPE_SHORT2:    typeSize = 4;  break;
        case D3DDECLTYPE_SHORT4:    typeSize = 8;  break;
        case D3DDECLTYPE_UBYTE4N:   typeSize = 4;  break;
        case D3DDECLTYPE_SHORT2N:   typeSize = 4;  break;
        case D3DDECLTYPE_SHORT4N:   typeSize = 8;  break;
        case D3DDECLTYPE_USHORT2N:  typeSize = 4;  break;
        case D3DDECLTYPE_USHORT4N:  typeSize = 8;  break;
        case D3DDECLTYPE_UDEC3:     typeSize = 4;  break;
        case D3DDECLTYPE_DEC3N:     typeSize = 4;  break;
        case D3DDECLTYPE_FLOAT16_2: typeSize = 4;  break;
        case D3DDECLTYPE_FLOAT16_4: typeSize = 8;  break;
        default:                    typeSize = 0;  break;
        }

        const UINT end = e->Offset + typeSize;
        if (end > size) size = end;
    }
    return size;
}

inline UINT D3DXGetDeclLength(const D3DVERTEXELEMENT9* decl)
{
    if (!decl) return 0;
    UINT n = 0;
    while (decl[n].Stream != 0xFF) ++n;
    return n;
}

// --- shader compilation ---------------------------------------------------

inline HRESULT D3DXCompileShader(LPCSTR, UINT, const D3DXMACRO*, LPD3DXINCLUDE, LPCSTR, LPCSTR,
                                 DWORD, LPD3DXBUFFER* code, LPD3DXBUFFER* errors,
                                 LPD3DXCONSTANTTABLE* table)
{
    if (code)   *code   = nullptr;
    if (errors) *errors = nullptr;
    if (table)  *table  = nullptr;
    return E_NOTIMPL;
}

inline HRESULT D3DXCompileShaderFromFileA(LPCSTR, const D3DXMACRO*, LPD3DXINCLUDE, LPCSTR, LPCSTR,
                                          DWORD, LPD3DXBUFFER* code, LPD3DXBUFFER* errors,
                                          LPD3DXCONSTANTTABLE* table)
{
    if (code)   *code   = nullptr;
    if (errors) *errors = nullptr;
    if (table)  *table  = nullptr;
    return E_NOTIMPL;
}
#define D3DXCompileShaderFromFile D3DXCompileShaderFromFileA

inline HRESULT D3DXDisassembleShader(const DWORD*, BOOL, LPCSTR, LPD3DXBUFFER* out)
{ if (out) *out = nullptr; return E_NOTIMPL; }

#endif // __WARZ_COMPAT_D3DX9_H
