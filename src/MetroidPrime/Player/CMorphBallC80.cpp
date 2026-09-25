#include "MetroidPrime/Player/CMorphBall.hpp"

// Retail 0x800C08D4. The morphball's own state byte (0xC80) is 4, 5 or 6 while the ball counts as
// ball-shaped; the gun's state handlers and every actor in the room ask this before touching it.
// Named for the address because retail's symbol table has no name for it - the port calls it as
// `fn_800C08D4` from `CPlayerGun::InMorphball`, which is one of its 31 callers.
extern "C"
bool fn_800C08D4(const CMorphBall* self) {
  bool result = false;
  if (self->xc80_maybe_sa_state == 4 || self->xc80_maybe_sa_state == 5 ||
      self->xc80_maybe_sa_state == 6) {
    result = true;
  }
  return result;
}
