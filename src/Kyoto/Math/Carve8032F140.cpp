// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses
// and sizes come from `config/G2ME01/symbols.txt:14956-14957`, the instructions are the ones dtk
// itself emitted into `build/G2ME01/asm/auto_03_8032EE68_text.s:225-288`, and the bodies below are
// the C those bytes are the compilation of.  The byte evidence is the pristine disc, not our own
// build: `python3 tools/dol_read.py 0x8032F140 0xDC orig/G2ME01/sys/main.dol`.
//
// .text 0x8032F140..0x8032F21C, 0xDC = 220 bytes, 2 functions:
//
//   fn_8032F140    0x8032F140  0xB0    44 instructions
//                                       stwu r1,-0x20(r1) / mflr r0 / stw r0,0x24(r1) /
//                                       neg r0,r4 / or r0,r0,r4 / stw r31,0x1c(r1) /
//                                       srwi r0,r0,31 / mr r31,r3 / addi r3,r1,8 /
//                                       stw r4,0x14(r1) / addi r4,r1,0x10 / stb r0,0x10(r1) /
//                                       bl fn_8032F1F0 / lwz r3,0xc(r1) / li r0,0 / stb r0,8(r1) /
//                                       neg r0,r3 / or r0,r0,r3 / srwi r0,r0,31 / stb r0,0(r31) /
//                                       stw r3,4(r31) / lbz r0,8(r1) / cmplwi r0,0 / beq /
//                                       lwz r3,0xc(r1) / cmplwi r3,0 / beq / lwz r12,0(r3) /
//                                       li r4,1 / lwz r12,8(r12) / mtctr r12 / bctrl /
//                                       lbz r0,0x10(r1) / cmplwi r0,0 / beq / lwz r3,0x14(r1) /
//                                       li r4,1 / bl fn_8032F2B8 / lwz r0,0x24(r1) / mr r3,r31 /
//                                       lwz r31,0x1c(r1) / mtlr r0 / addi r1,r1,0x20 / blr
//   fn_8032F1F0    0x8032F1F0  0x2C    11 instructions
//                                       stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1) /
//                                       stw r31,0xc(r1) / mr r31,r3 / bl fn_8032F21C /
//                                       lwz r0,0x14(r1) / lwz r31,0xc(r1) / mtlr r0 /
//                                       addi r1,r1,0x10 / blr
//
// **Both are byte-shape twins of retail functions that are already matched**, which is where the
// bodies below are read from rather than guessed.  `./tools/dis.sh` on both pairs gives the same
// instructions in the same order, the only differences being the `bl` displacements, which are
// address-relative:
//
//   * `fn_8032F140` is `__ct<13CSkinnedModel>__16CFactoryFnReturnFP13CSkinnedModel` (`.text
//     0x80031950`, `size:0xB0`, `symbols.txt:930`), whose source is the one member initialiser in
//     `include/Kyoto/CFactoryMgr.hpp:19`:
//     `CFactoryFnReturn(T* ptr) : obj(TToken< T >::GetIObjObjectFor(ptr).release()) {}`.
//     `src/Kyoto/Animation/CAnimCharacterSet.cpp:428-434` has the same function already written
//     out and **Matching at 100.00%** (`build/report.json`, unit
//     `main/Kyoto/Animation/CAnimCharacterSet`), 0xA4 bytes: retail's 0xB0 differs from it by
//     exactly three instructions at the tail, which is the one thing that differs in the source.
//     Retail's tail is **guarded and passing the owned pointer**, not unconditional and passing
//     the `auto_ptr`:
//
//         0x8032f1c0  lbz r0,0x10(r1) / cmplwi r0,0 / beq      ; if (local.mHas)
//         0x8032f1cc  lwz r3,0x14(r1) / li r4,1 / bl fn_8032F2B8
//
//     where `fn_8028E820`'s is `addi r3,r1,16 / li r4,-1 / bl fn_8028ED18` - `this` = the
//     `auto_ptr` and the flag *not* deleting, because `~auto_ptr<CAnimCharacterSet>` was emitted
//     out of line there (`src/Kyoto/Animation/CAnimCharacterSet.cpp:73-95`).  Here the temporary's
//     teardown is **inlined** and hands the raw pointer to a deleting call, which is retail's
//     `rstl::auto_ptr< T >::~auto_ptr()` - `if (mHas) delete mItem;` - with `~T` reached as the
//     out-of-line **deleting destructor** `fn_8032F2B8`.  So the outer temporary is spelled with a
//     trivial destructor and the call written out; see `SOwnedPtr` below.
//   * `fn_8032F1F0` is `GetIObjObjectFor__23TToken<13CSkinnedModel>FRCQ24rstl25auto_ptr<13CSki`
//     (0x80031A00, 0x2C, `symbols.txt:931`,
//     `build/G2ME01/asm/MetroidPrime/Factories/CCharacterFactory.s:2564-2578`), i.e.
//     `return GetNewDerivedObject(obj);`.  It is the same eleven instructions, and the same
//     spelling `src/MetroidPrime/Carve801EF730.cpp:175-178` already uses for its own 0x2C-byte
//     copy `fn_801EF784` (also `Matching` 100.00%).
//
// **The `mtctr`/`bctrl` pair in the middle of `fn_8032F140` is the *other* temporary**, the
// `rstl::auto_ptr< IObj >` that `fn_8032F1F0` returns.  It is destroyed by
// `rstl::auto_ptr< IObj >::~auto_ptr()` (`include/rstl/auto_ptr.hpp:21-25`) = `if (mHas) delete
// mItem;`, and because `mItem` is an `IObj*` whose destructor is **virtual**
// (`include/Kyoto/IObj.hpp:14`), `delete` compiles to retail's exact nine instructions: the null
// test on `mItem` (0x8032F1A0-0x8032F1A8), the vtable load `lwz r12,0(r3)`, the deleting flag
// `li r4,1`, the slot load `lwz r12,8(r12)` and the indirect call.  `CFactoryFnReturn` holds
// nothing else (`include/Kyoto/CFactoryMgr.hpp:24`), so `self` is that member and the two stores
// at 0x8032F18C/0x8032F190 are `rstl::auto_ptr`'s own public fields.
//
// **The member stores must stay in the same full expression that built the temporaries.**  That
// is measured, not guessed: `src/Kyoto/Animation/CAnimCharacterSet.cpp:416-423` records both
// orderings.  Putting `IObj* p = ...release(); obj->mHas = p != nullptr; obj->mItem = p;` in
// separate statements puts both stores *after* the teardown and scores 46.32%, and mwceppc then
// folds the released `mHas` and drops the `lbz`/`cmplwi`/`beq` over the inner destructor entirely.
// Written as the comma expression below it is 100.00%, and the `mHas` test survives.
//
// **The callees are declared, never defined here**:
//
//   * `fn_8032F21C` (0x8032F21C, 0x9C, `symbols.txt:14958`) is **not** claimed by anything: it is
//     the next unsourced function of the same `auto_*` run and this claim stops at 0x8032F21C, so
//     the DOL link takes it from dtk's object of the surrounding run.  It is this copy's
//     `TObjOwnerDerivedFromIObj<T>::GetNewDerivedObject(const rstl::auto_ptr<T>&)` - `li r3,0x8 /
//     bl __nw__FUlPCcPCc` with the placement string `lbl_803B0100`, then the three `stw` of the
//     `__vt__4IObj` / `__vt__31CObjOwnerDerivedFromIObjUntyped` / `lbl_803BB3F8` vtable and the
//     `stb`/`stw` pair that releases the source's flag and copies its +4 - which is byte for byte
//     `GetNewDerivedObject__41TObjOwnerDerivedFromIObj<13CSkinnedModel>FRCQ24rstl25auto_ptr<13CSki`
//     at 0x80031A2C (`symbols.txt:932`, also 0x9C), so its declaration is the same `rstl::auto_ptr`
//     by value.  For the host link it is new and the announced stand-in at the end of this file
//     is the same trade `src/MetroidPrime/Carve801EF730.cpp:197-215` makes for `fn_801EF7B0`.
//   * `fn_8032F2B8` (0x8032F2B8, 0x58, `symbols.txt:14959`) **is** claimed, and `Matching` 100%,
//     by `Kyoto/Math/Carve8032F2B8.c`, the unit immediately above this claim.  Declared, never
//     defined here: a second definition is the duplicate `tools/gate.sh`'s `port link dups` step
//     exists to catch.  Its own `symbols.txt` twin is
//     `__dt__Q24rstl32single_ptr<18CGameGlobalObjects>Fv` (0x80006AE0, 0x58), the same 22
//     instructions, which is why the flag here is `1` and not `-1`.
//
// **The unit is a `.cpp` with `extern "C"`, not a `.c`, and that is forced by `fn_8032F1F0`.**  The
// seed said plain C "so the fn_ names do not mangle", and the concern is right, but for this pair
// of bodies the sret convention is the whole of the second function: retail returns a **class**,
// so MWCC passes a hidden result slot in r3 and keeps it in the callee-saved r31 across the call
// (`mr r31,r3`).  A plain-C function returning an 8-byte POD is handed back in r3:r4 and never
// touches r31 - 8 instructions, not retail's 11 (measured, and recorded at
// `src/MetroidPrime/Carve801EF730.cpp:99-110`).  The `extern "C"` wrapper is what keeps the names
// verbatim, and it is the arrangement `src/MetroidPrime/Carve801EF730.cpp` already uses for a
// Matching carve of exactly this shape.  `fn_8032F140` needs it too: it returns `self`.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function, objdiff still happy,
// `tools/unit_fit.sh` still "fits", the link still succeeding, and a broken DOL sha1.  Only
// `tools/flip_test.sh` catches that.  Check it with
// `python3 tools/check_decl_order.py --unit Kyoto/Math/Carve8032F140.cpp`.
//
// **`tools/unit_fit.sh` reports this unit 192 bytes over its claim, and that is expected.**  The
// raw mwceppc object `build/G2ME01/src/Kyoto/Math/Carve8032F140.o` also carries the weak COMDAT
// copies `__dt__4IObjFv` (72 bytes) and `__dt__Q24rstl15auto_ptr<4IObj>Fv` (120 bytes) plus a
// 12-byte `.data` `__vt__4IObj`, all three emitted because `delete mItem` needs `IObj`'s
// out-of-line destructor and a vtable for it.  mwldeppc's split drops them: the object that goes
// into the link, `build/G2ME01/obj/Kyoto/Math/Carve8032F140.o`, is **0xDC = 220 bytes and holds
// exactly `fn_8032F140` and `fn_8032F1F0`** (measured with `powerpc-eabi-nm`), which is the
// "harmless causes first" case `unit_fit.sh` describes.  `flip_test` decides and passed.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges in one section (dtk `dol split` fails with "Cyclic dependency ... link
// order").  The claim starts and ends inside `auto_03_8032EE68_text` (0x8032EE68..0x8032F2B8,
// size 0x450): the function below is
// `FSortedParticleSystemDataFactory__FRC10SObjectTagR12CInputStreamRC15CVParamTransfer`
// (0x8032F0D4, 0x6C, `symbols.txt:14955`) and the one above is `fn_8032F21C` (0x9C).  The
// neighbour above is not a problem for `dtk dol split`: it is inside the same unclaimed run, not
// a claim of its own.  `Kyoto/Math/Carve8032F2B8.c` begins at 0x8032F2B8, exactly where that run
// ends.
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `Kyoto/Math/Carve8032E444.c` (`.text` 0x8032E444..0x8032E44C) and above is
// `Kyoto/Math/Carve8032F2B8.c` (0x8032F2B8..0x8032F31C), so this address sits in the
// `Kyoto/Math` neighbourhood - the same reasoning `src/Kyoto/Math/Carve8032E444.c` records.

