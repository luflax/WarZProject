// COMPAT: Scaleform GFx -> RmlUi
//
// Replaces:  Src/Render/D3D9/D3D9_Texture.h from Scaleform GFx (discontinued 2018).
// Backed by: nothing yet -- see the note below.
//
// Scaleform's D3D9 renderer wrapped a D3D9 texture so a render target could be bound
// into a movie as an image resource. Exactly one consumer survives:
// FrontEndWarZ re-binds the character-preview render target after a device reset
// (UI/FrontEndWarZ.cpp:937). RmlUi has the same capability, but it goes through the
// RenderInterface's texture handles rather than a renderer-owned texture object, so
// this type has no direct RmlUi counterpart -- it is a seam, not a translation.
//
// The type is kept because FrontEndWarZ.h stores one by pointer and GFx.h
// forward-declares it. Wiring it up is part of the RmlUi RenderInterface work
// (see modern/src/GameEngine/RmlUiIntegration/), which is still stubbed.
//
// Clean-room declaration derived from the two call sites. No code originates from
// the Scaleform SDK.

#pragma once

#include "../../../../GFx.h"

namespace Scaleform {
namespace Render {
namespace D3D9 {

// Bound to an Rml texture handle once the RenderInterface is implemented.
class Texture
{
public:
    virtual ~Texture() = default;

    // [PORT] no-op: the render target is not bound into the UI while the RmlUi
    // RenderInterface is stubbed. Returns success so callers keep their flow.
    bool Initialize(void* /*pD3D9Texture*/) { return true; }

    void*    pTexture = nullptr;   // LPDIRECT3DTEXTURE9, kept opaque
    unsigned Width    = 0;
    unsigned Height   = 0;
};

} // namespace D3D9
} // namespace Render
} // namespace Scaleform
