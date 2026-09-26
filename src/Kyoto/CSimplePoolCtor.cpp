/**
 * `CSimplePool::CSimplePool(IFactory&)` - retail `fn_80301008`, `.text:0x80301008`, `size:0x150`
 * = 336 bytes. `CGameGlobalObjects::CGameGlobalObjects` calls it on `this+0xE4` with
 * `this+0x04` - the `CResFactory` - as its `IFactory&` (0x800084AC-0x800084B4), and it is the
 * second of the two constructors on the port's frame-0 critical path.
 *
 * Retail's bytes, all of them:
 *
 *     80301010  lis  r5,0x803C / addi r0,r5,-20636   r0 = 0x803BAF64
 *     8030101c  lbz  r5,8(r1)                        an uninitialised byte of this frame
 *     80301028  lis  r3,0x803C / addi r3,r3,-20592   r3 = 0x803BAF90
 *     8030102c  stw  r0,0(r31)                       the base vtable, dead on the next line
 *     80301038  stw  r3,0(r31)                       `__vt__10CSimplePool`
 *     80301044  stb  r5,4(r31)                       +0x04
 *     80301048  stb  r6,5(r31)                       +0x05, from 0x80419AF8 in .sbss
 *     8030104c  stw  r0,8(r31) .. 20(r31)            +0x08..+0x14, four zero words
 *     8030105c  stb  r6,12(r1)                       the same .sbss byte into a frame slot
 *     80301060  stw  r4,24(r31)                      +0x18, the `IFactory&`
 *     80301064  bl   80031f68                        rc_ptr<IVParamObj>'s from-null ctor
 *     80301068-80301088  the two words into +0x1C/+0x20, then ++*+0x20
 *     8030108c  bl   80031d98                        that temporary's ReleaseData
 *     803010a0  bl   802ce278                        operator new(8, lbl_803AFAE0, 0)
 *     803010ac-803010d0  three vtable stores and    stw r31,4(r3) - the 8-byte object, with
 *                         0x803B16DC, 0x803B19AC,     this as its one member
 *                         0x803BAF68
 *     803010e8  bl   802ce278                        operator new(4, lbl_803AFAE0, 0)
 *     803010f4  li   r0,1 / stw r0,0(r3)             the refcount
 *     80301100  addi r3,r31,28                       &x1c_paramXfr
 *     8030110c  cmplw r4,r0 / beq                    rc_ptr::operator='s identity test
 *     80301114  bl   80031d98                        ... and its release
 *     80301118-80301134  the two words, then ++
 *     8030113c  bl   80031d98                        the temporary's ReleaseData
 *
 * **The 0x150 is one function, but the class is not modelled the way the header models it**, and
 * the difference is measured rather than guessed: the 8-byte object `operator new(8)` returns has a
 * **vtable and a `CSimplePool*`**, and it is what `+0x1C` ends up pointing at. `CSimplePool.hpp`
 * calls `+0x1C` an `rstl::rc_ptr< CVParamTransfer >`, and `CVParamTransfer` is
 * `{ rstl::rc_ptr< IVParamObj > x0_obj; }` - 8 bytes and **no vtable of its own**. So the header's
 * member type cannot produce those four instructions, and the body below carries its own two-word
 * overlay instead. **Nothing in the header is touched**: no member is read through the header's
 * type anywhere in this file, so this is not a header change and has no blast radius. A lane that
 * identifies the 8-byte object can fix the member's type properly.
 *
 * `fn_80031F68` and `fn_80031D98` are retail's own out-of-line `rstl::rc_ptr<IVParamObj>`
 * constructor-from-null and `ReleaseData`; both are unnamed in `config/G2ME01/symbols.txt` and
 * neither is claimed by any unit, so they are called by address - the arrangement
 * `src/Kyoto/CResFactoryBuild.cpp` already uses for `fn_802FAAE4`.
 *
 * `lbl_803AFAE0` is the merged `.rodata` pool object every placement-tagged `new` in this tree
 * passes as its file operand; `Kyoto/Streams/COutputStream.cpp` claims `.rodata` up to exactly
 * that address, so nothing owns it and the reference is free.
 *
 * ## Four things in here that are not logic, all measured
 *
 *  * **The two vtable stores at +0x00 are both kept, and only `volatile` keeps them.** Retail's
 *    is a base class constructor's store followed by the derived one's; written as two assignments
 *    through one overlay pointer, mwcceppc drops the first as dead - it is, and the function still
 *    scores 66.51% without it. `x00_vptr` is a `volatile` member, so both stores are volatile
 *    stores and neither is eliminable. The addresses are written out as **data operands rather
 *    than derived from a class**: deriving them makes this object *emit* `__vt__8IObjectStore`
 *    and `__vt__10CSimplePool` (0x34 and 0x30 bytes of unclaimed `.data`) and needs the nine
 *    `CSimplePool` virtuals that have to fill the second one.
 *  * **A global whose *address* is taken must not be `const`.** Declared `const void*`, all five
 *    vtable operands came out as `lwz rX,0(r13)` / `R_PPC_EMB_SDA21` - read-only small data -
 *    against retail's `lis` + `addi` / `R_PPC_ADDR16_HA` + `_LO`. Non-`const` is what puts them
 *    back in `.data`. The one symbol that *is* `const`, `lbl_80419AF8` in `.sbss`, does want SDA21
 *    and retail has `lbz r6,-25224(r13)`.
 *  * **`+0x04` is an uninitialised byte of this frame, four bytes from a second frame slot.**
 *    Nothing writes either: `stwu r1,-48(r1)` puts this frame's own area at 0..47, the callee
 *    saves LR at 52(r1) and r31 at 44(r1), and 0x08/0x0C are read by `lbz r5,8(r1)` and written
 *    by `stb r6,12(r1)` with no store of their own. This is `src/MetroidPrime/Player/
 *    CPersistentOptionsCtor.cpp`'s spelling - an 8-byte `volatile` local read as two bytes four
 *    apart - because `volatile` is what gives the slots a home at all: without it mwcceppc folds
 *    the read away and emits `stb r3,4(r3)`.
 *  * **`stb r6,12(r1)` is the `.sbss` byte going into that second slot, out of the same
 *    register.** So the value is a named local, not a second read of the global: `w[1] = b`
 *    stores through the `volatile` array and `p->x05_ = b` keeps `b` in r6, and retail has one
 *    `lbz` for both stores.
 *  * **The two `rc_ptr` temporaries live at 16(r1) and 24(r1), and only `volatile` puts them
 *    there.** The second one is handed by address to `fn_80031D98` at 0x80301138
 *    (`addi r3,r1,16`) and read word by word at 0x80301104/0x80301118/0x80301120; the first at
 *    24(r1). Kept in registers instead - which is what a plain local does - the two `operator new`
 *    calls force r29 and r30 into the frame (`stw r29,36(r1)` / `stw r30,40(r1)`, which retail
 *    does not have) and the whole tail is register-allocated differently.
 * ## What is left: five instructions, all one scheduling decision
 *
 * 94.32%, and `tools/unit_fit.sh` reports the object **fits** - 336 claimed, 336 ours, 336 retail,
 * no extra functions - so this unit is one `lis`/`li` placement away from `Matching` and a
 * `flip_test`. The five that differ are the two byte loads and one byte store, and they are in the
 * wrong order relative to each other:
 *
 *     retail  lbz r5,8(r1)          <- position 6,  immediately after the first vtable's `addi`
 *             lbz r6,lbl_80419AF8(r13)  <- position 15, after `addi r3,r1,24`
 *             stb r6,12(r1)         <- position 22, after the four zero words
 *     ours    lbz r6,lbl_80419AF8(r13)  <- position 4
 *             lbz r5,8(r1)          <- position 15
 *             stb r6,12(r1)         <- position 16
 *
 * Twenty-two statement orderings were measured with `tools/try_batch.py`, and the shipped one is
 * the best of them (5 differing instructions; the next best were 6, 7, 10, 12, 13, 13, 16, 17,
 * 19, 20, 20, 22, 23). What does *not* work: reading the frame byte into a named local first
 * (19), splitting the two stores out of the first statement (10), moving the `w[1]` store after
 * `p->x18_factory` (unchanged at 5 - the store position is already right and the loads are what is
 * wrong), and putting the uninitialised read first (12, 13, 20). **It is an mwcceppc list-schedule
 * priority, not a source-order problem**, and no spelling tried moves `lbz` of the `.sbss` byte
 * below the two `lwz`-free `stw`s without moving the volatile load with it.
 */

