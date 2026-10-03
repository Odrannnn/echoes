// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:7975-7976`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_801EF598_text.s:128-167`, and the bodies below
// are the C those bytes are the compilation of.  The byte evidence is the pristine disc, not our
// own build: `python3 tools/dol_read.py 0x801EF730 0x80 orig/G2ME01/sys/main.dol`.
//
// .text 0x801EF730..0x801EF7B0, 0x80 = 128 bytes, 2 functions:
//
//   fn_801EF730    0x801EF730  0x54    21 instructions
//                                       stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1) /
//                                       stw r31,0xc(r1) / mr r31,r4 / stw r30,0x8(r1) /
//                                       mr. r30,r3 / beq / li r4,-1 / bl fn_80004744 /
//                                       extsh. r0,r31 / ble / mr r3,r30 /
//                                       bl Free__7CMemoryFPCv / lwz r0,0x14(r1) /
//                                       mr r3,r30 / lwz r31,0xc(r1) / lwz r30,0x8(r1) /
//                                       mtlr r0 / addi r1,r1,0x10 / blr
//   fn_801EF784    0x801EF784  0x2C    11 instructions
//                                       stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1) /
//                                       stw r31,0xc(r1) / mr r31,r3 / bl fn_801EF7B0 /
//                                       lwz r0,0x14(r1) / lwz r31,0xc(r1) / mtlr r0 /
//                                       addi r1,r1,0x10 / blr
//
// **Both are byte-shape twins of retail functions that are already matched**, which is where the
// bodies below are read from rather than guessed.  Read out of the disc with `tools/dol_read.py`,
// each range is its twin's word for word apart from the `bl` displacements, which are
// address-relative:
//
//   * `fn_801EF730` is `fn_800045A0` (0x800045A0, 0x54, `symbols.txt:77`,
//     `src/MetroidPrime/Carve800045A0.c:195`), the head of a deleting-destructor chain - the same
//     21 instructions, with `48 00 00 31` where this copy has `4b e1 4f f1` and `48 2c 9d b5`
//     where this copy has the same `bl 0x802CE388`.  So the body is the MWCC convention
//     `if (self) { <this copy's own member teardown>; if (flag > 0) Free(self); } return self;`,
//     with this copy's teardown at 0x801EF754.  Two details the bytes fix rather than suggest:
//     the flag is a **short** (`extsh. r0,r31`, not the `cmpwi` an `int` gives) and
//     `if (flag > 0)` sits **inside** `if (self)`, because the receiver's `beq` (`41 82 00 1c`,
//     target 0x801EF768) branches over the flag test straight to the epilogue.
//   * `fn_801EF784` is
//     `GetIObjObjectFor__23TToken<13CSkinnedModel>FRCQ24rstl25auto_ptr<13CSkinnedModel>`
//     (0x80031A00, 0x2C, `symbols.txt:931`,
//     `build/G2ME01/asm/MetroidPrime/Factories/CCharacterFactory.s:2565-2578`) - these same
//     eleven instructions, with this copy's `bl fn_801EF7B0` where the twin has its
//     `GetNewDerivedObject__41TObjOwnerDerivedFromIObj<13CSkinnedModel>FRCQ24rstl25auto_ptr<13CSki`.
//     That is the same shape `src/Kyoto/Animation/CAnimCharacterSet.cpp:124-139` already spells
//     for its own `fn_8028EC50`, **including the `mr r31,r3`**: the return type is the class
//     `rstl::auto_ptr< T >` (8 bytes), so MWCC passes a hidden return slot in r3 and keeps it in
//     the callee-saved r31 across the call, and the object argument moves to r4.  `fn_8028EC50`
//     is therefore the closer twin of the two, not the function named in the seed.
//
// **What `fn_801EF784`'s return slot is, measured off retail's own caller.**  `fn_801EF5FC`
// (0x801EF5FC, 0xA4) sets `addi r3,r1,0x8 / addi r4,r1,0x10` before its `bl fn_801EF784` at
// 0x801EF62C (`build/G2ME01/asm/auto_03_801EF598_text.s:49-53`) and reads `lwz r3,0xc(r1)` after it,
// i.e. the result lands at `r1+8` and the caller takes its **+4** word.  It also writes
// `stb r0,0x10(r1)` *before* the call, which is `obj.mHas = false` at the argument's +0.  So the
// return is the two-word `{ bool mHas; T* mItem; }` that `include/rstl/auto_ptr.hpp:22-24` is,
// it is **8 bytes and not 12**, and it does not overlap the argument slot at `r1+0x10`.  That is
// why this copy's declaration is the real `rstl::auto_ptr` and not a 12-byte C struct: a struct
// that size reproduces the same eleven instructions (measured), but it describes an ABI retail
// does not have and would make the two slots overlap in the caller.
//
// **Who calls them**, measured with `grep -rn 'bl fn_801EF730\|bl fn_801EF784' build/G2ME01/asm/` -
// three call sites, two of them in the unclaimed `auto_03_801EF598_text` this claim sits inside:
//
//   * `fn_801EF784` at 0x801EF62C, inside `fn_801EF5FC`, with the two stack addresses above.
//     `fn_801EF5FC` then reads `0xc(r1)` - what the callee stored at the +4 of the return slot -
//     and tears that object down again through a vtable slot.
//   * `fn_801EF730` at 0x801EF6DC, inside `fn_801EF6A0` (0x801EF6A0, 0x90) with `li r4,0x1` - the
//     deleting flag.  That caller sets `lbl_803B78D0` at the receiver's +0, loads the +4 member and
//     only calls when it is non-zero, so this is the teardown step of the owned pointer's own
//     destructor.
//   * `fn_801EF730` at 0x801EF880, in the **matched** `MetroidPrime/Carve801EF84C.cpp`, whose
//     `.text` starts at 0x801EF84C.  That file already called this symbol by declaring it
//     `extern "C"` and carrying an `#ifndef __MWERKS__` empty stand-in for the host link; this
//     carve supplies the real bytes and the stand-in was **deleted**, because a second definition
//     is the duplicate `tools/gate.sh`'s `port link dups` step exists to catch.
//
// **The callees are declared, never defined here**, and each has a twin whose bytes are the same
// ones here:
//
//   * `fn_80004744` (0x80004744, 0x54, `symbols.txt:81`) is claimed and matched by
//     `MetroidPrime/Carve80004744.c`, which models its receiver as the four-word `SBufferHolder`
//     whose +0xC pointer it frees.  `src/MetroidPrime/Carve800045A0.c:80-83` already records that
//     this shape is that function's with one more step in front.
//   * `Free__7CMemoryFPCv` (0x802CE388, `symbols.txt:12992`) is claimed by
//     `Kyoto/Alloc/CMemory.cpp`; `src/Kyoto/Alloc/PortMwccNew.cpp` defines it for the host.
//   * `fn_801EF7B0` (0x801EF7B0, 0x9C, `symbols.txt:7977`) is **not** claimed by anything: it is
//     the next unsourced function of the same `auto_*` run, and this claim stops at 0x801EF7B0, so
//     the DOL link takes it from dtk's object of the surrounding run.  It is this copy's
//     `TObjOwnerDerivedFromIObj<T>::GetNewDerivedObject` - `li r3,0x8 / bl __nw__FUlPCcPCc`, the
//     three `stw` of the `__vt__4IObj` / `__vt__31CObjOwnerDerivedFromIObjUntyped` /
//     `lbl_803B78D0` vtable, `stb r5,0x0(r31)` releasing the source's +0 flag and `stw r4,0x4(r3)`
//     copying its +4 pointer - which is byte for byte
//     `GetNewDerivedObject__41TObjOwnerDerivedFromIObj<13CSkinnedModel>FRCQ24rstl25auto_ptr<13CSki`
//     at 0x80031A2C (`symbols.txt:932`, `CCharacterFactory.s:2579-2620`, also 0x9C), so its
//     declaration is the same `rstl::auto_ptr` by value.  For the host link it is new, and the
//     announced stand-in at the end of this file is the same trade
//     `src/MetroidPrime/Cameras/Carve801E7C14.c:119-140` makes for `__dt__17CCameraShakerDataFv`.
//     So this unit claims `.text` and nothing else.
//
// **The unit is a `.cpp` with `extern "C"`, not a `.c`, and that is forced by `fn_801EF784`.**
// The seed said plain C "so the fn_ names do not mangle", and the concern is right, but for this
// pair of bodies the sret convention is the whole of the second function: a plain-C function
// returning `void`, or returning an 8-byte POD struct, is **8 instructions** (measured) because
// MWCC hands an 8-byte POD back in r3:r4 and never touches r31, whereas a plain-C function
// returning a **12-byte** struct emits retail's exact eleven - the return type alone decides it.
// Retail returns 8 bytes, so the honest spelling is the class, and only C++ has the class.  The
// `extern "C"` wrapper is what keeps the names verbatim, and it is the arrangement
// `src/MetroidPrime/Carve801EF84C.cpp:99` and
// `src/MetroidPrime/Player/CGameStateBlockCopyCtor.cpp:34` already use for Matching carves of
// exactly this shape.  `fn_801EF730` compiles identically either way - the same 21 instructions
// were measured in C and in C++ - so nothing else in the unit depends on the choice.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function, objdiff still happy,
// `tools/unit_fit.sh` still "fits", the link still succeeding, and a broken DOL sha1.  Only
// `tools/flip_test.sh` catches that.  Check it with
// `python3 tools/check_decl_order.py --unit MetroidPrime/Carve801EF730.cpp`.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges in one section (dtk `dol split` fails with "Cyclic dependency ... link
// order").  The claim starts and ends inside `auto_03_801EF598_text` (0x801EF598..0x801EF84C),
// the function below is `fn_801EF6A0` (0x801EF6A0, 0x90) and the one above is `fn_801EF7B0`
// (0x801EF7B0, 0x9C).  `MetroidPrime/Carve801EF84C.cpp` above this claim begins at 0x801EF84C and
// `MetroidPrime/CActorField25.cpp` below it ends at 0x801ECDCC; the neighbour above is not a
// problem for `dtk dol split` because it is a claim of its own, not an unclaimed gap.
//
// **The directory is retail's own, taken from the nearest claimed ranges**: below is
// `MetroidPrime/CActorField25.cpp` (`.text` 0x801ECD8C..0x801ECDCC) and above is
// `MetroidPrime/Carve801EF84C.cpp` (0x801EF84C..0x801EFA00), so this address sits in the
// `MetroidPrime/` neighbourhood - the same reasoning `src/MetroidPrime/Carve80004744.c:55-59`
// records for the same destructor chain.

