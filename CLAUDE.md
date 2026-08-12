# CLAUDE.md

Guidance for Claude Code when working in this repository.

## What this is

Leaked source of **Infestation: Survivor Stories** (formerly *The War Z*), Hammerpoint
Interactive / OP Productions, circa 2013–2014. Windows / DirectX 9, C++, originally built
with Visual Studio 2008 (the `.vcxproj` files were later upgraded to the v120 / VS2013
toolset; both `.vcproj` and `.vcxproj` forms are checked in).

The repo bundles four separately-built products: the game client, three server binaries,
the launcher/patcher, and an ASP.NET/PHP web backend.

**Total: ~370k lines of C++.**

## Repository layout

```
src/Eternity/          The "r3d" engine core          (~89k LOC)
src/GameEngine/        Object model, physics, terrain (~61k LOC)
src/EclipseStudio/     Game client + editors          (~175k LOC)
src/RSUpdate/          Launcher / patcher / login     (~6k LOC)
src/RSBuild/           Asset packer (builds .wz)      (~3k LOC)
src/ServerNetPackets/  Packet structs shared c/s      (~0.5k LOC)
src/External/          Third-party libs (MOSTLY ABSENT — see Gotchas)
server/src/            GameServer, MasterServer, SupervisorServer (~38k LOC)
web/WZBackend-ASP.NET/ Backend API (accounts, shop, characters)
db/                    MSSQL schema dumps
bin/                   Runtime configs (game.ini, MasterServer.cfg, ...)
Doc/                   Chinese-language setup guides for two community re-releases
```

### Layering

`Eternity` → `GameEngine` → `EclipseStudio` / `server`

- **Eternity** is the older, generic engine. Everything is prefixed `r3d`
  (`r3dRender`, `r3dMesh`, `r3dTex`, `r3dSkeleton`, `r3dNetwork`).
- **GameEngine** adds `GameObject` / `ObjectManager`, PhysX, terrain, sky, AI/navmesh, FMOD.
- **EclipseStudio** is the actual WarZ game *and* all the editors, in one executable.
- **The server reuses GameEngine and parts of EclipseStudio**, compiled with `WO_SERVER`
  defined. `server/src/WO_GameServer/Sources/ServerGame.cpp:44-80` stubs out every
  rendering global so the server links against the deferred-renderer headers without
  ever drawing.

## Solutions and build order

| Solution | Builds | Configs |
|---|---|---|
| `src/RSBuild.sln` | Asset packer | Release |
| `src/RSUpdate.sln` | Launcher / updater | Release |
| `src/Eternity/Eternity.sln` | Engine static lib | Debug / Release / Final |
| `src/EclipseStudio/WarZ.sln` | Game client + editors | Debug / Release / **Final** |
| `server/src/WarZ_Server.sln` | 3 server exes | Debug / Release |
| `web/WZBackend-ASP.NET/...sln` | Backend API | Debug |

`Final` is the shipping client config: strips editors, enables the single-instance guard,
caps FPS at 60, hardcodes server IPs.

Per the community docs, build order is RSBuild → RSUpdate → WarZ_Server → WarZ → backend,
then run `bin/RSBuild.exe` to pack assets and `CreateUpdater.bat` to produce the launcher.

## Architecture

### Object model

Everything derives from `GameObject` (`src/GameEngine/gameobjects/GameObj.h:243`), which
inherits `AObject` — a hand-rolled reflection system. Classes register themselves by name
via macros in `AObject.h:22-49`:

```cpp
DECLARE_CLASS(obj_Foo, GameObject)      // in the header
IMPLEMENT_CLASS(obj_Foo, "obj_Foo", "")  // in the .cpp
AUTOREGISTER_CLASS(obj_Foo)              // adds to the global class table
```

This is what lets `LevelData.xml` name a class as a string and have it constructed at load.

`ObjectManager` (`src/GameEngine/gameobjects/ObjManag.h:82`) owns the world:

- **Two pools.** Dynamic objects in a linked list + ID array (`OBJECTMANAGER_MAXOBJECTS`
  = 16383). Static objects in a flat array (`OBJECTMANAGER_MAXSTATICOBJECTS` = 49152).
- **Statics update round-robin**, capped at 2048 per frame (`ObjManag.cpp:1082-1136`).
- **Deferred deletion.** `setActiveFlag(0)` schedules removal; `EndFrame()` counts down
  to `-4` over four frames before the destructor runs (`ObjManag.cpp:1161-1204`). Assume
  a pointer stays valid for the rest of the frame it was deleted in.
- **Spatial partition** is an octree of `SceneBox` nodes, 25 objects per node
  (`sceneBox.h`), used for frustum culling, occlusion queries and ray casts.

### Client frame

`WinMain` → `game::PreInit` → `win::Init` → `game::Init` → `game::MainLoop`
(`src/Eternity/Source/WinMain.cpp:582`).

