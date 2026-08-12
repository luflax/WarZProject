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
| **Milestone B — links** | ✅ **done** — see [`../MILESTONE-B-PLAN.md`](../MILESTONE-B-PLAN.md) |
| ↳ B0 build-configuration correctness | ✅ **done** |
| ↳ B1 CMake targets | ✅ **done** — `cmake --build` drives the whole tree |
| ↳ B2 vendored libraries | ✅ RakNet, pugixml, Recast/Detour, RmlUi, **PhysX 4.1** |
| ↳ B3 link the binaries | ✅ **4 of 4** |
| ↳ B4 residual symbols | ✅ **done** — zero own-code symbols left |
| ↳ B5 PhysX for MinGW-i686 | ✅ **done** — 403 TUs, 15 static libs |
| **Milestone C — runs to a known point** | ⬜ blocked on a Windows runtime — pre-work in [`../MILESTONE-C-PREWORK.md`](../MILESTONE-C-PREWORK.md) |
| ↳ staged `--selftest` ladder + test suite | ✅ **done** — see [Tests](#tests); the tiers needing no runtime report green |
| ↳ compat layer (HTTP, DDS, shaders, crash dumps, PVD) | ✅ **done** — §2.1-2.3, 2.5, 2.6 of that plan |

### Binaries

```
SupervisorServer.exe    3,222,276 bytes   PE32 i386, console
MasterServer.exe        3,358,117 bytes   PE32 i386, console
GameServer.exe         15,543,210 bytes   PE32 i386, console
WarZ.exe               23,949,606 bytes   PE32 i386, GUI
```

Each grew by roughly 190 KB when the shims listed under
[the compat layer](#srcexternalcompat--the-shims-that-grew-implementations) stopped
being no-ops, and the two that link PhysX grew a further ~800 KB when PVD was turned
on and its instrumentation started being compiled.

**Zero unresolved symbols across the whole product.** Four binaries, from a tree that
did not build at all as checked out, with no commercial SDK anywhere in it.

Build with:

```sh
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw-i686.cmake
./tools/build.sh              # reports COMPILE vs LINK failure per binary
```

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

**All three discontinued SDKs are gone.** PhysX 4.1 is vendored and **built from source**
(BSD-3), Autodesk Navigation is replaced by Recast & Detour (zlib), Scaleform GFx by
RmlUi (MIT).

PhysX is built for **i686-w64-mingw32**, a configuration NVIDIA neither ships nor tests —
they build Windows with MSVC and Linux with GCC, and their build system cannot produce
the combination. `cmake/BuildPhysX.cmake` is that build; `tools/gen_physx_sources.py`
derives the 403 translation units from NVIDIA's own module files rather than globbing.
Six source changes were needed, all marked `[PORT]`; the one that mattered was
`PX_ALIGN` silently evaporating, because it keyed off a *platform* test that is true for
MinGW while `__declspec(align)` is not something GCC implements. See
[`src/External/PhysX/README.md`](src/External/PhysX/README.md).

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

- **Convex hulls come from points alone** — `PxConvexMeshDesc::triangles` no longer exists.
- **No navmesh generation.** Recast's build pipeline belongs in the asset cook, as
  Kynapse's generator did. `BuildForCurrentLevel`/`LoadPathData` are the seams. The level
  editor's generator panel now exposes Recast's parameters rather than Kynapse's.
- **No UI screens.** Every screen is a `.swf`; nothing imports Flash into RML, so each
  must be re-authored. The RmlUi `RenderInterface` is also still stubbed.
- **No voice chat or Steam.** TeamSpeak (client *and* server) and Steamworks are
  proprietary; each is shimmed to fail cleanly so its subsystem disables itself rather
  than proceeding half-initialised. **HTTP and gzip are no longer in that list** — see
  the compat layer below.
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

> ⚠️ **Do not run `tools/bootstrap.sh` on this tree.** It `rm -rf`s the five copied
> source trees before re-copying, and 348 files in them now carry Milestone A/B port
> work that exists nowhere else — `../src/` and `../server/` have none of it. The fork
> was cut in practice at Milestone B; the script has not caught up. See
> [`../MILESTONE-C-PREWORK.md`](../MILESTONE-C-PREWORK.md) §1.1.

```bash
# 1. Copy source from the original tree (idempotent; never writes to it)
#    -- SUPERSEDED, see the warning above
./tools/bootstrap.sh

# 2. Configure and build
cmake --preset default
./tools/build.sh
```

`tools/build.sh` is preferred over a bare `cmake --build` because it reports each
binary's real status, and distinguishes **COMPILE FAILED** from **LINK FAILED** — grepping
a build log for `undefined reference` reports a target as having zero unresolved symbols
when in truth it never reached the linker.

The presets in [`CMakePresets.json`](CMakePresets.json) carry the cross-toolchain, so
there is nothing to remember:

| Preset | Build dir | For |
|---|---|---|
| `default` | `build/` | Release, `-O3`. The normal build. |
| `dev` | `build-dev/` | Release's defines at `-O1`. Faster to build; **not** for profiling or shipping. |
| `final` | `build-final/` | The original shipping configuration. Not yet verified by this port. |
| `no-cache` | `build-reference/` | ccache and PCH both off. The reference build for `tools/buildtime.sh --verify`. |

Preview what would be copied without writing anything:

```bash
./tools/bootstrap.sh --dry-run
```

### Requirements

- **CMake ≥ 3.25**
- **MinGW-w64 i686 GCC 13** — what this port is actually built and verified with; it
  produces all four binaries, client included
- MSVC 19.30+ (VS 2022) is the other intended target and the flags are kept in
  `cmake/CompilerFlags.cmake`, but no configuration of this port has been built with it
- Windows SDK (supplies D3D9 and DirectXMath; the DirectX SDK is *not* needed)

Optional, both detected automatically:

- **Ninja** — the default generator; falls back to Unix Makefiles
- **ccache** — see [Build times](#build-times)

### Options

| Option | Default | Effect |
|---|---|---|
| `WARZ_BUILD_CLIENT` | `ON` | Build `WarZ.exe` |
| `WARZ_BUILD_SERVER` | `ON` | Build the three server binaries |
| `WARZ_USE_SHIMS` | `ON` | No-op shims for absent commercial SDKs |
| `WARZ_WARNINGS_AS_ERRORS` | `OFF` | Leave off for Phase 1 — a 2013 codebase under C++20 produces thousands of warnings |
| `WARZ_PCH` | `ON` | Precompile `r3dPCH.h`. See [Build times](#build-times) |
| `WARZ_CCACHE` | `ON` | Use `ccache` if it is installed; silently skipped if not |
| `WARZ_CCACHE_DIR` | *(ccache default)* | Cache location. The default `~/.cache/ccache` already survives `rm -rf build` |
| `WARZ_CCACHE_MAXSIZE` | `10G` | 1,303 objects per configuration, and this port has several |

---

## Build times

Measured on 4 cores, i686-w64-mingw32 GCC 13, `Release`. `tools/buildtime.sh` reproduces
every number here.

| | Before | Now |
|---|---|---|
| Null build (nothing to do) | 2.3 s | **0.06 s** |
| One `.cpp` + relink | 6.3 s | **1.3 s** |
| `P2PMessages.h` — 58 TUs | 1 m 42 s | **29 s** |
| `r3dPCH.h` — rebuilds everything | — | **2 m 06 s** |
| Clean build | 18.9 min | **8 m 23 s** |
| Clean build, warm ccache | 18.9 min | **13 s** |

Three separate mechanisms, because these are three different problems:

**Ninja** (`CMakePresets.json`) fixes only the null build. Recursive make spent 2.3
seconds stat-ing 1,303 objects to decide there was nothing to do.

**Precompiled headers** (`cmake/Pch.cmake`) fix the per-edit cost. An average
translation unit here pulls in **888 headers**, and 698 of them arrive through
`r3dPCH.h` — which costs **4.98 s to parse against a whole TU's 5.25 s**. It was already
the original MSVC build's precompiled header, and 417 of the 419 sources still open with
`#include "r3dPCH.h"`; only the wiring was lost in the move to CMake. Restoring it needed
no source changes at all.

**ccache** (`cmake/Speed.cmake`) fixes the clean build, which is what a branch switch
really is. Optional — absent, everything still works, just uncached.

Two things worth knowing:

- **The eight precompiled headers cost 1.6 GB** in the build directory (~200 MB each).
  One per target, because `r3dPCH.h` branches on `WO_SERVER`, `DISABLE_PHYSX` and
  `FINAL_BUILD`, so the client and server variants are genuinely different headers.
- **In an ephemeral container the ccache directory does not survive the session.** The
  13-second figure needs a cache that persists; set `WARZ_CCACHE_DIR` to somewhere that
  does.

### Neither cache may change the binary

`tools/buildtime.sh --verify` builds a reference tree with PCH and ccache both off and
compares **every object file**. Current result: **1,300 identical, 0 differing** — the
three exceptions being the TUs that expand `__TIME__` (`Main.cpp` and the two
`VersionNo.cpp`), which differ between any two builds.

This is not ceremony. `CCACHE_BASEDIR` — the standard advice for sharing a cache between
checkouts — rewrites absolute paths to relative *on the way to the compiler*, which
changed `__FILE__` in **621 of 1,303 objects** and took 27 KB off `WarZ.exe`. Correct
output, but it meant that installing a cache changed the program, and that two people
reading the same `r3d_assert` would see different paths. It is deliberately not set;
`cmake/Speed.cmake` records why.

### What was not done

**Unity builds** would cut the clean build further, but this codebase fights them:
`menu_sliders_x`/`_y` are file-scope statics in two different files and `gNearPlaneNormal`
in three, and each collision is a hand-fixed compile error. PCH and ccache capture most
of the same win without touching what the compiler sees.

**Header fan-out** is the deeper problem and is untouched. `r3d.h` reaches 208 of WarZ's
209 translation units, so it is still the case that changing one header rebuilds almost
everything — PCH scales that cost down but does not change its shape. `tools/header_cost.py`
ranks headers by *(TUs reached × parse cost)* so that work, if it is ever done, is aimed
by measurement:

```bash
./tools/header_cost.py                 # rank by reach
./tools/header_cost.py --measure 15    # price the top 15, rank by real cost
```

**A faster linker** was measured and rejected: linking `WarZ.exe` — 22.9 MB, static, ~20
archives — takes about one second. There is nothing there to win.

---

## Tests

The product is PE32 and needs Wine to run, and `wine32:i386` frequently does not resolve
in a container. A suite that only ran under Wine would report nothing in the environment
this port is developed in, so the tests are split by **what each one needs in order to
reach a verdict**:

| Tier | Needs | Reports today |
|---|---|---|
| `warz_layout_checks` | the cross-compiler only | ✅ |
| `warz_tests_host` | a host compiler only | ✅ |
| `warz_tests` | a working emulator | ⬜ built, disabled |
| `warz_tests_server` | a working emulator | ⬜ built, disabled |
| `milestone_c.*` | a working emulator | ⬜ built, disabled |

```sh
# compile-only + whatever the emulator can run
cmake --build build --target warz_layout_checks
ctest --test-dir build

# host-native tier -- runs anywhere, no Wine
cmake -B build-host -S tests && cmake --build build-host
./build-host/warz_tests_host
```

The cross build probes the emulator with `try_run` at configure time and **disables**
the tiers it cannot run rather than failing them, so a red suite always means something
is broken rather than that a container has no Wine.

**`warz_layout_checks` is compile-only** — every assertion is a `static_assert`, so
building the target *is* the test. It covers the wire format: 154 `#pragma pack(1)`
packet structs whose layout the client and server must agree on byte for byte, frozen by
`tools/gen_packet_layout.py` (which reads `DW_AT_byte_size` out of the compiler's own
DWARF, so it needs no runtime either). Regenerate it only alongside a deliberate packet
change, with a `P2PNET_VERSION` bump in the same commit.

**`warz_tests_host`** carries the tests whose code under test has no Win32 dependency —
principally the D3DX math. That shim is the only one in the tree that fails *silently*:
every other one returns an error or makes no sound, this one returns numbers, and 864
uses of `D3DXMATRIX` sit downstream. `tests/host/` stands in for `<d3d9.h>` so it can run
natively; `tests/layout/test_d3dx_layout.cpp` is compiled in *both* configurations and
asserts the same sizes and offsets in each, so the stand-in cannot drift into a
comfortable fiction.

**`milestone_c.*`** takes Milestone C's server criterion apart. As
[`PHASE1-BUILD-PLAN.md`](../PHASE1-BUILD-PLAN.md) states it, it is one sentence covering
four independent claims — and the real boot path cannot reach any of them without a
MasterServer, a SupervisorServer and a backend, because `gameServerLoop` calls
`r3dError` when the API does not answer. Every one of those dependencies is in the outer
shell, so `GameServer.exe --selftest=<stage>` drives the initialisation directly:

```sh
GameServer.exe --selftest=config                      # process, log, console vars
GameServer.exe --selftest=world  --ticks=100          # + ObjectManager and PhysX
GameServer.exe --selftest=level  --level=<dir>        # + a level loads
```

There is no game data in the repository ([`MILESTONE-C-PREWORK.md`](../MILESTONE-C-PREWORK.md)
§1.3), so `tests/fixtures/levels/UnitTestLevel` is the synthetic level that doc proposes
as the alternative: four objects with written-down positions and no mesh, texture or
sound reference anywhere.

The framework is ~250 lines in `tests/framework`, not doctest or GoogleTest — both would
arrive through a fetch that fails offline, and neither is worth an entry in
[`DEPENDENCIES.md`](../DEPENDENCIES.md).

---

## Layout

The tree **mirrors the original exactly**. This is deliberate and load-bearing: the
codebase is full of deep relative includes like
`#include "../../External/fmod/fmod_event.hpp"`, and restructuring would break thousands
of them.

```
modern/
├── CMakeLists.txt        replaces six Visual Studio solutions
├── CMakePresets.json     default / dev / final / no-cache
├── cmake/
│   ├── CompilerFlags.cmake    /permissive-, /Zc:__cplusplus, C++20
│   ├── Dependencies.cmake     pugixml, stb (fetched)
│   ├── BuildPhysX.cmake       PhysX 4.1 for MinGW-i686 — a config NVIDIA does not ship
│   ├── Pch.cmake              precompiles r3dPCH.h; ~90% of each TU's front-end cost
│   └── Speed.cmake            ccache, when it is installed
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
| `dxsdk/` | D3DX (**removed from the Windows SDK**) | **None for math, loading or shaders — these are real.** Math is a hand-written scalar implementation; DDS loading and `D3DCompile` are in `compat/`. Image *saving* is still stubbed, and only the editors use it |
| `ChilKat/` | Chilkat HTTP + gzip (commercial) | **None — real.** WinHTTP transport, the in-tree zlib for gzip, local base64. See `compat/ChilkatHttp.cpp` |
| `CrashRpt/` | crash reporting | **None locally — real.** dbghelp minidumps plus a text report and the attached files. No *upload*: CrashRpt's sender is not part of this build |
| `Scaleform3/` | Scaleform GFx (discontinued 2018) | No UI |
| `fmod/` | FMOD Ex (commercial) | Silence |
| `ts3_sdk_3/` | TeamSpeak 3 SDK (commercial) | No VOIP |
| `Steam/` | Steamworks (proprietary, optional) | Runs standalone |
| `GameBlocks/` | GameBlocks / FairFight anti-cheat (commercial) | No cheat detection, no telemetry |

`PhysX/` is **not** a shim — PhysX 4.1 is vendored and compiled in full under BSD-3, with
a `compat/` layer for the 3.x spellings. Physics is real.

Compiled out entirely via flags that already existed upstream — no shim needed:
`ENABLE_WEB_BROWSER=0`, `APEX_ENABLED=0`, `__WITH_PB__` and `USE_VMPROTECT` undefined.

### `src/External/compat/` — the shims that grew implementations

Four of the entries above stopped being no-ops. Their headers still sit at the path the
source includes from; what changed is that the declarations now have bodies, in a
directory of their own:

| File | Backed by | Serves |
|---|---|---|
| `ChilkatHttp.cpp` | WinHTTP, the in-tree zlib, local base64 | `CWOBackendReq` — client login and menus, **and every `api_Srv*.aspx` call the GameServer makes**. Also `CkString::base64Decode`, which both servers hit at startup decoding the game name out of `argv` |
| `D3DXImage.cpp` | A DDS parser, no dependency | `r3dTexture::LoadTextureInternal` via `r3dDeviceTunnel` — 2D, cube and volume — plus `D3DXLoadSurfaceFromSurface` for Terrain3 |
| `D3DXShaderCompile.cpp` | `d3dcompiler_NN.dll`, loaded at first use | `r3dCompileShader`, for the `vs_3_0` / `ps_3_0` the engine targets |
| `CrashReport.cpp` | `MiniDumpWriteDump` | `r3dThreadEntryHelper`'s `crInstall`, in all four binaries |

They are compiled **into Eternity** rather than into a library of their own. Eternity is
below every caller and is where `r3dOutToLog` lives, so it is the only placement that
lets them log without forming a cycle GNU ld would refuse to resolve. See
`src/Eternity/CMakeLists.txt`.

Three limits are worth knowing before relying on them:

- **Image loading is DDS only, and never resamples.** A requested size is honoured when
  it is the file's own or one of its mip levels — which is what the engine asks for,
  since `LoadTextureInternal` computes its downscale as a power-of-two mip count and
  does the resize itself. Anything else fails loudly rather than returning a
  wrong-sized texture. Image *saving* is untouched and still stubbed.
- **`D3DXCompileShader` sets `D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY`**, because the
  HLSL in `Data/Shaders` is 2013-vintage. `ID3DXConstantTable` is not implemented and
  never returned; nothing in this codebase asks for one.
- **Crash reports are written, not sent.** CrashRpt shipped a separate
  `CrashSender.exe`; there is no uploader here, so reports accumulate under
  `CrashReports/` and the configured endpoint is recorded in `report.txt` rather than
  contacted.

HTTPS to a private backend with a self-signed certificate needs `WARZ_HTTP_INSECURE=1`
in the environment. Certificate validation is on by default, and a validation failure
says so in the log along with the name of that switch.

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
| [`../MILESTONE-C-PREWORK.md`](../MILESTONE-C-PREWORK.md) | Milestone C (running) — blockers, which stubs can be completed, ordering |
| [`../PERFORMANCE-OPTIMIZATION-PLAN.md`](../PERFORMANCE-OPTIMIZATION-PLAN.md) | The performance work this unblocks |
