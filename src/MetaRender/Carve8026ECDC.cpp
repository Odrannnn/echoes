// `CCubeRenderer::PrimNormal`, carved out of dtk's `auto_03_8026258C_text` range.
// Retail .text 0x8026ECDC..0x8026ECF8, 0x1C = 28 bytes:
//
//     lfs  f0,0(r4) ; lfs f1,4(r4) ; stfs f0,848(r3)
//     lfs  f0,8(r4) ; stfs f1,852(r3) ; stfs f0,856(r3) ; blr
//
// Three loads and three stores, in an order the compiler chose: retail copies the vector
// **whole**, it does not assign three channels - the interleaving is what says so, and four
// separate `mPrimNormal.x = nrm.x; ...` statements would not produce this order. The three
// destinations are +0x350, +0x354 and +0x358, so the member is a `CVector3f` there.
//
// The offset is why `include/MetaRender/CCubeRenderer.hpp` names `mPrimNormal`: retail's body
// writes it and a `uchar pad[]` cannot be written to. Upstream spells the member `mPrimNormal`,
// and it is the same member at the same offset: `include/MetaRender/CCubeRenderer.hpp` declares
// `CColor mPrimColor;` then **`CVector3f mPrimNormal;`** then `CColor mWorldLightColor;`, and the
// pre-merge header had `x34c_color` (0x34C, 4 bytes) then **`x350_normal` (0x350, 12 bytes)** then
// `x35c_color` (0x35C) - so the two agree member for member from 0x34C on, `mPrimNormal` is
// 0x350, and it is the `CVector3f` those three `lfs` load into. The class's full offset table is
// in the pre-merge header's comment; `CHECK_SIZEOF(CCubeRenderer, 0x560)` is unchanged.
//
// Its own unit because `EndPrimitive` (0x8026EC78..0x8026ECDC) sits directly above and
// `PrimVertex` (0x8026ECF8..0x8026ED44) directly below, and one unit may not claim two
// `.text` ranges in a section. Both are CCubeRenderer and both are left for a later lane:
// `PrimVertex` stores to a hard-coded 0xCC008000 (the EFB mirror) with the vertex count
// written back to +0x18 but unused for the address, and `EndPrimitive`'s 100 bytes are a
// `do`/`while` over an unnamed vtable slot 45.

#include "MetaRender/CCubeRenderer.hpp"

void CCubeRenderer::PrimNormal(const CVector3f& nrm) { mPrimNormal = nrm; }
