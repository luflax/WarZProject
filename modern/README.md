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
| **Eternity** (engine core) | ✅ **91/91 TUs** — in *both* client and server configuration |
| **GameEngine** | ✅ **44/44 TUs** — both configurations; PhysX 4.1, Recast/Detour and RmlUi all in |
| **EclipseStudio** (client + editors) | ✅ **209/209 TUs** |
| **server/src** (3 server binaries) | ✅ **71/71 TUs** |
| Shared sources, server configuration | ✅ **48/48 TUs** |
| **Milestone A — compiles** | ✅ **done** |
| **Milestone B — links** | 🔨 **in progress** — see [`../MILESTONE-B-PLAN.md`](../MILESTONE-B-PLAN.md) |
| ↳ B0 build-configuration correctness | ✅ **done** |
| ↳ B1 CMake targets | ✅ **done** — `cmake --build` drives the whole tree |
| ↳ B2 vendored libraries | ✅ RakNet, pugixml, Recast/Detour, RmlUi · ⬜ PhysX |
| ↳ B3 link the binaries | 🔨 **2 of 4 linked** — see below |
| ↳ B4 residual symbols | ✅ **done** — zero own-code symbols left |

### Binaries

```
SupervisorServer.exe   LINKED   3,033,706 bytes   PE32 i386
MasterServer.exe       LINKED   3,168,442 bytes   PE32 i386
GameServer             17 undefined  — all PhysX
WarZ.exe               56 undefined  — all PhysX
```

All four **compile** completely and all four reach the linker. **Every remaining
unresolved symbol in the entire product is PhysX** — it is vendored headers-only, so
`PxCreateFoundation` and friends have nothing to resolve against. Nothing else stands
between this tree and four binaries.

Build with:

```sh
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-i686.cmake
./tools/build.sh              # reports COMPILE vs LINK failure per binary
```
| Milestone C — runs to a known point | ⬜ not started |

**The whole product compiles under strict ISO C++20** — 415 translation units, no
`-fpermissive`, no commercial SDK. Every one of them also passes real **codegen**
(`-c`, not just `-fsyntax-only`), which is what Milestone B needs.

Eternity and GameEngine are checked in **both** configurations, because the client links
them without `WO_SERVER` and the servers with it, and the two take different `#ifdef`
branches. That is 135 TUs × 2 configurations + 280 single-configuration TUs = **550
compilations**. Measured with `./tools/probe.sh` and `./tools/codegen.sh` (MinGW-w64
i686, `-std=c++20 -fms-extensions -msse2`); pass `CONFIG=client` to select the
client-side build of the two shared libraries.

Several sources are compiled **twice** across the product — once as client code and once
with `WO_SERVER` defined, which takes different `#ifdef` branches. `WO_GameServer.vcxproj`
pulls in 50 files from `src/` and `MasterServer.vcxproj` three more.
`tools/find_shared_server_sources.py` extracts that list, and `probe.sh` checks it in the
server configuration:

```bash
./tools/find_shared_server_sources.py
FORCE_BINARY=WO_GameServer ./tools/probe.sh @.probe-shared.WO_GameServer
```

### What replaced what

**All three discontinued SDKs are gone.** PhysX 4.1 is vendored (BSD-3), Autodesk
Navigation is replaced by Recast & Detour (zlib), Scaleform GFx by RmlUi (MIT).

PhysX 3 → 4 was the largest single piece of work. Renames that could be aliased live in
`src/External/PhysX/compat/Px3xCompat.h`; the rest were ported by hand:

