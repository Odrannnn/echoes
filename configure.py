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
from typing import Any, Dict, List, Optional

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
    f"-DVERSION={version_num}",
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

# Main-game and game REL sources use the same Retro compiler and base flags.
retro_mw_version = "GC/2.7"

# Retro flags
cflags_retro = [
    *cflags_base,
    "-use_lmw_stmw on",
    "-str reuse,pool,readonly",
    "-gccinc",
    "-inline deferred,noauto",
    "-common on",
    "-i extern/musyx/include",
    "-DMUSY_TARGET=MUSY_TARGET_DOLPHIN",
    "-DMUSY_VERSION_MAJOR=2",
    "-DMUSY_VERSION_MINOR=0",
    "-DMUSY_VERSION_PATCH=3",
]

if config.version == "G2ME01":
    cflags_retro.append('-pragma "inline_max_size(125)"')

# Relocatable code cannot use the DOL's small-data bases.
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
# RELs build with GC/1.3.2, not upstream's GC/2.7: 24 of our Matching modules (the accessor
# RELs, ScriptCoin, SwarmBasics, WallCrawler, Metaree, DarkSamus, ...) reproduce retail only
# under 1.3.2, while every Matching REL unit upstream has (SLdrTweakPlayer, SLdrTweakGuiColors)
# reproduces under both (before upstream folded both into ScriptLoader/Tweaks.cpp). A module whose sources were matched upstream under 2.7 (Tweaks) passes
# mw_version=retro_mw_version.
def Rel(
    lib_name: str,
    objects: List[Object],
    extra_cflags: Optional[List[str]] = None,
    mw_version: str = "GC/1.3.2",
) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": mw_version,
        "cflags": cflags_rel + (extra_cflags or []),
        "progress_category": "game",
        "host": True,
        "objects": objects,
    }

# MusyX flags
cflags_musyx = [
    "-proc gekko",
    "-nodefaults",
    "-nosyspath",
    "-i include",
    "-i libc",
    "-i extern/musyx/include",
    "-inline auto,depth=4",
    "-O4,p",
    "-fp hard",
    "-enum int",
    "-sym on",
    "-Cpp_exceptions off",
    "-str reuse,pool,readonly",
    "-fp_contract off",
    "-DMUSY_TARGET=MUSY_TARGET_DOLPHIN",
    "-DM_PI=3.14159265358979323846",
]


