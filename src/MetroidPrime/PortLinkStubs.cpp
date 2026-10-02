/**
 * Port link stubs - GENERATED, do not hand-edit.
 *
 *   generator: tools/gen_link_stubs.py
 *   input:     docs/research/boot_path_stubbable.tsv  (from tools/link_reach.py)
 *
 * The port's link asked for 523 symbols that nothing in the tree defines. This
 * file supplies 168 of them: the ones referenced **only by
 * objects unreachable from the program's roots**, so a definition cannot change
 * what the game does and can only let the link finish.
 *
 *   164 functions, 4 data objects (counted 2026-10-02, after `fn_80008C28` was added by hand
 *   below for `Carve800052A0.c`; before that 163, after `fn_8020D278` was added by hand
 *   below for `Carve80213320.cpp`; before that 162, an exchange: `fn_80045014`'s stub was
 *   retired below because `Carve80045014.c` now defines that symbol for the port's link as well,
 *   and `fn_8004509C`'s was added in its place for the same unit, so the total did not move;
 *   before that, 162 after `fn_80044DD4` was added by hand below for `Carve80044D48.c`; before
 *   that 161, after `fn_80045014` was
 *   added by hand below for `Carve80044F88.c`; before that 160, after `fn_801FEC64` and `fn_801FDAA4` were
 *   added by hand below for `Carve801FF8A0.cpp`; before that 158, after `fn_801FE7E8` was added by
 *   hand below for `Carve801FFA20.cpp`; before that 157, after `fn_801B9C68` was added by hand
 *   below for `Carve801B9BE0.c`; before that 156, after `fn_8000408C` was added by hand
 *   below for `Carve80004010.c`; before that 155, after `fn_801F9848` was added by hand
 *   below for `Carve801F97C8.c`; before that 154, after the two
 *   `Carve801FF720.cpp` callees below were added; the count before that was 152, after `fn_80008D68` was added by
 *   hand below for `Carve800045A0.c`; the count before that was 151, after the three
 *   `Carve801FF5A0.cpp` callees were added by hand below, and 148 before those, itself
 *   after `fn_801FDC88` was added by hand, and 147 before that, measured
 *   2026-10-01 after the eighth upstream sync, which retired
 *   `CDamageVulnerability::~CDamageVulnerability()` - upstream's
 *   `CDamageVulnerability.cpp` defines it and is listed in `files.cmake`; this line read
 *   154 for the 151-function file, which its own breakdown already contradicted by one).
 *
 * Breakdown: 86 REL loader, 45 game method, 32 unmangled fn_/lbl_, 1 CodeWarrior-mangled
 * `rstl::rmemory_allocator::allocate`, 4 vtable/typeinfo. (counted 2026-10-02, after `fn_80008C28`
 *   was added by hand below for `Carve800052A0.c`; the game-method term read 46 and was wrong by
 *   one - it is the residual, `total - REL loaders - vtable/typeinfo - allocator`, which gives 45
 *   both before and after this stub. Derive rather than carry the numeral: the total is
 *   `grep -cE 'asm\("'`, the unmangled term is `grep -cE 'asm\("(fn_|lbl_)'`, the vtable/typeinfo
 *   term is the file's `stub_data_*` count, and the game-method term is what is left; before that
 *   31, after `fn_8020D278` was added by hand
 *   below for `Carve80213320.cpp`; before that 30, an exchange:
 *   `fn_80045014`'s stub was retired below because `Carve80045014.c` now defines that symbol for
 *   the port's link as well, and `fn_8004509C`'s was added in its place for the same unit, so the
 *   30 did not move; before that, 30 after `fn_80044DD4`
 *   was added by hand below for `Carve80044D48.c`; before that 28, after `fn_80045014`
 *   was added by hand below for `Carve80044F88.c`; before that 27, after `fn_801FEC64`
 *   and `fn_801FDAA4` were added by hand below for `Carve801FF8A0.cpp`; before that 25, after
 *   `fn_801FE7E8` was added by hand
 *   below for `Carve801FFA20.cpp`; before that 24, after `fn_801F9848` was added by hand
 *   below for `Carve801F97C8.c`; before that 154, after the two
 *   `Carve801FF720.cpp` callees below were added; the count before that was 152, after `fn_80008D68` was added by
 *   hand below for `Carve800045A0.c`; the count before that was 151, after the three
 *   `Carve801FF5A0.cpp` callees were added by hand below, and 148 before those, itself
 *   after `fn_801FDC88` was added by hand, and 147 before that, measured
 *   2026-10-01 after the eighth upstream sync, which retired
 *   `CDamageVulnerability::~CDamageVulnerability()` - upstream's
 *   `CDamageVulnerability.cpp` defines it and is listed in `files.cmake`; this line read
 *   154 for the 151-function file, which its own breakdown already contradicted by one).
 *
 * Breakdown: 86 REL loader, 46 game method, 22 unmangled fn_/lbl_, 1 CodeWarrior-mangled
 * `rstl::rmemory_allocator::allocate`, 4 vtable/typeinfo.
 *
 * **Eighteen more were deleted by hand in the 2026-09-28 upstream merge**, each now defined by an
 * upstream TU: `CPlayer::SetSpawnedMorphBallState`, `CPlayer::fn_80019E40`, `CPlayer::Teleport`,
 * `CPlayerGun::CPlayerGun`, `CModelData::CModelData(const CAnimRes&)`, `LoadForgottenObject`
 * and ten `CGunWeapon` members (`CPlayer.cpp`, `CPlayerDynamics.cpp`, `CPlayerGun.cpp`,
 * `CModelData.cpp`, `CScriptForgottenObject.cpp`, `CGunWeapon.cpp`); the seventeenth,
 * `CCharAnimMemoryMetrics::AddToTotalSize`, went when upstream's
 * `CCharAnimMemoryMetrics.cpp` was listed, the eighteenth, `CAi::TypesMatch`, when
 * upstream's `CPatterned` body made it reachable and PortGlobals.cpp took retail's body, and
 * five more - `CAxisAngle::CAxisAngle(CVector3f const&)`, `CAxisAngle::Identity()`,
 * `CAxisAngle::operator+=`, `operator*(CAxisAngle const&, float const&)` and
 * `operator+(CAxisAngle const&, CAxisAngle const&)` - when upstream's `CAxisAngle.cpp` was
 * listed, which is also where the port's own carve `CAxisAngleGetVector.cpp` went. The counts
 * above are the measured ones
 * rather than the ones this header used to claim. `CAi::CanBeShot`, `CAxisAngle::GetVector`,
 * `CGameArea::SetAreaAttributes` and `CGunWeapon::IsLoaded` each gained a real body in a
 * `Matching` unit, and a `Matching` unit *and* a stub for the same symbol is a duplicate
 * definition the host link refuses. Separately, the header used to claim 181 symbols and 177
 * functions where the file has always had 180 and 176 - the counts were never derived, which
 * is the same failure `tools/check_docs_claims.py` was written to stop.
 *
 * **One more was added by hand on 2026-10-01**, `CMetaTransFactory::CreateMetaTrans(CInputStream&)`,
 * because listing `src/Kyoto/Animation/CHalfTransition.cpp` in `files.cmake` opened it and nothing
 * in the port defines it. That is the same trade the deleted stubs recorded above: a real body
 * needs `CMetaTransFactory.cpp`, which opens three more. The measurement and the reachability
 * evidence are on `stub_177` itself.
 *
 * **Why a hand edit and not `tools/gen_link_stubs.py`:** its input is a link log, and a link
 * log records only *undefined* symbols. A symbol that is now defined twice is indistinguishable
 * in it from one that was never missing, so regenerating would silently strip the port's other
 * stubs. The generator refuses to regenerate for exactly that reason, and the refusal is
 * correct. The fix belongs in the generator's input, not in a hand edit.
 *
 * **None of this is decompilation and none of it is claimed to match retail.**
 * `configure.py` does not mention this file, so it cannot affect `main.dol` or
 * any of the 86 REL modules - it exists only in the port's build. What the
 * decompilation still owes is the other 342 undefined symbols, which
 * `tools/link_reach.py` says are referenced by objects that *are* reachable and
 * so cannot be stubbed blind.
 *
 * **How a symbol is defined without its signature.** The linker resolves the
 * mangled name and it is not a legal identifier, so `extern "C"` plus an `asm`
 * label carries it verbatim. The signature is the emptiest available, which is
 * only safe because the symbol is unreachable - and `tools/link_reach.py` will
 * move it into the reachable set if that ever stops being true, which is exactly
 * when a stub here would become a bug.
 */

