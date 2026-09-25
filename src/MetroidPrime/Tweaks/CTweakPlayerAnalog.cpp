// CTweakPlayer's two analog-limit accessors.
//
// These are the two calls `CGameArchitectureSupport`'s constructor makes on
// `gpTweakPlayerA` immediately after loading it with no null test:
//
//   80007f38:  lwz    r29,-28220(r13)                ; gpTweakPlayerA
//   80007f3c:  mr     r3,r29
//   80007f40:  bl     802184cc <GetRightAnalogMax__12CTweakPlayerFv>
//   80007f4c:  bl     802184d8 <GetLeftAnalogMax__12CTweakPlayerFv>
//
// and retail defines each of them in **12 bytes**, and the shape is what says the
// receiver is the 4-byte cell and not a CTweakPlayer with members of its own:
//
//   802184cc:  lwz    r3,0(r3)          ; the cell's only word: a SLdrTweakPlayer*
//   802184d0:  lfs    f1,428(r3)        ; 428 = 0x1AC
//   802184d4:  blr
//
// Addresses and sizes from `config/G2ME01/symbols.txt`, confirmed against
// `build/G2ME01/main.elf`:
//
//   GetLeftAnalogMax    0x802184D8  0xC  reads +0x1A8  misc.leftAnalogMax
//   GetRightAnalogMax   0x802184CC  0xC  reads +0x1AC  misc.rightAnalogMax
//
// **The offsets are retail's, and this tree's `SLdrTweakPlayer` already reproduces
// every one of them** - `sizeof(SLdrTweakPlayer)` is 0x37C measured by compiling
// an `offsetof` table with *mwcceppc*, and `misc.leftAnalogMax` lands at +0x1A8.
// That distinction is the whole point: the same table measured with a 64-bit
// `g++` says 0x388 and a different offset, because a `rstl::string` member is
// 8 bytes of pointer on a 64-bit host instead of retail's 0x10. So these bodies
// are written against **named members** and emit retail's three instructions
// exactly; not one raw offset appears here. `docs/research/tweak_player.md` has
// the probe, the numbers and the correction.
//
// Declared descending by retail offset: mwcceppc emits in reverse source order,
// so the source order has to run the other way for the object to match.

#include "MetroidPrime/ScriptLoader/SLdrTweakPlayer.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayer.hpp"

float CTweakPlayer::GetLeftAnalogMax() { return mTweak->misc.leftAnalogMax; }

float CTweakPlayer::GetRightAnalogMax() { return mTweak->misc.rightAnalogMax; }
