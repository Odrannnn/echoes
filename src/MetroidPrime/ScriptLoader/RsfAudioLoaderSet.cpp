// Retail `fn_80227B2C`, .text 0x80227B2C..0x80227B34, 8 bytes:
//
//     80227b2c  stw  r3,-26616(r13)
//     80227b30  blr
//
// `-26616(r13)` off `_SDA_BASE_` = 0x8041FD80 is 0x80419588, which `tools/sda.py` names
// `gLoader_RsfAudio` - the `.sbss` slot that `MetroidPrime/ScriptLoader/RsfAudio.cpp` owns
// (`.sbss 0x80419588..0x80419590`) and whose `LoadRsfAudio` thunk dereferences. So this is the
// module's loader setter: `gLoader_RsfAudio.value = loader`, and `value` is at +0 of the 8-byte
// slot, which is why the store has no displacement.
//
// **It has to be its own unit, and that is the fix for the note in `RsfAudio.cpp`.** That file
// says the 8-byte setter "is deliberately NOT claimed: REL modules import it by its retail name,
// so it cannot be renamed and must stay in dtk's auto unit". The name really is fixed - the
// `Coin`, `RsfAudio`, `FlyerSwarm` and `SkyRipple` modules all import theirs - but the constraint
// is on the *name*, not on the file. `RsfAudio.cpp` cannot claim 0x80227B2C..0x80227B34 because it already
// claims 0x80227B00..0x80227B2C immediately below it, and a unit may not claim two discontiguous ranges
// in one section (`dtk dol split`: "Cyclic dependency encountered while resolving link order").
// A unit that emits nothing but `fn_80227B2C` can, and it keeps the retail name, so every REL import
// still resolves. The family is 0x8021FA80, 0x80227B2C, 0x80229FBC and 0x80232334, and
// `docs/research/rel_loaders.md` has the rest of it.
#include "MetroidPrime/ScriptLoader.hpp"

// The same 8-byte slot `RsfAudio.cpp` defines. The struct is repeated rather than shared
// because it is a local definition in that file, and moving it into a header would edit a
// `Matching` unit.
struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

extern SLoaderSlot gLoader_RsfAudio;

extern "C" void fn_80227B2C(FScriptLoader* loader) { gLoader_RsfAudio.value = loader; }
