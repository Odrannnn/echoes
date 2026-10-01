# src/Kyoto/CARAMManager.cpp is not listed in upstream configure.py, and
# include/Kyoto/CARAMManager.hpp is still a stub missing members the .cpp defines.
# Re-enable it when the decompilation catches up.
set(MP_GAME_SOURCES
    src/Kyoto/Alloc/CCircularBuffer.cpp
    src/Kyoto/Alloc/CGameAllocator.cpp
    src/Kyoto/Alloc/CMediumAllocPool.cpp
    src/Kyoto/Alloc/CMemory.cpp
    # Port-only: `__nw__FUlPCcPCc`, the mwcceppc-mangled operator new that retail-shaped units
    # call through extern "C", forwarded to CMemory.cpp's. See the file's header.
    src/Kyoto/Alloc/PortMwccNew.cpp
    src/Kyoto/Alloc/CSmallAllocPool.cpp
    src/Kyoto/Alloc/IAllocator.cpp
    src/Kyoto/Animation/CCharAnimTime.cpp
    src/Kyoto/Animation/CTimeRemainderAndFraction.cpp
    src/Kyoto/Audio/CStaticAudioPlayer.cpp
    src/Kyoto/Audio/g721.cpp
    src/Kyoto/Basics/COsContext.cpp
    src/Kyoto/Basics/CStopwatch.cpp
    # CStopwatchCSWData.cpp and CStopwatchCSWDataWait.cpp were dropped here on 2026-09-28:
    # upstream's configure.py unit Kyoto/Basics/CSWDataDolphin.cpp claims the same
    # 0x8028C17C-0x8028C28C range, is 100% matched at 3/3 (build/report.json), and defines
    # the same two symbols, and its Initialize reads OS_TIMER_CLOCK instead of the guest
    # address 0x800000F8 the carve had to #ifdef out. tools/check_files_cmake.py carries both
    # carve paths with that reason.
    src/Kyoto/Basics/RAssertDolphin.cpp
    src/Kyoto/CARAMManagerPort.cpp
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
    # CActor::SetDirtyFlags, retail 0x8004A0A0, Matching at 100.00%: it closes a symbol
    # the port's link already asks for, and its three setters are inline in CActor.hpp so
    # the object carries no new relocation.
    src/MetroidPrime/CActorSetDirtyFlags.cpp
    # CCallStack.cpp was dropped here on 2026-09-28: it was this port's own carve of
    # 0x8028BFD8..0x8028BFF4, and upstream's configure.py unit Kyoto/Alloc/CCallStackDolphin.cpp
    # claims the same range, is 100% matched at 3/3, and defines the same three members plus
    # the `kUnknownType` string the carve had to get from PortGlobals.cpp. It is listed in the
    # upstream block at the end of this file; tools/check_files_cmake.py carries the carve path.
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
    # Port-only: `LdrToEntityInfo` (retail 0x80239BD4, 0x38) - retail's non-const body, plus the
    # const overload the four loaders whose `info` is a `const CEntityInfo&` bind to here
    # (`undef.base.txt` lines 159/160). 0x80239BD4 is in the same unclaimed `.text` range as
    # SLdrEditorProperties_Load.cpp below (RubiksPuzzle ends 0x802399F4, ScriptLoader starts
    # 0x80242894), so this is a files.cmake entry only and never a configure.py one. See the
    # file's own header for the instruction-by-instruction reading.
    src/MetroidPrime/LdrToEntityInfo.cpp
    # Port-only: retail's REL module manager, the closure under `fn_801F05D0` (0x801F05D0 and
    # 0x80213650-0x80213BAC, 0x8033EDA8-0x8033EE7C). Its link and unlink run the compiled module's
    # host init/shutdown instead of the PowerPC image's prolog/epilog, so it can never be a
    # configure.py unit as written. See the file's header.
    src/MetroidPrime/PortModuleManager.cpp
    # Port-only: retail's CGMSinglePlayer (0x80193E08 ctor, 0x803B5CB0 vtable), from unsplit text.
    # The reach-stubbed ctor left CGameState's game mode an uninitialised object. See the header.
    src/MetroidPrime/PortCGMSinglePlayer.cpp
    # Port-only: host definitions of the `CTweakBall` accessors CMorphBall.cpp calls, copied
    # character for character from src/MetroidPrime/Tweaks/CTweakBall.cpp. That unit is in
    # tools/check_files_cmake.py's EXCLUDED list and a lane cannot un-exclude it, so it has
    # no compiled home for them; every call CMorphBall.cpp makes has to be paid for here or
    # the port link grows. Same grounds as PortModuleManager.cpp. See the file's header, and
    # do not compile the two together.
    src/MetroidPrime/PortCTweakBall.cpp
    # Port-only: host definitions of `CGuiWidget::SetColor` and
    # `CGuiFrame::ProcessUserInput`, copied character for character from
    # src/GuiSys/CGuiWidget.cpp and src/GuiSys/CGuiFrame.cpp. CQuitGameScreen.cpp's
    # SetColors and ProcessUserInput call both. Both GuiSys units are in
    # tools/check_files_cmake.py's EXCLUDED list and a lane cannot un-exclude them, so they
    # have no compiled home for these bodies; the calls have to be paid for here or the port
    # link grows. Same grounds as PortCTweakBall.cpp above. See the file's header, and do
    # not compile the two together.
    src/MetroidPrime/PortCGuiAccessors.cpp
    # Port-only: `fn_80215860`, the 3-instruction tweak-control bool reader that
    # CMorphBall.cpp's `IsMovementAllowed` calls. Retail's copy at 0x80215860 is in an
    # unclaimed auto-split range, so no configure.py unit can own it. See the file's header,
    # and do not compile the two together.
    src/MetroidPrime/PortCTweakPlayerControls.cpp
    # Port-only: the three unclaimed `.data` vtables CMorphBall.cpp's `fn_800C88C0` /
    # `fn_800C33DC` store (`lbl_803B36F0` / `lbl_803B36FC` / `lbl_803B1750`). dtk fills them
    # with retail's bytes in the DOL build; the host build has no dtk step, so without this
    # file the port link grows and `tools/gate.sh` fails on `link-gap`. See the file's
    # header, and do not compile the two together.
    src/MetroidPrime/PortCMorphBallVtables.cpp
    src/MetroidPrime/CHealthInfo.cpp
    # CIOWinCtor.cpp, CIOWinDtor.cpp and CIOWinAccessors.cpp were dropped here on
    # 2026-09-28: configure.py's own src/MetroidPrime/CIOWin.cpp (MatchingFor, 100.00%
    # matched, 10 functions, retail 0x80049D28..0x80049E44) defines all eight symbols the
    # three carves held, and both copies in one flat link is a multiple definition. See
    # tools/check_files_cmake.py's EXCLUDED list.
    # The boot's three step-17 IOWins as real classes with real vtables (2026-09-29). The DOL's
    # own units store retail's vtable *object* into word 0, which a host link binds to a zero
    # stub, so the first virtual call faulted - the frame-1 crash in fn_80049244.
    # CErrorOutputWindow.cpp is upstream's header-based class (NonMatching in the DOL) and
    # replaces the `(bool)` carve CErrorOutputWindowCtor.cpp, whose signature the header no
    # longer declares; PortIOWins.cpp is port-only and holds CAudioStateWin and
    # CConsoleOutputWindow. See each file's header.
    src/MetroidPrime/CErrorOutputWindow.cpp
    src/MetroidPrime/PortIOWins.cpp
    # configure.py Matching. Closes _ZNK13CSimpleShadow12GetTransformEv, which is in the port's
    # link gap list; the body is a single `blr`, so it pulls in no new undefined symbol.
    # configure.py NonMatching (76.65%). Listed anyway: it closes _ZNK13CSimpleShadow9GetBoundsEv
    # from the port's link gap list, and it calls __ct__6CAABoxFRC9CVector3fRC9CVector3f, which
    # Kyoto/Math/CAABox.cpp already provides.
    # configure.py Matching. Closes _ZNK13CSimpleShadow5ValidEv, which is in the port's link gap
    # list. SetAlwaysCalculateRadius is not in that list - the port does not ask for it - but the
    # decompilation unit is Matching either way, and between them these two are what measure the
    # `bool : 1` declaration-order encoding rule.
    # configure.py Matching. The five 8-byte module-loader setters are each imported by their
    # REL module under the retail name; four are also in the port's link gap list.
    src/MetroidPrime/ScriptLoader/CoinLoaderSet.cpp
    src/MetroidPrime/ScriptLoader/RsfAudioLoaderSet.cpp
    src/MetroidPrime/ScriptLoader/FlyerSwarmLoaderSet.cpp
    src/MetroidPrime/ScriptLoader/SkyRippleLoaderSet.cpp
    src/MetroidPrime/ScriptLoader/IngBlobSwarmLoaderSet.cpp
    # configure.py Matching, 0x80049E10..0x80049E20 and 0x80049E30..0x80049E98 plus
    # `vtable for CIOWin` at 0x803B1BA0. Net -1 on the port's link: the vtable was the only
    # symbol the linker asked for, and its three slots now point at code in the tree.
    # CIOWinAccessors.cpp and CIOWinDtor.cpp: see the note at CIOWinCtor.cpp's old place -
    # superseded by configure.py's src/MetroidPrime/CIOWin.cpp, which is now listed.
    # configure.py Matching, the whole unit 0x80048F78..0x80049E10. Replaces the five split
    # files (Ctor, AddIOWin, RemoveIOWin, RemoveAllIOWins, PumpMessages) and Carve80049244.cpp,
    # which covered pieces of this range and left DistributeOneMessage (0x8004935C) unwritten.
    src/MetroidPrime/CIOWinManager.cpp
    # Port-only: CPreFrontEnd, retail 0x80192708..0x80192864, dtk's auto gap with no split, so
    # there is no configure.py unit. CMainFlow::SetGameState creates it on the first frame.
    src/MetroidPrime/CPreFrontEnd.cpp
    src/MetroidPrime/CModelDataModelSlots.cpp
    # configure.py Matching, 0x80018FBC..0x800190F8. The only retail symbol it defines is
    # `__ct__10CModelDataFRC10CModelData`; its only callees are CToken's copy constructor and
    # CToken::Lock, both in src/Kyoto/CToken.cpp, so it is net -1 on the port's link.
    src/MetroidPrime/CModelDataCopyCtor.cpp
    # Port-only: `CModelData::~CModelData()`, retail `__dt__10CModelDataFv`, 0x800E6810, 0xF0.
    # No split covers that address, so there is no configure.py unit for it and the file is
    # listed here alone. Closes `_ZN10CModelDataD1Ev` (five referring objects) and opens
    # `_ZN9CAnimDataD1Ev`, which is retail's own dependency - the auto_ptr's `delete` is
    # retail's `bl fn_8002C340` at 0x800E68D0 - so the port's undefined count does not move.
    # The file's header has the disassembly and the member-by-member list of what it frees.
    # configure.py Matching, 0x8019E6BC..0x8019E714 (retail's `fn_8019E6BC` alone). The class's
    # other two methods are here rather than beside it, because dtk fills the ranges either side
    # with retail's own bytes and a Matching unit defining them is multiply-defined; see the
    # file's header. All three together: net -3 on the port's link, no callee introduced.
    src/MetroidPrime/CStateManagerScriptMsgArray.cpp
    src/MetroidPrime/CStateManagerScriptMsgArrayCursor.cpp
    src/MetroidPrime/CModelTouchParts.cpp
    src/Kyoto/Graphics/CModelTouch.cpp
    # Upstream's CResLoader TU (configure.py Matching) replaces the thirteen CResLoader*
    # fragments the port carried before the upstream merge; CGroupReadCache and
    # CBufferedDvdRequest are the new types it builds.
    src/Kyoto/CResLoader.cpp
    src/Kyoto/Streams/CBufferedDvdRequest.cpp
    # CMainFlowCtor.cpp, CMainFlowOnMessage.cpp, CMainFlowAccessors.cpp and
    # CMainFlowDtor.cpp stay. configure.py's own src/MetroidPrime/CMainFlow.cpp (MatchingFor,
    # 100.00% matched, 7 functions, retail 0x8001DAF4..0x8001E070) does define the same ten
    # symbols, and dropping the four carves for it was measured and rejected: with them gone
    # the unit takes the port's undefined count 318 -> 334, because it opens sixteen symbols
    # (`CMain::StreamNewGameState(bool)`, `CGameState::SetGameMode(CGameMode*)`, `CCredits::CCredits`,
    # two `fn_8014...` and twelve more) and closes none. So the carves stay and CMainFlow.cpp is
    # excluded; see tools/check_files_cmake.py's EXCLUDED list.
    src/MetroidPrime/CMainFlowCtor.cpp
    src/MetroidPrime/CWorldStateCtor.cpp
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
    # Upstream's unit (ctor + Update, MatchingFor G2ME01) replaces CInputGeneratorCtor.cpp, which
    # held the constructor alone. Update's two MakeMsg factories are in PortMakeMsg.cpp.
    src/MetroidPrime/CInputGenerator.cpp
    src/MetroidPrime/PortMakeMsg.cpp
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
    # `fn_80161D04` (0x80161D04, 0x1C4), `fn_80161EC8` (0x80161EC8, 0x78) and `fn_80161F40`
    # (0x80161F40, 0x7C) - `rstl::sort` / `__sort3` / `__insertion_sort` for
    # `rstl::vector<rstl::pair<Ui,Ui> >`, carved whole out of the unclaimed gap that dtk was
    # handing to `main/auto_03_80161D04_text`. It defines the placeholder `fn_80161D04` that
    # CGameOptions.cpp and CMemoryCard.cpp both reference, so the port link closes one gap.
    src/auto_03_80161D04_text.cpp
    src/MetroidPrime/Player/CPlayer.cpp
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
    # `fn_80006954` (retail 0x80006954, 0x58) and `fn_80008B60` (0x80008B60, 0xC8) - the frame
    # loop's two frame-time calls, which `CMain::RsMain` makes at 0x80006114 and 0x80006234 and
    # which were `PORT_FRAME_STOP`s, so nothing after the first frame ran. Port-only for the
    # reason the file's header gives: 0x80006954 is inside MetroidPrime/main.cpp's .text claim
    # and 0x80008B60 inside MetroidPrime/mainTail.cpp's, so a unit for either needs that unit's
    # claim cut. Not a carve: it claims nothing in the DOL, so main.dol cannot move because of
    # it. Delete it when either of those cuts lands, as with PortModuleManager.cpp.
    src/MetroidPrime/PortFrameTimeHistory.c
    # Single-function units carved out of dtk `auto_03_*` ranges - an accessor and the two ARAM
    # pointer sentinels. (`CPatterned::VSlot70/72` and `CAi::CanBeShot` were deleted in the
    # upstream merge, 2026-09-28: upstream's `CPatterned` has no such slots and defines
    # `CanBeShot` inline.)
    # CAxisAngleGetVector.cpp was dropped here on 2026-09-28: configure.py's own
    # src/MetroidPrime/CAxisAngle.cpp (MatchingFor, 100.00% matched, 13 functions, retail
    # 0x8001D0CC..0x8001D430) defines `CAxisAngle::GetVector() const` along with the five
    # CAxisAngle symbols PortLinkStubs.cpp stubbed, and both copies in one flat link is a
    # multiple definition. See tools/check_files_cmake.py's EXCLUDED list.
    src/Kyoto/CARAMManagerGetInvalidAlloc.cpp
    src/Kyoto/CARAMManagerIsAllocValid.cpp
    # More single-purpose units. The two METROTRK files are the SDK's no-op trace hooks and
    # nothing in the port calls them. (CPlayerGetTweakPlayer, CPlayerGetPlayerIndex and
    # SLdrTweak{CameraBob,SlideShow,Targeting}_Load were deleted in the 2026-09-28 upstream merge:
    # upstream's CPlayer.cpp and SLdrTweak*.cpp define the same symbols. The two CannonBall units
    # were listed here then, and are back because the merge also folded LoadCannonBall and
    # SetLoader_CannonBall into ScriptLoaderRel.cpp - see src/MetroidPrime/ScriptLoader/CannonBall.cpp.)
    src/MetroidPrime/ScriptLoader/CannonBall.cpp
    src/MetroidPrime/ScriptLoader/CannonBallLoaderSet.cpp
    src/Runtime/MetroTRKConsoleStubs.cpp
    src/Runtime/InitMetroTRKBba.c
    # --- host-port link wave, 2026-09-28: bodies that already existed in a src/ file
    # `files.cmake` did not list, plus the two `TypesMatch` key functions the upstream merge
    # declared without a body. None of these is a `configure.py` unit, so the DOL is
    # byte-identical with or without them; each is one function (or one small closed group) per
    # file precisely so the port's undefined count only ever falls. See each file's header for
    # the retail addresses and the "why not the whole .cpp" argument.
    # CGrappleArm: the two state helpers CPlayerGun::ReturnArmAndGunToDefault calls.
    src/MetroidPrime/Player/CGrappleArmReturnToDefault.cpp
    # CGameCollision: the static/dynamic combiner CStateManager::RayWorldIntersection
    # forwards to. Its two callees are already in the recorded baseline, so this is net -1.
    src/MetroidPrime/CGameCollisionRayWorldIntersection.cpp
    # CGameCamera::SetAspectRatio, and CScriptCamera::MarkViewed /
    # CScriptActor::CheckActorRenderOnly: one body each out of three `NonMatching` units.
    src/MetroidPrime/Cameras/CGameCameraSetAspectRatio.cpp
    src/MetroidPrime/ScriptObjects/CScriptCameraMarkViewed.cpp
    src/MetroidPrime/ScriptObjects/CScriptActorCheckActorRenderOnly.cpp
    # FogOverlay's (module 23) two empty virtual overrides, `fn_23_624` and `fn_23_628` - each a
    # single `blr` in retail - so listing them adds two defined symbols and no dependency.
    # `CFogOverlayRel.cpp`, the same module's head, is NOT listed: it defines RELMain/RELExit,
    # which collide in a flat link, and tools/check_files_cmake.py counts that case separately
    # ("further units are out because they define a module entry point") rather than failing.
    src/MetroidPrime/ScriptObjects/CFogOverlayRelStubs.cpp
    # CRumbleManager::StopRumble. Its own file because CRumbleManager.cpp is a
    # `MatchingFor("G2ME01")` unit and must not be edited; see the file's header.
    src/MetroidPrime/CRumbleManagerStopRumble.cpp
    # CDamageVulnerability::NormalVulnerabilty - retail
    # `NormalVulnerabilty__20CDamageVulnerabilityFv`, 0x800DBB70. The unit holds exactly that
    # one function, so the whole file goes in rather than a carve-out of it.
    src/MetroidPrime/CDamageVulnerabilityStatics.cpp
    # CStaticInterference's ctor and Update: the two of its bodies that close an undefined
    # symbol, and Update needs `RemoveSource` from the same file, so the whole unit goes in.
    # Measured on the host: it closes 2 and adds nothing but `memmove`.
    src/MetroidPrime/Player/CStaticInterference.cpp
    # The retail-named float reader CGameState::GetHardModeDamageMultiplier calls, `fn_80216D38`
    # at +0x54 of the opaque Tweaks block.
    src/MetroidPrime/Tweaks/CTweakGameHardModeDamageMultiplier.cpp
    # --- host-port link wave, CPlayerGun decompilation (2026-09-30). Four callees that
    # `CPlayerGun`'s newly decompiled bodies reach, each in a file of its own for the reason
    # `CGrappleArmReturnToDefault.cpp` gives: the unit that holds the body is a whole
    # `NonMatching` unit or, for `CGraphics`, cannot be listed at all, so listing it would be a
    # net *rise* in the port's undefined count. Bodies are retail's, verbatim. Each file's header
    # names the caller and the callees it closes.
    # CGrappleArm::TouchModel, from CPlayerGun::TouchModel.
    src/MetroidPrime/Player/CGrappleArmTouchModel.cpp
    # CGrappleArm::EnterFidget, from CPlayerGun::EnterFidget.
    src/MetroidPrime/Player/CGrappleArmEnterFidget.cpp
    # CGrappleArm::Render, from CPlayerGun::DrawArm.
    src/MetroidPrime/Player/CGrappleArmRender.cpp
    # CGraphics::GetPerspectiveProjectionMatrix, from CPlayerGun::ConvertToScreenSpace.
    # DolphinCGraphics.cpp is retail's whole CGraphics and PORT_NOTES.md records why it is not
    # listed; this is the one body of it the port needs.
    src/Kyoto/Graphics/CGraphicsGetPerspectiveProjectionMatrix.cpp
    # --- host-port link wave, round 2 (2026-09-28): four `CAudioSys` streamed-audio thunks
    # over the SDK's DTK entry points, which `platform/sdk_stubs.cpp` does provide, and the
    # three `CGraphics` immediate-mode setters that touch only the vertex descriptor
    # (`lbl_80416EE0`). Both are decompiled from `build/G2ME01/main.elf`; each file's header
    # gives the retail addresses, the `.sdata2` flag values, and why the rest of each family
    # is not here. Measured on the host: 7 closures, 0 new undefined symbols.
    src/Kyoto/Audio/CAudioSysTrkThunks.cpp
    src/Kyoto/Graphics/CGraphicsStreamState.cpp
    # --- carve2: generated carve units
    src/Kyoto/Math/Carve8001994C.cpp
    src/MetroidPrime/Player/Carve8000B7C4.c
    src/MetroidPrime/Player/Carve8000EC00.c
    src/MetroidPrime/Player/Carve80010F48.c
    src/MetroidPrime/Carve8001935C.c
    src/MetroidPrime/Carve8001FF7C.c
    src/MetroidPrime/Carve800208E8.c
    src/rstl/Carve800239F4.c
    src/Kyoto/Math/Carve80031414.c
    src/Kyoto/Math/Carve80031AC8.c
    src/MetroidPrime/Enemies/Carve800358E0.c
    src/MetroidPrime/Carve80003858.c
    src/MetroidPrime/Carve80045CD4.c
    src/MetroidPrime/Carve80049E20.c
    src/MetroidPrime/Carve8005065C.c
    src/MetroidPrime/Carve800534B0.c
    src/MetroidPrime/Carve80053594.c
    src/MetroidPrime/Carve80054F74.c
    src/MetroidPrime/Carve80055990.c
    src/MetroidPrime/Carve8006653C.c
    src/MetroidPrime/Carve800069AC.c
    src/MetroidPrime/Carve8006CB00.c
    src/MetroidPrime/Carve8007062C.c
    src/MetroidPrime/Carve80071498.c
    src/MetroidPrime/Carve80073594.c
    src/MetroidPrime/Enemies/Carve80073F50.c
    src/MetroidPrime/Enemies/Carve80074774.c
    src/MetroidPrime/Enemies/Carve800766CC.c
    src/MetroidPrime/Enemies/Carve8007C208.c
    src/MetroidPrime/Enemies/Carve8008181C.c
    src/MetroidPrime/Carve800A1598.c
    src/MetroidPrime/Carve800B243C.c
    src/MetroidPrime/ScriptObjects/Carve800B7438.c
    src/MetroidPrime/ScriptObjects/Carve800BDA98.c
    src/MetroidPrime/Player/Carve800C1908.c
    src/MetroidPrime/Player/Carve800D85A0.c
    src/MetroidPrime/Player/Carve800DAE94.c
    src/MetroidPrime/Carve800E39D0.c
    src/MetroidPrime/Carve800E8E4C.c
    src/MetroidPrime/Carve800EC508.c
    src/MetroidPrime/Carve800EC978.c
    src/MetroidPrime/Carve800ED550.c
    src/MetroidPrime/Carve800ED604.c
    src/MetroidPrime/Carve800F0198.c
    src/MetroidPrime/Carve800F1234.c
    src/MetroidPrime/Carve800F1264.c
    src/MetroidPrime/Carve800F15C8.c
    src/MetroidPrime/Carve800F1A18.c
    src/MetroidPrime/Carve800F2390.c
    src/MetroidPrime/Carve800F24AC.c
    src/MetroidPrime/Carve800F2C54.c
    src/MetroidPrime/Carve800F4E28.c
    src/MetroidPrime/Carve800F5004.c
    src/MetroidPrime/Carve800F51EC.c
    src/MetroidPrime/Carve800F549C.c
    src/MetroidPrime/Carve800F609C.c
    src/MetroidPrime/Carve800F794C.c
    src/MetroidPrime/Carve800F7B80.c
    src/MetroidPrime/Carve800F8BDC.c
    src/MetroidPrime/Carve800FAC18.c
    src/MetroidPrime/Carve800FACE0.c
    src/MetroidPrime/Carve800FAFE8.c
    src/MetroidPrime/Carve800FB66C.c
    src/MetroidPrime/Carve800FBEF0.c
    src/MetroidPrime/Carve800FBF68.c
    src/MetroidPrime/Carve800FC474.c
    src/MetroidPrime/Carve800FD2D0.c
    src/MetroidPrime/Carve800FD648.c
    src/MetroidPrime/Carve800FEE98.c
    src/MetroidPrime/Carve800FEF78.c
    src/MetroidPrime/Carve800FF164.c
    src/MetroidPrime/Carve800FF294.c
    src/MetroidPrime/Carve800FFC2C.c
    src/MetroidPrime/Carve800FFD34.c
    src/MetroidPrime/Carve801007D8.c
    src/MetroidPrime/Carve80107994.c
    src/MetroidPrime/Carve8010805C.c
    src/MetroidPrime/Carve8010EE54.c
    src/MetroidPrime/Carve801174E8.c
    src/MetroidPrime/Carve801184E8.c
    src/MetroidPrime/Carve801185AC.c
    src/MetroidPrime/Carve8011A97C.c
    src/MetroidPrime/Carve801255B8.c
    src/MetroidPrime/Carve80127E7C.c
    src/MetroidPrime/Carve8012CB4C.c
    src/MetroidPrime/Carve8012CD10.c
    src/MetroidPrime/Carve8012D164.c
    src/MetroidPrime/Player/Carve801476D0.c
    src/MetroidPrime/Player/Carve80149108.c
    src/MetroidPrime/Player/Carve80149288.c
    src/MetroidPrime/Player/Carve8014A5DC.c
    src/MetroidPrime/Player/Carve8014A6D4.c
    src/MetroidPrime/Player/Carve8014FFCC.c
    src/MetroidPrime/Player/Carve8015180C.c
    src/MetroidPrime/Player/Carve8015294C.c
    src/MetroidPrime/ScriptObjects/Carve8015DF08.c
    src/MetroidPrime/Player/Carve80168498.c
    src/MetroidPrime/Player/Carve8016BDE4.c
    src/MetroidPrime/Carve80171DD4.c
    src/MetroidPrime/Carve80179E08.c
    src/MetroidPrime/Carve8018C4A8.c
    src/MetroidPrime/Carve8018EF50.c
    src/MetroidPrime/Carve80192D74.c
    src/MetroidPrime/Carve80193C30.c
    src/MetroidPrime/Carve80193C54.c
    src/MetroidPrime/Carve80193C7C.c
    src/MetroidPrime/Carve80193D9C.c
    src/MetroidPrime/Carve80193DB8.c
    src/MetroidPrime/Carve80193E04.c
    src/MetroidPrime/Carve80194BF0.c
    src/MetroidPrime/Carve801956B4.c
    src/MetroidPrime/Carve80195FEC.c
    src/MetroidPrime/Carve80196530.c
    src/MetroidPrime/Carve801997B0.c
    src/MetroidPrime/Carve8019CE6C.c
    src/MetroidPrime/Carve8019E368.c
    src/MetroidPrime/Carve801A7C18.c
    src/MetroidPrime/Carve801ABD0C.c
    src/MetroidPrime/Carve801ADD2C.c
    src/MetroidPrime/Carve801AE9C4.c
    src/MetroidPrime/Carve801AEE0C.c
    src/MetroidPrime/Carve801B02DC.c
    src/MetroidPrime/Carve801B0624.c
    src/MetroidPrime/Carve801B1A6C.c
    src/MetroidPrime/Carve801B2E38.c
    src/MetroidPrime/Carve801B3B44.c
    src/MetroidPrime/Carve801B5694.c
    src/MetroidPrime/Carve801B9420.c
    src/MetroidPrime/Carve801B94B4.c
    src/MetroidPrime/Carve801BC900.c
    src/MetroidPrime/Carve801C128C.c
    src/MetroidPrime/Carve801C13F4.c
    src/MetroidPrime/Carve801C3670.c
    src/MetroidPrime/Carve801C36A8.c
    src/MetroidPrime/Carve801C3718.c
    src/MetroidPrime/Carve801C6DF0.c
    src/MetroidPrime/Player/Carve801D3B88.c
    src/MetroidPrime/Weapons/Carve801D5E9C.c
    src/MetroidPrime/Weapons/Carve801D688C.c
    src/MetroidPrime/Weapons/Carve801D6920.c
    src/MetroidPrime/Weapons/Carve801D6930.c
    src/MetroidPrime/ScriptObjects/Carve801E2AE8.c
    src/MetroidPrime/ScriptObjects/Carve801E3E34.c
    src/MetroidPrime/ScriptObjects/Carve801E8AEC.c
    src/MetroidPrime/Carve801F3690.c
    src/MetroidPrime/Carve801F36DC.c
    src/MetroidPrime/Carve801F7AC8.c
    src/MetroidPrime/ScriptObjects/Carve801FEEF0.c
    src/MetroidPrime/ScriptObjects/Carve801FF4A4.c
    src/MetroidPrime/ScriptLoader/Carve80201418.c
    src/MetroidPrime/ScriptObjects/Carve80212278.c
    src/MetroidPrime/ScriptObjects/Carve802126B0.c
    src/MetroidPrime/ScriptObjects/Carve80212944.c
    src/MetroidPrime/ScriptObjects/Carve802129A4.c
    src/MetroidPrime/ScriptObjects/Carve80212A24.c
    src/MetroidPrime/ScriptLoader/Carve80226B3C.c
    src/MetroidPrime/ScriptLoader/Carve80226B60.c
    src/MetroidPrime/ScriptLoader/Carve80226C78.c
    src/MetroidPrime/ScriptLoader/Carve80226C9C.c
    src/MetroidPrime/ScriptLoader/Carve80226CB8.c
    src/MetroidPrime/ScriptLoader/Carve80226CC8.c
    src/MetroidPrime/ScriptLoader/Carve80229410.c
    src/MetroidPrime/ScriptLoader/Carve80229568.c
    src/MetroidPrime/ScriptLoader/Carve80229BBC.c
    src/MetroidPrime/ScriptLoader/Carve8022A3F4.c
    src/MetroidPrime/ScriptLoader/Carve8022D758.c
    src/MetroidPrime/ScriptLoader/Carve8022DA40.c
    src/MetroidPrime/Carve802476D8.c
    src/MetroidPrime/Carve8026040C.c
    src/MetroidPrime/Carve8026F624.c
    src/MetroidPrime/Carve802740B0.c
    src/MetroidPrime/Carve80275F24.c
    src/MetroidPrime/Carve80276568.c
    src/MetroidPrime/Carve80277090.c
    src/MetroidPrime/Carve80278568.c
    src/MetroidPrime/Carve80278C74.c
    src/MetroidPrime/Carve80279250.c
    src/MetroidPrime/Carve8027A1C4.c
    src/MetroidPrime/Carve8027A53C.c
    src/MetroidPrime/Carve80280338.c
    src/Kyoto/Basics/Carve8028C058.c
    src/Kyoto/Basics/Carve8028EFD4.c
    src/Kyoto/Basics/Carve802905AC.c
    src/Kyoto/Basics/Carve802940D0.c
    src/Kyoto/Basics/Carve80294814.c
    src/Kyoto/Basics/Carve80294F98.c
    src/Kyoto/Basics/Carve80295584.c
    src/Kyoto/Basics/Carve80295B98.c
    src/Kyoto/Basics/Carve8029624C.c
    src/Kyoto/Basics/Carve80296468.c
    src/Kyoto/Basics/Carve80296D08.c
    src/Kyoto/Basics/Carve80296D94.c
    src/Kyoto/Basics/Carve8029AB1C.c
    src/Kyoto/Basics/Carve8029BC5C.c
    src/MetroidPrime/Carve8029F084.c
    src/MetroidPrime/Carve8029FA58.c
    src/MetroidPrime/Carve802A32D8.c
    src/MetroidPrime/Carve802A5E20.c
    src/MetroidPrime/Carve802A7C78.c
    src/MetroidPrime/Carve802AE9BC.c
    src/Kyoto/Animation/Carve802B2088.c
    src/Kyoto/Animation/Carve802B2568.c
    src/Kyoto/Carve802B3B7C.c
    src/Kyoto/Carve802B4A04.c
    src/Kyoto/Carve802B4D30.c
    src/Kyoto/Carve802B9D50.c
    src/Kyoto/Graphics/Carve802BE7E4.c
    src/Kyoto/PVS/Carve802E8304.c
    src/Kyoto/PVS/Carve802F2268.c
    src/Kyoto/PVS/Carve802F27B0.c
    src/Kyoto/PVS/Carve802F358C.c
    src/Kyoto/PVS/Carve802F3D3C.c
    src/Kyoto/PVS/Carve802F74A4.c
    src/Kyoto/Carve80300998.c
    src/Kyoto/Graphics/Carve8031B6EC.c
    src/Kyoto/Graphics/Carve80310E8C.c
    src/Kyoto/Math/Carve8032C144.c
    src/Kyoto/Math/Carve8032E444.c
    src/Kyoto/Math/Carve803359F4.c
    src/Kyoto/Math/Carve80335A14.c
    src/Kyoto/Math/Carve80335A3C.c
    src/Kyoto/Math/Carve80335A5C.c
    src/Kyoto/Math/Carve80335A8C.c
    src/Kyoto/Math/Carve80335AB0.c
    src/Kyoto/Math/Carve80335AE0.c
    src/Kyoto/Math/Carve80335B10.c
    src/Kyoto/Math/Carve80335B38.c
    src/Kyoto/Math/Carve80335B58.c
    src/Kyoto/Math/Carve80337198.c
    src/Kyoto/Math/Carve803371A4.c
    src/Kyoto/Math/Carve803371F4.c
    src/Kyoto/Math/Carve80339D1C.c
    src/Kyoto/Math/Carve8033BE1C.c
    src/Kyoto/Math/Carve8033F2CC.c
    src/Kyoto/Math/Carve803414FC.c
    src/Dolphin/Carve8038A7DC.c
    src/Dolphin/Carve80397B78.c
    src/Dolphin/Carve8039E164.c
    src/Dolphin/Carve803A1B28.c
    src/Dolphin/Carve803A1D2C.c
    src/Dolphin/Carve803A2324.c
    src/Dolphin/Carve803A2FD0.c

    # Ten CCubeRenderer methods as one unit: GetFPS, the eight SetBlendMode_* wrappers and
    # SetDepthReadWrite, one contiguous run at 0x8026E7F0..0x8026E9B8. configure.py claims it,
    # so it is Matching and counts as linked in both worlds.
    src/MetaRender/Carve8026E7F0.cpp
    src/MetaRender/Carve8026EC54.cpp
    src/MetaRender/Carve8026ECDC.cpp
    src/MetaRender/Carve8026EF24.cpp
    # AllocateRenderer, retail 0x8026EF54. configure.py claims it, so it is Matching and its
    # body is the definition the port's step 12 needs: it is what makes gpRender non-null.
    src/MetaRender/Carve8026EF54.cpp
    # CCubeRenderer, the other three of its four retail functions. BeginScene (0x8026FBFC,
    # 0x180 = 384 B) is byte-exact in .text and fn_80272958 (0x80272958, 0x30 = 48 B) is
    # byte-exact outright, so both count as `matched`; **only Carve80272958.c is `Matching`**,
    # and therefore only it counts as `linked`. BeginScene CANNOT be `Matching`: mwldeppc
    # attributes 20 bytes of .sdata2 to its object that retail does not have, which costs 32
    # bytes of main.dol and 43 broken REL hashes. Measured, in docs/HANDOFF.md and
    # docs/RUNNING_THE_DECOMP.md - do not promote it on the strength of its 100.00% fuzzy.
    # Carve80270848.cpp is ~CCubeRenderer, the class's key function, and a key function is the
    # only thing that emits a vtable: without this line nothing anywhere defines
    # `vtable for CCubeRenderer` and every `gpRender->` virtual on the host is a jump to 0.
    # Carve80271238.cpp is the constructor; it is `NonMatching` (98.92% on a proven
    # `@stringBase0` wall) so it adds 0 to both counts, but it is what puts a vtable pointer
    # into the 1376 bytes the constructor hands back.
    src/MetaRender/Carve8026FBFC.cpp
    # CCubeRenderer::EndScene, retail 0x8026FB80, 0x7C = 124 B - the other half of the frame's
    # begin/end pair, vtable slot 36, 0x7C bytes BEFORE BeginScene (slot 35) and abutting its
    # claim with no overlap. Pure GX teardown: no pixels, nothing drawn. configure.py claims
    # it, so whether it counts as `linked` is decided by flip_test, not by its percentage.
    src/MetaRender/Carve8026FB80.cpp
    src/MetaRender/Carve8026FDEC.cpp
    src/MetaRender/Carve80270848.cpp
    src/MetaRender/Carve80271238.cpp
    src/MetaRender/Carve80272958.c
    src/MetaRender/PortCCubeRenderer.cpp
    src/Kyoto/Graphics/CGraphicsPalettePortStub.cpp
    src/Kyoto/Graphics/Carve802C4248.cpp
    src/Kyoto/Graphics/CGraphicsHostGlobals.cpp
    # CGraphics' frame bracket (BeginScene/EndScene, SwapBuffers, the VI callbacks) and the
    # Aurora frame they own. Port-only like CGraphicsHostGlobals.cpp.
    src/Kyoto/Graphics/CGraphicsHostScene.cpp
    # Retail's CGraphics bring-up on the host: CGraphicsSys's ctor/dtor and
    # Startup -> ConfigureVideo -> InitGraphicsVariables -> ConfigureFrameBuffer ->
    # InitGraphicsDefaults -> SetDefaultVtxAttrFmt, copied from upstream's
    # src/Kyoto/Graphics/DolphinCGraphics.cpp, which is EXCLUDED above (4 compile errors that
    # are not local to it) and so cannot be listed. Port-only, like CGraphicsHostGlobals.cpp:
    # configure.py does not claim it, so the DOL objects are byte-identical with or without it.
    # This is what makes CGraphicsSys constructible in platform/main.cpp before InvokeCMain, and
    # it is what fills mRenderModeObj__9CGraphics - without it fbWidth is 0 and CGraphicsHostScene
    # skips the fade quad and GXCopyDisp.
    src/Kyoto/Graphics/CGraphicsHostStartup.cpp
    # retail fn_8032F6EC (skinned-model workspace set-up), hand-written from the asm; port-only.
    src/Kyoto/Graphics/CGraphicsHostWorkspace.cpp
    # CGraphics::SetViewPointMatrix (retail 0x802C2534, 0xE0 = 224 B), `NonMatching` at 99.11% -
    # 10 wrong bytes, all float register fields, and the file's header records the measurement and
    # the two spells that do not rescue it. Not listed for the "NonMatching is not in the DOL link"
    # reason any more: CGraphicsHostStartup.cpp above calls it from
    # CGraphics::SetIdentityViewPointMatrix (retail's own body is exactly that one call), so
    # listing it closes a symbol the port's boot path now asks for. Its three guest dependencies
    # have PC-side storage already: mViewMatrix__9CGraphics is aliased onto CGraphics::mViewMatrix
    # in PortGlobals.cpp, lbl_804172A0/lbl_804172D0 and fn_802C2614 are Carve802C2614.c below, and
    # lbl_8041E508 is defined in CGraphicsHostStartup.cpp.
    src/Kyoto/Graphics/Carve802C2534.cpp
    src/Kyoto/Graphics/CTexturePortStub.cpp
    src/Kyoto/Graphics/CModelPortStub.cpp
    src/Kyoto/Graphics/Carve802BEC1C.cpp
    # CGraphics' SetScreenPosition and SetUseVideoFilter. Both Matching at 100.00%, and
    # both close a symbol the port's link already asks for. Their guest globals already
    # have PC-side definitions - lbl_804199E0/E4/E8 in src/MetroidPrime/PortGlobals.cpp
    # and lbl_80418AFF / mRenderModeObj__9CGraphics in
    # src/Kyoto/Graphics/CGraphicsHostGlobals.cpp - and VIConfigure, VIFlush and
    # GXSetCopyFilter come from Aurora (aurora_vi, aurora_gx).
    src/Kyoto/Graphics/Carve802BE8F0.cpp
    src/Kyoto/Graphics/Carve802BEC24.cpp
    # CGraphics::SetModelMatrix (retail SetModelMatrix__9CGraphicsFRC12CTransform4f, 0x802C24AC,
    # 96 B) and fn_802C2614 (0x802C2614, 156 B), its shared tail with SetViewPointMatrix. Both
    # configure.py units, so both are Matching and count as linked in both worlds. The second
    # exists only because the first relocates against it: SetModelMatrix was Matching and
    # unlisted until fn_802C2614 gave `Carve802C24AC.cpp` a definition for its one unresolved
    # reference, and this pair is what closes CGraphics::SetModelMatrix in the port's link.
    # The four guest Mtx it composes into (0x804172A0/0x804172D0/0x80417300/0x80417330) and
    # the 0x80418AFC latch have their PC-side storage in CGraphicsHostGlobals.cpp, which is
    # listed below and is port-only, so the DOL objects are byte-identical with or without it.
    # PSMTXCopy/PSMTXConcat/PSMTXInvXpose and GXLoadPosMtxImm/GXLoadNrmMtxImm come from Aurora
    # (aurora::mtx, aurora::gx).
    src/Kyoto/Graphics/Carve802C24AC.cpp
    src/Kyoto/Graphics/Carve802C2614.c
    # CTweakPlayer's five accessors, as two units. configure.py claims these two,
    # so unlike PortGlobals.cpp they are Matching and count as linked.
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
    # Not a configure.py unit, on the same grounds: it is the host's definition of
    # `StreamNewGameState__5CMainFR12CInputStreami`, retail 0x800053B8. `CMainFlowDtor.cpp` calls
    # that exact name through `extern "C"` (its header's point 4 says why the call site cannot be
    # a C++ member call), and no host compiler mangles a member function to an MWCC name, so the
    # body could not be reached by the port however good `main.cpp`'s copy of it was. It costs one
    # named hole, `fn_80144140` (the `CGameState` stream constructor, whose unit is in
    # `tools/check_files_cmake.py`'s EXCLUDED list), against the one it closes, so the port's
    # unique undefined count does not move. See the file's own header for the block map.
    src/MetroidPrime/PortStreamNewGameState.cpp
    # Not a configure.py unit, on the same grounds: it is the host stand-in for Tweaks.rel's
    # REL_CreateTweakGlobals, the only writer of gpTweakPlayerA (0x80418F44), which
    # CGameArchitectureSupport's constructor dereferences at 0x80007F38 with no null test.
    # See the file's header and docs/research/tweak_globals.md.
    src/MetroidPrime/PortTweakGlobals.cpp
    # Not a configure.py unit, on the same grounds: it is the object pool's stand-in
    # registry - the name -> SObjectTag table CSimplePool::GetObj(const char*) consults
    # before the factory (which cannot resolve a name, no pak being loaded), the objects
    # that stand in for the tags it knows, and retail's own 0x1C0-byte .rodata string pool
    # at 0x803A56C0 that those names are offsets into. See the file's header.
    src/MetroidPrime/PortPoolStandIns.cpp
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
    src/MetroidPrime/ScriptLoader/Tweaks.cpp
    # Port-only: `LoadTypedefEditorProperties` (retail 0x8023EF3C, 0x140) and its one callee
    # `LoadTypedefSLdrTransform` (retail 0x8023F8CC, 0x9C, unnamed in symbols.txt because
    # nothing else in the DOL calls it). 0x8023EF3C is in an unclaimed `.text` range - the
    # nearest splits are `RubiksPuzzle.cpp` ending 0x802399F4 and `ScriptLoader.cpp` starting
    # 0x80242894 - so there is no unit to claim it and this is not a configure.py entry. It
    # closed `_Z27LoadTypedefEditorPropertiesR20SLdrEditorPropertiesR12CInputStream`, which
    # ten port objects referenced. See the file's own header for the property-by-property
    # reading of retail's body.
    src/MetroidPrime/ScriptLoader/Structs/SLdrEditorProperties_Load.cpp
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
    src/MetroidPrime/Tweaks/CTweakPlayer.cpp
    src/MetroidPrime/Tweaks/CTweakPlayerGun.cpp
    src/MetroidPrime/Tweaks/CTweakTargeting.cpp
    src/MetroidPrime/Tweaks/CTweakGuiColors.cpp
    src/MetroidPrime/Tweaks/CTweakGui.cpp
    src/MetroidPrime/Weapons/CPowerBeam.cpp
    src/MetroidPrime/Weapons/CGunWeaponTouch.cpp
    # Upstream's main.cpp is a skeleton (empty AddPaksAndFactories, RsMain, ...) that overlaps
    # mainMid.cpp/mainTail.cpp, so the port links the pre-merge lower third as mainHead.cpp.
    src/MetroidPrime/mainHead.cpp
    # `main.cpp`'s range is cut in three for the DOL and both halves are listed here, because the
    # port needs them: narrowing `main.cpp`'s claim ALONE drops `matched` 3974 -> 3965, the ten
    # functions above `CMain::FillInAssetIDs` stop being claimed and become
    # `main/auto_03_80006B80_text` at 0%. `mainMid.cpp` is the upper third
    # (0x80006B80-0x8000848C) and keeps `CGameArchitectureSupport`'s constructor - the one the boot
    # probe executes at step 17 - plus `AddPaksAndFactories`.
    src/MetroidPrime/mainMid.cpp
    # `CMain::MemoryCardInitializePump` (mainMid.cpp) constructs and pumps the memory card; without
    # it `gpMemoryCard` stays null and the boot never leaves `CPreFrontEnd`.
    src/MetroidPrime/CMemoryCard.cpp
    src/MetroidPrime/CWorldSaveGameInfo.cpp
    # Upstream's whole CGameState TU, on upstream's layout (the host dropped its opaque one on
    # 2026-10-01). It replaces the CGameStateCtor / PlayerLoop / SlotDefaults / SysOptsPutTo carves
    # and the eight other carves that duplicated its bodies; none is compiled any more.
    src/MetroidPrime/Player/CGameState.cpp
    # The memory card builds a CDummyWorld per MLVL (CSaveWorldIntermediate), whose areas are
    # CDummyGameArea: both whole upstream TUs, replacing CWorldTouchSky and the three CGameArea carves.
    src/MetroidPrime/CWorld.cpp
    src/MetroidPrime/CGameArea.cpp
    # `CMain::FillInAssetIDs` (0x80006B38, 72 B) carved out of `main.cpp`'s range so it could be
    # promoted: it was 100.00% and still `NonMatching` because it shared a claim with 49 other
    # functions. Listed because `MetroidPrime/PortPoolStandIns.cpp` reaches the resource chain
    # through it.
    src/MetroidPrime/CMainFillInAssetIDs.cpp
    # `main.cpp`'s upper half, split off for the DOL (see that file's header and
    # `MetroidPrime/CGameGlobalObjectsCtor.cpp`'s). It has to be listed here for the same reason
    # the split happened: `InvokeCMain`, `CMain::~CMain`, `__sys_free`,
    # `CGameArchitectureSupport::~CGameArchitectureSupport`, `~CPlayerState` and six more left
    # `main.cpp` with the range, and the port needs them.
    src/MetroidPrime/mainTail.cpp
    # `CMain::ShutdownSubsystems` (0x80008570, 272 B) carved out of mainTail.cpp's range so
    # that unit can start at 0x80008680 - a `Matching` carve for `CMain::InitializeSubsystems`
    # needs the whole 0x80008570..0x800087DC and one unit may not claim two discontiguous ranges
    # in a section. The port links this one too: it is the boot path's teardown.
    src/MetroidPrime/CMainShutdownSubsystems.cpp
    # Member constructors CGameGlobalObjects' constructor calls (fn_8016C230, fn_801F0A44, both
    # Matching), and the CGameState-subtree units that are net zero on the port's link now that
    # SGameStateBlock's rstl::vector<unsigned char> operations (CGameStateBlock*.cpp) and the
    # subtree's guest constants (PortGlobals.cpp) exist. The constructor itself, the builder at
    # +0x108 and the rest of the subtree are not listed: tools/check_files_cmake.py has the
    # measurements, docs/research/cgameglobalobjects_ctor.md the accounting.
    src/MetroidPrime/CInGameTweakManagerCtor.cpp
    src/MetroidPrime/CGameGlobalObjectsTailCtor.cpp
    # The integration (docs/research/cgameglobalobjects_ctor.md): CGameGlobalObjects' constructor,
    # the builder at +0x108, and the CGameState default-construction chain it allocates.
    src/MetroidPrime/CGameGlobalObjectsCtor.cpp
    src/MetroidPrime/Factories/CCharacterFactoryBuilder.cpp
    src/MetroidPrime/Player/CGameStateCardOptsCtor.cpp
    src/MetroidPrime/Player/CPersistentOptionsCtor.cpp
    src/MetroidPrime/Player/CGameStateMemcardCtor.cpp
    src/MetroidPrime/Player/SGameStateMemcardReset.cpp
    src/MetroidPrime/Player/SGameStateMemcardFill.cpp
    # fn_80009AC0 (NonMatching 85.20%), the chain's last memcard fill; its byte lbl_80417D92 is in
    # PortGlobals.cpp. fn_8000934C is rstl::rc_ptr<CPlayerState>::ReleaseData, not a configure.py
    # unit; the retail-named deleting destructor it calls is in PortGlobals.cpp too.
    src/MetroidPrime/Player/SGameStateMemcardBufFill.cpp
    src/MetroidPrime/Player/CPlayerStateRefRelease.cpp
    src/MetroidPrime/Player/CGameStateBlockCopyCtor.cpp
    src/MetroidPrime/Player/CGameStateBlockConstruct.cpp
    src/MetroidPrime/Player/CGameStateBlockReserve.cpp
    src/MetroidPrime/Player/CGameStateBlockDtor.cpp
    src/REL/REL_Setup.cpp
    src/rstl/RstlExtras.cpp
    src/rstl/rc_ptr_copy.cpp
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
    src/MetroidPrime/ScriptObjects/EyeBallAccessors.cpp
    src/MetroidPrime/ScriptObjects/WispTentacleAccessors.cpp
    src/MetroidPrime/ScriptObjects/WallWalkerAccessors.cpp
    src/MetroidPrime/ScriptObjects/StoneToadAccessors.cpp
    src/MetroidPrime/ScriptObjects/SporbAccessors.cpp
    src/MetroidPrime/ScriptObjects/SpankWeedAccessors.cpp
    src/MetroidPrime/ScriptObjects/ShredderAccessors.cpp
    src/MetroidPrime/ScriptObjects/CIngSnatchingSwarmGenAccessors.cpp
    src/MetroidPrime/ScriptObjects/RipperAccessors.cpp
    src/MetroidPrime/ScriptObjects/CRipperForwarders.cpp
    src/MetroidPrime/ScriptObjects/CSplitterRel.cpp
    src/MetroidPrime/ScriptObjects/PuddleSporeAccessors.cpp
    src/MetroidPrime/ScriptObjects/OctapedeSegmentAccessors.cpp
    src/MetroidPrime/ScriptObjects/KrocussAccessors.cpp
    src/MetroidPrime/ScriptObjects/KraleeAccessors.cpp
    src/MetroidPrime/ScriptObjects/IngSpiderballGuardianAccessors.cpp
    src/MetroidPrime/ScriptObjects/GunTurretAccessors.cpp
    src/MetroidPrime/ScriptObjects/GlowbugAccessors.cpp
    src/MetroidPrime/ScriptObjects/EmperorIngStage2TentacleAccessors.cpp
    src/MetroidPrime/ScriptObjects/EmperorIngStage1Accessors.cpp
    # Module 18's head, .text 0x0..0xF8 - fourteen functions. Listed here, unlike the other module
    # *head* sources (CMysteryFlyerRel.cpp and the rest are in check_files_cmake.py's EXCLUDED
    # list), because it defines no RELMain/RELExit: twelve of its functions read raw offsets and
    # DOL globals and nothing else, and `fn_18_8` is behind the same `#ifdef __MWERKS__` guard
    # KrocussAccessors.cpp uses, so the port's undefined count stays at 259.
    src/MetroidPrime/ScriptObjects/CEmperorIngStage3Rel.cpp
    # Module 25's accessor block, .text 0x2544..0x2584 - eight functions in two Matching units,
    # 0x2544..0x255C and 0x256C..0x2584, with the two float getters at 0x255C..0x256C left to dtk
    # because they are not in its FORCEACTIVE list and are dead-stripped if we claim them. Listed for
    # the same reason as CEmperorIngStage3Rel.cpp above and not the same reason as CGeomBlobV2Rel.cpp
    # (which is deliberately out): these two define no RELMain/RELExit and relocate against nothing
    # outside themselves - no DOL global, no module callee - so the port's undefined count is
    # unchanged. Their sibling CGeomBlobV2Rel.cpp calls fn_25_2490 and fn_80229EAC, which the port
    # cannot link, and is left out.
    src/MetroidPrime/ScriptObjects/CGeomBlobV2Accessors.cpp
    src/MetroidPrime/ScriptObjects/CGeomBlobV2AccessorsTail.cpp
    # Module 43's destructor block, .text 0x1F38..0x1FB8 - three functions, 0x1F38, 0x1F70 and
    # 0x1F90. Listed for the same reason as the two CGeomBlobV2*Accessors entries above: it
    # defines no RELMain/RELExit, so it does not collide in a flat link, and its only relocation
    # outside itself - the call to fn_43_1FB8, which begins exactly where the split ends and is
    # 0x13C bytes of the module's own unclaimed code - is behind the same `#ifdef __MWERKS__`
    # guard CEmperorIngStage3Rel.cpp uses for fn_18_DAEC. The rest reads raw offsets and calls
    # into this unit, so the port's undefined count is unchanged.
    src/MetroidPrime/ScriptObjects/CMetareeSwarmDes.cpp
    src/MetroidPrime/ScriptObjects/DigitalGuardianAccessors.cpp
    src/MetroidPrime/ScriptObjects/AtomicBetaAccessors.cpp
    src/MetroidPrime/ScriptObjects/ScriptGuiSetup.cpp
    # Module 50's cross product, .text 0xD80..0xDC0 - one function. Listed for the same reason as
    # CEmperorIngStage3Rel.cpp and the CGeomBlobV2*Accessors entries above, and *not* for the
    # module-entry reason the other PirateRagDoll unit gets: CPirateRagDollCross.cpp defines
    # neither RELMain nor RELExit, so tools/check_files_cmake.py's MODULE_ENTRY exemption does
    # not cover it, and its sibling CPirateRagDollRel.cpp - which does define them, and calls
    # fn_50_1938 and fn_80227538 the port cannot link - stays out. Safe to list because
    # `powerpc-eabi-nm -u` on its object prints nothing: one self-contained float function over
    # raw offsets, zero externals, so the port's undefined count is unchanged.
    src/MetroidPrime/ScriptObjects/CPirateRagDollCross.cpp
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
    src/Kyoto/Animation/CSegId.cpp
    src/Kyoto/Animation/CSegIdList.cpp
    src/Kyoto/Animation/CSkinnedModel.cpp
    src/Kyoto/Graphics/CCubeSurface.cpp
    src/Kyoto/CDependencyGroup.cpp
    src/Kyoto/Text/CFontImageDef.cpp
    src/Kyoto/CPakFile.cpp
    src/Kyoto/CFactoryMgr.cpp
    # Upstream's CResFactory and CSimplePool TUs (configure.py NonMatching) replace the port's
    # former fragments of both classes and of CFactoryMgr (deleted 2026-09-28). The
    # port's stand-in objects hook in through `#ifdef TARGET_PC` hunks in CSimplePool.cpp and
    # CResFactory.hpp; see `port::pool` in src/MetroidPrime/PortPoolStandIns.cpp.
    src/Kyoto/CResFactory.cpp
    src/Kyoto/CSimplePool.cpp
    # Port-only, and not a configure.py unit: `fn_803096C4`, the constructor of the four bytes at
    # `CGameGlobalObjects`+0x00 - a member holding nothing but a vptr, whose body is a one-shot
    # `CARDInit`. It kept retail's own name, which is the arrangement a future `Matching` unit
    # would need, since `main.cpp` is NonMatching and its object is what the DOL links.
    src/MetroidPrime/CGameGlobalObjectsPad0Ctor.cpp
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
    src/MetroidPrime/ScriptObjects/CDarkSamus.cpp
    src/MetroidPrime/ScriptObjects/CDarkSamusFlags.cpp
    src/MetroidPrime/ScriptObjects/CDarkSamusFloatParameter.cpp
    src/MetroidPrime/ScriptObjects/CDarkSamusMembers.cpp
    src/MetroidPrime/ScriptObjects/CDarkSamusState.cpp
    src/MetroidPrime/ScriptObjects/CScriptCoin.cpp
    src/MetroidPrime/ScriptObjects/CScriptCoinTouchBounds.cpp
    # Upstream (PrimeDecomp/echoes) TUs, listed in the 2026-09-28 merge: upstream's fuller
    # CActor/CPlayer/CPatterned/CPlayerGun bodies reference these, and each one defines symbols
    # that would otherwise be new port-link undefineds. Each probes clean on the host.
    src/Kyoto/Animation/CAdditiveAnimPlayback.cpp
    src/Kyoto/Animation/CCharAnimMemoryMetrics.cpp
    src/Kyoto/Animation/CCharLayoutInfo.cpp
    src/Kyoto/Animation/CHierarchyPoseBuilder.cpp
    src/Kyoto/Animation/CParticlePOINode.cpp
    src/Kyoto/Animation/CPASAnimParm.cpp
    src/Kyoto/Animation/CPOINode.cpp
    src/Kyoto/Animation/CPoseAsTransforms_Linear.cpp
    src/Kyoto/Animation/CSoundPOINode.cpp
    src/Kyoto/Audio/CSfxHandle.cpp
    src/Kyoto/Audio/CSfxManager.cpp
    src/Kyoto/Audio/CSfxPitchBend.cpp
    src/Kyoto/Audio/CStreamAudioManager.cpp
    src/Kyoto/Particles/CDeferredParticleEffect.cpp
    src/Kyoto/Particles/CParticleData.cpp
    src/MetroidPrime/BodyState/CBodyStateCmdMgr.cpp
    src/MetroidPrime/CActorModelParticles.cpp
    src/MetroidPrime/Cameras/CCameraManager.cpp
    src/MetroidPrime/CAnimData.cpp
    src/MetroidPrime/CEnvFxManager.cpp
    src/MetroidPrime/CModelData.cpp
    src/MetroidPrime/CObjectList.cpp
    src/MetroidPrime/CParticleDatabase.cpp
    src/MetroidPrime/CRainSplashGenerator.cpp
    src/MetroidPrime/CSimpleShadow.cpp
    src/MetroidPrime/CSortedLists.cpp
    src/MetroidPrime/CSteeringBehaviors.cpp
    src/MetroidPrime/CTargetReticles.cpp
    src/MetroidPrime/Enemies/CPatternedAiFunctions.cpp
    src/MetroidPrime/Player/CFidget.cpp
    src/MetroidPrime/Player/CMorphBall.cpp
    src/MetroidPrime/Player/CMorphBallShadow.cpp
    src/MetroidPrime/Player/CPlayerDynamics.cpp
    src/MetroidPrime/Player/CPlayerGunBase.cpp
    src/MetroidPrime/ScriptObjects/CScriptForgottenObject.cpp
    src/MetroidPrime/Weapons/CGunWeapon.cpp
    # `NWeaponTypes::lock_tokens` (retail 0x8018A748) and `fn_8018A6EC` (0x8018A6EC), the two
    # `Lock`/`Unlock`-per-element loops over an `rstl::vector<CToken>`. Both sit in the unclaimed
    # gap between `CDamageInfo.cpp` and `CMorphBallShadow.cpp`; `CGunWeapon::LockTokens` and
    # `UnlockTokens` call them, so without this the port's link reports 2 more undefined than
    # before. One function pair per file, as `CGameAreaSetAreaAttributes.cpp` does.
    src/MetroidPrime/Weapons/NWeaponTypesTokens.cpp
    # --- upstream (PrimeDecomp/echoes) Kyoto units, wave 2 (2026-09-28): 37 units of the 109
    # configure.py declares under src/Kyoto plus src/Dolphin/os that the port can carry at no
    # cost. Each probes clean on the host, none of them is a strong duplicate of anything
    # already in this list, and the set as a whole is **net 0** on the port's link: it opens no
    # symbol nothing defines and closes none, so `tools/link_check.sh` still reports 318.
    # It is a set and not 37 independent decisions because the units share: CAnimTreeTimeScale
    # on its own opens 16 symbols, every one of which a sibling below defines, and the same is
    # true of CAnimTreeAnimReaderContainer, CAnimTreeSingleChild, CSequenceHelper and
    # CAnimSourceReaderBase. Measured by nm over the port's own link inputs, which reproduces
    # link_check.sh's recorded 318 exactly; the 72 units of the same 109 that are not here are
    # in tools/check_files_cmake.py's EXCLUDED list, each with its own measured reason.
    src/Kyoto/Alloc/CCallStackDolphin.cpp
    src/Kyoto/Animation/CAnimSourceReaderBase.cpp
    src/Kyoto/Animation/CAnimTreeAnimReaderContainer.cpp
    src/Kyoto/Animation/CAnimTreeNode.cpp
    src/Kyoto/Animation/CAnimTreeSingleChild.cpp
    src/Kyoto/Animation/CAnimTreeTimeScale.cpp
    src/Kyoto/Animation/CHalfTransition.cpp
    src/Kyoto/Animation/CInt32POINode.cpp
    src/Kyoto/Animation/CMetaTransSnap.cpp
    src/Kyoto/Animation/CPASAnimInfo.cpp
    src/Kyoto/Animation/CPASAnimState.cpp
    src/Kyoto/Animation/CPASParmInfo.cpp
    src/Kyoto/Animation/CPrimitive.cpp
    src/Kyoto/Animation/CSequenceHelper.cpp
    src/Kyoto/Animation/CTransitionManager.cpp
    src/Kyoto/Animation/CTreeUtils.cpp
    src/Kyoto/Animation/IAnimReader.cpp
    src/Kyoto/Animation/IMetaAnim.cpp
    src/Kyoto/Basics/CSWDataDolphin.cpp
    src/Kyoto/Graphics/CLight.cpp
    src/Kyoto/Math/CCylinder.cpp
    src/Kyoto/Math/CLine.cpp
    src/Kyoto/Particles/CEffectComponent.cpp
    src/Kyoto/Particles/CElectricDescription.cpp
    src/Kyoto/Particles/CEmitterElement.cpp
    src/Kyoto/Particles/CGenDescription.cpp
    src/Kyoto/Particles/CModVectorElement.cpp
    src/Kyoto/Particles/CParticleGlobals.cpp
    src/Kyoto/Particles/CParticleSpawnRandom.cpp
    src/Kyoto/Particles/CParticleSpawnSystem.cpp
    src/Kyoto/Particles/CSpawnSystemKeyframeData.cpp
    src/Kyoto/Particles/CSwooshDescription.cpp
    src/Kyoto/Particles/CUVElement.cpp
    src/Kyoto/Text/CBlockInstruction.cpp
    src/Kyoto/Text/CDrawStringOptions.cpp
    src/Kyoto/Text/CLineExtraSpaceInstruction.cpp
    src/Kyoto/Text/CLineInstruction.cpp
    src/Kyoto/Text/CLineSpacingInstruction.cpp
    # --- upstream (PrimeDecomp/echoes) game-wave units, 2026-09-28: 37 of the 129 configure.py
    # objects the merge brought under src/MetroidPrime, src/Collision, src/GuiSys,
    # src/WorldFormat, src/Weapons and src/MetaRender that the port can carry. Each probes
    # clean on the host with the port's own mp_game flags, none is a duplicate of anything
    # else in this list, and **the set as a whole is net -4 on the port's link**: 318 -> 314.
    # It frees four of the undefined symbols rather than spending any.
    #
    # It is a set and not 37 independent decisions, because the units share: measured on their
    # own, CMainFlow.cpp opens 16 symbols and closes none, four of which
    # (`CArchMsgParmInt32::~CArchMsgParmInt32`, `vtable for CArchMsgParmInt32` and two more)
    # CArchMsgParmInt32.cpp and CArchMsgParmReal32.cpp below define, and CActorLights.cpp
    # closes five. Method: nm over the port's real link inputs, which reproduces
    # tools/link_check.sh's recorded 318 symbol for symbol, iterated to a fixed point because
    # 37 separate builds is not a measurement anyone can afford; the 92 units of the same 129
    # that are not here are in tools/check_files_cmake.py's EXCLUDED list, each with its own
    # measured reason.
    #
    # Two of the 37 replace host sources the port carried before upstream had the real unit,
    # and those host sources are no longer listed: CAxisAngle.cpp supersedes
    # CAxisAngleGetVector.cpp and five PortLinkStubs.cpp stubs, and CIOWin.cpp supersedes the
    # three CIOWin* carves. CMainFlow.cpp would supersede the four CMainFlow* carves the same
    # way and is still out, because with those carves gone it costs +16 on the port's link -
    # see the comment at CMainFlowCtor.cpp's place above. See also the comments at each of the
    # dropped files' old places above.
    src/Collision/CMRay.cpp
    src/MetroidPrime/CActorField25.cpp
    src/MetroidPrime/CActorLights.cpp
    src/MetroidPrime/CAnimationDatabaseGame.cpp
    src/MetroidPrime/CArchMsgParmControllerStatus.cpp
    src/MetroidPrime/CArchMsgParmInt32.cpp
    src/MetroidPrime/CArchMsgParmInt32Int32VoidPtr.cpp
    src/MetroidPrime/CArchMsgParmNull.cpp
    src/MetroidPrime/CArchMsgParmReal32.cpp
    src/MetroidPrime/CArchMsgParmUserInput.cpp
    src/MetroidPrime/CAxisAngle.cpp
    src/MetroidPrime/CControlMapper.cpp
    src/MetroidPrime/CFluidPlane.cpp
    src/MetroidPrime/CFluidPlaneCPU.cpp
    src/MetroidPrime/CFluidPlaneManagerGlobals.cpp
    src/MetroidPrime/CFluidUVMotion.cpp
    src/MetroidPrime/CGameHintInfo.cpp
    src/MetroidPrime/CIOWin.cpp
    src/MetroidPrime/CMatrix3f_Ext.cpp
    src/MetroidPrime/CMemoryDrawEnum.cpp
    src/MetroidPrime/CQuitGameScreen.cpp
    src/MetroidPrime/CTransitionDatabaseGame.cpp
    src/MetroidPrime/CWorldLayerState.cpp
    src/MetroidPrime/Cameras/CBallCameraTransitions.cpp
    src/MetroidPrime/Cameras/CCameraShakerData.cpp
    src/MetroidPrime/Carve8027D844.cpp
    src/MetroidPrime/Carve8027DC1C.cpp
    src/MetroidPrime/Enemies/CStateMachine.cpp
    src/MetroidPrime/Factories/CStateMachineFactory.cpp
    src/MetroidPrime/PathFinding/CPathFindRegion.cpp
    # CPathFindSearch::OnPath - the one CPatterned::NoPathNodes calls
    # (CPatternedAiFunctions.cpp:233) - plus the four functions it calls. CPathFindSearch.cpp and
    # CPathFindArea.cpp are both `NonMatching` units files.cmake does not list, and listing
    # either opens more than it closes (tools/check_files_cmake.py's EXCLUDED entries measure
    # them at +12 and +8). Bodies copied verbatim from those two files, closed group per file so
    # the gap only falls: net 250 -> 249, no new undefined symbol. See the file's header.
    src/MetroidPrime/PathFinding/CPathFindOnPath.cpp
    src/MetroidPrime/PathFinding/CPathFindSpline.cpp
    src/MetroidPrime/Player/CPlayerCameraBob.cpp
    src/MetroidPrime/Player/CPlayerOrbit.cpp
    src/MetroidPrime/Player/CPlayerVisor.cpp
    src/MetroidPrime/ScriptObjects/CScriptGenerator.cpp
    src/WorldFormat/CAreaOctTree_Tests.cpp
    src/WorldFormat/CCollisionPrimitiveData.cpp
    src/WorldFormat/CCollisionSurface.cpp
    # --- upstream (PrimeDecomp/echoes) units from the 2026-09-29 sync (upstream 03bd14b): 10 of
    # its 21 new configure.py objects, chosen by tools/link_check.sh against the recorded
    # baseline. CAuxWeapon closes 3 and opens 1 (CGameProjectile::GetBeamAttribType). The five
    # GunController units are one decision because they call each other: together they close 8
    # and open 4 (CPASDatabase::FindBestAnimation/GetAnimState, NWeaponTypes::are_tokens_ready,
    # CGunMotion::LoadAnimations). The other four open nothing: CFont.cpp replaces the port-only
    # CFontPortStub.cpp and CPlayerEnergyDrain.cpp one of PortLinkStubs.cpp's stubs. The
    # eleven left out open more than they close; they are in tools/check_files_cmake.py's
    # EXCLUDED list with their counts. The sync's headers also declare ~CCollidableAABox and
    # ~CCollidableSphere out of line, which CGameCollision.cpp and CRagDoll.cpp now ask for.
    # Net 254 -> 250 undefined, 0 duplicates.
    src/Collision/CCollidableCollisionSurface.cpp
    src/Collision/CCollisionInfo.cpp
    src/Kyoto/Text/CFont.cpp
    src/MetroidPrime/Player/CPlayerEnergyDrain.cpp
    src/MetroidPrime/Weapons/CAuxWeapon.cpp
    src/MetroidPrime/Weapons/GunController/CGSComboFire.cpp
    src/MetroidPrime/Weapons/GunController/CGSFidget.cpp
    src/MetroidPrime/Weapons/GunController/CGSFreeLook.cpp
    src/MetroidPrime/Weapons/GunController/CGunController.cpp
    src/MetroidPrime/Weapons/GunController/CGunMotion.cpp
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
