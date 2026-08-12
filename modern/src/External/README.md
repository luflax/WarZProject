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

| Directory | Replaces | Backed by | Implementation |
|---|---|---|---|
| `dxsdk/` | D3DX math | hand-written scalar code | inline, in `dxsdk/Include/d3dx9.h` |
| `dxsdk/` | D3DX image loading | a DDS parser, no dependency | `compat/D3DXImage.cpp` |
| `dxsdk/` | D3DX shader compilation | `d3dcompiler_NN.dll` | `compat/D3DXShaderCompile.cpp` |
| `ChilKat/` | Chilkat HTTP + gzip | WinHTTP, the in-tree zlib, local base64 | `compat/ChilkatHttp.cpp` |
| `CrashRpt/` | CrashRpt | `MiniDumpWriteDump` | `compat/CrashReport.cpp` |

`D3DXMATRIX` and friends are used pervasively — `GameObject::UpdateTransform`
(`GameObj.h:305-318`) builds every object's world matrix through them. A no-op here
produces a build that links and renders nothing correctly. This layer is real code.

**`compat/` is where a shim goes once it grows a `.cpp`.** The headers stay where they
are, at the path the source already includes from; only the bodies move. Those
translation units are compiled into the Eternity target (see
`../Eternity/CMakeLists.txt` for why that placement and not a library of their own),
and they are PCH exclusions because they talk to `winhttp.h`, `dbghelp.h` and
`d3dcompiler.h` rather than to `r3d.h`.

Each carries its remaining limits in its own header comment. The short version: images
load but do not save and are DDS only; shader reflection (`ID3DXConstantTable`) is not
implemented because nothing asks for it; crash reports are written to disk but never
uploaded.

### 3. No-op shims

Declare the API surface, do nothing at runtime. These exist purely so the compiler and
linker are satisfied.

| Directory | Replaces | Runtime behaviour |
|---|---|---|
| `Scaleform3/` | Scaleform GFx | UI does not render |
| `fmod/` | FMOD Ex | Silence |
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

| Order | Shim | Replacement | Status |
|---|---|---|---|
| 1 | `ChilKat/` | WinHTTP + zlib (not libcurl — see DEPENDENCIES.md) | **done** |
| 2 | `PhysX/` | PhysX 4.1 → Jolt | PhysX 4.1 **done**; Jolt is a performance-phase item |
| 3 | `fmod/` | miniaudio + Steam Audio | not started — the hard part is the *Event* system, not the mixer |
| 4 | *(none)* | Recast & Detour | linked, but `BuildForCurrentLevel` / `LoadPathData` are still seams |
| 5 | `Scaleform3/` | RmlUi | integrated; the render interface and every screen are still to do |

See `../../../DEPENDENCIES.md` for the full audit.
