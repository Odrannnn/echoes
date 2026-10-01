#include "MetroidPrime/ScriptObjects/CScriptSpecialFunction.hpp"

#include "Kyoto/Audio/CSfxManager.hpp"
#include "MetroidPrime/CActorParameters.hpp"
#include "MetroidPrime/CEnvFxManager.hpp"
#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Player/CGameState.hpp"
#include "MetroidPrime/Player/CGameMode.hpp"
#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/Player/CPlayerState.hpp"
#include "MetroidPrime/TCastTo.hpp"

CScriptSpecialFunction::CScriptSpecialFunction(
    TUniqueId uid, const rstl::string& name, const CEntityInfo& info, const CTransform4f& xf,
    ESpecialFunction function, const rstl::string& stringParm, float value1, float value2,
    float value3, float value4, int intParm1, int intParm2, const CVector3f& vectorParm,
    const CColor& colorParm, const CDamageInfo& damageInfo, CPlayerState::EItemType item,
    ushort sfx1, ushort sfx2, ushort sfx3)
: CActor(uid, name, info, 0, xf, CModelData(), CMaterialList(), CActorParameters(),
         kInvalidUniqueId)
, mFunction(function)
, mStringParm(stringParm)
, mValue1(value1)
, mValue2(value2)
, mValue3(value3)
, mValue4(value4)
, mIntParm1(intParm1)
, mIntParm2(intParm2)
, mVectorParm(vectorParm)
, mColorParm(colorParm)
, x194_(kInvalidUniqueId)
, mLastOriginatorPlayer(kInvalidUniqueId)
, mDamageInfo(damageInfo)
, mSpinnerPosition(0.f)
, mSpinnerInitialXf(CTransform4f::Identity())
, mShotSpinnerImpulse(0.f)
, mSfx1(sfx1)
, mSfx2(sfx2)
, mSfx3(sfx3)
, mSfxHandle()
, mPreviousSpinnerSpeed(0.f)
, mVolumeAverage(6, 0.f)
, mChaffTimer(0.f)
, mSilhouetteStrength(0.f)
, mTargetSilhouetteStrength(0.f)
, mItem(item)
, mSpinnerInitializedXf(false)
, mSpinnerCanMove(false)
, mSfx2Played(true)
, mSfx3Played(false)
, mInAreaDamage(false)
, mDoSave(false)
, mPlayerInArea(false)
, mFrustumEntered(false)
, mFrustumExited(false)
, mInFrustum(false) {
  if (mFunction == kSF_HUDTarget) {
    mTouchBounds = CAABox(CVector3f(-1.f, -1.f, -1.f), CVector3f(1.f, 1.f, 1.f));
  }
}

void CScriptSpecialFunction::AddFogVolumeToRenderer(const CStateManager& mgr) const {
  if (mInFrustum) {
    EnsureRendered(mgr);
  }
}

void CScriptSpecialFunction::AddSilhouetteToRenderer(const CStateManager& mgr) const {
  if (mInFrustum) {
    EnsureRendered(mgr);
  }
}

void CScriptSpecialFunction::AddToRenderer(const CStateManager& mgr) const {
  if (!GetActive()) {
    return;
  }

  switch (mFunction) {
  case kSF_FogVolume:
    AddFogVolumeToRenderer(mgr);
    break;
  case kSF_Silhouette:
    AddSilhouetteToRenderer(mgr);
    break;
  default:
    break;
  }
}

void CScriptSpecialFunction::PreRenderFogVolume(CStateManager& mgr) {
  // TODO: test the fog volume's bounds against the current frustum.
}

void CScriptSpecialFunction::PreRenderViewFrustumTester(CStateManager& mgr) {
  SetInFrustum(mgr.GetFrustumPlanes().PointInFrustumPlanes(GetTranslation()));
}

void CScriptSpecialFunction::PreRenderPlayerFrustumTester(CStateManager& mgr) {
  if (static_cast< uint >(mIntParm2) == static_cast< uint >(mgr.GetCurrentRenderPlayerIndex())) {
    SetInFrustum(mgr.GetFrustumPlanes().PointInFrustumPlanes(GetTranslation()));
  }
}

void CScriptSpecialFunction::PreRenderSilhouette(CStateManager& mgr) {
  // TODO: copy the connected actor's bounds and test visibility.
}

void CScriptSpecialFunction::PreRenderBillboard(CStateManager& mgr) {
  if (mIntParm1 != 0) {
    mgr.fn_800379C4(GetUniqueId());
  }
}

