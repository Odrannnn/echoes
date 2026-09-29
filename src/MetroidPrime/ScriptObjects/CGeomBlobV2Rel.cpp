// CGeomBlobV2Rel.cpp - GeomBlobV2's (module 25) head, .text 0x23E8..0x2490: the four
// functions around the module's entry points. Same arrangement as
// `MetroidPrime/ScriptObjects/CIngPuddleRel.cpp` and
// `MetroidPrime/ScriptObjects/CMetareeSwarmRel.cpp`, and the range comes from
// `config/G2ME01/rels/GeomBlobV2/symbols.txt`:
//
//   0x23E8 fn_25_23E8 0x2C  lbl_25_data_10[0x3C] / lbl_25_data_A8[0x3C]; calls slot 0x38
//   0x2414 RELExit    0x28  li r3,0 / bl fn_80229EAC / bl fn_25_48A8
//   0x243C RELMain    0x24  bl fn_25_2460 / bl fn_25_48CC
//   0x2460 fn_25_2460 0x30  lbl_25_bss_0 = fn_25_2490 ; fn_80229EAC(&lbl_25_bss_0)
//
// **This module's head is not at 0x0, and that is measured rather than assumed.** `fn_25_0`
// (0x0, 0x1FC) is a real function - a bone-blend loop over 0x50-byte records that calls
// `close_enough__FRC11CQuaternionRC11CQuaternionf` and `__as__9CMatrix3fFRC9CMatrix3f` - and
// `fn_25_1FC` above it is 0x894. There is no accessor block at the front of this module either,
// which is why the claim starts at 0x23E8 rather than at 0: everything below it is class code.
// The eight trivial accessors that sit immediately above the loader are **two** claims,
// `CGeomBlobV2Accessors.cpp` at `0x2544..0x255C` and `CGeomBlobV2AccessorsTail.cpp` at
// `0x256C..0x2584`; one unit cannot claim two discontiguous ranges, because `fn_25_2490`
// (0x2490, 0xB4) - the module's own entity loader, which allocates 0x1E0
// bytes through `__nw__FUlPCcPCc` and calls `fn_25_4290` - stands between the two, and the two
// float getters at `0x255C..0x256C` are not in dtk's FORCEACTIVE list and are dead-stripped
// if a unit claims them.
//
// Everything in this claim is referenced, so nothing here is a dead-stripping hazard: dtk's
// generated `ldscript.lcf` FORCEACTIVE list for this module holds `fn_25_23E8` (and both vtables
// store it), `_epilog`/`_prolog` call `RELExit`/`RELMain`, and `RELMain` calls `fn_25_2460`.
//
// The three callees are named by what they are, not invented:
//   - `fn_80229EAC` is the DOL's 0x80229EAC, two instructions,
//     `stw r3, -26608(r13) ; blr`, i.e. `stw r3, lbl_80419590` (`tools/sda.py` resolves the
//     displacement against `_SDA_BASE_` 0x8041FD80). So it stores the *address* of a loader slot,
//     not a loader. `LoadIngPuddle` at 0x80229EB4 reads the neighbouring
//     `gLoader_IngPuddle` the same way - `lwz r6, g ; lwz r12, 0(r6) ; mtctr r12 ; bctrl` - which
//     is why the store below hands it `&lbl_25_bss_0`. It is an unnamed DOL symbol, so it is
//     declared `extern "C"` by that name rather than by a mangled `SetLoader_*` spelling.
//   - `fn_25_48A8` and `fn_25_48CC` are this module's own, at 0x48A8 and 0x48CC: the second
//     loader's teardown and registration. They fill `lbl_25_bss_8` through the other unnamed DOL
//     setter `fn_802274FC` (`stw r3, -26664(r13)` = `lbl_80419558`). **They are in the
//     unclaimed remainder** - one unit cannot claim two discontiguous ranges - so they are called
//     by their dtk names and stay retail.
//   - `fn_25_2490` is this module's entity loader, at 0x2490. Same reason: declared, not defined.
//   - `lbl_25_bss_0` is `.bss:0x0`, `size:0x8 data:4byte`: the module's own copy of the loader
//     pointer. This unit's split claims .text only, so dtk's `.bss` object has to define it, and
//     a second definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// **`fn_25_23E8` is a vtable entry, not a free function.** `.data:0x10` (`lbl_25_data_10`, 0x80
// bytes) and `.data:0xA8` (`lbl_25_data_A8`, 0x7C) both store it at offset 0x3C, and it calls
// whatever sits in vtable slot 0x38, which both tables name `HealthInfo__6CActorFv`. So the call
// below is a member call, and that spelling is measured rather than guessed: loading the vtable
// by hand - `void* const* vt = *(void* const* const*)self;` and calling `vt[14]` - compiles to
// `lwz r3,0(r3)` where retail has `lwz r12,0(r3)`, and that one register is 99.09% on the
// function (measured in `CIngPuddleRel.cpp`, 2026-09-29, where the same seven instructions are
// `fn_32_8`). `mwcceppc` only reaches for r12 on its own virtual-dispatch path, and it lays a
// class's virtuals out the way retail's vtable is laid out: two leading words (offset-to-top,
// then the RTTI pointer, both zero in this REL) and then one word per virtual. Thirteen virtuals
// therefore put the last one at 0x38. The slots are named by position because no header here
// models a CActor virtual; none of them is defined or called from this file, because the only
// object that carries this vtable is the module's own retail bytes. Slot 12 returns
// `CHealthInfo*` because `.data:0x10` names it `HealthInfo__6CActorFv` and
// `include/MetroidPrime/CActor.hpp:85` gives `CActor::HealthInfo` that return type; the call
// discards it, so this does not affect the bytes.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100% and only tools/flip_test.sh would catch it.

