// CDarkCommandoRender.cpp - DarkCommando's (module 3) render run, .text 0x91F0..0x9218: two
// functions, `fn_3_91F0` and `fn_3_91F8`.
//
// Same shape as `MetroidPrime/ScriptObjects/CIngSnatchingSwarmBounds.cpp` (module 33), which is
// already `Matching` at 4/4: `fn_33_39B8` is the same `addi r3,r3,0x1c0` this is
// `addi r3,r3,0x17c`, and `fn_33_3A34`'s frame around a qualified `CActor::PreRender` is
// instruction for instruction the frame retail puts around its qualified `CActor::Render` here.
// Both spellings transfer verbatim, so this file is the module's own code, not a guess.
//
//   0x91F0 fn_3_91F0  0x08  addi r3,r3,0x17c / blr          - the address of the sub-object at
//                                                              +0x17C, not a value read out of it
//   0x91F8 fn_3_91F8  0x20  the 0x10 frame, then a **qualified** `CActor::Render` with `self`
//                      already in r3 and `mgr` already in r4, so nothing is moved
//
// The range is claimed exactly and nothing else. `fn_3_9184` (0x9184, 0x6C) below and
// `fn_3_9218` (0x9218, 0x8C) above stay retail, so dtk fills them and the module's sha1 against
// `config/G2ME01/config.yml` still holds. Both this file's functions are in the module's
// `ldscript.lcf` FORCEACTIVE block (`fn_3_91F0`, `fn_3_91F8`), so nothing dead-strips.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% while the module's hash breaks. Checked with
// `python3 tools/check_decl_order.py --unit MetroidPrime/ScriptObjects/CDarkCommandoRender.cpp`.
//
// `tools/unit_fit.sh` prints `.data claimed - ours 40 <- NOT CLAIMED BY splits.txt` for this
// object, and that is measured rather than assumed: the 0x28 bytes are `SolidMaterial` and nine
// companions from `Collision/CMaterialList.hpp`, which `MetroidPrime/CActor.hpp` reaches, and
// `MetroidPrime/ScriptObjects/CIngSnatchingSwarmBounds.cpp` carries the identical 40 unclaimed
// bytes with its module's hash also holding. `CDarkCommandoRel.cpp` avoids them by declaring its
// own one-method `CPhysicsActor` stand-in rather than including the real header; the one place
// that difference matters is a unit claiming the module's `.data`, and no unit here does.

#include "types.h"

#include "MetroidPrime/CActor.hpp"
#include "MetroidPrime/CStateManager.hpp"

// The object these two are written over, reduced to the one member either of them touches. The
// padding between offset 0 and the sub-object is the space the real CDarkCommando body occupies
// and is written as `char x_padN[...]`, which `tools/check_raw_offsets.py` does not count as a raw
// offset. Nothing here is evidence about a class declaration this tree does not have: retail names
// the class itself only through its vtable.
class CDarkCommandoRender {
private:
  char x_pad0[0x17C];

public:
  // +0x17C. Retail's own destructor destroys it as a `CDamageVulnerability`
  // (`fn_3_9890`: `addi r3,r30,0x17c ; li r4,-1 ; bl __dt__20CDamageVulnerabilityFv`), but the
  // function below hands back its address rather than a value read out of it, so the class
  // carries the range and `char` is enough - the same decision
  // `CIngSnatchingSwarmBounds.cpp` records for its own +0x1C0 sub-object.
  char x17C[0x1BC - 0x17C];
};

// `tools/check_symbol_names.py` reads these names out of the object, so they have to be exactly
// what `config/G2ME01/rels/DarkCommando/symbols.txt` calls them.
extern "C" {
// .text 0x91F8, 0x20 bytes. Retail calls the base implementation directly rather than dispatching
// through the vtable: the imported name is `Render__6CActorCFRC13CStateManager`, with no `Fv`
// suffix a virtual call carries, and `self` is already in r3 and `mgr` already in r4 - which is why
// the frame saves r31 nowhere and only holds the saved LR at r1+0x14.
void fn_3_91F8(CDarkCommandoRender* self, CStateManager& mgr) {
  reinterpret_cast< CActor* >(self)->CActor::Render(mgr);
}

// .text 0x91F0, 0x08 bytes. the address of the sub-object at +0x17C, so the whole body is the one
// `addi r3,r3,0x17c` and the array member decays to its own address.
char* fn_3_91F0(CDarkCommandoRender* self) { return self->x17C; }
}