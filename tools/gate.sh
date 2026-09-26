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
step "port probe";     ./tools/probe_sources.sh >build/gate-probe.log 2>&1 && echo ok || { fail+=(probe); tail -5 build/gate-probe.log; }
# Measures this tree's own sources: --rebuild keeps it from ever being stale, and a
# non-zero exit here is a real change in the port's link gap, not a build artefact.
step "port link gap";   python3 tools/link_gap.py --rebuild >build/gate-link.log 2>&1 && echo ok \
                         || { fail+=(link-gap); tail -6 build/gate-link.log; }
# link_gap.py counts what is MISSING and says nothing about what is defined twice, so a
# Matching unit landing on a symbol that src/MetroidPrime/PortLinkStubs.cpp still stubs
# passes this step and then fails the host link as a duplicate. That happened on
# 2026-09-26 with CSimpleShadow::SetAlwaysCalculateRadius, and the boot probe did not
# catch it either - it links with the reach stubs, so the duplicate never appears there.
# The port link is the only instrument that sees it, so the gate has to run it.
step "port link dups";  ./tools/link_check.sh >build/gate-dups.log 2>&1
                         dups=$(sed -n 's/^link_check: duplicate definitions *//p' build/gate-dups.log | head -1)
                         if [ "${dups:-1}" = "0" ]; then echo ok
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
