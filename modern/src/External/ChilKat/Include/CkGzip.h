// SHIM: Chilkat (CkGzip)
//
// Replaces:  Chilkat's gzip component (commercial, paid). See ../../../../DEPENDENCIES.md.
// Status:    NO-OP. Decompression fails, so gzipped backend responses are rejected
//            rather than silently treated as empty.
//
// This is the header Chilkat actually ships; CkGZip.h beside it is the same shim under
// the spelling this port first used.
//
// Clean-room declarations derived from call sites in Sources/Backend/WOBackendAPI.cpp.
// No code originates from the Chilkat SDK.
#pragma once
#include "CkShimCommon.h"
