#!/usr/bin/env python3
"""Every DOL object `configure.py` declares must be in `files.cmake`, or excluded with a reason.

    python3 tools/check_files_cmake.py

Why this exists. `configure.py` declares 239 DOL objects. `files.cmake` is the port build's source
list, and for most of a release the two disagreed: **96 configured, on-disk units were named in
neither**, so the port had never compiled the decompilation's own code. All 72 of the `Matching`
loader thunks were among them - `Matching` in the DOL, and still `MISSING` in the port's link, for
as long as that was true. `link_gap.py` did not catch it, because `link_gap.py` derives the gap from
the objects `files.cmake` produces, so the omission was invisible to the tool that was supposed to
measure it. Only `tools/link_check.sh`, which asks the real linker, saw it.

**A gap number from a tool is a statement about the tool's inputs.** This is the check for one class
of that.

Exclusions are not free: each one below is a unit that is configured and on disk and deliberately
not in the port build, with the reason it does not compile or does not belong. A new omission fails
the gate; a stale exclusion - a file that has since been listed, or no longer exists - also fails, so
the list cannot quietly become a place where things go to die.

**Note what this does *not* claim.** Being listed is not the same as being a win: lane e6 measured
`CModelDataDefaultCtor.cpp` at net **-1**, because defining a default constructor constructs its
members and can *open* a gap. This tool polices the omission, not the trade. Measure the net.

Exit status is 1 if anything disagrees.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# path -> why the port build does not compile it. Keep each reason to one line and keep it
# true; `tools/check_raw_offsets.py`'s rule applies here too - a named blocker beats a
# silent omission.
EXCLUDED = {
    "src/MetroidPrime/TypesMatch.cpp":
        "sizes throwaway classes with uchar x_pad0[0x2f0 - sizeof(CPhysicsActor)]; the host's "
        "CPhysicsActor exceeds retail's 0x2f0, the subtraction underflows. Its six TypesMatch "
        "bodies live in PortGlobals.cpp, so listing it would duplicate them.",
    "src/Kyoto/Text/CStringTable.cpp":
        "casts a pointer to `uint` at lines 92 and 98 and loses precision on a 64-bit host. Same "
        "class as the CTweakContents layout: 32-bit game pointers on a 64-bit target.",
    "src/Runtime/__init_cpp_exceptions.cpp":
        "includes __ppc_eabi_linker.h, which is PowerPC EABI linker sections. Host-incompatible "
        "by nature.",
}

# REL module entry points. Each is a module we have reimplemented and each defines RELMain and/or
# RELExit - and some a module-local SetFuncPtrs - which is correct on the cube, where they are
# separate modules, and impossible in a flat link. The fix is a host-only rename behind
# `#ifdef __MWERKS__`; tools/rename_module_entries.py writes it and
# platform/compiled_modules.cpp registers the results. **Attempted and reverted: it broke 8 module
# hashes in the matching build**, and proving a host-only rename costs nothing under mwcceppc is its
# own piece of work. Until then these modules' code is in no port binary.
MODULE_ENTRY = re.compile(r"\bvoid RELMain\((?:void)?\)|\bvoid RELExit\((?:void)?\)")

DOL_OBJECT = re.compile(
    r'Object\(\s*(Matching|NonMatching|MatchingFor)\s*(?:\([^)]*\))?\s*,\s*'
    r'"((?:MetroidPrime|Kyoto|rstl|Dolphin|Runtime|Lib|Math|NL)/[^"]+)\.cpp"')


def main() -> int:
    # Strip comments first: files.cmake documents its exclusions in comments that
    # name the very paths being excluded, and a naive scan counts those as listed.
    raw = (ROOT / "files.cmake").read_text()
    fm = "\n".join(line for line in raw.splitlines()
                    if not line.lstrip().startswith("#"))
    listed = set(re.findall(r"(src/\S+\.c(?:pp)?)", fm))
    configured = DOL_OBJECT.findall((ROOT / "configure.py").read_text())

    problems = []
    unaccounted = []
    module_entries = []
    for state, path in configured:
        src = ROOT / "src" / f"{path}.cpp"
        rel = f"src/{path}.cpp"
        if rel in listed:
            continue
        if rel in EXCLUDED:
            continue
        if not src.exists():
            # configure.py declares an object with no source file. That is a
            # different defect - a Matching unit with no source links retail
            # instead - and check_module_wiring.py is the right place for it.
            continue
        if MODULE_ENTRY.search(src.read_text(errors="replace")):
            # Counted and printed, never skipped in silence. The first version of
            # this tool `continue`d here, and a test that removed a listed unit
            # passed - which is the exact failure the tool exists to prevent.
            module_entries.append(rel)
            continue
        unaccounted.append((state, rel))

    for state, rel in unaccounted:
        problems.append(f"omitted: {rel} is a {state} object in configure.py and is neither in "
                        f"files.cmake nor in check_files_cmake.py's EXCLUDED list")

    # A stale exclusion is as bad as a missing one: it hides a unit that may have been
    # fixed, or that may no longer exist.
    for rel, why in EXCLUDED.items():
        if rel in listed:
            problems.append(f"stale:   {rel} is in EXCLUDED but is now listed in files.cmake")
        if not (ROOT / rel).exists():
            problems.append(f"stale:   {rel} is in EXCLUDED but the file does not exist")

    print(f"files.cmake: {len(listed)} sources; configure.py declares {len(configured)} DOL "
          f"objects; {len(EXCLUDED)} documented exclusions")
    print(f"  {len(module_entries)} further units are out because they define a module entry "
          f"point (RELMain/RELExit), which collides in a flat link. That is a known, "
          f"deliberate gap rather than an oversight - tools/rename_module_entries.py is "
          f"the fix and it is not yet proven safe under mwcceppc.")

    if problems:
        print("files.cmake disagrees with configure.py:")
        for p in problems:
            print("  " + p)
        print("\nEither add the path to files.cmake so the port compiles it, or add it to "
              "EXCLUDED in\ntools/check_files_cmake.py with a one-line reason. A configured unit "
              "that no port\nbinary contains is a symbol the port's link will ask for.")
        return 1
    print("every configured DOL object is either in files.cmake or excluded with a reason")
    return 0


if __name__ == "__main__":
    sys.exit(main())
