#ifndef _CFACTORYMGR
#define _CFACTORYMGR

#include "types.h"

#include "rstl/map.hpp"

#include "Kyoto/IObjectStore.hpp"
#include "Kyoto/Streams/CInputStream.hpp"
#include "Kyoto/TToken.hpp"

class CFactoryFnReturn;

// Every resource type the game knows, and the function that builds one from a stream. All 36
// of the latter are named in retail and are `CFactoryFnReturn f(const SObjectTag&, CInputStream&,
// const CVParamTransfer&)`; the 33 that this tree does not define yet are listed in
// docs/research/paks.md.
typedef CFactoryFnReturn (*CFactoryFn)(const SObjectTag&, CInputStream&, const CVParamTransfer&);

// The **other** table's value type, and it is a different function-pointer type, not a cast.
// `fn_802F8EB0` - the only caller that dispatches through the owner-keyed map - sets five words
// before `mtctr`/`bctrl` (`mr r4,r31` / `mr r5` is a stack local / `mr r6,r26` / `mr r7,r28`,
// 0x802F8FDC-0x802F8FE4), where the FourCC-keyed dispatch in `fn_802F94D8` sets four
// (0x802F9518-0x802F952C). Exactly three registrations go through it - CMDL, AGSC and PATH - and
// exactly those three factories read the extra argument (CMDL at 0x80311348 is
// `lwz r4,4(r7)`). So the second table holds a five-argument function and the first a
// four-argument one, and `RegisterFactoryByOwner` takes the former.
typedef CFactoryFnReturn (*CFactoryFnOwner)(const SObjectTag&, const CVParamTransfer&,
                                            CInputStream&, void* owner);

// Retail: 0x802F8D90 and 0x802F8E9C, 0x120 bytes together, and **unclaimed** by any unit before
// src/Kyoto/CFactoryMgr.cpp existed - so that unit claims a range nothing else did and the DOL
// hash cannot move. Two tables come with it:
//
//   .rodata 0x803AF9D8, 46 FourCCs (0xB8 bytes) - the resource types, in index order.
//   .data   0x803BC568, 256 bytes - `__upper_map` from <ctype.h>, which the Matching unit
//            Runtime/ctype.c already owns, so this class does not declare it.
//
// The layout is measured, not assumed, and the evidence is `rstl::red_black_tree`, which this
// tree already models to the byte:
//
//   * `CGameGlobalObjects::AddPaksAndFactories` calls both registrars with `r3` =
//     `gpResourceFactory`+0x74 (`addi r3,r31,116` at 0x80007510 and 0x80007528).
//   * `CResFactory` puts `CResLoader` at +0x04 and `CFactoryMgr` at +0x5C, so +0x74 is
//     `CFactoryMgr`+0x18 - **which is wrong**: retail's first table is at +0x00 of whatever
//     +0x74 points at, and its second at +0x14, so +0x74 *is* `CFactoryMgr`+0x00 and
//     `CResLoader` is 0x70 bytes, not the 0x58 the two old headers imply. See the
//     `CGameGlobalObjects` note in src/Kyoto/CResFactoryCtor.cpp.
//   * `fn_802F93C4` (0x802F93C4) reads the root at tree+0x10, the count at tree+0x04 and the
//     leftmost/rightmost at +0x08/+0x0C; `fn_802F9980` (0x802F9980) writes exactly those four
//     and calls `rbtree_rebalance__4rstlFPvPv` with `tree+8` - the header. That is
//     `red_black_tree`'s `x4_count` and `header`, field for field, and
//     `CHECK_SIZEOF(unk_map, 0x14)` in include/rstl/map.hpp is the same 20 bytes.
//   * `fn_802F9B2C` (0x802F9B2C) allocates 24 bytes for a node and fills
//     `{left, right, parent, colour, value[0], value[1]}`, with the key at +0x10 and the value
//     at +0x14 - `map<uint, CFactoryFn>`'s node, whose `pair` is exactly those two words.
//
// **The four words at +0x28 are not identified.** 0x38 is the size `docs/research/paks.md`
// records for this class, and 0x28 is what the two tables account for; nothing read in the DOL
// writes or reads +0x28..+0x38, so they are carried as unnamed `uint`s to reach the size and
// nothing may be added between them. A lane that identifies them should say so here.
class CFactoryMgr {
public:
  static uint FourCCToTypeIdx(uint fourCC);
  static uint TypeIdxToFourCC(uint typeIdx);

