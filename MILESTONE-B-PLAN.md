# Milestone B — Linking

**Goal.** Produce four linked executables from `modern/` — `WarZ.exe`, `GameServer.exe`,
`MasterServer.exe`, `SupervisorServer.exe` — under strict ISO C++20, with only
permissive dependencies (MIT / BSD / zlib / Apache-2.0 / FTL).

**Not the goal.** Running the game. Correct behaviour. Asset loading. That is Milestone C.
A binary that links and then exits on a missing DLL still completes Milestone B.

This plan is written against measurements taken on the current tree, not estimates.
Every number below is reproducible with two tools added alongside it:

```sh
cd modern
./tools/codegen.sh src/Eternity eternity        # compile to objects (not -fsyntax-only)
CONFIG=client ./tools/codegen.sh src/GameEngine ge_client
./tools/linkcheck.sh eternity ge_client         # classify the unresolved symbols
```

`codegen.sh` reuses `probe.sh`'s include and define tables by sourcing them, so the two
cannot drift apart.

---

## 1. Where Milestone A actually left us

Milestone A established that 405 translation units pass `-fsyntax-only` under
`-std=c++20` with no `-fpermissive`. That result holds. Re-measuring it for Milestone B
turned up three things it did not cover.

### 1.1 Codegen was never exercised — but it costs one flag

`-fsyntax-only` never emits code. Turning on real codegen (`-c`) across the whole tree:

| Tree | TUs | Objects emitted |
|---|---|---|
| Eternity (`Source/`) | 81 | 81 |
| Eternity (`SF/`, `UndoHistory/`) | 10 | 10 |
| GameEngine | 44 | 44 |
| EclipseStudio | 209 | 209 |
| WO_GameServer | 52 | 52 |
| MasterServer | 11 | 11 |
| SupervisorServer | 8 | 8 |
| **Total** | **415** | **415** |

Six files failed at first, five of them for one reason: SSE intrinsics
(`_mm_cvtss_si32`, `_mm_set_ss`) do not compile for bare `i686`, which has no SSE.
Adding **`-msse2`** fixes all five. That matches the original target — the 2013 build
assumed SSE2 — so it is a correction, not a relaxation.

The sixth, `src/Eternity/Source/r3dObj_OLDRender.cpp`, is in no `.vcxproj` and was
already on the orphan list. Nothing to do.

**Codegen is therefore a solved problem.** It was the risk I expected to dominate this
milestone and it does not.

### 1.2 Ten Eternity sources were never probed at all

`probe.sh` was pointed at `src/Eternity/Source`. `Eternity.vcxproj` also compiles ten
files outside it:

```
SF/script.cpp                    SF/CmdProcessor/CmdVar.cpp
SF/RenderBuffer.cpp              SF/CmdProcessor/CmdEvent.cpp
SF/CmdProcessor/CmdConsole.cpp   SF/CmdProcessor/CmdCommands.cpp
SF/CmdProcessor/CmdProcessor.cpp SF/Console/Config.cpp
SF/Console/EngineConsole.cpp     UndoHistory/UndoHistory.cpp
```

All ten compile and emit objects cleanly, so this is a bookkeeping fix, not new work —
but it is why `CmdVar::SetInt` and friends showed up as unresolved. The real engine
total is **91 TUs, not 81**.

### 1.3 Eternity and GameEngine were only ever checked as *server* code

This is the structural finding, and it shapes the whole milestone.

`probe.sh` gives Eternity and GameEngine `-DWO_SERVER` — deliberately, as the "smallest
surface" that would syntax-check. For a syntax check that is fine. For a link it is
wrong: `WO_SERVER` `#ifdef`s out the code the *client* calls.

Demonstrated directly — `src/Eternity/Source/UI/UIimEdit.cpp`:

