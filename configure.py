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
    "-i extern/musyx/include",
    "-DMUSY_TARGET=MUSY_TARGET_DOLPHIN",
    "-DMUSY_VERSION_MAJOR=2",
    "-DMUSY_VERSION_MINOR=0",
    "-DMUSY_VERSION_PATCH=3",
]

if config.version == "G2ME01":
    cflags_retro.append('-pragma "inline_max_size(125)"')

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
# RELs build with GC/1.3.2, not upstream's GC/2.7: 24 of our Matching modules (the accessor
# RELs, ScriptCoin, SwarmBasics, WallCrawler, Metaree, DarkSamus, ...) reproduce retail only
# under 1.3.2, while every Matching REL unit upstream has (SLdrTweakPlayer, SLdrTweakGuiColors)
# reproduces under both.
def Rel(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": "GC/1.3.2",
        "cflags": cflags_rel,
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
        "mw_version": "GC/2.7",
        "progress_category": "game",  # str | List[str]
        "host": True,
        "objects": [
            Object(NonMatching, "MetroidPrime/main.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CControlMapper.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CObjectList.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CAxisAngle.cpp"),
            Object(NonMatching, "MetroidPrime/CEulerAngles.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CMatrix3f_Ext.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CArchMsgParmUserInput.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CInputGenerator.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CMainFlow.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CCredits.cpp"),
            Object(NonMatching, "MetaRender/CCubeRenderer.cpp"),
            Object(Matching, "GuiSys/CGuiFrameFactory.cpp"),
            Object(NonMatching, "GuiSys/CGuiFrame.cpp"),
            Object(NonMatching, "GuiSys/CGuiObject.cpp"),
            Object(NonMatching, "GuiSys/CGuiWidget.cpp"),
            Object(NonMatching, "GuiSys/CGuiPane.cpp"),
            Object(NonMatching, "GuiSys/CGuiTextPane.cpp"),
            Object(NonMatching, "WorldFormat/COBBTree.cpp"),
            Object(NonMatching, "WorldFormat/CCollidableOBBTree.cpp"),
            Object(NonMatching, "WorldFormat/CCollidableOBBTreeGroup.cpp"),
            Object(NonMatching, "WorldFormat/CAreaOctTree.cpp"),
            Object(NonMatching, "WorldFormat/CMetroidAreaCollider.cpp"),
            Object(NonMatching, "WorldFormat/CAreaOctTree_Tests.cpp"),
            Object(NonMatching, "WorldFormat/CCollisionSurface.cpp"),
            Object(NonMatching, "WorldFormat/CMetroidModelInstance.cpp"),
            Object(MatchingFor("G2ME01"), "WorldFormat/CAreaBspTree.cpp"),
            Object(NonMatching, "WorldFormat/CPVSAreaSet.cpp"),
            Object(NonMatching, "WorldFormat/CAreaRenderOctTree.cpp"),
            Object(MatchingFor("G2ME01"), "WorldFormat/CWorldLight.cpp"),
            Object(NonMatching, "MetroidPrime/CStaticGeometryMap.cpp"),
            Object(NonMatching, "Collision/CollisionUtil.cpp"),
            Object(NonMatching, "Collision/COBBox.cpp"),
            Object(MatchingFor("G2ME01"), "Collision/CMRay.cpp"),
            Object(NonMatching, "MetroidPrime/CStateManager.cpp"),
            Object(NonMatching, "MetroidPrime/CVisorFlare.cpp"),
            Object(NonMatching, "MetroidPrime/CWorldTransManager.cpp"),
            Object(NonMatching, "MetroidPrime/CRagDoll.cpp"),
            Object(NonMatching, "MetroidPrime/CSortedLists.cpp"),
            Object(NonMatching, "MetroidPrime/CProjectedShadow.cpp"),
            Object(NonMatching, "MetroidPrime/CSlideShow.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptProjectedShadow.cpp"),
            Object(NonMatching, "MetroidPrime/CSteeringBehaviors.cpp"),
            Object(NonMatching, "MetroidPrime/CEntity.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CArchMsgParmInt32.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CArchMsgParmInt32Int32VoidPtr.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CArchMsgParmNull.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CArchMsgParmReal32.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/Decode.cpp"),
            Object(NonMatching, "MetroidPrime/CIOWinManager.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CIOWin.cpp"),
            Object(NonMatching, "MetroidPrime/CWorld.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CArchMsgParmControllerStatus.cpp"),
            Object(NonMatching, "MetroidPrime/CGameArea.cpp"),
            Object(NonMatching, "MetroidPrime/CWorldLayerState.cpp"),
            Object(NonMatching, "MetroidPrime/CMemoryCard.cpp"),
            Object(NonMatching, "MetroidPrime/CMemoryCardDriver.cpp"),
            Object(NonMatching, "MetroidPrime/CSaveGameScreen.cpp"),
            Object(NonMatching, "MetroidPrime/CGameHintInfo.cpp"),
            Object(NonMatching, "MetroidPrime/CErrorOutputWindow.cpp"),
            Object(NonMatching, "MetroidPrime/CRainSplashGenerator.cpp"),
            Object(NonMatching, "MetroidPrime/Cameras/CGameCamera.cpp"),
            Object(NonMatching, "MetroidPrime/Cameras/CCameraShakerData.cpp"),
            Object(NonMatching, "MetroidPrime/Cameras/CCameraFilter.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptCameraShaker.cpp"),
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
            Object(NonMatching, "MetroidPrime/CAutoMapper.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CPlayerGun.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CPlayerGunBase.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CGrappleArm.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CFidget.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptPickup.cpp"),
            Object(Matching, "MetroidPrime/HUD/CHUDMemoParms.cpp"),
            Object(NonMatching, "MetroidPrime/HUD/CSamusHud.cpp"),
            Object(NonMatching, "MetroidPrime/HUD/CHudDecoInterfaceScan.cpp"),
            Object(NonMatching, "MetroidPrime/CQuitGameScreen.cpp"),
            Object(NonMatching, "MetroidPrime/CPauseScreen.cpp"),
            Object(NonMatching, "MetroidPrime/CInGameGuiManager.cpp"),
            Object(NonMatching, "MetroidPrime/CSimpleShadow.cpp"),
            Object(NonMatching, "MetroidPrime/CWorldShadow.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptRepulsor.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptSound.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptPlatform.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptActor.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptDoor.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptDock.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptEffect.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptTrigger.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptWater.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptGenerator.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptColorModulate.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptSpecialFunction.cpp"),
            Object(NonMatching, "MetroidPrime/CMapWorldInfo.cpp"),
            Object(NonMatching, "MetroidPrime/CMapWorld.cpp"),
            Object(NonMatching, "MetroidPrime/CMemoryDrawEnum.cpp"),
            Object(NonMatching, "MetroidPrime/CMapUniverse.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptTeamAiMgr.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptCameraWaypoint.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptCameraHint.cpp"),
            Object(NonMatching, "MetroidPrime/CGameHint.cpp"),
            Object(NonMatching, "MetroidPrime/CGameLight.cpp"),
            Object(NonMatching, "MetroidPrime/CParticleGenInfoGeneric.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CParticleGenInfo.cpp"),
            Object(NonMatching, "MetroidPrime/CParticleDatabase.cpp"),
            Object(NonMatching, "MetroidPrime/CAnimData.cpp"),
            Object(NonMatching, "MetroidPrime/Factories/CCharacterFactory.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/Factories/CAssetFactory.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CAnimationDatabaseGame.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CTransitionDatabaseGame.cpp"),
            Object(NonMatching, "MetroidPrime/CTargetReticles.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptHUDMemo.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptAreaProperties.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CStaticInterference.cpp"),
            Object(NonMatching, "MetroidPrime/PathFinding/CPathFindSearch.cpp"),
            Object(NonMatching, "MetroidPrime/PathFinding/CPathFindRegion.cpp"),
            Object(NonMatching, "MetroidPrime/PathFinding/CPathFindArea.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/PathFinding/CPathFindSpline.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CHealthInfo.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CGameState.cpp"),
            Object(NonMatching, "MetroidPrime/Tweaks/CTweakAutoMapper.cpp"),
            Object(NonMatching, "MetroidPrime/Tweaks/CTweakBall.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CGMDeathMatch.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CGMCoin.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CGMMultiplayer.cpp"),
            Object(NonMatching, "MetroidPrime/CBoneTracking.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CGameOptions.cpp"),
            Object(NonMatching, "MetroidPrime/CEnvFxManager.cpp"),
            Object(MatchingFor("G2ME01"), "MetroidPrime/CRumbleManager.cpp"),
            Object(NonMatching, "MetroidPrime/CFluidUVMotion.cpp"),
            Object(NonMatching, "MetroidPrime/CFluidPlane.cpp"),
            Object(NonMatching, "MetroidPrime/CFluidPlaneCPU.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptSequenceTimer.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptSpindleCamera.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptPathCamera.cpp"),
            Object(NonMatching, "MetroidPrime/Cameras/CPathCamera.cpp"),
            Object(NonMatching, "MetroidPrime/Cameras/CInterpolationCamera.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScriptStreamedMusic.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoaderRel.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/CPowerBeam.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/CGunWeapon.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/CWeapon.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/CGameProjectile.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/CEnergyProjectile.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/CBeamProjectile.cpp"),
            Object(NonMatching, "MetroidPrime/Weapons/CPlasmaProjectile.cpp"),
            Object(NonMatching, "Weapons/CProjectileWeapon.cpp"),
            Object(NonMatching, "Weapons/CCollisionResponseData.cpp"),
            Object(NonMatching, "Weapons/CDecal.cpp"),
            Object(NonMatching, "MetroidPrime/CDecalManager.cpp"),
            Object(NonMatching, "MetroidPrime/CPhysicsActor.cpp"),
            Object(NonMatching, "MetroidPrime/CModelData.cpp"),
            Object(NonMatching, "MetroidPrime/CActorLights.cpp"),
            Object(NonMatching, "MetroidPrime/CGroundMovement.cpp"),
            Object(NonMatching, "MetroidPrime/CGameCollision.cpp"),
            Object(Matching, "MetroidPrime/Enemies/CAi.cpp"),
            Object(NonMatching, "MetroidPrime/Enemies/CStateMachine.cpp"),
            Object(NonMatching, "MetroidPrime/Factories/CStateMachineFactory.cpp"),
            Object(NonMatching, "MetroidPrime/Enemies/CKnockBackMgr.cpp"),
            Object(NonMatching, "MetroidPrime/Enemies/CPatterned.cpp"),
            Object(NonMatching, "MetroidPrime/Enemies/CPatternedAiFunctions.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CBodyController.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CBodyStateCmdMgr.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CBodyStateInfo.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CBSLocomotion.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CBSHurled.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CBSJump.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CBSTurn.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CBSWallHang.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CABSIdle.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CABSFlinch.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CABSAim.cpp"),
            Object(NonMatching, "MetroidPrime/BodyState/CABSReaction.cpp"),
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
            Object(NonMatching, "MetroidPrime/Player/CScanDisplay.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CMorphBall.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CMorphBallShadow.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptObjects/CScanTreeInventory.cpp"),
            # Units carved and matched before the upstream merge, in ranges upstream leaves unsplit.
            Object(Matching, "Runtime/MetroTRKConsoleStubs.cpp"),
            Object(Matching, "MetroidPrime/Carve80003858.c"),
            Object(NonMatching, "MetroidPrime/CMainResetGameState.cpp"),
            Object(Matching, "MetroidPrime/Player/CGameStateBlockDtor.cpp"),
            Object(NonMatching, "MetroidPrime/Player/CGameStateBlockCopyCtor.cpp"),
            Object(Matching, "MetroidPrime/Player/CGameStateBlockConstruct.cpp"),
            Object(Matching, "MetroidPrime/Carve80045CD4.c"),
            Object(Matching, "MetroidPrime/Carve800534B0.c"),
            Object(Matching, "MetroidPrime/Carve80053594.c"),
            Object(Matching, "MetroidPrime/Carve80073594.c"),
            Object(Matching, "MetroidPrime/Enemies/Carve800836B0.c"),
            Object(Matching, "MetroidPrime/Enemies/Carve80083FD8.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/CScriptRelay.cpp"),
            Object(Matching, "MetroidPrime/CConsoleOutputWindowCtor.cpp"),
            Object(Matching, "MetroidPrime/CDamageVulnerabilityStatics.cpp"),
            Object(Matching, "MetroidPrime/CAudioStateWinCtor.cpp"),
            Object(Matching, "MetroidPrime/Carve800E39D0.c"),
            Object(Matching, "MetroidPrime/Carve800E8E4C.c"),
            Object(Matching, "MetroidPrime/Carve800EC508.c"),
            Object(Matching, "MetroidPrime/Carve800ED550.c"),
            Object(Matching, "MetroidPrime/Carve800ED604.c"),
            Object(Matching, "MetroidPrime/Carve800F15C8.c"),
            Object(Matching, "MetroidPrime/Carve800F1A18.c"),
            Object(Matching, "MetroidPrime/Carve800F2390.c"),
            Object(Matching, "MetroidPrime/Carve800F24AC.c"),
            Object(Matching, "MetroidPrime/Carve800F5004.c"),
            Object(Matching, "MetroidPrime/Carve800F51EC.c"),
            Object(Matching, "MetroidPrime/Carve800F794C.c"),
            Object(Matching, "MetroidPrime/Carve800F7B80.c"),
            Object(Matching, "MetroidPrime/Carve800F8BDC.c"),
            Object(Matching, "MetroidPrime/Carve800FAC18.c"),
            Object(Matching, "MetroidPrime/Carve800FACE0.c"),
            Object(Matching, "MetroidPrime/Carve800FD2D0.c"),
            Object(Matching, "MetroidPrime/Carve800FD648.c"),
            Object(Matching, "MetroidPrime/Carve800FEE98.c"),
            Object(Matching, "MetroidPrime/Carve800FEF78.c"),
            Object(Matching, "MetroidPrime/Carve800FF164.c"),
            Object(Matching, "MetroidPrime/Carve800FF294.c"),
            Object(Matching, "MetroidPrime/Carve800FFC2C.c"),
            Object(Matching, "MetroidPrime/Carve800FFD34.c"),
            Object(Matching, "MetroidPrime/Carve801007D8.c"),
            Object(Matching, "MetroidPrime/Carve8010EE54.c"),
            Object(Matching, "MetroidPrime/Carve801174E8.c"),
            Object(Matching, "MetroidPrime/Carve801184E8.c"),
            Object(Matching, "MetroidPrime/Carve801185AC.c"),
            Object(Matching, "MetroidPrime/Carve8012CB4C.c"),
            Object(Matching, "MetroidPrime/Player/Carve801476D0.c"),
            Object(Matching, "MetroidPrime/Player/Carve80149108.c"),
            Object(Matching, "MetroidPrime/Player/Carve80149288.c"),
            Object(Matching, "MetroidPrime/Player/Carve8014FFCC.c"),
            Object(Matching, "MetroidPrime/Player/Carve8016BDE4.c"),
            Object(Matching, "MetroidPrime/CInGameTweakManagerCtor.cpp"),
            Object(Matching, "MetroidPrime/Carve80179E08.c"),
            Object(Matching, "MetroidPrime/Carve80193C30.c"),
            Object(Matching, "MetroidPrime/Carve80193C54.c"),
            Object(Matching, "MetroidPrime/Carve80193C7C.c"),
            Object(Matching, "MetroidPrime/Carve80193D9C.c"),
            Object(Matching, "MetroidPrime/Carve80193DB8.c"),
            Object(Matching, "MetroidPrime/Carve80193E04.c"),
            Object(Matching, "MetroidPrime/Carve801997B0.c"),
            Object(Matching, "MetroidPrime/Carve8019CE6C.c"),
            Object(Matching, "MetroidPrime/Carve8019E368.c"),
            Object(Matching, "MetroidPrime/CStateManagerScriptMsgArray.cpp"),
            Object(Matching, "MetroidPrime/Carve801B9420.c"),
            Object(Matching, "MetroidPrime/Carve801B94B4.c"),
            Object(Matching, "MetroidPrime/Carve801BC900.c"),
            Object(Matching, "MetroidPrime/Carve801C128C.c"),
            Object(Matching, "MetroidPrime/Carve801C13F4.c"),
            Object(Matching, "MetroidPrime/Player/Carve801D3B88.c"),
            Object(Matching, "MetroidPrime/Weapons/Carve801D5E9C.c"),
            Object(Matching, "MetroidPrime/Weapons/Carve801D688C.c"),
            Object(Matching, "MetroidPrime/Weapons/Carve801D6920.c"),
            Object(Matching, "MetroidPrime/Weapons/Carve801D6930.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801E2AE8.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801E3E34.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801E8AEC.c"),
            Object(Matching, "MetroidPrime/CActorField25.cpp"),
            Object(Matching, "MetroidPrime/CGameGlobalObjectsTailCtor.cpp"),
            Object(Matching, "MetroidPrime/Carve801F3690.c"),
            Object(Matching, "MetroidPrime/Carve801F36DC.c"),
            Object(Matching, "MetroidPrime/Carve801F7AC8.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/CUnknown90.cpp"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FEEF0.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve801FF4A4.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/SpacePirate.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Kralee.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Parasite.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/PillBug.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80201418.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve80212278.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve802126B0.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve80212944.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve802129A4.c"),
            Object(Matching, "MetroidPrime/ScriptObjects/Carve80212A24.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/SporbBase.cpp"),
            Object(Matching, "MetroidPrime/Tweaks/CTweakPlayerSuit.cpp"),
            Object(Matching, "MetroidPrime/Tweaks/CTweakPlayerAnalog.cpp"),
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
            Object(Matching, "MetroidPrime/ScriptLoader/CoinLoaderSet.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/CannonBall.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/CannonBallLoaderSet.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80226B3C.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80226B60.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80226C78.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80226C9C.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80226CB8.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80226CC8.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Krocus.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/AIMannedTurret.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/EmperorIngStage1.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/OctopedeSegment.cpp"),
            Object(Matching, "MetroidPrime/Player/CGameOptionsDefaults.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Rezbit.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/RsfAudio.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/RsfAudioLoaderSet.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80229410.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80229568.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve80229BBC.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/IngPuddle.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/FlyerSwarm.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/FlyerSwarmLoaderSet.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/StreamedMovie.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/IngSpiderBallGuardian.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/PuddleSpore.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve8022A3F4.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/EmperorIngStage2Tentacle.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/BacteriaSwarm.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/MetareeSwarm.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve8022D758.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/Carve8022DA40.c"),
            Object(Matching, "MetroidPrime/ScriptLoader/IngBlobSwarm.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/EmperorIngStage3.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/DestructableBarrier.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/SwampBossStage2.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/SwampBossStage1.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/IngBoostBallGuardian.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/PlantScarabSwarm.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/SkyRipple.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/SkyRippleLoaderSet.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/FogOverlay.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/MysteryFlyer.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/AtomicBeta.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/EyeBall.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/DarkSamusBattleStage.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/DarkCommando.cpp"),
            Object(Matching, "MetroidPrime/ScriptLoader/RubiksPuzzle.cpp"),
            Object(Matching, "MetroidPrime/Carve8027409C.c"),
            Object(Matching, "MetroidPrime/Carve80274774.cpp"),
            Object(Matching, "MetroidPrime/Carve80274C1C.c"),
            Object(Matching, "MetroidPrime/Carve80276AD8.c"),
            Object(Matching, "MetroidPrime/Carve80277090.c"),
            Object(Matching, "MetroidPrime/Carve80278C74.c"),
            Object(Matching, "MetroidPrime/Carve80279250.c"),
            Object(Matching, "MetroidPrime/Carve8027E404.c"),
            Object(Matching, "MetroidPrime/Carve80280338.c"),
            Object(Matching, "Kyoto/Animation/Carve802B2088.c"),
            Object(Matching, "Kyoto/Animation/Carve802B2568.c"),
            Object(Matching, "Kyoto/Carve802BAD08.c"),
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
            Object(Matching, "Kyoto/Math/Carve8033F2CC.c"),
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
            Object(NonMatching, "Kyoto/Graphics/CLight.cpp"),  # Float literal order
            Object(NonMatching, "Kyoto/Graphics/CCubeModel.cpp"),
            Object(NonMatching, "Kyoto/Graphics/CGX.cpp"),
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
            Object(NonMatching, "Kyoto/CARAMManager.cpp"),
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
            Object(NonMatching, "Kyoto/Animation/CAnimTreeAnimReaderContainer.cpp"),
            Object(NonMatching, "Kyoto/Animation/CAnimTreeDoubleChild.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CAnimTreeLoopIn.cpp"),
            Object(NonMatching, "Kyoto/Animation/CAnimTreeNode.cpp"),
            Object(NonMatching, "Kyoto/Animation/CAnimTreeSequence.cpp"),
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
            Object(NonMatching, "Kyoto/Animation/CMetaAnimRandom.cpp"),
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
            Object(NonMatching, "Kyoto/Animation/CSequenceHelper.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CTransition.cpp"),
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
            Object(NonMatching, "Kyoto/Animation/CPASDatabase.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CPASParmInfo.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CPOINode.cpp"),
            Object(NonMatching, "Kyoto/Animation/CSoundPOINode.cpp"),
            Object(NonMatching, "Kyoto/Animation/CPoseAsTransforms_Linear.cpp"),
            Object(NonMatching, "Kyoto/Particles/CColorElement.cpp"),
            Object(NonMatching, "Kyoto/Particles/CDeferredParticleEffect.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CSegId.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Input/CFinalInput.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Graphics/CColor.cpp"),
            Object(NonMatching, "Kyoto/Graphics/DolphinCColor.cpp"),
            Object(NonMatching, "Kyoto/CDependencyGroup.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Input/CRumbleVoice.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Input/RumbleAdsr.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Input/CRumbleGenerator.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CCharAnimTime.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CTimeRemainderAndFraction.cpp"),
            Object(NonMatching, "Kyoto/DolphinCDvdFile.cpp"),
            Object(NonMatching, "Kyoto/Graphics/CCubeMoviePlayer.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Animation/CAdditiveAnimPlayback.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Particles/CParticleElectricDataFactory.cpp"),
            Object(NonMatching, "Kyoto/Particles/CParticleElectric.cpp"),
            Object(NonMatching, "Kyoto/Particles/CElementGen.cpp"),
            Object(NonMatching, "Kyoto/Particles/CParticleSpawnSystem.cpp"),
            Object(NonMatching, "Kyoto/Particles/CParticleSpawnRandom.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Particles/CParticleSwooshDataFactory.cpp"),
            Object(NonMatching, "Kyoto/Particles/CRealElement.cpp"),
            Object(NonMatching, "Kyoto/Particles/CSpawnSystemKeyframeData.cpp"),
            Object(NonMatching, "Kyoto/Particles/CUVElement.cpp"),
            Object(NonMatching, "Kyoto/Particles/CVectorElement.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Audio/g721.cpp"),
            Object(NonMatching, "Kyoto/Audio/CStaticAudioPlayer.cpp"),
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
            Object(NonMatching, "Kyoto/Particles/CIntElement.cpp"),
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
            Object(NonMatching, "Kyoto/Audio/CSfxHandle.cpp"),
            Object(NonMatching, "Kyoto/Audio/CSfxManager.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CFontImageDef.cpp"),
            Object(NonMatching, "Kyoto/Text/CTextRenderBuffer.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CDrawStringOptions.cpp"),
            Object(NonMatching, "Kyoto/Text/CFontRenderState.cpp"),
            Object(MatchingFor("G2ME01"), "Kyoto/Text/CBlockInstruction.cpp"),
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
            Object(NonMatching, "musyx/runtime/stream.c"),
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
            Object(NonMatching, "musyx/runtime/snd3d.c"),
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
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakAutoMapper.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakBall.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakPlayerControls.cpp"),
            Object(Matching,    "MetroidPrime/ScriptLoader/SLdrTweakPlayer.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakCameraBob.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakPlayerGun.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakSlideShow.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakGame.cpp"),
            Object(Matching,    "MetroidPrime/ScriptLoader/SLdrTweakGuiColors.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakParticle.cpp"),
            Object(NonMatching, "MetroidPrime/ScriptLoader/SLdrTweakPlayerRes.cpp"),
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