#include "rstl/auto_ptr.hpp"

extern "C" {

/** 0x80004744, `symbols.txt:81`, size 0x54: the destructor that frees the pointer at +0xC of its
 *  receiver.  Claimed and matched by `MetroidPrime/Carve80004744.c`, which models the receiver as
 *  its `SBufferHolder`; declared only here. */
extern void* fn_80004744(void* self, short flag);

/** 0x802CE388, `symbols.txt:12992`, size 0x64: `CMemory::Free(void const*)`.  Claimed by
 *  `Kyoto/Alloc/CMemory.cpp`, so our own tree supplies it.  Declared, never defined here.
 *  `src/Kyoto/Alloc/PortMwccNew.cpp` defines it for the host. */
extern void Free__7CMemoryFPCv(const void* ptr);

/** What `fn_801EF7B0` allocates: 8 bytes whose first word is a vtable - `li r3,0x8` before its
 *  `bl __nw__FUlPCcPCc`, then `stw r0,0x0(r3)` three times for
 *  `__vt__4IObj` / `__vt__31CObjOwnerDerivedFromIObjUntyped` / `lbl_803B78D0`.  Only the size is
 *  modelled here; this unit's own bytes never load it. */
struct SOwnedObject {
  void** mVtable;
};

/** 0x801EF7B0, `symbols.txt:7977`, size 0x9C: this copy's
 *  `TObjOwnerDerivedFromIObj<T>::GetNewDerivedObject(const rstl::auto_ptr<T>&)`, byte for byte
 *  `GetNewDerivedObject__41TObjOwnerDerivedFromIObj<13CSkinnedModel>F...` at 0x80031A2C (0x9C) -
 *  hence the same return type, the class `rstl::auto_ptr<T>` by value.  Declared, never defined
 *  here: nothing in the tree claims it, so the DOL link takes it from dtk's own object of the run
 *  this claim was carved out of.  `T` is written out as the vtable-holding word the callee
 *  allocates, because `rstl::auto_ptr<T>` only ever holds a `T*` and nothing else is read. */
extern rstl::auto_ptr< SOwnedObject > fn_801EF7B0(const rstl::auto_ptr< SOwnedObject >& obj);

rstl::auto_ptr< SOwnedObject > fn_801EF784(const rstl::auto_ptr< SOwnedObject >& obj);
void* fn_801EF730(void* self, short flag);

/** `fn_801EF784` - retail `.text:0x801EF784`, 0x2C = 44 bytes, 11 instructions: a frame, the
 *  return slot copied into the callee-saved r31, one call and the epilogue.  The `mr r31,r3` and
 *  its `stw`/`lwz` pair are the sret convention for the class return - see the header - so `r3` is
 *  the caller's result slot and the object argument is already in r4, untouched.  Twin of
 *  `fn_8028EC50` (0x8028EC50, 0x2C, `src/Kyoto/Animation/CAnimCharacterSet.cpp:136`), the same
 *  eleven instructions, and of
 *  `GetIObjObjectFor__23TToken<13CSkinnedModel>F...` (0x80031A00, 0x2C), the same function
 *  retail's symbols.txt names for this shape. */
rstl::auto_ptr< SOwnedObject >
fn_801EF784(const rstl::auto_ptr< SOwnedObject >& obj) {
  return fn_801EF7B0(obj);
}

/** `fn_801EF730` - retail `.text:0x801EF730`, 0x54 = 84 bytes, 21 instructions: a frame, the
 *  receiver guard, `li r4,-1 / bl fn_80004744`, the 16-bit flag test, `Free__7CMemoryFPCv(self)`
 *  and the epilogue that returns the receiver in r3.  Twin of `fn_800045A0` (0x800045A0, 0x54),
 *  the head of the deleting-destructor chain at `src/MetroidPrime/Carve800045A0.c:195`, so the
 *  body below is that chain's shape with this copy's own teardown in place of `fn_800045F4`.  The
 *  flag is a **short** because the test is `extsh.`, and `flag > 0` sits **inside** `if (self)`
 *  because the receiver's `beq` targets the epilogue (0x801EF768, the `lwz r0,0x14(r1)`). */
void* fn_801EF730(void* self, short flag) {
  if (self) {
    fn_80004744(self, -1);
    if (flag > 0) {
      Free__7CMemoryFPCv(self);
    }
  }
  return self;
}

#ifndef __MWERKS__
// Port-only stand-in, empty body, and it **is** one: `fn_801EF7B0`'s own 0x9C retail bytes are a
// spelling job of their own and nothing in this tree has claimed them.  It exists so the host link
// resolves the `bl` above, and it is the same trade
// `src/MetroidPrime/Cameras/Carve801E7C14.c:119-140` makes for `__dt__17CCameraShakerDataFv` and
// `src/MetroidPrime/Carve801EF84C.cpp` made for `fn_801EF730` before this carve took its real
// bytes: an announced stand-in, kept beside the one reference that asks for it.  Before this
// carve nothing in the port referenced the symbol - only dtk's `auto_*` objects did - so it is
// new to the port's link, and `tools/link_gap.py` fails the gate on a missing symbol that is not
// accounted for in `docs/research/port_link_gap_list.md`.  The guard is `__MWERKS__`, not
// `TARGET_PC`, to match those files: the matching build must take the symbol from dtk's own
// object of the surrounding run, and a second definition there would be a duplicate.  It returns
// an empty `rstl::auto_ptr`, which is what its default constructor already holds.
rstl::auto_ptr< SOwnedObject >
fn_801EF7B0(const rstl::auto_ptr< SOwnedObject >& obj) {
  (void)obj;
  return rstl::auto_ptr< SOwnedObject >();
}
#endif

} // extern "C"