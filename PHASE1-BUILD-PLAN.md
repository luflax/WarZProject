# Phase 1 — A Buildable C++20 Project

Getting the codebase to compile and link as a modern C++20 CMake project, without
touching a single original file.

**Status: plan + scaffolding.** The `modern/` tree exists with its build system and shim
layer; the migration work itself is not done.

---

## Goal

> **A clean C++20 compile and link of client and server, on MSVC and (for the server)
> Clang/GCC.**

## Non-goals — read these carefully

Phase 1 produces a **buildable** project, not a **playable** one. Explicitly out of scope:

- ❌ The game rendering anything
- ❌ Audio, VOIP, in-game browser, anti-cheat
- ❌ Physics behaving correctly
- ❌ Any performance work from `PERFORMANCE-OPTIMIZATION-PLAN.md`
- ❌ Replacing dependencies that do not block compilation

**Only dependencies that block compilation get replaced.** Everything else gets a shim
that satisfies the compiler and does nothing at runtime. Substituting real
implementations is later phases' work.

This ordering is deliberate: a green build is the prerequisite for every other item in
the plan, and it is the only milestone reachable without first choosing an ECS, a
graphics API, or an audio engine.

---

## Strategy: mirror the tree, shim at the original include paths

The single most important decision in this phase.

The codebase is riddled with deep relative includes — `#include "../../External/fmod/fmod_event.hpp"`,
`#include "../../../External/PunkBuster/pbcommon.h"`, `#include "CkHttp.h"` resolved via
`AdditionalIncludeDirectories`. Restructuring the folder layout would break **thousands**
of them.

So `modern/` mirrors the original tree **exactly**:

```
modern/
├── CMakeLists.txt              ← replaces every .vcxproj / .vcproj
├── cmake/                      ← toolchain, warnings, dependency fetching
├── src/
│   ├── Eternity/               ← copied verbatim
│   ├── GameEngine/             ← copied verbatim
│   ├── EclipseStudio/          ← copied verbatim
│   ├── ServerNetPackets/       ← copied verbatim
│   └── External/               ← ★ shims live here, at the paths the code already expects
│       ├── RakNet/             (real — BSD, present in original)
│       ├── pugiXML/            (real — MIT, vendored fresh)
│       ├── fmod/               (shim)
│       ├── Scaleform3/         (shim)
│       ├── ChilKat/            (shim)
│       ├── ts3_sdk_3/          (shim)
│       ├── dxsdk/              (D3DX → DirectXMath compat)
│       ├── AutodeskNav/        (shim — also compiled out by flag)
│       ├── PhysX/              (PhysX 4.1 + 3.x compat header)
│       ├── CrashRpt/           (shim)
│       └── ...
├── server/src/                 ← copied verbatim
└── tools/bootstrap.sh          ← the copy script, re-runnable
```

**Because the shims sit at `src/External/<name>/` — the exact paths the source already
includes — most dependency substitution requires zero source edits.** `#include
"../../External/fmod/fmod_event.hpp"` silently resolves to a header that declares the
same API and does nothing.

This is what makes a compile-first milestone tractable at all.

---

## Milestones

| # | Milestone | Definition of done |
|---|---|---|
| **A** | **It compiles** | Every TU produces an object file under `/std:c++20` |
| **B** | **It links** | `WarZ.exe`, `GameServer.exe`, `MasterServer.exe`, `SupervisorServer.exe` produced |
| **C** | **It runs to a known point** | Server boots, loads a level, enters its tick loop; client reaches `game::MainLoop` with a null renderer |

Milestone C is the real proof — a build that links but faults on the first frame has not
demonstrated that the shim boundaries are correct.

---

## Work breakdown

### 1. Bootstrap the tree — S

`tools/bootstrap.sh` copies source from the original tree, excluding:

