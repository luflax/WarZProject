# PhysX 3.x -> 4.1 compatibility shims

The codebase was written against PhysX 3.x. PhysX 4.1 moved or removed several headers.
Rather than editing every include site, this directory reproduces the **old 3.x paths**
and forwards to the 4.1 equivalents — the same trick used for `src/External/` as a whole.

Put this directory on the include path *after* the real PhysX roots.

| 3.x path | 4.1 reality |
|---|---|
| `RepX/RepX.h` | serialization moved to `extensions/PxRepXSerializer.h` |
| `common/PxIO.h` | moved to `foundation/PxIO.h` |
| `extensions/PxVisualDebuggerExt.h` | PVD was rewritten as `pvd/PxPvd.h` |
