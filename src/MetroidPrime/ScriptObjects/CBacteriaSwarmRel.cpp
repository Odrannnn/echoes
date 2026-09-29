// CBacteriaSwarmRel.cpp - BacteriaSwarm's (module 6) head, .text 0x0..0xA0: the four
// functions above the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CIngPuddleRel.cpp` and
// `MetroidPrime/ScriptObjects/CMysteryFlyerRel.cpp`, and the ranges come from
// `config/G2ME01/rels/BacteriaSwarm/symbols.txt`:
//
//   0x00  fn_6_0   0x2C   lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr / bctrl
//   0x2C  RELExit  0x24   li r3,0 / bl fn_8022A5AC
//   0x50  RELMain  0x20   bl fn_6_70
//   0x70  fn_6_70  0x30   lbl_6_bss_10 = fn_6_A0 ; fn_8022A5AC(&lbl_6_bss_10)
//
// **This head is IngPuddle's** (module 32, `CIngPuddleRel.cpp`) minus its leading 8-byte
// accessor `fn_32_0`, and that is a measurement rather than an impression. Over the range each
// module claims, dtk's disassembly is 40 instructions for this module's 0xA0 against 42 for
// IngPuddle's 0xA8; aligning `fn_6_0` on `fn_32_8` (dropping IngPuddle's two leading
// instructions) leaves 40 against 40 in which **exactly two instructions differ in encoding, both
// of them `bl`** - the calls to each module's own loader-setter import. A third `bl` differs only
// in the name dtk prints, `bl fn_6_70` against `bl fn_32_78`: both encode as `48000015`, because
// each branches to the function immediately after it. So the stand-in class below is the one
// `CIngPuddleRel.cpp` measured, not a guess.
//
// Everything else in the module is left unclaimed, so dtk fills it from retail and the module's
// sha1 against `config/G2ME01/config.yml` still holds. The first neighbour left retail is
// fn_6_A0 (0xA0, 0x74C), the module's own entity loader: behavioural class code, and it needs
// the CActor/CPatterned hierarchy this tree does not model.
//
// The two callees are named by what they are, not invented:
//   - `fn_8022A5AC` is the DOL's 0x8022A5AC, two instructions, immediately after
//     `LoadBacteriaSwarm__FR13CStateManagerR12CInputStreamRC11CEntityInfo` at 0x8022A580
//     (0x2C bytes, so it ends exactly at 0x8022A5AC): `stw r3, gLoader_BacteriaSwarm; blr`.
//     So it stores the *address* of a loader slot, not a loader, and
//     `src/MetroidPrime/ScriptLoader/BacteriaSwarm.cpp` - a `Matching` unit - reads that slot as
//     `lwz r6, gLoader_BacteriaSwarm; lwz r12, 0(r6); mtctr r12; bctrl`, which is why the store
//     below hands it `&lbl_6_bss_10` and why that slot is four bytes wide. That file also records
//     why the setter is deliberately not claimed in the DOL: REL modules import it by its retail
//     name. **The import name here is the plain `fn_8022A5AC`**, checked in the module's own
//     `build/G2ME01/BacteriaSwarm/BacteriaSwarm.preplf` import table - not the long MWCC-mangled
//     `SetLoader_...` form `CIngSnatchingSwarmRel.cpp` had to spell out - so no `symbols.txt`
//     rename is needed and the DOL is untouched.
//   - `lbl_6_bss_10` is `.bss:0x10`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. This unit's split claims .text only, so dtk's `.bss` object has to define it, and
//     a second definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100%.

#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

// **fn_6_0 is a vtable entry, not a free function.** dtk puts it in the module's FORCEACTIVE
// list, and `.data:0x18` - CBacteriaSwarm's own vtable, 0x98 bytes = 38 words, two of them
// leading (offset-to-top and the RTTI pointer, both zero here) and 36 virtuals - stores it at
// `.data:0x54`, which is vtable offset 0x3C (`auto_04_00000000_data.s`). It calls whatever sits
// at vtable offset 0x38, which for CBacteriaSwarm is `HealthInfo__6CActorFv`.
//
// So the call below is a member call, and that spelling is measured rather than guessed: loading
// the vtable by hand - `void* const* vt = *(void* const* const*)self;` and calling `vt[14]` -
// compiles to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)` (measured 99.09% on the sibling in
// `CIngPuddleRel.cpp`). `mwcceppc` only reaches for r12 on its own virtual-dispatch path, and it
// lays a class's virtuals out the way retail's vtable is laid out: two leading words then one
// word per virtual, so thirteen virtuals put the last one at 0x38 and calling it gives retail's
// seven instructions byte for byte. The slots are named by position because no header here models
// a CActor virtual; none of them is defined or called from this file, because the only object that
// carries this vtable is the module's own retail bytes.
class CBacteriaSwarmVTable {
public:
  virtual void Slot0();
  virtual void Slot1();
  virtual void Slot2();
  virtual void Slot3();
  virtual void Slot4();
  virtual void Slot5();
  virtual void Slot6();
  virtual void Slot7();
  virtual void Slot8();
  virtual void Slot9();
  virtual void Slot10();
  virtual void Slot11();
  virtual void* Slot12();
};

extern "C" {
CEntity* fn_6_A0(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_8022A5AC(FScriptLoader* loader);

#ifdef __MWERKS__
extern FScriptLoader lbl_6_bss_10;
#else
FScriptLoader lbl_6_bss_10 = 0;
#endif

// .text 0x70, 0x30 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so the store
// writes the slot itself and leaves r3 holding its address for the setter call.
void fn_6_70() {
  lbl_6_bss_10 = fn_6_A0;
  fn_8022A5AC(&lbl_6_bss_10);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the matching
// build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CIngPuddleRel.cpp`: listing this file in `files.cmake` would make the port link `fn_6_A0` and
// `fn_8022A5AC`, which it cannot, and `tools/link_check.sh --strict` fails on a growing undefined
// count. The port keeps reading `BacteriaSwarm.rel` off the disc through `platform/rel.cpp`, and
// `tools/check_files_cmake.py` counts this file under "further units are out because they define a
// module entry point (RELMain/RELExit), which collides in a flat link".
#ifdef __MWERKS__
void RELMain() { fn_6_70(); }

void RELExit() { fn_8022A5AC(nullptr); }
#else
void mp_relmain_bacteriaswarm() { fn_6_70(); }

void mp_relexit_bacteriaswarm() { fn_8022A5AC(nullptr); }
#endif

// .text 0x0, 0x2C bytes. Vtable entry 0x3C of CBacteriaSwarm; it dispatches through vtable slot
// 0x38. Written as a member call, so no raw offset appears in this file and
// `docs/research/raw_offsets.md` has no `CBacteriaSwarmRel` section to add.
void fn_6_0(CBacteriaSwarmVTable* self) { self->Slot12(); }
}