`game::MainLoop` (`src/EclipseStudio/Sources/Main.cpp:1388`) picks a mode — network game,
level editor, particle editor, physics editor — then `ExecuteNetworkGame`
(`Main_Network.cpp:173`) runs a `goto`-driven login/menu state machine before entering
`PlayNetworkGame`'s loop.

The per-frame work is `GameStateGameLoop()` (`src/EclipseStudio/Sources/Game.cpp:532`):

```
window messages + net ticks  →  HUD input  →  camera
→ GameWorld().Update()  →  decals
→ PhysX StartSimulation()          ← kicked off BEFORE rendering
→ grass / environment / wind  →  sound
→ GameRender()  →  post-process
→ PhysX EndSimulation()            ← fetched AFTER, to overlap with GPU
→ navmesh  →  highlight + Flash UI passes  →  present  →  FPS limiter
```

Particles and zombies are pulled out of the serial update loop and dispatched to a worker
pool via `JobChief::Exec` (`ObjManag.cpp:1146-1154`). Nothing else is parallel.

### Renderer

Deferred, D3D9, in `src/EclipseStudio/Sources/RENDERING/Deffered/RenderDeffered.cpp`
(~10k lines). Render stages are an enum of 20 passes (`src/Eternity/Include/r3d.h:317`):
3 cascaded shadow slices + a transparent slice, G-buffer fill (with a separate
first-person pass so viewmodels don't clip), composite, distortion, transparents,
highlight stencil passes, Scaleform UI.

`r3dDefferedRenderer::Render()` (`RenderDeffered.cpp:7927`) order: grass/water/terrain-atlas
prep → G-buffer → SSAO (two-phase, optional double-depth + temporal filter) → lighting
(sun, point, plane, tube, spot, volume) → misc → volume effects → composite → distortion.

Post-processing is a chain of **44 `PFX_*` classes** driven by `PostFXChief`.

**Objects do not draw themselves.** They append `Renderable` structs — a 64-byte blob with
an `INT64 SortValue` and a function pointer (`r3d.h:297`) — into per-stage arrays that get
sorted and flushed. That's what the `AppendRenderables` / `AppendShadowRenderables`
virtuals on `GameObject` are for.

### Networking

RakNet, wrapped by `r3dNetwork` (`src/Eternity/Include/r3dNetwork.h`) behind an
`r3dNetCallback` interface (connected / disconnected / data).

Packets are `#pragma pack(1)` POD structs tagged by the `pkttype_e` enum — roughly 200
message types in `src/EclipseStudio/Sources/multiplayer/P2PMessages.h`. Direction is
encoded in the name:

- `PKT_C2S_*` — client to server
- `PKT_S2C_*` — server to client
- `PKT_C2C_*` — client to client, **relayed through the server**

`P2PNET_VERSION` (`P2PMessages.h:25`) must be bumped whenever the client binary changes,
or PunkBuster breaks.

**Architecture is authoritative-server.** Three server binaries:

- **MasterServer** — server browser, matchmaking, user sessions
- **SupervisorServer** — per-machine process babysitter; spawns and watches GameServer
  instances, uploads logs
- **GameServer** — one process per game instance. `ServerGameLogic`
  (`server/src/WO_GameServer/Sources/ServerGameLogic.h:30`) holds a peer table with a
  state machine (`PEER_FREE → CONNECTED → VALIDATED1 → LOADING → PLAYING`), up to 512
  players.

**Interest management is distance-based.** Every networked object has an `INetworkHelper`
(`NetworkHelper.h:67`) with a per-peer visibility byte array plus `distToCreateSq` /
`distToDeleteSq` hysteresis radii. Each tick the server walks all objects per player and
sends create/destroy packets on threshold crossings
(`ServerGameLogic.cpp:2393-2459`). O(players × objects), no spatial index.

**Movement** uses cell-relative compression (`CNetCellMover`, `multiplayer/NetCellMover.h`):
the server sets a cell origin with `PKT_C2C_MoveSetCell`, then streams deltas via
`PKT_C2C_MoveRel` with turn/bend angles quantized to single bytes.

Server loop is `Sleep(10)` → ~100 Hz (`server/src/WO_GameServer/Sources/ServerGame.cpp:176-333`),
with a performance report logged if a frame exceeds 90 ms.

## Conventions

- **`r3d` prefix** = Eternity engine code. **`obj_` prefix** = a `GameObject` subclass.
  **`sobj_` / `obj_Server`** prefix = the server-side counterpart.
- **`game_new` / `gfx_new`** are the tracked allocators. Prefer them over bare `new`.
- **`r3dOutToLog`** for logging, **`r3d_assert`** for assertions, **`r3dError`** for fatal.
- **`R3DPROFILE_START/END/FUNCTION`** wrap profiled scopes; they compile out when
  `DISABLE_PROFILER` is set.
- **`r3dSec_type<T, key>`** (`src/Eternity/Include/r3dProtect.h:38`) is an XOR-obfuscated
  value wrapper used on ~29 hot fields — object list heads, counts, class pointers — so
  cheat scanners can't find them. **It costs a union copy plus 4 XORs on every read**,
  and it's applied to `pNextObject`, so every linked-list step pays it.
- **`r3dSec_string`** does the same for usernames.

### Important preprocessor switches

| Define | Meaning |
|---|---|
| `WO_SERVER` | Building server-side; strips all rendering |
| `FINAL_BUILD` | Shipping client; strips editors and debug UI |
| `USE_VMPROTECT` | VMProtect obfuscation (**disabled throughout this drop**) |
| `__WITH_PB__` | PunkBuster SDK |
| `MISSIONS`, `VEHICLES_ENABLED` | Optional gameplay systems |
| `ENABLE_AUTODESK_NAVIGATION` | Kynapse navmesh for zombie pathing |
| `APEX_ENABLED` | PhysX APEX destruction |

## Gotchas

**The repo does not build as checked out.** `.gitignore` excludes `src/External/*`
(everything except RakNet), `src/Tools`, `server/GameBlocksSDK`, and most of `bin/`.
Missing third-party dependencies include **PhysX 3, Scaleform GFx, FMOD, Autodesk
Navigation (Kynapse), PunkBuster, VMProtect, Chilkat (`CkHttp`), and the GameBlocks /
FairFight SDK.** You cannot compile without sourcing these separately.

**Hardcoded IPs.** `192.168.21.100` is baked into `src/EclipseStudio/Sources/Main.cpp:1490-1504`
for both the game server and the backend API. The community setup docs in `Doc/` are
largely a find-and-replace exercise over these. Grep for the `// IP` comment marker.

**The client exe is also the editor.** Level, particle, physics and character editors are
`#ifndef FINAL_BUILD` branches in the same binary, using an immediate-mode UI (`imgui_*`)
with its own undo/redo system in `src/Eternity/UndoHistory/`.

**Two terrain implementations coexist** — `GameEngine/TrueNature` (legacy) and
`GameEngine/TrueNature2/Terrain3.cpp` (current, selected by `r_terrain3`).

**Level data is XML, parsed at runtime.** `LoadLevel_Objects`
(`src/EclipseStudio/Sources/GameLevel_IO.cpp:159`) reads `LevelData.xml`, `SoundData.xml`
and `ServerData.xml` with pugixml on every load, constructing objects by string-name
lookup in the `AObject` class table.

**Anti-cheat is layered and invasive**: PunkBuster, GameBlocks/FairFight (including a
server-side aimbot detector fed every player and zombie position each tick,
`ServerGameLogic.cpp:4194-4237`), HackShield, VMProtect hooks, `r3dSec_type` field
obfuscation, encrypted usernames, server-side shot accounting, and remote screenshot
capture (`PKT_C2S_ScreenshotData`).

**Some files are enormous** — `AI_Player.CPP` is ~10k lines, `RenderDeffered.cpp` ~10k.
Read targeted ranges rather than whole files.

**Implementation lives in some `.hpp` files** (`LoadWorld.hpp`, `CollMain.hpp`,
`RenderDefferedScene.hpp`, `DrawWorld.hpp`) which are `#include`d into a single
translation unit, not compiled separately.

## Where to look

| Task | Start here |
|---|---|
| Client entry / init | `src/Eternity/Source/WinMain.cpp`, `src/EclipseStudio/Sources/Main.cpp` |
| Per-frame loop | `src/EclipseStudio/Sources/Game.cpp:532` |
| Object lifecycle | `src/GameEngine/gameobjects/ObjManag.cpp` |
| Rendering | `src/EclipseStudio/Sources/RENDERING/Deffered/RenderDeffered.cpp` |
| Post effects | `src/EclipseStudio/Sources/RENDERING/Deffered/PFX_*.{h,cpp}` |
| Packets | `src/EclipseStudio/Sources/multiplayer/P2PMessages.h` |
| Client net logic | `src/EclipseStudio/Sources/multiplayer/ClientGameLogic.cpp` |
| Server net logic | `server/src/WO_GameServer/Sources/ServerGameLogic.cpp` |
| Server frame loop | `server/src/WO_GameServer/Sources/ServerGame.cpp:176` |
| Player (client) | `src/EclipseStudio/Sources/ObjectsCode/AI/AI_Player.{H,CPP}` |
| Player (server) | `server/src/WO_GameServer/Sources/ObjectsCode/obj_ServerPlayer.{h,cpp}` |
| Zombies | `.../Gameplay/obj_Zombie.{h,cpp}`, `.../ObjectsCode/Zombies/` (server) |
| Weapons / items | `src/EclipseStudio/Sources/ObjectsCode/WEAPONS/` |
| Level load/save | `src/EclipseStudio/Sources/GameLevel_IO.cpp` |
| Editors | `src/EclipseStudio/Sources/Editors/` |
| Console vars | `src/Eternity/SF/Console/Vars.h`, `SF/CmdProcessor/` |

## Related documents

- `PERFORMANCE-OPTIMIZATION-PLAN.md` — analysis of the engine's performance
  characteristics and a phased modernization plan.
</content>
</invoke>
