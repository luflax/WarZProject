#!/usr/bin/env python3
"""Derive PhysX's per-module source and include lists from NVIDIA's own CMake.

NVIDIA's build harness cannot be used directly: physx/source/compiler/cmake/CMakeLists.txt
fails hard unless PHYSX_ROOT_DIR, PM_CMakeModules_PATH and PM_PxShared_PATH are set by
their `generate_projects` scripts, and the `CMakeModules` package those point at is
fetched by packman and is not in the repository at all. The windows/ variants then set
MSVC-only flags (/arch:SSE2 /d2Zi+ /GS- /fp:fast) that mean nothing to GCC.

So we build PhysX ourselves -- but the *source lists* still come from NVIDIA, because
globbing physx/source/**/*.cpp is wrong in a way that is silent: the tree carries
per-platform files (src/unix/, src/linux/), GPU-only translation units and the
`immediatemode`/`physxgpu` directories, none of which belong in an i686 Windows static
build. This is the same reasoning as tools/gen_sources.py deriving the game's own lists
from the .vcxproj files rather than from the disk.

What this understands of CMake is deliberately small:

  SET(VAR a b c)                  -- assignment, values split on whitespace
  ${VAR}                          -- expansion, recursive
  include(.../<platform>/X.cmake) -- the platform file, spliced in at that point
  ADD_LIBRARY(name type srcs...)  -- the module's file list
  TARGET_INCLUDE_DIRECTORIES(...) -- the module's PRIVATE include path

IF/ELSE/ENDIF is IGNORED, and that is checked rather than assumed: no conditional in any
of the 15 module files guards a source file or an include directory. They guard only
lib type (STATIC vs SHARED), .pdb naming, GAMEWORKS output directories and the
source-distro file list. --audit re-checks that claim by reporting any .cpp on disk that
no module claims, and any claimed file that is missing.

Usage:
    tools/gen_physx_sources.py            # write cmake/sources/PhysX_*.cmake
    tools/gen_physx_sources.py --audit    # also report unclaimed / missing files
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MODERN = os.path.dirname(HERE)
PHYSX = os.path.join(MODERN, "src/External/PhysX")
CMAKE_DIR = os.path.join(PHYSX, "physx-source/compiler/cmake")
OUT_DIR = os.path.join(MODERN, "cmake/sources")

# The 15 modules windows/CMakeLists.txt includes, in its own order. PhysX itself builds
# them as separate libraries; we keep that split so link order can express the real
# dependency graph.
MODULES = [
    "PhysXFoundation",
    "LowLevel",
    "LowLevelAABB",
    "LowLevelDynamics",
    "PhysX",
    "PhysXCharacterKinematic",
    "PhysXCommon",
    "PhysXCooking",
    "PhysXExtensions",
    "PhysXVehicle",
    "SceneQuery",
    "SimulationController",
    "FastXml",
    "PhysXPvdSDK",
    "PhysXTask",
]

# Our vendored tree flattens NVIDIA's two-root layout (physx/ + pxshared/) into three
# sibling directories, so the seeded roots are sentinels that get rewritten on the way
# out. See src/External/PhysX/README.md.
ROOT = "<PHYSX_ROOT>"
PXSHARED = "<PXSHARED>"

REWRITES = [
    (ROOT + "/include", "${PHYSX_DIR}/physx-include"),
    (ROOT + "/source", "${PHYSX_DIR}/physx-source"),
    (PXSHARED + "/include", "${PHYSX_DIR}/pxshared-include"),
]

_NAME = re.compile(r"\s*([A-Za-z0-9_]+)")
_CALL = re.compile(r"^\s*([A-Za-z0-9_]+)\s*\(")
_VAR = re.compile(r"\$\{([A-Za-z0-9_]+)\}")


def _strip_comments(text):
    return "\n".join(re.sub(r"#.*$", "", line) for line in text.splitlines())


def _read_call(lines, i):
    """Collect a parenthesised command starting at lines[i]; return (body, next_index).

    Commands span lines and the closing paren may share a line with the last argument,
    so track depth rather than looking for a lone ')'.
    """
    depth = 0
    parts = []
    while i < len(lines):
        line = lines[i]
        depth += line.count("(") - line.count(")")
        parts.append(line)
        i += 1
        if depth <= 0:
            break
    body = "\n".join(parts)
    body = body[body.index("(") + 1:]
    body = body[: body.rindex(")")]
    return body, i


class Evaluator:
    """Just enough CMake to resolve the SET/${} chain the module files are written in."""

    def __init__(self, variables):
        self.vars = dict(variables)
        self.calls = []  # (command, raw body) in source order

    def expand(self, text, depth=0):
        if depth > 12:  # a malformed file must not hang the generator
            return text

        def sub(m):
            return " ".join(self.vars.get(m.group(1), []))

        out = _VAR.sub(sub, text)
        return self.expand(out, depth + 1) if _VAR.search(out) else out

    def run(self, path, platform_dir):
        lines = _strip_comments(open(path, encoding="utf-8", errors="replace").read()).splitlines()
        i = 0
        while i < len(lines):
            if "(" not in lines[i]:
                i += 1
                continue
            m = _CALL.match(lines[i])
            if not m:
                i += 1
                continue
            cmd = m.group(1).upper()
            body, nxt = _read_call(lines, i)
            i = nxt

            if cmd == "SET":
                sm = _NAME.match(body)
                if sm:
                    self.vars[sm.group(1)] = _tokens(self, body[sm.end():])
            elif cmd == "INCLUDE":
                # The generic file pulls in its platform variant partway through, after
                # PHYSX_SOURCE_DIR/LL_SOURCE_DIR are set and before ADD_LIBRARY. That
                # ordering is load-bearing, so splice it in right here.
                target = self.expand(body).strip()
                name = os.path.basename(target)
                candidate = os.path.join(platform_dir, name)
                if os.path.isfile(candidate):
                    self.run(candidate, platform_dir)
            else:
                self.calls.append((cmd, body))


def _tokens(evaluator, body):
    return evaluator.expand(body).replace(";", " ").split()


def collect(module):
    ev = Evaluator({
        "PHYSX_ROOT_DIR": [ROOT],
        "PXSHARED_PATH": [PXSHARED],
        "PROJECT_CMAKE_FILES_DIR": ["source/compiler/cmake"],
        "TARGET_BUILD_PLATFORM": ["windows"],
    })
    ev.run(os.path.join(CMAKE_DIR, module + ".cmake"), os.path.join(CMAKE_DIR, "windows"))

    sources, includes = [], []
    for cmd, body in ev.calls:
        toks = _tokens(ev, body)
        if not toks or toks[0] != module:
            continue
        if cmd == "ADD_LIBRARY":
            for t in toks[1:]:
                if t.lower().endswith((".cpp", ".c")) and t not in sources:
                    sources.append(t)
        elif cmd == "TARGET_INCLUDE_DIRECTORIES":
            scope = None
            for t in toks[1:]:
                if t in ("PRIVATE", "PUBLIC", "INTERFACE"):
                    scope = t
                    continue
                # INTERFACE entries are install/export generator expressions, not paths
                # we can use in-tree.
                if scope == "PRIVATE" and "$<" not in t and t not in includes:
                    includes.append(t)
    return sources, includes


_case_index = {}


def _resolve_case(rel):
    """Map a path as NVIDIA's CMake spells it onto the one that exists on disk.

    Their files say "Common/src/windows" and "LowLevel/software/include" where the tree
    has "common" and "lowlevel". On Windows that is the same directory; here it is not,
    and an include path that does not exist produces no error -- only a mysterious
    "no such file" from a #include several layers down. Resolve it explicitly.
    """
    full = os.path.join(PHYSX, rel)
    if os.path.exists(full):
        return rel
    if not _case_index:
        for root, dirs, files in os.walk(PHYSX):
            for name in dirs + files:
                p = os.path.relpath(os.path.join(root, name), PHYSX).replace(os.sep, "/")
                _case_index[p.lower()] = p
    return _case_index.get(rel.lower(), rel)


def rewrite(path):
    for old, new in REWRITES:
        if path.startswith(old):
            # NVIDIA's variables are concatenated as "${DIR}/src/x.cpp" where DIR
            # already ends in a slash, so collapse the doubles that produces.
            tail = re.sub(r"/{2,}", "/", path[len(old):])
            prefix = new.split("/")[-1]  # physx-source | physx-include | pxshared-include
            return "${PHYSX_DIR}/" + _resolve_case(prefix + tail)
    return path


def emit(module, sources, includes):
    var = module.upper()
    lines = [
        "# Generated by tools/gen_physx_sources.py -- do not edit.",
        "# Derived from PhysX 4.1's own compiler/cmake/{,windows/}%s.cmake." % module,
        "",
        "set(WARZ_%s_SOURCES" % var,
    ]
    lines += ["    %s" % rewrite(s) for s in sources]
    lines += [")", "", "set(WARZ_%s_INCLUDES" % var]
    lines += ["    %s" % rewrite(inc) for inc in includes]
    lines += [")", ""]
    out = os.path.join(OUT_DIR, "PhysX_%s.cmake" % module)
    with open(out, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines))
    return out


def audit(claimed):
    """Report what the extraction did not account for.

    Unclaimed files are expected -- per-platform and GPU sources we deliberately do not
    build -- but they must be *reviewable*, not invisible.
    """
    on_disk = set()
    src_root = os.path.join(PHYSX, "physx-source")
    for root, _dirs, files in os.walk(src_root):
        for f in files:
            if f.lower().endswith((".cpp", ".c")):
                rel = os.path.relpath(os.path.join(root, f), PHYSX).replace(os.sep, "/")
                on_disk.add("${PHYSX_DIR}/" + rel)

    missing = sorted(c for c in claimed
                     if not os.path.isfile(c.replace("${PHYSX_DIR}", PHYSX)))
    unclaimed = sorted(on_disk - claimed)

    print("\naudit: %d claimed, %d on disk" % (len(claimed), len(on_disk)))
    if missing:
        print("  MISSING (claimed by cmake, absent on disk) -- %d:" % len(missing))
        for m in missing:
            print("    " + m)
    else:
        print("  every claimed file exists")

    by_dir = {}
    for u in unclaimed:
        by_dir.setdefault(u.rsplit("/", 1)[0], []).append(u)
    print("  not built -- %d files in %d directories:" % (len(unclaimed), len(by_dir)))
    for d in sorted(by_dir):
        print("    %-72s %d" % (d.replace("${PHYSX_DIR}/physx-source/", ""), len(by_dir[d])))


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    claimed = set()
    total = 0
    for module in MODULES:
        sources, includes = collect(module)
        if not sources:
            print("WARNING: %s produced no sources" % module, file=sys.stderr)
        claimed.update(rewrite(s) for s in sources)
        total += len(sources)
        emit(module, sources, includes)
        print("  %-24s %3d sources  %2d include dirs" % (module, len(sources), len(includes)))
    print("\n%d modules, %d translation units" % (len(MODULES), total))

    if "--audit" in sys.argv:
        audit(claimed)


if __name__ == "__main__":
    main()
