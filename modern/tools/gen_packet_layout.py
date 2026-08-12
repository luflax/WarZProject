#!/usr/bin/env python3
"""Generate tests/layout/packet_sizes.inc -- the frozen wire-layout baseline.

WHY THIS IS GENERATED AND NOT HAND-WRITTEN

There are ~156 packet structs in P2PMessages.h, all `#pragma pack(1)` PODs, and they
are the wire format between a client and a server that are compiled separately. If GCC
lays one out differently from how it was laid out before -- a pack pragma that stops
applying, a base class that gains a vptr, an enum whose underlying type changes width --
every field after the change is read from the wrong offset. Nothing crashes. The server
just starts reading a player's position out of the middle of their inventory.

WHY DWARF AND NOT A PROGRAM THAT PRINTS sizeof()

The obvious way to collect these is to compile a program that printf()s each size. That
program is a PE32 executable and needs Wine to run, and Wine is exactly what is missing
in the environments this port is developed in (MILESTONE-C-PREWORK.md §1.2). Reading the
sizes out of the compiler's own debug output needs no runtime at all: `-g
-fno-eliminate-unused-debug-types` makes GCC emit a DW_TAG_structure_type for every type
in scope, carrying DW_AT_byte_size. Compiling is the only thing that has to work.

USAGE

    ./tools/gen_packet_layout.py                 # regenerate the baseline
    ./tools/gen_packet_layout.py --check         # fail if it would change

Regenerate ONLY when a packet change is intended, and bump P2PNET_VERSION in the same
commit -- that constant is the protocol's own version gate (P2PMessages.h:25).
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(ROOT, "tests", "layout", "packet_sizes.inc")

CXX = os.environ.get("WARZ_CXX", "i686-w64-mingw32-g++")
OBJDUMP = os.environ.get("WARZ_OBJDUMP", "i686-w64-mingw32-objdump")

# Mirrors cmake/WarzConfig.cmake. Kept as one list rather than reconstructed from CMake
# so this script runs without configuring the project.
INCLUDES = [
    "src/Eternity/Include", "src/Eternity", "src/GameEngine",
    "src/EclipseStudio/Sources", "src/ServerNetPackets",
    "src/External",
    "src/External/dxsdk/Include", "src/External/Scaleform3/Include",
    "src/External/ChilKat/Include", "src/External/Steam",
    "src/External/ts3_sdk_3/include", "src/External/GameBlocks",
    "src/External/RakNet/Source",
    "src/External/PhysX/physx-include", "src/External/PhysX/pxshared-include",
    "src/External/PhysX/compat",
    "src/External/Recast/Detour/Include", "src/External/Recast/DetourCrowd/Include",
    "src/External/Recast/Recast/Include", "src/External/RmlUi/Include",
]

DEFINES = [
    "NDEBUG",            # PxPreprocessor.h insists on exactly one of NDEBUG / _DEBUG
    "WIN32", "_WINDOWS",
    "WO_SERVER",         # the server build; see the note in the generated header
    "ENABLE_WEB_BROWSER=0",
    "PX_PHYSX_STATIC_LIB",
]

PROBE = """
#include "r3dPCH.h"
#include "r3d.h"
#include "multiplayer/P2PMessages.h"
int main() { return 0; }
"""

HEADER = "src/EclipseStudio/Sources/multiplayer/P2PMessages.h"

# Packet structs that P2PMessages.h declares but the build never compiles, so DWARF
# cannot see them and they cannot be asserted on.
#
# Both sit behind MISSION_TRIGGERS, which nothing defines anywhere in the tree -- it is
# only ever tested. They are dead code, not an oversight.
#
# Anything ELSE that is declared but not captured is a real gap: a packet nobody is
# checking the layout of. The completeness check below fails on it rather than quietly
# generating a baseline with a hole in it.
KNOWN_UNCOMPILED = {
    "PKT_S2C_CreateMissionTrigger_s",   # #ifdef MISSION_TRIGGERS
    "PKT_S2C_ShowMissionTrigger_s",     # #ifdef MISSION_TRIGGERS
}

RE_DECL = re.compile(r"^\s*struct\s+(PKT_\w+_s)\b", re.MULTILINE)


def check_complete(sizes):
    """Every packet struct the header declares must be in the baseline, or be a
    documented exclusion. Without this, a struct that stops compiling -- or a new one
    added behind a misspelled #ifdef -- silently drops out of the coverage."""
    with open(os.path.join(ROOT, HEADER)) as f:
        declared = set(RE_DECL.findall(f.read()))

    missing = declared - set(sizes) - KNOWN_UNCOMPILED
    if missing:
        sys.stderr.write(
            "these packet structs are declared in %s but absent from the debug info:\n"
            "  %s\n"
            "Either they no longer compile in this configuration, or they are newly\n"
            "guarded by an #ifdef. Fix the guard, or add them to KNOWN_UNCOMPILED with\n"
            "a note saying why.\n" % (HEADER, "\n  ".join(sorted(missing))))
        sys.exit(1)

    stale = KNOWN_UNCOMPILED & set(sizes)
    if stale:
        sys.stderr.write(
            "these are listed in KNOWN_UNCOMPILED but DO compile now: %s\n"
            "Remove them from the exclusion list so their layout gets asserted.\n"
            % ", ".join(sorted(stale)))
        sys.exit(1)

