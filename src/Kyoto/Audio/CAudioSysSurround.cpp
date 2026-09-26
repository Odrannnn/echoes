// Retail .text 0x8030787C-0x803078FC: 128 bytes, two functions.
// `SetSurroundMode` is retail's switch over the three ESurroundModes, and
// `GetSurroundMode` is the four-byte load behind it. Declared descending by retail
// offset; mwcceppc emits function definitions in reverse source order and
// mwldeppc keeps the object's .text order verbatim.
//
// `mSurroundMode` is retail's .sbss 0x80419B7C and is declared, not defined - see
// the long note at the top of Kyoto/Audio/CAudioSysVolume.cpp for why this block
// claims .text only. Retail's own text at 0x8030787C+ reads the same word, and it
// resolves against dtk's base object for .sbss.
//
// The two callees are unnamed in retail and live in the SDK region. They exist
// only inside main.dol, which is why this unit cannot also be a port source: a
// host build has no `fn_80389A58`. `fn_803078FC` is the AI/DSP "speaker mode"
// setter - it forwards to `fn_80389A58` and to the SDK's `OSSetSoundMode`.

#include "Kyoto/Audio/CAudioSys.hpp"

extern "C" {
/// 0x80419B7C, .sbss, 4 bytes. CAudioSys::mSurroundMode.
extern uint lbl_80419B7C;
/// 0x803078FC. Unnamed in retail.
void fn_803078FC(uchar);
/// 0x80389A58. Unnamed in retail; the DSP surround-mode setter.
void fn_80389A58(int);
}

CAudioSys::ESurroundModes CAudioSys::GetSurroundMode() {
  return static_cast< ESurroundModes >(lbl_80419B7C);
}

void CAudioSys::SetSurroundMode(ESurroundModes mode) {
  switch (mode) {
    case kSM_Mono:
      fn_803078FC(0);
      break;
    case kSM_Stereo:
      fn_803078FC(1);
      break;
    case kSM_Surround:
      fn_803078FC(1);
      fn_80389A58(2);
      break;
  }
  lbl_80419B7C = mode;
}
