#include "MetroidPrime/ScriptObjects/CScriptPickup.hpp"

// #include "MetroidPrime/CAnimData.hpp"
// #include "MetroidPrime/CAnimPlaybackParms.hpp"
// #include "MetroidPrime/CArtifactDoll.hpp"
#include "MetroidPrime/CExplosion.hpp"
#include "Kyoto/Graphics/CColor.hpp"
#include "MetroidPrime/CActorParameters.hpp"
#include "MetroidPrime/CEntityInfo.hpp"
#include "MetroidPrime/CModelData.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/SEchoParameters.hpp"

// #include "MetroidPrime/Cameras/CCameraManager.hpp"
// #include "MetroidPrime/Cameras/CFirstPersonCamera.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
// #include "MetroidPrime/Player/CPlayerGun.hpp"
#include "MetroidPrime/HUD/CSamusHud.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/TCastTo.hpp"
#include "MetroidPrime/Tweaks/CTweakGame.hpp"

#include "MetroidPrime/HUD/CHUDMemoParms.hpp"
// #include "MetroidPrime/HUD/CSamusHud.hpp"

#include "MetroidPrime/ScriptLoader.hpp"
#include "MetroidPrime/ScriptLoader/SLdrPickup.hpp"

#include "MetroidPrime/Player/CEnvironmentVariable.hpp"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/Math/CAbsAngle.hpp"
#include "Kyoto/Math/CMath.hpp"
#include "Kyoto/Text/CStringTable.hpp"

#include "rstl/math.hpp"

static TUniqueId sUnkPickupId = kInvalidUniqueId;

CScriptPickup::CScriptPickup(TUniqueId uid, const rstl::string& name, const CEntityInfo& info,
                             const CTransform4f& xf, const CModelData& modelData,
                             const CActorParameters& aParams, const SEchoParameters& echo,
                             const CAABox& aabb, CPlayerState::EItemType itemType, int amount,
                             int capacityIncrease, int itemPercentageIncrease,
                             CAssetId pickupEffect, bool absoluteValue, bool unknown, bool autoSpin,
                             bool blinkOut, float lifeTime, float respawnTime, float fadeTime,
                             float activateDelay, float pickupEffectLifetime, float autoHomeRange,
                             float delayUntilHome, float homingSpeed, const CVector3f& orbitOffset)
: CActor(uid, name, info, 0, xf, modelData, CMaterialList(), aParams, kInvalidUniqueId)
, mItemType(itemType)
, mAmount(amount)
, mCapacity(capacityIncrease)
, mPercentageIncrease(itemPercentageIncrease)
, mLifeTime(lifeTime)
, mRespawnTime(respawnTime)
, x170(0.f)
, mFadeTime(fadeTime)
, mCurTime(0.0f)
, mPickupEffectLifetime(pickupEffectLifetime)
, mActivateDelay(activateDelay)
, mAutoHomeRange(autoHomeRange)
, mDelayUntilHome(delayUntilHome)
, mHomingSpeed(homingSpeed)
, mTransformZ(xf.GetTranslation().GetZ())
, mPickupParticleDesc()
, mTouchBounds(aabb)
, x1bc(0)
, x1c0(0)
, mOrbitOffset(orbitOffset)
, mUnknownProp(unknown)
, mGenerated(false)
, mInTractor(false)
, mAbsoluteValue(absoluteValue)
, mEnableTractorTest(false)
, mAutoSpin(autoSpin)
, mUnk2(false)
, mUnk3(false)
, mBlinkOut(blinkOut) {
  if (pickupEffect != kInvalidAssetId) {
    mPickupParticleDesc = gpSimplePool->GetObj(SObjectTag('PART', pickupEffect));
    mPickupParticleDesc->Lock();
  }

  if (HasAnimation()) {
    // AnimationData()->SetAnimation(CAnimPlaybackParms(0, -1, 1.f, true), false);
  }

  if (fadeTime) {
  //   SetModelFlags(CModelFlags::AlphaBlended(0.f).DepthCompareUpdate(true, false));
  }
}

CScriptPickup::~CScriptPickup() {}

