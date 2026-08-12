// Shared declarations for the compat-layer translation units in this directory.
//
// These files implement shims that were previously header-only no-ops (CrashRpt,
// Chilkat, D3DX's imaging and shader-compilation halves). They are compiled INTO the
// Eternity target rather than into a library of their own, which is what lets them
// call r3dOutToLog without creating a dependency cycle -- Eternity is below everything
// that uses these APIs, so there is nowhere else for the code to sit that can both
// reach the log and be reached by the callers.
//
// They deliberately do NOT include r3dPCH.h or r3d.h. r3dPCH.h defines NOMINMAX and
// STRICT, selects a Windows version and in debug configurations redefines `new`; these
// files talk to winhttp.h, dbghelp.h and d3d9.h and are better off with the plain
// platform headers. cmake/Pch.cmake lists them as PCH exclusions for the same reason
// the seven pre-existing exclusions are listed.
//
// The one thing they do want from the engine is its log, so it is redeclared here.
// src/Eternity/Include/r3dAssert.h already sets that precedent.

#pragma once

extern bool r3dOutToLog(const char* Str, ...);
