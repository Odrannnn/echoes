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
    "src/MetroidPrime/CMainInitializeSubsystems.cpp":
        "PRESERVED, not landed - the collection was reverted. A carve of CMain::InitializeSubsystems (retail 0x80008680, 0x15C = 348 B) out of mainTail.cpp, which is the first function of that unit's claim so the split is narrow: 1 function moved, no WORSE, no GONE, mainTail 47 -> 46 functions. NonMatching at 99.08% with unit_fit 'claimed 348, ours 348, retail 348, fits' and no extra functions. The lane reproduced the lbl_80418BA8 fix (ARAlloc's return discarded, lbl_80418BA8 += lbl_80418EA0), which is what makes the object 0x15C rather than 0x158. **This is the one carve in this session whose split costs nothing**, unlike CMain::AsyncIdle's, which cost 5.11 points on the constructor step 17 runs. Not landed only because wiring it needs all four of configure.py, splits.txt, files.cmake and mainTail.cpp to land together, and two attempts at that went wrong: a three-way apply left splits.txt unmerged, and a hand-written claim copied mainTail's whole range including its .ctors/.sbss, which cost a claim and dropped total_functions 28465 -> 28464. **A carve is one change across four files or it is not a carve** - the same lesson as CMain::AsyncIdle, paid again. Source and the lane's own diff are intact at /tmp/opencode/initsub2.",
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
    "src/MetroidPrime/CMainAsyncIdle.cpp":
        "PRESERVED, not landed. A narrow-split carve of CMain::AsyncIdle (retail 0x80005B44, 0x120 = 288 B) taken out of main.cpp's single claim. The lane's findings are recorded in docs/HANDOFF.md and docs/RUNNING_THE_DECOMP.md - notably that the 'budget a split by how many functions it moves' rule is DISPROVEN, since 11 functions cost exactly what 38 did. Not configured because main.cpp would also have to be rewritten, and that rewrite is what cost matched 3973 -> 3972 when applied without the replacement unit: removing 11 functions from main.cpp's claim without adding the new unit's claim drops a matched function. Both halves must land together. Source intact at /tmp/opencode/asyncidle and /tmp/lane-keepers/async-m.patch.",
    "src/MetroidPrime/Carve80193E08.c":
        "fn_80193E08, retail 0x80193E08, 0x2C = 44 bytes, Matching at 100.00% on the first try, flip_test PASS. NOT listed, and the reason is specific: its byte-exact body stores two vtable pointers, lbl_803B0D68 (base, two zero header words plus one slot) and lbl_803B5CB0 (24 slots), which are RETAIL .data addresses at 0x803Bxxxx. On the host those are unmapped, so listing this unit makes the probe die earlier, inside this function, on a host-only artefact rather than on anything wrong with the decompilation. Give the port host definitions for both vtables - or better, name the class and give it a real key function - and it becomes listable. The unit also shows the tree's SGameStateMarker (u32,u32,u32) is wrong in two of three members: it zeroes a byte at +4, a byte at +5 and a word at +8, not three words. fn_80193C8C shows +8 is a difficulty tier read by CPlayerState::GetItemPercentageRatio. Class still unnamed.",
    "src/MetroidPrime/TypesMatch.cpp":
        "sizes throwaway classes with uchar x_pad0[0x2f0 - sizeof(CPhysicsActor)]; the host's CPhysicsActor exceeds retail's 0x2f0, the subtraction underflows. Its six TypesMatch bodies live in PortGlobals.cpp, so listing it would duplicate them.",
    "src/Kyoto/Text/CStringTable.cpp":
        "casts a pointer to `uint` at lines 92 and 98 and loses precision on a 64-bit host. Same class as the CTweakContents layout: 32-bit game pointers on a 64-bit target.",
    "src/Kyoto/CARAMManager.cpp":
        "The port's own ARAM allocator, carried in from the Aurora port work (commit 3e7f972) and never compiled: tools/probe_cc.sh fails on line 6, because include/Kyoto/CARAMManager.hpp is a stub of 15 public members and 3 private statics while the file defines 8 more statics (mbInitialized, mpARAMStart, mChunkSize, mNumChunks, mpBookKeepingMemory, mDMAUniqueID, mChunksAllocated, mActiveDMAs), 3 private methods and a DMA callback, and its Alloc(uint) does not match the header's Alloc(uint, const unkptr = nullptr). It would take the header filled in AND the file split five ways, because a unit may claim one contiguous .text range and retail's nine named CARAMManager functions sit at 0x80301710 and 0x80301718 (already Kyoto/CARAMManagerGetInvalidAlloc.cpp and Kyoto/CARAMManagerIsAllocValid.cpp, both Matching), 0x80301800, 0x8030188C, 0x80301908, 0x80301994, 0x80301B38, 0x80301C20 and 0x80301C50; and its other eight bodies (Initialize, Shutdown, FindFreeBlocks, CollectGarbage, RefreshActiveDMAList, AramManagerDMACallback, GetAndIncrementUniqueID, WaitForAllDMAsToComplete) have no symbol in config/G2ME01/symbols.txt at all, so objdiff has nothing to pair them against. files.cmake's own header comment already said 're-enable it when the decompilation catches up'.",
    "src/rstl/rstl_string_l.cpp":
        "rstl::string_l and rstl::wstring_l, retail 0x802FF3DC..0x802FF448, 0x6C = 108 bytes, Matching at 100.00%, flip_test PASS, 2/2 - and 15 port call sites want them (src/MetroidPrime/main.cpp alone calls string_l eight times), so this is the highest-value file of the thirteen and the one with the most misleading answer. NOT listed because src/MetroidPrime/PortGlobals.cpp:673 already defines BOTH `rstl::string_l` and `rstl::wstring_l` for the host, with the same bodies and the same retail disassembly quoted in its own comment - verified with powerpc-eabi-nm over every object in build-port-link, where `_ZN4rstl8string_lEPKc` is defined by PortGlobals.cpp.o. Listing this file is a multiple definition the moment the port links, which is precisely what link_check.sh's duplicate count exists to catch. It becomes listable when PortGlobals.cpp's hand-written copy is deleted; that file belongs to the orchestrator, not to the lane that found the unit.",
    # --- the 2026-09-28 upstream merge's Kyoto wave: the 109 configure.py units under
    # src/Kyoto plus src/Dolphin/os/__ppc_eabi_init.cpp. 37 are listed in files.cmake (the
    # block at the end of that file) and are net 0 on the port's link; the 72 below are
    # the units that are not, each with what it costs. Three more entries are host
    # sources the port carried before upstream had the real unit, now superseded and
    # dropped from files.cmake. Every "318 ->" figure is tools/link_check.sh's own
    # method - nm over the port's real link inputs, which reproduces the recorded 318
    # exactly - read per unit, because 109 separate builds is not a measurement anyone
    # can afford; the 37 that are listed were confirmed by one real link_check run.
    "src/MetroidPrime/CCallStack.cpp":
        "retail 0x8028BFD8..0x8028BFF4, 0x1C = 28 bytes: this port's own carve of CCallStack's three members (the 8-byte `+0` and `+4` reads and the 12-byte constructor that stores its second and third arguments). Superseded on the host by src/Kyoto/Alloc/CCallStackDolphin.cpp, which defines the same symbols - CCallStack::CCallStack(unsigned int, char const*, char const*), GetFileAndLineText and GetTypeText - plus the `kUnknownType` string, .rodata 0x803AEAB8, that this carve had to be handed by PortGlobals.cpp. That unit is configure.py's own Kyoto/Alloc/CCallStackDolphin, MatchingFor, 100.00% matched, 3/3, and its .rodata claim is 0x803AEAB8-0x803AEAC8, so the carve and it are the same three retail functions. Net on the port's link: 0 either way, measured with tools/link_check.sh's method (nm over the port's own link inputs, which reproduces the recorded 318 exactly).",
    "src/Kyoto/Basics/CStopwatchCSWData.cpp":
        "retail 0x8028C17C-0x8028C1F8, 124 bytes: this port's own carve of CStopwatch::CSWData::Initialize, taken out of CStopwatch.cpp's range so the unit could be promoted, with a `#ifdef TARGET_PC` hunk because retail's body reads the CPU-frequency register at the guest address 0x800000F8. Superseded on the host by src/Kyoto/Basics/CSWDataDolphin.cpp, which defines the same symbol (and the whole 0x8028C17C-0x8028C28C range, MatchingFor, 100.00% matched, 2/2) and derives mTimerFreq from OS_TIMER_CLOCK on every target, so the carve's TARGET_PC hunk is no longer needed. Net on the port's link: 0 either way.",
    "src/Kyoto/Basics/CStopwatchCSWDataWait.cpp":
        "retail 0x8028C1F8-0x8028C28C, 148 bytes: this port's own carve of CStopwatch::CSWData::Wait, NonMatching at 69.16% - the port wrote the busy-wait itself so that CStopwatch::Wait(float) would resolve. Superseded on the host by src/Kyoto/Basics/CSWDataDolphin.cpp, which defines the same symbol and is 100.00% matched (2/2 with Initialize), so retail's bytes and the port's are both in the DOL again and only one copy can be in the port's link. Net on the port's link: 0 either way.",
    "src/Kyoto/Basics/CBasicsDolphin.cpp":
        "retail 0x8028BD3C-0x8028BFD8, 0x29C = 668 bytes, NonMatching at 33.33%, 3 functions (CBasics::CopyMemory, CBasics::ZeroMemory and the one between them). 10 compile errors on a 64-bit host and none of them is a spelling away: `reinterpret_cast<uint>` of a `uchar*` loses precision at lines 25, 27, 43, 46, 83 and 90, and the copy loops call `__dcbz` - the PowerPC data-cache-block-zero builtin, which no host compiler declares - at lines 29, 56, 85 and 100. A host version of a dcbz-driven 32-byte-aligned copy loop is a rewrite, not a `#ifdef TARGET_PC` hunk. Nothing in the tree calls CBasics::CopyMemory or CBasics::ZeroMemory, so the port needs neither.",
    "src/Kyoto/Basics/COsContextDolphin.cpp":
        "retail 0x8028BFF4-0x8028C17C, 0x188 = 392 bytes, MatchingFor, 100.00% matched, 5 functions. Listing it is a duplicate of six symbols the port already defines - COsContext::COsContext(bool, bool), ~COsContext, AllocFromArena(unsigned long), Update - in src/Kyoto/Basics/COsContext.cpp, which is the port's own implementation and has intentional host behaviour rather than a translated body: it answers every method with the Aurora equivalent (OSAllocFromArenaLo, VIConfigure, aurora_dvd_open) and makes the memory-card and keyboard methods deliberate no-ops, none of which retail's Dolphin OS calls compile to. Same reason, and the same arrangement, as COsContextAllocFromArena.cpp and COsContextCtor.cpp in this list. It would also take the port's undefined count 318 -> 320, opening CBasics::Init() and OSGetLanguage.",
    "src/Kyoto/Graphics/CCubeModel.cpp":
        "retail 0x802BB3D8, 0x1814 = 6164 bytes, NonMatching at 88.46%, 26 functions. One compile error, and it is not local: include/Kyoto/Graphics/CGX_Impl.hpp:128 calls `GXSetArray(attr, data, stride)` with three arguments, where Aurora's dolphin/gx/GXGeometry.h:36 declares `GXSetArray(GXAttr, const void*, u32 size, u8 stride, bool le)`. CGX::SetArray has no `size` and no `le` to pass, and retail's third argument is a vertex *count* where Aurora's third is a byte size, so the host spelling is a question about what to pass rather than a mechanical fix - and the header is shared with Kyoto/Graphics/CGX.cpp, which is a configure.py unit.",
    "src/Kyoto/Graphics/DolphinCGraphics.cpp":
        "retail 0x802BE6F8, 0x57B4 = 22452 bytes, NonMatching at 88.24%, 102 functions - the largest unit in this batch. 4 compile errors, none of them local: a `const GXTexObj*` passed where a `GXTexObj*` is wanted (line 399), `GXGetOverflowCount` (line 798) which Aurora's dolphin/gx does not declare, and an inline-asm block (line 1269) that gcc parses as C++ (`expected '(' before '{' token`, `nop was not declared`). It is also retail's whole CGraphics, which the port replaces with src/Kyoto/Graphics/CGraphicsHostGlobals.cpp plus the Carve* units above.",
    "src/Kyoto/Graphics/DolphinCTexture.cpp":
        "retail 0x802C416C, 0x1F80 = 8064 bytes, NonMatching at 96.77%, 31 functions. 3 compile errors, all the same defect: `reinterpret_cast<uint>(this)` in retail's `sLoadedTextures` identity table - lines 137, 155 and 557 - loses precision on a 64-bit host. The table's whole purpose is to hold a 32-bit identity, so a host version needs a different data structure, not a cast change. The port's own CTexture constructor and destructor are in src/Kyoto/Graphics/CTexturePortStub.cpp.",
    "src/Kyoto/Graphics/CCubeMaterial.cpp":
        "retail 0x803029D8, 0x2ED0 = 11984 bytes, NonMatching at 62.79%, 43 functions. The same single error as CCubeModel.cpp - include/Kyoto/Graphics/CGX_Impl.hpp:128's three-argument `GXSetArray` against Aurora's five-argument `GXSetArray(GXAttr, const void*, u32, u8, bool)` - and the same reason it is not a local fix: the wrapper has no `size` or `le` and retail's third argument is a count where Aurora's is a byte size.",
    "src/Kyoto/Animation/DolphinCSkinnedModel.cpp":
        "retail 0x8030EEE4, 0xF3C = 3900 bytes, NonMatching at 92.31%, 26 functions. Listing it is a duplicate of CSkinnedModel::sPointGen and CSkinnedModel::ClearPointGeneratorFunc, which src/Kyoto/Animation/CSkinnedModel.cpp defines - and that file's own header says the arrangement is deliberate ('This unit is the GameCube matching unit and is listed in files.cmake but not in configure.py; DolphinCSkinnedModel.cpp is the reverse. Each build sees exactly one of the two definitions'), because the port needs the static at .sbss 0x80419BB8 to keep the symbol alive for two other functions. The port's two bodies win because they are the ones files.cmake needs. It also takes the port's undefined count 318 -> 331 (15 CCubeMaterial and CMaterialFilter accessors opened, 2 CSkinnedModel symbols closed).",
    "src/Kyoto/Animation/DolphinCSkinRules.cpp":
        "retail 0x8030FE20, 0xA18 = 2584 bytes, NonMatching at 95.65%, 23 functions. One compile error: `GXLoadNrmMtxIndx3x3` (line 54) is not declared by Aurora, whose extern/aurora/include/dolphin/gx/GXTransform.h has GXLoadPosMtxIndx and the GXLoad*Imm forms but no indexed 3x3 normal matrix. That is a missing platform entry point, not a `#ifdef TARGET_PC` hunk.",
    "src/Kyoto/Animation/DolphinCVirtualBone.cpp":
        "retail 0x80310838, 0x654 = 1620 bytes, NonMatching, 71.43% matched, 7 functions. Listing it takes the port's undefined count 318 -> 319: it opens `CGraphics::mGxModelView` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Graphics/DolphinCModel.cpp":
        "retail 0x80310F38, 0x13E8 = 5096 bytes, NonMatching at 94.44%, 36 functions. 3 compile errors: `GXLoadNrmMtxIndx3x3` (line 202) is not declared by Aurora - extern/aurora/include/dolphin/gx/GXTransform.h has GXLoadPosMtxIndx and the *Imm forms but no 3x3 indexed normal matrix - and line 340 reinterpret_casts `uchar**` to `const void**`, which casts away qualifiers. The port's own CModel bodies are in src/Kyoto/Graphics/CModelPortStub.cpp.",
    "src/Kyoto/DolphinCMemoryCardSys.cpp":
        "retail 0x80309108, 0x23A0 = 9120 bytes, NonMatching at 93.59%, 78 functions. It does not even reach the compiler's semantic pass: line 3 includes 'Kyoto/CCRC32.hpp', and this tree's header is include/Kyoto/CCrc32.hpp (src/Kyoto/CCrc32.cpp). The one-word fix was not taken because the unit is the whole memory-card system, whose host side is CARDInit/CARDProbe/CARDMount in src/MetroidPrime/PortAudio.cpp and CGameGlobalObjectsPad0Ctor.cpp, and nothing in the port calls it yet.",
    "src/Kyoto/Animation/CAnimSource.cpp":
        "retail 0x802A0F2C, 0x1D68 = 7528 bytes, NonMatching, 50.00% matched, 20 functions. Listing it takes the port's undefined count 318 -> 320: it opens `CAnimMathUtils::Slerp(CQuaternion const&, CQuaternion const&, flo…`, `CJointData_LinearStorage::ResetScales()` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CAnimSourceReader.cpp":
        "retail 0x802A2C94, 0x18A4 = 6308 bytes, NonMatching, 74.19% matched, 31 functions. Listing it takes the port's undefined count 318 -> 337: it opens `CAnimSource::GetOffset(CSegId const&, CCharAnimTime const&) const`, `CAnimSource::GetRotation(CSegId const&, CCharAnimTime const&) con…`, `CAnimSource::GetSegData(CCharLayoutInfo const&, CJointData_Linear…`, 16 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CAnimTreeDoubleChild.cpp":
        "retail 0x802A6C00, 0x1078 = 4216 bytes, NonMatching, 55.56% matched, 18 functions. Listing it takes the port's undefined count 318 -> 330: it opens `CAnimTreeNode::CAnimTreeNode(rstl::basic_string<char, rstl::char_…`, `CAnimTreeNode::IsCAnimTreeNode() const`, `CAnimTreeNode::~CAnimTreeNode()`, 9 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CAnimTreeLoopIn.cpp":
        "retail 0x8028EDDC, 0x1694 = 5780 bytes, MatchingFor, 100.00% matched, 32 functions. Listing it takes the port's undefined count 318 -> 343: it opens `CAnimTreeNode::CAnimTreeNode(rstl::basic_string<char, rstl::char_…`, `CAnimTreeNode::IsCAnimTreeNode() const`, `CAnimTreeNode::~CAnimTreeNode()`, 22 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CAnimTreeSequence.cpp":
        "retail 0x80290470, 0x2154 = 8532 bytes, NonMatching, 100.00% matched, 45 functions. Listing it takes the port's undefined count 318 -> 345: it opens `CAnimTreeNode::CAnimTreeNode(rstl::basic_string<char, rstl::char_…`, `CAnimTreeNode::IsCAnimTreeNode() const`, `CAnimTreeNode::~CAnimTreeNode()`, 24 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CAnimTreeBlend.cpp":
        "retail 0x802A6428, 0x7D8 = 2008 bytes, NonMatching, 50.00% matched, 10 functions. Listing it takes the port's undefined count 318 -> 348: it opens `CAnimTreeDoubleChild::VGetAdvancementResults(CCharAnimTime const&…`, `CAnimTreeDoubleChild::VGetBestUnblendedChild() const`, `CAnimTreeDoubleChild::VGetBoolPOIList(CCharAnimTime const&, CBool…`, 27 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CAnimTreeTransition.cpp":
        "retail 0x802A981C, 0xF34 = 3892 bytes, NonMatching, 27.78% matched, 18 functions. Listing it takes the port's undefined count 318 -> 344: it opens `CAnimTreeDoubleChild::VGetAdvancementResults(CCharAnimTime const&…`, `CAnimTreeDoubleChild::VGetBoolPOIList(CCharAnimTime const&, CBool…`, `CAnimTreeDoubleChild::VGetBoolPOIState(unsigned int) const`, 23 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CAnimTreeTweenBase.cpp":
        "retail 0x802AA750, 0x12B0 = 4784 bytes, NonMatching, 50.00% matched, 20 functions. Listing it takes the port's undefined count 318 -> 335: it opens `CAnimTreeDoubleChild::CAnimTreeDoubleChild(rstl::ncrc_ptr<CAnimTr…`, `CAnimTreeDoubleChild::VAdvanceView(CCharAnimTime const&)`, `CAnimTreeDoubleChild::VGetAdvancementResults(CCharAnimTime const&…`, 14 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CAnimation.cpp":
        "retail 0x8028C9C8, 0x1C0 = 448 bytes, NonMatching, 25.00% matched, 4 functions. Listing it takes the port's undefined count 318 -> 319: it opens `CMetaAnimFactory::CreateMetaAnim(CInputStream&)` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CAnimationSet.cpp":
        "retail 0x8028CB88, 0x1C34 = 7220 bytes, NonMatching, 19.40% matched, 67 functions. Listing it takes the port's undefined count 318 -> 323: it opens `CAnimPOIData::CAnimPOIData(CInputStream&)`, `CAnimation::CAnimation(CInputStream&)`, `CHalfTransition::CHalfTransition(CInputStream&)`, 2 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CAnimCharacterSet.cpp":
        "retail 0x8028E7BC, 0x620 = 1568 bytes, NonMatching, 12.50% matched, 16 functions. Listing it takes the port's undefined count 318 -> 320: it opens `CAnimationSet::CAnimationSet(CInputStream&)`, `CCharacterSet::CCharacterSet(CInputStream&)` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CCharacterInfo.cpp":
        "retail 0x802925C4, 0x16D4 = 5844 bytes, NonMatching, 9.52% matched, 42 functions. Listing it takes the port's undefined count 318 -> 320: it opens `CEffectComponent::CEffectComponent(CInputStream&)`, `CPASDatabase::CPASDatabase(CInputStream&)` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CCharacterSet.cpp":
        "retail 0x80293C98, 0x3AC = 940 bytes, NonMatching, 12 functions. Listing it takes the port's undefined count 318 -> 319: it opens `CCharacterInfo::CCharacterInfo(CInputStream&)` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CAnimPOIData.cpp":
        "retail 0x802A0B38, 0x3F4 = 1012 bytes, MatchingFor, 100.00% matched, 9 functions. Listing it takes the port's undefined count 318 -> 320: it opens `CBoolPOINode::CBoolPOINode(CInputStream&)`, `CInt32POINode::CInt32POINode(CInputStream&)` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CMetaAnimBlend.cpp":
        "retail 0x80294044, 0x4D0 = 1232 bytes, MatchingFor, 100.00% matched, 7 functions. Listing it takes the port's undefined count 318 -> 330: it opens `CAnimTreeBlend::CreatePrimitiveName(rstl::ncrc_ptr<CAnimTreeNode>…`, `CAnimTreeTweenBase::CAnimTreeTweenBase(bool, rstl::ncrc_ptr<CAnim…`, `CAnimTreeTweenBase::kBlendRoot_Offset`, 9 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CMetaAnimPhaseBlend.cpp":
        "retail 0x80294788, 0x790 = 1936 bytes, MatchingFor, 100.00% matched, 7 functions. Listing it takes the port's undefined count 318 -> 335: it opens `CAnimTreeBlend::CreatePrimitiveName(rstl::ncrc_ptr<CAnimTreeNode>…`, `CAnimTreeNode::CAnimTreeNode(rstl::basic_string<char, rstl::char_…`, `CAnimTreeNode::~CAnimTreeNode()`, 14 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CMetaAnimRandom.cpp":
        "retail 0x80295428, 0x6F8 = 1784 bytes, NonMatching, 84.62% matched, 13 functions. Listing it takes the port's undefined count 318 -> 322: it opens `CMetaAnimFactory::CreateMetaAnim(CInputStream&)`, `IMetaAnim::GetAnimationTree(CAnimSysContext const&, CMetaAnimTree…`, `IMetaAnim::PutTo(COutputStream&) const`, 1 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CMetaAnimSequence.cpp":
        "retail 0x80295B20, 0x4B8 = 1208 bytes, NonMatching, 85.71% matched, 7 functions. Listing it takes the port's undefined count 318 -> 325: it opens `CAnimTreeSequence::CAnimTreeSequence(rstl::vector<rstl::rc_ptr<IM…`, `CAnimTreeSequence::CreatePrimitiveName(rstl::vector<rstl::basic_s…`, `CMetaAnimFactory::CreateMetaAnim(CInputStream&)`, 4 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CMetaAnimFactory.cpp":
        "retail 0x80294514, 0x274 = 628 bytes, MatchingFor, 100.00% matched, 1 functions. Listing it takes the port's undefined count 318 -> 324: it opens `CMetaAnimBlend::CMetaAnimBlend(CInputStream&)`, `CMetaAnimPhaseBlend::CMetaAnimPhaseBlend(CInputStream&)`, `CMetaAnimRandom::CMetaAnimRandom(CInputStream&)`, 3 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CMetaAnimPlay.cpp":
        "retail 0x80294F18, 0x510 = 1296 bytes, MatchingFor, 100.00% matched, 7 functions. Listing it takes the port's undefined count 318 -> 325: it opens `CAllFormatsAnimSource::GetNewReader(TLockedToken<CAllFormatsAnimS…`, `CAnimTreeNode::CAnimTreeNode(rstl::basic_string<char, rstl::char_…`, `CMetaAnimTreeBuildOrders::PreAdvanceForAll(CPreAdvanceIndicator c…`, 4 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CMetaTransFactory.cpp":
        "retail 0x80295FD8, 0x1F8 = 504 bytes, MatchingFor, 100.00% matched, 1 functions. Listing it takes the port's undefined count 318 -> 322: it opens `CMetaTransMetaAnim::CMetaTransMetaAnim(CInputStream&)`, `CMetaTransPhaseTrans::CMetaTransPhaseTrans(CInputStream&)`, `CMetaTransTrans::CMetaTransTrans(CInputStream&)`, 1 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CMetaTransMetaAnim.cpp":
        "retail 0x802961D0, 0x23C = 572 bytes, MatchingFor, 100.00% matched, 5 functions. Listing it takes the port's undefined count 318 -> 326: it opens `CAnimTreeLoopIn::CAnimTreeLoopIn(rstl::ncrc_ptr<CAnimTreeNode> co…`, `CAnimTreeLoopIn::CreatePrimitiveName(rstl::ncrc_ptr<CAnimTreeNode…`, `CAnimTreeNode::~CAnimTreeNode()`, 5 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CMetaTransPhaseTrans.cpp":
        "retail 0x8029640C, 0x8A0 = 2208 bytes, NonMatching, 85.71% matched, 7 functions. Listing it takes the port's undefined count 318 -> 326: it opens `CAnimTreeNode::CAnimTreeNode(rstl::basic_string<char, rstl::char_…`, `CAnimTreeNode::~CAnimTreeNode()`, `CAnimTreeTimeScale::CreatePrimitiveName(rstl::ncrc_ptr<CAnimTreeN…`, 5 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CMetaTransTrans.cpp":
        "retail 0x80296D38, 0x2E8 = 744 bytes, MatchingFor, 100.00% matched, 5 functions. Listing it takes the port's undefined count 318 -> 320: it opens `CAnimTreeTransition::CAnimTreeTransition(bool, rstl::ncrc_ptr<CAn…`, `CAnimTreeTransition::CreatePrimitiveName(rstl::ncrc_ptr<CAnimTree…` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CTransition.cpp":
        "retail 0x8029A340, 0x98 = 152 bytes, MatchingFor, 100.00% matched, 1 functions. Listing it takes the port's undefined count 318 -> 319: it opens `CMetaTransFactory::CreateMetaTrans(CInputStream&)` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CAllFormatsAnimSource.cpp":
        "retail 0x802B2F74, 0x6EC = 1772 bytes, MatchingFor, 100.00% matched, 11 functions. Listing it takes the port's undefined count 318 -> 324: it opens `CAnimSource::CAnimSource(CInputStream&)`, `CAnimSource::~CAnimSource()`, `CAnimSourceReader::CAnimSourceReader(TSubAnimTypeToken<CAnimSourc…`, 3 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CFBStreamedAnimReader.cpp":
        "retail 0x802ACD44, 0x36B0 = 14000 bytes, NonMatching, 13.73% matched, 51 functions. Listing it takes the port's undefined count 318 -> 334: it opens `CAnimMathUtils::Slerp(CQuaternion const&, CQuaternion const&, flo…`, `CAnimSourceReaderBase::PostConstruct(CCharAnimTime const&)`, `CAnimSourceReaderBase::UpdatePOIStates()`, 13 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CFBStreamedCompression.cpp":
        "retail 0x802B03F4, 0xBE8 = 3048 bytes, NonMatching, 7.14% matched, 14 functions. Listing it takes the port's undefined count 318 -> 322: it opens `CFBStreamedAnimReaderTotals::CFBStreamedAnimReaderTotals(CFBStrea…`, `CFBStreamedAnimReaderTotals::CalculateDown()`, `CFBStreamedAnimReaderTotals::IncrementInto(CBitLevelLoader<CMemor…`, 1 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Animation/CPASDatabase.cpp":
        "retail 0x80298660, 0xAB4 = 2740 bytes, NonMatching, 100.00% matched, 17 functions. Listing it takes the port's undefined count 318 -> 320: it opens `CPASAnimState::CPASAnimState(CInputStream&)`, `CPASAnimState::CPASAnimState(int)`, `CPASAnimState::FindBestAnimation(rstl::reserved_vector<CPASAnimPa…`, and closes `CPASDatabase::FindBestAnimation(CPASAnimParmData const&, int) con…`. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Particles/CColorElement.cpp":
        "retail 0x802CEE6C, 0x1E38 = 7736 bytes, NonMatching, 97.78% matched, 45 functions. Listing it takes the port's undefined count 318 -> 323: it opens `CParticleDataFactory::GetRealElement(CInputStream&)`, `CParticleGlobals::mCurrentParticle`, `CParticleGlobals::mEmitterTime`, 2 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Particles/CParticleElectricDataFactory.cpp":
        "retail 0x8031AD0C, 0x9B0 = 2480 bytes, MatchingFor, 100.00% matched, 8 functions. Listing it takes the port's undefined count 318 -> 329: it opens `CElectricDescription::CElectricDescription()`, `CElectricDescription::~CElectricDescription()`, `CParticleDataFactory::GetBitflag(CInputStream&)`, 8 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Particles/CParticleElectric.cpp":
        "retail 0x8031B6BC, 0x4D20 = 19744 bytes, NonMatching, 91.78% matched, 73 functions. Listing it takes the port's undefined count 318 -> 337: it opens `CElementGen::EndLifetime()`, `CElementGen::sMoveRedToAlphaBuffer`, `CGraphics::DisableAllLights()`, 17 more, and closes `CParticleElectric::CParticleElectric(TToken<CElectricDescription>)`. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Particles/CElementGen.cpp":
        "retail 0x802D0CA4, 0xBE1C = 48668 bytes, NonMatching at 50.96%, 104 functions. Listing it would also make PortLinkStubs.cpp's stub_16 and stub_17 duplicates (they stand in for CElementGen's constructor and SetGlobalOrientAndTrans, PortLinkStubs.cpp:100-105), so the stubs would have to go with it. It is out on the link count first: 318 -> 370, opening 52 symbols nothing defines (CParticleGen's vtable and the CElement* accessors) and closing none.",
    "src/Kyoto/Particles/CParticleSwooshDataFactory.cpp":
        "retail 0x802ED1D0, 0x878 = 2168 bytes, MatchingFor, 100.00% matched, 8 functions. Listing it takes the port's undefined count 318 -> 329: it opens `CParticleDataFactory::GetBitflag(CInputStream&)`, `CParticleDataFactory::GetBool(CInputStream&)`, `CParticleDataFactory::GetClassID(CInputStream&)`, 8 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Particles/CRealElement.cpp":
        "retail 0x802EDA48, 0x4D90 = 19856 bytes, NonMatching, 99.34% matched, 151 functions. Listing it takes the port's undefined count 318 -> 331: it opens `CElementGen::GetExternalVar(int) const`, `CMath::Noise1d(float)`, `CMath::Noise2d(float, float)`, 10 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Particles/CVectorElement.cpp":
        "retail 0x802F3E30, 0x3694 = 13972 bytes, NonMatching, 98.91% matched, 92 functions. Listing it takes the port's undefined count 318 -> 325: it opens `CParticleDataFactory::GetRealElement(CInputStream&)`, `CParticleGlobals::mCurrentParticle`, `CParticleGlobals::mCurrentParticleSystem`, 4 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Audio/DolphinCAudioGroupSet.cpp":
        "retail 0x80307030, 0x7DC = 2012 bytes, NonMatching at 87.50%, 8 functions. 2 compile errors: `reinterpret_cast<uint>` of a `uchar*` (line 72) loses precision, and `__alloca` (line 95) is a CodeWarrior builtin with no host declaration - `alloca` is the spelling, and changing it changes the frame MWCC builds for this unit.",
    "src/Kyoto/Audio/DolphinCAudioSys.cpp":
        "retail 0x8030780C, 0x18FC = 6396 bytes, MatchingFor, 100.00% matched, 60 functions - and the single largest closure in this batch: listing it would close 16 of the port's 318 undefined (CAudioSys::TrkSetState, TrkNextTrack, TrkSetVolume, SysSetVolume, S3dAddEmitterParaEx, S3dAddListener, S3dCheckEmitter, S3dEmitterVoiceID and the rest). It is still out, because it is a duplicate of 24 symbols src/MetroidPrime/PortAudio.cpp defines, and that file is the port's own audio stack with intentional host behaviour: its bodies are the Aurora/DTK calls the SDK's platform/sdk_stubs.cpp makes into no-ops, not retail's, which is the arrangement docs/research/audio_stack.md describes and the same one the eight CAudioSys units already in this list are excluded for. The closure becomes available when the port's audio is reworked onto these bodies, not by listing this unit beside them.",
    "src/Kyoto/Audio/CDSPStreamManager.cpp":
        "retail 0x8033B940, 0x19AC = 6572 bytes, NonMatching at 68.97%, 29 functions. One compile error, and it is the interesting one: line 137 passes `reinterpret_cast<u32>(this)` as the opaque user context to `sndStreamAllocEx`, which loses precision on a 64-bit host. The fix is not a cast - the host's stream manager has to carry a real pointer in a wider parameter, which is a change to platform/sdk_stubs.cpp's `sndStreamAllocEx` signature and to the callback that casts it back, not a hunk in this file. The port's own audio path is src/MetroidPrime/PortAudio.cpp; see docs/research/audio_stack.md.",
    "src/Kyoto/Text/CRasterFont.cpp":
        "retail 0x802B4FB0, 0x1B18 = 6936 bytes, NonMatching, 97.22% matched, 36 functions. Listing it takes the port's undefined count 318 -> 323: it opens `CGraphicsPalette::CGraphicsPalette(EPaletteFormat, int)`, `CGraphicsPalette::UnLock()`, `CTextRenderBuffer::AddCharacter(CVector2i const&, short, unsigned…`, 2 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Text/CTextExecuteBuffer.cpp":
        "retail 0x802B6D90, 0x2340 = 9024 bytes, NonMatching, 26.09% matched, 46 functions. Listing it takes the port's undefined count 318 -> 338: it opens `CBlockInstruction::TestLargestFont(int, int, int)`, `CLineInstruction::GetHeight() const`, `CLineInstruction::TestLargestFont(int, int, int)`, 17 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Text/CGuiTextSupport.cpp":
        "retail 0x8027B184, 0x262C = 9772 bytes, NonMatching, 27.85% matched, 79 functions. Listing it takes the port's undefined count 318 -> 336: it opens `CBasics::Stringize(char const*, ...)`, `CGraphics::mModelMatrix`, `CRasterFont::IsFinishedLoading()`, 15 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Text/CTextParser.cpp":
        "retail 0x802B9368, 0x1478 = 5240 bytes, NonMatching, 53.33% matched, 15 functions. Listing it takes the port's undefined count 318 -> 319: it opens `CTextExecuteBuffer::AddString(wchar_t const*, int)` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Particles/CIntElement.cpp":
        "retail 0x802DCAC0, 0x2534 = 9524 bytes, NonMatching, 98.61% matched, 72 functions. Listing it takes the port's undefined count 318 -> 325: it opens `CParticleDataFactory::GetRealElement(CInputStream&)`, `CParticleGlobals::mCurrentParticle`, `CParticleGlobals::mCurrentParticleSystem`, 4 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Particles/CParticleDataFactory.cpp":
        "retail 0x802E1418, 0x6C9C = 27804 bytes, Matching, 100.00% matched, 38 functions. Listing it takes the port's undefined count 318 -> 472: it opens `CCEConstant::CCEConstant(CRealElement*, CRealElement*, CRealEleme…`, `CCEFade::CCEFade(CColorElement*, CColorElement*, CRealElement*)`, `CCEFadeEnd::CCEFadeEnd(CColorElement*, CColorElement*, CRealEleme…`, 153 more, and closes `IElement::CElementAllocator::Alloc(unsigned long, char const*, ch…`, `IElement::CElementAllocator::Free(void*, unsigned long)`. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Particles/CParticleGen.cpp":
        "retail 0x802E80B4, 0xE4 = 228 bytes, NonMatching, 100.00% matched, 3 functions. Listing it takes the port's undefined count 318 -> 320: it opens `CParticleGen::sDrawFlags`, `CParticleGen::sDrawMask` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Particles/CParticleSwoosh.cpp":
        "retail 0x802E82C4, 0x4F0C = 20236 bytes, NonMatching, 94.64% matched, 56 functions. Listing it takes the port's undefined count 318 -> 337: it opens `CElementGen::sMoveRedToAlphaBuffer`, `CGraphics::DisableAllLights()`, `CGraphics::SetBlendMode(ERglBlendMode, ERglBlendFactor, ERglBlend…`, 17 more, and closes `CParticleSwoosh::CParticleSwoosh(TToken<CSwooshDescription>, int)`. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Audio/CMidiManager.cpp":
        "retail 0x80314DC4, 0x8F4 = 2292 bytes, MatchingFor, 100.00% matched, 21 functions. Listing it takes the port's undefined count 318 -> 321: it opens `CAudioSys::SeqPlayEx(unsigned short, unsigned short, void*, SND_P…`, `CAudioSys::SeqStop(unsigned int)`, `CAudioSys::SeqVolume(unsigned char, unsigned short, unsigned int,…` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Text/CTextRenderBuffer.cpp":
        "retail 0x80315B3C, 0x2020 = 8224 bytes, NonMatching, 100.00% matched, 24 functions. Listing it takes the port's undefined count 318 -> 324: it opens `CGraphicsPalette::CGraphicsPalette(EPaletteFormat, int)`, `CGraphicsPalette::Load() const`, `CGraphicsPalette::UnLock()`, 3 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Text/CFontRenderState.cpp":
        "retail 0x802B3CB8, 0xC5C = 3164 bytes, NonMatching, 100.00% matched, 25 functions. Listing it takes the port's undefined count 318 -> 322: it opens `CDrawStringOptions::CDrawStringOptions()`, `CRasterFont::GetMode() const`, `CSaveableState::CSaveableState()`, 1 more and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Text/CWordInstruction.cpp":
        "retail 0x802BA95C, 0x1F0 = 496 bytes, MatchingFor, 100.00% matched, 4 functions. Listing it takes the port's undefined count 318 -> 321: it opens `CRasterFont::DrawSpace(CDrawStringOptions const&, int, int, int&,…`, `CRasterFont::GetCarriageAdvance() const`, `CRasterFont::GetSize(CDrawStringOptions const&, int&, int&, wchar…` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Text/CTextInstruction.cpp":
        "retail 0x802B90D0, 0x298 = 664 bytes, MatchingFor, 100.00% matched, 4 functions. Listing it takes the port's undefined count 318 -> 320: it opens `CLineInstruction::GetBaseline() const`, `CRasterFont::DrawString(CDrawStringOptions const&, int, int, int&…` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Text/CFontInstruction.cpp":
        "retail 0x802B3AD0, 0x1E8 = 488 bytes, MatchingFor, 100.00% matched, 5 functions. Listing it takes the port's undefined count 318 -> 320: it opens `CFontRenderState::RefreshPalette()`, `CTextRenderBuffer::AddFontChange(TToken<CRasterFont> const&)` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Text/CImageInstruction.cpp":
        "retail 0x803158D8, 0x264 = 612 bytes, MatchingFor, 100.00% matched, 5 functions. Listing it takes the port's undefined count 318 -> 321: it opens `CFontImageDef::GetWidth() const`, `CLineInstruction::GetBaseline() const`, `CTextRenderBuffer::AddImage(CVector2i const&, CFontImageDef const…` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Text/CColorInstruction.cpp":
        "retail 0x802B38C0, 0xB8 = 184 bytes, MatchingFor, 100.00% matched, 3 functions. Listing it takes the port's undefined count 318 -> 319: it opens `CFontRenderState::SetColor(EColorType, CTextColor const&)` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Text/CColorOverrideInstruction.cpp":
        "retail 0x802B3978, 0xE8 = 232 bytes, MatchingFor, 100.00% matched, 3 functions. Listing it takes the port's undefined count 318 -> 319: it opens `CFontRenderState::ConvertToTextureSpace(CTextColor const&) const` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Text/CRemoveColorOverrideInstruction.cpp":
        "retail 0x802B6AC8, 0xBC = 188 bytes, MatchingFor, 100.00% matched, 3 functions. Listing it takes the port's undefined count 318 -> 319: it opens `CFontRenderState::RefreshPalette()` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Text/CPushStateInstruction.cpp":
        "retail 0x802B4F04, 0xAC = 172 bytes, MatchingFor, 100.00% matched, 3 functions. Listing it takes the port's undefined count 318 -> 319: it opens `CFontRenderState::PushState()` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Text/CPopStateInstruction.cpp":
        "retail 0x802B4E1C, 0xE8 = 232 bytes, MatchingFor, 100.00% matched, 3 functions. Listing it takes the port's undefined count 318 -> 320: it opens `CFontRenderState::PopState()`, `CTextRenderBuffer::AddFontChange(TToken<CRasterFont> const&)` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Kyoto/Text/CSaveableState.cpp":
        "retail 0x802B6B84, 0x20C = 524 bytes, MatchingFor, 100.00% matched, 3 functions. Listing it takes the port's undefined count 318 -> 320: it opens `CDrawStringOptions::CDrawStringOptions()`, `CRasterFont::IsFinishedLoading()` and closes none. Measured with tools/link_check.sh's own method - nm over the port's real link inputs, which reproduces the recorded 318 exactly - because a per-unit figure for 109 units costs more than one build.",
    "src/Dolphin/os/__ppc_eabi_init.cpp":
        "PowerPC EABI by name: include/dolphin/__ppc_eabi_init.h declares __ppc_eabi_init and friends as `asm('section .init')`-style inline asm and typedefs BOOL, which collides with the host's include/dolphin/types.h:43 (`conflicting declaration 'typedef bool BOOL'`) before the file's first statement is parsed. Same class as src/Runtime/__init_cpp_exceptions.cpp, which is in this list for the same reason: a target that has no host equivalent.",
    # --- the 2026-09-28 upstream merge's game-wave units: the 92 of the 129
    # configure.py declares under src/MetroidPrime, src/Collision, src/GuiSys,
    # src/WorldFormat, src/Weapons and src/MetaRender that the port does not
    # compile. 37 are listed in files.cmake (the block at the end of that file) and
    # the set as a whole is **net -4** on the port's link: 318 -> 314, so it frees
    # four of the undefined symbols rather than spending any. The 92 below are
    # the ones that are out, each with what it costs. Four further entries are
    # host sources the port carried before upstream had the real unit, now
    # superseded and dropped from files.cmake. Every "318 ->" figure is the unit
    # measured on its own against the port's link with none of this batch listed -
    # tools/link_check.sh's own method, nm over the port's real link inputs, which
    # reproduces the recorded 318 symbol for symbol - because 92 separate builds is
    # not a measurement anyone can afford; the listed 37 are a set, not 37
    # independent decisions, and their joint figure is the 314 above.
    "src/Collision/COBBox.cpp":
        "Listing it takes the port's undefined count 318 -> 320: it opens 2 symbol(s) nothing defines (CollisionUtil::RayAABoxIntersection(CMRay con…, CMRay::GetInvUnscaledTransformRay(CTransform4…) and closes 0. 0x80289A90..0x8028AF74, 0x5348 = 21320 bytes, NonMatching, 25.28% matched, 10 functions",
    "src/Collision/CollisionUtil.cpp":
        "Listing it takes the port's undefined count 318 -> 321: it opens 3 symbol(s) nothing defines (CCollisionInfo::CCollisionInfo(CAABox const&,…, CCollisionInfo::CCollisionInfo(CVector3f cons…, CMRay::CMRay(CVector3f const&, CVector3f cons…) and closes 0. 0x8028385C..0x80288300, 0x19108 = 102664 bytes, NonMatching, 17.42% matched, 29 functions",
    "src/GuiSys/CGuiFrame.cpp":
        "Listing it takes the port's undefined count 318 -> 339: it opens 21 symbol(s) nothing defines (CGuiWidget::FindWidget(short), CGuiWidget::RecalcWidgetColor(ETraversalMode), CGuiWidget::DispatchInitialize() and 18 more) and closes 0. 0x80275278..0x802769DC, 0x5988 = 22920 bytes, NonMatching, 33.13% matched, 39 functions",
    "src/GuiSys/CGuiFrameFactory.cpp":
        "Listing it takes the port's undefined count 318 -> 320: it opens 2 symbol(s) nothing defines (CGuiFrame::CGuiFrame(CInputStream&, CSimplePo…, CGuiFrame::~CGuiFrame()) and closes 0. 0x80274FD4..0x80275278, 0x676 = 1654 bytes, NonMatching, 100.00% matched, 5 functions",
    "src/MetaRender/CCubeRenderer.cpp":
        "Listing it is a multiple definition of 86 symbols the port's link already defines - AllocateRenderer(IObjectStore&, COsContext&, …, CCubeRenderer::BeginLines(int), CCubeRenderer::BeginScene() and 83 more - all of them in Carve8026E7F0.cpp.o, Carve8026EC54.cpp.o, Carve8026ECDC.cpp.o, Carve8026EF24.cpp.o, Carve8026EF54.cpp.o, Carve8026FB80.cpp.o, and 6 more port sources. 0x80262D3C..0x80273FAC, 0x70256 = 459350 bytes, NonMatching, 4.51% matched, 217 functions. And it is out on the count as well: with the port's own copies still in the link the alternative order was measured too, and listing it instead takes the port's undefined count 318 -> 346 - it opens 28 symbol(s) nothing defines (fn_802729B4(), fn_802729C0(), CCubeModel::SetDrawingOccluders(bool) and 25 more) and closes 0.",
    "src/MetroidPrime/BodyState/CBSHurled.cpp":
        "Listing it takes the port's undefined count 318 -> 330: it opens 12 symbol(s) nothing defines (CPatterned* TCastToPtr<CPatterned>(CEntity*), CPhysicsActor* TCastToPtr<CPhysicsActor>(CEnt…, CBodyController::SetFallState(pas::EFallState) and 9 more) and closes 0. 0x800FAFE8..0x800FBF68, 0x3968 = 14696 bytes, NonMatching, 6.96% matched, 13 functions",
    "src/MetroidPrime/BodyState/CBSJump.cpp":
        "Listing it takes the port's undefined count 318 -> 327: it opens 9 symbol(s) nothing defines (CPatterned* TCastToPtr<CPatterned>(CEntity*), CPhysicsActor* TCastToPtr<CPhysicsActor>(CEnt…, CBodyController::FaceDirection(CVector3f cons… and 6 more) and closes 0. 0x800FBF68..0x800FD21C, 0x4788 = 18312 bytes, NonMatching, 7.94% matched, 16 functions",
    "src/MetroidPrime/BodyState/CBSLocomotion.cpp":
        "Listing it takes the port's undefined count 318 -> 322: it opens 4 symbol(s) nothing defines (CPhysicsActor* TCastToPtr<CPhysicsActor>(CEnt…, CBodyController::FaceDirection(CVector3f cons…, CBodyController::SetCurrentAnimation(CAnimPla… and 1 more) and closes 0. 0x800F2BC4..0x800F5004, 0x9280 = 37504 bytes, NonMatching, 14.35% matched, 49 functions",
    "src/MetroidPrime/BodyState/CBSTurn.cpp":
        "Listing it takes the port's undefined count 318 -> 329: it opens 11 symbol(s) nothing defines (CPatterned* TCastToPtr<CPatterned>(CEntity*), CPhysicsActor* TCastToPtr<CPhysicsActor>(CEnt…, CBodyController::FaceDirection(CVector3f cons… and 8 more) and closes 0. 0x800F5430..0x800F649C, 0x4204 = 16900 bytes, NonMatching, 27.31% matched, 15 functions",
    "src/MetroidPrime/BodyState/CBSWallHang.cpp":
        "Listing it takes the port's undefined count 318 -> 327: it opens 9 symbol(s) nothing defines (CPatterned* TCastToPtr<CPatterned>(CEntity*), CPhysicsActor* TCastToPtr<CPhysicsActor>(CEnt…, CActor* TCastToPtr<CActor>(CEntity*) and 6 more) and closes 0. 0x8012CD10..0x8012DDFC, 0x4332 = 17202 bytes, NonMatching, 14.50% matched, 15 functions",
    "src/MetroidPrime/BodyState/CBodyController.cpp":
        "Listing it takes the port's undefined count 318 -> 328: it opens 10 symbol(s) nothing defines (CPhysicsActor* TCastToPtr<CPhysicsActor>(CEnt…, CPhysicsActor* TCastToPtr<CPhysicsActor>(CEnt…, CBodyStateInfo::GetCurrentState() and 7 more) and closes 0. 0x800F649C..0x800F794C, 0x5296 = 21142 bytes, NonMatching, 60.57% matched, 31 functions",
    "src/MetroidPrime/BodyState/CBodyStateInfo.cpp":
        "Listing it is a multiple definition of 11 symbols the port's link already defines - fn_800F1234, fn_800F123C, fn_800F1244 and 8 more - all of them in Carve800F1234.c.o, Carve800F1264.c.o. 0x800EF908..0x800F128C, 0x6532 = 25906 bytes, NonMatching, 26.15% matched, 52 functions. And it is out on the count as well: with the port's own copies still in the link the alternative order was measured too, and listing it instead takes the port's undefined count 318 -> 372 - it opens 55 symbol(s) nothing defines (CABSFlinch::CABSFlinch(), CABSFlinch::~CABSFlinch(), CBSGenerate::CBSGenerate() and 52 more) and closes 1.",
    "src/MetroidPrime/CAutoMapper.cpp":
        "Listing it takes the port's undefined count 318 -> 426: it opens 108 symbol(s) nothing defines (CGameState::StateForWorld(unsigned int), CGuiWidget::SetVisibility(bool, ETraversalMod…, CGuiWidget::SetColor(CColor const&) and 105 more) and closes 0. 0x80086D9C..0x80092310, 0x46452 = 287826 bytes, NonMatching, 30.17% matched, 100 functions",
    "src/MetroidPrime/CBoneTracking.cpp":
        "Listing it takes the port's undefined count 318 -> 321: it opens 3 symbol(s) nothing defines (CPatterned* TCastToPtr<CPatterned>(CEntity&), CActor* TCastToPtr<CActor>(CEntity*), CBodyStateInfo::ApplyHeadTracking() const) and closes 0. 0x80117614..0x80118294, 0x3200 = 12800 bytes, NonMatching, 19.38% matched, 12 functions",
    "src/MetroidPrime/CCredits.cpp":
        "Listing it takes the port's undefined count 318 -> 388: it opens 70 symbol(s) nothing defines (CStringTable::~CStringTable(), CGuiTextSupport::SetFontColor(CColor const&), CGuiTextSupport::SetOutlineColor(CColor const… and 67 more) and closes 0. 0x8001FF7C..0x80023FFC, 0x16512 = 91410 bytes, MatchingFor, 100.00% matched, 49 functions",
    "src/MetroidPrime/CDecalManager.cpp":
        "Listing it takes the port's undefined count 318 -> 326: it opens 8 symbol(s) nothing defines (CollisionUtil::TriBoxOverlap(CVector3f const&…, CDisplayListReader::CDisplayListReader(void c…, CDecal::Update(float) and 5 more) and closes 0. 0x800E6CEC..0x800E8C8C, 0x8096 = 32918 bytes, NonMatching, 13.44% matched, 65 functions",
    "src/MetroidPrime/CErrorOutputWindow.cpp":
        "Listing it takes the port's undefined count 318 -> 333: it opens 16 symbol(s) nothing defines (CMemoryCardSys::mIsCardBusy, CTextRenderBuffer::~CTextRenderBuffer(), CTextExecuteBuffer::BeginBlock(int, int, int,… and 13 more) and closes 1. 0x80180E94..0x80181750, 0x2236 = 8758 bytes, NonMatching, 49.02% matched, 9 functions",
    "src/MetroidPrime/CEulerAngles.cpp":
        "Listing it takes the port's undefined count 318 -> 319: it opens 1 symbol(s) nothing defines (msl_sqrtf__Ff) and closes 0. 0x8001D430..0x8001D7B0, 0x896 = 2198 bytes, NonMatching, 3.57% matched, 6 functions",
    "src/MetroidPrime/CGameArea.cpp":
        "Listing it is a multiple definition of 6 symbols the port's link already defines - CGameArea::SetAreaAttributes(CScriptAreaPrope…, CGameArea::TryTakingOutOfARAM(), CGameArea::CAreaFog::CAreaFog() and 3 more - all of them in CGameAreaCAreaFog.cpp.o, CGameAreaHasPendingLayerLoads.cpp.o, CGameAreaSetAreaAttributes.cpp.o, PortLinkStubs.cpp.o. 0x800536A8..0x800609B4, 0x54028 = 344104 bytes, NonMatching, 7.23% matched, 304 functions. And it is out on the count as well: with the port's own copies still in the link the alternative order was measured too, and listing it instead takes the port's undefined count 318 -> 324 - it opens 7 symbol(s) nothing defines (CRELFileToken::~CRELFileToken(), CGameArea::CPostConstructed::~CPostConstructe…, rstl::vector<rstl::pair<unsigned int, unsigne… and 4 more) and closes 1.",
    "src/MetroidPrime/CGameCollision.cpp":
        "Listing it is a multiple definition of 1 symbol the port's link already defines - CGameCollision::RayWorldIntersection(CStateMa… - all of them in CGameCollisionRayWorldIntersection.cpp.o. 0x80123510..0x801285DC, 0x20684 = 132740 bytes, NonMatching, 1.08% matched, 52 functions. And it is out on the count as well: with the port's own copies still in the link the alternative order was measured too, and listing it instead takes the port's undefined count 318 -> 347 - it opens 31 symbol(s) nothing defines (CPhysicsActor* TCastToPtr<CPhysicsActor>(CEnt…, CScriptPlatform* TCastToPtr<CScriptPlatform>(…, CCollisionInfo::Swap() and 28 more) and closes 2.",
    "src/MetroidPrime/CGameHint.cpp":
        "Listing it takes the port's undefined count 318 -> 322: it opens 4 symbol(s) nothing defines (CModelData::CModelDataNull(), CActorParameters::CActorParameters(), CGameCamera::Player(CStateManager&) const and 1 more) and closes 0. 0x8022D1EC..0x8022D548, 0x860 = 2144 bytes, NonMatching, 11.16% matched, 3 functions",
    "src/MetroidPrime/CGameLight.cpp":
        "Listing it takes the port's undefined count 318 -> 323: it opens 5 symbol(s) nothing defines (CEntityInfo::CEntityInfo(CEntityInfo const&), CEntityInfo::~CEntityInfo(), CActorParameters::CActorParameters() and 2 more) and closes 0. 0x800A5740..0x800A5A34, 0x756 = 1878 bytes, NonMatching, 46.56% matched, 5 functions",
    "src/MetroidPrime/CGroundMovement.cpp":
        "Listing it takes the port's undefined count 318 -> 327: it opens 9 symbol(s) nothing defines (CollisionUtil::FilterByClosestNormal(CVector3…, CCollisionInfo::CCollisionInfo(), CGameCollision::DetectCollision_Cached(CState… and 6 more) and closes 0. 0x8012878C..0x8012CA24, 0x17048 = 94280 bytes, NonMatching, 6.97% matched, 29 functions",
    "src/MetroidPrime/CIOWinManager.cpp":
        "Listing it is a multiple definition of 9 symbols the port's link already defines - CIOWinManager::IOWinPQNode::IOWinPQNode(rstl:…, CIOWinManager::IOWinPQNode::IOWinPQNode(rstl:…, CIOWinManager::PumpMessages(CArchitectureQueu… and 6 more - all of them in CIOWinManagerAddIOWin.cpp.o, CIOWinManagerCtor.cpp.o, CIOWinManagerPumpMessages.cpp.o, CIOWinManagerRemoveAllIOWins.cpp.o. 0x80048F78..0x80049E10, 0x3736 = 14134 bytes, NonMatching, 100.00% matched, 19 functions. And it is out on the count as well: with the port's own copies still in the link the alternative order was measured too, and listing it instead takes the port's undefined count 318 -> 323 - it opens 5 symbol(s) nothing defines (CIOWinManager::IOWinPQNode::~IOWinPQNode(), MakeMsg::GetParmCreateIOWin(CArchitectureMess…, MakeMsg::GetParmDeleteIOWin(CArchitectureMess… and 2 more) and closes 0.",
    "src/MetroidPrime/CInGameGuiManager.cpp":
        "Listing it takes the port's undefined count 318 -> 322: it opens 4 symbol(s) nothing defines (CGuiFrameLoader::CreateFrame(), CFaceplateDecoration::CFaceplateDecoration(CS…, CGuiFrame::~CGuiFrame() and 1 more) and closes 0. 0x80222EC4..0x80226B3C, 0x15480 = 87168 bytes, NonMatching, 2.66% matched, 45 functions",
    "src/MetroidPrime/CInputGenerator.cpp":
        "Listing it is a multiple definition of 2 symbols the port's link already defines - CInputGenerator::CInputGenerator(COsContext*,…, CInputGenerator::CInputGenerator(COsContext*,… - all of them in CInputGeneratorCtor.cpp.o. 0x8001D888..0x8001DAF4, 0x620 = 1568 bytes, MatchingFor, 100.00% matched, 2 functions. And it is out on the count as well: with the port's own copies still in the link the alternative order was measured too, and listing it instead takes the port's undefined count 318 -> 319 - it opens 2 symbol(s) nothing defines (MakeMsg::CreateUserInput(EArchMsgTarget, CFin…, MakeMsg::CreateControllerStatus(EArchMsgTarge…) and closes 1.",
    "src/MetroidPrime/CMainFlow.cpp":
        "retail 0x8001DAF4..0x8001E070, 0x1404 = 5124 bytes, MatchingFor, 100.00% matched, 7 functions. Out for two measured reasons, either of which is enough. (1) It is a multiple definition of ten symbols the port's four CMainFlow* carves already define - CMainFlow::CMainFlow, ~CMainFlow, OnMessage, SetGameState, AdvanceGameState, Draw, GetIsContinueDraw and the three vtable/typeinfo objects - in src/MetroidPrime/CMainFlowCtor.cpp, CMainFlowOnMessage.cpp, CMainFlowAccessors.cpp and CMainFlowDtor.cpp, all of which are still listed. (2) With those four carves dropped the unit is still out on the count: it opens sixteen symbols nothing defines - CMain::StreamNewGameState(bool), CGameState::SetGameMode(CGameMode*), CCredits::CCredits, CPreFrontEnd::CPreFrontEnd(), CMFGameLoader::CMFGameLoader(), CGMSinglePlayer::CGMSinglePlayer(), CAutoSave::CAutoSave(), CHintOptions::EnsureHintNextTime(), CPlayMovie::CPlayMovie(int), fn_80143884(), fn_80143E88(), MakeMsg::CreateCreateIOWin(...), MakeMsg::GetParmNewGameflowState(...) and vtable for CArchMsgParmInt32 - and closes none, so 318 -> 334. It becomes worth listing when CGameState's flow methods, CMain::StreamNewGameState and CCredits have bodies, which is CMain::Initialize in src/MetroidPrime/mainHead.cpp.",
    "src/MetroidPrime/CMapUniverse.cpp":
        "Listing it takes the port's undefined count 318 -> 329: it opens 11 symbol(s) nothing defines (CGameState::StateForWorld(unsigned int), CMapWorldInfo::IsAnythingSet(), CMapArea::CMapAreaSurface::SetupGXMaterial() and 8 more) and closes 0. 0x801545F0..0x801561A0, 0x7088 = 28808 bytes, NonMatching, 59.37% matched, 40 functions",
    "src/MetroidPrime/CMapWorld.cpp":
        "Listing it takes the port's undefined count 318 -> 357: it opens 39 symbol(s) nothing defines (CMemoryDrawEnum::mWorldMemory, CMath::FloorF(float), CMapArea::SetupLighting(CTransform4f const&) and 36 more) and closes 0. 0x80092918..0x80096C94, 0x17276 = 94838 bytes, NonMatching, 31.28% matched, 58 functions",
    "src/MetroidPrime/CMapWorldInfo.cpp":
        "Listing it takes the port's undefined count 318 -> 319: it opens 1 symbol(s) nothing defines (CMemoryCard::GetSaveWorldMemory(unsigned int)…) and closes 0. 0x8010F084..0x80110B18, 0x6804 = 26628 bytes, NonMatching, 61.02% matched, 23 functions",
    "src/MetroidPrime/CMemoryCard.cpp":
        "Listing it takes the port's undefined count 318 -> 321: it opens 3 symbol(s) nothing defines (CDummyWorld::CDummyWorld(unsigned int, bool), CDummyWorld::~CDummyWorld(), CResFactory::GetResourceIdToNameList() const) and closes 0. 0x801767BC..0x8017904C, 0x10384 = 66436 bytes, NonMatching, 8.78% matched, 56 functions",
    "src/MetroidPrime/CMemoryCardDriver.cpp":
        "Listing it takes the port's undefined count 318 -> 333: it opens 15 symbol(s) nothing defines (CGameState::LoadGameFileState(void const*), CMemoryCardSys::FormatCard(CMemoryCardSys::EM…, CMemoryCardSys::GetSerialNo(CMemoryCardSys::E… and 12 more) and closes 0. 0x80179F84..0x8017C548, 0x9668 = 38504 bytes, NonMatching, 13.41% matched, 53 functions",
    "src/MetroidPrime/CParticleGenInfo.cpp":
        "Listing it takes the port's undefined count 318 -> 319: it opens 1 symbol(s) nothing defines (CGameLight::CGameLight(TUniqueId, TAreaId, bo…) and closes 0. 0x800A6184..0x800A6444, 0x704 = 1796 bytes, MatchingFor, 100.00% matched, 3 functions",
    "src/MetroidPrime/CParticleGenInfoGeneric.cpp":
        "Listing it takes the port's undefined count 318 -> 325: it opens 7 symbol(s) nothing defines (CGameLight* TCastToPtr<CGameLight>(CEntity*), _initializeLight(rstl::ncrc_ptr<CParticleGen>…, CGameLight::SetLight(CLight const&) and 4 more) and closes 0. 0x800A5A34..0x800A6184, 0x1872 = 6258 bytes, NonMatching, 71.15% matched, 19 functions",
    "src/MetroidPrime/CPauseScreen.cpp":
        "Listing it takes the port's undefined count 318 -> 333: it opens 15 symbol(s) nothing defines (CActorLights::CActorLights(unsigned int, CVec…, CActorLights::~CActorLights(), CRepeatState::CRepeatState() and 12 more) and closes 0. 0x80201F4C..0x8020DA7C, 0x47920 = 293152 bytes, NonMatching, 0.85% matched, 84 functions",
    "src/MetroidPrime/CProjectedShadow.cpp":
        "Listing it takes the port's undefined count 318 -> 319: it opens 1 symbol(s) nothing defines (CTexture::ScheduleDeletion()) and closes 0. 0x801917F8..0x80192708, 0x3856 = 14422 bytes, NonMatching, 11.93% matched, 8 functions",
    "src/MetroidPrime/CRumbleManager.cpp":
        "retail 0x801A2E44..0x801A38B4, MatchingFor, 100.00% matched, 8 functions. It is a multiple definition of one symbol the port's link already has - `CRumbleManager::StopRumble(short)`, from src/MetroidPrime/CRumbleManagerStopRumble.cpp - and dropping that carve for it does not make it worth listing, because the trade is a missing function for a guest data address: measured on its own it opens `skRumbleFxTable`, a retail .data symbol at 0x8041xxxx that no host build will ever define, and closes nothing else. A net-0 trade that swaps a real function for an unnameable guest address is a regression dressed as a wash, which is the trade tools/check_files_cmake.py's own header warns about when it records lane e6's CModelDataDefaultCtor at net -1. It becomes worth listing when the port has a host skRumbleFxTable.",
    "src/MetroidPrime/CSaveGameScreen.cpp":
        "Listing it takes the port's undefined count 318 -> 332: it opens 16 symbol(s) nothing defines (CMemoryCardDriver::IsCardBusy(EState), CMemoryCardDriver::CopyFileSlot(int, int), CMemoryCardDriver::EraseFileSlot(int) and 13 more) and closes 2. 0x8017C548..0x8017DFBC, 0x6772 = 26482 bytes, NonMatching, 14.35% matched, 24 functions",
    "src/MetroidPrime/CSlideShow.cpp":
        "Listing it takes the port's undefined count 318 -> 322: it opens 4 symbol(s) nothing defines (CGuiTextSupport::Update(float), CGuiTextSupport::~CGuiTextSupport(), CPersistentOptions::FindEnvironmentVariable(c… and 1 more) and closes 0. 0x8018C4A8..0x801917F8, 0x21328 = 135976 bytes, NonMatching, 8.38% matched, 76 functions",
    "src/MetroidPrime/CVisorFlare.cpp":
        "Listing it takes the port's undefined count 318 -> 337: it opens 19 symbol(s) nothing defines (CTexture::GetBitMapData(int), CTexture::ScheduleDeletion(), CTexture::UnLock() and 16 more) and closes 0. 0x801561A0..0x801576FC, 0x5468 = 21608 bytes, NonMatching, 15.36% matched, 12 functions",
    "src/MetroidPrime/CWorld.cpp":
        "Listing it is a multiple definition of 3 symbols the port's link already defines - CWorld::skGlobalEnd, CWorld::skGlobalNonConstEnd, CWorld::TouchSky() const - all of them in CWorldTouchSky.cpp.o, PortGlobals.cpp.o. 0x8004E84C..0x80052880, 0x16436 = 91190 bytes, NonMatching, 56.00% matched, 96 functions. And it is out on the count as well: with the port's own copies still in the link the alternative order was measured too, and listing it instead takes the port's undefined count 318 -> 353 - it opens 36 symbol(s) nothing defines (CGameState::StateForWorld(unsigned int), CGameState::WorldTransitionManager(), CWorldState::GetLayerState() and 33 more) and closes 1.",
    "src/MetroidPrime/CWorldShadow.cpp":
        "Listing it takes the port's undefined count 318 -> 340: it opens 24 symbol(s) nothing defines (CCubeModel::EnableShadowMaps(CTexture const*,…, CCubeModel::DisableShadowMaps(), CCubeModel::SetDrawingOccluders(bool) and 21 more) and closes 2. 0x800E17E4..0x800E24D0, 0x3308 = 13064 bytes, NonMatching, 12.70% matched, 7 functions",
    "src/MetroidPrime/CWorldTransManager.cpp":
        "Listing it takes the port's undefined count 318 -> 328: it opens 10 symbol(s) nothing defines (CModelData::CModelDataNull(), CGameSpline::~CGameSpline(), CMotionSpline::~CMotionSpline() and 7 more) and closes 0. 0x801576FC..0x8015C634, 0x20280 = 131712 bytes, NonMatching, 11.14% matched, 76 functions",
    "src/MetroidPrime/Cameras/CBallCamera.cpp":
        "Listing it takes the port's undefined count 318 -> 387: it opens 69 symbol(s) nothing defines (CPhysicsActor* TCastToPtr<CPhysicsActor>(CEnt…, CCollisionActor* TCastToPtr<CCollisionActor>(…, CPlayer* TCastToPtr<CPlayer>(CEntity*) and 66 more) and closes 0. 0x8019E76C..0x801A8B78, 0x41996 = 268694 bytes, NonMatching, 2.81% matched, 53 functions",
    "src/MetroidPrime/Cameras/CBallCameraTransitionState.cpp":
        "Listing it takes the port's undefined count 318 -> 319: it opens 1 symbol(s) nothing defines (CMotionSpline::CMotionSpline(bool, float, CMo…) and closes 0. 0x801B730C..0x801B73E4, 0x216 = 534 bytes, MatchingFor, 100.00% matched, 2 functions",
    "src/MetroidPrime/Cameras/CCameraFilter.cpp":
        "Listing it takes the port's undefined count 318 -> 337: it opens 19 symbol(s) nothing defines (CGraphics::SetCullMode(ERglCullMode), CGraphics::StreamBegin(ERglPrimitive), CGraphics::SetLineWidth(float, ERglTexOffset) and 16 more) and closes 0. 0x800BDA8C..0x800C02A4, 0x10264 = 66148 bytes, NonMatching, 35.23% matched, 27 functions",
    "src/MetroidPrime/Cameras/CCinematicCamera.cpp":
        "Listing it takes the port's undefined count 318 -> 337: it opens 19 symbol(s) nothing defines (CEntityInfo::~CEntityInfo(), CGameCamera::UnkVtable84(), CGameCamera::UnkVtable88(TUniqueId) and 16 more) and closes 0. 0x801AD944..0x801AE584, 0x3136 = 12598 bytes, NonMatching, 9.57% matched, 9 functions",
    "src/MetroidPrime/Cameras/CFirstPersonCamera.cpp":
        "Listing it takes the port's undefined count 318 -> 328: it opens 10 symbol(s) nothing defines (CEntityInfo::~CEntityInfo(), CGameCamera::ClearFluidList(CStateManager&), CGameCamera::AcceptScriptMsg(CStateManager&, … and 7 more) and closes 0. 0x801AE584..0x801B0500, 0x8060 = 32864 bytes, NonMatching, 4.91% matched, 17 functions",
    "src/MetroidPrime/Cameras/CGameCamera.cpp":
        "Listing it is a multiple definition of 1 symbol the port's link already defines - CGameCamera::SetAspectRatio(float) - all of them in CGameCameraSetAspectRatio.cpp.o. 0x801B0500..0x801B1A18, 0x5400 = 21504 bytes, NonMatching, 17.85% matched, 35 functions. And it is out on the count as well: with the port's own copies still in the link the alternative order was measured too, and listing it instead takes the port's undefined count 318 -> 321 - it opens 4 symbol(s) nothing defines (CActorParameters::CActorParameters(), CGraphics::CalculatePerspectiveMatrix(float, …, CMatrix4f::CMatrix4f(CMatrix4f const&) and 1 more) and closes 1.",
    "src/MetroidPrime/Cameras/CInterpolationCamera.cpp":
        "Listing it takes the port's undefined count 318 -> 342: it opens 25 symbol(s) nothing defines (CEntityInfo::~CEntityInfo(), CGameCamera::UnkVtable84(), CGameCamera::UnkVtable88(TUniqueId) and 22 more) and closes 1. 0x801B1A18..0x801B31A8, 0x6032 = 24626 bytes, NonMatching, 3.45% matched, 14 functions",
    "src/MetroidPrime/Cameras/CPathCamera.cpp":
        "Listing it takes the port's undefined count 318 -> 345: it opens 27 symbol(s) nothing defines (CScriptPathCamera* TCastToPtr<CScriptPathCame…, CEntityInfo::~CEntityInfo(), CGameCamera::UnkVtable84() and 24 more) and closes 0. 0x801B31A8..0x801B4F68, 0x7616 = 30230 bytes, NonMatching, 2.47% matched, 16 functions",
    "src/MetroidPrime/Cameras/CSpindleCamera.cpp":
        "Listing it takes the port's undefined count 318 -> 329: it opens 11 symbol(s) nothing defines (CEntityInfo::~CEntityInfo(), CGameCamera::UnkVtable84(), CGameCamera::UnkVtable88(TUniqueId) and 8 more) and closes 0. 0x801B4F68..0x801B730C, 0x9124 = 37156 bytes, NonMatching, 10.43% matched, 16 functions",
    "src/MetroidPrime/Decode.cpp":
        "Listing it is a multiple definition of 2 symbols the port's link already defines - MakeMsg::CreateFrameEnd(EArchMsgTarget, int c…, MakeMsg::CreateTimerTick(EArchMsgTarget, floa… - all of them in mainMid.cpp.o. 0x800489AC..0x80048F78, 0x1484 = 5252 bytes, MatchingFor, 100.00% matched, 14 functions. And it is out on the count as well: with the port's own copies still in the link the alternative order was measured too, and listing it instead takes the port's undefined count 318 -> 324 - it opens 6 symbol(s) nothing defines (CArchMsgParmNull::CArchMsgParmNull(), CArchMsgParmInt32::CArchMsgParmInt32(int), CArchMsgParmReal32::CArchMsgParmReal32(float) and 3 more) and closes 0.",
    "src/MetroidPrime/Enemies/CKnockBackMgr.cpp":
        "Listing it takes the port's undefined count 318 -> 322: it opens 4 symbol(s) nothing defines (CRuleSetEvaluator::EvaluateRules(), CRuleSetEvaluator::CRuleSetEvaluator(unsigned…, CRuleSetEvaluator::~CRuleSetEvaluator() and 1 more) and closes 0. 0x801BD714..0x801BEDD0, 0x5820 = 22560 bytes, NonMatching, 41.79% matched, 16 functions",
    "src/MetroidPrime/Factories/CAssetFactory.cpp":
        "Listing it is a multiple definition of 7 symbols the port's link already defines - CCharacterFactoryBuilder::CDummyFactory::Buil…, CCharacterFactoryBuilder::CDummyFactory::Canc…, CCharacterFactoryBuilder::CDummyFactory::Buil… and 4 more - all of them in CCharacterFactoryBuilder.cpp.o. 0x80031E60..0x800324A4, 0x1604 = 5636 bytes, MatchingFor, 100.00% matched, 15 functions. And it is out on the count as well: with the port's own copies still in the link the alternative order was measured too, and listing it instead takes the port's undefined count 318 -> 320 - it opens 2 symbol(s) nothing defines (CCharacterFactory::CCharacterFactory(CSimpleP…, CCharacterFactory::~CCharacterFactory()) and closes 0.",
    "src/MetroidPrime/Factories/CCharacterFactory.cpp":
        "Listing it takes the port's undefined count 318 -> 325: it opens 9 symbol(s) nothing defines (IObjFactory::~IObjFactory(), CSkinnedModel::CSkinnedModel(TLockedToken<CMo…, CSkinnedModel::~CSkinnedModel() and 6 more) and closes 2. 0x8002F7A8..0x80031E60, 0x9912 = 39186 bytes, NonMatching, 81.52% matched, 60 functions",
    "src/MetroidPrime/HUD/CHudDecoInterfaceScan.cpp":
        "Listing it takes the port's undefined count 318 -> 321: it opens 3 symbol(s) nothing defines (CActor* TCastToPtr<CActor>(CEntity*), CGuiFrameLoader::~CGuiFrameLoader(), CGuiFrame::~CGuiFrame()) and closes 0. 0x8021BFE0..0x8021DC00, 0x7200 = 29184 bytes, NonMatching, 3.83% matched, 20 functions",
    "src/MetroidPrime/HUD/CSamusHud.cpp":
        "Listing it is a multiple definition of 1 symbol the port's link already defines - CSamusHud::DisplayHudMemo(rstl::basic_string<… - all of them in PortLinkStubs.cpp.o. 0x800609B4..0x8006CB00, 0x49484 = 300164 bytes, NonMatching, 1.58% matched, 88 functions. And it is out on the count as well: with the port's own copies still in the link the alternative order was measured too, and listing it instead takes the port's undefined count 318 -> 338 - it opens 20 symbol(s) nothing defines (CActorLights::CActorLights(unsigned int, CVec…, CActorLights::~CActorLights(), CGuiFrameLoader::CreateFrame() and 17 more) and closes 0.",
    "src/MetroidPrime/PathFinding/CPathFindArea.cpp":
        "Listing it takes the port's undefined count 318 -> 326: it opens 8 symbol(s) nothing defines (CPFOpenList::CPFOpenList(), CPFRegionData::CPFRegionData(), CPFPoint::Fixup(CPFArea&) and 5 more) and closes 0. 0x8013FDB8..0x801419C4, 0x7180 = 29056 bytes, NonMatching, 16.99% matched, 38 functions",
    "src/MetroidPrime/PathFinding/CPathFindSearch.cpp":
        "Listing it takes the port's undefined count 318 -> 330: it opens 12 symbol(s) nothing defines (CPFRegionData::CPFRegionData(), CPFArea::FindRegions(rstl::reserved_vector<CP…, CPFArea::FindClosestRegion(CVector3f const&, … and 9 more) and closes 0. 0x8013CE68..0x8013E93C, 0x6868 = 26728 bytes, NonMatching, 10.19% matched, 17 functions",
    "src/MetroidPrime/Player/CGrappleArm.cpp":
        "Listing it is a multiple definition of 2 symbols the port's link already defines - CGrappleArm::SetStateFlags(unsigned int), CGrappleArm::ReturnToDefault(CStateManager&, … - all of them in CGrappleArmReturnToDefault.cpp.o. 0x801C316C..0x801C6B20, 0x14772 = 83826 bytes, NonMatching, 14.32% matched, 64 functions. And it is out on the count as well: with the port's own copies still in the link the alternative order was measured too, and listing it instead takes the port's undefined count 318 -> 346 - it opens 29 symbol(s) nothing defines (CActor* TCastToPtr<CActor>(CEntity*), CElementGen::ForceParticleCreation(int), NWeaponTypes::lock_tokens(rstl::vector<CToken… and 26 more) and closes 1.",
    "src/MetroidPrime/Player/CScanDisplay.cpp":
        "Listing it takes the port's undefined count 318 -> 320: it opens 2 symbol(s) nothing defines (CScriptPointOfInterest* TCastToPtr<CScriptPoi…, CScannableObjectInfo::GetTotalDownloadTime() …) and closes 0. 0x801123F8..0x80116C74, 0x18556 = 99670 bytes, NonMatching, 3.28% matched, 54 functions",
    "src/MetroidPrime/ScriptObjects/CScriptActor.cpp":
        "Listing it is a multiple definition of 1 symbol the port's link already defines - CScriptActor::CheckActorRenderOnly() const - all of them in CScriptActorCheckActorRenderOnly.cpp.o. 0x8006EB98..0x80070DB4, 0x8732 = 34610 bytes, NonMatching, 15.76% matched, 34 functions. And it is out on the count as well: with the port's own copies still in the link the alternative order was measured too, and listing it instead takes the port's undefined count 318 -> 327 - it opens 9 symbol(s) nothing defines (CScriptTrigger* TCastToPtr<CScriptTrigger>(CE…, CCinematicCamera* TCastToPtr<CCinematicCamera…, CEchoEmitter::TriggerDamageEcho() and 6 more) and closes 0.",
    "src/MetroidPrime/ScriptObjects/CScriptCamera.cpp":
        "Listing it is a multiple definition of 1 symbol the port's link already defines - CScriptCamera::MarkViewed(CStateManager const… - all of them in CScriptCameraMarkViewed.cpp.o. 0x801DEBB4..0x801DF2A0, 0x1772 = 6002 bytes, NonMatching, 25.96% matched, 7 functions. And it is out on the count as well: with the port's own copies still in the link the alternative order was measured too, and listing it instead takes the port's undefined count 318 -> 335 - it opens 17 symbol(s) nothing defines (CScriptTimeKeyframe* TCastToPtr<CScriptTimeKe…, CPlayer* TCastToPtr<CPlayer>(CEntity*), CModelData::CModelDataNull() and 14 more) and closes 0.",
    "src/MetroidPrime/ScriptObjects/CScriptCameraHint.cpp":
        "Listing it takes the port's undefined count 318 -> 330: it opens 12 symbol(s) nothing defines (CScriptPathCamera* TCastToPtr<CScriptPathCame…, CActor* TCastToPtr<CActor>(CEntity*), CPlayer* TCastToPtr<CPlayer>(CEntity*) and 9 more) and closes 0. 0x800B8098..0x800B88D4, 0x2108 = 8456 bytes, NonMatching, 11.57% matched, 6 functions",
    "src/MetroidPrime/ScriptObjects/CScriptCameraShaker.cpp":
        "Listing it takes the port's undefined count 318 -> 319: it opens 1 symbol(s) nothing defines (CScriptCameraShaker::TypesMatch(int) const) and closes 0. 0x800D50B8..0x800D56A8, 0x1520 = 5408 bytes, NonMatching, 16.05% matched, 5 functions",
    "src/MetroidPrime/ScriptObjects/CScriptCameraWaypoint.cpp":
        "Listing it takes the port's undefined count 318 -> 327: it opens 9 symbol(s) nothing defines (CScriptCameraWaypoint* TCastToPtr<CScriptCame…, CScriptWaypoint::AcceptScriptMsg(CStateManage…, CScriptWaypoint::CScriptWaypoint(TUniqueId, r… and 6 more) and closes 0. 0x800A5594..0x800A5740, 0x428 = 1064 bytes, NonMatching, 76.64% matched, 5 functions",
    "src/MetroidPrime/ScriptObjects/CScriptColorModulate.cpp":
        "Listing it takes the port's undefined count 318 -> 322: it opens 4 symbol(s) nothing defines (CActor* TCastToPtr<CActor>(CEntity*), CEntityInfo::~CEntityInfo(), CStateManager::GetObjectByIdFromListAll(TUniq… and 1 more) and closes 0. 0x80152950..0x801545F0, 0x7328 = 29480 bytes, NonMatching, 9.33% matched, 14 functions",
    "src/MetroidPrime/ScriptObjects/CScriptEffect.cpp":
        "Listing it takes the port's undefined count 318 -> 341: it opens 25 symbol(s) nothing defines (CGameLight* TCastToPtr<CGameLight>(CEntity*), CScriptTrigger* TCastToPtr<CScriptTrigger>(CE…, CScriptWaypoint* TCastToPtr<CScriptWaypoint>(… and 22 more) and closes 2. 0x800802D4..0x80082EE8, 0x11284 = 70276 bytes, NonMatching, 5.21% matched, 35 functions",
    "src/MetroidPrime/ScriptObjects/CScriptPathCamera.cpp":
        "Listing it takes the port's undefined count 318 -> 342: it opens 24 symbol(s) nothing defines (CScriptWaypoint* TCastToPtr<CScriptWaypoint>(…, CScriptTimeKeyframe* TCastToPtr<CScriptTimeKe…, CActor* TCastToPtr<CActor>(CEntity*) and 21 more) and closes 0. 0x801E0650..0x801E1338, 0x3304 = 13060 bytes, NonMatching, 16.59% matched, 11 functions",
    "src/MetroidPrime/ScriptObjects/CScriptPlatform.cpp":
        "Listing it takes the port's undefined count 318 -> 321: it opens 3 symbol(s) nothing defines (CActor* TCastToPtr<CActor>(CEntity*), CStateManager::GetObjectByIdFromListAll(TUniq…, CScriptPlatform::TypesMatch(int) const) and closes 0. 0x800A0200..0x800A4850, 0x18000 = 98304 bytes, NonMatching, 2.64% matched, 60 functions",
    "src/MetroidPrime/ScriptObjects/CScriptProjectedShadow.cpp":
        "Listing it takes the port's undefined count 318 -> 324: it opens 6 symbol(s) nothing defines (CActor* TCastToPtr<CActor>(CEntity*), CModelData::CModelDataNull(), CStateManager::GetObjectByIdFromListAll(TUniq… and 3 more) and closes 0. 0x80192864..0x8019312C, 0x2248 = 8776 bytes, NonMatching, 38.26% matched, 7 functions",
    "src/MetroidPrime/ScriptObjects/CScriptRepulsor.cpp":
        "Listing it takes the port's undefined count 318 -> 320: it opens 2 symbol(s) nothing defines (CActorParameters::CActorParameters(), CScriptRepulsor::TypesMatch(int) const) and closes 0. 0x802278C8..0x80227ACC, 0x516 = 1302 bytes, NonMatching, 44.96% matched, 6 functions",
    "src/MetroidPrime/ScriptObjects/CScriptSound.cpp":
        "Listing it takes the port's undefined count 318 -> 320: it opens 2 symbol(s) nothing defines (CActorParameters::CActorParameters(), CScriptSound::TypesMatch(int) const) and closes 0. 0x8009DCE8..0x8009F4E4, 0x6140 = 24896 bytes, NonMatching, 2.93% matched, 15 functions",
    "src/MetroidPrime/ScriptObjects/CScriptSpecialFunction.cpp":
        "Listing it takes the port's undefined count 318 -> 321: it opens 3 symbol(s) nothing defines (CPlayer* TCastToPtr<CPlayer>(CEntity*), CActorParameters::CActorParameters(), CScriptSpecialFunction::TypesMatch(int) const) and closes 0. 0x80100C38..0x80109E7C, 0x37444 = 226372 bytes, NonMatching, 1.84% matched, 124 functions",
    "src/MetroidPrime/ScriptObjects/CScriptSpindleCamera.cpp":
        "Listing it takes the port's undefined count 318 -> 328: it opens 10 symbol(s) nothing defines (CScriptWaypoint* TCastToPtr<CScriptWaypoint>(…, CModelData::CModelDataNull(), CMotionSpline::Initialise(rstl::vector<CVecto… and 7 more) and closes 0. 0x801DFBE0..0x801E0004, 0x1060 = 4192 bytes, NonMatching, 13.96% matched, 3 functions",
    "src/MetroidPrime/ScriptObjects/CScriptTeamAiMgr.cpp":
        "Listing it takes the port's undefined count 318 -> 323: it opens 5 symbol(s) nothing defines (CPatterned* TCastToPtr<CPatterned>(CEntity*), CScriptTeamAiMgr* TCastToPtr<CScriptTeamAiMgr…, CActor* TCastToPtr<CActor>(CEntity*) and 2 more) and closes 0. 0x801727F8..0x8017645C, 0x15460 = 87136 bytes, NonMatching, 17.93% matched, 71 functions",
    "src/MetroidPrime/ScriptObjects/CScriptTrigger.cpp":
        "Listing it is a multiple definition of 1 symbol the port's link already defines - CScriptTrigger::GetTriggerBoundsWR() const - all of them in PortLinkStubs.cpp.o. 0x8007105C..0x80073474, 0x9240 = 37440 bytes, NonMatching, 10.00% matched, 44 functions. And it is out on the count as well: with the port's own copies still in the link the alternative order was measured too, and listing it instead takes the port's undefined count 318 -> 321 - it opens 3 symbol(s) nothing defines (CActor* TCastToPtr<CActor>(CEntity*), CActorParameters::CActorParameters(), CScriptTrigger::TypesMatch(int) const) and closes 0.",
    "src/MetroidPrime/ScriptObjects/CScriptWater.cpp":
        "Listing it takes the port's undefined count 318 -> 332: it opens 14 symbol(s) nothing defines (CFluidPlane::~CFluidPlane(), CFluidPlaneCPU::CFluidPlaneCPU(CVector2f cons…, CScriptTrigger::AcceptScriptMsg(CStateManager… and 11 more) and closes 0. 0x800D79D8..0x800DAE94, 0x13500 = 79104 bytes, NonMatching, 4.53% matched, 36 functions",
    "src/MetroidPrime/Weapons/CBeamProjectile.cpp":
        "Listing it takes the port's undefined count 318 -> 323: it opens 5 symbol(s) nothing defines (fn_80049ED8(CActor*, CStateManager&), CGameProjectile::CGameProjectile(bool, TToken…, CGameProjectile::~CGameProjectile() and 2 more) and closes 0. 0x80131404..0x80131DF8, 0x2548 = 9544 bytes, NonMatching, 40.97% matched, 7 functions",
    "src/MetroidPrime/Weapons/CEnergyProjectile.cpp":
        "Listing it takes the port's undefined count 318 -> 355: it opens 37 symbol(s) nothing defines (CGameLight* TCastToPtr<CGameLight>(CEntity*), CEnergyProjectile* TCastToPtr<CEnergyProjecti…, CActor* TCastToPtr<CActor>(CEntity*) and 34 more) and closes 0. 0x801685E4..0x8016B7B0, 0x12748 = 75592 bytes, NonMatching, 5.62% matched, 34 functions",
    "src/MetroidPrime/Weapons/CGameProjectile.cpp":
        "Listing it takes the port's undefined count 318 -> 332: it opens 14 symbol(s) nothing defines (CStateManager::UpdateActorInSortedLists(CActo…, CProjectileWeapon::Update(float), CProjectileWeapon::CProjectileWeapon(TToken<C… and 11 more) and closes 0. 0x80032C3C..0x80036200, 0x13764 = 79716 bytes, NonMatching, 10.00% matched, 49 functions",
    "src/MetroidPrime/Weapons/CPlasmaProjectile.cpp":
        "Listing it takes the port's undefined count 318 -> 326: it opens 8 symbol(s) nothing defines (CGameLight* TCastToPtr<CGameLight>(CEntity*), CBeamProjectile::UpdateFx(CTransform4f const&…, CBeamProjectile::CBeamProjectile(TToken<CWeap… and 5 more) and closes 0. 0x80119598..0x8011BFA8, 0x10768 = 67432 bytes, NonMatching, 0.07% matched, 21 functions",
    "src/MetroidPrime/Weapons/CWeapon.cpp":
        "Listing it takes the port's undefined count 318 -> 321: it opens 3 symbol(s) nothing defines (CEntityInfo::~CEntityInfo(), CActorParameters::CActorParameters(), CWeapon::TypesMatch(int) const) and closes 0. 0x800DAE94..0x800DB3B0, 0x1308 = 4872 bytes, NonMatching, 10.40% matched, 7 functions",
    "src/MetroidPrime/main.cpp":
        "Listing it is a multiple definition of 47 symbols the port's link already defines - InvokeCMain, InfiniteLoopAlarm(OSAlarm*, OSContext*), CPlayerState::SPersistentState::~SPersistentS… and 44 more - all of them in CGameGlobalObjectsCtor.cpp.o, CMainFillInAssetIDs.cpp.o, PortBoot.cpp.o, mainHead.cpp.o, mainMid.cpp.o, mainTail.cpp.o. 0x800053B8..0x80009880, 0x17608 = 95752 bytes, NonMatching, 18.11% matched, 99 functions. And it is out on the count as well: with the port's own copies still in the link the alternative order was measured too, and listing it instead takes the port's undefined count 318 -> 333 - it opens 15 symbol(s) nothing defines (CGameState::CGameState(), CGameState::~CGameState(), CTweakGame::GetPakFile() and 12 more) and closes 0.",
    "src/Weapons/CDecal.cpp":
        "Listing it takes the port's undefined count 318 -> 337: it opens 19 symbol(s) nothing defines (CTevCombiners::ResetStates(), CollisionUtil::RayPlaneIntersection(CVector3f…, CGraphics::mDepthNear and 16 more) and closes 0. 0x8026041C..0x8026258C, 0x8560 = 34144 bytes, NonMatching, 12.90% matched, 17 functions",
    "src/Weapons/CProjectileWeapon.cpp":
        "Listing it takes the port's undefined count 318 -> 341: it opens 23 symbol(s) nothing defines (CElementGen::EndLifetime(), CTevCombiners::sNextUniquePass, CTevCombiners::AlphaVar::AlphaVar(CTevCombine… and 20 more) and closes 0. 0x802591B4..0x8025CA80, 0x14540 = 83264 bytes, NonMatching, 14.94% matched, 33 functions",
    "src/WorldFormat/CAreaOctTree.cpp":
        "Listing it takes the port's undefined count 318 -> 319: it opens 1 symbol(s) nothing defines (CCollisionPrimitiveData::CCollisionPrimitiveD…) and closes 0. 0x80247704..0x80247CC0, 0x1468 = 5224 bytes, NonMatching, 47.41% matched, 5 functions",
    "src/WorldFormat/CCollidableOBBTree.cpp":
        "Listing it takes the port's undefined count 318 -> 324: it opens 6 symbol(s) nothing defines (COBBox::FromAABox(CAABox const&, CTransform4f…, CMRay::GetInvUnscaledTransformRay(CTransform4…, COBBox::CalculateAABox(CTransform4f const&) c… and 3 more) and closes 0. 0x8024F784..0x80252988, 0x12804 = 75780 bytes, NonMatching, 7.22% matched, 26 functions",
    "src/WorldFormat/CCollidableOBBTreeGroup.cpp":
        "Listing it takes the port's undefined count 318 -> 337: it opens 19 symbol(s) nothing defines (CollisionUtil::RayAABoxIntersection(CMRay con…, CCollidableOBBTree::CCollidableOBBTree(COBBTr…, CCollidableOBBTree::~CCollidableOBBTree() and 16 more) and closes 0. 0x80252988..0x802548B4, 0x7980 = 31104 bytes, NonMatching, 7.57% matched, 32 functions",
    "src/WorldFormat/CMetroidAreaCollider.cpp":
        "Listing it takes the port's undefined count 318 -> 319: it opens 1 symbol(s) nothing defines (CAreaOctTree::Node::GetChild(int) const) and closes 0. 0x80247CC0..0x8024DAFC, 0x24124 = 147748 bytes, NonMatching, 5.36% matched, 58 functions",
    "src/WorldFormat/COBBTree.cpp":
        "retail 0x8024DE68..0x8024F784, 0x6428 = 25640 bytes, NonMatching, 43.19% matched, 37 functions. 4 compile errors on a 64-bit host and all four are the same defect: `rs_new CNode(...)` at lines 53, 83, 100 and 101 is `no matching function for call to COBBTree::CNode::operator new(sizetype)`. include/Kyoto/Alloc/CMemory.hpp:52 defines `rs_new` as plain `new` off __MWERKS__, and include/WorldFormat/COBBTree.hpp:60 declares only the three-argument form `void* operator new(size_t, const char* file, int line)` that MWCC's `rs_new` needs - so on the host the placement arguments are gone and the one-argument call has no candidate. The fix is a second, host-only `operator new(size_t)` on CNode that forwards to the same `spAllocator->Alloc(size)`, which is a header change shared with CCollidableOBBTree.cpp and CCollidableOBBTreeGroup.cpp and therefore wants its own change with its own DOL sha gate, not a #ifdef TARGET_PC hunk inside a source-list edit. The unit is the one configure.py object in this batch that does not compile on the host at all; its link cost is unmeasured because there is no object to measure, and it becomes worth listing with the header fix and with the CCollidable* pair.",
    # --- the 2026-09-28 upstream merge's game wave, continued: four host sources the port
    # carried before upstream had the real unit. Each is dropped from files.cmake and recorded
    # here, because a source file in no manifest is a `dead:` failure and because a name that
    # quietly stops being compiled is how the previous 96 omissions happened. (The four
    # CMainFlow* carves are a fifth such set and are still listed - CMainFlow.cpp costs +16
    # without them; its own entry above says so.)
    "src/MetroidPrime/CAxisAngleGetVector.cpp":
        "retail GetVector__10CAxisAngleCFv, 0x8001D2BC, 0x4 = 4 bytes: this port's own carve of `CAxisAngle::GetVector() const`, taken out of dtk's auto_03_8001D084_text so it could be promoted to a unit of its own. Superseded on the host by configure.py's own src/MetroidPrime/CAxisAngle.cpp (MatchingFor, 100.00% matched, 13 functions, retail 0x8001D0CC..0x8001D430), which defines the same symbol plus the constructor, Identity, operator+=, operator* and operator+ that PortLinkStubs.cpp had stubbed - those five stubs were deleted at the same time. Both copies in one flat link is a multiple definition. Net on the port's link: 0 either way.",
    "src/MetroidPrime/CIOWinCtor.cpp":
        "retail 0x80049E98-0x80049ED8, 0x40 = 64 bytes: this port's own carve of CIOWin's two constructors. Superseded on the host by configure.py's own src/MetroidPrime/CIOWin.cpp (MatchingFor, 100.00% matched, 7 functions, retail 0x80049E10..0x80049ED8), which defines the same two symbols and the six the other two CIOWin* carves held, and emits `vtable for CIOWin` at 0x803B1BA0 itself. Net on the port's link: -1 either way - the vtable was the only symbol the linker asked for.",
    "src/MetroidPrime/CIOWinDtor.cpp":
        "retail 0x80049E30-0x80049E98, 104 bytes: this port's own carve of CIOWin's destructor, claimed together with `vtable for CIOWin` at 0x803B1BA0 so dtk would not fill that object with retail bytes a second time. Superseded on the host by configure.py's own src/MetroidPrime/CIOWin.cpp, which emits both. Net on the port's link: 0 either way.",
    "src/MetroidPrime/CIOWinAccessors.cpp":
        "retail 0x80049E10-0x80049E20, 0x10 = 16 bytes: this port's own carve of CIOWin::PreDraw, Draw and GetIsContinueDraw, the three vtable slots. Superseded on the host by configure.py's own src/MetroidPrime/CIOWin.cpp, which defines the same three symbols. Net on the port's link: 0 either way.",
}