- `*.lib`, `*.dll`, `*.exe` (31 MB of prebuilt binaries under `src/Eternity/lib/`)
- `*.vcproj`, `*.vcxproj`, `*.sln`, `*.user` — CMake replaces all of them
- Art assets (`*.png`, `*.ico`, `*.swf`)

Re-runnable and idempotent, so the original tree stays the source of truth until the fork
is deliberately cut.

**Net copy: ~13 MB of source.**

### 2. Build system — M

Replace six Visual Studio solutions with one CMake tree.

- **CMake ≥ 3.25**, `CXX_STANDARD 20`, `CXX_STANDARD_REQUIRED ON`
- **MSVC needs `/permissive-`, `/Zc:__cplusplus`, `/Zc:preprocessor`** — without these
  you are not really compiling C++20
- Targets mirroring the original: `r3dLib` (static) → `GameEngine` → `WarZ` / `GameServer`
  / `MasterServer` / `SupervisorServer`
- Configurations `Debug` / `Release` / `Final`, preserving the `FINAL_BUILD` semantics
- **CPM.cmake** for fetching new dependencies — no submodules
- Warnings **not** set to `-Werror` in this phase. A 2013 codebase under C++20 warnings
  produces thousands of hits; triaging them is Phase 2 work.

### 3. Compile-blocking dependency shims — L

The heart of the phase. Each shim declares the API surface the source references and
implements it as a no-op.

| Shim | Blocks compilation because | Shim strategy |
|---|---|---|
| **dxsdk / D3DX** | **D3DX was removed from the Windows SDK.** `D3DXMATRIX` is pervasive (`GameObj.h:305`) | *Real* compat layer: `D3DXMATRIX`, `D3DXMatrixTranslation`, `D3DXMatrixScaling`, etc. backed by **DirectXMath**. Must be functionally correct — the object transform path depends on it |
| **Scaleform3** | `GFx.h`, `GFx_Kernel.h`, `GFx_Renderer_D3D9.h`, `AS3_Global.h` absent | No-op classes. `Scaleform::GFx::Value` is used as a member (`AI_Player.H`), so it needs a real definition — a tagged variant will do |
| **fmod** | `fmod_event.hpp`, `Fmod_errors.h` absent | No-op. `SoundSys` returns invalid handles; `Play`/`Stop`/`Release` do nothing |
| **ChilKat** | `CkHttp.h`, `CkByteData.h`, `CkGZip.h`, `CkString.h` absent | No-op returning failure. **Or wire libcurl now** — it is small and unblocks real backend work early |
| **ts3_sdk_3** | `ts3client_*` API absent | No-op returning `ERROR_ok` |
| **PhysX** | Entire SDK absent; ~30 include dirs reference it | Vendor **PhysX 4.1** (BSD-3) + a 3.x→4.x compat header. The largest single item here |
| **CrashRpt** | `CrashRpt.h` absent | No-op |
| **libjpeg** | `libjpeg.lib` absent | Swap to **stb_image** (public domain) |
| **Steam** | `steam_api` absent | No-op. `steam_api_dyn.h` implies dynamic loading was already contemplated |
| **GameBlocks** | `ENABLE_GAMEBLOCKS` is defined **unconditionally** at `ServerGameLogic.h:9`; SDK is gitignored | Make the define conditional **and** provide a no-op shim |

**Compiled out by existing flags — no shim needed:**

| Flag | Set to | Effect |
|---|---|---|
| `ENABLE_AUTODESK_NAVIGATION` | `0` | Kynapse/navmesh out — zombies will not path |
| `ENABLE_WEB_BROWSER` | `0` | Berkelium out |
| `APEX_ENABLED` | `0` | Already off |
| `__WITH_PB__` | undefined | PunkBuster out |
| `USE_VMPROTECT` | undefined | Already off |

That the original authors left these switches in place is what makes this phase feasible.

### 4. C++20 conformance fixes — M

Source edits required regardless of shims. These are genuine ISO violations that MSVC
accepted in permissive mode.

