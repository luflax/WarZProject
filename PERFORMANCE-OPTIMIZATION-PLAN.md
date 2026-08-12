# Performance Optimization Plan

A phased plan for modernizing the Infestation: Survivor Stories engine for throughput.

**Status: proposal.** Nothing here has been implemented.

---

## Method and honesty about the numbers

This plan comes from **reading the source, not from profiling it**. The codebase does not
build as checked out (see *Gotchas* in `CLAUDE.md` — most third-party dependencies are
absent), so no measurements were taken.

The impact estimates below are order-of-magnitude judgements based on well-understood
costs — D3D9 draw call overhead, cache-miss rates on pointer-chased polymorphic data,
O(n·m) loop complexity. They are directionally reliable and numerically approximate.

**Phase 0 is to instrument and profile.** Do not commit engineering time to any item
below on the strength of this document alone.

---

## Summary: ranked by impact

| # | Workstream | Est. impact | Effort | Risk |
|---|---|---|---|---|
| 1 | Modern graphics API + GPU-driven rendering | 10–100x draw throughput | XL | High |
| 2 | Data-oriented object storage (ECS) | 5–20x on CPU update paths | XL | High |
| 3 | Spatial interest management (server) | 100x+ on server tick | M | Low |
| 4 | Fixed timestep + rewind hit registration | Moderate perf, large anti-cheat gain | L | Medium |
| 5 | Full-frame job parallelism | 3–6x on multicore | L | Medium |
| 6 | Binary mmap'd assets | Load time, server startup | M | Low |
| 7 | Remove per-access field obfuscation | Small, essentially free | S | Low |
| 8 | Replace Scaleform UI | Moderate frame-time win | M | Medium |

Items 3, 6 and 7 are the cheapest wins with the least risk. **Start there**, regardless of
whether the larger rewrites ever happen.

---

## 1. Graphics API and rendering pipeline

### Problem

D3D9 is the ceiling. Each draw call costs roughly 1–2 µs of driver CPU time, so with
thousands of objects the frame is CPU-bound in the driver before the GPU begins work.

The entire `Renderable` design (`src/Eternity/Include/r3d.h:297` — a 64-byte blob holding
an `INT64 SortValue` and a function pointer, appended per object, sorted, then flushed)
is an elaborate CPU-side workaround for an API that cannot batch.

Secondary costs:

- **Fat G-buffer.** Color + normal + depth + aux + optional secondary depth. Heavy
  bandwidth, and this game is full of small lights (flashlights, chemlights, flares,
  muzzle flashes) — exactly the case where deferred stops paying for itself.
- **SSAO is four passes.** Double-depth + temporal filter + blur + finalize
  (`RENDERING/Deffered/RenderDeffered.cpp:8032-8130`) to produce one AO term.
- **44 post-process passes.** `PFX_*` classes chained as full-screen render-target
  ping-pongs through `PostFXChief`.

### Change

Rewrite the renderer as **GPU-driven**:

- Persistent vertex/index buffers; geometry uploaded once
- Bindless descriptors / descriptor indexing
- Compute-shader frustum and occlusion culling writing an indirect draw buffer
- One `DrawIndexedIndirectCount` per material class

The CPU stops touching per-object draw submission entirely.

Alongside:

- **Clustered forward+ instead of deferred.** Kills G-buffer bandwidth, handles
  transparency natively, permits MSAA instead of the current MLAA/FXAA fixups.
- **GTAO in one compute dispatch** replacing the four-pass SSAO.
- **Fuse the post chain** into 2–3 compute passes instead of a dozen RT ping-pongs.

### Hardware compatibility strategy

This is the part that constrains everything else, so it is settled here.

**D3D11 the API is not D3D11 the hardware.** The D3D11 runtime exposes feature levels
9_1 through 11_1. FL9_3 is literal DX9 silicon; FL10_0 is GeForce 8800 (2006) / Radeon
HD 2900 (2007). A D3D11 backend reaches *older* hardware than the current D3D9 renderer
practically targets.

| API | GPU floor | OS floor |
|---|---|---|
| D3D11 (FL10_0) | GeForce 8000 / HD 2000 — 2006–07 | Vista+ |
| D3D11 (FL11_0) | GeForce 400 / HD 5000 — 2009–10 | Vista+ |
| D3D12 | FL11_0 — 2009–10 | **Windows 10+** |
| Vulkan 1.0 | Kepler / GCN / Haswell — 2012–13 | **Windows 7+, Linux** |

