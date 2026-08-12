# Milestone C — pre-work

**Milestone C, as `PHASE1-BUILD-PLAN.md:170` defines it:**

- Server: boots, parses config, loads a level via `LoadLevel_Objects`, enters the
  `PlayGameServer` tick loop, accepts a socket connection
- Client: reaches `game::MainLoop` with a null render pipeline without faulting

Milestone B ended with four linked PE32 binaries and zero unresolved symbols. Nothing
after that has been executed. This document is the gap between "links" and "can be
started at all", split into three parts: what blocks C from *starting*, which of our
stubs can be *completed*, and what to do first.

## 0. What this is measured against, and what it is not

Everything below is from reading the tree as it stands on
`claude/milestone-c-pre-work-b5awvh`. **Nothing here was run.** That is not a
disclaimer, it is the first finding — see §1.2. Where a claim is a static-analysis
inference rather than an observation, it says so.

---

## 1. Blockers — these come before any stub work

### 1.1 `tools/bootstrap.sh` will destroy the entire port — fix this first

`modern/tools/bootstrap.sh:62-71` does `rm -rf "$dst"` on each of `src/Eternity`,
`src/GameEngine`, `src/EclipseStudio`, `src/ServerNetPackets` and `server/src`, then
re-copies from `../src` and `../server`. Its comment says this is safe "because every
path in TREES/VENDORED is a pure copy target".

**That is no longer true.** Milestones A and B edited those trees in place:

| Tree | Files differing from the original, or new |
|---|---:|
| `src/Eternity` | 57 |
| `src/GameEngine` | 46 |
| `src/EclipseStudio` | 179 |
| `server/src` | 66 |
| **Total** | **348** |

115 of them carry `[PORT]` markers. The original `../src` and `../server` contain
**zero** `[PORT]` markers — the port exists only in `modern/`. Running `bootstrap.sh`
today deletes all of it, and `modern/README.md` still lists it as step 1 of the quick
start.

`PHASE1-BUILD-PLAN.md:202` already anticipated this: *"Cut the fork deliberately once
Milestone C passes, and record the commit."* The condition is now inverted — the fork
has been cut de facto, and the only thing that has not happened is admitting it. Do it
before Milestone C, not after.

**Fix (S):** make `bootstrap.sh` refuse to run against a diverged tree, or reduce it to
`--dry-run`-only with a printed diff; drop it from the README quick start; record the
fork commit in `modern/README.md`.

### 1.2 Neither the toolchain nor a Windows runtime is installed here

```
i686-w64-mingw32-g++   not installed  (g++-mingw-w64-i686 13.2.0 available via apt)
wine                   not installed  (wine 9.0 available via apt)
cmake 3.x, ninja       present
modern/build*/         absent
```

The Milestone B binaries are not in the tree (`.gitignore` excludes `modern/build*/`)
and cannot currently be rebuilt or run in this container. Milestone C is defined
entirely in terms of running things, so this is a hard prerequisite, not an
inconvenience.

**Fix (S):** add a `.claude/` SessionStart hook or a `modern/tools/setup-env.sh` that
installs `g++-mingw-w64-i686` and `wine32`, then verifies with a one-line PE32 hello
world under Wine. Confirm `wine` can actually run a 32-bit PE here (needs i386
multiarch) before relying on it.

**Partly resolved.** `g++-mingw-w64-i686` installs cleanly from apt and the four
binaries do still link on the current tree — that much is now confirmed rather than
assumed. **Wine is still not installed**, so nothing has been *run*, and every claim
about runtime behaviour in this document remains static analysis.

### 1.3 There is no game data in the repository

```
find . -name LevelData.xml -o -name ServerData.xml -o -name '*.wz'   → nothing
bin/  →  game.ini, local.ini, light_presets.ini, MasterServer.cfg,
         SupervisorServer.cfg, crashrpt_lang.ini   (and nothing else)
```

`.gitignore` excludes `bin/*` except the `.ini`/`.cfg` files. The server's Milestone C
criterion — *loads a level via `LoadLevel_Objects`* — reads `LevelData.xml`,
`SoundData.xml` and `ServerData.xml` out of a level directory
(`modern/server/src/WO_GameServer/Sources/ServerGame.cpp:83-110` maps map IDs to
`Colorado_V2`, `California_V2`, `WZ_Cliffside`, `Caliwood`, `WorkInProgress\ServerTest`).
None of those directories exist.

