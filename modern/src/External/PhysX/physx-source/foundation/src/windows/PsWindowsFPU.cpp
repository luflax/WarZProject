//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Copyright (c) 2008-2021 NVIDIA Corporation. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
#include "PsFPU.h"
#include "float.h"
#include "PsIntrinsics.h"

#if PX_X64 || PX_ARM || PX_A64
#define _MCW_ALL _MCW_DN | _MCW_EM | _MCW_RC
#else
#define _MCW_ALL _MCW_DN | _MCW_EM | _MCW_IC | _MCW_RC | _MCW_PC
#endif

// [PORT] 32-bit MinGW has no __control87_2.
//
// That function is specific to Microsoft's 32-bit CRT: it is the only entry point that
// reaches the x87 control word and MXCSR *separately*, which is exactly why PhysX uses
// it here and _controlfp_s everywhere else. MinGW-w64 declares _control87 and
// _controlfp_s but not __control87_2, and msvcrt.dll does not export it either -- so
// declaring it ourselves would only move the failure from compile time to link time.
//
// Rather than reimplement Microsoft's portable _MCW_*/_CW_* bit encoding on top of the
// hardware, save and restore the two control registers directly. mControlWords is
// private to FPUGuard and is only ever written by the constructor and read by the
// destructor, so what it holds is ours to choose; here it holds raw register values.
//
// The two constants are what _CW_DEFAULT | _DN_FLUSH resolves to on x86:
//   x87 CW 0x027F -- all six exceptions masked, round to nearest, 53-bit precision
//   MXCSR  0x9FC0 -- all six exceptions masked (0x1F80), FTZ (0x8000), DAZ (0x0040)
// which is the state the MSVC build puts the units in. DAZ is set unconditionally, as
// the CRT call it replaces did; every SSE2 part except the earliest Pentium 4 steppings
// supports it, and SSE2 is already this build's floor.
#if !(PX_X64 || PX_ARM || PX_A64) && PX_GCC_FAMILY
#include <xmmintrin.h>

namespace
{
const uint32_t PX_MINGW_X87_DEFAULT = 0x027F;
const uint32_t PX_MINGW_MXCSR_DEFAULT = 0x9FC0;

PX_FORCE_INLINE uint32_t getX87ControlWord()
{
	uint16_t cw;
	__asm__ __volatile__("fnstcw %0" : "=m"(cw));
	return cw;
}

PX_FORCE_INLINE void setX87ControlWord(uint32_t value)
{
	uint16_t cw = uint16_t(value);
	__asm__ __volatile__("fldcw %0" : : "m"(cw));
}
}
#endif

physx::shdfnd::FPUGuard::FPUGuard()
{
// default plus FTZ and DAZ
#if PX_X64 || PX_ARM || PX_A64
	// query current control word state
	_controlfp_s(mControlWords, 0, 0);

	// set both x87 and sse units to default + DAZ
	unsigned int cw;
	_controlfp_s(&cw, _CW_DEFAULT | _DN_FLUSH, _MCW_ALL);
#elif PX_GCC_FAMILY
	mControlWords[0] = getX87ControlWord();
	mControlWords[1] = _mm_getcsr();

	setX87ControlWord(PX_MINGW_X87_DEFAULT);
	_mm_setcsr(PX_MINGW_MXCSR_DEFAULT);
#else
	// query current control word state
	__control87_2(0, 0, mControlWords, mControlWords + 1);

	// set both x87 and sse units to default + DAZ
	unsigned int x87, sse;
	__control87_2(_CW_DEFAULT | _DN_FLUSH, _MCW_ALL, &x87, &sse);
#endif
}

physx::shdfnd::FPUGuard::~FPUGuard()
{
	_clearfp();

#if PX_X64 || PX_ARM || PX_A64
	// reset FP state
	unsigned int cw;
	_controlfp_s(&cw, *mControlWords, _MCW_ALL);
#elif PX_GCC_FAMILY
	setX87ControlWord(mControlWords[0]);
	_mm_setcsr(mControlWords[1]);
#else

	// reset FP state
	unsigned int x87, sse;
	__control87_2(mControlWords[0], _MCW_ALL, &x87, 0);
	__control87_2(mControlWords[1], _MCW_ALL, 0, &sse);
#endif
}

void physx::shdfnd::enableFPExceptions()
{
	// clear any pending exceptions
	_clearfp();

	// enable all fp exceptions except inexact and underflow (common, benign)
	_controlfp_s(NULL, uint32_t(~_MCW_EM) | _EM_INEXACT | _EM_UNDERFLOW, _MCW_EM);
}

void physx::shdfnd::disableFPExceptions()
{
	_controlfp_s(NULL, _MCW_EM, _MCW_EM);
}