void CScriptSpecialFunction::PreRender(CStateManager& mgr) {
  if (!GetActive()) {
    return;
  }

  switch (mFunction) {
  case kSF_FogVolume:
    PreRenderFogVolume(mgr);
    break;
  case kSF_ViewFrustumTester:
    PreRenderViewFrustumTester(mgr);
    break;
  case kSF_PlayerFrustumTester:
    PreRenderPlayerFrustumTester(mgr);
    break;
  case kSF_Silhouette:
    PreRenderSilhouette(mgr);
    break;
  case kSF_Billboard:
    PreRenderBillboard(mgr);
    break;
  default:
    break;
  }
}

void CScriptSpecialFunction::RenderFogVolume(const CStateManager& mgr) const {
  // TODO: draw the animated fog volume.
}

void CScriptSpecialFunction::RenderSilhouette(const CStateManager& mgr) const {
  // TODO: render the connected actor's silhouette.
}

void CScriptSpecialFunction::RenderBillboard() const {
  // TODO: draw the camera-facing textured effect.
}

void CScriptSpecialFunction::Render(const CStateManager& mgr) const {
  // TODO: dispatch special rendering and preserve the conditional base rendering.
}

void CScriptSpecialFunction::AcceptChaffTarget(CStateManager& mgr, const CScriptMsg& msg) {
  switch (msg.GetMessage()) {
  case kSM_XALD:
    AddMaterial(kMT_Target, mgr);
    break;
  default:
    break;
  }
}

void CScriptSpecialFunction::AcceptHUDFadeIn(CStateManager& mgr, const CScriptMsg& msg) {
  if (msg.GetMessage() == kSM_Action) {
    mgr.GetPlayer(0)->SetHudDisable(mValue1, 0.f, 1.f);
  }
}

void CScriptSpecialFunction::AcceptEscapeSequence(CStateManager& mgr, const CScriptMsg& msg) {
  if (msg.GetMessage() == kSM_Action && mValue1 >= 0.f) {
    mgr.fn_80038370(mValue1);
  }
}

void CScriptSpecialFunction::AcceptSpinner(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: update spinner motion, reset state and sound lifetime.
}

void CScriptSpecialFunction::AcceptShotSpinner(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: update the shot-spinner impulse and send Play.
}

void CScriptSpecialFunction::AcceptMapStation(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: map the connected destinations and open the map screen.
}

void CScriptSpecialFunction::AcceptMissileStation(CStateManager& mgr, const CScriptMsg& msg) {
  if (msg.GetMessage() == kSM_Action && !mgr.fn_80036F10()) {
    CPlayerState& state = *mgr.PlayerState(0);
    state.ResetAndIncrPickUp(CPlayerState::kIT_Missile,
                             state.GetItemCapacity(CPlayerState::kIT_Missile));
  }
}

void CScriptSpecialFunction::AcceptPowerBombStation(CStateManager& mgr, const CScriptMsg& msg) {
  if (msg.GetMessage() == kSM_Action && !mgr.fn_80036F10()) {
    CPlayerState& state = *mgr.PlayerState(0);
    state.ResetAndIncrPickUp(CPlayerState::kIT_Powerbomb,
                             state.GetItemCapacity(CPlayerState::kIT_Powerbomb));
  }
}

void CScriptSpecialFunction::AcceptSaveStation(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: request the save interaction and track its completion.
}

void CScriptSpecialFunction::AcceptEnergyTank(CStateManager& mgr, const CScriptMsg& msg) {
  if (msg.GetMessage() == kSM_Action) {
    if (CPlayer* player =
            TCastToPtr< CPlayer >(mgr.ObjectById(mLastOriginatorPlayer))) {
      player->GetPlayerState()->IncrPickUp(CPlayerState::kIT_EnergyTanks, 1);
    }
  }
}

void CScriptSpecialFunction::AcceptRadialDamage(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: apply radial damage and perform the optional self-deletion.
}

void CScriptSpecialFunction::AcceptBossEnergyBar(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: select the boss actor, health and localized name.
}

void CScriptSpecialFunction::AcceptEndGame(CStateManager& mgr, const CScriptMsg& msg) {
  if (msg.GetMessage() == kSM_Action) {
    // Retail calls the *const* `CGameState::GetGameMode` (0x8014246c) and then a non-const virtual
  // through the result, so the mode is reached through a const reference and const_cast back.
  const CGameState& gameState = *gpGameState;
  const_cast< CGameMode& >(gameState.GetGameMode()).EndGame(mIntParm1, mgr);
  }
}

void CScriptSpecialFunction::AcceptCinematicSkip(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: register or clear this actor as the cinematic-skip receiver.
}