This is the single largest unowned dependency in Milestone C and it cannot be solved by
writing code. Options, in order of preference:

1. **Source the original game data** from one of the community re-releases the `Doc/`
   guides are written against, and mount it at `bin/Data`. Keep it out of git.
2. **Author a minimal synthetic level** — an empty `LevelData.xml` plus a flat terrain —
   purely to exercise the load path. Cheaper to obtain, and arguably a better
   Milestone C test because it isolates the loader from asset-format drift. But it needs
   `RSBuild` (see §1.7) or a raw-directory load path.

**Decide this before starting C.** The rest of the milestone's shape depends on it.

### 1.4 The three server binaries cannot be started independently

`ServerGame.cpp:215-220`:

```cpp
if(gMasterServerLogic.IsMasterDisconnected()) {
    r3dOutToLog("Master Server disconnected, exiting\n");
    ...
    return;
}
```

`GameServer` exits as soon as the MasterServer link drops, and
`ServerMain.cpp:330-410` requires `--gameId` / port arguments that `SupervisorServer`
normally supplies when it spawns the process. So the boot order is
**MasterServer → SupervisorServer → GameServer**, and a Milestone C run script has to
orchestrate all three.

**Fix (M):** write `modern/tools/run-servers.sh` that starts the three under Wine in
order, with a local `MasterServer.cfg`, and tails their logs. Consider adding a
`--standalone` flag to GameServer that skips the master handshake — it is a small,
well-isolated change and it makes every later debugging session cheaper.

### 1.5 The GameServer's player paths all go through the HTTP backend

Every persistence operation is a `CWOBackendReq` against the ASP.NET API:

```
Async_ServerState.cpp     api_SrvSavedState.aspx
Async_ServerObjects.cpp   api_SrvObjects.aspx      (5 call sites)
AsyncFuncs.cpp            api_SrvUserGame.aspx, api_SrvBanUser.aspx,
                          api_SrvCharUpdate.aspx, api_CharBackpack.aspx,
                          api_SrvAddLogInfo.aspx
Backend/ServerUserProfile.cpp
```

`CWOBackendReq` is built on Chilkat. A server that boots and ticks satisfies Milestone
C's letter; a player who connects and gets a character does not, and that is the first
thing anyone will try.

Two dependencies here, and they are separable:

- **The Chilkat shim** — **done** (§2.1). Requests are now really issued, over WinHTTP.
  What that buys is that the transport is no longer the reason these fail.
- **The backend itself** — `web/WZBackend-ASP.NET/` plus the MSSQL schema in `db/`.
  Needs a Windows/IIS + SQL Server host, or a stand-in service that answers the ~10
  `api_Srv*.aspx` endpoints above with the response format `ParseResult`
  (`WOBackendAPI.cpp:104`) expects. **The stand-in is very likely the cheaper path** and
  it is also the thing that lets the whole stack run on one Linux box.

### 1.6 Hardcoded IPs, still

`modern/src/EclipseStudio/Sources/Main.cpp:1499`, `:1509`, `:1513` — `192.168.21.100`
for the item DB, the game server and the backend API, each tagged `// IP`. Whatever
Milestone C runs against, these have to come from `game.ini` / the command line first,
or every test run is a rebuild.

**Fix (S):** route all three through existing console vars with the current values as
defaults.

### 1.7 Out of scope but adjacent, and worth naming now

- **`RSBuild` and `RSUpdate` are not in `modern/` at all.** `MILESTONE-B-PLAN.md:§4`
  called them explicitly out of scope. `RSBuild` is the asset packer that produces `.wz`
  archives, so if §1.3 is resolved by authoring a synthetic level, `RSBuild` — or a raw
  directory load path — comes back into scope.
- **`FINAL_BUILD` is unverified.** The `final` preset exists and has never been built.
  Leave it out of Milestone C; do not let it be discovered as a surprise later.
- **ASan is not available** on the MinGW target. `PHASE1-BUILD-PLAN.md:174` asks for
  "both under ASan where the toolchain allows" — MinGW does not. Substitute
  `-fstack-protector-strong` plus Wine's own heap checking, or accept the gap and record
  it.