// CActor::CreateShadow(bool)
extern "C" void stub_0() asm("_ZN6CActor12CreateShadowEb");
extern "C" void stub_0() {}

// CActor::CreateShadowIfNeeded()
extern "C" void stub_1() asm("_ZN6CActor20CreateShadowIfNeededEv");
extern "C" void stub_1() {}

// CActorParameters::None()
extern "C" void stub_2() asm("_ZN16CActorParameters4NoneEv");
extern "C" void stub_2() {}
// CAi::GetOrigin(CStateManager const&, CTeamAiRole const&, CVector3f const&) const
extern "C" void stub_4() asm("_ZNK3CAi9GetOriginERK13CStateManagerRK11CTeamAiRoleRK9CVector3f");
extern "C" void stub_4() {}

// CAi::Listen(CVector3f const&, EListenNoiseType)
extern "C" void stub_5() asm("_ZN3CAi6ListenERK9CVector3f16EListenNoiseType");
extern "C" void stub_5() {}

// CAxisAngle::CAxisAngle(CVector3f const&), CAxisAngle::Identity(),
// CAxisAngle::operator+=(CAxisAngle const&), operator*(CAxisAngle const&, float const&)
// and operator+(CAxisAngle const&, CAxisAngle const&) - stubs stub_7, stub_9, stub_10,
// stub_171 and stub_172 were deleted here on 2026-09-28, for the reason the header above
// gives for `CAxisAngle::GetVector`: configure.py's own src/MetroidPrime/CAxisAngle.cpp
// (MatchingFor) defines all five, and a real unit and a stub for the same symbol is the
// duplicate definition the host link refuses. The port's own carve of GetVector,
// src/MetroidPrime/CAxisAngleGetVector.cpp, went the same way; both are recorded in
// tools/check_files_cmake.py's EXCLUDED list.

// CCollisionPrimitive::CCollisionPrimitive(CMaterialList const&)
extern "C" void stub_12() asm("_ZN19CCollisionPrimitiveC2ERK13CMaterialList");
extern "C" void stub_12() {}

// CDamageInfo::CDamageInfo(CDamageInfo const&, float)
extern "C" void stub_13() asm("_ZN11CDamageInfoC1ERKS_f");
extern "C" void stub_13() {}

// CDamageVulnerability::CDamageVulnerability(CDamageVulnerability const&)
extern "C" void stub_14() asm("_ZN20CDamageVulnerabilityC1ERKS_");
extern "C" void stub_14() {}

// CElementGen::CElementGen(TToken<CGenDescription>, CElementGen::EModelOrientationType, CElementGen::EOptionalSystemFlags)
extern "C" void stub_16() asm("_ZN11CElementGenC1E6TTokenI15CGenDescriptionENS_21EModelOrientationTypeENS_20EOptionalSystemFlagsE");
extern "C" void stub_16() {}

// CElementGen::SetGlobalOrientAndTrans(CTransform4f const&)
extern "C" void stub_17() asm("_ZN11CElementGen23SetGlobalOrientAndTransERK12CTransform4f");
extern "C" void stub_17() {}

// CEnvFxManager::Play_801620A8()
extern "C" void stub_18() asm("_ZN13CEnvFxManager13Play_801620A8Ev");
extern "C" void stub_18() {}

// CEnvFxManager::SetDensity(float, int)
extern "C" void stub_19() asm("_ZN13CEnvFxManager10SetDensityEfi");
extern "C" void stub_19() {}

// CEnvFxManager::Stop_801620B4()
extern "C" void stub_20() asm("_ZN13CEnvFxManager13Stop_801620B4Ev");
extern "C" void stub_20() {}

// CFluidPlaneManager::CreateSplash(TUniqueId, CStateManager&, CScriptWater const&, CVector3f const&, float, bool)
extern "C" void stub_21() asm("_ZN18CFluidPlaneManager12CreateSplashE9TUniqueIdR13CStateManagerRK12CScriptWaterRK9CVector3ffb");
extern "C" void stub_21() {}

// CFluidPlaneManager::GetLastSplashDeltaTime(TUniqueId) const
extern "C" void stub_22() asm("_ZNK18CFluidPlaneManager22GetLastSplashDeltaTimeE9TUniqueId");
extern "C" void stub_22() {}

// CFontImageDef::GetHeight() const
extern "C" void stub_23() asm("_ZNK13CFontImageDef9GetHeightEv");
extern "C" void stub_23() {}

// CGunWeapon::ActivateCharge()
extern "C" void stub_26() asm("_ZN10CGunWeapon14ActivateChargeEv");
extern "C" void stub_26() {}


// CGunWeapon::Draw(bool, CStateManager const&, CTransform4f const&, CModelFlags const&, CActorLights const*) const
extern "C" void stub_28() asm("_ZNK10CGunWeapon4DrawEbRK13CStateManagerRK12CTransform4fRK11CModelFlagsPK12CActorLights");
extern "C" void stub_28() {}


// CGunWeapon::Fire(CToken&, bool, float, CPlayerState::EChargeStage, CTransform4f const&, CStateManager&, TUniqueId, int, unsigned short, TUniqueId*, CSfxHandle*, float, float)
extern "C" void stub_30() asm("_ZN10CGunWeapon4FireER6CTokenbfN12CPlayerState12EChargeStageERK12CTransform4fR13CStateManager9TUniqueIditPS9_P10CSfxHandleff");
extern "C" void stub_30() {}




// CGunWeapon::Unk11(CStateManager&)
extern "C" void stub_36() asm("_ZN10CGunWeapon5Unk11ER13CStateManager");
extern "C" void stub_36() {}

// CGunWeapon::Unk7()
extern "C" void stub_37() asm("_ZN10CGunWeapon4Unk7Ev");
extern "C" void stub_37() {}

// CGunWeapon::Unk9(CStateManager&)
extern "C" void stub_38() asm("_ZN10CGunWeapon4Unk9ER13CStateManager");
extern "C" void stub_38() {}





