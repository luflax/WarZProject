// SHIM: Scaleform GFx build configuration
//
// Replaces:  GFxConfig.h from Scaleform GFx (Autodesk, discontinued 2018)
// Status:    NO-OP configuration header.
// Later:     RmlUi (MIT) -- see DEPENDENCIES.md. Every screen must be re-authored;
//            nothing imports Flash.

#pragma once

#define GFC_NO_FXPLAYER_AS_MOVIECLIP    1
#define GFC_NO_GLYPH_CACHE              1
#define GFC_NO_VIDEO                    1
#define GFC_NO_SOUND                    1
#define GFC_NO_IME                      1
#define GFC_BUILD_DEBUG                 0
