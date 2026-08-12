#!/usr/bin/env python3
"""
Normalize #include directives for portability.

The original codebase was MSVC-only, so it relies on two Windows-isms that break
every other toolchain:

  1. Backslash separators   #include "TSG_STL\\TArray.h"
     Non-standard -- the C++ standard leaves '\\' in a header-name undefined, and
     GCC/Clang treat it as an escape rather than a separator.

  2. Case-insensitive paths  #include "TSG_STL/TArray.h"  ->  Tsg_stl/TArray.h
     Fine on NTFS, fatal on a case-sensitive filesystem.

Both are fixed by rewriting the directive text. Forward slashes and correct casing
work identically on MSVC, so this is a pure portability win with no downside.

Runs against modern/ only. The original tree is never touched.

Usage:
    ./tools/normalize_includes.py [--dry-run] [--verbose]
"""

import argparse
import os
import re
import sys

MODERN_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SOURCE_EXT = {".cpp", ".CPP", ".h", ".H", ".hpp", ".HPP", ".c", ".inl"}

# Roots that -I points at, in the order the compiler searches them.
INCLUDE_ROOTS = [
    "src/Eternity/Include",
    "src/Eternity",
    "src/GameEngine",
    "src/EclipseStudio/Sources",
    "src/External",
    "src/External/dxsdk/Include",
    "src/External/RakNet/Source",
    "src/External/PhysX/physx-include",
    "src/External/PhysX/pxshared-include",
    "src/External/Scaleform3/Include",
    "src/ServerNetPackets",
    "server/src/WO_GameServer/Sources",
    ".",
]

INCLUDE_RE = re.compile(r'^(\s*#\s*include\s*)(["<])([^">]+)([">])', re.MULTILINE)

# Absent third-party SDKs. Their include paths cannot be resolved on disk, so
# case-correction must skip them rather than reporting them as unresolved.
SHIMMED_PREFIXES = (
    "PunkBuster/", "VMProtect/", "GameBlocks", "Apex/", "AutodeskNav/",
    "Berkelium", "PhysX/", "Scaleform", "GFx", "fmod/", "Ck", "ts3", "steam",
)


def build_index(root):
    """Map lowercased relative path -> real relative path, for case correction."""
    index = {}
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in (".git", "build", "__pycache__")]
        for fn in filenames:
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, root).replace(os.sep, "/")
            index.setdefault(rel.lower(), rel)
    return index


def resolve(path, source_dir, index):
    """Return the correctly-cased path if it resolves, else None."""
    candidates = [os.path.normpath(os.path.join(source_dir, path))]
    candidates += [os.path.normpath(os.path.join(r, path)) for r in INCLUDE_ROOTS]

    for cand in candidates:
        key = cand.replace(os.sep, "/").lower()
        if key in index:
            return index[key]
    return None


def fix_case(path, source_dir, index):
    """Rewrite only the casing of `path`, preserving its relative form."""
    real = resolve(path, source_dir, index)
    if real is None:
        return None

    # Keep the directive relative in the same style it was written: match the
    # resolved real path's trailing components against the original depth.
    parts = path.replace("\\", "/").split("/")
    real_parts = real.split("/")
    tail = [p for p in parts if p not in ("..", ".")]
    if len(tail) > len(real_parts):
        return None

    fixed = real_parts[len(real_parts) - len(tail):]
    prefix = [p for p in parts if p in ("..", ".")]
    return "/".join(prefix + fixed)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    os.chdir(MODERN_DIR)
    print(f"Scanning {MODERN_DIR} ...")
    index = build_index(".")
    print(f"Indexed {len(index)} files\n")

    n_files = n_slash = n_case = 0

    for dirpath, dirnames, filenames in os.walk("."):
        dirnames[:] = [d for d in dirnames if d not in (".git", "build", "__pycache__")]
        for fn in filenames:
            if os.path.splitext(fn)[1] not in SOURCE_EXT:
                continue

            full = os.path.join(dirpath, fn)
            source_dir = dirpath
            try:
                with open(full, "r", encoding="utf-8", errors="surrogateescape") as fh:
                    text = fh.read()
            except OSError:
                continue

            file_slash = file_case = 0

            def repl(m):
                nonlocal file_slash, file_case
                lead, open_q, path, close_q = m.groups()
                new_path = path

                if "\\" in new_path:
                    new_path = new_path.replace("\\", "/")
                    file_slash += 1

                # NOTE: do NOT skip paths matching SHIMMED_PREFIXES here. An earlier
                # version did, and "fmod/" matched the REAL project header
                # GameEngine/fmod/SoundSys.h, leaving "fmod/soundsys.h" uncorrected
                # and breaking 9 GameEngine TUs. fix_case() already returns None for
                # anything it cannot resolve on disk, which covers the shims safely.
                corrected = fix_case(new_path, source_dir, index)
                if corrected and corrected != new_path:
                    new_path = corrected
                    file_case += 1

                if new_path == path:
                    return m.group(0)
                if args.verbose:
                    print(f"  {full}: {path} -> {new_path}")
                return f"{lead}{open_q}{new_path}{close_q}"

            new_text = INCLUDE_RE.sub(repl, text)

            if new_text != text:
                n_files += 1
                n_slash += file_slash
                n_case += file_case
                if not args.dry_run:
                    with open(full, "w", encoding="utf-8", errors="surrogateescape") as fh:
                        fh.write(new_text)

    verb = "would fix" if args.dry_run else "fixed"
    print(f"\n{verb}: {n_files} files")
    print(f"  backslash separators : {n_slash}")
    print(f"  case corrections     : {n_case}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