void CScriptPickup::PreRenderAllViewports(CStateManager& mgr) {
  CActor::PreRenderAllViewports(mgr);
  if (mUnk2) {
    mUnk2 = false;
    x1bc = 0;
  } else {
    x1bc += 1;
  }
}

void CScriptPickup::PreRender(CStateManager& mgr) {
  CActor::PreRender(mgr);
  if (!GetPreRenderClipped()) {
    mUnk2 = true;
  }
}

bool CScriptPickup::IsVisible() const {
  if (mActivateDelay >= 0.0f) {
    return false;
  }
  return !(x170 > 0.0f);
}

void CScriptPickup::Think(float dt, CStateManager& mgr) {
  CActor::Think(dt, mgr);
  if (!GetActive()) {
    return;
  }

  // if (mDelayTimer >= 0.f) {
  //   // CActor::Stop();
  //   mDelayTimer -= dt;
  //   return;
  // }

  // x270_curTime += dt;
  // if (x28c_25_inTractor && (x26c_lifeTime - x270_curTime) < 2.f) {
  //   x270_curTime = rstl::max_val(x26c_lifeTime - 2.f - FLT_EPSILON, x270_curTime - 2.f * dt);
  // }

  // CModelFlags drawFlags = CModelFlags::Normal();

  // if (x268_fadeInTime) {
  //   if (x270_curTime < x268_fadeInTime) {
  //     drawFlags =
  //         CModelFlags::AlphaBlended(x270_curTime / x268_fadeInTime).DepthCompareUpdate(true,
  //         false);
  //   } else {
  //     x268_fadeInTime = 0.f;
  //   }
  // } else if (x26c_lifeTime) {
  //   float alpha = 1.f;
  //   if (x26c_lifeTime < 2.f) {
  //     alpha = 1.f - (x26c_lifeTime / x270_curTime);
  //   } else if ((x26c_lifeTime - x270_curTime) < 2.f) {
  //     alpha = (x26c_lifeTime - x270_curTime) / 2.f;
  //   }

  //   drawFlags = CModelFlags::AlphaBlended(alpha).DepthCompareUpdate(true, false);
  // }

  // SetModelFlags(drawFlags);

  // if (HasAnimation()) {
  //   CAdvancementDeltas deltas = UpdateAnimation(dt, m_gr, true);
  //   MoveToOR(deltas.GetOffsetDelta(), dt);
  //   RotateToOR(deltas.GetOrientationDelta(), dt);
  // }

  // if (x28c_25_inTractor) {
  //   CVector3f velocity =
  //       mgr.GetPlayer()->GetTranslation() + (CVector3f::Up() * 2.f) - GetTranslation();
  //   x274_tractorTime += dt;
  //   float halfTractorTime = rstl::min_val(x274_tractorTime, 2.f) * 0.5f;
  //   velocity = velocity.AsNormalized() * (halfTractorTime * 20.f);
  //   if (x28c_26_enableTractorTest && mgr.GetPlayer()->GetPlayerGun()->GetChargeBeamFactor() <
  //                                        CPlayerGun::GetTractorBeamFactor()) {
  //     x28c_26_enableTractorTest = false;
  //     x28c_25_inTractor = false;
  //     velocity = CVector3f::Zero();
  //   }
  //   SetVelocityWR(velocity);
  // } else if (x28c_24_generated) {
  //   if (mgr.GetPlayer()->GetPlayerGun()->GetChargeBeamFactor() >
  //       CPlayerGun::GetTractorBeamFactor()) {
  //     const CFirstPersonCamera* camera = mgr.CameraManager()->FirstPersonCamera();
  //     CVector3f posDelta = GetTranslation() - camera->GetTranslation();
  //     CVector3f cameraFront = camera->GetTransform().GetColumn(kDY);
  //     float dot = CVector3f::Dot(cameraFront, posDelta.AsNormalized());
  //     float fovCos = cosine(CAbsAngle::FromDegrees(gpTweakGame->GetFirstPersonFOV()));
  //     if (dot > fovCos && posDelta.MagSquared() < skDrawInDistance * skDrawInDistance) {
  //       x28c_25_inTractor = true;
  //       x28c_26_enableTractorTest = true;
  //       x274_tractorTime = 0.f;
  //     }
  //   }
  // }

  // if (x26c_lifeTime && x270_curTime > x26c_lifeTime) {
  //   mgr.FreeScriptObject(GetUniqueId());
  // }
}

