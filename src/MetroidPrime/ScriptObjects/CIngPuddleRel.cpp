// CIngPuddleRel.cpp - IngPuddle's (module 32) head, .text 0x0..0xA8: the five functions
// above the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CScriptPlayerProxy.cpp` and
// `MetroidPrime/ScriptObjects/CMetareeSwarmRel.cpp`, and the ranges come from
// `config/G2ME01/rels/IngPuddle/symbols.txt`:
//
//   0x00  fn_32_0   0x08   addi r3,r3,0x460 / blr
//   0x08  fn_32_8   0x2C   lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr / bctrl
//   0x34  RELExit   0x24   li r3,0 / bl fn_80229EE0
//   0x58  RELMain   0x20   bl fn_32_78
//   0x78  fn_32_78  0x30   lbl_32_bss_0 = fn_32_A8 ; fn_80229EE0(&lbl_32_bss_0)
//
// Everything else in the module is left unclaimed, so dtk fills it from retail and the module's
// sha1 against `config/G2ME01/config.yml` still holds. The first neighbour left retail is
// fn_32_A8 (0xA8, 0x1E4), the module's own entity loader: a generated `SLdrIngPuddle` loader,
// and the class code behind it needs the CActor/CPhysicsActor hierarchy this tree does not model.
//
// The two callees are named by what they are, not invented:
//   - `fn_80229EE0` is the DOL's 0x80229EE0, two instructions, `stw r3, 0x80419598; blr`.
//     So it stores the *address* of a loader slot, not a loader. `LoadIngPuddle` at 0x80229EB4
//     reads it as `lwz r6, 0x80419598; lwz r12, 0(r6); mtctr r12; bctrl`, which is why the store
//     below hands it `&lbl_32_bss_0` and why that slot is four bytes wide.
//     `src/MetroidPrime/ScriptLoader/IngPuddle.cpp` holds the reader; the 8-byte setter is left
//     unclaimed there because REL modules import it by its retail name.
//   - `lbl_32_bss_0` is `.bss:0x0`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. This unit's split claims .text only, so dtk's `.bss` object has to define it, and
//     a second definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100%.

#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

// **fn_32_0 and fn_32_8 are vtable entries, not free functions.** dtk puts both in the module's
// FORCEACTIVE list, and `.data:0xD0` - CIngPuddle's own vtable - stores them at offsets 0x38 and
// 0x3C. `fn_32_0` hands back `this + 0x460`; `fn_32_8` calls whatever sits in vtable slot 0x38,
// which for CIngPuddle is `fn_32_0` itself.
//
// So the call below is a member call, and that spelling is measured rather than guessed. Loading
// the vtable by hand - `void* const* vt = *(void* const* const*)self;` and calling `vt[14]` -
// compiles to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, and that one register is 99.09%
// on the function (measured, 2026-09-29). `mwcceppc` only reaches for r12 on its own
// virtual-dispatch path, and it lays a class's virtuals out the way retail's vtable is laid out:
// two leading words (offset-to-top, then the RTTI pointer, both zero in this REL) and then one
// word per virtual. Thirteen virtuals therefore put the last one at 0x38, and calling it gives
// retail's seven instructions byte for byte. The slots are named by position because no header
// here models a CActor virtual; none of them is defined or called from this file, because the
// only object that carries this vtable is the module's own retail bytes.
class CIngPuddleVTable {
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
CEntity* fn_32_A8(CStateManager&, CInputStream&, const CEntityInfo&);
void fn_80229EE0(FScriptLoader* loader);

#ifdef __MWERKS__
extern FScriptLoader lbl_32_bss_0;
#else
FScriptLoader lbl_32_bss_0 = 0;
#endif

void fn_32_78() {
  lbl_32_bss_0 = fn_32_A8;
  fn_80229EE0(&lbl_32_bss_0);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the matching
// build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMetareeSwarmRel.cpp`: listing this file in `files.cmake` would make the port link `fn_32_A8`
// and `fn_80229EE0`, which it cannot, and `tools/link_check.sh --strict` fails on a growing
// undefined count. The port keeps reading `IngPuddle.rel` off the disc through `platform/rel.cpp`.
#ifdef __MWERKS__
void RELMain() { fn_32_78(); }

void RELExit() { fn_80229EE0(nullptr); }
#else
void mp_relmain_ingpuddle() { fn_32_78(); }

void mp_relexit_ingpuddle() { fn_80229EE0(nullptr); }
#endif

void fn_32_8(CIngPuddleVTable* self) { self->Slot12(); }

// .text 0x0, 0x08 bytes. Vtable entry 0x38 of CIngPuddle (`TypesMatch.cpp` gives CIngPuddle the
// parent CPhysicsActor); it hands back a member 0x460 bytes into the object, which `fn_32_8` above
// then calls. Written as a byte-pointer walk because the class has no header here to name the
// member - see the `CIngPuddleRel` section of `docs/research/raw_offsets.md`.
void* fn_32_0(const void* self) {
  return const_cast< char* >(reinterpret_cast<const char*>(self) + 0x460);
}
}
