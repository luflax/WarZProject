// COMPAT LAYER: D3DX9 -> DirectXMath
//
// Replaces:  D3DX9 from the DirectX SDK (June 2010)
// Why:       D3DX was REMOVED from the Windows SDK. The last shipping version was
//            the June 2010 DXSDK, which is deprecated and cannot be relied on.
// Status:    FUNCTIONAL — this is real code, not a no-op.
//
// D3DXMATRIX is used pervasively. GameObject::UpdateTransform (GameObj.h:305-318)
// builds every object's world matrix through D3DXMatrixTranslation /
// D3DXMatrixScaling and operator*. A no-op here yields a build that links and
// transforms nothing correctly, so this layer must preserve semantics exactly.
//
// Row-major, row-vector convention (v * M), matching D3DX. DirectXMath uses the
// same convention, so the mapping is direct.
//
// This is a clean-room reimplementation derived from call sites in this codebase.
// No code originates from the DirectX SDK headers.
//
// COVERAGE IS INCOMPLETE BY DESIGN. Add symbols as the compiler demands them —
// see ../../README.md, "How these get built out".

#pragma once

#include <DirectXMath.h>
#include <cmath>

// ---------------------------------------------------------------------------
// D3DXVECTOR2 / 3 / 4
// ---------------------------------------------------------------------------

struct D3DXVECTOR2 : public DirectX::XMFLOAT2
{
    D3DXVECTOR2() = default;
    D3DXVECTOR2(float x_, float y_) : DirectX::XMFLOAT2(x_, y_) {}
};

struct D3DXVECTOR3 : public DirectX::XMFLOAT3
{
    D3DXVECTOR3() = default;
    D3DXVECTOR3(float x_, float y_, float z_) : DirectX::XMFLOAT3(x_, y_, z_) {}
};

struct D3DXVECTOR4 : public DirectX::XMFLOAT4
{
    D3DXVECTOR4() = default;
    D3DXVECTOR4(float x_, float y_, float z_, float w_) : DirectX::XMFLOAT4(x_, y_, z_, w_) {}
};

// ---------------------------------------------------------------------------
// D3DXMATRIX
//
// The original exposes both m[i][j] and named _11.._44 members through an
// anonymous union, and the codebase uses BOTH forms (e.g. ShadowExtrusionData
// touches ToExtrusionBox.m[3][0] directly in GameObj.h:588-590). Preserve both.
// ---------------------------------------------------------------------------

struct D3DXMATRIX
{
    union
    {
        struct
        {
            float _11, _12, _13, _14;
            float _21, _22, _23, _24;
            float _31, _32, _33, _34;
            float _41, _42, _43, _44;
        };
        float m[4][4];
    };

    D3DXMATRIX() = default;

    D3DXMATRIX(float m11, float m12, float m13, float m14,
               float m21, float m22, float m23, float m24,
               float m31, float m32, float m33, float m34,
               float m41, float m42, float m43, float m44)
        : _11(m11), _12(m12), _13(m13), _14(m14)
        , _21(m21), _22(m22), _23(m23), _24(m24)
        , _31(m31), _32(m32), _33(m33), _34(m34)
        , _41(m41), _42(m42), _43(m43), _44(m44)
    {}

    explicit D3DXMATRIX(const DirectX::XMMATRIX& xm)
    {
        DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(this), xm);
    }

    DirectX::XMMATRIX xm() const
    {
        return DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(this));
    }

    float&       operator()(unsigned r, unsigned c)       { return m[r][c]; }
    float const& operator()(unsigned r, unsigned c) const { return m[r][c]; }

    D3DXMATRIX operator*(const D3DXMATRIX& rhs) const
    {
        return D3DXMATRIX(DirectX::XMMatrixMultiply(xm(), rhs.xm()));
    }

    D3DXMATRIX& operator*=(const D3DXMATRIX& rhs)
    {
        *this = *this * rhs;
        return *this;
    }

    operator float*()             { return &_11; }
    operator const float*() const { return &_11; }
};