void CScriptPickup::Touch(CActor& act, CStateManager& mgr) {
  if (!GetActive()) {
    return;
  }
  if (!IsVisible()) {
    return;
  }
  if (CPlayer* player = TCastToPtr< CPlayer >(act)) {
    const TUniqueId playerUid = player->GetUniqueId();
    int playerIndex = mgr.MaskUIdNumPlayers(playerUid);
    CPlayerState* playerState = mgr.PlayerState(playerIndex);
    if (!playerState->IsPlayerAlive())
      return;

    CPlayerState::EItemType itemType = mItemType;

    if (mPickupParticleDesc) {
      mgr.AddObject(rs_new CExplosion(
          TLockedToken< CGenDescription >(*mPickupParticleDesc), mgr.AllocateUniqueId(),
          CEntityInfo(GetCurrentAreaId(), CEntity::NullConnectionList, true, kInvalidEditorId),
          rstl::string_l("Explosion - Pickup Effect"), GetTransform(), 0,
          CVector3f(1.f, 1.f, 1.f), CColor::White(), -1));
    }

    int previousAmount = playerState->GetItemAmount(itemType);
    playerState->AddPowerUp(CPlayerState::kIT_ItemPercentage, mPercentageIncrease);
    playerState->IncrPickUp(CPlayerState::kIT_ItemPercentage, mPercentageIncrease);
    if (!mAbsoluteValue) {
      playerState->AddPowerUp(itemType, mCapacity);
      playerState->IncrPickUp(itemType, mAmount);
    } else {
      playerState->ReInitializePowerUp(itemType, mCapacity);
      playerState->SetItemAmount(itemType, mAmount);
    }
    playerState->SetTimeLeft(itemType, mPickupEffectLifetime);
    SendScriptMsgs(kSS_Active, mgr, playerUid, kSM_None);
    if (mRespawnTime == 0.0f) {
      mEnableTractorTest = true;
      mgr.DeleteObjectRequest(GetUniqueId());
    } else {
      SetModelFlags(CModelFlags::AlphaBlended(0.f).DepthCompareUpdate(true, false));
      x170 = mRespawnTime;
      mCurTime = 0.f;
      mFadeTime = 0.25f;
    }

    if (playerState->GetItemAmount(itemType) > previousAmount) {
      ShowAllKeysCollectedAlert(mgr, playerState, itemType);
    }

    if (mPercentageIncrease > 0) {
      int total = playerState->GetTotalPickupCount();
      int colRate = playerState->CalculateItemCollectionRate();
      if (colRate == total) {
        CAssetId id =
            gpResourceFactory
                ->GetResourceIdByName("STRG_AllPickupsFound_2")
                ->id;
              
        mgr.QueueMessage(mgr.GetHUDMessageFrameCount() + 1, id, 0.f);
        gpGameState->SystemOptions().FindEnvironmentVariable("AllPickupsFound")->Set(1);
      }
    }

    if (!mgr.IsMultiplayer() && itemType == CPlayerState::kIT_Powerbomb && mCapacity == 0) {
      CPersistentOptions& opts = gpGameState->SystemOptions();
      if (opts.FindEnvironmentVariable("PowerbombPickupMessages")->GetValue() == 0) {
        opts.FindEnvironmentVariable("PowerbombPickupMessages")->Set(1);
        CSamusHud::DisplayHudMemo(
          rstl::wstring_l(gpStringTable->GetString("FirstPowerBombPickup")),
          CHUDMemoParms(5.f, true, false, false, 1 << playerIndex, true)
        );
      }
    }
    switch (itemType) {
      case CPlayerState::kIT_SwitchVisorCombat:
        playerState->StartTransitionToVisor(CPlayerState::kPV_Combat);
        break;
      case CPlayerState::kIT_SwitchVisorScan:
        playerState->StartTransitionToVisor(CPlayerState::kPV_Scan);
        break;
      case CPlayerState::kIT_SwitchVisorDark:
        playerState->StartTransitionToVisor(CPlayerState::kPV_Dark);
        break;
      case CPlayerState::kIT_SwitchVisorEcho:
        playerState->StartTransitionToVisor(CPlayerState::kPV_Echo);
        break;
    }
  }
}

