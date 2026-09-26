# `single-load` / `single-store` accessors: owning class, from evidence

Produced by `python3 tools/accessor_table.py`. The criterion is deliberately the
strict one, and this is what makes a row in the first table worth landing:

**A retail-named method `bl`s the accessor, and a forward abstract interpretation of
the caller's GPRs (`tools/recv.py`) says `r3` at that call site is still that method's
own incoming `this`.** The call site therefore passes the identical pointer, so the
accessor's class *is* that method's class. Nothing here is inferred from a name, an
offset coincidence, or a nearby symbol.

The second table lists the calls that **fail** the receiver test - `r3` was reloaded,
so the accessor belongs to a *member* object and the named caller is evidence about the
containing class only. Those are recorded and deliberately not claimed; they are the
false-positive class this repo has already been bitten by.

## Proven: class -> accessors

| class | accessors | contiguous runs | retail range(s) | offsets | evidence |
|---|---|---|---|---|---|
| **CAABox** | 13 | 1 | `0x802F8BC0..0x802F8CC4` | +0 +4 | `GetEdge__6CAABoxFQ26CAABox10EBoxEdgeId` (GetEdge__6CAABoxFQ26CAABox10EBoxEdgeId) at 0x802F8BA0 |
| **CPhysicsActor** | 10 | 3 | `0x800E9C24..0x800E9C54`, `0x800E9D64..0x800E9D7C`, `0x800E9D98..0x800E9DA8` | +344 +680 +684 +688 +692 +696 | `GetCoefficientOfRestitutionModifier__13CPhysicsActorCFv` (GetCoefficientOfRestitutionModifier__13CPhysicsActorCFv) at 0x800E9C44; `GetCollisionAccuracyModifier__13CPhysicsActorCFv` (GetCollisionAccuracyModifier__13CPhysicsActorCFv) at 0x800E9C34; `GetMaximumCollisionVelocity__13CPhysicsActorCFv` (GetMaximumCollisionVelocity__13CPhysicsActorCFv) at 0x800E9C24; `GetStepDownHeight__13CPhysicsActorCFv` (GetStepDownHeight__13CPhysicsActorCFv) at 0x800E9D74; `GetStepUpHeight__13CPhysicsActorCFv` (GetStepUpHeight__13CPhysicsActorCFv) at 0x800E9D64; `GetWeight__13CPhysicsActorCFv` (GetWeight__13CPhys |
| **CMoviePlayer** | 7 | 2 | `0x80317B88..0x80317B98`, `0x80318344..0x80318388` | +108 +112 +168 +228 +252 +256 +260 +268 | `GetHeight__12CMoviePlayerCFv` (GetHeight__12CMoviePlayerCFv) at 0x80317B88; `GetIsFullyCached__12CMoviePlayerCFv` (GetIsFullyCached__12CMoviePlayerCFv) at 0x80318344; `GetPlayMode__12CMoviePlayerCFv` (GetPlayMode__12CMoviePlayerCFv) at 0x80318378; `GetPlayedSeconds__12CMoviePlayerCFv` (GetPlayedSeconds__12CMoviePlayerCFv) at 0x80318360; `GetTotalSeconds__12CMoviePlayerCFv` (GetTotalSeconds__12CMoviePlayerCFv) at 0x80318370; `GetWidth__12CMoviePlayerCFv` (GetWidth__12CMoviePlayerCFv) at 0x80317B90; `SetPlayMode__12CMoviePlayerFQ212CMoviePlayer9EPlayMode` (SetPlayMode__12CMoviePlayerFQ212CMovie |
| **CActor** | 5 | 5 | `0x8004B640..0x8004B648`, `0x8004BA20..0x8004BA3C`, `0x8004C524..0x8004C53C`, `0x8004C54C..0x8004C564`, `0x8004C568..0x8004C574` | +8 +24 +128 +337 | `AcceptScriptMsg__3CAiFR13CStateManagerRC10CScriptMsg` (AcceptScriptMsg__3CAiFR13CStateManagerRC10CScriptMsg) at 0x80096F50; `GetCallTouch__6CActorCFv` (GetCallTouch__6CActorCFv) at 0x8004C524; `GetScannableObjectInfo__6CActorCFv` (GetScannableObjectInfo__6CActorCFv) at 0x8004B62C; `GetTouchBounds__6CActorCFv` (GetTouchBounds__6CActorCFv) at 0x8004C568; `GetUseInSortedLists__6CActorCFv` (GetUseInSortedLists__6CActorCFv) at 0x8004C54C; `SetMaterialFilter__6CActorFRC15CMaterialFilter` (SetMaterialFilter__6CActorFRC15CMaterialFilter) at 0x8004BA20 |
| **CPlayerState** | 5 | 5 | `0x80085224..0x80085234`, `0x80085470..0x80085480`, `0x80085498..0x800854B4`, `0x800854CC..0x800854DC`, `0x80085C18..0x80085C20` | +80 +96 +1424 | `FUN_80085c18__12CPlayerStateFUi` (FUN_80085c18__12CPlayerStateFUi) at 0x80085C18; `GetItemCapacity2__12CPlayerStateCFQ212CPlayerState9EItemType` (GetItemCapacity2__12CPlayerStateCFQ212CPlayerState9EItemType) at 0x80085458; `GetItemCapacity__12CPlayerStateCFQ212CPlayerState9EItemType` (GetItemCapacity__12CPlayerStateCFQ212CPlayerState9EItemType) at 0x800854B4; `GetVisorTransitionFactor__12CPlayerStateCFv` (GetVisorTransitionFactor__12CPlayerStateCFv) at 0x80085224; `HasPowerUp__12CPlayerStateCFQ212CPlayerState9EItemType` (HasPowerUp__12CPlayerStateCFQ212CPlayerState9EItemType) at 0x80085480 |
| **CPlayerGun** | 5 | 4 | `0x801C8B88..0x801C8B98`, `0x801C8DA8..0x801C8DB8`, `0x801C9414..0x801C9438`, `0x801C94C0..0x801C94CC` | +664 +916 +936 +1396 +1904 | `ButtonRelease__10CPlayerGunFR13CStateManagerRC12CTriggerData` (ButtonRelease__10CPlayerGunFR13CStateManagerRC12CTriggerData) at 0x801C94C0; `Grappling__10CPlayerGunFR13CStateManagerRC12CTriggerData` (Grappling__10CPlayerGunFR13CStateManagerRC12CTriggerData) at 0x801C8B88; `GunLoaded__10CPlayerGunFR13CStateManagerRC12CTriggerData` (GunLoaded__10CPlayerGunFR13CStateManagerRC12CTriggerData) at 0x801C8DA8; `IsHolstered__10CPlayerGunFR13CStateManagerRC12CTriggerData` (IsHolstered__10CPlayerGunFR13CStateManagerRC12CTriggerData) at 0x801C9428; `IsNotHolstered__10CPlayerGunFR13CStateManagerRC12CTrigg |
| **CTweakPlayer** | 5 | 2 | `0x80217D30..0x80217D54`, `0x802184CC..0x802184E4` | +0 +424 +428 +880 +884 +888 | `GetDarkSuitDamageReduction__12CTweakPlayerFv` (GetDarkSuitDamageReduction__12CTweakPlayerFv) at 0x80217D3C; `GetLeftAnalogMax__12CTweakPlayerFv` (GetLeftAnalogMax__12CTweakPlayerFv) at 0x802184D8; `GetLightSuitDamageReduction__12CTweakPlayerFv` (GetLightSuitDamageReduction__12CTweakPlayerFv) at 0x80217D30; `GetRightAnalogMax__12CTweakPlayerFv` (GetRightAnalogMax__12CTweakPlayerFv) at 0x802184CC; `GetVariaSuitDamageReduction__12CTweakPlayerFv` (GetVariaSuitDamageReduction__12CTweakPlayerFv) at 0x80217D48 |
| **CElementAllocationChunk** | 5 | 3 | `0x8032752C..0x80327540`, `0x80327560..0x8032756C`, `0x803275B0..0x80327600` | +0 +4 +8 | `CanAllocate__23CElementAllocationChunkCFUi` (CanAllocate__23CElementAllocationChunkCFUi) at 0x803275D8; `Contains__23CElementAllocationChunkCFPCv` (Contains__23CElementAllocationChunkCFPCv) at 0x803275B0; `GetAllocatedSize__23CElementAllocationChunkCFv` (GetAllocatedSize__23CElementAllocationChunkCFv) at 0x80327534; `GetAllocationCount__23CElementAllocationChunkCFv` (GetAllocationCount__23CElementAllocationChunkCFv) at 0x8032752C; `Rewind__23CElementAllocationChunkFUi` (Rewind__23CElementAllocationChunkFUi) at 0x80327540 |
| **CGameOptions** | 4 | 2 | `0x80160F40..0x80160F50`, `0x80160F84..0x80160F94` | +28 +32 | `EnsureOptions__12CGameOptionsFv` (EnsureOptions__12CGameOptionsFv) at 0x8016134C; `EnsureOptions__12CGameOptionsFv` (EnsureOptions__12CGameOptionsFv) at 0x80161358; `GetHelmetAlphaRaw__12CGameOptionsCFv` (GetHelmetAlphaRaw__12CGameOptionsCFv) at 0x80160F40; `GetHudAlphaRaw__12CGameOptionsCFv` (GetHudAlphaRaw__12CGameOptionsCFv) at 0x80160F8C; `SetHelmetAlpha__12CGameOptionsFi` (SetHelmetAlpha__12CGameOptionsFi) at 0x80160F48; `SetHudAlpha__12CGameOptionsFi` (SetHudAlpha__12CGameOptionsFi) at 0x80160F84 |
| **CRuleValue** | 4 | 1 | `0x801F5E04..0x801F5E38` | +0 +4 | `GetBool__10CRuleValueCFv` (GetBool__10CRuleValueCFv) at 0x801F5E14; `GetFloat__10CRuleValueCFv` (GetFloat__10CRuleValueCFv) at 0x801F5E04; `GetInt__10CRuleValueCFv` (GetInt__10CRuleValueCFv) at 0x801F5E0C; `__ct__10CRuleValueFiR12CInputStream` (__ct__10CRuleValueFiR12CInputStream) at 0x801F5E1C |
| **CPatterned** | 3 | 2 | `0x80073C58..0x80073C6C`, `0x80073C90..0x80073C9C` | +844 +1096 +1103 | `TakeDamage__10CPatternedFRC9CVector3ff` (TakeDamage__10CPatternedFRC9CVector3ff) at 0x80073C58; `VSlot50__10CPatternedFv` (VSlot50__10CPatternedFv) at 0x80073C64; `VSlot68__10CPatternedFv` (VSlot68__10CPatternedFv) at 0x80073C90 |
| **CEnvFxManager** | 3 | 1 | `0x801620A8..0x801620F0` | +52 +56 +4996 | `Play_801620A8__13CEnvFxManagerFv` (Play_801620A8__13CEnvFxManagerFv) at 0x801620A8; `SetDensity__13CEnvFxManagerFfi` (SetDensity__13CEnvFxManagerFfi) at 0x801620C0; `Stop_801620B4__13CEnvFxManagerFv` (Stop_801620B4__13CEnvFxManagerFv) at 0x801620B4 |
| **CCubeRenderer** | 3 | 3 | `0x8026E7C4..0x8026E7CC`, `0x8026E7E4..0x8026E7F0`, `0x8026EF24..0x8026EF30` | +152 +156 +180 +844 | `PrimColor__13CCubeRendererFRC6CColor` (PrimColor__13CCubeRendererFRC6CColor) at 0x8026EF24; `SetDebugOption__13CCubeRendererFQ29IRenderer12EDebugOptioni` (SetDebugOption__13CCubeRendererFQ29IRenderer12EDebugOptioni) at 0x8026E78C; `SetDrawableCallback__13CCubeRendererFPFPCvPCvi_vPCv` (SetDrawableCallback__13CCubeRendererFPFPCvPCvi_vPCv) at 0x8026E7E4 |
| **CVector2f** | 3 | 3 | `0x802CA304..0x802CA320`, `0x802CA440..0x802CA458`, `0x802CA564..0x802CA570` | +0 +4 | `AsNormalized__9CVector2fCFv` (AsNormalized__9CVector2fCFv) at 0x802CA424; `Dot__9CVector2fFRC9CVector2fRC9CVector2f` (Dot__9CVector2fFRC9CVector2fRC9CVector2f) at 0x802CA304; `GetAngleDiff__9CVector2fFRC9CVector2fRC9CVector2f` (GetAngleDiff__9CVector2fFRC9CVector2fRC9CVector2f) at 0x802CA380; `MagSquared__9CVector2fCFv` (MagSquared__9CVector2fCFv) at 0x802CA440; `__ct__9CVector2fFff` (__ct__9CVector2fFff) at 0x802CA564; `__dv__FRC9CVector2fRCf` (__dv__FRC9CVector2fRCf) at 0x802CA1E8; `__mi__FRC9CVector2fRC9CVector2f` (__mi__FRC9CVector2fRC9CVector2f) at 0x802CA2B8; `__ml__FRC9CVector2fRCf` (_ |
| **CEntity** | 3 | 3 | `0x800E9C14..0x800E9C1C`, `0x801861B0..0x801861B8`, `0x802179D8..0x802179E4` | +0 +40 +712 +4728 | `Teleport__7CPlayerFRC12CTransform4fR13CStateManagerb` (Teleport__7CPlayerFRC12CTransform4fR13CStateManagerb) at 0x8018657C; `Teleport__7CPlayerFRC12CTransform4fR13CStateManagerb` (Teleport__7CPlayerFRC12CTransform4fR13CStateManagerb) at 0x801866F4; `__ct__7CPlayerF9TUniqueIdRC12CTransform4fRC6CAABoxUiRC9CVector3fffffRC13CMaterialListP12CPlayerStateP14CCameraManagerbiii` (__ct__7CPlayerF9TUniqueIdRC12CTransform4fRC6CAABoxUiRC9CVector3fffffRC13CMaterialListP12CPlayerStateP14CCameraManagerbiii) at 0x8001C4A4 |
| **CStateManager** | 2 | 2 | `0x80036B50..0x80036B6C`, `0x80037180..0x80037194` | +5368 +9284 +9288 | `ApplyLocalDamage__13CStateManagerFRC9CVector3fRC9CVector3fR6CActorfRC9TUniqueIdRC9TUniqueIdRC11CDamageInfoi` (ApplyLocalDamage__13CStateManagerFRC9CVector3fRC9CVector3fR6CActorfRC9TUniqueIdRC9TUniqueIdRC11CDamageInfoi) at 0x8003D9AC; `DisplayAlertAboutOutOfAmmo__13CStateManagerCFRC7CPlayerQ212CPlayerState9EItemType` (DisplayAlertAboutOutOfAmmo__13CStateManagerCFRC7CPlayerQ212CPlayerState9EItemType) at 0x8003E970; `MaskUIdNumPlayers__13CStateManagerCF9TUniqueId` (MaskUIdNumPlayers__13CStateManagerCF9TUniqueId) at 0x80036B50; `SetBossParams__13CStateManagerF9TUniqueIdfUi` (SetBossParams__13CSta |
| **CArchitectureMessage** | 2 | 1 | `0x80048CE4..0x80048CF4` | +8 | `GetParm__20CArchitectureMessageCFv` (GetParm__20CArchitectureMessageCFv) at 0x80048CE4; `GetParm__20CArchitectureMessageFv` (GetParm__20CArchitectureMessageFv) at 0x80048CEC |
| **clear** | 2 | 2 | `0x80053EF0..0x80053EFC`, `0x80323548..0x80323554` | +4 | `__as__Q24rstl55vector<Q28CPakFile8SResInfo,Q24rstl17rmemory_allocator>FRCQ24rstl55vector<Q28CPakFile8SResInfo,Q24rstl17rmemory_allocator>` (__as__Q24rstl55vector<Q28CPakFile8SResInfo,Q24rstl17rmemory_allocator>FRCQ24rstl55vector<Q28CPakFile8SResInfo,Q24rstl17rmemory_allocator>) at 0x80323638; `clear__Q24rstl37vector<Ui,Q24rstl17rmemory_allocator>Fv` (clear__Q24rstl37vector<Ui,Q24rstl17rmemory_allocator>Fv) at 0x80053EF0; `clear__Q24rstl55vector<Q28CPakFile8SResInfo,Q24rstl17rmemory_allocator>Fv` (clear__Q24rstl55vector<Q28CPakFile8SResInfo,Q24rstl17rmemory_allocator>Fv) at 0x80323548 |
| **CScriptPickup** | 2 | 2 | `0x800B4570..0x800B4578`, `0x800B59A0..0x800B59BC` | +344 +368 | `GetItem__13CScriptPickupCFv` (GetItem__13CScriptPickupCFv) at 0x800B4570; `IsVisible__13CScriptPickupCFv` (IsVisible__13CScriptPickupCFv) at 0x800B5984 |
| **CGunWeapon** | 2 | 2 | `0x801CA070..0x801CA078`, `0x801D9B24..0x801D9B30` | +528 +624 | `EnableSecondaryFx__10CGunWeaponFQ210CGunWeapon16ESecondaryFxType` (EnableSecondaryFx__10CGunWeaponFQ210CGunWeapon16ESecondaryFxType) at 0x801CA070; `IsLoaded__10CGunWeaponCFv` (IsLoaded__10CGunWeaponCFv) at 0x801D9B24; `IsLoaded__10CPowerBeamCFv` (IsLoaded__10CPowerBeamCFv) at 0x801D5284; `Update__10CPowerBeamFfR13CStateManager` (Update__10CPowerBeamFfR13CStateManager) at 0x801D5568 |
| **CMatrix3f** | 2 | 1 | `0x802C62C4..0x802C631C` | +32 | `__as__9CMatrix3fFRC9CMatrix3f` (__as__9CMatrix3fFRC9CMatrix3f) at 0x802C62C4; `__ct__9CMatrix3fFRC9CMatrix3f` (__ct__9CMatrix3fFRC9CMatrix3f) at 0x802C62F0 |
| **CRandom16** | 2 | 1 | `0x802C8ABC..0x802C8ACC` | +0 | `SetSeed__9CRandom16FUi` (SetSeed__9CRandom16FUi) at 0x802C8ABC; `__ct__9CRandom16FUi` (__ct__9CRandom16FUi) at 0x802C8AC4 |
| **CResLoader** | 2 | 2 | `0x802FBC60..0x802FBC70`, `0x802FCCE4..0x802FCCF4` | +44 +68 +92 | `AreAllPaksLoaded__10CResLoaderCFv` (AreAllPaksLoaded__10CResLoaderCFv) at 0x802FCCE4; `GetPakCount__10CResLoaderCFv` (GetPakCount__10CResLoaderCFv) at 0x802FBC60 |
| **CCharAnimTime** | 2 | 2 | `0x80305FE8..0x80305FFC`, `0x803063EC..0x803063F8` | +0 +4 | `ZeroSignScale__13CCharAnimTimeCFf` (ZeroSignScale__13CCharAnimTimeCFf) at 0x80305F64; `__ct__13CCharAnimTimeFf` (__ct__13CCharAnimTimeFf) at 0x803063D0 |
| **CMayaSpline** | 2 | 2 | `0x803290C4..0x803290CC`, `0x80329130..0x80329144` | +12 +20 | `GetKnotCount__11CMayaSplineCFv` (GetKnotCount__11CMayaSplineCFv) at 0x803290C4; `GetMaxTime__11CMayaSplineCFv` (GetMaxTime__11CMayaSplineCFv) at 0x8032911C |
| **CMain** | 1 | 1 | `0x80005C64..0x80005C6C` | +72 | `AsyncIdle__5CMainFUi` (AsyncIdle__5CMainFUi) at 0x80005C1C; `RsMain__5CMainFiPCPCc` (RsMain__5CMainFiPCPCc) at 0x80006168; `SetFrameTimeMinimum__5CMainFi` (SetFrameTimeMinimum__5CMainFi) at 0x80005C64 |
| **CPlayer** | 1 | 1 | `0x8000D084..0x8000D08C` | +5048 | `GetPlayerIndex__7CPlayerCFv` (GetPlayerIndex__7CPlayerCFv) at 0x8000D084 |
| **CUnknownVec3List** | 1 | 1 | `0x8009D1E4..0x8009D1F0` | +12 | `ClearFlag__16CUnknownVec3ListFv` (ClearFlag__16CUnknownVec3ListFv) at 0x8009D1E4 |
| **CScriptSpawnPoint** | 1 | 1 | `0x800BA000..0x800BA010` | +88 | `GetItemAmount__17CScriptSpawnPointCFRCQ212CPlayerState9EItemType` (GetItemAmount__17CScriptSpawnPointCFRCQ212CPlayerState9EItemType) at 0x800B9FE8 |
| **CHUDMemoParms** | 1 | 1 | `0x800BABD4..0x800BABF4` | +8 | `EnabledForPlayer__13CHUDMemoParmsCFi` (EnabledForPlayer__13CHUDMemoParmsCFi) at 0x800BABD4 |
| **CSimpleShadow** | 1 | 1 | `0x800DF268..0x800DF274` | +72 | `Valid__13CSimpleShadowCFv` (Valid__13CSimpleShadowCFv) at 0x800DF268 |
| **CGameState** | 1 | 1 | `0x80142464..0x8014246C` | +412 | `GetGameMode__10CGameStateFv` (GetGameMode__10CGameStateFv) at 0x80142464 |
| **CCameraManager** | 1 | 1 | `0x801ABDB0..0x801ABDCC` | +22 | `IsInCinematicCamera__14CCameraManagerCFv` (IsInCinematicCamera__14CCameraManagerCFv) at 0x801ABDB0 |
| **CTweakGame** | 1 | 1 | `0x80216D20..0x80216D2C` | +0 +100 | `GetTotalPercentage__10CTweakGameFv` (GetTotalPercentage__10CTweakGameFv) at 0x80216D20 |
| **LoadTypedefSLdrPlayerItem** | 1 | 1 | `0x8023E0C4..0x8023E0DC` | +0 | `LoadTypedefSLdrPlayerItem__FR14SLdrPlayerItemR12CInputStream` (LoadTypedefSLdrPlayerItem__FR14SLdrPlayerItemR12CInputStream) at 0x8023E0C4 |
| **SLdrScannableParameters** | 1 | 1 | `0x8024156C..0x80241578` | +0 | `__ct__23SLdrScannableParametersFv` (__ct__23SLdrScannableParametersFv) at 0x8024156C |
| **CCallStack** | 1 | 1 | `0x8028BFE8..0x8028BFF4` | +0 +4 | `__ct__10CCallStackFUiPCcPCc` (__ct__10CCallStackFUiPCcPCc) at 0x8028BFE8 |
| **CSegId** | 1 | 1 | `0x802B1D10..0x802B1D28` | +0 | `__ct__6CSegIdFR12CInputStream` (__ct__6CSegIdFR12CInputStream) at 0x802B1D10 |
| **CGX** | 1 | 1 | `0x802BE220..0x802BE234` | +56 | `GetChanAmbColor__3CGXFQ23CGX10EChannelId` (GetChanAmbColor__3CGXFQ23CGX10EChannelId) at 0x802BE220 |
| **CVector2i** | 1 | 1 | `0x802CA6E0..0x802CA6EC` | +0 +4 | `__ct__9CVector2iFii` (__ct__9CVector2iFii) at 0x802CA6E0; `__dv__FRC9CVector2ii` (__dv__FRC9CVector2ii) at 0x802CA5CC; `__mi__FRC9CVector2iRC9CVector2i` (__mi__FRC9CVector2iRC9CVector2i) at 0x802CA694; `__ml__FRC9CVector2ii` (__ml__FRC9CVector2ii) at 0x802CA5FC; `__pl__FRC9CVector2iRC9CVector2i` (__pl__FRC9CVector2iRC9CVector2i) at 0x802CA6CC |
| **CPVSVisSet** | 1 | 1 | `0x802CEE10..0x802CEE24` | +0 +1 | `GetVisible__10CPVSVisSetCFi` (GetVisible__10CPVSVisSetCFi) at 0x802CED7C |
| **IController** | 1 | 1 | `0x8030B544..0x8030B554` | +0 | `__ct__11IControllerFv` (__ct__11IControllerFv) at 0x8030B544; `__ct__18CDolphinControllerFv` (__ct__18CDolphinControllerFv) at 0x8030BE68 |
| **CDolphinController** | 1 | 1 | `0x8030B5BC..0x8030B5CC` | +420 | `GetControllerType__18CDolphinControllerCFi` (GetControllerType__18CDolphinControllerCFi) at 0x8030B5BC |
| **CControllerAxis** | 1 | 1 | `0x8030BF8C..0x8030BF9C` | +0 +4 | `__ct__15CControllerAxisFv` (__ct__15CControllerAxisFv) at 0x8030BF8C |
| **SMediumAllocPuddle** | 1 | 1 | `0x8030CD6C..0x8030CD8C` | +0 +1 | `InitBookKeeping__18SMediumAllocPuddleFPUcUs` (InitBookKeeping__18SMediumAllocPuddleFPUcUs) at 0x8030CD1C |
| **CMediumAllocPool** | 1 | 1 | `0x8030D514..0x8030D528` | +20 | `HasPuddles__16CMediumAllocPoolCFv` (HasPuddles__16CMediumAllocPoolCFv) at 0x8030D514 |
| **CGameAllocator** | 1 | 1 | `0x8030DE98..0x8030DEA4` | +88 +92 | `SetOutOfMemoryCallback__14CGameAllocatorFPFPCvUi_CbPCv` (SetOutOfMemoryCallback__14CGameAllocatorFPFPCvUi_CbPCv) at 0x8030DE98 |
| **CStringTable** | 1 | 1 | `0x80312698..0x803126A8` | +16 | `GetString__12CStringTableCFi` (GetString__12CStringTableCFi) at 0x80312678 |
| **CDependencyGroupToken** | 1 | 1 | `0x80320964..0x80320978` | +24 | `IsLocked__21CDependencyGroupTokenCFv` (IsLocked__21CDependencyGroupTokenCFv) at 0x80320964 |
| **SetGroupedSize** | 1 | 1 | `0x80324678..0x80324684` | +10 | `SetGroupedSize__Q28CPakFile8SResInfoFUi` (SetGroupedSize__Q28CPakFile8SResInfoFUi) at 0x80324678 |
| **GetGroupedSize** | 1 | 1 | `0x80324684..0x80324690` | +10 | `GetGroupedSize__Q28CPakFile8SResInfoCFv` (GetGroupedSize__Q28CPakFile8SResInfoCFv) at 0x80324684 |
| **IsCompressed** | 1 | 1 | `0x80324690..0x803246A8` | +4 | `IsCompressed__Q28CPakFile8SResInfoCFv` (IsCompressed__Q28CPakFile8SResInfoCFv) at 0x80324690 |
| **CBitStreamWriter** | 1 | 1 | `0x80342D14..0x80342D30` | +0 +8 | `GetWrittenBits__16CBitStreamWriterCFv` (GetWrittenBits__16CBitStreamWriterCFv) at 0x80342D14 |
| **CBitStreamReader** | 1 | 1 | `0x80342DC8..0x80342DD4` | +8 | `Flush__16CBitStreamReaderFv` (Flush__16CBitStreamReaderFv) at 0x80342DC8; `GetInputStream__16CBitStreamReaderFv` (GetInputStream__16CBitStreamReaderFv) at 0x80342DAC |
| **CInputStream** | 1 | 1 | `0x80342F6C..0x80342F90` | +0 +4 | `ReleaseBuffer__12CInputStreamFv` (ReleaseBuffer__12CInputStreamFv) at 0x80342F6C |

**128 accessors placed, in 55 classes.**

## Contradicted: the accessor is a member, not the caller's class

| named caller | accessor | why it is not the class |
|---|---|---|
| `CalculateTangents__15CMayaSplineKnotFP15CMayaSplineKnotP15CMayaSplineKnot` (CalculateTangents__15CMayaSplineKnotFP15CMayaSplineKnotP15CMayaSplineKnot) | `0x802CA564` | r3=N at 0x80329F28; r3=N at 0x80329F5C; r3=N at 0x80329FB0; r3=N at 0x80329FD4; r3=N at 0x8032A0B0; r3=N at 0x8032A0E4; r3=N at 0x8032A138; r3=N at 0x8032A15C; r3=N at 0x8032A194; r3=N at 0x8032A1A4; r3=N at 0x8032A1D4; r3=N at 0x8032A220; r3=N at 0x8032A2D8; r3=N at 0x8032A2F8; r3=N at 0x8032A31C; r3=N at 0x8032A33C |
| `__ct__7CPlayerF9TUniqueIdRC12CTransform4fRC6CAABoxUiRC9CVector3fffffRC13CMaterialListP12CPlayerStateP14CCameraManagerbiii` (__ct__7CPlayerF9TUniqueIdRC12CTransform4fRC6CAABoxUiRC9CVector3fffffRC13CMaterialListP12CPlayerStateP14CCameraManagerbiii) | `0x80027030`, `0x8022EB90`, `0x8028BFE8`, `0x802CA564`, `0x802CA6E0` | r3=N at 0x8001B73C; r3=N at 0x8001B784; r3=N at 0x8001BB6C; r3=N at 0x8001BC34; r3=N at 0x8001BEE8; r3=N at 0x8001BF24; r3=N at 0x8001BF60; r3=N at 0x8001C458 |
| `DisplayAlertAboutOutOfAmmo__13CStateManagerCFRC7CPlayerQ212CPlayerState9EItemType` (DisplayAlertAboutOutOfAmmo__13CStateManagerCFRC7CPlayerQ212CPlayerState9EItemType) | `0x8000B528`, `0x8000D084` | r3=N at 0x8003E8D8; r3=U at 0x8003E8F8; r3=U at 0x8003E938; r3=U at 0x8003E9B8; r3=U at 0x8003EA54; r3=U at 0x8003EA80; r3=U at 0x8003EB1C |
| `FindControlPoints__11CMayaSplineFiRQ24rstl29reserved_vector<9CVector2f,4>` (FindControlPoints__11CMayaSplineFiRQ24rstl29reserved_vector<9CVector2f,4>) | `0x802CA564` | r3=N at 0x80328EBC; r3=N at 0x80328EF4; r3=N at 0x80328F04; r3=N at 0x80328FC0; r3=N at 0x80328FD0; r3=N at 0x80329030 |
| `Initialize__14CGameAllocatorFR10COsContext` (Initialize__14CGameAllocatorFR10COsContext) | `0x8028BFE8` | r3=N at 0x8030EC00; r3=N at 0x8030EC48; r3=N at 0x8030EC94; r3=N at 0x8030ECFC; r3=N at 0x8030ED58 |
| `ApplyLocalDamage__13CStateManagerFRC9CVector3fRC9CVector3fR6CActorfRC9TUniqueIdRC9TUniqueIdRC11CDamageInfoi` (ApplyLocalDamage__13CStateManagerFRC9CVector3fRC9CVector3fR6CActorfRC9TUniqueIdRC9TUniqueIdRC11CDamageInfoi) | `0x8014246C`, `0x801ABDB0`, `0x80217D30`, `0x80217D3C`, `0x80217D48` | r3=N at 0x8003D9C0; r3=N at 0x8003DC44; r3=U at 0x8003DA50; r3=U at 0x8003DA74; r3=U at 0x8003DAA4 |
| `InitializeTextures__12CMoviePlayerFv` (InitializeTextures__12CMoviePlayerFv) | `0x8028BFE8` | r3=N at 0x80319250; r3=N at 0x80319280; r3=N at 0x803192B0; r3=N at 0x803192E0 |
| `EvaluateInfinities__11CMayaSplineFfb` (EvaluateInfinities__11CMayaSplineFfb) | `0x802CA564` | r3=N at 0x80329908; r3=N at 0x80329918; r3=N at 0x803299F0; r3=N at 0x80329A00 |
| `__ct__15CMayaSplineKnotFffiiRCfRCf` (__ct__15CMayaSplineKnotFffiiRCfRCf) | `0x802CA564` | r3=N at 0x8032A548; r3=N at 0x8032A568; r3=N at 0x8032A5AC; r3=N at 0x8032A5F0 |
| `FixupAllocPtrs__14CGameAllocatorFPQ214CGameAllocator12SGameMemInfoUiUiQ210IAllocator5EHintRC10CCallStack` (FixupAllocPtrs__14CGameAllocatorFPQ214CGameAllocator12SGameMemInfoUiUiQ210IAllocator5EHintRC10CCallStack) | `0x8028BFD8`, `0x8028BFE0` | r3=U at 0x8030E430; r3=U at 0x8030E43C; r3=U at 0x8030E478; r3=U at 0x8030E484 |
| `__ct__18CStaticAudioPlayerFRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>ii` (__ct__18CStaticAudioPlayerFRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>ii) | `0x8028BFE8` | r3=N at 0x80326E68; r3=N at 0x80326EB0; r3=N at 0x80326F88 |
| `__ct__11CElementGenF25TToken<15CGenDescription>Q211CElementGen21EModelOrientationTypeQ211CElementGen20EOptionalSystemFlags` (__ct__11CElementGenF25TToken<15CGenDescription>Q211CElementGen21EModelOrientationTypeQ211CElementGen20EOptionalSystemFlags) | `0x802C62F0`, `0x802C8ABC`, `0x802C8AC4` | r3=N at 0x802DB2B0; r3=N at 0x802DB368; r3=N at 0x802DB508 |
| `AcceptScriptMsg__21CScriptAreaPropertiesFR13CStateManagerR10CScriptMsg` (AcceptScriptMsg__21CScriptAreaPropertiesFR13CStateManagerR10CScriptMsg) | `0x801620A8`, `0x801620B4`, `0x801620C0` | r3=N at 0x8013C6B8; r3=N at 0x8013C6C4; r3=N at 0x8013C6D0 |
| `__ct__10CPlayerGunF9TUniqueIdi` (__ct__10CPlayerGunF9TUniqueIdi) | `0x80214D04`, `0x80214D10`, `0x80214DA0` | r3=N at 0x801D2354; r3=N at 0x801D2360; r3=N at 0x801D2374 |
| `Free__Q28IElement17CElementAllocatorFPvUl` (Free__Q28IElement17CElementAllocatorFPvUl) | `0x8032752C`, `0x803275B0` | r3=N at 0x802E7C94; r3=N at 0x802E7CC0; r3=N at 0x802E7CF8 |
| `__OSThreadInit` (__OSThreadInit) | `0x80375364` | r3=N at 0x80375268; r3=N at 0x803752EC; r3=N at 0x80375308 |
| `AddPaksAndFactories__18CGameGlobalObjectsFv` (AddPaksAndFactories__18CGameGlobalObjectsFv) | `0x8028BFE8`, `0x802FCCE4` | r3=N at 0x80007218; r3=N at 0x80007484 |
| `StartARAMFileLoad__8CDvdFileFv` (StartARAMFileLoad__8CDvdFileFv) | `0x8028BFE8` | r3=N at 0x8030C5DC; r3=N at 0x8030C668 |
| `Alloc__14CGameAllocatorFUlQ210IAllocator5EHintQ210IAllocator6EScopeQ210IAllocator5ETypeRC10CCallStack` (Alloc__14CGameAllocatorFUlQ210IAllocator5EHintQ210IAllocator6EScopeQ210IAllocator5ETypeRC10CCallStack) | `0x8028BFE8`, `0x8030D514` | r3=N at 0x8030E7B8; r3=N at 0x8030E80C |
| `__ct__24CGameArchitectureSupportFR10COsContext` (__ct__24CGameArchitectureSupportFR10COsContext) | `0x802184CC`, `0x802184D8` | r3=N at 0x80007F40; r3=N at 0x80007F4C |
| `__dt__13CStateManagerFv` (__dt__13CStateManagerFv) | `0x8000B528` | r3=N at 0x80042748; r3=N at 0x800427D0 |
| `Think__13CScriptPickupFfR13CStateManager` (Think__13CScriptPickupFfR13CStateManager) | `0x801ABDB0`, `0x801B0B10` | r3=N at 0x800B4DCC; r3=N at 0x800B5824 |
| `ProcessSoundEvent__6CActorFififfRC6CSegIdUsUsfUcUcfRC9CVector3fiR13CStateManagerb` (ProcessSoundEvent__6CActorFififfRC6CSegIdUsUsfUcUcfRC9CVector3fiR13CStateManagerb) | `0x8000D09C`, `0x8032B068` | r3=N at 0x8004AEC8; r3=N at 0x8004AFD4 |
| `SetViewportOrtho__13CCubeRendererFbff` (SetViewportOrtho__13CCubeRendererFbff) | `0x802CA564` | r3=N at 0x8026EB98; r3=N at 0x8026EBA8 |
| `__ct__15CMayaSplineKnotFR12CInputStream` (__ct__15CMayaSplineKnotFR12CInputStream) | `0x802CA564` | r3=N at 0x8032A6BC; r3=N at 0x8032A6DC |
| `__ct__13CStateManagerFRCQ24rstl26ncrc_ptr<14CScriptMailbox>RCQ24rstl25ncrc_ptr<13CMapWorldInfo>RCQ24rstl24ncrc_ptr<12CPlayerState>RCQ24rstl30ncrc_ptr<18CWorldTransManager>` (__ct__13CStateManagerFRCQ24rstl26ncrc_ptr<14CScriptMailbox>RCQ24rstl25ncrc_ptr<13CMapWorldInfo>RCQ24rstl24ncrc_ptr<12CPlayerState>RCQ24rstl30ncrc_ptr<18CWorldTransManager>) | `0x80142464`, `0x802C8AC4` | r3=N at 0x80043FB8; r3=N at 0x80044C5C |
| `ReallyRenderFogVolume__13CCubeRendererFRC6CColorRC6CAABoxPC6CModelPC13CSkinnedModel` (ReallyRenderFogVolume__13CCubeRendererFRC6CColorRC6CAABoxPC6CModelPC13CSkinnedModel) | `0x802CA6E0` | r3=N at 0x8026BF90; r3=N at 0x8026BFA0 |
| `RebuildResourceLists__8CPakFileFRCQ24rstl55vector<Q28CPakFile8SResInfo,Q24rstl17rmemory_allocator>` (RebuildResourceLists__8CPakFileFRCQ24rstl55vector<Q28CPakFile8SResInfo,Q24rstl17rmemory_allocator>) | `0x80053EF0`, `0x80323548` | r3=N at 0x8032328C; r3=N at 0x803232A4 |
| `__ct__10SLdrPickupFv` (__ct__10SLdrPickupFv) | `0x8023E118`, `0x802422E4` | r3=N at 0x800B410C; r3=N at 0x800B411C |
| `__ct__10CPatternedF12EPatternedAI9TUniqueIdRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>Q210CPatterned11EFlavorTypeRC11CEntityInfoRC12CTransform4fRC10CModelDataRC14CPatternedInfoQ210CPatterned13EMovementTypeQ210CPatterned13EColliderType9EBodyTypeRC16CActorParameters` (__ct__10CPatternedF12EPatternedAI9TUniqueIdRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>Q210CPatterned11EFlavorTypeRC11CEntityInfoRC12CTransform4fRC10CModelDataRC14CPatternedInfoQ210CPatterned13EMovementTypeQ210CPatterned13EColliderType9EBodyTypeRC16CActorParameters) | `0x800FA748`, `0x801B7420` | r3=N at 0x8007A20C; r3=N at 0x8007A220 |
| `__ct__20CConsoleOutputWindowFiff` (__ct__20CConsoleOutputWindowFiff) | `0x802BAD0C`, `0x802BAD6C` | r3=N at 0x800D6468; r3=N at 0x800D6494 |
| `Update__10CGunWeaponFfR13CStateManager` (Update__10CGunWeaponFfR13CStateManager) | `0x80028230` | r3=N at 0x801DB354; r3=N at 0x801DB3A0 |
| `Fidgeting__10CPlayerGunFR13CStateManager9EStateMsgf` (Fidgeting__10CPlayerGunFR13CStateManager9EStateMsgf) | `0x801C6D58` | r3=N at 0x801C6FB4; r3=N at 0x801C6FE4 |
| `UpdateNormalShotCycle__10CPlayerGunFfR13CStateManager` (UpdateNormalShotCycle__10CPlayerGunFfR13CStateManager) | `0x80214DB8` | r3=N at 0x801CC82C; r3=N at 0x801CC9D8 |
| `Fire__10CGunWeaponFR6CTokenbfQ212CPlayerState12EChargeStageRC12CTransform4fR13CStateManager9TUniqueIdiUs9TUniqueId10CSfxHandleff` (Fire__10CGunWeaponFR6CTokenbfQ212CPlayerState12EChargeStageRC12CTransform4fR13CStateManager9TUniqueIdiUs9TUniqueId10CSfxHandleff) | `0x80214DB8` | r3=N at 0x801DA594; r3=N at 0x801DA5A4 |
| `CARDInit` (CARDInit) | `0x8036E43C`, `0x80375364` | r3=N at 0x80358234; r3=N at 0x8035823C |
| `Read` (Read) | `0x8036E43C` | r3=N at 0x8035F414; r3=N at 0x8035F448 |
| `stateReady` (stateReady) | `0x8036E43C` | r3=N at 0x80361808; r3=N at 0x803618F4 |
| `CARDFreeBlocks` (CARDFreeBlocks) | `0x80359B50`, `0x80359F54` | r3=N at 0x80358444; r3=N at 0x80358450 |
| `CARDCreateAsync` (CARDCreateAsync) | `0x80359B50`, `0x80359F54` | r3=N at 0x8035C9E4; r3=N at 0x8035CAAC |
| `__CARDSeek` (__CARDSeek) | `0x80359B50`, `0x80359F54` | r3=N at 0x8035CC04; r3=N at 0x8035CC9C |
| `WriteCallback` (WriteCallback) | `0x80359B50`, `0x80359F54` | r3=N at 0x8035D060; r3=N at 0x8035D0B0 |
| `CARDOpen` (CARDOpen) | `0x80359F54` | r3=N at 0x8035C70C; r3=N at 0x8035C76C |
| `EnsureWorldPaksReady__5CMainFv` (EnsureWorldPaksReady__5CMainFv) | `0x802FBC60` | r3=N at 0x80005674 |
| `SomethingWorldId_80005698` (SomethingWorldId_80005698) | `0x802FBC60` | r3=N at 0x80005748 |
| `RsMain__5CMainFiPCPCc` (RsMain__5CMainFiPCPCc) | `0x802FCCE4` | r3=N at 0x80006094 |
| `UpdateTicks__24CGameArchitectureSupportFv` (UpdateTicks__24CGameArchitectureSupportFv) | `0x80008A1C` | r3=N at 0x80007C7C |
| `Play__20CScriptStreamedMusicFR13CStateManager` (Play__20CScriptStreamedMusicFR13CStateManager) | `0x80008A1C` | r3=N at 0x8015DA5C |
| `__nwa__FUlPCcPCc` (__nwa__FUlPCcPCc) | `0x8028BFE8` | r3=N at 0x802CE248 |
| `__nw__FUlPCcPCc` (__nw__FUlPCcPCc) | `0x8028BFE8` | r3=N at 0x802CE29C |
| `reserve__Q24rstl37vector<Uc,Q24rstl17rmemory_allocator>Fi` (reserve__Q24rstl37vector<Uc,Q24rstl17rmemory_allocator>Fi) | `0x8028BFE8` | r3=N at 0x8030B344 |
| `LoadToMRAM__10CARAMTokenFv` (LoadToMRAM__10CARAMTokenFv) | `0x8028BFE8` | r3=N at 0x80314A30 |
| `PrefetchNextFrame__12CMoviePlayerFv` (PrefetchNextFrame__12CMoviePlayerFv) | `0x8028BFE8` | r3=N at 0x80318D0C |
| `PostDVDReadRequestIfNeeded__12CMoviePlayerFv` (PostDVDReadRequestIfNeeded__12CMoviePlayerFv) | `0x8028BFE8` | r3=N at 0x80319068 |
| `__ct__12CMoviePlayerFPCcfbb` (__ct__12CMoviePlayerFPCcfbb) | `0x8028BFE8` | r3=N at 0x80319E30 |
| `reserve__Q24rstl55vector<Q28CPakFile8SResInfo,Q24rstl17rmemory_allocator>Fi` (reserve__Q24rstl55vector<Q28CPakFile8SResInfo,Q24rstl17rmemory_allocator>Fi) | `0x8028BFE8` | r3=N at 0x80324AB8 |
| `get_buffer_and_size__FR12CInputStreamUlUl` (get_buffer_and_size__FR12CInputStreamUlUl) | `0x8028BFE8` | r3=N at 0x80344200 |
| `__ct__16CFilePreloadDataFRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>` (__ct__16CFilePreloadDataFRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>) | `0x8028BFE8` | r3=N at 0x80344A00 |
| `AcceptScriptMsg__17CScriptSpawnPointFR13CStateManagerRC10CScriptMsg` (AcceptScriptMsg__17CScriptSpawnPointFR13CStateManagerRC10CScriptMsg) | `0x801ABDB0` | r3=N at 0x800B9F34 |
| `InCinematic__10CPlayerGunFR13CStateManagerRC12CTriggerData` (InCinematic__10CPlayerGunFR13CStateManagerRC12CTriggerData) | `0x801ABDB0` | r3=N at 0x801C8D60 |
| `ShouldHolster__10CPlayerGunFR13CStateManagerRC12CTriggerData` (ShouldHolster__10CPlayerGunFR13CStateManagerRC12CTriggerData) | `0x801ABDB0` | r3=N at 0x801C94A0 |
| `Update__Q210CPlayerGun9CGunMorphFfffRC7CPlayer` (Update__Q210CPlayerGun9CGunMorphFfffRC7CPlayer) | `0x801ABDB0` | r3=N at 0x801CDE64 |
| `__sinit_CloseEnough_cpp` (__sinit_CloseEnough_cpp) | `0x802CA564` | r3=N at 0x802C62B0 |
| `__sinit_CVector2f_cpp` (__sinit_CVector2f_cpp) | `0x802CA564` | r3=N at 0x802CA588 |
| `SetGameState__9CMainFlowF17EClientFlowStatesR18CArchitectureQueue` (SetGameState__9CMainFlowF17EClientFlowStatesR18CArchitectureQueue) | `0x80142464` | r3=N at 0x8001DD4C |
| `OnMessage__9CMainFlowFRC20CArchitectureMessageR18CArchitectureQueue` (OnMessage__9CMainFlowFRC20CArchitectureMessageR18CArchitectureQueue) | `0x80048CE4` | r3=U at 0x8001DFA8 |
| `__ct__13CPhysicsActorF9TUniqueIdRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>RC11CEntityInfoUiRC12CTransform4fRC10CModelDataRC13CMaterialListRC6CAABoxRC10SMoverDataRC16CActorParametersRC8StepData` (__ct__13CPhysicsActorF9TUniqueIdRCQ24rstl66basic_string<c,Q24rstl14char_traits<c>,Q24rstl17rmemory_allocator>RC11CEntityInfoUiRC12CTransform4fRC10CModelDataRC13CMaterialListRC6CAABoxRC10SMoverDataRC16CActorParametersRC8StepData) | `0x802C62F0` | r3=N at 0x800EBA9C |
| `__sinit_CMatrix3f_cpp` (__sinit_CMatrix3f_cpp) | `0x802C62F0` | r3=N at 0x802C6ADC |
| `Touch__13CScriptPickupFR6CActorR13CStateManager` (Touch__13CScriptPickupFR6CActorR13CStateManager) | `0x80036B50` | r3=U at 0x800B47D8 |
| `AcceptScriptMsg__14CScriptHUDMemoFR13CStateManagerR10CScriptMsg` (AcceptScriptMsg__14CScriptHUDMemoFR13CStateManagerR10CScriptMsg) | `0x80036B50` | r3=U at 0x800BB074 |
| `StopChargeSound__10CPlayerGunFR13CStateManagerb` (StopChargeSound__10CPlayerGunFR13CStateManagerb) | `0x80036B50` | r3=U at 0x801CB06C |
| `UpdateChargeState__10CPlayerGunFfR13CStateManager` (UpdateChargeState__10CPlayerGunFfR13CStateManager) | `0x80036B50` | r3=U at 0x801CCF78 |
| `Unk9__10CGunWeaponFR13CStateManager` (Unk9__10CGunWeaponFR13CStateManager) | `0x80036B50` | r3=U at 0x801D8518 |
| `SetCurrentAreaId__13CStateManagerF7TAreaId` (SetCurrentAreaId__13CStateManagerF7TAreaId) | `0x8005075C` | r3=N at 0x800417A8 |
| `UpdateActorInSortedLists__13CStateManagerFP6CActor` (UpdateActorInSortedLists__13CStateManagerFP6CActor) | `0x8004C54C` | r3=U at 0x80041B44 |
| `AddToRenderer__6CActorCFRC13CStateManager` (AddToRenderer__6CActorCFRC13CStateManager) | `0x800DF268` | r3=N at 0x8004CC60 |
| `ComputeDerivedQuantities__13CPhysicsActorFv` (ComputeDerivedQuantities__13CPhysicsActorFv) | `0x802C62C4` | r3=N at 0x800EB584 |
| `DisplayHudMemo__9CSamusHudFRCQ24rstl66basic_string<w,Q24rstl14char_traits<w>,Q24rstl17rmemory_allocator>RC13CHUDMemoParms` (DisplayHudMemo__9CSamusHudFRCQ24rstl66basic_string<w,Q24rstl14char_traits<w>,Q24rstl17rmemory_allocator>RC13CHUDMemoParms) | `0x800BABD4` | r3=U at 0x8006B400 |
| `LoadScanTreeInventory__FPiR12CInputStream` (LoadScanTreeInventory__FPiR12CInputStream) | `0x8024156C` | r3=N at 0x8020EC34 |
| `GetTotalPickupCount__12CPlayerStateCFv` (GetTotalPickupCount__12CPlayerStateCFv) | `0x80216D20` | r3=N at 0x80084C5C |
| `LoadPickup__FR13CStateManagerR12CInputStreamRC11CEntityInfo` (LoadPickup__FR13CStateManagerR12CInputStreamRC11CEntityInfo) | `0x8023E0C4` | r3=N at 0x800B3B80 |
| `LoadActorParameters__FRC19SLdrActorParameters` (LoadActorParameters__FRC19SLdrActorParameters) | `0x8023AC94` | r3=N at 0x8023AB78 |
| `GetHardModeDamageMultiplier__10CGameStateCFv` (GetHardModeDamageMultiplier__10CGameStateCFv) | `0x80216D38` | r3=N at 0x801424A8 |
| `__ct__12CGameOptionsFv` (__ct__12CGameOptionsFv) | `0x80227694` | r3=N at 0x80161BF0 |
| `InitiateCombo__10CPlayerGunFR13CStateManagerRC12CTriggerData` (InitiateCombo__10CPlayerGunFR13CStateManagerRC12CTriggerData) | `0x80214DB8` | r3=N at 0x801C90E0 |
| `__ct__11CRuleActionFR12CInputStream` (__ct__11CRuleActionFR12CInputStream) | `0x801F5E1C` | r3=N at 0x801F5EB4 |
| `__ct__14CRuleConditionFR12CInputStream` (__ct__14CRuleConditionFR12CInputStream) | `0x801F5E1C` | r3=N at 0x801F5FF4 |
| `__ct__Q24rstl42vector<6CSegId,Q24rstl17rmemory_allocator>FR12CInputStreamRCQ24rstl17rmemory_allocator` (__ct__Q24rstl42vector<6CSegId,Q24rstl17rmemory_allocator>FR12CInputStreamRCQ24rstl17rmemory_allocator) | `0x802B1D10` | r3=N at 0x802AC28C |
| `Alloc__Q28IElement17CElementAllocatorFUlPCcPCc` (Alloc__Q28IElement17CElementAllocatorFUlPCcPCc) | `0x803275D8` | r3=N at 0x802E7DA8 |
| `__ct__9CAudioSysFccccUi` (__ct__9CAudioSysFccccUi) | `0x803A1D30` | r3=N at 0x80308A74 |
| `LoadResourceTable__8CPakFileFR15CMemoryInStream` (LoadResourceTable__8CPakFileFR15CMemoryInStream) | `0x80324678` | r3=N at 0x80323D18 |
| `VIInit` (VIInit) | `0x80375364` | r3=N at 0x80353948 |
| `DVDInit` (DVDInit) | `0x80375364` | r3=N at 0x80360708 |
| `__GXPEInit` (__GXPEInit) | `0x80375364` | r3=N at 0x80368E14 |
| `DVDLowRead` (DVDLowRead) | `0x8036E43C` | r3=N at 0x8035F750 |
| `DVDLowSeek` (DVDLowSeek) | `0x8036E43C` | r3=N at 0x8035F7F8 |
| `DVDLowReadDiskID` (DVDLowReadDiskID) | `0x8036E43C` | r3=N at 0x8035F8C8 |
| `DVDLowStopMotor` (DVDLowStopMotor) | `0x8036E43C` | r3=N at 0x8035F954 |
| `DVDLowRequestError` (DVDLowRequestError) | `0x8036E43C` | r3=N at 0x8035F9E0 |
| `DVDLowInquiry` (DVDLowInquiry) | `0x8036E43C` | r3=N at 0x8035FA7C |
| `DVDLowAudioStream` (DVDLowAudioStream) | `0x8036E43C` | r3=N at 0x8035FB14 |
| `DVDLowRequestAudioStatus` (DVDLowRequestAudioStatus) | `0x8036E43C` | r3=N at 0x8035FBA0 |
| `DVDLowAudioBufferConfig` (DVDLowAudioBufferConfig) | `0x8036E43C` | r3=N at 0x8035FC3C |
| `stateCoverClosed` (stateCoverClosed) | `0x8036E43C` | r3=N at 0x80361464 |
| `cbForStateMotorStopped` (cbForStateMotorStopped) | `0x8036E43C` | r3=N at 0x8036160C |
| `ReadCallback` (ReadCallback) | `0x80359B50` | r3=N at 0x8035CDB0 |
| `CreateCallbackFat` (CreateCallbackFat) | `0x80359F54` | r3=N at 0x8035C86C |
| `CARDReadAsync` (CARDReadAsync) | `0x80359F54` | r3=N at 0x8035CED0 |
| `CARDWriteAsync` (CARDWriteAsync) | `0x80359F54` | r3=N at 0x8035D27C |
| `CARDFastDeleteAsync` (CARDFastDeleteAsync) | `0x80359F54` | r3=N at 0x8035D424 |
| `CARDGetStatus` (CARDGetStatus) | `0x80359F54` | r3=N at 0x8035D73C |
| `CARDSetStatusAsync` (CARDSetStatusAsync) | `0x80359F54` | r3=N at 0x8035D894 |
| `__OSBootDol` (__OSBootDol) | `0x80373C24` | r3=N at 0x80370B94 |

**205 further accessor calls are named but contradicted by the receiver test.**

## Single caller: owner is one of these, not decided

A `this`-verified call proves the owner is an **ancestor-or-self** of the caller, so
one caller cannot pin it down. These are reported as candidate sets rather than
guesses - `CPowerBeam` is the live example, and the repo's own
`src/MetroidPrime/Weapons/CGunWeaponIsLoaded.cpp` shows what guessing costs.

| accessor | only caller | owner is one of | note |
|---|---|---|---|
| `0x80008A1C` | `CMain` | `CMain` | single caller; the derived class inherits the method |
| `0x80366F28` | `GXInitFifoBase` | `GXInitFifoBase` | single caller; the derived class inherits the method |
| `0x8036A784` | `__GXDefaultTexRegionCallback` | `__GXDefaultTexRegionCallback` | single caller; the derived class inherits the method |
| `0x8036A78C` | `__GXDefaultTexRegionCallback` | `__GXDefaultTexRegionCallback` | single caller; the derived class inherits the method |

## Already identified in this tree

Addresses inside a range a unit already claims. **Do not carve these** - a second
unit claiming bytes an existing `Matching` object reproduces is a link conflict, and
`link_gap.py` cannot see it.

| accessor | claimed by | class in that unit |
|---|---|---|


## Carveable now: runs in a range no unit claims

A run whose bytes are entirely outside every claimed `.text` range can be carved as its own
`Matching` unit. 13 of the 87 identified runs qualify. **Check the four carve traps in
`docs/RUNNING_THE_DECOMP.md` before starting**: descending source order, one discontiguous range per
unit, no link-order cycle against a neighbouring existing unit boundary, and no duplicate in
`src/MetroidPrime/PortLinkStubs.cpp`.

| retail range | bytes | class | note |
|---|---|---|---|
| `0x801620A8..0x801620F0` | 72 | `CEnvFxManager` | enclosing symbol `Play_801620A8__13CEnvFxManagerFv`; **one of the three touches +4996, which is a global, not `this`** - exclude it before carving |
| `0x801ABDB0..0x801ABDCC` | 28 | `CCameraManager` | enclosing `IsInCinematicCamera__14CCameraManagerCFv` |
| `0x8026E7C4..0x8026E7CC` | 8 | `CCubeRenderer` | enclosing `PrimColor__13CCubeRendererFRC6CColor` |
| `0x8026E7E4..0x8026E7F0` | 12 | `CCubeRenderer` | same TU; carve with the one above only if the range stays contiguous |
| `0x8026EF24..0x8026EF30` | 12 | `CCubeRenderer` | separate run, separate unit |
| `0x8023E0C4..0x8023E0DC` | 24 | `LoadTypedefSLdrPlayerItem` | a loader, not a class accessor |
| `0x8024156C..0x80241578` | 12 | `SLdrScannableParameters` | a loader |
| `0x80216D20..0x80216D2C` | 12 | `CTweakGame` | enclosing `GetTotalPercentage__10CTweakGameFv` |
| `0x8028BFE8..0x8028BFF4` | 12 | `CCallStack` | `__ct__10CCallStackFUiPCcPCc`; a constructor, not an accessor |
| `0x800E9C14..0x800E9C1C` | 8 | `CEntity`? | single-caller, see the candidate table - **not** proven |
| `0x801861B0..0x801861B8` | 8 | `CEntity`? | single-caller, not proven |
| `0x802179D8..0x802179E4` | 12 | `CEntity`? | single-caller, not proven |
| `0x80053EF0..0x80053EFC` | 12 | `clear`? | a mangling artefact, not a class - ignore |

## What the method costs, measured

Four false-positive classes, each of which produces a row that reads exactly like a correct one:

1. **Inheritance.** A `this`-verified call from `CPowerBeam` to `CGunWeapon::IsLoaded` (0x801D9B24)
   is what an *inherited* base method looks like. Crediting `CPowerBeam` is wrong, and the repo
   already has that exact function as a `Matching` unit
   (`src/MetroidPrime/Weapons/CGunWeaponIsLoaded.cpp`). Resolving to the lowest common ancestor of
   **two unrelated** caller classes fixes it; a single caller cannot, and is reported as a set.
2. **A reloaded `r3`.** 205 accessor calls are named but pass a member, an array element or a
   global. `class_sweep.py` reported 15 "confirmed" before the receiver test and **4** after - a
   73% false-positive rate from the "a named function called it" heuristic alone.
3. **A `lis`/`addi`-built `r3` is a global, not `this`.** The `AI*` DSP entry points at
   `0x80381780` are three `lhz`/`sth` against a fixed address and look exactly like an accessor run.
   `mine_accessors.py` now drops 1,822 candidates to 1,258 by rejecting them.
4. **A free operator has no `this`.** `__dv__FRC9CVector2fRCf` mangled naively yields a class called
   `__dv`; the class is the first *argument* type.

## Why the 6-to-10-accessor runs are still not identified

The runs the briefing points at - `0x80212A94..0x80212ADC` and its neighbours, ten contiguous
accessors on one object at +60/+64/+68/+96/+97 - **cannot be identified by any of the routes
above**, and that is a measured result, not a shortfall of effort:

* **No `bl` from a named function, at any depth.** Reverse-BFS from all 1,258 accessors to depth 6
  finds no retail-named function in the whole cone. Every caller in the region
  (`fn_80211864`, `fn_8020F7DC`, `fn_802026D4`, `fn_80211128`, `fn_802105C8`, `fn_802050B0`,
  `fn_802057B0`, `fn_802114A4`, `fn_80211720`, `fn_8020FA7C`, `fn_8020FC54`) is itself unnamed, and
  the nearest named symbols either side are `LoadPillBug` at 0x80200F04 and `LoadSporbProjectile` at
  0x80213C08 - enemy loaders 12 KB away, unrelated.
* **Not virtual.** `tools/vtable_of.py` finds no `.data` word equal to the accessor's address, so
  there is no vtable slot and no `typeinfo` to read a class name from.
* **The enclosing symbols are unnamed too.** `fn_80212A2C` (0x80212A2C, 104 B) is the last named-shape
  function before the run and `fn_80212AE4` the first after; both are `fn_`, so the primary
  evidence is absent.
* **The vtable-in-TU route finds nothing in a +-32 KB window**, so the class's constructor is not
  near its accessors in retail's `.text` either.

What is *known* about `0x80212A94..0x80212ADC` and is worth keeping: offsets **+60 (u32), +64 (u32),
+68 (f32), +96 (u8), +97 (u8)**; `fn_80211864` writes 1 to both +60 and +64 and then reads +96;
`fn_802026D4` reads +64 and +60 and `xoris` the +60 value with 0x8000, so **+60 is a bitfield whose
bit 15 is tested**; `fn_80211864` reads `this+52` and indexes it `slwi ...,3` (an 8-byte-stride
array) and also reads `this+56` (`fn_80212BE4`) and `this+52` (`fn_80212BEC`); and `fn_802026D4`
touches `this+440` and reads a `CStringTable` string. A class with a member at +440, a pointer array
at +52 and +56, two u32 at +60/+64 with bit 15 of the first meaning something, an f32 at +68 and two
u8 flags at +96/+97. The next instrument for this is an **offset fingerprint against the layouts
already measured** in `docs/research/cgamestate_layout.md`, `docs/research/CPatterned_layout.txt` and
`docs/research/missing_classes.md` - not more call-graph work, which is now measured to be empty here.

## Verification for this lane

Nothing in `src/`, `include/`, `configure.py`, `config/`, `files.cmake` or `CMakeLists.txt` was
touched - `git status --short` over those paths is empty - so the port link is unchanged by
construction and the numbers below are the tree's, not this lane's.

```
sha1sum build/G2ME01/main.dol   6ef9b491d0cc08bc81a124fdedb8bfaec34d0010
RELs matching config.yml        86/86  diff=0 missing=0
gate.sh: matched  3255 -> 3255   linked 1845 -> 1845   (+0 functions at 100%, 0 units newly linked)
tools/link_check.sh (before)    undefined 319, duplicates 0, compile errors 0
```

`tools/gate.sh` prints `GATE FAIL: link-gap link-dups` **in this lane only**, because the
`port link dups` step shells out to `tools/link_check.sh`, and re-running it here could not finish:
`/tmp` hit its quota with six other lanes resident (`extern/aurora` compile dies with
`fatal error: error writing to /tmp/ccXXXX.s: Disk quota exceeded`). The 319/0/0 above is the same
script's own output from a run that *did* complete, and the dups count is 0 both times. The other
gate steps are all `ok`. **This is an environment failure, not a regression**, and the
`matched 3255 -> 3255 / linked 1845 -> 1845` line above comes from `gate.sh`'s own per-function
ratchet.

One thing this lane did learn from the gate: `port link gap` reports four entries as **stale** -
listed in the gap list but no longer missing, so another lane has landed them:
`_ZN3CAi9CanBeShotERK13CStateManageri`, `_ZN9CGameArea17SetAreaAttributesEP21CScriptAreaProperties`,
`_ZNK10CAxisAngle9GetVectorEv`, `_ZNK10CGunWeapon8IsLoadedEv`. The last one is independent
confirmation of the inheritance trap above: the enclosing retail symbol for 0x801D9B24 is
`CGunWeapon::IsLoaded`, and the call graph alone would have said `CPowerBeam`.
