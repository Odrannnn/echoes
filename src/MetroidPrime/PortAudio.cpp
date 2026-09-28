// The port's own CAudioSys / CStreamAudioManager bodies.
//
// This file is in files.cmake and is **not** in configure.py, so mwcceppc never
// compiles it and it cannot affect main.dol or any of the 86 REL modules. It is the
// same arrangement as src/MetroidPrime/PortGlobals.cpp and
// src/MetroidPrime/PortBoot.cpp, and for the same reason: a `CAudioSys` that talks to
// Aurora's AI/DSP/OS is port work by nature, not decompilation.
//
// Why these symbols need a host body at all, given that twelve of them now have
// `Matching` units in src/Kyoto/Audio/. Three reasons, and they are different:
//
//   * Seven of the twelve cannot be a port source at all. Their retail bodies call
//     *retail-local* wrappers that exist only inside main.dol - `fn_80389964` and
//     `fn_803899C4` (the AUDIO_* thunks), `fn_803078FC`/`fn_80389A58` (the
//     speaker-mode setters) and `fn_803212C8` (the streamed-audio volume apply) - and
//     one more writes the guest word 0x80418C30. A host build has none of those.
//   * The other five could be: the three volume-scale accessors, `TrkSetSampleRate`
//     and the two AI-callback methods have no retail-local callee, and listing their
//     units in files.cmake would have worked. It is not done, deliberately. Those
//     units read and write retail *addresses* - .sdata 0x80418BEA/0x80418BEC/0x80418BEE
//     and .sbss 0x80419B84 - so host-compiling them would put undefined guest symbols
//     into the port's link in exchange for removing four that this file removes
//     anyway. One file, one alphabet, no guest addresses in a host binary.
//   * `CAudioSys`'s constructor and destructor are 512 and 148 bytes of retail and are
//     not decompilation candidates at all: they are almost entirely calls into unnamed
//     DSP/AUDIO configuration code that has no host analogue.
//
// What the bodies do, symbol by symbol, and what retail's does:
//
//   ctor            retail: AIInit, an unnamed AUDIO configuration call, three
//                   per-channel calls, two reverb setup calls, DTKInit, three
//                   container allocations, mSurroundMode from OSGetSoundMode. Here:
//                   AIInit and DTKInit (both real on the host), three container
//                   allocations, and mSurroundMode from OSGetSoundMode. The unnamed
//                   DSP configuration is dropped, and there is nothing on a PC that
//                   stands in for it.
//   dtor            retail: two unnamed shutdown calls, AUDIO stop, three container
//                   frees, mInitialized = 0, then the deleting-destructor tail.
//                   Here: the container frees and mInitialized = 0. `Free(void*)` is
//                   retail's CMemory free and is not the host's; `delete` is.
//   SysSetVolume /  retail forwards to the AUDIO thunks. Here the value is recorded in
//   SysSetSfxVolume  a port-side word, because on a PC the mixer is the host's and the
//                   game's only remaining job is to remember what was asked for.
//   SetSurroundMode retail stores the mode and calls OSSetSoundMode through
//                   fn_803078FC. Here: the same store and the same OSSetSoundMode
//                   call, which the port implements in platform/sdk_stubs.cpp.
//   GetSurroundMode a four-byte load. Identical.
//   SetVolumeScale / retail stores to .sdata 0x80418BEA / 0x80418BEC. Those are guest
//   SetDefaultVolumeScale / addresses; the host keeps its own two shorts.
//   GetDefaultVolumeScale
//   TrkSetSampleRate retail forwards to the SDK's DTKSetSampleRate, which the port
//                   defines in platform/sdk_stubs.cpp. Here: the same call.
//   IsAICallbackEnabled / retail flips a flag and hands the displaced AI DMA callback
//   EnableAICallback    back. Here: the same, against platform/ai_dma.cpp's real
//                   AIRegisterDMACallback.
//
// All fourteen are referenced by objects that are reachable from the program's roots,
// so a blind stub here would be a crash rather than a no-op: see
// docs/research/port_link_stubs.md. **Eleven of them are called before the game's
// first frame**, from `CGameArchitectureSupport`'s constructor
// (docs/research/boot_path.md step 17) and `CGameOptions::EnsureOptions` (step 19).
// `~CAudioSys` is on the teardown path rather than the frame path (step 22, and the
// port's `RsMain` returns immediately), and the other three are called only from
// CStaticAudioPlayer, which is streamed audio.

#include "Kyoto/Audio/CAudioSys.hpp"
#include "Kyoto/Audio/CStreamAudioManager.hpp"

#include <dolphin/ai.h>
#include <dolphin/dtk.h>
#include <dolphin/os.h>

// The class statics CAudioSys.hpp declares that the port build defines. Upstream's
// `Kyoto/Audio/DolphinCAudioSys.cpp` defines all of them, but it drives MusyX directly and
// is not in files.cmake; on the host these are ordinary objects with retail's meaning.
CAudioSys::ESurroundModes CAudioSys::mSurroundMode = CAudioSys::kSM_Mono;
bool CAudioSys::mInitialized = false;
rstl::map< rstl::string, rstl::ncrc_ptr< CAudioSys::CTrkData > >* CAudioSys::mpDVDTrackDB =
    nullptr;
