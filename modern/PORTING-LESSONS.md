# Porting Lessons

What actually worked, and what wasted time, driving the whole client — Eternity 81/81,
GameEngine 44/44, EclipseStudio 209/209 — to compile under strict C++20 GCC.

Read this before starting a new tree. Most of it generalises to any MSVC-era codebase.

---

## The one that matters most

> **A handful of shared headers cause almost all failures. Find them before fixing anything else.**

Measured, repeatedly:

| Root cause | One-line fix | Files unblocked |
|---|---|---|
| `virtual ... = NULL;` in `r3dNetwork.h` | `= 0` | **70 of 82** |
| `Min`/`Max` unqualified in `Tsg_stl/TString.h` | include + qualify `r3dTL::` | **61 of 81** |
| `_THROW0` / `_FARQ` in `r3dSTLAllocators.h` | define them at the top of the file | **198** |
| Wrong `-D` for the tree (`WO_SERVER` on client code) | per-binary defines in `probe.sh` | **22** |
| Leaked `for(int i…)` scope in `xpsobject.h` | give the second loop its own `i` | **6** |
| `fmod/soundsys.h` casing | one entry in a skip-list | **9** |

A failure count is not a work estimate. **Always ask "how many distinct root causes?"
before "how many files fail?"** Fixing in root-cause order is often 10x less work than
fixing in file order.

---

## Workflow

### Do this

1. **Scan statically first.** `tools/scan_conformance.py` finds every MSVC-ism in ~2
   seconds. Compiling to discover them one at a time costs minutes per discovery.
2. **Triage before fixing.** `tools/probe.sh --triage` groups failures by the
   *file:line that actually errored*, not by the failing TU. That's the root-cause view.
3. **Re-probe only what failed.** `tools/probe.sh <dir> --failed` reuses the last run's
   failure list. A full sweep is for checkpoints, not for iteration.
4. **Single file while porting.** `tools/check.sh <file.cpp>` — seconds, not minutes.
5. **Enumerate the whole API surface at once.** When a shim is missing symbols, grep
   every symbol the consumer uses and generate them in one pass. Adding them one
   compile at a time was the single biggest time sink in this project.

### Don't do this

- **Don't fix one error per probe.** Each full probe is ~60s even parallelised.
- **Don't grow a shim reactively.** Count the surface first (`grep -oE '\bPx[A-Z]\w+'
  | sort -u | wc -l`). At 171 symbols and 89 member accesses, vendoring the real SDK
  was cheaper than stubbing — and the stub would have been throwaway.
- **Don't trust a green build with `-fpermissive`.** It hid ~70 real errors behind one
  root cause. Strict is the only honest gate.

---

## Traps that cost real time

### MSVC never compiled some of this code

Delayed template parsing means **bodies of never-instantiated templates were never
type-checked**. `xpsobject.h` referenced `VertFVF` (defined nowhere), `Stats.BufferMem`
(wrong member name), and `Flush()` — beside which the original author had written
`// I dont know what is it. But it is not compiling.`

**Before "fixing" such code, ask whether it ever ran.** Often the honest change is to
delete it or comment it out, not to invent an implementation.

### Check whether the file was even in the build

`r3dObj_OLDRender.cpp` failed for real reasons — and was never compiled. Diffing
`<ClCompile>` entries in the original `.vcxproj` against files on disk found it in
seconds. **One orphan out of 82; worth checking before spending an hour on a file.**

```bash
# list .cpp on disk that the original project never built
python3 -c "import re,os;p=open('../src/Eternity/Eternity.vcxproj').read();\
l={os.path.basename(m.group(1).replace(chr(92),'/')).lower() for m in re.finditer(r'<ClCompile\s+Include=\"([^\"]+)\"',p)};\
d={f.lower() for _,_,fs in os.walk('src/Eternity/Source') for f in fs if f.endswith('.cpp')};\
print(sorted(d-l))"
```

### A shim's reach is bounded by pointers

A forward-declaration shim carries any header that only **holds pointers**. The moment
a `.cpp` **dereferences** (`actor->isRigidStatic()`, `hit.position`, `desc.thickness`),
you need the real SDK. Decide early which side of that line a dependency sits on —
growing a fake API past it is wasted work.

### The compiler's own suggestion is usually right — but check the direction

`'mNumRatios'; did you mean 'mNbRatios'?` and `'setRigidDynamicFlag'; did you mean
'setRigidDynamicLockFlags'?` look alike, and only one is correct: the second wanted
`setRigidBodyFlag`, which GCC never suggested. **Grep the vendored headers before
accepting a rename.**

### A friend declaration is also a declaration

`friend struct Editor_Level;` inside `namespace Nav`, with no prior declaration of
`Editor_Level`, befriends `Nav::Editor_Level` — a class that never exists. The real
`::Editor_Level` gets nothing. Forward-declare at the right scope and write
`friend struct ::Editor_Level;`. MSVC accepted the original.

### Watch for your own tooling bugs

- `normalize_includes.py` skipped anything matching `"fmod/"` as a shim path. That also
  matched the **real** header `GameEngine/fmod/SoundSys.h`, breaking 9 TUs.
- A replacement string containing `*/` **terminated the surrounding block comment**,
  exposing commented-out code as live code.
- A regex `([^)]+)\)` for a function argument stopped at the first `)`, producing
  `navPoints.end(, rng())`.

**After any bulk rewrite, re-run the scan and diff a sample.** Silent corruption is
worse than a compile error.

### Heuristic checks are leads, not findings

`extra-qualification` reports 923 hits; Eternity is 100% clean. The regex cannot tell a
legal out-of-line definition from an ill-formed in-class one without semantic analysis.
Checks marked `HEURISTIC` must be confirmed by the compiler — **never mass-applied**.

---

