#!/usr/bin/env python3
"""Rank this codebase's headers by what they actually cost the build.

A header's cost is not how big it is. It is:

    (number of translation units that include it) x (seconds to parse it)

Both halves matter and neither is guessable. r3d.h reaches 208 of WarZ's 209 TUs but is
cheap on its own; r3dPCH.h reaches nearly everything AND costs 4.98 s, which is why
precompiling it was worth more than any other single change to this build. Ranking by
either factor alone gets the priority list wrong.

This exists so that fan-out work -- forward declarations, splitting headers -- is aimed
by measurement rather than by intuition. It is the tool, not the surgery: it tells you
which header to look at, and says nothing about whether untangling it is worth the risk.

Usage:
    tools/header_cost.py                      # rank by reach (fast, no compiling)
    tools/header_cost.py --measure 15         # also time the top 15, rank by real cost
    tools/header_cost.py --build-dir build-dev

Reads the compiler's own dependency data, so it reports what was really included after
the preprocessor was done with it -- not what the #include lines suggest. Both the Ninja
deps log and Makefile .d files are understood; see collect_deps().
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
MODERN = os.path.dirname(HERE)

# Headers under these roots are ours and therefore actionable. System, CRT, STL and
# vendored third-party headers are excluded: they dominate any raw count (445 of an
# average TU's 888 are system headers) and none of them are ours to restructure.
OURS = ("src/Eternity/", "src/GameEngine/", "src/EclipseStudio/", "server/src/",
        "src/ServerNetPackets/")


def _is_ours(path):
    rel = os.path.relpath(path, MODERN)
    return not rel.startswith("..") and rel.startswith(OURS)


def collect_deps(build_dir):
    """Return {header_abspath: set(TUs that include it)}.

    Two sources, because the generator decides which one exists:

      Ninja  -- reads .d files then deletes them, keeping the data in .ninja_deps.
                `ninja -t deps` dumps that back out as text. This is why the function
                cannot simply glob for *.d and why doing so silently returned nothing
                the first time the build moved to Ninja.
      Make   -- leaves the .d files on disk next to each object.
    """
    deps = {}

    if os.path.exists(os.path.join(build_dir, ".ninja_deps")):
        ninja = shutil.which("ninja")
        if not ninja:
            sys.exit("build dir is Ninja but ninja is not on PATH")
        out = subprocess.run([ninja, "-C", build_dir, "-t", "deps"],
                             capture_output=True, text=True).stdout
        tu = None
        for line in out.splitlines():
            if not line.startswith(" "):
                # "path/to/x.obj: #deps 123, deps mtime ... (VALID)"
                tu = line.split(":", 1)[0].strip() or None
                continue
            if tu:
                h = os.path.normpath(os.path.join(build_dir, line.strip()))
                deps.setdefault(h, set()).add(tu)
        return deps

    for root, _dirs, files in os.walk(build_dir):
        for f in files:
            if not f.endswith(".d"):
                continue
            path = os.path.join(root, f)
            with open(path, encoding="utf-8", errors="replace") as fh:
                text = fh.read()
            if ":" not in text:
                continue
            tu, _, body = text.partition(":")
            for tok in body.replace("\\\n", " ").split():
                if tok.endswith((".h", ".hpp", ".hxx", ".inl", ".tcc")):
                    h = os.path.normpath(os.path.join(build_dir, tok))
                    deps.setdefault(h, set()).add(tu.strip())
    return deps


def representative_flags(build_dir):
    """A real compile command to measure headers with.

    Taken from compile_commands.json rather than invented, so the measurement uses the
    same include path, defines and standard the build really uses. Picks an
    EclipseStudio TU: it is the configuration with the widest include path, so any
    header in the tree resolves against it.
    """
    ccj = os.path.join(build_dir, "compile_commands.json")
    if not os.path.exists(ccj):
        sys.exit("no compile_commands.json in %s -- configure with "
                 "CMAKE_EXPORT_COMPILE_COMMANDS=ON" % build_dir)
    with open(ccj, encoding="utf-8") as fh:
        entries = json.load(fh)

    chosen = None
    for e in entries:
        if "EclipseStudio" in e["file"]:
            chosen = e
            break
    chosen = chosen or entries[0]

    args = chosen.get("arguments")
    if not args:
        args = chosen["command"].split()

    # Drop the parts that name the input and output, and anything that would force a
    # precompiled header in -- measuring a header THROUGH the PCH would report ~0 for
    # every header the PCH already contains, which is the opposite of informative.
    out, skip = [], False
    for i, a in enumerate(args):
        if skip:
            skip = False
            continue
        if a in ("-c", "-o", "-include"):
            skip = a in ("-o", "-include")
            continue
        if a.endswith((".cpp", ".c", ".cc")) and i > 0:
            continue
        if a.startswith("-Winvalid-pch"):
            continue
        out.append(a)
    return chosen["directory"], out


def measure(header, directory, args, stub_path):
    """Seconds to parse one header, standalone."""
    with open(stub_path, "w", encoding="utf-8") as fh:
        fh.write('#include "%s"\n' % header.replace("\\", "/"))
    cmd = args + ["-fsyntax-only", stub_path]
    start = time.time()
    r = subprocess.run(cmd, cwd=directory, capture_output=True, text=True)
    elapsed = time.time() - start
    return elapsed if r.returncode == 0 else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default="build")
    ap.add_argument("--measure", type=int, default=0, metavar="N",
                    help="time the top N headers standalone and rank by real cost")
    ap.add_argument("--top", type=int, default=25)
    args = ap.parse_args()

    build_dir = args.build_dir
    if not os.path.isabs(build_dir):
        build_dir = os.path.join(MODERN, build_dir)
    if not os.path.isdir(build_dir):
        sys.exit("no such build directory: %s" % build_dir)

    deps = collect_deps(build_dir)
    if not deps:
        sys.exit("no dependency data in %s -- has anything been built there?" % build_dir)

    ours = [(h, len(tus)) for h, tus in deps.items() if _is_ours(h)]
    ours.sort(key=lambda kv: -kv[1])
    total_tus = len({tu for tus in deps.values() for tu in tus})

    print("%d translation units, %d project headers\n" % (total_tus, len(ours)))

    if not args.measure:
        print("%-58s %6s  %s" % ("header", "TUs", "reach"))
        for h, n in ours[:args.top]:
            print("%-58s %6d  %5.1f%%" % (os.path.relpath(h, MODERN), n,
                                          100.0 * n / total_tus))
        print("\nRe-run with --measure N to price these and rank by TUs x parse time.")
        return

    directory, cflags = representative_flags(build_dir)
    stub = os.path.join(build_dir, "_header_cost_stub.cpp")

    rows = []
    for h, n in ours[:args.measure]:
        secs = measure(h, directory, cflags, stub)
        rows.append((h, n, secs))
        print(".", end="", flush=True)
    print("\n")
    os.remove(stub)

    # Headers that do not compile standalone are reported rather than dropped: needing
    # a specific include order is itself a finding, and one of the things fan-out work
    # would fix.
    ok = [r for r in rows if r[2] is not None]
    bad = [r for r in rows if r[2] is None]
    ok.sort(key=lambda r: -(r[1] * r[2]))

    print("%-52s %6s %8s %10s" % ("header", "TUs", "parse", "TU-seconds"))
    for h, n, secs in ok:
        print("%-52s %6d %7.2fs %9.0fs" % (os.path.relpath(h, MODERN), n, secs, n * secs))

    if bad:
        print("\nDid not compile standalone (needs a specific include order) -- %d:" % len(bad))
        for h, n, _ in bad:
            print("    %-52s %6d TUs" % (os.path.relpath(h, MODERN), n))


if __name__ == "__main__":
    main()
