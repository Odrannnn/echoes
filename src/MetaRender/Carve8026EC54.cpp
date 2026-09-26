// `CCubeRenderer::SetAmbientColor`, carved out of dtk's `auto_03_8026258C_text` range.
// Retail .text 0x8026EC54..0x8026EC78, 0x24 = 36 bytes:
//
//     stwu r1,-16(r1) ; mflr r0 ; mr r3,r4 ; stw r0,20(r1)
//     bl <fn_802C1FE4> ; lwz r0,20(r1) ; mtlr r0 ; addi r1,r1,16 ; blr
//
// `fn_802C1FE4` is the unnamed CGraphics ambient-colour setter, and it is a real named symbol
// in retail's ELF, so the call resolves. **The `mr r3,r4` is the whole of the body**: `this` is
// overwritten by the argument, so retail does not touch the renderer at all - it forwards the
// `CColor&` and nothing else. The parameter is named in the header already
// (`SetAmbientColor(const CColor& color)`), so this needs no header change.
//
// Its own unit because `SetPerspective` (0x8026EC00..0x8026EC54) sits directly above and
// `EndPrimitive` (0x8026EC78..0x8026ECDC) directly below, and one unit may not claim two
// `.text` ranges in a section. Both are CCubeRenderer and both are left for a later lane:
// retail's two `SetPerspective` definitions are **overloads of one name**
// (`SetPerspective__13CCubeRendererFffff` and `...Fffffff`), which this tree spells as two
// differently-named virtuals, so writing either needs the header to overload the name - and
// the 52-byte one divides f2 by f3, scales by a `.sdata` float at 0x80418AE8 and shuffles
// f3/f4 before the call.

#include "MetaRender/CCubeRenderer.hpp"

extern "C" void fn_802C1FE4(const CColor&);

void CCubeRenderer::SetAmbientColor(const CColor& color) { fn_802C1FE4(color); }
