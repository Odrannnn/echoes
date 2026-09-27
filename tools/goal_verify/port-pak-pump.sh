#!/bin/bash
# tools/goal_verify/port-pak-pump.sh - run from the tree being judged. Exit 0 = the port boots
# through CGameGlobalObjects::AddPaksAndFactories' pak pump: every pak it admits reaches
# kAP_Loaded, `while (!AreAllPaksLoaded())` exits on its own, and the boot goes on to the renderer.
#
# The item's target (CResLoader::AsyncIdlePakLoading and its port-only helpers) is defined and
# links, so "the target is no longer undefined" is true before any work is done. Only running
# the port shows the defect, so this runs it: tools/boot_probe.sh builds the diagnostic binary
# (incrementally, in build-boot-probe/) and boots it against the disc for up to 120 s.
#
# Measured on goal/decomp eac0c3e before the fix: 7 of 7 paks reach kAP_Loaded, then the pump
# never exits - `x18+x30` climbs to 2313 while the loading list never empties, and the run is
# killed at the 120 s timeout without reaching the renderer -> FAIL. With the port's
# fn_802FD174 erasing from the list it is handed: the pump drains, "Initializing renderer..."
# and "boot: step 21c returned" follow -> PASS. (The run then faults later, in
# CEnvFxManager::Initialize; that is past this check and not what it measures.)
#
# boot_probe.sh may append diagnostic stubs to src/MetroidPrime/PortReachStubs.cpp. Those are
# only compiled into the probe (MP_BOOT_STUBS=ON), but they must not leak into the change being
# judged, so the file is put back exactly as the agent left it.
set -uo pipefail
STUBS=src/MetroidPrime/PortReachStubs.cpp
RUNLOG="${MP_BOOT_BUILD:-$PWD/build-boot-probe}/run.log"
SAVED="${TMPDIR:-/tmp}/goal-verify-stubs.$$"
cp "$STUBS" "$SAVED" || { echo "verify: cannot save $STUBS"; exit 1; }
trap 'cp "$SAVED" "$STUBS"; rm -f "$SAVED"' EXIT

rm -f "$RUNLOG"
if ! ./tools/boot_probe.sh > "${TMPDIR:-/tmp}/goal-verify-probe.$$" 2>&1; then
  tail -5 "${TMPDIR:-/tmp}/goal-verify-probe.$$"
  rm -f "${TMPDIR:-/tmp}/goal-verify-probe.$$"
  echo "verify: the boot probe did not build or could not run - PAK_PUMP FAIL"
  exit 1
fi
rm -f "${TMPDIR:-/tmp}/goal-verify-probe.$$"
[ -s "$RUNLOG" ] || { echo "verify: no run log at $RUNLOG - PAK_PUMP FAIL"; exit 1; }

python3 - "$RUNLOG" <<'PY'
import re, sys
lines = open(sys.argv[1], errors="replace").read().splitlines()
def first(pat):
    return next((i for i, l in enumerate(lines) if pat in l), None)
pump = first("[pak] pump: block 7 entered")
if pump is None:
    print("verify: the boot never reached the pak pump - PAK_PUMP FAIL"); sys.exit(1)
admitted = {m.group(1) for l in lines
            for m in [re.search(r'AddPakFileAsync\("([^"]+)"\) -> GATE OPEN', l)] if m}
loaded = {m.group(1) for l in lines
          for m in [re.search(r'^\[pak\] (\S+)\.pak: .*phase -> kAP_Loaded', l)] if m}
missing = sorted(admitted - loaded)
if not admitted:
    print("verify: no pak was admitted - PAK_PUMP FAIL"); sys.exit(1)
if missing:
    print(f"verify: {len(loaded & admitted)}/{len(admitted)} paks reached kAP_Loaded; "
          f"not loaded: {', '.join(missing)} - PAK_PUMP FAIL"); sys.exit(1)
if first("[pak] pump: giving up") is not None:
    print("verify: every pak loaded but the pump gave up - AreAllPaksLoaded() never became "
          "true - PAK_PUMP FAIL"); sys.exit(1)
after = next((i for i in range(pump + 1, len(lines))
              if lines[i].startswith("Initializing renderer...")), None)
if after is None:
    print(f"verify: all {len(admitted)} paks loaded but the boot never left the pump "
          "(no \"Initializing renderer...\" after it) - PAK_PUMP FAIL"); sys.exit(1)
print(f"PAK_PUMP PASS: {len(admitted)}/{len(admitted)} paks loaded, the pump drained, "
      "the boot reached the renderer")
PY
