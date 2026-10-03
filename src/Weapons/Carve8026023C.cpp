// Carved out of an unclaimed dtk `auto_*` range by lane 3.  Every number here is measured: the
// addresses and sizes come from `config/G2ME01/symbols.txt`, the instructions are the ones
// `build/G2ME01/main.elf` holds, and the bodies below are the C those bytes are the compilation
// of.
//
// .text 0x8026023C..0x8026030C, 0xD0 = 208 bytes, 2 functions:
//
//   fn_802602E0    0x802602E0  0x2C    11 instructions
//                                       stwu r1,-0x10(r1) / mflr r0 / stw r0,0x14(r1) /
//                                       stw r31,0xc(r1) / mr r31,r3 / bl fn_8026030C /
//                                       lwz r0,0x14(r1) / lwz r31,0xc(r1) / mtlr r0 /
//                                       addi r1,r1,0x10 / blr
//   fn_8026023C    0x8026023C  0xA4    41 instructions
//                                       stwu r1,-0x20(r1) / mflr r0 / stw r0,0x24(r1) /
//                                       neg r0,r4 / or r0,r0,r4 / stw r31,0x1c(r1) /
//                                       srwi r0,r0,31 / mr r31,r3 / addi r3,r1,0x8 /
//                                       stw r4,0x14(r1) / addi r4,r1,0x10 / stb r0,0x10(r1) /
//                                       bl fn_802602E0 / lwz r3,0xc(r1) / li r0,0 /
//                                       stb r0,0x8(r1) / neg r0,r3 / or r0,r0,r3 /
//                                       srwi r0,r0,31 / stb r0,0x0(r31) / stw r3,0x4(r31) /
//                                       lbz r0,0x8(r1) / cmplwi r0,0 / beq +0x80 /
//                                       lwz r3,0xc(r1) / cmplwi r3,0 / beq +0x80 /
//                                       lwz r12,0x0(r3) / li r4,0x1 / lwz r12,0x8(r12) /
//                                       mtctr r12 / bctrl /
//                                       addi r3,r1,0x10 / li r4,-0x1 / bl fn_802603A8 /
//                                       lwz r0,0x24(r1) / mr r3,r31 / lwz r31,0x1c(r1) /
//                                       mtlr r0 / addi r1,r1,0x20 / blr
//
// **Both are byte-shape twins of retail functions this tree already has matched**, which is where
// the bodies below are read from rather than guessed.  Read out of `main.elf`, each range is its
// twin's word for word apart from the `bl` displacements, which are address-relative:
//
//   * `fn_8026023C` is `__ct<20CScannableObjectInfo>__16CFactoryFnReturnFP20CScannableObjectInfo`
//     (0x80110B90, 0xA4, `symbols.txt:4757`,
//     `build/G2ME01/asm/MetroidPrime/Factories/CScannableObjectInfo.s:44-89`) - the same 41
//     instructions, with `48 00 00 75` where the twin has its `48 00 01 05` (the call to
//     `GetIObjObjectFor__30TToken<20CScannableObjectInfo>FRCQ24rstl32auto_ptr<...>` at
//     0x80110CC4) and `48 00 00 e5` where it has `48 00 01 75` (the call to
//     `__dt__Q24rstl32auto_ptr<20CScannableObjectInfo>Fv` at 0x80110D8C).  That is
//     `CFactoryFnReturn<T>::CFactoryFnReturn(T* ptr)`, the source being
//     `include/Kyoto/CFactoryMgr.hpp:19`, and four details are fixed by the bytes rather than
//     suggested.  (1) The receiver is **r3 and is returned**: the twin's `mr r31,r3` at 0x80110BAC
//     and `mr r3,r31` at 0x80110C20 are the sret convention for a constructor, so the member
//     stores are `stb r0,0x0(r31)` / `stw r3,0x4(r31)` and `self` must come back in r3.
//     (2) The `neg r0,r4 / or r0,r0,r4 / srwi r0,r0,31` at 0x80260248-0x80260254 is the
//     *argument* temporary's `mHas = (ptr != nullptr)`, and the `stw r4,0x14(r1)` beside it is
//     that temporary's `mItem` - so the argument is an `rstl::auto_ptr<T>` built from a raw
//     pointer, which is the header's `auto_ptr(T* ptr) : mHas(ptr != nullptr), mItem(ptr)`.
//     (3) The `lbz r0,0x8(r1)` at 0x80260290 reloads the flag `release()` wrote as 0 twelve
//     instructions earlier and the branch at 0x80260298 is **not folded away**, so the *result*
//     temporary's teardown is a real destructor that mwcceppc inlines and does not re-optimise;
//     it is the only teardown retail inlines.  (4) The argument temporary is torn down by the
//     **out-of-line** `bl fn_802603A8` at 0x802602C4, with `li r4,-0x1` in r4, so its destructor is
//     a *different function* from the result temporary's even though both bodies are
//     `if (mHas) delete mItem`.  Modelling the two temporaries as one type collapses that
//     distinction, and costs 0x34 bytes and a wrong tail.
//   * `fn_802602E0` is
//     `GetIObjObjectFor__23TToken<13CSkinnedModel>FRCQ24rstl25auto_ptr<13CSkinnedModel>`
//     (0x80031A00, 0x2C, `symbols.txt:931`,
//     `build/G2ME01/asm/MetroidPrime/Factories/CCharacterFactory.s:2565-2578`) - these same
//     eleven instructions, with this copy's `bl fn_8026030C` where the twin has its
//     `GetNewDerivedObject__41TObjOwnerDerivedFromIObj<13CSkinnedModel>F...`.  So
//     `fn_802602E0` is `TToken<T>::GetIObjObjectFor(const rstl::auto_ptr<T>&)`, whose source is
//     `include/Kyoto/TToken.hpp:24-26`, and **the return type decides the whole function**: the
//     return is the class `rstl::auto_ptr<T>`, which MWCC hands back through a hidden pointer in
//     r3, so the caller's result slot is `addi r3,r1,0x8` and this function keeps it in r31 across
//     its one call.  A plain-C function returning `void`, or an 8-byte POD struct, is 8
//     instructions and never touches r31 (measured); that is the same measurement
//     `src/MetroidPrime/Carve801EF730.cpp:99-110` records for the same shape, and it is why this
//     unit is a `.cpp` carrying `extern "C"` rather than the `.c` the seed suggested.  The
//     `extern "C"` wrapper is what keeps the names verbatim: a C++ definition would mangle to
//     `_Z<len>fn_<addr>v` and objdiff would pair nothing.
//
// **What the callees are, measured:**
//
//   * `fn_8026030C` (0x8026030C, 0x9C, `symbols.txt:10678`) is **not claimed by anything**: it is
//     the next unsourced function of the same `auto_*` run and this claim stops at 0x8026030C,
//     so the DOL link takes it from dtk's object of the surrounding run.  It is this copy's
//     `TObjOwnerDerivedFromIObj<T>::GetNewDerivedObject(const rstl::auto_ptr<T>&)`: `li r3,0x8`
//     before its `bl __nw__FUlPCcPCc`, the three `stw` of `__vt__4IObj` /
//     `__vt__31CObjOwnerDerivedFromIObjUntyped` / `lbl_803B7BFC`, and the `stb r5,0x0(r31)` /
//     `stw r4,0x4(r3)` that steal the source's flag and pointer - byte for byte
//     `GetNewDerivedObject__48TObjOwnerDerivedFromIObj<20CScannableObjectInfo>F...` at 0x80110C34
//     (0x9C, `CScannableObjectInfo.s:91`).  Only the declared `rstl::auto_ptr<T>` by value is
//     modelled here; the allocation itself is that callee's.
//   * `fn_802603A8` (0x802603A8, 0x64, `symbols.txt:10679`) is likewise unclaimed and likewise
//     taken from dtk's object.  It is this copy's `rstl::auto_ptr<T>::~auto_ptr(int flag)`:
//     `mr r31,r4` keeps the flag in a callee-saved register, `mr. r30,r3` is the null-`this` test,
//     the `lbz`/`cmplwi`/`lwz`/`li r4,1` block is the virtual delete, and the trailing
//     `extsh. r0,r31 / ble / bl Free__7CMemoryFPCv` is the "was this the deleting destructor?"
//     test.  That is byte for byte `__dt__Q24rstl32auto_ptr<20CScannableObjectInfo>Fv` at
//     0x80110D8C (0x64), which is also why retail's call passes `-1` and not `1`: the argument
//     temporary is being destroyed, not deleted.  Declared, never defined here; only its
//     signature matters to this unit's own bytes.
//
// **Who calls them**, measured with
// `grep -n 'bl fn_8026023C\|bl fn_802602E0' build/G2ME01/asm/auto_03_8025F468_text.s` - both call
// sites are in the same unclaimed run this claim sits inside:
//
//   * `fn_8026023C` at 0x8026021C, inside
//     `FDecalDataFactory__FRC10SObjectTagR12CInputStreamRC15CVParamTransfer` (0x802601D0, 0x6C),
//     with `mr r3,r31` for the caller's own sret slot and `mr r4,r0` for the object it just built.
//     Nothing is copied back afterwards: the constructor writes through the caller's slot, which
//     is what the `mr r3,r31` at the end of this copy reproduces.
//   * `fn_802602E0` at 0x8026026C, inside `fn_8026023C` itself.
//
// **The directory is retail's own, taken from the nearest claimed ranges**: below is
// `Weapons/CCollisionResponseData.cpp` (`.text` 0x8025DD1C..0x8025F41C) and above is
// `Weapons/CDecal.cpp` (0x8026041C..0x8026258C), so this address sits in the `Weapons/`
// neighbourhood - the same reasoning `src/MetroidPrime/Carve80004744.c:55-59` records for the same
// destructor chain.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object's `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function, objdiff still happy,
// `tools/unit_fit.sh` still "fits", the link still succeeding, and a broken DOL sha1.  Only
// `tools/flip_test.sh` catches it.  Check it with
// `python3 tools/check_decl_order.py --unit Weapons/Carve8026023C.cpp`.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges in one section (dtk `dol split` fails with "Cyclic dependency ... link
// order").  The claim starts and ends inside `auto_03_8025F468_text` (0x8025F468..0x80260414): the
// function below is `fn_802601D0` (0x802601D0, 0x6C), which ends exactly at 0x8026023C, and the one
// above is `fn_8026030C` (0x8026030C, 0x9C).  So this claim spans no gap at all - it is a whole run
// of adjacent functions.  What *does* break `dtk dol split` is a claim starting exactly where
// another unit's `.text` ends, which this one does not.

