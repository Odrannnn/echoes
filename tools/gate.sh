#!/bin/bash
# The whole acceptance test, as one command that cannot be run partially.
#
#   tools/gate.sh [BASELINE_REPORT]      default baseline: build/report.base.json
#   tools/gate.sh --baseline             record the baseline from a clean tree (run on HEAD first)
#
# Exit 0 only if every check passed. Prints one verdict line last.
set -uo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"
TC="${MP_TOOLCHAIN_DIR:-$REPO_ROOT/../MetroidPrimePort}"
NINJA="$TC/build/review-tools/bin/ninja"
fail=()
step() { printf '%-28s' "$1"; }

# 0a. Refuse to start on a filesystem that cannot hold the build.
#
# A full disk does not fail a build cleanly. Two gate runs on 2026-09-26 died on
# `Disk quota exceeded`, and one produced a *truncated* `rules.ninja` (cut mid-line at
# 12288 bytes) which ninja reported as a lexing error and turned into a 36-symbol phantom
# failure. A truncated build input is the worst kind of failure: the tool reports a confident,
# wrong answer about the code.
#
# So this is a hard stop with a number, not a warning. It is cheap (one stat) and it converts
# an unrecognisable failure into an obvious one.
#
# `TMPDIR` matters as much as the repo filesystem: the compiler and cmake both spill there, and
# /tmp is a 31 GB tmpfs shared with every lane worktree. The default of 4 GB free is well under
# what a full port build wants, so a healthy tree passes this with room to spare.
need_kb=4194304
for target in "$REPO_ROOT" "${TMPDIR:-/tmp}"; do
  avail_kb=$(df -Pk "$target" 2>/dev/null | awk 'NR==2 {print $4}')
  if [ -z "$avail_kb" ]; then
    continue
  elif [ "$avail_kb" -lt "$need_kb" ]; then
    echo "GATE REFUSED: only $((avail_kb / 1024)) MB free on $target, need $((need_kb / 1024)) MB."
    echo "  A full disk truncates build inputs, and a truncated rules.ninja reads as a code error."
    echo "  Free space, or point TMPDIR at a larger filesystem, and re-run."
    exit 2
  fi
done

# 0b. Start the port build NOW, in the background, and let it run alongside everything below.
#
# The gate used to build the port twice, serially, near the end: `link_gap.py --rebuild` and
# then `link_check.sh --rebuild`, each into its own tree, the second deleting its own tree
# first. That is 674 host compiles, twice, with nothing else happening. It is now once, and it
# starts here so it overlaps the decomp build and objdiff instead of queueing behind them.
#
# Both later steps consume the result:
#   - `port link gap` runs link_gap.py, whose own ninja call is then a no-op ("no work to do"),
#     because link_check.sh has already built the same targets in the same tree. That is what
#     sharing one tree buys; see the note on link_gap.py's --build default.
#   - `port link dups` reads the log this produces instead of invoking link_check.sh again.
#
# `wait` is what makes this safe rather than a race: two ninjas in one build tree is a corrupt
# build tree, so the later steps block on this PID instead of starting a competing one. If the
# background job fails, its log is still there and the dups step reports it - a gate step that
# quietly skips its own build would be exactly the kind of step that cannot fail.
MP_TOOLCHAIN="$TC/build/review-tools" ./tools/link_check.sh >build/gate-linkcheck.log 2>&1 &
LINK_PID=$!

# 1. Configure with explicit arguments - never parsed back out of build.ninja - and into build/,
#    because config/G2ME01/build.sha1 names build/G2ME01/... literally.
step configure
python3 configure.py --version G2ME01 --compilers "$TC/build/compilers" --dtk "$TC/build/tools/dtk" \
  --wrapper "$TC/build/tools/wibo" --build-dir build >build/gate-configure.log 2>&1 \
  || fail+=(configure)
# project.py only *prints* this and then links the retail object: a vacuous Matching unit.
if grep -q '^Missing source file' build/gate-configure.log; then
  fail+=("matching-without-source"); grep '^Missing source file' build/gate-configure.log | sed 's/^/    /'
fi
echo "${fail[*]:-ok}"

# 2. Build. The CHECK edge runs `dtk shasum -c config/G2ME01/build.sha1` (DOL + 86 RELs), so ninja's
#    exit status IS the hash gate. Never read main.dol after a failed ninja: it is the previous one.
step "ninja + build.sha1"
if "$NINJA" >build/gate-ninja.log 2>&1; then echo ok
else fail+=(ninja); echo FAIL; grep -E 'FAILED|undefined|error|DIFF|MISMATCH' build/gate-ninja.log | head -10 | sed 's/^/    /'; fi

