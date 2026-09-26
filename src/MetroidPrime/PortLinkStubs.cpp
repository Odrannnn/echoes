/**
 * Port link stubs - GENERATED, do not hand-edit.
 *
 *   generator: tools/gen_link_stubs.py
 *   input:     docs/research/boot_path_stubbable.tsv  (from tools/link_reach.py)
 *
 * The port's link asked for 523 symbols that nothing in the tree defines. This
 * file supplies 181 of them: the ones referenced **only by
 * objects unreachable from the program's roots**, so a definition cannot change
 * what the game does and can only let the link finish.
 *
 *   177 functions, 4 data objects.
 *
 * Breakdown: 87 REL loader, 72 game method, 18 unmangled fn_/lbl_, 4 vtable/typeinfo.
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

// CAi::CanBeShot(CStateManager const&, int)
extern "C" void stub_3() asm("_ZN3CAi9CanBeShotERK13CStateManageri");
extern "C" void stub_3() {}

// CAi::GetOrigin(CStateManager const&, CTeamAiRole const&, CVector3f const&) const
extern "C" void stub_4() asm("_ZNK3CAi9GetOriginERK13CStateManagerRK11CTeamAiRoleRK9CVector3f");
extern "C" void stub_4() {}

// CAi::Listen(CVector3f const&, EListenNoiseType)
extern "C" void stub_5() asm("_ZN3CAi6ListenERK9CVector3f16EListenNoiseType");
extern "C" void stub_5() {}

// CAi::TypesMatch(int) const
extern "C" void stub_6() asm("_ZNK3CAi10TypesMatchEi");
extern "C" void stub_6() {}

// CAxisAngle::CAxisAngle(CVector3f const&)
extern "C" void stub_7() asm("_ZN10CAxisAngleC1ERK9CVector3f");
extern "C" void stub_7() {}

// CAxisAngle::GetVector() const
extern "C" void stub_8() asm("_ZNK10CAxisAngle9GetVectorEv");
extern "C" void stub_8() {}

// CAxisAngle::Identity()
extern "C" void stub_9() asm("_ZN10CAxisAngle8IdentityEv");
extern "C" void stub_9() {}

// CAxisAngle::operator+=(CAxisAngle const&)
extern "C" void stub_10() asm("_ZN10CAxisAnglepLERKS_");
extern "C" void stub_10() {}

// CCharAnimMemoryMetrics::AddToTotalSize(unsigned int, CCharAnimMemoryMetrics::EAnimSubSystem)
extern "C" void stub_11() asm("_ZN22CCharAnimMemoryMetrics14AddToTotalSizeEjNS_14EAnimSubSystemE");
extern "C" void stub_11() {}

// CCollisionPrimitive::CCollisionPrimitive(CMaterialList const&)
extern "C" void stub_12() asm("_ZN19CCollisionPrimitiveC2ERK13CMaterialList");
extern "C" void stub_12() {}

// CDamageInfo::CDamageInfo(CDamageInfo const&, float)
extern "C" void stub_13() asm("_ZN11CDamageInfoC1ERKS_f");
extern "C" void stub_13() {}

// CDamageVulnerability::CDamageVulnerability(CDamageVulnerability const&)
extern "C" void stub_14() asm("_ZN20CDamageVulnerabilityC1ERKS_");
extern "C" void stub_14() {}

// CDamageVulnerability::~CDamageVulnerability()
extern "C" void stub_15() asm("_ZN20CDamageVulnerabilityD1Ev");
extern "C" void stub_15() {}

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

// CGameArea::SetAreaAttributes(CScriptAreaProperties*)
extern "C" void stub_24() asm("_ZN9CGameArea17SetAreaAttributesEP21CScriptAreaProperties");
extern "C" void stub_24() {}

// CGameArea::TryTakingOutOfARAM()
extern "C" void stub_25() asm("_ZN9CGameArea18TryTakingOutOfARAMEv");
extern "C" void stub_25() {}

// CGunWeapon::ActivateCharge()
extern "C" void stub_26() asm("_ZN10CGunWeapon14ActivateChargeEv");
extern "C" void stub_26() {}

// CGunWeapon::CGunWeapon(EWeaponType, TUniqueId, CVector3f const&, int)
extern "C" void stub_27() asm("_ZN10CGunWeaponC2E11EWeaponType9TUniqueIdRK9CVector3fi");
extern "C" void stub_27() {}

// CGunWeapon::Draw(bool, CStateManager const&, CTransform4f const&, CModelFlags const&, CActorLights const*) const
extern "C" void stub_28() asm("_ZNK10CGunWeapon4DrawEbRK13CStateManagerRK12CTransform4fRK11CModelFlagsPK12CActorLights");
extern "C" void stub_28() {}

// CGunWeapon::DrawMuzzleFx(CStateManager const&) const
extern "C" void stub_29() asm("_ZNK10CGunWeapon12DrawMuzzleFxERK13CStateManager");
extern "C" void stub_29() {}

// CGunWeapon::Fire(CToken&, bool, float, CPlayerState::EChargeStage, CTransform4f const&, CStateManager&, TUniqueId, int, unsigned short, TUniqueId*, CSfxHandle*, float, float)
extern "C" void stub_30() asm("_ZN10CGunWeapon4FireER6CTokenbfN12CPlayerState12EChargeStageERK12CTransform4fR13CStateManager9TUniqueIditPS9_P10CSfxHandleff");
extern "C" void stub_30() {}

// CGunWeapon::IsLoaded() const
extern "C" void stub_31() asm("_ZNK10CGunWeapon8IsLoadedEv");
extern "C" void stub_31() {}

// CGunWeapon::Load(CStateManager&, bool)
extern "C" void stub_32() asm("_ZN10CGunWeapon4LoadER13CStateManagerb");
extern "C" void stub_32() {}

// CGunWeapon::PlayAnim(NWeaponTypes::EGunAnimType, bool)
extern "C" void stub_33() asm("_ZN10CGunWeapon8PlayAnimEN12NWeaponTypes12EGunAnimTypeEb");
extern "C" void stub_33() {}

// CGunWeapon::PostRenderGunFx(CStateManager const&, CTransform4f const&)
extern "C" void stub_34() asm("_ZN10CGunWeapon15PostRenderGunFxERK13CStateManagerRK12CTransform4f");
extern "C" void stub_34() {}

// CGunWeapon::Reset(CStateManager&)
extern "C" void stub_35() asm("_ZN10CGunWeapon5ResetER13CStateManager");
extern "C" void stub_35() {}

// CGunWeapon::Unk11(CStateManager&)
extern "C" void stub_36() asm("_ZN10CGunWeapon5Unk11ER13CStateManager");
extern "C" void stub_36() {}

// CGunWeapon::Unk7()
extern "C" void stub_37() asm("_ZN10CGunWeapon4Unk7Ev");
extern "C" void stub_37() {}

// CGunWeapon::Unk9(CStateManager&)
extern "C" void stub_38() asm("_ZN10CGunWeapon4Unk9ER13CStateManager");
extern "C" void stub_38() {}

// CGunWeapon::Unload(CStateManager&)
extern "C" void stub_39() asm("_ZN10CGunWeapon6UnloadER13CStateManager");
extern "C" void stub_39() {}

// CGunWeapon::Update(float, CStateManager&)
extern "C" void stub_40() asm("_ZN10CGunWeapon6UpdateEfR13CStateManager");
extern "C" void stub_40() {}

// CGunWeapon::UpdateGunFx(bool, float, CStateManager const&, CTransform4f const&)
extern "C" void stub_41() asm("_ZN10CGunWeapon11UpdateGunFxEbfRK13CStateManagerRK12CTransform4f");
extern "C" void stub_41() {}

// CGunWeapon::~CGunWeapon()
extern "C" void stub_42() asm("_ZN10CGunWeaponD2Ev");
extern "C" void stub_42() {}

// CHealthInfo::CHealthInfo(CHealthInfo const&)
extern "C" void stub_43() asm("_ZN11CHealthInfoC1ERKS_");
extern "C" void stub_43() {}

// CModelData::CModelData(CAnimRes const&)
extern "C" void stub_44() asm("_ZN10CModelDataC1ERK8CAnimRes");
extern "C" void stub_44() {}

// CMotionState::CMotionState(CVector3f const&, CNUQuaternion const&, CVector3f const&, CAxisAngle const&)
extern "C" void stub_45() asm("_ZN12CMotionStateC1ERK9CVector3fRK13CNUQuaternionS2_RK10CAxisAngle");
extern "C" void stub_45() {}

// CPhysicsActorUnkB::~CPhysicsActorUnkB()
extern "C" void stub_46() asm("_ZN17CPhysicsActorUnkBD1Ev");
extern "C" void stub_46() {}

// CPhysicsState::CPhysicsState(CVector3f const&, CQuaternion const&, CVector3f const&, CAxisAngle const&, CVector3f const&, CVector3f const&, CVector3f const&, CAxisAngle const&, CAxisAngle const&)
extern "C" void stub_47() asm("_ZN13CPhysicsStateC1ERK9CVector3fRK11CQuaternionS2_RK10CAxisAngleS2_S2_S2_S8_S8_");
extern "C" void stub_47() {}

// CPlayer::SetSpawnedMorphBallState(CPlayer::EPlayerMorphBallState, CStateManager&)
extern "C" void stub_48() asm("_ZN7CPlayer24SetSpawnedMorphBallStateENS_21EPlayerMorphBallStateER13CStateManager");
extern "C" void stub_48() {}

// CPlayer::Teleport(CTransform4f const&, CStateManager&, bool)
extern "C" void stub_49() asm("_ZN7CPlayer8TeleportERK12CTransform4fR13CStateManagerb");
extern "C" void stub_49() {}

// CPlayer::UnkStructA::UnkStructA(TUniqueId)
extern "C" void stub_50() asm("_ZN7CPlayer10UnkStructAC1E9TUniqueId");
extern "C" void stub_50() {}

// CPlayer::fn_80019E40(CStateManager&, int)
extern "C" void stub_51() asm("_ZN7CPlayer11fn_80019E40ER13CStateManageri");
extern "C" void stub_51() {}

// CPlayerEnergyDrain::CPlayerEnergyDrain(unsigned int)
extern "C" void stub_52() asm("_ZN18CPlayerEnergyDrainC1Ej");
extern "C" void stub_52() {}

// CPlayerGun::CPlayerGun(TUniqueId, int)
extern "C" void stub_53() asm("_ZN10CPlayerGunC1E9TUniqueIdi");
extern "C" void stub_53() {}

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

// LoadForgottenObject(CStateManager&, CInputStream&, CEntityInfo const&)
extern "C" void stub_100() asm("_Z19LoadForgottenObjectR13CStateManagerR12CInputStreamRK11CEntityInfo");
extern "C" void stub_100() {}

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

// fn_8023ACFC
extern "C" void stub_159() asm("fn_8023ACFC");
extern "C" void stub_159() {}

// kCAiSplashDenom
extern "C" void stub_160() asm("kCAiSplashDenom");
extern "C" void stub_160() {}

// lbl_4_rodata_0
extern "C" void stub_161() asm("lbl_4_rodata_0");
extern "C" void stub_161() {}

// lbl_8041AAB8
extern "C" void stub_162() asm("lbl_8041AAB8");
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

// operator*(CAxisAngle const&, float const&)
extern "C" void stub_171() asm("_ZmlRK10CAxisAngleRKf");
extern "C" void stub_171() {}

// operator+(CAxisAngle const&, CAxisAngle const&)
extern "C" void stub_172() asm("_ZplRK10CAxisAngleS1_");
extern "C" void stub_172() {}

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
