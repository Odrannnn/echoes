#!/usr/bin/env python3

###
# Generates build files for the project.
# This file also includes the project configuration,
# such as compiler flags and the object matching status.
#
# Usage:
#   python3 configure.py
#   ninja
#
# Append --help to see available options.
###

import argparse
import sys
from pathlib import Path
from typing import Any, Dict, List

from tools.project import (
    Object,
    ProgressCategory,
    ProjectConfig,
    calculate_progress,
    generate_build,
    is_windows,
)

# Game versions
DEFAULT_VERSION = 0
VERSIONS = [
    "G2ME01",  # 0
    "G2MP01",  # 1
    "G2MJ01",  # 2
    "R32J01",  # 3
    "R3ME01",  # 4
    "R3MP01",  # 5
]

parser = argparse.ArgumentParser()
parser.add_argument(
    "mode",
    choices=["configure", "progress"],
    default="configure",
    help="script mode (default: configure)",
    nargs="?",
)
parser.add_argument(
    "-v",
    "--version",
    choices=VERSIONS,
    type=str.upper,
    default=VERSIONS[DEFAULT_VERSION],
    help="version to build",
)
parser.add_argument(
    "--build-dir",
    metavar="DIR",
    type=Path,
    default=Path("build"),
    help="base build directory (default: build)",
)
parser.add_argument(
    "--binutils",
    metavar="BINARY",
    type=Path,
    help="path to binutils (optional)",
)
parser.add_argument(
    "--compilers",
    metavar="DIR",
    type=Path,
    help="path to compilers (optional)",
)
parser.add_argument(
    "--map",
    action="store_true",
    help="generate map file(s)",
)
parser.add_argument(
    "--debug",
    action="store_true",
    help="build with debug info (non-matching)",
)
if not is_windows():
    parser.add_argument(
        "--wrapper",
        metavar="BINARY",
        type=Path,
        help="path to wibo or wine (optional)",
    )
parser.add_argument(
    "--dtk",
    metavar="BINARY | DIR",
    type=Path,
    help="path to decomp-toolkit binary or source (optional)",
)
parser.add_argument(
    "--objdiff",
    metavar="BINARY | DIR",
    type=Path,
    help="path to objdiff-cli binary or source (optional)",
)
parser.add_argument(
    "--sjiswrap",
    metavar="EXE",
    type=Path,
    help="path to sjiswrap.exe (optional)",
)
parser.add_argument(
    "--ninja",
    metavar="BINARY",
    type=Path,
    help="path to ninja binary (optional)",
)
parser.add_argument(
    "--verbose",
    action="store_true",
    help="print verbose output",
)
parser.add_argument(
    "--non-matching",
    dest="non_matching",
    action="store_true",
    help="builds equivalent (but non-matching) or modded objects",
)
parser.add_argument(
    "--warn",
    dest="warn",
    type=str,
    choices=["all", "off", "error"],
    help="how to handle warnings",
)
parser.add_argument(
    "--no-progress",
    dest="progress",
    action="store_false",
    help="disable progress calculation",
)
args = parser.parse_args()

config = ProjectConfig()
config.version = str(args.version)
version_num = VERSIONS.index(config.version)

# Apply arguments
config.build_dir = args.build_dir
config.dtk_path = args.dtk
config.objdiff_path = args.objdiff
config.binutils_path = args.binutils
config.compilers_path = args.compilers
config.generate_map = args.map
config.non_matching = args.non_matching
config.sjiswrap_path = args.sjiswrap
config.ninja_path = args.ninja
config.progress = args.progress
if not is_windows():
    config.wrapper = args.wrapper
# Don't build asm unless we're --non-matching
if not config.non_matching:
    config.asm_dir = None

# Tool versions
config.binutils_tag = "2.42-2"
config.compilers_tag = "20251118"
config.dtk_tag = "v1.8.4"
# v3.8.1 fails to generate the G2ME01 report due to a symbol-pairing regression.
config.objdiff_tag = "v3.7.0"
config.sjiswrap_tag = "v1.2.2"
config.wibo_tag = "1.1.0"

# Project
config.config_path = Path("config") / config.version / "config.yml"
config.check_sha_path = Path("config") / config.version / "build.sha1"
config.asflags = [
    "-mgekko",
    "--strip-local-absolute",
    "-I include",
    f"-I build/{config.version}/include",
    f"--defsym BUILD_VERSION={version_num}",
    f"--defsym VERSION_{config.version}",
]
config.ldflags = [
    "-fp hardware",
    "-nodefaults",
]
if args.debug:
    config.ldflags.append("-g")  # Or -gdwarf-2 for Wii linkers
if args.map:
    config.ldflags.append("-mapunused")
    # config.ldflags.append("-listclosure") # For Wii linkers

# Use for any additional files that should cause a re-configure when modified
config.reconfig_deps = []

# Optional numeric ID for decomp.me preset
# Can be overridden in libraries or objects
config.scratch_preset_id = None

# Base flags, common to most GC/Wii games.
# Generally leave untouched, with overrides added below.
cflags_base = [
    "-nodefaults",
    "-proc gekko",
    "-align powerpc",
    "-enum int",
    "-fp hardware",
    "-Cpp_exceptions off",
    # "-W all",
    "-O4,p",
    "-inline auto",
    '-pragma "cats off"',
    '-pragma "warn_notinlined off"',
    "-maxerrors 1",
    "-nosyspath",
    "-RTTI off",
    "-fp_contract on",
    "-str reuse",
    "-i include",
    "-i libc",
    f"-i build/{config.version}/include",
    f"-DBUILD_VERSION={version_num}",
    f"-DVERSION_{config.version}",
]

# GC 3.0 and above require -enc SJIS instead of -multibyte
if version_num >= 3:
    cflags_base.append("-enc SJIS")
else:
    cflags_base.append("-multibyte")

# Debug flags
if args.debug:
    # Or -sym dwarf-2 for Wii compilers
    cflags_base.extend(["-sym on", "-DDEBUG=1"])
else:
    cflags_base.append("-DNDEBUG=1")

# Warning flags
if args.warn == "all":
    cflags_base.append("-W all")
elif args.warn == "off":
    cflags_base.append("-W off")
elif args.warn == "error":
    cflags_base.append("-W error")

# Dolphin flags
cflags_dolphin = [
    *cflags_base,
    "-multibyte",
    "-fp_contract off",
]

# Metrowerks library flags
cflags_runtime = [
    *cflags_base,
    "-use_lmw_stmw on",
    "-str reuse,pool,readonly",
    "-gccinc",
    "-common off",
    # "-inline auto",
]