void CScriptSpecialFunction::AcceptEnvFxDensity(CStateManager& mgr, const CScriptMsg& msg) {
  if (msg.GetMessage() == kSM_Action) {
    mgr.EnvFxManager()->FadeDensity(mValue1, static_cast< int >(mValue2));
  }
}

void CScriptSpecialFunction::AcceptRumble(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: start the configured positional or non-positional rumble.
}

void CScriptSpecialFunction::AcceptInventoryActivator(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: send Zero when any player has the configured item.
}

void CScriptSpecialFunction::AcceptAreaDamage(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: clean up the active area-damage state.
}

void CScriptSpecialFunction::AcceptDropBomb(CStateManager& mgr, const CScriptMsg& msg) {
  // Empty in the original.
}

void CScriptSpecialFunction::AcceptHint(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: control the named hint and hint timer.
}

void CScriptSpecialFunction::AcceptPlayerInArea(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: query whether the primary player is in this area.
}

void CScriptSpecialFunction::AcceptHUDTarget(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: add or remove the target and radar materials.
}

void CScriptSpecialFunction::AcceptFogFader(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: update the camera fog interpolation for each player.
}

void CScriptSpecialFunction::AcceptLogbook(CStateManager& mgr, const CScriptMsg& msg) {
  if (msg.GetMessage() == kSM_Action) {
    mgr.DeferStateTransition(kSMT_LogBook);
  }
}

void CScriptSpecialFunction::AcceptEnding(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: compare the requested ending with the unlocked tier.
}

void CScriptSpecialFunction::AcceptPlayerVelocity(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: direct the originating player's velocity toward a connected actor.
}

void CScriptSpecialFunction::AcceptDarkWorld(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: send the state selected by the world's light/dark state.
}

void CScriptSpecialFunction::fn_80107a58(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: identify the game-mode operation performed for the configured player.
}

void CScriptSpecialFunction::AcceptPlayerSpawnPoint(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: select a connected spawn point for the configured player.
}

void CScriptSpecialFunction::fn_80107994(CStateManager& mgr, const CScriptMsg& msg) {
  // Empty in the original.
}

void CScriptSpecialFunction::AcceptSetItemCapacity(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: set the selected player's item capacity.
}

void CScriptSpecialFunction::AcceptSetTimedItemAmount(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: set the selected player's item amount and timer.
}

void CScriptSpecialFunction::AcceptModifyItemAmount(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: adjust the selected player's item amount.
}

void CScriptSpecialFunction::AcceptModifyItemCapacity(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: adjust the selected player's item capacity.
}

void CScriptSpecialFunction::AcceptModifyItem(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: adjust the selected player's item capacity and amount.
}

void CScriptSpecialFunction::AcceptGiveTimedItem(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: set the selected player's item capacity, amount and timer.
}

void CScriptSpecialFunction::AcceptLastDamager(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: forward the sender's last damaging actor as the originator.
}

void CScriptSpecialFunction::fn_80107458(CStateManager& mgr, const CScriptMsg& msg) {
  switch (msg.GetMessage()) {
  case kSM_Increment:
    mgr.SetCinematicPause(true);
    break;
  case kSM_Decrement:
    mgr.SetCinematicPause(false);
    break;
  default:
    break;
  }
}

void CScriptSpecialFunction::AcceptSilhouette(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: update current and target silhouette strength.
}

void CScriptSpecialFunction::AcceptPauseGame(CStateManager& mgr, const CScriptMsg& msg) {
  if (msg.GetMessage() == kSM_Action) {
    mgr.DeferStateTransition(kSMT_PauseGame);
  }
}

void CScriptSpecialFunction::AcceptSkyboxLighting(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: set the skybox lighting level or its interpolation direction.
}

void CScriptSpecialFunction::AcceptAreaOcclusion(CStateManager& mgr, const CScriptMsg& msg) {
  switch (msg.GetMessage()) {
  case kSM_XCRT:
    mIntParm1 = 0;
    break;
  default:
    break;
  }
}

void CScriptSpecialFunction::AcceptMultiplayerEndConditions(CStateManager& mgr,
                                                            const CScriptMsg& msg) {
  switch (msg.GetMessage()) {
  case kSM_XCRT:
    mIntParm1 = 0;
    mIntParm2 = 0;
    break;
  default:
    break;
  }
}

void CScriptSpecialFunction::AcceptViewFrustumTester(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: flush the frustum-exit state before deactivation.
}

void CScriptSpecialFunction::AcceptDamageActor(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: apply configured damage to connected actors.
}