// CHealthInfo::CHealthInfo(CHealthInfo const&)
extern "C" void stub_43() asm("_ZN11CHealthInfoC1ERKS_");
extern "C" void stub_43() {}


// CMotionState::CMotionState(CVector3f const&, CNUQuaternion const&, CVector3f const&, CAxisAngle const&)
extern "C" void stub_45() asm("_ZN12CMotionStateC1ERK9CVector3fRK13CNUQuaternionS2_RK10CAxisAngle");
extern "C" void stub_45() {}

// CMetaTransFactory::CreateMetaTrans(CInputStream&) - added by hand on 2026-10-01, with
// src/Kyoto/Animation/CHalfTransition.cpp in `files.cmake`. That unit is retail's
// `CHalfTransition::CHalfTransition(CInputStream&)` and it calls this, so listing it opened
// exactly one symbol nothing in the port defines - measured with `tools/link_check.sh
// --strict`: 324 -> 325 undefined, "NEW CMetaTransFactory::CreateMetaTrans(CInputStream&)".
// Listing src/Kyoto/Animation/CMetaTransFactory.cpp instead, to give this a real body, was
// measured too and is worse: 324 -> 327, because it opens the three stream constructors
// `CMetaTransMetaAnim`, `CMetaTransPhaseTrans` and `CMetaTransTrans` and closes none. So this
// stub, not that unit.
//
// The reachability condition this file is written to holds, measured with `nm -A` over every
// object in build-port-link: CHalfTransition.cpp.o is the *only* object that references
// `_ZN17CMetaTransFactory15CreateMetaTransER12CInputStream`, and no object at all references
// `_ZN15CHalfTransitionC1ER12CInputStream`, so nothing in the port calls the constructor. The
// one other object that names the class - CTransitionDatabaseGame.cpp.o, which is listed -
// names it only inside its own mangled name, as the `rstl::vector<CHalfTransition>` parameter
// type of a constructor it already had.
extern "C" void stub_177() asm("_ZN17CMetaTransFactory15CreateMetaTransER12CInputStream");
extern "C" void stub_177() {}

// CPhysicsActorUnkB::~CPhysicsActorUnkB()
extern "C" void stub_46() asm("_ZN17CPhysicsActorUnkBD1Ev");
extern "C" void stub_46() {}

// CPhysicsState::CPhysicsState(CVector3f const&, CQuaternion const&, CVector3f const&, CAxisAngle const&, CVector3f const&, CVector3f const&, CVector3f const&, CAxisAngle const&, CAxisAngle const&)
extern "C" void stub_47() asm("_ZN13CPhysicsStateC1ERK9CVector3fRK11CQuaternionS2_RK10CAxisAngleS2_S2_S2_S8_S8_");
extern "C" void stub_47() {}



// CPlayer::UnkStructA::UnkStructA(TUniqueId)
extern "C" void stub_50() asm("_ZN7CPlayer10UnkStructAC1E9TUniqueId");
extern "C" void stub_50() {}



// CSamusHud::DisplayHudMemo(rstl::basic_string<wchar_t, rstl::char_traits<wchar_t>, rstl::rmemory_allocator> const&, CHUDMemoParms const&)
extern "C" void stub_54() asm("_ZN9CSamusHud14DisplayHudMemoERKN4rstl12basic_stringIwNS0_11char_traitsIwEENS0_17rmemory_allocatorEEERK13CHUDMemoParms");
extern "C" void stub_54() {}

// CScanTreeInventory::CScanTreeInventory(int, SLdrTransform const&, unsigned int, unsigned int, CPlayerState::EItemType, rstl::basic_string<char, rstl::char_traits<char>, rstl::rmemory_allocator> const&)
extern "C" void stub_55() asm("_ZN18CScanTreeInventoryC1EiRK13SLdrTransformjjN12CPlayerState9EItemTypeERKN4rstl12basic_stringIcNS5_11char_traitsIcEENS5_17rmemory_allocatorEEE");
extern "C" void stub_55() {}

// CScriptTrigger::GetTriggerBoundsWR() const
extern "C" void stub_56() asm("_ZNK14CScriptTrigger18GetTriggerBoundsWREv");
extern "C" void stub_56() {}

// CStateManager::SetActorAreaId(CActor&, TAreaId)
extern "C" void stub_58() asm("_ZN13CStateManager14SetActorAreaIdER6CActor7TAreaId");
extern "C" void stub_58() {}

// CStateManager::SetCurrentAreaId(TAreaId)
extern "C" void stub_59() asm("_ZN13CStateManager16SetCurrentAreaIdE7TAreaId");
extern "C" void stub_59() {}

// CStringTable::GetString(int) const
extern "C" void stub_60() asm("_ZNK12CStringTable9GetStringEi");
extern "C" void stub_60() {}

// CWorld::PropogateAreaChain(CGameArea::EOcclusionState, CGameArea*, CWorld*)
extern "C" void stub_61() asm("_ZN6CWorld18PropogateAreaChainEN9CGameArea15EOcclusionStateEPS0_PS_");
extern "C" void stub_61() {}

// GetBoundingBox__13CPhysicsActorCFv
extern "C" void stub_62() asm("GetBoundingBox__13CPhysicsActorCFv");
extern "C" void stub_62() {}

// LoadAIHint(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_63() asm("_Z10LoadAIHintR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_63() {}

// LoadAIJumpPoint(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_64() asm("_Z15LoadAIJumpPointR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_64() {}

// LoadAIKeyframe(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_65() asm("_Z14LoadAIKeyframeR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_65() {}

// LoadAIWaypoint(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_66() asm("_Z14LoadAIWaypointR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_66() {}

// LoadActor(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_67() asm("_Z9LoadActorR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_67() {}

// LoadActorKeyframe(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_68() asm("_Z17LoadActorKeyframeR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_68() {}

// LoadActorRotate(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_69() asm("_Z15LoadActorRotateR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_69() {}

// LoadAdvancedCounter(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_70() asm("_Z19LoadAdvancedCounterR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_70() {}

// LoadAmbientAI(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_71() asm("_Z13LoadAmbientAIR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_71() {}

// LoadAreaDamage(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_72() asm("_Z14LoadAreaDamageR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_72() {}

// LoadBallTrigger(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_73() asm("_Z15LoadBallTriggerR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_73() {}

// LoadCamera(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_74() asm("_Z10LoadCameraR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_74() {}

// LoadCameraBlurKeyframe(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_75() asm("_Z22LoadCameraBlurKeyframeR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_75() {}

// LoadCameraFilterKeyframe(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_76() asm("_Z24LoadCameraFilterKeyframeR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_76() {}

// LoadCameraHint(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_77() asm("_Z14LoadCameraHintR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_77() {}

// LoadCameraPitch(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_78() asm("_Z15LoadCameraPitchR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_78() {}

// LoadCameraShaker(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_79() asm("_Z16LoadCameraShakerR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_79() {}

// LoadCameraWaypoint(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_80() asm("_Z18LoadCameraWaypointR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_80() {}

// LoadColorModulate(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_81() asm("_Z17LoadColorModulateR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_81() {}

// LoadConditionalRelay(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_82() asm("_Z20LoadConditionalRelayR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_82() {}