What survives on a D3D11 backend:

| Feature | D3D11 | Notes |
|---|---|---|
| Indirect draws | Yes | `DrawIndexedInstancedIndirect`, FL11_0 |
| Compute culling | Yes | CS 5.0 at FL11_0; limited at FL10 |
| Lower draw cost | Yes | ~3–5x cheaper than D3D9 from the API alone |
| Bindless | **No** | 128 SRV slots/stage. Emulate: texture arrays + atlases + material index buffer |
| Parallel recording | Partial | Deferred contexts exist but drivers serialize; small win |

**D3D11 captures roughly 60–70% of the rendering win at near-universal compatibility.**

**Decision: ship a D3D11 baseline backend and a Vulkan fast path. Skip D3D12.**
Vulkan already covers Windows 7+ *and* Linux (Steam Deck, Proton); D3D12 would add a
third backend for marginal gain over Vulkan while losing Win7/8 users. For a revived
2013 game whose living community runs private servers in regions where older hardware is
common, that coverage matters more than D3D12's edge.

**The rule that makes this work: design for the modern API and emulate downward.** Write
the renderer around explicit command buffers, bindless handles and indirect draws, then
implement fallbacks on D3D11. Doing it the other way — D3D9-shaped code with a modern
backend bolted on — yields the verbosity of the new API with none of its wins.

API tier and *renderer* tier are separate axes. FL10 hardware cannot afford GPU culling
or a fat G-buffer regardless of API, so ship three render paths:

| Tier | Target | Path |
|---|---|---|
| 0 | D3D11 FL10_0 | Forward, CPU cull, no compute — roughly the current renderer, ported |
| 1 | D3D11 FL11_0 | Compute cull, clustered forward |
| 2 | Vulkan | Full GPU-driven, bindless, parallel command recording |

If team size makes a hand-written RHI unrealistic, **Diligent Engine** is the better
off-the-shelf option than bgfx — it exposes modern features (bindless-ish binding,
indirect draws) rather than settling at a lowest common denominator.

### Risk

High. This is a ground-up renderer rewrite touching every `obj_*` class that implements
`AppendRenderables`. Sequence it behind item 2 so you rewrite draw submission once, not
twice.

---

## 2. Object model: data-oriented storage

### Problem

`GameObject` (`src/GameEngine/gameobjects/GameObj.h:243`) is a god class — roughly 100
fields covering physics config, bounding boxes, shadow extrusion data, prefab pointers,
scene-box pointers, transform matrices, precalculated shader constants — with ~30 virtual
methods. Instances live in a pointer-chased linked list.

Every world traversal is therefore a cache miss per object plus a virtual dispatch. Worse,
the list pointers are wrapped in `r3dSec_type` (`GameObj.h:270`), so each step also pays a
union copy and four XOR operations just to decrypt `pNextObject`.

The actual access patterns are all **columnar**: "update all zombies", "append renderables
for all visible meshes", "test every networked object against every player". Row-based
polymorphic storage is the worst possible fit for every one of them.

Two visible symptoms of this cost:

- Statics update round-robin, ≤2048 per frame (`ObjManag.cpp:1087`)
- `ANIMATED_ZOMBIES_COUNT = 96` (`ObjManag.cpp:54`) — only 96 zombies animate per frame,
  the rest rotate through a window, causing visible animation popping

Both are budget hacks for per-object update costs.

### Change

Move to **archetype-based ECS**. Not for fashion — because it turns each of those
traversals into a linear scan over packed arrays that vectorizes.

- Components stored in contiguous per-archetype arrays
- Systems are plain functions over component slices
- **Generational-index handles replace raw `GameObject*`**

The handle change alone deletes the four-frame deferred-deletion scheme
(`ObjManag.cpp:1161-1204`), which exists solely to outlive dangling pointers. A stale
handle simply fails to resolve.

Both round-robin hacks disappear: with packed data, updating all statics and animating all
zombies every frame becomes affordable.

### Risk

High — it touches every gameplay class. Mitigate by migrating subsystem by subsystem
behind an adapter that presents the old `GameObject` interface over ECS storage, and
retiring the adapter last.

---

## 3. Server interest management

### Problem

**The clearest, cheapest, largest win in this document.**

