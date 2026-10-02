// CGunTurretBaseTriggers.cpp - a carve of GunTurret's .text 0x0009028..0x0009058, three functions.
//
// **The claim is .text 0x0009028..0x0009058 and nothing else.** Everything outside it stays
// unclaimed in `config/G2ME01/rels/GunTurret/splits.txt`, so `dtk` fills it from retail and the
// module's sha1 against `config/G2ME01/config.yml` still holds. The range is exactly three
// functions with nothing between them: `fn_28_9028` is 0x20 bytes, `fn_28_9048` and `fn_28_9050`
// are 0x8 each at 0x9028 + 0x20 and +0x28.
//
// `fn_28_9028` is a bare forward to `CPatterned::Attacked` - the same `stwu/mflr/stw/bl/lwz/mtlr/
// addi/blr` shape and the same single `bl` with `this`, the manager and the trigger data already
// in r3/r4/r5 - and its boolean result comes back in r3 with nothing added, which is why the return
// type is spelled `bool` and not something wider. `fn_28_9048` and `fn_28_9050` are the byte
// accessors at +0x808 and +0x7e8, the two-instruction shape `GunTurretAccessors.cpp` already
// reproduces fourteen times over in the same module.
//
// Like `fn_28_8B58`/`fn_28_8B78` in `CGunTurretBaseForwarders.cpp`, all three are vtable slots and
// are therefore already in the module's FORCEACTIVE list (`build/G2ME01/GunTurret/ldscript.lcf`),
// so none of them is dead-stripped out of the link.
//
// **No class is declared here.** The module's own `.text` names the class `CGunTurretBase` (the
// mangled `"TCastToPtr<14CGunTurretBase>__FP7CEntity"` at `fn_28_856C` and `fn_28_90C4`), but no
// header here declares it, and declaring a class with virtuals would make mwcceppc emit a vtable
// into this object - a `.data` the unit does not claim, which moves the module. So the bodies are
// ordinary C++ against `CPatterned`, the real base class, and nothing is claimed about the derived
// layout. The two accessors are stated through literal offsets for the same reason
// `GunTurretAccessors.cpp` does it.
//
// Definitions are in descending retail text order, because mwcceppc emits definitions in reverse
// source order and mwldeppc keeps the object's `.text` order verbatim. Ascending, the module's
// bytes come out permuted and its hash breaks while objdiff still reads 100%.

#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Enemies/CAi.hpp"
#include "MetroidPrime/Enemies/CPatterned.hpp"

extern "C" {

// .text 0x0009050, 0x08 bytes. the byte at +0x7e8.
unsigned char fn_28_9050(const void* self) {
  return *reinterpret_cast< const unsigned char* >(static_cast< const char* >(self) + 0x7E8);
}

// .text 0x0009048, 0x08 bytes. the byte at +0x808.
unsigned char fn_28_9048(const void* self) {
  return *reinterpret_cast< const unsigned char* >(static_cast< const char* >(self) + 0x808);
}

// .text 0x0009028, 0x20 bytes. `Attacked` forwards to `CPatterned::Attacked` and does nothing else.
// The manager is a non-const reference because that is how `CPatterned::Attacked` takes it - its
// mangled name is `Attacked__10CPatternedCFR13CStateManagerRC12CTriggerData`, and this compiler
// mangles `CStateManager&` exactly like that (`GetAnimOver__10CPatternedCFR13CStateManager
// RC12CTriggerData` is declared the same way in `include/MetroidPrime/Enemies/CPatterned.hpp`).
bool fn_28_9028(const CPatterned* self, CStateManager& mgr, const CTriggerData& data) {
  return self->CPatterned::Attacked(mgr, data);
}

} // extern "C"