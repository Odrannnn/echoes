/**
 * `CResFactory::CResFactory()` - retail `fn_802FB154`, `.text:0x802FB154`, `size:0xA8` = 168 bytes.
 * The **first** of the two constructors on the port's frame-0 critical path:
 * `CGameGlobalObjects::CGameGlobalObjects` calls it on `this+0x04` (0x800084A8) and stores
 * `this+0x04` into `gpResourceFactory` four instructions later (0x80008534).
 *
 * **This file used to be the port-only home of `fn_803096C4`, and it called that function
 * `CResFactory::CResFactory()`. That was the wrong function.** The two are 0x2,000,000 bytes apart
 * and unrelated: `fn_803096C4` is the constructor of the four bytes at `CGameGlobalObjects`+0x00 -
 * a member holding nothing but a vptr, whose body is a one-shot `CARDInit` - and it has been moved
 * to `src/MetroidPrime/CGameGlobalObjectsPad0Ctor.cpp`, which is where a function retail calls on
 * `+0x00` belongs. This file is now the real `CResFactory` constructor, and it is a `configure.py`
 * unit rather than a port-only file. The header comment of the old file is kept there, because
 * most of what it measured - the layout of `CResFactory`, the offsets of every member, the reason
 * the destructor could not own the vtable - is still true and is cited from both directions.
 *
 * Retail's bytes, all of them:
 *
 *     802fb15c  lis  r4,0x803B / addi r0,r4,6584   r0 = 0x803B19B8  `__vt__8IFactory`
 *     802fb16c  mr   r31,r3
 *     802fb170  lis  r3,0x803C / addi r0,r3,-20728 r0 = 0x803BAF08  `__vt__11CResFactory`
 *     802fb17c  addi r3,r31,4
 *     802fb184  bl   802fd0f4                     CResLoader's constructor, on this+0x04
 *     802fb188  addi r3,r31,116
 *     802fb18c  bl   802f98d0                     CFactoryMgr's constructor, on this+0x74
 *     802fb190  addi r7,r31,168                   &x9c_loading.xc_empty_prev
 *     802fb194  li   r6,0
 *     802fb198  stw  r7,160(r31)                  +0xA0 x4_start
 *     802fb19c  addi r0,r31,212                   &xc8_active.xc_empty_prev
 *     802fb1a0  lbz  r5,8(r1)                     an uninitialised byte of this frame
 *     802fb1a4  mr   r3,r31                        dead, and retail keeps it
 *     802fb1a8  stw  r7,164(r31)                  +0xA4 x8_end
 *     802fb1ac  lbz  r4,12(r1)                    and the second one
 *     802fb1b0  stw  r7,168(r31)                  +0xA8 xc_empty_prev
 *     802fb1b4  stw  r7,172(r31)                  +0xAC x10_empty_next
 *     802fb1b8  stw  r6,176(r31)                  +0xB0 x14_count = 0
 *     802fb1bc  stb  r5,180(r31)                  +0xB4
 *     802fb1c0  stb  r4,181(r31)                  +0xB5
 *     802fb1c4  stw  r6,184(r31) .. 196(r31)      +0xB8, +0xBC, +0xC0, +0xC4 = 0
 *     802fb1d4  stw  r0,204(r31) .. 216(r31)      +0xCC, +0xD0, +0xD4, +0xD8 = &xc8_active.xc_
 *     802fb1e4  stw  r6,220(r31)                  +0xDC x14_count = 0 - the object's last store
 *
 * Four pointers at one address and a zero is `rstl::list`'s empty state, and `+0xDC` being the
 * last store in the whole object is what fixes the extent at 0xE0. `x0_allocator` at +0x9C and
 * +0xC8 gets **no store at all**, which is exactly what an empty allocator compiles to.
 *
 * ## Four things in here that are not logic, all measured
 *
 *  * **The two vtable stores at +0x00 are both kept, and the addresses are data operands rather
 *    than a derived class.** Retail's is `IFactory`'s constructor followed by `CResFactory`'s.
 *    Written as two assignments through one overlay pointer, mwcceppc drops the first as dead;
 *    `x00_vptr` is a `volatile` member so both stores are volatile stores and neither is
 *    eliminable. Deriving them instead would make this object **emit** `__vt__8IFactory` (0x20
 *    bytes of `.data` at 0x803B19B8, all zeros in retail) and `__vt__11CResFactory` (0x20 bytes at
 *    0x803BAF08), plus a weak `__dt__8IFactoryFv` of 0x48 bytes at 0x802FB0E0, which is retail's
 *    `fn_802FB0E0`: `docs/research/paks.md` has both measurements, and they are why the
 *    destructor is not a unit either. **Each of those three ranges is unclaimed, so a unit that
 *    emitted them could not be `Matching`; this one emits none of them.**
 *  * **A global whose address is taken must be declared as a sized array.** mwcceppc decides
 *    small-data addressing from the declared *size*: a `void*` extern of unknown size comes out
 *    `lwz r0,0(r13)` / `R_PPC_EMB_SDA21`, and only a declaration of more than 8 bytes gets
 *    retail's `lis` + `addi` / `R_PPC_ADDR16_HA` + `_LO`. Measured with `tools/probe_cc.sh`;
 *    `src/Kyoto/CSimplePoolCtor.cpp` records it at length.
 *  * **+0xB4 and +0xB5 are two uninitialised bytes of this frame, four bytes apart.** Nothing
 *    writes either: `stwu r1,-32(r1)` puts this frame's own area at 0..31, the callee saves LR at
 *    36(r1) and r31 at 28(r1), and 0x08/0x0C are read by `lbz r5,8(r1)` and `lbz r4,12(r1)` with
 *    no store of their own. This is `src/MetroidPrime/Player/CPersistentOptionsCtor.cpp`'s
 *    spelling - an 8-byte `volatile` local read as two bytes four apart - because `volatile` is
 *    what gives the slots a home at all: without it mwcceppc folds the read away.
 *  * **The function returns `CResFactory*` and its epilogue has no `mr r3,r31`** - the only caller
 *    in the DOL (`CGameGlobalObjects`, 0x800084A8) discards the result, and retail does not pay for
 *    a copy. **MWCC emits that dead `mr r3,r31` in the middle of the body, not before the `blr`,
 *    and it is load-bearing: it is the return-value copy, and taking it away shifts every register
 *    in the tail by one.** Declared `void`, this function is 91.36% with r6/r5/r4/r3 where retail
 *    has r7/r6/r5/r4; declared `CResFactory*` with `return self;` it is 93.86% and carries the
 *    same registers retail does. That is also why this is a C-linkage free function rather than a
 *    C++ constructor: a C++ `CResFactory::CResFactory()` would additionally emit the vtables and
 *    `__dt__8IFactoryFv` (above), so the port's real `CResFactory::CResFactory()` is a one-line
 *    forwarder in `src/Kyoto/CResFactoryPortVirtuals.cpp`.
 *
 * ## What is left: eight instructions, all one scheduling decision
 *
 * 93.86%, and `tools/unit_fit.sh` reports the object **fits** - 168 claimed, 168 ours, 168 retail,
 * no extra functions - so this unit is one `li`/`addi` placement away from `Matching` and a
 * `flip_test`. Every one of the 42 instructions is present and in the same relative order; eight
 * are in a different order, and the register names follow from them:
 *
 *     retail  addi r7,r31,168 / li r6,0 / stw r7,160(r31) / addi r0,r31,212 / lbz r5,8(r1) ...
 *     ours    addi r7,r31,168 / lbz r5,8(r1) / stw r7,160(r31) / li r6,0 / lbz r4,12(r1) / addi r0,r31,212 ...
 *
 * i.e. retail hoists the two long-lived values (`li r6,0`, used at +0xB0 through +0xDC, and
 * `addi r0,r31,212`, first used at +0xCC) *above* the two volatile byte loads, and mwcceppc
 * interleaves them. Eleven statement orderings were measured with `tools/try_batch.py` and this
 * one is the best (8 differing instructions; the next were 10, 12, 12, 17, 22, 22, 31, 31, 33, 40).
 * What does *not* work: reading the two frame bytes into named locals at the top (33), putting
 * the second `SLoadList`'s empty state before the first (40), moving the flag stores after the
 * zeros (12), or splitting the zero block around them (10). The store order itself is already
 * right - it is strictly ascending in offset, +0xA0 to +0xDC - so this is an mwcceppc
 * list-schedule priority, not a source-order problem.
 *
 * `fn_802FD0F4` and `fn_802F98D0` are `CResLoader`'s and `CFactoryMgr`'s own constructors. Both are
 * unnamed in `config/G2ME01/symbols.txt` and neither is claimed by any unit, so they are called by
 * address - the arrangement `src/Kyoto/CResFactoryBuild.cpp` already uses for `fn_802FAAE4`. Note
 * that `Kyoto/CFactoryMgr.cpp` claims 0x802F8D90-0x802F8EB0 and does **not** own
 * `fn_802F98D0` at 0x802F98D0.
 */