rstl::optional_object< CAABox > CScriptPickup::GetTouchBounds() const {
  CVector3f off = GetTranslation();
  return CAABox(mTouchBounds.GetMinPoint() + off, mTouchBounds.GetMaxPoint() + off);
}

void CScriptPickup::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  switch (msg.GetMessage()) {
  case kSM_Activate:
    mTransformZ = GetTranslation().GetZ();
    break;
  case kSM_XCRT:
    if (mgr.fn_80036200().GetUnk14_24()) {
      mUnk3 = true;
    }
    break;
  case kSM_XDelete:
    if (!mEnableTractorTest) {
      SendScriptMsgs(kSS_Dead, mgr, kInvalidUniqueId, kSM_None);
    }
    sUnkPickupId = kInvalidUniqueId;
    break;
  default:
    break;
  }
  CActor::AcceptScriptMsg(mgr, msg);
}

void CScriptPickup::AddToRenderer(const CStateManager& mgr) const {
  if (IsVisible()) {
    CActor::AddToRenderer(mgr);
  }
}

CPlayerState::EItemType CScriptPickup::GetItem() const { return mItemType; }

void CScriptPickup::SetSpawned() { mGenerated = true; }

void CScriptPickup::fn_800B4518(CStateManager& mgr) {
  if (!mgr.IsMultiplayer()) {
    mUnknownProp = true;
  }
  mUnk3 = true;
}

// Retail's 116-byte free function at 0x800B44A4; nothing in the DOL calls it.
extern "C" CVector3f GetPosition_800B44A4(const CScriptPickup& pickup) {
  return pickup.GetTranslation() + pickup.GetTransform().Rotate(pickup.GetOrbitOffset());
}

// Retail 0x800B41FC, 680 bytes. The dispatch is a switch on the item type whose four groups
// cover the temple (0x1D-0x1F and 0x65-0x6A), Agon (0x20-0x22), Torvus (0x23-0x25) and hive
// (0x26-0x28) keys; each group re-checks its own keys and picks one of the four
// `STRG_All*KeysFound` strings, so the temple body is emitted once and reached from both
// halves of the case set.
void CScriptPickup::ShowAllKeysCollectedAlert(CStateManager& mgr, CPlayerState* playerState,
                                             CPlayerState::EItemType itemType) {
  const char* name = nullptr;
  switch (itemType) {
  case CPlayerState::kIT_TempleKey1:
  case CPlayerState::kIT_TempleKey2:
  case CPlayerState::kIT_TempleKey3:
  case CPlayerState::kIT_TempleKey4:
  case CPlayerState::kIT_TempleKey5:
  case CPlayerState::kIT_TempleKey6:
  case CPlayerState::kIT_TempleKey7:
  case CPlayerState::kIT_TempleKey8:
  case CPlayerState::kIT_TempleKey9:
    if (playerState->GetItemAmount(CPlayerState::kIT_TempleKey1) <= 0) {
      break;
    }
    if (playerState->GetItemAmount(CPlayerState::kIT_TempleKey2) <= 0) {
      break;
    }
    if (playerState->GetItemAmount(CPlayerState::kIT_TempleKey3) <= 0) {
      break;
    }
    if (playerState->GetItemAmount(CPlayerState::kIT_TempleKey4) <= 0) {
      break;
    }
    if (playerState->GetItemAmount(CPlayerState::kIT_TempleKey5) <= 0) {
      break;
    }
    if (playerState->GetItemAmount(CPlayerState::kIT_TempleKey6) <= 0) {
      break;
    }
    if (playerState->GetItemAmount(CPlayerState::kIT_TempleKey7) <= 0) {
      break;
    }
    if (playerState->GetItemAmount(CPlayerState::kIT_TempleKey8) <= 0) {
      break;
    }
    if (playerState->GetItemAmount(CPlayerState::kIT_TempleKey9) <= 0) {
      break;
    }
    name = "STRG_AllTempleKeysFound";
    break;
  case CPlayerState::kIT_AgonKey1:
  case CPlayerState::kIT_AgonKey2:
  case CPlayerState::kIT_AgonKey3:
    if (playerState->GetItemAmount(CPlayerState::kIT_AgonKey1) <= 0) {
      break;
    }
    if (playerState->GetItemAmount(CPlayerState::kIT_AgonKey2) <= 0) {
      break;
    }
    if (playerState->GetItemAmount(CPlayerState::kIT_AgonKey3) <= 0) {
      break;
    }
    name = "STRG_AllSandKeysFound";
    break;
  case CPlayerState::kIT_TorvusKey1:
  case CPlayerState::kIT_TorvusKey2:
  case CPlayerState::kIT_TorvusKey3:
    if (playerState->GetItemAmount(CPlayerState::kIT_TorvusKey1) <= 0) {
      break;
    }
    if (playerState->GetItemAmount(CPlayerState::kIT_TorvusKey2) <= 0) {
      break;
    }
    if (playerState->GetItemAmount(CPlayerState::kIT_TorvusKey3) <= 0) {
      break;
    }
    name = "STRG_AllSwampKeysFound";
    break;
  case CPlayerState::kIT_HiveKey1:
  case CPlayerState::kIT_HiveKey2:
  case CPlayerState::kIT_HiveKey3:
    if (playerState->GetItemAmount(CPlayerState::kIT_HiveKey1) <= 0) {
      break;
    }
    if (playerState->GetItemAmount(CPlayerState::kIT_HiveKey2) <= 0) {
      break;
    }
    if (playerState->GetItemAmount(CPlayerState::kIT_HiveKey3) <= 0) {
      break;
    }
    name = "STRG_AllCliffsKeysFound";
    break;
  default:
    break;
  }

  if (name) {
    CAssetId id = gpResourceFactory->GetResourceIdByName(name)->id;
    mgr.QueueMessage(mgr.GetHUDMessageFrameCount() + 1, id, 0.f);
  }
}

