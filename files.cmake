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
    src/Kyoto/Basics/CStopwatchCSWData.cpp
    src/Kyoto/Basics/CStopwatchCSWDataWait.cpp
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
    # configure.py Matching. Closes _ZNK11CQuaternion13BuildInvertedEv, in the port's link gap
    # list: the header declared the method and nothing defined it.
    src/Kyoto/Math/CQuaternionBuildInverted.cpp
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
    # configure.py Matching, retail 0x80048CE4..0x80048CF4: CArchitectureMessage::GetParm() and
    # its const overload, 8 bytes each. Unnamed in the retail DOL until symbols.txt was renamed.
    src/MetroidPrime/CArchitectureMessageGetParm.cpp
    # configure.py Matching, retail 0x800487B8 (0x5C) and 0x80048834 (0x5C) plus `lbl_803B1B60` and
    # `lbl_803B1B70`: the two message-parm destructors, which are their classes' key functions and
    # so the units that emit their vtables. CFrameMsgParm is the parm CMainFlow::OnMessage builds.
    src/MetroidPrime/CFrameMsgParmDtor.cpp
    src/MetroidPrime/CTimerMsgParmDtor.cpp
    src/MetroidPrime/CDamageInfo.cpp
    src/MetroidPrime/CEntity.cpp
    src/MetroidPrime/CHealthInfo.cpp
    src/MetroidPrime/CIOWinCtor.cpp
    # configure.py Matching. Closes _ZNK13CSimpleShadow12GetTransformEv, which is in the port's
    # link gap list; the body is a single `blr`, so it pulls in no new undefined symbol.
    src/MetroidPrime/CSimpleShadowAccessors.cpp
    # configure.py NonMatching (76.65%). Listed anyway: it closes _ZNK13CSimpleShadow9GetBoundsEv
    # from the port's link gap list, and it calls __ct__6CAABoxFRC9CVector3fRC9CVector3f, which
    # Kyoto/Math/CAABox.cpp already provides.
    src/MetroidPrime/CSimpleShadowGetBounds.cpp
    # configure.py Matching. Closes _ZNK13CSimpleShadow5ValidEv, which is in the port's link gap
    # list. SetAlwaysCalculateRadius is not in that list - the port does not ask for it - but the
    # decompilation unit is Matching either way, and between them these two are what measure the
    # `bool : 1` declaration-order encoding rule.
    src/MetroidPrime/CSimpleShadowValid.cpp
    src/MetroidPrime/CSimpleShadowSetAlwaysCalculateRadius.cpp
    # configure.py Matching. Closes _ZN10CGameState11GetGameModeEv, in the port's link gap list.
    src/MetroidPrime/Player/CGameStateGetGameMode.cpp
    # configure.py Matching. Closes _ZN10CGameState14SetIsDarkWorldEb. It is what forced
    # include/MetroidPrime/Player/CGameState.hpp to model x2ec_flags as a three-bit struct instead
    # of a `u8`, which its own comment had said was waiting for something to reach it.
    src/MetroidPrime/Player/CGameStateSetIsDarkWorld.cpp
    # configure.py Matching. The four 8-byte module-loader setters, each imported by its REL
    # module under its retail name and each in the port's link gap list.
    src/MetroidPrime/ScriptLoader/CoinLoaderSet.cpp
    src/MetroidPrime/ScriptLoader/RsfAudioLoaderSet.cpp
    src/MetroidPrime/ScriptLoader/FlyerSwarmLoaderSet.cpp
    src/MetroidPrime/ScriptLoader/SkyRippleLoaderSet.cpp
    # configure.py Matching, 0x80049E10..0x80049E20 and 0x80049E30..0x80049E98 plus
    # `vtable for CIOWin` at 0x803B1BA0. Net -1 on the port's link: the vtable was the only
    # symbol the linker asked for, and its three slots now point at code in the tree.
    src/MetroidPrime/CIOWinAccessors.cpp
    src/MetroidPrime/CIOWinDtor.cpp
    src/MetroidPrime/CIOWinManagerCtor.cpp
    src/MetroidPrime/CIOWinManagerAddIOWin.cpp
    src/MetroidPrime/CIOWinManagerRemoveAllIOWins.cpp
    src/MetroidPrime/CIOWinManagerPumpMessages.cpp
    src/MetroidPrime/CModelDataModelSlots.cpp
    # configure.py Matching, 0x80018FBC..0x800190F8. The only retail symbol it defines is
    # `__ct__10CModelDataFRC10CModelData`; its only callees are CToken's copy constructor and
    # CToken::Lock, both in src/Kyoto/CToken.cpp, so it is net -1 on the port's link.
    src/MetroidPrime/CModelDataCopyCtor.cpp
    # configure.py Matching, 0x8019E6BC..0x8019E714 (retail's `fn_8019E6BC` alone). The class's
    # other two methods are here rather than beside it, because dtk fills the ranges either side
    # with retail's own bytes and a Matching unit defining them is multiply-defined; see the
    # file's header. All three together: net -3 on the port's link, no callee introduced.
    src/MetroidPrime/CStateManagerScriptMsgArray.cpp
    src/MetroidPrime/CStateManagerScriptMsgArrayCursor.cpp
    src/MetroidPrime/CModelTouchParts.cpp
    src/Kyoto/Graphics/CModelTouch.cpp
    src/Kyoto/CResLoaderAddPakFileAsync.cpp
    src/Kyoto/CResLoaderInsert.cpp
    src/Kyoto/CResLoaderResAccessors.cpp
    src/Kyoto/CResLoaderFindPak.cpp
    src/Kyoto/CResLoaderLoadPartAsync.cpp
    src/Kyoto/CResLoaderLoadResourceSync.cpp
    src/Kyoto/CResLoaderLoadResourceSyncCompressed.cpp
    src/Kyoto/CResLoaderLoadNewResourceSync.cpp
    src/Kyoto/CResLoaderLoadAsync.cpp
    src/Kyoto/CResLoaderGetResIdByName.cpp
    src/Kyoto/CResLoaderPakPump.cpp
    src/Kyoto/CResLoaderGetPakCount.cpp
    src/Kyoto/CResLoaderGetPakFile.cpp
    src/MetroidPrime/CMainFlowCtor.cpp
    # CWorldState's default constructor, retail 0x8015C34C, 276 bytes. `CGameState`'s
    # `rc_ptr<CWorldState>` at +0x3C is filled by `new(1200)` + this, and boot-path step 17
    # dereferences it, so this is the last named piece between CGameState and a frame. Named
    # `fn_8015C34C` because retail's symbol table has no name for it, so the port link gets
    # `fn_8015C34C` and not a mangled constructor.
    src/MetroidPrime/CWorldStateCtor.cpp
    # configure.py Matching, 0x8001DAF4..0x8001DB54, 0x8001DF48..0x8001DF54 and
    # `vtable for CMainFlow` at 0x803B1770. The vtable's `OnMessage` slot points at retail's own
    # bytes - config/G2ME01/symbols.txt is renamed `fn_8001DF54` ->
    # `OnMessage__9CMainFlowFRC20CArchitectureMessageR18CArchitectureQueue` so dtk's fill object
    # carries the name the vtable needs - so the port's link now asks for
    # `CMainFlow::OnMessage` instead of `vtable for CMainFlow`, and that is a real hole, not a stub.
    # configure.py Matching, retail fn_8001DF54, 0x8001DF54, 0xB4 = 180 bytes: the function
    # `vtable for CMainFlow`'s slot 2 points at. It calls AdvanceGameState and SetGameState, which
    # are retail's own unnamed bytes renamed in symbols.txt, so it is net -1 on the port's link.
    src/MetroidPrime/CMainFlowOnMessage.cpp
    src/MetroidPrime/CMainFlowAccessors.cpp
    src/MetroidPrime/CMainFlowDtor.cpp
    src/MetroidPrime/CInputGeneratorCtor.cpp
    src/MetroidPrime/CPhysicsActor.cpp
    src/MetroidPrime/CMiscTableInit.cpp
    # A configure.py unit (NonMatching, 97.11%) that files.cmake did not name, so the port
    # never compiled it and never defined the one retail symbol it defines:
    # `fn_800E6AD0` = CModelData's default constructor, 0x800E6AD0, 0x98 bytes. Its only
    # callee is CColor::White(), which src/Kyoto/Graphics/CColor.cpp above already defines,
    # so wiring it in closes exactly one gap and opens none.
    src/MetroidPrime/CRuleSet.cpp
    src/MetroidPrime/CStateManager.cpp
    src/MetroidPrime/HUD/CHUDMemoParms.cpp
    src/MetroidPrime/Player/CGameOptions.cpp
    src/MetroidPrime/Player/CGameOptionsDefaults.cpp
    src/MetroidPrime/Player/CPlayer.cpp
    src/MetroidPrime/Player/CPlayerGetTweakPlayer.cpp
    src/MetroidPrime/Player/CPlayerGetPlayerIndex.cpp
    src/MetroidPrime/Player/CMorphBallC80.cpp
    # fn_80180738, retail 0x80180738, 0x24 = 36 bytes: the default constructor of
    # CGameState's CHintOptions member. configure.py Matching, and it calls nothing,
    # so it is net -1 on the port's link. See docs/research/boot_path.md.
    src/MetroidPrime/Player/CHintOptionsCtor.cpp
    src/MetroidPrime/Player/CPlayerGun.cpp
    src/MetroidPrime/Player/CPlayerState.cpp
    # Not a configure.py unit, on purpose: it holds the PC-side definitions of
    # the retail globals the decompilation can only declare, and a new small-data
    # symbol in any unit shifts that unit's SDA offsets. See the file's header.
    src/MetroidPrime/PortGlobals.cpp
