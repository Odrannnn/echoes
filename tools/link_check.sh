#!/bin/bash
# The port's real link: configure, build the executable, and report what the linker
# actually asks for.
#
#   tools/link_check.sh [--rebuild] [--record] [--strict]
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
#
# --strict is the pass/fail link gate, and it is what `tools/probe_sources.sh` calls. The
# default verdict is a *gap* measurement: "341 undefined is no worse than the recorded
# 341" is a true statement about a port that cannot be linked at all, and it exits 0. That
# is correct for a trend and useless as a gate, so --strict asks the only question a gate
# can ask: did the link resolve? It names the symbols, and it fails on any that are
# outstanding. Two modes, because a number that only goes down is a proxy for a thing that
# can also go up, and a gate built on the proxy alone is the trap in PROCESS_LESSONS.md 22.
#
# Only one build may run in one build tree: two ninjas in the same tree corrupt it. So the
# build is single-flight under a lock. A second caller - the gate's background run and the
# probe's own - waits for the first and then re-derives its verdict from the log that run
# left, which is also what keeps the gate from paying for the port build twice.

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT" || exit 1

TOOLCHAIN="${MP_TOOLCHAIN:-$REPO_ROOT/../MetroidPrimePort/build/review-tools}"
CMAKE="$TOOLCHAIN/bin/cmake"
NINJA="$TOOLCHAIN/bin/ninja"
BUILD_DIR="${MP_LINK_BUILD:-$REPO_ROOT/build-port-link}"
BASELINE="$REPO_ROOT/docs/research/port_link_baseline.txt"
export MP_LINK_BASELINE="$BASELINE"

REBUILD=0
RECORD=0
STRICT=0
for arg in "$@"; do
  case "$arg" in
    --rebuild) REBUILD=1 ;;
    --record)  RECORD=1 ;;
    --strict)  STRICT=1 ;;
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

# The logs live *inside* the build directory, which is gitignored. Writing them
# beside it as "$BUILD_DIR.configure.log" would drop an untracked file in the repo
# root on every fresh run.
mkdir -p "$BUILD_DIR"
CONFIG_LOG="$BUILD_DIR/configure.log"
LOG="$BUILD_DIR/build.log"
TMP_LOG="$BUILD_DIR/build.log.new"
SUMMARY="$BUILD_DIR/link_summary.txt"
UNDEF_LIST="$BUILD_DIR/link_undefined.txt"
DUP_LIST="$BUILD_DIR/link_duplicate.txt"

# --- single flight -----------------------------------------------------------
#
# `tools/gate.sh` starts this script in the background at the top of the run so its build
# overlaps the decomp build, and `tools/probe_sources.sh` then asks for the same link as
# its own verdict. Two `ninja` invocations in one build tree corrupt that tree, and the
# corruption shows up as a link result nobody asked for. So the build runs under a lock
# and a second caller *waits and re-derives its verdict from the log the first one left*
# rather than building again - which is also what stops the gate paying for the port build
# twice.
#
# Freshness of a waited-on result is guaranteed by ordering, not by a timestamp: the holder
# writes `build.log` and the summary from the same run, and writes them before it releases
# the lock, so a waiter that sees the lock free and a log present is reading a link that ran
# after it arrived.
#
# A lock left behind by a killed run would hang the gate forever, which is worse than the
# race it prevents, so the holder's PID goes in the lock and a lock whose owner is gone -
# and which has been that way for a minute, so a PID cannot be recycled into a false
# "still running" - is broken.
#
# The lock lives in build/, NOT in the build directory: `--rebuild` deletes the build
# directory, and a lock that deletes itself with it is a lock that lets the next caller in
# half way through. `build/` is gitignored and is what every other gate log lives in.
LOCK="${MP_PORT_LINK_LOCK:-$REPO_ROOT/build/port-link.lock}"
mkdir -p "$(dirname "$LOCK")"
LOCK_HELD=""
LOCK_WAITED=0
release_lock() { [ -n "$LOCK_HELD" ] && rm -rf "$LOCK"; }
acquire_lock() {
  local owner age now
  while :; do
    if mkdir "$LOCK" 2>/dev/null; then
      echo $$ > "$LOCK/pid"
      LOCK_HELD=1
      return 0
    fi
    owner=$(cat "$LOCK/pid" 2>/dev/null) || owner=""
    now=$(date +%s)
    age=$(( now - $(stat -c %Y "$LOCK" 2>/dev/null || echo "$now") ))
    if [ -z "$owner" ] || [ ! -d "/proc/$owner" ]; then
      if [ "$age" -ge 60 ]; then
        echo "link_check: breaking an abandoned build lock (owner pid ${owner:-unknown} is gone)"
        rm -rf "$LOCK"
        continue
      fi
    fi
    if [ "$LOCK_WAITED" = 0 ]; then
      echo "link_check: another link_check is already building $BUILD_DIR; waiting for its result"
      LOCK_WAITED=1
    fi
    sleep 2
  done
}

