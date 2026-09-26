// Retail .text 0x80321758-0x80321790: 56 bytes, one function.
//
// Same clamp shape as CStreamAudioManagerSfxVolume.cpp - store, then re-store the
// clamped value - followed by a call that takes the streamed-audio volume *scale*
// rather than the volume. That scale is retail's .sbss 0x80419C18, read here and
// never written in this block.
//
// The callee `fn_803212C8` is unnamed in retail and lives at 0x803212C8, inside the
// CStreamAudioManager region, so this unit cannot be a port source. The port's own
// body is in src/MetroidPrime/PortAudio.cpp.

#include "Kyoto/Audio/CStreamAudioManager.hpp"

extern "C" {
/// 0x80418C28, .sdata, 4 bytes. The streamed-music volume, 0..0x7F.
extern uint lbl_80418C28;
/// 0x80419C18, .sbss, 4 bytes. The streamed-audio volume scale.
extern float lbl_80419C18;
/// 0x803212C8. Unnamed in retail.
void fn_803212C8(float);
}

void CStreamAudioManager::SetMusicVolume(uint volume) {
  lbl_80418C28 = volume;
  // The `int` cast is load-bearing: see the note in
  // Kyoto/Audio/CStreamAudioManagerSfxVolume.cpp. Retail compares with `cmpwi`,
  // and without the cast MWCC emits `cmplwi`.
  if (static_cast< int >(volume) > 0x7F) {
    lbl_80418C28 = 0x7F;
  }
  fn_803212C8(lbl_80419C18);
}