src/MetroidPrime/PortLinkStubs.cpp
    # CTweakPlayer's five accessors, as two units. configure.py claims these two,
    # so unlike PortGlobals.cpp they are Matching and count as linked.
    src/MetroidPrime/Tweaks/CTweakPlayerAnalog.cpp
    src/MetroidPrime/Tweaks/CTweakPlayerSuit.cpp
    # CGraphics' time-provider pair and screen-position accessor. configure.py
    # claims all three, so they are Matching and count as linked in both worlds.
    src/Kyoto/Graphics/CGraphicsTimeProvider.cpp
    src/Kyoto/Graphics/CGraphicsScreenPosition.cpp
    # Also not a configure.py unit, for a different reason: the 136 SLdr* struct
    # constructors/destructors retail spells __ct__/__dt__ and the host spells
    # C1Ev/D1Ev, so no retail range can be claimed for them at all. See the
    # file's header and docs/research/sldr_ctors.md.
    src/MetroidPrime/ScriptLoader/SLdrStructMembers.cpp
    # Not a configure.py unit, on the same grounds: it holds the host-only bodies of
    # CMain::OpenWindow and CMain::RsMain, whose retail bodies cannot be written yet
    # and must not perturb MetroidPrime/main.cpp. See the file's header.
    src/MetroidPrime/PortBoot.cpp
    # Not a configure.py unit, on the same grounds: it is the host stand-in for Tweaks.rel's
    # REL_CreateTweakGlobals, the only writer of gpTweakPlayerA (0x80418F44), which
    # CGameArchitectureSupport's constructor dereferences at 0x80007F38 with no null test.
    # See the file's header and docs/research/tweak_globals.md.
    src/MetroidPrime/PortTweakGlobals.cpp
    # The module-publish thunks: one store per module, retail's 8-byte Set* family.
    # Port-side only (absent from configure.py), so it closes three link symbols and
    # makes a module's function-pointer table actually reachable.
    src/MetroidPrime/ModulePublish.cpp
    # The port's CAudioSys and CStreamAudioManager bodies. Not a configure.py unit:
    # seven of these fourteen symbols have Matching units in src/Kyoto/Audio/, but
    # their retail bodies call unnamed AUDIO/DSP wrappers that exist only inside
    # main.dol, so a host build needs its own. Eleven of the fourteen are reached
    # before the game's first frame. See the file's header and
    # docs/research/audio_stack.md.
    src/MetroidPrime/PortAudio.cpp
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
    # The 64 REL-module loader thunks (LoadSpacePirate ... LoadRubiksPuzzle). Each is a
    # Matching unit in configure.py; retail names them
    # Load<Name>__FR13CStateManagerR12CInputStreamRC11CEntityInfo and the host build mangles
    # the same C++ Load<Name> to the _Z<len>Load<Name>R13CStateManagerR... symbol the port's
    # ScriptLoader.cpp table needs, so one file serves both. These were measured as still
    # MISSING in the port link before this list existed - see docs/research/real_loaders.md.
    src/MetroidPrime/ScriptLoader/AIMannedTurret.cpp
    src/MetroidPrime/ScriptLoader/AtomicBeta.cpp
    src/MetroidPrime/ScriptLoader/BacteriaSwarm.cpp
    src/MetroidPrime/ScriptLoader/Blogg.cpp
    src/MetroidPrime/ScriptLoader/CannonBall.cpp
    src/MetroidPrime/ScriptLoader/ChozoGhost.cpp
    src/MetroidPrime/ScriptLoader/Coin.cpp
    src/MetroidPrime/ScriptLoader/CommandPirate.cpp
    src/MetroidPrime/ScriptLoader/DarkCommando.cpp
    src/MetroidPrime/ScriptLoader/DarkSamus.cpp
    src/MetroidPrime/ScriptLoader/DarkSamusBattleStage.cpp
    src/MetroidPrime/ScriptLoader/DarkTrooper.cpp
    src/MetroidPrime/ScriptLoader/DestructableBarrier.cpp
    src/MetroidPrime/ScriptLoader/DigitalGuardian.cpp
    src/MetroidPrime/ScriptLoader/ElitePirate.cpp
    src/MetroidPrime/ScriptLoader/EmperorIngStage1.cpp
    src/MetroidPrime/ScriptLoader/EmperorIngStage2Tentacle.cpp
    src/MetroidPrime/ScriptLoader/EmperorIngStage3.cpp
    src/MetroidPrime/ScriptLoader/EyeBall.cpp
    src/MetroidPrime/ScriptLoader/FlyerSwarm.cpp
    src/MetroidPrime/ScriptLoader/FlyingPirate.cpp
    src/MetroidPrime/ScriptLoader/FogOverlay.cpp
    src/MetroidPrime/ScriptLoader/FrontEndDataNetwork.cpp
    src/MetroidPrime/ScriptLoader/GlowBug.cpp
    src/MetroidPrime/ScriptLoader/Grenchler.cpp
    src/MetroidPrime/ScriptLoader/GunTurretBase.cpp
    src/MetroidPrime/ScriptLoader/IngBlobSwarm.cpp
    src/MetroidPrime/ScriptLoader/IngBoostBallGuardian.cpp
    src/MetroidPrime/ScriptLoader/IngPuddle.cpp
    src/MetroidPrime/ScriptLoader/IngSpaceJumpGuardian.cpp
    src/MetroidPrime/ScriptLoader/IngSpiderBallGuardian.cpp
    src/MetroidPrime/ScriptLoader/Ings.cpp
    src/MetroidPrime/ScriptLoader/Krocus.cpp
    src/MetroidPrime/ScriptLoader/Lumite.cpp
    src/MetroidPrime/ScriptLoader/MediumIng.cpp
    src/MetroidPrime/ScriptLoader/MetareeSwarm.cpp
    src/MetroidPrime/ScriptLoader/MetroidAlpha.cpp
    src/MetroidPrime/ScriptLoader/MinorIng.cpp
    src/MetroidPrime/ScriptLoader/MysteryFlyer.cpp
    src/MetroidPrime/ScriptLoader/OctopedeSegment.cpp
    src/MetroidPrime/ScriptLoader/Parasite.cpp
    src/MetroidPrime/ScriptLoader/PillBug.cpp
    src/MetroidPrime/ScriptLoader/PlantScarabSwarm.cpp
    src/MetroidPrime/ScriptLoader/PuddleSpore.cpp
    src/MetroidPrime/ScriptLoader/Rezbit.cpp
    src/MetroidPrime/ScriptLoader/RsfAudio.cpp
    src/MetroidPrime/ScriptLoader/RubiksPuzzle.cpp
    src/MetroidPrime/ScriptLoader/SandBoss.cpp
    src/MetroidPrime/ScriptLoader/Sandworm.cpp
    src/MetroidPrime/ScriptLoader/Shredder.cpp
    src/MetroidPrime/ScriptLoader/Shrieker.cpp
    src/MetroidPrime/ScriptLoader/SkyRipple.cpp
    src/MetroidPrime/ScriptLoader/SpacePirate.cpp
    src/MetroidPrime/ScriptLoader/SpankWeed.cpp
    src/MetroidPrime/ScriptLoader/Splinter.cpp
    src/MetroidPrime/ScriptLoader/SplitterMainChassis.cpp
    src/MetroidPrime/ScriptLoader/SporbBase.cpp
    src/MetroidPrime/ScriptLoader/StoneToad.cpp
    src/MetroidPrime/ScriptLoader/StreamedMovie.cpp
    src/MetroidPrime/ScriptLoader/SwampBossStage1.cpp
    src/MetroidPrime/ScriptLoader/SwampBossStage2.cpp
    src/MetroidPrime/ScriptLoader/Tryclops.cpp
    src/MetroidPrime/ScriptLoader/WispTentacle.cpp
    src/MetroidPrime/ScriptLoader.cpp
    src/MetroidPrime/ScriptLoaderRel.cpp
    src/MetroidPrime/ScriptObjects/CScanTreeInventory.cpp
    src/MetroidPrime/ScriptObjects/CScriptRelay.cpp
    src/MetroidPrime/ScriptObjects/CUnknown90.cpp
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
    # `main.cpp`'s upper half, split off for the DOL (see that file's header and
    # `MetroidPrime/CGameGlobalObjectsCtor.cpp`'s). It has to be listed here for the same reason
    # the split happened: `InvokeCMain`, `CMain::~CMain`, `__sys_free`,
    # `CGameArchitectureSupport::~CGameArchitectureSupport`, `~CPlayerState` and six more left
    # `main.cpp` with the range, and the port needs them.
    src/MetroidPrime/mainTail.cpp
    # Member constructors CGameGlobalObjects' constructor calls (fn_8016C230, fn_801F0A44, both
    # Matching), and the CGameState-subtree units that are net zero on the port's link now that
    # SGameStateBlock's rstl::vector<unsigned char> operations (CGameStateBlock*.cpp) and the
    # subtree's guest constants (PortGlobals.cpp) exist. The constructor itself, the builder at
    # +0x108 and the rest of the subtree are not listed: tools/check_files_cmake.py has the
    # measurements, docs/research/cgameglobalobjects_ctor.md the accounting.
    src/MetroidPrime/CInGameTweakManagerCtor.cpp
    src/MetroidPrime/CGameGlobalObjectsTailCtor.cpp
    src/MetroidPrime/Player/CGameStateBlockCopyCtor.cpp
    src/MetroidPrime/Player/CGameStateBlockConstruct.cpp
    src/MetroidPrime/Player/CGameStateBlockClear.cpp
    src/MetroidPrime/Player/CGameStateBlockFill.cpp
    src/MetroidPrime/Player/CGameStateBlockReserve.cpp
    src/MetroidPrime/Player/CGameStateBlockCopy.cpp
    src/MetroidPrime/Player/CGameStateSlotsCtor.cpp
    src/MetroidPrime/Player/CGameStateSlotDefaults.cpp
    src/MetroidPrime/Player/CGameStateSysOptsPutTo.cpp
    src/MetroidPrime/Player/CGameStateBlockDtor.cpp
    src/REL/REL_Setup.cpp
    src/rstl/RstlExtras.cpp
    src/rstl/rc_ptr_copy.cpp
    src/rstl/rstl_map.cpp
    src/rstl/rstl_misc.cpp
