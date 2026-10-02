// CGunTurretBaseForwarders.cpp - a carve of GunTurret's .text 0x0008B58..0x0008B98, two functions.
//
// **The claim is .text 0x0008B58..0x0008B98 and nothing else.** Everything outside that range stays
// unclaimed in `config/G2ME01/rels/GunTurret/splits.txt`, so `dtk` fills it from retail and the
// module's sha1 against `config/G2ME01/config.yml` still holds. The range is exactly the two
// functions and the bytes between them are none: `fn_28_8B58` is 0x20 bytes and `fn_28_8B78` starts
// at 0x8B78 = 0x8B58 + 0x20.
//
// **Both are pure forwarders to `CPatterned`,** and they are virtual slots in the module's class
// vtable (`build/G2ME01/GunTurret/ldscript.lcf` FORCEACTIVE holds both, which is how `dtk` found
// them: the `.rodata` vtable references them, so neither is dead-stripped from the link - the trap
// `docs/RUNNING_THE_DECOMP.md` records for a unit claiming a function nothing else references).
// Each body is the `stwu/mflr/stw/bl/lwz/mtlr/addi/blr` sequence a non-inline call to a base method
// produces, with no register shuffling: `this` is already in r3 and the manager in r4 when the
// `bl` goes out, so the call is to the qualified base method on `this` rather than a renamed
// spelling of it.
//
// **The class is named `CGunTurretBase` by the module itself, not by us.** The module's `.text` calls
// `"TCastToPtr<14CGunTurretBase>__FP7CEntity"` twice (`fn_28_856C`, `fn_28_90C4`), so the mangled
// name is retail's. No header here declares that class, and this unit deliberately does not declare
// one: a class with virtuals would make mwcceppc emit a vtable into this object, and a `.data` the
// unit does not claim moves the module. So the bodies are ordinary C++ over `CPatterned*` - the
// real base class, whose `Render`/`PreRender` they call - and nothing else is claimed about the
// layout. This is the same arrangement as `GunTurretAccessors.cpp` (offsets stated literally) and
// `CRipperForwarders.cpp` (forwarders spelled against the real base class).
//
// The `const` on `fn_28_8B58`'s `self` is retail's: `CPatterned::Render` is a const member, so the
// override that forwards to it has to be spelled on a const object.
//
// Definitions are in descending retail text order, because mwcceppc emits definitions in reverse
// source order and mwldeppc keeps the object's `.text` order verbatim. Ascending, the module's
// bytes come out permuted and its hash breaks while objdiff still reads 100%.

#include "MetroidPrime/CStateManager.hpp"
#include "MetroidPrime/Enemies/CPatterned.hpp"

extern "C" {

// .text 0x0008B78, 0x20 bytes. `PreRender` forwards to `CPatterned::PreRender` and does nothing else.
void fn_28_8B78(CPatterned* self, CStateManager& mgr) { self->CPatterned::PreRender(mgr); }

// .text 0x0008B58, 0x20 bytes. `Render` forwards to `CPatterned::Render` and does nothing else.
void fn_28_8B58(const CPatterned* self, const CStateManager& mgr) { self->CPatterned::Render(mgr); }

} // extern "C"