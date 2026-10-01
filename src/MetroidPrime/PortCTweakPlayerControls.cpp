// Host definition of `fn_80215860`, the free-function reader that
// `src/MetroidPrime/Player/CMorphBall.cpp`'s `CMorphBall::IsMovementAllowed` (retail
// 0x800CE7D0) calls at 0x800CE7EC.
//
// **Not a configure.py unit.** Retail's copy is at 0x80215860, which is inside
// `build/G2ME01/obj/auto_03_80215424_text.o` - an unclaimed auto-split range that no unit
// in `config/G2ME01/splits.txt` covers, so there is no source file a lane could list it in.
// This is the `Port*.cpp` arrangement the repo already uses for exactly that case:
// `PortCTweakBall.cpp`, `PortTweakGlobals.cpp`, `PortMwccNew.cpp`.
//
// **The two must never be compiled together** - only this one is in `files.cmake`, and no
// build lists both.
//
// The body is retail's own, character for character. `./tools/dis.sh 0x80215860 0xC` is
// three instructions, `lwz r3,0(r3)` / `lbz r3,319(r3)` / `blr`: dereference the tweak
// control's `mData` pointer and read one `bool` out of it. 319 = 0x13F, and
// `include/MetroidPrime/ScriptLoader/SLdrTweakPlayerControls.hpp` puts the `booleans`
// sub-struct at 0x130 (`rstl::string` = 4 bytes, then 75 `int` = 300 bytes), so 0x13F is
// its 16th member, `unknown_0x5282c47e`. It is read-only and has no side effect, so the
// host build needs nothing of `CTweakPlayerControls` beyond its data pointer.

#include "MetroidPrime/ScriptLoader/SLdrTweakPlayerControls.hpp"
#include "MetroidPrime/Tweaks/CTweakPlayerControls.hpp"

extern "C" bool fn_80215860(const CTweakPlayerControls* self) {
  return self->GetData()->booleans.unknown_0x5282c47e;
}
