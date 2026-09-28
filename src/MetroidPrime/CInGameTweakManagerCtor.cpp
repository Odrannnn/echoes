/**
 * `CInGameTweakManager`'s default constructor - retail `fn_8016C230`, `.text:0x8016C230`,
 * `size:0x14` = 20 bytes, unnamed in `symbols.txt`:
 *
 *   8016c230  li   r0,0
 *   8016c234  stw  r0,4(r3)
 *   8016c238  stw  r0,8(r3)
 *   8016c23c  stw  r0,12(r3)
 *   8016c240  blr
 *
 * Three of the object's four words are zeroed and `+0x00` is left as the heap had it; the size,
 * 0x10, is the `li r3,16` in front of the `operator new` at 0x80008508 in
 * `CGameGlobalObjects`'s constructor (`src/MetroidPrime/CGameGlobalObjectsCtor.cpp`), which is
 * the only caller. `include/MetroidPrime/CInGameTweakManager.hpp` has the class and the
 * `extern "C"` declaration; the name is retail's address because the class's constructor is not
 * named anywhere in the DOL.
 *
 * It returns `this` because `CGameGlobalObjects`'s constructor stores what this returns
 * (`mr r0,r3` after the `bl`), and r3 is untouched here, so returning `self` costs nothing.
 */

#include "MetroidPrime/CInGameTweakManager.hpp"

extern "C" CInGameTweakManager* fn_8016C230(CInGameTweakManager* self) {
  self->mUnk4 = 0;
  self->mUnk8 = 0;
  self->mUnkC = 0;
  return self;
}
