# PhysX 4.1 (vendored)

**Source:** https://github.com/NVIDIAGameWorks/PhysX, branch `4.1`
**Licence:** BSD-3-Clause — see `LICENSE.md`. No commercial agreement required.

Headers only. Linking against the compiled libraries is Milestone B work; Phase 1
targets a clean compile.

## Layout

| Directory | Origin | Provides |
|---|---|---|
| `physx-include/` | `physx/include` | `PxPhysicsAPI.h`, `characterkinematic/`, `extensions/`, `vehicle/`, `geometry/`, `cooking/`, `common/` |
| `pxshared-include/` | `pxshared/include` | `foundation/` — `PxAllocatorCallback.h`, `PxVec3.h`, `PxTransform.h`, … |
| `include/` | hand-written | `PxForwardDecls.h`, kept for `DISABLE_PHYSX` builds that want no SDK at all |

PhysX 4 deliberately splits foundation across `pxshared` and `physx`; both roots must be
on the include path, which is how PhysX itself is consumed.

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
