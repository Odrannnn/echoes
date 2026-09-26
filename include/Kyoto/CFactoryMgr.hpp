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
  // this one.
  void RegisterFactoryByTypeIdx(uint typeIdx, CFactoryFn factory);
  // Retail 0x802F963C: the same, against the second table, whose key is not a FourCC -
  // 3 registrations use it (CMDL, AGSC, PATH) and they are the only three.
  void RegisterFactoryByOwner(uint owner, CFactoryFn factory);

private:
  rstl::map< uint, CFactoryFn > x0_factoriesByType;
  rstl::map< uint, CFactoryFn > x14_factoriesByOwner;
  uint x28_;
  uint x2c_;
  uint x30_;
  uint x34_;
};
CHECK_SIZEOF(CFactoryMgr, 0x38);

class CFactoryFnReturn {
public:
  template < typename T >
  CFactoryFnReturn(T* ptr);

  const rstl::auto_ptr<CObjOwnerDerivedFromIObjUntyped>& GetObjForTransfer() const { return obj; }
private:
  rstl::auto_ptr< CObjOwnerDerivedFromIObjUntyped > obj;
};

template < typename T >
CFactoryFnReturn::CFactoryFnReturn(T* ptr)
: obj(TToken< T >::GetIObjObjectFor(ptr).release()) {}

CFactoryFnReturn FStringTableFactory(const SObjectTag& tag, CInputStream& in,
                                     const CVParamTransfer& xfer);

#endif // _CFACTORYMGR
