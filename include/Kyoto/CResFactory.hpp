#ifndef _CRESFACTORY
#define _CRESFACTORY

#include "types.h"

#include "rstl/list.hpp"

#include "Kyoto/CFactoryMgr.hpp"
#include "Kyoto/CResLoader.hpp"

class IFactory {
public:
  virtual ~IFactory() {}
  virtual CFactoryFnReturn Build(const SObjectTag&, const CVParamTransfer&) = 0;
  virtual void BuildAsync(const SObjectTag&, const CVParamTransfer&, IObj**) = 0;
  virtual void CancelBuild(const SObjectTag&) = 0;
  virtual bool CanBuild(const SObjectTag&) = 0;
  virtual const SObjectTag* GetResourceIdByName(const char* name) const = 0;
  // TODO
};

// Retail's constructor is 0x802FB154, `size:0xA8` = 168 bytes, and it is the *whole* of what
// the caller has to do: `CGameGlobalObjects`'s constructor emits only `addi r3,r31,4 ; bl
// 802fb154` at 0x800084A4-0x800084A8, and the two `stw r0,0(r31)` at the top of 0x802FB154 are
// the `IFactory` vptr and the `CResFactory` one. **So this declaration has to stay
// declared-only.** An inline body would make mwcceppc emit the base-class vptr store at the
// call site, which is 2 instructions in the caller against retail's 2 - and 34 more for the
// `CFactoryMgr`'s four `addi`s and twenty-odd `stw`s, none of which the retail caller has.
//
// That leaves the name, and it is now right: `fn_802FB154` was **renamed in
// `config/G2ME01/symbols.txt` to `__ct__11CResFactoryFv`**, so the call mwcceppc emits from a
// declared-only constructor is the symbol the DOL defines. `src/Kyoto/CResFactoryCtor.cpp`
// records why that rename was not made earlier - it is a DOL-wide change - and this is the
// change that needed it.
class CResFactory : public IFactory {
public:
  CResFactory();

  // **Out of line, and it is load-bearing in two different compilers.** Retail has exactly one
  // `__dt__11CResFactoryFv`, out of line, at 0x802FB038 (0xA8 = 168 bytes). Left as
  // `~CResFactory() {}` inline here, mwcceppc emits a *weak* copy of it into every unit that
  // touches the class, together with the whole `CFactoryMgr`/`rstl::red_black_tree` teardown -
  // twenty-odd functions at addresses retail does not have, and `tools/unit_fit.sh` will not
  // promote a unit whose object carries them. It is also what makes this declaration the class's
  // **key function**: the first non-pure, non-inline virtual *declared* is the destructor, in
  // GCC as well as MWCC, and a vtable is emitted only by the unit that defines it. That is why
  // the port's vtable comes from `Kyoto/CResFactoryPortVirtuals.cpp` and not from
  // `Kyoto/CResFactoryBuild.cpp`.
  //
  // **There is no `Matching` unit for it in the DOL**, and the reason is measured rather than
  // guessed: mwcceppc would also emit `__vt__8IFactory` - 0x20 bytes of `.data` that retail has
  // at 0x803B19B8 with a **zero** in the destructor slot, the whole 0x20 being zeros - and a weak
  // `__dt__8IFactoryFv` of 0x48 bytes at 0x802FB0E0, which is retail's `fn_802FB0E0`. Making
  // `~IFactory` pure virtual fixes the `.data` and turns the destructor into 0x9C, replacing
  // retail's base-vptr store with a call. `docs/research/paks.md` has both measurements.
  ~CResFactory();
  CFactoryFnReturn Build(const SObjectTag&, const CVParamTransfer&);
  void BuildAsync(const SObjectTag&, const CVParamTransfer&, IObj**);
  void CancelBuild(const SObjectTag&);
  bool CanBuild(const SObjectTag&);
  const SObjectTag* GetResourceIdByName(const char* name) const;

  uint ResourceSize(const SObjectTag& tag) const { return x4_resLoader.ResourceSize(tag); }

  void AsyncIdle(uint time, bool);

  CResLoader& GetResLoader() { return x4_resLoader; }
  CFactoryMgr& GetFactoryMgr() { return x74_factoryMgr; }
  FourCC GetResourceTypeById(CAssetId id) { return GetResLoader().GetResourceTypeById(id); }

