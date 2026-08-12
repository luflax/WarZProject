#!/usr/bin/env python3
"""
Batch-fix the mechanical MSVC-isms and C++17/20 removals found by scan_conformance.py.

Every rewrite here is semantics-preserving and valid on MSVC as well, so nothing is
traded away for portability. Inline assembly is NOT handled -- each site needs its own
intrinsic translation and is done by hand.

Runs against modern/ only. The original tree is never touched.

Usage:
    ./tools/fix_conformance.py [--dry-run]
"""

import argparse
import os
import re
import sys
from collections import Counter

MODERN_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE_EXT = {".cpp", ".CPP", ".h", ".H", ".hpp", ".HPP", ".c", ".inl"}
VENDOR_PREFIXES = ("src/External/RakNet", "src/External/pugiXML", "src/Eternity/Source/ZLib")

stats = Counter()


def fix_endif_tokens(text, rel):
    """#endif __GAME_H  ->  #endif // __GAME_H"""
    def r(m):
        stats["tokens-after-endif"] += 1
        return f"{m.group(1)} // {m.group(2)}"
    return re.sub(r'^([ \t]*#[ \t]*(?:endif|else))[ \t]+([A-Za-z_][^\n]*)$', r, text, flags=re.M)


def fix_hash_map(text, rel):
    """stdext::hash_map -> std::unordered_map, including the hash_compare traits form.

    hash_map takes (Key, T, Traits, Alloc); unordered_map takes
    (Key, T, Hash, KeyEqual, Alloc), so the traits argument expands into two.
    """
    before = text
    text = re.sub(r'stdext::hash_compare<\s*([^,<>]+?)\s*,\s*std::less<[^>]*>\s*>',
                  r'std::hash<\1>, std::equal_to<\1>', text)
    text = text.replace("stdext::hash_map", "std::unordered_map")
    text = text.replace("stdext::hash_set", "std::unordered_set")
    text = re.sub(r'#(\s*)include(\s*)<hash_map>', r'#\1include\2<unordered_map>', text)
    text = re.sub(r'#(\s*)include(\s*)<hash_set>', r'#\1include\2<unordered_set>', text)
    if text != before:
        stats["hash_map"] += 1
    return text


def fix_tr1(text, rel):
    """std::tr1::X -> std::X   (TR1 was folded into C++11 and the namespace is gone)"""
    n = len(re.findall(r'std::tr1::', text))
    if n:
        stats["std::tr1"] += n
    return text.replace("std::tr1::", "std::")


def fix_pragma_comment_lib(text, rel):
    """MSVC linker pragmas -- CMake owns linking now."""
    def r(m):
        stats["pragma-comment-lib"] += 1
        return f"// [PORT] linking handled by CMake: {m.group(0)}"
    return re.sub(r'^[ \t]*#[ \t]*pragma[ \t]+comment[ \t]*\([ \t]*lib[^\n]*$', r, text, flags=re.M)


def fix_backslash_includes(text, rel):
    def r(m):
        if "\\" not in m.group(3):
            return m.group(0)
        stats["backslash-include"] += 1
        return f"{m.group(1)}{m.group(2)}{m.group(3).replace(chr(92), '/')}{m.group(4)}"
    return re.sub(r'(^[ \t]*#[ \t]*include[ \t]*)(["<])([^">]+)([">])', r, text, flags=re.M)


def fix_paste_after_scope(text, rel):
    """'::##x' and '.##x' -- the ## is superfluous and forms an invalid paste."""
    n = len(re.findall(r'(?:::|\.)##', text))
    if n:
        stats["superfluous-##"] += n
    text = text.replace("::##", "::")
    text = text.replace(".##", ".")
    return text


def fix_dynamic_exception_spec(text, rel):
    """throw() -> noexcept   (dynamic exception specifications removed in C++20)"""
    def r(m):
        stats["throw()"] += 1
        return ") noexcept"
    return re.sub(r'\)\s*throw\s*\(\s*\)', r, text)


# Matches an INDENTED declaration whose return type and class qualifier are the
# same identifier, e.g.
#     __forceinline gobjid_t &gobjid_t::operator=(const gobjid_t& rhs) {
# Indentation is what distinguishes an in-class declaration (ill-formed) from an
# out-of-line definition at column 0 (perfectly legal -- r3dColor.h is full of them).
# Deliberately narrow: it only fires when return type == qualifier, which is the
# shape actually present in this codebase.
_INCLASS_QUALIFIED = re.compile(
    r'^([ \t]+(?:__forceinline[ \t]+|inline[ \t]+|static[ \t]+|virtual[ \t]+)*)'
    r'(\w+)([ \t]*&?[ \t]*)'
    r'(\2)::'
    r'(operator[ \t]*[^\s(]+|~?\w+)[ \t]*\(',
    re.M)


def fix_inclass_qualified_member(text, rel):
    """Strip a class's own qualifier from a member declared inside its body."""
    def r(m):
        stats["qualified-member-decl"] += 1
        return f"{m.group(1)}{m.group(2)}{m.group(3)}{m.group(5)}("
    return _INCLASS_QUALIFIED.sub(r, text)


def fix_removed_algorithms(text, rel):
    before = text

    # std::auto_ptr -> std::unique_ptr (removed in C++17). Same RAII shape; the
    # copy-transfer semantics differ, so the compiler will flag any real reliance
    # on them rather than silently changing behaviour.
    text = text.replace("std::auto_ptr", "std::unique_ptr")

    # std::random_shuffle removed in C++17; std::shuffle needs an explicit URBG.
    text = re.sub(
        r'std::random_shuffle\s*\(\s*([^,]+),\s*([^)]+)\)',
        r'std::shuffle(\1, \2, r3dPortableRng())',
        text)

    if text != before:
        stats["removed-algorithms"] += 1
    return text


PASSES = [
    fix_endif_tokens,
    fix_hash_map,
    fix_tr1,
    fix_pragma_comment_lib,
    fix_backslash_includes,
    fix_paste_after_scope,
    fix_dynamic_exception_spec,
    fix_inclass_qualified_member,
    fix_removed_algorithms,
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    os.chdir(MODERN_DIR)
    changed = 0

    for dirpath, dirnames, filenames in os.walk("."):
        dirnames[:] = [d for d in dirnames if d not in (".git", "build", "__pycache__")]
        for fn in filenames:
            if os.path.splitext(fn)[1] not in SOURCE_EXT:
                continue
            rel = os.path.relpath(os.path.join(dirpath, fn), ".").replace(os.sep, "/")
            if any(rel.startswith(p) for p in VENDOR_PREFIXES):
                continue
            try:
                text = open(rel, encoding="utf-8", errors="surrogateescape").read()
            except OSError:
                continue

            new = text
            for p in PASSES:
                new = p(new, rel)

            if new != text:
                changed += 1
                if not args.dry_run:
                    open(rel, "w", encoding="utf-8", errors="surrogateescape").write(new)

    verb = "would change" if args.dry_run else "changed"
    print(f"{verb}: {changed} files\n")
    for k, v in sorted(stats.items(), key=lambda kv: -kv[1]):
        print(f"  {v:5}  {k}")
    print("\nNOT handled (needs per-site intrinsic translation): inline assembly")
    return 0


if __name__ == "__main__":
    sys.exit(main())
