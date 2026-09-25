# src/Kyoto/CARAMManager.cpp is not listed in upstream configure.py, and
# include/Kyoto/CARAMManager.hpp is still a stub missing members the .cpp defines.
# Re-enable it when the decompilation catches up.
set(MP_GAME_SOURCES
    src/Kyoto/Alloc/CCircularBuffer.cpp
    src/Kyoto/Alloc/CGameAllocator.cpp
    src/Kyoto/Alloc/CMediumAllocPool.cpp
    src/Kyoto/Alloc/CMemory.cpp
    src/Kyoto/Alloc/CSmallAllocPool.cpp
    src/Kyoto/Alloc/IAllocator.cpp
    src/Kyoto/Animation/CCharAnimTime.cpp
    src/Kyoto/Animation/CTimeRemainderAndFraction.cpp
    src/Kyoto/Audio/CStaticAudioPlayer.cpp
    src/Kyoto/Audio/g721.cpp
    src/Kyoto/Basics/COsContext.cpp
    src/Kyoto/Basics/CStopwatch.cpp
    src/Kyoto/Basics/RAssertDolphin.cpp
    src/Kyoto/CARAMManagerWait.cpp
    src/Kyoto/CARAMToken.cpp
    src/Kyoto/CCrc32.cpp
    src/Kyoto/CDvdRequest.cpp
    src/Kyoto/CFrameDelayedKiller.cpp
    src/Kyoto/CRandom16.cpp
    src/Kyoto/DolphinCDvdFile.cpp
    src/Kyoto/Graphics/CColor.cpp
    src/Kyoto/Graphics/CCubeMoviePlayer.cpp
    src/Kyoto/Graphics/CGX.cpp
    src/Kyoto/Graphics/DolphinCColor.cpp
    src/Kyoto/Input/CDolphinController.cpp
    src/Kyoto/Input/CFinalInput.cpp
    src/Kyoto/Input/CRumbleGenerator.cpp
    src/Kyoto/Input/CRumbleVoice.cpp
    src/Kyoto/Input/DolphinIController.cpp
    src/Kyoto/Input/RumbleAdsr.cpp
    src/Kyoto/Math/CAABox.cpp
    src/Kyoto/Math/CFrustumPlanes.cpp
    src/Kyoto/Math/CMatrix3f.cpp
    src/Kyoto/Math/CMatrix4f.cpp
    src/Kyoto/Math/CMayaSpline.cpp
    src/Kyoto/Math/CNUQuaternion.cpp
    src/Kyoto/Math/CPlane.cpp
    src/Kyoto/Math/CQuad.cpp
    src/Kyoto/Math/CQuaternion.cpp
    src/Kyoto/Math/CMathSqrtF.cpp
    src/Kyoto/Math/CSphere.cpp
    src/Kyoto/Math/CTransform4f.cpp
    src/Kyoto/Math/CTri.cpp
    src/Kyoto/Math/CUnitVector3f.cpp
    src/Kyoto/Math/CVector2f.cpp
    src/Kyoto/Math/CVector2i.cpp
    src/Kyoto/Math/CVector3d.cpp
    src/Kyoto/Math/CVector3f.cpp
    src/Kyoto/Math/CVector3i.cpp
    src/Kyoto/Math/CloseEnough.cpp
    src/Kyoto/Math/RMathUtils.cpp
    src/Kyoto/PVS/CPVSVisOctree.cpp
    src/Kyoto/PVS/CPVSVisSet.cpp
    src/Kyoto/Streams/CBitStreamReader.cpp
    src/Kyoto/Streams/CBitStreamWriter.cpp
    src/Kyoto/Streams/CFilePreload.cpp
    src/Kyoto/Streams/CInputStream.cpp
    src/Kyoto/Streams/CLZOSupport.cpp
    src/Kyoto/Streams/CMemoryInStream.cpp
    src/Kyoto/Streams/CMemoryStreamOut.cpp
    src/Kyoto/Streams/COutputStream.cpp
    src/Kyoto/Streams/DolphinCLZOInputStream.cpp
    src/LZO/lzo1x_d1.c
    src/LZO/lzo_init.c
    src/LZO/lzo_ptr.c
    src/MetroidPrime/CActor.cpp
    src/MetroidPrime/CActorField25.cpp
    src/MetroidPrime/CDamageInfo.cpp
    src/MetroidPrime/CEntity.cpp
    src/MetroidPrime/CHealthInfo.cpp
    src/MetroidPrime/CPhysicsActor.cpp
    src/MetroidPrime/CMiscTableInit.cpp
    src/MetroidPrime/CRuleSet.cpp
    src/MetroidPrime/CStateManager.cpp
    src/MetroidPrime/HUD/CHUDMemoParms.cpp
    src/MetroidPrime/Player/CGameOptions.cpp
    src/MetroidPrime/Player/CGameOptionsDefaults.cpp
    src/MetroidPrime/Player/CPlayer.cpp
    src/MetroidPrime/Player/CMorphBallC80.cpp
    src/MetroidPrime/Player/CPlayerGun.cpp
    src/MetroidPrime/Player/CPlayerState.cpp
    # Not a configure.py unit, on purpose: it holds the PC-side definitions of
    # the retail globals the decompilation can only declare, and a new small-data
    # symbol in any unit shifts that unit's SDA offsets. See the file's header.
    src/MetroidPrime/PortGlobals.cpp
    # CTweakPlayer's five accessors, as two units. configure.py claims these two,
    # so unlike PortGlobals.cpp they are Matching and count as linked.
    src/MetroidPrime/Tweaks/CTweakPlayerAnalog.cpp
    src/MetroidPrime/Tweaks/CTweakPlayerSuit.cpp
    # Also not a configure.py unit, for a different reason: the 136 SLdr* struct
    # constructors/destructors retail spells __ct__/__dt__ and the host spells
    # C1Ev/D1Ev, so no retail range can be claimed for them at all. See the
    # file's header and docs/research/sldr_ctors.md.
    src/MetroidPrime/ScriptLoader/SLdrStructMembers.cpp
    # Not a configure.py unit, on the same grounds: it holds the host-only bodies of
    # CMain::OpenWindow and CMain::RsMain, whose retail bodies cannot be written yet
    # and must not perturb MetroidPrime/main.cpp. See the file's header.
    src/MetroidPrime/PortBoot.cpp
    # The module-publish thunks: one store per module, retail's 8-byte Set* family.
    # Port-side only (absent from configure.py), so it closes three link symbols and
    # makes a module's function-pointer table actually reachable.
    src/MetroidPrime/ModulePublish.cpp
    src/MetroidPrime/ScriptLoader/SLdrTweakAutoMapper.cpp
    src/MetroidPrime/ScriptLoader/SLdrTweakBall.cpp
    src/MetroidPrime/ScriptLoader/SLdrTweakCameraBob.cpp
    src/MetroidPrime/ScriptLoader/SLdrTweakGame.cpp
    src/MetroidPrime/ScriptLoader/SLdrTweakGui.cpp
    src/MetroidPrime/ScriptLoader/SLdrTweakGuiColors.cpp
    src/MetroidPrime/ScriptLoader/SLdrTweakParticle.cpp
    src/MetroidPrime/ScriptLoader/SLdrTweakPlayer.cpp
    src/MetroidPrime/ScriptLoader/SLdrTweakPlayerControls.cpp
    src/MetroidPrime/ScriptLoader/SLdrTweakPlayerGun.cpp
    src/MetroidPrime/ScriptLoader/SLdrTweakPlayerRes.cpp
    src/MetroidPrime/ScriptLoader/SLdrTweakSlideShow.cpp
    src/MetroidPrime/ScriptLoader/SLdrTweakTargeting.cpp
    src/MetroidPrime/ScriptLoader/Structs/SLdrTweakCameraBob_Load.cpp
    src/MetroidPrime/ScriptLoader/Structs/SLdrTweakSlideShow_Load.cpp
    src/MetroidPrime/ScriptLoader/Structs/SLdrTweakTargeting_Load.cpp
    src/MetroidPrime/ScriptLoader/Structs/SLdrTweakPlayerGun_Weapons.cpp
    src/MetroidPrime/ScriptLoader/Structs/SLdrTweakTargeting_Scan.cpp
    src/MetroidPrime/ScriptLoader/Structs/SLdrTweakTargeting_VulnerabilityIndicator.cpp
    src/MetroidPrime/ScriptLoader.cpp
    src/MetroidPrime/ScriptLoaderRel.cpp
    src/MetroidPrime/ScriptObjects/CScanTreeInventory.cpp
    src/MetroidPrime/ScriptObjects/CScriptAreaProperties.cpp
    src/MetroidPrime/ScriptObjects/CScriptCannonBall.cpp
    src/MetroidPrime/ScriptObjects/CScriptHUDMemo.cpp
    src/MetroidPrime/ScriptObjects/CScriptPickup.cpp
    src/MetroidPrime/ScriptObjects/CScriptSequenceTimer.cpp
    src/MetroidPrime/ScriptObjects/CScriptSpawnPoint.cpp
    src/MetroidPrime/ScriptObjects/CScriptStreamedMusic.cpp
    src/MetroidPrime/Tweaks/Tweaks.cpp
    src/MetroidPrime/Weapons/CPowerBeam.cpp
    src/MetroidPrime/Weapons/CGunWeaponTouch.cpp
    src/MetroidPrime/main.cpp
    src/REL/REL_Setup.cpp
    src/rstl/RstlExtras.cpp
    src/rstl/rstl_map.cpp
    src/rstl/rstl_misc.cpp
    src/rstl/rstl_strings.cpp
)