void CScriptSpecialFunction::AcceptRezbitState(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: start or stop the originating player's timed Rezbit state.
}

void CScriptSpecialFunction::AcceptFogPlane(CStateManager& mgr, const CScriptMsg& msg) {
  switch (msg.GetMessage()) {
  case kSM_Increment:
    mIntParm1 = 1;
    break;
  case kSM_Decrement:
    mIntParm1 = 0;
    break;
  default:
    break;
  }
}

void CScriptSpecialFunction::AcceptBillboard(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: update the billboard's visibility flags and fade state.
}

void CScriptSpecialFunction::AcceptAreaDocks(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: enable or disable this area's docks.
}

void CScriptSpecialFunction::AcceptEnvironmentVariable(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: update or query the named persistent environment variable.
}

void CScriptSpecialFunction::AcceptMultiplayerResult(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: forward the connection selected by the multiplayer result.
}

void CScriptSpecialFunction::AcceptMapObjectVisibility(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: update this object's map visibility.
}

void CScriptSpecialFunction::AcceptStopRezbitState(CStateManager& mgr, const CScriptMsg& msg) {
  // Retail reads the *message* field (offset 8) and compares it against 0x5a45524f, the value
  // this repo already spells `kSS_Zero`. The constant is a four-character tag, so the same bits
  // appear as both a state and (un-named here) a message; cast rather than invent a name.
  if (GetActive() && msg.GetMessage() == static_cast< EScriptObjectMessage >(kSS_Zero)) {
    mgr.GetPlayer(0)->StopRezbitState(mgr);
  }
}

void CScriptSpecialFunction::AcceptCredits(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: queue the credits screen.
}

void CScriptSpecialFunction::AcceptScriptMsg(CStateManager& mgr, const CScriptMsg& msg) {
  // TODO: special handlers run both before and after the base message handler.
  CActor::AcceptScriptMsg(mgr, msg);
}

void CScriptSpecialFunction::Think(float dt, CStateManager& mgr) {
  // TODO: recover all per-frame handlers before wiring up dispatch.
}

void CScriptSpecialFunction::ThinkBillboard(float dt, CStateManager& mgr) {
  // TODO: advance the billboard fade state.
}

void CScriptSpecialFunction::ThinkSaveStation(float dt, CStateManager& mgr) {
  // TODO: observe the deferred save transition and send its completion state.
}

void CScriptSpecialFunction::ThinkPlayerFollowLocator(float dt, CStateManager& mgr) {
  // TODO: move the originating player to the connected actor's locator.
}

void CScriptSpecialFunction::ThinkSpinnerController(float dt, CStateManager& mgr,
                                                    ESpinnerControllerMode mode) {
  // TODO: update spinner progress, connected actors and its sound emitter.
}

void CScriptSpecialFunction::ThinkObjectFollowLocator(float dt, CStateManager& mgr) {
  // TODO: move connected actors to the source actor's locator.
}

void CScriptSpecialFunction::ThinkObjectFollowObject(float dt, CStateManager& mgr) {
  // TODO: copy the active source actor's transform to its connected target.
}

void CScriptSpecialFunction::ThinkChaffTarget(float dt, CStateManager& mgr) {
  // TODO: inspect nearby projectiles and update each player's HUD interference.
}

void CScriptSpecialFunction::ThinkRainSimulator(float dt, CStateManager& mgr) {
  // TODO: send the state selected by the manager's update-frame cycle.
}

void CScriptSpecialFunction::ThinkAreaDamage(float dt, CStateManager& mgr) {
  // TODO: track single-player area damage and apply the frame's damage.
}

void CScriptSpecialFunction::ThinkActorScale(float dt, CStateManager& mgr) {
  // TODO: scale connected actors toward the configured limit.
}

void CScriptSpecialFunction::ThinkPlayerInArea(float dt, CStateManager& mgr) {
  // TODO: send Entered/Exited when the primary player's area changes.
}

void CScriptSpecialFunction::ThinkViewFrustumTester(float dt, CStateManager& mgr) {
  SendFrustumMessages(mgr);
}

void CScriptSpecialFunction::ThinkPlayerFrustumTester(float dt, CStateManager& mgr) {
  if (static_cast< uint >(mIntParm1) < static_cast< uint >(mgr.GetNumPlayers())) {
    SetTranslation(mgr.GetPlayer(mIntParm1)->GetTranslation());
  }
  SendFrustumMessages(mgr);
}

void CScriptSpecialFunction::ThinkPlayerItemRelay(float dt, CStateManager& mgr) {
  // TODO: route player messages using the configured item's value.
}

