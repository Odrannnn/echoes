// Retail .text 0x80321740-0x80321758: 24 bytes, one function.
//
// Retail stores the requested volume and then re-stores the clamped one, with a
// `blelr` for the common case; the source shape that produces it is two stores,
// not a min(). Its sibling `SetMusicVolume` at 0x80321758 is the same body without
// the early return, which is why the two are separate units here rather than one.
//
// The word it writes is retail's .sdata 0x80418C30, declared not defined - see the
// note at the top of Kyoto/Audio/CAudioSysVolume.cpp for why this block claims
// .text only. Nothing else in the DOL reads it; the streamed-audio side of retail
// does, through the base object dtk splits for 0x80320FD8.

#include "Kyoto/Audio/CStreamAudioManager.hpp"

extern "C" {
/// 0x80418C30, .sdata, 4 bytes. The streamed-SFX volume, 0..0x7F.
extern uint lbl_80418C30;
}

void CStreamAudioManager::SetSfxVolume(uint volume) {
  lbl_80418C30 = volume;
  // The `int` cast is load-bearing and is not stylistic. Retail compares with
  // `cmpwi r3,127`; without the cast MWCC narrows the compare to `cmplwi r3,127`,
  // which is the same size and a different instruction. `cmplwi` is chosen
  // whenever the left operand is not already a 32-bit signed value.
  if (static_cast< int >(volume) > 0x7F) {
    lbl_80418C30 = 0x7F;
  }
}