#include "types.h"

// `#define _CMEMORY` keeps `Kyoto/Alloc/CMemory.hpp`'s own throwing `operator new` out, so the
// two below are the only `new` in this translation unit.
#define _CMEMORY

#include "Kyoto/CSimplePool.hpp"

#if !defined(__MWERKS__)
#include <new>
#endif

// The merged `.rodata` pool object, `lbl_803AFAE0`, `size:0x7`, `type:string` in symbols.txt.
extern "C" const char lbl_803AFAE0[];
// `lbl_80419AF8`, `.sbss:0x80419AF8`, `size:0x8`. Retail loads it with SDA21, so this one *is*
// `const`; nothing in the DOL writes it, so it reads as zero, but the store has to come from
// somewhere and this is it.
extern "C" const uchar lbl_80419AF8;
// The five vtable addresses, each declared as a **sized array**, and that is the whole trick:
// mwcceppc decides small-data addressing from the declared *size*, so a `void*` extern comes out
// `lwz r0,0(r13)` / `R_PPC_EMB_SDA21` and only a declaration of more than 8 bytes gets retail's
// `lis` + `addi` / `R_PPC_ADDR16_HA` + `_LO`. Measured with `tools/probe_cc.sh` on five spellings
// of the same store; see the header. The sizes are retail's own, from `config/G2ME01/symbols.txt`.
extern "C" char lbl_803BAF64[0x2c]; //!< the base class's vtable, dead on the next store
extern "C" char lbl_803BAF90[0x30]; //!< `__vt__10CSimplePool`
extern "C" char lbl_803B16DC[0xc];
extern "C" char lbl_803B19AC[0xc];
extern "C" char lbl_803BAF58[0xc];

