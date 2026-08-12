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
| **GameEngine** | 🟡 **36/46 TUs compile** — PhysX 4.1 vendored and ported; 10 remain (below) |
| EclipseStudio (client + editors) | ⬜ not started (~471 files) |
| server/src | ⬜ not started (~156 files) |
| Milestone B — links | ⬜ not started |
| Milestone C — runs to a known point | ⬜ not started |

Measured with `./tools/probe.sh <dir>` (MinGW-w64 i686, `-std=c++20`, **no `-fpermissive`**).

### GameEngine: what the 10 remaining failures need

**PhysX is done.** 4.1 is vendored (BSD-3) with a 3.x→4.1 compat layer, and the four
files that used APIs PhysX 4 *removed* have been ported by hand rather than aliased.

| Blocked on | Files | Path forward |
|---|---|---|
| **Autodesk Navigation** | 7 — `AI_Brain`, `AI_Tactics`, five `AutodeskNav*` wrappers | Unobtainable (discontinued). Replace with Recast & Detour |
| **EclipseStudio client surface** | 2 — `obj_Vehicle`, `VehicleManager` | Pull in client weapon/UI headers; unblocked when EclipseStudio is ported |
| **Scaleform GFx** | 1 — `APIScaleformGfx.cpp` | Drives the full Loader/Movie/Renderer/FontProvider API; a port target for RmlUi, not a shim target |

#### What the PhysX port involved

| File | 3.x API that PhysX 4 removed | Ported to |
|---|---|---|
| `PhysXRepXHelpers` | `RepXCollection`, `buildExtensionList`, `RepXUpgrader` | `PxSerialization::createCollectionFromXml` — one call replaces all of it |
| `PhysXWorld` | RepX scene export, `PxSerializable`, `PxUserReferences`, the PVD connection manager, `raycastMultiple`, `PxProfileZoneManager` | `PxCollectionExt::createCollection` + `serializeCollectionToXml`; `PxRaycastBuffer`; PVD left disconnected (see below) |
| `PhysObj` | `PxControllerDesc::interactionMode`/`groupsBitmask`/`callback`, `PxControllerFilters::mActiveGroups`, `raycastAny` | `reportCallback`, `mFilterData`, `raycast` + `PxQueryFlag::eANY_HIT` |
| `Terrain3` | `PxPhysics::createHeightField(desc)`, `PxHeightFieldDesc::thickness` | `PxCooking::createHeightField(desc, insertionCallback)` |

**Two deliberate behaviour changes**, both marked `[PORT]` in the source:

- **PVD is left disconnected.** PhysX 4 requires the `PxPvd` to be created *before*
  `PxCreatePhysics` and passed into it, so reinstating it means restructuring `Init()`.
  PVD is a debug-only convenience; the exact call sequence to restore it is written out
  in a comment at the call site.
- **`PxConvexMeshDesc::triangles` is gone** — PhysX 4 computes convex hulls from the
  point cloud alone, so the index data is no longer supplied and `eINFLATE_CONVEX`
  became a cooking parameter.

Dead code was dropped rather than ported: everything after the original `return true;`
in `ExportWholeScene` was unreachable (an abandoned binary-serialization experiment).

---|---|---|
| **Autodesk Navigation** | 7 — `AI_Brain`, `AI_Tactics`, five `AutodeskNav*` wrappers | Unobtainable (discontinued). Replace with Recast & Detour |
| **PhysX 3.x APIs removed in 4.1** | 4 — `PhysXWorld`, `PhysXRepXHelpers`, `PhysObj`, `Terrain3` | Genuine porting, not aliasing — see below |
| **EclipseStudio client surface** | 2 — `obj_Vehicle`, `VehicleManager` | Blocked until EclipseStudio is ported |
| **Scaleform GFx** | 1 — `APIScaleformGfx.cpp` | Drives the full Loader/Movie/Renderer API; a port target, not a shim target |

The four PhysX files use APIs that PhysX 4 **removed outright**, so no alias can bridge
them:

| File | Needs |
|---|---|
| `PhysXRepXHelpers` | RepX 3.x (`RepXCollection`, `instantiateCollection`) → `PxSerialization` + `PxRepXSerializer` |
| `PhysXWorld` | 3.x serialization (`PxSerializable`, `PxSerialFlags`) and the old PVD connection manager |
| `PhysObj` | `PxControllerDesc::interactionMode`, `groupsBitmask`, `PxControllerFilters::mActiveGroups` — all gone |
| `Terrain3` | heightfield cooking now goes through `PxInputStream`; `PxHeightFieldDesc::thickness` removed |

A `Scaleform::GFx` **type-surface** shim was added — enough for headers that merely
declare Scaleform members (`AI_Player.H` holds a `GFx::Value` by value). It deliberately
does not attempt the full API.

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