| Removed in PhysX 4 | How it is handled now |
|---|---|
| `PxScene::raycastSingle` / `sweepSingle` / `sweepMultiple` / `overlapAny` / `overlapMultiple` | `PxScene3x`, a derived class in `Px3xCompat.h` that adds them back over PhysX 4's buffer API. `PhysXWorld::PhysXScene` is typed as `PxScene3x*`, so ~35 call sites are untouched. |
| `PxVehicleWheelsDynData::getSuspJounce` / `getSteer` | `PxVehicleWheelQueryResult`, plumbed through `PxVehicleUpdates` and exposed by `VehicleManager::GetWheelQueryResults` |
| `PxVehicleWheels::isInAir` | `PxVehicleIsInAir(wheelQueryResult)` |
| `PxRigidActor::createShape` | `PxRigidActorExt::createExclusiveShape` |
| `PxShape::getWorldBounds` | `PxShapeExt::getWorldBounds(shape, actor)` |
| RepX (`RepXCollection`, `instantiateCollection`) | `PxSerialization::createCollectionFromXml` + an explicit type-dispatch walk over the `PxCollection` |
| `PxBatchQueryDesc` buffer fields | `PxBatchQueryMemory`, sized through the descriptor's constructor |

**Deliberate behaviour changes**, all marked `[PORT]` in the source:

- **PhysX PVD is left disconnected.** PhysX 4 needs the `PxPvd` created *before*
  `PxCreatePhysics`, so restoring it means restructuring `Init()`. The exact sequence is
  in a comment at the call site.
- **Convex hulls come from points alone** — `PxConvexMeshDesc::triangles` no longer exists.
- **No navmesh generation.** Recast's build pipeline belongs in the asset cook, as
  Kynapse's generator did. `BuildForCurrentLevel`/`LoadPathData` are the seams. The level
  editor's generator panel now exposes Recast's parameters rather than Kynapse's.
- **No UI screens.** Every screen is a `.swf`; nothing imports Flash into RML, so each
  must be re-authored. The RmlUi `RenderInterface` is also still stubbed.
- **No voice chat, HTTP, gzip, or Steam.** TeamSpeak (client *and* server), Chilkat and
  Steamworks are all proprietary; each is shimmed to fail cleanly so its subsystem
  disables itself rather than proceeding half-initialised.
- **No anti-cheat or gameplay telemetry.** The GameBlocks / FairFight SDK is absent, so
  `GBClient::Connected()` returns false and every guarded call site is skipped. That
  disables the server-side aimbot detector, the weapon-cheat projectile accounting, and
  the whole event stream (kills, chat, item pickups, god-mode attempts). Those call
  sites are a reasonable seam for a replacement detector.
- **Zombie nav diagnostics are re-expressed.** Kynapse exposed a `Kaim::Bot` with a
  visual-debug id, live-path status and a trajectory object; Detour has none of these.
  The logs now report the agent's `dtCrowd` index, status and avoidance result.
  `FindBarricade` tests against the barricade itself rather than walking a Kynapse
  obstacle's spatialised cylinders — which is what the original `TODO` on that line
  asked for, and is slightly more permissive at corners.

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
| `dxsdk/` | D3DX (**removed from the Windows SDK**) | **None — this one is real**: a hand-written, dependency-free scalar implementation |
| `Scaleform3/` | Scaleform GFx (discontinued 2018) | No UI |
| `fmod/` | FMOD Ex (commercial) | Silence |
| `ChilKat/` | Chilkat HTTP + gzip (commercial) | No backend connectivity |
| `ts3_sdk_3/` | TeamSpeak 3 SDK (commercial) | No VOIP |
| `Steam/` | Steamworks (proprietary, optional) | Runs standalone |
| `GameBlocks/` | GameBlocks / FairFight anti-cheat (commercial) | No cheat detection, no telemetry |
| `CrashRpt/` | crash reporting | None meaningful |

`PhysX/` is **not** a shim — PhysX 4.1 is vendored in full under BSD-3, with a
`compat/` layer for the 3.x spellings.

Compiled out entirely via flags that already existed upstream — no shim needed:
`ENABLE_WEB_BROWSER=0`, `APEX_ENABLED=0`, `__WITH_PB__` and `USE_VMPROTECT` undefined.

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
| [`../MILESTONE-B-PLAN.md`](../MILESTONE-B-PLAN.md) | Milestone B (linking) — measured work list, ordering, risks |
| [`../PERFORMANCE-OPTIMIZATION-PLAN.md`](../PERFORMANCE-OPTIMIZATION-PLAN.md) | The performance work this unblocks |