#if defined(__MWERKS__) || defined(CLANGD)
void* operator new(size_t sz, const char*, const char*);
inline void* operator new(size_t sz) { return operator new(sz, lbl_803AFAE0, (const char*)0); }
#endif

// `fn_80031F68`, retail 0x80031F68: `rstl::rc_ptr<IVParamObj>`'s constructor from a null pointer.
// `li r4,0 / stw r4,0(r3) / addi r0,r13,-29160 / stw r0,4(r3) / ++*4(r3)` - 0x80418B98 is the one
// global refcount every null `rc_ptr` shares. `fn_80031D98`, retail 0x80031D98, is the same
// class's `ReleaseData`: `--*x4_refCount`, and on zero a delete through vtable slot 2 followed by
// `CMemory::Free(x4_refCount)`.
extern "C" void fn_80031F68(const volatile void* self);
extern "C" void fn_80031D98(const volatile void* self);

// The two words of a `rstl::rc_ptr`, as a local. `volatile` because retail keeps both
// temporaries in the frame rather than in registers - see the header.
struct SVolRc {
  void* x0_ptr;
  void* x4_refCount;
};

// `CSimplePool`'s 0x24 bytes as retail's constructor writes them. `x08_`..`x14_` are the four
// words `rstl::hash_map`'s `hash_table` degenerates to here - retail zeroes them and nothing else
// in the DOL reads them - and `x1c_`/`x20_` are the two words of the `rc_ptr` at +0x1C.
// `x00_vptr` is `volatile` so the base class's dead store survives; see the header.
struct SSimplePool {
  void* volatile x00_vptr;
  uchar x04_;
  uchar x05_;
  uint x08_;
  uint x0c_;
  uint x10_;
  uint x14_;
  const IFactory* x18_factory;
  void* x1c_ptr;
  void* x20_refCount;
};
CHECK_SIZEOF(SSimplePool, 0x24)

