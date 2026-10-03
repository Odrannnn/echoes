// Carved out of the unclaimed dtk `auto_03_8032AFC8_text` range.  Every number here is measured:
// the addresses and sizes come from `config/G2ME01/symbols.txt:14868-14869`, the instructions are
// the ones dtk itself emitted into `build/G2ME01/asm/auto_03_8032AFC8_text.s:504-567`, and the
// bodies below are the C those bytes are the compilation of.  The byte evidence is the pristine
// disc, not our own build: `python3 tools/dol_read.py 0x8032B648 0xDC orig/G2ME01/sys/main.dol`
// prints the 220 bytes reproduced below.
//
// .text 0x8032B648..0x8032B724, 0xDC = 220 bytes, 2 functions:
//
//   fn_8032B648  0x8032B648  0xB0  176 bytes  44 instructions
//   fn_8032B6F8  0x8032B6F8  0x2C   44 bytes  11 instructions
//
// **Both are byte-shape twins of retail functions that are already matched**, which is where the
// bodies are read from rather than guessed.  Read out of
// `build/G2ME01/asm/MetroidPrime/Factories/CCharacterFactory.s:2513-2578`, each range is its
// twin's word for word apart from the `bl` displacements, which are address-relative and are
// filled by the link:
//
//   fn_8032B648 is `CFactoryFnReturn<CSkinnedModel>::CFactoryFnReturn(CSkinnedModel*)` (retail
//     0x80031950, 0xB0, `symbols.txt:930`), the constructor spelled at
//     `include/Kyoto/CFactoryMgr.hpp:18-19` as
//     `CFactoryFnReturn(T* ptr) : obj(TToken<T>::GetIObjObjectFor(ptr).release()) {}`.
//     Its `bl __dt__13CSkinnedModelFv` is this copy's `bl fn_8032DC68`, and its
//     `bl "GetIObjObjectFor__23TToken<13CSkinnedModel>F..."` is this copy's `fn_8032B6F8`.
//   fn_8032B6F8 is `TToken<CSkinnedModel>::GetIObjObjectFor(const rstl::auto_ptr<CSkinnedModel>&)`
//     (retail 0x80031A00, 0x2C, `include/Kyoto/TToken.hpp:24-27`), whose
//     `bl "GetNewDerivedObject__41TObjOwnerDerivedFromIObj<13CSkinnedModel>F..."` is this copy's
//     `bl fn_8032B724`.
//
// **The three `rstl::auto_ptr<T>` shapes are retail's own, and each one is what fixes a
// different part of the bytes.**  `mHas` at +0 and `mItem` at +4 is
// `include/rstl/auto_ptr.hpp:15-16`; `stb` for the flag and `stw` for the pointer is what pins it
// to a 1-byte `bool` rather than a 4-byte one.  The three uses are:
//
//   * the **argument** wrapper `SArgPtr` below, destroyed by a *direct*
//     `fn_8032DC68(mItem, 1)`, because `CSkinnedModel`'s destructor is non-virtual - retail's
//     `bl __dt__13CSkinnedModelFv` at 0x800319E4 is the same direct call;
//   * the **return** wrapper `SRetPtr`, destroyed by `delete mItem` on a *virtual* destructor,
//     which is the `lwz r12,0(r3)` / `li r4,0x1` / `lwz r12,0x8(r12)` / `mtctr` / `bctrl` dispatch
//     at 0x8032B6B4;
//   * the **member** `obj`, which is only ever written, never destroyed here.
//
// **`fn_8032B648` keeps retail's two dead stores.**  The `neg`/`or`/`srwi` pair at 0x8032B654 is
// `(ptr != 0)` computed into `r0`; the identical pair at 0x8032B688 is `(result.mItem != 0)`
// feeding `stb r0,0(r31)`.  mwcceppc emits `stw r3,0x4(r31)` - reusing the `r3` already holding
// `result.mItem` - only when the pointer is read out of `result` into a **named local `p`** before
// the flag is cleared.  Written as two direct member reads (`self->obj.mItem = result.mItem;`)
// it reloads `lwz r0,0xc(r1)` first and the store is `stw r0,0x4(r31)`: same 44 instructions,
// one word different, 0xB0 bytes not reproduced.  Measured, both.
//
// **The `fn_8032B6F8` call goes through a function pointer, and that is load-bearing rather than
// a workaround.**  Called directly, mwcceppc emits `addi r3,r1,8` / `addi r4,r1,0x10` / `bl` and
// *also* emits the copy that materialises the returned class object, which is 7 extra
// instructions and a 0x30 frame instead of retail's 0x20.  Retail has no such copy because
// `GetIObjObjectFor` returns into a slot the caller already owns.  Casting the callee to
// `TGetObj` and calling through that pointer reproduces the sret call *and* keeps the returned
// object in the caller's own `result` local, which is what gives all 44 instructions.
//
// **`SRetPtr` must carry a user-declared constructor.**  That is the whole difference between
// `fn_8032B6F8`'s 8 instructions and retail's 11.  Without one, mwcceppc treats the 8-byte
// return as returned in registers and tail-calls `fn_8032B724` with no frame at all; retail's
// `stw r31,0xc(r1)` / `mr r31,r3` / `bl` / `lwz r31,0xc(r1)` is the hidden-return pointer parked
// in a callee-saved register across the call, which a class return type with a constructor
// forces.  Measured with `SRetPtr(void*)`, `SRetPtr()` and an out-of-line `SRetPtr(void*)` -
// all three reach it, and all three leave `fn_8032B648` at 44 instructions.  A bare
// `class SRetPtr { bool mHas; void* mItem; };` with no constructor does not.
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  Check it with
// `python3 tools/check_decl_order.py --unit Kyoto/Math/Carve8032B648.cpp`.
//
// Retail names none of these two.  `symbols.txt` carries the `fn_<addr>` placeholders and this
// file reproduces those symbols verbatim, so the definitions are `extern "C"`: without it they
// would mangle to `fn_8032B648Pv` and objdiff would pair nothing.
//
// Its own unit because a claim may not span an unclaimed gap and a unit may not claim two
// discontiguous ranges in one section.  The claim starts at `fn_8032B648`, which is exactly
// where `FSpawnParticleSystemDataFactory__FRC10SObjectTagR12CInputStreamRC15CVParamTransfer`
// (0x8032B5DC, 0x6C) ends, and stops at `fn_8032B724`, which `symbols.txt:14870` gives a size of
// 0x9C - so nothing above the item's two functions is taken.  Both neighbours stay in dtk's
// `auto_03_8032AFC8_text`, which now runs 0x8032AFC8..0x8032B648 and 0x8032B724..0x8032C144 as
// two objects.
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `Kyoto/Math/CMayaSpline.cpp` (`.text` 0x80327C10..0x8032AFC8) and above is
// `Kyoto/Math/Carve8032C144.c` (0x8032C144..0x8032C14C), so this address sits in the
// `Kyoto/Math` neighbourhood - the same reasoning `src/Kyoto/Math/Carve8032F2B8.c:86-89` records.
//
// `fn_8032B724` (`TObjOwnerDerivedFromIObj<CSkinnedModel>::GetNewDerivedObject`,
// `include/Kyoto/IObj.hpp:44-47`) and `fn_8032DC68` (`~CSkinnedModel`, 0x8032DC68, 0x218) are both
// retail's and both are claimed by no unit, so they stay in dtk's `auto_*` ranges.  Declaring
// them `extern` opens nothing in the DOL link; the host definitions are at the bottom of this
// file.

