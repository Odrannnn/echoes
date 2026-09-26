// CGraphics' screen-position accessor, `GetScreenPosition`.
//
//   802be9a4 <GetScreenPosition__9CGraphicsFPiPiPi>:   size:0x34
//   802be9a4:  28 03 00 00   cmplwi r3,0
//   802be9a8:  41 82 00 0c   beq   802be9b4
//   802be9ac:  80 0d 9c 60   lwz   r0,-25504(r13)   ; 0x804199E0
//   802be9b0:  90 03 00 00   stw   r0,0(r3)
//   802be9b4:  28 04 00 00   cmplwi r4,0
//   802be9b8:  41 82 00 0c   beq   802be9c4
//   802be9bc:  80 6d 9c 64   lwz   r0,-25500(r13)   ; 0x804199E4
//   802be9c0:  90 04 00 00   stw   r0,0(r4)
//   802be9c4:  28 05 00 00   cmplwi r5,0
//   802be9c8:  4d 82 00 20   beqlr
//   802be9cc:  80 6d 9c 68   lwz   r0,-25496(r13)   ; 0x804199E8
//   802be9d0:  90 05 00 00   stw   r0,0(r5)
//   802be9d4:  4e 80 00 20   blr
//
// Three independently null-tested loads out of three `.sbss` words, in the
// argument order, and the third test is the one MWCC folds into the return
// (`beqlr`), which is why the source below has no `return` and no fourth
// statement: the shape is produced by the compiler from three `if`s, not written.
//
// **The three addresses are the one thing in this file that is easy to get wrong,
// and this lane got them wrong twice.** The exact rule, measured rather than
// reasoned: an SDA21 field is **the full signed displacement, not half of it**,
//
//     field = (address - _SDA_BASE_) & 0xFFFF        _SDA_BASE_ = 0x8041FD80
//
// so `9c 60` is -25504 and the address is 0x8041FD80 - 25504 = **0x804199E0**.
// Retail's three fields are `9c 60` / `9c 64` / `9c 68` (DOL file offsets
// 0x2BB7B0 / 0x2BB7C0 / 0x2BB7D0, read out of
// `orig/G2ME01/sys/main.dol` with `cmp -l`), i.e. 0x804199E0 / 0x804199E4 /
// 0x804199E8.
//
// The first attempt used 0x804199D0/D4/D8 and the second 0x804199E4/E8/EC; each
// was off by a multiple of 4, each produced an object that is byte-identical in
// its own right, paired at 100% under objdiff, passed `unit_fit.sh` as "fits, no
// extra functions" - and each broke the DOL's sha1 on exactly three bytes, which
// is the only instrument that noticed. `tools/flip_test.sh` is what caught it and
// its message ("the REBUILD FAILED - do not trust build/ until it is green again")
// is the one to read first when a new unit's object looks perfect. Do the
// subtraction in a script; a 64-bit host doing 0xFD80 - 0x639C in your head is
// exactly where this goes wrong.
//
// These three words are adjacent to, and distinct from, the two that
// `CGraphicsTimeProvider.cpp` reaches at 0x804199D8 and 0x804199DC, so the two
// units' sets do not overlap and each is a real separate object. Retail leaves all
// five unnamed; `include/Kyoto/Graphics/CGraphics.hpp:428-432` invents
// `mSecondsMod900`, `mpExternalTimeProvider`, `mScreenStretch`,
// `mScreenPositionX` and `mScreenPositionY` for the same five words, and a
// `Matching` unit can only reference the `lbl_` spellings, because `CGraphics` has
// no `.cpp` in the tree that could define a C++-named static data member. Read
// `src/MetroidPrime/PortGlobals.cpp` before touching either spelling.
//
// `SetScreenPosition` (0x802BE8F0, 0xB4) is retail's other half of this pair and is
// the *writer* of all three words. It is not in this unit because it is 180 bytes
// of register-allocated arithmetic over a 0x3C-byte render-mode object at
// 0x80417264 whose layout this tree does not model, and a `Matching` unit that
// guesses at that layout reproduces nothing.

#include "Kyoto/Graphics/CGraphics.hpp"

extern "C" {
extern int lbl_804199E0;
extern int lbl_804199E4;
extern int lbl_804199E8;
}

void CGraphics::GetScreenPosition(int* stretch, int* xOffset, int* yOffset) {
  if (stretch != nullptr) {
    *stretch = lbl_804199E0;
  }
  if (xOffset != nullptr) {
    *xOffset = lbl_804199E4;
  }
  if (yOffset != nullptr) {
    *yOffset = lbl_804199E8;
  }
}