// The 8 bytes `operator new(8)` returns: a vtable and the `CSimplePool*` that owns it. Three
// vtable stores in a row, so three levels of base constructor, and the last one is the only one
// that survives.
struct SPooledEntry {
  void* volatile x00_vptr;
  const CSimplePool* x04_owner;
};
CHECK_SIZEOF(SPooledEntry, 0x8)

// Named for the address and given C linkage because retail's symbol table has no name for it: a
// C++ `CSimplePool::CSimplePool(IFactory&)` would mangle to `__ct__10CSimplePoolFR8IFactory` and
// objdiff would have nothing to pair it against. Returns `this`, as retail does (`mr r3,r31`).
extern "C"
CSimplePool* fn_80301008(CSimplePool* self, IFactory& factory) {
  SSimplePool* p = reinterpret_cast< SSimplePool* >(self);

  // **Declared in reverse of the order they are used, because that is how mwcceppc hands out
  // frame slots**: the last declaration gets the lowest address. Retail's frame is 8/12 for the
  // two uninitialised bytes, 16/20 for the second `rc_ptr` temporary and 24/28 for the first.
  SVolRc volatile tmpNull;
  SVolRc volatile tmpEntry;
  volatile u32 w[2];

  p->x00_vptr = lbl_803BAF64;
  p->x00_vptr = lbl_803BAF90;

  uchar b = lbl_80419AF8;
  p->x04_ = *reinterpret_cast< volatile uchar* >(&w[0]);
  p->x05_ = b;

  p->x08_ = 0;
  p->x0c_ = 0;
  p->x10_ = 0;
  p->x14_ = 0;
  *reinterpret_cast< volatile uchar* >(&w[1]) = b;
  p->x18_factory = &factory;

  // A `CVParamTransfer` temporary, built by `fn_80031F68` and *copy-constructed* into the member:
  // two words, then `++*refCount`, then the temporary's own `ReleaseData`. There is no release of
  // the member's previous value here, which is what makes this a copy construction rather than
  // `rc_ptr::operator=` - and the second assignment below is the same `operator=`, with its
  // identity test and its release, twenty instructions further on. The net refcount is 1.
  fn_80031F68(&tmpNull);
  p->x1c_ptr = tmpNull.x0_ptr;
  p->x20_refCount = tmpNull.x4_refCount;
  ++*static_cast< uint* >(p->x20_refCount);
  fn_80031D98(&tmpNull);

  // `x1c_paramXfr = rc_ptr<SPooledEntry>(new SPooledEntry(this))`. The two allocations are
  // independent null tests, the 4-byte one first-initialised to 1, and `operator new`'s file
  // operand is `lbl_803AFAE0` on both calls.
  SPooledEntry* entry = static_cast< SPooledEntry* >(::operator new(sizeof(SPooledEntry)));
  if (entry) {
    entry->x00_vptr = lbl_803B16DC;
    entry->x00_vptr = lbl_803B19AC;
    entry->x00_vptr = lbl_803BAF58;
    entry->x04_owner = self;
  }
  // `tmpEntry.x0_ptr` is stored **before** the second allocation (`stw r3,16(r1)` at 0x803010D8,
  // ahead of `li r3,4` at 0x803010E0), and that is what keeps the pointer out of a callee-saved
  // register: leave the store after the call and mwcceppc holds `entry` in r30 across it and
  // spends `stw r30,40(r1)` / `lwz r30,40(r1)` on it, which retail has no room for.
  tmpEntry.x0_ptr = entry;
  uint* refCount = static_cast< uint* >(::operator new(4));
  if (refCount) {
    *refCount = 1;
  }
  tmpEntry.x4_refCount = refCount;

  SVolRc* selfRc = reinterpret_cast< SVolRc* >(&p->x1c_ptr);
  if (selfRc->x0_ptr != tmpEntry.x0_ptr) {
    fn_80031D98(selfRc);
    p->x1c_ptr = tmpEntry.x0_ptr;
    p->x20_refCount = tmpEntry.x4_refCount;
    ++*static_cast< uint* >(p->x20_refCount);
  }
  fn_80031D98(&tmpEntry);
  return self;
}