src/rstl/rstl_string_member_op.cpp
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
# The twelve below carry that rename in the form CScriptForgottenObject.cpp uses:
# every definition sits in an #ifdef __MWERKS__ / #else pair whose MWCC branch is
# HEAD's source token for token, so mwcceppc sees exactly what it saw before. A
# loader variable a unit's split does not claim .bss for (CScriptRsfAudio,
# CScriptPlayerProxy) stays `extern` under MWCC and is defined on the host only;
# see docs/research/rel_rename_hazard.md. Still out: CSwarmBasicsREL.cpp,
# CScriptPlayerActor.cpp and CScriptPlayerTurretRel.cpp.
    src/MetroidPrime/ScriptObjects/CFlyerSwarmRel.cpp
    src/MetroidPrime/ScriptObjects/CScriptMetaree.cpp
    src/MetroidPrime/ScriptObjects/CScriptCoinRel.cpp
    src/MetroidPrime/ScriptObjects/CScriptPlayerActorMain.cpp
    src/MetroidPrime/ScriptObjects/CScriptPlayerProxy.cpp
    src/MetroidPrime/ScriptObjects/CScriptPufferRel.cpp
    src/MetroidPrime/ScriptObjects/CScriptRiftPortal.cpp
    src/MetroidPrime/ScriptObjects/CScriptRsfAudio.cpp
    src/MetroidPrime/ScriptObjects/CScriptSafeZone.cpp
    src/MetroidPrime/ScriptObjects/CScriptSkyRipple.cpp
    src/MetroidPrime/ScriptObjects/CScriptWallCrawler.cpp
    src/MetroidPrime/ScriptObjects/ScriptGuiSetup.cpp
    src/MetroidPrime/Player/CGunEffectTouch.cpp
    src/MetroidPrime/Player/CGunEffectTouchAll.cpp
