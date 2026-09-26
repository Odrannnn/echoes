// `CGraphics::SetFog` - retail `SetFog__9CGraphicsF11ERglFogModeffRC6CColor`,
// .text 0x802BF9C8..0x802BF9F8, 48 bytes:
//
//     802bf9c8:  94 21 ff f0   stwu  r1,-16(r1)
//     802bf9cc:  7c 08 02 a6   mflr  r0
//     802bf9d0:  3c a0 80 41   lis   r5,-32703      ; 0x8041 << 16
//     802bf9d4:  90 01 00 14   stw   r0,20(r1)
//     802bf9d8:  38 a5 6f 28   addi  r5,r5,28456     ; r5 = &mProj
//     802bf9dc:  c0 65 00 14   lfs   f3,20(r5)       ; mProj.x14_near
//     802bf9e0:  c0 85 00 18   lfs   f4,24(r5)       ; mProj.x18_far
//     802bf9e4:  4b ff df 11   bl    802bd8f4 <CGX::SetFog>
//     802bf9e8:  80 01 00 14   lwz   r0,20(r1)
//     802bf9ec:  7c 08 03 a6   mtlr  r0
//     802bf9f0:  38 21 00 10   addi  r1,r1,16
//     802bf9f4:  4e 80 00 20   blr
//
// This is the wrapper that says what `CGraphics`'s own fog interface *is*: retail's
// `ERglFogMode mode, float startz, float endz` carries only three of the five numbers
// `CGX::SetFog` takes, and the other two are the **current projection's** near and far
// planes, read out of the same `mProj` that `Carve802BF59C.cpp` hands back a reference
// to. So `CGraphics::SetFog` is not a call into the GX SDK at all; it is
// `CGX::SetFog(mode, startz, endz, mProj.GetNear(), mProj.GetFar(), color)` with no
// arithmetic of its own, which is why it is 48 bytes and all frame.
//
// The three register facts that make the one-liner below compile to those bytes:
//
//  * It is a `static` member function, so r3/f1/f2/r4 are already the four declared
//    arguments on entry; `this` never appears. r5 is the first free register, and
//    `&mProj` is the first temporary - it gets r5, which is also why the address is
//    built with `lis`+`addi` rather than an SDA21 (a `CProjectionState` is 0x1C bytes).
//  * `f3` and `f4` are the two *new* float arguments, and they are loaded in member
//    order (+0x14 then +0x18) before the call, so the source order is
//    `GetNear(), GetFar()` and not the other way round. `CProjectionState`'s declared
//    order is `..., x10_bottom, x14_near, x18_far`, so these are near then far.
//  * The `const CColor&` is passed straight through: r4 is never touched, so the
//    `CColor` -> `GXColor` conversion is a reinterpret, and `CColor` already has the
//    accessor for it (`GetGXColor()`, `include/Kyoto/Graphics/CColor.hpp:58`).
//
// `CGX::SetFog` is at 0x802BD8F4, inside `Kyoto/Graphics/CGX.cpp`'s claimed
// 0x802BCBEC..0x802BE31C, so the callee is defined by an existing unit and nothing
// here is a new link symbol except this function itself.
#include "Kyoto/Graphics/CGX.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"

extern "C" {
extern CGraphics::CProjectionState lbl_80416F28;
}

void CGraphics::SetFog(ERglFogMode mode, float startz, float endz, const CColor& color) {
  CGX::SetFog(static_cast< GXFogType >(mode), startz, endz, lbl_80416F28.GetNear(),
              lbl_80416F28.GetFar(), color.GetGXColor());
}