// LoadControlHint(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_83() asm("_Z15LoadControlHintR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_83() {}

// LoadControllerAction(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_84() asm("_Z20LoadControllerActionR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_84() {}

// LoadCounter(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_85() asm("_Z11LoadCounterR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_85() {}

// LoadCoverPoint(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_86() asm("_Z14LoadCoverPointR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_86() {}

// LoadDamageActor(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_87() asm("_Z15LoadDamageActorR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_87() {}

// LoadDamageableTrigger(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_88() asm("_Z21LoadDamageableTriggerR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_88() {}

// LoadDamageableTriggerOriented(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_89() asm("_Z29LoadDamageableTriggerOrientedR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_89() {}

// LoadDebris(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_90() asm("_Z10LoadDebrisR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_90() {}

// LoadDebrisExtended(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_91() asm("_Z18LoadDebrisExtendedR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_91() {}

// LoadDistanceFog(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_92() asm("_Z15LoadDistanceFogR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_92() {}

// LoadDock(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_93() asm("_Z8LoadDockR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_93() {}

// LoadDoor(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_94() asm("_Z8LoadDoorR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_94() {}

// LoadDynamicLight(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_95() asm("_Z16LoadDynamicLightR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_95() {}

// LoadEMPulse(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_96() asm("_Z11LoadEMPulseR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_96() {}

// LoadEffect(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_97() asm("_Z10LoadEffectR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_97() {}

// LoadEnvFxDensityController(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_98() asm("_Z26LoadEnvFxDensityControllerR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_98() {}

// LoadFogVolume(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_99() asm("_Z13LoadFogVolumeR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_99() {}


// LoadGenerator(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_101() asm("_Z13LoadGeneratorR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_101() {}

// LoadGrapplePoint(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_102() asm("_Z16LoadGrapplePointR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_102() {}

// LoadHUDHint(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_103() asm("_Z11LoadHUDHintR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_103() {}

// LoadMemoryRelay(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_104() asm("_Z15LoadMemoryRelayR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_104() {}

// LoadMidi(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_105() asm("_Z8LoadMidiR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_105() {}

// LoadPathCamera(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_106() asm("_Z14LoadPathCameraR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_106() {}

// LoadPathMeshCtrl(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_107() asm("_Z16LoadPathMeshCtrlR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_107() {}

// LoadPickupGenerator(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_108() asm("_Z19LoadPickupGeneratorR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_108() {}

// LoadPlatform(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_109() asm("_Z12LoadPlatformR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_109() {}

// LoadPlayerHint(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_110() asm("_Z14LoadPlayerHintR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_110() {}

// LoadPlayerStateChange(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_111() asm("_Z21LoadPlayerStateChangeR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_111() {}

// LoadPointOfInterest(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_112() asm("_Z19LoadPointOfInterestR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_112() {}

// LoadPortalTransition(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_113() asm("_Z20LoadPortalTransitionR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_113() {}

// LoadRadialDamage(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_114() asm("_Z16LoadRadialDamageR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_114() {}

// LoadRandomRelay(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_115() asm("_Z15LoadRandomRelayR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_115() {}

// LoadRepulsor(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_116() asm("_Z12LoadRepulsorR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_116() {}

// LoadRipple(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_117() asm("_Z10LoadRippleR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_117() {}

// LoadRoomAcoustics(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_118() asm("_Z17LoadRoomAcousticsR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_118() {}

// LoadRumbleEffect(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_119() asm("_Z16LoadRumbleEffectR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_119() {}

// LoadScriptLayerController(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_120() asm("_Z25LoadScriptLayerControllerR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_120() {}

// LoadShadowProjector(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_121() asm("_Z19LoadShadowProjectorR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_121() {}

// LoadSilhouette(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_122() asm("_Z14LoadSilhouetteR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_122() {}

// LoadSound(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_123() asm("_Z9LoadSoundR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_123() {}

// LoadSoundModifier(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_124() asm("_Z17LoadSoundModifierR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_124() {}

// LoadSpecialFunction(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_125() asm("_Z19LoadSpecialFunctionR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_125() {}

// LoadSpiderBallAttractionSurface(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_126() asm("_Z31LoadSpiderBallAttractionSurfaceR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_126() {}

// LoadSpiderBallWaypoint(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_127() asm("_Z22LoadSpiderBallWaypointR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_127() {}

// LoadSpindleCamera(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_128() asm("_Z17LoadSpindleCameraR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_128() {}

// LoadSpinner(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_129() asm("_Z11LoadSpinnerR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_129() {}

// LoadSteam(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_130() asm("_Z9LoadSteamR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_130() {}

// LoadSubtitle(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_131() asm("_Z12LoadSubtitleR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_131() {}

// LoadSurfaceCamera(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_132() asm("_Z17LoadSurfaceCameraR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_132() {}

// LoadSwitch(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_133() asm("_Z10LoadSwitchR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_133() {}

// LoadTargetingPoint(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_134() asm("_Z18LoadTargetingPointR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_134() {}

// LoadTeamAI(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_135() asm("_Z10LoadTeamAIR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_135() {}

// LoadTextPane(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_136() asm("_Z12LoadTextPaneR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_136() {}

// LoadTimer(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_137() asm("_Z9LoadTimerR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_137() {}

// LoadTrigger(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_138() asm("_Z11LoadTriggerR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_138() {}

// LoadTriggerEllipsoid(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_139() asm("_Z20LoadTriggerEllipsoidR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_139() {}

// LoadTriggerOrientated(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_140() asm("_Z21LoadTriggerOrientatedR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_140() {}

// LoadTypedefSLdrConnection(SLdrConnection&, CInputStream&)
extern "C" void stub_141() asm("_Z25LoadTypedefSLdrConnectionR14SLdrConnectionR12CInputStream");
extern "C" void stub_141() {}

// LoadTypedefSLdrScannableParameters(SLdrScannableParameters&, CInputStream&)
extern "C" void stub_142() asm("_Z34LoadTypedefSLdrScannableParametersR23SLdrScannableParametersR12CInputStream");
extern "C" void stub_142() {}

// LoadVisorFlare(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_143() asm("_Z14LoadVisorFlareR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_143() {}

// LoadVisorGoo(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_144() asm("_Z12LoadVisorGooR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_144() {}

// LoadWallWalker(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_145() asm("_Z14LoadWallWalkerR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_145() {}

// LoadWater(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_146() asm("_Z9LoadWaterR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_146() {}

// LoadWaypoint(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_147() asm("_Z12LoadWaypointR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_147() {}

// LoadWorldLightFader(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_148() asm("_Z19LoadWorldLightFaderR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_148() {}

// LoadWorldTeleporter(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_149() asm("_Z19LoadWorldTeleporterR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_149() {}

// Render__13CPhysicsActorCFRC13CStateManager
extern "C" void stub_150() asm("Render__13CPhysicsActorCFRC13CStateManager");
extern "C" void stub_150() {}

// SMoverData::SMoverData(float, CVector3f const&, CAxisAngle const&, CVector3f const&, CAxisAngle const&)
extern "C" void stub_151() asm("_ZN10SMoverDataC1EfRK9CVector3fRK10CAxisAngleS2_S5_");
extern "C" void stub_151() {}

