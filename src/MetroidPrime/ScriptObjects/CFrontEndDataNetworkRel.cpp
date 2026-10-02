// CFrontEndDataNetworkRel.cpp - ScriptFrontEndDataNetwork's (module 59) second code block,
// `.text 0x1B8..0x3F4`: ten functions, the module's entry-point block last.
//
// Same arrangement as `CAtomicAlphaRel.cpp` (module 2) and `CFlyerSwarmRel.cpp` (module 2's
// sibling), and the ranges come from `config/G2ME01/rels/ScriptFrontEndDataNetwork/symbols.txt`:
//
//   0x1B8  fn_59_1B8  0x20  `rstl::destroy< T >(T*)` - a frame and one call to 0x1D8
//   0x1D8  fn_59_1D8  0x24  `rstl::destroy_impl< T >(T*)` - `fn_59_1FC(self, -1)`
//   0x1FC  fn_59_1FC  0x58  the deleting destructor of T: the member at +0xC, then `Free(self)`
//   0x254  fn_59_254  0x58  the same for a second class, whose member sits at +0x8 (U)
//   0x2AC  fn_59_2AC  0x54  `~rstl::vector< ... >` for U's member: `Free(*(void**)(self + 0xC))`
//   0x300  fn_59_300  0x54  the same destructor again for T's member (two instantiation sites)
//   0x354  fn_59_354  0x2C  vtable entry 0x3C, dispatching to slot 0x38
//   0x380  RELExit    0x24  `fn_8021FA18(0)`, the DOL loader setter with a null loader
//   0x3A4  RELMain    0x20  `fn_59_3C4()`
//   0x3C4  fn_59_3C4  0x30  `lbl_59_bss_4 = fn_59_3F4; fn_8021FA18(&lbl_59_bss_4)`
//
// **The ten are one contiguous run and that is why they are one unit**: the block starts exactly
// where `fn_59_168` ends (0x168 + 0x50 = 0x1B8) and ends exactly where `fn_59_3F4`, the module's
// own entity loader, begins (0x3F4). One unit cannot claim two discontiguous ranges, so the
// neighbours stay with dtk and the claim spans no unclaimed gap.
//
// **Every one of the ten is referenced, so none of them is a dead-stripping hazard** - measured
// with `powerpc-eabi-objdump -r` over the module's `auto_*` objects before the claim existed:
// `fn_59_1B8` from `fn_59_168` (0x18C) and `fn_59_6E28` (0x6E4C), `fn_59_254` from `fn_59_0`
// four times (0x40, 0x4C, 0x58, 0x64) and from the class code above 0xAC4 eight more times
// (0x8AC, 0x8E4, 0x91C, 0x954, 0xC84, 0xC90, 0xC9C, 0xCA8), `fn_59_1FC` from `fn_59_1D8` and
// from two callers in that class code (0x52E0, 0x55D8), `fn_59_354` from the `.data` vtable
// `lbl_59_data_8` at +0x3C, and `RELExit`/`RELMain` from the `.text` tail's `_epilog` (0x7394)
// and `_prolog` (0x73B8). The module's own `force_active:` list does not need to grow.
//
// The bodies are the twins' bodies, not guesses. `src/MetroidPrime/Factories/Carve80032774.cpp`
// is a `Matching` unit at 100.00% per function and carries the same shapes at the same sizes:
// its `fn_80032854` (0x54) frees the pointer at +0xC of its receiver and then the receiver, which
// is `fn_59_2AC` and `fn_59_300` below word for word; its `__dt__11CMayaSplineFv` (0x58) destroys
// the member at +0x8 first, which is `fn_59_254` below with this copy's callee; and `fn_59_1FC`
// is that same 0x58-byte spelling with the member at +0xC, because the two classes' members are
// declared in the other order. The two `destroy` halves are
// `src/MetroidPrime/ScriptObjects/Carve801FD638.c`'s `fn_801FD638`/`fn_801FD658` pair verbatim.
// The entry block is `CAtomicAlphaRel.cpp`'s, token for token, with this module's names.
//
// What is *not* copied blindly:
//   - the `-1` flag is a `short` in the destructors: retail's `if (flag > 0)` is
//     `extsh. r0,r31 / ble`, which is the halfword sign-extend, so the parameter is not `int`.
//   - `fn_59_354` is a vtable entry, so it is written as a member call on a class with thirteen
//     virtuals - the same spelling `CAtomicAlphaRel.cpp` measured for the same seven
//     instructions (retail's `lwz r12,0(r3) / lwz r12,0x38(r12) / mtctr / bctrl`). Loading the
//     vtable by hand gives `lwz r3,0(r3)` and 99.09% there.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100%. `tools/check_decl_order.py --unit
// CFrontEndDataNetworkRel` is the cheap check.

#include "MetroidPrime/ScriptLoader.hpp"
#include "REL/REL_Setup.h"

// The DOL's 8-byte loader setter at 0x8021FA18 - `stw r3, gLoader_FrontEndDataNetwork; blr`
// (`build/G2ME01/asm/auto_03_8021FA18_text.s`) - which this module imports by that name. It is
// declared and never defined here for the reason
// `src/MetroidPrime/ScriptLoader/FrontEndDataNetwork.cpp` records: retail's DOL symbol table
// names it `fn_8021FA18`, so the name cannot be invented and must be reproduced.
extern "C" void fn_8021FA18(FScriptLoader* loader);

