# `src/External/` — vendored libraries and dependency shims

This directory sits at **the exact path the original source already includes from**, which
is what makes the whole Phase 1 approach work.

The codebase is full of deep relative includes:

```cpp
#include "../../External/fmod/fmod_event.hpp"
#include "../../../External/PunkBuster/pbcommon.h"
#include "CkHttp.h"          // resolved via AdditionalIncludeDirectories
```

Because the replacements live here under the original directory names, **most dependency
substitution requires zero source edits**. The include resolves to a shim that declares
the same API and does nothing.

---

## The three kinds of entry here

### 1. Real, vendored (permissive licence)

| Directory | Library | Licence | Note |
|---|---|---|---|
| `RakNet/` | RakNet | BSD-2 | Copied from the original drop — the one real dependency present |
| `pugiXML/` | pugixml | MIT | Fetched by CMake; absent from the original drop |

### 2. Functional compatibility layers

Shims that **must actually work**, because the code depends on their behaviour.

| Directory | Replaces | Backed by |
|---|---|---|
| `dxsdk/` | D3DX (removed from the Windows SDK) | **DirectXMath** |

`D3DXMATRIX` and friends are used pervasively — `GameObject::UpdateTransform`
(`GameObj.h:305-318`) builds every object's world matrix through them. A no-op here
produces a build that links and renders nothing correctly. This layer is real code.

### 3. No-op shims

Declare the API surface, do nothing at runtime. These exist purely so the compiler and
linker are satisfied.

| Directory | Replaces | Runtime behaviour |
|---|---|---|
| `Scaleform3/` | Scaleform GFx | UI does not render |
| `fmod/` | FMOD Ex | Silence |
| `ChilKat/` | Chilkat HTTP | All requests fail |
| `ts3_sdk_3/` | TeamSpeak 3 SDK | No VOIP |
| `PhysX/` | PhysX 3.x | Inert physics (until PhysX 4.1 is vendored) |
| `CrashRpt/` | CrashRpt | No crash reports |
| `Steam/` | Steamworks | Not on Steam |
| `GameBlocks/` | FairFight | No anti-cheat |

---

## Shim contract

A shim must:

1. **Declare every symbol the source references** — types, functions, constants, enums
2. **Be honest at runtime** — return failure or a null handle; never fake success in a way
   that makes calling code proceed down a path it cannot complete
3. **Compile under `/permissive-` and C++20**
4. **Carry a `// SHIM:` header comment** naming what it replaces and what it does not do

A shim must **not**:

- Contain any code copied from the original commercial SDK — these are clean-room
  declarations derived from call sites, not from vendor headers
- Silently succeed where the real library would do meaningful work

---

## How these get built out

Do not try to write these ahead of the compiler. The honest workflow is:

1. Build
2. Read the first missing-symbol error
3. Add exactly that symbol to the relevant shim
4. Repeat

The API surface these dependencies actually expose to this codebase is far smaller than
their full documentation, and only the compiler knows where the boundary is. The stubs
committed here are starting points covering known call sites, not complete surfaces.

---

## Replacing a shim with a real library

Later phases substitute real implementations. The order that minimises risk:

| Order | Shim | Replacement | Why this order |
|---|---|---|---|
| 1 | `ChilKat/` | libcurl | Small, self-contained, unblocks backend work |
| 2 | `PhysX/` | PhysX 4.1 → Jolt | Gameplay depends on it; large but well-bounded |
| 3 | `fmod/` | miniaudio + Steam Audio | `SoundSys` already isolates it |
| 4 | *(none)* | Recast & Detour | Currently compiled out via `ENABLE_AUTODESK_NAVIGATION=0` |
| 5 | `Scaleform3/` | RmlUi | Largest — every screen must be re-authored |

See `../../../DEPENDENCIES.md` for the full audit.
