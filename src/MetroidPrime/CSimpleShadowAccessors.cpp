// Retail `GetTransform__13CSimpleShadowCFv` = `_ZNK13CSimpleShadow12GetTransformEv`,
// .text 0x800DF478, 0x800DF478..0x800DF47C, 4 bytes:
//
//     800df478  blr
//
// A four-byte accessor that returns `this->x0_xf` unchanged: the member is at +0, so mwcceppc
// has no work to do and does not even materialise an address. That makes it the cheapest
// possible confirmation that a single function can be carved out of a dtk `auto_03_*` range and
// linked as a `Matching` unit of its own - the range is one word wide and the two retail
// functions either side of it (`fn_800DF470` at 0x800DF470 and `fn_800DF47C`, which is
// `Render`, at 0x800DF47C) stay where they are.
//
// The neighbouring block is retail's whole `CSimpleShadow` group: the destructor
// `fn_800DF204` (0x64), `Valid` (0x800DF268, 12), `Calculate` (0x800DF274, 0x168), `GetBounds`
// (0x800DF3DC, 0x7C), `SetAlwaysCalculateRadius` (0x800DF458, 16), `GetMaxObjectHeight`
// (`fn_800DF468`, 8), `SetUserAlpha` (`fn_800DF470`, 8), this, and `Render` (0x800DF47C,
// 0x1B4). Only the four retail functions whose *names* symbols.txt carries can be written in
// C++ and paired by objdiff; `fn_800DF468` and `fn_800DF470` are unnamed in the DOL, so writing
// them as methods would add two functions the retail object does not define and the unit could
// never be `Matching`.
#include "MetroidPrime/CSimpleShadow.hpp"

const CTransform4f& CSimpleShadow::GetTransform() const { return x0_xf; }