# 3. Independent re-hash against config.yml, only meaningful after a green ninja.
step "hashes vs config.yml"
if [[ " ${fail[*]:-} " != *" ninja "* ]]; then
  python3 - <<'PY' || fail+=(hashes)
import re, hashlib, os, sys
cfg = open('config/G2ME01/config.yml').read()
bad = [m for n, h in re.findall(r'object: files/RelProd/(\S+)\n\s+hash: ([0-9a-f]{40})', cfg)
       for m in [n[:-4]]
       if hashlib.sha1(open(f'build/G2ME01/{m}/{m}.rel', 'rb').read()).hexdigest() != h]
dol = hashlib.sha1(open('build/G2ME01/main.dol', 'rb').read()).hexdigest()
bad += [] if dol == '6ef9b491d0cc08bc81a124fdedb8bfaec34d0010' else ['main.dol']
print('ok' if not bad else 'FAIL ' + ' '.join(bad)); sys.exit(1 if bad else 0)
PY
else echo skipped; fi

# 4. Report, and the per-function diff against the baseline.
step report
./build/tools/objdiff-cli report generate -o build/report.json >/dev/null 2>&1 && echo ok || fail+=(report)
if [ "${1:-}" = "--baseline" ]; then
  [ -z "$(git status --porcelain --untracked-files=no)" ] || { echo "refusing: tree is dirty"; exit 2; }
  cp build/report.json build/report.base.json; echo "baseline recorded at $(git rev-parse --short HEAD)"
  exit 0
fi
BASE="${1:-build/report.base.json}"
step "per-function diff"
if [ -f "$BASE" ]; then python3 tools/report_diff.py "$BASE" build/report.json >build/gate-diff.log || fail+=(regression)
  head -1 build/gate-diff.log; grep -E 'WORSE|GONE|UNLINKED|FELL' build/gate-diff.log | head -20 | sed 's/^/  /'
else fail+=(no-baseline); echo "no baseline at $BASE - run tools/gate.sh --baseline on HEAD first"; fi

# 5. Structural checks.
step "module wiring";  python3 tools/check_module_wiring.py >build/gate-wiring.log 2>&1 && echo ok || { fail+=(wiring); grep -A5 -E 'UNWIRED|BROKEN' build/gate-wiring.log | head; }
step "dol_read";     python3 tools/test_dol_read.py >build/gate-dolread.log 2>&1 && echo ok || { fail+=(dol_read); head -5 build/gate-dolread.log; }
step "docs claims";    python3 tools/check_docs_claims.py >build/gate-docs.log 2>&1 && echo ok || { fail+=(docs); cat build/gate-docs.log; }
# CGameState's 42 member offsets and sizes, measured with mwcceppc's own flags. The header's map
# is a claim until something re-derives it, and this is that: a header edit that moved a member
# fails here rather than in a later lane's per-function percentage. It needs the toolchain, like
# the build does.
step "gs offsets";     python3 tools/probe_gs_offsets.py >build/gate-gsoff.log 2>&1 && echo ok || { fail+=(gs-offsets); tail -6 build/gate-gsoff.log; }
step "raw offsets";    python3 tools/check_raw_offsets.py >build/gate-raw.log 2>&1 && echo ok || { fail+=(raw-offsets); cat build/gate-raw.log; }
step "decl order";     python3 tools/check_decl_order.py >build/gate-order.log 2>&1 && echo ok || { fail+=(decl-order); cat build/gate-order.log; }
step "files.cmake";     python3 tools/check_files_cmake.py >build/gate-files.log 2>&1 && echo ok || { fail+=(files-cmake); cat build/gate-files.log; }
step "module order";   python3 tools/gen_module_order.py --check >build/gate-modorder.log 2>&1 && echo ok || { fail+=(module-order); cat build/gate-modorder.log; }
# The compile sweep AND the port's real link, in one step and one verdict. This step used to
# compile 648 translation units and never link them, so it reported "0 failed" on a port
# whose `ld` failed - and the gate passed. The probe now runs `link_check.sh --strict`, which
# is the same executable, objects, libraries and flags the shipping build uses, and fails
# when the link does not resolve.
#
# NOTE: no --no-link here. A gate step that can be asked to skip the check it exists for is
# the same hole as a gate step that does not check, and this one is what caught nothing.
#
# The failure prints the summary line and then the named symbols, because "the link failed"
# is not actionable and "these 341 symbols are undefined" is. `tail -5` hid the names behind
# a wall of one-line parser output, so the names are pulled out specifically.
step "port probe";     ./tools/probe_sources.sh >build/gate-probe.log 2>&1 && echo ok \
                       || { fail+=(probe); head -1 build/gate-probe.log | sed 's/^/  /'
                            sed -n '2,13p' build/gate-probe.log | sed 's/^/  /'
                            tail -1 build/gate-probe.log | sed 's/^/  /'; }