# A caller that had to wait does NOT build: the holder it waited for just built this same
# tree, and a second `ninja` here is the corruption this lock exists to prevent. It still
# runs the parser below, on the holder's log, so the verdict is re-derived from evidence
# rather than read out of a summary file some earlier run wrote.
acquire_lock
trap release_lock EXIT

if [ "$LOCK_WAITED" = 0 ]; then
  if [ "$REBUILD" = 1 ] && stale_manifest; then
    # Only pay for the clean configure when the tree is genuinely behind. An explicit
    # --rebuild on a current tree is honoured as a full clean, because a human asking for
    # --rebuild is asking for a clean build. Inside the lock, and after it, so the clean
    # cannot be racing another run's build.
    if [ -f "$BUILD_DIR/build.ninja" ]; then
      echo "link_check: build tree is behind its inputs; removing $BUILD_DIR"
      rm -rf "$BUILD_DIR"
      mkdir -p "$BUILD_DIR"
    fi
  fi
  if ! "$CMAKE" -S . -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_MAKE_PROGRAM="$NINJA" \
        -DMP_SDK_HEADERS_ONLY=OFF > "$CONFIG_LOG" 2>&1; then
    echo "link_check: configure FAILED. Tail of the log:" >&2
    tail -25 "$CONFIG_LOG" >&2; exit 2
  fi
  "$CMAKE" --build "$BUILD_DIR" --target metroid_prime2_port > "$TMP_LOG" 2>&1
  build_status=$?

  # **`ninja: no work to do` must not be allowed to erase the answer.** The parser below
  # scrapes `build.log`, and a log rewritten to that one line contains no linker
  # diagnostic at all - so a fully up-to-date tree would scrape to "0 undefined, the
  # LINKER NEVER RAN". That is the vacuous-zero failure this script already guards against
  # in one place, arriving through a second door. So the previous log is *kept* when ninja
  # had nothing to do, and the artefacts are re-derived from it. If there is no previous
  # log then the tree has never linked and saying so is the only honest answer.
  if grep -q '^ninja: no work to do\.$' "$TMP_LOG" 2>/dev/null; then
    rm -f "$TMP_LOG"
    if [ ! -s "$LOG" ]; then
      echo "link_check: ninja has nothing to do and $LOG holds no link result." >&2
      echo "  Remove $BUILD_DIR and re-run: an up-to-date build tree with no log is not evidence." >&2
      exit 2
    fi
    retained=1
    build_status=0
    echo "link_check: nothing to relink; re-reading the existing link result in $LOG"
  else
    mv -f "$TMP_LOG" "$LOG"
    rm -f "$SUMMARY" "$UNDEF_LIST" "$DUP_LIST"
  fi
fi

# The parser runs in both paths - it is the verdict, and a verdict read out of a summary
# file is a claim about a file rather than a measurement of the log. A waiter therefore
# re-derives the same numbers from the log the holder just produced.
build_status=${build_status:-0}
# 1 when `build.log` is a *retained* log from an earlier run because ninja had nothing to
# do. The parser must not take this run's exit status as evidence about that log.
retained=${retained:-0}
python3 - "$LOG" "$build_status" "$STRICT" "$retained" <<'PY'
import re, sys, os
log, status, strict, retained = (sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]))
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
#
# **And "the linker ran" is asked of the log, not of the exit status alone**, because the
# failure this test exists to catch now also arrives from the other side. A *successful* link
# prints nothing at all: no `undefined reference`, no `multiple definition`, no `ld
# returned`. So a heuristic built only from failure diagnostics calls a perfectly good link
# "never ran" and turns the best news in the project into a gate failure - the same
# vacuous-zero rule, misfiring in the direction that makes a step unusable and therefore
# ignored. The evidence that *is* in a good run's log is ninja's own edge line,
# `[N/M] Linking CXX executable metroid_prime2_port`, so that is what is looked for; a clean
# build's exit status is accepted only for a log this run produced, because a retained
# log's status belongs to the no-op that kept it, not to the link it records.
link_edge = re.search(r'^\[\d+/\d+\] Linking .*\bmetroid_prime2_port\b', text, re.M) is not None
link_ran = (link_edge
            or any(m in text for m in ('undefined reference to', 'multiple definition of',
                                       'ld returned', 'collect2: error', 'undefined symbol:'))
            or (status == 0 and not retained))
if not link_ran:
    print("link_check: the LINKER NEVER RAN - the counts above are vacuous, not zero")
with open(os.path.join(os.path.dirname(log), 'link_summary.txt'), 'w') as fh:
    fh.write(f"{len(undef)} {len(dups)} {len(compile_errors)} {int(link_ran)}\n")

# **A count nobody can act on is half a measurement.** The three numbers are real, but a
# lane cannot close "341 undefined" - it can close the 341 named symbols. So the names go
# to their own files, next to the summary, written whether or not anyone asked: the strict
# verdict names them, and so can any other instrument that reads the build directory. A
# sorted file, so two runs of the same tree produce the same bytes and a diff is meaningful.
here = os.path.dirname(log)
with open(os.path.join(here, 'link_undefined.txt'), 'w') as fh:
    fh.write(''.join(f'{n}\n' for n in sorted(undef)))