rstl::vector< CAudioSys::CEmitterData >* CAudioSys::mpEmitterDB = nullptr;

namespace {
// What the game last asked the mixer for. Nothing reads these yet - the port's audio
// output is the host's, and the game's own mixer (CSfxManager, CDSPStreamManager) is
// not written. They exist so that a future streamed-audio implementation has one
// documented place to read the volume from, rather than each caller keeping its own.
ushort sVolumeScale = 0;
ushort sDefaultVolumeScale = 0;
uchar sMasterVolume = 0x7F;
uchar sSfxVolume = 0x7F;
uchar sMasterChannel = 0;
bool sAICallbackEnabled = false;
AIDCallback sPrevAICallback = nullptr;
} // namespace

CAudioSys::CAudioSys(uchar, uchar, uchar, uchar, uint) {
  // Retail's 0x80308A28. AIInit is real on the host (platform/ai_dma.cpp) and brings
  // up the SDL stream the AI DMA callback is fed from, so this is the point at which
  // the port starts making sound - the same point as retail.
  AIInit(nullptr);
  DTKInit();

  mpDVDTrackDB = new rstl::map< rstl::string, rstl::ncrc_ptr< CTrkData > >();
  mpEmitterDB = new rstl::vector< CEmitterData >();
  // Upstream's header has no group-set databases (retail's constructor allocates three
  // objects, 20, 16 and 144 bytes); the audio-group loader that needs them is unwritten.

  mSurroundMode = OSGetSoundMode() == 0 ? kSM_Mono : kSM_Surround;
  mInitialized = true;
}

CAudioSys::~CAudioSys() {
  // Retail's 0x8030889C. The three container frees and mInitialized = 0 are retail's;
  // the two unnamed shutdown calls and the AUDIO stop at the head of retail's body
  // have no host counterpart, because on a PC the mixer and the AI DMA stream are the
  // host's and are shut down by AIPortShutdown().
  delete mpDVDTrackDB;
  mpDVDTrackDB = nullptr;
  delete mpEmitterDB;
  mpEmitterDB = nullptr;
  mInitialized = false;
}

void CAudioSys::SysSetVolume(uchar volume, uint, uchar group) {
  // Retail 0x80308870 forwards to the AUDIO thunk at 0x80389964 (upstream: `sndVolume`).
  sMasterChannel = group;
  sMasterVolume = volume;
}

void CAudioSys::SysSetSfxVolume(uchar volume, ushort, uchar, uchar) {
  // Retail 0x80308840 forwards to the AUDIO thunk at 0x803899C4.
  sSfxVolume = volume;
}

void CAudioSys::SetSurroundMode(ESurroundModes mode) {
  // Retail 0x8030787C. fn_803078FC is the AI/DSP speaker-mode setter: it forwards to
  // fn_80389A58 and to the SDK's OSSetSoundMode, and the port implements OSSetSoundMode
  // in platform/sdk_stubs.cpp. The DSP half is dropped.
  switch (mode) {
    case kSM_Mono:
      OSSetSoundMode(0);
      break;
    case kSM_Stereo:
      OSSetSoundMode(1);
      break;
    case kSM_Surround:
      OSSetSoundMode(1);
      OSSetSoundMode(2);
      break;
  }
  mSurroundMode = mode;
}

CAudioSys::ESurroundModes CAudioSys::GetSurroundMode() {
  return mSurroundMode;
}

void CAudioSys::SetDefaultVolumeScale(short scale) {
  sDefaultVolumeScale = static_cast< ushort >(scale);
}

void CAudioSys::SetVolumeScale(short scale) {
  sVolumeScale = static_cast< ushort >(scale);
}

short CAudioSys::GetDefaultVolumeScale() {
  return static_cast< short >(sDefaultVolumeScale);
}

void CAudioSys::TrkSetSampleRate(ETRKSampleRate rate) {
  DTKSetSampleRate(rate);
}

bool CAudioSys::IsAICallbackEnabled() {
  return sAICallbackEnabled;
}

void CAudioSys::EnableAICallback(bool enable) {
  if (sAICallbackEnabled == enable) {
    return;
  }
  sAICallbackEnabled = enable;
  if (enable) {
    AIRegisterDMACallback(sPrevAICallback);
  } else {
    sPrevAICallback = AIRegisterDMACallback(nullptr);
  }
}

// --- CStreamAudioManager -----------------------------------------------------
//
// `SetSfxVolume` / `SetMusicVolume` were defined here until the 2026-09-28 upstream merge;
// upstream's `src/Kyoto/Audio/CStreamAudioManager.cpp` now defines both (clamp plus
// `InternalSetVolume`), so the port's clamp-only copies were deleted.