#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

class CHealthInfo;

// The vtable slot fn_25_23E8 dispatches to; see the note above on the layout.
class CGeomBlobV2Dispatch {
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
  virtual CHealthInfo* Slot12();
};

extern "C" {
// .text 0x2490, 0xB4 bytes. The module's own entity loader: `__nw__FUl(0x1E0, "GeomBlobV2")`
// then `fn_25_4290`. Unclaimed, and behavioural class code - it needs the CActor/CPatterned
// hierarchy this tree does not model.
CEntity* fn_25_2490(CStateManager&, CInputStream&, const CEntityInfo&);

// .text 0x48CC and 0x48A8. The second loader's registration and teardown, filling
// `lbl_25_bss_8` through the other unnamed DOL setter `fn_802274FC`. Both unclaimed; both called
// by name because one unit cannot claim two discontiguous ranges.
void fn_25_48A8();
void fn_25_48CC();

// The DOL's 0x80229EAC, `stw r3, lbl_80419590 ; blr`. An unnamed DOL symbol, so `extern "C"` by
// that name - see the note at the top of the file.
void fn_80229EAC(FScriptLoader* loader);

#ifdef __MWERKS__
extern FScriptLoader lbl_25_bss_0;
#else
FScriptLoader lbl_25_bss_0 = 0;
#endif

// .text 0x2460, 0x30 bytes. `stwu r0, lbl_25_bss_0` leaves r3 pointing at the slot, which is
// what `fn_80229EAC` is then handed.
void fn_25_2460() {
  lbl_25_bss_0 = fn_25_2490;
  fn_80229EAC(&lbl_25_bss_0);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names. The MWCC branch is the retail source token for token, so the
// matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CMetareeSwarmRel.cpp` and `CIngPuddleRel.cpp`: listing this file in `files.cmake` would make
// the port link `fn_25_2490` and `fn_80229EAC`, which it cannot, and
// `tools/link_check.sh --strict` fails on a growing undefined count. The port keeps reading
// `GeomBlobV2.rel` off the disc through `platform/rel.cpp`. The `#else` branch exists only so the
// file is still a valid translation unit.
#ifdef __MWERKS__
void RELMain() {
  fn_25_2460();
  fn_25_48CC();
}

void RELExit() {
  fn_80229EAC(nullptr);
  fn_25_48A8();
}
#else
void mp_relmain_geomblobv2() {
  fn_25_2460();
  fn_25_48CC();
}

void mp_relexit_geomblobv2() {
  fn_80229EAC(nullptr);
  fn_25_48A8();
}
#endif

// .text 0x23E8, 0x2C bytes. Vtable entry 0x3C of both of the module's vtables; the call it makes
// targets vtable slot 0x38, which `.data:0x10` names `HealthInfo__6CActorFv`.
void fn_25_23E8(CGeomBlobV2Dispatch* self) { self->Slot12(); }
}
