#!/bin/bash
# Why can this unit not be promoted to `Matching`?
#
#   tools/unit_fit.sh <unit> [unit...]      e.g. tools/unit_fit.sh Kyoto/Audio/CStaticAudioPlayer.cpp
#
# Promoting a unit swaps the retail-derived object for our compile in the link, so the unit
# reproduces the binary only if our object occupies exactly the bytes retail has there. This
# prints the claimed range against our object's sections, and - the part that actually explains
# a failure - the functions our object emits that the retail unit object does not define.
#
# Size alone is NOT a verdict. `Kyoto/Basics/RAssertDolphin.cpp` is `Matching` today with an
# object 112 bytes over its range, because the one extra function it emits (`hack__Fv`) is
# byte-identical to the bytes retail has immediately after the range - dtk's split simply
# attributed them to the neighbouring unit, so the overlap is harmless. A unit whose extra
# functions are *not* retail's bytes cannot flip, however good its percentages look.
#
# The only acceptance test is `tools/flip_test.sh`.
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"
BINUTILS=build/binutils
[ -x "$BINUTILS/powerpc-eabi-size" ] || { echo "error: $BINUTILS/powerpc-eabi-size missing" >&2; exit 1; }

if [ "$#" -eq 0 ]; then
  echo "usage: tools/unit_fit.sh <unit> [unit...]   (the unit name as configure.py writes it, .cpp optional)" >&2
  exit 2
fi

find_split() {
  local unit="$1" f
  for f in config/G2ME01/splits.txt config/G2ME01/rels/*/splits.txt; do
    [ -f "$f" ] || continue
    if grep -q "^${unit}:\$" "$f"; then echo "$f"; return 0; fi
  done
  return 1
}

obj_sizes() {
  [ -f "$1" ] || return 1
  "$BINUTILS/powerpc-eabi-size" -A "$1" 2>/dev/null | awk '$1 ~ /^\./ {print $1, $2}'
}

section_size() { awk -v s="$2" '$1==s {print $2; found=1} END {if (!found) print 0}' <<<"$1"; }

# Defined function symbols of an object: "size name" lines.
obj_functions() {
  [ -f "$1" ] || return 1
  "$BINUTILS/powerpc-eabi-nm" -S --defined-only "$1" 2>/dev/null |
    awk '$3 ~ /^[tTwW]$/ {print $4, $2}'
}

status=0
for arg in "$@"; do
  unit="${arg%.cpp}.cpp"
  base="${unit%.cpp}"
  if ! split_file="$(find_split "$unit")"; then
    echo "$unit: not declared in any splits.txt"; status=1; continue
  fi

  rel_module=""
  case "$split_file" in config/G2ME01/rels/*) rel_module="$(basename "$(dirname "$split_file")")";; esac
  if [ -n "$rel_module" ]; then
    ours_obj="build/G2ME01/$rel_module/src/$base.o"; retail_obj="build/G2ME01/$rel_module/obj/$base.o"
  else
    ours_obj="build/G2ME01/src/$base.o"; retail_obj="build/G2ME01/obj/$base.o"
  fi

  echo "== $unit  ($split_file)"
  ours="$(obj_sizes "$ours_obj")"
  retail="$(obj_sizes "$retail_obj")"
  if [ -z "$ours" ]; then
    echo "   no compiled object at $ours_obj - build it first (tools/decomp_build.sh)"
    continue
  fi

  over=0
  while read -r sec start end; do
    claimed=$(( ${end#end:} - ${start#start:} ))
    have=$(section_size "$ours" "$sec")
    their=$(section_size "$retail" "$sec")
    note=""
    if [ "$have" -gt "$claimed" ]; then note="over by $(( have - claimed ))"; over=$(( over + have - claimed )); fi
    [ -z "$note" ] && note="fits"
    printf "   %-10s claimed %6d   ours %6d   retail %6d   %s\n" "$sec" "$claimed" "$have" "$their" "$note"
  done < <(awk -v u="$unit:" '
      $0 == u { inblock=1; next }
      inblock && /^[A-Za-z0-9_\/.-]+\.cpp:$/ { inblock=0 }
      inblock && /^[[:space:]]*\.[a-z0-9_]+[[:space:]]/ { print $1, $2, $3 }
    ' "$split_file")

  # Symbols we emit that the retail unit object does not define: the thing that actually breaks
  # a flip, and invisible in the percentage.
  extra_total=0
  extra_lines="$(python3 - "$ours_obj" "$retail_obj" <<'PY'
import subprocess, sys
nm = 'build/binutils/powerpc-eabi-nm'
def fns(path):
    try:
        out = subprocess.run([nm, '-S', '--defined-only', path], capture_output=True, text=True).stdout
    except FileNotFoundError:
        return {}
    d = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) >= 4 and p[2] in 'tTwW':
            try: d[p[3]] = int(p[1], 16)
            except ValueError: pass
    return d
ours, retail = fns(sys.argv[1]), fns(sys.argv[2])
extra = {k: v for k, v in ours.items() if k not in retail}
for k, v in sorted(extra.items(), key=lambda kv: -kv[1])[:10]:
    print(f"   +{v:5d}  {k}")
print(f"#TOTAL {sum(extra.values())} {len(extra)}")
PY
)"
  echo "$extra_lines" | grep -v '^#TOTAL' | sed 's/^/   extra: /'
  extra_total=$(echo "$extra_lines" | awk '/^#TOTAL/{print $2}')
  extra_count=$(echo "$extra_lines" | awk '/^#TOTAL/{print $3}')
  if [ "${extra_count:-0}" -gt 0 ]; then
    echo "   $extra_count function(s) present in ours but not in the retail unit object, $extra_total bytes total"
    echo "   -> they are the likely cause of a failed flip; if they are retail's bytes (dtk put them in a"
    echo "      neighbouring unit) the flip still holds. tools/flip_test.sh is the only verdict."
  else
    echo "   no extra functions: our object defines only what the retail unit object does"
  fi
  [ "$over" -gt 0 ] && echo "   (sections over the claimed range by $over bytes in total - not a verdict by itself)"
done
exit $status