# REL module entry points. Each is a module we have reimplemented and each defines RELMain and/or
# RELExit - and some a module-local SetFuncPtrs - which is correct on the cube, where they are
# separate modules, and impossible in a flat link. The fix is a host-only rename behind
# `#ifdef __MWERKS__`; tools/rename_module_entries.py writes it and
# platform/compiled_modules.cpp registers the results. **Attempted and reverted: it broke 8 module
# hashes in the matching build**, and proving a host-only rename costs nothing under mwcceppc is its
# own piece of work. Until then these modules' code is in no port binary.
MODULE_ENTRY = re.compile(r"\bvoid RELMain\((?:void)?\)|\bvoid RELExit\((?:void)?\)")

# Any top-level directory. This used to be a fixed list (MetroidPrime|Kyoto|rstl|...), and the
# upstream merge brought Collision/, GuiSys/, WorldFormat/ and Weapons/ units that the list could
# not see - so they were never asked about here, only misreported as `dead:` below.
DOL_OBJECT = re.compile(
    r'Object\(\s*(Matching|NonMatching|MatchingFor)\s*(?:\([^)]*\))?\s*,\s*'
    r'"([^"/]+/[^"]+)\.cpp"')


def main() -> int:
    # Strip comments first: files.cmake documents its exclusions in comments that
    # name the very paths being excluded, and a naive scan counts those as listed.
    raw = (ROOT / "files.cmake").read_text()
    fm = "\n".join(line for line in raw.splitlines()
                    if not line.lstrip().startswith("#"))
    listed = set(re.findall(r"(src/\S+\.c(?:pp)?)", fm))
    configured = DOL_OBJECT.findall((ROOT / "configure.py").read_text())
    configured_rels = {f"src/{path}.cpp" for _, path in configured}

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
        # `configured` holds (state, path) tuples; comparing `rel` against it directly was
        # never true, so every configured unit missing from files.cmake read as `dead:`.
        if rel in configured_rels or rel in listed or rel in EXCLUDED:
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
