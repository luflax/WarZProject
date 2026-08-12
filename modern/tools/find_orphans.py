#!/usr/bin/env python3
"""
List .cpp files on disk that the ORIGINAL Visual Studio projects never compiled.

Dead legacy accumulates in a codebase this old, and fixing a file nobody built is
pure waste. Eternity had one such file; EclipseStudio has fifteen -- including every
Mech source and the whole http/ directory, which between them accounted for several
of the failures being chased one at a time.

Writes .probe-orphans, which probe.sh reads and excludes.

Usage:
    ./tools/find_orphans.py            # regenerate .probe-orphans
    ./tools/find_orphans.py --list     # print without writing
"""

import os
import re
import sys

MODERN_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORIGINAL_DIR = os.path.dirname(MODERN_DIR)

# (vcxproj relative to the ORIGINAL tree, source root relative to modern/)
PROJECTS = [
    ("src/Eternity/Eternity.vcxproj",                 "src/Eternity"),
    ("src/EclipseStudio/Studio.vcxproj",              "src/EclipseStudio"),
    ("server/src/WO_GameServer/WO_GameServer.vcxproj", "server/src/WO_GameServer"),
    ("server/src/MasterServer/MasterServer.vcxproj",   "server/src/MasterServer"),
    ("server/src/SupervisorServer/SupervisorServer.vcxproj", "server/src/SupervisorServer"),
]

# Files added by the port itself -- no vcxproj lists them, but they are live.
PORT_ADDED = ("RecastNav/", "RmlUiIntegration/")


def listed_in(proj_path):
    try:
        text = open(proj_path, encoding="utf-8", errors="surrogateescape").read()
    except OSError:
        return None
    return {os.path.basename(m.group(1).replace("\\", "/")).lower()
            for m in re.finditer(r'<ClCompile\s+Include="([^"]+)"', text)}


def main():
    list_only = "--list" in sys.argv
    os.chdir(MODERN_DIR)

    orphans = []
    for proj_rel, src_root in PROJECTS:
        listed = listed_in(os.path.join(ORIGINAL_DIR, proj_rel))
        if listed is None:
            print(f"  (no project file: {proj_rel})", file=sys.stderr)
            continue
        if not os.path.isdir(src_root):
            continue

        for dirpath, dirnames, filenames in os.walk(src_root):
            for fn in filenames:
                if not fn.lower().endswith(".cpp"):
                    continue
                rel = os.path.join(dirpath, fn).replace(os.sep, "/")
                if any(tag in rel for tag in PORT_ADDED):
                    continue
                if fn.lower() not in listed:
                    orphans.append(rel)

    orphans.sort()

    print(f"{len(orphans)} .cpp file(s) on disk were never compiled by the original projects:")
    for o in orphans:
        print("   ", o)

    if not list_only:
        with open(".probe-orphans", "w") as fh:
            fh.write("\n".join(orphans) + ("\n" if orphans else ""))
        print("\nwritten to .probe-orphans (probe.sh excludes these)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
