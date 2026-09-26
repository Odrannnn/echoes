// Retail .text 0x8030780C-0x80307864: 88 bytes, two functions. Declared descending
// by retail offset; mwcceppc emits function definitions in reverse source order
// and mwldeppc keeps the object's .text order verbatim.
//
// `EnableAICallback` is retail's "the streamed-audio mixer wants the AI DMA
// callback" flag. It is idempotent, it remembers the callback it displaced and
// hands it back when the flag is cleared, and the two branches differ only in
// which side of the call the old value is stored on.
//
// This is the one unit in the block that *could* also be a port source - its only
// callee, `AIRegisterDMACallback`, is real: platform/ai_dma.cpp:149 implements it
// for the host. It is not put in files.cmake anyway, because the two words it
// reads and writes are retail guest addresses (lbl_80418BEE, lbl_80419B84) that
// only exist inside the DOL; the port's own body is in src/MetroidPrime/PortAudio.cpp.

#include "Kyoto/Audio/CAudioSys.hpp"

#include <dolphin/ai.h>

extern "C" {
/// 0x80418BEE, .sdata, 1 byte. Whether the AI DMA callback is ours.
extern bool lbl_80418BEE;
/// 0x80419B84, .sbss, 4 bytes. The AI DMA callback we displaced.
extern AIDCallback lbl_80419B84;
}

void CAudioSys::EnableAICallback(bool enable) {
  if (lbl_80418BEE == enable) {
    return;
  }
  lbl_80418BEE = enable;
  if (enable) {
    AIRegisterDMACallback(lbl_80419B84);
  } else {
    lbl_80419B84 = AIRegisterDMACallback(nullptr);
  }
}

bool CAudioSys::IsAICallbackEnabled() {
  // `!= 0` here is one instruction too many: MWCC normalises the result of a
  // relational test with neg/or, while the implicit uchar-to-bool conversion
  // does not. Retail's body is `lbz; blr`.
  return lbl_80418BEE;
}
