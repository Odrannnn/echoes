// CFogOverlayRel.cpp - FogOverlay's (module 23) head, .text 0x0..0x100: the five functions
// above the module's class code. Same arrangement as
// `MetroidPrime/ScriptObjects/CBacteriaSwarmRel.cpp` and
// `MetroidPrime/ScriptObjects/CSnakeWeedSwarmRel.cpp`; the ranges come from
// `config/G2ME01/rels/FogOverlay/symbols.txt`:
//
//   0x00  fn_23_0   0x60   the class's deleting destructor
//   0x60  fn_23_60  0x2C   virtual dispatch, vtable slot 0x38
//   0x8C  RELExit   0x24   li r3,0 / bl fn_80232834
//   0xB0  RELMain   0x20   bl fn_23_D0
//   0xD0  fn_23_D0  0x30   lbl_23_bss_0 = fn_23_100 ; fn_80232834(&lbl_23_bss_0)
//
// **This head has no accessor block.** Most landed heads in this family open with the REL
// loader generator's thirteen short accessors - Krocuss, MysteryFlyer, ElitePirate,
// AtomicAlpha all do, which is why the recipe's first step is "copy `CAtomicAlphaRel.cpp`" - so
// read the first function of a module's `.text` before planning its head: it decides five
// functions or eighteen. FogOverlay opens with a 0x60-byte deleting destructor, so nothing can
// be copied from a sibling's accessors and the claim is five functions where those are
// eighteen. The reason is visible in the module's own data: `lbl_23_data_0` (`.data:0x0`) is
// 0x7C bytes = two leading zero words plus 29 slots, i.e. the plain `CActor` table, not one of
// the `CAi`/generator tables that carry an extra accessor.
//
// Everything else in the module is left unclaimed, so dtk fills it from retail and the module's
// sha1 against `config/G2ME01/config.yml` still holds. The first neighbour left retail is
// `fn_23_100` (0x100, 0x3E4), the module's own entity loader, and the sixteen functions from
// there to `fn_23_117C` are the class's own methods (Draw, AcceptScriptMsg, the constructor
// chain): behavioural class code that needs the CActor/CPatterned hierarchy this tree does not
// model, the same wall as Metaree's tail and IngSpaceJumpGuardian's 125 methods.
//
// The two callees are named by what they are, not invented:
//   - `fn_80232834` is the DOL's 0x80232834, two instructions,
//     `stw r3, gLoader_FogOverlay@sda21(r0); blr` in
//     `build/G2ME01/asm/auto_03_80232834_text.s`, and a plain named DOL symbol in
//     `config/G2ME01/symbols.txt` at 0x80232834 - immediately after `LoadFogOverlay__...` at
//     0x80232808, which is 0x2C bytes and so ends exactly there. So it stores the *address* of
//     a loader slot, not a loader, and the store below hands it `&lbl_23_bss_0`.
//     `src/MetroidPrime/ScriptLoader/FogOverlay.cpp` is that thunk, and the setter is its own
//     DOL unit, `Carve80232834.c`, which keeps the imported name verbatim. The import
//     name here is the plain `fn_80232834`, checked in the module's own
//     `build/G2ME01/FogOverlay/FogOverlay.preplf` import table, so no mangled `SetLoader_*` name
//     is needed and `config/G2ME01/symbols.txt` is untouched by that.
//   - `lbl_23_bss_0` is `.bss:0x0`, `size:0x4 data:4byte` in
//     `build/G2ME01/FogOverlay/asm/auto_05_00000000_bss.s`, so the record is four bytes and the
//     four-byte spelling is right here (a module whose record is bigger needs a pmf and a
//     different registration - see "Metroid" in `docs/RUNNING_THE_DECOMP.md`). This unit's
//     split claims .text only, so dtk's `.bss` object has to define the slot, and a second
//     definition under MWCC is what produced mwldeppc's internal linker error on
//     ScriptPlayerProxy. Hence extern under MWCC and a host definition.
//
// The two REL entry points are renamed in `config/G2ME01/rels/FogOverlay/symbols.txt`
// (`fn_23_8C` -> `RELExit`, `fn_23_B0` -> `RELMain`, both `scope:global`). dtk's own
// `_epilog`/`_prolog` (0x109C / 0x10C0) are force-active, are left unclaimed here, and take
// their second `bl` through these two; unnamed - or named without `scope:global`, as
// AtomicAlpha's were - the module grows 48 bytes of relocations. This is `tools/wire_rel_setup.py`'s
// rule; the module's own symbols file carried neither name.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100%. Only `flip_test.sh` catches that, and see the item
// notes: `flip_test.sh` cannot locate the source of any REL unit, so the module's sha1 is the
// acceptance test here.