# src/Dolphin/*.c are configured decompilation units that files.cmake does not
# name, and that is correct rather than an oversight: all four are GameCube
# register shims written as assembly-in-C (u32 typedefs, inline PPC asm, MMIO
# pokes) and none of them compiles for an x86-64 host - 15 errors. On the host
# platform/ai_dma.cpp and platform/shims.cpp replace them. Measured, not assumed:
# adding all four breaks the port build outright.

# ---------------------------------------------------------------------------
# Every remaining DOL object configure.py declares, added 2026-09-26.
# 113 of them were configured and on disk and simply never named here, so the
# port had never compiled the decompilation's own code: 64 are the 100%
# Matching loader thunks, and 192 Load* symbols were undefined at link purely
# because of this omission. tools/check_files_cmake.py now fails the gate on a
# new one.
# src/MetroidPrime/TypesMatch.cpp is configured and on disk but does not build
# for a host: it sizes its throwaway classes with
# `uchar x_pad0[0x2f0 - sizeof(CPhysicsActor)]`, the host's CPhysicsActor is larger
# than retail's 0x2f0, the subtraction underflows and gcc rejects the array. The
# six TypesMatch bodies it holds are defined in PortGlobals.cpp instead, so
# adding it later would duplicate them.
# src/Kyoto/Text/CStringTable.cpp: casts a pointer to `uint` at lines 92 and 98,
# which loses precision on a 64-bit host. Same class of defect as the
# CTweakContents layout: 32-bit game pointers on a 64-bit target.
# src/Runtime/__init_cpp_exceptions.cpp: includes __ppc_eabi_linker.h, which is
# PowerPC EABI linker sections. Host-incompatible by nature.