#include "rstl/auto_ptr.hpp"

extern "C" {

/** What both functions hand back and take: an object whose address is in r3, whose second vtable
 *  slot is the deleting destructor `fn_8026023C` dispatches through (`lwz r12,0x0(r3)` then
 *  `lwz r12,0x8(r12)`, `build/G2ME01/main.elf`), and which `fn_8026030C` allocates as 8 bytes.
 *  Declared, never defined here: nothing in this unit constructs one, and `rstl::auto_ptr<T>`
 *  only ever holds a `T*` and reads nothing else of it. */
struct SOwnedObject {
  virtual ~SOwnedObject();
};

/** the ARGUMENT temporary: what `rstl::auto_ptr<T>` is - a `bool` and a pointer - whose teardown
 *  retail calls out of line as `fn_802603A8`.  It is a class rather than a POD for that one
 *  reason: mwcceppc destroys a class temporary through its destructor, and retail's only out of
 *  line teardown in this function is exactly that.  The destructor is declared `inline` so that
 *  the *call* it becomes is `fn_802603A8` itself and not a copy of it. */
class SArgAutoPtr;

/** 0x802603A8, `symbols.txt:10679`, size 0x64: this copy's
 *  `rstl::auto_ptr<T>::~auto_ptr(int flag)`, declared over `SArgAutoPtr` because that is what the
 *  argument temporary is; the two are layout-identical and it arrives in r3 either way.  Declared,
 *  never defined here - see the header. */
extern void fn_802603A8(SArgAutoPtr* self, int flag);

/** the ARGUMENT temporary, defined now that its teardown is declared.  A class rather than a POD
 *  for one reason: mwcceppc destroys a class temporary through its destructor, and the only out of
 *  line teardown retail emits in this function is exactly that. */
class SArgAutoPtr {
public:
  mutable bool mHas;
  SOwnedObject* mItem;

public:
  inline SArgAutoPtr(SOwnedObject* p) : mHas(p != 0), mItem(p) {}
  inline ~SArgAutoPtr() { fn_802603A8(this, -1); }
};

/** 0x8026030C, `symbols.txt:10678`, size 0x9C: this copy's
 *  `TObjOwnerDerivedFromIObj<T>::GetNewDerivedObject(const rstl::auto_ptr<T>&)`, declared over
 *  `SArgAutoPtr` because that is what the argument temporary is.  Declared, never defined here -
 *  see the header for why. */
extern rstl::auto_ptr< SOwnedObject > fn_8026030C(const SArgAutoPtr& obj);

/** `fn_802602E0` - retail `.text:0x802602E0`, 0x2C = 44 bytes, 11 instructions: a frame, the
 *  caller's result slot copied into the callee-saved r31, one call, the epilogue.  The `mr r31,r3`
 *  and its `stw`/`lwz` pair are the sret convention for the class return - see the header - so
 *  r3 is the caller's result slot and the `rstl::auto_ptr<T>` argument is already in r4,
 *  untouched. */
rstl::auto_ptr< SOwnedObject > fn_802602E0(const SArgAutoPtr& obj) { return fn_8026030C(obj); }


/** stands for `CFactoryFnReturn<T>`, which holds exactly one `rstl::auto_ptr` - so the
 *  receiver's +0 is that member's `mHas` and its +4 the member's `mItem`. */
struct SFactoryFnReturn {
  mutable bool mHas;
  SOwnedObject* mItem;
};

/** `fn_8026023C` - retail `.text:0x8026023C`, 0xA4 = 164 bytes, 41 instructions: the frame, the
 *  `(ptr != nullptr)` the argument temporary's flag is built from, the result slot at `r1+0x8`
 *  and the argument temporary at `r1+0x10`, one call, `release()` on the result, the two member
 *  stores through the receiver kept in r31, the **inlined** teardown of the result temporary -
 *  whose `mHas` is reloaded rather than folded, so it is a real destructor - the **out-of-line**
 *  teardown of the argument temporary through `fn_802603A8`, and the epilogue returning the
 *  receiver.  Twin of `__ct<20CScannableObjectInfo>__16CFactoryFnReturnFP20CScannableObjectInfo`
 *  (0x80110B90, 0xA4), so this is that constructor over its own class and its own callees; the
 *  receiver is returned because a constructor returns `this` in r3.
 *
 *  **The member stores must be inside the same full-expression as the call**, or the temporaries
 *  are destroyed before them and the whole 0x34-byte tail moves (measured).  Hence `ctor()`
 *  below, which takes the released pointer by value and returns the receiver: the call and the
 *  stores are then one expression, and the two teardowns follow it in the order retail emits them
 *  - the result temporary's inline block first, the argument temporary's call second. */
static inline void ctor(SFactoryFnReturn* self, SOwnedObject* item) {
  self->mHas = item != 0;
  self->mItem = item;
}

SFactoryFnReturn* fn_8026023C(SFactoryFnReturn* self, SOwnedObject* ptr) {
  ctor(self, fn_802602E0(SArgAutoPtr(ptr)).release());
  return self;
}

} // extern "C"

