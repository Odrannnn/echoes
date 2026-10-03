// Carved out of an unclaimed dtk `auto_*` range.  Every number here is measured: the addresses and
// sizes come from `config/G2ME01/symbols.txt:10614-10615`, the instructions are the ones dtk itself
// emitted into `build/G2ME01/asm/auto_03_8025CA80_text.s:1251-1314` (the same range is now
// `build/G2ME01/asm/Weapons/Carve8025DBA4.s`), and the bodies below are the C those bytes are the
// compilation of.  The byte evidence is the pristine disc, not our own build:
// `python3 tools/dol_read.py 0x8025DBA4 0xDC orig/G2ME01/sys/main.dol`.
//
// .text 0x8025DBA4..0x8025DC80, 0xDC = 220 bytes, 2 functions:
//
//   fn_8025DBA4    0x8025DBA4  0xB0 = 176 bytes  44 instructions
//   fn_8025DC54    0x8025DC54  0x2C =  44 bytes  11 instructions
//
// **Both bodies are their twin's own, read out of this tree rather than guessed.**  `fn_8025DBA4` is
// `__ct<13CSkinnedModel>__16CFactoryFnReturnFP13CSkinnedModel` (0x80031950, 0xB0,
// `build/G2ME01/asm/MetroidPrime/Factories/CCharacterFactory.s:2513-2562`) instruction for
// instruction apart from the three `bl` displacements, which are address-relative.  `fn_8025DC54`
// is `GetIObjObjectFor__23TToken<13CSkinnedModel>FRCQ24rstl25auto_ptr<13CSki` (0x80031A00, 0x2C,
// `symbols.txt:931`, `CCharacterFactory.s:2564-2578`) - the same eleven instructions with this
// copy's own `bl fn_8025DC80` where the twin has its
// `GetNewDerivedObject__41TObjOwnerDerivedFromIObj<13CSkinnedModel>F...`.  Retail's own source for
// both is in this tree: `include/Kyoto/CFactoryMgr.hpp:19` is
// `CFactoryFnReturn(T* ptr) : obj(TToken< T >::GetIObjObjectFor(ptr).release())` and
// `include/Kyoto/TToken.hpp:24-27` is `GetIObjObjectFor`'s whole body, a single
// `return TObjOwnerDerivedFromIObj< T >::GetNewDerivedObject(obj);`.  The three types below are
// written out rather than `#include`d because the argument's type parameter is a class retail does
// not name here, and because spelling them locally is what keeps this file's `.text` at exactly
// 0xDC bytes (see the note on `SOwnedRes`).
//
// **What the two functions are is measured from their caller's arguments, not guessed.**
// `FProjectileWeaponDataFactory__FRC10SObjectTagR12CInputStreamRC15CVParamTransfer` (0x8025DB38,
// 0x6C, `symbols.txt:10613`, `auto_03_8025CA80_text.s:1218-1249`) sets `mr r3,r31 / mr r4,r0` and
// calls `fn_8025DBA4` at 0x8025DB84, so **r3 is the return slot of a class returned by value and
// r4 is one pointer** - and `fn_8025DBA4` returns its r3 in r31 (`mr r31,r3` at 0x8025DBC0,
// `mr r3,r31` at 0x8025DC40).  That is `CFactoryFnReturn`'s own `T*`-taking constructor, whose
// member `obj` is the two-word `rstl::auto_ptr< IObj >` at `include/Kyoto/CFactoryMgr.hpp:24`.  The
// `bl fn_8025DC54` at 0x8025DBD4 passes `addi r3,r1,0x8 / addi r4,r1,0x10`, i.e. an 8-byte class
// return in r3 and one 8-byte **by-const-reference** argument in r4 - the argument is the
// `rstl::auto_ptr<T>` temporary built from the incoming pointer at `stb r0,0x10(r1)` /
// `stw r4,0x14(r1)` (0x8025DBD0, 0x8025DBC8).  Both `T*` and that temporary are therefore the same
// two-word `{ bool mHas; T* mItem; }` at `include/rstl/auto_ptr.hpp:15-16`, which is why the
// `neg r0,r4 / or / srwi` at the top of `fn_8025DBA4` (0x8025DBB0..0x8025DBBC) is `ptr != nullptr`
// being stored into `mHas`.
//
// **What the `bl fn_8025DC80` callee is, measured from its own bytes.**  0x8025DC80, 0x9C,
// `symbols.txt:10616`, now `build/G2ME01/asm/auto_03_8025DC80_text.s`: `li r3,0x8 /
// bl __nw__FUlPCcPCc`, then the three `stw r0,0x0(r3)` of `__vt__4IObj` /
// `__vt__31CObjOwnerDerivedFromIObjUntyped` / `lbl_803B8B40`, then `stb r5,0x0(r31)` releasing the
// source's +0 flag and `stw r4,0x4(r3)` copying its +4 pointer.  That is byte for byte
// `GetNewDerivedObject__50TObjOwnerDerivedFromIObj<22CCollisionResponseData>FRCQ24rstl34auto_ptr<22CCollisionResponseData>`
// at 0x8025DF24 (`symbols.txt:10621`, also 0x9C) and the twin at 0x80031A2C, so its declaration is
// the same `rstl::auto_ptr` by value that `include/Kyoto/IObj.hpp:41-46` gives `GetNewDerivedObject`.
// **It is not claimed here**: it is the next unsourced function of the same run, and this claim
// stops at 0x8025DC80, so the DOL link takes it from dtk's own object of the surrounding run.  This
// unit claims `.text` and nothing else.
//
// **The virtual call at 0x8025DC10 is the returned `rstl::auto_ptr`'s own destructor**, reached
// through `include/rstl/auto_ptr.hpp:21-25`'s `if (mHas) { delete mItem; }`.  The bytes fix the
// rest: `lwz r12,0x0(r3)` is the vtable, `li r4,0x1` is the deleting flag and `bctrl` the vtable's
// slot +8, so the pointee type has a virtual destructor and this copy's `delete` is a virtual call -
// `rstl::auto_ptr< TObjOwnerDerivedFromIObj< T > >` at `include/Kyoto/IObj.hpp:36-39`, which derives
// from the polymorphic `IObj` at `include/Kyoto/IObj.hpp:13-14`.  The other destructor, the
// argument's, is the **direct** `bl fn_8026258C` at 0x8025DC38: the same `delete` on the argument's
// `T*`, whose destructor is not virtual, so the call is static.  `fn_8026258C` (0x8026258C,
// `symbols.txt:10699`, size 0x500) is unclaimed by anything and is declared, never defined, here.
//
// **`SOwnedRes` has no destructor on purpose, and that is load-bearing.**  The obvious spelling -
// give the two-word classes a destructor and let the compiler write the two blocks - emits
// `__dt__...` **out of line** as weak copies (measured: `powerpc-eabi-nm` on the object shows
// `__dt__9SOwnedArgFv` and `__dt__Q24rstl32auto_ptr<20SOwnedProjectileData>Fv` at 0xb0 and 0x114),
// which is 0xD4 bytes of `.text` retail does not have in this range and would break the unit for the
// reason `tools/unit_fit.sh` exists.  So the destructor bodies are written out as the two `if`s at
// the end of `fn_8025DBA4`, in the same order the frame destroys them (the returned pointer's
// first, then the argument's).  Three further spellings are forced by the bytes rather than chosen:
//
//   * `SOwnedRes::release()` is **`const` and `mHas` is `mutable`**, exactly as
//     `include/rstl/auto_ptr.hpp:15` and `:46-49` are.  That is what makes retail store the flag
//     *through the object it is about to release* (`stb r0,0x8(r1)` at 0x8025DBE0) and then
//     **reload** it (`lbz r0,0x8(r1)` at 0x8025DBF8) instead of folding the load to the constant it
//     just wrote.  Written with a non-`const` `release()` and a tracked local, mwcceppc emits the
//     41-instruction body that drops both instructions (measured).
//   * `fn_8025DC54`'s result is bound to a **`const&`**, not copied into a named local.  That is
//     retail's `return ...;` eliding the temporary's own copy, and it is what keeps the sret slot at
//     `r1+0x8` instead of `r1+0x10` with a second frame slot for the copy (measured: a named
//     `SOwnedRes res = fn_8025DC54(src);` puts the sret at 0x8025DBC4's `addi r3,r1,0x10` and drops
//     the `lwz r3,0xc(r1)`).
//   * `SOwnedRes` declares a **copy constructor**.  `rstl::auto_ptr` has one
//     (`include/rstl/auto_ptr.hpp:27-29`) and it is what makes the class non-POD, which is what
//     decides the sret convention: a plain 8-byte C struct comes back in r3:r4 with no r31 at all
//     (measured - the same finding `src/MetroidPrime/Carve801EF730.cpp:99-108` records for
//     `fn_801EF784`).
//   * `fn_8025DBA4` tests `if (out->mHas)` and then **a bare `delete`**, with no `&& mItem != 0`.
//     That is not a shortcut: retail has the null check, but as the `cmplwi r3,0 / beq` the compiler
//     writes for `delete` itself (0x8025DC08, 0x8025DC0C), and adding the test by hand emits a
//     **second** `beq` (measured: 56 instructions against retail's 55).
//
// Source order is **descending by address** and that is load-bearing: mwcceppc emits function
// definitions in *reverse* source order and mwldeppc keeps the object `.text` verbatim, so an
// ascending file is a permuted `.text` - 100.00% per function and a broken DOL.  Only
// `tools/flip_test.sh` catches that.  Check it with
// `python3 tools/check_decl_order.py --unit main/Weapons/Carve8025DBA4`.
//
// Retail names neither of these.  `symbols.txt` carries the `fn_<addr>` placeholders and this file
// reproduces those symbols verbatim, so the definitions have to be C-linkage - a C++ one would
// mangle to `_Z<len>fn_<addr>P...` and objdiff would pair nothing.  That is also why the unit is a
// `.cpp` with `extern "C"` rather than a `.c`: the sret convention above needs a class, and only C++
// has one.
//
// Its own unit because a unit may not claim two discontiguous ranges in one section (dtk `dol split`
// fails with "Cyclic dependency ... link order"), and because a claim may not span an unclaimed gap.
// The claim starts and ends inside `auto_03_8025CA80_text` (0x8025CA80..0x8025DBA4), the function
// below is `FProjectileWeaponDataFactory` (0x8025DB38, 0x6C) and the one above is `fn_8025DC80`
// (0x8025DC80, 0x9C).  Below the claim `Weapons/CProjectileWeapon.cpp` ends at 0x8025CA80 - exactly
// where the run starts - and above it `Weapons/CCollisionResponseData.cpp` starts at 0x8025DD1C; the
// 0x9C bytes between that and this claim's end are `fn_8025DC80` and stay unclaimed.  **The run
// starting exactly where the previous unit's `.text` ends is the shape `docs/RUNNING_THE_DECOMP.md`
// "The carve vein" warns can produce a `dtk dol split` link-order cycle**; it does not here
// (measured - the split and the full link both succeed), and `flip_test.sh` is the arbiter.
//
// The directory is retail's own, taken from the nearest claimed ranges: below is
// `Weapons/CProjectileWeapon.cpp` (`.text` 0x802591B4..0x8025CA80) and above is
// `Weapons/CCollisionResponseData.cpp` (0x8025DD1C..0x8025F41C), so this address sits in the
// `Weapons/` neighbourhood.  For an anonymous function that is the only evidence there is, and it
// beats a lane picking the directory it happened to own.
//
// **The callees are declared and never defined here, and each needs one answer for two builds.**
// This unit is in `files.cmake`, so the PC build compiles it with the *host* compiler while the
// matching build uses mwcceppc, and `__MWERKS__` is the discriminator the tree already uses for
// that split (`src/MetroidPrime/Carve80004438.c:78-81`,
// `src/MetroidPrime/ScriptObjects/Carve801FEAE0.cpp:250`).
//   * `fn_8025DC80` has **no** definition anywhere in `src/`, in either build, so for the port link
//     it needs an announced empty-body stand-in, and the same is true of `fn_8026258C`.  Both stand
//     at the bottom of this file under `#ifndef __MWERKS__` rather than in
//     `src/MetroidPrime/PortLinkStubs.cpp`, which has stand-ins for other carves: that file is
//     being rewritten by other lanes in this same goal run, so a concurrent edit there is a merge
//     conflict over a stand-in nobody has reviewed.  The trade is the same one
//     `src/MetroidPrime/Carve801EF730.cpp:210-214` makes for `fn_801EF7B0`, and both bodies are
//     announced where they are declared.  Nothing here claims either callee is decompiled.

