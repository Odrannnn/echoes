// Retail `SetAreaAttributes__9CGameAreaFP21CScriptAreaProperties`
// = `_ZN9CGameArea17SetAreaAttributesEP21CScriptAreaProperties`,
// .text 0x80055768..0x80055774, 0xC = 12 bytes:
//
//     80055768  lwz  r3,260(r3)      ; 0x104
//     8005576c  stw  r4,312(r3)      ; 0x138
//     80055770  blr
//
// Two memory operations and no arithmetic: the area's `m_postConstructed` (at +0x104) is
// followed, and the script object pointer lands in that post-construction block's +0x138.
// `CScriptAreaProperties::Load` is what calls this, and its `CScriptAreaProperties*`
// parameter arrives in `r4` - the *second* register after `this`, which is why the store
// takes `r4` and not `r3`.
//
// **+0x138 is inside what the header called padding.** `CGameArea::CPostConstructed` was
// `char pad1[0x13c]` followed by `m_occlusionState` at 0x13c, so the store's target was the
// last four bytes of the pad. `pad1` is now `char pad1[0x138]` plus a named
// `CScriptAreaProperties* x138_areaProperties`, which is the same size (0x13c) and the same
// offsets for everything after it - the split only names the field retail writes. That is
// the minimum edit that makes the body say what it does; it is not a claim about what the
// other 0x138 bytes are.
//
// Its own unit because it is 0x2C bytes into dtk's `auto_03_80052880_text` and the functions
// either side are unclaimed.
#include "MetroidPrime/CGameArea.hpp"

void CGameArea::SetAreaAttributes(CScriptAreaProperties* props) {
  mPostConstructed->mAreaAttributes = props;
}