| Configuration | `imgui_DrawList` symbols defined |
|---|---|
| with `-DWO_SERVER` (what Milestone A used) | 0 |
| without (the client's real configuration) | 8 |

So `Eternity` and `GameEngine` must each be **built twice** — once for the client, once
for the servers.

Measuring that previously-unverified client build:

| Tree, client configuration | Result |
|---|---|
| Eternity (91 TUs) | **88 pass / 3 fail** |
| GameEngine (44 TUs) | **42 pass / 2 fail** |

Five failures, each a distinct and bounded problem — and three of the five have a
known-good precedent already in this tree:

| File | Cause | Shape of fix |
|---|---|---|
| `r3dEternityWebBrowser.cpp` | needs `berkelium/Berkelium.hpp` | Berkelium is abandoned; the root `CMakeLists.txt` already sets `ENABLE_WEB_BROWSER=0`. Apply it, or drop the TU. |
| `r3dRender.CPP` | needs `NVApi/nvapi.h` | One more header shim, same pattern as the eight already written |
| `VehicleDescriptor.cpp` | `PxVehicleWheelsDynData::getTireLongSlip` removed in PhysX 4 | Read it from `PxWheelQueryResult` — the exact fix already applied in `obj_Vehicle.cpp` |
| `Terrain3.cpp:6225` | binds a non-const lvalue ref to an rvalue | Genuine C++20 conformance; one line |
| `r3dArenaAllocator.cpp:43` | malformed MSVC pragma construct in a client-only branch | Conformance; small |

**This was the one genuinely open question in the plan, and it came back small.** B0 is
still sequenced first, but it is hours of work, not a milestone of its own.

---

## 2. The link surface, measured

Compiling the client set to objects (Eternity + Eternity/SF + GameEngine + EclipseStudio
= 344 objects) and diffing defined against undefined symbols:

```
defined:    64,752
undefined:   4,655
unresolved:    601      # undefined minus everything the set itself defines
```

Those 601 classify cleanly. **This is the Milestone B work list.**

One caveat on how to read it: this measurement used the *server* build of Eternity and
GameEngine, because that is what `probe.sh` produces today (§1.3). It is therefore an
upper bound on the residual row — B0 fixes the configuration and a re-run should shrink
it substantially. Every other row is unaffected by the configuration.

| Class | Count | Resolved by |
|---|---:|---|
| Win32 `__imp_*` | 216 | Import libs — all present in MinGW |
| Win32 `stdcall` + GUIDs | ~90 | `d3d9 dinput8 dxguid dbghelp iphlpapi version …` |
| Third-party C++ (pugixml 49, PhysX 34, RmlUi 20, RakNet 14, Detour 12) | 129 | **Building the vendored libraries** |
| PhysX C entry points (`PxCreateFoundation`, …) | 12 | **Building PhysX** |
| zlib (`inflate`, `deflate`, …) | 6 | Compiling the 11 **C** files in `Eternity/Source/ZLib/src` |
| libgcc / libstdc++ runtime (`__cxa_*`, `__chkstk_ms`) | ~20 | Automatic |
| Residual own-code | 148 | Mostly §1.3 — the `WO_SERVER` mismatch |

Two checks worth recording because they came out better than expected:

- **Every Win32 import library the `.vcxproj` files reference is present in MinGW** —
  including `libd3dx9.a`. Nothing is missing on the Windows side.
- **Every commercial-SDK shim is fully inline and leaves zero unresolved symbols.**
  Chilkat, Steam, TeamSpeak, GameBlocks, FMOD and D3DX contribute *nothing* to the link
  gap. The shim strategy already satisfies the linker.

The RmlUi and Detour entries are worth a note: the unresolved RmlUi symbols are all
virtual methods of `Rml::RenderInterface` / `Rml::SystemInterface`, i.e. our code
derives from RmlUi's interfaces correctly and simply has no library to link against.
Same for Detour. Both are "build the library", not "fix the integration".

---

## 3. Work breakdown

Ordered so that each step de-risks the next, and so the first executable appears early.

### B0 — Build-configuration correctness *(prerequisite)*

1. Add `-msse2` to the probe and the build.
2. Add `Eternity/SF` and `Eternity/UndoHistory` to the probed source set (91, not 81).
3. **Split Eternity and GameEngine into client and server variants**, and fix the five
   client-configuration failures itemised in §1.3.
4. Compile `Eternity/Source/ZLib/src/*.c` as a C target.
5. Add an NVAPI header shim (`r3dRender.CPP`) and settle Berkelium by applying
   `ENABLE_WEB_BROWSER=0`.

*Exit:* every TU compiles in *every* configuration a binary actually uses.

### B1 — A real build system

`probe.sh` cannot link. The CMake tree exists but is a Phase-1 skeleton and is now stale:
`cmake/Dependencies.cmake` still says PhysX, Recast and RmlUi are "deliberately NOT
fetched" — all three are vendored today — and the four target `CMakeLists.txt` files it
`add_subdirectory`s (`src/Eternity`, `src/GameEngine`, `src/EclipseStudio`, `server/src`)
do not exist, so it cannot configure.

1. Refresh `Dependencies.cmake` and the root `CMakeLists.txt` to match reality.
2. Write the four missing target files.
3. **Derive source lists from the `.vcxproj` `<ClCompile>` entries, not from globs.** A
   glob pulls in the 16 orphan files that the original build never compiled. Generate
   them with a script so they stay honest.
4. Add a MinGW toolchain file.

*Exit:* `cmake --build` reproduces the 415 objects that `probe.sh` checks.

### B2 — Build the vendored dependencies

| Library | State | Effort |
|---|---|---|
| pugixml | 1 source file | trivial |
| zlib | 11 C files, in-tree | trivial |
| Recast / Detour / DetourCrowd | 30 sources, `CMakeLists` present | low |
| RakNet | 110 sources, `CMakeLists` present | low |
| RmlUi | 244 sources, needs FreeType (FTL — permissive) | medium |
| **PhysX 4.1** | **195 headers, zero sources** | **high — see §4** |

FreeType is not in the MinGW sysroot and must be built from source, or RmlUi configured
with `RMLUI_FONT_ENGINE=none` (links, renders no text — acceptable for this milestone).

### B3 — Link the binaries, smallest first

Deliberately ordered by dependency surface, so the first link is small enough to debug:

| # | Target | Own sources | Adds |
|---|---|---:|---|
| 1 | **SupervisorServer** | 8 | nothing — no PhysX, no rendering |
| 2 | **MasterServer** | 11 (+3 shared) | — |
| 3 | **GameServer** | 52 (+45 shared) | PhysX, Detour |
| 4 | **WarZ.exe** | 209 | D3D9, RmlUi, editors, everything |

Both small servers link `Eternity.vcxproj` and RakNet, so target 1 already exercises the
engine static library end to end. It is the cheapest possible proof that the layering
links.

### B4 — Residual symbols

Re-run the symbol diff after B0–B3 and work whatever survives. Expect it to be small:
most of today's 148 residual entries are §1.3 artefacts that B0 removes.

---

## 4. Risks

**PhysX 4.1 on MinGW — the one real risk.** It is vendored headers-only; the libraries do
not exist and NVIDIA's build system targets MSVC, not MinGW. In its favour:
`PxPreprocessor.h` has a real `PX_GCC` path, `PX_GCC_FAMILY` is orthogonal to
`PX_WINDOWS_FAMILY`, and the headers already compile clean under MinGW — that is genuine
evidence, but it is evidence about ~195 headers, not about ~2,000 source files that use
MSVC atomics and intrinsics.

Handle it as a timeboxed spike, with fallbacks ranked:

1. Write our own CMakeLists over the PhysX sources for `i686-mingw`.
2. Build PhysX with MSVC on Windows and link the resulting `.lib`. Breaks the
   single-toolchain story but unblocks everything.
3. Link the servers and client against a **stub PhysX** and defer the real one. The
   compat layer (`Px3xCompat.h`) is already ours, so a stub is cheap — and B3's ordering
   means SupervisorServer and MasterServer link regardless, since they build with
   `-DDISABLE_PHYSX`.

Fallback 3 is why B3 is ordered the way it is: PhysX cannot block the whole milestone.

**Secondary risks**

- *`Final` vs `Release`.* Everything measured here is the `Release`-equivalent
  configuration, with editors compiled in. `FINAL_BUILD` strips them and is a separate,
  unverified configuration. Milestone B should target `Release` and note `Final` as
  future work.
- *RSBuild and RSUpdate are not in `modern/` at all.* The asset packer and launcher
  (~9k LOC) were never bootstrapped. They are not needed to link the four main binaries;
  call them explicitly out of scope rather than discovering the gap later.
- *Static-library link order.* Four static libraries with cyclic-ish references
  (EclipseStudio ↔ GameEngine) may need `--start-group` / repeated entries under `ld`.

---

## 5. Definition of done

- [ ] `cmake --build` produces `SupervisorServer.exe`, `MasterServer.exe`,
      `GameServer.exe`, `WarZ.exe`
- [ ] Zero unresolved symbols in all four links
- [ ] No `-fpermissive`; `-std=c++20` throughout
- [ ] No dependency outside MIT / BSD / zlib / Apache-2.0 / FTL
- [ ] `modern/README.md` milestone table updated; `PORTING-LESSONS.md` carries the
      lessons from §1.3 and §2