#include "types.h"

#include "Kyoto/CResFactory.hpp"

// The two vtable addresses, as **sized arrays** - see the header. The sizes are retail's own,
// from `config/G2ME01/symbols.txt`.
// The second is retail's own name for the object at 0x803BAF08 - `config/G2ME01/symbols.txt` calls
// it `__vt__11CResFactory` - and using it is what makes the DOL link resolve, because dtk's auto
// data object defines that symbol. Declaring it `extern "C"` under a *different* name would leave
// the DOL reference dangling, and reusing the name under a *C++* declaration would mangle it to
// `_ZTV11CResFactory`, which the DOL does not have. The host, which mangles, is the other way
// round: GCC emits `_ZTV11CResFactory`, so this name is undefined there - which is one of the four
// symbols that put `src/Kyoto/CResFactoryCtor.cpp` in `tools/check_files_cmake.py`'s EXCLUDED list.
extern "C" char lbl_803B19B8[0x20]; //!< `__vt__8IFactory`: 0x20 bytes, every one of them zero
extern "C" char __vt__11CResFactory[0x20]; //!< `__vt__11CResFactory`, the vtable itself

// `fn_802FD0F4`, retail 0x802FD0F4, `size:0x80`: `CResLoader`'s constructor.
// `fn_802F98D0`, retail 0x802F98D0, `size:0x50`: `CFactoryMgr`'s constructor - the one
// `CResFactory::Build`'s 36 registrations address as `gpResourceFactory`+0x74.
extern "C" void fn_802FD0F4(void* self);
extern "C" void fn_802F98D0(void* self);