// ---------------------------------------------------------------------------
// D3DXQUATERNION / D3DXPLANE
// ---------------------------------------------------------------------------

struct D3DXQUATERNION : public DirectX::XMFLOAT4
{
    D3DXQUATERNION() = default;
    D3DXQUATERNION(float x_, float y_, float z_, float w_) : DirectX::XMFLOAT4(x_, y_, z_, w_) {}
};

struct D3DXPLANE : public DirectX::XMFLOAT4
{
    D3DXPLANE() = default;
    D3DXPLANE(float a_, float b_, float c_, float d_) : DirectX::XMFLOAT4(a_, b_, c_, d_) {}
};

// ---------------------------------------------------------------------------
// Matrix construction
// ---------------------------------------------------------------------------

inline D3DXMATRIX* D3DXMatrixIdentity(D3DXMATRIX* out)
{
    *out = D3DXMATRIX(DirectX::XMMatrixIdentity());
    return out;
}

inline D3DXMATRIX* D3DXMatrixTranslation(D3DXMATRIX* out, float x, float y, float z)
{
    *out = D3DXMATRIX(DirectX::XMMatrixTranslation(x, y, z));
    return out;
}

inline D3DXMATRIX* D3DXMatrixScaling(D3DXMATRIX* out, float sx, float sy, float sz)
{
    *out = D3DXMATRIX(DirectX::XMMatrixScaling(sx, sy, sz));
    return out;
}

inline D3DXMATRIX* D3DXMatrixRotationX(D3DXMATRIX* out, float angle)
{
    *out = D3DXMATRIX(DirectX::XMMatrixRotationX(angle));
    return out;
}

inline D3DXMATRIX* D3DXMatrixRotationY(D3DXMATRIX* out, float angle)
{
    *out = D3DXMATRIX(DirectX::XMMatrixRotationY(angle));
    return out;
}

inline D3DXMATRIX* D3DXMatrixRotationZ(D3DXMATRIX* out, float angle)
{
    *out = D3DXMATRIX(DirectX::XMMatrixRotationZ(angle));
    return out;
}

inline D3DXMATRIX* D3DXMatrixRotationYawPitchRoll(D3DXMATRIX* out, float yaw, float pitch, float roll)
{
    *out = D3DXMATRIX(DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, roll));
    return out;
}

inline D3DXMATRIX* D3DXMatrixRotationAxis(D3DXMATRIX* out, const D3DXVECTOR3* axis, float angle)
{
    const DirectX::XMVECTOR a = DirectX::XMLoadFloat3(axis);
    *out = D3DXMATRIX(DirectX::XMMatrixRotationAxis(a, angle));
    return out;
}

// ---------------------------------------------------------------------------
// Matrix operations
// ---------------------------------------------------------------------------

inline D3DXMATRIX* D3DXMatrixMultiply(D3DXMATRIX* out, const D3DXMATRIX* a, const D3DXMATRIX* b)
{
    *out = D3DXMATRIX(DirectX::XMMatrixMultiply(a->xm(), b->xm()));
    return out;
}

inline D3DXMATRIX* D3DXMatrixTranspose(D3DXMATRIX* out, const D3DXMATRIX* in)
{
    *out = D3DXMATRIX(DirectX::XMMatrixTranspose(in->xm()));
    return out;
}

inline D3DXMATRIX* D3DXMatrixInverse(D3DXMATRIX* out, float* determinant, const D3DXMATRIX* in)
{
    DirectX::XMVECTOR det;
    const DirectX::XMMATRIX inv = DirectX::XMMatrixInverse(&det, in->xm());
    if (determinant)
        *determinant = DirectX::XMVectorGetX(det);
    *out = D3DXMATRIX(inv);
    return out;
}

