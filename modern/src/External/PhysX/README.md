# PhysX 4.1 (vendored)

**Source:** https://github.com/NVIDIAGameWorks/PhysX, branch `4.1`
**Licence:** BSD-3-Clause — see `LICENSE.md`. No commercial agreement required.

Built from source, as 15 static libraries, for **i686-w64-mingw32** — a configuration
NVIDIA does not ship and does not test. See [`../../../cmake/BuildPhysX.cmake`](../../../cmake/BuildPhysX.cmake)
for how, and the [MinGW port notes](#the-mingw-i686-port) below for what had to change.

## Layout

| Directory | Origin | Provides |
|---|---|---|
| `physx-include/` | `physx/include` | `PxPhysicsAPI.h`, `characterkinematic/`, `extensions/`, `vehicle/`, `geometry/`, `cooking/`, `common/` |
| `pxshared-include/` | `pxshared/include` | `foundation/` — `PxAllocatorCallback.h`, `PxVec3.h`, `PxTransform.h`, … |
| `physx-source/` | `physx/source` | the SDK itself — 415 `.cpp`, of which **403 are built** |
| `include/` | hand-written | `PxForwardDecls.h`, kept for `DISABLE_PHYSX` builds that want no SDK at all |
| `compat/` | hand-written | `Px3xCompat.h` — the 3.x → 4.1 bridge |

The two include roots are unchanged from upstream, byte for byte, apart from the
`PxPreprocessor.h` fix noted below; `physx-source/` carries five other edits, all marked
`[PORT]`.

PhysX 4 deliberately splits foundation across `pxshared` and `physx`; both roots must be
on the include path, which is how PhysX itself is consumed.

The 12 sources not built are the ones that do not apply: 10 under `foundation/src/unix`,
`PsUWPThread.cpp`, and `physx/src/device/linux`. That accounting is checked rather than
asserted — `tools/gen_physx_sources.py --audit` reports every file on disk that no module
claims.

## Why 4.1 and not 3.4 or 5.x

- **3.4** is what this codebase was written against, but its source was published under
  NVIDIA's GameWorks terms, which are *not* clean BSD.
- **4.1** is the earliest BSD-3 release and the closest API to 3.4, so it minimises
  porting work.
- **5.x** diverges further and is a bigger jump for no Phase 1 benefit.

Jolt (MIT) remains the eventual target — see `../../../../DEPENDENCIES.md` and
`../../../../PERFORMANCE-OPTIMIZATION-PLAN.md`. PhysX 4.1 is the interim that unblocks
the build.

## Known 3.x → 4.1 breaks in this codebase

| 3.x | 4.1 |
|---|---|
| `#include "RepX/RepX.h"` | removed; serialization is now `extensions/PxRepXSerializer.h` |
| `CharacterKinematic/` | directory is lowercase `characterkinematic/` |
| `PxSceneQuery*` | renamed to `PxQuery*` (compat aliases provided) |

## The MinGW-i686 port

NVIDIA builds PhysX for Windows with MSVC and for Linux with GCC. Windows *with* GCC is
neither, and their build system cannot produce it: `physx/source/compiler/cmake` aborts
unless `generate_projects` has set `PHYSX_ROOT_DIR` and packman has fetched a
`CMakeModules` package that is not in the repository, and the flags underneath are
`/arch:SSE2 /d2Zi+ /GS- /GR- /fp:fast`. So the build is ours; only the source lists are
theirs, extracted by `tools/gen_physx_sources.py` from their module files.

The combination is legitimate rather than forced — `PxPreprocessor.h` resolves `_WIN32`
plus `__GNUC__` to `PX_WIN32 && PX_GCC` with no complaint — but nobody upstream compiles
it, so six things had to change. Each is marked `[PORT]` at the site.

| What | Where | Why |
|---|---|---|
| `__declspec(align(N))` dropped | `PxPreprocessor.h` | **The one that mattered.** `PX_ALIGN` keyed off `PX_MICROSOFT_FAMILY`, which is a *platform* test and true for MinGW. GCC does not implement `__declspec(align)`; it warns and discards it. `sizeof(Articulation)` fell from 192 to 184 and structures reached by aligned SSE loads would have been misaligned at runtime. Only caught because PhysX size-checks that one class at compile time. GCC is now tested first, and `-Wattributes` is re-enabled after `-w` so a dropped attribute can never be silent again. |
| `__control87_2` absent | `PsWindowsFPU.cpp` | Microsoft's 32-bit CRT only, and not exported by `msvcrt.dll` either. `FPUGuard` now saves and restores the x87 control word and MXCSR directly. |
| `__try` / `__except` | `PsWindowsThread.cpp` | MSVC extension. It guards a `RaiseException` that only an attached debugger catches — without a handler it would kill the process every time no debugger is present. Skipped under GCC; threads go unnamed in native debuggers. |
| `__m128` has no members | `PsWindowsInlineAoS.h` | `m128_u16`/`m128_u32` are Microsoft's extension. Three functions use them; they now pun through a local union. The vector *typedefs* are deliberately left alone — see the comment at the site. |
| `typeid` under `-fno-rtti` | `PxProfileAllocatorWrapper.h` | MSVC allows `typeid` on a non-polymorphic type under `/GR-`; GCC rejects every `typeid` under `-fno-rtti`. Now uses `__PRETTY_FUNCTION__`, exactly as the GCC platforms upstream already do. |
| `Winsock2.h`, `VersionHelpers.h` | 2 files | MinGW spells them lowercase, and Linux is case-sensitive. |

Two further things are configuration rather than source:

- **C++17, not the project's C++20.** C++20 made it ill-formed to name a constructor with
  template arguments, which is exactly what PhysX's own `PX_NOCOPY` macro generates for
  every class template. Safe here because no header under `physx-include/` or
  `pxshared-include/` tests `__cplusplus`, so both sides of the boundary see the same
  tokens — checked, not assumed. `BuildPhysX.cmake` records the full argument.
- **The ABI defines propagate; the build flags do not.** `DISABLE_CUDA_PHYSX` decides
  `PX_SUPPORT_GPU_PHYSX`, which changes the body of the inline `PxSceneDesc::isValid()`
  and the decoration on every GPU entry point — compile the library one way and the game
  the other and you have two definitions of one inline function, of which the linker
  silently keeps one. `-fno-rtti` and `-fno-exceptions` must equally *not* escape; they
  did at first, and broke `try`/`catch` in `AsyncFuncs.cpp`. Hence two interface targets,
  `physx_abi` (PUBLIC) and `physx_flags` (PRIVATE). `GameEngine` links `physx_abi` even
  though it needs no PhysX *symbols*, because it compiles `PxPhysicsAPI.h`.
