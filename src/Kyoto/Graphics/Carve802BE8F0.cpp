// `CGraphics::SetScreenPosition` - retail `SetScreenPosition__9CGraphicsFiii`,
// .text 0x802BE8F0..0x802BE9A4, 180 bytes:
//
//     802be8f0:  94 21 ff e0   stwu  r1,-32(r1)
//     802be8f4:  7c 08 02 a6   mflr  r0
//     802be8f8:  90 01 00 24   stw   r0,36(r1)
//     802be8fc:  93 e1 00 1c   stw   r31,28(r1)
//     802be900:  7c 9f 23 78   mr    r31,r4
//     802be904:  93 c1 00 18   stw   r30,24(r1)
//     802be908:  7c 7e 1b 78   mr    r30,r3
//     802be90c:  93 a1 00 14   stw   r29,20(r1)
//     802be910:  7c bd 2b 78   mr    r29,r5
//     802be914:  80 0d 9c 60   lwz   r0,-25504(r13)   ; 0x804199E0
//     802be918:  80 6d 9c 64   lwz   r3,-25500(r13)   ; 0x804199E4
//     802be91c:  7c 80 f0 51   subf. r4,r0,r30        ; dx = x - stretch
//     802be920:  80 0d 9c 68   lwz   r0,-25496(r13)   ; 0x804199E8
//     802be924:  7c a3 f8 50   subf  r5,r3,r31        ; dy = y - xOffset
//     802be928:  7c e0 e8 50   subf  r7,r0,r29        ; dz = z - yOffset
//     802be92c:  40 82 00 14   bne   802be940        ; if (dx) do the work
//     802be930:  2c 05 00 00   cmpwi r5,0
//     802be934:  40 82 00 0c   bne   802be940
//     802be938:  2c 07 00 00   cmpwi r7,0
//     802be93c:  41 82 00 4c   beq   802be988
//     802be940:  3c 60 80 41   lis   r3,-32703
//     802be944:  54 80 08 3c   slwi  r0,r4,1         ; 2 * dx
//     802be948:  38 63 72 64   addi  r3,r3,29284     ; &mRenderModeObj
//     802be94c:  7c 84 28 50   subf  r4,r4,r5        ; dx - dy
//     802be950:  a0 c3 00 0e   lhz   r6,14(r3)       ; rm->viWidth
//     802be954:  a0 a3 00 0a   lhz   r5,10(r3)       ; rm->viXOrigin
//     802be958:  7c c6 02 14   add   r6,r6,r0
//     802be95c:  a0 03 00 0c   lhz   r0,12(r3)       ; rm->viYOrigin
//     802be960:  7c 85 22 14   add   r4,r5,r4
//     802be964:  b0 c3 00 0e   sth   r6,14(r3)
//     802be968:  7c 00 3a 14   add   r0,r0,r7
//     802be96c:  b0 83 00 0a   sth   r4,10(r3)
//     802be970:  b0 03 00 0c   sth   r0,12(r3)
//     802be974:  48 09 55 05   bl    80353e78 <VIConfigure>
//     802be978:  48 09 60 9d   bl    80354a14 <VIFlush>
//     802be97c:  93 cd 9c 60   stw   r30,-25504(r13)  ; 0x804199E0 = x
//     802be980:  93 ed 9c 64   stw   r31,-25500(r13)  ; 0x804199E4 = y
//     802be984:  93 ad 9c 68   stw   r29,-25496(r13)  ; 0x804199E8 = z
//     ... epilogue
//
// **This is the function `src/Kyoto/Graphics/CGraphicsScreenPosition.cpp`'s header
// comment calls unwritable.** It says, in the version this lane started from:
//
//   "`SetScreenPosition` (0x802BE8F0, 0xB4) is retail's other half of this pair and is
//   the *writer* of all three words. It is not in this unit because it is 180 bytes of
//   register-allocated arithmetic over a 0x3C-byte render-mode object at 0x80417264
//   whose layout this tree does not model, and a `Matching` unit that guesses at that
//   layout reproduces nothing."
//
// The layout is modelled, and by the header: the three fields it touches are at
// **+0x0A, +0x0C and +0x0E**, read with `lhz` and written with `sth` - `u16` - and
// `GXRenderModeObj` (`include/dolphin/gx/GXStruct.h:41`) has `viXOrigin` at 0x0A,
// `viYOrigin` at 0x0C and `viWidth` at 0x0E, all `u16`. That is a three-way
// coincidence (offsets, width, signedness) and it is what makes this writable, so the
// comment's blocker is now a measurement rather than a guess. It is the same
// `mRenderModeObj` that `Carve802BEC24.cpp` reaches for the copy filter, and the same
// evidence settles that one's `aa` at +0x19, `sample_pattern` at +0x1A and `vfilter`
// at +0x32: retail reads those three and nothing else, and the object is `size:0x3C`,
// which is that struct's padded size.
//
// Three more things this settles that the comment could not:
//
//  * **The three arguments are differences, not absolutes.** `dx = x - stretch`,
//    `dy = y - xOffset`, `dz = z - yOffset`, and the render mode is *adjusted*:
//    `viWidth += 2*dx`, `viXOrigin += dx - dy`, `viYOrigin += dz`. The `2*` on the
//    width is why `dx` is doubled in `slwi` and is the whole reason the function
//    exists - the mode's width tracks a horizontal stretch twice as hard as the
//    origin moves.
//  * **It is a no-op when nothing changed**, and the test is three `!=` in argument
//    order with the last one folded into the return path (`beq` past the body), which
//    is the same MWCC shape as `GetScreenPosition`'s three null tests. The
//    `subf.`/`cmpwi` chain also fixes the *operand order* of each comparison - it is
//    `x - stretch`, not `stretch - x` - which is the rule that decides register
//    allocation, and the source below is written to match it.
//  * **The three words are only written after `VIConfigure` and `VIFlush` return.**
//    So the cached screen position and the VI state are updated in that order, and a
//    reader that sees the cache has already been told.
//
// The globals are the same three `lbl_` words `GetScreenPosition` reads, and for the
// same reason: `CGraphics` has no `.cpp` that could define the C++-named members, and
// a `Matching` object is linked into the DOL where only the retail names exist. It is
// a *port* link symbol (`_ZN9CGraphics17SetScreenPositionEiii` is on the gap
// ratchet), and it is the writer the reader in that unit was waiting for.
#include "Kyoto/Graphics/CGraphics.hpp"