# CModelDataDefaultCtor.cpp is listed, and lane e6 measured that as net -1, which was
# true of its configuration and is **wrong of this one**: with the 96 omitted units
# added and the module-entry files excluded, removing it takes the real link from 533
# undefined to 534. It holds the DOL's copy of CModelData's constructor; the two
# module copies live in files that are out for the module-entry reason below.
    src/MetroidPrime/CModelDataDefaultCtor.cpp
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
    src/Kyoto/CFactoryMgr.cpp
    src/Kyoto/CResFactoryCtor.cpp
    # configure.py Matching, 0x802FA960..0x802FAA20: `CResFactory::Build`, 100.00% and
    # flip_test PASS. In the port's link it is what makes `_ZTV11CResFactory` a real vtable
    # instead of the 64 zero bytes PortReachStubs.cpp used to carry, and it costs three named
    # holes (`fn_802FAAE4`, `fn_802FA1BC`, `fn_802FA7D4`) in exchange.
    src/Kyoto/CResFactoryBuild.cpp
    # Port-only. Defines `~CResFactory` - the key function, so the vtable is emitted here - and
    # the four members the port has no body for. See the file's own comment.
    src/Kyoto/CResFactoryPortVirtuals.cpp
    src/Kyoto/CFactoryMgrRegistrars.cpp
    src/Kyoto/CFactoryFunctionsPort.cpp
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
