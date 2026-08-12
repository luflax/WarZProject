#!/usr/bin/env bash
#
# Link-gap analysis: what would fail to link, and who is responsible for it?
#
# Takes object directories produced by codegen.sh, collects every undefined symbol,
# subtracts everything those objects define, and classifies what is left. This is how
# the Milestone B work list in ../../MILESTONE-B-PLAN.md was derived -- it turns
# "linking sounds hard" into a table with counts.
#
# Usage:
#   ./tools/codegen.sh src/Eternity          eternity
#   ./tools/codegen.sh src/GameEngine        gameengine
#   ./tools/codegen.sh src/EclipseStudio     eclipse
#   ./tools/linkcheck.sh eternity gameengine eclipse
#
# Env:
#   OBJDIR=<path>   default .objs   (must match codegen.sh)
#
set -uo pipefail

MODERN_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$MODERN_DIR"

OBJDIR=${OBJDIR:-.objs}
NM=${NM:-i686-w64-mingw32-nm}
FILT=${FILT:-i686-w64-mingw32-c++filt}
WORK="$OBJDIR/.linkcheck"; mkdir -p "$WORK"

[[ $# -gt 0 ]] || { echo "usage: linkcheck.sh <objdir-tag> [tag...]"; exit 2; }

DIRS=()
for t in "$@"; do
  [[ -d "$OBJDIR/$t" ]] || { echo "no such object dir: $OBJDIR/$t (run codegen.sh first)"; exit 2; }
  DIRS+=("$OBJDIR/$t")
done

mapfile -t OBJS < <(find "${DIRS[@]}" -name '*.o')
echo "objects: ${#OBJS[@]}"

printf '%s\n' "${OBJS[@]}" | xargs $NM --defined-only 2>/dev/null \
  | awk '{print $3}' | grep -v '^$' | sort -u > "$WORK/defined.txt"
printf '%s\n' "${OBJS[@]}" | xargs $NM --undefined-only 2>/dev/null \
  | awk '{print $2}' | grep -v '^$' | sort -u > "$WORK/undefined.txt"

comm -23 "$WORK/undefined.txt" "$WORK/defined.txt" > "$WORK/unresolved.txt"
$FILT < "$WORK/unresolved.txt" > "$WORK/unresolved.demangled.txt"

n() { grep -cE "$1" "$WORK/unresolved.demangled.txt"; }

total=$(wc -l < "$WORK/unresolved.txt")
cat <<EOF

defined:    $(wc -l < "$WORK/defined.txt")
undefined:  $(wc -l < "$WORK/undefined.txt")
unresolved: $total

Classification -- each row is a different owner, and a different fix:

  Win32 import (__imp_*)      $(n '^__imp_')   MinGW import libs; link flags only
  Win32 stdcall + GUIDs       $(n '@[0-9]+$|^_(IID|GUID|CLSID)')   as above
  third-party C++             $(n '^(physx|pugi|Rml|RakNet|dt[A-Z]|rc[A-Z])')   BUILD THE VENDORED LIBS
  PhysX C entry points        $(n '^_Px')   BUILD PHYSX
  zlib                        $(n '^_(inflate|deflate|compress|uncompress|adler32|crc32)')   compile Eternity/Source/ZLib/src/*.c
  libgcc / libstdc++          $(n '^_(_Unwind|__cxa|__chkstk|__gxx)')   automatic

Full list:      $WORK/unresolved.demangled.txt
EOF
