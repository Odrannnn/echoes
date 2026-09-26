// CPlayer::GetTweakPlayer - retail 0x8000BF94, 0x18 bytes.
//
// Disassembly, in full (build/binutils/powerpc-eabi-objdump -d build/G2ME01/main.elf):
//
//   8000bf94: 80 03 13 20   lwz    r3,4896(r3)      ; m_x1320_tweakPlayerSlot
//   8000bf98: 80 6d 91 c4   lwz    r3,-28220(r13)    ; gpTweakPlayerA
//   8000bf9c: 2c 00 00 01   cmpwi  r0,1
//   8000bfa0: 4c 82 00 20   bnelr
//   8000bfa4: 80 6d 91 c0   lwz    r3,-28224(r13)    ; gpTweakPlayerB
//   8000bfa8: 4e 80 00 20   blr
//
// So the second tweak-player pointer is only loaded on the equal branch, and the
// A pointer is what the `!= 1` path returns - it is loaded *before* the compare, into
// the return register, and `bnelr` hands it back. That is the shape of a value
// materialised first and conditionally overwritten, not of an if/else with two
// returns: `return x == 1 ? b : a;` compiles under mwcceppc to `cmpwi; bne; lwz b;
// blr; lwz a; blr` - 0x1C bytes with two `blr`s and the loads the other way round.
// Stating the default as a local is what reproduces retail's six instructions.
//
// `fn_8000BF7C` (0x8000BF7C, 0x18) is the same six instructions against the other
// pair of tweak-player globals (-28212/-28216(r13)), so this shape is a class method
// that appears at least twice and is not specific to CPlayer.

#include "MetroidPrime/Player/CPlayer.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"

CTweakPlayer* CPlayer::GetTweakPlayer() const {
  CTweakPlayer* tweak = gpTweakPlayerA;
  if (m_x1320_tweakPlayerSlot == 1) {
    tweak = gpTweakPlayerB;
  }
  return tweak;
}