CAABox LoadCAABox(CStateManager& mgr, const TAreaId& areaId, const CVector3f& collisionSize,
                  const CVector3f& collisionOffset);

rstl::optional_object< CModelData > LdrToModelData(const CVector3f&, CAssetId asset,
                                                  const SLdrAnimationSet&, bool);

CEntity* LoadPickup(CStateManager& mgr, CInputStream& input, CEntityInfo& info) {
  SLdrPickup sldrThis;
#include "MetroidPrime/ScriptLoader/SLdrPickup.inc"

  rstl::optional_object< CModelData > modelData(
      LdrToModelData(sldrThis.editorProperties.transform.scale, sldrThis.model,
                    sldrThis.animationInformation, true));
  if (!modelData) {
    return nullptr;
  }

  CAABox box =
      LoadCAABox(mgr, info.GetAreaId(), sldrThis.collisionSize, sldrThis.collisionOffset);
  if (sldrThis.collisionSize == CVector3f::Zero()) {
    box = modelData->GetBounds(CTransform4f(LdrToTransform4f(sldrThis.editorProperties)));
  }
  return new CScriptPickup(
      mgr.AllocateUniqueId(), sldrThis.editorProperties.name,
      LdrToEntityInfo(info, sldrThis.editorProperties),
      LdrToTransform4f(sldrThis.editorProperties), *modelData,
      LdrToActorParameters(sldrThis.actorInformation),
      LdrToEchoParameters(sldrThis.echoInformation), box,
      CPlayerState::EItemType(sldrThis.itemToGive.value), sldrThis.amount,
      sldrThis.capacityIncrease, sldrThis.itemPercentageIncrease, sldrThis.pickupEffect,
      sldrThis.absoluteValue, sldrThis.canHomeByDefault, sldrThis.autoSpin,
      sldrThis.blinkOut,
      sldrThis.lifetime, sldrThis.respawnTime, sldrThis.fadetime,
      sldrThis.activationDelay, sldrThis.pickupEffectLifetime, sldrThis.autoHomeRange,
      sldrThis.delayUntilHome, sldrThis.homingSpeed, CVector3f(sldrThis.orbitOffset));
}