# Retro flags
cflags_retro = [
    *cflags_base,
    "-use_lmw_stmw on",
    "-str reuse,pool,readonly",
    "-gccinc",
    "-inline deferred,noauto",
    "-common on",
]

# REL flags
cflags_rel = [
    *cflags_retro,
    "-sdata 0",
    "-sdata2 0",
]

if version_num >= 3:
    cflags_runtime.append("-inline auto")
    config.linker_version = "GC/3.0a5"
else:
    cflags_runtime.append("-inline deferred,auto")
    config.linker_version = "GC/2.7"

if version_num > 0:
    # RELs not yet set up for non-USA versions
    config.build_rels = False

# Helper function for Dolphin libraries
def DolphinLib(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": "GC/1.2.5n",
        "cflags": cflags_dolphin,
        "progress_category": "sdk",
        "host": False,
        "objects": objects,
    }


# Helper function for REL script objects
def Rel(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": "GC/1.3.2",
        "cflags": cflags_rel,
        "progress_category": "game",
        "host": True,
        "objects": objects,
    }


Matching = True                   # Object matches and should be linked
NonMatching = False               # Object does not match and should not be linked
Equivalent = config.non_matching  # Object should be linked when configured with --non-matching


# Object is only matching for specific versions
def MatchingFor(*versions):
    return config.version in versions