// fn_800747A4
extern "C" void stub_152() asm("fn_800747A4");
extern "C" void stub_152() {}

// fn_8007A73C
extern "C" void stub_153() asm("fn_8007A73C");
extern "C" void stub_153() {}

// fn_800FA748
extern "C" void stub_154() asm("fn_800FA748");
extern "C" void stub_154() {}

// fn_801B7420
extern "C" void stub_155() asm("fn_801B7420");
extern "C" void stub_155() {}

// fn_801B7E48
extern "C" void stub_156() asm("fn_801B7E48");
extern "C" void stub_156() {}

// fn_801B8C88
extern "C" void stub_157() asm("fn_801B8C88");
extern "C" void stub_157() {}

// fn_801BD654
extern "C" void stub_158() asm("fn_801BD654");
extern "C" void stub_158() {}

// LdrToDamageVulnerability__FRC23SLdrDamageVulnerability
extern "C" void stub_159() asm("LdrToDamageVulnerability__FRC23SLdrDamageVulnerability");
extern "C" void stub_159() {}

// kCAiSplashDenom
extern "C" void stub_160() asm("kCAiSplashDenom");
extern "C" void stub_160() {}

// lbl_4_rodata_0
extern "C" void stub_161() asm("lbl_4_rodata_0");
extern "C" void stub_161() {}

// skDamageHitTime__10CPatterned
extern "C" void stub_162() asm("skDamageHitTime__10CPatterned");
extern "C" void stub_162() {}

// lbl_8041AAC0
extern "C" void stub_163() asm("lbl_8041AAC0");
extern "C" void stub_163() {}

// lbl_8041AAC8
extern "C" void stub_164() asm("lbl_8041AAC8");
extern "C" void stub_164() {}

// lbl_8041AAD0
extern "C" void stub_165() asm("lbl_8041AAD0");
extern "C" void stub_165() {}

// lbl_8041AAD4
extern "C" void stub_166() asm("lbl_8041AAD4");
extern "C" void stub_166() {}

// lbl_8041AB0C
extern "C" void stub_167() asm("lbl_8041AB0C");
extern "C" void stub_167() {}

// lbl_8041AB20
extern "C" void stub_168() asm("lbl_8041AB20");
extern "C" void stub_168() {}

// lbl_8041AB24
extern "C" void stub_169() asm("lbl_8041AB24");
extern "C" void stub_169() {}

// lbl_8041B758
extern "C" void stub_170() asm("lbl_8041B758");
extern "C" void stub_170() {}

// sForwardVector__9CVector3f
extern "C" void stub_173() asm("sForwardVector__9CVector3f");
extern "C" void stub_173() {}

// sNoRotation__11CQuaternion
extern "C" void stub_174() asm("sNoRotation__11CQuaternion");
extern "C" void stub_174() {}

// sZeroVector__9CVector3f
extern "C" void stub_175() asm("sZeroVector__9CVector3f");
extern "C" void stub_175() {}

// sum_fn_80255128(rstl::vector<SLdrConnection, rstl::rmemory_allocator> const&)
extern "C" void stub_176() asm("_Z15sum_fn_80255128RKN4rstl6vectorI14SLdrConnectionNS_17rmemory_allocatorEEE");
extern "C" void stub_176() {}

// fn_801FDC88 - retail 0x801FDC88, 0x24 bytes, `destroy_impl<T*>` for this chain: it
// materialises `li r4,-1` and calls fn_801FDCAC (retail 0x801FDCAC). Asked for by the port
// because `src/MetroidPrime/ScriptObjects/Carve801FDB5C.c` (Matching, 0x801FDBE0..0x801FDC88)
// matches `fn_801FDC68` byte for byte and that body is exactly one `bl fn_801FDC88` - the call
// is in the bytes, so the carve cannot drop it. For the DOL nothing is needed: dtk's own
// `auto_03_801FDC88_text.o` defines it and `configure.py` does not mention this file, so the
// stub cannot reach main.dol. The port link does not have that object, which is why the gap
// grew 291 -> 292 (measured, `build/gate-probe.log`).
//
// This is a stand-in with an empty body, like every other stub in this file, and like them it
// is **not** a claim that fn_801FDC88 is decompiled - it is not, and
// `docs/research/port_link_gap.md` keeps the symbol listed as still missing. The alternative is
// carving 0x801FDC88..0x801FDCAC as well, which only moves the same gap one function along:
// that body calls fn_801FDCAC, and the chain continues. The call is unavoidable to begin with -
// the carve's `fn_801FDC68` is retail's byte for byte, and retail's is one `bl fn_801FDC88`.
extern "C" void stub_178() asm("fn_801FDC88");
extern "C" void stub_178() {}

// The three callees of `src/MetroidPrime/ScriptObjects/Carve801FF5A0.cpp` (Matching,
// 0x801FF5A0..0x801FF720), added by hand on 2026-10-02 for the same reason `stub_178` above
// exists: the carve's bytes *are* those `bl`s, so the calls cannot be dropped without losing
// the match, and the port's link does not have the dtk `auto_*` objects that define them in
// the DOL. Measured: the gap went 291 -> 294 with the carve listed (`build/gate-link.log`).
//
//   fn_801FF5A0 (retail 0x801FF5A0) calls `allocate__Q24rstl17rmemory_allocatorFi` at
//     0x801FF5D0 and `Free__7CMemoryFPCv` at 0x801FF624.  The latter was already defined; the
//     former was not, because the port spells the allocator `_ZN4rstl17rmemory_allocator8allocateEi`
//     and the DOL spells it with CodeWarrior's own mangling.
//   fn_801FF6B8 (retail 0x801FF6B8) calls `fn_801FEE40` at 0x801FF6E8 - the 0x20-byte element
//     copy, itself one `bl fn_801FEE60`.
//   fn_801FF66C (retail 0x801FF66C) calls `fn_801FD638` at 0x801FF690 - the 0x20-byte element
//     destructor, itself one `bl fn_801FD658`.
//
// Same trade as `stub_178`: empty bodies, no claim that any of the three is decompiled (none is),
// and carving them instead only moves the gap one function along because each is a forwarder.
// `docs/research/port_link_gap.md` keeps all three listed as still missing.
extern "C" void stub_179() asm("allocate__Q24rstl17rmemory_allocatorFi");
extern "C" void stub_179() {}

extern "C" void stub_180() asm("fn_801FEE40");
extern "C" void stub_180() {}

extern "C" void stub_181() asm("fn_801FD638");
extern "C" void stub_181() {}