// The DOL's `CMemory::Free(void const*)` (0x802CE388 of `Kyoto/Alloc/CMemory.cpp`, which is
// `Matching`). Declared rather than reached through the header so the symbol is spelled exactly
// as the DOL's, the same way `Carve80032774.cpp` declares it - this module's other functions
// already `bl` it, so the import exists in retail's own relocations.
extern "C" void Free__7CMemoryFPCv(const void* ptr);

// The module's own entity loader, retail 0x3F4 (0x6D0 bytes) - the function `fn_59_3C4` stores
// into the loader slot. Outside this claim and unclaimed, so it stays retail's: dtk's `auto_*`
// object defines it. Declared, never defined.
extern "C" CEntity* fn_59_3F4(CStateManager& mgr, CInputStream& input, CEntityInfo& info);

// Vtable entry 0x3C of this module (`lbl_59_data_8`, `.data:0x8`) calls whatever sits in slot
// 0x38, which the same table names `HealthInfo__6CActorFv`. The class has no header in this tree,
// so it is modelled by position: two leading words (offset-to-top and RTTI, both zero in a REL)
// and thirteen virtuals put the last one at 0x38. Nothing here defines or calls a slot, and the
// only object carrying this vtable is the module's own retail `.data`.
class CFrontEndDataNetworkDispatch {
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
  virtual float Slot12();
};

extern "C" {
// `.bss:0x4`, `size:0x4 data:4byte`: this module's copy of the loader pointer, which dtk's
// `auto_05_00000000_bss` object defines because this unit's split claims `.text` only. A second
// definition under MWCC is what produced mwldeppc's internal linker error on ScriptPlayerProxy,
// hence `extern` under MWCC and a host definition (`CAtomicAlphaRel.cpp` records the same).
#ifdef __MWERKS__
extern FScriptLoader lbl_59_bss_4;
#else
FScriptLoader lbl_59_bss_4 = 0;
#endif

// .text 0x3C4, 0x30 bytes. `SetRelLoaderFunctionToLoader`: store the module's loader into the
// slot, then hand the *address* of the slot to the DOL's setter, which is what
// `LoadFrontEndDataNetwork` (0x8021F9EC, `src/MetroidPrime/ScriptLoader/FrontEndDataNetwork.cpp`)
// reads back at load time.
void fn_59_3C4() {
  lbl_59_bss_4 = fn_59_3F4;
  fn_8021FA18(&lbl_59_bss_4);
}

// .text 0x3A4, 0x20 bytes. The module's entry point; `_prolog` in the `.text` tail calls it.
void RELMain() { fn_59_3C4(); }

// .text 0x380, 0x24 bytes. The module's exit point, which `_epilog` calls.
void RELExit() { fn_8021FA18(0); }

// .text 0x354, 0x2C bytes. Vtable entry 0x3C; the slot it reaches is 0x38 - see the class above.
void fn_59_354(CFrontEndDataNetworkDispatch* self) { self->Slot12(); }

// .text 0x300, 0x54 bytes. The same destructor as 0x2AC, instantiated a second time: U's member
// and T's member are different `vector`s, so retail carries two copies.
void* fn_59_300(void* self, short flag) {
  if (self) {
    Free__7CMemoryFPCv(*(const void**)((char*)self + 0xC));
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

// .text 0x2AC, 0x54 bytes. The member's own destructor: `mItems` - the pointer at +0xC, which is
// where `include/rstl/vector.hpp`'s `T* mItems` sits behind `mAllocator`/`mCount`/`mCapacity` -
// is released, then the receiver itself behind the deleting flag. There is **no**
// `destroy(begin(), end())` loop, so the element type is trivially destructible: the whole body
// is the deallocate and the deleting-destructor tail.
void* fn_59_2AC(void* self, short flag) {
  if (self) {
    Free__7CMemoryFPCv(*(const void**)((char*)self + 0xC));
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

// .text 0x254, 0x58 bytes. The deleting destructor of the class whose member sits at +0x8.
void* fn_59_254(void* self, short flag) {
  if (self) {
    fn_59_2AC((char*)self + 0x8, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

// .text 0x1FC, 0x58 bytes. The deleting destructor of the class whose member sits at +0xC - the
// element `rstl::destroy`/`destroy_impl` below are instantiated for.
void* fn_59_1FC(void* self, short flag) {
  if (self) {
    fn_59_300((char*)self + 0xC, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

// .text 0x1D8, 0x24 bytes. `rstl::destroy_impl< T >(T*)` - `in->~T()` with the "destroy, do not
// free me" flag, i.e. `include/rstl/construct.hpp`'s body.
void fn_59_1D8(void* self) { fn_59_1FC(self, -1); }

// .text 0x1B8, 0x20 bytes. `rstl::destroy< T >(T*)` - the frame and the one call that
// `include/rstl/construct.hpp` gives the forwarder.
void fn_59_1B8(void* self) { fn_59_1D8(self); }
}
