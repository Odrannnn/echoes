// Retail `fn_80232334`, .text 0x80232334..0x8023233C, 8 bytes:
//
//     80232334  stw  r3,-26448(r13)
//     80232338  blr
//
// `-26448(r13)` off `_SDA_BASE_` = 0x8041FD80 is 0x80419630, which `tools/sda.py` names
// `gLoader_SkyRipple` - the `.sbss` slot that `MetroidPrime/ScriptLoader/SkyRipple.cpp` owns
// (`.sbss 0x80419630..0x80419638`) and whose `LoadSkyRipple` thunk dereferences. So this is the
// module's loader setter: `gLoader_SkyRipple.value = loader`, and `value` is at +0 of the 8-byte
// slot, which is why the store has no displacement.
//
// **It has to be its own unit, and that is the fix for the note in `SkyRipple.cpp`.** That file
// says the 8-byte setter "is deliberately NOT claimed: REL modules import it by its retail name,
// so it cannot be renamed and must stay in dtk's auto unit". The name really is fixed - the
// `Coin`, `RsfAudio`, `FlyerSwarm` and `SkyRipple` modules all import theirs - but the constraint
// is on the *name*, not on the file. `SkyRipple.cpp` cannot claim 0x80232334..0x8023233C because it already
// claims 0x80232308..0x80232334 immediately below it, and a unit may not claim two discontiguous ranges
// in one section (`dtk dol split`: "Cyclic dependency encountered while resolving link order").
// A unit that emits nothing but `fn_80232334` can, and it keeps the retail name, so every REL import
// still resolves. The family is 0x8021FA80, 0x80227B2C, 0x80229FBC and 0x80232334, and
// `docs/research/rel_loaders.md` has the rest of it.
#include "MetroidPrime/ScriptLoader.hpp"

// The same 8-byte slot `SkyRipple.cpp` defines. The struct is repeated rather than shared
// because it is a local definition in that file, and moving it into a header would edit a
// `Matching` unit.
struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

extern SLoaderSlot gLoader_SkyRipple;

extern "C" void fn_80232334(FScriptLoader* loader) { gLoader_SkyRipple.value = loader; }
