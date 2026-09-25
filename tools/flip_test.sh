#!/bin/bash
# Authoritative completion test for a unit.
#
#   tools/flip_test.sh <unit> [unit...]      e.g. tools/flip_test.sh Kyoto/CFrameDelayedKiller.cpp
#
# `ninja` links the DOL from build/G2ME01/obj/*.o, the objects dtk split out of retail; marking a
# unit Matching in configure.py swaps in our own object. So a unit is genuinely complete when the
# build still reproduces retail with our object in the link. This flips each unit, rebuilds, checks
# the DOL sha1 and all 86 RELs, and keeps the flips that hold while reverting the ones that do not.
#
# Two ways this test can pass *vacuously*, both now refused rather than reported as PASS:
#   * the unit is in no splits.txt, so nothing claims its range and our object is never linked;
#   * the unit is Matching with no source file, which configure.py accepts - it prints
#     "Missing source file" and links the retail object instead (project.py, not an error).
# It also exits non-zero if anything FAILed or was SKIPped, so a caller cannot read success off a
# partial run; the per-unit lines carry the detail and the last line is a verdict.
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

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

# Where the unit's object is built, and where its source is: `.cp` and `.c` sources exist, and an
# entry may point elsewhere with source="...". Mirrors tools/check_module_wiring.py.
unit_info() {
  python3 - "$1" <<'PY'
import re, sys
u = sys.argv[1]
s = open('configure.py').read()
entry = re.compile(r'Object\(\s*(Matching|NonMatching|MatchingFor)\s*(?:\([^)]*\))?\s*,\s*"'
                   + re.escape(u) + r'"(?P<rest>[^)]*)\)?', re.S)
m = entry.search(s)
if not m:
    print("absent"); raise SystemExit
src = re.search(r'source\s*=\s*"([^"]+)"', m.group('rest') or '')
path = src.group(1) if src else u
print(m.group(1))
print('src/' + path)
PY
}

claimed_in_splits() {
  local unit="$1"
  grep -qs "^${unit}:\$" config/G2ME01/splits.txt config/G2ME01/rels/*/splits.txt
}

check() {
  local unit="$1" state="$2"
  # (a) our own object has to exist, or configure.py silently links retail's.
  local src_file
  src_file="$(unit_info "$unit" | tail -n1)"
  if [ ! -f "$src_file" ]; then
    echo "    no source file ($src_file) - configure.py would link the retail object and this would"
    echo "    pass while proving nothing (it only prints 'Missing source file')"
    return 1
  fi
  # (b) the range has to be claimed by some unit, or our object is never linked at all.
  if ! claimed_in_splits "$unit"; then
    echo "    $unit is in no splits.txt - nothing claims its range, so it cannot be linked and a"
    echo "    flip here proves nothing (works today on CScriptIngSwarm.cpp: reports PASS in 0.6s)"
    return 1
  fi
  # Regenerate explicitly: without this, a stale build.ninja can hide the failure.
  if ! python3 configure.py $CONFIGURE_ARGS > build/flip-configure.log 2>&1; then
    echo "    configure.py failed (args: $CONFIGURE_ARGS)"; tail -n 5 build/flip-configure.log | sed 's/^/      /'
    return 1
  fi
  if grep -q "^Missing source file ${src_file#src/}" build/flip-configure.log; then
    echo "    configure.py is linking the retail object for this unit (Missing source file)"
    return 1
  fi
  if ! "$NINJA" > build/flip-ninja.log 2>&1; then
    echo "    build failed:"
    grep -E 'FAILED|undefined|Error|error:' build/flip-ninja.log | head -n 10 | sed 's/^/      /'
    return 1
  fi
  # Only now is the on-disk binary this build's output rather than the previous one.
  local sha
  sha=$(sha1sum build/G2ME01/main.dol | cut -d' ' -f1)
  if [ "$sha" != "$EXPECT" ]; then echo "    DOL differs ($sha)"; return 1; fi
  local f m
  for f in orig/G2ME01/files/RelProd/*.rel; do
    m=$(basename "$f")
    cmp -s "build/G2ME01/${m%.rel}/${m}" "$f" || { echo "    REL differs ($m)"; return 1; }
  done
  return 0
}

pass=(); fail=(); skip=()
for u in "${units[@]}"; do
  info="$(unit_info "$u")"
  state="$(echo "$info" | head -n1)"
  backup="$(mktemp)"          # never a shared path: two lanes flipping at once would fight over it
  cp configure.py "$backup"
  case "$state" in
    absent) echo "SKIP $u - not listed in configure.py"; skip+=("$u"); continue ;;
    matching) echo "TEST $u (already Matching - verifying in place)";;
    *)        echo "TEST $u"
              python3 - "$u" <<'PY'
import sys
u = sys.argv[1]
s = open('configure.py').read()
old, new = f'Object(NonMatching, "{u}"', f'Object(Matching, "{u}"'
open('configure.py', 'w').write(s.replace(old, new, 1))
PY
              ;;
  esac
  if check "$u" "$state"; then
    case "$state" in
      matching) echo "  PASS  -> already Matching and reproducing retail" ;;
      *)        echo "  PASS  -> kept as Matching" ;;
    esac
    pass+=("$u")
  else
    cp "$backup" configure.py
    if [ "$state" = matching ]; then
      echo "  FAIL  -> ALREADY MATCHING AND NOT REPRODUCING RETAIL (config left as it is, fix it)"
    elif python3 configure.py $CONFIGURE_ARGS >/dev/null 2>&1 && "$NINJA" >/dev/null 2>&1; then
      echo "  FAIL  -> reverted (tree rebuilt: DOL $(sha1sum build/G2ME01/main.dol | cut -d' ' -f1))"
    else
      echo "  FAIL  -> reverted, but the REBUILD FAILED - do not trust build/ until it is green again"
    fi
    fail+=("$u")
  fi
  rm -f "$backup"
done

echo
echo "kept: ${#pass[@]} / ${#units[@]}   failed: ${#fail[@]}   skipped: ${#skip[@]}"
for u in "${pass[@]:-}"; do [ -n "$u" ] && echo "   PASS $u"; done
for u in "${fail[@]:-}"; do [ -n "$u" ] && echo "   FAIL $u"; done
for u in "${skip[@]:-}"; do [ -n "$u" ] && echo "   SKIP $u"; done
[ "${#fail[@]}" -eq 0 ] && [ "${#skip[@]}" -eq 0 ]
