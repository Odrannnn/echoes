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
//
// `CSfxManager::TranslateSFXID` is a fifteenth, added at the bottom of this file under
// its own heading. It is not one of the fourteen above and is not counted in them: it is
// the only body here that *reads* a table the port cannot build, and its note says which
// table that is and what the function answers without it.

#include "Kyoto/Audio/CAudioSys.hpp"
#include "Kyoto/Audio/CSfxManager.hpp"
#include "Kyoto/Audio/CSfxManagerPort.hpp"
#include "Kyoto/Audio/CStreamAudioManager.hpp"

#include <dolphin/ai.h>
#include <dolphin/dtk.h>
#include <dolphin/os.h>

// The class statics CAudioSys.hpp declares but nothing in the tree defines. On retail
// these are guest addresses in .sdata/.sbss; here they are ordinary host objects with
// the same meaning. The four container pointers are retail's too - .bss
// 0x80419B6C..0x80419B7C in the CAudioSys region - and retail's constructor allocates
// three objects of 20, 16 and 144 bytes. Which three of the four is not determined from
// the disassembly, and mpGroupSetDB cannot be allocated here at all (see the
// constructor), so the port allocates the other three and says so rather than guessing.
CAudioSys::ESurroundModes CAudioSys::mSurroundMode = CAudioSys::kSM_Mono;
bool CAudioSys::mInitialized = false;
rstl::map< rstl::string, rstl::ncrc_ptr< CAudioGroupSet > >* CAudioSys::mpGroupSetDB = nullptr;
rstl::map< uint, rstl::string >* CAudioSys::mpGroupSetResNameDB = nullptr;
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
uint sStreamSfxVolume = 0;
uint sStreamMusicVolume = 0;
bool sAICallbackEnabled = false;
AIDCallback sPrevAICallback = nullptr;
} // namespace

CAudioSys::CAudioSys(char, char, char, char, uint) {
  // Retail's 0x80308A28. AIInit is real on the host (platform/ai_dma.cpp) and brings
  // up the SDL stream the AI DMA callback is fed from, so this is the point at which
  // the port starts making sound - the same point as retail.
  AIInit(nullptr);
  DTKInit();

  mpGroupSetResNameDB = new rstl::map< uint, rstl::string >();
  mpDVDTrackDB = new rstl::map< rstl::string, rstl::ncrc_ptr< CTrkData > >();
  mpEmitterDB = new rstl::vector< CEmitterData >();
  // mpGroupSetDB is left null on purpose. Its value type is
  // rstl::ncrc_ptr<CAudioGroupSet>, and CAudioGroupSet is a forward declaration
  // with no definition anywhere in this tree, so instantiating the map - which
  // `new` and `delete` both do - instantiates a destructor that deletes through
  // an incomplete type (gcc: "invalid use of incomplete type 'class
  // CAudioGroupSet'"). Retail's constructor allocates three objects as well, at
  // 20, 16 and 144 bytes; which of them this one is not determined. Whoever
  // writes the audio-group loader has to define CAudioGroupSet first.

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
  delete mpGroupSetResNameDB;
  mpGroupSetResNameDB = nullptr;
  mInitialized = false;
}

