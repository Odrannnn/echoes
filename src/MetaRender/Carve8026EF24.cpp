// The two `CCubeRenderer::PrimColor` overloads, carved out of dtk's
// `auto_03_8026258C_text` range. Retail .text 0x8026EF24..0x8026EF54, 0x30 = 48 bytes, two
// adjacent functions and so one unit:
//
//   8026ef24 <PrimColor__13CCubeRendererFRC6CColor>:   0x0C = 12
//   8026ef30 <PrimColor__13CCubeRendererFffff>:        0x24 = 36
//
// The 12-byte one is a **whole-word** copy:
//
//     lwz r0,0(r4) ; stw r0,844(r3) ; blr
//
// which is the whole of the assignment and is the reason the member at +0x34C is a 4-byte
// `CColor`: retail assigns the object, it does not copy four channels, and a
// `CColor` here is a single `uint` (see `include/Kyoto/Graphics/CColor.hpp`). Spelling it
// `mPrimColor = color;` is the only thing that emits one `lwz`/`stw` pair.
//
// The 36-byte one moves `this+0x34C` into r3 and calls
// `Set__6CColorFffff` - `CColor::Set(float, float, float, float)` - with the four float
// arguments in f1..f4:
//
//     stwu r1,-16(r1) ; mflr r0 ; addi r3,r3,844 ; stw r0,20(r1)
//     bl <Set__6CColorFffff> ; lwz r0,20(r1) ; mtlr r0 ; addi r1,r1,16 ; blr
//
// so the body is `mPrimColor.Set(r, g, b, a);` and the *call target is part of the bytes* -
// `Set__6CColorFffff` is a real named symbol in retail's ELF, so the call resolves.
//
// The offset is why `include/MetaRender/CCubeRenderer.hpp` names `mPrimColor`: retail's body
// reads and writes +0x34C and a `uchar pad[]` cannot be written to. Upstream spells the member
// `mPrimColor`, and it is the same member at the same offset:
// `include/MetaRender/CCubeRenderer.hpp` declares **`CColor mPrimColor;`** then
// `CVector3f mPrimNormal;` then `CColor mWorldLightColor;`, and the pre-merge header had
// **`x34c_color` (0x34C, 4 bytes)** then `x350_normal` (0x350) then `x35c_color` (0x35C) - so the
// two agree member for member from 0x34C on and `mPrimColor` is 0x34C. The class's full offset
// table is in the pre-merge header's comment; `CHECK_SIZEOF(CCubeRenderer, 0x560)` is unchanged.
//
// Its own unit because `PrimVertex` (0x8026ECF8..0x8026ED44) and `BeginPrimitive`
// (0x8026EE0C..0x8026EF24) bracket it, and one unit may not claim two `.text` ranges in a
// section. Both are CCubeRenderer and both are left for a later lane; see
// `MetaRender/Carve8026ECDC.cpp` for why.
//
// Declared descending by retail offset: mwcceppc emits in reverse source order.

#include "MetaRender/CCubeRenderer.hpp"

void CCubeRenderer::PrimColor(float r, float g, float b, float a) {
  mPrimColor.Set(r, g, b, a);
}

void CCubeRenderer::PrimColor(const CColor& color) { mPrimColor = color; }