with open(os.path.join(here, 'link_duplicate.txt'), 'w') as fh:
    fh.write(''.join(f'{n}\n' for n in sorted(set(dups))))

if strict:
    # **This is a REGRESSION gate, not an absolute one, and the distinction is the whole design.**
    #
    # The obvious question - "did the link resolve?" - is the wrong question here, and asking it
    # makes the gate permanently red: the decompilation is ~14% done, so the whole-game link has
    # hundreds of undefined symbols and will have for a long time. A gate that cannot go green
    # stops being read, and a gate nobody reads is worse than the hole it replaced.
    #
    # What must never happen is the defect this step was added to catch: a change that *raises* the
    # undefined count, or introduces a duplicate, and reports success. Landing the `CCubeRenderer`
    # key function took the port from 312 undefined to 391 and every gate said `ok`, because the
    # probe compiled the sources and never linked them. So the gate asks the two questions that can
    # actually be answered today:
    #
    #   1. did the undefined count GROW against the recorded baseline?
    #   2. is there any duplicate definition?            (always 0, always must be)
    #
    # The absolute count is still printed, still written to `link_undefined.txt`, and still shown in
    # the summary - it just is not the pass condition.
    base_undef, base_names = 0, set()
    try:
        for ln in open(os.environ.get('MP_LINK_BASELINE', ''), encoding='utf-8'):
            ln = ln.strip()
            m = re.match(r'^undefined (\d+)$', ln)
            if m:
                base_undef = int(m.group(1))
            elif ln.startswith('sym '):
                base_names.add(ln[4:])
    except OSError:
        pass
    grew = len(undef) > base_undef
    regressed = grew or dups or compile_errors or not link_ran
    print(f"link_check: STRICT {'PASS' if not regressed else 'FAIL'} - regression gate: "
          f"{len(undef)} undefined against a baseline of {base_undef} "
          f"({'GREW' if grew else 'no growth'}), {len(dups)} duplicate(s), "
          f"{len(compile_errors)} compile error(s), linker_ran={int(link_ran)}")
    if regressed and undef:
        if grew:
            new = sorted(set(undef) - set(base_names))
            print(f"link_check: {len(new)} symbol(s) this change ADDED to the gap:")
            for n in new[:40]:
                print(f"  NEW  {n}")
            if len(new) > 40:
                print(f"  ... and {len(new) - 40} more, listed in "
                      f"{os.path.join(here, 'link_undefined.txt')}")
        else:
            print(f"link_check: {len(undef)} undefined symbol(s):")
            for n in sorted(undef)[:40]:
                print(f"  UNDEF {n}")
    sys.exit(0 if not regressed else 1)

# Exit non-zero on a duplicate definition as well as on a compile error. A duplicate is a
# real defect that the gate must fail on; reporting it and exiting 0 let it through once.
sys.exit(0 if (not compile_errors and not dups and link_ran) else 1)
PY
parse_status=$?
release_lock; LOCK_HELD=

[ -f "$SUMMARY" ] || { echo "link_check: no summary produced" >&2; exit 2; }
# Four fields. Reading three of them would put the fourth inside the third, so say four.
read -r undef_count dup_count compile_count link_ran_flag < "$SUMMARY"

if [ "$RECORD" = 1 ]; then
  {
    echo "# Measured by tools/link_check.sh on the port's real executable."
    echo "# The linker is the ground truth; tools/link_gap.py's count differs by a"
    echo "# few because a vtable is invisible to nm until its key function exists."
    echo "undefined $undef_count"
    echo "duplicates $dup_count"
    # The names, not just the count. `--strict` compares the two lists, so when a change grows the
    # gap it can say WHICH symbols it added rather than only that it added some - which is the
    # difference between a number you have to go and investigate and a diagnosis. Landing the
    # `CCubeRenderer` key function took the port 312 -> 391 and no gate mentioned it; with this,
    # the gate names all 79.
    echo "# sym <name> - one per undefined symbol, for the --strict regression diff."
    sed "s/^/sym /" "$(dirname "$SUMMARY")/link_undefined.txt"
  } > "$BASELINE"
  echo "link_check: recorded baseline to $BASELINE"
  exit 0
fi

# --strict: the pass/fail gate, answered from the same artefacts, with no baseline. It is
# checked *before* the baseline file, because a tree that cannot link must fail even if
# someone has deleted the baseline - and a gate whose verdict depends on a file that may
# be missing is a gate that can pass by having nothing to compare against.
if [ "$STRICT" = 1 ]; then
  # The python block owns the verdict, including the baseline comparison and the naming of
  # new symbols. Deriving it a SECOND time here is how the two disagreed: the python said
  # "STRICT PASS" against a 322 baseline while this block still applied the original
  # absolute policy (`undef_count = 0`) and exited 1 - so the summary line said one thing and
  # the gate read the other. **One verdict, computed once.**
  #
  # The duplication was not paranoia about a parser: both numbers came from one scrape of one
  # log, so they could not disagree about the facts - only about the POLICY, which is defined
  # in exactly one place now.
  exit "${parse_status:-1}"
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
