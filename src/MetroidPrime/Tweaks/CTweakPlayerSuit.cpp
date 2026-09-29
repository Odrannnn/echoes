// CTweakPlayer's three suit-damage-reduction accessors.
//
// The siblings of CTweakPlayerAnalog.cpp, in retail's other contiguous run. Same
// twelve-byte shape, same reason the receiver is a 4-byte cell rather than a
// CTweakPlayer with members:
//
//   80217d30:  lwz    r3,0(r3)          ; the cell's only word
//   80217d34:  lfs    f1,378(r3)        ; 378 = 0x378
//   80217d38:  blr
//
//   GetLightSuitDamageReduction  0x80217D30  0xC  +0x378  suitDamageReduction.light
//   GetDarkSuitDamageReduction   0x80217D3C  0xC  +0x374  suitDamageReduction.dark
//   GetVariaSuitDamageReduction  0x80217D48  0xC  +0x370  suitDamageReduction.varia
//
// `CStateManager.cpp` routes all three suit accessors through
// `CPlayer::GetTweakPlayer()` at 0x8000BF94, whose body is
// `x1320 == 1 ? gpTweakPlayerB : gpTweakPlayerA`; that function is the last
// undefined symbol in this chain and is `CPlayer`'s, not ours.
//
// The offsets are retail's and this tree's 32-bit layout reproduces them, measured
// with mwcceppc and not with a host compiler - see CTweakPlayerAnalog.cpp and
// `docs/research/tweak_player.md`. Named members only; no raw offsets.
//
// Declared descending by retail offset: mwcceppc emits in reverse source order.

#include "MetroidPrime/ScriptLoader/SLdrTweakPlayer.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"

float CTweakPlayer::GetVariaSuitDamageReduction() {
  return mData->suitDamageReduction.varia;
}

float CTweakPlayer::GetDarkSuitDamageReduction() {
  return mData->suitDamageReduction.dark;
}

float CTweakPlayer::GetLightSuitDamageReduction() {
  return mData->suitDamageReduction.light;
}
