// `CTexture::InvalidateTexmap`, carved out of dtk's `auto_03_802C3E88_text` range.
// Retail .text 0x802C4248..0x802C425C, 0x14 = 20 bytes:
//
//     lwz  r4,-29304(r13)   ; the array base, .sdata 0x80418B08
//     slwi r0,r3,2          ; id * 4
//     li   r3,0
//     stwx r3,r4,r0
//     blr
//
// So the body is one indexed store of zero, and the index scale is what says the global is a
// word array indexed by `GXTexMapID`. `lbl_80418B08` is left unnamed by retail and is declared
// here as `uint*`, which is what `lwz` of it means - dtk gives it `size:0x8` because it is the
// distance to the next symbol, not because the object is eight bytes.
//
// The signature is already right: `include/Kyoto/Graphics/CTexture.hpp` declares
// `static void InvalidateTexmap(GXTexMapID id);`, and the mangled name
// `InvalidateTexmap__8CTextureF11_GXTexMapID` is what the five `CCubeRenderer::Load`
// paths and `CGX` call. No header change.
//
// Its own unit because `CTexture::GetConstBitMapData` (0x802C4700) is the next named function
// and the range between is dtk's; one unit may not claim two `.text` ranges in a section.

#include "Kyoto/Graphics/CTexture.hpp"

extern "C" uint* lbl_80418B08;

void CTexture::InvalidateTexmap(GXTexMapID id) { lbl_80418B08[id] = 0; }