# A DW_TAG_structure_type DIE, then its attributes, until the next DIE at any depth.
RE_TAG = re.compile(r"<[0-9a-f]+><[0-9a-f]+>:\s+Abbrev Number:\s+\d+\s+\((DW_TAG_\w+)\)")
RE_NAME = re.compile(r"DW_AT_name\s*:\s*(?:\(indirect string, offset: 0x[0-9a-f]+\):\s*)?(\S+)")
RE_SIZE = re.compile(r"DW_AT_byte_size\s*:\s*(\d+)")


def compile_probe(tmpdir):
    src = os.path.join(tmpdir, "packet_probe.cpp")
    obj = os.path.join(tmpdir, "packet_probe.o")
    with open(src, "w") as f:
        f.write(PROBE)

    cmd = [CXX, "-std=c++20", "-fms-extensions", "-msse2", "-w",
           "-g", "-fno-eliminate-unused-debug-types", "-c", src, "-o", obj]
    cmd += ["-D" + d for d in DEFINES]
    cmd += ["-I" + os.path.join(ROOT, i) for i in INCLUDES]

    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.stderr.write("probe compilation failed:\n" + r.stderr[:4000] + "\n")
        sys.exit(1)
    return obj


def read_sizes(obj):
    r = subprocess.run([OBJDUMP, "--dwarf=info", obj], capture_output=True, text=True)
    if r.returncode != 0:
        sys.stderr.write("objdump failed:\n" + r.stderr[:2000] + "\n")
        sys.exit(1)

    sizes = {}
    cur_tag = None
    cur_name = None
    cur_size = None

    def flush():
        if cur_tag == "DW_TAG_structure_type" and cur_name and cur_size is not None:
            if cur_name.startswith("PKT_") and cur_name.endswith("_s"):
                # A type can appear more than once across DIEs; the size is the same
                # every time, and a disagreement would mean two distinct types sharing
                # a name, which is worth failing on rather than silently picking one.
                prev = sizes.get(cur_name)
                if prev is not None and prev != cur_size:
                    sys.stderr.write(
                        "conflicting sizes for %s: %d and %d\n" % (cur_name, prev, cur_size))
                    sys.exit(1)
                sizes[cur_name] = cur_size

    for line in r.stdout.splitlines():
        m = RE_TAG.search(line)
        if m:
            flush()
            cur_tag, cur_name, cur_size = m.group(1), None, None
            continue
        m = RE_NAME.search(line)
        if m and cur_name is None:
            cur_name = m.group(1)
            continue
        m = RE_SIZE.search(line)
        if m and cur_size is None:
            cur_size = int(m.group(1))
    flush()

    return sizes


def render(sizes):
    lines = [
        "// GENERATED by tools/gen_packet_layout.py -- do not edit.",
        "//",
        "// The frozen wire layout of every PKT_* struct in P2PMessages.h, as laid out by",
        "// the cross-compiler this port ships with. Regenerate ONLY alongside a",
        "// deliberate packet change, and bump P2PNET_VERSION in the same commit.",
        "//",
        "// Included by test_packet_layout.cpp, which turns each entry into a",
        "// static_assert. See that file for what these numbers do and do not prove.",
        "",
        "// clang-format off",
        "",
    ]
    for name in sorted(sizes):
        lines.append("WARZ_PACKET_SIZE(%s, %d)" % (name, sizes[name]))
    lines += ["", "// clang-format on", ""]
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true",
                    help="exit non-zero if the baseline would change; write nothing")
    args = ap.parse_args()

    with tempfile.TemporaryDirectory() as tmpdir:
        sizes = read_sizes(compile_probe(tmpdir))

    if not sizes:
        sys.stderr.write("no PKT_*_s structures found in the debug info -- "
                         "check that -fno-eliminate-unused-debug-types took effect\n")
        sys.exit(1)

    check_complete(sizes)

    text = render(sizes)

    if args.check:
        existing = open(OUT).read() if os.path.exists(OUT) else ""
        if existing != text:
            sys.stderr.write(
                "packet layout baseline is out of date.\n"
                "If a packet change was intended, run tools/gen_packet_layout.py and "
                "bump P2PNET_VERSION.\n")
            sys.exit(1)
        print("packet layout baseline up to date (%d structs)" % len(sizes))
        return

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        f.write(text)
    print("wrote %s (%d structs)" % (os.path.relpath(OUT, ROOT), len(sizes)))


if __name__ == "__main__":
    main()
