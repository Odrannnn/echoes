// Retail .text 0x803083A8-0x803083C8: 32 bytes, one function, a pure thunk to the
// SDK's DTK (streamed-audio track) entry point. mwcceppc emits function
// definitions in reverse source order, so a one-function unit is trivially in
// order.
//
// Its neighbour at 0x80308388 is the same shape over `DTKSetRepeatMode`; it is
// left unclaimed rather than folded in, because claiming it would mean owning an
// unnamed retail function for no gain to the port.
//
// `DTKSetSampleRate` is real on the host - platform/sdk_stubs.cpp:54 defines it -
// so this unit is one of the two in the block that *could* also be a port source.
// It is not put in files.cmake because the port calls it before anything has set a
// sample rate and the honest host behaviour is to ignore it, which is what
// src/MetroidPrime/PortAudio.cpp does.

#include "Kyoto/Audio/CAudioSys.hpp"

#include <dolphin/dtk.h>

void CAudioSys::TrkSetSampleRate(ETRKSampleRate rate) {
  DTKSetSampleRate(rate);
}