  // Retail 0x802F96E0: insert `{typeIdx, factory}` into the FourCC table unless typeIdx is
  // already there. 33 of the 36 registrations in CGameGlobalObjects::AddPaksAndFactories use
  // this one. `fn_802F96E0` finds in the map at `this+0x00` and inserts into the same one
  // (`addi r3, r29, 8` / `mr r4, r29` at 0x802F9720/0x802F9750) - **not** at `this+0x14`, which
  // is what docs/research/paks.md said before this was read back out of the disassembly.
  void RegisterFactoryByTypeIdx(uint typeIdx, CFactoryFn factory);
  // Retail 0x802F963C: the same, against the second table, whose key is not a FourCC -
  // 3 registrations use it (CMDL, AGSC, PATH) and they are the only three. `fn_802F963C` uses
  // `addi r4, r29, 20` for both the find (0x802F966C) and the insert (0x802F96B0), i.e.
  // `this+0x14`.
  void RegisterFactoryByOwner(uint owner, CFactoryFnOwner factory);

private:
  rstl::map< uint, CFactoryFn > x0_factoriesByType;
  // **Not** `CFactoryFn`: the owner-keyed table's value type takes a fourth argument, which is
  // what CMDL, AGSC and PATH - the only three entries - are given. See `CFactoryFnOwner` above.
  rstl::map< uint, CFactoryFnOwner > x14_factoriesByOwner;
};
// **0x28, not 0x38.** The two `rstl::map`s are 0x14 each (`CHECK_SIZEOF(unk_map, 0x14)` in
// `include/rstl/map.hpp`) and that is the whole class:
//
//  * `fn_802F9784` - the deleting destructor, which is what `~CResFactory` calls on
//    `CResFactory`+0x74 with the flag - destroys `this+0x14` (`addi r3,r30,20`) and `this+0x00`
//    and nothing else, so it never reaches +0x28;
//  * `fn_802F98D0`, the constructor `CResFactory` calls on +0x74, writes the same two maps and
//    nothing past +0x28;
//  * `AddPaksAndFactories`' 36 registrations address `this+0x00` and `this+0x14` and no other
//    field, and so do the two dispatch sites `fn_802F94D8` and `fn_802F8EB0`.
//
// **The four words this class used to carry at +0x28..+0x38 are not this class's at all.** They
// are `CResFactory`+0x9C..+0xAC, the first half of the `rstl::list` that follows the manager -
// see `Kyoto/CResFactory.hpp`. Carrying them here is what made `CResFactory`'s own interior
// unreadable: 0xA4, the word `CResFactory::Build` compares its map lookup against, is
// `CFactoryMgr`+0x30 under the old model and `x9c_loading.x8_end` under this one.
CHECK_SIZEOF(CFactoryMgr, 0x28);

class CFactoryFnReturn {
public:
  CFactoryFnReturn() {}
  template < typename T >
  CFactoryFnReturn(T* ptr);

  const rstl::auto_ptr<CObjOwnerDerivedFromIObjUntyped>& GetObjForTransfer() const { return obj; }

  // **Retail's `CResFactory::Build` stores the two words of this object's single `rstl::auto_ptr`
  // itself** - `stb (p != 0), 0(ret)` and `stw p, 4(ret)` at 0x802FA9E0 and 0x802FA9E4, into the
  // caller's return slot, with nothing written before them and **no destructor call after them** -
  // and no other constructor of this class can have produced that: the template one above goes
  // through `TToken<T>::GetIObjObjectFor`, which is a call, and a *named local* of this type
  // would be built in the frame, copied into the return slot and then destroyed, which is 0x50
  // bytes of `__dt__16CFactoryFnReturnFv` retail does not have in that function. So this is the
  // constructor retail used: the temporary is built straight into the return slot. It is a
  // non-template overload on purpose - the template is an exact match for a `T*` and a
  // derived-to-base pointer conversion cannot beat it, so no existing caller changes.
  CFactoryFnReturn(CObjOwnerDerivedFromIObjUntyped* ptr) : obj(ptr) {}

private:
  rstl::auto_ptr< CObjOwnerDerivedFromIObjUntyped > obj;
};

template < typename T >
CFactoryFnReturn::CFactoryFnReturn(T* ptr)
: obj(TToken< T >::GetIObjObjectFor(ptr).release()) {}

CFactoryFnReturn FStringTableFactory(const SObjectTag& tag, CInputStream& in,
                                     const CVParamTransfer& xfer);

#endif // _CFACTORYMGR
