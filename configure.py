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
            # `main.cpp`'s retail range is **cut six ways**, and each cut is a `splits.txt` range
            # on the same file: main.cpp 0x800053B8-0x80006B38, CMainFillInAssetIDs.cpp (Matching)
            # 0x80006B38-0x80006B80, mainMid.cpp 0x80006B80-0x8000848C, CGameGlobalObjectsCtor.cpp
            # (Matching) 0x8000848C-0x80008570, CMainShutdownSubsystems.cpp (Matching)
            # 0x80008570-0x80008680, and mainTail.cpp 0x80008680-0x80009880 plus .ctors and .sbss.
            # One unit may not claim two ranges in a section (dtk: link-order cycle), and .ctors/
            # .sbss must sit on the last of the six (dtk: "Mismatched splits for .ctors").
            # mainTail.cpp's header has the reasoning, and mainMid.cpp's has the rest of it.
            Object(
                NonMatching,
                "MetroidPrime/main.cpp",
                extra_cflags=['-pragma "inline_max_size(125)"'] if config.version == "G2ME01" else [],
            ),
            # The 72-byte carve at 0x80006B38, and the two units either side of it. All three
            # lines are ONE LINE EACH ON PURPOSE: `tools/flip_test.sh` marks a unit Matching with
            # a literal `s.replace('Object(NonMatching, "<unit>"', ...)`, so a wrapped
            # `Object(\n    NonMatching,\n    "<unit>"` entry is never actually flipped and the
            # test reports PASS having tested nothing.
            Object(NonMatching, "MetroidPrime/mainMid.cpp", extra_cflags=['-pragma "inline_max_size(125)"'] if config.version == "G2ME01" else []),
            Object(Matching, "MetroidPrime/CMainFillInAssetIDs.cpp", extra_cflags=['-pragma "inline_max_size(125)"'] if config.version == "G2ME01" else []),
            Object(
                NonMatching,
                "MetroidPrime/mainTail.cpp",
                extra_cflags=['-pragma "inline_max_size(125)"'] if config.version == "G2ME01" else [],
            ),
            # `CMain::ShutdownSubsystems` (0x80008570, 0x110 = 272 B) carved out of mainTail.cpp's
            # 0x80008570-0x80009880 so that mainTail can start at 0x80008680. The reason is
            # `CMain::InitializeSubsystems` (0x80008680): a `Matching` carve for it needs
            # 0x80008570..0x800087DC, and one unit may not claim two discontiguous ranges in a
            # section, so this function has to be a unit of its own before that carve is possible.
            # mainTail.cpp's header records the split.
            Object(Matching, "MetroidPrime/CMainShutdownSubsystems.cpp"),
            Object(Matching, "MetroidPrime/CGameGlobalObjectsCtor.cpp"),
            # Six single retail functions carved out of dtk `auto_03_*` ranges, one file and one
            # `splits.txt` range each. A unit may not claim two discontiguous ranges in a section
            # (`dtk dol split`: "Cyclic dependency ... link order"), so the carve is one function
            # per unit - which is also the shape `linked` wants, since each is its own `Matching`
            # object in the port's build.
            #   CAxisAngle::GetVector       0x8001D2BC  4 B  (a `blr`: a reference return)
            #   CPatterned::VSlot70         0x80073D0C  8 B  (`return 0`)
            #   CPatterned::VSlot72         0x80073CD0  4 B  (an empty `void`)
            #   CAi::CanBeShot              0x800358D8  8 B  (`return true`)
            #   CARAMManager::GetInvalidAlloc 0x80301710 8 B (the -1 ARAM sentinel)
            #   CARAMManager::IsAllocValid  0x80301718 20 B  (`ptr != kInvalidAlloc`)
            Object(Matching, "MetroidPrime/CAxisAngleGetVector.cpp"),
            Object(Matching, "MetroidPrime/Enemies/CPatternedVSlot70.cpp"),
            Object(Matching, "MetroidPrime/Enemies/CPatternedVSlot72.cpp"),
            Object(Matching, "MetroidPrime/Enemies/CAiCanBeShot.cpp"),
            Object(Matching, "Kyoto/CARAMManagerGetInvalidAlloc.cpp"),
            Object(Matching, "Kyoto/CARAMManagerIsAllocValid.cpp"),
            # Five more carves from the same vein, nine functions in total. The two METROTRK
            # files are `.c`, so their names stay unmangled the way `symbols.txt` has them
            # (`__read_console`, not a mangled form), and they are the SDK's no-op trace hooks:
            # an empty `void` is a bare `blr`, a `return 0` is `li r3,0; blr`.
            #   MetroTRK console+init  0x80003840  0x18 = 4 functions, ONE contiguous claim
            #   InitMetroTRK_BBA        0x803765C4  4 B
            #   SetLoader_CannonBall    0x8021FAB4  8 B   (the 5th REL loader setter)
            #   CGameArea::SetAreaAttributes 0x80055768 12 B
            #   CGunWeapon::IsLoaded    0x801D9B24 12 B
            Object(Matching, "Runtime/MetroTRKConsoleStubs.cpp"),
            Object(Matching, "Runtime/InitMetroTRKBba.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/CannonBallLoaderSet.cpp"),
            Object(Matching, "MetroidPrime/CGameAreaSetAreaAttributes.cpp"),
            Object(Matching, "MetroidPrime/Weapons/CGunWeaponIsLoaded.cpp"),
            # --- carve2: generated carve units
            Object(NonMatching, "Kyoto/Math/Carve8001994C.cpp"),
            Object(Matching, "MetroidPrime/Player/Carve8000B7C4.c"),
            Object(Matching, "MetroidPrime/Player/Carve8000EC00.c"),
            Object(Matching, "MetroidPrime/Player/Carve80010F48.c"),
            Object(Matching, "MetroidPrime/Carve8001935C.c"),
            Object(Matching, "MetroidPrime/Carve8001FF7C.c"),
            Object(Matching, "MetroidPrime/Carve800208E8.c"),
            Object(Matching, "rstl/Carve800239F4.c"),
            Object(Matching, "rstl/Carve80025D24.c"),
            Object(Matching, "rstl/Carve80025E08.c"),
            Object(Matching, "Kyoto/Math/Carve80031414.c"),
            Object(Matching, "Kyoto/Math/Carve80031AC8.c"),
            Object(Matching, "MetroidPrime/Enemies/Carve800358E0.c"),
            Object(Matching, "MetroidPrime/Carve80045CD4.c"),
            Object(Matching, "MetroidPrime/Carve80049E20.c"),
            Object(Matching, "MetroidPrime/Carve8005065C.c"),
            Object(Matching, "MetroidPrime/Carve800534B0.c"),
            Object(Matching, "MetroidPrime/Carve80053594.c"),
            Object(Matching, "MetroidPrime/Carve80054F74.c"),
            Object(Matching, "MetroidPrime/Carve80055990.c"),
            Object(Matching, "MetroidPrime/Carve8006653C.c"),
            Object(Matching, "MetroidPrime/Carve8006CB00.c"),
            Object(Matching, "MetroidPrime/Carve8007062C.c"),
            Object(Matching, "MetroidPrime/Carve80071498.c"),
            Object(Matching, "MetroidPrime/Carve80073594.c"),
            Object(Matching, "MetroidPrime/Enemies/Carve80073F50.c"),
            Object(Matching, "MetroidPrime/Enemies/Carve80074774.c"),
            Object(Matching, "MetroidPrime/Enemies/Carve800766CC.c"),
            Object(Matching, "MetroidPrime/Enemies/Carve8007C208.c"),
            Object(Matching, "MetroidPrime/Enemies/Carve8008181C.c"),
            Object(Matching, "MetroidPrime/Enemies/Carve800836B0.c"),
            Object(Matching, "MetroidPrime/Enemies/Carve80083FD8.c"),
            Object(Matching, "MetroidPrime/Carve800A1598.c"),
            Object(Matching, "MetroidPrime/Carve800B243C.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve800B7438.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve800BDA98.c"),
            Object(Matching, "MetroidPrime/Player/Carve800C1908.c"),
            Object(Matching, "MetroidPrime/Player/Carve800D85A0.c"),
            Object(Matching, "MetroidPrime/Player/Carve800DAE94.c"),
            Object(Matching, "MetroidPrime/Carve800E39D0.c"),
            Object(Matching, "MetroidPrime/Carve800E8E4C.c"),
            Object(Matching, "MetroidPrime/Carve800EC508.c"),
            Object(Matching, "MetroidPrime/Carve800EC978.c"),
            Object(Matching, "MetroidPrime/Carve800ED550.c"),
            Object(Matching, "MetroidPrime/Carve800ED604.c"),
            Object(Matching, "MetroidPrime/Carve800F0198.c"),
            Object(Matching, "MetroidPrime/Carve800F1234.c"),
            Object(Matching, "MetroidPrime/Carve800F1264.c"),
            Object(Matching, "MetroidPrime/Carve800F15C8.c"),
            Object(Matching, "MetroidPrime/Carve800F1A18.c"),
            Object(Matching, "MetroidPrime/Carve800F2390.c"),
            Object(Matching, "MetroidPrime/Carve800F24AC.c"),
            Object(Matching, "MetroidPrime/Carve800F2C54.c"),
            Object(Matching, "MetroidPrime/Carve800F4E28.c"),
            Object(Matching, "MetroidPrime/Carve800F5004.c"),
            Object(Matching, "MetroidPrime/Carve800F51EC.c"),
            Object(Matching, "MetroidPrime/Carve800F549C.c"),
            Object(Matching, "MetroidPrime/Carve800F609C.c"),
            Object(Matching, "MetroidPrime/Carve800F794C.c"),
            Object(Matching, "MetroidPrime/Carve800F7B80.c"),
            Object(Matching, "MetroidPrime/Carve800F8BDC.c"),
            Object(Matching, "MetroidPrime/Carve800FAC18.c"),
            Object(Matching, "MetroidPrime/Carve800FACE0.c"),
            Object(Matching, "MetroidPrime/Carve800FAFE8.c"),
            Object(Matching, "MetroidPrime/Carve800FB66C.c"),
            Object(Matching, "MetroidPrime/Carve800FBEF0.c"),
            Object(Matching, "MetroidPrime/Carve800FBF68.c"),
            Object(Matching, "MetroidPrime/Carve800FC474.c"),
            Object(Matching, "MetroidPrime/Carve800FD2D0.c"),
            Object(Matching, "MetroidPrime/Carve800FD648.c"),
            Object(Matching, "MetroidPrime/Carve800FEE98.c"),
            Object(Matching, "MetroidPrime/Carve800FEF78.c"),
            Object(Matching, "MetroidPrime/Carve800FF164.c"),
            Object(Matching, "MetroidPrime/Carve800FF294.c"),
            Object(Matching, "MetroidPrime/Carve800FFC2C.c"),
            Object(Matching, "MetroidPrime/Carve800FFD34.c"),
            Object(Matching, "MetroidPrime/Carve801007D8.c"),
            Object(Matching, "MetroidPrime/Carve80107994.c"),
            Object(Matching, "MetroidPrime/Carve8010805C.c"),
            Object(Matching, "MetroidPrime/Carve8010EE54.c"),
            Object(Matching, "MetroidPrime/Carve801174E8.c"),
            Object(Matching, "MetroidPrime/Carve801184E8.c"),
            Object(Matching, "MetroidPrime/Carve801185AC.c"),
            Object(Matching, "MetroidPrime/Carve8011A97C.c"),
            Object(Matching, "MetroidPrime/Carve801255B8.c"),
            Object(Matching, "MetroidPrime/Carve80127E7C.c"),
            Object(Matching, "MetroidPrime/Carve8012CB4C.c"),
            Object(Matching, "MetroidPrime/Carve8012CD10.c"),
            Object(Matching, "MetroidPrime/Carve8012D164.c"),
            Object(Matching, "MetroidPrime/Player/Carve801476D0.c"),
            Object(Matching, "MetroidPrime/Player/Carve80149108.c"),
            Object(Matching, "MetroidPrime/Player/Carve80149288.c"),
            Object(Matching, "MetroidPrime/Player/Carve8014A5DC.c"),
            Object(Matching, "MetroidPrime/Player/Carve8014A6D4.c"),
            Object(Matching, "MetroidPrime/Player/Carve8014FFCC.c"),
            Object(Matching, "MetroidPrime/Player/Carve8015180C.c"),
            Object(Matching, "MetroidPrime/Player/Carve8015294C.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve8015DF08.c"),
            Object(Matching, "MetroidPrime/CEnvFxManagerInitialize.cpp"),
            Object(Matching, "MetroidPrime/Player/Carve80168498.c"),
            Object(Matching, "MetroidPrime/Player/Carve8016BDE4.c"),
            Object(Matching, "MetroidPrime/Carve80171DD4.c"),
            Object(Matching, "MetroidPrime/Carve80179E08.c"),
            Object(Matching, "MetroidPrime/Carve8018C4A8.c"),
            Object(Matching, "MetroidPrime/Carve8018EF50.c"),
            Object(Matching, "MetroidPrime/Carve80192D74.c"),
            Object(Matching, "MetroidPrime/Carve80193C30.c"),
            Object(Matching, "MetroidPrime/Carve80193C54.c"),
            Object(Matching, "MetroidPrime/Carve80193C7C.c"),
            Object(Matching, "MetroidPrime/Carve80193D9C.c"),
            Object(Matching, "MetroidPrime/Carve80193DB8.c"),
            Object(Matching, "MetroidPrime/Carve80193E04.c"),
            Object(Matching, "MetroidPrime/Carve80193E08.c"),
            Object(Matching, "MetroidPrime/Carve80194BF0.c"),
            Object(Matching, "MetroidPrime/Carve801956B4.c"),
            Object(Matching, "MetroidPrime/Carve80195FEC.c"),
            Object(Matching, "MetroidPrime/Carve80196530.c"),
            Object(Matching, "MetroidPrime/Carve801997B0.c"),
            Object(Matching, "MetroidPrime/Carve8019CE6C.c"),
            Object(Matching, "MetroidPrime/Carve8019E368.c"),
            Object(Matching, "MetroidPrime/Carve801A7C18.c"),
            Object(Matching, "MetroidPrime/Carve801ABD0C.c"),
            Object(Matching, "MetroidPrime/Carve801ADD2C.c"),
            Object(Matching, "MetroidPrime/Carve801AE9C4.c"),
            Object(Matching, "MetroidPrime/Carve801AEE0C.c"),
            Object(Matching, "MetroidPrime/Carve801B02DC.c"),
            Object(Matching, "MetroidPrime/Carve801B0624.c"),
            Object(Matching, "MetroidPrime/Carve801B1A6C.c"),
            Object(Matching, "MetroidPrime/Carve801B2E38.c"),
            Object(Matching, "MetroidPrime/Carve801B3B44.c"),
            Object(Matching, "MetroidPrime/Carve801B5694.c"),
            Object(Matching, "MetroidPrime/Carve801B9420.c"),
            Object(Matching, "MetroidPrime/Carve801B94B4.c"),
            Object(Matching, "MetroidPrime/Carve801BC900.c"),
            Object(Matching, "MetroidPrime/Carve801C128C.c"),
            Object(Matching, "MetroidPrime/Carve801C13F4.c"),
            Object(Matching, "MetroidPrime/Carve801C3670.c"),
            Object(Matching, "MetroidPrime/Carve801C36A8.c"),
            Object(Matching, "MetroidPrime/Carve801C3718.c"),
            Object(Matching, "MetroidPrime/Carve801C6DF0.c"),
            Object(Matching, "MetroidPrime/Player/Carve801D3B88.c"),
            Object(Matching, "MetroidPrime/Weapons/Carve801D5E9C.c"),
            Object(Matching, "MetroidPrime/Weapons/Carve801D688C.c"),
            Object(Matching, "MetroidPrime/Weapons/Carve801D6920.c"),
            Object(Matching, "MetroidPrime/Weapons/Carve801D6930.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801E2AE8.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801E3E34.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801E8AEC.c"),
            Object(Matching, "MetroidPrime/Carve801F3690.c"),
            Object(Matching, "MetroidPrime/Carve801F36DC.c"),
            Object(Matching, "MetroidPrime/Carve801F7AC8.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FEEF0.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FF4A4.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80201418.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve80212278.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve802126B0.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve80212944.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve802129A4.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve80212A24.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80226B3C.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80226B60.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80226C78.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80226C9C.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80226CB8.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80226CC8.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80229410.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80229568.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80229BBC.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve8022A3F4.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve8022D758.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve8022DA40.c"),
            Object(Matching, "MetroidPrime/Carve802476D8.c"),
            Object(Matching, "MetroidPrime/Carve8026040C.c"),
            Object(Matching, "MetroidPrime/Carve8026F624.c"),
            Object(Matching, "MetroidPrime/Carve8027409C.c"),
            Object(Matching, "MetroidPrime/Carve802740B0.c"),
            Object(Matching, "MetroidPrime/Carve80274774.c"),
            Object(Matching, "MetroidPrime/Carve80274C1C.c"),
            Object(Matching, "MetroidPrime/Carve80275F24.c"),
            Object(Matching, "MetroidPrime/Carve80276568.c"),
            Object(Matching, "MetroidPrime/Carve80276AD8.c"),
            Object(Matching, "MetroidPrime/Carve80277090.c"),
            Object(Matching, "MetroidPrime/Carve80278568.c"),
            Object(Matching, "MetroidPrime/Carve80278C74.c"),
            Object(Matching, "MetroidPrime/Carve80279250.c"),
            Object(Matching, "MetroidPrime/Carve8027A1C4.c"),
            Object(Matching, "MetroidPrime/Carve8027A53C.c"),
            Object(Matching, "MetroidPrime/Carve8027D844.c"),
            Object(Matching, "MetroidPrime/Carve8027DC1C.c"),
            Object(Matching, "MetroidPrime/Carve8027E404.c"),
            Object(Matching, "MetroidPrime/Carve80280338.c"),
            Object(Matching, "Kyoto/Basics/Carve8028C058.c"),
            Object(Matching, "Kyoto/Basics/Carve8028EFD4.c"),
            Object(Matching, "Kyoto/Basics/Carve802905AC.c"),
            Object(Matching, "Kyoto/Basics/Carve802940D0.c"),
            Object(Matching, "Kyoto/Basics/Carve80294814.c"),
            Object(Matching, "Kyoto/Basics/Carve80294F98.c"),
            Object(Matching, "Kyoto/Basics/Carve80295584.c"),
            Object(Matching, "Kyoto/Basics/Carve80295B98.c"),
            Object(Matching, "Kyoto/Basics/Carve8029624C.c"),
            Object(Matching, "Kyoto/Basics/Carve80296468.c"),
            Object(Matching, "Kyoto/Basics/Carve80296D08.c"),
            Object(Matching, "Kyoto/Basics/Carve80296D94.c"),
            Object(Matching, "Kyoto/Basics/Carve8029AB1C.c"),
            Object(Matching, "Kyoto/Basics/Carve8029BC5C.c"),
            Object(Matching, "MetroidPrime/Carve8029F084.c"),
            Object(Matching, "MetroidPrime/Carve8029FA58.c"),
            Object(Matching, "MetroidPrime/Carve802A32D8.c"),
            Object(Matching, "MetroidPrime/Carve802A5E20.c"),
            Object(Matching, "MetroidPrime/Carve802A7C78.c"),
            Object(Matching, "MetroidPrime/Carve802AE9BC.c"),
            Object(Matching, "Kyoto/Animation/Carve802B2088.c"),
            Object(Matching, "Kyoto/Animation/Carve802B2568.c"),
            Object(Matching, "Kyoto/Carve802B3B7C.c"),
            Object(Matching, "Kyoto/Carve802B4A04.c"),
            Object(Matching, "Kyoto/Carve802B4D30.c"),
            Object(Matching, "Kyoto/Carve802B9D50.c"),
            Object(Matching, "Kyoto/Carve802BAD08.c"),
            Object(Matching, "Kyoto/Graphics/Carve802BE7E4.c"),
            Object(Matching, "Kyoto/PVS/Carve802E8304.c"),
            Object(Matching, "Kyoto/PVS/Carve802F2268.c"),
            Object(Matching, "Kyoto/PVS/Carve802F27B0.c"),
            Object(Matching, "Kyoto/PVS/Carve802F358C.c"),
            Object(Matching, "Kyoto/PVS/Carve802F3D3C.c"),
            Object(Matching, "Kyoto/PVS/Carve802F74A4.c"),
            Object(Matching, "Kyoto/Carve80300998.c"),
            Object(Matching, "Kyoto/Graphics/Carve8031B6EC.c"),
            Object(Matching, "Kyoto/Math/Carve8032C144.c"),
            Object(Matching, "Kyoto/Math/Carve8032E444.c"),
            Object(Matching, "Kyoto/Math/Carve803359F4.c"),
            Object(Matching, "Kyoto/Math/Carve80335A14.c"),
            Object(Matching, "Kyoto/Math/Carve80335A3C.c"),
            Object(Matching, "Kyoto/Math/Carve80335A5C.c"),
            Object(Matching, "Kyoto/Math/Carve80335A8C.c"),
            Object(Matching, "Kyoto/Math/Carve80335AB0.c"),
            Object(Matching, "Kyoto/Math/Carve80335AE0.c"),
            Object(Matching, "Kyoto/Math/Carve80335B10.c"),
            Object(Matching, "Kyoto/Math/Carve80335B38.c"),
            Object(Matching, "Kyoto/Math/Carve80335B58.c"),
            Object(Matching, "Kyoto/Math/Carve80337198.c"),
            Object(Matching, "Kyoto/Math/Carve803371A4.c"),
            Object(Matching, "Kyoto/Math/Carve803371F4.c"),
            Object(Matching, "Kyoto/Math/Carve80339D1C.c"),
            Object(Matching, "Kyoto/Math/Carve8033BE1C.c"),
            Object(Matching, "Kyoto/Math/Carve8033F2CC.c"),
            Object(Matching, "Kyoto/Math/Carve803414FC.c"),
            Object(Matching, "Dolphin/Carve8038A7DC.c"),
            Object(Matching, "Dolphin/Carve80397B78.c"),
            Object(Matching, "Dolphin/Carve8039E164.c"),
            Object(Matching, "Dolphin/Carve803A1B28.c"),
            Object(Matching, "Dolphin/Carve803A1D2C.c"),
            Object(Matching, "Dolphin/Carve803A2324.c"),
            Object(Matching, "Dolphin/Carve803A2FD0.c"),

            # Four CCubeRenderer carves from dtk's unclaimed auto_03_8026258C_text range, 15
            # functions. The port's frame loop draws through gpRender's vtable
            # (docs/research/boot_path.md step 21c) and these are fifteen of its slots.
            #   Carve8026E7F0  0x8026E7E4..0x8026E9B8  468 B  11: SetDrawableCallback, GetFPS,
            #                                                    the eight SetBlendMode_*,
            #                                                    SetDepthReadWrite
            #   Carve8026EC54  0x8026EC54..0x8026EC78   36 B   1: SetAmbientColor
            #   Carve8026ECDC  0x8026ECDC..0x8026ECF8   28 B   1: PrimNormal
            #   Carve8026EF24  0x8026EF24..0x8026EF54   48 B   2: PrimColor x2
            # Four units and not fewer, because one unit may not claim two discontiguous
            # `.text` ranges in a section, and not more, because four adjacent functions are
            # one claim worth four. The five `Begin*` methods at 0x8026ED44..0x8026EE0C are
            # the obvious next unit and are **blocked on a virtual call**, not on a body -
            # see include/MetaRender/CCubeRenderer.hpp and the vtable map in
            # docs/research/boot_path.md's step 21c.
            Object(Matching, "MetaRender/Carve8026E7F0.cpp"),
            Object(Matching, "MetaRender/Carve8026EC54.cpp"),
            Object(Matching, "MetaRender/Carve8026ECDC.cpp"),
            Object(Matching, "MetaRender/Carve8026EF24.cpp"),
            # AllocateRenderer, retail 0x8026EF54, 0x9C = 156 bytes. **This is the function
            # that makes gpRender non-null**: CGameGlobalObjects::PostInitialize (already
            # Matching) calls it and stores the result into the .sbss pointer the frame loop
            # dereferences at boot_path.md step 21c. Starts at exactly the end of
            # Carve8026EF24, so the two units are adjacent. It also claims the 8-byte .sdata
            # word at 0x80418998 it stores `p ? p+4 : p` into.
            #
            # **NonMatching, and the reason is structural.** The unit is 39 instructions and
            # 8 of its 156 bytes are the second argument, a pointer to `.rodata` 0x803AE412 that
            # this tree cannot own: the only spelling that reproduces `addi r3,r7,-7236 ;
            # addi r4,r3,86` needs a symbol at 0x803AE3BC, dtk will only let a claim end on a
            # symbol boundary so the whole 0xFC bytes are this unit's, and 0x803AE3BC is 4 (mod 8)
            # while every MWCC data input section is 8-aligned - so mwldeppc moves the object four
            # bytes and 6,651 .rodata bytes and 856 .text bytes of main.dol stop matching retail.
            # Measured, with the numbers, in the file's header. The body is retail's and it is
            # real, which is what the port needs; `Matching` would mean shipping a broken DOL.
            Object(NonMatching, "MetaRender/Carve8026EF54.cpp"),
            # Three more single-function carves in the same area, each a different class:
            #   MetaRender/Carve8026FDEC   0x8026FDEC  36 B  CCubeRenderer::SetModelMatrix
            #   Kyoto/Graphics/Carve802C4248 0x802C4248 20 B CTexture::InvalidateTexmap
            #   Kyoto/Graphics/Carve802BEC1C 0x802BEC1C  8 B CGraphics::GetUseVideoFilter
            Object(Matching, "MetaRender/Carve8026FDEC.cpp"),
            # `CCubeRenderer`'s constructor, 0x80271238, 0x59C = 1436 bytes. NonMatching
            # for three measured reasons, all in the file header and none of them a
            # missing spelling: `include/MetaRender/CCubeRenderer.hpp` is 516 bytes short
            # (0x35C against the 0x560 retail's own `li r3,1376` implies), the three
            # `.data` vtable addresses it stores are unowned, and the mem-init/body split
            # puts the stores in the wrong order. It is what makes `gpRender` a constructed
            # object rather than 1376 uninitialised bytes, so it is listed for the port even
            # though dtk links retail's object for the DOL.
            # CCubeRenderer, its other three retail functions. Measured 2026-09-27 (lane
            # `render2b`); the previous session measured the same win and reverted it on a
            # misdiagnosis - see the note on Carve8026FBFC.cpp below and docs/HANDOFF.md.
            #
            # `fn_80272958`, retail 0x80272958, 0x30 = 48 B. **Byte-exact**: 2 bytes differ, the
            # top halves of two `bl` displacements, both carrying the right R_PPC_REL24. It is
            # `Matching`, it is in `files.cmake`, and it is the only unit of the four that
            # contributes to `matched` and `linked` without any risk to the DOL.
            Object(Matching, "MetaRender/Carve80272958.c"),
            # **`CCubeRenderer::BeginScene`, retail 0x8026FBFC, 0x180 = 384 B, and `NonMatching`
            # because it CANNOT be `Matching` - measured, not guessed.** Its `.text` is retail's
            # byte for byte (objdiff 100.00%, 1/1; the 64 differing bytes in a raw compare are
            # all the low half of a four-byte relocated field, and its length is retail's), and
            # the object has NO `.sdata2` section at all. Declaring it `Matching` still breaks
            # the build: mwldeppc then attributes **0x14 = 20 bytes of `.sdata2` at 0x8041E250**
            # to `Carve8026FBFC.o` (visible in `build/G2ME01/main.elf.MAP`), which is where
            # `CStopwatch.o`'s own 8 bytes sit in a link without it. `.sdata2` grows
            # 0x54C0 -> 0x54E0, `main.dol` grows by 32 bytes, and **43 of the 86 RELs lose
            # their hashes**. Reproduced twice, one line at a time; the same claim in
            # `splits.txt` alone is harmless, and so are `Carve80270848` and `Carve80272958`.
            # So the cost of `Matching` here is 32 bytes of DOL and 43 broken module hashes
            # against +1 `matched` and +1 `linked`. Declined, and the unit is still compiled and
            # still measured, so the port gets the body and the vtable slot is filled.
            Object(NonMatching, "MetaRender/Carve8026FBFC.cpp"),
            # **`~CCubeRenderer`, retail 0x80270848, 0x220 = 544 B, and the key function** - the
            # only thing anywhere that emits `vtable for CCubeRenderer`, without which every
            # `gpRender->` virtual on the host is a jump to 0. It is `NonMatching` at 82.96%
            # and its object is 0x21C, four bytes short of its claim, so it contributes 0 to
            # both counts. It is measured safe for the DOL: with it declared, `main.dol` is
            # byte-identical to retail and all 86 RELs hold.
            Object(NonMatching, "MetaRender/Carve80270848.cpp"),
            Object(NonMatching, "MetaRender/Carve80271238.cpp"),
            Object(Matching, "Kyoto/Graphics/Carve802C4248.cpp"),
            # Five more `CGraphics` members, all written as one-function carves out of dtk's
            # unclaimed `auto_03_*` ranges. `CGraphics` has no `.cpp` anywhere in the tree, so
            # each is a file of its own and reaches its guest globals by their dtk labels; the
            # PC-side storage for those is in `src/Kyoto/Graphics/CGraphicsHostGlobals.cpp`
            # (port-only) and `src/MetroidPrime/PortGlobals.cpp`. All five are `static` members,
            # so r3/f1/f2/r4 are the declared arguments on entry and `this` never appears -
            # which is what makes the bodies as short as retail's.
            #   Carve802BE8F0  0x802BE8F0  180 B  SetScreenPosition(int,int,int)
            #   Carve802BEC24  0x802BEC24   72 B  SetUseVideoFilter(bool)
            #   Carve802BF59C  0x802BF59C   12 B  GetProjectionState()
            #   Carve802BF9C8  0x802BF9C8   48 B  SetFog(mode, near, far, const CColor&)
            #   Carve802C24AC  0x802C24AC   96 B  SetModelMatrix(const CTransform4f&)
            Object(Matching, "Kyoto/Graphics/Carve802BE8F0.cpp"),
            Object(Matching, "Kyoto/Graphics/Carve802BEC24.cpp"),
            Object(Matching, "Kyoto/Graphics/Carve802BF59C.cpp"),
            Object(Matching, "Kyoto/Graphics/Carve802BF9C8.cpp"),
            Object(Matching, "Kyoto/Graphics/Carve802C24AC.cpp"),
            Object(Matching, "Kyoto/Graphics/Carve802BEC1C.cpp"),
            # fn_802C2614, 0x802C2614, 156 B: the shared tail of `SetModelMatrix` and
            # `SetViewPointMatrix` - compose model*view and push it (and, when the normal-matrix
            # latch is set, its inverse-transpose) into GX_PNMTX0. It is the one symbol
            # `Carve802C24AC.cpp` relocates against and nothing defined, which is the whole
            # reason that Matching unit is not listed. A `.c`, because retail's name is
            # `fn_802C2614` and a C++ definition would mangle and pair nothing.
            Object(Matching, "Kyoto/Graphics/Carve802C2614.c"),
            Object(Matching, "MetroidPrime/CCallStack.cpp"),
            # `CGraphics::SetViewPointMatrix(const CTransform4f&)`, retail
            # `SetViewPointMatrix__9CGraphicsFRC12CTransform4f` at 0x802C2534, 0xE0 = 224 bytes:
            # store the matrix, then build the transposed rotation and the translation by hand
            # for GX and hand the result to `fn_802C2614`. Sits in the 0x108-byte unclaimed gap
            # between Carve802C24AC.cpp's end (0x802C250C) and fn_802C2614, and is the other
            # half of the pair the port's link asks for by name.
            # **NonMatching at 99.11% - 10 wrong bytes, all of them float register fields.**
            # Retail's source used a pooled literal `0.f` for the Mtx's zero column; reading the
            # guest `lbl_8041E508` instead makes MWCC create that temporary first, so it takes
            # f12 and the m[i][0] triple drops to f11/f10/f9. Claiming the 4-byte .sdata2 does
            # not rescue it, because that removes dtk's `lbl_8041E508` definition, which five
            # other unclaimed retail functions reference by name. The file's header has the
            # measurement; do not spend a cycle re-trying either spelling.
            Object(NonMatching, "Kyoto/Graphics/Carve802C2534.cpp"),
            # The two small member constructors CGameGlobalObjects' constructor calls, both
            # unnamed in symbols.txt and both on the port's boot path (step 7):
            # fn_8016C230 (CInGameTweakManager, 0x14) and fn_801F0A44 (the +0x150 member, 0x30).
            Object(Matching, "MetroidPrime/CInGameTweakManagerCtor.cpp"),
            Object(Matching, "MetroidPrime/CGameGlobalObjectsTailCtor.cpp"),
            # CCharacterFactoryBuilder (CGameGlobalObjects+0x108) and its CDummyFactory, retail
            # 0x80031E60..0x80032230. The port needs CDummyFactory's vtable because the
            # constructor stores it; see the file's header.
            Object(NonMatching, "MetroidPrime/Factories/CCharacterFactoryBuilder.cpp"),
            Object(NonMatching, "MetroidPrime/CStateManager.cpp"),
            Object(NonMatching, "MetroidPrime/CEntity.cpp"),
            Object(NonMatching, "MetroidPrime/TypesMatch.cpp"),
            Object(Matching, "MetroidPrime/CIOWinCtor.cpp"),
            # CErrorOutputWindow's constructor, retail `__ct__18CErrorOutputWindowFb` at
            # 0x8018169C, 0xB4 = 180 bytes. It is one of the three functions the port's step-17
            # stop message names, so it is on the boot path; see the file's header for why it
            # declares its own class (the class has no key function, so there is no vtable for
            # mwcceppc to emit and the derived-vptr store is written against retail's gap object).
            Object(NonMatching, "MetroidPrime/CErrorOutputWindowCtor.cpp"),
            # CSimpleShadow::GetTransform, retail 0x800DF478, 4 bytes, and the first function
            # carved out of a dtk `auto_03_*` range into a `Matching` unit of its own. It is
            # here to be cheap: a four-byte accessor that returns a +0 member is one `blr`, so
            # what it tests is the mechanism (a one-word claim in `splits.txt` that leaves the
            # retail functions either side in the auto unit), not the codegen.
            Object(Matching, "MetroidPrime/CSimpleShadowAccessors.cpp"),
            # CSimpleShadow::GetBounds, retail `GetBounds__13CSimpleShadowCFv` at 0x800DF3DC,
            # 0x7C = 124 bytes, its own file because a unit may not claim two discontiguous
            # ranges in one section and the 4-byte `GetTransform` is 0x1C bytes away with
            # `SetAlwaysCalculateRadius` between them. **NonMatching at 76.65%**: every store and
            # every float op is byte-identical to retail, and the only difference is that
            # retail's frame is 48 bytes and holds a third `CVector3f` this does not. It cannot
            # be `Matching` because a claimed range it does not reproduce breaks the DOL hash for
            # all 86 RELs; the claim exists so objdiff measures it and so the next attempt has
            # dtk's retail object in place.
            Object(NonMatching, "MetroidPrime/CSimpleShadowGetBounds.cpp"),
            # CSimpleShadow::SetAlwaysCalculateRadius, retail
            # `SetAlwaysCalculateRadius__13CSimpleShadowFb` at 0x800DF458, 0x10 = 16 bytes. The
            # `rlwimi r0,rX,6,25,25` encoding is the *second* `bool : 1` in declaration order -
            # the rule measured on CGameState::SetIsDarkWorld - and the header already calls the
            # second field `x48_25_alwaysCalculateRadius`, so the two agree.
            Object(Matching, "MetroidPrime/CSimpleShadowSetAlwaysCalculateRadius.cpp"),
            # CSimpleShadow::Valid, retail `Valid__13CSimpleShadowCFv` at 0x800DF268, 0xC = 12
            # bytes. `rlwinm r3,r0,25,31,31` is the *first* `bool : 1` read, and the header's
            # first field is `x48_24_collision`.
            Object(Matching, "MetroidPrime/CSimpleShadowValid.cpp"),
            # CGameState::GetGameMode, retail `GetGameMode__10CGameStateFv` at 0x80142464, 8
            # bytes, carved out of dtk's `auto_03_80142188_text`. One `lwz` of `x19c_ptr` and a
            # `blr`; the header declared it, nothing defined it.
            Object(Matching, "MetroidPrime/Player/CGameStateGetGameMode.cpp"),
            # CGameState::GetHardModeDamageMultiplier, retail
            # `GetHardModeDamageMultiplier__10CGameStateCFv` at 0x80142498, 0x24 = 36 bytes: a
            # 16-byte frame around `fn_80216D38(gpTweakGame)` and nothing else. `fn_80216D38`
            # stays an undefined `extern "C"`, so the unit is not in the port build
            # (`tools/check_files_cmake.py`'s EXCLUDED).
            Object(Matching, "MetroidPrime/Player/CGameStateGetHardModeDamageMultiplier.cpp"),
            # CGameState::SetIsDarkWorld, retail `SetIsDarkWorld__10CGameStateFb` at 0x801424BC,
            # 0x10 = 16 bytes: one `bool : 1` field of the flag byte at 0x2EC, which is the same
            # bit the save-game reader's third `ReadBits(1)` fills. This is the change
            # `include/MetroidPrime/Player/CGameState.hpp` said had "no measured effect" until
            # something reached it. **The bit index is measured, not guessed**: mwcceppc allocates
            # a `bool : 1` in declaration order starting at `rlwimi r0,rX,7,24,24`, so a
            # one-field struct compiles to `,7,24,24` (96.25%) and retail's `,5,26,26` is the
            # *third* field.
            Object(Matching, "MetroidPrime/Player/CGameStateSetIsDarkWorld.cpp"),
            # The four 8-byte module-loader setters - Coin, RsfAudio, FlyerSwarm, SkyRipple.
            # Each is `stw r3,<disp>(r13) ; blr` into the `.sbss` slot the thunk unit above it
            # owns, and each REL module imports it under its retail name, so it cannot be
            # renamed and cannot share a file with the thunk. See the source comments.
            Object(Matching, "MetroidPrime/ScriptLoader/CoinLoaderSet.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/RsfAudioLoaderSet.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/FlyerSwarmLoaderSet.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/SkyRippleLoaderSet.cpp"),
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
            Object(Matching, "MetroidPrime/CIOWinManagerRemoveAllIOWins.cpp"),
            # `CIOWinManager::RemoveIOWin`, retail 0x80049A98, 0x144 = 324 bytes. **99.63%
            # fuzzy, `NonMatching`** on 14 instructions that are all one register choice: the
            # first walk's `prev` and its comparison result hold r29/r27 where retail holds
            # r27/r29, and `cur` is r28 in both. The second walk is retail's byte for byte.
            # Source order, the inner `bool same;` block that puts one shared `ReleaseData`
            # before the branch, and the `volatile` read of the argument's first word (without
            # which mwcceppc hoists the loop-invariant load and needs a sixth callee-saved
            # register, which flips the whole allocation) are all load-bearing; see the file.
            # `symbols.txt` now names this address with MWCC's mangling rather than
            # `fn_80049A98`, which is what let `RemoveAllIOWins` above flip: its link was
            # missing the mangled name, not the range.
            Object(NonMatching, "MetroidPrime/CIOWinManagerRemoveIOWin.cpp"),
            # fn_80049244, retail 0x80049244, 0x118 = 280 bytes. **Not the draw** -
            # docs/research/cube_renderer_vtable.md measured vtable slot +0x94, the one
            # boot_path.md step 21c's `lwz r12,148(r12)` names, and it is
            # CCubeRenderer::BeginScene (0x8026FBFC, 0x180). This is CIOWinManager's
            # pre-draw-then-draw walk, twice over `x0_drawRoot`. **NonMatching for the same
            # reason as RemoveAllIOWins, one line up:** `rstl::rc_ptr<CIOWin>` instantiates
            # `ReleaseData` and its own destructor in this translation unit (0x64 + 0x50 =
            # 180 bytes of weak `.text` over a 280-byte claim), and it also differs from retail
            # in two prologue instructions. Measured counts are in the file's header.
            Object(NonMatching, "MetroidPrime/Carve80049244.cpp"),
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
            # The fourth of boot step 18's four IOWin constructors, and the only one that had no
            # source in the tree at all: retail `fn_800E25C4`, 0x800E25C4, 0x5C = 92 bytes, called
            # from `CGameArchitectureSupport`'s constructor at 0x800080E0 (src/MetroidPrime/main.cpp:
            # 289, `ioWinMgr.AddIOWin(new CAudioStateWin(), 100, -1)`). Retail's caller allocates
            # 20 bytes, which is `CIOWin` and nothing else, so the body is a base-constructor call,
            # the temporary's destructor and one vtable store. The class has no key function in the
            # tree, so this unit declares its own shape and writes the derived-vptr store against
            # retail's `lbl_803B3950` gap object, exactly as CConsoleOutputWindowCtor.cpp and
            # CErrorOutputWindowCtor.cpp do; the source carries the measurement and the vtable
            # contents that fix the slot order.
            Object(Matching, "MetroidPrime/CAudioStateWinCtor.cpp"),
            # The second of boot step 18's four IOWin constructors,
            # `__ct__20CConsoleOutputWindowFiff` at 0x800D63F0, 0x1B4 = 436 bytes, plus the 8-byte
            # `.sdata2` double at 0x8041B570 that mwcceppc's own int-to-float conversion reads. The
            # file was in the tree but in **neither** configure.py nor files.cmake, so nothing
            # compiled it and objdiff could not measure it; its own header records the three
            # measurements (r2 is _SDA2_BASE_ here, 0x43300000 is the high word of a bias double,
            # and the `fsubs` is the conversion) that made it writable at all. The class has no key
            # function anywhere, so the unit declares its own shape and writes the derived-vptr
            # store against retail's `lbl_803B37F0` gap object, like CErrorOutputWindowCtor.cpp.
            Object(Matching, "MetroidPrime/CConsoleOutputWindowCtor.cpp"),
            # One single retail function carved out of an unclaimed dtk `auto_00_*` range, and
            # it is on `CMain::RsMain`'s path - `docs/research/boot_path.md` step 21g, whose one
            # caller is at 0x80006368, inside RsMain's own code. 0x80003858 starts exactly where
            # `Runtime/MetroTRKConsoleStubs.cpp`'s `.text` ends, which is `FACTS.md`'s third carve
            # trap; it links and flip_tests green because the entry is in address order between
            # that unit and `CMainResetGameState.cpp` below. The cost is two undefined symbols in
            # the port's link (`fn_8032194C`, `lbl_8041A3C0`), recorded in
            # docs/research/port_link_gap_list.md rather than hidden.
            Object(Matching, "MetroidPrime/Carve80003858.c"),
            # CMain::ResetGameState, retail ResetGameState__5CMainFv, 0x80003A48, 0x1A0 = 416
            # bytes: one of only three functions the port's boot still waits on. Five copy-outs,
            # a reallocation of gpGameState and five copy-ins, and its `li r3,752` is an
            # independent confirmation of sizeof(CGameState) == 0x2F0. **NonMatching at 98.61%**,
            # and the claim exists so objdiff measures it; the seven remaining instructions are
            # two register-allocation decisions, not logic, and they are written up in the file.
            Object(NonMatching, "MetroidPrime/CMainResetGameState.cpp"),
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
            # flags by tools/probe_gs_offsets.py. One block inside it is **not expressible in
            # C++**: the memset-shaped fill at 0x801444E0 reads its length out of an uninitialised
            # stack word. It used to be two - the second, "fn_80146154 reads two uninitialised
            # words of its own parameter save area", was wrong: those are its own *locals*, and
            # fn_80146154 is now MetroidPrime/Player/CPersistentOptionsCtor.cpp, Matching at
            # 100%. Neither blocks the caller, which only needs the relocation.
            Object(NonMatching, "MetroidPrime/Player/CGameStateStreamCtor.cpp"),
            # fn_801449C8, retail 0x801449C8, 0x2E4 = 740 bytes: CGameState's default constructor,
            # which CGameGlobalObjects' constructor runs at 0x800084DC to fill gpGameState. The
            # straight-line half is the stream constructor's; see the file's header.
            Object(Matching, "MetroidPrime/Player/CGameStateCtor.cpp"),
            # fn_80145950, retail 0x80145950, 0x5C = 92 bytes: the constructor of CGameState+0x54,
            # `SGameStateCardOpts` (0x2C). One of the eight bodies fn_801449C8 calls and the first
            # one on the boot path. `fn_80146154(this, 0)`, four zero words at +0x1C..+0x28, and
            # `if (gpMemoryCard) fn_80145628(this)` - the same `lwz r0,-28356(r13)` global test
            # fn_801449C8 does at 0x80144C50. Returns `this`, so it is declared returning the
            # pointer although both callers discard it.
            Object(Matching, "MetroidPrime/Player/CGameStateCardOptsCtor.cpp"),
            # fn_80146154, retail 0x80146154, 0x58 = 88 bytes: the constructor of CGameState+0xDC,
            # `CPersistentOptions` (0x2C), and callee of the unit above. **The header's claim that
            # it "cannot be written as source" is wrong**, and this unit supersedes it: the two
            # bytes it stores at +0x04 and +0x05 come from `lbz r5,8(r1)` / `lbz r4,12(r1)`, which
            # are this frame's own uninitialised slots - 0..31 is local+outgoing area with the
            # prologue's `stwu r1,-32(r1)`, and LR/r31 go to 36/28 - so two uninitialised `bool`
            # locals stored into the object reproduce them, at 8(r1) and 12(r1). The argument is
            # not either byte: it is `stw r4,0(r3)`, and `fn_80145C98` branches on that word.
            Object(Matching, "MetroidPrime/Player/CPersistentOptionsCtor.cpp"),
            # fn_80145C98, retail 0x80145C98, 0x2F4 = 756 bytes: the default table the unit above
            # reaches through its one `bl`. It branches on the +0x00 word `fn_80146154` just stored
            # (the constructor's `int` argument - a "which game" selector, not a bool) and then
            # runs eleven straight-line `fn_80145ACC` inserts with no loop and no counter. Each
            # name is `lbl_803A9208 + K` against the one merged `.rodata` pool object, not a
            # literal, because a Matching unit may not own `.rodata` - the same reason, and the
            # same spelling, as CGameStateStreamCtor.cpp's two `CBasics::Stringize` sites.
            # The option map's front end, in the 0x80145ACC..0x80145C98 gap dtk left between
            # CGameStateCardOptsCtor.cpp and CPersistentOptionsInit.cpp. fn_80145ACC is the
            # "set if absent" the init table above calls eleven times, and it is written; its
            # only callee that lives in source, fn_80145B90, is the next unit and is
            # NonMatching at 99.05%/95.74% (one scheduling difference in each function), so the
            # pair is wired but the front end is not yet a `Matching` unit. See each file's
            # header for the 61 body variants that did not move either difference off zero.
            Object(NonMatching, "MetroidPrime/Player/CPersistentOptionsMapInsert.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CPersistentOptionsMapLookup.cpp"),
            # SPersistentOptionsValue, the 12-byte value type of the map above:
            #   Clamp fn_801461AC 0x801461AC 68 B, the one member function, and the ctor
            #   __ct__23SPersistentOptionsValueFiii 0x801462DC 60 B, whose return type is
            # load-bearing: mwcceppc forwards `this` out of the call only for a ctor it compiled
            # itself, and the ABI-legal by-value-return spelling breaks all eleven `addi r5,r1,N`
            # in CPersistentOptionsInit.cpp above (99.98%, not Matching). So the ctor stays a
            # real constructor and symbols.txt already carries the mangled name.
            Object(Matching, "MetroidPrime/Player/SPersistentOptionsValueClamp.cpp"),
            Object(Matching, "MetroidPrime/Player/SPersistentOptionsValueCtor.cpp"),
            Object(Matching, "MetroidPrime/Player/CPersistentOptionsInit.cpp"),
            # fn_80004A4C, 0x80004A4C..0x80004AA0, 0x54 = 84 bytes: the deleting destructor of
            # the 16-byte {u32, u32, u32, void*} block, `if (this) { CMemory::Free(x0c_data);
            # if ((short)flag > 0) CMemory::Free(this); } return this;`. 24 callers in the DOL,
            # two of them from CGameState::CGameState(). Its only callee, CMemory::Free at
            # 0x802CE388, is null-safe, which is what makes ten instructions enough.
            Object(Matching, "MetroidPrime/Player/CGameStateBlockDtor.cpp"),
            # The 16-byte SGameStateBlock's rstl::vector<unsigned char> operations, all reached by
            # CGameState's default constructor on the port's boot path: copy constructor
            # (fn_80004AA0), null-guarded construct (fn_80004D5C), clear (fn_80142914), fill
            # (fn_80142BA4) and reserve (fn_801465EC).
            Object(NonMatching, "MetroidPrime/Player/CGameStateBlockCopyCtor.cpp"),
            Object(Matching, "MetroidPrime/Player/CGameStateBlockConstruct.cpp"),
            Object(Matching, "MetroidPrime/Player/CGameStateBlockClear.cpp"),
            Object(Matching, "MetroidPrime/Player/CGameStateBlockFill.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CGameStateBlockReserve.cpp"),
            # fn_80009DBC, 0x80009DBC..0x80009E74, 0xB8 = 184 bytes: the constructor of the
            # 0xE8-byte SGameStateMemcard at CGameState+0x204. Two 76-byte buffers filled with
            # one byte from .sdata 19 times four bytes at a time, two word stores, and a call to
            # fn_80009898 whose result it discards. The four `lbz` per iteration are load-bearing
            # (the compiler cannot rule out that the buffer *is* the global); see the file header.
            Object(Matching, "MetroidPrime/Player/CGameStateMemcardCtor.cpp"),
            # fn_80009898, retail 0x80009898, 0x34 = 52 bytes. fn_80009DBC
            # (CGameStateMemcardCtor.cpp, Matching) calls it as its last statement, and it is the
            # next thing the port's boot waits on. **Split from fn_800098CC, which is adjacent
            # and NonMatching at 99.21%** - seven register-allocation instructions in a loop
            # retail's own bytes leave without a body. A Matching fn_80009898 is worth more than
            # one unit at 99.31%; see SGameStateMemcardFill.cpp's header for the measurements.
            Object(Matching, "MetroidPrime/Player/SGameStateMemcardReset.cpp"),
            # fn_800098CC, retail 0x800098CC, 0x164 = 356 bytes: the 76-byte append and the
            # empty-bodied tail loop, both of which rewrite what fn_80009DBC just wrote.
            Object(NonMatching, "MetroidPrime/Player/SGameStateMemcardFill.cpp"),
            # fn_80009AC0, 0x80009AC0..0x80009BF0, 0x130 = 304 bytes: the first of the two calls
            # fn_80009898 (SGameStateMemcardReset.cpp, Matching) makes, and it builds
            # SGameStateMemcard+0x00..+0x4F. **NonMatching at 85.20%** - all 76 instructions are
            # right and the 27 that differ are the nine stores, where mwcceppc folds the constant
            # +4 into the index (`addi r0,r5,4 ; stbx`) and retail carries it in the store's
            # displacement (`add r5,r3,r0 ; stb r6,4(r5)`). Measured to be the unroller, not the
            # source: outside an unrolled loop this exact body emits retail's form, and 73
            # spellings of the 76-trip loop all fold. The file header has the probe and the
            # spellings. The claim is kept so objdiff measures the 85.20% instead of the function
            # reading as "not started"; a NonMatching object is not in the link.
            Object(NonMatching, "MetroidPrime/Player/SGameStateMemcardBufFill.cpp"),
            # fn_80142CF8, 0x80142CF8, 0x80 and fn_80142DD4, 0x80142DD4, 0x94: the two
            # "serialise gameOptions into a scratch block" helpers the constructor calls at the
            # end. One is `fn_80142CF8(self)`, the other `fn_80142DD4(self, i)` for i = 0..2.
            Object(Matching, "MetroidPrime/Player/CGameStateSysOptsPutTo.cpp"),
            Object(Matching, "MetroidPrime/Player/CGameStateSlotDefaults.cpp"),
            # fn_801440C0, 0x801440C0, 0x80: the `if (gpMemoryCard)` hook the constructor calls
            # at 0x80144C50. A loop over the four player states then four unconditional calls.
            # Adjacent to fn_80144140 (CGameStateStreamCtor.cpp) and unrelated to it.
            Object(Matching, "MetroidPrime/Player/CGameStatePlayerLoop.cpp"),
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
            # CActor::SetDirtyFlags, retail 0x8004A0A0, 0x38 = 56 bytes. It sits inside dtk's
            # unclaimed 0x80049ED8..0x8004AC98 fill and NOT inside CActor.cpp's claim, so it
            # cannot be paired with that unit's object however well the body compiles - hence a
            # file of its own. See the file's header for the measured proof that the 0x150
            # bitfield group is a `uint` and must not be split into u8s.
            Object(Matching, "MetroidPrime/CActorSetDirtyFlags.cpp"),
            # CDamageVulnerability::NormalVulnerabilty, retail 0x800DBB70, 0x10 = 16 bytes: the
            # one of the class's five singleton accessors whose identity retail states. It hands
            # back a pointer 4 bytes into a 0x30 `.bss` object, so the source names a subobject -
            # a literal address folds the `+4` into the `addi` and the unit scores 3 instructions
            # instead of 4. The `.bss` is dtk's fill and is NOT claimed here; it defines
            # `lbl_803DA994`, which is what the one relocation resolves against. The other four
            # accessors are left as fill: the enum order in the header comment cannot be turned
            # into addresses, and a wrong assignment still links and still calls the wrong table.
            Object(Matching, "MetroidPrime/CDamageVulnerabilityStatics.cpp"),
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
            # COsContext::AllocFromArena, retail 0x8028BFFC..0x8028C058 (0x5C). dtk named it
            # `fn_8028BFFC`; renamed in symbols.txt so objdiff can pair it. It is NOT added to
            # files.cmake: src/Kyoto/Basics/COsContext.cpp already defines it for the port, and
            # listing both is a duplicate `link_gap.py` cannot see.
            Object(Matching, "Kyoto/Basics/COsContextAllocFromArena.cpp"),
            Object(NonMatching, "Kyoto/Basics/COsContextCtor.cpp"),
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
            # CQuaternion::BuildInverted, retail `BuildInverted__11CQuaternionCFv` at 0x80028E08,
            # 0x30 = 48 bytes, carved out of dtk's `auto_03_80027B68_text`. It is
            # `CQuaternion(w, -x, -y, -z)` and **not** the header's inline `BuildEquivalent()`,
            # which negates the scalar too. The header declared it with no body until now.
            Object(Matching, "Kyoto/Math/CQuaternionBuildInverted.cpp"),
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
            # `CResFactory::CResFactory()`, retail fn_802FB154 / 0x802FB154, 0xA8 = 168 bytes,
            # and the first of the two constructors on the port's frame-0 critical path:
            # `CGameGlobalObjects` calls it on `this+0x04` and stores `this+0x04` into
            # `gpResourceFactory` four instructions later. This file was the port-only home of
            # `fn_803096C4` and called *that* `CResFactory::CResFactory()`; `fn_803096C4` is the
            # constructor of the four bytes at `CGameGlobalObjects`+0x00 and has moved to
            # `src/MetroidPrime/CGameGlobalObjectsPad0Ctor.cpp`. NonMatching: the range is claimed
            # .text only, and the two vtable addresses it stores at +0x00 are data operands rather
            # than a derived class - deriving them makes this object emit `__vt__8IFactory` and
            # `__vt__11CResFactory` (0x20 bytes each) and a weak `__dt__8IFactoryFv` of 0x48 bytes,
            # none of whose ranges is claimed.
            Object(NonMatching, "Kyoto/CResFactoryCtor.cpp"),
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
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CSkinnedModel.cpp"),
            Object(Matching, "Kyoto/Animation/CSegId.cpp"),
            Object(Matching, "Kyoto/Animation/CSegIdList.cpp"),
            Object(NonMatching, "Kyoto/DolphinCDvdFile.cpp"),
            Object(Matching, "Kyoto/Graphics/CCubeSurface.cpp"),
            Object(NonMatching, "Kyoto/Graphics/CCubeMoviePlayer.cpp"),
            Object(Matching, "Kyoto/Audio/CAudioSysVolume.cpp"),
            Object(Matching, "Kyoto/Audio/CAudioSysSurround.cpp"),
            Object(Matching, "Kyoto/Audio/CAudioSysAICallback.cpp"),
            Object(Matching, "Kyoto/Audio/CAudioSysSysVolume.cpp"),
            Object(Matching, "Kyoto/Audio/CAudioSysDestructor.cpp"),
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
            # `CSimplePool::CSimplePool(IFactory&)`, retail fn_80301008 / 0x80301008, 0x150 =
            # 336 bytes, and the second of the two constructors on the port's frame-0 critical
            # path (`CGameGlobalObjects` builds it on `this+0xE4` with `this+0x04` as its
            # `IFactory&`). NonMatching: the range is claimed .text only, and the two vtable
            # addresses it stores at +0x00 are written out as data operands rather than derived
            # from a class, because deriving them makes this object emit `__vt__8IObjectStore`
            # and `__vt__10CSimplePool` - 0x34 and 0x30 bytes of unclaimed .data - plus the nine
            # `CSimplePool` virtuals that have to fill the second one.
            Object(NonMatching, "Kyoto/CSimplePoolCtor.cpp"),
            Object(Matching, "Kyoto/CToken.cpp"),
            Object(NonMatching, "Kyoto/Math/CMayaSpline.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Streams/CInputStream.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Streams/CBitStreamReader.cpp"),
            # rstl::rbtree_rebalance_for_erase (retail 0x802FD468, 0x41C) was 99.92% and is
            # byte-exact. It needed the `fake_header* header` local *deleted* and the cast done
            # at each use: with the local, mwceppc numbers its cast as a temporary ahead of
            # `node->get_left()`, so that value is pushed to r5 where retail keeps it in r3
            # (r3 holds the dead-in-prologue first parameter). See docs/research/rstl_map.md.
            Object(Matching, "rstl/rstl_map.cpp"),
            # rstl::CRcPtrData's copy constructor: retail's out-of-line `rc_ptr` copy
            # constructor, fn_80049010 / 0x80049010, 0x24 = 36 bytes. It is `NonMatching`
            # because mwcceppc allocates the AddRef to r5/r4 where retail uses r4/r3 - the
            # out-of-line allocator differs from the one used for an inlined expansion, and
            # no spelling of the body changes it. Five of the nine instructions match. Until
            # it is 100% the *DOL* keeps retail's object, which is what let
            # MetroidPrime/CIOWinManagerRemoveAllIOWins.cpp become `Matching` on
            # 2026-09-26: its flip was blocked by the missing **mangled name** for
            # `CIOWinManager::RemoveIOWin`, not by the copy constructor - the range was
            # always byte-exact, and the one rename in `symbols.txt` supplied the symbol.
            # See include/rstl/rc_ptr.hpp and docs/research/rc_ptr.md.
            Object(Matching, "rstl/rc_ptr_copy.cpp"),
            Object(
                MatchingFor("G2ME01"),
                "rstl/rstl_strings.cpp",
                extra_cflags=["-inline deferred"] if config.version == "G2ME01" else [],
            ),
            Object(MatchingFor("G2ME01"), "rstl/rstl_misc.cpp"),
            Object(Matching, "rstl/rstl_string_member_op.cpp"),
            # rstl::string_l / rstl::wstring_l, retail 0x802FF3DC..0x802FF448, 0x6C = 108 bytes:
            # the 108 bytes between the end of rstl_strings.cpp and the start of RstlExtras.cpp.
            # Both are declared in include/rstl/string.hpp and used from fifteen places, and
            # nothing in the tree defined them - the DOL linked only because dtk's `auto_*` object
            # still supplied retail's own copies. One claim of one range, so one unit: the
            # `auto_*` object that held it is shortened at both ends.
            Object(Matching, "rstl/rstl_string_l.cpp"),
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
    # DarkSamus: the largest REL module's .text (0x230F8), and the largest one with
    # no `Rel(...)` block at all. Its symbol table is unnamed retail (`fn_10_<offset>`
    # for 526 of 535 functions), so the units here are keyed by retail offset and each
    # claims only the contiguous run it reproduces. The module's RELMain/RELExit/loader
    # sit at 0xCED0..0xCF44, *between* two of the runs below, which is why they stay
    # with dtk's `auto_*` objects rather than being reimplemented here.
    #
    # **Every object listed here must contain at least one symbol in the module's retail
    # `ldscript.lcf` FORCEACTIVE list, or be reachable from `.data`.** The `.plf` link
    # runs with `-strip_partial`, so an object whose symbols are neither forced active nor
    # referenced is dropped silently: five byte-exact deleting destructors written for
    # 0x214D0..0x215FC disappeared from the module, `.text` came out 0x12C short, and
    # every `bl` after them resolved 0x12C low. Nothing in 0x21000..0x21600 is active and
    # no `.data` object references them, so that run cannot be claimed at all.
    # `tools/audit_rel_claim.py <Module>` prints the preplf/plf symbol counts that catch it.
    Rel(
        "DarkSamus",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CDarkSamus.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CDarkSamusState.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CDarkSamusFlags.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CDarkSamusFloatParameter.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CDarkSamusMembers.cpp"),
            # `REL/REL_Setup.cpp` and `REL/global_destructor_chain.c` are deliberately NOT
            # named here: the shared "REL" lib above already puts them in every module and
            # naming them twice is a "Duplicate object name" error. What this block must do
            # is *claim* their ranges in `splits.txt`, as ChozoGhost's default tail does -
            # omit the claim and dtk fills the range with its own bytes, and the gate then
            # reads both units as UNIT GONE against a baseline where this module was still
            # unconfigured.
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
    Rel(
        # Fourteen short accessors at the head of the module, .text 0x3C..0xD8. The same fourteen
        # functions, byte for byte, are WallCrawler's 0x00..0x9C, which CScriptWallCrawler.cpp
        # already reproduces at 100% as a Matching unit. Everything else in the module is left
        # unclaimed, so dtk fills it from retail and the module's sha1 is unchanged.
        "EyeBall",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/EyeBallAccessors.cpp"),
        ],
    ),
    Rel(
        # 14 short accessors, .text 0x000000..0x00009C: the accessor set the REL loader
        # generator emits, byte-identical to WallCrawler's 0x00..0x9C, which
        # MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching unit.
        # Everything else in the module is left unclaimed, so dtk fills it from retail.
        "AtomicBeta",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/AtomicBetaAccessors.cpp"),
        ],
    ),
    Rel(
        # 13 short accessors, .text 0x000078..0x00010C: the accessor set the REL loader
        # generator emits, byte-identical to WallCrawler's 0x00..0x9C, which
        # MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching unit.
        # Everything else in the module is left unclaimed, so dtk fills it from retail.
        "DigitalGuardian",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/DigitalGuardianAccessors.cpp"),
        ],
    ),
    Rel(
        # 14 short accessors, .text 0x00B994..0x00BA30: the accessor set the REL loader
        # generator emits, byte-identical to WallCrawler's 0x00..0x9C, which
        # MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching unit.
        # Everything else in the module is left unclaimed, so dtk fills it from retail.
        "EmperorIngStage1",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/EmperorIngStage1Accessors.cpp"),
        ],
    ),
    Rel(
        # 14 short accessors, .text 0x000000..0x00009C: the accessor set the REL loader
        # generator emits, byte-identical to WallCrawler's 0x00..0x9C, which
        # MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching unit.
        # Everything else in the module is left unclaimed, so dtk fills it from retail.
        "EmperorIngStage2Tentacle",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/EmperorIngStage2TentacleAccessors.cpp"),
        ],
    ),
    Rel(
        # 14 short accessors, .text 0x00038C..0x000428: the accessor set the REL loader
        # generator emits, byte-identical to WallCrawler's 0x00..0x9C, which
        # MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching unit.
        # Everything else in the module is left unclaimed, so dtk fills it from retail.
        "Glowbug",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/GlowbugAccessors.cpp"),
        ],
    ),
    Rel(
        # 14 short accessors, .text 0x000368..0x000404: the accessor set the REL loader
        # generator emits, byte-identical to WallCrawler's 0x00..0x9C, which
        # MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching unit.
        # Everything else in the module is left unclaimed, so dtk fills it from retail.
        "GunTurret",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/GunTurretAccessors.cpp"),
        ],
    ),
    Rel(
        # 13 short accessors, .text 0x000044..0x0000D8: the accessor set the REL loader
        # generator emits, byte-identical to WallCrawler's 0x00..0x9C, which
        # MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching unit.
        # Everything else in the module is left unclaimed, so dtk fills it from retail.
        "IngSpiderballGuardian",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/IngSpiderballGuardianAccessors.cpp"),
        ],
    ),
    Rel(
        # 14 short accessors, .text 0x000000..0x00009C: the accessor set the REL loader
        # generator emits, byte-identical to WallCrawler's 0x00..0x9C, which
        # MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching unit.
        # Everything else in the module is left unclaimed, so dtk fills it from retail.
        "Kralee",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/KraleeAccessors.cpp"),
        ],
    ),
    Rel(
        # 14 short accessors, .text 0x00003C..0x0000D8: the accessor set the REL loader
        # generator emits, byte-identical to WallCrawler's 0x00..0x9C, which
        # MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching unit.
        # Everything else in the module is left unclaimed, so dtk fills it from retail.
        "Krocuss",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/KrocussAccessors.cpp"),
        ],
    ),
    Rel(
        # 14 short accessors, .text 0x000000..0x00009C: the accessor set the REL loader
        # generator emits, byte-identical to WallCrawler's 0x00..0x9C, which
        # MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching unit.
        # Everything else in the module is left unclaimed, so dtk fills it from retail.
        "OctapedeSegment",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/OctapedeSegmentAccessors.cpp"),
        ],
    ),
    Rel(
        # 14 short accessors, .text 0x0002F0..0x00038C: the accessor set the REL loader
        # generator emits, byte-identical to WallCrawler's 0x00..0x9C, which
        # MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching unit.
        # Everything else in the module is left unclaimed, so dtk fills it from retail.
        "PuddleSpore",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/PuddleSporeAccessors.cpp"),
        ],
    ),
    Rel(
        # 14 short accessors, .text 0x00003C..0x0000D8: the accessor set the REL loader
        # generator emits, byte-identical to WallCrawler's 0x00..0x9C, which
        # MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching unit.
        # Everything else in the module is left unclaimed, so dtk fills it from retail.
        "Ripper",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/RipperAccessors.cpp"),
        ],
    ),
    Rel(
        # 12 short accessors, .text 0x00003C..0x0000C8: the accessor set the REL loader
        # generator emits, byte-identical to WallCrawler's 0x00..0x9C, which
        # MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching unit.
        # Everything else in the module is left unclaimed, so dtk fills it from retail.
        "Shredder",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/ShredderAccessors.cpp"),
        ],
    ),
    Rel(
        # 14 short accessors, .text 0x000358..0x0003F4: the accessor set the REL loader
        # generator emits, byte-identical to WallCrawler's 0x00..0x9C, which
        # MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching unit.
        # Everything else in the module is left unclaimed, so dtk fills it from retail.
        "SpankWeed",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/SpankWeedAccessors.cpp"),
        ],
    ),
    Rel(
        # 14 short accessors, .text 0x0002F0..0x00038C: the accessor set the REL loader
        # generator emits, byte-identical to WallCrawler's 0x00..0x9C, which
        # MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching unit.
        # Everything else in the module is left unclaimed, so dtk fills it from retail.
        "Sporb",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/SporbAccessors.cpp"),
        ],
    ),
    Rel(
        # 14 short accessors, .text 0x00038C..0x000428: the accessor set the REL loader
        # generator emits, byte-identical to WallCrawler's 0x00..0x9C, which
        # MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching unit.
        # Everything else in the module is left unclaimed, so dtk fills it from retail.
        "StoneToad",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/StoneToadAccessors.cpp"),
        ],
    ),
    Rel(
        # 14 short accessors, .text 0x000000..0x00009C: the accessor set the REL loader
        # generator emits, byte-identical to WallCrawler's 0x00..0x9C, which
        # MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching unit.
        # Everything else in the module is left unclaimed, so dtk fills it from retail.
        "WallWalker",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/WallWalkerAccessors.cpp"),
        ],
    ),
    Rel(
        # 14 short accessors, .text 0x00032C..0x0003C8: the accessor set the REL loader
        # generator emits, byte-identical to WallCrawler's 0x00..0x9C, which
        # MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching unit.
        # Everything else in the module is left unclaimed, so dtk fills it from retail.
        "WispTentacle",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/WispTentacleAccessors.cpp"),
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