#include "Kyoto/IObj.hpp"

#include "rstl/auto_ptr.hpp"

extern "C" {

/** The owned pointer retail builds at 0x10(r1) out of its own second argument, i.e. the
 *  `rstl::auto_ptr< T >` that `TToken< T >::GetIObjObjectFor(ptr)` materialises from the raw
 *  pointer its caller was given: `bool mHas` at +0 and the pointer at +4, 8 bytes in all
 *  (`include/rstl/auto_ptr.hpp:15-20`).  It is spelled as a class of its own, and the destructor
 *  is left trivial **on purpose**: retail tears this temporary down with `fn_8032F2B8`, reached
 *  as a *direct* `bl` on the owned pointer (0x8032F1CC-0x8032F1D4), whereas
 *  `rstl::auto_ptr< T >::~auto_ptr()` would emit `delete mItem`, and a `delete` on `T*` that the
 *  compiler cannot devirtualise is not that call.  The constructor is `rstl::auto_ptr`'s, member
 *  for member, because the order of the three stores is what fixes 0x8032F14C-0x8032F16C. */
class SOwnedPtr {
public:
  bool mHas;
  void* mItem;
  SOwnedPtr(void* ptr) : mHas(ptr != nullptr), mItem(ptr) {}
};

/** 0x8032F21C, `symbols.txt:14958`, size 0x9C: this copy's
 *  `TObjOwnerDerivedFromIObj<T>::GetNewDerivedObject(const rstl::auto_ptr<T>&)`.  Declared, never
 *  defined here - nothing claims it, so the DOL link takes it from dtk's own object of the run
 *  this claim was carved out of, and the host link gets the announced stand-in below. */
extern rstl::auto_ptr< IObj > fn_8032F21C(const SOwnedPtr& obj);

/** 0x8032F2B8, `symbols.txt:14959`, size 0x58: the deleting destructor retail's `delete mItem`
 *  reaches, claimed and matched by `Kyoto/Math/Carve8032F2B8.c` - the unit immediately above this
 *  claim.  Declared only; a second definition here would be the duplicate
 *  `tools/gate.sh`'s `port link dups` step exists to catch. */
extern void* fn_8032F2B8(void* self, short flag);

/** `fn_8032F1F0` - retail `.text:0x8032F1F0`, 0x2C = 44 bytes, 11 instructions: a frame, the
 *  result slot copied into the callee-saved r31, one call and the epilogue.  The `mr r31,r3` and
 *  its `stw`/`lwz` pair are the sret convention for the class return, so `r3` is the caller's
 *  result slot and the object argument is already in r4, untouched.  Twin of
 *  `GetIObjObjectFor__23TToken<13CSkinnedModel>F...` (0x80031A00, 0x2C,
 *  `build/G2ME01/asm/MetroidPrime/Factories/CCharacterFactory.s:2564-2578`) and of
 *  `fn_801EF784` (0x801EF784, 0x2C, `src/MetroidPrime/Carve801EF730.cpp:175`), which is the same
 *  eleven instructions with the same class return. */
rstl::auto_ptr< IObj > fn_8032F1F0(const SOwnedPtr& obj) { return fn_8032F21C(obj); }

/** `fn_8032F140` - retail `.text:0x8032F140`, 0xB0 = 176 bytes, 44 instructions.  Twin of
 *  `__ct<13CSkinnedModel>__16CFactoryFnReturnFP13CSkinnedModel` (0x80031950, 0xB0,
 *  `symbols.txt:930`), i.e. `CFactoryFnReturn<T>::CFactoryFnReturn(T* ptr)` with the member
 *  initialiser of `include/Kyoto/CFactoryMgr.hpp:19`.  `CFactoryFnReturn` is nothing but that one
 *  `rstl::auto_ptr< IObj >`, so `self` is the member and the two stores are `auto_ptr`'s own
 *  public fields; the constructor returns `this`, which is the `mr r3,r31` at 0x8032F1DC. */
void* fn_8032F140(void* self, void* ptr) {
  SOwnedPtr local(ptr);
  rstl::auto_ptr< IObj >* obj = static_cast< rstl::auto_ptr< IObj >* >(self);
  IObj* p;
  obj->mItem = (obj->mHas = (p = fn_8032F1F0(local).release()) != nullptr, p);
  if (local.mHas) {
    fn_8032F2B8(local.mItem, 1);
  }
  return self;
}

#ifndef __MWERKS__
// Port-only stand-in, empty body, and it **is** one: `fn_8032F21C`'s own 0x9C retail bytes are a
// spelling job of their own and nothing in this tree has claimed them.  It exists so the host
// link resolves the `bl` in `fn_8032F1F0`, and it is the same trade
// `src/MetroidPrime/Carve801EF730.cpp:197-215` makes for `fn_801EF7B0` and
// `src/MetroidPrime/Cameras/Carve801E7C14.c:119-140` for `__dt__17CCameraShakerDataFv`: an
// announced stand-in, kept beside the one reference that asks for it.  Before this carve nothing
// in the port referenced the symbol - only dtk's `auto_*` objects did - so it is new to the port's
// link and `tools/link_gap.py` fails the gate on a missing symbol that is not accounted for in
// `docs/research/port_link_gap_list.md`.  The guard is `__MWERKS__`, not `TARGET_PC`, to match
// those files: the matching build must take the symbol from dtk's own object of the surrounding
// run, and a second definition there would be a duplicate.  It returns an empty
// `rstl::auto_ptr`, which is what its default constructor already holds.
rstl::auto_ptr< IObj > fn_8032F21C(const SOwnedPtr& obj) {
  (void)obj;
  return rstl::auto_ptr< IObj >();
}
#endif

} // extern "C"
