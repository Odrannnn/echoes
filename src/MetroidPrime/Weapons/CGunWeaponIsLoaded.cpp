// Retail `IsLoaded__10CGunWeaponCFv` = `_ZNK10CGunWeapon8IsLoadedEv`,
// .text 0x801D9B24..0x801D9B30, 0xC = 12 bytes:
//
//     801d9b24  lbz     r0,624(r3)      ; 0x270
//     801d9b28  rlwinm  r3,r0,27,31,31
//     801d9b2c  blr
//
// `rlwinm r3,r0,27,31,31` is a `bool : 1` read out of a `u8`-sized bitfield group, and the
// encoding counts the field's position **down from 8**: field n is read
// `rlwinm r3,r0,25+n,31,31`, so 27 is `25+2` and this is the *third* `bool : 1` declared in
// the group at +0x270. The header's third is `x270_26` (the group is
// `x270_24, x270_25, x270_26, x270_27, x270_28, x270_29, x270_30_subtypeBasePose, x270_31`),
// so **`IsLoaded()` returns `x270_26`** - the same rule measured on
// `CSimpleShadow::Valid` (`,25,31,31` = the first field) and
// `CGameState::SetIsDarkWorld` (`,5,26,26` = the third). Moving the field to the front of the
// declaration scores 96.25% instead.
//
// The gun is "loaded" when its beam-type flags are settled, and `x270_26` sits directly
// below `x244_beamId` and `x248_frozenEffect`, which is where the load state lives.
//
// Its own unit: it is 0x224 bytes into dtk's `auto_03_801D5C70_text`, which is `CActor` and
// `CGunWeapon` territory with no claimed ranges nearby.
#include "MetroidPrime/Weapons/CGunWeapon.hpp"

bool CGunWeapon::IsLoaded() const { return x270_26; }
