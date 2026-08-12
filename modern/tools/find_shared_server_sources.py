#!/usr/bin/env python3
"""
List the sources OUTSIDE server/ that the server projects compile anyway.

WO_GameServer.vcxproj pulls in 50 files from src/ -- 28 of them from EclipseStudio --
and MasterServer.vcxproj pulls in 3. Those files are compiled TWICE across the product,
once as client code and once with WO_SERVER defined, and the two configurations take
different #ifdef branches. Probing them only in their client configuration leaves the
server half unchecked.

The GameEngine entries need no special handling: probe.sh already gives GameEngine the
server defines. It is the EclipseStudio ones that are only ever seen as client code.

Writes .probe-shared.<project>, which probe.sh reads via its "@listfile" target form:

    FORCE_BINARY=WO_GameServer ./tools/probe.sh @.probe-shared.WO_GameServer

Usage:
    ./tools/find_shared_server_sources.py            # regenerate the lists
    ./tools/find_shared_server_sources.py --list     # print without writing
"""

import os
import re
import sys

MODERN_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORIGINAL_DIR = os.path.dirname(MODERN_DIR)

PROJECTS = [
    ("WO_GameServer",    "server/src/WO_GameServer/WO_GameServer.vcxproj"),
    ("MasterServer",     "server/src/MasterServer/MasterServer.vcxproj"),
    ("SupervisorServer", "server/src/SupervisorServer/SupervisorServer.vcxproj"),
]

# Kynapse wrapper implementations, superseded by ai/RecastNav/. probe.sh excludes them
# by name; keep the two lists consistent.
EXCLUDE = re.compile(r"AutodeskNav/Autodesk.*\.cpp$")


def shared_sources(proj_rel):
    """Sources the project compiles that do NOT live under server/."""
    proj_path = os.path.join(ORIGINAL_DIR, proj_rel)
    try:
        text = open(proj_path, encoding="utf-8", errors="surrogateescape").read()
    except OSError:
        return None

    base = os.path.dirname(proj_rel)
    out = []
    for m in re.finditer(r'<ClCompile\s+Include="([^"]+)"', text):
        rel = m.group(1).replace("\\", "/")
        full = os.path.normpath(os.path.join(base, rel)).replace(os.sep, "/")
        if full.startswith("server/"):
            continue
        if EXCLUDE.search(full):
            continue
        out.append(full)
    return sorted(set(out))


def main():
    list_only = "--list" in sys.argv
    os.chdir(MODERN_DIR)

    for name, proj_rel in PROJECTS:
        files = shared_sources(proj_rel)
        if files is None:
            print(f"  (no project file: {proj_rel})", file=sys.stderr)
            continue

        # Only keep what exists on disk -- bootstrap.sh does not copy everything.
        present = [f for f in files if os.path.isfile(f)]
        missing = [f for f in files if not os.path.isfile(f)]

        print(f"{name}: {len(present)} shared source(s) compiled with the server defines")
        for f in present:
            print("   ", f)
        for f in missing:
            print("    (not on disk)", f)

        if not list_only:
            path = f".probe-shared.{name}"
            with open(path, "w") as fh:
                fh.write("\n".join(present) + ("\n" if present else ""))
            print(f"    -> {path}")
        print()

    if not list_only:
        print("Probe them with, e.g.:")
        print("  FORCE_BINARY=WO_GameServer ./tools/probe.sh @.probe-shared.WO_GameServer")
    return 0


if __name__ == "__main__":
    sys.exit(main())
