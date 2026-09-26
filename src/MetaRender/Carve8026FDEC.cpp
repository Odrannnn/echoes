// `CCubeRenderer::SetModelMatrix`, carved out of dtk's `auto_03_8026258C_text` range.
// Retail .text 0x8026FDEC..0x8026FE10, 0x24 = 36 bytes:
//
//     stwu r1,-16(r1) ; mflr r0 ; mr r3,r4 ; stw r0,20(r1)
//     bl <SetModelMatrix__9CGraphicsFRC12CTransform4f>
//     lwz r0,20(r1) ; mtlr r0 ; addi r1,r1,16 ; blr
//
// **The `mr r3,r4` is the whole body: `this` is overwritten and the renderer is never read.**
// The method is a member because it is vtable slot 14 of `__vt__13CCubeRenderer` (measured, see
// include/MetaRender/CCubeRenderer.hpp), not because it uses the object. `CGraphics` has no
// `.cpp` in the tree, so `CGraphics::SetModelMatrix` is declared in the header and defined by
// dtk's `auto_03_802BF640_text` range - it is a real named symbol in `main.elf`, so the call
// resolves. The parameter is already named in the header
// (`SetModelMatrix(const CTransform4f& xf)`), so this needs no header change.
//
// Its own unit because `SetWorldViewpoint` (0x8026FD7C..0x8026FDEC) sits directly above and
// `RemoveStaticGeometry` (0x8026FE10) directly below. `SetWorldViewpoint` is CCubeRenderer too
// and is left for a later lane: retail's is
// `SetWorldViewpoint__13CCubeRendererFRC12CTransform4f` and this header declares
// `SetWorldViewpoint()`, so its name does not mangle the way retail's does.

#include "Kyoto/Graphics/CGraphics.hpp"
#include "MetaRender/CCubeRenderer.hpp"

void CCubeRenderer::SetModelMatrix(const CTransform4f& xf) { CGraphics::SetModelMatrix(xf); }