void CAudioSys::SysSetVolume(uchar channel, uint volume, uchar) {
  // Retail 0x80308870 forwards to the AUDIO thunk at 0x80389964.
  sMasterChannel = channel;
  sMasterVolume = volume > 0xFF ? 0xFF : static_cast< uchar >(volume);
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
// Retail keeps both volumes in .sdata words (0x80418C30 and 0x80418C28) and, in
// SetMusicVolume only, calls the unnamed 0x803212C8 with the streamed-audio volume
// *scale* from .sbss 0x80419C18. That callee is what actually re-weights the music
// stream, and it belongs to retail's streaming path, which the port does not have
// yet: the two bodies here keep the clamp - which is the part that is observable and
// the part CGameOptions drives - and leave the scale application to whoever writes
// the streaming side.

void CStreamAudioManager::SetSfxVolume(uint volume) {
  sStreamSfxVolume = volume > 0x7F ? 0x7F : volume;
}

void CStreamAudioManager::SetMusicVolume(uint volume) {
  sStreamMusicVolume = volume > 0x7F ? 0x7F : volume;
}

// --- CSfxManager::TranslateSFXID ---------------------------------------------
//
// Retail `fn_8029C79C`, 0x4C bytes - the 0x4C-sized function immediately in front of
// `fn_8029C7E8`, which is how the address was found (`./tools/dis.sh 0x8029C79C 0x4C`;
// Metroid Prime's `CSfxManager::TranslateSFXID` is the same 0x4C, and MP1's
// `../MetroidPrimePort/src/Kyoto/Audio/CSfxManager.cpp:716` is statement for statement the
// body below).
//
// The game numbers its sounds per area and the mixer needs the runtime id, so this is the
// lookup between the two. The port's one caller is `CActor::ProcessSoundEvent`
// (`src/MetroidPrime/CActor.cpp:774`, and `build-port-link/link_undefined.txt` named
// `CActor.cpp.o` as the sole referrer before this body existed), and it stores the result in
// `CAudioSys::C3DEmitterParmData::x24_sfxId`. **What the stub this replaces put in that slot was
// 0, and 0 is a valid runtime sound id** - the last paragraph of this block says why that was
// the dangerous answer.
//
// ## The table, and why it is null here
//
// `rstl::vector< short >*`, retail's `.sbss` `lbl_80419884` (`python3 tools/sda.py -25852`
// -> `0x80419884 lbl_80419884 (in .sbss, +0x0)`), reached through
// `include/Kyoto/Audio/CSfxManagerPort.hpp` so that the loader in
// `src/Kyoto/CSimplePoolPort.cpp` can maintain it and this reads it. Retail reads it as
// `count = *(int*)(p + 4)` (`fn_8029C79C+0x0C`) and `items = *(short**)(p + 12)`
// (`+0x28`), and those are **this tree's `rstl::vector` fields** (`x4_count` at +4, `xc_items`
// at +12, `include/rstl/vector.hpp:18-21`), so the declaration is the real one and not a
// shape-compatible guess. `fn_8029C7E8` drops the vector before each load (`fn_80255C00` with
// `r4 = 1`, then `stw r0,-25852(r13)`), and `port::sfx::ClearTranslationTable()` is that
// statement.
//
// **The vector is not built, and nothing here stands in for it.** Its bytes are the
// `sound_lookup_ATBL` resource in `Strings.pak`, and `Strings.pak` is not on this disc -
// `docs/HANDOFF.md` records the measurement (20 `.pak`s on the ISO, none named that, so
// retail's own `CDvdFile::FileExists` probe at 0x800071A8 fails too). The pool holds a token
// over a null object (`src/Kyoto/CSimplePoolPort.cpp`), and the `ATBL` factory
// `fn_8029AB80` - 0x68 bytes, `operator new(0x10)` then a `rstl::vector< short >` from the
// stream - is a `return CFactoryFnReturn()` in `src/Kyoto/CFactoryFunctionsPort.cpp` for want
// of the stream. Writing any mapping here would be fabricating the game's sound table.
//
// ## What the port therefore answers, and why that is the right answer
//
// `kInternalInvalidSfxId`, 0xFFFF - which is **retail's own answer when the table is missing**:
// the first statement of the body below, and not a substitute for the other two. What changed is
// only the failure mode: the reach stub this replaces returned 0, and 0 is a *valid* sound id, so
// every untranslatable sound in the game would have been a real-looking wrong sound. The
// correct id and no sound is the honest one, and it is the same answer retail gives on a disc
// where `LoadTranslationTable` was never reached.
//
// The value is 0xFFFF as `include/Kyoto/Audio/CSfxManager.hpp` says, and
// `src/MetroidPrime/PortGlobals.cpp`'s comment on `kMedPriority` derives it out of the DOL
// (`.sdata2` 0x8041E2E6 = `kInternalInvalidSfxId`). It is defined here rather than there
// because that file's block is a counted list ("twelve class statics") whose heading figure is
// not this item's to move, and because nothing else in the tree needs the value.
namespace {
rstl::vector< short >* s_translationTable = nullptr; // retail's `lbl_80419884`
} // namespace

namespace port {
namespace sfx {

void ClearTranslationTable() {
  delete s_translationTable;
  s_translationTable = nullptr;
}

} // namespace sfx
} // namespace port

const ushort CSfxManager::kInternalInvalidSfxId = 0xFFFF;

ushort CSfxManager::TranslateSFXID(ushort id) {
  if (s_translationTable == nullptr || id >= s_translationTable->size()) {
    return kInternalInvalidSfxId;
  }
  const short ret = (*s_translationTable)[id];
  if (ret < 0) {
    return kInternalInvalidSfxId;
  }
  return static_cast< ushort >(ret);
}