#include "Kyoto/Alloc/CMemory.hpp"
#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

class CEntity;

// The module's own vtable, `.data:0x0`, 0x7C bytes. **Referenced, never defined**: a class
// with a defined virtual emits a `__vt__` into `.data` and the module's sha1 moves. It is in the
// module's own `ldscript.lcf` FORCEACTIVE list, and so are `fn_23_0` and `fn_23_60`, and
// `.data:0x0` stores both - so nothing in this claim is a dead-stripping hazard and no
// `force_active:` entry is needed.
extern "C" char lbl_23_data_0[];

// **fn_23_0 and fn_23_60 are vtable entries, not free functions.** `lbl_23_data_0` stores
// `fn_23_0` at vtable offset 0x8 (the class's own deleting destructor) and `fn_23_60` at 0x3C.
// `fn_23_60` dispatches through vtable offset 0x38, which `.data:0x0` names
// `HealthInfo__6CActorFv` - so it is the class's override of `GetHealthInfo`, returning that
// record, and the call discards it.
//
// So the call below is a member call, and that spelling is measured rather than guessed: loading
// the vtable by hand - `void* const* vt = *(void* const* const*)self;` and calling `vt[14]` -
// compiles to `lwz r3,0(r3)` where retail has `lwz r12,0(r3)` (measured 99.09% on the siblings
// in `CIngPuddleRel.cpp` and `CSnakeWeedSwarmRel.cpp`). `mwcceppc` only reaches for r12 on its
// own virtual-dispatch path, and it lays a class's virtuals out the way retail's vtable is
// laid out: two leading words (offset-to-top, then the RTTI pointer, both zero in this REL) and
// then one word per virtual, so **thirteen** virtuals put the called one at 0x38. The slots are
// named by position because no header here models a CActor virtual; none of them is defined or
// called from this file, because the only object that carries this vtable is the module's own
// retail bytes - and a defined virtual would emit a `__vt__` of our own into `.data`.
class CFogOverlay {
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
// .text 0x100, 0x3E4 bytes: the module's own entity loader, left retail. Its signature is fixed
// by what the DOL's LoadFogOverlay thunk calls it with - (CStateManager&, CInputStream&,
// const CEntityInfo&) - and only its address is taken here, so the body is not needed to
// reproduce these five functions.
CEntity* fn_23_100(CStateManager&, CInputStream&, const CEntityInfo&);

// The DOL's loader setter, 0x80232834; see the note at the top.
void fn_80232834(FScriptLoader* loader);

// `__dt__6CActorFv` is the base-class destructor the class's deleting destructor calls. It is
// declared and never defined here, which is also what keeps the base's own vtable - and so
// `__vt__6CActor` - out of this object.
CFogOverlay* __dt__6CActorFv(CFogOverlay* self, short flag);

#ifdef __MWERKS__
extern FScriptLoader lbl_23_bss_0;
#else
FScriptLoader lbl_23_bss_0 = 0;
#endif

// .text 0xD0, 0x30 bytes: the module's loader registration. Retail loads the loader out of
// `.text` and the slot address out of `.bss`, and stores the loader with `stwu` so the store
// writes the slot itself and leaves r3 holding its address for the setter call.
void fn_23_D0() {
  lbl_23_bss_0 = fn_23_100;
  fn_80232834(&lbl_23_bss_0);
}

// Every REL module defines RELMain/RELExit, which a flat host link cannot hold, so on the host
// these take distinct names that platform/compiled_modules.cpp could call. The MWCC branch is
// the retail source token for token, so the matching build cannot see this change.
//
// **Nothing calls the host pair, and that is deliberate**, exactly as in
// `CBacteriaSwarmRel.cpp` and `CSnakeWeedSwarmRel.cpp`: listing this file in `files.cmake` would
// make the port link `fn_23_100` and `fn_80232834`, which it cannot, and
// `tools/link_check.sh --strict` fails on a growing undefined count. So the port keeps reading
// `FogOverlay.rel` off the disc through `platform/rel.cpp`, and `tools/check_files_cmake.py`
// counts this file under "further units are out because they define a module entry point
// (RELMain/RELExit), which collides in a flat link".
#ifdef __MWERKS__
void RELMain() { fn_23_D0(); }

void RELExit() { fn_80232834(nullptr); }
#else
void mp_relmain_fogoverlay() { fn_23_D0(); }

void mp_relexit_fogoverlay() { fn_80232834(nullptr); }
#endif

// .text 0x60, 0x2C bytes. Vtable entry 0x3C of CFogOverlay: the class's `GetHealthInfo`,
// which returns what vtable slot 0x38 (`HealthInfo`) returns. Retail's CActor spelling of the
// same function is `return const_cast<CActor*>(this)->HealthInfo();`; the const_cast is
// invisible in the bytes - r3 is already the address that gets passed on - so the record's
// pointer is passed straight through and the result is discarded.
void fn_23_60(CFogOverlay* self) { self->Slot12(); }

// .text 0x0, 0x60 bytes. The class's deleting destructor: vtable entry 0x8 of `lbl_23_data_0`.
//
//   00000000  94 21 FF F0  stwu r1,-0x10(r1)
//   00000010  7C 9F 23 78  mr   r31,r4          ; the deleting flag
//   00000018  7C 7E 1B 79  mr.  r30,r3
//   0000001C  41 82 00 28  beq  <epilogue>      ; the `if (this)` guard
//   00000020  3C A0 00 00  lis  r5,lbl_23_data_0@ha
//   00000024  38 80 00 00  li   r4,0x0          ; the base destructor's flag
//   00000028  38 05 00 00  addi r0,r5,lbl_23_data_0@l
//   0000002C  90 1E 00 00  stw  r0,0x0(r30)     ; the derived vptr, by hand
//   00000030  48 00 0F A9  bl   __dt__6CActorFv
//   00000034  7F E0 07 35  extsh. r0,r31        ; `if (flag > 0)`, the SHORT flag
//   00000038  40 81 00 0C  ble  <epilogue>
//   0000003C  7F C3 F3 78  mr   r3,r30
//   00000040  48 00 0F 99  bl   Free__7CMemoryFPCv
//   ...                       epilogue, with `mr r3,r30` before `lwz r31` / `lwz r30`
//
// The three load-bearing details are the ones `src/MetroidPrime/CStateManager.cpp` measures on
// the same shape: the flag parameter is a **`short`** (`int` gives `cmpwi r31,0` where retail
// has `extsh. r0,r31`), the return type is a **pointer** (a `void` leaf loses the trailing
// `mr r3,r30`), and the base destructor is called with a literal `0`, which is what puts
// `li r4, 0` between the vtable `lis` and the `addi`.
//
// The vptr store is written by hand rather than left to a `~CFogOverlay(){}` body, for the same
// reason the stand-in class is not the real class: a class whose first non-inline virtual is
// defined here is the translation unit that emits `__vt__`, and an emitted `__data` object the
// split does not claim moves the module's sha1. This is the arrangement
// `MetroidPrime/CConsoleOutputWindowCtor.cpp` already uses for a derived vptr store, and
// `mwcceppc` lays the two halves out the way `~CIOWin` does: the `lis` in one register and the
// `addi` result in another.
CFogOverlay* fn_23_0(CFogOverlay* self, short flag) {
  if (self != nullptr) {
    *reinterpret_cast<void**>(self) = lbl_23_data_0;
    __dt__6CActorFv(self, 0);
    if (flag > 0) {
      CMemory::Free(self);
    }
  }
  return self;
}
}