`ServerGameLogic::UpdateNetObjVisData` (`server/.../ServerGameLogic.cpp:2442`) walks the
entire object linked list per player, per tick, virtual-calling `GetNetworkHelper()` and
computing a distance for each. At 512 players against tens of thousands of objects this
dominates the server frame, and it is O(n·m) with **no spatial index whatsoever**.

### Change

Three steps, each independently shippable:

1. **Spatial hash grid.** Objects register into cells; each player queries only its
   neighbouring cells. Reduces to O(players × nearby objects).
2. **Dirty tracking.** Most of the world is static. An object that did not move, tested
   against a player that did not move, cannot have changed visibility. Skip it.
3. **Priority-scored replication under a per-client bandwidth budget** (à la Unreal's
   `NetPriority`), replacing binary distance visibility. Nearby, recently-changed, or
   gameplay-relevant objects get bandwidth first.

### Impact

100x+ on the dominant server cost. Entirely algorithmic — no language, API or
architecture dependency. **Do this first.**

### Risk

Low. Self-contained within `ServerGameLogic` and `INetworkHelper`.

---

## 4. Fixed timestep and rewind hit registration

### Problem

The server runs `Sleep(10)` with a variable `r3dGetFrameTime()`
(`server/.../ServerGame.cpp:178`). A variable-timestep authoritative simulation is
non-deterministic, which forecloses lag compensation, replay and rollback.

Consequently, hit registration is an **event-relay model**: the client asserts
`PKT_C2C_PlayerFired` then `PKT_C2C_PlayerHitDynamic`, and the server validates only
loosely via shot-counter accounting (see the comment at `P2PMessages.h:68-71`). This is
inherently cheat-prone, and the codebase compensates with a large stack of anti-cheat
machinery that costs cycles on hot paths.

### Change

- **Fixed simulation timestep** (30 or 60 Hz) with an accumulator
- **Decoupled snapshot send rate** (~20 Hz), delta-compressed
- **Ring buffer of past world states** (Quake 3 / Overwatch model)
- **Server-side rewind hit registration**: client sends "fired at time T with this aim";
  server rewinds the world to T and traces the shot itself

### Impact

Moderate directly. Large indirectly — it makes an entire class of cheats structurally
impossible rather than detectable, which is the precondition for retiring parts of the
anti-cheat stack (see item 7).

### Risk

Medium. Changes gameplay feel; requires careful tuning of interpolation and the rewind
window. Determinism also depends on item 6's physics choice.

---

## 5. Frame parallelism

### Problem

`JobChief` (`src/Eternity/Include/JobChief.h`) exists and is competent, and is used for
exactly two things: particles and zombies (`ObjManag.cpp:1146-1154`). Everything else in
a ~500-line frame function is serial, and simulation fully precedes rendering.

### Change

A **task graph across the whole frame**: culling, animation, particle simulation, AI,
physics and command-buffer recording all parallel, with declared dependencies.

Then **pipeline it** — frame N's rendering runs while frame N+1's simulation runs.

Note the dependency: parallel command recording is impossible in D3D9 and natural in
Vulkan, so the full win requires item 1.

### Risk

Medium. Data races here are the classic source of unreproducible bugs. This is the single
strongest argument that was raised for Rust — see *Language decision* below for why it
was nonetheless dropped, and what to do instead.

---

## 6. Physics

### Change

**PhysX 3 → Jolt.** Faster, better multicore scaling, and notably more deterministic —
which item 4 depends on.

Additionally: 512 PhysX character controllers in a single scene is heavy. Use a cheap
custom capsule-sweep controller for player movement and reserve full rigid-body
simulation for vehicles and ragdolls.

The client currently runs the same PhysX scene as the server for prediction; give it a
substantially cheaper collision representation.

### Risk

Medium. Middleware swap with different solver behaviour; expect gameplay tuning work.

---

## 7. Asset pipeline and load time

### Problem

`LoadLevel_Objects` (`src/EclipseStudio/Sources/GameLevel_IO.cpp:159`) parses
`LevelData.xml`, `SoundData.xml` and `ServerData.xml` with pugixml at runtime, then
constructs every object via string-name lookup in the `AObject` class table.

This is the largest load-time cost, and it compounds on the server, where
SupervisorServer spawns GameServer instances continuously.

### Change

**Bake offline to a binary, memory-mappable format.** Zero parsing at load: `mmap` the
file and fix up offsets. Keep XML as the editor's authoring format and add a cook step.

### Risk

