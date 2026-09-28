// Four `CAudioSys` streamed-audio thunks, each a single tail call into the SDK's DTK (Digital
// Track) entry points.  Retail addresses and sizes, read off `build/G2ME01/main.elf`:
//
//   CAudioSys::TrkSetVolume(uchar,uchar) TrkSetVolume__9CAudioSysFUcUc   0x80308320  0x28
//   CAudioSys::TrkSetState(ETRKPlayState) TrkSetState__9CAudioSysF13ETRKPlayState
//                                                                0x80308368  0x20
//   CAudioSys::TrkSetRepeatMode(ETRKRepeatMode)
//                               TrkSetRepeatMode__9CAudioSysF14ETRKRepeatMode
//                                                                0x80308388  0x20
//   CAudioSys::TrkNextTrack()         TrkNextTrack__9CAudioSysFv         0x80308300  0x20
//
// Each body is the same five-instruction shape - `stwu`/`mflr`/`bl`/`mtlr`/`blr` - with no
// register shuffling, so the C++ below is the whole of retail's function and the argument
// registers are already the SDK's:
//
//   80308334  bl 8035ef18 <DTKSetVolume>
//   80308374  bl 8035ec68 <DTKSetState>
//   80308394  bl 8035ec60 <DTKSetRepeatMode>
//   8030830c  bl 8035ee58 <DTKNextTrack>
//
// `TrkSetVolume` is the one that is not a bare pass-through, and the two extra instructions are
// the whole of it:
//
//   80308328  clrlwi r3,r3,24
//   8030832c  clrlwi r4,r4,24
//
// i.e. retail narrows both arguments to 8 bits before the call, because the SDK prototype takes
// `int` and the C++ one takes `uchar`. The `uchar` parameters already carry that narrowing, so
// writing `DTKSetVolume(left, right)` reproduces the store width the call makes without
// restating it.
//
// ## Why these four and not the rest of the SDK wrappers
//
// `CAudioSys` has eleven other undefined thunks in this block and they are all the same shape -
// `SfxCheck`/`SfxStop`/`SfxCtrl`/`SfxPan`/`SfxSpan`/`SfxVolume`/`SfxSetFilter` over
// `sndFXCheck`/`sndFXKeyOff`/`sndFXCtrl`/`sndFXSetFilter`, and `SfxStart`/`S3d*`/`TrkQueueTrack` -
// but the **`sndFX*` half of the SND layer is not in the port build at all**: `nm` over every
// object `link_check.sh` links finds `DTK*` (platform/sdk_stubs.cpp) and no `sndFX*`. Writing a
// thunk over a function that does not exist would move the hole rather than close it, and
// writing the SND layer itself is a different piece of work. These four are the closed set.
//
// `CAudioSysTrkSetSampleRate.cpp` is the same argument for the same reason and is not listed
// either; see that file's header. `DTK*` on the host is `platform/sdk_stubs.cpp`'s no-ops, so
// what these four buy the port is the *call* reaching the audio layer rather than the link
// failing on it - which is the honest host behaviour, not a claim that music plays.
//
// The port has a live `CAudioSys` caller: `src/Kyoto/Audio/CStreamAudioManager.cpp` and
// `src/MetroidPrime/PortAudio.cpp` both reach this block.
#include "Kyoto/Audio/CAudioSys.hpp"

#include <dolphin/dtk.h>

void CAudioSys::TrkSetVolume(uchar left, uchar right) { DTKSetVolume(left, right); }

void CAudioSys::TrkSetState(ETRKPlayState state) { DTKSetState(state); }

void CAudioSys::TrkSetRepeatMode(ETRKRepeatMode mode) { DTKSetRepeatMode(mode); }

void CAudioSys::TrkNextTrack() { DTKNextTrack(); }
