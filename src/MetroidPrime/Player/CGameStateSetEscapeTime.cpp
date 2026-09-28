// Retail `SetEscapeTime__10CGameStateFf` = `_ZN10CGameState13SetEscapeTimeEf`,
// .text 0x801424EC..0x801424F4, 0x8 = 8 bytes:
//
//     801424ec  stfs    f1,80(r3)      ; +0x50
//     801424f0  bclr
//
// One store and a return: the argument arrives in `f1` and lands in the float at
// `+0x50`, which `include/MetroidPrime/Player/CGameState.hpp` already names `mEscapeTime`
// and `GetEscapeTime` already reads (it is set to retail's 100.0f at `+0x50` by
// 0x801441D0's `lfs f0,-25096(r2)`). So the accessor pair is retail's own and needs no
// view struct: the header's comment that guessed the name had already measured the
// offset and the store width, and this body is what makes the two agree.
//
// Its own unit: `CGameStateSetIsDarkWorld.cpp` claims 0x801424BC..0x801424CC immediately
// below and a unit may not claim two discontiguous ranges in one section.
#include "MetroidPrime/Player/CGameState.hpp"

void CGameState::SetEscapeTime(float time) { mEscapeTime = time; }