/** Retail's `rstl::auto_ptr<IObj>` as `CFactoryFnReturn` stores it: the flag and the pointer, and
 *  nothing else.  Read and written, never destroyed, so it needs no constructor of its own. */
struct SObjPtr {
  bool mHas;
  void* mItem;
};

/** Retail's `IObj`'s vtable, as far as `fn_8032B648` looks: slot +8 is the virtual destructor
 *  (`lwz r12,0x8(r12)` before the `mtctr`/`bctrl`). */
struct IObjV {
  virtual void deletingDtor(int) = 0;
};

extern "C" void fn_8032DC68(void* self, int flag);

/** Retail's `rstl::auto_ptr<TObjOwnerDerivedFromIObj<CSkinnedModel>>`, the type
 *  `fn_8032B6F8` returns.  **The user-declared constructor is what earns `fn_8032B6F8` its 11
 *  instructions** - see the note at the head of this file.  It is never constructed in the DOL
 *  path and never destroyed: `fn_8032B648` writes the `delete` out by hand, which is what keeps
 *  its own frame at retail's 0x20. */
class SRetPtr {
public:
  bool mHas;
  void* mItem;
  SRetPtr(void* p) : mHas(p != 0), mItem(p) {}
};

extern "C" SRetPtr fn_8032B724(SObjPtr* obj);
extern "C" SRetPtr fn_8032B6F8(SObjPtr* obj) { return fn_8032B724(obj); }

