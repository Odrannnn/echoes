// `CCubeRenderer::PrimNormal`, carved out of dtk's `auto_03_8026258C_text` range.
// Retail .text 0x8026ECDC..0x8026ECF8, 0x1C = 28 bytes:
//
//     lfs  f0,0(r4) ; lfs f1,4(r4) ; stfs f0,848(r3)
//     lfs  f0,8(r4) ; stfs f1,852(r3) ; stfs f0,856(r3) ; blr
//
// Three loads and three stores, in an order the compiler chose: retail copies the vector
// **whole**, it does not assign three channels - the interleaving is what says so, and four
// separate `x350_normal.x = nrm.x; ...` statements would not produce this order. The three
// destinations are +0x350, +0x354 and +0x358, so the member is a `CVector3f` there.
//
// The offset is why `include/MetaRender/CCubeRenderer.hpp` names `x350_normal`: retail's body
// writes it and a `uchar pad[]` cannot be written to. The header edit is layout-neutral for
// everything that was already right - the three bitfields at 0x318 have not moved - and its
// full reasoning is in the header.
//
// Its own unit because `EndPrimitive` (0x8026EC78..0x8026ECDC) sits directly above and
// `PrimVertex` (0x8026ECF8..0x8026ED44) directly below, and one unit may not claim two
// `.text` ranges in a section. Both are CCubeRenderer and both are left for a later lane:
// `PrimVertex` stores to a hard-coded 0xCC008000 (the EFB mirror) with the vertex count
// written back to +0x18 but unused for the address, and `EndPrimitive`'s 100 bytes are a
// `do`/`while` over an unnamed vtable slot 45.

#include "MetaRender/CCubeRenderer.hpp"

void CCubeRenderer::PrimNormal(const CVector3f& nrm) { x350_normal = nrm; }
