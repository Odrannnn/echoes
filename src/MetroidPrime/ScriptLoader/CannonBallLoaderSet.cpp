// Retail `SetLoader_CannonBall__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity`,
// .text 0x8021FAB4..0x8021FABC, 0x8 = 8 bytes:
//
//     8021fab4  stw  r3,-26696(r13)
//     8021fab8  blr
//
// `-26696(r13)` off `_SDA_BASE_` = 0x8041FD80 is 0x80419538, which `tools/sda.py` names
// `gLoader_CannonBall` - the `.sbss` slot that `MetroidPrime/ScriptLoader/CannonBall.cpp`
// owns (`.sbss 0x80419538..0x80419540`) and whose `LoadCannonBall` thunk dereferences. So
// this is the module's loader setter, `gLoader_CannonBall.value = loader`, and `value` is at
// +0 of the 8-byte slot, which is why the store has no displacement.
//
// **It has to be its own unit, and that is the fix for the note in `CannonBall.cpp`.** That
// file says the 8-byte setter "is deliberately NOT claimed: REL modules import it by its
// retail name, so it cannot be renamed and must stay in dtk's auto unit". The name really is
// fixed - the `CannonBall` REL imports `SetLoader_CannonBall` by it - but the constraint is on
// the *name*, not on the file. `CannonBall.cpp` cannot claim 0x8021FAB4..0x8021FABC because it
// already claims 0x8021FA88..0x8021FAB4 immediately below it, and a unit may not claim two
// discontiguous ranges in one section. A unit that emits nothing but this function can, and it
// keeps the retail name, so the REL import still resolves. The same argument closed
// `Coin`, `RsfAudio`, `FlyerSwarm` and `SkyRipple`; `docs/research/rel_loaders.md` has the
// family and this is the fifth member.
#include "MetroidPrime/ScriptLoaderRel.hpp"

// The same 8-byte slot `CannonBall.cpp` defines. The struct is repeated rather than shared
// because it is a local definition in that file, and moving it into a header would edit a
// `Matching` unit.
struct SLoaderSlot {
  FScriptLoader* value;
  unsigned int padding;
};

extern SLoaderSlot gLoader_CannonBall;

// **Not `extern "C"`.** MWCC mangles a free function as `name__<argtypes>` whether or not it
// is a member, so retail's `SetLoader_CannonBall__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity`
// is a *C++* function and matches the plain declaration in
// `include/MetroidPrime/ScriptLoaderRel.hpp`. `extern "C"` would emit the bare name
// `SetLoader_CannonBall`, which is not the symbol `symbols.txt` and the REL import carry.
void SetLoader_CannonBall(FScriptLoader* loader) { gLoader_CannonBall.value = loader; }