// fn_80008D68 - retail 0x80008D68, 0x80 = 128 bytes (`config/G2ME01/symbols.txt:180`), the
// recursive node teardown of the 3-node string-keyed tree: destroy both subtrees, release the
// node's 28-byte key, `CMemory::Free` the node. Asked for by the port because
// `src/MetroidPrime/Carve800045A0.c` (Matching, 0x800045A0..0x80004744) reproduces `fn_800046D0`
// byte for byte, and that body's `lwz r4,0x10(r30) / cmplwi / beq / bl fn_80008D68` at
// 0x800046F0..0x80004700 is in retail's bytes, so the carve cannot drop the call. For the DOL
// nothing is needed: `src/MetroidPrime/main.cpp:301` writes this function
// (`extern "C" void fn_80008D68(void* self, SNode* node)`), `main.cpp` is in `configure.py` but
// not in `files.cmake`, and this file is not in `configure.py` at all, so the stub cannot reach
// main.dol. The port link does not carry that object, which is why its gap grows by this symbol:
// measured in this tree with the stub removed, `python3 tools/link_gap.py --rebuild` exits 1,
// prints `287 MISSING` and names `gap grew: fn_80008D68 is not in port_link_gap_list.md`; with
// the stub in place the same command prints `286 MISSING`, all accounted for, and the gate's
// probe reports `LINKED (291 undefined, 0 duplicates)`.
//
// This is a stand-in with an empty body, like every other stub in this file, and it is **not** a
// claim that fn_80008D68 is decompiled into the port - it is a GameCube-only TU's function and
// the port has no body for it. `docs/research/port_link_gap_list.md` regenerates unchanged
// because the symbol is now defined here rather than MISSING.
extern "C" void stub_182() asm("fn_80008D68");
extern "C" void stub_182() {}

// The two element callees of `src/MetroidPrime/ScriptObjects/Carve801FF720.cpp` (Matching,
// 0x801FF720..0x801FF8A0), added by hand for the same reason `stub_178` above exists: the carve's
// bytes *are* those `bl`s, so the calls cannot be dropped without losing the match, and the port's
// link does not have the dtk `auto_*` objects that define them in the DOL. Measured: the probe
// went LINKED -> NOT LINKED (293 undefined) with the carve listed and these two undefined
// (`build/probe-logs/link_check.log`, `NEW  fn_801FD8E0` / `NEW  fn_801FEA98`).
//
//   fn_801FF838 (retail 0x801FF838) calls `fn_801FEA98` at 0x801FF868 - the 36-byte element's
//     copy constructor, itself one `bl fn_801FEAB8` (0x20 bytes, `symbols.txt:8311`).
//   fn_801FF7EC (retail 0x801FF7EC) calls `fn_801FD8E0` at 0x801FF810 - the same element's
//     destructor, itself one `bl fn_801FD900` (0x20 bytes, `symbols.txt:8279`).
//
// Same trade as `stub_178` and as the three `Carve801FF5A0.cpp` callees above: empty bodies, no
// claim that either is decompiled (neither is), and carving them instead only moves the gap one
// function along because each is a forwarder. `docs/research/port_link_gap_list.md` keeps both
// listed as still missing.
extern "C" void stub_183() asm("fn_801FEA98");
extern "C" void stub_183() {}

extern "C" void stub_184() asm("fn_801FD8E0");
extern "C" void stub_184() {}

// fn_801F9848 - retail 0x801F9848, 0x88 = 136 bytes / 34 instructions (`symbols.txt:8193`), the
// copy constructor of the 0x40-byte element that `CCameraColliderGroup`'s vector holds: it stores
// the vtable `lbl_803B6564` into +0x0 and copies the rest of the payload with interleaved
// `lfs`/`stfs` pairs and a `lwz`/`stw` at +0x38. Asked for by the port
// because `src/MetroidPrime/ScriptObjects/Carve801F97C8.c` (Matching, 0x801F97C8..0x801F9848)
// reproduces `fn_801F9820` byte for byte, and that body is exactly `cmplwi r3,0 / beq / bl
// fn_801F9848` - the call is in retail's bytes, so the carve cannot drop it. For the DOL nothing is
// needed: dtk's own `auto_03_801F9848_text.o` defines it, and this file is not in `configure.py`,
// so the stub cannot reach main.dol. The port link does not have that object, which is why the gap
// grew by this symbol: measured in this tree with the stub absent,
// `python3 tools/link_gap.py --rebuild` exits 1 and prints `gap grew: fn_801F9848 is not in
// port_link_gap_list.md` (`build/gate-link.log`), with 287 MISSING.
//
// This is a stand-in with an empty body, like every other stub in this file, and it is **not** a
// claim that fn_801F9848 is decompiled - it is not. Unlike `stub_178` and the callees above, the
// alternative is not "carve one more forwarder": this body has no `bl` at all, and matching its 34
// instructions is a spelling job of its own, which is why the claim stops at 0x801F9848.
extern "C" void stub_185() asm("fn_801F9848");
extern "C" void stub_185() {}

// fn_8000408C - retail 0x8000408C, 0xC8 = 200 bytes (`config/G2ME01/symbols.txt:67`), the
// copy-assign of the member at +4 of the `rstl::pair`-shaped object: it tears the destination's
// root at +0x10 down through `fn_80008D68`, clears +0x10/+8/+0xC/+4 in that order, clones the
// source's root with `fn_80008C28` and re-links the clone's two chains into +8/+0xC. Asked for by
// the port because `src/MetroidPrime/Carve80004010.c` (Matching, 0x80004010..0x8000408C)
// reproduces `fn_8000405C` byte for byte, and that body's `bl fn_8000408C` at 0x80004070 is in
// retail's bytes, so the carve cannot drop the call. For the DOL nothing is needed: dtk's own
// `auto_03_80003BE8_text.o` defines it, and this file is not in `configure.py`, so the stub cannot
// reach main.dol. The port link does not carry that object, which is why its gap would grow by
// this symbol: the carve's object is in `files.cmake`, and nothing else in the tree names
// `fn_8000408C` (measured: `grep -rn fn_8000408C src/ include/` is empty before this block).
//
// This is a stand-in with an empty body, like every other stub in this file, and it is **not** a
// claim that fn_8000408C is decompiled - it is not. Matching its 0xC8 bytes is a spelling job of
// its own, which is why `Carve80004010.c`'s claim stops at 0x8000408C. The same trade `stub_182`
// above makes for the neighbouring `Carve800045A0.c`.
extern "C" void stub_186() asm("fn_8000408C");
extern "C" void stub_186() {}

// fn_80008C28 - retail 0x80008C28, 0xB8 = 184 bytes (`config/G2ME01/symbols.txt:178`), the
// post-order clone of the three-node tree. Asked for by the port because
// `src/MetroidPrime/Player/Carve800052A0.c` (Matching, 0x800052A0..0x800053B8) reproduces
// `fn_80005310` byte for byte, and that body's `bl fn_80008C28` at 0x80005350 is in retail's
// bytes, so the carve cannot drop the call. It is the same callee `stub_186` above is asked for
// by `Carve80004010.c`, and for the same reason: the symbol sits **above** each carve's claim, so
// the carve can only declare it. For the DOL nothing is needed - dtk's own `main.o` defines it,
// and this file is not in `configure.py`, so the stub cannot reach main.dol; its DOL half is real
// and already `Matching` at `src/MetroidPrime/main.cpp:346`, whose comment derives the node. The
// port link does not carry that object, which is why its gap would grow by this symbol: measured
// in this tree without this block, `python3 tools/link_gap.py --rebuild` prints `286  MISSING`
// and names this symbol; with the block in place the same command prints `285  MISSING`, all
// accounted for.
//
// This is a stand-in with an empty body, like every other stub in this file, and it is **not** a
// claim that fn_80008C28 is decompiled for the port - it is not. Matching its 0xB8 bytes is a
// spelling job of its own, which is why `Carve800052A0.c`'s claim stops at 0x800053B8 and calls
// it instead. The same trade `stub_182` makes for the neighbouring `Carve800045A0.c`.
extern "C" void stub_195() asm("fn_80008C28");
extern "C" void stub_195() {}

