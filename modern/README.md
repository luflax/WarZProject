# Infestation: Survivor Stories — C++20 Port

A CMake/C++20 port of the original codebase, built in isolation.

**The original tree is never modified.** Source is copied in by
`tools/bootstrap.sh`; `../src/` and `../server/` remain the source of truth until the
fork is deliberately cut.

> **Phase 1 target: a project that COMPILES and LINKS.** Not one that plays. See
> [`../PHASE1-BUILD-PLAN.md`](../PHASE1-BUILD-PLAN.md) for scope and explicit non-goals.

---

## Status

| | |
|---|---|
| Scaffolding | ✅ build system, shim layer, bootstrap script |
| Source copied | ✅ `tools/bootstrap.sh` |
| **Eternity** (engine core) | ✅ **81/81 TUs compile** — strict C++20, no `-fpermissive` |
| **GameEngine** | 🟡 **29/46 TUs compile**; 17 blocked on absent SDKs (below) |
| EclipseStudio (client + editors) | ⬜ not started (~471 files) |
| server/src | ⬜ not started (~156 files) |
| Milestone B — links | ⬜ not started |
| Milestone C — runs to a known point | ⬜ not started |

Measured with `./tools/probe.sh <dir>` (MinGW-w64 i686, `-std=c++20`, **no `-fpermissive`**).

### GameEngine: what the 17 remaining failures need

None are conformance problems — every one is an absent SDK.

| Blocked on | Files | Obtainable? |
|---|---|---|
| **PhysX SDK** | 9 — `PhysXWorld`, `PhysObj`, `GameObj`, `Terrain3`, `VehicleManager`, `VehicleDescriptor`, `obj_Vehicle`, `PhysXRepXHelpers`, `CamouflageDataManager` | **Yes** — PhysX 4.1 is BSD-3. Not yet vendored |
| **Autodesk Navigation** | 7 — `AI_Brain`, `AI_Tactics`, and the five `AutodeskNav*` wrappers | **No** — discontinued product |
| **Scaleform GFx** | 1 — `APIScaleformGfx` | **No** — discontinued 2018 |

The PhysX forward-declaration shim carries headers that merely hold pointers. It cannot
carry these nine: they call the API directly (`actor->isRigidStatic()`,
`PxSceneQueryFilterData filter(...)`, `PxHeightFieldSample`). Across GameEngine the
codebase touches **171 distinct PhysX symbols and 89 member accesses** — past the point
where stubbing is cheaper than vendoring PhysX 4.1.

---

## Quick start

```bash
# 1. Copy source from the original tree (idempotent; never writes to it)
./tools/bootstrap.sh

# 2. Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# 3. Build
cmake --build build --parallel
```

Preview what would be copied without writing anything:

```bash
./tools/bootstrap.sh --dry-run
```

### Requirements

- **CMake ≥ 3.25**
- **MSVC 19.30+** (VS 2022) — the client is deeply Win32-bound
- Clang 15+ / GCC 12+ can target the **server** only
- Windows SDK (supplies D3D9 and DirectXMath; the DirectX SDK is *not* needed)

### Options

| Option | Default | Effect |
|---|---|---|
| `WARZ_BUILD_CLIENT` | `ON` | Build `WarZ.exe` |
| `WARZ_BUILD_SERVER` | `ON` | Build the three server binaries |
| `WARZ_USE_SHIMS` | `ON` | No-op shims for absent commercial SDKs |
| `WARZ_WARNINGS_AS_ERRORS` | `OFF` | Leave off for Phase 1 — a 2013 codebase under C++20 produces thousands of warnings |

---

## Layout

The tree **mirrors the original exactly**. This is deliberate and load-bearing: the
codebase is full of deep relative includes like
`#include "../../External/fmod/fmod_event.hpp"`, and restructuring would break thousands
of them.

```
modern/
├── CMakeLists.txt        replaces six Visual Studio solutions
├── cmake/
│   ├── CompilerFlags.cmake    /permissive-, /Zc:__cplusplus, C++20
│   └── Dependencies.cmake     pugixml, stb (fetched)
├── src/
│   ├── Eternity/         copied — r3d engine core
│   ├── GameEngine/       copied — object model, physics, terrain
│   ├── EclipseStudio/    copied — client + editors
│   ├── ServerNetPackets/ copied
│   └── External/         ★ shims, at the paths the source already expects
├── server/src/           copied — GameServer / MasterServer / SupervisorServer
└── tools/bootstrap.sh    the copy script
```

**`src/External/` is where the approach pays off.** Placing shims under the original
directory names means most dependency substitution requires no source edits at all — the
include silently resolves to a header that declares the same API and does nothing. See
[`src/External/README.md`](src/External/README.md) for the shim contract.

---

## What is shimmed, and what that costs

| Shim | Replaces | Runtime consequence |
|---|---|---|
| `dxsdk/` | D3DX (**removed from the Windows SDK**) | **None — this one is real**, backed by DirectXMath |
| `Scaleform3/` | Scaleform GFx (discontinued 2018) | No UI |
| `fmod/` | FMOD Ex (commercial) | Silence |
| `ChilKat/` | Chilkat HTTP (commercial) | No backend connectivity |
| `ts3_sdk_3/` | TeamSpeak 3 SDK (commercial) | No VOIP |
| `PhysX/` | PhysX 3.x | Inert physics until PhysX 4.1 is vendored |
| `CrashRpt/`, `GameBlocks/` | crash reporting, anti-cheat | None meaningful |

Compiled out entirely via flags that already existed upstream — no shim needed:
`ENABLE_AUTODESK_NAVIGATION=0` (zombie pathing), `ENABLE_WEB_BROWSER=0`, `APEX_ENABLED=0`,
`__WITH_PB__` and `USE_VMPROTECT` undefined.

**After Phase 1 the game builds and starts, renders nothing, and is silent. That is the
correct outcome** — each subsystem is restored by a later, independently scoped phase
against a codebase that already compiles.

---

## Licensing

Every dependency is MIT / BSD / zlib / Apache-2.0. **No commercial agreements, no
copyleft.** Shims are clean-room declarations derived from call sites in this codebase —
no code originates from any commercial SDK.

Full audit: [`../DEPENDENCIES.md`](../DEPENDENCIES.md).

---

## Related documents

| Document | Covers |
|---|---|
| [`../CLAUDE.md`](../CLAUDE.md) | Codebase architecture and conventions |
| [`../DEPENDENCIES.md`](../DEPENDENCIES.md) | Dependency audit and replacement choices |
| [`../PHASE1-BUILD-PLAN.md`](../PHASE1-BUILD-PLAN.md) | This phase — milestones, work breakdown, risks |
| [`../PERFORMANCE-OPTIMIZATION-PLAN.md`](../PERFORMANCE-OPTIMIZATION-PLAN.md) | The performance work this unblocks |