## Catalogue of MSVC-isms found

Ordered by files affected.

| Construct | ISO reality |
|---|---|
| `virtual f() = NULL;` | pure-specifier must be literal `0` |
| Unqualified name from a namespaced header in a template | two-phase lookup needs it visible at definition |
| `friend class X;` as the only declaration | friend does not introduce the name into the enclosing scope |
| `friend void f();` then `static void f(){}` | friend implies external linkage; conflicts with `static` |
| `for(int i…)` then `for(i=0…)` | loop variable is confined to the loop |
| `#include "A\B.h"` | backslash in a header-name is undefined |
| Case-insensitive include paths | fatal on a case-sensitive filesystem |
| `.##x` / `::##x` in macros | superfluous `##`, invalid paste |
| `unsigned char(x)` | functional cast needs a single type-name token |
| `sizeof TYPE` | parentheses required on a type |
| `Type &Type::f()` inside the class body | extra qualification |
| `this->T::T()` | cannot call a constructor directly — use a delegating ctor |
| Temporary bound to `T&` | needs a named local |
| Bit-field bound to `T&` | bit-fields have no address; copy first |
| `_asm` | none — and MSVC rejects it on x64 too |
| `_cdecl` | `__cdecl` |
| `stdext::hash_map`, `std::tr1::` | `std::unordered_map`, `std::` |
| `#endif TOKEN` | must be a comment |
| Anonymous struct in a local union | `-fms-extensions` only allows this inside a *named* type; at block scope, replace with explicit shifts and masks |
| Parenthesised return type: `typedef (unsigned int)(WINAPI *f)(void*)` | drop the parentheses |
| `T::iterator` in a template | `typename T::iterator` — including in the `typedef` that resolves it |
| Use-before-declaration of a global inside a template body | declare it above the template; non-dependent names resolve at the definition point |
| Function declared `extern` (or as a friend) then defined `static` | make the two agree |
| Rebinding a parameter used as an iteration cursor (`node = node.next_sibling()`) | take cheap handles like `pugi::xml_node` **by value**, not by `const&` |
| Explicit specialization inside a class body | must be at namespace scope |
| MSVC STL internals (`std::_String_base::_Xlen`) | no libstdc++ equivalent — guard with `_MSC_VER` |

C++17/20 removals hit far less often than MSVC laxity: `std::allocator<void>` member
typedefs, `throw()`, `auto_ptr`, `random_shuffle`. Missing `<algorithm>` was the most
common non-MSVC issue — MSVC pulled it in transitively.

---

## Flags

```
-std=c++20 -fsyntax-only -w -fms-extensions
```

- **No `-fpermissive`.** It masks real conformance errors.
- **`-fms-extensions` is legitimate here.** Anonymous structs inside unions are the
  intended idiom and used pervasively — `D3DMATRIX` itself is one.
- `-w` only because warning triage is Phase 2; the *errors* are the gate.

MinGW-w64 i686 matches the original `Win32` target and ships `windows.h`, `d3d9.h`,
`dinput.h`. It does **not** ship `d3dx9` implementations or `DirectXMath`, which is why
the D3DX compat layer is hand-written and dependency-free.

---

## Vendoring vs shimming

| Situation | Choose |
|---|---|
| Header only holds pointers/references | Forward-declaration shim |
| `.cpp` calls the API | Real SDK, or exclude the file |
| Surface > ~50 symbols with member access | Vendor — a stub is throwaway |
| Product discontinued *and* replaceable | Vendor the replacement (Recast, RmlUi) |
| Optional subsystem with an existing off-switch | Use the switch (`ENABLE_AUTODESK_NAVIGATION=0`) |

When vendoring a *newer* major version, put the old paths and spellings in a
`compat/` directory rather than editing call sites — the same trick that makes
`src/External/` work. `Px3xCompat.h` covers PhysX 3→4 renames in one header included
from the PCH; only genuinely **removed** APIs needed hand porting.

### Three tiers of compat, cheapest first

1. **A typedef.** `PxSceneQueryFilterData` → `PxQueryFilterData`, `PxRigidDynamicFlag`
   → `PxRigidBodyFlag`. Free, and invisible at the call site.
2. **A macro on an enumerator.** `#define eDISTANCE ePOSITION` reached 19 call sites at
   once. Only safe because the compat header is included *after* the SDK has finished
   parsing — a macro on a name that common will otherwise detonate somewhere unrelated.
   Say so in a comment, and check the SDK for other uses of the name first.
3. **A derived class.** PhysX 4 deleted six `PxScene` query methods with ~35 call sites
   spelled `scene->raycastSingle(...)`. `class PxScene3x : public PxScene` adds them back
   as non-virtual forwarders over the new buffer API, and the one member holding the
   pointer changes type. No data members, no virtuals, so the object's layout and vtable
   are untouched — it stays abstract and is only handled by pointer. **One declaration
   change beat 35 rewrites.**

Beyond that, port by hand. Trying to fake `PxVehicleWheelQueryResult` would have meant
inventing values the simulation actually produces.

### Renames are cheap; removals are the real work

The PhysX 3→4 migration was ~80% mechanical (`impact` → `position`, `getActor()` returning
a pointer instead of a reference, `PxTransform::createIdentity()` → `PxTransform(PxIdentity)`)
and ~20% genuinely removed capability that had to be re-plumbed — wheel query results,
RepX deserialization, batch-query memory. **Sort the list into those two buckets before
starting**: the first bucket is a `sed` per entry, the second needs a design decision each.

---

## Related documents

- [`README.md`](README.md) — current status
- [`../PHASE1-BUILD-PLAN.md`](../PHASE1-BUILD-PLAN.md) — milestones and scope
- [`../DEPENDENCIES.md`](../DEPENDENCIES.md) — dependency audit
