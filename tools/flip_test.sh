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
# `configure_args` is written as a ninja variable whose value spans several
# lines, each continuation ending in ` $`, so the whole block has to be joined
# before the line-continuation markers are dropped. (Reading only the first
# line silently produced "--version G2ME01 --compilers" and made every flip
# report "configure.py failed".)
CONFIGURE_ARGS="$(python3 - <<'PY'
import re
text = open('build.ninja').read()
m = re.search(r'^configure_args = (.*(?:\n[ \t]+.*)*)', text, re.M)
print(' '.join(m.group(1).replace('$\n', ' ').replace('$', ' ').split()) if m else '')
PY
)"

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
  python3 configure.py $CONFIGURE_ARGS >/dev/null 2>&1 || { echo "    configure.py failed (args: $CONFIGURE_ARGS)"; return 1; }
  local ninja_log
  ninja_log="$(mktemp)"
  if ! "$NINJA" >"$ninja_log" 2>&1; then
    echo "    build failed:"
    tail -n 15 "$ninja_log" | sed 's/^/      /'
    rm -f "$ninja_log"
    return 1
  fi
  rm -f "$ninja_log"
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
  listed="$(python3 - "$u" <<'PY'
import sys
u = sys.argv[1]
s = open('configure.py').read()
if f'Object(Matching, "{u}")' in s: print("matching")
elif f'Object(NonMatching, "{u}")' in s: print("nonmatching")
else: print("absent")
PY
)"
  case "$listed" in
    absent) echo "SKIP $u - not listed in configure.py"; continue ;;
    matching)
      # Already Matching: there is nothing to flip, but the claim still has to hold.
      echo "TEST $u (already Matching - verifying in place)"
      if check "$u"; then echo "  PASS  -> already Matching and reproducing retail"; pass+=("$u")
      else echo "  FAIL  -> ALREADY MATCHING AND NOT REPRODUCING RETAIL (config left as it is, fix it)"; fi
      continue ;;
  esac
  python3 - "$u" <<'PY'
import sys
u = sys.argv[1]
s = open('configure.py').read()
old, new = f'Object(NonMatching, "{u}")', f'Object(Matching, "{u}")'
open('configure.py', 'w').write(s.replace(old, new))
PY
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
