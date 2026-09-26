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
    "src/MetroidPrime/Player/SPersistentOptionsValueClamp.cpp":
        "fn_801461AC, retail 0x801461AC, 0x44 = 68 bytes, Matching at 100.00%, flip_test PASS, 1/1 - SPersistentOptionsValue's only member function, the clamp its own constructor calls as its last statement. Not listed because the port never asks for it: measured with tools/link_check.sh, neither `fn_801461AC` nor the class's constructor is in the undefined set, so listing it (with SPersistentOptionsValueCtor.cpp, which needs it) takes 325 -> 325 and closes none. It becomes worth listing when the port constructs a CPersistentOptions - i.e. when CGameState's option table is built, which is CMain::StreamNewGameState in src/MetroidPrime/main.cpp.",
    "src/MetroidPrime/Player/CPersistentOptionsMapInsert.cpp":
        "fn_80145ACC, retail 0x80145ACC, 0xC4 = 196 bytes, NonMatching at 71.37% - the option map's set-if-absent, and the one function of the map front end fn_80145C98 cannot be linked without. Not listed for two independent reasons: a NonMatching unit is not in the DOL link at all, and it relocates against fn_80146338 (retail 0x80146338, size:0x1B8, the rbtree node insert) which no unit implements and which is absent from the port's undefined set, so listing it would add one undefined to close none.",
    "src/MetroidPrime/Player/CPersistentOptionsMapLookup.cpp":
        "fn_80145B90 (0x80145B90, 0x4C) and fn_80145BDC (0x80145BDC, 0xBC), the contiguous pair 0x80145B90..0x80145C98. NonMatching at 96.70% - 99.05% and 95.74%, one instruction of *scheduling* in each: retail restores LR first in the epilogue of the first and loads the root where the local is created in the prologue of the second, and everything else is instruction-for-instruction identical. 61 body variants were ranked with tools/try_batch.py and nothing moved either off zero. Not listed: a NonMatching unit is not in the DOL link, neither symbol is on the port's undefined list, and until it is Matching nothing in the DOL may call it - which is what parks the map front end, MapInsert above included.",
    "src/MetroidPrime/Player/SPersistentOptionsValueCtor.cpp":
        "__ct__23SPersistentOptionsValueFiii, retail 0x801462DC, 0x3C = 60 bytes, Matching at 100.00%, flip_test PASS, 1/1. Same measured reason as SPersistentOptionsValueClamp.cpp: nothing on the port constructs an SPersistentOptionsValue, so the mangled name is absent from the undefined set. Its *return type* is load-bearing and has to stay a constructor - the ABI-legal by-value-return spelling of the same thing reproduces the frame byte for byte and re-derives all eleven `addi r5,r1,N` in CPersistentOptionsInit.cpp, which is 99.98% and not Matching - so the port-side caller has to be a real construction rather than a call.",
    "src/Kyoto/Audio/CStreamAudioManagerMusicVolume.cpp":
        "retail's body writes .sdata 0x80418C28 and calls fn_803212C8, a guest address and a main.dol-only symbol. The port's own body is in src/MetroidPrime/PortAudio.cpp; see docs/research/audio_stack.md.",
    "src/MetroidPrime/Player/CPersistentOptionsInit.cpp":
        "fn_80145C98, retail 0x80145C98, 0x2F4 - Matching at 100.00%, 1/1. Measured with tools/link_check.sh: 315 -> 318 with it listed. It opens fn_80145ACC (0xC4, no body), SPersistentOptionsValue::SPersistentOptionsValue(int,int,int) and lbl_803A9208 (a guest data address, defined nowhere in the tree - the comment in CGameStateStreamCtor.cpp claiming PortGlobals.cpp defines it is wrong), and closes none. Excluded until those three have bodies.",
    "src/MetroidPrime/Player/CGameStateStreamCtor.cpp":
        "fn_80144140, retail 0x80144140: CGameState's stream constructor. It names twenty-one retail functions as relocations that the port does not define (fn_8015C34C, fn_80144924, fn_80193E08, ReadBits__16CBitStreamReaderFUi, ...), and one block of it is not expressible in C++ at all - the memset-shaped fill at 0x801444E0, whose length is read out of an uninitialised stack word. (It used to be two blocks; the second, fn_80146154, is CPersistentOptionsCtor.cpp below, Matching at 100%.) Listing it would add twenty-one undefined symbols to the port's link to close none. It becomes worth listing when the port calls CGameState::CGameState(CInputStream&, int), which is CMain::StreamNewGameState in src/MetroidPrime/main.cpp:704.",
    "src/Kyoto/Audio/CStreamAudioManagerSfxVolume.cpp":
        "retail's body writes .sdata 0x80418C30, a guest address. The port's own body is in src/MetroidPrime/PortAudio.cpp; see docs/research/audio_stack.md.",
    "src/MetroidPrime/CDamageVulnerabilityStatics.cpp":
        "CDamageVulnerability::NormalVulnerabilty(), retail 0x800DBB70, 0x10 = 16 bytes, Matching at 100.00%, flip_test PASS, 1/1 - the one of the class's five singleton accessors whose identity retail states. The port's link DOES ask for it, and it is one definition away from listable: the accessor hands back a pointer 4 bytes INTO the `.bss` blob at 0x803DA994, so the object needs a PC-side definition of `lbl_803DA994` (0xF4 bytes, five 0x30 objects) beside PortGlobals.cpp's, and PortGlobals.cpp has no such entry. Listing it as it stands takes the undefined count 325 -> 325. Its file was dead in the tree for a session on a stale claim - that the blob 'exports no such symbol' - which is wrong: dtk's fill object `auto_08_803C5A20_bss.o` DEFINES it (`powerpc-eabi-nm`: `B lbl_803DA994`), which is what the one relocation resolves against.",
    "src/Kyoto/Basics/COsContextAllocFromArena.cpp":
        "fn_8028BFFC = COsContext::AllocFromArena(unsigned long), retail 0x8028BFFC, 0x5C = 92 bytes, Matching 100.00%, flip_test PASS. NOT in files.cmake on purpose: src/Kyoto/Basics/COsContext.cpp already defines AllocFromArena and the constructor for the host, so listing either here is a duplicate definition that link_gap.py structurally cannot see. Measured with tools/link_check.sh --rebuild.",
    "src/MetroidPrime/CConsoleOutputWindowCtor.cpp":
        "__ct__20CConsoleOutputWindowFiff, retail 0x800D63F0, 0x1B4 = 436 bytes plus the 8-byte .sdata2 double at 0x8041B570, Matching at 100.00%, flip_test PASS. The file was in the tree but in neither configure.py nor files.cmake, so nothing compiled it; it is now configured and flips. Same two reasons as CAudioStateWinCtor.cpp, and both are worse here: (1) src/MetroidPrime/PortReachStubs.cpp:189 declares reachstub_36() asm(\"_ZN20CConsoleOutputWindowC1Eiff\"), the same symbol, so listing it duplicates under MP_BOOT_STUBS=ON. (2) It relocates against FIVE guest objects with no PC-side definition - lbl_803A89E8, lbl_803B37F0, lbl_8041B568, lbl_804181D8 and lbl_804190D0 - and against three out-of-line retail callees nothing implements (fn_802BAD6C, fn_802BAD0C, fn_80052150, fn_800D65A4 - four, not three). Listing it as it stands would take the port's undefined count 325 -> 331, so it wants its own file only once those seven have bodies or PC-side definitions.",
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
    "src/MetroidPrime/CAudioStateWinCtor.cpp":
        "__ct__14CAudioStateWinFv, retail 0x800E25C4, 0x5C = 92 bytes, Matching at 100.00%, flip_test PASS - the fourth of boot step 18's four IOWin constructors and the only one that had no source. NOT listed yet, for two named reasons, both one deletion and one file apart. (1) src/MetroidPrime/PortReachStubs.cpp:149 declares reachstub_26() asm(\"_ZN14CAudioStateWinC1Ev\"), which is exactly the symbol this unit defines, so listing it is a duplicate the moment MP_BOOT_STUBS=ON is on - the configuration tools/boot_probe.sh uses, and the one gate.sh's 'port link dups' step cannot see. Delete that alias. (2) The unit relocates against two guest objects, lbl_803A8AB8 (the 'CAudioStateWin' name) and lbl_803B3950 (the vtable), and a PC link has no retail object to bind them to, so they need PC-side definitions beside PortGlobals.cpp's. With both done, measured expectation is the port's undefined count 325 -> 324.",
    "src/Kyoto/Audio/CAudioSysSysVolume.cpp":
        "retail's body calls fn_803899C4 and fn_80389964, which exist only inside main.dol. The port's own body is in src/MetroidPrime/PortAudio.cpp; see docs/research/audio_stack.md.",
    "src/Runtime/__init_cpp_exceptions.cpp":
        "includes __ppc_eabi_linker.h, which is PowerPC EABI linker sections. Host-incompatible by nature.",
    "src/Kyoto/Audio/CAudioSysSurround.cpp":
        "retail's body calls fn_803078FC and fn_80389A58, which exist only inside main.dol. The port's own body is in src/MetroidPrime/PortAudio.cpp; see docs/research/audio_stack.md.",
    "src/Kyoto/Graphics/Carve802BF59C.cpp":
        "CGraphics::GetProjectionState(), retail 0x802BF59C, 0xC = 12 bytes, Matching at 100.00%, flip_test PASS, 1/1. It is a pure reader of lbl_80416F28 and nothing on the port calls it: measured with tools/link_check.sh, the port's undefined set contains CGraphics::SetScreenPosition, CGraphics::SetUseVideoFilter and CGraphics::SetModelMatrix but NOT GetProjectionState, so listing it takes the count 325 -> 325 and closes nothing. It becomes worth listing when the port's VI setup asks for the current projection (docs/research/boot_path.md step 21).",
    "src/Kyoto/Graphics/Carve802BF9C8.cpp":
        "CGraphics::SetFog, retail 0x802BF9C8, 0x30 = 48 bytes, Matching at 100.00%, flip_test PASS, 1/1. Same measured reason as Carve802BF59C.cpp: `CGraphics::SetFog` is absent from the port's undefined set, so listing it closes nothing while adding a dependency on CGX::SetFog (defined by Kyoto/Graphics/CGX.cpp, which is listed) and on the 4-byte COMDAT weak black$localstatic3$apply_fog__3CGXFv that including CGX.hpp drags in. It becomes worth listing with the frame loop's first SetFog call.",
    "src/Kyoto/Graphics/Carve802C2534.cpp":
        "CGraphics::SetViewPointMatrix(const CTransform4f&), retail 0x802C2534, 0xE0 = 224 bytes, NonMatching at 99.11% - 10 wrong bytes, all of them float register fields, and unit_fit.sh reports .text 224/224/224 with no extra functions. Retail's source used a pooled literal 0.f for the Mtx's zero column; reading the guest lbl_8041E508 instead makes MWCC create that temporary first, so it takes f12 and the m[i][0] triple drops to f11/f10/f9. Claiming the 4-byte .sdata2 at 0x8041E508 does not rescue it: that removes dtk's lbl_8041E508 definition, which fn_802C229C, fn_802C27C4, fn_802C2B38, fn_802C2CC0 and CGraphics::LoadDolphinSpareTexture all reference by name, and mwldeppc then reports undefined: lbl_8041E508 six times. Not listed because a NonMatching unit is not in the DOL link at all. The file's header has the full measurement; do not re-try either spelling.",
    "src/Kyoto/Audio/CAudioSysVolume.cpp":
        "retail's body reads and writes .sdata 0x80418BEA/0x80418BEC, which are guest addresses. The port's own body is in src/MetroidPrime/PortAudio.cpp; see docs/research/audio_stack.md.",
    "src/Kyoto/Basics/COsContextCtor.cpp":
        "COsContext::COsContext(bool, bool), retail 0x8028C09C, 0xE0 = 224 bytes, NonMatching at 90.91% - 12 bytes over its claim, three strength-reduced instructions in the switch. Not listed for the same reason as COsContextAllocFromArena.cpp: COsContext.cpp defines the host's copy.",
    "src/MetroidPrime/Carve80193E08.c":
        "fn_80193E08, retail 0x80193E08, 0x2C = 44 bytes, Matching at 100.00% on the first try, flip_test PASS. NOT listed, and the reason is specific: its byte-exact body stores two vtable pointers, lbl_803B0D68 (base, two zero header words plus one slot) and lbl_803B5CB0 (24 slots), which are RETAIL .data addresses at 0x803Bxxxx. On the host those are unmapped, so listing this unit makes the probe die earlier, inside this function, on a host-only artefact rather than on anything wrong with the decompilation. Give the port host definitions for both vtables - or better, name the class and give it a real key function - and it becomes listable. The unit also shows the tree's SGameStateMarker (u32,u32,u32) is wrong in two of three members: it zeroes a byte at +4, a byte at +5 and a word at +8, not three words. fn_80193C8C shows +8 is a difficulty tier read by CPlayerState::GetItemPercentageRatio. Class still unnamed.",
    "src/MetroidPrime/TypesMatch.cpp":
        "sizes throwaway classes with uchar x_pad0[0x2f0 - sizeof(CPhysicsActor)]; the host's CPhysicsActor exceeds retail's 0x2f0, the subtraction underflows. Its six TypesMatch bodies live in PortGlobals.cpp, so listing it would duplicate them.",
    "src/Kyoto/Text/CStringTable.cpp":
        "casts a pointer to `uint` at lines 92 and 98 and loses precision on a 64-bit host. Same class as the CTweakContents layout: 32-bit game pointers on a 64-bit target.",
    "src/Kyoto/CResFactoryCtor.cpp":
        "fn_802FB154 = CResFactory::CResFactory(), retail 0x802FB154, 0xA8 - NonMatching at 93.86%, and it was implementing the WRONG function (fn_803096C4) until this session. Measured with tools/link_check.sh: listed it takes the port's undefined count 326 -> 330, because the port's own CResFactory::CResFactory() in CResFactoryPortVirtuals.cpp already provides that symbol. The port needs vtable for CResFactory, not this constructor.",
    "src/Kyoto/CSimplePoolCtor.cpp":
        "fn_80301008 = CSimplePool::CSimplePool(IFactory&), retail 0x80301008, 0x150 - NonMatching at 94.32%, unit_fit.sh reports 336/336/336 with no extra functions. Measured with link_check.sh: listed it takes the port's undefined count 326 -> 334. The port's real CSimplePool gap is vtable for CSimplePool - ten virtuals plus ~CSimplePool - not this constructor.",
    "src/Kyoto/CARAMManager.cpp":
        "The port's own ARAM allocator, carried in from the Aurora port work (commit 3e7f972) and never compiled: tools/probe_cc.sh fails on line 6, because include/Kyoto/CARAMManager.hpp is a stub of 15 public members and 3 private statics while the file defines 8 more statics (mbInitialized, mpARAMStart, mChunkSize, mNumChunks, mpBookKeepingMemory, mDMAUniqueID, mChunksAllocated, mActiveDMAs), 3 private methods and a DMA callback, and its Alloc(uint) does not match the header's Alloc(uint, const unkptr = nullptr). It would take the header filled in AND the file split five ways, because a unit may claim one contiguous .text range and retail's nine named CARAMManager functions sit at 0x80301710 and 0x80301718 (already Kyoto/CARAMManagerGetInvalidAlloc.cpp and Kyoto/CARAMManagerIsAllocValid.cpp, both Matching), 0x80301800, 0x8030188C, 0x80301908, 0x80301994, 0x80301B38, 0x80301C20 and 0x80301C50; and its other eight bodies (Initialize, Shutdown, FindFreeBlocks, CollectGarbage, RefreshActiveDMAList, AramManagerDMACallback, GetAndIncrementUniqueID, WaitForAllDMAsToComplete) have no symbol in config/G2ME01/symbols.txt at all, so objdiff has nothing to pair them against. files.cmake's own header comment already said 're-enable it when the decompilation catches up'.",
    "src/rstl/rstl_string_l.cpp":
        "rstl::string_l and rstl::wstring_l, retail 0x802FF3DC..0x802FF448, 0x6C = 108 bytes, Matching at 100.00%, flip_test PASS, 2/2 - and 15 port call sites want them (src/MetroidPrime/main.cpp alone calls string_l eight times), so this is the highest-value file of the thirteen and the one with the most misleading answer. NOT listed because src/MetroidPrime/PortGlobals.cpp:673 already defines BOTH `rstl::string_l` and `rstl::wstring_l` for the host, with the same bodies and the same retail disassembly quoted in its own comment - verified with powerpc-eabi-nm over every object in build-port-link, where `_ZN4rstl8string_lEPKc` is defined by PortGlobals.cpp.o. Listing this file is a multiple definition the moment the port links, which is precisely what link_check.sh's duplicate count exists to catch. It becomes listable when PortGlobals.cpp's hand-written copy is deleted; that file belongs to the orchestrator, not to the lane that found the unit.",
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

    # **The other direction, which this tool could not see until a unit died in it.**
    # Everything above walks `configure.py`, so it only ever asks about files that are
    # *declared* somewhere. A source file that no manifest mentions is invisible here - and
    # that is not hypothetical: `src/MetroidPrime/CConsoleOutputWindowCtor.cpp` sat in the
    # tree, in neither `configure.py` nor `files.cmake`, with nothing compiling it, while
    # being 98.17% written and one statement-split away from `Matching`. A lane found it by
    # accident. This tool was built after 96 units were in exactly that state, and it still
    # could not see the seventeenth.
    #
    # So walk `src/` as well. `src/Runtime/` is the C runtime and `src/Dolphin/` is the SDK;
    # both are compiled by CMake through a different list, and `PortReachStubs.cpp` is added
    # by CMakeLists.txt only under `-DMP_BOOT_STUBS=ON`. Those three are skipped by path,
    # and the count is printed either way so a skip cannot grow unnoticed.
    SKIP_DIRS = ("src/Runtime/", "src/Dolphin/")
    SKIP_FILES = {"src/MetroidPrime/PortReachStubs.cpp"}
    orphans = []
    dead_module_entries = []
    for path in sorted((ROOT / "src").rglob("*")):
        if path.suffix not in (".c", ".cpp") or not path.is_file():
            continue
        rel = path.relative_to(ROOT).as_posix()
        if rel in SKIP_FILES or rel.startswith(SKIP_DIRS):
            continue
        if rel in configured or rel in listed or rel in EXCLUDED:
            continue
        # **A module entry point is excluded for its own reason and is not a dead file.**
        # The first version of this check missed that and called three `RELExit` units dead,
        # which is the same class of error the check exists to catch: a report that does not
        # consult the rule the rest of the tool already applies. Counted and printed, never
        # skipped in silence - the comment above `module_entries` says why, and that a test
        # once passed here because the tool skipped without recording.
        if MODULE_ENTRY.search(path.read_text(errors="replace")):
            dead_module_entries.append(rel)
            continue
        orphans.append(rel)
    for rel in orphans:
        problems.append(f"dead:    {rel} is on disk but in no manifest: not in configure.py, "
                        f"not in files.cmake, and not in EXCLUDED. Nothing compiles it.")

    # A stale exclusion is as bad as a missing one: it hides a unit that may have been
    # fixed, or that may no longer exist.
    for rel, why in EXCLUDED.items():
        if rel in listed:
            problems.append(f"stale:   {rel} is in EXCLUDED but is now listed in files.cmake")
        if not (ROOT / rel).exists():
            problems.append(f"stale:   {rel} is in EXCLUDED but the file does not exist")

    print(f"files.cmake: {len(listed)} sources; configure.py declares {len(configured)} DOL "
          f"objects; {len(EXCLUDED)} documented exclusions")
    print(f"  {len(orphans)} on-disk sources are in no manifest at all (dead); "
          f"src/Runtime/ and src/Dolphin/ and PortReachStubs.cpp are skipped by path")
    print(f"  {len(dead_module_entries)} of the dead files define a module entry point "
          f"(RELMain/RELExit), which is why they are out - counted, not silently skipped")
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
              "that no port\nbinary contains is a symbol the port's link will ask for - and a "
              "`dead:`\none is worse, because nothing builds it at all, so it cannot even be "
              "measured\nuntil someone notices the file.")
        return 1
    print("every configured DOL object is either in files.cmake or excluded with a reason")
    return 0


if __name__ == "__main__":
    sys.exit(main())