---

## 2. Stubs that can be completed

Ordered by leverage. Sizes are S / M / L in the same spirit as the Milestone B plan.

> **Status: §2.1, §2.2, §2.3, §2.5 and §2.6 are done.** The implementations live in
> `modern/src/External/compat/` (HTTP, DDS loading, shader compilation, crash dumps)
> and in `PhysXWorld::Init` (PVD). All four binaries still link; sizes and the
> remaining limits are recorded in `modern/README.md`. §2.4 (RmlUi rendering), §2.7
> (Recast navmesh) and §2.8 remain, as does everything in §1 — which is still what
> actually gates the milestone.

### 2.1 Chilkat → WinHTTP + zlib — **the one to do first** (M) — ✅ done

**Why it is first:** it is the only stub that blocks *both* binaries' real work. The
client's entire login and menu flow runs through `CWOBackendReq`
(`WOBackendAPI.cpp`), and so does every server persistence path in §1.5.

**Why it is tractable:** the surface is 139 lines
(`modern/src/External/ChilKat/Include/CkShimCommon.h`) and it is already a clean-room
declaration derived from call sites, so the shape is known and correct. What is missing
is the bodies:

| Class | What it needs |
|---|---|
| `CkHttp::SynchronousRequest`, `quickGetStr`, `getDomain` | WinHTTP (`winhttp.dll`, ships with Windows, no new dependency and no licence question). `getDomain` is URL parsing. |
| `CkHttpRequest` | Accumulate method / path / params / headers / multipart body; it is a builder, nothing more |
| `CkHttpResponse` | Status, body, headers off the WinHTTP response |
| `CkGzip::UncompressMemory` / `CompressMemory` | **zlib is already vendored in-tree** at `src/Eternity/Source/ZLib/src` and already built as a C target |
| `CkString::base64Decode` / `base64Encode` | ~40 lines. Currently returns `false`, which breaks the launcher's auth-token unpack at `FrontEndWarZ.cpp:353` |
| `CkByteData::loadFile` / `saveFile` | Trivial; currently always fails, which is what disables the log uploader |

libcurl was the choice recorded in `DEPENDENCIES.md`. WinHTTP is worth reconsidering:
the target is Windows-only, it is already on every target machine, and it adds nothing
to the licence audit. libcurl is the better call only if HTTPS-to-a-modern-TLS-stack
turns out to matter.

**Caution:** the shim contract in `src/External/README.md` forbids "silently succeeding
where the real library would do meaningful work". Completing this stub means it stops
being a shim. Move it out of the no-op table in that README and in
`modern/README.md`'s shim table when it lands.

### 2.2 D3DX texture loading (M) — client only — ✅ done

`src/External/dxsdk/Include/d3dx9.h:854-923` — every image entry point returns
`E_NOTIMPL`. The math half of this shim is real; the imaging half is not.

The good news is the funnel: the whole engine loads textures through exactly two
functions.

```
r3dTex.cpp:667, 1844        D3DXGetImageInfoFromFileInMemory
r3dRender.CPP:5923          D3DXCreateTextureFromFileInMemoryEx   ← the single seam,
                                                                     via r3dDeviceTunnel
```

Implement those two and the cube/volume variants and every texture in the game loads.
WarZ assets are DDS, so the bulk of it is a DDS header parse handing DXT blocks
straight to `IDirect3DDevice9::CreateTexture` + `LockRect` — no decode, no dependency.
`stb_image` (already fetched by `cmake/Dependencies.cmake`) covers PNG/TGA/JPG for the
editor paths.

**Not needed by the servers**: `ServerMain.cpp:348-356` sets `r3dTexture_UseEmpty = 1`,
`_r3d_MatLib_SkipAllMaterials = 1` and `r3dMeshObject_SkipFillBuffers = 1`. Server-side
Milestone C is unaffected by this gap — which is a good argument for doing the server
half of Milestone C first.

### 2.3 D3DXCompileShader → `D3DCompile` (S) — better than it looks — ✅ done

`d3dx9.h:983-1001` stubs the compiler. The replacement is nearly a drop-in:

- The engine targets **`vs_3_0` / `ps_3_0`** (`r3d.h:693-694`), which
  `d3dcompiler_47.dll` still supports.