# Helper function for MusyX objects
def MusyX(objects: List[Object], mw_version="GC/1.3.2", major=2, minor=0, patch=3) -> Dict[str, Any]:
    return {
        "lib": "musyx",
        "mw_version": mw_version,
        "src_dir": "extern/musyx/src",
        "cflags": [
            *cflags_musyx,
            f"-DMUSY_VERSION_MAJOR={major}",
            f"-DMUSY_VERSION_MINOR={minor}",
            f"-DMUSY_VERSION_PATCH={patch}",
        ],
        "progress_category": "sdk",
        "host": False,
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
        "mw_version": retro_mw_version,
        "progress_category": "game",  # str | List[str]
        "host": True,
        "objects": [
            Object(NonMatching, "MetroidPrime/main.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/Startup.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CControlMapper.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CObjectList.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CAxisAngle.cpp"),
            Object(NonMatching, "MetroidPrime/CEulerAngles.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CMatrix3f_Ext.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CArchMsgParmUserInput.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CInputGenerator.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CMainFlow.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CCredits.cpp"),
            Object(NonMatching, "MetroidPrime/CSplashScreen.cpp"),
            Object(NonMatching, "MetaRender/CCubeRenderer.cpp"),
            Object(Matching, "GuiSys/CGuiFrameFactory.cpp"),
            Object(NonMatching, "GuiSys/CGuiFrame.cpp"),
            Object(NonMatching, "GuiSys/CAuiMeter.cpp"),
            Object(MatchingFor("G2ME01"), "GuiSys/CGuiCompoundWidget.cpp"),
            Object(Matching, "GuiSys/CGuiCamera.cpp"),
            Object(Matching, "GuiSys/CGuiLight.cpp"),
            Object(NonMatching, "GuiSys/CGuiObject.cpp"),
            Object(NonMatching, "GuiSys/CGuiWidget.cpp"),
            Object(NonMatching, "GuiSys/CAuiEnergyBarT01.cpp"),
            Object(NonMatching, "GuiSys/CAuiImagePane.cpp"),
            Object(Matching, "GuiSys/CGuiPane.cpp"),
            Object(NonMatching, "GuiSys/CGuiTextPane.cpp"),
            Object(NonMatching, "WorldFormat/COBBTree.cpp"),
            Object(NonMatching, "WorldFormat/CCollidableOBBTree.cpp"),
            Object(NonMatching, "WorldFormat/CCollidableOBBTreeGroup.cpp"),
            Object(NonMatching, "WorldFormat/CAreaOctTree.cpp"),
            Object(NonMatching, "WorldFormat/CMetroidAreaCollider.cpp"),
            Object(NonMatching, "WorldFormat/CAreaOctTree_Tests.cpp"),
            Object(Matching, "WorldFormat/CCollisionSurface.cpp"),
            Object(NonMatching, "WorldFormat/CMetroidModelInstance.cpp"),
            Object(MatchingFor("G2ME01"), "WorldFormat/CAreaBspTree.cpp"),
            Object(NonMatching, "WorldFormat/CPVSAreaSet.cpp"),
            Object(NonMatching, "WorldFormat/CAreaRenderOctTree.cpp"),
            Object(Matching, "WorldFormat/Carve80255A0C.c"),
            Object(Matching, "WorldFormat/CCollisionPrimitiveData.cpp"),
            Object(MatchingFor("G2ME01"), "WorldFormat/CWorldLight.cpp"),
            Object(Matching, "MetroidPrime/CStaticGeometryMap.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CRELFileToken.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CRELFileManager.cpp"),
            Object(NonMatching, "Collision/CCollidableAABox.cpp"),
            Object(Matching, "Collision/CCollidableSphere.cpp"),
            Object(MatchingFor("G2ME01"), "Collision/CCollidableCollisionSurface.cpp"),
            Object(MatchingFor("G2ME01"), "Collision/CCollisionInfo.cpp"),
            Object(MatchingFor("G2ME01"), "Collision/InternalColliders.cpp"),
            Object(MatchingFor("G2ME01"), "Collision/CCollisionPrimitive.cpp"),
            Object(NonMatching, "Collision/CollisionUtil.cpp"),
            Object(NonMatching, "Collision/COBBox.cpp"),
            Object(MatchingFor("G2ME01"), "Collision/CMRay.cpp"),
            Object(NonMatching, "MetroidPrime/CStateManager.cpp"),
            Object(NonMatching, "MetroidPrime/CVisorFlare.cpp"),
            Object(NonMatching, "MetroidPrime/CWorldTransManager.cpp"),
            Object(NonMatching, "MetroidPrime/CRagDoll.cpp"),
            Object(Matching, "MetroidPrime/CSortedLists.cpp"),
            Object(NonMatching, "MetroidPrime/CProjectedShadow.cpp"),
            Object(NonMatching, "MetroidPrime/CSlideShow.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CPreFrontEnd.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptProjectedShadow.cpp"),
            Object(NonMatching, "MetroidPrime/CSteeringBehaviors.cpp"),
            Object(NonMatching, "MetroidPrime/CEntity.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CArchMsgParmInt32.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CArchMsgParmInt32Int32VoidPtr.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CArchMsgParmNull.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CArchMsgParmReal32.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/Decode.cpp"),
            Object(Matching, "MetroidPrime/CIOWinManager.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CIOWin.cpp"),
            Object(NonMatching, "MetroidPrime/CWorld.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CArchMsgParmControllerStatus.cpp"),
            Object(NonMatching, "MetroidPrime/CGameArea.cpp"),
            Object(Matching, "MetroidPrime/CWorldLayerState.cpp"),
            Object(NonMatching, "MetroidPrime/CMemoryCard.cpp"),
            Object(NonMatching, "MetroidPrime/CMemoryCardDriver.cpp"),
            Object(NonMatching, "MetroidPrime/CSaveGameScreen.cpp"),
            Object(Matching, "MetroidPrime/CGameHintInfo.cpp"),
            Object(NonMatching, "MetroidPrime/CErrorOutputWindow.cpp"),
            Object(NonMatching, "MetroidPrime/CRainSplashGenerator.cpp"),
            Object(NonMatching, "MetroidPrime/Cameras/CGameCamera.cpp"),
            Object(NonMatching, "MetroidPrime/Cameras/CCameraShakerData.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptCameraFilterKeyframe.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/ScriptObjects/CScriptCameraBlurKeyframe.cpp"),
            Object(NonMatching, "MetroidPrime/Cameras/CCameraFilter.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptCameraShaker.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptActorKeyframe.cpp"),
            Object(NonMatching, "MetroidPrime/CConsoleOutputWindow.cpp"),
            Object(NonMatching, "MetroidPrime/Cameras/CBallCamera.cpp"),
            Object(NonMatching, "MetroidPrime/Cameras/CBallCameraTransitions.cpp"),
            Object(NonMatching, "MetroidPrime/Cameras/CFirstPersonCamera.cpp"),
            Object(NonMatching, "MetroidPrime/Cameras/CCameraManager.cpp"),
            Object(NonMatching, "MetroidPrime/Cameras/CCinematicCamera.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptCamera.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/Cameras/CBallCameraTransitionState.cpp"),
            Object(NonMatching, "MetroidPrime/Cameras/CSpindleCamera.cpp"),
            Object(NonMatching, "MetroidPrime/TypesMatch.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CPlayerState.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptTimer.cpp"),
            Object(NonMatching, "MetroidPrime/CAutoMapper.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CPlayerGun.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CPlayerGunBase.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CGrappleArm.cpp"),
            Object(Matching, "MetroidPrime/Player/CFidget.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptPickup.cpp"),
            Object(Matching, "MetroidPrime/HUD/CHUDMemoParms.cpp"),
            Object(NonMatching, "MetroidPrime/HUD/CSamusHud.cpp"),
            Object(NonMatching, "MetroidPrime/HUD/CHudRadarInterface.cpp"),
            Object(NonMatching, "MetroidPrime/HUD/CHudDecoInterfaceScan.cpp"),
            Object(NonMatching, "MetroidPrime/HUD/CHudVisorBeamMenu.cpp"),
            Object(NonMatching, "MetroidPrime/CQuitGameScreen.cpp"),
            Object(NonMatching, "MetroidPrime/CPauseScreen.cpp"),
            Object(NonMatching, "MetroidPrime/CInGameGuiManager.cpp"),
            Object(NonMatching, "MetroidPrime/CSimpleShadow.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CActorParameters.cpp"),
            Object(Matching, "MetroidPrime/CWorldShadow.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptRepulsor.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptSound.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptPlatform.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptActor.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptDoor.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptDock.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptDebris.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptEffect.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptTrigger.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptWater.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptGenerator.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptColorModulate.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptSpecialFunction.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptActorRotate.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptPickupGenerator.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptEMPulse.cpp"),
            Object(NonMatching, "MetroidPrime/CMapArea.cpp"),
            Object(Matching, "MetroidPrime/CMappableObject.cpp"),
            Object(NonMatching, "MetroidPrime/CMapWorldInfo.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptCounter.cpp"),
            Object(NonMatching, "MetroidPrime/CMapWorld.cpp"),
            Object(NonMatching, "MetroidPrime/CMemoryDrawEnum.cpp"),
            Object(NonMatching, "MetroidPrime/CMapUniverse.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptTeamAiMgr.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptCameraWaypoint.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptCameraHint.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptPlayerHint.cpp"),
            Object(NonMatching, "MetroidPrime/CHintState.cpp"),
            Object(NonMatching, "MetroidPrime/CHintManager.cpp"),
            Object(NonMatching, "MetroidPrime/CPlayerHintManager.cpp"),
            Object(NonMatching, "MetroidPrime/CGameHint.cpp"),
            Object(NonMatching, "MetroidPrime/CGameLight.cpp"),
            Object(NonMatching, "MetroidPrime/CExplosion.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CEffect.cpp"),
            Object(NonMatching, "MetroidPrime/CParticleGenInfoGeneric.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CParticleGenInfo.cpp"),
            Object(NonMatching, "MetroidPrime/CParticleDatabase.cpp"),
            Object(NonMatching, "MetroidPrime/CAnimData.cpp"),
            Object(NonMatching, "MetroidPrime/Factories/CCharacterFactory.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/Factories/CAssetFactory.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CAnimationDatabaseGame.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CTransitionDatabaseGame.cpp"),
            Object(NonMatching, "MetroidPrime/CTargetReticles.cpp"),
            Object(NonMatching, "MetroidPrime/CWeaponMgr.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptHUDMemo.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptAreaProperties.cpp"),
            Object(Matching, "MetroidPrime/Player/CPlayerEnergyDrain.cpp"),
            Object(Matching, "MetroidPrime/Player/CStaticInterference.cpp"),
            Object(NonMatching, "MetroidPrime/PathFinding/CPathFindSearch.cpp"),
            Object(Matching, "MetroidPrime/PathFinding/CPathFindRegion.cpp"),
            Object(NonMatching, "MetroidPrime/PathFinding/CPathFindArea.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/PathFinding/CPathFindSpline.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CHealthInfo.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CGameState.cpp"),
            Object(NonMatching, "MetroidPrime/Tweaks/CTweakAutoMapper.cpp"),
            Object(NonMatching, "MetroidPrime/Tweaks/CTweakBall.cpp"),
            Object(NonMatching, "MetroidPrime/Tweaks/CTweakPlayer.cpp"),
            Object(NonMatching, "MetroidPrime/Tweaks/CTweakPlayerGun.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CRelFile.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/Tweaks/CTweakTargeting.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/Tweaks/CTweakGuiColors.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/Tweaks/CTweakGui.cpp"),
            Object(Matching, "MetroidPrime/Tweaks/Carve80216D2C.c"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/Player/CFrontEndGameMode.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CGMDeathMatch.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CGMCoin.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CGMMultiplayer.cpp"),
            Object(NonMatching, "MetroidPrime/CBoneTracking.cpp"),
            # `TuneScreenBrightness` is the only function in this object whose 2.7 codegen differs
            # from retail's, and only by scheduler order: 2.7 hoists the int->float bias `lfd` to
            # slot 2 of the block and the `1.0f` load ahead of the dependent `lfd f2,8(r1)`, while
            # retail has both one step later. Same 17 instructions, same registers f0..f4, same
            # constants. That is a scheduling difference between two builds of the compiler family,
            # so it cannot be reached from the source: measured across every `GC/*` compiler, only
            # 2.0p1 reproduces the whole object instruction for instruction, and no `-O`/`-opt`/
            # `-schedule` flag combination does under 2.7. Hence the per-object `mw_version`.
            Object(Matching, "MetroidPrime/Player/CGameOptions.cpp", mw_version="GC/2.0p1"),
            # Whole-unit carve of the unclaimed gap 0x80161D04..0x80161FBC that dtk was handing to
            # `main/auto_03_80161D04_text`: `fn_80161D04` / `fn_80161F40` / `fn_80161EC8` are
            # `rstl::sort` / `__insertion_sort` / `__sort3` for `rstl::vector<rstl::pair<Ui,Ui> >`,
            # which `MetroidPrime/Player/CGameOptions.cpp`'s retail object and
            # `MetroidPrime/CMemoryCard.cpp`'s both reference as `U fn_80161D04`. NonMatching only
            # until `tools/flip_test.sh` says otherwise.
            Object(Matching, "auto_03_80161D04_text.cpp"),
            Object(NonMatching, "MetroidPrime/CEnvFxManager.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CRumbleManager.cpp"),
            Object(NonMatching, "MetroidPrime/CFluidUVMotion.cpp"),
            Object(Matching, "MetroidPrime/CFluidPlane.cpp"),
            Object(NonMatching, "MetroidPrime/CFluidPlaneManager.cpp"),
            Object(NonMatching, "MetroidPrime/CFluidPlaneCPU.cpp"),
            Object(NonMatching, "MetroidPrime/CCollisionActorManager.cpp"),
            Object(NonMatching, "MetroidPrime/CCollisionActor.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptSequenceTimer.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptTriggerEllipsoid.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptSpindleCamera.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptPathCamera.cpp"),
            Object(NonMatching, "MetroidPrime/Cameras/CPathCamera.cpp"),
            Object(NonMatching, "MetroidPrime/Cameras/CInterpolationCamera.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptStreamedMusic.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoaderRel.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/CBomb.cpp"),
            Object(Matching, "MetroidPrime/Weapons/CPowerBeam.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/CAuxWeapon.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/GunController/CGunMotion.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/CGunWeapon.cpp"),
            Object(Matching, "MetroidPrime/Weapons/GunController/CGunController.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/GunController/CGSComboFire.cpp"),
            Object(Matching, "MetroidPrime/Weapons/GunController/CGSFidget.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/GunController/CGSFreeLook.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/CWeapon.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/CGameProjectile.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/CEnergyProjectile.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/CIceImpact.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/CBeamProjectile.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/CPlasmaProjectile.cpp"),
            Object(NonMatching, "Weapons/CProjectileWeapon.cpp"),
            Object(Matching, "Weapons/CCollisionResponseData.cpp"),
            Object(NonMatching, "Weapons/CDecal.cpp"),
            Object(NonMatching, "MetroidPrime/CDecalManager.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptSpiderBallWaypoint.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/TGameTypes.cpp"),
            Object(NonMatching, "MetroidPrime/CPhysicsActor.cpp"),
            Object(NonMatching, "MetroidPrime/CModelData.cpp"),
            Object(NonMatching, "MetroidPrime/CDamageVulnerability.cpp"),
            Object(NonMatching, "MetroidPrime/CActorLights.cpp"),
            Object(NonMatching, "MetroidPrime/CGroundMovement.cpp"),
            Object(NonMatching, "MetroidPrime/CGameCollision.cpp"),
            Object(Matching, "MetroidPrime/Enemies/CAi.cpp"),
            Object(NonMatching, "MetroidPrime/Enemies/CStateMachine.cpp"),
            Object(Matching, "MetroidPrime/Factories/CStateMachineFactory.cpp"),
            Object(NonMatching, "MetroidPrime/Enemies/CKnockBackMgr.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptWaypoint.cpp"),
            Object(NonMatching, "MetroidPrime/Enemies/CPatterned.cpp"),
            Object(NonMatching, "MetroidPrime/Enemies/CPatternedAiFunctions.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CBodyController.cpp"),
            Object(Matching, "MetroidPrime/BodyState/CBodyStateCmdMgr.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CBodyStateInfo.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CBSLocomotion.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CBSHurled.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CBSJump.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/BodyState/CBSScripted.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/BodyState/CBSAttack.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/BodyState/CBSLoopAttack.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/BodyState/CBSCover.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/BodyState/CBSLoopReaction.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/BodyState/CBSGenerate.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CBSKnockBack.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/BodyState/CBSFall.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/BodyState/CBSGetup.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/BodyState/CBSLieOnGround.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/BodyState/CBSDie.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/BodyState/CBSGroundHit.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/BodyState/CBSSlide.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/BodyState/CBSStep.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/BodyState/CBSTaunt.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/BodyState/CBSProjectileAttack.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CBSTurn.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CBSWallHang.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptVisorFlare.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CHUDBillboardEffect.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptWorldTeleporter.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptVisorGoo.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptControllerAction.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/ScriptObjects/CScriptSwitch.cpp"),
            Object(Matching, "MetroidPrime/BodyState/CABSIdle.cpp"),
            Object(Matching, "MetroidPrime/BodyState/CABSFlinch.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CABSAim.cpp"),
            Object(Matching, "MetroidPrime/BodyState/CABSReaction.cpp"),
            Object(NonMatching, "MetroidPrime/CActor.cpp"),
            Object(NonMatching, "MetroidPrime/CActorModelParticles.cpp"),
            Object(NonMatching, "MetroidPrime/CDamageInfo.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptSpawnPoint.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CRuleSet.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CPlayer.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CPlayerDynamics.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CPlayerOrbit.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CPlayerVisor.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CPlayerCameraBob.cpp"),
            Object(NonMatching, "MetroidPrime/Factories/CScannableObjectInfo.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CScanDisplay.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CMorphBall.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CMorphBallShadow.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScanTreeInventory.cpp"),
            # Units carved and matched before the upstream merge, in ranges upstream leaves unsplit.
            Object(Matching, "Runtime/MetroTRKConsoleStubs.cpp"),
            Object(Matching, "MetroidPrime/Carve80003858.c"),
            Object(NonMatching, "MetroidPrime/CMainResetGameState.cpp"),
            Object(Matching, "MetroidPrime/Carve80004010.c"),
            Object(Matching, "MetroidPrime/Carve80004438.c"),
            Object(Matching, "MetroidPrime/Carve8000447C.cpp"),
            Object(Matching, "MetroidPrime/Carve800045A0.c"),
            Object(Matching, "MetroidPrime/Carve80004744.c"),
            Object(Matching, "MetroidPrime/Player/CGameStateBlockDtor.cpp"),
            Object(Matching, "MetroidPrime/Player/CGameStateBlockCopyCtor.cpp"),
            Object(Matching, "MetroidPrime/Player/Carve80004B9C.c"),
            Object(Matching, "MetroidPrime/Player/Carve80004C4C.c"),
            Object(Matching, "MetroidPrime/Player/CGameStateBlockConstruct.cpp"),
            Object(Matching, "MetroidPrime/Player/Carve800052A0.c"),
            Object(Matching, "MetroidPrime/Factories/Carve80032674.c"),
            Object(Matching, "MetroidPrime/Factories/Carve80032774.cpp"),
            Object(Matching, "MetroidPrime/Factories/Carve80032A98.c"),
            Object(Matching, "MetroidPrime/Factories/Carve80032BE8.c"),
            Object(Matching, "MetroidPrime/Carve80044D48.c"),
            Object(Matching, "MetroidPrime/Carve80044F88.c"),
            Object(Matching, "MetroidPrime/Carve80045014.c"),
            Object(Matching, "MetroidPrime/Carve80045160.c"),
            Object(Matching, "MetroidPrime/Carve80045CD4.c"),
            Object(Matching, "MetroidPrime/Carve80046DB8.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptRelay.cpp"),
            Object(Matching, "MetroidPrime/CConsoleOutputWindowCtor.cpp"),
            Object(Matching, "MetroidPrime/Carve800E10EC.cpp"),
            Object(Matching, "MetroidPrime/Carve800E1548.c"),
            Object(Matching, "MetroidPrime/CAudioStateWinCtor.cpp"),
            Object(Matching, "MetroidPrime/Carve800E39D0.c"),
            Object(Matching, "MetroidPrime/Carve800E9C14.c"),
            Object(Matching, "MetroidPrime/Carve800EC508.c"),
            Object(Matching, "MetroidPrime/Carve800ED550.c"),
            Object(Matching, "MetroidPrime/Carve800ED604.c"),
            Object(Matching, "MetroidPrime/Carve800FEE98.c"),
            Object(Matching, "MetroidPrime/Carve800FEF78.c"),
            Object(Matching, "MetroidPrime/Carve801007D8.c"),
            Object(Matching, "MetroidPrime/Carve8010EE54.c"),
            Object(Matching, "MetroidPrime/Carve801174E8.c"),
            Object(Matching, "MetroidPrime/Carve8012CB4C.c"),
            Object(Matching, "MetroidPrime/Player/Carve8014FFCC.c"),
            Object(Matching, "MetroidPrime/CInGameTweakManagerReadFromMemoryCard.cpp"),
            Object(Matching, "MetroidPrime/CInGameTweakManagerCtor.cpp"),
            Object(Matching, "MetroidPrime/Carve8016FD4C.c"),
            Object(Matching, "MetroidPrime/Carve80179E08.c"),
            Object(Matching, "MetroidPrime/Carve80193C30.c"),
            Object(Matching, "MetroidPrime/Carve80193C54.c"),
            Object(Matching, "MetroidPrime/Carve80193C7C.c"),
            Object(Matching, "MetroidPrime/Carve80193C84.c"),
            Object(Matching, "MetroidPrime/Carve80193D9C.c"),
            Object(Matching, "MetroidPrime/Carve80193DB8.c"),
            Object(Matching, "MetroidPrime/Carve80193E04.c"),
            Object(Matching, "MetroidPrime/Carve801997B0.c"),
            Object(Matching, "MetroidPrime/Carve8019AC78.c"),
            Object(Matching, "MetroidPrime/Carve8019CE6C.c"),
            Object(Matching, "MetroidPrime/Carve8019E368.c"),
            Object(Matching, "MetroidPrime/CStateManagerScriptMsgArray.cpp"),
            Object(Matching, "MetroidPrime/Carve801BC900.c"),
            Object(Matching, "MetroidPrime/Carve801C128C.c"),
            Object(Matching, "MetroidPrime/Carve801C13F4.c"),
            Object(Matching, "MetroidPrime/Player/Carve801D3B88.c"),
            Object(Matching, "MetroidPrime/Weapons/Carve801D5E9C.c"),
            Object(Matching, "MetroidPrime/Weapons/Carve801D688C.c"),
            Object(Matching, "MetroidPrime/Weapons/Carve801D6920.c"),
            Object(Matching, "MetroidPrime/Weapons/Carve801D6930.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801E3864.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801E3E34.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801E515C.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801E5230.c"),
            Object(Matching, "MetroidPrime/Cameras/Carve801E7C14.c"),
            Object(Matching, "MetroidPrime/Cameras/Carve801E8028.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801E8AEC.c"),
            Object(Matching, "MetroidPrime/CActorField25.cpp"),
            Object(Matching, "MetroidPrime/Carve801F3690.c"),
            Object(Matching, "MetroidPrime/Carve801F36DC.c"),
            Object(Matching, "MetroidPrime/Carve801F7AC8.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/CUnknown90.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801F97C8.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FBC58.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FD4B0.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FDB5C.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FD5E8.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FD638.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FD67C.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FD8E0.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FD924.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FD998.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FDA1C.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FDAA4.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FDAE8.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FEA98.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FEAE0.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FEC64.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FECAC.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FEE40.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FEEF0.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FF4A4.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FF5A0.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FF720.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FF8A0.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FFA20.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/SpacePirate.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80200E3C.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Kralee.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Parasite.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/PillBug.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80201418.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve80210978.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve80210980.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve80212278.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve802126B0.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve80212944.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve802129A4.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve80212A24.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve80213320.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/SporbBase.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Sandworm.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve8021887C.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/CommandPirate.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/DarkSamus.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve802188E4.c"),
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
            Object(Matching, "MetroidPrime/ScriptLoader/CoinLoaderSet.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/CannonBall.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/CannonBallLoaderSet.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve802201F8.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80220294.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80220394.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Krocus.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve802274F4.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/AIMannedTurret.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80227530.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/EmperorIngStage1.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve8022756C.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/OctopedeSegment.cpp"),
            Object(Matching, "MetroidPrime/Player/CGameOptionsDefaults.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Rezbit.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/RsfAudio.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/RsfAudioLoaderSet.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80229410.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80229568.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80229BBC.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80229EAC.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/IngPuddle.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80229EE0.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80229EE8.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/FlyerSwarm.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/FlyerSwarmLoaderSet.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/StreamedMovie.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/IngSpiderBallGuardian.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/PuddleSpore.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve8022A3F4.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/EmperorIngStage2Tentacle.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve8022A570.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/BacteriaSwarm.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/MetareeSwarm.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve8022D758.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve8022DA40.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/IngBlobSwarm.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/IngBlobSwarmLoaderSet.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve8022EB54.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/EmperorIngStage3.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/DestructableBarrier.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/SwampBossStage2.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/SwampBossStage1.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/IngBoostBallGuardian.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/PlantScarabSwarm.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/SkyRipple.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/SkyRippleLoaderSet.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/FogOverlay.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80232834.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/MysteryFlyer.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80232868.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/AtomicBeta.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve8023289C.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/EyeBall.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve8023492C.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/DarkSamusBattleStage.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/DarkCommando.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/RubiksPuzzle.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve8023B634.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve8023C860.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve8023C950.c"),
            Object(Matching, "MetroidPrime/Carve80277090.c"),
            Object(Matching, "MetroidPrime/Carve80278C74.c"),
            Object(Matching, "MetroidPrime/Carve80279250.c"),
            Object(Matching, "MetroidPrime/Carve80280338.c"),
            Object(Matching, "MetroidPrime/Carve80281310.c"),
            Object(Matching, "Kyoto/Animation/Carve802B2088.c"),
            Object(Matching, "Kyoto/Animation/Carve802B2568.c"),
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
            Object(Matching, "Kyoto/Math/Carve80335B48.c"),
            Object(Matching, "Kyoto/Math/Carve80335B58.c"),
            Object(Matching, "Kyoto/Math/Carve80337198.c"),
            Object(Matching, "Kyoto/Math/Carve803371A4.c"),
            Object(Matching, "Kyoto/Math/Carve803371F4.c"),
            Object(Matching, "Kyoto/Math/Carve80339D1C.c"),
            Object(Matching, "Kyoto/Math/Carve8033F2CC.c"),
            Object(Matching, "Kyoto/Math/Carve80341284.c"),
            Object(Matching, "Kyoto/Math/Carve803414FC.c"),
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
            Object(NonMatching, "Kyoto/Basics/CBasicsDolphin.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Alloc/CCallStackDolphin.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Basics/COsContextDolphin.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Basics/CSWDataDolphin.cpp"),
            Object(Matching, "Kyoto/Basics/RAssertDolphin.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CDvdRequest.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CDvdRequestManager.cpp"),
            Object(NonMatching, "Kyoto/Graphics/CLight.cpp"),  # Float literal order
            Object(NonMatching, "Kyoto/Graphics/CCubeModel.cpp"),
            Object(Matching, "Kyoto/Graphics/CGX.cpp"),
            Object(NonMatching, "Kyoto/Graphics/DolphinCGraphics.cpp"),
            Object(NonMatching, "Kyoto/Graphics/DolphinCTexture.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CloseEnough.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CMatrix3f.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CMatrix4f.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CNUQuaternion.cpp"),
            Object(NonMatching, "Kyoto/Math/CQuaternion.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CRandom16.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CObjectReference.cpp"),
            Object(NonMatching, "Kyoto/CSimplePool.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CToken.cpp"),
            Object(NonMatching, "Kyoto/Math/CTransform4f.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CUnitVector3f.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CAABox.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CTri.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CQuad.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CCylinder.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CLine.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CPlane.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Math/CSphere.cpp"),
            Object(NonMatching, "Kyoto/CFactoryMgr.cpp"),
            Object(NonMatching, "Kyoto/CResFactory.cpp"),
            Object(Matching, "Kyoto/CResLoader.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CARAMManager.cpp"),
            Object(NonMatching, "Kyoto/Math/CFrustumPlanes.cpp"),
            Object(NonMatching, "Kyoto/Graphics/CCubeMaterial.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Graphics/CCubeSurface.cpp"),
            Object(Matching, "Kyoto/Math/CVector2f.cpp"),
            Object(Matching, "Kyoto/Math/CVector2i.cpp"),
            Object(Matching, "Kyoto/Math/CVector3d.cpp"),
            Object(Matching, "Kyoto/Math/CVector3f.cpp"),
            Object(Matching, "Kyoto/Math/CVector3i.cpp"),
            Object(NonMatching, "Kyoto/Math/RMathUtils.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CCrc32.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Alloc/CCircularBuffer.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Alloc/CMemory.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Alloc/CMediumAllocPool.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Alloc/CSmallAllocPool.cpp"),
            Object(NonMatching, "Kyoto/Alloc/CGameAllocator.cpp"),
            Object(NonMatching, "Kyoto/Animation/DolphinCSkinnedModel.cpp"),
            Object(NonMatching, "Kyoto/Animation/DolphinCSkinRules.cpp"),
            Object(NonMatching, "Kyoto/Animation/DolphinCVirtualBone.cpp"),
            Object(Matching, "Kyoto/Graphics/Carve80310E8C.c"),
            Object(NonMatching, "Kyoto/Graphics/DolphinCModel.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Alloc/IAllocator.cpp"),
            Object(NonMatching, "Kyoto/PVS/CPVSVisOctree.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/PVS/CPVSVisSet.cpp"),
            Object(NonMatching, "Kyoto/DolphinCMemoryCardSys.cpp"),
            Object(Matching, "Kyoto/Input/DolphinIController.cpp"),
            Object(Matching, "Kyoto/Input/CDolphinController.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CSegIdList.cpp"),
            Object(NonMatching, "Kyoto/Animation/CAnimSource.cpp"),
            Object(NonMatching, "Kyoto/Animation/CAnimSourceReader.cpp"),
            Object(NonMatching, "Kyoto/Animation/CAnimSourceReaderBase.cpp"),
            Object(Matching, "Kyoto/Animation/CAnimTreeAnimReaderContainer.cpp"),
            Object(Matching, "Kyoto/Animation/CAnimTreeDoubleChild.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CAnimTreeLoopIn.cpp"),
            Object(Matching, "Kyoto/Animation/CAnimTreeNode.cpp"),
            Object(Matching, "Kyoto/Animation/CAnimTreeSequence.cpp"),
            Object(Matching, "Kyoto/Animation/CAnimTreeSingleChild.cpp"),
            Object(NonMatching, "Kyoto/Animation/CAnimTreeBlend.cpp"),
            Object(NonMatching, "Kyoto/Animation/CAnimTreeTimeScale.cpp"),
            Object(NonMatching, "Kyoto/Animation/CAnimTreeTransition.cpp"),
            Object(NonMatching, "Kyoto/Animation/CAnimTreeTweenBase.cpp"),
            Object(NonMatching, "Kyoto/Animation/IAnimReader.cpp"),
            Object(NonMatching, "Kyoto/Animation/CAnimation.cpp"),
            Object(NonMatching, "Kyoto/Animation/CAnimationSet.cpp"),
            Object(NonMatching, "Kyoto/Animation/CAnimCharacterSet.cpp"),
            Object(NonMatching, "Kyoto/Animation/CCharacterInfo.cpp"),
            Object(NonMatching, "Kyoto/Animation/CCharacterSet.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CAnimPOIData.cpp"),
            Object(NonMatching, "Kyoto/Animation/CCharLayoutInfo.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CHierarchyPoseBuilder.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CMetaAnimBlend.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CMetaAnimPhaseBlend.cpp"),
            Object(Matching, "Kyoto/Animation/CMetaAnimRandom.cpp"),
            Object(NonMatching, "Kyoto/Animation/CMetaAnimSequence.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CMetaAnimFactory.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CMetaAnimPlay.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CMetaTransFactory.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CMetaTransMetaAnim.cpp"),
            Object(NonMatching, "Kyoto/Animation/CMetaTransPhaseTrans.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CMetaTransSnap.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CMetaTransTrans.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/IMetaAnim.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CPrimitive.cpp"),
            Object(Matching, "Kyoto/Animation/CSequenceHelper.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CTransition.cpp"),
            Object(Matching, "Kyoto/Animation/CHalfTransition.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CTransitionManager.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CTreeUtils.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CAllFormatsAnimSource.cpp"),
            Object(NonMatching, "Kyoto/Animation/CFBStreamedAnimReader.cpp"),
            Object(NonMatching, "Kyoto/Animation/CFBStreamedCompression.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CCharAnimMemoryMetrics.cpp"),
            Object(Matching, "Kyoto/Animation/CInt32POINode.cpp"),
            Object(NonMatching, "Kyoto/Animation/CParticlePOINode.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CPASAnimParm.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CPASAnimInfo.cpp"),
            Object(NonMatching, "Kyoto/Animation/CPASAnimState.cpp"),
            Object(Matching, "Kyoto/Animation/CPASDatabase.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CPASParmInfo.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CPOINode.cpp"),
            Object(Matching, "Kyoto/Animation/CSoundPOINode.cpp"),
            Object(NonMatching, "Kyoto/Animation/CPoseAsTransforms_Linear.cpp"),
            Object(Matching, "Kyoto/Particles/CColorElement.cpp"),
            Object(Matching, "Kyoto/Particles/CDeferredParticleEffect.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CRelFileDebugInfo.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CSegId.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Input/CFinalInput.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Graphics/CColor.cpp"),
            Object(Matching, "Kyoto/Graphics/DolphinCColor.cpp"),
            Object(NonMatching, "Kyoto/CDependencyGroup.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Input/CRumbleVoice.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Input/RumbleAdsr.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Input/CRumbleGenerator.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CCharAnimTime.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CTimeRemainderAndFraction.cpp"),
            Object(Matching, "Kyoto/DolphinCDvdFile.cpp"),
            Object(NonMatching, "Kyoto/Graphics/CCubeMoviePlayer.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CAdditiveAnimPlayback.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Particles/CParticleElectricDataFactory.cpp"),
            Object(NonMatching, "Kyoto/Particles/CParticleElectric.cpp"),
            Object(NonMatching, "Kyoto/Particles/CElementGen.cpp"),
            Object(NonMatching, "Kyoto/Particles/CParticleSpawnSystem.cpp"),
            Object(NonMatching, "Kyoto/Particles/CParticleSpawnRandom.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Particles/CParticleSwooshDataFactory.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Particles/CRealElement.cpp"),
            Object(NonMatching, "Kyoto/Particles/CSpawnSystemKeyframeData.cpp"),
            Object(NonMatching, "Kyoto/Particles/CUVElement.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Particles/CVectorElement.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Audio/g721.cpp"),
            Object(Matching, "Kyoto/Audio/CStaticAudioPlayer.cpp"),
            Object(NonMatching, "Kyoto/Audio/DolphinCAudioGroupSet.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Audio/DolphinCAudioSys.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Audio/CStreamAudioManager.cpp"),
            Object(NonMatching, "Kyoto/Audio/CDSPStreamManager.cpp"),
            Object(Matching, "Kyoto/CFrameDelayedKiller.cpp"),
            Object(NonMatching, "Kyoto/Text/CStringTable.cpp"),
            Object(NonMatching, "Kyoto/Text/CRasterFont.cpp"),
            Object(NonMatching, "Kyoto/Text/CTextExecuteBuffer.cpp"),
            Object(NonMatching, "Kyoto/Text/CGuiTextSupport.cpp"),
            Object(NonMatching, "Kyoto/Text/CTextParser.cpp"),
            Object(NonMatching, "Kyoto/Particles/CEmitterElement.cpp"),
            Object(NonMatching, "Kyoto/Particles/CEffectComponent.cpp"),
            Object(Matching, "Kyoto/Particles/CIntElement.cpp"),
            Object(Matching, "Kyoto/Particles/CModVectorElement.cpp"),
            Object(Matching, "Kyoto/Particles/CParticleDataFactory.cpp"),
            Object(NonMatching, "Kyoto/Particles/CParticleGen.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Particles/CParticleGlobals.cpp"),
            Object(NonMatching, "Kyoto/Particles/CParticleSwoosh.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Particles/CParticleData.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CTimeProvider.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/CARAMToken.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Particles/CElectricDescription.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Particles/CSwooshDescription.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Particles/CGenDescription.cpp"),
            Object(NonMatching, "Kyoto/CPakFile.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Audio/CMidiManager.cpp"),
            Object(Matching, "Kyoto/Audio/CSfxHandle.cpp"),
            Object(NonMatching, "Kyoto/Audio/CSfxManager.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CFontImageDef.cpp"),
            Object(Matching, "Kyoto/Text/CTextRenderBuffer.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CDrawStringOptions.cpp"),
            Object(Matching, "Kyoto/Text/CFontRenderState.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CBlockInstruction.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CFont.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CLineInstruction.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CWordInstruction.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CTextInstruction.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CFontInstruction.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CImageInstruction.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CColorInstruction.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CColorOverrideInstruction.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CRemoveColorOverrideInstruction.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CLineSpacingInstruction.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CLineExtraSpaceInstruction.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CPushStateInstruction.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CPopStateInstruction.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CSaveableState.cpp"),
            Object(NonMatching, "Kyoto/Math/CMayaSpline.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Streams/CInputStream.cpp"),
            Object(Matching, "Kyoto/Streams/CBufferedDvdRequest.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Streams/CBitStreamReader.cpp"),
            Object(MatchingFor("G2ME01"), "rstl/rstl_map.cpp"),
            Object(
                MatchingFor("G2ME01"),
                "rstl/rstl_strings.cpp",
                extra_cflags=["-inline deferred"] if config.version == "G2ME01" else [],
            ),
            Object(MatchingFor("G2ME01"), "rstl/rstl_misc.cpp"),
            Object(NonMatching, "rstl/RstlExtras.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Streams/COutputStream.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Streams/CMemoryStreamOut.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Streams/CBitStreamWriter.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Streams/CMemoryInStream.cpp"),
            Object(Matching, "Kyoto/Streams/DolphinCLZOInputStream.cpp"),
            Object(Matching, "Kyoto/Streams/CFilePreload.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Streams/CLZOSupport.cpp", extra_cflags=["-i include/LZO"]),
            # Units carved and matched before the upstream merge, in ranges upstream leaves unsplit.
            Object(Matching, "rstl/rstl_string_l.cpp"),
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
            Object(NonMatching, "Runtime/NMWException.cp", extra_cflags=["-RTTI on", "-Cpp_exceptions on"]),
            Object(MatchingFor("G2ME01"), "Runtime/ptmf.c"),
            Object(MatchingFor("G2ME01"), "Runtime/runtime.c"),
            Object(Matching, "Runtime/__init_cpp_exceptions.cpp"),
            # TODO: need to implement all
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
            Object(MatchingFor("G2ME01"), "Dolphin/os/__ppc_eabi_init.cpp"),
        ],
    ),
    MusyX(
        [
            Object(MatchingFor("G2ME01"), "musyx/runtime/seq.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/synth.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/seq_api.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/snd_synthapi.c"),
            Object(Matching, "musyx/runtime/stream.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/synthdata.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/synthmacros.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/synthvoice.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/synth_ac.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/synth_dbtab.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/synth_adsr.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/synth_vsamples.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/s_data.c"),
            Object(NonMatching, "musyx/runtime/hw_dspctrl.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/hw_volconv.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/snd3d.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/snd_init.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/snd_math.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/snd_midictrl.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/snd_service.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/hardware.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/hw_aramdma.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/dsp_import.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/hw_dolphin.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/hw_memory.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/CheapReverb/creverb_fx.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/CheapReverb/creverb.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/StdReverb/reverb_fx.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/StdReverb/reverb.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/Delay/delay_fx.c"),
            Object(MatchingFor("G2ME01"), "musyx/runtime/Chorus/chorus_fx.c"),
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
            Object(NonMatching, "MetroidPrime/ScriptLoader/Tweaks.cpp"),
        ],
        # Native generated constructors address each float constant separately.
        extra_cflags=["-pool off"],
        mw_version=retro_mw_version,
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
    # MetareeSwarm's head, .text 0x0..0xD8: fn_43_0, fn_43_3C, RELExit, RELMain and the loader
    # registration RELMain calls. Module 43, next to Metaree above. Everything from fn_43_D8
    # (0xD8) up is left unclaimed, so dtk fills it from retail and the module's sha1 still holds.
    Rel(
        "MetareeSwarm",
        [
            Object(NonMatching, "MetroidPrime/ScriptObjects/CMetareeSwarmDes.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CMetareeSwarmRel.cpp"),
        ],
    ),
    # BacteriaSwarm's head, .text 0x0..0xA0: fn_6_0, RELExit, RELMain and the loader registration
    # RELMain calls. Module 6, the same arrangement as IngPuddle below and the same instructions:
    # aligning fn_6_0 on IngPuddle's fn_32_8 leaves exactly two differing encodings, both `bl`, to
    # each module's own loader-setter import. Its setter import is the plain `fn_8022A5AC`, so no
    # symbols.txt rename is needed. Everything from fn_6_A0 (0xA0) up is left unclaimed, so dtk
    # fills it from retail and the module's sha1 still holds.
    Rel(
        "BacteriaSwarm",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CBacteriaSwarmRel.cpp"),
        ],
    ),
    # ChozoGhost's head, .text 0x350..0x488: the eleven accessors above the module's class code,
    # RELExit, RELMain and the loader registration RELMain calls. Module 8, same arrangement as
    # MysteryFlyer below, and fn_8_35C is instruction for instruction CMysteryFlyerRel.cpp's
    # fn_45_10 with this module's own out-of-line optional_object<CAABox> constructor fn_8_55E4.
    # **The claim starts at 0x350, not 0x0**: the three functions below it (fn_8_0, fn_8_D8,
    # fn_8_2BC) are the destructors of the CPatterned / CBodyController / CKnockBackMgr chain and
    # need the actor hierarchy this tree does not model, so 0x0..0x350 stays unclaimed and dtk's
    # auto_00_00000000_text splits in two around this unit - the arrangement ScriptCoin and Metaree
    # already use. Its setter import is the plain fn_80218D24, so no symbols.txt rename is needed.
    # Everything from fn_8_488 (0x488) up is left unclaimed, so dtk fills it from retail and the
    # module's sha1 still holds.
    Rel(
        "ChozoGhost",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CChozoGhostRel.cpp"),
        ],
    ),
    # FogOverlay's head, .text 0x0..0x100: the class's deleting destructor fn_23_0, the vtable-0x38
    # dispatch fn_23_60, RELExit, RELMain and the loader registration RELMain calls. Module 23.
    # **Five functions, not the family's eighteen, and the first function is why**: most heads in
    # this family open with the REL loader generator's thirteen-accessor block (so the recipe's
    # first step is "copy CAtomicAlphaRel.cpp"), and FogOverlay opens with a 0x60-byte deleting
    # destructor. `.data:0x0` says why - 0x7C bytes is two leading zero words plus 29 slots,
    # the plain CActor table, not one of the CAi/generator tables that carry an extra accessor.
    # Its setter import is the plain `fn_80232834` and its loader slot is four bytes, so the
    # BacteriaSwarm registration spelling is right as it stands; `fn_23_8C`/`fn_23_B0` are
    # renamed to RELExit/RELMain with scope:global in the module's symbols.txt, because dtk's own
    # force-active _epilog/_prolog branch through them. Everything from fn_23_100 (0x100) up is
    # left unclaimed, so dtk fills it from retail and the module's sha1 still holds.
    Rel(
        "FogOverlay",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CFogOverlayRel.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CFogOverlayRelStubs.cpp"),
        ],
    ),
    # IngPuddle's head, .text 0x0..0xA8: fn_32_0, fn_32_8, RELExit, RELMain and the loader
    # registration RELMain calls. Module 32, same arrangement as MetareeSwarm above. Everything
    # from fn_32_A8 (0xA8) up is left unclaimed, so dtk fills it from retail and the module's
    # sha1 still holds.
    Rel(
        "IngPuddle",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CIngPuddleRel.cpp"),
        ],
    ),
    # IngSnatchingSwarm's head, .text 0x0..0xA8: fn_33_0, fn_33_8, RELExit, RELMain and the
    # loader registration RELMain calls. Module 33, same arrangement as IngPuddle above and the
    # same bytes instruction for instruction, only the two `bl` targets and the `addi` immediate
    # differ. Everything from fn_33_A8 (0xA8) up is left unclaimed, so dtk fills it from retail
    # and the module's sha1 still holds.
    Rel(
        "IngSnatchingSwarm",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CIngSnatchingSwarmRel.cpp"),
            # IngSnatchingSwarm's state-table run, .text 0x35F8..0x383C: eleven functions, the ten
            # named by the module's own `.data` record tables plus the box-snapshot copier above
            # them. A third contiguous claim, so it needs its own file and its own entry - one unit
            # cannot claim two discontiguous ranges.
            Object(Matching, "MetroidPrime/ScriptObjects/CIngSnatchingSwarmAi.cpp"),
            # The bounds/collision run that sits directly above the state-table one, .text
            # 0x383C..0x3AC0: the hit test against the actor's own box, the `1.0f` box around the
            # swarm's position, the two `CActor` render hooks and `gpRender`'s `AddParticleGen`
            # pair. A fourth contiguous claim, so it needs its own file and its own entry - one
            # unit cannot claim two discontiguous ranges.
            Object(Matching, "MetroidPrime/ScriptObjects/CIngSnatchingSwarmBounds.cpp"),
            # Three leaf accessors of the CParticleGen subclass this module carries as a member
            # (vtable index 12 SetDrawFlags, 20 GetGeneratorRate, 22 GetDrawFlags of the table at
            # .data:0x2E0), .text 0x4F28..0x4F44. A second contiguous claim, so it needs its own
            # file and its own entry - one unit cannot claim two discontiguous ranges. The range
            # is narrow on purpose: the neighbouring functions are this module's class body, which
            # needs the CActor hierarchy the tree does not model, and claiming past them would
            # put bytes in the link this object does not reproduce.
            Object(Matching, "MetroidPrime/ScriptObjects/CIngSnatchingSwarmGenAccessors.cpp"),
        ],
    ),
    # PlantScarabSwarm's head, .text 0x0..0xD8: fn_49_0, fn_49_3C, RELExit, RELMain and the
    # loader registration RELMain calls. Module 49, same arrangement as MetareeSwarm above - and
    # the same bytes instruction for instruction, only the two `bl` targets differ. Everything
    # from fn_49_D8 (0xD8) up is left unclaimed, so dtk fills it from retail and the module's
    # sha1 still holds.
    Rel(
        "PlantScarabSwarm",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CPlantScarabSwarmRel.cpp"),
        ],
    ),
    # AtomicAlpha's head, .text 0x0..0x13C: the fourteen-accessor block, fn_2_9C, RELExit, RELMain
    # and the loader registration RELMain calls. Module 2, same arrangement as PlantScarabSwarm
    # above - except that here the fourteen-accessor block comes *first*, so the claim reaches
    # from 0x0 and is 18 functions rather than 5. Twelve of the fourteen accessors are the
    # bodies AtomicBetaAccessors.cpp already reproduces at 100%; the two that differ are
    # AtomicAlpha's leading ones, at +0x8C8 and +0x7D8. Everything from fn_2_13C (0x13C) up is
    # left unclaimed, so dtk fills it from retail and the module's sha1 still holds.
    Rel(
        "AtomicAlpha",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CAtomicAlphaRel.cpp"),
        ],
    ),
    # MysteryFlyer's head, .text 0x0..0x170: fifteen CMysteryFlyer accessors, RELExit, RELMain
    # and the loader registration RELMain calls. Module 45; the registration is
    # instruction-for-instruction PlantScarabSwarm's, only the two `bl` targets differing.
    # fn_45_10 returns an optional_object<CAABox> whose converting ctor is out of line in
    # retail (fn_45_2BBC), so it is one call, not a template instance; see the source's header.
    # Everything from fn_45_170 (0x170) up is the module's own entity loader and members, left
    # unclaimed so dtk fills it from retail and the module's sha1 still holds.
    Rel(
        "MysteryFlyer",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp"),
        ],
    ),
    # SnakeWeedSwarm's head, .text 0x0..0xDC: fn_71_0, RELExit, RELMain and the loader
    # registration RELMain calls. Module 71, same arrangement as MetareeSwarm above, but its
    # registration fills a 0x1C-byte record (an FScriptLoader and two CodeWarrior
    # pointer-to-member-functions) rather than a four-byte loader slot. Everything from fn_71_DC
    # (0xDC) up is left unclaimed, so dtk fills it from retail and the module's sha1 still holds.
    Rel(
        "SnakeWeedSwarm",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CSnakeWeedSwarmRel.cpp"),
        ],
    ),
    # Lumite's head, .text 0x0..0x190, and the teardown pair at 0x778..0x7C0: fn_39_0, the
    # thirteen-accessor block, RELExit, RELMain and the loader registration RELMain calls, plus
    # fn_39_778/fn_39_798. Module 39, same arrangement as DarkCommando and MysteryFlyer above,
    # with two measured differences and one spelling that is its own. `fn_39_0` **inlines** the
    # optional_object<CAABox> conversion - there is no out-of-line constructor in this module's
    # symbols.txt, unlike MysteryFlyer's fn_45_2BBC - so it is the real
    # `rstl::optional_object<CAABox>` return type that reproduces its 0x68 bytes, the same shape
    # DarkCommando's fn_3_14 has. The registration's loader slot is `lbl_39_bss_40`
    # (`.bss:0x40`, four bytes), **not** `lbl_39_bss_0`, which is the first of the module's five
    # `.bss` objects; and the setter import is the plain `fn_80218BFC`, so no symbols.txt rename
    # is needed. Everything from fn_39_190
    # (0x190, 0x5A8), the module's own entity loader, up is left unclaimed - and fn_39_738
    # (0x738, 0x40) between the two claims stays retail - so dtk fills both gaps from retail and
    # the module's sha1 still holds.
    Rel(
        "Lumite",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CLumiteRel.cpp"),
            # The teardown pair at 0x778..0x7C0 **and fn_39_738 at 0x738**, which needs a
            # different compiler: GC/1.3.2 emits the pair's `stw r31,0xc(r1)` / `mr r31,r3` save
            # in the prologue, above the `lbz` retail has first, and GC/2.7 emits it after the
            # load, instruction for instruction (see the source's header). Same per-object
            # override `CGameOptions.cpp` uses above.
            Object(Matching, "MetroidPrime/ScriptObjects/CLumiteRelTail.cpp", mw_version="GC/2.7"),
        ],
    ),
    # PillBug's head, .text 0x0..0x130: the thirteen short accessors the REL loader generator
    # emits, the vtable call at 0x90, and the loader registration RELMain calls. Module 48, the
    # same arrangement as MetareeSwarm above; its table is CAi's, so the one call goes to vtable
    # slot 0x38 rather than CActor's. Everything from fn_48_130 (0x130) up is left unclaimed, so
    # dtk fills it from retail and the module's sha1 still holds.
    Rel(
        "PillBug",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CPillBugRel.cpp"),
        ],
    ),
    # FishCloud's head, .text 0x0..0xAC: fn_20_0, RELExit, RELMain and the loader registration
    # RELMain calls. Module 20, same arrangement as SnakeWeedSwarm above and the same vtable
    # entry at 0x0 - the CActor `GetHealthInfo` slot calling `HealthInfo` at 0x38, and it is in
    # both of the module's vtables - but its registration fills an 8-byte record of two
    # FScriptLoaders rather than a four-byte slot or SnakeWeed's 0x1C bytes. That record is
    # already spelled in ScriptLoaderRel.hpp, so no local struct is needed. Everything from
    # fn_20_AC (0xAC) up is left unclaimed, so dtk fills it from retail and the module's sha1
    # still holds.
    Rel(
        "FishCloud",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CFishCloudRel.cpp"),
        ],
    ),
    # Tryclops' head, .text 0x0..0x178: the two second-vtable entries, `fn_81_10` (the
    # out-of-line `optional_object<CAABox>` call, as MysteryFlyer's `fn_45_10`), the
    # thirteen-accessor block, the vtable entry 0x3C that calls slot 0x38, RELExit, RELMain and
    # the loader registration RELMain calls. Module 81. Same arrangement as AtomicAlpha above -
    # which is where the accessor bodies come from (0x4C..0xD8 here, 0x10..0x9C there, 35
    # instructions each with an identical multiset). `fn_81_178` (0x178,
    # 0x30C), the module's own entity loader, and the 89 Tryclops methods above it stay unclaimed,
    # so dtk fills them from retail and the module's sha1 still holds. Not in `files.cmake`, for
    # the reason the eight heads above measure.
    Rel(
        "Tryclops",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CTryclopsRel.cpp"),
        ],
    ),
    # Added 2026-09-29. IngSpaceJumpGuardian's head, .text 0x0..0x170: the fifteen-function
    # accessor block, `fn_34_1C` (the out-of-line `optional_object<CAABox>` call, as Tryclops'
    # `fn_81_10` above), the vtable entry 0x3C that calls slot 0x38, RELExit, RELMain and the
    # registration `fn_34_140` RELMain calls. Module 34, the same arrangement as Tryclops above -
    # and its `ldscript.lcf` puts all fifteen of `fn_34_0`..`fn_34_D0` in FORCEACTIVE while
    # `.data:0x3E4` stores them too, so no dead-strip hazard and no `force_active:` entry.
    # **The block is the family in a different order, and that is measured**: it opens with
    # `addi r3,r3,0x8d0` and `li r3,1`, and `fn_34_10` is a **module-local `.rodata` constant**
    # (`.rodata:0x0`, `.float 60`) where the family puts the `GetBoundingBox` wrapper - the
    # `lbl_8041B758` accessor Tryclops has at 0x98 is not here at all. The setter is the plain DOL
    # symbol `fn_8021DC2C` (0x8021DC2C, `stw r3, gLoader_IngSpaceJumpGuardian@sda21(r0)`), so
    # **no `symbols.txt` rename and no DOL change**; the loader slot is `lbl_34_bss_0` at
    # `.bss:0x0`. `fn_34_170` (0x170, 0x330), the module's own entity loader, and the 125
    # CIngSpaceJumpGuardian methods above it stay unclaimed, so dtk fills them from retail and the
    # module's sha1 still holds. Not in `files.cmake`, for the reason the heads above measure.
    Rel(
        "IngSpaceJumpGuardian",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CIngSpaceJumpGuardianRel.cpp"),
        ],
    ),
    # Added 2026-09-30. IngBoostBallGuardian's head, .text 0x0..0x130: the fourteen-function
    # accessor block, the vtable entry 0x3C that calls slot 0x38, RELExit, RELMain and the
    # registration `fn_30_100` RELMain calls. Module 30, the same arrangement as IngSpaceJumpGuardian
    # above - and its `ldscript.lcf` puts all fourteen of `fn_30_0`..`fn_30_90` in FORCEACTIVE, so
    # no dead-strip hazard and no `force_active:` entry; `.data:0x9C0` (0x148 bytes, two leading
    # words) stores `fn_30_90` at offset 0x3C above the `HealthInfo__3CAiFv` slot 0x38 and eight
    # of the other accessors besides. **The block is the family in a different order, and that is
    # measured**: it opens with two address accessors a word apart, `addi r3,r3,0xaec` and
    # `addi r3,r3,0xbd8`, where IngSpaceJumpGuardian opens `addi r3,r3,0x8d0` and `li r3,1`; it
    # has no `GetBoundingBox` wrapper and no `skDamageHitTime__10CPatterned` store at +0x448, so `lbl_8041B758` is
    # the only DOL global its relocations name; and its module-local `.rodata` constant accessor
    # sits at 0x18 (`lbl_30_rodata_64`, `.float 1`) rather than 0x10. The setter is the plain DOL
    # symbol `fn_8022FFC4` (0x8022FFC4, `stw r3, gLoader_IngBoostBallGuardian@sda21(r0)`), so
    # **no `symbols.txt` rename and no DOL change**; the loader slot is `lbl_30_bss_6C` at
    # `.bss:0x6C`, not `.bss:0x0` (this module's `.bss:0x0` is a different 0x8-byte object).
    # `fn_30_130` (0x130, 0x4B4), the module's own entity loader, and the 301
    # CIngBoostBallGuardian methods above it stay unclaimed, so dtk fills them from retail and the
    # module's sha1 still holds. Not in `files.cmake`, for the reason the heads above measure.
    Rel(
        "IngBoostBallGuardian",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CIngBoostBallGuardianRel.cpp"),
        ],
    ),
    # Added 2026-09-29. Blogg's head, .text 0x94..0x108: RELExit, RELMain and the loader
    # registration RELMain calls. Module 7, with no accessor block: the function in front of
    # RELExit is `fn_7_0` (0x0, 0x94), a CDamageVulnerability destructor, so the claim starts
    # above it and 0x0..0x94 stays with dtk. The loader setter is
    # the plain DOL symbol `fn_80218B08`, like Tryclops' `fn_80218D58`, so no `symbols.txt` edit
    # and no DOL change. `fn_7_108` (0x108, 0x96C) and the 207 module methods above it stay
    # unclaimed, so dtk fills them from retail and the module's sha1 still holds. Not in
    # `files.cmake`, for the reason the other landed heads measure.
    Rel(
        "Blogg",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CBloggRel.cpp"),
        ],
    ),
    # Added 2026-09-29. EmperorIngStage3's head, .text 0x0..0xF8: `fn_18_0`, the module's
    # GetBoundingBox wrapper `fn_18_8`, the twelve accessors above it, the vtable entry 0xB4 that
    # calls slot 0x38, and `fn_18_E0`. Module 18. Same arrangement as Krocuss and MysteryFlyer
    # above, and **its accessor block is not byte for byte either of theirs**: this module has no
    # `skDamageHitTime__10CPatterned` float store at +0x448 and no `lbl_8041B758` float accessor, so `kInvalidUniqueId`
    # is the only DOL global its relocations name; it runs four `li r3,0` predicates in a row where
    # Krocuss runs three and Tryclops two, `fn_18_90` is always-true where the family usually puts
    # always-false, and `fn_18_E0` has no counterpart in the family. `fn_18_8` is instruction for
    # instruction `CMysteryFlyerRel.cpp`'s `fn_45_10`, whose out-of-line `optional_object<CAABox>`
    # converting constructor is this module's own `fn_18_DAEC`. **RELMain (0xC330) and RELExit
    # (0xC30C) are in the unclaimed remainder**, not next to this head, so they stay retail -
    # one unit cannot claim two discontiguous ranges. Everything from `fn_18_F8` (0xF8, 0x11C) up
    # is the module's own entity code, left unclaimed so dtk fills it from retail and the module's
    # sha1 still holds. It **is** in `files.cmake`, unlike the other landed heads: it defines no
    # RELMain/RELExit, twelve of its fourteen functions read raw offsets and DOL globals and
    # nothing else, and `fn_18_8` is behind the `#ifdef __MWERKS__` guard `KrocussAccessors.cpp`
    # uses, so the port's undefined count stays at 259.
    Rel(
        "EmperorIngStage3",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CEmperorIngStage3Rel.cpp"),
        ],
    ),
    # Added 2026-09-29. GeomBlobV2's (module 25) entry-point block and its accessor block, in three
    # units, for two measured reasons: `fn_25_2490` - the module's own entity loader, 0xB4 bytes
    # allocating 0x1E0 through `__nw__FUlPCcPCc` - stands between the two blocks and one unit
    # cannot claim two discontiguous ranges, and the accessor block splits a second time because
    # `fn_25_255C` / `fn_25_2564` are not in dtk's FORCEACTIVE list and mwldeppc dead-strips them
    # (measured: claiming all eight left `unit_fit.sh` reporting `fits` and still broke the
    # module's sha1 by 16 bytes). `CGeomBlobV2Rel.cpp` claims 0x23E8..0x2490: `fn_25_23E8` (a
    # vtable entry at 0x3C of both `.data:0x10` and `.data:0xA8`, calling slot 0x38
    # `HealthInfo__6CActorFv`), `RELExit`, `RELMain` and the registration `fn_25_2460`.
    # `CGeomBlobV2Accessors.cpp` claims 0x2544..0x255C and `CGeomBlobV2AccessorsTail.cpp` claims
    # 0x256C..0x2584 - six trivial accessors between them, leaving 0x255C..0x256C (the two float
    # getters) unclaimed. **This module's head is not at 0x0 and its
    # accessors are not the family shape** - both measured, neither assumed: `fn_25_0` (0x0, 0x1FC)
    # is a real bone-blend loop calling `close_enough__FRC11CQuaternionRC11CQuaternionf`, and none
    # of Krocuss's / MysteryFlyer's / AtomicAlpha's thirteen-accessor set (no `kInvalidUniqueId`
    # store, no `+0x44f` byte, no `skDamageHitTime__10CPatterned` / `lbl_8041B758` floats) appears here. The two
    # loaders are both unnamed DOL setters, `fn_80229EAC` (0x80229EAC, `stw r3, lbl_80419590`) and
    # `fn_802274FC` (0x802274FC, `stw r3, lbl_80419558`), so no `symbols.txt` rename and no DOL
    # change. `fn_25_48A8` / `fn_25_48CC` (the second loader's teardown and registration) and
    # `fn_25_2490` are in the unclaimed remainder and are called by name. Everything else in the
    # module - 115 of its 130 text symbols (`audit_rel_claim.py`: 15 of 130 claimed, 4 + 3 + 3 ours
    # plus the 5 already-claimed `REL_Setup` functions) - stays retail, so dtk fills it and the sha1
    # still holds. `CGeomBlobV2Rel.cpp` is **not** in `files.cmake`, for the reason the other
    # landed heads measure: it calls `fn_25_2490` and `fn_80229EAC`, which the port cannot link.
    # `CGeomBlobV2Accessors.cpp` **is**, because it defines no RELMain/RELExit and relocates
    # against nothing outside itself.
    Rel(
        "GeomBlobV2",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CGeomBlobV2Rel.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CGeomBlobV2Accessors.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CGeomBlobV2AccessorsTail.cpp"),
        ],
    ),
    # Added 2026-09-29. SandBoss's (module 55) head, .text 0x0..0x178: the seventeen-function
    # accessor block, `fn_55_10` (the out-of-line `optional_object<CAABox>` call, as
    # MysteryFlyer's `fn_45_10` above), the vtable entry 0xD8 that calls slot 0x38, RELExit,
    # RELMain and the registration `fn_55_148` RELMain calls. Module 55, the same arrangement
    # as MysteryFlyer above - **and its accessor block is the family in a different order,
    # which is measured** from diffing `build/G2ME01/SandBoss/asm/auto_00_00000000_text.s`
    # against `CMysteryFlyerRel.cpp`'s: it opens with `li r3,1` twice where MysteryFlyer opens
    # with `li r3,1` then `+0x818`, so it covers one member fewer at the front, and it runs
    # three `li r3,0` predicates in a row, which pushes `kInvalidUniqueId` from 0x74 to 0x7C and
    # every accessor above it by 8 bytes. The setter is the plain DOL symbol `fn_802189D0`
    # (0x802189D0, `stw r3, gLoader_SandBoss@sda21(r0)`), so **no `symbols.txt` rename and no
    # DOL change**; the loader slot is `lbl_55_bss_4` at `.bss:0x4`, not `.bss:0x0` as in
    # MysteryFlyer, because this module's `.bss` holds three objects. `fn_55_178` (0x178,
    # 0x33C), the module's own entity loader, and its 298 class functions above it stay
    # unclaimed (322 text symbols: 19 ours, 5 `REL_Setup`, 298 unclaimed), so dtk fills them
    # from retail and the module's sha1 still holds. Not in `files.cmake`, for the reason the
    # other landed heads measure: it calls `fn_55_178` and `fn_802189D0`, which the port cannot
    # link.
    Rel(
        "SandBoss",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CSandBossRel.cpp"),
        ],
    ),
    # Added 2026-09-30 (goal item `progress-rel-head-sandworm`). 4 functions, .text
    # 0x000000..0x0000DC: `fn_56_0`'s vtable call on slot 0x38, and RELExit, RELMain and the
    # loader registration `fn_56_70`. Module 56, which had **no `Rel(...)` block at all** before
    # this, so its seven functions were the shared `REL_Setup` and `global_destructor_chain` units
    # and nothing of ours.
    #
    # **Placed here, next to SandBoss, rather than appended at the end of the `Rel` list.** The
    # list has no order requirement - each `Rel(...)` block is an independent module definition -
    # but a block appended before the closing `]` shares its patch context with every other lane
    # that appends there, and `tools/run_goal.sh`'s `rebase_onto_tip` carries a judged change onto
    # a moved tip with `git apply --3way`, union-merging **only** docs conflicts: a
    # `configure.py` conflict releases the item for a fresh attempt. Two attempts at this item
    # passed `goal_check.sh` and review, then were lost exactly there, against two different
    # tips, because Splinter and this block both appended at the end. This anchor is a committed
    # 2026-09-29 block that no lane touches, so the same carry applies cleanly.
    #
    # **The claim starts at 0x0 and the arrangement is the shortest head in this family by a wide
    # margin - four functions, where Splitter's is fourteen, Ing's seventeen and SandBoss's
    # seventeen - read off `build/G2ME01/Sandworm/asm/auto_00_00000000_text.s` and not off the
    # `fn_<id>_<off>` names, which say nothing about which function is which.** There is **no
    # accessor block above the four functions at all**: `.text` opens with `fn_56_0` itself, so the
    # unit is 0xDC over 4 functions, not 0x170 over 18. Measured before anything was written:
    # diffing that dump's 0x0..0xDC against
    # `build/G2ME01/SnakeWeedSwarm/asm/auto_00_00000000_text.s` over the same range with every
    # identifier masked gives 55 instructions on each side, 53 identical in opcode and operands,
    # and the two that differ are the `bl` at 0x3C and the `bl` at 0xC8 - the module's own setter
    # name and nothing else. Every body below is one `CSnakeWeedSwarmRel.cpp` already reproduces at
    # 100.00%, so no spelling had to be discovered.
    #
    # **The record is 0x1C bytes at `.bss:0x38`** (`lbl_56_bss_38`, `size:0x1C` per
    # `build/G2ME01/Sandworm/asm/auto_05_00000000_bss.s`) - an `FScriptLoader` plus two 12-byte
    # CodeWarrior pointer-to-member-functions, the same seven words `CSnakeWeedSwarmRel.cpp`
    # fills. **`.bss:0x0` is not it**: this module's `.bss` holds seven objects and the sixth is
    # the one `fn_56_70` stores through, so the offset was read off the dump. Unlike
    # `CFishCloudRel.cpp`'s and `SetLoader_FishCloud`'s, the setter here has **no C++ body in this
    # tree**, so it has no mangled name in `config/G2ME01/symbols.txt` either and the declaration
    # is what mwcceppc mangles: the file renames the one DOL token at 0x8021887C to
    # `SetLoader_Sandworm__FP18SSandworm_FuncPtrs` (`stw r3, gLoader_Sandworm@sda21(r0); blr`,
    # immediately after `LoadSandworm__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at
    # 0x80218850, which is 0x2C bytes and so ends exactly there). The unit holding 0x8021887C is
    # unclaimed, so no DOL byte moves. The two members are each read by exactly one three-line DOL
    # thunk - `fn_80218818` at +0x4 (`__ptmf_scall4`) and `fn_802187EC` at +0x10
    # (`__ptmf_scall`) - both called only from `fn_8012F3C0` at 0x8012FA70/0x8012FA98, whose bound
    # is the +0x10 call's return value, next to `TCastToPtr<12CSandwormEye>__FP7CEntity`; so the
    # 12-byte size is read, and the class those two hang off is not determined by the bytes, which
    # the source file says. **No dead-strip hazard**: `build/G2ME01/Sandworm/ldscript.lcf` lists
    # `fn_56_0` (and `fn_56_DC`) in its FORCEACTIVE block, `.data:0x6F8` - CSandwormEye's own
    # 0x98-byte vtable - and `.data:0x884` - CSandworm's 0x240-byte one - each store `fn_56_0` at
    # offset 0x3C above a `HealthInfo` slot at 0x38 (`HealthInfo__6CActorFv` and
    # `HealthInfo__3CAiFv`), RELMain/RELExit are the module's entry points and `fn_56_70` is
    # called from RELMain, so all four survive and nothing needs a `force_active:` entry in
    # `config/G2ME01/config.yml`. The one unclaimed callee, `fn_56_DC` (0xDC, 0xF6C), is the
    # module's own entity loader and is held the way the rest of the family holds it: by
    # `fn_56_70`. Not in `files.cmake`, for the reason the other heads measure: it calls
    # `fn_56_DC` and `SetLoader_Sandworm`, which the port cannot link.
    Rel(
        "Sandworm",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CSandwormRel.cpp"),
        ],
    ),
    # Added 2026-09-29. Splitter (module 75), two units. `CSplitterRel.cpp` is the head, .text
    # 0x0..0xFC: the fifteen functions below the module's class code - two member-address
    # accessors (+0xD6C, +0xE5C), then `fn_75_18`, the out-of-line `optional_object<CAABox>` call
    # as in MysteryFlyer's `fn_45_10` and SandBoss's `fn_55_10` above, the family's accessors, and
    # `fn_75_D0`, which calls vtable slot 0x38. **RELExit/RELMain are not next to the accessors**:
    # `fn_75_FC` (0xFC, 0x370), the module's entity loader, follows the head, and RELExit/RELMain
    # sit at 0x8210/0x8234, so `CSplitterRelMain.cpp` is a second unit claiming 0x81F8..0x82B0.
    # Its registration fills a 0x14-byte record (`lbl_75_bss_20`) whose two loaders are named from
    # the `Matching` reader src/MetroidPrime/ScriptLoader/SplitterMainChassis.cpp, and hands it to
    # the plain DOL setter `fn_80218CF0`, so there is no `symbols.txt` rename and no DOL change.
    # `fn_75_7C44`, `fn_75_FC` and everything between the two ranges stay unclaimed; dtk fills
    # them from retail and the module's sha1 holds. `CSplitterRel.cpp` has no module entry point,
    # so it is in `files.cmake` with its REL-internal calls under `__MWERKS__`, as the Accessors
    # units are; `CSplitterRelMain.cpp` is out for the reason the other landed heads measure.
    Rel(
        "Splitter",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CSplitterRel.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CSplitterRelMain.cpp"),
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
    # Added 2026-09-29. The module head only: RELExit, RELMain and the loader-registration
    # function RELMain calls, .text 0x64..0xD8, the same arrangement as CScriptPlayerProxy.cpp.
    # The behavioural class code needs the CIngBlobSwarm/CActor/CPatterned hierarchy and is
    # left to the unclaimed auto_* ranges, so the module still hashes to config.yml.
    Rel(
        "IngBlobSwarm",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptIngBlobSwarmRel.cpp"),
        ],
    ),
    # Added 2026-09-29 (goal item `progress-rel-head-darktrooper`). 16 functions, .text
    # 0x000000..0x00012C: the module head - the twelve short accessors the REL loader generator
    # emits (PillBug's set, re-ordered, plus one `fn_12_38` PillBug has not), `fn_12_8C`'s vtable
    # call on slot 0x38, and RELExit, RELMain and the loader registration `fn_12_FC`. The
    # behavioural class code needs the CActor/CPatterned/CAi hierarchy and is left to the
    # unclaimed auto_* ranges, so the module still hashes to config.yml.
    Rel(
        "DarkTrooper",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CDarkTrooperRel.cpp"),
        ],
    ),
    # Added 2026-09-29 (goal item `progress-rel-head-destructiblebarrier`). 4 functions, .text
    # 0x000000..0x0000A0: the module head - `fn_13_0`, the vtable call on slot 0x38, and RELExit,
    # RELMain and the loader registration `fn_13_70`. Module 13, and its head is
    # `CBacteriaSwarmRel.cpp`'s instruction for instruction: 40 instructions against 40, with 7
    # differing lines, all of them a symbol name. Its setter import is the plain `fn_8022EBFC`
    # (`stw r3, gLoader_DestructableBarrier; blr`), so no symbols.txt rename is needed. Everything
    # from fn_13_A0 (0xA0) up is left unclaimed, so dtk fills it from retail and the module's
    # sha1 still holds.
    Rel(
        "DestructibleBarrier",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CDestructibleBarrierRel.cpp"),
        ],
    ),
    # Added 2026-09-29 (goal item `progress-rel-head-elitepirate`). 19 functions, .text
    # 0x000000..0x000178: the module head - fourteen short accessors, `fn_15_10`'s GetBoundingBox
    # wrapper, `fn_15_D8`'s vtable call on slot 0x38, and RELExit, RELMain and the loader
    # registration `fn_15_148`. Module 15, and its head is `CMysteryFlyerRel.cpp`'s re-ordered: 94
    # instructions against 92 over the ranges each claims (0x000000..0x000178 and
    # 0x000000..0x000170), the only differences being two leading member-address accessors, one
    # extra `li r3,0` predicate, and the four `bl` displacements. `fn_15_10` returns an
    # optional_object<CAABox> whose converting ctor is out of line in retail (fn_15_C094,
    # .text 0xC094), so it is one call, not a template instance; see the source's header. Its
    # setter import is the plain `fn_80218AD4`
    # (`stw r3, gLoader_ElitePirate; blr`), so no symbols.txt rename is needed. Everything from
    # fn_15_178 (0x178, 0xACC) up - the module's own entity loader and its class - is left
    # unclaimed, so dtk fills it from retail and the module's sha1 still holds.
    Rel(
        "ElitePirate",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CElitePirateRel.cpp"),
        ],
    ),
    # Added 2026-09-29 (goal item `progress-rel-head-mediuming`). 15 functions, .text
    # 0x000000..0x000150: the module head - the twelve short accessors the REL loader generator
    # emits, `fn_41_8` (the out-of-line `optional_object<CAABox>` call, as
    # `CIngSpaceJumpGuardianRel`'s `fn_34_1C` above), `fn_41_B0`'s vtable call on slot 0x38, and
    # RELExit, RELMain and the loader registration `fn_41_120`. Module 41, and its block is the
    # family's **in a different order**, measured by diffing dtk's `auto_00_00000000_text.s`
    # against `CMysteryFlyerRel.cpp`'s rather than read off the `fn_<id>_<off>` names: it opens
    # `addi r3,r3,0x7c0` and then the `GetBoundingBox` wrapper where MysteryFlyer opens
    # `li r3,1` / `addi r3,r3,0x818`, it runs three `li r3,0` predicates in a row, and it has
    # **no** `skDamageHitTime__10CPatterned` store at +0x448 and no `li r3,1` at all. **No dead-strip hazard**:
    # the module's `ldscript.lcf` puts all twelve of `fn_41_0`..`fn_41_B0` in FORCEACTIVE and
    # `.data:0x5C8` stores every one of them, so no `force_active:` entry is needed (the trap
    # `CGeomBlobV2` hit). The setter import is the plain DOL symbol `fn_80218A6C`
    # (`stw r3, gLoader_MediumIng; blr`, immediately after `LoadMediumIng` at 0x80218A40), so no
    # `symbols.txt` rename and no DOL change; the loader slot is `lbl_41_bss_10` at `.bss:0x10`,
    # not `.bss:0x0`. `fn_41_150` (0x150, 0x868) is the module's own entity loader and the 160
    # functions above it are its methods; all stay retail - behavioural class code needing the
    # CActor/CPatterned/CAi hierarchy. Not in `files.cmake`, for the reason the other heads
    # measure.
    Rel(
        "MediumIng",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CMediumIngRel.cpp"),
        ],
    ),
    # Added 2026-09-30 (goal item `progress-rel-head-minoring`). 15 functions, .text
    # 0x000000..0x000110: the module head - the twelve accessors the REL loader generator emits,
    # `fn_44_70`'s vtable call on slot 0x38, and RELExit, RELMain and the loader registration
    # `fn_44_E0`. Module 44. **The claim starts at 0x0, unlike `CChozoGhostRel`'s at 0x350** -
    # MinorIng's `.text` opens on the accessor block with no destructor chain below it, so dtk's
    # `auto_00_00000000_text` splits once, into ours at `0x0..0x110` and retail's at
    # `0x110..0xB484`. The block is `CAtomicAlphaRel.cpp`'s with four measured differences: the
    # leading accessors are at +0x960 and +0xA4C (not +0x8C8/+0x7D8), the third function is
    # `li r3,1` where AtomicAlpha's third is the float store, there is one `li r3,0` predicate
    # after the byte read where AtomicAlpha has two, and this module has no three-float copy and
    # no `optional_object<CAABox>` wrapper - so the head stops at 0x110, not AtomicAlpha's 0x13C.
    # **No dead-strip hazard**: the module's `ldscript.lcf` puts all twelve of `fn_44_0`..`fn_44_70`
    # in FORCEACTIVE and `.data:0x620` - its own 0x148-byte CPatterned vtable - stores every one of
    # them, so nothing needs a `force_active:` entry in `config/G2ME01/config.yml`. The import is
    # the plain DOL symbol `fn_80218AA0` (`stw r3, gLoader_MinorIng; blr`, immediately after
    # `LoadMinorIng` at 0x80218A74), so no `symbols.txt` rename and no DOL change; the loader slot
    # is `lbl_44_bss_84` at `.bss:0x84`, the module's only `data:4byte` slot, not `.bss:0x0`.
    # `fn_44_110` (0x110, 0x844) is the module's own entity loader and the 194 functions above it
    # are its methods; all stay retail - behavioural class code needing the
    # CActor/CPatterned/CAi hierarchy. Not in `files.cmake`, for the reason the other heads measure.
    Rel(
        "MinorIng",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CMinorIngRel.cpp"),
        ],
    ),
    # Added 2026-09-29 (goal item `progress-rel-head-metroid`). 18 functions, .text
    # 0x000000..0x00017C: the module head - the short accessors the REL loader generator emits,
    # `fn_40_10` (the out-of-line `optional_object<CAABox>` call, as `CMysteryFlyerRel`'s
    # `fn_45_10`), `fn_40_BC`'s vtable call on slot 0x38, and RELExit, RELMain and the loader
    # registration `fn_40_12C`. Module 40. **The record is 0x10 bytes, not the four bytes most of
    # this family uses**: the setter `fn_80218B68` stores the *address* of a record the DOL reads
    # twice - `LoadMetroidAlpha` calls word 0 as the loader, and `OnDockTouch__13CMetroidAlpha`
    # (0x80218B10) does `addi r12, r5, 0x4; bl __ptmf_scall` on it, so words 4..15 are a
    # CodeWarrior pointer-to-member-function, copied out of `.data:0x358` as
    # `CSnakeWeedSwarmRel.cpp` copies its two. The import is the plain DOL symbol `fn_80218B68`
    # (`stw r3, gLoader_MetroidAlpha; blr`, immediately after `LoadMetroidAlpha` at 0x80218B3C), so
    # no `symbols.txt` rename and no DOL change; the loader slot is `lbl_40_bss_10` at `.bss:0x10`,
    # not `.bss:0x0`. **No dead-strip hazard**: the module's `ldscript.lcf` puts all fifteen of
    # `fn_40_0`..`fn_40_BC` in FORCEACTIVE and `.data:0x364` stores every one of them, so nothing
    # needs a `force_active:` entry. `fn_40_17C` (0x17C, 0x79C) is the module's own entity loader
    # and the 149 functions above it are its methods; all stay retail - behavioural class code
    # needing the CMetroidAlpha/CActor/CPatterned/CAi hierarchy. Not in `files.cmake`, for the
    # reason the other heads measure.
    Rel(
        "Metroid",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CMetroidRel.cpp"),
        ],
    ),
    # Added 2026-09-29 (goal item `progress-rel-head-rezbit`). 17 functions, .text
    # 0x000000..0x000168: the module head - the short accessors the REL loader generator emits,
    # `fn_53_10` (the out-of-line `optional_object<CAABox>` call, as `CMysteryFlyerRel`'s
    # `fn_45_10`), `fn_53_C8`'s vtable call on slot 0x38, and RELExit, RELMain and the loader
    # registration `fn_53_138`. Module 53. **The record is four bytes, the family's usual size,
    # and the `.bss` dump is what establishes it** (`lbl_53_bss_0`, `.bss:0x0`, `size:0x4` in
    # `build/G2ME01/Rezbit/asm/auto_05_00000000_bss.s`, where `CMetroidRel.cpp` has to place its
    # slot at `.bss:0x10`) - the only reader of the slot is `LoadRezbit` in the `Matching` unit
    # `src/MetroidPrime/ScriptLoader/Rezbit.cpp`, so there is no second reader and no pmf. The
    # import is the plain DOL symbol `fn_80227AF8` (0x80227AF8, 8 bytes, immediately after
    # `LoadRezbit__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80227ACC, which is 44
    # bytes, `stw r3, gLoader_Rezbit@sda21(r0); blr`), so no `symbols.txt` rename and no DOL
    # change. **No dead-strip hazard**: the module's `ldscript.lcf` puts all fifteen of
    # `fn_53_0`..`fn_53_C8` in FORCEACTIVE, so nothing needs a `force_active:` entry.
    # `fn_53_168` (0x168, 0x330) is the module's own entity loader and the 146 functions above it
    # are its methods; all stay retail - behavioural class code needing the
    # CActor/CPatterned hierarchy. Not in `files.cmake`, for the reason the other heads measure.
    # **Its accessor block is MysteryFlyer's with two measured differences**: the two leading
    # eight-byte accessors are `addi r3,r3,0xac0` then `li r3,1` (MysteryFlyer `li r3,1` then
    # `addi r3,r3,0x818`), and the block runs two `li r3,0` predicates where MysteryFlyer runs
    # three - so the three-float copy sits at 0xAC rather than 0xB4 and the claim ends at 0x168.
    Rel(
        "Rezbit",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CRezbitRel.cpp"),
        ],
    ),
    # Added 2026-09-29 (goal item `progress-rel-head-swampbossstage1`). 17 functions, .text
    # 0x000000..0x000160: the module head - the fourteen short accessors the REL loader
    # generator emits, `fn_78_8` (the out-of-line `optional_object<CAABox>` call, as
    # `CMysteryFlyerRel`'s `fn_45_10`), `fn_78_C0`'s vtable call on slot 0x38, and RELExit,
    # RELMain and the loader registration `fn_78_130`. Module 78. **The record is four bytes,
    # the family's usual size, and the `.bss` dump is what establishes it** - this module's
    # `.bss` holds three objects (`lbl_78_bss_0` size 0x8 at 0x0, `lbl_78_bss_8` size 0x18 at
    # 0x8, `lbl_78_bss_20` size 0x4 at 0x20 in
    # `build/G2ME01/SwampBossStage1/asm/auto_05_00000000_bss.s`) and it is the **last** one
    # `fn_78_130` stores through, not `.bss:0x0` as in MysteryFlyer. The only reader of the
    # slot is `LoadSwampBossStage1` in the `Matching` unit
    # `src/MetroidPrime/ScriptLoader/SwampBossStage1.cpp`, so there is no second reader and no
    # pmf. The import is the plain DOL symbol `fn_8022EC64` (0x8022EC64, 8 bytes, `stw r3,
    # gLoader_SwampBossStage1@sda21(r0); blr`, immediately after
    # `LoadSwampBossStage1__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x8022EC38,
    # which is 44 bytes and so ends exactly there), so no `symbols.txt` rename and no DOL
    # change. **No dead-strip hazard**: the module's `ldscript.lcf` puts all fourteen of
    # `fn_78_0`..`fn_78_C0` in FORCEACTIVE and `.data:0x510` stores every one of them, so
    # nothing needs a `force_active:` entry. `fn_78_160` (0x160, 0x314) is the module's own
    # entity loader and the 229 functions above it are its methods; all stay retail -
    # behavioural class code needing the CActor/CPatterned hierarchy. Not in `files.cmake`, for
    # the reason the other heads measure. **Its accessor block is MysteryFlyer's with two
    # members dropped and one predicate gained**, established by diffing
    # `build/G2ME01/SwampBossStage1/asm/auto_00_00000000_text.s` over the 0x160 this claims
    # against `CMysteryFlyerRel.cpp`'s 0x170 rather than from the `fn_<id>_<off>` names: it
    # opens `li r3,1` and goes straight to the `GetBoundingBox` wrapper (MysteryFlyer opens
    # `li r3,1` then `addi r3,r3,0x818`), it has **no** `skDamageHitTime__10CPatterned` store at +0x448, and it
    # runs **three** `li r3,0` predicates to MysteryFlyer's two - 55 instructions over 14
    # accessors here against 59 over 15 there, differing by exactly one `li r3,0` gained, one
    # `addi r3,r3,0x818` lost and the four-instruction float store lost. So the block is
    # 0x0..0xEC and the head 0x0..0x160, not 0x0..0x170: seventeen functions with nothing
    # missing. **251 is the module's complete text symbol count**: 17 ours + 229 unclaimed
    # + 5 setup, which is exactly the sum of its units' `total_functions` in
    # `build/report.json`.
    Rel(
        "SwampBossStage1",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CSwampBossStage1Rel.cpp"),
        ],
    ),
    # Added 2026-09-29 (goal item `progress-rel-head-swampbossstage2`). 18 functions, .text
    # 0x000000..0x000170: the module head - the fifteen short accessors the REL loader
    # generator emits, `fn_79_8` (the out-of-line `optional_object<CAABox>` call, as
    # `CMysteryFlyerRel`'s `fn_45_10`), `fn_79_D0`'s vtable call on slot 0x38, and RELExit,
    # RELMain and the loader registration `fn_79_140`. Module 79. **The record is four bytes,
    # the family's usual size, and the `.bss` dump is what establishes it** - this module's
    # `.bss` holds four objects (`lbl_79_bss_0` size 0x4 at 0x0, `lbl_79_bss_4` size 0x1 at
    # 0x4, an unnamed 3-byte gap at 0x5, and `lbl_79_bss_8` size 0x4 at 0x8 in
    # `build/G2ME01/SwampBossStage2/asm/auto_05_00000000_bss.s`) and it is the **second** of the
    # four `fn_79_140` stores through, not `.bss:0x0` as in MysteryFlyer. The only reader of the
    # slot is `LoadSwampBossStage2` in the `Matching` unit
    # `src/MetroidPrime/ScriptLoader/SwampBossStage2.cpp`, so there is no second reader and no
    # pmf. The import is the plain DOL symbol `fn_8022EC30` (0x8022EC30, 8 bytes, `stw r3,
    # gLoader_SwampBossStage2@sda21(r0); blr` - see
    # `build/G2ME01/asm/auto_03_8022EC30_text.s`), immediately *before*
    # `LoadSwampBossStage2__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x8022EC04,
    # which is 44 bytes and so ends exactly there, so no `symbols.txt` rename and no DOL
    # change. **No dead-strip hazard**: `.data:0x424` (`lbl_79_data_424`, 0x148 bytes) stores
    # every one of `fn_79_0`..`fn_79_D0`, so nothing needs a `force_active:` entry and
    # `config.yml` carries none for this module. `fn_79_170` (0x170, 0x2F0) is the module's own
    # entity loader and the 220 functions above it are its methods; all stay retail -
    # behavioural class code needing the CActor/CPatterned hierarchy. Not in `files.cmake`,
    # for the reason the other heads measure. **Its accessor block is MysteryFlyer's with the
    # `+0x818` member accessor traded for a fourth `li r3,0` predicate**, established by
    # diffing `build/G2ME01/SwampBossStage2/asm/auto_00_00000000_text.s` over 0x0..0xFC
    # against `CMysteryFlyerRel.cpp`'s over the same range rather than from the
    # `fn_<id>_<off>` names: 63 instructions over 15 accessors on both sides, differing by
    # exactly one `addi r3, r3, 0x818` lost and one `li r3, 0x0` gained. Against
    # `CSwampBossStage1Rel.cpp` (59 over 14, 0x0..0xEC) the only difference is this module's
    # four-instruction `skDamageHitTime__10CPatterned` store at +0x448. So the block is 0x0..0xFC and the head
    # 0x0..0x170: eighteen functions with nothing missing. **243 is the module's complete text
    # symbol count**: 18 ours + 220 unclaimed + 5 setup, which is exactly the sum of its units'
    # `total_functions` in `build/report.json`.
    Rel(
        "SwampBossStage2",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CSwampBossStage2Rel.cpp"),
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
    # DarkSamusBattleStage (module 11), the module right after DarkSamus above. Its head,
    # .text 0x000000..0x00000074: **three functions, not the family's fifteen accessors** -
    # RELExit (fn_11_0, `li r3,0 / bl fn_80235DCC`), RELMain (fn_11_24, `bl fn_11_44`) and
    # the loader registration fn_11_44 (`lbl_11_bss_0 = fn_11_74 ; fn_80235DCC(&lbl_11_bss_0)`).
    # That is measured, not assumed: the class is a CScript stage object over a `CEntity`
    # base, so its vtable is the 0x20-byte table at `.data:0x0` holding six virtuals
    # (`fn_11_B78`, `TypesMatch__27CScriptDarkSamusBattleStageCFi`, `PreThink`, `Think`,
    # `AcceptScriptMsg`, `SetActive`) after two leading words - not a `CActor`'s fourteen.
    # Hence no `GetBoundingBox` wrapper, no `+0x818` accessor and no `skDamageHitTime__10CPatterned` store,
    # and nothing missing: `fn_11_74` (0x74, 0x150) is already the module's entity loader.
    # Everything above it stays retail, so dtk fills it and the module's sha1 still holds.
    # **The record is four bytes and the `.bss` dump settles it without a choice** - this
    # module's `.bss` holds exactly one object, `lbl_11_bss_0` `size:0x4` at 0x0, and it is
    # the loader slot, where `CSwampBossStage1Rel.cpp` had to read the offset off the dump to
    # find the loader was the *last* of three. The import is the plain DOL symbol
    # `fn_80235DCC` (0x80235DCC, 8 bytes, `stw r3, gLoader_DarkSamusBattleStage@sda21(r0);
    # blr` - `build/G2ME01/asm/auto_03_80235DCC_text.s`), so no `symbols.txt` rename and no
    # DOL change. `symbols.txt` gains only RELExit/RELMain at 0x0/0x24, which the shared
    # `REL_Setup` tail also needs. Not in `files.cmake`, for the reason the other heads
    # measure. The module's tail at 0xD94..0xF38 is claimed separately by the shared "REL"
    # lib's `REL/REL_Setup.cpp` (the free 0 -> 5), which is why `symbols.txt` also gains
    # `ModuleDestructors` and `ModuleConstructors` there. **The module's 20 text symbols
    # therefore split 3 ours + 5 `REL_Setup` + 12 unclaimed**, which is exactly the sum of
    # its units' `total_functions` in `build/report.json`.
    Rel(
        "DarkSamusBattleStage",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptDarkSamusBattleStageRel.cpp"),
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
        # 15 functions at the head of the module, .text 0x000000..0x0000D8: fn_19_0, the
        # module's GetBoundingBox wrapper, and the fourteen short accessors above it. The fourteen
        # accessors are byte for byte WallCrawler's 0x00..0x9C, which CScriptWallCrawler.cpp
        # already reproduces at 100% as a Matching unit; fn_19_0 is instruction for instruction
        # CMysteryFlyerRel.cpp's fn_45_10. Everything else in the module is left unclaimed, so dtk
        # fills it from retail and the module's sha1 is unchanged.
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
        # 16 functions, .text 0x000000..0x00010C: `fn_14_10` (the module's GetBoundingBox
        # wrapper, which is the only one of these wrappers that *inlines* the
        # `optional_object<CAABox>` conversion rather than calling an out-of-line constructor),
        # `fn_14_0` and `fn_14_8`, and the 13 short accessors below them - the accessor set the
        # REL loader generator emits, byte-identical to WallCrawler's 0x00..0x9C, which
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
        # 15 functions, .text 0x000000..0x0000D8: fn_35_0 and fn_35_8, the module's
        # GetBoundingBox wrapper (instruction for instruction CMysteryFlyerRel.cpp's fn_45_10)
        # and the predicate in front of it, then the 13 short accessors the REL loader
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
        # 15 functions, .text 0x000000..0x0000D8: fn_38_0, the module's GetBoundingBox wrapper
        # (instruction for instruction CMysteryFlyerRel.cpp's fn_45_10), then the 14 short
        # accessors the REL loader generator emits, byte-identical to WallCrawler's 0x00..0x9C,
        # which MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching
        # unit. Everything else in the module is left unclaimed, so dtk fills it from retail.
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
        # 15 functions, .text 0x000000..0x0000D8: fn_54_0, the module's GetBoundingBox wrapper
        # (instruction for instruction CMysteryFlyerRel.cpp's fn_45_10), then the 14 short
        # accessors this *Accessors.cpp family shares, which MetroidPrime/ScriptObjects/
        # CScriptWallCrawler.cpp already reproduces as a Matching unit.
        # **The module entry is not next to them**, which is why this is two units:
        # CRipperRelMain.cpp claims .text 0x0000D8..0x000178, and 0xD8 sits *inside* dtk's
        # auto_00_000000D8_text, so that is a sub-range carve of one existing auto unit rather than
        # a new head - the arrangement CSplitterRelMain.cpp needed for the same reason in Splitter.
        # The setter is the **long MWCC-mangled** import
        # SetLoader_Ripper__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity (the DOL's
        # 0x8021BBD0, size:0x8, `stw r3, gLoader_Ripper; blr`), checked in the module's own
        # build/G2ME01/Ripper/Ripper.preplf import table, so unlike CBacteriaSwarmRel.cpp and
        # CTryclopsRel.cpp - which import the plain `fn_802...` name - the identifier has to be
        # spelled out in full inside an extern "C" block. That is the CIngSnatchingSwarmRel.cpp
        # arrangement; no symbols.txt rename and no DOL change is needed either way. The loader
        # slot is lbl_54_bss_0 at .bss:0x0.
        # CRipperForwarders.cpp claims .text 0x00000514..0x0000055C, fn_54_514 and fn_54_534, two
        # functions whose whole bodies are a call each, so 0x514 sits *inside* dtk's
        # auto_00_00000178_text and this is a second sub-range carve of an existing auto unit. It
        # calls fn_54_55C by its dtk name and defines nothing, which is why its definitions are
        # inside #ifdef __MWERKS__ and the file is still safe to list in files.cmake.
        # Everything else in the module is left unclaimed, so dtk fills it from retail: fn_54_178
        # (0x178, 0x35C) is the module's own entity loader and opens a 0x790 frame constructing an
        # SLdrEditorProperties, so it is behavioural CActor/CPatterned class code this tree does
        # not model, and the 29 functions still unclaimed from there to 0x15C8 are the same - the
        # module has no CRipper class at all. The five from 0x1618 on are CRT glue, and fn_54_15C8
        # is a .ctors initialiser thunk. CRipperRelMain.cpp is not in
        # files.cmake, for the reason the other landed heads measure: it defines RELMain/RELExit,
        # which collide in a flat link, and it calls fn_54_178 and the setter, which the port
        # cannot link.
        "Ripper",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/RipperAccessors.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CRipperRelMain.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CRipperForwarders.cpp"),
        ],
    ),
    Rel(
        # 13 functions, .text 0x000000..0x0000C8: fn_68_0, the module's GetBoundingBox wrapper
        # (instruction for instruction CMysteryFlyerRel.cpp's fn_45_10), then the 12 short
        # accessors the REL loader generator emits, byte-identical to WallCrawler's 0x00..0x9C,
        # which MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp already reproduces as a Matching
        # unit. Everything else in the module is left unclaimed, so dtk fills it from retail.
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
    Rel(
        # Parasite's head, .text 0x0..0x148: the fourteen-accessor block, fn_47_90, RELExit,
        # RELMain and the loader registration RELMain calls. Module 47, same arrangement as
        # AtomicAlpha above and the same thirteen-virtual stand-in class - but the accessor
        # block is AtomicAlpha's *minus three* functions (its two leading +0x8C8 / +0x7D8
        # member addresses and the +0x34c bit-3 flag test) and *plus two* `li r3,0` predicates,
        # so four in the run after the +0x44f byte read where AtomicAlpha runs two. The one
        # thing here that a copy of CFishCloudRel.cpp would have got wrong is the loader
        # record: 0xC bytes of three FScriptLoader, not FishCloud's 0x8 of two, because this
        # module registers three entities - which is why the registration is 0x48 bytes and
        # not 0x30. The import is the plain DOL symbol fn_80200EFC, so no symbols.txt rename
        # and no DOL change. Everything from fn_47_148 (0x148, 0x618) up is left unclaimed, so
        # dtk fills it from retail and the module's sha1 still holds.
        "Parasite",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CParasiteRel.cpp"),
        ],
    ),
    Rel(
        # Added 2026-09-29 (goal item `progress-rel-head-darkcommando`). 18 functions, .text
        # 0x000000..0x00019C: the module head - the short accessors the REL loader generator
        # emits, `fn_3_14` (this module's `GetBoundingBox` wrapper), `fn_3_FC`'s vtable call on
        # slot 0x38, and RELExit, RELMain and the loader registration `fn_3_16C`. Module 3.
        # **Its accessor block is MysteryFlyer's in a different order, measured** by diffing
        # `build/G2ME01/DarkCommando/asm/auto_00_00000000_text.s` against
        # `CMysteryFlyerRel.cpp`'s rather than read off the `fn_<id>_<off>` names: it opens
        # `li r3,1` then a **module-local `.rodata`** float accessor (`fn_3_8`, `.rodata:0x0`,
        # `.float 50`) where MysteryFlyer's second accessor is the `addi r3,r3,0x818` member
        # address, it has **no** `lbl_8041B758` accessor at all, and it runs **three** `li r3,0`
        # predicates to MysteryFlyer's two. The first two differences are what shift the block
        # by 0x30 (`fn_3_8` is 0x0C against 0x08, and the inlined `fn_3_14` is 0x68 against the
        # out-of-line 0x3C) and the third is a further 8 bytes, so `kInvalidUniqueId` sits at
        # 0xAC here and 0x74 there. So the block is 0x0..0x128 and the head 0x0..0x19C, with
        # nothing missing. **`fn_3_A4` (0xA4) is `li r3,0`**, which the family's true/false
        # alternation reads as `li r3,1`; the disc image is the authority and an earlier attempt
        # that guessed `true` held the unit at 17/18.
        # **`fn_3_14` is not `MysteryFlyer`'s `fn_45_10`**: here retail *inlines* the
        # `optional_object<CAABox>` converting constructor's body - the same six word copies
        # and the same `stb r0, 0x18(r31)` flag, but the flag is stored **before** the copies
        # and there is no second `bl` - so it is three statements, not a call. The record is
        # four bytes at `.bss:0x1C` (`lbl_3_bss_1C`), **not `.bss:0x0` as in MysteryFlyer**,
        # because this module's `.bss` holds three objects (`lbl_3_bss_0` size 0xC at 0x0,
        # `lbl_3_bss_C` size 0x10 at 0xC, `lbl_3_bss_1C` size 0x4 at 0x1C in
        # `build/G2ME01/DarkCommando/asm/auto_05_00000000_bss.s`). The only reader of the slot
        # is `LoadDarkCommando` in the `Matching` unit
        # `src/MetroidPrime/ScriptLoader/DarkCommando.cpp`, so there is no second reader and
        # no pmf. The import is the plain DOL symbol `fn_80235E00` (0x80235E00, 8 bytes,
        # immediately after `LoadDarkCommando__FR13CStateManagerR12CInputStreamRC11CEntityInfo`
        # at 0x80235DD4, which is 44 bytes and so ends exactly there), so no `symbols.txt`
        # rename and no DOL change. **No dead-strip hazard**: the module's `ldscript.lcf` puts
        # all fifteen of `fn_3_0`..`fn_3_FC` in its `FORCEACTIVE` block, and `.data` stores
        # every one of them - `.data:0x364` is the class vtable (0x148 bytes, and it holds
        # `fn_3_FC` and `fn_3_14`) and the generator's one-entry vtable records hold the other
        # thirteen, `fn_3_FC` in a third of them - so nothing needs a `force_active:` entry.
        # `fn_3_19C` (0x19C, 0x33C) is the
        # module's own entity loader and the 168 functions from there up are its methods; all
        # stay retail - behavioural class code needing the CActor/CPatterned hierarchy. The
        # module's 193 text symbols therefore split 18 ours + 5 `REL_Setup` + 2
        # `global_destructor_chain` + 168 unclaimed. Not in `files.cmake`, for the reason the
        # other heads measure: it calls `fn_3_19C` and `fn_80235E00`, which the port cannot
        # link.
        "DarkCommando",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CDarkCommandoRel.cpp"),
        ],
    ),
    Rel(
        # Added 2026-09-29 (goal item `progress-rel-head-grenchler`). 18 functions, .text
        # 0x000000..0x000168: the module head - the short accessors the REL loader generator
        # emits, `fn_27_8` (this module's `GetBoundingBox` wrapper), `fn_27_C8`'s vtable call on
        # slot 0x38, and RELExit, RELMain and the loader registration `fn_27_138`. Module 27.
        # **Its accessor block is `CMediumIngRel.cpp`'s in the same order plus three predicates,
        # measured** by diffing `build/G2ME01/Grenchler/asm/auto_00_00000000_text.s` over
        # 0x0..0x168 against `CMediumIngRel.cpp`'s over 0x0..0x150 rather than read off the
        # `fn_<id>_<off>` names, which say nothing about which function is which: both open
        # `addi r3,r3,0x7c0` and then the `GetBoundingBox` wrapper, both run **three** `li r3,0`
        # predicates in a row, and both have **no** `skDamageHitTime__10CPatterned` float store at +0x448, but
        # MediumIng goes straight from `addi r3,r3,0x754` to its three-float copy where this
        # module runs `li r3,1`, `li r3,0`, `li r3,0` first. That 0x18 is the whole difference
        # between the two claims (0x168 against 0x150, 18 functions against 15), so no spelling
        # had to be discovered: every body is one `CMediumIngRel.cpp` or
        # `CMysteryFlyerRel.cpp` already reproduces at 100%. **`fn_27_8` is not an
        # `optional_object` template problem**: retail *calls* the converting constructor out of
        # line at 0x13C6C (six words copied out of `r4+0x00..r4+0x14`, then `stb 1, 0x18(r3)`),
        # and that function stays unclaimed, so it is one call by its dtk name - see the
        # source's header. The record is four bytes at `.bss:0x40` (`lbl_27_bss_40`), **not
        # `.bss:0x0`** as in MysteryFlyer, because this module's `.bss` holds seven objects
        # (`build/G2ME01/Grenchler/asm/auto_05_00000000_bss.s`), and `.bss:0x0` is an 8-byte
        # object used far above the head. The import is the plain DOL symbol `fn_80218A38`
        # (`stw r3, gLoader_Grenchler@sda21(r0); blr`, immediately after
        # `LoadGrenchler__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80218A0C, which
        # is 0x2C bytes and so ends exactly at 0x80218A38), so no `symbols.txt` rename and no
        # DOL change. **No dead-strip hazard**: the module's `ldscript.lcf` puts all fifteen of
        # `fn_27_0`..`fn_27_C8` in its `FORCEACTIVE` block, and `.data:0xA54` (CGrenchler's
        # vtable, which holds `fn_27_C8` and `fn_27_8`) plus `.data:0xED4` (CPhysicsActor's)
        # store every one of them, so nothing needs a `force_active:` entry. `fn_27_168`
        # (0x168, 0xE00) is the module's own entity loader and the functions from there up are
        # its methods; all stay retail - behavioural class code needing the CActor/CPatterned
        # hierarchy. Not in `files.cmake`, for the reason the other heads measure: it calls
        # `fn_27_168` and `fn_80218A38`, which the port cannot link.
        "Grenchler",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CGrenchlerRel.cpp"),
        ],
    ),
    Rel(
        # Added 2026-09-29 (goal item `progress-rel-head-shrieker`). 17 functions, .text
        # 0x000000..0x000154: the module head - the fourteen short accessors the REL loader
        # generator emits, `fn_69_10` (this module's `GetBoundingBox` wrapper), `fn_69_B4`'s
        # vtable call on slot 0x38, and RELExit, RELMain and the loader registration `fn_69_124`.
        # Module 69. **Its accessor block is `CMysteryFlyerRel.cpp`'s with three differences,
        # measured** by diffing `build/G2ME01/Shrieker/asm/auto_00_00000000_text.s` over
        # 0x0..0x154 against `CMysteryFlyerRel.cpp`'s over 0x0..0x170 rather than read off the
        # `fn_<id>_<off>` names, which say nothing about which function is which: the first two
        # are swapped in role (this module opens `addi r3,r3,0x8c4` where MysteryFlyer opens
        # `li r3,1`, so the member offset is 0x8C4 against 0x818), and this module has **no**
        # three-float copy, so its vtable entry sits at 0xB4 rather than 0xD0. Everything from
        # 0x4C to 0xB4 - the `lbl_8041AAB8` store at +0x448, the `lbl_8041B758` accessor, the
        # `+0x34c` bit 3, `+0x754`, `+0x44f` and the five predicates - is the same block. So no
        # spelling had to be discovered: every body is one `CMysteryFlyerRel.cpp` or
        # `CGrenchlerRel.cpp` already reproduces at 100%. **`fn_69_10` is not an
        # `optional_object` template problem**: retail *calls* the converting constructor out of
        # line at 0x6EE4 (six words copied out of `r4+0x00..r4+0x14`, then `stb 1, 0x18(r3)`),
        # and that function stays unclaimed, so it is one call by its dtk name - see the
        # source's header. The record is four bytes at `.bss:0x60` (`lbl_69_bss_60`), **not
        # `.bss:0x0`**, because this module's `.bss` holds nine objects
        # (`build/G2ME01/Shrieker/asm/auto_05_00000000_bss.s`) and `.bss:0x0` is a 16-byte
        # float block read and written far above the head. The import is the plain DOL symbol
        # `fn_80218C30` (`stw r3, gLoader_Shrieker@sda21(r0); blr`, immediately after
        # `LoadShrieker__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80218C04, which
        # is 0x2C bytes and so ends exactly at 0x80218C30), so no `symbols.txt` rename and no
        # DOL change. **No dead-strip hazard**: the module's `ldscript.lcf` puts all fourteen of
        # `fn_69_0`..`fn_69_B4` in its `FORCEACTIVE` block, and `.data:0x314` (CShrieker's
        # vtable, which holds every one of them) stores them, so nothing needs a `force_active:`
        # entry. `fn_69_154` (0x154, 0x7D8) is the module's own entity loader and the functions
        # from there up are its methods; all stay retail - behavioural class code needing the
        # CActor/CPatterned hierarchy. Not in `files.cmake`, for the reason the other heads
        # measure: it calls `fn_69_154` and `fn_80218C30`, which the port cannot link.
        "Shrieker",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CShriekerRel.cpp"),
        ],
    ),
    Rel(
        # Added 2026-09-30 (goal item `progress-rel-head-flyingpirate`). 16 functions, .text
        # 0x000470..0x0005D4: the module's thirteen accessors, `fn_22_534`'s vtable call on slot
        # 0x38, and RELExit, RELMain and the loader registration `fn_22_5A4`. Module 22, which
        # had **no `Rel(...)` block at all** before this, so its seven functions were the shared
        # `REL_Setup` and `global_destructor_chain` units and nothing of ours.
        #
        # **The claim starts at 0x470, not 0x0, and that is the one difference from every other
        # head in this family**: `fn_22_0` (0x164), `fn_22_164` (0x1E4), `fn_22_348` (0x94) and
        # `fn_22_3DC` (0x94) are 0x470 bytes of behavioural class code rather than accessors, so
        # they stay with dtk. Everything below 0x470 and above 0x5D4 is unclaimed and filled from
        # retail; `fn_22_5D4` (0x5D4, 0x90C) is the module's own entity loader and the 136
        # functions from there up are its methods - all retail, for the reason the other heads
        # give (they need the CActor/CPatterned hierarchy this tree does not model). The module's
        # 163 text symbols therefore split 16 ours + 5 `REL_Setup` + 2
        # `global_destructor_chain` + 140 unclaimed.
        #
        # Eleven of the thirteen accessors are the family, which bodies were placed where was read
        # off `build/G2ME01/FlyingPirate/asm/auto_00_00000000_text.s` and not off the
        # `fn_<id>_<off>` names, which say nothing about which function is which. The two that are
        # not: `fn_22_470` is the member-address accessor at a third offset (+0x970, where
        # `fn_45_8` uses +0x818 and `fn_27_0` +0x7c0), and **`fn_22_480` has no counterpart in
        # the family at all** - it returns one of this module's own `.rodata` floats
        # (`lbl_22_rodata_3CC`, `.float 5`, and `lbl_22_rodata_3C8`, `.float 50`) according to
        # **bit 5** of the byte at +0xbb8, where the family's own bit accessor reads bit 3 of a
        # different byte, and it is the only function here that returns a float by branch. The
        # mask is measured: dtk renders the word as `extrwi. r0, r0, 1, 26` (which reads as bit 2)
        # and objdump on the retail `.rel` reads the same word as `rlwinm. r0, r0, 27, 31, 31`, a
        # rotate left by 32-5 masked to one bit, i.e. bit 5. `& 4` compiles to
        # `rlwinm. r0, r0, 0, 29, 29` and breaks the module's sha1 on exactly two bytes.
        # `fn_22_4A4` is instruction for instruction `CGrenchlerRel.cpp`'s `fn_27_8`, so it is
        # written the way that file writes it, with the module's out-of-line
        # `optional_object<CAABox>` converting constructor (`fn_22_AA48`, 0xAA48) called by its dtk
        # name because it stays unclaimed. The record is four bytes at `.bss:0x58`
        # (`lbl_22_bss_58`), **not `.bss:0x0`**, because this module's `.bss:0x0` is a different
        # object used far above the head. The import is the plain DOL symbol `fn_80218A04`
        # (`stw r3, gLoader_FlyingPirate; blr`, 0x80218A04, immediately after
        # `LoadGrenchler__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80218A0C, which is
        # 0x2C bytes and so ends exactly at 0x80218A38), so no `symbols.txt` rename and no DOL
        # change. **No dead-strip hazard**, and this is measured off the module's own
        # `ldscript.lcf`: it lists all thirteen of `fn_22_470`..`fn_22_534` in its FORCEACTIVE
        # block, and RELMain/RELExit are the module's entry points, referenced by the
        # `_epilog`/`_prolog` the shared `REL/REL_Setup.cpp` unit defines, so all sixteen survive
        # and nothing needs a `force_active:` entry in `config/G2ME01/config.yml`. The two
        # unclaimed callees are held the way the rest of the family holds them: `fn_22_5D4` by
        # `fn_22_5A4` (which RELMain calls) and `fn_22_AA48` by `fn_22_4A4` (which is itself
        # force-active). Not in `files.cmake`, for the reason the other heads measure: it calls
        # `fn_22_5D4` and `fn_80218A04`, which the port cannot link.
        "FlyingPirate",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CFlyingPirateRel.cpp"),
        ],
    ),
    Rel(
        # Added 2026-09-30 (goal item `progress-rel-head-ing`). 17 functions, .text
        # 0x000000..0x000130: the module's thirteen accessors, `fn_29_90`'s vtable call on slot
        # 0x38, and RELExit, RELMain and the loader registration `fn_29_100`. Module 29, which
        # had **no `Rel(...)` block at all** before this, so its seven functions were the shared
        # `REL_Setup` and `global_destructor_chain` units and nothing of ours.
        #
        # **The claim starts at 0x0 and the arrangement is one no sibling in this family has**,
        # read off `build/G2ME01/Ing/asm/auto_00_00000000_text.s` and not off the
        # `fn_<id>_<off>` names, which say nothing about which function is which. It opens with
        # **two** member-address accessors (+0x9dc then +0xac8) where `CRezbitRel.cpp`,
        # `CMediumIngRel.cpp`, `CMetroidRel.cpp` and `CGrenchlerRel.cpp` each open with at most
        # one, so the `li r3,1` lands third at 0x10; its 0x0C slot at 0x18 holds a **module-local
        # `.rodata` constant** (`lbl_29_rodata_64`, `.float 1`, `.rodata:0x64`) rather than a DOL
        # one, the same distinction `CDarkCommandoRel.cpp` and `CChozoGhostRel.cpp` already
        # carry; and it has **neither** the `GetBoundingBox` wrapper **nor** the `skDamageHitTime__10CPatterned`
        # store at +0x448, so the three-float copy sits at 0x74 and the claim ends 0x38 below
        # Rezbit's. Every body below is one a sibling already reproduces at 100%, so no spelling
        # had to be discovered.
        #
        # The record is four bytes at `.bss:0x6C` (`lbl_29_bss_6C`, `size:0x4` per
        # `build/G2ME01/Ing/asm/auto_05_00000000_bss.s`), **not `.bss:0x0`**, which is a different
        # 8-byte object this module's own code uses far above the head. The only reader of the
        # slot is `LoadIngs` in the `Matching` unit `src/MetroidPrime/ScriptLoader/Ings.cpp`, so
        # unlike `CMetroidRel.cpp`'s MetroidAlpha there is no second reader, no `__ptmf_scall`
        # and no pointer-to-member-function in the record - which is also why no
        # `CAABox`/`CPhysicsActor` stand-in is needed at all. The import is the plain DOL symbol
        # `fn_80218918` (`stw r3, gLoader_Ings@sda21(r0); blr`, 0x80218918, immediately after
        # `LoadIngs__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x802188EC, which is
        # 0x2C bytes and so ends exactly at 0x80218918), so no `symbols.txt` rename and no DOL
        # change. **No dead-strip hazard**, and this is measured off the module's own
        # `ldscript.lcf`: it lists all fourteen of `fn_29_0`..`fn_29_90` in its FORCEACTIVE
        # block, and `.data:0xA24` - CIng's own vtable, 0x148 bytes = 82 words, two of them
        # leading - stores every one of them (`fn_29_90` at offset 0x3C, above
        # `HealthInfo__3CAiFv` at 0x38), while RELMain/RELExit are the module's entry points and
        # `fn_29_100` is called from RELMain, so all seventeen survive and nothing needs a
        # `force_active:` entry in `config/G2ME01/config.yml`. The one unclaimed callee,
        # `fn_29_130`, is held the way the rest of the family holds it: by `fn_29_100`. Not in
        # `files.cmake`, for the reason the other heads measure: it calls `fn_29_130` and
        # `fn_80218918`, which the port cannot link.
        "Ing",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CIngRel.cpp"),
        ],
    ),
    Rel(
        # Added 2026-09-30 (goal item `progress-rel-head-splinter`). 14 functions, .text
        # 0x000000..0x000118: the module's ten accessors, `fn_74_78`'s vtable call on slot 0x38,
        # and RELExit, RELMain and the loader registration `fn_74_E8`. Module 74, which had **no
        # `Rel(...)` block at all** before this, so its seven functions were the shared
        # `REL_Setup` and `global_destructor_chain` units and nothing of ours.
        #
        # **The claim starts at 0x0 and the arrangement is the shortest head in this family so
        # far - fourteen functions, where Ing's is seventeen and AtomicAlpha's eighteen** - read
        # off `build/G2ME01/Splinter/asm/auto_00_00000000_text.s` and not off the `fn_<id>_<off>`
        # names, which say nothing about which function is which. Three accessor kinds are
        # missing and each is absent for a stated reason: there is **no**
        # `optional_object<CAABox>` wrapper, so nothing in this unit names a `CAABox` and
        # `Kyoto/Math/CAABox.hpp` is not included; there is **no `lbl_8041B758` accessor** and no
        # module-local `.rodata` constant either, so the 0x0C slot at 0x40 is the `+0x34c` bit
        # read (Ing spends it on `lbl_29_rodata_64`, MinorIng on the DOL one) and the next
        # function is the `+0x754` member address at 0x4C; and it opens with **one**
        # member-address accessor (`+0x7c4` at 0x0) where Ing, IngBoostBallGuardian and
        # AtomicAlpha each open with two, so the `skDamageHitTime__10CPatterned` float store is the *second*
        # function at 0x8. Every body below is one `CIngRel.cpp` or `CMinorIngRel.cpp` already
        # reproduces at 100%, so no spelling had to be discovered for this run.
        #
        # **The record is four bytes, and `.bss:0x70` is the one place the family's usual shape
        # does not hold, so the DOL was checked before the shape was chosen.**
        # `config/G2ME01/rels/Splinter/symbols.txt:545` gives `lbl_74_bss_70` `size:0x8` where
        # Ing's `lbl_29_bss_6C` and MinorIng's `lbl_44_bss_84` are `size:0x4`, and the rule in
        # `RUNNING_THE_DECOMP.md` for exactly that case is to grep the DOL for every reader of the
        # `gLoader_*` symbol first. There is **one** reader -
        # `LoadSplinter__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x80218C38 - and it
        # reads **word 0 only** (`lwz r6, gLoader_Splinter; lwz r12, 0x0(r6); mtctr r12; bctrl`):
        # no second reader, no `__ptmf_scall`, no pointer-to-member-function, and the
        # registration stores one word rather than three copied out of `.data` the way
        # `CSnakeWeedSwarmRel.cpp`'s and `CSplitterRelMain.cpp`'s do. So the record is a plain
        # `FScriptLoader` and the 0x8 is retail's 8-byte slot for it. The import is the plain DOL
        # symbol `fn_80218C64` (`stw r3, gLoader_Splinter@sda21(r0); blr`, immediately after
        # `LoadSplinter` at 0x80218C38, which is 0x2C bytes and so ends exactly at 0x80218C64), so
        # no `symbols.txt` rename and no DOL change. `fn_74_118` (0x118, 0x724) is the module's own
        # entity loader and the 264 functions above it are its methods; all stay retail - they
        # need the CActor/CPatterned/CAi hierarchy this tree does not model. **No dead-strip
        # hazard**: the module's `ldscript.lcf` lists all eleven of `fn_74_0`..`fn_74_78` in
        # FORCEACTIVE and `.data:0x854` - its own 0x26C-byte CPatterned vtable - stores every one
        # of them (`fn_74_78` at 0x3C, above `HealthInfo__3CAiFv` at 0x38), so nothing needs a
        # `force_active:` entry in `config/G2ME01/config.yml`. Not in `files.cmake`, for the reason
        # the other heads measure: it calls `fn_74_118` and `fn_80218C64`, which the port cannot
        # link.
        "Splinter",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CSplinterRel.cpp"),
        ],
    ),
    Rel(
        # Added 2026-09-30 (goal item `progress-rel-head-spacepirate`). 14 functions, .text
        # 0x000000..0x000140: the module's eleven accessors, `fn_72_64`'s vtable call on slot 0x38,
        # and RELExit, RELMain and the loader registration `fn_72_D4`. Module 72, which had **no
        # `Rel(...)` block at all** before this, so its seven functions were the shared
        # `REL_Setup` and `global_destructor_chain` units and nothing of ours.
        #
        # **The claim starts at 0x0 and there is nothing of ours left unclaimed below the head**,
        # unlike FlyingPirate whose claim starts at 0x470: this module has no behavioural class
        # code above the head, so `fn_72_140` (0x140, 0xACC) is the first neighbour left retail and
        # it is the module's own entity loader. Everything from there up is its class methods -
        # all retail, for the reason the other heads give (they need the CActor/CPatterned
        # hierarchy this tree does not model). The module's 262 text symbols therefore split 14
        # ours + 5 `REL_Setup` + 2 `global_destructor_chain` + 241 unclaimed.
        #
        # Nine of the eleven accessors are the family and every body is one `CIngRel.cpp` or
        # `CFlyingPirateRel.cpp` already reproduces at 100%: the `+0x448` float store
        # (`fn_22_4E0`), the bit at +0x34c (`fn_29_4C`), the `+0x754` member address (`fn_29_64`),
        # the three predicates and the vtable dispatch (`fn_29_90`). Which body belongs to which
        # offset was read off `build/G2ME01/SpacePirate/asm/auto_00_00000000_text.s` and not off
        # the `fn_<id>_<off>` names, which say nothing about which function is which. Three
        # accessor kinds are missing and each is absent for a stated reason: no
        # `optional_object<CAABox>` wrapper, so nothing here names a `CAABox`; no three-float
        # copy; and no `*id = kInvalidUniqueId` reset - which is why this head is eleven
        # accessors where Ing's is fourteen and why the claim ends at 0x140 rather than 0x130.
        # The two that are not the family are the reason this module is worth a lane:
        #   - `fn_72_8` is the member-address accessor at a **fourth** offset (+0x920, where
        #     `fn_29_0` uses +0x9dc, `fn_22_470` +0x970, `fn_45_8` +0x818 and `fn_27_0` +0x7c0).
        #   - `fn_72_18` is a **`TUniqueId` getter with no counterpart in any other head in the
        #     tree**: `lhz r0, 0xa94(r4)` / `sth r0, 0x0(r3)` / `blr`, so `self` in r4 and a
        #     *store* through r3 - the value comes back through memory, and a two-byte struct
        #     returned in r3 would be one instruction. Hence an explicit out-pointer, which is
        #     how the family spells its other struct-returning accessors (`fn_22_4A4`,
        #     `fn_29_74`). That `+0xa94` holds a TUniqueId is measured three ways, not assumed:
        #     `fn_72_7970` loads it and stores it through a pointer to call
        #     `GetObjectById__13CStateManagerCF9TUniqueId`, `fn_72_AF70` hands it the same way,
        #     and the same function compares it word for word against `kInvalidUniqueId`. The
        #     cast has to be `reinterpret_cast`: MWCC rejects an explicit conversion from
        #     `const char*` to `const TUniqueId*`.
        #
        # `fn_72_D4` is instruction for instruction `CSnakeWeedSwarmRel.cpp`'s `fn_71_70` - same
        # register allocation (r9/r8/r7 then r5/r4/r0), same six `lwz` out of `.data`, same
        # `stwu` of the loader, same six stores, same single call - so the same spelling
        # reproduces it at 100% with no search at all. **The record is 0x1C bytes, not the four
        # most of this family uses**, and the `.bss` dump is the cheap check that says so:
        # `auto_05_00000000_bss.s` gives `lbl_72_bss_24` `size:0x1C` where the four-byte family's
        # slot is `size:0x4`, because two of its members are CodeWarrior
        # pointer-to-member-functions. `.data:0x8C0` is `0 / 0xFFFFFFFF / fn_72_E0D8` and
        # `.data:0x8CC` is `0 / 0xFFFFFFFF / fn_72_E0AC`, and both are copied word for word
        # rather than assigned. It is `lbl_72_bss_24` and **not** `.bss:0x0`, which is a
        # different 0xC-byte object read and written far above the head. **MWCC does not encode a
        # variable's type in its name**, so the two `.data` objects are externs written as
        # `extern bool (CEntity::*lbl_72_data_8C0)(const TUniqueId*);` and the member signatures
        # come from the two functions themselves.
        #
        # The import is the plain DOL symbol `fn_80200E3C` (`stw r3,-0x6a28(r13); blr`, 0x80200E3C,
        # immediately after `LoadSpacePirate` at 0x80200E10, which is 0x2C bytes and so ends
        # exactly at 0x80200E3C), so **no `symbols.txt` rename and no DOL change**; it is
        # declared `extern "C"` under its retail name because an alias would be a different symbol
        # and the call would resolve to nothing, and
        # `src/MetroidPrime/ScriptLoader/SpacePirate.cpp` (a `Matching` unit) already records why
        # the DOL deliberately does not claim it: REL modules import it by its retail name.
        # **No dead-strip hazard**: the module's own `ldscript.lcf` lists all eleven accessors
        # (`fn_72_0`..`fn_72_64`) in its FORCEACTIVE block, and RELMain/RELExit are the module's
        # entry points, referenced by the `_epilog`/`_prolog` the shared `REL/REL_Setup.cpp` unit
        # defines, and `fn_72_D4` is called from the RELMain in this same unit, so nothing needs a
        # `force_active:` entry in `config/G2ME01/config.yml`. The vtable slot is measured too:
        # `.data:0x8D8` (0x148 bytes, 82 words) is CSpacePirate's vtable, it starts
        # `[0][0][fn_72_F10C][TypesMatch__12CSpacePirateCFi]` and stores `fn_72_64` at offset
        # **0x3C** (word 15), so with the two leading zero words not counted as virtuals it is the
        # fourteenth virtual and the slot it dispatches to, 0x38 (word 13), is the twelfth. That
        # 0x38 slot holds this module's own `fn_72_3A0C` and 0x40 (word 14) holds
        # `HealthInfo__3CAiFv`, so 0x38 is the override and `HealthInfo` is the thirteenth - still
        # CActor's `HealthInfo` / `GetHealthInfo` neighbourhood, one extra CPatterned virtual below
        # it compared with module 71, where `CSnakeWeedSwarmRel.cpp` records the pair at 0x38 and
        # 0x3C. Only the *offset* is load-bearing: thirteen virtuals on the stand-in class put
        # `Slot12` at exactly 0x38, and a by-hand vtable load would compile to `lwz r3,0(r3)`
        # where retail has `lwz r12,0(r3)`, so it has to be a member call. Not in `files.cmake`,
        # for the reason the other heads measure: it calls `fn_72_140` and `fn_80200E3C`, which
        # the port cannot link.
        "SpacePirate",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CSpacePirateRel.cpp"),
        ],
    ),
    Rel(
        # Added 2026-09-30 (goal item `progress-rel-head-pirateragdoll`). 5 functions in two
        # units. Module 50, which had **no `Rel(...)` block at all** before this, so its seven
        # functions were the shared `REL_Setup` and `global_destructor_chain` units and nothing
        # of ours.
        #
        # **This module is not the accessors-and-predicate family the other heads are, and that
        # is what shaped the claim.** Its head is four *entry* functions and nothing else:
        # RELExit (0x0, 0x24), RELMain (0x24, 0x20), the loader registration `fn_50_44` (0x44,
        # 0x30) and the loader itself `fn_50_74` (0x74, 0x98). There is not a single accessor in
        # it, because `fn_50_10C` (0x10C, 0x2A8) is already the module's ragdoll constraint
        # solver: it indexes two parallel 0x44-byte node arrays off a `mulli`, calls
        # `CVector3f::AsNormalized` and `CQuaternion::YRotation` and writes quaternion and vector
        # temporaries to its own frame. That needs the CActor hierarchy this tree does not model,
        # so **the claim stops at the end of the head at 0x10C** and everything from 0x10C up is
        # left unclaimed and filled from retail by dtk, which is what keeps the module's sha1
        # against `config/G2ME01/config.yml` holding. Nothing below 0x0 exists.
        #
        # The second unit is **not contiguous with the head**, and that is why it is a second
        # `Object(...)` rather than a wider first one: `fn_50_D80` (0xD80, 0x40), the module's
        # three-float cross product, sits above 0x10C with `fn_50_10C`..`fn_50_D4C` in between.
        # A carve is contiguous per source, so the two are two units and two claims in
        # `config/G2ME01/rels/PirateRagDoll/splits.txt`. Its spelling is not the textbook one:
        # retail's `fmuls` writes two of the three products **b-component first**, and all three
        # commutative textbook forms give the same 0x40 bytes, objdiff 100%, `unit_fit.sh`
        # "fits", and still break the module's sha1 on four bytes. Multiplication is commutative,
        # so only the encoder's operand order separates them and no percentage in this repo can
        # see it. See the file's own header for the full listing.
        #
        # **No dead-strip hazard, and that is measured off the module's own `ldscript.lcf`**:
        # it lists `fn_50_74`, `fn_50_4F8`, `fn_50_DC0`, `fn_50_E10`, `fn_50_1610`, `fn_50_1874`,
        # `fn_50_1938`, `fn_50_2500`, `fn_50_298C`, `fn_50_2EE4`, `lbl_50_bss_188` and every
        # module constant in its FORCEACTIVE block; RELMain/RELExit are the entry points reached
        # from `_prolog`/`_epilog`, `fn_50_44` is a direct `bl` from RELMain, and `fn_50_D80` is
        # a direct `bl` from `fn_50_4F8` (at .text 0x6F8 and 0x72C). So all five survive the
        # `.plf` link's `-strip_partial` and nothing needs a `force_active:` entry in
        # `config/G2ME01/config.yml`. `tools/audit_rel_claim.py PirateRagDoll` prints the
        # preplf/plf symbol counts that measure it.
        #
        # `files.cmake` holds **one** of the two, and that asymmetry is forced by the checker,
        # not chosen: `tools/check_files_cmake.py` exempts a REL unit that defines `RELMain` or
        # `RELExit` (`MODULE_ENTRY`), which is why `CPirateRagDollRel.cpp` is absent from it
        # (it also calls `fn_50_1938` and `fn_80227538`, which the port cannot link), while
        # `CPirateRagDollCross.cpp` defines neither and so **must** be listed. That is safe
        # because `powerpc-eabi-nm -u` on its object prints nothing - one self-contained float
        # function, zero externals - so the port's undefined count does not move.
        "PirateRagDoll",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CPirateRagDollRel.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/CPirateRagDollCross.cpp"),
        ],
    ),
    Rel(
        # CommandoPirate's head, .text 0x0..0x168: the seventeen functions above the module's class
        # code. Module 9, and the same arrangement as IngSpaceJumpGuardian and MysteryFlyer above:
        # fourteen head functions (thirteen accessors and the `fn_9_C8` vtable entry), RELExit,
        # RELMain and the loader registration RELMain calls.
        #
        # **Its accessor block is `CIngSpaceJumpGuardianRel.cpp`'s order, read off the bytes
        # rather than assumed from the family**: it opens with *two* leading accessors -
        # `addi r3,r3,0x928` then `li r3,1` - and puts this module's own `.rodata` constant at
        # 0x10 (`lbl_9_rodata_400`, `.float 50`) where module 34 puts `lbl_34_rodata_0`. The
        # `lbl_8041B758` accessor module 34 carries at 0x90 is not here at all; 0x90 is the
        # +0x34c bit-3 test, which is the one function whose *identity* differs from module 34's
        # block. Every body below is one `CIngSpaceJumpGuardianRel.cpp` / `CMysteryFlyerRel.cpp`
        # already reproduces at 100%, so no spelling had to be discovered. See the source's header
        # for the full listing and the caveats on the two functions dtk renders misleadingly.
        #
        # `fn_9_1C` is the family's `optional_object<CAABox>` return and **not** a template problem:
        # retail calls the converting constructor out of line at `fn_9_F9BC`, which stays
        # unclaimed, so it is one call through a constructor declared by its dtk name. The
        # registration's loader slot is `lbl_9_bss_28` (`.bss:0x28`, **eight** bytes, where module
        # 34's is four) and the setter import is the plain DOL symbol `fn_802188B0` (0x802188B0,
        # `stw r3, gLoader_CommandPirate@sda21(r0)`), which `CommandPirate.cpp` records is
        # deliberately unclaimed because REL modules import it by its retail name - so **no
        # `symbols.txt` rename and no DOL change** are needed, unlike FishCloud/AtomicAlpha.
        #
        # **No dead-strip hazard, measured off the module's own `ldscript.lcf`**: its FORCEACTIVE
        # block lists all fourteen head functions from `fn_9_0` to `fn_9_C8`, `fn_9_138` is a
        # direct `bl` from RELMain, and
        # RELMain/RELExit are the entry points the wired `REL_Setup` unit reaches from
        # `_prolog`/`_epilog`. Nothing needs a `force_active:` entry in `config/G2ME01/config.yml`.
        #
        # Everything from fn_9_168 (0x168, 0x76C), the module's own entity loader, is left
        # unclaimed - behavioural class code that needs the CActor/CPatterned/CAi hierarchy this
        # tree does not model - and so are the 230 members above it (231 functions in that range,
        # `build/report.json`'s `auto_00_00000168_text`), so dtk fills both from retail and the
        # module's sha1 still holds. Not in `files.cmake`, for the reason every other head
        # measures: it calls `fn_9_168` and `fn_802188B0`, which the port cannot link.
        "CommandoPirate",
        [
            Object(Matching, "MetroidPrime/ScriptObjects/CCommandoPirateRel.cpp"),
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
