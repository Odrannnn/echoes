#include "MetroidPrime/CModelData.hpp"

#include "Kyoto/Graphics/CModel.hpp"

// Retail 0x80018FBC .. 0x800190F8 (`__ct__10CModelDataFRC10CModelData`, 0x13C = 316 bytes):
// `CModelData`'s copy constructor. It is the only code in the DOL that writes all nine members,
// and reading it store by store is what fixes the layout of the class - including the four bytes
// at +0x10 that `CModelDataModelSlots.cpp` used to call an unidentified pointer:
//
//   +0x00..0x0B  x0_scale, three `lfs`/`stfs`
//   +0x0C        `xc_animData.x0_has`, `lbz`/`stb`
//   +0x10        `xc_animData.x4_item`, `lwz`/`stw`
//   +0x0C        **the source's `x0_has` is then set to 0** (`stb r0,12(r4)` with `r0` the
//                `li r0,0` at the function's top) - an `rstl::auto_ptr` copy *steals*, and
//                `include/rstl/auto_ptr.hpp`'s copy constructor already does exactly that
//   +0x14        the four bit-fields, copied as one byte (`lbz`/`stb`)
//   +0x18        x18_ambientColor, one word
//   +0x28/0x38/0x48  the three `optional_object` validity flags, then, for each one that is set,
//                `__ct__6CTokenFRC6CToken` + `dst.x8 = src.x8` + `Lock__6CTokenFv`
//
// The `Lock()` call is the whole reason the three members are `TLockedToken<CModel>` and not
// `TCachedToken<CModel>`: `TLockedToken`'s copy constructor
// (`include/Kyoto/TToken.hpp:67`) is `x0_token(token); x8_item(*token); x0_token.Lock();`, and
// those three statements are retail's three calls in retail's order. `TCachedToken` has no
// `Lock()` in its implicit copy, so it compiles 12 bytes short - once per member - with the
// per-function diff reporting nothing. The header was corrected from `TCachedToken` to
// `TLockedToken`; both are 0x10 bytes with the flag at +0xC and the value at +8, so the class
// layout and every other member are unchanged, and `CModelDataDefaultCtor.cpp` (which only writes
// `.m_valid`) still compiles to the same 152 bytes.
//
// The four bit-fields have to be in the mem-init list, not in the body. Retail writes the
// `lbz`/`stb` pair for 0x14 *between* `xc_animData` at 0x0C and `x18_ambientColor` at 0x18,
// which is member-declaration order, and a constructor body runs after every mem-init. C++ forbids
// bit-fields in a mem-init list; MWCC 2.7 accepts it, and `CModelDataDefaultCtor.cpp`'s header
// records the same finding for the default constructor.
CModelData::CModelData(const CModelData& other)
    : x0_scale(other.x0_scale),
      xc_animData(other.xc_animData),
      x14_flags(other.x14_flags),
      x18_ambientColor(other.x18_ambientColor),
      x1c_normalModel(other.x1c_normalModel),
      x2c_xrayModel(other.x2c_xrayModel),
      x3c_infraModel(other.x3c_infraModel) {}