void CScriptSpecialFunction::ThinkPlayerOffset(float dt, CStateManager& mgr) {
  // TODO: position connected actors at the configured angular offset from the player.
}

void CScriptSpecialFunction::ThinkConnectedEffectPlane(float dt, CStateManager& mgr) {
  // TODO: update the connected effect's plane from its source actor.
}

void CScriptSpecialFunction::ThinkSilhouette(float dt, CStateManager& mgr) {
  // TODO: interpolate the silhouette strength toward its target.
}

void CScriptSpecialFunction::ThinkMapTeleport(float dt, CStateManager& mgr) {
  // TODO: enable the selected world's teleporters and report the destination.
}

void CScriptSpecialFunction::ThinkSkyboxLighting(float dt, CStateManager& mgr) {
  // TODO: interpolate and clamp the world's skybox lighting level.
}

void CScriptSpecialFunction::ThinkAreaOcclusion(float dt, CStateManager& mgr) {
  // TODO: report changes in this area's occlusion state.
}

void CScriptSpecialFunction::ThinkMultiplayerEndConditions(float dt, CStateManager& mgr) {
  // TODO: send the multiplayer time/score threshold messages.
}

void CScriptSpecialFunction::ThinkTriggerScale(float dt, CStateManager& mgr) {
  // TODO: resize the connected trigger.
}

void CScriptSpecialFunction::ThinkObjectFollowJoint(float dt, CStateManager& mgr) {
  // TODO: include the source joint's bind rotation when following a locator.
}

void CScriptSpecialFunction::ThinkRezbitState(float dt, CStateManager& mgr) {
  // TODO: expire the originating player's Rezbit state.
}

void CScriptSpecialFunction::AddOrUpdateEmitter(float pitch, float maxDist, float falloff,
                                                CSfxHandle& handle, ushort id, CVector3f position,
                                                uchar volume) {
  // TODO: create or update the spinner emitter with Echoes sound parameters.
}

void CScriptSpecialFunction::DeleteEmitter(CSfxHandle& handle) {
  if (handle) {
    CSfxManager::RemoveEmitter(handle);
    handle.Clear();
  }
}

void CScriptSpecialFunction::SkipCinematic(CStateManager& mgr) {
  SendScriptMsgs(kSS_Zero, mgr, kInvalidUniqueId, kSM_None);
  mgr.SetSkipCinematicReceiver(kInvalidUniqueId);
}

void CScriptSpecialFunction::SetInFrustum(bool inFrustum) {
  if (mInFrustum == inFrustum) {
    return;
  }

  if (inFrustum) {
    if (mFrustumExited) {
      mFrustumExited = false;
    } else {
      mFrustumEntered = true;
    }
  } else if (mFrustumEntered) {
    mFrustumEntered = false;
  } else {
    mFrustumExited = true;
  }
  mInFrustum = inFrustum;
}

int CScriptSpecialFunction::ResolvePlayerIndex(int playerIndex, TUniqueId originator,
                                               CStateManager& mgr) {
  if (playerIndex == -1 && TCastToConstPtr< CPlayer >(mgr.GetObjectById(originator))) {
    return mgr.MaskUIdNumPlayers(originator);
  }
  return playerIndex;
}

void CScriptSpecialFunction::OnItemDepleted(CStateManager& mgr, int playerIndex,
                                            CPlayerState::EItemType item) {
  if (mFunction == kSF_ItemDepletion && item == mItem &&
      mgr.GetPlayerState(playerIndex)->GetItemAmount(item, true) == 0) {
    SendScriptMsgs(kSS_Zero, mgr, mgr.GetPlayer(playerIndex)->GetUniqueId(), kSM_None);
  }
}

void CScriptSpecialFunction::SendFrustumMessages(CStateManager& mgr) {
  // The two trailing arguments are left to their defaults deliberately: retail passes both call
  // sites the *same* outgoing stack slot at r1+8, and only the defaulted spelling makes MWCC fill
  // the arguments in after the frame layout so the slots are shared.
  if (mFrustumEntered) {
    mFrustumEntered = false;
    SendScriptMsgs(kSS_Entered, mgr);
  }
  if (mFrustumExited) {
    mFrustumExited = false;
    SendScriptMsgs(kSS_Exited, mgr);
  }
}

void CScriptSpecialFunction::PreRenderAllViewports(CStateManager& mgr) {
  // TODO: submit the per-viewport fog settings for the selected function.
}

CScriptSpecialFunction::~CScriptSpecialFunction() {}