config.warn_missing_config = True
config.warn_missing_source = False
config.libs = [
    {
        "lib": "MetroidPrime",
        "cflags": cflags_retro,
        "mw_version": "GC/2.7",
        "progress_category": "game",  # str | List[str]
        "host": True,
        "objects": [
            Object(
                NonMatching,
                "MetroidPrime/main.cpp",
                extra_cflags=['-pragma "inline_max_size(125)"'] if config.version == "G2ME01" else [],
            ),
            Object(NonMatching, "MetroidPrime/CStateManager.cpp"),
            Object(NonMatching, "MetroidPrime/CEntity.cpp"),
            Object(NonMatching, "MetroidPrime/TypesMatch.cpp"),
            Object(Matching, "MetroidPrime/CIOWinCtor.cpp"),
            # CIOWin's destructor and its three non-pure virtuals, 0x80049E10-0x80049E20 and
            # 0x80049E30-0x80049E98, plus `vtable for CIOWin` at 0x803B1BA0. The destructor is
            # the key function, so its unit emits the vtable; the accessors have to land with it
            # because the vtable's slots relocate against them (docs/research/boot_probe.md).
            Object(Matching, "MetroidPrime/CIOWinAccessors.cpp"),
            Object(Matching, "MetroidPrime/CIOWinDtor.cpp"),
            Object(Matching, "MetroidPrime/CIOWinManagerCtor.cpp"),
            # AddIOWin and RemoveAllIOWins became writable once rstl::rc_ptr had retail's
            # 8-byte layout (docs/research/rc_ptr.md). Both are NonMatching and neither is close
            # enough to link: AddIOWin differs only in mwcceppc's materialisation of new's
            # file-string operand (one extra `addi` per allocation site, twice) and in which
            # register holds the insertion cursor; RemoveAllIOWins differs only in inlining a
            # copy constructor retail calls out of line. Both claim their retail range so
            # objdiff measures them, which is safe: a NonMatching object is not in the link.
            Object(NonMatching, "MetroidPrime/CIOWinManagerAddIOWin.cpp"),
            Object(NonMatching, "MetroidPrime/CIOWinManagerRemoveAllIOWins.cpp"),
            # PumpMessages (0x800496A0, 196 bytes) and CArchitectureQueue::Pop
            # (fn_800495F0, 0x800495F0, 176 bytes). Both NonMatching for one reason, and it
            # is not rc_ptr: rstl::list<CArchitectureMessage>::do_erase is a template member, so
            # mwcceppc emits it W where retail has fn_80048F78 as a strong T. Pop calls it, so
            # Pop cannot be Matching; PumpMessages calls Pop, so it cannot be either.
            Object(NonMatching, "MetroidPrime/CIOWinManagerPumpMessages.cpp"),
            # CArchitectureMessage::GetParm() and GetParm() const, retail 0x80048CEC and 0x80048CE4,
            # 8 bytes each and identical. They were unnamed in the DOL and nothing called them, so
            # they are what `CMainFlow::OnMessage`'s `bl 0x80048ce4` needs; symbols.txt now names
            # the two mangled forms. `inline_max_size(0)` on the declarations keeps mwcceppc from
            # expanding two instructions into the caller.
            Object(Matching, "MetroidPrime/CArchitectureMessageGetParm.cpp"),
            # CFrameMsgParm / CTimerMsgParm's out-of-line destructors, retail 0x800487B8 and
            # 0x80048834, 0x5C = 92 bytes each, plus `lbl_803B1B60` and `lbl_803B1B70` - their
            # vtables. The destructor is the key function, so its unit is the one that emits the
            # vtable, which is the same arrangement as CIOWinDtor/CMainFlowDtor. The parm classes
            # came out of main.cpp's anonymous namespace for this: a local vtable symbol cannot be
            # placed, and `CMainFlow::OnMessage` stores 0x803B1B60 literally. See
            # docs/research/boot_probe.md's closing section, blockers 1 and 2.
            Object(Matching, "MetroidPrime/CFrameMsgParmDtor.cpp"),
            Object(Matching, "MetroidPrime/CTimerMsgParmDtor.cpp"),
            # CMainFlow::OnMessage, retail fn_8001DF54, 0x8001DF54, 0xB4 = 180 bytes. The
            # function `vtable for CMainFlow`'s slot 2 points at; closing it closes the last
            # named hole boot_probe.md found in the frame loop. Its three blockers are closed by
            # CArchitectureMessageGetParm.cpp (#3), CArchitectureMessageParm.hpp (#1) and
            # CFrameMsgParmDtor.cpp (#2).
            Object(Matching, "MetroidPrime/CMainFlowOnMessage.cpp"),
            Object(Matching, "MetroidPrime/CMainFlowAccessors.cpp"),
            # **One unit for all three of these, and that is forced.** ~CMainFlow is the key
            # function, so this is the unit that emits `vtable for CMainFlow` (0x803B1770, 0x1C).
            # AdvanceGameState's switch jumptable sits at 0x803B178C, immediately after the
            # vtable, and a Matching unit has to carry its own jumptable - but mwcceppc puts
            # `.data` in an 8-byte-aligned section, 0x803B178C is only 4-aligned, and the
            # jumptable is emitted as a local `@N` symbol rather than the `jumptable_803B178C`
            # symbols.txt names. So the only 8-aligned range that can hold it starts with the
            # vtable, which puts AdvanceGameState (0x8001DE68) and SetGameState (0x8001DB54)
            # here too: one unit cannot claim two discontiguous ranges. 1108 bytes of .text and
            # 96 of .data, all Matching, flip_test PASS. See the source for the measurement.
            Object(Matching, "MetroidPrime/CMainFlowDtor.cpp"),
            Object(Matching, "MetroidPrime/CMainFlowCtor.cpp"),
            Object(Matching, "MetroidPrime/CInputGeneratorCtor.cpp"),
            # CInputGenerator::Update, retail fn_8001D888, 0x8001D888, 0x1FC = 508 bytes - the
            # largest single symbol in the frame loop. **The reason this used to give for keeping
            # it NonMatching is wrong, and is corrected here (2026-09-26, lane j1).** It said:
            # "its `queue.Push(msg)` is Push__18CArchitectureQueueFRC20CArchitectureMessage at
            # 0x80007A80, and tools/range_owner.py says that range belongs to MetroidPrime/main.cpp,
            # a NonMatching unit, so nothing in the DOL link defines it and a Matching unit calling
            # it would not link." It *is* defined: `dtk dol split` writes a filled
            # build/G2ME01/obj/MetroidPrime/main.o carrying retail's bytes for the functions that
            # unit does not match, and that filled object is what the DOL link uses for a
            # NonMatching unit. `nm` shows `Push__18CArchitectureQueueFRC20CArchitectureMessage` as
            # a weak T there, and it is in main.elf at 0x80007A80. A probe Matching unit calling
            # one main.cpp symbol, one unclaimed symbol and one Matching symbol linked cleanly.
            # So this unit is NonMatching on its own merits (98.27%, blocked on mwcceppc's 16
            # bytes of stack slack - see RUNNING_THE_DECOMP.md's attempted table), not because of
            # the callee's owner. src/MetroidPrime/main.cpp is another lane's.
            Object(NonMatching, "MetroidPrime/CInputGeneratorUpdate.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CPlayerState.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CPlayerGun.cpp"),
            Object(Matching, "MetroidPrime/Player/CMorphBallC80.cpp"),
            # fn_80180738, retail 0x80180738, 0x24 = 36 bytes. The default constructor of the
            # class at CGameState+0xC4, reached from CGameState::CGameState(CInputStream&, int)
            # (retail fn_80144140) at 0x801441EC. Six stores and a blr, and it calls nothing, so
            # it is net -1 on the port's link. It is an extern "C" free function rather than
            # CHintOptions::CHintOptions() because retail's symbol table has no name for it.
            Object(Matching, "MetroidPrime/Player/CHintOptionsCtor.cpp"),
            # fn_80144924 + fn_8014495C, 0x80144924..0x801449C8 = 164 contiguous bytes: the
            # `(count, element)` fill constructor of CGameState's two {count; block[3]} members
            # at +0x110 and +0x144, and the element loop inside it. Both functions call exactly
            # one thing, fn_80142A10, which has its own unit 0x848 bytes away (a configure.py
            # unit may claim only one range) and is reached by a relocation either way.
            Object(Matching, "MetroidPrime/Player/CGameStateSlotsCtor.cpp"),
            # fn_80142A10, 0x80142A10, 0x20 = 32 bytes: the copy constructor of the 16-byte
            # element those two blocks are built from, eight instructions of frame and a tail
            # call to fn_80004D5C.
            Object(Matching, "MetroidPrime/Player/CGameStateBlockCopy.cpp"),
            # fn_80144140, retail 0x80144140, 0x684 = 1,668 bytes: CGameState's stream
            # constructor, the only writer of gpGameState on the boot path. **NonMatching** and
            # claiming its range so objdiff measures it, which is safe: a NonMatching object is
            # not in the DOL link. The 42 member offsets and sizes it needs are all in
            # include/MetroidPrime/Player/CGameState.hpp and are measured with mwcceppc's own
            # flags by tools/probe_gs_offsets.py. Two blocks inside it are **not expressible in
            # C++**: fn_80146154 reads two uninitialised words of its own parameter save area,
            # and the memset-shaped fill at 0x801444E0 reads its length out of an uninitialised
            # stack word. Neither blocks the caller, which only needs the relocation.
            Object(NonMatching, "MetroidPrime/Player/CGameStateStreamCtor.cpp"),
            # fn_8015C34C, retail 0x8015C34C, 0x114 = 276 bytes: CWorldState's default
            # constructor - the **+0x3C member** of CGameState, and so the one piece of CGameState
            # that boot-path step 17 needs. Its only two callees are named retail functions,
            # __ct__9CRandom16FUi (already a Matching unit) and __ct__12CTransform4fFRC12CTransform4f
            # (100% inside Kyoto/Math/CTransform4f.cpp), so it was the cheapest large thing left.
            # **NonMatching at 89.13%, 272 of 276 bytes**, on one dead instruction: retail has an
            # unused `mr r3,r31` between its +0x4A4 and +0x4A8 stores, which no spelling of the
            # source produces, and mwcceppc's register allocator then gives the bitfield block
            # r3/r4 where retail uses r4/r5. It claims its retail range so objdiff measures it,
            # which is safe: a NonMatching object is not in the link. The 25 member offsets and
            # sizeof 0x4B0 it needed are all in include/MetroidPrime/CWorldState.hpp, measured
            # with mwcceppc's own flags, and docs/research/cgamestate_layout.md has the same map
            # from the CGameState side.
            Object(NonMatching, "MetroidPrime/CWorldStateCtor.cpp"),
            Object(Matching, "MetroidPrime/Player/CGunEffectTouch.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CGunEffectTouchAll.cpp"),
            Object(NonMatching, "MetroidPrime/CModelDataDefaultCtor.cpp"),
            Object(Matching, "MetroidPrime/CModelDataModelSlots.cpp"),
            # __ct__10CModelDataFRC10CModelData, 0x80018FBC, 0x13C = 316 bytes. The class's
            # copy constructor, and the only code in the DOL that writes all nine members.
            # Needs include/MetroidPrime/CModelData.hpp's three model slots to be
            # `TLockedToken`, not `TCachedToken` - the `Lock__6CTokenFv` in retail's copy of
            # each is `TLockedToken`'s copy constructor - and the four flag bits to be a named
            # struct, because retail copies that byte whole and MWCC only does that for a
            # struct. Both are layout-neutral (0x4C before and after).
            Object(Matching, "MetroidPrime/CModelDataCopyCtor.cpp"),
            # fn_8019E6BC__Q213CStateManager14ScriptMsgArrayCFv, 0x8019E6BC, 0x58 = 88 bytes:
            # pop the oldest script message off CStateManager::ScriptMsgArray, a 192-entry
            # ring buffer. Call-free and byte-exact. Needs the two cursors to be `uint` (retail
            # wraps them with an unsigned multiply-high), the read cursor `mutable` (the pop is
            # a const member function that advances it), and the pop to return by value (retail
            # builds the message in the caller's return slot in r3). The class's other two
            # methods are written in CStateManager.cpp but are not exact, so they are not
            # claimed here. Together the three are net -3 on the port's link.
            Object(Matching, "MetroidPrime/CStateManagerScriptMsgArray.cpp"),
            Object(Matching, "MetroidPrime/CModelTouchParts.cpp"),
            # CModel::Touch(int) const: Touch__6CModelCFi, 0x803112DC, 0x4C = 76 bytes. The
            # callee CModelTouchParts.cpp has been calling since it went Matching.
            Object(Matching, "Kyoto/Graphics/CModelTouch.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptPickup.cpp"),
            Object(Matching, "MetroidPrime/HUD/CHUDMemoParms.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptHUDMemo.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptRelay.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CUnknown90.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptAreaProperties.cpp"),
            Object(Matching, "MetroidPrime/CHealthInfo.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CGameOptions.cpp"),
            Object(Matching, "MetroidPrime/Player/CGameOptionsDefaults.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptSequenceTimer.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptStreamedMusic.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoaderRel.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/SpacePirate.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Kralee.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Parasite.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/PillBug.cpp"),
            # CTweakPlayer's five accessors. Retail's are two contiguous runs of
            # named 12-byte functions in the unclaimed gap between SporbBase.cpp
            # (ends 0x80213CB8) and Sandworm.cpp (starts 0x80218850), so claiming
            # them disturbs nothing. Not the Tweaks REL module despite the path:
            # these are DOL addresses, and their only caller is
            # __ct__CGameArchitectureSupport at 0x80007F40.
            Object(Matching, "MetroidPrime/Tweaks/CTweakPlayerAnalog.cpp"),
            Object(Matching, "MetroidPrime/Tweaks/CTweakPlayerSuit.cpp"),
            # CGraphics has no .cpp in the tree, so a Matching unit here can only
            # reach the class's *unnamed* statics by their dtk labels - a reference
            # to CGraphics::mSecondsMod900 would mangle to a symbol nothing defines
            # in the DOL. Both ranges were UNCLAIMED; see the two source headers.
            Object(Matching, "Kyoto/Graphics/CGraphicsTimeProvider.cpp"),
            Object(Matching, "Kyoto/Graphics/CGraphicsScreenPosition.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/SporbBase.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Sandworm.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/CommandPirate.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/DarkSamus.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Ings.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/SandBoss.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/FlyingPirate.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Grenchler.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/MediumIng.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/MinorIng.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/ElitePirate.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Blogg.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/MetroidAlpha.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/GunTurretBase.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Lumite.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Shrieker.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Splinter.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/SplitterMainChassis.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/ChozoGhost.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Tryclops.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/WispTentacle.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/SpankWeed.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/DarkTrooper.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/GlowBug.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/IngSpaceJumpGuardian.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/DigitalGuardian.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Shredder.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/FrontEndDataNetwork.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/StoneToad.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Coin.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/CannonBall.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Krocus.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/AIMannedTurret.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/EmperorIngStage1.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/OctopedeSegment.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Rezbit.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/RsfAudio.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/IngPuddle.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/FlyerSwarm.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/StreamedMovie.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/IngSpiderBallGuardian.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/PuddleSpore.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/EmperorIngStage2Tentacle.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/BacteriaSwarm.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/MetareeSwarm.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/IngBlobSwarm.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/EmperorIngStage3.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/DestructableBarrier.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/SwampBossStage2.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/SwampBossStage1.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/IngBoostBallGuardian.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/PlantScarabSwarm.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/SkyRipple.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/FogOverlay.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/MysteryFlyer.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/AtomicBeta.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/EyeBall.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/DarkSamusBattleStage.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/DarkCommando.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/RubiksPuzzle.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader.cpp"),
            Object(NonMatching, "MetroidPrime/CAxisAngle.cpp"),
            Object(NonMatching, "MetroidPrime/CEulerAngles.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/CPowerBeam.cpp"),
            Object(Matching, "MetroidPrime/Weapons/CGunWeaponTouch.cpp"),
            Object(NonMatching, "MetroidPrime/CPhysicsActor.cpp"),
            Object(NonMatching, "MetroidPrime/CActor.cpp"),
            Object(Matching, "MetroidPrime/CActorField25.cpp"),
            Object(Matching, "MetroidPrime/CMiscTableInit.cpp"),
            Object(Matching, "MetroidPrime/Enemies/CAi.cpp"),
            Object(Matching, "MetroidPrime/Enemies/CPatterned.cpp"),
            Object(NonMatching, "MetroidPrime/Enemies/CPatternedCtor.cpp"),
            Object(NonMatching, "MetroidPrime/CDamageInfo.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptSpawnPoint.cpp"),
            Object(
                NonMatching,
                "MetroidPrime/CRuleSet.cpp",
                extra_cflags=['-pragma "inline_max_size(125)"'] if config.version == "G2ME01" else [],
            ),
            Object(NonMatching, "MetroidPrime/Player/CPlayer.cpp"),
            # Two CPlayer accessors, each its own unit because they are 0x10D8 apart
            # and a configure.py unit cannot claim two discontiguous ranges.
            Object(Matching, "MetroidPrime/Player/CPlayerGetTweakPlayer.cpp"),
            Object(Matching, "MetroidPrime/Player/CPlayerGetPlayerIndex.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScanTreeInventory.cpp"),
        ],
    },
    {
        "lib": "Kyoto_CW",
        "mw_version": "GC/2.7",
        "cflags": cflags_retro,
        "progress_category": "game",  # str | List[str]
        "host": True,
        "objects": [
            Object(Matching, "Kyoto/Basics/CStopwatch.cpp"),
            Object(Matching, "Kyoto/Basics/CStopwatchCSWData.cpp"),
            Object(NonMatching, "Kyoto/Basics/CStopwatchCSWDataWait.cpp"),
            Object(Matching, "Kyoto/Basics/RAssertDolphin.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CDvdRequest.cpp"),
            Object(NonMatching, "Kyoto/Graphics/CGX.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CloseEnough.cpp"),
            Object(Matching, "Kyoto/CARAMManagerWait.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CMatrix3f.cpp"),
            Object(Matching, "Kyoto/Math/CMathSqrtF.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CMatrix4f.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CNUQuaternion.cpp"),
            Object(NonMatching, "Kyoto/Math/CQuaternion.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CRandom16.cpp"),
            Object(NonMatching, "Kyoto/Math/CTransform4f.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CUnitVector3f.cpp"),
            Object(Matching, "Kyoto/Math/CAABox.cpp"),
            Object(Matching, "Kyoto/CFactoryMgr.cpp"),
            # `CResFactory::Build`, retail fn_802FA960, 0x802FA960, 0xC0 = 192 bytes. The key
            # function of the third frame-0 vtable, and the gate for it: with the destructor
            # declared out of line, this object emits no vtable at all, and the vtable's 0x20
            # bytes at 0x803BAF08 stay with dtk's fill. See docs/research/paks.md, "The vtable
            # and the destructor that cannot own it", for the two measurements that put it there.
            Object(Matching, "Kyoto/CResFactoryBuild.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CTri.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CQuad.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CPlane.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CSphere.cpp"),
            Object(NonMatching, "Kyoto/Math/CFrustumPlanes.cpp"),
            Object(Matching, "Kyoto/Math/CVector2f.cpp"),
            Object(Matching, "Kyoto/Math/CVector2i.cpp"),
            Object(Matching, "Kyoto/Math/CVector3d.cpp"),
            Object(Matching, "Kyoto/Math/CVector3f.cpp"),
            Object(Matching, "Kyoto/Math/CVector3i.cpp"),
            Object(NonMatching, "Kyoto/Math/RMathUtils.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CCrc32.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Alloc/CCircularBuffer.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Alloc/CMemory.cpp"),
            Object(NonMatching, "Kyoto/Alloc/CMediumAllocPool.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Alloc/CSmallAllocPool.cpp"),
            Object(NonMatching, "Kyoto/Alloc/CGameAllocator.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Alloc/IAllocator.cpp"),
            Object(NonMatching, "Kyoto/PVS/CPVSVisOctree.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/PVS/CPVSVisSet.cpp"),
            Object(Matching, "Kyoto/Input/DolphinIController.cpp"),
            Object(Matching, "Kyoto/Input/CDolphinController.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Graphics/CColor.cpp"),
            Object(NonMatching, "Kyoto/Graphics/DolphinCColor.cpp"),
            Object(NonMatching, "Kyoto/Input/CRumbleVoice.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Input/RumbleAdsr.cpp"),
            Object(NonMatching, "Kyoto/Input/CRumbleGenerator.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CCharAnimTime.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CTimeRemainderAndFraction.cpp"),
            Object(Matching, "Kyoto/Animation/CSegId.cpp"),
            Object(Matching, "Kyoto/Animation/CSegIdList.cpp"),
            Object(NonMatching, "Kyoto/DolphinCDvdFile.cpp"),
            Object(Matching, "Kyoto/Graphics/CCubeSurface.cpp"),
            Object(NonMatching, "Kyoto/Graphics/CCubeMoviePlayer.cpp"),
            Object(Matching, "Kyoto/Audio/CAudioSysVolume.cpp"),
            Object(Matching, "Kyoto/Audio/CAudioSysSurround.cpp"),
            Object(Matching, "Kyoto/Audio/CAudioSysAICallback.cpp"),
            Object(Matching, "Kyoto/Audio/CAudioSysSysVolume.cpp"),
            Object(Matching, "Kyoto/Audio/CAudioSysTrkSampleRate.cpp"),
            Object(Matching, "Kyoto/Audio/CStreamAudioManagerSfxVolume.cpp"),
            Object(Matching, "Kyoto/Audio/CStreamAudioManagerMusicVolume.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Audio/g721.cpp"),
            Object(NonMatching, "Kyoto/Audio/CStaticAudioPlayer.cpp"),
            Object(Matching, "Kyoto/CFrameDelayedKiller.cpp"),
            Object(NonMatching, "Kyoto/CDependencyGroup.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CARAMToken.cpp"),
            Object(NonMatching, "Kyoto/Text/CStringTable.cpp"),
            Object(NonMatching, "Kyoto/Text/CFontImageDef.cpp"),
            Object(NonMatching, "Kyoto/CPakFile.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CResLoaderAddPakFileAsync.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CResLoaderInsert.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CResLoaderResAccessors.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CResLoaderFindPak.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CResLoaderLoadPartAsync.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CResLoaderLoadResourceSync.cpp"),
            # 99.10% and left `NonMatching` on purpose: the whole body is right and the
            # range is claimed so retail bytes stay in the link, but mwcceppc hands the
            # compressed arm its four temporaries as r6/r7 and r29/r30 where retail uses
            # r7/r6 and r30/r29 - a register-allocation preference no source shape reached
            # (about 40 bodies tried). See the file comment and docs/research/paks.md.
            Object(NonMatching, "Kyoto/CResLoaderLoadResourceSyncCompressed.cpp"),
            # 98.04% and left `NonMatching` on purpose: every instruction is right and the
            # range is claimed so retail bytes stay in the link, but the compressed arm
            # gets r27/r28/r29 where retail uses r28/r29/r30 - the same allocation
            # preference fn_802FC4D8 has, in reverse. See the file comment.
            Object(NonMatching, "Kyoto/CResLoaderLoadNewResourceSync.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CResLoaderLoadAsync.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CResLoaderGetResIdByName.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CResLoaderPakPump.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CResLoaderGetPakCount.cpp"),
            Object(NonMatching, "Kyoto/CResLoaderGetPakFile.cpp"),
            Object(Matching, "Kyoto/CTimeProvider.cpp"),
            Object(NonMatching, "Kyoto/CObjectReference.cpp"),
            Object(Matching, "Kyoto/CToken.cpp"),
            Object(NonMatching, "Kyoto/Math/CMayaSpline.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Streams/CInputStream.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Streams/CBitStreamReader.cpp"),
            Object(NonMatching, "rstl/rstl_map.cpp"),
            # rstl::CRcPtrData's copy constructor: retail's out-of-line `rc_ptr` copy
            # constructor, fn_80049010 / 0x80049010, 0x24 = 36 bytes. It is `NonMatching`
            # because mwcceppc allocates the AddRef to r5/r4 where retail uses r4/r3 - the
            # out-of-line allocator differs from the one used for an inlined expansion, and
            # no spelling of the body changes it. Five of the nine instructions match. Until
            # it is 100%, nothing in the DOL may call it, so
            # MetroidPrime/CIOWinManagerRemoveAllIOWins.cpp is `NonMatching` too even though
            # it is now byte-exact. See include/rstl/rc_ptr.hpp and docs/research/rc_ptr.md.
            Object(NonMatching, "rstl/rc_ptr_copy.cpp"),
            Object(
                MatchingFor("G2ME01"),
                "rstl/rstl_strings.cpp",
                extra_cflags=["-inline deferred"] if config.version == "G2ME01" else [],
            ),
            Object(MatchingFor("G2ME01"), "rstl/rstl_misc.cpp"),
            Object(Matching, "rstl/rstl_string_member_op.cpp"),
            Object(NonMatching, "rstl/RstlExtras.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Streams/COutputStream.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Streams/CMemoryStreamOut.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Streams/CBitStreamWriter.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Streams/CMemoryInStream.cpp"),
            Object(Matching, "Kyoto/Streams/DolphinCLZOInputStream.cpp"),
            Object(Matching, "Kyoto/Streams/CFilePreload.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Streams/CLZOSupport.cpp", extra_cflags=["-i include/LZO"]),
        ],
    },
    {
        "lib": "LZO",
        "mw_version": "GC/2.7",
        "cflags": cflags_runtime + ["-i include/LZO"],
        "progress_category": "sdk",
        "host": False,
        "objects": [
            Object(MatchingFor("G2ME01"), "LZO/lzo_init.c"),
            Object(MatchingFor("G2ME01"), "LZO/lzo_ptr.c"),
            Object(MatchingFor("G2ME01"), "LZO/lzo1x_d1.c"),
        ],
    },
    {
        "lib": "Runtime.PPCEABI.H",
        "mw_version": config.linker_version,
        "cflags": cflags_runtime,
        "progress_category": "sdk",  # str | List[str]
        "host": False,
        "objects": [
            Object(Matching, "Runtime/global_destructor_chain.c"),
            Object(MatchingFor("G2ME01"), "Runtime/__va_arg.c"),
            Object(MatchingFor("G2ME01"), "Runtime/CPlusLibPPC.cpp"),
            Object(MatchingFor("G2ME01"), "Runtime/ptmf.c"),
            Object(MatchingFor("G2ME01"), "Runtime/runtime.c"),
            Object(Matching, "Runtime/__init_cpp_exceptions.cpp"),
            # TODO: need to implement all
            Object(NonMatching, "Runtime/NMWException.cp",
                   extra_cflags=["-RTTI on", "-Cpp_exceptions on"]),
            Object(NonMatching, "Runtime/Gecko_ExceptionPPC.cp"),
        ],
    },
    {
        "lib": "MSL_C.PPCEABI.bare.H",
        "mw_version": config.linker_version,
        "cflags": [*cflags_runtime, "-DMSL_OLD_FP_CLASSIFY", "-DMSL_NO_INLINE_SQRT"],
        "progress_category": "sdk",
        "host": False,
        "objects": [
            Object(MatchingFor("G2ME01"), "Runtime/arith.c"),
            Object(MatchingFor("G2ME01"), "Runtime/buffer_io.c"),
            Object(MatchingFor("G2ME01"), "Runtime/critical_regions.gamecube.c"),
            Object(MatchingFor("G2ME01"), "Runtime/ctype.c"),
            Object(MatchingFor("G2ME01"), "Runtime/abort_exit.c"),
            Object(MatchingFor("G2ME01"), "Runtime/alloc.c"),
            Object(MatchingFor("G2ME01"), "Runtime/errno.c"),
            Object(MatchingFor("G2ME01"), "Runtime/ansi_files.c"),
            Object(MatchingFor("G2ME01"), "Runtime/ansi_fp.c"),
            Object(MatchingFor("G2ME01"), "Runtime/locale.c"),
            Object(MatchingFor("G2ME01"), "Runtime/direct_io.c"),
            Object(MatchingFor("G2ME01"), "Runtime/file_io.c"),
            Object(MatchingFor("G2ME01"), "Runtime/FILE_POS.c"),
            Object(MatchingFor("G2ME01"), "Runtime/mbstring.c"),
            Object(MatchingFor("G2ME01"), "Runtime/printf.c"),
            Object(MatchingFor("G2ME01"), "Runtime/qsort.c"),
            Object(MatchingFor("G2ME01"), "Runtime/rand.c"),
            Object(MatchingFor("G2ME01"), "Runtime/float.c"),
            Object(MatchingFor("G2ME01"), "Runtime/sscanf.c"),
            Object(MatchingFor("G2ME01"), "Runtime/signal.c"),
            Object(MatchingFor("G2ME01"), "Runtime/strtold.c"),
            Object(MatchingFor("G2ME01"), "Runtime/uart_console_io.c"),
            Object(MatchingFor("G2ME01"), "Runtime/mem.c"),
            Object(MatchingFor("G2ME01"), "Runtime/mem_funcs.c"),
            Object(MatchingFor("G2ME01"), "Runtime/misc_io.c"),
            Object(MatchingFor("G2ME01"), "Runtime/wchar_io.c"),
            Object(MatchingFor("G2ME01"), "Runtime/string.c"),
            Object(MatchingFor("G2ME01"), "Runtime/e_acos.c"),
            Object(MatchingFor("G2ME01"), "Runtime/e_asin.c"),
            Object(MatchingFor("G2ME01"), "Runtime/e_atan2.c"),
            Object(MatchingFor("G2ME01"), "Runtime/e_fmod.c"),
            Object(MatchingFor("G2ME01"), "Runtime/e_log.c"),
            Object(MatchingFor("G2ME01"), "Runtime/e_pow.c"),
            Object(MatchingFor("G2ME01"), "Runtime/e_rem_pio2.c"),
            Object(MatchingFor("G2ME01"), "Runtime/k_cos.c"),
            Object(MatchingFor("G2ME01"), "Runtime/k_rem_pio2.c"),
            Object(MatchingFor("G2ME01"), "Runtime/k_sin.c"),
            Object(MatchingFor("G2ME01"), "Runtime/k_tan.c"),
            Object(MatchingFor("G2ME01"), "Runtime/s_atan.c"),
            Object(MatchingFor("G2ME01"), "Runtime/s_copysign.c"),
            Object(MatchingFor("G2ME01"), "Runtime/s_cos.c"),
            Object(MatchingFor("G2ME01"), "Runtime/s_floor.c"),
            Object(MatchingFor("G2ME01"), "Runtime/s_frexp.c"),
            Object(MatchingFor("G2ME01"), "Runtime/s_ldexp.c"),
            Object(MatchingFor("G2ME01"), "Runtime/s_modf.c"),
            Object(MatchingFor("G2ME01"), "Runtime/s_sin.c"),
            Object(MatchingFor("G2ME01"), "Runtime/s_tan.c"),
            Object(MatchingFor("G2ME01"), "Runtime/w_acos.c"),
            Object(MatchingFor("G2ME01"), "Runtime/w_asin.c"),
            Object(MatchingFor("G2ME01"), "Runtime/w_atan2.c"),
            Object(MatchingFor("G2ME01"), "Runtime/w_fmod.c"),
            Object(MatchingFor("G2ME01"), "Runtime/e_log10.c"),
            Object(MatchingFor("G2ME01"), "Runtime/w_pow.c"),
            Object(MatchingFor("G2ME01"), "Runtime/w_log10.c"),
            Object(MatchingFor("G2ME01"), "Runtime/e_sqrt.c"),
            Object(MatchingFor("G2ME01"), "Runtime/math_ppc.c"),
            Object(MatchingFor("G2ME01"), "Runtime/w_sqrt.c"),
        ],
    },
    DolphinLib(
        "dsp",
        [
            Object(MatchingFor("G2ME01"), "Dolphin/dsp/dsp.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/dsp/dsp_debug.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/dsp/dsp_task.c"),
        ],
    ),
    DolphinLib(
        "dtk",
        [
            Object(MatchingFor("G2ME01"), "Dolphin/dtk.c"),
        ],
    ),
    DolphinLib(
        "dvd",
        [
            Object(MatchingFor("G2ME01"), "Dolphin/dvd/dvdlow.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/dvd/dvdfs.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/dvd/dvd.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/dvd/dvdqueue.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/dvd/dvderror.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/dvd/dvdidutils.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/dvd/dvdfatal.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/dvd/fstload.c"),
        ],
    ),
    DolphinLib(
        "exi",
        [
            Object(MatchingFor("G2ME01"), "Dolphin/exi/EXIBios.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/exi/EXIUart.c"),
        ],
    ),
    DolphinLib(
        "gx",
        [
            Object(MatchingFor("G2ME01"), "Dolphin/gx/GXInit.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/gx/GXFifo.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/gx/GXAttr.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/gx/GXMisc.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/gx/GXGeometry.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/gx/GXFrameBuf.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/gx/GXLight.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/gx/GXTexture.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/gx/GXBump.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/gx/GXTev.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/gx/GXPixel.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/gx/GXDisplayList.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/gx/GXTransform.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/gx/GXPerf.c"),
        ],
    ),
    DolphinLib(
        "mtx",
        [
            Object(MatchingFor("G2ME01"), "Dolphin/mtx/mtx.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/mtx/mtxvec.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/mtx/mtx44.c"),
        ],
    ),
    DolphinLib(
        "pad",
        [
            Object(MatchingFor("G2ME01"), "Dolphin/pad/Pad.c", extra_cflags=["-char unsigned"]),
            Object(MatchingFor("G2ME01"), "Dolphin/pad/PadClamp.c"),
        ],
    ),
    DolphinLib(
        "vi",
        [
            Object(MatchingFor("G2ME01"), "Dolphin/vi/vi.c"),
        ],
    ),
    DolphinLib(
        "ai",
        [
            Object(MatchingFor("G2ME01"), "Dolphin/ai.c"),
        ],
    ),
    DolphinLib(
        "si",
        [
            Object(MatchingFor("G2ME01"), "Dolphin/si/SIBios.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/si/SISamplingRate.c"),
        ],
    ),
    DolphinLib(
        "thp",
        [
            Object(MatchingFor("G2ME01"), "Dolphin/thp/THPDec.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/thp/THPAudio.c"),
        ],
    ),
    DolphinLib(
        "ar",
        [
            Object(Matching, "Dolphin/ar/ar.c"),
            Object(Matching, "Dolphin/ar/arq.c"),
        ],
    ),
    DolphinLib(
        "card",
        [
            Object(Matching, "Dolphin/card/CARDBios.c"),
            Object(Matching, "Dolphin/card/CARDUnlock.c"),
            Object(Matching, "Dolphin/card/CARDRdwr.c"),
            Object(Matching, "Dolphin/card/CARDBlock.c"),
            Object(Matching, "Dolphin/card/CARDDir.c"),
            Object(Matching, "Dolphin/card/CARDCheck.c"),
            Object(Matching, "Dolphin/card/CARDMount.c"),
            Object(Matching, "Dolphin/card/CARDFormat.c"),
            Object(Matching, "Dolphin/card/CARDOpen.c"),
            Object(Matching, "Dolphin/card/CARDCreate.c"),
            Object(Matching, "Dolphin/card/CARDRead.c"),
            Object(Matching, "Dolphin/card/CARDWrite.c"),
            Object(Matching, "Dolphin/card/CARDDelete.c"),
            Object(Matching, "Dolphin/card/CARDStat.c"),
            Object(Matching, "Dolphin/card/CARDRename.c"),
            Object(Matching, "Dolphin/card/CARDStatEx.c"),
            Object(Matching, "Dolphin/card/CARDRaw.c"),
            Object(Matching, "Dolphin/card/CARDNet.c"),
            Object(Matching, "Dolphin/card/CARDErase.c"),
            Object(Matching, "Dolphin/card/CARDProgram.c"),
        ],
    ),
    DolphinLib(
        "base",
        [
            Object(Matching, "Dolphin/PPCArch.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/db.c"),
        ],
    ),
    DolphinLib(
        "os",
        [
            Object(Matching, "Dolphin/os/OSCache.c"),
            Object(Matching, "Dolphin/os/OSContext.c"),
            Object(Matching, "Dolphin/os/OSError.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/os/OSExec.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/os/OSFatal.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/os/OSFont.c", extra_cflags=["-char unsigned"]),
            Object(Matching, "Dolphin/os/OSInterrupt.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/os/OS.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/os/OSAlarm.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/os/OSArena.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/os/OSAudioSystem.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/os/OSLink.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/os/OSMemory.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/os/OSMutex.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/os/OSReboot.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/os/OSReset.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/os/OSResetSW.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/os/OSRtc.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/os/OSSync.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/os/OSThread.c"),
            Object(MatchingFor("G2ME01"), "Dolphin/os/OSTime.c"),
        ],
    ),
    # Begin RELs
    {
        "lib": "REL",
        "mw_version": "GC/2.7",
        "cflags": cflags_rel,
        "progress_category": "game",  # str | List[str]
        "host": False,
        "objects": [
            Object(Matching, "REL/REL_Setup.cpp"),
            Object(
                Matching,
                "REL/global_destructor_chain.c",
                source="Runtime/global_destructor_chain.c",
            ),
        ],
    },
    Rel(
        "ForgottenObject",
        [
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptForgottenObject.cpp"),
        ],
    ),
    Rel(
        "ScriptCannonBall",
        [
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptCannonBall.cpp"),
        ],
    ),
    Rel(
        "Tweaks",
        [
            Object(NonMatching, "MetroidPrime/Tweaks/Tweaks.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakAutoMapper.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakBall.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakPlayerControls.cpp"),
            Object(Matching,    "MetroidPrime/ScriptLoader/SLdrTweakPlayer.cpp"),
            Object(Matching,    "MetroidPrime/ScriptLoader/Structs/SLdrTweakCameraBob_Load.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakCameraBob.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakPlayerGun.cpp"),
            Object(Matching,    "MetroidPrime/ScriptLoader/Structs/SLdrTweakSlideShow_Load.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakSlideShow.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakGame.cpp"),
            Object(Matching,    "MetroidPrime/ScriptLoader/SLdrTweakGuiColors.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakParticle.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakPlayerRes.cpp"),
            Object(Matching,    "MetroidPrime/ScriptLoader/Structs/SLdrTweakTargeting_Load.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakTargeting.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakGui.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/Structs/SLdrTweakPlayerGun_Weapons.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/Structs/SLdrTweakTargeting_VulnerabilityIndicator.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/Structs/SLdrTweakTargeting_Scan.cpp"),
        ],
    ),
    Rel(
        "AIMannedTurret",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptAIMannedTurret.cpp"),
        ],
    ),
    Rel(
        "IngSwarm",
        [
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptIngSwarm.cpp"),
        ],
    ),
    Rel(
        "WallCrawlerSwarm",
        [
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptWallCrawlerSwarm.cpp"),
        ],
    ),
    Rel(
        "Metaree",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptMetaree.cpp"),
        ],
    ),
    Rel(
        "ScriptPlayerActor",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptPlayerActor.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptPlayerActorMain.cpp"),
        ],
    ),
    Rel(
        "SwarmBasics",
        [
            Object(Matching, "MetroidPrime/Enemies/CSwarmBasicsREL.cpp"),
            Object(Matching, "MetroidPrime/Enemies/CSwarmBasicsHealthInfo.cpp"),
            Object(Matching, "MetroidPrime/Enemies/CSwarmBasicsHooks.cpp"),
            Object(Matching, "MetroidPrime/Enemies/CSwarmBasicsOrbitPosition.cpp"),
            Object(Matching, "MetroidPrime/Enemies/CSwarmBasicsCanRender.cpp"),
        ],
    ),
    Rel(
        "ScriptSafeZone",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptSafeZone.cpp"),
        ],
    ),
    Rel(
        "ScriptRiftPortal",
        [
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptRiftPortalPrefix.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptRiftPortal.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptRiftPortalTail.cpp"),
        ],
    ),
    Rel(
        "SkyRipple",
        [
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptSkyRipple.cpp"),
        ],
    ),
    Rel(
        "FlyerSwarm",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CFlyerSwarm.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CFlyerSwarmRel.cpp"),
        ],
    ),
    Rel(
        "ScriptFrontEndDataNetwork",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/ScriptFrontEndDataNetwork.cpp"),
        ],
    ),
    Rel(
        "ScriptPlayerTurret",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptPlayerTurretRel.cpp"),
        ],
    ),
    Rel(
        "ScriptStreamedMovie",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptScriptStreamedMovie.cpp"),
        ],
    ),
    Rel(
        "RubiksPuzzle",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptRubiksPuzzle.cpp"),
        ],
    ),
    Rel(
        "ScriptPlayerProxy",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptPlayerProxy.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptPlayerProxyAccessors.cpp"),
        ],
    ),
    Rel(
        "ScriptRsfAudio",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptRsfAudio.cpp"),
        ],
    ),
    # Restored 2026-09-25: these three Rel blocks were lost by later commits that copied an older
    # configure.py - Puffer's block was replaced by WallCrawler's own (33b73a3), and WallCrawler's
    # and ScriptGui's were dropped later (f599488, "ScriptGui's loader registration"). Their sources
    # have been sitting in src/ unbuilt since, which is why the report showed them at 0.00%: those
    # units existed only because config.yml lists every retail module, not because anything of ours
    # was compiled or linked into them.
    Rel(
        "Puffer",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptPuffer.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptPufferRel.cpp"),
        ],
    ),
    Rel(
        "WallCrawler",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptWallCrawler_Rest.cpp"),
        ],
    ),
    Rel(
        "ScriptGui",
        [
            Object(NonMatching, "MetroidPrime/ScriptObjects/ScriptGuiPrefix.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/ScriptGuiSetup.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/ScriptGuiTail.cpp"),
        ],
    ),
    Rel(
        "ScriptCoin",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptCoinRel.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptCoin.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptCoinThink.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptCoinTouchBounds.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptCoinRest.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptCoinTail.cpp"),
        ],
    ),
]


