#!/usr/bin/env python3
"""
Scan for MSVC-isms and C++17/20 removals across the ported tree.

The original codebase targeted MSVC in permissive mode, which accepts a large set of
non-ISO constructs. Compiling under /permissive- (or any other compiler) surfaces them
one at a time, and each compile pass over the engine takes minutes -- far too slow to
iterate against. This finds them all statically in seconds so they can be fixed in bulk.

Two categories:
  MSVC   -- non-ISO constructs MSVC accepts
  CXX20  -- things removed or changed by C++17/C++20

Checks marked HEURISTIC in their note are pattern-matches that cannot distinguish
legal from illegal usage without semantic analysis. Eternity compiles 100% clean
under strict GCC while still producing hits on those, so treat them as leads and
confirm with tools/probe.sh -- never mass-apply a fix based on them alone.

Usage:
    ./tools/scan_conformance.py                # summary counts
    ./tools/scan_conformance.py --detail       # every hit with file:line
    ./tools/scan_conformance.py --only NAME    # one check
    ./tools/scan_conformance.py --skip-vendor  # exclude src/External (default on)
"""

import argparse
import os
import re
import sys
from collections import defaultdict

MODERN_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE_EXT = {".cpp", ".CPP", ".h", ".H", ".hpp", ".HPP", ".c", ".inl"}

# Vendored third-party is built permissively and is not ours to fix.
VENDOR_PREFIXES = ("src/External/RakNet", "src/External/pugiXML", "src/Eternity/Source/ZLib")

# (name, category, regex, note)
CHECKS = [
    # ---------------- MSVC non-ISO constructs ----------------
    ("inline-asm", "MSVC",
     r'(?:^|[^\w])__?asm\b',
     "MSVC inline assembly; rejected by GCC/Clang and by MSVC itself on x64"),

    ("dot-hash-hash", "MSVC",
     r'\.##',
     "superfluous ## after . in a macro; ill-formed token paste"),

    ("colon-hash-hash", "MSVC",
     r'::##|##::',
     "## adjacent to :: does not form a valid preprocessing token"),

    ("multiword-functional-cast", "MSVC",
     r'\b(?:unsigned|signed)\s+(?:char|int|short|long)\s*\([^)]',
     "functional cast needs a single type-name token in ISO C++"),

    ("qualified-member-decl", "MSVC",
     r'\b(\w+)\s*&?\s*\1::\w+\s*\([^;]*\)\s*(?:const)?\s*\{',
     "member declared with its own class qualifier inside the class body"),

    ("stdext", "MSVC",
     r'\bstdext::',
     "MSVC-only namespace; removed from modern MSVC too"),

    ("hash-map-header", "MSVC",
     r'#\s*include\s*<hash_(?:map|set)>',
     "MSVC/SGI extension header; use <unordered_map>"),

    # NOTE: [ \t]+ not \s+ -- \s crosses newlines and matches the next line's
    # first identifier, which produced ~1100 false positives.
    ("tokens-after-endif", "MSVC",
     r'^[ \t]*#[ \t]*(?:endif|else)[ \t]+[A-Za-z_]',
     "extra tokens after #endif/#else"),

    ("pragma-comment-lib", "MSVC",
     r'#\s*pragma\s+comment\s*\(\s*lib',
     "MSVC linker pragma; CMake handles linking"),

    ("bogus-namespace-int", "MSVC",
     r'\br3dTL::(?:u?int(?:8|16|32|64)_t|size_t)\b',
     "qualified name that does not exist; unchecked by MSVC two-phase lookup"),

    ("backslash-include", "MSVC",
     r'#\s*include\s*["<][^">]*\\',
     "backslash path separator in #include"),

    ("ms-typeof", "MSVC",
     r'\b__if_exists\b|\b__if_not_exists\b|\b__typeof\b',
     "MSVC-only extension"),

    ("declspec-novtable", "MSVC",
     r'__declspec\s*\(\s*novtable\s*\)',
     "MSVC-only; harmless to drop"),

    # This one alone accounted for 70 of 82 failures on the first engine probe:
    # every TU pulling in r3dNetwork.h died on it.
    ("null-pure-specifier", "MSVC",
     r'^\s*virtual\b[^;=]*=\s*NULL\s*;',
     "'= NULL' as a pure specifier; ISO C++ requires the literal 0"),

    # Found while driving Eternity to a clean strict build.
    ("bare-sizeof-type", "MSVC",
     r'\bsizeof\s+[A-Za-z_]\w*\s*[),;]',
     "HEURISTIC: sizeof without parens. Legal on an expression, illegal on a "
     "type name -- the regex cannot tell them apart. Verify with the compiler."),

    ("single-underscore-cdecl", "MSVC",
     r'(?<![_\w])_cdecl\b',
     "pre-standard MSVC spelling; use __cdecl"),

    ("extra-qualification", "MSVC",
     r'^[ \t]+[\w:]+[ \t]*&?[ \t]*(\w+)::\w+[ \t]*\(',
     "HEURISTIC: also matches legal indented out-of-line definitions. "
     "Verify with the compiler before touching."),

    ("msvc-stl-internals", "MSVC",
     r'std::_(?:String_base|Xlen|Xran|Xlength_error|Container_base)',
     "Microsoft STL internals; no libstdc++ equivalent"),

    ("friend-then-static", "MSVC",
     r'^\s*friend\s+(?:void|int|bool)\s+\w+\s*\(',
     "HEURISTIC: friend gives external linkage; only a conflict if the "
     "definition is later marked static."),

    ("friend-only-decl", "MSVC",
     r'^\s*friend\s+class\s+(\w+)\s*;',
     "friend declaration alone does not introduce the name into the enclosing "
     "scope in ISO C++; verify a real forward declaration exists"),

    # ---------------- C++17 / C++20 removals ----------------
    ("allocator-void-members", "CXX20",
     r'std::allocator<\s*void\s*>::\s*(?:const_)?pointer',
     "std::allocator<void> member typedefs removed in C++20"),

    ("dynamic-exception-spec", "CXX20",
     r'\)\s*throw\s*\(\s*\)',
     "throw() dynamic exception specification removed in C++20; use noexcept"),

    ("register-keyword", "CXX20",
     r'(?:^|[^\w])register\s+(?:int|char|float|double|unsigned|long|short)\b',
     "register storage class removed in C++17"),

    ("auto-ptr", "CXX20",
     r'\bstd::auto_ptr\b',
     "removed in C++17; use std::unique_ptr"),

    ("random-shuffle", "CXX20",
     r'\bstd::random_shuffle\b',
     "removed in C++17; use std::shuffle"),

    ("unary-binary-function", "CXX20",
     r'\bstd::(?:unary_function|binary_function)\b',
     "removed in C++17"),

    ("bind1st-2nd", "CXX20",
     r'\bstd::(?:bind1st|bind2nd|ptr_fun|mem_fun|mem_fun_ref)\b',
     "removed in C++17; use std::bind or lambdas"),

    ("result-of", "CXX20",
     r'\bstd::result_of\b',
     "removed in C++20; use std::invoke_result"),

    ("is-pod", "CXX20",
     r'\bstd::is_pod\b',
     "deprecated in C++20; use is_trivially_copyable + is_standard_layout"),

    ("bool-increment", "CXX20",
     r'\b(\w+)\s*\+\+\s*;\s*//\s*bool',
     "++ on bool removed in C++17 (heuristic; verify manually)"),

    ("uncaught-exception", "CXX20",
     r'\bstd::uncaught_exception\b',
     "removed in C++20; use uncaught_exceptions()"),

    ("std-iterator-base", "CXX20",
     r':\s*public\s+std::iterator\s*<',
     "std::iterator deprecated in C++17"),
]