  // **`rstl::list<T>`'s six words, field for field, and 0x18 bytes.** This tree's
  // `rstl::list` (`include/rstl/list.hpp`, whose members are private) is
  // `{ Alloc x0_allocator; node* x4_start; node* x8_end; node* xc_empty_prev; node* x10_empty_next;
  // int x14_count; }`, and `CResFactory` has two of them. The identification is four
  // independent stores in the constructor `CResFactory` calls on itself, retail `fn_802FB154`
  // (0x802FB154), which is the class's own map and costs nothing to read:
  //
  // ```
  // 802fb190  addi  r7,r31,168            ; &x9c_loading.xc_empty_prev, i.e. +0xA8
  // 802fb198  stw   r7,160(r31)           ; +0xA0  x4_start
  // 802fb1a8  stw   r7,164(r31)           ; +0xA4  x8_end
  // 802fb1b0  stw   r7,168(r31)           ; +0xA8  xc_empty_prev
  // 802fb1b4  stw   r7,172(r31)           ; +0xAC  x10_empty_next
  // 802fb1b8  stw   r6,176(r31)           ; +0xB0  x14_count = 0
  // 802fb19c  addi  r0,r31,212            ; &xc8_active.xc_empty_prev, i.e. +0xD4
  // 802fb1d4  stw   r0,204(r31)           ; +0xCC  x4_start
  // 802fb1d8  stw   r0,208(r31)           ; +0xD0  x8_end
  // 802fb1dc  stw   r0,212(r31)           ; +0xD4  xc_empty_prev
  // 802fb1e0  stw   r0,216(r31)           ; +0xD8  x10_empty_next
  // 802fb1e4  stw   r6,220(r31)           ; +0xDC  x14_count = 0
  // ```
  //
  // Four pointers at one address and a zero is `rstl::list`'s own empty state, and
  // **+0xDC is the constructor's last store in the whole object** - which is what fixes the
  // extent at 0xE0 (see `CHECK_SIZEOF` below). `x0_allocator` at +0x9C and +0xC8 gets **no
  // store at all**, which is exactly what an empty allocator compiles to and is the reason
  // those two words read as uninitialised. Three further reads confirm the shape:
  // `CResFactory::AsyncIdle` (0x802FA384) walks `x8_end` at 0xCC comparing against `x8_end` at
  // 0xD0; `CResFactory::Build` compares its map lookup against `x8_end` at **0xA4**; and
  // `~CResFactory` calls the same 0x8C-byte destructor on +0x9C and on +0xC8.
  //
  // The element type is not identified: the only reads of either list in the DOL are in
  // `AsyncIdle` (a vcall on `node+0x14` and an erase), `Build` (`node+0x18` as a `void**`) and
  // `CancelBuild` (a vcall on `*(node+0x08)+0x14`), so nothing here needs to know it.
  struct SLoadList {
    uint x0_allocator;
    void* x4_start;
    void* x8_end;
    void* xc_empty_prev;
    void* x10_empty_next;
    int x14_count;
  };

private:
  // `CResLoader` is **0x70** bytes, not 0x58 or 0x60 - see the four unnamed words at the end of
  // its member list in `Kyoto/CResLoader.hpp` for the two measurements that fix it.
  CResLoader x4_resLoader; // +0x04, 0x70
  // `CFactoryMgr` is at **+0x74**, which is what all 36 factory registrations in
  // `CGameGlobalObjects::AddPaksAndFactories` address: `addi r3, r31, 116` at 0x80007510 and
  // 0x80007528 and 34 more times, with `r31` = `gpResourceFactory`. `CFactoryMgr`'s own two
  // registrars then use `this+0x00` and `this+0x14` for their two `rstl::map`s
  // (`fn_802F96E0` at 0x802F971C-0x802F9720 and `fn_802F963C` at 0x802F967C-0x802F9680), which is
  // the layout `Kyoto/CFactoryMgr.hpp` already carries and which is a `Matching` unit at 100%.
  // The header used to say +0x64, which is 0x10 too low; the 36 registrations are what prove it.
  CFactoryMgr x74_factoryMgr; // +0x74, **0x28** - see the note in `Kyoto/CFactoryMgr.hpp`
  SLoadList x9c_loading;      // +0x9C, 0x18
  // **+0xB4, 0x14 bytes, and the only member of this class retail neither names nor this tree
  // models.** What is measured about it:
  //
  //  * the constructor sets `+0xB4` and `+0xB5` from its **own incoming stack frame** -
  //    `lbz r5,8(r1)` and `lbz r4,12(r1)` at 0x802FB1A0 and 0x802FB1AC, with no argument ever
  //    passed: `CGameGlobalObjects`'s constructor calls it as `addi r3,r31,4 / bl 0x802FB154`
  //    and sets nothing. So retail's `CResFactory` constructor takes two `bool`s it reads out
  //    of the caller's frame, and they are uninitialised on the only call in the DOL. It is the
  //    one place in this class where retail's own code reads garbage, and it is why the
  //    constructor is not written;
  //  * the constructor then zeroes +0xB8, +0xBC, +0xC0 and +0xC4 and nothing else, and
  //    `~CResFactory` destroys it with `fn_802FB0E0` (0x802FB0E0, 0x14 bytes), which tests
  //    `*(this+0x10)` and then `*(this+0x04)` and recurses through `fn_802FB1FC` (0x802FB1FC),
  //    a two-way tree free that ends in `CMemory::Free(this)`. Four words freed as two trees is
  //    two `rstl::map`s at +0x04 and +0x10, i.e. +0xB8 and +0xC4;
  //  * `fn_802FAAE4` - the helper both `Build` and `CancelBuild` call first - walks
  //    `fn_802FAA20(&xB4_x, tag)`, whose first instruction is `lwz r7,16(r3)`, so the lookup
  //    is rooted at +0xC4, and it returns a node whose **+0x18 is the payload pointer** that
  //    `Build` spins on. The sentinel it compares against is `&xB4_x[+0x08]` = `CResFactory`+0xBC.
  struct SUnknownMapPair {
    uchar x0_flags[4]; // +0xB4, +0xB5 are the two constructor bytes; +0xB6, +0xB7 unknown
    void* x4_root;     // +0xB8
    int x8_count;      // +0xBC
    void* xc_root;     // +0xC0
    int x10_count;     // +0xC4
  };
  SUnknownMapPair xb4_pending; // +0xB4, 0x14
  SLoadList xc8_active;       // +0xC8, 0x18
};
// **0xE0**, not 0xE4. Three independent measurements, all from the same base
// (`gpResourceFactory`, which the `CGameGlobalObjects` constructor stores as `this+0x04` at
// 0x80008528/0x80008534):
//
//  * the constructor's last store is at +0xDC, four bytes, so the extent is 0xE0 - and that
//    store is the *count* of the second `rstl::list` (`xc8_active.x14_count`), so the whole of
//    the last 0x18 is accounted for by name rather than by padding;
//  * `CGameGlobalObjects::CGameGlobalObjects` builds `CSimplePool` on `this+0xE4` with
//    `this+0x04` as its `IFactory&` (0x800084AC-0x800084B4), so 0xE4 - 0x04 = 0xE0;
//  * `CGameGlobalObjects::CGameGlobalObjects` builds `CCharacterFactoryBuilder` on `this+0x108`
//    (0x800084B8) and `CSimplePool` is `CHECK_SIZEOF(..., 0x24)`, so 0xE4 + 0x24 = 0x108.
//
// It *contains* two members: `CResLoader` at +0x04 (0x70 bytes) and `CFactoryMgr` at +0x74 -
// they are not siblings of this class in `CGameGlobalObjects`, which is the misreading that
// produced a wrong CResLoader size for a whole session. Both offsets are the ctor's own
// `addi r3,r31,4` / `addi r3,r31,116` at 0x802FB17C and 0x802FB188.
CHECK_SIZEOF(CResFactory, 0xe0);
// The 36 registrations' base, +0x74, is the one offset in this class retail states outright
// (`addi r3, r31, 116` with `r31` = `gpResourceFactory`, 36 times in `AddPaksAndFactories`).
// It has no `CHECK_OFFSETOF`, and cannot have one: mwcceppc 2.7 will not name a private member
// outside its class ("illegal access to protected/private member"), which is the same reason
// `MetroidPrime/CStateManager.hpp`'s two are commented out. The `x74_` in the member's name and
// the comment above it are the record.
//
// CHECK_OFFSETOF(CResFactory, x74_factoryMgr, 0x74);
// CHECK_OFFSETOF(CResFactory, x4_resLoader, 0x4);

extern CResFactory* gpResourceFactory;

#endif // _CRESFACTORY
