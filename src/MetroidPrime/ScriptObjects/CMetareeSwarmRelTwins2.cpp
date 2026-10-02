// CMetareeSwarmRelTwins2.cpp - MetareeSwarm's (module 43) second block of outlined rstl
// instantiations, .text 0x23D4..0x24A0: `rstl::vector< CWorldState >::reserve` and the destroy
// forwarder it calls.
//
// The same arrangement as `CMetareeSwarmRelTwins.cpp` above it in this directory: a range of the
// module that our own object reproduces byte for byte, claimed by a unit of its own, with
// everything between the module's named ranges left unclaimed so dtk fills it from retail. A
// second file is required because one unit cannot claim two discontiguous ranges (the `ScriptCoin`
// measurement in `docs/RUNNING_THE_DECOMP.md`), not because of the content.
//
// Ranges from `config/G2ME01/rels/MetareeSwarm/symbols.txt`:
//
//   0x23D4 fn_43_23D4  0xAC  `rstl::vector< CWorldState >::reserve`: growth test, allocate,
//                            copy, destroy, swap - the DOL's own instantiation at `0x801466F4`
//                            (`MetroidPrime/Player/CGameState`) is the same 0xAC bytes
//   0x2480 fn_43_2480  0x20  `rstl::destroy< CWorldState* >`'s forwarder to the module's own
//                            outlined loop at 0x24A0 (left unclaimed)
//
// Both are compiler emissions in retail (`reserve` is `void` in `rstl/vector.hpp`;
// `destroy`/`destroy_impl` are the `inline` pair in `rstl/construct.hpp`), kept here as
// `extern "C"` functions under the module's dtk names because objdiff pairs by symbol name - the
// same device `CMetareeSwarmRelTwins.cpp`, `CCharacterInfo.cpp` and `CTargetReticles.cpp` use for
// their outlined copies.
//
// **The element type is real here**: `include/MetroidPrime/Player/CWorldState.hpp` is
// `CHECK_SIZEOF(CWorldState, 0x24)`, and retail's `mulli r3,newSize,0x24` plus the 0x24-byte loop
// step in the unclaimed 0x24A0 agree with it. `CGameState.cpp` is where the DOL emits the same
// instantiation.
//
// **`destroy` is a call, not an inline loop.** Retail's bytes at 0x2440 are
// `lwz mCount / lwz mItems / mulli 0x24 / add / bl fn_43_2480`, because the element has a
// destructor and `destroy_impl` is outlined here; the loop itself is the module's 0x24A0, which
// this unit leaves retail. `fn_43_2480` is therefore written as the two-instruction forwarder
// retail has - it passes r3/r4 straight through, with no `mr` in front of the call.
//
// Definitions are in descending retail text order (mwcceppc emits definitions in reverse source
// order); `python3 tools/check_decl_order.py --unit <this file>`.
//
// This file is listed in `files.cmake`, and its host branch defines nothing - the same
// arrangement `CMetareeSwarmRelTwins.cpp` uses: the port reads `MetareeSwarm.rel` off the disc,
// so the host link must not see these symbols.

#include "MetroidPrime/Player/CWorldState.hpp"
#include "rstl/vector.hpp"

extern "C" {
#ifdef __MWERKS__

typedef rstl::vector< CWorldState > TWorldStateVector;

// .text 0x24A0, 0x90 bytes: the module's own `destroy_impl` loop over the 0x24-byte element.
// Left retail - not this unit's range - and declared only so `fn_43_2480`'s call resolves by the
// dtk name, the same way `CLumiteRelTail.cpp` declares its unclaimed `fn_39_7C0`.
void fn_43_24A0(CWorldState* first, CWorldState* last);

// .text 0x2530, 0xAC bytes: the module's own `uninitialized_copy` over the same element.
CWorldState* fn_43_2530(TWorldStateVector::iterator first, TWorldStateVector::iterator last,
                        CWorldState* dst);

// .text 0x2480, 0x20 bytes. The forwarder: the range endpoints arrive in r3/r4 and go straight
// into the loop's call.
void fn_43_2480(CWorldState* first, CWorldState* last) { fn_43_24A0(first, last); }

// .text 0x23D4, 0xAC bytes. The same statement order as `CMetareeSwarmRelTwins.cpp`'s
// `fn_43_26F8`: capacity test, `allocate(newSize * 0x24)` at the call site, the copy into the new
// block, the destroy of the old elements through the module's forwarder, then the swap.
void fn_43_23D4(TWorldStateVector* self, int newSize) {
  if (newSize <= self->mCapacity) {
    return;
  }

  CWorldState* newData;
  self->mAllocator.allocate(newData, newSize);
  fn_43_2530(self->begin(), self->end(), newData);
  fn_43_2480(self->mItems, self->mItems + self->mCount);
  self->mAllocator.deallocate(self->mItems);
  self->mItems = newData;
  self->mCapacity = newSize;
}

#endif
}