// fn_801B9C68 - retail 0x801B9C68, 0x148 = 328 bytes (`config/G2ME01/symbols.txt:7194`), the copy
// constructor of the member at +0x4 of the 0x70-byte element `Carve801B9BE0.c` copy-constructs: a
// 0x10-byte header (`lhz` at +0, word at +0x4, `lfs`/`stfs` at +0x8, word at +0xC), then its
// 2-byte-element run from +0x10 (eight `lhz`/`sth` pairs per `mtctr` iteration plus an `andi.`
// remainder loop), then words and floats from +0x20. Asked for by the port because
// `src/MetroidPrime/Carve801B9BE0.c` (Matching, 0x801B9BE0..0x801B9C68) reproduces `fn_801B9C28`
// byte for byte, and that body's `bl fn_801B9C68` at 0x801B9C4C is in retail's bytes, so the carve
// cannot drop the call. For the DOL nothing is needed: dtk's own `auto_03_801B9C68_text.o` defines
// it, and this file is not in `configure.py`, so the stub cannot reach main.dol. The port link does
// not carry that object, which is why its gap would grow by this symbol: measured in this tree
// without this block, `python3 tools/link_gap.py --rebuild` exits 1, prints `287 MISSING` and names
// `gap grew: fn_801B9C68 is not in port_link_gap_list.md`; with the block in place the same command
// prints `286 MISSING`, all accounted for, and the gate's probe reports
// `LINKED (291 undefined, 0 duplicates)`.
//
// This is a stand-in with an empty body, like every other stub in this file, and it is **not** a
// claim that fn_801B9C68 is decompiled - it is not. Matching its 0x148 bytes is a spelling job of
// its own, which is why `Carve801B9BE0.c`'s claim stops at 0x801B9C68. The same trade `stub_185`
// above makes for the neighbouring `Carve801F97C8.c`.
extern "C" void stub_187() asm("fn_801B9C68");
extern "C" void stub_187() {}

// fn_801FE7E8 - retail 0x801FE7E8, 0x20 bytes (`config/G2ME01/symbols.txt:8305`), the copy of the
// 0x30-byte element `Carve801FFA20.cpp` copy-constructs. It is itself a forwarder:
// `stwu`/`mflr`/`stw` / `bl fn_801FE808` / `mtlr`/`addi`/`blr`, and that is why the claim stops at
// 0x801FE7E8 rather than taking `fn_801FE808` as well - matching the forwarded body is a separate
// spelling job, and a second carve here would only move the port's link gap one function along,
// the trade `stub_186` and `stub_187` above already make for their neighbouring units.
//
// Asked for by the port because `src/MetroidPrime/ScriptObjects/Carve801FFA20.cpp` (Matching,
// 0x801FFA20..0x801FFBA0) reproduces `fn_801FFB38` byte for byte, and that body's `bl fn_801FE7E8`
// at 0x801FFB68 is in retail's bytes, so the carve cannot drop the call. For the DOL nothing is
// needed: dtk's own `auto_03_801FE7E8_text.o` defines it, and this file is not in `configure.py`,
// so the stub cannot reach main.dol. The port link does not carry that object, which is why its gap
// would grow by this symbol: measured in this tree without this block, `python3 tools/link_gap.py
// --rebuild` prints `287 MISSING` and names
// `gap grew: fn_801FE7E8 is not in port_link_gap_list.md`; with the block in place the same command
// prints `286 MISSING`, all accounted for.
//
// This is a stand-in with an empty body, like every other stub in this file, and it is **not** a
// claim that fn_801FE7E8 is decompiled - it is not.
extern "C" void stub_188() asm("fn_801FE7E8");
extern "C" void stub_188() {}

// fn_801FEC64 - retail 0x801FEC64, 0x20 bytes (`config/G2ME01/symbols.txt:8316`), the copy of the
// 0x2C-byte element `Carve801FF8A0.cpp` copy-constructs. It is itself a forwarder:
// `stwu`/`mflr`/`stw` / `bl fn_801FEC84` / `mtlr`/`addi`/`blr`, and that is why the claim stops at
// 0x801FEC64 rather than taking `fn_801FEC84` as well - matching the forwarded body is a separate
// spelling job, and a second carve here would only move the port's link gap one function along,
// the trade `stub_188` above already makes for the neighbouring unit.
//
// Asked for by the port because `src/MetroidPrime/ScriptObjects/Carve801FF8A0.cpp` (Matching,
// 0x801FF8A0..0x801FFA20) reproduces `fn_801FF9B8` byte for byte, and that body's `bl fn_801FEC64`
// at 0x801FF9E8 is in retail's bytes, so the carve cannot drop the call. For the DOL nothing is
// needed: dtk's own auto object for the unclaimed range that holds it defines it (this run
// `auto_03_801FDC88_text.o`, 0x801FDC88..0x801FEEF0), and this file is not in `configure.py`, so
// the stub cannot reach main.dol. The port link does not carry that object, which is why its gap
// would grow by this symbol: measured in this tree without this block, `python3 tools/link_gap.py
// --rebuild` prints `288 MISSING` and names both
// `gap grew: fn_801FDAA4 is not in port_link_gap_list.md` and
// `gap grew: fn_801FEC64 is not in port_link_gap_list.md`; with the two blocks in place the same
// command prints `286 MISSING`, all accounted for.
//
// This is a stand-in with an empty body, like every other stub in this file, and it is **not** a
// claim that fn_801FEC64 is decompiled - it is not.
extern "C" void stub_189() asm("fn_801FEC64");
extern "C" void stub_189() {}

// fn_801FDAA4 - retail 0x801FDAA4, 0x20 bytes (`config/G2ME01/symbols.txt:8285`), the destructor of
// the same 0x2C-byte element `Carve801FF8A0.cpp` destroys. It is itself a forwarder:
// `stwu`/`mflr`/`stw` / `bl fn_801FDAC4` / `mtlr`/`addi`/`blr`, the same trade as `stub_189` above.
//
// Asked for by the port because `src/MetroidPrime/ScriptObjects/Carve801FF8A0.cpp` (Matching,
// 0x801FF8A0..0x801FFA20) reproduces `fn_801FF96C` byte for byte, and that body's `bl fn_801FDAA4`
// at 0x801FF990 is in retail's bytes, so the carve cannot drop the call. For the DOL nothing is
// needed: dtk's own auto object for the unclaimed range that holds it defines it (this run
// `auto_03_801FA3CC_text.o`, 0x801FA3CC..0x801FDBE0), and this file is not in `configure.py`, so
// the stub cannot reach main.dol. The measurement for both this block and `stub_189` is on that
// stub's comment.
//
// This is a stand-in with an empty body, like every other stub in this file, and it is **not** a
// claim that fn_801FDAA4 is decompiled - it is not.
extern "C" void stub_190() asm("fn_801FDAA4");
extern "C" void stub_190() {}

