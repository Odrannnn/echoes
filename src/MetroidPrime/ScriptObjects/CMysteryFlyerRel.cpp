// CMysteryFlyerRel.cpp - MysteryFlyer's (module 45) head, .text 0xFC..0x170: the three
// functions RELMain and RELExit sit in. Same arrangement as
// `MetroidPrime/ScriptObjects/CScriptPlayerProxy.cpp`,
// `MetroidPrime/ScriptObjects/CMetareeSwarmRel.cpp` and
// `MetroidPrime/ScriptObjects/CPlantScarabSwarmRel.cpp`, and the ranges come from
// `config/G2ME01/rels/MysteryFlyer/symbols.txt`:
//
//   0x0FC  RELExit  0x24  li r3,0 / bl fn_80232868
//   0x120  RELMain  0x20  bl fn_45_140
//   0x140  fn_45_140 0x30  lbl_45_bss_0 = fn_45_170 ; fn_80232868(&lbl_45_bss_0)
//
// The claim starts at 0xFC rather than at 0x0 on purpose. The fourteen functions below RELExit
// (`fn_45_0`..`fn_45_D0`: the vtable accessors, the two `li r3,0/1` predicate slots and
// `fn_45_D0`'s vtable-0x38 dispatch) are this entity's own members and one contiguous claim
// cannot skip them, so taking them means writing all eighteen functions or none of the head.
// `fn_45_D0` needs a CActor vtable this tree does not model, and `fn_45_10` calls the module's
// `fn_45_2BBC`, so the range stays retail and dtk fills it: the arrangement the recipe
// describes, one contiguous range per unit and `Matching` only where the object reproduces it.
//
// Everything else in the module is left unclaimed for the same reason. The first neighbour left
// retail is fn_45_170 (0x170, 0x30C), the module's own entity loader: behavioural class code, and
// it needs the CActor/CPatterned hierarchy this tree does not model.
//
// The two callees are named by what they are, not invented:
//   - `fn_80232868` is the DOL's 0x80232868, two instructions, immediately after
//     `LoadMysteryFlyer__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x8023283C:
//     `stw r3, gLoader_MysteryFlyer; blr`. So it stores the *address* of a loader slot, not a
//     loader. `LoadMysteryFlyer` in `src/MetroidPrime/ScriptLoader/MysteryFlyer.cpp` (a `Matching`
//     unit) reads it as `lwz r6, gLoader_MysteryFlyer; lwz r12, 0(r6); mtctr r12; bctrl`, which is
//     why the store below hands it `&lbl_45_bss_0` and why that slot is four bytes wide. That
//     file also records that the setter is deliberately not claimed in the DOL, because REL
//     modules import it by its retail name - so it stays in dtk's auto unit and this declaration
//     is the same one `CPlantScarabSwarmRel.cpp` makes for `fn_8022FFF8`. It is `extern "C"`:
//     an alias would be a different symbol and the call would resolve to nothing.
//   - `lbl_45_bss_0` is `.bss:0x0`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. This unit's split claims .text only, so dtk's `.bss` object has to define it, and
//     a second definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only tools/flip_test.sh would catch it.

#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

extern "C" {
CEntity* fn_45_170(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_80232868(FScriptLoader* loader);

#ifdef __MWERKS__
extern FScriptLoader lbl_45_bss_0;
#else
FScriptLoader lbl_45_bss_0 = 0;
#endif

// .text 0x140, 0x30 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so the store
// writes the slot itself and leaves r3 holding its address for the setter call.
void fn_45_140() {
  lbl_45_bss_0 = fn_45_170;
  fn_80232868(&lbl_45_bss_0);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the
// host these take distinct names. The MWCC branch is the retail source token for token, so the
// matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMetareeSwarmRel.cpp`, `CPlantScarabSwarmRel.cpp` and `CSnakeWeedSwarmRel.cpp`: listing this
// file in `files.cmake` would make the port link `fn_45_170` and `fn_80232868`, which it cannot,
// and `tools/link_check.sh --strict` fails on a growing undefined count. So the port keeps
// reading `MysteryFlyer.rel` off the disc through `platform/rel.cpp`, and the `#else` branch
// exists only so the file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() { fn_45_140(); }

void RELExit() { fn_80232868(nullptr); }
#else
void mp_relmain_mysteryflyer() { fn_45_140(); }

void mp_relexit_mysteryflyer() { fn_80232868(nullptr); }
#endif
}