- **No call site anywhere uses `ID3DXConstantTable`.** `VShader.cpp:124` and
  `PShader.cpp` both pass `NULL` for the constant-table parameter, and a grep for
  `LPD3DXCONSTANTTABLE` across `src/Eternity`, `src/GameEngine` and
  `src/EclipseStudio` returns only the shim's own declaration. That removes the one
  genuinely hard piece of a D3DX-compiler replacement.
- `ID3DXBuffer` → `ID3DBlob` (same vtable shape: `GetBufferPointer` / `GetBufferSize`).
- `r3dDXInclude` (`VShader.cpp:60-88`) already implements the include interface; it maps
  onto `ID3DInclude` unchanged.
- `D3DXMACRO` → `D3D_SHADER_MACRO`, layout-identical.

Both open questions resolved during implementation. MinGW-w64 does ship
`libd3dcompiler.a` — but it is not used: the DLL is loaded with `LoadLibrary` at first
use, probing `_47` down to `_42`, so the binary carries no import for it and a missing
DLL becomes a logged error rather than a process that will not start.
`D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY` is set unconditionally, on the assumption
that 2013-vintage HLSL needs it; that assumption is unverified until real shaders are
compiled.

There is also a **shader binary cache** already in the engine
(`VShader.cpp:176` `LoadBinaryCache`, `:276` `SaveBinaryCache`). Once the compiler
works, a warm cache means later runs never invoke it — and a pre-populated cache is a
plausible fallback if the compiler route stalls.

### 2.4 RmlUi `RenderInterface` (M) — mechanical, but the content is not

`src/GameEngine/RmlUiIntegration/RmlUiMovie.cpp:60-97`. Every method is a documented
`SEAM`. The file itself says what to write: `CompileGeometry` builds an
`r3dVertexBuffer`/`r3dIndexBuffer` pair, `RenderGeometry` submits it, `LoadTexture`
routes through `r3dTexture`, scissor maps to a D3D9 scissor rect. That is a day of
straightforward renderer work and it depends on §2.2 for textures.

**But rendering is not the blocker for UI — content is.** `APIScaleformGfx.cpp:22-25`:
every screen is an authored `.swf`, nothing converts Flash to RML, so documents fail to
load until each screen is re-authored by hand. Completing the render interface gets you
a working UI runtime with nothing to display.

**Implication for Milestone C:** the client's route to `game::MainLoop` runs through
`ExecuteNetworkGame`'s login/menu state machine (`Main_Network.cpp:175`), which is
driven by those screens. Milestone C's client criterion is very likely unreachable
through the front end. Plan on a direct-connect bypass — a command-line flag that skips
the menu and calls into `PlayNetworkGame` with a server address — as part of Milestone C
itself, not as a workaround.

### 2.5 CrashRpt (S) — small, and it pays for itself immediately — ✅ done

96-line shim. `MiniDumpWriteDump` from `dbghelp`, which is already in the link line.
A milestone whose whole purpose is running binaries that have never run wants crash
dumps on day one.

### 2.6 PhysX PVD (S) — ✅ done

`modern/README.md` records it: PhysX 4 requires `PxPvd` to exist *before*
`PxCreatePhysics`, so restoring it means reordering `PhysXWorld::Init()`. The exact
sequence is already written out in a comment at the call site. Physics debugging during
Milestone C is worth the hour.

### 2.7 Recast navmesh build and load (L) — real, but not for Milestone C

`src/GameEngine/ai/RecastNav/RecastNavMesh.cpp` — `LoadPathData` (`:133`),
`BuildForCurrentLevel` (`:182`), the two obstacle entry points (`:216`, `:223`) and
debug draw (`:400`) are all `SEAM`. Detour is vendored and linked; nothing loads or
builds a mesh, so zombies have no navigation at all.

Note that the dead `AutodeskNav/AutodeskNavMesh.cpp` still loads `*.NavData` — Kynapse's
binary format. It is not in `cmake/sources/GameEngine.cmake` and is reference-only, but
it means **original game navmesh assets are useless to Detour** even once §1.3 is
resolved. This needs either an offline Recast cook or a `.NavData` → Detour converter.

Out of scope for C — the server ticks fine without it — but it is genuinely completable
work and it is the largest remaining functional hole after UI.

