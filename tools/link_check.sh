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

# Is the build tree behind something it was generated from?
#
# This replaces an unconditional `rm -rf`, which was correct and ruinous. The comment above
# it said a stale build directory "reports the previous build's answer", which is true - and
# the cure was to delete 674 objects on *every* gate run, so the gate spent most of its time
# recompiling the tree in order to check it. Two full builds of the port, back to back, on
# every invocation, is how a two-minute gate became a fifty-minute one.
#
# Freshness is decidable without deleting anything: if `build.ninja` is older than
# CMakeLists.txt, files.cmake or CMakeCache.txt, the manifest predates its inputs and must be
# regenerated - and the RERUN_CMAKE edge already lists all three, so a stale *file list* is
# caught here too. Otherwise the tree is current by construction: ninja tracks every object
# against its own source and header, so an edited file rebuilds itself.
#
# Note the last part is the part that matters and the part `rm -rf` was defending. Ninja
# decides what is out of date from mtimes and depfiles, which is a stronger guarantee than
# "we deleted everything". A hard-to-notice exception is a compiler that fails *without*
# touching its output, leaving ninja thinking the object is current; that is a real hazard,
# but the answer to it is to notice the stale object, not to delete 674 good ones every time.
stale_manifest() {
  local manifest="$BUILD_DIR/build.ninja"
  [ -f "$manifest" ] || return 0
  local newest=0 t
  for f in "$REPO_ROOT/CMakeLists.txt" "$REPO_ROOT/files.cmake" "$BUILD_DIR/CMakeCache.txt"; do
    [ -f "$f" ] || continue
    t=$(stat -c %Y "$f" 2>/dev/null || echo 0)
    [ "$t" -gt "$newest" ] && newest=$t
  done
  [ "$(stat -c %Y "$manifest" 2>/dev/null || echo 0)" -lt "$newest" ]
}

if [ "$REBUILD" = 1 ] && stale_manifest; then
  # Only pay for the clean configure when the tree is genuinely behind. An explicit
  # --rebuild on a current tree is honoured as a full clean, because a human asking for
  # --rebuild is asking for a clean build.
  if [ -f "$BUILD_DIR/build.ninja" ]; then
    echo "link_check: build tree is behind its inputs; removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
  fi
fi

for tool in "$CMAKE" "$NINJA"; do
  [ -x "$tool" ] || { echo "link_check: $tool not found." >&2
    echo "  Set MP_TOOLCHAIN, or build the sibling port's review tools." >&2; exit 2; }
done

# A stale build directory is worse than none: it reports the previous build's
# answer. `--rebuild` forces a clean configure, which is also the only way to pick up
# a change to CMakeLists.txt. See `stale_manifest` above for why the clean is now
# conditional rather than unconditional: deleting 674 objects on every run cost more
# than the problem it solved, and ninja's own mtime and depfile tracking is the real
# guarantee that the tree is current.

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

# **A duplicate count is only a measurement if the linker actually ran.** The three numbers
# above are scraped from the log, so on a run that stops at compile time - or that fails
# earlier in the link, before the duplicate pass - they scrape to 0 and read as a clean
# result. That is how `CErrorOutputWindow::CErrorOutputWindow(bool)` reached the shipping
# link as a `multiple definition`: this script printed `duplicate definitions 0` on the
# same run that failed. The same shape as a tautological check, and `PROCESS_LESSONS.md`
# has it. So the summary now carries whether the link ran, and a count with no link behind
# it is a failure rather than a zero.
link_ran = any(m in text for m in
               ('undefined reference to', 'multiple definition of',
                'ld returned', 'collect2: error', 'undefined symbol:'))
if not link_ran:
    print("link_check: the LINKER NEVER RAN - the counts above are vacuous, not zero")
with open(os.path.join(os.path.dirname(log), 'link_summary.txt'), 'w') as fh:
    fh.write(f"{len(undef)} {len(dups)} {len(compile_errors)} {int(link_ran)}\n")

# Exit non-zero on a duplicate definition as well as on a compile error. A duplicate is a
# real defect that the gate must fail on; reporting it and exiting 0 let it through once.
sys.exit(0 if (not compile_errors and not dups and link_ran) else 1)
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