def is_vendor(rel):
    return any(rel.startswith(p) for p in VENDOR_PREFIXES)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--detail", action="store_true", help="list every hit")
    ap.add_argument("--only", help="run a single check by name")
    ap.add_argument("--include-vendor", action="store_true")
    args = ap.parse_args()

    os.chdir(MODERN_DIR)

    checks = [c for c in CHECKS if not args.only or c[0] == args.only]
    if args.only and not checks:
        print(f"no such check: {args.only}", file=sys.stderr)
        print("available:", ", ".join(c[0] for c in CHECKS), file=sys.stderr)
        return 2

    compiled = [(n, cat, re.compile(rx, re.M), note) for n, cat, rx, note in checks]

    hits = defaultdict(list)
    n_scanned = 0

    for dirpath, dirnames, filenames in os.walk("."):
        dirnames[:] = [d for d in dirnames if d not in (".git", "build", "__pycache__")]
        for fn in filenames:
            if os.path.splitext(fn)[1] not in SOURCE_EXT:
                continue
            rel = os.path.relpath(os.path.join(dirpath, fn), ".").replace(os.sep, "/")
            if not args.include_vendor and is_vendor(rel):
                continue
            try:
                text = open(rel, encoding="utf-8", errors="surrogateescape").read()
            except OSError:
                continue
            n_scanned += 1

            for name, cat, rx, note in compiled:
                for m in rx.finditer(text):
                    line = text.count("\n", 0, m.start()) + 1
                    hits[name].append((rel, line, m.group(0).strip()[:60]))

    print(f"Scanned {n_scanned} source files in {MODERN_DIR}\n")

    width = max(len(c[0]) for c in checks)
    total = 0
    for name, cat, _, note in checks:
        h = hits.get(name, [])
        total += len(h)
        if not h:
            continue
        files = len({x[0] for x in h})
        print(f"  [{cat:5}] {name:<{width}}  {len(h):5} hits  {files:4} files   {note}")

    print(f"\n  TOTAL: {total} hits")

    if args.detail:
        for name, cat, _, note in checks:
            h = hits.get(name, [])
            if not h:
                continue
            print(f"\n=== {name} ({cat}) -- {note}")
            for rel, line, snippet in h:
                print(f"  {rel}:{line}: {snippet}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
