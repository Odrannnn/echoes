#!/bin/bash
# Rebuild one object and print its unit's scores, without relinking the DOL.
#
#   tools/fast_try.sh <unit> [<unit> ...]      e.g. tools/fast_try.sh Kyoto/CPakFile
#
# `<unit>` is the unit's path under build/G2ME01/src. REL modules use the same path
# (configure.py keeps one src tree); the unit is then reported under its module name.
# Only the named object is rebuilt, so this is the loop to use while trying source
# variants: it is a second or two, against a full build plus objdiff.
#
# Mined from a lane's private helper (/tmp/opencode/w14) and generalised: no hardcoded
# toolchain path, and REL units work.
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"
MP_TOOLCHAIN_DIR="${MP_TOOLCHAIN_DIR:-$REPO_ROOT/../MetroidPrimePort}"
NINJA="$(command -v ninja || true)"
[ -x "$MP_TOOLCHAIN_DIR/build/review-tools/bin/ninja" ] && NINJA="$MP_TOOLCHAIN_DIR/build/review-tools/bin/ninja"
[ -n "$NINJA" ] || { echo "error: no ninja found" >&2; exit 1; }

[ "$#" -gt 0 ] || { sed -n '2,12p' "$0"; exit 2; }

for unit in "$@"; do
  "$NINJA" "build/G2ME01/src/$unit.o" >/dev/null || { echo "$unit: build FAILED"; continue; }
done
./build/tools/objdiff-cli report generate -o build/report.json >/dev/null 2>&1

python3 - "$@" <<'PY'
import json, sys
report = json.load(open('build/report.json'))
wanted = set(sys.argv[1:])
seen = set()
for unit in report['units']:
    name = unit['name']
    short = name[5:] if name.startswith('main/') else name.split('/', 1)[-1]
    if short not in wanted:
        continue
    seen.add(short)
    m = unit['measures']
    print("{}: {:.2f}% fuzzy, {:.2f}% matched code, {}/{} functions".format(
        name, float(m.get('fuzzy_match_percent') or 0), float(m.get('matched_code_percent') or 0),
        m.get('matched_functions', 0), m.get('total_functions', 0)))
    for fn in unit.get('functions', []):
        p = float(fn.get('fuzzy_match_percent') or 0.0)
        if p < 100.0:
            print("   {:6.2f}%  {:>6} B  {}".format(p, fn.get('size', 0), fn['name']))
for unit in sorted(wanted - seen):
    print("{}: no such unit in the report".format(unit))
PY
