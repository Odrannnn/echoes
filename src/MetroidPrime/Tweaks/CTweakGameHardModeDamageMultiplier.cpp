// Retail `fn_80216D38` (config/G2ME01/symbols.txt 19086 region; the unit that owns it is
// `MetroidPrime/mainMid.cpp`'s neighbourhood), 0x80216D38..0x80216D44, 0xC = 12 bytes:
//
//     80216d38  lwz     r3,0(r3)        ; the Tweaks block behind the CTweakGame*
//     80216d3c  lfs     f1,84(r3)       ; +0x54
//     80216d40  blr
//
// A load, a load, a return: the hard-mode damage multiplier is a plain float at `+0x54` of the
// Tweaks block. `src/MetroidPrime/Player/CGameStateGetHardModeDamageMultiplier.cpp` reaches this
// through its own `extern "C" float fn_80216D38(CTweakGame*)` declaration and says, correctly,
// that defining it "here would need the Tweaks block layout, which is not what this unit is for".
// This is the unit for it, and the layout it needs is one word.
//
// **Why the offset and not a named field.** The `Tweaks` block's members are not declared
// anywhere in this tree - `src/MetroidPrime/Tweaks/Tweaks.cpp` is in the port build and
// `gpTweakGame` resolves, but the struct is opaque - so there is no `mHardModeDamageMultiplier`
// to write. Reading `+0x54` is what retail's own instruction does, and the three instructions
// above are the whole function, so the definition reproduces the load exactly rather than
// guessing what the word is called.
//
// `+0x54` sits in the middle of a run of identical one-instruction readers in this block -
// `fn_80216D44` reads `+0x34`, `fn_80216D50` reads `+0x30`, and so on - each returning a different
// tweak's float to a different caller. That is the evidence that this block is a row of tweak
// floats and that `+0x54` is one of them, not a pointer.
#include "MetroidPrime/Tweaks/CTweakGame.hpp"

extern "C" float fn_80216D38(CTweakGame* tweakGame) {
  return *reinterpret_cast< const float* >(
      reinterpret_cast< const char* >(reinterpret_cast< const void* const* >(tweakGame)) + 0x54);
}
