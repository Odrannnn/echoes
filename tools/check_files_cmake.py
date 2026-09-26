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
    "src/MetroidPrime/Player/CGameStateGetHardModeDamageMultiplier.cpp":
        "Matching at 100.00% (retail 0x80142498, 0x24) - CGameState::GetHardModeDamageMultiplier, a pure reader of the member at +0x34. Measured with tools/link_check.sh: listing it leaves the port's undefined count unchanged and its only caller is gameplay the boot never reaches.",
    "src/Kyoto/Audio/CStreamAudioManagerMusicVolume.cpp":
        "retail's body writes .sdata 0x80418C28 and calls fn_803212C8, a guest address and a main.dol-only symbol. The port's own body is in src/MetroidPrime/PortAudio.cpp; see docs/research/audio_stack.md.",
    "src/MetroidPrime/Player/CPersistentOptionsInit.cpp":
        "fn_80145C98, retail 0x80145C98, 0x2F4 - Matching at 100.00%, 1/1. Measured with tools/link_check.sh: 315 -> 318 with it listed. It opens fn_80145ACC (0xC4, no body), SPersistentOptionsValue::SPersistentOptionsValue(int,int,int) and lbl_803A9208 (a guest data address, defined nowhere in the tree - the comment in CGameStateStreamCtor.cpp claiming PortGlobals.cpp defines it is wrong), and closes none. Excluded until those three have bodies.",
    "src/MetroidPrime/Player/CGameStateStreamCtor.cpp":
        "fn_80144140, retail 0x80144140: CGameState's stream constructor. It names twenty-one retail functions as relocations that the port does not define (fn_8015C34C, fn_80144924, fn_80193E08, ReadBits__16CBitStreamReaderFUi, ...), and one block of it is not expressible in C++ at all - the memset-shaped fill at 0x801444E0, whose length is read out of an uninitialised stack word. (It used to be two blocks; the second, fn_80146154, is CPersistentOptionsCtor.cpp below, Matching at 100%.) Listing it would add twenty-one undefined symbols to the port's link to close none. It becomes worth listing when the port calls CGameState::CGameState(CInputStream&, int), which is CMain::StreamNewGameState in src/MetroidPrime/main.cpp:704.",
    "src/Kyoto/Audio/CStreamAudioManagerSfxVolume.cpp":
        "retail's body writes .sdata 0x80418C30, a guest address. The port's own body is in src/MetroidPrime/PortAudio.cpp; see docs/research/audio_stack.md.",
    "src/Kyoto/Basics/COsContextAllocFromArena.cpp":
        "fn_8028BFFC = COsContext::AllocFromArena(unsigned long), retail 0x8028BFFC, 0x5C = 92 bytes, Matching 100.00%, flip_test PASS. NOT in files.cmake on purpose: src/Kyoto/Basics/COsContext.cpp already defines AllocFromArena and the constructor for the host, so listing either here is a duplicate definition that link_gap.py structurally cannot see. Measured with tools/link_check.sh --rebuild.",
    "src/MetroidPrime/CInputGeneratorUpdate.cpp":
        "declares four extern \"C\" retail functions that nothing implements - fn_8028C058, fn_80306BB0, fn_80048CF4 and fn_80048C08 - and calls Push__18CArchitectureQueueFRC20CArchitectureMessage, which lives in src/MetroidPrime/main.cpp. Listing it would add five symbols to the port's link gap to close none: the port already asks for _ZN15CInputGenerator6UpdateEfR18CArchitectureQueue and has nothing that could satisfy it. It becomes worth listing when those four functions are written.",
    "src/Kyoto/Audio/CAudioSysTrkSampleRate.cpp":
        "retail's body calls the SDK's DTKSetSampleRate, which the port implements as a no-op in platform/sdk_stubs.cpp; the port's own body is in src/MetroidPrime/PortAudio.cpp. See docs/research/audio_stack.md.",
    "src/MetroidPrime/CMainResetGameState.cpp":
        "ResetGameState__5CMainFv, retail 0x80003A48, 0x1A0 = 416 bytes: one of only three functions the port's boot still waits on, and the one CGameArchitectureSupport's constructor calls (src/MetroidPrime/main.cpp:350) before it dereferences gpGameState. Measured with tools/link_check.sh on this tree: listing it takes the port's undefined count from 326 to 341, because all sixteen of its callees are retail functions the port does not define (fn_80005108, fn_80004E84, fn_80004C90, fn_80004AA0, fn_80004990, fn_80004154, fn_80003F08, fn_80003D00, fn_80142920, fn_801427DC, fn_80003BE8, the four destructors, and fn_801449C8, which is in CGameStateCtor.cpp and excluded above). It closes _ZN5CMain14ResetGameStateEv and nothing else, so the net is +15. It becomes worth listing together with CGameStateCtor.cpp, CGameStateStreamCtor.cpp and the port-side bodies of those sixteen.",
    "src/Kyoto/Audio/CAudioSysDestructor.cpp":
        "retail's body calls five main.dol-only symbols (fn_80307BC4, fn_80307EEC, fn_8039E2A0, fn_803089B4, fn_80308930) and reads four guest .sbss words (0x80419B68, 0x80419B6C, 0x80419B70, 0x80419B74), none of which a host build has. Matching at 100.00%, 1/1, retail 0x8030889C, 0x94. The port's own audio shutdown is in src/MetroidPrime/PortAudio.cpp; see docs/research/audio_stack.md.",
    "src/Kyoto/Audio/CAudioSysAICallback.cpp":
        "retail's body reads and writes .sdata 0x80418BEE and .sbss 0x80419B84, which are guest addresses. The port's own body is in src/MetroidPrime/PortAudio.cpp; see docs/research/audio_stack.md.",
    "src/Kyoto/Audio/CAudioSysSysVolume.cpp":
        "retail's body calls fn_803899C4 and fn_80389964, which exist only inside main.dol. The port's own body is in src/MetroidPrime/PortAudio.cpp; see docs/research/audio_stack.md.",
    "src/Runtime/__init_cpp_exceptions.cpp":
        "includes __ppc_eabi_linker.h, which is PowerPC EABI linker sections. Host-incompatible by nature.",
    "src/Kyoto/Audio/CAudioSysSurround.cpp":
        "retail's body calls fn_803078FC and fn_80389A58, which exist only inside main.dol. The port's own body is in src/MetroidPrime/PortAudio.cpp; see docs/research/audio_stack.md.",
    "src/Kyoto/Audio/CAudioSysVolume.cpp":
        "retail's body reads and writes .sdata 0x80418BEA/0x80418BEC, which are guest addresses. The port's own body is in src/MetroidPrime/PortAudio.cpp; see docs/research/audio_stack.md.",
    "src/Kyoto/Basics/COsContextCtor.cpp":
        "COsContext::COsContext(bool, bool), retail 0x8028C09C, 0xE0 = 224 bytes, NonMatching at 90.91% - 12 bytes over its claim, three strength-reduced instructions in the switch. Not listed for the same reason as COsContextAllocFromArena.cpp: COsContext.cpp defines the host's copy.",
    "src/MetroidPrime/TypesMatch.cpp":
        "sizes throwaway classes with uchar x_pad0[0x2f0 - sizeof(CPhysicsActor)]; the host's CPhysicsActor exceeds retail's 0x2f0, the subtraction underflows. Its six TypesMatch bodies live in PortGlobals.cpp, so listing it would duplicate them.",
    "src/Kyoto/Text/CStringTable.cpp":
        "casts a pointer to `uint` at lines 92 and 98 and loses precision on a 64-bit host. Same class as the CTweakContents layout: 32-bit game pointers on a 64-bit target.",
    "src/Kyoto/CResFactoryCtor.cpp":
        "fn_802FB154 = CResFactory::CResFactory(), retail 0x802FB154, 0xA8 - NonMatching at 93.86%, and it was implementing the WRONG function (fn_803096C4) until this session. Measured with tools/link_check.sh: listed it takes the port's undefined count 326 -> 330, because the port's own CResFactory::CResFactory() in CResFactoryPortVirtuals.cpp already provides that symbol. The port needs vtable for CResFactory, not this constructor.",
    "src/Kyoto/CSimplePoolCtor.cpp":
        "fn_80301008 = CSimplePool::CSimplePool(IFactory&), retail 0x80301008, 0x150 - NonMatching at 94.32%, unit_fit.sh reports 336/336/336 with no extra functions. Measured with link_check.sh: listed it takes the port's undefined count 326 -> 334. The port's real CSimplePool gap is vtable for CSimplePool - ten virtuals plus ~CSimplePool - not this constructor.",
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