// ---------------------------------------------------------------------------
// View / projection
// ---------------------------------------------------------------------------

inline D3DXMATRIX* D3DXMatrixLookAtLH(D3DXMATRIX* out, const D3DXVECTOR3* eye,
                                      const D3DXVECTOR3* at, const D3DXVECTOR3* up)
{
    *out = D3DXMATRIX(DirectX::XMMatrixLookAtLH(
        DirectX::XMLoadFloat3(eye), DirectX::XMLoadFloat3(at), DirectX::XMLoadFloat3(up)));
    return out;
}

inline D3DXMATRIX* D3DXMatrixPerspectiveFovLH(D3DXMATRIX* out, float fovy, float aspect,
                                              float zn, float zf)
{
    *out = D3DXMATRIX(DirectX::XMMatrixPerspectiveFovLH(fovy, aspect, zn, zf));
    return out;
}

inline D3DXMATRIX* D3DXMatrixOrthoLH(D3DXMATRIX* out, float w, float h, float zn, float zf)
{
    *out = D3DXMATRIX(DirectX::XMMatrixOrthographicLH(w, h, zn, zf));
    return out;
}

inline D3DXMATRIX* D3DXMatrixOrthoOffCenterLH(D3DXMATRIX* out, float l, float r,
                                              float b, float t, float zn, float zf)
{
    *out = D3DXMATRIX(DirectX::XMMatrixOrthographicOffCenterLH(l, r, b, t, zn, zf));
    return out;
}

// ---------------------------------------------------------------------------
// Vector transforms
// ---------------------------------------------------------------------------

inline D3DXVECTOR4* D3DXVec3Transform(D3DXVECTOR4* out, const D3DXVECTOR3* v, const D3DXMATRIX* mtx)
{
    const DirectX::XMVECTOR r = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(v), mtx->xm());
    DirectX::XMStoreFloat4(out, r);
    return out;
}

inline D3DXVECTOR3* D3DXVec3TransformCoord(D3DXVECTOR3* out, const D3DXVECTOR3* v, const D3DXMATRIX* mtx)
{
    const DirectX::XMVECTOR r = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(v), mtx->xm());
    DirectX::XMStoreFloat3(out, r);
    return out;
}

inline D3DXVECTOR3* D3DXVec3TransformNormal(D3DXVECTOR3* out, const D3DXVECTOR3* v, const D3DXMATRIX* mtx)
{
    const DirectX::XMVECTOR r = DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(v), mtx->xm());
    DirectX::XMStoreFloat3(out, r);
    return out;
}

inline D3DXVECTOR3* D3DXVec3Normalize(D3DXVECTOR3* out, const D3DXVECTOR3* v)
{
    DirectX::XMStoreFloat3(out, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(v)));
    return out;
}

inline D3DXVECTOR3* D3DXVec3Cross(D3DXVECTOR3* out, const D3DXVECTOR3* a, const D3DXVECTOR3* b)
{
    DirectX::XMStoreFloat3(out,
        DirectX::XMVector3Cross(DirectX::XMLoadFloat3(a), DirectX::XMLoadFloat3(b)));
    return out;
}

inline float D3DXVec3Dot(const D3DXVECTOR3* a, const D3DXVECTOR3* b)
{
    return DirectX::XMVectorGetX(
        DirectX::XMVector3Dot(DirectX::XMLoadFloat3(a), DirectX::XMLoadFloat3(b)));
}

inline float D3DXVec3Length(const D3DXVECTOR3* v)
{
    return DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(v)));
}

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#ifndef D3DX_PI
#define D3DX_PI    (3.14159265358979323846f)
#endif

#ifndef D3DXToRadian
#define D3DXToRadian(deg) ((deg) * (D3DX_PI / 180.0f))
#endif

#ifndef D3DXToDegree
#define D3DXToDegree(rad) ((rad) * (180.0f / D3DX_PI))
#endif