// fn_8004509C - retail 0x8004509C, 0x28 = 40 bytes (`config/G2ME01/symbols.txt:1277`), the
// per-element copy `Carve80045014.c`'s `fn_8004507C` calls at 0x80045088. Asked for by the port
// because that unit (Matching, 0x80045014..0x8004509C) reproduces its two functions byte for byte,
// and `fn_8004507C`'s `bl fn_8004509C` is in retail's bytes, so the carve cannot drop the call. For
// the DOL nothing is needed: dtk's own auto object for the unclaimed range that holds it defines it
// (this run `auto_03_8004509C_text.o`, 0x8004509C..0x80045CD4), and this file is not in
// `configure.py`, so the stub cannot reach main.dol. The port link does not carry that object,
// which is why its gap would grow by this symbol: measured in this tree without this block,
// `python3 tools/link_gap.py --rebuild` prints `286 MISSING` and names
// `gap grew: fn_8004509C is not in port_link_gap_list.md`; with the block in place the same command
// prints `285 MISSING`, all accounted for.
//
// This is a stand-in with an empty body, like every other stub in this file, and it is **not** a
// claim that fn_8004509C is decompiled - it is not. Matching its 0x28 bytes is a spelling job of its
// own, which is why `Carve80045014.c`'s claim stops at 0x8004509C. The same trade `stub_186` above
// makes for the neighbouring `Carve80004010.c`.
extern "C" void stub_193() asm("fn_8004509C");
extern "C" void stub_193() {}

// fn_80044DD4 - retail 0x80044DD4, 0x70 = 112 bytes (`config/G2ME01/symbols.txt:1267`), the
// elementwise copy `uninitialized_copy_n` for the 0x20-byte element `Carve80044D48.c`'s
// `fn_80044D90` copy-constructs. Asked for by the port because that unit (Matching,
// 0x80044D48..0x80044DD4) reproduces its three functions byte for byte, and `fn_80044D90`'s
// `bl fn_80044DD4` at 0x80044DB8 is in retail's bytes, so the carve cannot drop the call. For the
// DOL nothing is needed: dtk's own auto object for the unclaimed range that holds it defines it
// (this run `auto_03_80044DD4_text.o`, 0x80044DD4..0x80044F88), and this file is not in
// `configure.py`, so the stub cannot reach main.dol. The port link does not carry that object,
// which is why its gap would grow by this symbol: measured in this tree without this block,
// `python3 tools/link_gap.py --rebuild` prints `286 MISSING` and names
// `gap grew: fn_80044DD4 is not in port_link_gap_list.md`; with the block in place the same command
// prints `285 MISSING`, all accounted for.
//
// This is a stand-in with an empty body, like every other stub in this file, and it is **not** a
// claim that fn_80044DD4 is decompiled - it is not. Matching its 0x70 bytes is a spelling job of its
// own, which is why `Carve80044D48.c`'s claim stops at 0x80044DD4. The same trade `stub_193` above
// makes for the neighbouring `Carve80045014.c`.
extern "C" void stub_192() asm("fn_80044DD4");
extern "C" void stub_192() {}

// fn_8020D278 - retail 0x8020D278, 0x64 = 100 bytes (`config/G2ME01/symbols.txt:8466`), the
// element destructor `Carve80213320.cpp`'s destroy loop calls. Asked for by the port because that
// unit (Matching, 0x80213320..0x8021348C) reproduces its three functions byte for byte, and the
// loop's `bl fn_8020D278` at 0x802133A8 is in retail's bytes, so the carve cannot drop the call.
// For the DOL nothing is needed: 0x8020D278 sits inside `MetroidPrime/CPauseScreen.cpp`'s
// `NonMatching` claim (0x80201F4C..0x8020DA7C), so dtk emits retail's own bytes for it (this run
// `build/G2ME01/asm/MetroidPrime/CPauseScreen.s`, `.fn fn_8020D278, global`), and this file is not
// in `configure.py`, so the stub cannot reach main.dol. The port link does not carry that object,
// which is why its gap would grow by this symbol: measured in this tree without this block,
// `python3 tools/link_gap.py --rebuild` prints `286  MISSING` and
// `gap grew: fn_8020D278 is not in port_link_gap_list.md`; with the block in place the same command
// prints `285  MISSING`, all accounted for.
//
// This is a stand-in with an empty body, like every other stub in this file, and it is **not** a
// claim that fn_8020D278 is decompiled - it is not: 0x8020D278 is inside a `NonMatching` unit.
// Matching its 0x64 bytes is a spelling job of its own, which is why `Carve80213320.cpp`'s claim
// calls it rather than reproducing it. The same trade `stub_192`/`stub_193` above make for their
// carves' callees.
extern "C" void stub_194() asm("fn_8020D278");
extern "C" void stub_194() {}

// fn_801FBD68 - retail 0x801FBD68, 0x68 = 104 bytes (`config/G2ME01/symbols.txt:8232`), the walk
// `Carve801FBC58.c`'s fn_801FBD30 forwards to. Asked for by the port because that unit (Matching,
// 0x801FBC58..0x801FBD68) reproduces its three functions byte for byte, and fn_801FBD30's
// `bl fn_801FBD68` at 0x801FBD54 is in retail's bytes, so the carve cannot drop the call.
// For the DOL nothing is needed: 0x801FBD68 is exactly 0x0 bytes past that unit's claim end, so it
// stays retail's and dtk emits its own bytes from `auto_03_801FBD68_text.o`; this file is not in
// `configure.py`, so the stub cannot reach main.dol. The port link does not carry that object,
// which is why its gap would grow by this symbol: measured in this tree without this block,
// `python3 tools/link_gap.py --rebuild` prints `286  MISSING` and
// `gap grew: fn_801FBD68 is not in port_link_gap_list.md`; with the block in place the same command
// prints `285  MISSING`, all accounted for.
//
// This is a stand-in with an empty body, like every other stub in this file, and it is **not** a
// claim that fn_801FBD68 is decompiled - it is not: 0x801FBD68 is unclaimed and dtk's. Matching its
// 0x68 bytes is a spelling job of its own (it walks the 0xC-byte elements in strides of 0xC and
// calls fn_801FFE4C, the `rc_ptr` release at +4 of each, which needs the refcount layout), which is
// why `Carve801FBC58.c`'s claim calls it rather than reproducing it. The same trade `stub_192`/
// `stub_194` above make for their carves' callees. A lane that claims 0x801FBD68..0x801FBE00 gets
// it and fn_801FFE4C at once and can drop this block.
extern "C" void stub_196() asm("fn_801FBD68");
extern "C" void stub_196() {}


// Data objects. A vtable or typeinfo stub is zero-filled: harmless to take the
// address of, and a crash if used - which unreachable means it is not.
//
// The `= {}` is load-bearing. A tentative definition with no initialiser is
// discarded as unused and the symbol never reaches the object file, which
// looks exactly like the stub not working. Measured, not assumed.

// typeinfo for CGunWeapon
extern "C" char stub_data_0[64] asm("_ZTI10CGunWeapon") = {};

// vtable for CCollidableAABox
extern "C" char stub_data_1[64] asm("_ZTV16CCollidableAABox") = {};

// vtable for CPatterned
extern "C" char stub_data_2[64] asm("_ZTV10CPatterned") = {};

// vtable for CPlayer
extern "C" char stub_data_3[64] asm("_ZTV7CPlayer") = {};
