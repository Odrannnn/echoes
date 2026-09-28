#include "MetroidPrime/Player/CMorphBall.hpp"

// Retail 0x800C08D4. The morphball's own state (retail 0xC80) is 4, 5 or 6 while the ball counts
// as ball-shaped; the gun's state handlers and every actor in the room ask this before touching
// it. Named for the address because retail's symbol table has no name for it - the port calls it
// as `fn_800C08D4` from `CPlayerGun::InMorphball`, which is one of its 31 callers.
//
// Upstream calls retail's 0xC80 `CMorphBall::mBallState` and has recovered the values: its
// `CMorphBall::InScrewAttackMode()` (src/MetroidPrime/Player/CMorphBall.cpp) is the same test on
// the same member - `kBS_ScrewAttack` (4), `kBS_ScrewAttackWallJump` (5), `kBS_ScrewAttackRecovery`
// (6) - so 4/5/6 here are those three enumerators and not magic numbers. `mBallState` is private,
// so this reads it through the public `GetBallState()`.
extern "C"
bool fn_800C08D4(const CMorphBall* self) {
  const CMorphBall::EBallState state = self->GetBallState();
  return state == CMorphBall::kBS_ScrewAttack || state == CMorphBall::kBS_ScrewAttackWallJump ||
         state == CMorphBall::kBS_ScrewAttackRecovery;
}
