//=========================================================================
//  Scaleform GFx -> RmlUi integration
//
//  REPLACES: Scaleform GFx (Autodesk, discontinued 2018). See DEPENDENCIES.md.
//
//  r3dScaleformMovie keeps its name and its public API -- 37 files reference it --
//  but is now backed by RmlUi:
//
//      Scaleform                       RmlUi
//      ---------------------------     ------------------------------------
//      GFx::MovieDef (parsed .swf)     (none; documents load into a context)
//      GFx::Movie                      Rml::Context + Rml::ElementDocument
//      Movie::SetVariable(path, v)     element lookup by id -> inner RML / attribute
//      Movie::Invoke(method, args)     dispatch a named event to the document
//      Movie::Advance + Display        Context::Update() + Context::Render()
//      GFx::Value                      Rml::Variant
//
//  WHAT THIS DOES NOT DO: supply content. Every screen is a .swf and nothing
//  imports Flash into RML, so screens must be re-authored. Until then documents
//  fail to load, SetVariable/Invoke find no elements, and the UI draws nothing --
//  which is the Phase 1 contract.
//
//  ALSO NOT DONE: the Rml::RenderInterface and Rml::SystemInterface backed by
//  r3dRenderer. Those are the seams marked below, and they are Milestone C work.
//=========================================================================

#pragma once

namespace Rml
{
    class Context;
    class ElementDocument;
}

// Brings up RmlUi::Core once, installs the render/system interfaces, and creates
// the shared context. Called from r3dScaleformGfxCreate().
bool  r3dRmlUiInitialise();
void  r3dRmlUiShutdown();

// The single shared context all movies live in. RmlUi contexts are cheap but the
// original design had one screen active at a time, so one context matches it.
Rml::Context* r3dRmlUiGetContext();

// Resolution changes; mirrors r3dScaleformReset().
void  r3dRmlUiSetDimensions(int w, int h);
