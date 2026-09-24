#!/bin/bash
# Build the matching decompilation and report progress.
#
#   tools/decomp_build.sh [-r] [unit]
#
#   -r      reconfigure first (regenerates build.ninja)
#   unit    an objdiff unit name to report in detail, e.g. main/Kyoto/Alloc/CGameAllocator
#
# The MWCC compilers, dtk, wibo and objdiff-cli live in the Metroid Prime port's
# tree; override with MP_TOOLCHAIN_DIR. The originals (orig/G2ME01) come from an
# owned disc via tools/extract_disc_file.py and are checked by build.sha1.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

MP_TOOLCHAIN_DIR="${MP_TOOLCHAIN_DIR:-$REPO_ROOT/../MetroidPrimePort}"
COMPILERS="$MP_TOOLCHAIN_DIR/build/compilers"
DTK="$MP_TOOLCHAIN_DIR/build/tools/dtk"
WIBO="$MP_TOOLCHAIN_DIR/build/tools/wibo"

for tool in "$COMPILERS/GC/2.7/mwcceppc.exe" "$DTK" "$WIBO"; do
  [ -e "$tool" ] || { echo "error: missing $tool (set MP_TOOLCHAIN_DIR)" >&2; exit 1; }
done

NINJA="$(command -v ninja || true)"
if [ -x "$MP_TOOLCHAIN_DIR/build/review-tools/bin/ninja" ]; then
  NINJA="$MP_TOOLCHAIN_DIR/build/review-tools/bin/ninja"
fi
[ -n "$NINJA" ] || { echo "error: no ninja found" >&2; exit 1; }

mkdir -p build/tools
for tool in dtk wibo objdiff-cli sjiswrap.exe; do
  [ -e "build/tools/$tool" ] || cp "$MP_TOOLCHAIN_DIR/build/tools/$tool" build/tools/
done

reconfigure=0
[ "${1:-}" = "-r" ] && { reconfigure=1; shift; }

if [ "$reconfigure" = 1 ] || [ ! -f build.ninja ]; then
  python3 configure.py --version G2ME01 \
    --compilers "$COMPILERS" \
    --dtk "$DTK" \
    --wrapper "$WIBO" \
    --build-dir build
fi

"$NINJA"

./build/tools/objdiff-cli report generate -o build/report.json >/dev/null
python3 - "$@" <<'PY'
import json, os, sys
report = json.load(open('build/report.json'))
measured = report['measures']
print("All:  {:.2f}% fuzzy, {:.2f}% matched, {:.2f}% linked ({} / {} functions)".format(
    measured['fuzzy_match_percent'], measured['matched_code_percent'],
    measured['complete_code_percent'], measured['matched_functions'], measured['total_functions']))
wanted = [a for a in sys.argv[1:] if not a.startswith('-')]
for unit in report['units']:
    if wanted and not any(unit['name'] == w or unit['name'].endswith('/' + w) for w in wanted):
        continue
    if not wanted and not unit['name'].startswith('main/'):
        continue
    m = unit['measures']
    if not wanted and m['fuzzy_match_percent'] >= 100.0:
        continue
    print("{}: {:.2f}% fuzzy, {:.2f}% matched ({} / {} functions)".format(
        unit['name'], m['fuzzy_match_percent'], m['matched_code_percent'],
        m['matched_functions'], m['total_functions']))
    if wanted:
        for fn in unit['functions']:
            pct = float(fn.get('fuzzy_match_percent', 0.0))
            if pct < 100.0:
                print("   {:52s} {:6.2f}%  {} bytes".format(fn['name'], pct, fn.get('size', 0)))
PY