#ifndef __MWERKS__
// Port-only stand-ins, empty bodies, and they **are** empty.  Both callees are outside this
// claim: `fn_8026030C` is 0x9C retail bytes of an allocation this unit does not spell, and
// `fn_802603A8` is 0x64 bytes of `rstl::auto_ptr`'s deleting-destructor convention.  Neither is
// claimed by anything in the tree, so the DOL link takes both from dtk's own object of the run
// this claim was carved out of.  They exist here so the host link resolves the two `bl`s above, and
// they are the same trade `src/MetroidPrime/Carve801EF730.cpp:197-215` makes for `fn_801EF7B0`:
// an announced stand-in, kept beside the one reference that asks for it.  Before this carve
// nothing in the port referenced either symbol - only dtk's `auto_*` objects did - so they are new
// to the port's link, and `tools/link_gap.py` fails the gate on a missing symbol that is not
// accounted for in `docs/research/port_link_gap_list.md`.  The guard is `__MWERKS__`, not
// `TARGET_PC`, to match those files: the matching build must take the symbols from dtk's object,
// and a second definition there would be the duplicate `tools/gate.sh`'s `port link dups` step
// exists to catch.  `fn_8026030C` returns an empty `rstl::auto_ptr`, which is what its default
// constructor already holds, and `fn_802603A8` destroys nothing - which is the most an empty
// stand-in for a destructor can honestly be.
rstl::auto_ptr< SOwnedObject > fn_8026030C(const SArgAutoPtr& obj) {
  (void)obj;
  return rstl::auto_ptr< SOwnedObject >();
}

void fn_802603A8(SArgAutoPtr* self, int flag) {
  (void)self;
  (void)flag;
}
#endif
