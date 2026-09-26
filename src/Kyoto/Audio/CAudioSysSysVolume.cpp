// Retail .text 0x80308840-0x8030889C: 92 bytes, two functions, both pure thunks.
// Declared descending by retail offset; mwcceppc emits function definitions in
// reverse source order and mwldeppc keeps the object's .text order verbatim.
//
// The four `clrlwi` before each call are the proof of the callee's prototype: MWCC
// masks an argument to the width the *callee* declares, so `fn_803899C4` takes
// (uchar, ushort, uchar, uchar) and `fn_80389964` takes (uchar, ushort, uchar).
// Note that retail's public `SysSetVolume` takes an `unsigned int` and its second
// argument is still masked to 16 bits, so the narrowing happens at the call.
//
// Both callees are unnamed in retail and live in the SDK region at 0x803899xx, so
// this unit cannot be a port source. The port's own bodies are in
// src/MetroidPrime/PortAudio.cpp.

#include "Kyoto/Audio/CAudioSys.hpp"

extern "C" {
/// 0x803899C4. Unnamed in retail. The parameters are declared wide on purpose: the
/// four `clrlwi` in retail's body are MWCC masking each narrow argument to the width
/// it is then passed on as, and it only emits them for a conversion it can see in
/// the source, not for the callee's declared type.
void fn_803899C4(uint, uint, uint, uint);
/// 0x80389964. Unnamed in retail. Same, three parameters.
void fn_80389964(uint, uint, uint);
}

void CAudioSys::SysSetVolume(uchar channel, uint volume, uchar reverb) {
  fn_80389964(static_cast< uchar >(channel), static_cast< ushort >(volume),
              static_cast< uchar >(reverb));
}

void CAudioSys::SysSetSfxVolume(uchar volume, ushort spatializer, uchar a, uchar b) {
  fn_803899C4(static_cast< uchar >(volume), static_cast< ushort >(spatializer),
              static_cast< uchar >(a), static_cast< uchar >(b));
}
