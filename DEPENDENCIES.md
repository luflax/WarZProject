# External Dependencies

Audit of every third-party dependency in the original codebase, its licensing status,
and a permissive open-source replacement where one is needed.

**Selection criteria:** no commercial license agreements, no copyleft (no GPL/LGPL),
actively maintained, C++20-compatible.

> **Verify before committing engineering time.** License terms change — PhysX has changed
> twice. This table reflects research as of the audit date and is a starting map, not
> legal clearance. Confirm current terms directly from each project.

---

## Already dead — delete on sight

Three dependencies are referenced but not actually used. Removing them costs nothing.

| Dependency | Evidence | Note |
|---|---|---|
| **Granny (RAD Game Tools)** | `r3dGrannyLoader.h` + `r3dGrannyMesh.h` exist with **no `.cpp`**; `r3d.h:17` only forward-declares `r3dGrannyMesh` | The most expensive license in the original list, and it was never wired up |
| **ENet** | Full headers vendored at `src/Eternity/Include/enet/`, **zero `enet_*` call sites**; RakNet is the real backend (`r3dNetwork.cpp:5`) | Vestigial |
| **PhysX APEX** | `APEX_ENABLED 0` at `src/Eternity/Include/r3dPCH.h:127` | Already compiled out |

---

## Keep

| Dependency | Used for | License | Action |
|---|---|---|---|
| **pugixml** | `LevelData.xml`, `SoundData.xml`, `ServerData.xml` parsing | MIT | Keep; vendor current version (**not present in this drop**) |
| **zlib** | `.wz` archive compression | zlib | Keep — vendored at `src/Eternity/Source/ZLib/`. Consider zlib-ng (drop-in) or zstd later |
| **RakNet** | All game networking | BSD-2 (open-sourced 2014) | Keep for the build milestone — it is the only major dep actually present in the repo. Replace later; unmaintained since ~2016 |

---

## Must replace — commercial or unlicensable

| Dependency | Used for | Problem | Replacement | License |
|---|---|---|---|---|
| **Scaleform GFx** | All HUD and menus | Autodesk commercial, **discontinued 2018** — cannot be licensed at any price | **RmlUi** | MIT |
| **Autodesk Navigation** (Kynapse / Gameware) | Zombie navmesh + pathing | Autodesk commercial, **discontinued** | **Recast & Detour** | zlib |
| **Chilkat** (`CkHttp`) | HTTPS to backend API | Commercial, paid | **libcurl** | curl (MIT-like) |
| **FMOD Ex** | All audio | Commercial; free tier still requires a license agreement. FMOD Ex is EOL | **miniaudio** + **Steam Audio** | MIT-0 / Apache-2.0 |
| **TeamSpeak 3 SDK** | In-game VOIP | Commercial, paid | **Opus** over existing transport | BSD-3 |
| **PunkBuster** | Anti-cheat | Commercial, defunct vendor | *Drop* — see below | — |
| **GameBlocks / FairFight** | Anti-cheat, aimbot detection | Commercial | *Drop* — see below | — |
| **HackShield** | Anti-cheat | AhnLab commercial | *Drop* | — |
| **VMProtect** | Binary obfuscation | Commercial | *Drop* (already disabled) | — |
| **NVIDIA NVAPI** | SLI detection + 3D Vision stereo (`r3dRender.CPP` only) | Free download but under NVIDIA's SDK licence, not a permissive one; no redistributable source | *Drop* — no-op shim at `src/External/NVApi/`. 3D Vision was discontinued by NVIDIA in 2019, so this is dead capability | — |
| **AMD `atimgpud`** | Crossfire detection (`r3dRender.CPP` only) | AMD SDK terms; the `.lib` is absent from this drop | *Drop* — one-function shim at `src/External/ATICrossfireDetect/` returning 1 GPU | — |

### Replacement notes

