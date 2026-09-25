#!/bin/bash
# The port's real link: configure, build the executable, and report what the linker
# actually asks for.
#
#   tools/link_check.sh [--rebuild] [--record]
#
# Why this exists. `tools/link_gap.py` derives the port's link gap from `nm` set
# arithmetic over mp_game/mp_platform/mp_port_entry. It is convenient and it is
# close, but it cannot see two whole classes of thing, and a single `ld.bfd` run
# over the real executable found three bugs no `nm` arithmetic could have:
#
#   - `_ctors`/`_dtors` in src/REL/REL_Setup.cpp, which the GameCube's linker
#     synthesises and an ELF link does not.
#   - platform/ai_dma.cpp, fully written and compiled by nothing, so all five AI
#     DMA entry points were missing from the port.
#   - fourteen translation units defining one `RELMain`, which is the module
#     system working correctly and cannot coexist in a flat link.
#
# It also has a structural blind spot: a vtable is only *emitted* by the
# translation unit defining a class's key function, so `vtable for CPlayer` and
# `typeinfo for CGunWeapon` are invisible to `nm` until that key function is
# written. The linker asks for them; link_gap.py cannot count them.
#
# So the two instruments measure different things and this is the ground truth.
# A mismatch of a few symbols between this and link_gap.py is expected and is not
# a failure; a rise in the undefined count, or ANY duplicate definition, is.
#
# This is the slow gate. The rest of tools/gate.sh measures retail and finishes in
# minutes; this one configures Aurora, fetches its SDL3 and Dawn, and builds 118
# game units plus the SDK. Run it before committing anything that touches
# CMakeLists.txt, files.cmake, platform/, or the port side of a unit - the changes
# that can move it are exactly the ones the fast gates cannot see.
#
# There is no system cmake or ninja. Both come from the sibling Metroid Prime port,
# which is also where Aurora's dependencies are cached.

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT" || exit 1

TOOLCHAIN="${MP_TOOLCHAIN:-$REPO_ROOT/../MetroidPrimePort/build/review-tools}"
CMAKE="$TOOLCHAIN/bin/cmake"
NINJA="$TOOLCHAIN/bin/ninja"
BUILD_DIR="${MP_LINK_BUILD:-$REPO_ROOT/build-port-link}"
BASELINE="$REPO_ROOT/docs/research/port_link_baseline.txt"

REBUILD=0
RECORD=0
for arg in "$@"; do
  case "$arg" in
    --rebuild) REBUILD=1 ;;
    --record)  RECORD=1 ;;
    -h|--help) sed -n '2,40p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) echo "link_check: unknown argument $arg" >&2; exit 2 ;;
  esac
done

for tool in "$CMAKE" "$NINJA"; do
  [ -x "$tool" ] || { echo "link_check: $tool not found." >&2
    echo "  Set MP_TOOLCHAIN, or build the sibling port's review tools." >&2; exit 2; }
done

# A stale build directory is worse than none: it reports the previous build's
# answer. --rebuild forces a clean configure, which is also the only way to pick up
# a change to CMakeLists.txt.
if [ "$REBUILD" = 1 ]; then
  echo "link_check: removing $BUILD_DIR"
  rm -rf "$BUILD_DIR"
fi

# The log lives *inside* the build directory, which is gitignored. Writing it
# beside it as "$BUILD_DIR.configure.log" would drop an untracked file in the repo
# root on every fresh run.
mkdir -p "$BUILD_DIR"
CONFIG_LOG="$BUILD_DIR/configure.log"
if ! "$CMAKE" -S . -B "$BUILD_DIR" -G Ninja \
      -DCMAKE_MAKE_PROGRAM="$NINJA" \
      -DMP_SDK_HEADERS_ONLY=OFF > "$CONFIG_LOG" 2>&1; then
  echo "link_check: configure FAILED. Tail of the log:" >&2
  tail -25 "$CONFIG_LOG" >&2; exit 2
fi

LOG="$BUILD_DIR/build.log"
"$CMAKE" --build "$BUILD_DIR" --target metroid_prime2_port > "$LOG" 2>&1
build_status=$?

python3 - "$LOG" "$build_status" <<'PY'
import re, sys, os
log, status = sys.argv[1], int(sys.argv[2])
text = open(log, errors='replace').read() if os.path.exists(log) else ''

# The linker prints demangled names with a parameter list, so an exact-match test
# against a base name is wrong; take the set of whole names.
undef = set(re.findall(r"undefined reference to `([^']*)'", text))
dups = re.findall(r"multiple definition of `([^']*)'", text)
# A compile error is a different failure from a link failure and must not be
# counted as "the link asked for fewer things".
compile_errors = [l for l in text.splitlines()
                  if 'error:' in l and 'ld returned' not in l]

print(f"link_check: compile errors {len(compile_errors)}")
for l in compile_errors[:10]:
    print(f"  {l[:160]}")
print(f"link_check: unique undefined symbols {len(undef)}")
print(f"link_check: duplicate definitions   {len(dups)}")
for d in sorted(set(dups))[:10]:
    print(f"  DUP {d}")
linked = status == 0 and not undef and not dups and not compile_errors
print(f"link_check: {'LINKED' if linked else 'NOT LINKED'}")
with open(os.path.join(os.path.dirname(log), 'link_summary.txt'), 'w') as fh:
    fh.write(f"{len(undef)} {len(dups)} {len(compile_errors)}\n")
sys.exit(0 if not compile_errors else 1)
PY
parse_status=$?

summary="$BUILD_DIR/link_summary.txt"
[ -f "$summary" ] || { echo "link_check: no summary produced" >&2; exit 2; }
read -r undef_count dup_count compile_count < "$summary"

if [ "$RECORD" = 1 ]; then
  {
    echo "# Measured by tools/link_check.sh on the port's real executable."
    echo "# The linker is the ground truth; tools/link_gap.py's count differs by a"
    echo "# few because a vtable is invisible to nm until its key function exists."
    echo "undefined $undef_count"
    echo "duplicates $dup_count"
  } > "$BASELINE"
  echo "link_check: recorded baseline to $BASELINE"
  exit 0
fi

if [ ! -f "$BASELINE" ]; then
  echo "link_check: no baseline at $BASELINE; run with --record to create one" >&2
  exit 3
fi
base_undef=$(awk '$1=="undefined"{print $2}' "$BASELINE")
base_dup=$(awk '$1=="duplicates"{print $2}' "$BASELINE")

status=0
if [ "$dup_count" -gt "$base_dup" ]; then
  echo "link_check: FAIL duplicates went $base_dup -> $dup_count" >&2
  status=1
fi
if [ "$undef_count" -gt "$base_undef" ]; then
  echo "link_check: FAIL undefined went $base_undef -> $undef_count" >&2
  status=1
fi
if [ "$dup_count" -eq "$base_dup" ] && [ "$undef_count" -eq "$base_undef" ]; then
  echo "link_check: unchanged from baseline ($undef_count undefined, $dup_count duplicates)"
fi
exit $(( status != 0 ? 1 : parse_status ))
