#include "MetroidPrime/Player/CPlayer.hpp"

// NonMatching scaffold. Definitions are in reverse target order for deferred inlining.

void CPlayer::fn_8022c338(float dt, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::UpdatePlayerHints(CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

void CPlayer::ResetPlayerHintState(CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

bool CPlayer::SetAreaPlayerHint(const CScriptPlayerHint& hint, CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
  return false;
}

// Guessed name
bool CPlayer::FireBeamHeld(const CFinalInput& input) const {
  return !!(mControlMapper.GetDigitalInput(CControlMapper::kC_FireOrBomb, input) ||
            mControlMapper.GetDigitalInput(CControlMapper::kC_FireOrBomb2, input));
}

bool CPlayer::FireBeamPressed(const CFinalInput& input) const {
  return !!(mControlMapper.GetPressInput(CControlMapper::kC_FireOrBomb, input) ||
            mControlMapper.GetPressInput(CControlMapper::kC_FireOrBomb2, input));
}

bool CPlayer::fn_8022b974(const CFinalInput& input) const {
  bool result = false;
  if (mControlMapper.GetDigitalInput(CControlMapper::kC_Unknown15, input)) {
    result = true;
  }
  return result & 1;
}

// Guessed name
bool CPlayer::JumpHeld(const CFinalInput& input) const {
  return !!(mControlMapper.GetDigitalInput(CControlMapper::kC_JumpOrBoost, input) ||
            mControlMapper.GetDigitalInput(CControlMapper::kC_JumpOrBoost2, input));
}

// Guessed name
bool CPlayer::JumpPressed(const CFinalInput& input) const {
  return !!(mControlMapper.GetPressInput(CControlMapper::kC_JumpOrBoost, input) ||
            mControlMapper.GetPressInput(CControlMapper::kC_JumpOrBoost2, input));
}

bool CPlayer::fn_8022b7f4(const CFinalInput& input) const {
  return !!(mControlMapper.GetDigitalInput(CControlMapper::kC_ChargeBeam, input) ||
            mControlMapper.GetDigitalInput(CControlMapper::kC_ChargeBeam2, input));
}

bool CPlayer::fn_8022b7a8(const CFinalInput& input) const {
  bool result = false;
  if (mControlMapper.GetDigitalInput(CControlMapper::kC_Unknown73, input)) {
    result = true;
  }
  return result & 1;
}

// Guessed name
void CPlayer::UpdateRezbitRecoveryInput(const CFinalInput& input) {
  // TODO: Recover the remaining target behavior.
}

// Called from CPlayer::Freeze; retail clears the two Rezbit recovery fields.
extern "C" void fn_8022B6D0(CPlayer* self) {
  self->ResetRezbitRecoveryState();
}

CPlayer::ERezbitState CPlayer::GetRezbitState() const { return mRezbitState; }

// Guessed name
void CPlayer::SetRezbitState(ERezbitState state) {
  if (state == kRS_None || state == kRS_Recovered) {
    mRezbitState = state;
  }
}

void CPlayer::StartRezbitState(CStateManager& mgr, const CRezbitEffectOptions& options) {
  // TODO: Recover the remaining target behavior.
}

// Guessed name
void CPlayer::UpdateRezbitState(float dt) {
  // TODO: Recover the remaining target behavior.
}

// Guessed name
void CPlayer::BeginRezbitRecovery() {
  // TODO: Recover the remaining target behavior.
}

// Guessed name
void CPlayer::ResetRezbitState(CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

// Guessed name
void CPlayer::StopRezbitState(CStateManager& mgr) {
  // TODO: Recover the remaining target behavior.
}

TUniqueId CPlayer::fn_8022af0c(CStateManager& mgr, uint controls, TUniqueId source, float duration,
                               int breakType) {
  // TODO: Create a temporary control hint. Recover the shared break-hint enum.
  return kInvalidUniqueId;
}
