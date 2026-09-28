// Retail `SetDirtyFlags__6CActorFv`, .text 0x8004A0A0..0x8004A0D8, 0x38 = 56 bytes:
//
//     8004a0a0  lbz     r0,336(r3)      ; 0x150
//     8004a0a4  li      r4,1
//     8004a0a8  rlwimi  r0,r4,4,27,27
//     8004a0ac  stb     r0,336(r3)
//     8004a0b0  lbz     r0,336(r3)
//     8004a0b4  rlwimi  r0,r4,3,28,28
//     8004a0b8  stb     r0,336(r3)
//     8004a0bc  lbz     r0,336(r3)
//     8004a0c0  rlwimi  r0,r4,2,29,29
//     8004a0c4  stb     r0,336(r3)
//     8004a0c8  lbz     r0,336(r3)
//     8004a0cc  rlwimi  r0,r4,1,30,30
//     8004a0d0  stb     r0,336(r3)
//     8004a0d4  blr
//
// **Which declared field each `rlwimi` is: `m_notInSortedLists`, `m_transformDirty`,
// `m_actorLightsDirty`, `m_renderBoundsDirty` - in that order, and `include/MetroidPrime/CActor.hpp`
// already declares them so that they encode that way. No header change was needed, which is the
// finding: a previous lane recorded this as blocked on splitting the 0x150 group into `u8`s, and it
// is not.** `tools/probe_cactor_offsets.py` emits one setter per bitfield, compiles it with
// **mwcceppc's own flags** (the host compiler is 64-bit and is no evidence about anything MWCC
// emits), and reads the byte and merge range out of the object:
//
//     m_notInSortedLists    0x150  rlwimi mb=27     <- retail's 1st
//     m_transformDirty      0x150  rlwimi mb=28     <- retail's 2nd
//     m_actorLightsDirty    0x150  rlwimi mb=29     <- retail's 3rd
//     m_renderBoundsDirty   0x150  rlwimi mb=30     <- retail's 4th
//
// **Why the `lbz`/`stb` is not evidence that the group is byte-sized.** mwcceppc packs a 32-bit
// bitfield unit MSB-first and then picks the narrowest access that covers the field, so a one-bit
// field in the first byte of the group compiles to `lbz`/`stb` on 0x150 with `mb` = 24 + that field's
// index *within the byte*. Retail's byte access is therefore reproduced by a `uint` group, and
// splitting the group into `u8`s would have been the wrong fix. The same table is what makes the
// three already-matching accessors checkable rather than merely consistent: `m_muted` is
// 0x151/mb=27, `m_useInSortedLists` 0x151/mb=28 and `m_callTouch` 0x151/mb=30, which are retail's
// `SetMuted` (0x8004B600), `SetUseInSortedLists` (0x8004C53C) and `SetCallTouch` (0x8004C514)
// byte for byte - including `SetMuted` calling `RemoveEmitter` and the other two being bare
// read/modify/write pairs.
//
// `m_renderBoundsDirty` is the fourth flag that the three-line version of this body left out, and
// it is the one that makes the function coherent: `CalculateRenderBounds` tests it and clears it
// (`src/MetroidPrime/CActor.cpp`, the `if (m_renderBoundsDirty)` at the end), so this is its
// setter. It is the only flag in the group with no `Set*` accessor in the header, so it is written
// through the member.
//
// Its own unit: the function is at 0x8004A0A0, which is inside the unclaimed dtk fill range
// 0x80049ED8..0x8004AC98 and **not** inside `MetroidPrime/CActor.cpp`'s claimed range
// (0x8004AC98..0x8004E448), so objdiff cannot pair it with that unit's object however well the body
// compiles - defining it in CActor.cpp reproduced all 56 bytes and still counted for nothing. A
// unit may not claim two discontiguous ranges in one section, hence the file of its own.
#include "MetroidPrime/CActor.hpp"

void CActor::SetDirtyFlags() {
  SetTransformDirty(true);
  SetTransformDirtySpare(true);
  SetPreRenderHasMoved(true);
  mRenderBoundsDirty = true;
}