### 2.8 Minor, while you are in the area (S each)

- `PxConvexMeshDesc::triangles` removal — hulls are currently built from points alone.
- `MeshPropertyLib::AddEntry` and friends `r3dError("not implemented")` in the server's
  link-stub block (`ServerGame.cpp:73-77`) — inherited from the original, but they are
  live `r3dError` calls in a binary that is about to be run for the first time. Worth
  confirming they are unreachable rather than assuming.

---

## 3. Stubs that should stay stubbed

Completing these is either impossible without proprietary assets or simply not what
Milestone C is about.

| Stub | Why it stays |
|---|---|
| **FMOD** | 136 call sites go through the **Event** system (`.fev` / `.fsb` designer data), not raw sound playback. A permissive backend (miniaudio, OpenAL-soft) replaces the mixer but not the event runtime or the proprietary bank formats. This is a project, not a stub. Silence is the correct Phase 1 outcome. |
| **TeamSpeak 3** | VOIP, client *and* server. No Milestone C criterion touches it. |
| **GameBlocks / FairFight** | `GBClient::Connected()` returns false and every guarded call site skips. Anti-cheat on a private port is a non-goal. |
| **Steamworks** | Standalone is the intended configuration. |
| **NVApi / ATI Crossfire** | SLI/Crossfire detection. Returns "not present", which is true. |
| **PunkBuster / VMProtect** | Compiled out upstream (`__WITH_PB__`, `USE_VMPROTECT` undefined). Leave them out. |

---

## 4. Suggested order

The sequencing principle: **make the server half of Milestone C reachable before
touching anything client-side.** The server needs no textures, no shaders and no UI
(§2.2), so it is a much shorter path to the first binary that has actually run.

**Phase 0 — stop the bleeding (hours)**
1. Neuter `bootstrap.sh`; record the fork commit (§1.1)
2. Install the toolchain and Wine; re-verify all four binaries still link (§1.2)
3. Decide the game-data question (§1.3)

**Phase 1 — server reaches the tick loop (days)**
4. `run-servers.sh` + optional GameServer `--standalone` (§1.4)
5. Configurable IPs (§1.6)
6. CrashRpt (§2.5) — do this before the first run, not after the first mystery crash
7. Run. Record where it faults. *Everything above this line is guesswork until this
   step produces a log.*

**Phase 2 — the server is actually usable (weeks)**
8. ~~Complete the Chilkat shim (§2.1)~~ — **done**
9. Stand up a backend, real or stand-in (§1.5)

**Phase 3 — client (weeks)**
10. ~~D3DX texture loading (§2.2)~~ — **done**
11. ~~`D3DXCompileShader` → `D3DCompile` (§2.3)~~ — **done**
12. Direct-connect bypass around the front end (§2.4)
13. RmlUi render interface (§2.4) — only once there is something to draw

Items 8 and 10–11 were the ones independent of item 7's outcome, which is why they were
taken first: none of them needed a running binary to write, and all four binaries still
link with them in. **Everything still outstanding needs something that cannot be
written** — a Windows runtime, game data, or a backend.

## 5. Ready-to-start-C checklist

- [ ] `bootstrap.sh` can no longer destroy the port, and the fork commit is recorded
- [ ] Toolchain and Wine installed and reproducible; a PE32 i386 hello world runs
- [ ] All four Milestone B binaries re-link on the current tree
- [ ] Game data sourced, or a synthetic level authored and loadable
- [ ] Server IPs and ports configurable without a rebuild
- [ ] The three servers can be started in order by one script
- [ ] Crash dumps land somewhere on fault

Only the first three are strictly prerequisite. The rest determine whether Milestone C
is a week or a quarter.

---

## Related documents

| Document | Covers |
|---|---|
| [`PHASE1-BUILD-PLAN.md`](PHASE1-BUILD-PLAN.md) | Phase 1 scope; the Milestone C criteria quoted above |
| [`MILESTONE-B-PLAN.md`](MILESTONE-B-PLAN.md) | Milestone B (linking) — measured work list and outcome |
| [`modern/README.md`](modern/README.md) | Port status, shim table, build instructions |
| [`DEPENDENCIES.md`](DEPENDENCIES.md) | Dependency audit and replacement choices |