# Measures this tree's own sources: --rebuild keeps it from ever being stale, and a
# non-zero exit here is a real change in the port's link gap, not a build artefact.
# Block on the background port build first. link_check.sh owns it; link_gap.py's own ninja
# invocation is then a no-op against the same, already-current tree.
wait $LINK_PID; LINK_RC=$?
step "port link gap";   python3 tools/link_gap.py --rebuild >build/gate-link.log 2>&1 && echo ok \
                         || { fail+=(link-gap); tail -6 build/gate-link.log; }
# link_gap.py counts what is MISSING and says nothing about what is defined twice, so a
# Matching unit landing on a symbol that src/MetroidPrime/PortLinkStubs.cpp still stubs
# passes this step and then fails the host link as a duplicate. That happened on
# 2026-09-26 with CSimpleShadow::SetAlwaysCalculateRadius, and the boot probe did not
# catch it either - it links with the reach stubs, so the duplicate never appears there.
# The port link is the only instrument that sees it, so the gate has to run it.
# NOTE the variable: link_check.sh reads MP_TOOLCHAIN, not the MP_TOOLCHAIN_DIR that
# gate.sh itself uses. Getting that wrong makes the step fail with a bogus "cmake not
# found" that reads like a broken build - and a gate step that cannot pass is worse than
# no gate step, because it is a step everyone learns to ignore. So an unreadable log is
# a FAILURE here, never a pass.
# The build already happened, in the background, at the top of this script. Re-running
# link_check.sh here would be the second full port build of the run - which is what this step
# used to do, and the reason the gate grew from minutes to tens of minutes.
cp build/gate-linkcheck.log build/gate-dups.log 2>/dev/null
                         rc=$LINK_RC
                         dups=$(sed -n 's/^link_check: duplicate definitions *//p' build/gate-dups.log | head -1)
                         # An independent review noted this step parses only the duplicate
                         # line and ignores link_check.sh's *exit status*, so a link that
                         # cannot complete at all - a missing target, a configure error -
                         # could pass here while the duplicate line still parsed. Note the
                         # order: `$?` must be captured immediately after link_check.sh, and
                         # before the `sed` pipeline, or it is sed's status and not its own.
                         #
                         # `link_check.sh` deliberately exits non-zero whenever the port does
                         # not link - which is the normal state, since hundreds of retail
                         # symbols are still missing - so the exit status alone cannot be the
                         # test. The test is: a duplicate count is present *and* the log does
                         # not say the run was aborted rather than completed.
                         #
                         # The third condition is the one that was missing, and it is the one that
                         # let a real duplicate through. The three counts are scraped out of the
                         # build log, so a run that never reaches the linker's duplicate pass
                         # scrapes to zero and reads as a clean result. `link_check.sh` now says
                         # so explicitly ("the LINKER NEVER RAN") when the log holds no linker
                         # diagnostic at all, and this step treats that as a failure rather than
                         # as a zero. A count with no link behind it is not a small number, it is
                         # no number.
                         if [ -z "$dups" ] || grep -q "^link_check: compile errors [1-9]" build/gate-dups.log \
                            || grep -q "the LINKER NEVER RAN" build/gate-dups.log; then
                             fail+=(link-dups)
                             echo "    link_check.sh did not produce a trustworthy duplicate count"
                             echo "    (exit $rc, duplicates ${dups:-<none>}):"
                             tail -4 build/gate-dups.log | sed 's/^/      /'
                         elif [ "$dups" = "0" ]; then echo ok
                         else fail+=(link-dups); grep -A4 "^  DUP" build/gate-dups.log | head -8; fi
# The diagnostic reachability stubs (`-DMP_BOOT_STUBS=ON`, which only tools/boot_probe.sh
# passes) make the link SUCCEED and report 0 undefined - wrong by 318. This is the check
# that keeps the honest number honest, and it belongs in the gate rather than in a habit.
step "reach stubs";     python3 tools/check_boot_stubs.py build-port-link >/dev/null 2>&1 \
                         && echo "not in a real build  ok" \
                         || { fail+=(boot-stubs); python3 tools/check_boot_stubs.py build-port-link; }

echo
if [ "${#fail[@]}" -eq 0 ]; then echo "GATE PASS  $(git rev-parse --short HEAD)+$(git status --porcelain | wc -l) changed"; exit 0; fi
echo "GATE FAIL: ${fail[*]}"; exit 1
