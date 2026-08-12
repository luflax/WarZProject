// SHIM: AMD/ATI Crossfire detection (atimgpud)
//
// Replaces:  atimgpud.h from AMD's multi-GPU detection library, linked in the original
//            as atimgpud_s_x86.lib. Redistributed under AMD's SDK terms, not a
//            permissive licence, and the .lib is not in this drop.
//            See ../../../DEPENDENCIES.md.
// Status:    NO-OP. Crossfire is never detected.
//
// The entire surface is one function. r3dRender.CPP calls it once, to report the GPU
// count and seed gSLI_Crossfire_NumGPUs:
//
//     int numATIGPUs = AtiMultiGPUAdapters();
//     ...
//     gSLI_Crossfire_NumGPUs = std::max(1, std::max(numATIGPUs, numNVGPUs));
//
// Returning 1 is both honest and safe: it means "one GPU", which is what the caller
// already clamps to, so the single-GPU path is taken exactly as it would be on any
// non-Crossfire machine. Returning 0 would be misleading -- there is always at least
// one adapter -- and the max() above would mask it anyway.
//
// Clean-room declaration derived from the single call site in
// src/Eternity/Source/r3dRender.CPP. No code originates from AMD's SDK.

#ifndef __WARZ_COMPAT_ATIMGPUD_H
#define __WARZ_COMPAT_ATIMGPUD_H

inline int AtiMultiGPUAdapters()
{
    return 1;
}

#endif // __WARZ_COMPAT_ATIMGPUD_H