| Issue | Sites | Fix |
|---|---|---|
| **Qualified member declaration inside class** — `gobjid_t &gobjid_t::operator=(...)`. Ill-formed ISO C++; rejected under `/permissive-` and by Clang/GCC | `GameObj.h:177`, `GameObj.h:181` | Drop the `gobjid_t::` qualifier |
| **`stdext::hash_map`** — MSVC-only, removed from modern MSVC | `TeamSpeakClient.h:34`, `TeamSpeakClient.cpp` (×4), `ServerGameLogic.h:140` | → `std::unordered_map` |
| **Tokens after `#endif`** | `Game.h:44`, `GameCommon.h:94`, `VehicleDescriptor.h:216`, `ServerGame.h:4`, others | Comment them out |
| **`throw` of string literal** | `ServerMain.cpp:97-100`, `MasterServerLogic.cpp:203` | → `std::runtime_error`. Cheap, and starts unwinding the `TerminateProcess`-on-error habit |
| **Two-phase name lookup** in templates | Expect many in `Tsg_stl/` | Add `typename` / `this->` as the compiler demands |
| **String literal → `char*`** | Widespread | `const char*`, or `/Zc:strictStrings-` as a temporary crutch |

**Expect more.** This list comes from targeted greps, not a compile — `/permissive-` on a
2013 codebase will surface a long tail. Budget for it.

### 5. Milestone C validation — S

- Server: boots, parses config, loads a level via `LoadLevel_Objects`, enters the
  `PlayGameServer` tick loop, accepts a socket connection
- Client: reaches `game::MainLoop` with a null render pipeline without faulting
- Both under ASan where the toolchain allows

---

## Sequencing

```
1. Bootstrap ──▶ 2. CMake ──▶ 3. Shims ◀──▶ 4. Conformance ──▶ 5. Validate
                                  └──── iterate together ────┘
```

Steps 3 and 4 interleave: you cannot find the next conformance error until the previous
missing header is shimmed. Expect several passes.

Rough shape: bootstrap and CMake are days; shims and conformance are the bulk; PhysX 4.1
integration is the single largest sub-item and the most likely to slip.

---

## Risks

| Risk | Mitigation |
|---|---|
| **PhysX 3→4 API drift is larger than expected** | Fall back to a full no-op PhysX shim for Milestones A/B. Physics correctness is not a Phase 1 goal — do not let it block a green build |
| **`/permissive-` surfaces a very long conformance tail** | Compile module by module (Eternity → GameEngine → EclipseStudio → server), not all at once. Fix as encountered |
| **`Scaleform::GFx::Value` is a by-value member**, so it cannot be a pure forward declaration | Give the shim a real tagged-variant definition, not an opaque type |
| **Shims silently diverge from real API semantics** | Milestone C exists precisely to catch this. A linking build is not a passing build |
| **The copied tree drifts from the original** | `bootstrap.sh` is idempotent and re-runnable. Cut the fork deliberately once Milestone C passes, and record the commit |

---

## What Phase 1 explicitly leaves broken

By design, after Phase 1 the game will build and start, and:

- Render nothing (D3D9 path intact but Scaleform-shimmed; renderer replacement is a later phase)
- Play no audio
- Have no zombie pathfinding (`ENABLE_AUTODESK_NAVIGATION 0`)
- Have no backend connectivity (unless libcurl is wired early)
- Have no VOIP, browser, or anti-cheat
- Possibly have inert physics

**This is the correct outcome.** Each of these is restored by a later, independently
scoped phase against a codebase that already compiles — which is a far better position
than trying to modernize dependencies and architecture simultaneously.

---

## Related documents

- `CLAUDE.md` — codebase architecture and conventions
- `DEPENDENCIES.md` — full dependency audit and replacement choices
- `PERFORMANCE-OPTIMIZATION-PLAN.md` — the performance work this unblocks
- `modern/README.md` — how to build the ported tree
</content>
