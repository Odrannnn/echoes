#!/bin/bash
# Authoritative completion test for a unit.
#
#   tools/flip_test.sh <unit> [unit...]      e.g. tools/flip_test.sh Kyoto/CFrameDelayedKiller.cpp
#
# `ninja` links the DOL from build/G2ME01/obj/*.o, the objects dtk split out of
# retail; marking a unit Matching in configure.py swaps in our own object. So a unit
# is genuinely complete when the build still reproduces retail with our object in
# the link. This flips each unit, rebuilds, checks the DOL sha1 and all 86 RELs, and
# keeps the flips that hold while reverting the ones that do not.
#
# The objdiff percentages are necessary but not sufficient; this is the test that
# decides. tools/compare_unit.sh shows what differs first.
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

# Same arguments configure.py was last run with (recorded in build.ninja).
CONFIGURE_ARGS="$(sed -n 's/^configure_args = //p' build.ninja | tr -d '\\\n' | sed 's/  */ /g')"

NINJA=/run/media/odran/Leo/projects/Restored-projects/Chatgpt/MetroidPrimePort/build/review-tools/bin/ninja
EXPECT=6ef9b491d0cc08bc81a124fdedb8bfaec34d0010

if [ "$#" -eq 0 ]; then
  echo "usage: tools/flip_test.sh <unit> [unit...]   (configure.py paths, e.g. Kyoto/CFrameDelayedKiller.cpp)" >&2
  exit 2
fi
units=("$@")

check() {
  local unit="$1"
  # A Matching unit must have source, or configure.py refuses to regenerate.
  if [ ! -f "src/${unit%.cpp}.cpp" ]; then
    echo "    no source file (src/${unit%.cpp}.cpp) - cannot be Matching"
    return 1
  fi
  # Regenerate explicitly: without this, a stale build.ninja can hide the failure.
  python3 configure.py $CONFIGURE_ARGS >/dev/null 2>&1 || { echo "    configure.py failed"; return 1; }
  "$NINJA" >/dev/null 2>&1 || { echo "    build failed"; return 1; }
  local sha
  sha=$(sha1sum build/G2ME01/main.dol | cut -d' ' -f1)
  if [ "$sha" != "$EXPECT" ]; then echo "    DOL differs ($sha)"; return 1; fi
  for f in orig/G2ME01/files/RelProd/*.rel; do
    m=$(basename "$f")
    cmp -s "build/G2ME01/${m%.rel}/${m}" "$f" || { echo "    REL differs ($m)"; return 1; }
  done
  return 0
}

pass=()
for u in "${units[@]}"; do
  cp configure.py /tmp/opencode/cfg.before
  python3 - "$u" <<'PY'
import sys
u = sys.argv[1]
s = open('configure.py').read()
old, new = f'Object(NonMatching, "{u}")', f'Object(Matching, "{u}")'
if old not in s:
    print("    not listed as NonMatching"); sys.exit(3)
open('configure.py', 'w').write(s.replace(old, new))
PY
  if [ $? -ne 0 ]; then cp /tmp/opencode/cfg.before configure.py; echo "SKIP $u"; continue; fi
  echo "TEST $u"
  if check "$u"; then
    echo "  PASS  -> kept as Matching"
    pass+=("$u")
  else
    cp /tmp/opencode/cfg.before configure.py
    echo "  FAIL  -> reverted"
  fi
done

echo
echo "kept: ${#pass[@]} / ${#units[@]}"
for u in "${pass[@]:-}"; do echo "   $u"; done