# Optional callback to adjust link order. This can be used to add, remove, or reorder objects.
# This is called once per module, with the module ID and the current link order.
#
# For example, this adds "dummy.c" to the end of the DOL link order if configured with --non-matching.
# "dummy.c" *must* be configured as a Matching (or Equivalent) object in order to be linked.
def link_order_callback(module_id: int, objects: List[str]) -> List[str]:
    # Don't modify the link order for matching builds
    if not config.non_matching:
        return objects
    if module_id == 0:  # DOL
        return objects + ["dummy.c"]
    return objects


# Uncomment to enable the link order callback.
# config.link_order_callback = link_order_callback


# Optional extra categories for progress tracking
# Adjust as desired for your project
config.progress_categories = [
    ProgressCategory("game", "Game Code"),
    ProgressCategory("sdk", "SDK Code"),
]
config.progress_each_module = args.verbose
# Optional extra arguments to `objdiff-cli report generate`
config.progress_report_args = [
    # Marks relocations as mismatching if the target value is different
    # Default is "functionRelocDiffs=none", which is most lenient
    # "--config functionRelocDiffs=data_value",
]
config.extra_clang_flags = ["-DCLANGD"]

if args.mode == "configure":
    # Write build.ninja and objdiff.json
    generate_build(config)
elif args.mode == "progress":
    # Print progress information
    calculate_progress(config)
else:
    sys.exit("Unknown mode: " + args.mode)
