// Retail `SetAlwaysCalculateRadius__13CSimpleShadowFb` =
// `_ZN13CSimpleShadow21SetAlwaysCalculateRadiusEb`, .text 0x800DF458..0x800DF468, 0x10 = 16 bytes:
//
//     800df458  lbz     r0,72(r3)        ; 0x48
//     800df45c  rlwimi  r0,r4,6,25,25
//     800df460  stb     r0,72(r3)
//     800df464  blr
//
// The `rlwimi r0,rX,6,25,25` encoding is mwcceppc's **second** `bool : 1` in declaration order -
// the sequence measured on `CGameState::SetIsDarkWorld` is `,7,24,24` for the first field,
// `,6,25,25` for the second, `,5,26,26` for the third, and so on down - and
// `include/MetroidPrime/CSimpleShadow.hpp` already names the second field
// `x48_25_alwaysCalculateRadius`. The header's names and retail's encodings agree, which is the
// check that the two are the same field rather than a coincidence of the numbering.
//
// Its own unit: `CSimpleShadowGetBounds.cpp` claims 0x800DF3DC..0x800DF458 immediately below, and
// a unit may not claim two discontiguous ranges in one section. The 4-byte `GetTransform` at
// 0x800DF478 is above `fn_800DF468` and `fn_800DF470`, so that one is a third file.
#include "MetroidPrime/CSimpleShadow.hpp"

void CSimpleShadow::SetAlwaysCalculateRadius(bool always) {
  x48_25_alwaysCalculateRadius = always;
}