#include "rstl/auto_ptr.hpp"

extern "C" {

/** The `T` of this copy's `CFactoryFnReturn<T>::CFactoryFnReturn(T*)`.  Retail does not name it and
 *  nothing here reads it - `fn_8025DBA4` only ever tests the pointer against null and hands it to
 *  `fn_8025DC80`, whose own 0x9C bytes copy the flag and the pointer and nothing else. */
struct SProjectileData {
  int m_x;
};

/** 0x8026258C, `symbols.txt:10699`, size 0x500: the argument's deleting destructor, called
 *  **statically** - retail's `bl fn_8026258C` at 0x8025DC38 is a `bl`, not a `bctrl`, so `T`'s
 *  destructor is not virtual.  Unclaimed by anything, so the DOL link takes it from dtk's own
 *  object of the run this claim was carved out of.  Declared, never defined here. */
extern void fn_8026258C(SProjectileData* self, int flag);

/** The pointee of the returned `rstl::auto_ptr`: `TObjOwnerDerivedFromIObj<T>`'s shape, which
 *  derives from the polymorphic `IObj`.  Its virtual destructor is what makes `delete` in
 *  `fn_8025DBA4` a vtable call with the deleting flag (`li r4,0x1 / mtctr / bctrl`, 0x8025DC14).
 *  Only the vtable pointer is modelled here; this unit's own bytes never load it. */
struct SOwnedProjectileData {
  virtual ~SOwnedProjectileData();
  void* mObjPtr;
};

/** `rstl::auto_ptr<T>` by value, the type `fn_8025DBA4` builds from its `T*` argument and passes by
 *  const reference.  Two words, `include/rstl/auto_ptr.hpp:15-16`: `mHas` is the owning flag and
 *  `mItem` the pointer. */
struct SOwnedArg {
  bool mHas;
  SProjectileData* mItem;
};

/** `rstl::auto_ptr< TObjOwnerDerivedFromIObj<T> >` by value, what `fn_8025DC54` returns.
 *
 *  The copy constructor is declared for the reason given in the header: it is what makes the class
 *  non-POD, and a non-POD 8-byte class is returned through a hidden pointer in r3 - which is retail's
 *  `mr r31,r3` in `fn_8025DC54`.  Without it MWCC hands an 8-byte POD back in r3:r4 and the whole
 *  eleven-instruction shape is eight instructions (measured).
 *
 *  `release()` is `const` and `mHas` is `mutable`, exactly as `include/rstl/auto_ptr.hpp:15` and
 *  `:46-49` are, and that is not cosmetic: it is what makes retail **store** the flag and then
 *  **reload** it rather than folding the load to the constant just written. */
class SOwnedRes {
public:
  SOwnedRes(bool has, SOwnedProjectileData* item) : mHas(has), mItem(item) {}
  SOwnedRes(const SOwnedRes& o) : mHas(o.mHas), mItem(o.mItem) {}
  SOwnedProjectileData* release() const {
    mHas = false;
    return mItem;
  }
  mutable bool mHas;
  SOwnedProjectileData* mItem;
};

/** `CFactoryFnReturn`'s single member, `rstl::auto_ptr< IObj >`
 *  (`include/Kyoto/CFactoryMgr.hpp:24`).  `fn_8025DBA4`'s r3 is this struct: the caller's
 *  `FProjectileWeaponDataFactory` passes `mr r3,r31` and reads it back as the factory's own result. */
struct SFactoryFnReturn {
  SOwnedRes obj;
};

/** 0x8025DC80, `symbols.txt:10616`, size 0x9C: this copy's
 *  `TObjOwnerDerivedFromIObj<T>::GetNewDerivedObject(const rstl::auto_ptr<T>&)`, byte for byte
 *  `GetNewDerivedObject__50TObjOwnerDerivedFromIObj<22CCollisionResponseData>F...` at 0x8025DF24 -
 *  hence the same return type, the class `rstl::auto_ptr` by value.  Declared, never defined here:
 *  nothing in the tree claims it, so the DOL link takes it from dtk's own object of the run this
 *  claim was carved out of. */
extern SOwnedRes fn_8025DC80(const SOwnedArg& obj);

/** 0x8025DC54, `symbols.txt:10615`, size 0x2C: `TToken<T>::GetIObjObjectFor`, retail
 *  `GetIObjObjectFor__23TToken<13CSkinnedModel>F...` at 0x80031A00, 0x2C - the same eleven
 *  instructions.  Its body is `include/Kyoto/TToken.hpp:24-27` verbatim: one call and the epilogue.
 *  The `mr r31,r3` and its `stw`/`lwz` pair are the sret convention for the class return, so r3 is
 *  the caller's result slot and the argument is already in r4. */
SOwnedRes fn_8025DC54(const SOwnedArg& obj);

/** 0x8025DBA4, `symbols.txt:10614`, size 0xB0: this copy's
 *  `CFactoryFnReturn<T>::CFactoryFnReturn(T* ptr)`, retail
 *  `__ct<13CSkinnedModel>__16CFactoryFnReturnFP13CSkinnedModel` at 0x80031950, 0xB0 - the same 44
 *  instructions.  `ptr` is retail's `T*` argument; `self` is the hidden return pointer, the r3 the
 *  body parks in r31 across the call and hands back at 0x8025DC40.
 *
 *  The two `if`s at the end are the two `rstl::auto_ptr` destructors, written out rather than left
 *  to the compiler so that no `__dt__` is emitted out of line into this range - see the header.  The
 *  first is the returned pointer's (virtual, through the vtable), the second the argument's (a direct
 *  call to `fn_8026258C`). */
SFactoryFnReturn* fn_8025DBA4(SFactoryFnReturn* self, SProjectileData* ptr);

SOwnedRes fn_8025DC54(const SOwnedArg& obj) { return fn_8025DC80(obj); }

SFactoryFnReturn* fn_8025DBA4(SFactoryFnReturn* self, SProjectileData* ptr) {
  SOwnedArg src;
  src.mHas = ptr != 0;
  src.mItem = ptr;
  const SOwnedRes& res = fn_8025DC54(src);
  const SOwnedRes* out = &res;
  SOwnedProjectileData* raw = out->release();
  self->obj.mHas = raw != 0;
  self->obj.mItem = raw;
  if (out->mHas) {
    delete out->mItem;
  }
  if (src.mHas) {
    fn_8026258C(src.mItem, 1);
  }
  return self;
}

#ifndef __MWERKS__
// Port-only stand-ins, empty bodies, and they **are** ones: `fn_8025DC80`'s own 0x9C retail bytes and
// `fn_8026258C`'s 0x500 are spelling jobs of their own and nothing in this tree has claimed them.
// They exist so the host link resolves the three `bl`s above.  The guard is `__MWERKS__`, not
// `TARGET_PC`, to match `src/MetroidPrime/Carve801EF730.cpp:197-215`: the matching build must take
// both symbols from dtk's own objects of the surrounding run, and a second definition there would
// be the duplicate `tools/gate.sh`'s `port link dups` step exists to catch.
//
// **Reachable in the port today only through `fn_8025DBA4`**, whose one caller is
// `FProjectileWeaponDataFactory` (0x8025DB38) - itself inside the unclaimed `auto_03_8025CA80_text`,
// with no claimed unit and no vtable entry, so nothing in the port dispatches to it.  The two empty
// bodies therefore drop a factory that builds a projectile-weapon data object; see the notes file.
SOwnedRes fn_8025DC80(const SOwnedArg&) { return SOwnedRes(false, 0); }
void fn_8026258C(SProjectileData*, int) {}
#endif

} // extern "C"
