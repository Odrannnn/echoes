// Retail `SetIsDarkWorld__10CGameStateFb` = `_ZN10CGameState14SetIsDarkWorldEb`,
// .text 0x801424BC..0x801424CC, 0x10 = 16 bytes:
//
//     801424bc  lbz      r0,748(r3)      ; 0x2EC
//     801424c0  rlwimi   r0,r4,5,26,26
//     801424c4  stb      r0,748(r3)
//     801424c8  blr
//
// `rlwimi r0,r4,5,26,26` is mwcceppc's whole-byte read/merge/write for **one one-bit field of a
// `u8`**, and this encoding is the third of the three the save-game reader writes at 0x80144348,
// 0x8014436C and 0x80144390 (`rlwimi r0,rX,7,24,24` / `,6,25,25` / `,5,26,26`). So
// `SetIsDarkWorld` writes the flag the reader's **third** `ReadBits(1)` fills - the same byte, the
// same bit - and `include/MetroidPrime/Player/CGameState.hpp` now says so.
//
// **The bit index is measured.** Declaring the struct with this field *first* compiles to
// `rlwimi r0,r4,7,24,24` and scores 96.25%; mwcceppc allocates `bool : 1` in declaration order
// from `,7,24,24` upward, so retail's `,5,26,26` is the third field. That is why the header's
// `SGameStateFlags` has three bits and why `b5` is the one this writes - the first two are the
// reader's first two `ReadBits(1)` results, and `CGameStateCtor.cpp` (which has carried its own
// layout-identical local copy of the struct since before this header had one) sets `b6`.
//
// The header's own comment used to say splitting `u8 x2ec_flags` into named bits "is a change
// with no measured effect until `CGameStateStreamCtor.cpp` reaches 0x80144300". It has not
// reached it - that unit is `NonMatching` at 24.33% - but this one does, so the change is made
// and the comment above the member is the correction.
//
// Its own unit: `CGameStateGetHardModeDamageMultiplier.cpp` claims 0x80142498..0x801424BC
// immediately below, and a unit may not claim two discontiguous ranges in one section.
#include "MetroidPrime/Player/CGameState.hpp"

void CGameState::SetIsDarkWorld(bool darkWorld) { x2ec_flags.b5 = darkWorld; }
