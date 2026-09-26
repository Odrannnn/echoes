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
        "Matching at 100.00% (retail 0x80142498, 0x24) - CGameState::GetHardModeDamageMultiplier, a pure reader of the member at +0x34. Measured with tools/link_check.sh: listing it leaves the port's undefined count unchanged, and its only caller is gameplay the boot never reaches. Listed when something on the boot path calls it.",
    "src/MetroidPrime/TypesMatch.cpp":
        "sizes throwaway classes with uchar x_pad0[0x2f0 - sizeof(CPhysicsActor)]; the host's "
        "CPhysicsActor exceeds retail's 0x2f0, the subtraction underflows. Its six TypesMatch "
        "bodies live in PortGlobals.cpp, so listing it would duplicate them.",
    "src/Kyoto/Text/CStringTable.cpp":
        "casts a pointer to `uint` at lines 92 and 98 and loses precision on a 64-bit host. Same "
        "class as the CTweakContents layout: 32-bit game pointers on a 64-bit target.",
    "src/MetroidPrime/CInputGeneratorUpdate.cpp":
        "declares four extern \"C\" retail functions that nothing implements - fn_8028C058, "
        "fn_80306BB0, fn_80048CF4 and fn_80048C08 - and calls "
        "Push__18CArchitectureQueueFRC20CArchitectureMessage, which lives in "
        "src/MetroidPrime/main.cpp. Listing it would add five symbols to the port's link gap "
        "to close none: the port already asks for _ZN15CInputGenerator6UpdateEfR18CArchitectureQueue "
        "and has nothing that could satisfy it. It becomes worth listing when those four "
        "functions are written.",
    "src/MetroidPrime/Player/CGameStateStreamCtor.cpp":
        "fn_80144140, retail 0x80144140: CGameState's stream constructor. It names twenty-one "
        "retail functions as relocations that the port does not define (fn_8015C34C, "
        "fn_80144924, fn_80193E08, ReadBits__16CBitStreamReaderFUi, ...), and one block of it is "
        "not expressible in C++ at all - the memset-shaped fill at 0x801444E0, whose length is "
        "read out of an uninitialised stack word. (It used to be two blocks; the second, "
        "fn_80146154, is CPersistentOptionsCtor.cpp below, Matching at 100%.) Listing it would "
        "add twenty-one undefined symbols to the port's link to close none. It becomes worth "
        "listing when the port calls CGameState::CGameState(CInputStream&, int), which is "
        "CMain::StreamNewGameState in src/MetroidPrime/main.cpp:704.",
    "src/MetroidPrime/Player/CPersistentOptionsCtor.cpp":
        "fn_80146154, retail 0x80146154: the constructor of CGameState+0xDC. It calls fn_80145C98 "
        "(0x2F4, a 15-function subtree), which the port does not define, and nothing but "
        "CGameStateCtor.cpp calls it. The measurement for listing the whole chain is "
        "CGameGlobalObjectsCtor.cpp's entry.",
    "src/MetroidPrime/Player/CGameStateCardOptsCtor.cpp":
        "fn_80145950, retail 0x80145950: the constructor of CGameState+0x54. It calls fn_80146154 "
        "(CPersistentOptionsCtor.cpp) and fn_80145628 (a 16-function subtree). The measurement for "
        "listing the whole chain is CGameGlobalObjectsCtor.cpp's entry.",
    "src/MetroidPrime/Player/CGameStateMemcardCtor.cpp":
        "fn_80009DBC, retail 0x80009DBC: the SGameStateMemcard constructor. Its guest bytes "
        "lbl_80417D90/lbl_80417D91 are now defined (PortGlobals.cpp), so listed with "
        "SGameStateMemcardReset.cpp and SGameStateMemcardFill.cpp the three are +1 - fn_80009AC0, "
        "another lane's - and close nothing until CGameStateCtor.cpp calls them. The measurement "
        "for listing the whole chain is CGameGlobalObjectsCtor.cpp's entry.",
    "src/MetroidPrime/CGameGlobalObjectsCtor.cpp":
        "__ct__18CGameGlobalObjectsFR10COsContextR10CMemorySys, retail 0x8000848C, 0xE4 bytes, "
        "Matching (flip_test PASS), the only writer of gpGameState. Its three cheap callees are now "
        "written (fn_8016C230, fn_801F0A44 and lbl_80418EC8, all listed) and fn_80032008 is "
        "CCharacterFactoryBuilder's constructor (excluded below). Measured with tools/link_check.sh "
        "(lane frame, 2026-09-26): this tree is 325 undefined; listing CGameGlobalObjectsCtor.cpp, "
        "Factories/CCharacterFactoryBuilder.cpp, CGameStateCtor.cpp, CGameStateCardOptsCtor.cpp, "
        "CPersistentOptionsCtor.cpp, CGameStatePlayerLoop.cpp, CGameStateMemcardCtor.cpp, "
        "SGameStateMemcardReset.cpp and SGameStateMemcardFill.cpp together, with the boot call, is "
        "338 - thirteen opened, none closed: CSimplePool(IFactory&), fn_803096C4 and fn_80009AC0 "
        "(other lanes), CCharacterFactory's constructor and destructor, ~CSimplePool, fn_8000934C, "
        "fn_8014306C, fn_801437DC, fn_80145628, fn_80145A2C, fn_80145C98 and fn_80180430. "
        "docs/research/patches/cgameglobalobjects_integration.patch lists them all at once.",
    "src/MetroidPrime/Factories/CCharacterFactoryBuilder.cpp":
        "CCharacterFactoryBuilder and its CDummyFactory, retail 0x80031E60..0x80032230, NonMatching "
        "at 80.33% (8 of 10 functions at 100%). Listed alone it is +4 - CSimplePool's constructor "
        "and destructor, and CCharacterFactory's constructor (fn_80030410, the root of a "
        "114-function subtree) and destructor, which g++ names because it speculatively "
        "devirtualises the delete in TObjOwnerDerivedFromIObj<CCharacterFactory> - and nothing "
        "calls it until CGameGlobalObjectsCtor.cpp is listed. The measurement for listing the whole "
        "chain is CGameGlobalObjectsCtor.cpp's entry.",
    "src/MetroidPrime/Player/CGameStateCtor.cpp":
        "fn_801449C8, retail 0x801449C8: CGameState's default constructor, Matching. Worth listing "
        "only with its caller, CGameGlobalObjectsCtor.cpp. The measurement for listing the whole "
        "chain is CGameGlobalObjectsCtor.cpp's entry.",
    "src/MetroidPrime/CMainResetGameState.cpp":
        "ResetGameState__5CMainFv, retail 0x80003A48, 0x1A0 = 416 bytes: one of only three "
        "functions the port's boot still waits on, and the one CGameArchitectureSupport's "
        "constructor calls (src/MetroidPrime/main.cpp:350) before it dereferences gpGameState. "
        "Measured with tools/link_check.sh on this tree: listing it takes the port's undefined "
        "count from 326 to 341, because all sixteen of its callees are retail functions the port "
        "does not define (fn_80005108, fn_80004E84, fn_80004C90, fn_80004AA0, fn_80004990, "
        "fn_80004154, fn_80003F08, fn_80003D00, fn_80142920, fn_801427DC, fn_80003BE8, the four "
        "destructors, and fn_801449C8, which is in CGameStateCtor.cpp and excluded above). It "
        "closes _ZN5CMain14ResetGameStateEv and nothing else, so the net is +15. It becomes worth "
        "listing together with CGameStateCtor.cpp, CGameStateStreamCtor.cpp and the port-side "
        "bodies of those sixteen.",
    "src/MetroidPrime/Player/CGameStatePlayerLoop.cpp":
        "fn_801440C0, retail 0x801440C0: the if (gpMemoryCard) hook CGameState's default "
        "constructor calls. It opens fn_80180430, fn_80145A2C, fn_8014306C and fn_801437DC; none of "
        "the four is reached on the boot path under tools/boot_probe.sh, because gpMemoryCard is "
        "null there. The measurement for listing the whole chain is CGameGlobalObjectsCtor.cpp's "
        "entry.",
    "src/MetroidPrime/Player/SGameStateMemcardReset.cpp":
        "fn_80009898, retail 0x80009898: two calls on the SGameStateMemcard at CGameState+0x204, "
        "fn_80009AC0 (another lane's) and fn_800098CC. See CGameStateMemcardCtor.cpp's entry.",
    "src/MetroidPrime/Player/SGameStateMemcardFill.cpp":
        "fn_800098CC, retail 0x800098CC, NonMatching at 99.55%. Its guest byte lbl_80417D93 is now "
        "defined (PortGlobals.cpp), so it opens nothing; it is excluded only because nothing listed "
        "calls it. See CGameStateMemcardCtor.cpp's entry.",
    "src/Runtime/__init_cpp_exceptions.cpp":
        "includes __ppc_eabi_linker.h, which is PowerPC EABI linker sections. Host-incompatible "
        "by nature.",
    "src/Kyoto/Audio/CAudioSysVolume.cpp":
        "retail's body reads and writes .sdata 0x80418BEA/0x80418BEC, which are guest addresses. "
        "The port's own body is in src/MetroidPrime/PortAudio.cpp; see docs/research/audio_stack.md.",
    "src/Kyoto/Audio/CAudioSysSurround.cpp":
        "retail's body calls fn_803078FC and fn_80389A58, which exist only inside main.dol. The "
        "port's own body is in src/MetroidPrime/PortAudio.cpp; see docs/research/audio_stack.md.",
    "src/Kyoto/Audio/CAudioSysAICallback.cpp":
        "retail's body reads and writes .sdata 0x80418BEE and .sbss 0x80419B84, which are guest "
        "addresses. The port's own body is in src/MetroidPrime/PortAudio.cpp; see "
        "docs/research/audio_stack.md.",
    "src/Kyoto/Audio/CAudioSysSysVolume.cpp":
        "retail's body calls fn_803899C4 and fn_80389964, which exist only inside main.dol. The "
        "port's own body is in src/MetroidPrime/PortAudio.cpp; see docs/research/audio_stack.md.",
    "src/Kyoto/Audio/CAudioSysTrkSampleRate.cpp":
        "retail's body calls the SDK's DTKSetSampleRate, which the port implements as a no-op in "
        "platform/sdk_stubs.cpp; the port's own body is in src/MetroidPrime/PortAudio.cpp. See "
        "docs/research/audio_stack.md.",
    "src/Kyoto/Audio/CStreamAudioManagerSfxVolume.cpp":
        "retail's body writes .sdata 0x80418C30, a guest address. The port's own body is in "
        "src/MetroidPrime/PortAudio.cpp; see docs/research/audio_stack.md.",
    "src/Kyoto/Audio/CStreamAudioManagerMusicVolume.cpp":
        "retail's body writes .sdata 0x80418C28 and calls fn_803212C8, a guest address and a "
        "main.dol-only symbol. The port's own body is in src/MetroidPrime/PortAudio.cpp; see "
        "docs/research/audio_stack.md.",
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
