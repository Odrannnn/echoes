/**
 * The constructor of `CGameGlobalObjects`'s last member (+0x150, 0x14 bytes) - retail
 * `fn_801F0A44`, `.text:0x801F0A44`, `size:0x30` = 48 bytes, unnamed in `symbols.txt`:
 *
 *   801f0a44  stwu r1,-16(r1)
 *   801f0a48  li   r0,0
 *   801f0a4c  lbz  r5,8(r1)       ; an uninitialised byte of this frame
 *   801f0a50  lbz  r4,12(r1)      ; and another, four bytes further up
 *   801f0a54  stb  r5,0(r3)
 *   801f0a58  stb  r4,1(r3)
 *   801f0a5c  stw  r0,4(r3)  ... stw r0,16(r3)
 *   801f0a6c  addi r1,r1,16
 *   801f0a70  blr
 *
 * `CGameGlobalObjects`'s constructor calls it on `this+0x150` at 0x80008524 and publishes that
 * address in `lbl_80418EC8`; `include/MetroidPrime/CGameGlobalObjects.hpp` has the member
 * (`CGameGlobalObjectsTail`) and the `extern "C"` declaration.
 *
 * The two bytes at +0/+1 are what an empty-constructed one-byte object looks like in mwcceppc:
 * two locals nobody initialised, copied into the member. C++ has no spelling for "an
 * uninitialised byte", and `src/MetroidPrime/Player/CPersistentOptionsCtor.cpp` measured the
 * four candidates on exactly this shape (two bytes four apart at 8(r1) and 12(r1)); one
 * `volatile` 8-byte local read as two bytes four apart is the one that reproduces it, and it is
 * the spelling used here.
 */

#include "types.h"

struct SGameGlobalObjectsTail {
  u8 x0;   //!< +0x00 - an uninitialised frame byte
  u8 x1;   //!< +0x01 - another
  u8 x2_unk[2];
  u32 x4;  //!< +0x04
  u32 x8;  //!< +0x08
  u32 xc;  //!< +0x0C
  u32 x10; //!< +0x10
};
CHECK_SIZEOF(SGameGlobalObjectsTail, 0x14)

extern "C" void fn_801F0A44(void* self) {
  SGameGlobalObjectsTail* p = static_cast< SGameGlobalObjectsTail* >(self);
  volatile u32 w[2];

  p->x0 = *reinterpret_cast< volatile u8* >(&w[0]);
  p->x1 = *reinterpret_cast< volatile u8* >(&w[1]);
  p->x4 = 0;
  p->x8 = 0;
  p->xc = 0;
  p->x10 = 0;
}
