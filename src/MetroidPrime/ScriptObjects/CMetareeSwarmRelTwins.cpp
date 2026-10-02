// CMetareeSwarmRelTwins.cpp - MetareeSwarm's (module 43) block of outlined rstl instantiations,
// .text 0x2690..0x27B0: the two functions retail emitted out of line for the module's own
// `rstl::vector` of a 0x1C-byte record and for `rstl::vector< IGameArea::Dock >`.
//
// A third unit in the same module, the same arrangement as `CSplitterRel.cpp` /
// `CSplitterRelTwins.cpp`: the head claims `.text 0x0..0xD8`, the teardown group claims
// 0x1F38..0x1FB8, so everything between the named ranges stays unclaimed, dtk fills it from
// retail, and the module's sha1 against `config/G2ME01/config.yml` still holds.
//
// Ranges from `config/G2ME01/rels/MetareeSwarm/symbols.txt`:
//
//   0x2690 fn_43_2690  0x68  `rstl::uninitialized_copy` over `vector< IGameArea::Dock >`'s
//                            0x4C-byte element - `*first` read once, `*last` re-read every pass
//   0x26F8 fn_43_26F8  0xB8  the module's `vector< 0x1C-byte record >::reserve`: the growth
//                            test, then allocate, copy, destroy and swap, in retail's order
//
// Both are compiler emissions from `include/rstl/`: `uninitialized_copy` is a `static inline`
// template, so retail keeps one outlined copy per translation unit - a *local* symbol no other
// unit can name - and `vector::reserve` is outlined here rather than inlined at its two call
// sites. That is why the module's copies carry dtk's `fn_43_*` names rather than mangled ones,
// and why they are written here as `extern "C"` functions under those names: objdiff pairs by
// symbol name, so a hand-written `fn_43_2690` is what gives the module's retail symbol a partner
// to score against. The same device `CCharacterInfo.cpp`/`CTargetReticles.cpp` use for their
// outlined copies.
//
// **`fn_43_2690` is the DOL's own `uninitialized_copy< pointer_iterator< Dock > >`
// (`0x80060188`, `CGameArea.cpp`'s outlined copy) apart from its construct call**: retail's
// module calls its own `fn_43_1F70` (0x1F70, the module's `rstl::construct< Dock >` forwarder,
// inside the `CMetareeSwarmDes.cpp` claim) where the DOL calls
// `construct< IGameArea::Dock >`. The spelling below is that twin's, which is measured: `*first`
// hoisted into `r31` in the prologue and `*last` re-read inside the loop test.
//
// **`fn_43_26F8`'s element is not the tree's `CEffectComponent`.** `tools/twin_scan.py` pairs the
// function with the DOL's `reserve<vector<CEffectComponent>>` (`0x802936EC`,
// `Kyoto/Animation/CCharacterInfo`) by *shape*, but `include/Kyoto/Animation/CEffectComponent.hpp`
// is 0x34 bytes with two `rstl::string` members, while this module's copy steps by 0x1C and its
// outlined element copy `fn_43_27B0` (0x27B0, unclaimed) moves seven floats with no destructor
// call. So the element is modelled as the 0x1C-byte record below - it is what `allocate`'s
// `mulli r3,newSize,0x1c`, the `addi ...,0x1c` loop step and retail's *empty* destroy loop fix.
//
// **`mw_version="GC/2.7"` is load-bearing and measured** (the `Object(...)` line in
// `configure.py`). Under the module's default GC/1.3.2 the same source reproduces `fn_43_26F8`
// byte for byte and puts `fn_43_2690`'s `lwz r31,0(r3)` *after* the r30/r29 saves (92.31%);
// 2.7 hoists it into retail's slot and both are exact - the same save-order difference
// `CLumiteRelTail.cpp` records for `fn_39_738`, and the third module whose outlined block is 2.7
// code.
//
// Definitions are in descending retail text order: mwcceppc emits definitions in reverse source
// order and mwldeppc keeps the object's `.text` order verbatim, so ascending would permute the
// module's bytes with objdiff still at 100%, and only the module's sha1 would catch it.
//
// This file is listed in `files.cmake`, and its host branch defines nothing - the same
// arrangement `CLumiteRelTail.cpp` and `CSplitterRelTwins.cpp` use: the port reads
// `MetareeSwarm.rel` off the disc and never calls into the module, so defining these symbols for
// the host link would only add undefined references to `fn_43_*` neighbours.

#include "MetroidPrime/IGameArea.hpp"
#include "rstl/construct.hpp"
#include "rstl/vector.hpp"

extern "C" {
#ifdef __MWERKS__

// The module's own 0x1C-byte vector element - see the header comment. Seven floats and nothing
// else: no destructor to emit, which is why retail's destroy step below is an empty loop.
struct SRelSwarmElement {
  float mValues[7];
};

typedef rstl::vector< SRelSwarmElement > TRelSwarmVector;

// .text 0x27B0, 0x64 bytes: the module's own `uninitialized_copy` over that element. Left retail
// - not this unit's range - and declared only so `fn_43_26F8`'s call resolves by the dtk name,
// the same way `CLumiteRelTail.cpp` declares its unclaimed `fn_39_7C0`.
SRelSwarmElement* fn_43_27B0(TRelSwarmVector::iterator first, TRelSwarmVector::iterator last,
                             SRelSwarmElement* dst);

// .text 0x2690, 0x68 bytes: the module's own `rstl::construct< IGameArea::Dock >`. Declared here
// with the second parameter retail's `uninitialized_copy` call passes in r4 (`CMetareeSwarmDes.cpp`
// models the same address with one parameter, because that carve's own call site passes one and its
// body ignores r4 either way).
void fn_43_1F70(void* dst, const IGameArea::Dock& src);

// .text 0x26F8, 0xB8 bytes. `vector::reserve`: the capacity test first
// (`cmpw newSize,mCapacity` / `ble`), then `allocate(newSize * 0x1C)` - the multiply at the call
// site - then the copy into the new block, the destroy of the old elements, and the swap of
// `mItems`/`mCapacity`. `newData` is declared before the call so its address is what `allocate`
// writes through, and the copy call's two `pointer_iterator`s are materialised in the frame.
void fn_43_26F8(TRelSwarmVector* self, int newSize) {
  if (newSize <= self->mCapacity) {
    return;
  }

  SRelSwarmElement* newData;
  self->mAllocator.allocate(newData, newSize);
  fn_43_27B0(self->begin(), self->end(), newData);
  rstl::destroy(self->mItems, self->mItems + self->mCount);
  self->mAllocator.deallocate(self->mItems);
  self->mItems = newData;
  self->mCapacity = newSize;
}

// .text 0x2690, 0x68 bytes. `rstl::uninitialized_copy` over `vector<IGameArea::Dock>::iterator`:
// the destination in `r30`, the source cursor in `r31`, the end iterator's address in `r29` and
// `*last` re-read on every pass, because the loop body calls out of line.
IGameArea::Dock* fn_43_2690(IGameArea::Dock* const* first, IGameArea::Dock** last,
                           IGameArea::Dock* dst) {
  const IGameArea::Dock* it = *first;
  while (it != *last) {
    fn_43_1F70(dst, *it);
    ++it;
    ++dst;
  }
  return dst;
}

#endif
}