Low. Self-contained, and `RSBuild` already exists as a cook stage to extend.

---

## 8. Remove per-access field obfuscation

### Problem

`r3dSec_type` (`src/Eternity/Include/r3dProtect.h:38`) wraps ~29 hot fields — object list
heads, object counts, class pointers, `pNextObject` — in XOR obfuscation. Each read costs
a union copy plus four XORs, and `get()` is unconditional (it is *not* gated behind the
`USE_VMPROTECT` define, which is disabled throughout this drop).

Because it wraps `pNextObject`, **every linked-list traversal step in the engine pays it.**

It buys very little. A determined attacker finds a fixed XOR key in an afternoon.

### Change

Delete it. Replace client-side obfuscation with server-side authority (item 4) and
statistical server-side detection — the codebase already has a good example in the
GameBlocks aimbot detector (`ServerGameLogic.cpp:4194-4237`).

### Risk

Low, *provided item 4 lands first*. Removing obfuscation before the server is properly
authoritative weakens the position rather than improving it. Sequence accordingly.

---

## 9. UI

Scaleform is a Flash VM interpreting ActionScript on the game thread. It is discontinued
as a product and was never fast. Replace with a GPU-batched retained-mode UI — the editor
already contains a workable immediate-mode system (`imgui_*`) to build from.

---

## Language decision: Rust was evaluated and dropped

Recorded so the reasoning is not relitigated.

Rust and C++ generate essentially equivalent machine code. Differences run to single-digit
percent in both directions — bounds checks usually elide; `noalias` derived from the borrow
checker sometimes gives LLVM better aliasing information than C++ typically provides.
**Rust is not faster per instruction, and picking it for raw throughput would be a
mistake.**

The genuine argument was item 5: safe, aggressive frame parallelism, plus memory safety on
long-running server processes. The countervailing costs are the ones that decided it —
every piece of middleware required here (Jolt, PhysX, FMOD, navmesh, and especially
anti-cheat vendors) has a C++ API, and FFI boundaries are precisely where Rust's
guarantees stop.

**Decision: stay in C++.** To recover the parallelism safety argument without the language:

- Thread Sanitizer in CI on a dedicated build configuration
- Enforce the task-graph dependency model — no ad-hoc thread spawning
- `const`-correctness discipline at system boundaries; systems declare read vs write sets
- Keep shared mutable state out of the frame graph; prefer double-buffering per-frame data

Note that items 1–7 are **all language-independent**. Doing them in modernized C++
captures the overwhelming majority of the available performance.

---

## Sequencing

**Phase 0 — Measure (prerequisite for everything)**

Get the project building with its missing dependencies. Instrument with the existing
`R3DPROFILE_*` macros plus a GPU profiler. Establish a repeatable benchmark: a fixed
camera path on a populated map, and a server load test with synthetic clients.
**Validate or correct every estimate in this document before proceeding.**

**Phase 1 — Cheap, low-risk, independent**

- Item 3: spatial interest management ← *largest win per unit effort*
- Item 7: binary asset cook
- Item 6: Jolt migration

None of these depend on the rewrites. Ship them regardless of what follows.

**Phase 2 — Server correctness**

- Item 4: fixed timestep + rewind hit registration
- Item 8: remove `r3dSec_type` (**after** item 4, not before)

**Phase 3 — The rewrites**

- Item 2: ECS migration, subsystem by subsystem behind an adapter
- Item 1: RHI + GPU-driven renderer, sequenced *after* item 2 so draw submission is
  rewritten once
- Item 5: full task graph, which item 1 unlocks
- Item 9: UI replacement

---

## What not to do

- **Do not port D3D9 code to D3D12/Vulkan one-to-one.** You inherit the new API's
  verbosity and none of its benefits. Redesign for GPU-driven submission or stay on D3D11.
- **Do not micro-optimize inside the current object model.** The cost is structural —
  cache misses and virtual dispatch on pointer-chased data. SIMD in a `GameObject` method
  optimizes the wrong layer.
- **Do not remove anti-cheat before the server is authoritative.** Sequence item 8 after
  item 4.
- **Do not begin the ECS or renderer rewrite before Phase 0.** They are the two highest-
  risk items and the only ones justified purely by measurement.
- **Do not add a D3D12 backend.** Vulkan covers its hardware plus Windows 7/8 and Linux.
  A third backend is maintenance cost for marginal gain.
</content>