**Scaleform → RmlUi.** The hardest migration in the list. The entire frontend is authored
as `.swf` (`data\menu\Frontend.swf`, `Main_Network.cpp:145`) and Scaleform touches every
HUD class in `Sources/UI/`. RmlUi is the closest structural match — HTML/CSS-like markup
with a data-binding layer, preserving the "designer authors markup, code binds values"
workflow. **Every screen must be re-authored regardless**; nothing imports Flash. Keep
Dear ImGui (MIT) for the editors, which already use an equivalent immediate-mode system.

**Autodesk Navigation → Recast & Detour.** Clean one-to-one mapping: Recast builds the
navmesh, Detour does pathfinding, DetourCrowd covers local avoidance — which is precisely
what `AutodeskNavAvoidanceFilter.cpp` provides. The existing
`GameEngine/ai/AutodeskNav/AutodeskNavMesh.{h,cpp}` wrapper is already the right seam,
and `ENABLE_AUTODESK_NAVIGATION` (`r3dPCH.h:130`) lets you compile it out entirely in the
interim.

**FMOD → miniaudio + Steam Audio.** miniaudio covers device I/O, decoding and basic 3D
panning; Steam Audio adds HRTF, occlusion and reverb — needed because `ReverbZone.cpp`
and `ReverbZoneBox.cpp` are real gameplay features. What you lose is FMOD's *event
system* (`SoundSys.GetEventIDByPath("Sounds/MainMenu GUI/UI_MENU_MUSIC")`); a thin
event/bank layer must be written. `GameEngine/fmod/SoundSys.h` already isolates FMOD, so
the blast radius is contained.

**Chilkat → libcurl.** Direct substitution across `Sources/http/` and `WOBackendAPI.cpp`.
cpp-httplib (MIT, header-only) is the zero-friction alternative, but libcurl is the safer
choice for TLS correctness.

**TeamSpeak → Opus.** No drop-in open-source VOIP SDK exists. Opus gives you the codec;
the mixer, jitter buffer and session management must be written over the existing UDP
transport. **Recommendation: cut VOIP from the first milestone** and revisit once the
network layer is settled.

**Anti-cheat → drop all four.** No open-source equivalent exists for kernel-level
anti-cheat, and that is acceptable: making the server properly authoritative (see
`PERFORMANCE-OPTIMIZATION-PLAN.md` item 4) removes the structural need, and the
server-side statistical approach already present in the FairFight aimbot detector
(`ServerGameLogic.cpp:4194-4237`) is the right model to reimplement in-house.

---

## Replace — permissive but stale

| Dependency | Used for | Problem | Replacement | License |
|---|---|---|---|---|
| **DirectX SDK (June 2010)** | D3D9 + **D3DX** | D3DX was **removed from the Windows SDK**; `D3DXMATRIX` is used pervasively (e.g. `GameObj.h:305`) | **DirectXMath** (Windows SDK) or **GLM** | MIT |
| **libjpeg 6b** | Texture loading | 1998 vintage | **stb_image** at cook time; ship BC7 via **bc7enc_rdo** | Public domain / MIT |
| **CrashRpt** | Crash reporting | Stale | **Crashpad** or **Sentry Native** | Apache-2.0 / MIT |
| **Berkelium** | In-game web browser (`ENABLE_WEB_BROWSER`) | Abandoned, 2012-era Chromium | **Dropped** — the port builds with `-DENABLE_WEB_BROWSER=0`; `r3dPCH.h` now only defaults the flag to 1 when it is not already defined, and every call site is already behind `#if ENABLE_WEB_BROWSER`. Longer term, `ShellExecute` to the OS browser as `Main.cpp:1602` already does | — |
| **PhysX 3.x** | Physics, character controllers | Not a licensing problem (PhysX 4+ is BSD-3), but 3.x is old and the API differs | **Jolt Physics** (target) / **PhysX 4.1** (interim) | MIT / BSD-3 |

**GLM over DirectXMath** if you want the math layer to outlive the graphics API — relevant
given the planned Vulkan backend.