#include <dolphin/vi.h>

extern "C" {
extern int lbl_804199E0;
extern int lbl_804199E4;
extern int lbl_804199E8;
extern GXRenderModeObj mRenderModeObj__9CGraphics;
}

void CGraphics::SetScreenPosition(int stretch, int xOffset, int yOffset) {
  // The three differences are computed **before** the test, and the test is on the
  // differences (`subf.`/`cmpwi`/`cmpwi` on dx, dy, dz) rather than on the values
  // (`cmpw` x3). Putting them inside the `if` gives three `cmpw`s and forces the
  // globals to be re-loaded inside the body, which is nine instructions of
  // difference and four more bytes of `.text`.
  const int dx = stretch - lbl_804199E0;
  const int dy = xOffset - lbl_804199E4;
  const int dz = yOffset - lbl_804199E8;
  if (dx != 0 || dy != 0 || dz != 0) {
    GXRenderModeObj& rm = mRenderModeObj__9CGraphics;
    rm.viWidth = rm.viWidth + 2 * dx;
    // **`dy - dx`, and grouped.** Retail's `subf r4,r4,r5` computes `r5 - r4` on
    // the PowerPC (`subf rA,rB,rC` is `rC - rB`), i.e. `dy - dx` and *not* `dx - dy`;
    // written ungrouped the same statement compiles to `(viXOrigin + dx) - dy` and
    // puts the three loads in a different order. The sign is a real behavioural
    // difference, not a register choice, and it is the one thing in this file a
    // percentage would have hidden.
    rm.viXOrigin = rm.viXOrigin + (dy - dx);
    rm.viYOrigin = rm.viYOrigin + dz;
    VIConfigure(&rm);
    VIFlush();
    lbl_804199E0 = stretch;
    lbl_804199E4 = xOffset;
    lbl_804199E8 = yOffset;
  }
}
