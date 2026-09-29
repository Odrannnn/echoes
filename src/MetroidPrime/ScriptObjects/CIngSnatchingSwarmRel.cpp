// CIngSnatchingSwarmRel.cpp - IngSnatchingSwarm's (module 33) head, .text 0x0..0xA8: the five
// functions above the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CIngPuddleRel.cpp` and
// `MetroidPrime/ScriptObjects/CScriptPlayerProxy.cpp`, and the ranges come from
// `config/G2ME01/rels/IngSnatchingSwarm/symbols.txt`:
//
//   0x00  fn_33_0   0x08   addi r3, r3, 0x1F4 / blr
//   0x08  fn_33_8   0x2C   lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr / bctrl
//   0x34  RELExit   0x24   li r3,0 / bl SetLoader_IngSnatchingSwarm
//   0x58  RELMain   0x20   bl fn_33_78
//   0x78  fn_33_78  0x30   lbl_33_bss_0 = fn_33_A8 ; SetLoader_IngSnatchingSwarm(&lbl_33_bss_0)
//
// **This head is instruction-for-instruction IngPuddle's** (module 32, `CIngPuddleRel.cpp`):
// 0xA8 bytes is 42 instructions, and diffing the two modules' dtk disassembly over that range
// gives 3 differing instructions - the `addi` immediate and the two `bl` displacements, the latter
// because each module registers its own loader. Every other line differs only in the module's own
// symbol names and is byte-identical. The two accessors sit at the *same two words of the same
// 31-word vtable* (0x38 and 0x3C, measured in `auto_04_00000000_data.s` for both modules), so the
// member-call spelling below is the one `CIngPuddleRel.cpp` measured, not a guess.
//
// Everything else in the module is left unclaimed, so dtk fills it from retail and the module's
// sha1 against `config/G2ME01/config.yml` still holds. The first neighbour left retail is
// fn_33_A8 (0xA8, 0x5D4), the module's own entity loader: behavioural class code, and it needs the
// CActor hierarchy this tree does not model (`TypesMatch__18CIngSnatchingSwarmCFi` at 0x8009C5F4
// is `cmpwi r4,0x1e` against `TypesMatch__6CActorCFi`, so the parent is `CActor`, not
// `CPhysicsActor` as IngPuddle's is).
//
// The two callees are named by what they are, not invented:
//   - `SetLoader_IngSnatchingSwarm` is the DOL's 0x8021BA8C, two instructions,
//     `stw r3, gLoader_IngSnatchingSwarm; blr`. So it stores the *address* of a loader slot, not a
//     loader. `LoadIngSnatchingSwarm` at 0x8021BA60 reads it as
//     `lwz r6, gLoader_IngSnatchingSwarm; lwz r12, 0(r6); mtctr r12; bctrl`, which is why the
//     store below hands it `&lbl_33_bss_0` and why that slot is four bytes wide. It is already
//     decompiled, at `src/MetroidPrime/ScriptLoaderRel.cpp:141` as the C++ name
//     `SetLoader_IngSnatchingSwarm`, and this file must call it by the *mangled* name - see the
//     declaration below.
//   - `lbl_33_bss_0` is `.bss:0x0`, `size:0x4 data:4byte`: the module's own copy of the loader
//     pointer. This unit's split claims .text only, so dtk's `.bss` object has to define it, and
//     a second definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100%.

#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

// **fn_33_0 and fn_33_8 are vtable entries, not free functions.** dtk puts both in the module's
// FORCEACTIVE list, and `.data:0x264` - CIngSnatchingSwarm's own vtable, 0x7C bytes = two leading
// words plus 29 virtuals - stores them at offsets 0x38 and 0x3C. `fn_33_0` hands back `this +
// 0x1F4`; `fn_33_8` calls whatever sits in vtable slot 0x38, which for CIngSnatchingSwarm is
// `fn_33_0` itself.
//
// So the call below is a member call, and that spelling is measured rather than guessed: loading
// the vtable by hand - `void* const* vt = *(void* const* const*)self;` and calling `vt[14]` -
// compiles to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)` (measured 99.09% on the sibling in
// `CIngPuddleRel.cpp`). `mwcceppc` only reaches for r12 on its own virtual-dispatch path, and it
// lays a class's virtuals out the way retail's vtable is laid out: two leading words (offset-to-
// top, then the RTTI pointer, both zero in this REL) and then one word per virtual, so thirteen
// virtuals put the last one at 0x38 and calling it gives retail's seven instructions byte for byte.
// The slots are named by position because no header here models a CActor virtual; none of them is
// defined or called from this file, because the only object that carries this vtable is the
// module's own retail bytes.
class CIngSnatchingSwarmVTable {
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
CEntity* fn_33_A8(CStateManager&, CInputStream&, const CEntityInfo&);
// The import is named the way retail's symbol table names it. `SetLoader_IngSnatchingSwarm` is
// what the C++ function is called in `src/MetroidPrime/ScriptLoaderRel.cpp`, and MWCC mangles that
// one into the long name because the `FScriptLoader*` parameter is a function-pointer typedef -
// but this file is `extern "C"`, so MWCC emits the identifier verbatim and the name has to be
// written out in full. Measured, not assumed: the short spelling compiles, links every object and
// then fails the REL step with
//   Failed to find symbol SetLoader_IngSnatchingSwarm in any module
// whereas the long one is what the module imports (it is in the preplf's import list and in
// `config/G2ME01/symbols.txt:9529` at 0x8021BA8C).
void SetLoader_IngSnatchingSwarm__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity(
    FScriptLoader* loader);

#ifdef __MWERKS__
extern FScriptLoader lbl_33_bss_0;
#else
FScriptLoader lbl_33_bss_0 = 0;
#endif

void fn_33_78() {
  lbl_33_bss_0 = fn_33_A8;
  SetLoader_IngSnatchingSwarm__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity(
      &lbl_33_bss_0);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the matching
// build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CIngPuddleRel.cpp`: listing this file in `files.cmake` would make the port link `fn_33_A8` and
// `SetLoader_IngSnatchingSwarm`, which it cannot, and `tools/link_check.sh --strict` fails on a
// growing undefined count. The port keeps reading `IngSnatchingSwarm.rel` off the disc through
// `platform/rel.cpp`, and `tools/check_files_cmake.py` counts this file under "further units are
// out because they define a module entry point (RELMain/RELExit), which collides in a flat link".
#ifdef __MWERKS__
void RELMain() { fn_33_78(); }

void RELExit() {
  SetLoader_IngSnatchingSwarm__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity(nullptr);
}
#else
void mp_relmain_ingsnatchingswarm() { fn_33_78(); }

void mp_relexit_ingsnatchingswarm() {
  SetLoader_IngSnatchingSwarm__FPPFR13CStateManagerR12CInputStreamRC11CEntityInfo_P7CEntity(nullptr);
}
#endif

void fn_33_8(CIngSnatchingSwarmVTable* self) { self->Slot12(); }

// .text 0x0, 0x08 bytes. Vtable entry 0x38 of CIngSnatchingSwarm; it hands back a member 0x1F4
// bytes into the object, which `fn_33_8` above then calls. Written as a byte-pointer walk because
// the class has no header here to name the member - see the `CIngSnatchingSwarmRel` section of
// `docs/research/raw_offsets.md`.
void* fn_33_0(const void* self) {
  return const_cast< char* >(reinterpret_cast<const char*>(self) + 0x1F4);
}
}