**PhysX staging.** PhysX 3.4's source was published under NVIDIA's GameWorks terms, which
are *not* clean BSD; **PhysX 4.1 and later are BSD-3**. For a compile-first milestone,
vendor PhysX 4.1 behind a thin 3.x→4.x compatibility header (the renames are moderate:
`PxSceneQuery*` → `PxQuery*`, some flag reshuffling). Migrate to Jolt in the performance
phase, not the build phase.

---

## Optional

| Dependency | Used for | Status | Action |
|---|---|---|---|
| **Steamworks SDK** | Steam auth, overlay, callbacks | Free but proprietary; requires a Steamworks agreement | Keep **only** if shipping on Steam. Isolate behind an interface — `steam_api_dyn.h` suggests dynamic loading is already contemplated |

---

## New dependencies to add

All permissive. No copyleft, no LGPL, no commercial agreements.

| Library | License | Purpose |
|---|---|---|
| **EnTT** or **flecs** | MIT | ECS storage (plan item 2) |
| **Vulkan Memory Allocator** + **volk** | MIT | Vulkan backend |
| **meshoptimizer** | MIT | Replaces hand-rolled `r3dVCacheOptimize.cpp` |
| **Tracy** | BSD-3 | Profiler — Phase 0 depends on it |
| **DirectXShaderCompiler** or **slang** | MIT | Shader compilation |
| **zstd** | BSD (dual BSD/GPLv2 — take BSD) | Asset compression |
| **xxHash** | BSD-2 | Replaces `r3dHash.cpp` |
| **doctest** or **Catch2** | MIT / BSL-1.0 | Tests — there are currently none |
| **CMake** + **CPM.cmake** | BSD-3 / MIT | Build system and dependency fetching |

**OpenAL Soft was deliberately excluded** — it is LGPL, which constrains static linking.

---

## C++ standard

**Target C++20.** Not C++23.

The codebase is C++03-era (VS2008 origin, later upgraded to the v120/VS2013 toolset), so
either is a large jump. C++20 is the last rung where MSVC, Clang and GCC all have solid,
comparable support — which matters because Linux servers are a goal.

What earns its keep here:

| Feature | Applied to |
|---|---|
| `std::span` | The raw-pointer-plus-count pairs throughout `ObjManag` and the renderer |
| Concepts | ECS system signatures, replacing SFINAE |
| `constexpr` / `consteval` | Class-registration tables, shader permutation setup |
| Coroutines | Async asset streaming, job graph |
| Ranges | Large amounts of hand-written iteration |
| `std::bit_cast` | Union punning in `r3dSec_type`, packet casts |
| `<bit>`, `<numbers>` | Hand-rolled bit and math helpers |

From C++23 the one genuinely valuable feature is **`std::expected`** — the current error
strategy is `r3dError` → `TerminateProcess` (`ServerMain.cpp:204`), a server that kills
itself on recoverable conditions. Get ~90% of that today with **`tl::expected`** (MIT) on
C++20.

**Skip modules.** Cross-toolchain support remains uneven enough that betting the build
system on them costs more than the compile-time win returns.

---

## Summary

| Category | Count | Items |
|---|---|---|
| Delete (dead) | 3 | Granny, ENet, APEX |
| Keep | 3 | pugixml, zlib, RakNet (interim) |
| Must replace (commercial) | 9 | Scaleform, Autodesk Nav, Chilkat, FMOD, TeamSpeak, PunkBuster, GameBlocks, HackShield, VMProtect |
| Replace (stale) | 5 | DirectX SDK/D3DX, libjpeg, CrashRpt, Berkelium, PhysX 3 |
| Optional | 1 | Steamworks |
| To add | 9 | ECS, Vulkan tooling, profiler, tests, build system |

**Net licensing outcome: zero commercial agreements, zero copyleft.**

## Related documents

- `CLAUDE.md` — codebase architecture and conventions
- `PERFORMANCE-OPTIMIZATION-PLAN.md` — the full modernization plan
- `PHASE1-BUILD-PLAN.md` — getting to a buildable C++20 project
</content>
