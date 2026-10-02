// CScriptSafeZoneHealth.cpp - `fn_66_7748`, ScriptSafeZone's (module 66) override of CActor's
// `HealthInfo`: .text 0x7748..0x7750, two instructions (`addi r3,r3,0x1E8 / blr`), read off the
// module's own bytes (`build/G2ME01/ScriptSafeZone/asm/MetroidPrime/ScriptObjects/
// CScriptSafeZoneTail2.s`, which dtk writes for the range this unit claims).
//
// **It is a vtable entry, not a free function.** `tools/rel_class_map.py` reads it as word 15 -
// vtable offset 0x3C, the 15th virtual - of `lbl_66_data_B8` (`.data:0xB8`, 0x7C bytes = 31
// words: two zero words, then one per virtual, so 29 virtuals). In that table the slot above it,
// offset 0x38, is `fn_66_0` - CActor's `GetHealthInfo`, decompiled in
// `CScriptSafeZonePrefix.cpp`, which dispatches on offset 0x38 and so calls *this* function. The
// other table, `lbl_66_data_10` (`.data:0x10`, 0xA0 bytes = 40 words), does not define this slot:
// it holds the inherited `HealthInfo__6CActorFv`, whose DOL body at 0x8004C5C0 is `li r3,0 / blr`
// (CActor returns nullptr). So this is the class's real record accessor where the base class has
// none, and the base `6CActor` is what `rel_class_map` fits to these tables (19 inherited slots
// agree); the name and return type are `include/MetroidPrime/CActor.hpp`'s
// `virtual CHealthInfo* HealthInfo()`.
//
// **The layout below is a model, not retail's class, and it does not need to be retail's.** What is
// measured is the offset 0x1E8, and two neighbours confirm the member is a `CHealthInfo`:
// `fn_66_772C` (0x772C, 28 bytes, immediately below this function in `.text`) loads
// `*(float*)(self + 0x1E8)` and stores it at `*(float*)(self + 0x1EC)`, which is `healthA` into
// `healthB` in `CHealthInfo`'s own order (`include/MetroidPrime/CHealthInfo.hpp`), and
// `fn_66_67C4` - the next member accessor along, in `CScriptSafeZoneVulnerability.cpp` - returns
// `self + 0x208`, which is 0x1E8 plus `CHECK_SIZEOF(CHealthInfo, 0x20)`. Only the one member's
// offset is load-bearing, so the unit can be `Matching` without the rest of the class, and no
// behaviour is invented to make it match.
//
// The member's name is upstream's (`CHealthInfo* CActor::HealthInfo()` returns one in the Prime 1
// decomp), but this tree's `CActor` has no such member: it is `CHECK_SIZEOF(CActor, 0x158)` and the
// record is at 0x1E8, so the 0x90 bytes between are this module's own, and no header here names
// them. Correcting `CActor.hpp`'s layout is a port-side model change and belongs to another item.

#include "MetroidPrime/CHealthInfo.hpp"

struct CScriptSafeZoneHealthLayout {
  char mPad[0x1E8];
  // 0x1E8, 0x20 bytes, ending at 0x208 where the next member accessor reads.
  CHealthInfo mHealthInfo;
};

extern "C" {
CHealthInfo* fn_66_7748(CScriptSafeZoneHealthLayout* self) { return &self->mHealthInfo; }
} // extern "C"