/** `fn_8032B6F8`'s own signature as `fn_8032B648` calls it.  Retail's call is `addi r3,r1,0x8` /
 *  `addi r4,r1,0x10` / `bl`, i.e. the returned object is written straight into the caller's
 *  frame slot; saying so explicitly is what stops mwcceppc adding a copy. */
typedef void (*TGetObj)(SObjPtr* result, SObjPtr* obj);

/** `CFactoryFnReturn<T>`, which holds exactly one `rstl::auto_ptr<IObj>`. */
struct SFnReturn {
  SObjPtr obj;
};

extern "C" SFnReturn* fn_8032B648(SFnReturn* self, void* ptr) {
  SObjPtr tmp;
  SObjPtr result;
  void* p;
  TGetObj get = (TGetObj)fn_8032B6F8;
  tmp.mItem = ptr;
  tmp.mHas = (ptr != 0);
  get(&result, &tmp);
  /* `TToken<T>::GetIObjObjectFor(ptr).release()`: `release()` clears the flag at 0x8(r1) and
   * hands the pointer back, and `obj`'s constructor re-derives the flag from it.  `p` is a named
   * local so the pointer survives into `stw r3,0x4(r31)` instead of being reloaded. */
  p = result.mItem;
  result.mHas = 0;
  self->obj.mHas = (p != 0);
  self->obj.mItem = p;
  /* The returned `auto_ptr`'s destructor, written out: `delete mItem`, and the pointee's
   * destructor is virtual, so this is the vtable dispatch. */
  if (result.mHas) {
    IObjV* o = (IObjV*)result.mItem;
    if (o != 0) {
      o->deletingDtor(1);
    }
  }
  /* The argument `auto_ptr`'s destructor: a direct `~CSkinnedModel(mItem, 1)`, because
   * `CSkinnedModel`'s destructor is not virtual. */
  if (tmp.mHas) {
    fn_8032DC68(tmp.mItem, 1);
  }
  return self;
}

#ifdef TARGET_PC
/* The port's link is the half of this unit that `tools/link_check.sh` gates, and it has neither
 * of the two things the DOL build gets from elsewhere: `fn_8032B724` and `fn_8032DC68` live in
 * dtk's `auto_*` objects, *retail* split objects that only `mwldeppc` links.  A host build asks
 * for a symbol nothing defines and `tools/probe_sources.sh` fails when that count grows, so the
 * host definitions go here, next to the declarations they complete, and not in
 * `PortLinkStubs.cpp`: that file is generated (`tools/gen_link_stubs.py`).  This is the same
 * trade `src/Kyoto/Math/Carve8032F648.c:136-153` makes for the same reason.
 *
 * Both **are** stand-ins and both announce themselves: making a *new* derived IObj wrapper needs
 * `CMemory`'s allocator and knowing which class this one is, and destroying a `CSkinnedModel`
 * needs its layout.  Neither is knowable from here, so each prints its own name and returns
 * without claiming to have done the work. */
#include <stdio.h>
extern "C" void fn_8032DC68(void* self, int flag) {
  printf("[port-stub] fn_8032DC68 (CSkinnedModel::~CSkinnedModel) self=%p\n", self);
}
extern "C" SRetPtr fn_8032B724(SObjPtr* obj) {
  SRetPtr r((void*)obj);
  printf("[port-stub] fn_8032B724 (TObjOwnerDerivedFromIObj<CSkinnedModel>::GetNewDerivedObject)"
         " obj=%p\n",
         (void*)obj);
  r.mHas = false;
  r.mItem = 0;
  return r;
}
#endif