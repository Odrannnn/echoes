// The host stand-in for Tweaks.rel's `REL_CreateTweakGlobals`. See
// src/MetroidPrime/PortTweakGlobals.cpp for the measurement that makes it
// necessary. Kept in its own header, like port_entry.h, so platform/main.cpp does
// not have to include a game header to call one port-side function.

#pragma once

namespace port {
namespace tweaks {

// Give `gpTweakPlayerA` and `gpTweakPlayerB` (DOL 0x80418F44 and 0x80418F40) real
// 4-byte cells over a zeroed `SLdrTweakPlayer`, so the two
// `CTweakPlayer::Get*AnalogMax` calls `CGameArchitectureSupport`'s constructor
// makes at 0x80007F40 and 0x80007F4C have a receiver. Idempotent: a second call
// with both slots already set does nothing.
//
// There is no failure return. The allocation is `rs_new`, which throws
// `std::bad_alloc` exactly as retail's `operator new` does, so a caller cannot do
// anything useful with a false.
void CreateStandInTweakPlayers();

} // namespace tweaks
} // namespace port