list(APPEND MP_GAME_SOURCES
# CScriptPlayerActor.cpp and CScriptPlayerTurretRel.cpp are out for the same reason:
# each defines a RELExit and no RELMain, which is the shape my scan above missed
# because it looked for RELMain. Two bare RELExits in one flat link is one
# duplicate definition.
# The 17 module-entry files are deliberately NOT listed. Each is a REL module we
# have reimplemented, and each defines RELMain/RELExit - and some a module-local
# SetFuncPtrs and one an extern-C CModelData ctor - which is correct on the cube,
# where they are separate modules, and impossible in a flat link: adding them
# produced 28 duplicate definitions over four distinct symbols. The fix is a
# host-only rename behind #ifdef __MWERKS__ so MWCC still compiles the retail
# names; tools/rename_module_entries.py writes it and
# platform/compiled_modules.cpp registers the results. That was attempted and
# reverted: it broke 8 module hashes in the matching build, and proving the
# rename costs nothing under mwcceppc is its own piece of work. Until then these
# 14 modules' code is in no port binary.
    src/MetroidPrime/Player/CGunEffectTouch.cpp
    src/MetroidPrime/Player/CGunEffectTouchAll.cpp
# src/MetroidPrime/CModelDataDefaultCtor.cpp is excluded, not overlooked. It exists
# to hold a second copy of CModelData's constructor for a module that needs one, and
# CScriptCannonBall.cpp holds the other copy. Both are correct in separate modules;
# in a flat link they collide on __ct__10CModelDataFv. A constructor's symbol name is
# fixed by its class, so the __MWERKS__ rename the other modules use cannot help here.
    src/MetroidPrime/ScriptLoader/SpacePirate.cpp
    src/MetroidPrime/ScriptLoader/Kralee.cpp
    src/MetroidPrime/ScriptLoader/Parasite.cpp
    src/MetroidPrime/ScriptLoader/PillBug.cpp
    src/MetroidPrime/ScriptLoader/SporbBase.cpp
    src/MetroidPrime/ScriptLoader/Sandworm.cpp
    src/MetroidPrime/ScriptLoader/CommandPirate.cpp
    src/MetroidPrime/ScriptLoader/DarkSamus.cpp
    src/MetroidPrime/ScriptLoader/Ings.cpp
    src/MetroidPrime/ScriptLoader/SandBoss.cpp
    src/MetroidPrime/ScriptLoader/FlyingPirate.cpp
    src/MetroidPrime/ScriptLoader/Grenchler.cpp
    src/MetroidPrime/ScriptLoader/MediumIng.cpp
    src/MetroidPrime/ScriptLoader/MinorIng.cpp
    src/MetroidPrime/ScriptLoader/ElitePirate.cpp
    src/MetroidPrime/ScriptLoader/Blogg.cpp
    src/MetroidPrime/ScriptLoader/MetroidAlpha.cpp
    src/MetroidPrime/ScriptLoader/GunTurretBase.cpp
    src/MetroidPrime/ScriptLoader/Lumite.cpp
    src/MetroidPrime/ScriptLoader/Shrieker.cpp
    src/MetroidPrime/ScriptLoader/Splinter.cpp
    src/MetroidPrime/ScriptLoader/SplitterMainChassis.cpp
    src/MetroidPrime/ScriptLoader/ChozoGhost.cpp
    src/MetroidPrime/ScriptLoader/Tryclops.cpp
    src/MetroidPrime/ScriptLoader/WispTentacle.cpp
    src/MetroidPrime/ScriptLoader/SpankWeed.cpp
    src/MetroidPrime/ScriptLoader/DarkTrooper.cpp
    src/MetroidPrime/ScriptLoader/GlowBug.cpp
    src/MetroidPrime/ScriptLoader/IngSpaceJumpGuardian.cpp
    src/MetroidPrime/ScriptLoader/DigitalGuardian.cpp
    src/MetroidPrime/ScriptLoader/Shredder.cpp
    src/MetroidPrime/ScriptLoader/FrontEndDataNetwork.cpp
    src/MetroidPrime/ScriptLoader/StoneToad.cpp
    src/MetroidPrime/ScriptLoader/Coin.cpp
    src/MetroidPrime/ScriptLoader/CannonBall.cpp
    src/MetroidPrime/ScriptLoader/Krocus.cpp
    src/MetroidPrime/ScriptLoader/AIMannedTurret.cpp
    src/MetroidPrime/ScriptLoader/EmperorIngStage1.cpp
    src/MetroidPrime/ScriptLoader/OctopedeSegment.cpp
    src/MetroidPrime/ScriptLoader/Rezbit.cpp
    src/MetroidPrime/ScriptLoader/RsfAudio.cpp
    src/MetroidPrime/ScriptLoader/IngPuddle.cpp
    src/MetroidPrime/ScriptLoader/FlyerSwarm.cpp
    src/MetroidPrime/ScriptLoader/StreamedMovie.cpp
    src/MetroidPrime/ScriptLoader/IngSpiderBallGuardian.cpp
    src/MetroidPrime/ScriptLoader/PuddleSpore.cpp
    src/MetroidPrime/ScriptLoader/EmperorIngStage2Tentacle.cpp
    src/MetroidPrime/ScriptLoader/BacteriaSwarm.cpp
    src/MetroidPrime/ScriptLoader/MetareeSwarm.cpp
    src/MetroidPrime/ScriptLoader/IngBlobSwarm.cpp
    src/MetroidPrime/ScriptLoader/EmperorIngStage3.cpp
    src/MetroidPrime/ScriptLoader/DestructableBarrier.cpp
    src/MetroidPrime/ScriptLoader/SwampBossStage2.cpp
    src/MetroidPrime/ScriptLoader/SwampBossStage1.cpp
    src/MetroidPrime/ScriptLoader/IngBoostBallGuardian.cpp
    src/MetroidPrime/ScriptLoader/PlantScarabSwarm.cpp
    src/MetroidPrime/ScriptLoader/SkyRipple.cpp
    src/MetroidPrime/ScriptLoader/FogOverlay.cpp
    src/MetroidPrime/ScriptLoader/MysteryFlyer.cpp
    src/MetroidPrime/ScriptLoader/AtomicBeta.cpp
    src/MetroidPrime/ScriptLoader/EyeBall.cpp
    src/MetroidPrime/ScriptLoader/DarkSamusBattleStage.cpp
    src/MetroidPrime/ScriptLoader/DarkCommando.cpp
    src/MetroidPrime/ScriptLoader/RubiksPuzzle.cpp
    src/MetroidPrime/Enemies/CAi.cpp
    src/MetroidPrime/Enemies/CPatterned.cpp
    src/MetroidPrime/Enemies/CPatternedCtor.cpp
    src/Kyoto/Animation/CSegId.cpp
    src/Kyoto/Animation/CSegIdList.cpp
    src/Kyoto/Graphics/CCubeSurface.cpp
    src/Kyoto/CDependencyGroup.cpp
    src/Kyoto/Text/CFontImageDef.cpp
    src/Kyoto/CPakFile.cpp
    src/Kyoto/CTimeProvider.cpp
    src/Kyoto/CObjectReference.cpp
    src/Kyoto/CToken.cpp
    src/Runtime/CPlusLibPPC.cpp
    src/MetroidPrime/ScriptObjects/CScriptAIMannedTurret.cpp
    src/MetroidPrime/ScriptObjects/CScriptIngSwarm.cpp
    src/MetroidPrime/ScriptObjects/CScriptWallCrawlerSwarm.cpp
    src/MetroidPrime/Enemies/CSwarmBasicsHealthInfo.cpp
    src/MetroidPrime/Enemies/CSwarmBasicsHooks.cpp
    src/MetroidPrime/Enemies/CSwarmBasicsOrbitPosition.cpp
    src/MetroidPrime/Enemies/CSwarmBasicsCanRender.cpp
    src/MetroidPrime/ScriptObjects/CFlyerSwarm.cpp
    src/MetroidPrime/ScriptObjects/ScriptFrontEndDataNetwork.cpp
    src/MetroidPrime/ScriptObjects/CScriptScriptStreamedMovie.cpp
    src/MetroidPrime/ScriptObjects/CScriptRubiksPuzzle.cpp
    src/MetroidPrime/ScriptObjects/CScriptPlayerProxyAccessors.cpp
    src/MetroidPrime/ScriptObjects/CScriptPuffer.cpp
    src/MetroidPrime/ScriptObjects/CScriptCoin.cpp
    src/MetroidPrime/ScriptObjects/CScriptCoinTouchBounds.cpp
)

# LZO's bundled config assumes 32-bit size_t; set the host width for native builds.
# (The headers are on the include path project-wide; see CMakeLists.txt.)
set_source_files_properties(
    src/LZO/lzo1x_d1.c
    src/LZO/lzo_init.c
    src/LZO/lzo_ptr.c
    PROPERTIES
    COMPILE_DEFINITIONS "SIZEOF_SIZE_T=${CMAKE_SIZEOF_VOID_P}"
)