// `CResFactory`'s 0xE0 bytes, as retail's constructor writes them. `CResFactory`'s members are
// **private**, and mwcceppc 2.7 will not name a private member outside its class ("illegal access
// to protected/private member"), so the layout is carried here rather than read through
// `Kyoto/CResFactory.hpp`; `CHECK_SIZEOF` below and that header's own three derivations are the
// same measurement. `x04_` and `x74_` are `CResLoader`'s 0x70 bytes and `CFactoryMgr`'s 0x28, and
// nothing here looks inside either - retail's constructor only calls their own.
struct SResFactory {
  void* volatile x00_vptr;
  uchar x04_resLoader[0x70];
  uchar x74_factoryMgr[0x28];
  uint x9c_allocator;      //!< +0x9C, no store: an empty `rstl::list` allocator
  void* xa0_start;         //!< +0xA0
  void* xa4_end;           //!< +0xA4 - what `CResFactory::Build` compares a lookup against
  void* xa8_emptyPrev;     //!< +0xA8
  void* xac_emptyNext;     //!< +0xAC
  int xb0_count;           //!< +0xB0
  uchar xb4_flags[4];      //!< +0xB4, +0xB5 are the constructor's two uninitialised bytes
  void* xb8_root;          //!< +0xB8
  int xbc_count;           //!< +0xBC
  void* xc0_root;          //!< +0xC0
  int xc4_count;           //!< +0xC4
  uint xc8_allocator;      //!< +0xC8, no store
  void* xcc_start;         //!< +0xCC
  void* xd0_end;           //!< +0xD0 - what `CResFactory::AsyncIdle` walks
  void* xd4_emptyPrev;     //!< +0xD4
  void* xd8_emptyNext;     //!< +0xD8
  int xdc_count;           //!< +0xDC - the object's last store, and so its extent
};
CHECK_SIZEOF(SResFactory, 0xe0)

extern "C"
CResFactory* fn_802FB154(CResFactory* self) {
  SResFactory* p = reinterpret_cast< SResFactory* >(self);

  p->x00_vptr = lbl_803B19B8;
  p->x00_vptr = __vt__11CResFactory;

  fn_802FD0F4(p->x04_resLoader);
  fn_802F98D0(p->x74_factoryMgr);

  p->xa0_start = &p->xa8_emptyPrev;
  p->xa4_end = &p->xa8_emptyPrev;
  p->xa8_emptyPrev = &p->xa8_emptyPrev;
  p->xac_emptyNext = &p->xa8_emptyPrev;
  p->xb0_count = 0;

  // +0xB4 and +0xB5: the two uninitialised bytes of this frame. See the header.
  volatile u32 w[2];
  p->xb4_flags[0] = *reinterpret_cast< volatile uchar* >(&w[0]);
  p->xb4_flags[1] = *reinterpret_cast< volatile uchar* >(&w[1]);

  p->xb8_root = 0;
  p->xbc_count = 0;
  p->xc0_root = 0;
  p->xc4_count = 0;

  p->xcc_start = &p->xd4_emptyPrev;
  p->xd0_end = &p->xd4_emptyPrev;
  p->xd4_emptyPrev = &p->xd4_emptyPrev;
  p->xd8_emptyNext = &p->xd4_emptyPrev;
  p->xdc_count = 0;
  return self;
}
